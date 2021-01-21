/**
 * @file CameraPlatform.c
 *
 * @brief Implementation of the CameraPlatform methods.
 *
 * Copyright (c) 2011-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include "AEEstd.h"

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/media.h>
#include <media/cam_defs.h>
#include <media/cam_req_mgr.h>
#include <media/cam_sync.h>
#include <media/ais_sensor.h>

#include "CameraOSServices.h"
#include "CameraPlatform.h"
#include "CameraPlatformLinux.h"
#include "CameraConfig.h"

/*============================================================================
                        INTERNAL DEFINITIONS
============================================================================*/
/* Linux platform types */
enum {
    PLATFORM_SUBTYPE_SA8155_ADP_STAR    = 0x0,
    PLATFORM_SUBTYPE_SA8155_ADP_AIR     = 0x1,
    PLATFORM_SUBTYPE_SA8155_ADP_ALCOR   = 0x2,
    PLATFORM_SUBTYPE_SA8155_ADP_INVALID,
};

#define MAX_NUM_SUBDEVS 16
#define MAX_PATH_SIZE 64

#define COPY_DEV_PATH(_to_, _from_) \
    snprintf(_to_, sizeof(_to_), "/dev/%s", _from_);

typedef struct
{
    ais_subdev_id_t id;
    uint32 subdev_idx;
    char path[MAX_PATH_SIZE];
    int fd;
    uint32 kmdDevHandle;
    uint32 kmdSessionHandle;
}ais_subdev_t;

typedef struct
{
    boolean bInitialized;

    ais_subdev_t sAisSubdevs[MAX_NUM_SUBDEVS];
    uint32 numSubdevs;

    //For camera config
    void*  hCameraConfig;
    const  CameraBoardType *pBoardInfo;
    const  ICameraConfigInitDeinit *pCameraConfigInitDeinitIf;
    ICameraConfigPower const *pCameraConfigPowerIf;
    CameraChannelInfoType const * pChannelInfo;
    int nChannels;

    CameraPlatformChipIdType chipId;
    uint32                   chipVersion;
    CameraPlatformTitanVersion titanVersion;
    CameraPlatformTitanHWVersion titanHwVersion;
    CameraPlatformIdType platformId;
}CameraPlatformContext_t;


static CameraPlatformContext_t gPlatformCtxt =
{
    .bInitialized = FALSE,
};
/*============================================================================
                        INTERNAL API DECLARATIONS
============================================================================*/
static boolean CameraPlatformLoadConfigLib(void);
static void    CameraPlatformUnloadConfigLib(void);

/*============================================================================
                        EXTERNAL API DEFINITIONS
============================================================================*/
int CameraPlatform_GetCsidCore(CameraSensorIndex idx)
{
    int csidCore = -1;
    if (NULL != gPlatformCtxt.pBoardInfo && idx < CAMSENSOR_ID_MAX)
    {
        csidCore = gPlatformCtxt.pBoardInfo->camera[idx].csiId;
    }
    else
    {
        CAM_MSG(ERROR, "Board not initialized, returning -1");
    }

    return csidCore;
}

boolean CameraPlatformReadHWVersion()
{
    boolean result = FALSE;
    int32  socFd;
    char   buf[32]  = { 0 };
    int32  chipsetVersion = -1;
    char   fileName[] = "/sys/devices/soc0/soc_id";
    int    ret = 0;

    socFd = open(fileName, O_RDONLY);

    if (socFd > 0)
    {
        ret = read(socFd, buf, sizeof(buf) - 1);
        if (-1 == ret)
        {
            CAM_MSG(ERROR, "Unable to read soc_id");
        }
        else
        {
            gPlatformCtxt.chipId = atoi(buf);
            result = TRUE;
        }

        close(socFd);
    }

    return result;
}

boolean CameraPlatformReadPlatformId()
{
    boolean result = FALSE;
    int32  socFd;
    char   buf[32]  = { 0 };
    int32  platformId = -1;
    char   fileName[] = "/sys/devices/soc0/platform_subtype_id";
    int    ret = 0;

    socFd = open(fileName, O_RDONLY);

    if (socFd > 0)
    {
        ret = read(socFd, buf, sizeof(buf) - 1);
        if (-1 == ret)
        {
            CAM_MSG(ERROR, "Unable to read platform_subtype_id");
        }
        else
        {
            platformId = atoi(buf);
            result = TRUE;
        }

        close(socFd);
    }
    /* Map the Linux platform type to CameraPlatformIdType */
    switch (platformId)
    {
    case PLATFORM_SUBTYPE_SA8155_ADP_STAR:
    {
        gPlatformCtxt.platformId = HARDWARE_PLATFORM_ADP_STAR;
        break;
    }
    case PLATFORM_SUBTYPE_SA8155_ADP_AIR:
    {
        gPlatformCtxt.platformId = HARDWARE_PLATFORM_ADP_AIR;
        break;
    }
    case PLATFORM_SUBTYPE_SA8155_ADP_ALCOR:
    {
        gPlatformCtxt.platformId = HARDWARE_PLATFORM_ADP_ALCOR;
        break;
    }
    default:
        CAM_MSG(ERROR, "%s: unsupported platform type %d", __FUNCTION__, platformId);
        gPlatformCtxt.platformId = HARDWARE_PLATFORM_INVALID;
        break;
    }

    return result;
}

