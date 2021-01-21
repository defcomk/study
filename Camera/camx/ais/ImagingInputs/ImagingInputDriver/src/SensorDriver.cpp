/**
 * @file SensorDriver.c
 *
 * @brief SensorDriver Implementation
 *
 * Copyright (c) 2014-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
           INCLUDE FILES FOR MODULE
=========================================================================== */
#include "SensorPlatform.h"
#include "SensorDriver.h"
#include "CameraOSServices.h"
#include "SensorDebug.h"

/*===========================================================================
 Macro Definitions
===========================================================================*/

/* ===========================================================================
**                          Internal Helper Functions
** =========================================================================*/

static int SensorDriver_SlaveReadRegister(void* ctrl, unsigned short slave_addr,
        struct camera_i2c_reg_setting *reg_setting)
{
    SensorDriver* pSensorDriver = (SensorDriver*)ctrl;
    struct camera_reg_settings_t read_reg;
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    std_memset(&read_reg, 0, sizeof(read_reg));

    read_reg.i2c_operation = CAMERA_I2C_OP_READ;
    read_reg.addr_type = reg_setting->addr_type;
    read_reg.data_type = reg_setting->data_type;
    read_reg.reg_addr = reg_setting->reg_array[0].reg_addr;
    read_reg.delay = reg_setting->reg_array[0].delay;

    result = pSensorDriver->m_pSensorPlatform->SensorSlaveI2cRead(slave_addr, &read_reg);
    if (CAMERA_SUCCESS == result)
    {
        reg_setting->reg_array[0].reg_data = read_reg.reg_data;
    }

    SENSOR_FUNCTIONEXIT("");

    return result;
}

static int SensorDriver_SlaveWriteRegisters(void* ctrl, unsigned short slave_addr,
        struct camera_i2c_reg_setting *reg_setting)
{
    SensorDriver* pSensorDriver = (SensorDriver*)ctrl;
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    result = pSensorDriver->m_pSensorPlatform->SensorSlaveWriteI2cSetting(slave_addr, reg_setting);

    SENSOR_FUNCTIONEXIT("");

    return result;
}

static int SensorDriver_ExecutePowerSetting(void* ctrl,
        struct camera_power_setting *power_settings, unsigned short nSize)
{
    SensorDriver* pSensorDriver = (SensorDriver*)ctrl;
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    result = pSensorDriver->m_pSensorPlatform->SensorExecutePowerSetting(power_settings, nSize);

    SENSOR_FUNCTIONEXIT("");

    return result;
}

static int SensorDriver_SetupGpioInterrupt(void* ctrl,
        enum camera_gpio_type gpio_id, sensor_intr_callback_t cb, void *data)
{
    SensorDriver* pSensorDriver = (SensorDriver*)ctrl;
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    result = pSensorDriver->m_pSensorPlatform->SensorSetupGpioInterrupt(gpio_id, cb, data);

    SENSOR_FUNCTIONEXIT("");

    return result;
}

static int SensorDriver_InputStatusCallback(void* ctrl, struct camera_input_status_t *status)
{
    CameraResult result = CAMERA_SUCCESS;

    SensorDriver* pSensorDriver = (SensorDriver*)ctrl;
    CameraSensorDevice * sensorDeviceContext = pSensorDriver->m_pDeviceContext;
    input_message_t input_message;

    SENSOR_FUNCTIONENTRY("");

    if(status == NULL || sensorDeviceContext == NULL )
    {
        SENSOR_ERROR("Invalid Sensor or Port status context");
        result = CAMERA_EFAILED;
    }
    else
    {
        std_memset(&input_message, 0, sizeof(input_message));
        input_message.message_id = INPUT_MSG_ID_LOCK_STATUS;
        input_message.payload = *status;

        sensorDeviceContext->SendCallback(&input_message);
    }

    SENSOR_FUNCTIONEXIT("");

    return result;
}


