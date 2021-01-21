/**
 * @file SensorDriver.h
 *
 * @brief Declaration of SensorDriver camera sensor driver
 *
 * Copyright (c) 2009-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#ifndef __SENSORDRIVER_H_
#define __SENSORDRIVER_H_

/*============================================================================
                        INCLUDE FILES
=========================================================================== */

#include "CameraSensorDeviceInterface.h"
#include "CameraDevice.h"
#include "CameraSensorDriver.h"
#include "CameraMIPICSI2Types.h"
#include "CameraCommonTypes.h"

#include "SensorPlatform.h"
#include "SensorDebug.h"
#include "sensor_types.h"
#include "sensor_lib.h"

#define Q8    0x00000100

#define MAX_EXPOSURE_REGISTERS 64

/*===========================================================================
  Type Definition
===========================================================================*/

class CameraSensorDevice;

class SensorDriver
{
public:
    CameraResult Initialize(CameraSensorDevice* pCameraSensorDevice, sensor_lib_t *pSensorData);
    CameraResult Uninitialize();

    CameraResult InitializeRegisters();

    CameraResult DetectDevice();
    CameraResult DetectDeviceChannels();

    CameraResult PowerOn();
    CameraResult PowerOff();
    CameraResult PowerSuspend();
    CameraResult PowerResume();

    CameraResult StartStream();
    CameraResult StopStream();

    CameraResult SetSensorMode(uint32);
    CameraResult SetOperationalMode(Camera_Sensor_OperationalModeType operationalMode,
            Camera_Size * resolution,
            Camera_Sensor_FrameIntervalConfigType * frameIntervalConfig,
            uint32 * pNewSensorMode,
            float * SensitivityFactor);

    CameraResult ConfigResolution(Camera_Size *);
    CameraResult QueryField(Camera_Sensor_FieldType *);

    CameraResult ConfigParam(Camera_Sensor_ParamConfigType* pParamConfig);
    CameraResult ConfigColorParam(Camera_Sensor_ParamConfigType* pParamConfig);
    CameraResult ConfigExposure(Camera_Sensor_ParamConfigType* pParamConfig);


    SensorPlatform *m_pSensorPlatform;
    CameraSensorDevice* m_pDeviceContext;

    sensor_lib_t *m_pSensorLib;  // sensor data

private:
    uint32 m_CurrentSensorMode;

    char m_szSensorName[50];
};


/*============================================================================
                        Function Prototypes
=========================================================================== */

/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */
/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------------
** Type Definitions
** ------------------------------------------------------------------------ */
/**
 * This enumerates the possible sensor activity states.
 */
typedef enum
{
    POWER_OFF = 0, ///< Sensor is in complete power-down state
    POWER_ON, ///< Sensor is powered on and ready for being ACTIVE
    STANDBY,  ///< Sensor is in low-power standby mode
    ACTIVE,   ///< Sensor is actively outputting frames
} Camera_Sensor_SensorStateType;

/**
 * This structure aggregates other sensor-related structures
 */
typedef struct
{
    /// This is used to indicate the state of the sensor.
    Camera_Sensor_SensorStateType sensorState;

    /// This is a structure containing channels information
    Camera_Sensor_ChannelsInfoType channelsInfo;

    /// This is a structure containing channels information
    Camera_Sensor_SubchannelsInfoType subchannelsInfo;
} CameraSensorInfoType;

typedef struct _SensorDataType
{
    sensor_lib_t *ps_SensorLibData;
} SensorDataType;


/**
 * This structure contains instance data for camera sensor driver objects.
 */
class CameraSensorDevice : public CameraDeviceBase
{

private:
    CameraResult PowerOn();
    CameraResult PowerOff();
    CameraResult Reset();
    CameraResult PowerSuspend();
    CameraResult PowerResume();
    CameraResult DriverUninitialize();
    CameraResult InitializeData();
    CameraResult Detect();

#define CAMERA_SENSOR_DEVICE_FUNCTION(_name_) \
    CameraResult _name_ (const void* pIn, int nInLen, void* pOut, int nOutLen, int* pnOutLenReq)

    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_DetectChannels);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_OutputStart);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_OutputStop);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_Info_Channels);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_Info_Subchannels);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_Info_Csi_Params);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_GetInfoChromatix);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_ConfigMode);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_ConfigResolution);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_QueryField);
    CAMERA_SENSOR_DEVICE_FUNCTION(CCameraSensorDriver_ConfigParam);


    void CameraSensorDriver_DumpChannelData();

public:
    /** Control. Camera drivers implement this method.
     * @see CameraDeviceControl
     */
    virtual CameraResult Control(uint32 uidControl,
            const void* pIn, int nInLen, void* pOut, int nOutLen, int* pnOutLenReq);

    /**
     * Close function pointer. Camera drivers implement this method.
     * @see CameraDeviceClose
     */
    virtual CameraResult Close(void);

    /**
     * RegisterCallback. Camera drivers implement this method.
     * @see CameraDeviceRegisterCallback
     */
    virtual CameraResult RegisterCallback(CameraDeviceCallbackType pfnCallback, void *pClientData);

    CameraSensorDevice(CameraDeviceIDType deviceId, sensor_lib_interface_t* pSensorLibInterface)
    {
        m_deviceId = deviceId;
        m_sensorLibInterface = *pSensorLibInterface;
        std_memset(&m_info, 0, sizeof(m_info));
    };

    CameraResult Initialize();

    void SendCallback(input_message_t* pMsg);

private:
    void Uninitialize();
    CameraResult LoadSensorLibs();
    CameraResult UnloadSensorLibs();

    CameraResult DriverInitialize();

    CameraResult PowerOn_Internal();
    CameraResult PowerOff_Internal();


    /// This is a pointer to the client callback function.
    CameraDeviceCallbackType m_pfnClientCallback;

    /// This is client callback data
    void* m_pClientData;

    /// This is a struct for sensor information.
    CameraSensorInfoType m_info;

    SensorDataType m_SensorData;
    SensorDriver m_SensorDriver;

    sensor_lib_interface_t m_sensorLibInterface;

    CameraDeviceIDType m_deviceId;

};

#endif /* __SENSORDRIVER_H_ */
