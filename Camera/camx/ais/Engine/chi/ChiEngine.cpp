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

#include "ChiEngine.h"
#include "realtime_pipeline_test.h"
#include "ChiBufferManager.h"

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

#define FPS_FRAME_COUNT 330
//////////////////////////////////////////////////////////////////////////////////
/// FORWARD DECLARE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////
static void ais_usr_context_destroy(ChiUsrCtxt* pUsrCtxt);

//////////////////////////////////////////////////////////////////////////////////
/// GLOBAL VARIABLES
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
/// FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////


/**
 * Deinitialize
 *
 * @brief Deinitialize AIS context & frees resources
 *
 * @return n/a
 */
void ChiEngine::Deinitialize(void)
{
    int i;
    CameraResult rc = CAMERA_SUCCESS;

    //Signal Chi Handler
    CameraSetSignal(m_eventChiHandlerSignal);

    if (m_eventChiHandlerThreadId)
    {
        CameraJoinThread(m_eventChiHandlerThreadId, NULL);
        CameraReleaseThread(m_eventChiHandlerThreadId);
    }

    CameraDestroySignal(m_eventChiHandlerSignal);
    m_eventChiHandlerSignal = NULL;

    for (i = 0; i < CHI_ENGINE_QUEUE_MAX; i++)
    {
        if (m_eventQ[i])
        {
            CameraQueueDestroy(m_eventQ[i]);
        }
    }

    if (m_pChiModule)
    {
        m_pChiModule->DestroyInstance();
    }

    CameraDestroyMutex(m_mutex);
    CameraDestroyMutex(m_usrCtxtMapMutex);

    m_isInitialized = FALSE;
}

ChiEngine::ChiEngine()
{
    time_t t;
    srand((unsigned) time(&t));

    m_magic = AIS_CTXT_MAGIC;

    std_memset(m_usrCtxtMap, 0x0, sizeof(m_usrCtxtMap));
    m_clientCount=0;
    m_isInitialized = FALSE;
}

ChiEngine::~ChiEngine()
{

}