/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::PowerOn
* DESCRIPTION Power On. Return TRUE if success
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::PowerOn()
{
    CameraResult result = CAMERA_EFAILED;
    SENSOR_FUNCTIONENTRY("");

    if (m_pSensorLib->use_sensor_custom_func &&
            m_pSensorLib->sensor_custom_func.sensor_power_on)
    {
        if (0 == m_pSensorLib->sensor_custom_func.sensor_power_on((void*)m_pSensorLib))
        {
            result = CAMERA_SUCCESS;
        }
    }
    else
    {
        result = m_pSensorPlatform->SensorPowerUp();
    }

    SENSOR_FUNCTIONEXIT("");
    return result;

} /* SensorDriver::PowerOn */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::PowerOff
* DESCRIPTION Power Off. Return TRUE if detection
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::PowerOff()
{
    CameraResult result = CAMERA_EFAILED;

    SENSOR_FUNCTIONENTRY("");

    if (m_pSensorLib->use_sensor_custom_func &&
            m_pSensorLib->sensor_custom_func.sensor_power_off)
    {
        if (0 == m_pSensorLib->sensor_custom_func.sensor_power_off((void*)m_pSensorLib))
        {
            result = CAMERA_SUCCESS;
        }
    }
    else
    {
        result = m_pSensorPlatform->SensorPowerDown();
    }

    SENSOR_FUNCTIONEXIT("");

    return result;
} /* SensorDriver::PowerOff */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::PowerSuspend
* DESCRIPTION Power Off. Return TRUE if detection
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::PowerSuspend()
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    if (m_pSensorLib->use_sensor_custom_func &&
            m_pSensorLib->sensor_custom_func.sensor_power_suspend)
    {
        if (0 != m_pSensorLib->sensor_custom_func.sensor_power_suspend((void*)m_pSensorLib))
        {
            result = CAMERA_EFAILED;
        }
    }

    if (CAMERA_SUCCESS == result)
    {
        result = m_pSensorPlatform->SensorPowerSuspend();
    }

    SENSOR_FUNCTIONEXIT("");

    return result;
} /* SensorDriver::PowerSuspend */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::PowerResume
* DESCRIPTION Power Off. Return TRUE if detection
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::PowerResume()
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");
    result = m_pSensorPlatform->SensorPowerResume();
    if (CAMERA_SUCCESS == result)
    {
        if (m_pSensorLib->use_sensor_custom_func &&
                m_pSensorLib->sensor_custom_func.sensor_power_resume)
        {
            if (0 == m_pSensorLib->sensor_custom_func.sensor_power_resume((void*)m_pSensorLib))
            {
                result = CAMERA_SUCCESS;
            }
            else
            {
                result = CAMERA_EFAILED;
            }
        }
    }

    SENSOR_FUNCTIONEXIT("");

    return result;
} /* SensorDriver::PowerResume */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::DetectDevice
* DESCRIPTION Detect Device. Return TRUE if detection
* is not required.
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::DetectDevice()
{
    CameraResult result = CAMERA_EFAILED;

    SENSOR_FUNCTIONENTRY("");

    if (m_pSensorLib->use_sensor_custom_func && m_pSensorLib->sensor_custom_func.sensor_detect_device)
    {
        if (0 == m_pSensorLib->sensor_custom_func.sensor_detect_device((void*)m_pSensorLib))
        {
            //successfully probed device
            result = CAMERA_SUCCESS;
        }
    }
    else
    {
        struct camera_reg_settings_t read_reg;
        byte SlaveAddr = m_pSensorLib->sensor_slave_info.slave_addr;
        uint16 RegAddr = m_pSensorLib->sensor_slave_info.sensor_id_info.sensor_id_reg_addr;
        uint32 ExpectedModelID = m_pSensorLib->sensor_slave_info.sensor_id_info.sensor_id;

        std_memset(&read_reg, 0, sizeof(read_reg));

        read_reg.i2c_operation = CAMERA_I2C_OP_READ;
        read_reg.addr_type = m_pSensorLib->sensor_slave_info.addr_type;
        read_reg.data_type = m_pSensorLib->sensor_slave_info.data_type;
        read_reg.reg_addr = RegAddr;
        read_reg.delay = 0;

        SENSOR_DEBUG("SlaveAddr = 0x%x RegAddr = 0x%x  ModelId = 0x%x", SlaveAddr, RegAddr, ExpectedModelID);

        m_pSensorPlatform->SensorSlaveI2cRead(SlaveAddr, &read_reg);

        SENSOR_ERROR("ReadBack 0x%x, Expected 0x%x", read_reg.reg_data, ExpectedModelID);

        if (read_reg.reg_data == ExpectedModelID)
        {
            result = CAMERA_SUCCESS; // Sensor Detected Successfully
            SENSOR_LOG("ModelID = 0x%x Detected", read_reg.reg_data);
        }
        else
        {
            SENSOR_ERROR("Failed to Detect Sensor with Reference ID 0x%x, read back 0x%x", ExpectedModelID, read_reg.reg_data);
        }
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;

} /* SensorDriver::DetectDevice */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::DetectDeviceChannels
* DESCRIPTION Detect Device Channels. Return TRUE if detection
* is not required.
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::DetectDeviceChannels()
{
    CameraResult result = CAMERA_EFAILED;

    SENSOR_FUNCTIONENTRY("");

    if (m_pSensorLib->use_sensor_custom_func &&m_pSensorLib->sensor_custom_func.sensor_detect_device_channels)
    {
       if (0 == m_pSensorLib->sensor_custom_func.sensor_detect_device_channels((void*)m_pSensorLib))
        {
            //successfully probed device
            result = CAMERA_SUCCESS;
        }
    }
     else
    {
        result = CAMERA_SUCCESS;
    }

    return result;
}

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::InitializeRegisters
* DESCRIPTION Registers initialization
* is not required.
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::InitializeRegisters()
{
    CameraResult result = CAMERA_EFAILED;
    byte SlaveAddr;
    uint32 iInitSettingCount = 0;

    SENSOR_FUNCTIONENTRY("");

    if (m_pSensorLib->use_sensor_custom_func &&
            m_pSensorLib->sensor_custom_func.sensor_init_setting)
    {
        if (0 == m_pSensorLib->sensor_custom_func.sensor_init_setting((void*) m_pSensorLib))
        {
            result = CAMERA_SUCCESS;
        }
    }
    else
    {
        SlaveAddr = (byte)m_pSensorLib->sensor_slave_info.slave_addr;
        result = CAMERA_SUCCESS;
        for (iInitSettingCount = 0; iInitSettingCount < m_pSensorLib->init_settings_array.size; iInitSettingCount++)
        {
            SENSOR_DEBUG("Array[%d] ", iInitSettingCount);

            if (0 != m_pSensorPlatform->SensorSlaveWriteI2cSetting(SlaveAddr, &m_pSensorLib->init_settings_array.reg_settings[iInitSettingCount]))
            {
                result = CAMERA_EFAILED;
            }
        }
    }

    SENSOR_FUNCTIONEXIT("");

    return result;

} /* SensorDriver::InitializeRegisters */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::SetSensorMode
* DESCRIPTION Set sensor mode
* DEPENDENCIES None
* PARAMETERS this, mode to set
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::SetSensorMode(uint32 mode)
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    SENSOR_DEBUG("mode = %d", mode);

    if (mode >= MAX_RESOLUTION_MODES)
    {
        SERR("invalid mode %d", mode);
        result = CAMERA_EBADPARM;
    }

    else
    {
        m_CurrentSensorMode = mode;

        if (m_pSensorLib->use_sensor_custom_func &&
                m_pSensorLib->sensor_custom_func.sensor_set_channel_mode)
        {
            /*set default mode*/
            if (0 != m_pSensorLib->sensor_custom_func.sensor_set_channel_mode(
                    (void*) m_pSensorLib, 0xff, m_CurrentSensorMode))
            {
                result = CAMERA_EFAILED;
            }
        }
        else
        {

            if (0 == m_pSensorPlatform->SensorSlaveWriteI2cSetting(m_pSensorLib->sensor_slave_info.slave_addr,
                    &m_pSensorLib->res_settings_array.reg_settings[mode]))
            {
                SENSOR_DEBUG("Mode%d done", mode);
            }
            else
            {
                SENSOR_ERROR("Mode%d failure", mode);
                result = CAMERA_EFAILED;
            }
        }
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* SensorDriver::SetSensorMode */


/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::StartStream
* DESCRIPTION Start streaming.
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::StartStream()
{
    CameraResult result = CAMERA_EFAILED;

    SENSOR_FUNCTIONENTRY("");

    if (m_pSensorLib->use_sensor_custom_func &&
            m_pSensorLib->sensor_custom_func.sensor_start_stream)
    {
        if (0 == m_pSensorLib->sensor_custom_func.sensor_start_stream(
                (void*)m_pSensorLib, 0xff))
        {
            result = CAMERA_SUCCESS;
        }
    }
    else
    {
        if (0 == m_pSensorPlatform->SensorSlaveWriteI2cSetting(
                        m_pSensorLib->sensor_slave_info.slave_addr,
                        &m_pSensorLib->start_settings))
        {
            result = CAMERA_SUCCESS;
        }
    }

    SENSOR_FUNCTIONEXIT("");

    return result;

} /* SensorDriver::StartStream */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::StopStream
* DESCRIPTION Stop stream.
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::StopStream()
{
    CameraResult result = CAMERA_EFAILED;

    SENSOR_FUNCTIONENTRY("");

    if (m_pSensorLib->use_sensor_custom_func &&
            m_pSensorLib->sensor_custom_func.sensor_stop_stream)
    {
        if (0 == m_pSensorLib->sensor_custom_func.sensor_stop_stream(
                (void*)m_pSensorLib, 0xff))
        {
            //successfully stopped streams
            result = CAMERA_SUCCESS;
        }
    }
    else
    {
        if (0 == m_pSensorPlatform->SensorWriteI2cSetting(&m_pSensorLib->stop_settings))
        {
            result = CAMERA_SUCCESS;
        }
    }

    SENSOR_FUNCTIONEXIT("");

    return result;
} /* SensorDriver::StopStream */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::ConfigResolution
* DESCRIPTION Config Sensor Resolution returned by BA
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::ConfigResolution(Camera_Size* pResConfig)
{
    CameraResult result = CAMERA_EFAILED;

    SENSOR_FUNCTIONENTRY("");
    Camera_Size *resConfig = (Camera_Size*)pResConfig;

    if (m_pSensorLib->use_sensor_custom_func &&
            m_pSensorLib->sensor_custom_func.sensor_config_resolution)
    {
        if (0 == m_pSensorLib->sensor_custom_func.sensor_config_resolution(
                (void*)m_pSensorLib, resConfig->width,resConfig->height))
        {
            //successfully set resolution
            result = CAMERA_SUCCESS;
        }
    }

    SENSOR_FUNCTIONEXIT("");

    return result;
} /* SensorDriver::ConfigResolution */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::QueryField
* DESCRIPTION Get Field type returned by low level driver
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::QueryField(Camera_Sensor_FieldType* pFieldType)
{
    CameraResult result = CAMERA_EFAILED;
    boolean even_field;
    uint64_t field_ts;

    SENSOR_FUNCTIONENTRY("");

    if (m_pSensorLib->use_sensor_custom_func &&
            m_pSensorLib->sensor_custom_func.sensor_query_field)
    {
        if (0 == m_pSensorLib->sensor_custom_func.sensor_query_field(
                (void*)m_pSensorLib, &even_field, &field_ts))
        {
            //successfully get field info
            result = CAMERA_SUCCESS;
            pFieldType->even_field = even_field;
            pFieldType->field_ts = field_ts;
        }
    }

    SENSOR_FUNCTIONEXIT("");

    return result;
} /* SensorDriver::ConfigResolution */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::ConfigExposure
* DESCRIPTION calculate exposures and updates exposures through settings
* DEPENDENCIES None
* PARAMETERS Register Gain and Exposure  time, and settings
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::ConfigExposure(Camera_Sensor_ParamConfigType* pParamConfig)
{
    CameraResult result = CAMERA_SUCCESS;
    sensor_exposure_info_t exposure_info;

    if(NULL == pParamConfig)
    {
        return CAMERA_EFAILED;
    }

    Camera_Sensor_ExposureConfigType* pExposureConfig =
        &pParamConfig->sensor_params.sensor_exposure_config;

    std_memset(&exposure_info, 0, sizeof(sensor_exposure_info_t));
    exposure_info.exposure_mode_type = pExposureConfig->exposure_mode_type;

    if (!(m_pSensorLib->sensor_capability & (1 << SENSOR_CAPABILITY_EXPOSURE_CONFIG)))
    {
        SENSOR_ERROR("Exposure control not supported");
        return CAMERA_EUNSUPPORTED;
    }

    if (exposure_info.exposure_mode_type == QCARCAM_EXPOSURE_MANUAL)
    {
        exposure_info.real_gain = pExposureConfig->fGain;
        exposure_info.exposure_time = pExposureConfig->integrationParam.fExposureTime;
        SENSOR_HIGH("Manual exposure gain=%f, time=%f", exposure_info.real_gain, exposure_info.exposure_time);
        /** Calculate exposure params and set it.
         */
        if (m_pSensorLib->exposure_func_table.sensor_calculate_exposure)
        {
            m_pSensorLib->exposure_func_table.sensor_calculate_exposure(
                    (void*) m_pSensorLib, pExposureConfig->src_id, &exposure_info);
        }
        else
        {
            SENSOR_ERROR("Manual exposure calculation not supported");
            return CAMERA_EUNSUPPORTED;
        }
    }

    if (m_pSensorLib->exposure_func_table.sensor_exposure_config)
    {
        if (m_pSensorLib->exposure_func_table.sensor_exposure_config(
                (void*) m_pSensorLib, pExposureConfig->src_id, &exposure_info))
        {
            SENSOR_ERROR("Failed to set exposure configuration");
            result = CAMERA_EFAILED;
        }
    }
    else
    {
        SENSOR_ERROR("Exposure control not supported");
        result = CAMERA_EUNSUPPORTED;
    }

    return result;
}

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::ConfigColorParam
* DESCRIPTION Update color settings
* DEPENDENCIES None
* PARAMETERS Saturation value to be set, settings
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::ConfigColorParam(Camera_Sensor_ParamConfigType* pParamConfig)
{
    CameraResult result = CAMERA_SUCCESS;

    if(NULL == pParamConfig)
    {
        return CAMERA_EFAILED;
    }

    if (m_pSensorLib->use_sensor_custom_func
        && m_pSensorLib->sensor_custom_func.sensor_s_param
        && (m_pSensorLib->sensor_capability & (1 << SENSOR_CAPABILITY_COLOR_CONFIG)))
    {
           if(m_pSensorLib->sensor_custom_func.sensor_s_param(
                (void*)m_pSensorLib,
                pParamConfig->param,
                pParamConfig->sensor_params.sensor_color_config.src_id,
                &pParamConfig->sensor_params.sensor_color_config.nVal))
           {
               SENSOR_ERROR("Failed to set color param");
               result = CAMERA_EFAILED;
           }
    }
    else
    {
        SENSOR_ERROR("Color param not supported");
        result = CAMERA_EUNSUPPORTED;
    }

    return result;
}

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::ConfigParam
* DESCRIPTION Config parameter
* DEPENDENCIES None
* PARAMETERS Saturation value to be set, settings
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::ConfigParam(Camera_Sensor_ParamConfigType* pParamConfig)
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    if(NULL == pParamConfig)
    {
        return CAMERA_EBADPARM;
    }

    switch(pParamConfig->param)
    {
        case QCARCAM_PARAM_EXPOSURE:
        result = ConfigExposure(pParamConfig);
            break;
        case QCARCAM_PARAM_SATURATION:
        case QCARCAM_PARAM_HUE:
        result = ConfigColorParam(pParamConfig);
            break;
        default:
            break;
    }

    return result;
}


/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::Initialize
* DESCRIPTION Initialize the driver interface
* DEPENDENCIES None
* PARAMETERS this
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorDriver::Initialize(CameraSensorDevice* pCameraSensorDevice,
        sensor_lib_t *pSensorData)
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    m_pDeviceContext = pCameraSensorDevice;
    m_pSensorLib = pSensorData;
    if (pSensorData->use_sensor_custom_func &&
            pSensorData->sensor_custom_func.sensor_set_platform_func_table)
    {
        SENSOR_LOG("Custom probe");
        sensor_platform_func_table_t platform_func_table = {
                .i2c_read = NULL,
                .i2c_write_array = NULL,
                .i2c_slave_read = &SensorDriver_SlaveReadRegister,
                .i2c_slave_write_array = &SensorDriver_SlaveWriteRegisters,
                .execute_power_setting = &SensorDriver_ExecutePowerSetting,
                .setup_gpio_interrupt = &SensorDriver_SetupGpioInterrupt,
                .input_status_cb = &SensorDriver_InputStatusCallback,
        };
        pSensorData->sensor_custom_func.sensor_set_platform_func_table(
                pSensorData, &platform_func_table);
    }

    // Initialize sensor parameters using sensor data
    std_strlcpy(m_szSensorName, pSensorData->sensor_slave_info.sensor_name, STD_ARRAY_SIZE(m_szSensorName));

    SENSOR_DEBUG("m_szModelNameAndNumber = %s", m_szSensorName);

    m_pSensorPlatform = SensorPlatform::SensorPlatformInit(m_pSensorLib);
    if (!m_pSensorPlatform)
    {
        SENSOR_ERROR("Platform Initialization failed");
        result = CAMERA_EFAILED;
    }

    if(CAMERA_SUCCESS == result)
    {
        SENSOR_LOG("Success");
    }
    else
    {
        SENSOR_ERROR("Initialization failed");
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* SensorDriver::Initialize */

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver::Uninitialize
* DESCRIPTION UnInitialize the driver interface
* DEPENDENCIES None
* PARAMETERS None
* RETURN VALUE CameraResult
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */

CameraResult SensorDriver::Uninitialize()
{
    SENSOR_FUNCTIONENTRY("");

    m_pSensorLib = NULL;
    SensorPlatform::SensorPlatformDeinit(m_pSensorPlatform);

    SENSOR_FUNCTIONEXIT("");

    return CAMERA_SUCCESS;
} /* SensorDriver::Interface_UnInitialize */

