/**
 * @file CameraSensorCommon.c
 *
 * @brief Implementation of the common camera sensor abstraction APIs.
 *
 * Copyright (c) 2013 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES
=========================================================================== */
#include "CameraPlatform.h"

/* ===========================================================================
                DEFINITIONS AND DECLARATIONS FOR MODULE
=========================================================================== */

/* --------------------------------------------------------------------------
** Constant / Define Declarations
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Type Declarations
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Global Object Definitions
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Local Object Definitions
** ----------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
** Forward Declarations
** ----------------------------------------------------------------------- */


/* ===========================================================================
**                          Macro Definitions
** ======================================================================== */


/* ===========================================================================
**                          External API Definitions
** ======================================================================== */


/*===========================================================================

FUNCTION
    CameraSensor_PowerOn

DESCRIPTION
    This function is used to turn on the power to the camera sensor.

DEPENDENCIES
    None

RETURN VALUE
    TRUE - if successful
    FALSE - if failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensor_PowerOn (void)
{

    return TRUE;
}

/*===========================================================================

FUNCTION
    CameraSensorPowerOn

DESCRIPTION
    This function is used to turn on the power to the camera sensor with
    the specified index.

DEPENDENCIES
    None

RETURN VALUE
    TRUE - if successful
    FALSE - if failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorPowerOn (CameraSensorIndex idx)
{
    (void)idx;
    return CameraSensor_PowerOn();
}

/*===========================================================================

FUNCTION
    CameraSensor_PowerOff

DESCRIPTION
    This function is used to turn off the power to the camera sensor.

DEPENDENCIES
    None

RETURN VALUE
    TRUE - if successful
    FALSE - if failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensor_PowerOff (void)
{
    return  TRUE;
}

/*===========================================================================

FUNCTION
    CameraSensorPowerOff

DESCRIPTION
    This function is used to turn off the power to the camera sensor with
    the specified index.

DEPENDENCIES
    None

RETURN VALUE
    TRUE - if successful
    FALSE - if failed

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensorPowerOff (CameraSensorIndex idx)
{
    (void)idx;
    return CameraSensor_PowerOff();
}

/*===========================================================================

FUNCTION
    CameraSensor_Webcam_PowerOn

DESCRIPTION
    This function is used to power up the front-facing camera.

DEPENDENCIES
    None

RETURN VALUE
    TRUE if success
    FALSE if fail

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensor_Webcam_PowerOn(void)
{
    return TRUE;
}

/*===========================================================================

FUNCTION
    CameraSensor_Webcam_PowerOff

DESCRIPTION
    This function is used to power down the front-facing camera.

DEPENDENCIES
    None

RETURN VALUE
    TRUE if success
    FALSE if fail

SIDE EFFECTS
    None

===========================================================================*/
boolean CameraSensor_Webcam_PowerOff(void)
{
    return TRUE;
}
