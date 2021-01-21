/*!
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

#include "qcarcam.h"
#include "ais_comm.h"
#include "ais_conn.h"
#include "ais_log.h"

#include "ais_event_queue.h"

#include "CameraResult.h"
#include "CameraOSServices.h"

#if (defined (__GNUC__) && !defined(__INTEGRITY))
#define PUBLIC_API __attribute__ ((visibility ("default")))
#else
#define PUBLIC_API
#endif

#define AIS_EVENT_THREAD_MAX_NUM 2
#define AIS_EVENT_THREAD_IDX_RECV 0
#define AIS_EVENT_THREAD_IDX_CB 1

#if defined (__INTEGRITY)
#if defined(USE_HYP)
#define AIS_CLI_THRD_PRIO CAMERA_THREAD_PRIO_BACKEND
#else
#define AIS_CLI_THRD_PRIO CAMERA_THREAD_PRIO_DEFAULT
#endif
#else
#define AIS_CLI_THRD_PRIO CAMERA_THREAD_PRIO_NORMAL
#endif

/**
 * client management structure
 */
typedef struct
{
    volatile qcarcam_hndl_t qcarcam_hndl;
    s_ais_conn_info info;

    /** command connections */
    s_ais_conn cmd_conn[AIS_CONN_CMD_MAX_NUM];

    pthread_mutex_t cmd_mutex[AIS_CONN_CMD_MAX_NUM];

    /** event connection */
    s_ais_conn event_conn;

    /** event queue between cb and receiver */
    s_ais_event_queue event_queue;

    /** event thread */
    void* event_thread_id[AIS_EVENT_THREAD_MAX_NUM];
    volatile bool event_thread_abort[AIS_EVENT_THREAD_MAX_NUM];

    /** local content */
    qcarcam_event_cb_t event_cb;        // callback associated with the handle
    void* priv_data;                    // QCARCAM_PARAM_PRIVATE_DATA

    /** ctxt health */
    unsigned int signal_attempts;       // count failed wait for signal attempts
    volatile bool signal_bit;           // used to check if context has been signaled
    volatile bool health_active;        // enables health check for current context
    volatile bool ctxt_abort;           // set if context is in the process of being closed

} s_ais_client;


static bool sg_initialized = false;
static int sg_main_client_idx = -1;
static unsigned int sg_client_rand_id = 0;
static unsigned int sg_client_version = AIS_VERSION;

#ifndef HEALTH_DISABLED
static void* sg_health_thread_id = 0;
#endif

static pthread_mutex_t sgs_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sgs_mutex_cli = PTHREAD_MUTEX_INITIALIZER;
static s_ais_client sgs_ais_client[AIS_CONTEXT_MAX_NUM];


#define AIS_LOG_CLI(level, fmt...) AIS_LOG(AIS_MOD_ID_CLIENT, level, fmt)

#define AIS_LOG_CLI_ERR(fmt...) AIS_LOG(AIS_MOD_ID_CLIENT, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_CLI_WARN(fmt...) AIS_LOG(AIS_MOD_ID_CLIENT, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_CLI_HIGH(fmt...) AIS_LOG(AIS_MOD_ID_CLIENT, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CLI_MED(fmt...) AIS_LOG(AIS_MOD_ID_CLIENT, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_CLI_LOW(fmt...) AIS_LOG(AIS_MOD_ID_CLIENT, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_CLI_DBG(fmt...) AIS_LOG(AIS_MOD_ID_CLIENT, AIS_LOG_LVL_DBG, fmt)
#define AIS_LOG_CLI_DBG1(fmt...) AIS_LOG(AIS_MOD_ID_CLIENT, AIS_LOG_LVL_DBG1, fmt)

#define AIS_LOG_CLI_API_LEVEL AIS_LOG_LVL_LOW
#define AIS_LOG_CLI_API(fmt...) AIS_LOG(AIS_MOD_ID_CLIENT, AIS_LOG_CLI_API_LEVEL, fmt)

/**
 * Function declarations
 */
static int ais_client_cmd_2_idx(unsigned int cmd_id);
static int ais_client_init(s_ais_client *p);
static int ais_client_uninit(s_ais_client *p);
static int ais_client_alloc(void);
static int ais_client_free(int idx);
static int ais_client_find(qcarcam_hndl_t handle);
static int ais_client_create_main_conn(s_ais_client *p);
static int ais_client_destroy_main_conn(s_ais_client *p);
static int ais_client_create_work_conn(s_ais_client *p);
static int ais_client_destroy_work_conn(s_ais_client *p);
static int ais_client_event_recv_thread(void *p_arg);
static int ais_client_event_cb_thread (void *p_arg);
static int ais_client_create_event_thread(s_ais_client *p);
static int ais_client_destroy_event_thread(s_ais_client *p);
static int ais_client_release_event_thread(s_ais_client *p);
static int ais_client_create_event_conn(s_ais_client *p);
static int ais_client_destroy_event_conn(s_ais_client *p);
static int ais_client_exchange(s_ais_client *p);
static int ais_client_create(bool is_main_client);
static int ais_client_destroy(int idx, int flag);
static CameraResult ais_client_process_cmd(s_ais_client *p, void *p_param, unsigned int param_size, unsigned int size, unsigned int recv_timeout);
static CameraResult ais_health_signal(s_ais_client *p);

/**
 * converts command id to connection index
 *
 * @param cmd_id command id
 * @return connection index which would be used for processing this command
 *
 */
static int ais_client_cmd_2_idx(unsigned int cmd_id)
{
    int idx = 0;
    switch (cmd_id)
    {
    case AIS_CMD_GET_FRAME:
        idx = AIS_CONN_CMD_IDX_WORK;
        break;
    default:
        idx = AIS_CONN_CMD_IDX_MAIN;
        break;
    }

    return idx;
}

