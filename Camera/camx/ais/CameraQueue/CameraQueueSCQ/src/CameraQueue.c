/**
 * @file CameraQueue.c
 *
 * @brief This module contains QNX's CameraQueue.h implementation
 *
 * Copyright (c) 2009-2012,2014,2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/**
 * This CameraQueue implements a generic fixed size queue for
 *   N consumers and M producers.
 *
 * When N > 1, M > 1, caller should request CAMERAQUEUE_LOCK_THREAD or
 *   CAMERAQUEUE_LOCK_IST, where it would use pthread mutex or spin
 *   lock respectively when locking is needed.  (Spin lock locking is
 *   useful for when one of the thread is an IST.)
 *
 * When N = M = 1, caller can may request CAMERA_LOCK_NONE with special
 *   optimization - It is thread safe without the needs of mutex nor spin lock.
 *
 */

#if defined(CAMERA_UNITTEST)
#define DISABLE_QUEUE_LOCK_INTERRUPTS
#define EOK 0
#endif

/* disable interrupt lock for non-qnx OS */
#if !defined(__QNXNTO__)
#define DISABLE_QUEUE_LOCK_INTERRUPTS
#endif

#ifndef DISABLE_QUEUE_LOCK_INTERRUPTS
#include <sys/neutrino.h>
#endif
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "AEEstd.h"

#include "CameraEventLog.h"
#include "CameraOSServices.h"

#include "CameraQueue.h"
#include "CameraMemoryBarrier.h"

/* 'camq' is the magic identifier
 * - incoming handle without this magic identifer will be rejected.
 */
#define CAMERAQUEUE_MAGIC ((uint32)(('c' << 24)|('a' << 16)|('m' << 8)|('q')))


