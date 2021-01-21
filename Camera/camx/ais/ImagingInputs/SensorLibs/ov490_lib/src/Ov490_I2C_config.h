/**
 * @file
 *          ov490_i2c_config.h
 *
 * @brief
 *
 * @par Author (last changes):
 *          - Xin Huang
 *          - Phone +86 (0)21 6080 3824
 *          - xin.huang@continental-corporation.com
 *
 * @par Project Leader:
 *          - Pei Yu
 *          - Phone +86 (0)21 6080 3469
 *          - Yu.2.Pei@continental-corporation.com
 *
 * @par Responsible Architect:
 *          - Xin Huang
 *          - Phone +86 (0)21 6080 3824
 *          - xin.huang@continental-corporation.com
 *
 * @par Project:
 *          DiDi
 *
 * @par SW-Component:
 *          bridge driver header file
 *
 * @par SW-Package:
 *          libais_ov490.so
 *
 * @par SW-Module:
 *          AIS Camera HAL
 *
 * @par Description:
 *          Bridge driver  for DiDi board ov490 ISP  in AIS 
 * @note
 *
 * @par Module-History:
 *
 * @verbatim
 *
 *  Date            Author                  Reason
 *  18.03.2019      Xin Huang             Initial version.


 *
 * @endverbatim
 *
 * @par Copyright Notice:
 *
 * Copyright (c) Continental AG and subsidiaries 2015
 * Continental Automotive Holding (Shanghai)
 * Alle Rechte vorbehalten. All Rights Reserved.

 * The reproduction, transmission or use of this document or its contents is
 * not permitted without express written authority.
 * Offenders will be liable for damages. All rights, including rights created
 * by patent grant or registration of a utility model or design, are reserved.
 */


#ifndef __OV490_CONFIG_H__
#define __OV490_CONFIG_H__

#include "Ov490_lib.h"

/*I2C Configuration for OV490*/

#define DUMMY_READ \
{ \
    { 0x0000, 0x00, _reg_delay_ }, \
}

static struct camera_i2c_reg_array read_regs[] = DUMMY_READ;
static struct camera_i2c_reg_setting read_setting =
{
    .reg_array = read_regs,
    .size = ARRAY_SIZE(read_regs),
    .addr_type = CAMERA_I2C_WORD_ADDR,
    .data_type = CAMERA_I2C_BYTE_DATA,
};



#ifdef OV490_SCCB_SLAVE_BOOTUP_FREERUN
static struct camera_i2c_reg_array ov490_start_stream_reg_array[] = OV490_HOSTCMD_STREAM_ON_FOR_FREERUN;
#else
static struct camera_i2c_reg_array ov490_start_stream_reg_array[] = OV490_HOSTCMD_STREAM_ON;
#endif

