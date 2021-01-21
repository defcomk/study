/*!
 * Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ais_csi_configurer.h"

#include "CameraMIPICSI2Types.h"

//////////////////////////////////////////////////////////////////////////////////
/// MACRO DEFINITIONS
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
/// TYPE DEFINITIONS
//////////////////////////////////////////////////////////////////////////////////

///@brief AisEngine singleton
AisCSIConfigurer* AisCSIConfigurer::m_pCsiPhyConfigurerInstance = nullptr;

/***************************************************************************************************************************
*   AisCSIConfigurer::GetInstance
*
*   @brief
*       Gets the singleton instance for ChiDevice
***************************************************************************************************************************/
AisCSIConfigurer* AisCSIConfigurer::GetInstance()
{
    if(m_pCsiPhyConfigurerInstance == nullptr)
    {
        m_pCsiPhyConfigurerInstance = new AisCSIConfigurer();
    }

    return m_pCsiPhyConfigurerInstance;
}

/***************************************************************************************************************************
*   AisCSIConfigurer::DestroyInstance
*
*   @brief
*       Destroy the singleton instance of the AisCSIConfigurer class
*
*   @return
*       void
***************************************************************************************************************************/
void AisCSIConfigurer::DestroyInstance()
{
    if(m_pCsiPhyConfigurerInstance != nullptr)
    {
        delete m_pCsiPhyConfigurerInstance;
        m_pCsiPhyConfigurerInstance = nullptr;
    }
}



CameraResult AisCSIConfigurer::CsiPhyDeviceCallback(void* pClientData,
        uint32 uidEvent, int nEventDataLen, void* pEventData)
{
    CameraResult rc = CAMERA_SUCCESS;
    AisCSIConfigurer* pCsiCtxt = (AisCSIConfigurer*)pClientData;

    CAM_MSG(ERROR, "Received CSI %p callback %d", pCsiCtxt, uidEvent);


    return rc;
}

CameraResult AisCSIConfigurer::Init(void)
{
    CameraResult rc = CAMERA_SUCCESS;
    CameraDeviceInfoType cameraDeviceInfo[CSIPHY_CORE_MAX];
    int nCameraDeviceInfoLenReq;
    uint32 nDevice;

    memset(cameraDeviceInfo, 0, sizeof(cameraDeviceInfo));
    rc = mDeviceManagerContext->GetAvailableDevices(
        CAMERA_DEVICE_CATEGORY_CSIPHY,
        &cameraDeviceInfo[0],
        CSIPHY_CORE_MAX,
        &nCameraDeviceInfoLenReq);

    if (CAMERA_SUCCESS != rc)
    {
        CAM_MSG(ERROR, "Failed to get available csiphy devices");
        goto end;
    }
    else if (nCameraDeviceInfoLenReq > CSIPHY_CORE_MAX)
    {
        CAM_MSG(ERROR, "Queried more csiphy (%d) than maximum (%d)",
                nCameraDeviceInfoLenReq, CSIPHY_CORE_MAX);
        rc = CAMERA_ENEEDMORE;
        goto end;
    }

    m_nDevices = nCameraDeviceInfoLenReq;

    for (nDevice = 0; nDevice < m_nDevices && CAMERA_SUCCESS == rc; ++nDevice)
    {
        rc = mDeviceManagerContext->DeviceOpen(cameraDeviceInfo[nDevice].deviceID,
                        &m_CSIPHYCoreCtxt[nDevice].hCsiPhyHandle);
        CAM_MSG(HIGH, "open CsiPhyHandle %p", m_CSIPHYCoreCtxt[nDevice].hCsiPhyHandle);
        CAM_MSG_ON_ERR(rc, "Open CsiPhy%d failed with rc %d ", nDevice, rc);

        if (CAMERA_SUCCESS == rc)
        {
            rc = CameraDeviceRegisterCallback(m_CSIPHYCoreCtxt[nDevice].hCsiPhyHandle, CsiPhyDeviceCallback, this);
            CAM_MSG_ON_ERR(rc, "Registercallback CsiPhy%d failed with rc %d ", nDevice, rc);
        }

        if (CAMERA_SUCCESS == rc)
        {
            rc = CameraDeviceControl(m_CSIPHYCoreCtxt[nDevice].hCsiPhyHandle, CSIPHY_CMD_ID_RESET,
                    NULL, 0, NULL, 0, NULL);
            CAM_MSG_ON_ERR(rc, "PowerON CsiPhy%d failed with rc %d ", nDevice, rc);
        }
    }

end:
    return rc;
}