void CameraPlatformSetKMDHandle(ais_kmd_handle_type type, ais_subdev_id_t id, uint32 subdev_idx, uint32 handle)
{
    int i = 0;

    CAM_MSG(HIGH, "Entry SetKMD type %d id %d subdevidx %d devHandler 0x%x", type, id, subdev_idx, handle );

    for (i = 0; i < MAX_NUM_SUBDEVS; i++)
    {
        if (id == gPlatformCtxt.sAisSubdevs[i].id &&
                subdev_idx == gPlatformCtxt.sAisSubdevs[i].subdev_idx)
        {
            if( AIS_HANDLE_TYPE_SESSION == type)
            {
                gPlatformCtxt.sAisSubdevs[i].kmdSessionHandle = handle;
                CAM_MSG(HIGH, "Success SetKMD id %d subdevidx %d sessionHandler 0x%x",id, subdev_idx, handle );
            }
            else if(AIS_HANDLE_TYPE_DEVICE == type)
            {

                gPlatformCtxt.sAisSubdevs[i].kmdDevHandle = handle;
                CAM_MSG(HIGH, "Success SetKMD id %d subdevidx %d devHandler 0x%x",id, subdev_idx, handle );
            }
            else
            {
                CAM_MSG(HIGH, "Failed SetKMD type %d id %d subdevidx %d type 0x%x",type, id, subdev_idx);
            }
            break;
        }
    }
}

uint32 CameraPlatformGetKMDHandle(ais_kmd_handle_type type, ais_subdev_id_t id, uint32 subdev_idx)
{
    int i = 0;
    uint32 handle = 0;

    CAM_MSG(HIGH, "Entry GetKMD type %d id %d subdevidx %d",type, id, subdev_idx);

    for (i = 0; i < MAX_NUM_SUBDEVS; i++)
    {
        if (id == gPlatformCtxt.sAisSubdevs[i].id &&
                subdev_idx == gPlatformCtxt.sAisSubdevs[i].subdev_idx)
        {
            if( AIS_HANDLE_TYPE_SESSION == type)
            {
                handle = gPlatformCtxt.sAisSubdevs[i].kmdSessionHandle;
                CAM_MSG(HIGH, "Success GetKMD id %d subdevidx %d sessionHandler 0x%x",id, subdev_idx, handle );
            }
            else if(AIS_HANDLE_TYPE_DEVICE == type)
            {
                handle = gPlatformCtxt.sAisSubdevs[i].kmdDevHandle;

                CAM_MSG(HIGH, "Success GetKMD id %d subdevidx %d devHandler 0x%x",id, subdev_idx, handle );
            }
            else
            {
                CAM_MSG(HIGH, "Failed GetKMD id %d subdevidx %d type 0x%x",id, subdev_idx, type );
            }
            break;
        }
    }

    return handle;
}


const char* CameraPlatformGetDevPath(ais_subdev_id_t id, uint32 subdev_idx)
{
    int i=0;
    for (i=0; i<MAX_NUM_SUBDEVS; i++)
    {
        if (id == gPlatformCtxt.sAisSubdevs[i].id &&
                subdev_idx == gPlatformCtxt.sAisSubdevs[i].subdev_idx)
            return gPlatformCtxt.sAisSubdevs[i].path;
    }

    return NULL;
}

int CameraPlatformGetFd(ais_subdev_id_t id, uint32 subdev_idx)
{
    int i = 0, fd = 0;

    for (i = 0; i < MAX_NUM_SUBDEVS; i++)
    {
        if (id == gPlatformCtxt.sAisSubdevs[i].id &&
                subdev_idx == gPlatformCtxt.sAisSubdevs[i].subdev_idx)
        {
            fd = gPlatformCtxt.sAisSubdevs[i].fd;
            break;
        }
    }

    return fd;
}

void CameraPlatformClearFd(ais_subdev_id_t id, uint32 subdev_idx)
{
    int i = 0;

    for (i = 0; i < MAX_NUM_SUBDEVS; i++)
    {
        if (id == gPlatformCtxt.sAisSubdevs[i].id &&
                subdev_idx == gPlatformCtxt.sAisSubdevs[i].subdev_idx)
        {
            gPlatformCtxt.sAisSubdevs[i].fd = 0;
            break;
        }
    }
}

