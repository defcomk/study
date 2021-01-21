////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsnode.cpp
/// @brief BPS Node class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxcdmdefs.h"
#include "camxcslicpdefs.h"
#include "camxcslispdefs.h"
#include "camxcslresourcedefs.h"
#include "camximagebuffer.h"
#include "camximageformatutils.h"
#include "camxhal3metadatautil.h"
#include "camxhwcontext.h"
#include "camxpipeline.h"
#include "camxtrace.h"
#include "camxvendortags.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "camxtitan17xcontext.h"
#include "camxtitan17xdefs.h"
#include "camxbpsabf40.h"
#include "camxbpsawbbgstats14.h"
#include "camxbpsbpcpdpc20.h"
#include "camxbpscc13.h"
#include "camxbpscst12.h"
#include "camxbpsdemosaic36.h"
#include "camxbpsdemux13.h"
#include "camxbpsgamma16.h"
#include "camxbpsgic30.h"
#include "camxbpsgtm10.h"
#include "camxbpshdr22.h"
#include "camxbpshdrbhiststats13.h"
#include "camxbpshnr10.h"
#include "camxbpslinearization34.h"
#include "camxbpslsc34.h"
#include "camxbpspedestal13.h"
#include "camxbpswb13.h"
#include "camxispstatsmodule.h"
#include "camxispiqmodule.h"
#include "camxbpsnode.h"
#include "camxiqinterface.h"
#include "camxtranslator.h"
#include "chituningmodeparam.h"
#include "camxswtmc11.h"

/// @brief  Function to transalate HAl formats to BPS firmware image format.
/// This function needs to be outside camx namespace due to ImageFormat data structure
/// @todo (CAMX-1015) To avoid this translation if possible
ImageFormat TranslateBPSFormatToFirmwareImageFormat(
   const CamX::ImageFormat* pImageFormat,
   const BPS_IO_IMAGES      firmwarePortId)
{
    ImageFormat firmwareFormat = IMAGE_FORMAT_INVALID;

    /// @todo (CAMX-1375) Update the Plain16- YUV420 10b / P010-YUV420 10b / Plain16-Y 10b /
    /// P010-Y 10b / PD10. Update raw formats based bpp.

    switch (pImageFormat->format)
    {
        case CamX::Format::YUV420NV12TP10:
        case CamX::Format::YUV420NV21TP10:
            firmwareFormat = IMAGE_FORMAT_LINEAR_TP_10;
            break;
        case CamX::Format::UBWCTP10:
            firmwareFormat = IMAGE_FORMAT_UBWC_TP_10;
            break;
        case CamX::Format::RawMIPI:
            switch (pImageFormat->formatParams.rawFormat.bitsPerPixel)
            {
                case 8:
                    firmwareFormat = IMAGE_FORMAT_MIPI_8;
                    break;
                case 10:
                    firmwareFormat = IMAGE_FORMAT_MIPI_10;
                    break;
                case 12:
                    firmwareFormat = IMAGE_FORMAT_MIPI_12;
                    break;
                case 14:
                    firmwareFormat = IMAGE_FORMAT_MIPI_14;
                    break;
                default:
                    CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported MIPI format");
                    break;
            }
            break;
        case CamX::Format::RawPlain16:
            switch (pImageFormat->formatParams.rawFormat.bitsPerPixel)
            {
                case 8:
                    firmwareFormat = IMAGE_FORMAT_BAYER_8;
                    break;
                case 10:
                    firmwareFormat = IMAGE_FORMAT_BAYER_10;
                    break;
                case 12:
                    firmwareFormat = IMAGE_FORMAT_BAYER_12;
                    break;
                case 14:
                    firmwareFormat = IMAGE_FORMAT_BAYER_14;
                    break;
                default:
                    CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported Bayer format");
                    break;
            }
            break;
        case CamX::Format::YUV420NV12:
        case CamX::Format::YUV420NV21:
            firmwareFormat = IMAGE_FORMAT_LINEAR_NV12;
            break;
        case CamX::Format::PD10:
            firmwareFormat = IMAGE_FORMAT_PD_10;
            break;
        case CamX::Format::Blob:
            if (BPS_OUTPUT_IMAGE_STATS_BG == firmwarePortId)
            {
                firmwareFormat = IMAGE_FORMAT_STATISTICS_BAYER_GRID;
            }
            else if (BPS_OUTPUT_IMAGE_STATS_BHIST == firmwarePortId)
            {
                firmwareFormat = IMAGE_FORMAT_STATISTICS_BAYER_HISTOGRAM;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported BPS Port %d", firmwarePortId);
            }
            break;
        case CamX::Format::P010:
            firmwareFormat = IMAGE_FORMAT_LINEAR_P010;
            break;
        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported BPS format %d", pImageFormat->format);
            break;
    }
    return firmwareFormat;
}

/// @brief  Function to transalate UMD  formats to BPS firmware Bayer pixel order
BayerPixelOrder TranslateBPSFormatToFirmwareBayerOrder(const CamX::PixelFormat format)
{
    BayerPixelOrder pixelOrder = FIRST_PIXEL_MAX;

    switch (format)
    {
        case CamX::PixelFormat::BayerBGGR:
            pixelOrder = FIRST_PIXEL_B;
            break;
        case CamX::PixelFormat::BayerGBRG:
            pixelOrder = FIRST_PIXEL_GB;
            break;
        case CamX::PixelFormat::BayerGRBG:
            pixelOrder = FIRST_PIXEL_GR;
            break;
        case CamX::PixelFormat::BayerRGGB:
            pixelOrder = FIRST_PIXEL_R;
            break;
        case CamX::PixelFormat::YUVFormatY:
            pixelOrder = FIRST_PIXEL_R;
            break;
        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Wrong BPS Pixel Format");
            break;
    }

    return pixelOrder;
}
CAMX_NAMESPACE_BEGIN

/// @brief List of BPS IQ modules. Order of modules depends on inter dependency and not pipeline order
static BPSIQModuleInfo BPSIQModulesList[] =
{
    { ISPIQModuleType::SWTMC,                    TRUE,     SWTMC11::CreateBPS },
    { ISPIQModuleType::BPSPedestalCorrection,    FALSE,    BPSPedestal13::Create },
    { ISPIQModuleType::BPSABF,                   TRUE,     BPSABF40::Create },
    { ISPIQModuleType::BPSLinearization,         TRUE,     BPSLinearization34::Create },
    { ISPIQModuleType::BPSHDR,                   TRUE,     BPSHDR22::Create },
    { ISPIQModuleType::BPSBPCPDPC,               TRUE,     BPSBPCPDPC20::Create },
    { ISPIQModuleType::BPSDemux,                 TRUE,     BPSDemux13::Create },
    { ISPIQModuleType::BPSGIC,                   TRUE,     BPSGIC30::Create },
    { ISPIQModuleType::BPSLSC,                   TRUE,     BPSLSC34::Create },
    { ISPIQModuleType::BPSWB,                    TRUE,     BPSWB13::Create },
    { ISPIQModuleType::BPSDemosaic,              TRUE,     BPSDemosaic36::Create },
    { ISPIQModuleType::BPSCC,                    TRUE,     BPSCC13::Create },
    { ISPIQModuleType::BPSGTM,                   TRUE,     BPSGTM10::Create },
    { ISPIQModuleType::BPSGamma,                 TRUE,     BPSGamma16::Create },
    { ISPIQModuleType::BPSCST,                   TRUE,     BPSCST12::Create },
    { ISPIQModuleType::BPSChromaSubSample,       FALSE,    NULL },
    { ISPIQModuleType::BPSHNR,                   TRUE,     BPSHNR10::Create },
};

/// @brief List of BPS Stats modules
static BPSStatsModuleInfo BPSStatsModulesList[] =
{
    { ISPStatsModuleType::BPSAWBBG,              TRUE,    BPSAWBBGStats14::Create },
    { ISPStatsModuleType::BPSHDRBHist,           FALSE,   BPSHDRBHist13Stats::Create },
};

/// @todo (CAMX-1117) Calculate the max number of patches in BPS packet / DMI and update.
static const UINT BPSMaxPatchAddress    = 128;                  //< Max number of patch address that a BPS packet can have
static const UINT BPSMaxDMIPatchAddress = 64;                   //< Number of Max address patching for DMI headers
static const UINT BPSMaxCDMPatchAddress = 64;                   //< Number of Max address patching for CDM headers

static const UINT BPSMaxFWCmdBufferManagers = 10;  ///< Maximum FW command buffer Managers

FrameBuffers        BPSNode::s_frameBufferOffset[BPS_IO_IMAGES_MAX];
UINT32              BPSNode::s_debugDataWriterCounter;
DebugDataWriter*    BPSNode::s_pDebugDataWriter;

static const UINT BPSCmdBufferFrameProcessSizeBytes =
    sizeof(BpsFrameProcess) +
    (sizeof(CDMProgramArray)) +
    (static_cast<UINT>(BPSCDMProgramOrder::BPSProgramIndexMax) * sizeof(CdmProgram));

