/**
 * @file ife_device.cpp
 *
 * IFE device driver interface.
 *
 * Copyright (c) 2009-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "AEEStdDef.h"
#include "AEEstd.h"
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>

#include "CameraPlatform.h"
#include "CameraPlatformLinux.h"
#include "CameraOSServices.h"
#include <media/cam_defs.h>
#include <media/cam_req_mgr.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "ife_device.h"
#include "ife_drv_i.h"

/*-----------------------------------------------------------------------------
* Preprocessor Definitions and Constants
*---------------------------------------------------------------------------*/
#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1024

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
IFEDevice::IFEDevice(uint8 ifeId)
{
    this->m_ifeId = ifeId;
    memset(m_camDeviceResource, 0, sizeof(m_camDeviceResource));
    memset(&m_ifeQueryCap, 0, sizeof(m_ifeQueryCap));
    memset(m_rdiCfg, 0, sizeof(m_rdiCfg));
}

CameraResult IFEDevice::IfeQueryHwCap()
{
    struct cam_control       ioctlCmd;
    struct cam_query_cap_cmd queryCapsCmd;
    CameraResult             result = CAMERA_SUCCESS;
    int                      returnCode;

    queryCapsCmd.size        = sizeof(struct cam_isp_query_cap_cmd);
    queryCapsCmd.handle_type = CAM_HANDLE_USER_POINTER;
    queryCapsCmd.caps_handle = VoidPtrToUINT64(&m_ifeQueryCap);

    ioctlCmd.op_code         = CAM_QUERY_CAP;
    ioctlCmd.size            = sizeof(struct cam_query_cap_cmd);
    ioctlCmd.handle_type     = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved        = 0;
    ioctlCmd.handle          = VoidPtrToUINT64(&queryCapsCmd);

    returnCode = ioctl(m_ifeKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "IFE%d: ioctl failed IfeQueryHwCap index %d",m_ifeId, returnCode);
        result = CAMERA_EFAILED;
    }
    else
    {
        CAM_MSG(HIGH, "IFE%d: ioctl success IfeQueryHwCap handle:%d", m_ifeId, m_ifeKmdFd);
        //Print out the IOMMU settings
        CAM_MSG(HIGH, "IFE%d: ioctl success dev.nonsecure 0x%x",m_ifeId, m_ifeQueryCap.device_iommu.non_secure);
        CAM_MSG(HIGH, "IFE%d: ioctl success dev.secure 0x%x",m_ifeId, m_ifeQueryCap.device_iommu.secure);
        CAM_MSG(HIGH, "IFE%d: ioctl success cdm.nonsecure 0x%x",m_ifeId, m_ifeQueryCap.cdm_iommu.non_secure);
        CAM_MSG(HIGH, "IFE%d: ioctl success cdm.secure 0x%x",m_ifeId, m_ifeQueryCap.cdm_iommu.secure);
    }

    return result;
}

