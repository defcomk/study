/**
 * @file CameraSensorDevice.c
 *
 * This file contains the implementation of the CameraDevice API for the
 * Common Camera Sensor Driver.
 *
 * Copyright (c) 2014-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* ========================================================================== */
/*                        INCLUDE FILES                                       */
/* ========================================================================== */
#include "CameraResult.h"
#include "CameraOSServices.h"

#include "AEEstd.h"
#include <string.h>

#include "CameraDeviceManager.h"
#include "CameraSensorDeviceInterface.h"
#include "SensorDriver.h"

/* ========================================================================== */
/*                        DATA DECLARATIONS                                   */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* Constant / Define Declarations                                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Type Declarations                                                          */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Global Object Definitions                                                  */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Local Object Definitions                                                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Forward Declarations                                                       */
/* -------------------------------------------------------------------------- */


/* **************************************************************************
 * Camera Device Interface
 * ************************************************************************** */

/* ========================================================================== */
/*                        MACRO DEFINITIONS                                   */
/* ========================================================================== */

/* ========================================================================== */
/*                        FUNCTION DEFINITIONS                                */
/* ========================================================================== */
extern "C" CameraResult CameraSensorDevice_Open(CameraDeviceHandleType** ppNewHandle,
        CameraDeviceIDType deviceId, sensor_lib_interface_t* pSensorLibInterface)
{
    CameraResult result = CAMERA_SUCCESS;
    CameraSensorDevice* pCameraSensorDevice = NULL;

    CAM_MSG(HIGH, "Opening device 0x%x", deviceId);

    // Sanity check input parameters
    if (ppNewHandle == NULL || pSensorLibInterface == NULL)
    {
        CAM_MSG(ERROR, "null params");
        return CAMERA_EMEMPTR;
    }

    // Allocate instance memory
    pCameraSensorDevice = new CameraSensorDevice(deviceId, pSensorLibInterface);
    if (pCameraSensorDevice != NULL)
    {
        result = pCameraSensorDevice->Initialize();

        if (CAMERA_SUCCESS == result)
        {
            *ppNewHandle = pCameraSensorDevice;
        }
        else
        {
            *ppNewHandle = NULL;
        }
    }
    else
    {
        result = CAMERA_ENOMEMORY;
    }

    CAM_MSG(HIGH, "Open device 0x%x return %d", deviceId, result);

    return result;
} // CameraSensorDevice_Open


