#ifndef __CAMERADEVICEMANAGER_H_
#define __CAMERADEVICEMANAGER_H_

/**
 * @file CameraDeviceManager.h
 *
 * @brief Header file for camera device driver manager.
 *
 * This header file defines the interface for initializing and uninitializing
 * the camera device manager
 *
 * Copyright (c) 2009-2014, 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "CameraResult.h"
#include "CameraOSServices.h"

#include "CameraDevice.h"

/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */
/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */
/// The maximum number of camera devices managed by the CameraDeviceManager
#define MAX_CAMERA_DEVICES 12

// Camera Device Categories
/// The don't-care device category
#define CAMERA_DEVICE_CATEGORY_ALL 0

/// Flash device category
#define CAMERA_DEVICE_CATEGORY_FLASH 1

/// Sensor device category
#define CAMERA_DEVICE_CATEGORY_SENSOR 2

/// ISP device category
#define CAMERA_DEVICE_CATEGORY_ISP 3

// Subdevs
#define CAMERA_DEVICE_CATEGORY_IFE 4
#define CAMERA_DEVICE_CATEGORY_CSIPHY 5

/// Camera Platform category
#define CAMERA_DEVICE_CATEGORY_CAMERA_PLATFORM 4


// Because the Camera Device layer does not control specific sensor drivers
// but rather the KMD that contains one or more drivers, we define generic
// IDs here. They are simply the first 32-bits of the corresponding GUID below
// and are only used in the CameraDeviceManager implementation to look up
// the corresponding GUID. "Back" in this instance refers to the sensor facing
// the same direction as the user (i.e. the one on the side of the device
// opposite the display) and "front" refers to the sensor facing toward the
// user (i.e. the one on the side of the device containing the display).
#define CAMERADEVICEID_SENSOR_BACK      0x5e34e1c5
#define CAMERADEVICEID_SENSOR_FRONT     0xf27170b8

#define CAMERADEVICEID_CAMERA_PLATFORM  0x8d73ce35

#define CAMERADEVICEID_FLASH_WHITE_LED  0x0106c427



#define CAMERADEVICEID_IFE_0            0xA1501FE0
#define CAMERADEVICEID_IFE_1            0xA1501FE1
#define CAMERADEVICEID_IFE_2            0xA1501FE2
#define CAMERADEVICEID_IFE_3            0xA1501FE3

#define CAMERADEVICEID_CSIPHY_0         0xA150C510
#define CAMERADEVICEID_CSIPHY_1         0xA150C511
#define CAMERADEVICEID_CSIPHY_2         0xA150C512
#define CAMERADEVICEID_CSIPHY_3         0xA150C513

#define CAMERADEVICEID_INPUT_0          0xA15001D0
#define CAMERADEVICEID_INPUT_1          0xA15001D1
#define CAMERADEVICEID_INPUT_2          0xA15001D2
#define CAMERADEVICEID_INPUT_3          0xA15001D3
#define CAMERADEVICEID_INPUT_4          0xA15001D4
#define CAMERADEVICEID_INPUT_5          0xA15001D5

/* ---------------------------------------------------------------------------
** Type Definitions
** ------------------------------------------------------------------------ */
/**
 * Camera device driver category type. Each driver is assigned a category
 * to make differentiating camera devices simpler.
 */
typedef uint32 CameraDeviceCategoryType;

/**
 * Camera device information.
 *
 * This struct is used to capture the important information related to a
 * camera device
 */
typedef struct
{
    CameraDeviceIDType deviceID;
    CameraDeviceCategoryType deviceCategory;
} CameraDeviceInfoType;

/// This structure is used by the CameraDeviceManager to track the registered
/// CameraDevices
typedef struct
{
    CameraDeviceInfoType info;
    CameraDeviceOpenType pfnCameraDeviceOpen;
} CameraDeviceManagerRegisteredType;

/// This structure is used by the CameraDeviceManager to get static CameraDevices
typedef struct
{
    CameraDeviceManagerRegisteredType* pDevices;
    uint32 nDevices;
} CameraDeviceManagerStaticTableType;

CameraDeviceManagerStaticTableType* CameraDeviceManagerGetStaticsTable(void);

/* --------------------------------------------------------------------------
** Forward Declarations
** ----------------------------------------------------------------------- */
/// Forward declares the context of all registered devices
typedef struct CameraDeviceContexts CameraDeviceContexts;

/* ===========================================================================
                        FUNCTION DECLARATIONS
=========================================================================== */
#ifdef __cplusplus
class CameraDeviceManager
{
public:

    static CameraDeviceManager* GetInstance(void);
    static void DestroyInstance(void);

