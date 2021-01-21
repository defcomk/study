/*!
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ais_input_configurer.h"


///@brief AisEngine singleton
AisInputConfigurer* AisInputConfigurer::m_pInputConfigurerInstance = nullptr;

/***************************************************************************************************************************
*   AisInputConfigurer::GetInstance
*
*   @brief
*       Gets the singleton instance for ChiDevice
***************************************************************************************************************************/
AisInputConfigurer* AisInputConfigurer::GetInstance()
{
    if(m_pInputConfigurerInstance == nullptr)
    {
        m_pInputConfigurerInstance = new AisInputConfigurer();
    }

    return m_pInputConfigurerInstance;
}

/***************************************************************************************************************************
*   AisInputConfigurer::DestroyInstance
*
*   @brief
*       Destroy the singleton instance of the AisInputConfigurer class
*
*   @return
*       void
***************************************************************************************************************************/
void AisInputConfigurer::DestroyInstance()
{
    if(m_pInputConfigurerInstance != nullptr)
    {
        delete m_pInputConfigurerInstance;
        m_pInputConfigurerInstance = nullptr;
    }
}

CameraResult AisInputConfigurer::Init(void)
{
    CameraResult result = CAMERA_SUCCESS;
    CameraDeviceInfoType cameraDeviceInfo[MAX_CAMERA_INPUT_DEVICES];
    uint32 device;
    int nCameraDeviceInfoLenReq = 0;

    //Get platform channel mapping info
    CameraChannelDataType* cameraDataChannel = (CameraChannelDataType*)
            CameraAllocate(CAMERA_ALLOCATE_ID_AIS_MODULE_CTXT,
                    MAX_CAMERA_INPUT_CHANNELS*sizeof(CameraChannelDataType));
    int numChannels = MAX_CAMERA_INPUT_CHANNELS;
    if (0 == CameraPlatformGetChannelData(cameraDataChannel, &numChannels))
    {
        int i;
        for (i=0; i<numChannels; i++)
        {
            m_InputMappingTable[i].input_id = (qcarcam_input_desc_t)cameraDataChannel[i].aisInputId;
            m_InputMappingTable[i].device_id = cameraDataChannel[i].devId;
            m_InputMappingTable[i].src_id = cameraDataChannel[i].srcId;
        }
        m_nInputMapping = numChannels;

    }
    else
    {
        CAM_MSG(ERROR, "%s failed rc %d ", __func__, result);
        result = CAMERA_EFAILED;
    }
    CameraFree(CAMERA_ALLOCATE_ID_AIS_MODULE_CTXT, cameraDataChannel);

    //free first then goto error if failed
    if (CAMERA_SUCCESS != result)
        goto error;

    // Get the list of available sensor device drivers from the
    // CameraDeviceManager
    std_memset(cameraDeviceInfo, 0, sizeof(cameraDeviceInfo));
    result = mDeviceManagerContext->GetAvailableDevices(
        CAMERA_DEVICE_CATEGORY_SENSOR,
        &cameraDeviceInfo[0],
        MAX_CAMERA_INPUT_DEVICES,
        &nCameraDeviceInfoLenReq);

    if (CAMERA_SUCCESS != result)
    {
        CAM_MSG(ERROR, "Failed to get available input devices");
        goto error;
    }
    else if (nCameraDeviceInfoLenReq > MAX_CAMERA_INPUT_DEVICES)
    {
        CAM_MSG(ERROR, "Queried more inputs (%d) than can handle (%d)",
                nCameraDeviceInfoLenReq, MAX_CAMERA_INPUT_DEVICES);
        result = CAMERA_ENEEDMORE;
        goto error;
    }

    m_nInputDevices = nCameraDeviceInfoLenReq;

    // Search list of sensor device drivers to determine which ones are
    // actually available for use
    for (device = 0;
         device < (uint32)nCameraDeviceInfoLenReq;
         ++device)
    {
        unsigned int i;

        m_InputDevices[device].sDeviceInfo = cameraDeviceInfo[device];
        m_InputDevices[device].device_id = cameraDeviceInfo[device].deviceID;

        for(i = 0; i < m_nInputMapping; i++)
        {
            if (m_InputMappingTable[i].device_id == m_InputDevices[device].device_id)
            {
                m_InputMappingTable[i].dev_id = device;
            }
        }

        CAM_MSG(HIGH, "Open sensor with ID: 0x%x ...",
                m_InputDevices[device].sDeviceInfo.deviceID);

        CameraInputDeviceInterfaceType inputInterface;
        result = CameraPlatformGetInputDeviceInterface(m_InputDevices[device].device_id, &inputInterface);
        if (result != CAMERA_SUCCESS)
        {
            CAM_MSG(ERROR, "Could not get sensor interface info for device ID : 0x%x",
                    m_InputDevices[device].sDeviceInfo.deviceID);
            continue;
        }
        m_InputDevices[device].csid = inputInterface.csid;
        m_InputDevices[device].csiphy = inputInterface.csiphy;

        result = mDeviceManagerContext->DeviceOpen(m_InputDevices[device].sDeviceInfo.deviceID,
                &m_InputDevices[device].hDevice);

        // If the sensor driver did not open successfully, ignore it and move
        // on. Otherwise, if the sensor driver was successfully opened, then
        // add this sensor to the list of available ones.
        if (CAMERA_SUCCESS != result)
        {
            m_InputDevices[device].hDevice = NULL;
            CAM_MSG(ERROR, "Could not open sensor with ID: 0x%x",
                    m_InputDevices[device].sDeviceInfo.deviceID);
            continue;
        }

        result = CameraDeviceControl(m_InputDevices[device].hDevice,
                Camera_Sensor_AEEUID_CTL_DETECT_DEVICE,
                NULL, 0, NULL, 0, NULL);
        if (CAMERA_SUCCESS != result)
        {
            CAM_MSG(ERROR, "Could not detect sensor with ID: 0x%x",
                    m_InputDevices[device].sDeviceInfo.deviceID);
            continue;
        }

        result = CameraDeviceRegisterCallback(m_InputDevices[device].hDevice, AisInputConfigurer::InputDeviceCallback, this);
        if (CAMERA_SUCCESS != result)
        {
            CAM_MSG(ERROR, "Could not set callback for sensor with ID: 0x%x",
                    m_InputDevices[device].sDeviceInfo.deviceID);
            continue;
        }

        m_InputDevices[device].state = AIS_INPUT_STATE_DETECTED;
    }

    return CAMERA_SUCCESS;

error:
    return result;
}