/* ---------------------------------------------------------------------------
*    FUNCTION        CameraSensorDevice_LoadSensorLibs
*    DESCRIPTION     Open camera sensor libraries
*    DEPENDENCIES    None
*    PARAMETERS      CameraSensorDevice Pointer
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
 * ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::LoadSensorLibs()
{
    CameraResult result = CAMERA_SUCCESS;
    sensor_open_lib_t device_info;
    CameraPlatformGetInputDeviceIndex(m_deviceId, &device_info.camera_id, &device_info.subdev_id);

    m_SensorData.ps_SensorLibData =
            (sensor_lib_t*) m_sensorLibInterface.sensor_open_lib(
                    &m_SensorDriver,
                    &device_info);

    if (NULL == m_SensorData.ps_SensorLibData)
    {
        // Failed to open the Sensor Data File,
        SENSOR_ERROR("Sensor Data Not Found.");
        result = CAMERA_EUNSUPPORTED;
    }

    return result;
}

/* ---------------------------------------------------------------------------
*    FUNCTION        CameraSensorDevice_UnloadSensorLibs
*    DESCRIPTION     Closes any open camera sensor libraries
*    DEPENDENCIES    None
*    PARAMETERS      CameraSensorDevice Pointer
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
 * ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::UnloadSensorLibs()
{
    CameraResult result = CAMERA_SUCCESS;
    sensor_lib_t* pSensorLib = m_SensorData.ps_SensorLibData;
    if (pSensorLib)
    {
        pSensorLib->sensor_close_lib(pSensorLib);
    }

    m_SensorData.ps_SensorLibData = NULL;

    return result;
}

CameraResult CameraSensorDevice::Initialize()
{
    CameraResult result = CAMERA_SUCCESS;

    m_pfnClientCallback = NULL;
    m_pClientData = NULL;

    result = LoadSensorLibs();
    if (CAMERA_SUCCESS == result)
    {
        result = DriverInitialize();

        if (CAMERA_SUCCESS != result)
        {
            (void)UnloadSensorLibs();
        }
    }

    return result;
}

void CameraSensorDevice::Uninitialize()
{
    (void)DriverUninitialize();

    (void)UnloadSensorLibs();
}

CameraResult CameraSensorDevice::Control(
    uint32 uidControl,
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_SUCCESS;

    // Sanity check input parameters
    if (((NULL == pIn) && nInLen) ||
        ((NULL == pOut) && nOutLen))
    {
        return CAMERA_EMEMPTR;
    }

    switch (uidControl)
    {
    case Camera_Sensor_AEEUID_CTL_STATE_POWER_ON:
        result = PowerOn();
        break;

    case Camera_Sensor_AEEUID_CTL_STATE_POWER_OFF:
        result = PowerOff();
        break;

    case Camera_Sensor_AEEUID_CTL_STATE_RESET:
        result = Reset();
        break;

    case Camera_Sensor_AEEUID_CTL_STATE_POWER_SUSPEND:
        result = PowerSuspend();
        break;

    case Camera_Sensor_AEEUID_CTL_STATE_POWER_RESUME:
        result = PowerResume();
        break;

    case Camera_Sensor_AEEUID_CTL_DETECT_DEVICE:
        result = Detect();
        break;

    case Camera_Sensor_AEEUID_CTL_DETECT_DEVICE_CHANNELS:
        result = CCameraSensorDriver_DetectChannels(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_STATE_FRAME_OUTPUT_START:
        result = CCameraSensorDriver_OutputStart(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_STATE_FRAME_OUTPUT_STOP:
        result = CCameraSensorDriver_OutputStop(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_INFO_CHANNELS:
        result = CCameraSensorDriver_Info_Channels(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_INFO_SUBCHANNELS:
        result = CCameraSensorDriver_Info_Subchannels(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_CSI_INFO_PARAMS:
        result = CCameraSensorDriver_Info_Csi_Params(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_INFO_CHROMATIX:
        result = CCameraSensorDriver_GetInfoChromatix(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_CONFIG_MODE:
        result = CCameraSensorDriver_ConfigMode(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_CONFIG_RESOLUTION:
        result = CCameraSensorDriver_ConfigResolution(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_QUERY_FIELD:
        result = CCameraSensorDriver_QueryField(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    case Camera_Sensor_AEEUID_CTL_CONFIG_SENSOR_PARAMS:
        result = CCameraSensorDriver_ConfigParam(pIn, nInLen, pOut, nOutLen, pnOutLenReq);
        break;

    default:
        return CAMERA_EUNSUPPORTED;
    }

    return result;
}

CameraResult CameraSensorDevice::Close()
{
    CameraResult result = CAMERA_SUCCESS;

    Uninitialize();

    delete this;

    return result;
}

CameraResult CameraSensorDevice::RegisterCallback(
    CameraDeviceCallbackType pfnCallback,
    void* pClientData)
{
    CameraResult result = CAMERA_SUCCESS;

    if (NULL == pfnCallback)
    {
        result = CAMERA_EMEMPTR;
    }
    else
    {
        m_pfnClientCallback = pfnCallback;
        m_pClientData = pClientData;
    }

    return result;
}

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_Initialize
*    DESCRIPTION     Initializes camera sensor driver and hardware
*    DEPENDENCIES    None
*    PARAMETERS      CameraSensorDevice Pointer
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::DriverInitialize()
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");

    result = m_SensorDriver.Initialize(this,
            m_SensorData.ps_SensorLibData);

    if (CAMERA_SUCCESS == result)
    {
        /* Power on the sensor */
        result = PowerOn_Internal();
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_Initialize */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_Uninitialize
*    DESCRIPTION     Uninitializes camera sensor driver and hardware
*    DEPENDENCIES    None
*    PARAMETERS      CameraSensorDevice Pointer
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::DriverUninitialize()
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");

    /* Power off the sensor */
    (void)PowerOff();

    result = m_SensorDriver.Uninitialize();

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_Uninitialize */

