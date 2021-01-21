/**
 * @file CameraEventLog.cpp
 *
 * @brief Implement logging API for key camera events
 *
 * Copyright (c) 2009-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "AEEstd.h"

#include "CameraEventLog.h"
#include "CameraOSServices.h"
 #include <stdio.h>

#ifdef CAMERA_FEATURE_ENABLE_TRACE_EVENT

#ifdef __LINUX
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <trace.h>

#elif defined(__QNXNTO__)
#include <sys/neutrino.h>
#include <sys/trace.h>
#endif

#endif

extern "C"
{
/*NOTE: this feature will require additional changes to enable it with multiple engines
 as they would both be logging to same trace buffer */
#ifdef CAMERA_FEATURE_ENABLE_LOG_EVENT
/* ===========================================================================
**                 DECLARATIONS AND DEFINITIONS FOR MODULE
** ======================================================================== */

/* --------------------------------------------------------------------------
** Constant / Define Declarations
** ----------------------------------------------------------------------- */
#define MAX_NUMBER_OF_EVENTS 1024
/* --------------------------------------------------------------------------
** Type Declarations
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Global Object Definitions
** ----------------------------------------------------------------------- */

// This stores the most recent MAX_NUMBER_OF_EVENTS events
static CameraEventInfoType s_CameraEvents[MAX_NUMBER_OF_EVENTS];

// This points to the next event
static volatile uint32 s_nNextCameraEvent;

// mutex protection for multithreads
static CameraMutex s_DeviceMutex;

static boolean s_initialized = FALSE;

/* --------------------------------------------------------------------------
** Local Object Definitions
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Forward Declarations
** ----------------------------------------------------------------------- */

/* ===========================================================================
**                      MACRO DEFINITIONS
** ======================================================================== */

/* ===========================================================================
**                      FUNCTION DEFINITIONS
** ======================================================================== */
/**
 * CameraEventLogInitialize
 */
CameraResult CameraEventLogInitialize(void)
{
    CameraResult result;

    CAM_MSG(MEDIUM, "Initializing CameraEventLog");

    if (s_initialized)
    {
        CAM_MSG(ERROR, "CameraEventLog already initialized");
        return CAMERA_SUCCESS;
    }

    result = CameraCreateMutex(&s_DeviceMutex);
    if (CAMERA_SUCCESS == result)
    {
        // Initialize all camera events
        (void) std_memset (&s_CameraEvents[0], 0,
             MAX_NUMBER_OF_EVENTS * sizeof (s_CameraEvents[0]));

        // Initialize the next camera event index
        s_nNextCameraEvent = 0;

        s_initialized = TRUE;
    }

    return result;
}

/**
 * CameraEventLogUninitialize
 */
CameraResult CameraEventLogUninitialize (void)
{
    CameraDestroyMutex(s_DeviceMutex);
    s_DeviceMutex = NULL;
    s_initialized = FALSE;

    return CAMERA_SUCCESS;
}

/**
 * CameraLogEvent
 */
void CameraLogEvent(
    CameraEventType cameraEvent,
    uint64 nEventPayload1,
    long result)
{

    if (!s_initialized)
    {
        CAM_MSG(ERROR, "CameraLogEvent is not initialized");
        return;
    }

    uint32 nNextCameraEvent;

    CameraLockMutex (s_DeviceMutex);

    nNextCameraEvent = s_nNextCameraEvent++;
    if (s_nNextCameraEvent >= MAX_NUMBER_OF_EVENTS)
    {
        s_nNextCameraEvent = 0;
    }

    CameraUnlockMutex (s_DeviceMutex);

    s_CameraEvents[nNextCameraEvent].cameraEvent = cameraEvent;
    s_CameraEvents[nNextCameraEvent].nEventPayload1 = nEventPayload1;
    s_CameraEvents[nNextCameraEvent].result = result;
}

uint32 GetCameraEventLogBufferSizeReq (void)
{
    return (uint32)sizeof(s_CameraEvents);
}

CameraResult GetCameraEventLogBuffer (
    byte* pEventLogBuffer,
    uint32 nEventLogBufferLen,
    uint32* pnEventLogBufferLenReq)
{
    CameraResult result = CAMERA_EFAILED;

    uint32 nSizeEventLog = sizeof (s_CameraEvents);

    if (NULL != pnEventLogBufferLenReq)
    {

        *pnEventLogBufferLenReq = nSizeEventLog;

        if ((NULL != pEventLogBuffer) &&
            (nEventLogBufferLen >= nSizeEventLog))
        {
            // Copy the event log
            std_memcpy_s(pEventLogBuffer, nEventLogBufferLen, s_CameraEvents, nSizeEventLog);
            result = CAMERA_SUCCESS;
        }
        else if (NULL == pEventLogBuffer && 0 == nEventLogBufferLen)
        {
            // If no input buffer provided it is not an error. Just report the length requried.
            result = CAMERA_SUCCESS;
        }
    }

    return result;
}

#elif defined(CAMERA_FEATURE_ENABLE_TRACE_EVENT)

static const char* get_event_Name(CameraEventType CameraEvent)
{

	switch(CameraEvent)
	{
	case CAMERA_EVENT_LATENCY_POLL:
		return "latency_poll";

	case CAMERA_EVENT_LATENCY_DEQUEUE:
		return "latency_deq";

	case CAMERA_EVENT_LATENCY_TRAN:
		return "latency_tran";

	case CAMERA_EVENT_LATENCY_SEND:
		return "latency_send";

	default:
		return "";

	}

	return NULL;
}

#ifdef __LINUX
void CameraLogEvent (
    CameraEventType CameraEvent,
    uint64 nEventPayload,
    long result )
{

	if( CameraEvent < CAMERA_EVENT_LATENCY_POLL &&
			CameraEvent > CAMERA_EVENT_LATENCY_SEND )
		return;

	if( result == 0 )
	{
		const uint32 size = 128;
		char str[size];
		const char* sz = get_event_Name(CameraEvent);
		snprintf(str,size,"%s_%lu",sz,nEventPayload);

		ATRACE_BEGIN(str);

	} else if( result == 1 )
	{
		ATRACE_END();

	}
}

#elif defined(__QNXNTO__)
void CameraLogEvent (
    CameraEventType CameraEvent,
    uint64 nEventPayload,
    long result)
{
	if( CameraEvent < CAMERA_EVENT_LATENCY_POLL &&
			CameraEvent > CAMERA_EVENT_LATENCY_SEND )
		return;
	else {
		const uint32 size = 128;
		char str[size];
		const char* sz = get_event_Name(CameraEvent);
		snprintf(str,size,"%s_%lu",sz,nEventPayload);
		if( result == 0 )
		{
			trace_logf(_NTO_TRACE_USERFIRST+1, "%s begin",str);

		} else if( result == 1 )
		{
			trace_logf(_NTO_TRACE_USERFIRST+1, "%s end",str);
		}
	}
}

#endif

#endif
} // extern "C"