static const UINT CmdBufferGenericBlobSizeInBytes = (
    CSLGenericBlobHeaderSizeInDwords * sizeof(UINT32) +
    sizeof(CSLICPClockBandwidthRequest) +
    CSLGenericBlobHeaderSizeInDwords * sizeof(UINT32) +
    sizeof(ConfigIORequest) +
    CSLGenericBlobHeaderSizeInDwords * sizeof(UINT32) +
    sizeof(CSLICPMemoryMapUpdate));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::BPSNode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSNode::BPSNode()
    : m_pIQPacketManager(NULL)
    , m_OEMIQSettingEnable(FALSE)
    , m_OEMStatsSettingEnable(FALSE)
{
    m_pNodeName                 = "BPS";
    m_OEMStatsSettingEnable     = GetStaticSettings()->IsOEMStatSettingEnable;
    m_BPSHangDumpEnable         = GetStaticSettings()->enableBPSHangDump;
    m_BPSStripeDumpEnable       = GetStaticSettings()->enableBPSStripeDump;
    m_pPreTuningDataManager     = NULL;

    m_deviceResourceRequest     = { 0 };
    m_configIOMem               = { 0 };
    m_curIntermediateDimension  = { 0, 0, 1.0f };
    m_prevIntermediateDimension = { 0, 0, 1.0f };

    for (UINT port = 0; port < BPS_IO_IMAGES_MAX; port++)
    {
        for (UINT plane = 0; plane < MAX_NUM_OF_IMAGE_PLANES; plane++)
        {
            // Precompute the frame buffer offset for all ports
            s_frameBufferOffset[port].bufferPtr[plane]         =
                static_cast <UINT32>(offsetof(BpsFrameProcessData, frameBuffers[0]) + (sizeof(FrameBuffers) * port)) +
                static_cast <UINT32>(offsetof(FrameBuffers, bufferPtr[0]) + (sizeof(FrameBufferPtr) * plane));
            s_frameBufferOffset[port].metadataBufferPtr[plane] =
                static_cast <UINT32>(offsetof(BpsFrameProcessData, frameBuffers[0]) + (sizeof(FrameBuffers) * port)) +
                static_cast <UINT32>(offsetof(FrameBuffers, metadataBufferPtr[0]) + (sizeof(FrameBufferPtr) * plane));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::~BPSNode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSNode::~BPSNode()
{
    Cleanup();

    if (TRUE == IsDeviceAcquired())
    {
        if (CSLInvalidHandle != m_configIOMem.hHandle)
        {
            CSLReleaseBuffer(m_configIOMem.hHandle);
        }

        if (NULL != m_deviceResourceRequest.pDeviceResourceParam)
        {
            CAMX_FREE(m_deviceResourceRequest.pDeviceResourceParam);
            m_deviceResourceRequest.pDeviceResourceParam = NULL;
        }

        IQInterface::IQSettingModuleUninitialize(&m_libInitialData);

        ReleaseDevice();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::TranslateToFirmwarePortId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE VOID BPSNode::TranslateToFirmwarePortId(
    UINT32          portId,
    BPS_IO_IMAGES*  pFirmwarePortId)
{
    CAMX_ASSERT(portId < static_cast<UINT32>(BPS_IO_IMAGES::BPS_IO_IMAGES_MAX));

    *pFirmwarePortId = static_cast<BPS_IO_IMAGES>(portId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSNode* BPSNode::Create(
    const NodeCreateInputData* pCreateInputData,
    NodeCreateOutputData*      pCreateOutputData)
{
    CAMX_UNREFERENCED_PARAM(pCreateOutputData);

    if ((NULL != pCreateInputData) && (NULL != pCreateInputData->pNodeInfo))
    {
        UINT32           propertyCount   = pCreateInputData->pNodeInfo->nodePropertyCount;
        PerNodeProperty* pNodeProperties = pCreateInputData->pNodeInfo->pNodeProperties;

        BPSNode* pNodeObj = CAMX_NEW BPSNode;

        if (NULL != pNodeObj)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "nodePropertyCount %d", propertyCount);

            // There can be multiple BPS instances in a pipeline, each instance can have differnt IQ modules enabled
            for (UINT32 count = 0; count < propertyCount; count++)
            {
                UINT32 nodePropertyId     = pNodeProperties[count].id;
                VOID*  pNodePropertyValue = pNodeProperties[count].pValue;

                switch (nodePropertyId)
                {
                    case NodePropertyProfileId:
                        pNodeObj->m_instanceProperty.profileId = static_cast<BPSProfileId>(
                            atoi(static_cast<const CHAR*>(pNodePropertyValue)));
                        break;
                    case NodePropertyProcessingType: // If MFNR/MFSR is enabled, BPS instance needs to know.
                        pNodeObj->m_instanceProperty.processingType = static_cast<BPSProcessingType>(
                            atoi(static_cast<const CHAR*>(pNodePropertyValue)));
                        break;
                    default:
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unhandled node property Id %d", nodePropertyId);
                        break;
                }
            }

            CAMX_LOG_INFO(CamxLogGroupPProc, "BPS Instance profileId: %d processing: %d",
                          pNodeObj->m_instanceProperty.profileId,
                          pNodeObj->m_instanceProperty.processingType);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to create BPSNode, no memory");
        }

        return pNodeObj;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null input pointer");
        return NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::Destroy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::Destroy()
{
    CAMX_DELETE this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::ProcessingNodeInitialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::ProcessingNodeInitialize(
    const NodeCreateInputData* pCreateInputData,
    NodeCreateOutputData*      pCreateOutputData)
{
    CAMX_UNREFERENCED_PARAM(pCreateInputData);

    CamxResult        result                   = CamxResultSuccess;
    INT32             deviceIndex              = -1;
    UINT              indicesLengthRequired    = 0;
    HwEnvironment*    pHwEnvironment           = HwEnvironment::GetInstance();

    CAMX_ASSERT(BPS == Type());
    CAMX_ASSERT(NULL != pCreateOutputData);

    Titan17xContext* pContext = static_cast<Titan17xContext*>(GetHwContext());
    m_OEMIQSettingEnable      = pContext->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->IsOEMIQSettingEnable;

    m_pChiContext = pCreateInputData->pChiContext;
    if (NULL == pCreateOutputData)
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Output Data.");
    }
    if (CamxResultSuccess == result)
    {
        m_pChiContext = pCreateInputData->pChiContext;

        pCreateOutputData->maxOutputPorts = BPSMaxOutput;
        pCreateOutputData->maxInputPorts  = BPSMaxInput;

        UINT numOutputPorts = 0;
        UINT outputPortId[MaxBufferComposite];

        GetAllOutputPortIds(&numOutputPorts, &outputPortId[0]);

        CAMX_ASSERT(MaxBufferComposite >= numOutputPorts);
        UINT32 groupID = ISPOutputGroupIdMAX;
        for (UINT outputPortIndex = 0; outputPortIndex < numOutputPorts; outputPortIndex++)
        {
            switch (outputPortId[outputPortIndex])
            {
                case BPSOutputPortFull:
                case BPSOutputPortDS4:
                case BPSOutputPortDS16:
                case BPSOutputPortDS64:
                    pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                                                                                   ISPOutputGroupId0;
                    break;

                case BPSOutputPortReg1:
                    pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] = ISPOutputGroupId1;
                    break;

                case BPSOutputPortReg2:
                    pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] = ISPOutputGroupId2;
                    break;

                case BPSOutputPortStatsBG:
                case BPSOutputPortStatsHDRBHist:
                    pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                        ISPOutputGroupId3;
                    break;

                default:
                    pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                        groupID++;
                    break;
            }

            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Index %d, Port ID %d is maped group %d",
                           outputPortIndex,
                           outputPortId[outputPortIndex],
                           pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]]);
        }

        pCreateOutputData->bufferComposite.hasCompositeMask = TRUE;

        // Add device indices
        result = pHwEnvironment->GetDeviceIndices(CSLDeviceTypeICP, &deviceIndex, 1, &indicesLengthRequired);
    }
    if (CamxResultSuccess == result)
    {
        CAMX_ASSERT(indicesLengthRequired == 1);
        result = AddDeviceIndex(deviceIndex);
        m_deviceIndex = deviceIndex;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::PostPipelineCreate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::PostPipelineCreate()
{
    CamxResult      result = CamxResultSuccess;
    ResourceParams  params = { 0 };

    // If BPS tuning-data enable, initialize debug-data writer
    if ((TRUE   == GetStaticSettings()->enableTuningMetadata)   &&
        (0      != GetStaticSettings()->tuningDumpDataSizeBPS))
    {
        // We would disable dual BPS when supporting tuning data
        m_pTuningMetadata = static_cast<BPSTuningMetadata*>(CAMX_CALLOC(sizeof(BPSTuningMetadata)));
        if (NULL == m_pTuningMetadata)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to allocate Tuning metadata.");
            result = CamxResultENoMemory;
        }

        if (CamxResultSuccess == result)
        {
            if (NULL == s_pDebugDataWriter)
            {
                s_pDebugDataWriter = CAMX_NEW TDDebugDataWriter();
                if (NULL == s_pDebugDataWriter)
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to allocate Tuning metadata.");
                    result = CamxResultENoMemory;
                }
            }

            // Keep track of the intance using DebugDataWriter
            if (NULL != s_pDebugDataWriter)
            {
                s_debugDataWriterCounter++;
            }
        }
    }

    // Configure BPS Capability
    if (CamxResultSuccess == result)
    {
        result = ConfigureBPSCapability();
    }

    // Assemble BPS IQ Modules
    if (CamxResultSuccess == result)
    {
        result = CreateBPSIQModules();

        m_tuningData.noOfSelectionParameter = 1;
        m_tuningData.TuningMode[0].mode     = ChiModeType::Default;
    }

    if (CamxResultSuccess == result)
    {
        result = CreateBPSStatsModules();
    }

    if (CamxResultSuccess == result)
    {
        UpdateCommandBufferSize();
    }

    if (CamxResultSuccess == result)
    {
        m_BPSCmdBlobCount = GetPipeline()->GetRequestQueueDepth();
        result = InitializeCmdBufferManagerList(BPSCmdBufferMaxIds);
    }

    if (CamxResultSuccess == result)
    {
        // Command buffer for IQ packet
        params.usageFlags.packet               = 1;
        params.packetParams.maxNumCmdBuffers   = 2;
        // 1 Input and 5 Outputs
        params.packetParams.maxNumIOConfigs    = BPSMaxInput + BPSMaxOutput;
        params.packetParams.enableAddrPatching = 1;
        params.packetParams.maxNumPatches      = BPSMaxPatchAddress;
        params.resourceSize                    = Packet::CalculatePacketSize(&params.packetParams);
        params.memFlags                        = CSLMemFlagKMDAccess | CSLMemFlagUMDAccess;
        // Same number as cmd buffers
        params.poolSize                        = m_BPSCmdBlobCount * params.resourceSize;
        params.alignment                       = CamxPacketAlignmentInBytes;
        params.pDeviceIndices                  = NULL;
        params.numDevices                      = 0;

        result = CreateCmdBufferManager("IQPacketManager", &params, &m_pIQPacketManager);
        if (CamxResultSuccess == result)
        {
            // command buffer for BPS clock and BW
            params                              = { 0 };
            params.resourceSize                 = CmdBufferGenericBlobSizeInBytes;
            params.poolSize                     = m_BPSCmdBlobCount * params.resourceSize;
            params.usageFlags.cmdBuffer         = 1;
            params.cmdParams.type               = CmdType::Generic;
            params.alignment                    = CamxCommandBufferAlignmentInBytes;
            params.cmdParams.enableAddrPatching = 0;
            params.cmdParams.maxNumNestedAddrs  = 0;
            params.memFlags                     = CSLMemFlagUMDAccess | CSLMemFlagKMDAccess;
            params.pDeviceIndices               = DeviceIndices();
            params.numDevices                   = 1;

            result = CreateCmdBufferManager("CmdBufferGenericBlob", &params,
                                            &m_pBPSCmdBufferManager[BPSCmdBufferGenericBlob]);

            if (CamxResultSuccess == result)
            {
                ResourceParams               resourceParams[BPSMaxFWCmdBufferManagers];
                CHAR                         bufferManagerName[BPSMaxFWCmdBufferManagers][MaxStringLength256];
                struct CmdBufferManagerParam createParam[BPSMaxFWCmdBufferManagers];
                UINT32                       numberOfBufferManagers = 0;
                CamxResult                   result                 = CamxResultSuccess;

                if (m_maxCmdBufferSizeBytes[BPSCmdBufferCDMProgram] > 0)
                {
                    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                                        m_maxCmdBufferSizeBytes[BPSCmdBufferCDMProgram],
                                        CmdType::CDMDirect,
                                        CSLMemFlagUMDAccess,
                                        BPSMaxCDMPatchAddress,
                                        DeviceIndices(),
                                        m_BPSCmdBlobCount);
                    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                                      sizeof(CHAR) * MaxStringLength256,
                                      "CBM_%s_%s",
                                      NodeIdentifierString(),
                                      "CmdBufferCDMProgram");
                    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pBPSCmdBufferManager[BPSCmdBufferCDMProgram];

                    numberOfBufferManagers++;
                }

                if (m_maxCmdBufferSizeBytes[BPSCmdBufferDMIHeader] > 0)
                {
                    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                                        m_maxCmdBufferSizeBytes[BPSCmdBufferDMIHeader],
                                        CmdType::CDMDMI,
                                        CSLMemFlagUMDAccess,
                                        BPSMaxDMIPatchAddress,
                                        DeviceIndices(),
                                        m_BPSCmdBlobCount);
                    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                                      sizeof(CHAR) * MaxStringLength256,
                                      "CBM_%s_%s",
                                      NodeIdentifierString(),
                                      "BPSCmdBufferDMIHeader");
                    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pBPSCmdBufferManager[BPSCmdBufferDMIHeader];

                    numberOfBufferManagers++;
                }

                if (0 != numberOfBufferManagers)
                {
                    result = CreateMultiCmdBufferManager(createParam, numberOfBufferManagers);
                }

                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Multi Command Buffer Creation Failed");
                }
            }

        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Creat Cmd Buffer Manager");
        }
    }

    if (CamxResultSuccess != result)
    {
        Cleanup();
    }

    if (CamxResultSuccess == result)
    {
        result = AcquireDevice();
    }

    if (CamxResultSuccess == result)
    {
        // Save required static metadata
        GetStaticMetadata(GetPipeline()->GetCameraId());
        InitializeDefaultHALTags();
    }

    if (CamxResultSuccess == result)
    {
        IQInterface::IQSettingModuleInitialize(&m_libInitialData);
    }

    if (CamxResultSuccess == result)
    {
        UINT           numberOfMappings = 0;
        CSLBufferInfo  bufferInfo = { 0 };
        CSLBufferInfo* pBufferInfo[CSLICPMaxMemoryMapRegions];

        if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferFrameProcess])
        {
            if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferFrameProcess]->GetMergedCSLBufferInfo())
            {
                Utils::Memcpy(&bufferInfo,
                              m_pBPSCmdBufferManager[BPSCmdBufferFrameProcess]->GetMergedCSLBufferInfo(),
                              sizeof(CSLBufferInfo));
                pBufferInfo[numberOfMappings] = &bufferInfo;
                numberOfMappings++;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Unable to get the Merged Buffer info");
                result = CamxResultEFailed;
            }
        }

        if (0 != numberOfMappings)
        {
            result = SendFWCmdRegionInfo(CSLICPGenericBlobCmdBufferMapFWMemRegion,
                                         pBufferInfo,
                                         numberOfMappings);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::HardcodeSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BPSNode::HardcodeSettings(
    ISPInputData* pInputData)
{
    AECFrameControl*  pAECInput       = pInputData->pAECUpdateData;
    AWBFrameControl*  pAWBInput       = pInputData->pAWBUpdateData;
    BGBEConfig*       pBGConfig       = &pInputData->pAWBStatsUpdateData->statsConfig.BGConfig;
    HDRBHistConfig*   pHDRBHistConfig = &pInputData->pAECStatsUpdateData->statsConfig.HDRBHistConfig;
    UINT32            inputWidth      = pInputData->pipelineBPSData.width;
    UINT32            inputHeight     = pInputData->pipelineBPSData.height;

    if (NULL != pAECInput)
    {
        pAECInput->luxIndex                     = 220.0f;
        pAECInput->predictiveGain               = 1.0;
        pAECInput->exposureInfo[0].exposureTime = 1;
        pAECInput->exposureInfo[0].linearGain   = 1.0f;
        pAECInput->exposureInfo[0].sensitivity  = 0.0f;

        for ( UINT i = 0 ; i < ExposureIndexCount ; i++ )
        {
            pAECInput->exposureInfo[i].exposureTime = 1;
            pAECInput->exposureInfo[i].linearGain   = 1.0f;
            pAECInput->exposureInfo[i].sensitivity  = 1.0f;
        }
    }

    if (NULL != pAWBInput)
    {
        pAWBInput->colorTemperature = 0;
        pAWBInput->AWBGains.gGain   = 1.0f;
        pAWBInput->AWBGains.bGain   = 2.0f;
        pAWBInput->AWBGains.rGain   = 1.796875f;
    }

    if (NULL != pBGConfig)
    {
        pBGConfig->channelGainThreshold[ChannelIndexR]  = (1 << BPSPipelineBitWidth) - 1;
        pBGConfig->channelGainThreshold[ChannelIndexGR] = (1 << BPSPipelineBitWidth) - 1;
        pBGConfig->channelGainThreshold[ChannelIndexB]  = (1 << BPSPipelineBitWidth) - 1;
        pBGConfig->channelGainThreshold[ChannelIndexGB] = (1 << BPSPipelineBitWidth) - 1;
        pBGConfig->horizontalNum                        = 64;
        pBGConfig->verticalNum                          = 48;
        pBGConfig->ROI.left                             = 0;
        pBGConfig->ROI.top                              = 0;
        pBGConfig->ROI.width                            = inputWidth - (inputWidth / 10);
        pBGConfig->ROI.height                           = inputHeight - (inputHeight / 10);
        pBGConfig->outputBitDepth                       = 0;
        pBGConfig->outputMode                           = BGBERegular;
        pBGConfig->YStatsWeights[0]                     = static_cast<FLOAT>(0.2f);
        pBGConfig->YStatsWeights[0]                     = static_cast<FLOAT>(0.3);
        pBGConfig->YStatsWeights[0]                     = static_cast<FLOAT>(0.4);
        pBGConfig->greenType                            = Gr;
    }

    // HDR Bhist config
    if (NULL != pHDRBHistConfig)
    {
        pHDRBHistConfig->ROI.top           = 0;
        pHDRBHistConfig->ROI.left          = 0;
        pHDRBHistConfig->ROI.width         = inputWidth;
        pHDRBHistConfig->ROI.height        = inputHeight;
        pHDRBHistConfig->greenChannelInput = HDRBHistSelectGB;
        pHDRBHistConfig->inputFieldSelect  = HDRBHistInputAll;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSNode::Set3AInputData()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::Set3AInputData(
    ISPInputData* pInputData)
{
    VOID*            pPropertyData3A[9] = { 0 };
    UINT             length             = 0;
    ISPStripeConfig* pFrameConfig       = pInputData->pStripeConfig;
    BOOL   updateStats                  = TRUE;

    CAMX_ASSERT(NULL != pFrameConfig);

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "IsRealTime() %d", IsRealTime());

    if (TRUE == IsRealTime())
    {
        static const UINT Properties3A[] =
        {
            PropertyIDAECFrameControl,
            PropertyIDAWBFrameControl,
            PropertyIDAECStatsControl,
            PropertyIDAWBStatsControl,
            PropertyIDISPTintlessBGConfig,
            PropertyIDParsedTintlessBGStatsOutput,
            PropertyIDISPBHistConfig,
            PropertyIDParsedBHistStatsOutput,
            PropertyIDSensorNumberOfLEDs
        };

        UINT64 propertyData3AOffset[9] = { 0 };

        propertyData3AOffset[4] = GetMaximumPipelineDelay();
        propertyData3AOffset[6] = GetMaximumPipelineDelay();

        if (FALSE == GetPipeline()->HasStatsNode())
        {
            updateStats = FALSE;
        }
        else
        {
            length = CAMX_ARRAY_SIZE(Properties3A);
            GetDataList(Properties3A, pPropertyData3A, propertyData3AOffset, length);
        }
    }
    else
    {
        static const UINT Properties3A[] =
        {
            PropertyIDAECFrameControl             | InputMetadataSectionMask,
            PropertyIDAWBFrameControl             | InputMetadataSectionMask,
            PropertyIDAECStatsControl             | InputMetadataSectionMask,
            PropertyIDAWBStatsControl             | InputMetadataSectionMask,
            PropertyIDISPTintlessBGConfig         | InputMetadataSectionMask,
            PropertyIDParsedTintlessBGStatsOutput | InputMetadataSectionMask,
            PropertyIDISPBHistConfig              | InputMetadataSectionMask,
            PropertyIDParsedBHistStatsOutput      | InputMetadataSectionMask,
            PropertyIDSensorNumberOfLEDs          | InputMetadataSectionMask,
        };

        UINT64 propertyData3AOffset[9] = { 0 };

        length = CAMX_ARRAY_SIZE(Properties3A);
        GetDataList(Properties3A, pPropertyData3A, propertyData3AOffset, length);
    }

    if (TRUE == updateStats)
    {
        CAMX_ASSERT((NULL != pPropertyData3A[0]) &&
                    (NULL != pPropertyData3A[1]) &&
                    (NULL != pPropertyData3A[2]) &&
                    (NULL != pPropertyData3A[3]));

        if (NULL != pPropertyData3A[0])
        {
            Utils::Memcpy(pInputData->pAECUpdateData, pPropertyData3A[0], sizeof(AECFrameControl));
        }

        if (NULL != pPropertyData3A[1])
        {
            Utils::Memcpy(pInputData->pAWBUpdateData, pPropertyData3A[1], sizeof(AWBFrameControl));
        }

        if (NULL != pPropertyData3A[2])
        {
            Utils::Memcpy(pInputData->pAECStatsUpdateData, pPropertyData3A[2], sizeof(AECStatsControl));
        }

        if (NULL != pPropertyData3A[3])
        {
            Utils::Memcpy(pInputData->pAWBStatsUpdateData, pPropertyData3A[3], sizeof(AWBStatsControl));
        }

        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "Applying Stats Update. AWB: Gain(R:%f, G: %f, B: %f) CCT(%u) | AEC: Gain(%f) ExpTime(%llu)",
                         pInputData->pAWBUpdateData->AWBGains.rGain,
                         pInputData->pAWBUpdateData->AWBGains.gGain,
                         pInputData->pAWBUpdateData->AWBGains.bGain,
                         pInputData->pAWBUpdateData->colorTemperature,
                         pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].linearGain,
                         pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].exposureTime);

        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "BPS: Processing for ReqId(frameNum) = %llu. AEC Gain = %f, ExposureTime = %llu, luxIndex = %f",
                         pInputData->frameNum,
                         pInputData->pAECUpdateData->exposureInfo[0].linearGain,
                         pInputData->pAECUpdateData->exposureInfo[0].exposureTime,
                         pInputData->pAECUpdateData->luxIndex);


        UINT64 requestIdOffsetFromLastFlush = GetRequestIdOffsetFromLastFlush(pInputData->frameNum);

        if ((GetMaximumPipelineDelay() < requestIdOffsetFromLastFlush) || (FALSE == IsRealTime()))
        {
            CAMX_ASSERT(NULL != pPropertyData3A[4]);
            if (NULL != pPropertyData3A[4])
            {
                Utils::Memcpy(&(pFrameConfig->statsDataForISP.tintlessBGConfig),
                    &(reinterpret_cast<PropertyISPTintlessBG*>(pPropertyData3A[4])->statsConfig),
                    sizeof(pFrameConfig->statsDataForISP.tintlessBGConfig));
            }

            CAMX_ASSERT(NULL != pPropertyData3A[5]);
            if (NULL != pPropertyData3A[5])
            {
                pFrameConfig->statsDataForISP.pParsedTintlessBGStats =
                    *(reinterpret_cast<ParsedTintlessBGStatsOutput**>(pPropertyData3A[5]));
                if (NULL != pFrameConfig->statsDataForISP.pParsedTintlessBGStats)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "TLBG Config %d %d | %d TLBG stats %x %x %x ",
                                     pFrameConfig->statsDataForISP.tintlessBGConfig.tintlessBGConfig.ROI.height,
                                     pFrameConfig->statsDataForISP.tintlessBGConfig.tintlessBGConfig.ROI.width,
                                     pFrameConfig->statsDataForISP.pParsedTintlessBGStats->m_numOfRegions,
                                     pFrameConfig->statsDataForISP.pParsedTintlessBGStats->GetTintlessBGStatsInfo(0),
                                     pFrameConfig->statsDataForISP.pParsedTintlessBGStats->GetTintlessBGStatsInfo(0),
                                     pFrameConfig->statsDataForISP.pParsedTintlessBGStats->GetTintlessBGStatsInfo(0));
                }
            }

            CAMX_ASSERT(NULL != pPropertyData3A[6]);
            if (NULL != pPropertyData3A[6])
            {
                Utils::Memcpy(&(pFrameConfig->statsDataForISP.BHistConfig),
                    &(reinterpret_cast<PropertyISPBHistStats*>(pPropertyData3A[6])->statsConfig),
                    sizeof(pFrameConfig->statsDataForISP.BHistConfig));
            }

            CAMX_ASSERT(NULL != pPropertyData3A[7]);
            if (NULL != pPropertyData3A[7])
            {
                pFrameConfig->statsDataForISP.pParsedBHISTStats =
                    *(reinterpret_cast<ParsedBHistStatsOutput**>(pPropertyData3A[7]));
                if (NULL != pFrameConfig->statsDataForISP.pParsedBHISTStats)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "BHIST Config %d %d %d | %d %d %d BHIST stats %x %x %x ",
                                     pFrameConfig->statsDataForISP.BHistConfig.BHistConfig.channel,
                                     pFrameConfig->statsDataForISP.BHistConfig.BHistConfig.uniform,
                                     pFrameConfig->statsDataForISP.BHistConfig.numBins,
                                     pFrameConfig->statsDataForISP.pParsedBHISTStats->channelType,
                                     pFrameConfig->statsDataForISP.pParsedBHISTStats->uniform,
                                     pFrameConfig->statsDataForISP.pParsedBHISTStats->numBins,
                                     pFrameConfig->statsDataForISP.pParsedBHISTStats->BHistogramStats[0],
                                     pFrameConfig->statsDataForISP.pParsedBHISTStats->BHistogramStats[1],
                                     pFrameConfig->statsDataForISP.pParsedBHISTStats->BHistogramStats[2]);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "BHist data is NULL");
                }
            }
        }

        if (NULL != pPropertyData3A[8])
        {
            pInputData->numberOfLED = *reinterpret_cast<UINT16*>(pPropertyData3A[8]);
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Number of led %d", pInputData->numberOfLED);
        }

    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetSensorModeData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SensorMode* BPSNode::GetSensorModeData(
    BOOL sensorConnected)
{
    CamxResult        result      = CamxResultSuccess;
    const SensorMode* pSensorMode = NULL;

    if (FALSE == sensorConnected)
    {
        UINT       metaTag   = 0;
        UINT       modeIndex = 0;

        result = VendorTagManager::QueryVendorTagLocation("com.qti.sensorbps", "mode_index", &metaTag);

        metaTag |= InputMetadataSectionMask;

        /// @todo (CAMX-1015) Can optimize by keeping an array of SensorMode* per requestId
        UINT              propertiesBPS[] = { PropertyIDSensorCurrentMode, PropertyIDUsecaseSensorModes, metaTag };
        static const UINT Length          = CAMX_ARRAY_SIZE(propertiesBPS);
        VOID*             pData[Length]   = { 0 };
        UINT64            offsets[Length] = { 0, 0, 0 };

        GetDataList(propertiesBPS, pData, offsets, Length);

        if (NULL != pData[2])
        {
            // Sensor not connected so pull from the input metadata
            modeIndex = *reinterpret_cast<UINT*>(pData[2]);
            CAMX_LOG_INFO(CamxLogGroupPProc, "BPS using vendor tag com.qti.sensorbps mode index");
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupPProc, "Mode index vendor tag not provided! Using default 0!");
        }

        CAMX_ASSERT_MESSAGE(NULL != pData[1], "Usecase pool did not contain sensor modes. Going to fault");
        if (NULL != pData[1])
        {
            pSensorMode = &(reinterpret_cast<UsecaseSensorModes*>(pData[1])->allModes[modeIndex]);
        }
    }
    else
    {
        Node::GetSensorModeData(&pSensorMode);
        CAMX_LOG_INFO(CamxLogGroupPProc, "BPS using pipeline mode index");
    }

    return pSensorMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetPDAFInformation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::GetPDAFInformation(
    SensorPDAFInfo* pSensorPDAFInfo)
{
    UINT   sensorPDAFInfoTag[1] = { 0 };
    VOID*  pDataOutput[]        = { 0 };
    UINT64 PDAFdataOffset[1]    = { 0 };

    if (TRUE == IsRealTime())
    {
        sensorPDAFInfoTag[0] = { PropertyIDSensorPDAFInfo };
    }
    else
    {
        sensorPDAFInfoTag[0] = { PropertyIDSensorPDAFInfo | InputMetadataSectionMask };
    }

    GetDataList(sensorPDAFInfoTag, pDataOutput, PDAFdataOffset, 1);
    if (NULL != pDataOutput[0])
    {
        Utils::Memcpy(pSensorPDAFInfo, pDataOutput[0], sizeof(SensorPDAFInfo));
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "PDAF not enabled");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::IsSensorModeFormatBayer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSNode::IsSensorModeFormatBayer(
    PixelFormat format)
{
    BOOL isBayer = FALSE;

    if ((PixelFormat::BayerBGGR == format) ||
        (PixelFormat::BayerGBRG == format) ||
        (PixelFormat::BayerGRBG == format) ||
        (PixelFormat::BayerRGGB == format))
    {
        isBayer = TRUE;
    }

    return isBayer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::IsSensorModeFormatMono
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSNode::IsSensorModeFormatMono(
    PixelFormat format)
{
    BOOL isMono = FALSE;

    if (PixelFormat::Monochrome == format ||
        PixelFormat::YUVFormatY == format)
    {
        isMono = TRUE;
    }

    return isMono;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::ReadSensorDigitalGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT BPSNode::ReadSensorDigitalGain(
    BOOL sensorConnected)
{
    FLOAT      dGain   = 1.0;
    CamxResult result  = CamxResultSuccess;
    UINT       metaTag = 0;

    result   = VendorTagManager::QueryVendorTagLocation("com.qti.sensorbps", "gain", &metaTag);
    metaTag |= InputMetadataSectionMask;

    UINT              propertiesBPS[] = { PropertyIDPostSensorGainId, metaTag };
    static const UINT Length          = CAMX_ARRAY_SIZE(propertiesBPS);
    VOID*             pData[Length]   = { 0 };
    UINT64            offsets[Length] = { 0, 0 };

    GetDataList(propertiesBPS, pData, offsets, Length);

    if ((NULL != pData[1]) && ((FALSE == sensorConnected) ||(GetPipeline()->HasStatsNode() == FALSE)))
    {
        dGain = *reinterpret_cast<FLOAT*>(pData[1]);
        CAMX_LOG_INFO(CamxLogGroupPProc, "BPS using vendor tag com.qti.sensorbps gain");
    }
    else if ( NULL != pData[0])
    {
        dGain = *reinterpret_cast<FLOAT*>(pData[0]);
        CAMX_LOG_INFO(CamxLogGroupPProc, "BPS using pipeline gain");
    }

    return dGain;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::SendFWCmdRegionInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::SendFWCmdRegionInfo(
    UINT32          opcode,
    CSLBufferInfo** ppBufferInfo,
    UINT32          numberOfMappings)
{
    CamxResult            result          = CamxResultSuccess;
    Packet*               pPacket         = NULL;
    CmdBuffer*            pCmdBuffer      = NULL;
    CSLICPMemoryMapUpdate memoryMapUpdate = { 0 };
    CSLBufferInfo*        pBufferInfo     = NULL;

    pPacket = GetPacket(m_pIQPacketManager);

    if ((NULL != pPacket) && (NULL != ppBufferInfo) && (numberOfMappings <= CSLICPMaxMemoryMapRegions))
    {
        pPacket->SetOpcode(CSLDeviceType::CSLDeviceTypeICP, CSLPacketOpcodesBPSMemoryMapUpdate);
        pCmdBuffer = GetCmdBuffer(m_pBPSCmdBufferManager[BPSCmdBufferGenericBlob]);
        if (NULL != pCmdBuffer)
        {
            memoryMapUpdate.version               = CSLICPMemoryMapVersion0;
            memoryMapUpdate.numberOfMappings      = numberOfMappings;
            for (UINT index = 0; index < numberOfMappings; index++)
            {
                pBufferInfo = ppBufferInfo[index];
                if (NULL != pBufferInfo)
                {
                    memoryMapUpdate.regionInfo[index].hHandle = pBufferInfo->hHandle;
                    memoryMapUpdate.regionInfo[index].flags   = pBufferInfo->flags;
                    memoryMapUpdate.regionInfo[index].offset  = pBufferInfo->offset;
                    memoryMapUpdate.regionInfo[index].size    = pBufferInfo->size;
                }
                else
                {
                    result = CamxResultEInvalidArg;
                }
            }
            if (CamxResultSuccess == result)
            {
                PacketBuilder::WriteGenericBlobData(pCmdBuffer,
                                                    opcode,
                                                    sizeof(CSLICPMemoryMapUpdate),
                                                    reinterpret_cast<BYTE*>(&memoryMapUpdate));
                pPacket->CommitPacket();
                pCmdBuffer->SetMetadata(static_cast<UINT32>(CSLICPCmdBufferIdGenericBlob));
                result = pPacket->AddCmdBufferReference(pCmdBuffer, NULL);
                result = GetHwContext()->Submit(GetCSLSession(), m_hDevice, pPacket);
            }
        }
    }

    if (NULL != pCmdBuffer)
    {
        m_pBPSCmdBufferManager[BPSCmdBufferGenericBlob]->Recycle(reinterpret_cast<PacketResource*>(pCmdBuffer));
    }
    if (NULL != pPacket)
    {
        m_pIQPacketManager->Recycle(reinterpret_cast<PacketResource*>(pPacket));
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::ExecuteProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::ExecuteProcessRequest(
    ExecuteProcessRequestData* pExecuteProcessRequestData)
{
    CamxResult                  result                            = CamxResultSuccess;
    Packet*                     pIQPacket                         = NULL;
    CmdBuffer*                  pBPSCmdBuffer[BPSCmdBufferMaxIds] = { NULL };
    NodeProcessRequestData*     pNodeRequestData                  = pExecuteProcessRequestData->pNodeProcessRequestData;
    PerRequestActivePorts*      pPerRequestPorts                  = pExecuteProcessRequestData->pEnabledPortsInfo;
    UINT64                      requestId                         = pNodeRequestData->pCaptureRequest->requestId;
    ISPInputData                moduleInput                       = { 0 };
    ISPInternalData             calculatedData                    = { 0 };
    BpsFrameProcess*            pFrameProcess                     = NULL;
    BpsFrameProcessData*        pFrameProcessData                 = NULL;
    const ImageFormat*          pImageFormat                      = NULL;
    const SensorMode*           pSensorModeData                   = NULL; ///< Sensor related data for the current mode
    BpsIQSettings*              pBPSIQsettings                    = NULL;
    VOID*                       pOEMSetting                       = NULL;
    UINT32                      metaTag                           = 0;
    BOOL                        sensorConnected                   = FALSE;
    BOOL                        statsConnected                    = FALSE;
    BOOL                        hasDependency                     = FALSE;
    INT                         sequenceNumber                    = pNodeRequestData->processSequenceId;
    UINT32                      cameraId                          = GetPipeline()->GetCameraId();
    BOOL                        isMasterCamera                    = TRUE;
    BPSTuningMetadata*          pTuningMetadata                   = NULL;
    TuningDataManager*          pCurrentTuningDataManager         = NULL;
    AECFrameControl             AECUpdateData                     = {};
    AWBFrameControl             AWBUpdateData                     = {};
    AWBStatsControl             AWBStatsUpdateData                = {};
    AECStatsControl             AECStatsUpdateData                = {};
    AFStatsControl              AFStatsUpdateData                 = {};
    ISPStripeConfig             frameConfig                       = {};
    CSLICPClockBandwidthRequest ICPClockBandwidthRequest          = {};

    // Cannot have HFR request with BPS node
    CAMX_ASSERT(1 == pNodeRequestData->pCaptureRequest->numBatchedFrames);

    // Need to determine if sensor node is hooked up to set the appropriate dependencies
    if (TRUE == IsRealTime())
    {
        sensorConnected = TRUE;
    }
    if (TRUE == GetPipeline()->HasStatsNode())
    {
        statsConnected = TRUE;
    }

    if (0 == sequenceNumber)
    {
        SetDependencies(pExecuteProcessRequestData, sensorConnected, statsConnected);
    }

    if (pNodeRequestData->dependencyInfo[0].dependencyFlags.dependencyFlagsMask)
    {
        hasDependency = TRUE;
    }

    // If the sequenceRequest is 1, it means that we were called from the DRQ w/
    // all deps satisified.
    if ((CamxResultSuccess == result) &&
        ((1 == sequenceNumber) || ((0 == sequenceNumber) && (sensorConnected == FALSE) && (FALSE == hasDependency))))
    {
        UINT32 numberOfCamerasRunning;
        UINT32 currentCameraId;
        BOOL   isMultiCameraUsecase;

        GetMultiCameraInfo(&isMultiCameraUsecase, &numberOfCamerasRunning, &currentCameraId, &isMasterCamera);
        if (TRUE == isMultiCameraUsecase)
        {
            cameraId = currentCameraId;
            GetStaticMetadata(cameraId);
        }
        pTuningMetadata = (TRUE == isMasterCamera) ? m_pTuningMetadata : NULL;

        pCurrentTuningDataManager = GetTuningDataManagerWithCameraId(cameraId);

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "instance=%p, cameraId = %d,currentCameraId=%d, isMasterCamera = %d",
                         this, cameraId, currentCameraId, isMasterCamera);

        if (TRUE == m_OEMIQSettingEnable)
        {
            result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.iqsettings", "OEMBPSIQSetting", &metaTag);
            CAMX_ASSERT(CamxResultSuccess == result);
            const UINT OEMProperty[1] = { metaTag | InputMetadataSectionMask };
            VOID* pOEMData[1] = { 0 };
            UINT64 OEMDataOffset[1] = { 0 };
            GetDataList(OEMProperty, pOEMData, OEMDataOffset, 1);
            pOEMSetting = pOEMData[0];
        }
        else
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "BPS continuing with Non-OEM settings");
        }

        GetTintlessStatus(&moduleInput);

        // Sensor begins with publishing the selected mode into the "FirstValidRequestId" slot in the perFrame metadata pool
        pSensorModeData = GetSensorModeData(sensorConnected);
        CAMX_ASSERT(pSensorModeData != NULL);
        if (NULL == pSensorModeData)
        {
            result = CamxResultEFailed;
            CAMX_LOG_ERROR(CamxLogGroupISP, "Sensor Mode Data is NULL.");
        }

        CAMX_ASSERT(m_pOTPData != NULL);
        moduleInput.pOTPData       = m_pOTPData;

        // Get CmdBuffer for request
        pIQPacket = GetPacketForRequest(requestId, m_pIQPacketManager);

        if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferGenericBlob])
        {
            pBPSCmdBuffer[BPSCmdBufferGenericBlob] =
                GetCmdBufferForRequest(requestId, m_pBPSCmdBufferManager[BPSCmdBufferGenericBlob]);
        }

        if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferFrameProcess])
        {
            pBPSCmdBuffer[BPSCmdBufferFrameProcess] =
                GetCmdBufferForRequest(requestId, m_pBPSCmdBufferManager[BPSCmdBufferFrameProcess]);
        }

        if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferIQSettings])
        {
            pBPSCmdBuffer[BPSCmdBufferIQSettings] =
                GetCmdBufferForRequest(requestId, m_pBPSCmdBufferManager[BPSCmdBufferIQSettings]);
        }

        if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferCDMProgram])
        {
            pBPSCmdBuffer[BPSCmdBufferCDMProgram] =
                GetCmdBufferForRequest(requestId, m_pBPSCmdBufferManager[BPSCmdBufferCDMProgram]);
        }

        if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferDMIHeader])
        {
            pBPSCmdBuffer[BPSCmdBufferDMIHeader] =
                GetCmdBufferForRequest(requestId, m_pBPSCmdBufferManager[BPSCmdBufferDMIHeader]);
        }

        if ((NULL == pIQPacket)                                                         ||
            (NULL == pBPSCmdBuffer[BPSCmdBufferFrameProcess])                           ||
            ((m_maxLUT > 0) && (NULL == pBPSCmdBuffer[BPSCmdBufferDMIHeader]))          ||
            (NULL == pBPSCmdBuffer[BPSCmdBufferCDMProgram])                             ||
            (NULL == pBPSCmdBuffer[BPSCmdBufferIQSettings]))
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "Null IQPacket or CmdBuffer %x, %x, %x, %x, %x",
                          pIQPacket,
                          pBPSCmdBuffer[BPSCmdBufferFrameProcess],
                          pBPSCmdBuffer[BPSCmdBufferDMIHeader],
                          pBPSCmdBuffer[BPSCmdBufferCDMProgram],
                          pBPSCmdBuffer[BPSCmdBufferIQSettings]);
            result = CamxResultENoMemory;
        }

        if (CamxResultSuccess == result)
        {
            pFrameProcess = reinterpret_cast<BpsFrameProcess*>(
                                pBPSCmdBuffer[BPSCmdBufferFrameProcess]->BeginCommands(
                                    m_maxCmdBufferSizeBytes[BPSCmdBufferFrameProcess] / sizeof(UINT32)));

            CAMX_ASSERT(NULL != pFrameProcess);
            if (NULL == pFrameProcess)
            {
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Frame Process Data.");
            }
        }
        if (CamxResultSuccess == result)
        {
            pFrameProcess->userArg          = m_hDevice;
            pFrameProcessData               = &pFrameProcess->cmdData;
            pFrameProcessData->requestId    = static_cast<UINT32>(requestId);
            pBPSIQsettings = reinterpret_cast<BpsIQSettings*>(
                                 pBPSCmdBuffer[BPSCmdBufferIQSettings]->BeginCommands(
                                     m_maxCmdBufferSizeBytes[BPSCmdBufferIQSettings] / sizeof(UINT32)));

            InitializeBPSIQSettings(pBPSIQsettings);

            // BPS has only one input port so GetInputPortImageFormat is called with zero.
            pImageFormat = GetInputPortImageFormat(0);

            if (NULL != pImageFormat)
            {
                const SensorMode* pSensorModeRes0 = NULL;

                // Setup the Input data for IQ Parameter
                moduleInput.pOEMIQSetting                        = pOEMSetting;
                moduleInput.frameNum                             = requestId;
                moduleInput.pipelineBPSData.pIQSettings          = pBPSIQsettings;
                moduleInput.pipelineBPSData.ppBPSCmdBuffer       = pBPSCmdBuffer;
                moduleInput.pipelineBPSData.width                = pImageFormat->width;
                moduleInput.pipelineBPSData.height               = pImageFormat->height;
                moduleInput.pipelineBPSData.pBPSPathEnabled      = &m_BPSPathEnabled[0];
                moduleInput.pHwContext                           = GetHwContext();
                moduleInput.pTuningDataManager                   = GetTuningDataManagerWithCameraId(cameraId);
                moduleInput.pHALTagsData                         = &m_HALTagsData;
                moduleInput.pCmdBuffer                           = pBPSCmdBuffer[BPSCmdBufferCDMProgram];
                moduleInput.pDMICmdBuffer                        = pBPSCmdBuffer[BPSCmdBufferDMIHeader];
                moduleInput.pCalculatedData                      = &calculatedData;
                moduleInput.pBPSTuningMetadata                   = pTuningMetadata;
                moduleInput.pAECUpdateData                       = &AECUpdateData;
                moduleInput.pAWBUpdateData                       = &AWBUpdateData;
                moduleInput.pAWBStatsUpdateData                  = &AWBStatsUpdateData;
                moduleInput.pAECStatsUpdateData                  = &AECStatsUpdateData;
                moduleInput.pAFStatsUpdateData                   = &AFStatsUpdateData;
                moduleInput.pStripeConfig                        = &frameConfig;
                moduleInput.registerBETEn                        = FALSE;
                moduleInput.maximumPipelineDelay                 = GetMaximumPipelineDelay();

                // Set the sensor data information for ISP input
                moduleInput.sensorData.CAMIFCrop.firstPixel      = pSensorModeData->cropInfo.firstPixel;
                moduleInput.sensorData.CAMIFCrop.firstLine       = pSensorModeData->cropInfo.firstLine;
                moduleInput.sensorData.CAMIFCrop.lastPixel       = pSensorModeData->cropInfo.lastPixel;
                moduleInput.sensorData.CAMIFCrop.lastLine        = pSensorModeData->cropInfo.lastLine;
                moduleInput.sensorData.format                    = pSensorModeData->format;
                moduleInput.sensorData.isBayer                   = IsSensorModeFormatBayer(moduleInput.sensorData.format);
                moduleInput.sensorData.isMono                    = IsSensorModeFormatMono(moduleInput.sensorData.format);
                moduleInput.sensorData.sensorOut.width           = pSensorModeData->resolution.outputWidth;
                moduleInput.sensorData.sensorOut.height          = pSensorModeData->resolution.outputHeight;
                moduleInput.sensorData.sensorScalingFactor       = pSensorModeData->downScaleFactor;
                moduleInput.sensorData.sensorBinningFactor       = static_cast<FLOAT>(pSensorModeData->binningTypeH);
                moduleInput.sensorData.sensorOut.offsetX         = pSensorModeData->offset.xStart;
                moduleInput.sensorData.sensorOut.offsetY         = pSensorModeData->offset.yStart;
                moduleInput.sensorData.dGain                     = ReadSensorDigitalGain(sensorConnected);
                moduleInput.sensorID                             = cameraId;
                moduleInput.sensorData.ZZHDRColorPattern         = pSensorModeData->ZZHDRColorPattern;
                moduleInput.sensorData.ZZHDRFirstExposurePattern = pSensorModeData->ZZHDRFirstExposurePattern;
                moduleInput.useHardcodedRegisterValues           = CheckToUseHardcodedRegValues(moduleInput.pHwContext);
                moduleInput.sensorBitWidth                       = pSensorModeData->streamConfig[0].bitWidth;

                HardcodeSettings(&moduleInput);

                for (UINT32 i = 0; i < pSensorModeData->capabilityCount; i++)
                {
                    if (pSensorModeData->capability[i] == SensorCapability::IHDR)
                    {
                        moduleInput.sensorData.isIHDR        = TRUE;
                        CAMX_LOG_VERBOSE(CamxLogGroupISP, "sensor I-HDR mode");
                        break;
                    }
                }

                GetSensorModeRes0Data(&pSensorModeRes0);
                if (NULL != pSensorModeRes0)
                {
                    moduleInput.sensorData.fullResolutionWidth   = pSensorModeRes0->resolution.outputWidth;
                    moduleInput.sensorData.fullResolutionHeight  = pSensorModeRes0->resolution.outputHeight;
                }
                else
                {
                    moduleInput.sensorData.fullResolutionWidth   = pSensorModeData->resolution.outputWidth;
                    moduleInput.sensorData.fullResolutionHeight  = pSensorModeData->resolution.outputHeight;
                }
                // Get Sensor PDAF information
                GetPDAFInformation(&moduleInput.sensorData.sensorPDAFInfo);

                // Update data from 3A
                Set3AInputData(&moduleInput);

                if (TRUE == m_OEMStatsSettingEnable)
                {
                    GetOEMStatsConfig(&moduleInput);
                }

                if (TRUE == m_libInitialData.isSucceed)
                {
                    moduleInput.pLibInitialData = m_libInitialData.pLibData;
                }

                if ((TRUE == GetStaticSettings()->offlineImageDumpOnly) && (TRUE != IsRealTime()))
                {
                    CHAR  dumpFilename[256];
                    FILE* pFile = NULL;
                    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
                    pFile = CamX::OsUtils::FOpen(dumpFilename, "w");
                    CamX::OsUtils::FPrintF(pFile, "******** BPS REGISTER DUMP FOR BET ********************* \n");
                    CamX::OsUtils::FClose(pFile);
                    moduleInput.dumpRegConfig                        = 0x00200000;
                }
                else
                {
                    moduleInput.dumpRegConfig                        = GetStaticSettings()->logVerboseMask;
                }

                if (m_pPreTuningDataManager == pCurrentTuningDataManager)
                {
                    moduleInput.tuningModeChanged = ISPIQModule::IsTuningModeDataChanged(
                        pExecuteProcessRequestData->pTuningModeData,
                        &m_tuningData);
                }
                else
                {
                    moduleInput.tuningModeChanged = TRUE;
                    m_pPreTuningDataManager = pCurrentTuningDataManager;
                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "TuningDataManager pointer is updated");
                }

                // Needed to have different tuning data for different instances of a node within same pipeline
                //
                // Also, cache tuning mode selector data for comparison for next frame, to help
                // optimize tuning data (tree) search in the IQ modules
                if (TRUE == moduleInput.tuningModeChanged)
                {
                    Utils::Memcpy(&m_tuningData, pExecuteProcessRequestData->pTuningModeData, sizeof(ChiTuningModeParameter));

                    if (((BPSProfileId::BPSProfileIdBlendPost  == m_instanceProperty.profileId)        &&
                         (BPSProcessingType::BPSProcessingMFNR == m_instanceProperty.processingType))  ||
                        ((BPSProfileId::BPSProfileIdBlendPost  == m_instanceProperty.profileId)        &&
                         (BPSProcessingType::BPSProcessingMFSR == m_instanceProperty.processingType)))
                    {
                        m_tuningData.TuningMode[static_cast<UINT32>(ModeType::Feature2)].subMode.feature2 =
                            ChiModeFeature2SubModeType::MFNRBlend;
                    }
                }

                // Now refer to the updated tuning mode selector data
                moduleInput.pTuningData = &m_tuningData;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Null ImageFormat for Input");
                result = CamxResultENoMemory;
            }
        }

        if (CamxResultSuccess == result)
        {
            result = GetMetadataTags(&moduleInput);
        }

        if (CamxResultSuccess == result)
        {
            result = GetFaceROI(&moduleInput);
        }

        if (CamxResultSuccess == result)
        {
            // Update the BPS IQ settings. these are settings for blocks that do not have a module implementation.
            result = FillIQSetting(&moduleInput, pBPSIQsettings);
        }

        SetScaleRatios(&moduleInput, pBPSCmdBuffer[BPSCmdBufferGenericBlob]);

        if (CamxResultSuccess == result)
        {
            // API invoking the IQ modules to update the register/ DMI settings
            result = ProgramIQConfig(&moduleInput);
            UpdateLUTData(pBPSIQsettings);
        }

        if (CamxResultSuccess == result)
        {
            pIQPacket->SetRequestId(GetCSLSyncId(requestId));
            pIQPacket->SetOpcode(CSLDeviceType::CSLDeviceTypeICP, CSLPacketOpcodesBPSUpdate);

            PerRequestInputPortInfo* pInputPort = &pPerRequestPorts->pInputPorts[0];

            if ((NULL != pInputPort) && (NULL != pInputPort->pImageBuffer))
            {
                UINT32   numFences   = (NULL == pInputPort->phFence) ? 0 : 1;
                CSLFence hInputFence = 0;

                // IO config for Input
                result = pIQPacket->AddIOConfig(pInputPort->pImageBuffer,
                                                pInputPort->portId,
                                                CSLIODirection::CSLIODirectionInput,
                                                pInputPort->phFence,
                                                numFences,
                                                NULL,
                                                NULL,
                                                0);

                hInputFence = ((NULL != pInputPort->phFence) ? *(pInputPort->phFence) : CSLInvalidHandle);

                if (0 == numFences)
                {
                    CAMX_LOG_INFO(CamxLogGroupPProc,
                                  "node %s reporting Input config, portId=%d, request=%llu",
                                  NodeIdentifierString(),
                                  pInputPort->portId,
                                  pNodeRequestData->pCaptureRequest->requestId);
                }
                else
                {
                    CAMX_LOG_INFO(CamxLogGroupPProc,
                                  "node %s, reporting Input config, portId=%d, hFence=%d, request=%llu",
                                  NodeIdentifierString(),
                                  pInputPort->portId,
                                  hInputFence,
                                  pNodeRequestData->pCaptureRequest->requestId);
                }

                if (CamxResultSuccess == result)
                {
                    // Update the frame buffer data for input buffer
                    result = FillFrameBufferData(pBPSCmdBuffer[BPSCmdBufferFrameProcess],
                                                 pInputPort->pImageBuffer,
                                                 pInputPort->portId);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Add input IO config failed");

                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Input Port/Image Buffer is Null ");
                result = CamxResultEInvalidArg;
            }
        }

        if (CamxResultSuccess == result)
        {
            for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
            {
                PerRequestOutputPortInfo* pOutputPort = &pPerRequestPorts->pOutputPorts[i];

                // Cannot have HFR with a BPS request
                CAMX_ASSERT((NULL != pOutputPort) && (1 == pOutputPort->numOutputBuffers));

                // Cannot have HFR with a BPS request so only the first ImageBuffer (index 0) will ever be valid
                ImageBuffer* pImageBuffer = pOutputPort->ppImageBuffer[0];

                if (NULL != pImageBuffer)
                {
                    // Add IO config for output buffer
                    result = pIQPacket->AddIOConfig(pImageBuffer,
                                                    pOutputPort->portId,
                                                    CSLIODirection::CSLIODirectionOutput,
                                                    pOutputPort->phFence,
                                                    1,
                                                    NULL,
                                                    NULL,
                                                    0);

                    CAMX_ASSERT(NULL != pOutputPort->phFence);

                    CAMX_LOG_INFO(CamxLogGroupPProc,
                                  "node %s, reporting I/O config, portId=%d, imgBuf=0x%x, hFence=%d, request=%llu",
                                  NodeIdentifierString(),
                                  pOutputPort->portId,
                                  pImageBuffer,
                                  *(pOutputPort->phFence),
                                  pNodeRequestData->pCaptureRequest->requestId);

                    if (CamxResultSuccess == result)
                    {
                        // Fill frame  buffer data for output buffer
                        result = FillFrameBufferData(pBPSCmdBuffer[BPSCmdBufferFrameProcess],
                                                     pImageBuffer,
                                                     pOutputPort->portId);
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "Add output IO config failed");

                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Output Port/Image is Null ");
                    result = CamxResultEInvalidArg;
                }
                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Output Port: Add IO config failed");
                    break;
                }
            }
        }

        if (CamxResultSuccess == result)
        {
            result = FillFrameCDMArray(pBPSCmdBuffer, pFrameProcessData);
        }

        // Following code can only be called after SetScaleRatios(...)
        if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferStriping])
        {
            pBPSCmdBuffer[BPSCmdBufferStriping] =
                GetCmdBufferForRequest(requestId, m_pBPSCmdBufferManager[BPSCmdBufferStriping]);
        }

        if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferBLMemory])
        {
            pBPSCmdBuffer[BPSCmdBufferBLMemory] =
                GetCmdBufferForRequest(requestId, m_pBPSCmdBufferManager[BPSCmdBufferBLMemory]);
        }

        if ((CamxResultSuccess == result) && (TRUE == m_capability.swStriping))
        {
            result = FillStripingParams(pFrameProcessData, pBPSIQsettings, pBPSCmdBuffer, &ICPClockBandwidthRequest);
        }
        if (CamxResultSuccess == result)
        {
            result = PatchBLMemoryBuffer(pFrameProcessData, pBPSCmdBuffer);
        }
        if (CamxResultSuccess == result)
        {
            CheckAndUpdateClockBW(pBPSCmdBuffer[BPSCmdBufferGenericBlob], pExecuteProcessRequestData,
                                  &ICPClockBandwidthRequest);
        }
        if (CamxResultSuccess == result)
        {
            result = CommitAllCommandBuffers(pBPSCmdBuffer);
        }
        if (CamxResultSuccess == result)
        {
            result = pIQPacket->CommitPacket();
        }
        if (CamxResultSuccess == result)
        {
            result = pIQPacket->AddCmdBufferReference(pBPSCmdBuffer[BPSCmdBufferFrameProcess], NULL);
        }
        if (CamxResultSuccess == result)
        {
            if (pBPSCmdBuffer[BPSCmdBufferGenericBlob]->GetResourceUsedDwords() > 0)
            {
                pBPSCmdBuffer[BPSCmdBufferGenericBlob]->SetMetadata(static_cast<UINT32>(CSLICPCmdBufferIdGenericBlob));
                result = pIQPacket->AddCmdBufferReference(pBPSCmdBuffer[BPSCmdBufferGenericBlob], NULL);
            }
        }
        if ((CamxResultSuccess == result) && (BPSProfileId::BPSProfileIdHNR != m_instanceProperty.profileId))
        {
            result = PostMetadata(&moduleInput);
        }

        if ((NULL != pTuningMetadata) && (TRUE == isMasterCamera) && (CamxResultSuccess == result))
        {
            // Only use debug data on the master camera
            DumpTuningMetadata(&moduleInput);
        }

        if (CamxResultSuccess == result)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Submit packets for BPS:%d request %llu",
                             InstanceID(),
                             requestId);

            result = GetHwContext()->Submit(GetCSLSession(), m_hDevice, pIQPacket);

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT(CamxResultSuccess == result);
                CAMX_LOG_ERROR(CamxLogGroupPProc, "BPS:%d Submit packets with requestId = %llu failed %d",
                               InstanceID(),
                               requestId,
                               result);
            }
        }

        if (CamxResultSuccess == result)
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "BPS:%d Submitted packet(s) with requestId = %llu successfully",
                          InstanceID(),
                          requestId);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetFaceROI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::GetFaceROI(
    ISPInputData* pInputData)
{
    CamxResult   result                    = CamxResultSuccess;
    UINT32       metaTagFDRoi              = 0;
    BOOL         isFDPostingResultsEnabled = FALSE;

    GetFDPerFrameMetaDataSettings(0, &isFDPostingResultsEnabled, NULL);
    if (TRUE == isFDPostingResultsEnabled)
    {
        result = VendorTagManager::QueryVendorTagLocation(VendorTagSectionOEMFDResults,
                                                          VendorTagNameOEMFDResults, &metaTagFDRoi);

        if (CamxResultSuccess == result)
        {
            UINT              GetProps[]              = { metaTagFDRoi };
            static const UINT GetPropsLength          = CAMX_ARRAY_SIZE(GetProps);
            VOID*             pData[GetPropsLength]   = { 0 };
            UINT64            offsets[GetPropsLength] = { 0 };

            if (FALSE == IsRealTime())
            {
                GetProps[0] |= InputMetadataSectionMask;
            }

            GetDataList(GetProps, pData, offsets, GetPropsLength);

            if (NULL != pData[0])
            {
                FaceROIInformation faceRoiData = {};
                RectangleCoordinate* pRoiRect  = NULL;

                Utils::Memcpy(&faceRoiData, pData[0], sizeof(FaceROIInformation));
                pInputData->fDData.numberOfFace = static_cast<UINT16>(
                    (faceRoiData.ROICount > MAX_FACE_NUM) ? MAX_FACE_NUM : faceRoiData.ROICount);

                for (UINT32 i = 0; i < pInputData->fDData.numberOfFace; i++)
                {
                    pRoiRect = &faceRoiData.unstabilizedROI[i].faceRect;

                    // FD is already wrt camif, no need to translate

                    pInputData->fDData.faceCenterX[i] = static_cast<INT16>(pRoiRect->left + (pRoiRect->width / 2));
                    pInputData->fDData.faceCenterY[i] = static_cast<INT16>(pRoiRect->top + (pRoiRect->height / 2));
                    pInputData->fDData.faceRadius[i]  = static_cast<INT16>(Utils::MinUINT32(pRoiRect->width, pRoiRect->height));

                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "center x:%d y:%d  r:%d", pInputData->fDData.faceCenterX[i],
                                     pInputData->fDData.faceCenterY[i], pInputData->fDData.faceRadius[i]);
                }

                result = CamxResultSuccess;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Face ROI is not published");
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "query Face ROI failed result %d", result);
        }
    }

    return CamxResultSuccess;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::SetupDeviceResource
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::SetupDeviceResource(
    CSLBufferInfo*     pConfigIOMem,
    CSLDeviceResource* pResource)
{
    CamxResult                  result                      = CamxResultSuccess;
    CSLICPAcquireDeviceInfo*    pICPResource                = NULL;
    CSLICPResourceInfo*         pICPOutResource             = NULL;
    SIZE_T                      resourceSize                = 0;
    UINT                        numInputPort                = 0;
    UINT                        numOutputPort               = 0;
    UINT                        outputPortIndex             = 0;
    BpsConfigIo*                pConfigIO                   = NULL;
    BpsConfigIoData*            pConfigIOData               = NULL;
    UINT                        inputPortId[BPSMaxInput]    = { 0 };
    UINT                        outputPortId[BPSMaxOutput]  = { 0 };
    UINT                        FPS                         = 30;
    const ImageFormat*          pImageFormat                = NULL;
    BPS_IO_IMAGES               firmwarePortId;
    BOOL                        validFormat;
    UINT32                      BPSPathIndex;

    // Get Input Port List
    GetAllInputPortIds(&numInputPort, &inputPortId[0]);

    // Get Output Port List
    GetAllOutputPortIds(&numOutputPort, &outputPortId[0]);

    if ((numInputPort <= 0)             ||
        (numOutputPort <= 0)            ||
        (numOutputPort > BPSMaxOutput)  ||
        (numInputPort > BPSMaxInput))
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "invalid input/output port");
        result = CamxResultEUnsupported;
    }

    if (CamxResultSuccess == result)
    {
        /// @todo (CAMX-2142) Need correct (non-real-time) fps from HAL
        m_FPS = 30;
        static const UINT PropertiesBPS[] =
        {
            PropertyIDUsecaseFPS
        };
        static const UINT Length = CAMX_ARRAY_SIZE(PropertiesBPS);
        VOID* pData[Length] = { 0 };
        UINT64 propertyDataBPSOffset[Length] = { 0 };

        GetDataList(PropertiesBPS, pData, propertyDataBPSOffset, Length);
        if (NULL != pData[0])
        {
            m_FPS = *reinterpret_cast<UINT*>(pData[0]);
        }

        resourceSize = sizeof(CSLICPAcquireDeviceInfo) + (sizeof(CSLICPResourceInfo) * (numOutputPort - 1));
        pICPResource = static_cast<CSLICPAcquireDeviceInfo*>(CAMX_CALLOC(resourceSize));
        if (NULL == pICPResource)
        {
            result = CamxResultENoMemory;
            return result;
        }

        pICPResource->resourceType = CSLICPResourceIDBPS;
        pICPOutResource            = &pICPResource->outputResource[0];
        pConfigIO                  = reinterpret_cast<BpsConfigIo*>(pConfigIOMem->pVirtualAddr);
        pConfigIO->userArg         = 0;
        pConfigIOData              = &pConfigIO->cmdData;

        CamX::Utils::Memset(pConfigIOData, 0, sizeof(*pConfigIOData));

        CAMX_ASSERT(numInputPort == 1);

        // BPS has only one input port so GetInputPortImageFormat is called with zero.

        pImageFormat = GetInputPortImageFormat(0);
        if (NULL != pImageFormat)
        {
            // Once CHI Supports end to end BPS IDEAL RAW that is 14b bayer output we need to revisit
            // below check as by default it is "IMAGE_FORMAT_BAYER_10"
            pConfigIOData->images[BPS_INPUT_IMAGE].info.format                  =
                TranslateBPSFormatToFirmwareImageFormat(pImageFormat, BPS_INPUT_IMAGE);
            pConfigIOData->images[BPS_INPUT_IMAGE].info.dimensions.widthPixels  = pImageFormat->width;
            pConfigIOData->images[BPS_INPUT_IMAGE].info.dimensions.heightLines  = pImageFormat->height;
            pConfigIOData->images[BPS_INPUT_IMAGE].info.pixelPackingAlignment   = PIXEL_LSB_ALIGNED;
            pConfigIOData->images[BPS_INPUT_IMAGE].info.enableByteSwap          = 0;

            CAMX_LOG_INFO(CamxLogGroupPProc, "node %s Input : firmwarePortId %d format %d, width %d, height %d",
                           NodeIdentifierString(),
                           BPS_INPUT_IMAGE, pImageFormat->format,
                           pImageFormat->width, pImageFormat->height);

            if (TRUE == ImageFormatUtils::IsRAW(pImageFormat))
            {
                const SensorMode* pSensorMode = GetSensorModeData(IsRealTime());
                CAMX_ASSERT(NULL != pSensorMode);
                if (NULL != pSensorMode)
                {
                    pConfigIOData->images[BPS_INPUT_IMAGE].info.bayerOrder =
                        TranslateBPSFormatToFirmwareBayerOrder(pSensorMode->format);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Unable to get Sensor format.. BPS Output may be Wrong");
                    pConfigIOData->images[BPS_INPUT_IMAGE].info.bayerOrder =
                        FIRST_PIXEL_R;
                }
                pConfigIOData->images[BPS_INPUT_IMAGE].bufferLayout[0].bufferStride =
                    pImageFormat->formatParams.rawFormat.stride;
                pConfigIOData->images[BPS_INPUT_IMAGE].bufferLayout[0].bufferHeight =
                    pImageFormat->formatParams.rawFormat.sliceHeight;
            }
            else
            {
                validFormat = ((TRUE == ImageFormatUtils::IsYUV(pImageFormat)) ||
                               (TRUE == ImageFormatUtils::IsUBWC(pImageFormat->format)));

                CAMX_ASSERT(TRUE == validFormat);

                pConfigIOData->images[BPS_INPUT_IMAGE].info.yuv422Order = PIXEL_ORDER_Y_U_Y_V;
                for (UINT plane = 0; plane < ImageFormatUtils::GetNumberOfPlanes(pImageFormat); plane++)
                {
                    CAMX_ASSERT(plane <= MAX_NUM_OF_IMAGE_PLANES);
                    pConfigIOData->images[BPS_INPUT_IMAGE].bufferLayout[plane].bufferStride         =
                        pImageFormat->formatParams.yuvFormat[plane].planeStride;
                    pConfigIOData->images[BPS_INPUT_IMAGE].bufferLayout[plane].bufferHeight         =
                        pImageFormat->formatParams.yuvFormat[plane].sliceHeight;
                    pConfigIOData->images[BPS_INPUT_IMAGE].metadataBufferLayout[plane].bufferStride =
                        pImageFormat->formatParams.yuvFormat[plane].metadataStride;
                    pConfigIOData->images[BPS_INPUT_IMAGE].metadataBufferLayout[plane].bufferHeight =
                        pImageFormat->formatParams.yuvFormat[plane].metadataHeight;
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Input plane %d, stride %d, scanline %d, metastride %d,"
                                     "metascanline %d",
                                     plane,
                                     pImageFormat->formatParams.yuvFormat[plane].planeStride,
                                     pImageFormat->formatParams.yuvFormat[plane].sliceHeight,
                                     pImageFormat->formatParams.yuvFormat[plane].metadataStride,
                                     pImageFormat->formatParams.yuvFormat[plane].metadataHeight);
                }
            }

            pICPResource->inputResource.format = static_cast <UINT32>(pImageFormat->format);
            pICPResource->inputResource.width  = pImageFormat->width;
            pICPResource->inputResource.height = pImageFormat->height;
            pICPResource->inputResource.FPS    = m_FPS;
            pICPResource->numOutputResource    = numOutputPort;

            /// @todo (CAMX-1375) verify BPS ports with corresponding formats
            for (outputPortIndex = 0; outputPortIndex < numOutputPort; outputPortIndex++)
            {
                // Enable the path for BPS, required for dynamically enable/disable output paths based on use case
                BPSPathIndex                    = outputPortId[outputPortIndex];
                m_BPSPathEnabled[BPSPathIndex]  = TRUE;

                TranslateToFirmwarePortId(outputPortId[outputPortIndex], &firmwarePortId);

                /// @todo (CAMX-1375) update raw format output. Also update byte swap
                pImageFormat                                                       =
                    GetOutputPortImageFormat(OutputPortIndex(outputPortId[outputPortIndex]));

                if (NULL == pImageFormat)
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Null ImageFormat for Input");
                    result = CamxResultENoMemory;
                    break;
                }

                // Once CHI Supports end to end BPS IDEAL RAW that is 14b bayer output we need to revisit
                // below check as by default it is "IMAGE_FORMAT_BAYER_10"
                pConfigIOData->images[firmwarePortId].info.format                  =
                    TranslateBPSFormatToFirmwareImageFormat(pImageFormat, firmwarePortId);

                if ((Format::Blob == pImageFormat->format) && (BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_STATS_BG == firmwarePortId))
                {
                    // Special case where we have width in pixels stored in imageformat, as different blob buffers could
                    // have different strides. So for actual width read HW capability
                    pConfigIOData->images[firmwarePortId].info.dimensions.widthPixels  = AWBBGStatsMaxHorizontalRegions - 1;
                    pConfigIOData->images[firmwarePortId].info.dimensions.heightLines  = AWBBGStatsMaxVerticalRegions - 1;
                }
                else
                {
                    pConfigIOData->images[firmwarePortId].info.dimensions.widthPixels  =
                        pImageFormat->width;
                    pConfigIOData->images[firmwarePortId].info.dimensions.heightLines  =
                        pImageFormat->height;
                }
                pConfigIOData->images[firmwarePortId].info.enableByteSwap          =
                    ((pImageFormat->format == CamX::Format::YUV420NV21) ||
                    (pImageFormat->format == CamX::Format::YUV420NV21TP10)) ? 1 : 0;

                if (BPS_OUTPUT_IMAGE_FULL == firmwarePortId)
                {
                    validFormat = ((CamX::Format::YUV420NV21TP10 == pImageFormat->format) ||
                                  (CamX::Format::YUV420NV12TP10  == pImageFormat->format) ||
                                  (CamX::Format::UBWCTP10        == pImageFormat->format) ||
                                  (CamX::Format::P010            == pImageFormat->format));
                    CAMX_ASSERT_MESSAGE((TRUE == validFormat), "BPS output format is invalid, will blow up, FIX XML config");
                }

                if (TRUE == ImageFormatUtils::IsUBWC(pImageFormat->format))
                {
                    pConfigIOData->images[firmwarePortId].info.ubwcVersion      =
                        static_cast <UbwcVersion>(ImageFormatUtils::GetUBWCHWVersion());
                    if (UBWC_VERSION_3 <= ImageFormatUtils::GetUBWCHWVersion())
                    {
                        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Setting lossy %d", pImageFormat->ubwcVerInfo.lossy);
                        pConfigIOData->images[firmwarePortId].info.ubwcLossyMode    = pImageFormat->ubwcVerInfo.lossy;
                        pConfigIOData->images[firmwarePortId].info.ubwcBwLimit      = TITAN175UBWCBandwidthLimit;
                        pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy0 = TITAN175UBWCThresholdLossy0;
                        pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy1 = TITAN175UBWCThresholdLossy1;
                    }
                }

                CAMX_LOG_INFO(CamxLogGroupPProc, "node %s O/P: firmwarePortId %d format %d,"
                              "FW format %d width %d, height %d",
                              NodeIdentifierString(),
                              firmwarePortId,
                              pImageFormat->format,
                              pConfigIOData->images[firmwarePortId].info.format,
                              pImageFormat->width,
                              pImageFormat->height);

                if (TRUE == ImageFormatUtils::IsRAW(pImageFormat))
                {
                    pConfigIOData->images[firmwarePortId].bufferLayout[0].bufferStride =
                        pImageFormat->formatParams.rawFormat.stride;
                    pConfigIOData->images[firmwarePortId].bufferLayout[1].bufferHeight =
                        pImageFormat->formatParams.rawFormat.sliceHeight;
                }
                else
                {
                    validFormat = ((TRUE            == ImageFormatUtils::IsYUV(pImageFormat))           ||
                                   (TRUE            == ImageFormatUtils::IsUBWC(pImageFormat->format))  ||
                                   (Format::Blob    == pImageFormat->format));

                    CAMX_ASSERT(TRUE == validFormat);

                    for (UINT plane = 0; plane < ImageFormatUtils::GetNumberOfPlanes(pImageFormat); plane++)
                    {
                        CAMX_ASSERT(plane <= MAX_NUM_OF_IMAGE_PLANES);
                        pConfigIOData->images[firmwarePortId].bufferLayout[plane].bufferStride         =
                            pImageFormat->formatParams.yuvFormat[plane].planeStride;
                        pConfigIOData->images[firmwarePortId].bufferLayout[plane].bufferHeight         =
                            pImageFormat->formatParams.yuvFormat[plane].sliceHeight;
                        pConfigIOData->images[firmwarePortId].metadataBufferLayout[plane].bufferStride =
                            pImageFormat->formatParams.yuvFormat[plane].metadataStride;
                        pConfigIOData->images[firmwarePortId].metadataBufferLayout[plane].bufferHeight =
                            pImageFormat->formatParams.yuvFormat[plane].metadataHeight;
                        CAMX_LOG_INFO(CamxLogGroupPProc, "Ouptut plane %d, stride %d, scanline %d,"
                                         "metastride %d, metascanline %d",
                                         plane,
                                         pImageFormat->formatParams.yuvFormat[plane].planeStride,
                                         pImageFormat->formatParams.yuvFormat[plane].sliceHeight,
                                         pImageFormat->formatParams.yuvFormat[plane].metadataStride,
                                         pImageFormat->formatParams.yuvFormat[plane].metadataHeight);
                    }
                }

                // Special case where we have width in pixels stored in imageformat, as different blob buffers could
                // have different strides. So for actual width read HW capability
                if ((Format::Blob == pImageFormat->format) && (BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_STATS_BG == firmwarePortId))
                {
                    pICPOutResource->width  = AWBBGStatsMaxHorizontalRegions - 1;
                    pICPOutResource->height = AWBBGStatsMaxVerticalRegions - 1;
                }
                else
                {
                    pICPOutResource->width  = pImageFormat->width;
                    pICPOutResource->height = pImageFormat->height;
                }
                pICPOutResource->format = static_cast <UINT32>(pImageFormat->format);
                pICPOutResource->FPS    = m_FPS;
                pICPOutResource++;
            }

            if (CamxResultSuccess == result)
            {
                pICPResource->hIOConfigCmd = pConfigIOMem->hHandle;
                pICPResource->IOConfigLen  = sizeof(BpsConfigIo);

                // Add to the resource list
                pResource->resourceID              = CSLICPResourceIDBPS;
                pResource->pDeviceResourceParam    = static_cast<VOID*>(pICPResource);
                pResource->deviceResourceParamSize = resourceSize;

                CAMX_LOG_INFO(CamxLogGroupPProc, "resourceSize = %d, IOConfigLen = %d",
                              resourceSize, pICPResource->IOConfigLen);
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Null ImageFormat for Input");
            result = CamxResultENoMemory;
        }
    }

    if (CamxResultSuccess == result)
    {
        result = InitializeStripingParams(pConfigIOData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Initialize Striping params failed %d", result);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::AcquireDevice
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::AcquireDevice()
{
    CamxResult           result                   = CamxResultSuccess;

    if (CSLInvalidHandle == m_configIOMem.hHandle)
    {
        result = CSLAlloc(NodeIdentifierString(),
                      &m_configIOMem,
                      GetFWBufferAlignedSize(sizeof(BpsConfigIo)),
                      1,
                      (CSLMemFlagUMDAccess | CSLMemFlagSharedAccess | CSLMemFlagHw | CSLMemFlagKMDAccess),
                      &DeviceIndices()[0],
                      1);
    }

    if (CamxResultSuccess == result)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "CSLAlloc returned configIOMem.fd=%d", m_configIOMem.fd);
        CAMX_ASSERT(CSLInvalidHandle != m_configIOMem.hHandle);
        CAMX_ASSERT(NULL != m_configIOMem.pVirtualAddr);

        if ((NULL != m_configIOMem.pVirtualAddr) && (CSLInvalidHandle != m_configIOMem.hHandle))
        {
            result = SetupDeviceResource(&m_configIOMem, &m_deviceResourceRequest);
        }

        if (CamxResultSuccess == result)
        {
            result = CSLAcquireDevice(GetCSLSession(),
                                      &m_hDevice,
                                      DeviceIndices()[0],
                                      &m_deviceResourceRequest,
                                      1,
                                      NULL,
                                      0,
                                      NodeIdentifierString());

            if (CamxResultSuccess == result)
            {
                SetDeviceAcquired(TRUE);
                AddCSLDeviceHandle(m_hDevice);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Acquire BPS Device Failed");
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Out of memory");
    }



    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::ReleaseDevice
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::ReleaseDevice()
{
    CamxResult result = CamxResultSuccess;

    DeInitializeStripingParams();

    if (NULL != GetHwContext())
    {
        result = CSLReleaseDevice(GetCSLSession(), m_hDevice);

        if (CamxResultSuccess == result)
        {
            SetDeviceAcquired(FALSE);
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to release device");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::ConfigureBPSCapability
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::ConfigureBPSCapability()
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-1212) Finalize the version number definition with CSL layer.
    GetHwContext()->GetDeviceVersion(CSLDeviceTypeICP, &m_version);

    switch (m_version.majorVersion)
    {
        case 0:
            m_capability.numBPSIQModules     = sizeof(BPSIQModulesList) / sizeof(BPSIQModuleInfo);
            m_capability.pBPSIQModuleList    = BPSIQModulesList;
            m_capability.numBPSStatsModules  = sizeof(BPSStatsModulesList) / sizeof(BPSStatsModuleInfo);
            m_capability.pBPSStatsModuleList = BPSStatsModulesList;
            // Striping library in firmware will be removed in future. Remove this setting once striping in FW is removed.
            m_capability.swStriping          = static_cast<Titan17xContext*>(
                GetHwContext())->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->BPSSwStriping;
            break;

        default:
            result = CamxResultEUnsupported;
            CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported Version Number");
            break;
    }

    OverrideDefaultIQModuleEnablement();

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::OverrideDefaultIQModuleEnablement
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::OverrideDefaultIQModuleEnablement()
{
    const Titan17xStaticSettings* pSettings =
        static_cast<Titan17xContext*>(GetHwContext())->GetTitan17xSettingsManager()->GetTitan17xStaticSettings();

    for (UINT i = 0; i < m_capability.numBPSIQModules; i++)
    {
        // If there is a create function for the IQ module, check to see if it is being forced on/off
        if (NULL != m_capability.pBPSIQModuleList[i].IQCreate)
        {
            switch (m_capability.pBPSIQModuleList[i].moduleType)
            {
                case ISPIQModuleType::BPSPedestalCorrection:
                    if (IQModuleForceDefault != pSettings->forceBPSPedestalCorrection)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSPedestalCorrection;
                    }
                    break;
                case ISPIQModuleType::BPSLinearization:
                    if (IQModuleForceDefault != pSettings->forceBPSLinearization)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSLinearization;
                    }
                    break;
                case ISPIQModuleType::BPSBPCPDPC:
                    if (IQModuleForceDefault != pSettings->forceBPSBPCPDPC)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSBPCPDPC;
                    }
                    break;
                case ISPIQModuleType::BPSDemux:
                    if (IQModuleForceDefault != pSettings->forceBPSDemux)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSDemux;
                    }
                    break;
                case ISPIQModuleType::BPSHDR:
                    if (IQModuleForceDefault != pSettings->forceBPSHDR)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSHDR;
                    }
                    break;
                case ISPIQModuleType::BPSABF:
                    if (IQModuleForceDefault != pSettings->forceBPSABF)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSABF;
                    }
                    break;
                case ISPIQModuleType::BPSGIC:
                    if (IQModuleForceDefault != pSettings->forceBPSGIC)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSGIC;
                    }
                    break;
                case ISPIQModuleType::BPSLSC:
                    if (IQModuleForceDefault != pSettings->forceBPSLSC)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSLSC;
                    }
                    break;
                case ISPIQModuleType::BPSWB:
                    if (IQModuleForceDefault != pSettings->forceBPSWB)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSWB;
                    }
                    break;
                case ISPIQModuleType::BPSDemosaic:
                    if (IQModuleForceDefault != pSettings->forceBPSDemosaic)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSDemosaic;
                    }
                    break;
                case ISPIQModuleType::BPSCC:
                    if (IQModuleForceDefault != pSettings->forceBPSCC)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSCC;
                    }
                    break;
                case ISPIQModuleType::BPSGTM:
                    if (IQModuleForceDefault != pSettings->forceBPSGTM)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSGTM;
                    }
                    break;
                case ISPIQModuleType::BPSGamma:
                    if (IQModuleForceDefault != pSettings->forceBPSGamma)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSGamma;
                    }
                    break;
                case ISPIQModuleType::BPSCST:
                    if (IQModuleForceDefault != pSettings->forceBPSCST)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSCST;
                    }
                    break;
                case ISPIQModuleType::BPSChromaSubSample:
                    if (IQModuleForceDefault != pSettings->forceBPSChromaSubSample)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSChromaSubSample;
                    }
                    break;
                case ISPIQModuleType::BPSHNR:
                    if (IQModuleForceDefault != pSettings->forceBPSHNR)
                    {
                        m_capability.pBPSIQModuleList[i].isEnabled = pSettings->forceBPSHNR;
                    }
                    break;
                default:
                    CAMX_LOG_WARN(CamxLogGroupPProc,
                                   "IQ module exists without an appropriate settings override.  Must add override");
                    break;

            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GetProcessingSectionForBPSProfile
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSProcessingSection GetProcessingSectionForBPSProfile(
    BPSProfileId propertyValue)
{
    BPSProcessingSection type = BPSProcessingSection::BPSAll;
    switch (propertyValue)
    {
        case BPSProfileId::BPSProfileIdHNR:
            type = BPSProcessingSection::BPSHNR;
            break;
        // For ideal (LSC) raw input image, disable all the block pre-LSC and LSC blocks
        case BPSProfileId::BPSProfileIdIdealRawInput:
            type = BPSProcessingSection::BPSPostLSC;
            break;
        // For ideal (LSC) raw output image, disable all the block Post LSC
        case BPSProfileId::BPSProfileIdIdealRawOutput:
            type = BPSProcessingSection::BPSLSCOut;
            break;
        case BPSProfileId::BPSProfileIdDefault:
        case BPSProfileId::BPSProfileIdPrefilter:
        case BPSProfileId::BPSProfileIdBlend:
        case BPSProfileId::BPSProfileIdBlendPost:
            type = BPSProcessingSection::BPSAll;
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Unsupported IQ module type");
            break;
    }
    return type;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::CreateBPSIQModules
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::CreateBPSIQModules()
{
    CamxResult             result                   = CamxResultSuccess;
    BPSIQModuleInfo*       pIQModule                = m_capability.pBPSIQModuleList;
    UINT                   count                    = 0;
    BPSModuleCreateData    moduleInputData          = { 0 };
    BPSProcessingSection   instanceSection          = BPSProcessingSection::BPSAll;
    BPSProcessingSection   moduleSection            = BPSProcessingSection::BPSAll;
    CmdBufferManagerParam* pBufferManagerParam      = NULL;
    IQModuleCmdBufferParam bufferManagerCreateParam = { 0 };
    BOOL                   moduleDependeciesEnabled = TRUE;

    m_numBPSIQModuleEnabled                                         = 0;
    moduleInputData.initializationData.pipelineBPSData.pDeviceIndex = &m_deviceIndex;
    moduleInputData.initializationData.requestQueueDepth            = GetPipeline()->GetRequestQueueDepth();
    moduleInputData.pNodeIdentifier                                 = NodeIdentifierString();

    // Based on profileId get modules from which processing section shall be created.
    instanceSection = GetProcessingSectionForBPSProfile(m_instanceProperty.profileId);

    pBufferManagerParam =
        static_cast<CmdBufferManagerParam*>(CAMX_CALLOC(sizeof(CmdBufferManagerParam) * m_capability.numBPSIQModules));
    if (NULL != pBufferManagerParam)
    {
        bufferManagerCreateParam.pCmdBufManagerParam    = pBufferManagerParam;
        bufferManagerCreateParam.numberOfCmdBufManagers = 0;
        for (count = 0; count < m_capability.numBPSIQModules; count++)
        {
            moduleDependeciesEnabled = TRUE;
            if (pIQModule[count].moduleType == ISPIQModuleType::BPSHNR)
            {
                moduleSection = BPSProcessingSection::BPSHNR;
            }
            // check if module belongs to section which profile has asked to enable.
            if ((instanceSection != moduleSection) && (instanceSection != BPSProcessingSection::BPSAll))
            {
                moduleDependeciesEnabled = FALSE;
            }

            // Should be taken care by instance adding safe check
            if ((BPSProfileId::BPSProfileIdBlendPost == m_instanceProperty.profileId) &&
                    (pIQModule[count].moduleType == ISPIQModuleType::BPSHNR))
            {
                moduleDependeciesEnabled = FALSE;
            }

            if ((BPSProcessingSection::BPSPostLSC == instanceSection) &&
                (BPSProfileId::BPSProfileIdIdealRawInput == m_instanceProperty.profileId))
            {
                // Disable IQ modules till LSC and Enable Post LSC modules
                moduleDependeciesEnabled = IsPostLSCModule(pIQModule[count].moduleType);
                CAMX_LOG_INFO(CamxLogGroupPProc,
                    "BPS module: PostLSC: count = %u, m_numBPSIQModuleEnabled = %u, moduleDependeciesEnabled= %u",
                    count,
                    m_numBPSIQModuleEnabled,
                    moduleDependeciesEnabled);
            }

            if ((BPSProcessingSection::BPSLSCOut == instanceSection) &&
                (BPSProfileId::BPSProfileIdIdealRawOutput == m_instanceProperty.profileId))
            {
                // Enable IQ modules till LSC and Disable Post LSC modules
                moduleDependeciesEnabled = !IsPostLSCModule(pIQModule[count].moduleType);
                CAMX_LOG_INFO(CamxLogGroupPProc,
                    "BPS module: PreLSC: count = %u, m_numBPSIQModuleEnabled = %u, moduleDependeciesEnabled= %u",
                    count,
                    m_numBPSIQModuleEnabled,
                    moduleDependeciesEnabled);
            }

            if ((TRUE == pIQModule[count].isEnabled) && (TRUE == moduleDependeciesEnabled))
            {
                result = pIQModule[count].IQCreate(&moduleInputData);

                if (CamxResultSuccess == result)
                {
                    moduleInputData.pModule->FillCmdBufferManagerParams(&moduleInputData.initializationData,
                                                                        &bufferManagerCreateParam);
                    m_pBPSIQModules[m_numBPSIQModuleEnabled] = moduleInputData.pModule;
                    m_numBPSIQModuleEnabled++;
                }
                else
                {
                    CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Create IQ Module, count = %d", count);
                    break;
                }
            }
        }

        if ((CamxResultSuccess == result) && (0 != bufferManagerCreateParam.numberOfCmdBufManagers))
        {
            // Create Cmd Buffer Managers for IQ Modules
            result = CmdBufferManager::CreateMultiManager(pBufferManagerParam,
                                                          bufferManagerCreateParam.numberOfCmdBufManagers);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "IQ MOdules Cmd Buffer Manager Creation failed");
            }
        }

        // Free up the memory allocated by IQ Blocks
        for (UINT index = 0; index < bufferManagerCreateParam.numberOfCmdBufManagers; index++)
        {
            if (NULL != pBufferManagerParam[index].pBufferManagerName)
            {
                CAMX_DELETE pBufferManagerParam[index].pBufferManagerName;
            }

            if (NULL != pBufferManagerParam[index].pParams)
            {
                CAMX_DELETE pBufferManagerParam[index].pParams;
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Out Of Memory");
        result = CamxResultENoMemory;
    }

    if (NULL != pBufferManagerParam)
    {
        CAMX_DELETE pBufferManagerParam;
    }

    // The clean-up for the error case happens outside this function
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetBPSSWTMCModuleInstanceVersion()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SWTMCVersion BPSNode::GetBPSSWTMCModuleInstanceVersion()
{
    // If SWTMC IQ module is not installed, version is 1.0
    SWTMCVersion version = SWTMCVersion::TMC10;

    for (UINT i = 0; i < m_numBPSIQModuleEnabled; i++)
    {
        if (ISPIQModuleType::SWTMC == m_pBPSIQModules[i]->GetIQType())
        {
            // If several new versions of SWTMC are added later, instead of casting to SWTMC11, may need a polymorphism.
            version = static_cast<SWTMC11*>(m_pBPSIQModules[i])->GetTMCVersion();
            break;
        }
    }

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::CreateBPSStatsModules
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::CreateBPSStatsModules()
{
    CamxResult                  result             = CamxResultSuccess;
    BPSStatsModuleInfo*         pStatsModule       = m_capability.pBPSStatsModuleList;
    UINT                        count              = 0;
    BPSStatsModuleCreateData    moduleInputData    = { 0 };

    m_numBPSStatsModuleEnabled = 0;

    for (count = 0; count < m_capability.numBPSStatsModules; count++)
    {
        if (TRUE == pStatsModule[count].isEnabled)
        {
            result = pStatsModule[count].StatsCreate(&moduleInputData);

            if (CamxResultSuccess == result)
            {
                m_pBPSStatsModules[m_numBPSStatsModuleEnabled] = moduleInputData.pModule;
                m_numBPSStatsModuleEnabled++;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create Stats Module.  count %d", count);
                break;
            }
        }
    }

    // The clean-up for the error case happens outside this function
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::Cleanup()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::Cleanup()
{
    UINT           count            = 0;
    CamxResult     result           = CamxResultSuccess;
    UINT           numberOfMappings = 0;
    CSLBufferInfo  bufferInfo       = { 0 };
    CSLBufferInfo* pBufferInfo[CSLICPMaxMemoryMapRegions];

    if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferFrameProcess])
    {
        if (NULL != m_pBPSCmdBufferManager[BPSCmdBufferFrameProcess]->GetMergedCSLBufferInfo())
        {
            Utils::Memcpy(&bufferInfo,
                          m_pBPSCmdBufferManager[BPSCmdBufferFrameProcess]->GetMergedCSLBufferInfo(),
                          sizeof(CSLBufferInfo));
            pBufferInfo[numberOfMappings] = &bufferInfo;
            numberOfMappings++;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Unable to get the Merged Buffer info");
            result = CamxResultEFailed;
        }
    }

    if (0 != numberOfMappings)
    {
        result = SendFWCmdRegionInfo(CSLICPGenericBlobCmdBufferUnMapFWMemRegion,
                                     pBufferInfo,
                                     numberOfMappings);
    }

    // De-allocate all of the IQ modules
    for (count = 0; count < m_numBPSIQModuleEnabled; count++)
    {
        if (NULL != m_pBPSIQModules[count])
        {
            m_pBPSIQModules[count]->Destroy();
            m_pBPSIQModules[count] = NULL;
        }
    }

    for (count = 0; count < m_numBPSStatsModuleEnabled; count++)
    {
        if (NULL != m_pBPSStatsModules[count])
        {
            m_pBPSStatsModules[count]->Destroy();
            m_pBPSStatsModules[count] = NULL;
        }
    }
    m_numBPSIQModuleEnabled    = 0;
    m_numBPSStatsModuleEnabled = 0;

    // Check if striping in umd is enabled before destroying striping library context
    if (TRUE == m_capability.swStriping)
    {
        result = BPSStripingLibraryContextDestroy(&m_hStripingLib);
        if (CamxResultSuccess != result)
        {
            /// @todo (CAMX-1732) To add translation for firmware errors
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to destroy striping library");
        }
    }

    if (NULL != m_pTuningMetadata)
    {
        CAMX_FREE(m_pTuningMetadata);
        m_pTuningMetadata = NULL;
    }

    if (NULL != s_pDebugDataWriter)
    {
        s_debugDataWriterCounter--;

        if (0 == s_debugDataWriterCounter)
        {
            CAMX_DELETE s_pDebugDataWriter;
            s_pDebugDataWriter = NULL;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::PopulateGeneralTuningMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::PopulateGeneralTuningMetadata(
    ISPInputData* pInputData)
{
    BpsIQSettings*  pBPSIQsettings = static_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);

    // Copy to a packed structure the BPS module configuration used by FW
    CAMX_STATIC_ASSERT(sizeof(BpsIQSettings) <= sizeof(pInputData->pBPSTuningMetadata->BPSModuleConfigData));
    Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSModuleConfigData,
                  pBPSIQsettings,
                  sizeof(BpsIQSettings));

    // Populate trigger data
    BPSTuningTriggerData*   pBPSTuningTriggers  = &pInputData->pBPSTuningMetadata->BPSTuningTriggers;

    pBPSTuningTriggers->AECexposureTime             = pInputData->triggerData.AECexposureTime;
    pBPSTuningTriggers->AECSensitivity              = pInputData->triggerData.AECSensitivity;
    pBPSTuningTriggers->AECGain                     = pInputData->triggerData.AECGain;
    pBPSTuningTriggers->AECLuxIndex                 = pInputData->triggerData.AECLuxIndex;
    pBPSTuningTriggers->AWBleftGGainWB              = pInputData->triggerData.AWBleftGGainWB;
    pBPSTuningTriggers->AWBleftBGainWB              = pInputData->triggerData.AWBleftBGainWB;
    pBPSTuningTriggers->AWBleftRGainWB              = pInputData->triggerData.AWBleftRGainWB;
    pBPSTuningTriggers->AWBColorTemperature         = pInputData->triggerData.AWBColorTemperature;
    pBPSTuningTriggers->DRCGain                     = pInputData->triggerData.DRCGain;
    pBPSTuningTriggers->DRCGainDark                 = pInputData->triggerData.DRCGainDark;
    pBPSTuningTriggers->lensPosition                = pInputData->triggerData.lensPosition;
    pBPSTuningTriggers->lensZoom                    = pInputData->triggerData.lensZoom;
    pBPSTuningTriggers->postScaleRatio              = pInputData->triggerData.postScaleRatio;
    pBPSTuningTriggers->preScaleRatio               = pInputData->triggerData.preScaleRatio;
    pBPSTuningTriggers->sensorImageWidth            = pInputData->triggerData.sensorImageWidth;
    pBPSTuningTriggers->sensorImageHeight           = pInputData->triggerData.sensorImageHeight;
    pBPSTuningTriggers->CAMIFWidth                  = pInputData->triggerData.CAMIFWidth;
    pBPSTuningTriggers->CAMIFHeight                 = pInputData->triggerData.CAMIFHeight;
    pBPSTuningTriggers->numberOfLED                 = pInputData->triggerData.numberOfLED;
    pBPSTuningTriggers->LEDSensitivity              = static_cast<INT32>(pInputData->triggerData.LEDSensitivity);
    pBPSTuningTriggers->bayerPattern                = pInputData->triggerData.bayerPattern;
    pBPSTuningTriggers->sensorOffsetX               = pInputData->triggerData.sensorOffsetX;
    pBPSTuningTriggers->sensorOffsetY               = pInputData->triggerData.sensorOffsetY;
    pBPSTuningTriggers->blackLevelOffset            = pInputData->triggerData.blackLevelOffset;
    // Parameters added later (if this trigger data gets update it again, integrate with main BPSTuningTriggerData struct)
    pInputData->pBPSTuningMetadata->BPSExposureGainRatio = pInputData->triggerData.AECexposureGainRatio;

    // Populate Sensor configuration data
    IFEBPSSensorConfigData* pBPSSensorConfig    = &pInputData->pBPSTuningMetadata->BPSSensorConfig;
    pBPSSensorConfig->isBayer                   = pInputData->sensorData.isBayer;
    pBPSSensorConfig->format                    = static_cast<UINT32>(pInputData->sensorData.format);
    pBPSSensorConfig->digitalGain               = pInputData->sensorData.dGain;
    pBPSSensorConfig->ZZHDRColorPattern         = pInputData->sensorData.ZZHDRColorPattern;
    pBPSSensorConfig->ZZHDRFirstExposurePattern = pInputData->sensorData.ZZHDRFirstExposurePattern;

    // Populate node information
    pInputData->pBPSTuningMetadata->BPSNodeInformation.instaceId        = InstanceID();
    pInputData->pBPSTuningMetadata->BPSNodeInformation.requestId        = pInputData->frameNum;
    pInputData->pBPSTuningMetadata->BPSNodeInformation.isRealTime       = IsRealTime();
    pInputData->pBPSTuningMetadata->BPSNodeInformation.profileId        = m_instanceProperty.profileId;
    pInputData->pBPSTuningMetadata->BPSNodeInformation.processingType   = m_instanceProperty.processingType;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::DumpTuningMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::DumpTuningMetadata(
    ISPInputData* pInputData)
{
    CamxResult      result                              = CamxResultSuccess;
    DebugData       debugData                           = { 0 };
    UINT            PropertiesTuning[]                  = { 0 };
    static UINT     metaTagDebugDataAll                 = 0;
    const UINT      length                              = CAMX_ARRAY_SIZE(PropertiesTuning);
    VOID*           pData[length]                       = { 0 };
    UINT64          propertyDataTuningOffset[length]    = { 0 };
    DebugData*      pDebugDataPartial                   = NULL;
    BOOL            setNewPartition                     = FALSE;
    UINT32          debugDataPartition                  = 0;
    BpsIQSettings*  pBPSIQSettings                      = NULL;

    if (TRUE == IsRealTime())
    {
        PropertiesTuning[0] = PropertyIDTuningDataBPS;
    }
    else
    {
        VendorTagManager::QueryVendorTagLocation("org.quic.camera.debugdata", "DebugDataAll", &metaTagDebugDataAll);
        PropertiesTuning[0] = metaTagDebugDataAll | InputMetadataSectionMask;
    }

    GetDataList(PropertiesTuning, pData, propertyDataTuningOffset, length);
    pDebugDataPartial = reinterpret_cast<DebugData*>(pData[0]);
    if (NULL == pDebugDataPartial || NULL == pDebugDataPartial->pData)
    {
        // Debug-data buffer not available
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Debug-data requested but buffer not available");
        return;
    }

    if (FALSE == IsRealTime())
    {
        if ((BPSProcessingMFNR  == m_instanceProperty.processingType)   &&
            (BPSProfileIdHNR    == m_instanceProperty.profileId))
        {
            setNewPartition     = TRUE;
            debugDataPartition  = 1;
        }
        else
        {
            // Using single partition for all blend operations for same instance until integrated with new metadata.
            setNewPartition     = FALSE;
            debugDataPartition  = 0;
        }
    }

    if (TRUE == IsRealTime())
    {
        debugData.pData = pDebugDataPartial->pData;
        debugData.size  = pDebugDataPartial->size;
    }
    else if (DebugDataPartitionsBPS > debugDataPartition)
    {
        SIZE_T instanceOffset = 0;
        debugData.size  = HAL3MetadataUtil::DebugDataSize(DebugDataType::BPSTuning);
        // Use second half for offline data
        // Only 2 offline BPS pass supported for debug data
        debugData.size  = debugData.size / DebugDataPartitionsBPS;
        instanceOffset  = debugDataPartition * debugData.size;
        debugData.pData = Utils::VoidPtrInc(pDebugDataPartial->pData,
            (HAL3MetadataUtil::DebugDataOffset(DebugDataType::BPSTuning)) + (instanceOffset));
    }
    else
    {
        result = CamxResultENoMemory;
        CAMX_LOG_ERROR(CamxLogGroupPProc,
                       "RT: %u:  ERROR: Partition: %u: Debug-data Not enough memory to save IPE data frameNum: %llu",
                       IsRealTime(),
                       debugDataPartition,
                       pInputData->frameNum);
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "BPS:RT: %u, frameNum: %llu, InstanceID: %u, base: %p, start: %p, size: %u,"
                     " profId: %u, procType: %u, debugDataPartition: %u",
                     IsRealTime(),
                     pInputData->frameNum,
                     InstanceID(),
                     pDebugDataPartial->pData,
                     debugData.pData,
                     debugData.size,
                     m_instanceProperty.profileId,
                     m_instanceProperty.processingType,
                     debugDataPartition);

    // Populate any metadata obtained direclty from base BPS node
    PopulateGeneralTuningMetadata(pInputData);


    if ((result == CamxResultSuccess)                                                                               &&
        ((FALSE == setNewPartition)                                                                                 ||
        (FALSE  == (s_pDebugDataWriter->IsSameBufferPointer(static_cast<BYTE*>(debugData.pData), debugData.size)))))
    {
        // Set the buffer pointer
        s_pDebugDataWriter->SetBufferPointer(static_cast<BYTE*>(debugData.pData), debugData.size);
    }

    pBPSIQSettings = static_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT_MESSAGE((NULL != pBPSIQSettings), "BPS IQ Settings not available");

    // Add BPS tuning metadata tags
    s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSNodeInfo,
                                   DebugDataTagType::TuningIQNodeInfo,
                                   1,
                                   &pInputData->pBPSTuningMetadata->BPSNodeInformation,
                                   sizeof(pInputData->pBPSTuningMetadata->BPSNodeInformation));

    if (TRUE == pBPSIQSettings->pedestalParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSPedestalRegister,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSPedestalData.pedestalConfig),
                                       &pInputData->pBPSTuningMetadata->BPSPedestalData.pedestalConfig,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSPedestalData.pedestalConfig));


        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSPedestalPackedLUT,
                                       DebugDataTagType::TuningPedestalLUT,
                                       1,
                                       &pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.pedestalLUT,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.pedestalLUT));
    }

    if (TRUE == pBPSIQSettings->linearizationParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSLinearizationRegister,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(
                                            pInputData->pBPSTuningMetadata->BPSLinearizationData.linearizationConfig),
                                       &pInputData->pBPSTuningMetadata->BPSLinearizationData.linearizationConfig,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSLinearizationData.linearizationConfig));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSLinearizationPackedLUT,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->
                                            pBPSTuningMetadata->BPSDMIData.packedLUT.linearizationLUT.linearizationLUT),
                                       &pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.linearizationLUT.linearizationLUT,
                                       sizeof(pInputData->
                                           pBPSTuningMetadata->BPSDMIData.packedLUT.linearizationLUT.linearizationLUT));
    }

    if (TRUE == pBPSIQSettings->pdpcParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSBPCPDPCRegister,
                                       DebugDataTagType::TuningBPSBPCPDPCConfig,
                                       1,
                                       &pInputData->pBPSTuningMetadata->BPSBBPCPDPCData,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSBBPCPDPCData));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSBPCPDPCPackedLUT,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->
                                            pBPSTuningMetadata->BPSDMIData.packedLUT.PDPCLUT.BPSPDPCMaskLUT),
                                       &pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.PDPCLUT.BPSPDPCMaskLUT,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.PDPCLUT.BPSPDPCMaskLUT));
    }

    if ((TRUE == pBPSIQSettings->hdrMacParameters.moduleCfg.EN) || (TRUE == pBPSIQSettings->hdrReconParameters.moduleCfg.EN))
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSHDRRegister,
                                       DebugDataTagType::TuningBPSHDRConfig,
                                       1,
                                       &pInputData->pBPSTuningMetadata->BPSHDRData,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSHDRData));
    }

    if (TRUE == pBPSIQSettings->gicParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSGICRegister,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSGICData.filterConfig),
                                       &pInputData->pBPSTuningMetadata->BPSGICData.filterConfig,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSGICData.filterConfig));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSGICPackedNoiseLUT,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->
                                           pBPSTuningMetadata->BPSDMIData.packedLUT.GICLUT.BPSGICNoiseLUT),
                                       &pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.GICLUT.BPSGICNoiseLUT,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.GICLUT.BPSGICNoiseLUT));
    }

    if (TRUE == pBPSIQSettings->abfParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSABFRegister,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSABFData.config),
                                       &pInputData->pBPSTuningMetadata->BPSABFData.config,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSABFData.config));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSABFPackedLUT,
                                       DebugDataTagType::TuningABFLUT,
                                       1,
                                       &pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.ABFLUT,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.ABFLUT));
    }

    if (TRUE == pBPSIQSettings->lensRollOffParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSLSCRegister,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSLSCData.rolloffConfig),
                                       &pInputData->pBPSTuningMetadata->BPSLSCData.rolloffConfig,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSLSCData.rolloffConfig));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSLSCPackedMesh,
                                       DebugDataTagType::TuningLSCMeshLUT,
                                       1,
                                       &pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.LSCMesh,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.LSCMesh));
    }


    s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSWBRegister,
                                   DebugDataTagType::UInt32,
                                   CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSWBData.WBConfig),
                                   &pInputData->pBPSTuningMetadata->BPSWBData.WBConfig,
                                   sizeof(pInputData->pBPSTuningMetadata->BPSWBData.WBConfig));

    if (TRUE == pBPSIQSettings->demosaicParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSDemosaicRegister,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSDemosaicData.demosaicConfig),
                                       &pInputData->pBPSTuningMetadata->BPSDemosaicData.demosaicConfig,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSDemosaicData.demosaicConfig));
    }

    s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSDemuxRegister,
                                   DebugDataTagType::UInt32,
                                   CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSDemuxData.demuxConfig),
                                   &pInputData->pBPSTuningMetadata->BPSDemuxData.demuxConfig,
                                   sizeof(pInputData->pBPSTuningMetadata->BPSDemuxData.demuxConfig));

    if (TRUE == pBPSIQSettings->colorCorrectParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSCCRegister,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSCCData.colorCorrectionConfig),
                                       &pInputData->pBPSTuningMetadata->BPSCCData.colorCorrectionConfig,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSCCData.colorCorrectionConfig));
    }

    if (TRUE == pBPSIQSettings->gtmParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSGTMPackedLUT,
                                       DebugDataTagType::TuningGTMLUT,
                                       1,
                                       &pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.GTM,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.GTM));
    }

    if (TRUE == pBPSIQSettings->glutParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSGammaLUT,
                                       DebugDataTagType::TuningGammaCurve,
                                       CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.gamma),
                                       &pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.gamma,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.gamma));
    }

    if (TRUE == pBPSIQSettings->colorXformParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSCSTRegister,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSCSTData.CSTConfig),
                                       &pInputData->pBPSTuningMetadata->BPSCSTData.CSTConfig,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSCSTData.CSTConfig));
    }

    if (TRUE == pBPSIQSettings->hnrParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSHNRRegister,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSHNRData.HNRConfig),
                                       &pInputData->pBPSTuningMetadata->BPSHNRData.HNRConfig,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSHNRData.HNRConfig));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSHNRPackedLUT,
                                       DebugDataTagType::TuningBPSHNRLUT,
                                       1,
                                       &pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.HNRLUT,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.HNRLUT));
    }

    s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSModulesConfigRegister,
                                   DebugDataTagType::UInt32,
                                   CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->BPSModuleConfigData.modulesConfigData),
                                   &pInputData->pBPSTuningMetadata->BPSModuleConfigData.modulesConfigData,
                                   sizeof(pInputData->pBPSTuningMetadata->BPSModuleConfigData.modulesConfigData));

    s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSTriggerModulesData,
                                   DebugDataTagType::TuningBPSTriggerData,
                                   1,
                                   &pInputData->pBPSTuningMetadata->BPSTuningTriggers,
                                   sizeof(pInputData->pBPSTuningMetadata->BPSTuningTriggers));

    s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSensorConfigData,
                                   DebugDataTagType::TuningBPSSensorConfig,
                                   1,
                                   &pInputData->pBPSTuningMetadata->BPSSensorConfig,
                                   sizeof(pInputData->pBPSTuningMetadata->BPSSensorConfig));

    s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSExposureGainRatio,
                                   DebugDataTagType::Float,
                                   1,
                                   &pInputData->pBPSTuningMetadata->BPSExposureGainRatio,
                                   sizeof(pInputData->pBPSTuningMetadata->BPSExposureGainRatio));

    s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSOEMTuningData,
                                   DebugDataTagType::UInt32,
                                   CAMX_ARRAY_SIZE(pInputData->pBPSTuningMetadata->oemTuningData.IQOEMTuningData),
                                   &pInputData->pBPSTuningMetadata->oemTuningData.IQOEMTuningData,
                                   sizeof(pInputData->pBPSTuningMetadata->oemTuningData.IQOEMTuningData));

    if (TRUE == pBPSIQSettings->hnrParameters.moduleCfg.EN)
    {
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningBPSHNRFace,
                                       DebugDataTagType::TuningFaceData,
                                       1,
                                       &pInputData->pBPSTuningMetadata->BPSHNRFaceDetection,
                                       sizeof(pInputData->pBPSTuningMetadata->BPSHNRFaceDetection));
    }

    if (TRUE == IsRealTime())
    {
        // Make a copy in main metadata pool
        static const UINT   PropertiesDebugData[]                  = { PropertyIDDebugDataAll };
        VOID*               pSrcData[1]                            = { 0 };
        const UINT          lengthAll                              = CAMX_ARRAY_SIZE(PropertiesDebugData);
        UINT64              propertyDataTuningAllOffset[lengthAll] = { 0 };
        GetDataList(PropertiesDebugData, pSrcData, propertyDataTuningAllOffset, lengthAll);
        VendorTagManager::QueryVendorTagLocation("org.quic.camera.debugdata", "DebugDataAll", &metaTagDebugDataAll);

        const UINT  TuningVendorTag[]   = { metaTagDebugDataAll };
        const VOID* pDstData[1]         = { pSrcData[0] };
        UINT        pDataCount[1]       = { sizeof(DebugData) };
        WriteDataList(TuningVendorTag, pDstData, pDataCount, 1);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::ProgramIQModuleEnableConfig()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::ProgramIQModuleEnableConfig(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_UNREFERENCED_PARAM(pInputData);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::ProgramIQConfig()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::ProgramIQConfig(
    ISPInputData* pInputData)
{
    CamxResult              result              = CamxResultSuccess;
    UINT                    count               = 0;
    BOOL                    adrcEnabled         = FALSE;
    FLOAT                   percentageOfGTM     = 0.0f;
    ISPIQTuningDataBuffer   IQOEMTriggerData;

    if (NULL != pInputData->pBPSTuningMetadata)
    {
        IQOEMTriggerData.pBuffer    = pInputData->pBPSTuningMetadata->oemTuningData.IQOEMTuningData;
        IQOEMTriggerData.size       = sizeof(pInputData->pBPSTuningMetadata->oemTuningData.IQOEMTuningData);
    }
    else
    {
        IQOEMTriggerData.pBuffer    = NULL;
        IQOEMTriggerData.size       = 0;
    }

    // Call IQInterface to Set up the Trigger data
    Node* pBaseNode = this;

    IQInterface::IQSetupTriggerData(pInputData, pBaseNode, IsRealTime(), NULL);

    // Get the default ADRC status.
    IQInterface::GetADRCParams(pInputData, &adrcEnabled, &percentageOfGTM, GetBPSSWTMCModuleInstanceVersion());

    for (count = 0; count < m_numBPSIQModuleEnabled; count++)
    {
        if (TRUE == adrcEnabled)
        {
            // Update AEC Gain values for ADRC use cases, before GTM(includes) will be triggered by shortGain,
            // betweem GTM & LTM(includes) will be by shortGain*power(DRCGain, gtm_perc) and post LTM will be
            // by shortGain*DRCGain
            IQInterface::UpdateAECGain(m_pBPSIQModules[count]->GetIQType(), pInputData, percentageOfGTM);
        }
        if (m_pBPSIQModules[count]->GetIQType() == ISPIQModuleType::BPSWB)
        {
            pInputData->pAECUpdateData->exposureInfo[0].sensitivity
                = pInputData->pAECUpdateData->exposureInfo[0].sensitivity * pInputData->pAECUpdateData->predictiveGain;
            pInputData->pAECUpdateData->exposureInfo[0].linearGain
                = pInputData->pAECUpdateData->exposureInfo[0].linearGain * pInputData->pAECUpdateData->predictiveGain;
            IQInterface::IQSetupTriggerData(pInputData, pBaseNode, 0, &IQOEMTriggerData);
        }
        result = m_pBPSIQModules[count]->Execute(pInputData);
        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Run IQ Config, count %d", count);
        }

        if (TRUE == adrcEnabled &&
                ISPIQModuleType::BPSGTM == m_pBPSIQModules[count]->GetIQType())
        {
            percentageOfGTM = pInputData->pCalculatedData->percentageOfGTM;
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "update the percentageOfGTM: %f", percentageOfGTM);
        }

    }

    for (count = 0; count < m_numBPSStatsModuleEnabled; count++)
    {
        result = m_pBPSStatsModules[count]->Execute(pInputData);
        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to Run Stats module Config, count %d", count);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::UpdateCommandBufferSize()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::UpdateCommandBufferSize()
{
    UINT32 numLUT           = 0;

    for (UINT count = 0; count < m_numBPSIQModuleEnabled; count++)
    {
        numLUT    = m_pBPSIQModules[count]->GetNumLUT();
        m_maxLUT += numLUT;
        SetIQModuleNumLUT(m_pBPSIQModules[count]->GetIQType(), numLUT);

        m_maxCmdBufferSizeBytes[BPSCmdBufferCDMProgram] += m_pBPSIQModules[count]->GetIQCmdLength();
    }

    // Not accounting for stats, as there is no register writes

    m_maxCmdBufferSizeBytes[BPSCmdBufferDMIHeader]    =
        PacketBuilder::RequiredWriteDMISizeInDwords() * m_maxLUT * sizeof(UINT32);
    m_maxCmdBufferSizeBytes[BPSCmdBufferIQSettings]   = sizeof(BpsIQSettings);
    // Update the command buffer size to be aligned to multiple of 4
    m_maxCmdBufferSizeBytes[BPSCmdBufferIQSettings]  += Utils::ByteAlign32(m_maxCmdBufferSizeBytes[BPSCmdBufferIQSettings], 4);
    m_maxCmdBufferSizeBytes[BPSCmdBufferFrameProcess] = Utils::ByteAlign32(BPSCmdBufferFrameProcessSizeBytes, 4);
    // Convert BPSCmdBufferCDMProgram size from dwords to bytes. API from IQ modules returns the size in words
    m_maxCmdBufferSizeBytes[BPSCmdBufferCDMProgram]  *= sizeof(UINT32);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::SetDependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::SetDependencies(
    ExecuteProcessRequestData*  pExecuteProcessRequestData,
    BOOL                        sensorConnected,
    BOOL                        statsConnected)
{
    NodeProcessRequestData* pNodeRequestData    = pExecuteProcessRequestData->pNodeProcessRequestData;
    UINT32                  count               = 0;

    // Set dependencies only when sensor is connected
    /// @todo (CAMX-1211) Need to add 3A dependency on AEC, AWB, AF Frame Control
    if (TRUE == sensorConnected && (TRUE == IsTagPresentInPublishList(PropertyIDSensorCurrentMode)))
    {
        pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency   = TRUE;
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count]    = PropertyIDSensorCurrentMode;
        count++;

        if (TRUE == statsConnected)
        {
            if (TRUE == IsTagPresentInPublishList(PropertyIDPostSensorGainId))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = PropertyIDPostSensorGainId;
                count++;
            }

            /// @todo (CAMX-3080) As we do not support YUV ZSL our dependencies applied to BPS should reflect current
            ///                frame. At switch point reset of previous data otherwise would cause failures.
            ///                We will debate more for future YUV support in BPS. (Hence real-time or not BPS acts
            ///                like offline. Only delta is source of data read.
            ///                As a result; offsets for read meta is left as 0.
            if (TRUE == IsTagPresentInPublishList(PropertyIDAECFrameControl))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = PropertyIDAECFrameControl;
                count++;
            }
            if (TRUE == IsTagPresentInPublishList(PropertyIDAWBFrameControl))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = PropertyIDAWBFrameControl;
                count++;
            }
            if (TRUE == IsTagPresentInPublishList(PropertyIDAECStatsControl))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = PropertyIDAECStatsControl;
                count++;
            }
            if (TRUE == IsTagPresentInPublishList(PropertyIDAWBStatsControl))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = PropertyIDAWBStatsControl;
                count++;
            }
            if (TRUE == IsTagPresentInPublishList(PropertyIDISPTintlessBGConfig))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = PropertyIDISPTintlessBGConfig;
                count++;
            }
            if (TRUE == IsTagPresentInPublishList(PropertyIDParsedTintlessBGStatsOutput))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] =
                    PropertyIDParsedTintlessBGStatsOutput;
                count++;
            }
            if (TRUE == IsTagPresentInPublishList(PropertyIDISPBHistConfig))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = PropertyIDISPBHistConfig;
                count++;
            }
            if (TRUE == IsTagPresentInPublishList(PropertyIDParsedBHistStatsOutput))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = PropertyIDParsedBHistStatsOutput;
                count++;
            }
        }

        if (BPSProfileId::BPSProfileIdHNR == m_instanceProperty.profileId)
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDIntermediateDimension;
        }

        if (count > 0)
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.count                            = count;
            pNodeRequestData->dependencyInfo[0].processSequenceId                                   = 1;
            pNodeRequestData->dependencyInfo[0].dependencyFlags.hasIOBufferAvailabilityDependency   = TRUE;
            pNodeRequestData->numDependencyLists                                                    = 1;
        }
    }

    if (TRUE == GetStaticSettings()->enableImageBufferLateBinding)
    {
        // If latebinding is enabled, we want to delay packet preparation as late as possible, in other terms, we want to
        // prepare and submit to hw when it can really start processing. This is once all the input fences (+ property)
        // dependencies are satisfied. So, lets set input fence dependencies.
        // But we could optimize further to prepare packet with IQ configuration early once Property dependencies are
        // satisfied and IOConfiguration after input fence dependencies are satisfied.

        /// @todo (CAMX-12345678) Prepare IQConfig, IOConfig in 2 steps and set needBuffersOnDependencyMet in last sequenceId
        //  IQ Config : Prepare once the required (property) dependencies for IQ configuration are satisfied (sequenceId 1).
        //  IO Config : If LateBinding is enabled, Prepare after all dependencies(property + fence) are satisfied (sequendeId 2)
        //              If not enabled, prepare IO Config once property dependencies are satisfied (sequenceId 1)

        UINT fenceCount = SetInputBuffersReadyDependency(pExecuteProcessRequestData, 0);

        if (0 < fenceCount)
        {
            pNodeRequestData->dependencyInfo[0].processSequenceId                                   = 1;
            pNodeRequestData->dependencyInfo[0].dependencyFlags.hasIOBufferAvailabilityDependency   = TRUE;
            pNodeRequestData->numDependencyLists                                                    = 1;
        }
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::FillIQSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::FillIQSetting(
    ISPInputData*  pInputData,
    BpsIQSettings* pBPSIQsettings)
{

    /// @todo (CAMX-775) Implement BPS IQ modules
    if (NULL != pBPSIQsettings)
    {
        CamX::Utils::Memset(pBPSIQsettings, 0, sizeof(BpsIQSettings));

        // BPS IQ quater scaling parameters
        TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
        CAMX_ASSERT(NULL != pTuningManager);

        ds4to1_1_0_0::chromatix_ds4to1v10Type* pChromatix1 = NULL;
        ds4to1_1_0_0::chromatix_ds4to1v10Type* pChromatix2 = NULL;
        if (TRUE == pTuningManager->IsValidChromatix())
        {
            pChromatix1 = pTuningManager->GetChromatix()->GetModule_ds4to1v10_bps_full_dc4(
                reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                pInputData->pTuningData->noOfSelectionParameter);
            pChromatix2 = pTuningManager->GetChromatix()->GetModule_ds4to1v10_bps_dc4_dc16(
                reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                pInputData->pTuningData->noOfSelectionParameter);

            CAMX_ASSERT(NULL != pChromatix1 && NULL != pChromatix2);
            if (NULL != pChromatix1 && NULL != pChromatix2)
            {
                INT pass_dc64 = static_cast<INT>(ispglobalelements::trigger_pass::PASS_DC64);
                INT pass_dc16 = static_cast<INT>(ispglobalelements::trigger_pass::PASS_DC16);
                INT pass_dc4 = static_cast<INT>(ispglobalelements::trigger_pass::PASS_DC4);

                ds4to1_1_0_0::chromatix_ds4to1v10_reserveType*  pReserveData1 = &pChromatix1->chromatix_ds4to1v10_reserve;
                ds4to1_1_0_0::chromatix_ds4to1v10_reserveType*  pReserveData2 = &pChromatix2->chromatix_ds4to1v10_reserve;
                pBPSIQsettings->sixtyFourthScalingParameters.coefficient7 =
                    pReserveData2->mod_ds4to1v10_pass_reserve_data[pass_dc64].pass_data.coeff_07;
                pBPSIQsettings->sixtyFourthScalingParameters.coefficient16 =
                    pReserveData2->mod_ds4to1v10_pass_reserve_data[pass_dc64].pass_data.coeff_16;
                pBPSIQsettings->sixtyFourthScalingParameters.coefficient25 =
                    pReserveData2->mod_ds4to1v10_pass_reserve_data[pass_dc64].pass_data.coeff_25;;

                pBPSIQsettings->sixteenthScalingParameters.coefficient7 =
                    pReserveData2->mod_ds4to1v10_pass_reserve_data[pass_dc16].pass_data.coeff_07;
                pBPSIQsettings->sixteenthScalingParameters.coefficient16 =
                    pReserveData2->mod_ds4to1v10_pass_reserve_data[pass_dc16].pass_data.coeff_16;
                pBPSIQsettings->sixteenthScalingParameters.coefficient25 =
                    pReserveData2->mod_ds4to1v10_pass_reserve_data[pass_dc16].pass_data.coeff_25;

                pBPSIQsettings->quarterScalingParameters.coefficient7 =
                    pReserveData1->mod_ds4to1v10_pass_reserve_data[pass_dc4].pass_data.coeff_07;
                pBPSIQsettings->quarterScalingParameters.coefficient16 =
                    pReserveData1->mod_ds4to1v10_pass_reserve_data[pass_dc4].pass_data.coeff_16;
                pBPSIQsettings->quarterScalingParameters.coefficient25 =
                    pReserveData1->mod_ds4to1v10_pass_reserve_data[pass_dc4].pass_data.coeff_25;
            }
        }

        if (NULL == pChromatix1 || NULL == pChromatix2)
        {
            pBPSIQsettings->quarterScalingParameters.coefficient7 = 125;
            pBPSIQsettings->quarterScalingParameters.coefficient16 = 91;
            pBPSIQsettings->quarterScalingParameters.coefficient25 = 144;

            pBPSIQsettings->sixteenthScalingParameters.coefficient7 = 125;
            pBPSIQsettings->sixteenthScalingParameters.coefficient16 = 91;
            pBPSIQsettings->sixteenthScalingParameters.coefficient25 = 144;

            pBPSIQsettings->sixtyFourthScalingParameters.coefficient7 = 125;
            pBPSIQsettings->sixtyFourthScalingParameters.coefficient16 = 91;
            pBPSIQsettings->sixtyFourthScalingParameters.coefficient25 = 144;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                         "DC64: Coeff07 = %d, Coeff16 = %d, Coeff25 = %d",
                         pBPSIQsettings->sixtyFourthScalingParameters.coefficient7,
                         pBPSIQsettings->sixtyFourthScalingParameters.coefficient16,
                         pBPSIQsettings->sixtyFourthScalingParameters.coefficient25);

        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                         "DC16: Coeff07 = %d, Coeff16 = %d, Coeff25 = %d",
                         pBPSIQsettings->sixteenthScalingParameters.coefficient7,
                         pBPSIQsettings->sixteenthScalingParameters.coefficient16,
                         pBPSIQsettings->sixteenthScalingParameters.coefficient25);

        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                         "DC4: Coeff07 = %d, Coeff16 = %d, Coeff25 = %d",
                         pBPSIQsettings->quarterScalingParameters.coefficient7,
                         pBPSIQsettings->quarterScalingParameters.coefficient16,
                         pBPSIQsettings->quarterScalingParameters.coefficient25);

        // Chroma Subsample Parameters
        pBPSIQsettings->chromaSubsampleParameters.horizontalRoundingOption = 1;
        pBPSIQsettings->chromaSubsampleParameters.verticalRoundingOption   = 2;

        // BPS IQ full output Round and Clamp parameters
        const ImageFormat* pImageFormat = GetOutputPortImageFormat(OutputPortIndex(CSLBPSOutputPortIdPDIImageFull));
        uint32_t           clampMax     = Max8BitValue;
        if (TRUE == ImageFormatUtils::Is10BitFormat(pImageFormat->format))
        {
            clampMax     = Max10BitValue;
        }
        pBPSIQsettings->fullRoundAndClampParameters.lumaClampParameters.clampMin                      = 0;
        pBPSIQsettings->fullRoundAndClampParameters.lumaClampParameters.clampMax                      = clampMax;
        pBPSIQsettings->fullRoundAndClampParameters.chromaClampParameters.clampMin                    = 0;
        pBPSIQsettings->fullRoundAndClampParameters.chromaClampParameters.clampMax                    = clampMax;
        pBPSIQsettings->fullRoundAndClampParameters.lumaRoundParameters.roundingPattern               = 0;
        pBPSIQsettings->fullRoundAndClampParameters.chromaRoundParameters.roundingPattern             = 0;

        // BPS IQ DS4 output Round and Clamp parameters
        pBPSIQsettings->quarterRoundAndClampParameters.lumaClampParameters.clampMin                   = 0;
        pBPSIQsettings->quarterRoundAndClampParameters.lumaClampParameters.clampMax                   = Max10BitValue;
        pBPSIQsettings->quarterRoundAndClampParameters.chromaClampParameters.clampMin                 = 0;
        pBPSIQsettings->quarterRoundAndClampParameters.chromaClampParameters.clampMax                 = Max10BitValue;
        pBPSIQsettings->quarterRoundAndClampParameters.lumaRoundParameters.roundingPattern            = 0;
        pBPSIQsettings->quarterRoundAndClampParameters.chromaRoundParameters.roundingPattern          = 0;

        // BPS IQ DS16 output Round and Clamp parameters
        pBPSIQsettings->sixteenthRoundAndClampParameters.lumaClampParameters.clampMin                 = 0;
        pBPSIQsettings->sixteenthRoundAndClampParameters.lumaClampParameters.clampMax                 = Max10BitValue;
        pBPSIQsettings->sixteenthRoundAndClampParameters.chromaClampParameters.clampMin               = 0;
        pBPSIQsettings->sixteenthRoundAndClampParameters.chromaClampParameters.clampMax               = Max10BitValue;
        pBPSIQsettings->sixteenthRoundAndClampParameters.lumaRoundParameters.roundingPattern          = 0;
        pBPSIQsettings->sixteenthRoundAndClampParameters.chromaRoundParameters.roundingPattern        = 0;

        // BPS IQ DS64 output Round and Clamp parameters
        pBPSIQsettings->sixtyFourthRoundAndClampParameters.lumaClampParameters.clampMin               = 0;
        pBPSIQsettings->sixtyFourthRoundAndClampParameters.lumaClampParameters.clampMax               = Max10BitValue;
        pBPSIQsettings->sixtyFourthRoundAndClampParameters.chromaClampParameters.clampMin             = 0;
        pBPSIQsettings->sixtyFourthRoundAndClampParameters.chromaClampParameters.clampMax             = Max10BitValue;
        pBPSIQsettings->sixtyFourthRoundAndClampParameters.lumaRoundParameters.roundingPattern        = 0;
        pBPSIQsettings->sixtyFourthRoundAndClampParameters.chromaRoundParameters.roundingPattern      = 0;

        // BPS IQ registration1 output Round and Clamp parameters
        pBPSIQsettings->registration1RoundAndClampParameters.lumaClampParameters.clampMin             = 0;
        pBPSIQsettings->registration1RoundAndClampParameters.lumaClampParameters.clampMax             = Max8BitValue;
        pBPSIQsettings->registration1RoundAndClampParameters.chromaClampParameters.clampMin           = 0;
        pBPSIQsettings->registration1RoundAndClampParameters.chromaClampParameters.clampMax           = Max8BitValue;
        pBPSIQsettings->registration1RoundAndClampParameters.lumaRoundParameters.roundingPattern      = 0;
        pBPSIQsettings->registration1RoundAndClampParameters.chromaRoundParameters.roundingPattern    = 0;

        // BPS IQ registration2 output Round and Clamp parameters
        pBPSIQsettings->registration2RoundAndClampParameters.lumaClampParameters.clampMin             = 0;
        pBPSIQsettings->registration2RoundAndClampParameters.lumaClampParameters.clampMax             = Max8BitValue;
        pBPSIQsettings->registration2RoundAndClampParameters.chromaClampParameters.clampMin           = 0;
        pBPSIQsettings->registration2RoundAndClampParameters.chromaClampParameters.clampMax           = Max8BitValue;
        pBPSIQsettings->registration2RoundAndClampParameters.lumaRoundParameters.roundingPattern      = 0;
        pBPSIQsettings->registration2RoundAndClampParameters.chromaRoundParameters.roundingPattern    = 0;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "BpsIQSettings: failed to add patch");
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::FillFrameBufferData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::FillFrameBufferData(
    CmdBuffer*      pFrameProcessCmdBuffer,
    ImageBuffer*    pImageBuffer,
    UINT32          portId)
{
    CamxResult      result         = CamxResultSuccess;
    SIZE_T          planeOffset    = 0;
    SIZE_T          metadataSize   = 0;
    CSLMemHandle    hMem;

    // Prepare Patching struct for SMMU addresses
    for (UINT32 plane = 0; (plane < MAX_NUM_OF_IMAGE_PLANES) && (plane < pImageBuffer->GetNumberOfPlanes()); plane++)
    {
        result = pImageBuffer->GetPlaneCSLMemHandle(0, plane, &hMem, &planeOffset, &metadataSize);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid args pImageBuffer=%p, hMem=%x plane=%d, planeOffset %d metadataSize=%d",
                pImageBuffer, hMem, plane, planeOffset, metadataSize);
        }
        const ImageFormat* pFormat = pImageBuffer->GetFormat();
        if (NULL == pFormat)
        {
            result = CamxResultEInvalidPointer;
            CAMX_LOG_ERROR(CamxLogGroupPProc, "pFormat is NULL");
            break;
        }

        if (TRUE == ImageFormatUtils::IsUBWC(pFormat->format))
        {
            result = pFrameProcessCmdBuffer->AddNestedBufferInfo(s_frameBufferOffset[portId].metadataBufferPtr[plane],
                                                                 hMem,
                                                                 static_cast <UINT32>(planeOffset));
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Error in patching address portId %d plane %d", portId, plane);
                break;
            }
        }

        result = pFrameProcessCmdBuffer->AddNestedBufferInfo(s_frameBufferOffset[portId].bufferPtr[plane],
                                                             hMem,
                                                             (static_cast <UINT32>(planeOffset) +
                                                              static_cast <UINT32>(metadataSize)));
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Error in patching address portId %d plane %d", portId, plane);
            break;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetCDMProgramOffset
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INT BPSNode::GetCDMProgramOffset(
    UINT CDMProgramIndex)
{
    UINT CDMProgramOffset = sizeof(BpsFrameProcess)                 +
                            sizeof(CDMProgramArray)                 +
                            offsetof(CdmProgram, cdmBaseAndLength)  +
                            offsetof(CDM_BASE_LENGHT, bitfields);

    return ((sizeof(CdmProgram) * CDMProgramIndex) + CDMProgramOffset);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::FillCDMProgram
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::FillCDMProgram(
    CmdBuffer**             ppBPSCmdBuffer,
    CDMProgramArray*        pCDMProgramArray,
    CdmProgram*             pCDMProgram,
    UINT32                  programType,
    UINT32                  programIndex)
{
    CamxResult result           = CamxResultSuccess;
    UINT32     singleHeaderSize = (cdm_get_cmd_header_size(CDMCmdDMI) * RegisterWidthInBytes);

    if ((m_LUTCnt[programIndex] > 0) && (TRUE == m_moduleChromatixEnable[programIndex]))
    {

        UINT numPrograms                                 = pCDMProgramArray->numPrograms;
        pCDMProgram                                      = &pCDMProgramArray->programs[numPrograms];
        pCDMProgram->hasSingleReg                        = 0;
        pCDMProgram->programType                         = programType;
        pCDMProgram->uID                                 = 0;
        pCDMProgram->cdmBaseAndLength.bitfields.LEN      = (m_LUTCnt[programIndex] * singleHeaderSize) - 1;
        pCDMProgram->cdmBaseAndLength.bitfields.RESERVED = 0;
        pCDMProgram->bufferAllocatedInternally           = 0;

        /// @todo (CAMX-1033) Change below numPrograms to ProgramIndex once firmware support of program skip is available.
        INT dstOffset = GetCDMProgramOffset(numPrograms - 1);
        CAMX_ASSERT(dstOffset >= 0);

        result = ppBPSCmdBuffer[BPSCmdBufferFrameProcess]->AddNestedCmdBufferInfo(dstOffset,
                                                                                  ppBPSCmdBuffer[BPSCmdBufferDMIHeader],
                                                                                  m_LUTOffset[programIndex]);
        (pCDMProgramArray->numPrograms)++;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::FillFrameCDMArray
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::FillFrameCDMArray(
    CmdBuffer**          ppBPSCmdBuffer,
    BpsFrameProcessData* pFrameProcessData)
{
    CamxResult          result           = CamxResultSuccess;
    UINT                offset           = 0;
    CDMProgramArray*    pCDMProgramArray = NULL;
    CdmProgram*         pCDMProgram      = NULL;
    UINT32*             pCDMPayload      = reinterpret_cast<UINT32*>(pFrameProcessData);
    UINT32              dstoffset        = 0;
    UINT32              srcOffset        = 0;
    UINT                numPrograms      = 0;
    UINT32              programIndex     = BPSProgramIndexPEDESTAL;
    UINT32              programType      = BPS_PEDESTAL_LUT_PROGRAM;

    CAMX_ASSERT(NULL != pFrameProcessData);
    CAMX_ASSERT(NULL != ppBPSCmdBuffer);

    if ((NULL == pFrameProcessData) || (NULL == ppBPSCmdBuffer))
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Cmd Buffer.");
    }
    if (CamxResultSuccess == result)
    {
        // Patch IQSettings buffer in BpsFrameProcessData
        offset = static_cast<UINT32>(offsetof(BpsFrameProcess, cmdData)) +
                 static_cast<UINT32>(offsetof(BpsFrameProcessData, iqSettingsAddr));
        result = ppBPSCmdBuffer[BPSCmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset,
                                                                                  ppBPSCmdBuffer[BPSCmdBufferIQSettings],
                                                                                  0);
    }

    if (CamxResultSuccess == result)
    {
        dstoffset = static_cast <UINT32>(offsetof(BpsFrameProcess, cmdData) +
                    offsetof(BpsFrameProcessData, cdmProgramArrayAddr));
        srcOffset = sizeof(BpsFrameProcess);
        result    = ppBPSCmdBuffer[BPSCmdBufferFrameProcess]->AddNestedCmdBufferInfo(dstoffset,
                                                                                     ppBPSCmdBuffer[BPSCmdBufferFrameProcess],
                                                                                     srcOffset);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Patching failed for IQSettings / CDM program");
    }

    if (CamxResultSuccess == result)
    {
        // Patch CDM Cmd Buffer(BPS module registers)
        pCDMProgramArray              =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload + (sizeof(BpsFrameProcess) / RegisterWidthInBytes));
        pCDMProgramArray->allocator   = 0;
        // One for CDMProgramArray and one each for LUT
        pCDMProgramArray->numPrograms = 0;

        numPrograms                                      = pCDMProgramArray->numPrograms;
        pCDMProgram                                      = &pCDMProgramArray->programs[numPrograms];
        pCDMProgram->hasSingleReg                        = 0;
        pCDMProgram->programType                         = PROGRAM_TYPE_GENERIC;
        pCDMProgram->uID                                 = 0;
        pCDMProgram->cdmBaseAndLength.bitfields.LEN      =
            (ppBPSCmdBuffer[BPSCmdBufferCDMProgram]->GetResourceUsedDwords() * RegisterWidthInBytes) - 1;
        pCDMProgram->cdmBaseAndLength.bitfields.RESERVED = 0;
        pCDMProgram->bufferAllocatedInternally           = 0;

        // fetch the BASE offset in CdmProgram to patch with pCDMCmdBuffer
        dstoffset =
            sizeof(BpsFrameProcess) +
            offsetof(CDMProgramArray, programs) +
            offsetof(CdmProgram, cdmBaseAndLength) +
            offsetof(CDM_BASE_LENGHT, bitfields);

        // Patch cdmProgramArrayAddr with CDMProgramArray CdmProgram base
        result = ppBPSCmdBuffer[BPSCmdBufferFrameProcess]->AddNestedCmdBufferInfo(dstoffset,
                                                                                  ppBPSCmdBuffer[BPSCmdBufferCDMProgram],
                                                                                  0);
        (pCDMProgramArray->numPrograms)++;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Patching failed for CDM Program Array");
    }

    if (CamxResultSuccess == result)
    {
        for (programType = BPS_PEDESTAL_LUT_PROGRAM; programType <= BPS_HNR_LUT_PROGRAM; programType++, programIndex++)
        {
            result = FillCDMProgram(ppBPSCmdBuffer,
                                    pCDMProgramArray,
                                    pCDMProgram,
                                    programType,
                                    programIndex);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed CDMProgram for DMI %u", programType);
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Patching failed for Generic IQ Settings");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::CreateFWCommandBufferManagers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::CreateFWCommandBufferManagers()
{
    ResourceParams               resourceParams[BPSMaxFWCmdBufferManagers];
    CHAR                         bufferManagerName[BPSMaxFWCmdBufferManagers][MaxStringLength256];
    struct CmdBufferManagerParam createParam[BPSMaxFWCmdBufferManagers];
    UINT32                       numberOfBufferManagers = 0;
    CamxResult                   result                 = CamxResultSuccess;

    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                        m_maxCmdBufferSizeBytes[BPSCmdBufferStriping],
                        CmdType::FW,
                        CSLMemFlagUMDAccess,
                        0,
                        DeviceIndices(),
                        m_BPSCmdBlobCount);
    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                      sizeof(CHAR) * MaxStringLength256,
                      "CBM_%s_%s",
                      NodeIdentifierString(),
                      "CmdBufferStriping");
    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pBPSCmdBufferManager[BPSCmdBufferStriping];

    numberOfBufferManagers++;

    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                        m_maxCmdBufferSizeBytes[BPSCmdBufferBLMemory],
                        CmdType::FW,
                        CSLMemFlagUMDAccess,
                        0,
                        DeviceIndices(),
                        m_BPSCmdBlobCount);
    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                      sizeof(CHAR) * MaxStringLength256,
                      "CBM_%s_%s",
                      NodeIdentifierString(),
                      "CmdBufferBLMemory");
    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pBPSCmdBufferManager[BPSCmdBufferBLMemory];

    numberOfBufferManagers++;

    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                        m_maxCmdBufferSizeBytes[BPSCmdBufferFrameProcess],
                        CmdType::FW,
                        CSLMemFlagUMDAccess,
                        BPSMaxPatchAddress,
                        DeviceIndices(),
                        m_BPSCmdBlobCount);
    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                      sizeof(CHAR) * MaxStringLength256,
                      "CBM_%s_%s",
                      NodeIdentifierString(),
                      "CmdBufferFrameProcess");
    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pBPSCmdBufferManager[BPSCmdBufferFrameProcess];

    numberOfBufferManagers++;

    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                        m_maxCmdBufferSizeBytes[BPSCmdBufferIQSettings],
                        CmdType::FW,
                        CSLMemFlagUMDAccess,
                        0,
                        DeviceIndices(),
                        m_BPSCmdBlobCount);
    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                      sizeof(CHAR) * MaxStringLength256,
                      "CBM_%s_%s",
                      NodeIdentifierString(),
                      "CmdBufferIQSettings");
    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pBPSCmdBufferManager[BPSCmdBufferIQSettings];

    numberOfBufferManagers++;

    if (numberOfBufferManagers > BPSMaxFWCmdBufferManagers)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Maximum FW Commadn Buffers recahed");
        result = CamxResultEFailed;
    }
    if ((CamxResultSuccess == result) &&  (0 != numberOfBufferManagers))
    {
        result = CreateMultiCmdBufferManager(createParam, numberOfBufferManagers);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "FW Command Buffer creation Failed %d", result);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::InitializeStripingParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::InitializeStripingParams(
    BpsConfigIoData* pConfigIOData)
{
    CamxResult     result          = CamxResultSuccess;
    ResourceParams params          = { 0 };
    UINT32         titanVersion    = static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion();
    UINT32         hwVersion       = static_cast<Titan17xContext *>(GetHwContext())->GetHwVersion();

    CAMX_ASSERT(NULL != pConfigIOData);

    // Check if striping in umd is enabled before creating striping library context
    result = BPSStripingLibraryContextCreate(pConfigIOData,
                                             NULL,
                                             titanVersion,
                                             hwVersion,
                                             &m_hStripingLib,
                                             &m_maxCmdBufferSizeBytes[BPSCmdBufferStriping],
                                             &m_maxCmdBufferSizeBytes[BPSCmdBufferBLMemory]);

    if (CamxResultSuccess == result)
    {
        result =  CreateFWCommandBufferManagers();
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "StripingLib ctxt create failed %d configIO %p titanVersion 0x%x hwVersion 0x%x",
                           result, pConfigIOData, titanVersion, hwVersion);
        result = CamxResultEFailed;
    }
    return result;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::DeInitializeStripingParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::DeInitializeStripingParams()
{
    CamxResult  result = CamxResultSuccess;

    if (NULL != m_hStripingLib)
    {
        INT32 rc = BPSStripingLibraryContextDestroy(&m_hStripingLib);
        if (rc != 0)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "BPS:%d Cannot destroy Striping Library with rc=%d", InstanceID(), rc);
            result = CamxResultEFailed;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::FillStripingParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::FillStripingParams(
    BpsFrameProcessData*         pFrameProcessData,
    BpsIQSettings*               pBPSIQsettings,
    CmdBuffer**                  ppBPSCmdBuffer,
    CSLICPClockBandwidthRequest* pICPClockBandwidthRequest)
{
    CamxResult                     result                       = CamxResultSuccess;
    UINT32*                        pStripeMem                   = NULL;
    UINT32                         offset                       = 0;
    CmdBuffer*                     pBPSStripingCmdBuffer        = ppBPSCmdBuffer[BPSCmdBufferStriping];
    CmdBuffer*                     pBPSFrameProcessCmdBuffer    = ppBPSCmdBuffer[BPSCmdBufferFrameProcess];
    BPSStripingLibExecuteParams    stripeParams                 = { 0 };
    BPSStripingLibExecuteMetaData  metaDataBuffer               = { 0 };
    userBPSArgs                    userBPSArgs                  = { 0 };
    CAMX_ASSERT(NULL != pFrameProcessData);
    if (NULL == pFrameProcessData)
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupISP, "BPS Frame Process Data is NULL.");
    }
    if (CamxResultSuccess == result)
    {
        CAMX_ASSERT(NULL != pBPSIQsettings);
        CAMX_ASSERT(NULL != ppBPSCmdBuffer);
        CAMX_ASSERT(NULL != pBPSStripingCmdBuffer);

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, " node %s , profile %d,processingtype %d,pedestal %d,"
                      "linear %d,pdpc %d,hdr %d,hdrmac %d,gic %d"
                      "abf %d, lens %d, demosaic %d, bayergrid %d, bayerhisto %d colorC %d, gtm %d, glut %d",
                      "colorXf %d, hnr %d",
                      NodeIdentifierString(),
                      m_instanceProperty.profileId, m_instanceProperty.processingType,
                      pBPSIQsettings->pedestalParameters.moduleCfg.EN,
                      pBPSIQsettings->linearizationParameters.moduleCfg.EN,
                      pBPSIQsettings->pdpcParameters.moduleCfg.EN,
                      pBPSIQsettings->hdrReconParameters.moduleCfg.EN,
                      pBPSIQsettings->hdrMacParameters.moduleCfg.EN,
                      pBPSIQsettings->gicParameters.moduleCfg.EN,
                      pBPSIQsettings->abfParameters.moduleCfg.EN,
                      pBPSIQsettings->lensRollOffParameters.moduleCfg.EN,
                      pBPSIQsettings->demosaicParameters.moduleCfg.EN,
                      pBPSIQsettings->bayerGridParameters.moduleCfg.EN,
                      pBPSIQsettings->bayerHistogramParameters.moduleCfg.EN,
                      pBPSIQsettings->colorCorrectParameters.moduleCfg.EN,
                      pBPSIQsettings->gtmParameters.moduleCfg.EN,
                      pBPSIQsettings->glutParameters.moduleCfg.EN,
                      pBPSIQsettings->colorXformParameters.moduleCfg.EN,
                      pBPSIQsettings->hnrParameters.moduleCfg.EN);

        pFrameProcessData->targetCompletionTimeInNs = 0;

        pStripeMem = reinterpret_cast<UINT32*>(pBPSStripingCmdBuffer->BeginCommands(
            m_maxCmdBufferSizeBytes[BPSCmdBufferStriping] / sizeof(UINT32)));
    }
