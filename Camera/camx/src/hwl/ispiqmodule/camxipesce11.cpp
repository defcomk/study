////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipesce11.cpp
/// @brief IPE SCE class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxipesce11.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "ipe_data.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPESCE11::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPESCE11* pModule = CAMX_NEW IPESCE11(pCreateData->pNodeIdentifier);

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (result != CamxResultSuccess)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initilize common library data, no memory");
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
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPESCE11::Initialize()
{
    CamxResult result = CamxResultSuccess;
    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initilize common library data, no memory");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPESCE11::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(sce_1_1_0::sce11_rgn_dataType) * (SCE11MaxNoLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for sce_1_1_0::sce11_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPESCE11::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pipelineIPEData.pIPEIQSettings)
    {
        IpeIQSettings* pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

        pIPEIQSettings->skinEnhancementParameters.moduleCfg.EN = m_moduleEnable;

        // Post tuning metadata if setting is enabled
        if (NULL != pInputData->pIPETuningMetadata)
        {
            CAMX_STATIC_ASSERT(sizeof(IPESCERegCmd) <=
                               sizeof(pInputData->pIPETuningMetadata->IPESCEData.SCEConfig));

            Utils::Memcpy(&pInputData->pIPETuningMetadata->IPESCEData.SCEConfig,
                          &m_regCmd,
                          sizeof(IPESCERegCmd));
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input data");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPESCE11::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPostLTM];

    CAMX_ASSERT(NULL != pCmdBuffer);
    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX1_COORD_CFG_0,
                                          IPESCERegLength,
                                          reinterpret_cast<UINT32*>(&m_regCmd));
    CAMX_ASSERT(CamxResultSuccess == result);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPESCE11::ValidateDependenceParams(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-730) validate dependency parameters
    if ((NULL == pInputData)                                                   ||
        (NULL == pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPostLTM]) ||
        (NULL == pInputData->pipelineIPEData.pIPEIQSettings))
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid Input: pInputData %p", pInputData);

        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IPESCE11::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPESCE11::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                  &&
        (NULL != pInputData->pAECUpdateData)  &&
        (NULL != pInputData->pAWBUpdateData)  &&
        (NULL != pInputData->pHwContext)      &&
        (NULL != pInputData->pHALTagsData))
    {
        AECFrameControl* pNewAECUpdate   = pInputData->pAECUpdateData;
        AWBFrameControl* pNewAWBUpdate   = pInputData->pAWBUpdateData;
        FLOAT            newExposureTime = static_cast<FLOAT>(pNewAECUpdate->exposureInfo[ExposureIndexSafe].exposureTime);

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->SkinEnhancementEnable;

            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            if (NULL != pInputData->pTuningData)
            {
                TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
                CAMX_ASSERT(NULL != pTuningManager);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if ((TRUE == pInputData->tuningModeChanged)    &&
                    (TRUE == pTuningManager->IsValidChromatix()))
                {
                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_sce11_ipe(
                                       reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                       pInputData->pTuningData->noOfSelectionParameter);
                }

                CAMX_ASSERT(NULL != m_pChromatix);
                if (NULL != m_pChromatix)
                {
                    if (m_pChromatix != m_dependenceData.pChromatix)
                    {
                        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "updating chromatix pointer");
                        m_dependenceData.pChromatix = m_pChromatix;
                        m_moduleEnable              = m_pChromatix->enable_section.sce_enable;

                        isChanged                   = (TRUE == m_moduleEnable);
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Tuning Pointer is NULL");
            }
        }

        // Check for manual tone map mode, if yes disable SCE
        if ((TRUE                     == m_moduleEnable)                          &&
            (TonemapModeContrastCurve == pInputData->pHALTagsData->tonemapCurves.tonemapMode))
        {
            m_moduleEnable = FALSE;
            isChanged      = FALSE;
        }

        if ((TRUE == m_moduleEnable) &&
            ((FALSE == Utils::FEqual(m_dependenceData.luxIndex, pNewAECUpdate->luxIndex))         ||
             (FALSE == Utils::FEqual(m_dependenceData.realGain,
                                     pNewAECUpdate->exposureInfo[ExposureIndexSafe].linearGain))  ||
             (FALSE == Utils::FEqual(m_dependenceData.AECSensitivity,
                                     pNewAECUpdate->exposureInfo[ExposureIndexSafe].sensitivity)) ||
             (FALSE == Utils::FEqual(m_dependenceData.exposureTime, newExposureTime))))

        {
            m_dependenceData.luxIndex       = pNewAECUpdate->luxIndex;
            m_dependenceData.realGain       = pNewAECUpdate->exposureInfo[ExposureIndexSafe].linearGain;
            m_dependenceData.AECSensitivity = pNewAECUpdate->exposureInfo[ExposureIndexSafe].sensitivity;
            m_dependenceData.exposureTime   = newExposureTime;

            if (FALSE == Utils::FEqual(pNewAECUpdate->exposureInfo[ExposureIndexShort].sensitivity, 0.0f))
            {
                m_dependenceData.DRCGain = pNewAECUpdate->exposureInfo[ExposureIndexSafe].sensitivity /
                    pNewAECUpdate->exposureInfo[ExposureIndexShort].sensitivity;
            }
            else
            {
                m_dependenceData.DRCGain = 0.0f;
                CAMX_LOG_WARN(CamxLogGroupPProc, "Invalid short exposure, cannot obtain DRC gain");
            }

            isChanged = TRUE;
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Invalid Input: pNewAECUpdate %x pNewAWBUpdate %x  HwContext %x",
                           pInputData->pAECUpdateData,
                           pInputData->pAWBUpdateData,
                           pInputData->pHwContext);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPESCE11::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult      result      = CamxResultSuccess;
    SCE11OutputData outputData  = { 0 };

    outputData.pRegCmd = &m_regCmd;

    result = IQInterface::SCE11CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "SCE11 Calculation Failed. result %d", result);
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPESCE11::Execute(
    ISPInputData* pInputData)
{
    CamxResult result;

    if (NULL != pInputData)
    {
        // Check if dependency is published and valid
        result = ValidateDependenceParams(pInputData);

        if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);
            }

            // Regardless of any update in dependency parameters, command buffers and IQSettings/Metadata shall be updated.
            if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
            {
                result = CreateCmdList(pInputData);
            }

            if (CamxResultSuccess == result)
            {
                result = UpdateIPEInternalData(pInputData);
            }

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Operation failed %d", result);
            }
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
// IPESCE11::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPESCE11::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::IPESCE11
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPESCE11::IPESCE11(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type              = ISPIQModuleType::IPESCE;
    m_moduleEnable      = TRUE;
    m_numLUT            = 0;
    m_offsetLUT         = 0;
    m_cmdLength         = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IPESCERegCmd) / RegisterWidthInBytes) *
                          sizeof(UINT32);
    m_pChromatix        = NULL;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE SCE m_cmdLength %d ", m_cmdLength);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESCE11::~IPESCE11
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPESCE11::~IPESCE11()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPESCE11::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPESCE11::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE SCE11 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* SCE11 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex1.cfg0        = %x\n", m_regCmd.vertex1.cfg0);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex1.cfg1        = %x\n", m_regCmd.vertex1.cfg1);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex1.cfg2        = %x\n", m_regCmd.vertex1.cfg2);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex1.cfg3        = %x\n", m_regCmd.vertex1.cfg3);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex1.cfg4        = %x\n", m_regCmd.vertex1.cfg4);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex2.cfg0        = %x\n", m_regCmd.vertex2.cfg0);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex2.cfg1        = %x\n", m_regCmd.vertex2.cfg1);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex2.cfg2        = %x\n", m_regCmd.vertex2.cfg2);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex2.cfg3        = %x\n", m_regCmd.vertex2.cfg3);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex2.cfg4        = %x\n", m_regCmd.vertex2.cfg4);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex3.cfg0        = %x\n", m_regCmd.vertex3.cfg0);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex3.cfg1        = %x\n", m_regCmd.vertex3.cfg1);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex3.cfg2        = %x\n", m_regCmd.vertex3.cfg2);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex3.cfg3        = %x\n", m_regCmd.vertex3.cfg3);
        CamX::OsUtils::FPrintF(pFile, "SCE11.vertex3.cfg4        = %x\n", m_regCmd.vertex3.cfg4);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crCoefficient.cfg0  = %x\n", m_regCmd.crCoefficient.cfg0);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crCoefficient.cfg1  = %x\n", m_regCmd.crCoefficient.cfg1);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crCoefficient.cfg2  = %x\n", m_regCmd.crCoefficient.cfg2);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crCoefficient.cfg3  = %x\n", m_regCmd.crCoefficient.cfg3);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crCoefficient.cfg4  = %x\n", m_regCmd.crCoefficient.cfg4);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crCoefficient.cfg5  = %x\n", m_regCmd.crCoefficient.cfg5);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbCoefficient.cfg0  = %x\n", m_regCmd.cbCoefficient.cfg0);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbCoefficient.cfg1  = %x\n", m_regCmd.cbCoefficient.cfg1);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbCoefficient.cfg2  = %x\n", m_regCmd.cbCoefficient.cfg2);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbCoefficient.cfg3  = %x\n", m_regCmd.cbCoefficient.cfg3);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbCoefficient.cfg4  = %x\n", m_regCmd.cbCoefficient.cfg4);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbCoefficient.cfg5  = %x\n", m_regCmd.cbCoefficient.cfg5);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crOffset.cfg0       = %x\n", m_regCmd.crOffset.cfg0);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crOffset.cfg1       = %x\n", m_regCmd.crOffset.cfg1);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crOffset.cfg2       = %x\n", m_regCmd.crOffset.cfg2);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crOffset.cfg3       = %x\n", m_regCmd.crOffset.cfg3);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crOffset.cfg4       = %x\n", m_regCmd.crOffset.cfg4);
        CamX::OsUtils::FPrintF(pFile, "SCE11.crOffset.cfg5       = %x\n", m_regCmd.crOffset.cfg5);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbOffset.cfg0       = %x\n", m_regCmd.cbOffset.cfg0);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbOffset.cfg1       = %x\n", m_regCmd.cbOffset.cfg1);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbOffset.cfg2       = %x\n", m_regCmd.cbOffset.cfg2);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbOffset.cfg3       = %x\n", m_regCmd.cbOffset.cfg3);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbOffset.cfg4       = %x\n", m_regCmd.cbOffset.cfg4);
        CamX::OsUtils::FPrintF(pFile, "SCE11.cbOffset.cfg5       = %x\n", m_regCmd.cbOffset.cfg5);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
}
CAMX_NAMESPACE_END
