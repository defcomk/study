/**
 * @file max9296_lib.h
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __MAX9296_LIB_H__
#define __MAX9296_LIB_H__

#include "sensor_lib.h"

#define DEBUG_SENSOR_NAME SENSOR_MODEL
#include "SensorDebug.h"

#define SENSOR_MODEL "max9296"

#define MSM_DES_0_SLAVEADDR           0x90
#define MSM_DES_1_SLAVEADDR           0x94

#define CAM_SER_BROADCAST_ADDR        0x8E

//Addresses after reprogramming the serializer and cameras attached to MAX9296
#define MSM_DES_0_ALIAS_ADDR_CAM_SER_0        0x82
#define MSM_DES_0_ALIAS_ADDR_CAM_SER_1        0x84
#define MSM_DES_1_ALIAS_ADDR_CAM_SER_0        0x8A
#define MSM_DES_1_ALIAS_ADDR_CAM_SER_1        0x8C

#define MSM_DES_0_ALIAS_ADDR_CAM_SNSR_0       0xE4
#define MSM_DES_0_ALIAS_ADDR_CAM_SNSR_1       0xE8
#define MSM_DES_1_ALIAS_ADDR_CAM_SNSR_0       0xEA
#define MSM_DES_1_ALIAS_ADDR_CAM_SNSR_1       0xEC

#define MSM_DES_CHIP_ID_REG_ADDR              0xD
#define MSM_DES_REVISION_REG_ADDR             0xE
#define MSM_DES_REVISION_ES2                  0xFF

#define MSM_SER_CHIP_ID_REG_ADDR              0xD
#define MSM_SER_REVISION_REG_ADDR             0XE
#define MSM_SER_REVISION_ES2                  0xFF

#define MAX_LINK_PIPELINES 2

#define MAXIM9296_INIT_ARRAY_SIZE 64

struct max9296_context_t;
typedef struct max9296_context_t max9296_context_t;

typedef enum
{
    MAXIM_LINK_A,
    MAXIM_LINK_B,
    MAXIM_LINK_MAX
}maxim_link_id_t;

typedef enum
{
    MAXIM_SENSOR_ID_INVALID,
    MAXIM_SENSOR_ID_AR0231,
    MAXIM_SENSOR_ID_AR0231_EXT_ISP,
    MAXIM_SENSOR_ID_MAX
}maxim_sensor_id_t;

typedef enum
{
    MAXIM_OP_MODE_DEFAULT,
    MAXIM_OP_MODE_RECEIVER,
    MAXIM_OP_MODE_BROADCAST,
}maxim_op_mode_t;


/* configuration structure which holds the config params, will be updated     *
 * upon parsing the user provided ".ini" file                                 */
typedef struct
{
    // topology section params
    maxim_op_mode_t opMode;

    maxim_sensor_id_t sensor_id[MAXIM_LINK_MAX];

    int num_of_cameras;

}max9296_topology_config_t;


typedef enum
{
    MAXIM_PIPELINE_X,
    MAXIM_PIPELINE_Y,
    MAXIM_PIPELINE_Z,
    MAXIM_PIPELINE_U,
    MAXIM_PIPELINE_MAX
}maxim_pipeline_id_t;

typedef struct
{
    maxim_pipeline_id_t id;
    img_src_mode_t      mode;
    unsigned int        stream_id;
}maxim_pipeline_t;

typedef struct
{
    maxim_pipeline_t pipelines[MAX_LINK_PIPELINES];
    uint8 num_pipelines; /*number of ser pipelines used*/
}max9296_link_cfg_t;

/**
 * MAXIM Slave Description
 */
typedef struct
{
    maxim_sensor_id_t id;

    int (*detect)(max9296_context_t* ctxt, uint32 link);
    int (*get_link_cfg)(max9296_context_t* ctxt, uint32 link, max9296_link_cfg_t* p_cfg);

    int (*init_link)(max9296_context_t* ctxt, uint32 link);
    int (*start_link)(max9296_context_t* ctxt, uint32 link);
    int (*stop_link)(max9296_context_t* ctxt, uint32 link);

    int (*calculate_exposure)(max9296_context_t* ctxt, uint32 link, sensor_exposure_info_t* exposure_info);
    int (*apply_exposure)(max9296_context_t* ctxt, uint32 link, sensor_exposure_info_t* exposure_info);

}max9296_sensor_t;

typedef enum
{
    SENSOR_STATE_INVALID = 0,
    SENSOR_STATE_SERIALIZER_DETECTED,
    SENSOR_STATE_DETECTED,
    SENSOR_STATE_INITIALIZED,
    SENSOR_STATE_STREAMING,
    SENSOR_STATE_UNSUPPORTED
}max9296_sensor_state_t;

/*Structure that holds information regarding sensor devices*/
typedef struct
{
    max9296_sensor_state_t state;
    unsigned int serializer_addr;
    unsigned int serializer_alias;
    unsigned int sensor_addr;
    unsigned int sensor_alias;

    max9296_link_cfg_t link_cfg;

    max9296_sensor_t*   sensor;
}max9296_sensor_info_t;

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

struct max9296_context_t
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
  unsigned int platform_fcn_tbl_initialized;
  sensor_platform_func_table_t platform_fcn_tbl;
  struct camera_i2c_reg_setting init_reg_settings;
  unsigned int rxport_en;

  struct camera_i2c_reg_setting max9296_reg_setting;
  struct camera_i2c_reg_array init_array[MAXIM9296_INIT_ARRAY_SIZE];

  max9296_sensor_info_t max9296_sensors[MAXIM_LINK_MAX];
  max9296_topology_config_t max9296_config;

  img_src_channel_info_t pipelines[MAXIM_PIPELINE_MAX];

  unsigned int subdev_id;
  void* ctrl;
};


