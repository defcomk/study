/**
 * @file ife_stream.cpp
 *
 * @brief IFE Stream class implementation.
 *
 * Copyright (c) 2009-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* ===========================================================================
 *                         INCLUDE FILES FOR MODULE
 *===========================================================================
 */
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
#include "ife_drv_api.h"
#include "ife_stream_i.h"

/*-----------------------------------------------------------------------------
 * * Static Function Declarations and Definitions
 * *---------------------------------------------------------------------------*/
#define IFE_MAX_NUM_INTERFACES 12  //This needs to be updated based on the h/w

/**************************************************************************
 * Component Constructor/Destructor
 *************************************************************************/

IFEStream::IFEStream(uint8 ifeId, int ifeKmdFd, uint32 rdi_index)
{
    this->m_ifeId    = ifeId;
    this->m_ifeKmdFd = ifeKmdFd;
    this->m_rdiIndex = rdi_index;
}

CameraResult IFEStream::Init(
    cam_isp_query_cap_cmd* ifeQueryCap,
    int32_t                session_hdl,
    ife_rdi_info_t*        rdiCfg)
{
    CameraResult result    = CAMERA_SUCCESS;
    size_t allocationSize;
    size_t alignment       = 8;
    uint32 flags;
    uint32 genericBlobCmdBufferSizeBytes;
    CSLPacket* pPacket;

    //Init the m_bufferInfo
    memset(&m_bufferInfo, 0, sizeof(m_bufferInfo));
    m_ifeQueryCap    = ifeQueryCap;
    m_session_handle = session_hdl;
    m_rdiCfg         = rdiCfg;
    m_camReqMgr      = CamReqManager::GetInstance();
    m_pSyncMgr       = SyncManager::GetInstance();

    //TODO: double check these sizes.  May not be necessary for our RDI only usecase.
    // Number of blob command buffers in circulation 10
    // 1 extra is for Initial Configuration packet
    // 4 more for pipeline delay
    // 2 extra for meet max request depth
    m_ifeCmdBlobCount  = 17;

    // Command Buffers for GenericBlob, Common note:camx says 3 but code is 2
    m_maxNumCmdBuffers = 2;
    // Max of (4 Pixel output and 3 RDI output)
    m_maxNumIOConfigs  = 24;
    // The max below depends on the number of embeddings of DMI/indirect buffers in the packet. Ideally this
    // value should be calculated based on the design in this node. But an upper bound is fine too.
    m_maxNumPatches    = 24;

    //Create the packet buffer
    m_pktDataPayloadSize = (m_maxNumCmdBuffers * sizeof(CSLCmdMemDesc))  +
                           (m_maxNumIOConfigs  * sizeof(CSLBufferIOConfig)) +
                           (m_maxNumPatches    * sizeof(CSLAddrPatch));


    //Calculate aligned size which includes CSLPacket header + the pktDataPayload
    //Note that we subtract the data element from CSLPacket header as it is accounted for
    //in the pktDataPayload
    m_pktalignedResourceSize = AIS_ALIGN_SIZE((sizeof(CSLPacket) - sizeof(uint64) +
                                          m_pktDataPayloadSize +
                                          sizeof(AisCanary)), alignment);


    allocationSize = m_ifeCmdBlobCount * m_pktalignedResourceSize;

    flags = CAM_MEM_FLAG_CMD_BUF_TYPE|CAM_MEM_FLAG_UMD_ACCESS|CAM_MEM_FLAG_KMD_ACCESS;

    result = m_camReqMgr->AllocateCmdBuffers(&pktBufferInfo, allocationSize, alignment, flags, m_ifeQueryCap);

    if (CAMERA_SUCCESS != result)
    {
        DeInit();
        return result;
    }

    memset(pktBufferInfo.pVirtualAddr, 0, pktBufferInfo.size);

    //Partition the buffers
    for (uint32 j = 0; j < m_ifeCmdBlobCount; j++)
    {
        pktBuffer[j].pCSLBufferInfo = &pktBufferInfo;
        pktBuffer[j].offset         =  j * m_pktalignedResourceSize;
        pktBuffer[j].pVirtualAddr   = VoidPtrInc(pktBufferInfo.pVirtualAddr, pktBuffer[j].offset);
        pktBuffer[j].dataSize       = m_pktDataPayloadSize;
        pPacket                     = reinterpret_cast<CSLPacket*>(pktBuffer[j].pVirtualAddr);
        pPacket->header.size        = static_cast<uint32>(m_pktalignedResourceSize);
        pPacket->cmdBuffersOffset   = 0; //Offset from the data[1] field
        pPacket->ioConfigsOffset    = (m_maxNumCmdBuffers * sizeof(CSLCmdMemDesc));
        pPacket->patchsetOffset     = (m_maxNumCmdBuffers * sizeof(CSLCmdMemDesc)) + (m_maxNumIOConfigs * sizeof(CSLBufferIOConfig));

        //Reset the packet
        memset(pPacket->data, 0, pktBuffer[j].dataSize);

        pPacket->numBufferIOConfigs = 0;
        pPacket->numCmdBuffers      = 0;
        pPacket->numPatches         = 0;
        pPacket->kmdCmdBufferIndex  = static_cast<uint32>(-1);
    }

    //Create the initial config cmd buffer - one big block is allocated which will be carved up
    alignment = 32;

    genericBlobCmdBufferSizeBytes =
        (sizeof(IFEResourceHFRConfig) + (sizeof(IFEPortHFRConfig) * (MaxIFEOutputPorts - 1))) +  // HFR configuration
        sizeof(IFEResourceClockConfig) + (sizeof(uint64) * (RDIMaxNum - 1)) +
        sizeof(IFEResourceBWConfig) + (sizeof(IFEResourceBWVote) * (RDIMaxNum - 1)) +
        sizeof(CSLResourceUBWCConfig) + (sizeof(CSLPortUBWCConfig) * (CSLMaxNumPlanes - 1) * (MaxIFEOutputPorts - 1));

    m_cmdalignedResourceSize = AIS_ALIGN_SIZE((genericBlobCmdBufferSizeBytes + sizeof(AisCanary)), alignment);
    allocationSize = m_ifeCmdBlobCount * m_cmdalignedResourceSize;

    flags = CAM_MEM_FLAG_CMD_BUF_TYPE|CAM_MEM_FLAG_UMD_ACCESS|CAM_MEM_FLAG_KMD_ACCESS|CAM_MEM_FLAG_HW_READ_WRITE;

    result = m_camReqMgr->AllocateCmdBuffers(&cmdBufferInfo, allocationSize, alignment, flags, m_ifeQueryCap);

    if (CAMERA_SUCCESS != result)
    {
        DeInit();
        return result;
    }

    memset(cmdBufferInfo.pVirtualAddr, 0, cmdBufferInfo.size);

    //Partition the buffers
    for (uint32 j = 0; j < m_ifeCmdBlobCount; j++)
    {
        cmdBuffer[j].pCSLBufferInfo = &cmdBufferInfo;
        cmdBuffer[j].offset         = j * m_cmdalignedResourceSize;
        cmdBuffer[j].pVirtualAddr   = VoidPtrInc(cmdBufferInfo.pVirtualAddr, cmdBuffer[j].offset);
        cmdBuffer[j].size           = m_cmdalignedResourceSize;
        cmdBuffer[j].length         = 0;
        cmdBuffer[j].type           = static_cast<uint32>(CmdType::Generic);
        cmdBuffer[j].metadata       = CSLIFECmdBufferIdGenericBlob;
    }
    //TODO - add cleanup to release
    return result;
}

