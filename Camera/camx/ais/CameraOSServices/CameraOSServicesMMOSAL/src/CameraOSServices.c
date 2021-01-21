/**
 * @file CameraOSServices.c
 *
 * @brief Implementation of OS abstraction functions
 *
 * Copyright (c) 2009-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <errno.h>

#ifdef __QNXNTO__
#include <atomic.h>
#elif defined (__INTEGRITY)
#include <alltypes.h>
#endif

#include "CameraOSServices.h"

#include "AEEstd.h"
#include "AEEVaList.h"

#include "MMTimer.h"
#include "MMCriticalSection.h"
#include "MMThread.h"
#include "MMMalloc.h"

// Priority values are based on those defined by mmosal
#if defined(__QNXNTO__)
static int sPriorityMap[CAMERA_THREAD_PRIO_MAX] =
{
    10, 10, 10, 24, 30
};
#elif defined (__INTEGRITY)
static int sPriorityMap[CAMERA_THREAD_PRIO_MAX] =
{
    135, 140, 180, 201, 221
};
#else
static int sPriorityMap[CAMERA_THREAD_PRIO_MAX] =
{
    0, 0, 10, 50, 75
};
#endif

/* ===========================================================================
**                 DECLARATIONS AND DEFINITIONS FOR MODULE
** ======================================================================== */

/* --------------------------------------------------------------------------
** Constant / Define Declarations
** ----------------------------------------------------------------------- */

#ifndef CEIL64
#define CEIL64(a) (((a+63) / 64)* 64)
#endif /*CEIL64*/

/* --------------------------------------------------------------------------
** Type Declarations
** ----------------------------------------------------------------------- */
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
    int signal_count;
    int manualReset;
} os_event_t;

/* --------------------------------------------------------------------------
** Global Object Definitions
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Local Object Definitions
** ----------------------------------------------------------------------- */


/* --------------------------------------------------------------------------
** Forward Declarations
** ----------------------------------------------------------------------- */

/* ===========================================================================
**                      MACRO DEFINITIONS
** ======================================================================== */

#define OS_GET_ABSTIME(timeout, a_milliSeconds)           \
    clock_gettime(CLOCK_MONOTONIC, &timeout);             \
    timeout.tv_sec += (a_milliSeconds / 1000);            \
    timeout.tv_nsec += (a_milliSeconds % 1000) * 1000000; \
    if (timeout.tv_nsec >= 1000000000) {                  \
        timeout.tv_nsec -= 1000000000;                    \
        timeout.tv_sec++;                                 \
    }


/* ===========================================================================
**                      PUBLIC FUNCTION DEFINITIONS
** ======================================================================== */


boolean CameraGetTime(uint64* pnCurrentTime)
{
    boolean bResult = FALSE;
    unsigned long long currentTime;

    if (0 == MM_Time_GetTimeEx(&currentTime))
    {
        // convert milliseconds returned from MM_Time_GetTime to nanoseconds
        *pnCurrentTime = (int64)((currentTime)*1000000);
        bResult = TRUE;
    }

    return bResult;
}

void CameraGetSystemTime(uint64* pnCurrentTime)
{
    unsigned long long currentTime;
    // convert ms to ns
    MM_Time_GetCurrentTimeInMilliSecsFromEpoch(&currentTime);
    *pnCurrentTime = currentTime * 1000 * 1000;
}


void CameraDebugBreak(void)
{
}

void CameraBusyWait(uint32 nNanosecondsToWait)
{
    int status;
    if (0 == nNanosecondsToWait)
    {
        /* this call always returns success according to documentation */
        (void)sched_yield();
    }
#ifdef __QNXNTO__
    else if (nNanosecondsToWait < 2000000U) /* less than 2ms */
    {
        if (EOK != (status = nanospin_ns(nNanosecondsToWait)))
        {
            CAM_MSG(ERROR, "nanospin_ns: %s", strerror(status));
        }
    }
#endif
    else
    {
        uint64 usWait = (nNanosecondsToWait+999LLU)/1000LLU;

        if (usWait < 1000000LLU) /* less than 1 second */
        {
            /* Too long - won't be doing busy spin */
            if (-1 == usleep((useconds_t)usWait))
            {
                CAM_MSG(ERROR, "usleep: %s", strerror(errno));
            }
        }
        else /* 1 or more seconds */
        {
            /* Too long - won't be doing busy spin */
            struct timespec remain, req;
            remain.tv_sec = nNanosecondsToWait / 1000000000U; /* 4 or less */
            remain.tv_nsec = nNanosecondsToWait - remain.tv_sec * 1000000000U;
            do
            {
                req.tv_sec  = remain.tv_sec;
                req.tv_nsec = remain.tv_nsec;
#ifndef __INTEGRITY
                status = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &remain);
#else
                status = nanosleep(&req, &remain);
#endif
            } while ( EINTR == status );

            if (EOK != status)
            {
                CAM_MSG(ERROR, "clock_nanosleep: %s", strerror(status));
            }
        }
    }
}

