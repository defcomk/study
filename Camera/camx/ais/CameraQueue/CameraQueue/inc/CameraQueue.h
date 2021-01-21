#ifndef __CAMERAQUEUE_H_
#define __CAMERAQUEUE_H_

/**
 * @file CameraQueue.h
 *
 * @brief This file contains the declarations for CameraQueue
 *
 * Copyright (c) 2011-2013 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
**                      INCLUDE FILES
** ======================================================================== */
#include "CameraTypes.h"

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

/*
 * element copy to/from the queue functions prototypes
 */
typedef void  (*CameraQueueVCpyToFcn)(volatile void *pDstElem, const void *pSrcElem);
typedef void  (*CameraQueueVCpyFromFcn)(void *pDstElem, const volatile void *pSrcElem);

/// This handle represents a CameraQueue
typedef void* CameraQueue;

/// This represents a pointer to a Queue Data
typedef void* CameraQueueDataType;

/// This enum is used to specify the type of locking desired
typedef enum
{
    CAMERAQUEUE_LOCK_NONE = 0,  //No locking
    CAMERAQUEUE_LOCK_THREAD,    //Locking for non-ist
    CAMERAQUEUE_LOCK_IST,       //Locking for ist
    CAMERAQUEUE_LOCK_NUM,
    CAMERAQUEUE_LOCK_MAX = 0x7FFFFFFF
} CameraQueueLockType;

/// This structure is used to pass in creation parameters for a CameraQueue
typedef struct
{
    /* The number of elements to statically allocate upon queue creation. */
    uint32                  nCapacity;

    /* Size in bytes of each element to be enqueued. */
    uint32                  nDataSizeInBytes;

    /* Specify the intended use of the queue
     * The implementation may choose to implement a stronger locking type
     * than the one specified */
    CameraQueueLockType     eLockType;

    /* Optional copy access function from a src element to a volatile destination
     * element on the queue. This is called by CameraQueueEnqueue for element
     * transfer if it is provided. Leave NULL to use inbuilt simple payload copy */
    CameraQueueVCpyToFcn    pFcnVCpyTo;

    /* Optional copy access function from a volatile src element on the queue
     * to a destination element.  This is called by CameraQueueDequeue for
     * element transfer if it is provided. Leave NULL to use inbuilt simple
     * payload copy */
    CameraQueueVCpyFromFcn  pFcnVCpyFrom;

} CameraQueueCreateParamType;

/* ===========================================================================
**                      MACROS
** ======================================================================== */

/* ===========================================================================
**                      FUNCTION DECLARATIONS
** ======================================================================== */

/**
 * Creates queue and allocate memory for queue items and the queue structure.
 *
 * @param[out] phQueue Returns a queue handle.
 * @param[in]  CameraQueueCreateParamType pointer to struct specifying
 *                                         creation parameters
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EFAILED - if failed
 */
CameraResult CameraQueueCreate(CameraQueue* phQueue,
    const CameraQueueCreateParamType* pCreateParams);

/**
 * Destroy queue and frees memory for elements of the queue.
 *
 * Caller should stop all producer/consumer threads from
 * producing/consuming queue data before calling CameraQueueDestroy.
 * Not doing so may cause segfault.
 *
 * @param[in] hQueue Queue handle.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EFAILED - if failed
 */
CameraResult CameraQueueDestroy(CameraQueue hQueue);

/**
 * Insert element in a queue at the end.
 *
 * @param[in] hQueue Queue handle.
 * @param[in] dataIn Element to be inserted.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_ENOMORE - if hQueue is full
 *         CAMERA_EFAILED - if failed
 */
CameraResult CameraQueueEnqueue(CameraQueue hQueue,
    const CameraQueueDataType dataIn);

/**
 * Remove element from a queue at the head and frees memory.
 *
 * @param[in] hQueue Queue handle.
 * @param[out] dataOut Element that will be removed.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_ENOMORE - if hQueue is empty
 *         CAMERA_EFAILED - if failed
 */
CameraResult CameraQueueDequeue(CameraQueue hQueue,
    CameraQueueDataType dataOut);

/**
 * Remove element from a queue at the head of the queue
 *
 * @param[in] hQueue Queue handle.
 * @param[out] dataOut Element that will be removed.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_ENOMORE - if hQueue is empty
 *         CAMERA_EFAILED - if failed
 */
CameraResult CameraQueueDropHead(CameraQueue hQueue);

/**
 * Access next element in the queue.
 *
 * @param[in] hQueue : Queue handle.
 * @param[out] dataOut : pointer to the next element in the queue.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_ENOMORE - if hQueue is empty
 *         CAMERA_EFAILED - if failed
 */
CameraResult CameraQueueFront(CameraQueue hQueue,
    CameraQueueDataType dataOut,
    uint32 OutputBufferLength);

/**
 * Remove all elements from a queue and frees memory.
 *
 * @param[in] hQueue Queue handle.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EFAILED - if failed
 */
CameraResult CameraQueueClear(CameraQueue hQueue);

/**
 * Returns true if queue is empty, else false
 *
 * @param[in] hQueue Queue handle.
 * @param[out] pbIsEmptyOut Pointer to boolean with the resulting state.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EBADPARM - invalid parameter is passed in
 */
CameraResult CameraQueueIsEmpty(CameraQueue hQueue,
    boolean* pbIsEmptyOut);

/**
 * Returns true if queue is full, else false
 *
 * @param[in] pQueue Queue handle.
 * @param[out] pbIsEmptyOut Pointer to boolean with the resulting state.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EBADPARM - invalid parameter is passed in
 */
CameraResult CameraQueueIsFull(CameraQueue hQueue,
    boolean* pbIsFullOut);

/**
 * Returns the maximum capacity of the queue.
 *
 * @param[in] pQueue Queue handle.
 * @param[out] pnCapacityOut Pointer to an integer with the resulting capacity.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EBADPARM - invalid parameter is passed in
 */
CameraResult CameraQueueGetCapacity(CameraQueue hQueue,
    uint32* pnCapacityOut);

/**
 * Returns the number of available empty entries
 *
 * @param[in] pQueue Queue handle.
 * @param[out] pnCapacityOut Pointer to an integer with the resulting capacity.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EBADPARM - invalid parameter is passed in
 */
CameraResult CameraQueueGetAvailableEntries(CameraQueue hQueue,
    uint32* pnAvailableEntriesOut);


/**
 * Returns the number of items currently enqueued
 *
 * @param[in] pQueue Queue handle.
 * @param[out] pnLength Pointer to an integer with the amount of items enqueued.
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EBADPARM - invalid parameter is passed in
 */
CameraResult CameraQueueGetLength(CameraQueue hQueue,
    uint32* pnLength);

/**
 * Removes an entry from the Queue
 *
 * @param[in] pQueue Queue handle.
 * @param[in] dataIn Queue item to remove from the queue
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EBADPARM - invalid parameter is passed in
 */

CameraResult CameraQueueRemoveEntry(CameraQueue hQueue,
    const CameraQueueDataType dataIn);

/**
 * Iterate camera queue and calls client callback for each entry.

 * [in] pQueue: Queue handle.
 * [in] dataIn: Client data
 * [in] (*pt2Func): Client callback
 * @return void

 */

void CameraQueueIterate(CameraQueue hQueue, void* pDataIn, void (*pt2Func)(void*, void*));

#ifdef __cplusplus
} // extern "C"
#endif  // __cplusplus

#endif // __CAMERAQUEUE_H_