CameraResult IFEStream::DeInit()
{
    CameraResult result = CAMERA_SUCCESS;

    if (m_camReqMgr != NULL)
    {
        m_camReqMgr->FreeCmdBuffer(&pktBufferInfo);
        m_camReqMgr->FreeCmdBuffer(&cmdBufferInfo);
    }

    //Clear tracking structures
    memset(cmdBuffer, 0, sizeof(cmdBuffer));
    memset(pktBuffer, 0, sizeof(pktBuffer));

    return result;
}

CameraResult IFEStream::IfeAcquireDev(
    CamDeviceHandle*   phDevice,
    int32              deviceIndex,
    CamDeviceResource* pDeviceResourceRequest,
    size_t             numDeviceResources)
{
    struct cam_acquire_dev_cmd acquireCmd;
    size_t                     loop;
    struct cam_control         ioctlCmd;
    CameraResult               result  = CAMERA_EFAILED;
    int returnCode;
    struct cam_isp_resource*   pISPResource    = NULL;

    (void)pDeviceResourceRequest;
    (void)numDeviceResources;

    // num of resources will be CAM_API_COMPAT_CONSTANT to let kernel know to skip the hw resources acquiring
    acquireCmd.session_handle = this->m_session_handle;
    acquireCmd.num_resources  = CAM_API_COMPAT_CONSTANT;
    acquireCmd.handle_type    = CAM_HANDLE_USER_POINTER;
    acquireCmd.resource_hdl   = VoidPtrToUINT64(pISPResource);

    ioctlCmd.op_code     = CAM_ACQUIRE_DEV;
    ioctlCmd.size        = sizeof(struct cam_acquire_dev_cmd);
    ioctlCmd.handle_type = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved    = 0;
    ioctlCmd.handle      = VoidPtrToUINT64(&acquireCmd);

    returnCode = ioctl(m_ifeKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);

    if (-1 == returnCode)
    {
        CAM_MSG(ERROR, "IfeDevAcquire failed for index %d", deviceIndex);
    }
    else
    {
        CAM_MSG(HIGH,  "IfeDevAcquire success for session_handle 0x%x  index %d", this->m_session_handle, deviceIndex);
        *phDevice = acquireCmd.dev_handle;
        result = CAMERA_SUCCESS;
    }

    return result;
}

CameraResult IFEStream::IfeReleaseDev(CamDeviceHandle* deviceHandle)
{
    struct cam_release_dev_cmd releaseCmd;
    struct cam_control         ioctlCmd;
    int                        returnCode;
    CameraResult result = CAMERA_SUCCESS;

    releaseCmd.session_handle = m_session_handle;
    releaseCmd.dev_handle     = *deviceHandle;

    ioctlCmd.op_code     = CAM_RELEASE_DEV;
    ioctlCmd.size        = sizeof(struct cam_release_dev_cmd);
    ioctlCmd.handle_type = CAM_HANDLE_USER_POINTER;
    ioctlCmd.handle      = VoidPtrToUINT64(&releaseCmd);

    returnCode = ioctl(m_ifeKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);

    if(-1 == returnCode)
    {
        CAM_MSG(ERROR, "IfeReleaseDev failed");
        result = CAMERA_EFAILED;
    }
    else
    {
        CAM_MSG(HIGH, "IfeReleaseDev Success");
        *deviceHandle = 0;
    }

    return result;
}

