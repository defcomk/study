#ifndef __CAMERADEVICE_H_
#define __CAMERADEVICE_H_

/**
 * @file CameraDevice.h
 *
 * @brief Header file for CameraDevice driver interface.
 *
 * This header file defines the interface for camera hardware device drivers.
 * This interface applies to VFE device driver, camera sensor device drivers,
 * flash driver and other drivers to support camera hardware.
 *
 * Copyright (c) 2009, 2011, 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "CameraResult.h"

/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */
/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */
/// Invalid handle value
#define CAMERA_DEVICE_HANDLE_INVALID NULL

/* ---------------------------------------------------------------------------
** Type Definitions
** ------------------------------------------------------------------------ */
/**
 * Camera device driver identifier type. A unique id is assigned to each
 * camera device driver supported.
 */
typedef uint32 CameraDeviceIDType;

/**
 * Represents an opened camera device.
 */
typedef void* CameraDeviceHandle;

/**
 * Contains the context of device manager
 */
typedef void* CameraDeviceManagerContext;


/**
 * This defines the function signature that the client must implement in order
 * to be called back with data path events.
 *
 * @param[in] pClientData Pointer to client defined data.
 *
 * @param[in] uidEvent The message or event the driver is communicating.
 *
 * @param[in] nEventDataLen Length in bytes of the payload data
 *                          associated with this event.
 *
 * @param[in] pEventData Payload Data associated with the event. The client
 *                       should cast this with appropriate message data
 *                       structure.
 *
 * @return CAMERA_SUCCESS if successful.
 */
typedef CameraResult (* DataPathCallbackType)
(
    void* pClientData,
    uint32 uidEvent,
    int nEventDataLen,
    void* pEventData
);


/// This structure contains information that describes the data path event
/// notifier handle as a DataPathCallbackType.
typedef struct
{
    /// This is a pointer to the client's callback function
    DataPathCallbackType pfnCallback;

    /// This is a pointer to private client data to be passed back in
    /// the callback function.
    void* pClientData;
} DataPathCallbackInfoType;

/**
 * A camera driver calls the CameraDeviceCallbackType method to pass to the
 * client asyncronous messages and events
 *
 * @param[in] pClientData Pointer to client defined data.
 *
 * @param[in] uidEvent The message or event the driver is communicating.
 *
 * @param[in] nEventDataLen Length in bytes of the payload data
 *                          associated with this event.
 *
 * @param[in] pEventData Payload Data associated with the event. The client
 *                       should cast this with appropriate message data
 *                       structure.
 *
 * @return CAMERA_SUCCESS if successful.
 */
typedef DataPathCallbackType CameraDeviceCallbackType;

#ifdef __cplusplus
/**
 * Camera device driver handle.
 *
 * This struct is returned by the creation method of each camera driver and is
 * used to dispatch CameraDeviceControl and CameraDeviceClose method calls.
 */
class CameraDeviceBase
{
protected:
    CameraDeviceBase() = default;
    virtual ~CameraDeviceBase() = default;

public:

    /** Control. Camera drivers implement this method.
     * @see CameraDeviceControl
     */
    virtual CameraResult Control(uint32 uidControl,
            const void* pIn, int nInLen, void* pOut, int nOutLen, int* pnOutLenReq) = 0;

    /**
     * Close function pointer. Camera drivers implement this method.
     * @see CameraDeviceClose
     */
    virtual CameraResult Close(void) = 0;

    /**
     * RegisterCallback. Camera drivers implement this method.
     * @see CameraDeviceRegisterCallback
     */
    virtual CameraResult RegisterCallback(CameraDeviceCallbackType pfnCallback, void *pClientData) = 0;

    CameraDeviceManagerContext hCameraDeviceManagerContext;
};

typedef CameraDeviceBase CameraDeviceHandleType;

#else
/**
 * Camera device driver handle.
 *
 * This struct is returned by the creation method of each camera driver and is
 * used to dispatch CameraDeviceControl and CameraDeviceClose method calls.
 */