CameraResult AisInputConfigurer::Deinit(void)
{
    CameraResult result = CAMERA_SUCCESS;
    uint32 device;

    for (device = 0;
         device < m_nInputDevices;
         ++device)
    {
        if (m_InputDevices[device].hDevice)
        {
            result |= CameraDeviceClose(m_InputDevices[device].hDevice);
        }
    }

    DestroyInstance();

    return CAMERA_SUCCESS;
}

CameraResult AisInputConfigurer::PowerSuspend(void)
{
    CameraResult result = CAMERA_SUCCESS;
    uint32 device;

    for (device = 0;
            device < m_nInputDevices;
            ++device)
    {
        //check that device was successfully opened and chip detected
        if (!m_InputDevices[device].hDevice ||
                m_InputDevices[device].state == AIS_INPUT_STATE_UNAVAILABLE)
            continue;

        result = CameraDeviceControl(m_InputDevices[device].hDevice,
                Camera_Sensor_AEEUID_CTL_STATE_POWER_SUSPEND,
                NULL, 0, NULL, 0, NULL);
        if (CAMERA_SUCCESS != result)
        {
            CAM_MSG(HIGH, "Could not detect channels for sensor with ID: 0x%x",
                    m_InputDevices[device].sDeviceInfo.deviceID);
            continue;
        }
    }

    return CAMERA_SUCCESS;
}

CameraResult AisInputConfigurer::PowerResume(void)
{
    CameraResult result = CAMERA_SUCCESS;
    uint32 device;

    for (device = 0;
         device < m_nInputDevices;
         ++device)
    {
        //check that device was successfully opened and chip detected
        if (!m_InputDevices[device].hDevice ||
                m_InputDevices[device].state == AIS_INPUT_STATE_UNAVAILABLE)
            continue;

        result = CameraDeviceControl(m_InputDevices[device].hDevice,
                Camera_Sensor_AEEUID_CTL_STATE_POWER_RESUME,
                NULL, 0, NULL, 0, NULL);
        if (CAMERA_SUCCESS != result)
        {
            CAM_MSG(HIGH, "Could not detect channels for sensor with ID: 0x%x",
                    m_InputDevices[device].sDeviceInfo.deviceID);
            continue;
        }
    }

    return CAMERA_SUCCESS;
}