/* ===========================================================================
PUBLIC SENSOR CONTROL FUNCTION DEFINITIONS
=========================================================================== */

/* ---------------------------------------------------------------------------
*    FUNCTION        PowerOn
*    DESCRIPTION     Configure PMIC/GPIO to apply power to sensor
*                    Enable clock to sensor
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::PowerOn()
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");

    /* Power on the sensor */
    result = PowerOn_Internal();
    if (CAMERA_SUCCESS != result)
    {
        // PowerOnInternal Failed, Failed to detect the sensor successfully.
        // Return this Camera Sensor unsupported
        SENSOR_ERROR("PowerOnInternal Failed, result %d", result);
        return CAMERA_EUNSUPPORTED;
    }

    if (m_SensorDriver.InitializeRegisters())
    {
        SENSOR_ERROR("InitializeRegisters Failed");
        return CAMERA_EUNSUPPORTED;
    }

    m_info.sensorState = STANDBY;

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* PowerOn */

/* ---------------------------------------------------------------------------
*    FUNCTION        PowerOff
*    DESCRIPTION     Disable clock to sensor
*                    Configure PMIC/GPIO to stop power to sensor
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::PowerOff()
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");

    /* Power off the sensor */
    result = PowerOff_Internal();

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* PowerOff */

/* ---------------------------------------------------------------------------
*    FUNCTION        PowerSuspend
*    DESCRIPTION     Configure PMIC/GPIO to apply power to sensor
*                    Enable clock to sensor
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::PowerSuspend()
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    result = m_SensorDriver.PowerSuspend();
    if (CAMERA_SUCCESS != result)
    {
        SENSOR_ERROR("PowerSuspend Failed");
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* PowerSuspend */

