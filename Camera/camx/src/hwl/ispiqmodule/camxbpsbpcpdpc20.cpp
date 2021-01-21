////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsbpcpdpc20.cpp
/// @brief bpsbpcpdpc20 class implementation
///        BPC: corrects bad pixels and clusters
///        PDPC:corrects phase-differential auto focus pixels
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "pdpc_2_0_0.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 PDPCLUT = BPS_BPS_0_CLC_BPC_PDPC_DMI_LUT_CFG_LUT_SEL_PDAF_LUT;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSBPCPDPC20::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSBPCPDPC20::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSBPCPDPC20* pModule = CAMX_NEW BPSBPCPDPC20(pCreateData->pNodeIdentifier);

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
// BPSBPCPDPC20::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSBPCPDPC20::Initialize()
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
// BPSBPCPDPC20::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSBPCPDPC20::FillCmdBufferManagerParams(
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
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "BPSBPCPDPC20");
                pResourceParams->resourceSize                 = (BPSPDPC20DMILengthDword * sizeof(UINT32));
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
// BPSBPCPDPC20::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSBPCPDPC20::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(pdpc_2_0_0::pdpc20_rgn_dataType) * (PDPC20MaxNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for pdpc_2_0_0::pdpc20_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSBPCPDPC20::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSBPCPDPC20::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)             &&
        (NULL != pInputData->pHwContext) &&
        (NULL != pInputData->pHALTagsData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->PDPCEnable;
            isChanged      = (TRUE == m_moduleEnable);
            m_hotPixelMode = HotPixelModeFast;
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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_pdpc20_bps(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if (m_pChromatix != m_dependenceData.pChromatix)
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = (m_pChromatix->enable_section.pdpc_enable &&
                                                  (pInputData->sensorData.sensorPDAFInfo.PDAFPixelCount > 0));
                    m_dsBPCEnable               = m_pChromatix->enable_section.dsbpc_enable;

                    isChanged                   = ((TRUE == m_moduleEnable) || (TRUE == m_dsBPCEnable));
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }

            m_hotPixelMode = pInputData->pHALTagsData->hotPixelMode;
        }

        if ((TRUE == m_moduleEnable) || (TRUE == m_dsBPCEnable))
        {
            m_dependenceData.blackLevelOffset = pInputData->pCalculatedData->BLSblackLevelOffset;

            // Check for trigger update status
            if (TRUE == IQInterface::s_interpolationTable.BPSPDPC20TriggerUpdate(&(pInputData->triggerData), &m_dependenceData))
            {
                if (NULL == pInputData->pOEMIQSetting)
                {
                    // Check for dsBPC module dynamic enable trigger hysterisis
                    m_dsBPCEnable = IQSettingUtils::GetDynamicEnableFlag(
                        m_dependenceData.pChromatix->dynamic_enable_triggers.dsbpc_enable.enable,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.dsbpc_enable.hyst_control_var,
                        m_dependenceData.pChromatix->dynamic_enable_triggers.dsbpc_enable.hyst_mode,
                        &(m_dependenceData.pChromatix->dynamic_enable_triggers.dsbpc_enable.hyst_trigger),
                        static_cast<VOID*>(&(pInputData->triggerData)),
                        &m_dependenceData.moduleEnable);

                    // Set status to isChanged to avoid calling RunCalculation if the module is disabled
                    isChanged = ((TRUE == m_dsBPCEnable) || (TRUE == m_moduleEnable));
                }
            }
        }

        // PDAF data is fixed, and expected to comes only during the stream start
        if (TRUE == isChanged)
        {
            /// @todo (CAMX-1207) Sensor need to send PDAF dimensions info.
            /// remove this hardcoding after that.
            CamxResult result = IQInterface::IFEGetSensorMode(&pInputData->sensorData.format, &m_dependenceData.sensorType);

            if (CamxResultSuccess == result)
            {
                m_dependenceData.imageWidth        = pInputData->sensorData.sensorOut.width;
                m_dependenceData.imageHeight       = pInputData->sensorData.sensorOut.height;
                m_dependenceData.PDAFBlockWidth    = pInputData->sensorData.sensorPDAFInfo.PDAFBlockWidth;
                m_dependenceData.PDAFBlockHeight   = pInputData->sensorData.sensorPDAFInfo.PDAFBlockHeight;
                m_dependenceData.PDAFGlobaloffsetX =
                    static_cast<UINT16>(pInputData->sensorData.sensorPDAFInfo.PDAFGlobaloffsetX);
                m_dependenceData.PDAFGlobaloffsetY =
                    static_cast<UINT16>(pInputData->sensorData.sensorPDAFInfo.PDAFGlobaloffsetY);
                m_dependenceData.PDAFPixelCount    =
                    static_cast<UINT16>(pInputData->sensorData.sensorPDAFInfo.PDAFPixelCount);
                m_pixelFormat                      = pInputData->sensorData.format;
                m_dependenceData.zzHDRFirstRBEXP   = static_cast<UINT16>(pInputData->sensorData.ZZHDRFirstExposurePattern);
                m_dependenceData.ZZHDRPattern      = static_cast<UINT16>(pInputData->sensorData.ZZHDRColorPattern);

                Utils::Memcpy(m_dependenceData.PDAFPixelCoords,
                              &pInputData->sensorData.sensorPDAFInfo.PDAFPixelCoords,
                              sizeof(PDPixelCoordinates) * m_dependenceData.PDAFPixelCount);
            }
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP,
                "Invalid Input: pHwContext %p pTuningData %p",
                pInputData->pHwContext,
                pInputData->pTuningData);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSBPCPDPC20::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSBPCPDPC20::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult result       = CamxResultSuccess;
    UINT32     lengthInByte = BPSPDPC20DMILengthDword * sizeof(UINT32);

    // CDM pack the DMI buffer and patch the Red and Blue LUT DMI buffer into CDM pack
    if ((NULL != pInputData) && (NULL != pInputData->pDMICmdBuffer))
    {
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_BPC_PDPC_DMI_CFG,
                                         PDPCLUT,
                                         m_pLUTDMICmdBuffer,
                                         0,
                                         lengthInByte);

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write DMI data");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSBPCPDPC20::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSBPCPDPC20::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (0 != m_moduleConfig.moduleConfig.bitfields.PDAF_PDPC_EN)
    {
        result = WriteLUTtoDMI(pInputData);
    }

    if ((CamxResultSuccess == result) && (NULL != pInputData->pCmdBuffer))
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_BPC_PDPC_PDPC_BLACK_LEVEL,
                                              (sizeof(BPSBPCPDPC20RegConfig0) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd.config0));

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regBPS_BPS_0_CLC_BPC_PDPC_BAD_PIXEL_DET_OFFSET_T2,
                                                  (sizeof(BPSBPCPDPC20RegConfig1) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.config1));

        }

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
// BPSBPCPDPC20::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSBPCPDPC20::FetchDMIBuffer()
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
        CAMX_LOG_ERROR(CamxLogGroupISP, "LUT Cmd Buffer is NULL");
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
// BPSBPCPDPC20::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSBPCPDPC20::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        result = FetchDMIBuffer();

        if (CamxResultSuccess == result)
        {
            BPSBPCPDPC20OutputData outputData;

            // Update the LUT DMI buffer with Red LUT data
            outputData.pDMIDataPtr = reinterpret_cast<UINT32*>(m_pLUTDMICmdBuffer->BeginCommands(BPSPDPC20DMILengthDword));
            // BET ONLY - InputData is different per module tested
            pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pDMIDataPtr);
            CAMX_ASSERT(NULL != outputData.pDMIDataPtr);

            outputData.pRegCmd       = &m_regCmd;
            outputData.pModuleConfig = &m_moduleConfig;

            result = IQInterface::BPSBPCPDPC20CalculateSetting(&m_dependenceData,
                                                               pInputData->pOEMIQSetting,
                                                               &outputData,
                                                               m_pixelFormat);
            if (CamxResultSuccess == result)
            {
                m_pLUTDMICmdBuffer->CommitCommands();
                if (NULL != pInputData->pBPSTuningMetadata)
                {
                    m_pPDPCMaskLUT = outputData.pDMIDataPtr;
                }
                if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
                {
                    DumpRegConfig(pInputData, outputData.pModuleConfig);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "BPCPDPC Calculation Failed. result %d", result);
            }
        }
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "BPCPDPC Calculation Failed. result %d", result);
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Input");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSBPCPDPC20::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSBPCPDPC20::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);

    pBPSIQSettings->pdpcParameters.moduleCfg.PDAF_PDPC_EN                  =
        m_moduleConfig.moduleConfig.bitfields.PDAF_PDPC_EN;
    pBPSIQSettings->pdpcParameters.moduleCfg.BPC_EN                        =
        m_moduleConfig.moduleConfig.bitfields.BPC_EN;
    // PDPC block should be enabled if Demux is enabled, its status is stroed into below bit in Demux module.
    // simply with "OR" bit for pdpc mdoule En, it can be considered
    pBPSIQSettings->pdpcParameters.moduleCfg.EN                            |=
        m_moduleConfig.moduleConfig.bitfields.EN;
    pBPSIQSettings->pdpcParameters.moduleCfg.CHANNEL_BALANCE_EN            =
        m_moduleConfig.moduleConfig.bitfields.CHANNEL_BALANCE_EN;

    if (HotPixelModeOff == m_hotPixelMode)
    {
        pBPSIQSettings->pdpcParameters.moduleCfg.HOT_PIXEL_CORRECTION_DISABLE  = TRUE;
    }
    else
    {
        pBPSIQSettings->pdpcParameters.moduleCfg.HOT_PIXEL_CORRECTION_DISABLE  =
            m_moduleConfig.moduleConfig.bitfields.HOT_PIXEL_CORRECTION_DISABLE;
    }
    pBPSIQSettings->pdpcParameters.moduleCfg.COLD_PIXEL_CORRECTION_DISABLE =
        m_moduleConfig.moduleConfig.bitfields.COLD_PIXEL_CORRECTION_DISABLE;
    pBPSIQSettings->pdpcParameters.moduleCfg.USING_CROSS_CHANNEL_EN        =
        m_moduleConfig.moduleConfig.bitfields.USING_CROSS_CHANNEL_EN;
    pBPSIQSettings->pdpcParameters.moduleCfg.REMOVE_ALONG_EDGE_EN          =
        m_moduleConfig.moduleConfig.bitfields.REMOVE_ALONG_EDGE_EN;
    pBPSIQSettings->pdpcParameters.moduleCfg.BAYER_PATTERN                 =
        m_moduleConfig.moduleConfig.bitfields.BAYER_PATTERN;
    pBPSIQSettings->pdpcParameters.moduleCfg.PDAF_HDR_SELECTION            =
        m_moduleConfig.moduleConfig.bitfields.PDAF_HDR_SELECTION;
    pBPSIQSettings->pdpcParameters.moduleCfg.PDAF_ZZHDR_FIRST_RB_EXP       =
        m_moduleConfig.moduleConfig.bitfields.PDAF_ZZHDR_FIRST_RB_EXP;
    pBPSIQSettings->pdpcParameters.pdafGlobalOffsetX                       =
        m_regCmd.config0.PDAFLocationOffsetConfig.bitfields.X_OFFSET;
    pBPSIQSettings->pdpcParameters.pdafGlobalOffsetY                       =
        m_regCmd.config0.PDAFLocationOffsetConfig.bitfields.Y_OFFSET;
    pBPSIQSettings->pdpcParameters.pdafTableOffsetX                        =
        m_regCmd.config1.PDAFTabOffsetConfig.bitfields.X_OFFSET;
    pBPSIQSettings->pdpcParameters.pdafTableOffsetY                        =
        m_regCmd.config1.PDAFTabOffsetConfig.bitfields.Y_OFFSET;
    pBPSIQSettings->pdpcParameters.pdafXEnd                                =
        m_regCmd.config0.PDAFLocationEndConfig.bitfields.X_END;
    pBPSIQSettings->pdpcParameters.pdafYEnd                                =
        m_regCmd.config0.PDAFLocationEndConfig.bitfields.Y_END;

    // Post metdata
    pInputData->pCalculatedData->hotPixelMode = m_hotPixelMode;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT((BPSPDPC20DMILengthDword * sizeof(UINT32)) <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.PDPCLUT));
        CAMX_STATIC_ASSERT(sizeof(BPSBPCPDPC20RegCmd) <= sizeof(pInputData->pBPSTuningMetadata->BPSBBPCPDPCData));

        if (NULL != m_pPDPCMaskLUT)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.PDPCLUT,
                          m_pPDPCMaskLUT,
                          (BPSPDPC20DMILengthDword * sizeof(UINT32)));
        }
        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSBBPCPDPCData,
                      &m_regCmd,
                      sizeof(BPSBPCPDPC20RegCmd));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSBPCPDPC20::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSBPCPDPC20::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            result = RunCalculation(pInputData);
        }

        if ((CamxResultSuccess == result) &&
            ((TRUE == m_moduleEnable)     || (TRUE == m_dsBPCEnable)))
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
// BPSBPCPDPC20::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSBPCPDPC20::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSBPCPDPC20::BPSBPCPDPC20
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSBPCPDPC20::BPSBPCPDPC20(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier     = pNodeIdentifier;
    m_type                = ISPIQModuleType::BPSBPCPDPC;
    m_moduleEnable        = FALSE;
    m_dsBPCEnable         = FALSE;
    m_numLUT              = PDPCMaxLUT;

    m_cmdLength    = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSBPCPDPC20RegConfig0) / RegisterWidthInBytes) +
                     PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSBPCPDPC20RegConfig1) / RegisterWidthInBytes);

    m_hotPixelMode = HotPixelModeFast;
    m_pPDPCMaskLUT = NULL;
    m_pChromatix   = NULL;

    m_dependenceData.moduleEnable = FALSE;   ///< First frame is always FALSE
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSBPCPDPC20::~BPSBPCPDPC20
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSBPCPDPC20::~BPSBPCPDPC20()
{
    DeallocateCommonLibraryData();

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

    m_pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSBPCPDPC20::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSBPCPDPC20::DumpRegConfig(
    const ISPInputData* pInputData,
    const BPSBPCPDPC20ModuleConfig* pModuleConfig
    ) const
{
    CHAR     dumpFilename[256];
    FILE*    pFile = NULL;
    UINT32*  pDMIData;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "********  BPS BPCPDPC20 ********  \n");
        CamX::OsUtils::FPrintF(pFile, "* BPCPDPC20 module CFG [HEX]\n");
        CamX::OsUtils::FPrintF(pFile, "Module Config                = %x\n", pModuleConfig->moduleConfig);
        CamX::OsUtils::FPrintF(pFile, "* BPCPDPC20 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "PDPC Black level                = %x\n", m_regCmd.config0.PDPCBlackLevel);
        CamX::OsUtils::FPrintF(pFile, "HDR Exposure ratio              = %x\n", m_regCmd.config0.HDRExposureRatio);
        CamX::OsUtils::FPrintF(pFile, "BPC pixel threshold             = %x\n", m_regCmd.config0.BPCPixelThreshold);
        CamX::OsUtils::FPrintF(pFile, "BPC/BCC detection offset        = %x\n", m_regCmd.config0.badPixelDetectionOffset);
        CamX::OsUtils::FPrintF(pFile, "PDAF R/G white balance gain     = %x\n", m_regCmd.config0.PDAFRGWhiteBalanceGain);
        CamX::OsUtils::FPrintF(pFile, "PDAF B/G white balance gain     = %x\n", m_regCmd.config0.PDAFBGWhiteBalanceGain);
        CamX::OsUtils::FPrintF(pFile, "PDAF G/R white balance gain     = %x\n", m_regCmd.config0.PDAFGRWhiteBalanceGain);
        CamX::OsUtils::FPrintF(pFile, "PDAF G/B white balance gain     = %x\n", m_regCmd.config0.PDAFGBWhiteBalanceGain);
        CamX::OsUtils::FPrintF(pFile, "PDAF location offset config     = %x\n", m_regCmd.config0.PDAFLocationOffsetConfig);
        CamX::OsUtils::FPrintF(pFile, "PDAF location end config        = %x\n", m_regCmd.config0.PDAFLocationEndConfig);
        CamX::OsUtils::FPrintF(pFile, "BPC/BCC  offset for exposure T2 = %x\n", m_regCmd.config1.badPixelDetectionOffsetT2);
        CamX::OsUtils::FPrintF(pFile, "PDPC saturation threshold       = %x\n", m_regCmd.config1.PDPCSaturationThreshold);
        CamX::OsUtils::FPrintF(pFile, "PDAF tab offset config          = %x\n", m_regCmd.config1.PDAFTabOffsetConfig);

        if (TRUE == m_moduleEnable)
        {
            pDMIData = static_cast<UINT32*>(pInputData->pBetDMIAddr);

            CamX::OsUtils::FPrintF(pFile, "* pdpc20Data.PDAF_PD_Mask[2][64] = \n");
            for (UINT32 index = 0; index < BPSPDPC20DMILengthDword; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIData + index)));
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