#ifdef __cplusplus
extern "C" {
#endif

#define OSINLINE static inline

typedef union
{
#ifndef DISABLE_QUEUE_LOCK_INTERRUPTS
    intrspin_t      hSpinlock;
#endif
    pthread_mutex_t hCritSect;
} CameraQueueLock;

typedef enum
{
    /**
    \brief \e 0x1       no. of free slots available to do 'Add' operation */
    CameraQueueCounterFree = 1,
    /**
    \brief \e 0x2       no. of used slots available to do 'Remove' operation */
    CameraQueueCounterUsed,
    /**
    \brief \e 0x3       the size of the queue */
    CameraQueueCounterSize,
    /**
    \brief \e 0x4       primarily for checking no. of available count type query possible */
    CameraQueueCounterNum,
    CameraQueueCounterMax = 0x7FFFFFFF
} CameraQueueCounter_t;

/*
 * OS lock/unlock function prototypes.  They
 *   return 0 on success; non-zero otherwise.
 */
typedef int   (*CameraQueueLockFcn)(CameraQueueLock *pLock);
typedef int   (*CameraQueueUnLockFcn)(CameraQueueLock *pLock);

typedef struct _cameraQueueCtx_t
{
    /* When set to CAMERAQUEUE_MAGIC, it means the incoming
     * handle points to a properly initialized CameraQueue */
    volatile uint32         magic;

    /* write index to the queue - ie. the 'tail' */
    volatile uint32         wrIndex;
    /* read index to the queue  - ie. the 'head' */
    volatile uint32         rdIndex;

    /* size of each element in the managed pArray */
    uint32                  elemSize;
    /* no. of element in the managed pArray */
    uint32                  elemNum;

    /* The array to be managed an circular queue, where
     * 'pArray' points to the beginning of an array with
     * 'elemNum' no. of elements, each of size 'elemSize'
     * bytes.  The max no. of elements allowed in this
     * queue is 'elemNum' - 1. */
    volatile void          *pArray;

    /* optional customized copy to/from functions
     * pArray element copy operations.  std_memove()
     * will be used if not supplied by caller.  */
    CameraQueueVCpyToFcn    pFcnVCpyTo;
    CameraQueueVCpyFromFcn  pFcnVCpyFrom;

    /* Select which type of protection mechanism to use. */
    CameraQueueLockType     eLockType;
    /* lock to use locking is enabled */
    CameraQueueLock         sLock;
    /* called whenever locking is needed to protect the queue */
    CameraQueueLockFcn      pFcnLock;
    /* called whenever unlock is needed to undo a prior lock */
    CameraQueueUnLockFcn    pFcnUnLock;

} cameraQueueCtx_t;

/*
 * --------------------------------------------------------------------------
 *                       Private Help Functions
 * --------------------------------------------------------------------------
 */
static CameraResult cameraQueueGetCounter(const cameraQueueCtx_t     *pCameraQueue,
                                          const CameraQueueCounter_t eCounter,
                                          uint32                     *pValue);

static int cameraQueueLockInit(cameraQueueCtx_t         *pCameraQueue,
                               const CameraQueueLockType eLockType);

static int cameraQueueLockDeInit(cameraQueueCtx_t *pCameraQueue);

static CameraResult cameraQueueProtectedGetCounter(CameraQueue                *hQueue,
                                                   const CameraQueueCounter_t eCounter,
                                                   uint32                     *pValue);

/******************************Internal*Routine******************************\
 * cameraQueueMemoryBarrier()
 *
 */
/**      \brief   The \b cameraQueueMemoryBarrier issues a DMB instruction
 *                only when no lock.
 *
 *       \param   [IN] eLock  the CameraQueueLockType to determine whether
 *                            DMB is will be issued.
 *
 *       \retval  none
 *
 *       \remarks The assumption is that when caller doesn't want any locking
 *                protection, the scenario is exactly 1 consumer, and 1 producer.
 *
 *       \sa
 *
 ***************************************************************************/
OSINLINE void cameraQueueMemoryBarrier(const CameraQueueLockType eLock)
{
    /* issue memory barrier only when there is no lock */
    if (CAMERAQUEUE_LOCK_NONE == eLock)
    {
        CAMERA_MEMORY_BARRIER();
    }
}

/******************************Internal*Routine******************************\
 * cameraQueueLockNone()
 *
 */
/**      \brief   The \b cameraQueueLockNone does nothing
 *
 *       \param   [IN] pLock  a CameraQueueLock to operate on
 *
 *       \retval  0 on success; otherwise non-zero
 *
 *       \remarks
 *
 *       \sa      cameraQueueUnLockNone
 *
 ***************************************************************************/
OSINLINE int cameraQueueLockNone(CameraQueueLock *pLock)
{
    (void)pLock;
    return 0;
}

/******************************Internal*Routine******************************\
 * cameraQueueUnLockNone()
 *
 */
/**      \brief   The \b cameraQueueUnLockNone does nothing
 *
 *       \param   [IN] pLock  a CameraQueueLockType to operate on
 *
 *       \retval  0 on success; otherwise non-zero
 *
 *       \remarks
 *
 *       \sa      cameraQueueLockNone
 *
 ***************************************************************************/
OSINLINE int cameraQueueUnLockNone(CameraQueueLock *pLock)
{
    (void)pLock;
    return 0;
}

/******************************Internal*Routine******************************\
 * cameraQueueLockThread()
 *
 */
/**      \brief   The \b cameraQueueLockThread enters the critical section
 *
 *       \param   [IN] pLock  a CameraQueueLock to operate on
 *
 *       \retval  0 on success; otherwise non-zero
 *
 *       \remarks It is caller's responsibility to initialize the critical section
 *
 *       \sa      cameraQueueUnLockThread
 *
 ***************************************************************************/
OSINLINE int cameraQueueLockThread(CameraQueueLock *pLock)
{
    return (EOK != pthread_mutex_lock(&(pLock->hCritSect)));
}

/******************************Internal*Routine******************************\
 * cameraQueueUnLockThread()
 *
 */
/**      \brief   The \b cameraQueueUnLockThread leaves the critical section
 *
 *       \param   [IN] pLock  a CameraQueueLock to operate on
 *
 *       \retval  0 on success; otherwise non-zero
 *
 *       \remarks It is caller's responsibility to initialize the critical section
 *
 *       \sa      cameraQueueLockThread
 *
 ***************************************************************************/
OSINLINE int cameraQueueUnLockThread(CameraQueueLock *pLock)
{
    return (EOK != pthread_mutex_unlock(&(pLock->hCritSect)));
}

#ifndef DISABLE_QUEUE_LOCK_INTERRUPTS
/******************************Internal*Routine******************************\
 * cameraQueueLockIST()
 *
 */
/**      \brief   The \b cameraQueueLockIST acquires the spinlock for the queue
 *
 *       \param   [IN] pLock  a CameraQueueLock to operate on
 *
 *       \retval  0 on success; otherwise non-zero
 *
 *       \remarks It is caller's responsibility to initialize the spinlock and
 *                to have requested privilege needed to operate on the spinlock
 *
 *       \sa      cameraQueueUnLockIST
 *
 ***************************************************************************/
OSINLINE int cameraQueueLockIST(CameraQueueLock *pLock)
{
    InterruptLock(&(pLock->hSpinlock));
    return 0;
}

/******************************Internal*Routine******************************\
 * cameraQueueUnLockIST()
 *
 */
/**      \brief   The \b cameraQueueUnLockIST unlocks the spinlock for the queue
 *
 *       \param   [IN] pLock  a CameraQueueLock to operate on
 *
 *       \retval  0 on success; otherwise non-zero
 *
 *       \remarks It is caller's responsibility to initialize the spinlock and
 *                to have requested privilege needed to operate on the spinlock
 *
 *       \sa      cameraQueueLockIST
 *
 ***************************************************************************/
OSINLINE int cameraQueueUnLockIST(CameraQueueLock *pLock)
{
    InterruptUnlock(&(pLock->hSpinlock));
    return 0;
}
#endif

/******************************Internal*Routine******************************\
 * cameraQueueLockInit()
 *
 */
/**      \brief   The \b cameraQueueLockInit initializes the queue context for
 *                the requested locking mechanism and requests necessary privileges
 *                for its subsequent operation.
 *
 *       \param   [IN/OUT] pCameraQueue  a cqueue context
 *       \param   [IN]     eLockType     the locking type requested
 *
 *       \retval  0 on success; otherwise non-zero
 *
 *       \remarks
 *
 *       \sa      cameraQueueLockDeInit
 *
 ***************************************************************************/
static int cameraQueueLockInit(cameraQueueCtx_t         *pCameraQueue,
                        const CameraQueueLockType eLockType)
{
    int err = 0;

    switch (eLockType)
    {
        case CAMERAQUEUE_LOCK_NONE:
            pCameraQueue->eLockType  = eLockType;
            pCameraQueue->pFcnLock   = cameraQueueLockNone;
            pCameraQueue->pFcnUnLock = cameraQueueUnLockNone;
            break;
#ifdef DISABLE_QUEUE_LOCK_INTERRUPTS
        case CAMERAQUEUE_LOCK_IST:
#endif
        case CAMERAQUEUE_LOCK_THREAD:
            err = pthread_mutex_init(&(pCameraQueue->sLock.hCritSect), 0);
            if (EOK == err)
            {
                pCameraQueue->eLockType  = eLockType;
                pCameraQueue->pFcnLock   = cameraQueueLockThread;
                pCameraQueue->pFcnUnLock = cameraQueueUnLockThread;
            }
            else
            {
                CAM_MSG(ERROR, "pthread_mutex_init failed : %s", strerror(err));
                err = 1;
            }
            break;
#ifndef DISABLE_QUEUE_LOCK_INTERRUPTS
        case CAMERAQUEUE_LOCK_IST:
            /* Notes :
             * - Request io privileges for InterruptLock
             * - Call could fail if process does not have permission
             * */
            if ((-1) == ThreadCtl( _NTO_TCTL_IO, 0 ))
            {
                CAM_MSG(ERROR, "ThreadCtl failed: %s", strerror(errno));
                err = 1;
            }
            else
            {
                pCameraQueue->eLockType  = eLockType;
                pCameraQueue->pFcnLock   = cameraQueueLockIST;
                pCameraQueue->pFcnUnLock = cameraQueueUnLockIST;
                /* initializes the spinlock */
                (void)std_memset(&(pCameraQueue->sLock.hSpinlock), 0, sizeof(pCameraQueue->sLock.hSpinlock));
            }
            break;
#endif
        case CAMERAQUEUE_LOCK_NUM:
        case CAMERAQUEUE_LOCK_MAX:
        default:
            /* TODO: Don't think this is the right behaviour - We should return error.
             *       Keep it backward compatible for now. */
            CAM_MSG(ERROR, "Unimplemented locking mechanism : using no locks");
            pCameraQueue->eLockType  = CAMERAQUEUE_LOCK_NONE;
            pCameraQueue->pFcnLock   = cameraQueueLockNone;
            pCameraQueue->pFcnUnLock = cameraQueueUnLockNone;
            break;

    }

    return err;
}


/******************************Internal*Routine******************************\
 * cameraQueueLockDeInit()
 *
 */
/**      \brief   The \b cameraQueueLockDeInit cleans up cameraQueueLockInit
 *
 *       \param   [IN] pCameraQueue  a cqueue context
 *
 *       \retval  0 on success; otherwise non-zero
 *
 *       \remarks
 *
 *       \sa      cameraQueueLockInit
 *
 ***************************************************************************/
static int cameraQueueLockDeInit(cameraQueueCtx_t *pCameraQueue)
{
    int ret = 0;

    if (CAMERAQUEUE_LOCK_THREAD == pCameraQueue->eLockType)
    {
        int err = pthread_mutex_destroy(&(pCameraQueue->sLock.hCritSect));
        if (EOK != err)
        {
            CAM_MSG(ERROR, "pthread_mutex_destroy failed : %s", strerror(err));
            ret = 1;
        }
    }
    return ret;
}


/******************************Internal*Routine******************************\
 * cameraQueueNextIndex()
 *
 *
 *       \brief   The \b cameraQueueNextIndex calculates and return the next
 *                value of curIndex based on the size of the queue.
 *
 *       \param   [IN] pCameraQueue  a cqueue context
 *       \param   [IN] curIndex  index to calculate the next value of
 *
 *       \retval  next value of curIndex
 *
 *       \remarks It is caller's responsibility to check param, and acquire
 *                mutex when needed.
 *
 *       \sa
 *
 ***************************************************************************/
OSINLINE uint32 cameraQueueNextIndex(const cameraQueueCtx_t *pCameraQueue, const uint32 curIndex)
{
    return ( ((pCameraQueue->elemNum-1) == curIndex) ? 0 : (curIndex+1) );
}


/******************************Internal*Routine******************************\
 * cameraQueueUsed()
 *
 *       \brief   The \b cameraQueueUsed calculated no. of queue entries contain
 *                unconsumed/valid elements.
 *
 *       \param   [IN] pCameraQueue  a cqueue context
 *
 *       \retval  no. of elements in the queue at the time of called
 *
 *       \remarks It is caller's responsibility to check param, and grab mutex
 *                when required.
 *
 *       \sa
 *
 ***************************************************************************/
OSINLINE uint32 cameraQueueUsed(const cameraQueueCtx_t *pCameraQueue)
{
    const uint32 wrIndex = pCameraQueue->wrIndex;
    const uint32 rdIndex = pCameraQueue->rdIndex;
    return ( (wrIndex < rdIndex) ?
             (pCameraQueue->elemNum - rdIndex + wrIndex) :
             (wrIndex - rdIndex) );
}


/******************************Internal*Routine******************************\
 * cameraQueueGetNode()
 *
 */
/**      \brief   The \b cameraQueueGetNode returns the node entry at 'nIndex'.
 *
 *       \param   [IN] pCameraQueue  a cqueue context
 *       \param   [IN] nIndex   the index of the interested node
 *
 *       \retval  The node corresponse to the 'nIndex'.
 *
 *       \remarks It is caller's responsibility to check param, and grab mutex
 *                when required.  Under normal circumstance, return value is
 *                always valid.
 *
 *       \sa
 *
 ***************************************************************************/
OSINLINE CameraQueueDataType cameraQueueGetNode(cameraQueueCtx_t  *pCameraQueue,
                                               const uint32        nIndex)
{
    /* returning &pArray[nIndex] */
    return (((char*)pCameraQueue->pArray) + nIndex*pCameraQueue->elemSize);
}


/******************************Internal*Routine******************************\
 * cameraQueueIsEmpty()
 *
 */
/**      \brief   The \b cameraQueueIsEmpty determines if the queue is empty
 *
 *       \param   [IN] pCameraQueue  a cqueue context
 *
 *       \retval  1 if empty; 0 otherwise.
 *
 *       \remarks It is caller's responsibility to check param, and grab mutex
 *                when required.
 *
 *       \sa CameraQueueIsFull()
 *
 ***************************************************************************/
OSINLINE int cameraQueueIsEmpty(const cameraQueueCtx_t *pCameraQueue)
{
    return ( (pCameraQueue->rdIndex == pCameraQueue->wrIndex) ? 1 : 0 );
}


/******************************Internal*Routine******************************\
 * cameraQueueIsFull()
 *
 */
/**      \brief   The \b cameraQueueIsFull determines if the queue is full
 *
 *       \param   [IN] pCameraQueue  a cqueue context
 *
 *       \retval  1 if full; 0 otherwise.
 *
 *       \remarks It is caller's responsibility to check param, and grab mutex
 *                when required.
 *
 *       \sa cameraQueueIsEmpty()
 *
 ***************************************************************************/
OSINLINE int cameraQueueIsFull(const cameraQueueCtx_t *pCameraQueue)
{
    return ( (cameraQueueNextIndex(pCameraQueue, pCameraQueue->wrIndex) == pCameraQueue->rdIndex) ? 1 : 0 );
}

/******************************Internal*Routine******************************\
 * cameraQueueGetCounter()
 *
 */
/**      \brief   The \b cameraQueueGetCounter allows caller to query one of
 *                the counters associated with the given cqueue.
 *
 *       \param   [IN/OUT] hQueue   The queue handle returned by a prior
 *                                  CameraQueueCreate call.
 *       \param   [IN]     eCounter The type of counter value to be returned
 *       \param   [OUT]    pValue   The value of the counter to be returned
 *
 *       \retval  if pValue is valid, CAMERA_SUCCESS
 *
 *       \remarks
 *
 *       \sa
 *
 ***************************************************************************/
static CameraResult cameraQueueGetCounter(const cameraQueueCtx_t     *pCameraQueue,
                                          const CameraQueueCounter_t eCounter,
                                          uint32                     *pValue)
{
    CameraResult result = CAMERA_SUCCESS;

    switch (eCounter)
    {
        case CameraQueueCounterFree:
            /* no. of entries that is free */
            *pValue = (pCameraQueue->elemNum-1) - cameraQueueUsed(pCameraQueue);
            break;
        case CameraQueueCounterUsed:
            /* no. of entries that is used */
            *pValue = cameraQueueUsed(pCameraQueue);
            break;
        case CameraQueueCounterSize:
            /* queue size that never changes */
            *pValue = pCameraQueue->elemNum-1;
            break;
        default:
            /* returning error */
            result = CAMERA_ENOSUCH;
            break;
    }

    return result;
}


/******************************Internal*Routine******************************\
 * cameraQueueProtectedGetCounter()
 *
 */
/**      \brief   The \b cameraQueueProtectedGetCounter calls cameraQueueGetCounter
 *                with standard CameraQueue public API error checkings. It validates
 *                handle, and use locks, if enabled.
 *
 *       \param   [IN]  hQueue   The queue handle returned by a prior CameraQueueCreate()
 *       \param   [IN]  eCounter The type of counter value to be returned
 *       \param   [OUT] pValue   value to be returned when returning CAMERA_SUCCESS
 *
 *       \retval  CAMERA_SUCCESS only if successful
 *
 *       \remarks
 *
 *       \sa CameraQueueGetCounter()
 *
 ***************************************************************************/
static CameraResult cameraQueueProtectedGetCounter(CameraQueue                *hQueue,
                                                   const CameraQueueCounter_t eCounter,
                                                   uint32                     *pValue)
{
    CameraResult      result;
    cameraQueueCtx_t *pCameraQueue = (cameraQueueCtx_t*) hQueue;

    if (!pCameraQueue || !pValue)
    {
        result = CAMERA_EBADPARM;
    }
    else if (CAMERAQUEUE_MAGIC != pCameraQueue->magic)
    {
        CAM_MSG(ERROR, "invalid or destroyed queue handle");
        result = CAMERA_EBADHANDLE;
    }
    else if (0 != pCameraQueue->pFcnLock(&pCameraQueue->sLock))
    {
        CAM_MSG(ERROR, "failed to lock hQueue=0x%p", hQueue);
        result = CAMERA_EFAILED;
    }
    else
    {
        result = cameraQueueGetCounter(pCameraQueue, eCounter, pValue);

        if (0 != pCameraQueue->pFcnUnLock(&pCameraQueue->sLock))
        {
            CAM_MSG(ERROR, "failed to unlock queue=0x%p", hQueue);
            if (CAMERA_SUCCESS == result) { result = CAMERA_EFAILED; }
        }
    }

    return result;
}


/*
 * --------------------------------------------------------------------------
 *                              Public API
 * --------------------------------------------------------------------------
 */
CAM_API CameraResult CameraQueueCreate(CameraQueue* phQueue,
    const CameraQueueCreateParamType* pCreateParams)
{
    CameraResult result = CAMERA_SUCCESS;
    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUECREATE, 0, 0);

    /* input validation */
    if ( phQueue && pCreateParams                         && /* check valid pointers    */
         (pCreateParams->nDataSizeInBytes > 0)            && /* check zero size element */
         (pCreateParams->nCapacity > 0)                   && /* minimal array element   */
         (pCreateParams->eLockType < CAMERAQUEUE_LOCK_NUM) ) /* valid locking type      */
    {
        /* allocate queue context */
        void *pMem;
        if ( 0 == (pMem = CameraAllocate(CAMERA_ALLOCATE_ID_CAMERA_QUEUE_CTXT,
                                         sizeof(cameraQueueCtx_t))))
        {
            CAM_MSG(ERROR, "failed to allocate memory for queue");
            result = CAMERA_ENOMEMORY;
        }
        else
        {
            cameraQueueCtx_t* pCameraQueue = (cameraQueueCtx_t*) pMem;
            const uint32 nInternalCapacity = pCreateParams->nCapacity + 1;
            const uint32 nArraySize        = (pCreateParams->nDataSizeInBytes *
                                              nInternalCapacity);

            STD_ZEROAT(pCameraQueue);

            /* allocate the managed array */
            if (0 == (pCameraQueue->pArray = CameraAllocate(CAMERA_ALLOCATE_ID_CAMERA_QUEUE_ARRAY,
                                                            nArraySize)))
            {
                CAM_MSG(ERROR, "failed to allocate memory for queue data array");
                CameraFree(CAMERA_ALLOCATE_ID_CAMERA_QUEUE_CTXT, pCameraQueue);
                result = CAMERA_ENOMEMORY;
            }
            /* init lock related fields/state */
            else if (0 != cameraQueueLockInit(pCameraQueue, pCreateParams->eLockType))
            {
                CAM_MSG(ERROR, "failed to init lock type for queue");
                CameraFree(CAMERA_ALLOCATE_ID_CAMERA_QUEUE_ARRAY, (void*)pCameraQueue->pArray);
                CameraFree(CAMERA_ALLOCATE_ID_CAMERA_QUEUE_CTXT, pCameraQueue);
                result = CAMERA_EFAILED;
            }
            else /* success */
            {
                (void)std_memset((void*)pCameraQueue->pArray, 0, nArraySize);

                pCameraQueue->elemSize     = pCreateParams->nDataSizeInBytes;
                pCameraQueue->elemNum      = nInternalCapacity;

                /* Use copy functions if provided */
                pCameraQueue->pFcnVCpyTo   = pCreateParams->pFcnVCpyTo;
                pCameraQueue->pFcnVCpyFrom = pCreateParams->pFcnVCpyFrom;

                pCameraQueue->magic        = CAMERAQUEUE_MAGIC;
                *phQueue                   = (CameraQueue)pCameraQueue;
            }
        }
    }
    else
    {
        result = CAMERA_EBADPARM;
    }

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUECREATE, 0, 0);

    return result;
}

