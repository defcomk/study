/*!
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ais_ife_configurer.h"
#include "ife_drv_api.h"


///@brief AisEngine singleton
AisIFEConfigurer* AisIFEConfigurer::m_pIfeConfigurerInstance = nullptr;

/***************************************************************************************************************************
*   AisIFEConfigurer::GetInstance
*
*   @brief
*       Gets the singleton instance for ChiDevice
***************************************************************************************************************************/
AisIFEConfigurer* AisIFEConfigurer::GetInstance()
{
    if(m_pIfeConfigurerInstance == nullptr)
    {
        m_pIfeConfigurerInstance = new AisIFEConfigurer();
    }

    return m_pIfeConfigurerInstance;
}

/***************************************************************************************************************************
*   AisIFEConfigurer::DestroyInstance
*
*   @brief
*       Destroy the singleton instance of the AisIFEConfigurer class
*
*   @return
*       void
***************************************************************************************************************************/
void AisIFEConfigurer::DestroyInstance()
{
    if(m_pIfeConfigurerInstance != nullptr)
    {
        delete m_pIfeConfigurerInstance;
        m_pIfeConfigurerInstance = nullptr;
    }
}

CameraResult AisIFEConfigurer::IfeDeviceCallback(void* pClientData,
        uint32 uidEvent, int nEventDataLen, void* pEventData)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisIFEConfigurer* pIfeCtxt = (AisIFEConfigurer*)pClientData;

    CAM_MSG(ISR, "Received IFE %p callback %d", pIfeCtxt, uidEvent);

    vfe_message_payload* pPayload = ((vfe_message_payload*)pEventData);
    IfeCoreType ifeId;


    if (nEventDataLen != sizeof(vfe_message_payload))
    {
        CAM_MSG(ERROR, "Received VFE %p callback %d with wrong size", pClientData, uidEvent);
        return CAMERA_EBADPARM;
    }
    else if (!pIfeCtxt || !pPayload)
    {
        CAM_MSG(ERROR, "Received VFE %p callback %d with no payload", pClientData, uidEvent);
        return CAMERA_EMEMPTR;
    }

    ifeId = (IfeCoreType)pPayload->vfeId;

    switch (uidEvent)
    {
    case VFE_MSG_ID_RAW_FRAME_DONE:
    {
        vfe_msg_output_raw* pRawMsg = &pPayload->_u.msgRaw;
        ais_engine_event_msg_t msg;

        STD_ZEROAT(&msg);
        msg.event_id = AIS_EVENT_RAW_FRAME_DONE;

        msg.payload.ife_frame_done.ife_core = ifeId;
        msg.payload.ife_frame_done.ife_output = pRawMsg->outputPath;

        msg.payload.ife_frame_done.frame_info.idx = pRawMsg->bufIndex;
        msg.payload.ife_frame_done.frame_info.seq_no = pRawMsg->frameCounter;
        msg.payload.ife_frame_done.frame_info.timestamp = pRawMsg->startTime;
        msg.payload.ife_frame_done.frame_info.sof_qtimestamp =  pIfeCtxt->m_IFECoreCtxt[ifeId].sof_ts_qtime[pRawMsg->outputPath+1];

#ifdef ENABLE_DIAG
        if( ais_diag_isenable(AIS_DIAG_LATENCY) )
        {
            int64 tm = 0;
            CameraGetTime(&tm);
            msg.payload.vfe_frame_done.us_starttime = tm;
        }
#endif

        /* store system time from payload into timestamo_system attribute of frame_info */
        msg.payload.ife_frame_done.frame_info.timestamp_system = pRawMsg->systemTime;

        CAM_MSG(MEDIUM, "VFE %d Output %d Frame Done buffer %d",
                msg.payload.ife_frame_done.ife_core, msg.payload.ife_frame_done.ife_output, msg.payload.ife_frame_done.frame_info.idx);

        AisEngine::GetInstance()->QueueEvent(&msg);

        break;
    }
    case VFE_MSG_ID_SOF:
    {
        vfe_msg_sof* pSofMsg = &pPayload->_u.msgStartOfFrame;
        ais_engine_event_msg_t msg;

        STD_ZEROAT(&msg);
        msg.event_id = AIS_EVENT_SOF;

        msg.payload.sof_info.ife_core = ifeId;
        msg.payload.sof_info.ife_input = (IfeInterfaceType)pSofMsg->intf;
        msg.payload.sof_info.sof_ts = pSofMsg->sofTime;
        msg.payload.sof_info.target_frame_id = pSofMsg->frameCounter;
        pIfeCtxt->m_IFECoreCtxt[ifeId].sof_ts_qtime[pSofMsg->intf] = pSofMsg->sofQTime;
        CAM_MSG(MEDIUM, "VFE %d Input %d Target frame id %d TS %llu",
            msg.payload.sof_info.ife_core, msg.payload.sof_info.ife_input,
            msg.payload.sof_info.target_frame_id, msg.payload.sof_info.sof_ts);

        rc = AisEngine::GetInstance()->QueueEvent(&msg);
        if (rc != CAMERA_SUCCESS)
        {
            CAM_MSG(ERROR, "Enqueue AIS_EVENT_SOF event failed: %d", rc);
        }

        break;
    }
    case VFE_MSG_ID_IFE_CSID_FATAL_ERROR:
    {
        vfe_msg_csid_error_status* pCsidMsg = &pPayload->_u.msgCsidError;
        ais_engine_event_msg_t msg;

        STD_ZEROAT(&msg);
        msg.event_id = AIS_EVENT_CSI_FATAL_ERROR;

        msg.payload.csid_fatal_error.csid = ifeId;
        msg.payload.csid_fatal_error.csid_error_timestamp = pCsidMsg->timestamp;

        CAM_MSG(ERROR, "VFE %d Received CSID_FATAL_ERROR at %lld",
                ifeId, pCsidMsg->timestamp);

        rc = AisEngine::GetInstance()->QueueEvent(&msg);
        if (rc != CAMERA_SUCCESS)
        {
            CAM_MSG(ERROR, "Enqueue CSID_FATAL_ERROR event failed: %d", rc);
        }

        break;
    }
    case VFE_MSG_ERROR_SOF_FREEZE:
    {
        vfe_msg_sof* pSofMsg = &pPayload->_u.msgStartOfFrame;
        ais_engine_event_msg_t msg;

        STD_ZEROAT(&msg);
        msg.event_id = AIS_EVENT_SOF_FREEZE;

        msg.payload.sof_info.ife_core = (IfeCoreType)ifeId;
        msg.payload.sof_info.ife_input = (IfeInterfaceType)pSofMsg->intf;
        msg.payload.sof_info.sof_ts = pSofMsg->sofTime;

        CAM_MSG(ERROR, "VFE %d Received VFE_MSG_ERROR_SOF_FREEZE at %lld",
                ifeId, pSofMsg->sofTime);

        rc = AisEngine::GetInstance()->QueueEvent(&msg);
        if (rc != CAMERA_SUCCESS)
        {
            CAM_MSG(ERROR, "Enqueue VFE_MSG_ERROR_SOF_FREEZE event failed: %d", rc);
        }

        break;
    }
    default:
        break;
    }

    return rc;
}


