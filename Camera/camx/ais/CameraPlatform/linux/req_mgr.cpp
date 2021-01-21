/**
 * @file req_mgr.cpp
 *
 * Cam Req manager driver interface.
 *
 * Copyright (c) 2010-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "AEEStdDef.h"
#include "AEEstd.h"
#include <stdio.h>
#include <errno.h>
#include <poll.h>

#include "CameraPlatform.h"
#include "CameraPlatformLinux.h"
#include "CameraOSServices.h"
#include <media/cam_defs.h>
#include <media/cam_req_mgr.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "req_mgr_drv.h"

CamReqManager* CamReqManager::m_CamReqManagerInstance = nullptr;

/* ===========================================================================
=========================================================================== */
CameraResult CamReqManager::RegisterCallback(CameraDeviceCallbackType pfnCallback, void *pClientData)
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


CameraResult CamReqManager::SubscribeEvents(int fd, uint32 id, uint32 type)
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

int CamReqManager::CRMEventThreadProc(void* arg)

{
    int pollStatus = -1;
    int exit_thread = 0;
    struct pollfd pollfds;
    CameraResult result = CAMERA_SUCCESS;
    CamReqManager* ReqMgr = (CamReqManager*)arg;

    pollfds.fd = ReqMgr->m_camReqMgrKmdFd;

    while(!exit_thread)
    {
        pollfds.events = POLLIN|POLLRDNORM|POLLPRI;
        pollStatus = poll(&pollfds, 1, -1);
        if(0 < pollStatus)
        {
            result = ReqMgr->CRMProcessEvent(ReqMgr);
        }
        else
        {
            CAM_MSG(ERROR, "poll failed");
        }
    }
    return 0;
}

CameraResult CamReqManager::Init()
{
    CameraResult result = CAMERA_SUCCESS;
    m_camReqMgrKmdFd = CameraPlatformGetFd(AIS_SUBDEV_REQMGR, 0);
    if (0 == m_camReqMgrKmdFd)
    {
        CAM_MSG(ERROR,   "Failed to get the Camera AIS_SUBDEV_AISMGR");
        result = CAMERA_EFAILED;
    }
    // Add subscribe events of event queue from KMD
    if (CAMERA_SUCCESS == result)
    {
        result = SubscribeEvents(m_camReqMgrKmdFd, V4L_EVENT_CAM_REQ_MGR_SOF,
            V4L_EVENT_CAM_REQ_MGR_EVENT);
        if (CAMERA_SUCCESS == result)
        {
            result = SubscribeEvents(m_camReqMgrKmdFd, V4L_EVENT_CAM_REQ_MGR_ERROR,
                V4L_EVENT_CAM_REQ_MGR_EVENT);
            if (CAMERA_SUCCESS != result)
            {
                CAM_MSG(ERROR, "Error event subscription failed");
                result = CAMERA_EFAILED;
            }
        }
        else
        {
            CAM_MSG(ERROR, "SOF Event subscription failed");
            result = CAMERA_EFAILED;
        }
    }

    //Create event thread that polls
    if (CAMERA_SUCCESS == result)
    {
        char name[64];
        snprintf(name, sizeof(name), "ife_event_thread");

        result = CameraCreateThread(CAMERA_THREAD_PRIO_HIGH_REALTIME, 0, CamReqManager::CRMEventThreadProc, this, 0x8000, name, &IfeEventThreadHandle);
        if(CAMERA_SUCCESS != result)
        {
            CAM_MSG(ERROR, "Create IfeEvent thread failed for VFE");
        }
    }
    return result;
}

