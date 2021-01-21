/**
 * @file max9296_lib.c
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
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
#include "CameraEventLog.h"
#include "max9296_lib.h"

#include "ar0231.h"
#include "ar0231_ext_isp.h"

#ifdef MAX9296_ENABLE_CONFIG_PARSER
/* INI file parser header file                                                */
#include "inifile_parser.h"

/* Input INI file to be parsed                                                */
#define FILE_NAME    "/data/misc/camera/mizar_config.ini"

#endif /* MAX9296_ENABLE_CONFIG_PARSER */

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
static int max9296_calculate_exposure(void* ctxt, unsigned int src_id, sensor_exposure_info_t* exposure_info);
static int max9296_sensor_exposure_config(void* ctxt, unsigned int src_id, sensor_exposure_info_t* exposure_info);

static sensor_lib_t sensor_lib_ptr =
{
  .sensor_slave_info =
  {
      .sensor_name = SENSOR_MODEL,
      .slave_addr = MSM_DES_0_SLAVEADDR,
      .i2c_freq_mode = SENSOR_I2C_MODE_CUSTOM,
      .addr_type = CAMERA_I2C_WORD_ADDR,
      .data_type = CAMERA_I2C_BYTE_DATA,
      .sensor_id_info =
      {
        .sensor_id_reg_addr = 0x00,
        .sensor_id = MSM_DES_0_SLAVEADDR,
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
            .delay = 1,
          },
          {
            .seq_type = CAMERA_POW_SEQ_GPIO,
            .seq_val = CAMERA_GPIO_RESET,
            .config_val = GPIO_OUT_HIGH,
            .delay = 20,
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
  .csi_params =
  {
    .lane_cnt = 4,
    .settle_cnt = 0xE,
    .lane_mask = 0x1F,
    .combo_mode = 0,
    .is_csi_3phase = 0,
  },
  .sensor_close_lib = max9296_sensor_close_lib,
  .exposure_func_table =
  {
    .sensor_calculate_exposure = &max9296_calculate_exposure,
    .sensor_exposure_config = &max9296_sensor_exposure_config,
  },
  .sensor_capability = (1 << SENSOR_CAPABILITY_EXPOSURE_CONFIG),
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

static max9296_topology_config_t default_config =
{
    .opMode = MAXIM_OP_MODE_DEFAULT,
    .num_of_cameras = 2,
    .sensor_id = {
        MAXIM_SENSOR_ID_AR0231_EXT_ISP,
        MAXIM_SENSOR_ID_AR0231_EXT_ISP
    }
};

static max9296_sensor_info_t max9296_sensors_init_table[] =
{
  {
      .state = SENSOR_STATE_INVALID,
      .serializer_alias = MSM_DES_0_ALIAS_ADDR_CAM_SER_0,
      .sensor_alias = MSM_DES_0_ALIAS_ADDR_CAM_SNSR_0,
  },
  {
      .state = SENSOR_STATE_INVALID,
      .serializer_alias = MSM_DES_0_ALIAS_ADDR_CAM_SER_1,
      .sensor_alias = MSM_DES_0_ALIAS_ADDR_CAM_SNSR_1,
  },
};

static struct camera_i2c_reg_array max9296_mode_split_receiver[] = CAM_DES_INIT_SPLITTER_MODE_RECEIVER;
static struct camera_i2c_reg_array max9296_mode_split_sender[] = CAM_DES_INIT_SPLITTER_MODE_SENDER;
static struct camera_i2c_reg_array max9296_mode_link_receiver[] = CAM_DES_INIT_LINK_MODE_RECEIVER;
static struct camera_i2c_reg_array max9296_start_reg_array[] = CAM_DES_START;
static struct camera_i2c_reg_array max9296_stop_reg_array[] = CAM_DES_STOP;
static struct camera_i2c_reg_array max9296_reg_array_disable_reverse[] = CAM_DES_DISABLE_I2C_REVERSE;

#define ADD_I2C_REG_ARRAY(_a_, _size_, _addr_, _val_, _delay_) \
do { \
    _a_[_size_].reg_addr = _addr_; \
    _a_[_size_].reg_data = _val_; \
    _a_[_size_].delay = _delay_; \
    _size_++; \
} while(0)

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
    if (MATCH(def_sec1, "op_mode"))
    {
        pconfig->opMode = atoi(param_value);
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
            pCtxt->max9296_config.opMode,
            pCtxt->max9296_config.num_of_cameras
            );

    /* checking valid ranges */
    if(pCtxt->max9296_config.opMode < 0 || pCtxt->max9296_config.opMode > 1)
    {
        printf("Unsupported value of opMode param value = %d\n", pCtxt->max9296_config.opMode);
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
                pCtxt->max9296_config.opMode,
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

    SERR("max9296_sensor_open_lib()");

    max9296_read_ini_file(pCtxt);

    if (pCtxt)
    {
        memcpy(&pCtxt->sensor_lib, &sensor_lib_ptr, sizeof(pCtxt->sensor_lib));
        memcpy(&pCtxt->max9296_sensors, max9296_sensors_init_table, sizeof(pCtxt->max9296_sensors));

        pCtxt->ctrl = ctrl;
        pCtxt->sensor_lib.sensor_slave_info.camera_id = device_info->camera_id;
        pCtxt->state = MAX9296_STATE_INVALID;
        pCtxt->num_supported_sensors = MAXIM_LINK_MAX;

        pCtxt->max9296_reg_setting.addr_type = CAMERA_I2C_WORD_ADDR;
        pCtxt->max9296_reg_setting.data_type = CAMERA_I2C_BYTE_DATA;

        for (i = 0; i < pCtxt->num_supported_sensors; i++)
        {
            pCtxt->max9296_sensors[i].state = SENSOR_STATE_INVALID;
        }

        /*default to dev id 0*/
        switch (device_info->subdev_id)
        {
        case 1:
        case 3:
            pCtxt->slave_addr = MSM_DES_1_SLAVEADDR;
            pCtxt->max9296_sensors[0].serializer_alias = MSM_DES_1_ALIAS_ADDR_CAM_SER_0;
            pCtxt->max9296_sensors[1].serializer_alias = MSM_DES_1_ALIAS_ADDR_CAM_SER_1;
            pCtxt->max9296_sensors[0].sensor_alias = MSM_DES_1_ALIAS_ADDR_CAM_SNSR_0;
            pCtxt->max9296_sensors[1].sensor_alias = MSM_DES_1_ALIAS_ADDR_CAM_SNSR_1;
            break;
        default:
            pCtxt->slave_addr = MSM_DES_0_SLAVEADDR;
            break;
        }

        pCtxt->subdev_id = device_info->subdev_id;
        pCtxt->sensor_lib.sensor_slave_info.sensor_id_info.sensor_id = pCtxt->slave_addr;
    }

    return pCtxt;
}

/**
 * FUNCTION: max9296_sensor_close_lib
 *
 * DESCRIPTION: Closes sensor library
 **/
static int max9296_sensor_close_lib(void* ctxt)
{
    free(ctxt);
    return 0;
}

static unsigned int max9296_get_bpp_from_dt(uint32 dt)
{
    switch(dt)
    {
    case 0:
        return 8;
    case CSI_DT_RAW10:
        return 10;
    case CSI_DT_RAW12:
        return 12;
    case CSI_DT_RAW8:
        return 8;
    default:
        SERR("get bpp for dt 0x%x not implemented", dt);
        return 8;
    }
}

/**
 * FUNCTION: max9296_set_init_sequence
 *
 * DESCRIPTION: max9296_set_init_sequence
 **/
static int max9296_set_init_sequence(max9296_context_t* pCtxt)
{
    int rc = 0;

    SENSOR_HIGH("max9296_set_init_sequence()");

    if (pCtxt->max9296_config.opMode != MAXIM_OP_MODE_RECEIVER)
    {
        int i = 0;
        struct camera_i2c_reg_array write_regs[] = { {0x0010, 0x20, MAX9296_SELECT_LINK_DELAY} };

        //Dual mode to program both cameras
        pCtxt->max9296_reg_setting.reg_array = write_regs;
        pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(write_regs);
        pCtxt->max9296_reg_setting.reg_array[0].reg_data |= pCtxt->sensor_lib.src_id_enable_mask;

        rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(pCtxt->ctrl, pCtxt->slave_addr, &pCtxt->max9296_reg_setting);
        if (rc)
        {
            SERR("Unable to set deserailzer 0x%x I2C Mode 0x%x. Fatal error!", pCtxt->slave_addr, pCtxt->sensor_lib.src_id_enable_mask);
            return -1;
        }
        SENSOR_WARN("Setting MAX9296 I2C to 0x%x", pCtxt->sensor_lib.src_id_enable_mask);

        for (i = 0; i < MAXIM_LINK_MAX; i++)
        {
            max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[i];
            if (pCtxt->sensor_lib.src_id_enable_mask & (1 << i))
            {
                rc = pSensor->sensor->init_link(pCtxt, i);
            }
        }
    }
    else //MAXIM_OP_MODE_RECEIVER
    {
        //Set them to intialize as data is coming from another mizar and not actual cameras here
        if (2 == pCtxt->max9296_config.num_of_cameras)
        {
            pCtxt->max9296_reg_setting.reg_array = max9296_mode_split_receiver;
            pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(max9296_mode_split_receiver);
            pCtxt->max9296_sensors[0].state = SENSOR_STATE_INITIALIZED;
            pCtxt->max9296_sensors[1].state = SENSOR_STATE_INITIALIZED;

            rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
                           pCtxt->ctrl, pCtxt->slave_addr, &pCtxt->max9296_reg_setting);
            if (rc)
            {
                SERR("Failed to init de-serializer(0x%x)", pCtxt->slave_addr);
            }
        }
        else
        {
            pCtxt->max9296_reg_setting.reg_array = max9296_mode_link_receiver;
            pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(max9296_mode_link_receiver);
            pCtxt->max9296_sensors[0].state = SENSOR_STATE_INITIALIZED;

            rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
                           pCtxt->ctrl, pCtxt->slave_addr, &pCtxt->max9296_reg_setting);
            if (rc)
            {
                SERR("Failed to init de-serializer(0x%x)", pCtxt->slave_addr);
            }
        }
    }

    {
        img_src_subchannel_t *pSubchannels = pCtxt->sensor_lib.subchannels;
        unsigned char val = 0;
        unsigned int seq_size = 0;
        unsigned int i = 0;

        val = 0x2 | (max9296_get_bpp_from_dt(pCtxt->pipelines[0].dt) << 3);
        ADD_I2C_REG_ARRAY(pCtxt->init_array, seq_size, 0x0313, val, _max9296_delay_);

        val = pCtxt->pipelines[0].dt | ((pCtxt->pipelines[1].dt & 0x30) << 2);
        ADD_I2C_REG_ARRAY(pCtxt->init_array, seq_size, 0x0316, val, _max9296_delay_);

        val = (pCtxt->pipelines[1].dt & 0x0F) | ((pCtxt->pipelines[2].dt & 0x3C) << 2);
        ADD_I2C_REG_ARRAY(pCtxt->init_array, seq_size, 0x0317, val, _max9296_delay_);

        val = (pCtxt->pipelines[2].dt & 0x03) | (pCtxt->pipelines[3].dt << 2);
        ADD_I2C_REG_ARRAY(pCtxt->init_array, seq_size, 0x0318, val, _max9296_delay_);

        val = max9296_get_bpp_from_dt(pCtxt->pipelines[1].dt) |
            ((max9296_get_bpp_from_dt(pCtxt->pipelines[2].dt) & 0x1C) << 3);
        ADD_I2C_REG_ARRAY(pCtxt->init_array, seq_size, 0x0319, val, _max9296_delay_);

        val = (max9296_get_bpp_from_dt(pCtxt->pipelines[2].dt) & 0x3) |
            (max9296_get_bpp_from_dt(pCtxt->pipelines[3].dt) << 2);
        ADD_I2C_REG_ARRAY(pCtxt->init_array, seq_size, 0x031A, val, _max9296_delay_);

        for (i = 0; i < pCtxt->sensor_lib.num_subchannels; i++)
        {
            uint32 stream_id = pSubchannels[i].src_id;
            uint32 vc = pSubchannels[i].modes[0].channel_info.vc;
            uint32 dt = pSubchannels[i].modes[0].channel_info.dt;

            ADD_I2C_REG_ARRAY(pCtxt->init_array, seq_size, 0x040D + (0x40*stream_id), dt, _max9296_delay_);
            ADD_I2C_REG_ARRAY(pCtxt->init_array, seq_size, 0x040E + (0x40*stream_id), ((vc << 6) | dt), _max9296_delay_);
        }

        /* default to mode split sender */
        memcpy(&pCtxt->init_array[seq_size],
            max9296_mode_split_sender,
            sizeof(max9296_mode_split_sender));

        seq_size += STD_ARRAY_SIZE(max9296_mode_split_sender);

        for (i = 0; i < seq_size; i++)
        {
            SHIGH("%d - 0x%x, 0x%x, %d", i,
                pCtxt->init_array[i].reg_addr, pCtxt->init_array[i].reg_data, pCtxt->init_array[i].delay);
        }

        pCtxt->max9296_reg_setting.reg_array = pCtxt->init_array;
        pCtxt->max9296_reg_setting.size = seq_size;
        rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
                       pCtxt->ctrl, pCtxt->slave_addr, &pCtxt->max9296_reg_setting);
        if (rc)
        {
            SERR("Failed to init de-serializer(0x%x)", pCtxt->slave_addr);
        }
    }

    if (!rc)
    {
        pCtxt->state = MAX9296_STATE_INITIALIZED;
    }

    return rc;
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

static int max9296_sensor_remap_channels(max9296_context_t* pCtxt)
{
    int rc = 0;
    int i = 0;

    if (pCtxt->state != MAX9296_STATE_DETECTED)
    {
        SHIGH("max9296 0x%x not be detected - wrong state", pCtxt->slave_addr);
        return -1;
    }

    for (i = 0; i < MAXIM_LINK_MAX; i++)
    {
        max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[i];

        switch (pCtxt->max9296_config.sensor_id[i])
        {
        case MAXIM_SENSOR_ID_AR0231:
            pSensor->sensor = &ar0231_info;
            break;
        case MAXIM_SENSOR_ID_AR0231_EXT_ISP:
            pSensor->sensor = &ar0231_ext_isp_info;
            break;
        default:
            SERR("Slave ID %d NOT SUPPORTED", pCtxt->max9296_config.sensor_id[i]);
            pSensor->state = SENSOR_STATE_UNSUPPORTED;
            break;
        }

        if (pSensor->sensor)
        {
            //Enable I2C forwarding and select link
            struct camera_i2c_reg_array write_regs[] = { {0x0001, 0x02, _max9296_delay_},
                                                         {0x0010, 0x20 | (1 << i), MAX9296_SELECT_LINK_DELAY} };
            pCtxt->max9296_reg_setting.reg_array = write_regs;
            pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(write_regs);

            if ((rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(pCtxt->ctrl, pCtxt->slave_addr, &pCtxt->max9296_reg_setting)))
            {
                SERR("Unable to set deserailzer 0x%x I2C Mode Link %d. Fatal error!", pCtxt->slave_addr, i);
                return -1;
            }

            rc = pSensor->sensor->detect(pCtxt, i);
            if (!rc)
            {
                pSensor->state = SENSOR_STATE_DETECTED;
                pCtxt->sensor_lib.src_id_enable_mask |= (1 << i);
                pCtxt->num_connected_sensors++;
            }
        }
    }

    return 0;
}

static int max9296_sensor_detect_device(void* ctxt)
{
    int rc = 0;
    struct camera_i2c_reg_array dummy_reg[] = {{0, 0, 0}};
    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

    SHIGH("detect max9296 0x%x", pCtxt->slave_addr);

    if (pCtxt->state >= MAX9296_STATE_DETECTED)
    {
        SHIGH("already detected");
        return 0;
    }

    if (!pCtxt->platform_fcn_tbl_initialized)
    {
        SERR("I2C function table not initialized");
        return -1;
    }

    //Detect MAX9296
    pCtxt->max9296_reg_setting.reg_array = dummy_reg;
    pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(dummy_reg);
    pCtxt->max9296_reg_setting.reg_array[0].reg_addr = MSM_DES_CHIP_ID_REG_ADDR;
    if ((rc = pCtxt->platform_fcn_tbl.i2c_slave_read(pCtxt->ctrl, pCtxt->slave_addr,
              &pCtxt->max9296_reg_setting)))
    {
        SERR("Unable to read from the MAX9296 De-serialzer 0x%x (0x%08x)",
            pCtxt->slave_addr, pCtxt->sensor_lib.sensor_slave_info.camera_id);
        return -1;
    }

    if (MAXIM_OP_MODE_RECEIVER == pCtxt->max9296_config.opMode)
    {
        pCtxt->max9296_reg_setting.reg_array = max9296_reg_array_disable_reverse;
        pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(max9296_reg_array_disable_reverse);

        // Disable any reverse channel for  I2C commands in case another board is connected to this board
        // Since the I2C addresses are common, this causes corruption in each other's programming
        // If this was a sender board then we don't want to disbale as we want to be able send I2C to cameras (so remove then)
        if ((rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
                  pCtxt->ctrl,
                  pCtxt->slave_addr,
                  &pCtxt->max9296_reg_setting)))
        {
            SERR("Failed to disable reverse channel on msm deserializer(0x%x)", pCtxt->slave_addr);
            return -1;;
        }
    }

    pCtxt->max9296_reg_setting.reg_array = dummy_reg;
    pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(dummy_reg);
    pCtxt->max9296_reg_setting.reg_array[0].reg_addr = MSM_DES_REVISION_REG_ADDR;
    if ((rc = pCtxt->platform_fcn_tbl.i2c_slave_read( pCtxt->ctrl, pCtxt->slave_addr,
              &pCtxt->max9296_reg_setting)))
    {
        SERR("Failed to read de-serializer(0x%x) revision", pCtxt->slave_addr);
    }
    else
    {
        //Check if its an ES4 or later chip
        if (pCtxt->max9296_reg_setting.reg_array[0].reg_addr != MSM_DES_REVISION_ES2)
        {
            pCtxt->revision = 4;
        }
        else
        {
            pCtxt->revision = 2;
        }
        SENSOR_WARN("MAX9296 revision %d detected!", pCtxt->revision);

        //Disable I2C forwarding to allow other devices on the same I2C bus remap their sensors
        struct camera_i2c_reg_array write_regs[] = { {0x0001, 0x12, _max9296_delay_} };
        pCtxt->max9296_reg_setting.reg_array = write_regs;
        pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(write_regs);

        if ((rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(pCtxt->ctrl, pCtxt->slave_addr, &pCtxt->max9296_reg_setting)))
        {
            SERR("Unable to disable 0x%x I2C.", pCtxt->slave_addr);
            return -1;
        }

        pCtxt->state = MAX9296_STATE_DETECTED;
    }
    return rc;
}

