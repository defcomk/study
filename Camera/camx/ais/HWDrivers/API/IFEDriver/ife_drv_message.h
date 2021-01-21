/**
 * @file ife_drv_message.h
 *
 * Copyright (c) 2010-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifndef _IFE_DRV_MESSAGE_H
#define _IFE_DRV_MESSAGE_H

#include "CameraTypes.h"

//** These are messages related to the VFE portion of the IFE **/
/**  VFE driver messages to the upper layer SW. */
//IMPORTANT: Requirement to keep same message ID for future chips
typedef enum VFE_MESSAGE_ID
{
    /************  Regular Message Queue  ************/
    VFE_MSG_REGULAR_QUEUE_FIRST_ENTRY = 0,

    VFE_MSG_ID_UPDATE_ACK,           /**<   No payload. */

    VFE_MSG_REGULAR_QUEUE_FIRST_OUTPUT_ENTRY,

    VFE_MSG_ID_FRAME_DONE,
    VFE_MSG_ID_RAW_FRAME_DONE,

    VFE_MSG_REGULAR_QUEUE_LAST_OUTPUT_ENTRY,
    VFE_MSG_REGULAR_QUEUE_FIRST_STATS_ENTRY,

    VFE_MSG_REGULAR_QUEUE_LAST_STATS_ENTRY,
    VFE_MSG_REGULAR_QUEUE_FIRST_TIMER_ENTRY,

    VFE_MSG_ID_EPOCH1,                      /**<   No payload.    */
    VFE_MSG_ID_EPOCH2,                      /**<   No payload.    */

    VFE_MSG_ID_SOF,                         /**< for sof message >*/

    VFE_MSG_REGULAR_QUEUE_LAST_TIMER_ENTRY,
    VFE_MSG_REGULAR_QUEUE_LAST_ENTRY,

    /************ Error Message Queue ********/
    VFE_MSG_ERROR_QUEUE_FIRST_ENTRY,


    VFE_MSG_ID_CAMIF_ERROR,            /**<   Payload is camif_status */

    VFE_MSG_ID_REALIGN_BUF_OVERFLOW,   /**<   No payload. If any realignment buffer overflow. */
    VFE_MSG_ID_VIOLATION,              /**<   No payload.  */
    VFE_MSG_ID_AXI_ERROR,              /**<   No payload.  */
    VFE_MSG_ID_IFE_CSID_FATAL_ERROR,   /**<   CSID error.  */
    VFE_MSG_ERROR_SOF_FREEZE,          /**<   SOF freeze error.  */

    VFE_MSG_ERROR_QUEUE_LAST_ENTRY,

    VFE_MSG_ID_INVALID

}VFE_MESSAGE_ID;


typedef struct
{
    uint32         bufferIndex;
    SMMU_ADDR_T    daBuffer;         /**<  Buffer device addresses for individual stats module.   */
    UINT_ADDR_T    vaBuffer;         /**<  Buffer virtual addresses for individual stats module.   */
}vfe_stats_buf_info;


typedef struct
{
    IfeInterfaceType      intf;           /**<  vfe interface */
    uint32                frameCounter;   /**<  Current frame count for sof message.  The frame count will roll over once
                                                it reaches the boundary of 32bits counter.      */
    uint64                sofTime;        /**<  time of SOF: ns */
    uint64                nSystemTime;    /**<  System time */
    uint64                sofQTime;       /**<  SOF qtimer timestamp: ns */
}vfe_msg_sof;

typedef struct
{
    IfeOutputPathType     outputPath;     /**<  output path */
    uint32                frameCounter;
                                    /**<  Current frame count for output message.  The frame
                                          count will roll over once it reaches the boundary of 32bits counter.*/
    uint32                bufIndex;    /** buffer index that was used on enqueue */
    SMMU_ADDR_T           bufAddrToRender[3];
                                    /**<  This corresponds to the three buffer addresses. (up to 3)*/
    UINT_ADDR_T           bufVirtualAddrToRender[3];
                                    /**<  This corresponds to the three buffer virtual addresses. (up to 3)*/
    SMMU_ADDR_T           bufAddrToRenderForUBWCMeta[3];
                                    /**<  This corresponds to the three buffer addresses for UBWC meta data*/
    UINT_ADDR_T           bufVirtualAddrToRenderForUBWCMeta[3];
                                   /**<  This corresponds to the three buffer virtual addresses for UBWC meta data*/
    boolean               bIsUBWCFrame;
                                    /**<  This indicates if this frame is a UBWC frame*/
    uint64                startTime;
                                    /**< Start time of frame */
    uint64                endTime;
                                    /**< End time of frame */
    uint64                systemTime;  /**< system time of frame */
    boolean               isLastSnapshotFrame;
                                    /**< a flag to indicate if this frame is last photosequence frame. */
    uint32                numberOfFramesPerGroup;
                                    /**<  Number of frames in a group in HFR case. */
    uint32                batchID;
                                    /**<  batchId to be filled in HFR case as (vfeFrameId / numberOfFramesPerGroup) + 1, in Non-HFR case as (vfeFrameId + 1)*/
    boolean               isZslFrame;
                                    /**< a flag to indicate if this frame is zsl frame or not */
    boolean               isConfFrame;
                                    /**< a flag to indicate if this frame is conf frame or not */
    boolean               isCDSFrame;
                                    /**< the flag to indicate if this frame is CDS frame or not */
    uint32                ispSettingID;
                                    /**< Setting ID for filling QSIF metadata */
}vfe_msg_output;

typedef struct
{
    IfeOutputPathType       outputPath; /**<  output path */
    uint32                  frameCounter;
                                    /**<  Current frame count for output message.  The frame
                                            count will roll over once it reaches the boundary of
                                            32bits counter.*/
    uint32                  bufIndex;    /** buffer index that was used on enqueue */
    SMMU_ADDR_T             pBuf;        /**<  Address for output buffer. */
    uint64                  startTime;   /**< Start time of frame */
    uint64                  endTime;     /**< End time of frame */
    uint64                  systemTime;  /**< System time of frame */
    uint32                  ispSettingID; /**< The setting ID that upper layer wants vfe to return back.
                                               with which, services can retrieve all the dmi tables for the frame*/
}vfe_msg_output_raw;

/**
 * For camif error message
 */
typedef struct
{
    boolean  camifState;        /* Camif state:   0x0 = Idle. 0x1 = Capturing data.              */
    uint32   pixelCount;        /* Indicates the number of clocks counted per Hsync by the camif */
    uint32   lineCount;         /* Indicates the number of Hsync lines counted per frame by the camif */
}vfe_msg_camif_status;

typedef struct
{
    uint32 violationStatus;    /*vfe violation status, refer to swi 0x00000048 VIOLATION_STATUS*/
}vfe_msg_violation_status;

/**
 * For csid error message
 */
typedef struct
{
    uint64  timestamp; /* error timestamp */
}vfe_msg_csid_error_status;

typedef struct
{
    uint32                          vfeId;
    union
    {
        vfe_msg_sof                     msgStartOfFrame;
        vfe_msg_output                  msgOutput;
        vfe_msg_output_raw              msgRaw;
        vfe_msg_camif_status            msgCamifError;
        vfe_msg_violation_status        msgViolation;
        vfe_msg_csid_error_status       msgCsidError;
    } _u;
} vfe_message_payload;

typedef struct
{
   VFE_MESSAGE_ID      msgId;
   vfe_message_payload payload;
}vfe_message;


#endif   /* _VFE_DRV_MESSAGE_H */

