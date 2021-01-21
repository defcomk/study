////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipenode.cpp
/// @brief IPE Node class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if (!defined(LE_CAMERA)) // ANDROID
#include "qdMetaData.h"
#endif // ANDROID
#include "camxtrace.h"
#include "camxpipeline.h"
#include "camxcdmdefs.h"
#include "camxcslicpdefs.h"
#include "camxcslispdefs.h"
#include "camxcslresourcedefs.h"
#include "camxhal3metadatautil.h"
#include "camxhal3module.h"
#include "camxhwcontext.h"
#include "camximagebuffer.h"
#include "camximageformatutils.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "camxtitan17xcontext.h"
#include "camxtitan17xdefs.h"
#include "camxipe2dlut10.h"
#include "camxipeanr10.h"
#include "camxipeasf30.h"
#include "camxipecac22.h"
#include "camxipechromaenhancement12.h"
#include "camxipechromasuppression20.h"
#include "camxipecolorcorrection13.h"
#include "camxipecolortransform12.h"
#include "camxipegamma15.h"
#include "camxipegrainadder10.h"
#include "camxipeica.h"
#include "camxipeltm13.h"
#include "camxipesce11.h"
#include "camxipetf10.h"
#include "camxipeupscaler20.h"
#include "camxipenode.h"
#include "camxiqinterface.h"
#include "ipdefs.h"
#include "titan170_base.h"
#include "parametertuningtypes.h"
#include "camxtranslator.h"
#include "camxipeicatestdata.h"

// This function needs to be outside the CAMX_NAMESPACE because firmware uses "ImageFormat" that is used in UMD as well
ImageFormat TranslateFormatToFirmwareImageFormat(
    CamX::Format format)
{
    ImageFormat firmwareFormat = IMAGE_FORMAT_INVALID;

    switch (format)
    {
        case CamX::Format::YUV420NV12:
        case CamX::Format::YUV420NV21:
        case CamX::Format::FDYUV420NV12:
            firmwareFormat = IMAGE_FORMAT_LINEAR_NV12;
            break;
        case CamX::Format::UBWCTP10:
            firmwareFormat = IMAGE_FORMAT_UBWC_TP_10;
            break;
        case CamX::Format::UBWCNV12:
            firmwareFormat = IMAGE_FORMAT_UBWC_NV_12;
            break;
        case CamX::Format::UBWCNV124R:
            firmwareFormat = IMAGE_FORMAT_UBWC_NV12_4R;
            break;
        case CamX::Format::YUV420NV12TP10:
        case CamX::Format::YUV420NV21TP10:
            firmwareFormat = IMAGE_FORMAT_LINEAR_TP_10;
            break;
        case CamX::Format::PD10:
            firmwareFormat = IMAGE_FORMAT_PD_10;
            break;
        case CamX::Format::P010:
            firmwareFormat = IMAGE_FORMAT_LINEAR_P010;
            break;
        default:
            firmwareFormat = IMAGE_FORMAT_INVALID;
            break;
    }

    return firmwareFormat;
}

CAMX_NAMESPACE_BEGIN

static const UINT IPEMaxInput                    = 8;    ///< Number of Input Ports : 4 Input for image buffers and 4 for ref
static const UINT IPEMaxOutput                   = 6;    ///< Number of Output Ports: Display,  Video plus 4 ref output ports
static const UINT IPEMaxTopCmdBufferPatchAddress = 546;  ///< Number of Max address patching for top level payload
static const UINT IPEMaxPreLTMPatchAddress       = 16;   ///< Number of Max address patching for preLTM IQ modules
static const UINT IPEMaxPostLTMPatchAddress      = 16;   ///< Number of Max address patching for postLTM IQ modules
static const UINT IPEMaxDMIPatchAddress          = 52;   ///< Number of Max address patching for DMI headers
static const UINT IPEMaxNPSPatchAddress          = 8;    ///< Number of Max address patching for NPS (ANR/TF)
static const UINT IPEMaxPatchAddress             = IPEMaxTopCmdBufferPatchAddress +
                                                   IPEMaxPreLTMPatchAddress       +
                                                   IPEMaxPostLTMPatchAddress      +
                                                   IPEMaxNPSPatchAddress;         ///< Max address patches for packet;
static const UINT IPEDefaultDownScalarMode       = 1;
static const UINT IPEMidDownScalarMode           = 2;
static const UINT IPECustomDownScalarMode        = 3;
static const UINT IPEMidDownScalarWidth          = 4928;
static const UINT IPEMidDownScalarHeight         = 3808;

static const FLOAT IPEDownscaleThresholdMin      = 1.0f; ///< Min scale ratio above which downscaling causes IQ issues
static const FLOAT IPEDownscaleThresholdMax      = 1.05f; ///< Max scale ratio below which downscaling causes IQ issues

UINT32              IPENode::s_debugDataWriterCounter;  ///< Count instance of users of debug-data writer
DebugDataWriter*    IPENode::s_pDebugDataWriter;        ///< Access to debug-data writer

static const UINT  IPEMaxFWCmdBufferManagers     = 10;  ///< Max FW Command Buffer Managers



/// @brief Information about android to native effect.
struct IPEAndroidToCamxEffect
{
    ModeEffectSubModeType   from;           ///< Effect mode value
    ControlEffectModeValues to;             ///< Control effect Mode Value
};

/// @brief Information about android to native effect.
struct IPEAndroidToCamxScene
{
    ModeSceneSubModeType   from;            ///< Scene mode value
    ControlSceneModeValues to;              ///< Control scene mode value
};

// Map effects from CamX to Android
static IPEAndroidToCamxEffect IPEEffectMap[] =
{
    { ModeEffectSubModeType::None,       ControlEffectModeOff },
    { ModeEffectSubModeType::Mono,       ControlEffectModeMono },
    { ModeEffectSubModeType::Sepia,      ControlEffectModeSepia },
    { ModeEffectSubModeType::Negative,   ControlEffectModeNegative },
    { ModeEffectSubModeType::Solarize,   ControlEffectModeSolarize },
    { ModeEffectSubModeType::Posterize,  ControlEffectModePosterize },
    { ModeEffectSubModeType::Aqua,       ControlEffectModeAqua },
    { ModeEffectSubModeType::Emboss,     ControlEffectModeOff },
    { ModeEffectSubModeType::Sketch,     ControlEffectModeOff },
    { ModeEffectSubModeType::Neon,       ControlEffectModeOff },
    { ModeEffectSubModeType::Blackboard, ControlEffectModeBlackboard },
    { ModeEffectSubModeType::Whiteboard, ControlEffectModeWhiteboard },
};

// Map scene modes from CamX to Android
static IPEAndroidToCamxScene IPESceneMap[] =
{
    { ModeSceneSubModeType::None,          ControlSceneModeDisabled },
    { ModeSceneSubModeType::Landscape,     ControlSceneModeLandscape },
    { ModeSceneSubModeType::Snow,          ControlSceneModeSnow },
    { ModeSceneSubModeType::Beach,         ControlSceneModeBeach },
    { ModeSceneSubModeType::Sunset,        ControlSceneModeSunset },
    { ModeSceneSubModeType::Night,         ControlSceneModeNight },
    { ModeSceneSubModeType::Portrait,      ControlSceneModePortrait },
    { ModeSceneSubModeType::BackLight,     ControlSceneModeDisabled },
    { ModeSceneSubModeType::Sports,        ControlSceneModeSports },
    { ModeSceneSubModeType::AntiShake,     ControlSceneModeDisabled },
    { ModeSceneSubModeType::Flowers,       ControlSceneModeDisabled },
    { ModeSceneSubModeType::CandleLight,   ControlSceneModeCandlelight },
    { ModeSceneSubModeType::Fireworks,     ControlSceneModeFireworks },
    { ModeSceneSubModeType::Party,         ControlSceneModeParty },
    { ModeSceneSubModeType::NightPortrait, ControlSceneModeNightPortrait },
    { ModeSceneSubModeType::Theater,       ControlSceneModeTheatre },
    { ModeSceneSubModeType::Action,        ControlSceneModeAction },
    { ModeSceneSubModeType::AR,            ControlSceneModeDisabled },
    { ModeSceneSubModeType::FacePriority,  ControlSceneModeFacePriority },
    { ModeSceneSubModeType::Barcode,       ControlSceneModeBarcode },
    { ModeSceneSubModeType::BestShot,      ControlSceneModeDisabled },
};

// The third column (installed) indicates if the module is available/enabled on a particular chipset e.g. bit 0 corresponds to
// SDM845/SDM710 etc. while bit 1 corresponds to SDM855. Setting the value of 0x0 disables the module.
//
// For example:
//
// ISPHwTitanInvalid (0b00)                               : No installation
// ISPHwTitan170 (0b01)                                   : Enable and applicable SDM845/SDM710 only
// ISPHwTitan175 (0b10)                                   : Enable and applicable SDM855 (SM8150) only
// (ISPHwTitan170 | ISPHwTitan175) (0b11)                 : Enable and applicable both SDM845/SDM710 and SDM855 (SM8150)
// ISPHwTitan150 (0b100)                                  : Enable and applicable SDM640(SM6150) only.
// (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150) (0b111): Enable and applicable for SDM845/SDM710/SDM640/SDM855

// This list will follow order of modules in real hardware
static IPEIQModuleInfo IQModulesList[] =
{
    { ISPIQModuleType::IPEICA,               IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEICA::Create },
    { ISPIQModuleType::IPE2DLUT,             IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPE2DLUT10::Create },
    { ISPIQModuleType::IPEANR,               IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEANR10::Create },
    { ISPIQModuleType::IPETF,                IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPETF10::Create },
    { ISPIQModuleType::IPEICA,               IPEPath::REFERENCE, (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEICA::Create },
    { ISPIQModuleType::IPECAC,               IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175),
    IPECAC22::Create },
    { ISPIQModuleType::IPECST,               IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEColorTransform12::Create },
    { ISPIQModuleType::IPELTM,               IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPELTM13::Create },
    { ISPIQModuleType::IPEColorCorrection,   IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEColorCorrection13::Create },
    { ISPIQModuleType::IPEGamma,             IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEGamma15::Create },
    { ISPIQModuleType::IPEChromaEnhancement, IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEChromaEnhancement12::Create },
    { ISPIQModuleType::IPEChromaSuppression, IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEChromaSuppression20::Create },
    { ISPIQModuleType::IPESCE,               IPEPath::INPUT,     ISPHwTitanInvalid,  IPESCE11::Create },
    { ISPIQModuleType::IPEASF,               IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEASF30::Create },
    { ISPIQModuleType::IPEUpscaler,          IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175),
    IPEUpscaler20::Create },
    { ISPIQModuleType::IPEUpscaler,          IPEPath::INPUT,     ISPHwTitan150,
    IPEUpscaler12::Create },
    { ISPIQModuleType::IPEGrainAdder,        IPEPath::INPUT,     (ISPHwTitan170 | ISPHwTitan175 | ISPHwTitan150),
    IPEGrainAdder10::Create },
};

static const UINT CmdBufferFrameProcessSizeBytes = sizeof(IpeFrameProcess) +                    ///< firmware requires different
    (static_cast<UINT>(CDMProgramArrayOrder::ProgramArrayMax) * sizeof(CDMProgramArray)) +      ///< CDM programs in payload
    (static_cast<UINT>(PreLTMCDMProgramOrder::ProgramIndexMaxPreLTM) +                          ///< are appended to Frame
     static_cast<UINT>(PostLTMCDMProgramOrder::ProgramIndexMaxPostLTM) * sizeof(CdmProgram));   ///< process data

// Private Static member holding fixed values of Frame buffer offsets within IpeFrameProcess struct, for ease of patching
FrameBuffers IPENode::s_frameBufferOffset[IPEMaxSupportedBatchSize][IPE_IO_IMAGES_MAX];

static const UINT CmdBufferGenericBlobSizeInBytes = (
    CSLGenericBlobHeaderSizeInDwords * sizeof(UINT32) +
    sizeof(CSLICPClockBandwidthRequest) +
    CSLGenericBlobHeaderSizeInDwords * sizeof(UINT32) +
    sizeof(ConfigIORequest) +
    CSLGenericBlobHeaderSizeInDwords * sizeof(UINT32) +
    sizeof(CSLICPMemoryMapUpdate));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::IPENode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPENode::IPENode()
{
    m_pNodeName                 = "IPE";
    m_OEMStatsSettingEnable     = GetStaticSettings()->IsOEMStatSettingEnable;
    m_enableIPEHangDump         = GetStaticSettings()->enableIPEHangDump;
    m_enableIPEStripeDump       = GetStaticSettings()->enableIPEStripeDump;
    m_pPreTuningDataManager     = NULL;
    m_compressiononOutput       = FALSE;
    m_inputPortIPEPassesMask    = 0;
    m_configIOMem               = { 0 };
    m_deviceResourceRequest     = { 0 };
    m_curIntermediateDimension  = { 0, 0, 1.0f };
    m_prevIntermediateDimension = { 0, 0, 1.0f };
    m_maxICAUpscaleLimit        = 2.0f; // by default is 2.0f
    m_stabilizationMargin       = { 0 };
    m_EISMarginRequest          = { 0 };
    m_additionalCropOffset      = { 0 };
    m_firstValidRequest         = FirstValidRequestId;
    m_referenceBufferCount      = ReferenceBufferCount;
    BOOL      enableRefDump     = FALSE;
    UINT32    enabledNodeMask   = GetStaticSettings()->autoImageDumpMask;
    UINT32    refOutputPortMask = 0x0;

    for (UINT pass = PASS_NAME_FULL; pass < PASS_NAME_MAX; pass++)
    {
        //  Intialize loop back portIDs
        refOutputPortMask |= (1 << (pass + IPE_OUTPUT_IMAGE_FULL_REF));
    }

    if (IsIPEReferenceDumpEnabled(enabledNodeMask, refOutputPortMask))
    {
        // When Reference output dump buffer count is 2; and UMD is trying to
        // dump an IPE Reference output buffer for given request x then there is
        // a chance that request x+2 might be updating the same bufer concurrently
        // and thus creating a race condition. So, to avoid this scenario increase
        // Reference output buffer count to more than 2 buffers, say 4.
        m_referenceBufferCount = MaxReferenceBufferCount;
    }

    for (UINT batchIndex = 0; batchIndex < IPEMaxSupportedBatchSize; batchIndex++)
    {
        for (UINT port = 0; port < IPE_IO_IMAGES_MAX; port++)
        {
            for (UINT plane = 0; plane < MAX_NUM_OF_IMAGE_PLANES; plane++)
            {
                // Precompute the frame buffer offset for all ports
                s_frameBufferOffset[batchIndex][port].bufferPtr[plane] =
                    static_cast<UINT32>(offsetof(IpeFrameProcessData, frameSets[0]) + sizeof(FrameSet) * batchIndex) +
                    static_cast<UINT32>(offsetof(FrameSet, buffers[0]) + sizeof(FrameBuffers) * port) +
                    static_cast<UINT32>(offsetof(FrameBuffers, bufferPtr[0]) + (sizeof(FrameBufferPtr) * plane));

                s_frameBufferOffset[batchIndex][port].metadataBufferPtr[plane] =
                    static_cast<UINT32>(offsetof(IpeFrameProcessData, frameSets[0]) + sizeof(FrameSet) * batchIndex) +
                    static_cast<UINT32>(offsetof(FrameSet, buffers[0]) + sizeof(FrameBuffers) * port) +
                    static_cast<UINT32>(offsetof(FrameBuffers, metadataBufferPtr[0]) + (sizeof(FrameBufferPtr) * plane));
            }
        }
    }

    Utils::Memset(&m_loopBackBufferParams[0], 0x0, sizeof(m_loopBackBufferParams));

    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ipeicaconfigs",
                                                                              "ICAInPerspectiveTransform",
                                                                              &m_IPEICATAGLocation[0]));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ipeicaconfigs",
                                                                              "ICAInGridTransform",
                                                                              &m_IPEICATAGLocation[1]));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ipeicaconfigs",
                                                                              "ICAInInterpolationParams",
                                                                              &m_IPEICATAGLocation[2]));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ipeicaconfigs",
                                                                              "ICARefPerspectiveTransform",
                                                                              &m_IPEICATAGLocation[3]));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ipeicaconfigs",
                                                                              "ICARefGridTransform",
                                                                              &m_IPEICATAGLocation[4]));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ipeicaconfigs",
                                                                              "ICARefInterpolationParams",
                                                                              &m_IPEICATAGLocation[5]));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ipeicaconfigs",
                                                                              "ICAReferenceParams",
                                                                              &m_IPEICATAGLocation[6]));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ipeicaconfigs",
                                                                              "ICAInPerspectiveTransformLookAhead",
                                                                              &m_IPEICATAGLocation[7]));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ipeicaconfigs",
                                                                              "ICAInGridTransformLookahead",
                                                                              &m_IPEICATAGLocation[8]));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.mfnrconfigs",
                                                                              "MFNRTotalNumFrames",
                                                                              &m_MFNRTotalNumFramesTAGLocation));
    CAMX_ASSERT(CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera2.mfsrconfigs",
                                                                              "MFSRTotalNumFrames",
                                                                              &m_MFSRTotalNumFramesTAGLocation));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::~IPENode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPENode::~IPENode()
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

        ReleaseDevice();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DumpPayload
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static CamxResult DumpPayload(
    IPECmdBufferId  index,
    CmdBuffer*      pIPECmdBuffer,
    UINT64          requestId)
{
    CamxResult result = CamxResultSuccess;
    CHAR       filename[100];

    /// @todo (CAMX-2491) Remove this way of getting settings
    if (TRUE == HwEnvironment::GetInstance()->GetStaticSettings()->dumpIPEFirmwarePayload)
    {
        switch (index)
        {
            case CmdBufferFrameProcess:
                CamX::OsUtils::SNPrintF(filename, sizeof(filename), "%s/IPEDumpFrameProcessData_%lld.txt",
                                        ConfigFileDirectory, requestId);
                break;
            case CmdBufferStriping:
                CamX::OsUtils::SNPrintF(filename, sizeof(filename), "%s/IPEDumpStripingOutput_%lld.txt",
                                        ConfigFileDirectory, requestId);
                break;
            case CmdBufferIQSettings:
                CamX::OsUtils::SNPrintF(filename, sizeof(filename), "%s/IPEDumpIQSettings_%lld.txt",
                                        ConfigFileDirectory, requestId);
                break;
            case CmdBufferPreLTM:
                CamX::OsUtils::SNPrintF(filename, sizeof(filename), "%s/IPEDumpPreLTM_%lld.txt",
                                        ConfigFileDirectory, requestId);
                break;
            case CmdBufferPostLTM:
                CamX::OsUtils::SNPrintF(filename, sizeof(filename), "%s/IPEDumpPostLTM_%lld.txt",
                                        ConfigFileDirectory, requestId);
                break;
            case CmdBufferDMIHeader:
                CamX::OsUtils::SNPrintF(filename, sizeof(filename), "%s/IPEDumpDMIHeader_%lld.txt",
                                        ConfigFileDirectory, requestId);
                break;
            case CmdBufferNPS:
                CamX::OsUtils::SNPrintF(filename, sizeof(filename), "%s/IPEDumpNPS_%lld.txt",
                                        ConfigFileDirectory, requestId);
                break;
            default:
                result = CamxResultEInvalidArg;
                break;
        }

        if (CamxResultSuccess == result)
        {
            FILE* pFile = CamX::OsUtils::FOpen(filename, "wb");
            if (NULL != pFile)
            {
                CamX::OsUtils::FWrite(pIPECmdBuffer->GetHostAddr(), pIPECmdBuffer->GetMaxLength(), 1, pFile);
                CamX::OsUtils::FClose(pFile);
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DumpDebug
///
/// @brief: This is called when firmware signal an error and UMD needs firmware dump
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static CamxResult DumpDebug(
    IPECmdBufferId          index,
    CmdBuffer*              pBuffer,
    UINT64                  requestId,
    UINT32                  realtime,
    IPEInstanceProperty     instanceProperty,
    const CHAR*             pPipelineName)
{
    CamxResult result = CamxResultSuccess;
    CHAR       filename[512];

    if (index == CmdBufferBLMemory)
    {
        switch (index)
        {
            case CmdBufferBLMemory:
                CamX::OsUtils::SNPrintF(filename, sizeof(filename),
                    "%s/IPEBLMemoryDump_%s_%llu_rt_%d_processType_%d_profId_%d_stabT_%d.txt",
                    ConfigFileDirectory, pPipelineName,
                    requestId, realtime,
                    instanceProperty.processingType, instanceProperty.profileId,
                    instanceProperty.stabilizationType);
                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "dump bl buffer %s", filename);
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
                CAMX_LOG_ERROR(CamxLogGroupPProc, "### Can't open file");
                return CamxResultEFailed;
            }
            CamX::OsUtils::FWrite(pBuffer->GetHostAddr(), pBuffer->GetMaxLength(), 1, pFile);
            CamX::OsUtils::FClose(pFile);
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DumpScratchBuffer
///
/// @brief: This is called when scratchBuffer dump is enabled in static settings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void DumpScratchBuffer(
            CSLBufferInfo**       ppScratchBuffer,
            UINT                  numScratchBuffer,
            UINT64                requestId,
            UINT32                instanceID,
            UINT32                realtime,
            IPEInstanceProperty   instanceProperty,
            Pipeline*             pPipelineName)
{
    CHAR filename[512] = { 0 };

    for (UINT count = 0; count < numScratchBuffer; count++)
    {
        if ((NULL != ppScratchBuffer[count]) && (NULL != ppScratchBuffer[count]->pVirtualAddr))
        {
            CamX::OsUtils::SNPrintF(filename, sizeof(filename),
                "%s/IPEScratchBufferDump_Pipeline_%p_req_%llu_inst_%d_rt_%d_profId_%d.txt",
                ConfigFileDirectory,
                pPipelineName,
                requestId, instanceID, realtime, instanceProperty.profileId);
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Dumping Scratch buffer: %s", filename);

            FILE* pFile = CamX::OsUtils::FOpen(filename, "wb");
            if (!pFile)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Can't open Scratch Buffer file");
            }
            else
            {
                CamX::OsUtils::FWrite(ppScratchBuffer[count]->pVirtualAddr, ppScratchBuffer[count]->size,
                                        1, pFile);
                CamX::OsUtils::FClose(pFile);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::TranslateToFirmwarePortId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE VOID IPENode::TranslateToFirmwarePortId(
    UINT32          portId,
    IPE_IO_IMAGES*  pFirmwarePortId)
{
    CAMX_ASSERT(portId < static_cast<UINT32>(IPE_IO_IMAGES::IPE_IO_IMAGES_MAX));

    *pFirmwarePortId = static_cast<IPE_IO_IMAGES>(portId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPENode* IPENode::Create(
    const NodeCreateInputData* pCreateInputData,
    NodeCreateOutputData*      pCreateOutputData)
{
    CAMX_UNREFERENCED_PARAM(pCreateOutputData);

    if ((NULL != pCreateInputData) && (NULL != pCreateInputData->pNodeInfo))
    {
        INT32            stabType        = 0;
        UINT32           propertyCount   = pCreateInputData->pNodeInfo->nodePropertyCount;
        PerNodeProperty* pNodeProperties = pCreateInputData->pNodeInfo->pNodeProperties;

        IPENode* pNodeObj = CAMX_NEW IPENode;

        if (NULL != pNodeObj)
        {
            pNodeObj->m_nodePropDisableZoomCrop = FALSE;

            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "nodePropertyCount %d", propertyCount);

            // There can be multiple IPE instances in a pipeline, each instance can have differnt IQ modules enabled
            for (UINT32 count = 0; count < propertyCount; count++)
            {
                UINT32 nodePropertyId    = pNodeProperties[count].id;
                VOID* pNodePropertyValue = pNodeProperties[count].pValue;

                switch (nodePropertyId)
                {
                    case NodePropertyProfileId:
                        pNodeObj->m_instanceProperty.profileId = static_cast<IPEProfileId>(
                            atoi(static_cast<const CHAR*>(pNodePropertyValue)));
                        break;
                    case NodePropertyStabilizationType:
                        // If EIS is enabled, IPE instance needs to know if its in EIS2.0 path 3.0.
                        // Node property value is shifted to use multiple stabilization type together.
                        stabType |= (1 << (atoi(static_cast<const CHAR*>(pNodePropertyValue))));

                        // Check if EIS ICA dependency need to be bypassed
                        if ((TRUE == HwEnvironment::GetInstance()->GetStaticSettings()->bypassIPEICADependency) &&
                            ((IPEStabilizationTypeEIS2 & stabType) ||
                            (IPEStabilizationTypeEIS3 & stabType)))
                        {
                            stabType &= ~(IPEStabilizationTypeEIS2 | IPEStabilizationTypeEIS3);
                            CAMX_LOG_INFO(CamxLogGroupPProc, "EIS stabalization disabled stabType %d", stabType);
                        }

                        pNodeObj->m_instanceProperty.stabilizationType = static_cast<IPEStabilizationType>(stabType);
                        break;
                    case NodePropertyProcessingType:
                        // If MFNR is enabled, IPE instance needs to know if its in prefilter/blend/scale or postfilter.
                        pNodeObj->m_instanceProperty.processingType = static_cast<IPEProcessingType>(
                            atoi(static_cast<const CHAR*>(pNodePropertyValue)));
                        break;
                    case NodePropertyIPEDownscale:
                        pNodeObj->m_instanceProperty.ipeOnlyDownscalerMode =
                            atoi(static_cast<const CHAR*>(pNodePropertyValue));
                        break;
                    case NodePropertyIPEDownscaleWidth:
                        pNodeObj->m_instanceProperty.ipeDownscalerWidth =
                            atoi(static_cast<const CHAR*>(pNodePropertyValue));
                        break;
                    case NodePropertyIPEDownscaleHeight:
                        pNodeObj->m_instanceProperty.ipeDownscalerHeight =
                            atoi(static_cast<const CHAR*>(pNodePropertyValue));
                        break;
                    case NodePropertyEnbaleIPECHICropDependency:
                        pNodeObj->m_instanceProperty.enableCHICropInfoPropertyDependency = static_cast<BOOL>(
                            atoi(static_cast<const CHAR*>(pNodePropertyValue)));
                        break;
                    case NodePropertyEnableFOVC:
                        pNodeObj->m_instanceProperty.enableFOVC = *static_cast<UINT*>(pNodePropertyValue);
                        break;
                    default:
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unhandled node property Id %d", nodePropertyId);
                        break;
                }
            }

            CAMX_LOG_INFO(CamxLogGroupPProc, "IPE Instance profileId: %d stabilization: %d processing: %d, "
                          "ipeOnlyDownscalerMode: %d, width: %d, height: %d, enbaleIPECHICropDependency: %d, "
                          "enableFOVC: %d",
                          pNodeObj->m_instanceProperty.profileId,
                          pNodeObj->m_instanceProperty.stabilizationType,
                          pNodeObj->m_instanceProperty.processingType,
                          pNodeObj->m_instanceProperty.ipeOnlyDownscalerMode,
                          pNodeObj->m_instanceProperty.ipeDownscalerWidth,
                          pNodeObj->m_instanceProperty.ipeDownscalerHeight,
                          pNodeObj->m_instanceProperty.enableCHICropInfoPropertyDependency,
                          pNodeObj->m_instanceProperty.enableFOVC);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to create IPENode, no memory");
        }

        return pNodeObj;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null input pointer");
        return NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::Destroy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::Destroy()
{
    CAMX_DELETE this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::ProcessingNodeInitialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::ProcessingNodeInitialize(
    const NodeCreateInputData* pCreateInputData,
    NodeCreateOutputData*      pCreateOutputData)
{
    CAMX_UNREFERENCED_PARAM(pCreateInputData);

    CamxResult      result                      = CamxResultSuccess;

    INT32           deviceIndex                 = -1;
    UINT            indicesLengthRequired       = 0;

    CAMX_ASSERT(IPE == Type());
    CAMX_ASSERT(NULL != pCreateOutputData);

    Titan17xContext* pContext = static_cast<Titan17xContext*>(GetHwContext());
    m_OEMICASettingEnable     = pContext->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->IsICAIQSettingEnable;
    m_OEMIQSettingEnable      = pContext->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->IsOEMIQSettingEnable;

    pCreateOutputData->maxOutputPorts = IPEMaxOutput;
    pCreateOutputData->maxInputPorts  = IPEMaxInput;

    UINT numOutputPorts = 0;
    UINT outputPortId[MaxBufferComposite];

    GetAllOutputPortIds(&numOutputPorts, &outputPortId[0]);

    CAMX_ASSERT(MaxBufferComposite >= numOutputPorts);

    UINT32 groupID = ISPOutputGroupIdMAX;
    for (UINT outputPortIndex = 0; outputPortIndex < numOutputPorts; outputPortIndex++)
    {
        switch (outputPortId[outputPortIndex])
        {
            case IPEOutputPortDisplay:
                pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                                                                               ISPOutputGroupId0;
                break;

            case IPEOutputPortVideo:
                pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] =
                    ISPOutputGroupId1;
                break;

            case IPEOutputPortFullRef:
            case IPEOutputPortDS4Ref:
            case IPEOutputPortDS16Ref:
            case IPEOutputPortDS64Ref:
                pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] = ISPOutputGroupId2;
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
    result = HwEnvironment::GetInstance()->GetDeviceIndices(CSLDeviceTypeICP, &deviceIndex, 1, &indicesLengthRequired);

    if (CamxResultSuccess == result)
    {
        CAMX_ASSERT(indicesLengthRequired == 1);
        result = AddDeviceIndex(deviceIndex);
        m_deviceIndex = deviceIndex;
    }

    SetLoopBackPortEnable();

    // If IPE tuning-data enable, initialize debug-data writer
    if ((CamxResultSuccess  == result)                                      &&
        (TRUE               == GetStaticSettings()->enableTuningMetadata)   &&
        (0                  != GetStaticSettings()->tuningDumpDataSizeIPE))
    {
        m_pTuningMetadata = static_cast<IPETuningMetadata*>(CAMX_CALLOC(sizeof(IPETuningMetadata)));
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

    m_OEMStatsConfig = GetStaticSettings()->IsOEMStatSettingEnable;
    Utils::Memset(&m_adrcInfo, 0, sizeof(PropertyISPADRCInfo));
    m_adrcInfo.enable = FALSE;

    // Configure IPE Capability
    result = ConfigureIPECapability();

    // Striping already considers 2 IPEs if it is used. This is accomodated in the frameCycles.
    // So the efficiency can be always  IPEClockEfficiency / 2
    m_IPEClockEfficiency = IPEClockEfficiency / 2;
    CAMX_LOG_INFO(CamxLogGroupISP, "m_IPEClockEfficiency %f  numIPE %d", m_IPEClockEfficiency, m_capability.numIPE);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetGammaOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetGammaOutput(
    ISPInternalData* pISPData,
    UINT32           parentID)
{
    CamxResult result       = CamxResultSuccess;
    UINT32*    pGammaOutput = NULL;
    UINT32     gammaLength  = 0;
    UINT32     metaTag = 0;
    BOOL       isGammaValid = FALSE;

    //  Get the Gamma output from IFE / BPS
    if (parentID == IFE)
    {
        static const UINT PropertiesIPE[] =
        {
            PropertyIDIFEGammaOutput,
        };
        const UINT length                    = CAMX_ARRAY_SIZE(PropertiesIPE);
        VOID* pData[length]                  = { 0 };
        UINT64 propertyDataIPEOffset[length] = { 0 };
        GetDataList(PropertiesIPE, pData, propertyDataIPEOffset, length);

        if (NULL != pData[0])
        {
            pGammaOutput = reinterpret_cast<GammaInfo*>(pData[0])->gammaG;
            gammaLength = sizeof(reinterpret_cast<GammaInfo*>(pData[0])->gammaG);
            isGammaValid = reinterpret_cast<GammaInfo*>(pData[0])->isGammaValid;
        }
    }
    else if (parentID == BPS)
    {
        static const UINT PropertiesIPE[] =
        {
            PropertyIDBPSGammaOutput,
        };
        const UINT length = CAMX_ARRAY_SIZE(PropertiesIPE);
        VOID* pData[length] = { 0 };
        UINT64 propertyDataIPEOffset[length] = { 0 };
        GetDataList(PropertiesIPE, pData, propertyDataIPEOffset, length);

        if (NULL != pData[0])
        {
            pGammaOutput = reinterpret_cast<GammaInfo*>(pData[0])->gammaG;
            gammaLength = sizeof(reinterpret_cast<GammaInfo*>(pData[0])->gammaG);
            isGammaValid = reinterpret_cast<GammaInfo*>(pData[0])->isGammaValid;
        }
    }
    else
    {
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.gammainfo",
            "GammaInfo",
            &metaTag);
        if (CamxResultSuccess == result)
        {
            static const UINT PropertiesIPE[] = { metaTag | InputMetadataSectionMask };
            const UINT length = CAMX_ARRAY_SIZE(PropertiesIPE);
            VOID* pData[length] = { 0 };
            UINT64 propertyDataIPEOffset[length] = { 0 };
            GetDataList(PropertiesIPE, pData, propertyDataIPEOffset, length);
            if (NULL != pData[0])
            {
                pGammaOutput = reinterpret_cast<GammaInfo*>(pData[0])->gammaG;
                gammaLength = sizeof(reinterpret_cast<GammaInfo*>(pData[0])->gammaG);
                isGammaValid = reinterpret_cast<GammaInfo*>(pData[0])->isGammaValid;
            }
        }
    }

    pISPData->gammaOutput.isGammaValid = isGammaValid;
    if (NULL != pGammaOutput)
    {
        CAMX_ASSERT(gammaLength == sizeof(pISPData->gammaOutput.gammaG));
        Utils::Memcpy(pISPData->gammaOutput.gammaG,
                      pGammaOutput,
                      gammaLength);
    }
    else
    {
        result = CamxResultEResource;
        CAMX_LOG_WARN(CamxLogGroupPProc, "Error in getting gamma output slot");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetGammaPreCalculationOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetGammaPreCalculationOutput(
    ISPInternalData* pISPData)
{
    //  The Gamma pre-calculated output comes from gamma15 module(in IPE) that executed in TMC1.1.
    //  It is different from the IPENode::GetGammaOutput function, which its output comes from Gamma16 module in IFE/BPS.
    //  Due to TMC 1.1 needs Gamma15 LUT output as its input, but Gamma15 is actually located in IPE.
    //  So we do a pre-calculation at TMC 1.1 in IFE/BPS. Then we can get the pre-calculated output from metadata for
    //  Gamma15 in IPE instead of calculating it again.

    CamxResult result       = CamxResultSuccess;

    static const UINT PropertiesIPE[] =
    {
        PropertyIDIPEGamma15PreCalculation,
    };

    const UINT length                    = CAMX_ARRAY_SIZE(PropertiesIPE);
    VOID* pData[length]                  = { 0 };
    UINT64 propertyDataIPEOffset[length] = { 0 };
    GetDataList(PropertiesIPE, pData, propertyDataIPEOffset, length);

    if (NULL != pData[0])
    {
        pISPData->IPEGamma15PreCalculationOutput = *reinterpret_cast<IPEGammaPreOutput*>(pData[0]);
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupPProc, "Cannot get data of gamma15 pre-calculation from meta");
    }

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetIntermediateDimension
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetIntermediateDimension()
{
    CamxResult result  = CamxResultSuccess;
    UINT32     metaTag = 0;

    IntermediateDimensions*  pIntermediateDimension = NULL;

    if ((IPEProcessingType::IPEMFSRPrefilter < m_instanceProperty.processingType) &&
        (IPEProcessingType::IPEMFSRPostfilter >= m_instanceProperty.processingType))
    {
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.intermediateDimension",
                                                          "IntermediateDimension",
                                                          &metaTag);
        if (CamxResultSuccess == result)
        {
            static const UINT PropertiesIPE[]        = { metaTag | InputMetadataSectionMask };
            const UINT        length                 = CAMX_ARRAY_SIZE(PropertiesIPE);
            VOID*             pData[length]          = { 0 };
            UINT64            pDataIPEOffset[length] = { 0 };
            GetDataList(PropertiesIPE, pData, pDataIPEOffset, length);
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

            CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d intermediate width=%d height=%d ratio=%f",
                           InstanceID(),
                           m_curIntermediateDimension.width,
                           m_curIntermediateDimension.height,
                           m_curIntermediateDimension.ratio);
        }
        else
        {
            result = CamxResultEResource;
            CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d Error in getting intermediate dimension slot", InstanceID());
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetADRCInfoOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetADRCInfoOutput()
{
    CamxResult           result       = CamxResultSuccess;
    PropertyISPADRCInfo* pADRCProperty = NULL;
    UINT propertiesIPE[1] = {0};

    if ((TRUE == IsNodeInPipeline(IFE)) && (TRUE == m_realTimeIPE))
    {
        //  Check m_realTimeIPE for the case which IFE and BPS are both in the pipeline.
        propertiesIPE[0] = PropertyIDIFEADRCInfoOutput;
    }
    else if (TRUE == IsNodeInPipeline(BPS))
    {
        propertiesIPE[0] = PropertyIDBPSADRCInfoOutput;
    }
    else
    {
        //  For the standalone IPE pipeline which is not scale type, we'll go to input pool for the ADRC info.
        propertiesIPE[0] = PropertyIDIFEADRCInfoOutput | InputMetadataSectionMask;
    }

    const UINT length                    = CAMX_ARRAY_SIZE(propertiesIPE);
    VOID* pData[length]                  = { 0 };
    UINT64 propertyDataIPEOffset[length] = { 0 };
    GetDataList(propertiesIPE, pData, propertyDataIPEOffset, length);
    if (NULL != pData[0])
    {
        pADRCProperty = reinterpret_cast<PropertyISPADRCInfo*>(pData[0]);

        m_adrcInfo.enable    = pADRCProperty->enable;
        m_adrcInfo.version   = pADRCProperty->version;
        m_adrcInfo.gtmEnable = pADRCProperty->gtmEnable;
        m_adrcInfo.ltmEnable = pADRCProperty->ltmEnable;

        if (SWTMCVersion::TMC10 == m_adrcInfo.version)
        {
            Utils::Memcpy(&m_adrcInfo.kneePoints.KneePointsTMC10.kneeX,
                            pADRCProperty->kneePoints.KneePointsTMC10.kneeX,
                            sizeof(FLOAT) * MAX_ADRC_LUT_KNEE_LENGTH_TMC10);
            Utils::Memcpy(&m_adrcInfo.kneePoints.KneePointsTMC10.kneeY,
                            pADRCProperty->kneePoints.KneePointsTMC10.kneeY,
                            sizeof(FLOAT) * MAX_ADRC_LUT_KNEE_LENGTH_TMC10);
        }
        else if (SWTMCVersion::TMC11 == m_adrcInfo.version)
        {
            Utils::Memcpy(&m_adrcInfo.kneePoints.KneePointsTMC11.kneeX,
                            pADRCProperty->kneePoints.KneePointsTMC11.kneeX,
                            sizeof(FLOAT) * MAX_ADRC_LUT_KNEE_LENGTH_TMC11);
            Utils::Memcpy(&m_adrcInfo.kneePoints.KneePointsTMC11.kneeY,
                            pADRCProperty->kneePoints.KneePointsTMC11.kneeY,
                            sizeof(FLOAT) * MAX_ADRC_LUT_KNEE_LENGTH_TMC11);
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Unknown SWTMCVersion %u, from property %x",
                                m_adrcInfo.version,
                                propertiesIPE[0]);
        }

        m_adrcInfo.drcGainDark   = pADRCProperty->drcGainDark;
        m_adrcInfo.ltmPercentage = pADRCProperty->ltmPercentage;
        m_adrcInfo.gtmPercentage = pADRCProperty->gtmPercentage;

        Utils::Memcpy(&m_adrcInfo.coef,
                        pADRCProperty->coef,
                        sizeof(FLOAT) * MAX_ADRC_LUT_COEF_SIZE);

        Utils::Memcpy(&m_adrcInfo.pchipCoef,
                        pADRCProperty->pchipCoef,
                        sizeof(FLOAT) * MAX_ADRC_LUT_PCHIP_COEF_SIZE);
        Utils::Memcpy(&m_adrcInfo.contrastEnhanceCurve,
                        pADRCProperty->contrastEnhanceCurve,
                        sizeof(FLOAT) * MAX_ADRC_CONTRAST_CURVE);

        m_adrcInfo.curveModel       = pADRCProperty->curveModel;
        m_adrcInfo.contrastHEBright = pADRCProperty->contrastHEBright;
        m_adrcInfo.contrastHEDark   = pADRCProperty->contrastHEDark;
    }
    else
    {
        m_adrcInfo.enable = FALSE;
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Can't get ADRC Info!!!, pData is NULL");
    }

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "adrcEnabled = %d, percentageOfGTM = %f, TMC version = %d, GTM:%d, LTM:%d",
                    m_adrcInfo.enable,
                    m_adrcInfo.gtmPercentage,
                    m_adrcInfo.version,
                    m_adrcInfo.gtmEnable,
                    m_adrcInfo.ltmEnable);
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillIQSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillIQSetting(
    ISPInputData*            pInputData,
    IpeIQSettings*           pIPEIQsettings,
    PerRequestActivePorts*   pPerRequestPorts)
{
    CamxResult             result               = CamxResultSuccess;
    UINT32                 clampValue           = 0;
    const ImageFormat*     pInputImageFormat    = NULL;
    const ImageFormat*     pOutputImageFormat   = NULL;
    BOOL                   isInput10bit         = FALSE;

    /// @todo (CAMX-729) Implement IPE IQ modules
    if (NULL != pPerRequestPorts)
    {
        // Loop to find if the port is IPE full port
        for (UINT portIndex = 0; portIndex < pPerRequestPorts->numInputPorts; portIndex++)
        {
            PerRequestInputPortInfo* pInputPort = &pPerRequestPorts->pInputPorts[portIndex];

            if (IPEInputPortFull == pInputPort->portId)
            {
                pInputImageFormat = GetInputPortImageFormat(InputPortIndex(pInputPort->portId));
                if ((NULL != pInputImageFormat) &&
                    (TRUE == ImageFormatUtils::Is10BitFormat(pInputImageFormat->format)))
                {
                    isInput10bit = TRUE;
                    break;
                }
            }
        }

        for (UINT portIndex = 0; portIndex < pPerRequestPorts->numOutputPorts; portIndex++)
        {
            PerRequestOutputPortInfo* pOutputPort = &pPerRequestPorts->pOutputPorts[portIndex];

            CAMX_ASSERT(NULL != pOutputPort);

            if (NULL != pOutputPort)
            {
                for (UINT bufferIndex = 0; bufferIndex < pOutputPort->numOutputBuffers; bufferIndex++)
                {
                    if (NULL != pOutputPort->ppImageBuffer[bufferIndex])
                    {
                        pOutputImageFormat = pOutputPort->ppImageBuffer[bufferIndex]->GetFormat();

                        CAMX_ASSERT(NULL != pOutputImageFormat);

                        if (NULL != pOutputImageFormat)
                        {
                            // Clamp value depends on output format. For 10-bit Max clamp value
                            // is 0x3FF and for 8-bit it is 0xFF. OEM can choose lower value at the
                            // cost of image quality. Clamp value should be read from chromatix
                            /// @todo (CAMX-2276) Temporary workaround until pImageBuffer for HAL buffers fix is in place
                            if (TRUE == ImageFormatUtils::Is10BitFormat(pOutputImageFormat->format))
                            {
                                clampValue = Max10BitValue;
                            }
                            else
                            {
                                clampValue = Max8BitValue;
                            }
                            if (NULL != pIPEIQsettings)
                            {
                                if (IPEOutputPortDisplay == pOutputPort->portId)
                                {
                                    pIPEIQsettings->displayClampParameters.lumaClamp.clampMin = 0;
                                    pIPEIQsettings->displayClampParameters.lumaClamp.clampMax = clampValue;
                                    pIPEIQsettings->displayClampParameters.chromaClamp.clampMin = 0;
                                    pIPEIQsettings->displayClampParameters.chromaClamp.clampMax = clampValue;
                                    if ((TRUE == isInput10bit) &&
                                        (TRUE == ImageFormatUtils::Is8BitFormat(pOutputImageFormat->format)))
                                    {
                                        pIPEIQsettings->displayRoundingParameters.lumaRoundingPattern =
                                            ROUNDING_PATTERN_REGULAR;
                                        pIPEIQsettings->displayRoundingParameters.chromaRoundingPattern =
                                            ROUNDING_PATTERN_CHECKERBOARD_ODD;
                                    }
                                    else
                                    {
                                        pIPEIQsettings->displayRoundingParameters.lumaRoundingPattern =
                                            ROUNDING_PATTERN_REGULAR;
                                        pIPEIQsettings->displayRoundingParameters.chromaRoundingPattern =
                                            ROUNDING_PATTERN_REGULAR;
                                    }
                                }

                                if (IPEOutputPortVideo == pOutputPort->portId)
                                {
                                    pIPEIQsettings->videoClampParameters.lumaClamp.clampMin = 0;
                                    pIPEIQsettings->videoClampParameters.lumaClamp.clampMax = clampValue;
                                    pIPEIQsettings->videoClampParameters.chromaClamp.clampMin = 0;
                                    pIPEIQsettings->videoClampParameters.chromaClamp.clampMax = clampValue;
                                    if ((TRUE == isInput10bit) &&
                                        (TRUE == ImageFormatUtils::Is8BitFormat(pOutputImageFormat->format)))
                                    {
                                        pIPEIQsettings->videoRoundingParameters.lumaRoundingPattern = ROUNDING_PATTERN_REGULAR;
                                        pIPEIQsettings->videoRoundingParameters.chromaRoundingPattern =
                                            ROUNDING_PATTERN_CHECKERBOARD_ODD;
                                    }
                                    else
                                    {
                                        pIPEIQsettings->videoRoundingParameters.lumaRoundingPattern = ROUNDING_PATTERN_REGULAR;
                                        pIPEIQsettings->videoRoundingParameters.chromaRoundingPattern =
                                            ROUNDING_PATTERN_REGULAR;
                                    }
                                }
                            }
                        }
                        else
                        {
                            CAMX_LOG_ERROR(CamxLogGroupPProc, "pOutputImageFormat is NULL");
                            result = CamxResultEInvalidPointer;
                        }
                    }
                }
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "pPerRequestPorts is NULL");
        result = CamxResultEInvalidPointer;
    }

    // DS parameters
    TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
    CAMX_ASSERT(NULL != pTuningManager);
    if (NULL != pIPEIQsettings)
    {
        ds4to1_1_0_0::chromatix_ds4to1v10Type* pChromatix = NULL;
        if ((NULL != pTuningManager) && (TRUE == pTuningManager->IsValidChromatix()))
        {
            pChromatix = pTuningManager->GetChromatix()->GetModule_ds4to1v10_ipe(
                            reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                            pInputData->pTuningData->noOfSelectionParameter);

            CAMX_ASSERT(NULL != pChromatix);
            if (NULL != pChromatix)
            {
                INT pass_dc64 = static_cast<INT>(ispglobalelements::trigger_pass::PASS_DC64);
                INT pass_dc16 = static_cast<INT>(ispglobalelements::trigger_pass::PASS_DC16);
                INT pass_dc4  = static_cast<INT>(ispglobalelements::trigger_pass::PASS_DC4);
                INT pass_full = static_cast<INT>(ispglobalelements::trigger_pass::PASS_FULL);
                ds4to1_1_0_0::chromatix_ds4to1v10_reserveType*  pReserveData = &pChromatix->chromatix_ds4to1v10_reserve;

                pIPEIQsettings->ds4Parameters.dc64Parameters.coefficient7  =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_dc64].pass_data.coeff_07;
                pIPEIQsettings->ds4Parameters.dc64Parameters.coefficient16 =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_dc64].pass_data.coeff_16;
                pIPEIQsettings->ds4Parameters.dc64Parameters.coefficient25 =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_dc64].pass_data.coeff_25;

                pIPEIQsettings->ds4Parameters.dc16Parameters.coefficient7  =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_dc16].pass_data.coeff_07;
                pIPEIQsettings->ds4Parameters.dc16Parameters.coefficient16 =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_dc16].pass_data.coeff_16;
                pIPEIQsettings->ds4Parameters.dc16Parameters.coefficient25 =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_dc16].pass_data.coeff_25;

                pIPEIQsettings->ds4Parameters.dc4Parameters.coefficient7  =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_dc4].pass_data.coeff_07;
                pIPEIQsettings->ds4Parameters.dc4Parameters.coefficient16 =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_dc4].pass_data.coeff_16;
                pIPEIQsettings->ds4Parameters.dc4Parameters.coefficient25 =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_dc4].pass_data.coeff_25;

                pIPEIQsettings->ds4Parameters.fullpassParameters.coefficient7  =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_full].pass_data.coeff_07;
                pIPEIQsettings->ds4Parameters.fullpassParameters.coefficient16 =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_full].pass_data.coeff_16;
                pIPEIQsettings->ds4Parameters.fullpassParameters.coefficient25 =
                    pReserveData->mod_ds4to1v10_pass_reserve_data[pass_full].pass_data.coeff_25;
            }
        }

        if (NULL == pChromatix)
        {
            pIPEIQsettings->ds4Parameters.dc64Parameters.coefficient7  = 125;
            pIPEIQsettings->ds4Parameters.dc64Parameters.coefficient16 = 91;
            pIPEIQsettings->ds4Parameters.dc64Parameters.coefficient25 = 144;

            pIPEIQsettings->ds4Parameters.dc16Parameters.coefficient7  = 125;
            pIPEIQsettings->ds4Parameters.dc16Parameters.coefficient16 = 91;
            pIPEIQsettings->ds4Parameters.dc16Parameters.coefficient25 = 144;

            pIPEIQsettings->ds4Parameters.dc4Parameters.coefficient7  = 125;
            pIPEIQsettings->ds4Parameters.dc4Parameters.coefficient16 = 91;
            pIPEIQsettings->ds4Parameters.dc4Parameters.coefficient25 = 144;

            pIPEIQsettings->ds4Parameters.fullpassParameters.coefficient7  = 125;
            pIPEIQsettings->ds4Parameters.fullpassParameters.coefficient16 = 91;
            pIPEIQsettings->ds4Parameters.fullpassParameters.coefficient25 = 144;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                        "DC64: Coeff07 = %d, Coeff16 = %d, Coeff25 = %d",
                        pIPEIQsettings->ds4Parameters.dc64Parameters.coefficient7,
                        pIPEIQsettings->ds4Parameters.dc64Parameters.coefficient16,
                        pIPEIQsettings->ds4Parameters.dc64Parameters.coefficient25);

        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                        "DC16: Coeff07 = %d, Coeff16 = %d, Coeff25 = %d",
                        pIPEIQsettings->ds4Parameters.dc16Parameters.coefficient7,
                        pIPEIQsettings->ds4Parameters.dc16Parameters.coefficient16,
                        pIPEIQsettings->ds4Parameters.dc16Parameters.coefficient25);

        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                        "DC4: Coeff07 = %d, Coeff16 = %d, Coeff25 = %d",
                        pIPEIQsettings->ds4Parameters.dc4Parameters.coefficient7,
                        pIPEIQsettings->ds4Parameters.dc4Parameters.coefficient16,
                        pIPEIQsettings->ds4Parameters.dc4Parameters.coefficient25);

        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                        "FULL: Coeff07 = %d, Coeff16 = %d, Coeff25 = %d",
                        pIPEIQsettings->ds4Parameters.fullpassParameters.coefficient7,
                        pIPEIQsettings->ds4Parameters.fullpassParameters.coefficient16,
                        pIPEIQsettings->ds4Parameters.fullpassParameters.coefficient25);


        //  set ICA's invalidPixelModeInterpolationEnabled to be always 1
        //  especially for the DS64 image from IPE (avoid green lines in its bottom)
        pIPEIQsettings->ica1Parameters.invalidPixelModeInterpolationEnabled = 1;
        pIPEIQsettings->ica1Parameters.eightBitOutputAlignment              = 1;
        pIPEIQsettings->ica2Parameters.invalidPixelModeInterpolationEnabled = 1;
        pIPEIQsettings->ica2Parameters.eightBitOutputAlignment              = 1;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE IQ settings is NULL");
        result = CamxResultEFailed;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillFramePerfParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillFramePerfParams(
    IpeFrameProcessData* pFrameProcessData)
{
    CamxResult    result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pFrameProcessData);

    pFrameProcessData->maxNumOfCoresToUse       = (GetStaticSettings()->numIPECoresToUse <= m_capability.numIPE) ?
                     GetStaticSettings()->numIPECoresToUse : m_capability.numIPE;
    pFrameProcessData->targetCompletionTimeInNs = 0;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillFrameUBWCParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillFrameUBWCParams(
    CmdBuffer*                pMainCmdBuffer,
    IpeFrameProcessData*      pFrameProcessData,
    CSLBufferInfo*            pUBWCStatsBuf,
    PerRequestOutputPortInfo* pOutputPort,
    UINT                      outputPortIndex)
{
    CamxResult             result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pMainCmdBuffer);
    CAMX_ASSERT(NULL != pFrameProcessData);
    CAMX_ASSERT(NULL != pOutputPort);

    if (IsRealTime() && IsSinkPortWithBuffer(outputPortIndex) && (NULL != pUBWCStatsBuf) &&
        ((IPEOutputPortDisplay == pOutputPort->portId) || (IPEOutputPortVideo == pOutputPort->portId)))
    {
        UINT32 ubwcStatsBufferOffset =
            static_cast <UINT32>(offsetof(IpeFrameProcessData, ubwcStatsBufferAddress));

        result = pMainCmdBuffer->AddNestedBufferInfo(ubwcStatsBufferOffset,
                                                    pUBWCStatsBuf->hHandle,
                                                    pUBWCStatsBuf->offset);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Error in patching ubwc stats address: %s", Utils::CamxResultToString(result));
            pFrameProcessData->ubwcStatsBufferSize = 0;
        }
        else
        {
            pFrameProcessData->ubwcStatsBufferSize = sizeof(UbwcStatsBuffer);
        }
    }
    else
    {
        pFrameProcessData->ubwcStatsBufferAddress = 0;
        pFrameProcessData->ubwcStatsBufferSize    = 0;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::IsValidDimension
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPENode::IsValidDimension(
    IpeZoomWindow* pZoomWindow)
{
    UINT32 IPEMaxInputWidth       = m_capability.maxInputWidth[ICA_MODE_DISABLED];
    UINT32 IPEMaxInputHeight      = m_capability.maxInputHeight[ICA_MODE_DISABLED];
    UINT32 IPEMinInputWidthLimit  = m_capability.minInputWidth;
    UINT32 IPEMinInputHeightLimit = m_capability.minInputHeight;


    if (pZoomWindow->windowLeft < 0                           ||
        pZoomWindow->windowTop < 0                            ||
        (pZoomWindow->windowHeight <= IPEMinInputHeightLimit  ||
         pZoomWindow->windowWidth <= IPEMinInputWidthLimit)   ||
        (pZoomWindow->windowHeight > IPEMaxInputHeight)       ||
        (pZoomWindow->windowWidth > IPEMaxInputWidth))
    {
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillFrameZoomWindow
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillFrameZoomWindow(
    ISPInputData*     pInputData,
    UINT              parentNodeId,
    CmdBuffer*        pCmdBuffer,
    UINT64            requestId)
{
    CamxResult        result             = CamxResultSuccess;
    IpeZoomWindow*    pZoomWindowICA1    = NULL;
    IpeZoomWindow*    pZoomWindowICA2    = NULL;
    CHIRectangle*     pCropInfo          = NULL;
    IFEScalerOutput*  pIFEScalerOutput   = NULL;
    CHIRectangle      cropInfo;
    int32_t           adjustedWidth      = 0;
    int32_t           adjustedHeight     = 0;
    UINT32            adjustedFullWidth  = 0;
    UINT32            adjustedFullHeight = 0;
    FLOAT             cropFactor         = 1.0f;
    UINT32            fullInputWidth     = m_fullInputWidth;
    UINT32            fullInputHeight    = m_fullInputHeight;
    UINT32            fullOutputWidth    = m_fullOutputWidth;
    UINT32            fullOutputHeight   = m_fullOutputHeight;

    IpeIQSettings*    pIPEIQsettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);
    BOOL              isMFSRNode = FALSE;

    isMFSRNode = ((IPEProfileIdNPS == m_instanceProperty.profileId)      &&
                 (IPEMFSRPrefilter <= m_instanceProperty.processingType) &&
                 (IPEMFSRPostfilter >= m_instanceProperty.processingType));

    CAMX_ASSERT(NULL != pIPEIQsettings);
    if ((IPEProfileIdPPS == m_instanceProperty.profileId)     ||
        (TRUE == isMFSRNode)                                  ||
        (IPEProfileIdDefault == m_instanceProperty.profileId) ||
        (IPEProfileIdHDR10 == m_instanceProperty.profileId))
    {
        cropInfo.left   = 0;
        cropInfo.top    = 0;
        cropInfo.width  = fullInputWidth;
        cropInfo.height = fullInputHeight;

        if (parentNodeId == IFE)
        {
            const UINT props[] =
            {
                PropertyIDFOVCFrameInfo,
                PropertyIDIFEScaleOutput,
            };
            VOID*      pData[CAMX_ARRAY_SIZE(props)]  = { 0 };
            UINT64     offset[CAMX_ARRAY_SIZE(props)] = { 0 };

            GetDataList(props, pData, offset, CAMX_ARRAY_SIZE(props));
            pIFEScalerOutput = reinterpret_cast<IFEScalerOutput*>(pData[1]);

            PortCropInfo portCropInfo = { { 0 } };
            if (CamxResultSuccess == Node::GetPortCrop(this, IPEInputPortFull, &portCropInfo, NULL))
            {
                cropInfo = portCropInfo.residualCrop;
            }
            pCropInfo = &cropInfo;

            CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                             "IFE: crop left:%d top:%d width:%d, height:%d, Scaler ratio = %f, requestId %llu",
                             pCropInfo->left,
                             pCropInfo->top,
                             pCropInfo->width,
                             pCropInfo->height,
                             pIFEScalerOutput->scalingFactor,
                             requestId);

            if ((TRUE == m_instanceProperty.enableFOVC) && (NULL != pData[0]))
            {
                FOVCOutput* pFOVCInfo = reinterpret_cast<FOVCOutput*>(pData[0]);

                UINT64 requestIdOffsetFromLastFlush = GetRequestIdOffsetFromLastFlush(pInputData->frameNum);

                if (pFOVCInfo->fieldOfViewCompensationFactor > 0.0f &&
                    (requestIdOffsetFromLastFlush > GetMaximumPipelineDelay()))
                {
                    m_prevFOVC = pFOVCInfo->fieldOfViewCompensationFactor;
                }
            }
        }
        // Don't need to add parent check, because ipe just use this property when parent node is chinode.
        else if (TRUE == m_instanceProperty.enableCHICropInfoPropertyDependency)
        {
            UINT32        metaTag = 0;

            result = VendorTagManager::QueryVendorTagLocation("com.qti.cropregions",
                                                              "ChiNodeResidualCrop",
                                                              &metaTag);
            if (CamxResultSuccess == result)
            {
                static const UINT tagInfo[] =
                {
                    metaTag,
                    PropertyIDIFEScaleOutput // For RT pipeline
                };

                const static UINT length             = CAMX_ARRAY_SIZE(tagInfo);
                VOID*             pData[length]      = { 0 };
                UINT64            dataOffset[length] = { 0 };
                CropWindow*       pCropWindow        = NULL;

                GetDataList(tagInfo, pData, dataOffset, length);

                pCropWindow      = reinterpret_cast<CropWindow*>(pData[0]);
                pIFEScalerOutput = reinterpret_cast<IFEScalerOutput*>(pData[1]);

                if (NULL != pCropWindow)
                {
                    cropInfo.left   = pCropWindow->left;
                    cropInfo.top    = pCropWindow->top;
                    cropInfo.width  = pCropWindow->width;
                    cropInfo.height = pCropWindow->height;
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to get ChiNodeResidualCrop data, requestId %llu", requestId);
                }

                if (NULL == pIFEScalerOutput)
                {
                    // Query from Input data(Offline Pipeline where ChiNode is parent node)
                    static const UINT PropertiesOfflinePreScale[] =
                    {
                        PropertyIDIFEScaleOutput | InputMetadataSectionMask
                    };

                    const static UINT length             = CAMX_ARRAY_SIZE(PropertiesOfflinePreScale);
                    VOID*             pData[length]      = { 0 };
                    UINT64            dataOffset[length] = { 0 };

                    GetDataList(PropertiesOfflinePreScale, pData, dataOffset, length);
                    pIFEScalerOutput = reinterpret_cast<IFEScalerOutput*>(pData[0]);

                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IFE Scaler output for Offline pipeline %p mask 0x%x",
                                     pIFEScalerOutput,
                                     PropertyIDIFEScaleOutput | InputMetadataSectionMask);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to query ChiNode Crop vendor tags, requestId %llu", requestId);
            }

            pCropInfo = &cropInfo;
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ChiNode pCropInfo %p, IFE Scaler Output %p", pCropInfo, pIFEScalerOutput);
        }
        else if ((parentNodeId == BPS) ||
                 ((TRUE == IsPipelineHasSnapshotJPEGStream() && (FALSE == IsRealTime()))) ||
                 (FALSE == IsRealTime()))
        {
            UINT32 metaTag = 0;

            result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ref.cropsize",
                                                              "RefCropSize",
                                                              &metaTag);

            if (CamxResultSuccess == result)
            {
                metaTag |= InputMetadataSectionMask;
                CropWindow*        pCropWindow     = NULL;
                RefCropWindowSize* pRefCropWindow  = NULL;
                static const UINT  PropertiesIPE[] =
                {
                    InputScalerCropRegion,
                    metaTag
                };
                static const UINT Length                        = CAMX_ARRAY_SIZE(PropertiesIPE);
                VOID*             pData[Length]                 = { 0 };
                UINT64            propertyDataIPEOffset[Length] = { 0 , 0};

                GetDataList(PropertiesIPE, pData, propertyDataIPEOffset, Length);
                pCropWindow    = (static_cast<CropWindow*>(pData[0]));
                pRefCropWindow = (static_cast<RefCropWindowSize*>(pData[1]));
                CAMX_ASSERT ((NULL != pCropWindow) || (NULL != pRefCropWindow));

                if ((0 == pRefCropWindow->width) || (0 == pRefCropWindow->height))
                {
                    pRefCropWindow->width  = fullInputWidth;
                    pRefCropWindow->height = fullInputHeight;
                }
                CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                                 "ZDBG IPE crop Window [%d, %d, %d, %d] full size %dX%d active %dX%d, requestId %llu",
                                 pCropWindow->left,
                                 pCropWindow->top,
                                 pCropWindow->width,
                                 pCropWindow->height,
                                 fullInputWidth,
                                 fullInputHeight,
                                 pRefCropWindow->width,
                                 pRefCropWindow->height,
                                 requestId);

                cropInfo.left   = (pCropWindow->left   * fullInputWidth)  / pRefCropWindow->width;
                cropInfo.top    = (pCropWindow->top    * fullInputHeight) / pRefCropWindow->height;
                cropInfo.width  = (pCropWindow->width  * fullInputWidth)  / pRefCropWindow->width;
                cropInfo.height = (pCropWindow->height * fullInputHeight) / pRefCropWindow->height;

                FLOAT widthRatio  = static_cast<FLOAT>(cropInfo.width)  / static_cast<FLOAT>(fullOutputWidth);
                FLOAT heightRatio = static_cast<FLOAT>(cropInfo.height) / static_cast<FLOAT>(fullOutputHeight);

                if ((widthRatio  > IPEDownscaleThresholdMin && widthRatio  < IPEDownscaleThresholdMax) &&
                    (heightRatio > IPEDownscaleThresholdMin && heightRatio < IPEDownscaleThresholdMax))
                {
                    cropInfo.left   = (cropInfo.width  - fullOutputWidth)  / 2;
                    cropInfo.top    = (cropInfo.height - fullOutputHeight) / 2;
                    cropInfo.width  = fullOutputWidth;
                    cropInfo.height = fullOutputHeight;
                }

                if (((cropInfo.left + cropInfo.width)  > (static_cast<INT32>(fullInputWidth))) ||
                    ((cropInfo.top  + cropInfo.height) > (static_cast<INT32>(fullInputHeight))))
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc,
                                   "ZDBG wrong IPE crop Window [%d, %d, %d, %d] full %dX%d, requestId %llu",
                                   cropInfo.left,
                                   cropInfo.top,
                                   cropInfo.width,
                                   cropInfo.height,
                                   fullInputWidth,
                                   fullInputHeight,
                                   requestId);

                    cropInfo.left   = 0;
                    cropInfo.top    = 0;
                    cropInfo.width  = fullInputWidth;
                    cropInfo.height = fullInputHeight;
                }

                pCropInfo = &cropInfo;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "cannot find vendor tag ref.cropsize, requestId %llu", requestId);
            }
        }
        else
        {
            // Check IFE scale output from the result metadata (RT pipeline)
            static const UINT PropertiesIPE[] =
            {
                PropertyIDIFEScaleOutput,
            };
            static const UINT Length                        = CAMX_ARRAY_SIZE(PropertiesIPE);
            VOID*             pData[Length]                 = { 0 };
            UINT64            propertyDataIPEOffset[Length] = { 0 };

            GetDataList(PropertiesIPE, pData, propertyDataIPEOffset, Length);
            pIFEScalerOutput = reinterpret_cast<IFEScalerOutput*>(pData[0]);

            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "CropInfo from RT pipeline %p mask 0x%x 0x%x",
                             pIFEScalerOutput, PropertyIDIFEScaleOutput);

            if (pIFEScalerOutput == NULL)
            {
                // Query from Input data(Offline Pipeline)
                static const UINT PropertiesOfflineIPE[] =
                {
                    PropertyIDIFEScaleOutput | InputMetadataSectionMask
                };

                GetDataList(PropertiesOfflineIPE, pData, propertyDataIPEOffset, Length);
                pIFEScalerOutput = reinterpret_cast<IFEScalerOutput*>(pData[0]);

                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "CropInfo from Offline pipeline %p mask 0x%x",
                    pIFEScalerOutput,
                    PropertyIDIFEScaleOutput | InputMetadataSectionMask);
            }

            CAMX_ASSERT(NULL != pIFEScalerOutput);
            PortCropInfo portCropInfo = { { 0 } };

            // Check residual crop
            // To find port crop of IFE for SWEIS case, recursion is required.
            if ((0 != (IPEStabilizationType::IPEStabilizationTypeSWEIS2 & m_instanceProperty.stabilizationType))||
                (0 != (IPEStabilizationType::IPEStabilizationTypeSWEIS3 & m_instanceProperty.stabilizationType)))
            {
                UINT32 findbyRecurssion = 1;
                if (CamxResultSuccess == Node::GetPortCrop(this, IPEInputPortFull, &portCropInfo, &findbyRecurssion))
                {
                    cropInfo = portCropInfo.residualCrop;
                }
            }
            else if (CamxResultSuccess == Node::GetPortCrop(this, IPEInputPortFull, &portCropInfo, NULL))
            {
                cropInfo = portCropInfo.residualCrop;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to get residual crop, requestId %llu", requestId);
            }

            pCropInfo = &cropInfo;
        }

        // Fill in ICA1 Zoom Params
        pZoomWindowICA1    = &pIPEIQsettings->ica1Parameters.zoomWindow;
        adjustedFullWidth  = fullInputWidth;
        adjustedFullHeight = fullInputHeight;

        //< Update Crop info if EIS is enabled. firmware expects crop window wrt window after margin crop (ICA1 output)
        UpdateZoomWindowForStabilization(pCropInfo,
                                         &adjustedFullWidth,
                                         &adjustedFullHeight,
                                         requestId);

        if ((TRUE == m_instanceProperty.enableFOVC) && (m_prevFOVC > 0.0f))
        {
            // Apply fixed FOV correction requested by stats
            ApplyStatsFOVCCrop(pCropInfo, parentNodeId, adjustedFullWidth, adjustedFullHeight);
        }

        if (NULL != pCropInfo)
        {
            pZoomWindowICA1->windowTop    = pCropInfo->top;
            pZoomWindowICA1->windowLeft   = pCropInfo->left;
            pZoomWindowICA1->windowWidth  = pCropInfo->width;
            pZoomWindowICA1->windowHeight = pCropInfo->height;
            pZoomWindowICA1->fullWidth    = adjustedFullWidth;
            pZoomWindowICA1->fullHeight   = adjustedFullHeight;
        }
        else if ((0 != m_previousCropInfo.width) && (0 != m_previousCropInfo.height))
        {
            pZoomWindowICA1->windowTop    = m_previousCropInfo.top;
            pZoomWindowICA1->windowLeft   = m_previousCropInfo.left;
            pZoomWindowICA1->windowWidth  = m_previousCropInfo.width;
            pZoomWindowICA1->windowHeight = m_previousCropInfo.height;
            pZoomWindowICA1->fullWidth    = adjustedFullWidth;
            pZoomWindowICA1->fullHeight   = adjustedFullHeight;
        }

        // override ICA1 zoom window in MFSR postfilter stage
        if ((IPEProcessingType::IPEMFSRPostfilter  == m_instanceProperty.processingType) &&
            (IPEProfileId::IPEProfileIdDefault     == m_instanceProperty.profileId))
        {
            pZoomWindowICA1->windowTop    = 0;
            pZoomWindowICA1->windowLeft   = 0;
            pZoomWindowICA1->windowWidth  = 0;
            pZoomWindowICA1->windowHeight = 0;
            pZoomWindowICA1->fullWidth    = 0;
            pZoomWindowICA1->fullHeight   = 0;
        }

        /// @todo (CAMX-2313) Enable ifeZoomWIndow param in IPE IQSettings
        // Fill in ICA2 Zoom Params
        pZoomWindowICA2 = &pIPEIQsettings->ica2Parameters.zoomWindow;

        // ICA2 Zoom Window is needed for reference frames. Hence this is populated from previous crop value which is stored
        if ((TRUE == IsLoopBackPortEnabled()) &&
            (0 != m_previousCropInfo.width) && (0 != m_previousCropInfo.height))
        {
            pZoomWindowICA2->windowTop    = m_previousCropInfo.top;
            pZoomWindowICA2->windowLeft   = m_previousCropInfo.left;
            pZoomWindowICA2->windowWidth  = m_previousCropInfo.width;
            pZoomWindowICA2->windowHeight = m_previousCropInfo.height;
            pZoomWindowICA2->fullWidth    = adjustedFullWidth;
            pZoomWindowICA2->fullHeight   = adjustedFullHeight;
            /// @todo (CAMX-2313) Enable ifeZoomWIndow param in IPE IQSettings
        }
        else
        {
            pZoomWindowICA2->windowTop    = 0;
            pZoomWindowICA2->windowLeft   = 0;
            pZoomWindowICA2->windowWidth  = 0;
            pZoomWindowICA2->windowHeight = 0;
            pZoomWindowICA2->fullWidth    = 0;
            pZoomWindowICA2->fullHeight   = 0;
        }

        // Save crop info for next frame reference crop information for ICA2
        if (NULL != pCropInfo)
        {
            Utils::Memcpy(&m_previousCropInfo, pCropInfo, sizeof(CHIRectangle));
        }
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ZDBG: IPE:%d ICA1 Zoom Window [%d, %d, %d, %d] full %d x %d, requestId %llu",
                         InstanceID(),
                         pIPEIQsettings->ica1Parameters.zoomWindow.windowLeft,
                         pIPEIQsettings->ica1Parameters.zoomWindow.windowTop,
                         pIPEIQsettings->ica1Parameters.zoomWindow.windowWidth,
                         pIPEIQsettings->ica1Parameters.zoomWindow.windowHeight,
                         pIPEIQsettings->ica1Parameters.zoomWindow.fullWidth,
                         pIPEIQsettings->ica1Parameters.zoomWindow.fullHeight,
                         requestId);
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ZDBG: IPE:%d ICA1 IFE Zoom Window [%d, %d, %d, %d] full %d x %d, requestId %llu",
                         InstanceID(),
                         pIPEIQsettings->ica1Parameters.ifeZoomWindow.windowLeft,
                         pIPEIQsettings->ica1Parameters.ifeZoomWindow.windowTop,
                         pIPEIQsettings->ica1Parameters.ifeZoomWindow.windowWidth,
                         pIPEIQsettings->ica1Parameters.ifeZoomWindow.windowHeight,
                         pIPEIQsettings->ica1Parameters.ifeZoomWindow.fullWidth,
                         pIPEIQsettings->ica1Parameters.ifeZoomWindow.fullHeight,
                         requestId);
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ZDBG: IPE:%d ICA2 Zoom Window [%d, %d, %d, %d] full %d x %d, requestId %llu",
                         InstanceID(),
                         pIPEIQsettings->ica2Parameters.zoomWindow.windowLeft,
                         pIPEIQsettings->ica2Parameters.zoomWindow.windowTop,
                         pIPEIQsettings->ica2Parameters.zoomWindow.windowWidth,
                         pIPEIQsettings->ica2Parameters.zoomWindow.windowHeight,
                         pIPEIQsettings->ica2Parameters.zoomWindow.fullWidth,
                         pIPEIQsettings->ica2Parameters.zoomWindow.fullHeight,
                         requestId);
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ZDBG: IPE:%d ICA2 IFE Zoom Window [%d, %d, %d, %d] full %d x %d, requestId %llu",
                         InstanceID(),
                         pIPEIQsettings->ica2Parameters.ifeZoomWindow.windowLeft,
                         pIPEIQsettings->ica2Parameters.ifeZoomWindow.windowTop,
                         pIPEIQsettings->ica2Parameters.ifeZoomWindow.windowWidth,
                         pIPEIQsettings->ica2Parameters.ifeZoomWindow.windowHeight,
                         pIPEIQsettings->ica2Parameters.ifeZoomWindow.fullWidth,
                         pIPEIQsettings->ica2Parameters.ifeZoomWindow.fullHeight,
                         requestId);

        if (FALSE == SetScaleRatios(pInputData, parentNodeId, pCropInfo, pIFEScalerOutput, pCmdBuffer))
        {
            CAMX_LOG_WARN(CamxLogGroupPProc, "Cannot Set Scale Ratios! Use default 1.0f");
        }
    }
    else if (IPEProfileIdScale == m_instanceProperty.profileId)
    {
        if (m_curIntermediateDimension.ratio > 1.0f)
        {
            if ((FALSE == Utils::FEqual(m_curIntermediateDimension.ratio, m_prevIntermediateDimension.ratio)) ||
                (m_curIntermediateDimension.height != m_prevIntermediateDimension.height)                     ||
                (m_curIntermediateDimension.width  != m_prevIntermediateDimension.width))
            {
                UpdateConfigIO(pCmdBuffer, m_curIntermediateDimension.width, m_curIntermediateDimension.height);
                // Ensure the downscale sizes bigger than striping requirements; otherwise, disable passes
                UpdateNumberofPassesonDimension(IPE, m_curIntermediateDimension.width, m_curIntermediateDimension.height);
                CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d, Updated with intermediate width = %d, height = %d, ratio = %f",
                              InstanceID(),
                              m_curIntermediateDimension.width,
                              m_curIntermediateDimension.height,
                              m_curIntermediateDimension.ratio);

                m_prevIntermediateDimension = m_curIntermediateDimension;
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::UpdateZoomWindowForStabilization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::UpdateZoomWindowForStabilization(
    CHIRectangle*   pCropInfo,
    UINT32*         pAdjustedFullWidth,
    UINT32*         pAdjustedFullHeight,
    UINT64          requestId)
{
    //< Update Crop info if EIS is enabled. firmware expects crop window wrt window after margin crop (ICA1 output)
    if ((NULL != pCropInfo) &&
        (NULL != pAdjustedFullWidth) &&
        (NULL != pAdjustedFullHeight) &&
        ((0   != (IPEStabilizationType::IPEStabilizationTypeEIS2 & m_instanceProperty.stabilizationType)) ||
        (0    != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType))))
    {
        UINT32 fullInputWidth  = *pAdjustedFullWidth;
        UINT32 fullInputHeight = *pAdjustedFullHeight;

        GetSizeWithoutStablizationMargin(fullInputWidth, fullInputHeight, pAdjustedFullWidth, pAdjustedFullHeight);

        FLOAT cropFactor            = static_cast<FLOAT>(pCropInfo->height) / fullInputHeight;
        FLOAT cropFactorOffsetLeft  = static_cast<FLOAT>(pCropInfo->left) / fullInputWidth;
        FLOAT cropFactorOffsetTop   = static_cast<FLOAT>(pCropInfo->top) / fullInputHeight;

        UINT32 adjustedAdditionalCropWidth  = *pAdjustedFullWidth  - m_additionalCropOffset.widthPixels;
        UINT32 adjustedAdditionalCropHeight = *pAdjustedFullHeight - m_additionalCropOffset.heightLines;

        pCropInfo->width  = static_cast<INT32>((adjustedAdditionalCropWidth  * cropFactor));
        pCropInfo->height = static_cast<INT32>((adjustedAdditionalCropHeight * cropFactor));
        pCropInfo->left   = static_cast<INT32>((adjustedAdditionalCropWidth  * cropFactorOffsetLeft) +
                                               (m_additionalCropOffset.widthPixels / 2));
        pCropInfo->top    = static_cast<INT32>((adjustedAdditionalCropHeight * cropFactorOffsetTop) +
                                               (m_additionalCropOffset.heightLines / 2));

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ZDBG: After IPE ICA1 Zoom Window [%d, %d, %d, %d] full %d x %d "
                         "crop_factor %f, leftOffsetCropFactor %f, topOffsetCropFactor %f, Stabilization type %d,"
                         " request ID %llu",
                         pCropInfo->left,
                         pCropInfo->top,
                         pCropInfo->width,
                         pCropInfo->height,
                         *pAdjustedFullWidth,
                         *pAdjustedFullHeight,
                         cropFactor,
                         cropFactorOffsetLeft,
                         cropFactorOffsetTop,
                         m_instanceProperty.stabilizationType,
                         requestId);
    }
    else if ((NULL != pCropInfo) &&
             (NULL != pAdjustedFullWidth) &&
             (NULL != pAdjustedFullHeight) &&
             ((0   != (IPEStabilizationType::IPEStabilizationTypeSWEIS2 & m_instanceProperty.stabilizationType)) ||
             (0    != (IPEStabilizationType::IPEStabilizationTypeSWEIS3 & m_instanceProperty.stabilizationType))))
    {
        FLOAT cropFactor            = static_cast<FLOAT>(pCropInfo->height) / *pAdjustedFullHeight;
        FLOAT cropFactorOffsetLeft  = static_cast<FLOAT>(pCropInfo->left) / *pAdjustedFullWidth;
        FLOAT cropFactorOffsetTop   = static_cast<FLOAT>(pCropInfo->top) / *pAdjustedFullHeight;

        UINT32 adjustedAdditionalCropWidth  = *pAdjustedFullWidth  - m_additionalCropOffset.widthPixels;
        UINT32 adjustedAdditionalCropHeight = *pAdjustedFullHeight - m_additionalCropOffset.heightLines;

        pCropInfo->width  = static_cast<INT32>((adjustedAdditionalCropWidth  * cropFactor));
        pCropInfo->height = static_cast<INT32>((adjustedAdditionalCropHeight * cropFactor));

        pCropInfo->width  = Utils::FloorUINT32(16, pCropInfo->width);
        pCropInfo->height = Utils::FloorUINT32(16, pCropInfo->height);

        pCropInfo->left   = static_cast<INT32>((adjustedAdditionalCropWidth  * cropFactorOffsetLeft));
        pCropInfo->top    = static_cast<INT32>((adjustedAdditionalCropHeight * cropFactorOffsetTop));

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ZDBG: Zoom Window [%d, %d, %d, %d] full %d x %d "
                         "crop_factor %f, leftOffsetCropFactor %f, topOffsetCropFactor %f, Stabilization type %d,"
                         " request ID %llu",
                         pCropInfo->left,
                         pCropInfo->top,
                         pCropInfo->width,
                         pCropInfo->height,
                         *pAdjustedFullWidth,
                         *pAdjustedFullHeight,
                         cropFactor,
                         cropFactorOffsetLeft,
                         cropFactorOffsetTop,
                         m_instanceProperty.stabilizationType,
                         requestId);
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillFrameBufferData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillFrameBufferData(
    CmdBuffer*      pMainCmdBuffer,
    ImageBuffer*    pImageBuffer,
    UINT32          payloadBatchFrameIdx,
    UINT32          bufferBatchFrameIdx,
    UINT32          portId)
{
    CamxResult      result       = CamxResultSuccess;
    SIZE_T          planeOffset  = 0;
    SIZE_T          metadataSize = 0;
    CSLMemHandle    hMem;
    UINT32          numPlanes;

    numPlanes = pImageBuffer->GetNumberOfPlanes();

    if (IPEMaxSupportedBatchSize > payloadBatchFrameIdx)
    {
        // Prepare Patching struct for smmu addresses
        for (UINT32 i = 0; i < numPlanes; i++)
        {
            if (1 < pImageBuffer->GetNumFramesInBatch())
            {
                // For super buffer output from IFE, IPE node shall get new frame offset within same buffer.
                pImageBuffer->GetPlaneCSLMemHandle(bufferBatchFrameIdx, i, &hMem, &planeOffset, &metadataSize);
            }
            else
            {
                // For IPE video port,
                //     batch mode is enabled on link but instead of super buffer, these are individual HAL buffers.
                pImageBuffer->GetPlaneCSLMemHandle(0, i, &hMem, &planeOffset, &metadataSize);
            }

            const ImageFormat* pFormat = pImageBuffer->GetFormat();
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //  Following is the memory layout of UBWC formats. Pixel data is located after Meta data.                        //
            //                                                                                                                //
            //   4K aligned  Address -->  ----------------------------                                                        //
            //                            |  Y  - Meta Data Plane    |                                                        //
            //                            ----------------------------                                                        //
            //                            |  Y  - Pixel Data Plane   |                                                        //
            //   4K aligned  Address -->  ----------------------------                                                        //
            //                            |  UV - Meta Data Plane    |                                                        //
            //                            ----------------------------                                                        //
            //                            |  UV - Pixel Data Plane   |                                                        //
            //                            ----------------------------                                                        //
            //                                                                                                                //
            //  So, metadata Size for the plane needs to be added to get the Pixel data address.                              //
            //  Note for Linear formats metadataSize should be 0.                                                             //
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            if (TRUE == ImageFormatUtils::IsUBWC(pFormat->format))
            {
                result = pMainCmdBuffer->AddNestedBufferInfo(
                    s_frameBufferOffset[payloadBatchFrameIdx][portId].metadataBufferPtr[i],
                    hMem,
                    static_cast <UINT32>(planeOffset));

                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Error in patching address portId %d plane %d", portId, i);
                    break;
                }
            }

            result = pMainCmdBuffer->AddNestedBufferInfo(
                s_frameBufferOffset[payloadBatchFrameIdx][portId].bufferPtr[i],
                hMem,
                (static_cast <UINT32>(planeOffset) + static_cast <UINT32>(metadataSize)));

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Error in patching address portId %d plane %d", portId, i);
                break;
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Batch frame index %d is greater than IPEMaxSupportedBatchSize %d",
                       payloadBatchFrameIdx,
                       IPEMaxSupportedBatchSize);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillInputFrameSetData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillInputFrameSetData(
    CmdBuffer*      pFrameProcessCmdBuffer,
    UINT            portId,
    ImageBuffer*    pImageBuffer,
    UINT32          numFramesInBuffer)
{
    CamxResult result = CamxResultSuccess;

    for (UINT32 batchedFrameIndex = 0; batchedFrameIndex < numFramesInBuffer; batchedFrameIndex++)
    {
        result = FillFrameBufferData(pFrameProcessCmdBuffer,
                                     pImageBuffer,
                                     batchedFrameIndex,
                                     batchedFrameIndex,
                                     portId);
        if (CamxResultSuccess != result)
        {
            break;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillOutputFrameSetData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillOutputFrameSetData(
    CmdBuffer*              pFrameProcessCmdBuffer,
    UINT                    portId,
    ImageBuffer*            pImageBuffer,
    UINT32                  batchedFrameIndex)
{
    CamxResult              result              = CamxResultSuccess;

    result = FillFrameBufferData(pFrameProcessCmdBuffer,
                                 pImageBuffer,
                                 batchedFrameIndex,
                                 batchedFrameIndex,
                                 portId);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::InitializeUBWCStatsBuf
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::InitializeUBWCStatsBuf()
{
    CamxResult result        = CamxResultSuccess;
    BOOL       UBWCStatsFlag = FALSE;
    UINT       numOutputPort = 0;
    UINT32     bufferOffset  = 0;
    UINT       outputPortId[IPEMaxOutput];

    // Get Output Port List
    GetAllOutputPortIds(&numOutputPort, &outputPortId[0]);

    for (UINT index = 0; index < numOutputPort; index++)
    {
        if ((IPEOutputPortVideo == outputPortId[index]) ||
            (IPEOutputPortDisplay == outputPortId[index]))
        {
            if ((TRUE == IsSinkPortWithBuffer(OutputPortIndex(outputPortId[index]))) &&
                (TRUE == ImageFormatUtils::IsUBWC(GetOutputPortImageFormat(OutputPortIndex(outputPortId[index]))->format)))
            {
                UBWCStatsFlag = TRUE;
                break;
            }
        }
    }

    if ((TRUE == m_realTimeIPE) && (TRUE == UBWCStatsFlag))
    {
        CSLBufferInfo* pBufferInfo = NULL;

        pBufferInfo = static_cast<CSLBufferInfo*>(CAMX_CALLOC(sizeof(CSLBufferInfo)));
        if (NULL == pBufferInfo)
        {
            result = CamxResultENoMemory;
        }
        else
        {

            result = CSLAlloc(NodeIdentifierString(),
                              pBufferInfo,
                              sizeof(UbwcStatsBuffer) * MaxUBWCStatsBufferSize,
                              1,
                              (CSLMemFlagUMDAccess | CSLMemFlagSharedAccess | CSLMemFlagHw),
                              &DeviceIndices()[0],
                              1);
        }

        if (CamxResultSuccess == result)
        {
            for (UINT count = 0; count < MaxUBWCStatsBufferSize; count++)
            {
                m_UBWCStatBufInfo.pUBWCStatsBuffer = pBufferInfo;
                m_UBWCStatBufInfo.offset[count]    = bufferOffset;
                bufferOffset                      += sizeof(UbwcStatsBuffer);

            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::PostPipelineCreate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::PostPipelineCreate()
{
    CamxResult      result   = CamxResultSuccess;
    UINT32          memFlags = (CSLMemFlagUMDAccess | CSLMemFlagHw);
    ResourceParams  params   = { 0 };

    IQInterface::IQSetHardwareVersion(
        static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion(),
        static_cast<Titan17xContext *>(GetHwContext())->GetHwVersion());
    // Assemble IPE IQ Modules
    result = CreateIPEIQModules();

    m_tuningData.noOfSelectionParameter = 1;
    m_tuningData.TuningMode[0].mode     = ChiModeType::Default;

    m_IPECmdBlobCount = GetPipeline()->GetRequestQueueDepth();

    CAMX_LOG_INFO(CamxLogGroupPProc, "IPE Instance ID %d numbufs %d", InstanceID(), m_IPECmdBlobCount);

    if (CamxResultSuccess == result)
    {
        UpdateIQCmdSize();
        result = InitializeCmdBufferManagerList(IPECmdBufferMaxIds);
    }

    if (CamxResultSuccess == result)
    {
        params.usageFlags.packet               = 1;
        // 1 Command Buffer for all the IQ Modules
        // 2 KMD command buffer
        params.packetParams.maxNumCmdBuffers   = CSLIPECmdBufferMaxIds;

        // 8 Input and 6 Outputs
        params.packetParams.maxNumIOConfigs    = IPEMaxInput + IPEMaxOutput;
        params.packetParams.enableAddrPatching = 1;
        params.packetParams.maxNumPatches      = IPEMaxPatchAddress;
        params.resourceSize                    = Packet::CalculatePacketSize(&params.packetParams);
        params.memFlags                        = CSLMemFlagKMDAccess | CSLMemFlagUMDAccess;
        params.pDeviceIndices                  = &m_deviceIndex;
        params.numDevices                      = 1;

        // Same number as cmd buffers
        params.poolSize                        = m_IPECmdBlobCount * params.resourceSize;
        params.alignment                       = CamxPacketAlignmentInBytes;

        result = CreateCmdBufferManager("IQPacketManager", &params, &m_pIQPacketManager);


        if (CamxResultSuccess == result)
        {
            ResourceParams               resourceParams[IPEMaxFWCmdBufferManagers];
            CHAR                         bufferManagerName[IPEMaxFWCmdBufferManagers][MaxStringLength256];
            struct CmdBufferManagerParam createParam[IPEMaxFWCmdBufferManagers];
            UINT32                       numberOfBufferManagers = 0;

            FillCmdBufferParams(&params,
                                CmdBufferGenericBlobSizeInBytes,
                                CmdType::Generic,
                                CSLMemFlagUMDAccess | CSLMemFlagKMDAccess,
                                0,
                                NULL,
                                m_IPECmdBlobCount);
            result = CreateCmdBufferManager("CmdBufferGenericBlob", &params, &m_pIPECmdBufferManager[CmdBufferGenericBlob]);
            if (CamxResultSuccess == result)
            {
                if (m_maxCmdBufferSizeBytes[CmdBufferPreLTM] > 0)
                {
                    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                                        m_maxCmdBufferSizeBytes[CmdBufferPreLTM],
                                        CmdType::CDMDirect,
                                        CSLMemFlagUMDAccess,
                                        IPEMaxPreLTMPatchAddress,
                                        &m_deviceIndex,
                                        m_IPECmdBlobCount);
                    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                                      sizeof(CHAR) * MaxStringLength256,
                                      "CBM_%s_%s",
                                      NodeIdentifierString(),
                                      "CmdBufferPreLTM");
                    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pIPECmdBufferManager[CmdBufferPreLTM];
                    numberOfBufferManagers++;
                }

                if (m_maxCmdBufferSizeBytes[CmdBufferPostLTM] > 0)
                {
                    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                                        m_maxCmdBufferSizeBytes[CmdBufferPostLTM],
                                        CmdType::CDMDirect,
                                        CSLMemFlagUMDAccess,
                                        IPEMaxPostLTMPatchAddress,
                                        &m_deviceIndex,
                                        m_IPECmdBlobCount);

                    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                                      sizeof(CHAR) * MaxStringLength256,
                                      "CBM_%s_%s",
                                      NodeIdentifierString(),
                                      "CmdBufferPostLTM");
                    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pIPECmdBufferManager[CmdBufferPostLTM];
                    numberOfBufferManagers++;
                }

                if (m_maxCmdBufferSizeBytes[CmdBufferDMIHeader] > 0)
                {
                    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                                        m_maxCmdBufferSizeBytes[CmdBufferDMIHeader],
                                        CmdType::CDMDMI,
                                        CSLMemFlagUMDAccess,
                                        IPEMaxDMIPatchAddress,
                                        &m_deviceIndex,
                                        m_IPECmdBlobCount);

                    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                                      sizeof(CHAR) * MaxStringLength256,
                                      "CBM_%s_%s",
                                      NodeIdentifierString(),
                                      "CmdBufferDMIHeader");
                    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
                    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pIPECmdBufferManager[CmdBufferDMIHeader];
                    numberOfBufferManagers++;
                }

                if (0 != numberOfBufferManagers)
                {
                    result = CreateMultiCmdBufferManager(createParam, numberOfBufferManagers);
                }
            }

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Cmd Buffer Creation failed result %d", result);
            }
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("%s: Failed to Creat Cmd Buffer Manager", __FUNCTION__);
        }
    }

    if (CamxResultSuccess != result)
    {
        Cleanup();
    }

    if (CamxResultSuccess == result)
    {
        // Save required static metadata
        GetStaticMetadata();
    }

    if (CamxResultSuccess == result)
    {
        /// @todo (CAMX-738) Find input port dimensions/format from metadata / use case pool and do acquire.
        result = AcquireDevice();
    }

    if (CamxResultSuccess == result)
    {
        /// @todo (CAMX-732) Get Scratch buffer from topology from loopback port
        m_numScratchBuffers = MaxScratchBuffer;
        for (UINT count = 0; count < m_numScratchBuffers; count++)
        {
            /// @todo (CAMX-886) Add CSLMemFlagSharedAccess once available from memory team
            m_pScratchMemoryBuffer[count] = static_cast<CSLBufferInfo*>(CAMX_CALLOC(sizeof(CSLBufferInfo)));

            if (NULL != m_pScratchMemoryBuffer[count])
            {
                if (TRUE == IsSecureMode())
                {
                    memFlags = (CSLMemFlagProtected | CSLMemFlagHw);
                }

                result = CSLAlloc(NodeIdentifierString(),
                                  m_pScratchMemoryBuffer[count],
                                  m_firmwareScratchMemSize,
                                  CamxCommandBufferAlignmentInBytes,
                                  memFlags,
                                  &DeviceIndices()[0],
                                  1);
                if (CamxResultSuccess == result)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                                     "CSLAlloc returned m_pScratchMemoryBuffer[%d].fd=%d",
                                     count,
                                     m_pScratchMemoryBuffer[count]->fd);
                }
                else
                {
                    CAMX_ASSERT_ALWAYS_MESSAGE("%s: Failed to Alloc scratch", __FUNCTION__);
                    break;
                }
                CAMX_ASSERT(CSLInvalidHandle != m_pScratchMemoryBuffer[count]->hHandle);
                CAMX_ASSERT(NULL != m_pScratchMemoryBuffer[count]->pVirtualAddr);
            }
            else
            {
                result = CamxResultENoMemory;
            }
        }

        // Allocate buffer for UBWC stats
        if (CamxResultSuccess == result)
        {
            result = InitializeUBWCStatsBuf();
        }

        if (CamxResultSuccess == result)
        {
            UINT32         numberOfMappings = 0;
            CSLBufferInfo  bufferInfo = { 0 };
            CSLBufferInfo* pBufferInfo[CSLICPMaxMemoryMapRegions];

            if (NULL != m_pIPECmdBufferManager[CmdBufferFrameProcess])
            {
                if (NULL != m_pIPECmdBufferManager[CmdBufferFrameProcess]->GetMergedCSLBufferInfo())
                {
                    Utils::Memcpy(&bufferInfo,
                                  m_pIPECmdBufferManager[CmdBufferFrameProcess]->GetMergedCSLBufferInfo(),
                                  sizeof(CSLBufferInfo));
                    pBufferInfo[numberOfMappings] = &bufferInfo;
                    numberOfMappings++;

                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Unable to get Merged CSL Buffer Info");
                    result = CamxResultEFailed;
                }
            }

            if (NULL != m_UBWCStatBufInfo.pUBWCStatsBuffer)
            {
                pBufferInfo[numberOfMappings] = m_UBWCStatBufInfo.pUBWCStatsBuffer;
                numberOfMappings++;
            }

            if (0 != numberOfMappings)
            {
                result = SendFWCmdRegionInfo(CSLICPGenericBlobCmdBufferMapFWMemRegion,
                                             pBufferInfo,
                                             numberOfMappings);
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::CheckDimensionRequirementsForIPEDownscaler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPENode::CheckDimensionRequirementsForIPEDownscaler(
    UINT32 width,
    UINT32 height,
    UINT   downScalarMode)
{
    BOOL    result          = TRUE;
    UINT32  referenceWidth  = 0;
    UINT32  referenceHeight = 0;
    FLOAT   aspectRatio     = static_cast<FLOAT>(width) / height;

    if (aspectRatio <= 1.0f)
    {
        referenceWidth  = 1440;
        referenceHeight = 1440;
    }
    else if ((aspectRatio > 1.0f) && (aspectRatio <= 4.0f/3.0f))
    {
        referenceWidth  = 1920;
        referenceHeight = 1440;
    }
    else if ((aspectRatio > 4.0f/3.0f) && (aspectRatio <= 16.0f/9.0f))
    {
        referenceWidth  = 1920;
        referenceHeight = 1080;
    }
    else
    {   // (aspectRatio > 16.0f/9.0f)
        referenceWidth  = 1920;
        referenceHeight = 1440;
    }

    if (IPEMidDownScalarMode == downScalarMode)
    {
        referenceWidth  = IPEMidDownScalarWidth;
        referenceHeight = IPEMidDownScalarHeight;
    }
    else if (IPECustomDownScalarMode == downScalarMode)
    {
        referenceWidth  = m_instanceProperty.ipeDownscalerWidth;
        referenceHeight = m_instanceProperty.ipeDownscalerHeight;
    }

    if ((width >= referenceWidth) && (height >= referenceHeight))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Streams dim %dx%d bigger than ref dims %dx%d",
            width, height, referenceWidth, referenceHeight);
        result = FALSE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::IsStandardAspectRatio
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPENode::IsStandardAspectRatio(
    FLOAT aspectRatio)
{
    BOOL    result          = TRUE;

    // Use a reduced precision for comparing aspect ratios as updating dimensions should not be very sensitive to small
    // differences in aspect ratios
    if (Utils::FEqualCoarse(aspectRatio, 1.0f)     ||
        Utils::FEqualCoarse(aspectRatio, 4.0f/3.0f)   ||
        Utils::FEqualCoarse(aspectRatio, 16.0f/9.0f)  ||
        Utils::FEqualCoarse(aspectRatio, 18.5f/9.0f) ||
        Utils::FEqualCoarse(aspectRatio, 19.9f / 9.0f) ||
        Utils::FEqualCoarse(aspectRatio, 18.0f/9.0f))
    {
        result = TRUE;
    }
    else
    {
        result = FALSE;
    }

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetIPEDownscalerOnlyDimensions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::GetIPEDownscalerOnlyDimensions(
    UINT32  width,
    UINT32  height,
    UINT32* pMaxWidth,
    UINT32* pMaxHeight,
    FLOAT   downscaleLimit,
    UINT    downScalarMode)
{
    FLOAT heightRatio   = 0.0f;
    FLOAT widthRatio    = 0.0f;
    FLOAT aspectRatio   = static_cast<FLOAT>(width) / height;

    if (aspectRatio <= 1.0f)
    {
        *pMaxWidth  = 1440;
        *pMaxHeight = 1440;
    }
    else if ((aspectRatio > 1.0f) && (aspectRatio <= 4.0f/3.0f))
    {
        *pMaxWidth  = 1920;
        *pMaxHeight = 1440;
    }
    else if ((aspectRatio > 4.0f/3.0f) && (aspectRatio <= 16.0f/9.0f))
    {
        *pMaxWidth  = 1920;
        *pMaxHeight = 1080;
    }
    else if ((aspectRatio > (18.5 / 9 + 0.1)) && (aspectRatio <= (19.9 / 9 + 0.1)))
    {
        *pMaxWidth  = 2288;
        *pMaxHeight = 1080;
    }
    else
    {   // (aspectRatio > 16.0f/9.0f)
        *pMaxWidth  = 1920;
        *pMaxHeight = 1440;
    }

    if (IPEMidDownScalarMode == downScalarMode)
    {
        *pMaxWidth  = IPEMidDownScalarWidth;
        *pMaxHeight = IPEMidDownScalarHeight;
    }
    else if (IPECustomDownScalarMode == downScalarMode)
    {
        *pMaxWidth  = m_instanceProperty.ipeDownscalerWidth;
        *pMaxHeight = m_instanceProperty.ipeDownscalerHeight;
    }

    widthRatio  = *reinterpret_cast<FLOAT*>(pMaxWidth) / width;
    heightRatio = *reinterpret_cast<FLOAT*>(pMaxHeight) / height;

    if ((widthRatio > downscaleLimit) ||
        (heightRatio > downscaleLimit))
    {
        *pMaxWidth  = width * static_cast<UINT32>(downscaleLimit);
        *pMaxHeight = height * static_cast<UINT32>(downscaleLimit);
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE Downscaler resolution selected: %d X %d", *pMaxWidth, *pMaxHeight);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::IsIPEOnlyDownscalerEnabled
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPENode::IsIPEOnlyDownscalerEnabled(
    BufferNegotiationData* pBufferNegotiationData)
{
    BOOL    isDimensionRequirementValid = FALSE;

    if ((0 != m_instanceProperty.ipeOnlyDownscalerMode) ||
        (TRUE == GetStaticSettings()->enableIPEOnlyDownscale))
    {
        for (UINT index = 0; index < pBufferNegotiationData->numOutputPortsNotified; index++)
        {
            OutputPortNegotiationData* pOutputPortNegotiationData = &pBufferNegotiationData->pOutputPortNegotiationData[index];
            for (UINT inputIndex = 0; inputIndex < pOutputPortNegotiationData->numInputPortsNotification; inputIndex++)
            {
                BufferRequirement* pInputPortRequirement = &pOutputPortNegotiationData->inputPortRequirement[inputIndex];
                isDimensionRequirementValid =
                    CheckDimensionRequirementsForIPEDownscaler(pInputPortRequirement->optimalWidth,
                                                               pInputPortRequirement->optimalHeight,
                                                               m_instanceProperty.ipeOnlyDownscalerMode);
            }
        }
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE Downscaler only not enabled");
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE Downscaler enabled: %d", isDimensionRequirementValid);

    return isDimensionRequirementValid;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::ProcessingNodeFinalizeInputRequirement
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::ProcessingNodeFinalizeInputRequirement(
    BufferNegotiationData* pBufferNegotiationData)
{
    CAMX_ASSERT(NULL != pBufferNegotiationData);

    CamxResult          result                         = CamxResultSuccess;
    UINT32              optimalInputWidth              = 0;
    UINT32              optimalInputHeight             = 0;
    UINT32              maxInputWidth                  = 0xffff;
    UINT32              maxInputHeight                 = 0xffff;
    UINT32              minInputWidth                  = 0;
    UINT32              minInputHeight                 = 0;
    UINT32              perOutputPortOptimalWidth      = 0;
    UINT32              perOutputPortOptimalHeight     = 0;
    UINT32              perOutputPortMinWidth          = 0;
    UINT32              perOutputPortMinHeight         = 0;
    UINT32              perOutputPortMaxWidth          = 0xffff;
    UINT32              perOutputPortMaxHeight         = 0xffff;
    FLOAT               perOutputPortAspectRatio       = 0.0f;
    FLOAT               inputAspectRatio               = 0.0f;
    FLOAT               optimalAspectRatio             = 0.0f;
    FLOAT               selectedAspectRatio            = 0.0f;
    FLOAT               prevOutputPortAspectRatio      = 0.0f;
    const ImageFormat*  pFormat                        = NULL;
    FLOAT               upscaleLimit                   = 1.0;
    FLOAT               downscaleLimit                 = 1.0;
    UINT32              IPEMaxInputWidth               = 0;
    UINT32              IPEMaxInputHeight              = 0;
    UINT32              IPEMinInputWidthLimit          = 0;
    UINT32              IPEMinInputHeightLimit         = 0;
    BOOL                isIPEDownscalerEnabled         = FALSE;
    const FLOAT         FFOV_PER                       = 0.06f;
    AlignmentInfo       alignmentLCM[FormatsMaxPlanes] = { {0} };
    CHIDimension        stabilizedOutputDimensions     = { 0 };
    UINT32              sensorOutputWidth              = 0;
    UINT32              sensorOutputHeight             = 0;
    FLOAT               sensorAspectRatio              = 0.0f;

    CAMX_ASSERT(NULL != pBufferNegotiationData);

    pFormat = static_cast<const ImageFormat *>
        (&pBufferNegotiationData->pOutputPortNegotiationData->pFinalOutputBufferProperties->imageFormat);

    upscaleLimit   = m_capability.maxUpscale[ImageFormatUtils::IsUBWC(pFormat->format)];
    downscaleLimit = m_capability.maxDownscale[ImageFormatUtils::IsUBWC(pFormat->format)];

    // Update IPE IO capability info based on ports enabled
    result = UpdateIPEIOLimits(pBufferNegotiationData);
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d Unable to update the capability", InstanceID());
    }

    /// @todo (CAMX-2013) Read ICA if enabled and take respective IO limits
    IPEMaxInputWidth       = m_capability.maxInputWidth[ICA_MODE_DISABLED];
    IPEMaxInputHeight      = m_capability.maxInputHeight[ICA_MODE_DISABLED];
    IPEMinInputWidthLimit  = m_capability.minInputWidth;
    IPEMinInputHeightLimit = m_capability.minInputHeight;

    // Get Sensor Aspect Ratio
    const UINT sensorInfoTag[] =
    {
        StaticSensorInfoActiveArraySize,
    };

    const UINT length         = CAMX_ARRAY_SIZE(sensorInfoTag);
    VOID*      pData[length]  = { 0 };
    UINT64     offset[length] = { 0 };

    result = GetDataList(sensorInfoTag, pData, offset, length);

    if (CamxResultSuccess == result)
    {
        if (NULL != pData[0])
        {
            Region region      = *static_cast<Region*>(pData[0]);
            sensorOutputWidth  = region.width;
            sensorOutputHeight = region.height;
            if (sensorOutputHeight != 0)
            {
                sensorAspectRatio =
                    static_cast<FLOAT>(sensorOutputWidth) / static_cast<FLOAT>(sensorOutputHeight);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid Sensor Dimensions");
            }
        }
    }

    // The IPE node will have to loop through all the output ports which are connected to a child node or a HAL target.
    // The input buffer requirement will be the super resolution after looping through all the output ports.
    // The super resolution may have different aspect ratio compared to the original output port aspect ratio, but
    // this will be taken care of by the crop hardware associated with the output port.
    UINT isUBWCFormat = 0;
    for (UINT index = 0; index < pBufferNegotiationData->numOutputPortsNotified; index++)
    {
        OutputPortNegotiationData* pOutputPortNegotiationData = &pBufferNegotiationData->pOutputPortNegotiationData[index];
        UINT                       outputPortId = GetOutputPortId(pOutputPortNegotiationData->outputPortIndex);

        if (((IPEProcessingType::IPEProcessingTypeDefault != m_instanceProperty.processingType)  &&
            (IPEProcessingType::IPEProcessingPreview != m_instanceProperty.processingType))      ||
            (TRUE == IsScalerOnlyIPE()) ||
            (FALSE == IsReferenceOutputPort(outputPortId)))
        {
            perOutputPortOptimalWidth = 0;
            perOutputPortOptimalHeight = 0;
            perOutputPortMinWidth = 0;
            perOutputPortMinHeight = 0;
            perOutputPortMaxWidth = 0xffff;
            perOutputPortMaxHeight = 0xffff;
            perOutputPortAspectRatio = 0.0f;

            pFormat        = static_cast<const ImageFormat *>
                (&pBufferNegotiationData->pOutputPortNegotiationData[index].pFinalOutputBufferProperties->imageFormat);
            isUBWCFormat   = ImageFormatUtils::IsUBWC(pFormat->format);

            upscaleLimit   = m_capability.maxUpscale[isUBWCFormat];
            downscaleLimit = m_capability.maxDownscale[isUBWCFormat];

            Utils::Memset(&pOutputPortNegotiationData->outputBufferRequirementOptions, 0, sizeof(BufferRequirement));

            // Go through the requirements of the input ports connected to the output port
            for (UINT inputIndex = 0; inputIndex < pOutputPortNegotiationData->numInputPortsNotification; inputIndex++)
            {
                BufferRequirement* pInputPortRequirement = &pOutputPortNegotiationData->inputPortRequirement[inputIndex];
                /// @todo (CAMX-2013) take into account aspect ratio and format as well during negotiation.
                // Take the max of the min dimensions, min of the max dimensions and the
                // max of the optimal dimensions
                perOutputPortOptimalWidth = Utils::MaxUINT32(perOutputPortOptimalWidth, pInputPortRequirement->optimalWidth);
                perOutputPortOptimalHeight = Utils::MaxUINT32(perOutputPortOptimalHeight, pInputPortRequirement->optimalHeight);

                perOutputPortMinWidth = Utils::MaxUINT32(perOutputPortMinWidth, pInputPortRequirement->minWidth);
                perOutputPortMinHeight = Utils::MaxUINT32(perOutputPortMinHeight, pInputPortRequirement->minHeight);

                perOutputPortMaxWidth = Utils::MinUINT32(perOutputPortMaxWidth, pInputPortRequirement->maxWidth);
                perOutputPortMaxHeight = Utils::MinUINT32(perOutputPortMaxHeight, pInputPortRequirement->maxHeight);

                inputAspectRatio = static_cast<FLOAT>(pInputPortRequirement->optimalWidth) /
                                       pInputPortRequirement->optimalHeight;
                perOutputPortAspectRatio = Utils::MaxFLOAT(perOutputPortAspectRatio, inputAspectRatio);

                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE:%d Idx:%d In=%dx%d Opt:%dx%d inAR:%f peroutAR:%f",
                    InstanceID(), inputIndex, pInputPortRequirement->optimalWidth, pInputPortRequirement->optimalHeight,
                    perOutputPortOptimalWidth, perOutputPortOptimalHeight, inputAspectRatio,
                    perOutputPortAspectRatio);

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

            // Store the buffer requirements for this output port which will be reused to set, during forward walk.
            // The values stored here could be final output dimensions unless it is overridden by forward walk.

            // Optimal dimension should lie between the min and max, ensure the same.
            // There is a chance of the Optimal dimension going over the max dimension.
            // Correct the same.
            perOutputPortOptimalWidth =
                Utils::ClampUINT32(perOutputPortOptimalWidth, perOutputPortMinWidth, perOutputPortMaxWidth);
            perOutputPortOptimalHeight =
                Utils::ClampUINT32(perOutputPortOptimalHeight, perOutputPortMinHeight, perOutputPortMaxHeight);

            // This current output port requires resolution that IPE cannot handle for UBWC format
            if ((0 != isUBWCFormat) &&
                ((perOutputPortOptimalWidth < m_capability.minOutputWidthUBWC) ||
                (perOutputPortOptimalHeight < m_capability.minOutputHeightUBWC)))
            {
                CAMX_LOG_WARN(CamxLogGroupPProc,
                              "IPE:%d unabled to handle resolution %dx%d with current format %d for output port %d ",
                              InstanceID(),
                              perOutputPortOptimalWidth,
                              perOutputPortOptimalHeight,
                              pFormat->format,
                              GetOutputPortId(pOutputPortNegotiationData->outputPortIndex));
                result = CamxResultEFailed;
                break; // break out of loop as IPE fails to work with current resolution and format
            }

            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth = perOutputPortOptimalWidth;
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight = perOutputPortOptimalHeight;
            pOutputPortNegotiationData->outputBufferRequirementOptions.minWidth = perOutputPortMinWidth;
            pOutputPortNegotiationData->outputBufferRequirementOptions.minHeight = perOutputPortMinHeight;
            pOutputPortNegotiationData->outputBufferRequirementOptions.maxWidth = perOutputPortMaxWidth;
            pOutputPortNegotiationData->outputBufferRequirementOptions.maxHeight = perOutputPortMaxHeight;
            Utils::Memcpy(&pOutputPortNegotiationData->outputBufferRequirementOptions.planeAlignment[0],
                          &alignmentLCM[0],
                          sizeof(AlignmentInfo) * FormatsMaxPlanes);

            Utils::Memset(&alignmentLCM[0], 0, sizeof(AlignmentInfo) * FormatsMaxPlanes);

            if (IPEStabilizationTypeEIS2 == (m_instanceProperty.stabilizationType & IPEStabilizationTypeEIS2)    ||
                (IPEStabilizationTypeEIS3 == (m_instanceProperty.stabilizationType & IPEStabilizationTypeEIS3)))
            {
                stabilizedOutputDimensions.width  = Utils::MaxUINT32(stabilizedOutputDimensions.width,
                                                                     perOutputPortOptimalWidth);
                stabilizedOutputDimensions.height = Utils::MaxUINT32(stabilizedOutputDimensions.height,
                                                                     perOutputPortOptimalHeight);
            }

            optimalInputWidth = Utils::MaxUINT32(optimalInputWidth, perOutputPortOptimalWidth);
            optimalInputHeight = Utils::MaxUINT32(optimalInputHeight, perOutputPortOptimalHeight);

            FLOAT minInputLimitAspectRatio = static_cast<FLOAT>(IPEMinInputWidthLimit) /
                                                static_cast<FLOAT>(IPEMinInputHeightLimit);

            if (minInputLimitAspectRatio > perOutputPortAspectRatio)
            {
                IPEMinInputHeightLimit =
                    Utils::EvenFloorUINT32(static_cast<UINT32>(IPEMinInputWidthLimit / perOutputPortAspectRatio));
                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Updated  IPEMinInputHeightLimit %d ",
                                   IPEMinInputHeightLimit);
            }
            else
            {
                IPEMinInputWidthLimit =
                    Utils::EvenFloorUINT32(static_cast<UINT32>(IPEMinInputHeightLimit * perOutputPortAspectRatio));
                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Updated  IPEMinInputWidthLimit %d ",
                                   IPEMinInputWidthLimit);
            }

            optimalInputWidth  = Utils::MaxUINT32(IPEMinInputWidthLimit, optimalInputWidth);
            optimalInputHeight = Utils::MaxUINT32(IPEMinInputHeightLimit, optimalInputHeight);

            optimalAspectRatio = static_cast<FLOAT>(optimalInputWidth) / optimalInputHeight;

            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE:%d OptimalIn:%dx%d OptAR:%f perOutAR:%f",
                InstanceID(), optimalInputWidth, optimalInputHeight, optimalAspectRatio, perOutputPortAspectRatio);

            if ((prevOutputPortAspectRatio != 0) &&
                (FALSE == Utils::FEqualCoarse(perOutputPortAspectRatio, prevOutputPortAspectRatio)) &&
                (sensorAspectRatio != 0))
            {
                if (optimalAspectRatio > sensorAspectRatio)
                {
                    optimalInputHeight =
                        Utils::EvenFloorUINT32(static_cast<UINT32>(optimalInputWidth / sensorAspectRatio));
                    CAMX_LOG_INFO(CamxLogGroupPProc, "Change AspectRatio %f to Sensor AR:Change Height %d using AR:%f",
                                                       optimalAspectRatio, optimalInputHeight, sensorAspectRatio);
                }
                else
                {
                    optimalInputWidth =
                        Utils::EvenFloorUINT32(static_cast<UINT32>(optimalInputHeight * sensorAspectRatio));
                    CAMX_LOG_INFO(CamxLogGroupPProc, "Change AspectRatio %f to Sensor AR:Change Width %d using AR:%f",
                                                       optimalAspectRatio, optimalInputWidth, sensorAspectRatio);
                }
                selectedAspectRatio = sensorAspectRatio;
            }
            else
            {

                // Based on the various negotiations above it is possible that the optimal dimensions as input
                // to IPE could end up with an arbitrary aspect ratio. Hence make sure that the dimensions conform
                // to the maximum of the aspect ratio from the output dimensions. Assumption here is that the
                // output dimensions requested from IPE are proper. The dimensions are only adapted for the IPE input.
                if ((TRUE == IsStandardAspectRatio(optimalAspectRatio))                         ||
                    (TRUE == Utils::FEqualCoarse(optimalAspectRatio, perOutputPortAspectRatio)) ||
                    ((TRUE == IsMFProcessingType()) && (FALSE == IsPostfilterWithDefault())))
                {
                    // The dimensions are fine. Do nothing
                }
                else if (optimalAspectRatio > perOutputPortAspectRatio)
                {
                    if (perOutputPortAspectRatio != 0)
                    {
                        optimalInputHeight =
                            Utils::EvenFloorUINT32(static_cast<UINT32>(optimalInputWidth / perOutputPortAspectRatio));
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "NonConformant AspectRatio:%f Change Height %d using AR:%f",
                            optimalAspectRatio, optimalInputHeight, perOutputPortAspectRatio);
                    }
                }
                else
                {
                    optimalInputWidth =
                        Utils::EvenFloorUINT32(static_cast<UINT32>(optimalInputHeight * perOutputPortAspectRatio));
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "NonConformant AspectRatio:%f Change Width %d using AR:%f",
                        optimalAspectRatio, optimalInputWidth, perOutputPortAspectRatio);
                }
                selectedAspectRatio = perOutputPortAspectRatio;
            }
            prevOutputPortAspectRatio = perOutputPortAspectRatio;

            // Minimum IPE input dimension should be big enough to give the
            // max output required for a connected to one of IPE destination ports,
            // considering the upscale limitations.
            if ((FALSE == GetStaticSettings()->enableIPEUpscale) &&
                ((GetOutputPortId(pOutputPortNegotiationData->outputPortIndex) == IPEOutputPortVideo) ||
                (GetOutputPortId(pOutputPortNegotiationData->outputPortIndex) == IPEOutputPortDisplay)))
            {
                minInputHeight = Utils::MaxUINT32(minInputHeight,
                                                  static_cast<UINT32>(optimalInputHeight / 1.0f));
                minInputWidth = Utils::MaxUINT32(minInputWidth,
                                                 static_cast<UINT32>(optimalInputWidth / 1.0f));
            }
            else
            {
                minInputHeight = Utils::MaxUINT32(minInputHeight,
                                                  static_cast<UINT32>(optimalInputHeight / upscaleLimit));
                minInputWidth = Utils::MaxUINT32(minInputWidth,
                                                 static_cast<UINT32>(optimalInputWidth / upscaleLimit));
            }
            // Set the value at the minInputLimit of IPE if the current value is smaller than required.
            minInputWidth = Utils::MaxUINT32(IPEMinInputWidthLimit, minInputWidth);
            minInputHeight = Utils::MaxUINT32(IPEMinInputHeightLimit, minInputHeight);

            // Maximum input dimension should be small enough to give the
            // min output required for a connected IPE destination port,
            // considering the downscale limitations.
            if ((IPEProcessingType::IPEMFSRPrefilter <= m_instanceProperty.processingType) &&
                (IPEProcessingType::IPEMFSRPostfilter >= m_instanceProperty.processingType))
            {
                // do no use the downscaleLimit
            }
            else
            {
                maxInputHeight = Utils::MinUINT32(maxInputHeight,
                                                  static_cast<UINT32>(perOutputPortMinHeight * downscaleLimit));
                maxInputWidth = Utils::MinUINT32(maxInputWidth,
                                                 static_cast<UINT32>(perOutputPortMinWidth * downscaleLimit));
            }

            // Cap the value at the IPE limitations if the current value is bigger than required.
            maxInputWidth = Utils::MinUINT32(IPEMaxInputWidth, maxInputWidth);
            maxInputHeight = Utils::MinUINT32(IPEMaxInputHeight, maxInputHeight);
        }
        else if (TRUE == IsReferenceOutputPort(outputPortId))
        {
            // For certain pipelines IPE is operated with only Reference ouput ports and there won't
            // be any video or display ports configured then optimal res will be zero.
            // So, cap them to supported resolutions and avoid buffer negotiation failure.
            optimalInputWidth  = Utils::MaxUINT32(IPEMinInputWidthLimit, optimalInputWidth);
            optimalInputHeight = Utils::MaxUINT32(IPEMinInputHeightLimit, optimalInputHeight);

            minInputWidth      = Utils::MaxUINT32(IPEMinInputWidthLimit, minInputWidth);
            minInputHeight     = Utils::MaxUINT32(IPEMinInputHeightLimit, minInputHeight);

            maxInputWidth      = Utils::MinUINT32(IPEMaxInputWidth, maxInputWidth);
            maxInputHeight     = Utils::MinUINT32(IPEMaxInputHeight, maxInputHeight);
        }
    }

    if (CamxResultSuccess == result)
    {
        UINT metaTag = 0;
        if (0 != (m_instanceProperty.stabilizationType & IPEStabilizationTypeEIS2))
        {
            result = VendorTagManager::QueryVendorTagLocation(VendorTagSectionEISRealTimeConfig,
                                                              "StabilizedOutputDims",
                                                              &metaTag);
        }
        else if (0 != (m_instanceProperty.stabilizationType & IPEStabilizationTypeEIS3))
        {
            result = VendorTagManager::QueryVendorTagLocation(VendorTagSectionEISLookAheadConfig,
                                                              "StabilizedOutputDims",
                                                              &metaTag);
        }

        if ((CamxResultSuccess == result) && (0 != metaTag))
        {
            const UINT  stabilizationOutDimsTag[]   = { metaTag | UsecaseMetadataSectionMask };
            const UINT  length                      = CAMX_ARRAY_SIZE(stabilizationOutDimsTag);
            const VOID* pDimensionData[length]      = { &stabilizedOutputDimensions };
            UINT        pDimensionDataCount[length] = { sizeof(CHIDimension) };

            result = WriteDataList(stabilizationOutDimsTag, pDimensionData, pDimensionDataCount, length);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc,
                               "IPE:%d failed to write stabilization output dimensions type %d to vendor data list error = %d",
                               m_instanceProperty.stabilizationType, InstanceID(), result);
            }
        }

        result = GetEISMargin();
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d Unable to determine EIS margins", InstanceID());
        }

        // Account for additional margin need in EIS usecases
        if (0 != (m_instanceProperty.stabilizationType & IPEStabilizationTypeEIS2))
        {
            optimalInputWidth  += static_cast<UINT32>(optimalInputWidth * m_EISMarginRequest.widthMargin);
            optimalInputHeight += static_cast<UINT32>(optimalInputHeight * m_EISMarginRequest.heightMargin);
        }
        else if (0 != (m_instanceProperty.stabilizationType & IPEStabilizationTypeEIS3))
        {
            optimalInputWidth  += static_cast<UINT32>(optimalInputWidth * m_EISMarginRequest.widthMargin);
            optimalInputHeight += static_cast<UINT32>(optimalInputHeight * m_EISMarginRequest.heightMargin);
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Not EIS 2 or 3 for node %d instance %d, stabType %d",
                             Type(), InstanceID(), m_instanceProperty.stabilizationType);
        }

        // Add extra 3% for left and 3% for right(total 6%) on optimal input dim
        if ((TRUE == m_instanceProperty.enableFOVC) &&
            (optimalInputWidth < maxInputWidth)     &&
            (optimalInputHeight < maxInputHeight)   &&
            IsRealTime())
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE:%d bfr Add extra margin for fixed FOV width %d height %d",
                InstanceID(), optimalInputWidth, optimalInputHeight);

            optimalInputWidth += static_cast<UINT32>(optimalInputWidth * (FFOV_PER));
            optimalInputHeight += static_cast<UINT32>(optimalInputHeight * (FFOV_PER));

            minInputWidth += static_cast<UINT32>(minInputWidth * (FFOV_PER));
            minInputHeight += static_cast<UINT32>(minInputHeight * (FFOV_PER));

            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE:%d Afr Add extra margin for fixed FOV width %d height %d",
                InstanceID(), optimalInputWidth, optimalInputHeight);
            // if AF's fov factor is 0 then IPE should cropout 6%
            m_prevFOVC = FFOV_PER;
        }

        optimalInputWidth  = Utils::AlignGeneric32(optimalInputWidth, 4);
        optimalInputHeight = Utils::AlignGeneric32(optimalInputHeight, 4);

        minInputWidth  = Utils::AlignGeneric32(minInputWidth, 4);
        minInputHeight = Utils::AlignGeneric32(minInputHeight, 4);

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE:%d optimal input dimension after alignment width %d height %d",
                         InstanceID(), optimalInputWidth, optimalInputHeight);
    }

    if (CamxResultSuccess == result)
    {
        if ((optimalInputWidth == 0) || (optimalInputHeight == 0))
        {
            result = CamxResultEFailed;

            CAMX_LOG_ERROR(CamxLogGroupPProc,
                           "IPE:%d Buffer Negotiation Failed, W:%d x H:%d!\n",
                           InstanceID(),
                           optimalInputWidth,
                           optimalInputHeight);
        }
        else
        {
            if ((minInputWidth > maxInputWidth) ||
                (minInputHeight > maxInputHeight))
            {
                CAMX_LOG_WARN(CamxLogGroupPProc,
                              "IPE:%d "
                              "minInputWidth=%d maxInputWidth=%d minInputHeight=%d maxInputHeight=%d "
                              "Min > Max, unable to use current format",
                              InstanceID(),
                              minInputWidth, maxInputWidth, minInputHeight, maxInputHeight);
                result = CamxResultEFailed;
            }
            // Ensure optimal dimension is within min and max dimension,
            // There are chances that the optmial dimension is more than max dimension.
            // Correct for the same.
            UINT32              tempOptimalInputWidth              = 0;
            UINT32              tempOptimalInputHeight             = 0;

            tempOptimalInputWidth  =
                Utils::ClampUINT32(optimalInputWidth, minInputWidth, maxInputWidth);
            tempOptimalInputHeight =
                Utils::ClampUINT32(optimalInputHeight, minInputHeight, maxInputHeight);

            if ((tempOptimalInputWidth != optimalInputWidth) ||
                (tempOptimalInputHeight != optimalInputHeight))
            {
                optimalAspectRatio = static_cast<FLOAT>(tempOptimalInputWidth) / tempOptimalInputHeight;
                if ((TRUE == Utils::FEqualCoarse(optimalAspectRatio, selectedAspectRatio)) ||
                    ((TRUE == IsMFProcessingType()) && (FALSE == IsPostfilterWithDefault())))
                {
                    // The dimensions are fine. Do nothing
                }
                else if (optimalAspectRatio > selectedAspectRatio)
                {
                    tempOptimalInputHeight =
                        Utils::EvenFloorUINT32(static_cast<UINT32>(tempOptimalInputWidth / selectedAspectRatio));
                    // ensure that we dont exceed max
                    optimalInputHeight =
                        Utils::ClampUINT32(tempOptimalInputHeight, minInputHeight, maxInputHeight);
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d NonConformant AspectRatio:%f Change Height %d using AR:%f",
                        InstanceID(), optimalAspectRatio, optimalInputHeight, selectedAspectRatio);
                }
                else
                {
                    tempOptimalInputWidth =
                        Utils::EvenFloorUINT32(static_cast<UINT32>(tempOptimalInputHeight * selectedAspectRatio));
                    // ensure that we dont exceed max
                    optimalInputWidth =
                        Utils::ClampUINT32(tempOptimalInputWidth, minInputWidth, maxInputWidth);
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d NonConformant AspectRatio:%f Change Width %d using AR:%f",
                        InstanceID(), optimalAspectRatio, optimalInputWidth, selectedAspectRatio);
                }
            }
            UINT32 numInputPorts = 0;
            UINT32 inputPortId[IPEMaxInput];

            // Get Input Port List
            GetAllInputPortIds(&numInputPorts, &inputPortId[0]);

            pBufferNegotiationData->numInputPorts = numInputPorts;

            isIPEDownscalerEnabled = IsIPEOnlyDownscalerEnabled(pBufferNegotiationData);
            if (TRUE == isIPEDownscalerEnabled)
            {
                UINT32  IPEDownscalerInputWidth = 0;
                UINT32  IPEDownscalerInputHeight = 0;

                GetIPEDownscalerOnlyDimensions(optimalInputWidth,
                    optimalInputHeight,
                    &IPEDownscalerInputWidth,
                    &IPEDownscalerInputHeight,
                    downscaleLimit,
                    m_instanceProperty.ipeOnlyDownscalerMode);

                optimalInputWidth = IPEDownscalerInputWidth;
                optimalInputHeight = IPEDownscalerInputHeight;

                minInputWidth = IPEDownscalerInputWidth;
                minInputHeight = IPEDownscalerInputHeight;
            }

            for (UINT input = 0; input < numInputPorts; input++)
            {
                pBufferNegotiationData->inputBufferOptions[input].nodeId     = Type();
                pBufferNegotiationData->inputBufferOptions[input].instanceId = InstanceID();
                pBufferNegotiationData->inputBufferOptions[input].portId     = inputPortId[input];

                BufferRequirement* pInputBufferRequirement =
                    &pBufferNegotiationData->inputBufferOptions[input].bufferRequirement;

                pInputBufferRequirement->optimalWidth  = optimalInputWidth;
                pInputBufferRequirement->optimalHeight = optimalInputHeight;
                // If IPE is enabling SIMO and if one of the output is smaller than the other,
                // then the scale capabilities (min,max) needs to be adjusted after accounting for
                // the scaling needed on the smaller output port.
                pInputBufferRequirement->minWidth      = minInputWidth;
                pInputBufferRequirement->minHeight     = minInputHeight;

                pInputBufferRequirement->maxWidth      = maxInputWidth;
                pInputBufferRequirement->maxHeight     = maxInputHeight;

                CAMX_LOG_INFO(CamxLogGroupPProc,
                              "Buffer Negotiation Result: %d dims IPE: %d, Port %d Optimal %d x %d, Min %d x %d, Max %d x %d\n",
                              result,
                              InstanceID(),
                              inputPortId[input],
                              optimalInputWidth,
                              optimalInputHeight,
                              minInputWidth,
                              minInputHeight,
                              maxInputWidth,
                              maxInputHeight);
            }
        }
    }

    if ((FALSE == GetPipeline()->HasStatsNode()) || (TRUE == GetStaticSettings()->disableStatsNode))
    {
        m_isStatsNodeAvailable = FALSE;
    }
    else
    {
        m_isStatsNodeAvailable = TRUE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FinalizeBufferProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::FinalizeBufferProperties(
    BufferNegotiationData* pBufferNegotiationData)
{
    CamxResult result  = CamxResultSuccess;
    UINT               numInputPort;
    UINT               inputPortId[IPEMaxInput];
    const ImageFormat* pImageFormat = NULL;

    CAMX_ASSERT(NULL != pBufferNegotiationData);

    // Get Input Port List
    GetAllInputPortIds(&numInputPort, &inputPortId[0]);

    // Loop through input ports to get IPEInputPortFull
    for (UINT index = 0; index < numInputPort; index++)
    {
        if (pBufferNegotiationData->pInputPortNegotiationData[index].inputPortId == IPEInputPortFull)
        {
            pImageFormat = pBufferNegotiationData->pInputPortNegotiationData[index].pImageFormat;
            break;
        }
    }

    CAMX_ASSERT(NULL != pImageFormat);

    UINT width = 0;
    UINT height = 0;
    if (NULL != pImageFormat)
    {
        width  = pImageFormat->width;
        height = pImageFormat->height;
    }


    for (UINT index = 0; index < pBufferNegotiationData->numOutputPortsNotified; index++)
    {
        OutputPortNegotiationData* pOutputPortNegotiationData   = &pBufferNegotiationData->pOutputPortNegotiationData[index];
        InputPortNegotiationData*  pInputPortNegotiationData    = &pBufferNegotiationData->pInputPortNegotiationData[0];
        BufferProperties*          pFinalOutputBufferProperties = pOutputPortNegotiationData->pFinalOutputBufferProperties;
        UINT outputPortId = GetOutputPortId(pOutputPortNegotiationData->outputPortIndex);

        if ((FALSE == IsSinkPortWithBuffer(pOutputPortNegotiationData->outputPortIndex)) &&
            (FALSE == IsNonSinkHALBufferOutput(pOutputPortNegotiationData->outputPortIndex)))
        {
            switch (outputPortId)
            {
                case IPEOutputPortDisplay:
                    if (FALSE == m_nodePropDisableZoomCrop)
                    {
                        pFinalOutputBufferProperties->imageFormat.width  =
                        pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth;
                        pFinalOutputBufferProperties->imageFormat.height =
                        pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight;
                    }
                    else
                    {
                        CAMX_LOG_INFO(CamxLogGroupPProc, "IPE Profile ID is IPEProfileWithoutScale so no zoom");
                        pFinalOutputBufferProperties->imageFormat.width  =
                                pInputPortNegotiationData->pImageFormat->width;
                        pFinalOutputBufferProperties->imageFormat.height =
                                pInputPortNegotiationData->pImageFormat->height;
                    }
                    break;
                case IPEOutputPortVideo:
                    pFinalOutputBufferProperties->imageFormat.width  =
                        pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth;
                    pFinalOutputBufferProperties->imageFormat.height =
                        pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight;
                    break;
                case IPEOutputPortFullRef:
                    CAMX_ASSERT(0 < width);
                    CAMX_ASSERT(0 < height);
                    pFinalOutputBufferProperties->imageFormat.width  = width;
                    pFinalOutputBufferProperties->imageFormat.height = height;
                    break;
                case IPEOutputPortDS4Ref:
                    if (((m_instanceProperty.processingType == IPEProcessingType::IPEMFNRPostfilter) ||
                         (m_instanceProperty.processingType == IPEProcessingType::IPEMFSRPostfilter)) &&
                        (m_instanceProperty.profileId != IPEProfileId::IPEProfileIdScale))
                    {
                        pFinalOutputBufferProperties->imageFormat.width =
                            pBufferNegotiationData->pInputPortNegotiationData[0].pImageFormat->width;

                        pFinalOutputBufferProperties->imageFormat.height =
                            pBufferNegotiationData->pInputPortNegotiationData[0].pImageFormat->height;
                        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "DS4 port dim %d x %d",
                            pFinalOutputBufferProperties->imageFormat.width,
                            pFinalOutputBufferProperties->imageFormat.height);
                    }
                    else
                    {
                        CAMX_ASSERT(0 < width);
                        CAMX_ASSERT(0 < height);
                        pFinalOutputBufferProperties->imageFormat.width  =
                            Utils::EvenCeilingUINT32(Utils::AlignGeneric32(width, 4) / DS4Factor);
                        pFinalOutputBufferProperties->imageFormat.height =
                            Utils::EvenCeilingUINT32(Utils::AlignGeneric32(height, 4) / DS4Factor);
                    }
                    break;
                case IPEOutputPortDS16Ref:
                    CAMX_ASSERT(0 < width);
                    CAMX_ASSERT(0 < height);
                    pFinalOutputBufferProperties->imageFormat.width  =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(width, 16) / DS16Factor);
                    pFinalOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(height, 16) / DS16Factor);
                    break;
                case IPEOutputPortDS64Ref:
                    CAMX_ASSERT(0 < width);
                    CAMX_ASSERT(0 < height);
                    pFinalOutputBufferProperties->imageFormat.width  =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(width, 64) / DS64Factor);
                    pFinalOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(height, 64) / DS64Factor);
                    break;
                default:
                    break;
            }
            Utils::Memcpy(&pFinalOutputBufferProperties->imageFormat.planeAlignment[0],
                          &pOutputPortNegotiationData->outputBufferRequirementOptions.planeAlignment[0],
                          sizeof(AlignmentInfo) * FormatsMaxPlanes);
        }

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "output port %d, Final dim %d x %d",
                         outputPortId,
                         pFinalOutputBufferProperties->imageFormat.width,
                         pFinalOutputBufferProperties->imageFormat.height);
    }

    // Check ports buffer properties and disable ports as per firmware limitation
    if (TRUE == IsPortStatusUpdatedByOverride())
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ports with unsupported buffer properties got disabled");
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::CommitAllCommandBuffers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::CommitAllCommandBuffers(
    CmdBuffer**  ppIPECmdBuffer)
{
    CamxResult  result = CamxResultSuccess;

    CAMX_ASSERT(NULL != ppIPECmdBuffer[CmdBufferFrameProcess]);
    CAMX_ASSERT(NULL != ppIPECmdBuffer[CmdBufferIQSettings]);

    result = ppIPECmdBuffer[CmdBufferFrameProcess]->CommitCommands();
    if (CamxResultSuccess == result)
    {
        result = ppIPECmdBuffer[CmdBufferIQSettings]->CommitCommands();
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "failed to commit CmdBufferIQSettings");
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "failed to commit CmdBufferFrameProcess");
    }

    if ((NULL != ppIPECmdBuffer[CmdBufferStriping]) && (CamxResultSuccess == result))
    {
        result = ppIPECmdBuffer[CmdBufferStriping]->CommitCommands();
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "failed to commit CmdBufferStriping");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetCDMProgramArrayOffsetFromBase
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INT IPENode::GetCDMProgramArrayOffsetFromBase(
    CDMProgramArrayOrder    arrayIndex)
{
    INT offset = -1;

    CAMX_ASSERT(arrayIndex <= ProgramArrayICA2);
    /// @todo (CAMX-1033) Remove this function and make static variable holding offsets.
    if (arrayIndex <= ProgramArrayPreLTM)
    {
        offset = sizeof(CDMProgramArray) * arrayIndex;
    }
    else if (arrayIndex == ProgramArrayPostLTM)
    {
        offset = (sizeof(CDMProgramArray) * ProgramArrayPreLTM) + (sizeof(CdmProgram) * ProgramIndexMaxPreLTM);
    }
    else if (arrayIndex == ProgramArrayICA1)
    {
        offset =
            // size of postLTM CDM programs
            ((sizeof(CDMProgramArray) * ProgramArrayPostLTM) +
             (sizeof(CdmProgram) * ProgramIndexMaxPostLTM)   +
             (sizeof(CdmProgram) * ProgramIndexMaxPreLTM));
    }
    else if (arrayIndex == ProgramArrayICA2)
    {
        offset =
            // size of postLTM CDM programs
            ((sizeof(CDMProgramArray) * ProgramArrayICA1)  +
             (sizeof(CdmProgram) * ProgramIndexMaxPostLTM) +
             (sizeof(CdmProgram) * ProgramIndexMaxPreLTM));
    }

    return offset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetCDMProgramArrayOffsetFromTop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INT IPENode::GetCDMProgramArrayOffsetFromTop(
    CDMProgramArrayOrder    arrayIndex)
{
    INT offset = 0;

    offset = GetCDMProgramArrayOffsetFromBase(arrayIndex);
    if (offset >= 0)
    {
        offset += sizeof(IpeFrameProcess);
    }

    return offset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetCDMProgramOffset
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INT IPENode::GetCDMProgramOffset(
    CDMProgramArrayOrder    arrayIndex,
    UINT                    CDMProgramIndex)
{
    INT     offset              = 0;
    UINT    CDMProgramOffset    = offsetof(CDMProgramArray, programs) + offsetof(CdmProgram, cdmBaseAndLength) +
        offsetof(CDM_BASE_LENGHT, bitfields);

    offset = GetCDMProgramArrayOffsetFromTop(arrayIndex);
    if (offset >= 0)
    {
        offset += sizeof(CdmProgram) * CDMProgramIndex + CDMProgramOffset;
    }

    return offset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillPreLTMCDMProgram
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillPreLTMCDMProgram(
    CmdBuffer**             ppIPECmdBuffer,
    CDMProgramArray*        pCDMProgramArray,
    CdmProgram*             pCDMProgram,
    ProgramType             programType,
    PreLTMCDMProgramOrder   programIndex,
    UINT                    identifier)
{
    CamxResult result = CamxResultSuccess;

    if (m_preLTMLUTCount[programIndex] > 0)
    {
        UINT numPrograms                                    = pCDMProgramArray->numPrograms;
        pCDMProgram                                         = &pCDMProgramArray->programs[numPrograms];
        pCDMProgram->hasSingleReg                           = 0;
        pCDMProgram->programType                            = programType;
        pCDMProgram->uID                                    = identifier;
        pCDMProgram->cdmBaseAndLength.bitfields.LEN         = ((cdm_get_cmd_header_size(CDMCmdDMI) * RegisterWidthInBytes)
                                                               * m_preLTMLUTCount[programIndex]) - 1;
        pCDMProgram->cdmBaseAndLength.bitfields.RESERVED    = 0;
        pCDMProgram->cdmBaseAndLength.bitfields.BASE        = 0;
        pCDMProgram->bufferAllocatedInternally              = 0;

        /// @todo (CAMX-1033) Change below numPrograms to ProgramIndex once firmware support of program skip is available.
        INT offset  = GetCDMProgramOffset(ProgramArrayPreLTM, pCDMProgramArray->numPrograms);
        CAMX_ASSERT(offset >= 0);

        result      = ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset,
                                                                                    ppIPECmdBuffer[CmdBufferDMIHeader],
                                                                                    m_preLTMLUTOffset[programIndex]);
        (pCDMProgramArray->numPrograms)++;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillPostLTMCDMProgram
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillPostLTMCDMProgram(
    CmdBuffer**             ppIPECmdBuffer,
    CDMProgramArray*        pCDMProgramArray,
    CdmProgram*             pCDMProgram,
    ProgramType             programType,
    PostLTMCDMProgramOrder  programIndex,
    UINT                    identifier)
{
    CamxResult result = CamxResultSuccess;
    if (m_postLTMLUTCount[programIndex] > 0)
    {
        UINT numPrograms                                    = pCDMProgramArray->numPrograms;
        pCDMProgram                                         = &pCDMProgramArray->programs[numPrograms];
        pCDMProgram->hasSingleReg                           = 0;
        pCDMProgram->programType                            = programType;
        pCDMProgram->uID                                    = identifier;
        pCDMProgram->cdmBaseAndLength.bitfields.LEN         = ((cdm_get_cmd_header_size(CDMCmdDMI) * RegisterWidthInBytes)
                                                               * m_postLTMLUTCount[programIndex]) - 1;
        pCDMProgram->cdmBaseAndLength.bitfields.RESERVED    = 0;
        pCDMProgram->cdmBaseAndLength.bitfields.BASE        = 0;

        /// @todo (CAMX-1033) Change below numPrograms to ProgramIndex once firmware support of program skip is available.
        INT offset  = GetCDMProgramOffset(ProgramArrayPostLTM, pCDMProgramArray->numPrograms);
        CAMX_ASSERT(offset >= 0);
        result      = ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset,
                                                                                    ppIPECmdBuffer[CmdBufferDMIHeader],
                                                                                    m_postLTMLUTOffset[programIndex]);
        (pCDMProgramArray->numPrograms)++;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillNPSCDMProgram
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillNPSCDMProgram(
    CmdBuffer**             ppIPECmdBuffer,
    CDMProgramArray*        pCDMProgramArray,
    CdmProgram*             pCDMProgram,
    ProgramType             programType,
    CDMProgramArrayOrder    arrayIndex,
    UINT32                  passCmdBufferSize,
    UINT32                  passOffset)
{
    CamxResult result     = CamxResultSuccess;

    UINT numPrograms                                 = pCDMProgramArray->numPrograms;
    pCDMProgram                                      = &pCDMProgramArray->programs[numPrograms];
    pCDMProgram->hasSingleReg                        = 0;
    pCDMProgram->programType                         = programType;
    pCDMProgram->uID                                 = 0;
    pCDMProgram->cdmBaseAndLength.bitfields.LEN      = passCmdBufferSize - 1;
    pCDMProgram->cdmBaseAndLength.bitfields.RESERVED = 0;
    pCDMProgram->bufferAllocatedInternally           = 0;

    /// @todo (CAMX-1033) Change below numPrograms to ProgramIndex once firmware support of program skip is available.
    INT offset = GetCDMProgramOffset(arrayIndex, pCDMProgramArray->numPrograms);
    CAMX_ASSERT(offset >= 0);
    result = ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset,
                                                                           ppIPECmdBuffer[CmdBufferNPS],
                                                                           passOffset);
    (pCDMProgramArray->numPrograms)++;


    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillCDMProgramArrays
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillCDMProgramArrays(
    IpeFrameProcessData*    pFrameProcessData,
    IpeIQSettings*          pIpeIQSettings,
    CmdBuffer**             ppIPECmdBuffer,
    UINT                    batchFrames)
{
    INT                 offset;
    CdmProgram*         pCDMProgram;
    CDMProgramArray*    pCDMProgramArray;
    UINT8*              pCDMPayload;
    UINT                numPrograms     = 0;
    CamxResult          result          = CamxResultSuccess;
    ProgramType         type;
    UINT32              identifier      = 0;

    // Patch IQSettings buffer in IpeFrameProcessData
    offset = static_cast <UINT32>(offsetof(IpeFrameProcessData, iqSettingsAddr));
    result = ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset, ppIPECmdBuffer[CmdBufferIQSettings], 0);
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "%s: Patching failed for IQSettings", __FUNCTION__);
    }
    else
    {
        // Patch cdmProgramArrayBase, which is allocated contiguously below IpeFrameProcessData
        offset = static_cast <UINT32>(offsetof(IpeFrameProcessData, cdmProgramArrayBase));
        result = ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset,
                                                                               ppIPECmdBuffer[CmdBufferFrameProcess],
                                                                               sizeof(IpeFrameProcess));
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "%s: Patching failed for IQSettings", __FUNCTION__);
        }
    }

    // Populate offsets of all cdmPrograArrays in IpeFrameProcessData with respect to Base
    pFrameProcessData->cdmProgramArrayAnrFullPassAddr   =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayANRFullPass);
    pFrameProcessData->cdmProgramArrayAnrDc4Addr        =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayANRDS4);
    pFrameProcessData->cdmProgramArrayAnrDc16Addr       =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayANRDS16);
    pFrameProcessData->cdmProgramArrayAnrDc64Addr       =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayANRDS64);
    pFrameProcessData->cdmProgramArrayTfFullPassAddr    =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayTFFullPass);
    pFrameProcessData->cdmProgramArrayTfDc4Addr         =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayTFDS4);
    pFrameProcessData->cdmProgramArrayTfDc16Addr        =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayTFDS16);
    pFrameProcessData->cdmProgramArrayTfDc64Addr        =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayTFDS64);
    pFrameProcessData->cdmProgramArrayPreLtmAddr        =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayPreLTM);
    pFrameProcessData->cdmProgramArrayPostLtmAddr       =
        GetCDMProgramArrayOffsetFromBase(ProgramArrayPostLTM);

    for (UINT i = 0; i < batchFrames; i++)
    {
        pFrameProcessData->frameSets[i].cdmProgramArrayIca1Addr =
            GetCDMProgramArrayOffsetFromBase(ProgramArrayICA1);
        pFrameProcessData->frameSets[i].cdmProgramArrayIca2Addr =
            GetCDMProgramArrayOffsetFromBase(ProgramArrayICA2);

    }

    identifier = pFrameProcessData->requestId % GetPipeline()->GetRequestQueueDepth();

    pCDMPayload         = reinterpret_cast<UINT8*>(pFrameProcessData);
    pCDMProgramArray    =
        reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(ProgramArrayPreLTM));
    pCDMProgramArray->allocator     = 0;
    pCDMProgramArray->numPrograms   = 0;
    type = ((TRUE == pIpeIQSettings->cacParameters.moduleCfg.EN) ||
            (TRUE == pIpeIQSettings->colorTransformParameters.moduleCfg.EN)) ?
            PROGRAM_TYPE_GENERIC : PROGRAM_TYPE_SKIP;
    // CDMProgramArray :: Pre LTM Section
    numPrograms                            = pCDMProgramArray->numPrograms;
    pCDMProgram                            = &pCDMProgramArray->programs[numPrograms];
    pCDMProgram->hasSingleReg              = 0;
    pCDMProgram->programType               = type;
    pCDMProgram->uID                       = 0;
    pCDMProgram->bufferAllocatedInternally = 0;

    if (NULL != ppIPECmdBuffer[CmdBufferPreLTM])
    {
        UINT length = (ppIPECmdBuffer[CmdBufferPreLTM]->GetResourceUsedDwords() * RegisterWidthInBytes);
        if (length > 0)
        {
            pCDMProgram->cdmBaseAndLength.bitfields.LEN         = length - 1;
            pCDMProgram->cdmBaseAndLength.bitfields.RESERVED    = 0;

            // CDMProgram :: Pre LTM :: GENERIC Cmd buffer
            offset = GetCDMProgramOffset(ProgramArrayPreLTM, ProgramIndexPreLTMGeneric);
            CAMX_ASSERT(offset >= 0);

            ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset, ppIPECmdBuffer[CmdBufferPreLTM], 0);
            (pCDMProgramArray->numPrograms)++;
        }
    }

    if (NULL != ppIPECmdBuffer[CmdBufferDMIHeader] && (CamxResultSuccess == result))
    {
        type = ((TRUE == pIpeIQSettings->ltmParameters.moduleCfg.EN) ? IPE_LTM_LUT_PROGRAM : PROGRAM_TYPE_SKIP);
        result = FillPreLTMCDMProgram(ppIPECmdBuffer, pCDMProgramArray, pCDMProgram, type, ProgramIndexLTM, identifier);
    }

    // CDMProgramArray :: Post LTM Section
    pCDMProgramArray =
        reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(ProgramArrayPostLTM));
    pCDMProgramArray->allocator     = 0;
    pCDMProgramArray->numPrograms   = 0;

    type = (
        (TRUE == pIpeIQSettings->glutParameters.moduleCfg.EN)              ||
        (TRUE == pIpeIQSettings->chromaEnhancementParameters.moduleCfg.EN) ||
        (TRUE == pIpeIQSettings->lut2dParameters.moduleCfg.EN)             ||
        (TRUE == pIpeIQSettings->asfParameters.moduleCfg.EN)               ||
        (TRUE == pIpeIQSettings->chromaSupressionParameters.moduleCfg.EN)  ||
        (TRUE == pIpeIQSettings->skinEnhancementParameters.moduleCfg.EN)   ||
        (TRUE == pIpeIQSettings->colorCorrectParameters.moduleCfg.EN))     ?
                PROGRAM_TYPE_GENERIC : PROGRAM_TYPE_SKIP;

    // CDMProgram :: Generic
    numPrograms                            = pCDMProgramArray->numPrograms;
    pCDMProgram                            = &pCDMProgramArray->programs[numPrograms];
    pCDMProgram->hasSingleReg              = 0;
    pCDMProgram->programType               = type;
    pCDMProgram->uID                       = 0;
    pCDMProgram->bufferAllocatedInternally = 0;

    if (NULL != ppIPECmdBuffer[CmdBufferPostLTM])
    {
        UINT length = (ppIPECmdBuffer[CmdBufferPostLTM]->GetResourceUsedDwords() * RegisterWidthInBytes);
        if (length > 0)
        {
            pCDMProgram->cdmBaseAndLength.bitfields.LEN         = length - 1;
            pCDMProgram->cdmBaseAndLength.bitfields.RESERVED    = 0;

            // Generic Reg Random CDM from pre LTM Modules
            offset = GetCDMProgramOffset(ProgramArrayPostLTM, ProgramIndexPostLTMGeneric);
            CAMX_ASSERT(offset >= 0);

            ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset, ppIPECmdBuffer[CmdBufferPostLTM], 0);
            (pCDMProgramArray->numPrograms)++;
        }
    }

    if (NULL != ppIPECmdBuffer[CmdBufferDMIHeader])
    {
        // CDMProgram :: Gamma LUT
        if (CamxResultSuccess == result)
        {
            type = ((TRUE == pIpeIQSettings->glutParameters.moduleCfg.EN) ?
                IPE_GAMMA_GLUT_LUT_PROGRAM : PROGRAM_TYPE_SKIP);
            result = FillPostLTMCDMProgram(ppIPECmdBuffer,
                                           pCDMProgramArray,
                                           pCDMProgram,
                                           type,
                                           ProgramIndexGLUT,
                                           identifier);
        }
        // CDMProgram :: 2D LUT
        if (CamxResultSuccess == result)
        {
            type = ((TRUE == pIpeIQSettings->lut2dParameters.moduleCfg.EN) ?
                 IPE_2D_LUT_LUT_PROGRAM : PROGRAM_TYPE_SKIP);
            result = FillPostLTMCDMProgram(ppIPECmdBuffer,
                                           pCDMProgramArray,
                                           pCDMProgram,
                                           type,
                                           ProgramIndex2DLUT,
                                           identifier);
        }
        // CDMProgram :: ASF LUT
        if (CamxResultSuccess == result)
        {
            type = ((TRUE == pIpeIQSettings->asfParameters.moduleCfg.EN) ?
                IPE_ASF_LUT_PROGRAM : PROGRAM_TYPE_SKIP);
            result = FillPostLTMCDMProgram(ppIPECmdBuffer,
                                           pCDMProgramArray,
                                           pCDMProgram,
                                           type,
                                           ProgramIndexASF,
                                           identifier);
        }
        // CDMProgram :: GRA LUT
        type = ((TRUE == pIpeIQSettings->graParameters.moduleCfg.EN) ?
            IPE_GRA_LUT_PROGRAM : PROGRAM_TYPE_SKIP);
        if (CamxResultSuccess == result)
        {
            result = FillPostLTMCDMProgram(ppIPECmdBuffer,
                                           pCDMProgramArray,
                                           pCDMProgram,
                                           type,
                                           ProgramIndexGRA,
                                           identifier);
        }

        if (CamxResultSuccess == result)
        {
            // CDMProgram :: ICA1 LUT
            // if module is disabled dynamically skip the CDM program
            type = ((TRUE == pIpeIQSettings->ica1Parameters.isGridEnable) ||
                    (TRUE == pIpeIQSettings->ica1Parameters.isPerspectiveEnable)) ?
                    IPE_ICA1_LUT_PROGRAM : PROGRAM_TYPE_SKIP;
            result = FillICACDMprograms(pFrameProcessData,
                                        ppIPECmdBuffer,
                                        type,
                                        ProgramArrayICA1,
                                        ProgramIndexICA1,
                                        identifier);
        }

        if (CamxResultSuccess == result)
        {
            // CDMProgram :: ICA2 LUT
            type = ((TRUE == pIpeIQSettings->ica2Parameters.isGridEnable) ||
                    (TRUE == pIpeIQSettings->ica2Parameters.isPerspectiveEnable)) ?
                    IPE_ICA2_LUT_PROGRAM : PROGRAM_TYPE_SKIP;
            result = FillICACDMprograms(pFrameProcessData,
                                        ppIPECmdBuffer,
                                        type,
                                        ProgramArrayICA2,
                                        ProgramIndexICA2,
                                        identifier);
        }
    }

    if (NULL != ppIPECmdBuffer[CmdBufferNPS])
    {
        // CDMProgramArray :: NPS : ANR Full Pass
        pCDMProgramArray =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload +
                GetCDMProgramArrayOffsetFromTop(ProgramArrayANRFullPass));

        pCDMProgramArray->allocator   = 0;
        pCDMProgramArray->numPrograms = 0;

        numPrograms = pCDMProgramArray->numPrograms;
        pCDMProgram = &pCDMProgramArray->programs[numPrograms];

        type = (TRUE == pIpeIQSettings->anrParameters.parameters[0].moduleCfg.EN) ?
            IPE_ANR_CYLPF_PROGRAM : PROGRAM_TYPE_SKIP;
        result = FillNPSCDMProgram(ppIPECmdBuffer,
                                   pCDMProgramArray,
                                   pCDMProgram,
                                   type,
                                   ProgramArrayANRFullPass,
                                   m_ANRSinglePassCmdBufferSize,
                                   m_ANRPassOffset[PASS_NAME_FULL]);

        // CDMProgramArray :: NPS : ANR DS4 Pass
        pCDMProgramArray =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(ProgramArrayANRDS4));

        pCDMProgramArray->allocator   = 0;
        pCDMProgramArray->numPrograms = 0;

        numPrograms = pCDMProgramArray->numPrograms;
        pCDMProgram = &pCDMProgramArray->programs[numPrograms];

        type = (TRUE == pIpeIQSettings->anrParameters.parameters[1].moduleCfg.EN) ?
            IPE_ANR_CYLPF_PROGRAM : PROGRAM_TYPE_SKIP;
        result = FillNPSCDMProgram(ppIPECmdBuffer,
                                   pCDMProgramArray,
                                   pCDMProgram,
                                   type,
                                   ProgramArrayANRDS4,
                                   m_ANRSinglePassCmdBufferSize,
                                   m_ANRPassOffset[PASS_NAME_DC_4]);

        // CDMProgramArray :: NPS : ANR DS16 Pass
        pCDMProgramArray =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(ProgramArrayANRDS16));

        pCDMProgramArray->allocator   = 0;
        pCDMProgramArray->numPrograms = 0;

        numPrograms = pCDMProgramArray->numPrograms;
        pCDMProgram = &pCDMProgramArray->programs[numPrograms];

        type = (TRUE == pIpeIQSettings->anrParameters.parameters[2].moduleCfg.EN) ?
            IPE_ANR_CYLPF_PROGRAM : PROGRAM_TYPE_SKIP;
        result = FillNPSCDMProgram(ppIPECmdBuffer,
                                   pCDMProgramArray,
                                   pCDMProgram,
                                   type,
                                   ProgramArrayANRDS16,
                                   m_ANRSinglePassCmdBufferSize,
                                   m_ANRPassOffset[PASS_NAME_DC_16]);


        // CDMProgramArray :: NPS : ANR DS64 Pass
        pCDMProgramArray =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(ProgramArrayANRDS64));

        pCDMProgramArray->allocator   = 0;
        pCDMProgramArray->numPrograms = 0;

        numPrograms = pCDMProgramArray->numPrograms;
        pCDMProgram = &pCDMProgramArray->programs[numPrograms];

        type = (TRUE == pIpeIQSettings->anrParameters.parameters[3].moduleCfg.EN) ?
            IPE_ANR_CYLPF_PROGRAM : PROGRAM_TYPE_SKIP;
        result = FillNPSCDMProgram(ppIPECmdBuffer,
                                   pCDMProgramArray,
                                   pCDMProgram,
                                   type,
                                   ProgramArrayANRDS64,
                                   m_ANRSinglePassCmdBufferSize,
                                   m_ANRPassOffset[PASS_NAME_DC_64]);

        // CDMProgramArray :: NPS : TF Full Pass
        pCDMProgramArray =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(ProgramArrayTFFullPass));

        pCDMProgramArray->allocator   = 0;
        pCDMProgramArray->numPrograms = 0;

        numPrograms = pCDMProgramArray->numPrograms;
        pCDMProgram = &pCDMProgramArray->programs[numPrograms];

        type = (TRUE == pIpeIQSettings->tfParameters.parameters[0].moduleCfg.EN) ?
            IPE_TF_PROGRAM : PROGRAM_TYPE_SKIP;
        result = FillNPSCDMProgram(ppIPECmdBuffer,
                                   pCDMProgramArray,
                                   pCDMProgram,
                                   type,
                                   ProgramArrayTFFullPass,
                                   m_TFSinglePassCmdBufferSize,
                                   m_TFPassOffset[PASS_NAME_FULL]);
        // CDMProgramArray :: NPS : TF DS4 Pass
        pCDMProgramArray =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(ProgramArrayTFDS4));

        pCDMProgramArray->allocator   = 0;
        pCDMProgramArray->numPrograms = 0;

        numPrograms = pCDMProgramArray->numPrograms;
        pCDMProgram = &pCDMProgramArray->programs[numPrograms];


        type = (TRUE == pIpeIQSettings->tfParameters.parameters[1].moduleCfg.EN) ?
            IPE_TF_PROGRAM : PROGRAM_TYPE_SKIP;
        result = FillNPSCDMProgram(ppIPECmdBuffer,
                                   pCDMProgramArray,
                                   pCDMProgram,
                                   type,
                                   ProgramArrayTFDS4,
                                   m_TFSinglePassCmdBufferSize,
                                   m_TFPassOffset[PASS_NAME_DC_4]);

        // CDMProgramArray :: NPS : TF DS16 Pass
        pCDMProgramArray =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(ProgramArrayTFDS16));

        pCDMProgramArray->allocator   = 0;
        pCDMProgramArray->numPrograms = 0;

        numPrograms = pCDMProgramArray->numPrograms;
        pCDMProgram = &pCDMProgramArray->programs[numPrograms];

        type = (TRUE == pIpeIQSettings->tfParameters.parameters[2].moduleCfg.EN) ?
            IPE_TF_PROGRAM : PROGRAM_TYPE_SKIP;
        result = FillNPSCDMProgram(ppIPECmdBuffer,
                                   pCDMProgramArray,
                                   pCDMProgram,
                                   type,
                                   ProgramArrayTFDS16,
                                   m_TFSinglePassCmdBufferSize,
                                   m_TFPassOffset[PASS_NAME_DC_16]);

        // CDMProgramArray :: NPS : TF DS64 Pass
        pCDMProgramArray =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(ProgramArrayTFDS64));

        pCDMProgramArray->allocator   = 0;
        pCDMProgramArray->numPrograms = 0;

        numPrograms = pCDMProgramArray->numPrograms;
        pCDMProgram = &pCDMProgramArray->programs[numPrograms];

        type = (TRUE == pIpeIQSettings->tfParameters.parameters[3].moduleCfg.EN) ?
            IPE_TF_PROGRAM : PROGRAM_TYPE_SKIP;
        result = FillNPSCDMProgram(ppIPECmdBuffer,
                                   pCDMProgramArray,
                                   pCDMProgram,
                                   type,
                                   ProgramArrayTFDS64,
                                   m_TFSinglePassCmdBufferSize,
                                   m_TFPassOffset[PASS_NAME_DC_64]);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillICACDMprograms
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillICACDMprograms(
    IpeFrameProcessData*    pFrameProcessData,
    CmdBuffer**             ppIPECmdBuffer,
    ProgramType             programType,
    CDMProgramArrayOrder    programArrayOrder,
    ICAProgramOrder         programIndex,
    UINT                    identifier)
{
    CamxResult          result           = CamxResultSuccess;
    UINT8*              pCDMPayload      = NULL;
    CDMProgramArray*    pCDMProgramArray = NULL;
    CdmProgram*         pCDMProgram      = NULL;

    if (m_ICALUTCount[programIndex] > 0)
    {
        pCDMPayload                                      = reinterpret_cast<UINT8*>(pFrameProcessData);
        pCDMProgramArray                                 =
            reinterpret_cast<CDMProgramArray*>(pCDMPayload + GetCDMProgramArrayOffsetFromTop(programArrayOrder));

        pCDMProgramArray->allocator                      = 0;
        pCDMProgramArray->numPrograms                    = 0;
        pCDMProgram                                      = &pCDMProgramArray->programs[pCDMProgramArray->numPrograms];
        pCDMProgram->hasSingleReg                        = 0;
        pCDMProgram->programType                         = programType;
        pCDMProgram->uID                                 = identifier;
        pCDMProgram->cdmBaseAndLength.bitfields.LEN      = ((cdm_get_cmd_header_size(CDMCmdDMI) * RegisterWidthInBytes)
            * m_ICALUTCount[programIndex]) - 1;
        pCDMProgram->cdmBaseAndLength.bitfields.RESERVED = 0;
        pCDMProgram->cdmBaseAndLength.bitfields.BASE     = 0;

        /// @todo (CAMX-1033) Change below numPrograms to ProgramIndex once firmware support of program skip is available.
        INT offset = GetCDMProgramOffset(programArrayOrder, pCDMProgramArray->numPrograms);
        CAMX_ASSERT(offset >= 0);
        result = ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset,
                                                                               ppIPECmdBuffer[CmdBufferDMIHeader],
                                                                               m_ICALUTOffset[programIndex]);
        (pCDMProgramArray->numPrograms)++;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetMetadataTags
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetMetadataTags(
    ISPInputData* pModuleInput)
{
    CamxResult    result                 = CamxResultSuccess;
    CamxResult    resultContrast         = CamxResultSuccess;
    UINT32        metaTag                = 0;
    UINT32        metaTagSharpness       = 0;
    UINT32        metaTagContrast        = 0;
    UINT32        metaTagMFNRTotalFrames = 0;
    UINT32        metaTagMFSRTotalFrames = 0;
    UINT32        metaTagDynamicContrast = 0;
    UINT32        metaTagDarkBoostStrength = 0;
    UINT32        metaTagBrightSupressStrength = 0;
    ISPTonemapPoint*    pBlueTonemapCurve   = NULL;
    ISPTonemapPoint*    pGreenTonemapCurve  = NULL;
    ISPTonemapPoint*    pRedTonemapCurve    = NULL;
    const PlatformStaticCaps*   pStaticCaps = HwEnvironment::GetInstance()->GetPlatformStaticCaps();
    // Populate default value
    pModuleInput->pHALTagsData->saturation                    = 5;
    pModuleInput->pHALTagsData->sharpness                     = 1.0f;
    pModuleInput->pHALTagsData->noiseReductionMode            = NoiseReductionModeFast;
    pModuleInput->pHALTagsData->edgeMode                      = EdgeModeOff;
    pModuleInput->pHALTagsData->controlVideoStabilizationMode = ControlVideoStabilizationModeOff;
    pModuleInput->pHALTagsData->ltmContrastStrength.ltmDynamicContrastStrength      = 0.0f;
    pModuleInput->pHALTagsData->ltmContrastStrength.ltmDarkBoostStrength            = 0.0f;
    pModuleInput->pHALTagsData->ltmContrastStrength.ltmBrightSupressStrength        = 0.0f;

    result = VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.saturation",
                                                      "use_saturation",
                                                      &metaTag);
    result = VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.sharpness",
                                                      "strength",
                                                      &metaTagSharpness);
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.mfnrconfigs",
                                                      "MFNRTotalNumFrames",
                                                      &metaTagMFNRTotalFrames);
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.mfsrconfigs",
                                                      "MFSRTotalNumFrames",
                                                      &metaTagMFSRTotalFrames);
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.ltmDynamicContrast",
                                                       "ltmDynamicContrastStrength",
                                                       &metaTagDynamicContrast);
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.ltmDynamicContrast",
                                                       "ltmDarkBoostStrength",
                                                       &metaTagDarkBoostStrength);
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.ltmDynamicContrast",
                                                      "ltmBrightSupressStrength",
                                                       &metaTagBrightSupressStrength);

    static const UINT VendorTagsIPE[] =
    {
        metaTag | InputMetadataSectionMask,
        InputEdgeMode,
        InputControlVideoStabilizationMode,
        metaTagSharpness| InputMetadataSectionMask,
        metaTagMFNRTotalFrames | InputMetadataSectionMask,
        metaTagMFSRTotalFrames | InputMetadataSectionMask,
        InputColorCorrectionAberrationMode,
        InputNoiseReductionMode,
        InputTonemapMode,
        InputColorCorrectionMode,
        InputControlMode,
        InputTonemapCurveBlue,
        InputTonemapCurveGreen,
        InputTonemapCurveRed,
        InputColorCorrectionGains,
        InputColorCorrectionMode,
        InputColorCorrectionTransform,
        InputControlAEMode,
        InputControlAWBMode,
        InputControlAWBLock,
        metaTagDynamicContrast | InputMetadataSectionMask,
        metaTagDarkBoostStrength | InputMetadataSectionMask,
        metaTagBrightSupressStrength | InputMetadataSectionMask,
    };

    const static UINT length = CAMX_ARRAY_SIZE(VendorTagsIPE);
    VOID* pData[length] = { 0 };
    UINT64 vendorTagsIPEDataIPEOffset[length] = { 0 };

    GetDataList(VendorTagsIPE, pData, vendorTagsIPEDataIPEOffset, length);

    if (NULL != pData[0])
    {
        Utils::Memcpy(&pModuleInput->pHALTagsData->saturation, pData[0], sizeof(&pModuleInput->pHALTagsData->saturation));
    }

    if (NULL != pData[1])
    {
        Utils::Memcpy(&pModuleInput->pHALTagsData->edgeMode, pData[1], sizeof(&pModuleInput->pHALTagsData->edgeMode));
    }

    if (NULL != pData[2])
    {
        Utils::Memcpy(&pModuleInput->pHALTagsData->controlVideoStabilizationMode,
            pData[2], sizeof(&pModuleInput->pHALTagsData->controlVideoStabilizationMode));
    }

    if (IPEProcessingType::IPEProcessingTypeDefault != m_instanceProperty.processingType)
    {
        if (IPEProcessingType::IPEMFNRPrefilter <= m_instanceProperty.processingType &&
            IPEProcessingType::IPEMFNRPostfilter >= m_instanceProperty.processingType)
        {
            pModuleInput->pipelineIPEData.numOfFrames = *(static_cast<UINT *>(pData[4]));
            CAMX_LOG_INFO(CamxLogGroupPProc, "Total number of MFNR Frames = %d", pModuleInput->pipelineIPEData.numOfFrames);
        }
        else if (IPEProcessingType::IPEMFSRPrefilter <= m_instanceProperty.processingType &&
                 IPEProcessingType::IPEMFSRPostfilter >= m_instanceProperty.processingType)
        {
            pModuleInput->pipelineIPEData.numOfFrames = *(static_cast<UINT *>(pData[5]));
            CAMX_LOG_INFO(CamxLogGroupPProc, "Total number of MFSR Frames = %d", pModuleInput->pipelineIPEData.numOfFrames);
        }

        if (pModuleInput->pipelineIPEData.numOfFrames < 3)
        {
            pModuleInput->pipelineIPEData.numOfFrames = 3;
            CAMX_LOG_WARN(CamxLogGroupPProc, "hardcoded Total number of MFNR/MFSR frames to 3");
        }
        else if (pModuleInput->pipelineIPEData.numOfFrames > 8)
        {
            pModuleInput->pipelineIPEData.numOfFrames = 8;
            CAMX_LOG_WARN(CamxLogGroupPProc, "hardcoded Total number of MFNR/MFSR frames to 8");
        }
    }

    if (NULL != pData[3])
    {
        pModuleInput->pHALTagsData->sharpness =
        static_cast<FLOAT> (*(static_cast<UINT *>(pData[3]))) / pStaticCaps->sharpnessRange.defValue;
    }

    if (NULL != pData[6])
    {
        pModuleInput->pHALTagsData->colorCorrectionAberrationMode = *(static_cast<UINT8*>(pData[6]));
    }

    if (NULL != pData[7])
    {
        pModuleInput->pHALTagsData->noiseReductionMode = *(static_cast<UINT8*>(pData[7]));
    }

    if (NULL != pData[8])
    {
        pModuleInput->pHALTagsData->tonemapCurves.tonemapMode = *(static_cast<UINT8*>(pData[8]));
    }

    if (NULL != pData[9])
    {
        pModuleInput->pHALTagsData->colorCorrectionMode = *(static_cast<UINT8*>(pData[9]));
    }

    if (NULL != pData[10])
    {
        pModuleInput->pHALTagsData->controlMode = *(static_cast<UINT8*>(pData[10]));
    }

    if (NULL != pData[11])
    {
        pBlueTonemapCurve = static_cast<ISPTonemapPoint*>(pData[11]);
    }

    if (NULL != pData[12])
    {
        pGreenTonemapCurve = static_cast<ISPTonemapPoint*>(pData[12]);
    }

    if (NULL != pData[13])
    {
        pRedTonemapCurve = static_cast<ISPTonemapPoint*>(pData[13]);
    }

    if (NULL != pData[14])
    {
        pModuleInput->pHALTagsData->colorCorrectionGains = *(static_cast<ColorCorrectionGain*>(pData[14]));
    }

    if (NULL != pData[15])
    {
        pModuleInput->pHALTagsData->colorCorrectionMode = *(static_cast<UINT8*>(pData[15]));
    }

    if (NULL != pData[16])
    {
        pModuleInput->pHALTagsData->colorCorrectionTransform = *(static_cast<ISPColorCorrectionTransform*>(pData[16]));
    }

    if (NULL != pData[17])
    {
        pModuleInput->pHALTagsData->controlAEMode = *(static_cast<UINT8*>(pData[17]));
    }

    if (NULL != pData[18])
    {
        pModuleInput->pHALTagsData->controlAWBMode = *(static_cast<UINT8*>(pData[18]));
    }

    if (NULL != pData[19])
    {
        pModuleInput->pHALTagsData->controlAWBLock = *(static_cast<UINT8*>(pData[19]));
    }

    if (NULL != pData[20] && NULL != pData[21] && NULL != pData[22])
    {
        pModuleInput->pHALTagsData->ltmContrastStrength.ltmDynamicContrastStrength  = *(static_cast<FLOAT*>(pData[20]));
        pModuleInput->pHALTagsData->ltmContrastStrength.ltmDarkBoostStrength        = *(static_cast<FLOAT*>(pData[21]));
        pModuleInput->pHALTagsData->ltmContrastStrength.ltmBrightSupressStrength    = *(static_cast<FLOAT*>(pData[22]));
    }

    resultContrast = VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.contrast",
                                                              "level",
                                                              &metaTagContrast);
    if (CamxResultSuccess == resultContrast)
    {
        static const UINT VendorTagContrast[] =
        {
            metaTagContrast | InputMetadataSectionMask,
        };

        const static UINT lengthContrast    = CAMX_ARRAY_SIZE(VendorTagContrast);
        VOID* pDataContrast[lengthContrast] = { 0 };
        UINT64 vendorTagsContrastIPEOffset[lengthContrast] = { 0 };

        GetDataList(VendorTagContrast, pDataContrast, vendorTagsContrastIPEOffset, lengthContrast);
        if (NULL != pDataContrast[0])
        {
            UINT8 appLevel = *(static_cast<UINT8*>(pDataContrast[0]));
            if (appLevel > 0)
            {
                pModuleInput->pHALTagsData->contrastLevel = appLevel - 1;
            }
            else
            {
                pModuleInput->pHALTagsData->contrastLevel = 5;
            }
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Manual Contrast Level = %d", pModuleInput->pHALTagsData->contrastLevel);
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupPProc, "Cannot obtain Contrast Level. Set default to 5");
            pModuleInput->pHALTagsData->contrastLevel = 5;
        }
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupPProc, "No Contrast Level available. Set default to 5");
        pModuleInput->pHALTagsData->contrastLevel = 5; // normal without contrast change
    }

    // Deep copy tone map curves, only when the tone map is contrast curve
    if (TonemapModeContrastCurve == pModuleInput->pHALTagsData->tonemapCurves.tonemapMode)
    {
        CAMX_ASSERT(NULL != pBlueTonemapCurve);
        CAMX_ASSERT(NULL != pGreenTonemapCurve);
        CAMX_ASSERT(NULL != pRedTonemapCurve);

        pModuleInput->pHALTagsData->tonemapCurves.curvePoints = static_cast<INT32>(
            GetDataCountFromPipeline(InputTonemapCurveRed, 0, GetPipeline()->GetPipelineId(), TRUE));

        if ((pModuleInput->pHALTagsData->tonemapCurves.curvePoints > 0) && (NULL != pRedTonemapCurve))
        {
            // Red tone map curve
            Utils::Memcpy(pModuleInput->pHALTagsData->tonemapCurves.tonemapCurveRed,
                          pRedTonemapCurve,
                          (sizeof(ISPTonemapPoint) * pModuleInput->pHALTagsData->tonemapCurves.curvePoints));
        }

        pModuleInput->pHALTagsData->tonemapCurves.curvePoints = static_cast<INT32>(
            GetDataCountFromPipeline(InputTonemapCurveBlue, 0, GetPipeline()->GetPipelineId(), TRUE));

        if ((pModuleInput->pHALTagsData->tonemapCurves.curvePoints > 0) && (NULL != pBlueTonemapCurve))
        {
            // Blue tone map curve
            Utils::Memcpy(pModuleInput->pHALTagsData->tonemapCurves.tonemapCurveBlue,
                          pBlueTonemapCurve,
                          (sizeof(ISPTonemapPoint) * pModuleInput->pHALTagsData->tonemapCurves.curvePoints));
        }

        pModuleInput->pHALTagsData->tonemapCurves.curvePoints = static_cast<INT32>(
            GetDataCountFromPipeline(InputTonemapCurveGreen, 0, GetPipeline()->GetPipelineId(), TRUE));

        if ((pModuleInput->pHALTagsData->tonemapCurves.curvePoints > 0) && (NULL != pGreenTonemapCurve))
        {
            // Green tone map curve
            Utils::Memcpy(pModuleInput->pHALTagsData->tonemapCurves.tonemapCurveGreen,
                          pGreenTonemapCurve,
                          (sizeof(ISPTonemapPoint) * pModuleInput->pHALTagsData->tonemapCurves.curvePoints));
        }
    }

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetEISMargin
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetEISMargin()
{
    CamxResult    result          = CamxResultSuccess;
    UINT32        marginEISV2Tag  = 0;
    UINT32        marginEISV3Tag  = 0;

    if (0 != (IPEStabilizationType::IPEStabilizationTypeEIS2 & m_instanceProperty.stabilizationType))
    {
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.eisrealtime", "RequestedMargin", &marginEISV2Tag);
        CAMX_ASSERT(CamxResultSuccess == result);

        UINT   marginTags[1] = { marginEISV2Tag | UsecaseMetadataSectionMask };
        VOID*  pData[1]      = { 0 };
        UINT64 offset[1]     = { 0 };

        result = GetDataList(marginTags, pData, offset, CAMX_ARRAY_SIZE(marginTags));
        if (CamxResultSuccess == result)
        {
            if (NULL != pData[0])
            {
                m_EISMarginRequest = *static_cast<MarginRequest*>(pData[0]);
            }
        }

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "EISv2 margin requested: W %f, H %f", m_EISMarginRequest.widthMargin,
                         m_EISMarginRequest.heightMargin);
    }
    else if (0 != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType))
    {
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.eislookahead", "RequestedMargin", &marginEISV3Tag);
        CAMX_ASSERT(CamxResultSuccess == result);

        UINT   marginTags[1] = { marginEISV3Tag | UsecaseMetadataSectionMask };
        VOID*  pData[1]      = { 0 };
        UINT64 offset[1]     = { 0 };

        result = GetDataList(marginTags, pData, offset, CAMX_ARRAY_SIZE(marginTags));
        if (CamxResultSuccess == result)
        {

            if (NULL != pData[0])
            {
                m_EISMarginRequest = *static_cast<MarginRequest*>(pData[0]);
            }
        }

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "EISv3 margin requested:  W %f, H %f", m_EISMarginRequest.widthMargin,
                         m_EISMarginRequest.heightMargin);
    }
    else if (0 != m_instanceProperty.stabilizationType)
    {
        CAMX_LOG_WARN(CamxLogGroupPProc, "No margin for stabilization type %d", m_instanceProperty.stabilizationType);
    }

    CAMX_ASSERT(m_EISMarginRequest.widthMargin == m_EISMarginRequest.heightMargin);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::UpdateClock
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::UpdateClock(
    ExecuteProcessRequestData*   pExecuteProcessRequestData,
    CSLICPClockBandwidthRequest* pICPClockBandwidthRequest)
{
    NodeProcessRequestData* pNodeRequestData = pExecuteProcessRequestData->pNodeProcessRequestData;
    UINT64                  requestId        = pNodeRequestData->pCaptureRequest->requestId;
    UINT                    frameCycles;
    UINT                    FPS               = DefaultFPS;
    UINT64                  budgetNS;
    FLOAT                   budget;

    // Framecycles calculation considers Number of Pixels processed in the current frame, Overhead and Efficiency
    if (0 != m_FPS)
    {
        FPS = m_FPS;
    }

    frameCycles                             = pICPClockBandwidthRequest->frameCycles;
    frameCycles                             = static_cast<UINT>(frameCycles  / m_IPEClockEfficiency);
    // Budget is the Max duration of current frame to process
    budget                                  = 1.0f / FPS;
    budgetNS                                = static_cast<UINT64>(budget * NanoSecondMult);

    pICPClockBandwidthRequest->budgetNS     = budgetNS;
    pICPClockBandwidthRequest->frameCycles  = frameCycles;
    pICPClockBandwidthRequest->realtimeFlag = IsRealTime();

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "[%s][%llu] FPS = %d budget = %lf budgetNS = %lld fc = %d",
                     NodeIdentifierString(), requestId, FPS, budget, budgetNS, frameCycles);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::CalculateIPERdBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::CalculateIPERdBandwidth(
    PerRequestActivePorts*  pPerRequestPorts,
    IPEBandwidth*           pBandwidth)
{
    UINT   srcWidth               = 0;
    UINT   srcHeight              = 0;
    FLOAT  bppSrc                 = IPEBpp8Bit;
    FLOAT  overhead               = IPEBandwidthOverhead;
    FLOAT  EISOverhead            = IPEEISOverhead;
    FLOAT  IPEUbwcRdCr            = IPEUBWCRdCompressionRatio;
    FLOAT  IPEUbwcMCTFr           = IPEUBWCMctfReadCompressionRatio;
    DOUBLE swMargin               = IPESwMargin;
    BOOL   UBWCEnable             = FALSE;
    UINT32 UBWCVersion            = 0;
    UINT   FPS                    = pBandwidth->FPS;
    UINT64 readBandwidthPartial   = 0;
    UINT64 readBandwidthPass0;
    UINT64 readBandwidthPass1;
    UINT64 readBandwidthPass2;
    UINT64 readBandwidthPass3;

    pBandwidth->readBW.unCompressedBW = 0;
    pBandwidth->readBW.compressedBW   = 0;
    UBWCVersion                       = ImageFormatUtils::GetUBWCVersion();

    for (UINT i = 0; i < pPerRequestPorts->numInputPorts; i++)
    {
        PerRequestInputPortInfo* pInputPort = &pPerRequestPorts->pInputPorts[i];

        if (pInputPort->portId == CSLIPEInputPortIdFull)
        {
            const ImageFormat* pImageFormat = GetInputPortImageFormat(InputPortIndex(pInputPort->portId));

            GetSizeWithoutStablizationMargin(m_fullInputWidth, m_fullInputHeight, &srcWidth, &srcHeight);

            if (srcHeight < IPEPartialRdSourceHeight)
            {
                readBandwidthPartial = IPEPartialRdMultiplication * FPS;
                readBandwidthPartial = (readBandwidthPartial * srcHeight) / IPEPartialRdSourceHeight;
            }
            else
            {
                readBandwidthPartial = IPEPartialRdMultiplication * FPS;
            }

            pBandwidth->partialBW = readBandwidthPartial;

            if (NULL != pImageFormat)
            {
                if ((TRUE == ImageFormatUtils::Is10BitFormat(pImageFormat->format)))
                {
                    bppSrc = IPEBpp10Bit;
                }

                UBWCEnable = ImageFormatUtils::IsUBWC(pImageFormat->format);
            }

            break;
        }
    }

    if (FALSE == CheckIsIPERealtime())
    {
        // Pass0_RdAB = (((src_ImgW/64/2 * src_ImgH/64/2) * 8 * jpegOvhd ) * fps
        readBandwidthPass0 = static_cast<UINT64>(
                             (((srcWidth / 64.0 / 2) * (srcHeight / 64.0 / 2)) * 8 * IPESnapshotOverhead) * FPS);

        // Pass1_RdAB = ((src_ImgW/16/2 * src_ImgH/16/2 * 8 * jpegOvhd ) +
        // ((src_ImgW/64/2 * src_ImgH/64/2 * 102)/8 * jpegOvhd) ) * fps
        readBandwidthPass1 = static_cast<UINT64>(
                             (((srcWidth / 16.0 / 2) * (srcHeight / 16.0 / 2)) * 8 * IPESnapshotOverhead) +
                             ((((((srcWidth / 64.0 / 2) * (srcHeight / 64.0 / 2)) * 102) / 8.0) * IPESnapshotOverhead) * FPS));

        // Pass2_RdAB = ((src_ImgW/4/2 * src_ImgH/4/2 * 8 * jpegOvhd ) +
        // ((src_ImgW/16/2 * src_ImgH/16/2 * 102)/8 * jpegOvhd) ) * fps
        readBandwidthPass2 = static_cast<UINT64>(
                             (((srcWidth / 4.0 / 2) * (srcHeight / 4.0 / 2)) * 8 * IPESnapshotOverhead) +
                             ((((((srcWidth / 16.0 / 2) * (srcHeight / 16.0 / 2)) * 102) / 8.0) * IPESnapshotOverhead)* FPS));

        // Pass3_RdAB = ((src_ImgW  *  src_ImgH  *   jpegRdBPP * Ovhd/IPE_UBWC_RdCr )  +
        // ((src_ImgW/4/2 * src_imgH/4/2 * 102)/8) ) * fps
        readBandwidthPass3 = static_cast<UINT64>(
                             ((srcWidth * srcHeight) * IPESnapshotRdBPP10bit * IPESnapshotOverhead / IPEUbwcRdCr) +
                             ((((((srcWidth / 4.0 / 2) * (srcHeight / 4.0 / 2)) * 102) / 8.0)) * FPS));

        // IPE_RdAB_Frame   =  (Pass0_RdAB (DS64) + Pass1_RdAB (DS16) + Pass2_RdAB (DS4) + Pass3_RdAB (1:1)) * SW_Margin
        pBandwidth->readBW.unCompressedBW = static_cast<UINT64>((readBandwidthPass0 +
                                                                 readBandwidthPass1 +
                                                                 readBandwidthPass2 +
                                                                 readBandwidthPass3) * swMargin);

        CAMX_LOG_VERBOSE(CamxLogGroupPower,
                         "Snapshot bw: sw = %d sh = %d FPS = %d Pass0: %d Pass1:%llu Pass2: %llu Pass3: %llu BW = %llu",
                         srcWidth, srcHeight, FPS,
                         readBandwidthPass0, readBandwidthPass1, readBandwidthPass2, readBandwidthPass3,
                         pBandwidth->readBW.unCompressedBW);

        if (TRUE == UBWCEnable)
        {
            IPEUbwcRdCr  = IPEUBWCvr2RdCompressionRatio10Bit; // As BPS output is always 10bit

            readBandwidthPass3 = static_cast<UINT64>(
                                 ((srcWidth * srcHeight) * IPESnapshotRdBPP10bit * IPESnapshotOverhead / IPEUbwcRdCr) +
                                 ((((((srcWidth / 4.0 / 2) * (srcHeight / 4.0 / 2)) * 102) / 8.0)) * FPS));

            pBandwidth->readBW.compressedBW = static_cast<UINT64>((readBandwidthPass0 +
                                                                   readBandwidthPass1 +
                                                                   readBandwidthPass2 +
                                                                   readBandwidthPass3) * swMargin);

            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                             "Snapshot cbw: sw = %d sh = %d IPEUbwcRdCr = %f FPS = %d "
                             "Pass0: %llu Pass1:%llu Pass2:%llu Pass3:%llu cbw = %llu",
                             srcWidth, srcHeight, IPEUbwcRdCr, FPS,
                             readBandwidthPass0, readBandwidthPass1, readBandwidthPass2, readBandwidthPass3,
                             pBandwidth->readBW.compressedBW);
        }
        else
        {
            pBandwidth->writeBW.compressedBW = pBandwidth->writeBW.unCompressedBW;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupPower, "Snapshot Rd: cbw = %llu bw = %llu",
                         pBandwidth->readBW.compressedBW, pBandwidth->readBW.unCompressedBW);
    }
    else
    {
        // Calculate Uncompressed Bandwidth
        // Pass0_RdAB(DS16) = ( ((src_Img_w/DS16/N_PDT * src_Img_H/DS16/N_PDT) * PD_TS * Overhead * NS  ) * fps
        readBandwidthPass0 = static_cast<UINT64>((((srcWidth / 16.0 / 2) * (srcHeight / 16.0 / 2)) * 8 * overhead * 2) * FPS);

        // Pass1_RdAB(DS4) = ( ((src_Img_w/DS4/N_PDT * src_Img_H/DS4/N_PDT) * PD_TS * Overhead * NS  ) +
        //                  ((src_Img_W/DS16/N_PDT * src_Img_H/DS16/N_PDT * PDI_bits)/8 * Oveahead) +
        //                  ((src_Img_W/DS16 * src_Img_H/DS16 * TFI_bits)/8  * Overhead)   ) * fps
        readBandwidthPass1 = static_cast<UINT64>(
                             (((srcWidth / 4.0 / 2) * (srcHeight / 4.0 / 2)) * 8 * overhead * 2) +
                             (((((srcWidth / 16.0 / 2) * (srcHeight / 16.0 / 2)) * 102) / 8.0) * overhead) +
                             (((((srcWidth/16.0) * (srcHeight/16.0)) * 4) / 8.0) * overhead)
                             ) * FPS;

        // Pass2_RdAB(1:1) = ( (src_Img_W * src_Img_H * Bytes_per_pix * Overhead /  UBWC_Comp * fmt)  +
        //                  ((src_Img_W/DS4/N_PDT * src_img_H/DS4/N_PDT * PDI_bits)/8 * Overhead) +
        //                  ((src_ImgW/DS4 * src_imgH/DS4 * TFI_bits)/8) *Ovearhead ) * fps
        readBandwidthPass2 = static_cast<UINT64>(
            ((srcWidth * srcHeight * bppSrc * overhead * EISOverhead) / IPEUbwcRdCr) +
            ((((srcWidth/4.0/2) * (srcHeight/4.0/2)) * 102) / 8.0) +
            ((((srcWidth/4.0) * (srcHeight/4.0)) * 4) / 8.0));

        if (TRUE == IsLoopBackPortEnabled())
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPower, "Including MCTF BW");
            readBandwidthPass2 += static_cast<UINT64>((srcWidth * srcHeight * bppSrc * overhead) / IPEUbwcMCTFr);
        }

        readBandwidthPass2 *= FPS;

        pBandwidth->readBW.unCompressedBW = static_cast<UINT64>(
            (readBandwidthPass0 + readBandwidthPass1 + readBandwidthPass2 + readBandwidthPartial)* swMargin);

        CAMX_LOG_VERBOSE(CamxLogGroupPower,
            "Preview/Video bw: sw = %d sh = %d Pass0:%llu Pass1:%llu Pass2:%llu pr: %llu BW = %llu",
            srcWidth, srcHeight, readBandwidthPass0, readBandwidthPass1, readBandwidthPass2, readBandwidthPartial,
            pBandwidth->readBW.unCompressedBW);

        // Calculate Compressed Bandwidth
        if (TRUE == UBWCEnable)
        {
            if (IPEBpp10Bit == bppSrc)
            {
                if (UBWCver2 == UBWCVersion)
                {
                    IPEUbwcRdCr  = IPEUBWCvr2RdCompressionRatio10Bit;
                    IPEUbwcMCTFr = IPEUBWCvr2MctfReadCompressionRatio10Bit;
                }
                else if (UBWCver3 == UBWCVersion)
                {
                    IPEUbwcRdCr  = IPEUBWCvr3RdCompressionRatio10Bit;
                    IPEUbwcMCTFr = IPEUBWCvr3MctfReadCompressionRatio10Bit;

                    // Different compression ratio for reolution => UHD
                    if (IsUHDResolution(srcWidth, srcHeight))
                    {
                        IPEUbwcRdCr  = IPEUBWCvr3RdCompressionRatio10BitUHD;
                        IPEUbwcMCTFr = IPEUBWCvr3MctfReadCompressionRatio10BitUHD;
                    }
                }
            }
            else
            {
                if (UBWCver2 == UBWCVersion)
                {
                    IPEUbwcRdCr  = IPEUBWCvr2RdCompressionRatio8Bit;
                    IPEUbwcMCTFr = IPEUBWCvr2MctfReadCompressionRatio8Bit;
                }
                else if (UBWCver3 == UBWCVersion)
                {
                    IPEUbwcRdCr  = IPEUBWCvr3RdCompressionRatio8Bit;
                    IPEUbwcMCTFr = IPEUBWCvr3MctfReadCompressionRatio8Bit;

                    // Different compression ratio for reolution => UHD
                    if (IsUHDResolution(srcWidth, srcHeight))
                    {
                        IPEUbwcRdCr  = IPEUBWCvr3RdCompressionRatio8BitUHD;
                        IPEUbwcMCTFr = IPEUBWCvr3MctfReadCompressionRatio8BitUHD;
                    }
                }
            }
            readBandwidthPass2 = static_cast<UINT64>(
                ((srcWidth * srcHeight * bppSrc * overhead * EISOverhead) / IPEUbwcRdCr) +
                ((((srcWidth/4.0/2) * (srcHeight/4.0/2)) * 102) / 8.0) +
                ((((srcWidth/4.0) * (srcHeight/4.0)) * 4) / 8.0));

            if (TRUE == IsLoopBackPortEnabled())
            {
                readBandwidthPass2 += static_cast<UINT64>((srcWidth * srcHeight * bppSrc * overhead) / IPEUbwcMCTFr);
            }

            readBandwidthPass2 *= FPS;

            pBandwidth->readBW.compressedBW = static_cast<UINT64>(
                (readBandwidthPass0 + readBandwidthPass1 + readBandwidthPass2 + readBandwidthPartial) * swMargin);

            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                "bppSrc %f overhead %f IPEUbwcRdCr %f IPEUbwcMCTFr %f swMargin %d UBWCEnable %d FPS %d",
                bppSrc, overhead, IPEUbwcRdCr, IPEUbwcMCTFr, swMargin, UBWCEnable, FPS);
            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                "Preview/Video cbw: sw = %d sh = %d Pass0:%llu Pass1:%llu Pass2:%llu pr: %llu cbw = %llu",
                srcWidth, srcHeight, readBandwidthPass0, readBandwidthPass1, readBandwidthPass2, readBandwidthPartial,
                pBandwidth->readBW.compressedBW);
        }
        else
        {
            pBandwidth->readBW.compressedBW = pBandwidth->readBW.unCompressedBW;
        }
        CAMX_LOG_VERBOSE(CamxLogGroupPower, "Preview/Video Rd: cbw = %llu bw = %llu", pBandwidth->readBW.compressedBW,
            pBandwidth->readBW.unCompressedBW);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::CalculateIPEWrBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::CalculateIPEWrBandwidth(
    PerRequestActivePorts*  pPerRequestPorts,
    IPEBandwidth*           pBandwidth)
{
    IPEImageInfo        source;
    IPEImageInfo        video;
    IPEImageInfo        preview;
    BOOL                video_enable;
    BOOL                previewEnable;
    DOUBLE              swMargin              = IPESwMargin;
    FLOAT               IPEUbwcPreviewCr      = IPEUBWCWrPreviewCompressionRatio;
    FLOAT               IPEUbwcVideoCr        = IPEUBWCWrVideoCompressionRatio;
    DOUBLE              IPEUbwcMCTFCr         = IPEUBWCWrMctfCompressionRatio;
    UINT                FPS                   = pBandwidth->FPS;
    UINT64              writeBandwidthPass0;
    UINT64              writeBandwidthPass1;
    UINT64              writeBandwidthPass2;
    UINT64              writeBandwidthPass3;
    UINT64              writeBandwidthPartial;
    UINT                numBuffers;

    pBandwidth->writeBW.unCompressedBW = 0;
    pBandwidth->writeBW.compressedBW   = 0;

    source.width                              = 0;
    source.height                             = 0;
    source.bpp                                = IPEBpp8Bit;
    source.UBWCEnable                         = FALSE;

    // Check UBWC and BPP Info for Input Ports and get Dimesions
    for (UINT i = 0; i < pPerRequestPorts->numInputPorts; i++)
    {
        PerRequestInputPortInfo* pInputPort = &pPerRequestPorts->pInputPorts[i];
        if (pInputPort->portId == CSLIPEInputPortIdFull)
        {
            const ImageFormat* pImageFormat = GetInputPortImageFormat(InputPortIndex(pInputPort->portId));
            GetSizeWithoutStablizationMargin(m_fullInputWidth, m_fullInputHeight, &source.width, &source.height);

            if (NULL != pImageFormat)
            {
                if ((TRUE == ImageFormatUtils::Is10BitFormat(pImageFormat->format)))
                {
                    source.bpp                  = IPEBpp10Bit;
                }
                source.UBWCEnable               = ImageFormatUtils::IsUBWC(pImageFormat->format);
                source.UBWCVersion              = ImageFormatUtils::GetUBWCVersion();
            }
            break;
        }
    }
    if (FALSE == CheckIsIPERealtime())
    {
        // Pass0_WrAB = ( ((src_ImgW/64/2 * src_ImgH/64/2 * 102)/8) ) * fps

        writeBandwidthPass0 = static_cast<UINT64>(((((source.width/64.0/2) * (source.height/64.0/2)) * 102) / 8.0) * FPS);

        // Pass1_WrAB = (((src_ImgW/16/2 * src_ImgH/16/2 * 102)/8) )  * fps

        writeBandwidthPass1 = static_cast<UINT64>(((((source.width/16.0/2) * (source.height/16.0/2)) * 102) / 8.0) * FPS);

        // Pass2_WrAB = ( ((src_ImgW/4/2 * src_ImgH/4/2 * 102)/8) )  *  fps

        writeBandwidthPass2 = static_cast<UINT64>(((((source.width/4.0/2) * (source.height/4.0/2)) * 102) / 8.0) * FPS);

        // Pass3_WrAB = ((src_ImgW  *  src_ImgH     *  jpegWrBPP) )  * fps

        writeBandwidthPass3 = static_cast<UINT64>((((source.width) * (source.height)) * IPESnapshotWrBPP8bit) * FPS);

        // IPE_WrAB_Frame   =  (Pass0_WrAB (DS64) + Pass1_WrAB (DS16) + Pass2_WrAB (DS4) + Pass3_WrAB (1:1))*SW_Margin

        pBandwidth->writeBW.unCompressedBW = static_cast<UINT64>(
            (writeBandwidthPass0 + writeBandwidthPass1 + writeBandwidthPass2 + writeBandwidthPass3) * swMargin);

        pBandwidth->writeBW.compressedBW = pBandwidth->writeBW.unCompressedBW;


        CAMX_LOG_VERBOSE(CamxLogGroupPower, "Snapshot bw: pass0:%llu pass1:%llu pass2:%llu pass3 = %llu BW = %llu",
                writeBandwidthPass0, writeBandwidthPass1, writeBandwidthPass2, writeBandwidthPass2,
                pBandwidth->writeBW.compressedBW);
    }
    else
    {
        video_enable                              = FALSE;
        video.width                               = 0;
        video.height                              = 0;
        video.bpp                                 = IPEBpp8Bit;
        video.UBWCEnable                          = FALSE;

        previewEnable                             = FALSE;
        preview.width                             = 0;
        preview.height                            = 0;
        preview.bpp                               = IPEBpp8Bit;
        preview.UBWCEnable                        = FALSE;
        // Check UBWC and BPP Info for Output Ports and get Dimensions
        for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
        {
            PerRequestOutputPortInfo* pOutputPort   = &pPerRequestPorts->pOutputPorts[i];
            numBuffers                              = pOutputPort->numOutputBuffers;
            if (IPEOutputPortVideo == pOutputPort->portId)
            {
                const ImageFormat* pImageFormatVideo = GetOutputPortImageFormat(OutputPortIndex(pOutputPort->portId));
                video.width                          = pImageFormatVideo->width;
                video.height                         = pImageFormatVideo->height;
                video_enable                         = TRUE;
                if (TRUE == ImageFormatUtils::Is10BitFormat(pImageFormatVideo->format))
                {
                    video.bpp      = IPEBpp10Bit;
                }
                video.UBWCEnable   = ImageFormatUtils::IsUBWC(pImageFormatVideo->format);
                video.UBWCVersion =  GetUBWCVersion(pOutputPort->portId);
            }
            if (IPEOutputPortDisplay == pOutputPort->portId)
            {
                const ImageFormat* pImageFormatPreview = GetOutputPortImageFormat(OutputPortIndex(pOutputPort->portId));
                preview.width                          = pImageFormatPreview->width;
                preview.height                         = pImageFormatPreview->height;
                previewEnable                          = TRUE;
                if (TRUE == ImageFormatUtils::Is10BitFormat(pImageFormatPreview->format))
                {
                    preview.bpp      = IPEBpp10Bit;
                }
                preview.UBWCEnable     = ImageFormatUtils::IsUBWC(pImageFormatPreview->format);
                preview.UBWCVersion    = GetUBWCVersion(pOutputPort->portId);
            }
        }

        // Calculate uncompressed bandwidth
        // Pass0_WrAB(DS16) = ( ((src_Img_W/DS16/N_PDT * src_Img_H/DS16/N_PDT * PDI_bits)/8) +
        //                       ((src_Img_W/DS16 * src_Img_H/DS16 * TFI_bits)/8 ) ) * FPS
        writeBandwidthPass0 = static_cast<UINT64>(
            ((((source.width/16.0/2) * (source.height/16.0/2)) * 102) / 8.0) +
            ((((source.width/16.0) * (source.height/16.0)) * 4) / 8.0)) * FPS;

        // Pass1_WrAB = ( (src_Img_W/DS16/N_PDT  *  src_Img_H/DS16/N_PDT* PD_TS)
        //                ((src_Img_W/DS4/N_PDT * src_Img_H/DS4/N_PDT * PDI_bits)/8)  +
        //                ((src_Img_W/DS4 * src_Img_H/DS4 * TFI_bits)/8)   )  *  FPS
        writeBandwidthPass1 = static_cast<UINT64>(
            (((source.width/16.0/2) * (source.height/16.0/2)) * 8) +
            ((((source.width/4.0/2) * (source.height/4.0/2)) * 102) / 8.0) +
            ((((source.width/4.0) * (source.height/4.0)) * 4) / 8.0)) * FPS;

        // Pass2_WrAB = ((src_Img_W  *  src_Img_H  *  Bytes_per_pix  *  UBWC_CompMCTF )   +
        //               (vid_Img_W  *  vid_img_H  *  Bytes_per_pix  /  UBWC_CompVideo )  *  vid_enable  +
        //               (prev_Img_W  *  pre_img_H  *  Bytes_per_pix  / UBWC_CompPrev )  * prev_enable  +
        //               (src_Img_W/DS4/N_PDT  *  src_Img_H/DS4/N_PDT * PD_TS)  )    *  FPS
        writeBandwidthPass2 = static_cast<UINT64>(
            (((video.width * video.height * video.bpp) / IPEUbwcVideoCr) * video_enable) +
            (((preview.width * preview.height * preview.bpp) / IPEUbwcPreviewCr) * previewEnable) +
            (((source.width/4.0/2)  * (source.height/4.0/2)) * 8));

        if (TRUE == IsLoopBackPortEnabled())
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPower, "Including MCTF BW");
            writeBandwidthPass2 += static_cast<UINT64>((source.width * source.height * source.bpp) / IPEUbwcMCTFCr);
        }

        writeBandwidthPass2 *= FPS;

        writeBandwidthPartial = static_cast<UINT64>(pBandwidth->partialBW);

        pBandwidth->writeBW.unCompressedBW = static_cast<UINT64>(
            (writeBandwidthPass0 + writeBandwidthPass1 + writeBandwidthPass2 + writeBandwidthPartial) * swMargin);

        CAMX_LOG_VERBOSE(CamxLogGroupPower, "Preview/Video bw: srcw=%u srch=%u vidw=%u vidh=%u prevw=%u prevh=%u",
            source.width, source.height, video.width, video.height, preview.width, preview.height);

        CAMX_LOG_VERBOSE(CamxLogGroupPower, "Preview/Video bw  pass0:%llu pass1:%llu pass2:%llu pw = %llu uBW = %llu",
            writeBandwidthPass0, writeBandwidthPass1, writeBandwidthPass2, writeBandwidthPartial,
            pBandwidth->writeBW.unCompressedBW);

        // Calculate Compressed bandwidth
        if ((TRUE == preview.UBWCEnable) || (TRUE == video.UBWCEnable) || (TRUE == source.UBWCEnable))
        {
            if (TRUE == preview.UBWCEnable)
            {
                if (IPEBpp10Bit == preview.bpp)
                {
                    IPEUbwcPreviewCr = IPEUBWCvr2WrPreviewCompressionRatio10Bit;
                }
                else
                {
                    IPEUbwcPreviewCr = IPEUBWCvr2WrPreviewCompressionRatio8Bit;
                }
            }
            if (TRUE == video.UBWCEnable)
            {
                if (IPEBpp10Bit == video.bpp)
                {
                    IPEUbwcVideoCr = IPEUBWCvr2WrVideoCompressionRatio10Bit;
                }
                else
                {
                    IPEUbwcVideoCr = IPEUBWCvr2WrVideoCompressionRatio8Bit;
                }
            }
            if (TRUE == source.UBWCEnable)
            {
                if (UBWCver3 == source.UBWCVersion)
                {
                    if (IsUHDResolution(source.width, source.height))
                    {
                        if (IPEBpp10Bit == source.bpp)
                        {
                            IPEUbwcMCTFCr = IPEUBWCvr3WrMctfCompressionRatio10BitUHD;
                        }
                        else
                        {
                            IPEUbwcMCTFCr = IPEUBWCvr3WrMctfCompressionRatio8BitUHD;
                        }
                    }
                    else
                    {
                        if (IPEBpp10Bit == source.bpp)
                        {
                            IPEUbwcMCTFCr = IPEUBWCvr3WrMctfCompressionRatio10Bit;
                        }
                        else
                        {
                            IPEUbwcMCTFCr = IPEUBWCvr3WrMctfCompressionRatio8Bit;
                        }
                    }
                }
                else
                {
                    if (IPEBpp10Bit == source.bpp)
                    {
                        IPEUbwcMCTFCr = IPEUBWCvr2WrMctfCompressionRatio10Bit;
                    }
                    else
                    {
                        IPEUbwcMCTFCr = IPEUBWCvr2WrMctfCompressionRatio8Bit;
                    }
                }
            }

            writeBandwidthPass2 = static_cast<UINT64>(
                (((video.width * video.height * video.bpp) / IPEUbwcVideoCr) * video_enable) +
                (((preview.width * preview.height * preview.bpp) / IPEUbwcPreviewCr) * previewEnable) +
                (((source.width/4.0/2)  * (source.height/4.0/2)) * 8));

            if (TRUE == IsLoopBackPortEnabled())
            {
                writeBandwidthPass2 += static_cast<UINT64>((source.width * source.height * source.bpp) / IPEUbwcMCTFCr);
            }

            writeBandwidthPass2 *= FPS;

            pBandwidth->writeBW.compressedBW = static_cast<UINT64>(
                (writeBandwidthPass0 + writeBandwidthPass1 + writeBandwidthPass2 + writeBandwidthPartial) * swMargin);

            CAMX_LOG_VERBOSE(CamxLogGroupPower,
                "IPEUbwcPreviewCr %f IPEUbwcVideoCr %f IPEUbwcMCTFCr %f prev.bpp %f video.bpp %f source.bpp %f",
                IPEUbwcPreviewCr, IPEUbwcVideoCr, IPEUbwcMCTFCr, preview.bpp, video.bpp, source.bpp);

            CAMX_LOG_VERBOSE(CamxLogGroupPower, "Preview/Video cbw: srcw=%u srch=%u vidw = %u vidh = %u prevw = %u prevh = %u",
                source.width, source.height, video.width, video.height, preview.width, preview.height);

            CAMX_LOG_VERBOSE(CamxLogGroupPower, "Preview/Video cbw: pass0:%llu pass1:%llu pass2:%llu pw = %llu cBW = %llu",
                writeBandwidthPass0, writeBandwidthPass1, writeBandwidthPass2, writeBandwidthPartial,
                pBandwidth->writeBW.compressedBW);
        }
        else
        {
            pBandwidth->writeBW.compressedBW = pBandwidth->writeBW.unCompressedBW;
        }
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPower, "Wr: cbw = %llu bw = %llu", pBandwidth->writeBW.compressedBW,
            pBandwidth->writeBW.unCompressedBW);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::UpdateBandwidth
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::UpdateBandwidth(
    ExecuteProcessRequestData*   pExecuteProcessRequestData,
    CSLICPClockBandwidthRequest* pICPClockBandwidthRequest)
{
    PerRequestActivePorts*  pPerRequestPorts   = pExecuteProcessRequestData->pEnabledPortsInfo;
    NodeProcessRequestData* pNodeRequestData   = pExecuteProcessRequestData->pNodeProcessRequestData;
    UINT64                  requestId          = pNodeRequestData->pCaptureRequest->requestId;
    struct IPEBandwidth     bandwidth;
    UINT                    FPS                = DefaultFPS;
    INT64                   requestDelta       = requestId - m_currentrequestID;

    if (0 != m_FPS)
    {
        FPS = m_FPS;
    }

    if (requestDelta > 1)
    {
        FPS = FPS / requestDelta;
    }

    bandwidth.FPS = FPS;
    CalculateIPERdBandwidth(pPerRequestPorts, &bandwidth);
    CalculateIPEWrBandwidth(pPerRequestPorts, &bandwidth);

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
// IPENode::CheckAndUpdateClockBW
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::CheckAndUpdateClockBW(
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
// IPENode::SendFWCmdRegionInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::SendFWCmdRegionInfo(
    UINT32                opcode,
    CSLBufferInfo** const ppBufferInfo,
    UINT32                numberOfMappings)
{
    CamxResult            result          = CamxResultSuccess;
    Packet*               pPacket         = NULL;
    CmdBuffer*            pCmdBuffer      = NULL;
    CSLICPMemoryMapUpdate memoryMapUpdate = { 0 };
    CSLBufferInfo*        pBufferInfo     = NULL;

    pPacket = GetPacket(m_pIQPacketManager);

    if ((NULL != pPacket) && (NULL != ppBufferInfo) && (numberOfMappings <= CSLICPMaxMemoryMapRegions))
    {
        pPacket->SetOpcode(CSLDeviceType::CSLDeviceTypeICP, CSLPacketOpcodesIPEMemoryMapUpdate);
        pCmdBuffer = GetCmdBuffer(m_pIPECmdBufferManager[CmdBufferGenericBlob]);
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
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Send Fw region info failed");
    }

    if (NULL != pCmdBuffer)
    {
        m_pIPECmdBufferManager[CmdBufferGenericBlob]->Recycle(reinterpret_cast<PacketResource*>(pCmdBuffer));
    }
    if (NULL != pPacket)
    {
        m_pIQPacketManager->Recycle(reinterpret_cast<PacketResource*>(pPacket));
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::ExecuteProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::ExecuteProcessRequest(
    ExecuteProcessRequestData* pExecuteProcessRequestData)
{
    CAMX_ASSERT(NULL != pExecuteProcessRequestData);
    CAMX_ASSERT(NULL != pExecuteProcessRequestData->pNodeProcessRequestData);
    CAMX_ASSERT(NULL != pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest);
    CAMX_ASSERT(NULL != pExecuteProcessRequestData->pTuningModeData);

    CamxResult  result    = CamxResultSuccess;
    Packet*     pIQPacket = NULL;
    CmdBuffer*  pIPECmdBuffer[IPECmdBufferMaxIds] = { NULL };

    AECFrameControl         AECUpdateData    = {};
    AWBFrameControl         AWBUpdateData    = {};
    AECStatsControl         AECStatsUpdate   = {};
    AWBStatsControl         AWBStatsUpdate   = {};
    AFStatsControl          AFStatsUpdate    = {};
    ISPInputData            moduleInput      = {};
    NodeProcessRequestData* pNodeRequestData = pExecuteProcessRequestData->pNodeProcessRequestData;
    UINT64                  requestId        = pNodeRequestData->pCaptureRequest->requestId;
    PerRequestActivePorts*  pPerRequestPorts = pExecuteProcessRequestData->pEnabledPortsInfo;
    BOOL                    useDependencies  = GetHwContext()->GetStaticSettings()->enableIPEDependencies;
    UINT                    parentNodeID     = 0;
    INT                     sequenceNumber   = 0;
    UINT32                  cameraId         = 0;
    IPETuningMetadata*      pTuningMetadata  = NULL;
    BOOL                    isMasterCamera   = TRUE;
    BOOL                    isPendingBuffer  = FALSE;
    BOOL                    requestUBWCMeta  = FALSE;
    IpeFrameProcess*        pFrameProcess;
    IpeFrameProcessData*    pFrameProcessData;
    IpeIQSettings*          pIPEIQsettings;

    // Initialize ICA parameters
    moduleInput.ICAConfigData.ICAInGridParams.gridTransformEnable                  = 0;
    moduleInput.ICAConfigData.ICAInInterpolationParams.customInterpolationEnabled  = 0;
    moduleInput.ICAConfigData.ICAInPerspectiveParams.perspectiveTransformEnable    = 0;
    moduleInput.ICAConfigData.ICARefGridParams.gridTransformEnable                 = 0;
    moduleInput.ICAConfigData.ICARefPerspectiveParams.perspectiveTransformEnable   = 0;
    moduleInput.ICAConfigData.ICARefInterpolationParams.customInterpolationEnabled = 0;
    moduleInput.ICAConfigData.ICAReferenceParams.perspectiveTransformEnable        = 0;
    moduleInput.registerBETEn                                                      = FALSE;
    moduleInput.frameNum                                                           = requestId;

    // PublishICADependencies(pNodeRequestData);

    for (UINT i = 0; i < pPerRequestPorts->numInputPorts; i++)
    {
        PerRequestInputPortInfo* pInputPort = &pPerRequestPorts->pInputPorts[i];

        if ((NULL != pInputPort) && (TRUE == pInputPort->flags.isPendingBuffer))
        {
            isPendingBuffer = TRUE;
        }
        if (NULL != pInputPort && IPEInputPortFull == pInputPort->portId)
        {
            parentNodeID = GetParentNodeType(pInputPort->portId);
        }
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Req ID %llu, sequenceId %d", requestId, pNodeRequestData->processSequenceId);

    if (TRUE == isPendingBuffer)
    {
        sequenceNumber = pNodeRequestData->processSequenceId;
    }
    else
    {
        sequenceNumber = 1;
    }

    if (TRUE == useDependencies)
    {
        sequenceNumber = pNodeRequestData->processSequenceId;
    }

    if (0 == sequenceNumber)
    {
        if (TRUE == useDependencies)
        {
            SetDependencies(pExecuteProcessRequestData, parentNodeID);

            // Need to determine if this IPE instance is for EISv3
            // If yes, then we need to publish metadata early to not delay preview
            if ((0 != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType)) ||
                (0 != (IPEStabilizationType::IPEStabilizationTypeSWEIS3 & m_instanceProperty.stabilizationType)))
            {
                ProcessPartialMetadataDone(requestId);
                ProcessMetadataDone(requestId);
            }
        }
    }

    if (1 == sequenceNumber)
    {
        UINT32 numberOfCamerasRunning;
        UINT32 currentCameraId;
        BOOL   isMultiCameraUsecase;

        cameraId = GetPipeline()->GetCameraId();

        GetMultiCameraInfo(&isMultiCameraUsecase, &numberOfCamerasRunning, &currentCameraId, &isMasterCamera);

        if (TRUE == isMultiCameraUsecase)
        {
            cameraId = currentCameraId;
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "instance=%p, cameraId = %d, isMasterCamera = %d",
                             this, cameraId, isMasterCamera);
        }

        // Assign debug-data
        pTuningMetadata = (TRUE == isMasterCamera) ? m_pTuningMetadata : NULL;

        moduleInput.pTuningDataManager         = GetTuningDataManagerWithCameraId(cameraId);
        moduleInput.pHwContext                 = GetHwContext();
        moduleInput.pipelineIPEData.instanceID = InstanceID();
        moduleInput.titanVersion               = static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion();

        UpdateICADependencies(&moduleInput);

        // Get CmdBuffer for request
        CAMX_ASSERT(NULL != m_pIQPacketManager);
        CAMX_ASSERT(NULL != m_pIPECmdBufferManager[CmdBufferFrameProcess]);
        CAMX_ASSERT(NULL != m_pIPECmdBufferManager[CmdBufferIQSettings]);

        pIQPacket                            = GetPacketForRequest(requestId, m_pIQPacketManager);
        pIPECmdBuffer[CmdBufferFrameProcess] =
            GetCmdBufferForRequest(requestId, m_pIPECmdBufferManager[CmdBufferFrameProcess]);
        pIPECmdBuffer[CmdBufferIQSettings]   =
            GetCmdBufferForRequest(requestId, m_pIPECmdBufferManager[CmdBufferIQSettings]);
        pIPECmdBuffer[CmdBufferGenericBlob]  =
            GetCmdBufferForRequest(requestId, m_pIPECmdBufferManager[CmdBufferGenericBlob]);
        CSLICPClockBandwidthRequest        ICPClockBandwidthRequest;

        if (NULL != m_pIPECmdBufferManager[CmdBufferPreLTM])
        {
            pIPECmdBuffer[CmdBufferPreLTM] = GetCmdBufferForRequest(requestId, m_pIPECmdBufferManager[CmdBufferPreLTM]);
        }

        if (NULL != m_pIPECmdBufferManager[CmdBufferPostLTM])
        {
            pIPECmdBuffer[CmdBufferPostLTM] = GetCmdBufferForRequest(requestId, m_pIPECmdBufferManager[CmdBufferPostLTM]);
        }

        if (NULL != m_pIPECmdBufferManager[CmdBufferDMIHeader])
        {
            pIPECmdBuffer[CmdBufferDMIHeader] = GetCmdBufferForRequest(requestId, m_pIPECmdBufferManager[CmdBufferDMIHeader]);
        }

        if (m_pIPECmdBufferManager[CmdBufferNPS] != NULL)
        {
            pIPECmdBuffer[CmdBufferNPS] = GetCmdBufferForRequest(requestId, m_pIPECmdBufferManager[CmdBufferNPS]);
        }

        // Check for mandatory buffers (even for bypass test)
        if ((NULL == pIQPacket) ||
            (NULL == pIPECmdBuffer[CmdBufferFrameProcess]) ||
            (NULL == pIPECmdBuffer[CmdBufferGenericBlob]) ||
            (NULL == pIPECmdBuffer[CmdBufferIQSettings]))
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc,
                "%s: Null IQPacket or CmdBuffer %x, %x, %x",
                __FUNCTION__,
                pIQPacket,
                pIPECmdBuffer[CmdBufferFrameProcess],
                pIPECmdBuffer[CmdBufferGenericBlob]);

            result = CamxResultENoMemory;
        }

        if (CamxResultSuccess == result)
        {
            pFrameProcess = reinterpret_cast<IpeFrameProcess*>(
                pIPECmdBuffer[CmdBufferFrameProcess]->BeginCommands(CmdBufferFrameProcessSizeBytes / 4));
            CAMX_ASSERT(NULL != pFrameProcess);

            pFrameProcess->userArg          = m_hDevice;
            pFrameProcessData               = &pFrameProcess->cmdData;
            pFrameProcessData->requestId    = static_cast<UINT32>(requestId);
            pIPEIQsettings                  =
                reinterpret_cast<IpeIQSettings*>(pIPECmdBuffer[CmdBufferIQSettings]->BeginCommands(sizeof(IpeIQSettings) / 4));
            Utils::Memset(&pFrameProcessData->frameSets[0], 0x0, sizeof(pFrameProcessData->frameSets));

            InitializeIPEIQSettings(pIPEIQsettings);

            // Setup the Input data for IQ Parameter
            moduleInput.pHwContext                                 = GetHwContext();
            moduleInput.pAECUpdateData                             = &AECUpdateData;
            moduleInput.pAWBUpdateData                             = &AWBUpdateData;
            moduleInput.pAECStatsUpdateData                        = &AECStatsUpdate;
            moduleInput.pAWBStatsUpdateData                        = &AWBStatsUpdate;
            moduleInput.pAFStatsUpdateData                         = &AFStatsUpdate;
            moduleInput.pIPETuningMetadata                         = pTuningMetadata;
            moduleInput.pipelineIPEData.pFrameProcessData          = pFrameProcessData;
            moduleInput.pipelineIPEData.pIPEIQSettings             = pIPEIQsettings;
            moduleInput.pipelineIPEData.ppIPECmdBuffer             = pIPECmdBuffer;
            moduleInput.pipelineIPEData.batchFrameNum              = pNodeRequestData->pCaptureRequest->numBatchedFrames;
            moduleInput.pipelineIPEData.numOutputRefPorts          = m_numOutputRefPorts;
            moduleInput.pipelineIPEData.realtimeFlag               = m_realTimeIPE;
            moduleInput.pHALTagsData                               = &m_HALTagsData;
            moduleInput.pipelineIPEData.instanceProperty           = m_instanceProperty;
            moduleInput.pipelineIPEData.inputDimension.widthPixels = m_fullInputWidth;
            moduleInput.pipelineIPEData.inputDimension.heightLines = m_fullInputHeight;
            moduleInput.pipelineIPEData.numPasses                  = m_numPasses;
            moduleInput.sensorID                                   = cameraId;
            moduleInput.pCalculatedData                            = &m_ISPData;
            moduleInput.opticalCenterX                             = m_fullInputWidth / 2;
            moduleInput.opticalCenterY                             = m_fullInputHeight / 2;
            moduleInput.fDData.numberOfFace                        = 0;
            moduleInput.pipelineIPEData.pWarpGeometryData          = NULL;
            moduleInput.pipelineIPEData.compressiononOutput        = m_compressiononOutput;
            Utils::Memset(moduleInput.fDData.faceCenterX, 0x0, sizeof(moduleInput.fDData.faceCenterX));
            Utils::Memset(moduleInput.fDData.faceCenterY, 0x0, sizeof(moduleInput.fDData.faceCenterY));
            Utils::Memset(moduleInput.fDData.faceRadius, 0x0, sizeof(moduleInput.fDData.faceRadius));
            moduleInput.pipelineIPEData.hasTFRefInput =
                IsIPEHasInputReferencePort((requestId - m_currentrequestID), m_firstValidRequest);

            moduleInput.maximumPipelineDelay = GetMaximumPipelineDelay();

            moduleInput.pipelineIPEData.isLowResolution = IsZSLNRLowResolution();

            // @note: need to set it here now before getting the data
            moduleInput.pipelineIPEData.upscalingFactorMFSR        = 1.0f;
            moduleInput.mfFrameNum                                 = 0;
            moduleInput.pipelineIPEData.numOfFrames                = 5;  // default number of frames in MFNR/MFSR case
            moduleInput.pipelineIPEData.isDigitalZoomEnabled       = 0;
            moduleInput.pipelineIPEData.digitalZoomStartX          = 0;
            // If MCTF is enabled with EIS
            moduleInput.pipelineIPEData.mctfEis                    = ((TRUE == IsMCTFWithEIS()) &&
                                                                      (TRUE == GetStaticSettings()->enableMCTF)) ? TRUE : FALSE;
            moduleInput.pipelineIPEData.configIOTopology           = GetIPEConfigIOTopology();

            if ((TRUE == GetStaticSettings()->offlineImageDumpOnly) && (TRUE != IsRealTime()))
            {
                CHAR  dumpFilename[256];
                FILE* pFile = NULL;
                CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
                pFile = CamX::OsUtils::FOpen(dumpFilename, "w");
                CamX::OsUtils::FPrintF(pFile, "******** IPE REGISTER DUMP FOR BET ********************* \n");
                CamX::OsUtils::FClose(pFile);
                moduleInput.dumpRegConfig                          = 0x00200000;
            }
            else
            {
                moduleInput.dumpRegConfig                          = GetStaticSettings()->logVerboseMask;
            }

            // Get HAL tags
            result = GetMetadataTags(&moduleInput);
            CAMX_ASSERT(CamxResultSuccess == result);

            if (TRUE == IsBlendWithNPS())
            {
                moduleInput.mfFrameNum = (requestId - 1) % (moduleInput.pipelineIPEData.numOfFrames - 2) + 1;
                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "MFNR/MFSR Blend Stage FrameNum = %d", moduleInput.mfFrameNum);
            }
            else if (TRUE == IsPostfilterWithNPS())
            {
                moduleInput.mfFrameNum = moduleInput.pipelineIPEData.numOfFrames - 1;
                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "MFNR/MFSR Postfilter Stage FrameNum = %d", moduleInput.mfFrameNum);
            }
            else if ((IPEProcessingType::IPEMFNRPrefilter == m_instanceProperty.processingType) ||
                     (IPEProcessingType::IPEMFSRPrefilter == m_instanceProperty.processingType))
            {
                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "MFNR/MFSR Prefilter Stage FrameNum = %d", moduleInput.mfFrameNum);
            }

            if (m_pPreTuningDataManager == moduleInput.pTuningDataManager)
            {
                moduleInput.tuningModeChanged = ISPIQModule::IsTuningModeDataChanged(
                    pExecuteProcessRequestData->pTuningModeData,
                    &m_tuningData);
            }
            else
            {
                moduleInput.tuningModeChanged = TRUE;
                m_pPreTuningDataManager = moduleInput.pTuningDataManager;
                CAMX_LOG_INFO(CamxLogGroupISP, "TuningDataManager pointer is updated");
            }

            // if camera ID changed, it should set tuningModeChanged TRUE to trigger all IQ
            // module to update tuning parameters
            if (m_cameraId != cameraId)
            {
                moduleInput.tuningModeChanged = TRUE;
                m_cameraId = cameraId;
            }

            // Needed to have different tuning data for different instances of a node within same pipeline
            //
            // Also, cache tuning mode selector data for comparison for next frame, to help
            // optimize tuning data (tree) search in the IQ modules.
            // And, force update the tuning mode if camera id is updated, IPE node in SAT preview offline pipeline,
            // the active camera will switch based on the zoom level, we also need to update tuning data even
            // tuning mode not changed.
            if (TRUE == moduleInput.tuningModeChanged)
            {
                Utils::Memcpy(&m_tuningData, pExecuteProcessRequestData->pTuningModeData, sizeof(ChiTuningModeParameter));

                if ((IPEProfileId::IPEProfileIdDefault    != m_instanceProperty.profileId)    &&
                    ((IPEProcessingType::IPEMFNRPostfilter == m_instanceProperty.processingType) ||
                     (IPEProcessingType::IPEMFSRPostfilter == m_instanceProperty.processingType)))
                {
                    m_tuningData.TuningMode[static_cast<UINT32>(ModeType::Feature2)].subMode.feature2 =
                        ChiModeFeature2SubModeType::MFNRBlend;
                }

                if (IPEProcessingType::IPEProcessingPreview == m_instanceProperty.processingType)
                {
                    // This node instnace is preview instance.. Overwrite the tuning param to preview
                    m_tuningData.TuningMode[static_cast<UINT32>(ModeType::Usecase)].subMode.usecase =
                        ChiModeUsecaseSubModeType::Preview;
                }
            }

            if (m_tuningData.TuningMode[static_cast<UINT32>(ModeType::Feature2)].subMode.feature2 ==
                ChiModeFeature2SubModeType::HDR10)
            {
                moduleInput.isHDR10On = TRUE;
            }
            else
            {
                moduleInput.isHDR10On = FALSE;
            }

            // Now refer to the updated tuning mode selector data
            moduleInput.pTuningData = &m_tuningData;

#if DEBUG
            DumpTuningModeData();
#endif // DumpTuningModeData

            if (TRUE == useDependencies)
            {
                SetAAAInputData(&moduleInput, parentNodeID);
            }
            else
            {
                HardcodeAAAInputData(&moduleInput, parentNodeID);
            }

            if (TRUE == m_OEMStatsSettingEnable)
            {
                GetOEMStatsConfig(&moduleInput, parentNodeID);
            }

            if (TRUE == m_OEMIQSettingEnable)
            {
                GetOEMIQConfig(&moduleInput, parentNodeID);
            }
        }

        if (CamxResultSuccess == result)
        {
            result = GetFaceROI(&moduleInput, parentNodeID);
            CAMX_ASSERT(CamxResultSuccess == result);
        }

        if (CamxResultSuccess == result)
        {
            GetGammaOutput(moduleInput.pCalculatedData, parentNodeID);
            result = GetIntermediateDimension();
        }

        if (CamxResultSuccess == result)
        {
            GetGammaPreCalculationOutput(moduleInput.pCalculatedData);
        }

        if ((CamxResultSuccess == result) && (FALSE == IsScalerOnlyIPE()))
        {
            GetADRCInfoOutput();
        }

        if (CamxResultSuccess == result)
        {
            result = FillIQSetting(&moduleInput, pIPEIQsettings, pPerRequestPorts);
            CAMX_ASSERT(CamxResultSuccess == result);
        }

        if (CamxResultSuccess == result)
        {
            for (UINT i = 0; i < pPerRequestPorts->numInputPorts; i++)
            {
                PerRequestInputPortInfo* pInputPort = &pPerRequestPorts->pInputPorts[i];

                CAMX_ASSERT(NULL != pInputPort->phFence);

                if ((NULL != pInputPort) && (NULL != pInputPort->pImageBuffer))
                {
                    result = pIQPacket->AddIOConfig(pInputPort->pImageBuffer,
                                                    pInputPort->portId,
                                                    CSLIODirection::CSLIODirectionInput,
                                                    pInputPort->phFence,
                                                    1,
                                                    NULL,
                                                    NULL,
                                                    0);
                    CAMX_ASSERT(CamxResultSuccess == result);

                    if (CamxResultSuccess == result)
                    {
                        CAMX_LOG_INFO(CamxLogGroupPProc,
                                      "node %s reporting Input config,portId=%d,imgBuf=0x%x,hFence=%d,request=%llu",
                                      NodeIdentifierString(),
                                      pInputPort->portId,
                                      pInputPort->pImageBuffer,
                                      *(pInputPort->phFence),
                                      pNodeRequestData->pCaptureRequest->requestId);
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to create IO config packet result = %d", result);
                    }

                    if (CamxResultSuccess == result)
                    {
                        result = FillInputFrameSetData(pIPECmdBuffer[CmdBufferFrameProcess],
                                                       pInputPort->portId,
                                                       pInputPort->pImageBuffer,
                                                       pNodeRequestData->pCaptureRequest->numBatchedFrames);
                        CAMX_ASSERT(CamxResultSuccess == result);
                    }

                    if ((CamxResultSuccess == result) &&
                        (IPEInputPortFull == pInputPort->portId))
                    {
                        parentNodeID = GetParentNodeType(pInputPort->portId);
                        const ImageFormat* pImageFormat = pInputPort->pImageBuffer->GetFormat();
                        if (NULL != pImageFormat)
                        {
                            moduleInput.pipelineIPEData.fullInputFormat = pImageFormat->format;
                        }
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "%s: Input Port/Image Buffer is Null ", __FUNCTION__);

                    result = CamxResultEInvalidArg;
                }

                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Input Port: Add IO config failed i %d", i);
                    break;
                }
            }
        }

        UINT32 metaTag = 0;
        INT32  disableZoomCrop = FALSE;

        if (CamxResultSuccess == result)
        {
            result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.ref.cropsize",
                                                              "DisableZoomCrop",
                                                              &metaTag);
        }

        if (CamxResultSuccess == result)
        {
            metaTag |= InputMetadataSectionMask;

            static const UINT32 PropertiesIPE[] =
            {
                metaTag
            };
            UINT length = CAMX_ARRAY_SIZE(PropertiesIPE);
            VOID* pData[1] = { 0 };
            UINT64 propertyDataIPEOffset[1] = { 0 };

            result = GetDataList(PropertiesIPE, pData, propertyDataIPEOffset, length);
            if ((CamxResultSuccess == result) &&
                (NULL != pData[0]))
            {
                disableZoomCrop = *reinterpret_cast<INT32*>(pData[0]);
            }
        }

        if ((CamxResultSuccess == result) && (FALSE == disableZoomCrop))
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE:%d %s: zoom operaton from IPE", InstanceID(), __FUNCTION__);
            result = FillFrameZoomWindow(&moduleInput, parentNodeID, pIPECmdBuffer[CmdBufferGenericBlob],
                                         pNodeRequestData->pCaptureRequest->requestId);
        }

        // update it one more time in case if m_numPasses is updated from FillFrameZoomWindow(...) in MFSR
        moduleInput.pipelineIPEData.numPasses = m_numPasses;

        if (CamxResultSuccess == result)
        {
            moduleInput.pAECUpdateData->exposureInfo[0].sensitivity
                = moduleInput.pAECUpdateData->exposureInfo[0].sensitivity * moduleInput.pAECUpdateData->predictiveGain;
            moduleInput.pAECUpdateData->exposureInfo[0].linearGain
                = moduleInput.pAECUpdateData->exposureInfo[0].linearGain * moduleInput.pAECUpdateData->predictiveGain;
            result = ProgramIQConfig(&moduleInput);
        }
        if (CamxResultSuccess == result)
        {
            pIQPacket->SetRequestId(GetCSLSyncId(requestId));
            pIQPacket->SetOpcode(CSLDeviceType::CSLDeviceTypeICP, CSLPacketOpcodesIPEUpdate);
        }
        if (CamxResultSuccess == result)
        {
            result = FillCDMProgramArrays(pFrameProcessData,
                         pIPEIQsettings,
                         pIPECmdBuffer,
                         pNodeRequestData->pCaptureRequest->numBatchedFrames);
        }

        if (CamxResultSuccess == result)
        {
            for (UINT portIndex = 0; portIndex < pPerRequestPorts->numOutputPorts; portIndex++)
            {
                PerRequestOutputPortInfo* pOutputPort  = &pPerRequestPorts->pOutputPorts[portIndex];
                ImageBuffer*              pImageBuffer = pOutputPort->ppImageBuffer[0];

                CAMX_ASSERT(NULL != pOutputPort);
                CAMX_ASSERT(NULL != pOutputPort->phFence);

                // Even though there are 4 buffers on same port, AddIOConfig shall be called only once.
                if (NULL != pImageBuffer)
                {
                    result = pIQPacket->AddIOConfig(pImageBuffer,
                                                    pOutputPort->portId,
                                                    CSLIODirection::CSLIODirectionOutput,
                                                    pOutputPort->phFence,
                                                    1,
                                                    NULL,
                                                    NULL,
                                                    0);
                    CAMX_ASSERT(CamxResultSuccess == result);
                }

                pFrameProcessData->numFrameSetsInBatch = pNodeRequestData->pCaptureRequest->numBatchedFrames;

                UINT32 numBuffers    = pOutputPort->numOutputBuffers;

                for (UINT bufferIndex = 0; bufferIndex < numBuffers; bufferIndex++)
                {
                    pImageBuffer = pOutputPort->ppImageBuffer[bufferIndex];

                    if (NULL != pImageBuffer)
                    {
                        if (CamxResultSuccess == result)
                        {
                            // IPE will always output to non batched image buffer
                            result = FillOutputFrameSetData(pIPECmdBuffer[CmdBufferFrameProcess],
                                         pOutputPort->portId,
                                         pImageBuffer,
                                         bufferIndex);
                        }

                        CAMX_LOG_INFO(CamxLogGroupPProc,
                                      "node %s reporting I/O config,portId=%d,imgBuf=0x%x,hFence=%d,request=%llu",
                                      NodeIdentifierString(),
                                      pOutputPort->portId,
                                      pImageBuffer,
                                      *(pOutputPort->phFence),
                                      pNodeRequestData->pCaptureRequest->requestId);

                        if ((CamxResultSuccess == result) &&
                            (TRUE == m_realTimeIPE)       &&
                            (TRUE != requestUBWCMeta)     &&
                            (TRUE == ImageFormatUtils::IsUBWC(pImageBuffer->GetFormat()->format)))
                        {
                            CSLBufferInfo  UBWCStatsBuf = { 0 };
                            if (CamxResultSuccess == GetUBWCStatBuffer(&UBWCStatsBuf, requestId))
                            {
                                // UBWC meta is obtained for all the outport with a single command buffer request
                                requestUBWCMeta = TRUE;
                                result          = FillFrameUBWCParams(pIPECmdBuffer[CmdBufferFrameProcess],
                                                      pFrameProcessData,
                                                      &UBWCStatsBuf,
                                                      pOutputPort,
                                                      portIndex);
                            }
                        }
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "%s: Output Port/Image is Null ", __FUNCTION__);
                        result = CamxResultEInvalidArg;
                    }

                    if (CamxResultSuccess != result)
                    {
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "Output Port: Add IO config failed");
                        break;
                    }
                }

                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Output Port: Add IO config failed");
                    break;
                }
            }
            if ((CamxResultSuccess == result) && (TRUE == IsLoopBackPortEnabled()))
            {
                result = FillLoopBackFrameBufferData(
                    pNodeRequestData->pCaptureRequest->requestId,
                    m_firstValidRequest,
                    pIPECmdBuffer[CmdBufferFrameProcess],
                    pNodeRequestData->pCaptureRequest->numBatchedFrames);
            }
            if (CamxResultSuccess == result)
            {
                /// @todo (CAMX-732) Get Scratch buffer from topology from loopback port
                if (NULL != m_pScratchMemoryBuffer[0])
                {
                    pFrameProcessData->scratchBufferSize = m_firmwareScratchMemSize;
                    // Patch scratch buffer: pFrameProcessData->scratchBufferAddress
                    UINT32 scratchBufferOffset =
                        static_cast <UINT32>(offsetof(IpeFrameProcessData, scratchBufferAddress));
                    result = pIPECmdBuffer[CmdBufferFrameProcess]->AddNestedBufferInfo(scratchBufferOffset,
                                                                                       m_pScratchMemoryBuffer[0]->hHandle,
                                                                                       0);
                }
                else
                {
                    pFrameProcessData->scratchBufferAddress = 0;
                    pFrameProcessData->scratchBufferSize = 0;
                }

                if (CamxResultSuccess == result)
                {
                    result = FillFramePerfParams(pFrameProcessData);
                }

                // Following code can only be called after FillFrameZoomWindow(...)
                if (NULL != m_pIPECmdBufferManager[CmdBufferStriping])
                {
                    pIPECmdBuffer[CmdBufferStriping] =
                        GetCmdBufferForRequest(requestId, m_pIPECmdBufferManager[CmdBufferStriping]);
                }

                if (NULL != m_pIPECmdBufferManager[CmdBufferBLMemory])
                {
                    pIPECmdBuffer[CmdBufferBLMemory] =
                        GetCmdBufferForRequest(requestId, m_pIPECmdBufferManager[CmdBufferBLMemory]);
                }

                if ((CamxResultSuccess == result) && (TRUE == m_capability.swStriping))
                {
                    result = FillStripingParams(pFrameProcessData, pIPEIQsettings, pIPECmdBuffer, &ICPClockBandwidthRequest);
                }

                if (CamxResultSuccess == result)
                {
                    result = PatchBLMemoryBuffer(pFrameProcessData, pIPECmdBuffer);
                }

                if (CamxResultSuccess == result)
                {
                    CheckAndUpdateClockBW(pIPECmdBuffer[CmdBufferGenericBlob], pExecuteProcessRequestData,
                                          &ICPClockBandwidthRequest);
                }
                if (CamxResultSuccess == result)
                {
                    result = CommitAllCommandBuffers(pIPECmdBuffer);
                }

                if (CamxResultSuccess == result)
                {
                    result = pIQPacket->CommitPacket();
                }

                if (CamxResultSuccess == result)
                {
                    result = pIQPacket->AddCmdBufferReference(pIPECmdBuffer[CmdBufferFrameProcess], NULL);
                }
                if (TRUE == HwEnvironment::GetInstance()->GetStaticSettings()->dumpIPEFirmwarePayload)
                {
                    // Dump all firmware payload components for debugging purpose only
                    DumpPayload(CmdBufferFrameProcess, pIPECmdBuffer[CmdBufferFrameProcess], requestId);
                    DumpPayload(CmdBufferStriping, pIPECmdBuffer[CmdBufferStriping], requestId);
                    DumpPayload(CmdBufferIQSettings, pIPECmdBuffer[CmdBufferIQSettings], requestId);
                    DumpPayload(CmdBufferPreLTM, pIPECmdBuffer[CmdBufferPreLTM], requestId);
                    DumpPayload(CmdBufferPostLTM, pIPECmdBuffer[CmdBufferPostLTM], requestId);
                    DumpPayload(CmdBufferDMIHeader, pIPECmdBuffer[CmdBufferDMIHeader], requestId);
                    DumpPayload(CmdBufferNPS, pIPECmdBuffer[CmdBufferNPS], requestId);
                }

                if (CamxResultSuccess == result)
                {
                    if (pIPECmdBuffer[CmdBufferGenericBlob]->GetResourceUsedDwords() > 0)
                    {
                        pIPECmdBuffer[CmdBufferGenericBlob]->SetMetadata(static_cast<UINT32>(CSLICPCmdBufferIdGenericBlob));
                        result = pIQPacket->AddCmdBufferReference(pIPECmdBuffer[CmdBufferGenericBlob], NULL);
                    }
                }

                // Post metadata from IQ modules to metadata
                if ((CamxResultSuccess == result) && (FALSE == IsSkipPostMetadata()))
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
                    CAMX_LOG_INFO(CamxLogGroupPProc, "Submit packets for IPE:%d request %llu",
                                     InstanceID(),
                                     requestId);

                    result = GetHwContext()->Submit(GetCSLSession(), m_hDevice, pIQPacket);

                    if (CamxResultSuccess != result)
                    {
                        CAMX_ASSERT(CamxResultSuccess == result);
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d Submit packets with requestId = %llu failed %d",
                                       InstanceID(),
                                       requestId,
                                       result);
                    }
                }
            }
        }


        if (CamxResultSuccess == result)
        {
            m_currentrequestID  = requestId;
            // Reset value for first valid request
            m_firstValidRequest = 0;
            CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d Submitted packet(s) with requestId = %llu successfully",
                          InstanceID(),
                          requestId);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::UpdateUBWCLossyParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::UpdateUBWCLossyParams(
    IpeConfigIoData*   pConfigIOData,
    const ImageFormat* pImageFormat,
    IPE_IO_IMAGES      firmwarePortId)
{
    BOOL is10BitFormat = ImageFormatUtils::Is10BitFormat(pImageFormat->format);

    pConfigIOData->images[firmwarePortId].info.ubwcLossyMode = pImageFormat->ubwcVerInfo.lossy;

    if (TRUE == is10BitFormat)
    {
        pConfigIOData->images[firmwarePortId].info.ubwcBwLimit = TITAN175UBWC10BitBandwidthLimit;
        if (UBWC_LOSSY == pConfigIOData->images[firmwarePortId].info.ubwcLossyMode)
        {
            switch (firmwarePortId)
            {
                case IPE_OUTPUT_IMAGE_FULL_REF:
                    pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy0 = TITAN175UBWC10BitThresholdLossy0TF;
                    pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy1 = TITAN175UBWC10BitThresholdLossy1TF;
                    break;
                case IPE_OUTPUT_IMAGE_DISPLAY:
                case IPE_OUTPUT_IMAGE_VIDEO:
                    pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy0 = TITAN175UBWC10BitThresholdLossy0;
                    pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy1 = TITAN175UBWC10BitThresholdLossy1;
                    break;
                default:
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "node %s firmwarePortId:%d is NOT UBWC3.0 Lossy supported port",
                                  NodeIdentifierString(),
                                  firmwarePortId);
                    break;
            }
        }
    }
    else
    {
        // 8 Bit output
        pConfigIOData->images[firmwarePortId].info.ubwcBwLimit = TITAN175UBWC8BitBandwidthLimit;
        if (UBWC_LOSSY == pConfigIOData->images[firmwarePortId].info.ubwcLossyMode)
        {
            switch (firmwarePortId)
            {
                case IPE_OUTPUT_IMAGE_FULL_REF:
                case IPE_OUTPUT_IMAGE_DISPLAY:
                case IPE_OUTPUT_IMAGE_VIDEO:
                    pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy0 = TITAN175UBWC8BitThresholdLossy0;
                    pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy1 = TITAN175UBWC8BitThresholdLossy1;
                    break;
                default:
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "node %s firmwarePortId:%d is NOT UBWC3.0 Lossy supported port",
                                  NodeIdentifierString(),
                                  firmwarePortId);
                    break;
            }
        }
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                    "node %s firmwarePortId:%d Is10Bit:%d BWLimit:0x%x LossyMode?%d Lossy0:0x%x Lossy1:0x%x",
                    NodeIdentifierString(),
                    firmwarePortId,
                    is10BitFormat,
                    pConfigIOData->images[firmwarePortId].info.ubwcBwLimit,
                    pConfigIOData->images[firmwarePortId].info.ubwcLossyMode,
                    pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy0,
                    pConfigIOData->images[firmwarePortId].info.ubwcThreshLossy1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::SetConfigIOData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::SetConfigIOData(
    IpeConfigIoData*   pConfigIOData,
    const ImageFormat* pImageFormat,
    IPE_IO_IMAGES      firmwarePortId,
    const CHAR*        pPortType)
{
    CamxResult result = CamxResultSuccess;

    pConfigIOData->images[firmwarePortId].info.format                 =
        TranslateFormatToFirmwareImageFormat(pImageFormat->format);
    pConfigIOData->images[firmwarePortId].info.dimensions.widthPixels = pImageFormat->width;
    pConfigIOData->images[firmwarePortId].info.dimensions.heightLines = pImageFormat->height;
    pConfigIOData->images[firmwarePortId].info.enableByteSwap         =
        ((pImageFormat->format == CamX::Format::YUV420NV21) ||
        (pImageFormat->format == CamX::Format::YUV420NV21TP10)) ? 1 : 0;

    if (TRUE == ImageFormatUtils::IsUBWC(pImageFormat->format))
    {
        pConfigIOData->images[firmwarePortId].info.ubwcVersion =
            static_cast <UbwcVersion>(GetUBWCHWVersion(firmwarePortId));
        if (UBWC_VERSION_3 <= GetUBWCHWVersion(firmwarePortId))
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Setting UBWC lossy %d", pImageFormat->ubwcVerInfo.lossy);
            UpdateUBWCLossyParams(pConfigIOData, pImageFormat, firmwarePortId);
        }
    }
    CAMX_LOG_INFO(CamxLogGroupPProc,
                  "node %s, %s, firmwarePortId %d format %d, width %d, height %d",
                  NodeIdentifierString(),
                  pPortType,
                  firmwarePortId,
                  pImageFormat->format,
                  pImageFormat->width,
                  pImageFormat->height,
                  pConfigIOData->images[firmwarePortId].info.ubwcVersion);

    const YUVFormat* pPlane = &pImageFormat->formatParams.yuvFormat[0];
    for (UINT plane = 0; plane < ImageFormatUtils::GetNumberOfPlanes(pImageFormat); plane++)
    {
        CAMX_ASSERT(plane <= MAX_NUM_OF_IMAGE_PLANES);
        pConfigIOData->images[firmwarePortId].bufferLayout[plane].bufferStride         =
            pPlane[plane].planeStride;
        pConfigIOData->images[firmwarePortId].bufferLayout[plane].bufferHeight         =
            pPlane[plane].sliceHeight;
        pConfigIOData->images[firmwarePortId].metadataBufferLayout[plane].bufferStride =
            pPlane[plane].metadataStride;
        pConfigIOData->images[firmwarePortId].metadataBufferLayout[plane].bufferHeight =
            pPlane[plane].metadataHeight;
        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                         "%s, plane %d, stride %d, scanline %d, metastride %d, metascanline %d",
                         pPortType,
                         plane,
                         pPlane[plane].planeStride,
                         pPlane[plane].sliceHeight,
                         pPlane[plane].metadataStride,
                         pPlane[plane].metadataHeight);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetFPSAndBatchSize()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetFPSAndBatchSize()
{
    static const UINT UsecasePropertiesIPE[] = { PropertyIDUsecaseFPS, PropertyIDUsecaseBatch };
    const UINT        length = CAMX_ARRAY_SIZE(UsecasePropertiesIPE);
    VOID*             pData[length] = { 0 };
    UINT64            usecasePropertyDataIPEOffset[length] = { 0 };

    CamxResult result = CamxResultSuccess;

    GetDataList(UsecasePropertiesIPE, pData, usecasePropertyDataIPEOffset, length);

    // This is a soft dependency
    if ((NULL != pData[0]) && (NULL != pData[1]))
    {
        m_FPS = *reinterpret_cast<UINT*>(pData[0]);
        m_maxBatchSize = *reinterpret_cast<UINT*>(pData[1]);
    }
    else
    {
        m_FPS = 30;
        m_maxBatchSize = 1;
    }

    if (FALSE == IsValidBatchSize())
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unsupported batch size %d for fps %d", m_maxBatchSize, m_FPS);
        result = CamxResultEUnsupported;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::SetupDeviceResource
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::SetupDeviceResource(
    CSLBufferInfo*     pConfigIOMem,
    CSLDeviceResource* pResource)
{
    CamxResult                  result          = CamxResultSuccess;
    CSLICPAcquireDeviceInfo*    pIcpResource    = NULL;
    CSLICPResourceInfo*         pIcpOutResource = NULL;
    UINT                        countResource   = 0;
    SIZE_T                      resourceSize    = 0;
    UINT                        numOutputPort   = 0;
    UINT                        numInputPort    = 0;
    const ImageFormat*          pImageFormat    = NULL;
    UINT                        inputPortId[IPEMaxInput];
    UINT                        outputPortId[IPEMaxOutput];
    IPE_IO_IMAGES               firmwarePortId;
    UINT                        parentNodeID = IFE;

    // Get Input Port List
    GetAllInputPortIds(&numInputPort, &inputPortId[0]);

    // Get Output Port List
    GetAllOutputPortIds(&numOutputPort, &outputPortId[0]);

    if (numInputPort <= 0 || numInputPort > IPEMaxInput ||
        numOutputPort <= 0 || numOutputPort > IPEMaxOutput)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "invalid input (%u) or output port (%u)", numInputPort, numOutputPort);
        result = CamxResultEUnsupported;
    }

    result = GetFPSAndBatchSize();

    if (CamxResultSuccess == result)
    {
        resourceSize                    = sizeof(CSLICPAcquireDeviceInfo) + (sizeof(CSLICPResourceInfo) * (numOutputPort - 1));
        pIcpResource                    = static_cast<CSLICPAcquireDeviceInfo*>(CAMX_CALLOC(resourceSize));

        if (NULL == pIcpResource)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "pIcpResource is NULL");
            result = CamxResultENoMemory;
        }

        if (CamxResultSuccess == result)
        {
            pIcpResource->numOutputResource = numOutputPort;
            pIcpResource->secureMode        = IsSecureMode();

            IpeConfigIo*     pConfigIO;
            IpeConfigIoData* pConfigIOData;

            pConfigIO          = reinterpret_cast<IpeConfigIo*>(pConfigIOMem->pVirtualAddr);
            pConfigIO->userArg = 0;
            pConfigIOData      = &pConfigIO->cmdData;
            CamX::Utils::Memset(pConfigIOData, 0, sizeof(*pConfigIOData));
            pConfigIOData->secureMode = IsSecureMode();

            for (UINT inputPortIndex = 0; inputPortIndex < numInputPort; inputPortIndex++)
            {
                TranslateToFirmwarePortId(inputPortId[inputPortIndex], &firmwarePortId);

                pImageFormat                                                      =
                    GetInputPortImageFormat(InputPortIndex(inputPortId[inputPortIndex]));

                if (NULL == pImageFormat)
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "pImageFormat is NULL");
                    result = CamxResultENoMemory;
                    break;
                }

                SetConfigIOData(pConfigIOData, pImageFormat, firmwarePortId, "Input");

                if (inputPortId[inputPortIndex] == IPEInputPortFull)
                {
                    pIcpResource->inputResource.format = static_cast <UINT32>(pImageFormat->format);
                    pIcpResource->inputResource.width  = pImageFormat->width;
                    pIcpResource->inputResource.height = pImageFormat->height;
                    pIcpResource->inputResource.FPS    = m_FPS;

                    m_fullInputWidth  = pImageFormat->width;
                    m_fullInputHeight = pImageFormat->height;
                    m_numPasses++;
                }

                if ((IPEInputPortDS4  == inputPortId[inputPortIndex])  ||
                    (IPEInputPortDS16 == inputPortId[inputPortIndex]) ||
                    (IPEInputPortDS64 == inputPortId[inputPortIndex]))
                {
                    m_numPasses++;
                    CAMX_LOG_INFO(CamxLogGroupPProc, "IPE: %d number of passes %d", InstanceID(), m_numPasses);
                }

                parentNodeID = GetParentNodeType(inputPortId[inputPortIndex]);
            }

            if (CamxResultSuccess == result)
            {
                pIcpOutResource = pIcpResource->outputResource;
                m_fullOutputHeight = 0;
                m_fullOutputWidth = 0;

                for (UINT outputPortIndex = 0; outputPortIndex < numOutputPort; outputPortIndex++)
                {
                    TranslateToFirmwarePortId(outputPortId[outputPortIndex], &firmwarePortId);

                    pImageFormat =
                        GetOutputPortImageFormat(OutputPortIndex(firmwarePortId));

                    if (NULL == pImageFormat)
                    {
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "pImageFormat is NULL");
                        result = CamxResultENoMemory;
                        break;
                    }

                    SetConfigIOData(pConfigIOData, pImageFormat, firmwarePortId, "Output");

                    pIcpOutResource->format = static_cast <UINT32>(pImageFormat->format);
                    pIcpOutResource->width = pImageFormat->width;
                    pIcpOutResource->height = pImageFormat->height;
                    pIcpOutResource->FPS = m_FPS;
                    pIcpOutResource++;

                    if ((outputPortId[outputPortIndex] == IPEOutputPortDisplay) ||
                        (outputPortId[outputPortIndex] == IPEOutputPortVideo))
                    {
                        if (m_fullOutputHeight < static_cast<INT32>(pImageFormat->height))
                        {
                            m_fullOutputHeight = pImageFormat->height;
                        }
                        if (m_fullOutputWidth < static_cast<INT32>(pImageFormat->width))
                        {
                            m_fullOutputWidth = pImageFormat->width;
                        }
                        if (TRUE == ImageFormatUtils::IsUBWC(pImageFormat->format))
                        {
                            m_compressiononOutput = TRUE;
                        }
                    }

                    if ((outputPortId[outputPortIndex] >= IPEOutputPortFullRef) &&
                        (outputPortId[outputPortIndex] <= IPEOutputPortDS64Ref))
                    {
                        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                            "Reference port enabled = %d", outputPortId[outputPortIndex]);
                        if (FALSE == IsLoopBackPortEnabled())
                        {
                            m_numOutputRefPorts++;
                        }
                        else
                        {
                            CAMX_LOG_ERROR(CamxLogGroupPProc, "node %s, invalid configuration"
                                "loop back ports enabled from xml with MCTF stabilization type",
                                NodeIdentifierString());
                            result = CamxResultEInvalidArg;
                            break;
                        }
                    }
                }
            }

            if (CamxResultSuccess == result)
            {
                if (TRUE == CheckIsIPERealtime())
                {
                    m_realTimeIPE = TRUE;
                    pIcpResource->resourceType = CSLICPResourceIDIPERealTime;
                }
                else
                {
                    m_realTimeIPE = FALSE;
                    pIcpResource->resourceType = CSLICPResourceIDIPENonRealTime;
                }

                result = GetStabilizationMargins();
                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to get stabilization margins %d", result);
                }

                UpdateNumberofPassesonDimension(parentNodeID, m_fullInputWidth, m_fullInputHeight);

                // Update IPE configIO topology type based on profile and lower pass input
                UpdateIPEConfigIOTopology(pConfigIOData, m_numPasses, m_instanceProperty.profileId);

                pConfigIOData->maxBatchSize = m_maxBatchSize;

                if ((TRUE == IsBlendWithNPS()) || (TRUE == IsPostfilterWithNPS()))
                {
                    SetMuxMode(pConfigIOData);
                    CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d, MUX Mode: %d", InstanceID(), pConfigIOData->muxMode);
                }

                pIcpResource->hIOConfigCmd  = pConfigIOMem->hHandle;
                pIcpResource->IOConfigLen   = sizeof(IpeConfigIo);
                countResource               = 1;

                // Add to the resource list
                pResource->resourceID              = pIcpResource->resourceType;
                pResource->pDeviceResourceParam    = static_cast<VOID*>(pIcpResource);
                pResource->deviceResourceParamSize = resourceSize;

                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "numResources %d", countResource);

                if ((CamxResultSuccess == result) && (TRUE == IsLoopBackPortEnabled()))
                {
                    result = CreateLoopBackBufferManagers();
                }

                if (CamxResultSuccess == result)
                {
                    pConfigIOData->stabilizationMargins.widthPixels = m_stabilizationMargin.widthPixels;
                    pConfigIOData->stabilizationMargins.heightLines = m_stabilizationMargin.heightLines;
                    result = InitializeStripingParams(pConfigIOData);
                    if (CamxResultSuccess != result)
                    {
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "Initialize Striping params failed %d", result);
                    }
                }
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::AcquireDevice
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::AcquireDevice()
{
    CSLICPAcquireDeviceInfo* pDevInfo;
    CamxResult               result = CamxResultSuccess;

    /// @todo (CAMX-886) Add CSLMemFlagSharedAccess once available from memory team

    if (CSLInvalidHandle == m_configIOMem.hHandle)
    {
        result = CSLAlloc(NodeIdentifierString(),
                          &m_configIOMem,
                          GetFWBufferAlignedSize(sizeof(IpeConfigIo)),
                          1,
                          (CSLMemFlagUMDAccess | CSLMemFlagSharedAccess | CSLMemFlagHw | CSLMemFlagKMDAccess),
                          &DeviceIndices()[0],
                          1);
    }
    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "CSLAlloc returned configIOMem.fd=%d", m_configIOMem.fd);

    if (CamxResultSuccess == result)
    {
        CAMX_ASSERT(CSLInvalidHandle != m_configIOMem.hHandle);
        CAMX_ASSERT(NULL != m_configIOMem.pVirtualAddr);

        if ((NULL != m_configIOMem.pVirtualAddr) && (CSLInvalidHandle != m_configIOMem.hHandle))
        {
            result = SetupDeviceResource(&m_configIOMem, &m_deviceResourceRequest);
        }

        if (CamxResultSuccess == result)
        {
            // During acquire device, KMD will create firmware handle and also call config IO
            result = CSLAcquireDevice(GetCSLSession(),
                                      &m_hDevice,
                                      DeviceIndices()[0],
                                      &m_deviceResourceRequest,
                                      1,
                                      NULL,
                                      0,
                                      NodeIdentifierString());

            pDevInfo = reinterpret_cast<CSLICPAcquireDeviceInfo*>(m_deviceResourceRequest.pDeviceResourceParam);
            // Firmware will provide scratch buffer requirements during configIO
            m_firmwareScratchMemSize = pDevInfo->scratchMemSize;

            if (CamxResultSuccess == result)
            {
                SetDeviceAcquired(TRUE);
                AddCSLDeviceHandle(m_hDevice);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d Acquire IPE Device Failed", InstanceID());
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d Out of memory", InstanceID());
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::ReleaseDevice
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::ReleaseDevice()
{
    CamxResult result = CamxResultSuccess;

    DeInitializeStripingParams();

    if ((NULL != GetHwContext()) && (0 != m_hDevice))
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
/// IPENode::ConfigureIPECapability
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::ConfigureIPECapability()
{
    CamxResult  result = CamxResultSuccess;
    BOOL  UBWCScaleRatioLimit = TRUE;

    GetHwContext()->GetDeviceVersion(CSLDeviceTypeICP, &m_version);
    switch (m_version.majorVersion)
    {
        /// @todo (CAMX-652) Finalize the version number definiation with CSL layer.
        case 0:
            /// @todo (CAMX-727) Implement query capability from hardware/firmware.
            m_capability.numIPEIQModules    = sizeof(IQModulesList) / sizeof(IPEIQModuleInfo);
            m_capability.pIPEIQModuleList   = IQModulesList;
            // Striping library in firmware will be removed in future. Remove this setting once striping in FW is removed.
            m_capability.swStriping         = static_cast<Titan17xContext *>(
                GetHwContext())->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->IPESwStriping;
            m_capability.maxInputWidth[ICA_MODE_DISABLED]  = IPEMaxInputWidthICADisabled;  // ICA Disabled
            m_capability.maxInputHeight[ICA_MODE_DISABLED] = IPEMaxInputHeightICADisabled; // ICA Disabled
            m_capability.maxInputWidth[ICA_MODE_ENABLED]   = IPEMaxInputWidthICAEnabled;   // ICA Enabled
            m_capability.maxInputHeight[ICA_MODE_ENABLED]  = IPEMaxInputHeightICAEnabled;  // ICA Enabled

            UBWCScaleRatioLimit = ImageFormatUtils::GetUBWCScaleRatioLimitationFlag();

            m_capability.minInputWidth   = IPEMinInputWidth;
            m_capability.minInputHeight  = IPEMinInputHeight;

            m_capability.maxDownscale[UBWC_MODE_DISABLED] = IPEMaxDownscaleLinear; // LINEAR Format
            m_capability.maxDownscale[UBWC_MODE_ENABLED]  =
                (UBWCScaleRatioLimit == FALSE) ? IPEMaxDownscaleLinear : IPEMaxDownscaleUBWC;   // UBWC   Format
            m_capability.maxUpscale[UBWC_MODE_DISABLED]   = IPEMaxUpscaleLinear;   // LINEAR Format
            m_capability.maxUpscale[UBWC_MODE_ENABLED]    =
                (UBWCScaleRatioLimit == FALSE) ? IPEMaxUpscaleLinear : IPEMaxUpscaleUBWC;       // UBWC   Format

            m_capability.numIPE = GetHwContext()->GetNumberOfIPE();
            break;
        default:
            result = CamxResultEUnsupported;
            CAMX_ASSERT_ALWAYS_MESSAGE("%s: Unsupported Version Number", __FUNCTION__);
            break;
    }

    switch (static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion())
    {
        case CSLCameraTitanVersion::CSLTitan170:
            m_capability.minOutputWidthUBWC  = TITAN170IPEMinOutputWidthUBWC;
            m_capability.minOutputHeightUBWC = TITAN170IPEMinOutputHeightUBWC;
            m_hwMask = ISPHwTitan170;
            break;

        case CSLCameraTitanVersion::CSLTitan175:
        case CSLCameraTitanVersion::CSLTitan160:
            // Both CSLTitan175 and CSLTitan160 have the same IPE h/w
            m_capability.minOutputWidthUBWC  = TITAN175IPEMinOutputWidthUBWC;
            m_capability.minOutputHeightUBWC = TITAN175IPEMinOutputHeightUBWC;
            m_hwMask = ISPHwTitan175;
            break;

        case CSLCameraTitanVersion::CSLTitan150:
            m_capability.minOutputWidthUBWC  = TITAN150IPEMinOutputWidthUBWC;
            m_capability.minOutputHeightUBWC = TITAN150IPEMinOutputHeightUBWC;
            m_hwMask = ISPHwTitan150;
            break;

        default:
            result = CamxResultEUnsupported;
            CAMX_LOG_ERROR(CamxLogGroupISP, "Unsupported Titan Version = %u",
                static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion());
            break;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::UpdateIPEIOLimits
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::UpdateIPEIOLimits(
    BufferNegotiationData* pBufferNegotiationData)
{
    UINT parentNodeID    = IFE;
    UINT numInputPort    = 0;
    UINT inputPortId[IPEMaxInput];

    if (pBufferNegotiationData == NULL)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d Invalid buffer negotiation data pointer", InstanceID());
        return CamxResultEInvalidPointer;
    }

    // Get Input Port List
    GetAllInputPortIds(&numInputPort, &inputPortId[0]);

    parentNodeID = GetParentNodeType(inputPortId[0]);

    if (parentNodeID == BPS)
    {
        // If other ports in addition to full port is notified, then limits change.
        // This is because if Full port along with other ports, DSx are enabled then
        // the limits of the IPE input processing changes due to NPS limitation.
        if (pBufferNegotiationData->numOutputPortsNotified > 1)
        {
            m_capability.minInputWidth  = IPEMinInputWidthMultiPassOffline;
            m_capability.minInputHeight = IPEMinInputHeightMultiPassOffline;
        }
        else
        {
            m_capability.minInputWidth  = IPEMinInputWidth;
            m_capability.minInputHeight = IPEMinInputHeight;
        }
    }
    else
    {
        // If other ports in addition to full port is notified, then limits change.
        // This is because if Full port along with other ports, DSx are enabled then
        // the limits of the IPE input processing changes due to NPS limitation.
        if (pBufferNegotiationData->numOutputPortsNotified > 1)
        {
            m_capability.minInputWidth  = IPEMinInputWidthMultiPass;
            m_capability.minInputHeight = IPEMinInputHeightMultiPass;
        }
        else
        {
            m_capability.minInputWidth  = IPEMinInputWidth;
            m_capability.minInputHeight = IPEMinInputHeight;
        }
    }

    return CamxResultSuccess;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GetModuleProcessingSection
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEProcessingSection GetModuleProcessingSection(
    ISPIQModuleType ipeIQModule)
{
    IPEProcessingSection type = IPEProcessingSection::IPEInvalidSection;

    switch (ipeIQModule)
    {
        case ISPIQModuleType::IPEICA:
        case ISPIQModuleType::IPERaster22:
        case ISPIQModuleType::IPERasterPD:
        case ISPIQModuleType::IPEANR:
        case ISPIQModuleType::IPETF:
            type = IPEProcessingSection::IPENPS;
            break;
        case ISPIQModuleType::IPECAC:
        case ISPIQModuleType::IPECrop:
        case ISPIQModuleType::IPEChromaUpsample:
        case ISPIQModuleType::IPECST:
        case ISPIQModuleType::IPELTM:
            type = IPEProcessingSection::IPEPPSPreLTM;
            break;

        case ISPIQModuleType::IPEColorCorrection:
        case ISPIQModuleType::IPEGamma:
        case ISPIQModuleType::IPE2DLUT:
        case ISPIQModuleType::IPEChromaEnhancement:
        case ISPIQModuleType::IPEChromaSuppression:
        case ISPIQModuleType::IPESCE:
        case ISPIQModuleType::IPEASF:
        case ISPIQModuleType::IPEUpscaler:
        case ISPIQModuleType::IPEGrainAdder:
        case ISPIQModuleType::IPEDownScaler:
        case ISPIQModuleType::IPEFOVCrop:
        case ISPIQModuleType::IPEClamp:
            type = IPEProcessingSection::IPEPPSPostLTM;
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Unsupported IQ module type");
            break;
    }
    return type;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GetProcessingSectionForProfile
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEProcessingSection GetProcessingSectionForProfile(
    IPEProfileId propertyValue)
{
    IPEProcessingSection type = IPEProcessingSection::IPEAll;
    switch (propertyValue)
    {
        case IPEProfileId::IPEProfileIdNPS:
            type = IPEProcessingSection::IPENPS;
            break;
        case IPEProfileId::IPEProfileIdPPS:
            type = IPEProcessingSection::IPEPPS;
            break;
        case IPEProfileId::IPEProfileIdScale:
            break;
        case IPEProfileId::IPEProfileIdNoZoomCrop:
            type = IPEProcessingSection::IPENoZoomCrop;
            break;
        case IPEProfileId::IPEProfileIdDefault:
            type = IPEProcessingSection::IPEAll;
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Unsupported IQ module type");
            break;
    }
    return type;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::CreateIPEIQModules
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::CreateIPEIQModules()
{
    CamxResult              result                      = CamxResultSuccess;
    IPEIQModuleInfo*        pIQModule                   = m_capability.pIPEIQModuleList;
    IPEModuleCreateData     moduleInputData             = { 0 };
    IPEProcessingSection    instanceSection             = IPEProcessingSection::IPEAll;
    IPEProcessingSection    moduleSection               = IPEProcessingSection::IPEAll;
    BOOL                    moduleDependeciesEnabled    = TRUE;

    instanceSection = GetProcessingSectionForProfile(m_instanceProperty.profileId);
    // This is a special case where we want all IQ blocks, but want to skip the fillzoomwindow
    if (IPEProcessingSection::IPENoZoomCrop == instanceSection)
    {
        m_nodePropDisableZoomCrop = TRUE;
        // set the instanceSection back to IPEALL to simplify reuse the logic for IPEAll
        instanceSection = IPEProcessingSection::IPEAll;
    }
    moduleInputData.initializationData.pipelineIPEData.pDeviceIndex = &m_deviceIndex;
    moduleInputData.initializationData.pHwContext                   = GetHwContext();
    moduleInputData.initializationData.requestQueueDepth            = GetPipeline()->GetRequestQueueDepth();
    moduleInputData.pNodeIdentifier                                 = NodeIdentifierString();

    m_numIPEIQModulesEnabled                                        = 0;

    result = CreateIPEICAInputModule(&moduleInputData, pIQModule);
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d Failed to Create ICA1 Module, result = %d",
            InstanceID(), result);
    }

    if (CamxResultSuccess == result)
    {
        // create rest of the IQ module starting from index after first mandatory ICA1 module
        for (UINT count = 1; count < m_capability.numIPEIQModules; count++)
        {
            if (IPEProfileId::IPEProfileIdScale == m_instanceProperty.profileId)
            {
                // No IQ modules for Scale profile
                break;
            }
            moduleDependeciesEnabled = TRUE;

            if (instanceSection != IPEProcessingSection::IPEAll)
            {
                moduleSection = GetModuleProcessingSection(pIQModule[count].moduleType);
                if ((moduleSection == IPEProcessingSection::IPEPPSPreLTM) ||
                    (moduleSection == IPEProcessingSection::IPEPPSPostLTM))
                {
                    moduleSection = IPEProcessingSection::IPEPPS;
                }
                // In case of Invalid Processing section only moduleDependeciesEnabled should be FALSE
                if (instanceSection != moduleSection)
                {
                    moduleDependeciesEnabled = FALSE;
                }
            }

            /// @todo (CAMX-735) Link IPE IQ modules with new Chromatix adapter
            if ((TRUE == IsIQModuleInstalled(&pIQModule[count])) && (TRUE == moduleDependeciesEnabled))
            {
                moduleInputData.path = pIQModule[count].path;
                result               = pIQModule[count].IQCreate(&moduleInputData);
                if (CamxResultSuccess == result)
                {
                    m_pEnabledIPEIQModule[m_numIPEIQModulesEnabled] = moduleInputData.pModule;
                    m_numIPEIQModulesEnabled++;
                }
                else
                {
                    CAMX_ASSERT_ALWAYS_MESSAGE("%s: Failed to Create IQ Module, count = %d", __FUNCTION__, count);
                    break;
                }
            }
        }
    }

    if ((CamxResultSuccess == result) && (0 < m_numIPEIQModulesEnabled))
    {
        result = CreateIQModulesCmdBufferManager(&moduleInputData);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::CreateIQModulesCmdBufferManager
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::CreateIQModulesCmdBufferManager(
    IPEModuleCreateData*     pModuleInputData)
{
    CamxResult              result                   = CamxResultSuccess;
    CmdBufferManagerParam*  pBufferManagerParam      = NULL;
    IQModuleCmdBufferParam  bufferManagerCreateParam = { 0 };

    pBufferManagerParam =
        static_cast<CmdBufferManagerParam*>(CAMX_CALLOC(sizeof(CmdBufferManagerParam) * m_capability.numIPEIQModules));

    if (NULL != pBufferManagerParam)
    {
        bufferManagerCreateParam.pCmdBufManagerParam    = pBufferManagerParam;
        bufferManagerCreateParam.numberOfCmdBufManagers = 0;

        for (UINT count = 0; count < m_numIPEIQModulesEnabled; count++)
        {
            pModuleInputData->pModule = m_pEnabledIPEIQModule[count];
            pModuleInputData->pModule->FillCmdBufferManagerParams(&pModuleInputData->initializationData,
                &bufferManagerCreateParam);
        }

        if (0 != bufferManagerCreateParam.numberOfCmdBufManagers)
        {
            // Create Cmd Buffer Managers for IQ Modules
            result = CmdBufferManager::CreateMultiManager(pBufferManagerParam, bufferManagerCreateParam.numberOfCmdBufManagers);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "IQ Modules Cmd Buffer Manager Creation failed");
            }
        }

        // Free up the memory allocated bt IQ Blocks
        for (UINT index = 0; index < bufferManagerCreateParam.numberOfCmdBufManagers; index++)
        {
            if (NULL != pBufferManagerParam[index].pBufferManagerName)
            {
                CAMX_DELETE pBufferManagerParam[index].pBufferManagerName;
                pBufferManagerParam[index].pBufferManagerName = NULL;
            }

            if (NULL != pBufferManagerParam[index].pParams)
            {
                CAMX_DELETE pBufferManagerParam[index].pParams;
                pBufferManagerParam[index].pParams = NULL;
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

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::Cleanup()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::Cleanup()
{
    UINT           count            = 0;
    CamxResult     result           = CamxResultSuccess;
    UINT32         numberOfMappings = 0;
    CSLBufferInfo  bufferInfo       = { 0 };
    CSLBufferInfo* pBufferInfo[CSLICPMaxMemoryMapRegions];

    if (NULL != m_pIPECmdBufferManager[CmdBufferFrameProcess])
    {
        if (NULL != m_pIPECmdBufferManager[CmdBufferFrameProcess]->GetMergedCSLBufferInfo())
        {
            Utils::Memcpy(&bufferInfo,
                          m_pIPECmdBufferManager[CmdBufferFrameProcess]->GetMergedCSLBufferInfo(),
                          sizeof(CSLBufferInfo));
            pBufferInfo[numberOfMappings] = &bufferInfo;
            numberOfMappings++;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Unable to get Merged Buffer Info");
            result = CamxResultEFailed;
        }
    }

    if (NULL != m_UBWCStatBufInfo.pUBWCStatsBuffer)
    {
        pBufferInfo[numberOfMappings] = m_UBWCStatBufInfo.pUBWCStatsBuffer;
        numberOfMappings++;

    }

    if (0 != numberOfMappings)
    {
        result = SendFWCmdRegionInfo(CSLICPGenericBlobCmdBufferUnMapFWMemRegion,
                                     pBufferInfo,
                                     numberOfMappings);
    }

    // De-allocate all of the IQ modules

    for (count = 0; count < m_numIPEIQModulesEnabled; count++)
    {
        if (NULL != m_pEnabledIPEIQModule[count])
        {
            m_pEnabledIPEIQModule[count]->Destroy();
            m_pEnabledIPEIQModule[count] = NULL;
        }
    }
    for (count = 0; count < m_numScratchBuffers; count++)
    {
        if (NULL != m_pScratchMemoryBuffer[count])
        {
            if (CSLInvalidHandle != m_pScratchMemoryBuffer[count]->hHandle)
            {
                CSLReleaseBuffer(m_pScratchMemoryBuffer[count]->hHandle);
            }
            CAMX_FREE(m_pScratchMemoryBuffer[count]);
            m_pScratchMemoryBuffer[count] = NULL;
        }
    }

    // free ubwc buffer
    if (NULL != m_UBWCStatBufInfo.pUBWCStatsBuffer)
    {
        CSLReleaseBuffer(m_UBWCStatBufInfo.pUBWCStatsBuffer->hHandle);
        CAMX_FREE(m_UBWCStatBufInfo.pUBWCStatsBuffer);
        m_UBWCStatBufInfo.pUBWCStatsBuffer = NULL;
    }


    if (TRUE == IsLoopBackPortEnabled())
    {
        DeleteLoopBackBufferManagers();
    }

    m_numIPEIQModulesEnabled = 0;

    // Check if striping in UMD is enabled before destroying striping library context
    if (m_capability.swStriping)
    {
        result = IPEStripingLibraryContextDestroy(&m_hStripingLib);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to destroy striping library, error = %d", result);
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

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::PopulateGeneralTuningMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::PopulateGeneralTuningMetadata(
    ISPInputData* pInputData)
{
    IpeIQSettings*          pIPEIQsettings          = static_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);
    ChiTuningModeParameter* pTuningModeParameter    = pInputData->pTuningData;
    TuningModeDebugData*    pTuningDebugData        = &pInputData->pIPETuningMetadata->IPETuningModeDebugData;

    // Copy to a packed structure the BPS module configuration used by FW
    CAMX_STATIC_ASSERT(sizeof(IpeIQSettings) <= sizeof(pInputData->pIPETuningMetadata->IPEModuleConfigData));
    Utils::Memcpy(&pInputData->pIPETuningMetadata->IPEModuleConfigData,
                  pIPEIQsettings,
                  sizeof(IpeIQSettings));

    // Populate node information
    pInputData->pIPETuningMetadata->IPENodeInformation.instaceId        = InstanceID();
    pInputData->pIPETuningMetadata->IPENodeInformation.requestId        = pInputData->frameNum;
    pInputData->pIPETuningMetadata->IPENodeInformation.isRealTime       = IsRealTime();
    pInputData->pIPETuningMetadata->IPENodeInformation.profileId        = m_instanceProperty.profileId;
    pInputData->pIPETuningMetadata->IPENodeInformation.processingType   = m_instanceProperty.processingType;

    ChiTuningMode* pTuningMode = NULL;
    for (UINT32 paramNumber = 0; paramNumber <  pTuningModeParameter->noOfSelectionParameter; paramNumber++)
    {
        pTuningMode = &pTuningModeParameter->TuningMode[paramNumber];

        switch (pTuningMode->mode)
        {
            case ChiModeType::Default:
                pTuningDebugData->base = static_cast<UINT32>(pTuningMode->subMode.value);
                break;
            case ChiModeType::Sensor:
                pTuningDebugData->sensor = static_cast<UINT32>(pTuningMode->subMode.value);
                break;
            case ChiModeType::Usecase:
                pTuningDebugData->usecase = static_cast<UINT32>(pTuningMode->subMode.usecase);
                break;
            case ChiModeType::Feature1:
                pTuningDebugData->feature1 = static_cast<UINT32>(pTuningMode->subMode.feature1);
                break;
            case ChiModeType::Feature2:
                pTuningDebugData->feature2 = static_cast<UINT32>(pTuningMode->subMode.feature2);
                break;
            case ChiModeType::Scene:
                pTuningDebugData->scene = static_cast<UINT32>(pTuningMode->subMode.scene);
                break;
            case ChiModeType::Effect:
                pTuningDebugData->effect = static_cast<UINT32>(pTuningMode->subMode.effect);
                break;
            default:
                CAMX_LOG_WARN(CamxLogGroupPProc, "IPE: fail to set tuning mode type");
                break;
        }
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                     "TuningMode: ReqID: %llu: Default %u, Sensor %u usecase %u feature1 %u feature2 %u secne %u effect %u",
                     pInputData->frameNum,
                     pTuningDebugData->base,
                     pTuningDebugData->sensor,
                     pTuningDebugData->usecase,
                     pTuningDebugData->feature1,
                     pTuningDebugData->feature2,
                     pTuningDebugData->scene,
                     pTuningDebugData->effect);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::DumpTuningMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::DumpTuningMetadata(
    ISPInputData* pInputData)
{
    CamxResult      result                              = CamxResultSuccess;
    BOOL            setNewPartition                     = FALSE;
    UINT32          debugDataPartition                  = 0;
    DebugData       debugData                           = { 0 };
    UINT            PropertiesTuning[]                  = { 0 };
    static UINT     metaTagDebugDataAll                 = 0;
    const UINT      length                              = CAMX_ARRAY_SIZE(PropertiesTuning);
    VOID*           pData[length]                       = { 0 };
    UINT64          propertyDataTuningOffset[length]    = { 0 };
    DebugData*      pDebugDataPartial                   = NULL;
    IpeIQSettings*  pIPEIQSettings                      = NULL;

    if (TRUE == IsRealTime())
    {
        PropertiesTuning[0] = PropertyIDTuningDataIPE;
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
        CAMX_LOG_WARN(CamxLogGroupPProc, "Debug-data requested but buffer not available");
        return;
    }
    if ((FALSE == IsRealTime())                             &&
        (IPEProfileIdScale == m_instanceProperty.profileId))
    {
        CAMX_LOG_INFO(CamxLogGroupPProc, "RT: %u, Do not dump on scale processing",
                      IsRealTime());
        return;
    }

    if (FALSE == IsRealTime())
    {
        if ((IPEProfileIdNPS    == m_instanceProperty.profileId)        &&
            (IPEMFNRPostfilter  == m_instanceProperty.processingType))
        {
            // MFNR NPS: Post-filter will go in secondary partition
            setNewPartition     = TRUE;
            debugDataPartition  = 2;
        }
        else
        {
            // Offline IPE will start in partition one
            setNewPartition     = FALSE;
            debugDataPartition  = 1;

        }
    }

    if (TRUE == IsRealTime())
    {
        debugData.pData = pDebugDataPartial->pData;
        debugData.size  = pDebugDataPartial->size;
        // Use first half for Real time data
        debugData.size  = debugData.size / DebugDataPartitionsIPE;
    }
    else if (DebugDataPartitionsIPE > debugDataPartition) // Using copy done for offline processing
    {
        SIZE_T instanceOffset = 0;
        debugData.size  = HAL3MetadataUtil::DebugDataSize(DebugDataType::IPETuning);
        // Use second half for offline data
        // Only 2 offline IPE pass supported for debug data, scale not included
        debugData.size  = debugData.size / DebugDataPartitionsIPE;
        instanceOffset  = debugDataPartition * debugData.size; // Real-time takes the first partition of IPE data
        debugData.pData = Utils::VoidPtrInc(pDebugDataPartial->pData,
                                           (HAL3MetadataUtil::DebugDataOffset(DebugDataType::IPETuning)) + (instanceOffset));
    }
    else
    {
        result = CamxResultENoMemory;
        CAMX_LOG_ERROR(CamxLogGroupPProc,
                       "RT: %u:  ERROR: debugDataPartition: %u: Debug-data Not enough memory to save IPE data frameNum: %llu",
                       IsRealTime(),
                       debugDataPartition,
                       pInputData->frameNum);
    }

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE:RT: %u, frameNum: %llu, InstanceID: %u, base: %p, start: %p, size: %u,"
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

    // Populate any metadata obtained direclty from base IPE node
    PopulateGeneralTuningMetadata(pInputData);

    if ((result == CamxResultSuccess)                                                                               &&
        ((FALSE == setNewPartition)                                                                                 ||
        (FALSE  == (s_pDebugDataWriter->IsSameBufferPointer(static_cast<BYTE*>(debugData.pData), debugData.size)))))
    {
        // Set the buffer pointer
        s_pDebugDataWriter->SetBufferPointer(static_cast<BYTE*>(debugData.pData), debugData.size);
    }

    pIPEIQSettings = static_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);
    CAMX_ASSERT_MESSAGE((NULL != pIPEIQSettings), "IPE IQ Settings not available");

    if ((TRUE   == IsRealTime()) &&
        (result == CamxResultSuccess))
    {
        // Add IPE tuning metadata tags
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPENodeInfo,
                                       DebugDataTagType::TuningIQNodeInfo,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPENodeInformation,
                                       sizeof(pInputData->pIPETuningMetadata->IPENodeInformation));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPETuningMode,
                                       DebugDataTagType::TuningModeInfo,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPETuningModeDebugData,
                                       sizeof(pInputData->pIPETuningMetadata->IPETuningModeDebugData));

        if (TRUE == pIPEIQSettings->glutParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEGammaPackedLUT,
                                           DebugDataTagType::TuningGammaIPECurve,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.gamma),
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.gamma,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.gamma));
        }

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEModulesConfigRegisterV1,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEModuleConfigData.modulesConfigData),
                                       &pInputData->pIPETuningMetadata->IPEModuleConfigData.modulesConfigData,
                                       sizeof(pInputData->pIPETuningMetadata->IPEModuleConfigData.modulesConfigData));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEICEInputPackedLUT,
                                       DebugDataTagType::TuningICALUT,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ICALUT[TuningICAInput],
                                       sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ICALUT[TuningICAInput]));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEICEReferencePackedLUT,
                                       DebugDataTagType::TuningICALUT,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ICALUT[TuningICAReference],
                                       sizeof(pInputData->pIPETuningMetadata->
                                           IPEDMIData.packedLUT.ICALUT[TuningICAReference]));

        if (TRUE == pIPEIQSettings->anrParameters.parameters[0].moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEANRRegister,
                                           DebugDataTagType::TuningANRConfig,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEANRData.ANRData),
                                           &pInputData->pIPETuningMetadata->IPEANRData.ANRData,
                                           sizeof(pInputData->pIPETuningMetadata->IPEANRData.ANRData));
        }

        if (TRUE == pIPEIQSettings->tfParameters.parameters[0].moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPETFRegister,
                                           DebugDataTagType::TuningTFConfig,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPETFData.TFData),
                                           &pInputData->pIPETuningMetadata->IPETFData.TFData,
                                           sizeof(pInputData->pIPETuningMetadata->IPETFData.TFData));
        }

        if (TRUE == pIPEIQSettings->cacParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPECACRegister,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPECACData.CACConfig),
                                           &pInputData->pIPETuningMetadata->IPECACData.CACConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPECACData.CACConfig));
        }

        if (TRUE == pIPEIQSettings->colorTransformParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPECSTRegister,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPECSTData.CSTConfig),
                                           &pInputData->pIPETuningMetadata->IPECSTData.CSTConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPECSTData.CSTConfig));
        }

        if (TRUE == pIPEIQSettings->ltmParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPELTMPackedLUT,
                                           DebugDataTagType::TuningLTMLUT,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LTMLUT,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LTMLUT));
        }

        if (TRUE == pIPEIQSettings->colorCorrectParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPECCRegister,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPECCData.colorCorrectionConfig),
                                           &pInputData->pIPETuningMetadata->IPECCData.colorCorrectionConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPECCData.colorCorrectionConfig));
        }

        if (TRUE == pIPEIQSettings->lut2dParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPE2DLUTRegister,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPE2DLUTData.LUT2DConfig),
                                           &pInputData->pIPETuningMetadata->IPE2DLUTData.LUT2DConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPE2DLUTData.LUT2DConfig));

            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPE2DLUTPackedLUT,
                                           DebugDataTagType::Tuning2DLUTLUT,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LUT2D,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LUT2D));
        }

        if (TRUE == pIPEIQSettings->chromaEnhancementParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEChromaEnhancementRegister,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEChromaEnhancementData.CEConfig),
                                           &pInputData->pIPETuningMetadata->IPEChromaEnhancementData.CEConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPEChromaEnhancementData.CEConfig));
        }

        if (TRUE == pIPEIQSettings->chromaSupressionParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEChromasuppressionRegister,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEChromaSuppressionData.CSConfig),
                                           &pInputData->pIPETuningMetadata->IPEChromaSuppressionData.CSConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPEChromaSuppressionData.CSConfig));
        }

        if (TRUE == pIPEIQSettings->skinEnhancementParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPESCERegister,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPESCEData.SCEConfig),
                                           &pInputData->pIPETuningMetadata->IPESCEData.SCEConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPESCEData.SCEConfig));
        }

        if (TRUE == pIPEIQSettings->asfParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEASFRegister,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEASFData.ASFConfig),
                                           &pInputData->pIPETuningMetadata->IPEASFData.ASFConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPEASFData.ASFConfig));

            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEASFPackedLUT,
                                           DebugDataTagType::TuningASFLUT,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ASFLUT,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ASFLUT));
        }

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEUpscalerPackedLUT,
                                       DebugDataTagType::TuningUpscalerLUT,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.upscalerLUT,
                                       sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.upscalerLUT));

        if (TRUE == pIPEIQSettings->graParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEGrainAdderPackedLUT,
                                           DebugDataTagType::TuningGrainAdderLUT,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.grainAdderLUT,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.grainAdderLUT));
        }

        if (TRUE == pIPEIQSettings->ltmParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPELTMExposureIndex,
                                           DebugDataTagType::Float,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPELTMExposureData.exposureIndex),
                                           &pInputData->pIPETuningMetadata->IPELTMExposureData.exposureIndex,
                                           sizeof(pInputData->pIPETuningMetadata->IPELTMExposureData.exposureIndex));
        }

        if (TRUE == pIPEIQSettings->asfParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEASFFace,
                                           DebugDataTagType::TuningFaceData,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEASFFaceDetection,
                                           sizeof(pInputData->pIPETuningMetadata->IPEASFFaceDetection));
        }

        if (TRUE == pIPEIQSettings->anrParameters.parameters[0].moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEANRFace,
                                           DebugDataTagType::TuningFaceData,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEANRFaceDetection,
                                           sizeof(pInputData->pIPETuningMetadata->IPEANRFaceDetection));
        }

        // Make a copy in main metadata pool
        static const UINT PropertiesDebugData[] = { PropertyIDDebugDataAll };
        VOID* pSrcData[1] = { 0 };
        const UINT lengthAll = CAMX_ARRAY_SIZE(PropertiesDebugData);
        UINT64 propertyDataTuningAllOffset[lengthAll] = { 0 };
        GetDataList(PropertiesDebugData, pSrcData, propertyDataTuningAllOffset, lengthAll);

        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.debugdata", "DebugDataAll", &metaTagDebugDataAll);
        const UINT TuningVendorTag[] = { metaTagDebugDataAll };
        const VOID* pDstData[1] = { pSrcData[0] };
        UINT pDataCount[1] = { sizeof(DebugData) };

        WriteDataList(TuningVendorTag, pDstData, pDataCount, 1);
    }
    else if (result == CamxResultSuccess) // Handle offline data
    {
        // Add IPE tuning metadata tags
        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPENodeInfoOffline,
                                       DebugDataTagType::TuningIQNodeInfo,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPENodeInformation,
                                       sizeof(pInputData->pIPETuningMetadata->IPENodeInformation));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPETuningModeOffline,
                                       DebugDataTagType::TuningModeInfo,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPETuningModeDebugData,
                                       sizeof(pInputData->pIPETuningMetadata->IPETuningModeDebugData));

        if (TRUE == pIPEIQSettings->glutParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEGammaPackedLUTOffline,
                                           DebugDataTagType::TuningGammaIPECurve,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.gamma),
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.gamma,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.gamma));
        }

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEModulesConfigRegisterOfflineV1,
                                       DebugDataTagType::UInt32,
                                       CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEModuleConfigData.modulesConfigData),
                                       &pInputData->pIPETuningMetadata->IPEModuleConfigData.modulesConfigData,
                                       sizeof(pInputData->pIPETuningMetadata->IPEModuleConfigData.modulesConfigData));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEICEInputPackedLUTOffline,
                                       DebugDataTagType::TuningICALUT,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ICALUT[TuningICAInput],
                                       sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ICALUT[TuningICAInput]));

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEICEReferencePackedLUTOffline,
                                       DebugDataTagType::TuningICALUT,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ICALUT[TuningICAReference],
                                       sizeof(pInputData->pIPETuningMetadata->
                                           IPEDMIData.packedLUT.ICALUT[TuningICAReference]));

        if (TRUE == pIPEIQSettings->anrParameters.parameters[0].moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEANRRegisterOffline,
                                           DebugDataTagType::TuningANRConfig,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEANRData.ANRData),
                                           &pInputData->pIPETuningMetadata->IPEANRData.ANRData,
                                           sizeof(pInputData->pIPETuningMetadata->IPEANRData.ANRData));
        }

        if (TRUE == pIPEIQSettings->tfParameters.parameters[0].moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPETFRegisterOffline,
                                           DebugDataTagType::TuningTFConfig,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPETFData.TFData),
                                           &pInputData->pIPETuningMetadata->IPETFData.TFData,
                                           sizeof(pInputData->pIPETuningMetadata->IPETFData.TFData));
        }

        if (TRUE == pIPEIQSettings->cacParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPECACRegisterOffline,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPECACData.CACConfig),
                                           &pInputData->pIPETuningMetadata->IPECACData.CACConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPECACData.CACConfig));
        }

        if (TRUE == pIPEIQSettings->colorTransformParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPECSTRegisterOffline,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPECSTData.CSTConfig),
                                           &pInputData->pIPETuningMetadata->IPECSTData.CSTConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPECSTData.CSTConfig));
        }

        if (TRUE == pIPEIQSettings->ltmParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPELTMPackedLUTOffline,
                                           DebugDataTagType::TuningLTMLUT,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LTMLUT,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LTMLUT));
        }

        if (TRUE == pIPEIQSettings->colorCorrectParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPECCRegisterOffline,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPECCData.colorCorrectionConfig),
                                           &pInputData->pIPETuningMetadata->IPECCData.colorCorrectionConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPECCData.colorCorrectionConfig));
        }

        if (TRUE == pIPEIQSettings->lut2dParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPE2DLUTRegisterOffline,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPE2DLUTData.LUT2DConfig),
                                           &pInputData->pIPETuningMetadata->IPE2DLUTData.LUT2DConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPE2DLUTData.LUT2DConfig));


            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPE2DLUTPackedLUTOffline,
                                           DebugDataTagType::Tuning2DLUTLUT,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LUT2D,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.LUT2D));
        }

        if (TRUE == pIPEIQSettings->chromaEnhancementParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEChromaEnhancementRegisterOffline,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEChromaEnhancementData.CEConfig),
                                           &pInputData->pIPETuningMetadata->IPEChromaEnhancementData.CEConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPEChromaEnhancementData.CEConfig));
        }

        if (TRUE == pIPEIQSettings->chromaSupressionParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEChromasuppressionRegisterOffline,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEChromaSuppressionData.CSConfig),
                                           &pInputData->pIPETuningMetadata->IPEChromaSuppressionData.CSConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPEChromaSuppressionData.CSConfig));
        }

        if (TRUE == pIPEIQSettings->skinEnhancementParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPESCERegisterOffline,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPESCEData.SCEConfig),
                                           &pInputData->pIPETuningMetadata->IPESCEData.SCEConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPESCEData.SCEConfig));
        }

        if (TRUE == pIPEIQSettings->asfParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEASFRegisterOffline,
                                           DebugDataTagType::UInt32,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPEASFData.ASFConfig),
                                           &pInputData->pIPETuningMetadata->IPEASFData.ASFConfig,
                                           sizeof(pInputData->pIPETuningMetadata->IPEASFData.ASFConfig));

            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEASFPackedLUTOffline,
                                           DebugDataTagType::TuningASFLUT,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ASFLUT,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.ASFLUT));
        }

        s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEUpscalerPackedLUTOffline,
                                       DebugDataTagType::TuningUpscalerLUT,
                                       1,
                                       &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.upscalerLUT,
                                       sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.upscalerLUT));

        if (TRUE == pIPEIQSettings->graParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEGrainAdderPackedLUTOffline,
                                           DebugDataTagType::TuningGrainAdderLUT,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.grainAdderLUT,
                                           sizeof(pInputData->pIPETuningMetadata->IPEDMIData.packedLUT.grainAdderLUT));
        }

        if (TRUE == pIPEIQSettings->ltmParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPELTMExposureIndexOffline,
                                           DebugDataTagType::Float,
                                           CAMX_ARRAY_SIZE(pInputData->pIPETuningMetadata->IPELTMExposureData.exposureIndex),
                                           &pInputData->pIPETuningMetadata->IPELTMExposureData.exposureIndex,
                                           sizeof(pInputData->pIPETuningMetadata->IPELTMExposureData.exposureIndex));
        }

        if (TRUE == pIPEIQSettings->asfParameters.moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEASFFaceOffline,
                                           DebugDataTagType::TuningFaceData,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEASFFaceDetection,
                                           sizeof(pInputData->pIPETuningMetadata->IPEASFFaceDetection));
        }

        if (TRUE == pIPEIQSettings->anrParameters.parameters[0].moduleCfg.EN)
        {
            s_pDebugDataWriter->AddDataTag(DebugDataTagID::TuningIPEANRFaceOffline,
                                           DebugDataTagType::TuningFaceData,
                                           1,
                                           &pInputData->pIPETuningMetadata->IPEANRFaceDetection,
                                           sizeof(pInputData->pIPETuningMetadata->IPEANRFaceDetection));
        }

    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::PostMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::PostMetadata(
    const ISPInputData* pInputData)
{
    CamxResult  result                               = CamxResultSuccess;
    const VOID* ppData[NumIPEMetadataOutputTags]     = { 0 };
    UINT        pDataCount[NumIPEMetadataOutputTags] = { 0 };
    UINT        index                                = 0;
    UINT        effectMap                            = 0;
    UINT        sceneMap                             = 0;
    UINT        modeMap                              = 0;

    ChiModeType              mode = ChiModeType::Default;
    ChiModeEffectSubModeType effect = ChiModeEffectSubModeType::None;
    ChiModeSceneSubModeType  scene = ChiModeSceneSubModeType::None;

    if (pInputData->pTuningData)
    {
        for (UINT i = 0; i < pInputData->pTuningData->noOfSelectionParameter; i++)
        {
            mode = pInputData->pTuningData->TuningMode[i].mode;
            if (ChiModeType::Effect == mode)
            {
                effect = pInputData->pTuningData->TuningMode[i].subMode.effect;
                effectMap = IPEEffectMap[static_cast<UINT>(effect)].to;
            }
            if (ChiModeType::Scene == mode)
            {
                if (ChiModeSceneSubModeType::BestShot >= pInputData->pTuningData->TuningMode[i].subMode.scene)
                {
                    scene = pInputData->pTuningData->TuningMode[i].subMode.scene;
                }
                else
                {
                    CAMX_LOG_INFO(CamxLogGroupPProc, "Tuning data scene out of bound, Use default scene : None ");
                }
                sceneMap = IPESceneMap[static_cast<UINT>(scene)].to;
            }
        }
    }

    // Get Control mode, Scene mode and effects mode from HAL tag
    static const UINT vendorTagsControlMode[] = { InputControlMode, InputControlSceneMode, InputControlEffectMode };
    const SIZE_T numTags                      = CAMX_ARRAY_SIZE(vendorTagsControlMode);
    VOID*        pData[numTags]               = { 0 };
    UINT64       pDataModeOffset[numTags]     = { 0 };

    GetDataList(vendorTagsControlMode, pData, pDataModeOffset, numTags);

    if (NULL != pData[0])
    {
        Utils::Memcpy(&modeMap, pData[0], sizeof(modeMap));
    }

    if (NULL != pData[1])
    {
        Utils::Memcpy(&sceneMap, pData[1], sizeof(sceneMap));
    }

    if (NULL != pData[2])
    {
        Utils::Memcpy(&effectMap, pData[2], sizeof(effectMap));
    }

    pDataCount[index] = 1;
    ppData[index]     = &modeMap;
    index++;

    pDataCount[index] = 1;
    ppData[index]     = &effectMap;
    index++;

    pDataCount[index] = 1;
    ppData[index]     = &sceneMap;
    index++;

    pDataCount[index] = 1;
    ppData[index]     = &(pInputData->pCalculatedData->metadata.edgeMode);
    index++;

    pDataCount[index] = 1;
    ppData[index]     = &(pInputData->pHALTagsData->controlVideoStabilizationMode);
    index++;

    pDataCount[index] = 1;
    ppData[index]     = &(pInputData->pCalculatedData->metadata.colorCorrectionAberrationMode);
    index++;

    pDataCount[index] = 1;
    ppData[index]     = &(pInputData->pHALTagsData->noiseReductionMode);
    index++;

    pDataCount[index] = 1;
    ppData[index]     = &pInputData->pCalculatedData->toneMapData.tonemapMode;
    index++;

    pDataCount[index] = 1;
    ppData[index]     = &pInputData->pCalculatedData->colorCorrectionMode;
    index++;

    pDataCount[index] = 3 * 3;
    ppData[index]     = &pInputData->pCalculatedData->CCTransformMatrix;
    index++;

    if (pInputData->pCalculatedData->toneMapData.curvePoints > 0)
    {
        pDataCount[index] = pInputData->pCalculatedData->toneMapData.curvePoints;
        ppData[index]     = &pInputData->pCalculatedData->toneMapData.tonemapCurveBlue;
        index++;

        pDataCount[index] = pInputData->pCalculatedData->toneMapData.curvePoints;
        ppData[index]     = &pInputData->pCalculatedData->toneMapData.tonemapCurveGreen;
        index++;

        pDataCount[index] = pInputData->pCalculatedData->toneMapData.curvePoints;
        ppData[index]     = &pInputData->pCalculatedData->toneMapData.tonemapCurveRed;
        index++;

        WriteDataList(IPEMetadataOutputTags, ppData, pDataCount, NumIPEMetadataOutputTags);
    }
    else
    {
        WriteDataList(IPEMetadataOutputTags, ppData, pDataCount, NumIPEMetadataOutputTags - 3);
    }

    if (IPEProcessingType::IPEMFSRPrefilter == m_instanceProperty.processingType)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE:%d dimension width=%d height=%d ratio=%f",
                        InstanceID(),
                        m_curIntermediateDimension.width,
                        m_curIntermediateDimension.height,
                        m_curIntermediateDimension.ratio);

        // write intermediate dimension
        static const UINT dimensionProps[]            = { PropertyIDIntermediateDimension };
        const UINT        length                      = CAMX_ARRAY_SIZE(dimensionProps);
        const VOID*       pDimensionData[length]      = { &m_curIntermediateDimension };
        UINT              pDimensionDataCount[length] = { sizeof(IntermediateDimensions) };

        result = WriteDataList(dimensionProps, pDimensionData, pDimensionDataCount, length);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_WARN(CamxLogGroupPProc,
                          "IPE:%d failed to write intermediate dimension to property data list with error = %d",
                          InstanceID(), result);
        }

        UINT metaTag = 0;
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.intermediateDimension",
                                                          "IntermediateDimension",
                                                          &metaTag);
        if (CamxResultSuccess == result)
        {
            static const UINT dimensionInfoTag[]        = { metaTag };
            const UINT        dataLength                = CAMX_ARRAY_SIZE(dimensionInfoTag);
            const VOID*       pDimData[dataLength]      = { &m_curIntermediateDimension };
            UINT              pDimDataCount[dataLength] = { sizeof(IntermediateDimensions) };

            result = WriteDataList(dimensionInfoTag, pDimData, pDimDataCount, dataLength);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc,
                               "IPE:%d failed to write intermediate dimension to vendor data list error = %d",
                               InstanceID(), result);
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::ProgramIQConfig()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::ProgramIQConfig(
    ISPInputData* pInputData)
{
    CamxResult      result        = CamxResultSuccess;
    UINT            count         = 0;
    UINT            path          = 0;
    UINT            index         = 0;

    IPEIQModuleData moduleData;

    // Call IQInterface to Set up the Trigger data
    Node* pBaseNode = this;
    IQInterface::IQSetupTriggerData(pInputData, pBaseNode, IsRealTime(), NULL);


    if (SWTMCVersion::TMC11 == m_adrcInfo.version)
    {
        pInputData->triggerData.isTMC11Enabled = TRUE;
    }

    pInputData->triggerData.pADRCData = &m_adrcInfo;

    if (TRUE == GetHwContext()->GetStaticSettings()->enableIPEIQLogging)
    {
        CAMX_LOG_INFO(CamxLogGroupISP, "Begin of dumping IPE Trigger ------");
        IQInterface::s_interpolationTable.IQTriggerDataDump(&pInputData->triggerData);
        CAMX_LOG_INFO(CamxLogGroupISP, "End of dumping IPE Trigger ------");
    }

    for (count = 0; count < m_numIPEIQModulesEnabled; count++)
    {
        if (TRUE == m_adrcInfo.enable)
        {
            // Update AEC Gain values for ADRC use cases, before GTM(includes) will be triggered by shortGain,
            // betweem GTM & LTM(includes) will be by shortGain*power(DRCGain, gtm_perc) and post LTM will be
            // by shortGain*DRCGain
            IQInterface::UpdateAECGain(m_pEnabledIPEIQModule[count]->GetIQType(), pInputData, m_adrcInfo.gtmPercentage);
        }

        result = m_pEnabledIPEIQModule[count]->Execute(pInputData);
        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("%s: Failed to Run IQ Config, count %d", __FUNCTION__, count);
            break;
        }

        m_pEnabledIPEIQModule[count]->GetModuleData(&moduleData);

        switch (m_pEnabledIPEIQModule[count]->GetIQType())
        {
            case ISPIQModuleType::IPELTM:
                m_preLTMLUTOffset[ProgramIndexLTM]      = m_pEnabledIPEIQModule[count]->GetLUTOffset();
                break;
            case ISPIQModuleType::IPEGamma:
                m_postLTMLUTOffset[ProgramIndexGLUT]    = m_pEnabledIPEIQModule[count]->GetLUTOffset();
                break;
            case ISPIQModuleType::IPE2DLUT:
                m_postLTMLUTOffset[ProgramIndex2DLUT]   = m_pEnabledIPEIQModule[count]->GetLUTOffset();
                break;
            case ISPIQModuleType::IPEASF:
                m_postLTMLUTOffset[ProgramIndexASF]     = m_pEnabledIPEIQModule[count]->GetLUTOffset();
                break;
            case ISPIQModuleType::IPEUpscaler:
                m_postLTMLUTOffset[ProgramIndexUpscale] = m_pEnabledIPEIQModule[count]->GetLUTOffset();
                break;
            case ISPIQModuleType::IPEGrainAdder:
                m_postLTMLUTOffset[ProgramIndexGRA]     = m_pEnabledIPEIQModule[count]->GetLUTOffset();
                break;
            case ISPIQModuleType::IPEANR:
                for (UINT passNum = PASS_NAME_FULL; passNum < PASS_NAME_MAX; passNum++)
                {
                    m_ANRPassOffset[passNum] = moduleData.offsetPass[passNum];
                }
                m_ANRSinglePassCmdBufferSize = moduleData.singlePassCmdLength;
                break;
            case ISPIQModuleType::IPEICA:
                path                  = moduleData.IPEPath;
                CAMX_ASSERT((path == IPEPath::REFERENCE) || (path == IPEPath::INPUT));
                index                 =
                    (path == REFERENCE) ? ProgramIndexICA2 : ProgramIndexICA1;
                m_ICALUTOffset[index] = m_pEnabledIPEIQModule[count]->GetLUTOffset();
                break;
            case ISPIQModuleType::IPETF:
                for (UINT passNum = PASS_NAME_FULL; passNum < PASS_NAME_MAX; passNum++)
                {
                    m_TFPassOffset[passNum] = moduleData.offsetPass[passNum];
                }
                m_TFSinglePassCmdBufferSize = moduleData.singlePassCmdLength;
                break;
            default:
                break;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::SetIQModuleNumLUT
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IPENode::SetIQModuleNumLUT(
    ISPIQModuleType type,
    UINT            numLUTs,
    INT             path)
{
    UINT index = 0;
    switch (type)
    {
        case ISPIQModuleType::IPELTM:
            m_preLTMLUTCount[ProgramIndexLTM]      = numLUTs;
            break;
        case ISPIQModuleType::IPEGamma:
            m_postLTMLUTCount[ProgramIndexGLUT]    = numLUTs;
            break;
        case ISPIQModuleType::IPE2DLUT:
            m_postLTMLUTCount[ProgramIndex2DLUT]   = numLUTs;
            break;
        case ISPIQModuleType::IPEASF:
            m_postLTMLUTCount[ProgramIndexASF]     = numLUTs;
            break;
        case ISPIQModuleType::IPEUpscaler:
            m_postLTMLUTCount[ProgramIndexUpscale] = numLUTs;
            break;
        case ISPIQModuleType::IPEGrainAdder:
            m_postLTMLUTCount[ProgramIndexGRA]     = numLUTs;
            break;
        case ISPIQModuleType::IPEICA:
            index                =
                (path == (IPEPath::REFERENCE)) ? ProgramIndexICA2 : ProgramIndexICA1;
            m_ICALUTCount[index] = numLUTs;
            break;
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::UpdateIQCmdSize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IPENode::UpdateIQCmdSize()
{
    IPEProcessingSection section    = IPEProcessingSection::IPEInvalidSection;
    UINT                 numLUTs    = 0;
    ISPIQModuleType      type;
    INT                  path       = -1;
    IPEIQModuleData      moduleData;

    for (UINT count = 0; count < m_numIPEIQModulesEnabled; count++)
    {
        numLUTs = m_pEnabledIPEIQModule[count]->GetNumLUT();
        type    = m_pEnabledIPEIQModule[count]->GetIQType();
        section = GetModuleProcessingSection(type);
        switch (section)
        {
            case IPEProcessingSection::IPEPPSPreLTM:
                m_maxCmdBufferSizeBytes[CmdBufferPreLTM]      += m_pEnabledIPEIQModule[count]->GetIQCmdLength();
                m_maxCmdBufferSizeBytes[CmdBufferDMIHeader]   += numLUTs *
                    cdm_get_cmd_header_size(CDMCmdDMI) * RegisterWidthInBytes;
                break;
            case IPEProcessingSection::IPEPPSPostLTM:
                m_maxCmdBufferSizeBytes[CmdBufferPostLTM]     += m_pEnabledIPEIQModule[count]->GetIQCmdLength();
                m_maxCmdBufferSizeBytes[CmdBufferDMIHeader]   += numLUTs *
                    cdm_get_cmd_header_size(CDMCmdDMI) * RegisterWidthInBytes;
                break;
            case IPEProcessingSection::IPENPS:
                m_maxCmdBufferSizeBytes[CmdBufferNPS]         +=
                    m_pEnabledIPEIQModule[count]->GetIQCmdLength();
                m_maxCmdBufferSizeBytes[CmdBufferDMIHeader]   +=
                    numLUTs * cdm_get_cmd_header_size(CDMCmdDMI) * RegisterWidthInBytes;
                m_pEnabledIPEIQModule[count]->GetModuleData(&moduleData);
                path                                           = moduleData.IPEPath;
                CAMX_ASSERT((path == IPEPath::REFERENCE) || (path == IPEPath::INPUT));
                break;
            default:
                CAMX_LOG_WARN(CamxLogGroupPProc, "%s: invalid module type %d", __FUNCTION__, type);
                break;
        }
        SetIQModuleNumLUT(type, numLUTs, path);
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::IsFDEnabled
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPENode::IsFDEnabled(
    VOID)
{
    BOOL bIsFDPostingResultsEnabled = FALSE;

    // Set offset to 1 to point to the previous request.
    GetFDPerFrameMetaDataSettings(1, &bIsFDPostingResultsEnabled, NULL);

    return bIsFDPostingResultsEnabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::SetDependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::SetDependencies(
    ExecuteProcessRequestData*  pExecuteProcessRequestData,
    UINT                        parentNodeId)
{
    NodeProcessRequestData* pNodeRequestData    = pExecuteProcessRequestData->pNodeProcessRequestData;
    UINT32                  count               = 0;
    UINT32                  metaTagFDRoi        = 0;
    UINT32                  result              = CamxResultSuccess;
    BOOL                    isSWEISEnabled      = FALSE;

    if ((m_hwMask == ISPHwTitan150) &&
        ((0 != (IPEStabilizationType::IPEStabilizationTypeSWEIS2 & m_instanceProperty.stabilizationType)) ||
         (0 != (IPEStabilizationType::IPEStabilizationTypeSWEIS3 & m_instanceProperty.stabilizationType))))
    {
        isSWEISEnabled = TRUE;
    }

    result = VendorTagManager::QueryVendorTagLocation(VendorTagSectionOEMFDResults,
           VendorTagNameOEMFDResults, &metaTagFDRoi);

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE: ProcessRequest: Setting dependency for Req#%llu",
                     pNodeRequestData->pCaptureRequest->requestId);

    UINT64 requestIdOffsetFromLastFlush = GetRequestIdOffsetFromLastFlush(pNodeRequestData->pCaptureRequest->requestId);
    if (FirstValidRequestId < requestIdOffsetFromLastFlush)
    {
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = GetNodeCompleteProperty();
        // Always point to the previous request. Should NOT be tied to the pipeline delay!
        pNodeRequestData->dependencyInfo[0].propertyDependency.offsets[count] = 1;
        count++;
    }

    if (parentNodeId == IFE)
    {
        UINT32 metaTagAppliedCrop = 0;
        if (CamxResultSuccess == VendorTagManager::QueryVendorTagLocation("org.quic.camera.ifecropinfo",
                                "AppliedCrop", &metaTagAppliedCrop) &&
            (TRUE == IsTagPresentInPublishList(metaTagAppliedCrop)))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = metaTagAppliedCrop;
        }
    }

    if (TRUE == m_isStatsNodeAvailable)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Set dependency for real time pipeline");

        // 3A dependency
        if (TRUE == IsTagPresentInPublishList(PropertyIDAECFrameControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAECFrameControl;

        }
        if (TRUE == IsTagPresentInPublishList(PropertyIDAECStatsControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAECStatsControl;
        }

        if (TRUE == IsTagPresentInPublishList(PropertyIDAWBFrameControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAWBFrameControl;
        }

        if (TRUE == IsTagPresentInPublishList(PropertyIDAWBStatsControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAWBStatsControl;
        }

        if (TRUE == IsTagPresentInPublishList(PropertyIDAFStatsControl))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDAFStatsControl;
        }

        if ((parentNodeId == IFE) || isSWEISEnabled)
        {
            // IFE dependency
            if (TRUE == IsTagPresentInPublishList(PropertyIDIFEDigitalZoom))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDIFEDigitalZoom;
            }
            if (TRUE == IsTagPresentInPublishList(PropertyIDIFEScaleOutput))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDIFEScaleOutput;
            }
            if (TRUE == IsTagPresentInPublishList(PropertyIDIFEGammaOutput))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDIFEGammaOutput;
            }
            if ((TRUE == m_instanceProperty.enableFOVC) && (TRUE == IsTagPresentInPublishList(PropertyIDFOVCFrameInfo)))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDFOVCFrameInfo;
            }

            if ((result == CamxResultSuccess) && (TRUE == IsTagPresentInPublishList(metaTagFDRoi)) && IsFDEnabled() &&
                ((pNodeRequestData->pCaptureRequest->requestId) > GetStaticSettings()->minReqFdDependency))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.offsets[count] = 1;
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = metaTagFDRoi;
            }

        }
        else if (IsNodeInPipeline(BPS))
        {
            if (TRUE == IsTagPresentInPublishList(PropertyIDBPSGammaOutput))
            {
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDBPSGammaOutput;
            }
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "parent Node is not IFE in real time pipeline id %d", parentNodeId);
        }
    }
    else
    {
        if (TRUE == m_OEMStatsSettingEnable)
        {
            if ((parentNodeId == IFE) || isSWEISEnabled)
            {
                if (TRUE == IsTagPresentInPublishList(PropertyIDIFEDigitalZoom))
                {
                    pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDIFEDigitalZoom;
                }
                if (TRUE == IsTagPresentInPublishList(PropertyIDIFEGammaOutput))
                {
                    pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDIFEGammaOutput;
                }
            }
            else if (parentNodeId == BPS)
            {
                if (TRUE == IsTagPresentInPublishList(PropertyIDBPSGammaOutput))
                {
                    pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDBPSGammaOutput;
                }
            }
            else
            {
                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "parent Node is not IFE/BPS in OEMSetting pipeline id %d", parentNodeId);
            }
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Set dependency for none real time pipeline");

            if (IsNodeInPipeline(BPS))
            {
                // BPS dependency
                if (TRUE == IsTagPresentInPublishList(PropertyIDBPSGammaOutput))
                {
                    pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = PropertyIDBPSGammaOutput;
                }
            }
        }
    }

    // Don't need to add parent check, because ipe just use this property when parent node is chinode.
    if (TRUE == m_instanceProperty.enableCHICropInfoPropertyDependency)
    {
        UINT32 metaTag = 0;
        if (CDKResultSuccess == VendorTagManager::QueryVendorTagLocation("com.qti.cropregions",
                                                                         "ChiNodeResidualCrop", &metaTag) &&
            (TRUE == IsTagPresentInPublishList(metaTag)))
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] = metaTag;
        }
    }

    if (0 < count)
    {
        /// @todo (CAMX-739) Need to add AWB update support and other dependency here
        pNodeRequestData->dependencyInfo[0].propertyDependency.count = count;
    }

    // Set Dependency for ADRC Info.
    SetADRCDependencies(pNodeRequestData);

    // ICA dependency needed for offline pipeline in case of MFNR / MFSR uscases
    SetICADependencies(pNodeRequestData);

    // multi-camera
    SetMultiCameraMasterDependency(pNodeRequestData);

    if (0 < pNodeRequestData->dependencyInfo[0].propertyDependency.count)
    {
        pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency = TRUE;
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

        SetInputBuffersReadyDependency(pExecuteProcessRequestData, 0);
    }

    // ExecuteProcessRequest always requires sequenceId 1, purposefully reporting dep regardless of having a dependency or not
    pNodeRequestData->numDependencyLists                                                    = 1;
    pNodeRequestData->dependencyInfo[0].processSequenceId                                   = 1;
    pNodeRequestData->dependencyInfo[0].dependencyFlags.hasIOBufferAvailabilityDependency   = TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::CreateFWCommandBufferManagers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::CreateFWCommandBufferManagers()
{
    ResourceParams               resourceParams[IPEMaxFWCmdBufferManagers];
    CHAR                         bufferManagerName[IPEMaxFWCmdBufferManagers][MaxStringLength256];
    struct CmdBufferManagerParam createParam[IPEMaxFWCmdBufferManagers];
    UINT32                       numberOfBufferManagers = 0;
    CamxResult                   result                 = CamxResultSuccess;

    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                        m_maxCmdBufferSizeBytes[CmdBufferStriping],
                        CmdType::FW,
                        CSLMemFlagUMDAccess,
                        0,
                        &m_deviceIndex,
                        m_IPECmdBlobCount);
    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                      sizeof(CHAR) * MaxStringLength256,
                      "CBM_%s_%s",
                      NodeIdentifierString(),
                      "CmdBufferStriping");
    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pIPECmdBufferManager[CmdBufferStriping];

    numberOfBufferManagers++;

    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                        m_maxCmdBufferSizeBytes[CmdBufferBLMemory],
                        CmdType::FW,
                        CSLMemFlagUMDAccess,
                        0,
                        &m_deviceIndex,
                        m_IPECmdBlobCount);
    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                      sizeof(CHAR) * MaxStringLength256,
                      "CBM_%s_%s",
                      NodeIdentifierString(),
                      "CmdBufferBLMemory");
    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pIPECmdBufferManager[CmdBufferBLMemory];

    numberOfBufferManagers++;

    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                        CmdBufferFrameProcessSizeBytes,
                        CmdType::FW,
                        CSLMemFlagUMDAccess,
                        IPEMaxTopCmdBufferPatchAddress,
                        &m_deviceIndex,
                        m_IPECmdBlobCount);
    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                      sizeof(CHAR) * MaxStringLength256,
                      "CBM_%s_%s",
                      NodeIdentifierString(),
                      "CmdBufferFrameProcess");
    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pIPECmdBufferManager[CmdBufferFrameProcess];

    numberOfBufferManagers++;

    FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                        sizeof(IpeIQSettings),
                        CmdType::FW,
                        CSLMemFlagUMDAccess,
                        0,
                        &m_deviceIndex,
                        m_IPECmdBlobCount);
    OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                      sizeof(CHAR) * MaxStringLength256,
                      "CBM_%s_%s",
                      NodeIdentifierString(),
                      "CmdBufferIQSettings");
    createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
    createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
    createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pIPECmdBufferManager[CmdBufferIQSettings];

    numberOfBufferManagers++;

    if (m_maxCmdBufferSizeBytes[CmdBufferNPS] > 0)
    {
        FillCmdBufferParams(&resourceParams[numberOfBufferManagers],
                            m_maxCmdBufferSizeBytes[CmdBufferNPS],
                            CmdType::FW,
                            CSLMemFlagUMDAccess,
                            IPEMaxNPSPatchAddress,
                            &m_deviceIndex,
                            m_IPECmdBlobCount);
        OsUtils::SNPrintF(bufferManagerName[numberOfBufferManagers],
                          sizeof(CHAR) * MaxStringLength256,
                          "CBM_%s_%s",
                          NodeIdentifierString(),
                          "CmdBufferNPS");
        createParam[numberOfBufferManagers].pBufferManagerName = bufferManagerName[numberOfBufferManagers];
        createParam[numberOfBufferManagers].pParams            = &resourceParams[numberOfBufferManagers];
        createParam[numberOfBufferManagers].ppCmdBufferManager = &m_pIPECmdBufferManager[CmdBufferNPS];

        numberOfBufferManagers++;
    }

    if (numberOfBufferManagers > IPEMaxFWCmdBufferManagers)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Maxmimum FW Cmd buffers reached");
        result = CamxResultEFailed;
    }

    if ((CamxResultSuccess == result) &&  (0 != numberOfBufferManagers))
    {
        result = CreateMultiCmdBufferManager(createParam, numberOfBufferManagers);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "FW Cmd Buffer Creation failed result %d", result);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::InitializeStripingParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::InitializeStripingParams(
    IpeConfigIoData* pConfigIOData)
{
    CamxResult     result       = CamxResultSuccess;
    ResourceParams params       = { 0 };
    UINT32         titanVersion = static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion();
    UINT32         hwVersion    = static_cast<Titan17xContext *>(GetHwContext())->GetHwVersion();

    CAMX_ASSERT(NULL != pConfigIOData);

    // Check if striping in UMD is enabled before creating striping library context
    result = IPEStripingLibraryContextCreate(pConfigIOData,
                                             NULL,
                                             titanVersion,
                                             hwVersion,
                                             &m_hStripingLib,
                                             &m_maxCmdBufferSizeBytes[CmdBufferStriping],
                                             &m_maxCmdBufferSizeBytes[CmdBufferBLMemory]);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Stripinglib ctxt failed result %d,ConfigIO %p,titanversion 0x%x,hwversion 0x%x",
                           result, pConfigIOData, titanVersion, hwVersion);
    }
    else
    {
        result = CreateFWCommandBufferManagers();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::DeInitializeStripingParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::DeInitializeStripingParams()
{
    CamxResult result = CamxResultSuccess;

    if (NULL != m_hStripingLib)
    {
        INT32 rc = IPEStripingLibraryContextDestroy(&m_hStripingLib);
        if (rc != 0)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d Cannot destroy Striping Library with rc=%d", InstanceID(), rc);
            result = CamxResultEFailed;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::FillStripingParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillStripingParams(
    IpeFrameProcessData*         pFrameProcessData,
    IpeIQSettings*               pIPEIQsettings,
    CmdBuffer**                  ppIPECmdBuffer,
    CSLICPClockBandwidthRequest* pICPClockBandwidthRequest)
{
    CamxResult                     result          = CamxResultSuccess;
    IPEStripingLibExecuteParams    stripeParams    = { 0 };
    UINT32*                        pStripeMem      = NULL;
    UINT32                         offset;
    IPEStripingLibExecuteMetaData metaDataBuffer   = { 0 };
    userIPEArgs                   userIPEArgs      = {0};
    CAMX_ASSERT(NULL != ppIPECmdBuffer[CmdBufferStriping]);
    pStripeMem = reinterpret_cast<UINT32*>(
        ppIPECmdBuffer[CmdBufferStriping]->BeginCommands(m_maxCmdBufferSizeBytes[CmdBufferStriping] / 4));

#if DEBUG
    if (TRUE != IsRealTime())
    {
        DumpConfigIOData();
    }
#endif // if (TRUE != IsRealTime())

    if (NULL != pStripeMem)
    {
        stripeParams.iq                 = pIPEIQsettings;
        stripeParams.ica1               = &pIPEIQsettings->ica1Parameters;
        stripeParams.ica2               = &pIPEIQsettings->ica2Parameters;
        stripeParams.zoom               = &pIPEIQsettings->ica1Parameters.zoomWindow;
        stripeParams.prevZoom           = &pIPEIQsettings->ica2Parameters.zoomWindow;
        stripeParams.maxNumOfCoresToUse = pFrameProcessData->maxNumOfCoresToUse;

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "## node %s,"
                         "rt %d blob %u processingType %d, profile %d, stab %d, ica1 pers %u, grid %u, ica2 persp %u, grid %u"
                         "anr %d %d %d %d, tf %d, %d, %d %d, cac %d, ltm %d, cc %d glut %d lut %d, chromaEn %d,chromasup %d"
                         "skin %d, asf %d, gra %d, colortransform %d refine %d, %d, %d, numcores %d, request %d",
                         NodeIdentifierString(),
                         IsRealTime(), m_IPECmdBlobCount, m_instanceProperty.processingType,
                         m_instanceProperty.profileId, m_instanceProperty.stabilizationType,
                         pIPEIQsettings->ica1Parameters.isPerspectiveEnable,
                         pIPEIQsettings->ica1Parameters.isGridEnable,
                         pIPEIQsettings->ica2Parameters.isPerspectiveEnable,
                         pIPEIQsettings->ica2Parameters.isGridEnable,
                         pIPEIQsettings->anrParameters.parameters[0].moduleCfg.EN,
                         pIPEIQsettings->anrParameters.parameters[1].moduleCfg.EN,
                         pIPEIQsettings->anrParameters.parameters[2].moduleCfg.EN,
                         pIPEIQsettings->anrParameters.parameters[3].moduleCfg.EN,
                         pIPEIQsettings->tfParameters.parameters[PASS_NAME_FULL].moduleCfg.EN,
                         pIPEIQsettings->tfParameters.parameters[PASS_NAME_DC_4].moduleCfg.EN,
                         pIPEIQsettings->tfParameters.parameters[PASS_NAME_DC_16].moduleCfg.EN,
                         pIPEIQsettings->tfParameters.parameters[PASS_NAME_DC_64].moduleCfg.EN,
                         pIPEIQsettings->cacParameters.moduleCfg.EN,
                         pIPEIQsettings->ltmParameters.moduleCfg.EN,
                         pIPEIQsettings->colorCorrectParameters.moduleCfg.EN,
                         pIPEIQsettings->glutParameters.moduleCfg.EN,
                         pIPEIQsettings->lut2dParameters.moduleCfg.EN,
                         pIPEIQsettings->chromaEnhancementParameters.moduleCfg.EN,
                         pIPEIQsettings->chromaSupressionParameters.moduleCfg.EN,
                         pIPEIQsettings->skinEnhancementParameters.moduleCfg.EN,
                         pIPEIQsettings->asfParameters.moduleCfg.EN,
                         pIPEIQsettings->graParameters.moduleCfg.EN,
                         pIPEIQsettings->colorTransformParameters.moduleCfg.EN,
                         pIPEIQsettings->refinementParameters.dc[0].refinementCfg.TRENABLE,
                         pIPEIQsettings->refinementParameters.dc[1].refinementCfg.TRENABLE,
                         pIPEIQsettings->refinementParameters.dc[2].refinementCfg.TRENABLE,
                         stripeParams.maxNumOfCoresToUse,
                         pFrameProcessData->requestId);

        userIPEArgs.dumpEnable          = 0;
        userIPEArgs.frameNumber         = pFrameProcessData->requestId;
        userIPEArgs.instance            = InstanceID();
        userIPEArgs.processingType      = m_instanceProperty.processingType;
        userIPEArgs.profileID           = m_instanceProperty.profileId;
        userIPEArgs.realTime            = CheckIsIPERealtime();
        userIPEArgs.FileDumpPath        = FileDumpPath;
        userIPEArgs.dumpEnable          = m_enableIPEStripeDump;

        result = IPEStripingLibraryExecute(m_hStripingLib, &stripeParams, pStripeMem, &metaDataBuffer, &userIPEArgs);
        if (CamxResultSuccess == result)
        {
            offset =
                static_cast<UINT32>(offsetof(IpeFrameProcess, cmdData)) +
                static_cast<UINT32>(offsetof(IpeFrameProcessData, stripingLibOutAddr));
            result = ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(
                offset, ppIPECmdBuffer[CmdBufferStriping], 0);
            pICPClockBandwidthRequest->frameCycles = metaDataBuffer.pixelCount;
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Num pixels = %d", metaDataBuffer.pixelCount);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc,
                           "IPE:%d Striping Library execution failed %d",
                           InstanceID(), result);
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
// IPENode::InitializeIPEIQSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE VOID IPENode::InitializeIPEIQSettings(
    IpeIQSettings*               pIPEIQsettings)
{
    CamxResult                     result = CamxResultSuccess;

    Utils::Memset(&pIPEIQsettings->ica1Parameters.zoomWindow, 0x0, sizeof(IpeZoomWindow));
    Utils::Memset(&pIPEIQsettings->ica1Parameters.ifeZoomWindow, 0x0, sizeof(IpeZoomWindow));
    Utils::Memset(&pIPEIQsettings->ica2Parameters.zoomWindow, 0x0, sizeof(IpeZoomWindow));
    Utils::Memset(&pIPEIQsettings->ica2Parameters.ifeZoomWindow, 0x0, sizeof(IpeZoomWindow));

    for (UINT16 i = 0; i < PASS_NAME_MAX; i++)
    {
        pIPEIQsettings->anrParameters.parameters[i].moduleCfg.EN    = 0;
        pIPEIQsettings->tfParameters.parameters[i].moduleCfg.EN     = 0;
    }

    pIPEIQsettings->ica1Parameters.isGridEnable                 = 0;
    pIPEIQsettings->ica1Parameters.isPerspectiveEnable          = 0;
    pIPEIQsettings->ica2Parameters.isGridEnable                 = 0;
    pIPEIQsettings->ica2Parameters.isPerspectiveEnable          = 0;
    pIPEIQsettings->cacParameters.moduleCfg.EN                  = 0;
    pIPEIQsettings->ltmParameters.moduleCfg.EN                  = 0;
    pIPEIQsettings->colorCorrectParameters.moduleCfg.EN         = 0;
    pIPEIQsettings->glutParameters.moduleCfg.EN                 = 0;
    pIPEIQsettings->lut2dParameters.moduleCfg.EN                = 0;
    pIPEIQsettings->chromaEnhancementParameters.moduleCfg.EN    = 0;
    pIPEIQsettings->chromaSupressionParameters.moduleCfg.EN     = 0;
    pIPEIQsettings->skinEnhancementParameters.moduleCfg.EN      = 0;
    pIPEIQsettings->asfParameters.moduleCfg.EN                  = 0;
    pIPEIQsettings->graParameters.moduleCfg.EN                  = 0;
    pIPEIQsettings->colorTransformParameters.moduleCfg.EN       = 0;
    pIPEIQsettings->colorCorrectParameters.moduleCfg.EN         = 0;

    for (UINT16 i = 0; i < (PASS_NAME_MAX - 1); i++)
    {
        pIPEIQsettings->refinementParameters.dc[i].refinementCfg.TRENABLE = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::PatchBLMemoryBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::PatchBLMemoryBuffer(
    IpeFrameProcessData* pFrameProcessData,
    CmdBuffer**          ppIPECmdBuffer)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != ppIPECmdBuffer[CmdBufferBLMemory]) && (0 != m_maxCmdBufferSizeBytes[CmdBufferBLMemory]))
    {
        pFrameProcessData->cdmBufferSize = m_maxCmdBufferSizeBytes[CmdBufferBLMemory];

        UINT offset =
            static_cast<UINT32>(offsetof(IpeFrameProcess, cmdData)) +
            static_cast<UINT32>(offsetof(IpeFrameProcessData, cdmBufferAddress));

        result = ppIPECmdBuffer[CmdBufferFrameProcess]->AddNestedCmdBufferInfo(offset,
                                                                               ppIPECmdBuffer[CmdBufferBLMemory],
                                                                               0);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetFaceROI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetFaceROI(
    ISPInputData* pInputData,
    UINT          parentNodeId)
{
    CamxResult    result                      = CamxResultSuccess;
    FaceROIInformation faceRoiData            = {};
    RectangleCoordinate* pRoiRect             = NULL;
    CHIRectangle  roiRect                     = {};
    CHIRectangle  cropInfo                    = {};
    CHIDimension  currentFrameDimension       = {};
    CHIRectangle  currentFrameMapWrtReference = {};
    CHIRectangle  roiWrtReferenceFrame        = {};
    UINT32        metaTagFDRoi                = 0;
    BOOL          bIsFDPostingResultsEnabled  = FALSE;

    GetFDPerFrameMetaDataSettings((BPS == parentNodeId) ? 0 : 2, &bIsFDPostingResultsEnabled, NULL);
    if (TRUE == bIsFDPostingResultsEnabled)
    {
        result = VendorTagManager::QueryVendorTagLocation(VendorTagSectionOEMFDResults,
                                                          VendorTagNameOEMFDResults, &metaTagFDRoi);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "query FD vendor failed result %d",
                result);
            return CamxResultSuccess;
        }

        UINT              GetProps[]              = { metaTagFDRoi };
        static const UINT GetPropsLength          = CAMX_ARRAY_SIZE(GetProps);
        VOID*             pData[GetPropsLength]   = { 0 };
        UINT64            offsets[GetPropsLength] = { 0 };

        if (FALSE == IsRealTime())
        {
            GetProps[0] |= InputMetadataSectionMask;
        }

        offsets[0] = (BPS == parentNodeId) ? 0 : 2;
        GetDataList(GetProps, pData, offsets, GetPropsLength);

        if (NULL != pData[0])
        {
            Utils::Memcpy(&faceRoiData, pData[0], sizeof(FaceROIInformation));

            // Translate face roi if BPS is not parent
            if (BPS != parentNodeId)
            {
                PortCropInfo portCropInfo = { { 0 } };
                if (CamxResultSuccess == Node::GetPortCrop(this, IPEInputPortFull, &portCropInfo, NULL))
                {
                    cropInfo                            = portCropInfo.appliedCrop;
                    currentFrameMapWrtReference.top     = cropInfo.top;
                    currentFrameMapWrtReference.left    = cropInfo.left;
                    currentFrameMapWrtReference.width   = cropInfo.width;
                    currentFrameMapWrtReference.height  = cropInfo.height;

                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "currentFrameWrtReference T:%d L:%d W:%d H:%d",
                                     currentFrameMapWrtReference.top, currentFrameMapWrtReference.left,
                                     currentFrameMapWrtReference.width, currentFrameMapWrtReference.height);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "no applied crop data");
                    result = CamxResultEFailed;
                }

                if (CamxResultSuccess == result)
                {
                    // Input width/height
                    currentFrameDimension.width  = m_fullInputWidth;
                    currentFrameDimension.height = m_fullInputHeight;
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "current dim W:%d H:%d",
                                     currentFrameDimension.width, currentFrameDimension.height);
                }
            }

            if (CamxResultSuccess == result)
            {
                pInputData->fDData.numberOfFace = static_cast<UINT16>(
                    (faceRoiData.ROICount > MAX_FACE_NUM) ? MAX_FACE_NUM : faceRoiData.ROICount);

                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Face ROI is published face num %d max %d",
                                 pInputData->fDData.numberOfFace, MAX_FACE_NUM);

                for (UINT16 i = 0; i < pInputData->fDData.numberOfFace; i++)
                {
                    pRoiRect = &faceRoiData.unstabilizedROI[i].faceRect;

                    roiWrtReferenceFrame.left   = pRoiRect->left;
                    roiWrtReferenceFrame.top    = pRoiRect->top;
                    roiWrtReferenceFrame.width  = pRoiRect->width;
                    roiWrtReferenceFrame.height = pRoiRect->height;

                    // If BPS is parent, no conversion would be performed
                    roiRect = Translator::ConvertROIFromReferenceToCurrent(
                        &currentFrameDimension, &currentFrameMapWrtReference, &roiWrtReferenceFrame);

                    pInputData->fDData.faceCenterX[i] = static_cast<INT16>(roiRect.left + (roiRect.width / 2));
                    pInputData->fDData.faceCenterY[i] = static_cast<INT16>(roiRect.top + (roiRect.height / 2));
                    pInputData->fDData.faceRadius[i]  = static_cast<INT16>(Utils::MinUINT32(roiRect.width, roiRect.height));

                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, " center x:%d y:%d r:%d",
                        pInputData->fDData.faceCenterX[i], pInputData->fDData.faceCenterY[i], pInputData->fDData.faceRadius[i]);
                }
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Face ROI is not published");
        }
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::SetAAAInputData()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::SetAAAInputData(
    ISPInputData* pInputData,
    UINT          parentNodeID)
{
    VOID*   pData[IPEVendorTagMax]   = { 0 };
    UINT64  offsets[IPEVendorTagMax] = { 0 };

    if (TRUE == m_isStatsNodeAvailable)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Get 3A properties for realtime pipeline");

        // Note, the property order should matched what defined in the enum IPEVendorTagId
        static const UINT Properties3A[] =
        {
            PropertyIDAECStatsControl,
            PropertyIDAWBStatsControl,
            PropertyIDAFStatsControl,
            PropertyIDAECFrameControl,
            PropertyIDAWBFrameControl,
            PropertyIDSensorNumberOfLEDs,
        };
        static const UINT PropertySize = CAMX_ARRAY_SIZE(Properties3A);
        GetDataList(Properties3A, pData, offsets, PropertySize);
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Get 3A properties for non realtime pipeline");
        static const UINT Properties3A[] =
        {
            PropertyIDAECStatsControl    | InputMetadataSectionMask,
            PropertyIDAWBStatsControl    | InputMetadataSectionMask,
            PropertyIDAFStatsControl     | InputMetadataSectionMask,
            PropertyIDAECFrameControl    | InputMetadataSectionMask,
            PropertyIDAWBFrameControl    | InputMetadataSectionMask,
            PropertyIDSensorNumberOfLEDs | InputMetadataSectionMask,
        };
        static const UINT PropertySize = CAMX_ARRAY_SIZE(Properties3A);
        GetDataList(Properties3A, pData, offsets, PropertySize);
    }

    if (NULL != pData[IPEVendorTagAECStats])
    {
        Utils::Memcpy(pInputData->pAECStatsUpdateData, pData[IPEVendorTagAECStats], sizeof(AECStatsControl));
    }

    if (NULL != pData[IPEVendorTagAWBStats])
    {
        Utils::Memcpy(pInputData->pAWBStatsUpdateData, pData[IPEVendorTagAWBStats], sizeof(AWBStatsControl));
    }

    if (NULL != pData[IPEVendorTagAFStats])
    {
        Utils::Memcpy(pInputData->pAFStatsUpdateData, pData[IPEVendorTagAFStats], sizeof(AFStatsControl));
    }

    if (NULL != pData[IPEVendorTagAECFrame])
    {
        Utils::Memcpy(pInputData->pAECUpdateData, pData[IPEVendorTagAECFrame], sizeof(AECFrameControl));
    }

    if (NULL != pData[IPEVendorTagAWBFrame])
    {
        Utils::Memcpy(pInputData->pAWBUpdateData, pData[IPEVendorTagAWBFrame], sizeof(AWBFrameControl));
    }

    if (NULL != pData[IPEVendorTagFlashNumber])
    {
        pInputData->numberOfLED = *reinterpret_cast<UINT16*>(pData[IPEVendorTagFlashNumber]);
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Number of led %d", pInputData->numberOfLED);
    }

    pInputData->parentNodeID = parentNodeID;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::HardcodeSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::HardcodeSettings(
    ISPInputData* pInputData)
{
    pInputData->pipelineIPEData.hasTFRefInput             = 0;
    pInputData->pipelineIPEData.numOfFrames               = 2;
    pInputData->pipelineIPEData.isDigitalZoomEnabled      = 0;
    pInputData->pipelineIPEData.upscalingFactorMFSR       = 1.0f;
    pInputData->pipelineIPEData.digitalZoomStartX         = 0;
    pInputData->pipelineIPEData.digitalZoomStartY         = 0;
    pInputData->pipelineIPEData.pWarpGeometryData         = NULL;
    pInputData->lensPosition                              = 1.0f;
    pInputData->lensZoom                                  = 1.0f;
    pInputData->preScaleRatio                             = 1.0f;
    pInputData->postScaleRatio                            = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::SetICADependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::SetICADependencies(
    NodeProcessRequestData*  pNodeRequestData)
{
    // @todo (CAMX-2690) Provide this input based on usecase local variable setting to zero for now
    UINT32 count         = pNodeRequestData->dependencyInfo[0].propertyDependency.count;

    if ((TRUE == IsBlendWithNPS()) || (TRUE == IsPostfilterWithNPS()))
    {
        pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency = TRUE;
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count]  = m_IPEICATAGLocation[0];

        if (CSLCameraTitanVersion::CSLTitan150 == (static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion()))
        {
            // For Talos Perspective Transform should be performed on ICA2
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count]  = m_IPEICATAGLocation[3];
        }

        count++;
    }

    if (TRUE == IsLoopBackPortEnabled())
    {
        pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency = TRUE;
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count]  = m_IPEICATAGLocation[6];
        count++;
    }

    if (0 != (IPEStabilizationType::IPEStabilizationTypeEIS2 & m_instanceProperty.stabilizationType))
    {
        pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency = TRUE;
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count]  = m_IPEICATAGLocation[0];
        count++;
        //  Use grid flag and stabilization type to enable this property
        if (TRUE == static_cast<Titan17xContext*>(GetHwContext())->GetTitan17xSettingsManager()->
            GetTitan17xStaticSettings()->enableICAInGrid)
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = m_IPEICATAGLocation[1];
            count++;
        }
    }

    if (0 != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType))
    {
        pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency = TRUE;
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count]  = m_IPEICATAGLocation[7];
        count++;

        //  Use grid flag and stabilization type to enable this property
        if (TRUE == static_cast<Titan17xContext*>(GetHwContext())->GetTitan17xSettingsManager()->
            GetTitan17xStaticSettings()->enableICAInGrid)
        {
            pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count] = m_IPEICATAGLocation[8];
            count++;
        }
    }

    if (TRUE == m_OEMICASettingEnable)
    {
        pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency = TRUE;
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count]  = m_IPEICATAGLocation[1];
        count++;

        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count]  = m_IPEICATAGLocation[4];
        count++;
    }
    pNodeRequestData->dependencyInfo[0].propertyDependency.count = count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::SetADRCDependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::SetADRCDependencies(
    NodeProcessRequestData*  pNodeRequestData)
{
    UINT32 count     = pNodeRequestData->dependencyInfo[0].propertyDependency.count;
    if (TRUE == IsNodeInPipeline(IFE) && (TRUE == IsTagPresentInPublishList(PropertyIDIFEADRCInfoOutput)))
    {
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] =
            PropertyIDIFEADRCInfoOutput;
    }
    else if (TRUE == IsNodeInPipeline(BPS) && (TRUE == IsTagPresentInPublishList(PropertyIDBPSADRCInfoOutput)))
    {
        pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count++] =
            PropertyIDBPSADRCInfoOutput;
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Don't need Set ADRC Dependency");
    }
    pNodeRequestData->dependencyInfo[0].propertyDependency.count = count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::UpdateICADependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::UpdateICADependencies(
    ISPInputData* pInputData)
{
    CamxResult     result                               = CamxResultSuccess;
    // Perspective transform dependency for ICA1
    VOID*          pPropertyDataICA[IPEProperties]      = { 0 };
    UINT64         propertyDataICAOffset[IPEProperties] = { 0 };
    UINT           length                               = CAMX_ARRAY_SIZE(m_IPEICATAGLocation);;

    CAMX_ASSERT(length == IPEProperties);

    GetDataList(m_IPEICATAGLocation, pPropertyDataICA, propertyDataICAOffset, length);

    if (NULL != pPropertyDataICA[0])
    {
        if ((IPEProcessingType::IPEMFNRPostfilter == m_instanceProperty.processingType) &&
            (IPEProfileId::IPEProfileIdDefault == m_instanceProperty.profileId))
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "Skip ICA transforms for MFNR  NPS Postfilter final stage");
        }
        else if ((IPEProcessingType::IPEMFSRPostfilter == m_instanceProperty.processingType) &&
            (IPEProfileId::IPEProfileIdDefault == m_instanceProperty.profileId))
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "Skip ICA transforms for MFSR  NPS Postfilter final stage");
        }
        else if (0 != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType))
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Skip EISv2 ICA transforms for EISv3");
        }
        else if (TRUE == IsValidInputPrespectiveParams())
        {
            Utils::Memcpy(&pInputData->ICAConfigData.ICAInPerspectiveParams,
                reinterpret_cast<IPEICAPerspectiveTransform*>(pPropertyDataICA[0]),
                sizeof(pInputData->ICAConfigData.ICAInPerspectiveParams));

            CAMX_LOG_INFO(CamxLogGroupPProc, "perspective IN %d , w %d, h %d, c %d, r %d, frameNum %llu",
                          pInputData->ICAConfigData.ICAInPerspectiveParams.perspectiveTransformEnable,
                          pInputData->ICAConfigData.ICAInPerspectiveParams.transformDefinedOnWidth,
                          pInputData->ICAConfigData.ICAInPerspectiveParams.transformDefinedOnHeight,
                          pInputData->ICAConfigData.ICAInPerspectiveParams.perspetiveGeometryNumColumns,
                          pInputData->ICAConfigData.ICAInPerspectiveParams.perspectiveGeometryNumRows,
                          pInputData->frameNum);
        }

    }

    if (NULL != pPropertyDataICA[1])
    {
        if ((0 != (IPEStabilizationType::IPEStabilizationTypeEIS2 & m_instanceProperty.stabilizationType)) &&
            (TRUE == static_cast<Titan17xContext*>(
             GetHwContext())->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->enableICAInGrid))
        {
            Utils::Memcpy(&pInputData->ICAConfigData.ICAInGridParams,
                reinterpret_cast<IPEICAGridTransform*>(pPropertyDataICA[1]),
                sizeof(pInputData->ICAConfigData.ICAInGridParams));

            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "grid IN %d , w %d, h %d, corner %d, frameNum %llu",
                             pInputData->ICAConfigData.ICAInGridParams.gridTransformEnable,
                             pInputData->ICAConfigData.ICAInGridParams.transformDefinedOnWidth,
                             pInputData->ICAConfigData.ICAInGridParams.transformDefinedOnHeight,
                             pInputData->ICAConfigData.ICAInGridParams.gridTransformArrayExtrapolatedCorners,
                             pInputData->frameNum);
        }
    }

    if (NULL != pPropertyDataICA[2])
    {
        Utils::Memcpy(&pInputData->ICAConfigData.ICAInInterpolationParams,
            reinterpret_cast<IPEICAInterpolationParams*>(pPropertyDataICA[2]),
            sizeof(pInputData->ICAConfigData.ICAInInterpolationParams));
    }

    if (NULL != pPropertyDataICA[3])
    {
        if ((IPEProcessingType::IPEMFNRPostfilter == m_instanceProperty.processingType) &&
            (IPEProfileId::IPEProfileIdDefault == m_instanceProperty.profileId))
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "Skip ICA transforms for MFNR  NPS Postfilter final stage");
        }
        else
        {
            Utils::Memcpy(&pInputData->ICAConfigData.ICARefPerspectiveParams,
                reinterpret_cast<IPEICAPerspectiveTransform*>(pPropertyDataICA[3]),
                sizeof(pInputData->ICAConfigData.ICARefPerspectiveParams));
        }
    }

    if (NULL != pPropertyDataICA[4])
    {
        if (TRUE == static_cast<Titan17xContext*>(
            GetHwContext())->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->enableICARefGrid)
        {
            Utils::Memcpy(&pInputData->ICAConfigData.ICARefGridParams,
                reinterpret_cast<IPEICAGridTransform*>(pPropertyDataICA[4]),
                sizeof(pInputData->ICAConfigData.ICARefGridParams));
        }
    }

    if (NULL != pPropertyDataICA[5])
    {
        Utils::Memcpy(&pInputData->ICAConfigData.ICARefInterpolationParams,
            reinterpret_cast<IPEICAInterpolationParams*>(pPropertyDataICA[5]),
            sizeof(pInputData->ICAConfigData.ICARefInterpolationParams));
    }

    if (NULL != pPropertyDataICA[6])
    {
        Utils::Memcpy(&pInputData->ICAConfigData.ICAReferenceParams,
            reinterpret_cast<IPEICAPerspectiveTransform*>(pPropertyDataICA[6]),
            sizeof(pInputData->ICAConfigData.ICAReferenceParams));
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "perspective REF %d , w %d, h %d, c %d, r %d, frameNum %llu",
                         pInputData->ICAConfigData.ICAReferenceParams.perspectiveTransformEnable,
                         pInputData->ICAConfigData.ICAReferenceParams.transformDefinedOnWidth,
                         pInputData->ICAConfigData.ICAReferenceParams.transformDefinedOnHeight,
                         pInputData->ICAConfigData.ICAReferenceParams.perspetiveGeometryNumColumns,
                         pInputData->ICAConfigData.ICAReferenceParams.perspectiveGeometryNumRows,
                         pInputData->frameNum);
    }

    if (NULL != pPropertyDataICA[7])
    {

        if (0 != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType))
        {
            Utils::Memcpy(&pInputData->ICAConfigData.ICAInPerspectiveParams,
                          reinterpret_cast<IPEICAPerspectiveTransform*>(pPropertyDataICA[7]),
                          sizeof(pInputData->ICAConfigData.ICAInPerspectiveParams));

            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "perspective lookahead %d , w %d, h %d, c %d, r %d, frameNum %llu",
                             pInputData->ICAConfigData.ICAInPerspectiveParams.perspectiveTransformEnable,
                             pInputData->ICAConfigData.ICAInPerspectiveParams.transformDefinedOnWidth,
                             pInputData->ICAConfigData.ICAInPerspectiveParams.transformDefinedOnHeight,
                             pInputData->ICAConfigData.ICAInPerspectiveParams.perspetiveGeometryNumColumns,
                             pInputData->ICAConfigData.ICAInPerspectiveParams.perspectiveGeometryNumRows,
                             pInputData->frameNum);
        }
    }

    if (NULL != pPropertyDataICA[8])
    {
        if ((0 != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType)) &&
           (TRUE == static_cast<Titan17xContext*>(GetHwContext())->GetTitan17xSettingsManager()->
                GetTitan17xStaticSettings()->enableICAInGrid))
        {
            Utils::Memcpy(&pInputData->ICAConfigData.ICAInGridParams,
                          reinterpret_cast<IPEICAGridTransform*>(pPropertyDataICA[8]),
                          sizeof(pInputData->ICAConfigData.ICAInGridParams));
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "grid lookahead %d , w %d, h %d, corner  %d, frameNum %llu",
                             pInputData->ICAConfigData.ICAInGridParams.gridTransformEnable,
                             pInputData->ICAConfigData.ICAInGridParams.transformDefinedOnWidth,
                             pInputData->ICAConfigData.ICAInGridParams.transformDefinedOnHeight,
                             pInputData->ICAConfigData.ICAInGridParams.gridTransformArrayExtrapolatedCorners,
                             pInputData->frameNum);
        }
    }

    // Update margins in pixels
    pInputData->pipelineIPEData.marginDimension.widthPixels = m_stabilizationMargin.widthPixels;
    pInputData->pipelineIPEData.marginDimension.heightLines = m_stabilizationMargin.heightLines;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::PublishICADependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::PublishICADependencies(
    NodeProcessRequestData* pNodeRequestData)
{

    const VOID*       pData[2]      = { 0 };
    UINT              pDataCount[2] = { 0 };
    CamxResult        result        = CamxResultSuccess;
    UINT              i             = 0;


    // Example as to how perspective and grid are posted
    if ((IPEProcessingType::IPEMFNRBlend                == m_instanceProperty.processingType)          ||
        (IPEProcessingType::IPEMFNRPostfilter           == m_instanceProperty.processingType)          ||
        (IPEProcessingType::IPEMFSRBlend                == m_instanceProperty.processingType)          ||
        (IPEProcessingType::IPEMFSRPostfilter           == m_instanceProperty.processingType)          ||
        (0 != (IPEStabilizationType::IPEStabilizationTypeEIS2 & m_instanceProperty.stabilizationType)) ||
        (0 != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType)))
    {
        CAMX_UNREFERENCED_PARAM(pNodeRequestData);
        IPEICAPerspectiveTransform transform;
        {
            transform.perspectiveConfidence        = 255;
            transform.perspectiveGeometryNumRows   = 9;
            transform.perspetiveGeometryNumColumns = 1;
            transform.perspectiveTransformEnable   = 1;
            transform.ReusePerspectiveTransform    = 0;
            transform.transformDefinedOnWidth      = m_fullInputWidth;
            transform.transformDefinedOnHeight     = m_fullInputHeight;
            Utils::Memcpy(&transform.perspectiveTransformArray,
                          perspArray, sizeof(perspArray));
        }
        pData[i]      = &transform;
        pDataCount[i] = sizeof(transform);
        i++;

    }

    if ((0 != (IPEStabilizationType::IPEStabilizationTypeEIS2 & m_instanceProperty.stabilizationType)) ||
        (0 != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType)))
    {
        CAMX_STATIC_ASSERT(sizeof(gridArrayX20) == sizeof(gridArrayY20));
        CAMX_STATIC_ASSERT(sizeof(gridArrayX) == sizeof(gridArrayY));
        BOOL isICA20 = ((CSLCameraTitanVersion::CSLTitan175 ==
                        static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion()) ||
                        (CSLCameraTitanVersion::CSLTitan160 ==
                        static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion())) ? TRUE : FALSE;

        const FLOAT* gArrayX  = (TRUE == isICA20) ? gridArrayX20 : gridArrayX;
        const FLOAT* gArrayY  = (TRUE == isICA20) ? gridArrayY20 : gridArrayY;
        UINT32 gridTWidth     = (TRUE == isICA20) ? ICA20GridTransformWidth  : ICA10GridTransformWidth;
        UINT32 gridTHeight    = (TRUE == isICA20) ? ICA20GridTransformHeight : ICA10GridTransformHeight;

        IPEICAGridTransform gridTransform;
        {
            gridTransform.gridTransformEnable      = 1;
            gridTransform.reuseGridTransform       = 0;
            gridTransform.transformDefinedOnWidth  = GridTransformDefinedOnWidth;
            gridTransform.transformDefinedOnHeight = GridTransformDefinedOnHeight;

            for (UINT idx = 0; idx < (gridTWidth * gridTHeight); idx++)
            {
                gridTransform.gridTransformArray[idx].x = gArrayX[idx];
                gridTransform.gridTransformArray[idx].y = gArrayY[idx];
            }

            gridTransform.gridTransformArrayExtrapolatedCorners = 0;
        }
        pData[i] = &gridTransform;
        pDataCount[i] = sizeof(gridTransform);
        i++;
    }

    result = WriteDataList(&m_IPEICATAGLocation[0], pData, pDataCount, i);
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "WriteDataList failed");
    }
    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::HardcodeAAAInputData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::HardcodeAAAInputData(
    ISPInputData* pInputData,
    UINT          parentNodeID)
{
    pInputData->pAECUpdateData->luxIndex                       = 350.0f;
    for (UINT i = 0; i < ExposureIndexCount; i++)
    {
        pInputData->pAECUpdateData->exposureInfo[i].exposureTime    = 1;
        pInputData->pAECUpdateData->exposureInfo[i].linearGain  = 1.0f;
        pInputData->pAECUpdateData->exposureInfo[i].sensitivity = 1.0f;
    }
    pInputData->pAECUpdateData->predictiveGain                 = 1.0f;
    pInputData->pAWBUpdateData->AWBGains.rGain                 = 2.043310f;
    pInputData->pAWBUpdateData->AWBGains.gGain                 = 1.0f;
    pInputData->pAWBUpdateData->AWBGains.bGain                 = 1.493855f;
    pInputData->pAWBUpdateData->colorTemperature               = 2600;
    pInputData->pAWBUpdateData->numValidCCMs                   = 1;
    pInputData->pAWBUpdateData->AWBCCM[0].isCCMOverrideEnabled = FALSE;
    pInputData->parentNodeID                                   = parentNodeID;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::UpdateNumberofPassesonDimension
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::UpdateNumberofPassesonDimension(
    UINT   parentNodeID,
    UINT32 fullInputWidth,
    UINT32 fullInputHeight)
{

    UINT        numPasses                  = 1;
    UINT        downScale                  = 4;
    UINT32      inputWidthWithoutMargin    = 0;
    UINT32      inputHeightWithoutMargin   = 0;
    CAMX_UNREFERENCED_PARAM(parentNodeID);

    GetSizeWithoutStablizationMargin(fullInputWidth, fullInputHeight, &inputWidthWithoutMargin, &inputHeightWithoutMargin);
    // Full pass input port is always enabled
    m_inputPortIPEPassesMask |= 1 << PASS_NAME_FULL;

    for (UINT passNumber = PASS_NAME_DC_4; passNumber < m_numPasses; passNumber++)
    {
        if (((inputWidthWithoutMargin / downScale) >= ICAMinWidthPixels) &&
            ((inputHeightWithoutMargin / downScale) >= ICAMinHeightPixels))
        {
            m_inputPortIPEPassesMask |= 1 << passNumber;
            numPasses++;
        }
        downScale *= 4;
    }

    if (numPasses != m_numPasses)
    {
        m_numPasses = numPasses;
        CAMX_LOG_INFO(CamxLogGroupIQMod, " Update numberofpasses to %d due to unsupported dimension", m_numPasses);
    }
    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetOEMStatsConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetOEMStatsConfig(
    ISPInputData* pInputData,
    UINT          parentNodeID)
{
    CamxResult result = CamxResultSuccess;

    UINT32 metadataAECFrameControl   = 0;
    UINT32 metadataAWBFrameControl   = 0;
    UINT32 metadataAECStatsControl   = 0;
    UINT32 metadataAWBStatsControl   = 0;
    UINT32 metadataAFStatsControl    = 0;

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
    Utils::Memcpy(pInputData->pAECStatsUpdateData, pVendorTagsControl3A[IPEVendorTagAECStats], sizeof(AECStatsControl));
    Utils::Memcpy(pInputData->pAWBStatsUpdateData, pVendorTagsControl3A[IPEVendorTagAWBStats], sizeof(AWBStatsControl));
    Utils::Memcpy(pInputData->pAFStatsUpdateData, pVendorTagsControl3A[IPEVendorTagAFStats], sizeof(AFStatsControl));
    Utils::Memcpy(pInputData->pAECUpdateData, pVendorTagsControl3A[IPEVendorTagAECFrame], sizeof(AECFrameControl));
    Utils::Memcpy(pInputData->pAWBUpdateData, pVendorTagsControl3A[IPEVendorTagAWBFrame], sizeof(AWBFrameControl));

    pInputData->parentNodeID = parentNodeID;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetOEMIQConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetOEMIQConfig(
    ISPInputData* pInputData,
    UINT          parentNodeID)
{
    CamxResult result = CamxResultSuccess;

    UINT32 metadataIPEIQParam                       = 0;

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.iqsettings", "OEMIPEIQSetting",
        &metadataIPEIQParam);
    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: OEMIPEIQSetting");

    static const UINT vendorTagsControlIQ[] =
    {
        metadataIPEIQParam                      | InputMetadataSectionMask,
    };

    const SIZE_T numTags                            = CAMX_ARRAY_SIZE(vendorTagsControlIQ);
    VOID*        pVendorTagsControlIQ[numTags]      = { 0 };
    UINT64       vendorTagsControlIQOffset[numTags] = { 0 };

    GetDataList(vendorTagsControlIQ, pVendorTagsControlIQ, vendorTagsControlIQOffset, numTags);

    pInputData->pOEMIQSetting = pVendorTagsControlIQ[IPEIQParam];
    pInputData->parentNodeID = parentNodeID;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::NotifyRequestProcessingError()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::NotifyRequestProcessingError(
    NodeFenceHandlerData* pFenceHandlerData,
    UINT                  unSignaledFenceCount)
{
    CAMX_ASSERT(NULL != pFenceHandlerData);
    const StaticSettings* pSettings = HwEnvironment::GetInstance()->GetStaticSettings();
    OutputPort*     pOutputPort     = pFenceHandlerData->pOutputPort;
    CSLFenceResult  fenceResult     = pFenceHandlerData->fenceResult;
    BOOL            enableDump      = ((((0x2 == m_enableIPEHangDump) && (CSLFenceResultFailed == fenceResult)) ||
                                      ((0x1 == m_enableIPEHangDump) && (0 == unSignaledFenceCount))) ? TRUE : FALSE);

    BOOL      enableRefDump         = FALSE;
    UINT32    enableOutputPortMask  = 0xFFFFFFFF;
    UINT32    enableInstanceMask    = 0xFFFFFFFF;
    UINT32    enabledNodeMask       = pSettings->autoImageDumpMask;

    if (CSLFenceResultSuccess != fenceResult)
    {
        if (CSLFenceResultFailed == fenceResult)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Fence failure for output port %d req %llu enableDump %d",
                           pOutputPort->portId, pFenceHandlerData->requestId, enableDump);
        }
        else
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "Fence failure during flush for output port %d req %llu enableDump %d",
                           pOutputPort->portId, pFenceHandlerData->requestId, enableDump);
        }
    }

    if ((TRUE == pSettings->autoImageDump) && (enabledNodeMask & ImageDumpIPE))
    {
        enableOutputPortMask   = pSettings->autoImageDumpIPEoutputPortMask;
        enableInstanceMask     = pSettings->autoImageDumpIPEInstanceMask;
        enableRefDump          = ((0 == unSignaledFenceCount) ? TRUE : FALSE);
    }

    if (TRUE == enableRefDump)
    {
        DumpLoopBackReferenceBuffer(pFenceHandlerData->requestId, enableOutputPortMask,
            enableInstanceMask, pFenceHandlerData->pOutputPort->numBatchedFrames);
    }

    if (TRUE == enableDump)
    {
        CAMX_LOG_INFO(CamxLogGroupPProc, "notify error fence back for request %llu", pFenceHandlerData->requestId);
        CmdBuffer* pBuffer = NULL;
        pBuffer =
            CheckCmdBufferWithRequest(pFenceHandlerData->requestId, m_pIPECmdBufferManager[CmdBufferBLMemory]);
        if (!pBuffer)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "cant find buffer");
            return;
        }

        DumpDebug(CmdBufferBLMemory, pBuffer, pFenceHandlerData->requestId,
            IsRealTime(), m_instanceProperty, NodeIdentifierString());
    }
    if (TRUE == GetStaticSettings()->enableIPEScratchBufferDump)
    {
        DumpScratchBuffer(m_pScratchMemoryBuffer, m_numScratchBuffers, pFenceHandlerData->requestId, InstanceID(),
                            IsRealTime(), m_instanceProperty, GetPipeline());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetStaticMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::GetStaticMetadata()
{
    HwCameraInfo    cameraInfo;

    HwEnvironment::GetInstance()->GetCameraInfo(GetPipeline()->GetCameraId(), &cameraInfo);

    // Initialize default metadata
    m_HALTagsData.saturation                        = 5;
    m_HALTagsData.colorCorrectionAberrationMode     = ColorCorrectionAberrationModeFast;
    m_HALTagsData.edgeMode                          = EdgeModeFast;
    m_HALTagsData.controlVideoStabilizationMode     = NoiseReductionModeFast;
    m_HALTagsData.sharpness                         = 1;
    // Initialize default metadata
    m_HALTagsData.blackLevelLock                    = BlackLevelLockOff;
    m_HALTagsData.colorCorrectionMode               = ColorCorrectionModeFast;
    m_HALTagsData.controlAEMode                     = ControlAEModeOn;
    m_HALTagsData.controlAWBMode                    = ControlAWBModeAuto;
    m_HALTagsData.controlMode                       = ControlModeAuto;

    m_HALTagsData.noiseReductionMode                = NoiseReductionModeFast;
    m_HALTagsData.shadingMode                       = ShadingModeFast;
    m_HALTagsData.statisticsHotPixelMapMode         = StatisticsHotPixelMapModeOff;
    m_HALTagsData.statisticsLensShadingMapMode      = StatisticsLensShadingMapModeOff;
    m_HALTagsData.tonemapCurves.tonemapMode         = TonemapModeFast;

    // Retrieve the static capabilities for this camera
    CAMX_ASSERT(MaxCurvePoints >= cameraInfo.pPlatformCaps->maxTonemapCurvePoints);
    m_HALTagsData.tonemapCurves.curvePoints = cameraInfo.pPlatformCaps->maxTonemapCurvePoints;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::GetStabilizationMargins()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetStabilizationMargins()
{
    CamxResult          result                     = CamxResultSuccess;
    UINT32              receivedMarginEISTag       = 0;
    UINT32              additionalCropOffsetEISTag = 0;
    StabilizationMargin receivedMargin             = { 0 };
    ImageDimensions     additionalCropOffset       = { 0 };

    if (0 != (IPEStabilizationType::IPEStabilizationTypeEIS2 & m_instanceProperty.stabilizationType))
    {
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.eisrealtime",
                                                          "StabilizationMargins", &receivedMarginEISTag);
        CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: StabilizationMargins");

        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.eisrealtime",
                                                          "AdditionalCropOffset", &additionalCropOffsetEISTag);
        CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AdditionalCropOffset");
    }
    else if (0 != (IPEStabilizationType::IPEStabilizationTypeEIS3 & m_instanceProperty.stabilizationType))
    {
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.eislookahead",
                                                          "StabilizationMargins", &receivedMarginEISTag);
        CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: StabilizationMargins");

        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.eislookahead",
                                                          "AdditionalCropOffset", &additionalCropOffsetEISTag);
        CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AdditionalCropOffset");
    }
    else if (0 != (IPEStabilizationType::IPEStabilizationTypeSWEIS2 & m_instanceProperty.stabilizationType))
    {
        // In case of SWEIS, read AdditionalCropOffset meta only, as stabilization margin is already reduced by
        // dewarp node.
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.eisrealtime",
                                                          "AdditionalCropOffset", &additionalCropOffsetEISTag);
        CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AdditionalCropOffset");
    }
    else if (0 != (IPEStabilizationType::IPEStabilizationTypeSWEIS3 & m_instanceProperty.stabilizationType))
    {
        // In case of SWEIS, read AdditionalCropOffset meta only, as stabilization margin is already reduced by
        // dewarp node.
        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.eislookahead",
                                                          "AdditionalCropOffset", &additionalCropOffsetEISTag);
        CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Fail to query: AdditionalCropOffset");
    }

    const UINT marginTags[] =
    {
        receivedMarginEISTag       | UsecaseMetadataSectionMask,
        additionalCropOffsetEISTag | UsecaseMetadataSectionMask,
    };

    const SIZE_T length         = CAMX_ARRAY_SIZE(marginTags);
    VOID*        pData[length]  = { 0 };
    UINT64       offset[length] = { 0 };

    result = GetDataList(marginTags, pData, offset, length);

    if (CamxResultSuccess == result)
    {
        if (NULL != pData[0])
        {
            receivedMargin                    = *static_cast<StabilizationMargin*>(pData[0]);
            m_stabilizationMargin.widthPixels = Utils::EvenFloorUINT32(receivedMargin.widthPixels);
            m_stabilizationMargin.heightLines = Utils::EvenFloorUINT32(receivedMargin.heightLines);
        }

        if (NULL != pData[1])
        {
            additionalCropOffset               = *static_cast<ImageDimensions*>(pData[1]);
            m_additionalCropOffset.widthPixels = Utils::EvenFloorUINT32(additionalCropOffset.widthPixels);
            m_additionalCropOffset.heightLines = Utils::EvenFloorUINT32(additionalCropOffset.heightLines);
        }
    }

    CAMX_LOG_VERBOSE(CamxLogGroupCore,
                     "IPE stabilization margins for stabilization type %d set to %ux%u, additional Crop offset %ux%u",
                     m_instanceProperty.stabilizationType,
                     m_stabilizationMargin.widthPixels,
                     m_stabilizationMargin.heightLines,
                     m_additionalCropOffset.widthPixels,
                     m_additionalCropOffset.heightLines);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::SetScaleRatios
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPENode::SetScaleRatios(
    ISPInputData*     pInputData,
    UINT              parentNodeID,
    CHIRectangle*     pCropInfo,
    IFEScalerOutput*  pIFEScalerOutput,
    CmdBuffer*        pCmdBuffer)
{
    // initialize with default 1.0f
    pInputData->preScaleRatio  = 1.0f;
    pInputData->postScaleRatio = 1.0f;

    if (NULL == pCropInfo)
    {
        CAMX_LOG_INFO(CamxLogGroupPProc, "pCropInfo is NULL");
        return FALSE;
    }

    CAMX_LOG_INFO(CamxLogGroupPProc,
                  "IPE:%d, parentNodeID=%d, input width = %d, height = %d, output width = %d, height = %d",
                  InstanceID(), parentNodeID, m_fullInputWidth, m_fullInputHeight, m_fullOutputWidth, m_fullOutputHeight);

    if (BPS == parentNodeID)
    {
        FLOAT ratio1 = static_cast<FLOAT>(pCropInfo->width)  / static_cast<FLOAT>(m_fullOutputWidth);
        FLOAT ratio2 = static_cast<FLOAT>(pCropInfo->height) / static_cast<FLOAT>(m_fullOutputHeight);
        pInputData->postScaleRatio = (ratio1 > ratio2) ? ratio2 : ratio1;

        CAMX_LOG_INFO(CamxLogGroupPProc,
                      "IPE:%d crop width = %d, height = %d, fwidth = %d, fheight = %d, preScaleRatio = %f, postScaleRatio = %f",
                      InstanceID(),
                      pCropInfo->width,
                      pCropInfo->height,
                      m_fullOutputWidth,
                      m_fullOutputHeight,
                      pInputData->preScaleRatio,
                      pInputData->postScaleRatio);
    }
    else if ((IFE == parentNodeID) || (ChiExternalNode == parentNodeID))
    {
        FLOAT preScaleRatio = 1.0f;

        // Update with previous scaler output
        if (NULL != pIFEScalerOutput)
        {
            preScaleRatio = pIFEScalerOutput->scalingFactor;
            CAMX_LOG_INFO(CamxLogGroupPProc, "IFE scaling factor %f", pIFEScalerOutput->scalingFactor);
        }

        pInputData->preScaleRatio = preScaleRatio;

        // calculate post Scale Ratio
        FLOAT ratio1 = static_cast<FLOAT>(pCropInfo->width)  / static_cast<FLOAT>(m_fullOutputWidth);
        FLOAT ratio2 = static_cast<FLOAT>(pCropInfo->height) / static_cast<FLOAT>(m_fullOutputHeight);
        pInputData->postScaleRatio = (ratio1 > ratio2) ? ratio2 : ratio1;

        CAMX_LOG_INFO(CamxLogGroupPProc,
                      "IPE %d, parentId %d, crop w %d, h %d, full w %d, h %d, preScaleRatio %f, postScaleRatio %f",
                      InstanceID(), parentNodeID,
                      pCropInfo->width,
                      pCropInfo->height,
                      m_fullOutputWidth,
                      m_fullOutputHeight,
                      pInputData->preScaleRatio,
                      pInputData->postScaleRatio);
    }

    if ((IPEProcessingType::IPEMFSRPrefilter  <= m_instanceProperty.processingType) &&
        (IPEProcessingType::IPEMFSRPostfilter >= m_instanceProperty.processingType))
    {
        if (IPEProcessingType::IPEMFSRPrefilter == m_instanceProperty.processingType)
        {
            CalculateIntermediateSize(pInputData, pCropInfo);
            // Update preScaleRatio and postScaleRatio for MFSR usecase
            pInputData->preScaleRatio  = 1.0f / m_curIntermediateDimension.ratio;
            pInputData->postScaleRatio = pInputData->postScaleRatio / pInputData->preScaleRatio;
        }
        else
        {
            pInputData->preScaleRatio  = 1.0f / m_curIntermediateDimension.ratio;
            pInputData->postScaleRatio = pInputData->postScaleRatio / pInputData->preScaleRatio;
        }

        if (m_curIntermediateDimension.ratio > 1.0f)
        {
            if ((FALSE == Utils::FEqual(m_curIntermediateDimension.ratio, m_prevIntermediateDimension.ratio)) ||
                (m_curIntermediateDimension.height != m_prevIntermediateDimension.height)                     ||
                (m_curIntermediateDimension.width  != m_prevIntermediateDimension.width))
            {
                UpdateConfigIO(pCmdBuffer, m_curIntermediateDimension.width, m_curIntermediateDimension.height);
                CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d, Updated with intermediate width = %d, height = %d, ratio = %f",
                               InstanceID(),
                               m_curIntermediateDimension.width,
                               m_curIntermediateDimension.height,
                               m_curIntermediateDimension.ratio);

                m_prevIntermediateDimension = m_curIntermediateDimension;
            }

            // Ensure the downscale sizes bigger than striping requirements; otherwise, disable passes
            UpdateNumberofPassesonDimension(IPE, m_curIntermediateDimension.width, m_curIntermediateDimension.height);
        }
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::CheckIsIPERealtime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPENode::CheckIsIPERealtime()
{
    // if IPE is part of realtime pipeline
    BOOL isRealTime = FALSE;
    if (TRUE == IsRealTime())
    {
        isRealTime = TRUE;
        if (m_numPasses == 4)
        {
            isRealTime = FALSE;
        }
    }
    else if (FALSE == IsScalerOnlyIPE())
    {

        // Preview / video part of offline pipeline , check for processing type so does not fall into MFNR category.
        if (0 != (IPEStabilizationType::IPEStabilizationMCTF & m_instanceProperty.stabilizationType))
        {
            isRealTime = TRUE;
        }
        // if node does not have BPS in pipeline and number of passess less than 3
        // then this is realtime IPE part of an offline pipeline
        if ((m_numPasses <= 3) && (FALSE == IsNodeInPipeline(BPS)))
        {
            isRealTime = TRUE;
        }
    }

    return isRealTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::GetUBWCStatBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::GetUBWCStatBuffer(
    CSLBufferInfo* pBuf,
    UINT64         requestId)
{
    CamxResult result = CamxResultSuccess;
    UINT       bufIdx = requestId % MaxUBWCStatsBufferSize;

    if ((NULL != pBuf) &&
        (NULL != m_UBWCStatBufInfo.pUBWCStatsBuffer))
    {
        m_UBWCStatBufInfo.requestId[bufIdx] = requestId;
        Utils::Memcpy(pBuf, m_UBWCStatBufInfo.pUBWCStatsBuffer, sizeof(CSLBufferInfo));
        pBuf->offset = m_UBWCStatBufInfo.offset[bufIdx];
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Fail to memcpy, ptrs should be valid: pBuf: %p, pUBWCStatsBuffer: %p",
                       pBuf, m_UBWCStatBufInfo.pUBWCStatsBuffer);
        result = CamxResultEInvalidPointer;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::UpdateConfigIO
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::UpdateConfigIO(
    CmdBuffer* pCmdBuffer,
    UINT32     intermediateWidth,
    UINT32     intermediateHeight)
{
    CamxResult       result        = CamxResultSuccess;
    IpeConfigIo*     pConfigIO     = reinterpret_cast<IpeConfigIo*>(m_configIOMem.pVirtualAddr);
    IpeConfigIoData* pConfigIOData = &pConfigIO->cmdData;

    // MFSR: updating intermediate size in Config IO
    if (IPEProcessingType::IPEMFSRPrefilter == m_instanceProperty.processingType)
    {
        pConfigIOData->icaMaxUpscalingQ16 = static_cast<UINT32>(m_maxICAUpscaleLimit * (1 << 16));
        /// @todo (CAMX-4148) uncomment below line after new ipe_data.h check in by FW team
        // pConfigIOData->topologyType       = IpeTopologyType::MFSR_PREFILTER;

        ImageDimensions  ds4Dimension  = { 0 };
        ImageDimensions  ds16Dimension = { 0 };
        ImageDimensions  ds64Dimension = { 0 };
        GetDownscaleDimension(intermediateWidth, intermediateHeight, &ds4Dimension, &ds16Dimension, &ds64Dimension);

        pConfigIOData->images[IPE_OUTPUT_IMAGE_FULL_REF].info.dimensions.widthPixels = intermediateWidth;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_FULL_REF].info.dimensions.heightLines = intermediateHeight;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DS4_REF].info.dimensions.widthPixels  = ds4Dimension.widthPixels;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DS4_REF].info.dimensions.heightLines  = ds4Dimension.heightLines;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DS16_REF].info.dimensions.widthPixels = ds16Dimension.widthPixels;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DS16_REF].info.dimensions.heightLines = ds16Dimension.heightLines;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DS64_REF].info.dimensions.widthPixels = ds64Dimension.widthPixels;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DS64_REF].info.dimensions.heightLines = ds64Dimension.heightLines;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DISPLAY].info.dimensions.widthPixels  = intermediateWidth;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DISPLAY].info.dimensions.heightLines  = intermediateHeight;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DISPLAY].info.format                  = IMAGE_FORMAT_INVALID;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_VIDEO].info.dimensions.widthPixels    = intermediateWidth;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_VIDEO].info.dimensions.heightLines    = intermediateHeight;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_VIDEO].info.format                    = IMAGE_FORMAT_INVALID;
    }
    else if ((IPEProcessingType::IPEMFSRBlend == m_instanceProperty.processingType) ||
             ((IPEProcessingType::IPEMFSRPostfilter == m_instanceProperty.processingType) &&
              (IPEProfileId::IPEProfileIdNPS == m_instanceProperty.profileId)))
    {
        pConfigIOData->icaMaxUpscalingQ16 = static_cast<UINT32>(m_maxICAUpscaleLimit * (1 << 16));
        /// @todo (CAMX-4148) uncomment below line after new ipe_data.h check in by FW team
        // pConfigIOData->topologyType       = IpeTopologyType::MFSR_BLEND;

        ImageDimensions  ds4Dimension  = { 0 };
        ImageDimensions  ds16Dimension = { 0 };
        ImageDimensions  ds64Dimension = { 0 };
        GetDownscaleDimension(intermediateWidth, intermediateHeight, &ds4Dimension, &ds16Dimension, &ds64Dimension);

        if (IPEProcessingType::IPEMFSRBlend == m_instanceProperty.processingType)
        {
            pConfigIOData->images[IPE_OUTPUT_IMAGE_FULL_REF].info.dimensions.widthPixels = intermediateWidth;
            pConfigIOData->images[IPE_OUTPUT_IMAGE_FULL_REF].info.dimensions.heightLines = intermediateHeight;
        }
        else
        {
            pConfigIOData->images[IPE_OUTPUT_IMAGE_DS4_REF].info.dimensions.widthPixels = intermediateWidth;
            pConfigIOData->images[IPE_OUTPUT_IMAGE_DS4_REF].info.dimensions.heightLines = intermediateHeight;
        }

        pConfigIOData->images[IPE_INPUT_IMAGE_FULL_REF].info.dimensions.widthPixels  = intermediateWidth;
        pConfigIOData->images[IPE_INPUT_IMAGE_FULL_REF].info.dimensions.heightLines  = intermediateHeight;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS4_REF].info.dimensions.widthPixels   = ds4Dimension.widthPixels;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS4_REF].info.dimensions.heightLines   = ds4Dimension.heightLines;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS16_REF].info.dimensions.widthPixels  = ds16Dimension.widthPixels;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS16_REF].info.dimensions.heightLines  = ds16Dimension.heightLines;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS64_REF].info.dimensions.widthPixels  = ds64Dimension.widthPixels;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS64_REF].info.dimensions.heightLines  = ds64Dimension.heightLines;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DISPLAY].info.dimensions.widthPixels  = intermediateWidth;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DISPLAY].info.dimensions.heightLines  = intermediateHeight;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_DISPLAY].info.format                  = IMAGE_FORMAT_INVALID;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_VIDEO].info.dimensions.widthPixels    = intermediateWidth;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_VIDEO].info.dimensions.heightLines    = intermediateHeight;
        pConfigIOData->images[IPE_OUTPUT_IMAGE_VIDEO].info.format                    = IMAGE_FORMAT_INVALID;
    }
    else if ((IPEProcessingType::IPEMFSRPostfilter == m_instanceProperty.processingType) &&
             (IPEProfileId::IPEProfileIdDefault == m_instanceProperty.profileId))
    {
        pConfigIOData->icaMaxUpscalingQ16 = 1 << 16;

        ImageDimensions  ds4Dimension = { 0 };
        ImageDimensions  ds16Dimension = { 0 };
        ImageDimensions  ds64Dimension = { 0 };
        GetDownscaleDimension(intermediateWidth, intermediateHeight, &ds4Dimension, &ds16Dimension, &ds64Dimension);

        pConfigIOData->images[IPE_INPUT_IMAGE_FULL].info.dimensions.widthPixels = intermediateWidth;
        pConfigIOData->images[IPE_INPUT_IMAGE_FULL].info.dimensions.heightLines = intermediateHeight;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS4].info.dimensions.widthPixels  = ds4Dimension.widthPixels;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS4].info.dimensions.heightLines  = ds4Dimension.heightLines;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS16].info.dimensions.widthPixels = ds16Dimension.widthPixels;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS16].info.dimensions.heightLines = ds16Dimension.heightLines;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS64].info.dimensions.widthPixels = ds64Dimension.widthPixels;
        pConfigIOData->images[IPE_INPUT_IMAGE_DS64].info.dimensions.heightLines = ds64Dimension.heightLines;
    }

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

        CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE:%d WriteGenericBlobData() return %d", InstanceID(), result);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::ProcessFrameDataCallBack
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::ProcessFrameDataCallBack(
    UINT64          requestId,
    UINT            portId,
    ChiBufferInfo*  pBufferInfo)
{
#if !defined(LE_CAMERA) // qdmetadata not present
    UbwcStatsBuffer* pUBWCStats;
    UINT             ubwcStatsYBufIndx;
    UINT             ubwcStatsCBufIndx;
    UBWCStats        ubwcStatsMetadata[2];
    CSLBufferInfo*   pBuf;
    UINT             bufIdx = requestId % MaxUBWCStatsBufferSize;

    CAMX_ASSERT(NULL != pBufferInfo);

    ubwcStatsMetadata[0].bDataValid = 0;
    ubwcStatsMetadata[1].bDataValid = 0;
    ubwcStatsMetadata[0].version    = static_cast<enum UBWC_Version>(GetUBWCVersion(portId));

    if ((TRUE                  == IsRealTime())                                &&
        ((IPEOutputPortDisplay == portId) || (IPEOutputPortVideo   == portId)) &&
        (TRUE                  == CamX::IsGrallocBuffer(pBufferInfo)))
    {
        if (requestId == m_UBWCStatBufInfo.requestId[bufIdx])
        {
            ubwcStatsMetadata[0].bDataValid = 1;

            pBuf = m_UBWCStatBufInfo.pUBWCStatsBuffer;
            pUBWCStats = reinterpret_cast<UbwcStatsBuffer*>(
               static_cast<BYTE*>(pBuf->pVirtualAddr) + m_UBWCStatBufInfo.offset[bufIdx]);

            if (IPEOutputPortDisplay == portId)
            {
                ubwcStatsYBufIndx = IPE_UBWC_STATS_DISPLAY_Y;
                ubwcStatsCBufIndx = IPE_UBWC_STATS_DISPLAY_C;
            }
            else
            {
                ubwcStatsYBufIndx = IPE_UBWC_STATS_VIDEO_Y;
                ubwcStatsCBufIndx = IPE_UBWC_STATS_VIDEO_C;
            }

            ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile32 =
                (pUBWCStats->UbwcStatsBuffer[ubwcStatsYBufIndx].UBWC_COMPRESSED_32B +
                    pUBWCStats->UbwcStatsBuffer[ubwcStatsCBufIndx].UBWC_COMPRESSED_32B);
            ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile64 =
                (pUBWCStats->UbwcStatsBuffer[ubwcStatsYBufIndx].UBWC_COMPRESSED_64B +
                    pUBWCStats->UbwcStatsBuffer[ubwcStatsCBufIndx].UBWC_COMPRESSED_64B);
            ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile96 =
                (pUBWCStats->UbwcStatsBuffer[ubwcStatsYBufIndx].UBWC_COMPRESSED_96B +
                    pUBWCStats->UbwcStatsBuffer[ubwcStatsCBufIndx].UBWC_COMPRESSED_96B);
            ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile128 =
                (pUBWCStats->UbwcStatsBuffer[ubwcStatsYBufIndx].UBWC_COMPRESSED_128B +
                    pUBWCStats->UbwcStatsBuffer[ubwcStatsCBufIndx].UBWC_COMPRESSED_128B);
            ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile160 =
                (pUBWCStats->UbwcStatsBuffer[ubwcStatsYBufIndx].UBWC_COMPRESSED_160B +
                    pUBWCStats->UbwcStatsBuffer[ubwcStatsCBufIndx].UBWC_COMPRESSED_160B);
            ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile192 =
                (pUBWCStats->UbwcStatsBuffer[ubwcStatsYBufIndx].UBWC_COMPRESSED_192B +
                    pUBWCStats->UbwcStatsBuffer[ubwcStatsCBufIndx].UBWC_COMPRESSED_192B);
            ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile256 =
                (pUBWCStats->UbwcStatsBuffer[ubwcStatsYBufIndx].UBWC_COMPRESSED_256B +
                    pUBWCStats->UbwcStatsBuffer[ubwcStatsCBufIndx].UBWC_COMPRESSED_256B);

#if DUMP_UBWC_STATS_INFO
            /* Weights are set to 1 for now, might change later*/
            const UINT weight32 = 1;
            const UINT weight64 = 1;
            const UINT weight96 = 1;
            const UINT weight128 = 1;
            const UINT weight160 = 1;
            const UINT weight192 = 1;
            const UINT weight256 = 1;

            const auto totalTiles = pUBWCStats->UbwcStatsBuffer[ubwcStatsYBufIndx].totalTiles +
                pUBWCStats->UbwcStatsBuffer[ubwcStatsCBufIndx].totalTiles;
            float frameCR =
                (static_cast<float>(ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile32) / totalTiles * 256 / 32) * weight32 +
                (static_cast<float>(ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile64) / totalTiles * 256 / 64) * weight64 +
                (static_cast<float>(ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile96) / totalTiles * 256 / 96) * weight96 +
                (static_cast<float>(ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile128) / totalTiles * 256 / 128) * weight128 +
                (static_cast<float>(ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile160) / totalTiles * 256 / 160) * weight160 +
                (static_cast<float>(ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile192) / totalTiles * 256 / 192) * weight192 +
                (static_cast<float>(ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile256) / totalTiles * 256 / 256) * weight256;

            const auto pYBuf = &(pUBWCStats->UbwcStatsBuffer[ubwcStatsYBufIndx]);
            const auto yBuftotalTiles = pYBuf->totalTiles;
            float yBufCR =
                (static_cast<float>(pYBuf->UBWC_COMPRESSED_32B) / yBuftotalTiles * 256 / 32) * weight32 +
                (static_cast<float>(pYBuf->UBWC_COMPRESSED_64B) / yBuftotalTiles * 256 / 64) * weight64 +
                (static_cast<float>(pYBuf->UBWC_COMPRESSED_96B) / yBuftotalTiles * 256 / 96) * weight96 +
                (static_cast<float>(pYBuf->UBWC_COMPRESSED_128B) / yBuftotalTiles * 256 / 128) * weight128 +
                (static_cast<float>(pYBuf->UBWC_COMPRESSED_160B) / yBuftotalTiles * 256 / 160) * weight160 +
                (static_cast<float>(pYBuf->UBWC_COMPRESSED_192B) / yBuftotalTiles * 256 / 192) * weight192 +
                (static_cast<float>(pYBuf->UBWC_COMPRESSED_256B) / yBuftotalTiles * 256 / 256) * weight256;

            const auto pCBuf = &(pUBWCStats->UbwcStatsBuffer[ubwcStatsCBufIndx]);
            const auto cBufTotalTiles = pCBuf->totalTiles;
            float cBufCR =
                (static_cast<float>(pCBuf->UBWC_COMPRESSED_32B) / cBufTotalTiles * 256 / 32) * weight32 +
                (static_cast<float>(pCBuf->UBWC_COMPRESSED_64B) / cBufTotalTiles * 256 / 64) * weight64 +
                (static_cast<float>(pCBuf->UBWC_COMPRESSED_96B) / cBufTotalTiles * 256 / 96) * weight96 +
                (static_cast<float>(pCBuf->UBWC_COMPRESSED_128B) / cBufTotalTiles * 256 / 128) * weight128 +
                (static_cast<float>(pCBuf->UBWC_COMPRESSED_160B) / cBufTotalTiles * 256 / 160) * weight160 +
                (static_cast<float>(pCBuf->UBWC_COMPRESSED_192B) / cBufTotalTiles * 256 / 192) * weight192 +
                (static_cast<float>(pCBuf->UBWC_COMPRESSED_256B) / cBufTotalTiles * 256 / 256) * weight256;

            CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                "Update UBWC stats for port %u, buf %u, request %llu\n"
                "UBWC version %d\n"
                "Frame UBWC CR = %f, Y buffer UBWC CR=%f, C buffer UBWC CR=%f\n"
                "Frame: 32B = %u 64B = %u 96B = %u 128B = %u 160B = %u "
                "192B = %u 256B = %u total tiles = %u\n"
                "Y buffer idx[%u]: 32B = %u 64B = %u 96B = %u 128B = %u 160B = %u "
                "192B = %u 256B = %u total tiles = %u\n"
                "C buffer idx[%u]: 32B = %u 64B = %u 96B = %u 128B = %u 160B = %u "
                "192B = %u 256B = %u total tiles = %u",
                portId, bufIdx, requestId,
                ubwcStatsMetadata[0].version,
                frameCR, yBufCR, cBufCR,
                ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile32,
                ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile64,
                ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile96,
                ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile128,
                ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile160,
                ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile192,
                ubwcStatsMetadata[0].ubwc_stats.nCRStatsTile256,
                totalTiles,
                ubwcStatsYBufIndx,
                pYBuf->UBWC_COMPRESSED_32B,
                pYBuf->UBWC_COMPRESSED_64B,
                pYBuf->UBWC_COMPRESSED_96B,
                pYBuf->UBWC_COMPRESSED_128B,
                pYBuf->UBWC_COMPRESSED_160B,
                pYBuf->UBWC_COMPRESSED_192B,
                pYBuf->UBWC_COMPRESSED_256B,
                pYBuf->totalTiles,
                ubwcStatsCBufIndx,
                pCBuf->UBWC_COMPRESSED_32B,
                pCBuf->UBWC_COMPRESSED_64B,
                pCBuf->UBWC_COMPRESSED_96B,
                pCBuf->UBWC_COMPRESSED_128B,
                pCBuf->UBWC_COMPRESSED_160B,
                pCBuf->UBWC_COMPRESSED_192B,
                pCBuf->UBWC_COMPRESSED_256B,
                pCBuf->totalTiles);

#endif // DUMP_UBWC_STATS_INFO
        }
        else
        {
            if (FALSE == GetFlushStatus())
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid UBWC stats buffer: expected request=%lld,"
                    "actual request=%lld",
                    m_UBWCStatBufInfo.requestId[bufIdx],
                    requestId);
            }
            else
            {
                CAMX_LOG_INFO(CamxLogGroupPProc, "Invalid UBWC stats buffer during flush: expected request=%lld,"
                    "actual request=%lld",
                    m_UBWCStatBufInfo.requestId[bufIdx],
                    requestId);
            }
        }

        BufferHandle* phNativeHandle = CamX::GetBufferHandleFromBufferInfo(pBufferInfo);

        if (NULL != phNativeHandle)
        {
            OsUtils::SetGrallocMetaData(*phNativeHandle, SET_UBWC_CR_STATS_INFO, &ubwcStatsMetadata[0]);
        }
    }
#endif // LE_CAMERA
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::CalculateIntermediateSize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT IPENode::CalculateIntermediateSize(
    ISPInputData*     pInputData,
    CHIRectangle*     pCropInfo)
{
    m_curIntermediateDimension.ratio  = 1.0f / pInputData->postScaleRatio;
    m_curIntermediateDimension.width  = m_fullOutputWidth;
    m_curIntermediateDimension.height = m_fullOutputHeight;

    if (m_curIntermediateDimension.ratio >= m_maxICAUpscaleLimit)
    {
        m_curIntermediateDimension.ratio  = m_maxICAUpscaleLimit;
        m_curIntermediateDimension.width  = Utils::EvenCeilingUINT32(
            static_cast<UINT32>(m_curIntermediateDimension.ratio * pCropInfo->width));
        m_curIntermediateDimension.height = Utils::EvenCeilingUINT32(
            static_cast<UINT32>(m_curIntermediateDimension.ratio * pCropInfo->height));

        CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d intermediate width=%d, height=%d, ratio=%f",
                       InstanceID(),
                       m_curIntermediateDimension.width,
                       m_curIntermediateDimension.height,
                       m_curIntermediateDimension.ratio);
    }

    if (m_curIntermediateDimension.height > static_cast<UINT32>(m_fullOutputHeight))
    {
        m_curIntermediateDimension.height = static_cast<UINT32>(m_fullOutputHeight);
    }
    if (m_curIntermediateDimension.width > static_cast<UINT32>(m_fullOutputWidth))
    {
        m_curIntermediateDimension.width = static_cast<UINT32>(m_fullOutputWidth);
    }

    return m_curIntermediateDimension.ratio;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::DumpConfigIOData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::DumpConfigIOData()
{
    IpeConfigIo*     pConfigIO;
    IpeConfigIoData* pConfigIOData;

    pConfigIO     = reinterpret_cast<IpeConfigIo*>(m_configIOMem.pVirtualAddr);
    pConfigIOData = &pConfigIO->cmdData;

    CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d Config IO Data Dump:", InstanceID());
    CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d icaMaxUpscalingQ16 = %d", InstanceID(), pConfigIOData->icaMaxUpscalingQ16);
    CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d topologyType = %d", InstanceID(), pConfigIOData->topologyType);
    for (UINT i = 0; i < IPE_IO_IMAGES_MAX; i++)
    {
        CAMX_LOG_INFO(CamxLogGroupPProc, "IPE:%d image %d format=%d, width=%d, height=%d",
                       InstanceID(),  i,
                       pConfigIOData->images[i].info.format,
                       pConfigIOData->images[i].info.dimensions.widthPixels,
                       pConfigIOData->images[i].info.dimensions.heightLines);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::DumpTuningModeData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::DumpTuningModeData()
{
    // print out tuning mode information
    CAMX_LOG_VERBOSE(CamxLogGroupPProc,
                     "IPE:%d "
                     "Level 0 mode (default) type %d "
                     "Level 0 submode value %d "
                     "Level 1 mode (sensor) type %d "
                     "Level 1 submode value (sensorModeIndex) %d "
                     "Level 2 mode (usecase) type %d "
                     "Level 2 submode value (usecase value) %d "
                     "Level 3 mode (feature1) type %d "
                     "Level 3 submode value (feature1 value) %d "
                     "Level 4 mode (feature2) type %d "
                     "Level 4 submode value (feature2 value) %d "
                     "Level 5 mode (scene) type %d "
                     "Level 5 submode value (scene mode) %d "
                     "Level 6 mode (effect) type %d "
                     "Level 6 submode value (effect value) %d",
                     InstanceID(),
                     static_cast<UINT32>(m_tuningData.TuningMode[0].mode),
                     static_cast<UINT32>(m_tuningData.TuningMode[0].subMode.value),
                     static_cast<UINT32>(m_tuningData.TuningMode[1].mode),
                     static_cast<UINT32>(m_tuningData.TuningMode[1].subMode.value),
                     static_cast<UINT32>(m_tuningData.TuningMode[2].mode),
                     static_cast<UINT32>(m_tuningData.TuningMode[2].subMode.usecase),
                     static_cast<UINT32>(m_tuningData.TuningMode[3].mode),
                     static_cast<UINT32>(m_tuningData.TuningMode[3].subMode.feature1),
                     static_cast<UINT32>(m_tuningData.TuningMode[4].mode),
                     static_cast<UINT32>(m_tuningData.TuningMode[4].subMode.feature2),
                     static_cast<UINT32>(m_tuningData.TuningMode[5].mode),
                     static_cast<UINT32>(m_tuningData.TuningMode[5].subMode.scene),
                     static_cast<UINT32>(m_tuningData.TuningMode[6].mode),
                     static_cast<UINT32>(m_tuningData.TuningMode[6].subMode.effect));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPENode::IsSupportedPortConfiguration
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPENode::IsSupportedPortConfiguration(
    UINT portId)
{
    CamxResult result             = CamxResultSuccess;
    BOOL       supportedConfig    = TRUE;

    const ImageFormat* pImageFormat    = NULL;

    pImageFormat = GetInputPortImageFormat(InputPortIndex(portId));
    if (NULL == pImageFormat)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "pImageFormat is NULL");
        result = CamxResultEInvalidPointer;
    }

    if (CamxResultSuccess == result)
    {
        if ((IPEInputPortFull != portId) && (IPEInputPortFullRef != portId))
        {
            if ((pImageFormat->height < ICAMinHeightPixels) || (pImageFormat->width < ICAMinWidthPixels))
            {
                supportedConfig = FALSE;
            }
            else
            {
                supportedConfig = TRUE;
            }
        }
        else
        {
            CAMX_ASSERT((pImageFormat->height >= ICAMinHeightPixels) && (pImageFormat->width >= ICAMinWidthPixels));
        }
    }

    m_numPasses = Utils::CountSetBits((GetInputPortDisableMask() & m_inputPortIPEPassesMask) ^ m_inputPortIPEPassesMask);
    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Updated IPE:%d num of passes %d", InstanceID(), m_numPasses);

    return supportedConfig;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::QueryMetadataPublishList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::QueryMetadataPublishList(
    NodeMetadataList* pPublistTagList)
{
    UINT32 finalNumIPEOutputTags = 0;

    if (FALSE == IsSkipPostMetadata())
    {
        finalNumIPEOutputTags = NumIPEMetadataOutputTags;

        for (UINT32 tagIndex = 0; tagIndex < finalNumIPEOutputTags; ++tagIndex)
        {
            pPublistTagList->tagArray[tagIndex] = IPEMetadataOutputTags[tagIndex];
        }
    }

    pPublistTagList->tagCount = finalNumIPEOutputTags;
    CAMX_LOG_VERBOSE(CamxLogGroupMeta, "%u tags will be published", finalNumIPEOutputTags);

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::ApplyStatsFOVCCrop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPENode::ApplyStatsFOVCCrop(
    CHIRectangle* pCropInfo,
    UINT          parentNodeId,
    UINT32        fullInputWidth,
    UINT32        fullInputHeight)
{
    FLOAT fovcCropFactor = 1.0f;

    if ((BPS != parentNodeId) && (NULL != pCropInfo))
    {
        fovcCropFactor = m_prevFOVC;

        int32_t adjustedWidth  = pCropInfo->width;
        int32_t adjustedHeight = pCropInfo->height;

        // Calculate total change in width or height
        adjustedWidth  -= static_cast<int32_t>(adjustedWidth  * (1 - fovcCropFactor));
        adjustedHeight -= static_cast<int32_t>(adjustedHeight * (1 - fovcCropFactor));

        // (change in height)/2 is change in top, (change in width)/2 is change in left
        int32_t width  = adjustedWidth / 2;
        int32_t height = adjustedHeight / 2;

        pCropInfo->left   += width;
        pCropInfo->top    += height;
        pCropInfo->width  -= width  * 2;
        pCropInfo->height -= height * 2;

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "ZDBG: After IPE ICA1 Zoom Window: [%d, %d, %d, %d] full %d x %d,"
            "FOVC crop_factor %f",
            pCropInfo->left, pCropInfo->top,
            pCropInfo->width, pCropInfo->height,
            fullInputWidth, fullInputHeight, fovcCropFactor);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::FillLoopBackFrameBufferData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::FillLoopBackFrameBufferData(
    UINT64        requestId,
    UINT          firstValidRequest,
    CmdBuffer*    pFrameProcessCmdBuffer,
    UINT32        numFramesInBuffer)
{
    CamxResult result        = CamxResultSuccess;
    ImageBuffer* pPingBuffer = NULL;
    ImageBuffer* pPongBuffer = NULL;
    INT32  outputBufferIndex = 0;
    INT32  inputBufferIndex  = 0;

    UINT64 requestDelta = requestId - m_currentrequestID;
    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "pipeline %s IPE:%d Input for setting loop-back"
                     "-requestDelta %llu, numFramesInBuffer %d, firstValidRequest %d",
                     NodeIdentifierString(),
                     InstanceID(),
                     requestDelta,
                     numFramesInBuffer,
                     firstValidRequest);

    // if IPE Reference output dumps are not enabled m_referenceBufferCount remains 2;
    // then output and input buffer indices will circularly switch across {0, 1}
    // if IPE Reference output dumps are ENABLED, m_referenceBufferCount is made 32;
    // considering HFR Max Batch size 16, for 480fps video @30fps preview then
    // output and input buffer indices will circularly switch across {0,1... ,31}
    outputBufferIndex = GetLoopBackBufferBatchIndex(requestId, numFramesInBuffer);
    inputBufferIndex  = ((outputBufferIndex - 1) < 0) ? m_referenceBufferCount - 1 : outputBufferIndex - 1;
    CAMX_LOG_VERBOSE(CamxLogGroupISP, "refbufcnt: %d requestId: %llu o/pidx: %d I/pIdx: %d ",
        m_referenceBufferCount, requestId, outputBufferIndex, inputBufferIndex);

    for (UINT pass = 0; pass < m_numPasses; pass++)
    {
        // Patch buffers only if the port is enabled. Could be disabled by dimension limitations
        if (TRUE == m_loopBackBufferParams[pass].portEnable)
        {
            // Non batch mode case ping/pong between each request.
            if (1 >= numFramesInBuffer)
            {
                pPingBuffer = m_loopBackBufferParams[pass].pReferenceBuffer[outputBufferIndex];
                pPongBuffer = m_loopBackBufferParams[pass].pReferenceBuffer[inputBufferIndex];

                CAMX_ASSERT(NULL != pPingBuffer);
                CAMX_ASSERT(NULL != pPongBuffer);

                result = FillFrameBufferData(pFrameProcessCmdBuffer,
                                             pPingBuffer,
                                             0,
                                             0,
                                             m_loopBackBufferParams[pass].outputPortID);
                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "node %s reporting o/p reference"
                                 "port %d, request %llu, buffer %p",
                                 NodeIdentifierString(),
                                 m_loopBackBufferParams[pass].outputPortID,
                                 requestId,
                                 pPingBuffer->GetPlaneVirtualAddr(0, 0));

                // reset reference is true for first request to a node and
                // also if difference between requests in greater than 1.
                if (FALSE == IsSkipReferenceInput(requestDelta, firstValidRequest))
                {
                    result = FillFrameBufferData(pFrameProcessCmdBuffer,
                                                 pPongBuffer,
                                                 0,
                                                 0,
                                                 m_loopBackBufferParams[pass].inputPortID);
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "node %s reporting i/p reference"
                                     "port %d,request %llu,buffer %p",
                                     NodeIdentifierString(),
                                     m_loopBackBufferParams[pass].inputPortID,
                                     requestId,
                                     pPongBuffer->GetPlaneVirtualAddr(0, 0));
                }

            }
            else
            {
                /* In order to accomodate HFR dump frames we use certain slots of buffer indices for
                 * m_loopBackBufferParams[pass].pReferenceBuffer[] of size "numFramesInBuffer" or batch size.
                 * Every request gets an unique slot of buffer index and those slots are rotated circularly.
                 * For example: if Batch size is 8 then we'll have 4 slots of buffer indices mapped to each
                 * requestID.
                 * {
                 *     RequestID: 1 ==> slot1 (8  to 15) ,
                 *     RequestID: 2 ==> slot2 (16 to 23) ,
                 *     RequestID: 3 ==> slot3 (24 to 31) ,
                 *     RequestID: 4 ==> slot0 (0  to 7)  ,
                 *     .......
                 * }
                 * */
                INT32 bufferBatchOffset = GetLoopBackBufferBatchIndex(requestId, numFramesInBuffer);
                INT32 maxBufferIndex    =
                    ((m_referenceBufferCount > numFramesInBuffer) ? (numFramesInBuffer - 1) : (m_referenceBufferCount - 1));
                for (UINT batchedFrameIndex = 0; batchedFrameIndex < numFramesInBuffer; batchedFrameIndex++)
                {
                    if (m_referenceBufferCount >= numFramesInBuffer)
                    {
                        outputBufferIndex = bufferBatchOffset + batchedFrameIndex;
                        inputBufferIndex  = bufferBatchOffset +
                            ((batchedFrameIndex == 0U) ? maxBufferIndex : (batchedFrameIndex - 1U));
                    }
                    else
                    {
                        outputBufferIndex = (batchedFrameIndex % m_referenceBufferCount);
                        inputBufferIndex  = ((outputBufferIndex - 1) < 0) ?
                            m_referenceBufferCount - 1 : outputBufferIndex - 1;
                    }

                    pPingBuffer = m_loopBackBufferParams[pass].pReferenceBuffer[outputBufferIndex];
                    pPongBuffer = m_loopBackBufferParams[pass].pReferenceBuffer[inputBufferIndex];

                    result = FillFrameBufferData(pFrameProcessCmdBuffer,
                                                 pPingBuffer,
                                                 batchedFrameIndex,
                                                 0,
                                                 m_loopBackBufferParams[pass].outputPortID);
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "node %s reporting o/p reference-"
                                     "batchedFrameIndex %d port %d, request %llu, buffer %p",
                                     NodeIdentifierString(),
                                     batchedFrameIndex,
                                     m_loopBackBufferParams[pass].outputPortID,
                                     requestId,
                                     pPingBuffer->GetPlaneVirtualAddr(0, 0));

                    if (CamxResultSuccess != result)
                    {
                        break;
                    }

                    if (TRUE == IsSkipReferenceInput(requestDelta, firstValidRequest))
                    {
                        continue;
                    }

                    {
                        result = FillFrameBufferData(pFrameProcessCmdBuffer,
                                                     pPongBuffer,
                                                     batchedFrameIndex,
                                                     0,
                                                     m_loopBackBufferParams[pass].inputPortID);
                        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "node %s reporting i/p reference"
                                         "batchedFrameIndex %d port %d, request %llu, buffer %p",
                                         NodeIdentifierString(),
                                         batchedFrameIndex,
                                         m_loopBackBufferParams[pass].inputPortID,
                                         requestId,
                                         pPongBuffer->GetPlaneVirtualAddr(0, 0));
                    }

                    if (CamxResultSuccess != result)
                    {
                        break;
                    }
                }
            }
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::DeleteLoopBackBufferManagers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::DeleteLoopBackBufferManagers()
{
    CamxResult result         = CamxResultSuccess;
    UINT       referenceCount = 0;
    for (UINT pass = 0; pass < m_numPasses; pass++)
    {
        if (TRUE == m_loopBackBufferParams[pass].portEnable)
        {
            for (UINT bufferIndex = 0; bufferIndex < m_referenceBufferCount; bufferIndex++)
            {
                referenceCount =
                    m_loopBackBufferParams[pass].pReferenceBuffer[bufferIndex]->ReleaseBufferManagerImageReference();
                if (0 == referenceCount)
                {
                    CAMX_ASSERT(TRUE == m_loopBackBufferParams[pass].pReferenceBuffer[bufferIndex]->HasBackingBuffer());
                    m_loopBackBufferParams[pass].pReferenceBuffer[bufferIndex]->Release(FALSE);
                    CAMX_ASSERT(FALSE == m_loopBackBufferParams[pass].pReferenceBuffer[bufferIndex]->HasBackingBuffer());
                    m_loopBackBufferParams[pass].pReferenceBuffer[bufferIndex] = NULL;
                }
            }

            if (NULL != m_loopBackBufferParams[pass].pReferenceBufferManager)
            {
                m_loopBackBufferParams[pass].pReferenceBufferManager->Deactivate(FALSE);
                m_loopBackBufferParams[pass].pReferenceBufferManager->Destroy();
                m_loopBackBufferParams[pass].pReferenceBufferManager = NULL;
            }
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::GetUBWCVersion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 IPENode::GetUBWCVersion(
    UINT portId)
{
    UINT32 UBWCVersion = 0;

    if ((IPEOutputPortDisplay == portId) || (IPEOutputPortVideo == portId))
    {
        UBWCVersion = ImageFormatUtils::GetUBWCVersionExternal();
    }
    else
    {
        UBWCVersion = ImageFormatUtils::GetUBWCVersion();
    }

    return UBWCVersion;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::GetUBWCHWVersion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 IPENode::GetUBWCHWVersion(
    UINT portId)
{
    UINT32 UBWCHWVersion = 0;

    if ((IPEOutputPortDisplay == portId) || (IPEOutputPortVideo == portId))
    {
        UBWCHWVersion = ImageFormatUtils::GetUBWCHWVersionExternal();
    }
    else
    {
        UBWCHWVersion = ImageFormatUtils::GetUBWCHWVersion();
    }

    return UBWCHWVersion;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPENode::CreateLoopBackBufferManagers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPENode::CreateLoopBackBufferManagers()
{
    CamxResult result                = CamxResultSuccess;
    UINT       numPasses             = m_numPasses;
    UINT32     inputReferenceOffset  = IPEInputPortFullRef;
    UINT32     outputReferenceOffset = IPEOutputPortFullRef;
    UINT32     width                 = 0;
    UINT32     height                = 0;

    GetSizeWithoutStablizationMargin(m_fullInputWidth, m_fullInputHeight, &width, &height);
    BufferManagerCreateData createData      = {};
    FormatParamInfo         formatParamInfo = { 0 };
    IpeConfigIo*            pConfigIO       = reinterpret_cast<IpeConfigIo*>(m_configIOMem.pVirtualAddr);
    IpeConfigIoData*        pConfigIOData   = &pConfigIO->cmdData;

    // Initialize buffer manager name for each passess
    OsUtils::SNPrintF(m_loopBackBufferParams[0].bufferManagerName, sizeof(m_loopBackBufferParams[0].bufferManagerName),
        "%s_%s%d_OutputPortId_FullRef", GetPipelineName(), Name(), InstanceID());

    OsUtils::SNPrintF(m_loopBackBufferParams[1].bufferManagerName, sizeof(m_loopBackBufferParams[1].bufferManagerName),
        "%s_%s%d_OutputPortId_Ds4Ref", GetPipelineName(), Name(), InstanceID());

    OsUtils::SNPrintF(m_loopBackBufferParams[2].bufferManagerName, sizeof(m_loopBackBufferParams[2].bufferManagerName),
        "%s_%s%d_OutputPortId_DS16Ref", GetPipelineName(), Name(), InstanceID());

    // update each pass specific parameters
    for (UINT pass = 0; pass < numPasses; pass++)
    {
        //  Intialize loop back portIDs
        m_loopBackBufferParams[pass].outputPortID = static_cast<IPE_IO_IMAGES>(pass + IPE_OUTPUT_IMAGE_FULL_REF);
        m_loopBackBufferParams[pass].inputPortID  = static_cast<IPE_IO_IMAGES>(pass + IPE_INPUT_IMAGE_FULL_REF);

        if (IPEOutputPortFullRef == m_loopBackBufferParams[pass].outputPortID)
        {
            m_loopBackBufferParams[pass].format.format              = CamX::Format::UBWCTP10;
            m_loopBackBufferParams[pass].format.width               = width;
            m_loopBackBufferParams[pass].format.height              = height;
            m_loopBackBufferParams[pass].format.ubwcVerInfo.version =
                static_cast <UbwcVersion>(ImageFormatUtils::GetUBWCHWVersion());
            m_loopBackBufferParams[pass].format.ubwcVerInfo.lossy =
                (TRUE == IsUBWCLossyNeededForLoopBackBuffer(
                    &m_loopBackBufferParams[pass].format)) ? UBWC_LOSSY : UBWC_LOSSLESS;
        }
        else
        {
            UINT factor                                = static_cast<UINT>(pow(4, pass));
            m_loopBackBufferParams[pass].format.format = CamX::Format::PD10;
            m_loopBackBufferParams[pass].format.width  =
                Utils::EvenCeilingUINT32(Utils::AlignGeneric32(width, factor) / factor);
            m_loopBackBufferParams[pass].format.height =
                Utils::EvenCeilingUINT32(Utils::AlignGeneric32(height, factor) / factor);
        }

        // Minimum pass dimension not met
        if ((m_loopBackBufferParams[pass].format.width  < ICAMinWidthPixels) ||
            (m_loopBackBufferParams[pass].format.height < ICAMinHeightPixels))
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "node %s, pass %d, dimension w %d, h %d < than 30 * 26",
                          NodeIdentifierString(),
                          pass,
                          m_loopBackBufferParams[pass].format.width,
                          m_loopBackBufferParams[pass].format.height);
            break;
        }

        m_loopBackBufferParams[pass].format.alignment  = 1;
        m_loopBackBufferParams[pass].format.colorSpace = CamX::ColorSpace::Unknown;
        m_loopBackBufferParams[pass].format.rotation   = CamX::Rotation::CW0Degrees;

        ImageFormatUtils::InitializeFormatParams(&m_loopBackBufferParams[pass].format, &formatParamInfo, 0);

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "node %s, pass %d, port %d-%d, w %d, h %d,format %d,buffersize %d",
                         NodeIdentifierString(),
                         pass,
                         m_loopBackBufferParams[pass].inputPortID,
                         m_loopBackBufferParams[pass].outputPortID,
                         m_loopBackBufferParams[pass].format.width,
                         m_loopBackBufferParams[pass].format.height,
                         m_loopBackBufferParams[pass].format.format,
                         ImageFormatUtils::GetTotalSize(&m_loopBackBufferParams[pass].format));

        createData.bufferProperties.immediateAllocImageBuffers = m_referenceBufferCount;
        createData.bufferProperties.maxImageBuffers            = m_referenceBufferCount;
        createData.bufferProperties.consumerFlags              = 0;
        createData.bufferProperties.producerFlags              = 0;
        createData.bufferProperties.grallocFormat              = 0;
        createData.bufferProperties.bufferHeap                 = CSLBufferHeapIon;
        createData.bufferProperties.memFlags                   = CSLMemFlagHw;
        createData.bufferProperties.imageFormat                = m_loopBackBufferParams[pass].format;

        if (TRUE == IsSecureMode())
        {
            createData.bufferProperties.memFlags |= CSLMemFlagProtected;
        }
        else
        {
            createData.bufferProperties.memFlags |= CSLMemFlagUMDAccess;
        }

        createData.bNeedDedicatedBuffers      = TRUE;
        createData.bDisableSelfShrinking      = TRUE;
        createData.deviceIndices[0]           = m_deviceIndex;
        createData.deviceCount                = 1;
        createData.allocateBufferMemory       = TRUE;
        createData.bEnableLateBinding         = 0;
        createData.maxBufferCount             = m_referenceBufferCount;
        createData.immediateAllocBufferCount  = m_referenceBufferCount;
        createData.bufferManagerType          = BufferManagerType::CamxBufferManager;
        createData.linkProperties.pNode       = this;
        createData.numBatchedFrames           = 1;

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "node %s, pass %d, buffer manager name:%s, allocate buffer:%d",
                         NodeIdentifierString(),
                         pass,
                         m_loopBackBufferParams[pass].bufferManagerName,
                         createData.allocateBufferMemory);

        result = Node::CreateImageBufferManager(m_loopBackBufferParams[pass].bufferManagerName, &createData,
                                          &m_loopBackBufferParams[pass].pReferenceBufferManager);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "node %s, pass %d, Create buffer manager failed %s",
                           NodeIdentifierString(),
                           pass,
                           m_loopBackBufferParams[pass].bufferManagerName);

            break;
        }
        else
        {
            result = m_loopBackBufferParams[pass].pReferenceBufferManager->Activate();
            if (CamxResultSuccess == result)
            {
                for (UINT bufferIndex = 0; bufferIndex < m_referenceBufferCount; bufferIndex++)
                {
                    m_loopBackBufferParams[pass].pReferenceBuffer[bufferIndex] =
                        m_loopBackBufferParams[pass].pReferenceBufferManager->GetImageBuffer();

                    if (NULL == m_loopBackBufferParams[pass].pReferenceBuffer[bufferIndex])
                    {
                        CAMX_LOG_ERROR(CamxLogGroupPProc, "Node %s, pass %d, buffer not available failed %s",
                                       NodeIdentifierString(),
                                       pass,
                                       m_loopBackBufferParams[pass].bufferManagerName);
                        result = CamxResultENoMemory;
                        break;
                    }
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "node %s, pass %d,"
                                     "get buffer success %s, bufferindex %d , buffer %p, vaddr %p",
                                     NodeIdentifierString(),
                                     pass,
                                     m_loopBackBufferParams[pass].bufferManagerName,
                                     bufferIndex,
                                     m_loopBackBufferParams[pass].pReferenceBuffer[bufferIndex],
                                     m_loopBackBufferParams[pass].pReferenceBuffer[bufferIndex]->GetPlaneVirtualAddr(0, 0));
                }
            }
            if (CamxResultSuccess != result)
            {
                break;
            }
            else
            {
                m_loopBackBufferParams[pass].portEnable = TRUE;
                m_numOutputRefPorts++;

                SetConfigIOData(pConfigIOData,
                                &m_loopBackBufferParams[pass].format,
                                m_loopBackBufferParams[pass].outputPortID,
                                "ReferenceOutput");
                SetConfigIOData(pConfigIOData,
                                &m_loopBackBufferParams[pass].format,
                                m_loopBackBufferParams[pass].inputPortID,
                                "ReferenceInput");
            }
        }
    }
    return result;
}
CAMX_NAMESPACE_END