static struct camera_i2c_reg_setting ov490_start_stream_cmd =
{
    .reg_array = ov490_start_stream_reg_array,
	.size = ARRAY_SIZE(ov490_start_stream_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

#ifdef OV490_SCCB_SLAVE_BOOTUP_FREERUN
static struct camera_i2c_reg_array ov490_stop_stream_reg_array[]= OV490_HOSTCMD_STREAM_OFF_FOR_FREERUN;
#else
static struct camera_i2c_reg_array ov490_start_stream_reg_array[] = OV490_HOSTCMD_STREAM_OFF;
#endif

static struct camera_i2c_reg_setting ov490_stop_stream_cmd = 
{
	.reg_array = ov490_stop_stream_reg_array,
	.size = ARRAY_SIZE(ov490_stop_stream_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array ov490_detect_chipid_reg_array[] = OV490_HOSTCMD_PREFIX_DETECT_CHIPID;
static struct camera_i2c_reg_setting ov490_detect_chipid = 
{
	.reg_array = ov490_detect_chipid_reg_array,
	.size = ARRAY_SIZE(ov490_detect_chipid_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array ov490_enable_burst_reg_array[] = OV490_HOSTCMD_ENABLE_BURST;
static struct camera_i2c_reg_setting ov490_enable_burst =
{
	.reg_array = ov490_enable_burst_reg_array,
	.size = ARRAY_SIZE(ov490_enable_burst_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array ov490_enable_clock_reg_array[] = OV490_HOSTCMD_ENABLE_CLOCK;
static struct camera_i2c_reg_setting ov490_enable_clock =
{
	.reg_array = ov490_enable_clock_reg_array,
	.size = ARRAY_SIZE(ov490_enable_clock_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array ov490_enable_boot_reg_array[] = OV490_HOSTCMD_ENABLE_BOOT;
static struct camera_i2c_reg_setting ov490_enable_boot =
{
	.reg_array = ov490_enable_boot_reg_array,
	.size = ARRAY_SIZE(ov490_enable_boot_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array ov490_enable_download_CRCcheck_reg_array[] = OV490_HOSTCMD_ENABLE_DOWNLOAD_CRC;
static struct camera_i2c_reg_setting ov490_enable_download_CRCcheck = 
{
	.reg_array = ov490_enable_download_CRCcheck_reg_array,
	.size = ARRAY_SIZE(ov490_enable_download_CRCcheck_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array ov490_status_sccb_ready_reg_array[] = OV490_HOSTCMD_PREFIX_REG_ADDR(0x80,0x19);
static struct camera_i2c_reg_setting ov490_status_sccb_ready =
{
	.reg_array = ov490_status_sccb_ready_reg_array,
	.size = ARRAY_SIZE(ov490_status_sccb_ready_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

#ifdef OV490_HOTPLUG_FEATURE_ENABLE
static struct camera_i2c_reg_array ov490_enable_lock_signal_reg_array[] = OV490_HOSTCMD_ENABLE_LOCK_SIGNAL;
static struct camera_i2c_reg_setting ov490_enable_lock_signal = 
{
    .reg_array = ov490_enable_lock_signal_reg_array,
	.size = ARRAY_SIZE(ov490_enable_lock_signal_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array ov490_disable_lock_signal_reg_array[] = OV490_HOSTCMD_DISABLE_LOCK_SIGNAL;
static struct camera_i2c_reg_setting ov490_disable_lock_signal = 
{
    .reg_array = ov490_disable_lock_signal_reg_array,
	.size = ARRAY_SIZE(ov490_disable_lock_signal_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

static struct camera_i2c_reg_array ov490_request_init_sensor_reg_array[] = OV490_HOSTCMD_INIT_9284;
static struct camera_i2c_reg_setting ov490_request_init_sensor =
{
    .reg_array = ov490_request_init_sensor_reg_array,
	.size = ARRAY_SIZE(ov490_request_init_sensor_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};


#endif


static struct camera_i2c_reg_array ov490_status_prefix_8080_reg_array[] = OV490_HOSTCMD_PREFIX_REG_ADDR(0x80,0x80);
static struct camera_i2c_reg_setting ov490_status_prefix_8080 = 
{
	.reg_array = ov490_status_prefix_8080_reg_array,
	.size = ARRAY_SIZE(ov490_status_prefix_8080_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};


static struct camera_i2c_reg_array ov490_status_prefix_8019_reg_array[] = OV490_HOSTCMD_PREFIX_REG_ADDR(0x80,0x19);
static struct camera_i2c_reg_setting ov490_status_prefix_8019 = 
{
	.reg_array = ov490_status_prefix_8019_reg_array,
	.size = ARRAY_SIZE(ov490_status_prefix_8019_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};



static struct camera_i2c_reg_array ov490_status_prefix_8029_reg_array[] = OV490_HOSTCMD_PREFIX_REG_ADDR(0x80,0x29);
static struct camera_i2c_reg_setting ov490_status_prefix_8029 = 
{
	.reg_array = ov490_status_prefix_8029_reg_array,
	.size = ARRAY_SIZE(ov490_status_prefix_8029_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};


static struct camera_i2c_reg_array ov490_set_format_YUYV_reg_array[] = OV490_HOSTCMD_SET_YUV2_FORMAT;
static struct camera_i2c_reg_setting ov490_set_format_YUYV = 
{
	.reg_array = ov490_set_format_YUYV_reg_array,
	.size = ARRAY_SIZE(ov490_set_format_YUYV_reg_array),
	.addr_type = CAMERA_I2C_WORD_ADDR,
	.data_type = CAMERA_I2C_BYTE_DATA,
};

#endif