static boolean CameraPlatformLoadDevPaths(CameraPlatformContext_t* pCtxt)
{
    struct media_device_info mdev_info;
    int num_media_devices = 0;
    char dev_name[32];
    int rc = 0, dev_fd = 0, sd_fd = 0;
    int num_entities;
    int free_idx = 0;
    int vfe_idx = 0;

    CAM_MSG(HIGH, "%s: E", __func__);

    while (1)
    {
        struct media_entity_desc entity;
        memset(&entity, 0, sizeof(entity));

        snprintf(dev_name, sizeof(dev_name), "/dev/media%d", num_media_devices);
        dev_fd = open(dev_name, O_RDWR | O_NONBLOCK);
        if (dev_fd < 0)
        {
            CAM_MSG(HIGH, "Done discovering media devices");
            break;
        }

        num_media_devices++;
        rc = ioctl(dev_fd, MEDIA_IOC_DEVICE_INFO, &mdev_info);
        if (rc < 0)
        {
            CAM_MSG(ERROR, "Error: ioctl media_dev failed: %s", strerror(errno));
            close(dev_fd);
            return rc;
        }

        if (strncmp(mdev_info.model, CAM_REQ_MGR_VNODE_NAME, sizeof(mdev_info.model)) == 0)
        {
            pCtxt->sAisSubdevs[pCtxt->numSubdevs].id = AIS_SUBDEV_REQMGR_NODE;
            pCtxt->sAisSubdevs[pCtxt->numSubdevs].fd = dev_fd;
            pCtxt->numSubdevs++;

            num_entities = 1;
            while (1)
            {
                entity.id = num_entities;
                rc = ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
                if (rc < 0) {
                    CAM_MSG(HIGH, "Done enumerating media entities");
                    close(dev_fd);
                    rc = 0;
                    break;
                }

                num_entities = entity.id | MEDIA_ENT_ID_FLAG_NEXT;

                CAM_MSG(HIGH, "entity name %s type %d group id %d",
                    entity.name, entity.type, entity.group_id);

                free_idx = pCtxt->numSubdevs;

                switch (entity.type)
                {
                case CAM_IFE_DEVICE_TYPE:
                {
                    pCtxt->sAisSubdevs[free_idx].id = AIS_SUBDEV_IFE;
                    pCtxt->sAisSubdevs[free_idx].subdev_idx = vfe_idx++;
                    COPY_DEV_PATH(pCtxt->sAisSubdevs[free_idx].path, entity.name);

                    sd_fd = open(pCtxt->sAisSubdevs[free_idx].path, O_RDWR | O_NONBLOCK);
                    if (sd_fd <= 0) {
                        CAM_MSG(ERROR, "%s: cannot open '%s'", __func__, pCtxt->sAisSubdevs[free_idx].path);
                        continue;
                    }
                    pCtxt->sAisSubdevs[free_idx].fd = sd_fd;
                    pCtxt->numSubdevs++;
                    break;
                }
                case CAM_CSIPHY_DEVICE_TYPE:
                {
                    struct cam_control cam_cmd;
                    struct cam_csiphy_query_cap csiphy_cap;

                    pCtxt->sAisSubdevs[free_idx].id = AIS_SUBDEV_CSIPHY;

                    COPY_DEV_PATH(pCtxt->sAisSubdevs[free_idx].path, entity.name);
                    sd_fd = open(pCtxt->sAisSubdevs[free_idx].path, O_RDWR | O_NONBLOCK);
                    if (sd_fd < 0) {
                        CAM_MSG(ERROR, "Open subdev failed: %s", pCtxt->sAisSubdevs[free_idx].path);
                        continue;
                    }

                    memset(&cam_cmd, 0x0, sizeof(cam_cmd));
                    memset(&csiphy_cap, 0x0, sizeof(csiphy_cap));

                    cam_cmd.op_code     = CAM_QUERY_CAP;
                    cam_cmd.size        = sizeof(csiphy_cap);
                    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
                    cam_cmd.reserved    = 0;
                    cam_cmd.handle      = (uint64_t)&csiphy_cap;
                    rc = ioctl(sd_fd, VIDIOC_CAM_CONTROL, &cam_cmd);
                    if (rc < 0) {
                        CAM_MSG(ERROR, "%s query rc %d", pCtxt->sAisSubdevs[free_idx].path, rc);
                        close(sd_fd);
                        continue;
                    }

                    pCtxt->sAisSubdevs[free_idx].subdev_idx = csiphy_cap.slot_info;
                    pCtxt->sAisSubdevs[free_idx].fd = sd_fd;
                    pCtxt->numSubdevs++;
                    break;
                }
                case CAM_SENSOR_DEVICE_TYPE:
                {
                    struct cam_control cam_cmd;
                    struct cam_sensor_query_cap sensor_cap;

                    pCtxt->sAisSubdevs[free_idx].id = AIS_SUBDEV_SENSOR;

                    COPY_DEV_PATH(pCtxt->sAisSubdevs[free_idx].path, entity.name);
                    sd_fd = open(pCtxt->sAisSubdevs[free_idx].path, O_RDWR | O_NONBLOCK);
                    if (sd_fd < 0) {
                        CAM_MSG(ERROR, "Open subdev failed: %s", pCtxt->sAisSubdevs[free_idx].path);
                        continue;
                    }

                    memset(&cam_cmd, 0x0, sizeof(cam_cmd));
                    memset(&sensor_cap, 0x0, sizeof(sensor_cap));

                    cam_cmd.op_code     = CAM_QUERY_CAP;
                    cam_cmd.size        = sizeof(sensor_cap);
                    cam_cmd.handle_type = CAM_HANDLE_USER_POINTER;
                    cam_cmd.reserved    = 0;
                    cam_cmd.handle      = (uint64_t)&sensor_cap;
                    rc = ioctl(sd_fd, VIDIOC_CAM_CONTROL, &cam_cmd);
                    if (rc < 0) {
                        CAM_MSG(ERROR, "%s query rc %d", pCtxt->sAisSubdevs[free_idx].path, rc);
                        close(sd_fd);
                        continue;
                    }
                    close(sd_fd);

                    pCtxt->sAisSubdevs[free_idx].subdev_idx = sensor_cap.slot_info;

                    CAM_MSG(HIGH, "[%d] added subdev %d idx %d",
                        free_idx, pCtxt->sAisSubdevs[free_idx].id,
                        pCtxt->sAisSubdevs[free_idx].subdev_idx);

                    pCtxt->numSubdevs++;
                    break;
                }
                case CAM_VNODE_DEVICE_TYPE:
                {
                    pCtxt->sAisSubdevs[free_idx].id = AIS_SUBDEV_REQMGR;
                    COPY_DEV_PATH(pCtxt->sAisSubdevs[free_idx].path, entity.name);
                    sd_fd = open(pCtxt->sAisSubdevs[free_idx].path, O_RDWR | O_NONBLOCK);
                    if (sd_fd <= 0) {
                        CAM_MSG(ERROR, "%s: cannot open '%s'", __func__, pCtxt->sAisSubdevs[free_idx].path);
                        continue;
                    }
                    else
                    {
                        CAM_MSG(HIGH, "Opened the Camera request manager");
                    }
                    pCtxt->sAisSubdevs[free_idx].fd = sd_fd;
                    pCtxt->numSubdevs++;
                    break;
                }
                default:
                    CAM_MSG(ERROR, "Unknown: %s - %d", entity.name, entity.group_id);
                    break;
                }
            }
        }
        else if (strncmp(mdev_info.model, CAM_SYNC_DEVICE_NAME, sizeof(mdev_info.model)) == 0)
        {
            pCtxt->sAisSubdevs[pCtxt->numSubdevs].id = AIS_SUBDEV_SYNCMGR_NODE;
            pCtxt->sAisSubdevs[pCtxt->numSubdevs].fd = dev_fd;
            pCtxt->numSubdevs++;

            num_entities = 1;
            while (1)
            {
                entity.id = num_entities;
                rc = ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
                if (rc < 0) {
                    CAM_MSG(HIGH, "Done enumerating media entities");
                    close(dev_fd);
                    rc = 0;
                    break;
                }

                num_entities = entity.id | MEDIA_ENT_ID_FLAG_NEXT;

                CAM_MSG(HIGH, "entity name %s type %d group id %d",
                    entity.name, entity.type, entity.group_id);

                free_idx = pCtxt->numSubdevs;

                switch (entity.type)
                {
                case CAM_SYNC_DEVICE_TYPE:
                {
                    pCtxt->sAisSubdevs[free_idx].id = AIS_SUBDEV_SYNCMGR;
                    COPY_DEV_PATH(pCtxt->sAisSubdevs[free_idx].path, entity.name);
                    sd_fd = open(pCtxt->sAisSubdevs[free_idx].path, O_RDWR | O_NONBLOCK);
                    if (sd_fd <= 0) {
                        CAM_MSG(ERROR, "%s: cannot open '%s'", __func__, pCtxt->sAisSubdevs[free_idx].path);
                        continue;
                    }
                    else
                    {
                        CAM_MSG(HIGH, "Opened the Camera SYNC manager");
                    }
                    pCtxt->sAisSubdevs[free_idx].fd = sd_fd;
                    pCtxt->numSubdevs++;
                    break;
                }
                }
            }
        }
        else
        {
            close(dev_fd);
            continue;
        }
    }

    return TRUE;
}

