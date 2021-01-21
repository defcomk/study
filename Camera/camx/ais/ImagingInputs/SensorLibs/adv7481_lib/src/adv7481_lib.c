/**
 * @file adv7481_lib.c
 * @brief adapt sensor lib interfaces for adv7481
 *
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "CameraSensorDeviceInterface.h"

#include "adv7481.h"
#include "adv7481_lib.h"

#include "SensorDebug.h"

/**
 * Instruction to enable adv7481 cvbs driver
 * 1, define SAMPLE_ADV7481 in camera_config.c
 * 2, if using polling for interrupt, define ADV7481_INTR_USE_POLL in adv7481.h
 *    and change .intr_type = CAMERA_GPIO_INTR_PMIC, to CAMERA_GPIO_INTR_POLL
 * 3, add -lais_adv7481S to LDFLAGS in libais/common.mk
 * 4, compile camera and upload all program an libs to board
 * 5, change input_id to 10 for cvbs config files
 * 6, kill avin_server to avoid interferences
 */


typedef enum
{
    ADV7481_STATE_INVALID = 0,
    ADV7481_STATE_DETECTED,
    ADV7481_STATE_INITIALIZED,
    ADV7481_STATE_STREAMING,
} adv7481_state_t;

typedef struct
{
    /*must be first element*/
    sensor_lib_t sensor_lib;

    unsigned int num_supported_sensors;
    unsigned int num_connected_sensors;

    adv7481_state_t state;
    int             power_state;

    //only one device and one channle are supported for adv7481
    s_adv7481_drv *p_drv;
    CameraMutex mutex;

    sensor_platform_func_table_t sensor_platform_fcn_tbl;
    void* ctrl;
} adv7481_context_t;

static int adv7481_sensor_close_lib(void* ctxt);
static int adv7481_sensor_detect_device(void* ctxt);
static int adv7481_sensor_detect_device_channels(void* ctxt);
static int adv7481_sensor_init_setting(void* ctxt);
static int adv7481_sensor_set_channel_mode(void* ctxt, unsigned int src_id_mask, unsigned int mode);
static void adv7481_sensor_isr(void* ctxt);
static int adv7481_sensor_power_on(void* ctxt);
static int adv7481_sensor_power_off(void* ctxt);
static int adv7481_sensor_power_resume(void* ctxt);
static int adv7481_sensor_power_suspend(void* ctxt);
static int adv7481_sensor_start_stream(void* ctxt, unsigned int src_id_mask);
static int adv7481_sensor_stop_stream(void* ctxt, unsigned int src_id_mask);
static int adv7481_sensor_set_platform_func_table(void* ctxt, sensor_platform_func_table_t* table);
static int adv7481_sensor_query_field_info(void* ctxt, boolean *p_is_even, uint64_t *p_ts);


