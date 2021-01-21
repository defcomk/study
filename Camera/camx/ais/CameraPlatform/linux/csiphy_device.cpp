/**
 * @file csiphy_device.c
 *
 * CSIPHY device driver interface.
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "AEEStdDef.h"
#include "AEEstd.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include "CameraPlatform.h"
#include "CameraPlatformLinux.h"
#include "csiphy_device.h"
#include "csiphy_i.h"

/*-----------------------------------------------------------------------------
* Static Function Declarations and Definitions
*---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
* Preprocessor Definitions and Constants
*---------------------------------------------------------------------------*/

/*===========================================================================
                 DEFINITIONS AND DECLARATIONS FOR MODULE
=========================================================================== */
/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */

/* Description:
* _fcn_ will always be called. _err_ will be updated by
* return value of _fcn_ only if _err_ is CAMERA_SUCCESS.
* So, as soon as an error occur in a sequence of use, the
* first error won't be overridden.
*
* Param:
* _fcn_ - the function which will always be called;
*         return value of the function must be CameraResult
* _err_ - running CameraResult.
* _tmp_ - a scratch CameraResult.
*
*/
#define LOG_FIRST_RESULT(_fcn_, _err_, _tmp_) \
    (_tmp_) = (_fcn_); \
    if ((CAMERA_SUCCESS == (_err_)) && \
        (CAMERA_SUCCESS != (_tmp_))) \
    { \
        (_err_) = (_tmp_); \
    } \



/* ---------------------------------------------------------------------------
** Global Object Definitions
** -------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------
** Local Object Definitions
** ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------
** Forward Declarations
** ------------------------------------------------------------------------ */

/**************************************************************************
 ** Component Constructor/Destructor
 *************************************************************************/
CSIPHYDevice::CSIPHYDevice(uint8 csiphyId)
{
    m_csiphyId = csiphyId;

    m_fd = 0;
    memset(&m_csiphy_acq_dev, 0x0, sizeof(m_csiphy_acq_dev));
    memset(&m_csiphy_acq_params, 0x0, sizeof(m_csiphy_acq_params));

    m_pfnCallBack = NULL;
    m_pClientData = NULL;
}

CameraResult CSIPHYDevice::Init()
{
    CameraResult result = CAMERA_SUCCESS;

    const char * path = NULL;
    path = CameraPlatformGetDevPath(AIS_SUBDEV_CSIPHY, m_csiphyId);
    if (path)
    {
        CAM_MSG(HIGH,"Path to CSIPHY%d = %s", m_csiphyId, path);
        m_fd = open(path, O_RDWR | O_NONBLOCK);
        if (m_fd <= 0)
        {
            CAM_MSG(ERROR, "Cannot open CSIPHY%d '%s'", m_csiphyId, path);
            m_fd = 0;
            result = CAMERA_EFAILED;
        }
    }
    else
    {
        CAM_MSG(ERROR, "CSIPHY%d subdev not available", m_csiphyId);
        result = CAMERA_EFAILED;
    }

    return result;
}

CameraResult CSIPHYDevice::DeInit()
{
    if (m_fd > 0)
    {
        close(m_fd);
        m_fd = 0;
    }
    return CAMERA_SUCCESS;
}

CameraResult CSIPHYDevice::Reset()
{
    return CAMERA_SUCCESS;
}

CameraResult CSIPHYDevice::Config(MIPICsiPhyConfig_t* pMipiCsiCfg)
{
    CameraResult result = CAMERA_SUCCESS;

    if (pMipiCsiCfg->ePhase != MIPICsiPhy_TwoPhase)
    {
        CAM_MSG(ERROR, "Unsupported ePhase %d", pMipiCsiCfg->ePhase);
        result = CAMERA_EBADPARM;
    }

    if (pMipiCsiCfg->eMode != MIPICsiPhyModeDefault)
    {
        CAM_MSG(ERROR, "Unsupported eMode %d", pMipiCsiCfg->eMode);
        result = CAMERA_EBADPARM;
    }

    if (CAMERA_SUCCESS == result)
    {
        m_sMipiCsiCfg = *pMipiCsiCfg;
    }

    return result;
}

