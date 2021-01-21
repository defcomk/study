/**
 * @file CameraDeviceManager.c
 *
 * @brief Implementation of CameraDeviceManager and CameraDevice interface.
 *
 * Copyright (c) 2009, 2011, 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>

#include "CameraOSServices.h"
#include "CameraDeviceManager.h"
#include "CameraConfig.h"


/* ===========================================================================
                 DEFINITIONS AND DECLARATIONS FOR MODULE
=========================================================================== */
/* ---------------------------------------------------------------------------
** Constants / Defines
** ------------------------------------------------------------------------ */
#define DYNAMIC_DEVICE_ID      0x7F000000

/* ---------------------------------------------------------------------------
** Type Declarations
** ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------------
** Global Object Definitions
** ------------------------------------------------------------------------ */
#ifdef AIS_BUILD_STATIC_CONFIG
extern ICameraConfig const * GetCameraConfigInterface(void);
#endif

/* ---------------------------------------------------------------------------
** Local Object Definitions
** ------------------------------------------------------------------------ */
///@brief CameraDeviceManager singleton
CameraDeviceManager* CameraDeviceManager::m_pDeviceManager = nullptr;

/* ---------------------------------------------------------------------------
** Forward Declarations
** ------------------------------------------------------------------------ */

/* ===========================================================================
                        FUNCTION DEFINITIONS
=========================================================================== */
CameraDeviceManager* CameraDeviceManager::GetInstance(void)
{
    if(m_pDeviceManager == nullptr)
    {
        m_pDeviceManager = new CameraDeviceManager();
    }

    return m_pDeviceManager;
}

void CameraDeviceManager::DestroyInstance(void)
{
    if(m_pDeviceManager != nullptr)
    {
        delete m_pDeviceManager;
        m_pDeviceManager = nullptr;
    }
}

CameraResult CameraDeviceManager::RegisterDeviceFromLib(void const * pLibInfo)
{
    int result = CAMERA_SUCCESS;
    CameraDeviceOpenType pfnOpen = NULL;

    CameraDeviceDriverInfoType const * pInfo = (CameraDeviceDriverInfoType const *)pLibInfo;

#ifdef AIS_BUILD_STATIC_CONFIG
    pfnOpen = pInfo->pfnCameraDeviceOpen;
#else
    void* hInstance = NULL;
    char strLibName[MAX_CAMERA_DEVICE_NAME + PRE_SUF_FIX_SIZE];

    // Set strLibName to NULL to gurantee NULL terminated string.
    (void) std_memset(strLibName, 0, STD_ARRAY_SIZE(strLibName));

    // Form library name based.
    snprintf(strLibName, sizeof(strLibName), "lib%s.so", pInfo->strDeviceLibraryName);

    hInstance = dlopen(strLibName, RTLD_NOW|RTLD_LOCAL);
    if (hInstance)
    {
        CAM_MSG(HIGH, "Loaded shared object %s", strLibName);
        registeredDeviceInstances[nFreeDeviceInstancesIndex++] = hInstance;
        pfnOpen = (CameraDeviceOpenType)dlsym(hInstance, pInfo->strCameraDeviceOpenFn);
        if (pfnOpen == NULL)
        {
            CAM_MSG(ERROR, "Failed to find symbol %s in shared object %s! Error: %s", pInfo->strCameraDeviceOpenFn, strLibName, dlerror());
            result = CAMERA_EBADHANDLE;
        }
    }
    else
    {
        CAM_MSG(ERROR, "DLL %s Failed To Load! Error: %s", strLibName, dlerror());
        result = CAMERA_EBADHANDLE;
    }
#endif

    // If successfully loaded the device driver register
    // it with the device manager.
    if (CAMERA_SUCCESS == result)
    {
        CameraDeviceInfoType cameraDeviceInfo;
        cameraDeviceInfo.deviceID = pInfo->devId;
        cameraDeviceInfo.deviceCategory = pInfo->deviceCategory;
        result = RegisterDevice(&cameraDeviceInfo, pfnOpen);
    }
    return result;
}