static sensor_lib_t adv7481_lib_ptr =
{
    .sensor_slave_info =
    {
        .sensor_name = SENSOR_MODEL,
        .addr_type = CAMERA_I2C_BYTE_ADDR,
        .data_type = CAMERA_I2C_BYTE_DATA,
        .i2c_freq_mode = SENSOR_I2C_MODE_CUSTOM,

        .power_setting_array =
        {
            //check the programming document for details
            .power_up_setting_a =
            {
                {
                    .seq_type = CAMERA_POW_SEQ_GPIO,
                    .seq_val = CAMERA_GPIO_RESET,
                    .config_val = GPIO_OUT_LOW,
                    .delay = 0,
                },
                {
                    .seq_type = CAMERA_POW_SEQ_VREG,
                    .seq_val = CAMERA_VDIG,
                    .config_val = 0,
                    .delay = 0,
                },
                {
                    .seq_type = CAMERA_POW_SEQ_VREG,
                    .seq_val = CAMERA_VANA,
                    .config_val = 0,
                    .delay = 0,
                },
                {
                    .seq_type = CAMERA_POW_SEQ_VREG,
                    .seq_val = CAMERA_VIO,
                    .config_val = 0,
                    .delay = 5,
                },
                {
                    .seq_type = CAMERA_POW_SEQ_GPIO,
                    .seq_val = CAMERA_GPIO_RESET,
                    .config_val = GPIO_OUT_HIGH,
                    .delay = 5,
                },
            },
            .size_up = 5,
            .power_down_setting_a =
            {
                {
                    .seq_type = CAMERA_POW_SEQ_GPIO,
                    .seq_val = CAMERA_GPIO_RESET,
                    .config_val = GPIO_OUT_LOW,
                    .delay = 2,
                },
                {
                    .seq_type = CAMERA_POW_SEQ_VREG,
                    .seq_val = CAMERA_VIO,
                    .config_val = 0,
                    .delay = 0,
                },
                {
                    .seq_type = CAMERA_POW_SEQ_VREG,
                    .seq_val = CAMERA_VANA,
                    .config_val = 0,
                    .delay = 0,
                },
                {
                    .seq_type = CAMERA_POW_SEQ_VREG,
                    .seq_val = CAMERA_VDIG,
                    .config_val = 0,
                    .delay = 0,
                },
            },
            .size_down = 4,
        },

    },
    .num_channels = 1,
    .channels =
    {
        {
            .output_mode =
            {
                .fmt = QCARCAM_FMT_UYVY_8,
                .res = {.width = 720, .height = 507, .fps = 30.0f},
                .channel_info = {.vc = 0, .dt = DT_ADV7481, .cid = CID_VC0,},
                .interlaced = TRUE,
            },
            .num_subchannels = 1,
            .subchan_layout =
            {
                {
                    .src_id = 0,
                    .x_offset = 0,
                    .y_offset = 0,
                    .stride = 1440,
                },
            },
        },
    },
    .num_subchannels = 1,
    .subchannels =
    {
        {
            .src_id = 0,
            .modes =
            {
                {
                    .fmt = QCARCAM_FMT_UYVY_8,
                    .res = {.width = 720, .height = 507, .fps = 30.0f},
                    .channel_info = {.vc = 0, .dt = DT_ADV7481, .cid = CID_VC0,},
                    .interlaced = TRUE,
                },
            },
            .num_modes = 1,
        },
    },
    .sensor_output =
    {
        .output_format = SENSOR_YCBCR,
        .connection_mode = SENSOR_MIPI_CSI,
        .raw_output = SENSOR_8_BIT_DIRECT,
        .filter_arrangement = SENSOR_UYVY,
    },
    .csi_params =
    {
        .lane_cnt = 1,
        .settle_cnt = ADV7481_SETTLE_COUNT,
        .lane_mask = 0x1F,
        .combo_mode = 0,
        .is_csi_3phase = 0,
    },
    .sensor_close_lib = &adv7481_sensor_close_lib,
    .sensor_capability = 0x00000000,
    .sensor_custom_func =
    {
        .sensor_set_platform_func_table = &adv7481_sensor_set_platform_func_table,
        .sensor_power_on = &adv7481_sensor_power_on,
        .sensor_power_off = &adv7481_sensor_power_off,
        .sensor_power_suspend = &adv7481_sensor_power_suspend,
        .sensor_power_resume = &adv7481_sensor_power_resume,
        .sensor_detect_device = &adv7481_sensor_detect_device,
        .sensor_detect_device_channels = &adv7481_sensor_detect_device_channels,
        .sensor_init_setting = &adv7481_sensor_init_setting,
        .sensor_set_channel_mode = &adv7481_sensor_set_channel_mode,
        .sensor_start_stream = &adv7481_sensor_start_stream,
        .sensor_stop_stream = &adv7481_sensor_stop_stream,
		.sensor_query_field = &adv7481_sensor_query_field_info,
    },
    .use_sensor_custom_func = TRUE,
};


#define AIS_LOG_ADV_LIB(level, fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_LIB, level, fmt)

#define AIS_LOG_ADV_LIB_ERR(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_LIB, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_ADV_LIB_WARN(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_LIB, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_ADV_LIB_HIGH(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_LIB, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_ADV_LIB_MED(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_LIB, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_ADV_LIB_LOW(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_LIB, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_ADV_LIB_DBG(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_LIB, AIS_LOG_LVL_DBG, fmt)
#define AIS_LOG_ADV_LIB_DBG1(fmt...) AIS_LOG(AIS_MOD_ID_ADV7481_LIB, AIS_LOG_LVL_DBG1, fmt)