CameraResult CSIPHYDevice::Start()
{
    CameraResult result = CAMERA_SUCCESS;
    int rc = 0;
    struct cam_control cam_cmd;

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));

    cam_cmd.op_code     = CAM_ACQUIRE_DEV;
    cam_cmd.size        = sizeof(m_csiphy_acq_dev);
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.reserved    = 0;
    cam_cmd.handle      = (uint64_t)&m_csiphy_acq_dev;

    m_csiphy_acq_dev.session_handle = CameraPlatformGetKMDHandle(AIS_HANDLE_TYPE_SESSION, AIS_SUBDEV_CSIPHY, m_csiphyId);
    m_csiphy_acq_dev.device_handle  = 0;
    m_csiphy_acq_dev.reserved       = 0;
    m_csiphy_acq_dev.handle_type    = CAM_HANDLE_USER_POINTER;
    m_csiphy_acq_dev.info_handle    = (uint64_t)&m_csiphy_acq_params;

    rc = ioctl(m_fd, VIDIOC_CAM_CONTROL, &cam_cmd);
    if (rc < 0)
    {
        CAM_MSG(ERROR, "CSIPHY%d acquire failed %d", m_csiphyId, rc);
        result = CAMERA_EFAILED;
    }

    if (CAMERA_SUCCESS == result)
    {
        struct cam_csiphy_info csi_info;
        struct cam_config_dev_cmd submit_cmd;
        memset(&csi_info, 0x0, sizeof(csi_info));

        csi_info.lane_mask = m_sMipiCsiCfg.nLaneMask;
        csi_info.lane_assign = 0x3210; //@TODO: fixed mapping for now
        csi_info.csiphy_3phase = (uint8_t)m_sMipiCsiCfg.ePhase;
        csi_info.combo_mode = (uint8_t)m_sMipiCsiCfg.eMode;
        csi_info.lane_cnt = (uint8_t)m_sMipiCsiCfg.nNumOfDataLanes;
        csi_info.secure_mode = 0;
        csi_info.settle_time = (uint64_t)m_sMipiCsiCfg.nSettleCount;
        csi_info.data_rate = 0;

        submit_cmd.packet_handle = (uint64_t)&csi_info;


        cam_cmd.op_code     = CAM_CONFIG_DEV_EXTERNAL;
        cam_cmd.size        = sizeof(submit_cmd);
        cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
        cam_cmd.reserved    = 0;
        cam_cmd.handle      = (uint64_t)&submit_cmd;

        rc = ioctl(m_fd, VIDIOC_CAM_CONTROL, &cam_cmd);
        if (rc < 0)
        {
            CAM_MSG(ERROR, "CSIPHY%d config failed %d", m_csiphyId, rc);
            result = CAMERA_EFAILED;
        }
    }

    if (CAMERA_SUCCESS == result)
    {
        struct cam_start_stop_dev_cmd start_cmd;
        memset(&start_cmd, 0x0, sizeof(start_cmd));

        //provide the csiphy device handle to platform layer
        //so ife can link with it
        CameraPlatformSetKMDHandle(AIS_HANDLE_TYPE_DEVICE, AIS_SUBDEV_CSIPHY, m_csiphyId, m_csiphy_acq_dev.device_handle);


        start_cmd.session_handle = m_csiphy_acq_dev.session_handle;
        start_cmd.dev_handle = m_csiphy_acq_dev.device_handle;

        cam_cmd.op_code     = CAM_START_DEV;
        cam_cmd.size        = sizeof(start_cmd);
        cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
        cam_cmd.reserved    = 0;
        cam_cmd.handle      = (uint64_t)&start_cmd;

        rc = ioctl(m_fd, VIDIOC_CAM_CONTROL, &cam_cmd);
        if (rc < 0)
        {
            CAM_MSG(ERROR, "CSIPHY%d start failed %d", m_csiphyId, rc);
            result = CAMERA_EFAILED;
        }
    }

    return result;
}

CameraResult CSIPHYDevice::Stop()
{
    CameraResult result = CAMERA_SUCCESS;
    int rc = 0;
    struct cam_control cam_cmd;
    struct cam_start_stop_dev_cmd stop_cmd;

    memset(&cam_cmd, 0x0, sizeof(cam_cmd));

    //1. stop device
    memset(&stop_cmd, 0x0, sizeof(stop_cmd));

    stop_cmd.session_handle = m_csiphy_acq_dev.session_handle;
    stop_cmd.dev_handle = m_csiphy_acq_dev.device_handle;

    cam_cmd.op_code     = CAM_STOP_DEV;
    cam_cmd.size        = sizeof(stop_cmd);
    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
    cam_cmd.reserved    = 0;
    cam_cmd.handle      = (uint64_t)&stop_cmd;

    rc = ioctl(m_fd, VIDIOC_CAM_CONTROL, &cam_cmd);
    if (rc < 0)
    {
        CAM_MSG(ERROR, "CSIPHY%d start failed %d", m_csiphyId, rc);
        result = CAMERA_EFAILED;
    }

    //2. release device
    {
        struct cam_release_dev_cmd release_cmd;
        memset(&release_cmd, 0x0, sizeof(release_cmd));

        release_cmd.session_handle = m_csiphy_acq_dev.session_handle;
        release_cmd.dev_handle = m_csiphy_acq_dev.device_handle;

        cam_cmd.op_code     = CAM_RELEASE_DEV;
        cam_cmd.size        = sizeof(release_cmd);
        cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
        cam_cmd.reserved    = 0;
        cam_cmd.handle      = (uint64_t)&release_cmd;

        rc = ioctl(m_fd, VIDIOC_CAM_CONTROL, &cam_cmd);
        if (rc < 0)
        {
            CAM_MSG(ERROR, "CSIPHY%d start failed %d", m_csiphyId, rc);
            result = CAMERA_EFAILED;
        }
    }

    return result;
}

/**************************************************************************
 ** Ife Device Interface
 *************************************************************************/