CameraResult AisIFEConfigurer::Init(void)
{
    CameraResult rc = CAMERA_SUCCESS;
    CameraDeviceInfoType cameraDeviceInfo[CSIPHY_CORE_MAX];
    int nCameraDeviceInfoLenReq;
    uint32 nDevice;

    memset(cameraDeviceInfo, 0, sizeof(cameraDeviceInfo));
    rc = mDeviceManagerContext->GetAvailableDevices(
        CAMERA_DEVICE_CATEGORY_IFE,
        &cameraDeviceInfo[0],
        IFE_CORE_MAX,
        &nCameraDeviceInfoLenReq);

    if (CAMERA_SUCCESS != rc)
    {
        CAM_MSG(ERROR, "Failed to get available ife devices");
        goto end;
    }
    else if (nCameraDeviceInfoLenReq > IFE_CORE_MAX)
    {
        CAM_MSG(ERROR, "Queried more IFE (%d) than maximum (%d)",
                nCameraDeviceInfoLenReq, IFE_CORE_MAX);
        rc = CAMERA_ENEEDMORE;
        goto end;
    }

    m_nDevices = nCameraDeviceInfoLenReq;

    for (nDevice = 0; nDevice < m_nDevices && CAMERA_SUCCESS == rc; ++nDevice)
    {
        rc = mDeviceManagerContext->DeviceOpen(cameraDeviceInfo[nDevice].deviceID,
                        &m_IFECoreCtxt[nDevice].hIfeHandle);
        CAM_MSG(HIGH, "open ifeHandle %p", m_IFECoreCtxt[nDevice].hIfeHandle);
        CAM_MSG_ON_ERR(rc, "Open ife%d failed with rc %d ", nDevice, rc);

        if (CAMERA_SUCCESS == rc)
        {
            rc = CameraDeviceRegisterCallback(m_IFECoreCtxt[nDevice].hIfeHandle, IfeDeviceCallback, this);
            CAM_MSG_ON_ERR(rc, "Registercallback ife%d failed with rc %d ", nDevice, rc);
        }

        if (CAMERA_SUCCESS == rc)
        {
            rc = CameraDeviceControl(m_IFECoreCtxt[nDevice].hIfeHandle, IFE_CMD_ID_POWER_ON,
                NULL,0, NULL, 0, NULL);
            CAM_MSG_ON_ERR(rc, "PowerON ife%d failed with rc %d ", nDevice, rc);
        }

        if (CAMERA_SUCCESS == rc)
        {
            rc = CameraDeviceControl(m_IFECoreCtxt[nDevice].hIfeHandle, IFE_CMD_ID_RESET,
                    NULL,0, NULL, 0, NULL);
            CAM_MSG_ON_ERR(rc, "Reset ife%d failed with rc %d ", nDevice, rc);
        }
    }

end:
    return rc;
}

