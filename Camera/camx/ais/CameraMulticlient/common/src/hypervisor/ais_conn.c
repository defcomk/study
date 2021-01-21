/**
 * @file ais_conn.c
 *
 * @brief defines all functions of connection
 *        connection is an abstract layer for socket/Connection/hab on Qnx/Linux/Android/Integrity.
 *        this is an abstraction layer for hab
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#if defined(__LINUX) || defined(__ANDROID__)
#include <sys/mman.h>
#endif

#if defined(__INTEGRITY)
#include "pmem.h"
#include "pmemext.h"
#endif

#include "habmm.h"

#include "ais_log.h"
#include "ais_conn.h"
#include "ais_comm.h"


#define AIS_LOG_CONN(level, fmt...) AIS_LOG(AIS_MOD_ID_CONN_HYPERVISOR, level, fmt)

#define AIS_LOG_CONN_ERR(fmt...) AIS_LOG(AIS_MOD_ID_CONN_HYPERVISOR, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_CONN_WARN(fmt...) AIS_LOG(AIS_MOD_ID_CONN_HYPERVISOR, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_CONN_HIGH(fmt...) AIS_LOG(AIS_MOD_ID_CONN_HYPERVISOR, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CONN_MED(fmt...) AIS_LOG(AIS_MOD_ID_CONN_HYPERVISOR, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_CONN_LOW(fmt...) AIS_LOG(AIS_MOD_ID_CONN_HYPERVISOR, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_CONN_DBG(fmt...) AIS_LOG(AIS_MOD_ID_CONN_HYPERVISOR, AIS_LOG_LVL_DBG, fmt)


/**
 * initializes a connection structure
 * @param p points to a connection structure
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_init)(s_ais_conn *p)
{
    int rc = 0;

    AIS_LOG_CONN_MED("E %s 0x%p", __func__, p);

    if (p == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }
    memset(p, 0, sizeof(s_ais_conn));

    p->status = AIS_CONN_STAT_NONE;

EXIT_FLAG:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * opens a new connection
 * @param p points to a connection structure
 * @param id connection id which is used to identify by client and server
 * @param max_cnt maximum number of sub connections with the specific id
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_open)(s_ais_conn *p, unsigned int id, unsigned int max_cnt)
{
    int rc = 0;

    AIS_LOG_CONN_HIGH("E %s 0x%p %d %d", __func__, p, id, max_cnt);

    if (p == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }
    memset(p, 0, sizeof(s_ais_conn));

    p->id = id;
    p->max_cnt = max_cnt;

    pthread_mutex_init(&p->mutex, NULL);

    p->status = AIS_CONN_STAT_INIT;

EXIT_FLAG:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d %d %d", __func__, p, id, max_cnt, rc);

    return rc;
}

/**
 * closes a connection
 * @param p points to a connection structure
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_close)(s_ais_conn *p)
{
    int rc = 0;

    AIS_LOG_CONN_HIGH("E %s 0x%p", __func__, p);

    if (p == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    //set exit flag for other APIs
    p->abort = 1;

    if (p->status == AIS_CONN_STAT_READY)
    {
        //unmap all buffers for hab before close
        AIS_CONN_API(ais_conn_flush)(p, 0);
        AIS_CONN_API(ais_conn_flush)(p, 0);

        rc = habmm_socket_close(p->handle);
        p->handle = 0;
    }

    if (p->status != AIS_CONN_STAT_NONE)
    {
        struct timespec ts;

        //waiting for other APIs exit
        AIS_CONN_CALC_ABSTIME(ts, CLOCK_REALTIME, AIS_CONN_CLOSE_TIMEOUT);

        pthread_mutex_timedlock(&p->mutex, &ts);
        p->status = AIS_CONN_STAT_NONE;
        pthread_mutex_unlock(&p->mutex);

        pthread_mutex_destroy(&p->mutex);
    }

EXIT_FLAG:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * copys a listening connection to spawn a new connection
 * @param p points to a connection structure
 * @param p points to a spawned connection structure
 * @return none
 */
void AIS_CONN_API(ais_conn_copy)(s_ais_conn *p, s_ais_conn *p_new)
{
    p_new->status = p->status;
    p_new->id = p->id;
    p_new->max_cnt = p->max_cnt;

    pthread_mutex_init(&p_new->mutex, NULL);

    p_new->map_idx = 0;
    p_new->map_num[0] = 0;
    p_new->map_num[1] = 0;

    p_new->abort = 0;

    p_new->handle = p->handle;

    return ;
}

