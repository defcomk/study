/**
 * @file ov10635.c
 *
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "CameraTypes.h"

#include "ti960_lib.h"
#include "SensorDebug.h"

#if defined(SENSOR_OV10635)

int slave_calculate_exposure(sensor_exposure_info_t* exposure_info)
{
    /* Exposure register value is n
       General Integration time = n*hts*1000/sclk (ms)*/
    /* For OV it is (n*hts*1000/(sclk))/8 ms*/
    /* hts= value in TIMING_HTS registers 0x380C and 0x380D for OV10635 */
    /* This is defined in _CAMTIMING_HTS in the sensor header */
    /* sclk = 96MHz in OV10635.*/
    exposure_info->line_count =(unsigned int)((exposure_info->exposure_time *_SCLK)/(_CAMTIMING_HTS*1000));
    exposure_info->sensor_digital_gain = (unsigned int)(exposure_info->real_gain * _QGAIN);

    SHIGH("fGain=%0.3f, linecnt=%d, expTime=%0.3f ",
            exposure_info->real_gain, exposure_info->line_count, exposure_info->exposure_time);

    return 0;
}

static struct camera_i2c_reg_array ov10635_manual_exp_reg_array[] = ENABLE_MANUAL_EXP;
static struct camera_i2c_reg_array ov10635_auto_exp_reg_array[] = ENABLE_AUTO_EXP;

int slave_get_exposure_config(struct camera_i2c_reg_setting* ov10635_reg, sensor_exposure_info_t* exposure_info)
{
    int rc = 0;

    if (exposure_info->exposure_mode_type == QCARCAM_EXPOSURE_MANUAL)
    {
        unsigned int exposure_time = (exposure_info->line_count << 3);
        unsigned int sensor_digital_gain = exposure_info->sensor_digital_gain;

        char exp_time_l = (char) (exposure_time & 0x000000FF);
        char exp_time_h = (char) ((exposure_time >> 8) & 0x000000FF);
        char gain_l = (char) (sensor_digital_gain & 0x000000FF);
        char gain_h = (char) ((sensor_digital_gain >> 8) & 0x000000FF);

        //Write exposure time and gain for OV 10635 sensor
        //Enable the manual exposure
        ov10635_reg->reg_array = ov10635_manual_exp_reg_array;
        ov10635_reg->size = STD_ARRAY_SIZE(ov10635_manual_exp_reg_array);
        ov10635_reg->addr_type = CAMERA_I2C_WORD_ADDR;
        ov10635_reg->data_type = SENSOR_I2C_DATA_TYPE;
        ov10635_reg->delay = _reg_delay_;
        ov10635_reg->reg_array[0].reg_data = exp_time_h;
        ov10635_reg->reg_array[1].reg_data = exp_time_l;
        ov10635_reg->reg_array[2].reg_data = exp_time_h;
        ov10635_reg->reg_array[3].reg_data = exp_time_l;
        ov10635_reg->reg_array[4].reg_data = gain_h;
        ov10635_reg->reg_array[5].reg_data = gain_l;
        ov10635_reg->reg_array[6].reg_data = gain_h;
        ov10635_reg->reg_array[7].reg_data = gain_l;
    }
    else if (exposure_info->exposure_mode_type == QCARCAM_EXPOSURE_AUTO)
    {
        ov10635_reg->reg_array = ov10635_auto_exp_reg_array;
        ov10635_reg->size = STD_ARRAY_SIZE(ov10635_auto_exp_reg_array);
        ov10635_reg->addr_type = CAMERA_I2C_WORD_ADDR;
        ov10635_reg->data_type = SENSOR_I2C_DATA_TYPE;
        ov10635_reg->delay = _reg_delay_;
    }
    else
    {
        SERR("not supported AEC mode %d", exposure_info->exposure_mode_type);
        rc = -1;
    }

    return rc;
}

#endif //defined(SENSOR_OV10635)
