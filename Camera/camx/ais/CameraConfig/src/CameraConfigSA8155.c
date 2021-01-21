/**
 * @file CameraConfigSA8155.h
 *
 * @brief SM8155 Camera Board Definition
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include "CameraConfig.h"
#include "CameraPlatform.h"

// PMIC/TLMM pins and TLMM interrupt pins
#if defined(__QNXNTO__) && !defined(CAMERA_UNITTEST)
#include "gpio_devctl.h"
#else
#define GPIO_PIN_CFG(output, pull, strength, func) 0
#endif

/* ===========================================================================
                SA8155 Definitions
=========================================================================== */
/*                   CSID    lanes    I2C Port    Resolution      FPS    Connection
 * max9296_0          0        4     /dev/cci0    1920x1024 x 2    60       <todo>
 * max9296_1          1        4     /dev/cci0    1920x1024 x 2    60       <todo>
 * max9296_2          2        4     /dev/cci1    1920x1024 x 2    60       <todo>
 * max9296_3          3        4     /dev/cci1    1920x1024 x 2    60       <todo>
*/

/* --------------------------------------------------------------------------
** Constant / Define Declarations
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Type Declarations
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Global Object Definitions
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Local Object Definitions
** ----------------------------------------------------------------------- */
static const CameraBoardType cameraBoardDefinition =
{ // Configuration
    .boardType = "SA8155_ADP",
    .i2c =
    {
        {
            .i2cDevname = "cci0",
            .i2ctype = CAMERA_I2C_TYPE_CCI,
            .port_id = 0,
            .sda_pin = {
                .num = 17,
                .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 1),
            },
            .scl_pin = {
                .num = 18,
                .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 1),
            },
        },
        {
            .i2cDevname = "cci1",
            .i2ctype = CAMERA_I2C_TYPE_CCI,
            .port_id = 1,
            .sda_pin = {
                .num = 19,
                .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 1),
            },
            .scl_pin = {
                .num = 20,
                .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 1),
            },
        }
    },
    .camera =
    {
        {
            .devId = CAMERADEVICEID_INPUT_0,
            .subdevId = 0,
            .csiId = 0,
            .gpioConfig = {
                    [CAMERA_GPIO_RESET] = {
                        .num = 21,
                        .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 0),
                    }
            },
            .i2cPort = {
                .i2ctype = CAMERA_I2C_TYPE_CCI,
                .port_id = 0,
            },
            .intr = {
                    {.gpio_id = CAMERA_GPIO_INTR, .pin_id = 13, .intr_type = CAMERA_GPIO_INTR_TLMM,
                            .trigger = CAMERA_GPIO_TRIGGER_FALLING, .gpio_cfg = {0, 0x30, 0, 0}},
                    {.gpio_id = CAMERA_GPIO_INVALID,},
                    {.gpio_id = CAMERA_GPIO_INVALID,},
            }
        },
        {
            .devId = CAMERADEVICEID_INPUT_1,
            .subdevId = 1,
            .csiId = 1,
            .gpioConfig = {
                    [CAMERA_GPIO_RESET] = {
                        .num = 22,
                        .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 0),
                    }
            },
            .i2cPort = {
                .i2ctype = CAMERA_I2C_TYPE_CCI,
                .port_id = 0,
            },
            .intr = {
                    {.gpio_id = CAMERA_GPIO_INTR, .pin_id = 14, .intr_type = CAMERA_GPIO_INTR_TLMM,
                            .trigger = CAMERA_GPIO_TRIGGER_FALLING, .gpio_cfg = {0, 0x30, 0, 0}},
                    {.gpio_id = CAMERA_GPIO_INVALID,},
                    {.gpio_id = CAMERA_GPIO_INVALID,},
            }
        },
        {
            .devId = CAMERADEVICEID_INPUT_2,
            .subdevId = 2,
            .csiId = 2,
            .gpioConfig = {
                    [CAMERA_GPIO_RESET] = {
                        .num = 23,
                        .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 0),
                    }
            },
            .i2cPort = {
                .i2ctype = CAMERA_I2C_TYPE_CCI,
                .port_id = 1,
            },
            .intr = {
                    {.gpio_id = CAMERA_GPIO_INTR, .pin_id = 15, .intr_type = CAMERA_GPIO_INTR_TLMM,
                            .trigger = CAMERA_GPIO_TRIGGER_FALLING, .gpio_cfg = {0, 0x30, 0, 0}},
                    {.gpio_id = CAMERA_GPIO_INVALID,},
                    {.gpio_id = CAMERA_GPIO_INVALID,},
            }
        },
        {
            .devId = CAMERADEVICEID_INPUT_3,
            .subdevId = 3,
            .csiId = 3,
            .gpioConfig = {
                    [CAMERA_GPIO_RESET] = {
                        .num = 25,
                        .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 0),
                    }
            },
            .i2cPort = {
                .i2ctype = CAMERA_I2C_TYPE_CCI,
                .port_id = 1,
            },
            .intr = {
                    {.gpio_id = CAMERA_GPIO_INTR, .pin_id = 16, .intr_type = CAMERA_GPIO_INTR_TLMM,
                            .trigger = CAMERA_GPIO_TRIGGER_FALLING, .gpio_cfg = {0, 0x30, 0, 0}},
                    {.gpio_id = CAMERA_GPIO_INVALID,},
                    {.gpio_id = CAMERA_GPIO_INVALID,},
            }
        },
    },
};