/**
 * initializes one client context
 *
 * @param p points to a client context
 * @return 0: success, others: failed
 *
 */
static int ais_client_init(s_ais_client *p)
{
    int i;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    for (i = 0; i < AIS_CONN_CMD_MAX_NUM; i++)
    {
        AIS_CONN_API(ais_conn_init)(p->cmd_conn + i);
        pthread_mutex_init(p->cmd_mutex + i, NULL);
    }

    AIS_CONN_API(ais_conn_init)(&p->event_conn);

    memset(&p->event_queue, 0, sizeof(p->event_queue));

    for (i = 0; i < AIS_EVENT_THREAD_MAX_NUM; i++)
    {
        p->event_thread_id[i] = 0;
        p->event_thread_abort[i] = FALSE;
    }

    p->event_cb = NULL;
    p->priv_data = NULL;

    p->signal_attempts = 0;
    p->signal_bit = FALSE;
    p->health_active = FALSE;
    p->ctxt_abort = FALSE;

    AIS_LOG_CLI_API("X %s 0x%p", __func__, p);

    return 0;
}

/**
 * uninitializes one client context
 *
 * @param p points to a client context
 * @return 0: success, others: failed
 *
 */
static int ais_client_uninit(s_ais_client *p)
{
    int i;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    for (i = 0; i < AIS_CONN_CMD_MAX_NUM; i++)
    {
        pthread_mutex_destroy(p->cmd_mutex + i);
    }

    AIS_LOG_CLI_API("X %s 0x%p", __func__, p);

    return 0;
}

/**
 * allocates one client context from the global resource
 *
 * @return >=0: index of valid context, <0: failed
 *
 */
static int ais_client_alloc(void)
{
    int rc = -1;
    int i;

    AIS_LOG_CLI_API("E %s", __func__);

    pthread_mutex_lock(&sgs_mutex);

    for (i = 0; i < AIS_CONTEXT_MAX_NUM; i++)
    {
        if (sgs_ais_client[i].qcarcam_hndl == NULL)
        {
            ais_client_init(&sgs_ais_client[i]);

            sgs_ais_client[i].qcarcam_hndl = AIS_CONTEXT_IN_USE;

            rc = i;
            break;
        }
    }

    pthread_mutex_unlock(&sgs_mutex);

    AIS_LOG_CLI(rc >= 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_WARN,
                "X %s %d", __func__, rc);

    return rc;
}

/**
 * frees one client context to the global resource
 * @parma idx the index of a client context
 * @return 0: success, others: failed
 */
static int ais_client_free(int idx)
{
    int rc = 0;

    AIS_LOG_CLI_API("E %s %d", __func__, idx);

    if (idx < 0 || idx >= AIS_CONTEXT_MAX_NUM)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    pthread_mutex_lock(&sgs_mutex);

    if (sgs_ais_client[idx].qcarcam_hndl != NULL)
    {
        rc = ais_client_uninit(&sgs_ais_client[idx]);

        sgs_ais_client[idx].qcarcam_hndl = NULL;
    }

    pthread_mutex_unlock(&sgs_mutex);

EXIT_FLAG:

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s %d %d", __func__, idx, rc);

    return rc;
}

/**
 * finds the index of one client context with specific handle
 * @parma handle handle opened
 * @return >=0: index of valid context, <0: failed
 */
static int ais_client_find(qcarcam_hndl_t handle)
{
    int rc = -1;
    int i;

    AIS_LOG_CLI_DBG("E %s 0x%p", __func__, handle);

    for (i = 0; i < AIS_CONTEXT_MAX_NUM; i++)
    {
        if (sgs_ais_client[i].qcarcam_hndl == handle)
        {
            rc = i;
            break;
        }
    }

    AIS_LOG_CLI(rc >= 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, handle, rc);

    return rc;
}

/**
 * creates the main connection with one client based on the exchaged connection id
 * @param p points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_create_main_conn(s_ais_client *p)
{
    int rc = 0;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    rc = AIS_CONN_API(ais_conn_open)(&p->cmd_conn[AIS_CONN_CMD_IDX_MAIN], AIS_CONN_ID_CMD_MAIN(p->info.id), 1);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    rc = AIS_CONN_API(ais_conn_connect)(&p->cmd_conn[AIS_CONN_CMD_IDX_MAIN]);

EXIT_FLAG:

    if (rc != 0)
    {
        AIS_CONN_API(ais_conn_close)(&p->cmd_conn[AIS_CONN_CMD_IDX_MAIN]);
    }

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * destroys the main connection.
 * @param p points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_destroy_main_conn(s_ais_client *p)
{
    int rc = 0;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    rc = AIS_CONN_API(ais_conn_close)(&p->cmd_conn[AIS_CONN_CMD_IDX_MAIN]);

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * creates all working connections with one client based on the exchanged connection id
 * @param p points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_create_work_conn(s_ais_client *p)
{
    int rc = 0;
    int i;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    for (i = AIS_CONN_CMD_IDX_WORK; i < AIS_CONN_CMD_NUM(p->info.cnt); i++)
    {
        rc = AIS_CONN_API(ais_conn_open)(p->cmd_conn + i, AIS_CONN_ID_CMD_WORK(p->info.id) + i - AIS_CONN_CMD_IDX_WORK, AIS_CONN_CMD_MAX_NUM);
        if (rc != 0)
        {
            rc = -1;
            goto EXIT_FLAG;
        }

        rc = AIS_CONN_API(ais_conn_connect)(p->cmd_conn + i);
        if (rc != 0)
        {
            rc = -2;
            goto EXIT_FLAG;
        }
    }

EXIT_FLAG:
    if (rc != 0)
    {
        for (i = AIS_CONN_CMD_IDX_WORK; i < AIS_CONN_CMD_NUM(p->info.cnt); i++)
        {
            AIS_CONN_API(ais_conn_close)(p->cmd_conn + i);
        }
    }

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * destroys all working connections
 * @param p points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_destroy_work_conn(s_ais_client *p)
{
    int rc = 0;
    int i;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    for (i = AIS_CONN_CMD_IDX_WORK; i < AIS_CONN_CMD_NUM(p->info.cnt); i++)
    {
        rc = AIS_CONN_API(ais_conn_close)(p->cmd_conn + i);
    }

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * receives enqueues events and notifies to handle them.
 * this is thread function which should be run in one thread
 * @param p_arg points to a client context.
 * @return 0
 */
