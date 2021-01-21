#ifndef _CAMERAPLATFORMIST_H
#define _CAMERAPLATFORMIST_H

/**
 * @file CameraPlatformIST.h
 *
 * @brief Generic wrapper to setup and teardown device IST
 *
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "CameraResult.h"
#include "CameraPlatform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    CameraHwBlockType hwBlock;
    uint32 deviceId;

    /* payload to callback function - usually device handle */
    void* pDevHandle;

    /* callback function */
    void (*istCB)(void* pDevHandle);

    /*Private structure used by CameraPlatform*/
    void* pPrivate;
} CameraISTCtlType;

/**
* This function inits the device interrupt handler.
* @paramin CameraISTCtlType with all fields set except for pPrivate.
*/
CameraResult CameraPlatformISTInit(CameraISTCtlType* pIfeISTCtl);

/**
* This function stops the device interrupt handler.
* @paramin CameraISTCtlType that was used for Init.
*/
CameraResult CameraPlatformISTStop(CameraISTCtlType* pIfeISTCtl);

#ifdef __cplusplus
}
#endif

#endif /* _CAMERAPLATFORMIST_H */
