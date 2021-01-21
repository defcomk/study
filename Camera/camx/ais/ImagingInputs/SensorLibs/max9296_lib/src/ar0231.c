/**
 * @file ar0231.c
 *
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ar0231.h"

/*CONFIGURATION OPTIONS*/

#define SENSOR_WIDTH    1928
#define SENSOR_HEIGHT   1208

#define _CAMTIMING_XOUT SENSOR_WIDTH
#define _CAMTIMING_YOUT SENSOR_HEIGHT
#define _CAMTIMING_CLKS_PER_LINE SENSOR_WIDTH
#define _CAMTIMING_TOTAL_LINES SENSOR_HEIGHT
#define _CAMTIMING_VT_CLK 79976160
#define _CAMTIMING_OP_CLK 79976160

#define AR0231_PCLK 99000000 /*99MHz*/
#define AR0231_LINE_LENGTH_PCK 1499 /*0x0898*/
#define AR0231_FRAME_LENGTH_LINES 2200 /*0x0A8C*/
#define AR0231_EXTRA_DELAY 0x11F0
#define X_ADDR_START 0
#define X_ADDR_END   0x787
#define Y_ADDR_START 0x0
#define Y_ADDR_END   0x4B7

#define COARSE_INTEGRATION_TIME 0x0288
#define FINE_INTEGRATION_TIME 0x032A

#define _ar0231_delay_ 0
#define MAX9295_LINK_RESET_DELAY 25000
#define MAX9295_START_DELAY 20000

#define CAM_SENSOR_DEFAULT_ADDR    0x20

#define FMT_LINK    QCARCAM_FMT_MIPIRAW_12
#define DT_LINK     CSI_DT_RAW12

#define CID_VC0        0
#define CID_VC1        4


#define FLOAT_TO_Q(exp, f) \
  ((int32_t)((f*(1<<(exp))) + ((f<0) ? -0.5 : 0.5)))

#define AR0231_PACK_ANALOG_GAIN(_g_) \
    (((_g_) << 12) | ((_g_) << 8) | ((_g_) << 4) | (_g_))

/* Difference between link A and link B is the stream ID set in register 0x53 */
#define CAM_SER_INIT_A \
{ \
    { 0x0002, 0x03, _ar0231_delay_ }, \
    { 0x0330, 0x00, _ar0231_delay_ }, \
    { 0x0332, 0xEE, _ar0231_delay_ }, \
    { 0x0333, 0xE4, _ar0231_delay_ }, \
    { 0x0331, 0x33, _ar0231_delay_ }, \
    { 0x0311, 0x10, _ar0231_delay_ }, \
    { 0x0308, 0x7F, _ar0231_delay_ }, \
    { 0x0314, 0x6C, _ar0231_delay_ }, \
    { 0x02BE, 0x90, _ar0231_delay_ }, /*set sensor_rst GPIO*/ \
    { 0x03F1, 0x89, _ar0231_delay_ } \
}

#define CAM_SER_INIT_B \
{ \
    { 0x0002, 0x03, _ar0231_delay_ }, \
    { 0x0053, 0x01, _ar0231_delay_ }, \
    { 0x0330, 0x00, _ar0231_delay_ }, \
    { 0x0332, 0xEE, _ar0231_delay_ }, \
    { 0x0333, 0xE4, _ar0231_delay_ }, \
    { 0x0331, 0x33, _ar0231_delay_ }, \
    { 0x0311, 0x10, _ar0231_delay_ }, \
    { 0x0308, 0x7F, _ar0231_delay_ }, \
    { 0x0314, 0x6C, _ar0231_delay_ }, \
    { 0x02BE, 0x90, _ar0231_delay_ }, /*set sensor_rst GPIO*/ \
    { 0x03F1, 0x89, _ar0231_delay_ } \
}


#define CAM_SER_START \
{ \
    { 0x0002, 0xF3, MAX9295_START_DELAY }, \
}

#define CAM_SER_STOP \
{ \
    { 0x0002, 0x03, MAX9295_START_DELAY }, \
}

#define CAM_SER_ADDR_CHNG_A \
{ \
    { 0x006B, 0x10, _max9296_delay_ }, \
    { 0x0073, 0x11, _max9296_delay_ }, \
    { 0x007B, 0x30, _max9296_delay_ }, \
    { 0x0083, 0x30, _max9296_delay_ }, \
    { 0x0093, 0x30, _max9296_delay_ }, \
    { 0x009B, 0x30, _max9296_delay_ }, \
    { 0x00A3, 0x30, _max9296_delay_ }, \
    { 0x00AB, 0x30, _max9296_delay_ }, \
    { 0x008B, 0x30, _max9296_delay_ }, \
}

#define CAM_SER_ADDR_CHNG_B \
{ \
    { 0x006B, 0x12, _max9296_delay_ }, \
    { 0x0073, 0x13, _max9296_delay_ }, \
    { 0x007B, 0x32, _max9296_delay_ }, \
    { 0x0083, 0x32, _max9296_delay_ }, \
    { 0x0093, 0x32, _max9296_delay_ }, \
    { 0x009B, 0x32, _max9296_delay_ }, \
    { 0x00A3, 0x32, _max9296_delay_ }, \
    { 0x00AB, 0x32, _max9296_delay_ }, \
    { 0x008B, 0x32, _max9296_delay_ }, \
}