typedef struct CameraDeviceHandleType
{
    /** Control function pointer. Camera drivers implemnt this method.
     * @see CameraDeviceControl
     */
    CameraResult (* Control)
    (
        struct CameraDeviceHandleType* pDeviceHandle,
        uint32 uidControl,
        const void* pIn,
        int nInLen,
        void* pOut,
        int nOutLen,
        int* pnOutLenReq
    );

    /**
     * Close function pointer. Camera drivers implement this method.
     * @see CameraDeviceClose
     */
    CameraResult (* Close)
    (
        struct CameraDeviceHandleType* pDeviceHandle
    );

    /**
     * RegisterCallback. Camera drivers implement this method.
     * @see CameraDeviceRegisterCallback
     */
    CameraResult (* RegisterCallback)
    (
        struct CameraDeviceHandleType* pDeviceHandle,
        CameraDeviceCallbackType pfnCallback,
        void *pClientData
    );

    CameraDeviceManagerContext hCameraDeviceManagerContext;
} CameraDeviceHandleType;
#endif

/**
 * A driver should implement its Open method as a CameraDeviceOpenType to
 * minimize the information required for the CameraDeviceManager to open the
 * driver.
 *
 * @param[out] ppNewHandle This pointer is filled with the handle to the new
 *                         camera device. The value CAMERA_DEVICE_HANDLE_INVALID
 *                         is returned on error.
 * @return
 *     CAMERA_SUCCESS if successful.
 */
typedef CameraResult (* CameraDeviceOpenType)
(
    CameraDeviceHandleType** ppNewHandle,
    CameraDeviceIDType deviceId
);


/* ===========================================================================
                        FUNCTION DECLARATIONS
=========================================================================== */
/**
 * This function creates, or opens a camera device driver.
 *
 * @param[in] hDeviceManagerContextIn Handle to the device manager context
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
CameraResult CameraDeviceOpen (
    CameraDeviceManagerContext hDeviceManagerContextIn,
    CameraDeviceIDType deviceId,
    CameraDeviceHandle* ppDeviceHandle);

/**
 * This function sends a command to a device.
 *
 * @param[in] device Open handle to a camera device.
 *
 * @param[in] uidControl I/O control operation to perform.
 * These codes are device-specific and are usually exposed to developers through a header file.
 *
 * @param[in] pIn Pointer to the buffer containing data to transfer to the device.
 *
 * @param[in] nInLen Number of bytes of data in the buffer specified for pBufIn.
 *
 * @param[out] pOut Pointer to the buffer used to transfer the output data from the device.
 *
 * @param[in] nOutLen Maximum number of bytes in the buffer specified by pBufOut.
 *
 * @param[out] pnOutLenReq Used to return the actual number of bytes that can be received from the device.
 *
 * @return CAMERA_SUCCESS if successful.
 */
CameraResult CameraDeviceControl (
    CameraDeviceHandle device,
    uint32 uidControl,
    const void *pIn,
    int nInLen,
    void *pOut,
    int nOutLen,
    int* pnOutLenReq);

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
CameraResult CameraDeviceClose (CameraDeviceHandle device);

/**
 * This function registers a client-specific event notifier handle with the
 * provided camera device. The eventNotifierHandle is interpreted differently
 * for different clients. For example, it can be an event or a callback that is
 * invoked when an event happens in the device. The device may also need to
 * share context with the client, in which case the sharedContext output
 * parameter can be used.  The type of the eventNotifierHandle and
 * sharedContext (if applicable) can be determined from the
 * CameraCoreExt[CLIENT_NAME].h header file in the public API directory.
 *
 * @param[in] hDevice Open handle to a camera device.
 * @param[in] eventNotifierHandle Handle to the event notifier object.
 * @param[in] nEventNotifierHandleLen Length of event notifier handle buffer
 *            in bytes.
 * @param[out] sharedContext Handle to any context that must be shared by the
 *             device with the client in order to notify it of events.
 * @param[in] nSharedContextLen Length of shared context buffer in bytes.
 *
 * @return CAMERA_SUCCESS if successful.
 */
CameraResult CameraDeviceRegisterEventNotifierHandle (
    CameraDeviceHandle hDevice,
    void* eventNotifierHandle,
    int nEventNotifierHandleLen,
    void* sharedContext,
    int nSharedContextLen);

/**
 * NOTE: THIS FUNCTION HAS BEEN DEPRECATED. USE
 * CameraDeviceRegisterEventNotifierHandle() INSTEAD.
 *
 * This function registers a callback function to a device.
 *
 * @param[in] hDevice Open handle to a camera device.
 *
 * @param[in] pfnCallback Pointer to callback method.
 *
 * @param[in] pClientData Pointer to private client data to be passed back in
 *                        callback method.
 *
 * @return CAMERA_SUCCESS if successful.
 */
CameraResult CameraDeviceRegisterCallback (
    CameraDeviceHandle hDevice,
    CameraDeviceCallbackType pfnCallback,
    void *pClientData);

#endif // __CAMERADEVICE_H_
