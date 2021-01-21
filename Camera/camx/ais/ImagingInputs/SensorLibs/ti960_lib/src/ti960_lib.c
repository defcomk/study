/**
 * @file ti960_lib.c
 *
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "CameraSensorDeviceInterface.h"
#include "CameraEventLog.h"

#include "ti960_lib.h"
#include "SensorDebug.h"

/* Turn this on to disable interrupts on sleep to prevent
 *   wake up of MSM
 */
#define DISABLE_INTR_ON_SLEEP 1

/* Turn this on to initialize remote serializer
 *   - sets higher throughput through BCC
 */
#define INITIALIZE_SERIALIZER 0

#define TI960_SIGNAL_LOCK_NUM_TRIES 10 //10 tries with 20ms sleeps
#define TI960_SIGNAL_LOCK_WAIT 20
#define TI960_INTR_SETTLE_SLEEP 10 //10ms for now
#define SIGNAL_LOCKED TRUE
#define SIGNAL_LOST   FALSE

#define AIS_LOG_TI_LIB(level, fmt...) AIS_LOG(AIS_MOD_ID_TI960_LIB, level, fmt)
#define AIS_LOG_TI_LIB_ERR(fmt...) AIS_LOG(AIS_MOD_ID_TI960_LIB, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_TI_LIB_WARN(fmt...) AIS_LOG(AIS_MOD_ID_TI960_LIB, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_TI_LIB_ALWAYS(fmt...) AIS_LOG(AIS_MOD_ID_TI960_LIB, AIS_LOG_LVL_ALWAYS, fmt)
#define AIS_LOG_TI_LIB_INFO(fmt...) AIS_LOG(AIS_MOD_ID_TI960_LIB, AIS_LOG_LVL_INFO, fmt)
#define AIS_LOG_TI_LIB_VERB(fmt...) AIS_LOG(AIS_MOD_ID_TI960_LIB, AIS_LOG_LVL_VERB, fmt)
#define AIS_LOG_TI_LIB_DBG(fmt...) AIS_LOG(AIS_MOD_ID_TI960_LIB, AIS_LOG_LVL_DBG, fmt)
#define AIS_LOG_TI_LIB_FREQ(fmt...) AIS_LOG(AIS_MOD_ID_TI960_LIB, AIS_LOG_LVL_FREQ, fmt)

typedef enum
{
    SLAVE_STATE_INVALID = 0,
    SLAVE_STATE_SERIALIZER_DETECTED,
    SLAVE_STATE_DETECTED,
    SLAVE_STATE_INITIALIZED,
    SLAVE_STATE_STREAMING,
} ti960_slave_state_t;

/*Structure that holds information regarding slave devices*/
typedef struct
{
    ti960_slave_state_t slave_state;

    unsigned int rxport;
    unsigned int sensor_alias;
    unsigned int serializer_alias;
    unsigned int vc;
    unsigned int dt;
    unsigned int cid;
    unsigned int gpio_power_up;
    unsigned int gpio_power_down;
    unsigned int rxport_en;
    volatile unsigned int lock_status;
} ti960_slave_devices_info_t;

#define MAX_INIT_SEQUENCE_SIZE 256

typedef enum
{
    TI960_STATE_INVALID = 0,
    TI960_STATE_DETECTED,
    TI960_STATE_INITIALIZED,
    TI960_STATE_SUSPEND,
    TI960_STATE_STREAMING,
}ti960_state_t;

typedef struct
{
    /*must be first element*/
    sensor_lib_t sensor_lib;

    CameraMutex mutex;

    ti960_state_t state;
    unsigned int streaming_src_mask;

    unsigned int revision;
    unsigned int slave_addr;
    unsigned int num_supported_sensors;
    unsigned int num_connected_sensors;
    unsigned int sensor_i2c_tbl_initialized;
    sensor_platform_func_table_t sensor_platform_fcn_tbl;
    struct camera_i2c_reg_setting init_reg_settings;
    unsigned int rxport_en;

    struct camera_i2c_reg_setting ti960_reg_setting;
    struct camera_i2c_reg_setting slave_reg_setting;

    struct camera_i2c_reg_array init_seq[MAX_INIT_SEQUENCE_SIZE];

    unsigned int slave_alias_group;
    ti960_slave_devices_info_t ti960_sensors[NUM_SUPPORT_SENSORS];

    boolean enable_intr;
    volatile boolean intr_in_process;

    void* ctrl;
}ti960_context_t;

//external
static int ti960_sensor_close_lib(void* ctxt);
static int ti960_sensor_power_suspend(void* ctxt);
static int ti960_sensor_power_resume(void* ctxt);
static int ti960_sensor_detect_device(void* ctxt);
static int ti960_sensor_detect_device_channels(void* ctxt);
static int ti960_sensor_init_setting(void* ctxt);
static int ti960_sensor_set_channel_mode(void* ctxt, unsigned int src_id_mask, unsigned int mode);
static int ti960_sensor_start_stream(void* ctxt, unsigned int src_id_mask);
static int ti960_sensor_stop_stream(void* ctxt, unsigned int src_id_mask);
static int ti960_sensor_set_platform_func_table(void* ctxt, sensor_platform_func_table_t* table);
static int ti960_calculate_exposure(void* ctxt, unsigned int src_id, sensor_exposure_info_t* exposure_info);
static int ti960_sensor_exposure_config(void* ctxt, unsigned int src_id, sensor_exposure_info_t* exposure_info);
static int ti960_sensor_s_param(void* ctxt, qcarcam_param_t param, unsigned int src_id, void* p_param);
static int ti960_read_status(ti960_context_t* pCtxt, uint32 port);
static int ti960_enable_intr_pin(ti960_context_t* pCtxt);
static int ti960_disable_intr_pin(ti960_context_t* pCtxt);
static int ti960_set_init_sequence(ti960_context_t* pCtxt);
static int ti960_read_int_sts (ti960_context_t* pCtxt, unsigned short * intr_src_mask);
static int ti960_read_lock_pass_status(ti960_context_t* pCtxt, boolean* lock_chg, boolean* lock_pass, uint32 rx_port);
static void ti960_intr_handler(void* data);
static int slave_reset_indirect_reg(ti960_context_t* pCtxt, uint32 src_id);