#define CAM_SNSR_INIT \
{ \
    { 0x301A, 0x0018, 100 },         \
    { 0x3056, 0x0080, _ar0231_delay_ }, \
    { 0x3058, 0x0080, _ar0231_delay_ }, \
    { 0x305A, 0x0080, _ar0231_delay_ }, \
    { 0x305C, 0x0080, _ar0231_delay_ }, \
    { 0x3138, 0x000B, _ar0231_delay_ }, \
    { 0x30FE, 0x0020, _ar0231_delay_ }, \
    { 0x3372, 0xF54F, _ar0231_delay_ }, \
    { 0x337A, 0x0D70, _ar0231_delay_ }, \
    { 0x337E, 0x1FFD, _ar0231_delay_ }, \
    { 0x3382, 0x00C0, _ar0231_delay_ }, \
    { 0x3092, 0x0024, _ar0231_delay_ }, \
    { 0x3C04, 0x0E80, _ar0231_delay_ }, \
    { 0x3F90, 0x06E1, _ar0231_delay_ }, \
    { 0x3F92, 0x06E1, _ar0231_delay_ }, \
    { 0x350E, 0xFF14, _ar0231_delay_ }, \
    { 0x3506, 0x4444, _ar0231_delay_ }, \
    { 0x3508, 0x4444, _ar0231_delay_ }, \
    { 0x350A, 0x4465, _ar0231_delay_ }, \
    { 0x350C, 0x055F, _ar0231_delay_ }, \
    { 0x30BA, 0x11F2, _ar0231_delay_ }, \
    { 0x3566, 0x1D28, _ar0231_delay_ }, \
    { 0x3518, 0x1FFE, _ar0231_delay_ }, \
    { 0x318E, 0x0200, _ar0231_delay_ }, \
    { 0x3190, 0x5000, _ar0231_delay_ }, \
    { 0x319E, 0x6060, _ar0231_delay_ }, \
    { 0x3520, 0x4688, _ar0231_delay_ }, \
    { 0x3522, 0x8840, _ar0231_delay_ }, \
    { 0x3524, 0x4046, _ar0231_delay_ }, \
    { 0x352C, 0xC6C6, _ar0231_delay_ }, \
    { 0x352A, 0x089F, _ar0231_delay_ }, \
    { 0x352E, 0x0011, _ar0231_delay_ }, \
    { 0x352E, 0x0011, _ar0231_delay_ }, \
    { 0x3530, 0x4400, _ar0231_delay_ }, \
    { 0x3530, 0x4400, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3536, 0xFF06, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x3538, 0xFFFF, _ar0231_delay_ }, \
    { 0x353A, 0x9000, _ar0231_delay_ }, \
    { 0x353C, 0x3F00, _ar0231_delay_ }, \
    { 0x353C, 0x3F00, _ar0231_delay_ }, \
    { 0x353C, 0x3F00, _ar0231_delay_ }, \
    { 0x353C, 0x3F00, _ar0231_delay_ }, \
    { 0x353C, 0x3F00, _ar0231_delay_ }, \
    { 0x353C, 0x3F00, _ar0231_delay_ }, \
    { 0x32EC, 0x72A1, _ar0231_delay_ }, \
    { 0x3540, 0xC659, _ar0231_delay_ }, \
    { 0x3540, 0xC659, _ar0231_delay_ }, \
    { 0x3556, 0x101F, _ar0231_delay_ }, \
    { 0x3566, 0x1D28, _ar0231_delay_ }, \
    { 0x3566, 0x1D28, _ar0231_delay_ }, \
    { 0x3566, 0x1D28, _ar0231_delay_ }, \
    { 0x3566, 0x1128, _ar0231_delay_ }, \
    { 0x3566, 0x1328, _ar0231_delay_ }, \
    { 0x3566, 0x3328, _ar0231_delay_ }, \
    { 0x3528, 0xDDDD, _ar0231_delay_ }, \
    { 0x3540, 0xC63E, _ar0231_delay_ }, \
    { 0x3542, 0x545B, _ar0231_delay_ }, \
    { 0x3544, 0x645A, _ar0231_delay_ }, \
    { 0x3546, 0x5A5A, _ar0231_delay_ }, \
    { 0x3548, 0x6400, _ar0231_delay_ }, \
    { 0x301A, 0x10D8, 10 },          \
    { 0x2512, 0x8000, _ar0231_delay_ }, \
    { 0x2510, 0x0905, _ar0231_delay_ }, \
    { 0x2510, 0x3350, _ar0231_delay_ }, \
    { 0x2510, 0x2004, _ar0231_delay_ }, \
    { 0x2510, 0x1460, _ar0231_delay_ }, \
    { 0x2510, 0x1578, _ar0231_delay_ }, \
    { 0x2510, 0x1360, _ar0231_delay_ }, \
    { 0x2510, 0x7B24, _ar0231_delay_ }, \
    { 0x2510, 0xFF24, _ar0231_delay_ }, \
    { 0x2510, 0xFF24, _ar0231_delay_ }, \
    { 0x2510, 0xEA24, _ar0231_delay_ }, \
    { 0x2510, 0x1022, _ar0231_delay_ }, \
    { 0x2510, 0x2410, _ar0231_delay_ }, \
    { 0x2510, 0x155A, _ar0231_delay_ }, \
    { 0x2510, 0x1342, _ar0231_delay_ }, \
    { 0x2510, 0x1400, _ar0231_delay_ }, \
    { 0x2510, 0x24FF, _ar0231_delay_ }, \
    { 0x2510, 0x24FF, _ar0231_delay_ }, \
    { 0x2510, 0x24EA, _ar0231_delay_ }, \
    { 0x2510, 0x2324, _ar0231_delay_ }, \
    { 0x2510, 0x647A, _ar0231_delay_ }, \
    { 0x2510, 0x2404, _ar0231_delay_ }, \
    { 0x2510, 0x052C, _ar0231_delay_ }, \
    { 0x2510, 0x400A, _ar0231_delay_ }, \
    { 0x2510, 0xFF0A, _ar0231_delay_ }, \
    { 0x2510, 0xFF0A, _ar0231_delay_ }, \
    { 0x2510, 0x1808, _ar0231_delay_ }, \
    { 0x2510, 0x3851, _ar0231_delay_ }, \
    { 0x2510, 0x1440, _ar0231_delay_ }, \
    { 0x2510, 0x0004, _ar0231_delay_ }, \
    { 0x2510, 0x0801, _ar0231_delay_ }, \
    { 0x2510, 0x0408, _ar0231_delay_ }, \
    { 0x2510, 0x1180, _ar0231_delay_ }, \
    { 0x2510, 0x15DC, _ar0231_delay_ }, \
    { 0x2510, 0x134C, _ar0231_delay_ }, \
    { 0x2510, 0x1002, _ar0231_delay_ }, \
    { 0x2510, 0x1016, _ar0231_delay_ }, \
    { 0x2510, 0x1181, _ar0231_delay_ }, \
    { 0x2510, 0x1189, _ar0231_delay_ }, \
    { 0x2510, 0x1056, _ar0231_delay_ }, \
    { 0x2510, 0x1210, _ar0231_delay_ }, \
    { 0x2510, 0x0901, _ar0231_delay_ }, \
    { 0x2510, 0x0D08, _ar0231_delay_ }, \
    { 0x2510, 0x0913, _ar0231_delay_ }, \
    { 0x2510, 0x13C8, _ar0231_delay_ }, \
    { 0x2510, 0x092B, _ar0231_delay_ }, \
    { 0x2510, 0x1588, _ar0231_delay_ }, \
    { 0x2510, 0x0901, _ar0231_delay_ }, \
    { 0x2510, 0x1388, _ar0231_delay_ }, \
    { 0x2510, 0x0909, _ar0231_delay_ }, \
    { 0x2510, 0x11D9, _ar0231_delay_ }, \
    { 0x2510, 0x091D, _ar0231_delay_ }, \
    { 0x2510, 0x1441, _ar0231_delay_ }, \
    { 0x2510, 0x0903, _ar0231_delay_ }, \
    { 0x2510, 0x1214, _ar0231_delay_ }, \
    { 0x2510, 0x0901, _ar0231_delay_ }, \
    { 0x2510, 0x10D6, _ar0231_delay_ }, \
    { 0x2510, 0x1210, _ar0231_delay_ }, \
    { 0x2510, 0x1212, _ar0231_delay_ }, \
    { 0x2510, 0x1210, _ar0231_delay_ }, \
    { 0x2510, 0x11DD, _ar0231_delay_ }, \
    { 0x2510, 0x11D9, _ar0231_delay_ }, \
    { 0x2510, 0x1056, _ar0231_delay_ }, \
    { 0x2510, 0x0905, _ar0231_delay_ }, \
    { 0x2510, 0x11DB, _ar0231_delay_ }, \
    { 0x2510, 0x092B, _ar0231_delay_ }, \
    { 0x2510, 0x119B, _ar0231_delay_ }, \
    { 0x2510, 0x11BB, _ar0231_delay_ }, \
    { 0x2510, 0x121A, _ar0231_delay_ }, \
    { 0x2510, 0x1210, _ar0231_delay_ }, \
    { 0x2510, 0x1460, _ar0231_delay_ }, \
    { 0x2510, 0x1250, _ar0231_delay_ }, \
    { 0x2510, 0x1076, _ar0231_delay_ }, \
    { 0x2510, 0x10E6, _ar0231_delay_ }, \
    { 0x2510, 0x0901, _ar0231_delay_ }, \
    { 0x2510, 0x15AB, _ar0231_delay_ }, \
    { 0x2510, 0x0901, _ar0231_delay_ }, \
    { 0x2510, 0x13A8, _ar0231_delay_ }, \
    { 0x2510, 0x1240, _ar0231_delay_ }, \
    { 0x2510, 0x1260, _ar0231_delay_ }, \
    { 0x2510, 0x0923, _ar0231_delay_ }, \
    { 0x2510, 0x158D, _ar0231_delay_ }, \
    { 0x2510, 0x138D, _ar0231_delay_ }, \
    { 0x2510, 0x0901, _ar0231_delay_ }, \
    { 0x2510, 0x0B09, _ar0231_delay_ }, \
    { 0x2510, 0x0108, _ar0231_delay_ }, \
    { 0x2510, 0x0901, _ar0231_delay_ }, \
    { 0x2510, 0x1440, _ar0231_delay_ }, \
    { 0x2510, 0x091D, _ar0231_delay_ }, \
    { 0x2510, 0x1588, _ar0231_delay_ }, \
    { 0x2510, 0x1388, _ar0231_delay_ }, \
    { 0x2510, 0x092D, _ar0231_delay_ }, \
    { 0x2510, 0x1066, _ar0231_delay_ }, \
    { 0x2510, 0x0905, _ar0231_delay_ }, \
    { 0x2510, 0x0C08, _ar0231_delay_ }, \
    { 0x2510, 0x090B, _ar0231_delay_ }, \
    { 0x2510, 0x1441, _ar0231_delay_ }, \
    { 0x2510, 0x090D, _ar0231_delay_ }, \
    { 0x2510, 0x10E6, _ar0231_delay_ }, \
    { 0x2510, 0x0901, _ar0231_delay_ }, \
    { 0x2510, 0x1262, _ar0231_delay_ }, \
    { 0x2510, 0x1260, _ar0231_delay_ }, \
    { 0x2510, 0x11BF, _ar0231_delay_ }, \
    { 0x2510, 0x11BB, _ar0231_delay_ }, \
    { 0x2510, 0x1066, _ar0231_delay_ }, \
    { 0x2510, 0x11FB, _ar0231_delay_ }, \
    { 0x2510, 0x0935, _ar0231_delay_ }, \
    { 0x2510, 0x11BB, _ar0231_delay_ }, \
    { 0x2510, 0x1263, _ar0231_delay_ }, \
    { 0x2510, 0x1260, _ar0231_delay_ }, \
    { 0x2510, 0x1400, _ar0231_delay_ }, \
    { 0x2510, 0x1510, _ar0231_delay_ }, \
    { 0x2510, 0x11B8, _ar0231_delay_ }, \
    { 0x2510, 0x12A0, _ar0231_delay_ }, \
    { 0x2510, 0x1200, _ar0231_delay_ }, \
    { 0x2510, 0x1026, _ar0231_delay_ }, \
    { 0x2510, 0x1000, _ar0231_delay_ }, \
    { 0x2510, 0x1342, _ar0231_delay_ }, \
    { 0x2510, 0x1100, _ar0231_delay_ }, \
    { 0x2510, 0x7A06, _ar0231_delay_ }, \
    { 0x2510, 0x0913, _ar0231_delay_ }, \
    { 0x2510, 0x0507, _ar0231_delay_ }, \
    { 0x2510, 0x0841, _ar0231_delay_ }, \
    { 0x2510, 0x3750, _ar0231_delay_ }, \
    { 0x2510, 0x2C2C, _ar0231_delay_ }, \
    { 0x2510, 0xFE05, _ar0231_delay_ }, \
    { 0x2510, 0xFE13, _ar0231_delay_ }, \
    { 0x1008, 0x0361, _ar0231_delay_ }, \
    { 0x100C, 0x0589, _ar0231_delay_ }, \
    { 0x100E, 0x07B1, _ar0231_delay_ }, \
    { 0x1010, 0x0139, _ar0231_delay_ }, \
    { 0x3230, 0x0304, _ar0231_delay_ }, \
    { 0x3232, 0x052C, _ar0231_delay_ }, \
    { 0x3234, 0x0754, _ar0231_delay_ }, \
    { 0x3236, 0x00DC, _ar0231_delay_ }, \
    { 0x3566, 0x3328, _ar0231_delay_ }, \
    { 0x350C, 0x055F, _ar0231_delay_ }, \
    { 0x32D0, 0x3A02, _ar0231_delay_ }, \
    { 0x32D2, 0x3508, _ar0231_delay_ }, \
    { 0x32D4, 0x3702, _ar0231_delay_ }, \
    { 0x32D6, 0x3C04, _ar0231_delay_ }, \
    { 0x32DC, 0x370A, _ar0231_delay_ }, \
    { 0x302A, 0x0006, _ar0231_delay_ }, \
    { 0x302C, 0x0001, _ar0231_delay_ }, \
    { 0x302E, 0x0002, _ar0231_delay_ }, \
    { 0x3030, 0x002C, _ar0231_delay_ }, \
    { 0x3036, 0x000C, _ar0231_delay_ }, \
    { 0x3038, 0x0001, _ar0231_delay_ }, \
    { 0x30B0, 0x0A00, _ar0231_delay_ }, \
    { 0x30A2, 0x0001, _ar0231_delay_ }, \
    { 0x30A6, 0x0001, _ar0231_delay_ }, \
    { 0x3040, 0x0000, _ar0231_delay_ }, \
    { 0x3040, 0x0000, _ar0231_delay_ }, \
    { 0x3082, 0x0001, _ar0231_delay_ }, \
    { 0x3082, 0x0001, _ar0231_delay_ }, \
    { 0x3082, 0x0001, _ar0231_delay_ }, \
    { 0x3082, 0x0001, _ar0231_delay_ }, \
    { 0x30BA, 0x11F2, _ar0231_delay_ }, \
    { 0x30BA, 0x11F2, _ar0231_delay_ }, \
    { 0x30BA, 0x11F2, _ar0231_delay_ }, \
    { 0x3044, 0x0400, _ar0231_delay_ }, \
    { 0x3044, 0x0400, _ar0231_delay_ }, \
    { 0x3044, 0x0400, _ar0231_delay_ }, \
    { 0x3044, 0x0400, _ar0231_delay_ }, \
    { 0x3064, 0x1882, _ar0231_delay_ }, \
    { 0x3064, 0x1802, _ar0231_delay_ }, \
    { 0x3064, 0x1802, _ar0231_delay_ }, \
    { 0x3064, 0x1802, _ar0231_delay_ }, \
    { 0x33E0, 0x0C80, _ar0231_delay_ }, \
    { 0x33E0, 0x0C80, _ar0231_delay_ }, \
    { 0x3180, 0x0080, _ar0231_delay_ }, \
    { 0x33E4, 0x0080, _ar0231_delay_ }, \
    { 0x33E0, 0x0C80, _ar0231_delay_ }, \
    { 0x33E0, 0x0C80, _ar0231_delay_ }, \
    { 0x3004, X_ADDR_START, _ar0231_delay_ }, \
    { 0x3008, X_ADDR_END, _ar0231_delay_ }, \
    { 0x3002, Y_ADDR_START, _ar0231_delay_ }, \
    { 0x3006, Y_ADDR_END, _ar0231_delay_ }, \
    { 0x3032, 0x0000, _ar0231_delay_ }, \
    { 0x3400, 0x0010, _ar0231_delay_ }, \
    { 0x3402, 0x0788, _ar0231_delay_ }, \
    { 0x3402, 0x0F10, _ar0231_delay_ }, \
    { 0x3404, 0x04B8, _ar0231_delay_ }, \
    { 0x3404, 0x0970, _ar0231_delay_ }, \
    { 0x3082, 0x0001, _ar0231_delay_ }, \
    { 0x30BA, 0x11F1, 10},           \
    { 0x30BA, AR0231_EXTRA_DELAY, _ar0231_delay_ }, \
    { 0x300C, AR0231_LINE_LENGTH_PCK, _ar0231_delay_ }, \
    { 0x300A, AR0231_FRAME_LENGTH_LINES, _ar0231_delay_ }, \
    { 0x3042, 0x0000, _ar0231_delay_ }, \
    { 0x3238, 0x0222, _ar0231_delay_ }, \
    { 0x3238, 0x0222, _ar0231_delay_ }, \
    { 0x3238, 0x0222, _ar0231_delay_ }, \
    { 0x3238, 0x0222, _ar0231_delay_ }, \
    { 0x3012, COARSE_INTEGRATION_TIME, _ar0231_delay_ }, \
    { 0x3014, FINE_INTEGRATION_TIME, _ar0231_delay_ }, \
    { 0x30B0, 0x0A00, _ar0231_delay_ }, \
    { 0x32EA, 0x3C0C, _ar0231_delay_ }, \
    { 0x32EA, 0x3C08, _ar0231_delay_ }, \
    { 0x32EA, 0x3C08, _ar0231_delay_ }, \
    { 0x32EC, 0x72A1, _ar0231_delay_ }, \
    { 0x32EC, 0x72A1, _ar0231_delay_ }, \
    { 0x32EC, 0x72A1, _ar0231_delay_ }, \
    { 0x32EC, 0x72A1, _ar0231_delay_ }, \
    { 0x32EC, 0x72A1, _ar0231_delay_ }, \
    { 0x32EC, 0x72A1, _ar0231_delay_ }, \
    { 0x31D0, 0x0000, _ar0231_delay_ }, \
    { 0x31AE, 0x0004, _ar0231_delay_ }, \
    { 0x31AE, 0x0304, _ar0231_delay_ }, \
    { 0x31AC, 0x140C, _ar0231_delay_ }, \
    { 0x31AC, 0x0C0C, _ar0231_delay_ }, \
    { 0x301A, 0x1098, _ar0231_delay_ }, \
    { 0x301A, 0x1018, _ar0231_delay_ }, \
    { 0x301A, 0x0018, _ar0231_delay_ }, \
    { 0x31AE, 0x0204, _ar0231_delay_ }, \
    { 0x3342, 0x122C, _ar0231_delay_ }, \
    { 0x3346, 0x122C, _ar0231_delay_ }, \
    { 0x334A, 0x122C, _ar0231_delay_ }, \
    { 0x334E, 0x122C, _ar0231_delay_ }, \
    { 0x3344, 0x0011, _ar0231_delay_ }, \
    { 0x3348, 0x0111, _ar0231_delay_ }, \
    { 0x334C, 0x0211, _ar0231_delay_ }, \
    { 0x3350, 0x0311, _ar0231_delay_ }, \
    { 0x31B0, 0x003A, _ar0231_delay_ }, \
    { 0x31B2, 0x0020, _ar0231_delay_ }, \
    { 0x31B4, 0x2876, _ar0231_delay_ }, \
    { 0x31B6, 0x2193, _ar0231_delay_ }, \
    { 0x31B8, 0x3048, _ar0231_delay_ }, \
    { 0x31BA, 0x0188, _ar0231_delay_ }, \
    { 0x31BC, 0x8006, _ar0231_delay_ }, \
    { 0x3366, 0x000A, _ar0231_delay_ }, \
    { 0x3306, 0x0400, _ar0231_delay_ }, \
    { 0x301A, 0x001C, 100 }, \
}