CameraResult AisInputConfigurer::Config(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;

    rc = ValidateInputId(pUsrCtxt);
    if (CAMERA_SUCCESS != rc)
    {
        CAM_MSG(ERROR, "Input %d not connected",(int)pUsrCtxt->m_inputId);
        return rc;
    }

    if (CAMERA_SUCCESS == rc)
    {
        uint32 mode = 0;
        rc = SetParam(pUsrCtxt,
                AIS_INPUT_CTRL_MODE,
                &mode);
    }

    if(CAMERA_SUCCESS == rc && (pUsrCtxt->m_usrSettings.bitmask & (1 << QCARCAM_PARAM_RESOLUTION)))
    {
        qcarcam_res_t resInfo;
        resInfo.width = pUsrCtxt->m_usrSettings.width;
        resInfo.height = pUsrCtxt->m_usrSettings.height;

        rc = SetParam(pUsrCtxt,
                AIS_INPUT_CTRL_RESOLUTION,
                &resInfo);

        CAM_MSG(HIGH, "QCARCAM_PARAM_RESOLUTION is set to %dx%d",
                resInfo.width,resInfo.height);
    }

    if (CAMERA_SUCCESS == rc)
    {
        rc = GetParam(pUsrCtxt,
                AIS_INPUT_CTRL_CSI_PARAM,
                &pUsrCtxt->m_inputCfg.csiInfo);

        CAM_MSG(HIGH, "settle %d lanes %d mask 0x%x",
                pUsrCtxt->m_inputCfg.csiInfo.settle_count, pUsrCtxt->m_inputCfg.csiInfo.num_lanes,
                pUsrCtxt->m_inputCfg.csiInfo.lane_mask);

    }

    if (CAMERA_SUCCESS == rc)
    {
        rc = GetParam(pUsrCtxt,
                AIS_INPUT_CTRL_MODE_INFO,
                &pUsrCtxt->m_inputCfg.inputModeInfo);
        CAM_MSG(HIGH, "width %d height %d fps %d vc %d dt %d interlaced %d",
                pUsrCtxt->m_inputCfg.inputModeInfo.width, pUsrCtxt->m_inputCfg.inputModeInfo.height,
                pUsrCtxt->m_inputCfg.inputModeInfo.fps, pUsrCtxt->m_inputCfg.inputModeInfo.vc,
                pUsrCtxt->m_inputCfg.inputModeInfo.dt, pUsrCtxt->m_inputCfg.inputModeInfo.interlaced);
    }

    return rc;
}

CameraResult AisInputConfigurer::Start(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;
    uint32 devId = pUsrCtxt->m_inputCfg.devId;

    if (AIS_INPUT_STATE_ON == m_InputDevices[devId].state)
    {
        rc = CameraDeviceControl(m_InputDevices[devId].hDevice,
                Camera_Sensor_AEEUID_CTL_STATE_FRAME_OUTPUT_START,
                NULL, 0, NULL, 0, NULL);
        CAM_MSG_ON_ERR((CAMERA_SUCCESS != rc), "Failed to start input device %d rc=%d", devId, rc);
        if (CAMERA_SUCCESS == rc)
        {
            m_InputDevices[devId].state = AIS_INPUT_STATE_STREAMING;
        }
        else
        {
            return rc;
        }
    }

    //Add user to track
    m_InputDevices[devId].refcnt++;
    CAM_MSG(MEDIUM, "input start refcnt %d",
            m_InputDevices[devId].refcnt);

    return rc;
}

CameraResult AisInputConfigurer::Stop(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;
    uint32 devId = pUsrCtxt->m_inputCfg.devId;

    //Remove user of input
    if (0 == m_InputDevices[devId].refcnt)
    {
        return CAMERA_SUCCESS;
    }

    m_InputDevices[devId].refcnt--;
    CAM_MSG(MEDIUM, "input stop CID: 0x%x ref %d",
                pUsrCtxt->m_resources.cid, m_InputDevices[devId].refcnt);

    if (0 == m_InputDevices[devId].refcnt &&
            AIS_INPUT_STATE_STREAMING == m_InputDevices[devId].state)
    {
        CAM_MSG(HIGH, "No active users stop device %d", devId);
        rc = CameraDeviceControl(m_InputDevices[devId].hDevice,
                Camera_Sensor_AEEUID_CTL_STATE_FRAME_OUTPUT_STOP,
                NULL, 0, NULL, 0, NULL);
        CAM_MSG_ON_ERR((CAMERA_SUCCESS != rc), "Failed to stop input device %d rc=%d", devId, rc);
        m_InputDevices[devId].state = AIS_INPUT_STATE_ON;
    }

    return rc;
}