#if 0
static int CameraPlatformSetClkStatus(CameraPlatformContext_t* pCtxt, unsigned int value)
{
    int rc = 0;
    unsigned int i = 0;

    CAM_MSG(HIGH, "%s: E", __func__);
    for (i = 0; i < pCtxt->numSubdevs; i++)
    {
        if (pCtxt->sAisSubdevs[i].id == AIS_SUBDEV_IFE)
        {
            if (pCtxt->sAisSubdevs[i].fd)
            {
                rc = ioctl(pCtxt->sAisSubdevs[i].fd, VIDIOC_MSM_ISP_SET_CLK_STATUS, &value);
                if (rc < 0) {
                    CAM_MSG(ERROR, "%s set clk status %d failed: rc %d",
                               pCtxt->sAisSubdevs[i].path, value, rc);
                    return rc;
                }

                CAM_MSG(HIGH, "[%d] Set clk status subdev %s, value %d, subdev_idx %d",
                            i, pCtxt->sAisSubdevs[i].path, value,
                            pCtxt->sAisSubdevs[i].subdev_idx);
            }
            else
            {
                CAM_MSG(ALWAYS, "[%d] VFE fd 0, subdev %s, subdev_idx %d",
                            i, pCtxt->sAisSubdevs[i].path,
                            pCtxt->sAisSubdevs[i].subdev_idx);
            }
        }
    }
    CAM_MSG(HIGH, "%s: E", __func__);

    return rc;
}
#endif

/*===========================================================================

FUNCTION
    CameraRegisterWithSleep

DESCRIPTION
    This function registers the camera with the sleep module

DEPENDENCIES
    None

RETURN VALUE
    None

SIDE EFFECTS
    None

===========================================================================*/
void CameraRegisterWithSleep (void)
{
}

/*===========================================================================

FUNCTION
    CameraDeregisterWithSleep

DESCRIPTION
    This function de-registers the camera with the sleep module

DEPENDENCIES
    None

RETURN VALUE
    None

SIDE EFFECTS
    None

===========================================================================*/
void CameraDeregisterWithSleep (void)
{
}

/*===========================================================================

FUNCTION
    CameraPermitSleep

DESCRIPTION
    This function indicates to the sleep module that it is ok to sleep

DEPENDENCIES
    None

RETURN VALUE
    None

SIDE EFFECTS
    None

===========================================================================*/
void CameraPermitSleep (void)
{
}

/*===========================================================================

FUNCTION
    CameraProhibitSleep

DESCRIPTION
    This function indicates to the sleep module that it is not ok to sleep

DEPENDENCIES
    None

RETURN VALUE
    None

SIDE EFFECTS
    None

===========================================================================*/
void CameraProhibitSleep (void)
{
}

/*===========================================================================

FUNCTION      CameraPlatform_WaitMicroSeconds

DESCRIPTION
              Wait for the specfied number of microseconds

DEPENDENCIES
  None

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
void CameraPlatform_WaitMicroSeconds (uint32 nMicroSecToWait)
{
    (void)nMicroSecToWait;
}


/*===========================================================================

FUNCTION
    CameraSensorGPIOConfigCamera

DESCRIPTION
    Configure GPIO pin for Camera Interface.

DEPENDENCIES
    None

RETURN VALUE
    TRUE if success
    FALSE if fail

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorGPIOConfigCamera (CameraSensorIndex idx)
{
    (void)idx;
    return TRUE;
}

/*===========================================================================

FUNCTION
    CameraSensorGPIOUnConfigCamera

DESCRIPTION
    Unconfigure GPIO pin for Camera Interface.

DEPENDENCIES
    None

RETURN VALUE
    TRUE if success
    FALSE if fail

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorGPIOUnConfigCamera(CameraSensorIndex idx)
{
    (void)idx;
    return TRUE;
}

/*===========================================================================

FUNCTION      CameraSensorGPIO_In

DESCRIPTION
              Read current logic level of gpio pin.

DEPENDENCIES
  None

RETURN VALUE
  TRUE if success
  FALSE if fail

SIDE EFFECTS
  None

===========================================================================*/
boolean CameraSensorGPIO_In (CameraSensorGPIO_SignalType GPIOSignal,
                             byte * pValue)
{
    (void)GPIOSignal;
    (void)pValue;
    return TRUE;
}

