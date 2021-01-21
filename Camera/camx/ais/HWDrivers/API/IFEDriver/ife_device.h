#ifndef _IFE_DEVICE_H
#define _IFE_DEVICE_H
/**
 * @file ife_device.h
 *
 * @brief Public interface for the IFE device driver
 *
 * Copyright (c) 2010-2014, 2016-2017 Qualcomm Technologies, Inc.
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
CameraResult ife_device_open(CameraDeviceHandleType** ppNewDeviceHandle,
        CameraDeviceIDType deviceId);

#endif  /* _IFE_DEVICE_H */

