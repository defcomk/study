#ifndef _AIS_ENGINE_H_
#define _AIS_ENGINE_H_

/*!
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "ais.h"
#include "ais_i.h"
#include "CameraDeviceManager.h"
#include "ais_configurer.h"

#define MAX_USR_CONTEXTS 32
#define NUM_EVENT_HNDLR_POOL 2

#define EVENT_QUEUE_MAX_SIZE 32

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


/**
 * AIS csi information required for csi configuration
 */
typedef struct
{
    uint32 num_lanes;   /**< number of lanes for input */
    uint32 lane_mask;   /**< lane assignment for input */
    uint32 settle_count; /**< settle count for input */
}ais_input_csi_params_t;

/**
 * AIS input mode information that is required by csi/ife
 */
typedef struct
{
    uint32 width;   /**< width of input mode */
    uint32 height;  /**< height of input mode */
    uint32 fps;     /**< fps of input mode */
    uint32 vc;      /**< vc of input mode */
    uint32 dt;      /**< dt of input mode */
    qcarcam_color_fmt_t fmt;  /**< color fmt of input mode*/
    boolean interlaced;       /**< interlaced or progressive of input mode*/
}ais_input_mode_info_t;

/** HW pipeline resources */
typedef struct
{
    uint32 cid;     /**< cid on CSI */
    uint32 csiphy;  /**< CSI phy core */
    uint32 csid;    /**< CSID core */

    ais_ife_user_path_info ife_user_path_info[AIS_USER_PATHS_MAX];

    uint32 n_user_paths;
}ais_userctx_resources_t;

/** User context private settings */
typedef struct
{
    uint32 bitmask;                                          /**< bitmask of user set parameters */

    uint32 width;                                            /**< width of output*/
    uint32 height;                                           /**< height of output*/
    qcarcam_color_fmt_t fmt;                                 /**< output color format */
    qcarcam_exposure_config_t exposure_params;               /**< exposure */
    qcarcam_frame_rate_t frame_rate_config;                  /**< frame rate */
    float saturation_param;                                  /**< saturation param */
    float hue_param;                                         /**< hue param */

    void* pPrivData; /**< QCARCAM_PARAM_PRIVATE_DATA */

    uint32 n_latency_max; /**< max latency in number of buffers; this is the max number of buffers in buffer_done_q
                                after which we will proceed to remove stale ones */
    uint32 n_latency_reduce_rate; /**< the number of stale buffers we remove once n_latency_max is exceeded */
}ais_userctx_settings_t;

typedef struct
{
    uint32 devId;   /**< input device id */
    uint32 srcId;   /**< input source id */

    ais_input_csi_params_t csiInfo;
    ais_input_mode_info_t  inputModeInfo;
}ais_userctx_input_cfg_t;

/**
 * field related info
 */
typedef struct
{
    boolean  even_field; /**< fixed to the 1st member */
    uint64_t  field_ts;   /**< fixed to the 2nd member */

    boolean  valid; /**< specifies if this is obsolete */

    uint32   target_frame_id;  /**< specifies which frame it belongs to */
    uint64_t  sof_ts;           /**< specifies the sof of current field timestamp: ns */
}ais_field_info_t;

/**
 * diagnostic related info
 */
typedef struct
{
    unsigned long long last_frame_time; /**< the last frame timestamp when VFE IRQ happen */
    unsigned long long fps_start_time;  /**< when statistic fps, the start timestamp */
    uint32 nframes;                     /**< the frame number since the start time */
}ais_usrctx_diag_info_t;


class AisEngine;

/**
 * AIS User Context
 */
class AisUsrCtxt
{
private:
    AisUsrCtxt() {
        m_magic = 0;
        m_mapIdx = 0;
        m_qcarcamHndl = 0;
        m_inputId = QCARCAM_INPUT_MAX;

        m_mutex = NULL;
        m_state = AIS_USR_STATE_UNINITIALIZED;

        m_isPendingStart = FALSE;

        memset(&m_resources, 0x0, sizeof(m_resources));
        memset(&m_inputCfg, 0x0, sizeof(m_inputCfg));
        memset(&m_usrSettings, 0x0, sizeof(m_usrSettings));
        memset(m_fieldInfo, 0x0, sizeof(m_fieldInfo));

        m_bufferDoneQ = NULL;
        m_eventCbFcn = NULL;
        m_eventMask = 0;

        m_startTime = 0;
    }


public:
    static AisUsrCtxt* CreateUsrCtxt(qcarcam_input_desc_t input_id);
    static void DestroyUsrCtxt(AisUsrCtxt* pUsrCtxt);

    CameraResult Reserve(void);
    CameraResult Release(void);
    CameraResult Start(void);
    CameraResult Stop(void);
    CameraResult Pause(void);
    CameraResult Resume(void);

    CameraResult InitMutex(void);
    CameraResult DeinitMutex(void);

