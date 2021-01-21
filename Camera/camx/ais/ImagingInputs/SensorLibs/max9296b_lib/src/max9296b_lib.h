/**
 * @file max9296b_lib.h
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __MAX9296_LIB_H__
#define __MAX9296_LIB_H__

#include "sensor_lib.h"

#define SENSOR_WIDTH    1920
// The sensor embeds extra 4 lines of metadata with ISP parameters, this is the default behaviour
// Adding this by default, if possible to query, will add code to this later
#define SENSOR_HEIGHT   1024
#define _CAMTIMING_XOUT SENSOR_WIDTH
#define _CAMTIMING_YOUT SENSOR_HEIGHT
#define _CAMTIMING_CLKS_PER_LINE 1920
#define _CAMTIMING_TOTAL_LINES 1020
#define _CAMTIMING_VT_CLK 79976160
#define _CAMTIMING_OP_CLK 79976160

#define NUM_SUPPORT_SENSORS 2
#define SENSOR_MODEL "max9296"


#define MSM_DESER_0_SLAVEADDR           0x90
#define MSM_DESER_1_SLAVEADDR           0x94
#define CAM_SENSOR_DEFAULT_ADDR         0xE0
#define CAM_SERIALIZER_BROADCAST_ADDR   0x8E

#define MSM_SERIALIZER_ADDR             0x80

//Addresses after reporgramming the serialzer and cameras attached to MX9296
#define MSM_DESER_0_ALIAS_ADDR_CAM_SER_0        0x82
#define MSM_DESER_0_ALIAS_ADDR_CAM_SER_1        0x84
#define MSM_DESER_1_ALIAS_ADDR_CAM_SER_0        0x8A
#define MSM_DESER_1_ALIAS_ADDR_CAM_SER_1        0x8C
#define MSM_DESER_0_ALIAS_ADDR_CAM_SNSR_0       0xE4
#define MSM_DESER_0_ALIAS_ADDR_CAM_SNSR_1       0xE8
#define MSM_DESER_1_ALIAS_ADDR_CAM_SNSR_0       0xEA
#define MSM_DESER_1_ALIAS_ADDR_CAM_SNSR_1       0xEC

#define MSM_DESER_CHIP_ID_REG_ADDR              0xD
#define MSM_SER_CHIP_ID_REG_ADDR                0xD
#define MSM_DESER_REVISION_REG_ADDR             0xE
#define MSM_SER_REVISION_REG_ADDR               0XE

#define MSM_SER_REVISION_ES2                    0xFF
#define MSM_DESER_REVISION_ES2                  0xFF

/*CONFIGURATION OPTIONS*/

#define _reg_delay_ 0

/*
* Supported format    DT in CSI  Y/U/V layout in mem
* QCARCAM_FMT_UYVY_8  CSI_RAW8   Plain8: 8bits in 1 byte
* QCARCAM_FMT_UYVY_10 CSI_RAW10  Plain16: 000000xxxxxxxxxx(LSB)
*/
#define FMT_9296_LINK_A   QCARCAM_FMT_UYVY_8
#define FMT_9296_LINK_B   QCARCAM_FMT_UYVY_8
#define DT_9296_LINK_A    CSI_RAW8
#define DT_9296_LINK_B    CSI_RAW8

#define CID_VC0        0
#define CID_VC1        4


#define DESERIALIZER_SELECT_LINK_A \
{\
    { 0x0010, 0x21, 100000}, \
}

#define DESERIALIZER_SELECT_LINK_B \
{\
    { 0x0010, 0x22, 100000}, \
}

#define INIT_CAM_SERIALIZER_ADDR_CHNG_A \
{ \
    { 0x006B, 0x10, _reg_delay_ }, \
    { 0x0073, 0x11, _reg_delay_ }, \
    { 0x007B, 0x30, _reg_delay_ }, \
    { 0x0083, 0x30, _reg_delay_ }, \
    { 0x0093, 0x30, _reg_delay_ }, \
    { 0x009B, 0x30, _reg_delay_ }, \
    { 0x00A3, 0x30, _reg_delay_ }, \
    { 0x00AB, 0x30, _reg_delay_ }, \
    { 0x008B, 0x30, _reg_delay_ }, \
}

#define INIT_CAM_SERIALIZER_ADDR_CHNG_B \
{ \
    { 0x006B, 0x12, _reg_delay_ }, \
    { 0x0073, 0x13, _reg_delay_ }, \
    { 0x007B, 0x32, _reg_delay_ }, \
    { 0x0083, 0x32, _reg_delay_ }, \
    { 0x0093, 0x32, _reg_delay_ }, \
    { 0x009B, 0x32, _reg_delay_ }, \
    { 0x00A3, 0x32, _reg_delay_ }, \
    { 0x00AB, 0x32, _reg_delay_ }, \
    { 0x008B, 0x32, _reg_delay_ }, \
}

