/**
 * @file ais_server.c
 *
 * @brief implementation of ais server which handle both host and hypervisor
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>

#include "ais_comm.h"
#include "ais_log.h"

#include "ais_conn.h"
#include "ais_event_queue.h"

#include "CameraResult.h"
#include "CameraOSServices.h"

#ifdef ENABLE_DIAGNOSTIC
#include "ais_diag_server.h"
#endif

#if defined(__AGL__)
#include <limits.h>
#endif

#if defined(__INTEGRITY)
#include "pmem.h"
#include "pmemext.h"

// Object number 10 is defined for server VAS in corresponding INT file
#define AIS_SERV_OBJ_NUM  10
// Maximum thread priority for AIS server
const Value __PosixServerPriority = CAM_POSIXSERVER_PRIORITY;
#endif

// Priority for ais_server worker threads
#if defined(USE_HYP)
#define AIS_SRV_THRD_PRIO CAMERA_THREAD_PRIO_BACKEND
#else
#define AIS_SRV_THRD_PRIO CAMERA_THREAD_PRIO_NORMAL
#endif

#define AIS_CMD_THREAD_STACK_SIZE (CAM_THREAD_STACK_MIN * 2)

/**
 * supported versions
 */
const static unsigned int sg_ais_supported_version[] =
{
    AIS_VERSION,
    AIS_VERSION,
    AIS_VERSION,
};

/**
 * thread parameter for main or work connection
 */
typedef struct
{
    void         *p;  /**< server context */
    unsigned int idx; /**< connection index */
} s_ais_server_thread_param;


/**
 * server management structures
 */
typedef struct
{
    volatile qcarcam_hndl_t qcarcam_hndl;
    s_ais_conn_info info;

    /** command connections */
    s_ais_conn cmd_conn[AIS_CONN_CMD_MAX_NUM];

    /** command threads */
    s_ais_server_thread_param cmd_thread_param[AIS_CONN_CMD_MAX_NUM];
    void* cmd_thread_id[AIS_CONN_CMD_MAX_NUM];
    volatile bool cmd_thread_abort[AIS_CONN_CMD_MAX_NUM];

    /** event connection */
    s_ais_conn event_conn;

    /** event queue between cb and sender */
    s_ais_event_queue event_queue;

    /** event thread */
    void* event_thread_id;
    volatile bool event_thread_abort;

    /** local content */
    qcarcam_event_cb_t event_cb;        // callback associated with the handle
    void* priv_data;                    // QCARCAM_PARAM_PRIVATE_DATA

    /** ctxt health */
    unsigned int signal_attempts;       // count failed wait for signal attempts
    volatile bool signal_bit;           // used to check if context has been signaled
    volatile bool health_active;        // enables health check for currect context
    volatile bool ctxt_abort;           // set if context is in the process of being closed

} s_ais_client_ctxt;


/**
 * all client infos are stored here.
 * all operations to this array are guarded by single variable
 * and operations sequence in client thread.
 * so mutex is not necessary
 * android doesn't support current implementation,
 * need to use other way to handle it later
 */
static char  sgp_singleton_name[64];
#if defined(__QNXNTO__) || defined(__INTEGRITY)
static sem_t *sgsp_singleton_sem = NULL;
#elif defined(__ANDROID__) || defined(__AGL__) || defined(__LINUX)
static int sg_singleton_fd = -1;
#endif

static pthread_mutex_t sgs_mutex = PTHREAD_MUTEX_INITIALIZER;
static s_ais_client_ctxt sgs_ais_client_ctxt[AIS_CONTEXT_MAX_NUM];
static s_ais_conn sgs_ais_server_conn;

static const int exceptsigs[] = {
    SIGCHLD, SIGIO, SIGURG, SIGWINCH,
    SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGCONT,
    -1,
};


#define AIS_LOG_SRV(level, fmt...) AIS_LOG(AIS_MOD_ID_SERVER, level, fmt)

#define AIS_LOG_SRV_ERR(fmt...) AIS_LOG(AIS_MOD_ID_SERVER, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_SRV_WARN(fmt...) AIS_LOG(AIS_MOD_ID_SERVER, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_SRV_HIGH(fmt...) AIS_LOG(AIS_MOD_ID_SERVER, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_SRV_MED(fmt...) AIS_LOG(AIS_MOD_ID_SERVER, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_SRV_LOW(fmt...) AIS_LOG(AIS_MOD_ID_SERVER, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_SRV_DBG(fmt...) AIS_LOG(AIS_MOD_ID_SERVER, AIS_LOG_LVL_DBG, fmt)
#define AIS_LOG_SRV_DBG1(fmt...) AIS_LOG(AIS_MOD_ID_SERVER, AIS_LOG_LVL_DBG1, fmt)

#define AIS_LOG_LVL_SRV_API AIS_LOG_LVL_LOW
#define AIS_LOG_SRV_API(fmt...) AIS_LOG(AIS_MOD_ID_SERVER, AIS_LOG_LVL_SRV_API, fmt)

/**
 * Function declarations
 */
static int ais_server_exit(void);
static int ais_server_signal_thread(void *arg);
static void ais_server_register_signal_handler(void);
static char* ais_server_get_name(char *p_path);
static int ais_server_singleton_acquire(char *p_name);
static int ais_server_singleton_release(void);
static int ais_server_check_version(unsigned int version);
static int ais_server_init_client_ctxt(s_ais_client_ctxt *p);
static int ais_server_alloc_client_ctxt(void);
static int ais_server_free_client_ctxt(int idx);
static int ais_server_find(qcarcam_hndl_t handle);
static int ais_server_create_conns(s_ais_client_ctxt *p);
static int ais_server_create_conns_thread(void *p_arg);
static int ais_server_create_main_conn(s_ais_client_ctxt *p);
static int ais_server_destroy_main_conn(s_ais_client_ctxt *p);
static int ais_server_create_work_thread(s_ais_client_ctxt *p);
static int ais_server_destroy_work_thread(s_ais_client_ctxt *p);
static int ais_server_release_work_thread(s_ais_client_ctxt *p);
static int ais_server_create_work_conn(s_ais_client_ctxt *p);
static int ais_server_destroy_work_conn(s_ais_client_ctxt *p);
static void ais_server_health_event_cb(s_ais_client_ctxt *p, qcarcam_event_t event_id, qcarcam_event_payload_t *p_payload);
static void ais_server_event_cb(qcarcam_hndl_t handle, qcarcam_event_t event_id, qcarcam_event_payload_t *p_payload);
static int ais_server_event_send_thread(void *p_arg);
static int ais_server_create_event_thread(s_ais_client_ctxt *p);
static int ais_server_destroy_event_thread(s_ais_client_ctxt *p);
static int ais_server_release_event_thread(s_ais_client_ctxt *p);
static int ais_server_create_event_conn(s_ais_client_ctxt *p);
static int ais_server_destroy_event_conn(s_ais_client_ctxt *p);
static int ais_server_exchange(s_ais_client_ctxt *p, s_ais_conn *p_conn, s_ais_conn_info *p_info);
static int ais_server_create(s_ais_conn *p_conn);
static int ais_server_destroy(int idx, int flag);
static CameraResult ais_server_query_inputs(s_ais_client_ctxt *p, s_ais_cmd_query_inputs *p_param);
static CameraResult ais_server_open(s_ais_client_ctxt *p, s_ais_cmd_open *p_param);
static CameraResult ais_server_close(s_ais_client_ctxt *p, s_ais_cmd_close *p_param);
static CameraResult ais_server_g_param(s_ais_client_ctxt *p, s_ais_cmd_g_param *p_param);
static CameraResult ais_server_s_param(s_ais_client_ctxt *p, s_ais_cmd_s_param *p_param);
static CameraResult ais_server_s_buffers(s_ais_client_ctxt *p, int idx, s_ais_cmd_s_buffers *p_param);
static CameraResult ais_server_start(s_ais_client_ctxt *p, s_ais_cmd_start *p_param);
static CameraResult ais_server_stop(s_ais_client_ctxt *p, s_ais_cmd_stop *p_param);
static CameraResult ais_server_pause(s_ais_client_ctxt *p, s_ais_cmd_pause *p_param);
static CameraResult ais_server_resume(s_ais_client_ctxt *p, s_ais_cmd_resume *p_param);
static CameraResult ais_server_get_frame(s_ais_client_ctxt *p, s_ais_cmd_get_frame *p_param);
static CameraResult ais_server_release_frame(s_ais_client_ctxt *p, s_ais_cmd_release_frame *p_param);
static CameraResult ais_server_get_event(s_ais_client_ctxt *p, s_ais_cmd_get_event *p_param);
static int ais_server_process_cmd(s_ais_client_ctxt *p, unsigned int idx, u_ais_cmd *p_param);
static int ais_server_cmd_thread(void *p_arg);


