/**
 * @file max9296b_lib.c
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>


#include "CameraSensorDeviceInterface.h"
#include "max9296b_lib.h"
#include "SensorDebug.h"

#ifdef MAX9296_ENABLE_CONFIG_PARSER
/* INI file parser header file                                                */
#include "inifile_parser.h"

/* Input INI file to be parsed                                                */
#define FILE_NAME    "/data/misc/camera/mizar_config.ini"

#endif /* MAX9296_ENABLE_CONFIG_PARSER */

/* Turn this on to initialize remote serializer
 *   - sets higher throughput through BCC
 */
//#define INITIALIZE_SERIALIZER 0

typedef enum
{
    SLAVE_STATE_INVALID = 0,
    SLAVE_STATE_SERIALIZER_DETECTED,
    SLAVE_STATE_DETECTED,
    SLAVE_STATE_INITIALIZED,
    SLAVE_STATE_STREAMING,
}max9296_slave_state_t;

/*Structure that holds information regarding slave devices*/
typedef struct
{
    max9296_slave_state_t slave_state;
    unsigned int sensor_alias;
    unsigned int serializer;
    unsigned int serializer_alias;
}max9296_slave_devices_info_t;

#define MAX_INIT_SEQUENCE_SIZE 256

typedef enum
{
    MAX9296_STATE_INVALID = 0,
    MAX9296_STATE_DETECTED,
    MAX9296_STATE_INITIALIZED,
    MAX9296_STATE_SUSPEND,
    MAX9296_STATE_STREAMING,
}max9296_state_t;


typedef enum
{
    MAX9296_MODE_LINK_A = 0,
    MAX9296_MODE_LINK_B,
    MAX9296_MODE_SPLITTER,
}max9296_link_info_t;

/* configuration structure which holds the config params, will be updated     *
 * upon parsing the user provided ".ini" file                                 */
typedef struct
{
    // topology section params
    int mizar_config_receiver_enabled;
    int num_of_cameras;
} max9296_topology_config_t;

typedef struct
{
  /*must be first element*/
  sensor_lib_t sensor_lib;

  CameraMutex mutex;

  max9296_state_t state;
  unsigned int streaming_src_mask;

  unsigned int revision;
  max9296_link_info_t link_mode;
  unsigned int slave_addr;
  unsigned int num_supported_sensors;
  unsigned int num_connected_sensors;
  unsigned int sensor_i2c_tbl_initialized;
  sensor_platform_func_table_t sensor_platform_fcn_tbl;
  struct camera_i2c_reg_setting init_reg_settings;
  unsigned int rxport_en;

  struct camera_i2c_reg_setting max9296_reg_setting;
  struct camera_i2c_reg_setting camera_sensor_reg_setting;
  struct camera_i2c_reg_array dummy_reg[1];
  struct camera_i2c_reg_array init_seq[MAX_INIT_SEQUENCE_SIZE];
  max9296_slave_devices_info_t max9296_sensors[NUM_SUPPORT_SENSORS];
  max9296_topology_config_t max9296_config;

  void* ctrl;
}max9296_context_t;

static int max9296_sensor_close_lib(void* ctxt);
static int max9296_sensor_power_suspend(void* ctxt);
static int max9296_sensor_power_resume(void* ctxt);
static int max9296_sensor_detect_device(void* ctxt);
static int max9296_sensor_detect_device_channels(void* ctxt);
static int max9296_sensor_init_setting(void* ctxt);
static int max9296_sensor_set_channel_mode(void* ctxt, unsigned int src_id_mask, unsigned int mode);
static int max9296_sensor_start_stream(void* ctxt, unsigned int src_id_mask);
static int max9296_sensor_stop_stream(void* ctxt, unsigned int src_id_mask);
static int max9296_sensor_set_platform_func_table(void* ctxt, sensor_platform_func_table_t* table);

static sensor_lib_t sensor_lib_ptr =
{
  .sensor_slave_info =
  {
      .sensor_name = SENSOR_MODEL,
      .slave_addr = MSM_DESER_0_SLAVEADDR,
      .i2c_freq_mode = SENSOR_I2C_MODE_CUSTOM,
      .addr_type = CAMERA_I2C_WORD_ADDR,
      .data_type = CAMERA_I2C_BYTE_DATA,
      .sensor_id_info =
      {
        .sensor_id_reg_addr = 0x00,
        .sensor_id = MSM_DESER_0_SLAVEADDR,
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
            .seq_type = CAMERA_POW_SEQ_GPIO,
            .seq_val = CAMERA_GPIO_RESET,
            .config_val = GPIO_OUT_LOW,
            .delay = 10,
          },
          {
            .seq_type = CAMERA_POW_SEQ_GPIO,
            .seq_val = CAMERA_GPIO_RESET,
            .config_val = GPIO_OUT_HIGH,
            .delay = 2000, /*@todo: change once HW is fixed*/
          },
        },
        .size_up = 3,
        .power_down_setting_a =
        {
          {
            .seq_type = CAMERA_POW_SEQ_VREG,
            .seq_val = CAMERA_VDIG,
            .config_val = 0,
            .delay = 0,
          },
        },
        .size_down = 1,
      },
      .is_init_params_valid = 1,
  },
  .num_channels = NUM_SUPPORT_SENSORS,
  .channels =
  {
    {
      .output_mode =
      {
        .fmt = FMT_9296_LINK_A,
        .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
        .channel_info = {.vc = 0, .dt = DT_9296_LINK_A, .cid = CID_VC0,},
      },
      .num_subchannels = 1,
      .subchan_layout =
      {
        {
          .src_id = 0,
          .x_offset = 0,
          .y_offset = 0,
          .stride = 1920,
        },
      },
    },
    {
      .output_mode =
      {
        .fmt = FMT_9296_LINK_B,
        .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
        .channel_info = {.vc = 1, .dt = DT_9296_LINK_B, .cid = CID_VC1,},
      },
      .num_subchannels = 1,
      .subchan_layout =
      {
        {
          .src_id = 1,
          .x_offset = 0,
          .y_offset = 0,
          .stride = 1920,
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
          .fmt = FMT_9296_LINK_A,
          .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
          .channel_info = {.vc = 0, .dt = DT_9296_LINK_A, .cid = CID_VC0,},
        },
      },
      .num_modes = 1,
    },
    {
      .src_id = 1,
      .modes =
      {
        {
          .fmt = FMT_9296_LINK_B,
          .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
          .channel_info = {.vc = 1, .dt = DT_9296_LINK_B, .cid = CID_VC1,},
        },
      },
      .num_modes = 1,
    },
  },
  .sensor_output =
  {
    .output_format = SENSOR_YCBCR,
    .connection_mode = SENSOR_MIPI_CSI,
    .raw_output = SENSOR_10_BIT_DIRECT,
    .filter_arrangement = SENSOR_UYVY,
  },
  .csi_params =
  {
    .lane_cnt = 2,
    .settle_cnt = 0xE,
    .lane_mask = 0x1F,
    .combo_mode = 0,
    .is_csi_3phase = 0,
  },
  .sensor_close_lib = max9296_sensor_close_lib,
  .sensor_custom_func =
  {
    .sensor_set_platform_func_table = &max9296_sensor_set_platform_func_table,
    .sensor_power_suspend = max9296_sensor_power_suspend,
    .sensor_power_resume = max9296_sensor_power_resume,
    .sensor_detect_device = &max9296_sensor_detect_device,
    .sensor_detect_device_channels = &max9296_sensor_detect_device_channels,
    .sensor_init_setting = &max9296_sensor_init_setting,
    .sensor_set_channel_mode = &max9296_sensor_set_channel_mode,
    .sensor_start_stream = &max9296_sensor_start_stream,
    .sensor_stop_stream = &max9296_sensor_stop_stream,
  },
  .use_sensor_custom_func = TRUE,
};