    uint32 m_magic; /**< canary initialized to USR_CTXT_MAGIC to indicate valid ctxt */

    unsigned int m_mapIdx; /**< index into mapping table */

    qcarcam_hndl_t m_qcarcamHndl;    /**< opened qcarcam handle */
    qcarcam_input_desc_t m_inputId;  /**< input desc associated with handle */

    /** states */
    CameraMutex m_mutex;
    ais_usr_state_t m_state; /**< state of user context */

    boolean m_isPendingStart; /**< tracks start state until we receive first frame */

    ais_userctx_resources_t m_resources; /**< resources that are reserved for user */
    ais_userctx_input_cfg_t m_inputCfg; /**< input configuration */

    ais_userctx_settings_t m_usrSettings; /**< user settings */
    AisBufferList m_bufferList[AIS_BUFFER_LIST_MAX];

    ais_field_info_t m_fieldInfo[AIS_FIELD_BUFFER_SIZE];

    pthread_cond_t  m_bufferDoneQCond; /**< conditional variable for buffer done Q
                                     signaled by ais_resmgr_bufdone and waited by ais_get_frame */
    pthread_mutex_t m_bufferDoneQMutex;    /**< mutex to protect buffer done Q */
    CameraQueue m_bufferDoneQ;  /**< buffer done queue */
    /** event management */
    qcarcam_event_cb_t m_eventCbFcn; /**< event callback function */
    uint32 m_eventMask; /**< event mask */

    unsigned long long m_startTime;   /**current start timestamp*/

#if 0
    ais_usrctx_diag_info_t diag;
    void*   p_diaginfo;
#endif
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
    AIS_ENGINE_QUEUE_EVENTS,
    AIS_ENGINE_QUEUE_ERRORS,
    AIS_ENGINE_QUEUE_MAX,
}ais_engine_queue_t;



/**
 * AIS engine event cb function
 */
typedef CameraResult (*ais_engine_queue_event_t) (void* m_pAisEngine, ais_engine_event_msg_t* event_msg);

typedef enum
{
    AIS_CONFIGURER_INPUT = 0,
    AIS_CONFIGURER_CSI,
    AIS_CONFIGURER_IFE,
    AIS_CONFIGURER_MAX
}ais_configurer_type_t;


#define AIS_CTXT_MAGIC 0xA150C0DE

typedef boolean (*ais_usr_ctxt_match_func)(AisUsrCtxt* usrCtxt, void* match);
typedef boolean (*ais_usr_ctxt_operate_func)(AisUsrCtxt* usrCtxt, void* user_data);

class AisEngineConfigurer;
class AisResourceManager;

/**
 * AIS overall context
 */
class AisEngine : public AisEngineBase
{
private:
    AisEngine();
    ~AisEngine();
    static AisEngine* m_pEngineInstance;        // Singleton instance of AisEngine
public:
    static AisEngine* CreateInstance();
    static AisEngine* GetInstance();
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

private:
    CameraResult Initialize(void);
    void Deinitialize(void);


    int ProcessEvent(ais_engine_event_msg_t* msg);
    static int EventHandler(void *arg);

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

    uintptr_t AssignNewHndl(AisUsrCtxt* usr_ctxt);
    void ReleaseHndl(void* user_hndl);
    void PutUsrCtxt(AisUsrCtxt* usr_ctxt);
    AisUsrCtxt* AcquireUsrCtxt(unsigned int idx);
    AisUsrCtxt* GetUsrCtxt(void* user_hndl);
    void RelinquishUsrCtxt(unsigned int idx);

    AisUsrCtxt* FindUsrCtxt(ais_usr_ctxt_match_func match_func, void* match);
    uint32 TraverseUsrCtxt(ais_usr_ctxt_match_func match_func, void* match,
            ais_usr_ctxt_operate_func op_func, void* user_data);

    CameraResult UsrCtxtEventFrameDone(AisUsrCtxt* usr_ctxt, ais_frame_done_t* ais_frame_done);

    uint32 m_magic; /**< canary initialized to AIS_CTXT_MAGIC to indicate valid ctxt */

    boolean m_isInitialized;

    uint32  m_clientCount;

    CameraMutex m_mutex;

    /*event handlers*/
    CameraSignal m_eventHandlerSignal;
    void        *m_eventHandlerThreadId[NUM_EVENT_HNDLR_POOL];

    volatile boolean m_eventHandlerIsExit;
    CameraQueue m_eventQ[AIS_ENGINE_QUEUE_MAX];
    CameraMutex m_usrCtxtMapMutex;
    ais_usr_ctxt_map_t m_usrCtxtMap[MAX_USR_CONTEXTS];

    CameraDeviceManager* mDeviceManagerContext;

    AisResourceManager*   m_ResourceManager;

public:
    AisEngineConfigurer* mConfigurers[AIS_CONFIGURER_MAX];

private:

};

#endif /* _AIS_ENGINE_H_ */