CameraResult AisInputConfigurer::Resume(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;

    return rc;
}

CameraResult AisInputConfigurer::Pause(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;

    return rc;
}


CameraResult AisInputConfigurer::SetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param)
{
    CameraResult result = CAMERA_SUCCESS;
    uint32 devId = pUsrCtxt->m_inputCfg.devId;
    uint32 srcId = pUsrCtxt->m_inputCfg.srcId;

    switch (nCtrl)
    {
    case AIS_INPUT_CTRL_MODE:
    {
        result = CameraDeviceControl(m_InputDevices[devId].hDevice,
                Camera_Sensor_AEEUID_CTL_CONFIG_MODE,
                param, sizeof(uint32), NULL, 0, NULL);
        break;
    }
    case AIS_INPUT_CTRL_RESOLUTION:
    {
        result = CameraDeviceControl(m_InputDevices[devId].hDevice,
                Camera_Sensor_AEEUID_CTL_CONFIG_RESOLUTION,
                param, sizeof(qcarcam_res_t), NULL, 0, NULL);
        break;
    }
    case AIS_INPUT_CTRL_EXPOSURE_CONFIG:
    {
        Camera_Sensor_ParamConfigType sensor_config_params;
        sensor_config_params.param = QCARCAM_PARAM_EXPOSURE;
        sensor_config_params.sensor_params.sensor_exposure_config.fGain =
                pUsrCtxt->m_usrSettings.exposure_params.gain;
        sensor_config_params.sensor_params.sensor_exposure_config.integrationParam.fExposureTime =
                pUsrCtxt->m_usrSettings.exposure_params.exposure_time;
        sensor_config_params.sensor_params.sensor_exposure_config.src_id = srcId;
        sensor_config_params.sensor_params.sensor_exposure_config.exposure_mode_type =
                pUsrCtxt->m_usrSettings.exposure_params.exposure_mode_type;
        result = CameraDeviceControl(m_InputDevices[devId].hDevice,
                Camera_Sensor_AEEUID_CTL_CONFIG_SENSOR_PARAMS,
                &sensor_config_params, sizeof(Camera_Sensor_ParamConfigType), NULL, 0, NULL);
        CAM_MSG_ON_ERR((CAMERA_SUCCESS != result),
                "Error setting  AIS_INPUT_EXPOSURE_CONFIG for devID =%d, inputid=%d, result=%d",
                devId, pUsrCtxt->m_inputId, result);
        break;
    }
    case AIS_INPUT_CTRL_SATURATION_CONFIG:
    {
        Camera_Sensor_ParamConfigType sensor_config_params;
        sensor_config_params.param = QCARCAM_PARAM_SATURATION;
        sensor_config_params.sensor_params.sensor_color_config.nVal =
            pUsrCtxt->m_usrSettings.saturation_param;
        sensor_config_params.sensor_params.sensor_color_config.src_id = srcId;

        result = CameraDeviceControl(m_InputDevices[devId].hDevice,
                Camera_Sensor_AEEUID_CTL_CONFIG_SENSOR_PARAMS,
                &sensor_config_params, sizeof(Camera_Sensor_ParamConfigType), NULL, 0, NULL);
        CAM_MSG_ON_ERR((CAMERA_SUCCESS != result),
                "Error setting  AIS_INPUT_CTRL_SATURATION_CONFIG for devID =%d, inputid=%d, result=%d",
                devId, pUsrCtxt->m_inputId, result);
        break;
    }
    case AIS_INPUT_CTRL_HUE_CONFIG:
    {
        Camera_Sensor_ParamConfigType sensor_config_params;
        sensor_config_params.param = QCARCAM_PARAM_HUE;
        sensor_config_params.sensor_params.sensor_color_config.nVal =
            pUsrCtxt->m_usrSettings.hue_param;
        sensor_config_params.sensor_params.sensor_color_config.src_id = srcId;

        result = CameraDeviceControl(m_InputDevices[devId].hDevice,
                Camera_Sensor_AEEUID_CTL_CONFIG_SENSOR_PARAMS,
                &sensor_config_params, sizeof(Camera_Sensor_ParamConfigType), NULL, 0, NULL);
        CAM_MSG_ON_ERR((CAMERA_SUCCESS != result),
                "Error setting  AIS_INPUT_CTRL_HUE_CONFIG for devID =%d, inputid=%d, result=%d",
                devId, pUsrCtxt->m_inputId, result);
        break;
    }
    default:
    {
        CAM_MSG (ERROR, "%s unsupported CTRL_ID %d",__func__, nCtrl);
        result = CAMERA_EBADPARM;
        break;
    }
    }

    return result;
}