/*CONFIGURATION OPTIONS*/
#define _max9296_delay_ 0
#define MAX9296_SELECT_LINK_DELAY 100000


#define CAM_DES_START \
{ \
    { 0x0002, 0x33, _max9296_delay_ }, \
}

#define CAM_DES_STOP \
{ \
    { 0x0002, 0x03, _max9296_delay_ }, \
}

//NOTE: 1 -> 12 disables backward propogation of I2C commands.
// We only do this in the case of receriver board in case its only connected to another board.
// If connected to camera this will need to be removed

#define CAM_DES_INIT_LINK_MODE_RECEIVER \
{ \
    { 0x0001, 0x12, _max9296_delay_ }, \
    { 0x0002, 0x03, _max9296_delay_ }, \
    { 0x031C, 0x20, _max9296_delay_ }, \
    { 0x031F, 0x20, _max9296_delay_ }, \
    { 0x0051, 0x01, _max9296_delay_ }, \
    { 0x0473, 0x02, _max9296_delay_ }, \
    { 0x0100, 0x23, _max9296_delay_ }, \
    { 0x0112, 0x23, _max9296_delay_ }, \
    { 0x0124, 0x23, _max9296_delay_ }, \
    { 0x0136, 0x23, _max9296_delay_ }, \
    { 0x0320, 0x2C, _max9296_delay_ }, \
}


#define CAM_DES_INIT_SPLITTER_MODE_RECEIVER \
{\
    { 0x0001, 0x12, _max9296_delay_ }, \
    { 0x0002, 0x03, _max9296_delay_ }, \
    { 0x031C, 0x60, _max9296_delay_ }, \
    { 0x031F, 0x60, _max9296_delay_ }, \
    { 0x0473, 0x02, _max9296_delay_ }, \
    { 0x04B3, 0x02, _max9296_delay_ }, \
    { 0x0100, 0x23, _max9296_delay_ }, \
    { 0x0112, 0x23, _max9296_delay_ }, \
    { 0x0124, 0x23, _max9296_delay_ }, \
    { 0x0136, 0x23, _max9296_delay_ }, \
    { 0x0320, 0x30, _max9296_delay_ }, \
    { 0x048B, 0x07, _max9296_delay_ }, \
    { 0x04AD, 0x15, _max9296_delay_ }, \
    { 0x048D, 0x6A, _max9296_delay_ }, \
    { 0x048E, 0x6A, _max9296_delay_ }, \
    { 0x048F, 0x40, _max9296_delay_ }, \
    { 0x0490, 0x40, _max9296_delay_ }, \
    { 0x0491, 0x41, _max9296_delay_ }, \
    { 0x0492, 0x41, _max9296_delay_ }, \
}

#define CAM_DES_INIT_SPLITTER_MODE_SENDER \
{ \
    { 0x0002, 0x03, _max9296_delay_ }, \
    { 0x0330, 0x04, _max9296_delay_ }, \
    { 0x0332, 0xF0, _max9296_delay_ }, \
    { 0x0050, 0x00, _max9296_delay_ }, \
    { 0x0051, 0x01, _max9296_delay_ }, \
    { 0x0052, 0x02, _max9296_delay_ }, \
    { 0x0053, 0x03, _max9296_delay_ }, \
    { 0x0314, 0x00, _max9296_delay_ }, \
    { 0x040B, 0x1F, _max9296_delay_ }, \
    { 0x042D, 0x55, _max9296_delay_ }, \
    { 0x042E, 0x01, _max9296_delay_ }, \
    { 0x044B, 0x1F, _max9296_delay_ }, \
    { 0x046D, 0x55, _max9296_delay_ }, \
    { 0x046E, 0x01, _max9296_delay_ }, \
    { 0x040F, 0x00, _max9296_delay_ }, \
    { 0x0410, 0x00, _max9296_delay_ }, \
    { 0x0411, 0x01, _max9296_delay_ }, \
    { 0x0412, 0x01, _max9296_delay_ }, \
    { 0x0413, 0x02, _max9296_delay_ }, \
    { 0x0414, 0x02, _max9296_delay_ }, \
    { 0x0415, 0x03, _max9296_delay_ }, \
    { 0x0416, 0x03, _max9296_delay_ }, \
    { 0x044F, 0x00, _max9296_delay_ }, \
    { 0x0450, 0x40, _max9296_delay_ }, \
    { 0x0451, 0x01, _max9296_delay_ }, \
    { 0x0452, 0x41, _max9296_delay_ }, \
    { 0x0453, 0x02, _max9296_delay_ }, \
    { 0x0454, 0x42, _max9296_delay_ }, \
    { 0x0455, 0x03, _max9296_delay_ }, \
    { 0x0456, 0x43, _max9296_delay_ }, \
    { 0x031D, 0xEC, _max9296_delay_ }, \
    { 0x0320, 0xEC, _max9296_delay_ }, \
    { 0x0100, 0x22, _max9296_delay_ }, \
    { 0x0112, 0x22, _max9296_delay_ }, \
    { 0x0100, 0x23, _max9296_delay_ }, \
    { 0x0112, 0x23, _max9296_delay_ }, \
}


// NOTE: 1 -> 12 disables backward propogation of I2C commands.
// We only do this in the case of receriver board in case its only connected to another board.
// If connected to camera this will need to be removed
#define CAM_DES_DISABLE_I2C_REVERSE \
{ \
    { 0x0001, 0x12, _max9296_delay_ }, \
}

#endif /* __MAX9296_LIB_H__ */
