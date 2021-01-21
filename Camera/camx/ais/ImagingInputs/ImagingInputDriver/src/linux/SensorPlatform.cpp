/**
 * @file SensorPlatform.c
 *
 * @brief SensorPlatform Implementation
 *
 * Copyright (c) 2010-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <linux/media.h>
#include <media/ais_sensor.h>

#include "CameraOSServices.h"
#include "CameraPlatformLinux.h"
#include "SensorPlatform.h"
#include "SensorDebug.h"

/* ---------------------------------------------------------------------------
 ** Macro Definitions
 ** ------------------------------------------------------------------------ */

/* ===========================================================================
                DEFINITIONS AND DECLARATIONS FOR MODULE
=========================================================================== */

/* --------------------------------------------------------------------------
 ** Constant / Define Declarations
 ** ----------------------------------------------------------------------- */
#define MULTII2C_QUEUE_SIZE 64
#define MAX_INTERRUPT_GPIO_PER_INPUT 3

/* --------------------------------------------------------------------------
 ** Type Declarations
 ** ----------------------------------------------------------------------- */
class SensorPlatformLinux;

typedef struct
{
    enum camera_gpio_type gpio_id;
    uint32 nIRQ;
    byte isUsed;
    void* hThread;
    char  tname[64];
    sensor_intr_callback_t pCbFcn;
    void *pData;
    SensorPlatformLinux *pCtxt;
} sensor_platform_interrupt_t;

class SensorPlatformLinux : public SensorPlatform
{
public:
    SensorPlatformLinux(sensor_lib_t* p_sensor_lib) {
        this->m_pSensorLib = p_sensor_lib;
    }

    CameraResult Init();
    void Deinit();

    virtual CameraResult SensorPowerUp();
    virtual CameraResult SensorPowerDown();

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_write_i2c_setting_array -
     *
     * DESCRIPTION: Write I2C setting array
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorWriteI2cSettingArray(struct camera_i2c_reg_setting_array *settings);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_slave_write_i2c_setting -
     *
     * DESCRIPTION: Slave Write I2C setting array
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorSlaveWriteI2cSetting(unsigned short slave_addr,
        struct camera_i2c_reg_setting *setting);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_slave_read_i2c -
     *
     * DESCRIPTION: Slave read I2C reg
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorSlaveI2cRead(unsigned short slave_addr,
        struct camera_reg_settings_t *setting);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_write_i2c_setting -
     *
     * DESCRIPTION: Write I2C setting
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorWriteI2cSetting(struct camera_i2c_reg_setting *setting);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_execute_power_setting -
     *
     * DESCRIPTION: Execute Sensor power config sequence
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorExecutePowerSetting(struct camera_power_setting *power_settings, unsigned short nSize);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_setup_gpio_interrupt -
     *
     * DESCRIPTION: Setup gpio id as interrupt
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorSetupGpioInterrupt(enum camera_gpio_type gpio_id,
        sensor_intr_callback_t cb, void *data);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_power_resume -
     *
     * DESCRIPTION: Executes power resume sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerResume();

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_power_suspend -
     *
     * DESCRIPTION: Executes power suspend sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerSuspend();

    void sensor_platform_process_event(sensor_platform_interrupt_t *pPlatformIntr);

    static int sensor_platform_intr_poll_thread(void* arg);

private:
    sensor_lib_t* m_pSensorLib;

    int fd;
    CameraSensorIndex CameraInterface;

    // i2c/cci
    CameraSensorI2CMasterType I2CMasterIndex;
    byte I2CSlaveAddr;
    byte I2CRegisterSize;
    byte I2CDataSize;
    CameraSensorI2C_SpeedType I2CBusFrequency;

    // interrupt
    sensor_platform_interrupt_t interrupts[MAX_INTERRUPT_GPIO_PER_INPUT];
    /*power*/
    enum sensor_camera_id camera_id;
    struct ais_sensor_probe_cmd *probe_cmd;
};

typedef int (*sensor_platform_event_thread_t)(void *arg);

/*===========================================================================
 * FUNCTION - LOG_IOCTL -
 *
 * DESCRIPTION: Wrapper for logging and to trace ioctl calls.
 * ------------------------------------------------------------------------ */
