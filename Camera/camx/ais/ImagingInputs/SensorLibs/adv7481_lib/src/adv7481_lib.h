#ifndef _ADV7481_LIB_H_
#define _ADV7481_LIB_H_

/**
 * @file adv7481_lib.h
 *
 * @brief defines all hardware capcities and limitations and wraps some for facilities.
 *        if hardware is changed, all infos should be reviewed again.
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */


#include "sensor_lib.h"


#define ADV7481_SETTLE_COUNT 0x10

#define DT_ADV7481 0x1e //8 bit yuv422
#define DT_BITDEPTH CSI_DECODE_8BIT


#define CID_VC0        0
#define CID_VC1        4
#define CID_VC2        8
#define CID_VC3        12

#define VC0 0
#define VC1 (1 << 6)
#define VC2 (2 << 6)
#define VC3 (3 << 6)

#define SENSOR_MODEL "adv7481"
#define DEBUG_SENSOR_NAME SENSOR_MODEL

#endif //_ADV7481_LIB_H_
