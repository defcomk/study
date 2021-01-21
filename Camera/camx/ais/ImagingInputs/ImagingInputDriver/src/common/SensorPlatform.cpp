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
#ifdef __QNXNTO__
#include <sys/neutrino.h>
#include "gpio_devctl.h"
#include "gpio_client.h"
#ifdef ENABLE_PMIC_GPIO
#include "pm_gpio.h"
#endif /* ENABLE_PMIC_GPIO */
#endif
#ifdef __INTEGRITY
#include <INTEGRITY.h>
#include <util/gpiopin.h>
#endif
#include <errno.h>
#include <string.h>
#include <pthread.h>

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
#define MAX_I2C_RETRIES 3

/* --------------------------------------------------------------------------
** Global variables
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Type Declarations
** ----------------------------------------------------------------------- */
class SensorPlatformCommon;

typedef struct
{
    enum camera_gpio_type gpio_id;
    uint32 nIRQ;
    byte isUsed;
    void* hThread;
    char  tname[64];
    int   channel_id;

#if !(defined(CAMERA_UNITTEST) || defined(__INTEGRITY))
    struct sigevent sigEvent;
#endif
    sensor_intr_callback_t pCbFcn;
    void *pData;
    SensorPlatformCommon *pCtxt;
} sensor_platform_interrupt_t;

typedef struct
{
    byte slave_addr;
    byte register_size;
    byte data_size;
}sensor_platform_i2c_op_t;


class SensorPlatformCommon : public SensorPlatform
{
public:
    SensorPlatformCommon(sensor_lib_t* p_sensor_lib);

    CameraResult Init();
    void Deinit();

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_power_up -
     *
     * DESCRIPTION: Executes power up sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerUp();

    /* ---------------------------------------------------------------------------
     * FUNCTION    - sensor_power_down -
     *
     * DESCRIPTION: Executes power down sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerDown();

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorWriteI2cSettingArray -
     *
     * DESCRIPTION: Write I2C setting array
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorWriteI2cSettingArray(struct camera_i2c_reg_setting_array *settings);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorSlaveWriteI2cSetting -
     *
     * DESCRIPTION: Slave Write I2C setting array
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorSlaveWriteI2cSetting(unsigned short slave_addr,
            struct camera_i2c_reg_setting *setting);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorSlaveI2cRead -
     *
     * DESCRIPTION: Slave read I2C reg
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorSlaveI2cRead(unsigned short slave_addr,
            struct camera_reg_settings_t *setting);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorWriteI2cSetting -
     *
     * DESCRIPTION: Write I2C setting
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorWriteI2cSetting(struct camera_i2c_reg_setting *setting);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorExecutePowerSetting -
     *
     * DESCRIPTION: Execute Sensor power config sequence
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorExecutePowerSetting(struct camera_power_setting *power_settings, unsigned short nSize);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorSetupGpioInterrupt -
     *
     * DESCRIPTION: Setup gpio id as interrupt
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorSetupGpioInterrupt(enum camera_gpio_type gpio_id,
            sensor_intr_callback_t cb, void *data);

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorPowerResume -
     *
     * DESCRIPTION: Executes power resume sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerResume();

    /* ---------------------------------------------------------------------------
     * FUNCTION    - SensorPowerSuspend -
     *
     * DESCRIPTION: Executes power suspend sequence defined in sensor library
     * ------------------------------------------------------------------------ */
    virtual CameraResult SensorPowerSuspend();

private:
    CameraResult SensorPlatform_RegisterSeqWrite(sensor_platform_i2c_op_t* i2c_op,
            struct camera_i2c_reg_array *pRegSeq,
            uint16 reg_seq_size, uint16 delay_ms);

    boolean I2CRead(sensor_platform_i2c_op_t* i2c_op, uint16 offset, uint16* pdata);


    sensor_lib_t* m_pSensorLib;

    CameraSensorIndex m_eCameraInterface;

    // i2c/cci
    CameraMutex m_i2cMutex;
    CameraSensorI2CMasterType m_I2CMasterIndex;
    byte m_I2CSlaveAddr;
    byte m_I2CRegisterSize;
    byte m_I2CDataSize;
    CameraSensorI2C_SpeedType m_I2CBusFrequency;

