#ifndef __CAMERACONFIG_H_
#define __CAMERACONFIG_H_

/**
 * @file CameraConfig.h
 *
 * @brief This header file defines the interface for initializing and uninitializing
 * the camera device manager
 *
 * Copyright (c) 2011-2012, 2014, 2016-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include <stdint.h>
#include "CameraDeviceManager.h"
#include "sensor_sdk_common.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */

/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */
#define MAX_NUM_CAMERA_INPUT_DEVS                   6
#define MAX_NUM_CAMERA_I2C_DEVS                     4
#define MAX_CAMERA_DEV_INTR_PINS                    3
#define MAX_CAMERA_DEVICE_NAME                     64
#define PRE_SUF_FIX_SIZE                            6 //reserve 6 bytes for prefix "lib" and suffix ".so"
#define MAX_I2C_DEVICE_NAME                        16
#define CAMERA_SENSOR_GPIO_INVALID                 0x7FFFFFFF

#define CAMERA_BOARD_LIBRARY                       "libais_config.so"
#define GET_CAMERA_CONFIG_INTERFACE                "GetCameraConfigInterface"
#define GET_CAMERA_CONFIG_POWER_INTERFACE          "GetCameraConfigPowerInterface"
#define GET_CAMERA_CONFIG_INIT_DEINIT_INTERFACE    "GetCameraConfigInitDeinitInterface"

#define CAMERA_BOARD_LIBRARY_VERSION               0x310

/* ---------------------------------------------------------------------------
** Type Declarations
** ------------------------------------------------------------------------ */
/**
 * This structure defines GPIO pin used by camera
 */
typedef struct {
    uint32 num;       /**< GPIO pin number */
    uint32 config;    /**< GPIO pin configuration */
} CameraGPIOPinType;

/**
 * This enum describes available I2C port speeds
 */
typedef enum {
    I2C_SPEED_POW_CONSERVE = 0,
    I2C_SPEED_100,
    I2C_SPEED_400,
} I2CSpeedType;

/**
 * This enum describes available I2C port types
 */
typedef enum {
    CAMERA_I2C_TYPE_NONE = 0,
    CAMERA_I2C_TYPE_CCI,
    CAMERA_I2C_TYPE_I2C,
} CameraI2CType;

/**
 * This structure describes I2C Devices
 */
typedef struct {
    CameraI2CType i2ctype;      /**< CCI or I2C port */
    uint32 port_id;             /**< I2C port number */
    CameraGPIOPinType sda_pin;  /**< I2C SDA pin configuration */
    CameraGPIOPinType scl_pin;  /**< I2C SCL pin configuration */
    char i2cDevname[MAX_I2C_DEVICE_NAME];    /**< i2c port used by sensor */
} CameraI2CDeviceType;

/**
 * This structure describes I2C port used by the camera
 */
typedef struct {
    CameraI2CType i2ctype; /**< CCI or I2C port */
    uint32 port_id;             /**< I2C port number */
    I2CSpeedType speed;         /**< I2C port speed */
} CameraI2CPortType;

/**
 * This structure defines the board specific configurable items for a given
 * camera sensor.
 */
typedef struct
{
    CameraDeviceIDType devId;   /**< unique dev id */
    uint32 subdevId;            /**< sub device id in case of multiple devices sharing same driver */

    uint8 csiId;                /**< CSI port */

    CameraGPIOPinType gpioConfig[CAMERA_GPIO_MAX]; /*GPIO Config*/

    struct camera_gpio_intr_cfg_t intr[MAX_CAMERA_DEV_INTR_PINS];

    CameraI2CPortType i2cPort;               /**< I2C port */

} CameraSensorBoardType;

/**
 * This structure defines the board specific configurable items related to the
 * camera subsystem.
 */
typedef struct
{
    /**< Board type info. */
    char boardType[MAX_CAMERA_DEVICE_NAME];

    /**< Camera configurations */
    CameraSensorBoardType camera[MAX_NUM_CAMERA_INPUT_DEVS];

    /**< */
    CameraI2CDeviceType i2c[MAX_NUM_CAMERA_I2C_DEVS];

    /**< i2c server for camera0 and camera1. */
    char i2cFlashDevname[MAX_I2C_DEVICE_NAME];    /**< i2c port used by flash */
} CameraBoardType;