CAM_API CameraResult CameraQueueDestroy(CameraQueue hQueue)
{
    CameraResult result = CAMERA_SUCCESS;
    cameraQueueCtx_t *pCameraQueue;

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUEDESTROY, 0, 0);

    if (0 == (pCameraQueue = (cameraQueueCtx_t*)hQueue))
    {
        result = CAMERA_EBADPARM;
    }
    else if (CAMERAQUEUE_MAGIC != pCameraQueue->magic)
    {
        CAM_MSG(ERROR, "invalid or destroyed queue handle");
        result = CAMERA_EBADHANDLE;
    }
    else if (0 != pCameraQueue->pFcnLock(&pCameraQueue->sLock))
    {
        CAM_MSG(ERROR, "possible concurrent destory or memory corruption");
        result = CAMERA_EHEAP;
    }
    else
    {
        pCameraQueue->magic = 0; /* mark queue handle invalid */

        if (0 != pCameraQueue->pFcnUnLock(&pCameraQueue->sLock))
        {
            CAM_MSG(ERROR, "pCameraQueue->pFcnUnLock failed ignored on 0x%p", pCameraQueue);
        }
        if (0 != cameraQueueLockDeInit(pCameraQueue))
        {
            CAM_MSG(ERROR, "cameraQueueOSDeInit failed ignored on 0x%p", pCameraQueue);
        }

        CameraFree(CAMERA_ALLOCATE_ID_CAMERA_QUEUE_ARRAY, (void*)pCameraQueue->pArray);
        CameraFree(CAMERA_ALLOCATE_ID_CAMERA_QUEUE_CTXT, pCameraQueue);
    }

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUEDESTROY, 0, 0);

    return result;
}