static sensor_lib_t sensor_lib_ptr =
{
  .sensor_slave_info =
  {
      .sensor_name = SENSOR_MODEL,
      .slave_addr = TI960_0_SLAVEADDR,
      .i2c_freq_mode = SENSOR_I2C_MODE_CUSTOM,
      .addr_type = CAMERA_I2C_BYTE_ADDR,
      .data_type = CAMERA_I2C_BYTE_DATA,
      .is_init_params_valid = 1,
      .sensor_id_info =
      {
        .sensor_id = TI960_0_SLAVEADDR,
        .sensor_id_reg_addr = 0x00,
        .sensor_id_mask = 0xff00,
      },
      .power_setting_array =
      {
        .power_up_setting_a =
        {
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
            .delay = 0,
          },
          {
            .seq_type = CAMERA_POW_SEQ_GPIO,
            .seq_val = CAMERA_GPIO_RESET,
            .config_val = GPIO_OUT_LOW,
            .delay = 2,
          },
          {
            .seq_type = CAMERA_POW_SEQ_GPIO,
            .seq_val = CAMERA_GPIO_RESET,
            .config_val = GPIO_OUT_HIGH,
            .delay = 2,
          },
        },
        .size_up = 5,
        .power_down_setting_a =
        {
          {
            .seq_type = CAMERA_POW_SEQ_GPIO,
            .seq_val = CAMERA_GPIO_RESET,
            .config_val = GPIO_OUT_LOW,
            .delay = 0,
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
  .num_channels = NUM_SUPPORT_SENSORS,
  .channels =
  {
    {
      .output_mode =
      {
        .fmt = _IMG_SRC_COLOR_FMT,
        .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
        .channel_info = {.vc = 0, .dt = DT_960, .cid = CID_VC0,},
      },
      .num_subchannels = 1,
      .subchan_layout =
      {
        {
          .src_id = 0,
          .x_offset = 0,
          .y_offset = 0,
          .stride = 1280,
        },
      },
    },
    {
      .output_mode =
      {
        .fmt = _IMG_SRC_COLOR_FMT,
        .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
        .channel_info = {.vc = 1, .dt = DT_960, .cid = CID_VC1,},
      },
      .num_subchannels = 1,
      .subchan_layout =
      {
        {
          .src_id = 1,
          .x_offset = 0,
          .y_offset = 0,
          .stride = 1280,
        },
      },
    },
    {
      .output_mode =
      {
        .fmt = _IMG_SRC_COLOR_FMT,
        .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
        .channel_info = {.vc = 2, .dt = DT_960, .cid = CID_VC2,},
      },
      .num_subchannels = 1,
      .subchan_layout =
      {
        {
          .src_id = 2,
          .x_offset = 0,
          .y_offset = 0,
          .stride = 1280,
        },
      },
    },
    {
      .output_mode =
      {
        .fmt = _IMG_SRC_COLOR_FMT,
        .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
        .channel_info = {.vc = 3, .dt = DT_960, .cid = CID_VC3,},
      },
      .num_subchannels = 1,
      .subchan_layout =
      {
        {
          .src_id = 3,
          .x_offset = 0,
          .y_offset = 0,
          .stride = 1280,
        },
      },
    },
  },
  .num_subchannels = NUM_SUPPORT_SENSORS,
  .subchannels =
  {
    {
      .src_id = 0,
      .modes =
      {
        {
          .fmt = _IMG_SRC_COLOR_FMT,
          .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
          .channel_info = {.vc = 0, .dt = DT_960, .cid = CID_VC0,},
        },
      },
      .num_modes = 1,
    },
    {
      .src_id = 1,
      .modes =
      {
        {
          .fmt = _IMG_SRC_COLOR_FMT,
          .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
          .channel_info = {.vc = 1, .dt = DT_960, .cid = CID_VC1,},
        },
      },
      .num_modes = 1,
    },
    {
      .src_id = 2,
      .modes =
      {
        {
          .fmt = _IMG_SRC_COLOR_FMT,
          .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
          .channel_info = {.vc = 2, .dt = DT_960, .cid = CID_VC2,},
        },
      },
      .num_modes = 1,
    },
    {
      .src_id = 3,
      .modes =
      {
        {
          .fmt = _IMG_SRC_COLOR_FMT,
          .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
          .channel_info = {.vc = 3, .dt = DT_960, .cid = CID_VC3,},
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
  .sensor_num_frame_skip = 1,
  .sensor_num_HDR_frame_skip = 2,
  .sensor_max_pipeline_frame_delay = 1,
  .pixel_array_size_info =
  {
    .active_array_size =
    {
      .width = SENSOR_WIDTH,
      .height = SENSOR_HEIGHT,
    },
    .left_dummy = 0,
    .right_dummy = 0,
    .top_dummy = 0,
    .bottom_dummy = 0,
  },
  .out_info_array =
  {
    .out_info =
    {
      {
        .x_output = _CAMTIMING_XOUT,
        .y_output = _CAMTIMING_YOUT,
        .line_length_pclk = _CAMTIMING_CLKS_PER_LINE,
        .frame_length_lines = _CAMTIMING_TOTAL_LINES,
        .vt_pixel_clk = _CAMTIMING_VT_CLK,
        .op_pixel_clk = _CAMTIMING_OP_CLK,
        .binning_factor = 1,
        .min_fps = 30.0,
        .max_fps = 30.0,
        .mode = SENSOR_DEFAULT_MODE,
        .offset_x = 0,
        .offset_y = 0,
        .scale_factor = 0,
      },
    },
    .size = 1,
  },
  .csi_params =
  {
    .lane_cnt = 4,
    .settle_cnt = SETTLE_COUNT,
    .lane_mask = 0x1F,
    .combo_mode = 0,
    .is_csi_3phase = 0,
  },
  .sensor_close_lib = ti960_sensor_close_lib,
  .exposure_func_table =
  {
    .sensor_calculate_exposure = &ti960_calculate_exposure,
    .sensor_exposure_config = &ti960_sensor_exposure_config,
  },
  .sensor_capability = (1 << SENSOR_CAPABILITY_EXPOSURE_CONFIG),
  .sensor_custom_func =
  {
    .sensor_set_platform_func_table = &ti960_sensor_set_platform_func_table,
    .sensor_power_suspend = ti960_sensor_power_suspend,
    .sensor_power_resume = ti960_sensor_power_resume,
    .sensor_detect_device = &ti960_sensor_detect_device,
    .sensor_detect_device_channels = &ti960_sensor_detect_device_channels,
    .sensor_init_setting = &ti960_sensor_init_setting,
    .sensor_set_channel_mode = &ti960_sensor_set_channel_mode,
    .sensor_start_stream = &ti960_sensor_start_stream,
    .sensor_stop_stream = &ti960_sensor_stop_stream,
    .sensor_s_param = &ti960_sensor_s_param,
  },
  .use_sensor_custom_func = TRUE,
};


static ti960_slave_devices_info_t ti960_sensors_init_table[] =
{
  {
      .rxport = 0,
      .sensor_alias = TI960_0_SENSOR_ALIAS0,
      .serializer_alias = TI960_0_SERIALIZER_ALIAS_RX0,
      .vc = VC0,
      .dt = CSI_DT_YUV422_8,
      .cid = CID_VC0,
      .gpio_power_up = PWR_GPIO_UP,
      .gpio_power_down = PWR_GPIO_DOWN,
      .rxport_en = RXPORT_EN_RX0,
  },
  {
      .rxport = 1,
      .sensor_alias = TI960_0_SENSOR_ALIAS1,
      .serializer_alias = TI960_0_SERIALIZER_ALIAS_RX1,
      .vc = VC1,
      .dt = CSI_DT_YUV422_8,
      .cid = CID_VC1,
      .gpio_power_up = PWR_GPIO_UP,
      .gpio_power_down = PWR_GPIO_DOWN,
      .rxport_en = RXPORT_EN_RX1,
  },
  {
      .rxport = 2,
      .sensor_alias = TI960_0_SENSOR_ALIAS2,
      .serializer_alias = TI960_0_SERIALIZER_ALIAS_RX2,
      .vc = VC2,
      .dt = CSI_DT_YUV422_8,
      .cid = CID_VC2,
      .gpio_power_up = PWR_GPIO_UP,
      .gpio_power_down = PWR_GPIO_DOWN,
      .rxport_en = RXPORT_EN_RX2,
  },
  {
      .rxport = 3,
      .sensor_alias = TI960_0_SENSOR_ALIAS3,
      .serializer_alias = TI960_0_SERIALIZER_ALIAS_RX3,
      .vc = VC3,
      .dt = CSI_DT_YUV422_8,
      .cid = CID_VC3,
      .gpio_power_up = PWR_GPIO_UP,
      .gpio_power_down = PWR_GPIO_DOWN,
      .rxport_en = RXPORT_EN_RX3,
  },
};

static struct camera_i2c_reg_array slave_init_reg_array[] = INIT_SLAVE(TI960_REV_TX_8BIT);
static struct camera_i2c_reg_array slave_start_reg_array[] = START_SLAVE;
static struct camera_i2c_reg_array slave_stop_reg_array[] = STOP_SLAVE;

#if INITIALIZE_SERIALIZER
static struct camera_i2c_reg_array ti960_serializer_regs[] = INIT_SERIALIZER;
static struct camera_i2c_reg_setting ti960_serializer_setting =
{
    .reg_array = ti960_serializer_regs,
    .size = STD_ARRAY_SIZE(ti960_serializer_regs),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};
#endif

static struct camera_i2c_reg_array ti960_start_reg_array[] = START_REG_ARRAY;
static struct camera_i2c_reg_array ti960_stop_reg_array[] = STOP_REG_ARRAY;
static struct camera_i2c_reg_array ti960_init_intr_reg_array[] = INIT_INTR_ARRAY;

/**
 * FUNCTION: ti960_sensor_open_lib
 *
 * DESCRIPTION: Open sensor library and returns data pointer
 **/
static void* ti960_sensor_open_lib(void* ctrl, void* arg)
{
    unsigned int i=0;

    ti960_context_t* pCtxt = calloc(1, sizeof(ti960_context_t));
    if (pCtxt)
    {
        sensor_open_lib_t* device_info = (sensor_open_lib_t*)arg;

        // default to dev id 0
        pCtxt->slave_addr = TI960_0_SLAVEADDR;
        pCtxt->slave_alias_group = TI960_0_SENSOR_ALIAS_GROUP;
        memcpy(&pCtxt->sensor_lib, &sensor_lib_ptr, sizeof(pCtxt->sensor_lib));
        memcpy(&pCtxt->ti960_sensors, ti960_sensors_init_table, sizeof(pCtxt->ti960_sensors));

        pCtxt->ctrl = ctrl;
        pCtxt->state = TI960_STATE_INVALID;
        pCtxt->sensor_lib.sensor_slave_info.camera_id = device_info->camera_id;
        pCtxt->num_supported_sensors = NUM_SUPPORT_SENSORS;
        pCtxt->revision = TI960_REV_ID_3;
        pCtxt->rxport_en = 0xff;

        pCtxt->ti960_reg_setting.addr_type = CAMERA_I2C_BYTE_ADDR;
        pCtxt->ti960_reg_setting.data_type = CAMERA_I2C_BYTE_DATA;

        pCtxt->slave_reg_setting.addr_type = CAMERA_I2C_WORD_ADDR;
        pCtxt->slave_reg_setting.data_type = SENSOR_I2C_DATA_TYPE;

        for (i=0; i<pCtxt->num_supported_sensors; i++)
        {
            pCtxt->ti960_sensors[i].slave_state = SLAVE_STATE_INVALID;
        }

        // override values based on device ID
        if (device_info->subdev_id == 1)
        {
            pCtxt->slave_addr = TI960_1_SLAVEADDR;
            pCtxt->slave_alias_group = TI960_1_SENSOR_ALIAS_GROUP;
            pCtxt->sensor_lib.sensor_slave_info.slave_addr = pCtxt->slave_addr;
            pCtxt->sensor_lib.sensor_slave_info.sensor_id_info.sensor_id = pCtxt->slave_addr;
            pCtxt->ti960_sensors[0].sensor_alias = TI960_1_SENSOR_ALIAS0;
            pCtxt->ti960_sensors[0].serializer_alias = TI960_1_SERIALIZER_ALIAS_RX0;
            pCtxt->ti960_sensors[1].sensor_alias = TI960_1_SENSOR_ALIAS1;
            pCtxt->ti960_sensors[1].serializer_alias = TI960_1_SERIALIZER_ALIAS_RX1;
            pCtxt->ti960_sensors[2].sensor_alias = TI960_1_SENSOR_ALIAS2;
            pCtxt->ti960_sensors[2].serializer_alias = TI960_1_SERIALIZER_ALIAS_RX2;
            pCtxt->ti960_sensors[3].sensor_alias = TI960_1_SENSOR_ALIAS3;
            pCtxt->ti960_sensors[3].serializer_alias = TI960_1_SERIALIZER_ALIAS_RX3;
        }

        CameraCreateMutex(&pCtxt->mutex);
    }

    return pCtxt;
}

static int ti960_sensor_close_lib(void* ctxt)
{
    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;
    CameraDestroyMutex(pCtxt->mutex);
    free(pCtxt);
    return 0;
}

#define ADD_I2C_REG_ARRAY(_a_, _size_, _addr_, _val_, _delay_) \
do { \
    _a_[_size_].reg_addr = _addr_; \
    _a_[_size_].reg_data = _val_; \
    _a_[_size_].delay = _delay_; \
    _size_++; \
} while(0)

#define TI960_PORT_SEL(_rxport_) ((1 << (_rxport_ & 0x3)) | ((_rxport_ & 0x3) << 4))

static int ti960_set_init_sequence(ti960_context_t* pCtxt)
{
    struct camera_i2c_reg_array *init_seq = pCtxt->init_seq;
    struct camera_i2c_reg_array dummy_reg[1];

    unsigned short init_seq_size = 0;
    unsigned int i = 0;
    int ret = 0;

    /*detect far end serializer*/
    pCtxt->ti960_reg_setting.delay = 0;
    pCtxt->ti960_reg_setting.reg_array = dummy_reg;
    pCtxt->ti960_reg_setting.size = STD_ARRAY_SIZE(dummy_reg);

    pCtxt->ti960_reg_setting.reg_array[0].delay = 0;
    for (i=0; i<pCtxt->num_supported_sensors; i++)
    {
        /*LOCK to protect RX_SELECT register and subsequent RX operations*/
        CameraLockMutex(pCtxt->mutex);

        pCtxt->ti960_reg_setting.reg_array[0].reg_addr = 0x4C;
        pCtxt->ti960_reg_setting.reg_array[0].reg_data = TI960_PORT_SEL(pCtxt->ti960_sensors[i].rxport);
        ret = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                pCtxt->ctrl,
                pCtxt->slave_addr,
                &pCtxt->ti960_reg_setting);
        if (ret)
        {
            AIS_LOG_TI_LIB_ERR("Failed to set rxport%d", pCtxt->ti960_sensors[i].rxport);
            continue;
        }

        pCtxt->ti960_reg_setting.reg_array[0].reg_addr = 0x5B;
        ret = pCtxt->sensor_platform_fcn_tbl.i2c_slave_read(
                pCtxt->ctrl,
                pCtxt->slave_addr,
                &pCtxt->ti960_reg_setting);

        CameraUnlockMutex(pCtxt->mutex);

        if (ret)
        {
            AIS_LOG_TI_LIB_ERR("Unable to read far end serializer for rxport%d", pCtxt->ti960_sensors[i].rxport);
            continue;
        }
        else
        {
            /*if SER ALIAS ID not 0 then far end serializer is present*/
            if (pCtxt->ti960_reg_setting.reg_array[0].reg_data & 0xFE)
            {
                pCtxt->ti960_sensors[i].slave_state = SLAVE_STATE_SERIALIZER_DETECTED;
            }
        }
    }

    /*set init sequence*/
#if INITIALIZE_SERIALIZER
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x0A, 0x32, _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x0B, 0x32, _reg_delay_);
#endif
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x10, 0x81, _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x11, 0x85, _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x12, 0x89, _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x13, 0x8D, _reg_delay_);
    if (pCtxt->revision == TI960_REV_ID_1)
    {
        ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x1f, CSI_800MBPS_REV_1, _reg_delay_);
    }
    else
    {
        ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x1f, CSI_800MBPS_REV_2, _reg_delay_);
    }
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x32, 0x01, _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x33, 0x01, _reg_delay_);

    i = 0;
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x4c, 0x0f, _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x58, 0x58, _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x5d, SENSOR_ID, _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x7c, (_MODE_8_10|_FV_POLARITY), _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x6f, 0x08, _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x6d, (0x7c|_MODE_FPD3_INPUT_RAW), _reg_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x6e, pCtxt->ti960_sensors[i].gpio_power_down, _start_delay_);
    ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x6e, pCtxt->ti960_sensors[i].gpio_power_up, _start_delay_);

    for (i=0; i<pCtxt->num_supported_sensors; i++)
    {
        if (pCtxt->ti960_sensors[i].slave_state != SLAVE_STATE_SERIALIZER_DETECTED)
            continue;
        ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x4c, TI960_PORT_SEL(pCtxt->ti960_sensors[i].rxport), _reg_delay_);
        ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x5c, pCtxt->ti960_sensors[i].serializer_alias, _reg_delay_);
        ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x65, pCtxt->ti960_sensors[i].sensor_alias, _reg_delay_);
        ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x70, (pCtxt->ti960_sensors[i].vc|pCtxt->ti960_sensors[i].dt), _reg_delay_); /* RAW10 setting*/
        ADD_I2C_REG_ARRAY(init_seq, init_seq_size, 0x71, (pCtxt->ti960_sensors[i].vc|pCtxt->ti960_sensors[i].dt), _reg_delay_); /* RAW12 setting*/
    }

    pCtxt->init_reg_settings.reg_array = init_seq;
    pCtxt->init_reg_settings.size = init_seq_size;
    pCtxt->init_reg_settings.addr_type = CAMERA_I2C_BYTE_ADDR;
    pCtxt->init_reg_settings.data_type = CAMERA_I2C_BYTE_DATA;
    /*LOCK to protect RX_SELECT register and subsequent RX operations*/
    CameraLockMutex(pCtxt->mutex);
    ret = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pCtxt->slave_addr,
            &pCtxt->init_reg_settings);
    CameraUnlockMutex(pCtxt->mutex);
    if (ret)
    {
        AIS_LOG_TI_LIB_ERR("Failed to set init sequence");
    }

    //Clearing reg_setting as it was set to a local variable
    pCtxt->ti960_reg_setting.reg_array = NULL;
    return ret;
}