static int max9296_sensor_detect_device_channels(void* ctxt)
{
    int rc = 0;
    int i = 0;

    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

    SHIGH("initialize max9296 0x%x with %d sensors", pCtxt->slave_addr, pCtxt->num_supported_sensors);

    if (pCtxt->state >= MAX9296_STATE_INITIALIZED)
    {
        SERR("already detected %d out of %d", pCtxt->num_connected_sensors, pCtxt->num_supported_sensors);
        return 0;
    }
    else if (pCtxt->state != MAX9296_STATE_DETECTED)
    {
        SERR("MAX9296 0x%x not detected - wrong state", pCtxt->slave_addr);
        return -1;
    }

    if (pCtxt->max9296_config.opMode == MAXIM_OP_MODE_RECEIVER)
    {
        //If receiver board no need to check for cameras just use what the use has indicated in the ini file
        pCtxt->num_connected_sensors = pCtxt->max9296_config.num_of_cameras;
        SENSOR_WARN("Receiver side forcing camera to %d", pCtxt->num_connected_sensors);
    }

    // Allow sensors to fully power up before trying to detect it
    // Only needed for first subdev_id since all sensors will be powered on by this time
    if (pCtxt->subdev_id == 0)
    {
        CameraSleep(250);
    }

    if (0 != (rc = max9296_sensor_remap_channels(pCtxt)))
    {
        SERR("Unable to remap deserailzer 0x%x", pCtxt->slave_addr);
        return -1;
    }

    for (i = 0; i < MAXIM_LINK_MAX; i++)
    {
        max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[i];

        if (pSensor->state != SENSOR_STATE_DETECTED)
        {
            continue;
        }

        rc = pSensor->sensor->get_link_cfg(pCtxt, i, &pSensor->link_cfg);

        if (!rc)
        {
            int stream_id = i;
            int j = 0;

            if (pSensor->link_cfg.num_pipelines > MAX_LINK_PIPELINES)
            {
                SERR("Only support max of %d pipelines per link", MAX_LINK_PIPELINES);
                pSensor->link_cfg.num_pipelines = MAX_LINK_PIPELINES;
            }

            /**
             * @todo: for now we only support single pipeline per link.
             * We fix assignment of stream_id as 0 for link A and 1 for link B
             *
             * for (j = 0; j < pSensor->link_cfg.num_pipelines; j++)
             */
            {
                img_src_channel_t *pChannel = &pCtxt->sensor_lib.channels[pCtxt->sensor_lib.num_channels];
                img_src_subchannel_t *pSubChannel = &pCtxt->sensor_lib.subchannels[pCtxt->sensor_lib.num_subchannels];
                img_src_subchannel_layout_t layout = {
                    .src_id = stream_id,
                };

                pCtxt->pipelines[stream_id] = pSensor->link_cfg.pipelines[j].mode.channel_info;

                pChannel->num_subchannels = 1;
                pChannel->output_mode = pSensor->link_cfg.pipelines[j].mode;
                pChannel->subchan_layout[0] = layout;

                pSubChannel->src_id = stream_id;
                pSubChannel->modes[0] = pSensor->link_cfg.pipelines[j].mode;
                pSubChannel->num_modes = 1;

                pSensor->link_cfg.pipelines[j].stream_id = stream_id;

                pCtxt->sensor_lib.num_channels++;
                pCtxt->sensor_lib.num_subchannels++;
            }
        }
    }

    CameraLogEvent(CAMERA_SENSOR_EVENT_PROBED, 0, 0);

    return rc;
}