CAM_API CameraResult CameraQueueClear(CameraQueue hQueue)
{
    CameraResult result = CAMERA_SUCCESS;
    cameraQueueCtx_t* pCameraQueue = (cameraQueueCtx_t*) hQueue;

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUECLEAR, 0, 0);

    if (0 == (pCameraQueue = (cameraQueueCtx_t*) hQueue))
    {
        result = CAMERA_EBADPARM;
    }
    else if (CAMERAQUEUE_MAGIC != pCameraQueue->magic)
    {
        CAM_MSG(ERROR, "invalid or destroyed queue handle");
        result = CAMERA_EBADHANDLE;
    }
    else if (0 != pCameraQueue->pFcnLock(&pCameraQueue->sLock))
    {
        CAM_MSG(ERROR, "failed to lock hQueue=0x%p", hQueue);
        result = CAMERA_EFAILED;
    }
    else
    {
        pCameraQueue->wrIndex = pCameraQueue->rdIndex = 0;
        if (0 != pCameraQueue->pFcnUnLock(&pCameraQueue->sLock))
        {
            CAM_MSG(ERROR, "failed to unlock queue=0x%p", hQueue);
            result = CAMERA_EFAILED;
        }
    }

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUECLEAR, 0, 0);

    return result;
}


CAM_API CameraResult CameraQueueEnqueue(CameraQueue               hQueue,
                                const CameraQueueDataType dataIn)
{
    CameraResult      result       = CAMERA_SUCCESS;
    cameraQueueCtx_t* pCameraQueue = (cameraQueueCtx_t*) hQueue;

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUEENQUEUE, 0, 0);

    if (!pCameraQueue || !dataIn)
    {
        result = CAMERA_EBADPARM;
    }
    else if (CAMERAQUEUE_MAGIC != pCameraQueue->magic)
    {
        CAM_MSG(ERROR, "invalid or destroyed queue handle");
        result = CAMERA_EBADHANDLE;
    }
    else if (0 != pCameraQueue->pFcnLock(&pCameraQueue->sLock))
    {
        CAM_MSG(ERROR, "failed to lock hQueue=0x%p", hQueue);
        result = CAMERA_EFAILED;
    }
    else
    {
        if (cameraQueueIsFull(pCameraQueue))
        {
            /* caller attempted to enqueue more than allocated for or it wants
             * to know if the queue is full by doing an enqueue */
            CAM_MSG(MEDIUM, "caller needs to allocate more elements than created");
            result = CAMERA_ENOMORE;
        }
        else
        {
            /* get the node corresponding to the current write index */
            volatile CameraQueueDataType pNode = cameraQueueGetNode(pCameraQueue, pCameraQueue->wrIndex);
            if (pNode)
            {
                /* Copy the client's node to the current node */
                if (pCameraQueue->pFcnVCpyTo)
                {
                    pCameraQueue->pFcnVCpyTo(pNode, dataIn);
                }
                else
                {
                    std_memmove(pNode, dataIn, pCameraQueue->elemSize);
                }
                /* update write index */
                pCameraQueue->wrIndex = cameraQueueNextIndex(pCameraQueue, pCameraQueue->wrIndex);
                /* multi-core only */
                cameraQueueMemoryBarrier(pCameraQueue->eLockType);
                /* returing success */
                result  = CAMERA_SUCCESS;
            }
            else
            {
                /* impossible to happen */
                CAM_MSG(ERROR, "possible memory corruption detected");
                result = CAMERA_EHEAP;
            }
        }

        /* unlock queue context */
        if (0 != pCameraQueue->pFcnUnLock(&pCameraQueue->sLock))
        {
            CAM_MSG(ERROR, "failed to unlock queue=0x%p", hQueue);
            if (CAMERA_SUCCESS == result) { result = CAMERA_EFAILED; }
        }
    }

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUEENQUEUE, 0, 0);
    return result;
}