CameraResult IFEStream::IfeAcquireHw(
    CamDeviceHandle deviceHandle,
    uint32 intfNum,
    CamDeviceResource* pDeviceResourceRequest)
{
    struct cam_acquire_hw_cmd_v1    acquireHWCmd;
    struct cam_control              ioctlCmd;
    CameraResult                    result          = CAMERA_SUCCESS;
    int                             returnCode;
    CAM_MSG(HIGH, "Entering Acquire IfeHwAcquire for interface %d", intfNum);

    if (intfNum < IFE_MAX_NUM_INTERFACES)
    {
        acquireHWCmd.struct_version = CAM_ACQUIRE_HW_STRUCT_VERSION_1;
        acquireHWCmd.session_handle = m_session_handle;
        acquireHWCmd.dev_handle     = deviceHandle;
        acquireHWCmd.handle_type    = CAM_HANDLE_USER_POINTER;
        acquireHWCmd.data_size      = pDeviceResourceRequest->deviceResourceParamSize;
        acquireHWCmd.resource_hdl   = VoidPtrToUINT64(pDeviceResourceRequest->pDeviceResourceParam);

        ioctlCmd.op_code        = CAM_ACQUIRE_HW;
        ioctlCmd.size           = sizeof(struct cam_acquire_hw_cmd_v1);
        ioctlCmd.handle_type    = CAM_HANDLE_USER_POINTER;
        ioctlCmd.reserved       = 0;
        ioctlCmd.handle         = VoidPtrToUINT64(&acquireHWCmd);

        returnCode = ioctl(m_ifeKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
        if (0 != returnCode)
        {
            CAM_MSG(ERROR, "IFE%d: ioctl failed IfeHwAcquire index %d", m_ifeId, returnCode);
            result = CAMERA_EFAILED;
        }
        else
        {
            CAM_MSG(HIGH, "IFE%d: ioctl success IfeHwAcquire for index %d devhandle 0x%x",m_ifeId, intfNum, deviceHandle);
        }
    }
    else
    {
        CAM_MSG(ERROR, "Invalid: intfNum = %d", intfNum);
        result = CAMERA_EFAILED;
    }

    return result;
}


CameraResult IFEStream::IfeConfigAcquireHw()
{
    CameraResult result = CAMERA_SUCCESS;
    uint32 ifeInputResourceSize;
    ISPInResourceInfo*  pInputResource = NULL;
    ISPOutResourceInfo* pOutputResource = NULL;
    uint32 numInputSource = 1;
    uint32 numOutputSource = 1;
    uint32 resourceSize = 0;


    ifeInputResourceSize = (numInputSource * sizeof(ISPInResourceInfo)) +
                            ((numOutputSource - 1) * sizeof(ISPOutResourceInfo));
    m_IFEResourceSize    = ifeInputResourceSize + sizeof(ISPAcquireHWInfo);
    m_pIFEResourceInfo   = static_cast<ISPAcquireHWInfo*>(CameraAllocate(CAMERA_ALLOCATE_ID_VFE_DEVICE, m_IFEResourceSize));

    // Update the IFE resource structure so that KMD will decide which structure to use while acquiriing.
    m_pIFEResourceInfo->commonInfoVersion = ISPAcquireCommonVersion0;
    m_pIFEResourceInfo->commonInfoSize    = 0;
    m_pIFEResourceInfo->commonInfoOffset  = 0;
    m_pIFEResourceInfo->numInputs         = 1; //total output ports
    m_pIFEResourceInfo->inputInfoVersion  = ISPAcquireInputVersion0;
    m_pIFEResourceInfo->inputInfoSize     = ifeInputResourceSize;
    m_pIFEResourceInfo->inputInfoOffset   = 0;

    //Setup the input and output resources
    //One to one map input to output port
    resourceSize = 0;
    pInputResource = reinterpret_cast<ISPInResourceInfo*>
                                (reinterpret_cast<uint8*>(&m_pIFEResourceInfo->data) + resourceSize);

    pInputResource->resourceType = m_rdiCfg->input_resourceType;

    //some initial hardcoding here.  Need to get this from
    //the CSID config.
    pInputResource->laneType     = IFELaneTypeDPHY;
    pInputResource->laneNum      = m_rdiCfg->rdi_cfg.numLanes;
    pInputResource->laneConfig   = 0x3210;  //default lane mapping TODO:  retrieve this from user settings
    pInputResource->VC           = m_rdiCfg->rdi_cfg.vc;
    pInputResource->DT           = m_rdiCfg->rdi_cfg.dt;
    pInputResource->format       = m_rdiCfg->input_format;
    pInputResource->height       = m_rdiCfg->rdi_cfg.height;
    pInputResource->lineStart    = 0;
    pInputResource->lineStop     = m_rdiCfg->rdi_cfg.height - 1;
    pInputResource->batchSize    = 0;
    pInputResource->DSPMode      = 0;
    pInputResource->HBICount     = 0;
    pInputResource->testPattern  = ISPPatternBayerRGRGRG;
    pInputResource->usageType    = ISPResourceUsageSingle;

    pInputResource->leftStart    = 0;
    pInputResource->leftStop     = m_rdiCfg->rdi_cfg.width - 1;
    pInputResource->leftWidth    = m_rdiCfg->rdi_cfg.width;

    pInputResource->numberOutResource = 1;

    //Setup the output port
    pOutputResource = reinterpret_cast<ISPOutResourceInfo*>(&(pInputResource->pDataField));

    resourceSize += sizeof(ISPInResourceInfo) + (sizeof(ISPOutResourceInfo));
    pOutputResource->width                           = m_rdiCfg->rdi_cfg.width;
    pOutputResource->height                          = m_rdiCfg->rdi_cfg.height;
    pOutputResource->resourceType                    = m_rdiCfg->output_resourceType;
    pOutputResource->format                          = m_rdiCfg->input_format;
    pOutputResource->compositeGroupId                = 0;
    pOutputResource->splitPoint                      = 0;
    pOutputResource->secureMode                      = 0;

    //Using reserved field to pass IfeId to KMD
    pOutputResource->reserved                        = (uint32)m_ifeId;

    //Now we acquire the Hw
    m_deviceResource.deviceResourceParamSize = m_IFEResourceSize;
    m_deviceResource.pDeviceResourceParam    = m_pIFEResourceInfo;
    m_deviceResource.resourceID              = ISPResourceIdPort;

    // acquire hardware context for each interface
    result = IfeAcquireHw(m_camDeviceResource, m_rdiIndex, &m_deviceResource);

    return result;
}

CameraResult IFEStream::IfeReleaseHW(CamDeviceHandle deviceHandle, uint32 intfNum)
{
    struct cam_release_hw_cmd_v1    releaseHWCmd;
    struct cam_control              ioctlCmd;
    CameraResult                    result          = CAMERA_SUCCESS;
    int                             returnCode;

    if (intfNum < IFE_MAX_NUM_INTERFACES)
    {
        releaseHWCmd.struct_version = CAM_ACQUIRE_HW_STRUCT_VERSION_1;
        releaseHWCmd.session_handle = m_session_handle;
        releaseHWCmd.dev_handle     = deviceHandle;

        ioctlCmd.op_code        = CAM_RELEASE_HW;
        ioctlCmd.size           = sizeof(struct cam_release_hw_cmd_v1);
        ioctlCmd.handle_type    = CAM_HANDLE_USER_POINTER;
        ioctlCmd.reserved       = 0;
        ioctlCmd.handle         = VoidPtrToUINT64(&releaseHWCmd);

        returnCode = ioctl(m_ifeKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
        if (0 != returnCode)
        {
            CAM_MSG(ERROR, "IFE%d: ioctl failed IfeReleaseHW index %d", m_ifeId, returnCode);
            result = CAMERA_EFAILED;
        }
        else
        {
            CAM_MSG(HIGH, "IFE%d: ioctl success IfeReleaseHW for index %d devhandle 0x%x",m_ifeId, intfNum, deviceHandle);
        }
    }
    else
    {
        CAM_MSG(ERROR, "Invalid: intfNum = %d", intfNum);
        result = CAMERA_EFAILED;
    }

    return result;
}

CameraResult IFEStream::IfeMapNativeBufferHW(
    vfe_cmd_output_buffer* cmd_buf,
    uint32 bufIdx)
{
    CameraResult             result = CAMERA_SUCCESS;
    size_t size = (size_t)cmd_buf->daMax[0] - (size_t)cmd_buf->da[0];
    size_t offset = 0;
    uint32 flags = CAM_MEM_FLAG_HW_READ_WRITE;
    int fd = (int)(uintptr_t)cmd_buf->da[0];

    result = m_camReqMgr->MapNativeBuffer(&m_bufferInfo[bufIdx], fd, offset, size, flags, m_ifeQueryCap);

    if (CAMERA_SUCCESS == result)
    {
        m_bufferInfo[bufIdx].mappedToKMD = TRUE;

        result = m_pSyncMgr->SyncCreate("tmp", &m_syncObj[bufIdx]);
    }
    if (CAMERA_SUCCESS == result)
    {
        CAM_MSG(HIGH, "synccreate is success");
        result = m_pSyncMgr->SyncRegisterPayload(m_syncObj[bufIdx], m_ifeId, bufIdx, m_rdiIndex);
    }
    if (CAMERA_SUCCESS == result)
    {
        CAM_MSG(HIGH, "SyncRegisterPayload is success");
    }
    return result;
}


CameraResult IFEStream::IfeUnMapNativeBufferHw(uint32 bufIdx)
{
    CameraResult result = CAMERA_SUCCESS;

    CAM_MSG(HIGH, "Unmap Native Buffer Idx %d", bufIdx);

    if (bufIdx < MAX_NUM_BUFF)
    {
        if (m_bufferInfo[bufIdx].mappedToKMD)
        {
            result = m_camReqMgr->ReleaseNativeBuffer(&m_bufferInfo[bufIdx]);
            m_bufferInfo[bufIdx].mappedToKMD = FALSE;
        }
    }

    CAM_MSG_ON_ERR(result, "IFE%d: UnMapNative Buffer %d failed result %d", m_ifeId, bufIdx, result);

    return result;
}

CameraResult IFEStream::AddBufferConfigIO(uint32 pktBufIdx)
{
    CameraResult result = CAMERA_SUCCESS;

    CSLPacket* pPacket = reinterpret_cast<CSLPacket*>(pktBuffer[pktBufIdx].pVirtualAddr);
    if (NULL == pPacket)
    {
       CAM_MSG(ERROR, "pPacket is NULL");
       return CAMERA_EBADPARM;
    }

    CSLBufferIOConfig* pIOConfigs = reinterpret_cast<CSLBufferIOConfig*>(VoidPtrInc(pPacket->data, pPacket->ioConfigsOffset));
    if (NULL == pIOConfigs)
    {
        CAM_MSG(ERROR, "pIOConfigs is NULL");
        return CAMERA_EBADPARM;
    }

    memset(&pIOConfigs[pPacket->numBufferIOConfigs], 0x0, sizeof(CSLBufferIOConfig));

    pIOConfigs[pPacket->numBufferIOConfigs].portResourceId        = IFEOutputRDI0 + m_rdiIndex;
    pIOConfigs[pPacket->numBufferIOConfigs].planes[0].planeStride = m_rdiCfg->rdi_cfg.width;;
    pIOConfigs[pPacket->numBufferIOConfigs].planes[0].sliceHeight = m_rdiCfg->rdi_cfg.height;
    pIOConfigs[pPacket->numBufferIOConfigs].planes[0].height      = m_rdiCfg->rdi_cfg.height;
    pIOConfigs[pPacket->numBufferIOConfigs].planes[0].width       = m_rdiCfg->rdi_cfg.width;
    pIOConfigs[pPacket->numBufferIOConfigs].bitsPerPixel          = 8;
    pIOConfigs[pPacket->numBufferIOConfigs].colorFilterPattern    = 0; //Y for RAW 8
    pIOConfigs[pPacket->numBufferIOConfigs].format                = m_rdiCfg->output_format;
    pIOConfigs[pPacket->numBufferIOConfigs].direction             = 2;
    pIOConfigs[pPacket->numBufferIOConfigs].framedropPattern      = 1;
    pIOConfigs[pPacket->numBufferIOConfigs].framedropPeriod       = 0;
    pIOConfigs[pPacket->numBufferIOConfigs].subsamplePattern      = 1;
    pIOConfigs[pPacket->numBufferIOConfigs].subsamplePeriod       = 0;
    pIOConfigs[pPacket->numBufferIOConfigs].hMems[0]              = m_bufferInfo[pktBufIdx].hHandle;
    pIOConfigs[pPacket->numBufferIOConfigs].offsets[0]            = 0;
    pIOConfigs[pPacket->numBufferIOConfigs].hSync                 = m_syncObj[pktBufIdx];

    CAM_MSG(HIGH, "IOConfig[%d] hMems = 0x%x, 0x%x 0x%x",
        pPacket->numBufferIOConfigs,
        pIOConfigs[pPacket->numBufferIOConfigs].hMems[0],
        pIOConfigs[pPacket->numBufferIOConfigs].hMems[1],
        pIOConfigs[pPacket->numBufferIOConfigs].hMems[2]);

    pPacket->numBufferIOConfigs++;

    return result;
}

CameraResult IFEStream::IfeBufferConfigIO(uint32 bufIndex)
{
    CameraResult result = CAMERA_SUCCESS;

    CAM_MSG(HIGH, "Configuring IFE Packet for bufIndex:%d", bufIndex);

    result = AddBufferConfigIO(getPktIdx(bufIndex));

    return result;
}


CameraResult IFEStream::IfeSubmit(CamDeviceHandle deviceHandle,
                                  size_t offset, CSLMemHandle hPacket)
{
    struct cam_control        ioctlCmd;
    struct cam_config_dev_cmd submitCmd;
    CameraResult              result    = CAMERA_SUCCESS;
    int                       returnCode;

    ioctlCmd.op_code         = CAM_CONFIG_DEV;
    ioctlCmd.size            = sizeof(struct cam_config_dev_cmd);
    ioctlCmd.handle_type     = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved        = 0;
    ioctlCmd.handle          = VoidPtrToUINT64(&submitCmd);
    submitCmd.session_handle = m_session_handle;
    submitCmd.dev_handle     = deviceHandle;
    submitCmd.offset         = offset;
    submitCmd.packet_handle  = hPacket;
    CAM_MSG(HIGH, "submitting request for ife:%d devhandle 0x%x", m_ifeId, deviceHandle);
    returnCode = ioctl(m_ifeKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "IFE%d: ioctl failed IfeSubmit index %d devhandle 0x%x",m_ifeId, returnCode, deviceHandle);
        result = CAMERA_EFAILED;
    }
    else
    {
        CAM_MSG(HIGH, "IFE%d: ioctl success IfeSubmit devhandle 0x%x",m_ifeId, deviceHandle);
    }

    return result;
}

CameraResult IFEStream::IfeSubmitHw(uint32 pktBufIdx)
{
    CameraResult result = CAMERA_SUCCESS;

    CSLMemHandle hPacket = pktBuffer[pktBufIdx].pCSLBufferInfo->hHandle;

    result = IfeSubmit(m_camDeviceResource, pktBuffer[pktBufIdx].offset, hPacket);

    CSLPacket* pPacket = reinterpret_cast<CSLPacket*>(pktBuffer[pktBufIdx].pVirtualAddr);
    if (NULL == pPacket)
    {
        CAM_MSG(ERROR, "pPacket is NULL");
        return CAMERA_EBADPARM;
    }

    CSLBufferIOConfig* pIOConfigs = reinterpret_cast<CSLBufferIOConfig*>(VoidPtrInc(pPacket->data, pPacket->ioConfigsOffset));
    if (NULL == pIOConfigs)
    {
        CAM_MSG(ERROR, "pIOConfigs is NULL");
        return CAMERA_EBADPARM;
    }

    CAM_MSG(HIGH, "%d IOConfig[0] hMems = 0x%x, 0x%x 0x%x",
        pktBufIdx,
        pIOConfigs[0].hMems[0],
        pIOConfigs[0].hMems[1],
        pIOConfigs[0].hMems[2]);

    return result;
}

uint32 IFEStream::WriteGenericBlobData(void* cmdBufPtr, uint32 offset,
                                             uint32 type, char* data, uint32 size)
{
    //Write the data to the command buffer
    CAM_MSG(HIGH, "IFE%d: add command %d size %d",m_ifeId, type);

    CameraResult result = CAMERA_SUCCESS;

    if (((size >> (32 - CSLGenericBlobCmdSizeShift)) > 0) || (((type & (!CSLGenericBlobCmdTypeMask)) > 0)) || 0 != (offset & 0x3) )
    {
        CAM_MSG(ERROR, "Invalid blob params type %d, size %d, offset %d", type, size, offset);
        result = CAMERA_EFAILED;
    }

    uint32  blobSizeWithPaddingInBytes  = (CSLGenericBlobHeaderSizeInDwords * sizeof(uint32)) +
                                          ((size + sizeof(uint32) - 1) / sizeof(uint32)) * sizeof(uint32);

    uint32* pCmd                        = static_cast<uint32*>(cmdBufPtr) + offset/4;
    // Always insert actual blob size in the header
    *pCmd++ = ((size << CSLGenericBlobCmdSizeShift) & CSLGenericBlobCmdSizeMask) |
              ((type << CSLGenericBlobCmdTypeShift) & CSLGenericBlobCmdTypeMask);


    memcpy(reinterpret_cast<byte*>(pCmd), data, size);

    return blobSizeWithPaddingInBytes;
}

CameraResult IFEStream::IfeResetPacketInfo(uint32 pktBufIdx)
{
    CameraResult result = CAMERA_SUCCESS;
    if (pktBufIdx < MAX_CMD_BUFF)
    {
        CSLPacket* pPacket  = reinterpret_cast<CSLPacket*>(pktBuffer[pktBufIdx].pVirtualAddr);
        pPacket->numCmdBuffers = 0;
        pPacket->numBufferIOConfigs = 0;
    }
    else
    {
        CAM_MSG(ERROR, "IFE%x: IFE failed to reset packet invalid pktIdx %d!", pktBufIdx);
        result = CAMERA_EBADPARM;
    }

    return result;
}


CameraResult IFEStream::AddCmdBufferReference(uint32 pktBufIdx, uint32 cmdBufIdx,
    uint32 opcode, uint32 request_id)
{
    CameraResult result = CAMERA_SUCCESS;
    //Add pktBufferInfo reference in pktBufferInfo

    CAM_MSG(HIGH, "IFE%d: add cmd buff reference to pktbuf",m_ifeId);

    if (pktBufIdx < MAX_CMD_BUFF && cmdBufIdx < MAX_CMD_BUFF)
    {
        CSLCmdMemDesc* pCmdDesc;
        CSLPacket* pPacket          = reinterpret_cast<CSLPacket*>(pktBuffer[pktBufIdx].pVirtualAddr);

        pPacket->kmdCmdBufferIndex  = pPacket->numCmdBuffers;
        pPacket->kmdCmdBufferOffset = cmdBuffer[cmdBufIdx].length;

        pPacket->header.opcode      = opcode;
        pPacket->header.requestId   = request_id;

        CSLBufferIOConfig* pIOConfigs = reinterpret_cast<CSLBufferIOConfig*>(VoidPtrInc(pPacket->data, pPacket->ioConfigsOffset));
        CAM_MSG(HIGH, "%d IOConfig[0] hMems = 0x%x, 0x%x 0x%x",
            pktBufIdx,
            pIOConfigs[0].hMems[0],
            pIOConfigs[0].hMems[1],
            pIOConfigs[0].hMems[2]);

        uint32 packetCode = (pPacket->header.opcode << CSLPacketOpcodeShift) & CSLPacketOpcodeMask;
        packetCode |= (IfeDeviceType << CSLPacketOpcodeDeviceShift) & CSLPacketOpcodeDeviceMask;
        pPacket->header.opcode = packetCode;

        //Get command buffer descriptor pointer location
        pCmdDesc = reinterpret_cast<CSLCmdMemDesc*>(VoidPtrInc(pPacket->data, pPacket->cmdBuffersOffset));

        pCmdDesc = pCmdDesc + pPacket->numCmdBuffers;

        CAM_MSG(HIGH, "%d cmdbuffer = 0x%x, 0x%x 0x%x",
            cmdBuffer[cmdBufIdx].pCSLBufferInfo->hHandle, cmdBuffer[cmdBufIdx].offset, pPacket->ioConfigsOffset);

        //add descriptor
        pCmdDesc->hMem     = cmdBuffer[cmdBufIdx].pCSLBufferInfo->hHandle;
        pCmdDesc->offset   = cmdBuffer[cmdBufIdx].offset;
        pCmdDesc->size     = cmdBuffer[cmdBufIdx].size;
        pCmdDesc->length   = cmdBuffer[cmdBufIdx].length;
        pCmdDesc->type     = cmdBuffer[cmdBufIdx].type;
        pCmdDesc->metadata = cmdBuffer[cmdBufIdx].metadata;

        CAM_MSG(HIGH, "pktIdx %d cmdBuffer[%d] = hHandle 0x%x, offset 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
            pktBufIdx, cmdBufIdx,
            cmdBuffer[cmdBufIdx].pCSLBufferInfo->hHandle,
            cmdBuffer[cmdBufIdx].offset,
            cmdBuffer[cmdBufIdx].size,
            cmdBuffer[cmdBufIdx].length,
            cmdBuffer[cmdBufIdx].type,
            cmdBuffer[cmdBufIdx].metadata,
            cmdBuffer[cmdBufIdx].pVirtualAddr);

        pPacket->numCmdBuffers++;
    }
    else
    {
        CAM_MSG(ERROR, "IFE%x: IFE failed to add Cmd buf %d to packet %d!",
                                     m_ifeId, cmdBufIdx, pktBufIdx);
        result = CAMERA_EBADPARM;
    }

    return result;
}

CameraResult IFEStream::InitialSetupAndConfig()
{
    CameraResult result = CAMERA_SUCCESS;


    IFEResourceHFRConfig hfrConfig;
    IFEResourceClockConfig clockConfig;
    IFEResourceBWConfig bwConfig;
    uint32 cmdBytesWritten;


    memset(&hfrConfig, 0, sizeof(hfrConfig));
    memset(&clockConfig, 0, sizeof(clockConfig));
    memset(&bwConfig, 0, sizeof(bwConfig));

    //Setup HFR - do we really need this?  -  Generic blob buffer
    hfrConfig.numPorts = 1;

    hfrConfig.portHFRConfig[0].portResourceId = IFEOutputRDI0 + m_rdiIndex;  //channel ID
    hfrConfig.portHFRConfig[0].framedropPattern = 1;
    hfrConfig.portHFRConfig[0].framedropPeriod = 0;
    hfrConfig.portHFRConfig[0].subsamplePattern = 1;
    hfrConfig.portHFRConfig[0].subsamplePeriod = 0;


    CAM_MSG(HIGH, "IFE%d: InitialSetupAndConfig cmdBuf virtual 0x%x",m_ifeId, cmdBuffer[CMDBUF_INIT_CONFIG_IDX].pVirtualAddr);

    cmdBytesWritten = WriteGenericBlobData(cmdBuffer[CMDBUF_INIT_CONFIG_IDX].pVirtualAddr, 0, IFEGenericBlobTypeHFRConfig, reinterpret_cast<char*>(&hfrConfig), sizeof(IFEResourceHFRConfig));

    cmdBuffer[CMDBUF_INIT_CONFIG_IDX].length = cmdBytesWritten;

    //Setup ResourceClockConfig            -  Generic blob buffer
    clockConfig.usageType        = 0; //ISPResourceUsageSingle
    clockConfig.leftPixelClockHz = 404000000;
    clockConfig.rdiClockHz[0]    = 404000000;  //default clock rate for RDI
    clockConfig.numRdi           = 4;  //RDIMaxNum  TODO: get actual RDI count from QueryCap

    cmdBytesWritten = WriteGenericBlobData(cmdBuffer[CMDBUF_INIT_CONFIG_IDX].pVirtualAddr, cmdBuffer[CMDBUF_INIT_CONFIG_IDX].length, IFEGenericBlobTypeResourceClockConfig,  reinterpret_cast<char*>(&clockConfig), sizeof(IFEResourceClockConfig));
    cmdBuffer[CMDBUF_INIT_CONFIG_IDX].length += cmdBytesWritten;

    //Bandwidth config - Generic blob buffer
    bwConfig.numRdi                     = 4; //TODO: get actual RDI count from QueryCap
    bwConfig.rdiVote[0].camnocBWbytes   = 9051246400;  //hardcoded value from camx calculations - TODO: calculate this based on sensor res
    bwConfig.rdiVote[0].externalBWbytes = 9051246400;

    cmdBytesWritten = WriteGenericBlobData(cmdBuffer[CMDBUF_INIT_CONFIG_IDX].pVirtualAddr, cmdBuffer[CMDBUF_INIT_CONFIG_IDX].length, IFEGenericBlobTypeResourceBWConfig, reinterpret_cast<char*>(&bwConfig), sizeof(bwConfig));
    cmdBuffer[CMDBUF_INIT_CONFIG_IDX].length += cmdBytesWritten;

    AddCmdBufferReference(PKT_INIT_CONFIG_IDX, CMDBUF_INIT_CONFIG_IDX, 0, 1); //Add cmdbuf 0 reference to pktbuf 0 - Initial config
    return result;
}

CameraResult IFEStream::IfeStreamOn(CamDeviceHandle deviceHandle)
{
    struct cam_control        ioctlCmd;
    struct cam_start_stop_dev_cmd streamCmd;
    CameraResult              result    = CAMERA_SUCCESS;
    int                       returnCode;

    ioctlCmd.op_code         = CAM_START_DEV;
    ioctlCmd.size            = sizeof(struct cam_config_dev_cmd);
    ioctlCmd.handle_type     = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved        = 0;
    ioctlCmd.handle          = VoidPtrToUINT64(&streamCmd);
    streamCmd.session_handle = m_session_handle;
    streamCmd.dev_handle     = deviceHandle;

    returnCode = ioctl(m_ifeKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "IFE%d: ioctl failed IfeStreamOn index %d",m_ifeId, returnCode);
        result = CAMERA_EFAILED;
    }
    else
    {
        CAM_MSG(HIGH, "IFE%d: ioctl success IfeStreamOn devhandle 0x%x",m_ifeId, deviceHandle);
    }

    return result;
}

