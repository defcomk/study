/**
 * @file
 *          ov490_lib.h
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

#ifndef __OV490_LIB_H__
#define __OV490_LIB_H__

#include "sensor_lib.h"

#define SENSOR_WIDTH (1280)
#define SENSOR_HEIGHT (720)


#define NUM_SUPPORT_SENSORS   1
#define SENSOR_MODEL    "ov490"

#define OV490_SC   (0x0E) 

#define CID_VC0    0
#define CID_VC1    4
#define CID_VC2    8
#define CID_VC3    12

#define VC0  0
#define VC1  (1 << 6)
#define VC2  (2 << 6)
#define VC3  (3 << 6)

#define FMT_OV490   QCARCAM_FMT_UYVY_8
#define DT_OV490 	CSI_DT_YUV422_8
#define DT_BITDEPTH CSI_DECODE_8BIT


/*I2C CONFIGURATION OPTIONS*/
#define _reg_delay_ 0
#define _prefix_reg_delay_ 10

#ifdef OV490_SCCB_SLAVE_BOOTUP_FREERUN
// #define FIRMWARE_BIN_FREERUN_PATH  "/data/misc/camera/firmware_freerun.bin"
#define FIRMWARE_BIN_FREERUN_PATH  "/vendor/firmware/camera/firmware_ispov490.bin"
#define FIRMWARE_READ_BUFF (4*1024)
#define FIRMWARE_LOAD_BUFF (4*1024)
#define FIRMWARE_BOOT_RETRY_TIME (1)

#define OV490_HOSTCMD_STREAM_ON_FOR_FREERUN	\
{ \
    { 0xFFFD, 0x80, _prefix_reg_delay_ }, \
    { 0xFFFE, 0x19, _prefix_reg_delay_ }, \
    { 0x5000, 0x03, _reg_delay_ }, \
    { 0xFFFE, 0x80, _reg_delay_ }, \
    { 0x00C0, 0xE2, _reg_delay_ }, \
}


#define OV490_HOSTCMD_STREAM_OFF_FOR_FREERUN \
{ \
    { 0xFFFD, 0x80, _prefix_reg_delay_ }, \
    { 0xFFFE, 0x19, _prefix_reg_delay_ }, \
    { 0x5000, 0x00, _reg_delay_ }, \
    { 0xFFFE, 0x80, _reg_delay_ }, \
    { 0x00C0, 0xE2, _reg_delay_ }, \
}

#endif /*OV490_SCCB_SLAVE_BOOTUP_FREERUN*/

#define OV490_HOSTCMD_PREFIX_DETECT_CHIPID \
{ \
    { 0xFFFD, 0x80, _prefix_reg_delay_}, \
    { 0xFFFE, 0x80, _prefix_reg_delay_}, \
}

#define OV490_HOSTCMD_PREFIX_REG_ADDR(x,y) \
{ \
    { 0xFFFD, x, _prefix_reg_delay_}, \
    { 0xFFFE, y, _prefix_reg_delay_}, \
}

#define OV490_HOSTCMD_ENABLE_BURST \
{ \
    { 0xFFFD, 0x80, _prefix_reg_delay_}, \
    { 0xFFFE, 0x2a, _prefix_reg_delay_}, \
    { 0x6000, 0x01, _reg_delay_}, \
}

#define OV490_HOSTCMD_ENABLE_CLOCK \
{ \
    {0xFFFD, 0x80, _prefix_reg_delay_}, \
    {0xFFFE, 0x80, _prefix_reg_delay_}, \
    {0x0020, 0x01, _reg_delay_}, \
    {0x0021, 0x16, _reg_delay_}, \
    {0x00a0, 0x1d, _reg_delay_}, \
    {0x00a1, 0xa1, _reg_delay_}, \
    {0x00a2, 0x74, _reg_delay_}, \
    {0x00a3, 0x29, _reg_delay_}, \
}

#define OV490_HOSTCMD_ENABLE_BOOT \
{ \
    {0xFFFD, 0x80, _prefix_reg_delay_}, \
    {0xFFFE, 0x19, _prefix_reg_delay_}, \
    {0xffc4, 0x00, _reg_delay_}, \
    {0xffc5, 0x19, _reg_delay_}, \
    {0xffc6, 0x00, _reg_delay_}, \
    {0xffc7, 0x00, _reg_delay_}, \
}

