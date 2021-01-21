/**
 * @file ais_conn.c
 *
 * @brief defines all functions of connection
 *        connection is an abstract layer for socket/Connection/hab on Qnx/Linux/Android/Integrity.
 *        this is an abstraction layer for QNX socket
 *
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "pmemext.h"

#include "ais_log.h"
#include "ais_conn.h"
#include "ais_comm.h"


#define AIS_LOG_CONN(level, fmt...) AIS_LOG(AIS_MOD_ID_CONN_QNX, level, fmt)

#define AIS_LOG_CONN_ERR(fmt...) AIS_LOG(AIS_MOD_ID_CONN_QNX, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_CONN_WARN(fmt...) AIS_LOG(AIS_MOD_ID_CONN_QNX, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_CONN_HIGH(fmt...) AIS_LOG(AIS_MOD_ID_CONN_QNX, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CONN_MED(fmt...) AIS_LOG(AIS_MOD_ID_CONN_QNX, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_CONN_LOW(fmt...) AIS_LOG(AIS_MOD_ID_CONN_QNX, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_CONN_DBG(fmt...) AIS_LOG(AIS_MOD_ID_CONN_QNX, AIS_LOG_LVL_DBG, fmt)

#define AIS_CONNECT_RETRY_TIMEOUT  1000

/**
 * initializes a connection structure
 * @param p points to a connection structure
 * @return 0: success, others: failed
 */
int ais_conn_init(s_ais_conn *p)
{
    int rc = 0;

    AIS_LOG_CONN_MED("E %s 0x%p", __func__, p);

    if (p == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }
    memset(p, 0, sizeof(s_ais_conn));

    p->fd = -1;

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
int ais_conn_open(s_ais_conn *p, unsigned id, unsigned int max_cnt)
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

    p->fd = -1;

    pthread_mutex_init(&p->mutex, NULL);

    p->status = AIS_CONN_STAT_INIT;

EXIT_FLAG:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d %d %d", __func__, p, id, max_cnt, rc);

    return rc;
}

/**
 * closes a connection
 * frees all mapped buffers and close the socket handle
 * @param p points to a connection structure
 * @return 0: success, others: failed
 */
int ais_conn_close(s_ais_conn *p)
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

    if (p->fd >= 0)
    {
        //shutdown and close socket, so other APIs would break and exit.
        rc = shutdown(p->fd, SHUT_RDWR);
        rc = close(p->fd);
        p->fd = -1;
    }

    if (p->status != AIS_CONN_STAT_NONE)
    {
        struct timespec ts;

        //waiting for other APIs exit
        AIS_CONN_CALC_ABSTIME(ts, CLOCK_REALTIME, AIS_CONN_CLOSE_TIMEOUT);

        pthread_mutex_timedlock(&p->mutex, &ts);
        p->status = AIS_CONN_STAT_NONE;
        pthread_mutex_unlock(&p->mutex);

        //free all pmem mappings
        ais_conn_flush(p, 0);
        ais_conn_flush(p, 0);

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
void ais_conn_copy(s_ais_conn *p, s_ais_conn *p_new)
{
    p_new->status = p->status;
    p_new->id = p->id;
    p_new->max_cnt = p->max_cnt;

    pthread_mutex_init(&p_new->mutex, NULL);

    p_new->map_idx = 0;
    p_new->map_num[0] = 0;
    p_new->map_num[1] = 0;

    p_new->abort = 0;

    return ;
}

/**
 * listens a connection from clients and spawns a new connection once a client requests.
 * @param p points to a connection structure
 * @param p_new points to a spawned connection structure
 * @param mode indicates the mode of the listening connection
 * @return 0: success, others: failed
 */
int ais_conn_accept(s_ais_conn *p, s_ais_conn *p_new, e_ais_conn_mode mode)
{
    int rc = 0;
    int fd;
    struct sockaddr_un addr;
    socklen_t len;

    AIS_LOG_CONN_MED("E %s 0x%p 0x%p %d", __func__, p, p_new, mode);

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

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), AIS_TEMP_PATH "/ais_socket_%d", p->id);

    if (p->fd < 0)
    {
        unlink(addr.sun_path);

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
        {
            AIS_LOG_CONN_ERR("%s:%d 0x%p %d", __func__, __LINE__, p, errno);
            rc = -4;
            goto EXIT_FLAG;
        }

        rc = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
        if (rc < 0)
        {
            AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, rc, errno);
            rc = -5;
            goto EXIT_FLAG;
        }

        listen(fd, p->max_cnt);

        p->fd = fd;
    }

    len = sizeof(addr);
    fd = accept(p->fd, (struct sockaddr*)&addr, &len);
    if (fd == -1 || p->abort)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, errno, p->abort);
        rc = -6;
        goto EXIT_FLAG;
    }
    rc = 0;

    ais_conn_copy(p, p_new);
    p_new->fd = fd;
    p_new->status = AIS_CONN_STAT_READY;

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
int ais_conn_connect(s_ais_conn *p)
{
    int rc = 0;
    int cnt = 0;
    int fd;
    struct sockaddr_un addr;

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

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d", __func__, __LINE__, p, errno);
        rc = -4;
        goto EXIT_FLAG;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), AIS_TEMP_PATH "/ais_socket_%d", p->id);

    while (!p->abort)
    {
        rc = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
        if (rc == 0)
        {
            break;
        }
        else if (errno == ENOENT || errno == ECONNREFUSED)
        {
            //2 seconds timeout, handle the case of server is not listening
            if (cnt++ >= 2000)
            {
                rc = -5;
                break;
            }
            usleep(AIS_CONNECT_RETRY_TIMEOUT);
        }
        else
        {
            rc = -6;
            break;
        }
    }
    if (rc != 0)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, rc, errno);
        goto EXIT_FLAG;
    }

    p->fd = fd;

    p->status = AIS_CONN_STAT_READY;

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d", __func__, p, rc);

    return rc;
}