/**
 * signal handling
 */
static volatile int sg_abort = 0;


/**
 * closes all connections and free all resouces.
 *
 * @return 0: success
 *
 */
static int ais_server_exit(void)
{
    int i;

    AIS_LOG_SRV_HIGH("E %s", __func__);

    sg_abort = 1;

    for (i = 0; i < AIS_CONTEXT_MAX_NUM; i++)
    {
        ais_server_destroy(i, 0);
    }

    AIS_CONN_API(ais_conn_close)(&sgs_ais_server_conn);

    AIS_LOG_SRV_HIGH("X %s", __func__);

    return 0;
}

/**
 *  signal handle thread
 *
 * @return none
 */
static int ais_server_signal_thread(void *arg)
{
    sigset_t sigset;
    int sig, i, rc = 0;

    pthread_detach(pthread_self());

    sigfillset(&sigset);
    for (i = 0; exceptsigs[i] != -1; i++)
    {
        sigdelset(&sigset, exceptsigs[i]);
    }

    AIS_LOG_SRV(AIS_LOG_LVL_HIGH, "ENTER %s", __func__);

    for (;;)
    {
        rc = sigwait(&sigset, &sig);
        if ( rc == 0)
        {
            /**
             * in QNX, after client closes the socket fd, ais_server uses the socket, will receive the SIGPIPE
             * in LV, doesn't come across it
             * */
            if (sig == SIGPIPE)
            {
                AIS_LOG_SRV(AIS_LOG_LVL_ALWAYS, "receive the signal SIGPIPE");
                continue;
            }
            else
            {
                AIS_LOG_SRV(AIS_LOG_LVL_ALWAYS, "receive the signal %d", sig);
                ais_server_exit();
                break;
            }
        }
        else
        {
            AIS_LOG_SRV_ERR("sigwait failed rc=%d", rc);
        }
    }

    AIS_LOG_SRV(AIS_LOG_LVL_ALWAYS, "EXIT %s", __func__);
    return 0;
}

/**
 * registers all signals which needs to be handled.
 *
 * @return none
 *
 */
static void ais_server_register_signal_handler(void)
{
    int i = 0;
    int rc = 0;
    char name[32];
    sigset_t sigset;
    void* signal_thread_handle = NULL;

    sigfillset(&sigset);
    for (i = 0; exceptsigs[i] != -1; i++)
    {
        sigdelset(&sigset, exceptsigs[i]);
    }
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    AIS_LOG_SRV_HIGH("E %s ", __func__);

    snprintf(name, sizeof(name), "ais_server_signal_thread");

    rc = CameraCreateThread(AIS_SRV_THRD_PRIO,
                            0,
                            &ais_server_signal_thread,
                            NULL,
                            0,
                            name,
                            &signal_thread_handle);

    if (rc != 0)
    {
        AIS_LOG_SRV_ERR("CameraCreateThread rc = %d", rc);
    }

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s %d", __func__,  rc);
}

/**
 * gets the name of program from the whole path.
 * @param p_path points the path of this program
 *
 * @return the name or NULL
 *
 */
static char* ais_server_get_name(char *p_path)
{
    char *p = NULL;

    if (p_path == NULL)
    {
        goto EXIT_FLAG;
    }

    p = strrchr(p_path, '/');
    if (p == NULL)
    {
        p = p_path;
    }
    else
    {
        p++;
    }

EXIT_FLAG:

    return p;
}

/**
 * creates and acquires singleton semaphore.
 * @param p_name points the name of this program
 *
 * @return 0: success, others: fail
 *
 */
static int ais_server_singleton_acquire(char *p_name)
{
    int rc = 0;

    AIS_LOG_SRV_HIGH("E %s 0x%p", __func__, p_name);

    if (p_name == NULL)
    {
        goto EXIT_FLAG;
    }

#if defined(__QNXNTO__) || defined(__INTEGRITY)
    // if enable given name, have to start with a slash
    snprintf(sgp_singleton_name, sizeof(sgp_singleton_name), "/%s", p_name);
    AIS_LOG_SRV_MED("%s:%d %s %p %s", __func__, __LINE__, p_name, sgsp_singleton_sem, sgp_singleton_name);

    sgsp_singleton_sem = sem_open(sgp_singleton_name, O_CREAT|O_EXCL, S_IRWXU, 0);
    if (sgsp_singleton_sem == SEM_FAILED)
    {
        rc = -2;
    }
#elif defined(CAMERA_UNITTEST) || defined(__AGL__)
    snprintf(sgp_singleton_name, sizeof(sgp_singleton_name), "%s/%s.lock", AIS_TEMP_PATH, p_name);
    AIS_LOG_SRV_MED("%s:%d %s %d %s", __func__, __LINE__, p_name, sg_singleton_fd, sgp_singleton_name);

    sg_singleton_fd = open(sgp_singleton_name, O_CREAT|O_EXCL, S_IRWXU);
    if (sg_singleton_fd == -1)
    {
        rc = -2;
    }
#endif

EXIT_FLAG:

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_name, rc);

    return rc;
}

/**
 * releases singleton semaphore.
 *
 * @return 0: success, others: fail
 *
 */
static int ais_server_singleton_release(void)
{
    int rc = 0;

    AIS_LOG_SRV_HIGH("E %s", __func__);

#if defined(__QNXNTO__) || defined(__INTEGRITY)
    if (sgsp_singleton_sem != NULL)
    {
        AIS_LOG_SRV_MED("%s:%d 0x%p %s", __func__, __LINE__, sgsp_singleton_sem, sgp_singleton_name);

        sem_close(sgsp_singleton_sem);
        sgsp_singleton_sem = NULL;

        sem_unlink(sgp_singleton_name);
        sgp_singleton_name[0] = '\0';
    }
#elif defined(CAMERA_UNITTEST) || defined(__AGL__)
    if (sg_singleton_fd != -1)
    {
        AIS_LOG_SRV_MED("%s:%d %d %s", __func__, __LINE__, sg_singleton_fd, sgp_singleton_name);

        close(sg_singleton_fd);
        sg_singleton_fd = -1;

        unlink(sgp_singleton_name);
        sgp_singleton_name[0] = '\0';
    }
#endif

    AIS_LOG_SRV_HIGH("X %s %d", __func__, rc);

    return rc;
}