CameraResult CamReqManager::AllocateCmdBuffers(CSLBufferInfo * pBufferInfo,
                                           size_t bufferSize,
                                           size_t alignment,
                                           uint32 flags,
                                           cam_isp_query_cap_cmd* ifeQueryCap)
{
    struct cam_control        ioctlCmd;
    struct cam_mem_mgr_alloc_cmd allocCmd = {};
    CameraResult              result    = CAMERA_SUCCESS;
    int                       returnCode;
    void*                     pLocalVirtualAddr = NULL;


    if (NULL == pBufferInfo)
    {
        CAM_MSG(ERROR, "IFE: pBufferInfo is NULL");
        return CAMERA_ENOMEMORY;
    }

    ioctlCmd.op_code         = CAM_REQ_MGR_ALLOC_BUF;
    ioctlCmd.size            = sizeof(allocCmd);
    ioctlCmd.handle_type     = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved        = 0;
    ioctlCmd.handle          = VoidPtrToUINT64(&allocCmd);


    allocCmd.len   = bufferSize;
    allocCmd.align = alignment;
    allocCmd.flags = flags;
    allocCmd.num_hdl = 2;  //only mapping non-secure ife and cdm mmu for now

    if ((MMU_HANDLE_INVALID == ifeQueryCap->device_iommu.non_secure) ||
        (MMU_HANDLE_INVALID == ifeQueryCap->cdm_iommu.non_secure) ||
        (0 == ifeQueryCap->device_iommu.non_secure) ||
        (0 == ifeQueryCap->cdm_iommu.non_secure))
    {
        CAM_MSG(ERROR, "IFE: ioctl failed invalid mmu handle ife 0x%x cdm 0x%x",
                       ifeQueryCap->device_iommu.non_secure,
                       ifeQueryCap->cdm_iommu.non_secure);
        return CAMERA_EBADPARM;
    }

    allocCmd.mmu_hdls[0] = ifeQueryCap->device_iommu.non_secure;
    allocCmd.mmu_hdls[1] = ifeQueryCap->cdm_iommu.non_secure;

    returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "IFE: ioctl failed AllocateCmdBuffers index %d", returnCode);
        result = CAMERA_EFAILED;
    }
    else
    {
        if (flags & CAM_MEM_FLAG_UMD_ACCESS)
        {
            //need to mmap the buffer for user space
            if ((allocCmd.len > 0) && (allocCmd.out.fd > 0))
            {
                pLocalVirtualAddr = mmap(NULL, allocCmd.len, (PROT_READ | PROT_WRITE), MAP_SHARED, allocCmd.out.fd, 0);
                if (MAP_FAILED == pLocalVirtualAddr)
                {
                    CAM_MSG(ERROR, "mmap() failed! errno=%d, bufferFD=%zd, bufferLength=%d offset=%zd",
                                   errno, allocCmd.out.fd, allocCmd.len, 0);
                    pLocalVirtualAddr = NULL;
                }
            }
        }

        pBufferInfo->hHandle         = allocCmd.out.buf_handle;
        pBufferInfo->pVirtualAddr    = pLocalVirtualAddr;
        pBufferInfo->deviceAddr      = 0;
        pBufferInfo->fd              = allocCmd.out.fd;
        pBufferInfo->offset          = 0;
        pBufferInfo->size            = allocCmd.len;
        CAM_MSG(HIGH, "IFE: ioctl success AllocateCmdBuffers");
    }

    return result;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLHwInternalCreateSession
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CameraResult CamReqManager::CRMCreateSession(int32_t* session_handle)
{
    struct cam_control              cmd;
    struct cam_req_mgr_session_info create_session;
    CameraResult result    = CAMERA_EFAILED;
    int returnCode;

    cmd.op_code     = CAM_REQ_MGR_CREATE_SESSION;
    cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cmd.size        = sizeof(create_session);
    cmd.handle      = VoidPtrToUINT64(&create_session);
    cmd.reserved    = 0;

    returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &cmd);
    if (-1 == returnCode)
    {
        CAM_MSG(ERROR, "Ioctl Failed for create session return = %d", returnCode);
    }
    else
    {
        if (0 > create_session.session_hdl)
        {
            struct cam_req_mgr_session_info destroy_session;

            cmd.op_code     = CAM_REQ_MGR_DESTROY_SESSION;
            cmd.size        = sizeof(destroy_session);
            cmd.handle      = VoidPtrToUINT64(&destroy_session);
            cmd.handle_type = CAM_HANDLE_USER_POINTER;
            cmd.reserved    = 0;

            destroy_session.session_hdl = create_session.session_hdl;
            returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &cmd);

            if (-1 == returnCode)
            {
                result = CAMERA_EFAILED;
                CAM_MSG(ERROR, "IFE: octl Failed for destroy session return = %d.", result);
            }
            *session_handle = 0;
        }
        else
        {
            *session_handle = create_session.session_hdl;
        }
        result = CAMERA_SUCCESS;
    }
    return result;
}