/*===========================================================================
=========================================================================== */
CameraResult csiphy_device_open(CameraDeviceHandleType** ppDeviceHandle,
        CameraDeviceIDType deviceId)
{
    CameraResult result = CAMERA_SUCCESS;

    if (NULL == ppDeviceHandle)
    {
        CAM_MSG(ERROR, "Invalid ptr passed in");
        return CAMERA_EMEMPTR;
    }

    CSIPHYDevice* pCsiPhyCtxt = CSIPHYDevice::CSIPHYOpen(deviceId);

    /* This has to be is the last block */
    if (pCsiPhyCtxt)
    {
        /* no error detected */
        *ppDeviceHandle = static_cast<CameraDeviceHandleType*>(pCsiPhyCtxt);

        CAM_MSG(MEDIUM, "CSIPHY 0x%x: CSIPHY device opened!", deviceId);
    }
    else
    {
        //If init is NOT successful, so tear down whatever has been created.
        CAM_MSG(ERROR, "CSIPHY 0x%x: CSIPHY open failed", deviceId);

        *ppDeviceHandle = NULL;
        result = CAMERA_EFAILED;
    }

    return result;
}

CSIPHYDevice* CSIPHYDevice::CSIPHYOpen(CameraDeviceIDType deviceId)
{
    CameraResult result = CAMERA_SUCCESS;

    //the last nimble represents the CSIPHY ID
    uint8 m_csiphyId = (deviceId & 0xF);

    CSIPHYDevice* pCsiPhyCtxt = NULL;

    if ((result == CAMERA_SUCCESS) && (m_csiphyId > 3))
    {
        result = CAMERA_EBADPARM;
    }
    else
    {
        //---------------------------------------------------------------------
        pCsiPhyCtxt = new CSIPHYDevice(m_csiphyId);
        if (pCsiPhyCtxt)
        {
            result = pCsiPhyCtxt->Init();

            /* This has to be is the last block */
            if (CAMERA_SUCCESS != result)
            {
                pCsiPhyCtxt->DeInit();
                delete pCsiPhyCtxt;
                pCsiPhyCtxt = NULL;
            }
        }
        else
        {
            CAM_MSG(ERROR, "CSIPHY%d: Failed to create new CSIPHYDevice", m_csiphyId);
        }
    }

    return pCsiPhyCtxt;
}


/************************** CSIPHY Wrapper ****************************\
* csiphy_device_register_callback()
*
* Description:  Register a client callback to csiphy device
*
* Parameters:
*               pDevice     - handle to Ife device
*               pfnCallback - function pointer for callback
*
* Returns:      CAMERA_SUCCESS
*               CAMERA_EMEMPTR
*
* Note:         Synchronous call.
*
\**************************************************************************/
CameraResult CSIPHYDevice::RegisterCallback(CameraDeviceCallbackType pfnCallback, void *pClientData)
{
    CameraResult result = CAMERA_SUCCESS;

    if (pfnCallback)
    {
        this->m_pfnCallBack = pfnCallback;
        this->m_pClientData = pClientData;
    }

    return result;
}

/* ===========================================================================
=========================================================================== */
CameraResult CSIPHYDevice::Close(void)
{
    CameraResult result = CAMERA_SUCCESS;

    result = DeInit();

    delete this;

    return result;
}

/* ===========================================================================
                     csiphy_device_control
=========================================================================== */
CameraResult CSIPHYDevice::Control(uint32 uidControl,
        const void* pIn, int nInLen,
        void* pOut, int nOutLen, int* pnOutLenReq)
{
    CSIPHYDevice* pCsiPhyCtxt = this;
    CameraResult result   = CAMERA_SUCCESS;

    if (NULL == pIn)
    {
        if (nInLen > 0)
        {
            return CAMERA_EMEMPTR;
        }
    }

    if (NULL == pOut)
    {
        if (nOutLen > 0)
        {
            return CAMERA_EMEMPTR;
        }
    }

    if (NULL != pnOutLenReq)
    {
        *pnOutLenReq = 0;
    }

    CAM_MSG(LOW, "CSIPHY%d: CSIPHY Driver Processing %d command.", pCsiPhyCtxt->m_csiphyId,  uidControl);

    switch (uidControl)
    {
        case CSIPHY_CMD_ID_RESET:
        {
            result = Reset();
            break;
        }
        case CSIPHY_CMD_ID_CONFIG:
        {
            MIPICsiPhyConfig_t* pMipiCsiCfg = (MIPICsiPhyConfig_t*)pIn;
            if (nInLen == sizeof(*pMipiCsiCfg))
            {
                result = Config(pMipiCsiCfg);
            }
            else
            {
                result = CAMERA_EBADPARM;
            }
            break;
        }
        case CSIPHY_CMD_ID_START:
        {
            result = Start();
            break;
        }
        case CSIPHY_CMD_ID_STOP:
        {
            result = Stop();
            break;
        }
        default:
        {
            CAM_MSG(ERROR, "CSIPHY%d: unexpected command(%d)", pCsiPhyCtxt->m_csiphyId, uidControl);
            result = CAMERA_EUNSUPPORTED;
            break;
        }
    }

    return result;
}
