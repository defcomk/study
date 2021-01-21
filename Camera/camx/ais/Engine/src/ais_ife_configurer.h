#ifndef _AIS_IFE_CONFIGURER_H_
#define _AIS_IFE_CONFIGURER_H_

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
    IFE_INTERF_OPENED = 0,
    IFE_INTERF_CONFIG,
    IFE_INTERF_STREAMING,
}IFEInterfaceState;

typedef struct
{
    CameraDeviceHandle hIfeHandle;
    IFEInterfaceState InterfState[IFE_INTF_MAX];
    uint64_t sof_ts_qtime[IFE_INTF_MAX];
}IFECoreCtxtType;

class AisIFEConfigurer : public AisEngineConfigurer
{
public:
    static AisIFEConfigurer* GetInstance();
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

    /**
     * ais_config_ife_queue_buffers
     *
     * @brief Queue user buffers to IFE
     *
     * @param usr_ctxt
     *
     * @return CameraResult
     */
    CameraResult QueueBuffers(AisUsrCtxt* pUsrCtxt);

    /**
     * ais_config_ife_queue_buffer
     *
     * @brief Queue user buffer to IFE
     *
     * @param usr_ctxt
     * @param p_buffer_list
     * @param idx - index of buffer in buffer list
     *
     * @return CameraResult
     */
    CameraResult QueueBuffer(
            AisUsrCtxt* pUsrCtxt,
            AisBufferList* pBufferList,
            uint32 nIdx);

    /**
     * ais_config_ife_queue_input_buffer
     *
     * @brief Queue input user buffer to IFE
     *
     * @param hCtxt - IFE configurer handle
     * @param usr_ctxt
     * @param p_buffer_list
     * @param idx - index of buffer in buffer list
     *
     * @return CameraResult
     */
    CameraResult QueueInputBuffer(
            AisUsrCtxt* pUsrCtxt,
            AisBufferList* pBufferList,
            uint32 nIdx);

private:
    static AisIFEConfigurer* m_pIfeConfigurerInstance;

    AisIFEConfigurer()
    {
        mDeviceManagerContext = CameraDeviceManager::GetInstance();

        m_nDevices = 0;
        memset(m_IFECoreCtxt, 0x0, sizeof(m_IFECoreCtxt));
    }
    ~AisIFEConfigurer()
    {
        // Implement the destructor here
    }

    static CameraResult IfeDeviceCallback(void* pClientData,
            uint32 uidEvent, int nEventDataLen, void* pEventData);

    CameraDeviceManager* mDeviceManagerContext;

    IFECoreCtxtType m_IFECoreCtxt[IFE_CORE_MAX];
    uint32 m_nDevices;
};


#endif /* _AIS_IFE_CONFIGURER_H_ */
