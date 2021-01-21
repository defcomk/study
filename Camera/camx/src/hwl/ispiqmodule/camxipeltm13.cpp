////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipeltm13.cpp
/// @brief IPELTM class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "camxdefs.h"
#include "camxipeltm13.h"
#include "camxiqinterface.h"
#include "camxispiqmodule.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "ipe_data.h"
#include "parametertuningtypes.h"
#include "camxtitan17xcontext.h"

CAMX_NAMESPACE_BEGIN

static UINT32 gamma64Table[] =
{
    0,    64, 108, 144, 176, 205, 232, 258, 282, 304, 326, 347, 367, 386, 405, 423, 441,
    458, 475, 491, 507, 523, 538, 553, 568, 583, 597, 611, 625, 638, 651, 665, 678, 690,
    703, 715, 728, 740, 752, 764, 775, 787, 798, 810, 821, 832, 843, 854, 865, 875, 886,
    896, 907, 917, 927, 937, 947, 957, 967, 977, 987, 996, 1006, 1015, 1023,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPELTM13* pModule = CAMX_NEW IPELTM13(pCreateData->pNodeIdentifier, pCreateData);

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Module initialization failed !!");
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
// IPELTM13::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::Initialize()
{
    CamxResult result = CamxResultSuccess;

    // Precompute Offset of Look up table with LUT command buffer for ease of patching
    m_offsetLUTCmdBuffer[LTMIndexWeight]      = 0;
    m_offsetLUTCmdBuffer[LTMIndexLA0]         = (IPELTMLUTNumEntries[LTMIndexWeight]) * sizeof(UINT32);
    m_offsetLUTCmdBuffer[LTMIndexLA1]         =
        IPELTMLUTNumEntries[LTMIndexLA0] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexLA0];
    m_offsetLUTCmdBuffer[LTMIndexCurve]       =
        IPELTMLUTNumEntries[LTMIndexLA1] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexLA1];

    m_offsetLUTCmdBuffer[LTMIndexScale]       =
        IPELTMLUTNumEntries[LTMIndexCurve] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexCurve];
    m_offsetLUTCmdBuffer[LTMIndexMask]        =
        IPELTMLUTNumEntries[LTMIndexScale] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexScale];

    m_offsetLUTCmdBuffer[LTMIndexLCEPositive] =
        IPELTMLUTNumEntries[LTMIndexMask] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexMask];
    m_offsetLUTCmdBuffer[LTMIndexLCENegative] =
        IPELTMLUTNumEntries[LTMIndexLCEPositive] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexLCEPositive];
    m_offsetLUTCmdBuffer[LTMIndexRGamma0]     =
        IPELTMLUTNumEntries[LTMIndexLCENegative] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexLCENegative];

    m_offsetLUTCmdBuffer[LTMIndexRGamma1]     =
        IPELTMLUTNumEntries[LTMIndexRGamma0] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexRGamma0];
    m_offsetLUTCmdBuffer[LTMIndexRGamma2]     =
        IPELTMLUTNumEntries[LTMIndexRGamma1] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexRGamma1];
    m_offsetLUTCmdBuffer[LTMIndexRGamma3]     =
        IPELTMLUTNumEntries[LTMIndexRGamma2] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexRGamma2];
    m_offsetLUTCmdBuffer[LTMIndexRGamma4]     =
        IPELTMLUTNumEntries[LTMIndexRGamma3] * sizeof(UINT32) + m_offsetLUTCmdBuffer[LTMIndexRGamma3];

    m_dependenceData.pGammaPrev  = m_gammaPrev;
    m_dependenceData.pIGammaPrev = m_igammaPrev;
    m_pLUTCmdBufferManager       = NULL;

    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initilize common library data, no memory");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::FillCmdBufferManagerParams(
    const ISPInputData*     pInputData,
    IQModuleCmdBufferParam* pParam)
{
    CamxResult result                  = CamxResultSuccess;
    ResourceParams* pResourceParams    = NULL;
    CHAR*           pBufferManagerName = NULL;

    if ((NULL != pParam) && (NULL != pParam->pCmdBufManagerParam) && (NULL != pInputData))
    {
        // The Resource Params and Buffer Manager Name will be freed by caller Node
        pResourceParams = static_cast<ResourceParams*>(CAMX_CALLOC(sizeof(ResourceParams)));
        if (NULL != pResourceParams)
        {
            pBufferManagerName = static_cast<CHAR*>(CAMX_CALLOC((sizeof(CHAR) * MaxStringLength256)));
            if (NULL != pBufferManagerName)
            {
                OsUtils::SNPrintF(pBufferManagerName, (sizeof(CHAR) * MaxStringLength256), "CBM_%s_%s",
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "IPELTM13");
                pResourceParams->resourceSize                 = IPELTM13LUTBufferSizeInDwords * sizeof(UINT32);
                pResourceParams->poolSize                     = (pInputData->requestQueueDepth * pResourceParams->resourceSize);
                pResourceParams->usageFlags.cmdBuffer         = 1;
                pResourceParams->cmdParams.type               = CmdType::CDMDMI;
                pResourceParams->alignment                    = CamxCommandBufferAlignmentInBytes;
                pResourceParams->cmdParams.enableAddrPatching = 0;
                pResourceParams->cmdParams.maxNumNestedAddrs  = 0;
                pResourceParams->memFlags                     = CSLMemFlagUMDAccess;
                pResourceParams->pDeviceIndices               = pInputData->pipelineIPEData.pDeviceIndex;
                pResourceParams->numDevices                   = 1;

                pParam->pCmdBufManagerParam[pParam->numberOfCmdBufManagers].pBufferManagerName = pBufferManagerName;
                pParam->pCmdBufManagerParam[pParam->numberOfCmdBufManagers].pParams            = pResourceParams;
                pParam->pCmdBufManagerParam[pParam->numberOfCmdBufManagers].ppCmdBufferManager = &m_pLUTCmdBufferManager;
                pParam->numberOfCmdBufManagers++;

            }
            else
            {
                result = CamxResultENoMemory;
                CAMX_FREE(pResourceParams);
                pResourceParams = NULL;
                CAMX_LOG_ERROR(CamxLogGroupISP, "Out Of Memory");
            }
        }
        else
        {
            result = CamxResultENoMemory;
            CAMX_LOG_ERROR(CamxLogGroupISP, "Out Of Memory");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Input Error %p", pParam);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(ltm_1_3_0::ltm13_rgn_dataType) * (LTM13MaxNoLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for ltm_1_3_0::ltm13_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::WriteLUTInDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::WriteLUTInDMI(
    const ISPInputData* pInputData)
{
    CamxResult  result        = CamxResultSuccess;
    CmdBuffer*  pDMICmdBuffer = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferDMIHeader];

    if ((NULL != pDMICmdBuffer) && (NULL != m_pLUTCmdBuffer))
    {
        // This offset holds CDM header of LTM LUTs, store this offset and node will patch it in top level payload
        m_offsetLUT = pDMICmdBuffer->GetResourceUsedDwords() * sizeof(UINT32);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexWeight + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexWeight],
                                         IPELTMLUTNumEntries[LTMIndexWeight] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexLA0 + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexLA0],
                                         IPELTMLUTNumEntries[LTMIndexLA0] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexLA1 + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexLA1],
                                         IPELTMLUTNumEntries[LTMIndexLA1] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexCurve + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexCurve],
                                         IPELTMLUTNumEntries[LTMIndexCurve] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexScale + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexScale],
                                         IPELTMLUTNumEntries[LTMIndexScale] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexMask + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexMask],
                                         IPELTMLUTNumEntries[LTMIndexMask] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexLCEPositive + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexLCEPositive],
                                         IPELTMLUTNumEntries[LTMIndexLCEPositive] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexLCENegative + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexLCENegative],
                                         IPELTMLUTNumEntries[LTMIndexLCENegative] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexRGamma0 + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexRGamma0],
                                         IPELTMLUTNumEntries[LTMIndexRGamma0] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexRGamma1 + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexRGamma1],
                                         IPELTMLUTNumEntries[LTMIndexRGamma1] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexRGamma2 + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexRGamma2],
                                         IPELTMLUTNumEntries[LTMIndexRGamma2] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexRGamma3 + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexRGamma3],
                                         IPELTMLUTNumEntries[LTMIndexRGamma3] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_LTM_DMI_CFG,
                                         LTMIndexRGamma4 + 1,
                                         m_pLUTCmdBuffer,
                                         m_offsetLUTCmdBuffer[LTMIndexRGamma4],
                                         IPELTMLUTNumEntries[LTMIndexRGamma4] * sizeof(UINT32));
        CAMX_ASSERT(CamxResultSuccess == result);

    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    CamxResult      result          = CamxResultSuccess;
    IpeIQSettings*  pIPEIQSettings  = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

    // set LTM module enable based on ANR status or ipe configIO topology type
    pIPEIQSettings->ltmParameters.moduleCfg.EN = GetLTMEnableStatus(pInputData, pIPEIQSettings);

    // hardcode to 0 to disable Reverse Gamma. Will set to igamma_en when metadata works for IFE/BPS Gamma Output
    if (TRUE == m_ignoreChromatixRGammaFlag)
    {
        pIPEIQSettings->ltmParameters.moduleCfg.RGAMMA_EN = 0;
    }
    else
    {
        pIPEIQSettings->ltmParameters.moduleCfg.RGAMMA_EN = m_moduleConfig.moduleConfig.bitfields.RGAMMA_EN;
    }
    pIPEIQSettings->ltmParameters.moduleCfg.LA_EN           = m_moduleConfig.moduleConfig.bitfields.LA_EN;
    pIPEIQSettings->ltmParameters.moduleCfg.MASK_EN         = m_moduleConfig.moduleConfig.bitfields.MASK_EN;

    pIPEIQSettings->ltmParameters.dcBinInitCnt              = m_regCmd.LTMDataCollectionConfig0.bitfields.BIN_INIT_CNT;
    pIPEIQSettings->ltmParameters.rgb2yC1                   = m_regCmd.LTMRGB2YConfig.bitfields.C1;
    pIPEIQSettings->ltmParameters.rgb2yC2                   = m_regCmd.LTMRGB2YConfig.bitfields.C2;
    pIPEIQSettings->ltmParameters.rgb2yC3                   = m_regCmd.LTMRGB2YConfig.bitfields.C3;
    pIPEIQSettings->ltmParameters.rgb2yC4                   = m_regCmd.LTMRGB2YConfig.bitfields.C4;

    pIPEIQSettings->ltmParameters.ipLceThd                  = m_regCmd.LTMImageProcessingConfig6.bitfields.LCE_THD;
    pIPEIQSettings->ltmParameters.ipYRatioMax               = m_regCmd.LTMImageProcessingConfig6.bitfields.YRATIO_MAX;

    pIPEIQSettings->ltmParameters.maskD0                    = m_regCmd.LTMMaskFilterCoefficientConfig0.bitfields.D0;
    pIPEIQSettings->ltmParameters.maskD1                    = m_regCmd.LTMMaskFilterCoefficientConfig0.bitfields.D1;
    pIPEIQSettings->ltmParameters.maskD2                    = m_regCmd.LTMMaskFilterCoefficientConfig0.bitfields.D2;
    pIPEIQSettings->ltmParameters.maskD3                    = m_regCmd.LTMMaskFilterCoefficientConfig0.bitfields.D3;
    pIPEIQSettings->ltmParameters.maskD4                    = m_regCmd.LTMMaskFilterCoefficientConfig0.bitfields.D4;
    pIPEIQSettings->ltmParameters.maskD5                    = m_regCmd.LTMMaskFilterCoefficientConfig0.bitfields.D5;

    pIPEIQSettings->ltmParameters.maskShift                 = m_regCmd.LTMMaskFilterCoefficientConfig1.bitfields.SHIFT;
    pIPEIQSettings->ltmParameters.maskScale                 = m_regCmd.LTMMaskFilterCoefficientConfig1.bitfields.SCALE;

    pIPEIQSettings->ltmParameters.downscaleMNHorTermination =
        m_regCmd.LTMDownScaleMNYConfig.bitfields.HORIZONTAL_TERMINATION_EN;
    pIPEIQSettings->ltmParameters.downscaleMNVerTermination = m_regCmd.LTMDownScaleMNYConfig.bitfields.VERTICAL_TERMINATION_EN;
    pIPEIQSettings->ltmParameters.dataCollectionEnabled     = m_moduleConfig.moduleConfig.bitfields.DC_EN;
    pIPEIQSettings->ltmParameters.ltmImgProcessEn           = m_moduleConfig.moduleConfig.bitfields.IP_EN;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pIPETuningMetadata)
    {
        CAMX_STATIC_ASSERT((IPELTM13LUTBufferSizeInDwords * sizeof(UINT32)) ==
                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LTMLUT));

        if (NULL != m_pLTMLUTs)
        {
            Utils::Memcpy(&pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LTMLUT,
                          m_pLTMLUTs,
                          (IPELTM13LUTBufferSizeInDwords * sizeof(UINT32)));
        }

        pInputData->pIPETuningMetadata->IPELTMExposureData.exposureIndex[IPELTMExposureIndexPrevious] =
            m_dependenceData.prevExposureIndex;
        pInputData->pIPETuningMetadata->IPELTMExposureData.exposureIndex[IPELTMExposureIndexCurrent] =
            m_dependenceData.exposureIndex;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    result = WriteLUTInDMI(pInputData);

    if (CamxResultSuccess != result)
    {
        result = CamxResultEFailed;
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to update DMI buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPELTM13::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                  &&
        (NULL != pInputData->pHALTagsData)    &&
        (NULL != pInputData->pAECUpdateData)  &&
        (NULL != pInputData->pCalculatedData) &&
        (NULL != pInputData->pHwContext))
    {
        ISPHALTagsData* pHALTagsData = pInputData->pHALTagsData;
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->LTMEnable;
            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            if (NULL != pInputData->pTuningData)
            {
                TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
                CAMX_ASSERT(NULL != pTuningManager);

                if (NULL != pTuningManager)
                {
                    if (TRUE == pTuningManager->IsValidChromatix())
                    {
                        // Search through the tuning data (tree), only when there
                        // are changes to the tuning mode data as an optimization
                        if (TRUE == pInputData->tuningModeChanged)
                        {
                            m_pChromatix = pTuningManager->GetChromatix()->GetModule_ltm13_ipe(
                                reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                pInputData->pTuningData->noOfSelectionParameter);

                            m_ptmcChromatix = pTuningManager->GetChromatix()->GetModule_tmc10_sw(
                                reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                pInputData->pTuningData->noOfSelectionParameter);
                        }
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid Chromatix tuning data");
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "NULL Tuning data pointer");
                }

                CAMX_ASSERT(NULL != m_pChromatix);

                m_dependenceData.ltmLceStrength = pInputData->pHALTagsData->ltmContrastStrength.ltmDynamicContrastStrength;
                m_dependenceData.ltmDarkBoostStrength = pInputData->pHALTagsData->ltmContrastStrength.ltmDarkBoostStrength;
                m_dependenceData.ltmBrightSupressStrength =
                    pInputData->pHALTagsData->ltmContrastStrength.ltmBrightSupressStrength;

                if (NULL != m_pChromatix)
                {
                    if ((m_pChromatix != m_dependenceData.pChromatix) ||
                        (m_pChromatix->SymbolTableID != m_dependenceData.pChromatix->SymbolTableID) ||
                        (m_pChromatix->enable_section.ltm_enable != m_moduleEnable))
                    {
                        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "updating chromatix pointer");
                        m_dependenceData.pChromatix = m_pChromatix;
                        if (NULL != m_ptmcChromatix)
                        {
                            m_pTMCInput.pChromatix      = m_ptmcChromatix;
                        }
                        m_moduleEnable              = m_pChromatix->enable_section.ltm_enable;

                        if (TRUE == m_moduleEnable)
                        {
                            isChanged = TRUE;
                        }
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to get Chromatix");
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "NULL Tuning Pointer");
            }

            if ((TonemapModeContrastCurve == pHALTagsData->tonemapCurves.tonemapMode))
            {
                m_moduleEnable = FALSE;
                isChanged = FALSE;
            }
        }

        if (TRUE == m_moduleEnable)
        {
            UINT32* pGammaOutput = NULL;

            if ((TRUE == m_useHardcodedGamma) ||
                (FALSE == pInputData->pCalculatedData->gammaOutput.isGammaValid))
            {
                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "use hardcodedGamma for iGamma");
                pGammaOutput = &gamma64Table[0];
            }
            else
            {
                pGammaOutput = &pInputData->pCalculatedData->gammaOutput.gammaG[0];
            }

            if (TRUE == IQInterface::s_interpolationTable.LTM13TriggerUpdate(&pInputData->triggerData, &m_dependenceData))
            {
                isChanged = TRUE;
            }

            if (pInputData->parentNodeID == IFE)
            {
                m_dependenceData.prevExposureIndex = m_dependenceData.exposureIndex;
                m_dependenceData.exposureIndex     = m_dependenceData.luxIndex;
            }

            if (TRUE != pInputData->registerBETEn)
            {
                if ((pInputData->frameNum <= (pInputData->maximumPipelineDelay + 1)) || (BPS == pInputData->parentNodeID))
                {
                    // For first valid request the previous exposre index is 0 so assign the current exposre index
                    m_dependenceData.prevExposureIndex = m_dependenceData.exposureIndex;
                }
            }
            else
            {
                // Skip the attenuation for the first valid 3A Input Data
                if ((4 >= pInputData->frameNum) || (BPS == pInputData->parentNodeID))
                {
                    // For first valid request the previous exposre index is 0 so assign the current exposre index
                    m_dependenceData.prevExposureIndex = m_dependenceData.exposureIndex;
                }
            }

            for (INT i = 0; i < 65; i++)
            {
                m_dependenceData.gammaOutput[i] = static_cast<FLOAT>(pGammaOutput[i]);
            }

            if ((m_dependenceData.imageWidth != pInputData->pipelineIPEData.inputDimension.widthPixels) ||
                (m_dependenceData.imageHeight != pInputData->pipelineIPEData.inputDimension.heightLines))
            {
                m_dependenceData.imageWidth  = pInputData->pipelineIPEData.inputDimension.widthPixels;
                m_dependenceData.imageHeight = pInputData->pipelineIPEData.inputDimension.heightLines;
                isChanged = TRUE;
            }

            CAMX_ASSERT(NULL != m_ptmcChromatix);
            m_pTMCInput.adrcLTMEnable = FALSE;

            if ((NULL  != m_ptmcChromatix)                                  &&
                (TRUE  == m_ptmcChromatix->enable_section.adrc_isp_enable)  &&
                (TRUE  == m_ptmcChromatix->chromatix_tmc10_reserve.use_ltm) &&
                (FALSE == pInputData->triggerData.isTMC11Enabled))
            {
                isChanged                 = TRUE;
                m_pTMCInput.adrcLTMEnable = TRUE;
                IQInterface::s_interpolationTable.TMC10TriggerUpdate(&pInputData->triggerData, &m_pTMCInput);

                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ADRC_ENABLED DRC gain = %f, DRCGainDark %f",
                    pInputData->triggerData.DRCGain, pInputData->triggerData.DRCGainDark);

                // Assign memory for parsing ADRC Specific data.
                if (NULL == m_pADRCData)
                {
                    m_pADRCData = CAMX_NEW ADRCData;
                    if (NULL != m_pADRCData)
                    {
                        Utils::Memset(m_pADRCData, 0, sizeof(ADRCData));
                    }
                    else
                    {
                        isChanged                 = FALSE;
                        CAMX_LOG_ERROR(CamxLogGroupISP, "Memory allocation failed for ADRC Specific data");
                    }
                }
                pInputData->triggerData.pADRCData = m_pADRCData;
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "IPE LTM : using TMC 1.0");
            }
            else
            {
                if (NULL != pInputData->triggerData.pADRCData)
                {
                    m_pTMCInput.adrcLTMEnable = pInputData->triggerData.pADRCData->ltmEnable;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "IPE LTM : using TMC 1.1");
                }
            }

            m_pTMCInput.pAdrcOutputData = pInputData->triggerData.pADRCData;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null Input Pointer");
    }

    CAMX_LOG_INFO(CamxLogGroupPProc, "isChanged = %d", isChanged);

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::FetchDMIBuffer()
{
    CamxResult      result          = CamxResultSuccess;
    PacketResource* pPacketResource = NULL;

    if (NULL != m_pLUTCmdBufferManager)
    {
        // Recycle the last updated LUT DMI cmd buffer
        if (NULL != m_pLUTCmdBuffer)
        {
            m_pLUTCmdBufferManager->Recycle(m_pLUTCmdBuffer);
        }
        // fetch a fresh LUT DMI cmd buffer
        result = m_pLUTCmdBufferManager->GetBuffer(&pPacketResource);
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupISP, "LUT Command Buffer manager is NULL");
    }

    if (CamxResultSuccess == result)
    {
        m_pLUTCmdBuffer = static_cast<CmdBuffer*>(pPacketResource);
    }
    else
    {
        m_pLUTCmdBuffer = NULL;
        result          = CamxResultEUnableToLoad;
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to fetch DMI Buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        result = FetchDMIBuffer();

        if (CamxResultSuccess == result)
        {
            LTM13OutputData outputData;

            outputData.pDMIDataPtr = reinterpret_cast<UINT32*>(m_pLUTCmdBuffer->BeginCommands(IPELTM13LUTBufferSizeInDwords));
            CAMX_ASSERT(NULL != outputData.pDMIDataPtr);

            // BET ONLY - InputData is different per module tested
            pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pDMIDataPtr);

            outputData.pRegCmd       = &m_regCmd;
            outputData.pModuleConfig = &m_moduleConfig;

            result =
                IQInterface::IPELTM13CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData, &m_pTMCInput);

            if (CamxResultSuccess == result)
            {
                result = m_pLUTCmdBuffer->CommitCommands();

                if (NULL != pInputData->pIPETuningMetadata)
                {
                    m_pLTMLUTs  = outputData.pDMIDataPtr;
                }
            }

            if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
            {
                DumpRegConfig(outputData.pDMIDataPtr);
            }

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "LTM Calculation Failed.");

                m_pLUTCmdBufferManager->Recycle(m_pLUTCmdBuffer);
                m_pLUTCmdBuffer = NULL;
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Cannot get buffer from CmdBufferManager");
        }
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPELTM13::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
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
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPELTM13::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::IPELTM13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPELTM13::IPELTM13(
    const CHAR*          pNodeIdentifier,
    IPEModuleCreateData* pCreateData)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type              = ISPIQModuleType::IPELTM;
    m_moduleEnable      = TRUE;
    m_cmdLength         = 0;
    m_numLUT            = LTMIndexMax;
    m_pLUTCmdBuffer     = NULL;

    m_pLTMLUTs          = NULL;
    m_pChromatix        = NULL;
    m_ptmcChromatix     = NULL;
    m_pADRCData         = NULL;

    if (TRUE == pCreateData->initializationData.registerBETEn)
    {
        m_ignoreChromatixRGammaFlag = FALSE;
        m_useHardcodedGamma         = FALSE;
    }
    else
    {
        Titan17xContext*               pContext        = NULL;
        const Titan17xSettingsManager* pSettingManager = NULL;

        pContext                    = static_cast<Titan17xContext*>(pCreateData->initializationData.pHwContext);
        pSettingManager             = pContext->GetTitan17xSettingsManager();
        m_ignoreChromatixRGammaFlag = pSettingManager->GetTitan17xStaticSettings()->ignoreChromatixRGammaFlag;
        m_useHardcodedGamma         = pSettingManager->GetTitan17xStaticSettings()->useHardcodedGamma;
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                     "IPE Local Tone Map m_numLUT %d, m_ignoreChromatixRGammaFlag %d, m_useHardcodedGamma %d",
                     m_numLUT,
                     m_ignoreChromatixRGammaFlag,
                     m_useHardcodedGamma);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPELTM13::~IPELTM13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPELTM13::~IPELTM13()
{
    CAMX_FREE(m_pADRCData);

    if (NULL != m_pLUTCmdBufferManager)
    {
        if (NULL != m_pLUTCmdBuffer)
        {
            m_pLUTCmdBufferManager->Recycle(m_pLUTCmdBuffer);
            m_pLUTCmdBuffer = NULL;
        }

        m_pLUTCmdBufferManager->Uninitialize();
        CAMX_DELETE m_pLUTCmdBufferManager;
        m_pLUTCmdBufferManager = NULL;
    }

    m_pChromatix    = NULL;
    m_ptmcChromatix = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPELTM13::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPELTM13::DumpRegConfig(
    UINT32* pDMIDataPtr
    ) const
{
    CHAR   dumpFilename[256];
    FILE*  pFile = NULL;
    UINT32 LTMWeightCurve[LTM_WEIGHT_LUT_SIZE]   = {};
    UINT32 tempLTMWeightCurveTable[12];
    UINT32 offset               = 0;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE LTM13 ********\n");

        if (TRUE == m_moduleEnable)
        {
            CamX::OsUtils::FPrintF(pFile, "* ltm13Data.wt[2][24] = \n");
            for (UINT index = 0; index < LTM_WEIGHT_LUT_SIZE; index++)
            {
                LTMWeightCurve[index] = static_cast<UINT32>(*(pDMIDataPtr + index));
            }
            for (UINT index = 0; index < LTM_WEIGHT_LUT_SIZE/2; index++)
            {
                if (index % 2 == 0)
                {
                    tempLTMWeightCurveTable[index*2+3] = (LTMWeightCurve[index] & 0xff00) >> 8;
                    tempLTMWeightCurveTable[index*2+1] = (LTMWeightCurve[index] & 0x00ff) >> 0;
                    CAMX_LOG_VERBOSE(CamxLogGroupApp, "tempLTMWeightCurveTable[%d] = %d, LTMWeightCurve[%d] = %d",
                        index*2+3, tempLTMWeightCurveTable[index*2+3], index, LTMWeightCurve[index]);
                    CAMX_LOG_VERBOSE(CamxLogGroupApp, "tempLTMWeightCurveTable[%d] = %d, LTMWeightCurve[%d] = %d",
                        index*2+1, tempLTMWeightCurveTable[index*2+1], index, LTMWeightCurve[index]);
                }
                else
                {
                    tempLTMWeightCurveTable[index*2+0] = (LTMWeightCurve[index] & 0xff00) >> 8;
                    tempLTMWeightCurveTable[index*2-2] = (LTMWeightCurve[index] & 0x00ff) >> 0;
                    CAMX_LOG_VERBOSE(CamxLogGroupApp, "tempLTMWeightCurveTable[%d] = %d, LTMWeightCurve[%d] = %d",
                        index*2+0, tempLTMWeightCurveTable[index*2+0], index, LTMWeightCurve[index]);
                    CAMX_LOG_VERBOSE(CamxLogGroupApp, "tempLTMWeightCurveTable[%d] = %d, LTMWeightCurve[%d] = %d",
                        index*2-2, tempLTMWeightCurveTable[index*2-2], index, LTMWeightCurve[index]);
                }
            }
            for (UINT index = 0; index < LTM_WEIGHT_LUT_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", tempLTMWeightCurveTable[index]);
            }

            CamX::OsUtils::FPrintF(pFile, "\n* ltm13Data.la_curve[2][64] = \n");
            offset += IPELTMLUTNumEntries[LTMIndexWeight];
            for (UINT index = 0; index < LTM_CURVE_LUT_SIZE - 1; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIDataPtr + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* ltm13Data.ltm_curve[2][64] = \n");
            offset += IPELTMLUTNumEntries[LTMIndexLA0] + IPELTMLUTNumEntries[LTMIndexLA1];
            for (UINT index = 0; index < LTM_CURVE_LUT_SIZE - 1; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIDataPtr + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* ltm13Data.ltm_scale[2][64] = \n");
            offset += IPELTMLUTNumEntries[LTMIndexCurve];
            for (UINT index = 0; index < LTM_SCALE_LUT_SIZE - 1; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIDataPtr + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* ltm13Data.mask_rect_curve[2][64] = \n");
            offset += IPELTMLUTNumEntries[LTMIndexScale];
            for (UINT index = 0; index < LTM_SCALE_LUT_SIZE - 1; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIDataPtr + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* ltm13Data.lce_scale_pos[2][16] = \n");
            offset += IPELTMLUTNumEntries[LTMIndexMask];
            for (UINT index = 0; index < LCE_SCALE_LUT_SIZE - 1; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIDataPtr + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "* ltm13Data.lce_scale_neg[2][16] = \n");
            offset += IPELTMLUTNumEntries[LTMIndexLCEPositive];
            for (UINT index = 0; index < LCE_SCALE_LUT_SIZE - 1; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIDataPtr + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* ltm13Data.igamma64[2][64] = \n");
            offset += IPELTMLUTNumEntries[LTMIndexLCENegative];
            for (UINT index = 0; index < LTM_GAMMA_LUT_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIDataPtr + offset + index)));
            }
        }
        else
        {
            CamX::OsUtils::FPrintF(pFile, "NOT ENABLED");
        }

        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
}

CAMX_NAMESPACE_END
