/*!
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "ais_engine.h"
#include "ais_res_mgr.h"
#include "ais_buffer_manager.h"
#include "ais_ife_configurer.h"
#include "ais_input_configurer.h"
#include "ais_csi_configurer.h"
#include "CameraDeviceManager.h"

#include "CameraPlatform.h"

//////////////////////////////////////////////////////////////////////////////////
/// MACRO DEFINES
//////////////////////////////////////////////////////////////////////////////////
#define AIS_API CAM_API

#define AIS_API_ENTER() CAM_MSG(MEDIUM, "%s: enter", __func__);
#define AIS_API_ENTER_HNDL(_ctxt_) CAM_MSG(MEDIUM, "%s: enter %p", __func__, _ctxt_);


#define CHECK_AIS_CTXT_MAGIC(_hndl_) (AIS_CTXT_MAGIC == _hndl_->magic)

#define USR_CTXT_MAGIC 0xA1500000
#define USR_CTXT_MAGIC_MASK 0xFFF00000
#define CHECK_USR_CTXT_MAGIC(_hndl_) (USR_CTXT_MAGIC == (_hndl_ & USR_CTXT_MAGIC_MASK))

#define USR_CTXT_IDX_MASK  0x00007E00
#define USR_CTXT_IDX_SHIFT 9

#define USR_CTXT_RAND_MASK 0x000F81FF

#define USR_IDX_FROM_HNDL(_hndl_) (((_hndl_ & USR_CTXT_IDX_MASK) >> USR_CTXT_IDX_SHIFT) - 1)

#define USR_NEW_HNDL(_idx_) \
(USR_CTXT_MAGIC | \
(rand() & USR_CTXT_RAND_MASK) | \
(USR_CTXT_IDX_MASK & ((_idx_ + 1) << USR_CTXT_IDX_SHIFT)))

#define NANOSEC_TO_SEC 1000000000L

#define USR_CTXT_SET_MASK(_bitmask_, _param_) ((_bitmask_) |= 1 << (_param_))
#define USR_CTXT_CHECK_MASK(_bitmask_, _param_) ((_bitmask_) & (1 << (_param_)))

#define FPS_FRAME_COUNT 330
//////////////////////////////////////////////////////////////////////////////////
/// FORWARD DECLARE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
/// GLOBAL VARIABLES
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
/// FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////


/**
 * ais_deinit
 *
 * @brief Deinitialize AIS context & frees resources
 *
 * @param ais_ctxt_t
 *
 * @return n/a
 */
void AisEngine::Deinitialize(void)
{
    int i;
    CameraResult rc = CAMERA_SUCCESS;

    m_eventHandlerIsExit = TRUE;

    for (i = 0; i < NUM_EVENT_HNDLR_POOL; i++)
    {
        CameraSetSignal(m_eventHandlerSignal);
    }

    for (i = 0; i < NUM_EVENT_HNDLR_POOL; i++)
    {
        if (m_eventHandlerThreadId[i])
        {
            CameraJoinThread(m_eventHandlerThreadId[i], NULL);
            CameraReleaseThread(m_eventHandlerThreadId[i]);
        }
    }

    CameraDestroySignal(m_eventHandlerSignal);
    m_eventHandlerSignal = NULL;

    for (i = 0; i < AIS_ENGINE_QUEUE_MAX; i++)
    {
        if (m_eventQ[i])
        {
            CameraQueueDestroy(m_eventQ[i]);
        }
    }

    rc = CameraPlatformClockEnable();
    if (CAMERA_SUCCESS == rc)
    {
        // Initialize all configurers
        for (i = 0; i < AIS_CONFIGURER_MAX; i++)
        {
            if (mConfigurers[i])
            {
                rc = mConfigurers[i]->Deinit();
                mConfigurers[i] = NULL;
            }
        }
        CameraPlatformClockDisable();
    }
    else
    {
        CAM_MSG(ERROR, "CameraPlatformClockEnable failed before deinit, skip deinit");
    }

    if (m_ResourceManager)
    {
        m_ResourceManager->Deinit();
        m_ResourceManager = NULL;
    }

    mDeviceManagerContext->Uninitialize();
    mDeviceManagerContext = NULL;

    CameraDestroyMutex(m_mutex);
    CameraDestroyMutex(m_usrCtxtMapMutex);

    CameraPlatformDeinit();

    m_isInitialized = FALSE;
}

AisEngine::AisEngine()
{
    time_t t;
    srand((unsigned) time(&t));

    m_magic = AIS_CTXT_MAGIC;

    std_memset(m_usrCtxtMap, 0x0, sizeof(m_usrCtxtMap));
    m_clientCount = 0;
    m_isInitialized = FALSE;

    m_ResourceManager = NULL;
    std_memset(mConfigurers, 0x0, sizeof(mConfigurers));
}

AisEngine::~AisEngine()
{

}

CameraResult AisEngine::Initialize(void)
{
    CameraResult rc = CAMERA_SUCCESS;
    int i;

    if (TRUE != CameraPlatformInit())
    {
        return CAMERA_EFAILED;
    }


    rc = CameraPlatformClockEnable();
    if (CAMERA_SUCCESS == rc)
    {
        rc = CameraCreateMutex(&m_mutex);
    }

    if (CAMERA_SUCCESS == rc)
    {
        rc = CameraCreateMutex(&m_usrCtxtMapMutex);
    }

    if (CAMERA_SUCCESS == rc)
    {
        CameraQueueCreateParamType sCreateParams;
        int i;

        /*create buffer done Q*/
        STD_ZEROAT(&sCreateParams);
        sCreateParams.nCapacity = EVENT_QUEUE_MAX_SIZE;
        sCreateParams.nDataSizeInBytes = sizeof(ais_engine_event_msg_t);
        sCreateParams.eLockType = CAMERAQUEUE_LOCK_THREAD;
        for (i = 0; i < AIS_ENGINE_QUEUE_MAX; i++)
        {
            rc = CameraQueueCreate(&m_eventQ[i], &sCreateParams);
            CAM_MSG_ON_ERR(rc, "Failed to create event queue: %d", rc);
        }
    }

    if (CAMERA_SUCCESS == rc)
    {
        rc = CameraCreateSignal(&m_eventHandlerSignal);
        CAM_MSG_ON_ERR(rc, "Failed to create signal: %d", rc);
    }

    if (CAMERA_SUCCESS == rc)
    {
        m_eventHandlerIsExit = FALSE;

        for (i = 0; i < NUM_EVENT_HNDLR_POOL; i++)
        {
            char name[64];
            snprintf(name, sizeof(name), "engine_evnt_hndlr_%d", i);

            if (0 !=  CameraCreateThread(CAMERA_THREAD_PRIO_HIGH_REALTIME,
                    0,
                    AisEngine::EventHandler,
                    this,
                    0x8000,
                    name,
                    &m_eventHandlerThreadId[i]))
            {
                CAM_MSG(ERROR, "CameraCreateThread failed");
                rc = CAMERA_EFAILED;
                break;
            }
        }
    }

    if (CAMERA_SUCCESS == rc)
    {
        mDeviceManagerContext = CameraDeviceManager::GetInstance();
        rc = mDeviceManagerContext->Initialize();
    }

    if (CAMERA_SUCCESS == rc)
    {
        mConfigurers[AIS_CONFIGURER_INPUT] = AisInputConfigurer::GetInstance();
        mConfigurers[AIS_CONFIGURER_IFE] = AisIFEConfigurer::GetInstance();
        mConfigurers[AIS_CONFIGURER_CSI] = AisCSIConfigurer::GetInstance();
    }

    // Initialize all configurers
    for (i = 0; i < AIS_CONFIGURER_MAX && CAMERA_SUCCESS == rc; i++)
    {
        if (!mConfigurers[i])
        {
            rc = CAMERA_EFAILED;
        }
        else
        {
            rc = mConfigurers[i]->Init();
        }
    }

    // Initialize resource manager
    if (CAMERA_SUCCESS == rc)
    {
        m_ResourceManager = AisResourceManager::GetInstance();
        rc = m_ResourceManager->Init();
    }

    if (CAMERA_SUCCESS == rc)
    {
        rc = AisInputConfigurer::GetInstance()->DetectAll();
    }

    if (CAMERA_SUCCESS == rc)
    {
        rc = PowerSuspend();
    }

    if (CAMERA_SUCCESS == rc)
    {
        m_isInitialized = TRUE;
    }
    else
    {
        /*clean up on error*/
        Deinitialize();
    }

    return rc;
}

/**
 * ais_initialize
 *
 * @brief Initialize AIS context, modules & resources
 *
 * @return CameraResult
 */
CameraResult AisEngine::ais_initialize(qcarcam_init_t* p_init_params)
{
    CameraResult rc = CAMERA_SUCCESS;

    CAM_UNUSED(p_init_params);

    rc = Initialize();

    return rc;
}

/**
 * ais_uninitialize
 *
 * @brief Uninitialize AIS context, modules & resources
 *
 * @return CameraResult
 */
CameraResult AisEngine::ais_uninitialize(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (m_isInitialized)
    {
        int i;
        for (i = 0; i<MAX_USR_CONTEXTS; i++)
        {
            if (m_usrCtxtMap[i].in_use)
            {
                (void)ais_close((qcarcam_hndl_t)(m_usrCtxtMap[i].phndl));
            }
        }

        Deinitialize();
    }
    else
    {
        CAM_MSG(ERROR, "Engine already uninitialized, should not happen");
        rc = CAMERA_EBADSTATE;
    }

    DestroyInstance();

    return rc;
}

/**
 * PowerSuspend
 *
 * @brief Goes into low power mode
 *
 * @return CameraResult
 */
CameraResult AisEngine::PowerSuspend(void)
{
    CameraResult rc = CAMERA_SUCCESS;
    int i;

    for (i = 0; i < AIS_CONFIGURER_MAX && CAMERA_SUCCESS == rc; i++)
    {
        rc = mConfigurers[i]->PowerSuspend();
    }

    rc = CameraSensorI2C_PowerDown();

    rc = CameraPlatformClockDisable();

    rc = CAMERA_SUCCESS;

    return rc;
}

/**
 * ais_engine_power_resume
 *
 * @brief Power back up
 *
 * @return CameraResult
 */
CameraResult AisEngine::PowerResume(void)
{
    CameraResult rc = CAMERA_SUCCESS;
    int i = 0;

    rc = CameraPlatformClockEnable();

    (void)CameraSensorI2C_PowerUp();

    for (i = 0; i < AIS_CONFIGURER_MAX && CAMERA_SUCCESS == rc; i++)
    {
        rc = mConfigurers[i]->PowerResume();
    }

    return rc;
}

/**
 * ais_assign_new_hndl
 *
 * @brief Assigns handle to new user context ptr
 *        Finds free slot in handle to usr_ctxt mapping table and
 *        creates new handle for user context using available index
 *
 * @param usr_ctxt
 *
 * @return uintptr_t - new handle
 */
