/**
 * @file CameraSensorI2C_CCI.c
 *
 * @brief Implementation of the camera sensor I2C abstraction API for CCI
 *
 * Copyright (c) 2011-2012, 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include <string.h>
#include "AEEstd.h"
#include "CameraSensorI2C.h"
#include "CameraPlatform.h"
#include "CameraOSServices.h"

static boolean CameraSensorCCII2C_Init(void);
static boolean CameraSensorCCII2C_Deinit( void );
static boolean CameraSensorCCII2C_PowerUp(void);
static boolean CameraSensorCCII2C_PowerDown(void);
static boolean CameraSensorCCII2C_Write(void*);
static boolean CameraSensorCCII2C_Read(void*);
static boolean CameraSensorCCII2C_SetClockFrequency (uint32 nFreq);

/*===========================================================================

FUNCTION     CameraSensor_ConstructCCII2C

DESCRIPTION  This function must be called first once before dali2c read or write functions are called.

DEPENDENCIES None

RETURN VALUE TRUE  - success
             FALSE - failed

SIDE EFFECTS None

===========================================================================*/
void CameraSensor_ConstructCCII2C ( CameraSensorI2CServicesType *service )
{
    CAM_MSG(HIGH, "%s", __FUNCTION__);
    service->CameraSensorI2C_Init        = CameraSensorCCII2C_Init;
    service->CameraSensorI2C_PowerUp     = CameraSensorCCII2C_PowerUp;
    service->CameraSensorI2C_PowerDown   = CameraSensorCCII2C_PowerDown;
    service->CameraSensorI2C_DeInit      = CameraSensorCCII2C_Deinit;
    service->CameraSensorI2C_Write       = CameraSensorCCII2C_Write;
    service->CameraSensorI2C_Read        = CameraSensorCCII2C_Read;
    service->CameraSensorI2C_Busspeed    = CameraSensorCCII2C_SetClockFrequency;
}

/*===========================================================================

FUNCTION     CameraSensorCCII2C_Init

DESCRIPTION  Initializes cci

DEPENDENCIES None

RETURN VALUE TRUE  - success
             FALSE - failed

SIDE EFFECTS None

===========================================================================*/
static boolean CameraSensorCCII2C_Init(void)
{
    //@todo

    return TRUE;
}
/*===========================================================================

FUNCTION     CameraSensorCCII2C_Deinit

DESCRIPTION  This function deinitializes cci

DEPENDENCIES None

RETURN VALUE TRUE  - success
             FALSE - failed

SIDE EFFECTS None

===========================================================================*/
static boolean CameraSensorCCII2C_Deinit(void)
{
    //@todo

    return TRUE;
}

/*===========================================================================

FUNCTION     CameraSensorCCII2C_PowerUp

DESCRIPTION  This function powers up cci

DEPENDENCIES None

RETURN VALUE TRUE  - success
             FALSE - failed

SIDE EFFECTS None

===========================================================================*/
static boolean CameraSensorCCII2C_PowerUp(void)
{
    //@todo

    return TRUE;
}

/*===========================================================================

FUNCTION     CameraSensorCCII2C_PowerUp

DESCRIPTION  This function powers down cci

DEPENDENCIES None

RETURN VALUE TRUE  - success
             FALSE - failed

SIDE EFFECTS None

===========================================================================*/
static boolean CameraSensorCCII2C_PowerDown(void)
{
    //@todo

    return TRUE;
}


/*===========================================================================

FUNCTION     CameraSensorCCII2C_Write

DESCRIPTION  This function is used to write data using cci

DEPENDENCIES Dal configure function must be called before this function is first called

RETURN VALUE TRUE  - success
             FALSE - failed

SIDE EFFECTS None

===========================================================================*/
static boolean CameraSensorCCII2C_Write(void *p_cmd)
{
    //@todo

    return TRUE;
}

/*===========================================================================

FUNCTION     CameraSensorCCII2C_Read

DESCRIPTION  This function is used to read data using cci

DEPENDENCIES Dal initialized method must be called before this function is first called

RETURN VALUE TRUE  - success
             FALSE - failed

SIDE EFFECTS None

===========================================================================*/
static boolean CameraSensorCCII2C_Read(void *p_cmd)
{
    //@todo

    return TRUE;
}

/*===========================================================================

FUNCTION
    CameraSensorCCII2C_SetClockFrequency

DESCRIPTION
    Set the CCI Bus Clock Frequencies
DEPENDENCIES
    None

RETURN VALUE
    TRUE  - success
    FALSE - failed

SIDE EFFECTS
    None

===========================================================================*/
static boolean CameraSensorCCII2C_SetClockFrequency(uint32 nFreq)
{
    //@todo

    return TRUE;
}

