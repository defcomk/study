#ifndef _AIS_COMM_H_
#define _AIS_COMM_H_

/**
 * @file ais_comm.h
 *
 * @brief defines all structures and definition between client and server.
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "CameraResult.h"

#include "ais.h"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif


/** ais version */
/** ADD SUPPORTED VERSION, CHANGE THE ARRAY OF sg_ais_supported_version FOR BACKWARD COMPATIBILITY */

#define AIS_VERSION_MAJOR QCARCAM_VERSION_MAJOR
#define AIS_VERSION_MINOR QCARCAM_VERSION_MINOR

#define AIS_VERSION_GET_MAJOR(ver) (((ver) >> 8) & 0xFF)
#define AIS_VERSION_GET_MINOR(ver) ((ver) & 0xFF)

#define AIS_VERSION QCARCAM_VERSION

/**
 * ais temporary file path with write/read permission
 */
#if defined(__QNXNTO__)
#define AIS_TEMP_PATH "/tmp"
#elif defined(__ANDROID__)
#define AIS_TEMP_PATH "/data/vendor/camera"
#elif defined(__AGL__)
#define AIS_TEMP_PATH "/tmp"
#elif defined(__LINUX) || defined(CAMERA_UNITTEST)
#define AIS_TEMP_PATH "/tmp/camera"
#endif


/** maximum number of clients supported */
#define AIS_CONTEXT_MAX_NUM 32

/** indicator of one context which has been allocated */
#define AIS_CONTEXT_IN_USE ((qcarcam_hndl_t)-1)

/** maximum number of event connections */
#define AIS_CONN_EVENT_MAX_NUM 1

/** maximum number of command connections */
#define AIS_CONN_CMD_MAX_NUM 2

/** index for command connections */
#define AIS_CONN_CMD_IDX_MAIN 0
#define AIS_CONN_CMD_IDX_WORK 1


/** maximum number of all connections */
#define AIS_CONN_MAX_NUM (AIS_CONN_CMD_MAX_NUM + AIS_CONN_EVENT_MAX_NUM)


#define AIS_CONN_EVENT_NUM(cnt) (AIS_CONN_EVENT_MAX_NUM)

/** count of connection for commands */
#define AIS_CONN_CMD_NUM(cnt) ((cnt) - AIS_CONN_EVENT_MAX_NUM)

/** event connection ID */
#define AIS_CONN_ID_EVENT(id) ((id) + 0)

/** command connection ID */
#define AIS_CONN_ID_CMD_MAIN(id) ((id) + 1)
#define AIS_CONN_ID_CMD_WORK(id) ((id) + 2)


/** delay between health checks */
#define HEALTH_CHECK_DELAY_MSEC 200
/** max number of attempts for signal to be set */
#define MAX_HEALTH_SIGNAL_ATTEMPTS 20


/** health message event ID */
#define EVENT_HEALTH_MSG 0xA150C0EE


/**
 * command id
 * IF A NEW COMMAND IS ADDED, PLEASE ADD A NEW COMMAND PARAMETER
 * AND PUT THIS PARAMETER IN COMMAND PARAMETER HOLDER
 */
typedef enum
{
    AIS_CMD_QUERY_INPUTS = 0, /**< query inputs */
    AIS_CMD_OPEN,             /**< open */
    AIS_CMD_CLOSE,            /**< close */
    AIS_CMD_G_PARAM,          /**< g_param */
    AIS_CMD_S_PARAM,          /**< s_param */
    AIS_CMD_S_BUFFERS,        /**< s_buffers */
    AIS_CMD_START,            /**< start */
    AIS_CMD_STOP,             /**< stop */
    AIS_CMD_PAUSE,            /**< pause */
    AIS_CMD_RESUME,           /**< resume */
    AIS_CMD_GET_FRAME,        /**< get_frame */
    AIS_CMD_RELEASE_FRAME,    /**< release_frame */

    AIS_CMD_GET_EVENT, // not supported currently

    //INTERNAL USE
    //sub transactions for s_buffers
    AIS_CMD_S_BUFFERS_START,
    AIS_CMD_S_BUFFERS_TRANS,
    AIS_CMD_S_BUFFERS_END,

    AIS_CMD_HEALTH
} e_ais_cmd;

