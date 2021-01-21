/**
 * @file sync_mgr_i.h
 *
 * Copyright (c) 2010-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _SYNC_MGR_I_H
#define _SYNC_MGR_I_H

#include "CameraDevice.h"
#include "ife_drv_api.h"
#include "AEEstd.h"
#include "CameraPlatformLinux.h"


/*-----------------------------------------------------------------------------
 * Type Declarations
 *---------------------------------------------------------------------------*/
class SyncManager
{
public:

    /**
     * RegisterCallback. Camera drivers implement this method.
     * @see CameraDeviceRegisterCallback
     */
    CameraResult RegisterCallback(CameraDeviceCallbackType pfnCallback, void *pClientData);

    CameraResult Init();
    CameraResult DeInit();

    CameraResult SyncRegisterPayload(
        int32 syncObj,
        uint64 payload0,
        uint64 payload1,
        uint64 payload2);

    CameraResult SyncCreate(
        const char* pName,
        int32* pSyncid);
    CameraResult SyncRelease(int32 syncObj);
    static SyncManager* GetInstance();
    static void DestroyInstance();

private:
    static SyncManager* m_SyncManagerInstance;
    int m_camSyncMgrKmdFd; //The file descriptor of the camera sync manager

    CameraResult ProcessSyncEvent(void* pData);
    CameraResult SubscribeEvents(int fd,
        uint32 id, uint32 type);

    static CameraResult SyncMgrEventThreadProc(void* arg);

    vfe_message        m_vfeMsg;
    uint64             m_frameCounter[IFE_OUTPUT_PATH_MAX]; //Internal frame counter

    /* readWrite by caller thread */
    CameraDeviceCallbackType           pfnCallBack;

    /* readWrite by caller thread */
    void*                              pClientData;
    void*                              SyncEventThreadHandle;
};

#endif /* _SYNC_MGR_I_H */
