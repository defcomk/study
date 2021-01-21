#ifndef _AIS_INPUT_CONFIGURER_H_
#define _AIS_INPUT_CONFIGURER_H_

/*!
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ais_configurer.h"
#include "CameraSensorDriver.h"
#include "CameraPlatform.h"

 /**
  * input get and set parameters
  */
typedef enum
{
    AIS_INPUT_CTRL_INPUT_INTERF, /**< get the input interface */
    AIS_INPUT_CTRL_STATUS,     /**< get the input status */
    AIS_INPUT_CTRL_CSI_PARAM,  /**< get input csi parameters */
    AIS_INPUT_CTRL_MODE_INFO,  /**< get input mode information */
    AIS_INPUT_CTRL_EVENT_NOTIFICATION, /**< set/get input event notification */
    AIS_INPUT_CTRL_CHROMATIX_DATA,
    AIS_INPUT_CTRL_MODE,
    AIS_INPUT_CTRL_RESOLUTION,  /**< set input resolution */
    AIS_INPUT_CTRL_EXPOSURE_CONFIG, /**< set exposure params */
    AIS_INPUT_CTRL_SATURATION_CONFIG, /**< set saturation params */
    AIS_INPUT_CTRL_HUE_CONFIG, /**< set hue params */
    AIS_INPUT_CTRL_FIELD_TYPE, /**< get field type */
    AIS_INPUT_CTRL_MAX
}ais_input_ctrl_t;

//////////////////////////////////////////////////////////////////////////////////
/// MACRO DEFINITIONS
//////////////////////////////////////////////////////////////////////////////////
// max number of CSI input devices
#define MAX_CAMERA_INPUT_DEVICES 5
#define MAX_CAMERA_INPUT_CHANNELS 16


//////////////////////////////////////////////////////////////////////////////////
/// TYPE DEFINITIONS
//////////////////////////////////////////////////////////////////////////////////
/* @todo: Mapping physical imaging inputs to input ID
Input ID    sensor  CID CSI-PHY / CSID
*/
typedef struct
{
    qcarcam_input_desc_t input_id;

    CameraDeviceIDType device_id; /* CameraDevice ID */
    uint32 src_id;

    uint32 dev_id;  /*index into input_devices[]*/
    boolean available;
}InputMappingType;

typedef enum
{
    AIS_INPUT_STATE_UNAVAILABLE = 0,
    AIS_INPUT_STATE_DETECTED,
    AIS_INPUT_STATE_OFF,
    AIS_INPUT_STATE_ON,
    AIS_INPUT_STATE_STREAMING,
}InputStateType;

/**
 * Input interface description
 */
typedef struct
{
    qcarcam_input_desc_t input_id;

    uint32 devId; /**< input device idx */
    uint32 srcId; /**< input device src */

    uint32 cid;     /**< CSI channel id */
    uint32 csid;    /**< CSID core */
    uint32 csiphy;  /**< CSIPHY core */

    boolean interlaced;
}ais_input_interface_t;

typedef struct
{
    CameraDeviceIDType device_id;

    CameraDeviceHandle hDevice;
    CameraDeviceInfoType sDeviceInfo;

    Camera_Sensor_ChannelsInfoType    channelsInfo;
    Camera_Sensor_SubchannelsInfoType subchannelsInfo;
    Camera_Sensor_MipiCsiInfoType     csiInfo;

    uint32 csid;
    uint32 csiphy;
    boolean available;

    uint8 refcnt; //Track users of input
    InputStateType state;
    uint32 src_id_enable_mask;
}InputDeviceType;

class AisInputConfigurer : public AisEngineConfigurer
{
public:
    static AisInputConfigurer* GetInstance();
    static void       DestroyInstance();

    virtual CameraResult Init(void);
    virtual CameraResult Deinit(void);
    virtual CameraResult PowerSuspend(void);
    virtual CameraResult PowerResume(void);
    virtual CameraResult Config(AisUsrCtxt* pUsrCtxt);
    virtual CameraResult Start(AisUsrCtxt* pUsrCtxt);
    virtual CameraResult Stop(AisUsrCtxt* pUsrCtxt);
    virtual CameraResult Resume(AisUsrCtxt* pUsrCtxt);
    virtual CameraResult Pause(AisUsrCtxt* pUsrCtxt);
    virtual CameraResult SetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param);
    virtual CameraResult GetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param);

    CameraResult DetectAll();
    CameraResult QueryInputs(qcarcam_input_t* p_inputs, unsigned int size,
            unsigned int* ret_size);
    CameraResult IsInputAvailable(uint32 input_id);

    CameraResult GetInterface(ais_input_interface_t* interf);
private:
    static AisInputConfigurer* m_pInputConfigurerInstance;

    AisInputConfigurer()
    {
        mDeviceManagerContext = CameraDeviceManager::GetInstance();

        memset(m_InputDevices, 0x0, sizeof(m_InputDevices));
        m_nInputDevices = 0;

        memset(m_InputMappingTable, 0x0, sizeof(m_InputMappingTable));
        m_nInputMapping = 0;
    }

    ~AisInputConfigurer()
    {
    }

    CameraResult GetModeInfo(AisUsrCtxt* pUsrCtxt, uint32 mode, ais_input_mode_info_t* pModeInfo);
    uint32 GetDeviceId(uint32 device_id);
    CameraResult ValidateInputId(AisUsrCtxt* hndl);

    static CameraResult InputDeviceCallback(void* pClientData,
            uint32 uidEvent, int nEventDataLen, void* pEventData);


    CameraDeviceManager* mDeviceManagerContext;
    InputDeviceType m_InputDevices[MAX_CAMERA_INPUT_DEVICES];
    uint32 m_nInputDevices;

    InputMappingType m_InputMappingTable[MAX_CAMERA_INPUT_CHANNELS];
    uint32 m_nInputMapping;
};


#endif /* _AIS_INPUT_CONFIGURER_H_ */