CameraResult CamReqManager::CRMDestroySession(int32_t* session_handle)
{
    struct cam_control              cmd;
    struct cam_req_mgr_session_info destroy_session;
    CameraResult result = CAMERA_EFAILED;
    int returnCode;

    cmd.op_code     = CAM_REQ_MGR_DESTROY_SESSION;
    cmd.size        = sizeof(destroy_session);
    cmd.handle      = VoidPtrToUINT64(&destroy_session);
    cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cmd.reserved    = 0;

    destroy_session.session_hdl = *session_handle;

    returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &cmd);

    if (-1 == returnCode)
    {
        result = CAMERA_EFAILED;
        CAM_MSG(ERROR, "IFE: octl Failed for destroy session return = %d.", result);
    }
    else
    {
        *session_handle = 0;
    }

    return result;
}


CameraResult CamReqManager::CRMProcessEvent(void* pData)
{
    CameraResult result = CAMERA_SUCCESS;
    int rc       = -1;
    CamReqManager* ReqMgr= (CamReqManager*)pData;
    struct v4l2_event v4l2_event;
    struct cam_req_mgr_message* pMessage = NULL;
    int64 mono_timestamp = 0;

    CAM_MSG(HIGH, "We got an Event !!!");
    memset(&v4l2_event, 0, sizeof(v4l2_event));

    rc = ioctl(m_camReqMgrKmdFd, VIDIOC_DQEVENT, &v4l2_event);
    if (rc >= 0)
    {
        if (V4L_EVENT_CAM_REQ_MGR_EVENT == v4l2_event.type)
        {
            CAM_MSG(MEDIUM, "Received an V4l2 event");
            pMessage = reinterpret_cast<struct cam_req_mgr_message*>(v4l2_event.u.data);

            if (NULL == pMessage)
            {
                CAM_MSG(MEDIUM, "Received an V4l2 event with enpty data!!");
                return CAMERA_EFAILED;
            }

            switch(v4l2_event.id)
            {
                case V4L_EVENT_CAM_REQ_MGR_SOF:
                {
                    CAM_MSG(HIGH, "Received SOF frame id %llu", pMessage->u.frame_msg.frame_id);
                    CAM_MSG(MEDIUM, "Frame timestamp = %llu", pMessage->u.frame_msg.timestamp);
                    CAM_MSG(MEDIUM, "Frame request id = %llu", pMessage->u.frame_msg.request_id);
                    CAM_MSG(MEDIUM, "Frame SOF status = %u", pMessage->u.frame_msg.sof_status);

                    // The timestamp reported by KMD is Qtimer timestamp. For framework, we need to convert it to
                    // MONOTONIC system timestamp
                    mono_timestamp =
                    ReqMgr->ConvertQTimerTimestampToMonotonicSystemTimestamp(pMessage->u.frame_msg.request_id,
                                                                            pMessage->u.frame_msg.timestamp);
                    CAM_MSG(MEDIUM, "Frame mono_timestamp = %llu", mono_timestamp);

                    ReqMgr->m_vfeMsg.msgId = VFE_MSG_ID_SOF;
                    ReqMgr->m_vfeMsg.payload._u.msgStartOfFrame.sofTime = (uint64)mono_timestamp;
                    //holds original SOF qtimer timestamp from KMD
                    ReqMgr->m_vfeMsg.payload._u.msgStartOfFrame.sofQTime = pMessage->u.frame_msg.timestamp;
                    //TODO::Add intf mapping
                    ReqMgr->m_vfeMsg.payload._u.msgStartOfFrame.intf = IFE_INTF_RDI0;
                    ReqMgr->m_vfeMsg.payload._u.msgStartOfFrame.frameCounter = pMessage->u.frame_msg.frame_id;
                }
                break;

                case V4L_EVENT_CAM_REQ_MGR_ERROR:
                {
                    CAM_MSG(HIGH, "Received event V4L_EVENT_CAM_REQ_MGR_ERROR error_type = %u",pMessage->u.err_msg.error_type);
                    CAM_MSG(MEDIUM, "Frame request id = %u", pMessage->u.err_msg.request_id);
                    CAM_MSG(MEDIUM, "Frame resource_size = %llu", pMessage->u.err_msg.resource_size);

                    ReqMgr->m_vfeMsg.msgId = VFE_MSG_ERROR_SOF_FREEZE;
                    (void)CameraGetTime(&ReqMgr->m_vfeMsg.payload._u.msgStartOfFrame.sofTime);
                    //TODO::Add intf mapping
                    ReqMgr->m_vfeMsg.payload._u.msgStartOfFrame.intf = IFE_INTF_RDI0;
                }
                break;

                default:
                    CAM_MSG(HIGH, "Received Unknown v4l2 event %d", v4l2_event.id);
                    return CAMERA_EFAILED;
            }

            if (ReqMgr->pfnCallBack)
            {
                ReqMgr->pfnCallBack(ReqMgr->pClientData, (uint32)(ReqMgr->m_vfeMsg.msgId),
                        sizeof(ReqMgr->m_vfeMsg.payload), (void*)(&(ReqMgr->m_vfeMsg.payload)));
            }
        }
        else
        {
            CAM_MSG(ERROR, "Received non v4l2 event = %d", v4l2_event.type);
            result = CAMERA_EFAILED;
        }
    }
    else
    {
        CAM_MSG(ERROR, "VIDIOC_DQEVENT failed on FD %d", m_camReqMgrKmdFd);
        result = CAMERA_EFAILED;
    }

    return result;
}

