/**
 * @file ov10635_lib.h
 *
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifndef __OV10635_H__
#define __OV10635_H__

#define SLAVE_IDENT_PID_REG 0x300A
#define SLAVE_IDENT_PID_ID 0xA6

#define _MODE_8_10           0x80  //0x80 8 bit 0x00 10 bit
#define _MODE_FPD3_INPUT_RAW 0x3   //RAW10 mode
#define _FV_POLARITY         0x1
#define _IMG_SRC_COLOR_FMT   QCARCAM_FMT_UYVY_8


#define SENSOR_I2C_DATA_TYPE CAMERA_I2C_BYTE_DATA
#define SENSOR_ID       0x60
#define SENSOR_WIDTH    1280
#define SENSOR_HEIGHT   720

#define _CAMTIMING_XOUT (SENSOR_WIDTH*2)
#define _CAMTIMING_YOUT SENSOR_HEIGHT
#define _CAMTIMING_CLKS_PER_LINE 3504*2
#define _CAMTIMING_TOTAL_LINES 804
#define _CAMTIMING_VT_CLK 79976160
#define _CAMTIMING_OP_CLK 79976160
#define _CAMTIMING_HTS 1782
#define _SCLK 96000000

#define _QGAIN 0x10

#define _ov_reg_delay_ 0

#define INIT_SLAVE(_rev_tx_) \
{ \
    { 0x0103, 0x01, _ov_reg_delay_ }, \
    { 0x301b, 0xff, _ov_reg_delay_ }, \
    { 0x301c, 0xff, _ov_reg_delay_ }, \
    { 0x301a, 0xff, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x300c, 0x61, _ov_reg_delay_ }, \
    { 0x3021, 0x03, _ov_reg_delay_ }, \
    { 0x3011, 0x02, _ov_reg_delay_ }, \
    { 0x6900, 0x0c, _ov_reg_delay_ }, \
    { 0x6901, 0x01, _ov_reg_delay_ }, \
    { 0x3033, 0x08, _ov_reg_delay_ }, \
    { 0x3503, 0x10, _ov_reg_delay_ }, \
    { 0x302d, 0x2f, _ov_reg_delay_ }, \
    { 0x3025, 0x03, _ov_reg_delay_ }, \
    { 0x3003, 0x20, _ov_reg_delay_ }, \
    { 0x3004, 0x03, _ov_reg_delay_ }, \
    { 0x3005, 0x20, _ov_reg_delay_ }, \
    { 0x3006, 0x91, _ov_reg_delay_ }, \
    { 0x3600, 0x74, _ov_reg_delay_ }, \
    { 0x3601, 0x2b, _ov_reg_delay_ }, \
    { 0x3612, 0x00, _ov_reg_delay_ }, \
    { 0x3611, 0x67, _ov_reg_delay_ }, \
    { 0x3633, 0xba, _ov_reg_delay_ }, \
    { 0x3602, 0x2f, _ov_reg_delay_ }, \
    { 0x3603, 0x00, _ov_reg_delay_ }, \
    { 0x3630, 0xa8, _ov_reg_delay_ }, \
    { 0x3631, 0x16, _ov_reg_delay_ }, \
    { 0x3714, 0x10, _ov_reg_delay_ }, \
    { 0x371d, 0x01, _ov_reg_delay_ }, \
    { 0x4300, 0x3a, _ov_reg_delay_ }, \
    { 0x3007, 0x01, _ov_reg_delay_ }, \
    { 0x3024, 0x01, _ov_reg_delay_ }, \
    { 0x3020, 0x0b, _ov_reg_delay_ }, \
    { 0x3702, 0x0d, _ov_reg_delay_ }, \
    { 0x3703, 0x20, _ov_reg_delay_ }, \
    { 0x3704, 0x15, _ov_reg_delay_ }, \
    { 0x3709, 0x28, _ov_reg_delay_ }, \
    { 0x370d, 0x00, _ov_reg_delay_ }, \
    { 0x3712, 0x00, _ov_reg_delay_ }, \
    { 0x3713, 0x20, _ov_reg_delay_ }, \
    { 0x3715, 0x04, _ov_reg_delay_ }, \
    { 0x381d, 0x43, _ov_reg_delay_ }, \
    { 0x6900, 0x01, _ov_reg_delay_ }, \
    { 0x381c, 0xC0, _ov_reg_delay_ }, \
    { 0x3824, 0x10, _ov_reg_delay_ }, \
    { 0x3815, 0x8c, _ov_reg_delay_ }, \
    { 0x3804, 0x05, _ov_reg_delay_ }, \
    { 0x3805, 0x1f, _ov_reg_delay_ }, \
    { 0x3800, 0x00, _ov_reg_delay_ }, \
    { 0x3801, 0x00, _ov_reg_delay_ }, \
    { 0x3806, 0x03, _ov_reg_delay_ }, \
    { 0x3807, 0x01, _ov_reg_delay_ }, \
    { 0x3802, 0x00, _ov_reg_delay_ }, \
    { 0x3803, 0x2e, _ov_reg_delay_ }, \
    { 0x3808, 0x05, _ov_reg_delay_ }, \
    { 0x3809, 0x00, _ov_reg_delay_ }, \
    { 0x380a, 0x02, _ov_reg_delay_ }, \
    { 0x380b, 0xd0, _ov_reg_delay_ }, \
    { 0x380c, 0x06, _ov_reg_delay_ }, \
    { 0x380d, 0xf6, _ov_reg_delay_ }, \
    { 0x380e, 0x03, _ov_reg_delay_ }, \
    { 0x380f, 0x80, _ov_reg_delay_ }, \
    { 0x3811, 0x10, _ov_reg_delay_ }, \
    { 0x381f, 0x0c, _ov_reg_delay_ }, \
    { 0x3621, 0x63, _ov_reg_delay_ }, \
    { 0x5005, 0x08, _ov_reg_delay_ }, \
    { 0x56d5, 0x00, _ov_reg_delay_ }, \
    { 0x56d6, 0x80, _ov_reg_delay_ }, \
    { 0x56d7, 0x00, _ov_reg_delay_ }, \
    { 0x56d8, 0x00, _ov_reg_delay_ }, \
    { 0x56d9, 0x00, _ov_reg_delay_ }, \
    { 0x56da, 0x80, _ov_reg_delay_ }, \
    { 0x56db, 0x00, _ov_reg_delay_ }, \
    { 0x56dc, 0x00, _ov_reg_delay_ }, \
    { 0x56e8, 0x00, _ov_reg_delay_ }, \
    { 0x56e9, 0x7f, _ov_reg_delay_ }, \
    { 0x56ea, 0x00, _ov_reg_delay_ }, \
    { 0x56eb, 0x7f, _ov_reg_delay_ }, \
    { 0x56d0, 0x00, _ov_reg_delay_ }, \
    { 0x5006, 0x24, _ov_reg_delay_ }, \
    { 0x5608, 0x00, _ov_reg_delay_ }, \
    { 0x52d7, 0x06, _ov_reg_delay_ }, \
    { 0x528d, 0x08, _ov_reg_delay_ }, \
    { 0x5293, 0x12, _ov_reg_delay_ }, \
    { 0x52d3, 0x12, _ov_reg_delay_ }, \
    { 0x5288, 0x06, _ov_reg_delay_ }, \
    { 0x5289, 0x20, _ov_reg_delay_ }, \
    { 0x52c8, 0x06, _ov_reg_delay_ }, \
    { 0x52c9, 0x20, _ov_reg_delay_ }, \
    { 0x52cd, 0x04, _ov_reg_delay_ }, \
    { 0x5381, 0x00, _ov_reg_delay_ }, \
    { 0x5382, 0xff, _ov_reg_delay_ }, \
    { 0x5651, 0x00, _ov_reg_delay_ }, \
    { 0x5652, 0x80, _ov_reg_delay_ }, \
    { 0x4605, 0x00, _ov_reg_delay_ }, \
    { 0x4606, 0x07, _ov_reg_delay_ }, \
    { 0x4607, 0x71, _ov_reg_delay_ }, \
    { 0x460a, 0x02, _ov_reg_delay_ }, \
    { 0x460b, 0x70, _ov_reg_delay_ }, \
    { 0x460c, 0x00, _ov_reg_delay_ }, \
    { 0x4620, 0x0e, _ov_reg_delay_ }, \
    { 0x4700, 0x04, _ov_reg_delay_ }, \
    { 0x4701, 0x01, _ov_reg_delay_ }, \
    { 0x4702, 0x00, _ov_reg_delay_ }, \
    { 0x4703, 0x00, _ov_reg_delay_ }, \
    { 0x4704, 0x00, _ov_reg_delay_ }, \
    { 0x4705, 0x00, _ov_reg_delay_ }, \
    { 0x4706, 0x00, _ov_reg_delay_ }, \
    { 0x4707, 0x00, _ov_reg_delay_ }, \
    { 0x4004, 0x08, _ov_reg_delay_ }, \
    { 0x4005, 0x18, _ov_reg_delay_ }, \
    { 0x4001, 0x04, _ov_reg_delay_ }, \
    { 0x4050, 0x20, _ov_reg_delay_ }, \
    { 0x4051, 0x22, _ov_reg_delay_ }, \
    { 0x4057, 0x9c, _ov_reg_delay_ }, \
    { 0x405a, 0x00, _ov_reg_delay_ }, \
    { 0x4202, 0x02, _ov_reg_delay_ }, \
    { 0x3023, 0x10, _ov_reg_delay_ }, \
    { 0x0100, 0x01, _ov_reg_delay_ }, \
    { 0x0100, 0x01, _ov_reg_delay_ }, \
    { 0x6f0e, 0x00, _ov_reg_delay_ }, \
    { 0x6f0f, 0x00, _ov_reg_delay_ }, \
    { 0x460e, 0x08, _ov_reg_delay_ }, \
    { 0x460f, 0x01, _ov_reg_delay_ }, \
    { 0x4610, 0x00, _ov_reg_delay_ }, \
    { 0x4611, 0x01, _ov_reg_delay_ }, \
    { 0x4612, 0x00, _ov_reg_delay_ }, \
    { 0x4613, 0x01, _ov_reg_delay_ }, \
    { 0x4605, _rev_tx_, _ov_reg_delay_ }, \
    { 0x4608, 0x00, _ov_reg_delay_ }, \
    { 0x4609, 0x08, _ov_reg_delay_ }, \
    { 0x6804, 0x00, _ov_reg_delay_ }, \
    { 0x6805, 0x06, _ov_reg_delay_ }, \
    { 0x6806, 0x00, _ov_reg_delay_ }, \
    { 0x5120, 0x00, _ov_reg_delay_ }, \
    { 0x3510, 0x00, _ov_reg_delay_ }, \
    { 0x3504, 0x00, _ov_reg_delay_ }, \
    { 0x6800, 0x00, _ov_reg_delay_ }, \
    { 0x6f0d, 0x00, _ov_reg_delay_ }, \
    { 0x5000, 0xff, _ov_reg_delay_ }, \
    { 0x5001, 0xbf, _ov_reg_delay_ }, \
    { 0x5002, 0xfe, _ov_reg_delay_ }, \
    { 0x503d, 0x00, _ov_reg_delay_ }, \
    { 0xc450, 0x01, _ov_reg_delay_ }, \
    { 0xc452, 0x04, _ov_reg_delay_ }, \
    { 0xc453, 0x00, _ov_reg_delay_ }, \
    { 0xc454, 0x00, _ov_reg_delay_ }, \
    { 0xc455, 0x00, _ov_reg_delay_ }, \
    { 0xc456, 0x00, _ov_reg_delay_ }, \
    { 0xc457, 0x00, _ov_reg_delay_ }, \
    { 0xc458, 0x00, _ov_reg_delay_ }, \
    { 0xc459, 0x00, _ov_reg_delay_ }, \
    { 0xc45b, 0x00, _ov_reg_delay_ }, \
    { 0xc45c, 0x00, _ov_reg_delay_ }, \
    { 0xc45d, 0x00, _ov_reg_delay_ }, \
    { 0xc45e, 0x00, _ov_reg_delay_ }, \
    { 0xc45f, 0x00, _ov_reg_delay_ }, \
    { 0xc460, 0x00, _ov_reg_delay_ }, \
    { 0xc461, 0x01, _ov_reg_delay_ }, \
    { 0xc462, 0x01, _ov_reg_delay_ }, \
    { 0xc464, 0x88, _ov_reg_delay_ }, \
    { 0xc465, 0x00, _ov_reg_delay_ }, \
    { 0xc466, 0x8a, _ov_reg_delay_ }, \
    { 0xc467, 0x00, _ov_reg_delay_ }, \
    { 0xc468, 0x86, _ov_reg_delay_ }, \
    { 0xc469, 0x00, _ov_reg_delay_ }, \
    { 0xc46a, 0x40, _ov_reg_delay_ }, \
    { 0xc46b, 0x50, _ov_reg_delay_ }, \
    { 0xc46c, 0x30, _ov_reg_delay_ }, \
    { 0xc46d, 0x28, _ov_reg_delay_ }, \
    { 0xc46e, 0x60, _ov_reg_delay_ }, \
    { 0xc46f, 0x40, _ov_reg_delay_ }, \
    { 0xc47c, 0x01, _ov_reg_delay_ }, \
    { 0xc47d, 0x38, _ov_reg_delay_ }, \
    { 0xc47e, 0x00, _ov_reg_delay_ }, \
    { 0xc47f, 0x00, _ov_reg_delay_ }, \
    { 0xc480, 0x00, _ov_reg_delay_ }, \
    { 0xc481, 0xff, _ov_reg_delay_ }, \
    { 0xc482, 0x00, _ov_reg_delay_ }, \
    { 0xc483, 0x40, _ov_reg_delay_ }, \
    { 0xc484, 0x00, _ov_reg_delay_ }, \
    { 0xc485, 0x18, _ov_reg_delay_ }, \
    { 0xc486, 0x00, _ov_reg_delay_ }, \
    { 0xc487, 0x18, _ov_reg_delay_ }, \
    { 0xc488, 0x2e, _ov_reg_delay_ }, \
    { 0xc489, 0x80, _ov_reg_delay_ }, \
    { 0xc48a, 0x2e, _ov_reg_delay_ }, \
    { 0xc48b, 0x80, _ov_reg_delay_ }, \
    { 0xc48c, 0x00, _ov_reg_delay_ }, \
    { 0xc48d, 0x04, _ov_reg_delay_ }, \
    { 0xc48e, 0x00, _ov_reg_delay_ }, \
    { 0xc48f, 0x04, _ov_reg_delay_ }, \
    { 0xc490, 0x07, _ov_reg_delay_ }, \
    { 0xc492, 0x20, _ov_reg_delay_ }, \
    { 0xc493, 0x08, _ov_reg_delay_ }, \
    { 0xc498, 0x02, _ov_reg_delay_ }, \
    { 0xc499, 0x00, _ov_reg_delay_ }, \
    { 0xc49a, 0x02, _ov_reg_delay_ }, \
    { 0xc49b, 0x00, _ov_reg_delay_ }, \
    { 0xc49c, 0x02, _ov_reg_delay_ }, \
    { 0xc49d, 0x00, _ov_reg_delay_ }, \
    { 0xc49e, 0x02, _ov_reg_delay_ }, \
    { 0xc49f, 0x60, _ov_reg_delay_ }, \
    { 0xc4a0, 0x04, _ov_reg_delay_ }, \
    { 0xc4a1, 0x00, _ov_reg_delay_ }, \
    { 0xc4a2, 0x06, _ov_reg_delay_ }, \
    { 0xc4a3, 0x00, _ov_reg_delay_ }, \
    { 0xc4a4, 0x00, _ov_reg_delay_ }, \
    { 0xc4a5, 0x10, _ov_reg_delay_ }, \
    { 0xc4a6, 0x00, _ov_reg_delay_ }, \
    { 0xc4a7, 0x40, _ov_reg_delay_ }, \
    { 0xc4a8, 0x00, _ov_reg_delay_ }, \
    { 0xc4a9, 0x80, _ov_reg_delay_ }, \
    { 0xc4aa, 0x0d, _ov_reg_delay_ }, \
    { 0xc4ab, 0x00, _ov_reg_delay_ }, \
    { 0xc4ac, 0x0f, _ov_reg_delay_ }, \
    { 0xc4ad, 0xc0, _ov_reg_delay_ }, \
    { 0xc4b4, 0x01, _ov_reg_delay_ }, \
    { 0xc4b5, 0x01, _ov_reg_delay_ }, \
    { 0xc4b6, 0x00, _ov_reg_delay_ }, \
    { 0xc4b7, 0x01, _ov_reg_delay_ }, \
    { 0xc4b8, 0x00, _ov_reg_delay_ }, \
    { 0xc4b9, 0x01, _ov_reg_delay_ }, \
    { 0xc4ba, 0x01, _ov_reg_delay_ }, \
    { 0xc4bb, 0x00, _ov_reg_delay_ }, \
    { 0xc4be, 0x02, _ov_reg_delay_ }, \
    { 0xc4bf, 0x33, _ov_reg_delay_ }, \
    { 0xc4c8, 0x03, _ov_reg_delay_ }, \
    { 0xc4c9, 0xd0, _ov_reg_delay_ }, \
    { 0xc4ca, 0x0e, _ov_reg_delay_ }, \
    { 0xc4cb, 0x00, _ov_reg_delay_ }, \
    { 0xc4cc, 0x0e, _ov_reg_delay_ }, \
    { 0xc4cd, 0x51, _ov_reg_delay_ }, \
    { 0xc4ce, 0x0e, _ov_reg_delay_ }, \
    { 0xc4cf, 0x51, _ov_reg_delay_ }, \
    { 0xc4d0, 0x04, _ov_reg_delay_ }, \
    { 0xc4d1, 0x80, _ov_reg_delay_ }, \
    { 0xc4e0, 0x04, _ov_reg_delay_ }, \
    { 0xc4e1, 0x02, _ov_reg_delay_ }, \
    { 0xc4e2, 0x01, _ov_reg_delay_ }, \
    { 0xc4e4, 0x10, _ov_reg_delay_ }, \
    { 0xc4e5, 0x20, _ov_reg_delay_ }, \
    { 0xc4e6, 0x30, _ov_reg_delay_ }, \
    { 0xc4e7, 0x40, _ov_reg_delay_ }, \
    { 0xc4e8, 0x50, _ov_reg_delay_ }, \
    { 0xc4e9, 0x60, _ov_reg_delay_ }, \
    { 0xc4ea, 0x70, _ov_reg_delay_ }, \
    { 0xc4eb, 0x80, _ov_reg_delay_ }, \
    { 0xc4ec, 0x90, _ov_reg_delay_ }, \
    { 0xc4ed, 0xa0, _ov_reg_delay_ }, \
    { 0xc4ee, 0xb0, _ov_reg_delay_ }, \
    { 0xc4ef, 0xc0, _ov_reg_delay_ }, \
    { 0xc4f0, 0xd0, _ov_reg_delay_ }, \
    { 0xc4f1, 0xe0, _ov_reg_delay_ }, \
    { 0xc4f2, 0xf0, _ov_reg_delay_ }, \
    { 0xc4f3, 0x80, _ov_reg_delay_ }, \
    { 0xc4f4, 0x00, _ov_reg_delay_ }, \
    { 0xc4f5, 0x20, _ov_reg_delay_ }, \
    { 0xc4f6, 0x02, _ov_reg_delay_ }, \
    { 0xc4f7, 0x00, _ov_reg_delay_ }, \
    { 0xc4f8, 0x04, _ov_reg_delay_ }, \
    { 0xc4f9, 0x0b, _ov_reg_delay_ }, \
    { 0xc4fa, 0x00, _ov_reg_delay_ }, \
    { 0xc4fb, 0x01, _ov_reg_delay_ }, \
    { 0xc4fc, 0x01, _ov_reg_delay_ }, \
    { 0xc4fd, 0x01, _ov_reg_delay_ }, \
    { 0xc4fe, 0x04, _ov_reg_delay_ }, \
    { 0xc4ff, 0x02, _ov_reg_delay_ }, \
    { 0xc500, 0x68, _ov_reg_delay_ }, \
    { 0xc501, 0x74, _ov_reg_delay_ }, \
    { 0xc502, 0x70, _ov_reg_delay_ }, \
    { 0xc503, 0x80, _ov_reg_delay_ }, \
    { 0xc504, 0x05, _ov_reg_delay_ }, \
    { 0xc505, 0x80, _ov_reg_delay_ }, \
    { 0xc506, 0x03, _ov_reg_delay_ }, \
    { 0xc507, 0x80, _ov_reg_delay_ }, \
    { 0xc508, 0x01, _ov_reg_delay_ }, \
    { 0xc509, 0xc0, _ov_reg_delay_ }, \
    { 0xc50a, 0x01, _ov_reg_delay_ }, \
    { 0xc50b, 0xa0, _ov_reg_delay_ }, \
    { 0xc50c, 0x01, _ov_reg_delay_ }, \
    { 0xc50d, 0x2c, _ov_reg_delay_ }, \
    { 0xc50e, 0x01, _ov_reg_delay_ }, \
    { 0xc50f, 0x0a, _ov_reg_delay_ }, \
    { 0xc510, 0x00, _ov_reg_delay_ }, \
    { 0xc511, 0x00, _ov_reg_delay_ }, \
    { 0xc512, 0xe5, _ov_reg_delay_ }, \
    { 0xc513, 0x14, _ov_reg_delay_ }, \
    { 0xc514, 0x04, _ov_reg_delay_ }, \
    { 0xc515, 0x00, _ov_reg_delay_ }, \
    { 0xc518, 0x03, _ov_reg_delay_ }, \
    { 0xc519, 0x48, _ov_reg_delay_ }, \
    { 0xc51a, 0x07, _ov_reg_delay_ }, \
    { 0xc51b, 0x70, _ov_reg_delay_ }, \
    { 0xc2e0, 0x00, _ov_reg_delay_ }, \
    { 0xc2e1, 0x51, _ov_reg_delay_ }, \
    { 0xc2e2, 0x00, _ov_reg_delay_ }, \
    { 0xc2e3, 0xd6, _ov_reg_delay_ }, \
    { 0xc2e4, 0x01, _ov_reg_delay_ }, \
    { 0xc2e5, 0x5e, _ov_reg_delay_ }, \
    { 0xc2e9, 0x01, _ov_reg_delay_ }, \
    { 0xc2ea, 0x7a, _ov_reg_delay_ }, \
    { 0xc2eb, 0x90, _ov_reg_delay_ }, \
    { 0xc2ed, 0x01, _ov_reg_delay_ }, \
    { 0xc2ee, 0x7a, _ov_reg_delay_ }, \
    { 0xc2ef, 0x64, _ov_reg_delay_ }, \
    { 0xc308, 0x00, _ov_reg_delay_ }, \
    { 0xc309, 0x00, _ov_reg_delay_ }, \
    { 0xc30a, 0x00, _ov_reg_delay_ }, \
    { 0xc30c, 0x00, _ov_reg_delay_ }, \
    { 0xc30d, 0x01, _ov_reg_delay_ }, \
    { 0xc30e, 0x00, _ov_reg_delay_ }, \
    { 0xc30f, 0x00, _ov_reg_delay_ }, \
    { 0xc310, 0x01, _ov_reg_delay_ }, \
    { 0xc311, 0x60, _ov_reg_delay_ }, \
    { 0xc312, 0xff, _ov_reg_delay_ }, \
    { 0xc313, 0x08, _ov_reg_delay_ }, \
    { 0xc314, 0x01, _ov_reg_delay_ }, \
    { 0xc315, 0x7f, _ov_reg_delay_ }, \
    { 0xc316, 0xff, _ov_reg_delay_ }, \
    { 0xc317, 0x0b, _ov_reg_delay_ }, \
    { 0xc318, 0x00, _ov_reg_delay_ }, \
    { 0xc319, 0x0c, _ov_reg_delay_ }, \
    { 0xc31a, 0x00, _ov_reg_delay_ }, \
    { 0xc31b, 0xe0, _ov_reg_delay_ }, \
    { 0xc31c, 0x00, _ov_reg_delay_ }, \
    { 0xc31d, 0x14, _ov_reg_delay_ }, \
    { 0xc31e, 0x00, _ov_reg_delay_ }, \
    { 0xc31f, 0xc5, _ov_reg_delay_ }, \
    { 0xc320, 0xff, _ov_reg_delay_ }, \
    { 0xc321, 0x4b, _ov_reg_delay_ }, \
    { 0xc322, 0xff, _ov_reg_delay_ }, \
    { 0xc323, 0xf0, _ov_reg_delay_ }, \
    { 0xc324, 0xff, _ov_reg_delay_ }, \
    { 0xc325, 0xe8, _ov_reg_delay_ }, \
    { 0xc326, 0x00, _ov_reg_delay_ }, \
    { 0xc327, 0x46, _ov_reg_delay_ }, \
    { 0xc328, 0xff, _ov_reg_delay_ }, \
    { 0xc329, 0xd2, _ov_reg_delay_ }, \
    { 0xc32a, 0xff, _ov_reg_delay_ }, \
    { 0xc32b, 0xe4, _ov_reg_delay_ }, \
    { 0xc32c, 0xff, _ov_reg_delay_ }, \
    { 0xc32d, 0xbb, _ov_reg_delay_ }, \
    { 0xc32e, 0x00, _ov_reg_delay_ }, \
    { 0xc32f, 0x61, _ov_reg_delay_ }, \
    { 0xc330, 0xff, _ov_reg_delay_ }, \
    { 0xc331, 0xf9, _ov_reg_delay_ }, \
    { 0xc332, 0x00, _ov_reg_delay_ }, \
    { 0xc333, 0xd9, _ov_reg_delay_ }, \
    { 0xc334, 0x00, _ov_reg_delay_ }, \
    { 0xc335, 0x2e, _ov_reg_delay_ }, \
    { 0xc336, 0x00, _ov_reg_delay_ }, \
    { 0xc337, 0xb1, _ov_reg_delay_ }, \
    { 0xc338, 0xff, _ov_reg_delay_ }, \
    { 0xc339, 0x64, _ov_reg_delay_ }, \
    { 0xc33a, 0xff, _ov_reg_delay_ }, \
    { 0xc33b, 0xeb, _ov_reg_delay_ }, \
    { 0xc33c, 0xff, _ov_reg_delay_ }, \
    { 0xc33d, 0xe8, _ov_reg_delay_ }, \
    { 0xc33e, 0x00, _ov_reg_delay_ }, \
    { 0xc33f, 0x48, _ov_reg_delay_ }, \
    { 0xc340, 0xff, _ov_reg_delay_ }, \
    { 0xc341, 0xd0, _ov_reg_delay_ }, \
    { 0xc342, 0xff, _ov_reg_delay_ }, \
    { 0xc343, 0xed, _ov_reg_delay_ }, \
    { 0xc344, 0xff, _ov_reg_delay_ }, \
    { 0xc345, 0xad, _ov_reg_delay_ }, \
    { 0xc346, 0x00, _ov_reg_delay_ }, \
    { 0xc347, 0x66, _ov_reg_delay_ }, \
    { 0xc348, 0x01, _ov_reg_delay_ }, \
    { 0xc349, 0x00, _ov_reg_delay_ }, \
    { 0x6700, 0x04, _ov_reg_delay_ }, \
    { 0x6701, 0x7b, _ov_reg_delay_ }, \
    { 0x6702, 0xfd, _ov_reg_delay_ }, \
    { 0x6703, 0xf9, _ov_reg_delay_ }, \
    { 0x6704, 0x3d, _ov_reg_delay_ }, \
    { 0x6705, 0x71, _ov_reg_delay_ }, \
    { 0x6706, 0x78, _ov_reg_delay_ }, \
    { 0x6708, 0x05, _ov_reg_delay_ }, \
    { 0x3822, 0x50, _ov_reg_delay_ }, \
    { 0x6f06, 0x6f, _ov_reg_delay_ }, \
    { 0x6f07, 0x00, _ov_reg_delay_ }, \
    { 0x6f0a, 0x6f, _ov_reg_delay_ }, \
    { 0x6f0b, 0x00, _ov_reg_delay_ }, \
    { 0x6f00, 0x03, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x3042, 0xf0, _ov_reg_delay_ }, \
    { 0x301b, 0xf0, _ov_reg_delay_ }, \
    { 0x301c, 0xf0, _ov_reg_delay_ }, \
    { 0x301a, 0xf0, _ov_reg_delay_ }, \
}

#define START_SLAVE \
{ \
}

#define STOP_SLAVE \
{ \
}

#define ENABLE_MANUAL_EXP \
{ \
    { 0xC2F0, 0x34, _ov_reg_delay_ }, \
    { 0xC2F1, 0x70, _ov_reg_delay_ }, \
    { 0xC2F2, 0x34, _ov_reg_delay_ }, \
    { 0xC2F3, 0x70, _ov_reg_delay_ }, \
    { 0xC2FC, 0x00, _ov_reg_delay_ }, \
    { 0xC2FD, 0x18, _ov_reg_delay_ }, \
    { 0xC2FE, 0x00, _ov_reg_delay_ }, \
    { 0xC2FF, 0x18, _ov_reg_delay_ }, \
    { 0xC450, 0x01, _ov_reg_delay_ }, \
    { 0xC454, 0x01, _ov_reg_delay_ }, \
    { 0xC30A, 0x02, _ov_reg_delay_ }, \
    { 0xC309, 0x01, _ov_reg_delay_ }, \
    { 0xC308, 0x01, _ov_reg_delay_ }, \
}

#define ENABLE_AUTO_EXP \
{ \
    { 0xC308, 0x00, _ov_reg_delay_ }, \
}

#endif /* __OV10635_H__ */