CameraResult AisInputConfigurer::GetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param)
{
    CameraResult result = CAMERA_SUCCESS;

    switch (nCtrl)
    {
        case AIS_INPUT_CTRL_INPUT_INTERF:
        {
            uint32 idx;
            for (idx=0; idx<m_nInputMapping; idx++)
            {
                if (pUsrCtxt->m_inputId == m_InputMappingTable[idx].input_id)
                {
                    pUsrCtxt->m_inputCfg.devId = m_InputMappingTable[idx].dev_id;
                    pUsrCtxt->m_inputCfg.srcId = m_InputMappingTable[idx].src_id;
                    break;
                }
            }
        }
        break;
        case AIS_INPUT_CTRL_CSI_PARAM:
        {
            Camera_Sensor_MipiCsiInfoType csiInfo;
            uint32 devId = pUsrCtxt->m_inputCfg.devId;
            uint32 srcId = pUsrCtxt->m_inputCfg.srcId;

            std_memset(&csiInfo, 0, sizeof(csiInfo));

            ais_input_csi_params_t* pCsiParams = (ais_input_csi_params_t*)param;

            result = CameraDeviceControl(m_InputDevices[devId].hDevice,
                    Camera_Sensor_AEEUID_CTL_CSI_INFO_PARAMS,
                    NULL, 0, &csiInfo, sizeof(csiInfo), NULL);

            if (CAMERA_SUCCESS == result)
            {
                pCsiParams->num_lanes = csiInfo.num_lanes;
                pCsiParams->lane_mask = csiInfo.lane_mask;
                pCsiParams->settle_count = csiInfo.settle_count;
            }

            CAM_MSG_ON_ERR((CAMERA_SUCCESS != result),
                "Error retrieving csi params for devId %d srcId %d result=%d",
                devId, srcId, result);
        }
        break;
        case AIS_INPUT_CTRL_MODE_INFO:
        {
            ais_input_mode_info_t* pModeInfo = (ais_input_mode_info_t*)param;
            uint32 devId = pUsrCtxt->m_inputCfg.devId;
            uint32 srcId = pUsrCtxt->m_inputCfg.srcId;
            uint32 currentMode = 0;  //Todo: This needs to be set and queried

            result = CameraDeviceControl(m_InputDevices[devId].hDevice,
                    Camera_Sensor_AEEUID_CTL_INFO_SUBCHANNELS,
                    NULL, 0,
                    &m_InputDevices[devId].subchannelsInfo,
                    sizeof(m_InputDevices[devId].subchannelsInfo), NULL);

            if (CAMERA_SUCCESS == result)
            {
                result = GetModeInfo(pUsrCtxt,
                        currentMode, pModeInfo);
            }

            CAM_MSG_ON_ERR((CAMERA_SUCCESS != result),
                "Error retrieving input mode info for devId %d srcId %d result=%d",
                devId, srcId, result);
        }
        break;
        case AIS_INPUT_CTRL_FIELD_TYPE:
        {
            uint32 devId = pUsrCtxt->m_inputCfg.devId;
            result = CameraDeviceControl(m_InputDevices[devId].hDevice,
                Camera_Sensor_AEEUID_CTL_QUERY_FIELD,
                NULL, 0,
                param, sizeof(ais_field_info_t), NULL);
            break;
        }
        default:
            CAM_MSG (ERROR, "%s unsupported CTRL_ID %d",__func__, nCtrl);
            result = CAMERA_EBADPARM;
            break;
    }


    return result;
}


