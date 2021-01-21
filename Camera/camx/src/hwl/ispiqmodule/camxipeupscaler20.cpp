////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipeupscaler20.cpp
/// @brief CAMXIPEUpscaler class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxipeupscaler20.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "ipe_data.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IPEUpscalerLUTBufferSizeInDWord =
    IPEUpscaleLUTNumEntries[DMI_LUT_A] +
    IPEUpscaleLUTNumEntries[DMI_LUT_B] +
    IPEUpscaleLUTNumEntries[DMI_LUT_C] +
    IPEUpscaleLUTNumEntries[DMI_LUT_D];

static const UINT32 IPEUpscalerLUTBufferSize        =
    IPEUpscaleLUTSizes[DMI_LUT_A] +
    IPEUpscaleLUTSizes[DMI_LUT_B] +
    IPEUpscaleLUTSizes[DMI_LUT_C] +
    IPEUpscaleLUTSizes[DMI_LUT_D];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPEUpscaler20* pModule = CAMX_NEW IPEUpscaler20(pCreateData->pNodeIdentifier);

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
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null input pointer");
    }

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::Initialize()
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
// IPEUpscaler20::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::FillCmdBufferManagerParams(
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
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "IPEUpscaler20");
                pResourceParams->resourceSize                 = IPEUpscalerLUTBufferSize;
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
// IPEUpscaler20::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(upscale_2_0_0::upscale20_rgn_dataType) * (Upscale20MaxNoLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for upscale_2_0_0::upscale20_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult  result          = CamxResultSuccess;
    CmdBuffer*  pDMICmdBuffer   = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferDMIHeader];
    UINT32      offset          = 0;

    if ((NULL != m_pLUTCmdBuffer) && (NULL != pDMICmdBuffer))
    {
        // Record the offset within DMI Header buffer, it is the offset at which this module has written a CDM DMI header.
        m_offsetLUT = (pDMICmdBuffer->GetResourceUsedDwords() * sizeof(UINT32));

        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_UPSCALE_DMI_CFG,
                                         IPE_IPE_0_PPS_CLC_UPSCALE_DMI_LUT_CFG_LUT_SEL_LUT_A,
                                         m_pLUTCmdBuffer,
                                         offset,
                                         IPEUpscaleLUTSizes[DMI_LUT_A]);
        CAMX_ASSERT(CamxResultSuccess == result);

        offset += IPEUpscaleLUTSizes[DMI_LUT_A];
        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_UPSCALE_DMI_CFG,
                                         IPE_IPE_0_PPS_CLC_UPSCALE_DMI_LUT_CFG_LUT_SEL_LUT_B,
                                         m_pLUTCmdBuffer,
                                         offset,
                                         IPEUpscaleLUTSizes[DMI_LUT_B]);
        CAMX_ASSERT(CamxResultSuccess == result);

        offset += IPEUpscaleLUTSizes[DMI_LUT_B];
        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_UPSCALE_DMI_CFG,
                                         IPE_IPE_0_PPS_CLC_UPSCALE_DMI_LUT_CFG_LUT_SEL_LUT_C,
                                         m_pLUTCmdBuffer,
                                         offset,
                                         IPEUpscaleLUTSizes[DMI_LUT_C]);
        CAMX_ASSERT(CamxResultSuccess == result);

        offset += IPEUpscaleLUTSizes[DMI_LUT_C];
        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_UPSCALE_DMI_CFG,
                                         IPE_IPE_0_PPS_CLC_UPSCALE_DMI_LUT_CFG_LUT_SEL_LUT_D,
                                         m_pLUTCmdBuffer,
                                         offset,
                                         IPEUpscaleLUTSizes[DMI_LUT_D]);
        CAMX_ASSERT(CamxResultSuccess == result);
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("NULL pointer m_pLUTCmdBuffer = %p, pDMICmdBuffer = %p", m_pLUTCmdBuffer, pDMICmdBuffer);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEUpscaler20::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    IpeIQSettings*            pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);
    UpScalerParameters*       pUpscalerParam = &pIPEIQSettings->upscalerParameters;
    ChromaUpScalerParameters* pChromaUpParam = &pIPEIQSettings->chromaUpscalerParameters;

    pUpscalerParam->directionalUpscalingEnabled = m_regCmdUpscale.opMode.bitfields.DIR_EN;
    pUpscalerParam->sharpeningEnabled           = m_regCmdUpscale.opMode.bitfields.DE_EN;
    pUpscalerParam->lumaCfg                     = m_regCmdUpscale.opMode.bitfields.Y_CFG;
    pUpscalerParam->chromaCfg                   = m_regCmdUpscale.opMode.bitfields.UV_CFG;
    pUpscalerParam->bitwidth                    = m_regCmdUpscale.opMode.bitfields.BIT_WIDTH;
    pUpscalerParam->blendCfg                    = m_regCmdUpscale.opMode.bitfields.BLEND_CFG;
    pUpscalerParam->weightGain                  = m_regCmdUpscale.opMode.bitfields.WEIGHT_GAIN;
    pChromaUpParam->chromaUpCosited             = m_dependenceData.cosited;
    pChromaUpParam->chromaUpEven                = m_dependenceData.evenOdd;
    pUpscalerParam->sharpenLevel1               = m_regCmdUpscale.detailEnhancerSharpen.bitfields.SHARPEN_LEVEL1;
    pUpscalerParam->sharpenLevel2               = m_regCmdUpscale.detailEnhancerSharpen.bitfields.SHARPEN_LEVEL2;
    pUpscalerParam->sharpenPrecision            = m_regCmdUpscale.detailEnhancerSharpenControl.bitfields.DE_PREC;
    pUpscalerParam->sharpenClip                 = m_regCmdUpscale.detailEnhancerSharpenControl.bitfields.DE_CLIP;
    pUpscalerParam->shapeThreshQuiet            = m_regCmdUpscale.detailEnhancerShapeControl.bitfields.THR_QUIET;
    pUpscalerParam->shapeThreshDieout           = m_regCmdUpscale.detailEnhancerShapeControl.bitfields.THR_DIEOUT;
    pUpscalerParam->threshLow                   = m_regCmdUpscale.detailEnhancerThreshold.bitfields.THR_LOW;
    pUpscalerParam->threshHigh                  = m_regCmdUpscale.detailEnhancerThreshold.bitfields.THR_HIGH;
    pUpscalerParam->adjustDataA0                = m_regCmdUpscale.detailEnhancerAdjustData0.bitfields.ADJUST_A0;
    pUpscalerParam->adjustDataA1                = m_regCmdUpscale.detailEnhancerAdjustData0.bitfields.ADJUST_A1;
    pUpscalerParam->adjustDataA2                = m_regCmdUpscale.detailEnhancerAdjustData0.bitfields.ADJUST_A2;
    pUpscalerParam->adjustDataB0                = m_regCmdUpscale.detailEnhancerAdjustData1.bitfields.ADJUST_B0;
    pUpscalerParam->adjustDataB1                = m_regCmdUpscale.detailEnhancerAdjustData1.bitfields.ADJUST_B1;
    pUpscalerParam->adjustDataB2                = m_regCmdUpscale.detailEnhancerAdjustData1.bitfields.ADJUST_B2;
    pUpscalerParam->adjustDataC0                = m_regCmdUpscale.detailEnhancerAdjustData2.bitfields.ADJUST_C0;
    pUpscalerParam->adjustDataC1                = m_regCmdUpscale.detailEnhancerAdjustData2.bitfields.ADJUST_C1;
    pUpscalerParam->adjustDataC2                = m_regCmdUpscale.detailEnhancerAdjustData2.bitfields.ADJUST_C2;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pIPETuningMetadata)
    {
        if ((IPEUpscalerLUTBufferSizeInDWord * sizeof(UINT32)) <=
            sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.upscalerLUT))
        {
            if (NULL != m_pUpscalerLUTs)
            {
                Utils::Memcpy(&pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.upscalerLUT,
                              m_pUpscalerLUTs,
                              (IPEUpscalerLUTBufferSizeInDWord * sizeof(UINT32)));
            }
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Tuning data, incorrect LUT buffer size");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    CAMX_UNREFERENCED_PARAM(pInputData);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::ValidateDependenceParams(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-730) validate dependency parameters
    if ((NULL == pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferDMIHeader]) ||
        (NULL == pInputData->pipelineIPEData.pIPEIQSettings))
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input data or command buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPEUpscaler20::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                  &&
        (NULL != pInputData->pAECUpdateData)  &&
        (NULL != pInputData->pAWBUpdateData)  &&
        (NULL != pInputData->pHwContext))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", pInputData->pAECUpdateData->luxIndex);

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->US20Enable;

            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            if ((NULL != pInputData->pTuningData))
            {
                TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
                CAMX_ASSERT(NULL != pTuningManager);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if ((TRUE == pInputData->tuningModeChanged)    &&
                    (TRUE == pTuningManager->IsValidChromatix()))
                {
                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_upscale20_ipe(
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
                        // Firmware has the control. As of now, we need to set it to TRUE in order to program DMI LUT
                        m_moduleEnable              = TRUE;
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
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid Tuning Pointer");
            }
        }

        if (TRUE == m_moduleEnable)
        {
            AECFrameControl* pNewAECUpdate = pInputData->pAECUpdateData;

            // This is hardcoded for now based on discussion with FW team
            m_dependenceData.cosited = 0;
            m_dependenceData.evenOdd = 0;

            if ((FALSE == Utils::FEqual(m_dependenceData.luxIndex, pNewAECUpdate->luxIndex)) ||
                (FALSE == Utils::FEqual(m_dependenceData.AECGain,
                              pNewAECUpdate->exposureInfo[ExposureIndexSafe].linearGain)))
            {
                m_dependenceData.luxIndex = pNewAECUpdate->luxIndex;
                m_dependenceData.AECGain  = pNewAECUpdate->exposureInfo[ExposureIndexSafe].linearGain;
                isChanged                 = TRUE;
            }
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "UpScalar20 Module is not enabled");
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
// IPEUpscaler20::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", pInputData->pAECUpdateData->luxIndex);

    result = FetchDMIBuffer();
    if (CamxResultSuccess == result)
    {
        Upscale20OutputData outputData;
        UINT32* pLUT = reinterpret_cast<UINT32*>(m_pLUTCmdBuffer->BeginCommands(IPEUpscalerLUTBufferSizeInDWord));
        CAMX_ASSERT(NULL != pLUT);

        outputData.pRegCmdUpscale = &m_regCmdUpscale;
        outputData.pDMIPtr        = pLUT;

        result = IQInterface::IPEUpscale20CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);
        if (CamxResultSuccess == result)
        {
            result = m_pLUTCmdBuffer->CommitCommands();

            if (NULL != pInputData->pIPETuningMetadata)
            {
                m_pUpscalerLUTs = pLUT;
            }
        }

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE Upscale20 / ChromaUp20 Calculation Failed.");
            m_pLUTCmdBufferManager->Recycle(m_pLUTCmdBuffer);
            m_pLUTCmdBuffer = NULL;
        }

        if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
        {
            DumpRegConfig();
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Cannot get buffer from CmdBufferManager");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::FetchDMIBuffer()
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
        CAMX_LOG_ERROR(CamxLogGroupISP, "LUT Command Buffer Is NULL");
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
// IPEUpscaler20::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler20::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

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
                UpdateIPEInternalData(pInputData);
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
// IPEUpscaler20::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEUpscaler20::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::IPEUpscaler20
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEUpscaler20::IPEUpscaler20(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type              = ISPIQModuleType::IPEUpscaler;
    m_moduleEnable      = TRUE;
    m_cmdLength         = 0;
    m_numLUT            = MaxLUTNum;
    m_pLUTCmdBuffer     = NULL;
    m_pChromatix        = NULL;
    m_pUpscalerLUTs     = NULL;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE Upscaler m_numLUT %d ", m_numLUT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPEUpscaler20::~IPEUpscaler20
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEUpscaler20::~IPEUpscaler20()
{
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

    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler20::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEUpscaler20::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE Upscaler20 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* Upscaler20 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.chromaUpCosited             = %x\n",
            m_dependenceData.cosited);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.chromaUpEven                = %x\n",
            m_dependenceData.evenOdd);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.directionalUpscalingEnabled = %x\n",
            m_regCmdUpscale.opMode.bitfields.DIR_EN);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.sharpeningEnabled           = %x\n",
            m_regCmdUpscale.opMode.bitfields.DE_EN);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.lumaConfig                  = %x\n",
            m_regCmdUpscale.opMode.bitfields.Y_CFG);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.chromaConfig                = %x\n",
            m_regCmdUpscale.opMode.bitfields.UV_CFG);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.bitWidth                    = %x\n",
            m_regCmdUpscale.opMode.bitfields.BIT_WIDTH);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.blendConfig                 = %x\n",
            m_regCmdUpscale.opMode.bitfields.BLEND_CFG);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.weightGain                  = %x\n",
            m_regCmdUpscale.opMode.bitfields.WEIGHT_GAIN);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.sharpenLevel1               = %x\n",
            m_regCmdUpscale.detailEnhancerSharpen.bitfields.SHARPEN_LEVEL1);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.sharpenLevel2               = %x\n",
            m_regCmdUpscale.detailEnhancerSharpen.bitfields.SHARPEN_LEVEL2);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.sharpenPrecision            = %x\n",
            m_regCmdUpscale.detailEnhancerSharpenControl.bitfields.DE_PREC);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.sharpenClip                 = %x\n",
            m_regCmdUpscale.detailEnhancerSharpenControl.bitfields.DE_CLIP);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.shapeThreshQuiet            = %x\n",
            m_regCmdUpscale.detailEnhancerShapeControl.bitfields.THR_QUIET);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.shapeThreshDieout           = %x\n",
            m_regCmdUpscale.detailEnhancerShapeControl.bitfields.THR_DIEOUT);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.threshLow                   = %x\n",
            m_regCmdUpscale.detailEnhancerThreshold.bitfields.THR_LOW);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.threshHigh                  = %x\n",
            m_regCmdUpscale.detailEnhancerThreshold.bitfields.THR_HIGH);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.adjustDataA0                = %x\n",
            m_regCmdUpscale.detailEnhancerAdjustData0.bitfields.ADJUST_A0);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.adjustDataA1                = %x\n",
            m_regCmdUpscale.detailEnhancerAdjustData0.bitfields.ADJUST_A1);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.adjustDataA2                = %x\n",
            m_regCmdUpscale.detailEnhancerAdjustData0.bitfields.ADJUST_A2);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.adjustDataB0                = %x\n",
            m_regCmdUpscale.detailEnhancerAdjustData1.bitfields.ADJUST_B0);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.adjustDataB1                = %x\n",
            m_regCmdUpscale.detailEnhancerAdjustData1.bitfields.ADJUST_B1);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.adjustDataB2                = %x\n",
            m_regCmdUpscale.detailEnhancerAdjustData1.bitfields.ADJUST_B2);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.adjustDataC0                = %x\n",
            m_regCmdUpscale.detailEnhancerAdjustData2.bitfields.ADJUST_C0);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.adjustDataC1                = %x\n",
            m_regCmdUpscale.detailEnhancerAdjustData2.bitfields.ADJUST_C1);
        CamX::OsUtils::FPrintF(pFile, "Upscaler20.adjustDataC2                = %x\n",
            m_regCmdUpscale.detailEnhancerAdjustData2.bitfields.ADJUST_C2);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }

}

CAMX_NAMESPACE_END