#define ATRACE_BEGIN_SNPRINTF(...)
#define ATRACE_END(...)
static int LOG_IOCTL(int d, int request, void* par1, const char* trace_func)
{
    int ret;
    ATRACE_BEGIN_SNPRINTF(35,"Camera:sensorIoctl %s", trace_func);
    ret = ioctl(d, request, par1);
    ATRACE_END();
    return ret;
}


static uint8_t sensor_sdk_util_get_kernel_i2c_type(
    enum camera_i2c_reg_addr_type addr_type)
{
    switch (addr_type) {
    case CAMERA_I2C_BYTE_ADDR:
        return 1;
    case CAMERA_I2C_WORD_ADDR:
        return 2;
    case CAMERA_I2C_3B_ADDR:
        return 3;
    default:
        CAM_MSG(HIGH, "Invalid addr_type = %d", addr_type);
        return 0;
    }
}

static uint8_t sensor_sdk_util_get_kernel_i2c_type(
    enum camera_i2c_data_type data_type)
{
    switch (data_type) {
    case CAMERA_I2C_BYTE_DATA:
        return 1;
    case CAMERA_I2C_WORD_DATA:
        return 2;
    case CAMERA_I2C_DWORD_DATA:
        return 4;
    default:
        CAM_MSG(HIGH, "Invalid data_type = %d", data_type);
        return 0;
    }
}

static uint16_t sensor_sdk_util_get_kernel_power_seq_type(
    enum camera_power_seq_type seq_type, long config_val)
{
    switch (seq_type) {
    case CAMERA_POW_SEQ_CLK:
        return 0; //MCLK
        break;
    case CAMERA_POW_SEQ_GPIO:
    case CAMERA_POW_SEQ_VREG:
        return config_val;
        break;
    case CAMERA_POW_SEQ_I2C:
    default:
        CAM_MSG(ERROR, "Invalid seq_type = %d, config_val = %d", seq_type, config_val);
        return -1;
    }
}

static void translate_sensor_reg_setting(
    struct ais_sensor_cmd_i2c_wr_array *setting_k,
    struct camera_i2c_reg_setting *setting_u)
{
    setting_k->count = setting_u->size;
    setting_k->wr_array =
        (struct ais_sensor_i2c_wr_payload *)setting_u->reg_array;
    setting_k->addr_type =
        sensor_sdk_util_get_kernel_i2c_type(setting_u->addr_type);
    setting_k->data_type =
        sensor_sdk_util_get_kernel_i2c_type(setting_u->data_type);
    //setting_k->delay = setting_u->delay;
}

static void translate_sensor_reg_setting_array(
    struct ais_sensor_cmd_i2c_wr_array *setting_k,
    struct camera_i2c_reg_setting_array *setting_u)
{
    setting_k->count = setting_u->size;
    setting_k->wr_array =
        (struct ais_sensor_i2c_wr_payload *)setting_u->reg_array;
    setting_k->addr_type =
        sensor_sdk_util_get_kernel_i2c_type(setting_u->addr_type);
    setting_k->data_type =
        sensor_sdk_util_get_kernel_i2c_type(setting_u->data_type);
    //setting_k->delay = setting_u->delay;
}

static void translate_sensor_read_reg(
    struct ais_sensor_cmd_i2c_read *setting_k,
    struct camera_reg_settings_t *setting_u)
{
    setting_k->reg_addr = setting_u->reg_addr;
    setting_k->addr_type =
        sensor_sdk_util_get_kernel_i2c_type(setting_u->addr_type);
    setting_k->data_type =
        sensor_sdk_util_get_kernel_i2c_type(setting_u->data_type);
}

static uint8_t sensor_sdk_util_get_i2c_freq_mode(
    enum camera_i2c_freq_mode i2c_freq_mode)
{
    switch (i2c_freq_mode){
    case SENSOR_I2C_MODE_STANDARD:
        return 0;
    case SENSOR_I2C_MODE_FAST:
        return 1;
    case SENSOR_I2C_MODE_CUSTOM:
        return 2;
    case SENSOR_I2C_MODE_FAST_PLUS:
        return 3;
    default:
        CAM_MSG(ERROR, "Invalid i2c_freq_mode = %d", i2c_freq_mode);
        return 4;
    }
}

static void translate_sensor_slave_info(
    struct ais_sensor_probe_cmd *probe_cmd,
    struct camera_sensor_slave_info *slave_info_u)
{
    uint16_t i = 0;