/**
 * listens a connection from clients and spawns a new connection once a client requests.
 * the listening connection is not really created,
 * it is only used to simulate the same behavoir of socket, once a request is made from a client,
 * the spawned connection would get the real connection.
 * @param p points to a connection structure
 * @param p_new points to a spawned connection structure
 * @param mode indicates the mode of the listening connection
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_accept)(s_ais_conn *p, s_ais_conn *p_new, e_ais_conn_mode mode)
{
    int rc = 0;
    int32_t handle = 0;

    AIS_LOG_CONN_MED("E %s 0x%p 0x%p %d", __func__, p, p_new, mode);

    if (p == NULL || p_new == NULL)
    {
        rc = -1;
        goto EXIT_FLAG2;
    }

    rc = pthread_mutex_lock(&p->mutex);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (p->status != AIS_CONN_STAT_INIT)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    while (!p->abort)
    {
        rc = habmm_socket_open(&handle, HAB_MMID_CREATE(MM_CAM_1, p->id),
                (uint32_t)-1, HABMM_SOCKET_OPEN_FLAGS_SINGLE_BE_SINGLE_FE);
        if (rc == -ETIMEDOUT)
        {
            continue;
        }
        else if (rc != 0)
        {
            AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d %d", __func__, __LINE__, p, rc, errno, p->abort);
            rc = -4;
            goto EXIT_FLAG;
        }
        else
        {
            break;
        }
    }

    if (handle != 0)
    {
        //make a new connection
        AIS_CONN_API(ais_conn_copy)(p, p_new);
        p_new->handle = handle;
        p_new->status = AIS_CONN_STAT_READY;
    }
    else
    {
        rc = -5;
        AIS_LOG_CONN_ERR("%s:%d handle NULL, abort: %d", __func__, __LINE__, p->abort);
    }

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d %d", __func__, p, p_new, mode, rc);

    return rc;
}

/**
 * connects to a listening connection
 * @param p points to a connection structure
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_connect)(s_ais_conn *p)
{
    int rc = 0;

    AIS_LOG_CONN_MED("E %s 0x%p", __func__, p);

    if (p == NULL)
    {
        rc = -1;
        goto EXIT_FLAG2;
    }

    rc = pthread_mutex_lock(&p->mutex);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (p->status != AIS_CONN_STAT_INIT)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    rc = habmm_socket_open(&p->handle, HAB_MMID_CREATE(MM_CAM_1, p->id),
            (uint32_t)-1, HABMM_SOCKET_OPEN_FLAGS_SINGLE_BE_SINGLE_FE);
    if (rc != 0)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, rc, errno);
        rc = (rc == -ETIMEDOUT) ? 1 : -4;
        goto EXIT_FLAG;
    }

    p->status = AIS_CONN_STAT_READY;

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * sends data to the peer via connection
 * @param p points to connection structure
 * @param p_data points to a buffer to be sent
 * @param size data size in the specified buffer
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_send)(s_ais_conn *p, void *p_data, unsigned int size)
{
    int rc = 0;

    AIS_LOG_CONN_DBG("E %s 0x%p 0x%p %d", __func__, p, p_data, size);

    if (p == NULL || p_data == NULL || size == 0)
    {
        rc = -1;
        goto EXIT_FLAG2;
    }

    rc = pthread_mutex_lock(&p->mutex);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (p->status != AIS_CONN_STAT_READY)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    rc = habmm_socket_send(p->handle, p_data, size, 0);
    if (rc != 0)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, rc, errno);
        rc = -4;
    }

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d %d", __func__, p, p_data, size, rc);

    return rc;
}

/**
 * receives data from the peer via connection with timeout
 * @param p points to connection structure
 * @param p_data points to a buffer to be sent
 * @param p_size points to the size of the buffer specified by p_data
 * @param timeout timed-out time in millisecond, not supported currently
 * @return 0: success, others: failed
 */