/**
 * This structure holds device driver information
 */
typedef struct
{
    /**
     * The type of the device driver (camera sensor or flash)
     */
    CameraDeviceCategoryType deviceCategory;

    /**
     * The name of the shared object containing the camera device driver.
     */
    char strDeviceLibraryName[MAX_CAMERA_DEVICE_NAME];

    /**
     * Name of the device driver open function
     */
    char strCameraDeviceOpenFn[MAX_CAMERA_DEVICE_NAME];
#ifdef AIS_BUILD_STATIC_CONFIG
    /**
     * Device Open function (for static linking)
     */
    CameraDeviceOpenType pfnCameraDeviceOpen;
#endif
    /**
     * Unique Device driver ID
     */
    CameraDeviceIDType devId;
} CameraDeviceDriverInfoType;


typedef struct _CameraChannelInfoType
{
    /**
     * unique inputId that maps to <devId, srcId>
     */
    uint32 aisInputId;

    /**
     * unique <devId, srcId> of input id
     */
    CameraDeviceIDType devId;
    uint32 srcId;
} CameraChannelInfoType;


/**
 * This structure defines an interface for camera board specific configurations
 * and methods.
 */
typedef struct
{
    /**
     * This method returns a pointer to an array of device drivers which should then
     * be registered with the camera device manager.
     * @param[out] pointer to pointer to const table of camera devices.
     * @param[out] pointer to number of elements in the table of camera devices.
     * @return 0 if Success.
     */
    int (*GetCameraDeviceDriverInfo)(CameraDeviceDriverInfoType const** ppDriverInfo, int* nDrivers);

    /**
     * This method returns the CameraBoardType for the current hardware platform.
     * @return CameraBoardType Camera board configuration information.
     */
    CameraBoardType const* (*GetCameraBoardInfo)(void);

    /**
     * This method returns the Camera Board Config version
     * @return version
     */
    int (*GetCameraConfigVersion)(void);

    /*
     * This method returns the array of channel config info and its
     * size in no. of elements (channels).
     */
    int (*GetCameraChannelInfo)(CameraChannelInfoType const **ppChannelInfo, int *nChannels);
} ICameraConfig;

/*
 * Defined type for method GetCameraConfigInterface.
 */
typedef ICameraConfig const* (*GetCameraConfigInterfaceType)(void);

/*
 * This method is used to retrieve the camera board config interface pointer.
 * @return pointer to the interface.
 */
ICameraConfig const* GetCameraConfigInterface(void);

/**
 * This structure defines an extension to interface for camera board specific configurations
 * and methods.
 */
typedef struct
{
    /**
     * This method returns the Camera Board Config Pre-Power Function
     * @return 0 SUCCESS and -1 otherwise
     */
    int (*CameraSensorPrePowerOn)(void);

    /**
     * This method returns the Camera Board Config Pre-Power Function
     * @return 0 SUCCESS and -1 otherwise
     */
    int (*CameraSensorPostPowerOn)(void);

} ICameraConfigPower;

/*
 * Defined type for method GetCameraConfigPowerInterface.
 */
typedef ICameraConfigPower const* (*GetCameraConfigPowerInterfaceType)(void);

/*
 * This method is used to retrieve the camera board config interface pointer.
 * @return pointer to the interface.
 */
ICameraConfigPower const* GetCameraConfigPowerInterface(void);

typedef struct
{
    /**
     * This method inititates the camera config
     * @return 0 SUCCESS and -1 otherwise
     */
    int (*CameraConfigInit)(void);

    /**
     * This method deinits the camera config
     * @return 0 SUCCESS and -1 otherwise
     */
    int (*CameraConfigDeInit)(void);

} ICameraConfigInitDeinit;

typedef ICameraConfigInitDeinit const* (*GetCameraConfigInitDeinitInterfaceType)(void);


#ifdef __cplusplus
} // extern "C"
#endif  // __cplusplus

#endif // __CAMERADEVICEMANAGERQNX_H_
