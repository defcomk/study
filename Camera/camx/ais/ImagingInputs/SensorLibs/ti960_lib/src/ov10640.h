/**
 * @file ov10640_lib.h
 *
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifndef __OV10640_H__
#define __OV10640_H__

#define SLAVE_IDENT_PID_REG 0x300A
#define SLAVE_IDENT_PID_ID 0x4

#define _MODE_8_10           0x80  //0x80 8 bit 0x00 10 bit
#define _MODE_FPD3_INPUT_RAW 0x3   //RAW10 mode
#define _FV_POLARITY         0x0
#define _IMG_SRC_COLOR_FMT   QCARCAM_FMT_UYVY_8

#define SENSOR_I2C_DATA_TYPE CAMERA_I2C_BYTE_DATA
#define SENSOR_ID       0x48
#define SENSOR_WIDTH    1280
#define SENSOR_HEIGHT   1086

#define _CAMTIMING_XOUT (SENSOR_WIDTH*2)
#define _CAMTIMING_YOUT SENSOR_HEIGHT
#define _CAMTIMING_CLKS_PER_LINE 3504*2
#define _CAMTIMING_TOTAL_LINES 804
#define _CAMTIMING_VT_CLK 79976160
#define _CAMTIMING_OP_CLK 79976160
#define _CAMTIMING_HTS 1782
#define _SCLK 96000000
#define _QGAIN 0x20
#define ov_reg_delay 10

#define OV_INDIRECT_HI 0xfffd
#define OV_INDIRECT_LO 0xfffe

#define INIT_SLAVE(_rev_tx_) \
{ \
}

#define START_SLAVE \
{ \
    { OV_INDIRECT_HI , 0x80, ov_reg_delay }, \
    { OV_INDIRECT_LO , 0x29, ov_reg_delay }, \
    { 0x6010 , 0x1, ov_reg_delay }, \
}

#define STOP_SLAVE \
{ \
}

#define ENABLE_MANUAL_EXP \
{ \
}

#define ENABLE_AUTO_EXP \
{ \
}

#endif /* __OV10640_H__ */
