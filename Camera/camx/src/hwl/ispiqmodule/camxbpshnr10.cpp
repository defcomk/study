////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpshnr10.cpp
/// @brief bpshnr10 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxbpshnr10.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "hnr10setting.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 BPSLNRLutBufferSize        = HNR_V10_LNR_ARR_NUM * sizeof(UINT32);
static const UINT32 BPSFNRAndGAinLutBufferSize = HNR_V10_FNR_ARR_NUM * sizeof(UINT32);
static const UINT32 BPSFNRAcLutBufferSize      = HNR_V10_FNR_ARR_NUM * sizeof(UINT32);
static const UINT32 BPSSNRLutBufferSize        = HNR_V10_SNR_ARR_NUM * sizeof(UINT32);
static const UINT32 BPSBlendLNRLutBufferSize   = HNR_V10_BLEND_LNR_ARR_NUM * sizeof(UINT32);
static const UINT32 BPSBlendSNRLutBufferSize   = HNR_V10_BLEND_SNR_ARR_NUM * sizeof(UINT32);

static const UINT32 BPSHNRLutBufferSize        = BPSLNRLutBufferSize        +
                                                 BPSFNRAndGAinLutBufferSize +
                                                 BPSFNRAcLutBufferSize      +
                                                 BPSSNRLutBufferSize        +
                                                 BPSBlendLNRLutBufferSize   +
                                                 BPSBlendSNRLutBufferSize;