uintptr_t AisEngine::AssignNewHndl(AisUsrCtxt* usr_ctxt)
{
    uintptr_t hndl = 0;
    uint32 idx;

    CameraLockMutex(m_usrCtxtMapMutex);

    for (idx = 0; idx < MAX_USR_CONTEXTS; idx++)
    {
        if (!m_usrCtxtMap[idx].in_use)
        {
            m_usrCtxtMap[idx].in_use = AIS_USR_CTXT_MAP_USED;
            m_usrCtxtMap[idx].usr_ctxt = usr_ctxt;
            m_usrCtxtMap[idx].phndl = USR_NEW_HNDL(idx);
            usr_ctxt->m_mapIdx = idx;
            hndl = m_usrCtxtMap[idx].phndl;
            break;
        }
    }

    CameraUnlockMutex(m_usrCtxtMapMutex);

    if (idx == MAX_USR_CONTEXTS)
    {
        CAM_MSG(ERROR, "Exceed Max number of contexts (%d)", MAX_USR_CONTEXTS);
    }

    return hndl;
}

/**
 * ReleaseHndl
 *
 * @brief Frees index in mapping table used by user_hndl
 *
 * @note: user_hndl is assumed to be validated through ais_get_user_context
 *
 * @param user_hndl
 *
 * @return n/a
 */
void AisEngine::ReleaseHndl(void* user_hndl)
{
    uint32 idx;
    uintptr_t hndl = (uintptr_t)user_hndl;

    idx = USR_IDX_FROM_HNDL(hndl);

    if (idx < MAX_USR_CONTEXTS)
    {
        AisUsrCtxt* usr_ctxt = NULL;

        CameraLockMutex(m_usrCtxtMapMutex);

        m_usrCtxtMap[idx].refcount--;

        if (0 == m_usrCtxtMap[idx].refcount)
        {
            usr_ctxt = (AisUsrCtxt*)m_usrCtxtMap[idx].usr_ctxt;
            memset(&m_usrCtxtMap[idx], 0x0, sizeof(m_usrCtxtMap[idx]));
        }
        else
        {
            m_usrCtxtMap[idx].in_use = AIS_USR_CTXT_MAP_DESTROY;
        }

        CameraUnlockMutex(m_usrCtxtMapMutex);

        if (usr_ctxt)
        {
            AisUsrCtxt::DestroyUsrCtxt(usr_ctxt);
        }
    }
}

/**
 * GetUsrCtxt
 *
 * @brief Get user context pointer from handle
 *
 * @param user_hndl
 *
 * @return AIS_USR_CTXT*  - NULL if fail
 */
AisUsrCtxt* AisEngine::GetUsrCtxt(void* user_hndl)
{
    uint32 idx;
    uintptr_t hndl = (uintptr_t)user_hndl;

    if (!CHECK_USR_CTXT_MAGIC(hndl))
    {
        CAM_MSG(ERROR, "invalid hndl ptr %p", hndl);
        return NULL;
    }

    idx = USR_IDX_FROM_HNDL(hndl);

    CameraLockMutex(m_usrCtxtMapMutex);
    if(idx < MAX_USR_CONTEXTS &&
       hndl == m_usrCtxtMap[idx].phndl &&
       m_usrCtxtMap[idx].in_use)
    {
        m_usrCtxtMap[idx].refcount++;
        CameraUnlockMutex(m_usrCtxtMapMutex);
        return (AisUsrCtxt*)m_usrCtxtMap[idx].usr_ctxt;
    }
    else
    {
        CAM_MSG(ERROR, "invalid hndl ptr %p", hndl);
    }

    CameraUnlockMutex(m_usrCtxtMapMutex);

    return NULL;
}

/**
 * PutUsrCtxt
 *
 * @brief relinquishes user ctxt
 *
 * @param usr_ctxt
 *
 * @return none
 */
void AisEngine::PutUsrCtxt(AisUsrCtxt* usr_ctxt)
{
    RelinquishUsrCtxt(usr_ctxt->m_mapIdx);
}

/**
 * AcquireUsrCtxt
 *
 * @brief Increments refcount of usr_ctxt handle if handle is in use
 *
 * @param idx
 *
 * @return AIS_USR_CTXT*  - NULL if fail
 */
AisUsrCtxt* AisEngine::AcquireUsrCtxt(unsigned int idx)
{
    if (idx < MAX_USR_CONTEXTS)
    {
        CameraLockMutex(m_usrCtxtMapMutex);
        if(AIS_USR_CTXT_MAP_USED == m_usrCtxtMap[idx].in_use)
        {
            m_usrCtxtMap[idx].refcount++;
            CameraUnlockMutex(m_usrCtxtMapMutex);

            return (AisUsrCtxt*)m_usrCtxtMap[idx].usr_ctxt;
        }
        CameraUnlockMutex(m_usrCtxtMapMutex);
    }

    return NULL;
}

/**
 * RelinquishUsrCtxt
 *
 * @brief Decrement refcount and cleanup if handle is being closed and last user
 *
 * @param idx
 *
 * @return none
 */
void AisEngine::RelinquishUsrCtxt(unsigned int idx)
{
    if (idx < MAX_USR_CONTEXTS)
    {
        AisUsrCtxt* usr_ctxt = NULL;

        CameraLockMutex(m_usrCtxtMapMutex);

        if (AIS_USR_CTXT_MAP_UNUSED != m_usrCtxtMap[idx].in_use)
        {
            m_usrCtxtMap[idx].refcount--;

            if (AIS_USR_CTXT_MAP_DESTROY == m_usrCtxtMap[idx].in_use &&
                !m_usrCtxtMap[idx].refcount)
            {
                m_usrCtxtMap[idx].in_use = AIS_USR_CTXT_MAP_UNUSED;
                usr_ctxt = (AisUsrCtxt*)m_usrCtxtMap[idx].usr_ctxt;
                m_usrCtxtMap[idx].usr_ctxt = NULL;
            }
        }

        CameraUnlockMutex(m_usrCtxtMapMutex);

        if (usr_ctxt)
        {
            AisUsrCtxt::DestroyUsrCtxt(usr_ctxt);
        }
    }
}

/**
 * ais_is_valid_input
 *
 * @brief Checks the qcarcam input id is valid & available
 *
 * @param desc
 *
 * @return boolean - TRUE if valid
 */
static inline boolean ais_is_valid_input(qcarcam_input_desc_t desc)
{
    return (CAMERA_SUCCESS == AisInputConfigurer::GetInstance()->IsInputAvailable(desc));
}

/**
 * ais_usr_context_init_mutex
 *
 * @brief Initialize user context mutexes
 *
 * @param usr_ctxt
 *
 * @return CameraResult
 */
CameraResult AisUsrCtxt::InitMutex(void)
{
    CameraResult rc;
    pthread_condattr_t attr;

    rc = CameraCreateMutex(&m_mutex);
    if (CAMERA_SUCCESS == rc)
    {
        if (EOK != (rc = pthread_condattr_init(&attr)))
        {
            CAM_MSG(ERROR, "%s: pthread_cond_attr_init failed: %s", __func__, strerror(rc));
        }
        else
        {
#ifndef __INTEGRITY
            if (EOK != (rc = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)))
            {
                CAM_MSG(ERROR, "%s: pthread_cond_attr_setclock failed: %s", __func__, strerror(rc));
            }
#endif
            if (EOK != (rc = pthread_cond_init(&m_bufferDoneQCond, &attr)))
            {
                CAM_MSG(ERROR, "%s: pthread_cond_init(cond_buffer_done_q) failed: %s", __func__, strerror(rc));
            }
            else if (EOK != (rc = pthread_mutex_init(&m_bufferDoneQMutex, 0)))
            {
                pthread_cond_destroy(&m_bufferDoneQCond);
            }

            // clean up - ignore non-fatal error
            int rc1 = EOK;
            if (EOK != (rc1 = pthread_condattr_destroy(&attr)))
            {
                CAM_MSG(ERROR, "%s: pthread_condattr_destroy() failed: %s", __func__, strerror(rc1));
            }
        }

        if (CAMERA_SUCCESS != rc)
        {
            CameraDestroyMutex(m_mutex);
        }
    }
    return ERRNO_TO_RESULT(rc);
}

/**
 * ais_usr_context_deinit_mutex
 *
 * @brief Deinitialize user context mutexes
 *
 * @param usr_ctxt
 *
 * @return CameraResult
 */
CameraResult AisUsrCtxt::DeinitMutex(void)
{
    int rc = EOK;
    int tmprc;

    rc = pthread_cond_destroy(&m_bufferDoneQCond);
    tmprc = pthread_mutex_destroy(&m_bufferDoneQMutex);
    if (EOK == rc) {rc = tmprc;}

    CameraDestroyMutex(m_mutex);

    return ERRNO_TO_RESULT(rc);
}

/**
 * ais_usr_context_create
 *
 * @brief Create new user context
 *
 * @param ais_ctxt - global context
 * @param input_id - qcarcam input id
 *
 * @return AIS_USR_CTXT* - NULL if fail
 */
AisUsrCtxt* AisUsrCtxt::CreateUsrCtxt(qcarcam_input_desc_t input_id)
{
    AisUsrCtxt* pUsrCtxt = new AisUsrCtxt;

    if (pUsrCtxt)
    {
        CameraResult result = CAMERA_SUCCESS;
        CameraQueueCreateParamType sCreateParams;

        pUsrCtxt->m_magic = USR_CTXT_MAGIC;

        /*create buffer done Q*/
        STD_ZEROAT(&sCreateParams);
        sCreateParams.nCapacity = QCARCAM_MAX_NUM_BUFFERS;
        sCreateParams.nDataSizeInBytes = sizeof(qcarcam_frame_info_t);
        sCreateParams.eLockType = CAMERAQUEUE_LOCK_THREAD;
        result = CameraQueueCreate(&pUsrCtxt->m_bufferDoneQ, &sCreateParams);

        /* reserve buffer list for user output buffers */
        pUsrCtxt->m_bufferList[AIS_USER_OUTPUT_IDX].m_inUse = TRUE;
        if(result == CAMERA_SUCCESS)
        {
            result = pUsrCtxt->InitMutex();
        }

        if (result == CAMERA_SUCCESS)
        {
            CAM_MSG(HIGH, "%s %p", __func__, pUsrCtxt);

            pUsrCtxt->m_inputId = input_id;
            (void)AisInputConfigurer::GetInstance()->GetParam(pUsrCtxt, AIS_INPUT_CTRL_INPUT_INTERF, NULL);
            pUsrCtxt->m_usrSettings.n_latency_max = 1;
            pUsrCtxt->m_usrSettings.n_latency_reduce_rate = 1;
        }
        else
        {
            CAM_MSG(ERROR, "Failed to create buffer done Q");
            delete pUsrCtxt;
            pUsrCtxt = NULL;
        }
    }
    else
    {
        CAM_MSG(ERROR, "Failed to allocate handle");
    }

    return pUsrCtxt;
}