    // interrupt
    sensor_platform_interrupt_t m_aInterrupts[MAX_INTERRUPT_GPIO_PER_INPUT];
};

typedef int (*sensor_platform_event_thread_t)(void *arg);

/* --------------------------------------------------------------------------
** Internal Functions
** ----------------------------------------------------------------------- */
static void SensoPlatform_GetRegisterandDataSize(enum camera_i2c_reg_addr_type addr_type,
        enum camera_i2c_data_type data_type,
        byte *register_size, byte *data_size);

extern "C" boolean CameraSensorGPIO_InitInterrupt(const CameraSensorGPIO_IntrPinType * const pIntrPin,
        uint32* hIRQ);

SensorPlatformCommon::SensorPlatformCommon(sensor_lib_t* p_sensor_lib)
{
    m_pSensorLib = p_sensor_lib;
}

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver_GetRegisterandDataSize
* DESCRIPTION Retrieve Register and Data size from I2C addr and data type
* DEPENDENCIES None
* PARAMETERS I2C register address and data type
* RETURN VALUE None
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
static void SensoPlatform_GetRegisterandDataSize(enum camera_i2c_reg_addr_type addr_type,
                                         enum camera_i2c_data_type data_type,
                                         byte *register_size, byte *data_size)
{
    SENSOR_FUNCTIONENTRY("");

    switch(addr_type)
    {
        case CAMERA_I2C_BYTE_ADDR:
            *register_size = 1;
            break;
        case CAMERA_I2C_WORD_ADDR:
            *register_size = 2;
            break;
        case CAMERA_I2C_3B_ADDR:
            *register_size = 3;
            break;
        default:
            *register_size = 0;
            SENSOR_ERROR("Invalid Address type = %d", addr_type);
            break;
    }

    switch(data_type)
    {
        case CAMERA_I2C_BYTE_DATA:
            *data_size = 1;
            break;
        case CAMERA_I2C_WORD_DATA:
            *data_size = 2;
            break;
        default:
            *data_size = 0;
            SENSOR_ERROR("Invalid data type = %d", data_type);
            break;
    }
    SENSOR_FUNCTIONEXIT("");

} /* SensorDriver_GetRegisterandDataSize */

#if !(defined(CAMERA_UNITTEST) || defined(__INTEGRITY))
static void sensor_platform_ist_cleanup(void *param)
{
    if (param)
    {
        if (-1 == InterruptDetach( *((int *)param) ) )
        {
            CAM_MSG(ERROR, "InterruptDetach: %s", strerror(errno));
        }
    }
}
#endif

static int sensor_platform_intr_poll_thread(void *arg)
{
    sensor_platform_interrupt_t *pPlatformIntr = (sensor_platform_interrupt_t*)arg;

    CAM_MSG(HIGH,"%s: I'm running for instance ...", __FUNCTION__);

    while (pPlatformIntr->isUsed == TRUE)
    {
        /* block infinitely to wait for interrupt or being cancelled */
        pPlatformIntr->pCbFcn(pPlatformIntr->pData);

        CameraPlatform_WaitMicroSeconds(100000);
    }

    return 0;
}

