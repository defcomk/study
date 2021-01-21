/**
 * @file CameraSensorI2C.c
 *
 * @brief Implementation of the camera sensor I2C abstraction API.
 *
 * Copyright (c) 2011-2012, 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include "AEEstd.h"
#include "CameraPlatform.h"
#include "CameraOSServices.h"
#include "CameraSensorI2C.h"

void CameraSensor_ConstructCCII2C(CameraSensorI2CServicesType *service);

/* ===========================================================================
                DEFINITIONS AND DECLARATIONS FOR MODULE
=========================================================================== */

/* --------------------------------------------------------------------------
** Constant / Define Declarations
** ----------------------------------------------------------------------- */
#define MAX_I2C_MULTICOMMANDS 8

/* --------------------------------------------------------------------------
** Type Declarations
** ----------------------------------------------------------------------- */
typedef enum
{
   I2CFLAG_DEFAULT,
   I2CFLAG_CCI
} CameraSensor_I2C_MODE;

/* --------------------------------------------------------------------------
** Global Object Definitions
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Local Object Definitions
** ----------------------------------------------------------------------- */
static const CameraSensor_I2C_MODE cCurrent_I2C_Mode = I2CFLAG_CCI;
static CameraSensorI2CServicesType camera_I2C_Service;

/* --------------------------------------------------------------------------
** Forward Declarations
** ----------------------------------------------------------------------- */

/* ===========================================================================
**                          Macro Definitions
** ======================================================================== */
/* ===========================================================================
**                          Internal Helper Functions
** ======================================================================== */

/* ===========================================================================
**                          External API Definitions
** ======================================================================== */
/*===========================================================================

FUNCTION
    CameraSensorI2C_Init

DESCRIPTION
    This function is used to intialize I2C communication.

DEPENDENCIES
    None

RETURN VALUE
    TRUE  - success
    FALSE - failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorI2C_Init(void)
{
    boolean ret = FALSE;

    if (I2CFLAG_CCI == cCurrent_I2C_Mode)
    {
        CameraSensor_ConstructCCII2C(&camera_I2C_Service);
    }
    else
    {
        CAM_MSG(ERROR, "Only support CCI!!!");
        return ret;
    }

    ret = camera_I2C_Service.CameraSensorI2C_Init();

    return ret;
}

/*===========================================================================

FUNCTION
    CameraSensorI2C_DeInit

DESCRIPTION
    This function is used to de-intialize I2C communication.

DEPENDENCIES
    None

RETURN VALUE
    TRUE  - success
    FALSE - failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorI2C_DeInit(void)
{
    boolean bResult = FALSE;

    if (NULL == camera_I2C_Service.CameraSensorI2C_DeInit)
    {
        return bResult;
    }
    bResult = camera_I2C_Service.CameraSensorI2C_DeInit();

    std_memset((void *) &camera_I2C_Service, 0, sizeof(camera_I2C_Service));

    return bResult;
}

/*===========================================================================

FUNCTION
    CameraSensorI2C_PowerUp

DESCRIPTION
    This function is used to power up I2C communication.

DEPENDENCIES
    None

RETURN VALUE
    TRUE  - success
    FALSE - failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorI2C_PowerUp(void)
{
    boolean bResult = FALSE;

    if (NULL == camera_I2C_Service.CameraSensorI2C_PowerUp)
    {
        return bResult;
    }
    bResult = camera_I2C_Service.CameraSensorI2C_PowerUp();

    return bResult;
}

/*===========================================================================

FUNCTION
    CameraSensorI2C_PowerDown

DESCRIPTION
    This function is used to power down I2C communication.

DEPENDENCIES
    None

RETURN VALUE
    TRUE  - success
    FALSE - failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorI2C_PowerDown(void)
{
    boolean bResult = FALSE;

    if (NULL == camera_I2C_Service.CameraSensorI2C_PowerDown)
    {
        return bResult;
    }
    bResult = camera_I2C_Service.CameraSensorI2C_PowerDown();

    return bResult;
}

/*===========================================================================

FUNCTION
    CameraSensorI2C_Write

DESCRIPTION
    This function is used to write data over the I2C bus.

DEPENDENCIES
    None

RETURN VALUE
    TRUE - if successful
    FALSE - if failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorI2C_Write (void* cmd)
{
    boolean ret = FALSE;
    if ((NULL == cmd) || (NULL == camera_I2C_Service.CameraSensorI2C_Write))
    {
        return FALSE;
    }

    ret = camera_I2C_Service.CameraSensorI2C_Write(cmd);

    return ret;
}

/*===========================================================================

FUNCTION
    CameraSensorI2C_Read

DESCRIPTION
    This function is used to read data from the I2C bus.

DEPENDENCIES
    None

RETURN VALUE
    TRUE  - success
    FALSE - failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorI2C_Read (void* cmd)
{
    boolean ret = FALSE;
    if ((NULL == cmd) || (NULL == camera_I2C_Service.CameraSensorI2C_Read))
    {
        return FALSE;
    }

    ret = camera_I2C_Service.CameraSensorI2C_Read(cmd);

    return ret;
}

/*===========================================================================

FUNCTION
    CameraSensorI2C_SetClockFrequency

DESCRIPTION
    This function is used to set the I2C clock frequency of the bus.

DEPENDENCIES
    None

RETURN VALUE
    TRUE  - success
    FALSE - failed

SIDE EFFECTS
    None

===========================================================================*/
void CameraSensorI2C_SetClockFrequency (CameraSensorI2C_SpeedType I2CBusSpeed)
{
    //TODO
    CAM_MSG(ERROR, "%s UNSUPPORTED API", __FUNCTION__);
    return;
}