static max9296_slave_devices_info_t max9296_sensors_init_table[] =
{
  {
      .slave_state = SLAVE_STATE_INVALID,
      .sensor_alias = MSM_DESER_0_ALIAS_ADDR_CAM_SNSR_0,
      .serializer_alias = MSM_DESER_0_ALIAS_ADDR_CAM_SER_0,
  },
  {
      .slave_state = SLAVE_STATE_INVALID,
      .sensor_alias = MSM_DESER_0_ALIAS_ADDR_CAM_SNSR_1,
      .serializer_alias = MSM_DESER_0_ALIAS_ADDR_CAM_SER_1,
  },
};

static struct camera_i2c_reg_array max9296_deserializer_split_regs_receiver[] = INIT_MSM_DESERIALIZER_SPLITTER_MODE_RECEIVER;
static struct camera_i2c_reg_setting max9296_deserializer_split_setting_receiver =
{
    .reg_setting = max9296_deserializer_split_regs_receiver,
    .size = STD_ARRAY_SIZE(max9296_deserializer_split_regs_receiver),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_deserializer_split_regs_sender[] = INIT_MSM_DESERIALIZER_SPLITTER_MODE_SENDER;
static struct camera_i2c_reg_setting max9296_deserializer_split_setting_sender =
{
    .reg_setting = max9296_deserializer_split_regs_sender,
    .size = STD_ARRAY_SIZE(max9296_deserializer_split_regs_sender),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_deserializer_8bit_a_regs[] = INIT_MSM_DESERIALIZER_8BIT_A;
static struct camera_i2c_reg_setting max9296_deserializer_8bit_a_setting =
{
    .reg_setting = max9296_deserializer_8bit_a_regs,
    .size = STD_ARRAY_SIZE(max9296_deserializer_8bit_a_regs),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_deserializer_8bit_b_regs[] = INIT_MSM_DESERIALIZER_8BIT_B;
static struct camera_i2c_reg_setting max9296_deserializer_8bit_b_setting =
{
    .reg_setting = max9296_deserializer_8bit_b_regs,
    .size = STD_ARRAY_SIZE(max9296_deserializer_8bit_b_regs),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_deserializer_10bit_a_regs[] = INIT_MSM_DESERIALIZER_10BIT_A;
static struct camera_i2c_reg_setting max9296_deserializer_10bit_a_setting =
{
    .reg_setting = max9296_deserializer_10bit_a_regs,
    .size = STD_ARRAY_SIZE(max9296_deserializer_10bit_a_regs),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_deserializer_10bit_b_regs[] = INIT_MSM_DESERIALIZER_10BIT_B;
static struct camera_i2c_reg_setting max9296_deserializer_10bit_b_setting =
{
    .reg_setting = max9296_deserializer_10bit_b_regs,
    .size = STD_ARRAY_SIZE(max9296_deserializer_10bit_b_regs),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_deserializer_link_regs_receiver[] = INIT_MSM_DESERIALIZER_LINK_MODE_RECEIVER;
static struct camera_i2c_reg_setting max9296_deserializer_link_setting_receiver =
{
    .reg_setting = max9296_deserializer_link_regs_receiver,
    .size = STD_ARRAY_SIZE(max9296_deserializer_link_regs_receiver),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};


static struct camera_i2c_reg_array max9296_start_split_reg_array[] = START_MSM_DESERIALIZER_SPLIT;
static struct camera_i2c_reg_setting max9296_start_split_setting =
{
    .reg_setting = max9296_start_split_reg_array,
    .size = STD_ARRAY_SIZE(max9296_start_split_reg_array),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_start_link_a_reg_array[] = START_MSM_DESERIALIZER_LINK_A;
static struct camera_i2c_reg_setting max9296_start_link_a_setting =
{
    .reg_setting = max9296_start_link_a_reg_array,
    .size = STD_ARRAY_SIZE(max9296_start_link_a_reg_array),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_start_link_b_reg_array[] = START_MSM_DESERIALIZER_LINK_B;
static struct camera_i2c_reg_setting max9296_start_link_b_setting =
{
    .reg_setting = max9296_start_link_b_reg_array,
    .size = STD_ARRAY_SIZE(max9296_start_link_b_reg_array),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_stop_reg_array[] = STOP_MSM_DESERIALIZER;
static struct camera_i2c_reg_setting max9296_stop_setting =
{
    .reg_setting = max9296_stop_reg_array,
    .size = STD_ARRAY_SIZE(max9296_stop_reg_array),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9295_stop_reg_array[] = STOP_CAM_SERIALIZER;
static struct camera_i2c_reg_setting max9295_stop_setting =
{
    .reg_setting = max9295_stop_reg_array,
    .size = STD_ARRAY_SIZE(max9295_stop_reg_array),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array cam_serializer_init_8bit_regs_0[] = INIT_CAM_SERIALIZER_8BIT_0;
static struct camera_i2c_reg_setting cam_serializer_init_8bit_setting_0 =
{
    .reg_setting = cam_serializer_init_8bit_regs_0,
    .size = STD_ARRAY_SIZE(cam_serializer_init_8bit_regs_0),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array cam_serializer_init_8bit_regs_1[] = INIT_CAM_SERIALIZER_8BIT_1;
static struct camera_i2c_reg_setting cam_serializer_init_8bit_setting_1 =
{
    .reg_setting = cam_serializer_init_8bit_regs_1,
    .size = STD_ARRAY_SIZE(cam_serializer_init_8bit_regs_1),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array cam_serializer_init_10bit_regs_0[] = INIT_CAM_SERIALIZER_10BIT_0;
static struct camera_i2c_reg_setting cam_serializer_init_10bit_setting_0 =
{
    .reg_setting = cam_serializer_init_10bit_regs_0,
    .size = STD_ARRAY_SIZE(cam_serializer_init_10bit_regs_0),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array cam_serializer_init_10bit_regs_1[] = INIT_CAM_SERIALIZER_10BIT_1;
static struct camera_i2c_reg_setting cam_serializer_init_10bit_setting_1 =
{
    .reg_setting = cam_serializer_init_10bit_regs_1,
    .size = STD_ARRAY_SIZE(cam_serializer_init_10bit_regs_1),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array cam_serializer_start_reg_array[] = START_CAM_SERIALIZER;
static struct camera_i2c_reg_setting cam_serializer_start_setting =
{
    .reg_setting = cam_serializer_start_reg_array,
    .size = STD_ARRAY_SIZE(cam_serializer_start_reg_array),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array max9296_reg_array_disable_reverse[] = MSM_DESERIALIZER_DISABLE_I2C_REVERSE;
static struct camera_i2c_reg_setting max9296_setting_disable_reverse =
{
    .reg_setting = max9296_reg_array_disable_reverse,
    .size = STD_ARRAY_SIZE(max9296_reg_array_disable_reverse),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

#ifdef MAX9296_ENABLE_MSM_SERIALIZER
static struct camera_i2c_reg_array mx9295_init_msm_reg_array_disable_forward[] = INIT_MSM_SERIALIZER_DISABLE_I2C_FORWARD;
static struct camera_i2c_reg_setting mx9295_init_msm_setting_disable_forward =
{
    .reg_setting = mx9295_init_msm_reg_array_disable_forward,
    .size = STD_ARRAY_SIZE(mx9295_init_msm_reg_array_disable_forward),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};



static struct camera_i2c_reg_array mx9295_init_msm_reg_array_dual[] = INIT_MSM_SERIALIZER_DUAL_BCAST;
static struct camera_i2c_reg_setting mx9295_init_msm_setting_dual =
{
    .reg_setting = mx9295_init_msm_reg_array_dual,
    .size = STD_ARRAY_SIZE(mx9295_init_msm_reg_array_dual),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array mx9295_init_msm_reg_array_single[] = INIT_MSM_SERIALIZER_SINGLE_BCAST;
static struct camera_i2c_reg_setting mx9295_init_msm_setting_single =
{
    .reg_setting = mx9295_init_msm_reg_array_single,
    .size = STD_ARRAY_SIZE(mx9295_init_msm_reg_array_single),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};
#endif


#define DUMMY_READ \
{ \
    { 0x0000, 0x00, _reg_delay_ }, \
}

static struct camera_i2c_reg_array read_regs[] = DUMMY_READ;
static struct camera_i2c_reg_setting read_setting =
{
    .reg_setting = read_regs,
    .size = STD_ARRAY_SIZE(read_regs),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

#ifdef MAX9296_ENABLE_CONFIG_PARSER
/**
*******************************************************************************
*  @fn            ini_parser_cb
*
*  @brief         call-back functionality, will be invoked from the inifile parser
*                 and updates the config structure members
*  @param
*
*  @return
*******************************************************************************
*/
static int ini_parser_cb(void* config, const char* section, const char* param_name,
                   const char* param_value)
{
    max9296_topology_config_t* pconfig = (max9296_topology_config_t*)config;

    const char* def_sec1 = "topology"; // exact section name and param names to be provided in .ini file

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(param_name, n) == 0
    if (MATCH(def_sec1, "mizar_config_receiver_enabled"))
    {
        pconfig->mizar_config_receiver_enabled = atoi(param_value);
    }
    else if (MATCH(def_sec1, "num_of_cameras"))
    {
        pconfig->num_of_cameras = atoi(param_value);
    }
    else
    {
        printf("Error: Unknown section/name\n");
        return 0;  /* unknown section/name, error */
    }

    printf("Identified section = %s, param_name = %s\n", section, param_name);
    return 1;
}
#endif

int max9296_read_ini_file(max9296_context_t * pCtxt)
{
    max9296_topology_config_t default_config =
    {
        .mizar_config_receiver_enabled = 0,
        .num_of_cameras = 2
    };

#ifdef MAX9296_ENABLE_CONFIG_PARSER
    FILE* fp;
    int error = 0;
    const char* filename = FILE_NAME;

    fp = fopen(filename, "r");
    if (!fp)
    {
        printf("Couldn't open %s\n", filename);
        error = -1;
        goto def_set;
    }

    error = inifile_parser(fp, ini_parser_cb, &pCtxt->max9296_config);
    if(error)
    {
        printf("Couldn't parse %s\n", filename);
        goto def_set;
    }

    printf("topology section params: %d, %d\n",
            pCtxt->max9296_config.mizar_config_receiver_enabled,
            pCtxt->max9296_config.num_of_cameras
            );

    /* checking valid ranges */
    if(pCtxt->max9296_config.mizar_config_receiver_enabled < 0 || pCtxt->max9296_config.mizar_config_receiver_enabled > 1)
    {
        printf("Unsupported value of mizar_config_receiver_enabled param value = %d\n", pCtxt->max9296_config.mizar_config_receiver_enabled);
        error = -1;
        goto def_set;
    }

    if(pCtxt->max9296_config.num_of_cameras != 1 &&
            pCtxt->max9296_config.num_of_cameras != 2 &&
            pCtxt->max9296_config.num_of_cameras != 4)
    {
        printf("Unsupported value of num_of_cameras param value = %d\n", pCtxt->max9296_config.num_of_cameras);
        error = -1;
        goto def_set;
    }

def_set:
    if(error)
    {
        memcpy(&pCtxt->max9296_config, &default_config, sizeof(max9296_topology_config_t));
        printf("using default topology params: %d, %d\n",
                pCtxt->max9296_config.mizar_config_receiver_enabled,
                pCtxt->max9296_config.num_of_cameras
                );
    }

    if(fp)
        fclose(fp);
#else
    pCtxt->max9296_config = default_config;
#endif

    return 0;
}

/**
 * FUNCTION: sensor_open_lib
 *
 * DESCRIPTION: Open sensor library and returns data pointer
 **/
static void* max9296_sensor_open_lib(void* ctrl, void* arg)
{
    unsigned int i=0;
    sensor_open_lib_t* device_info = (sensor_open_lib_t*)arg;

    max9296_context_t* pCtxt = calloc(1, sizeof(max9296_context_t));

    SERR("max9296b_sensor_open_lib()");

    max9296_read_ini_file(pCtxt);

    if (pCtxt)
    {
        memcpy(&pCtxt->sensor_lib, &sensor_lib_ptr, sizeof(pCtxt->sensor_lib));
        memcpy(&pCtxt->max9296_sensors, max9296_sensors_init_table, sizeof(pCtxt->max9296_sensors));

        pCtxt->ctrl = ctrl;
        pCtxt->sensor_lib.sensor_slave_info.camera_id = device_info->camera_id;
        pCtxt->state = MAX9296_STATE_INVALID;
        pCtxt->num_supported_sensors = NUM_SUPPORT_SENSORS;
        pCtxt->max9296_reg_setting.addr_type = CAMERA_I2C_WORD_ADDR;
        pCtxt->max9296_reg_setting.data_type = CAMERA_I2C_BYTE_DATA;

        for (i=0; i<pCtxt->num_supported_sensors; i++)
        {
            pCtxt->max9296_sensors[i].slave_state = SLAVE_STATE_INVALID;
        }

        /*default to dev id 0*/
        switch (device_info->subdev_id)
        {
        case 1:
        case 3:
            pCtxt->slave_addr = MSM_DESER_1_SLAVEADDR;
            pCtxt->max9296_sensors[0].serializer_alias = MSM_DESER_1_ALIAS_ADDR_CAM_SER_0;
            pCtxt->max9296_sensors[1].serializer_alias = MSM_DESER_1_ALIAS_ADDR_CAM_SER_1;
            pCtxt->max9296_sensors[0].sensor_alias = MSM_DESER_1_ALIAS_ADDR_CAM_SNSR_0;
            pCtxt->max9296_sensors[1].sensor_alias = MSM_DESER_1_ALIAS_ADDR_CAM_SNSR_1;
            break;
        default:
            pCtxt->slave_addr = MSM_DESER_0_SLAVEADDR;
            break;
        }
        pCtxt->sensor_lib.sensor_slave_info.sensor_id_info.sensor_id = pCtxt->slave_addr;
    }

    return pCtxt;
}


static int max9296_sensor_close_lib(void* ctxt)
{
    free(ctxt);
    return 0;
}

/**
 * FUNCTION: sensor_ext_open_lib
 *
 * DESCRIPTION: Open sensor library and returns data pointer
 **/
void *sensor_open_lib_ext(void)
{
    return NULL;
}

#define place_marker(_name_) CAM_MSG(HIGH, _name_)
#define ADD_I2C_REG_ARRAY(_a_, _size_, _addr_, _val_, _delay_) \
do { \
    _a_[_size_].reg_addr = _addr_; \
    _a_[_size_].reg_data = _val_; \
    _a_[_size_].delay = _delay_; \
    _size_++; \
} while(0)

/* Not sure about below line need to check with MAX9296 Datasheet
 * If it's required need to implement similar logic for MAX9296 port selection
 */
static int max9296_set_init_sequence(max9296_context_t* pCtxt)
{
    struct camera_i2c_reg_setting *p_deserialzer_mode_setting = NULL, *p_serialzer_init_setting = NULL;
    struct camera_i2c_reg_setting *p_deserialzer_a_dt_setting = NULL, *p_deserialzer_b_dt_setting = NULL;
    int ret = 0;

    SENSOR_WARN("max9296_set_init_sequence()");

    // Only program cameras if you are directly connected to them (broadacast mizar)
    if (!pCtxt->max9296_config.mizar_config_receiver_enabled)
    {
        if (pCtxt->link_mode == MAX9296_MODE_SPLITTER)
        {
            //Splitter mode detected (program for both cameras)
            struct camera_i2c_reg_array write_regs[] = DESERIALIZER_SPLITER_MODE;
            struct camera_i2c_reg_setting write_setting =
            {
                .reg_setting = write_regs,
                .size = STD_ARRAY_SIZE(write_regs),
                .addr_type = CAMERA_I2C_WORD_ADDR,
                .data_type = CAMERA_I2C_BYTE_DATA,
            };

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(pCtxt->ctrl, pCtxt->slave_addr, &write_setting)))
            {
                SERR("Unable to set deserailzer in Splitter Mode. Fatal error!", pCtxt->slave_addr);
                return -1;
            }
            SENSOR_WARN("Setting MAX9296 to Splitter Mode!");
        }
        else if(pCtxt->link_mode == MAX9296_MODE_LINK_A)
        {
            /* Select Link A*/
            struct camera_i2c_reg_array write_regs[] = DESERIALIZER_SELECT_LINK_A;
            struct camera_i2c_reg_setting write_setting =
            {
                .reg_setting = write_regs,
                .size = STD_ARRAY_SIZE(write_regs),
                .addr_type = CAMERA_I2C_WORD_ADDR,
                .data_type = CAMERA_I2C_BYTE_DATA,
            };

            ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(pCtxt->ctrl, pCtxt->slave_addr,
                                   &write_setting);
            if (ret)
            {
                SERR("Unable to select link A. Fatal error!", pCtxt->slave_addr);
                return -1;
            }
            SENSOR_WARN("Initliazed Camera connected to Link A");
        }
        else if(pCtxt->link_mode == MAX9296_MODE_LINK_B)
        {
            /* Select Link B*/
            struct camera_i2c_reg_array write_regs[] = DESERIALIZER_SELECT_LINK_B;
            struct camera_i2c_reg_setting write_setting =
            {
                .reg_setting = write_regs,
                .size = STD_ARRAY_SIZE(write_regs),
                .addr_type = CAMERA_I2C_WORD_ADDR,
                .data_type = CAMERA_I2C_BYTE_DATA,
            };

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(pCtxt->ctrl, pCtxt->slave_addr,
                       &write_setting)))
            {
                SERR("Unable to select link B. Fatal error!", pCtxt->slave_addr);
                return -1;
            }
            SENSOR_WARN("Initialized Camera connected to Link B");
        }