static int ais_client_event_recv_thread(void *p_arg)
{
    int rc;
    s_ais_client *p = (s_ais_client *)p_arg;
    s_ais_event event;
    unsigned int size = sizeof(s_ais_event);

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p_arg);

    while (!p->event_thread_abort[AIS_EVENT_THREAD_IDX_RECV])
    {
        size = sizeof(s_ais_event);
        rc = AIS_CONN_API(ais_conn_recv)(&p->event_conn, &event, &size, AIS_CONN_RECV_TIMEOUT);
        if (rc != 0)
        {
            rc = -1;
            usleep(100);
            continue;
        }

        AIS_LOG_CLI_DBG("received event id: %d cnt: %d cb: %p",
                        event.event_id, event.event_cnt, p->event_cb);

        rc = ais_event_queue_enqueue(&p->event_queue, &event);
    }

    AIS_LOG_CLI_API("X %s 0x%p", __func__, p_arg);

    return 0;
}

/**
 * dequeues events and calls callback functions which is registered by app.
 * this is thread function which should be run in one thread
 * @param p_arg points to a client context.
 * @return 0
 */
static int ais_client_event_cb_thread(void *p_arg)
{
    s_ais_client *p = (s_ais_client *)p_arg;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p_arg);

    while (!p->event_thread_abort[AIS_EVENT_THREAD_IDX_CB])
    {
        s_ais_event event;

        int rc = ais_event_queue_dequeue(&p->event_queue, &event, 100);
        if (rc != 0)
        {
            continue;
        }

        AIS_LOG_CLI_DBG("callback event id: %d cnt: %d cb: %p",
                        event.event_id, event.event_cnt, p->event_cb);

        p->signal_bit = TRUE;
        if (event.event_id == (qcarcam_event_t)EVENT_HEALTH_MSG)
        {
            /* Send health message to server */
            ais_health_signal(p);
        }

        if (p->event_cb)
        {
            p->event_cb(p->qcarcam_hndl, event.event_id, &event.payload);
        }
    }

    AIS_LOG_CLI_API("X %s 0x%p", __func__, p_arg);

    return 0;
}

/**
 * creates all event threads with one client based on the exchaged connection id
 * @param p points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_create_event_thread(s_ais_client *p)
{
    int rc;
    char name[32];

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    snprintf(name, sizeof(name), "ais_c_evnt_recv_%d", AIS_CONN_ID_EVENT(p->info.id));

    if (0 != (rc = CameraCreateThread(AIS_CLI_THRD_PRIO,
                                      0,
                                      &ais_client_event_recv_thread,
                                      p,
                                      0,
                                      name,
                                      &p->event_thread_id[AIS_EVENT_THREAD_IDX_RECV])))
    {
        AIS_LOG_CLI_ERR("CameraCreateThread rc = %d", rc);
    }

    if (rc != 0)
    {
        goto EXIT_FLAG;
    }

    snprintf(name, sizeof(name), "ais_c_evnt_cb_%d", AIS_CONN_ID_EVENT(p->info.id));

    if (0 != (rc = CameraCreateThread(AIS_CLI_THRD_PRIO,
                                      0,
                                      &ais_client_event_cb_thread,
                                      p,
                                      0,
                                      name,
                                      &p->event_thread_id[AIS_EVENT_THREAD_IDX_CB])))
    {
        AIS_LOG_CLI_ERR("CameraCreateThread rc = %d", rc);
    }

EXIT_FLAG:

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * destroys all event threads
 * @param p points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_destroy_event_thread(s_ais_client *p)
{
    int i;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    //stop thread
    for (i = 0; i < AIS_EVENT_THREAD_MAX_NUM; i++)
    {
        if (p->event_thread_id[i] != 0)
        {
            p->event_thread_abort[i] = TRUE;
        }
    }

    AIS_LOG_CLI_API("X %s 0x%p", __func__, p);

    return 0;
}

/**
 * waits all event threads exit
 * @param p points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_release_event_thread(s_ais_client *p)
{
    int i;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    //wait until thread is done
    for (i = 0; i < AIS_EVENT_THREAD_MAX_NUM; i++)
    {
        if (p->event_thread_id[i] != 0)
        {
            /* make sure event dequeue can return */
            if (i == AIS_EVENT_THREAD_IDX_CB)
            {
                if (0 != ais_event_queue_signal(&p->event_queue))
                {
                    AIS_LOG_CLI_ERR("ais_event_queue_signal failed");
                }
            }

            if (0 != (CameraJoinThread(p->event_thread_id[i], NULL)))
            {
                AIS_LOG_CLI_ERR("CameraJoinThread failed");
            }
            p->event_thread_id[i] = 0;
        }
    }

    AIS_LOG_CLI_API("X %s 0x%p", __func__, p);

    return 0;
}