/*
Reg 1B0 - 1B9
Use ‘crossbar’ in the serializer move bits from 10 to 8, moving last two LSB to MSB

9 .... 0 -> 1098765432 in serialzer (cross bar)
10 | 98765432 (8 bits) in deser (truncate)

Deseralizer is programmed to truncate from 10 to 8 (in MSB). Effective, you get 9 - 2, truncating just the 2 LSB bits
*/

#define INIT_CAM_SERIALIZER_8BIT_0 \
{ \
    { 0x0002, 0x03, _reg_delay_ }, \
    { 0x0100, 0x60, _reg_delay_ }, \
    { 0x0101, 0x4A, _reg_delay_ }, \
    { 0x01B0, 0x02, _reg_delay_ }, \
    { 0x01B1, 0x03, _reg_delay_ }, \
    { 0x01B2, 0x04, _reg_delay_ }, \
    { 0x01B3, 0x05, _reg_delay_ }, \
    { 0x01B4, 0x06, _reg_delay_ }, \
    { 0x01B5, 0x07, _reg_delay_ }, \
    { 0x01B6, 0x08, _reg_delay_ }, \
    { 0x01B7, 0x09, _reg_delay_ }, \
}

#define INIT_CAM_SERIALIZER_8BIT_1 \
{ \
    { 0x0002, 0x03, _reg_delay_ }, \
    { 0x0100, 0x60, _reg_delay_ }, \
    { 0x0101, 0x0A, _reg_delay_ }, \
    { 0x01B0, 0x02, _reg_delay_ }, \
    { 0x01B1, 0x03, _reg_delay_ }, \
    { 0x01B2, 0x04, _reg_delay_ }, \
    { 0x01B3, 0x05, _reg_delay_ }, \
    { 0x01B4, 0x06, _reg_delay_ }, \
    { 0x01B5, 0x07, _reg_delay_ }, \
    { 0x01B6, 0x08, _reg_delay_ }, \
    { 0x01B7, 0x09, _reg_delay_ }, \
    { 0x01B8, 0x00, _reg_delay_ }, \
    { 0x01B9, 0x01, _reg_delay_ }, \
    { 0x0053, 0x01, _reg_delay_ }, \
    { 0x0102, 0x0E, _reg_delay_ }, \
    { 0x0311, 0x10, _reg_delay_ }, \
}

#define INIT_CAM_SERIALIZER_10BIT_0 \
{ \
    { 0x0002, 0x03, _reg_delay_ }, \
    { 0x0100, 0x60, _reg_delay_ }, \
    { 0x0101, 0x0A, _reg_delay_ }, \
    { 0x0053, 0x00, _reg_delay_ }, \
    { 0x0102, 0x0E, _reg_delay_ }, \
    { 0x0311, 0x10, _reg_delay_ }, \
}

#define INIT_CAM_SERIALIZER_10BIT_1 \
{ \
    { 0x0002, 0x03, _reg_delay_ }, \
    { 0x0100, 0x60, _reg_delay_ }, \
    { 0x0101, 0x0A, _reg_delay_ }, \
    { 0x0053, 0x01, _reg_delay_ }, \
    { 0x0102, 0x0E, _reg_delay_ }, \
    { 0x0311, 0x10, _reg_delay_ }, \
}


#define ENABLE_DESERIALIZER \
{ \
    { 0x0002, 0x73, _reg_delay_ }, \
}

#define DESERIALIZER_SPLITER_MODE \
{\
    { 0x0010, 0x23, 100000},\
}


#define SERIALIZER_LINK_RESET \
{\
    { 0x0010, 0x31, 100000},\
}


//NOTE: 1 -> 12 disables backward propogation of I2C commands.
// We only do this in the case of receriver board in case its only connected to another board.
// If connected to camera this will need to be removed

#define INIT_MSM_DESERIALIZER_LINK_MODE_RECEIVER \
{ \
    { 0x0001, 0x12, _reg_delay_ }, \
    { 0x0002, 0x03, _reg_delay_ }, \
    { 0x031C, 0x20, _reg_delay_ }, \
    { 0x031F, 0x20, _reg_delay_ }, \
    { 0x0051, 0x01, _reg_delay_ }, \
    { 0x0473, 0x02, _reg_delay_ }, \
    { 0x0100, 0x23, _reg_delay_ }, \
    { 0x0112, 0x23, _reg_delay_ }, \
    { 0x0124, 0x23, _reg_delay_ }, \
    { 0x0136, 0x23, _reg_delay_ }, \
    { 0x0320, 0x2C, _reg_delay_ }, \
}


