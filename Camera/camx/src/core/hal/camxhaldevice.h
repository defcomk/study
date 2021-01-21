////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxhaldevice.h
/// @brief Declarations for HALDevice class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXHALDEVICE_H
#define CAMXHALDEVICE_H

#include "camxatomic.h"
#include "camxdefs.h"
#include "camxhal3defs.h"
#include "camxhal3stream.h"
#include "camxhal3types.h"
#include "camxhwenvironment.h"
#include "camxmetadatapool.h"
#include "camxchitypes.h"
#include "chioverride.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward Declaration
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum class ChiFormatType;

struct PipelineHandle;
struct PerPipelineInfo;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const UINT32 HALMaxPipelinesInUsecase    = 2;       ///< Max pipelines in a HAL usecase
static const UINT32 MaxOutstandingRequests      = 64;      ///< Number is bigger to prevent throttling of the preview pipeline

/// @brief Holds data related to framework request used to help debug when flush occurs
struct FrameworkRequestData
{
    UINT32      frameworkNum;           ///< Framework Number
    UINT        numBuffers;             ///< Number of output buffer streams
    UINT        numPartialRequest;      ///< Partial metadata
    BOOL        notifyRequestError;     ///< Indicator of notify error request message
    BOOL        notifyResultError;      ///< Indicator of notify error result message
    BOOL        notifyBufferError;      ///< Indicator of notify error buffer message
    BOOL        notifyShutter;          ///< Indicator of notify shutter message
    BOOL        indexUsed;              ///< Indicator for whether or not data has been entered for dumping purposes
};

