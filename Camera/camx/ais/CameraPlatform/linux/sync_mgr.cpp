/**
 * @file sync_mgr.cpp
 *
 * SYNC manager driver interface.
 *
 * Copyright (c) 2009-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include <media/cam_sync.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include "CameraPlatform.h"
#include "sync_mgr_i.h"

SyncManager* SyncManager::m_SyncManagerInstance = nullptr;

/*-----------------------------------------------------------------------------
* Static Function Declarations and Definitions
*---------------------------------------------------------------------------*/
static uint64 VoidPtrToUINT64(
    void* pVoid)
{
    return static_cast<uint64>(reinterpret_cast<size_t>(pVoid));
}
/*-----------------------------------------------------------------------------
* Preprocessor Definitions and Constants
*---------------------------------------------------------------------------*/
#define AIS_SYNC_COMBINE_PAYLOAD(x,y) (x << 32) | y

#define AIS_SYNC_SPLIT_PAYLOAD(x, y, z) \
    y = z & 0xFFFFFFFF;                 \
    x = (z >> 32) & 0xFFFFFFFF;         \

/*===========================================================================
                 DEFINITIONS AND DECLARATIONS FOR MODULE
=========================================================================== */
/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */
/* ---------------------------------------------------------------------------
** Global Object Definitions
** -------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------
** Local Object Definitions
** ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------
** Forward Declarations
** ------------------------------------------------------------------------ */

/* ===========================================================================
=========================================================================== */
CameraResult SyncManager::RegisterCallback(CameraDeviceCallbackType pfnCallback, void *pClientData)
{
    CameraResult result = CAMERA_SUCCESS;

    if (pfnCallback)
    {
        this->pfnCallBack = pfnCallback;
        this->pClientData = pClientData;
    }
    else
    {
        CAM_MSG(ERROR, "SyncManager pfnCallback pointer is NULL");
        result = CAMERA_EBADPARM;
    }

    return result;
}

CameraResult SyncManager::ProcessSyncEvent(void* pData)
{
    CameraResult result = CAMERA_SUCCESS;
    int rc       = -1;
    SyncManager* pSyncMgr = (SyncManager*)pData;
    struct v4l2_event v4l2_event;
    struct cam_sync_ev_header *pEvHeader = NULL;
    int64 mono_timestamp = 0;
    uint64_t* pPayloadData = NULL;
    uint64 payloads[2] = {0};

    CAM_MSG(LOW, "We got a Sync Event !!!");
    memset(&v4l2_event, 0, sizeof(v4l2_event));

    rc = ioctl(pSyncMgr->m_camSyncMgrKmdFd, VIDIOC_DQEVENT, &v4l2_event);
    if (rc >= 0)
    {
        if (CAM_SYNC_V4L_EVENT == v4l2_event.type)
        {
            CAM_MSG(HIGH, "Received a SYNC event");
            pEvHeader = CAM_SYNC_GET_HEADER_PTR(v4l2_event);
            pPayloadData = CAM_SYNC_GET_PAYLOAD_PTR(v4l2_event, uint64_t);

            if (NULL == pEvHeader || NULL == pPayloadData)
            {
                CAM_MSG(ERROR, "Received an sync v4l event with enpty data or payload!!");
                return CAMERA_EFAILED;
            }

            switch(v4l2_event.id)
            {
                case CAM_SYNC_V4L_EVENT_ID_CB_TRIG:
                {
                    CAM_MSG(HIGH, "Received CAM_SYNC_V4L_EVENT_ID_CB_TRIG on sync_obj %d [%d]", pEvHeader->sync_obj,
                        pEvHeader->status);
                    CAM_MSG(HIGH, "Dispatch Payload data0 = %llx", pPayloadData[0]); // IfeId && output path
                    CAM_MSG(HIGH, "Dispatch Payload data1 = %llx", pPayloadData[1]); // BufferIndex

                    AIS_SYNC_SPLIT_PAYLOAD(payloads[0], payloads[1], pPayloadData[0]);

                    pSyncMgr->m_vfeMsg.msgId = VFE_MSG_ID_RAW_FRAME_DONE;
                    pSyncMgr->m_vfeMsg.payload.vfeId = (uint32)payloads[0];
                    (void)CameraGetTime(&pSyncMgr->m_vfeMsg.payload._u.msgRaw.startTime);
                    pSyncMgr->m_vfeMsg.payload._u.msgRaw.endTime =
                        pSyncMgr->m_vfeMsg.payload._u.msgRaw.systemTime =
                        pSyncMgr->m_vfeMsg.payload._u.msgRaw.startTime;

                    pSyncMgr->m_vfeMsg.payload._u.msgRaw.outputPath = (IfeOutputPathType)payloads[1];
                    pSyncMgr->m_vfeMsg.payload._u.msgRaw.frameCounter = ++m_frameCounter[pSyncMgr->m_vfeMsg.payload._u.msgRaw.outputPath]; // Local counter
                    pSyncMgr->m_vfeMsg.payload._u.msgRaw.bufIndex = (uint32)pPayloadData[1];
                    //TODO::Add Output buffer pointer
                    pSyncMgr->m_vfeMsg.payload._u.msgRaw.pBuf = 0;

                    if (pfnCallBack)
                    {
                        result = pfnCallBack(pClientData, (uint32)(m_vfeMsg.msgId), sizeof(m_vfeMsg.payload), (void*)(&(m_vfeMsg.payload)));
                    }
                }
                break;
                default:
                    CAM_MSG(HIGH, "Received Unknown v4l sync event %d", v4l2_event.id);
                    return CAMERA_EFAILED;
            }
        }
        else
        {
            CAM_MSG(ERROR, "Received Unknown v4l event = %d", v4l2_event.type);
            result = CAMERA_EFAILED;
        }
    }

    return result;
}