CameraResult AisIFEConfigurer::Deinit(void)
{
    uint32 nDevice;

    for (nDevice = 0; nDevice < m_nDevices; nDevice++)
    {
        if (m_IFECoreCtxt[nDevice].hIfeHandle)
        {
            (void)CameraDeviceControl(m_IFECoreCtxt[nDevice].hIfeHandle, IFE_CMD_ID_POWER_OFF,
                NULL,0, NULL, 0, NULL);

            (void)CameraDeviceClose(m_IFECoreCtxt[nDevice].hIfeHandle);

            CAM_MSG(HIGH, "close ifeHandle %p", m_IFECoreCtxt[nDevice].hIfeHandle);

            m_IFECoreCtxt[nDevice].hIfeHandle = NULL;
        }
    }

    DestroyInstance();

    return CAMERA_SUCCESS;
}

CameraResult AisIFEConfigurer::PowerSuspend(void)
{
    return CAMERA_SUCCESS;
}
CameraResult AisIFEConfigurer::PowerResume(void)
{
    return CAMERA_SUCCESS;
}

CameraResult AisIFEConfigurer::Config(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc;
    ife_rdi_config_t sRdiCfg;
    ais_ife_user_path_info* pIfePath = &pUsrCtxt->m_resources.ife_user_path_info[0];

    memset(&sRdiCfg, 0x0, sizeof(sRdiCfg));
    sRdiCfg.outputPath = pIfePath->ife_output;
    sRdiCfg.csiphyId = (CsiphyCoreType)pUsrCtxt->m_resources.csiphy;
    sRdiCfg.vc = pUsrCtxt->m_inputCfg.inputModeInfo.vc;
    sRdiCfg.dt = pUsrCtxt->m_inputCfg.inputModeInfo.dt;
    sRdiCfg.width = pUsrCtxt->m_inputCfg.inputModeInfo.width;
    sRdiCfg.height = pUsrCtxt->m_inputCfg.inputModeInfo.height;
    sRdiCfg.numLanes = pUsrCtxt->m_inputCfg.csiInfo.num_lanes;

    rc = CameraDeviceControl(m_IFECoreCtxt[pIfePath->ife_core].hIfeHandle, IFE_CMD_ID_RDI_CONFIG,
            &sRdiCfg, sizeof(sRdiCfg), NULL, 0, NULL);
    CAM_MSG_ON_ERR(rc, "Config ife%d failed with rc %d ", 0, rc);

    return rc;
}

CameraResult AisIFEConfigurer::Start(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc;
    ife_start_cmd_t sStartCmd;
    int list_index = AIS_USER_OUTPUT_IDX;
    AisBufferList* pBufferList = &pUsrCtxt->m_bufferList[list_index];
    ais_ife_user_path_info* pIfePath = &pUsrCtxt->m_resources.ife_user_path_info[0];

    memset(&sStartCmd, 0x0, sizeof(sStartCmd));
    sStartCmd.outputPath = pIfePath->ife_output;
    sStartCmd.numOfOutputBufs = pBufferList->m_nBuffers;

    rc = CameraDeviceControl(m_IFECoreCtxt[pIfePath->ife_core].hIfeHandle, IFE_CMD_ID_START,
            &sStartCmd, sizeof(sStartCmd), NULL, 0, NULL);
    CAM_MSG_ON_ERR(rc, "Start ife%d failed with rc %d ", 0, rc);

    return rc;
}

CameraResult AisIFEConfigurer::Stop(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc;
    ife_stop_cmd_t sStopCmd;
    ais_ife_user_path_info* pIfePath = &pUsrCtxt->m_resources.ife_user_path_info[0];

    memset(&sStopCmd, 0x0, sizeof(sStopCmd));
    sStopCmd.outputPath = pIfePath->ife_output;

    rc = CameraDeviceControl(m_IFECoreCtxt[pIfePath->ife_core].hIfeHandle, IFE_CMD_ID_STOP,
            &sStopCmd, sizeof(sStopCmd), NULL, 0, NULL);
    CAM_MSG_ON_ERR(rc, "Stop ife%d failed with rc %d ", 0, rc);

    return rc;
}