static int adv7481_sensor_event_handler(    s_adv7481_msg *p_msg, void *p_client)
{
   adv7481_context_t *pCtxt = (adv7481_context_t*)p_client;
   struct camera_input_status_t status;

   memset(&status, 0, sizeof(status));

   AIS_LOG_ADV_LIB_MED("E %s %p %p", __func__, p_msg, p_client);

   if (p_msg->msg == ADV7481_MSG_SIGNAL_VALID)
   {
      status.src = 0;
      status.event = INPUT_SIGNAL_VALID_EVENT;
   }
   else if (p_msg->msg == ADV7481_MSG_SIGNAL_LOST)
   {
      status.src = 0;
      status.event = INPUT_SIGNAL_LOST_EVENT;
   }
   pCtxt->sensor_platform_fcn_tbl.input_status_cb(pCtxt->ctrl, &status);

   AIS_LOG_ADV_LIB_MED("X %s %p %p", __func__, p_msg, p_client);

   return 0;
}

static void* adv7481_sensor_open_lib(void* ctrl, void* arg)
{
    int rc = 0;
    sensor_open_lib_t* device_info = (sensor_open_lib_t*)arg;
    adv7481_context_t* pCtxt = NULL;

    AIS_LOG_ADV_LIB_HIGH("E %s %p %p", __func__, ctrl, arg);

    if (ctrl == NULL || arg == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    pCtxt = (adv7481_context_t *)calloc(1, sizeof(adv7481_context_t));
    if (pCtxt == NULL)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    memcpy(&pCtxt->sensor_lib, &adv7481_lib_ptr, sizeof(pCtxt->sensor_lib));

    pCtxt->ctrl = ctrl;
    pCtxt->sensor_lib.sensor_slave_info.camera_id = device_info->camera_id;
    pCtxt->num_supported_sensors = 1;
    pCtxt->state = ADV7481_STATE_INVALID;

    CameraCreateMutex(&pCtxt->mutex);

    rc = adv7481_create(&pCtxt->p_drv);
    if (rc == 0)
    {
        rc = adv7481_register_callback(pCtxt->p_drv, adv7481_sensor_event_handler, pCtxt);
    }

EXIT_FLAG:

    if (rc != 0 && pCtxt != NULL)
    {
        CameraDestroyMutex(pCtxt->mutex);
        if (pCtxt->p_drv != NULL)
        {
            adv7481_destroy(pCtxt->p_drv);
            pCtxt = NULL;
        }
    }

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                    "X %s %p %p %p", __func__, ctrl, arg, pCtxt);

    return pCtxt;
}

static int adv7481_sensor_close_lib(void* ctxt)
{
    int rc = 0;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_HIGH("E %s %p", __func__, ctxt);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    rc = adv7481_destroy(pCtxt->p_drv);
    CameraDestroyMutex(pCtxt->mutex);

    free(ctxt);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                    "X %s %p", __func__, ctxt, rc);

    return rc;
}

static int adv7481_sensor_init_setting(void* ctxt)
{
    int rc = 0;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_MED("E %s %p", __func__, ctxt);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    rc = adv7481_config(pCtxt->p_drv);

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                    "X %s %p", __func__, ctxt, rc);

    return rc;
}


static int adv7481_sensor_detect_device(void* ctxt)
{
    int rc = -1;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_MED("E %s %p", __func__, ctxt);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    if (pCtxt->state < ADV7481_STATE_DETECTED)
    {
        rc = adv7481_check_chip(pCtxt->p_drv);
        if (rc == 0)
        {
            pCtxt->state = ADV7481_STATE_DETECTED;
        }
    }
    else
    {
        rc = 0;
    }

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                    "X %s %p", __func__, ctxt, rc);

    return rc;
}