int SyncManager::SyncMgrEventThreadProc(void* arg)

{
    int pollStatus = -1;
    int exit_thread = 0;
    struct pollfd pollfds;
    CameraResult result = CAMERA_SUCCESS;
    SyncManager* pSyncMgr = (SyncManager*)arg;

    if (NULL == pSyncMgr)
    {
        CAM_MSG(ERROR, "Sync Mgr is null!!");
        return -1;
    }

    pollfds.fd = pSyncMgr->m_camSyncMgrKmdFd;

    while(!exit_thread)
    {
        pollfds.events = POLLPRI;
        pollStatus = poll(&pollfds, 1, -1);
        if(0 < pollStatus)
        {
            result = pSyncMgr->ProcessSyncEvent(pSyncMgr);
        }
        else
        {
            CAM_MSG(ERROR, "poll failed");
        }
    }
    return 0;
}

CameraResult SyncManager::SubscribeEvents(int fd, uint32 id, uint32 type)
{
    CameraResult result = CAMERA_SUCCESS;
    struct v4l2_event_subscription sub = {};
    int returnCode = -1;

    sub.id   = id;
    sub.type = type;

    CAM_MSG(HIGH, "Subscribe events id 0x%x type 0x%x",sub.id,  sub.type);

    returnCode = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

    if (-1 == returnCode)
    {
        CAM_MSG(ERROR, "Subscribe events failed %d", returnCode);
        result = CAMERA_EFAILED;
    }
    return result;
}

CameraResult SyncManager::Init()
{
    CameraResult result = CAMERA_SUCCESS;

    // Fetch sync mgr FD
    m_camSyncMgrKmdFd = CameraPlatformGetFd(AIS_SUBDEV_SYNCMGR, 0);
    if (0 == m_camSyncMgrKmdFd)
    {
        CAM_MSG(ERROR,   "Failed to get the FD for CAM_SYNC_DEVICE_NAME");
        result = CAMERA_EFAILED;
    }

    // Add subscribe sync events of event queue from KMD
    if (CAMERA_SUCCESS == result)
    {
        result = SubscribeEvents(m_camSyncMgrKmdFd, CAM_SYNC_V4L_EVENT_ID_CB_TRIG,
                CAM_SYNC_V4L_EVENT);
        if (CAMERA_SUCCESS != result)
        {
            CAM_MSG(ERROR, "Error sync event subscription failed");
            result = CAMERA_EFAILED;
        }
    }

    //Create sync event thread that polls CAM_SYNC_V4L_EVENT
    if (CAMERA_SUCCESS == result)
    {
        char name[64];
        snprintf(name, sizeof(name), "sync_mgr_event_thread");

        result = CameraCreateThread(CAMERA_THREAD_PRIO_HIGH_REALTIME, 0, SyncManager::SyncMgrEventThreadProc, this, 0x8000, name, &SyncEventThreadHandle);
        if(CAMERA_SUCCESS != result)
        {
            CAM_MSG(ERROR, "Create SyncMgrEventThreadProc thread failed for VFE");
        }
    }

    return result;

}

