#ifndef _CHI_ENGINE_H_
#define _CHI_ENGINE_H_

/*!
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "ais.h"
#include "ais_chi_i.h"
#include "chimodule.h"
#include "realtime_pipeline_test.h"

#define MAX_USR_CONTEXTS 32
#define NUM_EVENT_HNDLR_POOL 2

#define EVENT_QUEUE_MAX_SIZE 32

#define CHI_MAX_NUM_CAM 4

/**
 * Convert errno to CameraResult
 */
static inline int ERRNO_TO_RESULT(const int err)
{
    int result = CAMERA_EFAILED;
    switch (err)
    {
        case EOK:
            result = CAMERA_SUCCESS;
            break;
        case EPERM:
            result = CAMERA_ENOTOWNER;
            break;
        case ENOENT:
            result = CAMERA_EINVALIDITEM;
            break;
        case ESRCH:
            result = CAMERA_ERESOURCENOTFOUND;
            break;
        case EINTR:
            result = CAMERA_EINTERRUPTED;
            break;
        case EIO:
            result = CAMERA_EFAILED;
            break;
        case ENXIO:
            result = CAMERA_ENOSUCH;
            break;
        case E2BIG:
            result = CAMERA_EOUTOFBOUND;
            break;
        case ENOEXEC:
            result = CAMERA_EINVALIDFORMAT;
            break;
        case EBADF:
            result = CAMERA_EINVALIDITEM;
            break;
        case ECHILD:
            result = CAMERA_EBADTASK;
            break;
        case EAGAIN:
            result = CAMERA_EWOULDBLOCK;
            break;
        case ENOMEM:
            result = CAMERA_ENOMEMORY;
            break;
        case EACCES:
            result = CAMERA_EPRIVLEVEL;
            break;
        case EFAULT:
            result = CAMERA_EMEMPTR;
            break;
        case EBUSY:
            result = CAMERA_EITEMBUSY;
            break;
        case EEXIST:
            result = CAMERA_EALREADY;
            break;
        case ENODEV:
            result = CAMERA_ERESOURCENOTFOUND;
            break;
        case EINVAL:
            result = CAMERA_EBADPARM;
            break;
        case ENFILE:
            result = CAMERA_EOUTOFBOUND;
            break;
        case EMFILE:
            result = CAMERA_EOUTOFHANDLES;
            break;
        case ETXTBSY:
            result = CAMERA_EITEMBUSY;
            break;
        case EFBIG:
        case ENOSPC:
            result = CAMERA_EOUTOFBOUND;
            break;
        case EROFS:
            result = CAMERA_EREADONLY;
            break;
        case EMLINK:
            result = CAMERA_EOUTOFHANDLES;
            break;
        case ERANGE:
            result = CAMERA_EOUTOFBOUND;
            break;
        case ENOTSUP:
            result = CAMERA_EUNSUPPORTED;
            break;
        case ETIMEDOUT:
            result = CAMERA_EEXPIRED;
            break;
    }

    return result;
}

/**
 * AIS user context states
 */
typedef enum
{
    AIS_USR_STATE_UNINITIALIZED = 0,
    AIS_USR_STATE_OPENED,
    AIS_USR_STATE_RESERVED,
    AIS_USR_STATE_STREAMING,
    AIS_USR_STATE_PAUSED,
    AIS_USR_STATE_ERROR,
    AIS_USR_STATE_MAX
}ais_usr_state_t;

class ChiEngine;

/**
 * AIS User Context
 */
class ChiUsrCtxt
{
private:
    ChiUsrCtxt() = default;


public:
    static ChiUsrCtxt* CreateUsrCtxt(qcarcam_input_desc_t input_id);

    CameraResult Reserve(void);
    CameraResult Release(void);
    CameraResult Start(void);
    CameraResult Stop(void);
    CameraResult Pause(void);
    CameraResult Resume(void);

    CameraResult InitMutex(void);
    CameraResult DeinitMutex(void);