        //Split and link modes share same sender configuration
        p_deserialzer_mode_setting = &max9296_deserializer_split_setting_sender;
    }

    if (!pCtxt->max9296_config.mizar_config_receiver_enabled)
    {
        //Initialize 1st camera if detected
        if (SLAVE_STATE_DETECTED == pCtxt->max9296_sensors[0].slave_state)
        {
            switch (pCtxt->sensor_lib.subchannels[0].modes[0].channel_info.dt)
            {
                case CSI_RAW8:
                    p_serialzer_init_setting = &cam_serializer_init_8bit_setting_0;
                    p_deserialzer_a_dt_setting = &max9296_deserializer_8bit_a_setting;
                    break;
                case CSI_RAW10:
                    p_serialzer_init_setting = &cam_serializer_init_10bit_setting_0;
                    p_deserialzer_a_dt_setting = &max9296_deserializer_10bit_a_setting;
                    break;
                default:
                    p_serialzer_init_setting = &cam_serializer_init_8bit_setting_0;
                    p_deserialzer_a_dt_setting = &max9296_deserializer_8bit_a_setting;
                    SENSOR_WARN("slave 0x%x unknown DT: 0x%x\n", pCtxt->slave_addr,
                        pCtxt->sensor_lib.subchannels[0].modes[0].channel_info.dt);
                    break;
            }

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                       pCtxt->ctrl,
                       pCtxt->max9296_sensors[0].serializer_alias, p_serialzer_init_setting)))
            {
                SERR("Failed to init camera serializer(0x%x)", pCtxt->max9296_sensors[0].serializer_alias);
                return -1;
            }
            pCtxt->max9296_sensors[0].slave_state = SLAVE_STATE_INITIALIZED;
        }