/**
 * DestroyUsrCtxt
 *
 * @brief Destroy user context
 *
 * @param usr_ctxt
 *
 * @return n/a
 */
void AisUsrCtxt::DestroyUsrCtxt(AisUsrCtxt* pUsrCtxt)
{
    CameraResult result;

    CAM_MSG(HIGH, "%s %p", __func__, pUsrCtxt);

    if (pUsrCtxt)
    {
        result = CameraQueueDestroy(pUsrCtxt->m_bufferDoneQ);
        CAM_MSG_ON_ERR((CAMERA_SUCCESS != result), " CameraQueueDestroy Failed usrctx %p result=%d", pUsrCtxt, result);

        pUsrCtxt->DeinitMutex();
        CAM_MSG_ON_ERR((CAMERA_SUCCESS != result), " deinit_cond_mutex Failed usrctx %p result=%d", pUsrCtxt, result);

        delete pUsrCtxt;
    }
}

/**
 * AisUsrCtxt::Reserve
 *
 * @brief Reserve HW pipeline resources for user context
 *
 * @param usr_ctxt
 *
 * @return CameraResult
 */
CameraResult AisUsrCtxt::Reserve(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    CAM_MSG(HIGH, "%s current state m_state %d",__func__, m_state);


    if (AIS_USR_STATE_OPENED == m_state)
    {
        rc = AisResourceManager::GetInstance()->Reserve(this);
        if (CAMERA_SUCCESS == rc)
        {
            m_state = AIS_USR_STATE_RESERVED;
        }
    }
    return rc;
}

/**
 * AisUsrCtxt::Release
 *
 * @brief Release HW pipeline resources for user context
 *
 * @param usr_ctxt
 *
 * @return CameraResult
 */
CameraResult AisUsrCtxt::Release(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (AIS_USR_STATE_RESERVED == m_state)
    {
        m_state = AIS_USR_STATE_OPENED;

        rc = AisResourceManager::GetInstance()->Release(this);
    }

    return rc;
}

/**
 * AisUsrCtxt::Start
 *
 * @brief Start HW pipeline resources for user context
 *
 * @param usr_ctxt
 *
 * @return CameraResult
 */
CameraResult AisUsrCtxt::Start(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    CAM_MSG(HIGH, "%s m_state=%d", __func__, m_state);

    if (AIS_USR_STATE_RESERVED == m_state)
    {
        int i = 0;
        ais_configurer_type_t config_seq[AIS_CONFIGURER_MAX] =
                        {AIS_CONFIGURER_INPUT, AIS_CONFIGURER_CSI, AIS_CONFIGURER_IFE};

        AisEngine* pEngine = AisEngine::GetInstance();

        for (i = 0; i < AIS_CONFIGURER_MAX; i++)
        {
            rc = pEngine->mConfigurers[config_seq[i]]->Config(this);
            if (CAMERA_SUCCESS != rc)
            {
                break;
            }
        }

        if (CAMERA_SUCCESS == rc)
        {
            rc = ((AisIFEConfigurer*)pEngine->mConfigurers[AIS_CONFIGURER_IFE])->QueueBuffers(this);
        }

        /*start all resources*/
        if (CAMERA_SUCCESS == rc)
        {
            ais_configurer_type_t start_seq[AIS_CONFIGURER_MAX] =
                {AIS_CONFIGURER_CSI, AIS_CONFIGURER_IFE, AIS_CONFIGURER_INPUT};
#if 0
            int64 tm;
            CameraGetTime(&tm);
            m_startTime = (unsigned long long)(tm);
            m_isPendingStart = TRUE;
            diag.last_frame_time = m_startTime;
            diag.nframes = 0;
#endif
            for (i = 0; i < AIS_CONFIGURER_MAX; i++)
            {
                rc = pEngine->mConfigurers[start_seq[i]]->Start(this);
                if (CAMERA_SUCCESS != rc)
                {
                    break;
                }
            }

            /*if success then we are buffer pending state*/
            if (CAMERA_SUCCESS == rc)
            {
                m_state = AIS_USR_STATE_STREAMING;
            }
            else
            {
                for (; i >= 0; i--)
                {
                    rc = pEngine->mConfigurers[start_seq[i]]->Stop(this);
                }
                m_isPendingStart = FALSE;
            }
        }
    }

    return rc;
}

/**
 * AisUsrCtxt::Stop
 *
 * @brief Stop HW pipeline resources for user context
 *
 * @return CameraResult
 */
CameraResult AisUsrCtxt::Stop(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (AIS_USR_STATE_RESERVED != m_state)
    {
        uint32 i = 0;
        CameraResult tmpRet;
        ais_configurer_type_t stop_seq[AIS_CONFIGURER_MAX] =
            {AIS_CONFIGURER_INPUT, AIS_CONFIGURER_CSI, AIS_CONFIGURER_IFE};

        AisEngine* pEngine = AisEngine::GetInstance();

        pthread_mutex_lock(&m_bufferDoneQMutex);
        m_state = AIS_USR_STATE_RESERVED;

        for (i = 0; i < AIS_CONFIGURER_MAX; i++)
        {
            tmpRet = pEngine->mConfigurers[stop_seq[i]]->Stop(this);
            if (rc == CAMERA_SUCCESS) { rc = tmpRet;}
        }

        tmpRet = CameraQueueClear(m_bufferDoneQ);
        if (rc == CAMERA_SUCCESS) { rc = tmpRet;}

        //Signal buffer done q
        if (EOK != (tmpRet = pthread_cond_signal(&m_bufferDoneQCond)))
        {
            CAM_MSG(ERROR, "%s: pthread_cond_signal failed: %s",
                    __func__, strerror(tmpRet));
            rc = ERRNO_TO_RESULT(tmpRet);
        }
        pthread_mutex_unlock(&m_bufferDoneQMutex);
    }

    return rc;
}

/**
 * AisUsrCtxt::Pause
 *
 * @brief Pause HW pipeline resources for user context.
 *        For now we only stop IFE.
 *
 * @return CameraResult
 */
CameraResult AisUsrCtxt::Pause(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (AIS_USR_STATE_STREAMING == m_state)
    {
        AisEngine* pEngine = AisEngine::GetInstance();

        rc = pEngine->mConfigurers[AIS_CONFIGURER_IFE]->Pause(this);
        if (CAMERA_SUCCESS == rc)
        {
            CameraResult tmpRet;

            m_state = AIS_USR_STATE_PAUSED;

            pthread_mutex_lock(&m_bufferDoneQMutex);

            rc = CameraQueueClear(m_bufferDoneQ);

            //Signal buffer done q
            if (EOK != (tmpRet = pthread_cond_signal(&m_bufferDoneQCond)))
            {
                CAM_MSG(ERROR, "%s: pthread_cond_signal failed: %s",
                        __func__, strerror(tmpRet));
                rc = ERRNO_TO_RESULT(tmpRet);
            }
            pthread_mutex_unlock(&m_bufferDoneQMutex);
        }
        //@todo what state if fail?
    }

    return rc;
}

/**
 * AisUsrCtxt::Resume
 *
 * @brief Resume HW pipeline resources for user context.
 *        For now we only restart IFE.
 *
 * @return CameraResult
 */
