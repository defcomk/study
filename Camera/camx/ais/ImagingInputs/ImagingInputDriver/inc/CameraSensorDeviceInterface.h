#ifndef __CAMERASENSORDEVICEINTERFACE_H_
#define __CAMERASENSORDEVICEINTERFACE_H_

/**
 * @file CameraSensorDeviceInterface.h
 *
 * @brief Camera Sensor Device interface
 *
 * Copyright (c) 2016-2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ========================================================================== */
/*                        INCLUDE FILES                                       */
/* ========================================================================== */
#include "CameraDevice.h"
#include "CameraOSServices.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* ========================================================================== */
/*                        DATA DECLARATIONS                                   */
/* ========================================================================== */

/* ========================================================================== */
/*                        TYPE DECLARATIONS                                   */
/* ========================================================================== */
typedef void* (*SensorOpenLibType)(void* ctrl, void* arg);

typedef struct
{
    SensorOpenLibType sensor_open_lib;
} sensor_lib_interface_t;

/* ========================================================================== */
/*                        FUNCTION DECLARATIONS                               */
/* ========================================================================== */
CAM_API CameraResult CameraSensorDevice_Open(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId, sensor_lib_interface_t* pSensorLibInterface);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif /* __CAMERASENSORDEVICEINTERFACE_H_ */