    /**
     * This function initializes the camera device manager. This method should be
     * called once at startup.
     *
     * @return CAMERA_SUCCESS if successful.
     */
    CameraResult Initialize(void);

    /**
     * This function uninitializes the camera device manager. This method should be
     * called at shutdown.
     *
     * @return CAMERA_SUCCESS if successful.
     */
    CameraResult Uninitialize(void);

    /**
     * This function is used by CameraDevices to register themselves with the
     * CameraDeviceManager.
     *
     * @param[in] pCameraDeviceInfo Information from the CameraDevice
     *                              registering itself.
     * @param[in] pfnCameraDeviceOpen A pointer to the open function of the
     *                                CameraDevice registering itself.
     *
     * @return CAMERA_SUCCESS if successful.
     */
    CameraResult RegisterDevice(
        CameraDeviceInfoType* pCameraDeviceInfo,
        CameraDeviceOpenType pfnCameraDeviceOpen);


    /**
     * This function register all opened devices by the upper layer inside
     * the device manager
     *
     * @param[in] pDeviceContexts Context (sensor, vfe, flash etc..) of
     * currently opened devices.
     *
     * @return CAMERA_SUCCESS if successful.
     */
    CameraResult RegisterDevices(CameraDeviceContexts* pDeviceContexts);

    /**
     * This unregister all currently opened devices.
     *
     * @param[in] hDeviceManagerContextIn The device manager context
     *
     * @return CAMERA_SUCCESS if successful.
     */
    CameraResult ClearDevices(void);

    /**
     * This function gets the available camera devices. If a category is specified,
     * only the camera devices in that category are returned in a list.
     *
     * @param[in] hDeviceManagerContextIn The device manager context
     * @param[in] deviceCategory The category of camera devices to return in the
     *                           list
     * @param[out] pCameraDeviceInfo The list of camera devices returned
     * @param[in] nCameraDeviceInfoLen The length of the list in number of items
     * @param[out] pnCameraDeviceInfoLenReq The actual number of items returned in
     *                                      the list, or the length required
     *
     * @return CAMERA_SUCCESS if successful.
     */
    CameraResult GetAvailableDevices(
        CameraDeviceCategoryType deviceCategory,
        CameraDeviceInfoType* pCameraDeviceInfo,
        int nCameraDeviceInfoLen,
        int* pnCameraDeviceInfoLenReq);


    /**
     * This function creates, or opens a camera device driver.
     *
     * @param[in] deviceId Identifier of the camera device to create or open.
     * @param[out] ppDeviceHandle This pointer is filled with the handle to the
     *                            camera device. If an error occurs, the value
     *                            CAMERA_DEVICE_HANDLE_INVALID is returned.
     *
     * @return
     *  - CAMERA_SUCCESS
     *        If successful.
     *  - CAMERA_ECLASSNOTSUPPORT
     *        If the specified deviceID does not match any of the registered
     *        devices.
     */
    CameraResult DeviceOpen(
        CameraDeviceIDType deviceId,
        CameraDeviceHandle* ppDeviceHandle);

    /**
     * This function closes a handle to a device.
     *
     * When the last handle to a device is closed, the close method is called on the device
     * driver and it is destroyed.
     *
     * @param[in] device Open handle to a camera device.
     *
     * @return CAMERA_SUCCESS if successful.
     */
    CameraResult DeviceClose (CameraDeviceHandle device);

private:
    CameraDeviceManager() = default;

    ~CameraDeviceManager() = default;

    CameraResult RegisterDeviceFromLib(void const * pLibInfo);

    /// Singleton instance of CameraDeviceManager
    static CameraDeviceManager* m_pDeviceManager;

    /// This indicates if context is initialized or not
    uint32 bIsInitialized;

    /// This is the mutex to protect the code for opening a device
    CameraMutex hDeviceMutex;

    /// This is the list of devices currently registered with the
    /// CameraDeviceManager
    CameraDeviceManagerRegisteredType registeredDevices[MAX_CAMERA_DEVICES];

    /// This is the list of dl instances pointers loaded by the camera device
    //manager.
    void* registeredDeviceInstances[MAX_CAMERA_DEVICES];

    /// This is the index of the next available entry in the device list. This is
    /// also exactly the number of devices registered to the CameraDeviceManager
    /// because a CameraDevice cannot unregister itself.
    uint32 nFreeRegisteredDevicesIndex;

    /// This is the index of the next available entry in the device instance list.
    uint32 nFreeDeviceInstancesIndex;

};
#endif

#endif // __CAMERADEVICEMANAGER_H_