CameraResult CameraDeviceManager::Initialize(void)
{
    CameraResult result = CAMERA_SUCCESS;
    CAM_MSG(MEDIUM, "Initializing CameraDeviceManager");

    int device = 0;
#ifndef AIS_BUILD_STATIC_CONFIG
    void * hSymTable = NULL;
#endif
    GetCameraConfigInterfaceType pfnGetCameraConfigInterface = NULL;
    ICameraConfig const * pCameraConfigIf = NULL;

    CameraDeviceManagerStaticTableType* pStaticDevicesTable = NULL;
    pStaticDevicesTable = CameraDeviceManagerGetStaticsTable();
    if (!pStaticDevicesTable)
    {
        return CAMERA_EFAILED;
    }

    // Create mutex
    result = CameraCreateMutex (&hDeviceMutex);
    if (CAMERA_SUCCESS == result)
    {

        // Initialize all device contexts
        (void) std_memset(registeredDevices, 0, sizeof(registeredDevices));
        (void) std_memset(registeredDeviceInstances, 0, sizeof(registeredDeviceInstances));

        // Initialize the registered device counter
        nFreeRegisteredDevicesIndex = 0;
        nFreeDeviceInstancesIndex = 0;

        // All variables are initialized
        bIsInitialized = 1;

        // Register any static devices
        for (device = 0; device < (int)pStaticDevicesTable->nDevices; ++device)
        {
            RegisterDevice(&pStaticDevicesTable->pDevices[device].info,
                pStaticDevicesTable->pDevices[device].pfnCameraDeviceOpen);
        }

#ifdef AIS_BUILD_STATIC_CONFIG
        pfnGetCameraConfigInterface = GetCameraConfigInterface;
#else
        // Load the camera board shared object.
        hSymTable = dlopen(CAMERA_BOARD_LIBRARY, RTLD_NOW|RTLD_GLOBAL);
        if (NULL != hSymTable)
        {
            // Get entry point for the camera board shared object.
            pfnGetCameraConfigInterface = (GetCameraConfigInterfaceType)dlsym(hSymTable, GET_CAMERA_CONFIG_INTERFACE);
            // Get a handle to the camera board interface
            if (NULL != pfnGetCameraConfigInterface)
            {
#endif
                pCameraConfigIf = pfnGetCameraConfigInterface();
                if (NULL != pCameraConfigIf)
                {
                    CameraDeviceDriverInfoType const * pBoardDevicesInfo;
                    int nBoardDevices = 0;
                    // Retrieve list of dynamic devices
                    if (0 == pCameraConfigIf->GetCameraDeviceDriverInfo(&pBoardDevicesInfo, &nBoardDevices))
                    {
                        // Load and register each device with the device manager.
                        for (device = 0; device < nBoardDevices; ++device)
                        {
                            result = RegisterDeviceFromLib(&pBoardDevicesInfo[device]);
                            if (result != CAMERA_SUCCESS)
                            {
                                // Registering the device from a dynamic library. Library is likely not installed
                                // on the system. There may be good reason for this. We will log the error and
                                // continue on.
                                CAM_MSG(ERROR, "Register dynamic device %s:%s failed!",
                                        pBoardDevicesInfo[device].strDeviceLibraryName, pBoardDevicesInfo[device].strCameraDeviceOpenFn);
                                result = CAMERA_SUCCESS;
                            }
                        }
                    }
                    else
                    {
                        CAM_MSG(ERROR, "Failed to retrieve driver information from the camera config interface.!");
                        result = CAMERA_EFAILED;
                    }
                }
#ifndef AIS_BUILD_STATIC_CONFIG
            }
            else
            {
                CAM_MSG(ERROR, "Failed to retrieve symbol %s.! Error: %s", GET_CAMERA_CONFIG_INTERFACE, dlerror());
                result = CAMERA_EFAILED;
            }
            dlclose(hSymTable);
        }
        else
        {
            CAM_MSG(ERROR, "Failed to retrieve the global symbol table! Error: %s", dlerror());
        }
#endif
    }

    return result;
}

CameraResult CameraDeviceManager::Uninitialize(void)
{
    CameraResult result = CAMERA_SUCCESS;

    if (0 < bIsInitialized)
    {
        bIsInitialized--;
        if (0 == bIsInitialized)
        {
            CAM_MSG(MEDIUM, "Uninitializing CameraDeviceManager");

#ifndef AIS_BUILD_STATIC_DEVICES
            int i = 0;
            // Free any dynamically loaded libraries
            for (i = 0; i < (int)nFreeDeviceInstancesIndex; i++)
            {
                if (NULL != registeredDeviceInstances[i])
                {
                    if (dlclose(registeredDeviceInstances[i]))
                    {
                        CAM_MSG(ERROR, "Free dynamic device library failed! Error: %s", dlerror());
                    }
                    registeredDeviceInstances[i] = NULL;
                }
            }
#endif
            CameraDestroyMutex(hDeviceMutex);
            hDeviceMutex = NULL;

            DestroyInstance();
        }
    }
    else
    {
        result = CAMERA_EBADSTATE;
    }

    return result;
}