static int adv7481_sensor_detect_device_channels(void* ctxt)
{
    int rc = 0;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_MED("E %s %p", __func__, ctxt);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    if (pCtxt->state == ADV7481_STATE_DETECTED)
    {
        pCtxt->sensor_lib.src_id_enable_mask = 1;
    }

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                    "X %s %p", __func__, ctxt, rc);

    return 0;
}

static int adv7481_sensor_set_channel_mode(void* ctxt, unsigned int src_id_mask, unsigned int mode)
{
    int rc = -1;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_MED("E %s %p %d %d", __func__, ctxt, src_id_mask, mode);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    if (pCtxt->state == ADV7481_STATE_DETECTED)
    {
        rc = adv7481_port_config(pCtxt->p_drv, pCtxt->p_drv->port);
        if (rc == 0)
        {
#ifndef CAMERA_UNITTEST
            rc = adv7481_port_wait_ready(pCtxt->p_drv, pCtxt->p_drv->port);
#endif
        }

        if (rc == 0)
        {
            pCtxt->num_connected_sensors = 1;
            pCtxt->state = ADV7481_STATE_INITIALIZED;
        }
    }
    else if (pCtxt->state > ADV7481_STATE_DETECTED)
    {
        rc = 0;
    }

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                    "X %s %p %d %d %d", __func__, ctxt, src_id_mask, mode, rc);

    return rc;
}

#ifdef ADV7481_INTR_USE_POLL
static void adv7481_sensor_isr(void* ctxt)
{
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;
    if (ctxt != NULL)
    {
        CameraLockMutex(pCtxt->mutex);
        if (pCtxt->state >= ADV7481_STATE_DETECTED && pCtxt->power_state)
        {
            adv7481_isr(pCtxt->p_drv);
        }
        CameraUnlockMutex(pCtxt->mutex);
    }
}
#else
static void adv7481_sensor_isr(void* ctxt)
{
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;
    if (ctxt != NULL)
    {
        adv7481_isr(pCtxt->p_drv);
    }
}
#endif

static int adv7481_sensor_power_on(void* ctxt)
{
    int rc = 0;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_MED("E %s %p", __func__, ctxt);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    rc = pCtxt->sensor_platform_fcn_tbl.sensor_execute_power_setting(pCtxt->ctrl,
            pCtxt->sensor_lib.sensor_slave_info.power_setting_array.power_up_setting_a,
            pCtxt->sensor_lib.sensor_slave_info.power_setting_array.size_up);
    if (rc == CAMERA_SUCCESS)
    {
        rc = adv7481_init(pCtxt->p_drv);
        if (rc == 0)
        {
            rc = pCtxt->sensor_platform_fcn_tbl.sensor_setup_gpio_interrupt(pCtxt->ctrl,
                    CAMERA_GPIO_INTR, adv7481_sensor_isr, pCtxt);
            rc = (rc == CAMERA_SUCCESS) ? 0 : -1;

            pCtxt->power_state = !rc;
        }
    }
    else
    {
        rc = -2;
    }

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                    "X %s %p %d", __func__, ctxt, rc);

    return rc;
}

static int adv7481_sensor_power_off(void* ctxt)
{
    int rc = 0;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_MED("E %s %p", __func__, ctxt);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    pCtxt->power_state = 0;

    rc = adv7481_deinit(pCtxt->p_drv);

    rc = pCtxt->sensor_platform_fcn_tbl.sensor_execute_power_setting(pCtxt->ctrl,
            pCtxt->sensor_lib.sensor_slave_info.power_setting_array.power_down_setting_a,
            pCtxt->sensor_lib.sensor_slave_info.power_setting_array.size_down);
    rc = (rc == CAMERA_SUCCESS) ? 0 : -1;


    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                    "X %s %p %d", __func__, ctxt, rc);

    return rc;
}


static int adv7481_sensor_power_resume(void* ctxt)
{
    int rc = 0;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_LOW("E %s %p", __func__, ctxt);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    pCtxt->power_state = 1;

#ifndef ADV7481_INTR_USE_POLL
    rc = adv7481_intr_enable(pCtxt->p_drv);
#endif

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                    "X %s %p", __func__, ctxt, rc);

    return rc;
}