CameraResult IFEStream::IfeStreamOff(CamDeviceHandle deviceHandle)
{
    struct cam_control        ioctlCmd;
    struct cam_start_stop_dev_cmd streamCmd;
    CameraResult              result    = CAMERA_SUCCESS;
    int                       returnCode;

    ioctlCmd.op_code         = CAM_STOP_DEV;
    ioctlCmd.size            = sizeof(struct cam_start_stop_dev_cmd);
    ioctlCmd.handle_type     = CAM_HANDLE_USER_POINTER;
    ioctlCmd.reserved        = 0;
    ioctlCmd.handle          = VoidPtrToUINT64(&streamCmd);
    streamCmd.session_handle = m_session_handle;
    streamCmd.dev_handle     = deviceHandle;

    returnCode = ioctl(m_ifeKmdFd, VIDIOC_CAM_CONTROL, &ioctlCmd);
    if (0 != returnCode)
    {
        CAM_MSG(ERROR, "IFE%d: ioctl failed IfeStreamOff index %d",m_ifeId, returnCode);
        result = CAMERA_EFAILED;
    }
    else
    {
        CAM_MSG(HIGH, "IFE%d: ioctl success IfeStreamOff devhandle 0x%x",m_ifeId, deviceHandle);
    }

    return result;
}