        //Initialize 2nd camera if detected
        if (SLAVE_STATE_DETECTED == pCtxt->max9296_sensors[1].slave_state)
        {
            switch (pCtxt->sensor_lib.subchannels[1].modes[0].channel_info.dt)
            {
                case CSI_RAW8:
                    p_serialzer_init_setting = &cam_serializer_init_8bit_setting_1;
                    p_deserialzer_b_dt_setting = &max9296_deserializer_8bit_b_setting;
                    break;
                case CSI_RAW10:
                    p_serialzer_init_setting = &cam_serializer_init_10bit_setting_1;
                    p_deserialzer_b_dt_setting = &max9296_deserializer_10bit_b_setting;
                    break;
                default:
                    p_serialzer_init_setting = &cam_serializer_init_8bit_setting_1;
                    p_deserialzer_b_dt_setting = &max9296_deserializer_8bit_b_setting;
                    SENSOR_WARN("slave 0x%x unknown DT: 0x%x\n", pCtxt->slave_addr,
                        pCtxt->sensor_lib.subchannels[0].modes[0].channel_info.dt);
                    break;
            }

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                       pCtxt->ctrl,
                       pCtxt->max9296_sensors[1].serializer_alias, p_serialzer_init_setting)))
            {
                SERR("Failed to init camera serializer(0x%x)", pCtxt->max9296_sensors[1].serializer_alias);
                return -1;
            }
            pCtxt->max9296_sensors[1].slave_state = SLAVE_STATE_INITIALIZED;
        }
    }
    else //Receiver mode
    {
        //Set them to intialize as data is coming from another mizar and not actual cameras here
        if (2 == pCtxt->max9296_config.num_of_cameras)
        {
            p_deserialzer_mode_setting =&max9296_deserializer_split_setting_receiver;
            pCtxt->max9296_sensors[0].slave_state = SLAVE_STATE_INITIALIZED;
            pCtxt->max9296_sensors[1].slave_state = SLAVE_STATE_INITIALIZED;
        }
        else
        {
            p_deserialzer_mode_setting =&max9296_deserializer_link_setting_receiver;
            pCtxt->max9296_sensors[0].slave_state = SLAVE_STATE_INITIALIZED;
        }
    }