/// @brief Contains array that holds most recent framework requests and the last index we used in the array
struct FrameworkRequestBuffer
{
    FrameworkRequestData    requestData[MaxOutstandingRequests];     ///< Array of framework requests
    UINT32                  lastFrameworkRequestIndex;               ///< Index of array we are currently using
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  HALDevice class that contains HAL API specific information
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class HALDevice
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Static create function to make an instance of this class
    ///
    /// @param  pHwModule       HwModule pointer
    /// @param  cameraId        CameraId
    /// @param  frameworkId     framework identifier
    ///
    /// @return Created HALDevice object pointer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static HALDevice* Create(
        const HwModule* pHwModule,
        UINT32          cameraId,
        UINT32          frameworkId);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Destroy
    ///
    /// @brief  Called to destroy the object instance
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID Destroy();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetCallbackOps
    ///
    /// @brief  Set the framework callback functions
    ///
    /// @param  pCamera3CbOps    Callback functions into the framework
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID SetCallbackOps(
        const camera3_callback_ops_t* pCamera3CbOps)
    {
        m_pCamera3CbOps = pCamera3CbOps;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetCallbackOps
    ///
    /// @brief  Get the callback functions into the framework
    ///
    /// @return Callback functions structure pointer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    const camera3_callback_ops_t* GetCallbackOps()
    {
        return m_pCamera3CbOps;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetCameraDevice
    ///
    /// @brief  Gets the camera3 device corresponding to the camera HAL device
    ///
    /// @return Pointer to the Camera3Device
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    Camera3Device* GetCameraDevice()
    {
        return &m_camera3Device;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureStreams
    ///
    /// @brief  This method is used by the application framework to setup new input and output streams to satisfy an
    ///         application use case. The framework will only call this method when no capture requests are being processed.
    ///         Subsequent calls to this method replace previously set stream configurations. This method will be called at
    ///         least once after Initialize() and before a request is submitted with process_capture_request(). This method is
    ///         an abstraction of the camera3_device_ops_t::configure_streams() API.
    ///
    /// @param  pStreamConfigs A list of input and output stream configurations
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ConfigureStreams(
        Camera3StreamConfig* pStreamConfigs);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConstructDefaultRequestSettings
    ///
    /// @brief  This method constructs default request settings expected for templetes.
    ///
    /// @param  requestTemplate template for settings.
    ///
    /// @return metadata pointer for default settings.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    const Metadata* ConstructDefaultRequestSettings(
        Camera3RequestTemplate requestTemplate);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Close
    ///
    /// @brief  This method closes the camera device created during open(). This may be called at any time when no other calls
    ///         from the framework are active and the call may block until all in-flight results have been returned and all
    ///         buffers filled. During and after the close() call, no further callbacks from the HAL should be expected and the
    ///         framework may not call any other HAL device functions. This method is an abstraction of the hw_device_t::close()
    ///         API.
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult Close();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CloseCachedSensorHandles
    ///
    /// @brief  Close Cached Sensor Handles while exiting the app. This is a check for releasing any HW handles that
    ///         were cached during the camera operation and failed to release due to an error or early exit from the app.
    ///
    /// @param  cameraID  Physical camera ID
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void CloseCachedSensorHandles(
        UINT32 cameraID);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetPhysicalCameraIDs
    ///
    /// @brief  Retrieve the physical camera IDs associated with the logical cameraID param.
    ///
    /// @param  cameraID       Logical camera ID
    /// @param  arraySize      Max size of pCDKPhysCamIds->physicalCameraIds[] provided by caller
    /// @param  pCDKPhysCamIds Array of cameraIds
    ///
    /// @return CamxResultSuccess on success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult GetPhysicalCameraIDs(
       UINT32                    cameraID,
       UINT32                    arraySize,
       CDKInfoPhysicalCameraIds* pCDKPhysCamIds);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ProcessCaptureRequest
    ///
    /// @brief  This method is used by the application framework to send a new capture request to the HAL. Calls from the
    ///         client will come one at a time from a single context. In a streaming use case, this method will be called
    ///         repeatedly. The HAL should not return from this call until it is ready to accept the next request to process.
    ///         After completing this call, the framework then expects the HAL to asynchronously return the resulting metadata
    ///         and buffer(s) in the process_capture_result() callback. This method is an abstraction of the
    ///         camera3_device_ops_t::process_capture_request() API.
    ///
    /// @param  pCaptureRequest The metadata and input/output buffers to use for the capture request.
    ///
    /// @return CamxResultSuccess or some failure code
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ProcessCaptureRequest(
        Camera3CaptureRequest* pCaptureRequest);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Dump
    ///
    /// @brief  This method is used by the application framework to print debugging state for the camera device. It will be
    ///         called when using the dumpsys tool or capturing a bugreport. This method is an abstraction of the
    ///         camera3_device_ops_t::dump() API.
    ///
    /// @param  fd The file descriptor which can be used to write debugging text using dprintf() or write().
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID Dump(
        INT fd
        ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Flush
    ///
    /// @brief  This method is used by the application framework to flush all in-process capture requests and all buffers in
    ///         the pipeline in preparation for a configure_streams() call. In-flight results and buffers are not required to
    ///         be returned; it is at the discretion of the HAL. Hardware will be synchronously stopped. This method is an
    ///         abstraction of the camera3_device_ops_t::flush() API.
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult Flush();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DestroyPipelines
    ///
    /// @brief  Destroy all the pipelines
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DestroyPipelines();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetFwCameraId
    ///
    /// @brief  Gets framework cameraID
    ///
    /// @return camera id
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    UINT32 GetFwCameraId()
    {
        return m_fwId;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetCameraId
    ///
    /// @brief  Gets the camera ID corresponding to this device
    ///
    /// @return CameraId
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT32 GetCameraId() const
    {
        return m_cameraId;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetFrameworkRequestData
    ///
    /// @brief  Gets the Framework Request Data struct instance for a given framework number
    ///
    /// @param  frameIndex     Framework index
    /// @param  updateIndex    Used to indicate we want a new Framework Request Data struct to be put in the buffer
    ///
    /// @return pFrameworkRequest
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE FrameworkRequestData* GetFrameworkRequestData(
        UINT32 frameIndex,
        BOOL   updateIndex)
    {
        FrameworkRequestData* pFrameworkRequest;

        pFrameworkRequest = &m_frameworkRequests.requestData[frameIndex];

        // Keep track of lastest index used in the buffer
        if (TRUE == updateIndex)
        {
            m_frameworkRequests.lastFrameworkRequestIndex = frameIndex;
        }

        return pFrameworkRequest;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpFrameworkRequests
    ///
    /// @brief  Dumps framework requests that are in error state
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpFrameworkRequests();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ClearFrameworkRequestBuffer
    ///
    /// @brief  Resets all data in Framework Request Buffer to default values
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ClearFrameworkRequestBuffer();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PopulateFrameworkRequestBuffer
    ///
    /// @brief  Initializes values of Framework Request buffer when we do a process capture request
    ///
    /// @param  pRequest  Pointer to the current capture request
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PopulateFrameworkRequestBuffer(
        Camera3CaptureRequest* pRequest);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateFrameworkRequestBufferResult
    ///
    /// @brief  Updates current values of Framework Request buffer related to the process capture result
    ///
    /// @param  pCamera3CaptureResult  Pointer to the current capture result
    /// @param  pFrameworkRequest      Pointer to the current framework request
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateFrameworkRequestBufferResult(
        const Camera3CaptureResult* pCamera3CaptureResult,
        FrameworkRequestData*       pFrameworkRequest);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateFrameworkRequestBufferNotify
    ///
    /// @brief  Updates current values of Framework Request buffer related to notify (shutter/error)
    ///
    /// @param  pCamera3NotifyMessage  Pointer to the current notify message
    /// @param  pFrameworkRequest      Pointer to the current framework request
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateFrameworkRequestBufferNotify(
        const Camera3NotifyMessage* pCamera3NotifyMessage,
        FrameworkRequestData*       pFrameworkRequest);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// NotifyMessage
    ///
    /// @brief  Sends warning/error notification messages to HAL
    ///
    /// @param  pHALDevice  Pointer to the HAL Device
    /// @param  pCameraMsg  Pointer to the Notify Message
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID NotifyMessage(
        HALDevice*                  pHALDevice,
        const Camera3NotifyMessage* pCameraMsg);

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// HALDevice
    ///
    /// @brief  Default constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    HALDevice() = default;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~HALDevice
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~HALDevice();

private:

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ProcessCaptureResult
    ///
    /// @brief  Defines the prototype for the callback method to send completed capture results to the framework
    ///
    /// @param  pCamera3Device          Camera device
    /// @param  pCamera3CaptureResult   Camera3 capture result
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID ProcessCaptureResult(
        const camera3_device_t*         pCamera3Device,
        const camera3_capture_result_t* pCamera3CaptureResult);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Notify
    ///
    /// @brief  Defines the prototype for the asynchronous notification callback method from to the framework
    ///
    /// @param  pCamera3Device           Camera3 device
    /// @param  pCamera3NotifyMessage    Camera3 capture result
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID Notify(
        const camera3_device_t*     pCamera3Device,
        const camera3_notify_msg_t* pCamera3NotifyMessage);

    HALDevice(const HALDevice&) = delete;                   ///< Disallow the copy constructor
    HALDevice& operator=(const HALDevice&) = delete;        ///< Disallow assignment operator

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initializes the object instance after creation
    ///
    /// @param  pHwModule   HwModule pointer
    /// @param  cameraId    CameraId
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult Initialize(
        const HwModule* pHwModule,
        UINT32          cameraId);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckValidStreamConfig
    ///
    /// @brief  Check if the incoming stream configuration is well formed and valid
    ///
    /// @param  pStreamConfigs A list of input and output stream configurations
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CheckValidStreamConfig(
        Camera3StreamConfig* pStreamConfigs
        ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckSupportedResolution
    ///
    /// @brief  Check if the incoming stream configuration is well formed and valid
    ///
    /// @param  cameraId   logical camera id
    /// @param  format     stream format
    /// @param  width      stream width
    /// @param  height     stream height
    /// @param  streamType stream type
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CheckSupportedResolution(
        INT32 cameraId,
        INT32 format,
        UINT32 width,
        UINT32 height,
        INT32 streamType) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// InitializeHAL3Streams
    ///
    /// @brief  This method initializes the internal HAL3Stream objects from the incoming stream configurations.
    ///
    /// @param  pStreamConfigs The framework-provided stream configurations from which to initialize internal stream
    ///                        configurations.
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult InitializeHAL3Streams(
        Camera3StreamConfig* pStreamConfigs);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DestroyUnusedStreams
    ///
    /// @brief  Destroy streams that are not currently used.
    ///
    /// @param  ppHAL3Streams   The list of streams to destroy
    /// @param  numStreams      The number of streams in ppHAL3Streams
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DestroyUnusedStreams(
        HAL3Stream** ppHAL3Streams,
        UINT32       numStreams
        ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SearchNumBatchedFrames
    ///
    /// @brief  Search the static table published in the static metadata to obtain the number of batched frames
    ///
    /// @param  pStreamConfigs A list of input and output stream configurations
    /// @param  pBatchSize     Batch size in case of HFR usecase
    /// @param  pFPSValue      Fps Value selected
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SearchNumBatchedFrames(
        Camera3StreamConfig* pStreamConfigs,
        UINT*                pBatchSize,
        UINT*                pFPSValue);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsPipelineRealtime
    ///
    /// @brief  Check if a pipeline is realtime or offline
    ///
    /// @param  pPipelineInfo Info about the pipeline, obtained after parsing topology XML
    ///
    /// @return TRUE if a pipeline is realtime, FALSE otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL IsPipelineRealtime(
        const PerPipelineInfo*  pPipelineInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CHIModuleInitialize
    ///
    /// @brief  Initialize the CHI for the Pipelines to to implement the usecase
    ///
    /// @param  pStreamConfigs A list of input and output stream configurations
    ///
    /// @return TRUE if Initialization is good, FALSE otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CHIModuleInitialize(
        Camera3StreamConfig* pStreamConfigs);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetCHIAppCallbacks
    ///
    /// @brief  Get the pointer to the CHI App callback functions structure.
    ///
    /// @return Chi Callback function pointer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE chi_hal_callback_ops_t* GetCHIAppCallbacks() const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsCHIModuleInitialized
    ///
    /// @brief  Query if the CHI modeule has been initialized
    ///
    /// @return TRUE if CHI has initialized, FALSE otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsCHIModuleInitialized() const
    {
        return m_bCHIModuleInitialized;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsFlushEnabled
    ///
    /// @brief  Query if flush is enabled
    ///
    /// @return TRUE if flush enabled, FALSE otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsFlushEnabled() const
    {
        return m_bFlushEnabled;
    }

    const camera3_callback_ops_t* m_pCamera3CbOps;                          ///< Camera3 callback ops needed by framework

    Camera3Device         m_camera3Device;                                  ///< Camera3 device needed by framework
    UINT                  m_numPipelines;                                   ///< Number of pipelines for the usecase
    HAL3Stream**          m_ppHAL3Streams;                                  ///< HAL3 streams
    UINT                  m_numStreams;                                     ///< Num streams
    UINT                  m_HALXMLTargetSelectedStreamId[MaxNumStreams];    ///< HAL output buffer to Stream Id mapping
    ChiFormatType         m_streamIdSelectedFormat[MaxNumStreams];          ///< Selected internal format for the stream
                                                                            ///  output buffer
    UINT                  m_HALXMLInputTargetStreamId[MaxNumStreams];       ///< HAL output buffer to Stream Id mapping
    VOID*                 m_pHALSession;                                    ///< HALSession
    UINT                  m_usecaseNumBatchedFrames;                        ///< Number of framework frames batched together if
                                                                            ///  batching is enabled
    UINT                  m_FPSValue;                                       ///< FPS value selected in HFR case.
    BOOL                  m_bCHIModuleInitialized;                          ///< CHI module Initialized
    BOOL                  m_bFlushEnabled;                                  ///< Flag for flush enabled
    chi_hal_ops_t         m_HALCallbacks;                                   ///< HAL callbacks provided to the CHI override
    UINT32                m_fwId;                                           ///< Framework device identifier
    UINT32                m_cameraId;                                       ///< ID of camera for which this instance was made
    UINT32                m_flushRequest[MaxOutstandingRequests];           ///< Frames that we have flushed

    Metadata*             m_pResultMetadata;                                ///< Framework Result metadata
    Metadata*             m_pDefaultRequestMetadata[RequestTemplateCount];  ///< A list of templates for
                                                                            ///< ConstructDefaultRequestSettings
    FrameworkRequestBuffer  m_frameworkRequests;                            ///< Array of framework requests
    UINT32                  m_numPartialResult;                             ///< Partial result count

};

CAMX_NAMESPACE_END

#endif // CAMXHALDEVICE_H
