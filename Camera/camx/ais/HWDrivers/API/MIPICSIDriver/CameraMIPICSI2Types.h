#ifndef _CAMERAMIPICSI2TYPES_H
#define _CAMERAMIPICSI2TYPES_H
/**
 * @file CameraMIPICSI2Types.h
 *
 * @brief Header file for camera MIPI CSI2 types
 *
 * Copyright (c) 2009-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

/* ===========================================================================
                        INCLUDE FILES
=========================================================================== */
#include "CameraTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */

/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */
#define MIPICSI_NUM_VC_CHANNELS 4
#define MIPICSI_NUM_DT_PER_VC 4

/* ---------------------------------------------------------------------------
** Type Declarations
** ------------------------------------------------------------------------ */
/*MIPI supported formats **/
typedef enum _MIPICsiPixelFormat_t
{
    MIPICsiPixelYUV422        ,    //8 bits only
    MIPICsiPixelRaw8          ,
    MIPICsiPixelRaw10         ,
    MIPICsiPixelRaw12         ,
	MIPICsiPixelRaw14         ,
	MIPICsiPixelRaw16         ,
    MIPICsiPixelDPCM10_6      ,   // DPCM format 12-6-12
    MIPICsiPixelDPCM12_6      ,   // DPCM format 10-6-10
    MIPICsiPixelDPCM12        ,   // DPCM format 12-8-12
    MIPICsiPixelDPCM10        ,   // DPCM format 10-8-10
    MIPICsiPixelDPCMOther     ,   // bypass from the MIPI CSI interface to the VFE.
    MIPICsiPixelNUM           ,
    MIPICsiPixelMAX = 0x7FFFFFFF
}MIPICsiPixelFormat_t;

/*MIPI supported Data Type (DT) **/
typedef enum _MIPICsiDT_t
{
    MIPICsiDT_EmbeddedData = 0x12,
    MIPICsiDT_YUV422_8b    = 0x1E,
    MIPICsiDT_YUV422_10b   = 0x1F,
    MIPICsiDT_RGB888       = 0x24,
    MIPICsiDT_RAW6         = 0x28,
    MIPICsiDT_RAW8         = 0x2A,
    MIPICsiDT_RAW10        = 0x2B,
    MIPICsiDT_RAW12        = 0x2C,
    MIPICsiDT_RAW14        = 0x2D,
    MIPICsiDT_UserDefined  = 0xFF,
    MIPICsiDT_MAX          = 0x7FFFFFFF
}MIPICsiDT_t;

/*MIPI supported Decode Type **/
typedef enum _MIPICsiDecode_t
{
    MIPICsiDecode_RAW6            = 0x0,
    MIPICsiDecode_RAW8            = 0x1,
    MIPICsiDecode_RAW10           = 0x2,
    MIPICsiDecode_RAW12           = 0x3,
	MIPICsiDecode_RAW14           = 0x4,
	MIPICsiDecode_RAW16           = 0x5,
	MIPICsiDecode_RAW20           = 0x6,
    MIPICsiDecode_DPCM_10_6_10    = 0x7,
    MIPICsiDecode_DPCM_10_8_10    = 0x8,
    MIPICsiDecode_DPCM_12_6_12    = 0x9,
    MIPICsiDecode_DPCM_12_8_12    = 0xA,
    MIPICsiDecode_DPCM_14_8_14    = 0xB,
    MIPICsiDecode_DPCM_14_10_14   = 0xC,
    MIPICsiDecode_NUM             = 0xD,
    MIPICsiDecode_MAX      = 0x7FFFFFFF
}MIPICsiDecode_t;

typedef enum _MIPICsiCSIPhyPhase_t
{
    MIPICsiPhy_TwoPhase = 0,
    MIPICsiPhy_ThreePhase,
} MIPICsiCSIPhyPhase_t;

/*MIPI Phy Mode**/
typedef enum _MIPICsiPhyMode_t
{
    MIPICsiPhyModeDefault = 0,  // Default Mode
    MIPICsiPhyModeCombo   = 1,  // Uses Combo Mode
    MIPICsiPhyModeMax
} MIPICsiPhyMode_t;

typedef struct
{
    MIPICsiPhyMode_t eMode;
    MIPICsiCSIPhyPhase_t ePhase;
    uint32 nSettleCount;
    uint32 nLaneMask;
    uint8 nNumOfDataLanes;
} MIPICsiPhyConfig_t;

/**
 *  @brief MIPI CSI2 Callback Messages
 */
typedef enum
{
    MIPICSI_MSG_ID_WARNING,
    MipiCSI_MSG_ID_FATAL_ERROR
} MIPICsiPhyMessage_t;

typedef enum
{
    CSIPHY_CMD_ID_RESET,
    CSIPHY_CMD_ID_CONFIG,
    CSIPHY_CMD_ID_START,
    CSIPHY_CMD_ID_STOP,
    CSIPHY_CMD_ID_MAX,
    CSIPHY_CMD_ID_INVALID = 0x7FFFFFFF
} CSIPHY_CMD_ID;

#ifdef __cplusplus
}
#endif

#endif // _CAMERAMIPICSI2TYPES_H
