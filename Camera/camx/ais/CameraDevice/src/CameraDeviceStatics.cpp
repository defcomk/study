/**
 * @file CameraDeviceStatics.c
 *
 * @brief Contains code to register statically link camera device drivers with
 * the camera device manager.
 *
 * Copyright (c) 2009-2011, 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "CameraDeviceManager.h"
#include "CameraPlatform.h"
#include "ife_device.h"
#include "csiphy_device.h"

/* ===========================================================================
                 DEFINITIONS AND DECLARATIONS FOR MODULE
=========================================================================== */
/* ---------------------------------------------------------------------------
** Constants / Defines
** ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------------
** Type Declarations
** ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------------
** Global Object Definitions
** ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------------
** Local Object Definitions
** ------------------------------------------------------------------------ */
/// This list contains the important information of the statically linked
/// devices.
static CameraDeviceManagerRegisteredType sStaticDevicesSA8155[] =
{
    {{CAMERADEVICEID_IFE_0, CAMERA_DEVICE_CATEGORY_IFE}, ife_device_open},
    {{CAMERADEVICEID_IFE_1, CAMERA_DEVICE_CATEGORY_IFE}, ife_device_open},
    {{CAMERADEVICEID_IFE_2, CAMERA_DEVICE_CATEGORY_IFE}, ife_device_open},
    {{CAMERADEVICEID_IFE_3, CAMERA_DEVICE_CATEGORY_IFE}, ife_device_open},
    {{CAMERADEVICEID_CSIPHY_0, CAMERA_DEVICE_CATEGORY_CSIPHY}, csiphy_device_open},
    {{CAMERADEVICEID_CSIPHY_1, CAMERA_DEVICE_CATEGORY_CSIPHY}, csiphy_device_open},
    {{CAMERADEVICEID_CSIPHY_2, CAMERA_DEVICE_CATEGORY_CSIPHY}, csiphy_device_open},
    {{CAMERADEVICEID_CSIPHY_3, CAMERA_DEVICE_CATEGORY_CSIPHY}, csiphy_device_open},
};

static CameraDeviceManagerRegisteredType sStaticDevicesSA6155[] =
{
    {{CAMERADEVICEID_IFE_0, CAMERA_DEVICE_CATEGORY_IFE}, ife_device_open},
    {{CAMERADEVICEID_IFE_1, CAMERA_DEVICE_CATEGORY_IFE}, ife_device_open},
    {{CAMERADEVICEID_IFE_2, CAMERA_DEVICE_CATEGORY_IFE}, ife_device_open},
    {{CAMERADEVICEID_CSIPHY_0, CAMERA_DEVICE_CATEGORY_CSIPHY}, csiphy_device_open},
    {{CAMERADEVICEID_CSIPHY_1, CAMERA_DEVICE_CATEGORY_CSIPHY}, csiphy_device_open},
    {{CAMERADEVICEID_CSIPHY_2, CAMERA_DEVICE_CATEGORY_CSIPHY}, csiphy_device_open},
};

CameraDeviceManagerStaticTableType sStaticDeviceTable = {};

/* ---------------------------------------------------------------------------
** Forward Declarations
** ------------------------------------------------------------------------ */

/* ===========================================================================
                        FUNCTION DEFINITIONS
=========================================================================== */
CameraDeviceManagerStaticTableType* CameraDeviceManagerGetStaticsTable(void)
{
    CameraPlatformChipIdType chipId = CameraPlatform_GetChipId();

    switch(chipId)
    {
    case CHIP_ID_SA8155:
    case CHIP_ID_SA8155P:
    case CHIP_ID_SA8195P:
        sStaticDeviceTable.pDevices = sStaticDevicesSA8155;
        sStaticDeviceTable.nDevices = STD_ARRAY_SIZE(sStaticDevicesSA8155);
        break;
    case CHIP_ID_SA6155:
        sStaticDeviceTable.pDevices = sStaticDevicesSA6155;
        sStaticDeviceTable.nDevices = STD_ARRAY_SIZE(sStaticDevicesSA6155);
        break;
    default:
        CAM_MSG(ERROR, "Static Table Not Available for chipID=%d", chipId);
        sStaticDeviceTable.nDevices = 0;
        break;
    }

    return &sStaticDeviceTable;
}