static int ti960_enable_intr_pin(ti960_context_t* pCtxt)
{
    int rc = 0;
#if DISABLE_INTR_ON_SLEEP
    struct camera_i2c_reg_array ti960_enable_intr[] = {{ 0x23, 0x8f, 0 },};

    pCtxt->ti960_reg_setting.reg_array = ti960_enable_intr;
    pCtxt->ti960_reg_setting.size = STD_ARRAY_SIZE(ti960_enable_intr);

    rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pCtxt->slave_addr,
            &pCtxt->ti960_reg_setting);
    if (rc)
    {
        AIS_LOG_TI_LIB_ERR("ti960 0x%x failed to stop", pCtxt->slave_addr);
    }
#endif

    return rc;
}

static int ti960_disable_intr_pin(ti960_context_t* pCtxt)
{
    int rc = 0;
#if DISABLE_INTR_ON_SLEEP
    struct camera_i2c_reg_array ti960_disable_intr[] = {{ 0x23, 0x0, 0 },};

    pCtxt->ti960_reg_setting.reg_array = ti960_disable_intr;
    pCtxt->ti960_reg_setting.size = STD_ARRAY_SIZE(ti960_disable_intr);

    rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pCtxt->slave_addr,
            &pCtxt->ti960_reg_setting);
    if (rc)
    {
        AIS_LOG_TI_LIB_ERR("ti960 0x%x failed to stop", pCtxt->slave_addr);
    }