CameraResult AisUsrCtxt::Resume(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (AIS_USR_STATE_PAUSED == m_state)
    {
        AisEngine* pEngine = AisEngine::GetInstance();

        rc = ((AisIFEConfigurer*)pEngine->mConfigurers[AIS_CONFIGURER_IFE])->QueueBuffers(this);
        if (CAMERA_SUCCESS == rc)
        {
            rc = pEngine->mConfigurers[AIS_CONFIGURER_IFE]->Resume(this);
        }

        if (CAMERA_SUCCESS == rc)
        {
            m_state = AIS_USR_STATE_STREAMING;
        }
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_query_inputs
///
/// @brief Queries available inputs. To get the number of available inputs to query, call with p_inputs set to NULL.
///
/// @param p_inputs   Pointer to array inputs. If NULL, then ret_size returns number of available inputs to query
/// @param size       Number of elements in array
/// @param ret_size   If p_inputs is set, number of elements in array that were filled
///                   If p_inputs is NULL, number of available inputs to query
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_query_inputs(qcarcam_input_t* p_inputs, unsigned int size, unsigned int* ret_size)
{
    AIS_API_ENTER();

    if (!ret_size)
    {
        CAM_MSG(ERROR, "%s - bad ptr", __func__);
        return CAMERA_EMEMPTR;
    }

    return AisInputConfigurer::GetInstance()->QueryInputs(p_inputs, size, ret_size);
}

///////////////////////////////////////////////////////////////////////////////
/// ais_open
///
/// @brief Opens handle to input
///
/// @param desc   Unique identifier of input to be opened
///
/// @return NOT NULL if successful; NULL on failure
///////////////////////////////////////////////////////////////////////////////
/* Check input is available (physically connected as well as SW resource)
 * Create SW context */
qcarcam_hndl_t AisEngine::ais_open(qcarcam_input_desc_t desc)
{
    qcarcam_hndl_t qcarcam_hndl = NULL;

    AIS_API_ENTER();

    if (ais_is_valid_input(desc))
    {
        CAM_MSG(LOW, "%s input id %d", __func__, desc);
        AisUsrCtxt* pUsrCtxt = AisUsrCtxt::CreateUsrCtxt(desc);
        if (pUsrCtxt)
        {
            qcarcam_hndl = (qcarcam_hndl_t)AssignNewHndl(pUsrCtxt);
            if (qcarcam_hndl)
            {
                CAM_MSG(LOW, "new handle %p for usrctxt %p input %d", qcarcam_hndl, pUsrCtxt, desc);
                pUsrCtxt->m_qcarcamHndl = qcarcam_hndl;
                pUsrCtxt->m_state = AIS_USR_STATE_OPENED;


                CAM_MSG(HIGH, "%s input id %d usrctxt id %d", __func__, desc, pUsrCtxt->m_inputId);

                Lock();
                if (0 == m_clientCount)
                {
                    PowerResume();
                }
                m_clientCount++;
                Unlock();
#if 0
                ais_diag_update_context(pUsrCtxt,AID_DIAG_CONTEXT_CREATE);
#endif
            }
            else
            {
                CAM_MSG(ERROR, "Failed to get new hndl");
                AisUsrCtxt::DestroyUsrCtxt(pUsrCtxt);
            }
        }
    }
    else
    {
        CAM_MSG(ERROR, "Invalid input id %d", desc);
    }


    return qcarcam_hndl;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_close
///
/// @brief Closes handle to input
///
/// @param hndl   Handle of input that was opened
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_close(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        CameraLockMutex(pUsrCtxt->m_mutex);

        if(pUsrCtxt->m_state == AIS_USR_STATE_STREAMING ||
           pUsrCtxt->m_state == AIS_USR_STATE_PAUSED)
        {
            CAM_MSG(HIGH, "%s closing handle %p in state %d.  Need to stop first.",
                __func__, hndl, pUsrCtxt->m_state);
            /*stop the context to release resources*/
            rc = ais_stop(hndl);
            CAM_MSG_ON_ERR(rc, "%s failed to stop handle %p rc %d", __func__, hndl, rc);
        }

        uint32 i;
        for (i = 0; i < AIS_BUFFER_LIST_MAX ;i ++)
        {
            AisBufferManager::UnmapBuffers(&pUsrCtxt->m_bufferList[i]);
        }

        pUsrCtxt->m_state = AIS_USR_STATE_UNINITIALIZED;

        CameraUnlockMutex(pUsrCtxt->m_mutex);
#if 0
        ais_diag_update_context(pUsrCtxt,AID_DIAG_CONTEXT_DELETE);
#endif

        /*release handle*/
        ReleaseHndl(hndl);

        Lock();
        m_clientCount--;
        if (0 == m_clientCount)
        {
            PowerSuspend();
        }
        Unlock();
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_g_param
///
/// @brief Get parameter value
///
/// @param hndl     Handle of input
/// @param param    Parameter to get
/// @param p_value  Pointer to structure of value that will be retrieved
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_g_param(qcarcam_hndl_t hndl, qcarcam_param_t param, qcarcam_param_value_t* p_value)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        if (!p_value)
        {
            CAM_MSG(ERROR, "%s null pointer for param %d", __func__, param);
            rc = CAMERA_EMEMPTR;
        }
        else if (AIS_USR_STATE_UNINITIALIZED == pUsrCtxt->m_state)
        {
            CAM_MSG(ERROR, "usrctxt %p in incorrect state %d",
                    pUsrCtxt, pUsrCtxt->m_state);
            rc = CAMERA_EBADSTATE;
        }
        else
        {
            switch (param)
            {
            case QCARCAM_PARAM_EVENT_CB:
                p_value->ptr_value = (void*)pUsrCtxt->m_eventCbFcn;
                break;
            case QCARCAM_PARAM_EVENT_MASK:
                p_value->uint_value = pUsrCtxt->m_eventMask;
                break;
            case QCARCAM_PARAM_EXPOSURE:
                if (USR_CTXT_CHECK_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_EXPOSURE))
                {
                    /* Get this parameter from the user context */
                    p_value->exposure_config.exposure_time =
                        pUsrCtxt->m_usrSettings.exposure_params.exposure_time;
                    p_value->exposure_config.gain =
                        pUsrCtxt->m_usrSettings.exposure_params.gain;
                    p_value->exposure_config.exposure_mode_type =
                        pUsrCtxt->m_usrSettings.exposure_params.exposure_mode_type;

                }
                else
                {
                    CAM_MSG(ERROR, "%s Exposure param not set", __func__);
                    rc = CAMERA_EBADSTATE;
                }
                break;
            case QCARCAM_PARAM_FRAME_RATE:
                if (USR_CTXT_CHECK_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_FRAME_RATE))
                {
                    /* Get this parameter from the user context */
                    p_value->frame_rate_config.frame_drop_mode =
                        pUsrCtxt->m_usrSettings.frame_rate_config.frame_drop_mode;
                    p_value->frame_rate_config.frame_drop_period =
                        pUsrCtxt->m_usrSettings.frame_rate_config.frame_drop_period;
                    p_value->frame_rate_config.frame_drop_pattern =
                        pUsrCtxt->m_usrSettings.frame_rate_config.frame_drop_pattern;

                }
                else
                {
                    p_value->frame_rate_config.frame_drop_mode = QCARCAM_KEEP_ALL_FRAMES;
                }
                break;
            case QCARCAM_PARAM_PRIVATE_DATA:
                if (USR_CTXT_CHECK_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_PRIVATE_DATA))
                {
                    p_value->ptr_value = pUsrCtxt->m_usrSettings.pPrivData;
                }
                else
                {
                    CAM_MSG(ERROR, "%s PRIVATE_DATA param not set", __func__);
                    rc = CAMERA_EBADSTATE;
                }
                break;
            case QCARCAM_PARAM_HUE:
                if (USR_CTXT_CHECK_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_HUE))
                {
                    /* Get this parameter from the user context */
                    p_value->float_value =
                        pUsrCtxt->m_usrSettings.hue_param;
                }
                else
                {
                    CAM_MSG(ERROR, "%s Hue param not set", __func__);
                    rc = CAMERA_EBADSTATE;
                }
                break;
            case QCARCAM_PARAM_SATURATION:
                if (USR_CTXT_CHECK_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_SATURATION))
                {
                    /* Get this parameter from the user context */
                    p_value->float_value =
                        pUsrCtxt->m_usrSettings.saturation_param;
                }
                else
                {
                    CAM_MSG(ERROR, "%s Saturation param not set", __func__);
                    rc = CAMERA_EBADSTATE;
                }
                break;
            default:
                CAM_MSG(ERROR, "%s unsupported param %d", __func__, param);
                rc = CAMERA_EUNSUPPORTED;
                break;
            }
        }

        PutUsrCtxt(pUsrCtxt);
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_s_param
///
/// @brief Set parameter
///
/// @param hndl     Handle of input
/// @param param    Parameter to set
/// @param p_value  Pointer to structure of value that will be set
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_s_param(qcarcam_hndl_t hndl, qcarcam_param_t param, const qcarcam_param_value_t* p_value)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        if (!p_value)
        {
            CAM_MSG(ERROR, "%s null pointer for param %d", __func__, param);
            rc = CAMERA_EMEMPTR;
        }
        else if (AIS_USR_STATE_UNINITIALIZED == pUsrCtxt->m_state)
        {
            CAM_MSG(ERROR, "%s usrctxt %p in incorrect state %d",
                    __func__, pUsrCtxt, pUsrCtxt->m_state);
            rc = CAMERA_EBADSTATE;
        }
        else
        {
            CameraLockMutex(pUsrCtxt->m_mutex);

            switch (param)
            {
            case QCARCAM_PARAM_EVENT_CB:
                pUsrCtxt->m_eventCbFcn = (qcarcam_event_cb_t)p_value->ptr_value;
                break;
            case QCARCAM_PARAM_EVENT_MASK:
                pUsrCtxt->m_eventMask = p_value->uint_value;
                break;
            case QCARCAM_PARAM_EXPOSURE:
                pUsrCtxt->m_usrSettings.exposure_params = p_value->exposure_config;

                /* Send this parameter to sensor */
                rc = AisInputConfigurer::GetInstance()->SetParam(pUsrCtxt,
                    AIS_INPUT_CTRL_EXPOSURE_CONFIG, NULL);
                if (rc == CAMERA_SUCCESS)
                {
                    USR_CTXT_SET_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_EXPOSURE);
                }
                break;
            case QCARCAM_PARAM_SATURATION:
                pUsrCtxt->m_usrSettings.saturation_param = p_value->float_value;

                /* Send this parameter to sensor */
                rc = AisInputConfigurer::GetInstance()->SetParam(pUsrCtxt,
                    AIS_INPUT_CTRL_SATURATION_CONFIG, NULL);
                if (rc == CAMERA_SUCCESS)
                {
                    USR_CTXT_SET_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_SATURATION);
                }
                break;
            case QCARCAM_PARAM_HUE:
                pUsrCtxt->m_usrSettings.hue_param = p_value->float_value;

                /* Send this parameter to sensor */
                rc = AisInputConfigurer::GetInstance()->SetParam(pUsrCtxt,
                    AIS_INPUT_CTRL_HUE_CONFIG, NULL);
                if (rc == CAMERA_SUCCESS)
                {
                    USR_CTXT_SET_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_HUE);
                }
                break;
            case QCARCAM_PARAM_RESOLUTION:
                if (AIS_USR_STATE_OPENED != pUsrCtxt->m_state &&
                    AIS_USR_STATE_RESERVED != pUsrCtxt->m_state)
                {
                    CAM_MSG(ERROR, "%s usrctxt %p in incorrect state %d",
                            __func__, pUsrCtxt, pUsrCtxt->m_state);
                    rc = CAMERA_EBADSTATE;
                }
                else
                {
                    USR_CTXT_SET_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_RESOLUTION);

                    pUsrCtxt->m_usrSettings.width = p_value->res_value.width;
                    pUsrCtxt->m_usrSettings.height = p_value->res_value.height;
                }
                break;
            case QCARCAM_PARAM_FRAME_RATE:
                pUsrCtxt->m_usrSettings.frame_rate_config.frame_drop_mode =
                    p_value->frame_rate_config.frame_drop_mode;
                pUsrCtxt->m_usrSettings.frame_rate_config.frame_drop_period =
                    p_value->frame_rate_config.frame_drop_period;
                pUsrCtxt->m_usrSettings.frame_rate_config.frame_drop_pattern =
                    p_value->frame_rate_config.frame_drop_pattern;
                if (AIS_USR_STATE_STREAMING == pUsrCtxt->m_state)
                {
#if 0
                    rc = ais_config_ife_framedrop_update(g_ais_ctxt.configurers[AIS_CONFIGURER_IFE].hndl, pUsrCtxt);
#endif
                }
                if (rc == CAMERA_SUCCESS)
                {
                    USR_CTXT_SET_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_FRAME_RATE);
                }
                break;
            case QCARCAM_PARAM_PRIVATE_DATA:
                pUsrCtxt->m_usrSettings.pPrivData = p_value->ptr_value;
                USR_CTXT_SET_MASK(pUsrCtxt->m_usrSettings.bitmask, QCARCAM_PARAM_PRIVATE_DATA);
                break;
            default:
                CAM_MSG(ERROR, "%s unsupported param %d", __func__, param);
                rc = CAMERA_EUNSUPPORTED;
                break;
            }

            CameraUnlockMutex(pUsrCtxt->m_mutex);
        }

        PutUsrCtxt(pUsrCtxt);
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_s_buffers
///
/// @brief Set buffers
///
/// @param hndl       Handle of input
/// @param p_buffers  Pointer to set buffers structure
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_s_buffers(qcarcam_hndl_t hndl, qcarcam_buffers_t* p_buffers)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        CameraLockMutex(pUsrCtxt->m_mutex);

        /*check we are in correct state*/
        if (!p_buffers)
        {
            CAM_MSG(ERROR, "p_buffers is NULL");
            rc = CAMERA_EMEMPTR;
        }
        else if (AIS_USR_STATE_OPENED != pUsrCtxt->m_state &&
            AIS_USR_STATE_RESERVED != pUsrCtxt->m_state)
        {
            CAM_MSG(ERROR, "%s usrctxt %p in incorrect state %d",
                    __func__, pUsrCtxt, pUsrCtxt->m_state);
            rc = CAMERA_EBADSTATE;
        }
        else if (p_buffers->n_buffers < MIN_USR_NUMBER_OF_BUFFERS || p_buffers->n_buffers > QCARCAM_MAX_NUM_BUFFERS)
        {
            CAM_MSG(ERROR, "Invalid number of buffers set [%d] for user %p. Need in range [%d->%d]",
                    p_buffers->n_buffers, pUsrCtxt, MIN_USR_NUMBER_OF_BUFFERS, QCARCAM_MAX_NUM_BUFFERS);
            rc = CAMERA_EBADPARM;
        }
        else
        {
            AisBufferList* pBufferList;

            pBufferList = &pUsrCtxt->m_bufferList[AIS_USER_OUTPUT_IDX];
            if (pBufferList->m_nBuffers)
            {
                CAM_MSG(HIGH, "ais_s_buffers usrctxt %p unmap buffers", pUsrCtxt);
                rc = AisBufferManager::UnmapBuffers(pBufferList);
            }

            rc = AisBufferManager::MapBuffers(pBufferList, p_buffers);

        }

        CameraUnlockMutex(pUsrCtxt->m_mutex);

        PutUsrCtxt(pUsrCtxt);