static int sensor_platform_intr_tlmm_thread(void *arg)
{
#if defined(__INTEGRITY)
    Error ret;
    Connection obj;
    struct GpioPin *pin;
    Time T = {0, 0x40000000}; /* 0.25 seconds */

    char gpio_intr_name[16];
    sensor_platform_interrupt_t *pPlatformIntr = (sensor_platform_interrupt_t*)arg;

    CAM_MSG(HIGH, "%s: Initializing interrupt for pin_id %d ...", __FUNCTION__, pPlatformIntr->nIRQ);
    snprintf(gpio_intr_name, sizeof(gpio_intr_name), "GPIO:0x000000%x", pPlatformIntr->nIRQ);

    ret = GpioPin_Import(gpio_intr_name, &pin);
    CAM_MSG_ON_ERR(ret, "GpioPin_Import failed, %s error = %d", gpio_intr_name, ret);
    /* Set pin as input */
    GpioPin_SetDirection(pin, GpioPinDirection_Input);
    usleep(100);
    /* Enabling falling edge interrupts */
    ret =GpioPin_ListenForEvent(pin, GpioPinEvent_FallingEdge);
    CAM_MSG_ON_ERR(ret, "GpioPin_ListenForEvent failed, %s error = %d", gpio_intr_name, ret);
    obj = (Connection) GpioPin_GetEventNotificationObject(pin);
    ret = TimedSynchronousReceive(obj, NULL, &T);

    ret = GpioPin_ListenForEvent(pin, GpioPinEvent_FallingEdge);
    CAM_MSG_ON_ERR(ret, "GpioPin_ListenForEvent failed, %s error = %d", gpio_intr_name, ret);
    TimedSynchronousReceive(obj, NULL, &T);
    pPlatformIntr->pCbFcn(pPlatformIntr->pData);

    if (Success == ret)
    {
        while (1)
        {
            /* Making sure that only one interrupt is generated */
            GpioPin_ListenForEvent(pin, GpioPinEvent_FallingEdge);
            ret = SynchronousReceive(obj, NULL);

            if(ret == ObjectTransfersAborted || ret == ObjectIsNotInUse)
            {
                /* Interrupt was deinitialized */
                CAM_MSG(ERROR, "GPIO %d was deinitialized rc = %d", pin, ret);
                break;
            }

            pPlatformIntr->pCbFcn(pPlatformIntr->pData);
        }
    }
    else
    {
        CAM_MSG(FATAL, "Interrupt initialization failed with error: %d", ret);
        GpioPin_Close(pin);
    }
#endif

#if !(defined(CAMERA_UNITTEST) || defined(__INTEGRITY))
    sensor_platform_interrupt_t *pPlatformIntr = (sensor_platform_interrupt_t*)arg;
    int id;

    CAM_MSG(HIGH, "%s: I'm running for instance ...", __FUNCTION__);

    /* request IO privileges for the next call */
    ThreadCtl(_NTO_TCTL_IO, 0);
    SIGEV_INTR_INIT(&pPlatformIntr->sigEvent);

    id = InterruptAttachEvent(pPlatformIntr->nIRQ,
            &pPlatformIntr->sigEvent,
            _NTO_INTR_FLAGS_TRK_MSK);  /* always use this! */

    if (-1 != id)
    {
        pthread_cleanup_push(&sensor_platform_ist_cleanup, (void*)&id);
        while (1)
        {
            /* block infinitely to wait for interrupt or being cancelled */
            InterruptWait(0, NULL);

            /* re-enable the interrupt immediately */
            InterruptUnmask(pPlatformIntr->nIRQ, id);

            pPlatformIntr->pCbFcn(pPlatformIntr->pData);
        }
        pthread_cleanup_pop(1);
    }
    else
    {
        CAM_MSG(FATAL, "InterruptAttachEvent: %s", strerror(errno));
    }
#endif
    return 0;
}


#ifdef ENABLE_PMIC_GPIO
static void sensor_platform_ist_pmic_cleanup(void *param)
{
    if (param != NULL)
    {
        sensor_platform_interrupt_t *pPlatformIntr = (sensor_platform_interrupt_t*)param;

        uint32 dev_id = GPIO_MODULE_TYPE(pPlatformIntr->nIRQ) - PMIC_MODULE;
        uint32 pin_id = GPIO_NUM(pPlatformIntr->nIRQ);

        if (pPlatformIntr->channel_id != -1)
        {
            pm_gpio_clear_channel(dev_id, pin_id, pPlatformIntr->channel_id);
            pm_gpio_destroy_channel(dev_id, pPlatformIntr->channel_id);
            pm_gpio_cleanup(dev_id);
        }
    }

}