int AIS_CONN_API(ais_conn_recv)(s_ais_conn *p, void *p_data, unsigned int *p_size, unsigned int timeout)
{
    int rc = 0;
    uint32_t temp_size;

    AIS_LOG_CONN_DBG("E %s 0x%p 0x%p 0x%p | %d %d", __func__, p,
                p_data, p_size, (p_size != NULL) ? *p_size : 0, timeout);

    if (p == NULL || p_data == NULL || p_size == NULL || *p_size == 0)
    {
        rc = -1;
        goto EXIT_FLAG2;
    }

    rc = pthread_mutex_lock(&p->mutex);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (p->status != AIS_CONN_STAT_READY)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    do
    {
        temp_size = *p_size;
        rc = habmm_socket_recv(p->handle, p_data, &temp_size, timeout, 0);
        if (rc == -EINTR)
        {
            AIS_LOG_CONN_HIGH("habmm_socket_recv -EINTR ");
        }
    }while (rc == -EINTR && !p->abort);

    if (rc != 0 || p->abort)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d %d", __func__, __LINE__, p, rc, errno, p->abort);
        rc = (rc == -ETIMEDOUT) ? 1 : -4;
        goto EXIT_FLAG;
    }

    *p_size = temp_size;

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p 0x%p | %d %d %d", __func__, p,
                p_data, p_size, (p_size != NULL) ? *p_size : 0, timeout, rc);

    return rc;
}

/**
 * exports the buffer from one process/OS to another
 * On LA/LV which means file descriptor, on QNX/Integrity which means virtual address
 * @param p points to a connection structure
 * @param p_data points to the buffer which needs to be exported
 * @param size the size of the exported buffer
 * @return 0: success, others: failure
 *
 */
int AIS_CONN_API(ais_conn_export)(s_ais_conn *p, void *p_data, unsigned int size)
{
    int rc = 0;

    AIS_LOG_CONN_MED("E %s 0x%p 0x%p %d", __func__, p, p_data, size);

    if (p == NULL || p_data == NULL || size == 0)
    {
        rc = -1;
        goto EXIT_FLAG2;
    }

    rc = pthread_mutex_lock(&p->mutex);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (p->status != AIS_CONN_STAT_READY)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    {
        uint32_t id = 0;
        uintptr_t exp_id = 0;
        void *p_temp = NULL;
        int temp_size = CAM_ALIGN_SIZE(size, 4096);
#if defined(__LINUX) || defined(__ANDROID__)
        int fd = (int)(int *)p_data;

        AIS_LOG_CONN_LOW("%s:%d 0x%x", __func__, __LINE__, fd);

        //convert file descriptor to virtual address
        p_temp = mmap(NULL, temp_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
#else
        p_temp = p_data;
#endif

        AIS_LOG_CONN_LOW("%s:%d %p %d", __func__, __LINE__, p_temp, temp_size);

        rc = habmm_export(p->handle, p_temp, temp_size, &id, HABMM_EXP_MEM_TYPE_DMA);
        if (rc != 0)
        {
            AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, rc, errno);
            rc = -4;
            goto EXIT_FLAG;
        }

        p->map[p->map_idx][p->map_num[p->map_idx]].p_buf = 0;
        p->map[p->map_idx][p->map_num[p->map_idx]].fd = id;
        p->map[p->map_idx][p->map_num[p->map_idx]].size = temp_size;
        p->map_num[p->map_idx]++;
        if (p->map_num[p->map_idx] >= AIS_CONN_BUFFER_MAP_MAX_NUM)
        {
            p->map_num[p->map_idx]--;
            AIS_LOG_CONN_ERR("%s:%d too many export", __func__, __LINE__);
        }

        AIS_LOG_CONN_LOW("%s:%d %d", __func__, __LINE__, id);

        exp_id = id;
        rc = habmm_socket_send(p->handle, &exp_id, sizeof(exp_id), 0);
        if (rc != 0)
        {
            AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, rc, errno);
            rc = -5;
        }
    }

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d %d", __func__, p, p_data, size, rc);

    return rc;
}

/**
 * imports the buffer from other process/OS
 * only qnx/integrity can be HOST
 * On LA/LV which means file descriptor, on QNX/Integrity which means virtual address
 * @param p points to a connection structure
 * @param pp_data points to the buffer which is used to hold the buffer pointer imported
 * @param size the size of the exported buffer
 * @param timeout timed-out time in millisecond, not supported currently
 * @return 0: success, others: failure
 *
 */