#define AIS_CONN_FLAG_HEALTH_CONN (1 << 0)

/**
 * connection information to be exchanged between client/server
 */
typedef struct
{
    int id;                     /**< connection id */
    int cnt;                    /**< number of connections */
    unsigned int gid;           /**< group id of client*/
    unsigned int pid;           /**< process id of client */
    unsigned int app_version;   /**< QCarCam API version of application */
    unsigned int version;       /**< QCarCam API version of client lib */
    CameraResult result;        /**< result of exchange */
    unsigned int flags;         /**< flags for new connection */
} s_ais_conn_info;


/**
 * event structures
 */
typedef struct
{
    qcarcam_event_t event_id;         /**< event id */
    qcarcam_event_payload_t payload;  /**< event payload */

    unsigned int event_cnt;           /**< event counter, debug purpuse */
} s_ais_event;


/**
 * command parameter header
 * IT MUST BE IN THE FIRST PLACE FOR ALL COMMANDS' PARAMETERS
 */
typedef struct
{
    int          cmd_id;   /**< command id */
    CameraResult result;   /**< result of current command */

} s_ais_cmd_header;

/**
 * command parameter for query_inputs
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    int is_p_inputs_set;
    unsigned int size;
    unsigned int flags;

    qcarcam_input_t inputs[MAX_NUM_AIS_INPUTS];
    unsigned int ret_size;
} s_ais_cmd_query_inputs;

/**
 * command parameter for open
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_input_desc_t input_desc;

    qcarcam_hndl_t handle;
} s_ais_cmd_open;

/**
 * command parameter for close
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
} s_ais_cmd_close;

/**
 * command parameter for g_param
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
    qcarcam_param_t param;

    qcarcam_param_value_t value;
} s_ais_cmd_g_param;

/**
 * command parameter for s_param
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
    qcarcam_param_t param;
    qcarcam_param_value_t value;
} s_ais_cmd_s_param;

/**
 * command parameter for s_buffers
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
    qcarcam_buffers_t buffers;
    qcarcam_buffer_t buffer[QCARCAM_MAX_NUM_BUFFERS];
} s_ais_cmd_s_buffers;

/**
 * command parameter for start
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
} s_ais_cmd_start;

/**
 * command parameter for stop
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
} s_ais_cmd_stop;

/**
 * command parameter for pause
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
} s_ais_cmd_pause;

/**
 * command parameter for resume
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
} s_ais_cmd_resume;

/**
 * command parameter for get_frame
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
    unsigned long long int timeout;
    unsigned int flags;

    qcarcam_frame_info_t frame_info;
} s_ais_cmd_get_frame;

/**
 * command parameter for release_frame
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;
    unsigned int idx;
} s_ais_cmd_release_frame;

/**
 * command parameter for get_event
 */
typedef struct
{
    int          cmd_id;
    CameraResult result;

    qcarcam_hndl_t handle;

    s_ais_event event;
} s_ais_cmd_get_event;


/**
 * command parameter holder for all commands
 */
typedef union
{

    s_ais_cmd_header           header;

    s_ais_cmd_query_inputs     query_inputs;

    s_ais_cmd_open             open;
    s_ais_cmd_close            close;

    s_ais_cmd_g_param          g_param;
    s_ais_cmd_s_param          s_param;
    s_ais_cmd_s_buffers        s_buffers;

    s_ais_cmd_start            start;
    s_ais_cmd_stop             stop;
    s_ais_cmd_pause            pause;
    s_ais_cmd_resume           resume;

    s_ais_cmd_get_frame        get_frame;
    s_ais_cmd_release_frame    release_frame;

    s_ais_cmd_get_event        get_event;

} u_ais_cmd;

#ifdef __cplusplus
}
#endif


#endif //_AIS_COMM_H_