/**
 * sends data to the peer via connection.
 * SOCKET WOULD MERGE MULTIPLE MESSAGES INTO ONE FIRST AND THEN SEND OUT, WHICH CAUSE ISSUES
 * @param p points to connection structure
 * @param p_data points to a buffer to be sent
 * @param size data size in the specified buffer
 * @return 0: success, others: failed
 */
int ais_conn_send(s_ais_conn *p, void *p_data, unsigned int size)
{
    int rc = 0;

    struct msghdr msg;
    struct iovec iov;

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

    memset(&msg, 0, sizeof(msg));

    iov.iov_base = p_data;
    iov.iov_len = size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = 0;
    msg.msg_controllen = 0;

    rc = sendmsg(p->fd, &msg, 0);
    if (rc != size)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, rc, errno);
        rc = -4;
        goto EXIT_FLAG;
    }
    rc = 0;

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
int ais_conn_recv(s_ais_conn *p, void *p_data, unsigned int *p_size, unsigned int timeout)
{
    int rc = 0;

    struct msghdr msg;
    struct iovec iov;

    AIS_LOG_CONN_DBG("E %s 0x%p 0x%p 0x%p | %d", __func__, p,
                p_data, p_size, (p_size != NULL) ? *p_size : 0);

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

    memset(&msg, 0, sizeof(msg));

    iov.iov_base = p_data;
    iov.iov_len = *p_size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = 0;
    msg.msg_controllen = 0;

    rc = recvmsg(p->fd, &msg, 0);
    if (rc <= 0 || p->abort)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d %d", __func__, __LINE__, p, rc, errno, p->abort);
        rc = -4;
        goto EXIT_FLAG;
    }

    *p_size = rc;
    rc = 0;

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_DBG : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p 0x%p | %d %d", __func__, p,
                p_data, p_size, (p_size != NULL) ? *p_size : 0, rc);

    return rc;
}


typedef struct
{
    void *p_buf;
    int pid;
} s_ais_conn_buffer;


/**
 * exports the buffer from one process/OS to another
 *
 * @param p points to a connection structure
 * @param p_data points to the buffer which needs to be exported
 * @param size the size of the exported buffer
 * @return 0: success, others: failure
 *
 */
int ais_conn_export(s_ais_conn *p, void *p_data, unsigned int size)
{
    int rc = 0;
    struct msghdr msg;
    struct iovec iov;
    s_ais_conn_buffer buffer;

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

    buffer.p_buf = p_data;
    buffer.pid = getpid();

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = &buffer;
    iov.iov_len = sizeof(buffer);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = 0;
    msg.msg_controllen = 0;

    rc = sendmsg(p->fd, &msg, 0);
    if (rc != sizeof(buffer))
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d", __func__, __LINE__, p, rc, errno);
        rc = -4;
        goto EXIT_FLAG;
    }
    rc = 0;

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p 0x%p %d %d", __func__, p, p_data, size, rc);

    return rc;
}

/**
 * imports the buffer from another process/OS
 * maps buffers from other process with pid info to current process
 * @param p points to a connection structure
 * @param pp_data points to the buffer which is used to hold the buffer pointer imported
 * @param size the size of the exported buffer
 * @param timeout timed-out time in millisecond, not supported currently
 * @return 0: success, others: failure
 *
 */
int ais_conn_import(s_ais_conn *p, void **pp_data, unsigned int size, unsigned int timeout)
{
    int rc = 0;
    struct msghdr msg;
    struct iovec iov;
    s_ais_conn_buffer buffer;

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

    memset(&buffer, 0, sizeof(buffer));

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = &buffer;
    iov.iov_len = sizeof(buffer);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = 0;
    msg.msg_controllen = 0;

    rc = recvmsg(p->fd, &msg, 0);
    if (rc != sizeof(buffer) || p->abort)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d %d %d", __func__, __LINE__, p, rc, errno, p->abort);
        rc = -4;
        goto EXIT_FLAG;
    }
    rc = 0;

#ifdef USE_SMMU_V2
    // Pmem handle from client
    *pp_data = buffer.p_buf;
#else //SMMU V1
    //map buffer from address space of other process
    *pp_data = pmem_map_add_from_pid(buffer.p_buf, size, buffer.pid);
#endif

    rc = (*pp_data != NULL) ? 0 : -5;

    if (rc == 0)
    {
        p->map[p->map_idx][p->map_num[p->map_idx]].p_buf = *pp_data;
        p->map[p->map_idx][p->map_num[p->map_idx]].size = size;
        p->map_num[p->map_idx]++;
        if (p->map_num[p->map_idx] >= AIS_CONN_BUFFER_MAP_MAX_NUM)
        {
            p->map_num[p->map_idx]--;
            AIS_LOG_CONN_ERR("%s:%d too many import", __func__, __LINE__);
        }
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
int ais_conn_flush(s_ais_conn *p, int op)
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
        if (p->map[p->map_idx][i].p_buf != NULL)
        {
            pmem_map_free(p->map[p->map_idx][i].p_buf);
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