static const CameraChannelInfoType InputDeviceChannelInfo[] =
{
    { /*max9296_0*/
        .aisInputId = 0,
        .devId = CAMERADEVICEID_INPUT_0,
        .srcId = 0,
    },
    { /*max9296_0*/
        .aisInputId = 1,
        .devId = CAMERADEVICEID_INPUT_0,
        .srcId = 1,
    },
    { /*max9296_1*/
        .aisInputId = 2,
        .devId = CAMERADEVICEID_INPUT_1,
        .srcId = 0,
    },
    { /*max9296_1*/
        .aisInputId = 3,
        .devId = CAMERADEVICEID_INPUT_1,
        .srcId = 1,
    },
    { /*max9296_2*/
        .aisInputId = 4,
        .devId = CAMERADEVICEID_INPUT_2,
        .srcId = 0,
    },
    { /*max9296_2*/
        .aisInputId = 5,
        .devId = CAMERADEVICEID_INPUT_2,
        .srcId = 1,
    },
    { /*max9296_3*/
        .aisInputId = 8,
        .devId = CAMERADEVICEID_INPUT_3,
        .srcId = 0,
    },
    { /*max9296_3*/
        .aisInputId = 9,
        .devId = CAMERADEVICEID_INPUT_3,
        .srcId = 1,
    },
};

#ifdef AIS_BUILD_STATIC_DEVICES
extern CameraResult CameraSensorDevice_Open_max9296(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId);
#endif

static const CameraDeviceDriverInfoType InputDevicesInfo[] =
{
    {
        .deviceCategory = CAMERA_DEVICE_CATEGORY_SENSOR,
        .strDeviceLibraryName = "ais_max9296",
        .strCameraDeviceOpenFn = "CameraSensorDevice_Open_max9296",
#ifdef AIS_BUILD_STATIC_DEVICES
        .pfnCameraDeviceOpen = CameraSensorDevice_Open_max9296,
#endif
        .devId = CAMERADEVICEID_INPUT_0,
    },
    {
        .deviceCategory = CAMERA_DEVICE_CATEGORY_SENSOR,
        .strDeviceLibraryName = "ais_max9296",
        .strCameraDeviceOpenFn = "CameraSensorDevice_Open_max9296",
#ifdef AIS_BUILD_STATIC_DEVICES
        .pfnCameraDeviceOpen = CameraSensorDevice_Open_max9296,
#endif
        .devId = CAMERADEVICEID_INPUT_1,
    },
    {
        .deviceCategory = CAMERA_DEVICE_CATEGORY_SENSOR,
        .strDeviceLibraryName = "ais_max9296",
        .strCameraDeviceOpenFn = "CameraSensorDevice_Open_max9296",
#ifdef AIS_BUILD_STATIC_DEVICES
        .pfnCameraDeviceOpen = CameraSensorDevice_Open_max9296,
#endif
        .devId = CAMERADEVICEID_INPUT_2,
    },
    {
        .deviceCategory = CAMERA_DEVICE_CATEGORY_SENSOR,
        .strDeviceLibraryName = "ais_max9296",
        .strCameraDeviceOpenFn = "CameraSensorDevice_Open_max9296",
#ifdef AIS_BUILD_STATIC_DEVICES
        .pfnCameraDeviceOpen = CameraSensorDevice_Open_max9296,
#endif
        .devId = CAMERADEVICEID_INPUT_3,
    },
};