    struct ais_power_settings *power_up_setting_k = probe_cmd->power_config.power_up_setting;
    struct ais_power_settings *power_down_setting_k = probe_cmd->power_config.power_down_setting;

    struct camera_power_setting* power_up_array = slave_info_u->power_setting_array.power_up_setting_a;
    struct camera_power_setting* power_down_array = slave_info_u->power_setting_array.power_down_setting_a;

    probe_cmd->i2c_config.i2c_freq_mode = sensor_sdk_util_get_i2c_freq_mode(
        slave_info_u->i2c_freq_mode);
    probe_cmd->i2c_config.slave_addr = slave_info_u->slave_addr;

    probe_cmd->power_config.size_up =
        slave_info_u->power_setting_array.size_up;
    probe_cmd->power_config.size_down =
        slave_info_u->power_setting_array.size_down;

    if (probe_cmd->power_config.size_up > AIS_MAX_POWER_SEQ)
    {
        CAM_MSG(ERROR, "power up seq too long (%d). Will truncate to %d",
            probe_cmd->power_config.size_up, AIS_MAX_POWER_SEQ);
        probe_cmd->power_config.size_up = AIS_MAX_POWER_SEQ;
    }

    if (probe_cmd->power_config.size_down > AIS_MAX_POWER_SEQ)
    {
        CAM_MSG(ERROR, "power down seq too long (%d). Will truncate to %d",
            probe_cmd->power_config.size_down, AIS_MAX_POWER_SEQ);
        probe_cmd->power_config.size_down = AIS_MAX_POWER_SEQ;
    }

    for (i = 0; i < probe_cmd->power_config.size_up; i++) {
        power_up_setting_k[i].power_seq_type =
            sensor_sdk_util_get_kernel_power_seq_type(
                power_up_array[i].seq_type,
                power_up_array[i].seq_val);
        power_up_setting_k[i].config_val_low =
            power_up_array[i].config_val;
        power_up_setting_k[i].delay =
            power_up_array[i].delay;
    }

    for (i = 0; i < probe_cmd->power_config.size_down; i++) {
        power_down_setting_k[i].power_seq_type =
            sensor_sdk_util_get_kernel_power_seq_type(
                power_down_array[i].seq_type,
                power_down_array[i].seq_val);
        power_down_setting_k[i].config_val_low =
            power_down_array[i].config_val;
        power_down_setting_k[i].delay =
            power_down_array[i].delay;
    }
}

void SensorPlatformLinux::sensor_platform_process_event(sensor_platform_interrupt_t *pPlatformIntr)
{
    int rc;
    int i = 0;
    struct v4l2_event v4l2_event;
    struct ais_sensor_event_data *event_data = NULL;
    SensorPlatformLinux* pCtxt = this;

    memset(&v4l2_event, 0, sizeof(v4l2_event));
    rc = ioctl(pCtxt->fd, VIDIOC_DQEVENT, &v4l2_event);
    if (rc >= 0)
    {
        event_data = (struct ais_sensor_event_data *)v4l2_event.u.data;
        switch(v4l2_event.type) {
        case AIS_SENSOR_EVENT_TYPE:
        {
            CAM_MSG(ERROR, "Sensor platform slave_addr=0x%x", event_data->data[0]);
            //Read the register inside this callback
            pPlatformIntr->pCbFcn(pPlatformIntr->pData);
            break;
        }

        default:
            CAM_MSG(ERROR, "Sensor platform %d - Unknown event %d", pCtxt->CameraInterface, v4l2_event.type);
            break;
        }

    }
    else
    {
        CAM_MSG(ERROR, "Sensor platform VIDIOC_DQEVENT failed");
    }
}

/* ----------------------------------------------------------------------------
 *    FUNCTION        sensor_platform_intr_poll_thread
 *    DESCRIPTION
 *    DEPENDENCIES
 *    PARAMETERS
 *
 *    RETURN VALUE
 *    SIDE EFFECTS
 * ----------------------------------------------------------------------------
 */
