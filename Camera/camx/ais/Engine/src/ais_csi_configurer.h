#ifndef _AIS_CSI_CONFIGURER_H_
#define _AIS_CSI_CONFIGURER_H_

/*!
 * Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ais_configurer.h"
#include "CameraDevice.h"
#include "CameraDeviceManager.h"

typedef enum
{
    CSI_INTERF_INIT = 0,
    CSI_INTERF_STREAMING,
}CSIInterfaceState;

typedef struct
{
    CameraDeviceHandle hCsiPhyHandle;
    CSIInterfaceState eState;
    uint32 nRefCount;
}CSICoreCtxtType;

class AisCSIConfigurer : public AisEngineConfigurer
{
public:
    static AisCSIConfigurer* GetInstance();
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

private:
    static AisCSIConfigurer* m_pCsiPhyConfigurerInstance;

    AisCSIConfigurer()
    {
        mDeviceManagerContext = CameraDeviceManager::GetInstance();

        memset(m_CSIPHYCoreCtxt, 0x0, sizeof(m_CSIPHYCoreCtxt));
        m_nDevices = 0;
    }
    ~AisCSIConfigurer()
    {
        // Implement the destructor here
    }

    static CameraResult CsiPhyDeviceCallback(void* pClientData,
            uint32 uidEvent, int nEventDataLen, void* pEventData);

    CameraDeviceManager* mDeviceManagerContext;

    CSICoreCtxtType m_CSIPHYCoreCtxt[CSIPHY_CORE_MAX];
    uint32 m_nDevices;
};


#endif /* _AIS_CSI_CONFIGURER_H_ */