static int ar0231_detect(max9296_context_t* ctxt, uint32 link);
static int ar0231_get_link_cfg(max9296_context_t* ctxt, uint32 link, max9296_link_cfg_t* p_cfg);
static int ar0231_init_link(max9296_context_t* ctxt, uint32 link);
static int ar0231_start_link(max9296_context_t* ctxt, uint32 link);
static int ar0231_stop_link(max9296_context_t* ctxt, uint32 link);
static int ar0231_calculate_exposure(max9296_context_t* ctxt, uint32 link, sensor_exposure_info_t* exposure_info);
static int ar0231_apply_exposure(max9296_context_t* ctxt, uint32 link, sensor_exposure_info_t* exposure_info);

max9296_sensor_t ar0231_info = {
    .id = MAXIM_SENSOR_ID_AR0231,
    .detect = ar0231_detect,
    .get_link_cfg = ar0231_get_link_cfg,

    .init_link = ar0231_init_link,
    .start_link = ar0231_start_link,
    .stop_link = ar0231_stop_link,

    .calculate_exposure = ar0231_calculate_exposure,
    .apply_exposure = ar0231_apply_exposure
};


static struct camera_i2c_reg_setting cam_ser_reg_setting =
{
    .reg_array = NULL,
    .size = 0,
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_setting cam_sensor_reg_setting =
{
    .reg_array = NULL,
    .size = 0,
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_WORD_DATA,
};

static struct camera_i2c_reg_array max9295_gmsl_0[] = CAM_SER_ADDR_CHNG_A;
static struct camera_i2c_reg_array max9295_gmsl_1[] = CAM_SER_ADDR_CHNG_B;
static struct camera_i2c_reg_array max9295_init_0[] = CAM_SER_INIT_A;
static struct camera_i2c_reg_array max9295_init_1[] = CAM_SER_INIT_B;
static struct camera_i2c_reg_array max9295_start_reg[] = CAM_SER_START;
static struct camera_i2c_reg_array max9295_stop_reg[] = CAM_SER_STOP;
static struct camera_i2c_reg_array ar0231_init_array[] = CAM_SNSR_INIT;

// List of serializer addresses we support
static uint16 supported_ser_addr[] = {0xC4, 0x88, 0x80};

static maxim_pipeline_t ar0231_isp_pipelines[MAXIM_LINK_MAX] =
{
    {
        .id = MAXIM_PIPELINE_X,
        .mode =
        {
            .fmt = FMT_LINK,
            .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
            .channel_info = {.vc = 0, .dt = DT_LINK, .cid = CID_VC0},
        },
    },
    {
        .id = MAXIM_PIPELINE_X,
        .mode =
        {
            .fmt = FMT_LINK,
            .res = {.width = SENSOR_WIDTH, .height = SENSOR_HEIGHT, .fps = 30.0f},
            .channel_info = {.vc = 1, .dt = DT_LINK, .cid = CID_VC1},
        },
    }
};


struct gain_table_t
{
    float real_gain;
    char analog_gain;
    char lcg_hcg;
};

static struct gain_table_t gain_table[] =
{
    { 1.0f,  0x7, 0 },
    { 1.25f, 0x8, 0 },
    { 1.5f,  0x9, 0 },
    { 2.0f,  0xA, 0 },
    { 2.33f, 0xB, 0 },
    { 3.50f, 0xC, 0 },
    { 4.0f,  0xD, 0 },
    { 5.0f,  0x7, 1 },
    { 6.25f, 0x8, 1 },
    { 7.5f,  0x9, 1 },
    { 10.0f, 0xA, 1 },
    { 11.67f,0xB, 1 },
    { 17.5f, 0xC, 1 },
    { 20.0f, 0xD, 1 },
    { 40.0f, 0xE, 1 },
};


static int ar0231_detect(max9296_context_t* ctxt, uint32 link)
{
    max9296_context_t* pCtxt = (max9296_context_t*)ctxt;
    int rc = 0;
    int i = 0;
    int num_addr = STD_ARRAY_SIZE(supported_ser_addr);
    struct camera_i2c_reg_array read_reg[] = {{0, 0, 0}};
    max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[link];
    sensor_platform_func_table_t* sensor_fcn_tbl = &pCtxt->platform_fcn_tbl;

    cam_ser_reg_setting.reg_array = read_reg;
    cam_ser_reg_setting.size = STD_ARRAY_SIZE(read_reg);

    /* Detect far end serializer */
    for (i = 0; i < num_addr; i++)
    {
        cam_ser_reg_setting.reg_array[0].reg_addr = MSM_SER_CHIP_ID_REG_ADDR;
        rc = sensor_fcn_tbl->i2c_slave_read(pCtxt->ctrl, supported_ser_addr[i], &cam_ser_reg_setting);
        if (!rc)
        {
            pSensor->serializer_addr = supported_ser_addr[i];
            break;
        }
    }

    if (i == num_addr)
    {
        SENSOR_WARN("No Camera connected to Link %d of MAX9296 0x%x", link, pCtxt->slave_addr);
    }
    else if (pSensor->serializer_alias == pSensor->serializer_addr)
    {
        SENSOR_WARN("LINK %d already re-mapped", link);
        rc = 0;
    }
    else
    {
        struct camera_i2c_reg_array remap_ser[] = {
            {0x0, pSensor->serializer_alias, _ar0231_delay_}
        };

        //link reset, remap cam, create broadcast addr,
        struct camera_i2c_reg_array remap_ser_2[] = {
            {0x0010, 0x31, MAX9295_LINK_RESET_DELAY },
            {0x0042, pSensor->sensor_alias, _ar0231_delay_},
            {0x0043, CAM_SENSOR_DEFAULT_ADDR, _ar0231_delay_},
            {0x0044, CAM_SER_BROADCAST_ADDR, _ar0231_delay_},
            {0x0045, pSensor->serializer_alias, _ar0231_delay_}
        };

        SENSOR_WARN("Detected Camera connected to Link %d, Ser ID[0x%x]: 0x%x",
            link, MSM_SER_CHIP_ID_REG_ADDR, cam_ser_reg_setting.reg_array[0].reg_data);

        cam_ser_reg_setting.reg_array = remap_ser;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(remap_ser);
        rc = sensor_fcn_tbl->i2c_slave_write_array(pCtxt->ctrl,pSensor->serializer_addr, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to change serializer address (0x%x) of MAX9296 0x%x Link %d",
                pSensor->serializer_addr, pCtxt->slave_addr, link);
            return -1;
        }

        cam_ser_reg_setting.reg_array = remap_ser_2;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(remap_ser_2);
        rc = sensor_fcn_tbl->i2c_slave_write_array(pCtxt->ctrl, pSensor->serializer_alias, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to reset link %d and remap cam on serializer(0x%x)", link, pSensor->serializer_alias);
            return -1;
        }

        cam_ser_reg_setting.reg_array = link ? max9295_gmsl_1 : max9295_gmsl_0;
        cam_ser_reg_setting.size = link ? STD_ARRAY_SIZE(max9295_gmsl_1) : STD_ARRAY_SIZE(max9295_gmsl_0);
        rc = sensor_fcn_tbl->i2c_slave_write_array(pCtxt->ctrl, pSensor->serializer_alias, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to reset link %d and remap cam on serializer(0x%x)", link, pSensor->serializer_alias);
            return -1;
        }

        // Read mapped SER to double check if successful
        cam_ser_reg_setting.reg_array = read_reg;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(read_reg);
        cam_ser_reg_setting.reg_array[0].reg_addr = 0x0000;
        rc = sensor_fcn_tbl->i2c_slave_read(pCtxt->ctrl, pSensor->serializer_alias, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to read serializer(0x%x) after remap", pSensor->serializer_alias);
            return -1;
        }

        if (pSensor->serializer_alias != cam_ser_reg_setting.reg_array[0].reg_data)
        {
            SENSOR_WARN("Remote SER address remap failed: 0x%x, should be 0x%x",
                cam_ser_reg_setting.reg_array[0].reg_data, pSensor->serializer_alias);
        }
    }