//////////////////////////////////////////////////////////////////////////////////
/// FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////
CameraResult AisInputConfigurer::GetModeInfo(AisUsrCtxt* pUsrCtxt,
                                             uint32 mode, ais_input_mode_info_t* pModeInfo)
{
    CameraResult rc = CAMERA_ERESOURCENOTFOUND;
    uint32 i;
    uint32 devId = pUsrCtxt->m_inputCfg.devId;
    uint32 srcId = pUsrCtxt->m_inputCfg.srcId;

    if (devId >= MAX_CAMERA_INPUT_DEVICES)
    {
        CAM_MSG(ERROR, "Invalid input devId %d srcId %d");
        return CAMERA_EBADPARM;
    }

    for (i = 0; i < m_InputDevices[devId].subchannelsInfo.num_subchannels; i++)
    {
        if (m_InputDevices[devId].subchannelsInfo.subchannels[i].src_id == srcId)
        {
            pModeInfo->width = m_InputDevices[devId].subchannelsInfo.subchannels[i].modes[mode].res.width;
            pModeInfo->height = m_InputDevices[devId].subchannelsInfo.subchannels[i].modes[mode].res.height;
            pModeInfo->fps = m_InputDevices[devId].subchannelsInfo.subchannels[i].modes[mode].res.fps;
            pModeInfo->vc = m_InputDevices[devId].subchannelsInfo.subchannels[i].modes[mode].channel_info.vc;
            pModeInfo->dt = m_InputDevices[devId].subchannelsInfo.subchannels[i].modes[mode].channel_info.dt;
            pModeInfo->fmt = m_InputDevices[devId].subchannelsInfo.subchannels[i].modes[mode].fmt;
            pModeInfo->interlaced = m_InputDevices[devId].subchannelsInfo.subchannels[i].modes[mode].interlaced;
            rc = CAMERA_SUCCESS;
            break;
        }
    }

    return rc;
}



uint32 AisInputConfigurer::GetDeviceId(uint32 device_id)
{
    uint32 devId;

    for (devId = 0; devId < m_nInputDevices; ++devId)
    {
        if (m_InputDevices[devId].device_id == device_id)
        {
            return devId;
        }
    }

    return MAX_CAMERA_INPUT_DEVICES;
}

CameraResult AisInputConfigurer::InputDeviceCallback(void* pClientData,
        uint32 uidEvent, int nEventDataLen, void* pEventData)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisInputConfigurer* pCtxt = (AisInputConfigurer*)pClientData;

    if (nEventDataLen != sizeof(input_message_t))
    {
        CAM_MSG(ERROR, "Received input callback with incorrect size=%d", nEventDataLen);
        return CAMERA_EBADPARM;
    }
    else if (!pCtxt)
    {
        CAM_MSG(ERROR, "Received input callback with null data");
        return CAMERA_EMEMPTR;
    }

    CAM_MSG(LOW, "Received %d", uidEvent);

    switch (uidEvent)
    {
    case INPUT_MSG_ID_LOCK_STATUS:
    {
        input_message_t * port_status_message = NULL;
        ais_engine_event_msg_t msg;

        port_status_message = (input_message_t *)pEventData;
        if (port_status_message != NULL)
        {
             CAM_MSG(HIGH, "LOCK_STATUS for source %d is %d for device-id 0x%x",
                   port_status_message->payload.src, port_status_message->payload.event, port_status_message->device_id);

             msg.event_id = AIS_EVENT_INPUT_STATUS;
             msg.payload.input_status.dev_id = pCtxt->GetDeviceId(port_status_message->device_id);
             msg.payload.input_status.signal_status =((port_status_message->payload.event == INPUT_SIGNAL_VALID_EVENT)
                                                    ? QCARCAM_INPUT_SIGNAL_VALID : QCARCAM_INPUT_SIGNAL_LOST);
             msg.payload.input_status.src_id = port_status_message->payload.src;

             rc = AisEngine::GetInstance()->QueueEvent(&msg);
        }
        else
        {
             CAM_MSG(ERROR, "Invalid imaging input message");
        }

        break;
    }
    case INPUT_MSG_ID_MODE_CHANGE:
    {
        CAM_MSG(ERROR, "Received INPUT_MSG_ID_MODE_CHANGE");
        break;
    }
    case INPUT_MSG_ID_FATAL_ERROR:
    {
        CAM_MSG(ERROR, "Received INPUT_MSG_ID_FATAL_ERROR");
        break;
    }
    default:
        CAM_MSG(ERROR, "Received unknown input msg id %d", uidEvent);
        break;
    }

    return rc;
}

/**
 * ais_config_input_detect_all
 *
 * @brief Detects all available devices
 *
 * @return CameraResult
 */
