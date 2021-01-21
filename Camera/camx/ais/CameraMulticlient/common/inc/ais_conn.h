#ifndef _AIS_CONN_H_
#define _AIS_CONN_H_

/**
 * @file ais_conn.h
 *
 * @brief defines all functions, structures and definitions of connection
 *        connection is an abstract layer for socket/Connection/hab on Qnx/Linux/Android/Integrity.
 *
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include "qcarcam_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * this is used to change connection APIs' names which would conflict
 * when both local connection and hypervisor connection are used at the same time
 */
#ifdef USE_HYP
#define AIS_CONN_API(name) name##_hv
#else
#define AIS_CONN_API(name) name
#endif

/**
 * Used to convert msec to time_t factor
 */
#define MSEC_MULTIPLIER 4294967

/**
 * calculates absolute time with specified time interval
 */
#define AIS_CONN_CALC_ABSTIME(ts, clk, interval)                               \
    do                                                                         \
    {                                                                          \
        clock_gettime((clk), &(ts));                                           \
        (ts).tv_sec += (interval) / 1000;                                      \
        (ts).tv_nsec += ((interval) % 1000) * 1000000;                         \
        if ((ts).tv_nsec >= 1000000000)                                        \
        {                                                                      \
            (ts).tv_nsec -= 1000000000;                                        \
            (ts).tv_sec++;                                                     \
        }                                                                      \
    } while (0)

/**
 * timeout for connection APIs
 */
#define AIS_CONN_START_TIMEOUT    500   //timeout for start in millisecond
#define AIS_CONN_CLOSE_TIMEOUT    300    //timeout for close in millisecond
#define AIS_CONN_RECV_TIMEOUT     250    //timeout for recv in millisecond
#define AIS_CONN_SEND_TIMEOUT     250    //timeout for send in millisecond

/**
 * maximum number of mapped buffers which is supported in one connection.
 * these buffers are used to keep records for all exported/imported,
 * and can be released when this connection is destroyed
 */
#define AIS_CONN_BUFFER_MAP_MAX_NUM (QCARCAM_MAX_NUM_BUFFERS)

/**
 * flush and release buffer mapping
 */
#define AIS_CONN_FLUSH_MODE_CURR (1 << 0)
#define AIS_CONN_FLUSH_MODE_OPPO (1 << 1)

/**
 * connection state
 */
typedef enum
{
    AIS_CONN_STAT_NONE = 0, /**< connection is not opened, or has been closed*/
    AIS_CONN_STAT_INIT,     /**< connection is opened */
    AIS_CONN_STAT_READY,    /**< connection is connected, and is ready to use */
} e_ais_conn_stat;

/**
 * connection mode, which indicates how to spawn a new connection
 */
typedef enum
{
    AIS_CONN_MODE_LISTEN = 0, /**< the listening connection is kept, and spawns a new one */
    AIS_CONN_MODE_WORK,       /**< the listening connection is copied as the spawned connection */
} e_ais_conn_mode;


/**
 * buffer mapping, which keep records of all mapped buffers
 */
typedef struct
{
    /** buffer address, only one can be used at the same time */
    void *p_buf; /**< buffer pointer */
    int  fd;     /**< buffer's file descriptor */

    int  size;   /**< buffer size */
} s_ais_buffer_map;

/**
 * connection management structures
 */
typedef struct
{
    volatile e_ais_conn_stat    status; /**< connection status */

    uint32_t                    id;     /**< connection id */
    unsigned int                max_cnt;/**< maximum number of sub connections of the same id */
    pthread_mutex_t             mutex;  /**< mutex of all operations */

    int                         map_idx; /**< current buffer map */
    int                         map_num[2]; /**< number of mapped buffers */
    s_ais_buffer_map            map[2][AIS_CONN_BUFFER_MAP_MAX_NUM]; /**< buffer mapping */

    volatile int                abort;

    //hab
    int32_t                     handle; /**< handle of mmhab, which may not be used for local connection */

    //os specific connection
#if defined(__LINUX) || defined(__ANDROID__) || defined(__QNXNTO__)

    int                         fd;      /**< socket handle */

#elif defined(__INTEGRITY)

    unsigned int                flag;    /**< indicator of client/server */
    unsigned int                ref_cnt; /**< reference count of connection which is used */
    Connection                  server;  /**< Connection of server */
    Connection                  client;  /**< Connection of client */

#endif

} s_ais_conn;

/**
 * initializes a connection structure
 * @param p points to a connection structure
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_init)(s_ais_conn *p);

/**
 * opens a new connection
 * @param p points to a connection structure
 * @param id connection id which is used to identify by client and server
 * @param max_cnt maximum number of sub connections with the specific id
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_open)(s_ais_conn *p, unsigned int id, unsigned int max_cnt);

/**
 * closes a connection
 * @param p points to a connection structure
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_close)(s_ais_conn *p);

/**
 * listens a connection from clients and spawns a new connection once a client requests.
 * @param p points to a connection structure
 * @param p_new points to a spawned connection structure
 * @param mode indicates the mode of the listening connection
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_accept)(s_ais_conn *p, s_ais_conn *p_new, e_ais_conn_mode mode);

/**
 * connects to a listening connection
 * @param p points to a connection structure
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_connect)(s_ais_conn *p);

/**
 * sends data to the peer via connection
 * @param p points to connection structure
 * @param p_data points to a buffer to be sent
 * @param size data size in the specified buffer
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_send)(s_ais_conn *p, void *p_data, unsigned int size);

/**
 * receives data from the peer via connection with timeout
 * @param p points to connection structure
 * @param p_data points to a buffer to be sent
 * @param p_size points to the size of the buffer specified by p_data
 * @param timeout timed-out time in millisecond, not supported currently
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_recv)(s_ais_conn *p, void *p_data, unsigned int *p_size, unsigned int timeout);

/**
 * exports the buffer from one process/OS to another
 *
 * @param p points to a connection structure
 * @param p_data points to the buffer which needs to be exported
 * @param size the size of the exported buffer
 * @return 0: success, others: failure
 *
 */
int AIS_CONN_API(ais_conn_export)(s_ais_conn *p, void *p_data, unsigned int size);

/**
 * imports the buffer from another process/OS
 *
 * @param p points to a connection structure
 * @param pp_data points to the buffer which is used to hold the buffer pointer imported
 * @param size the size of the exported buffer
 * @param timeout timed-out time in millisecond, not supported currently
 * @return 0: success, others: failure
 *
 */
int AIS_CONN_API(ais_conn_import)(s_ais_conn *p, void **pp_data, unsigned int size, unsigned int timeout);

/**
 * frees buffer mapping for different oprations results
 * @param p points to a connection structure
 * @param op indicates operation result
 * @return 0: success, others: failure
 *
 */
int AIS_CONN_API(ais_conn_flush)(s_ais_conn *p, int op);


#ifdef __cplusplus
}
#endif


#endif //_AIS_CONN_H_
