#ifndef _ADV7481_HW_H_
#define _ADV7481_HW_H_

/**
 * @file adv7481_hw.h
 *
 * @brief defines all hardware capcities and limitations and wraps some for facilities.
 *        if hardware is changed, all infos should be reviewed again.
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "adv7481_hwio.h"

#ifdef __cplusplus
extern "C" {
#endif


/** i2c slave address */
#define ADV7481_IO_MAP_SLAVE_ADDR                         0xE0
#define ADV7481_SDP_MAP_SLAVE_ADDR                        0xF2
#define ADV7481_CSI_TXA_MAP_SLAVE_ADDR                    0x94
#define ADV7481_CSI_TXB_MAP_SLAVE_ADDR                    0x90


/** possible chip revision id */
#define ADV7481_IO_MAP_CHIP_REV_ES1                       0x2140
#define ADV7481_IO_MAP_CHIP_REV_ES2                       0x2141
#define ADV7481_IO_MAP_CHIP_REV_ES3                       0x2142
#define ADV7481_IO_MAP_CHIP_REV_ES3_1                     0x2143


/** sdp map selection */
#define ADV7481_SDP_MAP_SEL_SDP_MAIN_MAP                  0x0
#define ADV7481_SDP_MAP_SEL_SDP_MAP_1                     0x20
#define ADV7481_SDP_MAP_SEL_SDP_MAP_2                     0x40
#define ADV7481_SDP_MAP_SEL_SDP_RO_MAIN_MAP               0x01
#define ADV7481_SDP_MAP_SEL_SDP_RO_MAP_1                  0x02
#define ADV7481_SDP_MAP_SEL_SDP_RO_MAP_2                  0x03


#ifdef __cplusplus
}
#endif

#endif //_ADV7481_HW_H_