#if DEBUG
    DumpConfigIOData();
#endif  // DumpConfigIOData

    if ((NULL != pStripeMem) && (CamxResultSuccess == result))
    {
        stripeParams.iq                 = pBPSIQsettings;
        stripeParams.maxNumOfCoresToUse = pFrameProcessData->maxNumOfCoresToUse;

        userBPSArgs.frameNumber         = pFrameProcessData->requestId;
        userBPSArgs.instance            = InstanceID();
        userBPSArgs.processingType      = m_instanceProperty.processingType;
        userBPSArgs.profileID           = m_instanceProperty.profileId;
        userBPSArgs.realTime            = IsRealTime();
        userBPSArgs.FileDumpPath        = FileDumpPath;
        userBPSArgs.dumpEnable          = m_BPSStripeDumpEnable;

        result = BPSStripingLibraryExecute(m_hStripingLib, &stripeParams, pStripeMem, &metaDataBuffer, &userBPSArgs);
        if (CamxResultSuccess == result)
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "Striping Library execution completed");
            offset =
                static_cast<UINT32>(offsetof(BpsFrameProcess, cmdData)) +
                static_cast<UINT32>(offsetof(BpsFrameProcessData, stripingLibOutAddr));
            result = pBPSFrameProcessCmdBuffer->AddNestedCmdBufferInfo(offset, pBPSStripingCmdBuffer, 0);
            pICPClockBandwidthRequest->frameCycles = metaDataBuffer.pixelCount;
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Num pixels = %d", metaDataBuffer.pixelCount);
        }
        else
        {
            /// @todo (CAMX-1732) To add transalation for firmware errors
            result = CamxResultEFailed;
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Striping Library execution failed %d", result);
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid Striping memory");
        result = CamxResultENoMemory;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::InitializeBPSIQSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE VOID BPSNode::InitializeBPSIQSettings(
    BpsIQSettings*               pBPSIQsettings)
{

    pBPSIQsettings->pedestalParameters.moduleCfg.EN         = 0;
    pBPSIQsettings->linearizationParameters.moduleCfg.EN    = 0;
    pBPSIQsettings->pdpcParameters.moduleCfg.EN             = 0;
    pBPSIQsettings->hdrReconParameters.moduleCfg.EN         = 0;
    pBPSIQsettings->hdrMacParameters.moduleCfg.EN           = 0;
    pBPSIQsettings->gicParameters.moduleCfg.EN              = 0;
    pBPSIQsettings->abfParameters.moduleCfg.EN              = 0;
    pBPSIQsettings->lensRollOffParameters.moduleCfg.EN      = 0;
    pBPSIQsettings->demosaicParameters.moduleCfg.EN         = 0;
    pBPSIQsettings->bayerGridParameters.moduleCfg.EN        = 0;
    pBPSIQsettings->bayerHistogramParameters.moduleCfg.EN   = 0;
    pBPSIQsettings->colorCorrectParameters.moduleCfg.EN     = 0;
    pBPSIQsettings->glutParameters.moduleCfg.EN             = 0;
    pBPSIQsettings->colorXformParameters.moduleCfg.EN       = 0;
    pBPSIQsettings->hnrParameters.moduleCfg.EN              = 0;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::PatchBLMemoryBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::PatchBLMemoryBuffer(
    BpsFrameProcessData* pFrameProcessData,
    CmdBuffer**          ppBPSCmdBuffer)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != ppBPSCmdBuffer[BPSCmdBufferBLMemory]) && (0 != m_maxCmdBufferSizeBytes[BPSCmdBufferBLMemory]))
    {
        pFrameProcessData->cdmBufferSize = m_maxCmdBufferSizeBytes[BPSCmdBufferBLMemory];

        UINT  offset =
            static_cast<UINT32>(offsetof(BpsFrameProcess, cmdData)) +
            static_cast<UINT32>(offsetof(BpsFrameProcessData, cdmBufferAddress));

        result = ppBPSCmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset,
                                                                               ppBPSCmdBuffer[BPSCmdBufferBLMemory],
                                                                               0);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::CommitAllCommandBuffers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::CommitAllCommandBuffers(
    CmdBuffer** ppBPSCmdBuffer)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != ppBPSCmdBuffer[BPSCmdBufferStriping])
    {
        result = ppBPSCmdBuffer[BPSCmdBufferStriping]->CommitCommands();
    }

    if (CamxResultSuccess == result)
    {
        result = ppBPSCmdBuffer[BPSCmdBufferIQSettings]->CommitCommands();
    }

    if (CamxResultSuccess == result)
    {
        ppBPSCmdBuffer[BPSCmdBufferFrameProcess]->CommitCommands();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::SetIQModuleNumLUT
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BPSNode::SetIQModuleNumLUT(
    ISPIQModuleType type,
    UINT            numLUTs)
{
    switch (type)
    {
        case ISPIQModuleType::BPSPedestalCorrection:
            m_LUTCnt[BPSProgramIndexPEDESTAL] = numLUTs;
            break;
        case ISPIQModuleType::BPSLinearization:
            m_LUTCnt[BPSProgramIndexLINEARIZATION] = numLUTs;
            break;
        case ISPIQModuleType::BPSBPCPDPC:
            m_LUTCnt[BPSProgramIndexBPCPDPC] = numLUTs;
            break;
        case ISPIQModuleType::BPSABF:
            m_LUTCnt[BPSProgramIndexABF] = numLUTs;
            break;
        case ISPIQModuleType::BPSLSC:
            m_LUTCnt[BPSProgramIndexRolloff] = numLUTs;
            break;
        case ISPIQModuleType::BPSGIC:
            m_LUTCnt[BPSProgramIndexGIC] = numLUTs;
            break;
        case ISPIQModuleType::BPSGTM:
            m_LUTCnt[BPSProgramIndexGTM] = numLUTs;
            break;
        case ISPIQModuleType::BPSGamma:
            m_LUTCnt[BPSProgramIndexGLUT] = numLUTs;
            break;
        case ISPIQModuleType::BPSHNR:
            m_LUTCnt[BPSProgramIndexHNR] = numLUTs;
            break;
        case ISPIQModuleType::BPSDemux:
        case ISPIQModuleType::BPSHDR:
        case ISPIQModuleType::BPSDemosaic:
        case ISPIQModuleType::BPSCC:
        case ISPIQModuleType::BPSCST:
        case ISPIQModuleType::BPSChromaSubSample:
        case ISPIQModuleType::BPSWB:
            // These IQ Modules does not have LUTs to program
            break;
        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported BPS IQ Module type");
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::SetIQModuleLUTOffset
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BPSNode::SetIQModuleLUTOffset(
    ISPIQModuleType type,
    BpsIQSettings*  pBPSIQSettings,
    UINT            LUTOffset,
    UINT32*         pLUTCount)
{
    switch (type)
    {
        case ISPIQModuleType::BPSPedestalCorrection:
            m_moduleChromatixEnable[BPSProgramIndexPEDESTAL] = pBPSIQSettings->pedestalParameters.moduleCfg.EN;
            if (TRUE == m_moduleChromatixEnable[BPSProgramIndexPEDESTAL])
            {
                m_LUTOffset[BPSProgramIndexPEDESTAL] = LUTOffset;
                *pLUTCount                           = m_LUTCnt[BPSProgramIndexPEDESTAL];
            }
            break;
        case ISPIQModuleType::BPSLinearization:
            m_moduleChromatixEnable[BPSProgramIndexLINEARIZATION] = pBPSIQSettings->linearizationParameters.moduleCfg.EN;
            if (TRUE == m_moduleChromatixEnable[BPSProgramIndexLINEARIZATION])
            {
                m_LUTOffset[BPSProgramIndexLINEARIZATION] = LUTOffset;
                *pLUTCount                                = m_LUTCnt[BPSProgramIndexLINEARIZATION];
            }
            break;
        case ISPIQModuleType::BPSBPCPDPC:
            m_moduleChromatixEnable[BPSProgramIndexBPCPDPC] = pBPSIQSettings->pdpcParameters.moduleCfg.PDAF_PDPC_EN;
            if (TRUE == m_moduleChromatixEnable[BPSProgramIndexBPCPDPC])
            {
                m_LUTOffset[BPSProgramIndexBPCPDPC] = LUTOffset;
                *pLUTCount                          = m_LUTCnt[BPSProgramIndexBPCPDPC];
            }
            break;
        case ISPIQModuleType::BPSABF:
            m_moduleChromatixEnable[BPSProgramIndexABF] = pBPSIQSettings->abfParameters.moduleCfg.EN;
            if (TRUE == m_moduleChromatixEnable[BPSProgramIndexABF])
            {
                m_LUTOffset[BPSProgramIndexABF] = LUTOffset;
                *pLUTCount                      = m_LUTCnt[BPSProgramIndexABF];
            }
            break;
        case ISPIQModuleType::BPSLSC:
            m_moduleChromatixEnable[BPSProgramIndexRolloff] = pBPSIQSettings->lensRollOffParameters.moduleCfg.EN;
            if (TRUE == m_moduleChromatixEnable[BPSProgramIndexRolloff])
            {
                m_LUTOffset[BPSProgramIndexRolloff] = LUTOffset;
                *pLUTCount                          = m_LUTCnt[BPSProgramIndexRolloff];
            }
            break;
        case ISPIQModuleType::BPSGIC:
            m_moduleChromatixEnable[BPSProgramIndexGIC] = pBPSIQSettings->gicParameters.moduleCfg.EN;
            if (TRUE == m_moduleChromatixEnable[BPSProgramIndexGIC])
            {
                m_LUTOffset[BPSProgramIndexGIC] = LUTOffset;
                *pLUTCount                      = m_LUTCnt[BPSProgramIndexGIC];
            }
            break;
        case ISPIQModuleType::BPSGTM:
            m_moduleChromatixEnable[BPSProgramIndexGTM] = pBPSIQSettings->gtmParameters.moduleCfg.EN;
            if (TRUE == m_moduleChromatixEnable[BPSProgramIndexGTM])
            {
                m_LUTOffset[BPSProgramIndexGTM] = LUTOffset;
                *pLUTCount                      = m_LUTCnt[BPSProgramIndexGTM];
            }
            break;
        case ISPIQModuleType::BPSGamma:
            m_moduleChromatixEnable[BPSProgramIndexGLUT] = pBPSIQSettings->glutParameters.moduleCfg.EN;
            if (TRUE == m_moduleChromatixEnable[BPSProgramIndexGLUT])
            {
                m_LUTOffset[BPSProgramIndexGLUT] = LUTOffset;
                *pLUTCount                       = m_LUTCnt[BPSProgramIndexGLUT];
            }
            break;
        case ISPIQModuleType::BPSHNR:
            m_moduleChromatixEnable[BPSProgramIndexHNR] = pBPSIQSettings->hnrParameters.moduleCfg.EN;
            if (TRUE == m_moduleChromatixEnable[BPSProgramIndexHNR])
            {
                m_LUTOffset[BPSProgramIndexHNR] = LUTOffset;
                *pLUTCount                      = m_LUTCnt[BPSProgramIndexHNR];
            }
            break;
        case ISPIQModuleType::BPSDemux:
        case ISPIQModuleType::BPSHDR:
        case ISPIQModuleType::BPSDemosaic:
        case ISPIQModuleType::BPSCC:
        case ISPIQModuleType::BPSCST:
        case ISPIQModuleType::BPSChromaSubSample:
        case ISPIQModuleType::BPSWB:
            // These IQ Modules does not have LUTs to program
            break;
        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported BPS IQ Module type");
            break;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::UpdateLUTData()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::UpdateLUTData(
    BpsIQSettings* pBPSIQSettings)
{
    UINT32 numLUT           = 0;
    UINT32 singleHeaderSize = cdm_get_cmd_header_size(CDMCmdDMI) * sizeof(UINT32);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    for (UINT count = 0; count < m_numBPSIQModuleEnabled; count++)
    {
        UINT32 LUTCount = 0;
        SetIQModuleLUTOffset(m_pBPSIQModules[count]->GetIQType(), pBPSIQSettings, (numLUT * singleHeaderSize), &LUTCount);
        numLUT += LUTCount;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::ProcessingNodeFinalizeInputRequirement
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::ProcessingNodeFinalizeInputRequirement(
    BufferNegotiationData* pBufferNegotiationData)
{
    CAMX_ASSERT(NULL != pBufferNegotiationData);

    CamxResult                 result                          = CamxResultSuccess;
    UINT32                     optimalInputWidth               = 0;
    UINT32                     optimalInputHeight              = 0;
    UINT32                     minInputWidth                   = BPSMinInputWidth;
    UINT32                     minInputHeight                  = BPSMinInputHeight;
    UINT32                     maxInputWidth                   = BPSMaxInputWidth;
    UINT32                     maxInputHeight                  = BPSMaxInputHeight;
    UINT32                     perOutputPortOptimalWidth       = 0;
    UINT32                     perOutputPortOptimalHeight      = 0;
    UINT32                     perOutputPortMinWidth           = 0;
    UINT32                     perOutputPortMinHeight          = 0;
    UINT32                     perOutputPortMaxWidth           = 0;
    UINT32                     perOutputPortMaxHeight          = 0;
    BufferRequirement*         pOutputBufferRequirementOptions = NULL;
    BufferRequirement*         pInputPortRequirement           = NULL;
    OutputPortNegotiationData* pOutputPortNegotiationData      = NULL;
    UINT                       outputPortIndex                 = 0;
    UINT                       outputPortId                    = 0;
    AlignmentInfo              alignmentLCM[FormatsMaxPlanes]  = { {0} };

    // The input buffer requirement will be the super resolution after looping
    // through all the output ports.
    for (UINT index = 0; (NULL != pBufferNegotiationData) && (index < pBufferNegotiationData->numOutputPortsNotified); index++)
    {
        pOutputPortNegotiationData      = &pBufferNegotiationData->pOutputPortNegotiationData[index];
        pOutputBufferRequirementOptions = &pOutputPortNegotiationData->outputBufferRequirementOptions;
        outputPortIndex                 = pOutputPortNegotiationData->outputPortIndex;
        outputPortId                    = GetOutputPortId(outputPortIndex);

        if ((TRUE == IsDownscaleOutputPort(outputPortId)) || (TRUE == IsStatsPort(outputPortId)) ||
            (((BPSProcessingMFNR == m_instanceProperty.processingType) ||
              (BPSProcessingMFSR == m_instanceProperty.processingType)) &&
             ((outputPortId == BPSOutputPortReg1) || (outputPortId == BPSOutputPortReg2))))
        {
            optimalInputWidth  = Utils::MaxUINT32(optimalInputWidth, minInputWidth);
            optimalInputHeight = Utils::MaxUINT32(optimalInputHeight, minInputHeight);
            continue;
        }

        Utils::Memset(pOutputBufferRequirementOptions, 0, sizeof(BufferRequirement));

        perOutputPortOptimalWidth  = 0;
        perOutputPortOptimalHeight = 0;
        perOutputPortMinWidth      = 0;
        perOutputPortMinHeight     = 0;
        perOutputPortMaxWidth      = 0;
        perOutputPortMaxHeight     = 0;

        // Go through the requirements of the input ports connected to the output port
        for (UINT inputIndex = 0; inputIndex < pOutputPortNegotiationData->numInputPortsNotification; inputIndex++)
        {
            pInputPortRequirement = &pOutputPortNegotiationData->inputPortRequirement[inputIndex];

            // Max dimension should be smallest from all the ports connected.
            perOutputPortMaxWidth = (0 == perOutputPortMaxWidth) ? pInputPortRequirement->maxWidth :
                Utils::MinUINT32(perOutputPortMaxWidth, pInputPortRequirement->maxWidth);
            perOutputPortMaxHeight = (0 == perOutputPortMaxHeight) ? pInputPortRequirement->maxHeight :
                Utils::MinUINT32(perOutputPortMaxHeight, pInputPortRequirement->maxHeight);

            // Optimal dimension should be largest optimal value from all the ports connected.
            perOutputPortOptimalWidth = Utils::MaxUINT32(perOutputPortOptimalWidth, pInputPortRequirement->optimalWidth);
            perOutputPortOptimalHeight = Utils::MaxUINT32(perOutputPortOptimalHeight, pInputPortRequirement->optimalHeight);

            // Min dimension should be largest minimum from all the ports connected.
            perOutputPortMinWidth = Utils::MaxUINT32(perOutputPortMinWidth, pInputPortRequirement->minWidth);
            perOutputPortMinHeight = Utils::MaxUINT32(perOutputPortMinHeight, pInputPortRequirement->minHeight);

            CAMX_LOG_INFO(CamxLogGroupPProc, "input ports: index %d, inputIndex %d, width((opt, min, max) %d , %d,  %d,"
                "height((opt, min, max) %d, %d, %d",
                index, inputIndex, perOutputPortOptimalWidth, perOutputPortMinWidth, perOutputPortMaxWidth,
                perOutputPortOptimalHeight, perOutputPortMinHeight, perOutputPortMaxHeight);

            for (UINT planeIdx = 0; planeIdx < FormatsMaxPlanes; planeIdx++)
            {
                alignmentLCM[planeIdx].strideAlignment   =
                Utils::CalculateLCM(
                                    static_cast<INT32>(alignmentLCM[planeIdx].strideAlignment),
                                    static_cast<INT32>(pInputPortRequirement->planeAlignment[planeIdx].strideAlignment));
                alignmentLCM[planeIdx].scanlineAlignment =
                Utils::CalculateLCM(
                                    static_cast<INT32>(alignmentLCM[planeIdx].scanlineAlignment),
                                    static_cast<INT32>(pInputPortRequirement->planeAlignment[planeIdx].scanlineAlignment));
            }
        }

        // Optimal dimension should lie between the min and max, ensure the same.
        // There is a chance of the Optimal dimension going over the max dimension.
        perOutputPortOptimalWidth =
            Utils::ClampUINT32(perOutputPortOptimalWidth, perOutputPortMinWidth, perOutputPortMaxWidth);
        perOutputPortOptimalHeight =
            Utils::ClampUINT32(perOutputPortOptimalHeight, perOutputPortMinHeight, perOutputPortMaxHeight);

        // Store the buffer requirements for this output port which will be reused to set, during forward walk.
        // The values stored here could be final output dimensions unless it is overridden by forward walk.
        pOutputBufferRequirementOptions->optimalWidth      = perOutputPortOptimalWidth;
        pOutputBufferRequirementOptions->optimalHeight     = perOutputPortOptimalHeight;

        pOutputBufferRequirementOptions->minWidth          = perOutputPortMinWidth;
        pOutputBufferRequirementOptions->maxWidth          = perOutputPortMaxWidth;
        pOutputBufferRequirementOptions->minHeight         = perOutputPortMinHeight;
        pOutputBufferRequirementOptions->maxHeight         = perOutputPortMaxHeight;

        Utils::Memcpy(&pOutputBufferRequirementOptions->planeAlignment[0],
                      &alignmentLCM[0],
                      sizeof(AlignmentInfo) * FormatsMaxPlanes);

        maxInputWidth = (0 == maxInputWidth) ? perOutputPortMaxWidth :
            Utils::MinUINT32(maxInputWidth, perOutputPortMaxWidth);
        maxInputHeight = (0 == maxInputHeight) ? perOutputPortMaxHeight :
            Utils::MinUINT32(maxInputHeight, perOutputPortMaxHeight);

        optimalInputWidth  = Utils::MaxUINT32(optimalInputWidth, perOutputPortOptimalWidth);
        optimalInputHeight = Utils::MaxUINT32(optimalInputHeight, perOutputPortOptimalHeight);

        minInputWidth      = Utils::MaxUINT32(minInputWidth, perOutputPortMinWidth);
        minInputHeight     = Utils::MaxUINT32(minInputHeight, perOutputPortMinHeight);

        CAMX_LOG_INFO(CamxLogGroupPProc, "output port: index %d, width(opt, min, max) %d , %d,  %d,"
                         "height (opt, min, max) %d, %d, %d",
                         index, optimalInputWidth, minInputWidth, maxInputWidth,
                         optimalInputHeight, minInputHeight, maxInputHeight);
    }

    if ((optimalInputWidth == 0) || (optimalInputHeight == 0))
    {
        result = CamxResultEFailed;

        CAMX_LOG_ERROR(CamxLogGroupPProc,
                       "Buffer Negotiation Failed, W:%d x H:%d!\n",
                       optimalInputWidth,
                       optimalInputHeight);
    }
    else
    {
        // Ensure optimal dimension is within min and max dimension,
        // There are chances that the optimal dimension is more than max dimension.
        optimalInputWidth  =
            Utils::ClampUINT32(optimalInputWidth, minInputWidth, maxInputWidth);
        optimalInputHeight =
            Utils::ClampUINT32(optimalInputHeight, minInputHeight, maxInputHeight);

        UINT32 numInputPorts = 0;
        UINT32 inputPortId[BPSMaxInput];

        // Get Input Port List
        GetAllInputPortIds(&numInputPorts, &inputPortId[0]);
        if (NULL == pBufferNegotiationData)
        {
            result = CamxResultEFailed;
            CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Buffer Negotiation Data.");
        }

        if (CamxResultSuccess == result)
        {
            pBufferNegotiationData->numInputPorts = numInputPorts;

            for (UINT input = 0; input < numInputPorts; input++)
            {
                pBufferNegotiationData->inputBufferOptions[input].nodeId     = Type();
                pBufferNegotiationData->inputBufferOptions[input].instanceId = InstanceID();
                pBufferNegotiationData->inputBufferOptions[input].portId     = inputPortId[input];

                BufferRequirement* pInputBufferRequirement =
                    &pBufferNegotiationData->inputBufferOptions[input].bufferRequirement;

                pInputBufferRequirement->optimalWidth  = optimalInputWidth;
                pInputBufferRequirement->optimalHeight = optimalInputHeight;
                pInputBufferRequirement->minWidth      = minInputWidth;
                pInputBufferRequirement->maxWidth      = maxInputWidth;
                pInputBufferRequirement->minHeight     = minInputHeight;
                pInputBufferRequirement->maxHeight     = maxInputHeight;

                CAMX_LOG_INFO(CamxLogGroupPProc, "Port %d width(opt, min, max) %d , %d,  %d,"
                              "height((opt, min, max)) %d, %d, %d",
                              inputPortId[input],
                              optimalInputWidth,
                              minInputWidth,
                              maxInputWidth,
                              optimalInputHeight,
                              minInputHeight,
                              maxInputHeight);
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::FinalizeBufferProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::FinalizeBufferProperties(
    BufferNegotiationData* pBufferNegotiationData)
{
    CAMX_ASSERT(NULL != pBufferNegotiationData);

    for (UINT index = 0; (NULL != pBufferNegotiationData) && (index < pBufferNegotiationData->numOutputPortsNotified); index++)
    {
        OutputPortNegotiationData* pOutputPortNegotiationData   = &pBufferNegotiationData->pOutputPortNegotiationData[index];
        InputPortNegotiationData*  pInputPortNegotiationData    = &pBufferNegotiationData->pInputPortNegotiationData[0];
        BufferProperties*          pFinalOutputBufferProperties = pOutputPortNegotiationData->pFinalOutputBufferProperties;

        if ((FALSE == IsSinkPortWithBuffer(pOutputPortNegotiationData->outputPortIndex)) &&
            (FALSE == IsNonSinkHALBufferOutput(pOutputPortNegotiationData->outputPortIndex)))
        {
            UINT outputPortId = GetOutputPortId(pOutputPortNegotiationData->outputPortIndex);

            switch (outputPortId)
            {
                case BPSOutputPortFull:
                    pFinalOutputBufferProperties->imageFormat.width  =
                        pInputPortNegotiationData->pImageFormat->width;
                    pFinalOutputBufferProperties->imageFormat.height =
                        pInputPortNegotiationData->pImageFormat->height;
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "full width %d, height %d",
                        pFinalOutputBufferProperties->imageFormat.width,
                        pFinalOutputBufferProperties->imageFormat.height);
                    break;
                case BPSOutputPortDS4:
                    if (BPSProfileId::BPSProfileIdIdealRawOutput == m_instanceProperty.profileId)
                    {
                        pFinalOutputBufferProperties->imageFormat.width  =
                            pInputPortNegotiationData->pImageFormat->width;
                        pFinalOutputBufferProperties->imageFormat.height =
                            pInputPortNegotiationData->pImageFormat->height;
                    }
                    else
                    {
                        pFinalOutputBufferProperties->imageFormat.width =
                            Utils::EvenCeilingUINT32(
                                Utils::AlignGeneric32(pInputPortNegotiationData->pImageFormat->width, 4) / 4);
                        pFinalOutputBufferProperties->imageFormat.height =
                            Utils::EvenCeilingUINT32(
                                Utils::AlignGeneric32(pInputPortNegotiationData->pImageFormat->height, 4) / 4);
                    }
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "DS4 width %d, height %d",
                        pFinalOutputBufferProperties->imageFormat.width,
                        pFinalOutputBufferProperties->imageFormat.height);
                    break;
                case BPSOutputPortDS16:
                    pFinalOutputBufferProperties->imageFormat.width =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(pInputPortNegotiationData->pImageFormat->width, 16) / 16);
                    pFinalOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(pInputPortNegotiationData->pImageFormat->height, 16) / 16);
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "DS16 width %d, height %d",
                        pFinalOutputBufferProperties->imageFormat.width,
                        pFinalOutputBufferProperties->imageFormat.height);
                    break;
                case BPSOutputPortDS64:
                    pFinalOutputBufferProperties->imageFormat.width =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(pInputPortNegotiationData->pImageFormat->width, 64) / 64);
                    pFinalOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(
                            Utils::AlignGeneric32(pInputPortNegotiationData->pImageFormat->height, 64) / 64);
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "DS64 width %d, height %d",
                        pFinalOutputBufferProperties->imageFormat.width,
                        pFinalOutputBufferProperties->imageFormat.height);
                    break;
                case BPSOutputPortReg1:
                case BPSOutputPortReg2:
                    if ((BPSProcessingMFNR == m_instanceProperty.processingType) ||
                        (BPSProcessingMFSR == m_instanceProperty.processingType))
                    {
                        pFinalOutputBufferProperties->imageFormat.width =
                            Utils::EvenCeilingUINT32(Utils::AlignGeneric32(
                                pInputPortNegotiationData->pImageFormat->width / 3, 64));
                        pFinalOutputBufferProperties->imageFormat.height =
                            Utils::EvenCeilingUINT32(Utils::AlignGeneric32(
                                pInputPortNegotiationData->pImageFormat->height / 3, 64));

                        pFinalOutputBufferProperties->imageFormat.formatParams.yuvFormat[0].planeStride =
                            Utils::AlignGeneric32(pFinalOutputBufferProperties->imageFormat.width, 64);
                        pFinalOutputBufferProperties->imageFormat.formatParams.yuvFormat[0].sliceHeight =
                            Utils::AlignGeneric32(pFinalOutputBufferProperties->imageFormat.height, 64);
                        pFinalOutputBufferProperties->imageFormat.formatParams.yuvFormat[1].planeStride =
                            Utils::AlignGeneric32(pFinalOutputBufferProperties->imageFormat.width, 64);
                        pFinalOutputBufferProperties->imageFormat.formatParams.yuvFormat[1].sliceHeight =
                            pFinalOutputBufferProperties->imageFormat.formatParams.yuvFormat[0].sliceHeight / 2;
                        CAMX_LOG_INFO(CamxLogGroupPProc,
                                      "MFNR Reg width %d, height %d,"
                                      "planeStride=%d, sliceHeight=%d,"
                                      "planceStride2=%d, sliceHeight2=%d",
                                      pFinalOutputBufferProperties->imageFormat.width,
                                      pFinalOutputBufferProperties->imageFormat.height,
                                      pFinalOutputBufferProperties->imageFormat.formatParams.yuvFormat[0].planeStride,
                                      pFinalOutputBufferProperties->imageFormat.formatParams.yuvFormat[0].sliceHeight,
                                      pFinalOutputBufferProperties->imageFormat.formatParams.yuvFormat[1].planeStride,
                                      pFinalOutputBufferProperties->imageFormat.formatParams.yuvFormat[1].sliceHeight);
                    }
                    else
                    {
                        pFinalOutputBufferProperties->imageFormat.width =
                            Utils::EvenCeilingUINT32(pInputPortNegotiationData->pImageFormat->width);
                        pFinalOutputBufferProperties->imageFormat.height =
                            Utils::EvenCeilingUINT32(pInputPortNegotiationData->pImageFormat->height);
                        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Reg width %d, height %d",
                            pFinalOutputBufferProperties->imageFormat.width,
                            pFinalOutputBufferProperties->imageFormat.height);

                    }
                    break;

                case BPSOutputPortStatsBG:
                    pFinalOutputBufferProperties->imageFormat.width  = BPSAWBBGStatsMaxWidth;
                    pFinalOutputBufferProperties->imageFormat.height = BPSAWBBGStatsMaxHeight;
                    break;

                case BPSOutputPortStatsHDRBHist:
                    pFinalOutputBufferProperties->imageFormat.width  = HDRBHistStatsMaxWidth;
                    pFinalOutputBufferProperties->imageFormat.height = HDRBHistStatsMaxHeight;
                    break;

                default:
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Unsupported port %d", outputPortId);
                    break;
            }

            Utils::Memcpy(&pFinalOutputBufferProperties->imageFormat.planeAlignment[0],
                          &pOutputPortNegotiationData->outputBufferRequirementOptions.planeAlignment[0],
                          sizeof(AlignmentInfo) * FormatsMaxPlanes);

            if ((BPSProcessingMFNR == m_instanceProperty.processingType) &&
                ((outputPortId == BPSOutputPortReg1) || (outputPortId == BPSOutputPortReg2)))
            {
                pFinalOutputBufferProperties->imageFormat.planeAlignment[0].strideAlignment = 64;
                pFinalOutputBufferProperties->imageFormat.planeAlignment[1].strideAlignment = 64;
                pFinalOutputBufferProperties->imageFormat.planeAlignment[0].scanlineAlignment = 64;
                pFinalOutputBufferProperties->imageFormat.planeAlignment[1].scanlineAlignment = 32;
                CAMX_LOG_INFO(CamxLogGroupPProc,
                    "MFNR Reg width %d, height %d, strideAlign=%d, scanlineAlign=%d, strideAlign2=%d, scanlineAlign2=%d",
                    pFinalOutputBufferProperties->imageFormat.width,
                    pFinalOutputBufferProperties->imageFormat.height,
                    pFinalOutputBufferProperties->imageFormat.planeAlignment[0].strideAlignment,
                    pFinalOutputBufferProperties->imageFormat.planeAlignment[0].scanlineAlignment,
                    pFinalOutputBufferProperties->imageFormat.planeAlignment[1].strideAlignment,
                    pFinalOutputBufferProperties->imageFormat.planeAlignment[1].scanlineAlignment);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetStaticMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::GetStaticMetadata(
    UINT32 cameraId)
{
    CamxResult   result = CamxResultSuccess;
    HwCameraInfo cameraInfo;

    result = HwEnvironment::GetInstance()->GetCameraInfo(cameraId, &cameraInfo);
    if (CamxResultSuccess == result)
    {
        m_HALTagsData.controlPostRawSensitivityBoost    = cameraInfo.pPlatformCaps->minPostRawSensitivityBoost;
        // Retrieve the static capabilities for this camera
        m_pOTPData = &(cameraInfo.pSensorCaps->OTPData);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to obtain camera info, result: %d", result);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::InitializeDefaultHALTags
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::InitializeDefaultHALTags()
{
    // Initialize default metadata
    m_HALTagsData.blackLevelLock                = BlackLevelLockOff;
    m_HALTagsData.colorCorrectionMode           = ColorCorrectionModeFast;
    m_HALTagsData.controlAEMode                 = ControlAEModeOn;
    m_HALTagsData.controlAWBMode                = ControlAWBModeAuto;
    m_HALTagsData.controlMode                   = ControlModeAuto;
    m_HALTagsData.noiseReductionMode            = NoiseReductionModeFast;
    m_HALTagsData.shadingMode                   = ShadingModeFast;
    m_HALTagsData.statisticsHotPixelMapMode     = StatisticsHotPixelMapModeOff;
    m_HALTagsData.statisticsLensShadingMapMode  = StatisticsLensShadingMapModeOff;
    m_HALTagsData.tonemapCurves.tonemapMode     = TonemapModeFast;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetMetadataTags
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::GetMetadataTags(
    ISPInputData* pModuleInput)
{
    VOID*      pTagsData[NumBPSMetadataTags] = {0};
    UINT       dataIndex                     = 0;
    UINT       metaTag                       = 0;
    CamxResult result                        = CamxResultSuccess;

    GetDataList(BPSMetadataTags, pTagsData, BPSMetadataTagReqOffset, NumBPSMetadataTags);

    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->blackLevelLock = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }

    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->colorCorrectionGains = *(static_cast<ColorCorrectionGain*>(pTagsData[dataIndex++]));
    }

    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->colorCorrectionMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }

    if (NULL != pTagsData[dataIndex])
    {
        Utils::Memcpy(&pModuleInput->pHALTagsData->colorCorrectionTransform,
            pTagsData[dataIndex++], sizeof(pModuleInput->pHALTagsData->colorCorrectionTransform));
    }

    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlAEMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlAWBMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->controlPostRawSensitivityBoost = *(static_cast<INT32*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->hotPixelMode = *(static_cast<HotPixelModeValues*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->noiseReductionMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->shadingMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->statisticsHotPixelMapMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->statisticsLensShadingMapMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }
    if (NULL != pTagsData[dataIndex])
    {
        pModuleInput->pHALTagsData->tonemapCurves.tonemapMode = *(static_cast<UINT8*>(pTagsData[dataIndex++]));
    }

    if ((BPSProfileId::BPSProfileIdHNR         == m_instanceProperty.profileId) &&
        ((BPSProcessingType::BPSProcessingMFSR == m_instanceProperty.processingType)))
    {
        IntermediateDimensions* pIntermediateDimension = NULL;
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.intermediateDimension",
                                                          "IntermediateDimension",
                                                          &metaTag);
        if (CamxResultSuccess == result)
        {
            static const UINT PropertiesIPE[]     = { metaTag | InputMetadataSectionMask };
            const UINT        length              = CAMX_ARRAY_SIZE(PropertiesIPE);
            VOID*             pData[length]       = { 0 };
            UINT64            pDataOffset[length] = { 0 };

            GetDataList(PropertiesIPE, pData, pDataOffset, length);
            if (NULL != pData[0])
            {
                pIntermediateDimension = reinterpret_cast<IntermediateDimensions*>(pData[0]);
            }
        }

        if (NULL != pIntermediateDimension)
        {
            m_curIntermediateDimension.width  = pIntermediateDimension->width;
            m_curIntermediateDimension.height = pIntermediateDimension->height;
            m_curIntermediateDimension.ratio  = pIntermediateDimension->ratio;

            CAMX_LOG_INFO(CamxLogGroupPProc, "BPS:%d intermediate width=%d height=%d ratio=%f",
                           InstanceID(),
                           m_curIntermediateDimension.width,
                           m_curIntermediateDimension.height,
                           m_curIntermediateDimension.ratio);
        }
        else
        {
            result = CamxResultEResource;
            CAMX_LOG_ERROR(CamxLogGroupPProc, "BPS:%d Error in getting intermediate dimension slot", InstanceID());
        }
    }

    GetMetadataContrastLevel(pModuleInput->pHALTagsData);
    GetMetadataTonemapCurve(pModuleInput->pHALTagsData);

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::PostMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::PostMetadata(
    const ISPInputData* pInputData)
{
    CamxResult  result                               = CamxResultSuccess;
    const VOID* ppData[NumBPSMetadataOutputTags]     = { 0 };
    UINT        pDataCount[NumBPSMetadataOutputTags] = { 0 };
    UINT        index                                = 0;
    UINT32      metaTag                              = 0;
    FLOAT       dynamicBlackLevel[ISPChannelMax];

    if (TRUE == m_BPSPathEnabled[BPSOutputPortStatsBG])
    {
        PropertyISPAWBBGStats  AWBBGStatsProperty = {};
        Utils::Memcpy(&AWBBGStatsProperty.statsConfig,
                      &pInputData->pCalculatedData->metadata.AWBBGStatsConfig,
                      sizeof(ISPAWBBGStatsConfig));

        static const UINT ISPAWBBGConfigProps[] = { PropertyIDISPAWBBGConfig };
        const VOID*       pISPAWBBGConfigOutputData[CAMX_ARRAY_SIZE(ISPAWBBGConfigProps)] = { &AWBBGStatsProperty };
        UINT              pISPAWBBGConfigDataCount[CAMX_ARRAY_SIZE(ISPAWBBGConfigProps)]  = { sizeof(AWBBGStatsProperty) };
        WriteDataList(ISPAWBBGConfigProps, pISPAWBBGConfigOutputData,
                      pISPAWBBGConfigDataCount, CAMX_ARRAY_SIZE(ISPAWBBGConfigProps));
    }

    if (TRUE == m_BPSPathEnabled[BPSOutputPortStatsHDRBHist])
    {
        // HDR BHist stats config data
        PropertyISPHDRBHistStats HDRBHistStatsProperty = {};
        Utils::Memcpy(&HDRBHistStatsProperty.statsConfig,
                      &pInputData->pCalculatedData->metadata.HDRBHistStatsConfig,
                      sizeof(ISPHDRBHistStatsConfig));

        static const UINT HDRBHistStatsConfigProps[] =
        {
            PropertyIDISPHDRBHistConfig
        };

        const VOID* pHDRBHistStatsConfigData[CAMX_ARRAY_SIZE(HDRBHistStatsConfigProps)] =
        {
            &HDRBHistStatsProperty
        };

        UINT pHDRBHistStatsConfigDataCount[CAMX_ARRAY_SIZE(HDRBHistStatsConfigProps)] =
        {
            sizeof(HDRBHistStatsProperty)
        };

        WriteDataList(HDRBHistStatsConfigProps, pHDRBHistStatsConfigData,
                      pHDRBHistStatsConfigDataCount, CAMX_ARRAY_SIZE(HDRBHistStatsConfigProps));
    }

    /**
     * Post ADRC/Gamma Metadata
     * During the post filter second stage, the BPS will run HNR only.
     * the Gamma/ADRC structure will not be filed. So, We skip the Gamma/ADRC info publishing.
     * and IPE of post filter second stage will use the ADRC/Gamma info published by
     * the Post filter first stage.
     */
    if ((BPSProfileId::BPSProfileIdHNR == m_instanceProperty.profileId) &&
        ((BPSProcessingType::BPSProcessingMFNR == m_instanceProperty.processingType) ||
         (BPSProcessingType::BPSProcessingMFSR == m_instanceProperty.processingType)))
    {
        CAMX_LOG_INFO(CamxLogGroupPProc, "Disable the BPS ADRC/Gamma Publish for MFNR/MFSR post filter second stage");
    }
    else
    {
        // Post PerFrame metadata tags
        pDataCount[index] = 1;
        ppData[index] = &pInputData->pHALTagsData->blackLevelLock;
        index++;

        pDataCount[index] = 4;
        ppData[index] = &pInputData->pCalculatedData->colorCorrectionGains;
        index++;

        pDataCount[index] = 1;
        ppData[index] = &pInputData->pCalculatedData->controlPostRawSensitivityBoost;
        index++;

        pDataCount[index] = 1;
        ppData[index] = &pInputData->pCalculatedData->noiseReductionMode;
        index++;

        pDataCount[index] = 1;
        ppData[index] = &pInputData->pCalculatedData->hotPixelMode;
        index++;

        pDataCount[index] = 1;
        ppData[index] = &pInputData->pCalculatedData->lensShadingInfo.shadingMode;
        index++;

        pDataCount[index] = 1;
        ppData[index] = &pInputData->pCalculatedData->lensShadingInfo.lensShadingMapMode;
        index++;

        pDataCount[index] = 2;
        ppData[index] = &pInputData->pCalculatedData->lensShadingInfo.lensShadingMapSize;
        index++;

        if (StatisticsLensShadingMapModeOn == pInputData->pCalculatedData->lensShadingInfo.lensShadingMapMode)
        {
            pDataCount[index] = 4 * MESH_ROLLOFF_SIZE;
            ppData[index] = pInputData->pCalculatedData->lensShadingInfo.lensShadingMap;
            index++;
        }
        else
        {
            pDataCount[index] = 1;
            ppData[index] = NULL;
            index++;
        }


        for (UINT32 channel = 0; channel < ISPChannelMax; channel++)
        {
            dynamicBlackLevel[channel] =
                static_cast<FLOAT>(((pInputData->pCalculatedData->BLSblackLevelOffset +
                    pInputData->pCalculatedData->linearizationAppliedBlackLevel[channel]) >>
                    (IFEPipelineBitWidth - pInputData->sensorBitWidth)));
        }

        pDataCount[index] = 4;
        ppData[index] = dynamicBlackLevel;
        index++;

        pDataCount[index] = 1;
        ppData[index] = &pInputData->pCalculatedData->blackLevelLock;
        index++;

        pDataCount[index] = 1;
        ppData[index] = &pInputData->pHALTagsData->statisticsHotPixelMapMode;
        index++;

        pDataCount[index] = 1;
        ppData[index] = &pInputData->pCalculatedData->toneMapData.tonemapMode;
        index++;

        pDataCount[index] = 1;
        ppData[index] = &pInputData->pCalculatedData->IPEGamma15PreCalculationOutput;
        index++;

        WriteDataList(BPSMetadataOutputTags, ppData, pDataCount, NumBPSMetadataOutputTags);
        // publish the metadata: PropertyIDBPSADRCInfoOutput
        PropertyISPADRCInfo adrcInfo;

        if (NULL != pInputData->triggerData.pADRCData)
        {
            adrcInfo.enable    = pInputData->triggerData.pADRCData->enable;
            adrcInfo.version   = pInputData->triggerData.pADRCData->version;
            adrcInfo.gtmEnable = pInputData->triggerData.pADRCData->gtmEnable;
            adrcInfo.ltmEnable = pInputData->triggerData.pADRCData->ltmEnable;

            if (SWTMCVersion::TMC10 == adrcInfo.version)
            {
                Utils::Memcpy(&adrcInfo.kneePoints.KneePointsTMC10.kneeX,
                        pInputData->triggerData.pADRCData->kneePoints.KneePointsTMC10.kneeX,
                        sizeof(FLOAT) * MAX_ADRC_LUT_KNEE_LENGTH_TMC10);
                Utils::Memcpy(&adrcInfo.kneePoints.KneePointsTMC10.kneeY,
                        pInputData->triggerData.pADRCData->kneePoints.KneePointsTMC10.kneeY,
                        sizeof(FLOAT) * MAX_ADRC_LUT_KNEE_LENGTH_TMC10);
            }
            else if (SWTMCVersion::TMC11 == adrcInfo.version)
            {
                Utils::Memcpy(&adrcInfo.kneePoints.KneePointsTMC11.kneeX,
                        pInputData->triggerData.pADRCData->kneePoints.KneePointsTMC11.kneeX,
                        sizeof(FLOAT) * MAX_ADRC_LUT_KNEE_LENGTH_TMC11);
                Utils::Memcpy(&adrcInfo.kneePoints.KneePointsTMC11.kneeY,
                        pInputData->triggerData.pADRCData->kneePoints.KneePointsTMC11.kneeY,
                        sizeof(FLOAT) * MAX_ADRC_LUT_KNEE_LENGTH_TMC11);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "bad adrcInfo version: %d", adrcInfo.version);
                result = CamxResultEInvalidArg;
            }

            adrcInfo.drcGainDark   = pInputData->triggerData.pADRCData->drcGainDark;
            adrcInfo.ltmPercentage = pInputData->triggerData.pADRCData->ltmPercentage;
            adrcInfo.gtmPercentage = pInputData->triggerData.pADRCData->gtmPercentage;

            Utils::Memcpy(&adrcInfo.coef,
                    pInputData->triggerData.pADRCData->coef,
                    sizeof(FLOAT) * MAX_ADRC_LUT_COEF_SIZE);

            Utils::Memcpy(&adrcInfo.pchipCoef,
                    pInputData->triggerData.pADRCData->pchipCoef,
                    sizeof(FLOAT) * MAX_ADRC_LUT_PCHIP_COEF_SIZE);
            Utils::Memcpy(&adrcInfo.contrastEnhanceCurve,
                    pInputData->triggerData.pADRCData->contrastEnhanceCurve,
                    sizeof(FLOAT) * MAX_ADRC_CONTRAST_CURVE);

            adrcInfo.curveModel       = pInputData->triggerData.pADRCData->curveModel;
            adrcInfo.contrastHEBright = pInputData->triggerData.pADRCData->contrastHEBright;
            adrcInfo.contrastHEDark   = pInputData->triggerData.pADRCData->contrastHEDark;
        }
        else
        {
            Utils::Memset(&adrcInfo, 0, sizeof(adrcInfo));
        }

        if (CamxResultSuccess == result)
        {
            static const UINT tADRCProps[]                                  = { PropertyIDBPSADRCInfoOutput };
            const VOID*       pADRCData[CAMX_ARRAY_SIZE(tADRCProps)]        = { &adrcInfo };
            UINT              pADRCDataCount[CAMX_ARRAY_SIZE(tADRCProps)]   = { sizeof(adrcInfo) };

            result = WriteDataList(tADRCProps, pADRCData, pADRCDataCount, CAMX_ARRAY_SIZE(tADRCProps));
        }

        // publish the metadata: PropertyIDBPSGammaOutput
        GammaInfo greenGamma = {0};
        Utils::Memcpy(&greenGamma.gammaG,
                pInputData->pCalculatedData->gammaOutput.gammaG,
                NumberOfEntriesPerLUT * sizeof(UINT32));
        greenGamma.isGammaValid = pInputData->pCalculatedData->gammaOutput.isGammaValid;

        if (CamxResultSuccess == result)
        {
            static const UINT gammaValidProps[] = { PropertyIDBPSGammaOutput };
            const VOID*       pGammaValidData[CAMX_ARRAY_SIZE(gammaValidProps)] = { &greenGamma };
            UINT              pGammaValidDataCount[CAMX_ARRAY_SIZE(gammaValidProps)] = { sizeof(greenGamma) };
            result = WriteDataList(gammaValidProps, pGammaValidData, pGammaValidDataCount, CAMX_ARRAY_SIZE(gammaValidProps));
        }

        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.gammainfo",
                "GammaInfo",
                &metaTag);
        if (CamxResultSuccess == result)
        {
            static const UINT gammaInfoTag[] = { metaTag };
            const VOID*       pGammaInfoData[1] = { &greenGamma };
            UINT              length = CAMX_ARRAY_SIZE(gammaInfoTag);
            UINT              pGammaInfoDataCount[] = { sizeof(greenGamma) };

            WriteDataList(gammaInfoTag, pGammaInfoData, pGammaInfoDataCount, length);
        }
    }

    // Read and post dual camera metadata if available
    UINT32 tag = 0;
    if (CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("com.qti.chi.multicamerainfo", "MultiCameraIdRole", &tag))
    {
        UINT              tagReadInput[]                 = { tag | InputMetadataSectionMask };
        static const UINT TagLength                      = CAMX_ARRAY_SIZE(tagReadInput);
        VOID*             pMultiCamIdRoleGetData[TagLength] = { 0 };
        UINT64            multiCamIdRoleGetDataOffset[TagLength]          = { 0 };
        if (CamxResultSuccess == GetDataList(tagReadInput, pMultiCamIdRoleGetData, multiCamIdRoleGetDataOffset, 1))
        {
            const UINT pMultiCamIDRoleDataCount[TagLength] = { sizeof(MultiCameraIdRole) };
            const VOID* pMultiCamIDRoleData[TagLength]     = { pMultiCamIdRoleGetData[0] };
            WriteDataList(&tag, pMultiCamIDRoleData, pMultiCamIDRoleDataCount, 1);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetOEMStatsConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::GetOEMStatsConfig(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    UINT32 metadataAECFrameControl = 0;
    UINT32 metadataAWBFrameControl = 0;
    UINT32 metadataAECStatsControl = 0;
    UINT32 metadataAWBStatsControl = 0;
    UINT32 metadataAFStatsControl  = 0;

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AECFrameControl",
        &metadataAECFrameControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AECFrameControl");

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AWBFrameControl",
        &metadataAWBFrameControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AWBFrameControl");

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AECStatsControl",
        &metadataAECStatsControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AECStatsControl");

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AWBStatsControl",
        &metadataAWBStatsControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AWBStatsControl");

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AFStatsControl",
        &metadataAFStatsControl);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AFStatsControl");

    static const UINT vendorTagsControl3A[] =
    {
        metadataAECStatsControl | InputMetadataSectionMask,
        metadataAWBStatsControl | InputMetadataSectionMask,
        metadataAFStatsControl  | InputMetadataSectionMask,
        metadataAECFrameControl | InputMetadataSectionMask,
        metadataAWBFrameControl | InputMetadataSectionMask,
    };

    const SIZE_T numTags                            = CAMX_ARRAY_SIZE(vendorTagsControl3A);
    VOID*        pVendorTagsControl3A[numTags]      = { 0 };
    UINT64       vendorTagsControl3AOffset[numTags] = { 0 };

    GetDataList(vendorTagsControl3A, pVendorTagsControl3A, vendorTagsControl3AOffset, numTags);

    // Pointers in pVendorTagsControl3A[] guaranteed to be non-NULL by GetDataList() for InputMetadataSectionMask
    Utils::Memcpy(pInputData->pAECStatsUpdateData, pVendorTagsControl3A[BPSVendorTagAECStats], sizeof(AECStatsControl));
    Utils::Memcpy(pInputData->pAWBStatsUpdateData, pVendorTagsControl3A[BPSVendorTagAWBStats], sizeof(AWBStatsControl));
    Utils::Memcpy(pInputData->pAFStatsUpdateData, pVendorTagsControl3A[BPSVendorTagAFStats], sizeof(AFStatsControl));
    Utils::Memcpy(pInputData->pAECUpdateData, pVendorTagsControl3A[BPSVendorTagAECFrame], sizeof(AECFrameControl));
    Utils::Memcpy(pInputData->pAWBUpdateData, pVendorTagsControl3A[BPSVendorTagAWBFrame], sizeof(AWBFrameControl));

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetTintlessStatus
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::GetTintlessStatus(
    ISPInputData* pModuleInput)
{
    CamxResult result;
    UINT32     metaTag = 0;

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.tintless", "enable", &metaTag);

    if (CamxResultSuccess == result)
    {
        const UINT tintlessProperty[] = { metaTag | InputMetadataSectionMask };
        VOID* tintlessData[]          = { 0 };
        UINT64 tintlessDataOffset[]   = { 0 };
        GetDataList(tintlessProperty, tintlessData, tintlessDataOffset, 1);

        pModuleInput->tintlessEnable = *(reinterpret_cast<BYTE*>(tintlessData[0]));
        CAMX_LOG_INFO(CamxLogGroupISP, "Tintless Enable %d", pModuleInput->tintlessEnable);
    }
    else
    {
        pModuleInput->tintlessEnable = FALSE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::UpdateClock
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::UpdateClock(
    ExecuteProcessRequestData*   pExecuteProcessRequestData,
    CSLICPClockBandwidthRequest* pICPClockBandwidthRequest)
{
    NodeProcessRequestData* pNodeRequestData = pExecuteProcessRequestData->pNodeProcessRequestData;
    UINT64                  requestId        = pNodeRequestData->pCaptureRequest->requestId;
    UINT                    frameCycles;
    UINT                    FPS          = DefaultFPS;
    UINT64                  budgetNS;
    FLOAT                   overHead     = BPSClockOverhead;
    FLOAT                   efficiency   = BPSClockEfficiency;
    FLOAT                   budget;

    // Framecycles calculation considers Number of Pixels processed in the current frame, Overhead and Efficiency
    if ( 0 != m_FPS)
    {
        FPS = m_FPS;
    }

    frameCycles = pICPClockBandwidthRequest->frameCycles;
    frameCycles = static_cast<UINT>((frameCycles * overHead) / efficiency);

    // Budget is the Max duration of current frame to process
    budget   = 1.0f / FPS;
    budgetNS = static_cast<UINT64>(budget * NanoSecondMult);

    pICPClockBandwidthRequest->budgetNS     = budgetNS;
    pICPClockBandwidthRequest->frameCycles  = frameCycles;
    pICPClockBandwidthRequest->realtimeFlag = IsRealTime();

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "[%s][%llu] FPS = %d budget = %lf budgetNS = %lld fc = %d",
                     NodeIdentifierString(), requestId, FPS, budget, budgetNS, frameCycles);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::CalculateBPSRdBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::CalculateBPSRdBandwidth(
    PerRequestActivePorts*  pPerRequestPorts,
    BPSBandwidth*           pBandwidth)
{
    UINT   srcWidth  = 0;
    UINT   srcHeight = 0;
    FLOAT  bppSrc    = 0;
    FLOAT  overhead  = BPSBandwidthOverhead;
    FLOAT  swMargin  = BPSSwMargin;
    UINT   FPS       = pBandwidth->FPS;
    UINT64 readBandwidthPass0;
    UINT64 readBandwidthPass1;

    pBandwidth->readBW.unCompressedBW = 0;
    pBandwidth->readBW.compressedBW   = 0;

    for (UINT i = 0; i < pPerRequestPorts->numInputPorts; i++)
    {
        PerRequestInputPortInfo* pInputPort = &pPerRequestPorts->pInputPorts[i];

        if (pInputPort->portId == CSLBPSInputPortImage)
        {
            const ImageFormat* pImageFormat = GetInputPortImageFormat(InputPortIndex(pInputPort->portId));

            if (NULL != pImageFormat)
            {
                srcWidth    = pImageFormat->width;
                srcHeight   = pImageFormat->height;
                bppSrc      = static_cast<FLOAT>(GetNumberOfPixels(pImageFormat)) / 8;
            }

            break;
        }
    }

    // Pass0_RdAB = (  (SrcWidth * SrcHeight * SrcBPP * Ovhd ) ) * fps
    readBandwidthPass0 = static_cast<UINT64>((srcWidth * srcHeight * bppSrc * overhead ) * FPS);

    // Pass1_RdAB = (  (SrcWidth/4/2 * SrcHeight/4/2 * 8 * Ovhd ) )  *  fps
    readBandwidthPass1 = static_cast<UINT64>(((srcWidth / 4.0f / 2.0f) * (srcHeight / 4.0f / 2.0f) * 8.0f * overhead) * FPS);

    pBandwidth->readBW.unCompressedBW = static_cast<UINT64>((readBandwidthPass0 + readBandwidthPass1 ) * swMargin);
    pBandwidth->readBW.compressedBW   = pBandwidth->readBW.unCompressedBW;

    CAMX_LOG_VERBOSE(CamxLogGroupPower,
                     "BW: sw = %d sh = %ld bppSrc = %f overhead = %f FPS = %d "
                     "Pass0: %llu Pass1: %llu unCompressedBW = %llu, compressedBW = %llu",
                     srcWidth, srcHeight, bppSrc, overhead, FPS, readBandwidthPass0, readBandwidthPass1,
                     pBandwidth->readBW.unCompressedBW, pBandwidth->readBW.compressedBW);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::CalculateBPSWrBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::CalculateBPSWrBandwidth(
    PerRequestActivePorts*  pPerRequestPorts,
    BPSBandwidth*           pBandwidth)
{
    UINT   dstWidth   = 0;
    UINT   dstHeight  = 0;
    FLOAT  bppDst     = BPSBpp8Bit;
    FLOAT  overhead   = BPSBandwidthOverhead;
    FLOAT  swMargin   = BPSSwMargin;
    FLOAT  BPSUbwcCr  = BPSUBWCWrCompressionRatio;
    UINT   FPS        = pBandwidth->FPS;
    BOOL   UBWCEnable = FALSE;
    UINT64 writeBandwidthPass0;
    UINT64 writeBandwidthPass1;

    pBandwidth->writeBW.unCompressedBW = 0;
    pBandwidth->writeBW.compressedBW   = 0;

    for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
    {
        PerRequestOutputPortInfo* pOutputPort = &pPerRequestPorts->pOutputPorts[i];

        if (pOutputPort->portId == CSLBPSOutputPortIdPDIImageFull)
        {
            const ImageFormat* pImageFormat = GetOutputPortImageFormat(OutputPortIndex(pOutputPort->portId));

            dstWidth    = pImageFormat->width;
            dstHeight   = pImageFormat->height;
            UBWCEnable  = ImageFormatUtils::IsUBWC(pImageFormat->format);

            if (TRUE == ImageFormatUtils::Is10BitFormat(pImageFormat->format))
            {
                bppDst = BPSBpp10Bit;
            }

            break;
        }
    }

    // Pass0_WrAB = ((DstWidth * DstHeight * DstBPP * Ovhd ) / BPS_UBWC_WrCr + (DstWidth/4/2 * DstHeight/4/2 * 8 ) )  *  fps
    writeBandwidthPass0 = static_cast<UINT64>(
                          (((dstWidth * dstHeight * bppDst * overhead) * FPS) / BPSUbwcCr) +
                          (((dstWidth / 4.0f / 2.0f) * (dstHeight / 4.0f / 2.0f) * 8.0f) * FPS));

    // Pass1_WrAB = ((DstWidth/16/2 * DstHeight/16/2 * 8 )  + (DstWidth/64/2 * DstHeight/64/2 * 8 ) )  *  fps
    writeBandwidthPass1 = static_cast<UINT64>(
                          (((dstWidth / 16.0f / 2.0f) * (dstHeight / 16.0f / 2.0f) * 8.0f ) * FPS) +
                          (((dstWidth / 64.0f / 2.0f) * (dstHeight / 64.0f / 2.0f) * 8.0f ) * FPS));

    pBandwidth->writeBW.unCompressedBW = static_cast<UINT64>((writeBandwidthPass0 + writeBandwidthPass1) * swMargin);

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "uncompressed: dstw = %d sh = %d bppDst = %f overhead = %f FPS = %d BPSUbwcCr = %f"
                     "Pass0: %llu Pass1: %llu BW = %llu",
                     dstWidth, dstHeight, bppDst, overhead, FPS, BPSUbwcCr,
                     writeBandwidthPass0, writeBandwidthPass1, pBandwidth->writeBW.unCompressedBW);

    if (TRUE == UBWCEnable)
    {
        if (BPSBpp10Bit == bppDst)
        {
            BPSUbwcCr = BPSUBWCWrCompressionRatio10Bit;
        }
        else
        {
            BPSUbwcCr = BPSUBWCWrCompressionRatio8Bit;
        }

        writeBandwidthPass0 = static_cast<UINT64>(
                              (((dstWidth * dstHeight * bppDst * overhead) * FPS) / BPSUbwcCr) +
                              (((dstWidth / 4.0f / 2.0f) * (dstHeight / 4.0f / 2.0f) * 8.0f) * FPS));

        pBandwidth->writeBW.compressedBW = static_cast<UINT64>((writeBandwidthPass0 + writeBandwidthPass1) * swMargin);

        CAMX_LOG_VERBOSE(CamxLogGroupPower, "compressed: dstw = %d sh = %d bppDst = %f overhead = %f FPS = %d BPSUbwcCr = %f "
                         "Pass0: %llu Pass1: %llu BW = %d",
                         dstWidth, dstHeight, bppDst, overhead, FPS, BPSUbwcCr,
                         writeBandwidthPass0, writeBandwidthPass1, pBandwidth->writeBW.compressedBW);
    }
    else
    {
        pBandwidth->writeBW.compressedBW = pBandwidth->writeBW.unCompressedBW;
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "Wr: cbw = %llu bw = %llu", pBandwidth->writeBW.compressedBW,
                     pBandwidth->writeBW.unCompressedBW);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::UpdateBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::UpdateBandwidth(
    ExecuteProcessRequestData*   pExecuteProcessRequestData,
    CSLICPClockBandwidthRequest* pICPClockBandwidthRequest)
{
    PerRequestActivePorts*  pPerRequestPorts = pExecuteProcessRequestData->pEnabledPortsInfo;
    NodeProcessRequestData* pNodeRequestData = pExecuteProcessRequestData->pNodeProcessRequestData;
    UINT64                  requestId        = pNodeRequestData->pCaptureRequest->requestId;
    struct BPSBandwidth     bandwidth;
    UINT                    FPS              = DefaultFPS;

    if (0 != m_FPS)
    {
        FPS = m_FPS;
    }

    bandwidth.FPS = FPS;

    CalculateBPSRdBandwidth(pPerRequestPorts, &bandwidth);
    CalculateBPSWrBandwidth(pPerRequestPorts, &bandwidth);

    pICPClockBandwidthRequest->unCompressedBW = bandwidth.readBW.unCompressedBW   +
                                                bandwidth.writeBW.unCompressedBW;
    pICPClockBandwidthRequest->compressedBW   = bandwidth.readBW.compressedBW     +
                                                bandwidth.writeBW.compressedBW;

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "[%s][%llu] : fps=%d, "
                     "Read BandWidth (uncompressed = %lld compressed = %lld), "
                     "WriteBandwidth (uncompressed = %lld compressed = %lld), "
                     "TotalBandwidth (uncompressed = %lld compressed = %lld), ",
                     NodeIdentifierString(), requestId, FPS,
                     bandwidth.readBW.unCompressedBW,           bandwidth.readBW.compressedBW,
                     bandwidth.writeBW.unCompressedBW,          bandwidth.writeBW.compressedBW,
                     pICPClockBandwidthRequest->unCompressedBW, pICPClockBandwidthRequest->compressedBW);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::CheckAndUpdateClockBW
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::CheckAndUpdateClockBW(
    CmdBuffer*                   pCmdBuffer,
    ExecuteProcessRequestData*   pExecuteProcessRequestData,
    CSLICPClockBandwidthRequest* pICPClockBandwidthRequest)
{
    UpdateClock(pExecuteProcessRequestData, pICPClockBandwidthRequest);
    UpdateBandwidth(pExecuteProcessRequestData, pICPClockBandwidthRequest);

    PacketBuilder::WriteGenericBlobData(pCmdBuffer,
                                        CSLICPGenericBlobCmdBufferClk,
                                        sizeof(CSLICPClockBandwidthRequest),
                                        reinterpret_cast<BYTE*>(pICPClockBandwidthRequest));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DumpDebug
///
/// @brief: This is called when firmware signal an error and UMD needs firmware dump
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static CamxResult DumpDebug(
    BPSCmdBufferId          index,
    CmdBuffer*              pBuffer,
    UINT64                  requestId,
    UINT32                  realtime,
    BPSIntanceProperty      instanceProperty,
    const CHAR*             pPipelineName)
{
    CamxResult result = CamxResultSuccess;
    CHAR       filename[512];

    if (index == BPSCmdBufferBLMemory)
    {
        switch (index)
        {
            case BPSCmdBufferBLMemory:
                CAMX_LOG_ERROR(CamxLogGroupPProc, "dump bl buffer");
                CamX::OsUtils::SNPrintF(filename, sizeof(filename),
                    "%s/BPSBLMemoryDump_%s_%llu_realtime-%d_processTYpe_%d_profileId_%d.txt",
                    ConfigFileDirectory, pPipelineName, requestId,
                    realtime, instanceProperty.processingType, instanceProperty.profileId);

                break;
            default:
                result = CamxResultEInvalidArg;
                break;
        }
        if (CamxResultSuccess == result)
        {
            FILE* pFile = CamX::OsUtils::FOpen(filename, "wb");
            if (!pFile)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Can't open file");
                return CamxResultEFailed;
            }
            CamX::OsUtils::FWrite(pBuffer->GetHostAddr(), pBuffer->GetMaxLength(), 1, pFile);
            CamX::OsUtils::FClose(pFile);
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSNode::NotifyRequestProcessingError()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::NotifyRequestProcessingError(
    NodeFenceHandlerData* pFenceHandlerData,
    UINT                  unSignaledFenceCount)
{
    CAMX_ASSERT(NULL != pFenceHandlerData);
    CSLFenceResult  fenceResult    = pFenceHandlerData->fenceResult;
    BOOL            enableDump     = ((((0x2 == m_BPSHangDumpEnable) && (CSLFenceResultFailed == fenceResult)) ||
                                      ((0x1 == m_BPSHangDumpEnable) && (0 == unSignaledFenceCount))) ? TRUE : FALSE);

    if (TRUE == enableDump)
    {
        CAMX_LOG_INFO(CamxLogGroupPProc, "notify error fence back for request %d", pFenceHandlerData->requestId);
        CmdBuffer* pBuffer = NULL;
        pBuffer =
            CheckCmdBufferWithRequest(pFenceHandlerData->requestId, m_pBPSCmdBufferManager[BPSCmdBufferBLMemory]);
        if (!pBuffer)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "cant find buffer");
            return;
        }

        DumpDebug(BPSCmdBufferBLMemory, pBuffer, pFenceHandlerData->requestId,
            IsRealTime(), m_instanceProperty, NodeIdentifierString());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::SetScaleRatios
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSNode::SetScaleRatios(
    ISPInputData* pInputData,
    CmdBuffer*    pCmdBuffer)
{
    BOOL          isScaleRatioSet = FALSE;
    IFECropInfo   cropInfo;
    UINT32        metaTag = 0;
    CamxResult    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ref.cropsize",
                                                                    "RefCropSize",
                                                                    &metaTag);
    if (CamxResultSuccess == result)
    {
        metaTag |= InputMetadataSectionMask;
        CropWindow*        pCropWindow    = NULL;
        RefCropWindowSize* pRefCropWindow = NULL;

        UINT32  fullInputWidth = pInputData->pipelineBPSData.width;
        UINT32  fullInputHeight = pInputData->pipelineBPSData.height;

        static const UINT PropertiesBPS[] =
        {
            InputScalerCropRegion,
            metaTag
        };
        UINT length                          = CAMX_ARRAY_SIZE(PropertiesBPS);
        VOID* pData[2]                         = { 0 };
        UINT64 propertyDataBPSOffset[2]      = { 0 , 0};

        GetDataList(PropertiesBPS, pData, propertyDataBPSOffset, length);

        pCropWindow    = (static_cast<CropWindow*>(pData[0]));
        pRefCropWindow = (static_cast<RefCropWindowSize*>(pData[1]));

        if ((NULL != pCropWindow) && (NULL != pRefCropWindow))
        {

            if ((0 == pRefCropWindow->width) || (0 == pRefCropWindow->height))
            {
                pRefCropWindow->width = fullInputWidth;
                pRefCropWindow->height = fullInputHeight;
            }

            CAMX_LOG_INFO(CamxLogGroupPProc, "ZDBG IPE crop Window [%d, %d, %d, %d] full size %dX%d active %dX%d",
                pCropWindow->left,
                pCropWindow->top,
                pCropWindow->width,
                pCropWindow->height,
                fullInputWidth,
                fullInputHeight,
                pRefCropWindow->width,
                pRefCropWindow->height);

            cropInfo.fullPath.left   = (pCropWindow->left * fullInputWidth) / pRefCropWindow->width;
            cropInfo.fullPath.top    = (pCropWindow->top * fullInputHeight) / pRefCropWindow->height;
            cropInfo.fullPath.width  = (pCropWindow->width * fullInputWidth) / pRefCropWindow->width;
            cropInfo.fullPath.height = (pCropWindow->height * fullInputHeight) / pRefCropWindow->height;

            FLOAT ratio1 = static_cast<FLOAT>(cropInfo.fullPath.width) / static_cast<FLOAT>(fullInputWidth);
            FLOAT ratio2 = static_cast<FLOAT>(cropInfo.fullPath.height) / static_cast<FLOAT>(fullInputHeight);
            pInputData->postScaleRatio = (ratio1 > ratio2) ? ratio2 : ratio1;
            pInputData->preScaleRatio = 1.0f;

            if ((BPSProfileId::BPSProfileIdHNR == m_instanceProperty.profileId) &&
                (BPSProcessingType::BPSProcessingMFSR == m_instanceProperty.processingType))
            {
                if (m_curIntermediateDimension.ratio > 1.0f)
                {
                    // Update preScaleRatio and postScaleRatio for MFSR usecase
                    pInputData->preScaleRatio = 1.0f / m_curIntermediateDimension.ratio;
                    pInputData->postScaleRatio = pInputData->postScaleRatio / pInputData->preScaleRatio;

                    if (FALSE == Utils::FEqual(m_curIntermediateDimension.ratio, m_prevIntermediateDimension.ratio))
                    {
                        UpdateConfigIO(pCmdBuffer, m_curIntermediateDimension.width, m_curIntermediateDimension.height);
                        m_prevIntermediateDimension = m_curIntermediateDimension;
                    }
                }
            }

            CAMX_LOG_INFO(CamxLogGroupPProc,
                "BPS:%d crop w %d, h %d, fw %d, fh %d, iw %d, ih %d, preScaleRatio %f, postScaleRatio %f",
                InstanceID(),
                cropInfo.fullPath.width, cropInfo.fullPath.height,
                fullInputWidth, fullInputHeight,
                m_curIntermediateDimension.width, m_curIntermediateDimension.height,
                pInputData->preScaleRatio, pInputData->postScaleRatio);

            isScaleRatioSet = TRUE;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "crop window are NULL ");
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "cannot find vendor tag ref.cropsize");
    }

    return isScaleRatioSet;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::UpdateConfigIO
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::UpdateConfigIO(
    CmdBuffer* pCmdBuffer,
    UINT32     intermediateWidth,
    UINT32     intermediateHeight)
{
    CamxResult       result        = CamxResultSuccess;
    BpsConfigIo*     pConfigIO     = reinterpret_cast<BpsConfigIo*>(m_configIOMem.pVirtualAddr);
    BpsConfigIoData* pConfigIOData = &pConfigIO->cmdData;

    // MFSR: updating intermediate size in Config IO
    ImageDimensions  ds4Dimension = { 0 };
    ImageDimensions  ds16Dimension = { 0 };
    ImageDimensions  ds64Dimension = { 0 };
    GetDownscaleDimension(intermediateWidth, intermediateHeight, &ds4Dimension, &ds16Dimension, &ds64Dimension);

    pConfigIOData->images[BPS_INPUT_IMAGE].info.dimensions.widthPixels       = intermediateWidth;
    pConfigIOData->images[BPS_INPUT_IMAGE].info.dimensions.heightLines       = intermediateHeight;
    pConfigIOData->images[BPS_OUTPUT_IMAGE_FULL].info.dimensions.widthPixels = intermediateWidth;
    pConfigIOData->images[BPS_OUTPUT_IMAGE_FULL].info.dimensions.heightLines = intermediateHeight;
    pConfigIOData->images[BPS_OUTPUT_IMAGE_DS4].info.dimensions.widthPixels  = ds4Dimension.widthPixels;
    pConfigIOData->images[BPS_OUTPUT_IMAGE_DS4].info.dimensions.heightLines  = ds4Dimension.heightLines;
    pConfigIOData->images[BPS_OUTPUT_IMAGE_DS16].info.dimensions.widthPixels = ds16Dimension.widthPixels;
    pConfigIOData->images[BPS_OUTPUT_IMAGE_DS16].info.dimensions.heightLines = ds16Dimension.heightLines;
    pConfigIOData->images[BPS_OUTPUT_IMAGE_DS64].info.dimensions.widthPixels = ds64Dimension.widthPixels;
    pConfigIOData->images[BPS_OUTPUT_IMAGE_DS64].info.dimensions.heightLines = ds64Dimension.heightLines;

    DeInitializeStripingParams();
    result = InitializeStripingParams(pConfigIOData);
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Initialize Striping params failed %d", result);
    }
    else
    {
        m_configIORequest.pDeviceResourceParam = m_deviceResourceRequest.pDeviceResourceParam;
        result = PacketBuilder::WriteGenericBlobData(pCmdBuffer, CSLICPGenericBlobCmdBufferConfigIO,
                                                     sizeof(ConfigIORequest), reinterpret_cast<BYTE*>(&m_configIORequest));

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "BPS:%d WriteGenericBlobData() return %d", InstanceID(), result);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::DumpConfigIOData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::DumpConfigIOData()
{
    BpsConfigIo*     pConfigIO;
    BpsConfigIoData* pConfigIOData;

    pConfigIO     = reinterpret_cast<BpsConfigIo*>(m_configIOMem.pVirtualAddr);
    pConfigIOData = &pConfigIO->cmdData;

    CAMX_LOG_INFO(CamxLogGroupPProc, "BPS:%d Config IO Data Dump:", InstanceID());
    for (UINT i = 0; i < BPS_IO_IMAGES_MAX; i++)
    {
        CAMX_LOG_INFO(CamxLogGroupPProc, "BPS:%d image %d format=%d, width=%d, height=%d",
                       InstanceID(), i,
                       pConfigIOData->images[i].info.format,
                       pConfigIOData->images[i].info.dimensions.widthPixels,
                       pConfigIOData->images[i].info.dimensions.heightLines);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSNode::QueryMetadataPublishList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSNode::QueryMetadataPublishList(
    NodeMetadataList* pPublistTagList)
{
    for (UINT32 tagIndex = 0; tagIndex < NumBPSMetadataOutputTags; ++tagIndex)
    {
        pPublistTagList->tagArray[tagIndex] = BPSMetadataOutputTags[tagIndex];
    }

    pPublistTagList->tagCount = NumBPSMetadataOutputTags;
    CAMX_LOG_VERBOSE(CamxLogGroupMeta, "%u tags will be published", NumBPSMetadataOutputTags);
    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetMetadataContrastLevel
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::GetMetadataContrastLevel(
    ISPHALTagsData* pHALTagsData)
{
    UINT32     metaTagContrast = 0;
    CamxResult resultContrast  = VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.contrast",
                                                                            "level",
                                                                            &metaTagContrast);
    if (CamxResultSuccess == resultContrast)
    {
        static const UINT VendorTagContrast[] =
        {
            metaTagContrast | InputMetadataSectionMask,
        };

        const static UINT lengthContrast                    = CAMX_ARRAY_SIZE(VendorTagContrast);
        VOID* pDataContrast[lengthContrast]                 = { 0 };
        UINT64 vendorTagsContrastBPSOffset[lengthContrast]  = { 0 };

        GetDataList(VendorTagContrast, pDataContrast, vendorTagsContrastBPSOffset, lengthContrast);
        if (NULL != pDataContrast[0])
        {
            UINT8 appLevel = *(static_cast<UINT8*>(pDataContrast[0]));
            if (appLevel > 0)
            {
                pHALTagsData->contrastLevel = appLevel - 1;
            }
            else
            {
                pHALTagsData->contrastLevel = 5;
            }
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Manual Contrast Level = %d", pHALTagsData->contrastLevel);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Cannot obtain Contrast Level. Set default to 5");
            pHALTagsData->contrastLevel = 5;
        }
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupPProc, "No Contrast Level available. Set default to 5");
        pHALTagsData->contrastLevel = 5; // normal without contrast change
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSNode::GetMetadataTonemapCurve
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSNode::GetMetadataTonemapCurve(
    ISPHALTagsData* pHALTagsData)
{
    // Deep copy tone map curves, only when the tone map is contrast curve
    if (TonemapModeContrastCurve == pHALTagsData->tonemapCurves.tonemapMode)
    {
        ISPTonemapPoint* pBlueTonemapCurve  = NULL;
        ISPTonemapPoint* pGreenTonemapCurve = NULL;
        ISPTonemapPoint* pRedTonemapCurve   = NULL;

        static const UINT VendorTagsBPS[] =
        {
            InputTonemapCurveBlue,
            InputTonemapCurveGreen,
            InputTonemapCurveRed,
        };

        const static UINT length                    = CAMX_ARRAY_SIZE(VendorTagsBPS);
        VOID* pData[length]                         = { 0 };
        UINT64 vendorTagsTonemapBPSOffset[length]   = { 0 };

        GetDataList(VendorTagsBPS, pData, vendorTagsTonemapBPSOffset, length);

        if (NULL != pData[0])
        {
            pBlueTonemapCurve = static_cast<ISPTonemapPoint*>(pData[0]);
        }

        if (NULL != pData[1])
        {
            pGreenTonemapCurve = static_cast<ISPTonemapPoint*>(pData[1]);
        }

        if (NULL != pData[2])
        {
            pRedTonemapCurve = static_cast<ISPTonemapPoint*>(pData[2]);
        }

        CAMX_ASSERT(NULL != pBlueTonemapCurve);
        CAMX_ASSERT(NULL != pGreenTonemapCurve);
        CAMX_ASSERT(NULL != pRedTonemapCurve);

        pHALTagsData->tonemapCurves.curvePoints = static_cast<INT32>(
            GetDataCountFromPipeline(InputTonemapCurveBlue, 0, GetPipeline()->GetPipelineId(), TRUE));

        if ((pHALTagsData->tonemapCurves.curvePoints > 0) && (NULL != pBlueTonemapCurve))
        {
            // Blue tone map curve
            Utils::Memcpy(pHALTagsData->tonemapCurves.tonemapCurveBlue,
                          pBlueTonemapCurve,
                          (sizeof(ISPTonemapPoint) * pHALTagsData->tonemapCurves.curvePoints));
        }

        pHALTagsData->tonemapCurves.curvePoints = static_cast<INT32>(
            GetDataCountFromPipeline(InputTonemapCurveGreen, 0, GetPipeline()->GetPipelineId(), TRUE));

        if ((pHALTagsData->tonemapCurves.curvePoints > 0) && (NULL != pGreenTonemapCurve))
        {
            // Green tone map curve
            Utils::Memcpy(pHALTagsData->tonemapCurves.tonemapCurveGreen,
                          pGreenTonemapCurve,
                          (sizeof(ISPTonemapPoint) * pHALTagsData->tonemapCurves.curvePoints));
        }

        pHALTagsData->tonemapCurves.curvePoints = static_cast<INT32>(
            GetDataCountFromPipeline(InputTonemapCurveRed, 0, GetPipeline()->GetPipelineId(), TRUE));

        if ((pHALTagsData->tonemapCurves.curvePoints > 0) && (NULL != pRedTonemapCurve))
        {
            // Red tone map curve
            Utils::Memcpy(pHALTagsData->tonemapCurves.tonemapCurveRed,
                          pRedTonemapCurve,
                          (sizeof(ISPTonemapPoint) * pHALTagsData->tonemapCurves.curvePoints));
        }
    }
}

CAMX_NAMESPACE_END