CAM_API CameraResult CameraQueueDequeue(CameraQueue         hQueue,
                                CameraQueueDataType dataOut)
{
    CameraResult result;
    cameraQueueCtx_t *pCameraQueue = (cameraQueueCtx_t*) hQueue;

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUEDEQUEUE, 0, 0);

    if (!pCameraQueue || !dataOut)
    {
        result = CAMERA_EBADPARM;
    }
    else if (CAMERAQUEUE_MAGIC != pCameraQueue->magic)
    {
        CAM_MSG(ERROR, "invalid or destroyed queue handle");
        result = CAMERA_EBADHANDLE;
    }
    else if (0 != pCameraQueue->pFcnLock(&pCameraQueue->sLock))
    {
        CAM_MSG(ERROR, "failed to lock hQueue=0x%p", hQueue);
        result = CAMERA_EFAILED;
    }
    else
    {
        if (cameraQueueIsEmpty(pCameraQueue))
        {
            /* caller attempted to dequeue but queue is empty - this is a normal use-case */
            result = CAMERA_ENOMORE;
        }
        else
        {
            /* get the node corresponding to the current read index */
            volatile CameraQueueDataType pNode = cameraQueueGetNode(pCameraQueue, pCameraQueue->rdIndex);
            if (pNode)
            {
                /* Copy the current node to the client's node */
                if (pCameraQueue->pFcnVCpyFrom)
                {
                    pCameraQueue->pFcnVCpyFrom(dataOut, pNode);
                }
                else
                {
                    std_memmove(dataOut, pNode, pCameraQueue->elemSize);
                }
                /* update read index */
                pCameraQueue->rdIndex = cameraQueueNextIndex(pCameraQueue, pCameraQueue->rdIndex);
                /* multi-core only */
                cameraQueueMemoryBarrier(pCameraQueue->eLockType);
                result = CAMERA_SUCCESS;
            }
            else
            {
                /* impossible to happen */
                CAM_MSG(ERROR, "possible memory corruption");
                result = CAMERA_EHEAP;
            }
        }

        /* unlock queue context */
        if (0 != pCameraQueue->pFcnUnLock(&pCameraQueue->sLock))
        {
            CAM_MSG(ERROR, "failed to unlock queue=0x%p", hQueue);
            if (CAMERA_SUCCESS == result) { result = CAMERA_EFAILED; }
        }
    }

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUEDEQUEUE, 0, 0);
    return result;
}