#define INIT_MSM_DESERIALIZER_SPLITTER_MODE_RECEIVER \
{\
    { 0x0001, 0x12, _reg_delay_ }, \
    { 0x0002, 0x03, _reg_delay_ }, \
    { 0x031C, 0x60, _reg_delay_ }, \
    { 0x031F, 0x60, _reg_delay_ }, \
    { 0x0473, 0x02, _reg_delay_ }, \
    { 0x04B3, 0x02, _reg_delay_ }, \
    { 0x0100, 0x23, _reg_delay_ }, \
    { 0x0112, 0x23, _reg_delay_ }, \
    { 0x0124, 0x23, _reg_delay_ }, \
    { 0x0136, 0x23, _reg_delay_ }, \
    { 0x0320, 0x30, _reg_delay_ }, \
    { 0x048B, 0x07, _reg_delay_ }, \
    { 0x04AD, 0x15, _reg_delay_ }, \
    { 0x048D, 0x6A, _reg_delay_ }, \
    { 0x048E, 0x6A, _reg_delay_ }, \
    { 0x048F, 0x40, _reg_delay_ }, \
    { 0x0490, 0x40, _reg_delay_ }, \
    { 0x0491, 0x41, _reg_delay_ }, \
    { 0x0492, 0x41, _reg_delay_ }, \
}

/*
// Des Setup

0x04,0x90,0x00,0x02,0x03,   // Disable all pipes

0x04,0x90,0x00,0x50,0x00,   // Pipe X stream select = 0

0x04,0x90,0x00,0x51,0x00,   // Pipe Y stream select = 0

0x04,0x90,0x00,0x52,0x00,   // Pipe Z stream select = 0

0x04,0x90,0x00,0x53,0x01,   // Pipe U stream select = 1

// Pipes X, Y, and Z use DT = 0x2A (RAW8) and BPP = 8
0x04,0x90,0x03,0x13,0x42,
0x04,0x90,0x03,0x16,0xAA,
0x04,0x90,0x03,0x17,0xAA,
0x04,0x90,0x03,0x19,0x48,
0x04,0x90,0x03,0x18,0x02,

0x04,0x90,0x03,0x1D,0xC8,   // BPP/DT/VC override enabled on pipes X and Y
                            // PHY0 (Port C) CSI output is 800Mbps/lane on port C

0x04,0x90,0x03,0x20,0x48,   // BPP/DT/VC override enabled on pipe Z
                            // PHY1 (Port D) CSI outuput is 800Mbps/lane on port A

0x04,0x90,0x03,0x23,0x28,   // PHY2 (Port E) CSI output is 800Mbps/lane on port B

*/
#define INIT_MSM_DESERIALIZER_SPLITTER_MODE_SENDER \
{ \
    { 0x0002, 0x03, _reg_delay_ }, \
    { 0x0050, 0x00, _reg_delay_ }, \
    { 0x0051, 0x00, _reg_delay_ }, \
    { 0x0052, 0x00, _reg_delay_ }, \
    { 0x0053, 0x01, _reg_delay_ }, \
    { 0x0313, 0x42, _reg_delay_ }, \
    { 0x0316, 0xAA, _reg_delay_ }, \
    { 0x0317, 0xAA, _reg_delay_ }, \
    { 0x0319, 0x48, _reg_delay_ }, \
    { 0x0318, 0x02, _reg_delay_ }, \
    { 0x031D, 0xC8, _reg_delay_ }, \
    { 0x0320, 0x48, _reg_delay_ }, \
    { 0x0323, 0x28, _reg_delay_ }, \
}

#define INIT_MSM_DESERIALIZER_8BIT_A \
{ \
    { 0x0313, 0x42, _reg_delay_ }, \
    { 0x0316, 0xAA, _reg_delay_ }, \
    { 0x040D, 0x2A, _reg_delay_ }, \
    { 0x040E, 0x2A, _reg_delay_ }, \
}

#define INIT_MSM_DESERIALIZER_8BIT_B \
{ \
    { 0x0316, 0xAA, _reg_delay_ }, \
    { 0x0317, 0x0A, _reg_delay_ }, \
    { 0x0319, 0x08, _reg_delay_ }, \
    { 0x044D, 0x2A, _reg_delay_ }, \
    { 0x044E, 0x6A, _reg_delay_ }, \
}

