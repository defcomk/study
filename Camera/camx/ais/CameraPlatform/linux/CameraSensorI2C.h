#ifndef __CAMERASENSORI2C_H_
#define __CAMERASENSORI2C_H_

/**
 * @file CameraSensorI2C.h
 *
 * @brief Camera sensor I2C abstraction API
 *
 * Copyright (c) 2007-2012, 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES
=========================================================================== */

/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */

/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------------
** Type Declarations
** ------------------------------------------------------------------------ */

/**
 * This struct holds functions pointers that are mapped to either DALQUP or CAMQUP modes
 */
typedef struct
{
    boolean (* CameraSensorI2C_Init)       (void);
    boolean (* CameraSensorI2C_PowerUp)    (void);
    boolean (* CameraSensorI2C_PowerDown)  (void);
    boolean (* CameraSensorI2C_DeInit)     (void);
    boolean (* CameraSensorI2C_Write)      (void*);
    boolean (* CameraSensorI2C_Read)       (void*);
    boolean (* CameraSensorI2C_Busspeed)   (uint32);
} CameraSensorI2CServicesType;

/* ===========================================================================
                        EXTERNAL API DECLARATIONS
=========================================================================== */


#endif // __CAMERASENSORI2C_H_