CameraResult AisCSIConfigurer::Deinit(void)
{
    uint32 i;

    for (i=0; i<m_nDevices; i++)
    {
        if (m_CSIPHYCoreCtxt[i].hCsiPhyHandle)
        {
            CameraDeviceClose(m_CSIPHYCoreCtxt[i].hCsiPhyHandle);
            m_CSIPHYCoreCtxt[i].hCsiPhyHandle = NULL;
        }
    }

    DestroyInstance();

    return CAMERA_SUCCESS;
}

CameraResult AisCSIConfigurer::PowerSuspend(void)
{
    return CAMERA_SUCCESS;
}
CameraResult AisCSIConfigurer::PowerResume(void)
{
    return CAMERA_SUCCESS;
}

CameraResult AisCSIConfigurer::Config(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (CSI_INTERF_INIT == m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].eState)
    {
        MIPICsiPhyConfig_t sMipiCsiCfg;
        ais_input_csi_params_t* csiParams = &pUsrCtxt->m_inputCfg.csiInfo;

        memset(&sMipiCsiCfg, 0x0, sizeof(sMipiCsiCfg));
        sMipiCsiCfg.eMode = MIPICsiPhyModeDefault;
        sMipiCsiCfg.ePhase = MIPICsiPhy_TwoPhase;

        sMipiCsiCfg.nSettleCount = csiParams->settle_count;
        sMipiCsiCfg.nNumOfDataLanes = csiParams->num_lanes;
        sMipiCsiCfg.nLaneMask = csiParams->lane_mask;

        rc = CameraDeviceControl(m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].hCsiPhyHandle, CSIPHY_CMD_ID_CONFIG,
            &sMipiCsiCfg, sizeof(sMipiCsiCfg), NULL, 0, NULL);
        CAM_MSG_ON_ERR(rc, "PowerON CsiPhy%d failed with rc %d ", 0, rc);
    }

    return rc;
}

CameraResult AisCSIConfigurer::Start(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (CSI_INTERF_INIT == m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].eState)
    {
        rc = CameraDeviceControl(m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].hCsiPhyHandle, CSIPHY_CMD_ID_START,
            NULL, 0, NULL, 0, NULL);
        CAM_MSG_ON_ERR(rc, "PowerON CsiPhy%d failed with rc %d ", 0, rc);
    }

    if (CAMERA_SUCCESS == rc)
    {
        m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].eState = CSI_INTERF_STREAMING;
        m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].nRefCount++;
    }

    return rc;
}

CameraResult AisCSIConfigurer::Stop(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;

    if (m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].eState == CSI_INTERF_STREAMING)
    {
        m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].nRefCount--;
        if (0 == m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].nRefCount)
        {
            rc = CameraDeviceControl(m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].hCsiPhyHandle, CSIPHY_CMD_ID_STOP,
                NULL, 0, NULL, 0, NULL);
            CAM_MSG_ON_ERR(rc, "PowerON CsiPhy%d failed with rc %d ", 0, rc);

            m_CSIPHYCoreCtxt[pUsrCtxt->m_resources.csiphy].eState = CSI_INTERF_INIT;
        }

    }
    else
    {
        rc = CAMERA_EBADSTATE;
    }

    return rc;
}


CameraResult AisCSIConfigurer::Resume(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;

    return rc;
}

CameraResult AisCSIConfigurer::Pause(AisUsrCtxt* pUsrCtxt)
{
    CameraResult rc = CAMERA_SUCCESS;

    return rc;
}


CameraResult AisCSIConfigurer::SetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param)
{
    return CAMERA_SUCCESS;
}

CameraResult AisCSIConfigurer::GetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param)
{
    return CAMERA_SUCCESS;
}
