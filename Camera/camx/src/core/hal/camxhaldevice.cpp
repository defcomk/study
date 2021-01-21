////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxhaldevice.cpp
/// @brief Definitions for HALDevice class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <system/camera_metadata.h>

#include "camxchitypes.h"
#include "camxchi.h"
#include "camxdebug.h"
#include "camxhal3entry.h"  /// @todo (CAMX-351) Remove this header
#include "camxhal3metadatautil.h"
#include "camxhal3defaultrequest.h"
#include "camxhal3module.h"
#include "camxhaldevice.h"
#include "camxhwcontext.h"
#include "camxhwfactory.h"
#include "camxmem.h"
#include "camxmemspy.h"
#include "camxosutils.h"
#include "camxpipeline.h"
#include "camxsession.h"
#include "camxtrace.h"
#include "camxsensornode.h"

#include "chioverride.h"

CAMX_NAMESPACE_BEGIN

// Forward declaration
struct SubDeviceProperty;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const BOOL AllowStreamReuse = FALSE;         ///< Flag used to enable stream reuse

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::~HALDevice
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
HALDevice::~HALDevice()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
HALDevice* HALDevice::Create(
    const HwModule* pHwModule,
    UINT32          cameraId,
    UINT32          frameworkId)
{
    CamxResult result     = CamxResultENoMemory;
    HALDevice* pHALDevice = CAMX_NEW HALDevice;

    if (NULL != pHALDevice)
    {
        pHALDevice->m_fwId = frameworkId;

        result = pHALDevice->Initialize(pHwModule, cameraId);

        if (CamxResultSuccess != result)
        {
            pHALDevice->Destroy();

            pHALDevice = NULL;
        }
    }

    return pHALDevice;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::Destroy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::Destroy()
{
    ThermalManager* pThermalManager = HAL3Module::GetInstance()->GetThermalManager();
    if (NULL != pThermalManager)
    {
        pThermalManager->UnregisterHALDevice(this);
    }

    if (NULL != m_pHALSession)
    {
        /// @todo (CAMX-1797) Remove this
        m_pHALSession = NULL;

        DestroyPipelines();
    }

    if (NULL != m_ppHAL3Streams)
    {
        for (UINT32 stream = 0; stream < m_numStreams; stream++)
        {
            if (NULL != m_ppHAL3Streams[stream])
            {
                CAMX_DELETE m_ppHAL3Streams[stream];
                m_ppHAL3Streams[stream] = NULL;
            }
        }

        CAMX_FREE(m_ppHAL3Streams);
        m_ppHAL3Streams = NULL;
    }

    if (NULL != m_pResultMetadata)
    {
        HAL3MetadataUtil::FreeMetadata(m_pResultMetadata);
        m_pResultMetadata = NULL;
    }

    for (UINT i = 0; i < RequestTemplateCount; i++)
    {
        if (NULL != m_pDefaultRequestMetadata[i])
        {
            HAL3MetadataUtil::FreeMetadata(m_pDefaultRequestMetadata[i]);
        }
    }
    CAMX_DELETE this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::DestroyPipelines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::DestroyPipelines()
{
    m_numPipelines = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HALDevice::Initialize(
    const HwModule* pHwModule,
    UINT32          cameraId)
{
    CamxResult result = CamxResultSuccess;

    m_cameraId = cameraId;

    if (CamxResultSuccess == result)
    {
        m_camera3Device.hwDevice.tag     = HARDWARE_DEVICE_TAG; /// @todo (CAMX-351) Get from local macro

#if ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
        m_camera3Device.hwDevice.version = CAMERA_DEVICE_API_VERSION_3_5;
#else
        m_camera3Device.hwDevice.version = CAMERA_DEVICE_API_VERSION_3_3;
#endif // ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28))

        m_camera3Device.hwDevice.close   = reinterpret_cast<CloseFunc>(GetHwDeviceCloseFunc());
        m_camera3Device.pDeviceOps       = reinterpret_cast<Camera3DeviceOps*>(GetCamera3DeviceOps());
        m_camera3Device.pPrivateData     = this;
        // NOWHINE CP036a: Need exception here
        m_camera3Device.hwDevice.pModule = const_cast<HwModule*>(pHwModule);

        m_HALCallbacks.process_capture_result = ProcessCaptureResult;
        m_HALCallbacks.notify_result          = Notify;
    }

    m_pHALSession = NULL;
    Utils::Memset(m_flushRequest, 0, sizeof(m_flushRequest));

    SIZE_T                entryCapacity;
    SIZE_T                dataSize;
    HAL3MetadataUtil::CalculateSizeAllMeta(&entryCapacity, &dataSize, TagSectionVisibleToFramework);

    m_pResultMetadata = HAL3MetadataUtil::CreateMetadata(
            entryCapacity,
            dataSize);

    for (UINT i = RequestTemplatePreview; i < RequestTemplateCount; i++)
    {
        if (NULL == m_pDefaultRequestMetadata[i])
        {
            ConstructDefaultRequestSettings(static_cast<Camera3RequestTemplate>(i));
        }
    }

    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    m_numPartialResult = pStaticSettings->numMetadataResults;

    /* We will increment the Partial result count by 1 if CHI also has its own implementation */
    if (CHIPartialDataSeparate == pStaticSettings->enableCHIPartialData)
    {
        m_numPartialResult++;
    }

    if (TRUE == pStaticSettings->enableThermalMitigation)
    {
        ThermalManager* pThermalManager = HAL3Module::GetInstance()->GetThermalManager();
        if (NULL != pThermalManager)
        {
            CamxResult resultThermalReg = pThermalManager->RegisterHALDevice(this);
            if (CamxResultEResource == resultThermalReg)
            {
                result = resultThermalReg;
            }
            // else Ignore result even if it fails. We don't want camera to fail due to any issues with initializing the
            // thermal engine
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::ProcessCaptureResult
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::ProcessCaptureResult(
    const camera3_device_t*         pCamera3Device,
    const camera3_capture_result_t* pCamera3_CaptureResult)
{
    const Camera3CaptureResult* pCamera3CaptureResult = reinterpret_cast<const Camera3CaptureResult*>(pCamera3_CaptureResult);
    HALDevice*                  pHALDevice            = static_cast<HALDevice*>(pCamera3Device->priv);
    CamxResult                  result                = CamxResultSuccess;
    const StaticSettings*       pSettings             = HwEnvironment::GetInstance()->GetStaticSettings();

    // Keep track of information related to request for error conditions
    FrameworkRequestData* pFrameworkRequest = pHALDevice->GetFrameworkRequestData(
        pCamera3CaptureResult->frameworkFrameNum % MaxOutstandingRequests, FALSE);

    pHALDevice->UpdateFrameworkRequestBufferResult(pCamera3CaptureResult, pFrameworkRequest);

    // if Request error TRUE, do not send metadata or buffer
    if (TRUE == pFrameworkRequest->notifyRequestError)
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Received ERROR_REQUEST, do not sent metadata or buffer for frame number:%d",
                                         pFrameworkRequest->frameworkNum);
        pHALDevice->DumpFrameworkRequests();
    }

    // if Result error TRUE, if result has metadata
    if (TRUE == pFrameworkRequest->notifyResultError)
    {
        if (NULL != pCamera3CaptureResult->pResultMetadata)
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "Received ERROR_RESULT, should not have recieved metadata for frame number:%d",
                                             pFrameworkRequest->frameworkNum);
            pHALDevice->DumpFrameworkRequests();
        }
    }

    if (pCamera3CaptureResult->numOutputBuffers > 0)
    {
        for (UINT i = 0; i < pCamera3CaptureResult->numOutputBuffers; i++)
        {
            Camera3StreamBuffer* pStreamBuffer =
                // NOWHINE CP036a: Google API requires const type
                const_cast<Camera3StreamBuffer*>(&pCamera3CaptureResult->pOutputBuffers[i]);
            pStreamBuffer->releaseFence = -1;
            CAMX_LOG_INFO(CamxLogGroupHAL,
                          "Returning framework result Frame: %d, Metadata: %p, Stream %p, Fmt: %d Width: %d Height: %d",
                          pCamera3CaptureResult->frameworkFrameNum,
                          pCamera3CaptureResult->pResultMetadata,
                          pCamera3CaptureResult->pOutputBuffers[i].pStream,
                          pCamera3CaptureResult->pOutputBuffers[i].pStream->format,
                          pCamera3CaptureResult->pOutputBuffers[i].pStream->width,
                          pCamera3CaptureResult->pOutputBuffers[i].pStream->height,
                          pCamera3CaptureResult->pOutputBuffers[i].releaseFence,
                          pCamera3CaptureResult->pOutputBuffers[i].bufferStatus);
        }
    }
    else if (NULL != pCamera3CaptureResult->pResultMetadata)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupHAL,
                      "Returning framework result metadata only for frame: %d, Metadata: %p",
                      pCamera3CaptureResult->frameworkFrameNum, pCamera3CaptureResult->pResultMetadata);
    }

    pHALDevice->GetCallbackOps()->process_capture_result(pHALDevice->GetCallbackOps(), pCamera3_CaptureResult);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::Notify
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::Notify(
    const camera3_device_t*     pCamera3Device,
    const camera3_notify_msg_t* pNotifyMessage)
{
    const Camera3NotifyMessage* pCamera3NotifyMessage = reinterpret_cast<const Camera3NotifyMessage*>(pNotifyMessage);
    HALDevice*                  pHALDevice            = static_cast<HALDevice*>(pCamera3Device->priv);

    UINT32                previewMappingIndex = 0;
    FrameworkRequestData* pFrameworkRequest   = NULL;

    if (MessageTypeShutter == pCamera3NotifyMessage->messageType)
    {
        previewMappingIndex = pCamera3NotifyMessage->message.shutterMessage.frameworkFrameNum % MaxOutstandingRequests;
    }
    else if (MessageTypeError == pCamera3NotifyMessage->messageType)
    {
        previewMappingIndex = pCamera3NotifyMessage->message.errorMessage.frameworkFrameNum % MaxOutstandingRequests;
    }

    pFrameworkRequest = pHALDevice->GetFrameworkRequestData(previewMappingIndex, FALSE);

    // Keep track of information related to request for error conditions
    pHALDevice->UpdateFrameworkRequestBufferNotify(pCamera3NotifyMessage, pFrameworkRequest);

    // if Result error || Buffer error, if Request error TRUE, Invalid error
    if (MessageCodeResult == pCamera3NotifyMessage->message.errorMessage.errorMessageCode ||
        MessageCodeBuffer == pCamera3NotifyMessage->message.errorMessage.errorMessageCode)
    {
        if (TRUE == pFrameworkRequest->notifyRequestError)
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "Recieved ERROR_RESULT or ERROR_BUFFER after sending ERROR_REQUEST for"
                                             " frame number: %d",
                                             pFrameworkRequest->frameworkNum);
            pHALDevice->DumpFrameworkRequests();
        }
    }

    if (MessageTypeShutter == pCamera3NotifyMessage->messageType)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupCore, "Setting before notify error %d and mapping index = %d",
                pCamera3NotifyMessage->message.errorMessage.frameworkFrameNum,
                pHALDevice->m_flushRequest[previewMappingIndex]);
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupCore, "Setting before notify error %d index = %d",
                pCamera3NotifyMessage->message.errorMessage.frameworkFrameNum,
                pHALDevice->m_flushRequest[previewMappingIndex]);
    }

    if ((0 == pHALDevice->m_flushRequest[previewMappingIndex]) &&
        (TRUE == pHALDevice->m_bFlushEnabled))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupCore, "notify previewMappingIndex %d, msg type %d",
            previewMappingIndex,
            pCamera3NotifyMessage->messageType);
        if (MessageTypeError == pCamera3NotifyMessage->messageType)
        {
            if (pCamera3NotifyMessage->message.errorMessage.errorMessageCode == MessageCodeRequest)
            {
                CAMX_LOG_INFO(CamxLogGroupCore, "Setting notify error");
                pHALDevice->m_flushRequest[previewMappingIndex] = 1;
            }
        }
    }

    if (NULL != pNotifyMessage)
    {
        CAMX_LOG_INFO(CamxLogGroupHAL, "type %08x", pNotifyMessage->type);
        switch (pNotifyMessage->type)
        {
            case MessageTypeError:
                if (FALSE == pHALDevice->m_bFlushEnabled)
                {
                    CAMX_LOG_ERROR(CamxLogGroupHAL, "    frame_number %d", pNotifyMessage->message.error.frame_number);
                    CAMX_LOG_ERROR(CamxLogGroupHAL, "    error_code %08x", pNotifyMessage->message.error.error_code);
                    CAMX_LOG_ERROR(CamxLogGroupHAL, "    error_stream %p", pNotifyMessage->message.error.error_stream);
                }
                else
                {
                    CAMX_LOG_INFO(CamxLogGroupHAL, "    frame_number %d", pNotifyMessage->message.error.frame_number);
                    CAMX_LOG_INFO(CamxLogGroupHAL, "    error_code %08x", pNotifyMessage->message.error.error_code);
                    CAMX_LOG_INFO(CamxLogGroupHAL, "    error_stream %p", pNotifyMessage->message.error.error_stream);
                }
                break;
            case MessageTypeShutter:
                CAMX_LOG_INFO(CamxLogGroupHAL, "    frame_number %d", pNotifyMessage->message.shutter.frame_number);
                CAMX_LOG_INFO(CamxLogGroupHAL, "    timestamp %llu", pNotifyMessage->message.shutter.timestamp);
                CAMX_TRACE_ASYNC_END_F(CamxLogGroupNone, pNotifyMessage->message.shutter.frame_number,
                    "SHUTTERLAG frameID: %d", pNotifyMessage->message.shutter.frame_number);
                break;
            default:
                CAMX_LOG_INFO(CamxLogGroupHAL, "Unknown message type");
                break;
        }
    }

    pHALDevice->GetCallbackOps()->notify(pHALDevice->GetCallbackOps(), pNotifyMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::NotifyMessage
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::NotifyMessage(
    HALDevice*                  pHALDevice,
    const Camera3NotifyMessage* pCamera3NotifyMessage)
{
    pHALDevice->GetCallbackOps()->notify(pHALDevice->GetCallbackOps(),
                                         reinterpret_cast<const camera3_notify_msg_t*>(pCamera3NotifyMessage));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::CheckValidStreamConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HALDevice::CheckValidStreamConfig(
    Camera3StreamConfig* pStreamConfigs
    ) const
{

    HwCameraInfo                  cameraInfo;
    const HwEnvironment*          pHWEnvironment  = HwEnvironment::GetInstance();
    const StaticSettings*         pSettings       = HwEnvironment::GetInstance()->GetStaticSettings();
    const SensorModuleStaticCaps* pSensorCaps     = NULL;

    CamxResult  result            = CamxResultSuccess;
    UINT32      numOutputStreams  = 0;
    UINT32      numInputStreams   = 0;
    UINT32      logicalCameraId   = GetCHIAppCallbacks()->chi_remap_camera_id(m_fwId, IdRemapTorch);

    result = pHWEnvironment->GetCameraInfo(logicalCameraId, &cameraInfo);

    if (CamxResultSuccess == result)
    {
        pSensorCaps = cameraInfo.pSensorCaps;

        if ((StreamConfigModeConstrainedHighSpeed == pStreamConfigs->operationMode) ||
            (StreamConfigModeSuperSlowMotionFRC == pStreamConfigs->operationMode))
        {
            BOOL isConstrainedHighSpeedSupported = FALSE;
            BOOL isJPEGSnapshotStream            = FALSE;

            for (UINT i = 0; i < cameraInfo.pPlatformCaps->numRequestCaps; i++)
            {
                if (RequestAvailableCapabilitiesConstrainedHighSpeedVideo == cameraInfo.pPlatformCaps->requestCaps[i])
                {
                    isConstrainedHighSpeedSupported = TRUE;
                    break;
                }
            }

            if (FALSE == isConstrainedHighSpeedSupported)
            {
                result = CamxResultEInvalidArg;
                CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid operationMode: %d", pStreamConfigs->operationMode);
            }

            // @todo (CAMX-4248) Remove it when we start to support live snapshot in the HFR mode
            // Check if we have any JPEG stream. So far we don`t support live snapshot in the HFR mode.
            for (UINT32 stream = 0; stream < pStreamConfigs->numStreams; stream++)
            {
                if ((StreamTypeOutput   == pStreamConfigs->ppStreams[stream]->streamType) &&
                    (HALPixelFormatBlob == pStreamConfigs->ppStreams[stream]->format)     &&
                    ((HALDataspaceJFIF  == pStreamConfigs->ppStreams[stream]->dataspace)  ||
                    (HALDataspaceV0JFIF == pStreamConfigs->ppStreams[stream]->dataspace)))
                {
                    isJPEGSnapshotStream = TRUE;
                    break;
                }
            }

            if (TRUE == isJPEGSnapshotStream)
            {
                result = CamxResultEUnsupported;
                CAMX_LOG_ERROR(CamxLogGroupHAL, "Non-supporting snapshot in operationMode: %d", pStreamConfigs->operationMode);
            }
        }
    }

    if (CamxResultSuccess == result)
    {
        const UINT                  numAvailableConfigs     = cameraInfo.pHwEnvironmentCaps->numStreamConfigs;
        const ScalerStreamConfig*   pAvailableConfigs       = cameraInfo.pHwEnvironmentCaps->streamConfigs;
        const UINT                  numInternalPixelFormat  = cameraInfo.pPlatformCaps->numInternalPixelFormats;
        const HALPixelFormat*       pInternalFormats        = cameraInfo.pPlatformCaps->internalPixelFormats;
        BOOL                        isAvailableStream;
        UINT                        numMatchedInputOutputConfigs;

        // Count the number of input and output streams
        for (UINT32 stream = 0; stream < pStreamConfigs->numStreams; stream++)
        {
            isAvailableStream               = FALSE;
            numMatchedInputOutputConfigs    = 0;

            // Check if the stream has a valid rotation
            if ((pStreamConfigs->ppStreams[stream]->rotation != StreamRotationCCW0)   &&
                (pStreamConfigs->ppStreams[stream]->rotation != StreamRotationCCW90)  &&
                (pStreamConfigs->ppStreams[stream]->rotation != StreamRotationCCW180) &&
                (pStreamConfigs->ppStreams[stream]->rotation != StreamRotationCCW270))
            {
                result = CamxResultEInvalidArg;
                CAMX_LOG_ERROR(CamxLogGroupHAL,
                    "Stream has an invalid rotation: %d",
                    pStreamConfigs->ppStreams[stream]->rotation);
                break;
            }

            switch (pStreamConfigs->ppStreams[stream]->streamType)
            {
                case StreamTypeOutput:
                    numOutputStreams++;
                    break;

                case StreamTypeInput:
                    numInputStreams++;
                    break;

                case StreamTypeBidirectional:
                    numOutputStreams++;
                    numInputStreams++;
                    break;

                default:
                    CAMX_ASSERT_ALWAYS_MESSAGE("Invalid streamStype: %d", pStreamConfigs->ppStreams[stream]->streamType);
                    result = CamxResultEInvalidArg;
                    break;
            }

            // Check if device support the stream configuration.
            if ((TRUE == pSettings->multiCameraVREnable) || (TRUE == pSettings->multiCameraEnable))
            {
                logicalCameraId   = GetCHIAppCallbacks()->chi_remap_camera_id(m_fwId, IdRemapCamera);
                isAvailableStream = CheckSupportedResolution(logicalCameraId, pStreamConfigs->ppStreams[stream]->format,
                                   pStreamConfigs->ppStreams[stream]->width , pStreamConfigs->ppStreams[stream]->height ,
                                   pStreamConfigs->ppStreams[stream]->streamType);
            }
            else
            {
                for (UINT32 i = 0; i < numAvailableConfigs; i++)
                {
                    if ((pStreamConfigs->ppStreams[stream]->format == pAvailableConfigs[i].format) &&
                        (pStreamConfigs->ppStreams[stream]->width  == pAvailableConfigs[i].width) &&
                        (pStreamConfigs->ppStreams[stream]->height == pAvailableConfigs[i].height))
                    {
                        if (pStreamConfigs->ppStreams[stream]->streamType == pAvailableConfigs[i].type)
                        {
                            isAvailableStream = TRUE;
                            break;
                        }
                        else if ((StreamTypeBidirectional == pStreamConfigs->ppStreams[stream]->streamType) &&
                                 ((ScalerAvailableStreamConfigurationsOutput == pAvailableConfigs[i].type) ||
                                 (ScalerAvailableStreamConfigurationsInput  == pAvailableConfigs[i].type)))
                        {
                                 // For StreamTypeBidirectional, both input and output stream configuration need to be supported
                                 // by device.
                            numMatchedInputOutputConfigs++;
                            if (ScalerAvailableStreamConfigurationsEnd == numMatchedInputOutputConfigs)
                            {
                                isAvailableStream = TRUE;
                                break;
                            }
                        }
                    }
                }
            }
            // Check if driver allow non-standard Android scaler stream format. e.g. HALPixelFormatY8
            if ((FALSE == isAvailableStream) && (TRUE == pSettings->enableInternalHALPixelStreamConfig))
            {
                for (UINT32 i = 0; i < numInternalPixelFormat; i++)
                {
                    if (pStreamConfigs->ppStreams[stream]->format == pInternalFormats[i])
                    {
                        isAvailableStream = TRUE;
                        break;
                    }
                }
            }

            // For Quad CFA sensor, if only expose quarter size to all apps (OEM app, 3rd app or CTS),
            // but OEM app will still config full size snapshot, need to accept it.
            if ((FALSE == isAvailableStream) &&
                (TRUE  == pSensorCaps->isQuadCFASensor) &&
                (FALSE == pSettings->exposeFullSizeForQCFA))
            {
                if ((StreamTypeOutput == pStreamConfigs->ppStreams[stream]->streamType) &&
                    (pStreamConfigs->ppStreams[stream]->width  <= static_cast<UINT32>(pSensorCaps->QuadCFADim.width)) &&
                    (pStreamConfigs->ppStreams[stream]->height <= static_cast<UINT32>(pSensorCaps->QuadCFADim.height)))
                {
                    CAMX_LOG_INFO(CamxLogGroupHAL,
                                   "Accept size (%d x %d) for Quad CFA sensor",
                                   pStreamConfigs->ppStreams[stream]->width,
                                   pStreamConfigs->ppStreams[stream]->height);

                    isAvailableStream = TRUE;
                }
            }

            if (FALSE == isAvailableStream)
            {
                result = CamxResultEInvalidArg;
                CAMX_LOG_ERROR(CamxLogGroupHAL,
                               "Invalid streamStype: %d, format: %d, width: %d, height: %d",
                               pStreamConfigs->ppStreams[stream]->streamType,
                               pStreamConfigs->ppStreams[stream]->format,
                               pStreamConfigs->ppStreams[stream]->width,
                               pStreamConfigs->ppStreams[stream]->height);
                break;
            }
        }
    }

    if (CamxResultSuccess == result)
    {
        // We allow 0 up to MaxNumOutputBuffers output streams. Zero output streams are allowed, despite the HAL3 specification,
        // in order to support metadata-only requests.
        if (numOutputStreams > MaxNumOutputBuffers)
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL,
                           "Invalid number of output streams (including bi-directional): %d",
                           numOutputStreams);
            result = CamxResultEInvalidArg;
        }

        // We allow 0 up to MaxNumInputBuffers input streams
        if (numInputStreams > MaxNumInputBuffers)
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL,
                           "Invalid number of input streams (including bi-directional): %d",
                           numInputStreams);
            result = CamxResultEInvalidArg;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::CheckSupportedResolution
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL HALDevice::CheckSupportedResolution(
    INT32 cameraId,
    INT32 format,
    UINT32 width,
    UINT32 height,
    INT32 streamType
    ) const
{
    CameraInfo info;

    if (0 != GetCHIAppCallbacks()->chi_get_camera_info(cameraId, reinterpret_cast<struct camera_info*>(&info)))
    {
        CAMX_LOG_INFO(CamxLogGroupHAL, "Can't get camera info for camera id: %d", cameraId);
        return FALSE;
    }
    // NOWHINE CP036a: The 'getters' for metadata because of being a sorted hashmap can modify the object
    Metadata* pStaticCameraInfo = const_cast<Metadata*>(info.pStaticCameraInfo);
    INT32* pAvailStreamConfig   = NULL;
    INT32 val = HAL3MetadataUtil::GetMetadata(pStaticCameraInfo, ScalerAvailableStreamConfigurations,
            reinterpret_cast<VOID**>(&pAvailStreamConfig));

    if (val != CamxResultSuccess)
    {
        CAMX_LOG_INFO(CamxLogGroupHAL, "Can't find the metadata entry for ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS");
        return FALSE;
    }

    if (NULL == pAvailStreamConfig)
    {
        CAMX_LOG_INFO(CamxLogGroupHAL,
                      "Value of the metadata entry for ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS is NULL");
        return FALSE;
    }

    SIZE_T entryCount = HAL3MetadataUtil::GetMetadataEntryCount(pStaticCameraInfo);

    INT32 matched = 0;

    if (streamType != StreamTypeBidirectional)
    {
        streamType = (streamType == StreamTypeOutput ? ScalerAvailableStreamConfigurationsOutput :
            (streamType == StreamTypeInput ? ScalerAvailableStreamConfigurationsInput : -1));

        if (-1 == streamType)
        {
            CAMX_LOG_WARN(CamxLogGroupHAL, "Invalid stream direction: %d", streamType);
            return FALSE;
        }
    }

    const INT32 STREAM_WIDTH_OFFSET       = 1;
    const INT32 STREAM_HEIGHT_OFFSET      = 2;
    const INT32 STREAM_IS_INPUT_OFFSET    = 3;
    const INT32 STREAM_CONFIGURATION_SIZE = 4;

    for (UINT32 i = 0; i < entryCount; i += STREAM_CONFIGURATION_SIZE)
    {
        /* This is a special case because, for bi-directional stream we need to ensure
         * given resolution is supported both as input and output */

        INT32 queriedFormat    = pAvailStreamConfig[i];
        UINT32 swidth          = pAvailStreamConfig[i + STREAM_WIDTH_OFFSET];
        UINT32 sheight         = pAvailStreamConfig[i + STREAM_HEIGHT_OFFSET];
        INT32 streamDirection  = pAvailStreamConfig[i + STREAM_IS_INPUT_OFFSET];

        if (StreamTypeBidirectional == streamType)
        {
            if ((ScalerAvailableStreamConfigurationsOutput == streamDirection) &&
                (format == queriedFormat))
            {
                if (swidth == width &&
                    sheight == height)
                {
                    matched++;
                }
            }
            if ((ScalerAvailableStreamConfigurationsInput == streamDirection) &&
                (format == queriedFormat))
            {
                if (swidth == width &&
                    sheight == height)
                {
                    matched++;
                }
            }
        }
        else
        {
            if (streamDirection == streamType && format == queriedFormat)
            {
                if (swidth == width &&
                    sheight == height)
                {
                    matched++;
                }
            }
        }
    }

    if ((StreamTypeBidirectional == streamType) && (matched == 2))
    {
        return FALSE;
    }
    if ((StreamTypeBidirectional != streamType) && (matched == 1))
    {
        return TRUE;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::InitializeHAL3Streams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HALDevice::InitializeHAL3Streams(
    Camera3StreamConfig* pStreamConfigs)
{
    CamxResult   result        = CamxResultSuccess;
    UINT32       stream        = 0;
    HAL3Stream*  pHAL3Stream   = NULL;
    HAL3Stream** ppHAL3Streams = NULL;

    ppHAL3Streams = static_cast<HAL3Stream**>(CAMX_CALLOC(sizeof(HAL3Stream*) * MaxNumOutputBuffers));

    if (NULL != ppHAL3Streams)
    {
        // Reset the existing list of streams, if any. This is done before checking to see if the stream can be reused since
        // the existing stream's flag will be updated directly.
        if (NULL != m_ppHAL3Streams)
        {
            for (stream = 0; (stream < MaxNumOutputBuffers) && (NULL != m_ppHAL3Streams[stream]); stream++)
            {
                m_ppHAL3Streams[stream]->SetStreamReused(FALSE);
            }
        }

        // For each incoming stream, check whether it matches an existing configuration that can be reused or if it is a new
        // configuration.
        for (stream = 0; stream < pStreamConfigs->numStreams; stream++)
        {
            CAMX_ENTRYEXIT_SCOPE_ID(CamxLogGroupHAL, SCOPEEventHAL3DeviceInitializeHAL3Stream, stream);

            Camera3Stream* pStream = pStreamConfigs->ppStreams[stream];

            // If the maximum number of inflight buffers allowed by this stream is > 0, we know the HAL has already seen this
            // stream configuration before since the buffer count comes from the HAL. Therefore, attempt to reuse it. If the
            // maximum number of buffers is 0, create a new HAL3 stream object.
            if ((pStream->maxNumBuffers > 0) && (TRUE == AllowStreamReuse))
            {
                pHAL3Stream = reinterpret_cast<HAL3Stream*>(pStream->pPrivateInfo);

                if ((NULL != pHAL3Stream) && (TRUE == pHAL3Stream->IsStreamConfigMatch(pStream)))
                {
                    pHAL3Stream->SetStreamReused(TRUE);
                    pHAL3Stream->SetStreamIndex(stream);
                    ppHAL3Streams[stream] = pHAL3Stream;
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid stream configuration!");
                    // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
                    result = CamxResultEInvalidArg;
                    ppHAL3Streams[stream] = NULL;
                    break;
                }
            }
            else
            {
                pHAL3Stream = CAMX_NEW HAL3Stream(pStream, stream, Format::RawMIPI);

                if (NULL != pHAL3Stream)
                {
                    ppHAL3Streams[stream] = pHAL3Stream;
                    pStream->pPrivateInfo = pHAL3Stream;
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupHAL, "Cannot create HAL3Stream! Out of memory!");

                    result = CamxResultENoMemory;
                    ppHAL3Streams[stream] = NULL;
                    break;
                }
            }
        }

        // If stream reuse and creation successful, store configurations in m_ppHAL3Streams, otherwise clean up
        if (CamxResultSuccess == result)
        {
            DestroyUnusedStreams(m_ppHAL3Streams, m_numStreams);

            m_ppHAL3Streams = ppHAL3Streams;
            m_numStreams    = pStreamConfigs->numStreams;
        }
        else
        {
            DestroyUnusedStreams(ppHAL3Streams, pStreamConfigs->numStreams);
            ppHAL3Streams = NULL;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Creating ppHAL3Streams failed! Out of memory!");
        result = CamxResultENoMemory;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::DestroyUnusedStreams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void HALDevice::DestroyUnusedStreams(
    HAL3Stream** ppHAL3Streams,
    UINT32       numStreams
    ) const
{
    if (NULL != ppHAL3Streams)
    {
        for (UINT32 stream = 0; stream < numStreams; stream++)
        {
            if ((NULL != ppHAL3Streams[stream]) && (FALSE == ppHAL3Streams[stream]->IsStreamReused()))
            {
                CAMX_DELETE ppHAL3Streams[stream];
                ppHAL3Streams[stream] = NULL;
            }
        }

        CAMX_FREE(ppHAL3Streams);
        ppHAL3Streams = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::GetCHIAppCallbacks
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE chi_hal_callback_ops_t* HALDevice::GetCHIAppCallbacks() const
{
    return (HAL3Module::GetInstance()->GetCHIAppCallbacks());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::ConfigureStreams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HALDevice::ConfigureStreams(
    Camera3StreamConfig* pStreamConfigs)
{
    CamxResult result = CamxResultSuccess;

    // Validate the incoming stream configurations
    result = CheckValidStreamConfig(pStreamConfigs);

    if ((StreamConfigModeConstrainedHighSpeed == pStreamConfigs->operationMode) ||
        (StreamConfigModeSuperSlowMotionFRC == pStreamConfigs->operationMode))
    {
        SearchNumBatchedFrames (pStreamConfigs, &m_usecaseNumBatchedFrames, &m_FPSValue);
        CAMX_ASSERT(m_usecaseNumBatchedFrames > 1);
    }
    else
    {
        // Not a HFR usecase batch frames value need to set to 1.
        m_usecaseNumBatchedFrames = 1;
    }

    if (CamxResultSuccess == result)
    {
        ClearFrameworkRequestBuffer();

        m_numPipelines = 0;

        if (TRUE == m_bCHIModuleInitialized)
        {
            GetCHIAppCallbacks()->chi_teardown_override_session(reinterpret_cast<camera3_device*>(&m_camera3Device), 0, NULL);
        }

        m_bCHIModuleInitialized = CHIModuleInitialize(pStreamConfigs);

        if (FALSE == m_bCHIModuleInitialized)
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL, "CHI Module failed to configure streams");
            result = CamxResultEFailed;
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupHAL, "CHI Module configured streams ... CHI is in control!");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::CHIModuleInitialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL HALDevice::CHIModuleInitialize(
    Camera3StreamConfig* pStreamConfigs)
{
    BOOL isOverrideEnabled = FALSE;

    if (TRUE == HAL3Module::GetInstance()->IsCHIOverrideModulePresent())
    {
        /// @todo (CAMX-1518) Handle private data from Override module
        VOID*                   pPrivateData;
        chi_hal_callback_ops_t* pCHIAppCallbacks  = GetCHIAppCallbacks();

        pCHIAppCallbacks->chi_initialize_override_session(GetCameraId(),
                                                          reinterpret_cast<const camera3_device_t*>(&m_camera3Device),
                                                          &m_HALCallbacks,
                                                          reinterpret_cast<camera3_stream_configuration_t*>(pStreamConfigs),
                                                          &isOverrideEnabled,
                                                          &pPrivateData);
    }

    return isOverrideEnabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::IsPipelineRealtime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL HALDevice::IsPipelineRealtime(
    const PerPipelineInfo*  pPipelineInfo)
{
    BOOL isRealtimePipeline = TRUE;

    for (UINT i = 0; i < pPipelineInfo->numNodes; i++)
    {
        if ((1    == pPipelineInfo->pNodeInfo[i].inputPorts.numPorts) &&
            (TRUE == pPipelineInfo->pNodeInfo[i].inputPorts.pPortInfo[0].flags.isSourceBuffer))
        {
            isRealtimePipeline = FALSE;
            break;
        }
    }

    return isRealtimePipeline;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::SearchNumBatchedFrames
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::SearchNumBatchedFrames(
    Camera3StreamConfig* pStreamConfigs,
    UINT*                pBatchSize,
    UINT*                pFPSValue)
{
    INT32 width    = 0;
    INT32 height   = 0;

    // We will take the following steps -
    //  1) We will search SupportedHFRVideoSizes, for the matching Video/Preview stream.
    //     Note: For the use case of multiple output streams, application must select one unique size from this metadata
    //           to use (e.g., preview and recording streams must have the same size). Otherwise, the high speed capture
    //           session creation will fail
    //  2) If a single entry is found in SupportedHFRVideoSizes, we choose the batchsize from that entry
    //  3) Else (multiple entries are found), we see if PropertyIDUsecaseFPS is published
    //  4)  If published, we pick the batchsize from that entry which is closest to the published FPS
    //  5)  If not published, we pick the batchsize from the first entry

    for (UINT streamIndex = 0 ; streamIndex < pStreamConfigs->numStreams; streamIndex++)
    {
        if (StreamTypeOutput == pStreamConfigs->ppStreams[streamIndex]->streamType)
        {
            width  = pStreamConfigs->ppStreams[streamIndex]->width;
            height = pStreamConfigs->ppStreams[streamIndex]->height;
            break;
        }
    }

    const PlatformStaticCaps*       pCaps                     =
        HwEnvironment::GetInstance()->GetPlatformStaticCaps();

    const HFRConfigurationParams*   pHFRParams[MaxHFRConfigs] = { NULL };
    UINT                            numHFREntries             = 0;

    if ((0 != width) && (0 != height))
    {
        for (UINT i = 0; i < pCaps->numDefaultHFRVideoSizes; i++)
        {
            if ((pCaps->defaultHFRVideoSizes[i].width == width) && (pCaps->defaultHFRVideoSizes[i].height == height))
            {
                // Out of the pair of entries in the table, we would like to store the second entry
                pHFRParams[numHFREntries++] = &pCaps->defaultHFRVideoSizes[i + 1];
                // Make sure that we don't hit the other entry in the pair, again
                i++;
            }
        }

        if (numHFREntries >= 1)
        {
            *pBatchSize = pHFRParams[0]->batchSizeMax;
            *pFPSValue = pHFRParams[0]->maxFPS;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL, "Failed to find supported HFR entry!");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::Close
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HALDevice::Close()
{
    CamxResult              result            = CamxResultSuccess;
    chi_hal_callback_ops_t* pCHIAppCallbacks  = GetCHIAppCallbacks();

    DumpFrameworkRequests();

    if (TRUE == IsCHIModuleInitialized())
    {
        m_bCHIModuleInitialized = FALSE;
        pCHIAppCallbacks->chi_teardown_override_session(reinterpret_cast<camera3_device*>(&m_camera3Device), 0, NULL);
    }

    // Move torch release to post Session Destroy as we might set error conditions accordingly.
    UINT32 logicalCameraId = pCHIAppCallbacks->chi_remap_camera_id(m_fwId, IdRemapTorch);

    HAL3Module::GetInstance()->ReleaseTorchForCamera(logicalCameraId, m_fwId);

    /// @todo (CAMX-1797) Add support for SetDebugBuffers(0);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::GetPhysicalCameraIDs
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HALDevice::GetPhysicalCameraIDs(
    UINT32                    cameraID,
    UINT32                    arraySize,
    CDKInfoPhysicalCameraIds* pCDKPhysCamIds)
{
    CamxResult result = CamxResultEFailed;

    if ((0 < arraySize) && (NULL != pCDKPhysCamIds))
    {
        CDKInfoNumCameras cdkNumPhysicalCams;
        CDKInfoCameraId   cdkCamId                = { cameraID };
        chi_hal_callback_ops_t* pCHIAppCallbacks  = GetCHIAppCallbacks();

        if (NULL != pCHIAppCallbacks)
        {
            result = pCHIAppCallbacks->chi_get_info(CDKGetInfoNumPhysicalCameras, &cdkCamId, &cdkNumPhysicalCams);

            CAMX_LOG_VERBOSE(CamxLogGroupHAL, "Logical CameraID %u has %u physical cameras",
                             cameraID, cdkNumPhysicalCams.numCameras);
        }

        if ((CamxResultSuccess == result)                &&
            (0 != cdkNumPhysicalCams.numCameras)         &&
            (arraySize >= cdkNumPhysicalCams.numCameras))
        {
            result = pCHIAppCallbacks->chi_get_info(CDKGetInfoPhysicalCameraIds, &cdkCamId, pCDKPhysCamIds);

            if (CamxResultSuccess == result)
            {
                CAMX_ASSERT(cdkNumPhysicalCams.numCameras == pCDKPhysCamIds->numCameras);

                for (UINT32 camIdx = 0; camIdx < pCDKPhysCamIds->numCameras; camIdx++)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupHAL, "Logical CameraID %u: physical cameraID %u (%u of %u)",
                                     cameraID, pCDKPhysCamIds->physicalCameraIds[camIdx],
                                     (camIdx+1), pCDKPhysCamIds->numCameras);
                }
            }
        }
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Failed to get physical camera ids result=%u", result);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::CloseCachedSensorHandles
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void HALDevice::CloseCachedSensorHandles(
    UINT32 cameraID)
{
    CDKInfoPhysicalCameraIds    cdkPhysCamIds;
    UINT32                      physicalCamIds[MaxNumImageSensors];

    CamxResult                  result          = CamxResultEFailed;
    const HwEnvironment*        pHWEnvironment  = HwEnvironment::GetInstance();

    if (NULL != pHWEnvironment)
    {
        cdkPhysCamIds.physicalCameraIds = &physicalCamIds[0];
        result                          = GetPhysicalCameraIDs(cameraID, MaxNumImageSensors, &cdkPhysCamIds);
    }

    if (CamxResultSuccess == result)
    {
        for (UINT32 camIdx = 0; camIdx < cdkPhysCamIds.numCameras; camIdx++)
        {
            HwCameraInfo    cameraInfo;
            UINT32          physicalCamId = cdkPhysCamIds.physicalCameraIds[camIdx];

            result = pHWEnvironment->GetCameraInfo(physicalCamId, &cameraInfo);

            if (CamxResultSuccess == result)
            {
                SensorSubDevicesCache* pSensorDevicesCache =
                    reinterpret_cast<SensorSubDevicesCache*>(cameraInfo.pSensorCaps->pSensorDeviceCache);

                if (NULL == pSensorDevicesCache)
                {
                    // Cannot release the opened and cached HW handles OR No handles cached to release
                    // This is not a fatal error, if the handles or cache is null that indicates there is no
                    // HW that was cached to begin with.
                    // If the HW environment is NULL, we would probably crash much earlier than this point
                    result = CamxResultEFailed;
                    CAMX_LOG_WARN(CamxLogGroupHAL, "pSensorDeviceCache=NULL for Logical CameraID=%u, PhysicalID=%u",
                                   cameraID, physicalCamId);
                }
                else
                {
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "Releasing resources for Logical CameraID=%u, PhysicalID=%u",
                                    cameraID, physicalCamId);
                    pSensorDevicesCache->ReleaseAllSubDevices(physicalCamId);

                    // NOWHINE CP036a: Need exception here
                    SensorModuleStaticCaps* pSensorCaps = const_cast<SensorModuleStaticCaps*>(cameraInfo.pSensorCaps);
                    for (UINT i = 0; i < MaxRTSessionHandles; i++)
                    {
                        pSensorCaps->hCSLSession[i] = CSLInvalidHandle;
                    }
                    result = CamxResultSuccess;

                    HwEnvironment::GetInstance()->InitializeSensorHwDeviceCache(
                        physicalCamId, NULL, CSLInvalidHandle, 0, NULL, NULL);
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupHAL, "Failed to retrieve cameraInfo for Logical CameraID=%u, PhysicalID=%u",
                               cameraID, physicalCamId);
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Failed to physical cameraIDs for Logical cameraID=%u", cameraID);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::ProcessCaptureRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HALDevice::ProcessCaptureRequest(
    Camera3CaptureRequest* pRequest)
{
    CamxResult result = CamxResultEFailed;

    m_flushRequest[pRequest->frameworkFrameNum % MaxOutstandingRequests] = 0;
    if (TRUE == IsCHIModuleInitialized())
    {
        // Keep track of information related to request for error conditions
        PopulateFrameworkRequestBuffer(pRequest);

        CAMX_LOG_INFO(CamxLogGroupHAL,
                      "CHIModule: Original framework framenumber %d contains %d output buffers",
                      pRequest->frameworkFrameNum,
                      pRequest->numOutputBuffers);

        /// @todo (CAMX-1797) CHI-only, do we need to pass in the device or probably not required
        CAMX_LOG_WARN(CamxLogGroupHAL, "CHIModuleProcessRequest ignoring CameraDevice and PrivateData.");
        result = GetCHIAppCallbacks()->chi_override_process_request(reinterpret_cast<const camera3_device*>(&m_camera3Device),
                                                                    reinterpret_cast<camera3_capture_request_t*>(pRequest),
                                                                    NULL);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "CHIModule disabled, rejecting HAL request");
    }

    CAMX_ASSERT(CamxResultSuccess == result);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::Dump
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::Dump(
    INT fd
    ) const
{
    CAMX_UNREFERENCED_PARAM(fd);
    // nothing worth dumping in here at the moment
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::Flush
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HALDevice::Flush()
{
    CAMX_ENTRYEXIT(CamxLogGroupCore);

    DumpFrameworkRequests();

    CamxResult result = CamxResultEInvalidArg;

    m_bFlushEnabled = TRUE;

    if ((NULL != GetCHIAppCallbacks()) && (NULL != m_camera3Device.pDeviceOps))
    {
        result = GetCHIAppCallbacks()->chi_override_flush(reinterpret_cast<const camera3_device*>(&m_camera3Device));
    }

    Utils::Memset(m_flushRequest, 0, sizeof(m_flushRequest));
    m_bFlushEnabled = FALSE;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::ConstructDefaultRequestSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const Metadata* HALDevice::ConstructDefaultRequestSettings(
    Camera3RequestTemplate requestTemplate)
{
    UINT requestTemplateIndex       = requestTemplate - 1; // Need to subtract 1 since the templates start at 0x1
    const Metadata* pMetadata       = NULL;

    if (NULL == m_pDefaultRequestMetadata[requestTemplateIndex])
    {
        const Metadata*          pOverrideMetadata  = NULL;
        const camera_metadata_t* pChiMetadata       = NULL;

        GetCHIAppCallbacks()->chi_get_default_request_settings(GetCameraId(), requestTemplate, &pChiMetadata);

        UINT32 logicalCameraId = GetCHIAppCallbacks()->chi_remap_camera_id(GetFwCameraId(), IdRemapTorch);

        pOverrideMetadata = reinterpret_cast<const Metadata*>(pChiMetadata);

        pMetadata = HAL3DefaultRequest::ConstructDefaultRequestSettings(logicalCameraId, requestTemplate);

        if ((NULL != pMetadata) && (NULL != pOverrideMetadata))
        {
            CamxResult result = CamxResultSuccess;

            // NOWHINE CP036a: Since google function is non-const, had to add the const_cast
            result = HAL3MetadataUtil::MergeMetadata(const_cast<Metadata*>(pMetadata), pOverrideMetadata);

            if (CamxResultSuccess == result)
            {
                CAMX_LOG_INFO(CamxLogGroupHAL, "Override specific tags added to construct default settings");

                const StaticSettings*   pSettings = HwEnvironment::GetInstance()->GetStaticSettings();
                UINT32 visibility = (TRUE == pSettings->MetadataVisibility) ?
                    TagSectionVisibility::TagSectionVisibleToFramework : TagSectionVisibility::TagSectionVisibleToAll;

                SIZE_T                requestEntryCapacity;
                SIZE_T                requestDataSize;

                HAL3MetadataUtil::CalculateSizeAllMeta(&requestEntryCapacity, &requestDataSize, visibility);

                m_pDefaultRequestMetadata[requestTemplateIndex] = HAL3MetadataUtil::CreateMetadata(
                    requestEntryCapacity,
                    requestDataSize);

                result = HAL3MetadataUtil::CopyMetadata(m_pDefaultRequestMetadata[requestTemplateIndex],
                    // NOWHINE CP036a: Need cast
                    const_cast<Metadata*>(pMetadata), visibility);

                if (CamxResultSuccess != result)
                {
                    // NOWHINE CP036a: Need cast
                    m_pDefaultRequestMetadata[requestTemplateIndex] = const_cast<Metadata*>(pMetadata);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupHAL, "Could not add override specific tags to construct default settings");
            }
        }
        else
        {
            if (NULL == pMetadata)
            {
                CAMX_LOG_ERROR(CamxLogGroupHAL, "Construct default settings failures");
            }
            if (NULL == pOverrideMetadata)
            {
                CAMX_LOG_INFO(CamxLogGroupHAL, "No override specific tags given by override");
            }
        }
    }

    return m_pDefaultRequestMetadata[requestTemplateIndex];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::DumpFrameworkRequests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::DumpFrameworkRequests()
{
    UINT32 latestFrameworkRequestIndex  = m_frameworkRequests.lastFrameworkRequestIndex;
    UINT32 count                        = MaxOutstandingRequests;

    while (count)
    {
        count--;
        if (TRUE == m_frameworkRequests.requestData[latestFrameworkRequestIndex].indexUsed)
        {
            // Check if framework request entry is in error state
            if ((0 != m_frameworkRequests.requestData[latestFrameworkRequestIndex].numBuffers)            ||
                (0 != m_frameworkRequests.requestData[latestFrameworkRequestIndex].numPartialRequest)     ||
                (TRUE == m_frameworkRequests.requestData[latestFrameworkRequestIndex].notifyRequestError) ||
                (TRUE == m_frameworkRequests.requestData[latestFrameworkRequestIndex].notifyResultError)  ||
                (TRUE == m_frameworkRequests.requestData[latestFrameworkRequestIndex].notifyBufferError)  ||
                (FALSE == m_frameworkRequests.requestData[latestFrameworkRequestIndex].notifyShutter))
            {
                CAMX_LOG_DUMP(CamxLogGroupHAL, "+   Error for Framework Request: %d, Num Output Buffers: %d, "
                        "Partial Request: %d, notifyRequestError: %s, notifyResultError: %s, notifyBufferError: %s, "
                        "Notify Shutter: %s",
                        m_frameworkRequests.requestData[latestFrameworkRequestIndex].frameworkNum,
                        m_frameworkRequests.requestData[latestFrameworkRequestIndex].numBuffers,
                        m_frameworkRequests.requestData[latestFrameworkRequestIndex].numPartialRequest,
                        Utils::BoolToString(m_frameworkRequests.requestData[latestFrameworkRequestIndex].notifyRequestError),
                        Utils::BoolToString(m_frameworkRequests.requestData[latestFrameworkRequestIndex].notifyResultError),
                        Utils::BoolToString(m_frameworkRequests.requestData[latestFrameworkRequestIndex].notifyBufferError),
                        Utils::BoolToString(m_frameworkRequests.requestData[latestFrameworkRequestIndex].notifyShutter));
            }
        }

        if (0 == latestFrameworkRequestIndex)
        {
            latestFrameworkRequestIndex = MaxOutstandingRequests - 1;
        }
        else
        {
            latestFrameworkRequestIndex--;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::ClearFrameworkRequestBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::ClearFrameworkRequestBuffer()
{
    Utils::Memset(&m_frameworkRequests, 0, sizeof(m_frameworkRequests));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::PopulateFrameworkRequestBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::PopulateFrameworkRequestBuffer(
    Camera3CaptureRequest* pRequest)
{
    FrameworkRequestData* pFrameworkRequest = GetFrameworkRequestData(
        pRequest->frameworkFrameNum % MaxOutstandingRequests, TRUE);

    pFrameworkRequest->frameworkNum         = pRequest->frameworkFrameNum;
    pFrameworkRequest->numBuffers           = pRequest->numOutputBuffers;
    pFrameworkRequest->numPartialRequest    = m_numPartialResult;
    pFrameworkRequest->notifyRequestError   = FALSE;
    pFrameworkRequest->notifyResultError    = FALSE;
    pFrameworkRequest->notifyBufferError    = FALSE;
    pFrameworkRequest->notifyShutter        = FALSE;
    pFrameworkRequest->indexUsed            = TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::UpdateFrameworkRequestBufferResult
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VOID HALDevice::UpdateFrameworkRequestBufferResult(
    const Camera3CaptureResult* pCamera3CaptureResult,
    FrameworkRequestData*       pFrameworkRequest)
{
    pFrameworkRequest->numBuffers = pFrameworkRequest->numBuffers - pCamera3CaptureResult->numOutputBuffers;

    if (0 != pCamera3CaptureResult->numPartialMetadata)
    {
        pFrameworkRequest->numPartialRequest--;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HALDevice::UpdateFrameworkRequestBufferNotify
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HALDevice::UpdateFrameworkRequestBufferNotify(
    const Camera3NotifyMessage* pCamera3NotifyMessage,
    FrameworkRequestData*       pFrameworkRequest)
{
    if (MessageTypeShutter == pCamera3NotifyMessage->messageType)
    {
        pFrameworkRequest->notifyShutter = TRUE;
    }

    if (MessageTypeError == pCamera3NotifyMessage->messageType)
    {
        if (MessageCodeRequest == pCamera3NotifyMessage->message.errorMessage.errorMessageCode)
        {
            pFrameworkRequest->notifyRequestError = TRUE;
        }

        if (MessageCodeResult == pCamera3NotifyMessage->message.errorMessage.errorMessageCode)
        {
            pFrameworkRequest->notifyResultError = TRUE;
        }

        if (MessageCodeBuffer == pCamera3NotifyMessage->message.errorMessage.errorMessageCode)
        {
            pFrameworkRequest->notifyBufferError = TRUE;
        }

    }
}

CAMX_NAMESPACE_END