#endif

    return rc;
}

static int ti960_sensor_power_suspend(void* ctxt)
{
    int rc = 0;
    int num_tries = 0;

    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;

    ti960_disable_intr_pin(pCtxt);

    pCtxt->state = TI960_STATE_SUSPEND;

    while (pCtxt->intr_in_process && num_tries < 3)
    {
        CameraSleep(TI960_INTR_SETTLE_SLEEP);
        num_tries++;
    }

    return rc;
}

static int ti960_sensor_power_resume(void* ctxt)
{
    int rc = 0;

    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;

    pCtxt->state = TI960_STATE_INITIALIZED;

    rc = ti960_enable_intr_pin(pCtxt);

    ti960_intr_handler(pCtxt);

    return rc;
}

static int ti960_sensor_detect_device(void* ctxt)
{
    int rc = 0;

    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;
    struct camera_i2c_reg_array dummy_reg[1];

    AIS_LOG_TI_LIB_DBG("detect ti960 0x%x", pCtxt->slave_addr);

    if (pCtxt->state >= TI960_STATE_DETECTED)
    {
        AIS_LOG_TI_LIB_DBG("already detected");
        return 0;
    }

    if (!pCtxt->sensor_i2c_tbl_initialized ||
        !pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array ||
        !pCtxt->sensor_platform_fcn_tbl.i2c_slave_read)
    {
      AIS_LOG_TI_LIB_ERR("I2C function table not initialized");
      return -1;
    }

    /*read bridge chip slave ID*/
    pCtxt->ti960_reg_setting.delay = 0;
    pCtxt->ti960_reg_setting.reg_array = dummy_reg;
    pCtxt->ti960_reg_setting.size = STD_ARRAY_SIZE(dummy_reg);

    pCtxt->ti960_reg_setting.reg_array[0].delay = 0;
    pCtxt->ti960_reg_setting.reg_array[0].reg_addr = pCtxt->sensor_lib.sensor_slave_info.sensor_id_info.sensor_id_reg_addr;
    if ((rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_read(
                pCtxt->ctrl,
                pCtxt->slave_addr,
                &pCtxt->ti960_reg_setting)))
    {
            AIS_LOG_TI_LIB_ERR("Unable to read chip ID");
            goto error;
    }

    if (pCtxt->sensor_lib.sensor_slave_info.sensor_id_info.sensor_id != pCtxt->ti960_reg_setting.reg_array[0].reg_data)
    {
        AIS_LOG_TI_LIB_ERR("Chip ID does not match. Expect 0x%x, Read back 0x%x",
            pCtxt->sensor_lib.sensor_slave_info.sensor_id_info.sensor_id,
            pCtxt->ti960_reg_setting.reg_array[0].reg_data);
        goto error;
    }

    /*read bridge chip revision ID*/
    pCtxt->ti960_reg_setting.reg_array[0].reg_addr = TI960_REV_ADDR;
    if ((rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_read(
                pCtxt->ctrl,
                pCtxt->slave_addr,
                &pCtxt->ti960_reg_setting)))
    {
            AIS_LOG_TI_LIB_ERR("Unable to read bridge chip revision ID");
            goto error;
    }

    pCtxt->revision = pCtxt->ti960_reg_setting.reg_array[0].reg_data;
    switch(pCtxt->revision)
    {
        case TI960_REV_ID_1: {
            rc = -1;
            AIS_LOG_TI_LIB_ERR("ti960 rev 1 unsupported. PLEASE UPGRADE HW TO TI960 REV 3");
            goto error;
        }
        case TI960_REV_ID_2:
            /*support will be deprecated as soon as enough HW out there*/
            AIS_LOG_TI_LIB_ERR("ti960 rev 2 support will be deprecated. PLEASE UPGRADE HW TO TI960 REV 3");
            AIS_LOG_TI_LIB_ERR("\tKPIs may be adversely affected. Do NOT use for KPI measurement.");
            AIS_LOG_TI_LIB_ERR("\tSome use cases may not work as expected.");
            break;
        case TI960_REV_ID_3:
            AIS_LOG_TI_LIB_DBG("ti960 rev 3 detected. You have the latest known good TI960 HW.");
            break;
        default: {
            rc = -1;
            AIS_LOG_TI_LIB_ERR("ti960 unknown rev found 0x%x", pCtxt->ti960_reg_setting.reg_array[0].reg_data);
            goto error;
        }
    }

    pCtxt->state = TI960_STATE_DETECTED;

error:
    return rc;
}