    return rc;
}

static int ar0231_get_link_cfg(max9296_context_t* ctxt, uint32 link, max9296_link_cfg_t* p_cfg)
{
    (void)ctxt;

    p_cfg->num_pipelines = 1;
    p_cfg->pipelines[0] = ar0231_isp_pipelines[link];
    return 0;
}

static int ar0231_init_link(max9296_context_t* ctxt, uint32 link)
{
    max9296_context_t* pCtxt = ctxt;
    max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[link];
    int rc;

    if (SENSOR_STATE_DETECTED == pSensor->state)
    {
        struct camera_i2c_reg_array read_reg[] = {{0, 0, 0}};

        cam_ser_reg_setting.reg_array = link ? max9295_init_1 : max9295_init_0;
        cam_ser_reg_setting.size = link ? STD_ARRAY_SIZE(max9295_init_1) : STD_ARRAY_SIZE(max9295_init_0);

        rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(pCtxt->ctrl,
                    pSensor->serializer_alias, &cam_ser_reg_setting);
        if (rc)
        {
            SERR("Failed to init camera serializer(0x%x)", pSensor->serializer_alias);
            return -1;
        }

        cam_sensor_reg_setting.reg_array = read_reg;
        cam_sensor_reg_setting.size = STD_ARRAY_SIZE(read_reg);
        cam_sensor_reg_setting.reg_array[0].reg_addr = 0x3000;
        rc = pCtxt->platform_fcn_tbl.i2c_slave_read(pCtxt->ctrl,
                    pSensor->sensor_alias, &cam_sensor_reg_setting);
        if (rc)
        {
            SERR("Failed to read sensor (0x%x)", pSensor->sensor_alias);
            return -1;
        }
        else
        {
            SHIGH("Remote SENSOR addr 0x%x", cam_sensor_reg_setting.reg_array[0].reg_data);
        }

        pSensor->state = SENSOR_STATE_INITIALIZED;
    }
    else
    {
        SERR("ar0231 %d init in wrong state %d", link, pSensor->state);
        rc = -1;
    }

    return rc;
}