CAM_API CameraResult CameraQueueDropHead(CameraQueue hQueue)
{
    CameraResult result;
    cameraQueueCtx_t *pCameraQueue = (cameraQueueCtx_t*) hQueue;

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUEDROPHEAD, 0, 0);

    if (!pCameraQueue)
    {
        result = CAMERA_EBADPARM;
    }
    else if (CAMERAQUEUE_MAGIC != pCameraQueue->magic)
    {
        CAM_MSG(ERROR, "invalid or destroyed queue handle");
        result = CAMERA_EBADHANDLE;
    }
    else if (0 != pCameraQueue->pFcnLock(&pCameraQueue->sLock))
    {
        CAM_MSG(ERROR, "failed to lock hQueue=0x%p", hQueue);
        result = CAMERA_EFAILED;
    }
    else
    {
        if (cameraQueueIsEmpty(pCameraQueue))
        {
            /* caller attempted to dequeue but queue is empty - this is a normal use-case */
            result = CAMERA_ENOMORE;
        }
        else
        {
            /* update read index */
            pCameraQueue->rdIndex = cameraQueueNextIndex(pCameraQueue, pCameraQueue->rdIndex);
            result = CAMERA_SUCCESS;
        }

        /* unlock queue context */
        if (0 != pCameraQueue->pFcnUnLock(&pCameraQueue->sLock))
        {
            CAM_MSG(ERROR, "failed to unlock queue=0x%p", hQueue);
            if (CAMERA_SUCCESS == result) { result = CAMERA_EFAILED; }
        }
    }

    CameraLogEvent(CAMERA_CORE_EVENT_CAMERAQUEUEDROPHEAD, 0, 0);
    return result;
}