    // link b setting should be put before a, because 0x0316 value meight be overrided
    if (p_deserialzer_b_dt_setting)
    {
        if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
               pCtxt->ctrl, pCtxt->slave_addr, p_deserialzer_b_dt_setting)))
        {
            SERR("Failed to init de-serializer(0x%x) dt for link b", pCtxt->slave_addr);
        }
    }

    if (p_deserialzer_a_dt_setting)
    {
        if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
               pCtxt->ctrl, pCtxt->slave_addr, p_deserialzer_a_dt_setting)))
        {
            SERR("Failed to init de-serializer(0x%x) dt for link a", pCtxt->slave_addr);
        }
    }

    if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
               pCtxt->ctrl, pCtxt->slave_addr, p_deserialzer_mode_setting)))
    {
        SERR("Failed to init de-serializer(0x%x)", pCtxt->slave_addr);
    }

#ifdef MAX9296_ENABLE_MSM_SERIALIZER
    //Disable programming of serializer on the receiver board
    if (!pCtxt->max9296_config.mizar_config_receiver_enabled)
    {
        if (pCtxt->max9296_config.num_of_cameras == 2)
        {
            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                       pCtxt->ctrl,
                       MSM_SERIALIZER_ADDR,
                       &mx9295_init_msm_setting_dual)))
            {
                SERR("Failed to init msm serializer(0x%x)", MSM_SERIALIZER_ADDR);
                return -1;;
            }
        }
        else
        {
            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                       pCtxt->ctrl,
                       MSM_SERIALIZER_ADDR,
                       &mx9295_init_msm_setting_single)))
            {
                SERR("Failed to init msm serializer(0x%x)", MSM_SERIALIZER_ADDR);
                return -1;
            }
        }
    }
#endif

    pCtxt->state = MAX9296_STATE_INITIALIZED;
    return ret;
}

static int max9296_sensor_power_suspend(void* ctxt)
{
    int rc = 0;

    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;
    pCtxt->state = MAX9296_STATE_SUSPEND;

    return rc;
}

static int max9296_sensor_power_resume(void* ctxt)
{
    int rc = 0;

    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;
    pCtxt->state = MAX9296_STATE_INITIALIZED;

    return rc;
}