#define OV490_HOSTCMD_ENABLE_DOWNLOAD_CRC \
{ \
	{0xFFFD, 0x80, _prefix_reg_delay_}, \
	{0xFFFE, 0x19, _prefix_reg_delay_}, \
	{0xffc8, 0xc2, _reg_delay_}, \
}

#define OV490_HOSTCMD_SET_YUV2_FORMAT \
{ \
    { 0x6010, 0x01, _reg_delay_}, \
}

#ifdef OV490_HOTPLUG_FEATURE_ENABLE
/*
Host CMD for enable LOCK :

48 fffd 80;
48 fffe 19;
48 5000 60; //modified by winson
48 5001 00;
48 5002 10; 
48 5003 81;
48 5004 00
48 fffe 80; 
48 00c0 c1;*/

#define OV490_HOSTCMD_ENABLE_LOCK_SIGNAL \
{ \
    {0xFFFD, 0x80, _prefix_reg_delay_}, \
    {0xFFFE, 0x19, _prefix_reg_delay_}, \
    {0x5000, 0x60, _prefix_reg_delay_}, \
    {0x5001, 0x00, _prefix_reg_delay_}, \
    {0x5002, 0x0c, _prefix_reg_delay_}, \
    {0x5003, 0x81, _prefix_reg_delay_}, \
    {0x5004, 0x00, _prefix_reg_delay_}, \
    {0xfffe, 0x80, _prefix_reg_delay_}, \
    {0x00c0, 0xc1, _prefix_reg_delay_}, \
}    

/*
Host CMD for disable LOCK:

48 fffd 80;
48 fffe 19;
48 5000 60; //modified by winson
48 5001 00;
48 5002 10; 
48 5003 80;
48 5004 00
48 fffe 80; 
48 00c0 c1;*/

#define OV490_HOSTCMD_DISABLE_LOCK_SIGNAL \
{ \
	{0xFFFD, 0x80, _prefix_reg_delay_}, \
	{0xFFFE, 0x19, _prefix_reg_delay_}, \
	{0x5000, 0x60, _prefix_reg_delay_}, \
	{0x5001, 0x00, _prefix_reg_delay_}, \
	{0x5002, 0x0c, _prefix_reg_delay_}, \
	{0x5003, 0x80, _prefix_reg_delay_}, \
	{0x5004, 0x00, _prefix_reg_delay_}, \
	{0xfffe, 0x80, _prefix_reg_delay_}, \
	{0x00c0, 0xc1, _prefix_reg_delay_}, \
}  

/*
Only initialize sensor:

48 FFFD 80
48 FFFE 19
48 5000 02;
48 FFFE 80
48 00C0 E2
*/

#define OV490_HOSTCMD_INIT_9284 \
{ \
    {0xFFFD,0x80,_prefix_reg_delay_},\
    {0xFFFE,0x19,_prefix_reg_delay_},\
    {0x5000,0x02,_prefix_reg_delay_},\
    {0xFFFE,0x80,_prefix_reg_delay_},\
    {0x00c0,0xE2,_prefix_reg_delay_},\
}
#endif


/*GPIO CONFIGURATION OPTIONS*/
#define OV490_RESET_PIN		(29)
#define OV490_POWER_PIN     (28)
#define TI936_POWER_PIN		(40)

#ifdef OV490_HOTPLUG_FEATURE_ENABLE
#define TI936_LOCK_PIN      (35)
#define LOCK_SIGNAL_SCAN_TIMES (50)
#define LOCK_SIGNAL_DEBOUNCE_TIMES (5)
#define TI936_LOCK_PIN_LOCKED (1)
#define TI936_LOCK_PIN_LOST   (0)

#define OV490_ISP_HEARTBEAT_HALT (1)
#define OV490_ISP_HEARTBEAT_RUN  (0)
#define OV490_ISP_SETTLE_CNT   (5)
#define OV490_SET_YUYV_FORMAT_BIT   (0x10)  //0x80296010[4] to 1.
#endif

/*utility*/
#define place_marker(_name_) CAM_MSG(HIGH, _name_)
#define ADD_I2C_REG_ARRAY(_a_, _size_, _addr_, _val_, _delay_) \
do { \
    _a_[_size_].reg_addr = _addr_; \
    _a_[_size_].reg_data = _val_; \
    _a_[_size_].delay = _delay_; \
    _size_++; \
} while(0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#endif
