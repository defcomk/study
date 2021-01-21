#ifndef __CAMERAEVENTLOG_H_
#define __CAMERAEVENTLOG_H_

/**
 * @file CameraEventLog.h
 *
 * @brief Declarations needed for logging key camera events
 *
 * Copyright (c) 2009-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
**                      INCLUDE FILES
** ======================================================================== */
#include "CameraResult.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/* ===========================================================================
**                      DATA DECLARATIONS
** ======================================================================== */

/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------------
** Type Definitions
** ------------------------------------------------------------------------ */

/// This enumerates all key camera core and client events
typedef enum
{
    // Invalid event - intial value
    CAMERA_EVENT_INVALID,

    // CameraQueue Events
    CAMERA_CORE_EVENT_CAMERAQUEUECREATE,
    CAMERA_CORE_EVENT_CAMERAQUEUEDESTROY,
    CAMERA_CORE_EVENT_CAMERAQUEUECLEAR,
    CAMERA_CORE_EVENT_CAMERAQUEUEENQUEUE,
    CAMERA_CORE_EVENT_CAMERAQUEUEDEQUEUE,
    CAMERA_CORE_EVENT_CAMERAQUEUEISEMPTY,
    CAMERA_CORE_EVENT_CAMERAQUEUEDROPHEAD,

    // Sensor
    CAMERA_SENSOR_EVENT_PROBED,
    CAMERA_SENSOR_EVENT_INITIALIZE_START,
    CAMERA_SENSOR_EVENT_INITIALIZE_DONE,
    CAMERA_SENSOR_EVENT_STREAM_START,
    CAMERA_SENSOR_EVENT_STREAM_STOP,

	// latency
	CAMERA_EVENT_LATENCY_POLL,			/** poll spend time*/
	CAMERA_EVENT_LATENCY_DEQUEUE,		/** dequeue from kernel spend time*/
	CAMERA_EVENT_LATENCY_TRAN,			/** dequeue from kernel to before send spend time*/
	CAMERA_EVENT_LATENCY_SEND,			/** send to client spend time*/

    // Error event
    CAMERA_EVENT_ERROR,

    // Next event, indicating that the previous entry was the most recent
    // event that occured
    CAMERA_EVENT_NEXT
} CameraEventType;

/// This structure contains camera event information
typedef struct
{
    CameraEventType cameraEvent;
    uint64 nEventPayload1;
    long result;
} CameraEventInfoType;


/* ===========================================================================
**                      MACROS
** ======================================================================== */

/* ===========================================================================
**                      FUNCTION DECLARATIONS
** ======================================================================== */
#if defined(CAMERA_FEATURE_ENABLE_LOG_EVENT) || defined(CAMERA_FEATURE_ENABLE_TRACE_EVENT)
/**
 * This function is used to initialize Camera event log
 *
 * @param[] NONE
 */
CameraResult CameraEventLogInitialize (void);

/**
 * This function is used to uninitialize Camera event log
 *
 * @param[] NONE
 */
CameraResult CameraEventLogUninitialize (void);

/**
 * This function is used to log key camera events.
 *
 * @param[in] event this is specifying the
 * name of the core or client event to be logged
 * @param[in] nEventPayload this is specifying the
 * event payload to be logged
 * @param[in] result this is specifying the result
 * at the event to be logged
 */
CAM_API void CameraLogEvent (
    CameraEventType CameraEvent,
    uint64 nEventPayload,
    long result);

/**
 * Retrieves the size of the buffer needed to hold the camera event log buffer in bytes.
 * A buffer of this size should be provided to the GetCameraEventLogBuffer method.
 * @return Size of the tuning data set in bytes.
*/
uint32 GetCameraEventLogBufferSizeReq(void);

/**
 * Retrieves the camera event log buffer to a client-provided buffer. This data
 * should be written out with the JPEG header to support debug efforts.
 * @param[out] pEventLogBuffer Pointer to caller allocated memory where
 *             the debug trace buffer can be stored. This buffer size should
 *             be greater or equal to the number of bytes returned by
 *             GetDebugTraceBufferSizeReq.
 * @param[in]  nEventLogBufferLen Length of the caller allocated buffer passed
 *             in as pEventLogBuffer.
 * @param[out] pnEventLogBufferLenReq Number of bytes actually written to
 *             pEventLogBuffer. This may be different than the value
 *             returned by GetCameraEventLogBufferSizeReq.
 * @return CAMERA_SUCCESS if OK.
*/
CameraResult GetCameraEventLogBuffer(
    byte* pEventLogBuffer,
    uint32 nEventLogBufferLen,
    uint32* pnEventLogBufferLenReq);

#else

#define CameraLogEvent(...)

#endif /* CAMERA_FEATURE_ENABLE_LOG_EVENT */

#ifdef __cplusplus
} // extern "C"
#endif  // __cplusplus

#endif // __CAMERAEVENTLOG_H_
