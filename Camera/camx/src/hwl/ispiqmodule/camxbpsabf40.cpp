////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsabf40.cpp
/// @brief BPSABF40 class implementation
///        ABF: Individual or cross channel Bayer domain denoise filter.  Also provide radial noise reduction (RNR) capability.
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpsabf40.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSABF40::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSABF40::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSABF40* pModule = CAMX_NEW BPSABF40(pCreateData->pNodeIdentifier);

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
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSABF40::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSABF40::Initialize()
{
    CamxResult     result          = CamxResultSuccess;

    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initilizee common library data, no memory");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSABF40::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSABF40::FillCmdBufferManagerParams(
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
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "BPSABF40");
                pResourceParams->resourceSize                 = BPSABFTotalLUTBufferSize;
                pResourceParams->poolSize                     = (pInputData->requestQueueDepth * pResourceParams->resourceSize);
                pResourceParams->usageFlags.cmdBuffer         = 1;
                pResourceParams->cmdParams.type               = CmdType::CDMDMI;
                pResourceParams->alignment                    = CamxCommandBufferAlignmentInBytes;
                pResourceParams->cmdParams.enableAddrPatching = 0;
                pResourceParams->cmdParams.maxNumNestedAddrs  = 0;
                pResourceParams->memFlags                     = CSLMemFlagUMDAccess;
                pResourceParams->pDeviceIndices               = pInputData->pipelineBPSData.pDeviceIndex;
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
// BPSABF40::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSABF40::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(abf_4_0_0::abf40_rgn_dataType) * (ABF40MaxmiumNoLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for abf_4_0_0::abf40_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSABF40::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSABF40::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;
    CAMX_LOG_VERBOSE(CamxLogGroupISP, "#### pHALTagsData %p ", pInputData->pHALTagsData);

    if ((NULL != pInputData)              &&
        (NULL != pInputData->pHwContext)  &&
        (NULL != pInputData->pTuningData) &&
        (NULL != pInputData->pHALTagsData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable       = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->ABFEnable;
            isChanged            = (TRUE == m_moduleEnable);
            m_noiseReductionMode = NoiseReductionModeFast;
            m_minmaxEnable       = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->ABFEnableSection.minmax_en;
            m_dirsmthEnable      = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->ABFEnableSection.dirsmth_en;
        }
        else
        {
            m_noiseReductionMode = pInputData->pHALTagsData->noiseReductionMode;

            if (NULL != pInputData->pTuningData)
            {
                TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
                CAMX_ASSERT(NULL != pTuningManager);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if ((TRUE == pInputData->tuningModeChanged)   &&
                    (TRUE == pTuningManager->IsValidChromatix()))
                {
                    m_pChromatix    = pTuningManager->GetChromatix()->GetModule_abf40_bps(
                                          reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                          pInputData->pTuningData->noOfSelectionParameter);
                    m_pChromatixBLS = pTuningManager->GetChromatix()->GetModule_bls12_bps(
                                          reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                          pInputData->pTuningData->noOfSelectionParameter);
                }

                CAMX_ASSERT(NULL != m_pChromatix);
                CAMX_ASSERT(NULL != m_pChromatixBLS);

                if ((NULL != m_pChromatix)  &&
                    (NULL != m_pChromatixBLS))
                {
                    if ((m_pChromatix      != m_dependenceData.pChromatix)               ||
                        (m_pChromatixBLS   != m_dependenceData.BLSData.pChromatix)       ||
                        (m_bilateralEnable != m_pChromatix->enable_section.bilateral_en) ||
                        (m_minmaxEnable    != m_pChromatix->enable_section.minmax_en)    ||
                        (m_dirsmthEnable   != m_pChromatix->enable_section.dirsmth_en))
                    {
                        m_dependenceData.pChromatix         = m_pChromatix;
                        m_dependenceData.BLSData.pChromatix = m_pChromatixBLS;

                        m_bilateralEnable                   = m_pChromatix->enable_section.bilateral_en;
                        m_minmaxEnable                      = m_pChromatix->enable_section.minmax_en;
                        m_dirsmthEnable                     = m_pChromatix->enable_section.dirsmth_en;
                        m_moduleEnable                      = (m_bilateralEnable | m_minmaxEnable | m_dirsmthEnable);
                        isChanged                           = (TRUE == m_moduleEnable);
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Null TuningData Pointer");
            }
        }

        if (TRUE == m_moduleEnable)
        {
            CamxResult result = IQInterface::GetPixelFormat(&pInputData->sensorData.format,
                                                            &(m_dependenceData.BLSData.bayerPattern));
            CAMX_ASSERT(CamxResultSuccess == result);

            // Check for trigger update status
            if (TRUE == IQInterface::s_interpolationTable.BPSABF40TriggerUpdate(&(pInputData->triggerData), &m_dependenceData))
            {
                if (NULL == pInputData->pOEMIQSetting)
                {
                    // Check for module dynamic enable trigger hysterisis, only for bilateral filtering in abf40
                    m_bilateralEnable = IQSettingUtils::GetDynamicEnableFlag(
                        m_dependenceData.pChromatix->dynamic_enable_triggers.bilateral_en.enable,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.bilateral_en.hyst_control_var,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.bilateral_en.hyst_mode,
                        &(m_dependenceData.pChromatix->dynamic_enable_triggers.bilateral_en.hyst_trigger),
                        static_cast<VOID*>(&(pInputData->triggerData)),
                        &m_dependenceData.moduleEnable);

                    m_moduleEnable = (m_bilateralEnable | m_minmaxEnable | m_dirsmthEnable);

                    // Set the module status to avoid calling RunCalculation if it is disabled
                    isChanged = (TRUE == m_moduleEnable);
                }
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
// BPSABF40::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSABF40::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    // CDM pack the DMI buffer and patch the Red and Blue LUT DMI buffer into CDM pack
    if ((NULL != pInputData) && (NULL != pInputData->pDMICmdBuffer))
    {
        UINT32 LUTOffset = 0;
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_ABF_DMI_CFG,
                                         NoiseLUT0,
                                         m_pLUTDMICmdBuffer,
                                         LUTOffset,
                                         BPSABF40NoiseLUTSize);
        if (CamxResultSuccess == result)
        {
            LUTOffset += BPSABF40NoiseLUTSize;
            result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                             regBPS_BPS_0_CLC_ABF_DMI_CFG,
                                             NoiseLUT1,
                                             m_pLUTDMICmdBuffer,
                                             LUTOffset,
                                             BPSABF40NoiseLUTSize);
        }

        if (CamxResultSuccess == result)
        {
            LUTOffset += BPSABF40NoiseLUTSize;
            result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                             regBPS_BPS_0_CLC_ABF_DMI_CFG,
                                             ActivityLUT,
                                             m_pLUTDMICmdBuffer,
                                             LUTOffset,
                                             BPSABF40ActivityLUTSize);
        }

        if (CamxResultSuccess == result)
        {
            LUTOffset += BPSABF40ActivityLUTSize;
            result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                             regBPS_BPS_0_CLC_ABF_DMI_CFG,
                                             DarkLUT,
                                             m_pLUTDMICmdBuffer,
                                             LUTOffset,
                                             BPSABF40DarkLUTSize);
        }

        if (CamxResultSuccess == result)
        {
            ///< Switch LUT Bank select immediately after writing
            m_dependenceData.LUTBankSel = (m_dependenceData.LUTBankSel == ABFLUTBank0) ? ABFLUTBank1 : ABFLUTBank0;
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write DMI data");
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
// BPSABF40::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSABF40::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    result = WriteLUTtoDMI(pInputData);

    if ((CamxResultSuccess == result) && (NULL != pInputData->pCmdBuffer))
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_ABF_ABF_0_CFG,
                                              (sizeof(BPSABF40RegConfig) / RegisterWidthInBytes),
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
// BPSABF40::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSABF40::FetchDMIBuffer()
{
    CamxResult      result          = CamxResultSuccess;
    PacketResource* pPacketResource = NULL;

    if (NULL != m_pLUTCmdBufferManager)
    {
        // Recycle the last updated LUT DMI cmd buffer
        if (NULL != m_pLUTDMICmdBuffer)
        {
            m_pLUTCmdBufferManager->Recycle(m_pLUTDMICmdBuffer);
        }

        // fetch a fresh LUT DMI cmd buffer
        result = m_pLUTCmdBufferManager->GetBuffer(&pPacketResource);
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupISP, "LUT Command Buffer Failed");
    }

    if (CamxResultSuccess == result)
    {
        m_pLUTDMICmdBuffer = static_cast<CmdBuffer*>(pPacketResource);
    }
    else
    {
        m_pLUTDMICmdBuffer = NULL;
        result             = CamxResultEUnableToLoad;
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to fetch DMI Buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSABF40::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSABF40::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        result = FetchDMIBuffer();

        if (CamxResultSuccess == result)
        {
            ABF40OutputData outputData;

            outputData.pRegCmd       = &m_regCmd;
            outputData.pModuleConfig = &m_moduleConfig;
            outputData.pNoiseLUT     = NULL;
            outputData.pActivityLUT  = NULL;
            outputData.pDarkLUT      = NULL;

            // Update the LUT DMI buffer with Noise 0 LUT data
            outputData.pNoiseLUT = reinterpret_cast<UINT32*>(m_pLUTDMICmdBuffer->BeginCommands(BPSABFTotalDMILengthDword));
            CAMX_ASSERT(NULL != outputData.pNoiseLUT);
            // BET ONLY - InputData is different per module tested
            pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pNoiseLUT);

            // Update the LUT DMI buffer with Noise 0 LUT1 data
            outputData.pNoiseLUT1 = (outputData.pNoiseLUT) + BPSABF40NoiseLUTSizeDword;
            CAMX_ASSERT(NULL != outputData.pNoiseLUT1);

            // Update the LUT DMI buffer with Activity LUT data
            outputData.pActivityLUT = (outputData.pNoiseLUT1) + BPSABF40NoiseLUTSizeDword;

            // Update the LUT DMI buffer with Dark LUT data
            outputData.pDarkLUT = (outputData.pActivityLUT) + BPSABF40ActivityLUTSizeDword;

            result = IQInterface::BPSABF40CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

            if (CamxResultSuccess == result)
            {
                m_pLUTDMICmdBuffer->CommitCommands();

                if (NULL != pInputData->pBPSTuningMetadata)
                {
                    m_pABFNoiseLUT      = outputData.pNoiseLUT;
                    m_pABFNoiseLUT1     = outputData.pNoiseLUT1;
                    m_pABFActivityLUT   = outputData.pActivityLUT;
                    m_pABFDarkLUT       = outputData.pDarkLUT;
                }
                if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
                {
                    DumpRegConfig(&outputData);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "BPSABF40 Calculation Failed.");
            }
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
// BPSABF40::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSABF40::UpdateBPSInternalData(
    ISPInputData* pInputData)
{
    BpsIQSettings*  pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->abfParameters.moduleCfg.EN                     = m_moduleEnable;

    if (m_noiseReductionMode == NoiseReductionModeOff)
    {
        pBPSIQSettings->abfParameters.moduleCfg.FILTER_EN  = 0;
    }
    else
    {
        pBPSIQSettings->abfParameters.moduleCfg.FILTER_EN              = m_moduleConfig.moduleConfig.bitfields.FILTER_EN;
    }
    pBPSIQSettings->abfParameters.moduleCfg.ACT_ADJ_EN             = m_moduleConfig.moduleConfig.bitfields.ACT_ADJ_EN;
    pBPSIQSettings->abfParameters.moduleCfg.DARK_SMOOTH_EN         = m_moduleConfig.moduleConfig.bitfields.DARK_SMOOTH_EN;
    pBPSIQSettings->abfParameters.moduleCfg.DARK_DESAT_EN          = m_moduleConfig.moduleConfig.bitfields.DARK_DESAT_EN;
    pBPSIQSettings->abfParameters.moduleCfg.DIR_SMOOTH_EN          = m_moduleConfig.moduleConfig.bitfields.DIR_SMOOTH_EN;
    pBPSIQSettings->abfParameters.moduleCfg.MINMAX_EN              = m_moduleConfig.moduleConfig.bitfields.MINMAX_EN;
    pBPSIQSettings->abfParameters.moduleCfg.CROSS_PLANE_EN         = m_moduleConfig.moduleConfig.bitfields.CROSS_PLANE_EN;
    pBPSIQSettings->abfParameters.moduleCfg.BLS_EN                 = m_moduleConfig.moduleConfig.bitfields.BLS_EN;
    pBPSIQSettings->abfParameters.moduleCfg.PIX_MATCH_LEVEL_RB     = m_moduleConfig.moduleConfig.bitfields.PIX_MATCH_LEVEL_RB;
    pBPSIQSettings->abfParameters.moduleCfg.PIX_MATCH_LEVEL_G      = m_moduleConfig.moduleConfig.bitfields.PIX_MATCH_LEVEL_G;
    pBPSIQSettings->abfParameters.moduleCfg.BLOCK_MATCH_PATTERN_RB =
        m_moduleConfig.moduleConfig.bitfields.BLOCK_MATCH_PATTERN_RB;
    pInputData->pCalculatedData->BLSblackLevelOffset               = m_regCmd.config44.bitfields.BLS_OFFSET;
    pInputData->triggerData.blackLevelOffset                       = m_regCmd.config44.bitfields.BLS_OFFSET;
    pInputData->pCalculatedData->noiseReductionMode                = m_noiseReductionMode;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(BPSABFTotalLUTBufferSize <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.ABFLUT));
        CAMX_STATIC_ASSERT(sizeof(BPSABF40RegConfig) <= sizeof(pInputData->pBPSTuningMetadata->BPSABFData));

        if (NULL != m_pABFNoiseLUT)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.ABFLUT.noiseLUT,
                          m_pABFNoiseLUT,
                          BPSABF40NoiseLUTSize);
        }
        if (NULL != m_pABFNoiseLUT1)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.ABFLUT.noise1LUT,
                          m_pABFNoiseLUT1,
                          BPSABF40NoiseLUTSize);
        }
        if (NULL != m_pABFActivityLUT)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.ABFLUT.activityLUT,
                          m_pABFActivityLUT,
                          BPSABF40ActivityLUTSize);
        }
        if (NULL != m_pABFDarkLUT)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.ABFLUT.darkLUT,
                          m_pABFDarkLUT,
                          BPSABF40DarkLUTSize);
        }
        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSABFData,
                      &m_regCmd,
                      sizeof(BPSABF40RegConfig));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSABF40::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSABF40::Execute(
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
// BPSABF40::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSABF40::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSABF40::BPSABF40
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSABF40::BPSABF40(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier                = pNodeIdentifier;
    m_type                           = ISPIQModuleType::BPSABF;
    m_moduleEnable                   = FALSE;
    m_numLUT                         = MaxABFLUT;
    m_cmdLength                      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(
                                           sizeof(BPSABF40RegConfig) / RegisterWidthInBytes);
    m_dependenceData.LUTBankSel      = ABFLUTBank0;  ///< By default set to Bank 0
    m_dependenceData.moduleEnable    = FALSE;   ///< First frame is always FALSE

    m_pABFNoiseLUT                   = NULL;
    m_pABFActivityLUT                = NULL;
    m_pABFDarkLUT                    = NULL;
    m_pChromatix                     = NULL;
    m_pChromatixBLS                  = NULL;
    m_noiseReductionMode             = NoiseReductionModeFast;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSABF40::~BPSABF40
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSABF40::~BPSABF40()
{
    if (NULL != m_pLUTCmdBufferManager)
    {
        if (NULL != m_pLUTDMICmdBuffer)
        {
            m_pLUTCmdBufferManager->Recycle(m_pLUTDMICmdBuffer);
            m_pLUTDMICmdBuffer = NULL;
        }

        m_pLUTCmdBufferManager->Uninitialize();
        CAMX_DELETE m_pLUTCmdBufferManager;
        m_pLUTCmdBufferManager = NULL;
    }

    m_pChromatix    = NULL;
    m_pChromatixBLS = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSABF40::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSABF40::DumpRegConfig(
    const ABF40OutputData* pCmd
    ) const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;

    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "********  BPS ABF40 ********  \n");
        CamX::OsUtils::FPrintF(pFile, "* ABF Module CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "ABF Module CFG = %x\n", pCmd->pModuleConfig->moduleConfig);
        CamX::OsUtils::FPrintF(pFile, "* ABF CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "ABF Config 0  = %x\n", m_regCmd.config0);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 1  = %x\n", m_regCmd.config1);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 2  = %x\n", m_regCmd.config2);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 3  = %x\n", m_regCmd.config3);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 4  = %x\n", m_regCmd.config4);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 5  = %x\n", m_regCmd.config5);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 6  = %x\n", m_regCmd.config6);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 7  = %x\n", m_regCmd.config7);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 8  = %x\n", m_regCmd.config8);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 9  = %x\n", m_regCmd.config9);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 10 = %x\n", m_regCmd.config10);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 11 = %x\n", m_regCmd.config11);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 12 = %x\n", m_regCmd.config12);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 13 = %x\n", m_regCmd.config13);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 14 = %x\n", m_regCmd.config14);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 15 = %x\n", m_regCmd.config15);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 16 = %x\n", m_regCmd.config16);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 17 = %x\n", m_regCmd.config17);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 18 = %x\n", m_regCmd.config18);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 19 = %x\n", m_regCmd.config19);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 20 = %x\n", m_regCmd.config20);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 21 = %x\n", m_regCmd.config21);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 22 = %x\n", m_regCmd.config22);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 23 = %x\n", m_regCmd.config23);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 24 = %x\n", m_regCmd.config24);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 25 = %x\n", m_regCmd.config25);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 26 = %x\n", m_regCmd.config26);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 27 = %x\n", m_regCmd.config27);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 28 = %x\n", m_regCmd.config28);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 29 = %x\n", m_regCmd.config29);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 30 = %x\n", m_regCmd.config30);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 31 = %x\n", m_regCmd.config31);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 32 = %x\n", m_regCmd.config32);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 33 = %x\n", m_regCmd.config33);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 34 = %x\n", m_regCmd.config34);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 35 = %x\n", m_regCmd.config35);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 36 = %x\n", m_regCmd.config36);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 37 = %x\n", m_regCmd.config37);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 38 = %x\n", m_regCmd.config38);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 39 = %x\n", m_regCmd.config39);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 40 = %x\n", m_regCmd.config40);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 41 = %x\n", m_regCmd.config41);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 42 = %x\n", m_regCmd.config42);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 43 = %x\n", m_regCmd.config43);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 44 = %x\n", m_regCmd.config44);
        CamX::OsUtils::FPrintF(pFile, "ABF Config 45 = %x\n", m_regCmd.config45);

        if (0 != m_moduleEnable)
        {
            CamX::OsUtils::FPrintF(pFile, "* abf40Data.noise_std_lut[2][64] = \n");
            for (UINT32 index = 0; index < BPSABF40NoiseLUTSizeDword; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", pCmd->pNoiseLUT[index]);
            }
            CamX::OsUtils::FPrintF(pFile, "\n* abf40Data.act_fac_lut[2][32] = \n");
            for (UINT32 index = 0; index < BPSABF40ActivityLUTSizeDword; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", pCmd->pActivityLUT[index]);
            }
            CamX::OsUtils::FPrintF(pFile, "\n* abf40Data.dark_fac_lut[2][42] = \n");
            for (UINT32 index = 0; index < BPSABF40DarkLUTSizeDword; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", pCmd->pDarkLUT[index]);
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