static int ti960_read_int_sts(ti960_context_t* pCtxt, unsigned short *intr_src_mask)
{
    int rc = 0;
    unsigned short inter_ports = 0;

    int i = 0;
    struct camera_i2c_reg_setting ti960_reg_setting;
    struct camera_i2c_reg_array dummy_reg[1];

    if (pCtxt == NULL)
    {
        AIS_LOG_TI_LIB_ERR("Invalid ti960 context state");
        return -1;
    }

    if (intr_src_mask == NULL)
    {
        AIS_LOG_TI_LIB_ERR("Invalid src mask");
        return -1;
    }

    ti960_reg_setting.delay = 0;
    ti960_reg_setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    ti960_reg_setting.data_type = CAMERA_I2C_BYTE_DATA;
    ti960_reg_setting.reg_array = dummy_reg;
    ti960_reg_setting.size = STD_ARRAY_SIZE(dummy_reg);
    ti960_reg_setting.reg_array[0].delay = 0;
    ti960_reg_setting.reg_array[0].reg_addr = 0x24; //INTS register

    rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_read(
                pCtxt->ctrl,
                pCtxt->slave_addr,
                &ti960_reg_setting);
    if (rc)
    {
        AIS_LOG_TI_LIB_ERR("Failed to read INTS register");
        return -rc;

    }
    else
    {
        AIS_LOG_TI_LIB_DBG("read INT status as = 0x%x", ti960_reg_setting.reg_array[0].reg_data);
    }

    for (i = 0; i < 4; i++)
    {
        if (ti960_reg_setting.reg_array[0].reg_data & (1 << i))
        {
            inter_ports |= 1 << i;
        }
    }

    *intr_src_mask = inter_ports;

    return rc;
}

static int ti960_read_lock_pass_status(ti960_context_t* pCtxt, boolean* lock_chg, boolean* lock_pass, uint32 rx_port)
{
    int rc = 0;
    unsigned short sts0_data, sts1_data, sts2_data;
    struct camera_i2c_reg_setting ti960_reg_setting;
    struct camera_i2c_reg_array dummy_reg[1];

    ti960_reg_setting.delay = 0;
    ti960_reg_setting.addr_type = CAMERA_I2C_BYTE_ADDR;
    ti960_reg_setting.data_type = CAMERA_I2C_BYTE_DATA;
    ti960_reg_setting.reg_array = dummy_reg;
    ti960_reg_setting.size = STD_ARRAY_SIZE(dummy_reg);
    ti960_reg_setting.reg_array[0].delay = 0;
    ti960_reg_setting.reg_array[0].reg_addr = 0x4C;
    ti960_reg_setting.reg_array[0].reg_data = TI960_PORT_SEL(rx_port);

    /*LOCK to protect RX_SELECT register and subsequent RX operations*/
    CameraLockMutex(pCtxt->mutex);

    rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                pCtxt->ctrl,
                pCtxt->slave_addr,
                &ti960_reg_setting);
    if (rc)
    {
        AIS_LOG_TI_LIB_ERR("Failed to set port %d ", rx_port);
    }
    else
    {
        //Read Port status register
        ti960_reg_setting.reg_array[0].reg_addr = 0xDB;
        rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_read(
                    pCtxt->ctrl,
                    pCtxt->slave_addr,
                    &ti960_reg_setting);
        sts0_data = ti960_reg_setting.reg_array[0].reg_data;

        ti960_reg_setting.reg_array[0].reg_addr = 0x4D; //RX_PORT_STS1
        rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_read(
                    pCtxt->ctrl,
                    pCtxt->slave_addr,
                    &ti960_reg_setting);
        sts1_data = ti960_reg_setting.reg_array[0].reg_data;

        ti960_reg_setting.reg_array[0].reg_addr = 0x4E; //RX_PORT_STS2
        rc |= pCtxt->sensor_platform_fcn_tbl.i2c_slave_read(
                    pCtxt->ctrl,
                    pCtxt->slave_addr,
                    &ti960_reg_setting);
        sts2_data = ti960_reg_setting.reg_array[0].reg_data;
        if (rc)
        {
            AIS_LOG_TI_LIB_ERR("Failed to read RX_PORT_STATUS for port %d", rx_port);
        }

        *lock_chg = (sts0_data & 0x03) ? TRUE : FALSE;
        *lock_pass = ((sts1_data & 0x03) == 0x3) ? SIGNAL_LOCKED : SIGNAL_LOST;
        AIS_LOG_TI_LIB_DBG("read RX port %d status %d %d [0x%x, 0x%x, 0x%x]", rx_port,
                                  *lock_chg, *lock_pass,
                                  sts0_data, sts1_data, sts2_data);

        if (sts2_data & 0x80)
        {
            AIS_LOG_TI_LIB_ERR("RX port %d: LINE LEN UNSTABLE", rx_port);
        }
    }

    CameraUnlockMutex(pCtxt->mutex);

    return rc;
}


static int ti960_read_status(ti960_context_t* pCtxt, uint32 port)
{
    boolean lock_sts = 0;
    boolean lock_sts_chg = 0;
    int rc = 0;

    rc = ti960_read_lock_pass_status(pCtxt, &lock_sts_chg, &lock_sts, port);
    if (rc < 0)
    {
        AIS_LOG_TI_LIB_ERR("ti960_read_status failed for port %d.", port);
    }
    else
    {
        pCtxt->ti960_sensors[port].lock_status = lock_sts;
    }
    return rc;
}

static void ti960_intr_handler(void* data)
{
    int i = 0;
    int rc = 0;
    boolean lock_sts_chg[4] = {0,0,0,0};
    boolean lock_sts_chg2[4] = {0,0,0,0};
    boolean lock_sts[4] = {0,0,0,0};
    unsigned short intr_srcs = 0;

    ti960_context_t* pCtxt = (ti960_context_t*)data;

    if(pCtxt == NULL)
    {
        AIS_LOG_TI_LIB_ERR("Invalid ti960 context");
        return;
    }

    if (pCtxt->state < TI960_STATE_INITIALIZED)
    {
        AIS_LOG_TI_LIB_ERR("Invalid ti960 context state");
        return;
    }

    if (pCtxt->state == TI960_STATE_SUSPEND)
    {
        AIS_LOG_TI_LIB_DBG("Received interrupt in suspend state");
        return;
    }

    pCtxt->intr_in_process = TRUE;
    AIS_LOG_TI_LIB_ERR("ti960_intr_handler %x!", pCtxt->slave_addr);

    rc = ti960_read_int_sts(pCtxt, &intr_srcs);
    if (rc < 0)
    {
        pCtxt->intr_in_process = FALSE;
        AIS_LOG_TI_LIB_ERR("Failed to read INTS status");
        return;
    }

    while (intr_srcs & 0x0F)
    {
        /* Read the PORT status for all ports */
        for (i = 0; i < 4; i++)
        {
            if (!(intr_srcs & (1 << i)))
                continue;

            rc = ti960_read_lock_pass_status(pCtxt, &lock_sts_chg[i], &lock_sts[i], i);
            if (rc < 0)
            {
                AIS_LOG_TI_LIB_ERR("lock_pass_status read failed for port %d", i);
                lock_sts_chg[i] = 1;
                lock_sts[i] = SIGNAL_LOST;
            }
        }

        /* Send LOST if we lost lock */
        for (i = 0; i < 4; i++)
        {
            if (!(intr_srcs & (1 << i)))
                continue;

            if (lock_sts_chg[i] && pCtxt->ti960_sensors[i].lock_status == SIGNAL_LOCKED)
            {
                struct camera_input_status_t sensor_status;
                sensor_status.src = i;
                sensor_status.event = INPUT_SIGNAL_LOST_EVENT;
                pCtxt->sensor_platform_fcn_tbl.input_status_cb(pCtxt->ctrl, &sensor_status);
            }

            pCtxt->ti960_sensors[i].lock_status = lock_sts[i];
        }

        CameraSleep(TI960_INTR_SETTLE_SLEEP);

        rc = ti960_read_int_sts(pCtxt, &intr_srcs);
        if (rc < 0)
        {
            pCtxt->intr_in_process = FALSE;
            AIS_LOG_TI_LIB_ERR("Failed to read INTS status");
            return;
        }

        /* Read the PORT status for all ports again*/
        for (i = 0; i < 4; i++)
        {
            if (!(intr_srcs & (1 << i)))
                continue;

            rc = ti960_read_lock_pass_status(pCtxt, &lock_sts_chg2[i], &lock_sts[i], i);
            if (rc < 0)
            {
                AIS_LOG_TI_LIB_ERR("lock_pass_status read failed for port %d.", i);
                lock_sts_chg2[i] = 1;
                lock_sts[i] = SIGNAL_LOST;
            }
        }


        /* Send LOST if we lost lock */
        for (i = 0; i < 4; i++)
        {
            //if we lost signal again, send signal lost event
            if (lock_sts_chg2[i] && pCtxt->ti960_sensors[i].lock_status == SIGNAL_LOCKED)
            {
                struct camera_input_status_t sensor_status;
                sensor_status.src = i;
                sensor_status.event = INPUT_SIGNAL_LOST_EVENT;
                pCtxt->sensor_platform_fcn_tbl.input_status_cb(pCtxt->ctrl, &sensor_status);
            }

            //assign new lock status
            if (intr_srcs & (1 << i))
                pCtxt->ti960_sensors[i].lock_status = lock_sts[i];

            //if status ever changed and we are now locked, send signal valid
            if ((lock_sts_chg[i] || lock_sts_chg2[i]) && (pCtxt->ti960_sensors[i].lock_status == SIGNAL_LOCKED))
            {
                struct camera_input_status_t sensor_status;
                sensor_status.src = i;
                sensor_status.event = INPUT_SIGNAL_VALID_EVENT;
                pCtxt->sensor_platform_fcn_tbl.input_status_cb(pCtxt->ctrl, &sensor_status);
            }
        }

        rc = ti960_read_int_sts(pCtxt, &intr_srcs);
        if (rc < 0)
        {
            AIS_LOG_TI_LIB_ERR("Failed to read INTS status: %d", rc);
            return;
        }
    }

    pCtxt->intr_in_process = FALSE;
}