#ifdef AIS_BUILD_STATIC_DEVICES
extern CameraResult CameraSensorDevice_Open_max9296b(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId);
#endif
/* Alcor uses MAX9296B */
static const CameraDeviceDriverInfoType InputDevicesInfo_Alcor[] =
{
    {
        .deviceCategory = CAMERA_DEVICE_CATEGORY_SENSOR,
        .strDeviceLibraryName = "ais_max9296b",
        .strCameraDeviceOpenFn = "CameraSensorDevice_Open_max9296b",
#ifdef AIS_BUILD_STATIC_DEVICES
        .pfnCameraDeviceOpen = CameraSensorDevice_Open_max9296b,
#endif
        .devId = CAMERADEVICEID_INPUT_0,
    },
    {
        .deviceCategory = CAMERA_DEVICE_CATEGORY_SENSOR,
        .strDeviceLibraryName = "ais_max9296b",
        .strCameraDeviceOpenFn = "CameraSensorDevice_Open_max9296b",
#ifdef AIS_BUILD_STATIC_DEVICES
        .pfnCameraDeviceOpen = CameraSensorDevice_Open_max9296b,
#endif
        .devId = CAMERADEVICEID_INPUT_1,
    },
};
/* --------------------------------------------------------------------------
** Forward Declarations
** ----------------------------------------------------------------------- */
static int GetCameraDeviceDriversInfo( CameraDeviceDriverInfoType const **ppDrivers, int *nDrivers);
static CameraBoardType const * GetCameraBoardInfo(void);
static int GetCameraConfigVersion(void);
static int GetCameraChannelInfo(CameraChannelInfoType const **ppChannelInfo, int *nChannels);

const ICameraConfig cameraConfigIf_SA8155 =
{
    GetCameraDeviceDriversInfo,
    GetCameraBoardInfo,
    GetCameraConfigVersion,
    GetCameraChannelInfo
};

/* ===========================================================================
**                          Macro Definitions
** ======================================================================== */

/* ===========================================================================
**                          Local Function Definitions
** ======================================================================== */
// This method returns a pointer to an array of device driver information.
static int GetCameraDeviceDriversInfo(
        CameraDeviceDriverInfoType const **ppDrivers, int *nDrivers)
{
    int rtnVal = -1;
    if (NULL != ppDrivers && NULL != nDrivers)
    {
        if(CameraPlatform_GetPlatformId() == HARDWARE_PLATFORM_ADP_ALCOR)
        {
          *ppDrivers = InputDevicesInfo_Alcor;
          *nDrivers = STD_ARRAY_SIZE(InputDevicesInfo_Alcor);
        }
        else
        {
          *ppDrivers = InputDevicesInfo;
          *nDrivers = STD_ARRAY_SIZE(InputDevicesInfo);
        }
        rtnVal = 0;
    }

    return rtnVal;
}

// This method returns the camera board info.
static CameraBoardType const * GetCameraBoardInfo(void)
{
    return &cameraBoardDefinition;
}

// This method is used to retrieve the camera board version.
static int GetCameraConfigVersion(void)
{
    return CAMERA_BOARD_LIBRARY_VERSION;
}

static int GetCameraChannelInfo(CameraChannelInfoType const **ppChannelInfo, int *nChannels)
{
    int rtnVal = -1;
    if (ppChannelInfo && nChannels)
    {
        *ppChannelInfo = InputDeviceChannelInfo;
        *nChannels     = sizeof(InputDeviceChannelInfo)/sizeof(InputDeviceChannelInfo[0]);
        rtnVal         = 0;
    }

    return rtnVal;
}