CameraResult IFEStream::IfeSubmitRequest(uint32 bufIdx)
{
    CameraResult result = CAMERA_SUCCESS;

    m_ifeRequestId++;
    bool bubble = FALSE; //TRUE

    result = m_camReqMgr->CRMScheduleRequest(m_session_handle, m_link_handle, m_ifeRequestId, bubble, CAM_REQ_MGR_SYNC_MODE_NO_SYNC);
    if (CAMERA_SUCCESS == result)
    {
        result = AddCmdBufferReference(getPktIdx(bufIdx), getCmdIdx(bufIdx), 1, m_ifeRequestId);
    }
    if (CAMERA_SUCCESS == result)
    {
        result = IfeSubmitHw(getPktIdx(bufIdx));
    }
    CAM_MSG_ON_ERR(result, "IFE%d: IFE submitRequest faile with result = %d for requestId:%d", m_ifeId,  result, m_ifeRequestId);

    return result;

}

void IFEStream::IfeResetPktandCommandBuffers(void)
{
    CSLPacket* pPacket;

    CAM_MSG(HIGH, "IFE%d: clear pktBufferInfo", pktBufferInfo.size);
    for (uint32 j = 0; j < m_ifeCmdBlobCount; j++)
    {
        pPacket = reinterpret_cast<CSLPacket*>(pktBuffer[j].pVirtualAddr);

        //Reset the packet
        memset(pPacket->data, 0, pktBuffer[j].dataSize);

        pPacket->numBufferIOConfigs = 0;
        pPacket->numCmdBuffers      = 0;
        pPacket->numPatches         = 0;
        pPacket->kmdCmdBufferIndex  = static_cast<uint32>(-1);
    }

    //clean up the command buffers
    memset(cmdBufferInfo.pVirtualAddr, 0, cmdBufferInfo.size);
}