CameraResult IFEDevice::Init()
{
    CameraResult result = CAMERA_SUCCESS;

    //init rdi setting structure
    for (int rdi_index  = 0; rdi_index < IFE_RDI_MAX; rdi_index++)
    {
        m_rdiCfg[rdi_index].input_resourceType  = IFEInputPHY0 + m_ifeId;
        m_rdiCfg[rdi_index].output_resourceType = IFEOutputRDI0 + rdi_index;
        m_rdiCfg[rdi_index].rdi_cfg.width       = DEFAULT_WIDTH; //Default.  Currently not used in framebase mode.
        m_rdiCfg[rdi_index].rdi_cfg.height      = DEFAULT_HEIGHT; //Default.  Currently not used in framebase mode.
        m_rdiCfg[rdi_index].rdi_cfg.vc          = rdi_index *4;  //set default VC - config should update
        m_rdiCfg[rdi_index].rdi_cfg.numLanes    = 4; // Default 4 lane
    }

    //Only 1 KMD IFE Manager
    m_ifeKmdFd = CameraPlatformGetFd(AIS_SUBDEV_IFE, 0);
    if (0 == m_ifeKmdFd)
    {
        CAM_MSG(ERROR,   "Failed to get the Camera AIS_SUBDEV_IFE");
        result = CAMERA_EFAILED;
    }

    m_camReqMgr = CamReqManager::GetInstance();
    m_pSyncMgr = SyncManager::GetInstance();
    if (!m_camReqMgr || !m_pSyncMgr)
    {
        CAM_MSG(ERROR, "Failed to get Camera request manager & sync manager");
        result = CAMERA_EFAILED;
    }

    if (CAMERA_SUCCESS == result)
    {
        CAM_MSG(HIGH, "IFE query capabilities");
        result = IfeQueryHwCap();
    }

    if (CAMERA_SUCCESS == result)
    {
        result = m_camReqMgr->CRMCreateSession(&m_session_handle);
    }

    if (CAMERA_SUCCESS == result)
    {
        //Session should be common for a CSIPhy/IFE link. CSIPhy will retrieve when acquiring device.
        CameraPlatformSetKMDHandle(AIS_HANDLE_TYPE_SESSION, AIS_SUBDEV_CSIPHY, m_ifeId, m_session_handle);

        for(uint32 rdi_index = 0; rdi_index < IFE_RDI_MAX; rdi_index++)
        {
            m_RDIStream[rdi_index] = NULL;

            IFEStream* ifeStreamCtx =  NULL;
            ifeStreamCtx = new IFEStream(m_ifeId, m_ifeKmdFd, rdi_index);
            if (ifeStreamCtx != NULL)
            {
                m_RDIStream[rdi_index] = ifeStreamCtx;
                result = m_RDIStream[rdi_index]->Init(&m_ifeQueryCap, m_session_handle, &m_rdiCfg[rdi_index]);
            }
            else
            {
                result = CAMERA_EFAILED;
                break;
            }
        }
    }

    return result;
}

CameraResult IFEDevice::DeInit()
{
    CameraResult result = CAMERA_SUCCESS;
    m_ifeKmdFd = 0;

    //release resources that have not been deallocated
    for(uint32 rdi_index = 0; rdi_index < IFE_RDI_MAX; rdi_index++)
    {
        if (m_RDIStream[rdi_index] != NULL)
        {
            m_RDIStream[rdi_index]->DeInit();
            delete m_RDIStream[rdi_index];
            m_RDIStream[rdi_index] = NULL;
        }
    }

    if (m_session_handle)
        m_camReqMgr->CRMDestroySession(&m_session_handle);

    if (m_camReqMgr)
    {
        CamReqManager::DestroyInstance();
        m_camReqMgr = nullptr;
    }

    if (m_pSyncMgr)
    {
        SyncManager::DestroyInstance();
        m_pSyncMgr = nullptr;
    }

    //TODO: release the hardware context and resources

    return result;
}

/**************************************************************************
 ** Ife Device Interface
 *************************************************************************/
/*===========================================================================
=========================================================================== */
CameraResult ife_device_open(CameraDeviceHandleType** ppDeviceHandle,
        CameraDeviceIDType deviceId)
{
    CameraResult result = CAMERA_SUCCESS;

    if (NULL == ppDeviceHandle)
    {
        CAM_MSG(ERROR, "Invalid ptr passed in");
        return CAMERA_EMEMPTR;
    }

    IFEDevice* pIfeCtx = IFEDevice::IFEOpen(deviceId);

    /* This has to be is the last block */
    if (pIfeCtx)
    {
        /* no error detected */
        *ppDeviceHandle = static_cast<CameraDeviceHandleType*>(pIfeCtx);

        CAM_MSG(MEDIUM, "IFE%x: IFE device opened!", deviceId);
    }
    else
    {
        //If init is NOT successful, so tear down whatever has been created.
        CAM_MSG(ERROR, "IFE%d: IFE open failed", deviceId);

        *ppDeviceHandle = NULL;
        result = CAMERA_EFAILED;
    }

    return result;
}

IFEDevice* IFEDevice::IFEOpen(CameraDeviceIDType deviceId)
{
    CameraResult result = CAMERA_SUCCESS;

    //the last nimble represents the IFE ID
    uint8 ifeId = (deviceId & 0xF);

    IFEDevice* pIfeCtx = NULL;

    if (ifeId >= IFE_CORE_MAX)
    {
        result = CAMERA_EBADPARM;
    }
    else
    {
        //---------------------------------------------------------------------
        pIfeCtx = new IFEDevice(ifeId);
        if (pIfeCtx)
        {
            result = pIfeCtx->Init();

            /* This has to be is the last block */
            if (CAMERA_SUCCESS != result)
            {
                pIfeCtx->DeInit();
                delete pIfeCtx;
                pIfeCtx = NULL;
            }
        }
        else
        {
            CAM_MSG(ERROR, "IFE%d: Failed to create new IFEDevice", ifeId);
        }
    }

    return pIfeCtx;
}