CAM_API CameraResult CameraQueueGetCapacity(CameraQueue hQueue, uint32* pnCapacityOut)
{
    CameraResult      result;
    cameraQueueCtx_t *pCameraQueue = (cameraQueueCtx_t*) hQueue;

    if (!pCameraQueue || !pnCapacityOut)
    {
        result = CAMERA_EBADPARM;
    }
    else if (CAMERAQUEUE_MAGIC != pCameraQueue->magic)
    {
        CAM_MSG(ERROR, "invalid or destroyed queue handle");
        result = CAMERA_EBADHANDLE;
    }
    else
    {
        /* no need to lock, because array size is fixed upon creation */
        result = cameraQueueGetCounter(pCameraQueue, CameraQueueCounterSize, pnCapacityOut);
    }

    return result;
}


CAM_API CameraResult CameraQueueGetAvailableEntries(CameraQueue hQueue,
    uint32* pnAvailableEntriesOut)
{
    return cameraQueueProtectedGetCounter(hQueue, CameraQueueCounterFree, pnAvailableEntriesOut);
}


CAM_API CameraResult CameraQueueGetLength(CameraQueue hQueue, uint32* pnLength)
{
    return cameraQueueProtectedGetCounter(hQueue, CameraQueueCounterUsed, pnLength);
}