void CameraSleep(uint32 nMillisecondsToSleep)
{
    (void) MM_Timer_Sleep(nMillisecondsToSleep);
}

CameraResult CameraCreateMutex(CameraMutex* pMutex)
{
    if (pMutex == NULL)
    {
        return CAMERA_EMEMPTR;
    }

    if (0 != MM_CriticalSection_Create(pMutex))
        return CAMERA_EFAILED;

    return CAMERA_SUCCESS;
}

void CameraLockMutex(CameraMutex Mutex)
{
    (void) MM_CriticalSection_Enter(Mutex);
}

void CameraUnlockMutex(CameraMutex Mutex)
{
    (void) MM_CriticalSection_Leave(Mutex);
}

void CameraDestroyMutex(CameraMutex Mutex)
{
    (void) MM_CriticalSection_Release(Mutex);
}

CameraResult CameraCreateSpinLock(CameraSpinLock* pSpinLock)
{
    CameraResult result = CAMERA_EMEMPTR;

    if (pSpinLock)
    {
        result = CameraCreateMutex((CameraMutex*)pSpinLock);
    }

    return result;
}

void CameraAcquireSpinLock(CameraSpinLock spinLock)
{
    CameraLockMutex((CameraMutex)spinLock);
}

void CameraReleaseSpinLock(CameraSpinLock spinLock)
{
    CameraUnlockMutex((CameraMutex)spinLock);
}

void CameraDestroySpinLock(CameraSpinLock* pSpinLock)
{
    if (pSpinLock)
    {
        CameraDestroyMutex((CameraMutex)(*pSpinLock));
        *pSpinLock = NULL;
    }
}

CameraResult CameraCreateSignal(CameraSignal* ppSignal)
{
    pthread_condattr_t attr;

    if (ppSignal == NULL)
    {
        return CAMERA_EFAILED;
    }

    pthread_condattr_init(&attr);
#ifndef __INTEGRITY
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif

    os_event_t *ev = (os_event_t*) CameraAllocate(
            CAMERA_ALLOCATE_ID_OS_SIGNAL,
            sizeof(os_event_t));
    if(NULL == ev)
    {
        pthread_condattr_destroy(&attr);
        return CAMERA_ENOMEMORY;
    }
    if (pthread_mutex_init(&ev->mutex, 0))
    {
        CameraFree(CAMERA_ALLOCATE_ID_OS_SIGNAL, ev);
        pthread_condattr_destroy(&attr);
        return CAMERA_EFAILED;
    }
    if (pthread_cond_init(&ev->cond_var, &attr))
    {
        pthread_mutex_destroy(&ev->mutex);
        CameraFree(CAMERA_ALLOCATE_ID_OS_SIGNAL, ev);
        pthread_condattr_destroy(&attr);
        return CAMERA_EFAILED;
    }
    ev->signal_count = 0;
    ev->manualReset = FALSE;

    *ppSignal = ev;

    return CAMERA_SUCCESS;
}

CameraResult CameraResetSignal(CameraSignal pSignal)
{
    os_event_t *ev;

    if (!pSignal)
    {
        return CAMERA_EMEMPTR;
    }

    ev = (os_event_t*)pSignal;

    if (pthread_mutex_lock(&ev->mutex))
    {
        return CAMERA_EFAILED;
    }

    ev->signal_count = 0;

    if (pthread_mutex_unlock(&ev->mutex))
    {
        return CAMERA_EFAILED;
    }

    return CAMERA_SUCCESS;
}

CameraResult CameraWaitOnSignal(CameraSignal pSignal, uint32 nTimeoutMilliseconds)
{
    CameraResult ret = CAMERA_SUCCESS;
    int result = 0;
    os_event_t *ev;

    if (!pSignal)
    {
        return CAMERA_EMEMPTR;
    }

    ev = (os_event_t*)pSignal;

    pthread_mutex_lock(&ev->mutex);

    if(nTimeoutMilliseconds != CAM_SIGNAL_WAIT_NO_TIMEOUT)
    {
        struct timespec timeout;

        memset(&timeout, 0, sizeof(struct timespec));
        OS_GET_ABSTIME(timeout, nTimeoutMilliseconds);

        /* we break on being signaled or timeout and errors */
        while (ev->signal_count == 0 && !result)
        {
            result = pthread_cond_timedwait(&ev->cond_var, &ev->mutex, &timeout);
        }

        /*check for errors*/
        if(result && result != ETIMEDOUT)
        {
            pthread_mutex_unlock(&ev->mutex);
            return CAMERA_EFAILED;
        }
        ret = (!result) ? CAMERA_SUCCESS : CAMERA_EEXPIRED;
    }
    else
    {
        while(ev->signal_count == 0)
        {
            ret = pthread_cond_wait(&ev->cond_var, &ev->mutex);
        }
    }

    if(!ev->manualReset && ret != CAMERA_EEXPIRED)
    {
        ev->signal_count--;
    }
    pthread_mutex_unlock(&ev->mutex);

    return ret;
}

