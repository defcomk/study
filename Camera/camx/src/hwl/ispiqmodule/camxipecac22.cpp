////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @file  camxipecac22.cpp
// @brief IPECAC class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxtuningdatamanager.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxipecac22.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPECAC22::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPECAC22* pModule = CAMX_NEW IPECAC22(pCreateData->pNodeIdentifier);

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Module initialization failed !!");
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
// IPECAC22::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPECAC22::Initialize()
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
// IPECAC22::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPECAC22::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(cac_2_2_0::cac22_rgn_dataType) * (CAC22MaxNoLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for cac_2_2_0::cac22_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPECAC22::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pInputData) && (NULL != pInputData->pCalculatedData))
    {
        IpeIQSettings* pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

        // SW CAC can be enabled independently either CAC2 or SNR. HW CAC should be enabled with region enable
        pIPEIQSettings->cacParameters.moduleCfg.EN = m_moduleEnable && m_enableCAC2;
        pIPEIQSettings->cacParameters.cac2Enable   = m_moduleEnable && m_enableCAC2;
        pIPEIQSettings->cacParameters.cacSnrEnable = m_dependenceData.enableSNR;

        // Disable CAC module for realtime streams
        if ((TRUE == pInputData->pipelineIPEData.realtimeFlag) &&
           (TRUE == pInputData->pipelineIPEData.compressiononOutput))
        {
            pIPEIQSettings->cacParameters.moduleCfg.EN = 0;
        }

        // Post tuning metadata if setting is enabled
        if (NULL != pInputData->pIPETuningMetadata)
        {
            CAMX_STATIC_ASSERT(sizeof(IPECACRegCmd) <= sizeof(pInputData->pIPETuningMetadata->IPECACData.CACConfig));

            Utils::Memcpy(&pInputData->pIPETuningMetadata->IPECACData.CACConfig,
                          &m_regCmd,
                          sizeof(IPECACRegCmd));
        }
        pInputData->pCalculatedData->metadata.colorCorrectionAberrationMode = m_CACMode;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Input Null pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPECAC22::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPreLTM];

    CAMX_ASSERT(NULL != pCmdBuffer);
    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_CAC_CFG_0,
                                          IPECACRegLength,
                                          reinterpret_cast<UINT32*>(&m_regCmd));
    CAMX_ASSERT(CamxResultSuccess == result);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPECAC22::ValidateDependenceParams(
    const ISPInputData* pInputData
    ) const
{
    CamxResult result = CamxResultSuccess;

    // @todo (CAMX-730) validate dependency parameters
    if ((NULL == pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPreLTM]) ||
        (NULL == pInputData->pipelineIPEData.pIPEIQSettings)                  ||
        (NULL == pInputData->pHwContext)                                      ||
        (NULL == pInputData->pAECUpdateData))
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc,
            "Invalid Input: pInputData %p ppIPECmdBuffer[CmdBufferPreLTM] %p pIPEIQSettings %p",
            pInputData,
            pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPreLTM],
            pInputData->pipelineIPEData.pIPEIQSettings);

        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IPECAC22::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPECAC22::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                 &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData) &&
        (NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pHALTagsData))
    {
        AECFrameControl* pNewAECUpdate = pInputData->pAECUpdateData;
        ISPHALTagsData*  pHALTagsData  = pInputData->pHALTagsData;

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->CACEnable;

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
                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_cac22_ipe(
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
                        m_moduleEnable              = m_pChromatix->enable_section.cac_global_enable;
                        isChanged                   = (TRUE == m_moduleEnable);
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to get Chromatix");
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Tuning Pointer is NULL");
            }

            if (NULL != pHALTagsData)
            {
                if ((TRUE == m_moduleEnable) &&
                    (ColorCorrectionAberrationModeOff == pHALTagsData->colorCorrectionAberrationMode))
                {
                    m_moduleEnable = FALSE;
                    isChanged = FALSE;
                    m_CACMode = pHALTagsData->colorCorrectionAberrationMode;
                }
                else
                {
                    // Check Whether CAC is enabled from chromatix
                    if (NULL != m_dependenceData.pChromatix &&
                        TRUE == m_dependenceData.pChromatix->enable_section.cac_global_enable)
                    {
                        m_CACMode = pHALTagsData->colorCorrectionAberrationMode;
                        m_moduleEnable = m_dependenceData.pChromatix->enable_section.cac_global_enable;
                        isChanged = TRUE;
                    }
                    else
                    {
                        m_CACMode = ColorCorrectionAberrationModeOff;
                    }
                }
            }
        }
        if (NULL != pHALTagsData)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "CAC hal %d m_CACMode %d m_moduleEnable %d",
                pHALTagsData->colorCorrectionAberrationMode, m_CACMode, m_moduleEnable);
        }

        // @todo (CAMX-2576) Map CAC2 parameters from ISP to algorithm interface
        m_dependenceData.enableSNR  = TRUE;
        m_dependenceData.resolution = 0;

        if (TRUE == m_moduleEnable)
        {
            if (FALSE == Utils::FEqual(m_dependenceData.luxIndex, pNewAECUpdate->luxIndex)      ||
                FALSE == Utils::FEqual(m_dependenceData.digitalGain,
                                       pNewAECUpdate->exposureInfo[ExposureIndexSafe].linearGain))
            {
                m_dependenceData.luxIndex    = pNewAECUpdate->luxIndex;
                m_dependenceData.digitalGain = pNewAECUpdate->exposureInfo[ExposureIndexSafe].linearGain;
                isChanged = TRUE;
            }
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "CAC22 Module is not enabled");
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc,
                           "Invalid Input: pNewAECUpdate %p pNewAWBUpdate %p pHwContext %p",
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
// IPECAC22::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPECAC22::RunCalculation(
    const ISPInputData* pInputData)
{
    CAMX_UNREFERENCED_PARAM(pInputData);

    CamxResult      result = CamxResultSuccess;
    CAC22OutputData outputData;

    outputData.pRegCmd = &m_regCmd;

    result = IQInterface::IPECAC22CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);
    if (CamxResultSuccess == result)
    {
        m_enableCAC2 = outputData.enableCAC2;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "CAC22 Calculation Failed.");
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPECAC22::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        // Check if dependency is published and valid
        result = ValidateDependenceParams(pInputData);

        if (CamxResultSuccess == result)
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
// IPECAC22::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPECAC22::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::IPECAC22
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPECAC22::IPECAC22(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    UINT size           = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IPECACRegCmd) / RegisterWidthInBytes);
    m_type              = ISPIQModuleType::IPECAC;
    m_moduleEnable      = TRUE;
    m_cmdLength         = size * sizeof(UINT32);
    m_numLUT            = 0;
    m_offsetLUT         = 0;
    m_CACMode           = ColorCorrectionAberrationModeFast;
    m_pChromatix        = NULL;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE CAC m_cmdLength %d ", m_cmdLength);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::~IPECAC22
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPECAC22::~IPECAC22()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPECAC22::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPECAC22::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE CAC22 ********\n");
        CamX::OsUtils::FPrintF(pFile, "******** IPE CAC22 [HEX] ********\n");
        CamX::OsUtils::FPrintF(pFile, "CAC22.lumaThreshold.bitfields.Y_SATURATION_THR     = %x\n",
            m_regCmd.lumaThreshold.bitfields.Y_SATURATION_THR);
        CamX::OsUtils::FPrintF(pFile, "CAC22.lumaThreshold.bitfields.Y_SPOT_THR           = %x\n",
            m_regCmd.lumaThreshold.bitfields.Y_SPOT_THR);
        CamX::OsUtils::FPrintF(pFile, "CAC22.chromaThreshold.bitfields.C_SATURATION_THR   = %x\n",
            m_regCmd.chromaThreshold.bitfields.C_SATURATION_THR);
        CamX::OsUtils::FPrintF(pFile, "CAC22.chromaThreshold.bitfields.C_SPOT_THR         = %x\n",
            m_regCmd.chromaThreshold.bitfields.C_SPOT_THR);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }

    CAMX_LOG_INFO(CamxLogGroupIQMod, "Y Saturation threshold[0x%x]  ", m_regCmd.lumaThreshold.bitfields.Y_SATURATION_THR);
    CAMX_LOG_INFO(CamxLogGroupIQMod, "Y Spot threshold[0x%x]        ", m_regCmd.lumaThreshold.bitfields.Y_SPOT_THR);
    CAMX_LOG_INFO(CamxLogGroupIQMod, "Cr Saturation threshold[0x%x] ", m_regCmd.chromaThreshold.bitfields.C_SATURATION_THR);
    CAMX_LOG_INFO(CamxLogGroupIQMod, "Cr Spot threshold[0x%x]       ", m_regCmd.chromaThreshold.bitfields.C_SPOT_THR);

    return;
}

CAMX_NAMESPACE_END