CAM_API CameraResult CameraQueueIsEmpty(CameraQueue hQueue, boolean* pbIsEmptyOut)
{
    CameraResult      result;
    cameraQueueCtx_t *pCameraQueue = (cameraQueueCtx_t*) hQueue;

    if (!pCameraQueue || !pbIsEmptyOut)
    {
        result = CAMERA_EBADPARM;
    }
    else if (CAMERAQUEUE_MAGIC != pCameraQueue->magic)
    {
        CAM_MSG(ERROR, "invalid or destroyed queue handle");
        result = CAMERA_EBADHANDLE;
    }
    else if (0 != pCameraQueue->pFcnLock(&pCameraQueue->sLock))
    {
        CAM_MSG(ERROR, "failed to lock hQueue=0x%p", hQueue);
        result = CAMERA_EFAILED;
    }
    else
    {
        *pbIsEmptyOut = (cameraQueueIsEmpty(pCameraQueue) == 1) ? TRUE : FALSE;
        result        = CAMERA_SUCCESS;

        if (0 != pCameraQueue->pFcnUnLock(&pCameraQueue->sLock))
        {
            CAM_MSG(ERROR, "failed to unlock queue=0x%p", hQueue);
            result = CAMERA_EFAILED;
        }

    }

    return result;
}


CAM_API CameraResult CameraQueueIsFull(CameraQueue hQueue, boolean* pbIsFullOut)
{
    CameraResult      result;
    cameraQueueCtx_t* pCameraQueue = (cameraQueueCtx_t*) hQueue;

    if (!pCameraQueue || !pbIsFullOut)
    {
        result = CAMERA_EBADPARM;
    }
    else if (CAMERAQUEUE_MAGIC != pCameraQueue->magic)
    {
        CAM_MSG(ERROR, "invalid or destroyed queue handle");
        result = CAMERA_EBADHANDLE;
    }
    else if (0 != pCameraQueue->pFcnLock(&pCameraQueue->sLock))
    {
        CAM_MSG(ERROR, "failed to lock hQueue=0x%p", hQueue);
        result = CAMERA_EFAILED;
    }
    else
    {
        *pbIsFullOut = (cameraQueueIsFull(pCameraQueue) == 1) ? TRUE : FALSE;
        result       = CAMERA_SUCCESS;

        if (0 != pCameraQueue->pFcnUnLock(&pCameraQueue->sLock))
        {
            CAM_MSG(ERROR, "failed to unlock queue=0x%p", hQueue);
            result = CAMERA_EFAILED;
        }

    }

    return result;
}


/**
 * Removes an entry from the Queue
 *
 * @param[in] pQueue Queue handle.
 * @param[in] dataIn Queue item to remove from the queue
 * @return CAMERA_SUCCESS - if successful
 *         CAMERA_EBADPARM - invalid parameter is passed in
 */

CAM_API CameraResult CameraQueueRemoveEntry(CameraQueue hQueue,
    const CameraQueueDataType dataIn)
{
    (void)hQueue;
    (void)dataIn;
    return CAMERA_EUNSUPPORTED;
}


#ifdef __cplusplus
}
#endif

