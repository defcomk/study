/**
 * @file csiphy_i.h
 *
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _CSIPHY_I_H
#define _CSIPHY_I_H

#include <linux/media.h>
#include <media/ais_sensor.h>

#include "CameraDevice.h"
#include "CameraOSServices.h"
#include "CameraQueue.h"
#include "CameraPlatform.h"

#include "CameraMIPICSI2Types.h"

/*-----------------------------------------------------------------------------
 * Type Declarations
 *---------------------------------------------------------------------------*/
class CSIPHYDevice : public CameraDeviceBase
{
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

    static CSIPHYDevice* CSIPHYOpen(CameraDeviceIDType deviceId);

private:
    CSIPHYDevice(uint8 csiphyId);
    CameraResult Init();
    CameraResult DeInit();
    CameraResult Reset();
    CameraResult Config(MIPICsiPhyConfig_t* pMipiCsiCfg);
    CameraResult Start();
    CameraResult Stop();

    static void CsiPhyIST(void* hCSIPHY);
    void ProcessIST();


    uint8 m_csiphyId;

    //kernel fd
    int m_fd;
    struct cam_sensor_acquire_dev m_csiphy_acq_dev;
    struct cam_csiphy_acquire_dev_info m_csiphy_acq_params;

    /* readWrite by caller thread */
    CameraDeviceCallbackType           m_pfnCallBack;
    void*                              m_pClientData;

    MIPICsiPhyConfig_t m_sMipiCsiCfg;
};

#endif /* _CSIPHY_I_H */