/**
 * creates the event connection and threads with one client based on the exchanged connection id
 * @param p points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_create_event_conn(s_ais_client *p)
{
    int rc = 0;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    rc = AIS_CONN_API(ais_conn_open)(&p->event_conn, AIS_CONN_ID_EVENT(p->info.id), 1);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    rc = AIS_CONN_API(ais_conn_connect)(&p->event_conn);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    rc = ais_event_queue_init(&p->event_queue, 2, 60, ais_event_queue_event_2_idx);
    if (rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    rc = ais_client_create_event_thread(p);
    rc = (rc != 0) ? -4 : 0;

    if (rc != 0)
    {
        ais_event_queue_deinit(&p->event_queue);
    }

EXIT_FLAG:

    if (rc != 0)
    {
        ais_client_destroy_event_thread(p);
        AIS_CONN_API(ais_conn_close)(&p->event_conn);
    }

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * destroys the event connection and threads
 * @param p points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_destroy_event_conn(s_ais_client *p)
{
    int rc = 0;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    rc = ais_client_destroy_event_thread(p);

    rc = AIS_CONN_API(ais_conn_close)(&p->event_conn);

    rc = ais_client_release_event_thread(p);

    ais_event_queue_deinit(&p->event_queue);

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
            "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

#ifndef HEALTH_DISABLED
/**
 * checks contexts are active and destroys them otherwise
 * @param void p_arg: no params
 * @return void
 */
static int ais_client_health_thread(void *p_arg)
{
    int i = 0;
    int sleep_usec = HEALTH_CHECK_DELAY_MSEC * 1000; // Convert msec to usec

    AIS_LOG_CLI_API("E %s", __func__);

    pthread_detach(pthread_self());

    s_ais_client *p = &sgs_ais_client[sg_main_client_idx];

    while (p->health_active)
    {
        usleep(sleep_usec);

        /* Check if context has been signaled */
        if (p->signal_bit == TRUE)
        {
            p->signal_bit = FALSE;
            p->signal_attempts = 0;
        }
        else
        {
            if (p->signal_attempts > MAX_HEALTH_SIGNAL_ATTEMPTS)
            {
                for (i = 0; i < AIS_CONTEXT_MAX_NUM; i++)
                {
                    if (!sgs_ais_client[i].ctxt_abort &&
                        sgs_ais_client[i].qcarcam_hndl != AIS_CONTEXT_IN_USE)
                    {
                        qcarcam_event_payload_t payload;

                        sgs_ais_client[i].event_thread_abort[AIS_EVENT_THREAD_IDX_RECV] = TRUE;
                        sgs_ais_client[i].event_thread_abort[AIS_EVENT_THREAD_IDX_CB] = TRUE;

                        /* There can be a case when ais_server doesn't respond for
                         * for more than MAX_HEALTH_SIGNAL_ATTEMPTS but client app
                         * haven't set the callback yet. We want to destroy client
                         * context log this condition but won't notify the client app.
                         */
                        if (sgs_ais_client[i].event_cb != NULL)
                        {
                            payload.uint_payload = QCARCAM_CONN_ERROR;
                            sgs_ais_client[i].event_cb(sgs_ais_client[i].qcarcam_hndl, QCARCAM_EVENT_ERROR, &payload);
                        }
                        else
                        {
                            AIS_LOG_CLI_WARN("sgs_ais_client[%d].event_cb is NULL. \
                                Destroying context without client notification", i);
                        }

                        AIS_LOG_CLI_WARN("Health signal timeout. Closing context %d", i);
                        ais_client_destroy(i, 1);
                    }
                }
            }
            sgs_ais_client[i].signal_attempts++;
        }
    }

    AIS_LOG_CLI(AIS_LOG_LVL_HIGH, "X %s", __func__);

    return 0;
}

static int ais_client_create_health_thread(void)
{
    int rc = 0;
    char name[32];

    AIS_LOG_CLI_API("E %s", __func__);

    snprintf(name, sizeof(name), "c_health");

    if (0 != (rc = CameraCreateThread(AIS_CLI_THRD_PRIO,
                                      0,
                                      &ais_client_health_thread,
                                      NULL,
                                      0,
                                      name,
                                      &sg_health_thread_id)))
    {
        AIS_LOG_CLI_ERR("CameraCreateThread rc = %d", rc);
    }

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s %d", __func__, rc);

    return rc;
}
#endif //HEALTH_DISABLED

/**
 * exchanges the infos of connections, and gets the assigned connection id and range
 * @param p_conn points to a client context
 * @return 0: success, others: failed
 */