/*===========================================================================

FUNCTION      CameraSensorGPIO_Out

DESCRIPTION
              Set the logic level of GPIO.

DEPENDENCIES
  None

RETURN VALUE
  TRUE if success
  FALSE if fail

SIDE EFFECTS
  None

===========================================================================*/
boolean CameraSensorGPIO_Out (CameraSensorIndex idx,
                              CameraSensorGPIO_SignalType GPIOSignal,
                              CameraSensorGPIO_ValueType  GPIOValue)
{
    (void) idx;
    (void)GPIOSignal;
    (void)GPIOValue;
    return TRUE;
}

/*===========================================================================

FUNCTION
    CameraSensorCLKConfigMclk

DESCRIPTION
    This function is used to configure the required master clock for
    the camera sensor for the given index. nCameraSensorMCLKHz is the
    frequency to set the clock to in Hertz.

DEPENDENCIES
    None

RETURN VALUE
    TRUE - if successful
    FALSE - if failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorCLKConfigMclk (CameraSensorIndex idx,
    uint32 nCameraSensorMCLKHz)
{
    // Do nothing; clocks configured using ACPI
    (void)idx;
    (void)nCameraSensorMCLKHz;
    return TRUE;
}

/*===========================================================================

FUNCTION
    CameraSensorCLKUnConfigMclk

DESCRIPTION
    This function is used to configure master clock GPIO back to default.

DEPENDENCIES
    None

RETURN VALUE
    TRUE - if successful
    FALSE - if failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorCLKUnConfigMclk(CameraSensorIndex idx)
{
    (void)idx;
    return TRUE;
}

/* ===========================================================================

FUNCTION
    CameraPlatformInit

DESCRIPTION
    This function initializes CameraPlatform

DEPENDENCIES
    None

RETURN VALUE
    None

SIDE EFFECTS
    None

=========================================================================== */
boolean CameraPlatformInit(void)
{
    int i;
    boolean result = TRUE;

    if (gPlatformCtxt.bInitialized)
    {
        CAM_MSG(ERROR, "CameraPlatform already initialized");
        return FALSE;
    }

    std_memset(&gPlatformCtxt, 0x0, sizeof(gPlatformCtxt));

    if (CAMERA_SUCCESS != CameraOSServicesInitialize(NULL))
    {
        result = FALSE;
    }

    if (TRUE == result)
    {
        result = CameraPlatformReadHWVersion();
    }

    if (TRUE == result)
    {
        result = CameraPlatformReadPlatformId();
    }

    if (TRUE == result)
    {
        result = CameraPlatformLoadDevPaths(&gPlatformCtxt);
    }

    if (TRUE == result)
    {
        result = CameraPlatformLoadConfigLib();
    }

    if (TRUE == result)
    {
        result = CameraSensorI2C_Init();
    }

    if (TRUE == result)
    {
        result = CameraPMEMInit();
    }

    if (TRUE == result)
    {
        result = CameraSensorGPIO_Init();
    }

    if (TRUE == result)
    {
        gPlatformCtxt.bInitialized = TRUE;
    }
    else
    {
        CameraPlatformDeinit();
    }

    return result;
}

/* ===========================================================================

FUNCTION
    CameraPlatformDeinit

DESCRIPTION
    This function uninitializes CameraPlatform

DEPENDENCIES
    None

RETURN VALUE
    None

SIDE EFFECTS
    None

=========================================================================== */
void CameraPlatformDeinit(void)
{
    int i;
    boolean result = TRUE;

    result = CameraSensorI2C_DeInit();
    CAM_MSG_ON_ERR((FALSE == result), "CameraSensorI2C_DeInit failed with %d", result);

    result = CameraSensorGPIO_DeInit();
    CAM_MSG_ON_ERR((FALSE == result), "CameraSensorGPIO_DeInit failed with %d", result);

    result = CameraPMEMDeinit();
    CAM_MSG_ON_ERR((FALSE == result), "CameraPMEMDeinit failed with %d", result);

    CameraPlatformUnloadConfigLib();

    (void)CameraOSServicesUninitialize();

    gPlatformCtxt.bInitialized = FALSE;
}

/*===========================================================================

FUNCTION
   CameraPlatformGetVirtualAddress

DESCRIPTION
    This returns the virtual base address of requested hw block registers.

DEPENDENCIES
    None

RETURN VALUE
    The virtual base address of hw block Registers

SIDE EFFECTS
    None

===========================================================================*/
void* CameraPlatformGetVirtualAddress(CameraHwBlockType hwBlockType, uint8 id)
{
    void* virtualAddress = NULL;
    switch (hwBlockType)
    {
        default:
            CAM_MSG(ERROR, "%s: unsupported %d",
                        __FUNCTION__, hwBlockType);
            break;
    }

    return virtualAddress;
}

CameraResult CameraPlatformGetInputDeviceInterface(uint32 CameraDeviceId,
        CameraInputDeviceInterfaceType* pInterf)
{
    CameraResult rc = CAMERA_SUCCESS;
    if (!gPlatformCtxt.pBoardInfo || !pInterf)
    {
        return CAMERA_EFAILED;
    }
    else
    {
        int idx = 0;
        for (idx = 0; idx < CAMSENSOR_ID_MAX; idx++)
        {
            if (gPlatformCtxt.pBoardInfo->camera[idx].devId == CameraDeviceId)
            {
                pInterf->csid = gPlatformCtxt.pBoardInfo->camera[idx].csiId;
                /*1 to 1 mapping for csi and csiphy*/
                pInterf->csiphy = pInterf->csid;
                break;
            }
        }
    }
    return rc;
}