static int max9296_sensor_init_setting(void* ctxt)
{
    int rc = 0;

    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

    if (!pCtxt->num_connected_sensors)
    {
        SHIGH("skipping as no sensors detected");
        return 0;
    }
    else
    {
        CameraLogEvent(CAMERA_SENSOR_EVENT_INITIALIZE_START, 0, 0);

        ais_log_kpi(AIS_EVENT_KPI_SENSOR_PROG_START);
        rc = max9296_set_init_sequence(pCtxt);
        ais_log_kpi(AIS_EVENT_KPI_SENSOR_PROG_END);

        CameraLogEvent(CAMERA_SENSOR_EVENT_INITIALIZE_DONE, 0, 0);
    }

    return rc;
}

static int max9296_sensor_set_channel_mode(void* ctxt, unsigned int src_id_mask, unsigned int mode)
{
    (void)ctxt;
    (void)mode;
    (void)src_id_mask;

    SHIGH("max9296_sensor_set_channel_mode()");

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
            if (SENSOR_STATE_INITIALIZED == pCtxt->max9296_sensors[i].state)
            {
                SENSOR_WARN("starting slave %x", pCtxt->max9296_sensors[i].serializer_alias);
                {
                    CameraLockMutex(pCtxt->mutex);

                    //Only write to camera if sender
                    if (!pCtxt->max9296_config.opMode)
                    {
                        pCtxt->max9296_sensors[i].sensor->start_link(pCtxt, i);
                    }

                   CameraUnlockMutex(pCtxt->mutex);
                }

                pCtxt->max9296_sensors[i].state = SENSOR_STATE_STREAMING;
                started_mask |= (1 << i);
            }
            else
            {
                /*TODO: change this to SERR once we limit which slaves to start*/
                SHIGH("sensor 0x%x not ready to start (state=%d) - bad state",
                        pCtxt->max9296_sensors[i].serializer_alias, pCtxt->max9296_sensors[i].state);
            }
        }
    }


    if (!rc &&
        MAX9296_STATE_INITIALIZED == pCtxt->state &&
        started_mask)
    {
        //@todo: dynamically only start needed streams
        pCtxt->max9296_reg_setting.reg_array = max9296_start_reg_array;
        pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(max9296_start_reg_array);

        SHIGH("starting deserializer");

        CameraLockMutex(pCtxt->mutex);
        //Start the deserialzer
        if ((rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
                    pCtxt->ctrl,
                    pCtxt->slave_addr,
                    &pCtxt->max9296_reg_setting)))
        {
            SERR("Failed to start de-serializer(0x%x)", pCtxt->slave_addr);
        }
        CameraUnlockMutex(pCtxt->mutex);

        pCtxt->streaming_src_mask |= started_mask;
        pCtxt->state = MAX9296_STATE_STREAMING;
    }

    CameraLogEvent(CAMERA_SENSOR_EVENT_STREAM_START, 0, 0);
    SHIGH("max9296(0x%x) streaming...", pCtxt->slave_addr);

    return rc;
}