int SensorPlatformLinux::sensor_platform_intr_poll_thread(void* arg)
{
    int rc;
    struct pollfd pollfds;
    sensor_platform_interrupt_t *pPlatformIntr = (sensor_platform_interrupt_t*)arg;
    SensorPlatformLinux* pCtxt = NULL;

    if (pPlatformIntr != NULL)
    {
        pCtxt = pPlatformIntr->pCtxt;
    }

    if (pCtxt == NULL)
    {
        //TODO: propagate error state
        CAM_MSG(ERROR,  "sensor event thread has NULL context");
        return -1;
    }

    CAM_MSG(ERROR,"%s: I'm running for instance ...", __FUNCTION__);
    pollfds.fd = pCtxt->fd;
    while (pPlatformIntr->isUsed == TRUE)
    {
        /* block infinitely to wait for interrupt or being cancelled */
        pollfds.events = POLLIN|POLLRDNORM|POLLPRI;
        rc = poll(&pollfds, 1, -1);
        if (rc <= 0)
        {
            CAM_MSG(ERROR, "%s: poll failed %d", __func__, rc);
            continue;
        }
        pCtxt->sensor_platform_process_event(pPlatformIntr);

    }

    CAM_MSG(MEDIUM,  "Sensor Platform %d Event thread is exiting...", pCtxt->CameraInterface);
    return 0;
}

/* ---------------------------------------------------------------------------
 *    FUNCTION        SensorPlatformInit
 *    DESCRIPTION     Initialize sensor platform with dev id
 *    DEPENDENCIES    None
 *    PARAMETERS
 *    RETURN VALUE    sensor platform ctxt ptr
 *    SIDE EFFECTS    None
 * ------------------------------------------------------------------------ */
SensorPlatform* SensorPlatform::SensorPlatformInit(sensor_lib_t* p_sensor_lib)
{
    CameraResult result = CAMERA_SUCCESS;

    SensorPlatformLinux* pCtxt = new SensorPlatformLinux(p_sensor_lib);
    if (pCtxt)
    {
        (void)pCtxt->Init();

        if (CAMERA_SUCCESS != result)
        {
            delete pCtxt;
            pCtxt = NULL;
        }
    }

    return pCtxt;
}