CameraResult CamReqManager::MapNativeBuffer(
    CSLBufferInfo* pBufferInfo,
    int fd,
    size_t offset,
    size_t size,
    uint32 flags,
    cam_isp_query_cap_cmd* ifeQueryCap)
{
    struct cam_mem_mgr_map_cmd  mapCmd = {};
    struct cam_control              ioctlCmd;
    CameraResult                    result          = CAMERA_SUCCESS;
    int                             returnCode;
    void*                     pLocalVirtualAddr = NULL;
    CAM_MSG(HIGH, "Entering IfeMapNativeBuffer");

    ioctlCmd.op_code         = CAM_REQ_MGR_MAP_BUF;
    ioctlCmd.size            = sizeof(mapCmd);
    ioctlCmd.handle_type     = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved        = 0;
    ioctlCmd.handle          = VoidPtrToUINT64(&mapCmd);

    mapCmd.num_hdl = 2;
    mapCmd.flags = flags;
    mapCmd.fd = fd;

    if ((MMU_HANDLE_INVALID == ifeQueryCap->device_iommu.non_secure) ||
       (MMU_HANDLE_INVALID == ifeQueryCap->cdm_iommu.non_secure) ||
       (0 == ifeQueryCap->device_iommu.non_secure) ||
       (0 == ifeQueryCap->cdm_iommu.non_secure))
    {
        CAM_MSG(ERROR, "IFE: ioctl failed invalid mmu handle ife 0x%x cdm 0x%x", ifeQueryCap->device_iommu.non_secure,
                                                                                 ifeQueryCap->cdm_iommu.non_secure);
        return CAMERA_EBADPARM;
    }
    mapCmd.mmu_hdls[0] = ifeQueryCap->device_iommu.non_secure;
    mapCmd.mmu_hdls[1] = ifeQueryCap->cdm_iommu.non_secure;
    returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "IFE: ioctl failed MapNativeBuffers index %d", returnCode);
        result = CAMERA_EFAILED;
    }
    else
    {
        if (flags & CAM_MEM_FLAG_UMD_ACCESS)
        {
            //need to mmap the buffer for user space
            if ((size > 0) && (fd > 0))
            {
                pLocalVirtualAddr = mmap(NULL, size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0);
                if (MAP_FAILED == pLocalVirtualAddr)
                {
                    CAM_MSG(ERROR, "mmap() failed! errno=%d, bufferFD=%zd, bufferLength=%d offset=%zd",
                                   errno, fd, size, 0);
                    pLocalVirtualAddr = NULL;
                    result = CAMERA_EFAILED;
                }
            }
        }
        pBufferInfo->hHandle         = mapCmd.out.buf_handle;
        pBufferInfo->pVirtualAddr    = pLocalVirtualAddr;
        pBufferInfo->deviceAddr      = 0;
        pBufferInfo->fd              = fd;
        pBufferInfo->offset          = 0;
        pBufferInfo->size            = size;
        pBufferInfo->flags           = flags;
    }
    return result;
}