/**
 * checks if the version is compatible
 *
 * @param version the checking version
 * @return 0: success, others: failed
 *
 */
static int ais_server_check_version(unsigned int version)
{
    int rc = -1;
    int i;

    //check backward-compatible
    for (i = 0; i < sizeof(sg_ais_supported_version)/sizeof(*sg_ais_supported_version); i++)
    {
        if (version == sg_ais_supported_version[i])
        {
            rc = 0;
            break;
        }
    }

    return rc;
}

/**
 * initializes one server context
 *
 * @param p points to a server context
 * @return 0: success, others: failed
 *
 */
static int ais_server_init_client_ctxt(s_ais_client_ctxt *p)
{
    int i;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    memset(p, 0, sizeof(s_ais_client_ctxt));

    for (i = 0; i < AIS_CONN_CMD_MAX_NUM; i++)
    {
        AIS_CONN_API(ais_conn_init)(p->cmd_conn + i);

        p->cmd_thread_param[i].p = p;
        p->cmd_thread_param[i].idx = i;

        p->cmd_thread_id[i] = 0;
        p->cmd_thread_abort[i] = FALSE;
    }

    AIS_CONN_API(ais_conn_init)(&p->event_conn);

    memset(&p->event_queue, 0, sizeof(p->event_queue));

    p->event_thread_id = 0;
    p->event_thread_abort = FALSE;

    p->event_cb = NULL;
    p->priv_data = NULL;

    p->signal_attempts = 0;
    p->signal_bit = FALSE;
    p->health_active = FALSE;
    p->ctxt_abort = FALSE;

    AIS_LOG_SRV_API("X %s 0x%p", __func__, p);

    return 0;
}

/**
 * allocates one server context from the global resource
 *
 * @return >=0: index of valid context, <0: failed
 *
 */
static int ais_server_alloc_client_ctxt(void)
{
    int rc = -1;
    int i;

    AIS_LOG_SRV_API("E %s", __func__);

    pthread_mutex_lock(&sgs_mutex);

    for (i = 0; i < AIS_CONTEXT_MAX_NUM; i++)
    {
        if (sgs_ais_client_ctxt[i].qcarcam_hndl == NULL)
        {
            ais_server_init_client_ctxt(&sgs_ais_client_ctxt[i]);

            sgs_ais_client_ctxt[i].qcarcam_hndl = AIS_CONTEXT_IN_USE;

            rc = i;
            break;
        }
    }

    pthread_mutex_unlock(&sgs_mutex);

    AIS_LOG_SRV(rc >= 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_WARN,
                "X %s %d", __func__, rc);

    return rc;
}

/**
 * frees one server context to the global resource
 * @parma idx the index of a server context
 * @return 0: success, others: failed
 */
static int ais_server_free_client_ctxt(int idx)
{
    int rc = 0;

    AIS_LOG_SRV_API("E %s %d", __func__, idx);

    if (idx < 0 || idx >= AIS_CONTEXT_MAX_NUM)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    pthread_mutex_lock(&sgs_mutex);

    sgs_ais_client_ctxt[idx].qcarcam_hndl = NULL;

    pthread_mutex_unlock(&sgs_mutex);

EXIT_FLAG:

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s %d %d", __func__, idx, rc);

    return rc;
}

/**
 * finds the index of one server context with specific handle
 * @parma handle handle opened
 * @return >=0: index of valid context, <0: failed
 */
static int ais_server_find(qcarcam_hndl_t handle)
{
    int rc = -1;
    int i;

    AIS_LOG_SRV_DBG("E %s 0x%p", __func__, handle);

    for (i = 0; i < AIS_CONTEXT_MAX_NUM; i++)
    {
        if (sgs_ais_client_ctxt[i].qcarcam_hndl == handle)
        {
            rc = i;
            break;
        }
    }

    AIS_LOG_SRV(rc >= 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, handle, rc);

    return rc;
}

/**
 * Creates all connections for one client based on exchanged connection ID
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_create_conns_thread(void *p_arg)
{
    int rc = 0;
    s_ais_client_ctxt *p = (s_ais_client_ctxt *)p_arg;

    pthread_detach(pthread_self());

    rc = ais_server_create_main_conn(p);
    if (0 != rc)
    {
        goto EXIT_FLAG;
    }

    rc = ais_server_create_event_conn(p);
    if (0 != rc)
    {
        goto EXIT_FLAG;
    }

EXIT_FLAG:
    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * Creates thread used to initialize connections for one client
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_create_conns(s_ais_client_ctxt *p)
{
    int rc = 0;
    char name[32];
    void* thread_handl;

    snprintf(name, sizeof(name), "srv_cr_conn_%d", AIS_CONN_ID_CMD_MAIN(p->info.id));

    rc = CameraCreateThread(AIS_SRV_THRD_PRIO,
                            0,
                            &ais_server_create_conns_thread,
                            p,
                            AIS_CMD_THREAD_STACK_SIZE,
                            name,
                            &thread_handl);

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * creates the main connection and thread with one client based on the exchaged connection id
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_create_main_conn(s_ais_client_ctxt *p)
{
    int rc = 0;
    char name[32];
    s_ais_conn temp_conn;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    rc = AIS_CONN_API(ais_conn_open)(&temp_conn, AIS_CONN_ID_CMD_MAIN(p->info.id), 1);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    rc = AIS_CONN_API(ais_conn_accept)(&temp_conn, &p->cmd_conn[AIS_CONN_CMD_IDX_MAIN], AIS_CONN_MODE_WORK);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    AIS_CONN_API(ais_conn_close)(&temp_conn);

    snprintf(name, sizeof(name), "srv_cmd_%d", AIS_CONN_ID_CMD_MAIN(p->info.id));

    if (0 != (rc = CameraCreateThread(AIS_SRV_THRD_PRIO,
                                      0,
                                      &ais_server_cmd_thread,
                                      &p->cmd_thread_param[AIS_CONN_CMD_IDX_MAIN],
                                      AIS_CMD_THREAD_STACK_SIZE,
                                      name,
                                      &p->cmd_thread_id[AIS_CONN_CMD_IDX_MAIN])))
    {
        AIS_LOG_SRV_ERR("CameraCreateThread rc = %d", rc);
    }

EXIT_FLAG:
    if (rc != 0)
    {
        AIS_CONN_API(ais_conn_close)(&p->cmd_conn[AIS_CONN_CMD_IDX_MAIN]);
    }

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * destroys the main connection, main thread would exit itself.
 * @param p points to a server context
 * @param mode indicates if it is destroyed inside
 * @return 0: success, others: failed
 */
