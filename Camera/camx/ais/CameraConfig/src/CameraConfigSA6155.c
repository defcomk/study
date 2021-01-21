/**
 * @file CameraConfigSA6155.h
 *
 * @brief SA6155 Camera Board Definition
 *
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include "CameraConfig.h"

// PMIC/TLMM pins and TLMM interrupt pins
#if defined(__QNXNTO__) && !defined(CAMERA_UNITTEST)
#include "gpio_devctl.h"
#else
#define GPIO_PIN_CFG(output, pull, strength, func) 0
#endif

/* ===========================================================================
                SA6155 Definitions
=========================================================================== */
/*                   CSID    lanes    I2C Port    Resolution      FPS    Connection
 * max9296_0          0        4     /dev/cci0    1920x1024 x 2    60       <todo>
 * max9296_1          1        4     /dev/cci0    1920x1024 x 2    60       <todo>
 * max9296_2          2        4     /dev/cci1    1920x1024 x 2    60       <todo>
 * ov490              1        4     /dev/cci0    1080x720         30       <todo>
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
    .boardType = "SA6155_DIDI_B01",
    .i2c =
    {
        {
            .i2cDevname = "cci0",
            .i2ctype = CAMERA_I2C_TYPE_CCI,
            .port_id = 0,
            .sda_pin = {
                .num = 32,
                .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 1),
            },
            .scl_pin = {
                .num = 33,
                .config = GPIO_PIN_CFG(GPIO_OUTPUT, GPIO_PULL_UP, GPIO_STRENGTH_2MA, 1),
            },
        }
    },
    .camera =
    {
        {
            .devId = CAMERADEVICEID_INPUT_0,
            .subdevId = 0,
            .csiId = 1,
            .i2cPort = {
                .i2ctype = CAMERA_I2C_TYPE_CCI,
                .port_id = 0,
            }
        },
    },
};

static const CameraChannelInfoType InputDeviceChannelInfo[] =
{
    { /*ov490*/
        .aisInputId = 0,
        .devId = CAMERADEVICEID_INPUT_0,
        .srcId = 0,
    },
};

#ifdef AIS_BUILD_STATIC_DEVICES
extern CameraResult CameraSensorDevice_Open_ov490(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId);
#endif

static const CameraDeviceDriverInfoType InputDevicesInfo[] =
{
    {
        .deviceCategory = CAMERA_DEVICE_CATEGORY_SENSOR,
        .strDeviceLibraryName = "ais_ov490",
        .strCameraDeviceOpenFn = "CameraSensorDevice_Open_ov490",
#ifdef AIS_BUILD_STATIC_DEVICES
        .pfnCameraDeviceOpen = CameraSensorDevice_Open_ov490,
#endif
        .devId = CAMERADEVICEID_INPUT_0,
    },
};


/* --------------------------------------------------------------------------
** Forward Declarations
** ----------------------------------------------------------------------- */
static int GetCameraDeviceDriversInfo( CameraDeviceDriverInfoType const **ppDrivers, int *nDrivers);
static CameraBoardType const * GetCameraBoardInfo(void);
static int GetCameraConfigVersion(void);
static int GetCameraChannelInfo(CameraChannelInfoType const **ppChannelInfo, int *nChannels);

const ICameraConfig cameraConfigIf_SA6155 =
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
        *ppDrivers = InputDevicesInfo;
        *nDrivers = sizeof(InputDevicesInfo)/sizeof(InputDevicesInfo[0]);
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

