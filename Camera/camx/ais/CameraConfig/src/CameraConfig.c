/**
 * @file camera_config.c
 *
 * @brief Implementation of the camera sensor board specifics
 *
 * Copyright (c) 2011-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include <stdio.h>
#include <unistd.h>
#include "CameraOSServices.h"
#include "CameraConfig.h"
#include "CameraPlatform.h"

extern const ICameraConfig cameraConfigIf_SA8155;
extern const ICameraConfig cameraConfigIf_SA6155;
extern const ICameraConfig cameraConfigIf_SA8195;

/* ===========================================================================
                DEFINITIONS AND DECLARATIONS FOR MODULE
=========================================================================== */


// This method is used to retrieve the camera board interface pointer.
CAM_API ICameraConfig const * GetCameraConfigInterface(void)
{
    CameraPlatformChipIdType chipId = CameraPlatform_GetChipId();

    switch(chipId)
    {
    case CHIP_ID_SA8155:
    case CHIP_ID_SA8155P:
        return &cameraConfigIf_SA8155;
    case CHIP_ID_SA6155:
        return &cameraConfigIf_SA6155;
    case CHIP_ID_SA8195P:
        return &cameraConfigIf_SA8195;
    default:
        CAM_MSG(ERROR, "CameraConfig not implemented for chipId=%d", chipId);
        break;
    }

    return NULL;
}

