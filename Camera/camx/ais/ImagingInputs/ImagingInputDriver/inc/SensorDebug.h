/**
 * @file SensorDebug.h
 *
 * @brief Sensor Debug macros
 *
 * Copyright (c) 2012-2017, 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */
#ifndef __SENSORDEBUG_H__
#define __SENSORDEBUG_H__

#include "CameraOSServices.h"

//#define CAMSENSOR_DRIVER_DEBUG
//#define NODEBUG_MSGS
//#define ALLDEBUG_MSGS // Enabling this will cause delay in preview

#ifndef NODEBUG_MSGS
    #define SENSOR_ERRORMSG_LEVEL       ERROR
    #define SENSOR_WARNMSG_LEVEL        ALWAYS
    #define SENSOR_LOGMSG_LEVEL         MEDIUM
    #define SENSOR_HIGHMSG_LEVEL        HIGH
    #define SENSOR_MEDIUMMSG_LEVEL      MEDIUM
    #define SENSOR_LOWMSG_LEVEL         LOW
    #define SENSOR_ENTRYEXITMSG_LEVEL   LOW
#endif

#ifdef CAMSENSOR_DRIVER_DEBUG
    #define SENSOR_DEBUGMSG_LEVEL       ALWAYS
#else
    #define SENSOR_DEBUGMSG_LEVEL       MEDIUM
#endif

#ifdef NODEBUG_MSGS
    #define CAMSENSOR_DEBUG(x, ...)
#else
    #ifdef ALLDEBUG_MSGS
        #define CAMSENSOR_DEBUG(x, ...) CAM_MSG(ALWAYS, ##__VA_ARGS__)
    #else
        #define CAMSENSOR_DEBUG(x, ...) CAM_MSG(x, ##__VA_ARGS__)
    #endif
#endif

// For Debug messages to Identify the Sensor Name
#ifndef DEBUG_SENSOR_NAME
#define DEBUG_SENSOR_NAME "sensor"
#endif

#define SENSOR_MSG(ErrorLevel, fmt, ...) \
    CAMSENSOR_DEBUG(ErrorLevel, \
     "[%s ] " fmt, DEBUG_SENSOR_NAME, ##__VA_ARGS__)

#if 1
#define SENSOR_MSG1 SENSOR_MSG2
#else
#define SENSOR_MSG1(ErrorLevel, fmt1, fmt2, ...) \
    CAMSENSOR_DEBUG(ErrorLevel, \
     "[%s " fmt1 "] " __FILE__ " " fmt2, DEBUG_SENSOR_NAME, ##__VA_ARGS__)
#endif

#define SENSOR_MSG2(ErrorLevel, fmt1, fmt2, ...) \
    CAMSENSOR_DEBUG(ErrorLevel, \
     "[%s " fmt1 "] " fmt2, DEBUG_SENSOR_NAME, ##__VA_ARGS__)

#ifdef SENSOR_ERRORMSG_LEVEL
    #define SENSOR_ERROR(fmt, ...) \
        SENSOR_MSG2(SENSOR_ERRORMSG_LEVEL, "ERROR", fmt, ##__VA_ARGS__);
#else
    #define SENSOR_ERROR(fmt, ...)
#endif

#ifdef SENSOR_WARNMSG_LEVEL
    #define SENSOR_WARN(fmt, ...) \
        SENSOR_MSG2(SENSOR_WARNMSG_LEVEL, "WARN ", fmt, ##__VA_ARGS__);
#else
    #define SENSOR_WARN(fmt, ...)
#endif

#ifdef SENSOR_LOGMSG_LEVEL
    #define SENSOR_LOG(fmt, ...) \
        SENSOR_MSG2(SENSOR_LOGMSG_LEVEL, "LOG  ", fmt, ##__VA_ARGS__);
#else
    #define SENSOR_LOG(fmt, ...)
#endif

#ifdef SENSOR_HIGHMSG_LEVEL
    #define SENSOR_HIGH(fmt, ...) \
        SENSOR_MSG(SENSOR_HIGHMSG_LEVEL, fmt, ##__VA_ARGS__);
#else
    #define SENSOR_HIGH(fmt, ...)
#endif

#ifdef SENSOR_MEDIUMMSG_LEVEL
    #define SENSOR_MED(fmt, ...) \
        SENSOR_MSG(SENSOR_MEDIUMMSG_LEVEL, fmt, ##__VA_ARGS__);
#else
    #define SENSOR_MED(fmt, ...)
#endif

#ifdef SENSOR_LOWMSG_LEVEL
    #define SENSOR_LOW(fmt, ...) \
        SENSOR_MSG(SENSOR_LOWMSG_LEVEL, fmt, ##__VA_ARGS__);
#else
    #define SENSOR_LOW(fmt, ...)
#endif

#ifdef SENSOR_DEBUGMSG_LEVEL
    #define SENSOR_DEBUG(fmt, ...) \
        SENSOR_MSG2(SENSOR_DEBUGMSG_LEVEL, "DEBUG", fmt, ##__VA_ARGS__);
#else
    #define SENSOR_DEBUG(fmt, ...)
#endif

#ifdef SENSOR_ENTRYEXITMSG_LEVEL
    #define SENSOR_FUNCTIONENTRY(fmt, ...) \
        SENSOR_MSG1(SENSOR_ENTRYEXITMSG_LEVEL, "ENTRY","++ "  fmt, ##__VA_ARGS__)
#else
    #define SENSOR_FUNCTIONENTRY(fmt, ...)
#endif

#ifdef SENSOR_ENTRYEXITMSG_LEVEL
    #define SENSOR_FUNCTIONEXIT(fmt, ...) \
        SENSOR_MSG1(SENSOR_ENTRYEXITMSG_LEVEL, "EXIT ","-- " fmt, ##__VA_ARGS__)
#else
    #define SENSOR_FUNCTIONEXIT(fmt, ...)
#endif

// Support For Sensor Library Log messages
#ifndef __func__
#define __func__        __FUNCTION__
#endif

#define SERR SENSOR_ERROR
#define SLOW SENSOR_LOW
#define SHIGH SENSOR_HIGH

#endif /* __SENSORDEBUG_H__ */