CameraResult CameraDeviceManager::RegisterDevice(
        CameraDeviceInfoType* pCameraDeviceInfo,
        CameraDeviceOpenType pfnCameraDeviceOpen)
{
    CameraResult result = CAMERA_SUCCESS;

    if ((pCameraDeviceInfo == NULL) || (pfnCameraDeviceOpen == NULL))
    {
        return CAMERA_EMEMPTR;
    }

    if (0 < bIsInitialized)
    {
        // Synchronize access to registered device array
        CameraLockMutex (hDeviceMutex);

        // Save the device information if there is room in the registered device
        // array
        if (nFreeRegisteredDevicesIndex < MAX_CAMERA_DEVICES)
        {
            // Add the device info to the list of registered devices
            registeredDevices[nFreeRegisteredDevicesIndex].info =
                *pCameraDeviceInfo;
            registeredDevices[nFreeRegisteredDevicesIndex].pfnCameraDeviceOpen =
                pfnCameraDeviceOpen;

            ++nFreeRegisteredDevicesIndex;

            result = CAMERA_SUCCESS;
        }
        else
        {
            CAM_MSG(ERROR, "Registered device list full; device registration failed!");
            result = CAMERA_ENOMORE;
        }

        CameraUnlockMutex (hDeviceMutex);
    }
    else
    {
        result = CAMERA_EBADSTATE;
    }

    return result;
}

CameraResult CameraDeviceManager::RegisterDevices(CameraDeviceContexts* pDeviceContexts)
{
    (void)pDeviceContexts;  //stub
    return CAMERA_SUCCESS;
}

CameraResult CameraDeviceManager::ClearDevices(void)
{
    return CAMERA_SUCCESS;
}


CameraResult CameraDeviceManager::GetAvailableDevices(
    CameraDeviceCategoryType deviceCategory,
    CameraDeviceInfoType* pCameraDeviceInfo,
    int nCameraDeviceInfoLen,
    int* pnCameraDeviceInfoLenReq)
{
    CameraResult result = CAMERA_SUCCESS;
    uint32 nDevice = 0;
    uint32 nDeviceInCategory = 0;

    if (((pCameraDeviceInfo == NULL) && nCameraDeviceInfoLen) ||
        (pnCameraDeviceInfoLenReq == NULL))
    {
        return CAMERA_EMEMPTR;
    }

    if (0 < bIsInitialized)
    {
        *pnCameraDeviceInfoLenReq = 0;

        // Synchronize access to registered device array
        CameraLockMutex(hDeviceMutex);

        // Find the devices in the list with the given category
        for (nDevice = 0; nDevice < nFreeRegisteredDevicesIndex; ++nDevice)
        {
            if ((deviceCategory == CAMERA_DEVICE_CATEGORY_ALL) ||
                (deviceCategory == registeredDevices[nDevice].info.deviceCategory))
            {
                if ((pCameraDeviceInfo != NULL) &&
                    (nDeviceInCategory < (uint32)nCameraDeviceInfoLen))
                {
                    pCameraDeviceInfo[nDeviceInCategory] =
                            registeredDevices[nDevice].info;

                    ++nDeviceInCategory;
                }

                ++(*pnCameraDeviceInfoLenReq);
            }
        }

        CameraUnlockMutex(hDeviceMutex);
    }
    else
    {
        result = CAMERA_EBADSTATE;
    }

    return result;
}