static int ar0231_start_link(max9296_context_t* ctxt, uint32 link)
{
    max9296_context_t* pCtxt = ctxt;
    max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[link];
    int rc;

    if (SENSOR_STATE_INITIALIZED == pSensor->state)
    {
        SHIGH("starting serializer");
        cam_ser_reg_setting.reg_array = max9295_start_reg;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(max9295_start_reg);
        rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pSensor->serializer_alias,
            &cam_ser_reg_setting);
        if (rc)
        {
            SERR("serializer 0x%x failed to start", pSensor->serializer_alias);
        }
        else
        {
            SHIGH("starting sensor");

            cam_sensor_reg_setting.reg_array = ar0231_init_array;
            cam_sensor_reg_setting.size = STD_ARRAY_SIZE(ar0231_init_array);
            rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
                pCtxt->ctrl,
                pSensor->sensor_alias,
                &cam_sensor_reg_setting);
            if (rc)
            {
                SERR("sensor 0x%x failed to start", pSensor->sensor_alias);
            }
            else
            {
                pSensor->state = SENSOR_STATE_STREAMING;
            }
        }
    }
    else
    {
        SERR("ar0231 %d start in wrong state %d", link, pSensor->state);
        rc = -1;
    }

    return rc;
}

static int ar0231_stop_link(max9296_context_t* ctxt, uint32 link)
{
    max9296_context_t* pCtxt = ctxt;
    max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[link];
    int rc;

    if (SENSOR_STATE_STREAMING == pSensor->state)
    {
        cam_ser_reg_setting.reg_array = max9295_stop_reg;
        cam_ser_reg_setting.size = STD_ARRAY_SIZE(max9295_stop_reg);
        if ((rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pSensor->serializer_alias,
            &cam_ser_reg_setting)))
        {
            SERR("Failed to stop serializer(0x%x)", pSensor->serializer_alias);
        }

        pSensor->state = SENSOR_STATE_INITIALIZED;
    }
    else
    {
        SERR("ar0231 %d start in wrong state %d", link, pSensor->state);
        rc = -1;
    }

    return rc;
}

