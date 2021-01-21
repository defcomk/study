/**
 * @file ife_drv_api.h
 *
 * Copyright (c) 2010-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _IFE_DRV_API_H
#define _IFE_DRV_API_H

#include "CameraCommonTypes.h"
#include "ife_drv_message.h"

/**
*  @brief IFE driver command ID list and message ID list

*  A list of all commands from IFE services layer to IFE DAL
*  layer.
*
*  A list of all messages from IFE DAL layer to IFE services
*  layer.
*/

extern const char ifeCommandIDtoString[][80];

typedef enum IFE_CMD_ID
{
    // X-Macro technique: redefining macros so that we can interpret the command ID's as identifiers or strings as needed.
    // This way the order of the strings in ifeCommandIDtoString will always be correct.
    #define CMD(x) x,
    #define CMD_CLONE(x,y) x = y,   // Defines a new command that has the same value (ID) as another command
    #include "ife_drv_cmds.h"
    #undef CMD
    #undef CMD_CLONE
    IFE_CMD_ID_MAX      // The last IFE command ID
}IFE_CMD_ID;

#define VFE_MAX_CHAN_PER_OUTPUT 2

/**
 * Buffer Enqueue for output paths
 * */
typedef struct
{
    IfeOutputPathType  outputPath;
    uint32                bufIndex; /** buffer index that will be used as identifier */
    SMMU_ADDR_T           da[VFE_MAX_CHAN_PER_OUTPUT]; /** device address */
    SMMU_ADDR_T           daMax[VFE_MAX_CHAN_PER_OUTPUT];
    UINT_ADDR_T           va[VFE_MAX_CHAN_PER_OUTPUT]; /** virtual address */
}vfe_cmd_output_buffer;

/**********************************************************
 ** IFE Driver Information Structures
 *********************************************************/
typedef struct
{
    uint32 ifeHWVersion;
}ife_cmd_hw_version;

typedef struct
{
    IfeOutputPathType  outputPath;
    uint32 vc;
    uint32 dt;
    uint32 numLanes;
    CsiphyCoreType csiphyId;
    uint32 width;
    uint32 height;
}ife_rdi_config_t;

typedef struct
{
    IfeOutputPathType  outputPath;
    uint32             numOfOutputBufs;
}ife_start_cmd_t;

typedef struct
{
    IfeOutputPathType  outputPath;
}ife_stop_cmd_t;

#endif   /* _IFE_DRV_API_H */