static const UINT32 BPSHNRLutBufferSizeDWORD   = BPSHNRLutBufferSize / sizeof(UINT32);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHNR10::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHNR10::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSHNR10* pModule = CAMX_NEW BPSHNR10(pCreateData->pNodeIdentifier);

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
// BPSHNR10::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHNR10::Initialize()
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
// BPSHNR10::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHNR10::FillCmdBufferManagerParams(
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
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "BPSHNR10");
                pResourceParams->resourceSize                 = BPSHNRLutBufferSize;
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
// BPSHNR10::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHNR10::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(hnr_1_0_0::hnr10_rgn_dataType) * (HNR10MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for hnr_1_0_0::hnr10_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHNR10::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSHNR10::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)           &&
        (NULL != pInputData->pHwContext))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->HNREnable;
            isChanged      = (TRUE == m_moduleEnable);

            /// @todo (CAMX-2012) Face detection parameters need to be consumed from FD.
            m_dependenceData.pFDData = &pInputData->fDData;
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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_hnr10_bps(
                        reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                        pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                /// @todo (CAMX-2109) hardcode prescale ratio to 1, need to calculate based on video or snapshot case selection
                // m_dependenceData.totalScaleRatio = 1.0f;

                /// @todo (CAMX-2012) Face detection parameters need to be consumed from FD.
                m_dependenceData.pFDData = &pInputData->fDData;

                if (m_pChromatix != m_dependenceData.pChromatix)
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.hnr_nr_enable;

                    isChanged                   = (TRUE == m_moduleEnable);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }
        }

        // Disable module if requested by HAL
        m_noiseReductionMode = pInputData->pHALTagsData->noiseReductionMode;
        if (m_noiseReductionMode == NoiseReductionModeOff)
        {
            m_moduleEnable  = FALSE;
            isChanged       = FALSE;
        }

        // Check for trigger update status
        if ((TRUE == m_moduleEnable) &&
            (TRUE == IQInterface::s_interpolationTable.HNR10TriggerUpdate(&pInputData->triggerData, &m_dependenceData)))
        {
            if (NULL == pInputData->pOEMIQSetting)
            {
                // Check for module dynamic enable trigger hysterisis
                m_moduleEnable = IQSettingUtils::GetDynamicEnableFlag(
                    m_dependenceData.pChromatix->dynamic_enable_triggers.hnr_nr_enable.enable,
                    m_dependenceData.pChromatix->dynamic_enable_triggers.hnr_nr_enable.hyst_control_var,
                    m_dependenceData.pChromatix->dynamic_enable_triggers.hnr_nr_enable.hyst_mode,
                    &(m_dependenceData.pChromatix->dynamic_enable_triggers.hnr_nr_enable.hyst_trigger),
                    static_cast<VOID*>(&pInputData->triggerData),
                    &m_dependenceData.moduleEnable);

                // Set status to isChanged to avoid calling RunCalculation if the module is disabled
                isChanged = (TRUE == m_moduleEnable);
            }
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP,
                           "Invalid Input: pHwContext %p",
                           pInputData->pHwContext);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHNR10::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHNR10::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    // CDM pack the DMI buffer and patch the LUT DMI buffer into CDM pack
    if ((NULL != pInputData) && (NULL != pInputData->pDMICmdBuffer))
    {
        UINT32 LUTOffset = 0;
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_HNR_DMI_CFG,
                                         LNRGainLUT,
                                         m_pLUTDMICmdBuffer,
                                         LUTOffset,
                                         BPSLNRLutBufferSize);
        CAMX_ASSERT(CamxResultSuccess == result);

        LUTOffset += BPSLNRLutBufferSize;
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_HNR_DMI_CFG,
                                         MergedFNRGainAndGainClampLUT,
                                         m_pLUTDMICmdBuffer,
                                         LUTOffset,
                                         BPSFNRAndGAinLutBufferSize);
        CAMX_ASSERT(CamxResultSuccess == result);

        LUTOffset += BPSFNRAndGAinLutBufferSize;
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_HNR_DMI_CFG,
                                         FNRAcLUT,
                                         m_pLUTDMICmdBuffer,
                                         LUTOffset,
                                         BPSFNRAcLutBufferSize);
        CAMX_ASSERT(CamxResultSuccess == result);

        LUTOffset += BPSFNRAcLutBufferSize;
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_HNR_DMI_CFG,
                                         SNRGainLUT,
                                         m_pLUTDMICmdBuffer,
                                         LUTOffset,
                                         BPSSNRLutBufferSize);
        CAMX_ASSERT(CamxResultSuccess == result);

        LUTOffset += BPSSNRLutBufferSize;
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_HNR_DMI_CFG,
                                         BlendLNRGainLUT,
                                         m_pLUTDMICmdBuffer,
                                         LUTOffset,
                                         BPSBlendLNRLutBufferSize);
        CAMX_ASSERT(CamxResultSuccess == result);

        LUTOffset += BPSBlendLNRLutBufferSize;
        result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                         regBPS_BPS_0_CLC_HNR_DMI_CFG,
                                         BlendSNRGainLUT,
                                         m_pLUTDMICmdBuffer,
                                         LUTOffset,
                                         BPSBlendSNRLutBufferSize);
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
// BPSHNR10::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHNR10::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    result = WriteLUTtoDMI(pInputData);

    if ((CamxResultSuccess == result) && (NULL != pInputData->pCmdBuffer))
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_0,
                                              (sizeof(BPSHNR10RegConfig) / RegisterWidthInBytes),
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
// BPSHNR10::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHNR10::FetchDMIBuffer()
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
        CAMX_LOG_ERROR(CamxLogGroupISP, "LUT command Buffer is NULL");
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
// BPSHNR10::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHNR10::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        result = FetchDMIBuffer();

        if (CamxResultSuccess == result)
        {
            HNR10OutputData outputData;

            outputData.pLNRDMIBuffer          =
                reinterpret_cast<UINT32*>(m_pLUTDMICmdBuffer->BeginCommands(BPSHNRLutBufferSizeDWORD));
            // BET ONLY - InputData is different per module tested
            pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pLNRDMIBuffer);
            CAMX_ASSERT(NULL != outputData.pLNRDMIBuffer);

            outputData.pRegCmd                = &m_regCmd;
            outputData.pHNRParameters         = &m_HNRParameters;

            outputData.pFNRAndClampDMIBuffer  =
                reinterpret_cast<UINT32*>(reinterpret_cast<UCHAR*>(outputData.pLNRDMIBuffer) +
                    BPSLNRLutBufferSize);
            CAMX_ASSERT(NULL != outputData.pFNRAndClampDMIBuffer);

            outputData.pFNRAcDMIBuffer        =
                reinterpret_cast<UINT32*>(reinterpret_cast<UCHAR*>(outputData.pFNRAndClampDMIBuffer) +
                    BPSFNRAndGAinLutBufferSize);
            CAMX_ASSERT(NULL != outputData.pFNRAcDMIBuffer);

            outputData.pSNRDMIBuffer          =
                reinterpret_cast<UINT32*>(reinterpret_cast<UCHAR*>(outputData.pFNRAcDMIBuffer) +
                    BPSFNRAcLutBufferSize);
            CAMX_ASSERT(NULL != outputData.pSNRDMIBuffer);

            outputData.pBlendLNRDMIBuffer     =
                reinterpret_cast<UINT32*>(reinterpret_cast<UCHAR*>(outputData.pSNRDMIBuffer) +
                    BPSSNRLutBufferSize);
            CAMX_ASSERT(NULL != outputData.pBlendLNRDMIBuffer);

            outputData.pBlendSNRDMIBuffer     =
                reinterpret_cast<UINT32*>(reinterpret_cast<UCHAR*>(outputData.pBlendLNRDMIBuffer) +
                    BPSBlendLNRLutBufferSize);
            CAMX_ASSERT(NULL != outputData.pBlendSNRDMIBuffer);

            m_dependenceData.LUTBankSel      ^= 1;

            result = IQInterface::BPSHNR10CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

            if (CamxResultSuccess == result)
            {
                m_pLUTDMICmdBuffer->CommitCommands();

                if (NULL != pInputData->pBPSTuningMetadata)
                {
                    m_pLNRDMIBuffer         = outputData.pLNRDMIBuffer;
                    m_pFNRAndClampDMIBuffer = outputData.pFNRAndClampDMIBuffer;
                    m_pFNRAcDMIBuffer       = outputData.pFNRAcDMIBuffer;
                    m_pSNRDMIBuffer         = outputData.pSNRDMIBuffer;
                    m_pBlendLNRDMIBuffer    = outputData.pBlendLNRDMIBuffer;
                    m_pBlendSNRDMIBuffer    = outputData.pBlendSNRDMIBuffer;
                }
                if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
                {
                    DumpRegConfig(pInputData, outputData.pHNRParameters);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "HNR10 Calculation Failed %d", result);
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
// BPSHNR10::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSHNR10::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->hnrParameters              = m_HNRParameters;
    pBPSIQSettings->hnrParameters.moduleCfg.EN = m_moduleEnable;
    pInputData->pCalculatedData->noiseReductionMode = m_noiseReductionMode;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(BPSHNR10RegConfig) <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSHNRData.HNRConfig));
        CAMX_STATIC_ASSERT(MAX_FACE_NUM <= TuningMaxFaceNumber);
        CAMX_STATIC_ASSERT(BPSHNRLutBufferSize <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.HNRLUT));

        if (NULL != m_pLNRDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.HNRLUT.LNRLUT,
                          m_pLNRDMIBuffer,
                          BPSLNRLutBufferSize);
        }
        if (NULL != m_pFNRAndClampDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.HNRLUT.FNRAndClampLUT,
                          m_pFNRAndClampDMIBuffer,
                          BPSFNRAndGAinLutBufferSize);
        }
        if (NULL != m_pFNRAcDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.HNRLUT.FNRAcLUT,
                          m_pFNRAcDMIBuffer,
                          BPSFNRAcLutBufferSize);
        }
        if (NULL != m_pSNRDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.HNRLUT.SNRLUT,
                          m_pSNRDMIBuffer,
                          BPSSNRLutBufferSize);
        }
        if (NULL != m_pBlendLNRDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.HNRLUT.blendLNRLUT,
                          m_pBlendLNRDMIBuffer,
                          BPSBlendLNRLutBufferSize);
        }
        if (NULL != m_pBlendSNRDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.HNRLUT.blendSNRLUT,
                          m_pBlendSNRDMIBuffer,
                          BPSBlendSNRLutBufferSize);
        }

        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSHNRData.HNRConfig, &m_regCmd, sizeof(BPSHNR10RegConfig));

        TuningFaceData* pHNRFaceDetection = &pInputData->pBPSTuningMetadata->BPSHNRFaceDetection;

        if (NULL != m_dependenceData.pFDData)
        {
            pHNRFaceDetection->numberOfFace = (m_dependenceData.pFDData->numberOfFace > HNR_V10_MAX_FACE_NUM) ?
                HNR_V10_MAX_FACE_NUM : m_dependenceData.pFDData->numberOfFace;
            for (UINT32 count = 0; count < m_dependenceData.pFDData->numberOfFace; count++)
            {
                pHNRFaceDetection->faceRadius[count]  = m_dependenceData.pFDData->faceRadius[count];
                pHNRFaceDetection->faceCenterX[count] = m_dependenceData.pFDData->faceCenterX[count];
                pHNRFaceDetection->faceCenterY[count] = m_dependenceData.pFDData->faceCenterY[count];
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHNR10::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSHNR10::Execute(
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
    CAMX_LOG_INFO(CamxLogGroupPProc, "HNR10 Calculation completed %d", result);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHNR10::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSHNR10::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHNR10::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSHNR10::DumpRegConfig(
    const ISPInputData* pInputData,
    const HnrParameters* pModuleConfig
    ) const
{
    CHAR     dumpFilename[256];
    FILE*    pFile = NULL;
    UINT32*  pDMIData;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** BPS HNR10 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* HNR10 module CFG \n");
        CamX::OsUtils::FPrintF(pFile, "EN                   = %x\n", pModuleConfig->moduleCfg.EN);
        CamX::OsUtils::FPrintF(pFile, "BLEND_SNR_EN         = %x\n", pModuleConfig->moduleCfg.BLEND_SNR_EN);
        CamX::OsUtils::FPrintF(pFile, "BLEND_CNR_EN         = %x\n", pModuleConfig->moduleCfg.BLEND_CNR_EN);
        CamX::OsUtils::FPrintF(pFile, "BLEND_ENABLE         = %x\n", pModuleConfig->moduleCfg.BLEND_ENABLE);
        CamX::OsUtils::FPrintF(pFile, "LPF3_EN              = %x\n", pModuleConfig->moduleCfg.LPF3_EN);
        CamX::OsUtils::FPrintF(pFile, "FNR_EN               = %x\n", pModuleConfig->moduleCfg.FNR_EN);
        CamX::OsUtils::FPrintF(pFile, "FD_SNR_EN            = %x\n", pModuleConfig->moduleCfg.FD_SNR_EN);
        CamX::OsUtils::FPrintF(pFile, "SNR_EN               = %x\n", pModuleConfig->moduleCfg.SNR_EN);
        CamX::OsUtils::FPrintF(pFile, "CNR_EN               = %x\n", pModuleConfig->moduleCfg.CNR_EN);
        CamX::OsUtils::FPrintF(pFile, "RNR_EN               = %x\n", pModuleConfig->moduleCfg.RNR_EN);
        CamX::OsUtils::FPrintF(pFile, "LNR_EN               = %x\n", pModuleConfig->moduleCfg.LNR_EN);
        CamX::OsUtils::FPrintF(pFile, "* HNR10 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "nrGainCoefficient0         = %x\n", m_regCmd.nrGainCoefficient0);
        CamX::OsUtils::FPrintF(pFile, "nrGainCoefficient1         = %x\n", m_regCmd.nrGainCoefficient1);
        CamX::OsUtils::FPrintF(pFile, "nrGainCoefficient2         = %x\n", m_regCmd.nrGainCoefficient2);
        CamX::OsUtils::FPrintF(pFile, "nrGainCoefficient3         = %x\n", m_regCmd.nrGainCoefficient3);
        CamX::OsUtils::FPrintF(pFile, "nrGainCoefficient4         = %x\n", m_regCmd.nrGainCoefficient4);
        CamX::OsUtils::FPrintF(pFile, "nrGainCoefficient5         = %x\n", m_regCmd.nrGainCoefficient5);
        CamX::OsUtils::FPrintF(pFile, "nrGainCoefficient6         = %x\n", m_regCmd.nrGainCoefficient6);
        CamX::OsUtils::FPrintF(pFile, "nrGainCoefficient7         = %x\n", m_regCmd.nrGainCoefficient7);
        CamX::OsUtils::FPrintF(pFile, "nrGainCoefficient8         = %x\n", m_regCmd.nrGainCoefficient8);

        CamX::OsUtils::FPrintF(pFile, "cnrCFGCoefficient0         = %x\n", m_regCmd.cnrCFGCoefficient0);
        CamX::OsUtils::FPrintF(pFile, "cnrCFGCoefficient1         = %x\n", m_regCmd.cnrCFGCoefficient1);

        CamX::OsUtils::FPrintF(pFile, "cnrGaninCoeffincient0      = %x\n", m_regCmd.cnrGaninCoeffincient0);
        CamX::OsUtils::FPrintF(pFile, "cnrGaninCoeffincient1      = %x\n", m_regCmd.cnrGaninCoeffincient1);
        CamX::OsUtils::FPrintF(pFile, "cnrGaninCoeffincient2      = %x\n", m_regCmd.cnrGaninCoeffincient2);
        CamX::OsUtils::FPrintF(pFile, "cnrGaninCoeffincient3      = %x\n", m_regCmd.cnrGaninCoeffincient3);
        CamX::OsUtils::FPrintF(pFile, "cnrGaninCoeffincient4      = %x\n", m_regCmd.cnrGaninCoeffincient4);

        CamX::OsUtils::FPrintF(pFile, "snrCoefficient0            = %x\n", m_regCmd.snrCoefficient0);
        CamX::OsUtils::FPrintF(pFile, "snrCoefficient1            = %x\n", m_regCmd.snrCoefficient1);
        CamX::OsUtils::FPrintF(pFile, "snrCoefficient2            = %x\n", m_regCmd.snrCoefficient2);

        CamX::OsUtils::FPrintF(pFile, "faceConfigCoefficient      = %x\n", m_regCmd.faceConfigCoefficient);
        CamX::OsUtils::FPrintF(pFile, "faceOffsetCoefficient      = %x\n", m_regCmd.faceOffsetCoefficient);

        CamX::OsUtils::FPrintF(pFile, "faceCoefficient0           = %x\n", m_regCmd.faceCoefficient0);
        CamX::OsUtils::FPrintF(pFile, "faceCoefficient1           = %x\n", m_regCmd.faceCoefficient1);
        CamX::OsUtils::FPrintF(pFile, "faceCoefficient2           = %x\n", m_regCmd.faceCoefficient2);
        CamX::OsUtils::FPrintF(pFile, "faceCoefficient3           = %x\n", m_regCmd.faceCoefficient3);
        CamX::OsUtils::FPrintF(pFile, "faceCoefficient4           = %x\n", m_regCmd.faceCoefficient4);

        CamX::OsUtils::FPrintF(pFile, "radiusCoefficient0         = %x\n", m_regCmd.radiusCoefficient0);
        CamX::OsUtils::FPrintF(pFile, "radiusCoefficient1         = %x\n", m_regCmd.radiusCoefficient1);
        CamX::OsUtils::FPrintF(pFile, "radiusCoefficient2         = %x\n", m_regCmd.radiusCoefficient2);
        CamX::OsUtils::FPrintF(pFile, "radiusCoefficient3         = %x\n", m_regCmd.radiusCoefficient3);
        CamX::OsUtils::FPrintF(pFile, "radiusCoefficient4         = %x\n", m_regCmd.radiusCoefficient4);

        CamX::OsUtils::FPrintF(pFile, "rnrAnchorCoefficient0      = %x\n", m_regCmd.rnrAnchorCoefficient0);
        CamX::OsUtils::FPrintF(pFile, "rnrAnchorCoefficient1      = %x\n", m_regCmd.rnrAnchorCoefficient1);
        CamX::OsUtils::FPrintF(pFile, "rnrAnchorCoefficient2      = %x\n", m_regCmd.rnrAnchorCoefficient2);
        CamX::OsUtils::FPrintF(pFile, "rnrAnchorCoefficient3      = %x\n", m_regCmd.rnrAnchorCoefficient3);
        CamX::OsUtils::FPrintF(pFile, "rnrAnchorCoefficient4      = %x\n", m_regCmd.rnrAnchorCoefficient4);
        CamX::OsUtils::FPrintF(pFile, "rnrAnchorCoefficient5      = %x\n", m_regCmd.rnrAnchorCoefficient5);

        CamX::OsUtils::FPrintF(pFile, "rnrSlopeShiftCoefficient0  = %x\n", m_regCmd.rnrSlopeShiftCoefficient0);
        CamX::OsUtils::FPrintF(pFile, "rnrSlopeShiftCoefficient1  = %x\n", m_regCmd.rnrSlopeShiftCoefficient1);
        CamX::OsUtils::FPrintF(pFile, "rnrSlopeShiftCoefficient2  = %x\n", m_regCmd.rnrSlopeShiftCoefficient2);
        CamX::OsUtils::FPrintF(pFile, "rnrSlopeShiftCoefficient3  = %x\n", m_regCmd.rnrSlopeShiftCoefficient3);
        CamX::OsUtils::FPrintF(pFile, "rnrSlopeShiftCoefficient4  = %x\n", m_regCmd.rnrSlopeShiftCoefficient4);
        CamX::OsUtils::FPrintF(pFile, "rnrSlopeShiftCoefficient5  = %x\n", m_regCmd.rnrSlopeShiftCoefficient5);

        CamX::OsUtils::FPrintF(pFile, "hnrCoefficient1            = %x\n", m_regCmd.hnrCoefficient1);

        CamX::OsUtils::FPrintF(pFile, "rnr_r_squareCoefficient    = %x\n", m_regCmd.rnr_r_squareCoefficient);
        CamX::OsUtils::FPrintF(pFile, "rnr_r_ScaleCoefficient     = %x\n", m_regCmd.rnr_r_ScaleCoefficient);
        CamX::OsUtils::FPrintF(pFile, "lpf3ConfigCoefficient      = %x\n", m_regCmd.lpf3ConfigCoefficient);
        CamX::OsUtils::FPrintF(pFile, "miscConfigCoefficient      = %x\n", m_regCmd.miscConfigCoefficient);

        if (TRUE == m_moduleEnable)
        {
            UINT32 offset               = 0;
            pDMIData = static_cast<UINT32*>(pInputData->pBetDMIAddr);

            CamX::OsUtils::FPrintF(pFile, "* hnr10Data.lnr_gain_arr[2][33]\n");
            for (UINT32 index = 0; index < HNR_V10_LNR_ARR_NUM; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIData + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* hnr10Data.merged_fnr_gain_arr_gain_clamp_arr[2][17] = \n");
            offset += HNR_V10_LNR_ARR_NUM;
            for (UINT32 index = 0; index < HNR_V10_FNR_ARR_NUM; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIData + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* hnr10Data.fnr_ac_th_arr[2][17] = \n");
            offset += HNR_V10_FNR_ARR_NUM;
            for (UINT32 index = 0; index < HNR_V10_FNR_ARR_NUM; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIData + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* hnr10Data.snr_gain_arr[2][17] = \n");
            offset += HNR_V10_FNR_ARR_NUM;
            for (UINT32 index = 0; index < HNR_V10_SNR_ARR_NUM; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIData + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* hnr10Data.blend_lnr_gain_arr[2][17] = \n");
            offset += HNR_V10_SNR_ARR_NUM;
            for (UINT32 index = 0; index < HNR_V10_BLEND_LNR_ARR_NUM; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIData + offset + index)));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* hnr10Data.blend_snr_gain_arr[2][17] = \n");
            offset += HNR_V10_BLEND_LNR_ARR_NUM;
            for (UINT32 index = 0; index < HNR_V10_BLEND_SNR_ARR_NUM; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", (*(pDMIData + offset + index)));
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHNR10::BPSHNR10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSHNR10::BPSHNR10(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type         = ISPIQModuleType::BPSHNR;
    m_moduleEnable = FALSE;
    m_numLUT       = HNRMaxLUT;
    m_pChromatix   = NULL;
    m_cmdLength    = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSHNR10RegConfig) / RegisterWidthInBytes);

    m_dependenceData.moduleEnable = FALSE; ///< First frame is always FALSE
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSHNR10::~BPSHNR10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSHNR10::~BPSHNR10()
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

CAMX_NAMESPACE_END