static int max9296_sensor_stop_stream(void* ctxt, unsigned int src_id_mask)
{
    int rc = 0;
    unsigned int i;
    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

    SHIGH("max9296_sensor_stop_stream()");

    /*stop transmitter first if no more clients*/
    if (!rc && MAX9296_STATE_STREAMING == pCtxt->state)
    {
        pCtxt->streaming_src_mask &= (~src_id_mask);
        SHIGH("stopping max9296 0x%x transmitter (%x)", pCtxt->slave_addr, pCtxt->streaming_src_mask);

        /*stop if no slaves streaming*/
        if (!pCtxt->streaming_src_mask)
        {
            pCtxt->max9296_reg_setting.reg_array = max9296_stop_reg_array;
            pCtxt->max9296_reg_setting.size = STD_ARRAY_SIZE(max9296_stop_reg_array);
            CameraLockMutex(pCtxt->mutex);
            if ((rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
                    pCtxt->ctrl,
                    pCtxt->slave_addr,
                    &pCtxt->max9296_reg_setting)))
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

            CameraLockMutex(pCtxt->mutex);

            rc = pCtxt->max9296_sensors[i].sensor->stop_link(pCtxt, i);

            CameraUnlockMutex(pCtxt->mutex);
            if (rc)
            {
                /*TODO: change this to SERR once we limit which slaves to stop*/
                SHIGH("sensor 0x%x not in state to stop (state=%d) - bad state",
                        pCtxt->max9296_sensors[i].sensor_alias, pCtxt->max9296_sensors[i].state);
            }
        }
    }

    /* TODO: cleanup in case of failure */

    CameraLogEvent(CAMERA_SENSOR_EVENT_STREAM_STOP, 0, 0);
    SHIGH("max9296(0x%x) stopped", pCtxt->slave_addr);

    return rc;
}