CameraResult CameraPlatformGetInputDeviceIndex(uint32 CameraDeviceId, CameraSensorIndex* pIndex, uint32* pSubdevIndex)
{
    CameraResult rc = CAMERA_SUCCESS;
    if (!gPlatformCtxt.pBoardInfo || !pIndex || !pSubdevIndex)
    {
        return CAMERA_EFAILED;
    }
    else
    {
        int idx = 0;
        for (idx = 0; idx < CAMSENSOR_ID_MAX; idx++)
        {
            if (gPlatformCtxt.pBoardInfo->camera[idx].devId == CameraDeviceId)
            {
                *pIndex = idx;
                *pSubdevIndex = gPlatformCtxt.pBoardInfo->camera[idx].subdevId;
                break;
            }
        }
    }
    return rc;
}

const CameraSensorBoardType* CameraPlatformGetSensorBoardType(CameraSensorIndex idx)
{
    const CameraSensorBoardType* sensorInfo = NULL;
    if (NULL != gPlatformCtxt.pBoardInfo && idx < CAMSENSOR_ID_MAX)
    {
        sensorInfo = &gPlatformCtxt.pBoardInfo->camera[idx];
    }
    return sensorInfo;
}

boolean CameraSensorGPIO_GetIntrPinInfo(const CameraSensorIndex idx,
        CameraSensorGPIO_SignalType pin, CameraSensorGPIO_IntrPinType *pPinInfo)
{
    boolean bReturnVal = FALSE;

    if (pPinInfo)
    {
        const CameraSensorBoardType *sensorInfo = CameraPlatformGetSensorBoardType(idx);
        if (sensorInfo)
        {
            int i = 0;
            for (i = 0; i < MAX_CAMERA_DEV_INTR_PINS; i++)
            {
                if (pin == sensorInfo->intr[i].gpio_id)
                {
                    *pPinInfo = sensorInfo->intr[i];
                    bReturnVal = TRUE;
                    break;
                }
            }


        }
    }

    return bReturnVal;
}

#ifdef AIS_BUILD_STATIC_CONFIG
extern ICameraConfig const * GetCameraConfigInterface(void);
#endif
static boolean CameraPlatformLoadConfigLib(void)
{
    boolean result = FALSE;
    ICameraConfig const * pCameraConfigIf = 0;
    GetCameraConfigInterfaceType pfnGetCameraConfigInterface = 0;

    /* Load camera platform board libraries, and acquire main
     * camera config interface to load board specific data:
     * - hCameraConfig, pBoardInfo, pChannelInfo, nChannels
     */
#ifdef AIS_BUILD_STATIC_CONFIG
    pfnGetCameraConfigInterface = GetCameraConfigInterface;
    gPlatformCtxt.hCameraConfig = (void*)-1;
#else
    if (0 == (gPlatformCtxt.hCameraConfig = dlopen(CAMERA_BOARD_LIBRARY, RTLD_NOW|RTLD_GLOBAL)))
    {
        CAM_MSG(ERROR, "%s: '%s' loading failed '%s'",
            __FUNCTION__, CAMERA_BOARD_LIBRARY, dlerror());
    }
    else if (0 == (pfnGetCameraConfigInterface = (GetCameraConfigInterfaceType)
                   dlsym(gPlatformCtxt.hCameraConfig, GET_CAMERA_CONFIG_INTERFACE)))
    {
        CAM_MSG(ERROR, "%s: '%s' interface not found '%s'",
            __FUNCTION__, CAMERA_BOARD_LIBRARY, dlerror());
    }
    else
#endif
    if (0 == (pCameraConfigIf = pfnGetCameraConfigInterface()))
    {
        CAM_MSG(ERROR, "%s: %s returns NULL",
            __FUNCTION__, GET_CAMERA_CONFIG_INTERFACE);
    }
    else if (0 == pCameraConfigIf->GetCameraConfigVersion)
    {
        CAM_MSG(ERROR, "%s: '%s' bad interface", __FUNCTION__, CAMERA_BOARD_LIBRARY);
    }
    else if (CAMERA_BOARD_LIBRARY_VERSION != pCameraConfigIf->GetCameraConfigVersion())
    {
        CAM_MSG(ERROR, "%s: '%s' runtime version mismatch",
            __FUNCTION__, CAMERA_BOARD_LIBRARY);
    }
    else if (0 == pCameraConfigIf->GetCameraBoardInfo)
    {
        CAM_MSG(ERROR, "%s: '%s' bad interface", __FUNCTION__, CAMERA_BOARD_LIBRARY);
    }
    else if (0 == (gPlatformCtxt.pBoardInfo = pCameraConfigIf->GetCameraBoardInfo()))
    {
        CAM_MSG(ERROR, "%s: '%s' GetCameraBoardInfo failed",
            __FUNCTION__, CAMERA_BOARD_LIBRARY);
    }
    else if (0 == pCameraConfigIf->GetCameraChannelInfo)
    {
        CAM_MSG(ERROR, "%s: '%s' bad interface", __FUNCTION__, CAMERA_BOARD_LIBRARY);
    }
    else if (0 != pCameraConfigIf->GetCameraChannelInfo(&gPlatformCtxt.pChannelInfo, &gPlatformCtxt.nChannels))
    {
        CAM_MSG(ERROR, "%s: '%s' GetCameraChannelInfo failed",
            __FUNCTION__, CAMERA_BOARD_LIBRARY);
    }
    else if (!gPlatformCtxt.pChannelInfo || (0 >= gPlatformCtxt.nChannels))
    {
        CAM_MSG(ERROR, "%s: '%s' GetCameraChannelInfo(0x%p, %d) bad data",
            __FUNCTION__, CAMERA_BOARD_LIBRARY, gPlatformCtxt.pChannelInfo, gPlatformCtxt.nChannels);
    }
    else
    {
        // up to here, all required interfaces have been loaded;
        // required data has been loaded.
        result = TRUE;

#ifndef AIS_BUILD_STATIC_CONFIG
        //
        // 2 optional interfaces
        //

        // 1. Get a handle to the camera config power interface
        // - pCameraConfigPowerIf
        GetCameraConfigPowerInterfaceType pfnGetCameraConfigPowerInterface = 0;
        if (0 != (pfnGetCameraConfigPowerInterface = (GetCameraConfigPowerInterfaceType)
                  dlsym(gPlatformCtxt.hCameraConfig, GET_CAMERA_CONFIG_POWER_INTERFACE)))
        {
            if (0 != (gPlatformCtxt.pCameraConfigPowerIf = pfnGetCameraConfigPowerInterface()))
            {
                CAM_MSG(HIGH, "%s: '%s' supports Power Interface",
                   __FUNCTION__, CAMERA_BOARD_LIBRARY);
            }
        }

        // 2. Get a handle to the camera config init/deinit interface
        // - pCameraConfigInitDeinitIf
        GetCameraConfigInitDeinitInterfaceType pfnGetCameraConfigInitDeinitInterface = 0;
        if (0 != (pfnGetCameraConfigInitDeinitInterface = (GetCameraConfigInitDeinitInterfaceType)
                  dlsym(gPlatformCtxt.hCameraConfig, GET_CAMERA_CONFIG_INIT_DEINIT_INTERFACE)))
        {
            if (0 != (gPlatformCtxt.pCameraConfigInitDeinitIf = pfnGetCameraConfigInitDeinitInterface()))
            {
                CAM_MSG(HIGH, "%s: '%s' supports InitDeinit Interface",
                    __FUNCTION__, CAMERA_BOARD_LIBRARY);
            }
        }
#endif
        CAM_MSG(HIGH, "%s: '%s' loaded successfully",
            __FUNCTION__, CAMERA_BOARD_LIBRARY);

    }

    return result;
}

