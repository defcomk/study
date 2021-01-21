/**
 * @file req_mgr_drv.h
 *
 * Copyright (c) 2010-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _REQ_MGR_DRV_I_H
#define _REQ_MGR_DRV_I_H

#include "CameraDevice.h"
#include "CameraOSServices.h"
#include "CameraQueue.h"

#include "ife_drv_api.h"
#include "sync_mgr_i.h"
#include "common_types.h"
#include <media/cam_isp.h>

class CamReqManager
{
public:
    CameraResult Init();
    CameraResult DeInit();
    CameraResult RegisterCallback(
        CameraDeviceCallbackType pfnCallback,
        void *pClientData);
    CameraResult AllocateCmdBuffers(
        CSLBufferInfo * pBufferInfo,
        size_t bufferSize,
        size_t alignment,
        uint32 flags,
        cam_isp_query_cap_cmd* ifeQueryCap);
    CameraResult CRMCreateSession(int32_t* session_handle);
    CameraResult CRMDestroySession(int32_t* session_handle);
    CameraResult CRMProcessEvent(void* pData);
    CameraResult MapNativeBuffer(
        CSLBufferInfo* pBufferInfo,
        int fd,
        size_t offset,
        size_t size,
        uint32 flags,
        cam_isp_query_cap_cmd* ifeQueryCap);
    CameraResult ReleaseNativeBuffer(CSLBufferInfo* pBufferInfo);

    CameraResult FreeCmdBuffer(CSLBufferInfo* pBufferInfo);
    CameraResult LinkDevices(
        CamDeviceHandle camDeviceResource,
        int32_t session_handle,
        int32_t* link_handle);
    CameraResult UnlinkDevices(
        CamDeviceHandle camDeviceResource,
        int32_t session_handle,
        int32_t* link_handle);
    CameraResult CRMScheduleRequest(
        int32_t hSession,
        int32_t hLink,
        uint64 requestid,
        bool bubble,
        int32 syncMode);
    static CamReqManager* GetInstance();
    static void DestroyInstance();

private:
    CameraResult SubscribeEvents(
        int fd,
        uint32 id,
        uint32 type);
    static int CRMEventThreadProc(void* arg);
    uint64 ConvertQTimerTimestampToMonotonicSystemTimestamp(
        uint64 requestId,
        uint64 qTimerTimestamp);
    static CamReqManager* m_CamReqManagerInstance;
    int m_camReqMgrKmdFd; //The file descriptor of the camera request manager
    void* IfeEventThreadHandle;
    vfe_message        m_vfeMsg;
    uint64             m_lastMonoTimestamp;
    uint64             m_lastQTimerTimestamp;
    /* readWrite by caller thread */
    CameraDeviceCallbackType           pfnCallBack;

    /* readWrite by caller thread */
    void*                              pClientData;
};

#endif /* _REQ_MGR_DRV_I_H */