    static boolean MatchIfeFrameDone(ChiUsrCtxt* pUsrCtxt, void* match);

    uint32 m_magic; /**< canary initialized to USR_CTXT_MAGIC to indicate valid ctxt */

    unsigned int m_mapIdx; /**< index into mapping table */

    qcarcam_hndl_t m_qcarcamHndl;    /**< opened qcarcam handle */
    qcarcam_input_desc_t m_inputId;  /**< input desc associated with handle */

    /** states */
    CameraMutex m_mutex;
    ais_usr_state_t m_state; /**< state of user context */

    boolean m_isPendingStart; /**< tracks start state until we receive first frame */

    AisBufferList m_bufferList[AIS_BUFFER_LIST_MAX];

    pthread_cond_t  m_bufferDoneQCond; /**< conditional variable for buffer done Q
                                     signaled by ais_resmgr_bufdone and waited by ais_get_frame */
    pthread_mutex_t m_bufferDoneQMutex;    /**< mutex to protect buffer done Q */
    CameraQueue m_bufferDoneQ;  /**< buffer done queue */
    /** event management */
    qcarcam_event_cb_t m_eventCbFcn; /**< event callback function */
    uint32 m_eventMask; /**< event mask */

    void* m_usrPrivData; /**< QCARCAM_PARAM_PRIVATE_DATA */
    unsigned long long m_startTime;   /**current start timestamp*/

    RealtimePipelineTest* m_realtimePipe; /**CHI realtime pipeline*/

    uint32 m_pipelineId;
};


typedef enum
{
    AIS_USR_CTXT_MAP_UNUSED = 0,
    AIS_USR_CTXT_MAP_USED,
    AIS_USR_CTXT_MAP_DESTROY,
}ais_usr_ctxt_map_state_t;

/**
 * AIS context table mapping
 */
typedef struct
{
    volatile uint32 in_use; /**< flag to indicate in use */
    uintptr_t phndl; /**< qcarcam handle */
    unsigned int refcount; /**< refcounter to handle */
    void* usr_ctxt; /**< user context pointer */
}ais_usr_ctxt_map_t;

/**
 * AIS Event Queue Type
 */
typedef enum
{
    CHI_ENGINE_QUEUE_EVENTS,
    CHI_ENGINE_QUEUE_ERRORS,
    CHI_ENGINE_QUEUE_CHI_EVENTS,
    CHI_ENGINE_QUEUE_MAX,
}ais_engine_queue_t;



/**
 * AIS engine event cb function
 */
typedef CameraResult (*ais_engine_queue_event_t) (void* m_pChiEngine, ais_engine_event_msg_t* event_msg);

#define AIS_CTXT_MAGIC 0xA150C0DE

typedef boolean (*ais_usr_ctxt_match_func)(ChiUsrCtxt* usrCtxt, void* match);
typedef boolean (*ais_usr_ctxt_operate_func)(ChiUsrCtxt* usrCtxt, void* user_data);

typedef struct
{
    CHICAMERAINFO sCameraInfo;
    CHISENSORMODEINFO sSensorModeInfo;
}ChiCameraInfoType;

/**
 * AIS overall context
 */
class ChiEngine : public AisEngineBase
{
private:
    ChiEngine();
    ~ChiEngine();
    static ChiEngine* m_pEngineInstance;        // Singleton instance of ChiEngine
public:
    static ChiEngine* CreateInstance();
    static ChiEngine* GetInstance();
    static void       DestroyInstance();

