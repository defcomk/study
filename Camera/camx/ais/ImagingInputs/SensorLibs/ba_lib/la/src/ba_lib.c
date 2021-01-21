/**
 * @file ba_lib.c
 *
 * Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <asm/types.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <media/msm_ba.h>
#include <asm-generic/errno-base.h>

#include "CameraSensorDeviceInterface.h"
#include "ba_lib.h"
#include "SensorDebug.h"

#define BA_DEVICE_NAME "/dev/video35"
#define SENSOR_NAME_MAX_LEN 32

typedef enum {
    CVBS,
    HDMI,
    TUNER,
    SENSOR_MAX,
} ba_sensor_t;

typedef struct
{
    /*must be first element*/
    sensor_lib_t sensor_lib;
    ba_sensor_t input_type;
    int fd;
    void* ctrl;
} ba_context_t;

static sensor_lib_t ba_lib_ptr =
{
  .sensor_slave_info =
  {
      .sensor_name = SENSOR_MODEL,
      .addr_type = CAMERA_I2C_BYTE_ADDR,
      .data_type = CAMERA_I2C_BYTE_DATA,
      .i2c_freq_mode = SENSOR_I2C_MODE_CUSTOM,
  },
  .num_channels = 1,
  .channels =
  {
    {
      .output_mode =
      {
        .fmt = QCARCAM_FMT_UYVY_8,
        .res = {.width = 720, .height = 507, .fps = 30.0f},
        .channel_info = {.vc = 0, .dt = DT_AV7481, .cid = CID_VC0,},
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
          .channel_info = {.vc = 0, .dt = DT_AV7481, .cid = CID_VC0,},
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
    .settle_cnt = BA_SETTLE_COUNT,
    .lane_mask = 0x1F,
    .combo_mode = 0,
    .is_csi_3phase = 0,
  },
  .sensor_close_lib = &ba_sensor_close_lib,
  .sensor_capability = (1 << SENSOR_CAPABILITY_COLOR_CONFIG),
  .sensor_custom_func =
  {
    .sensor_detect_device = &ba_sensor_detect_device,
    .sensor_detect_device_channels = &ba_sensor_detect_device_channels,
    .sensor_init_setting = &ba_sensor_init_setting,
    .sensor_set_channel_mode = &ba_sensor_set_channel_mode,
    .sensor_start_stream = &ba_sensor_start_stream,
    .sensor_stop_stream = &ba_sensor_stop_stream,
    .sensor_config_resolution = &ba_sensor_config_resolution,
    .sensor_query_field = &ba_sensor_field_info_query,
    .sensor_set_platform_func_table = &ba_sensor_set_platform_func_table,
    .sensor_s_param = &ba_sensor_s_param,
  },
  .use_sensor_custom_func = TRUE,
};

/**
 * FUNCTION: ba_sensor_open_lib
 *
 * DESCRIPTION: Open sensor library and returns data pointer
 **/
static void* ba_sensor_open_lib(void* ctrl, void* arg)
{
    (void)ctrl;
    SHIGH("open BA lib called");
    sensor_open_lib_t* device_info = (sensor_open_lib_t*)arg;
    ba_context_t* pCtxt = calloc(1, sizeof(ba_context_t));
    if (pCtxt)
    {
        memcpy(&pCtxt->sensor_lib, &ba_lib_ptr, sizeof(pCtxt->sensor_lib));
        pCtxt->sensor_lib.sensor_slave_info.camera_id = device_info->camera_id;
        pCtxt->fd = -1;

        pCtxt->input_type = (ba_sensor_t)device_info->subdev_id;

        if (pCtxt->input_type == HDMI)
        {
            pCtxt->sensor_lib.channels[0].output_mode.res.width = 1280;
            pCtxt->sensor_lib.channels[0].output_mode.res.height = 720;
            pCtxt->sensor_lib.channels[0].output_mode.interlaced = FALSE;
            pCtxt->sensor_lib.channels[0].subchan_layout[0].stride = 2560;
            pCtxt->sensor_lib.subchannels[0].modes[0].res.width = 1280;
            pCtxt->sensor_lib.subchannels[0].modes[0].res.height = 720;
            pCtxt->sensor_lib.subchannels[0].modes[0].interlaced = FALSE;
            pCtxt->sensor_lib.csi_params.lane_cnt = 2;
        }
        if (pCtxt->input_type == TUNER)
        {
            pCtxt->sensor_lib.channels[0].output_mode.res.width = 1280;
            pCtxt->sensor_lib.channels[0].output_mode.res.height = 720;
            pCtxt->sensor_lib.channels[0].output_mode.interlaced = FALSE;
            pCtxt->sensor_lib.channels[0].subchan_layout[0].stride = 2560;
            pCtxt->sensor_lib.subchannels[0].modes[0].res.width = 1280;
            pCtxt->sensor_lib.subchannels[0].modes[0].res.height = 720;
            pCtxt->sensor_lib.subchannels[0].modes[0].interlaced = FALSE;
            pCtxt->sensor_lib.csi_params.lane_cnt = 2;
        }
        pCtxt->fd = open(BA_DEVICE_NAME, O_RDWR);

        if (pCtxt->fd < 0) {
            SERR("Failed to open ba device %s. fd %d", BA_DEVICE_NAME, pCtxt->fd);
            free (pCtxt);
            return NULL;
        } else {
            SHIGH("Open ba device %s successful! fd %d", BA_DEVICE_NAME, pCtxt->fd);
        }
    }
    return pCtxt;
}

static int ba_sensor_close_lib(void* ctxt)
{
    SHIGH("close BA lib called");
    if (ctxt) {
       ba_context_t* pCtxt = (ba_context_t*)ctxt;
       if(pCtxt->fd < 0)
       {
          SERR("invalid fd  %d", pCtxt->fd);
       }
       else
       {
          close(pCtxt->fd);
       }
       free(ctxt);
    }
    return 0;
}

/**
 * FUNCTION: ba_sensor_init_setting
 *
 * DESCRIPTION: init setting of ba driver
 **/
static int ba_sensor_init_setting (void* ctxt)
{
    (void)ctxt;
    SHIGH("ba_sensor_init_setting called");
    return 0;
}


/**
 * FUNCTION: ba_sensor_detect_device
 *
 * DESCRIPTION: detect and initialize BA driver
 **/
static int ba_sensor_detect_device (void* ctxt)
{
    (void)ctxt;
    SHIGH("detect device called");
    return 0;
}

/**
 * FUNCTION: ba_sensor_detect_device_channels
 *
 * DESCRIPTION: open BA instance
 **/
static int ba_sensor_detect_device_channels (void* ctxt)
{
    ba_context_t* pCtxt = (ba_context_t*)ctxt;
    struct v4l2_input input;
    char *sensor_name;

    SHIGH(" ba_sensor_detect_device_channels called %s", BA_DEVICE_NAME);

    if (pCtxt) {
        sensor_name = (pCtxt->input_type == CVBS) ? "CVBS" : (pCtxt->input_type == HDMI) ? "HDMI" : "TUNER";
        pCtxt->sensor_lib.src_id_enable_mask = 0;
    } else {
        SERR("Null ctxt");
        return -EINVAL;
    }

    memset(&input, 0, sizeof(input));
    while (ioctl(pCtxt->fd, VIDIOC_ENUMINPUT, &input) >= 0) {
        SHIGH("Port %d(%s)", input.index, input.name);
        if (!strncmp((const char *)input.name, sensor_name, strlen(sensor_name))) {
            SHIGH("Set available to port %d(%s)", input.index, input.name);
            pCtxt->sensor_lib.src_id_enable_mask = 1;
            break;
        }
        input.index++;
    }

    return 0;
}

/**
 * FUNCTION: ba_sensor_set_channel_mode
 *
 * DESCRIPTION: connect ba input port
 **/
static int ba_sensor_set_channel_mode (void* ctxt, unsigned int src_id_mask, unsigned int mode)
{
    (void) mode;
    (void) src_id_mask;

    ba_context_t* pCtxt = (ba_context_t*)ctxt;
    char *sensor_name;

    sensor_name = (pCtxt->input_type == CVBS) ? "CVBS" : (pCtxt->input_type == HDMI) ? "HDMI" : "TUNER";
    int str_len = strlen(sensor_name);
    int rc = 0;
    struct v4l2_input input;
    uint32_t device_id = 0;

    memset(&input, 0, sizeof(input));
    rc = ioctl(pCtxt->fd, VIDIOC_G_INPUT, &input.index);
    if (rc < 0) {
        SERR("Could not get input port: rc %d(%s)", rc, strerror(errno));
    } else {
        rc = -ENODEV;
        while (ioctl(pCtxt->fd, VIDIOC_ENUMINPUT, &input) >= 0) {
            SHIGH("Port %d(%s)", input.index, input.name);
            if (!strncmp((const char *)input.name,sensor_name, str_len)) {
                SHIGH("Set input to port %d(%s)", input.index, input.name);
                device_id = input.index;
                rc = ioctl(pCtxt->fd, VIDIOC_S_INPUT, &device_id);
                if (rc < 0)
                    SERR("Error setting input for port %d(%s)", input.index, input.name);
                break;
            }
            input.index++;
        }
    }

    SHIGH("ba_sensor_set_channel_mode called %s, status = %d", BA_DEVICE_NAME, rc);
    return rc;
}

/**
 * FUNCTION: ba_sensor_start_stream
 *
 * DESCRIPTION: BA start stream
 **/
static int ba_sensor_start_stream(void* ctxt, unsigned int src_id_mask)
{
    int rc = 0;
    (void)src_id_mask;

    ba_context_t* pCtxt =  (ba_context_t* )ctxt;

    if (pCtxt->fd < 0) {
        SERR("Invalid  ba device %s. fd %d", BA_DEVICE_NAME, pCtxt->fd);
        return -EIO;
    } else {
        enum v4l2_buf_type buf_type;
        SHIGH("ba_sensor_start_stream called %s, fd %d", BA_DEVICE_NAME, pCtxt->fd);
        rc = ioctl(pCtxt->fd, VIDIOC_STREAMON, &buf_type);
        if (rc < 0)
            SERR("Error in ba stream ON. rc %d", rc);
    }
    return rc;
}

/**
 * FUNCTION: ba_sensor_stop_stream
 *
 * DESCRIPTION: BA stop stream
 **/
static int ba_sensor_stop_stream(void* ctxt, unsigned int src_id_mask)
{
    int rc = 0;
    (void)src_id_mask;

    ba_context_t* pCtxt =  (ba_context_t* )ctxt;

    if (pCtxt->fd < 0) {
        SERR("Invalid ba device %s. fd %d", BA_DEVICE_NAME, pCtxt->fd);
        return -EIO;
    } else {
        enum v4l2_buf_type buf_type;
        SHIGH("ba_sensor_stop_stream called %s, fd %d", BA_DEVICE_NAME, pCtxt->fd);
        rc = ioctl(pCtxt->fd, VIDIOC_STREAMOFF, &buf_type);
        if (rc < 0)
            SERR("Error in ba stream off. rc %d", rc);
    }
    return rc;
}

/**
 * FUNCTION: ba_sensor_config_resolution
 *
 * DESCRIPTION: config dynamic resolution
 **/
static int ba_sensor_config_resolution (void* ctxt, int32 width, int32 height)
{
    int rc = 0;
    ba_context_t* pCtxt =  (ba_context_t* )ctxt;
    struct csi_ctrl_params csi_ctrl;
    struct msm_ba_v4l2_ioctl_t ba_ctrl;

    if (pCtxt->input_type == HDMI) {
        ba_ctrl.ptr = (void *)&csi_ctrl;
        ba_ctrl.len = sizeof(struct csi_ctrl_params);

        rc = ioctl(pCtxt->fd, VIDIOC_G_CSI_PARAMS, &ba_ctrl);
        if (rc < 0) {
            SERR("Failed to get CSI params for %dx%d: rc %d(%s)", width, height, rc, strerror(errno));
            return -EINVAL;
        }
        pCtxt->sensor_lib.csi_params.settle_cnt = csi_ctrl.settle_count;
        pCtxt->sensor_lib.csi_params.lane_cnt = csi_ctrl.lane_count;
    }
    SHIGH("%s:resolution %dx%d settle_cnt %d lane_cnt %d", __func__, width, height,
                pCtxt->sensor_lib.csi_params.settle_cnt, pCtxt->sensor_lib.csi_params.lane_cnt);
    return rc;
}

/**
 * FUNCTION: ba_sensor_field_info_query
 *
 * DESCRIPTION: get the current field info
 **/
static int ba_sensor_field_info_query (void* ctxt, boolean *even_field, uint64_t *field_ts)
{
    int rc = 0;
    ba_context_t* pCtxt =  (ba_context_t* )ctxt;
    struct field_info_params field_info;
    struct msm_ba_v4l2_ioctl_t ba_ctrl;

    ba_ctrl.ptr = (void *)&field_info;
    ba_ctrl.len = sizeof(struct field_info_params);

    rc = ioctl(pCtxt->fd, VIDIOC_G_FIELD_INFO, &ba_ctrl);
    if (rc < 0) {
        SERR("Failed to get field info: rc %d(%s)", rc, strerror(errno));
        return -EINVAL;
    }
    *even_field = field_info.even_field;
    *field_ts = field_info.field_ts.tv_sec*1000000000 + field_info.field_ts.tv_usec*1000;

    SHIGH("even field %d ts %llu", field_info.even_field,
        field_info.field_ts.tv_sec*1000000000 + field_info.field_ts.tv_usec*1000);

    return rc;
}

/**
 * FUNCTION: ba_sensor_set_i2c_func_table
 *
 * DESCRIPTION: set i2c function table
 **/
static int ba_sensor_set_platform_func_table (void* ctxt, sensor_platform_func_table_t* table)
{
    (void) ctxt;
    SHIGH("ba_sensor_set_i2c_func_table 0x%p", table);
    return 0;
}

/**
 * FUNCTION: ba_sensor_s_param
 *
 * DESCRIPTION: set color parameters
 **/
static int ba_sensor_s_param(void* ctxt, qcarcam_param_t param, unsigned int src_id, void* p_param)
{
    int rc = 0;

    if (ctxt == NULL)
    {
        SERR("ba_sensor_s_param Invalid ctxt");
        return -1;
    }

    if (p_param == NULL)
    {
        SERR("ba_sensor_s_param Invalid params");
        return -1;
    }

    switch(param)
    {
        default:
            SERR("ba_sensor_s_param. Param not supported = %d, src_id = %d", param, src_id);
            rc = -1;
            break;
    }
    return rc;
}

/**
 * FUNCTION: CameraSensorDevice_Open_ba
 *
 * DESCRIPTION: Entry function for device driver framework
 **/
CAM_API CameraResult CameraSensorDevice_Open_ba(CameraDeviceHandleType** ppNewHandle,
                                                CameraDeviceIDType deviceId)
{
    sensor_lib_interface_t sensor_lib_interface = {
        .sensor_open_lib = ba_sensor_open_lib,
    };
    SHIGH("CameraSensorDevice_Open_ba called");
    return CameraSensorDevice_Open(ppNewHandle, deviceId, &sensor_lib_interface);
}
