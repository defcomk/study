////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxcslhwinternalois.cpp
///
/// @brief CamxCSL HW Internal implemention for OIS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if ANDROID

#include <media/cam_defs.h>
#include <media/cam_sensor.h>

#include "camxcslhwinternal.h"
#include "camxcslsensordefs.h"
#include "camxmem.h"
#include "camxpacketdefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLHwOisKMDQueryCapability
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CSLHwOisKMDQueryCapability(
    CSLHandle   hIndex)
{
    CSLHwDevice*         pDevice   = &g_CSLHwInstance.CSLInternalKMDDevices[hIndex];
    INT32                status    = -1;
    CamxResult           result    = CamxResultEFailed;
    struct cam_control   ioctlCmd;

    if (NULL == pDevice->pKMDDeviceData)
    {
        pDevice->pKMDDeviceData = static_cast<cam_ois_query_cap_t *>(CAMX_CALLOC(sizeof(cam_ois_query_cap_t)));
        ioctlCmd.op_code        = CAM_QUERY_CAP;
        ioctlCmd.size           = sizeof(cam_ois_query_cap_t);
        ioctlCmd.handle_type    = CAM_HANDLE_USER_POINTER;
        ioctlCmd.reserved       = 0;
        ioctlCmd.handle         = CamX::Utils::VoidPtrToUINT64(pDevice->pKMDDeviceData);

        status = pDevice->deviceOp.Ioctl(pDevice, VIDIOC_CAM_CONTROL, &ioctlCmd);
        if (0 > status)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupCSL, "ioctl failed for fd=%d hIndex %d", pDevice->fd, hIndex);
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupCSL, "ioctl success for fd=%d hIndex %d", pDevice->fd, hIndex);
            // Sensor doesn't support iommu handles so defaulting it to -1 always.
            pDevice->hMapIOMMU.hNonSecureIOMMU      = -1;
            pDevice->hMapIOMMU.hSecureIOMMU         = -1;
            pDevice->hMapCDMIOMMU.hNonSecureIOMMU   = -1;
            pDevice->hMapCDMIOMMU.hSecureIOMMU      = -1;
            // For now no one in UMD interested in KMD data, So later update to the UMD data type properly
            result = CamxResultSuccess;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLHwOisKMDAcquire
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CSLHwOisKMDAcquire(
    CSLHandle            hCSL,
    CSLDeviceHandle*     phDevice,
    INT32                deviceIndex,
    CSLDeviceResource*   pDeviceResourceRequest,
    SIZE_T               numDeviceResources)
{
    CAMX_UNREFERENCED_PARAM(pDeviceResourceRequest);
    CAMX_UNREFERENCED_PARAM(numDeviceResources);

    cam_sensor_acquire_dev*   pAcquireCmd;
    struct cam_control        ioctlCmd;
    CSLHwDevice*              pDevice         = &g_CSLHwInstance.CSLInternalKMDDevices[deviceIndex];
    CSLHandle                 hCSLHwSession   = CSLInvalidHandle;
    CamxResult                result          = CamxResultEFailed;

    hCSLHwSession = CAM_REQ_MGR_GET_HDL_IDX(hCSL);
    CAMX_ASSERT(CSLHwMaxNumSessions > hCSLHwSession);

    pAcquireCmd                 = reinterpret_cast<cam_sensor_acquire_dev *>(CAMX_CALLOC(sizeof(cam_sensor_acquire_dev)));
    if (NULL == pAcquireCmd)
    {
        CAMX_LOG_ERROR(CamxLogGroupCSL, "Out of memory");
        return CamxResultENoMemory;
    }

    pAcquireCmd->session_handle = hCSL;
    pAcquireCmd->device_handle  = CSLInvalidHandle;
    pAcquireCmd->reserved       = 0;
    pAcquireCmd->handle_type    = CAM_HANDLE_USER_POINTER;
    pAcquireCmd->info_handle    = 0;

    ioctlCmd.op_code     = CAM_ACQUIRE_DEV;
    ioctlCmd.size        = sizeof(cam_sensor_acquire_dev);
    ioctlCmd.handle_type = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved    = 0;
    ioctlCmd.handle      = CamX::Utils::VoidPtrToUINT64(pAcquireCmd);

    result = pDevice->deviceOp.Ioctl(pDevice, VIDIOC_CAM_CONTROL, &ioctlCmd);
    if (CamxResultEFailed == result)
    {
        CAMX_LOG_ERROR(CamxLogGroupCSL, "ioctl failed for fd=%d Index %d", pDevice->fd, deviceIndex);
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupCSL, "ioctl success for fd=%d Index %d", pDevice->fd, deviceIndex);
        *phDevice = pAcquireCmd->device_handle;
    }

    if (NULL != pAcquireCmd)
    {
        CAMX_FREE(pAcquireCmd);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSLHwInternalOisUMDQueryCapability
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CSLHwInternalOisUMDQueryCapability(
    INT32   deviceIndex,
    VOID*   pDeviceData,
    SIZE_T  deviceDataSize)
{
    CSLHwDevice* pDevice  = &g_CSLHwInstance.CSLInternalKMDDevices[deviceIndex];
    CamxResult   result   = CamxResultSuccess;

    if ((sizeof(CSLOISCapability) != deviceDataSize) || (NULL == pDevice->pKMDDeviceData))
    {
        result = CamxResultEFailed;
    }
    else
    {
        CamX::Utils::Memcpy(pDeviceData, pDevice->pKMDDeviceData, deviceDataSize);
    }
    return result;

}

CSLHwDeviceOps g_CSLHwDeviceOisOps =
{
    CSLHwInternalDefaultOpen,
    CSLHwInternalDefaultClose,
    CSLHwInternalDefaultIoctl,
    NULL,
    CSLHwOisKMDQueryCapability,
    CSLHwInternalOisUMDQueryCapability,
    CSLHwOisKMDAcquire,
    CSLHwInternalKMDRelease,
    CSLHwInternalKMDStreamOn,
    CSLHwInternalKMDStreamOff,
    CSLHwInternalDefaultSubmit,
    CSLHwInternalKMDAcquireHardware,
    CSLHwInternalKMDReleaseHardware
};

#endif // ANDROID
