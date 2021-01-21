/**
 * @file ba_lib.h
 *
 * Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __BA_LIB_H__
#define __BA_LIB_H__

#include "sensor_lib.h"

static int ba_sensor_close_lib(void* ctrl);
static int ba_sensor_detect_device (void* ctxt);
static int ba_sensor_detect_device_channels (void* ctxt);
static int ba_sensor_init_setting (void* ctxt);
static int ba_sensor_set_channel_mode (void* ctxt, unsigned int src_id_mask, unsigned int mode);
static int ba_sensor_start_stream(void* ctxt, unsigned int src_id_mask);
static int ba_sensor_stop_stream(void* ctxt, unsigned int src_id_mask);
static int ba_sensor_config_resolution(void* ctxt, int32 width, int32 height);
static int ba_sensor_field_info_query (void* ctxt, boolean *even_field, uint64_t *field_ts);
static int ba_sensor_set_platform_func_table (void* ctxt, sensor_platform_func_table_t* table);
static int ba_sensor_param_config(void* ctxt, qcarcam_param_t param, unsigned int src_id, void* p_param);

#define BA_SETTLE_COUNT 0x10

#define DT_AV7481 0x1e //8 bit yuv422
#define DT_BITDEPTH CSI_DECODE_8BIT


#define CID_VC0        0
#define CID_VC1        4
#define CID_VC2        8
#define CID_VC3        12

#define VC0 0
#define VC1 (1 << 6)
#define VC2 (2 << 6)
#define VC3 (3 << 6)

#define SENSOR_MODEL "ba"
#define DEBUG_SENSOR_NAME SENSOR_MODEL

#endif /* __BA_LIB_H__ */
