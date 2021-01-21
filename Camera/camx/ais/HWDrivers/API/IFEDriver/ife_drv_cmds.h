/**
 * @file ife_drv_cmds.h
 *
 * Copyright (c) 2010-2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/**
*  This group of id can only run in idle state
*/
CMD(IFE_CMD_ID_DUMMY_FOR_FIRST_IDLE_STATE_CMD)   /**< dummy command id*/

/** Power up VFE and enable clocks. */
CMD(IFE_CMD_ID_POWER_ON)    /**< This command must be sent before any other commands. */

/** Power down VFE and disable clocks. */
CMD(IFE_CMD_ID_POWER_OFF)   /**< This command must only be followed by IFE_CMD_ID_POWER_ON. */

CMD(IFE_CMD_ID_RESET)

CMD(IFE_CMD_ID_DUMMY_FOR_FIRST_IDLE_STATE_CONFIG_CMD)   /**<dymmy command Id */

CMD(IFE_CMD_ID_START)       /**< For continuous mode only. */
CMD(IFE_CMD_ID_RDI_CONFIG)

/** io config */
CMD(IFE_CMD_ID_OPERATION_CONFIG)    /**< This command must be sent before the input/output commands.  (AXI input/output and Camif commands.)*/
CMD(IFE_CMD_ID_AXI_OUTPUT_CONFIG)
CMD(IFE_CMD_ID_AXI_INPUT_CONFIG)
CMD(IFE_CMD_ID_CAMIF_CONFIG)

/** module cfg */
CMD(IFE_CMD_ID_SCALER_ENC_CONFIG)


CMD(IFE_CMD_ID_DUMMY_FOR_LAST_IDLE_STATE_CMD)     /**< dummy command id*/

/**
*  This group of id can only run in active state
* */
CMD(IFE_CMD_ID_DUMMY_FOR_FIRST_ACTIVE_STATE_CMD)  /**< dummy command id*/

CMD(IFE_CMD_ID_DUMMY_FOR_FIRST_UPDATE_CMD_IN_ACTIVE_STATE)

CMD(IFE_CMD_ID_AXI_INPUT_TRIGGER)
CMD(IFE_CMD_ID_UPDATE)
CMD(IFE_CMD_ID_FRAMEDROP_UPDATE)
CMD(IFE_CMD_ID_RECOVER_AXI_ERROR)

CMD(IFE_CMD_ID_DUMMY_FOR_FIRST_COMPOSITE_UPDATE_CMD_IN_ACTIVE_STATE)

CMD(IFE_CMD_ID_DUMMY_FOR_LAST_UPDATE_CMD_IN_ACTIVE_STATE)  /**< dummy command id*/

/** ackownledge from upper layer */
CMD(IFE_CMD_ID_EPOCH1_ACK)
CMD(IFE_CMD_ID_EPOCH2_ACK)

/** dummy ID for range checking  */
CMD(IFE_CMD_ID_DUMMY_FOR_LAST_ACTIVE_STATE_CMD)   /**< dummy command id*/

/**
* This group of id can run in both idle and active state
*/

CMD(IFE_CMD_ID_DUMMY_FOR_FIRST_IDLE_AND_ACTIVE_STATE_CMD)  /**< dummy command id*/

CMD(IFE_CMD_ID_DUMMY_FOR_FIRST_ALLOWED_COMMANDS_DURING_STATE_TRANSITION)

/// This is the command ID for the hardware version retrieval structure.
/// @par Type:
///   Output only
/// @par Input Expected:
///   None
/// @par Output Expected:
///   Pointer to a ife_cmd_hw_version structure with the current VFE
///   hardware version.
CMD(IFE_CMD_ID_GET_HW_VERSION)

CMD(IFE_CMD_ID_STOP)


CMD(IFE_CMD_ID_PAUSE)
CMD(IFE_CMD_ID_RESUME)


/**
* Note to all en-queue commands:  if the driver queue can't
* handle the request:  ie. number of available space is less
* than that of the request) simply fail the command without
* en-queue.
*/

CMD(IFE_CMD_ID_DUMMY_BUFFER_QUEUE_CMD_START)

CMD(IFE_CMD_ID_OUTPUT_BUFFER_ENQUEUE)
CMD(IFE_CMD_ID_OUTPUT_BUFFER_DEQUEUE)
CMD(IFE_CMD_ID_OUTPUT_BUFFER_FLUSH)
CMD(IFE_CMD_ID_INPUT_BUFFER_ENQUEUE)
CMD(IFE_CMD_ID_INPUT_BUFFER_FLUSH)
CMD(IFE_CMD_ID_STATS_BUFFER_ENQUEUE)
CMD(IFE_CMD_ID_STATS_BUFFER_FLUSH)
CMD(IFE_CMD_ID_REG_CFG_CMD)
CMD(IFE_CMD_ID_REG_CFG_CMD_LIST)
CMD(IFE_CMD_ID_GET_BUFFERQ_STATES)

CMD(IFE_CMD_ID_DUMMY_BUFFER_QUEUE_CMD_END)

CMD(IFE_CMD_ID_DUMMY_FOR_LAST_ALLOWED_COMMANDS_DURING_STATE_TRANSITION)

/** stats */
CMD(IFE_CMD_ID_DUMMY_FIRST_STATS_START_STOP_CMD)

CMD(IFE_CMD_ID_DUMMY_LAST_STATS_START_STOP_CMD)
