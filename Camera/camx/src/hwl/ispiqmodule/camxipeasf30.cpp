////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipeasf30.cpp
/// @brief IPEASF30 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxcdmdefs.h"
#include "camxdefs.h"
#include "camxipeasf30.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "ipe_data.h"
#include "parametertuningtypes.h"
#include "asf30setting.h"

CAMX_NAMESPACE_BEGIN

/// @brief: Enumerator to map optimized Look Up Tables indices with DMI LUT_SEL in ASF module HPG/SWI
enum ASFDMILUTConfigLUTSelIndex
{
    NoLUTSelected                               = 0x0,    ///< No LUT
    Layer1ActivityNormalGainPosGainNegSoftThLUT = 0x1,    ///< L1 LUTs-Activity Clamp Th, Normal Scale, Gain +/-, Soft Th
    Layer1GainWeightLUT                         = 0x2,    ///< L1 LUT-Gain Weight
    Layer1SoftThresholdWeightLUT                = 0x3,    ///< L1 LUT-Soft Threshold Weight
    Layer2ActivityNormalGainPosGainNegSoftThLUT = 0x4,    ///< L2 LUTs-Activity Clamp Th, Normal Scale, Gain +/-, Soft Th
    Layer2GainWeightLUT                         = 0x5,    ///< L2 LUT-Gain Weight
    Layer2SoftThresholdWeightLUT                = 0x6,    ///< L2 LUT-Soft Threshold Weight
    ChromaGradientPosNegLUT                     = 0x7,    ///< Chroma Gradient +/- LUT
    ContrastPosNegLUT                           = 0x8,    ///< Contrast +/- LUT
    SkinActivityGainLUT                         = 0x9,    ///< Skit Weight, Activity Gain LUT
    MaxASFLUT                                   = 0xA,    ///< MAX LUT
};

/// @brief: This structure has information of number of entries for each LUT.
static const UINT IPEASFLUTNumEntries[MaxASFLUT] =
{
    0,      ///< NoLUTSelected
    256,    ///< Layer1ActivityNormalGainPosGainNegSoftThLUT
    256,    ///< Layer1GainWeightLUT
    256,    ///< Layer1SoftThresholdWeightLUT
    256,    ///< Layer2ActivityNormalGainPosGainNegSoftThLUT
    256,    ///< Layer2GainWeightLUT
    256,    ///< Layer2SoftThresholdWeightLUT
    256,    ///< ChromaGradientPosNegLUT
    256,    ///< ContrastPosNegLUT
    17,     ///< SkinActivityGainLUT
};

static const UINT MaxASF30LUTNumEntries =
    IPEASFLUTNumEntries[Layer1ActivityNormalGainPosGainNegSoftThLUT] +
    IPEASFLUTNumEntries[Layer1GainWeightLUT]                         +
    IPEASFLUTNumEntries[Layer1SoftThresholdWeightLUT]                +
    IPEASFLUTNumEntries[Layer2ActivityNormalGainPosGainNegSoftThLUT] +
    IPEASFLUTNumEntries[Layer2GainWeightLUT]                         +
    IPEASFLUTNumEntries[Layer2SoftThresholdWeightLUT]                +
    IPEASFLUTNumEntries[ChromaGradientPosNegLUT]                     +
    IPEASFLUTNumEntries[ContrastPosNegLUT]                           +
    IPEASFLUTNumEntries[SkinActivityGainLUT];