static int sensor_platform_intr_pmic_thread(void *arg)
{
    pm_err_flag_type rc;
    sensor_platform_interrupt_t *pPlatformIntr = (sensor_platform_interrupt_t*)arg;

    uint32 dev_id = GPIO_MODULE_TYPE(pPlatformIntr->nIRQ) - PMIC_MODULE;
    uint32 pin_id = GPIO_NUM(pPlatformIntr->nIRQ);
    int chid = -1;

    CAM_MSG(HIGH, "E %s 0x%p %d %d\n", __func__, arg, dev_id, pin_id);

    pPlatformIntr->channel_id = -1;

    rc = pm_gpio_create_channel(dev_id, pin_id, &chid);
    if (rc != PM_ERR_FLAG__SUCCESS)
    {
        goto EXIT_FLAG;
    }

    pPlatformIntr->channel_id = chid;
    pthread_cleanup_push(&sensor_platform_ist_pmic_cleanup, arg);

    while (1)
    {
        //wait for interrupt events
        rc = pm_gpio_wait_for_irq(dev_id, pin_id, chid);

        if (rc != PM_ERR_FLAG__SUCCESS)
        {
            CAM_MSG(ERROR, "%s failed (%d) to wait for irq %d %d\n", __func__, rc, dev_id, pin_id);
            continue;
        }

        rc = pm_gpio_irq_clear(dev_id, pin_id);

        pPlatformIntr->pCbFcn(pPlatformIntr->pData);

        if (rc != PM_ERR_FLAG__SUCCESS)
        {
            break;
        }
    }

    pthread_cleanup_pop(1);

    CAM_MSG(HIGH, "X %s 0x%p %d %d\n", __func__, arg, dev_id, pin_id);

EXIT_FLAG:

    return 0;
}
#endif /*ENABLE_PMIC_GPIO*/

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
    SensorPlatformCommon* pCtxt = new SensorPlatformCommon(p_sensor_lib);
    if (pCtxt)
    {
        (void)pCtxt->Init();
    }

    return pCtxt;
}

CameraResult SensorPlatformCommon::Init()
{
    enum sensor_camera_id camera_id = m_pSensorLib->sensor_slave_info.camera_id;
    CAM_MSG(HIGH, "SensorPlatform init subdev %d", camera_id);

    CameraSensorGPIOConfigCamera(camera_id);

    CameraCreateMutex (&m_i2cMutex);

    memset(m_aInterrupts, 0x0, sizeof(m_aInterrupts));

    m_eCameraInterface = (CameraSensorIndex)camera_id;
    m_I2CMasterIndex = CameraPlatform_GetMasterIndex(camera_id);

    switch (m_pSensorLib->sensor_slave_info.i2c_freq_mode)
    {
    case SENSOR_I2C_MODE_FAST_PLUS:
        m_I2CBusFrequency = CAMSENSOR_I2C_1000KHZ;
        break;
    case SENSOR_I2C_MODE_CUSTOM:
    case SENSOR_I2C_MODE_FAST:
        m_I2CBusFrequency = CAMSENSOR_I2C_400KHZ;
        break;
    case SENSOR_I2C_MODE_STANDARD:
    default:
        m_I2CBusFrequency = CAMSENSOR_I2C_100KHZ;
        break;
    }

    m_I2CSlaveAddr = m_pSensorLib->sensor_slave_info.slave_addr;
    SensoPlatform_GetRegisterandDataSize(
            m_pSensorLib->sensor_slave_info.addr_type,
            m_pSensorLib->sensor_slave_info.data_type,
            &m_I2CRegisterSize, &m_I2CDataSize);

    return CAMERA_SUCCESS;
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
    SensorPlatformCommon* pCtxt = static_cast<SensorPlatformCommon*>(pPlatform);
    if (pCtxt)
    {
        pCtxt->Deinit();

        delete pCtxt;
    }
}