int AIS_CONN_API(ais_conn_import)(s_ais_conn *p, void **pp_data, unsigned int size, unsigned int timeout)
{
    int rc = 0;

    AIS_LOG_CONN_MED("E %s 0x%p 0x%p | 0x%p %d %d", __func__, p,
                pp_data, (pp_data != NULL) ? *pp_data : NULL, size, timeout);

    if (p == NULL || pp_data == NULL || *pp_data == NULL || size == 0)
    {
        rc = -1;
        goto EXIT_FLAG2;
    }

    rc = pthread_mutex_lock(&p->mutex);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (p->status != AIS_CONN_STAT_READY)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    {
        uintptr_t exp_id = 0;
        void *p_temp = NULL;
        uint32_t temp_size;

        do
        {
            temp_size = sizeof(exp_id);
            rc = habmm_socket_recv(p->handle, &exp_id, &temp_size, timeout, 0);
            if (rc == -EINTR)
            {
                AIS_LOG_CONN_HIGH("habmm_socket_recv -EINTR ");
            }
        }while (rc == -EINTR && !p->abort);

        if (rc != 0 || p->abort)
        {
            AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d %d", __func__, __LINE__, p, rc, errno, p->abort);
            rc = (rc == -ETIMEDOUT) ? 1 : -4;
            goto EXIT_FLAG;
        }

        AIS_LOG_CONN_LOW("%s:%d %d %d", __func__, __LINE__, (int)exp_id, temp_size);

        temp_size = CAM_ALIGN_SIZE(size, 4096);
#ifdef USE_SMMU_V2
        rc = habmm_import(p->handle, &p_temp, temp_size, (uint32_t)exp_id, HABMM_EXPIMP_FLAGS_FD);
#else
        rc = habmm_import(p->handle, &p_temp, temp_size, (uint32_t)exp_id, 0);
#endif
        if (rc != 0)
        {
            AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, rc, errno);
            rc = -5;
            goto EXIT_FLAG;
        }

        p->map[p->map_idx][p->map_num[p->map_idx]].p_buf = p_temp;
        p->map[p->map_idx][p->map_num[p->map_idx]].fd = exp_id;
        p->map[p->map_idx][p->map_num[p->map_idx]].size = temp_size;
        p->map_num[p->map_idx]++;
        if (p->map_num[p->map_idx] >= AIS_CONN_BUFFER_MAP_MAX_NUM)
        {
            p->map_num[p->map_idx]--;
            AIS_LOG_CONN_ERR("%s:%d too many import", __func__, __LINE__);
        }

        AIS_LOG_CONN_LOW("%s:%d 0x%p %d", __func__, __LINE__, p_temp, temp_size);

        *pp_data = p_temp;
    }

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p | 0x%p %d %d %d", __func__, p,
                pp_data, (pp_data != NULL) ? *pp_data : NULL, size, timeout, rc);

    return rc;
}

/**
 * frees buffer mapping for different oprations results
 * @param p points to a connection structure
 * @param op indicates operation result
 * @return 0: success, others: failure
 *
 */
int AIS_CONN_API(ais_conn_flush)(s_ais_conn *p, int op)
{
    int rc = 0;
    int i;

    AIS_LOG_CONN_MED("E %s 0x%p %d", __func__, p, op);

    if (p == NULL)
    {
        rc = -1;
        goto EXIT_FLAG2;
    }

    rc = pthread_mutex_lock(&p->mutex);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (op == 0)
    {
        p->map_idx = !p->map_idx;
    }

    for (i = 0; i < p->map_num[p->map_idx]; i++)
    {
        /* if there is any mapping on this connection */
        if (p->map[p->map_idx][i].fd > 0)
        {
            /* use p_buf to identify front(export) end or backend(import) end */
            if (p->map[p->map_idx][i].p_buf)
            {
                rc = habmm_unimport(p->handle, p->map[p->map_idx][i].fd, p->map[p->map_idx][i].p_buf, HABMM_EXPIMP_FLAGS_FD);
                if (rc != 0)
                {
                    AIS_LOG_CONN_ERR("%s:%d habmm_unimport failed, 0x%p %d", __func__, __LINE__, p, rc);
                }
                else
                {
                    AIS_LOG_CONN_HIGH("Backend habmm_unimport ok 0x%p %d %d", __func__, p, p->map_idx, i);
                }
            }
            else
            {
                rc = habmm_unexport(p->handle, p->map[p->map_idx][i].fd, HABMM_EXP_MEM_TYPE_DMA);
                if (rc != 0)
                {
                    AIS_LOG_CONN_ERR("%s:%d habmm_unexport failed, 0x%p %d", __func__, __LINE__, p, rc);
                }
                else
                {
                    AIS_LOG_CONN_HIGH("Frontend habmm_unexport ok 0x%p %d %d", __func__, p, p->map_idx, i);
                }
            }
        }
    }
    p->map_num[p->map_idx] = 0;

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d %d", __func__, p, op, rc);

    return rc;
}

