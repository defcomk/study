#ifndef __CAMERAPLATFORMLINUX_H_
#define __CAMERAPLATFORMLINUX_H_

/**
 * @file CameraPlatformLinux.h
 *
 * @brief Camera platform APIs for Linux platform
 *
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES
=========================================================================== */
#include "CameraPlatform.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */

/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------------
** Type Declarations
** ------------------------------------------------------------------------ */
typedef enum {
    AIS_SUBDEV_REQMGR_NODE,
    AIS_SUBDEV_IFE,
    AIS_SUBDEV_CSIPHY,
    AIS_SUBDEV_SENSOR,
    AIS_SUBDEV_REQMGR,
    AIS_SUBDEV_AISMGR,
    AIS_SUBDEV_SYNCMGR_NODE,
    AIS_SUBDEV_SYNCMGR,
    AIS_SUBDEV_MAX,
}ais_subdev_id_t;

typedef enum {
    AIS_HANDLE_TYPE_SESSION,
    AIS_HANDLE_TYPE_DEVICE,
    AIS_HANDLE_TYPE_MAX,
}ais_kmd_handle_type;

/* ===========================================================================
                        EXTERNAL API DECLARATIONS
=========================================================================== */
const char* CameraPlatformGetDevPath(ais_subdev_id_t id, uint32 subdev_idx);
int CameraPlatformGetFd(ais_subdev_id_t id, uint32 subdev_idx);
void CameraPlatformClearFd(ais_subdev_id_t id, uint32 subdev_idx);
uint32 CameraPlatformGetKMDHandle(ais_kmd_handle_type type, ais_subdev_id_t id, uint32 subdev_idx);
void CameraPlatformSetKMDHandle(ais_kmd_handle_type type, ais_subdev_id_t id, uint32 subdev_idx, uint32 handle);

int CameraPlatform_GetCsidCore(CameraSensorIndex idx);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // __CAMERAPLATFORMLINUX_H_