CameraResult IFEStream::IfeStreamConfig()
{

    CameraResult result = CAMERA_SUCCESS;

    // acquire a dev context
    result = IfeAcquireDev(&m_camDeviceResource, 0, NULL, 0);

    //Acquire the hw - TODO - This should acquire on first use and release when there are no users
    if (CAMERA_SUCCESS == result)
    {
        result = IfeConfigAcquireHw();
        CAM_MSG_ON_ERR(result, "IFE%d: IFE ConfigAcquireHw failed, with result = %d.", m_ifeId,  result);
    }

    if (CAMERA_SUCCESS == result)
    {
        result = InitialSetupAndConfig();
        CAM_MSG_ON_ERR(result, "IFE%d: IFE Initialsetupconfig failed, with result = %d.", m_ifeId,  result);
    }
    if (CAMERA_SUCCESS == result)
    {
        result = IfeSubmitHw(PKT_INIT_CONFIG_IDX);
        CAM_MSG_ON_ERR(result, "IFE%d: IFE initial submit failed, with result = %d.", m_ifeId,  result);
    }

    if (CAMERA_SUCCESS == result)
    {
        result = m_camReqMgr->LinkDevices(m_camDeviceResource, m_session_handle, &m_link_handle);
        CAM_MSG_ON_ERR(result, "IFE%d: IFE linkdevices failed, with result = %d.", m_ifeId,  result);
    }

    return result;
}