static int ais_client_exchange(s_ais_client *p)
{
    int rc = 0;
    s_ais_conn conn;
    s_ais_conn_info info;
    unsigned int size;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, p);

    rc = AIS_CONN_API(ais_conn_open)(&conn, 0, 1);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    rc = AIS_CONN_API(ais_conn_connect)(&conn);
    if (rc != 0)
    {
       rc = -2;
       goto EXIT_FLAG;
    }

    info.id = 0;
    info.cnt = AIS_CONN_MAX_NUM;
    info.pid = getpid();
    info.gid = sg_client_rand_id;
    info.app_version = sg_client_version;
    info.version = AIS_VERSION;
    info.flags = (p->health_active) ? AIS_CONN_FLAG_HEALTH_CONN : 0;
    size = sizeof(s_ais_conn_info);

    rc = AIS_CONN_API(ais_conn_send)(&conn, &info, size);
    if (rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    rc = AIS_CONN_API(ais_conn_recv)(&conn, &info, &size, AIS_CONN_RECV_TIMEOUT);
    if (rc != 0 || size == 0)
    {
        rc = -4;
        goto EXIT_FLAG;
    }

    if (info.result != CAMERA_SUCCESS)
    {
        AIS_LOG_CLI_ERR("Connection exchange with server failed with result=%d", info.result);

        if (info.result == CAMERA_EVERSIONNOTSUPPORT)
        {
            AIS_LOG_CLI_ERR("Version mismatch error: ver_client=%x, ver_app=%x, ver_server=%x",
                AIS_VERSION, sg_client_version, info.version);
        }
        rc = -5;
        goto EXIT_FLAG;
    }
    else if (sg_client_version != info.version)
    {
        AIS_LOG_CLI_WARN("API version of Server(%x) is different but still compatible with that of APP(%x), please upgrade soon",
            info.version, sg_client_version);
    }

    if (info.id < 0 || info.cnt <= 0)
    {
        rc = -6;
        goto EXIT_FLAG;
    }

    p->info = info;

    AIS_LOG_CLI_MED("%s:%d %d %d", __func__, __LINE__, info.id, info.cnt);

EXIT_FLAG:

    AIS_CONN_API(ais_conn_close)(&conn);

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
            "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * creates a client context and setup connection with a server
 * it allocates a client context, gets connection id and range from a server
 * @return >=0: index of client context, <0: failed
 */
static int ais_client_create(bool is_main_client)
{
    int rc = 0;
    int idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_HIGH("E %s", __func__);

    idx = ais_client_alloc();
    if (idx < 0)
    {
        goto EXIT_FLAG;
    }

    p = &sgs_ais_client[idx];

#ifndef HEALTH_DISABLED
    p->health_active = (is_main_client) ? TRUE : FALSE;
#else
    p->health_active = FALSE;
#endif
    rc = ais_client_exchange(p);
    if (rc != 0)
    {
        goto EXIT_FLAG;
    }

    rc = ais_client_create_main_conn(p);
    if (rc != 0)
    {
        goto EXIT_FLAG;
    }

    rc = ais_client_create_event_conn(p);
    if (rc != 0)
    {
        goto EXIT_FLAG;
    }

    if (is_main_client)
    {
        p->signal_bit = FALSE;
        sg_main_client_idx = idx;

#ifndef HEALTH_DISABLED
        rc = ais_client_create_health_thread();
        if (rc != 0)
        {
            goto EXIT_FLAG;
        }
#endif
        rc = ais_health_signal(p);
    }

EXIT_FLAG:

    if (rc != 0 && idx >= 0)
    {
        ais_client_free(idx);
        idx = -1;
    }

    AIS_LOG_CLI(idx >= 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
            "X %s %d %d", __func__, idx, rc);

    return idx;
}

/**
 * destroys a client context and frees all resource
 * @param idx index of a client context
 * @param flag indicates if context is aborted from health monitor
 * @return 0: success, others: failed
 */
static int ais_client_destroy(int idx, int flag)
{
    int rc = 0;
    int is_aborted = 0;
    s_ais_client *p;

    AIS_LOG_CLI_HIGH("E %s %d", __func__, idx);

    if (idx < 0 || idx >= AIS_CONTEXT_MAX_NUM)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    p = &sgs_ais_client[idx];

    pthread_mutex_lock(&sgs_mutex);
    if (p->ctxt_abort == TRUE)
    {
        is_aborted = 1;
    }
    else
    {
        is_aborted = 0;
        p->ctxt_abort = TRUE;
    }
    pthread_mutex_unlock(&sgs_mutex);

    if (is_aborted == 0)
    {
        if (p->qcarcam_hndl != NULL && p->qcarcam_hndl != AIS_CONTEXT_IN_USE
            && flag == 0)
        {
            ais_close(p->qcarcam_hndl);
            p->qcarcam_hndl = AIS_CONTEXT_IN_USE;
        }

        rc = ais_client_destroy_event_conn(p);

        p->health_active = FALSE;
        p->signal_attempts = 0;
        p->signal_bit = FALSE;

        rc = ais_client_destroy_work_conn(p);
        rc = ais_client_destroy_main_conn(p);

        rc = ais_client_free(idx);
    }

EXIT_FLAG:
    AIS_LOG_CLI(rc == 0 ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
            "X %s %d %d", __func__, idx, rc);

    return rc;
}

/**
 * processes all commands
 * @param p points to a client context
 * @param p_param points to a command paramter
 * @param param_size the size of a command parameter
 * @param size the size of a command parameter's buffer
 * @param recv_timeout for receiving response back from server
  * @return 0: success, others: failed
 */
static CameraResult ais_client_process_cmd(s_ais_client *p, void *p_param, unsigned int param_size, unsigned int size, unsigned int recv_timeout)
{
    CameraResult rc = CAMERA_SUCCESS;
    int ret = 0;
    s_ais_cmd_header *p_header = (s_ais_cmd_header *)p_param;
    int cmd_id = p_header->cmd_id;
    int idx = ais_client_cmd_2_idx(cmd_id);

    AIS_LOG_CLI_LOW("E %s 0x%p 0x%p %d %d", __func__, p, p_param, param_size, size);

    ret = pthread_mutex_lock(p->cmd_mutex +idx);
    if (ret != 0)
    {
        rc = CAMERA_EINVALIDITEM;
        goto EXIT_FLAG;
    }

    if (p->qcarcam_hndl == AIS_CONTEXT_IN_USE)
    {
        if ((cmd_id != AIS_CMD_QUERY_INPUTS) &&
            (cmd_id != AIS_CMD_OPEN) &&
            (cmd_id != AIS_CMD_HEALTH))
        {
            AIS_LOG_CLI_WARN("%s:%d: client may have already exited before cmd %d\n",
                             __func__, __LINE__, cmd_id);
            rc = CAMERA_EBADSTATE;
            goto EXIT_FLAG;
        }
    }

    ret = AIS_CONN_API(ais_conn_send)(&p->cmd_conn[idx], p_param, param_size);
    if (ret != 0)
    {
        rc = CAMERA_EFAILED;
        goto EXIT_FLAG;
    }

    if (cmd_id == AIS_CMD_S_BUFFERS)
    {
        unsigned int i;
        unsigned int temp_size = sizeof(int);
        s_ais_cmd_s_buffers *p_buffers = (s_ais_cmd_s_buffers *)p_param;

        //sending socket would merge multiple messages into one
        //if receiving socket doesn't receive them immediately.
        //use this send/recv pair to synchronize on both sides
        ret = AIS_CONN_API(ais_conn_recv)(&p->cmd_conn[idx], &i, &temp_size, recv_timeout);
        if (ret != 0 || temp_size != sizeof(int) || i != p_buffers->buffers.n_buffers)
        {
            rc = CAMERA_EFAILED;
            goto EXIT_FLAG;
        }

        AIS_LOG_CLI_MED("%s:%d %d", __func__, __LINE__, p_buffers->buffers.n_buffers);
        for (i = 0; i < p_buffers->buffers.n_buffers; i++)
        {
            ret = AIS_CONN_API(ais_conn_export)(&p->cmd_conn[idx],
                                    p_buffers->buffers.buffers[i].planes[0].p_buf,
                                    p_buffers->buffers.buffers[i].planes[0].size);
            if (ret != 0)
            {
                rc = CAMERA_EFAILED;
                goto EXIT_FLAG;
            }
        }
    }

    ret = AIS_CONN_API(ais_conn_recv)(&p->cmd_conn[idx], p_param, &size, recv_timeout);
    if (ret != 0 || size == 0)
    {
        rc = CAMERA_EFAILED;
        goto EXIT_FLAG;
    }

    if (cmd_id != p_header->cmd_id)
    {
        AIS_LOG_CLI(AIS_LOG_LVL_CRIT, "%s:%d %d %d", __func__, __LINE__, cmd_id, p_header->cmd_id);
        rc = CAMERA_EBADTASK;
        goto EXIT_FLAG;
    }

    rc = p_header->result;

EXIT_FLAG:

    if (cmd_id == AIS_CMD_CLOSE)
    {
        p->qcarcam_hndl = AIS_CONTEXT_IN_USE;
    }
    pthread_mutex_unlock(p->cmd_mutex +idx);

    AIS_LOG_CLI(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
            "X %s 0x%p 0x%p %d %d %d", __func__, p, p_param, param_size, size, rc);

    return rc;
}

/**
 * Initialize ais
 *
 * @return CAMERA_SUCCESS
 */
PUBLIC_API CameraResult ais_initialize(qcarcam_init_t* p_init_params)
{
    int idx = 0;
    CameraResult rc = CAMERA_SUCCESS;

    pthread_mutex_lock(&sgs_mutex_cli);

    if (sg_initialized)
    {
        rc = CAMERA_EALREADYLOADED;
        goto EXIT_FLAG;
    }

    memset(sgs_ais_client, 0, sizeof(sgs_ais_client));

    time_t t;
    srand((unsigned) time(&t));

    sg_client_rand_id = rand();
    sg_client_version = AIS_VERSION;

    ais_log_init(NULL, NULL);

    AIS_LOG_CLI_HIGH("AIS CLIENT START %x", AIS_VERSION);

    AIS_LOG_CLI_HIGH("%s:%d %d %d %d %d %d %d %d %d %d %d %d %d", __func__, __LINE__,
        (int)sizeof(s_ais_cmd_query_inputs),
        (int)sizeof(s_ais_cmd_open),
        (int)sizeof(s_ais_cmd_close),
        (int)sizeof(s_ais_cmd_g_param),
        (int)sizeof(s_ais_cmd_s_param),
        (int)sizeof(s_ais_cmd_s_buffers),
        (int)sizeof(s_ais_cmd_start),
        (int)sizeof(s_ais_cmd_stop),
        (int)sizeof(s_ais_cmd_pause),
        (int)sizeof(s_ais_cmd_resume),
        (int)sizeof(s_ais_cmd_get_frame),
        (int)sizeof(s_ais_cmd_release_frame));

    if (p_init_params && p_init_params->version)
    {
        sg_client_version = p_init_params->version;
    }

    idx = ais_client_create(TRUE);
    if (idx < 0)
    {
        rc = CAMERA_ENOMORE;
        goto EXIT_FLAG;
    }

    sg_initialized = true;

EXIT_FLAG:

    if (rc != 0 && idx >= 0)
    {
        ais_client_destroy(idx, 0);
    }

    pthread_mutex_unlock(&sgs_mutex_cli);

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
        "X %s | %d", __func__, rc);

    return rc;
}

