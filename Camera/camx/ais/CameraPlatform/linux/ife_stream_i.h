/*
 * @file ife_stream_i.h
 *
 * Copyright (c) 2010-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _IFE_STREAM_I_H
#define _IFE_STREAM_I_H

#include "CameraDevice.h"
#include "CameraOSServices.h"
#include "CameraQueue.h"

#include "ife_drv_api.h"
#include "ife_drv_common.h"
#include "sync_mgr_i.h"
#include "req_mgr_drv.h"
#include "common_types.h"
#include <media/cam_isp.h>

class IFEStream
{
    public:
        IFEStream(uint8 ifeId, int ifeKmdFd, uint32 rdi_index);

        CameraResult Init(
            cam_isp_query_cap_cmd* ifeQueryCap,
            int32_t session_hdl,
            ife_rdi_info_t* rdiCfg);

        CameraResult DeInit();
        CameraResult IfeStreamConfig();
        CameraResult IfeStreamStart(ife_start_cmd_t* p_start_cmd);
        CameraResult IfeStreamStop(ife_stop_cmd_t* p_stop_cmd);
        CameraResult IfeStreamBufferEnqueue(vfe_cmd_output_buffer* pBuf);
        CameraResult IfeStreamResume();
        CameraResult IfeStreamPause();
    private:
        CameraResult IfeAcquireDev(
            CamDeviceHandle*   phDevice,
            int32              deviceIndex,
            CamDeviceResource* pDeviceResourceRequest,
            size_t             numDeviceResources);

        CameraResult IfeReleaseDev(CamDeviceHandle* deviceHandle);

        CameraResult IfeAcquireHw(
            CamDeviceHandle deviceHandle,
            uint32 intfNum,
            CamDeviceResource* pDeviceResourceRequest);

        CameraResult IfeConfigAcquireHw();

        CameraResult IfeReleaseHW(CamDeviceHandle deviceHandle, uint32 intfNum);

        CameraResult IfeMapNativeBufferHW(
            vfe_cmd_output_buffer* cmd_buf,
            uint32 bufIdx);
        CameraResult IfeUnMapNativeBufferHw(uint32 bufIdx);

        CameraResult AddBufferConfigIO(uint32 pktBufIdx);
        CameraResult IfeBufferConfigIO(uint32 bufIndex);

        CameraResult IfeSubmit(
            CamDeviceHandle deviceHandle,
            size_t offset, CSLMemHandle hPacket);
        CameraResult IfeSubmitHw(uint32 pktBufIdx);

        uint32 WriteGenericBlobData(
            void* cmdBufPtr, uint32 offset,
            uint32 type, char* data, uint32 size);

        CameraResult AddCmdBufferReference(
            uint32 pktBufIdx, uint32 cmdBufIdx,
            uint32 opcode, uint32 request_id);

        CameraResult InitialSetupAndConfig();

        CameraResult IfeStreamOn(CamDeviceHandle deviceHandle);
        CameraResult IfeStreamOff(CamDeviceHandle deviceHandle);

        CameraResult IfeSubmitRequest(uint32 bufIdx);

        CameraResult IfeResetPacketInfo(uint32 pktBufIdx);
        void IfeResetPktandCommandBuffers(void);

        uint32 getPktIdx(uint32 bufIdx);
        uint32 getCmdIdx(uint32 bufIdx);


        uint8 m_ifeId;
        uint32 m_rdiIndex;
        bool m_IfeStreamOn = FALSE;
        CamDeviceResource m_deviceResource;
        uint32 m_ifeRequestId = 1;
        int m_ifeKmdFd;
        CamDeviceHandle m_camDeviceResource;
        ISPAcquireHWInfo* m_pIFEResourceInfo;
        size_t            m_IFEResourceSize;
        size_t            m_pktalignedResourceSize;
        size_t            m_cmdalignedResourceSize;
        uint32            m_pktDataPayloadSize;
        struct cam_isp_query_cap_cmd* m_ifeQueryCap;
        ife_rdi_info_t* m_rdiCfg;
        uint32 m_ifeCmdBlobCount;
        uint32 m_maxNumCmdBuffers;
        uint32 m_maxNumIOConfigs;
        uint32 m_maxNumPatches;
        int32_t m_session_handle;
        int32_t m_link_handle;
        CSLBufferInfo      m_bufferInfo[MAX_NUM_BUFF]; ///< CSL Buffer information having memhandle, fd, etc.
        int32              m_syncObj[MAX_NUM_BUFF];
        SyncManager*       m_pSyncMgr;
        CamReqManager*     m_camReqMgr;

        //Command Buffers
        CSLBufferInfo cmdBufferInfo; //KMD/UMD access - one larger buffer is allocated
        AISCmdBuffer  cmdBuffer[MAX_CMD_BUFF]; //The cmdBufferInfo memory is carved up

        //Packet Buffer
        CSLBufferInfo pktBufferInfo; //KMD/UMD access
        AISPktBuffer  pktBuffer[MAX_CMD_BUFF]; //The pktBufferInfo memory is carved up
};

#endif /* _IFE_STREAM_I_H */
