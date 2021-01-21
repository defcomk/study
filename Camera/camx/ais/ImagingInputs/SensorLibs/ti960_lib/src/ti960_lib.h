/**
 * @file ti960_lib.h
 *
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __TI960_LIB_H__
#define __TI960_LIB_H__

#include "sensor_lib.h"

#if defined SENSOR_OV10635
#include "ov10635.h"
#define SENSOR_MODEL "ti960_ov10635"
#elif defined SENSOR_OV10640
#include "ov10640.h"
#define SENSOR_MODEL "ti960_ov10640"
#elif defined SENSOR_AR0140
#include "ar0140.h"
#define SENSOR_MODEL "ti960_ar0140"
#elif defined SENSOR_AR0143
/*Note: NEED 12V Supply*/
#include "ar0143.h"
#define SENSOR_MODEL "ti960_ar0143"
#else
#error "ti960 error UNDEFINED SENSOR"
#endif

/*CONFIGURATION OPTIONS*/
//#define CSI_TRANSMISSION_1_6GBPS
#define CSI_TRANSMISSION_800MBPS
#define OV10635_RES_720P30_TI

// Rev 1 is TI960 rev 2 is TI964
#define TI960_REV_ADDR 0x03
#define TI960_REV_ID_1 0x10
#define TI960_REV_ID_2 0x20
#define TI960_REV_ID_3 0x30

/*Settle count for 800Mbps/1.6Gbps*/
#define SETTLE_COUNT_1_6GBPS 0x14
#define SETTLE_COUNT_800MBPS 0x1A

#ifdef CSI_TRANSMISSION_1_6GBPS
#define SETTLE_COUNT SETTLE_COUNT_1_6GBPS
#else
#define SETTLE_COUNT SETTLE_COUNT_800MBPS
#endif

/*CSI start setting for 800Mbps/1.6Gbps*/
#define CSI_1_6GBPS 0x4
#define CSI_800MBPS_REV_1 0x5
#define CSI_800MBPS_REV_2 0x6

#define _reg_delay_ 0
#define _start_delay_ 5000

#define DT_960 CSI_DT_YUV422_8
#define DT_BITDEPTH CSI_DECODE_8BIT

#define RXPORT_EN_RX0  0xe0
#define RXPORT_EN_RX1  0xd0
#define RXPORT_EN_RX2  0xb0
#define RXPORT_EN_RX3  0x70
#define RXPORT_EN_ALL  0x0

#define RXPORT_EN_NONE 0xf0

#define TI_GPIO_UP 0x8
#define TI_GPIO_DWN 0x9

#define IMI_GPIO_UP 0x9
#define IMI_GPIO_DWN 0x8

#define CID_VC0        0
#define CID_VC1        4
#define CID_VC2        8
#define CID_VC3        12

#define VC0 0
#define VC1 (1 << 6)
#define VC2 (2 << 6)
#define VC3 (3 << 6)


#define TI960_REV_TX_10BIT 0x08
#define TI960_REV_TX_8BIT 0x00

#define INIT_SERIALIZER \
{ \
    { 0x11, 0x32, _reg_delay_ }, \
    { 0x12, 0x32, _reg_delay_ }, \
}

#define CAMERA_POWERUP_TI() \
  { 0x6e, 0x08, _start_delay_ },

#define CAMERA_POWERDOWN_TI() \
  { 0x6e, 0x09, _start_delay_ },

#define CAMERA_POWERUP_IMI() \
  { 0x6e, 0x08, _start_delay_ }, \
  { 0x6e, 0x09, _start_delay_ },

#define CAMERA_POWERDOWN_IMI() \
  { 0x6e, 0x08, _start_delay_ },

#define CAMERA_POWERUP(_gpio_up_, _gpio_down_) \
  { 0x6e, _gpio_down_, _start_delay_ }, \
  { 0x6e, _gpio_up_, _start_delay_ }, \


/*disable VIDEO_FREEZE and 1 frame threshold*/
#define PORT_PASS_CTL_VAL 0xb9

#define INIT_INTR_ARRAY \
{ \
  { 0x4c, 0x0f, 0 }, \
  { 0xd8, 0x00, 0 }, \
  { 0xd9, 0x73, 0 }, \
  { 0x7d, PORT_PASS_CTL_VAL, 0 }, \
  { 0x23, 0x8f, 0 }, \
}

#define START_REG_ARRAY \
{ \
  { 0xb0, 0x1c, 0 }, \
  { 0xb1, 0x13, 0 }, \
  { 0xb2, 0x1f, 0 }, \
  { 0x33, 0x03, 0 }, \
  { 0x20, RXPORT_EN_PORTS, 0 }, \
}

#define STOP_REG_ARRAY \
{ \
  { 0x20, RXPORT_EN_NONE, 0 }, \
  { 0x33, 0x2, 0 }, \
}


#define NUM_SUPPORT_SENSORS 4
#define RXPORT_EN_PORTS (RXPORT_EN_ALL)
#define PWR_GPIO_UP IMI_GPIO_UP
#define PWR_GPIO_DOWN IMI_GPIO_DWN

#define TI960_0_SLAVEADDR 0x7a
#define TI960_0_SENSOR_ALIAS_GROUP 0x94
#define TI960_0_SENSOR_ALIAS0 0x86
#define TI960_0_SENSOR_ALIAS1 0x88
#define TI960_0_SENSOR_ALIAS2 0x90
#define TI960_0_SENSOR_ALIAS3 0x92
#define TI960_0_SERIALIZER_ALIAS_RX0 0x20
#define TI960_0_SERIALIZER_ALIAS_RX1 0x22
#define TI960_0_SERIALIZER_ALIAS_RX2 0x24
#define TI960_0_SERIALIZER_ALIAS_RX3 0x26

#define TI960_1_SLAVEADDR 0x60
#define TI960_1_SENSOR_ALIAS_GROUP 0x56
#define TI960_1_SENSOR_ALIAS0 0x48
#define TI960_1_SENSOR_ALIAS1 0x50
#define TI960_1_SENSOR_ALIAS2 0x52
#define TI960_1_SENSOR_ALIAS3 0x54
#define TI960_1_SERIALIZER_ALIAS_RX0 0x18
#define TI960_1_SERIALIZER_ALIAS_RX1 0x1a
#define TI960_1_SERIALIZER_ALIAS_RX2 0x1c
#define TI960_1_SERIALIZER_ALIAS_RX3 0x1e

#define DEBUG_SENSOR_NAME SENSOR_MODEL



/*Slave APIs*/
int slave_calculate_exposure(sensor_exposure_info_t* exposure_info);
int slave_get_exposure_config(struct camera_i2c_reg_setting* ar0143_reg, sensor_exposure_info_t* exposure_info);

#endif /* __TI960_LIB_H__ */