CameraResult AisIFEConfigurer::Resume(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc;
    ife_start_cmd_t sStartCmd;
    int list_index = AIS_USER_OUTPUT_IDX;
    AisBufferList* pBufferList = &pUsrCtxt->m_bufferList[list_index];
    ais_ife_user_path_info* pIfePath = &pUsrCtxt->m_resources.ife_user_path_info[0];

    memset(&sStartCmd, 0x0, sizeof(sStartCmd));
    sStartCmd.outputPath = pIfePath->ife_output;
    sStartCmd.numOfOutputBufs = pBufferList->m_nBuffers;

    rc = CameraDeviceControl(m_IFECoreCtxt[pIfePath->ife_core].hIfeHandle, IFE_CMD_ID_RESUME,
    &sStartCmd, sizeof(sStartCmd), NULL, 0, NULL);

    return rc;
}

CameraResult AisIFEConfigurer::Pause(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc;
    ife_stop_cmd_t sStopCmd;
    ais_ife_user_path_info* pIfePath = &pUsrCtxt->m_resources.ife_user_path_info[0];

    memset(&sStopCmd, 0x0, sizeof(sStopCmd));
    sStopCmd.outputPath = pIfePath->ife_output;


    rc = CameraDeviceControl(m_IFECoreCtxt[pIfePath->ife_core].hIfeHandle, IFE_CMD_ID_PAUSE,
    &sStopCmd, sizeof(sStopCmd), NULL, 0, NULL);

    return rc;
}


CameraResult AisIFEConfigurer::SetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param)
{
    return CAMERA_SUCCESS;
}

CameraResult AisIFEConfigurer::GetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param)
{
    return CAMERA_SUCCESS;
}

CameraResult AisIFEConfigurer::QueueBuffers(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;
    int list_index = AIS_USER_OUTPUT_IDX;
    AisBufferList* pBufferList = &pUsrCtxt->m_bufferList[list_index];
    uint32 i=0;
    uint32 num = 0;

    for (i=0; i < pBufferList->m_nBuffers; i++)
    {
        rc = QueueBuffer(pUsrCtxt, pBufferList, i);
        if (rc != CAMERA_SUCCESS)
        {
            break;
        }
        ++num;
    }

    if (num == 0)
    {
        CAM_MSG(ERROR, "%s:%d usrctxt %p : no avail buffer, please release", __func__, __LINE__, pUsrCtxt);
    }

    return rc;
}

CameraResult AisIFEConfigurer::QueueBuffer(AisUsrCtxt* pUsrCtxt,
    AisBufferList* pBufferList, uint32 nIdx)
{
    CameraResult rc = CAMERA_SUCCESS;
    ais_ife_user_path_info* pIfePath = &pUsrCtxt->m_resources.ife_user_path_info[0];
    vfe_cmd_output_buffer sBufferEnqCmd;
    ais_buffer_t* pBuffer;

    if (nIdx >= pBufferList->m_nBuffers)
    {
        CAM_MSG(ERROR, "Invalid buffer idx %d", nIdx);
        return CAMERA_EBADPARM;
    }

    pBuffer = &(pBufferList->m_pBuffers[nIdx]);

    memset(&sBufferEnqCmd, 0x0, sizeof(sBufferEnqCmd));
    sBufferEnqCmd.outputPath = pIfePath->ife_output;
    sBufferEnqCmd.bufIndex = pBuffer->index;
    sBufferEnqCmd.da[0] = pBuffer->p_da;

    if (pIfePath->ife_core < IFE_CORE_MAX)
    {
         rc = CameraDeviceControl(m_IFECoreCtxt[pIfePath->ife_core].hIfeHandle, IFE_CMD_ID_OUTPUT_BUFFER_ENQUEUE,
                 &sBufferEnqCmd, sizeof(sBufferEnqCmd), NULL, 0, NULL);
    }
    else
    {
         rc = CAMERA_EBADPARM;
    }
    CAM_MSG_ON_ERR(rc, "VFE_CMD_ID_OUTPUT_BUFFER_ENQUEUE failed ife%d rc %d", pIfePath->ife_core, rc);

    if (rc == CAMERA_SUCCESS)
    {
        pBufferList->SetBufferState(nIdx, AIS_IFE_OUTPUT_QUEUE);
#ifdef DIAG_ENABLED
        ais_diag_update_context(pUsrCtxt,AIS_DIAG_CONTEXT_BUFFER_STATE);
#endif
    }

    return rc;
}

CameraResult AisIFEConfigurer::QueueInputBuffer(AisUsrCtxt* pUsrCtxt,
    AisBufferList* pBufferList, uint32 nIdx)
{
    return CAMERA_SUCCESS;
}