static int ar0231_calculate_exposure(max9296_context_t* ctxt, uint32 link, sensor_exposure_info_t* exposure_info)
{
    (void)ctxt;
    (void)link;

    exposure_info->exp_pclk_count = exposure_info->exposure_time*AR0231_PCLK/1000;
    exposure_info->line_count = STD_MIN((unsigned int)((exposure_info->exposure_time*AR0231_PCLK)/(AR0231_LINE_LENGTH_PCK*1000)), (AR0231_FRAME_LENGTH_LINES-1));

    SLOW("AEC - AR0143 : exp_time=%f => line_cnt=0x%x", exposure_info->exposure_time, exposure_info->line_count);

    int gain_table_size = STD_ARRAY_SIZE(gain_table);
    int last_idx = gain_table_size - 1;
    int i = 0;

    if (exposure_info->real_gain >= gain_table[last_idx].real_gain)
    {
        exposure_info->reg_gain = last_idx;
    }
    else
    {
        for (i = 0; i < last_idx; i++)
        {
            if (exposure_info->real_gain < gain_table[i+1].real_gain)
            {
                exposure_info->reg_gain = i;
                break;
            }
        }
    }

    exposure_info->sensor_analog_gain = gain_table[exposure_info->reg_gain].real_gain;
    exposure_info->sensor_digital_gain = exposure_info->real_gain / exposure_info->sensor_analog_gain;

    SLOW("AEC - AR0143 :idx = %d, gain=%0.2f, a_gain=%0.2f + d_gain=%0.2f",
            exposure_info->reg_gain, exposure_info->real_gain,
            exposure_info->sensor_analog_gain, exposure_info->sensor_digital_gain);

    return 0;
}