CameraResult SyncManager::DeInit()
{
    CameraResult result = CAMERA_SUCCESS;

    CameraReleaseThread(SyncEventThreadHandle);

    return result;

}

CameraResult SyncManager::SyncRegisterPayload(
    int32 syncObj,
    uint64 payload0,
    uint64 payload1,
    uint64 payload2)
{
    int                              rc               = 0;
    struct cam_private_ioctl_arg     sync_ioctl       = {};
    struct cam_sync_userpayload_info payload_info     = {};
    uint64                           combined_payload = 0;

    combined_payload = AIS_SYNC_COMBINE_PAYLOAD(payload0, payload2);

    payload_info.sync_obj   = syncObj;
    payload_info.payload[0] = combined_payload;
    payload_info.payload[1] = payload1;

    sync_ioctl.id        = CAM_SYNC_REGISTER_PAYLOAD;
    sync_ioctl.size      = sizeof(payload_info);
    sync_ioctl.ioctl_ptr = VoidPtrToUINT64(&payload_info);

    rc = ioctl(m_camSyncMgrKmdFd,
        CAM_PRIVATE_IOCTL_CMD,
        &sync_ioctl);

    if (rc < 0)
    {
        CAM_MSG(ERROR, "CAM_SYNC_REGISTER_PAYLOAD failed %d", rc);
        return CAMERA_EFAILED;
    }

    return CAMERA_SUCCESS;
}

CameraResult SyncManager::SyncCreate(
    const char* pName,
    int32* pSyncid)
{
    int returnCode;
    struct cam_private_ioctl_arg sync_ioctl  = {};
    struct cam_sync_info         sync_create = {};
    CameraResult                 result      = CAMERA_EFAILED;

    CAM_MSG(HIGH, "CreateSync Enter");

    strlcpy(sync_create.name, pName, 64);

    sync_ioctl.id        = CAM_SYNC_CREATE;
    sync_ioctl.size      = sizeof(sync_create);
    sync_ioctl.ioctl_ptr = VoidPtrToUINT64(&sync_create);

    returnCode = ioctl(m_camSyncMgrKmdFd,
        CAM_PRIVATE_IOCTL_CMD,
        &sync_ioctl);

    if (-1 == returnCode)
    {
        CAM_MSG(ERROR, "CreateSync failed for index %d", 0);
    }
    else
    {
        *pSyncid = reinterpret_cast<struct cam_sync_info *>(sync_ioctl.ioctl_ptr)->sync_obj;
        result = CAMERA_SUCCESS;
    }

    return result;
}

CameraResult SyncManager::SyncRelease(
    int32 syncObj)
{
    int                          rc         = 0;
    struct cam_private_ioctl_arg sync_ioctl = {};
    struct cam_sync_info         sync_info  = {};

    sync_info.sync_obj = syncObj;

    sync_ioctl.id        = CAM_SYNC_DESTROY;
    sync_ioctl.size      = sizeof(sync_info);
    sync_ioctl.ioctl_ptr = VoidPtrToUINT64(&sync_info);

    rc = ioctl(m_camSyncMgrKmdFd,
        CAM_PRIVATE_IOCTL_CMD,
        &sync_ioctl);

    if (rc < 0)
    {
        CAM_MSG(ERROR, "Sync destroy IOCTL failed");
        return rc;
    }
    return rc;
}

SyncManager* SyncManager::GetInstance()
{
    if (m_SyncManagerInstance == nullptr)
    {
        m_SyncManagerInstance = new SyncManager();
        m_SyncManagerInstance->Init();
    }
    return m_SyncManagerInstance;
}

void SyncManager::DestroyInstance()
{
    if (m_SyncManagerInstance != nullptr)
    {
        m_SyncManagerInstance->DeInit();
        delete m_SyncManagerInstance;
        m_SyncManagerInstance = nullptr;
    }
}