CameraResult CameraDeviceManager::DeviceOpen(CameraDeviceIDType deviceID, CameraDeviceHandle* pNewHandle)
{
    CameraResult result = CAMERA_SUCCESS;

    if ((0 < bIsInitialized) && (NULL != pNewHandle))
    {
        uint32 nDevice;

        // Synchronize Open calls
        CameraLockMutex (hDeviceMutex);

        // Initialize data in case device ID is not found
        *pNewHandle = CAMERA_DEVICE_HANDLE_INVALID;
        result = CAMERA_ECLASSNOTSUPPORT;

        // Search the registered devices for the matching device ID, then open
        // an instance of the device driver.
        for (nDevice = 0; (nDevice < nFreeRegisteredDevicesIndex); ++nDevice)
        {
            if (deviceID == registeredDevices[nDevice].info.deviceID)
            {
                CameraDeviceHandleType* pNewDeviceHandle = NULL;

                // Open the device
                result = registeredDevices[nDevice].pfnCameraDeviceOpen(&pNewDeviceHandle, deviceID);
                if (CAMERA_SUCCESS == result)
                {
                    pNewDeviceHandle->hCameraDeviceManagerContext = (CameraDeviceManagerContext)this;
                    *pNewHandle = (CameraDeviceHandle)pNewDeviceHandle;
                }

                break;
            }
        }

        CameraUnlockMutex (hDeviceMutex);
    }
    else
    {
        result = CAMERA_EBADSTATE;
    }

    return result;
}

CameraResult CameraDeviceControl (
    CameraDeviceHandle hDevice,
    uint32 uidControl,
    const void *pIn,
    int nInLen,
    void *pOut,
    int nOutLen,
    int* pnOutLenReq)
{
    CameraResult result = CAMERA_EBADHANDLE;

    CameraDeviceHandleType* pDeviceHandle =
        (CameraDeviceHandleType*)hDevice;
    if (pDeviceHandle != NULL)
    {
        result = pDeviceHandle->Control(
            uidControl,
            pIn, nInLen,
            pOut, nOutLen, pnOutLenReq);
    }

    return result;
}

CameraResult CameraDeviceManager::DeviceClose(CameraDeviceHandle hDevice)
{
    CameraResult result = CAMERA_EBADHANDLE;

    CameraDeviceHandleType* pDeviceHandle =
        (CameraDeviceHandleType*)hDevice;
    if (pDeviceHandle != NULL && pDeviceHandle->hCameraDeviceManagerContext != NULL)
    {
        CameraLockMutex(hDeviceMutex);
        result = pDeviceHandle->Close();
        CameraUnlockMutex(hDeviceMutex);
    }

    return result;
}


CameraResult CameraDeviceOpen (
    CameraDeviceManagerContext hDeviceManagerContextIn,
    CameraDeviceIDType deviceId,
    CameraDeviceHandle* ppDeviceHandle)
{
    CameraDeviceManager* pCtxt = CameraDeviceManager::GetInstance();

    return pCtxt ? pCtxt->DeviceOpen(deviceId, ppDeviceHandle) : CAMERA_EBADSTATE;
}

CameraResult CameraDeviceClose (CameraDeviceHandle device)
{
    CameraDeviceManager* pCtxt = CameraDeviceManager::GetInstance();

    return pCtxt ? pCtxt->DeviceClose(device) : CAMERA_EBADSTATE;
}

CameraResult CameraDeviceRegisterEventNotifierHandle (
    CameraDeviceHandle hDevice,
    void* eventNotifierHandle,
    int nEventNotifierHandleLen,
    void* sharedContext,
    int nSharedContextLen
    )
{
    CameraResult result = CAMERA_EBADHANDLE;
    CameraDeviceHandleType* pDeviceHandle =
        (CameraDeviceHandleType*)hDevice;

    (void)nEventNotifierHandleLen;
    (void)sharedContext;
    (void)nSharedContextLen;

    if (pDeviceHandle != NULL)
    {
        DataPathCallbackInfoType* pCallbackInfo =
            (DataPathCallbackInfoType*)eventNotifierHandle;
        if (pCallbackInfo != NULL)
        {
            result = pDeviceHandle->RegisterCallback(
                pCallbackInfo->pfnCallback,
                pCallbackInfo->pClientData);
        }
    }

    return result;
}

CameraResult CameraDeviceRegisterCallback (
    CameraDeviceHandle hDevice,
    CameraDeviceCallbackType pfnCallback,
    void *pClientData)
{
    CameraResult result = CAMERA_EBADHANDLE;

    CameraDeviceHandleType* pDeviceHandle =
        (CameraDeviceHandleType*)hDevice;
    if (pDeviceHandle != NULL)
    {
        result = pDeviceHandle->RegisterCallback(
            pfnCallback,
            pClientData);
    }

    return result;
}