static void CameraPlatformUnloadConfigLib(void)
{
    gPlatformCtxt.pBoardInfo   = 0;
    gPlatformCtxt.pChannelInfo = 0;
    gPlatformCtxt.nChannels    = 0;
    gPlatformCtxt.pCameraConfigPowerIf = 0;
    if (0 != gPlatformCtxt.hCameraConfig)
    {
        // camera_config is assumed to be loaded
        if (0 != gPlatformCtxt.pCameraConfigInitDeinitIf)
        {
            gPlatformCtxt.pCameraConfigInitDeinitIf->CameraConfigDeInit();
        }
#ifndef AIS_BUILD_STATIC_CONFIG
        (void)dlclose(gPlatformCtxt.hCameraConfig);
#endif
        gPlatformCtxt.hCameraConfig = 0;
    }
    gPlatformCtxt.pCameraConfigInitDeinitIf = 0;
}

/**
 * This method retrieve a copy of the channel config info from
 * camera config.  Caller shall allocate necessary memory in
 * pChannelData and indicates its size as number of elements in
 * pNumChannels.  Upon successful return, pNumChannels will be
 * updated to reflect the actual number of channel data copied
 * into pChannelData.
 *
 * @return 0 if successful
 *         non-0 otherwise
 */
int CameraPlatformGetChannelData(CameraChannelDataType * const pChannelData,
                                 int* pNumChannels)
{
    int result = -1;
    if (!pChannelData || !pNumChannels)
    {
        CAM_MSG(ERROR, "%s: null parameter(s)", __FUNCTION__);
        result = -2;
    }
    else if (!gPlatformCtxt.hCameraConfig || !gPlatformCtxt.pChannelInfo || 0 >= gPlatformCtxt.nChannels)
    {
        CAM_MSG(ERROR, "%s: bad state", __FUNCTION__);
        result = -3;
    }
    else if (gPlatformCtxt.nChannels > *pNumChannels)
    {
        CAM_MSG(ERROR, "%s: need more data", __FUNCTION__);
        result = -4;
    }
    else
    {
        int i;
        for ( i=0; i<gPlatformCtxt.nChannels; i++)
        {
            pChannelData[i].aisInputId = gPlatformCtxt.pChannelInfo[i].aisInputId;
            pChannelData[i].devId = gPlatformCtxt.pChannelInfo[i].devId;
            pChannelData[i].srcId = gPlatformCtxt.pChannelInfo[i].srcId;
        }
        *pNumChannels = gPlatformCtxt.nChannels;
        result        = 0;
    }
    return result;
}


/**
 * This function is used to read Chip ID.
 * @return void
 */
CameraPlatformChipIdType CameraPlatform_GetChipId(void)
{
    return gPlatformCtxt.chipId;
}

/**
 * This function is used to read Chip ID Version.
 * @return void
 */
uint32 CameraPlatform_GetChipVersion(void)
{
    return 0;
}

/**
 * This function is used to read Platform ID.
 * @return void
 */
CameraPlatformIdType CameraPlatform_GetPlatformId(void)
{
    return gPlatformCtxt.platformId;
}

/**
 * This function initializes SMMU objects
 *
 * @param[in] None
 *
 * @return 0 if successful
 */
CameraResult CameraSMMUInit(void)
{
    return CAMERA_SUCCESS;
}

/**
* This function maps the output buffer
* into the camera SMMU context.
*
* @param[in] pMemHandle The buffer to be mapped
* @param[in] numBytes   The size of the buffer to be allocated in bytes
* @param[in] devAddr    Dev address if fixed mapping flag is set
* @param[in] flags      Bit mask of flags for allocation (cont, cached, secure, read/write, etc...)
* @param[in] pHwBlocks  List of hw blocks to map to
* @param[in] nHwBlocks  Number of hw blocks in pHwBlocks

* @return CameraResult
*/
CameraResult CameraSMMUMapCameraBuffer(void* pMemHandle,
       uint32 numBytes,
       uint64_t devAddr,
       uint32 flags,
       const CameraHwBlockType* pHwBlocks,
       uint32 nHwBlocks)
{
    CAM_UNUSED(pMemHandle);
    CAM_UNUSED(numBytes);
    CAM_UNUSED(devAddr);
    CAM_UNUSED(flags);
    CAM_UNUSED(pHwBlocks);
    CAM_UNUSED(nHwBlocks);

    return CAMERA_SUCCESS;
}

