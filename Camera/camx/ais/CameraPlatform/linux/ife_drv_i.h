/**
 * @file ife_drv_i.h
 *
 * Copyright (c) 2010-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _IFE_DRV_I_H
#define _IFE_DRV_I_H

#include "CameraDevice.h"
#include "CameraOSServices.h"
#include "CameraQueue.h"

#include "ife_drv_api.h"
#include "ife_drv_common.h"
#include "ife_stream_i.h"
#include "sync_mgr_i.h"
#include "req_mgr_drv.h"
#include "common_types.h"
#include <media/cam_isp.h>

/*-----------------------------------------------------------------------------
 * Type Declarations
 *---------------------------------------------------------------------------*/
class IFEDevice : public CameraDeviceBase
{
public:
    /** Control. Camera drivers implement this method.
     * @see CameraDeviceControl
     */
    virtual CameraResult Control(uint32 uidControl,
            const void* pIn, int nInLen, void* pOut, int nOutLen, int* pnOutLenReq);

    /**
     * Close function pointer. Camera drivers implement this method.
     * @see CameraDeviceClose
     */
    virtual CameraResult Close(void);

    /**
     * RegisterCallback. Camera drivers implement this method.
     * @see CameraDeviceRegisterCallback
     */
    virtual CameraResult RegisterCallback(CameraDeviceCallbackType pfnCallback, void *pClientData);

    static IFEDevice* IFEOpen(CameraDeviceIDType deviceId);

private:
    IFEDevice(uint8 ifeId);
    CameraResult Init();
    CameraResult DeInit();

    CameraResult IfeQueryHwCap();

    uint32 GetKMDCsiPhy(CsiphyCoreType csiCore);

    uint8 m_ifeId;
    int m_ifeKmdFd = 0; //The file descriptor of the ife hw mgr node

    IFEStream* m_RDIStream[IFE_RDI_MAX];

    CamDeviceHandle   m_camDeviceResource[IFE_RDI_MAX];

    struct cam_isp_query_cap_cmd m_ifeQueryCap;

    ife_rdi_info_t m_rdiCfg[IFE_RDI_MAX];

    int32_t m_session_handle = 0;

    SyncManager*       m_pSyncMgr  = nullptr;
    CamReqManager*     m_camReqMgr = nullptr;
};

#endif /* IFE_DRV_I_H */