CameraResult CamReqManager::ReleaseNativeBuffer(CSLBufferInfo* pBufferInfo)
{
    struct cam_mem_mgr_release_cmd releaseCmd = {};
    struct cam_control              ioctlCmd;
    CameraResult                    result          = CAMERA_SUCCESS;
    int                             returnCode;

    ioctlCmd.op_code         = CAM_REQ_MGR_RELEASE_BUF;
    ioctlCmd.size            = sizeof(releaseCmd);
    ioctlCmd.handle_type     = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved        = 0;
    ioctlCmd.handle          = VoidPtrToUINT64(&releaseCmd);

    releaseCmd.buf_handle = pBufferInfo->hHandle;

    if(NULL == pBufferInfo)
    {
        CAM_MSG(ERROR, "IFE: pBufferInfo is NULL");
        return CAMERA_ENOMEMORY;
    }

    if(FALSE == pBufferInfo->mappedToKMD)
    {
        CAM_MSG(ERROR, "IFE: pBufferInfo is not mapped");
        return CAMERA_EBADPARM;
    }

    //Unmap user space virtual address
    if (pBufferInfo->pVirtualAddr)
    {
        returnCode = munmap(pBufferInfo->pVirtualAddr, pBufferInfo->size);
        if (0 != returnCode)
        {
            CAM_MSG(ERROR, "munmap() failed! errno %d", errno);
            result = CAMERA_EFAILED;
        }
        else
        {
            pBufferInfo->pVirtualAddr = NULL;
        }
    }

    returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "Release Buff IOCTL failed! errno %d", errno);
        result = CAMERA_EFAILED;
    }
    else
    {
        //unmap KMD
        pBufferInfo->hHandle         = 0;
        pBufferInfo->deviceAddr      = 0;
    }

    return result;
}

CameraResult CamReqManager::FreeCmdBuffer(CSLBufferInfo* pBufferInfo)
{
    struct cam_control        ioctlCmd;
    struct cam_mem_mgr_release_cmd releaseCmd = {};
    CameraResult              result    = CAMERA_SUCCESS;
    int                       returnCode;

    if (NULL == pBufferInfo)
    {
        CAM_MSG(ERROR, "IFE: pBufferInfo is NULL");
        return CAMERA_ENOMEMORY;
    }

    ioctlCmd.op_code         = CAM_REQ_MGR_RELEASE_BUF;
    ioctlCmd.size            = sizeof(releaseCmd);
    ioctlCmd.handle_type     = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved        = 0;
    ioctlCmd.handle          = VoidPtrToUINT64(&releaseCmd);


    releaseCmd.buf_handle = pBufferInfo->hHandle;


    returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "IFE: ioctl failed deallocate buffer index %d", returnCode);
        result = CAMERA_EFAILED;
    }
    else
    {

        //mumap the buffer for user space if necessary
        if (pBufferInfo->pVirtualAddr)
        {
            returnCode = munmap(pBufferInfo->pVirtualAddr, pBufferInfo->size);
            if (0 != returnCode)
            {
                CAM_MSG(ERROR, "mumap() failed! errno=%d, bufferFD=%zd, bufferLength=%d",
                                errno, pBufferInfo->pVirtualAddr, pBufferInfo->size);
            }
        }

        memset(pBufferInfo, 0, sizeof(CSLBufferInfo));

        CAM_MSG(HIGH, "IFE: ioctl success DeAllocateCmdBuffers");
    }

    return result;

}

//*******************************************************
//* This function is used to link the ife with the csiphy
//************************************** ****************
CameraResult CamReqManager::LinkDevices(
    CamDeviceHandle camDeviceResource,
    int32_t session_handle,
    int32_t* link_handle)
{
    struct cam_control           cmd;
    struct cam_req_mgr_link_info linkHw;
    CameraResult              result    = CAMERA_SUCCESS;
    int                       returnCode;

    cmd.op_code     = CAM_REQ_MGR_LINK;
    cmd.size        = sizeof(linkHw);
    cmd.handle      = VoidPtrToUINT64(&linkHw);
    cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cmd.reserved    = 0;

    linkHw.session_hdl = session_handle;
    linkHw.num_devices = 1; //Lets try 1 device

    linkHw.dev_hdls[0] = camDeviceResource;  //Ife device
    linkHw.dev_hdls[1] = 0;

    returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &cmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "IFE: ioctl failed linking device 0x%x 0x%x returnCode 0x%x", linkHw.dev_hdls[0], linkHw.dev_hdls[1], returnCode );
        result = CAMERA_EFAILED;
    }
    else
    {
        *link_handle = linkHw.link_hdl;
        CAM_MSG(HIGH, "IFE: ioctl success linking device 0x%x 0x%x", linkHw.dev_hdls[0], linkHw.dev_hdls[1] );
    }

    return result;
}