/**
 * Uninitialize AIS
 *
 * @return CAMERA_SUCCESS
 */
PUBLIC_API CameraResult ais_uninitialize(void)
{
    CameraResult rc = CAMERA_SUCCESS;

    pthread_mutex_lock(&sgs_mutex_cli);

    if (!sg_initialized)
    {
        rc = CAMERA_EBADSTATE;
        goto EXIT_FLAG;
    }

    AIS_LOG_CLI_API("E %s", __func__);

    rc = ais_client_destroy(sg_main_client_idx, 0);
    sg_main_client_idx = -1;

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
            "X %s | %d", __func__, rc);

    ais_log_uninit();

EXIT_FLAG:
    pthread_mutex_unlock(&sgs_mutex_cli);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_health_signal
///
/// @brief Sends a message used for checking if client is responsive
///
/// @param hndl   Handle of input sensing
///
/// @return NOT NULL if successful; NULL on failure
///////////////////////////////////////////////////////////////////////////////
static CameraResult ais_health_signal(s_ais_client *p)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_header cmd_param;

    AIS_LOG_CLI_LOW("E %s 0x%p", __func__, p);

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_HEALTH;
    cmd_param.result = CAMERA_SUCCESS;

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_query_inputs
///
/// @brief Queries available inputs. To get the number of available inputs to query, call with p_inputs set to NULL.
///
/// @param p_inputs   Pointer to array inputs. If NULL, then ret_size returns number of available inputs to query
/// @param size       Number of elements in array
/// @param ret_size   If p_inputs is set, number of elements in array that were filled
///                   If p_inputs is NULL, number of available inputs to query
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_query_inputs(qcarcam_input_t* p_inputs, unsigned int size, unsigned int* ret_size)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_query_inputs cmd_param;
    s_ais_client *p;

    AIS_LOG_CLI_API("E %s 0x%p %d 0x%p | %d", __func__, p_inputs, size,
                ret_size, (ret_size != NULL) ? *ret_size : 0);

    if (ret_size == NULL)
    {
        rc = CAMERA_EBADPARM;
        goto EXIT_FLAG;
    }

    p = &sgs_ais_client[sg_main_client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_QUERY_INPUTS;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.is_p_inputs_set = (p_inputs == NULL) ? 0 : 1;
    cmd_param.size = size;

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);
    if (rc != CAMERA_SUCCESS)
    {
        goto EXIT_FLAG;
    }

    if (p_inputs != NULL)
    {
        memcpy(p_inputs, cmd_param.inputs, cmd_param.ret_size * sizeof(*p_inputs));
    }
    *ret_size = cmd_param.ret_size;

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d 0x%p | %d %d", __func__, p_inputs, size,
                ret_size, (ret_size != NULL) ? *ret_size : 0, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_open
