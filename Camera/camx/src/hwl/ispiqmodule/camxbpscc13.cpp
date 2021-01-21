////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpscc13.cpp
/// @brief bpscc13 class implementation
///        Compensates global chroma difference due to R/G/B channel crosstalk by applying a
///        3x3 matrix color correction matrix (CCM)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpscc13.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCC13::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCC13::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSCC13* pModule = CAMX_NEW BPSCC13(pCreateData->pNodeIdentifier);

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
// BPSCC13::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCC13::Initialize()
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
// BPSCC13::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCC13::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(cc_1_3_0::cc13_rgn_dataType) * (CC13MaxNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for cc_1_3_0::cc13_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCC13::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSCC13::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                 &&
        (NULL != pInputData->pHALTagsData)   &&
        (NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData))
    {
        ISPHALTagsData* pHALTagsData = pInputData->pHALTagsData;

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->CCEnable;

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
                m_pChromatix = pTuningManager->GetChromatix()->GetModule_cc13_bps(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if ((NULL == m_dependenceData.pChromatix)                                       ||
                    (m_pChromatix->SymbolTableID != m_dependenceData.pChromatix->SymbolTableID) ||
                    (m_moduleEnable != m_pChromatix->enable_section.cc_enable))
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.cc_enable;

                    isChanged                   = (TRUE == m_moduleEnable);
                }
            }
        }

        // Check for manual tone map mode, if yes disable 2dLUT
        if ((TRUE                               == m_moduleEnable)                    &&
            (ColorCorrectionModeTransformMatrix == pHALTagsData->colorCorrectionMode) &&
            (((ControlAWBModeOff == pHALTagsData->controlAWBMode) &&
              (ControlModeAuto   == pHALTagsData->controlMode))   ||
             (ControlModeOff     == pHALTagsData->controlMode)))
        {
            m_moduleEnable = FALSE;
            isChanged      = FALSE;

            CAMX_LOG_ERROR(CamxLogGroupISP, "<CCM> Manual Mode m_moduleEnable %d", m_moduleEnable);
        }

        if (TRUE == m_moduleEnable)
        {
            if (TRUE == IQInterface::s_interpolationTable.CC13TriggerUpdate(&pInputData->triggerData, &m_dependenceData))
            {
                isChanged = TRUE;
            }
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Invalid Input: pHwContext %p pNewAECUpdate %p pNewAWBUpdate %p",
                           pInputData->pHwContext,
                           pInputData->pAECUpdateData,
                           pInputData->pAWBUpdateData);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCC13::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCC13::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_COLOR_CORRECT_COLOR_CORRECT_COEFF_A_CFG_0,
                                              (sizeof(BPSCC13RegConfig) / RegisterWidthInBytes),
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
// BPSCC13::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCC13::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        CC13OutputData outputData = { PipelineType::BPS, { { 0 } } };

        outputData.regCommand.BPS.pRegCmd = &m_regCmd;

        result = IQInterface::CC13CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "CC13 Calculation Failed.");
        }
        if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
        {
            DumpRegConfig();
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
// BPSCC13::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSCC13::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->colorCorrectParameters.moduleCfg.EN = m_moduleEnable;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(BPSCC13RegConfig) <=
                           sizeof(pInputData->pBPSTuningMetadata->BPSCCData.colorCorrectionConfig));

        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSCCData.colorCorrectionConfig, &m_regCmd, sizeof(BPSCC13RegConfig));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCC13::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCC13::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            result = RunCalculation(pInputData);
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
// BPSCC13::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSCC13::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCC13::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSCC13::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "********  BPS SCC13 ********  \n");
        CamX::OsUtils::FPrintF(pFile, "coefficient A Config 0 = 0x%x\n", m_regCmd.coefficientAConfig0);
        CamX::OsUtils::FPrintF(pFile, "coefficient A Config 1 = 0x%x\n", m_regCmd.coefficientAConfig1);
        CamX::OsUtils::FPrintF(pFile, "coefficient B Config 0 = 0x%x\n", m_regCmd.coefficientBConfig0);
        CamX::OsUtils::FPrintF(pFile, "coefficient B Config 1 = 0x%x\n", m_regCmd.coefficientBConfig1);
        CamX::OsUtils::FPrintF(pFile, "coefficient C Config 0 = 0x%x\n", m_regCmd.coefficientCConfig0);
        CamX::OsUtils::FPrintF(pFile, "coefficient C Config 1 = 0x%x\n", m_regCmd.coefficientCConfig1);
        CamX::OsUtils::FPrintF(pFile, "Offset K Config 0      = 0x%x\n", m_regCmd.offsetKConfig0);
        CamX::OsUtils::FPrintF(pFile, "Offset K Config 1      = 0x%x\n", m_regCmd.offsetKConfig1);
        CamX::OsUtils::FPrintF(pFile, "Shift M Config         = 0x%x\n", m_regCmd.shiftMConfig);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }

    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "coefficient A Config 0 [0x%x]", m_regCmd.coefficientAConfig0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "coefficient A Config 1 [0x%x]", m_regCmd.coefficientAConfig1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "coefficient B Config 0 [0x%x]", m_regCmd.coefficientBConfig0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "coefficient B Config 1 [0x%x]", m_regCmd.coefficientBConfig1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "coefficient C Config 0 [0x%x]", m_regCmd.coefficientCConfig0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "coefficient C Config 1 [0x%x]", m_regCmd.coefficientCConfig1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Offset K Config 0      [0x%x]", m_regCmd.offsetKConfig0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Offset K Config 1      [0x%x]", m_regCmd.offsetKConfig1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Shift M Config         [0x%x]", m_regCmd.shiftMConfig);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCC13::BPSCC13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSCC13::BPSCC13(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type         = ISPIQModuleType::BPSCC;
    m_moduleEnable = FALSE;
    m_pChromatix   = NULL;
    m_cmdLength    = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSCC13RegConfig) / RegisterWidthInBytes);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCC13::~BPSCC13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSCC13::~BPSCC13()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