static int ais_server_destroy_main_conn(s_ais_client_ctxt *p)
{
    int rc = 0;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    rc = AIS_CONN_API(ais_conn_close)(&p->cmd_conn[AIS_CONN_CMD_IDX_MAIN]);

    if (p->cmd_thread_id[AIS_CONN_CMD_IDX_MAIN] != 0)
    {
        p->cmd_thread_abort[AIS_CONN_CMD_IDX_MAIN] = TRUE;
        p->cmd_thread_id[AIS_CONN_CMD_IDX_MAIN] = 0;
    }

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * creates all working threads with one client based on the exchaged connection range
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_create_work_thread(s_ais_client_ctxt *p)
{
    int rc = 0;
    int i = 0;
    char name[32];

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    for (i = AIS_CONN_CMD_IDX_WORK; i < AIS_CONN_CMD_NUM(p->info.cnt); i++)
    {
        snprintf(name, sizeof(name), "srv_cmd_%d", AIS_CONN_ID_CMD_WORK(p->info.id) + i - AIS_CONN_CMD_IDX_WORK);

        if (0 != (rc = CameraCreateThread(AIS_SRV_THRD_PRIO,
                                          0,
                                          &ais_server_cmd_thread,
                                          &p->cmd_thread_param[i],
                                          AIS_CMD_THREAD_STACK_SIZE,
                                          name,
                                          &p->cmd_thread_id[i])))
        {
            AIS_LOG_SRV_ERR("CameraCreateThread rc = %d", rc);
        }
    }

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * destroys all the working threads
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_destroy_work_thread(s_ais_client_ctxt *p)
{
    int i;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    //stop threads
    for (i = AIS_CONN_CMD_IDX_WORK; i < AIS_CONN_CMD_NUM(p->info.cnt); i++)
    {
        if (p->cmd_thread_id[i] != 0)
        {
            p->cmd_thread_abort[i] = TRUE;
        }
    }

    AIS_LOG_SRV_API("X %s 0x%p", __func__, p);

    return 0;
}

/**
 * waits all the working threads exit
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_release_work_thread(s_ais_client_ctxt *p)
{
    int i;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    for (i = AIS_CONN_CMD_IDX_WORK; i < AIS_CONN_CMD_NUM(p->info.cnt); i++)
    {
        /* Wait until thread is done */
        if (p->cmd_thread_id[i] != 0)
        {
            if (0 != (CameraJoinThread(p->cmd_thread_id[i], NULL)))
            {
                AIS_LOG_SRV_ERR("CameraJoinThread failed");
            }
            p->cmd_thread_id[i] = 0;
        }
    }

    AIS_LOG_SRV_API("X %s 0x%p", __func__, p);

    return 0;
}

/**
 * creates all working connections and threads with one client based on the exchaged connection id
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_create_work_conn(s_ais_client_ctxt *p)
{
    int rc = 0;
    int i;
    s_ais_conn temp_conn;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    for (i = AIS_CONN_CMD_IDX_WORK; i < AIS_CONN_CMD_NUM(p->info.cnt); i++)
    {
        rc = AIS_CONN_API(ais_conn_open)(&temp_conn, AIS_CONN_ID_CMD_WORK(p->info.id) + i - AIS_CONN_CMD_IDX_WORK, AIS_CONN_CMD_MAX_NUM);
        if (rc != 0)
        {
            rc = -1;
            goto EXIT_FLAG;
        }

        rc = AIS_CONN_API(ais_conn_accept)(&temp_conn, p->cmd_conn + i, AIS_CONN_MODE_WORK);
        if (rc != 0)
        {
            rc = -2;
            goto EXIT_FLAG;
        }

        AIS_CONN_API(ais_conn_close)(&temp_conn);
    }

    rc = ais_server_create_work_thread(p);

EXIT_FLAG:

    if (rc != 0)
    {
        ais_server_destroy_work_thread(p);

        for (i = AIS_CONN_CMD_IDX_WORK; i < AIS_CONN_CMD_NUM(p->info.cnt); i++)
        {
            AIS_CONN_API(ais_conn_close)(p->cmd_conn + i);
        }
    }

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * destroys all working connections and threads
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_destroy_work_conn(s_ais_client_ctxt *p)
{
    int i = 0;
    int rc = 0;
    int temp_ret = 0;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    temp_ret = ais_server_destroy_work_thread(p);
    if (temp_ret != 0)
    {
        rc = temp_ret;
    }

    //close connections
    for (i = AIS_CONN_CMD_IDX_WORK; i < AIS_CONN_CMD_NUM(p->info.cnt); i++)
    {
        temp_ret = AIS_CONN_API(ais_conn_close)(p->cmd_conn + i);
        if (temp_ret != 0 && rc == 0)
        {
            rc = temp_ret;
        }
    }

    temp_ret = ais_server_release_work_thread(p);
    if (temp_ret != 0 && rc == 0)
    {
        rc = temp_ret;
    }

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * enqueues events from health monitor.
 * @param index of valid context
 * @param event_id generated event id
 * @param p_payload points to the payload of health message
 * @return none
 */
static void ais_server_health_event_cb(s_ais_client_ctxt *p, qcarcam_event_t event_id,
                                                qcarcam_event_payload_t *p_payload)
{
    s_ais_event event;

    memset(&event, 0, sizeof(event));
    event.event_id = event_id;
    if (p_payload != NULL)
    {
        event.payload = *p_payload;
    }

    AIS_LOG_SRV_DBG("health msg cb event cnt: %d", event.event_cnt);

    ais_event_queue_enqueue(&p->event_queue, &event);
}

/**
 * enqueues events from engine. this is callback, should be registered to engine.
 * @param handle opened handle
 * @param event_id generated event id
 * @param p_payload points to the payload of the specific event
 * @return none
 */
static void ais_server_event_cb(qcarcam_hndl_t handle, qcarcam_event_t event_id,
                                        qcarcam_event_payload_t *p_payload)
{
    int idx = ais_server_find(handle);

    if (idx >= 0)
    {
        s_ais_event event;

        memset(&event, 0, sizeof(event));
        event.event_id = event_id;
        if (p_payload != NULL)
        {
            event.payload = *p_payload;
        }

        AIS_LOG_SRV_DBG("callback event id: %d cnt: %d", event.event_id, event.event_cnt);

        ais_event_queue_enqueue(&sgs_ais_client_ctxt[idx].event_queue, &event);
    }
}

/**
 * dequeues events and sends to a client.
 * this is thread function which should be run in one thread
 * @param p_arg points to a server context.
 * @return NULL
 */
static int ais_server_event_send_thread(void *p_arg)
{
    s_ais_client_ctxt *p = (s_ais_client_ctxt *)p_arg;
    int rc = 0;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p_arg);

    while (!sg_abort && !p->event_thread_abort)
    {
        s_ais_event event;

        rc = ais_event_queue_dequeue(&p->event_queue, &event, 100);
        if (rc != 0)
        {
            continue;
        }

        AIS_LOG_SRV_DBG("sent event id: %d cnt: %d", event.event_id, event.event_cnt);

        rc = AIS_CONN_API(ais_conn_send)(&p->event_conn, &event, sizeof(event));
    }

    AIS_LOG_SRV_API("X %s 0x%p", __func__, p_arg);

    return 0;
}