CameraResult ChiEngine::Initialize(void)
{
    CameraResult rc = CAMERA_SUCCESS;
    int i;

    rc = CameraCreateMutex(&m_mutex);

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
        for (i = 0; i < CHI_ENGINE_QUEUE_MAX; i++)
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
                    ChiEngine::EventHandler,
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
        m_pChiModule = ChiModule::GetInstance();
        if (nullptr == m_pChiModule)
        {
            rc = CAMERA_EMEMPTR;
        }

        //m_realtimePipe = new RealtimePipelineTest;
        //m_realtimePipe->TestRealtimePipeline(RealtimePipelineTest::TestIFERDI0RawPlain16, 1);
    }


    if (CAMERA_SUCCESS == rc)
    {
        rc = CameraCreateSignal(&m_eventChiHandlerSignal);
        CAM_MSG_ON_ERR(rc, "Failed to create Chi signal: %d", rc);
    }

    if (CAMERA_SUCCESS == rc)
    {
        char name[64];
        snprintf(name, sizeof(name), "engine_chi_evnt_hndlr");

        if (0 !=  CameraCreateThread(CAMERA_THREAD_PRIO_HIGH_REALTIME,
                0,
                ChiEngine::EventChiHandler,
                this,
                0x8000,
                name,
                &m_eventChiHandlerThreadId))
        {
            CAM_MSG(ERROR, "CameraCreateThread failed");
            rc = CAMERA_EFAILED;
        }
    }


    rc = PowerSuspend();
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
CameraResult ChiEngine::ais_initialize(qcarcam_init_t* p_init_params)
{
    CameraResult rc = CAMERA_SUCCESS;

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
CameraResult ChiEngine::ais_uninitialize(void)
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
 * ais_engine_power_suspend
 *
 * @brief Goes into low power mode
 *
 * @return CameraResult
 */
CameraResult ChiEngine::PowerSuspend(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    return rc;
}

/**
 * ais_engine_power_resume
 *
 * @brief Power back up
 *
 * @return CameraResult
 */
CameraResult ChiEngine::PowerResume(void)
{
    CameraResult rc = CAMERA_SUCCESS;

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
uintptr_t ChiEngine::AssignNewHndl(ChiUsrCtxt* usr_ctxt)
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
 * ais_release_hndl
 *
 * @brief Frees index in mapping table used by user_hndl
 *
 * @note: user_hndl is assumed to be validated through ais_get_user_context
 *
 * @param user_hndl
 *
 * @return n/a
 */
void ChiEngine::ReleaseHndl(void* user_hndl)
{
    uint32 idx;
    uintptr_t hndl = (uintptr_t)user_hndl;

    idx = USR_IDX_FROM_HNDL(hndl);

    if (idx < MAX_USR_CONTEXTS)
    {
        ChiUsrCtxt* usr_ctxt = NULL;

        CameraLockMutex(m_usrCtxtMapMutex);

        m_usrCtxtMap[idx].refcount--;

        if (0 == m_usrCtxtMap[idx].refcount)
        {
            usr_ctxt = (ChiUsrCtxt*)m_usrCtxtMap[idx].usr_ctxt;
            memset(&m_usrCtxtMap[idx], 0x0, sizeof(m_usrCtxtMap[idx]));
        }
        else
        {
            m_usrCtxtMap[idx].in_use = AIS_USR_CTXT_MAP_DESTROY;
        }

        CameraUnlockMutex(m_usrCtxtMapMutex);

        if (usr_ctxt)
        {
            ais_usr_context_destroy(usr_ctxt);
        }
    }
}

/**
 * ais_get_user_context
 *
 * @brief Get user context pointer from handle
 *
 * @param user_hndl
 *
 * @return AIS_USR_CTXT*  - NULL if fail
 */
ChiUsrCtxt* ChiEngine::GetUsrCtxt(void* user_hndl)
{
    uint32 idx;
    uintptr_t hndl = (uintptr_t)user_hndl;
    if (!IsInitialized())
    {
        CAM_MSG(ERROR, "AIS not initialized");
        return NULL;
    }
    else if (!CHECK_USR_CTXT_MAGIC(hndl))
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
        return (ChiUsrCtxt*)m_usrCtxtMap[idx].usr_ctxt;
    }
    else
    {
        CAM_MSG(ERROR, "invalid hndl ptr %p", hndl);
    }

    CameraUnlockMutex(m_usrCtxtMapMutex);

    return NULL;
}

/**
 * ais_put_user_context
 *
 * @brief relinquishes user ctxt
 *
 * @param usr_ctxt
 *
 * @return none
 */
void ChiEngine::PutUsrCtxt(ChiUsrCtxt* usr_ctxt)
{
    RelinquishUsrCtxt(usr_ctxt->m_mapIdx);
}

/**
 * ais_acquire_user_context
 *
 * @brief Increments refcount of usr_ctxt handle if handle is in use
 *
 * @param idx
 *
 * @return AIS_USR_CTXT*  - NULL if fail
 */
ChiUsrCtxt* ChiEngine::AcquireUsrCtxt(unsigned int idx)
{
    if (idx < MAX_USR_CONTEXTS)
    {
        CameraLockMutex(m_usrCtxtMapMutex);
        if(AIS_USR_CTXT_MAP_USED == m_usrCtxtMap[idx].in_use)
        {
            m_usrCtxtMap[idx].refcount++;
            CameraUnlockMutex(m_usrCtxtMapMutex);

            return (ChiUsrCtxt*)m_usrCtxtMap[idx].usr_ctxt;
        }
        CameraUnlockMutex(m_usrCtxtMapMutex);
    }

    return NULL;
}

/**
 * ais_relinquish_user_context
 *
 * @brief Decrement refcount and cleanup if handle is being closed and last user
 *
 * @param idx
 *
 * @return none
 */
void ChiEngine::RelinquishUsrCtxt(unsigned int idx)
{
    if (idx < MAX_USR_CONTEXTS)
    {
        ChiUsrCtxt* usr_ctxt = NULL;

        CameraLockMutex(m_usrCtxtMapMutex);

        if (AIS_USR_CTXT_MAP_UNUSED != m_usrCtxtMap[idx].in_use)
        {
            m_usrCtxtMap[idx].refcount--;

            if (AIS_USR_CTXT_MAP_DESTROY == m_usrCtxtMap[idx].in_use &&
                !m_usrCtxtMap[idx].refcount)
            {
                m_usrCtxtMap[idx].in_use = AIS_USR_CTXT_MAP_UNUSED;
                usr_ctxt = (ChiUsrCtxt*)m_usrCtxtMap[idx].usr_ctxt;
                m_usrCtxtMap[idx].usr_ctxt = NULL;
            }
        }

        CameraUnlockMutex(m_usrCtxtMapMutex);

        if (usr_ctxt)
        {
            ais_usr_context_destroy(usr_ctxt);
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
    return TRUE;
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
CameraResult ChiUsrCtxt::InitMutex(void)
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
CameraResult ChiUsrCtxt::DeinitMutex(void)
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
 * @param input_id - qcarcam input id
 *
 * @return AIS_USR_CTXT* - NULL if fail
 */
ChiUsrCtxt* ChiUsrCtxt::CreateUsrCtxt(qcarcam_input_desc_t input_id)
{
    ChiUsrCtxt* pUsrCtxt = new ChiUsrCtxt;

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

            pUsrCtxt->m_pipelineId = RealtimePipelineTest::TestIFERDI0RawPlain16 + (int)input_id;
        }
        else
        {
            CAM_MSG(ERROR, "Failed to create buffer done Q");
            delete pUsrCtxt;
        }
    }
    else
    {
        CAM_MSG(ERROR, "Failed to allocate handle");
    }

    return pUsrCtxt;
}

/**
 * ais_usr_context_destroy
 *
 * @brief Destroy user context
 *
 * @param usr_ctxt
 *
 * @return n/a
 */
static void ais_usr_context_destroy(ChiUsrCtxt* pUsrCtxt)
{
    CameraResult result;

    CAM_MSG(HIGH, "%s %p", __func__, pUsrCtxt);

    result = CameraQueueDestroy(pUsrCtxt->m_bufferDoneQ);
    CAM_MSG_ON_ERR((CAMERA_SUCCESS != result), " CameraQueueDestroy Failed usrctx %p result=%d", pUsrCtxt, result);

    pUsrCtxt->DeinitMutex();
    CAM_MSG_ON_ERR((CAMERA_SUCCESS != result), " deinit_cond_mutex Failed usrctx %p result=%d", pUsrCtxt, result);

    delete pUsrCtxt;
}

/**
 * ChiUsrCtxt::Reserve
 *
 * @brief Reserve HW pipeline resources for user context
 *
 * @param usr_ctxt
 *
 * @return CameraResult
 */
CameraResult ChiUsrCtxt::Reserve(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    CAM_MSG(HIGH, "%s current state m_state %d",__func__, m_state);


    if (AIS_USR_STATE_OPENED == m_state)
    {

         m_state = AIS_USR_STATE_RESERVED;
    }

    return rc;
}

/**
 * ChiUsrCtxt::Release
 *
 * @brief Release HW pipeline resources for user context
 *
 * @param usr_ctxt
 *
 * @return CameraResult
 */
CameraResult ChiUsrCtxt::Release(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (AIS_USR_STATE_RESERVED == m_state)
    {
        m_state = AIS_USR_STATE_OPENED;
    }

    return rc;
}

/**
 * ChiUsrCtxt::Start
 *
 * @brief Start HW pipeline resources for user context
 *
 * @param usr_ctxt
 *
 * @return CameraResult
 */
CameraResult ChiUsrCtxt::Start(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    CAM_MSG(HIGH, "%s %d m_state", __func__, m_state);
    if (AIS_USR_STATE_RESERVED == m_state)
    {
        /*if success then we are buffer pending state*/
        if (CAMERA_SUCCESS == rc)
        {
            m_state = AIS_USR_STATE_STREAMING;
            CAM_MSG(HIGH, "streaming m_state %d", m_state);

        }
    }

    return rc;
}

/**
 * ChiUsrCtxt::Stop
 *
 * @brief Stop HW pipeline resources for user context
 *
 * @return CameraResult
 */
CameraResult ChiUsrCtxt::Stop(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (AIS_USR_STATE_RESERVED != m_state)
    {
        m_state = AIS_USR_STATE_RESERVED;
    }

    return rc;
}

/**
 * ChiUsrCtxt::Pause
 *
 * @brief Pause HW pipeline resources for user context.
 *        For now we only stop IFE.
 *
 * @return CameraResult
 */
CameraResult ChiUsrCtxt::Pause(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (AIS_USR_STATE_STREAMING == m_state)
    {
        m_state = AIS_USR_STATE_PAUSED;
    }

    return rc;
}

/**
 * ChiUsrCtxt::Resume
 *
 * @brief Resume HW pipeline resources for user context.
 *        For now we only restart IFE.
 *
 * @param usr_ctxt
 *
 * @return CameraResult
 */
CameraResult ChiUsrCtxt::Resume(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (AIS_USR_STATE_PAUSED == m_state)
    {
        m_state = AIS_USR_STATE_STREAMING;
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
CameraResult ChiEngine::ais_query_inputs(qcarcam_input_t* p_inputs, unsigned int size, unsigned int* ret_size)
{
    AIS_API_ENTER();

    if (!ret_size)
    {
        CAM_MSG(ERROR, "%s - bad ptr", __func__);
        return CAMERA_EMEMPTR;
    }

    int qcarcam_version_server = (QCARCAM_VERSION_MAJOR << 8) | (QCARCAM_VERSION_MINOR);
    uint32 modes;
    int dst_idx = m_pChiModule->GetNumberCameras();

    if (!p_inputs)
    {
        *ret_size = dst_idx;
        return CAMERA_SUCCESS;
    }

    const CHICAMERAINFO* pCameraInfo;
    CHISENSORMODEINFO* pSensorModeInfo;
    //Hard coded mapping - TODO match this to camera config
    // camera 0 - mode 10 ->  qcarcam id 0
    // camera 1 - mode 0 ->  qcarcam id 1
    for (int idx = 0; idx < dst_idx; idx++)
    {
        pCameraInfo = m_pChiModule->GetCameraInfo(idx);

        CAM_MSG(HIGH, "cam idx %d nummodes %d", idx, pCameraInfo->numSensorModes);

        for (modes = 0; modes < 1 /*pCameraInfo->numSensorModes*/; modes++)
        {
            pSensorModeInfo = m_pChiModule->GetCameraSensorModeInfo(idx, modes);

            p_inputs[idx].desc          = ( qcarcam_input_desc_t)idx;
            p_inputs[idx].res[modes].width  = pSensorModeInfo->frameDimension.width;
            p_inputs[idx].res[modes].height = pSensorModeInfo->frameDimension.height;
            p_inputs[idx].color_fmt[modes]  = QCARCAM_FMT_UYVY_8;   //Assume colour format is uyvy 8bit Need to update
        }
        p_inputs[idx].num_color_fmt = modes;
        p_inputs[idx].num_res       = modes;
        p_inputs[idx].flags         = qcarcam_version_server;
    }

    CAM_MSG(MEDIUM, "filled_size=%d", dst_idx);
    *ret_size = dst_idx;

    return CAMERA_SUCCESS;
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
qcarcam_hndl_t ChiEngine::ais_open(qcarcam_input_desc_t desc)
{
    qcarcam_hndl_t qcarcam_hndl = NULL;

    AIS_API_ENTER();

    if (ais_is_valid_input(desc))
    {
        CAM_MSG(LOW, "%s input id %d", __func__, desc);
        ChiUsrCtxt* pUsrCtxt = ChiUsrCtxt::CreateUsrCtxt(desc);
        if (pUsrCtxt)
        {
            qcarcam_hndl = (qcarcam_hndl_t)AssignNewHndl(pUsrCtxt);
            if (qcarcam_hndl)
            {
                CAM_MSG(LOW, "new handle %p for usrctxt %p input %d", qcarcam_hndl, pUsrCtxt, desc);

                pUsrCtxt->m_qcarcamHndl = qcarcam_hndl;
                pUsrCtxt->m_state = AIS_USR_STATE_OPENED;

                CAM_MSG(HIGH, "%s TEHO input id %d usrctxt id %d", __func__, desc, pUsrCtxt->m_inputId);

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
                ais_usr_context_destroy(pUsrCtxt);
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
CameraResult ChiEngine::ais_close(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiUsrCtxt* pUsrCtxt;

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

        pUsrCtxt->m_state = AIS_USR_STATE_UNINITIALIZED;

        CameraUnlockMutex(pUsrCtxt->m_mutex);

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
CameraResult ChiEngine::ais_g_param(qcarcam_hndl_t hndl, qcarcam_param_t param, qcarcam_param_value_t* p_value)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        if (!p_value)
        {
            CAM_MSG(ERROR, "%s null pointer for param %d", __func__, param);
            rc = CAMERA_EMEMPTR;
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
CameraResult ChiEngine::ais_s_param(qcarcam_hndl_t hndl, qcarcam_param_t param, const qcarcam_param_value_t* p_value)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiUsrCtxt* pUsrCtxt;

    AIS_API_ENTER_HNDL(hndl);

    pUsrCtxt = GetUsrCtxt(hndl);
    if (pUsrCtxt)
    {
        if (!p_value)
        {
            CAM_MSG(ERROR, "%s null pointer for param %d", __func__, param);
            rc = CAMERA_EMEMPTR;
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
CameraResult ChiEngine::ais_s_buffers(qcarcam_hndl_t hndl, qcarcam_buffers_t* p_buffers)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiUsrCtxt* pUsrCtxt;

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
            AisBufferList* p_buffer_list;

            p_buffer_list = &pUsrCtxt->m_bufferList[AIS_USER_OUTPUT_IDX];
            if (p_buffer_list->m_nBuffers)
            {
                CAM_MSG(HIGH, "ais_s_buffers usrctxt %p unmap buffers", pUsrCtxt);
                rc = ChiBufferManager::UnmapBuffers(pUsrCtxt, p_buffer_list);
            }

            rc = ChiBufferManager::MapBuffers(pUsrCtxt, p_buffers, p_buffer_list);

            //Now register the buffers to the CHI context
            mapBuffers(pUsrCtxt, p_buffer_list);
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

CameraResult ChiEngine::submitBuffers(ChiUsrCtxt* usr_ctxt, int numBufs)
{
    CameraResult rc = CAMERA_SUCCESS;

    CAM_MSG(HIGH, "submit realtime pipeline request");

    ais_engine_event_msg_t msg;

    STD_ZEROAT(&msg);
    msg.event_id = AIS_EVENT_SUBMIT_BUFFER;
    msg.payload.submit_buf.chiCamIdx = usr_ctxt->m_inputId;

    CAM_MSG(HIGH, "TEHO submitBuffers camIdx %d", msg.payload.submit_buf.chiCamIdx);

    for (int i = 0; i < numBufs; i++)
    {
        msg.payload.submit_buf.idx = i;
        CAM_MSG(HIGH, "AIS_EVENT_SUBMIT_BUF %d ", msg.payload.submit_buf.idx);
        //Submit a capture request
        QueueChiEvent(&msg);
    }
    return rc;
}


CameraResult ChiEngine::submitBufferIdx(ChiUsrCtxt* usr_ctxt, int idx)
{
    CameraResult rc = CAMERA_SUCCESS;

    CAM_MSG(HIGH, "submitBufferIdx usrCtxt->m_inputId %d", usr_ctxt->m_inputId);

    ais_engine_event_msg_t msg;

    STD_ZEROAT(&msg);
    msg.event_id = AIS_EVENT_SUBMIT_BUFFER;

    msg.payload.submit_buf.chiCamIdx = usr_ctxt->m_inputId;
    msg.payload.submit_buf.idx = idx;
    CAM_MSG(HIGH, "AIS_EVENT_SUBMIT_BUF %d ", msg.payload.submit_buf.idx);
    //Submit a capture request
    QueueChiEvent(&msg);

    return rc;
}

CameraResult ChiEngine::mapBuffers(ChiUsrCtxt* usr_ctxt, AisBufferList* p_buffer_list)
{
    CameraResult rc = CAMERA_SUCCESS;

    CAM_MSG(HIGH, "TEHO Chi mapbuffers usrCtxt->m_inputId %d", usr_ctxt->m_inputId);

    ais_engine_event_msg_t msg;

    STD_ZEROAT(&msg);
    msg.event_id = AIS_EVENT_MAP_BUFFERS;
    msg.payload.map_buf.chiCamIdx = usr_ctxt->m_inputId;
    msg.payload.map_buf.pBufferList = p_buffer_list;

    //Submit to chi thread
    QueueChiEvent(&msg);

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
CameraResult ChiEngine::ais_start(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiUsrCtxt* pUsrCtxt;

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
                CameraResult rc = CAMERA_SUCCESS;

                rc = pUsrCtxt->Start();

                submitBuffers(pUsrCtxt, p_buffer_list->m_nBuffers);

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
CameraResult ChiEngine::ais_stop(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiUsrCtxt* pUsrCtxt;

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
CameraResult ChiEngine::ais_pause(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiUsrCtxt* pUsrCtxt;

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
CameraResult ChiEngine::ais_resume(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiUsrCtxt* pUsrCtxt;

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
CameraResult ChiEngine::ais_get_frame(qcarcam_hndl_t hndl,
        qcarcam_frame_info_t* p_frame_info,
        unsigned long long int timeout,
        unsigned int flags)
{
    CameraResult rc = CAMERA_SUCCESS;
    struct timespec t;
    struct timespec to;
    ChiUsrCtxt* pUsrCtxt;

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
CameraResult ChiEngine::ais_release_frame(qcarcam_hndl_t hndl, unsigned int idx)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiUsrCtxt* pUsrCtxt;

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

                rc = submitBufferIdx(pUsrCtxt, idx);

                if( rc != CAMERA_SUCCESS)
                {
                    CAM_MSG(ERROR, "%s queue_buffer fail %u", __func__, idx);
                }
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
 * @note: Caller must call ais_put_user_context() on returned context when done
 *
 * @param match_func  match function
 * @param match       match function argument

 * @return AIS_USR_CTXT*     NULL if not found
 */
ChiUsrCtxt* ChiEngine::FindUsrCtxt(ais_usr_ctxt_match_func match_func, void* match)
{
    uint32 idx;
    ChiUsrCtxt* pUsrCtxt;

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
 * @param match_func match function
 * @param match match data parameter
 * @param op_func operate function
 * @param user_data operate function parameter
 *
 * @return boolean TRUE if callback success
 */
uint32 ChiEngine::TraverseUsrCtxt(ais_usr_ctxt_match_func match_func, void* match,
        ais_usr_ctxt_operate_func op_func, void* user_data)
{
    uint32 idx;
    boolean ret = FALSE;
    ChiUsrCtxt* pUsrCtxt;

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
 * ais_usr_ctxt_event_frame_done
 *
 * @brief Handles frame done event of a user context
 *
 * @param usr_ctxt
 * @param ais_frame_done
 * @return CameraResult  CAMERA_SUCCESS on success
 */
CameraResult ChiEngine::UsrCtxtEventFrameDone(ChiUsrCtxt* pUsrCtxt, ais_frame_done_t* ais_frame_done)
{
    CameraResult result = CAMERA_SUCCESS;
    AisBufferList* p_buffer_list = &pUsrCtxt->m_bufferList[AIS_USER_OUTPUT_IDX];
    uint32 buffer_done_q_length;
    int rc = EOK;

    CameraLockMutex(pUsrCtxt->m_mutex);

    //context must be streaming
    if (pUsrCtxt->m_state != AIS_USR_STATE_STREAMING)
    {
        CAM_MSG(HIGH, "m_pipelineId %d context %p in bad state %d",
            pUsrCtxt->m_pipelineId, pUsrCtxt, pUsrCtxt->m_state);
        CameraUnlockMutex(pUsrCtxt->m_mutex);
        return CAMERA_EBADSTATE;
    }

    if (pUsrCtxt->m_isPendingStart)
    {
        CAM_MSG(RESERVED, "m_pipelineId %d First Frame buffer %d",
                ais_frame_done->pipeline_id, ais_frame_done->frame_info.idx);
        pUsrCtxt->m_isPendingStart = FALSE;
    }


//TEHO  NEED TO FIX THE TIMESTAMPS!!! IT'S returning 0 for all frames
#if 0
    if( ais_frame_done->frame_info.timestamp <= pUsrCtxt->m_startTime )
    {
        CAM_MSG(HIGH,  "frame timestamp <= usr_ctxt->starttime");
        CameraUnlockMutex(pUsrCtxt->m_mutex);
        return CAMERA_EFAILED;
    }
#endif

    pthread_mutex_lock(&pUsrCtxt->m_bufferDoneQMutex);

#if 0
    /*Check latency and reduce if needed*/
    CameraQueueGetLength(pUsrCtxt->m_bufferDoneQ, &buffer_done_q_length);
    if (buffer_done_q_length > pUsrCtxt->m_usrSettings.n_latency_max)
    {
        uint32 i;

        for (i = 0; i<pUsrCtxt->m_usrSettings.n_latency_reduce_rate; i++)
        {
            qcarcam_frame_info_t frame_info;
            result = CameraQueueDequeue(pUsrCtxt->m_bufferDoneQ, (CameraQueueDataType)&frame_info);
            if (CAMERA_SUCCESS == result)
            {
                //TODO: Re-enqueue buffers here
                SubmitBuffer();
                if (CAMERA_SUCCESS == result)
                {
                    CAM_MSG(HIGH, "ctxt %p requeued frame %d to IFE", pUsrCtxt, frame_info.idx);
                }
                else
                {
                    CAM_MSG(ERROR, "ctxt %p failed to requeue frame %d to IFE", pUsrCtxt, frame_info.idx);
                }
            }
            else
            {
                CAM_MSG(ERROR, "ctxt %p failed to dequeue frame to requeue", pUsrCtxt);
            }
        }
    }
#endif

    /*Queue new frame and signal condition*/
    result = CameraQueueEnqueue(pUsrCtxt->m_bufferDoneQ, (CameraQueueDataType)&(ais_frame_done->frame_info));

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
 * ais_usr_ctxt_match_ife_frame_done
 *
 * @brief Matches user context with ife frame done event
 *
 * @param usr_ctxt
 * @param match  (ais_frame_done_t*)
 * @return boolean TRUE if match
 */
boolean ChiUsrCtxt::MatchIfeFrameDone(ChiUsrCtxt* pUsrCtxt, void* match)
{
    CAM_MSG(HIGH, "usr_ctxt %p pipeline id %d", pUsrCtxt, pUsrCtxt->m_pipelineId);

    return ((AIS_USR_STATE_STREAMING == pUsrCtxt->m_state) &&
            (pUsrCtxt->m_pipelineId == ((ais_frame_done_t*)match)->pipeline_id));
}

/**
 * ais_engine_queue_event
 *
 * @brief Queues events for engine
 *
 * @param event msg
 *
 * @return CameraResult
 */
CameraResult ChiEngine::QueueEvent(ais_engine_event_msg_t* pMsg)
{
    CameraResult result;

    CAM_MSG(LOW, "q_event %d", pMsg->event_id);

    result = CameraQueueEnqueue(m_eventQ[CHI_ENGINE_QUEUE_EVENTS], pMsg);
    if (result == CAMERA_SUCCESS)
    {
        result = CameraSetSignal(m_eventHandlerSignal);
    }

    return result;
}

/**
 * ais_engine_process_event
 *
 * @brief Dequeues event from event Q and processes it
 *
 * @param msg
 *
 * @return int
 */
int ChiEngine::ProcessEvent(ais_engine_event_msg_t* pMsg)
{
    CameraResult result;

    result = CameraQueueDequeue(m_eventQ[CHI_ENGINE_QUEUE_EVENTS], pMsg);
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
    case AIS_EVENT_FRAME_DONE:
    {
        ChiUsrCtxt* pUsrCtxt;
        ais_frame_done_t* ais_frame_done = &pMsg->payload.ife_frame_done;
        pUsrCtxt = FindUsrCtxt(ChiUsrCtxt::MatchIfeFrameDone, ais_frame_done);
        if (pUsrCtxt)
        {
            CAM_MSG(MEDIUM, "frame done: %llu, %d", ais_frame_done->frame_info.timestamp/1000, ais_frame_done->frame_info.seq_no);
            UsrCtxtEventFrameDone(pUsrCtxt, ais_frame_done);
            PutUsrCtxt(pUsrCtxt);
        }
        else
        {
            CAM_MSG(ERROR, "Can't find user context for frame done of pipeline_id %d",
                    ais_frame_done->pipeline_id);
            //Todo: cleanup?
        }
        break;
    }
    default:
        CAM_MSG(ALWAYS, "Unknown message id %d", pMsg->event_id);
        break;
    }

    return 1;
}

/**
 * ais_engine_event_handler
 *
 * @brief Engine event handler thread to process engine events
 *
 * @param Engine
 *
 * @return int
 */
int ChiEngine::EventHandler(void *arg)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiEngine* pEngine = (ChiEngine*)arg;

    if (pEngine)
    {
        ais_engine_event_msg_t* msg = (ais_engine_event_msg_t*)CameraAllocate(CAMERA_ALLOCATE_ID_UNASSIGNED, sizeof(ais_engine_event_msg_t));
        if (msg)
        {
            while (!pEngine->m_eventHandlerIsExit)
            {
                // track if there is anything processed in this iteration
                int serviced = 0;

                CAM_MSG(LOW, "%s: is awake; ready to work.",
                    __FUNCTION__);

                //
                // service IFE callback, parse the msg, and put filled buffers in filledQ.
                //
                serviced += pEngine->ProcessEvent(msg);

                //
                // If we woke up with nothing to do, we'll try to sleep again.
                //
                //if (!serviced)
                {
                    CAM_MSG(LOW, "%s: has nothing to do, going to sleep", __FUNCTION__);
                    CameraWaitOnSignal(pEngine->m_eventHandlerSignal, CAM_SIGNAL_WAIT_NO_TIMEOUT);
                }
            }

            CameraFree(CAMERA_ALLOCATE_ID_UNASSIGNED, msg);
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


/**
 * QueueChiEvent
 *
 * @brief Queues events for engine
 *
 * @param msg
 *
 * @return CameraResult
 */
CameraResult ChiEngine::QueueChiEvent(ais_engine_event_msg_t* msg)
{
    CameraResult result;

    CAM_MSG(LOW, "q_event %d", msg->event_id);

    result = CameraQueueEnqueue(m_eventQ[CHI_ENGINE_QUEUE_CHI_EVENTS], msg);
    if (result == CAMERA_SUCCESS)
    {
        CameraSetSignal(m_eventChiHandlerSignal);
    }

    return result;
}

int ChiEngine::ProcessChiEvent(ais_engine_event_msg_t* msg)
{
    CameraResult result;

    result = CameraQueueDequeue(m_eventQ[CHI_ENGINE_QUEUE_CHI_EVENTS], msg);
    if (CAMERA_SUCCESS != result)
    {
        if (CAMERA_ENOMORE != result)
        {
            CAM_MSG(ERROR, "Failed to dequeue event (%d)", result);
        }
        return 0;
    }

    switch (msg->event_id)
    {
    case AIS_EVENT_SUBMIT_BUFFER:
    {
        ais_chi_submitbuff_t* submit_buf = &msg->payload.submit_buf;
        uint32 chiCamIdx = submit_buf->chiCamIdx;
        if (chiCamIdx < CHI_MAX_NUM_CAM)
        {
            CAM_MSG(ERROR, "submit frame idx (%d) camIdx %d", submit_buf->idx, chiCamIdx);
            m_realtimePipe[chiCamIdx]->SubmitBuffer(submit_buf->idx);
        }
        else
        {
            CAM_MSG(ERROR, "AIS_EVENT_MAP_BUFFERS invalid camidx (%d)", chiCamIdx);
            return 0;
        }
        break;
    }
    case AIS_EVENT_MAP_BUFFERS:
    {
        ais_chi_map_buff_t* mapBuffer = &msg->payload.map_buf;
        uint32 chiCamIdx = mapBuffer->chiCamIdx;
        if (chiCamIdx < CHI_MAX_NUM_CAM)
        {
            CAM_MSG(ERROR, "mapbuffer pBufferlist(%p) camIdx %u", mapBuffer->pBufferList, chiCamIdx);
            m_realtimePipe[chiCamIdx]->PrepareRDIBuffers(mapBuffer->pBufferList);
        }
        else
        {
            CAM_MSG(ERROR, "AIS_EVENT_MAP_BUFFERS invalid camidx (%d)", chiCamIdx);
            return 0;
        }
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
 * @param Engine
 *
 * @return int
 */
int ChiEngine::EventChiHandler(void *arg)
{
    CameraResult rc = CAMERA_SUCCESS;
    ChiEngine* pEngine = (ChiEngine*)arg;

    if (pEngine)
    {
        pEngine->m_realtimePipe[0] = new RealtimePipelineTest;
        pEngine->m_realtimePipe[0]->TestRealtimePipeline(RealtimePipelineTest::TestIFERDI0RawPlain16, 1, 0);

        pEngine->m_realtimePipe[1] = new RealtimePipelineTest;
        pEngine->m_realtimePipe[1]->TestRealtimePipeline(RealtimePipelineTest::TestIFERDI1RawPlain16, 1, 1);

        pEngine->m_realtimePipe[2] = new RealtimePipelineTest;
        pEngine->m_realtimePipe[2]->TestRealtimePipeline(RealtimePipelineTest::TestIFERDI2RawPlain16, 1, 2);

        pEngine->m_realtimePipe[3] = new RealtimePipelineTest;
        pEngine->m_realtimePipe[3]->TestRealtimePipeline(RealtimePipelineTest::TestIFERDI3RawPlain16, 1, 3);

        CAM_MSG(HIGH, "%s: TEHO testrealtime pipeline",
                    __FUNCTION__);

        ais_engine_event_msg_t* msg = (ais_engine_event_msg_t*)CameraAllocate(CAMERA_ALLOCATE_ID_UNASSIGNED, sizeof(ais_engine_event_msg_t));
        if (msg)
        {
            while (!pEngine->m_eventHandlerIsExit)
            {
                // track if there is anything processed in this iteration
                int serviced = 0;

                CAM_MSG(LOW, "%s: is awake; ready to process chi event work.",
                    __FUNCTION__);

                //
                // service IFE callback, parse the msg, and put filled buffers in filledQ.
                //
                serviced += pEngine->ProcessChiEvent(msg);

                //
                // If we woke up with nothing to do, we'll try to sleep again.
                //
                //if (!serviced)
                {
                    CAM_MSG(LOW, "%s: has nothing to do, going to sleep", __FUNCTION__);
                    CameraWaitOnSignal(pEngine->m_eventChiHandlerSignal, CAM_SIGNAL_WAIT_NO_TIMEOUT);
                }
            }

            CameraFree(CAMERA_ALLOCATE_ID_UNASSIGNED, msg);
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


///@brief ChiEngine singleton
ChiEngine* ChiEngine::m_pEngineInstance = nullptr;

/***************************************************************************************************************************
*   ChiEngine::CreateInstance
*
*   @brief
*       Create singleton instance for AisEngine
***************************************************************************************************************************/
ChiEngine* ChiEngine::CreateInstance()
{
    if(m_pEngineInstance == nullptr)
    {
        m_pEngineInstance = new AisEngine();
    }

    return m_pEngineInstance;
}

/***************************************************************************************************************************
*   ChiEngine::GetInstance
*
*   @brief
*       Gets the singleton instance for ChiEngine
***************************************************************************************************************************/
ChiEngine* ChiEngine::GetInstance()
{
    return m_pEngineInstance;
}

/***************************************************************************************************************************
*   ChiEngine::DestroyInstance
*
*   @brief
*       Destroy the singleton instance of the ChiEngine class
*
*   @return
*       void
***************************************************************************************************************************/
void ChiEngine::DestroyInstance()
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
    ChiEngine* pEngine = ChiEngine::GetInstance();

    if (pEngine)
    {
        CAM_MSG(ERROR, "ChiEngine already initialized");
        return CAMERA_EBADSTATE;
    }

    pEngine = ChiEngine::CreateInstance();
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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

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
    ChiEngine* pEngine = ChiEngine::GetInstance();

    if (pEngine)
    {
        result = pEngine->ais_release_frame(hndl, idx);
    }

    return result;
}
