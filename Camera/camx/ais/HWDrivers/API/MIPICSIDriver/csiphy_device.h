#ifndef _CSIPHY_DEVICE_H
#define _CSIPHY_DEVICE_H
/**
 * @file csiphy_device.h
 *
 * @brief Public interface for the MIPI CSI2 device driver
 *
 * Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES FOR MODULE
=========================================================================== */
#include "CameraDevice.h"


/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */
/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */

/* ===========================================================================
                        FUNCTION DECLARATIONS
=========================================================================== */
/**
 * This function creates ife device.
 * @return Handle to the newly created ife device.
 */
CameraResult csiphy_device_open(CameraDeviceHandleType** ppNewDeviceHandle,
        CameraDeviceIDType deviceId);

#endif  /* _CSIPHY_DEVICE_H */