CameraResult AisInputConfigurer::DetectAll()
{
    CameraResult result = CAMERA_SUCCESS;
    uint32 device;

    for (device = 0;
         device < m_nInputDevices;
         ++device)
    {
        //check that device was successfully opened and chip detected
        if (!m_InputDevices[device].hDevice ||
                m_InputDevices[device].state != AIS_INPUT_STATE_DETECTED)
            continue;

        result = CameraDeviceControl(m_InputDevices[device].hDevice,
                Camera_Sensor_AEEUID_CTL_DETECT_DEVICE_CHANNELS,
                NULL, 0, &m_InputDevices[device].src_id_enable_mask,
                sizeof(m_InputDevices[device].src_id_enable_mask), NULL);
        if (CAMERA_SUCCESS != result)
        {
            CAM_MSG(HIGH, "Could not detect channels for sensor with ID: 0x%x",
                    m_InputDevices[device].sDeviceInfo.deviceID);
            continue;
        }
        CAM_MSG(MEDIUM, "src id enable mask = %x\n", m_InputDevices[device].src_id_enable_mask);

        result = CameraDeviceControl(m_InputDevices[device].hDevice,
                Camera_Sensor_AEEUID_CTL_INFO_CHANNELS,
                NULL,
                0,
                &m_InputDevices[device].channelsInfo,
                sizeof(m_InputDevices[device].channelsInfo),
                NULL);
        if (CAMERA_SUCCESS == result)
        {
            result = CameraDeviceControl(m_InputDevices[device].hDevice,
                    Camera_Sensor_AEEUID_CTL_INFO_SUBCHANNELS,
                    NULL,
                    0,
                    &m_InputDevices[device].subchannelsInfo,
                    sizeof(m_InputDevices[device].subchannelsInfo),
                    NULL);
        }

        if (CAMERA_SUCCESS == result)
        {
            uint32 i, j;
            m_InputDevices[device].available = TRUE;
            m_InputDevices[device].state = AIS_INPUT_STATE_OFF;

            for (i=0; i<MAX_CAMERA_INPUT_CHANNELS; i++)
            {
                if (m_InputDevices[device].device_id != m_InputMappingTable[i].device_id)
                    continue;

                for (j=0; j<m_InputDevices[device].subchannelsInfo.num_subchannels; j++)
                {
                    if (m_InputDevices[device].subchannelsInfo.subchannels[j].src_id ==
                            m_InputMappingTable[i].src_id)
                    {
                        m_InputMappingTable[i].available = TRUE;
                        break;
                    }
                }
                if (j == m_InputDevices[device].subchannelsInfo.num_subchannels)
                {
                    CAM_MSG(ERROR, "Cannot match dev 0x%x src %d to sensor subchannels",
                            m_InputMappingTable[i].device_id, m_InputMappingTable[i].src_id);
                }
            }
        }

        if (CAMERA_SUCCESS == result)
        {
            result = CameraDeviceControl(m_InputDevices[device].hDevice,
                    Camera_Sensor_AEEUID_CTL_STATE_POWER_ON,
                    NULL, 0, NULL, 0, NULL);
            CAM_MSG_ON_ERR((CAMERA_SUCCESS != result), "Failed to power on input device %d rc=%d", device, result);
            if (CAMERA_SUCCESS == result)
            {
                m_InputDevices[device].state = AIS_INPUT_STATE_ON;
            }
        }
    }

    return CAMERA_SUCCESS;
}

/**
 * IsInputAvailable
 *
 * @brief Check if qcarcam input id is available to use
 *
 * @param input_id
 *
 * @return CameraResult
 */
CameraResult AisInputConfigurer::IsInputAvailable(uint32 input_id)
{
    CameraResult rc = CAMERA_ERESOURCENOTFOUND;
    uint32 idx;

    for (idx=0; idx<m_nInputMapping; idx++)
    {
        if (input_id == m_InputMappingTable[idx].input_id)
        {
            rc = (m_InputMappingTable[idx].available) ? CAMERA_SUCCESS : CAMERA_EFAILED;
            break;
        }
    }

    return rc;
}

/**
 * ais_config_input_get_interface
 *
 * @brief Returns input interface parameters of particular qcarcam input id
 *
 * @param interf - interf->input_id specifies which input id to look up
 *
 * @return CameraResult
 */
CameraResult AisInputConfigurer::GetInterface(ais_input_interface_t* interf)
{
    CameraResult rc = CAMERA_ERESOURCENOTFOUND;
    uint32 idx, devId, j;

    for (idx=0; idx<m_nInputMapping; idx++)
    {
        if (interf->input_id == m_InputMappingTable[idx].input_id)
        {
            devId = m_InputMappingTable[idx].dev_id;

            /*get cid*/
            for (j=0; j<m_InputDevices[devId].subchannelsInfo.num_subchannels; j++)
            {
                if (m_InputDevices[devId].subchannelsInfo.subchannels[j].src_id ==
                        m_InputMappingTable[idx].src_id)
                {
                    interf->devId = devId;
                    interf->srcId = m_InputMappingTable[idx].src_id;
                    interf->cid = m_InputDevices[devId].subchannelsInfo.subchannels[j].modes[0].channel_info.cid;
                    interf->csid = m_InputDevices[devId].csid;
                    interf->csiphy = m_InputDevices[devId].csiphy;
                    interf->interlaced = m_InputDevices[devId].subchannelsInfo.subchannels[j].modes[0].interlaced;
                    return CAMERA_SUCCESS;
                }
            }
        }
    }

    return rc;
}