//*******************************************************
//* This function is used to unlink the ife with the csiphy
//************************************** ****************
CameraResult CamReqManager::UnlinkDevices(
    CamDeviceHandle camDeviceResource,
    int32_t session_handle,
    int32_t* link_handle)
{
    struct cam_control           cmd;
    struct cam_req_mgr_unlink_info unlinkHw;
    CameraResult              result    = CAMERA_SUCCESS;
    int                       returnCode;

    cmd.op_code     = CAM_REQ_MGR_UNLINK;
    cmd.size        = sizeof(unlinkHw);
    cmd.handle      = VoidPtrToUINT64(&unlinkHw);
    cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cmd.reserved    = 0;

    unlinkHw.session_hdl = session_handle;
    unlinkHw.link_hdl    = *link_handle;

    returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &cmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "IFE: ioctl failed with session = %d for link = %d", unlinkHw.session_hdl, unlinkHw.link_hdl);
        result = CAMERA_EFAILED;
    }
    else
    {
        *link_handle = 0;
    }
    return result;
}


CameraResult CamReqManager::CRMScheduleRequest(
    int32_t   hSession,
    int32_t   hLink,
    uint64    requestid,
    bool      bubble,
    int32     syncMode)
{
    int                              returnCode  = 0;
    struct cam_control               cmd         = {};
    struct cam_req_mgr_sched_request openRequest = {};
    CameraResult                     result      = CAMERA_SUCCESS;

    cmd.op_code     = CAM_REQ_MGR_SCHED_REQ;
    cmd.size        = sizeof(openRequest);
    cmd.handle      = VoidPtrToUINT64(&openRequest);
    cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cmd.reserved    = 0;

    openRequest.req_id        = requestid;
    openRequest.sync_mode     = syncMode;
    openRequest.session_hdl   = hSession;
    openRequest.bubble_enable = bubble;
    openRequest.link_hdl      = hLink;


    returnCode = ioctl(m_camReqMgrKmdFd, VIDIOC_CAM_CONTROL, &cmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "Ioctl Failed with session = %d, result = %d", hSession, result);
        result = CAMERA_EFAILED;
    }

    return result;
}

uint64 CamReqManager::ConvertQTimerTimestampToMonotonicSystemTimestamp(
    uint64 requestId,
    uint64 qTimerTimestamp)
{
    uint64 currentTimestamp;
    if (1 == requestId || m_lastMonoTimestamp == 0)
    {
        (void)CameraGetTime(&currentTimestamp);
    }
    else
    {
        currentTimestamp = (qTimerTimestamp - m_lastQTimerTimestamp) + m_lastMonoTimestamp;
    }
    m_lastMonoTimestamp   = currentTimestamp;
    m_lastQTimerTimestamp = qTimerTimestamp;

    return currentTimestamp;
}

CameraResult CamReqManager::DeInit()
{
    CameraResult result = CAMERA_SUCCESS;
    m_camReqMgrKmdFd = 0;
    result = CameraReleaseThread(IfeEventThreadHandle);

    return result;

}

CamReqManager* CamReqManager::GetInstance()
{
    if (m_CamReqManagerInstance == nullptr)
    {
        m_CamReqManagerInstance = new CamReqManager();
        m_CamReqManagerInstance->Init();
    }
    return m_CamReqManagerInstance;
}

void CamReqManager::DestroyInstance()
{
    if (m_CamReqManagerInstance != nullptr)
    {
        m_CamReqManagerInstance->DeInit();
        delete m_CamReqManagerInstance;
        m_CamReqManagerInstance = nullptr;
    }
}