/************************** IFE Wrapper ****************************\
* ife_device_register_callback()
*
* Description:  Register a client callback to ife device
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
CameraResult IFEDevice::RegisterCallback(CameraDeviceCallbackType pfnCallback, void *pClientData)
{
    CameraResult result = CAMERA_SUCCESS;

    //Pass the call back info to Sync Manager and Request Manager
    if (pfnCallback)
    {
        m_pSyncMgr->RegisterCallback(pfnCallback, pClientData);
        m_camReqMgr->RegisterCallback(pfnCallback, pClientData);
    }

    return result;
}

/* ===========================================================================
=========================================================================== */
CameraResult IFEDevice::Close(void)
{
    CameraResult result = CAMERA_SUCCESS;

    result = DeInit();

    delete this;

    return result;
}

/* ===========================================================================
                     ife_device_control
=========================================================================== */
const char ifeCommandIDtoString[][80] =
{
    #define CMD(x) #x,      // Stringize parameter x
    #define CMD_CLONE(x,y)  // Don't stringize CMD_CLONE since its not a new value
    #include "ife_drv_cmds.h"
    #undef CMD
    #undef CMD_CLONE
    "IFE_CMD_ID_MAX"        // The last VFE command ID
};

/*
 * Translate ais csi code type to KMD csi core
*/
uint32 IFEDevice::GetKMDCsiPhy(CsiphyCoreType csiCore)
{
    uint32 csiPhy = IFEInputInvalid;
    switch (csiCore)
    {
    case CSIPHY_CORE_0:
        csiPhy = IFEInputPHY0;
        break;
    case CSIPHY_CORE_1:
        csiPhy = IFEInputPHY1;
        break;
    case CSIPHY_CORE_2:
        csiPhy = IFEInputPHY2;
        break;
#ifndef TARGET_SM6150
    case CSIPHY_CORE_3:
        csiPhy = IFEInputPHY3;
        break;
#endif
    default:
        CAM_MSG(ERROR, "IFE%d: IFE invalid csi %d", csiCore);
        break;
    }

    return csiPhy;
}