/**
 * ais_config_input_query
 *
 * @brief Query available inputs
 *
 * @param p_inputs
 * @param size - size of p_inputs array
 * @param filled_size - If p_inputs is set, number of elements in array that were filled
 *                      If p_inputs is NULL, number of available inputs to query
 *
 * @return CameraResult
 */
CameraResult AisInputConfigurer::QueryInputs(qcarcam_input_t* p_inputs, unsigned int size,
        unsigned int* ret_size)
{
    CameraResult rc = CAMERA_SUCCESS;
    int qcarcam_version_server = (QCARCAM_VERSION_MAJOR << 8) | (QCARCAM_VERSION_MINOR);
    uint32 idx, dst_idx, device, j, k;

    if (!p_inputs)
    {
        dst_idx = 0;
        for (idx=0; idx<m_nInputMapping; idx++)
        {
            if (m_InputMappingTable[idx].available)
            {
                dst_idx++;
            }
        }
        CAM_MSG(MEDIUM, "counted_size=%d", dst_idx);
        *ret_size = dst_idx;
        return CAMERA_SUCCESS;
    }

    for (idx=0, dst_idx=0; idx<m_nInputMapping && dst_idx < size; idx++)
    {
        /*do not advertise if it is not available*/
        if (!m_InputMappingTable[idx].available)
            continue;

        p_inputs[dst_idx].desc = m_InputMappingTable[idx].input_id;
        CAM_MSG(MEDIUM, "%d - input_desc=%d", dst_idx, p_inputs[dst_idx].desc);

        device = m_InputMappingTable[idx].dev_id;

        /*get cid*/
        for (j=0; j<m_InputDevices[device].subchannelsInfo.num_subchannels; j++)
        {
            if (m_InputDevices[device].subchannelsInfo.subchannels[j].src_id ==
                    m_InputMappingTable[idx].src_id)
            {
                for (k=0; k<m_InputDevices[device].subchannelsInfo.subchannels[j].num_modes; k++)
                {
                    p_inputs[dst_idx].res[k].width = m_InputDevices[device].subchannelsInfo.subchannels[j].modes[k].res.width;
                    p_inputs[dst_idx].res[k].height = m_InputDevices[device].subchannelsInfo.subchannels[j].modes[k].res.height;
                    p_inputs[dst_idx].res[k].fps = m_InputDevices[device].subchannelsInfo.subchannels[j].modes[k].res.fps;

                    p_inputs[dst_idx].color_fmt[k] = m_InputDevices[device].subchannelsInfo.subchannels[j].modes[k].fmt;
                    CAM_MSG(MEDIUM, "\t\t mode[%d]: %dx%d fmt:0x%08x fps:%f", k,
                            p_inputs[dst_idx].res[k].width, p_inputs[dst_idx].res[k].height,
                            p_inputs[dst_idx].color_fmt[k], p_inputs[dst_idx].res[k].fps);
                }
                p_inputs[dst_idx].num_color_fmt = k;
                p_inputs[dst_idx].num_res = k;
                p_inputs[dst_idx].flags = qcarcam_version_server;
                dst_idx++;
                CAM_MSG(MEDIUM, "\t\t num_modes=%d", k);
                break;
            }
        }
    }

    CAM_MSG(MEDIUM, "filled_size=%d", dst_idx);
    *ret_size = dst_idx;

    return rc;
}

/**
 * ais_config_validate_input_id
 *
 * @brief Validate input id based on connected sensors
 *
 * @param hndl - user context
 * @param CTRL_ID
 * @param param
 *
 * @return CameraResult
 */
CameraResult AisInputConfigurer::ValidateInputId(AisUsrCtxt* pUsrCtxt)
{
    CameraResult result = CAMERA_EBADPARM;


    if (pUsrCtxt)
    {
        if (m_InputDevices[pUsrCtxt->m_inputCfg.devId].src_id_enable_mask & (1 << pUsrCtxt->m_inputCfg.srcId))
        {
            result = CAMERA_SUCCESS;
        }
    }

    return result;
}