static int max9296_sensor_remap_channels(void* ctxt)
{
    int ret = 0;
    // List of serializer addresses we support
    uint16 serializer_addr[3] = {0xC4, 0x88, 0x80};
    int i;

    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

    if (pCtxt->state != MAX9296_STATE_DETECTED)
    {
        SHIGH("max9296 0x%x not be detected - wrong state", pCtxt->slave_addr);
        return -1;
    }

    pCtxt->num_connected_sensors = 0;

    /* Todo: refine the codes */

    /* Operation on Link A */
    {
        /* Select Link A*/
        struct camera_i2c_reg_array write_regs[] = DESERIALIZER_SELECT_LINK_A;
        struct camera_i2c_reg_setting write_setting =
        {
            .reg_setting = write_regs,
            .size = STD_ARRAY_SIZE(write_regs),
            .addr_type = CAMERA_I2C_WORD_ADDR,
            .data_type = CAMERA_I2C_BYTE_DATA,
        };
        ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(pCtxt->ctrl, pCtxt->slave_addr, &write_setting);
        if (ret)
        {
            SERR("Unable to select link A. Fatal error!", pCtxt->slave_addr);
            return -1;
        }

        /* Detect far end serializer */
        for (i = 0; i < 3; i++)
        {
            read_setting.reg_setting[0].reg_addr = MSM_SER_CHIP_ID_REG_ADDR;
            ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_read(pCtxt->ctrl, serializer_addr[i], &read_setting);
            if (!ret)
            {
                pCtxt->max9296_sensors[0].serializer = serializer_addr[i];
                break;
            }
        }

        if (i == 3)
        {
            SENSOR_WARN("No Camera connected to Link A of MAX9296 0x%x", pCtxt->slave_addr);
        }
        else
        {
            struct camera_i2c_reg_array write_regs[1], write_regs_2[2];
            struct camera_i2c_reg_setting write_setting =
            {
                .reg_setting = write_regs,
                .size = STD_ARRAY_SIZE(write_regs),
                .addr_type = CAMERA_I2C_WORD_ADDR,
                .data_type = CAMERA_I2C_BYTE_DATA,
            };
            struct camera_i2c_reg_setting write_setting_2 =
            {
                .reg_setting = write_regs_2,
                .size = STD_ARRAY_SIZE(write_regs_2),
                .addr_type = CAMERA_I2C_WORD_ADDR,
                .data_type = CAMERA_I2C_BYTE_DATA,
            };

            SENSOR_WARN("Detected Camera connected to Link A, Ser ID[0x%x]: 0x%x", MSM_SER_CHIP_ID_REG_ADDR, read_setting.reg_setting[0].reg_data);
            pCtxt->num_connected_sensors++;
            pCtxt->link_mode = MAX9296_MODE_LINK_A;

            /* Remap serializer to address in table */
            write_regs[0].reg_addr = 0x0000;
            write_regs[0].delay = 200000;
            write_regs[0].reg_data = pCtxt->max9296_sensors[0].serializer_alias;

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(pCtxt->ctrl, pCtxt->max9296_sensors[0].serializer, &write_setting)))
            {
                SERR("Failed to change serializer address (0x%x) of MAX9296 0x%x Link A", pCtxt->max9296_sensors[0].serializer, pCtxt->slave_addr);
                return -1;
            }

            // Perform link reset
            {
                struct camera_i2c_reg_array write_regs[] = SERIALIZER_LINK_RESET;
                struct camera_i2c_reg_setting write_setting =
                {
                    .reg_setting = write_regs,
                    .size = STD_ARRAY_SIZE(write_regs),
                    .addr_type = CAMERA_I2C_WORD_ADDR,
                    .data_type = CAMERA_I2C_BYTE_DATA,
                };
                if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                            pCtxt->ctrl, pCtxt->max9296_sensors[0].serializer_alias, &write_setting)))
                {
                    SERR("Failed to perform link reset on serializer(0x%x)", pCtxt->max9296_sensors[0].serializer_alias);
                    return -1;
                }
            }

            // Remap remote Sensor
            write_regs_2[0].reg_addr = 0x0042;
            write_regs_2[0].reg_data =  pCtxt->max9296_sensors[0].sensor_alias;
            write_regs_2[0].delay = _reg_delay_;
            write_regs_2[1].reg_addr = 0x0043;
            write_regs_2[1].reg_data = CAM_SENSOR_DEFAULT_ADDR;
            write_regs_2[1].delay = _reg_delay_;

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array( pCtxt->ctrl, pCtxt->max9296_sensors[0].serializer_alias, &write_setting_2)))
            {
                SERR("Failed to remap serializer sensor (0x%x) of MAX9296 0x%x Link A", pCtxt->max9296_sensors[0].serializer_alias, pCtxt->slave_addr);
                return -1;
            }

            // Create serializer broadcast address at 0x8E
            write_regs_2[0].reg_addr = 0x0044;
            write_regs_2[0].reg_data = CAM_SERIALIZER_BROADCAST_ADDR;
            write_regs_2[1].reg_addr = 0x0045;
            write_regs_2[1].reg_data = pCtxt->max9296_sensors[0].serializer_alias;

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array( pCtxt->ctrl, pCtxt->max9296_sensors[0].serializer_alias, &write_setting_2)))
            {
                SERR("Failed to Create ser (0x%x) broadcast address of MAX9296 0x%x Link A", pCtxt->max9296_sensors[0].serializer_alias, pCtxt->slave_addr);
                return -1;
            }

            // Change GMSL2 packet headers identifier values to unique
            {
                struct camera_i2c_reg_array write_regs_2[] = INIT_CAM_SERIALIZER_ADDR_CHNG_A;
                struct camera_i2c_reg_setting write_setting_2 =
                {
                    .reg_setting = write_regs_2,
                    .size = STD_ARRAY_SIZE(write_regs_2),
                    .addr_type = CAMERA_I2C_WORD_ADDR,
                    .data_type = CAMERA_I2C_BYTE_DATA,
                };
                if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                            pCtxt->ctrl, pCtxt->max9296_sensors[0].serializer_alias, &write_setting_2)))
                {
                    SERR("Failed to address change on serializer(0x%x)", pCtxt->max9296_sensors[0].serializer_alias);
                    return -1;
                }
            }

            // Read mapped SER to double check if successful
            read_setting.reg_setting[0].reg_addr = 0x0000;
            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_read(pCtxt->ctrl, pCtxt->max9296_sensors[0].serializer_alias, &read_setting)))
            {
                SERR("Failed to read serializer(0x%x)", pCtxt->max9296_sensors[0].serializer_alias);
                return -1;
            }

            if (pCtxt->max9296_sensors[0].serializer_alias != read_setting.reg_setting[0].reg_data)
            {
                SENSOR_WARN("Remote SER address remap failed: 0x%x, should be 0x%x", read_setting.reg_setting[0].reg_data, pCtxt->max9296_sensors[0].serializer_alias);
            }

            pCtxt->max9296_sensors[0].slave_state = SLAVE_STATE_DETECTED;
            pCtxt->sensor_lib.src_id_enable_mask |= (1 << 0);
        }
    }

    /* Operation on Link B */
    {
        /* Select Link B*/
        struct camera_i2c_reg_array write_regs[] = DESERIALIZER_SELECT_LINK_B;
        struct camera_i2c_reg_setting write_setting =
        {
            .reg_setting = write_regs,
            .size = STD_ARRAY_SIZE(write_regs),
            .addr_type = CAMERA_I2C_WORD_ADDR,
            .data_type = CAMERA_I2C_BYTE_DATA,
        };
        ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(pCtxt->ctrl, pCtxt->slave_addr, &write_setting);
        if (ret)
        {
            SERR("Unable to select link B. Fatal error!", pCtxt->slave_addr);
            return -1;
        }

        /* Read remote SER ID */
        for (i = 0; i < 3; i++)
        {
            read_setting.reg_setting[0].reg_addr = MSM_SER_CHIP_ID_REG_ADDR;
            ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_read(pCtxt->ctrl, serializer_addr[i], &read_setting);
            if (!ret)
            {
                pCtxt->max9296_sensors[1].serializer = serializer_addr[i];
                break;
            }
        }

        if (i == 3)
        {
            SENSOR_WARN("No Camera connected to Link B of MAX9296 0x%x", pCtxt->slave_addr);
        }
        else
        {
            struct camera_i2c_reg_array write_regs[1], write_regs_2[2];
            struct camera_i2c_reg_setting write_setting =
            {
                .reg_setting = write_regs,
                .size = STD_ARRAY_SIZE(write_regs),
                .addr_type = CAMERA_I2C_WORD_ADDR,
                .data_type = CAMERA_I2C_BYTE_DATA,
            };
            struct camera_i2c_reg_setting write_setting_2 =
            {
                .reg_setting = write_regs_2,
                .size = STD_ARRAY_SIZE(write_regs_2),
                .addr_type = CAMERA_I2C_WORD_ADDR,
                .data_type = CAMERA_I2C_BYTE_DATA,
            };

            SENSOR_WARN("Detected Camera connected to Link B, Ser ID[0x%x]: 0x%x", MSM_SER_CHIP_ID_REG_ADDR, read_setting.reg_setting[0].reg_data);
            pCtxt->num_connected_sensors++;
            pCtxt->link_mode = MAX9296_MODE_LINK_B;

            // Remap remote SER
            write_regs[0].reg_addr = 0x0000;
            write_regs[0].delay = 200000;
            write_regs[0].reg_data = pCtxt->max9296_sensors[1].serializer_alias;

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array( pCtxt->ctrl, pCtxt->max9296_sensors[1].serializer, &write_setting)))
            {
                SERR("Failed to change serializer address (0x%x) of MAX9296 0x%x Link B", pCtxt->max9296_sensors[1].serializer, pCtxt->slave_addr);
                return -1;
            }

            // Perform link reset
            {
                struct camera_i2c_reg_array write_regs[] = SERIALIZER_LINK_RESET;
                struct camera_i2c_reg_setting write_setting =
                {
                    .reg_setting = write_regs,
                    .size = STD_ARRAY_SIZE(write_regs),
                    .addr_type = CAMERA_I2C_WORD_ADDR,
                    .data_type = CAMERA_I2C_BYTE_DATA,
                };
                if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                            pCtxt->ctrl, pCtxt->max9296_sensors[1].serializer_alias, &write_setting)))
                {
                    SERR("Failed to perform link reset on serializer(0x%x)", pCtxt->max9296_sensors[1].serializer_alias);
                    return -1;
                }
            }

            // Remap remote Sensor
            write_regs_2[0].reg_addr = 0x0042;
            write_regs_2[0].reg_data = pCtxt->max9296_sensors[1].sensor_alias;
            write_regs_2[0].delay = _reg_delay_;
            write_regs_2[1].reg_addr = 0x0043;
            write_regs_2[1].reg_data = CAM_SENSOR_DEFAULT_ADDR;
            write_regs_2[1].delay = _reg_delay_;

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array( pCtxt->ctrl, pCtxt->max9296_sensors[1].serializer_alias, &write_setting_2)))
            {
                SERR("Failed to remap serializer sensor (0x%x) of MAX9296 0x%x Link B", pCtxt->max9296_sensors[1].serializer_alias, pCtxt->slave_addr);
                return -1;
            }

            // Create serializer broadcast address
            write_regs_2[0].reg_addr = 0x0044;
            write_regs_2[0].reg_data = CAM_SERIALIZER_BROADCAST_ADDR;
            write_regs_2[1].reg_addr = 0x0045;
            write_regs_2[1].reg_data = pCtxt->max9296_sensors[1].serializer_alias;

            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array( pCtxt->ctrl, pCtxt->max9296_sensors[1].serializer_alias, &write_setting_2)))
            {
                SERR("Failed to Create ser (0x%x) broadcast address of MAX9296 0x%x Link B", pCtxt->max9296_sensors[1].serializer_alias, pCtxt->slave_addr);
                return -1;
            }

            // Change GMSL2 packet headers identifier values to unique
            {
                struct camera_i2c_reg_array write_regs_2[] = INIT_CAM_SERIALIZER_ADDR_CHNG_B;
                struct camera_i2c_reg_setting write_setting_2 =
                {
                    .reg_setting = write_regs_2,
                    .size = STD_ARRAY_SIZE(write_regs_2),
                    .addr_type = CAMERA_I2C_WORD_ADDR,
                    .data_type = CAMERA_I2C_BYTE_DATA,
                };
                if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                            pCtxt->ctrl, pCtxt->max9296_sensors[1].serializer_alias, &write_setting_2)))
                {
                    SERR("Failed to address change on serializer(0x%x)", pCtxt->max9296_sensors[1].serializer_alias);
                    return -1;
                }
            }

            // Read mapped SER to double check if successful
            read_setting.reg_setting[0].reg_addr = 0x0000;
            if ((ret = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_read(pCtxt->ctrl, pCtxt->max9296_sensors[1].serializer_alias, &read_setting)))
            {
                SERR("Failed to read serializer(0x%x)", pCtxt->max9296_sensors[1].serializer_alias);
                return -1;
            }

            if (pCtxt->max9296_sensors[1].serializer_alias != read_setting.reg_setting[0].reg_data)
            {
                SENSOR_WARN("Remote SER address remap failed: 0x%x, should be 0x%x", read_setting.reg_setting[0].reg_data, pCtxt->max9296_sensors[1].serializer_alias);
            }

            pCtxt->max9296_sensors[1].slave_state = SLAVE_STATE_DETECTED;
            pCtxt->sensor_lib.src_id_enable_mask |= (1 << 1);
        }
    }

    return 0;
}