void SensorPlatformCommon::Deinit()
{
    int i;

    for (i = 0; i < MAX_INTERRUPT_GPIO_PER_INPUT; i++)
    {
        if (m_aInterrupts[i].isUsed && m_aInterrupts[i].hThread)
        {
            CameraReleaseThread(m_aInterrupts[i].hThread);
            m_aInterrupts[i].isUsed = FALSE;
        }
    }

    CameraDestroyMutex(m_i2cMutex);
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - SensorSetupGpioInterrupt -
 *
 * DESCRIPTION: Setup gpio id as interrupt
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformCommon::SensorSetupGpioInterrupt(enum camera_gpio_type gpio_id,
        sensor_intr_callback_t cb, void *data)
{
    SensorPlatformCommon* pCtxt = this;
    CameraSensorGPIO_IntrPinType pinInfo;
    sensor_platform_interrupt_t *pPlatformIntr = NULL;
    CameraResult result = CAMERA_SUCCESS;
    int freeIdx = MAX_INTERRUPT_GPIO_PER_INPUT;
    int i=0;

    for (i = 0; i < MAX_INTERRUPT_GPIO_PER_INPUT; i++)
    {
        pPlatformIntr = &pCtxt->m_aInterrupts[i];

        if (pPlatformIntr->isUsed && pPlatformIntr->gpio_id == gpio_id)
        {
            CAM_MSG(ERROR, "GPIO%d Interrupt is already setup", gpio_id);
            freeIdx = MAX_INTERRUPT_GPIO_PER_INPUT;
            break;
        }

        if (!pPlatformIntr->isUsed && MAX_INTERRUPT_GPIO_PER_INPUT == freeIdx)
        {
            freeIdx = i;
        }
    }

    if (MAX_INTERRUPT_GPIO_PER_INPUT != freeIdx)
    {
        pPlatformIntr = &pCtxt->m_aInterrupts[freeIdx];
        pPlatformIntr->gpio_id = gpio_id;
        pPlatformIntr->pCtxt = pCtxt;
        pPlatformIntr->isUsed = TRUE;
        pPlatformIntr->pCbFcn = cb;
        pPlatformIntr->pData = data;
        pPlatformIntr->channel_id = -1;

        if (CameraSensorGPIO_GetIntrPinInfo(pCtxt->m_eCameraInterface, gpio_id, &pinInfo))
        {
            sensor_platform_event_thread_t pEventThreadFcn = NULL;

            switch (pinInfo.intr_type)
            {
            case CAMERA_GPIO_INTR_NONE:
                /* no thread required */
                pPlatformIntr->hThread = NULL;
                break;
            case CAMERA_GPIO_INTR_TLMM:
            {
                if (CameraSensorGPIO_InitInterrupt(&pinInfo, &pPlatformIntr->nIRQ))
                {
                    pEventThreadFcn = sensor_platform_intr_tlmm_thread;
                    snprintf(pPlatformIntr->tname, sizeof(pPlatformIntr->tname),
                            "intr_tlmm_%d", pCtxt->m_eCameraInterface);
                }
                else
                {
                    CAM_MSG(ERROR, "%s: CameraSensorGPIO_InitInterrupt failed", __FUNCTION__);
                    result = CAMERA_EFAILED;
                }
                break;
            }
#ifdef ENABLE_PMIC_GPIO
            case CAMERA_GPIO_INTR_PMIC:
            {
                if (CameraSensorGPIO_InitInterrupt(&pinInfo, &pPlatformIntr->nIRQ))
                {
                    pEventThreadFcn = sensor_platform_intr_pmic_thread;
                    snprintf(pPlatformIntr->tname, sizeof(pPlatformIntr->tname),
                            "intr_pmic_%d", pCtxt->m_eCameraInterface);
                }
                else
                {
                    CAM_MSG(ERROR, "%s: CameraSensorGPIO_InitInterrupt failed", __FUNCTION__);
                    result = CAMERA_EFAILED;
                }
                break;
            }
#endif
            case CAMERA_GPIO_INTR_POLL:
            {
                pEventThreadFcn = sensor_platform_intr_poll_thread;
                snprintf(pPlatformIntr->tname, sizeof(pPlatformIntr->tname),
                        "intr_poll_%d", pCtxt->m_eCameraInterface);
            }
            break;
            default:
                result = CAMERA_EFAILED;
                CAM_MSG(ERROR, "%s: bad intr config %d",
                        __FUNCTION__, pinInfo.intr_type);
                break;
            }

            if ( (CAMERA_SUCCESS == result) && pEventThreadFcn )
            {
                if (0 == CameraCreateThread(CAMERA_THREAD_PRIO_HIGH_REALTIME,
                        0,
                        pEventThreadFcn,
                        (void*)pPlatformIntr,
                        0x8000,
                        pPlatformIntr->tname,
                        &pPlatformIntr->hThread))
                {
                    CAM_MSG(HIGH, "%s: started %s", __FUNCTION__, pPlatformIntr->tname);
                }
                else
                {
                    CAM_MSG(ERROR, "%s: MM_Thread_CreateEx failed", __FUNCTION__);
                    result = CAMERA_EFAILED;
                }
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
 * FUNCTION    - SensorPowerUp -
 *
 * DESCRIPTION: Executes power up sequence defined in sensor library
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformCommon::SensorPowerUp()
{
    SensorPlatformCommon* pCtxt = this;

    return SensorExecutePowerSetting(
            pCtxt->m_pSensorLib->sensor_slave_info.power_setting_array.power_up_setting_a,
            pCtxt->m_pSensorLib->sensor_slave_info.power_setting_array.size_up);
}


/* ---------------------------------------------------------------------------
 * FUNCTION    - SensorPowerDown -
 *
 * DESCRIPTION: Executes power down sequence defined in sensor library
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformCommon::SensorPowerDown()
{
    SensorPlatformCommon* pCtxt = this;

    return SensorExecutePowerSetting(
            pCtxt->m_pSensorLib->sensor_slave_info.power_setting_array.power_down_setting_a,
            pCtxt->m_pSensorLib->sensor_slave_info.power_setting_array.size_down);
}

/* ---------------------------------------------------------------------------
* FUNCTION SensorDriver_RegisterSeqWrite
* DESCRIPTION Execute Sensor power config sequence
* DEPENDENCIES None
* PARAMETERS Register write sequence, its size and delay
* RETURN VALUE None
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorPlatformCommon::SensorPlatform_RegisterSeqWrite(
        sensor_platform_i2c_op_t* i2c_op, struct camera_i2c_reg_array *pRegSeq,
        uint16 reg_seq_size, uint16 delay_ms)
{
    boolean ret = TRUE;
    cam_i2c_cmd cmd;
    SensorPlatformCommon* pCtxt = this;

    SENSOR_FUNCTIONENTRY("");

    SENSOR_DEBUG("Execute I2C register write of array size = %d", reg_seq_size);

    CameraLockMutex(pCtxt->m_i2cMutex);

    memset(&cmd, 0x0, sizeof(cmd));

    cmd.client.clk_mode = CAM_I2C_CLK_MODE_STANDARD;
    if (pCtxt->m_I2CBusFrequency == CAMSENSOR_I2C_100KHZ)
    {
        cmd.client.clk_mode = CAM_I2C_CLK_MODE_STANDARD;
    }
    else if (pCtxt->m_I2CBusFrequency == CAMSENSOR_I2C_400KHZ)
    {
        cmd.client.clk_mode = CAM_I2C_CLK_MODE_FAST;
    }
    else if (pCtxt->m_I2CBusFrequency == CAMSENSOR_I2C_1000KHZ)
    {
        cmd.client.clk_mode = CAM_I2C_CLK_MODE_FAST_PLUS;
    }

    cmd.client.slave_id  = i2c_op->slave_addr >> 1;
    cmd.client.retry_cnt = 3;
    cmd.client.master_id = pCtxt->m_I2CMasterIndex;
    cmd.client.queue_id = 2;

    cmd.op_type = 0;
    cmd.buf.multi.p_buf = pRegSeq;
    cmd.buf.multi.size = reg_seq_size;

    cmd.buf.multi.addr_option = CAM_I2C_CMD_PARAM_OPTION_MAKE(i2c_op->register_size, sizeof(pRegSeq->reg_addr), CAM_I2C_ENDIAN_BIG);
    cmd.buf.multi.data_option = CAM_I2C_CMD_PARAM_OPTION_MAKE(i2c_op->data_size, sizeof(pRegSeq->reg_data), CAM_I2C_ENDIAN_BIG);

    ret = CameraSensorI2C_Write(&cmd);

    if (ret && delay_ms)
    {
        CameraPlatform_WaitMicroSeconds(ConvertMilliToMicroSeconds(delay_ms));
    }

    CameraUnlockMutex(pCtxt->m_i2cMutex);

    SENSOR_FUNCTIONEXIT("");

    return ret ? CAMERA_SUCCESS : CAMERA_EFAILED;
} /* SensorDriver_RegisterSeqWrite */



/* ---------------------------------------------------------------------------
* FUNCTION SensorExecutePowerSetting
* DESCRIPTION Execute Sensor power config sequence
* DEPENDENCIES None
* PARAMETERS PowerConfig sequence and its size
* RETURN VALUE None
* SIDE EFFECTS None
* ------------------------------------------------------------------------ */
CameraResult SensorPlatformCommon::SensorExecutePowerSetting(
        struct camera_power_setting *power_settings, unsigned short nSize)
{
    uint8 index;
    SensorPlatformCommon* pCtxt = this;

    SENSOR_FUNCTIONENTRY("");

    if (MAX_POWER_CONFIG < nSize)
    {
        SENSOR_ERROR("Invalid array size = %d", nSize);
        return CAMERA_EFAILED;
    }

    for (index = 0; index < nSize; index++)
    {
        // Execute Power Config
        switch(power_settings[index].seq_type)
        {
            case CAMERA_POW_SEQ_CLK:
            {
                switch(power_settings[index].seq_val)
                {
                    case CAMERA_MCLK:
                        CameraSensorCLKConfigMclk(pCtxt->m_eCameraInterface, power_settings[index].config_val);
                        break;
                    default:
                        //Nothing to do
                        break;
                }
            }
            break;

            case CAMERA_POW_SEQ_GPIO:
            {
                CameraSensorGPIO_Out(pCtxt->m_eCameraInterface, (CameraSensorGPIO_SignalType)power_settings[index].seq_val,
                        (CameraSensorGPIO_ValueType)((power_settings[index].config_val == GPIO_OUT_HIGH) ? GPIO_OUT_HIGH : GPIO_OUT_LOW));
            }
            break;

            case CAMERA_POW_SEQ_VREG:
            case CAMERA_POW_SEQ_I2C_MUX:
            case CAMERA_POW_SEQ_I2C:
            default:
                // Nothing to do, Configured in ACPI
                break;
        }

        if (power_settings[index].delay > 0)
        {
            CameraPlatform_WaitMicroSeconds(ConvertMilliToMicroSeconds(power_settings[index].delay));
        }
    }

    SENSOR_FUNCTIONEXIT("");

    return CAMERA_SUCCESS;
} /* SensorDriver_ExecutePowerConfig */


/*===========================================================================
 * FUNCTION    - SensorWriteI2cSettingArray -
 *
 * DESCRIPTION: Write I2C setting array
 *==========================================================================*/
CameraResult SensorPlatformCommon::SensorWriteI2cSettingArray(struct camera_i2c_reg_setting_array *settings)
{
    struct camera_i2c_reg_setting reg_setting;
    reg_setting.reg_array = settings->reg_array;
    reg_setting.addr_type = settings->addr_type;
    reg_setting.data_type = settings->data_type;
    reg_setting.delay = settings->delay;
    reg_setting.size = settings->size;
    return SensorWriteI2cSetting(&reg_setting);
}

/*===========================================================================
 * FUNCTION    - SensorSlaveWriteI2cSetting -
 *
 * DESCRIPTION: Slave Write I2C setting array
 *==========================================================================*/
CameraResult SensorPlatformCommon::SensorSlaveWriteI2cSetting(
        unsigned short slave_addr, struct camera_i2c_reg_setting *setting)
{
    sensor_platform_i2c_op_t i2c_op;
    CameraResult rc;


    SensoPlatform_GetRegisterandDataSize(
            setting->addr_type,
            setting->data_type,
            &i2c_op.register_size, &i2c_op.data_size);

    i2c_op.slave_addr = (byte)slave_addr;

    rc = SensorPlatform_RegisterSeqWrite(
            &i2c_op,
            setting->reg_array,
            setting->size,
            setting->delay);

    return rc;
}

/*===========================================================================
 * FUNCTION    - SensorSlaveI2cRead -
 *
 * DESCRIPTION: Slave read I2C reg
 *==========================================================================*/
CameraResult SensorPlatformCommon::SensorSlaveI2cRead(unsigned short slave_addr,
        struct camera_reg_settings_t *setting)
{
    SensorPlatformCommon* pCtxt = this;
    sensor_platform_i2c_op_t i2c_op;
    CameraResult rc;

    SensoPlatform_GetRegisterandDataSize(
            setting->addr_type,
            setting->data_type,
            &i2c_op.register_size, &i2c_op.data_size);

    i2c_op.slave_addr = (byte)slave_addr;

    CameraLockMutex(pCtxt->m_i2cMutex);

    rc = I2CRead(&i2c_op, setting->reg_addr, &setting->reg_data) ? CAMERA_SUCCESS : CAMERA_EFAILED;

    CameraUnlockMutex(pCtxt->m_i2cMutex);

    return rc;
}

/*===========================================================================
 * FUNCTION    - SensorWriteI2cSetting -
 *
 * DESCRIPTION: Write I2C setting
 *==========================================================================*/
CameraResult SensorPlatformCommon::SensorWriteI2cSetting(
        struct camera_i2c_reg_setting *setting)
{
    SensorPlatformCommon* pCtxt = this;

    return SensorSlaveWriteI2cSetting(
            pCtxt->m_pSensorLib->sensor_slave_info.slave_addr, setting);
}

/* ---------------------------------------------------------------------------
 *    FUNCTION        I2CRead
 *    DESCRIPTION     I2C read
 *    DEPENDENCIES    None
 *    PARAMETERS
 *    RETURN VALUE    TRUE if the read was successful, FALSE otherwise
 *    SIDE EFFECTS    None
 * ------------------------------------------------------------------------ */
boolean SensorPlatformCommon::I2CRead(sensor_platform_i2c_op_t* i2c_op, uint16 offset, uint16* pdata)
{
    boolean rc = FALSE;
    SensorPlatformCommon* pCtxt = this;
    cam_i2c_cmd cmd;
    struct camera_i2c_reg_array arr;

    if (pdata == NULL)
        return FALSE;

    memset(&cmd, 0x0, sizeof(cmd));

    cmd.client.clk_mode = CAM_I2C_CLK_MODE_STANDARD;
    if (pCtxt->m_I2CBusFrequency == CAMSENSOR_I2C_100KHZ)
    {
        cmd.client.clk_mode = CAM_I2C_CLK_MODE_STANDARD;
    }
    else if (pCtxt->m_I2CBusFrequency == CAMSENSOR_I2C_400KHZ)
    {
        cmd.client.clk_mode = CAM_I2C_CLK_MODE_FAST;
    }
    else if (pCtxt->m_I2CBusFrequency == CAMSENSOR_I2C_1000KHZ)
    {
        cmd.client.clk_mode = CAM_I2C_CLK_MODE_FAST_PLUS;
    }

    cmd.client.slave_id  = i2c_op->slave_addr >> 1;
    cmd.client.retry_cnt = 3;
    cmd.client.master_id = pCtxt->m_I2CMasterIndex;
    cmd.client.queue_id = 2;

    cmd.op_type = 0;
    cmd.buf.multi.p_buf = &arr;
    cmd.buf.multi.size = 1;

    arr.reg_addr = offset;
    arr.reg_data = 0;
    arr.delay = 0;

    cmd.buf.multi.addr_option = CAM_I2C_CMD_PARAM_OPTION_MAKE(i2c_op->register_size, sizeof(arr.reg_addr), CAM_I2C_ENDIAN_BIG);
    cmd.buf.multi.data_option = CAM_I2C_CMD_PARAM_OPTION_MAKE(i2c_op->data_size, sizeof(arr.reg_data), CAM_I2C_ENDIAN_BIG);

    rc = CameraSensorI2C_Read(&cmd);

    *pdata = arr.reg_data;

    return rc;
} /* I2CRead */

/* ---------------------------------------------------------------------------
 * FUNCTION    - SensorPowerResume -
 *
 * DESCRIPTION: Executes power resume sequence defined in sensor library
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformCommon::SensorPowerResume()
{
    return CAMERA_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * FUNCTION    - SensorPowerSuspend -
 *
 * DESCRIPTION: Executes power suspend sequence defined in sensor library
 * ------------------------------------------------------------------------ */
CameraResult SensorPlatformCommon::SensorPowerSuspend()
{
    return CAMERA_SUCCESS;
}