static int slave_reset_indirect_reg(ti960_context_t* pCtxt, uint32 src_id)
{
    int rc = 0;
#if defined SENSOR_OV10640
    struct camera_i2c_reg_array dummy_reg[2];
    /*Sensor with indirect registers need to be configured*/
    pCtxt->slave_reg_setting.reg_array = dummy_reg;
    pCtxt->slave_reg_setting.size = 2;
    pCtxt->slave_reg_setting.reg_array[0].delay = 0;
    pCtxt->slave_reg_setting.reg_array[0].reg_data = 0;
    pCtxt->slave_reg_setting.reg_array[0].reg_addr = OV_INDIRECT_HI;
    pCtxt->slave_reg_setting.reg_array[1].delay = 0;
    pCtxt->slave_reg_setting.reg_array[1].reg_data = 0;
    pCtxt->slave_reg_setting.reg_array[1].reg_addr = OV_INDIRECT_LO;
    AIS_LOG_TI_LIB_DBG("resetting indirect registers for slave %x", pCtxt->slave_reg_setting.size, pCtxt->ti960_sensors[src_id].sensor_alias);

    if (pCtxt->slave_reg_setting.size)
    {
        rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                pCtxt->ctrl,
                pCtxt->ti960_sensors[src_id].sensor_alias,
                &pCtxt->slave_reg_setting);
        if (rc)
        {
            AIS_LOG_TI_LIB_ERR("sensor 0x%x failed to set", pCtxt->ti960_sensors[src_id].sensor_alias);
        }
    }
#else
    (void)pCtxt;
    (void)src_id;
#endif
    return rc;
}

static int ti960_sensor_detect_device_channels(void* ctxt)
{
    int rc = 0;
    unsigned int i = 0;

    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;

    AIS_LOG_TI_LIB_DBG("initialize ti960 0x%x with %d sensors",
        pCtxt->slave_addr,
        pCtxt->num_supported_sensors);

    if (pCtxt->state >= TI960_STATE_INITIALIZED)
    {
        AIS_LOG_TI_LIB_ERR("already detected %d out of %d",
            pCtxt->num_connected_sensors, pCtxt->num_supported_sensors);
        return 0;
    }
    else if (pCtxt->state != TI960_STATE_DETECTED)
    {
        AIS_LOG_TI_LIB_ERR("ti960 0x%x not detected - wrong state", pCtxt->slave_addr);
        return -1;
    }

    pCtxt->enable_intr = FALSE;
    rc = pCtxt->sensor_platform_fcn_tbl.setup_gpio_interrupt(pCtxt->ctrl,
            CAMERA_GPIO_INTR, ti960_intr_handler, pCtxt);

    rc = ti960_set_init_sequence(pCtxt);
    if (!rc)
    {
        struct camera_i2c_reg_array dummy_reg[1];

        pCtxt->num_connected_sensors = 0;
        for (i = 0; i < pCtxt->num_supported_sensors; i++)
        {
            if (pCtxt->ti960_sensors[i].slave_state != SLAVE_STATE_SERIALIZER_DETECTED)
                continue;

            /*initialize remote serializer*/
#if INITIALIZE_SERIALIZER
            if ((rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                    ctrl,
                    pCtxt->ti960_sensors[i].serializer_alias,
                    &ti960_serializer_setting))) {
                AIS_LOG_TI_LIB_ERR("Failed to init serializer(0x%x)", pCtxt->ti960_sensors[i].serializer_alias);
                continue;
            }
#endif

            /*reset indirect registers if necessary*/
            if ((rc = slave_reset_indirect_reg(pCtxt, i))) {
                AIS_LOG_TI_LIB_ERR("OV sensor(0x%x) unable to reset indirect registers ID", pCtxt->ti960_sensors[i].sensor_alias);
                continue;
            }

            /*read chip id*/
            pCtxt->slave_reg_setting.delay = 0;
            pCtxt->slave_reg_setting.reg_array = dummy_reg;
            pCtxt->slave_reg_setting.size = STD_ARRAY_SIZE(dummy_reg);

            pCtxt->slave_reg_setting.reg_array[0].delay = 0;
            pCtxt->slave_reg_setting.reg_array[0].reg_data = 0;
            pCtxt->slave_reg_setting.reg_array[0].reg_addr = SLAVE_IDENT_PID_REG;
            if ((rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_read(
                    pCtxt->ctrl,
                    pCtxt->ti960_sensors[i].sensor_alias,
                    &pCtxt->slave_reg_setting))) {
                AIS_LOG_TI_LIB_ERR("OV sensor(0x%x) unable to read ID", pCtxt->ti960_sensors[i].sensor_alias);
                continue;
            }

            if (SLAVE_IDENT_PID_ID != pCtxt->slave_reg_setting.reg_array[0].reg_data)
            {
                AIS_LOG_TI_LIB_ERR("OV sensor(0x%x) read back incorrect ID 0x%x. Expecting 0x%x.",
                        pCtxt->ti960_sensors[i].sensor_alias,
                        pCtxt->slave_reg_setting.reg_array[0].reg_data,
                        SLAVE_IDENT_PID_ID);
                continue;
            }

            pCtxt->ti960_sensors[i].slave_state = SLAVE_STATE_DETECTED;
            pCtxt->rxport_en &= pCtxt->ti960_sensors[i].rxport_en;
            pCtxt->sensor_lib.src_id_enable_mask |= (1 << i);
            pCtxt->num_connected_sensors++;
            AIS_LOG_TI_LIB_ALWAYS("detected camera ID 0x%x on bridge chip 0x%x",i,pCtxt->slave_addr);

            /*read initial status of the sensor*/
            rc = ti960_read_status(pCtxt, i);
            if (rc)
            {
                AIS_LOG_TI_LIB_ERR("sensor 0x%x could not read initial status", pCtxt->ti960_sensors[i].sensor_alias);
            }
        }

        pCtxt->state = TI960_STATE_INITIALIZED;

        /**setup interrupts**/
        pCtxt->enable_intr = TRUE;

        pCtxt->ti960_reg_setting.reg_array = ti960_init_intr_reg_array;
        pCtxt->ti960_reg_setting.size = STD_ARRAY_SIZE(ti960_init_intr_reg_array);
        rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                pCtxt->ctrl,
                pCtxt->slave_addr,
                &pCtxt->ti960_reg_setting);
        if (rc)
        {
            AIS_LOG_TI_LIB_ERR("ti960 0x%x failed to init interrupt", pCtxt->slave_addr);
        }
        /********************/

        CameraLogEvent(CAMERA_SENSOR_EVENT_PROBED, 0, 0);

        AIS_LOG_TI_LIB_WARN("ti960 detected %d out of %d",
               pCtxt->num_connected_sensors, pCtxt->num_supported_sensors);
    }

    return rc;
}