CameraResult IFEDevice::Control(uint32 uidControl,
        const void* pIn, int nInLen,
        void* pOut, int nOutLen, int* pnOutLenReq)
{
    IFEDevice* pIfeCtx = this;
    CameraResult result   = CAMERA_SUCCESS;

    // pIn = NULL, nInLen !=0 ---> not OK
    // pIn = NULL, nInLen = 0 ---> OK
    // pIn !=NULL, nInLen !=0 ---> OK
    // pIn !=NULL, nInLen = 0 ---> not OK
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


    CAM_MSG(LOW, "IFE%d: IFE Driver Processing %s command.", pIfeCtx->m_ifeId,  ifeCommandIDtoString[uidControl]);

    switch (uidControl)
    {

        case IFE_CMD_ID_GET_HW_VERSION:
        {
            if (nOutLen != sizeof(ife_cmd_hw_version))
            {
                return CAMERA_EBADPARM;
            }

            if (NULL != pnOutLenReq)
            {
                *pnOutLenReq = sizeof(ife_cmd_hw_version);
            }
            ((ife_cmd_hw_version*)pOut)->ifeHWVersion = 0x0;
            break;
        }

        case IFE_CMD_ID_POWER_ON:
        {
            result = CAMERA_SUCCESS;
            break;
        }

        case IFE_CMD_ID_POWER_OFF:
        {
            result = CAMERA_SUCCESS;
            break;
        }
        case IFE_CMD_ID_RESET:
        {
            result = CAMERA_SUCCESS;
            break;
        }
        case IFE_CMD_ID_RDI_CONFIG:
        {
            if (nInLen != sizeof(ife_rdi_config_t))
            {
                return CAMERA_EBADPARM;
            }

            uint32 rdi_index = 0;
            ife_rdi_config_t* p_rdi_config = (ife_rdi_config_t*)pIn;
            result = CAMERA_SUCCESS;

            //Check if it's for the correct phy
            if (GetKMDCsiPhy(p_rdi_config->csiphyId) == (m_rdiCfg[rdi_index].input_resourceType))
            {
                switch(p_rdi_config->outputPath)
                {
                    case IFE_OUTPUT_PATH_RDI0:
                        rdi_index = 0;
                        break;
                    case IFE_OUTPUT_PATH_RDI1:
                        rdi_index = 1;
                        break;
                    case IFE_OUTPUT_PATH_RDI2:
                        rdi_index = 2;
                        break;
                    case IFE_OUTPUT_PATH_RDI3:
                        if (m_ifeId > 1) //check if it's ife lite
                        {
                            rdi_index = 3;
                        }
                        else
                        {
                            CAM_MSG(ERROR, "IFE%d: IFE invalid rdi %d path", pIfeCtx->m_ifeId, p_rdi_config->outputPath);
                            result = CAMERA_EBADPARM;
                        }
                        break;
                    default:
                        result = CAMERA_EBADPARM;
                }
            }
            else
            {
                CAM_MSG(ERROR, "IFE%d: IFE invalid csiphy %d for rdi %d path", pIfeCtx->m_ifeId,
                                                                               p_rdi_config->csiphyId,
                                                                               p_rdi_config->outputPath);
                result = CAMERA_EBADPARM;
            }

            CAM_MSG_ON_ERR(result, "IFE%d: IFE Config failed, with result = %d.", m_ifeId,  result);

            if (CAMERA_SUCCESS == result)
            {
                m_rdiCfg[rdi_index].rdi_cfg = *p_rdi_config;
                m_rdiCfg[rdi_index].input_format = ISPFormatMIPIRaw8;
                m_rdiCfg[rdi_index].output_format = ISPFormatMIPIRaw8;
            }

            result = m_RDIStream[rdi_index]->IfeStreamConfig();

            break;
        }
        case IFE_CMD_ID_START:
        {
            if (nInLen != sizeof(ife_start_cmd_t))
            {
                return CAMERA_EBADPARM;
            }

            ife_start_cmd_t* p_start_cmd = (ife_start_cmd_t*)pIn;
            result = CAMERA_SUCCESS;

            CAM_MSG(HIGH, "IFE%d: IFE start rdi %d path output buffers %d", pIfeCtx->m_ifeId,
                                                          p_start_cmd->outputPath,
                                                          p_start_cmd->numOfOutputBufs);
            result = m_RDIStream[p_start_cmd->outputPath]->IfeStreamStart(p_start_cmd);

            break;
        }
        case IFE_CMD_ID_STOP:
        {
            if (nInLen != sizeof(ife_stop_cmd_t))
            {
                return CAMERA_EBADPARM;
            }

            ife_stop_cmd_t* p_stop_cmd = (ife_stop_cmd_t*)pIn;
            result = m_RDIStream[p_stop_cmd->outputPath]->IfeStreamStop(p_stop_cmd);

            break;
        }

    case IFE_CMD_ID_RESUME:
        {
            if (nInLen != sizeof(ife_start_cmd_t))
            {
                return CAMERA_EBADPARM;
            }

            ife_start_cmd_t* p_start_cmd = (ife_start_cmd_t*)pIn;
            m_RDIStream[p_start_cmd->outputPath]->IfeStreamResume();

            break;
        }

    case IFE_CMD_ID_PAUSE:
        {
            if (nInLen != sizeof(ife_stop_cmd_t))
            {
                return CAMERA_EBADPARM;
            }

            ife_stop_cmd_t* p_stop_cmd = (ife_stop_cmd_t*)pIn;
            m_RDIStream[p_stop_cmd->outputPath]->IfeStreamPause();

            break;
        }

        case IFE_CMD_ID_OUTPUT_BUFFER_ENQUEUE:
        {
            if (nInLen != sizeof(vfe_cmd_output_buffer))
            {
                return CAMERA_EBADPARM;
            }
            vfe_cmd_output_buffer* pBuf = (vfe_cmd_output_buffer*)pIn;

            result = m_RDIStream[pBuf->outputPath]->IfeStreamBufferEnqueue(pBuf);
            break;
        }
        default:
        {
            CAM_MSG(ERROR, "IFE%d: Unexpected command ID %d", pIfeCtx->m_ifeId, uidControl);
            result = CAMERA_EUNSUPPORTED;
            break;
        }
    }

    return result;
}