static const UINT32 IPEASFLUTBufferSize        = MaxASF30LUTNumEntries * sizeof(UINT32);
static const UINT32 IPEASFLUTBufferSizeInDWord = MaxASF30LUTNumEntries;
static const UINT32 NUM_OF_NZ_ENTRIES          = 8;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPEASF30* pModule = CAMX_NEW IPEASF30(pCreateData->pNodeIdentifier);

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
// IPEASF30::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    CamxResult      result         = CamxResultSuccess;
    IpeIQSettings*  pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

    CAMX_ASSERT(NULL != pInputData);

    if (NULL != pIPEIQSettings)
    {
        if (TRUE == m_moduleEnable)
        {
            CamX::Utils::Memcpy(&pIPEIQSettings->asfParameters, &m_ASFParameters, sizeof(AsfParameters));
            if (TRUE == m_bypassMode)
            {
                pIPEIQSettings->asfParameters.moduleCfg.EN            = 1;
                pIPEIQSettings->asfParameters.moduleCfg.LAYER_1_EN    = 0;
                pIPEIQSettings->asfParameters.moduleCfg.LAYER_2_EN    = 0;
                pIPEIQSettings->asfParameters.moduleCfg.EDGE_ALIGN_EN = 0;
                pIPEIQSettings->asfParameters.moduleCfg.CONTRAST_EN   = 0;
            }
        }
        else
        {
            pIPEIQSettings->asfParameters.moduleCfg.EN = m_moduleEnable;
        }
    }

    // Disable additional ASF modules for realtime streams
    if ((TRUE == pInputData->pipelineIPEData.realtimeFlag) &&
        (TRUE == pInputData->pipelineIPEData.compressiononOutput) &&
        (NULL != pIPEIQSettings))
    {
        pIPEIQSettings->asfParameters.moduleCfg.LAYER_2_EN     = 0;
        pIPEIQSettings->asfParameters.moduleCfg.EDGE_ALIGN_EN  = 0;
        pIPEIQSettings->asfParameters.moduleCfg.CONTRAST_EN    = 0;
        pIPEIQSettings->asfParameters.moduleCfg.CHROMA_GRAD_EN = 0;
    }

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pIPETuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(IPEASFRegCmd) <=
                           sizeof(pInputData->pIPETuningMetadata->IPEASFData.ASFConfig));

        if (((IPEASFLUTBufferSizeInDWord * sizeof(UINT32))  <=
            sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ASFLUT)))
        {
            if (NULL != m_pASFLUTs)
            {
                Utils::Memcpy(&pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ASFLUT,
                              m_pASFLUTs,
                              (IPEASFLUTBufferSizeInDWord * sizeof(UINT32)));
            }
            /// @todo (CAMX-2846) Tuning data has been keep enough for the 68 registers, but writing only 65, data for others,
            ///                   should be obtain from FW.
            Utils::Memcpy(&pInputData->pIPETuningMetadata->IPEASFData.ASFConfig,
                          &m_regCmd,
                          sizeof(IPEASFRegCmd));

            TuningFaceData* pASFFaceDetection = &pInputData->pIPETuningMetadata->IPEASFFaceDetection;

            CAMX_STATIC_ASSERT(MAX_FACE_NUM <= TuningMaxFaceNumber);

            if (NULL != m_dependenceData.pFDData)
            {
                pASFFaceDetection->numberOfFace =
                    ((m_dependenceData.pFDData->numberOfFace) > MAX_FACE_NUM) ?
                    MAX_FACE_NUM : (m_dependenceData.pFDData->numberOfFace);

                for (INT32 count = 0; count < pASFFaceDetection->numberOfFace; count++)
                {
                    pASFFaceDetection->faceRadius[count]  = m_dependenceData.pFDData->faceRadius[count];
                    pASFFaceDetection->faceCenterX[count] = m_dependenceData.pFDData->faceCenterX[count];
                    pASFFaceDetection->faceCenterY[count] = m_dependenceData.pFDData->faceCenterY[count];
                }
            }
        }
        else
        {
            result = CamxResultEInvalidArg;
            CAMX_ASSERT_ALWAYS_MESSAGE("Tuning data, incorrect LUT buffer size or suported face num missmatch error");
        }
    }
    if (NULL != pInputData->pCalculatedData)
    {
        pInputData->pCalculatedData->metadata.edgeMode = m_dependenceData.edgeMode;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::Initialize()
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
// IPEASF30::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::FillCmdBufferManagerParams(
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
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "IPEASF30");
                pResourceParams->resourceSize                 = IPEASFLUTBufferSize;
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
// IPEASF30::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(asf_3_0_0::asf30_rgn_dataType) * (ASF30MaxNoLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Allocate memory for asf_3_0_0::asf30_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    result = WriteLUTtoDMI(pInputData);
    if (CamxResultSuccess == result)
    {
        result = WriteConfigCmds(pInputData);
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "ASF configuration failed");
        result = CamxResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::WriteLUTEntrytoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE CamxResult IPEASF30::WriteLUTEntrytoDMI(
    CmdBuffer*   pDMICmdBuffer,
    const UINT8  LUTIndex,
    UINT32*      pLUTOffset,
    const UINT32 LUTSize)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pDMICmdBuffer) && (NULL != pLUTOffset) && (NULL != m_pLUTDMICmdBuffer))
    {
        result = PacketBuilder::WriteDMI(pDMICmdBuffer,
                                         regIPE_IPE_0_PPS_CLC_ASF_DMI_CFG,
                                         LUTIndex,
                                         m_pLUTDMICmdBuffer,
                                         *pLUTOffset,
                                         LUTSize);
        CAMX_ASSERT(CamxResultSuccess == result);

        // update LUT Offset
        *pLUTOffset += LUTSize;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc,
                       "Pointer parameters are empty, pDMICmdBuffer=%p, pLUTOffset=%p, m_pLUTDMICmdBuffer=%p",
                       pDMICmdBuffer,
                       pLUTOffset,
                       m_pLUTDMICmdBuffer);

        result = CamxResultENoMemory;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult  result        = CamxResultSuccess;
    CmdBuffer*  pDMICmdBuffer = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferDMIHeader];
    UINT32      LUTOffset     = 0;

    CAMX_ASSERT(NULL != pDMICmdBuffer);
    CAMX_ASSERT(NULL != m_pLUTDMICmdBuffer);

    // Record offset where CDM DMI header starts for LUTs for the module
    // Patch the LUTs DMI buffer into CDM pack
    m_offsetLUT = (pDMICmdBuffer->GetResourceUsedDwords() * sizeof(UINT32));

    WriteLUTEntrytoDMI(pDMICmdBuffer,
                       Layer1ActivityNormalGainPosGainNegSoftThLUT,
                       &LUTOffset,
                       IPEASFLUTNumEntries[Layer1ActivityNormalGainPosGainNegSoftThLUT]*sizeof(UINT32));

    WriteLUTEntrytoDMI(pDMICmdBuffer,
                       Layer1GainWeightLUT,
                       &LUTOffset,
                       IPEASFLUTNumEntries[Layer1GainWeightLUT] * sizeof(UINT32));

    WriteLUTEntrytoDMI(pDMICmdBuffer,
                       Layer1SoftThresholdWeightLUT,
                       &LUTOffset,
                       IPEASFLUTNumEntries[Layer1SoftThresholdWeightLUT] * sizeof(UINT32));

    WriteLUTEntrytoDMI(pDMICmdBuffer,
                       Layer2ActivityNormalGainPosGainNegSoftThLUT,
                       &LUTOffset,
                       IPEASFLUTNumEntries[Layer2ActivityNormalGainPosGainNegSoftThLUT] * sizeof(UINT32));

    WriteLUTEntrytoDMI(pDMICmdBuffer,
                       Layer2GainWeightLUT,
                       &LUTOffset,
                       IPEASFLUTNumEntries[Layer2GainWeightLUT] * sizeof(UINT32));

    WriteLUTEntrytoDMI(pDMICmdBuffer,
                       Layer2SoftThresholdWeightLUT,
                       &LUTOffset,
                       IPEASFLUTNumEntries[Layer2SoftThresholdWeightLUT] * sizeof(UINT32));

    WriteLUTEntrytoDMI(pDMICmdBuffer,
                       ChromaGradientPosNegLUT,
                       &LUTOffset,
                       IPEASFLUTNumEntries[ChromaGradientPosNegLUT] * sizeof(UINT32));

    WriteLUTEntrytoDMI(pDMICmdBuffer,
                       ContrastPosNegLUT,
                       &LUTOffset,
                       IPEASFLUTNumEntries[ContrastPosNegLUT] * sizeof(UINT32));

    WriteLUTEntrytoDMI(pDMICmdBuffer,
                       SkinActivityGainLUT,
                       &LUTOffset,
                       IPEASFLUTNumEntries[SkinActivityGainLUT] * sizeof(UINT32));

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::WriteConfigCmds
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::WriteConfigCmds(
    const ISPInputData* pInputData)
{
    CamxResult  result     = CamxResultSuccess;
    CmdBuffer*  pCmdBuffer = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPostLTM];

    CAMX_ASSERT(NULL != pCmdBuffer);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1Config));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2Config));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_NORM_SCALE_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFNormalScaleConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_GAIN_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFGainConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_NZ_FLAG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFNegativeAndZeroFlag));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1SharpnessCoeffConfig0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1SharpnessCoeffConfig1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1SharpnessCoeffConfig2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_3,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1SharpnessCoeffConfig3));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_4,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1SharpnessCoeffConfig4));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1LPFCoeffConfig0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1LPFCoeffConfig1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1LPFCoeffConfig2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_3,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1LPFCoeffConfig3));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_4,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1LPFCoeffConfig4));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_ACT_BPF_COEFF_CFG_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1ActivityBPFCoeffConfig0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_ACT_BPF_COEFF_CFG_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1ActivityBPFCoeffConfig1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_1_ACT_BPF_COEFF_CFG_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer1ActivityBPFCoeffConfig2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_SHARP_COEFF_CFG_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2SharpnessCoeffConfig0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_SHARP_COEFF_CFG_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2SharpnessCoeffConfig1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_SHARP_COEFF_CFG_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2SharpnessCoeffConfig2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_LPF_COEFF_CFG_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2LPFCoeffConfig0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_LPF_COEFF_CFG_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2LPFCoeffConfig1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_LPF_COEFF_CFG_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2LPFCoeffConfig2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_ACT_BPF_COEFF_CFG_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2ActivityBPFCoeffConfig0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_ACT_BPF_COEFF_CFG_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2ActivityBPFCoeffConfig1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_LAYER_2_ACT_BPF_COEFF_CFG_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFLayer2ActivityBPFCoeffConfig2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_R_SQUARE_LUT_ENTRY_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRRSquareLUTEntry0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_R_SQUARE_LUT_ENTRY_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRRSquareLUTEntry1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_R_SQUARE_LUT_ENTRY_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRRSquareLUTEntry2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_SQUARE_CFG_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRSquareConfig0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_LUT_ENTRY_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRActivityCFLUTEntry0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_LUT_ENTRY_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRActivityCFLUTEntry1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_LUT_ENTRY_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRActivityCFLUTEntry2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_SLOPE_LUT_ENTRY_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRActivitySlopeLUTEntry0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_SLOPE_LUT_ENTRY_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRActivitySlopeLUTEntry1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_SLOPE_LUT_ENTRY_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRActivitySlopeLUTEntry2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_SHIFT_LUT_ENTRY_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRActivityCFShiftLUTEntry0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_SHIFT_LUT_ENTRY_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRActivityCFShiftLUTEntry1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_SHIFT_LUT_ENTRY_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRActivityCFShiftLUTEntry2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_LUT_ENTRY_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRGainCFLUTEntry0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_LUT_ENTRY_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRGainCFLUTEntry1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_LUT_ENTRY_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRGainCFLUTEntry2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_SLOPE_LUT_ENTRY_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRGainSlopeLUTEntry0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_SLOPE_LUT_ENTRY_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRGainSlopeLUTEntry1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_SLOPE_LUT_ENTRY_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRGainSlopeLUTEntry2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_SHIFT_LUT_ENTRY_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRGainCFShiftLUTEntry0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_SHIFT_LUT_ENTRY_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRGainCFShiftLUTEntry1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_SHIFT_LUT_ENTRY_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFRNRGainCFShiftLUTEntry2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_THRESHOLD_CFG_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFThresholdConfig0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_THRESHOLD_CFG_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFThresholdConfig1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_SKIN_CFG_0,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFSkinConfig0));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_SKIN_CFG_1,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFSkinConfig1));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_SKIN_CFG_2,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFSkinConfig2));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFaceConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_1_CENTER_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace1CenterConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_1_RADIUS_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace1RadiusConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_2_CENTER_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace2CenterConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_2_RADIUS_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace2RadiusConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_3_CENTER_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace3CenterConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_3_RADIUS_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace3RadiusConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_4_CENTER_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace4CenterConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_4_RADIUS_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace4RadiusConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_5_CENTER_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace5CenterConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIPE_IPE_0_PPS_CLC_ASF_FACE_5_RADIUS_CFG,
                                          0x1,
                                          reinterpret_cast<UINT32*>(&m_regCmd.ASFFace5RadiusConfig));
    CAMX_ASSERT(CamxResultSuccess == result);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::ValidateDependenceParams(
    const ISPInputData* pInputData
    ) const
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-1807) validate dependency parameters
    if ((NULL == pInputData)                                                     ||
        (NULL == pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferDMIHeader]) ||
        (NULL == pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPostLTM])   ||
        (NULL == pInputData->pipelineIPEData.pIPEIQSettings))
    {
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPEASF30::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                  &&
        (NULL != pInputData->pHwContext)      &&
        (NULL != pInputData->pAECUpdateData)  &&
        (NULL != pInputData->pAWBUpdateData))
    {
        ISPHALTagsData* pHALTagsData = pInputData->pHALTagsData;
        UINT32          sceneMode    = 0;
        UINT32          splEffect    = 0;

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", pInputData->pAECUpdateData->luxIndex);


        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->ASFEnable;

            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            if ((NULL != pInputData->pTuningData)  &&
                (NULL != pInputData->pHALTagsData))
            {
                TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
                CAMX_ASSERT(NULL != pTuningManager);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if ((TRUE == pInputData->tuningModeChanged)    &&
                    (TRUE == pTuningManager->IsValidChromatix()))
                {
                    if (pInputData->pTuningData->noOfSelectionParameter == 7)
                    {
                        sceneMode = static_cast<UINT32>(pInputData->pTuningData->TuningMode[5].subMode.scene);
                        splEffect = static_cast<UINT32>(pInputData->pTuningData->TuningMode[6].subMode.effect);
                    }

                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_asf30_ipe(
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
                        m_moduleEnable              = m_pChromatix->enable_section.asf_enable;

                        if (TRUE == m_moduleEnable)
                        {
                            isChanged = TRUE;
                        }
                    }

                    if (TRUE == m_moduleEnable)
                    {
                        m_dependenceData.layer1Enable    = static_cast<UINT16>(
                            m_dependenceData.pChromatix->chromatix_asf30_reserve.layer_1_enable);
                        m_dependenceData.layer2Enable    = static_cast<UINT16>(
                            m_dependenceData.pChromatix->chromatix_asf30_reserve.layer_2_enable);
                        m_dependenceData.contrastEnable  = static_cast<UINT16>(
                            m_dependenceData.pChromatix->chromatix_asf30_reserve.contrast_enable);
                        m_dependenceData.radialEnable    = static_cast<UINT16>(
                            m_dependenceData.pChromatix->chromatix_asf30_reserve.radial_enable);
                        m_dependenceData.edgeAlignEnable = static_cast<UINT16>(
                            m_dependenceData.pChromatix->chromatix_asf30_reserve.edge_alignment_enable);
                    }
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "EdgeMode chromatix %d hal %d edgeMode %d m_moduleEnable %d",
                        m_pChromatix->enable_section.asf_enable,
                        pHALTagsData->edgeMode, m_dependenceData.edgeMode, m_moduleEnable);
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
        }

        if (TRUE == m_moduleEnable)
        {
            SetAdditionalASF30Input(pInputData);

            if (TRUE ==
                IQInterface::s_interpolationTable.ASF30TriggerUpdate(&pInputData->triggerData, &m_dependenceData))
            {
                isChanged = TRUE;
            }
        }
        if (NULL != pHALTagsData)
        {
            if (m_dependenceData.sharpness != pHALTagsData->sharpness)
            {
                isChanged                  = TRUE;
                m_dependenceData.sharpness = pHALTagsData->sharpness;
            }
            m_dependenceData.edgeMode = pHALTagsData->edgeMode;

            m_bypassMode = FALSE;
            if (((EdgeModeOff            == pHALTagsData->edgeMode)         ||
                ((EdgeModeZeroShutterLag == pHALTagsData->edgeMode)         &&
                (FALSE == pInputData->pipelineIPEData.isLowResolution)))    &&
                (0                       == sceneMode)                      &&
                (0                       == splEffect))
            {
                m_bypassMode = TRUE;
            }

            CAMX_LOG_INFO(CamxLogGroupPProc, "EdgeMode %d m_bypassMode %d sceneMode %d splEffect %d sharpness %f",
                pHALTagsData->edgeMode, m_bypassMode, sceneMode, splEffect, m_dependenceData.sharpness);
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
// IPEASF30::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::FetchDMIBuffer()
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

        // Fetch a fresh LUT DMI cmd buffer
        result = m_pLUTCmdBufferManager->GetBuffer(&pPacketResource);
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupISP, "LUT Cmd Buffer Manager is NULL");
    }

    if (CamxResultSuccess == result)
    {
        CAMX_ASSERT(TRUE == pPacketResource->GetUsageFlags().cmdBuffer);
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
// IPEASF30::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult      result         = CamxResultSuccess;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", pInputData->pAECUpdateData->luxIndex);

    result = FetchDMIBuffer();

    if (CamxResultSuccess == result)
    {
        ASF30OutputData outputData = { 0 };
        UINT32*         pLUT       = reinterpret_cast<UINT32*>(m_pLUTDMICmdBuffer->BeginCommands(IPEASFLUTBufferSizeInDWord));
        CAMX_ASSERT(NULL != pLUT);

        outputData.pRegCmd        = &m_regCmd;
        outputData.pDMIDataPtr    = pLUT;
        outputData.pAsfParameters = &m_ASFParameters;

        // BET ONLY - InputData is different per module tested
        pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pDMIDataPtr);

        result = IQInterface::IPEASF30CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);
        if (CamxResultSuccess == result)
        {
            result = m_pLUTDMICmdBuffer->CommitCommands();

            if (NULL != pInputData->pIPETuningMetadata)
            {
                m_pASFLUTs = pLUT;
            }
        }

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "ASF30 Calculation Failed. result %d", result);
        }

        if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
        {
            DumpRegConfig(outputData.pDMIDataPtr);
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Cannot get buffer from CmdBufferManager");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEASF30::Execute(
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
// IPEASF30::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEASF30::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::IPEASF30
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEASF30::IPEASF30(
    const CHAR* pNodeIdentifier)
{
    UINT regRangeSizeInDwords = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IPEASFRegCmd));

    m_pNodeIdentifier         = pNodeIdentifier;
    m_type                    = ISPIQModuleType::IPEASF;
    m_moduleEnable            = TRUE;
    m_cmdLength               = regRangeSizeInDwords * RegisterWidthInBytes;
    m_numLUT                  = MaxASFLUT - 1;

    m_pASFLUTs                = NULL;
    m_pChromatix              = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPEASF30::~IPEASF30
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEASF30::~IPEASF30()
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
/// IPEASF30::SetAdditionalASF30Input
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEASF30::SetAdditionalASF30Input(
    ISPInputData* pInputData)
{
    UINT8 negateAbsoluteY1            = 0;
    UINT8 specialEffectAbsoluteEnable = 0;
    UINT8 specialEffectEnable         = 0;

    CAMX_ASSERT(NULL != pInputData);

    for (UINT i = 0; i < pInputData->pTuningData->noOfSelectionParameter; i++)
    {
        if (ChiModeType::Effect == pInputData->pTuningData->TuningMode[i].mode)
        {
            specialEffectEnable = 1;
            switch(pInputData->pTuningData->TuningMode[i].subMode.effect)
            {
                case ChiModeEffectSubModeType::Emboss:
                    break;
                case ChiModeEffectSubModeType::Sketch:
                    specialEffectAbsoluteEnable = 1;
                    negateAbsoluteY1            = 1;
                    break;
                case ChiModeEffectSubModeType::Neon:
                    specialEffectAbsoluteEnable = 1;
                    break;
                default:
                    specialEffectEnable         = 0;
                    break;
            }
        }
    }

    m_dependenceData.negateAbsoluteY1            = negateAbsoluteY1;
    m_dependenceData.specialEffectAbsoluteEnable = specialEffectAbsoluteEnable;
    m_dependenceData.specialEffectEnable         = specialEffectEnable;
    m_dependenceData.specialPercentage           = 0;
    m_dependenceData.smoothPercentage            = 0;

    if (1 == specialEffectEnable)
    {
        m_dependenceData.nonZero[0] = 0x1;
        m_dependenceData.nonZero[1] = 0x1;
        m_dependenceData.nonZero[2] = 0x0;
        m_dependenceData.nonZero[3] = 0x3;
        m_dependenceData.nonZero[4] = 0x3;
        m_dependenceData.nonZero[5] = 0x3;
        m_dependenceData.nonZero[6] = 0x0;
        m_dependenceData.nonZero[7] = 0x1;

    }
    else
    {
        for (UINT i = 0; i < NUM_OF_NZ_ENTRIES; i++)
        {
            m_dependenceData.nonZero[i] = 1;
        }
    }

    // @todo (CAMX-2109) Map scale ratio from ISP to algorithm interface
    // m_dependenceData.totalScaleRatio   = 1.0f;

    m_dependenceData.chYStreamInWidth  = pInputData->pipelineIPEData.inputDimension.widthPixels;
    m_dependenceData.chYStreamInHeight = pInputData->pipelineIPEData.inputDimension.heightLines;

    // @todo (CAMX-2143) Map ASF Face parameters from ISP to algorithm interface
    m_dependenceData.pFDData               = &pInputData->fDData;
    m_dependenceData.pWarpGeometriesOutput = pInputData->pipelineIPEData.pWarpGeometryData;
    m_dependenceData.faceHorzOffset        = 0;
    m_dependenceData.faceVertOffset        = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEASF30::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEASF30::DumpRegConfig(
    UINT32* pDMIDataPtr
    ) const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE ASF30 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* ASF30 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "LAYER 1 Activity Clamp Threshold = %x\n",
            m_regCmd.ASFLayer1Config.bitfields.ACTIVITY_CLAMP_THRESHOLD);
        CamX::OsUtils::FPrintF(pFile, "LAYER 1 Clamp Lower Bound        = %x\n",
            m_regCmd.ASFLayer1Config.bitfields.CLAMP_LL);
        CamX::OsUtils::FPrintF(pFile, "LAYER 1 Clamp Upper Bound        = %x\n",
            m_regCmd.ASFLayer1Config.bitfields.CLAMP_UL);
        CamX::OsUtils::FPrintF(pFile, "LAYER 2 Activity Clamp Threshold = %x\n",
            m_regCmd.ASFLayer2Config.bitfields.ACTIVITY_CLAMP_THRESHOLD);
        CamX::OsUtils::FPrintF(pFile, "LAYER 2 Clamp Lower Bound        = %x\n",
            m_regCmd.ASFLayer2Config.bitfields.CLAMP_LL);
        CamX::OsUtils::FPrintF(pFile, "LAYER 2 Clamp Upper Bound        = %x\n",
            m_regCmd.ASFLayer2Config.bitfields.CLAMP_UL);
        CamX::OsUtils::FPrintF(pFile, "LAYER 1 L2 NORM EN               = %x\n",
            m_regCmd.ASFNormalScaleConfig.bitfields.LAYER_1_L2_NORM_EN);
        CamX::OsUtils::FPrintF(pFile, "LAYER 1 Norm Scale               = %x\n",
            m_regCmd.ASFNormalScaleConfig.bitfields.LAYER_1_NORM_SCALE);
        CamX::OsUtils::FPrintF(pFile, "LAYER 2 L2 NORM EN               = %x\n",
            m_regCmd.ASFNormalScaleConfig.bitfields.LAYER_2_L2_NORM_EN);
        CamX::OsUtils::FPrintF(pFile, "LAYER 2 Norm Scale               = %x\n",
            m_regCmd.ASFNormalScaleConfig.bitfields.LAYER_2_L2_NORM_EN);
        CamX::OsUtils::FPrintF(pFile, "Gain Cap                         = %x\n",
            m_regCmd.ASFGainConfig.bitfields.GAIN_CAP);
        CamX::OsUtils::FPrintF(pFile, "Gamma Corrected Luma Target      = %x\n",
            m_regCmd.ASFGainConfig.bitfields.GAMMA_CORRECTED_LUMA_TARGET);
        CamX::OsUtils::FPrintF(pFile, "Median Blend Lower Offset        = %x\n",
            m_regCmd.ASFGainConfig.bitfields.MEDIAN_BLEND_LOWER_OFFSET);
        CamX::OsUtils::FPrintF(pFile, "Median Blend Upper Offset        = %x\n",
            m_regCmd.ASFGainConfig.bitfields.MEDIAN_BLEND_UPPER_OFFSET);
        CamX::OsUtils::FPrintF(pFile, "NZ Flag 0                        = %x\n",
            m_regCmd.ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_0);
        CamX::OsUtils::FPrintF(pFile, "NZ Flag 1                        = %x\n",
            m_regCmd.ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_1);
        CamX::OsUtils::FPrintF(pFile, "NZ Flag 2                        = %x\n",
            m_regCmd.ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_2);
        CamX::OsUtils::FPrintF(pFile, "NZ Flag 3                        = %x\n",
            m_regCmd.ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_3);
        CamX::OsUtils::FPrintF(pFile, "NZ Flag 4                        = %x\n",
            m_regCmd.ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_4);
        CamX::OsUtils::FPrintF(pFile, "NZ Flag 5                        = %x\n",
            m_regCmd.ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_5);
        CamX::OsUtils::FPrintF(pFile, "NZ Flag 6                        = %x\n",
            m_regCmd.ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_6);
        CamX::OsUtils::FPrintF(pFile, "NZ Flag 7                        = %x\n",
            m_regCmd.ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_7);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 0          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig0.bitfields.COEFF0);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 1          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig0.bitfields.COEFF1);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 2          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig1.bitfields.COEFF2);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 3          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig1.bitfields.COEFF3);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 4          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig2.bitfields.COEFF4);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 5          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig2.bitfields.COEFF5);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 6          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig3.bitfields.COEFF6);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 7          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig3.bitfields.COEFF7);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 8          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig4.bitfields.COEFF8);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Sharping Coeff 9          = %x\n",
            m_regCmd.ASFLayer1SharpnessCoeffConfig4.bitfields.COEFF9);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 0   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig0.bitfields.COEFF0);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 1   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig0.bitfields.COEFF1);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 2   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig1.bitfields.COEFF2);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 3   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig1.bitfields.COEFF3);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 4   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig2.bitfields.COEFF4);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 5   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig2.bitfields.COEFF5);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 6   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig3.bitfields.COEFF6);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 7   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig3.bitfields.COEFF7);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 8   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig4.bitfields.COEFF8);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Low Pass Filter Coeff 9   = %x\n",
            m_regCmd.ASFLayer1LPFCoeffConfig4.bitfields.COEFF9);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Band Pass Filter Coeff 0  = %x\n",
            m_regCmd.ASFLayer1ActivityBPFCoeffConfig0.bitfields.COEFF0);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Band Pass Filter Coeff 1  = %x\n",
            m_regCmd.ASFLayer1ActivityBPFCoeffConfig0.bitfields.COEFF1);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Band Pass Filter Coeff 2  = %x\n",
            m_regCmd.ASFLayer1ActivityBPFCoeffConfig1.bitfields.COEFF2);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Band Pass Filter Coeff 3  = %x\n",
            m_regCmd.ASFLayer1ActivityBPFCoeffConfig1.bitfields.COEFF3);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Band Pass Filter Coeff 4  = %x\n",
            m_regCmd.ASFLayer1ActivityBPFCoeffConfig2.bitfields.COEFF4);
        CamX::OsUtils::FPrintF(pFile, "Layer1 Band Pass Filter Coeff 5  = %x\n",
            m_regCmd.ASFLayer1ActivityBPFCoeffConfig2.bitfields.COEFF5);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Sharping Coeff 0          = %x\n",
            m_regCmd.ASFLayer2SharpnessCoeffConfig0.bitfields.COEFF0);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Sharping Coeff 1          = %x\n",
            m_regCmd.ASFLayer2SharpnessCoeffConfig0.bitfields.COEFF1);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Sharping Coeff 2          = %x\n",
            m_regCmd.ASFLayer2SharpnessCoeffConfig1.bitfields.COEFF2);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Sharping Coeff 3          = %x\n",
            m_regCmd.ASFLayer2SharpnessCoeffConfig1.bitfields.COEFF3);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Sharping Coeff 4          = %x\n",
            m_regCmd.ASFLayer2SharpnessCoeffConfig2.bitfields.COEFF4);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Sharping Coeff 5          = %x\n",
            m_regCmd.ASFLayer2SharpnessCoeffConfig2.bitfields.COEFF5);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Low Pass Filter Coeff 0   = %x\n",
            m_regCmd.ASFLayer2LPFCoeffConfig0.bitfields.COEFF0);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Low Pass Filter Coeff 1   = %x\n",
            m_regCmd.ASFLayer2LPFCoeffConfig0.bitfields.COEFF1);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Low Pass Filter Coeff 2   = %x\n",
            m_regCmd.ASFLayer2LPFCoeffConfig1.bitfields.COEFF2);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Low Pass Filter Coeff 3   = %x\n",
            m_regCmd.ASFLayer2LPFCoeffConfig1.bitfields.COEFF3);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Low Pass Filter Coeff 4   = %x\n",
            m_regCmd.ASFLayer2LPFCoeffConfig2.bitfields.COEFF4);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Low Pass Filter Coeff 5   = %x\n",
            m_regCmd.ASFLayer2LPFCoeffConfig2.bitfields.COEFF5);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Band Pass Filter Coeff 0  = %x\n",
            m_regCmd.ASFLayer2ActivityBPFCoeffConfig0.bitfields.COEFF0);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Band Pass Filter Coeff 1  = %x\n",
            m_regCmd.ASFLayer2ActivityBPFCoeffConfig0.bitfields.COEFF1);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Band Pass Filter Coeff 2  = %x\n",
            m_regCmd.ASFLayer2ActivityBPFCoeffConfig1.bitfields.COEFF2);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Band Pass Filter Coeff 3  = %x\n",
            m_regCmd.ASFLayer2ActivityBPFCoeffConfig1.bitfields.COEFF3);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Band Pass Filter Coeff 4  = %x\n",
            m_regCmd.ASFLayer2ActivityBPFCoeffConfig2.bitfields.COEFF4);
        CamX::OsUtils::FPrintF(pFile, "Layer2 Band Pass Filter Coeff 5  = %x\n",
            m_regCmd.ASFLayer2ActivityBPFCoeffConfig2.bitfields.COEFF5);
        CamX::OsUtils::FPrintF(pFile, "RNR LUT Entry 0 RSquare          = %x\n",
            m_regCmd.ASFRNRRSquareLUTEntry0.bitfields.R_SQUARE);
        CamX::OsUtils::FPrintF(pFile, "RNR LUT Entry 1 RSquare          = %x\n",
            m_regCmd.ASFRNRRSquareLUTEntry1.bitfields.R_SQUARE);
        CamX::OsUtils::FPrintF(pFile, "RNR LUT Entry 2 RSquare          = %x\n",
            m_regCmd.ASFRNRRSquareLUTEntry2.bitfields.R_SQUARE);
        CamX::OsUtils::FPrintF(pFile, "RNR RSquare Scale                = %x\n",
            m_regCmd.ASFRNRSquareConfig0.bitfields.R_SQUARE_SCALE);
        CamX::OsUtils::FPrintF(pFile, "RNR RSquare Shift                = %x\n",
            m_regCmd.ASFRNRSquareConfig0.bitfields.R_SQUARE_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 0 Activity CF            = %x\n",
            m_regCmd.ASFRNRActivityCFLUTEntry0.bitfields.ACTIVITY_CF);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 1 Activity CF            = %x\n",
            m_regCmd.ASFRNRActivityCFLUTEntry1.bitfields.ACTIVITY_CF);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 2 Activity CF            = %x\n",
            m_regCmd.ASFRNRActivityCFLUTEntry2.bitfields.ACTIVITY_CF);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 0 Activity Slope         = %x\n",
            m_regCmd.ASFRNRActivitySlopeLUTEntry0.bitfields.ACTIVITY_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 1 Activity Slope         = %x\n",
            m_regCmd.ASFRNRActivitySlopeLUTEntry1.bitfields.ACTIVITY_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 2 Activity Slope         = %x\n",
            m_regCmd.ASFRNRActivitySlopeLUTEntry2.bitfields.ACTIVITY_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 0 Activity CF Shift      = %x\n",
            m_regCmd.ASFRNRActivityCFShiftLUTEntry0.bitfields.ACTIVITY_CF_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 1 Activity CF Shift      = %x\n",
            m_regCmd.ASFRNRActivityCFShiftLUTEntry1.bitfields.ACTIVITY_CF_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 2 Activity CF Shift      = %x\n",
            m_regCmd.ASFRNRActivityCFShiftLUTEntry2.bitfields.ACTIVITY_CF_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 0 Gain CF                = %x\n",
            m_regCmd.ASFRNRGainCFLUTEntry0.bitfields.GAIN_CF);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 1 Gain CF                = %x\n",
            m_regCmd.ASFRNRGainCFLUTEntry1.bitfields.GAIN_CF);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 2 Gain CF                = %x\n",
            m_regCmd.ASFRNRGainCFLUTEntry2.bitfields.GAIN_CF);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 0 Gain Slope             = %x\n",
            m_regCmd.ASFRNRGainSlopeLUTEntry0.bitfields.GAIN_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 1 Gain Slope             = %x\n",
            m_regCmd.ASFRNRGainSlopeLUTEntry1.bitfields.GAIN_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 2 Gain Slope             = %x\n",
            m_regCmd.ASFRNRGainSlopeLUTEntry2.bitfields.GAIN_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 0 Gain CF Shift          = %x\n",
            m_regCmd.ASFRNRGainCFShiftLUTEntry0.bitfields.GAIN_CF_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 1 Gain CF Shift          = %x\n",
            m_regCmd.ASFRNRGainCFShiftLUTEntry1.bitfields.GAIN_CF_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "RNR Lut 2 Gain CF Shift          = %x\n",
            m_regCmd.ASFRNRGainCFShiftLUTEntry2.bitfields.GAIN_CF_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Flat Threshold                   = %x\n",
            m_regCmd.ASFThresholdConfig0.bitfields.FLAT_TH);
        CamX::OsUtils::FPrintF(pFile, "Max Smoothing Clamp              = %x\n",
            m_regCmd.ASFThresholdConfig0.bitfields.TEXTURE_TH);
        CamX::OsUtils::FPrintF(pFile, "Corner Threshold                 = %x\n",
            m_regCmd.ASFThresholdConfig1.bitfields.SIMILARITY_TH);
        CamX::OsUtils::FPrintF(pFile, "Smooth Strength                  = %x\n",
            m_regCmd.ASFThresholdConfig1.bitfields.SMOOTH_STRENGTH);
        CamX::OsUtils::FPrintF(pFile, "Hue Min                          = %x\n",
            m_regCmd.ASFSkinConfig0.bitfields.H_MIN);
        CamX::OsUtils::FPrintF(pFile, "Hue Max                          = %x\n",
            m_regCmd.ASFSkinConfig0.bitfields.H_MAX);
        CamX::OsUtils::FPrintF(pFile, "Y Min                            = %x\n",
            m_regCmd.ASFSkinConfig0.bitfields.Y_MIN);
        CamX::OsUtils::FPrintF(pFile, "Y Max                            = %x\n",
            m_regCmd.ASFSkinConfig1.bitfields.Y_MAX);
        CamX::OsUtils::FPrintF(pFile, "Q Non Skin                       = %x\n",
            m_regCmd.ASFSkinConfig1.bitfields.Q_NON_SKIN);
        CamX::OsUtils::FPrintF(pFile, "Q Skin                           = %x\n",
            m_regCmd.ASFSkinConfig1.bitfields.Q_SKIN);
        CamX::OsUtils::FPrintF(pFile, "Boundary Probability             = %x\n",
            m_regCmd.ASFSkinConfig1.bitfields.BOUNDARY_PROB);
        CamX::OsUtils::FPrintF(pFile, "SHY Max                          = %x\n",
            m_regCmd.ASFSkinConfig2.bitfields.SHY_MAX);
        CamX::OsUtils::FPrintF(pFile, "SHY Min                          = %x\n",
            m_regCmd.ASFSkinConfig2.bitfields.SHY_MIN);
        CamX::OsUtils::FPrintF(pFile, "SMax Para                        = %x\n",
            m_regCmd.ASFSkinConfig2.bitfields.SMAX_PARA);
        CamX::OsUtils::FPrintF(pFile, "SMin Para                        = %x\n",
            m_regCmd.ASFSkinConfig2.bitfields.SMIN_PARA);
        CamX::OsUtils::FPrintF(pFile, "Face Enable                      = %x\n",
            m_regCmd.ASFFaceConfig.bitfields.FACE_EN);
        CamX::OsUtils::FPrintF(pFile, "Face Number                      = %x\n",
            m_regCmd.ASFFaceConfig.bitfields.FACE_NUM);
        CamX::OsUtils::FPrintF(pFile, "Face1 Center Cfg Horz Offset     = %x\n",
            m_regCmd.ASFFace1CenterConfig.bitfields.FACE_CENTER_HORIZONTAL);
        CamX::OsUtils::FPrintF(pFile, "Face1 Center Cfg Vert Offset     = %x\n",
            m_regCmd.ASFFace1CenterConfig.bitfields.FACE_CENTER_VERTICAL);
        CamX::OsUtils::FPrintF(pFile, "Face1 Radius Cfg Radius Boundary = %x\n",
            m_regCmd.ASFFace1RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY);
        CamX::OsUtils::FPrintF(pFile, "Face1 Radius Cfg Radius Shift    = %x\n",
            m_regCmd.ASFFace1RadiusConfig.bitfields.FACE_RADIUS_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Face1 Radius Cfg Radius Slope    = %x\n",
            m_regCmd.ASFFace1RadiusConfig.bitfields.FACE_RADIUS_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "Face1 Radius Cfg Slope Shift     = %x\n",
            m_regCmd.ASFFace1RadiusConfig.bitfields.FACE_SLOPE_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Face2 Center Cfg Horz Offse      = %x\n",
            m_regCmd.ASFFace2CenterConfig.bitfields.FACE_CENTER_HORIZONTAL);
        CamX::OsUtils::FPrintF(pFile, "Face2 Center Cfg Vert Offset     = %x\n",
            m_regCmd.ASFFace2CenterConfig.bitfields.FACE_CENTER_VERTICAL);
        CamX::OsUtils::FPrintF(pFile, "Face2 Radius Cfg Radius Boundary = %x\n",
            m_regCmd.ASFFace2RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY);
        CamX::OsUtils::FPrintF(pFile, "Face2 Radius Cfg Radius Shift    = %x\n",
            m_regCmd.ASFFace2RadiusConfig.bitfields.FACE_RADIUS_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Face2 Radius Cfg Radius Slope    = %x\n",
            m_regCmd.ASFFace2RadiusConfig.bitfields.FACE_RADIUS_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "Face2 Radius Cfg Slope Shift     = %x\n",
            m_regCmd.ASFFace2RadiusConfig.bitfields.FACE_SLOPE_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Face3 Center Cfg Horz Offse      = %x\n",
            m_regCmd.ASFFace3CenterConfig.bitfields.FACE_CENTER_HORIZONTAL);
        CamX::OsUtils::FPrintF(pFile, "Face3 Center Cfg Vert Offset     = %x\n",
            m_regCmd.ASFFace3CenterConfig.bitfields.FACE_CENTER_VERTICAL);
        CamX::OsUtils::FPrintF(pFile, "Face3 Radius Cfg Radius Boundary = %x\n",
            m_regCmd.ASFFace3RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY);
        CamX::OsUtils::FPrintF(pFile, "Face3 Radius Cfg Radius Shift    = %x\n",
            m_regCmd.ASFFace3RadiusConfig.bitfields.FACE_RADIUS_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Face3 Radius Cfg Radius Slope    = %x\n",
            m_regCmd.ASFFace3RadiusConfig.bitfields.FACE_RADIUS_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "Face3 Radius Cfg Slope Shift     = %x\n",
            m_regCmd.ASFFace3RadiusConfig.bitfields.FACE_SLOPE_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Face4 Center Cfg Horz Offse      = %x\n",
            m_regCmd.ASFFace4CenterConfig.bitfields.FACE_CENTER_HORIZONTAL);
        CamX::OsUtils::FPrintF(pFile, "Face4 Center Cfg Vert Offset     = %x\n",
            m_regCmd.ASFFace4CenterConfig.bitfields.FACE_CENTER_VERTICAL);
        CamX::OsUtils::FPrintF(pFile, "Face4 Radius Cfg Radius Boundary = %x\n",
            m_regCmd.ASFFace4RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY);
        CamX::OsUtils::FPrintF(pFile, "Face4 Radius Cfg Radius Shift    = %x\n",
            m_regCmd.ASFFace4RadiusConfig.bitfields.FACE_RADIUS_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Face4 Radius Cfg Radius Slope    = %x\n",
            m_regCmd.ASFFace4RadiusConfig.bitfields.FACE_RADIUS_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "Face4 Radius Cfg Slope Shift     = %x\n",
            m_regCmd.ASFFace4RadiusConfig.bitfields.FACE_SLOPE_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Face5 Center Cfg Horz Offse      = %x\n",
            m_regCmd.ASFFace5CenterConfig.bitfields.FACE_CENTER_HORIZONTAL);
        CamX::OsUtils::FPrintF(pFile, "Face5 Center Cfg Vert Offset     = %x\n",
            m_regCmd.ASFFace5CenterConfig.bitfields.FACE_CENTER_VERTICAL);
        CamX::OsUtils::FPrintF(pFile, "Face5 Radius Cfg Radius Boundary = %x\n",
            m_regCmd.ASFFace5RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY);
        CamX::OsUtils::FPrintF(pFile, "Face5 Radius Cfg Radius Shift    = %x\n",
            m_regCmd.ASFFace5RadiusConfig.bitfields.FACE_RADIUS_SHIFT);
        CamX::OsUtils::FPrintF(pFile, "Face5 Radius Cfg Radius Slope    = %x\n",
            m_regCmd.ASFFace5RadiusConfig.bitfields.FACE_RADIUS_SLOPE);
        CamX::OsUtils::FPrintF(pFile, "Face5 Radius Cfg Slope Shift     = %x\n",
            m_regCmd.ASFFace5RadiusConfig.bitfields.FACE_SLOPE_SHIFT);

        CamX::OsUtils::FPrintF(pFile, "\n* ASF30 Module CGF \n");

        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.EN                 = %d\n",
            m_ASFParameters.moduleCfg.EN);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.SP_EFF_EN          = %d\n",
            m_ASFParameters.moduleCfg.SP_EFF_EN);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.SP_EFF_ABS_EN      = %d\n",
            m_ASFParameters.moduleCfg.SP_EFF_ABS_EN);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.LAYER_1_EN         = %d\n",
            m_ASFParameters.moduleCfg.LAYER_1_EN);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.LAYER_2_EN         = %d\n",
            m_ASFParameters.moduleCfg.LAYER_2_EN);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.CONTRAST_EN        = %d\n",
            m_ASFParameters.moduleCfg.CONTRAST_EN);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.CHROMA_GRAD_EN     = %d\n",
            m_ASFParameters.moduleCfg.CHROMA_GRAD_EN);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.EDGE_ALIGN_EN      = %d\n",
            m_ASFParameters.moduleCfg.EDGE_ALIGN_EN);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.SKIN_EN            = %d\n",
            m_ASFParameters.moduleCfg.SKIN_EN);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.RNR_ENABLE         = %d\n",
            m_ASFParameters.moduleCfg.RNR_ENABLE);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.NEG_ABS_Y1         = %d\n",
            m_ASFParameters.moduleCfg.NEG_ABS_Y1);
        CamX::OsUtils::FPrintF(pFile, "asfParameters.moduleCfg.SP                 = %d\n",
            m_ASFParameters.moduleCfg.SP);


        if (TRUE == m_moduleEnable)
        {
            UINT32 offset               = 0;

            CamX::OsUtils::FPrintF(pFile, "* asf30Data.layer_1_soft_threshold_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr+offset+index)) & 0xFF));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_1_gain_negative_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr+offset+index) >> 8) & 0xFF));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_1_gain_positive_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr+offset+index) >> 16) & 0xFF));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_1_activity_normalization_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr+offset+index) >> 24) & 0xFF));
            }

            offset += DMI_WEIGHT_MOD_SIZE;
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_1_gain_weight_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)) & 0xFF));
            }

            offset += DMI_WEIGHT_MOD_SIZE;
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_1_soft_threshold_weight_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)) & 0xFF));
            }

            offset += DMI_WEIGHT_MOD_SIZE;
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_2_soft_threshold_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr+offset+index)) & 0xFF));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_2_gain_negative_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr+offset+index) >> 8) & 0xFF));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_2_gain_positive_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr+offset+index) >> 16) & 0xFF));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_2_activity_normalization_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr+offset+index) >> 24) & 0xFF));
            }

            offset += DMI_WEIGHT_MOD_SIZE;
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_2_gain_weight_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)) & 0xFF));
            }

            offset += DMI_WEIGHT_MOD_SIZE;
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.layer_2_soft_threshold_weight_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)) & 0xFF));
            }

            offset += DMI_WEIGHT_MOD_SIZE;
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.chroma_gradient_negative_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)) & 0xFF));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.chroma_gradient_positive_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)>>8) & 0xFF));
            }

            offset += DMI_WEIGHT_MOD_SIZE;
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.gain_contrast_negative_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)) & 0xFF));
            }
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.gain_contrast_positive_lut[2][256] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)>>8) & 0xFF));
            }

            offset += DMI_WEIGHT_MOD_SIZE;
            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.skin_gain_lut[2][17] = \n");
            for (UINT index = 0; index < DMI_SKIN_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)) & 0xFF));
            }

            CamX::OsUtils::FPrintF(pFile, "\n* asf30Data.skin_activity_lut[2][17] = \n");
            for (UINT index = 0; index < DMI_WEIGHT_MOD_SIZE; index++)
            {
                CamX::OsUtils::FPrintF(pFile, "           %d", static_cast<UINT8>((*(pDMIDataPtr + offset + index)>>8) & 0xFF));
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