#define INIT_MSM_DESERIALIZER_10BIT_A \
{ \
    { 0x0313, 0x52, _reg_delay_ }, \
    { 0x0316, 0xAB, _reg_delay_ }, \
    { 0x040D, 0x2B, _reg_delay_ }, \
    { 0x040E, 0x2B, _reg_delay_ }, \
}

#define INIT_MSM_DESERIALIZER_10BIT_B \
{ \
    { 0x0316, 0xAB, _reg_delay_ }, \
    { 0x0317, 0x0B, _reg_delay_ }, \
    { 0x0319, 0x0A, _reg_delay_ }, \
    { 0x044D, 0x2B, _reg_delay_ }, \
    { 0x044E, 0x6B, _reg_delay_ }, \
}

#define START_MSM_DESERIALIZER_SPLIT \
{ \
    { 0x002, 0x73, 100000 }, \
}

#define START_MSM_DESERIALIZER_LINK_A \
{ \
    { 0x002, 0x73, 100000 }, \
}

#define START_MSM_DESERIALIZER_LINK_B \
{ \
    { 0x002, 0x73, 100000 }, \
}

#define STOP_MSM_DESERIALIZER \
{ \
    { 0x002, 0x03, _reg_delay_ }, \
}

#define START_CAM_SERIALIZER \
{ \
    { 0x0007, 0x37, _reg_delay_ }, \
    { 0x0002, 0x13, 100000 }, \
}

#define STOP_CAM_SERIALIZER \
{ \
    { 0x0002, 0x03, 100000 }, \
}

#define INIT_MSM_SERIALIZER_DISABLE_I2C_FORWARD \
{ \
    { 0x0001, 0x18, _reg_delay_ }, \
}

// NOTE: 1 -> 12 disables backward propogation of I2C commands.
// We only do this in the case of receriver board in case its only connected to another board.
// If connected to camera this will need to be removed
#define MSM_DESERIALIZER_DISABLE_I2C_REVERSE \
{ \
    { 0x0001, 0x12, _reg_delay_ }, \
}

#define INIT_MSM_SERIALIZER_DUAL_BCAST \
{ \
    { 0x0001, 0x18, _reg_delay_ }, \
    { 0x0330, 0x00, _reg_delay_ }, \
    { 0x0331, 0x33, _reg_delay_ }, \
    { 0x0332, 0xEE, _reg_delay_ }, \
    { 0x0333, 0xE4, _reg_delay_ }, \
    { 0x0311, 0xF0, _reg_delay_ }, \
    { 0x0308, 0x7F, _reg_delay_ }, \
    { 0x0002, 0xF3, _reg_delay_ }, \
    { 0x0314, 0xE2, _reg_delay_ }, \
    { 0x0315, 0x80, _reg_delay_ }, \
    { 0x0316, 0x6A, _reg_delay_ }, \
    { 0x0318, 0x6A, _reg_delay_ }, \
    { 0x031A, 0x62, _reg_delay_ }, \
    { 0x030B, 0x01, _reg_delay_ }, \
    { 0x030C, 0x00, _reg_delay_ }, \
    { 0x030D, 0x02, _reg_delay_ }, \
    { 0x030E, 0x00, _reg_delay_ }, \
    { 0x0312, 0x06, _reg_delay_ }, \
    { 0x031D, 0x30, _reg_delay_ }, \
    { 0x031E, 0x30, _reg_delay_ }, \
}

#define INIT_MSM_SERIALIZER_SINGLE_BCAST \
{ \
    { 0x0001, 0x18, _reg_delay_ }, \
    { 0x0330, 0x00, _reg_delay_ }, \
    { 0x0331, 0x33, _reg_delay_ }, \
    { 0x0332, 0xEE, _reg_delay_ }, \
    { 0x0333, 0xE4, _reg_delay_ }, \
    { 0x0311, 0xF0, _reg_delay_ }, \
    { 0x0308, 0x7F, _reg_delay_ }, \
    { 0x0002, 0xF3, _reg_delay_ }, \
    { 0x0314, 0x62, _reg_delay_ }, \
    { 0x0316, 0x6A, _reg_delay_ }, \
    { 0x0318, 0x62, _reg_delay_ }, \
    { 0x031A, 0x62, _reg_delay_ }, \
    { 0x0312, 0x02, _reg_delay_ }, \
    { 0x031D, 0x30, _reg_delay_ }, \
    { 0x010A, 0x0E, _reg_delay_ }, \
    { 0x0102, 0x0E, _reg_delay_ }, \
    { 0x0112, 0x0E, _reg_delay_ }, \
    { 0x011A, 0x0E, _reg_delay_ }, \
}

#endif /* __MAX9296_LIB_H__ */