///
/// @brief Opens handle to input
///
/// @param desc   Unique identifier of input to be opened
///
/// @return NOT NULL if successful; NULL on failure
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API qcarcam_hndl_t ais_open(qcarcam_input_desc_t desc)
{
    CameraResult rc;
    int ret = 0;
    int client_idx = -1;
    int active_clients = 0;
    s_ais_cmd_open cmd_param;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s %d", __func__, desc);

    pthread_mutex_lock(&sgs_mutex);

    if (!sg_initialized)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    pthread_mutex_unlock(&sgs_mutex);

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_OPEN;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.input_desc = desc;

    client_idx = ais_client_create(FALSE);
    if (client_idx < 0)
    {
        rc = CAMERA_ENOMORE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    AIS_LOG_CLI_MED("%s:%d 0x%x %d %d", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.input_desc);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);
    if (rc != CAMERA_SUCCESS)
    {
        cmd_param.handle = NULL;
        goto EXIT_FLAG;
    }

    ret = ais_client_create_work_conn(p);
    if (ret != 0)
    {
        goto EXIT_FLAG;
    }

    p->qcarcam_hndl = cmd_param.handle;

EXIT_FLAG:

    if (ret != 0 && client_idx >= 0)
    {
        ais_client_destroy(client_idx, 0);
    }

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s %d 0x%p active_clients %d", __func__, desc, cmd_param.handle, active_clients);

    return cmd_param.handle;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_close
///
/// @brief Closes handle to input
///
/// @param hndl   Handle of input that was opened
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_close(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_stop cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, hndl);

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_CLOSE;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;

    AIS_LOG_CLI_MED("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_CLOSE_TIMEOUT);
    p->qcarcam_hndl = AIS_CONTEXT_IN_USE;

    ais_client_destroy(client_idx, 0);

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, hndl, rc);

    return rc;

}

///////////////////////////////////////////////////////////////////////////////
/// ais_g_param
///
/// @brief Get parameter value
///
/// @param hndl     Handle of input
/// @param param    Parameter to get
/// @param p_value  Pointer to structure of value that will be retrieved
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_g_param(qcarcam_hndl_t hndl, qcarcam_param_t param, qcarcam_param_value_t* p_value)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_g_param cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p %d 0x%p", __func__, hndl, param, p_value);

    if (p_value == NULL)
    {
        rc = CAMERA_EBADPARM;
        goto EXIT_FLAG;
    }

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_G_PARAM;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;
    cmd_param.param = param;

    AIS_LOG_CLI_MED("%s:%d 0x%x %d 0x%p %d", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle,
                                        cmd_param.param);

    if (param == QCARCAM_PARAM_EVENT_CB)
    {
        p_value->ptr_value = (void *)p->event_cb;
        goto EXIT_FLAG;
    }
    else if (param == QCARCAM_PARAM_PRIVATE_DATA)
    {
        p_value->ptr_value = p->priv_data;
        goto EXIT_FLAG;
    }

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);
    if (rc == CAMERA_SUCCESS)
    {
        *p_value = cmd_param.value;
    }

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d 0x%p %d", __func__, hndl, param, p_value, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_s_param
///
/// @brief Set parameter
///
/// @param hndl     Handle of input
/// @param param    Parameter to set
/// @param p_value  Pointer to structure of value that will be set
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_s_param(qcarcam_hndl_t hndl, qcarcam_param_t param, const qcarcam_param_value_t* p_value)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_s_param cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p %d 0x%p", __func__, hndl, param, p_value);

    if (p_value == NULL)
    {
        rc = CAMERA_EBADPARM;
        goto EXIT_FLAG;
    }

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_S_PARAM;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;
    cmd_param.param = param;
    cmd_param.value = *p_value;

    AIS_LOG_CLI_MED("%s:%d 0x%x %d 0x%p %d", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle,
                                        cmd_param.param);

    if (param == QCARCAM_PARAM_PRIVATE_DATA)
    {
        p->priv_data = (void*)p_value->ptr_value;
        goto EXIT_FLAG;
    }

    if (param == QCARCAM_PARAM_EVENT_CB)
    {
        p->event_cb = (qcarcam_event_cb_t)p_value->ptr_value;
    }

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d 0x%p %d", __func__, hndl, param, p_value, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_s_buffers
///
/// @brief Set buffers
///
/// @param hndl       Handle of input
/// @param p_buffers  Pointer to set buffers structure
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_s_buffers(qcarcam_hndl_t hndl, qcarcam_buffers_t* p_buffers)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_s_buffers cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p 0x%p", __func__, hndl, p_buffers);

    if (p_buffers == NULL || p_buffers->n_buffers > QCARCAM_MAX_NUM_BUFFERS)
    {
        rc = CAMERA_EBADPARM;
        goto EXIT_FLAG;
    }

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_S_BUFFERS;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;
    cmd_param.buffers = *p_buffers;
    cmd_param.buffers.buffers = cmd_param.buffer;

    memcpy(cmd_param.buffers.buffers, p_buffers->buffers, sizeof(qcarcam_buffer_t) * p_buffers->n_buffers);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, hndl, p_buffers, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_start