static int ti960_sensor_init_setting(void* ctxt)
{
    (void)ctxt;

    int rc = 0;
    int err = 0;
    unsigned int i = 0;
    struct camera_i2c_reg_array set_group_alias[2];
    unsigned short set_group_size = 0;
    struct camera_i2c_reg_array set_indiv_alias[8];
    unsigned short set_indiv_size = 0;
    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;
    unsigned int src_mask = 0;

    for (i = 0; i < pCtxt->num_supported_sensors; i++)
    {
        if (SLAVE_STATE_DETECTED == pCtxt->ti960_sensors[i].slave_state)
        {
            src_mask |= (1 << i);
            ADD_I2C_REG_ARRAY(set_indiv_alias, set_indiv_size, 0x4c, TI960_PORT_SEL(pCtxt->ti960_sensors[i].rxport), _reg_delay_);
            ADD_I2C_REG_ARRAY(set_indiv_alias, set_indiv_size, 0x65, pCtxt->ti960_sensors[i].sensor_alias, _reg_delay_);
        }
    }

    CameraLogEvent(CAMERA_SENSOR_EVENT_INITIALIZE_START, 0, 0);

    if (!src_mask)
    {
        AIS_LOG_TI_LIB_DBG("skipping as no sensors detected");
        return 0;
    }

    ADD_I2C_REG_ARRAY(set_group_alias, set_group_size, 0x4c, src_mask & 0x0f, _reg_delay_);
    ADD_I2C_REG_ARRAY(set_group_alias, set_group_size, 0x65, pCtxt->slave_alias_group, _reg_delay_);

    CameraLockMutex(pCtxt->mutex);

    /*set group slave alias*/
    pCtxt->ti960_reg_setting.reg_array = set_group_alias;
    pCtxt->ti960_reg_setting.size = set_group_size;
    rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pCtxt->slave_addr,
            &pCtxt->ti960_reg_setting);
    if (rc)
    {
        err = rc;
        AIS_LOG_TI_LIB_ERR("ti960 0x%x failed to set group alias", pCtxt->slave_addr);
        goto RESET_ALIAS;
    }

    /*init all slaves*/
    pCtxt->slave_reg_setting.reg_array = slave_init_reg_array;
    pCtxt->slave_reg_setting.size = STD_ARRAY_SIZE(slave_init_reg_array);
    AIS_LOG_TI_LIB_DBG("initializing %d registers for slave %x", pCtxt->slave_reg_setting.size, pCtxt->slave_alias_group);

    if (pCtxt->slave_reg_setting.size)
    {
        ais_log_kpi(AIS_EVENT_KPI_SENSOR_PROG_START);
        rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                pCtxt->ctrl,
                pCtxt->slave_alias_group,
                &pCtxt->slave_reg_setting);
        if (rc)
        {
            err = rc;
            AIS_LOG_TI_LIB_ERR("sensor 0x%x failed to init", pCtxt->slave_alias_group);
            goto RESET_ALIAS;
        }
        ais_log_kpi(AIS_EVENT_KPI_SENSOR_PROG_END);
    }

    for (i = 0; i < pCtxt->num_supported_sensors; i++)
    {
        if ((1 << i) & src_mask)
        {
            pCtxt->ti960_sensors[i].slave_state = SLAVE_STATE_INITIALIZED;
        }
    }

RESET_ALIAS:
    pCtxt->ti960_reg_setting.reg_array = set_indiv_alias;
    pCtxt->ti960_reg_setting.size = set_indiv_size;
    rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pCtxt->slave_addr,
            &pCtxt->ti960_reg_setting);
    if (rc)
    {
        err = rc;
        AIS_LOG_TI_LIB_ERR("ti960 0x%x failed to set indiv alias", pCtxt->slave_addr);
    }

    CameraUnlockMutex(pCtxt->mutex);

    /* TODO: cleanup initialized slaves in case of error */
    CameraLogEvent(CAMERA_SENSOR_EVENT_INITIALIZE_DONE, 0, 0);

    return err;
}

static int ti960_sensor_set_channel_mode(void* ctxt, unsigned int src_id_mask, unsigned int mode)
{
    (void)ctxt;
    (void)mode;
    (void)src_id_mask;

    return 0;
}

static int ti960_sensor_start_stream(void* ctxt, unsigned int src_id_mask)
{
    int rc = 0;
    unsigned int i = 0;
    unsigned int started_mask = 0;

    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;

    pCtxt->ti960_reg_setting.reg_array = ti960_start_reg_array;
    pCtxt->ti960_reg_setting.size = STD_ARRAY_SIZE(ti960_start_reg_array);

    pCtxt->slave_reg_setting.reg_array = slave_start_reg_array;
    pCtxt->slave_reg_setting.size = STD_ARRAY_SIZE(slave_start_reg_array);

    /*start slaves first*/
    for (i = 0; i < pCtxt->num_supported_sensors; i++)
    {
        if ((1 << i) & src_id_mask)
        {
            ti960_slave_devices_info_t* ti960_slave = &pCtxt->ti960_sensors[i];

            if (SLAVE_STATE_INITIALIZED == ti960_slave->slave_state)
            {
                int num_tries = 0;

                AIS_LOG_TI_LIB_DBG("starting slave %x [%d]", ti960_slave->sensor_alias,
                        ti960_slave->lock_status);
                if (pCtxt->slave_reg_setting.size)
                {
                    rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                            pCtxt->ctrl,
                            ti960_slave->sensor_alias,
                            &pCtxt->slave_reg_setting);
                    if (rc)
                    {
                        AIS_LOG_TI_LIB_ERR("sensor 0x%x failed to start", ti960_slave->sensor_alias);
                        break;
                    }
                }

                /*workaround for v2 until we fully deprecate it*/
                if (TI960_REV_ID_2 == pCtxt->revision)
                {
                    AIS_LOG_TI_LIB_ERR("***WARNING***");
                    AIS_LOG_TI_LIB_ERR("* ti960 rev2 does not support PASS pin. Start behavior may lead to errors!\n Please Upgrade HW!");
                    AIS_LOG_TI_LIB_ERR("*************");
                    CameraSleep(50);
                }
                else
                {
                    while (ti960_slave->lock_status != SIGNAL_LOCKED)
                    {
                        if (TI960_SIGNAL_LOCK_NUM_TRIES == num_tries)
                        {
                            /* TODO: in case of error, cleanup other sensors that were started */
                            AIS_LOG_TI_LIB_ERR("ti960 0x%x not locked timeout", ti960_slave->sensor_alias);
                            return -1;
                        }
                        AIS_LOG_TI_LIB_ERR("ti960 0x%x not locked try %d", ti960_slave->sensor_alias, num_tries);

                        CameraSleep(TI960_SIGNAL_LOCK_WAIT);
                        num_tries++;
                    }
                }

                ti960_slave->slave_state = SLAVE_STATE_STREAMING;
                started_mask |= (1 << i);
            }
            else
            {
                /*TODO: change this to AIS_LOG_TI_LIB_ERR once we limit which slaves to start*/
                AIS_LOG_TI_LIB_DBG("sensor 0x%x not ready to start (state=%d) - bad state",
                    ti960_slave->sensor_alias, ti960_slave->slave_state);
          }
        }
    }

    /* TODO: in case of error, cleanup other sensors that were started */

    /* then start ti960 transmitter if not already started */
    if (!rc &&
        TI960_STATE_INITIALIZED == pCtxt->state &&
        started_mask)
    {
        AIS_LOG_TI_LIB_DBG("starting ti960 0x%x transmitter (%x)", pCtxt->slave_addr, started_mask);

        rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                pCtxt->ctrl,
                pCtxt->slave_addr,
                &pCtxt->ti960_reg_setting);
        if (rc)
        {
            AIS_LOG_TI_LIB_ERR("ti960 0x%x failed to start", pCtxt->slave_addr);
        }

        pCtxt->streaming_src_mask |= started_mask;
        pCtxt->state = TI960_STATE_STREAMING;
    }

    CameraLogEvent(CAMERA_SENSOR_EVENT_STREAM_START, 0, 0);

    return rc;
}