CameraResult CameraSetSignal(CameraSignal pSignal)
{
    os_event_t *ev;

    if (!pSignal)
    {
        return CAMERA_EMEMPTR;
    }

    ev = (os_event_t*)pSignal;

    if (pthread_mutex_lock(&ev->mutex))
    {
        return CAMERA_EFAILED;
    }

    ev->signal_count++;
    pthread_cond_signal(&ev->cond_var);

    if (pthread_mutex_unlock(&ev->mutex))
    {
        return CAMERA_EFAILED;
    }

    return CAMERA_SUCCESS;
}

void CameraDestroySignal(CameraSignal pSignal)
{
    if (pSignal)
    {
        os_event_t *ev = (os_event_t*)pSignal;

        pthread_cond_destroy(&ev->cond_var);
        pthread_mutex_destroy(&ev->mutex);
        CameraFree(CAMERA_ALLOCATE_ID_OS_SIGNAL, ev);
        pSignal = NULL;
    }
}

CameraResult CameraOSServicesInitialize(void* pParam1)
{
    return CAMERA_SUCCESS;
}

CameraResult CameraOSServicesUninitialize(void)
{
    return CAMERA_SUCCESS;
}

uint32 CameraAtomicIncrement(uint32* pnAddend)
{
    /* signal/intr handlers safe */
#ifdef __QNXNTO__
    return ( atomic_add_value((volatile unsigned*)pnAddend, 1) + 1 );
#elif defined (__INTEGRITY)
    Address old_val;
    return AtomicModify((Address*)pnAddend, &old_val, 0, 1);
#else
    return __sync_add_and_fetch((volatile unsigned*)pnAddend, 1);
#endif
}

uint32 CameraAtomicDecrement(uint32* pnAddend)
{
    /* signal/intr handlers safe */
#ifdef __QNXNTO__
    return ( atomic_sub_value((volatile unsigned*)pnAddend, 1) - 1 );
#elif defined (__INTEGRITY)
    Address old_val;
    return AtomicModify((Address*)pnAddend, &old_val, 0, (Address)-1);
#else
    return __sync_sub_and_fetch((volatile unsigned*)pnAddend, 1);
#endif
}

void* CameraAllocate(CameraAllocateID allocateID, uint32 numBytes)
{
    (void) allocateID;
    return (void*) MM_Malloc(numBytes);
}

void CameraFree(CameraAllocateID allocateID, void* pBuffer)
{
    (void) allocateID;
    if (pBuffer != NULL)
    {
        (void) MM_Free(pBuffer);
        pBuffer = NULL;
    }
}

/**
 * This function creates a thread.
 */
CameraResult CameraCreateThread(CameraThreadPriorityType nPriority,
    int nSuspend,
    int(*pfnStart)(void*),
    void* pvStartArg,
    unsigned int nStackSize,
    char* pstrName,
    void** pHandle)
{
    if (nPriority >= CAMERA_THREAD_PRIO_MAX)
    {
        nPriority = CAMERA_THREAD_PRIO_MAX - 1;
    }

    if (nStackSize < CAM_THREAD_STACK_MIN)
    {
        nStackSize = CAM_THREAD_STACK_MIN;
    }

    return MM_Thread_CreateEx(sPriorityMap[nPriority], nSuspend, pfnStart, pvStartArg, nStackSize, pstrName, pHandle) ?
            CAMERA_EFAILED : CAMERA_SUCCESS;
}

/**
 * This function releases a thread.
 */
CameraResult CameraReleaseThread(void* handle)
{
    return MM_Thread_Release(handle) ? CAMERA_EFAILED : CAMERA_SUCCESS;
}

/**
 * This function terminates a thread
 */
CameraResult CameraExitThread(void* handle, int nExitCode)
{
    return MM_Thread_Exit(handle, nExitCode) ? CAMERA_EFAILED : CAMERA_SUCCESS;
}

/**
 * This function joins a thread
 */
CameraResult CameraJoinThread(void* handle, void* pExitCode)
{
    return MM_Thread_Join(handle, pExitCode) ? CAMERA_EFAILED : CAMERA_SUCCESS;
}