///
/// @brief Start input
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_start(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_start cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, hndl);

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_START;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;

    AIS_LOG_CLI_MED("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_START_TIMEOUT);

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, hndl, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_stop
///
/// @brief Stop input that was started
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_stop(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_stop cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, hndl);

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_STOP;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;

    AIS_LOG_CLI_MED("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, hndl, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_pause
///
/// @brief Pause input that was started. Does not relinquish resource
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_pause(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_pause cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, hndl);

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_PAUSE;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;

    AIS_LOG_CLI_MED("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, hndl, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_resume
///
/// @brief Resumes input that was paused
///
/// @param hndl       Handle of input
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_resume(qcarcam_hndl_t hndl)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_resume cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p", __func__, hndl);

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_RESUME;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;

    AIS_LOG_CLI_MED("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, hndl, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_get_frame
///
/// @brief Get available frame
///
/// @param hndl          Handle of input
/// @param p_frame_info  Pointer to frame information that will be filled
/// @param timeout       Max wait time in ms for frame to be available before timeout
/// @param flags         Flags
///
/// @return CAMERA_SUCCESS if successful; CAMERA_EEXPIRED if timeout
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_get_frame(qcarcam_hndl_t hndl, qcarcam_frame_info_t* p_frame_info, unsigned long long int timeout, unsigned int flags)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_get_frame cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p 0x%p %lld 0x%x", __func__, hndl, p_frame_info, timeout, flags);

    if (p_frame_info == NULL)
    {
        rc = CAMERA_EBADPARM;
        goto EXIT_FLAG;
    }

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_GET_FRAME;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;
    cmd_param.timeout = timeout;
    cmd_param.flags = flags;

    AIS_LOG_CLI_DBG1("%s:%d 0x%x %d 0x%p %lld %d", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle,
                                        cmd_param.timeout,
                                        cmd_param.flags);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);
    if (rc != CAMERA_SUCCESS)
    {
        goto EXIT_FLAG;
    }

    *p_frame_info = cmd_param.frame_info;

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %lld 0x%x %d", __func__, hndl, p_frame_info, timeout, flags, rc);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////
/// ais_release_frame
///
/// @brief Re-enqueue frame buffers
///
/// @param hndl       Handle of input
/// @param idx        Index into the qcarcam_buffers_t buffers table to reenqueue
///
/// @return CAMERA_SUCCESS if successful
///////////////////////////////////////////////////////////////////////////////
PUBLIC_API CameraResult ais_release_frame(qcarcam_hndl_t hndl, unsigned int idx)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_release_frame cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_API("E %s 0x%p %d", __func__, hndl, idx);

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_RELEASE_FRAME;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;
    cmd_param.idx = idx;

    AIS_LOG_CLI_DBG1("%s:%d 0x%x %d 0x%p %d", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle,
                                        cmd_param.idx);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_CLI_API_LEVEL : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d %d", __func__, hndl, idx, rc);

    return rc;
}


//TODO: not supported fully, have to change ais engine
PUBLIC_API CameraResult ais_get_event(qcarcam_hndl_t hndl, s_ais_event *p_event)
{
    CameraResult rc = CAMERA_SUCCESS;
    s_ais_cmd_get_event cmd_param;
    int client_idx = -1;
    s_ais_client *p = NULL;

    AIS_LOG_CLI_LOW("E %s 0x%p 0x%p", __func__, hndl, p_event);

    if (p_event == NULL)
    {
        rc = CAMERA_EBADPARM;
        goto EXIT_FLAG;
    }

    client_idx = ais_client_find(hndl);
    if (client_idx < 0)
    {
        rc = CAMERA_EBADHANDLE;
        goto EXIT_FLAG;
    }
    p = &sgs_ais_client[client_idx];

    memset(&cmd_param, 0, sizeof(cmd_param));
    cmd_param.cmd_id = AIS_CMD_RELEASE_FRAME;
    cmd_param.result = CAMERA_SUCCESS;
    cmd_param.handle = hndl;

    AIS_LOG_CLI_DBG1("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        cmd_param.cmd_id,
                                        cmd_param.result,
                                        cmd_param.handle);

    rc = ais_client_process_cmd(p, &cmd_param, sizeof(cmd_param), sizeof(cmd_param), AIS_CONN_RECV_TIMEOUT);
    if (rc != CAMERA_SUCCESS)
    {
        goto EXIT_FLAG;
    }

    *p_event = cmd_param.event;

EXIT_FLAG:

    AIS_LOG_CLI(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, hndl, p_event, rc);

    return rc;
}