#if 0
        ais_diag_update_context(pUsrCtxt,AID_DIAG_CONTEXT_BUFFER);
#endif
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }
    return rc;
}


///////////////////////////////////////////////////////////////////////////////
/// ais_start
///
/// @brief Start input
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_start(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {

        CameraLockMutex(pUsrCtxt->m_mutex);

        AisBufferList* p_buffer_list;
        p_buffer_list = &pUsrCtxt->m_bufferList[AIS_USER_OUTPUT_IDX];

        if (AIS_USR_STATE_OPENED != pUsrCtxt->m_state &&
            AIS_USR_STATE_RESERVED != pUsrCtxt->m_state)
        {
            CAM_MSG(ERROR, "%s usrctxt %p in incorrect state %d",
                    __func__, pUsrCtxt, pUsrCtxt->m_state);
            rc = CAMERA_EBADSTATE;
        }
        else if (!p_buffer_list->m_nBuffers)
        {
            CAM_MSG(ERROR, "%s usrctxt %p no buffers set",
                                __func__, pUsrCtxt);
            rc = CAMERA_EBADSTATE;
        }
        else
        {
            Lock();

            /*reserve first*/
            rc = pUsrCtxt->Reserve();

            /*configure and start*/
            if (CAMERA_SUCCESS == rc)
            {
                rc = pUsrCtxt->Start();

                if (CAMERA_SUCCESS != rc)
                {
                    (void)pUsrCtxt->Release();
                }
            }

            Unlock();
        }

        CameraUnlockMutex(pUsrCtxt->m_mutex);

        PutUsrCtxt(pUsrCtxt);

        CAM_MSG_ON_ERR((CAMERA_SUCCESS != rc), "Failed to start usrctx %p rc=%d", pUsrCtxt, rc);
#if 0
        ais_diag_update_context(pUsrCtxt,AID_DIAG_CONTEXT_COMMON);
        ais_diag_update_context(pUsrCtxt,AIS_DIAG_CONTEXT_USEDRES);
#endif
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_stop
///
/// @brief Stop input that was started
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_stop(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        CameraResult tmpRet = CAMERA_SUCCESS;

        CameraLockMutex(pUsrCtxt->m_mutex);

        if (pUsrCtxt->m_state != AIS_USR_STATE_STREAMING &&
            pUsrCtxt->m_state != AIS_USR_STATE_PAUSED)

        {
            CAM_MSG(ERROR, "%s usrctxt %p in incorrect state %d",
                                __func__, pUsrCtxt, pUsrCtxt->m_state);
            rc = CAMERA_EBADSTATE;
        }
        else
        {
            Lock();

            tmpRet = pUsrCtxt->Stop();
            if (rc == CAMERA_SUCCESS) { rc = tmpRet;}

            tmpRet = pUsrCtxt->Release();
            if (rc == CAMERA_SUCCESS) { rc = tmpRet;}

            Unlock();
#if 0
            ais_diag_update_context(pUsrCtxt,AID_DIAG_CONTEXT_COMMON);
#endif
        }

        CameraUnlockMutex(pUsrCtxt->m_mutex);

        PutUsrCtxt(pUsrCtxt);
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_pause
///
/// @brief Pause input that was started. Does not relinquish resource
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_pause(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        CameraLockMutex(pUsrCtxt->m_mutex);

        /*must be streaming to pause*/
        if (pUsrCtxt->m_state != AIS_USR_STATE_STREAMING)
        {
            CAM_MSG(ERROR, "%s usrctxt %p in incorrect state %d",
                                __func__, pUsrCtxt, pUsrCtxt->m_state);
            rc = CAMERA_EBADSTATE;
        }
        else
        {
            Lock();
            rc = pUsrCtxt->Pause();
            Unlock();
        }

        CameraUnlockMutex(pUsrCtxt->m_mutex);

        PutUsrCtxt(pUsrCtxt);
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_resume
///
/// @brief Resumes input that was paused
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_resume(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        CameraLockMutex(pUsrCtxt->m_mutex);

        /*must be in paused state to resume*/
        if (pUsrCtxt->m_state != AIS_USR_STATE_PAUSED)
        {
            rc = CAMERA_EBADSTATE;
        }
        else
        {
            Lock();
            rc = pUsrCtxt->Resume();
            Unlock();
        }

        CameraUnlockMutex(pUsrCtxt->m_mutex);

        PutUsrCtxt(pUsrCtxt);
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_get_frame
///
/// @brief Get available frame
///
/// @param hndl          Handle of input
/// @param p_frame_info  Pointer to frame information that will be filled
/// @param timeout       Max wait time in ms for frame to be available before timeout
/// @param flags         Flags
///
/// @return CAMERA_SUCCESS if successful; CAMERA_EEXPIRED if timeout
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_get_frame(qcarcam_hndl_t hndl,
        qcarcam_frame_info_t* p_frame_info,
        unsigned long long int timeout,
        unsigned int flags)
{
    CameraResult rc = CAMERA_SUCCESS;
    struct timespec t;
    struct timespec to;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        if (!p_frame_info)
        {
            CAM_MSG(ERROR, "p_frame_info is null");
            PutUsrCtxt(pUsrCtxt);
            return CAMERA_EMEMPTR;
        }

        CameraLockMutex(pUsrCtxt->m_mutex);

        /*must be started to be able to get frame*/
        if (pUsrCtxt->m_state != AIS_USR_STATE_STREAMING)
        {
            CAM_MSG(ERROR, "%s usrctxt %p in incorrect state %d",
                                __func__, pUsrCtxt, pUsrCtxt->m_state);
            CameraUnlockMutex(pUsrCtxt->m_mutex);
            PutUsrCtxt(pUsrCtxt);
            return CAMERA_EBADSTATE;
        }

        CameraUnlockMutex(pUsrCtxt->m_mutex);

        if (timeout != QCARCAM_TIMEOUT_NO_WAIT && timeout != QCARCAM_TIMEOUT_INIFINITE)
        {
#ifndef __INTEGRITY
            if (-1 == clock_gettime(CLOCK_MONOTONIC, &to))
#else
            if (-1 == clock_gettime(CLOCK_REALTIME, &to))
#endif
            {
                CAM_MSG(ERROR, "%s: clock_gettime failed: %s",
                    __func__, strerror(errno));
                rc = ERRNO_TO_RESULT(errno);
            }
            else
            {
#if defined (__QNXNTO__)
                nsec2timespec(&t, timeout);
#else
                t.tv_sec = timeout / NANOSEC_TO_SEC;
                t.tv_nsec = timeout % NANOSEC_TO_SEC;
#endif

                CAM_MSG(LOW, "%s: t.tv_sec %d t.tv_nsec %d: timeout %d ns ",
                    __func__,t.tv_sec,  t.tv_nsec, timeout);

                to.tv_sec  += t.tv_sec;
                to.tv_nsec += t.tv_nsec;
                if (to.tv_nsec >= NANOSEC_TO_SEC)
                {
                    to.tv_sec  += 1;
                    to.tv_nsec -= NANOSEC_TO_SEC;
                }
            }
        }

        if (CAMERA_SUCCESS == rc)
        {
            int result = EOK;
            boolean isEmpty = TRUE;

            pthread_mutex_lock(&pUsrCtxt->m_bufferDoneQMutex);

            do {
                /*in case of abort*/
                if (pUsrCtxt->m_state != AIS_USR_STATE_STREAMING)
                {
                    CAM_MSG(ERROR, "%s: abort called",
                                        __func__, rc);
                    rc = CAMERA_ENOMORE;
                    isEmpty = TRUE;
                    break;
                }

                rc = CameraQueueIsEmpty(pUsrCtxt->m_bufferDoneQ, &isEmpty);

                if (isEmpty)
                {
                    if (QCARCAM_TIMEOUT_NO_WAIT == timeout)
                    {
                        rc = CAMERA_ENOMORE;
                    }
                    else if (QCARCAM_TIMEOUT_INIFINITE == timeout)
                    {
                        result = pthread_cond_wait(&pUsrCtxt->m_bufferDoneQCond,
                                &pUsrCtxt->m_bufferDoneQMutex);
                    }
                    else
                    {
                        result = pthread_cond_timedwait(&pUsrCtxt->m_bufferDoneQCond,
                                &pUsrCtxt->m_bufferDoneQMutex, &to);
                    }
                }
                else
                {
                    //Something in Q, break out of loop
                    result = EOK;
                    break;
                }

                CAM_MSG(LOW, "ais_get_frame usrctxt %p isEmpty %d result %d rc %d",
                        pUsrCtxt, isEmpty, result, rc);

            } while (isEmpty && (EOK == result) && rc == CAMERA_SUCCESS);

            pthread_mutex_unlock(&pUsrCtxt->m_bufferDoneQMutex);

            if (EOK != result)
            {
                CAM_MSG(ERROR, "%s: pthread_cond_wait failed: %s",
                    __func__, strerror(result));
                rc = ERRNO_TO_RESULT(result);
            }
            else if(CAMERA_SUCCESS != rc)
            {
                CAM_MSG(ERROR, "%s: CameraQueueIsEmpty call failed: %d",
                    __func__, rc);
            }
            else if (!isEmpty)
            {
                rc = CameraQueueDequeue(pUsrCtxt->m_bufferDoneQ, p_frame_info);

                if (rc == CAMERA_SUCCESS)
                {
                    AisBufferList* p_buffer_list;
                    p_buffer_list = &pUsrCtxt->m_bufferList[AIS_USER_OUTPUT_IDX];

                    rc = p_buffer_list->SetBufferState(p_frame_info->idx, AIS_USER_ACQUIRED);
#if 0
                    if( ais_diag_isenable(AIS_DIAG_GET_FRAMETS))
                    {
                        int64 tm = 0;
                        CameraGetTime(&tm);
                        ais_diag_log(AIS_DIAG_GET_FRAMETS,&tm);
                    }
                    ais_diag_update_context(pUsrCtxt,AIS_DIAG_CONTEXT_BUFFER_STATE);
#endif
                }

            }

            CAM_MSG(LOW, "ais_get_frame usrctxt %p rc=%d, idx=%d", pUsrCtxt, rc, p_frame_info->idx);
        }
        PutUsrCtxt(pUsrCtxt);
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_release_frame
///
/// @brief Re-enqueue frame buffers
///
/// @param hndl       Handle of input
/// @param idx        Index into the qcarcam_buffers_t buffers table to reenqueue
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
CameraResult AisEngine::ais_release_frame(qcarcam_hndl_t hndl, unsigned int idx)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        CameraLockMutex(pUsrCtxt->m_mutex);

        /*must be streaming to be able to release frame*/
        if (pUsrCtxt->m_state != AIS_USR_STATE_STREAMING)
        {
            CAM_MSG(ERROR, "%s usrctxt %p in incorrect state %d",
                                __func__, pUsrCtxt, pUsrCtxt->m_state);
            rc = CAMERA_EBADSTATE;
        }
        else
        {
            AisBufferList* p_buffer_list;
            p_buffer_list = &pUsrCtxt->m_bufferList[AIS_USER_OUTPUT_IDX];

            if (p_buffer_list->GetBufferState(idx) == AIS_USER_ACQUIRED)
            {
                rc = ((AisIFEConfigurer*)mConfigurers[AIS_CONFIGURER_IFE])->QueueBuffer(pUsrCtxt, p_buffer_list, idx);

                if( rc != CAMERA_SUCCESS)
                {
                    CAM_MSG(ERROR, "%s queue_buffer fail %u", __func__, idx);
                }
#if 0
                if( ais_diag_isenable(AIS_DIAG_RELEASE_FRAMETS))
                {
                    int64 tm = 0;
                    CameraGetTime(&tm);
                    ais_diag_log(AIS_DIAG_RELEASE_FRAMETS,&tm);
                }
#endif
            }
            else
            {
                CAM_MSG(ERROR, "%s this buffer has already been released %u", __func__,idx);
            }

        }

        CameraUnlockMutex(pUsrCtxt->m_mutex);

        PutUsrCtxt(pUsrCtxt);
    }
    else
    {
        CAM_MSG(ERROR, "%s invalid hndl %p", __func__, hndl);
        rc = CAMERA_EBADHANDLE;
    }
    return rc;
}


//////////////////////////////////////////////////////////////////////////////////
/// INTERNAL ENGINE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////

/**
 * ais_usr_ctxt_find
 *
 * @brief Returns user context that first matches the match_func
 *
 * @note: Caller must call PutUsrCtxt() on returned context when done
 *
 * @param ais_ctxt
 * @param match_func  match function
 * @param match       match function argument

 * @return AIS_USR_CTXT*     NULL if not found
 */
AisUsrCtxt* AisEngine::FindUsrCtxt(ais_usr_ctxt_match_func match_func, void* match)
{
    uint32 idx;
    AisUsrCtxt* pUsrCtxt;

    for (idx = 0; idx < MAX_USR_CONTEXTS; idx++)
    {
        pUsrCtxt = AcquireUsrCtxt(idx);
        if (pUsrCtxt)
        {
            CameraLockMutex(pUsrCtxt->m_mutex);
            if (match_func(pUsrCtxt, match))
            {
                CameraUnlockMutex(pUsrCtxt->m_mutex);
                return pUsrCtxt;
            }
            CameraUnlockMutex(pUsrCtxt->m_mutex);
            RelinquishUsrCtxt(idx);
        }
    }

    return NULL;
}

/**
 * ais_usr_ctxt_traverse
 *
 * @brief Executes op_func for all contexts that match the match_func
 *
 * @param ais_ctxt
 * @param match_func match function
 * @param match match data parameter
 * @param op_func operate function
 * @param user_data operate function parameter
 *
 * @return boolean TRUE if callback success
 */
uint32 AisEngine::TraverseUsrCtxt(ais_usr_ctxt_match_func match_func, void* match,
        ais_usr_ctxt_operate_func op_func, void* user_data)
{
    uint32 idx;
    boolean ret = FALSE;
    AisUsrCtxt* pUsrCtxt;

    for (idx = 0; idx < MAX_USR_CONTEXTS; idx++)
    {
        pUsrCtxt = AcquireUsrCtxt(idx);
        if (pUsrCtxt)
        {
            CameraLockMutex(pUsrCtxt->m_mutex);
            if (match_func(pUsrCtxt, match))
            {
                ret = op_func(pUsrCtxt, user_data);
            }
            CameraUnlockMutex(pUsrCtxt->m_mutex);
            RelinquishUsrCtxt(idx);
        }
    }

    return ret;
}

/**
 * ais_usr_ctxt_match_csid
 *
 * @brief Matches user context using csid core
 *
 * @param pUsrCtxt
 * @param match  (ais_csid_fatal_error_t*)
 * @return boolean TRUE if match
 */
static boolean ais_usr_ctxt_match_csid(AisUsrCtxt* pUsrCtxt, void* match)
{
    return (pUsrCtxt->m_resources.csid == ((ais_csid_fatal_error_t*)match)->csid);
}

/**
 * ais_usr_ctxt_match_ifeid
 *
 * @brief Matches user context using ifeid
 *
 * @param usr_ctxt
 * @param match  (ais_sof_t*)
 * @return boolean TRUE if match
 */
static boolean ais_usr_ctxt_match_ifeid(AisUsrCtxt* pUsrCtxt, void* match)
{
    ais_ife_user_path_info* ife_path_info = &pUsrCtxt->m_resources.ife_user_path_info[0];
    return ((ife_path_info->ife_core == ((ais_sof_t*)match)->ife_core) &&
        (ife_path_info->ife_interf == ((ais_sof_t*)match)->ife_input));
}

/**
 * ais_usr_ctxt_csid_error
 *
 * @brief handles csid fatal errors. Notifies users they have been aborted and initiates recovery
 *
 * @param pUsrCtxt
 * @param user_data
 * @return boolean TRUE if success
 */
static boolean ais_usr_ctxt_csid_error(AisUsrCtxt* pUsrCtxt, void* user_data)
{
    CAM_MSG(HIGH, "CSID error for context %p", pUsrCtxt);

    if (pUsrCtxt->m_eventCbFcn && (pUsrCtxt->m_eventMask & QCARCAM_EVENT_ERROR))
    {
        CAM_MSG(HIGH, "Send error for context %p", pUsrCtxt);
        pUsrCtxt->m_eventCbFcn(pUsrCtxt->m_qcarcamHndl, QCARCAM_EVENT_ERROR, NULL);
    }

    /*
    ais_usr_context_stop(pUsrCtxt);
    ais_usr_context_release(pUsrCtxt);
    pUsrCtxt->state = AIS_USR_STATE_ERROR;
    ais_engine_unlock();
    */

    return TRUE;
}

/**
 * ais_usr_ctxt_match_input_status
 *
 * @brief Matches user context with same sensor
 *
 * @param pUsrCtxt
 * @param match  (ais_input_status_payload_t*)
 * @return boolean TRUE if match
 */
static boolean ais_usr_ctxt_match_input_status(AisUsrCtxt* pUsrCtxt, void* match)
{
    return ((pUsrCtxt->m_inputCfg.devId == ((ais_input_status_payload_t*)match)->dev_id) &&
            (pUsrCtxt->m_inputCfg.srcId == ((ais_input_status_payload_t*)match)->src_id));
}

/**
 * ais_usr_ctxt_input_status
 *
 * @brief handles input signal changes. Notifies users they have been aborted and initiates recovery
 *
 * @param pUsrCtxt
 * @param user_data
 * @return boolean TRUE if success
 */
static boolean ais_usr_ctxt_input_status(AisUsrCtxt* pUsrCtxt, void* user_data)
{
    if (pUsrCtxt->m_eventCbFcn && (pUsrCtxt->m_eventMask & QCARCAM_EVENT_INPUT_SIGNAL))
    {
        CAM_MSG(HIGH, "Send input Status for context %p", pUsrCtxt);
        pUsrCtxt->m_eventCbFcn(pUsrCtxt->m_qcarcamHndl, QCARCAM_EVENT_INPUT_SIGNAL, (qcarcam_event_payload_t*)user_data);
    }

    return TRUE;
}

/**
 * ais_usr_ctxt_match_ife_frame_done
 *
 * @brief Matches user context with ife frame done event
 *
 * @param usr_ctxt
 * @param match  (ais_frame_done_t*)
 * @return boolean TRUE if match
 */
static boolean ais_usr_ctxt_match_ife_frame_done(AisUsrCtxt* pUsrCtxt, void* match)
{
    ais_ife_user_path_info* ife_path_info = &pUsrCtxt->m_resources.ife_user_path_info[0];

    CAM_MSG(MEDIUM, "usr_ctxt %p ife_core %d ife_output %d", pUsrCtxt, ife_path_info->ife_core, ife_path_info->ife_output);

    return ((AIS_USR_STATE_STREAMING == pUsrCtxt->m_state) &&
            (ife_path_info->ife_core == ((ais_frame_done_t*)match)->ife_core) &&
            (ife_path_info->ife_output == ((ais_frame_done_t*)match)->ife_output));
}

/**
 * ais_usr_ctxt_event_frame_done
 *
 * @brief Handles frame done event of a user context
 *
 * @param usr_ctxt
 * @param ais_frame_done
 * @return CameraResult  CAMERA_SUCCESS on success
 */
CameraResult AisEngine::UsrCtxtEventFrameDone(AisUsrCtxt* pUsrCtxt, ais_frame_done_t* ais_frame_done)
{
    CameraResult result = CAMERA_SUCCESS;
    AisBufferList* pBufferList = &pUsrCtxt->m_bufferList[AIS_USER_OUTPUT_IDX];
    uint32 nBufferDoneQLength;
    int rc = EOK;

    CameraLockMutex(pUsrCtxt->m_mutex);

    //context must be streaming
    if (pUsrCtxt->m_state != AIS_USR_STATE_STREAMING)
    {
        CAM_MSG(HIGH, "IFE %d Output %d context %p in bad state %d",
                        ais_frame_done->ife_core, ais_frame_done->ife_output, pUsrCtxt, pUsrCtxt->m_state);
        CameraUnlockMutex(pUsrCtxt->m_mutex);
        return CAMERA_EBADSTATE;
    }

    if (pUsrCtxt->m_isPendingStart)
    {
        CAM_MSG(RESERVED, "IFE %d Output %d First Frame buffer %d",
                ais_frame_done->ife_core, ais_frame_done->ife_output, ais_frame_done->frame_info.idx);
        pUsrCtxt->m_isPendingStart = FALSE;
    }

    if( ais_frame_done->frame_info.timestamp <= pUsrCtxt->m_startTime )
    {
        CAM_MSG(HIGH,  "frame timestamp <= usr_ctxt->starttime");
        CameraUnlockMutex(pUsrCtxt->m_mutex);
        return CAMERA_EFAILED;
    }

    pthread_mutex_lock(&pUsrCtxt->m_bufferDoneQMutex);

    /*Check latency and reduce if needed*/
    CameraQueueGetLength(pUsrCtxt->m_bufferDoneQ, &nBufferDoneQLength);
    if (nBufferDoneQLength > pUsrCtxt->m_usrSettings.n_latency_max)
    {
        uint32 i;

        for (i = 0; i<pUsrCtxt->m_usrSettings.n_latency_reduce_rate; i++)
        {
            qcarcam_frame_info_t sFrameInfo;
            result = CameraQueueDequeue(pUsrCtxt->m_bufferDoneQ, (CameraQueueDataType)&sFrameInfo);
            if (CAMERA_SUCCESS == result)
            {
                rc = ((AisIFEConfigurer*)mConfigurers[AIS_CONFIGURER_IFE])->QueueBuffer(pUsrCtxt, pBufferList, sFrameInfo.idx);
                if (CAMERA_SUCCESS == result)
                {
                    CAM_MSG(HIGH, "ctxt %p requeued frame %d to IFE", pUsrCtxt, sFrameInfo.idx);
                }
                else
                {
                    CAM_MSG(ERROR, "ctxt %p failed to requeue frame %d to IFE", pUsrCtxt, sFrameInfo.idx);
                }
            }
            else
            {
                CAM_MSG(ERROR, "ctxt %p failed to dequeue frame to requeue", pUsrCtxt);
            }
        }
    }

    /*Queue new frame and signal condition*/
    result = CameraQueueEnqueue(pUsrCtxt->m_bufferDoneQ, (CameraQueueDataType)&(ais_frame_done->frame_info));
    pBufferList->SetBufferState(ais_frame_done->frame_info.idx, AIS_DELIVERY_QUEUE);
#if 0
    ais_diag_update_context(pUsrCtxt,AIS_DIAG_CONTEXT_BUFFER_STATE);
#endif
    if (EOK != (rc = pthread_cond_signal(&pUsrCtxt->m_bufferDoneQCond)))
    {
        CAM_MSG(ERROR, "%s: pthread_cond_signal failed: %s",
                __FUNCTION__, strerror(rc));
        result = ERRNO_TO_RESULT(rc);
    }

    pthread_mutex_unlock(&pUsrCtxt->m_bufferDoneQMutex);

    /*send callback if needed*/
    if (pUsrCtxt->m_eventCbFcn && (pUsrCtxt->m_eventMask & QCARCAM_EVENT_FRAME_READY))
    {
        pUsrCtxt->m_eventCbFcn(pUsrCtxt->m_qcarcamHndl, QCARCAM_EVENT_FRAME_READY, NULL);
    }

    CameraUnlockMutex(pUsrCtxt->m_mutex);

    return result;
}

/**
 * ais_usr_ctxt_field_info_insert
 *
 * @brief  insert field info into user context
 *
 * @param usr_ctxt
 * @param ais_field_info_t
 * @return CameraResult  CAMERA_SUCCESS on success
 */
static CameraResult ais_usr_ctxt_field_info_insert(AisUsrCtxt* pUsrCtxt, ais_field_info_t field_info)
{
    CameraResult result = CAMERA_SUCCESS;
    int i;

    //context must be streaming
    if (pUsrCtxt->m_state != AIS_USR_STATE_STREAMING)
    {
        CAM_MSG(HIGH, "Don't insert field info into context %p in bad state %d",
                        pUsrCtxt, pUsrCtxt->m_state);
        return CAMERA_EBADSTATE;
    }

    for (i = 0; i < AIS_FIELD_BUFFER_SIZE; i++)
    {
        if (pUsrCtxt->m_fieldInfo[i].valid != TRUE)
        {
            pUsrCtxt->m_fieldInfo[i] = field_info;
            pUsrCtxt->m_fieldInfo[i].valid = TRUE;
            CAM_MSG(MEDIUM, "Insert [%d]: %llu, %llu, %d %d",
                i, pUsrCtxt->m_fieldInfo[i].sof_ts/1000,
                pUsrCtxt->m_fieldInfo[i].field_ts/1000,
                pUsrCtxt->m_fieldInfo[i].target_frame_id,
                pUsrCtxt->m_fieldInfo[i].even_field);
            break;
        }
    }

    if (i == AIS_FIELD_BUFFER_SIZE)
    {
        int min_idx = 0;
        uint64_t min_sof_ts = pUsrCtxt->m_fieldInfo[0].sof_ts;
        for (i = 1; i < AIS_FIELD_BUFFER_SIZE; i++)
        {
            if (pUsrCtxt->m_fieldInfo[i].sof_ts < min_sof_ts)
            {
                min_sof_ts = pUsrCtxt->m_fieldInfo[i].sof_ts;
                min_idx = i;
            }
        }
        pUsrCtxt->m_fieldInfo[min_idx] = field_info;
        pUsrCtxt->m_fieldInfo[min_idx].valid = TRUE;
        CAM_MSG(MEDIUM, "Overwrite [%d]: %llu, %llu, %d %d",
            min_idx, pUsrCtxt->m_fieldInfo[min_idx].sof_ts/1000,
            pUsrCtxt->m_fieldInfo[min_idx].field_ts/1000,
            pUsrCtxt->m_fieldInfo[min_idx].target_frame_id,
            pUsrCtxt->m_fieldInfo[min_idx].even_field);
    }

    return result;
}

/**
 * ais_usr_ctxt_calc_field
 *
 * @brief figure out field type in the current frame
 *
 * @param usr_ctxt
 * @param ais_frame_done
 * @return CameraResult  CAMERA_SUCCESS on success
 */
static CameraResult ais_usr_ctxt_calc_field(AisUsrCtxt* pUsrCtxt, ais_frame_done_t* ais_frame_done)
{
    CameraResult result = CAMERA_SUCCESS;
    int i, temp_idx, f1_idx = -1, f2_idx = -1;
    uint32_t frame_id = ais_frame_done->frame_info.seq_no;
    uint64_t frame_ts = ais_frame_done->frame_info.timestamp;
    ais_field_info_t field_info;

    for (i = 0; i < AIS_FIELD_BUFFER_SIZE; i++)
    {
        if (pUsrCtxt->m_fieldInfo[i].valid == TRUE)
        {
            field_info = pUsrCtxt->m_fieldInfo[i];
            if (field_info.target_frame_id <= frame_id)
            {
                pUsrCtxt->m_fieldInfo[i].valid = FALSE;

                if (f1_idx == -1)
                {
                    f1_idx = i;
                }
                else if (f2_idx == -1)
                {
                    f2_idx = i;
                    if (pUsrCtxt->m_fieldInfo[f2_idx].sof_ts < pUsrCtxt->m_fieldInfo[f1_idx].sof_ts)
                    {
                        temp_idx = f1_idx;
                        f1_idx = f2_idx;
                        f2_idx = temp_idx;
                    }
                }
                else
                {
                    if (field_info.sof_ts > pUsrCtxt->m_fieldInfo[f1_idx].sof_ts &&
                        field_info.sof_ts < pUsrCtxt->m_fieldInfo[f2_idx].sof_ts)
                    {
                        f1_idx = i;
                    }
                    else if (field_info.sof_ts > pUsrCtxt->m_fieldInfo[f2_idx].sof_ts)
                    {
                        f1_idx = f2_idx;
                        f2_idx = i;
                    }
                }
            }
        }
    }

    if ((f1_idx == -1) || (f2_idx == -1))
    {
        ais_frame_done->frame_info.field_type = QCARCAM_FIELD_UNKNOWN;
        CAM_MSG(ERROR, "Not enough field info for current frame %d, %d:%d, %llu", frame_id, f1_idx, f2_idx, frame_ts);
        for (i = 0; i < AIS_FIELD_BUFFER_SIZE; i++)
        {
            CAM_MSG(ERROR, "[%d]: %llu-%llu %d %d", i,
                           pUsrCtxt->m_fieldInfo[i].sof_ts, pUsrCtxt->m_fieldInfo[i].field_ts,
                           pUsrCtxt->m_fieldInfo[i].target_frame_id, pUsrCtxt->m_fieldInfo[i].even_field);
        }
    }
    else
    {
        if ((pUsrCtxt->m_fieldInfo[f1_idx].sof_ts <= pUsrCtxt->m_fieldInfo[f1_idx].field_ts) &&
            (pUsrCtxt->m_fieldInfo[f2_idx].sof_ts <= pUsrCtxt->m_fieldInfo[f2_idx].field_ts) &&
            (pUsrCtxt->m_fieldInfo[f1_idx].field_ts < pUsrCtxt->m_fieldInfo[f2_idx].sof_ts) &&
            (pUsrCtxt->m_fieldInfo[f2_idx].field_ts < frame_ts) &&
            ((frame_ts - pUsrCtxt->m_fieldInfo[f1_idx].sof_ts)/1000000 < (33 + 15)) &&
            (pUsrCtxt->m_fieldInfo[f1_idx].even_field != pUsrCtxt->m_fieldInfo[f2_idx].even_field))
        {
            ais_frame_done->frame_info.field_type = pUsrCtxt->m_fieldInfo[f1_idx].even_field ?
                                                    QCARCAM_FIELD_EVEN_ODD : QCARCAM_FIELD_ODD_EVEN;
        }
        else
        {
            ais_frame_done->frame_info.field_type = QCARCAM_FIELD_UNKNOWN;
            CAM_MSG(ERROR, "Trustless field info %llu-%llu-%llu-%llu: %d %d %llu %d",
                pUsrCtxt->m_fieldInfo[f1_idx].sof_ts, pUsrCtxt->m_fieldInfo[f1_idx].field_ts,
                pUsrCtxt->m_fieldInfo[f2_idx].sof_ts, pUsrCtxt->m_fieldInfo[f2_idx].field_ts,
                pUsrCtxt->m_fieldInfo[f1_idx].even_field, pUsrCtxt->m_fieldInfo[f2_idx].even_field,
                frame_ts, frame_id);
        }
    }

    return result;
}

/**
 * ais_engine_queue_event
 *
 * @brief Queues events for engine
 *
 * @param ais_ctxt
 * @param event_id
 * @param payload
 *
 * @return CameraResult
 */
CameraResult AisEngine::QueueEvent(ais_engine_event_msg_t* pMsg)
{
    CameraResult result;

    CAM_MSG(LOW, "q_event %d", pMsg->event_id);

    result = CameraQueueEnqueue(m_eventQ[AIS_ENGINE_QUEUE_EVENTS], pMsg);
    if (result == CAMERA_SUCCESS)
    {
        result = CameraSetSignal(m_eventHandlerSignal);
    }

    return result;
}

/**
 * ProcessEvent
 *
 * @brief Dequeues event from event Q and processes it
 *
 * @param pMsg
 *
 * @return int
 */
int AisEngine::ProcessEvent(ais_engine_event_msg_t* pMsg)
{
    CameraResult result;

    result = CameraQueueDequeue(m_eventQ[AIS_ENGINE_QUEUE_EVENTS], pMsg);
    if (CAMERA_SUCCESS != result)
    {
        if (CAMERA_ENOMORE != result)
        {
            CAM_MSG(ERROR, "Failed to dequeue event (%d)", result);
        }
        return 0;
    }

    switch (pMsg->event_id)
    {
    case AIS_EVENT_RAW_FRAME_DONE:
    case AIS_EVENT_FRAME_DONE:
    {
        AisUsrCtxt* pUsrCtxt;
        ais_frame_done_t* ais_frame_done = &pMsg->payload.ife_frame_done;
        pUsrCtxt = FindUsrCtxt(ais_usr_ctxt_match_ife_frame_done, ais_frame_done);
        if (pUsrCtxt)
        {
            CAM_MSG(MEDIUM, "frame done: %llu, %d", ais_frame_done->frame_info.timestamp/1000, ais_frame_done->frame_info.seq_no);
            if (pUsrCtxt->m_inputCfg.inputModeInfo.interlaced)
            {
                CameraLockMutex(pUsrCtxt->m_mutex);
                ais_usr_ctxt_calc_field(pUsrCtxt, ais_frame_done);
                CameraUnlockMutex(pUsrCtxt->m_mutex);
            }
            UsrCtxtEventFrameDone(pUsrCtxt, ais_frame_done);
            PutUsrCtxt(pUsrCtxt);
        }
        else
        {
            CAM_MSG(ERROR, "Can't find user context for frame done of ife %d output %d",
                    ais_frame_done->ife_core, ais_frame_done->ife_output);
            //Todo: cleanup?
        }
        break;
    }
    case AIS_EVENT_CSI_FATAL_ERROR:
    {
        TraverseUsrCtxt(ais_usr_ctxt_match_csid, &pMsg->payload.csid_fatal_error,
                ais_usr_ctxt_csid_error, NULL);
        break;
    }
    case AIS_EVENT_INPUT_STATUS:
    {
        qcarcam_event_payload_t input_event;

        input_event.uint_payload = pMsg->payload.input_status.signal_status;

        TraverseUsrCtxt(ais_usr_ctxt_match_input_status, &pMsg->payload.input_status,
                ais_usr_ctxt_input_status, &input_event);
        break;
    }
    case AIS_EVENT_SOF:
    {
        ais_field_info_t field_info;
        ais_sof_t* sof_info = &pMsg->payload.sof_info;
        AisUsrCtxt* pUsrCtxt = FindUsrCtxt(ais_usr_ctxt_match_ifeid, sof_info);
        if (pUsrCtxt && (AIS_USR_STATE_STREAMING == pUsrCtxt->m_state))
        {
            if (pUsrCtxt->m_inputCfg.inputModeInfo.interlaced)
            {
                CameraLockMutex(pUsrCtxt->m_mutex);
#if 0
                result = ais_config_input_g_param(configurers[AIS_CONFIG_INPUT].hndl, pUsrCtxt,
                                      AIS_INPUT_CTRL_FIELD_TYPE, &field_info);
#endif
                if (result == CAMERA_SUCCESS)
                {
                    field_info.sof_ts = sof_info->sof_ts;
                    field_info.target_frame_id = sof_info->target_frame_id;
                    ais_usr_ctxt_field_info_insert(pUsrCtxt, field_info);
                }
                else
                {
                    CAM_MSG(ERROR, "ais_config_input_g_param failed: %d", result);
                }

                CameraUnlockMutex(pUsrCtxt->m_mutex);
            }
            PutUsrCtxt(pUsrCtxt);
        }
        else
        {
            CAM_MSG(ERROR, "Can't find user context for field query of ife %d input %d",
                    sof_info->ife_core, sof_info->ife_input);
        }
        break;
    }
    case AIS_EVENT_SOF_FREEZE:
    {
        TraverseUsrCtxt(ais_usr_ctxt_match_ifeid, &pMsg->payload.sof_info,
                ais_usr_ctxt_csid_error, NULL);
        break;
    }
    default:
        break;
    }

    return 1;
}

/**
 * ais_engine_event_handler
 *
 * @brief Engine event handler thread to process engine events
 *
 * @param ais_ctxt
 *
 * @return int
 */
int AisEngine::EventHandler(void *arg)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisEngine* ais_ctxt = (AisEngine*)arg;

    if (ais_ctxt)
    {
        ais_engine_event_msg_t* pMsg = (ais_engine_event_msg_t*)CameraAllocate(CAMERA_ALLOCATE_ID_UNASSIGNED, sizeof(ais_engine_event_msg_t));
        if (pMsg)
        {
            while (!ais_ctxt->m_eventHandlerIsExit)
            {
                // track if there is anything processed in this iteration
                int serviced = 0;

                CAM_MSG(LOW, "%s: is awake; ready to work.",
                    __FUNCTION__);

                //
                // service IFE callback, parse the msg, and put filled buffers in filledQ.
                //
                serviced += ais_ctxt->ProcessEvent(pMsg);

                //
                // If we woke up with nothing to do, we'll try to sleep again.
                //
                //if (!serviced)
                {
                    CAM_MSG(LOW, "%s: has nothing to do, going to sleep", __FUNCTION__);
                    CameraWaitOnSignal(ais_ctxt->m_eventHandlerSignal, CAM_SIGNAL_WAIT_NO_TIMEOUT);
                }
            }

            CameraFree(CAMERA_ALLOCATE_ID_UNASSIGNED, pMsg);
        }
        else
        {
            rc = CAMERA_ENOMEMORY;
        }

        CAM_MSG(HIGH, "%s: terminating (%d) ...", __FUNCTION__, rc);
    }

    // Don't use MM_Thread_Exit.
    // - Depending on scheduler's thread exection,
    //   MM_Thread_Exit may just do "return 1",
    //   which is not what we want.

    return 0;
}

///@brief AisEngine singleton
AisEngine* AisEngine::m_pEngineInstance = nullptr;

/***************************************************************************************************************************
*   AisEngine::CreateInstance
*
*   @brief
*       Create singleton instance for AisEngine
***************************************************************************************************************************/
AisEngine* AisEngine::CreateInstance()
{
    if(m_pEngineInstance == nullptr)
    {
        m_pEngineInstance = new AisEngine();
    }

    return m_pEngineInstance;
}

/***************************************************************************************************************************
*   AisEngine::GetInstance
*
*   @brief
*       Gets the singleton instance for AisEngine
***************************************************************************************************************************/
AisEngine* AisEngine::GetInstance()
{
    return m_pEngineInstance;
}

/***************************************************************************************************************************
*   AisEngine::DestroyInstance
*
*   @brief
*       Destroy the singleton instance of the AisEngine class
*
*   @return
*       void
***************************************************************************************************************************/
void AisEngine::DestroyInstance()
{
    if(m_pEngineInstance != nullptr)
    {
        delete m_pEngineInstance;
        m_pEngineInstance = nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////
/// ais_initialize
///
/// @brief Initialize AIS engine
///
/// @param initialization parameters
///
/// @return CAMERA_SUCCESS if successful;
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_initialize(qcarcam_init_t* p_init_params)
{
    CameraResult result;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        CAM_MSG(ERROR, "AisEngine already initialized");
        return CAMERA_EBADSTATE;
    }

    pEngine = AisEngine::CreateInstance();
    if (!pEngine)
    {
        return CAMERA_ENOMEMORY;
    }

    result = pEngine->ais_initialize(p_init_params);

    if (CAMERA_SUCCESS != result)
    {
        AisEngine::DestroyInstance();
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_uninitialize
///
/// @brief Uninitialize AIS engine
///
/// @return CAMERA_SUCCESS if successful;
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_uninitialize(void)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_uninitialize();
    }

    return result;
}


///////////////////////////////////////////////////////////////////////////////
/// ais_query_inputs
///
/// @brief Queries available inputs. To get the number of available inputs to query, call with p_inputs set to NULL.
///
/// @param p_inputs   Pointer to array inputs. If NULL, then ret_size returns number of available inputs to query
/// @param size       Number of elements in array
/// @param ret_size   If p_inputs is set, number of elements in array that were filled
///                   If p_inputs is NULL, number of available inputs to query
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_query_inputs(qcarcam_input_t* p_inputs, unsigned int size, unsigned int* ret_size)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_query_inputs(p_inputs, size, ret_size);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_open
///
/// @brief Opens handle to input
///
/// @param desc   Unique identifier of input to be opened
///
/// @return NOT NULL if successful; NULL on failure
///////////////////////////////////////////////////////////////////////////////
AIS_API qcarcam_hndl_t ais_open(qcarcam_input_desc_t desc)
{
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        return pEngine->ais_open(desc);
    }

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_close
///
/// @brief Closes handle to input
///
/// @param hndl   Handle of input that was opened
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_close(qcarcam_hndl_t hndl)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_close(hndl);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_g_param
///
/// @brief Get parameter value
///
/// @param hndl     Handle of input
/// @param param    Parameter to get
/// @param p_value  Pointer to structure of value that will be retrieved
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_g_param(qcarcam_hndl_t hndl, qcarcam_param_t param, qcarcam_param_value_t* p_value)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_g_param(hndl, param, p_value);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_s_param
///
/// @brief Set parameter
///
/// @param hndl     Handle of input
/// @param param    Parameter to set
/// @param p_value  Pointer to structure of value that will be set
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_s_param(qcarcam_hndl_t hndl, qcarcam_param_t param, const qcarcam_param_value_t* p_value)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_s_param(hndl, param, p_value);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_s_buffers
///
/// @brief Set buffers
///
/// @param hndl       Handle of input
/// @param p_buffers  Pointer to set buffers structure
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_s_buffers(qcarcam_hndl_t hndl, qcarcam_buffers_t* p_buffers)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_s_buffers(hndl, p_buffers);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_start
///
/// @brief Start input
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_start(qcarcam_hndl_t hndl)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_start(hndl);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_stop
///
/// @brief Stop input that was started
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_stop(qcarcam_hndl_t hndl)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_stop(hndl);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_pause
///
/// @brief Pause input that was started. Does not relinquish resource
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_pause(qcarcam_hndl_t hndl)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_pause(hndl);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_resume
///
/// @brief Resumes input that was paused
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_resume(qcarcam_hndl_t hndl)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_resume(hndl);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_get_frame
///
/// @brief Get available frame
///
/// @param hndl          Handle of input
/// @param p_frame_info  Pointer to frame information that will be filled
/// @param timeout       Max wait time in ns for frame to be available before timeout
/// @param flags         Flags
///
/// @return CAMERA_SUCCESS if successful; CAMERA_EEXPIRED if timeout
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_get_frame(qcarcam_hndl_t hndl, qcarcam_frame_info_t* p_frame_info,
        unsigned long long int timeout, unsigned int flags)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_get_frame(hndl, p_frame_info, timeout, flags);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_release_frame
///
/// @brief Re-enqueue frame buffers
///
/// @param hndl       Handle of input
/// @param idx        Index into the qcarcam_buffers_t buffers table to reenqueue
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
AIS_API CameraResult ais_release_frame(qcarcam_hndl_t hndl, unsigned int idx)
{
    CameraResult result = CAMERA_EBADSTATE;
    AisEngine* pEngine = AisEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_release_frame(hndl, idx);
    }

    return result;
}