CameraResult SensorPlatformLinux::Init()
{
    CameraResult result = CAMERA_SUCCESS;
    SensorPlatformLinux* pCtxt = this;
    enum sensor_camera_id camera_id = m_pSensorLib->sensor_slave_info.camera_id;
    int csidCoreIdx = CameraPlatform_GetCsidCore((CameraSensorIndex)camera_id);
    CAM_MSG(HIGH, "SensorPlatform init camera_id %d csidCoreIdx %i",camera_id, csidCoreIdx);

    if (csidCoreIdx < 0)
    {
        CAM_MSG(ERROR, "camera_id %d invalid csidCoreIdx %i", camera_id, csidCoreIdx);
        result = CAMERA_EFAILED;
    }
    else
    {
        const char* path = CameraPlatformGetDevPath(AIS_SUBDEV_SENSOR, csidCoreIdx);

        pCtxt->camera_id = camera_id;
        if (path)
        {
            pCtxt->fd = open(path, O_RDWR | O_NONBLOCK);
            if (pCtxt->fd <= 0) {
                CAM_MSG(ERROR, "cannot open '%s'", path);
                result = CAMERA_EFAILED;
            }
        }
        else
        {
            CAM_MSG(ERROR, "sensor %d subdev not available", camera_id);
            result = CAMERA_EFAILED;
        }
    }

    if (CAMERA_SUCCESS == result)
    {
        boolean                              ret = TRUE;
        int32_t                              rc = 0, i;

        struct camera_power_setting_array *power_setting_array;
        power_setting_array = &m_pSensorLib->sensor_slave_info.
            power_setting_array;

        pCtxt->probe_cmd = (struct ais_sensor_probe_cmd *)CameraAllocate(
            CAMERA_ALLOCATE_ID_SENSOR_PLATFORM_CTXT,
            sizeof(*pCtxt->probe_cmd));

        if(!pCtxt->probe_cmd) {
            CAM_MSG(ERROR, "failed to malloc probe cmd for %s",
                m_pSensorLib->sensor_slave_info.sensor_name);
            result = CAMERA_ENOMEMORY;
        }
        else
        {
            int rc;
            CameraSensorGPIO_IntrPinType pinInfo;
            translate_sensor_slave_info(pCtxt->probe_cmd,
                &m_pSensorLib->sensor_slave_info);

            pCtxt->probe_cmd->gpio_intr_config[0].gpio_num = -1;
            if (CameraSensorGPIO_GetIntrPinInfo((CameraSensorIndex)camera_id, CAMERA_GPIO_INTR, &pinInfo))
            {
                pCtxt->probe_cmd->gpio_intr_config[0].gpio_num =
                    pinInfo.pin_id;
                switch (pinInfo.trigger)
                {
                case 0: /* trigger NONE */
                    pCtxt->probe_cmd->gpio_intr_config[0].gpio_cfg0 =
                        0x00000000;
                    break;
                case 1: /* trigger RISING */
                    pCtxt->probe_cmd->gpio_intr_config[0].gpio_cfg0 =
                        0x00000001;
                    break;
                case 2: /* trigger FALLING */
                    pCtxt->probe_cmd->gpio_intr_config[0].gpio_cfg0 =
                        0x00000002;
                    break;
                case 3: /* trigger EDGE */
                    pCtxt->probe_cmd->gpio_intr_config[0].gpio_cfg0 =
                        0x00000000;
                    break;
                case 4: /* trigger LEVEL_LOW */
                    pCtxt->probe_cmd->gpio_intr_config[0].gpio_cfg0 =
                        0x00000008;
                    break;
                case 5: /* trigger LEVEL_HIGH */
                    pCtxt->probe_cmd->gpio_intr_config[0].gpio_cfg0 =
                        0x00000004;
                    break;
                default:
                    pCtxt->probe_cmd->gpio_intr_config[0].gpio_cfg0 =
                        0x00000000;
                    break;
                }

            }

            /* Pass slave information to kernel and probe */
            struct cam_control cam_cmd;
            memset(&cam_cmd, 0x0, sizeof(cam_cmd));

            cam_cmd.op_code     = AIS_SENSOR_PROBE_CMD;
            cam_cmd.size        = sizeof(*pCtxt->probe_cmd);
            cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
            cam_cmd.reserved    = 0;
            cam_cmd.handle      = (uint64_t)pCtxt->probe_cmd;

            if ((rc = LOG_IOCTL(pCtxt->fd, VIDIOC_CAM_CONTROL, &cam_cmd, "probe")) < 0)
            {
                CAM_MSG(ERROR, "[%s] AIS_SENSOR_PROBE_CMD [0x%x] failed %d",
                    m_pSensorLib->sensor_slave_info.sensor_name,
                    AIS_SENSOR_PROBE_CMD, rc);
                result = CAMERA_EFAILED;
            }
        }

        if(CAMERA_SUCCESS == result)
        {
            int ret = 0;
            int i = 0;
            struct v4l2_event_subscription sub;
            memset(&sub, 0, sizeof(sub));
            sub.type = AIS_SENSOR_EVENT_TYPE;
            ret = ioctl(pCtxt->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
            if(ret)
            {
                //@Todo: set result to EFAILED
                CAM_MSG(ERROR, " [%s] VIDIOC_SUBSCRIBE_EVENT [0x%x] failed %d",
                    m_pSensorLib->sensor_slave_info.sensor_name,
                    VIDIOC_SUBSCRIBE_EVENT, ret);
            }
            else
            {
                CAM_MSG(HIGH, " [%s] VIDIOC_SUBSCRIBE_EVENT [0x%x] passed %d",
                    m_pSensorLib->sensor_slave_info.sensor_name,
                    VIDIOC_SUBSCRIBE_EVENT, ret);
            }

            for (i = 0;  i < MAX_INTERRUPT_GPIO_PER_INPUT; i++)
            {
                pCtxt->interrupts[i].isUsed = FALSE;
            }
        }

        if (CAMERA_SUCCESS != result)
        {
            if (pCtxt->probe_cmd)
            {
                CameraFree(CAMERA_ALLOCATE_ID_SENSOR_PLATFORM_CTXT, pCtxt->probe_cmd);
            }
        }
    }

    return result;
}

/* ---------------------------------------------------------------------------
 *    FUNCTION        SensorPlatformDeinit
 *    DESCRIPTION     Deinitialize sensor platform with dev id
 *    DEPENDENCIES    None
 *    PARAMETERS
 *    RETURN VALUE    sensor platform ctxt ptr
 *    SIDE EFFECTS    None
 * ------------------------------------------------------------------------ */
void SensorPlatform::SensorPlatformDeinit(SensorPlatform* pPlatform)
{
    SensorPlatformLinux* pCtxt = static_cast<SensorPlatformLinux*>(pPlatform);

    if (pCtxt)
    {
        pCtxt->Deinit();

        delete pCtxt;
    }
}

void SensorPlatformLinux::Deinit()
{
    SensorPlatformLinux* pCtxt = this;
    int i;

    for (i = 0; i < MAX_INTERRUPT_GPIO_PER_INPUT; i++)
    {
        if (pCtxt->interrupts[i].isUsed && pCtxt->interrupts[i].hThread)
        {
            pCtxt->interrupts[i].isUsed = FALSE;
            CameraReleaseThread(pCtxt->interrupts[i].hThread);
        }
    }

    {//Unsubscribe events
        int ret = 0;
        struct v4l2_event_subscription sub;
        memset(&sub, 0, sizeof(sub));
        sub.type = AIS_SENSOR_EVENT_TYPE;
        ret = ioctl(pCtxt->fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
        if(ret)
        {
            CAM_MSG(ERROR, "SensorPlatformDeinit VIDIOC_UNSUBSCRIBE_EVENT failed %d",ret);
        }
        else
        {
            CAM_MSG(HIGH, "SensorPlatformDeinit VIDIOC_UNSUBSCRIBE_EVENT passed %d",ret);
        }
    }

    close(pCtxt->fd);

    if (pCtxt->probe_cmd)
    {
        CameraFree(CAMERA_ALLOCATE_ID_SENSOR_PLATFORM_CTXT, pCtxt->probe_cmd);
    }
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - sensor_setup_gpio_interrupt -
 *
 * DESCRIPTION: Setup gpio id as interrupt
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorSetupGpioInterrupt(enum camera_gpio_type gpio_id,
    sensor_intr_callback_t cb, void *data)
{
    SensorPlatformLinux* pCtxt = this;
    CameraSensorGPIO_IntrPinType pinInfo;
    sensor_platform_interrupt_t *pPlatformIntr = NULL;
    CameraResult result = CAMERA_SUCCESS;
    int i=0;

    for (i = 0; i < MAX_INTERRUPT_GPIO_PER_INPUT; i++)
    {
        if (!pCtxt->interrupts[i].isUsed)
        {
            pPlatformIntr = &pCtxt->interrupts[i];
            pPlatformIntr->pCtxt = pCtxt;
            pPlatformIntr->isUsed = TRUE;
            pPlatformIntr->pCbFcn = cb;
            pPlatformIntr->pData = data;
            break;
        }
    }

    if (pPlatformIntr)
    {
        {
            sensor_platform_event_thread_t pEventThreadFcn = NULL;
            {
                pEventThreadFcn = sensor_platform_intr_poll_thread;
                snprintf(pPlatformIntr->tname, sizeof(pPlatformIntr->tname),
                    "sensor_platform_intr_poll_thread_%d", pCtxt->CameraInterface);
            }


            if ( (CAMERA_SUCCESS == result) && pEventThreadFcn )
            {
                result = CameraCreateThread(CAMERA_THREAD_PRIO_DEFAULT, 0,
                    pEventThreadFcn,
                    (void*)(pPlatformIntr),
                    0,
                    pPlatformIntr->tname,
                    &pPlatformIntr->hThread);
                (void)CameraSetThreadPriority(pPlatformIntr->hThread,
                    CAMERA_THREAD_PRIO_HIGH_REALTIME);
            }
        }

        if (result != CAMERA_SUCCESS)
        {
            pPlatformIntr->isUsed = FALSE;
            //@todo: any cleanup...
        }
    }
    else
    {
        result = CAMERA_EFAILED;
    }

    return result;
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - sensor_power_up -
 *
 * DESCRIPTION: Executes power up sequence defined in sensor library
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorPowerUp()
{
    struct cam_control cam_cmd;
    SensorPlatformLinux* pCtxt = this;

    SENSOR_FUNCTIONENTRY("");

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));

    cam_cmd.op_code     = AIS_SENSOR_POWER_UP;
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.size        = 0;

    if (LOG_IOCTL(pCtxt->fd, VIDIOC_CAM_CONTROL, &cam_cmd, "power_up") < 0)
    {
        CAM_MSG(ERROR, "AIS_SENSOR_POWER_UP failed");
        return CAMERA_EFAILED;
    }

    SENSOR_FUNCTIONEXIT("");

    return CAMERA_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - sensor_power_down -
 *
 * DESCRIPTION: Executes power down sequence defined in sensor library
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorPowerDown()
{
    struct cam_control cam_cmd;
    SensorPlatformLinux* pCtxt = this;

    SENSOR_FUNCTIONENTRY("");

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));

    cam_cmd.op_code     = AIS_SENSOR_POWER_DOWN;
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.size        = 0;

    if (LOG_IOCTL(pCtxt->fd, VIDIOC_CAM_CONTROL, &cam_cmd, "power_down") < 0)
    {
        CAM_MSG(ERROR, "AIS_SENSOR_POWER_DOWN failed");
        return CAMERA_EFAILED;
    }

    SENSOR_FUNCTIONEXIT("");

    return CAMERA_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * FUNCTION sensor_execute_power_setting
 * DESCRIPTION Execute Sensor power config sequence
 * DEPENDENCIES None
 * PARAMETERS PowerConfig sequence and its size
 * RETURN VALUE None
 * SIDE EFFECTS None
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorExecutePowerSetting(
    struct camera_power_setting *power_settings, unsigned short nSize)
{
    SENSOR_FUNCTIONENTRY("");

    SENSOR_FUNCTIONEXIT("");

    return CAMERA_SUCCESS;
} /* SensorDriver_ExecutePowerConfig */


/* ---------------------------------------------------------------------------
 * FUNCTION    - sensor_write_i2c_setting_array -
 *
 * DESCRIPTION: Write I2C setting array
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorWriteI2cSettingArray(
    struct camera_i2c_reg_setting_array *settings)
{
    CameraResult result = CAMERA_SUCCESS;
    uint32_t i = 0;
    struct cam_control cam_cmd;
    struct ais_sensor_cmd_i2c_wr_array i2c_write;
    SensorPlatformLinux* pCtxt = this;
    unsigned short slave_addr = m_pSensorLib->sensor_slave_info.slave_addr;

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));
    memset(&i2c_write, 0x0, sizeof(i2c_write));

    cam_cmd.op_code = AIS_SENSOR_I2C_WRITE_ARRAY;
    cam_cmd.size = sizeof(i2c_write);
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.reserved = 0;
    cam_cmd.handle = (uint64_t)&i2c_write;

    translate_sensor_reg_setting_array(&i2c_write, settings);
    i2c_write.i2c_config.i2c_freq_mode = sensor_sdk_util_get_i2c_freq_mode(
        m_pSensorLib->sensor_slave_info.i2c_freq_mode);
    i2c_write.i2c_config.slave_addr = slave_addr;

    CAM_MSG(HIGH, "slave %x reg array size %d", slave_addr,
        i2c_write.count);
    for (i = 0; i < i2c_write.count; i++)
    {
        CAM_MSG(LOW, "addr %x data %x delay = %d",
            i2c_write.wr_array[i].reg_addr,
            i2c_write.wr_array[i].reg_data,
            i2c_write.wr_array[i].delay);
    }

    if (LOG_IOCTL(pCtxt->fd, VIDIOC_CAM_CONTROL, &cam_cmd, "write_i2c_array") < 0)
    {
        CAM_MSG(ERROR, "failed to write array to 0x%x %d", slave_addr, i2c_write.count);
        result = CAMERA_EFAILED;
    }

    return result;
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - sensor_slave_write_i2c_setting -
 *
 * DESCRIPTION: Slave Write I2C setting array
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorSlaveWriteI2cSetting(
    unsigned short slave_addr, struct camera_i2c_reg_setting *setting)
{
    uint32_t i = 0;
    struct cam_control cam_cmd;
    struct ais_sensor_cmd_i2c_wr_array i2c_write;
    SensorPlatformLinux* pCtxt = this;

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));
    memset(&i2c_write, 0x0, sizeof(i2c_write));

    cam_cmd.op_code = AIS_SENSOR_I2C_WRITE_ARRAY;
    cam_cmd.size = sizeof(i2c_write);
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.reserved = 0;
    cam_cmd.handle = (uint64_t)&i2c_write;

    translate_sensor_reg_setting(&i2c_write, setting);
    i2c_write.i2c_config.i2c_freq_mode = sensor_sdk_util_get_i2c_freq_mode(
        m_pSensorLib->sensor_slave_info.i2c_freq_mode);
    i2c_write.i2c_config.slave_addr = slave_addr;

    CAM_MSG(HIGH, "slave %x reg array size %d", slave_addr,
        i2c_write.count);
    for (i = 0; i < i2c_write.count; i++)
    {
        CAM_MSG(LOW, "addr %x data %x delay = %d",
            i2c_write.wr_array[i].reg_addr,
            i2c_write.wr_array[i].reg_data,
            i2c_write.wr_array[i].delay);
    }

    if (LOG_IOCTL(pCtxt->fd, VIDIOC_CAM_CONTROL, &cam_cmd, "write_i2c_setting") < 0)
    {
        CAM_MSG(ERROR, "failed to write array to 0x%x %d", slave_addr, i2c_write.count);
        return CAMERA_EFAILED;
    }

    return CAMERA_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - sensor_slave_read_i2c -
 *
 * DESCRIPTION: Slave read I2C reg
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorSlaveI2cRead(unsigned short slave_addr,
    struct camera_reg_settings_t *setting)
{
    uint32_t i = 0;
    struct cam_control cam_cmd;
    struct ais_sensor_cmd_i2c_read i2c_read;
    SensorPlatformLinux* pCtxt = this;

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));
    memset(&i2c_read, 0x0, sizeof(i2c_read));

    cam_cmd.op_code = AIS_SENSOR_I2C_READ;
    cam_cmd.size = sizeof(i2c_read);
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.reserved = 0;
    cam_cmd.handle = (uint64_t)&i2c_read;

    translate_sensor_read_reg(&i2c_read, setting);
    i2c_read.i2c_config.i2c_freq_mode = sensor_sdk_util_get_i2c_freq_mode(
        m_pSensorLib->sensor_slave_info.i2c_freq_mode);
    i2c_read.i2c_config.slave_addr = slave_addr;

    CAM_MSG(LOW, "read (%x) addr %x", slave_addr,
        i2c_read.reg_addr);

    if (LOG_IOCTL(pCtxt->fd, VIDIOC_CAM_CONTROL, &cam_cmd, "slave_read_i2c") < 0)
    {
        CAM_MSG(ERROR, "failed to read 0x%x:0x%x", slave_addr, i2c_read.reg_addr);
        return CAMERA_EFAILED;
    }

    setting->reg_data = i2c_read.reg_data;
    CAM_MSG(LOW, "read (%x) addr %x data %x", slave_addr,
        i2c_read.reg_addr, i2c_read.reg_data);

    return CAMERA_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - sensor_write_i2c_setting -
 *
 * DESCRIPTION: Write I2C setting
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorWriteI2cSetting(
    struct camera_i2c_reg_setting *setting)
{
    return SensorSlaveWriteI2cSetting(m_pSensorLib->sensor_slave_info.slave_addr, setting);
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - sensor_power_resume -
 *
 * DESCRIPTION: Executes power resume sequence defined in sensor library
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorPowerResume()
{
    struct cam_control cam_cmd;
    SensorPlatformLinux* pCtxt = this;

    SENSOR_FUNCTIONENTRY("");

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));

    cam_cmd.op_code     = AIS_SENSOR_I2C_POWER_UP;
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.size        = 0;

    if (LOG_IOCTL(pCtxt->fd, VIDIOC_CAM_CONTROL, &cam_cmd, "i2c_power_up") < 0)
    {
        CAM_MSG(ERROR, "AIS_SENSOR_I2C_POWER_UP failed");
        return CAMERA_EFAILED;
    }

    SENSOR_FUNCTIONEXIT("");

    return CAMERA_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - sensor_power_suspend -
 *
 * DESCRIPTION: Executes power suspend sequence defined in sensor library
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformLinux::SensorPowerSuspend()
{
    struct cam_control cam_cmd;
    SensorPlatformLinux* pCtxt = this;

    SENSOR_FUNCTIONENTRY("");

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));

    cam_cmd.op_code     = AIS_SENSOR_I2C_POWER_DOWN;
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.size        = 0;

    if (LOG_IOCTL(pCtxt->fd, VIDIOC_CAM_CONTROL, &cam_cmd, "i2c_power_down") < 0)
    {
        CAM_MSG(ERROR, "AIS_SENSOR_I2C_POWER_DOWN failed");
        return CAMERA_EFAILED;
    }

    SENSOR_FUNCTIONEXIT("");

    return CAMERA_SUCCESS;
}