static int max9296_sensor_detect_device(void* ctxt)
{
    int rc = 0;

    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;
    SHIGH("detect max9296 0x%x", pCtxt->slave_addr);

    if (pCtxt->state >= MAX9296_STATE_DETECTED)
    {
        SHIGH("already detected");
        return 0;
    }

    if (!pCtxt->sensor_i2c_tbl_initialized ||
        !pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array ||
        !pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_read)
    {
      SERR("I2C function table not initialized");
      return -1;
    }

#ifdef MAX9296_ENABLE_MSM_SERIALIZER

    /* Check if MSM Serialzer is addressable */
    read_setting.reg_setting[0].reg_addr = MSM_DESER_CHIP_ID_REG_ADDR;
    if ((rc = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_read( pCtxt->ctrl, MSM_SERIALIZER_ADDR,
              &read_setting)))
    {
        SERR("Unable to read from the MX9295 Serialzer chip on the MSM. I2C address (0x%x)", MSM_SERIALIZER_ADDR);
        return -1;
    }

    // Disable any forwarding I2C commands in case another board is connected to this board
    // Since the I2C addresses are common, this causes corruption in each other's programming
    if ((rc = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
              pCtxt->ctrl,
              MSM_SERIALIZER_ADDR,
              &mx9295_init_msm_setting_disable_forward)))
    {
        SERR("Failed to disable forwarding on msm serializer(0x%x)", MSM_SERIALIZER_ADDR);
        return -1;
    }
#endif

    /* Check if anything is connected to the MSM Serialzer */

    /* Check if MSM Deserialzer is addressable */

    read_setting.reg_setting[0].reg_addr = MSM_SER_CHIP_ID_REG_ADDR;
    if ((rc = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_read(pCtxt->ctrl, pCtxt->slave_addr,
              &read_setting)))
    {
        SERR("Unable to read from the MAX9296 De-serialzer chip on the MSM. I2C address (0x%x)", pCtxt->slave_addr);
        return -1;
    }

    if (pCtxt->max9296_config.mizar_config_receiver_enabled)
    {
        // Disable any reverse channel for  I2C commands in case another board is connected to this board
        // Since the I2C addresses are common, this causes corruption in each other's programming
        // If this was a sender board then we don't want to disbale as we want to be able send I2C to cameras (so remove then)
        if ((rc = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                  pCtxt->ctrl,
                  pCtxt->slave_addr,
                  &max9296_setting_disable_reverse)))
        {
            SERR("Failed to disable reverse channle on msm deserializer(0x%x)", pCtxt->slave_addr);
            return -1;;
        }
    }

    read_setting.reg_setting[0].reg_addr = MSM_DESER_REVISION_REG_ADDR;
    if ((rc = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_read( pCtxt->ctrl, pCtxt->slave_addr,
              &read_setting)))
    {
        SERR("Failed to init de-serializer(0x%x)", pCtxt->slave_addr);
    }
    else
    {
        //Check if its an ES4 or later chip
        if (read_setting.reg_setting[0].reg_data != MSM_DESER_REVISION_ES2)
        {
            pCtxt->revision = 4;
        }
        else
        {
            pCtxt->revision = 2;
        }
        SENSOR_WARN("Maxim ICs revision %d detected!", pCtxt->revision);

        pCtxt->state = MAX9296_STATE_DETECTED;

        rc = max9296_sensor_remap_channels(pCtxt);
    }

    return rc;
}

static int max9296_sensor_detect_device_channels(void* ctxt)
{
    int rc = 0;

    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

    SHIGH("initialize max9296 0x%x with %d sensors", pCtxt->slave_addr, pCtxt->num_supported_sensors);

    if (pCtxt->state >= MAX9296_STATE_INITIALIZED)
    {
        SERR("already detected %d out of %d",pCtxt->num_connected_sensors, pCtxt->num_supported_sensors);
        return 0;
    }
    else if (pCtxt->state != MAX9296_STATE_DETECTED)
    {
        SERR("MAX9296 0x%x not detected - wrong state", pCtxt->slave_addr);
        return -1;
    }

    if (pCtxt->max9296_config.mizar_config_receiver_enabled)
    {
        //If receiver board no need to check for cameras just use what the use has indicated in the ini file
        pCtxt->num_connected_sensors = pCtxt->max9296_config.num_of_cameras;
        SENSOR_WARN("Receiver side forcing camera to %d", pCtxt->num_connected_sensors);
    }

    if (pCtxt->num_connected_sensors == 2)
    {
        pCtxt->link_mode = MAX9296_MODE_SPLITTER;
    }

    return rc;
}