static int ti960_sensor_stop_stream(void* ctxt, unsigned int src_id_mask)
{
    int rc = 0;
    unsigned int i = 0;

    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;

    pCtxt->ti960_reg_setting.reg_array = ti960_stop_reg_array;
    pCtxt->ti960_reg_setting.size = STD_ARRAY_SIZE(ti960_stop_reg_array);

    pCtxt->slave_reg_setting.reg_array = slave_stop_reg_array;
    pCtxt->slave_reg_setting.size = STD_ARRAY_SIZE(slave_stop_reg_array);

    /*stop transmitter first if no more clients*/
    if (!rc && TI960_STATE_STREAMING == pCtxt->state)
    {
        pCtxt->streaming_src_mask &= (~src_id_mask);
        AIS_LOG_TI_LIB_DBG("stopping ti960 0x%x transmitter (%x)", pCtxt->slave_addr, pCtxt->streaming_src_mask);

        /*stop if no slaves streaming*/
        if (!pCtxt->streaming_src_mask)
        {
            rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                        pCtxt->ctrl,
                        pCtxt->slave_addr,
                        &pCtxt->ti960_reg_setting);
            if (rc)
            {
                AIS_LOG_TI_LIB_ERR("ti960 0x%x failed to stop", pCtxt->slave_addr);
            }

            pCtxt->state = TI960_STATE_INITIALIZED;
        }
    }

    /*then stop slaves*/
    for (i = 0; i < pCtxt->num_supported_sensors; i++)
    {
        if ((1 << i) & src_id_mask)
        {
            AIS_LOG_TI_LIB_DBG("stopping slave %x", pCtxt->ti960_sensors[i].sensor_alias);
            if (SLAVE_STATE_STREAMING == pCtxt->ti960_sensors[i].slave_state)
            {
                if (pCtxt->slave_reg_setting.size)
                {
                    rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
                            pCtxt->ctrl,
                            pCtxt->ti960_sensors[i].sensor_alias,
                            &pCtxt->slave_reg_setting);
                    if (rc)
                    {
                        AIS_LOG_TI_LIB_ERR("sensor 0x%x failed to stop", pCtxt->ti960_sensors[i].sensor_alias);
                        break;
                    }
                }
                pCtxt->ti960_sensors[i].slave_state = SLAVE_STATE_INITIALIZED;
            }
            else
            {
                /*TODO: change this to AIS_LOG_TI_LIB_ERR once we limit which slaves to stop*/
                AIS_LOG_TI_LIB_DBG("sensor 0x%x not in state to stop (state=%d) - bad state",
                        pCtxt->ti960_sensors[i].sensor_alias, pCtxt->ti960_sensors[i].slave_state);
            }
        }
    }

    /* TODO: cleanup in case of failure */

    /*workaround for v2 until we fully deprecate it*/
    if (TI960_REV_ID_2 == pCtxt->revision)
    {
        AIS_LOG_TI_LIB_ERR("Running Workaround for ti960 rev 2 stop/start");
        pCtxt->state = TI960_STATE_DETECTED;
        return ti960_sensor_detect_device_channels(pCtxt);
    }

    CameraLogEvent(CAMERA_SENSOR_EVENT_STREAM_STOP, 0, 0);

    return rc;
}

static int ti960_sensor_set_platform_func_table(void* ctxt, sensor_platform_func_table_t* table)
{
    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;

    if (!pCtxt->sensor_i2c_tbl_initialized)
    {
        if (!table)
        {
            AIS_LOG_TI_LIB_ERR("Invalid i2c func table param");
            return -1;
        }
        pCtxt->sensor_platform_fcn_tbl = *table;
        pCtxt->sensor_i2c_tbl_initialized = 1;
        AIS_LOG_TI_LIB_DBG("i2c func table set");
    }

    return 0;
}

static int ti960_calculate_exposure(void* ctxt, unsigned int src_id, sensor_exposure_info_t* exposure_info)
{
    int rc = 0;

    (void)ctxt;
    (void)src_id;

    rc = slave_calculate_exposure(exposure_info);

    return rc;
}


static int ti960_sensor_exposure_config(void* ctxt, unsigned int src_id, sensor_exposure_info_t* exposure_info)
{
    int rc = 0;

    ti960_context_t* pCtxt = (ti960_context_t*)ctxt;

    if (src_id >= NUM_SUPPORT_SENSORS)
    {
       AIS_LOG_TI_LIB_ERR("ti960_sensor_exposure_config Invalid src_id = %d", src_id);
       return -1;
    }

    if (!((pCtxt->ti960_sensors[src_id].slave_state == SLAVE_STATE_INITIALIZED) ||
       (pCtxt->ti960_sensors[src_id].slave_state == SLAVE_STATE_STREAMING)))
    {
       AIS_LOG_TI_LIB_ERR("ti960_sensor_exposure_config Sensor src_id = %d invalid state = %d",
                src_id,pCtxt->ti960_sensors[src_id].slave_state);
       return -1;
    }

    struct camera_i2c_reg_setting slave_reg;
    memset(&slave_reg, 0x0, sizeof(slave_reg));

    //get setting from slaves
    rc = slave_get_exposure_config(&slave_reg, exposure_info);

    //apply setting
    if (!rc && slave_reg.size)
    {
        CameraLockMutex(pCtxt->mutex);
        rc = pCtxt->sensor_platform_fcn_tbl.i2c_slave_write_array(
               pCtxt->ctrl,
               pCtxt->ti960_sensors[src_id].sensor_alias,
               &slave_reg);
        CameraUnlockMutex(pCtxt->mutex);
        if (rc)
        {
           AIS_LOG_TI_LIB_ERR("sensor 0x%x failed to write exposure", pCtxt->ti960_sensors[src_id].sensor_alias);
           return rc;
        }
    }

    return 0;
}

static int ti960_sensor_s_param(void* ctxt, qcarcam_param_t param, unsigned int src_id, void* p_param)
{
    int rc = 0;

    if (ctxt == NULL)
    {
        AIS_LOG_TI_LIB_ERR("ti960_sensor_s_param Invalid ctxt");
        return -1;
    }

    if (p_param == NULL)
    {
        AIS_LOG_TI_LIB_ERR("ti960_sensor_s_param Invalid params");
        return -1;
    }

    switch(param)
    {
        case QCARCAM_PARAM_EXPOSURE:
            rc = ti960_sensor_exposure_config(ctxt, src_id, (sensor_exposure_info_t*)p_param);
            break;
        default:
            AIS_LOG_TI_LIB_ERR("ti960_sensor_s_param. Param not supported = %d", param);
            rc = -1;
            break;
    }
    return rc;
}

/**
 * FUNCTION: CameraSensorDevice_Open_ti960
 *
 * DESCRIPTION: Entry function for device driver framework
 **/
CAM_API CameraResult CameraSensorDevice_Open_ti960(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId)
{
    sensor_lib_interface_t sensor_lib_interface = {
            .sensor_open_lib = ti960_sensor_open_lib,
    };

    return CameraSensorDevice_Open(ppNewHandle, deviceId, &sensor_lib_interface);
}