/* ---------------------------------------------------------------------------
*    FUNCTION        PowerResume
*    DESCRIPTION     Configure PMIC/GPIO to apply power to sensor
*                    Enable clock to sensor
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::PowerResume()
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    result = m_SensorDriver.PowerResume();
    if (CAMERA_SUCCESS != result)
    {
        SENSOR_ERROR("PowerResume Failed");
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* PowerResume */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_Detect
*    DESCRIPTION     Determine if the sensor attached is the sensor controlled
*                    by this sensor driver.
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::Detect()
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");

    if (POWER_OFF == m_info.sensorState)
    {
        CAM_MSG(ERROR, "Sensor not powered on");
        return CAMERA_EBADSTATE;
    }

    /* Device detecting */
    if (CAMERA_SUCCESS == m_SensorDriver.DetectDevice())
    {
        SENSOR_DEBUG("Sensor Detection Success");
        result = CAMERA_SUCCESS;
    }
    else
    {
        SENSOR_ERROR("Sensor Detection Failed, Return Unsupported");
        result = CAMERA_EUNSUPPORTED;
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_Detect */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_Detect_Channels
*    DESCRIPTION     Determine if the sensor attached is the sensor controlled
*                    by this sensor driver.
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_DetectChannels (
        const void* pIn, int nInLen,
        void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    (void)pIn;
    (void)nInLen;
    (void)pnOutLenReq;

    SENSOR_FUNCTIONENTRY("");

    if (POWER_OFF == m_info.sensorState)
    {
        CAM_MSG(ERROR, "Sensor not powered on");
        return CAMERA_EBADSTATE;
    }

    /* Device detecting */
    if (CAMERA_SUCCESS == m_SensorDriver.DetectDeviceChannels())
    {
        SENSOR_DEBUG("Sensor Channels Detection Success");
        // Copy connected sensors information to out variable
        if (pOut &&  (sizeof(m_SensorDriver.m_pSensorLib->src_id_enable_mask) == nOutLen))
        {
            memcpy(pOut, &m_SensorDriver.m_pSensorLib->src_id_enable_mask, nOutLen);
        }

        (void)InitializeData();

        result = CAMERA_SUCCESS;
    }
    else
    {
        SENSOR_ERROR("Sensor Detection Failed, Return Unsupported");
        result = CAMERA_EUNSUPPORTED;
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_Detect_Channels */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_OutputStart
*    DESCRIPTION     Enable Straming
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_OutputStart (
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");

    if (m_info.sensorState == STANDBY)
    {
        result = m_SensorDriver.StartStream();
        if (CAMERA_SUCCESS == result)
        {
            m_info.sensorState = ACTIVE;
        }
        else
        {
            SENSOR_ERROR("sensor failed to start streaming");
        }
    }
    else
    {
        SENSOR_ERROR("sensor already streaming");
        result = CAMERA_EALREADY;
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_OutputStart */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_OutputStop
*    DESCRIPTION     Stop Steaming
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_OutputStop (
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");

    if (m_info.sensorState == ACTIVE)
    {
        (void)m_SensorDriver.StopStream();

        m_info.sensorState = STANDBY;

        result = CAMERA_SUCCESS;
    }
    else
    {
        SENSOR_ERROR("sensor already not streaming");
        result = CAMERA_EALREADY;
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_OutputStop */

/* ---------------------------------------------------------------------------
*    FUNCTION        Reset
*    DESCRIPTION     Configure sensor to POR defaults
*                    Reset driver
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::Reset()
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");

    /* Power off the sensor */
    result = PowerOff();
    if (CAMERA_SUCCESS != result) return result;

    /* Power on the sensor */
    result = PowerOn();
    if (CAMERA_SUCCESS != result) return result;

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* Reset */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_Info_Channels
*    DESCRIPTION     Get Channels information
*
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_Info_Channels(
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");

    (void)pIn;
    (void)nInLen;

    if (pnOutLenReq)
    {
        *pnOutLenReq = sizeof(Camera_Sensor_ChannelsInfoType);
    }

    if (pOut)
    {
        if (nOutLen == sizeof (Camera_Sensor_ChannelsInfoType))
        {
            /* Copy the data */
            Camera_Sensor_ChannelsInfoType* pChannelsInfo = (Camera_Sensor_ChannelsInfoType*)pOut;
            *pChannelsInfo = m_info.channelsInfo;
            result = CAMERA_SUCCESS;
        }
        else
        {
            result = CAMERA_EBADPARM;
        }
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_Info_Channels */


/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_Info_Subhannels
*    DESCRIPTION     Get Subchannels information
*
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_Info_Subchannels(
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    (void)pIn;
    (void)nInLen;

    if (pnOutLenReq)
    {
        *pnOutLenReq = sizeof(Camera_Sensor_SubchannelsInfoType);
    }

    if (pOut)
    {
        if (nOutLen == sizeof (Camera_Sensor_SubchannelsInfoType))
        {
            /* Copy the data */
            Camera_Sensor_SubchannelsInfoType* pSubchannelsInfo = (Camera_Sensor_SubchannelsInfoType*)pOut;
            *pSubchannelsInfo = m_info.subchannelsInfo;
            result = CAMERA_SUCCESS;
        }
        else
        {
            result = CAMERA_EBADPARM;
        }
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_Info_Subhannels */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_Info_Csi_Params
*    DESCRIPTION     Get csi parameter information
*
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_Info_Csi_Params (
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    (void)this;
    (void)pIn;
    (void)nInLen;

    if (pnOutLenReq)
    {
        *pnOutLenReq = sizeof(Camera_Sensor_MipiCsiInfoType);
    }

    if (pOut)
    {
        if (nOutLen == sizeof (Camera_Sensor_MipiCsiInfoType))
        {
            /* Populate the data */
            Camera_Sensor_MipiCsiInfoType* pMipiCsiInfo = (Camera_Sensor_MipiCsiInfoType*)pOut;
            pMipiCsiInfo->num_lanes = m_SensorDriver.m_pSensorLib->csi_params.lane_cnt;
            pMipiCsiInfo->lane_mask = m_SensorDriver.m_pSensorLib->csi_params.lane_mask;
            pMipiCsiInfo->settle_count = m_SensorDriver.m_pSensorLib->csi_params.settle_cnt;
            result = CAMERA_SUCCESS;
        }
        else
        {
            result = CAMERA_EBADPARM;
        }
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_Info_Csi_Params */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_GetInfoChromatix
*    DESCRIPTION     Retrieve the chromatix information
*    DEPENDENCIES    None
*    PARAMETERS      CCameraSensorDriver pointer, output pointer, output length,
*                    required output length pointer
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_GetInfoChromatix(
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_EUNSUPPORTED;

    SENSOR_FUNCTIONENTRY("");
#if 0
    result = CAMERA_SUCCESS;

    if (NULL != pnOutLenReq)
    {
        *pnOutLenReq = sizeof(void*);

        if ((NULL != pIn) &&
                (NULL != pOut) &&
                (sizeof(Camera_Sensor_ChromatixConfigType) == nInLen))
        {
            Camera_Sensor_ChromatixConfigType* pChromatixConfig =
                    (Camera_Sensor_ChromatixConfigType*)pIn;

            switch(pChromatixConfig->chromatixType)
            {
            case CAMERA_CHROMATIX_PARAMS:
                if (m_SensorDriver.m_pSensorLib->chromatix_ptrs.p_chromatix)
                {
                    void** pChromatixDataOut = (void**)pOut;
                    *pChromatixDataOut = m_SensorDriver.m_pSensorLib->chromatix_ptrs.p_chromatix;
                }
                break;
            case CAMERA_CHROMATIX_VFE_COMMON_PARAMS:
                if (m_SensorDriver.m_pSensorLib->chromatix_ptrs.p_chromatix_VFE_common)
                {
                    void** pChromatixDataOut = (void**)pOut;
                    if (pChromatixConfig->chromatixTypeVersion !=
                            m_SensorDriver.m_pSensorLib->chromatix_ptrs.p_chromatix_VFE_common->chromatix_version_info.chromatix_version)
                    {
                        // Version mismatch
                        result = CAMERA_EBADPARM;
                    }
                    *pChromatixDataOut = (void*)m_SensorDriver.m_pSensorLib->chromatix_ptrs.p_chromatix_VFE_common;
                }
                break;
            case CAMERA_CHROMATIX_3A_PARAMS:
                if (m_SensorDriver.m_pSensorLib->chromatix_ptrs.p_chromatix_3a)
                {
                    void** pChromatixDataOut = (void**)pOut;
                    if (pChromatixConfig->chromatixTypeVersion !=
                            m_SensorDriver.m_pSensorLib->chromatix_ptrs.p_chromatix_3a->aaa_version.chromatix_version)
                    {
                        // Version mismatch
                        result = CAMERA_EBADPARM;
                    }
                    *pChromatixDataOut = (void*)m_SensorDriver.m_pSensorLib->chromatix_ptrs.p_chromatix_3a;
                }
                break;
            case CAMERA_CHROMATIX_ALL:
            {
                chromatix_ptrs_t* pChromatixDataOut = (chromatix_ptrs_t*)pOut;
                *pChromatixDataOut = m_SensorDriver.m_pSensorLib->chromatix_ptrs;
                break;
            }
            default:
                result = CAMERA_EUNSUPPORTED;
            }
        }
    }
    else
    {
        result = CAMERA_EBADPARM;
    }
#endif
    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_GetInfoChromatix */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_ConfigMode
*    DESCRIPTION     Configure resolution to Sensor
*
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_ConfigMode(
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    if (pIn && nInLen == sizeof (uint32))
    {
        result = m_SensorDriver.SetSensorMode(*((uint32*)pIn));
    }
    else
    {
        result = CAMERA_EBADPARM;
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_ConfigMode */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_ConfigResolution
*    DESCRIPTION     Configure resolution to Sensor
*
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_ConfigResolution(
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    if (pIn && nInLen == sizeof (Camera_Size))
    {
        /* Config Resolution */
        Camera_Size* pResConfig = (Camera_Size*)pIn;
        result = m_SensorDriver.ConfigResolution(pResConfig);
        if (CAMERA_SUCCESS != result)
        {
            CAM_MSG(ERROR, "Failed to set Resolution");
        }
        else
        {
            m_info.subchannelsInfo.subchannels[0].modes[0].res.width = pResConfig->width;
            m_info.subchannelsInfo.subchannels[0].modes[0].res.height = pResConfig->height;
        }
    }
    else
    {
        result = CAMERA_EBADPARM;
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_ConfigResolution */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_QueryField
*    DESCRIPTION     Get Field type from Sensor
*
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_QueryField(
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    if (pOut && nOutLen != sizeof (Camera_Sensor_FieldType))
    {
        Camera_Sensor_FieldType* pFieldType = (Camera_Sensor_FieldType*)pOut;
        result = m_SensorDriver.QueryField(pFieldType);
        if (CAMERA_SUCCESS != result)
        {
            CAM_MSG(ERROR, "Failed to get Field type");
        }
        else
        {
            CAM_MSG(HIGH, "Get Field type: %d %llu", pFieldType->even_field, pFieldType->field_ts);
        }
    }
    else
    {
        result = CAMERA_EBADPARM;
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_ConfigResolution */

/* ---------------------------------------------------------------------------
*    FUNCTION        CCameraSensorDriver_ConfigExposure
*    DESCRIPTION     Set the manuual exposure time and gain
*
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::CCameraSensorDriver_ConfigParam(
    const void* pIn, int nInLen,
    void* pOut, int nOutLen, int* pnOutLenReq)
{
    CameraResult result = CAMERA_SUCCESS;
    (void)pnOutLenReq;
    (void)pOut;
    (void)nOutLen;

    /* Sanity check input parameters */
    if ((NULL == pIn)  ||
        ((NULL != pIn) && (sizeof(Camera_Sensor_ParamConfigType) != nInLen)))
    {
        CAM_MSG(ERROR, "Invalid input");
        return CAMERA_EBADPARM;
    }

    SENSOR_FUNCTIONENTRY("");
    if (POWER_OFF == m_info.sensorState)
    {
        CAM_MSG(ERROR, "Sensor not powered on");
        return CAMERA_EBADSTATE;
    }

    result = m_SensorDriver.ConfigParam((Camera_Sensor_ParamConfigType *)pIn);
    if (result)
    {
        CAM_MSG(ERROR, "Failed to set Exposure Config result = %d",result);
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* CCameraSensorDriver_ConfigParam */

/* ===========================================================================
PRIVATE FUNCTION DEFINITIONS
=========================================================================== */

/* ---------------------------------------------------------------------------
*    FUNCTION        PowerOn_Internal
*    DESCRIPTION     Configure PMIC/GPIO to apply power to sensor
*                    Enable clock to sensor
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::PowerOn_Internal()
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");


    if (POWER_OFF != m_info.sensorState)
    {
        CAM_MSG(HIGH, "Sensor already powered on");
        return CAMERA_SUCCESS; //@todo: should we return CAMERA_EALREADY?
    }

    result = m_SensorDriver.PowerOn();
    if (CAMERA_SUCCESS == result)
    {
        m_info.sensorState = POWER_ON;
    }
    else
    {
        CAM_MSG(ERROR, "Failed to power on sensor");
    }

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* PowerOn_Internal */

/* ---------------------------------------------------------------------------
*    FUNCTION        PowerOff_Internal
*    DESCRIPTION     Disable clock to sensor
*                    Configure PMIC/GPIO to stop power to sensor
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the condition of return
*    SIDE EFFECTS    None
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::PowerOff_Internal()
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    if (POWER_OFF == m_info.sensorState)
    {
        CAM_MSG(HIGH, "Sensor already powered off");
        return CAMERA_SUCCESS; //@todo: should we return CAMERA_EALREADY?
    }

    result = m_SensorDriver.PowerOff();
    if (CAMERA_SUCCESS != result)
    {
        CAM_MSG(ERROR, "Failed to power off sensor");
    }

    m_info.sensorState = POWER_OFF;

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* PowerOff_Internal */

void CameraSensorDevice::CameraSensorDriver_DumpChannelData()
{
    unsigned int i=0, j=0;
    CAM_MSG(HIGH, "Subchannels %d", m_info.subchannelsInfo.num_subchannels);
    for (i=0; i<m_info.subchannelsInfo.num_subchannels; i++)
    {
        CAM_MSG(HIGH, "  [%d]: src_id=%d, num_modes=%d",
                i,
                m_info.subchannelsInfo.subchannels[i].src_id,
                m_info.subchannelsInfo.subchannels[i].num_modes);
        for (j=0; j<m_info.subchannelsInfo.subchannels[i].num_modes; j++)
        {
            CAM_MSG(HIGH, "    (%d): vc/dt/cid=%d/%d/%d, fmt=%d, res=%d x %d, fps=%f",
                            j,
                            m_info.subchannelsInfo.subchannels[i].modes[j].channel_info.vc,
                            m_info.subchannelsInfo.subchannels[i].modes[j].channel_info.dt,
                            m_info.subchannelsInfo.subchannels[i].modes[j].channel_info.cid,
                            m_info.subchannelsInfo.subchannels[i].modes[j].fmt,
                            m_info.subchannelsInfo.subchannels[i].modes[j].res.width,
                            m_info.subchannelsInfo.subchannels[i].modes[j].res.height,
                            m_info.subchannelsInfo.subchannels[i].modes[j].res.fps);
        }
    }

    CAM_MSG(HIGH, "channels %d", m_info.channelsInfo.num_channels);
    for (i=0; i<m_info.channelsInfo.num_channels; i++)
    {
        CAM_MSG(HIGH, "  [%d]: sub=%d vc/dt/cid=%d/%d/%d, fmt=%d, res=%d x %d, fps=%f",
                i,
                m_info.channelsInfo.channels[i].num_subchannels,
                m_info.channelsInfo.channels[i].output_mode.channel_info.vc,
                m_info.channelsInfo.channels[i].output_mode.channel_info.dt,
                m_info.channelsInfo.channels[i].output_mode.channel_info.cid,
                m_info.channelsInfo.channels[i].output_mode.fmt,
                m_info.channelsInfo.channels[i].output_mode.res.width,
                m_info.channelsInfo.channels[i].output_mode.res.height,
                m_info.channelsInfo.channels[i].output_mode.res.fps);
        for (j=0; j<m_info.channelsInfo.channels[i].num_subchannels; j++)
        {
            CAM_MSG(HIGH, "    (%d): src_id=%d, offset=%d x %d",
                            j,
                            m_info.channelsInfo.channels[i].subchan_layout[j].src_id,
                            m_info.channelsInfo.channels[i].subchan_layout[j].x_offset,
                            m_info.channelsInfo.channels[i].subchan_layout[j].y_offset);
        }
    }
}

/* ---------------------------------------------------------------------------
*    FUNCTION        InitializeData
*    DESCRIPTION     Initialize sensor driver's internal variables.
*    DEPENDENCIES    None
*    PARAMETERS      None
*    RETURN VALUE    CameraResult type based on the conditon of return
*    SIDE EFFECTS    None
*    Notes           This function calls the sensor specified InitilaizeData
*                    after the completion of this function if registered,
*                    This provides option for customization for sensor driver
* ------------------------------------------------------------------------ */
CameraResult CameraSensorDevice::InitializeData()
{
    CameraResult result = CAMERA_SUCCESS;

    SENSOR_FUNCTIONENTRY("");

    m_info.channelsInfo.num_channels = m_SensorDriver.m_pSensorLib->num_channels;
    memcpy(m_info.channelsInfo.channels,
            m_SensorDriver.m_pSensorLib->channels,
            m_info.channelsInfo.num_channels*sizeof(m_info.channelsInfo.channels[0]));

    m_info.subchannelsInfo.num_subchannels = m_SensorDriver.m_pSensorLib->num_subchannels;
    memcpy(m_info.subchannelsInfo.subchannels,
            m_SensorDriver.m_pSensorLib->subchannels,
            m_info.subchannelsInfo.num_subchannels*sizeof(m_info.subchannelsInfo.subchannels[0]));

    CameraSensorDriver_DumpChannelData();

    SENSOR_FUNCTIONEXIT("result %d", result);

    return result;
} /* InitializeData */


void CameraSensorDevice::SendCallback(input_message_t* pMsg)
{
    if (m_pfnClientCallback)
    {
        pMsg->device_id = m_deviceId;

        m_pfnClientCallback(m_pClientData,
                INPUT_MSG_ID_LOCK_STATUS,
                sizeof(input_message_t),
                pMsg);
    }
}