/**
 * This function detaches a thread.
 */
CameraResult CameraDetachThread(void* handle)
{
#if defined(__QNXNTO__) || defined(__INTEGRITY)
    return MM_Thread_Detach(handle) ? CAMERA_EFAILED : CAMERA_SUCCESS;
#else
    //TODO: implement once supported on linux
    return CAMERA_EFAILED;
#endif
}

/**
 * This function sets priority to a thread
 */
CameraResult CameraSetThreadPriority(void* handle, CameraThreadPriorityType nPriority)
{
    if (nPriority >= CAMERA_THREAD_PRIO_MAX)
    {
        nPriority = CAMERA_THREAD_PRIO_MAX - 1;
    }

    return MM_Thread_SetPriority(handle, sPriorityMap[nPriority]) ? CAMERA_EFAILED : CAMERA_SUCCESS;
}

/**
 * This function returns priority defined by CameraOSServices
 */
unsigned int CameraTranslateThreadPriority(CameraThreadPriorityType nPriority)
{
    if (nPriority >= CAMERA_THREAD_PRIO_MAX)
    {
        nPriority = CAMERA_THREAD_PRIO_MAX - 1;
    }

    return sPriorityMap[nPriority];
}

boolean CameraBuildAbsPathName(char *pOutputBuffer, uint32 bufferSize, const char *pFileName)
{
    static const char f_basePath[] = "";
    const char *pc, *pStart;

    pStart = pOutputBuffer;

    if (pOutputBuffer && bufferSize && pFileName)
    {
        /* copy base path */
        pc = f_basePath;
        while (bufferSize && ('\0' != *pc))
        {
            --bufferSize;
            *pOutputBuffer++ = *pc++;
        }

        /* copy file name */
        while (bufferSize && ('\0' != *pFileName))
        {
            --bufferSize;
            *pOutputBuffer++ = *pFileName++;
        }

        /* NULL terminate the string */
        if ((0 == bufferSize) && (pOutputBuffer > pStart))
        {
            --pOutputBuffer;
        }
        *pOutputBuffer = '\0';

        if (bufferSize)
        {
            return TRUE;
        }
    }
    return FALSE;
}

CameraResult CameraOpenFile(CameraOSFileHandle_t *handle, const char *pFileName)
{
    CameraResult result;

    result = CAMERA_EBADPARM;

    if (handle && pFileName)
    {
        handle->pFileHandle = fopen(pFileName, "rb");
        handle->nFileHandle = 0; /* not used */
        handle->fileSize    = 0; /* not used */

        if (handle->pFileHandle)
        {
            result = CAMERA_SUCCESS;
        }
        else
        {
            CAM_MSG(HIGH, "%s: %s, %s", __FUNCTION__, pFileName, strerror(errno));
            result = CAMERA_EFAILED;
        }
    }
    return result;
}

CameraResult CameraCloseFile(CameraOSFileHandle_t *handle)
{
    CameraResult result;

    result = CAMERA_EBADPARM;
    if (handle)
    {
        if (handle->pFileHandle)
        {
            fclose(handle->pFileHandle);
            handle->pFileHandle = 0;
        }
        result = CAMERA_SUCCESS;
    }
    return result;
}

CameraResult CameraReadFileSegment(CameraOSFileHandle_t *handle,
                                   unsigned char        *pBuffer,
                                   unsigned int         *pBufferSize)
{
    CameraResult result;

    result = CAMERA_EBADPARM;

    if (handle && pBuffer && pBufferSize)
    {
        if (*pBufferSize)
        {
            if (handle->pFileHandle)
            {
                /* note: may not read enough bytes if EOF */
                *pBufferSize = fread(pBuffer, 1, *pBufferSize, handle->pFileHandle);
                if (*pBufferSize)
                {
                    result = CAMERA_SUCCESS;
                }
                else
                {
                    /* assume EOF */
                    result = CAMERA_ENOMORE;
                }
            }
            else
            {
                /* file handle not available */
                result = CAMERA_EBADSTATE;
            }
        }
    }
    return result;
}

CameraResult CameraReadFile(char * pFileName, void ** ppBuff, int nMaxSize)
{
    CameraResult         result;
    CameraOSFileHandle_t handle;
    unsigned int         readSize;

    /* open file handle */
    result = CameraOpenFile(&handle, pFileName);
    if (CAMERA_SUCCESS == result)
    {
        /* attempt to read bytes */
        readSize = nMaxSize;
        result   = CameraReadFileSegment(&handle, *((unsigned char **)ppBuff), &readSize);

        /* close file handle */
        (void)CameraCloseFile(&handle);
    }
    return result;
}