/**
 * creates all event threads with one client based on the exchaged connection id
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_create_event_thread(s_ais_client_ctxt *p)
{
    int rc = 0;
    char name[32];

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    snprintf(name, sizeof(name), "srv_evnt_snd_%d", AIS_CONN_ID_EVENT(p->info.id));

    if (0 != (rc = CameraCreateThread(AIS_SRV_THRD_PRIO,
                                      0,
                                      &ais_server_event_send_thread,
                                      p,
                                      0,
                                      name,
                                      &p->event_thread_id)))
    {
        AIS_LOG_SRV_ERR("CameraCreateThread rc = %d", rc);
    }

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * destroys all the events threads
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_destroy_event_thread(s_ais_client_ctxt *p)
{
    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    //destroys event thread
    if (p->event_thread_id != 0)
    {
        p->event_thread_abort = TRUE;
    }

    AIS_LOG_SRV_API("X %s 0x%p", __func__, p);

    return 0;
}

/**
 * waits all the event threads exit
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_release_event_thread(s_ais_client_ctxt *p)
{
    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    //waits event thread exit
    if (p->event_thread_id != 0)
    {
        if (0 != (CameraJoinThread(p->event_thread_id, NULL)))
        {
            AIS_LOG_SRV_ERR("CameraJoinThread failed");
        }
        p->event_thread_id = 0;
    }

    AIS_LOG_SRV_API("X %s 0x%p", __func__, p);

    return 0;
}

/**
 * creates the event connection and thread with one client based on the exchaged connection id
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_create_event_conn(s_ais_client_ctxt *p)
{
    int rc = 0;
    s_ais_conn temp_conn;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    rc = AIS_CONN_API(ais_conn_open)(&temp_conn, AIS_CONN_ID_EVENT(p->info.id), 1);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    rc = AIS_CONN_API(ais_conn_accept)(&temp_conn, &p->event_conn, AIS_CONN_MODE_WORK);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    AIS_CONN_API(ais_conn_close)(&temp_conn);

    rc = ais_event_queue_init(&p->event_queue, 2, 60, ais_event_queue_event_2_idx);
    if (rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    rc = ais_server_create_event_thread(p);
    if (rc != 0)
    {
        ais_event_queue_deinit(&p->event_queue);
    }

EXIT_FLAG:

    if (rc != 0)
    {
        AIS_CONN_API(ais_conn_close)(&p->event_conn);
    }

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * destroys the event connection and thread
 * @param p points to a server context
 * @return 0: success, others: failed
 */
static int ais_server_destroy_event_conn(s_ais_client_ctxt *p)
{
    int rc = 0;
    int temp_ret = 0;

    AIS_LOG_SRV_API("E %s 0x%p", __func__, p);

    temp_ret = ais_server_destroy_event_thread(p);
    if (temp_ret != 0)
    {
        rc = temp_ret;
    }

    temp_ret = ais_server_release_event_thread(p);
    if (temp_ret != 0 && rc == 0)
    {
        rc = temp_ret;
    }

    temp_ret = AIS_CONN_API(ais_conn_close)(&p->event_conn);
    if (temp_ret != 0 && rc == 0)
    {
        rc = temp_ret;
    }

    ais_event_queue_deinit(&p->event_queue);

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

#ifndef HEALTH_DISABLED
/**
 * checks contexts are active and destroys them otherwise
 * @param void p_arg: no params
 * @return void
 */
static int ais_server_health_thread(void *p_arg)
{
    int i = 0, j = 0;
    int sleep_usec = HEALTH_CHECK_DELAY_MSEC * 1000; // Convert msec to usec

    AIS_LOG_SRV_HIGH("E %s", __func__);

    pthread_detach(pthread_self());

    while (!sg_abort)
    {
        /* Check if active contexts have been signaled */
        for (i = 0; i < AIS_CONTEXT_MAX_NUM; i++)
        {
            bool abort_ctxt = FALSE;

            if (sgs_ais_client_ctxt[i].health_active)
            {
                /* Send health event to client */
                ais_server_health_event_cb(&sgs_ais_client_ctxt[i],
                                    (qcarcam_event_t)EVENT_HEALTH_MSG,
                                    NULL);

                /* Check if signal was set */
                if (sgs_ais_client_ctxt[i].signal_bit == TRUE)
                {
                    sgs_ais_client_ctxt[i].signal_bit = FALSE;
                    sgs_ais_client_ctxt[i].signal_attempts = 0;
                }
                else
                {
                    /* If signal was read as false more than MAX_HEALTH_SIGNAL_ATTEMPTS */
                    /* abort threads from such context */
                    if (sgs_ais_client_ctxt[i].signal_attempts > MAX_HEALTH_SIGNAL_ATTEMPTS
                        && !sgs_ais_client_ctxt[i].ctxt_abort)
                    {
                        sgs_ais_client_ctxt[i].event_thread_abort = TRUE;
                        sgs_ais_client_ctxt[i].cmd_thread_abort[AIS_CONN_CMD_IDX_MAIN] = TRUE;
                        sgs_ais_client_ctxt[i].cmd_thread_abort[AIS_CONN_CMD_IDX_WORK] = TRUE;

                        abort_ctxt = TRUE;

                        AIS_LOG_SRV_WARN("Health signal timeout. Closing contexts from %d:%d",
                            sgs_ais_client_ctxt[i].info.pid, sgs_ais_client_ctxt[i].info.gid);
                    }
                    sgs_ais_client_ctxt[i].signal_attempts++;
                }
            }

            if (abort_ctxt)
            {
                for (j = 0; j < AIS_CONTEXT_MAX_NUM; j++)
                {
                    if (i != j &&
                        !sgs_ais_client_ctxt[j].ctxt_abort &&
                        sgs_ais_client_ctxt[j].info.gid == sgs_ais_client_ctxt[i].info.gid &&
                        sgs_ais_client_ctxt[j].info.pid == sgs_ais_client_ctxt[i].info.pid &&
                        sgs_ais_client_ctxt[j].qcarcam_hndl &&
                        sgs_ais_client_ctxt[j].qcarcam_hndl != AIS_CONTEXT_IN_USE)
                    {
                        AIS_LOG_SRV_WARN("Health signal timeout. Closing context %d", j);
#ifdef USE_HYP
                        /* If HYP BE server, send try sending msg to client */
                        ais_server_destroy(j, 0);
#else
                        /* Destroy context without trying to send msg to client */
                        ais_server_destroy(j, 1);
#endif
                    }
                }
            }
        }

        if (!sg_abort)
        {
            usleep(sleep_usec);
        }
    }

    AIS_LOG_SRV_HIGH("X %s %d", __func__);
    return 0;
}

static int ais_server_create_health_thread()
{
    int rc = 0;
    char name[32];
    void *health_thread_id = NULL;

    AIS_LOG_SRV_API("E %s", __func__);

    snprintf(name, sizeof(name), "srv_health");

    if (0 != (rc = CameraCreateThread(AIS_SRV_THRD_PRIO,
                                      0,
                                      &ais_server_health_thread,
                                      NULL,
                                      0,
                                      name,
                                      &health_thread_id)))
    {
        AIS_LOG_SRV_ERR("CameraCreateThread rc = %d", rc);
    }

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s %d", __func__, rc);

    return rc;
}
#endif //HEALTH_DISABLED

/**
 * exchanges the infos of connections
 * @param p_conn points to a connection
 * @param p_info points to a connection info which would be sent back to a client
 * @return 0: success, others: failed
 */
static int ais_server_exchange(s_ais_client_ctxt *p, s_ais_conn *p_conn, s_ais_conn_info *p_info)
{
    int rc = 0;
    int version_check = 0;
    s_ais_conn_info info;
    unsigned int size;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p_conn, p_info);

    size = sizeof(s_ais_conn_info);
    rc = AIS_CONN_API(ais_conn_recv)(p_conn, &info, &size, AIS_CONN_RECV_TIMEOUT);
    if (rc != 0 || size == 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    if (info.cnt > p_info->cnt)
    {
        info.cnt = p_info->cnt;
    }

    info.id = p_info->id;
    info.version = AIS_VERSION;
    info.result = CAMERA_SUCCESS;

    if (info.app_version != AIS_VERSION)
    {
        AIS_LOG_SRV_WARN("API version mismatch between Client and Server");
    }
    version_check = ais_server_check_version(info.app_version);
    if (version_check != 0)
    {
        AIS_LOG_SRV_ERR("API version of Client is unsupported by Server");
        info.result = CAMERA_EVERSIONNOTSUPPORT;
    }
    else
    {
        AIS_LOG_SRV_WARN("API version of Client is compatible with version of Server");
    }

    version_check = ais_server_check_version(info.version);
    if (version_check != 0)
    {
        AIS_LOG_SRV_ERR("API version of APP is unsupported by Server");
        info.result = CAMERA_EVERSIONNOTSUPPORT;
    }
    else
    {
        AIS_LOG_SRV_WARN("API version of APP is compatible with version of Server");
    }

    if (NULL != p &&
        CAMERA_SUCCESS == info.result)
    {
        p_info->cnt = info.cnt;
        p_info->pid = info.pid;
        p_info->gid = info.gid;
        p_info->flags = info.flags;
        p->info = *p_info;

        rc = ais_server_create_conns(p);
        if (0 != rc)
        {
            info.result = CAMERA_EFAILED;
            goto SEND_REPLY;
        }

        p->health_active = (p_info->flags & AIS_CONN_FLAG_HEALTH_CONN) ? TRUE : FALSE;
    }
    else
    {
        info.result = CAMERA_ENOMORE;
    }

SEND_REPLY:
    rc = AIS_CONN_API(ais_conn_send)(p_conn, &info, sizeof(s_ais_conn_info));
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (info.result != CAMERA_SUCCESS)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

EXIT_FLAG:

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p_conn, p_info, rc);

    return rc;
}