static int max9296_sensor_init_setting(void* ctxt)
{
    int rc = 0;

    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

    place_marker("QCAR sensor init");

    if (!pCtxt->num_connected_sensors)
    {
        SHIGH("skipping as no sensors detected");
        return 0;
    }
    else
    {
        rc = max9296_set_init_sequence(pCtxt);
    }

    return rc;
}

static int max9296_sensor_set_channel_mode(void* ctxt, unsigned int src_id_mask, unsigned int mode)
{
    (void)ctxt;
    (void)mode;
    (void)src_id_mask;

    SERR("max9296_sensor_set_channel_mode()");

    return 0;
}

static int max9296_sensor_start_stream(void* ctxt, unsigned int src_id_mask)
{
    int rc = 0;
    unsigned int i = 0;
    unsigned int started_mask = 0;

    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

    SERR("max9296_sensor_start_stream()");

    //Now start the cameras
    for (i = 0; i < pCtxt->num_supported_sensors; i++)
    {
        if ((1 << i) & src_id_mask)
        {
            if (SLAVE_STATE_INITIALIZED == pCtxt->max9296_sensors[i].slave_state)
            {
                SENSOR_WARN("starting slave %x", pCtxt->max9296_sensors[i].serializer_alias);
                {
                    CameraLockMutex(pCtxt->mutex);

                    //Only write to camera if sender
                    if (!pCtxt->max9296_config.mizar_config_receiver_enabled)
                    {
                        rc = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                        pCtxt->ctrl,
                        pCtxt->max9296_sensors[i].serializer_alias,
                        &cam_serializer_start_setting);
                        if (rc)
                        {
                            SERR("sensor 0x%x failed to start", pCtxt->max9296_sensors[i].serializer_alias);
                            break;
                        }
                    }

                   CameraUnlockMutex(pCtxt->mutex);
                }

                pCtxt->max9296_sensors[i].slave_state = SLAVE_STATE_STREAMING;
                started_mask |= (1 << i);
            }
            else
            {
                /*TODO: change this to SERR once we limit which slaves to start*/
                SHIGH("sensor 0x%x not ready to start (state=%d) - bad state",
                        pCtxt->max9296_sensors[i].serializer_alias, pCtxt->max9296_sensors[i].slave_state);
            }
        }
    }


    if (!rc &&
        MAX9296_STATE_INITIALIZED == pCtxt->state &&
        started_mask)
    {
        struct camera_i2c_reg_setting * p_max9296_start_setting = NULL;

        switch (pCtxt->link_mode)
        {
            case MAX9296_MODE_LINK_A:
                p_max9296_start_setting = &max9296_start_link_a_setting;
                break;
            case MAX9296_MODE_LINK_B:
                p_max9296_start_setting = &max9296_start_link_b_setting;
                break;
            case MAX9296_MODE_SPLITTER:
                p_max9296_start_setting = &max9296_start_split_setting;
                break;
            default:
                p_max9296_start_setting = &max9296_start_split_setting;
                SENSOR_WARN("slave 0x%x unknown mode: %d", pCtxt->slave_addr, pCtxt->link_mode);
                break;
        }

        CameraLockMutex(pCtxt->mutex);
        //Start the deserialzer
        if ((rc = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                    pCtxt->ctrl,
                    pCtxt->slave_addr,
                    p_max9296_start_setting)))
        {
            SERR("Failed to start de-serializer(0x%x)", pCtxt->slave_addr);
        }
        CameraUnlockMutex(pCtxt->mutex);

        pCtxt->streaming_src_mask |= started_mask;
        pCtxt->state = MAX9296_STATE_STREAMING;
    }

    place_marker("QCAR sensor started");

    return rc;
}

static int max9296_sensor_stop_stream(void* ctxt, unsigned int src_id_mask)
{
    int rc = 0;
    unsigned int i;
    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

    SERR("max9296_sensor_stop_stream()");

    /*stop transmitter first if no more clients*/
    if (!rc && MAX9296_STATE_STREAMING == pCtxt->state)
    {
        pCtxt->streaming_src_mask &= (~src_id_mask);
        SHIGH("stopping max9296 0x%x transmitter (%x)", pCtxt->slave_addr, pCtxt->streaming_src_mask);

        /*stop if no slaves streaming*/
        if (!pCtxt->streaming_src_mask)
        {
            CameraLockMutex(pCtxt->mutex);
            if ((rc = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                    pCtxt->ctrl,
                    pCtxt->slave_addr,
                    &max9296_stop_setting)))
            {
                SERR("Failed to stop de-serializer(0x%x)", pCtxt->slave_addr);
            }
            CameraUnlockMutex(pCtxt->mutex);
            pCtxt->state = MAX9296_STATE_INITIALIZED;
        }
    }

    /*then stop slaves*/
    for (i = 0; i < pCtxt->num_supported_sensors; i++)
    {
        if ((1 << i) & src_id_mask)
        {
            SHIGH("stopping slave %x", pCtxt->max9296_sensors[i].serializer_alias);
            if (SLAVE_STATE_STREAMING == pCtxt->max9296_sensors[i].slave_state)
            {
                CameraLockMutex(pCtxt->mutex);
                if ((rc = pCtxt->sensor_platform_fcn_tbl.sensor_i2c_slave_write_array(
                        pCtxt->ctrl,
                        pCtxt->max9296_sensors[i].serializer_alias,
                        &max9295_stop_setting)))
                {
                    SERR("Failed to stop serializer(0x%x)", pCtxt->max9296_sensors[i].serializer_alias);
                }
                CameraUnlockMutex(pCtxt->mutex);
                pCtxt->max9296_sensors[i].slave_state = SLAVE_STATE_INITIALIZED;
            }
            else
            {
                /*TODO: change this to SERR once we limit which slaves to stop*/
                SHIGH("sensor 0x%x not in state to stop (state=%d) - bad state",
                        pCtxt->max9296_sensors[i].sensor_alias, pCtxt->max9296_sensors[i].slave_state);
            }
        }
    }

    /* TODO: cleanup in case of failure */

    place_marker("QCAR sensor stopped");

    return rc;
}

static int max9296_sensor_set_platform_func_table(void* ctxt, sensor_platform_func_table_t* table)
{
   max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

   if (!pCtxt->sensor_i2c_tbl_initialized)
    {
        if (!table)
        {
            SERR("Invalid i2c func table param");
            return -1;
        }
        pCtxt->sensor_platform_fcn_tbl = *table;
        pCtxt->sensor_i2c_tbl_initialized = 1;
        SLOW("i2c func table set");
    }

    return 0;
}

/**
 * FUNCTION: CameraSensorDevice_Open_max9296b
 *
 * DESCRIPTION: Entry function for device driver framework
 **/
CAM_API CameraResult CameraSensorDevice_Open_max9296b(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId)
{
    sensor_lib_interface_t sensor_lib_interface = {
            .sensor_open_lib = max9296_sensor_open_lib,
    };

    return CameraSensorDevice_Open(ppNewHandle, deviceId, &sensor_lib_interface);
}