static int max9296_sensor_set_platform_func_table(void* ctxt, sensor_platform_func_table_t* table)
{
   max9296_context_t* pCtxt = (max9296_context_t*)ctxt;

   if (!pCtxt->platform_fcn_tbl_initialized)
    {
        if (!table || !table->i2c_slave_write_array || !table->i2c_slave_read)
        {
            SERR("Invalid i2c func table param");
            return -1;
        }

        pCtxt->platform_fcn_tbl = *table;
        pCtxt->platform_fcn_tbl_initialized = 1;
        SLOW("i2c func table set");
    }

    return 0;
}

static int max9296_calculate_exposure(void* ctxt, unsigned int src_id, sensor_exposure_info_t* exposure_info)
{
    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;
    int rc = 0;

    if (src_id < MAXIM_LINK_MAX)
    {
        if (pCtxt->max9296_sensors[src_id].sensor->calculate_exposure)
        {
            rc = pCtxt->max9296_sensors[src_id].sensor->calculate_exposure(pCtxt, src_id, exposure_info);
        }
    }
    else
    {
        SERR("invalid src_id %d", src_id);
        rc = -1;
    }

    return rc;
}


static int max9296_sensor_exposure_config(void* ctxt, unsigned int src_id, sensor_exposure_info_t* exposure_info)
{
    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;
    int rc = 0;

    if (src_id < MAXIM_LINK_MAX)
    {
        if (pCtxt->max9296_sensors[src_id].sensor->apply_exposure)
        {
            rc = pCtxt->max9296_sensors[src_id].sensor->apply_exposure(pCtxt, src_id, exposure_info);
        }
    }
    else
    {
        SERR("invalid src_id %d", src_id);
        rc = -1;
    }

    return rc;
}

/**
 * FUNCTION: CameraSensorDevice_Open_max9296
 *
 * DESCRIPTION: Entry function for device driver framework
 **/
CAM_API CameraResult CameraSensorDevice_Open_max9296(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId)
{
    sensor_lib_interface_t sensor_lib_interface = {
            .sensor_open_lib = max9296_sensor_open_lib,
    };

    return CameraSensorDevice_Open(ppNewHandle, deviceId, &sensor_lib_interface);
}