/**
* This function unmaps the output buffer
* into the camera SMMU context.
*
* @param[in] out_buf The buffer to be unmapped
*/
void CameraSMMUUnmapCameraBuffer(void* pMemHandle)
{
    CAM_UNUSED(pMemHandle);

    return;
}

/**
* This function looks up the device address that is mapped to a specific HW block
*
* @param[in] pMemHandle The memory handle that was mapped
* @param[in] hwBlock The HW block for which the DA is needed
*
* @return The device address corresponding to the virtual address
*         passed in.
*/
uint64_t CameraSMMUGetDeviceAddress(void* pMemHandle, CameraHwBlockType hwBlock)
{
    CAM_UNUSED(hwBlock);

    return (uintptr_t)pMemHandle;
}

/**
 * This function closes the SMMU handles
 *
 * @param[in] None
 *
 * @return None
 */
void CameraSMMUDeinit()
{
    return;
}

/**
 * This function is used to identify the I2C bus the sensors are configured on.
 *param[in] UINT32 value that chooses back or front camera
 *
 * @return CameraPlatformSensorOrientationType *
 */
CameraPlatformSensorOrientationType * CameraPlatform_GetOrientation(CameraSensorIndex idx)
{
    (void)idx;
    static CameraPlatformSensorOrientationType orientation;
    memset(&orientation, 0, sizeof(orientation));
    return &orientation;
}

 /**
 * This function is used to identify the I2C bus Master Index sensors are configured on.
 *param[in] UINT32 value that chooses back or front camera
 *
 * @return uint16 *
 */
CameraSensorI2CMasterType CameraPlatform_GetMasterIndex(CameraSensorIndex idx)
{
    (void) idx;
    return 0;
}


/**
 * This function is used to initialize GPIO Block.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorGPIO_Init(void)
{
    return TRUE;
}

/**
 * This function is used to deinitialize GPIO Block.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */
boolean CameraSensorGPIO_DeInit(void)
{
    return TRUE;
}

/**
 * This function is used to configure GPIO pins for camera communication
 *      control interface.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */

boolean CameraSensorGPIO_ConfigCameraCCI(void)
{
    return TRUE;
}

/**
 * This function is used to unconfigure GPIO pins for camera communication
 *      control interface.
 *
 * @return TRUE - if successful
 *         FALSE - if failed
 */

boolean CameraSensorGPIO_UnConfigCameraCCI(void)
{
    return TRUE;
}

/**
 * This function is used to enable camera clocks
 *
 * @return CAMERA_SUCCESS - if successful
 */
CameraResult CameraPlatformClockEnable(void)
{
    CameraResult result = CAMERA_SUCCESS;
#if 0
    struct clk_mgr_cfg_data cfg_cmd;
    int rc = 0;

    if (gPlatformCtxt.aisMgrFd)
    {
        memset(&cfg_cmd, 0, sizeof(struct clk_mgr_cfg_data));
        cfg_cmd.cfg_type = AIS_CLK_ENABLE;
        rc = ioctl(gPlatformCtxt.aisMgrFd, VIDIOC_MSM_AIS_CLK_CFG, &cfg_cmd);
        if (rc < 0) {
            result = CAMERA_EFAILED;
            CAM_MSG(ERROR,"%s: VIDIOC_MSM_AIS_CLK_CFG failed: errorno = %d", __func__, rc);
            return result;
        }

        rc = CameraPlatformSetClkStatus(&gPlatformCtxt, 1);
        if (rc < 0) {
            result = CAMERA_EFAILED;
            CAM_MSG(ERROR,"%s: VIDIOC_MSM_AIS_SET_CLK_STATUS 1 failed: %d", __func__, rc);
        }
    }
    else
    {
        CAM_MSG(ERROR, "AIS_MGR subdev not available");
        result = CAMERA_EFAILED;
    }
#endif
    return result;
}

/**
 * This function is used to disable camera clocks
 *
 * @return CAMERA_SUCCESS - if successful
 */
CameraResult CameraPlatformClockDisable(void)
{
    CameraResult result = CAMERA_SUCCESS;
#if 0
    struct clk_mgr_cfg_data cfg_cmd;
    int rc = 0;

    if (gPlatformCtxt.aisMgrFd)
    {
        rc = CameraPlatformSetClkStatus(&gPlatformCtxt, 0);
        if (rc < 0) {
            result = CAMERA_EFAILED;
            CAM_MSG(ERROR,"%s: VIDIOC_MSM_AIS_SET_CLK_STATUS 0 failed: %d", __func__, rc);
            return result;
        }

        memset(&cfg_cmd, 0, sizeof(struct clk_mgr_cfg_data));
        cfg_cmd.cfg_type = AIS_CLK_DISABLE;
        rc = ioctl(gPlatformCtxt.aisMgrFd, VIDIOC_MSM_AIS_CLK_CFG, &cfg_cmd);
        if (rc < 0) {
            result = CAMERA_EFAILED;
            CAM_MSG(ERROR,"%s: VIDIOC_MSM_AIS_CLK_CFG failed: errorno = %d", __func__, rc);
        }
    }
    else
    {
        CAM_MSG(ERROR, "AIS_MGR subdev not available");
        result = CAMERA_EFAILED;
    }
#endif
    return result;
}