CameraResult IFEStream::IfeStreamStart(ife_start_cmd_t* p_start_cmd)
{
    CameraResult result = CAMERA_SUCCESS;

    result = IfeStreamOn(m_camDeviceResource);
    CAM_MSG_ON_ERR(result, "IFE%d: IFE streamOn failed, with result = %d.", m_ifeId,  result);

    if (CAMERA_SUCCESS == result)
    {
        m_IfeStreamOn = TRUE;
        m_ifeRequestId = 0;

        for (uint32 i = 0; i < p_start_cmd->numOfOutputBufs; i++)
        {
            result = IfeSubmitRequest(i);
        }
        CAM_MSG_ON_ERR(result, "IFE%d: IFE submit IO Config failed, with result = %d.", m_ifeId,  result);
    }

    return result;
}

CameraResult IFEStream::IfeStreamStop(ife_stop_cmd_t* p_stop_cmd)
{
    CameraResult result = CAMERA_SUCCESS;

    result = IfeStreamOff(m_camDeviceResource);

    if (CAMERA_SUCCESS == result)
    {
        m_IfeStreamOn = FALSE;
        result = m_camReqMgr->UnlinkDevices(m_camDeviceResource, m_session_handle, &m_link_handle);
    }
    if (CAMERA_SUCCESS == result)
    {
        result = IfeReleaseHW(m_camDeviceResource, m_rdiIndex);
    }

    if (CAMERA_SUCCESS == result)
    {
        result = IfeReleaseDev(&m_camDeviceResource);
    }

    //Release the KMD mapping
    if (CAMERA_SUCCESS == result)
    {
        for (uint32 i = 0; i < MAX_NUM_BUFF; i++)
        {
            result = IfeUnMapNativeBufferHw(i);
            if (CAMERA_SUCCESS != result)
            {
                break;
            }
        }
    }

    IfeResetPktandCommandBuffers();

    // Reset the requestId to 1
    m_ifeRequestId = 1;

    return result;
}