static int adv7481_sensor_power_suspend(void* ctxt)
{
    int rc = 0;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_LOW("E %s %p", __func__, ctxt);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    pCtxt->power_state = 0;

    rc = adv7481_intr_disable(pCtxt->p_drv);

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                    "X %s %p %d", __func__, ctxt, rc);

    return rc;
}

static int adv7481_sensor_start_stream(void* ctxt, unsigned int src_id_mask)
{
    int rc = -1;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_LOW("E %s %p %x", __func__, ctxt, src_id_mask);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    if (pCtxt->state == ADV7481_STATE_INITIALIZED)
    {
        rc = adv7481_port_start(pCtxt->p_drv, pCtxt->p_drv->port);
        if (rc == 0)
        {
            pCtxt->state = ADV7481_STATE_STREAMING;
        }
    }
    else if (pCtxt->state == ADV7481_STATE_STREAMING)
    {
        rc = 0;
    }

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                    "X %s %p %x %d", __func__, ctxt, src_id_mask, rc);

    return rc;
}


static int adv7481_sensor_stop_stream(void* ctxt, unsigned int src_id_mask)
{
    int rc = -1;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_LOW("E %s %p %x", __func__, ctxt, src_id_mask);

    if (ctxt == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    CameraLockMutex(pCtxt->mutex);

    if (pCtxt->state == ADV7481_STATE_STREAMING)
    {
        rc = adv7481_port_stop(pCtxt->p_drv, pCtxt->p_drv->port);
        pCtxt->state = ADV7481_STATE_INITIALIZED;
    }
    else if (pCtxt->state == ADV7481_STATE_INITIALIZED)
    {
        rc = 0;
    }

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                    "X %s %p %x %d", __func__, ctxt, src_id_mask, rc);

    return rc;
}

static int adv7481_sensor_query_field_info (void* ctxt, boolean *p_is_even, uint64_t *p_ts)
{
    int rc = 0;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_DBG("E %s %p %p %p\n", __func__, ctxt, p_is_even, p_ts);

    if (ctxt == NULL || p_is_even == NULL || p_ts == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    *p_is_even = FALSE;
    *p_ts = 0;

    CameraLockMutex(pCtxt->mutex);

    if (pCtxt->state == ADV7481_STATE_STREAMING)
    {
        rc = adv7481_port_read_field_type(pCtxt->p_drv, pCtxt->p_drv->port, p_is_even);
        if (rc == 0)
        {
            struct timespec ts;

            clock_gettime(CLOCK_MONOTONIC, &ts);
            *p_ts = (uint64_t)(ts.tv_sec*1000000000 + ts.tv_nsec);

            AIS_LOG_ADV_LIB_DBG("%s:%d %d %lld\n", __func__, __LINE__, *p_is_even, *p_ts);
        }
    }
    else
    {
        rc = -2;
    }

    CameraUnlockMutex(pCtxt->mutex);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                    "X %s %p %p %p %d\n", __func__, ctxt, p_is_even, p_ts, rc);

    return rc;
}

static int adv7481_sensor_set_platform_func_table(void* ctxt, sensor_platform_func_table_t* table)
{
    int rc = 0;
    adv7481_context_t* pCtxt = (adv7481_context_t*)ctxt;

    AIS_LOG_ADV_LIB_DBG("E %s %p %p", __func__, ctxt, table);

    if (pCtxt == NULL || table ==  NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    pCtxt->sensor_platform_fcn_tbl = *table;

    rc = adv7481_register_ext_if(pCtxt->p_drv, table, pCtxt->ctrl);

EXIT_FLAG:

    AIS_LOG_ADV_LIB(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                    "X %s %p %p %d", __func__, ctxt, table, rc);

    return rc;
}


CAM_API CameraResult CameraSensorDevice_Open_adv7481(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId)
{
    sensor_lib_interface_t sensor_lib_interface = {
                            .sensor_open_lib = adv7481_sensor_open_lib,
    };

    return CameraSensorDevice_Open(ppNewHandle, deviceId, &sensor_lib_interface);
}
