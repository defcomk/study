////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsdemosaic36.cpp
/// @brief BPSDEMOSAIC36 class implementation
///        Demosic Interpolates R/Gr/Gb/B Bayer mosaicked image to R, G, B full sized images
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpsdemosaic36.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 BPSDemosaic36RegLengthDWord = sizeof(BPSDemosaic36RegConfig) / sizeof(UINT32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemosaic36::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSDemosaic36* pModule = CAMX_NEW BPSDemosaic36(pCreateData->pNodeIdentifier);

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Module initialization failed");
                pModule->Destroy();
                pModule = NULL;
            }
        }
        else
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemosaic36::Initialize()
{
    CamxResult result = CamxResultSuccess;
    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Unable to initilize common library data, no memory");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemosaic36::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(demosaic_3_6_0::demosaic36_rgn_dataType) * (Demosaic36MaximumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for demosaic_3_6_0::demosaic36_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSDemosaic36::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)           &&
        (NULL != pInputData->pHwContext))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->DemosaicEnable;

            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
            CAMX_ASSERT(NULL != pTuningManager);

            // Search through the tuning data (tree), only when there
            // are changes to the tuning mode data as an optimization
            if ((TRUE == pInputData->tuningModeChanged)    &&
                (TRUE == pTuningManager->IsValidChromatix()))
            {
                CAMX_ASSERT(NULL != pInputData->pTuningData);

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_demosaic36_bps(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if (m_pChromatix != m_dependenceData.pChromatix)
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.demosaic_enable;

                    isChanged                   = (TRUE == m_moduleEnable);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }
        }
        if (TRUE == m_moduleEnable)
        {
            if (TRUE == IQInterface::s_interpolationTable.
                demosaic36TriggerUpdate(&(pInputData->triggerData), &m_dependenceData))
            {
                isChanged = TRUE;
            }
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Input: pHwContext %p", pInputData->pHwContext);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemosaic36::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_DEMOSAIC_INTERP_COEFF_CFG,
                                              BPSDemosaic36RegLengthDWord,
                                              reinterpret_cast<UINT32*>(&m_regCmd));

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
        }
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemosaic36::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        BpsIQSettings*       pBPSIQSettings        = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
        DemosaicParameters*  pDemosaicModuleConfig = &pBPSIQSettings->demosaicParameters;
        Demosaic36OutputData outputData;

        outputData.type              = PipelineType::BPS;
        outputData.BPS.pRegBPSCmd    = &m_regCmd;
        outputData.BPS.pModuleConfig = pDemosaicModuleConfig;

        result = IQInterface::Demosaic36CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Demosaic Calculation Failed. result %d", result);
        }
        if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
        {
            DumpRegConfig(pDemosaicModuleConfig);
        }
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSDemosaic36::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->demosaicParameters.moduleCfg.EN = m_moduleEnable;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(BPSDemosaic36RegConfig) <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDemosaicData.demosaicConfig));

        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDemosaicData.demosaicConfig,
                      &m_regCmd,
                      sizeof(BPSDemosaic36RegConfig));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemosaic36::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            if (FALSE == pInputData->useHardcodedRegisterValues)
            {
                result = RunCalculation(pInputData);
            }
            else
            {
                InitRegValues();
            }
        }

        if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
        {
            result = CreateCmdList(pInputData);
        }

        if (CamxResultSuccess == result)
        {
            UpdateBPSInternalData(pInputData);
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Operation failed %d", result);
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSDemosaic36::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::BPSDemosaic36
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSDemosaic36::BPSDemosaic36(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type         = ISPIQModuleType::BPSDemosaic;
    m_moduleEnable = FALSE;
    m_pChromatix   = NULL;
    m_cmdLength    = PacketBuilder::RequiredWriteRegRangeSizeInDwords(BPSDemosaic36RegLengthDWord);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::~BPSDemosaic36
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSDemosaic36::~BPSDemosaic36()
{
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::InitRegValues
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSDemosaic36::InitRegValues()
{
    m_regCmd.interpolationCoefficientConfig.u32All = 0x00000080;
    m_regCmd.interpolationClassifierConfig.u32All  = 0x00800066;

    m_pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemosaic36::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSDemosaic36::DumpRegConfig(
    const DemosaicParameters*  pDemosaicModuleConfig
    ) const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "********  BPS DEMOSAIC36 ********  \n");
        CamX::OsUtils::FPrintF(pFile, "* DEMOSAIC36 module CFG \n");
        CamX::OsUtils::FPrintF(pFile, "EN                = %d\n", pDemosaicModuleConfig->moduleCfg.EN);
        CamX::OsUtils::FPrintF(pFile, "COSITED_RGB_EN    = %d\n", pDemosaicModuleConfig->moduleCfg.COSITED_RGB_EN);
        CamX::OsUtils::FPrintF(pFile, "DIR_G_INTERP_DIS  = %d\n", pDemosaicModuleConfig->moduleCfg.DIR_G_INTERP_DIS);
        CamX::OsUtils::FPrintF(pFile, "DIR_G_INTERP_DIS  = %d\n", pDemosaicModuleConfig->moduleCfg.DIR_G_INTERP_DIS);
        CamX::OsUtils::FPrintF(pFile, "DIR_RB_INTERP_DIS = %d\n", pDemosaicModuleConfig->moduleCfg.DIR_RB_INTERP_DIS);
        CamX::OsUtils::FPrintF(pFile, "DIR_RB_INTERP_DIS = %d\n", pDemosaicModuleConfig->moduleCfg.DIR_RB_INTERP_DIS);
        CamX::OsUtils::FPrintF(pFile, "DYN_G_CLAMP_EN    = %d\n", pDemosaicModuleConfig->moduleCfg.DYN_G_CLAMP_EN);
        CamX::OsUtils::FPrintF(pFile, "DYN_G_CLAMP_EN    = %d\n", pDemosaicModuleConfig->moduleCfg.DYN_G_CLAMP_EN);
        CamX::OsUtils::FPrintF(pFile, "DYN_RB_CLAMP_EN   = %d\n", pDemosaicModuleConfig->moduleCfg.DYN_RB_CLAMP_EN);
        CamX::OsUtils::FPrintF(pFile, "DYN_RB_CLAMP_EN   = %d\n", pDemosaicModuleConfig->moduleCfg.DYN_RB_CLAMP_EN);
        CamX::OsUtils::FPrintF(pFile, "* DEMOSAIC36 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "interpolation Coefficient Config = %x\n", m_regCmd.interpolationCoefficientConfig);
        CamX::OsUtils::FPrintF(pFile, "interpolation Classifier Config  = %x\n", m_regCmd.interpolationClassifierConfig);
        CamX::OsUtils::FPrintF(pFile, "interpolation Coefficient Config = %x\n", m_regCmd.interpolationCoefficientConfig);
        CamX::OsUtils::FPrintF(pFile, "interpolation Classifier Config  = %x\n", m_regCmd.interpolationClassifierConfig);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
}

CAMX_NAMESPACE_END