    virtual CameraResult ais_initialize(qcarcam_init_t* p_init_params);
    virtual CameraResult ais_uninitialize(void);
    virtual CameraResult ais_query_inputs(qcarcam_input_t* p_inputs, unsigned int size, unsigned int* ret_size);
    virtual qcarcam_hndl_t ais_open(qcarcam_input_desc_t desc);
    virtual CameraResult ais_close(qcarcam_hndl_t hndl);
    virtual CameraResult ais_g_param(qcarcam_hndl_t hndl, qcarcam_param_t param, qcarcam_param_value_t* p_value);
    virtual CameraResult ais_s_param(qcarcam_hndl_t hndl, qcarcam_param_t param, const qcarcam_param_value_t* p_value);
    virtual CameraResult ais_s_buffers(qcarcam_hndl_t hndl, qcarcam_buffers_t* p_buffers);
    virtual CameraResult ais_start(qcarcam_hndl_t hndl);
    virtual CameraResult ais_stop(qcarcam_hndl_t hndl);
    virtual CameraResult ais_pause(qcarcam_hndl_t hndl);
    virtual CameraResult ais_resume(qcarcam_hndl_t hndl);
    virtual CameraResult ais_get_frame(qcarcam_hndl_t hndl, qcarcam_frame_info_t* p_frame_info,
                unsigned long long int timeout, unsigned int flags);
    virtual CameraResult ais_release_frame(qcarcam_hndl_t hndl, unsigned int idx);

    CameraResult QueueEvent(ais_engine_event_msg_t* msg);

    /* Checks the global context is initialized */
    boolean IsInitialized(void)
    {
        return m_isInitialized;
    }

private:
    CameraResult Initialize(void);
    void Deinitialize(void);

    int ProcessEvent(ais_engine_event_msg_t* msg);
    static int EventHandler(void *arg);

    CameraResult QueueChiEvent(ais_engine_event_msg_t* msg);
    int ProcessChiEvent(ais_engine_event_msg_t* msg);
    static int EventChiHandler(void *arg);

    CameraResult submitBufferIdx(ChiUsrCtxt* usr_ctxt, int idx);
    CameraResult submitBuffers(ChiUsrCtxt* usr_ctxt, int numBufs);
    CameraResult mapBuffers(ChiUsrCtxt* usr_ctxt, AisBufferList* p_buffer_list);

    CameraResult PowerSuspend(void);
    CameraResult PowerResume(void);

    void Lock()
    {
        CameraLockMutex(this->m_mutex);
    }

    void Unlock()
    {
        CameraUnlockMutex(this->m_mutex);
    }

    uintptr_t AssignNewHndl(ChiUsrCtxt* usr_ctxt);
    void ReleaseHndl(void* user_hndl);
    void PutUsrCtxt(ChiUsrCtxt* usr_ctxt);
    ChiUsrCtxt* AcquireUsrCtxt(unsigned int idx);
    ChiUsrCtxt* GetUsrCtxt(void* user_hndl);
    void RelinquishUsrCtxt(unsigned int idx);

    ChiUsrCtxt* FindUsrCtxt(ais_usr_ctxt_match_func match_func, void* match);
    uint32 TraverseUsrCtxt(ais_usr_ctxt_match_func match_func, void* match,
            ais_usr_ctxt_operate_func op_func, void* user_data);

    CameraResult UsrCtxtEventFrameDone(ChiUsrCtxt* usr_ctxt, ais_frame_done_t* ais_frame_done);

    uint32 m_magic; /**< canary initialized to AIS_CTXT_MAGIC to indicate valid ctxt */
    boolean m_isInitialized;

    uint32  m_clientCount;

    CameraMutex m_mutex;

    /*event handlers*/
    CameraSignal m_eventHandlerSignal;
    void        *m_eventHandlerThreadId[NUM_EVENT_HNDLR_POOL];

    CameraSignal m_eventChiHandlerSignal;
    void        *m_eventChiHandlerThreadId;

    volatile boolean m_eventHandlerIsExit;
    CameraQueue m_eventQ[CHI_ENGINE_QUEUE_MAX];
    CameraMutex m_usrCtxtMapMutex;
    ais_usr_ctxt_map_t m_usrCtxtMap[MAX_USR_CONTEXTS];


    ChiModule*       m_pChiModule;
    RealtimePipelineTest* m_realtimePipe[CHI_MAX_NUM_CAM]; /**CHI realtime pipeline*/

    ChiCameraInfoType* m_pChiCameras;
    int m_numCameras;
};

#endif /* _CHI_ENGINE_H_ */