static int ar0231_apply_exposure(max9296_context_t* ctxt, uint32 link, sensor_exposure_info_t* exposure_info)
{
    max9296_context_t* pCtxt = ctxt;
    max9296_sensor_info_t* pSensor = &pCtxt->max9296_sensors[link];
    int rc = 0;

    if (SENSOR_STATE_INITIALIZED == pSensor->state)
    {
        SERR("ar0231 %d start in wrong state %d", link, pSensor->state);
        rc = -1;
    }
    else if(exposure_info->exposure_mode_type == QCARCAM_EXPOSURE_MANUAL ||
       exposure_info->exposure_mode_type == QCARCAM_EXPOSURE_AUTO)
    {
        static struct camera_i2c_reg_array ar0231_exp_reg_array[5];

        float real_gain = gain_table[exposure_info->reg_gain].real_gain;
        char analog_gain = gain_table[exposure_info->reg_gain].analog_gain;
        char lcg_hcg = gain_table[exposure_info->reg_gain].lcg_hcg;

        unsigned int fine_integration_time = STD_MIN(exposure_info->exp_pclk_count - (exposure_info->line_count*AR0231_LINE_LENGTH_PCK),
                (AR0231_LINE_LENGTH_PCK-1));

        SLOW("AEC - AR0231 : coarse=0x%x, fine=0x%x, real_gain=%0.2f",
                exposure_info->line_count, fine_integration_time, real_gain);

        ar0231_exp_reg_array[0].reg_addr = 0x3012; //COARSE_INTEGRATION_TIME_
        ar0231_exp_reg_array[0].reg_data = exposure_info->line_count & 0xffff;
        ar0231_exp_reg_array[0].delay = _ar0231_delay_;

        ar0231_exp_reg_array[1].reg_addr = 0x3014; //FINE_INTEGRATION_TIME_
        ar0231_exp_reg_array[1].reg_data = FINE_INTEGRATION_TIME; //TODO: fine_integration_time & 0xffff;
        ar0231_exp_reg_array[1].delay = _ar0231_delay_;

        ar0231_exp_reg_array[2].reg_addr = 0x3366; //ANALOG_GAIN
        ar0231_exp_reg_array[2].reg_data = AR0231_PACK_ANALOG_GAIN(analog_gain & 0xf);
        ar0231_exp_reg_array[2].delay = _ar0231_delay_;

        ar0231_exp_reg_array[3].reg_addr = 0x3362; //DC_GAIN : Dual conversion gain
        ar0231_exp_reg_array[3].reg_data = lcg_hcg ? 0xFF : 0x00;
        ar0231_exp_reg_array[3].delay = _ar0231_delay_;

        //0x305E; GLOBAL_GAIN : Q7 <- USE ONLY FOR WHITE BALANCING
        ar0231_exp_reg_array[4].reg_addr = 0x3308; //DIG_GLOBAL_GAIN : Q9
        ar0231_exp_reg_array[4].reg_data = STD_MIN(FLOAT_TO_Q(9, exposure_info->sensor_digital_gain), 0x7FF);
        ar0231_exp_reg_array[4].delay = _ar0231_delay_;

        cam_sensor_reg_setting.reg_array = ar0231_exp_reg_array;
        cam_sensor_reg_setting.size = STD_ARRAY_SIZE(ar0231_exp_reg_array);

        rc = pCtxt->platform_fcn_tbl.i2c_slave_write_array(
            pCtxt->ctrl,
            pSensor->sensor_alias,
            &cam_sensor_reg_setting);
        if (rc)
        {
            SERR("sensor 0x%x failed to start", pSensor->sensor_alias);
        }
    }
    else
    {
        //TODO: pass-through for now
        SERR("not implemented AEC mode %d", exposure_info->exposure_mode_type);
        rc = 0;
    }


    return rc;
}
