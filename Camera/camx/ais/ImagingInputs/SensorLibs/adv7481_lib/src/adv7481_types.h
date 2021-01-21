#ifndef _ADV7481_TYPES_H_
#define _ADV7481_TYPES_H_

/**
 * @file adv7481_types.h
 *
 * @brief defines all adv7481 structures and definitions.
 *
 * Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "sensor_lib.h"


#ifdef __cplusplus
extern "C" {
#endif

/** maximum number of supported port */
#define ADV7481_PORT_NUM       1

/** port timeout time for connection in millisecond */
#define ADV7481_PORT_TIMEOUT   5000


/**
 * SDP signal type of lock/unlock
 */
typedef enum _e_adv7481_sdp_lock_type
{
   ADV7481_SDP_LOCK_TYPE_SD = 0,
   ADV7481_SDP_LOCK_TYPE_H,
   ADV7481_SDP_LOCK_TYPE_V
} e_adv7481_sdp_lock_type;


/**
 * bit field of SDP signal type of lock/unlock
 */
#define ADV7481_SDP_LOCK_TYPE_BF(type) (1 << (type))
#define ADV7481_SDP_LOCK_TYPE_BF_SD    ADV7481_SDP_LOCK_TYPE_BF(ADV7481_SDP_LOCK_TYPE_SD)
#define ADV7481_SDP_LOCK_TYPE_BF_H     ADV7481_SDP_LOCK_TYPE_BF(ADV7481_SDP_LOCK_TYPE_H)
#define ADV7481_SDP_LOCK_TYPE_BF_V     ADV7481_SDP_LOCK_TYPE_BF(ADV7481_SDP_LOCK_TYPE_V)

/** check if SDP signal is locked totally */
#define ADV7481_SDP_IS_LOCKED(x) (((x) & ADV7481_SDP_LOCK_TYPE_BF_SD)          \
                                    && ((x) & ADV7481_SDP_LOCK_TYPE_BF_H)      \
                                    && ((x) & ADV7481_SDP_LOCK_TYPE_BF_V))

/**
 * analog receiver type
 * check the user guide of adv7481 for details
 */
typedef enum _e_adv7481_analog_rx
{
    ADV7481_ANALOG_RX_CVBS_AIN1 = 0,
    ADV7481_ANALOG_RX_CVBS_AIN2 = 1, 
    ADV7481_ANALOG_RX_CVBS_AIN3 = 2, 
    ADV7481_ANALOG_RX_CVBS_AIN4 = 3, 
    ADV7481_ANALOG_RX_CVBS_AIN5 = 4, 
    ADV7481_ANALOG_RX_CVBS_AIN6 = 5, 
    ADV7481_ANALOG_RX_CVBS_AIN7 = 6, 
    ADV7481_ANALOG_RX_CVBS_AIN8 = 7, 

    ADV7481_ANALOG_RX_Y_AIN1_C_AIN2 = 8, 
    ADV7481_ANALOG_RX_Y_AIN3_C_AIN4 = 9, 
    ADV7481_ANALOG_RX_Y_AIN5_C_AIN6 = 10, 
    ADV7481_ANALOG_RX_Y_AIN7_C_AIN8 = 11, 

    ADV7481_ANALOG_RX_Y_AIN1_PB_AIN2_PR_AIN3 = 12, 
    ADV7481_ANALOG_RX_Y_AIN4_PB_AIN5_PR_AIN6 = 13, 

    ADV7481_ANALOG_RX_P_AIN1_N_AIN2 = 14,
    ADV7481_ANALOG_RX_P_AIN3_N_AIN4 = 15,
    ADV7481_ANALOG_RX_P_AIN5_N_AIN6 = 16,
    ADV7481_ANALOG_RX_P_AIN7_N_AIN8 = 17,

} e_adv7481_analog_rx;

/** csi lane/a/b bit mask */
#define ADV7481_CSI_TX_LANE_BMSK  (0xFFFF)
#define ADV7481_CSI_TXA_BMSK      (0x10000)
#define ADV7481_CSI_TXB_BMSK      (0x20000)

/**
 * csi trasmitter type
 * check the user guide of adv7481 for details
 */
typedef enum _e_adv7481_csi_tx
{
    ADV7481_CSI_TXA_LANE_1 = ADV7481_CSI_TXA_BMSK | 1,
    ADV7481_CSI_TXA_LANE_2 = ADV7481_CSI_TXA_BMSK | 2,
    ADV7481_CSI_TXA_LANE_4 = ADV7481_CSI_TXA_BMSK | 4,

    ADV7481_CSI_TXB_LANE_1 = ADV7481_CSI_TXB_BMSK | 1
} e_adv7481_csi_tx;

/** get the lane number from e_adv7481_csi_tx */
#define ADV7481_CSI_TX_LANE(x)    ((x) & ADV7481_CSI_TX_LANE_BMSK)

/**
 * message type
 */
typedef enum _e_adv7481_msg
{
    ADV7481_MSG_SIGNAL_LOST,   /**< signal is lost, cannot be operated */
    ADV7481_MSG_SIGNAL_VALID,  /**< signal is good, can be operated correctly */

} e_adv7481_msg;

/**
 * message structure
 */
typedef struct _s_adv7481_msg
{
    e_adv7481_msg msg; /** message type */
    void *p_data;      /** associated info */

} s_adv7481_msg;

/** type of message callback funtion */
typedef int (*f_callback)(s_adv7481_msg *p_msg, void *p_client);


/**
 * port management structure
 */
typedef struct _s_adv7481_port
{
    uint32              signal_status; /**< signal status */


    f_callback          cb;            /**< notification callback */
    void                *p_client;     /**< associated info of callback */

    uint32              flag;          /**< signal is required or not*/
    CameraSignal        signal;        /**< signal for event of signal status */

    e_adv7481_analog_rx in;            /**< input interface */
    e_adv7481_csi_tx    out;           /**< output interface */

} s_adv7481_port;

/**
 * driver management structure
 */
typedef struct _s_adv7481_drv
{
    s_adv7481_port port[ADV7481_PORT_NUM]; /**< port info */

    CameraSpinLock spinlock;               /**< spinlock between ist thread and other threads */

    //sensor platform info
    sensor_platform_func_table_t sp_func_tbl;
    void *p_sp_ctx;

} s_adv7481_drv;


#ifdef __cplusplus
}
#endif


#endif //_ADV7481_TYPES_H_