CameraResult IFEStream::IfeStreamResume()
{
    CameraResult result = CAMERA_SUCCESS;

    result = IfeStreamOn(m_camDeviceResource);

    return result;
}




CameraResult IFEStream::IfeStreamPause()
{
    CameraResult result = CAMERA_SUCCESS;

    result = IfeStreamOff(m_camDeviceResource);

    m_ifeRequestId = 1;

    IfeSubmitHw(PKT_INIT_CONFIG_IDX);//change the isp status to READY

    return result;
}

CameraResult IFEStream::IfeStreamBufferEnqueue(vfe_cmd_output_buffer* pBuf)
{

    CameraResult result = CAMERA_SUCCESS;

    if (FALSE == m_IfeStreamOn)
    {
        result = IfeMapNativeBufferHW(pBuf, pBuf->bufIndex);
        if (CAMERA_SUCCESS == result)
        {
            result = IfeBufferConfigIO(pBuf->bufIndex);
        }
    }
    else
    {
        ////usleep(20000);
        result = m_pSyncMgr->SyncRelease(m_syncObj[pBuf->bufIndex]);
        if (CAMERA_SUCCESS != result)
        {
            CAM_MSG(ERROR, "sync release failed on %d",m_syncObj[pBuf->bufIndex]);
        }
        //Recreate sync and register payload
        if (CAMERA_SUCCESS == result)
        {
            result = m_pSyncMgr->SyncCreate("tmp", &m_syncObj[pBuf->bufIndex]);
            CAM_MSG(HIGH, "sync re-created m_syncObj[%d]=%d",pBuf->bufIndex,m_syncObj[pBuf->bufIndex]);
        }
        else
        {
            CAM_MSG(ERROR, "Sync recreation failed");
        }
        if (CAMERA_SUCCESS == result)
        {
            result = m_pSyncMgr->SyncRegisterPayload(m_syncObj[pBuf->bufIndex], m_ifeId, pBuf->bufIndex, m_rdiIndex);
        }
        else
        {
            CAM_MSG(ERROR, "Sync payload reload failed");
        }

        //Reset the pktBuf info
        result = IfeResetPacketInfo(getPktIdx(pBuf->bufIndex));

        //Remap the buffer
        if (CAMERA_SUCCESS == result)
        {
            result = IfeBufferConfigIO(pBuf->bufIndex);
        }

        if (CAMERA_SUCCESS == result)
        {
            result = IfeSubmitRequest(pBuf->bufIndex);
        }

        CAM_MSG_ON_ERR(result, "IFE%d: IFE submit IO Config failed, with result = %d.", m_ifeId,  result);
    }

    return result;
}
uint32 IFEStream::getPktIdx(uint32 bufIdx)
{
    return bufIdx;
}

uint32 IFEStream::getCmdIdx(uint32 bufIdx)
{
    return bufIdx;
}





