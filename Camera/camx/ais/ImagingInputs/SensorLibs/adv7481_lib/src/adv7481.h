#ifndef _ADV7481_H_
#define _ADV7481_H_

/**
 * @file adv7481.h
 *
 * @brief defines all APIs of ADV7481 driver module.
 *
 * Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "adv7481_types.h"

#ifdef __cplusplus
extern "C" {
#endif



#define ADV7481_INTR_USE_POLL


/**
 * reset the chip
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_reset(s_adv7481_drv *p_drv);

/**
 * check if the chip is supported by verifying chip id
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_check_chip(s_adv7481_drv *p_drv);

/**
 * config the chip and map sub tables
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_config(s_adv7481_drv *p_drv);


/**
 * configure the chip to enable interrupt to host
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_intr_enable(s_adv7481_drv *p_drv);

/**
 * configure the chip to disable interrupt to host
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_intr_disable(s_adv7481_drv *p_drv);

/**
 * start video on the specified port
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_start(s_adv7481_drv *p_drv, s_adv7481_port *p_port);

/**
 * stop video on the specified port
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_stop(s_adv7481_drv *p_drv, s_adv7481_port *p_port);

/**
 * configure the specified port
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_config(s_adv7481_drv *p_drv, s_adv7481_port *p_port);

/**
 * wait until the specified port is connected and locked
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_wait_ready(s_adv7481_drv *p_drv, s_adv7481_port *p_port);


/**
 * read the field type on the specified port
 *
 * @param p_drv points to the driver management structure
 * @param p_port points to the port management structure
 * @param p_is_event points to the boolean variable indicating field type
 * @return 0: success, otherwise fail
 *
 */
int adv7481_port_read_field_type(s_adv7481_drv *p_drv, s_adv7481_port *p_port,         boolean *p_is_even);

/**
 * handle all interrupts
 *
 * @param p_arg points to the driver management structure
 *
 */
void adv7481_isr(void *p_arg);

/**
 * register a callback function to get notifications for interrupt events
 *
 * @param p_drv points to the driver management structure
 * @param cb points to a callback function
 * @param p_client points to the info for callback function
 * @return 0: success, otherwise fail
 *
 */
int adv7481_register_callback(s_adv7481_drv *p_drv, f_callback cb, void *p_client);

/**
 * register a platform function table. i2c function would be used
 *
 * @param p_drv points to the driver management structure
 * @param p_func_tbl points to a function table
 * @param p_ctx points to the info the functions in the function table would be used
 * @return 0: success, otherwise fail
 *
 */
int adv7481_register_ext_if(s_adv7481_drv *p_drv, sensor_platform_func_table_t *p_func_tbl, void *p_ctx);

/**
 * initialize the chip
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_init(s_adv7481_drv *p_drv);

/**
 * deinitialize the chip
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_deinit(s_adv7481_drv *p_drv);

/**
 * create the management structure of the chip and initialize some HW resource
 *
 * @param pp_drv points to the driver management structure which would be returned
 * @return 0: success, otherwise fail
 *
 */
int adv7481_create(   s_adv7481_drv **pp_drv);

/**
 * destroy the management structure of the chip and free all HW resource
 *
 * @param p_drv points to the driver management structure
 * @return 0: success, otherwise fail
 *
 */
int adv7481_destroy(s_adv7481_drv *p_drv);

#ifdef __cplusplus
}
#endif


#endif //_ADV7481_H_