/**
 * creates a server context and setup connection with a client
 * it allocates a server context, assigns connection id and range to a client
 * @param p_conn points to a connection
 * @return 0: success, others: failed
 */
static int ais_server_create(s_ais_conn *p_conn)
{
    int rc = 0;
    int idx = -1;
    s_ais_client_ctxt *p = NULL;
    s_ais_conn_info info;

    AIS_LOG_SRV_HIGH("E %s 0x%p", __func__, p_conn);

    idx = ais_server_alloc_client_ctxt();
    if (idx < 0)
    {
        info.id = -1;
        info.cnt = 0;
    }
    else
    {
        //1 is used for reserving the original one when the first client is connected
        info.id = (idx + 1) * AIS_CONN_MAX_NUM;
        info.cnt = AIS_CONN_MAX_NUM;
    }

    AIS_LOG_SRV_MED("%s:%d %d %d %d", __func__, __LINE__, info.id, info.cnt, idx);

    if (idx >= 0)
    {
        p = &sgs_ais_client_ctxt[idx];
    }

    rc = ais_server_exchange(p, p_conn, &info);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    if (idx < 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

EXIT_FLAG:

    if (rc != 0 && idx >= 0)
    {
        ais_server_free_client_ctxt(idx);
        idx = -1;
    }

    AIS_LOG_SRV(idx >= 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p_conn, rc);

    return rc;
}

/**
 * destroys a server context and frees all resource
 * @param idx index of a server context
 * @param flag indicates if context is aborted from health monitor
 * @return 0: success, others: failed
 */
static int ais_server_destroy(int idx, int flag)
{
    int rc = 0;
    int temp_ret = 0;
    int is_aborted = 0;

    s_ais_client_ctxt *p;

    AIS_LOG_SRV_HIGH("E %s %d", __func__, idx);

    if (idx < 0 || idx >= AIS_CONTEXT_MAX_NUM)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    p = &sgs_ais_client_ctxt[idx];

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
        //event callback has stopped because of close operation
        //so no race condition is existing for closing event connection
        //if it is not closed by close operation, disable it here,
        //which makes event callback non-functional,
        //and make sure the last event callback is finished.
        if (p->qcarcam_hndl != NULL && p->qcarcam_hndl != AIS_CONTEXT_IN_USE
            && flag == 0)
        {
            CameraResult ret = ais_close(p->qcarcam_hndl);
            p->qcarcam_hndl = AIS_CONTEXT_IN_USE;

            if (ret != CAMERA_SUCCESS)
            {
                usleep(100 * 10000);
            }
        }

        temp_ret = ais_server_destroy_event_conn(p);
        if (temp_ret != 0)
        {
            rc = temp_ret;
        }

        p->health_active = FALSE;
        p->signal_attempts = 0;
        p->signal_bit = FALSE;

        temp_ret = ais_server_destroy_work_conn(p);
        if(temp_ret != 0 && rc == 0)
        {
            rc = temp_ret;
        }

        temp_ret = ais_server_destroy_main_conn(p);
        if(temp_ret != 0 && rc == 0)
        {
            rc = temp_ret;
        }

        temp_ret = ais_server_free_client_ctxt(idx);
        if(temp_ret != 0 && rc == 0)
        {
            rc = temp_ret;
        }
    }

EXIT_FLAG:

    AIS_LOG_SRV(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s %d %d", __func__, idx, rc);

    return rc;
}

/**
 * processes query_inputs command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_query_inputs(s_ais_client_ctxt *p, s_ais_cmd_query_inputs *p_param)
{
    CameraResult rc = CAMERA_SUCCESS;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_MED("%s:%d 0x%x %d %d %d 0x%x", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->is_p_inputs_set,
                                        p_param->size);

    if (p_param->is_p_inputs_set)
    {
        if (p_param->size > MAX_NUM_AIS_INPUTS)
        {
            p_param->size = MAX_NUM_AIS_INPUTS;
        }
    }

    if (rc != CAMERA_SUCCESS)
    {
        goto EXIT_FLAG;
    }

    rc = ais_query_inputs(p_param->is_p_inputs_set ? p_param->inputs : NULL,
                            p_param->size, &p_param->ret_size);

EXIT_FLAG:

    p_param->result = rc;
    p_param->flags = AIS_VERSION;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes open command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_open(s_ais_client_ctxt *p, s_ais_cmd_open *p_param)
{
    CameraResult rc;
    qcarcam_hndl_t handle;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_MED("%s:%d 0x%x %d %d", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->input_desc);

    handle = ais_open(p_param->input_desc);
    if (handle != 0)
    {
        p->qcarcam_hndl = handle;

        p_param->handle = handle;
        rc = CAMERA_SUCCESS;
    }
    else
    {
        rc = CAMERA_EFAILED;
    }

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes close command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_close(s_ais_client_ctxt *p, s_ais_cmd_close *p_param)
{
    CameraResult rc;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_MED("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle);

    rc = ais_close(p_param->handle);
    p->qcarcam_hndl = AIS_CONTEXT_IN_USE;

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes g_param command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_g_param(s_ais_client_ctxt *p, s_ais_cmd_g_param *p_param)
{
    CameraResult rc;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_MED("%s:%d 0x%x %d 0x%p %d", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle,
                                        p_param->param);

    rc = ais_g_param(p_param->handle, p_param->param, &p_param->value);

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes s_param command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_s_param(s_ais_client_ctxt *p, s_ais_cmd_s_param *p_param)
{
    CameraResult rc;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_MED("%s:%d 0x%x %d 0x%p %d", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle,
                                        p_param->param);

    if (p_param->param == QCARCAM_PARAM_EVENT_CB)
    {
        p_param->value.ptr_value = ais_server_event_cb;
    }

    rc = ais_s_param(p_param->handle, p_param->param, &p_param->value);

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes s_buffers command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_s_buffers(s_ais_client_ctxt *p, int idx, s_ais_cmd_s_buffers *p_param)
{
    CameraResult rc;
    int ret;
    int i;

    AIS_LOG_SRV_API("E %s 0x%p %d 0x%p", __func__, p, idx, p_param);

    if (p_param->buffers.n_buffers > QCARCAM_MAX_NUM_BUFFERS)
    {
        rc = CAMERA_EOUTOFBOUND;
        goto EXIT_FLAG;
    }

    //sending socket would merge multiple messages into one
    //if receiving socket doesn't receive them immediately.
    //use this send/recv pair to synchronize on both sides
    i = p_param->buffers.n_buffers;
    ret = AIS_CONN_API(ais_conn_send)(&p->cmd_conn[idx], &i, sizeof(int));
    if (ret != 0)
    {
        rc = CAMERA_EFAILED;
        goto EXIT_FLAG;
    }

    p_param->buffers.buffers = p_param->buffer;

    AIS_LOG_SRV_MED("%s:%d %d", __func__, __LINE__, p_param->buffers.n_buffers);
    for (i = 0; i < p_param->buffers.n_buffers; i++)
    {
        ret = AIS_CONN_API(ais_conn_import)(&p->cmd_conn[idx],
                                &p_param->buffers.buffers[i].planes[0].p_buf,
                                p_param->buffers.buffers[i].planes[0].size, 1000);
        if(ret != 0)
        {
            AIS_LOG_SRV_ERR("ais_conn_import error ret = %d", ret);
            rc = CAMERA_EBADITEM;
            goto EXIT_FLAG;
        }
    }

    rc = ais_s_buffers(p_param->handle, &p_param->buffers);
    AIS_CONN_API(ais_conn_flush)(&p->cmd_conn[idx], (rc == CAMERA_SUCCESS) ? 0 : -1);

EXIT_FLAG:

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d 0x%p %d", __func__, p, idx, p_param, rc);

    return rc;
}

/**
 * processes start command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_start(s_ais_client_ctxt *p, s_ais_cmd_start *p_param)
{
    CameraResult rc;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_MED("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle);

    rc = ais_start(p_param->handle);

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes stop command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_stop(s_ais_client_ctxt *p, s_ais_cmd_stop *p_param)
{
    CameraResult rc;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_MED("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle);

    rc = ais_stop(p_param->handle);

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes pause command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_pause(s_ais_client_ctxt *p, s_ais_cmd_pause *p_param)
{
    CameraResult rc;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_API("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle);

    rc = ais_pause(p_param->handle);

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes resume command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_resume(s_ais_client_ctxt *p, s_ais_cmd_resume *p_param)
{
    CameraResult rc;

    AIS_LOG_SRV_API("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_MED("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle);

    rc = ais_resume(p_param->handle);

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_SRV_API : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes get_frame command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_get_frame(s_ais_client_ctxt *p, s_ais_cmd_get_frame *p_param)
{
    CameraResult rc;

    AIS_LOG_SRV_LOW("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_DBG1("%s:%d 0x%x %d 0x%p %lld %d", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle,
                                        p_param->timeout,
                                        p_param->flags);

    rc = ais_get_frame(p_param->handle, &p_param->frame_info, p_param->timeout, p_param->flags);

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes release_frame command
 * @param p points to a server context
 * @param p_param points to the coresponding command parameter
 * @return CameraResult
 */
static CameraResult ais_server_release_frame(s_ais_client_ctxt *p, s_ais_cmd_release_frame *p_param)
{
    CameraResult rc;

    AIS_LOG_SRV_LOW("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_DBG1("%s:%d 0x%x %d 0x%p %d", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle,
                                        p_param->idx);

    rc = ais_release_frame(p_param->handle, p_param->idx);

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

//TODO: not supported fully, have to change ais engine
static CameraResult ais_server_get_event(s_ais_client_ctxt *p, s_ais_cmd_get_event *p_param)
{
    CameraResult rc = CAMERA_SUCCESS;

    AIS_LOG_SRV_LOW("E %s 0x%p 0x%p", __func__, p, p_param);

    AIS_LOG_SRV_DBG1("%s:%d 0x%x %d 0x%p", __func__, __LINE__,
                                        p_param->cmd_id,
                                        p_param->result,
                                        p_param->handle);

//    rc = ais_get_event(p_param->handle, &p_param->event);

    p_param->result = rc;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d", __func__, p, p_param, rc);

    return rc;
}

/**
 * processes all commands
 * @param p points to a server context
 * @param idx indicates which connection should be used
 * @param p_param points to commands' parameter holder
 * @return 0: success, others: failed
 */
static int ais_server_process_cmd(s_ais_client_ctxt *p, unsigned int idx, u_ais_cmd *p_param)
{
    int rc = -1;
    CameraResult ret;
    s_ais_cmd_header *p_header = &p_param->header;

    AIS_LOG_SRV_LOW("E %s 0x%p %d 0x%p", __func__, p, idx, p_param);

    switch (p_header->cmd_id)
    {
    case AIS_CMD_QUERY_INPUTS:
        ret = ais_server_query_inputs(p, &p_param->query_inputs);
        break;

    case AIS_CMD_OPEN:
        ret = ais_server_open(p, &p_param->open);
        break;

    case AIS_CMD_CLOSE:
        ret = ais_server_close(p, &p_param->close);
        break;

    case AIS_CMD_G_PARAM:
        ret = ais_server_g_param(p, &p_param->g_param);
        break;

    case AIS_CMD_S_PARAM:
        ret = ais_server_s_param(p, &p_param->s_param);
        break;

    case AIS_CMD_S_BUFFERS:
        ret = ais_server_s_buffers(p, idx, &p_param->s_buffers);
        break;

    case AIS_CMD_START:
        ret = ais_server_start(p, &p_param->start);
        break;

    case AIS_CMD_STOP:
        ret = ais_server_stop(p, &p_param->stop);
        break;

    case AIS_CMD_PAUSE:
        ret = ais_server_pause(p, &p_param->pause);
        break;

    case AIS_CMD_RESUME:
        ret = ais_server_resume(p, &p_param->resume);
        break;

    case AIS_CMD_GET_FRAME:
        ret = ais_server_get_frame(p, &p_param->get_frame);
        break;

    case AIS_CMD_RELEASE_FRAME:
        ret = ais_server_release_frame(p, &p_param->release_frame);
        break;

    case AIS_CMD_GET_EVENT:
        ret = ais_server_get_event(p, &p_param->get_event);
        break;

    case AIS_CMD_HEALTH:
        p->health_active = TRUE;
        ret = CAMERA_SUCCESS;
        break;

    default:
        AIS_LOG_SRV_ERR("%s:%d unsupported cmd id 0x%x", __func__, __LINE__, p_header->cmd_id);
        ret = CAMERA_EUNSUPPORTED;
        p_header->result = ret;
        break;
    }

    rc = (ret == CAMERA_SUCCESS) ? 0 : -1;

    AIS_LOG_SRV(rc == CAMERA_SUCCESS ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d 0x%p %d", __func__, p, idx, p_param, rc);

    return rc;
}

/**
 * processes all commands, this thread is used for main or work connections
 * the main and work threads are processing commands in the same way. the only difference is
 * the main thread would free all resources including those allocated by work and event threads
 * @param p_arg points to a server thread structure
 * @return NULL
 */
static int ais_server_cmd_thread(void *p_arg)
{
    int rc = 0;
    s_ais_server_thread_param *p_param = (s_ais_server_thread_param *)p_arg;
    s_ais_client_ctxt *p = (s_ais_client_ctxt *)p_param->p;
    unsigned int idx = p_param->idx;
    u_ais_cmd cmd;
    int cmd_id;
    unsigned int size;

#if defined(__INTEGRITY)
    int32_t pmem_init_attempts = 0;

    if (idx == AIS_CONN_CMD_IDX_MAIN)
    {
        while (pmem_init())
        {
            usleep(10000);
            pmem_init_attempts++;
            if (pmem_init_attempts > 2)
            {
                AIS_LOG_SRV_ERR("pmem initialization failed");
                break;
            }
        }
    }
#endif

    AIS_LOG_SRV_HIGH("E %s 0x%p %d", __func__, p, idx);

    if (idx == AIS_CONN_CMD_IDX_MAIN)
    {
        pthread_detach(pthread_self());
    }

    while (!sg_abort && !p->cmd_thread_abort[idx])
    {
        size = sizeof(cmd);
        rc = AIS_CONN_API(ais_conn_recv)(&p->cmd_conn[idx], &cmd, &size, AIS_CONN_RECV_TIMEOUT);
        if (rc > 0)
        {
            continue;
        }
        else if (rc < 0)
        {
            rc = -1;
            break;
        }

        cmd_id = cmd.header.cmd_id;

        rc = ais_server_process_cmd(p, idx, &cmd);

        if (cmd_id != cmd.header.cmd_id)
        {
            AIS_LOG_SRV(AIS_LOG_LVL_CRIT, "%s:%d %d %d", __func__, __LINE__, cmd_id, cmd.header.cmd_id);
        }

        rc = AIS_CONN_API(ais_conn_send)(&p->cmd_conn[idx], &cmd, size);
        if (rc != 0)
        {
            rc = -2;
            break;
        }

        if (cmd.header.cmd_id == AIS_CMD_CLOSE)
        {
            break;
        }
        else if (cmd.header.cmd_id == AIS_CMD_OPEN &&
                 cmd.header.result == CAMERA_SUCCESS)
        {
            if (0 != (rc = ais_server_create_work_conn(p)))
            {
                rc = -3;
                break;
            }
        }

        /* Signal health monitor */
        p->signal_bit = TRUE;
    }

    if (idx == AIS_CONN_CMD_IDX_MAIN)
    {
        ais_server_destroy(p - sgs_ais_client_ctxt, 0);
    }

    AIS_LOG_SRV_HIGH("X %s 0x%p %d", __func__, p, idx);

    return 0;
}

#ifdef CAMERA_UNITTEST
int ais_server_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    int rc = 0;
    int i;
    CameraResult ret;
    s_ais_conn conn;
    char *p_name = NULL;

#if defined(__INTEGRITY)
    // Set priority of main thread
    // Task needs to be linked to an object defined in corresponding INT file
    Task mainTask = TaskObjectNumber(AIS_SERV_OBJ_NUM);
    SetTaskPriority(mainTask, CameraTranslateThreadPriority(AIS_SRV_THRD_PRIO), false);
#endif

    p_name = ais_server_get_name(argv[0]);

    ais_log_init(NULL, p_name);

#ifdef USE_HYP
    ais_log_kpi(AIS_EVENT_KPI_BE_SERVER_INIT);
#else
    ais_log_kpi(AIS_EVENT_KPI_SERVER_INIT);
#endif

    AIS_LOG_SRV_HIGH("AIS SERVER START %x", AIS_VERSION);

    rc = ais_server_singleton_acquire(p_name);
    if (rc != 0)
    {
        rc = -1;
        goto EXIT_FLAG_SINGLETON;
    }

    for (i = 0; i < AIS_CONTEXT_MAX_NUM; i++)
    {
        ais_server_init_client_ctxt(&sgs_ais_client_ctxt[i]);
    }

    AIS_CONN_API(ais_conn_init)(&sgs_ais_server_conn);
    AIS_CONN_API(ais_conn_init)(&conn);

    ais_server_register_signal_handler();

#ifdef ENABLE_DIAGNOSTIC
    ais_diag_initialize();
#endif

    ret = ais_initialize(NULL);
    if (ret != CAMERA_SUCCESS)
    {
        rc = -2;
        goto EXIT_FLAG_INIT;
    }
#ifndef HEALTH_DISABLED
    ret = ais_server_create_health_thread();
    if (ret != CAMERA_SUCCESS)
    {
        rc = -3;
        goto EXIT_FLAG_INIT;
    }
#endif

    /* Write boot KPI marker */
#ifdef USE_HYP
    ais_log_kpi(AIS_EVENT_KPI_BE_SERVER_READY);
#else
    ais_log_kpi(AIS_EVENT_KPI_SERVER_READY);
#endif

    AIS_LOG_SRV_HIGH("%s:%d %d %d %d %d %d %d %d %d %d %d %d %d", __func__, __LINE__,
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

    rc = AIS_CONN_API(ais_conn_open)(&sgs_ais_server_conn, 0, AIS_CONTEXT_MAX_NUM);
    if (rc != 0)
    {
        rc = -4;
        goto EXIT_FLAG;
    }

    while (!sg_abort)
    {
        rc = AIS_CONN_API(ais_conn_accept)(&sgs_ais_server_conn, &conn, AIS_CONN_MODE_LISTEN);
        if (rc != 0)
        {
            break;
        }

        ais_server_create(&conn);
        AIS_CONN_API(ais_conn_close)(&conn);
    }

    AIS_CONN_API(ais_conn_close)(&sgs_ais_server_conn);

EXIT_FLAG:

    ais_uninitialize();

    AIS_LOG_SRV(AIS_LOG_LVL_ALWAYS,"finish ais_uninitialize");

#ifdef ENABLE_DIAGNOSTIC
    ais_diag_uninitialize();
#endif

EXIT_FLAG_INIT:

    ais_server_singleton_release();

EXIT_FLAG_SINGLETON:

    AIS_LOG_SRV(AIS_LOG_LVL_ALWAYS,"AIS SERVER EXIT %d", rc);

    ais_log_uninit();

    return rc;
}
