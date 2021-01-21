/**
 * @file ais_conn.c
 *
 * @brief defines all functions of connection
 *        connection is an abstract layer for socket/Connection/hab on Qnx/Linux/Android/Integrity.
 *        this is an abstraction layer for Connection
 *
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <INTEGRITY.h>
#include "pmem.h"
#include "pmemext.h"


#include "ais_log.h"
#include "ais_conn.h"
#include "ais_comm.h"


#define CONNECTION_SERVER "ais_connection_server"
#define CONNECTION_CLIENT "ais_connection_client"

#define CONNECTION_BF_SERVER (1<<0)
#define CONNECTION_BF_CLIENT (1<<1)

#define CONNECTION_TIMEOUT_CNT 500


#define AIS_LOG_CONN(level, fmt...) AIS_LOG(AIS_MOD_ID_CONN_INTEGRITY, level, fmt)

#define AIS_LOG_CONN_ERR(fmt...) AIS_LOG(AIS_MOD_ID_CONN_INTEGRITY, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_CONN_WARN(fmt...) AIS_LOG(AIS_MOD_ID_CONN_INTEGRITY, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_CONN_HIGH(fmt...) AIS_LOG(AIS_MOD_ID_CONN_INTEGRITY, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CONN_MED(fmt...) AIS_LOG(AIS_MOD_ID_CONN_INTEGRITY, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_CONN_LOW(fmt...) AIS_LOG(AIS_MOD_ID_CONN_INTEGRITY, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_CONN_DBG(fmt...) AIS_LOG(AIS_MOD_ID_CONN_INTEGRITY, AIS_LOG_LVL_DBG, fmt)


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
int ais_conn_open(s_ais_conn *p, unsigned int id, unsigned int max_cnt)
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
 * closes sub connections for client/server
 * @param p points to a connection structure
 * @return 0: success, others: failed
 */
int ais_conn_close(s_ais_conn *p)
{
    int rc = 0;
    Error ret;
    Value abort_num;
    char name[32];
    int cnt = 0;

    AIS_LOG_CONN_HIGH("E %s 0x%p", __func__, p);

    if (p == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    p->abort = 1;

    if ((p->flag & CONNECTION_BF_SERVER) != 0 && p->ref_cnt == 0)
    {
        ret = AbortConnectionSends(p->server, &abort_num);
        ret = AbortObjectReceives((Object)p->server, &abort_num);

        snprintf(name, sizeof(name), CONNECTION_CLIENT"_%d", p->id);
        while ((ret = RequestResource((Object*)&p->client, name, NULL)) != Success)
        {
            if (cnt++ >= CONNECTION_TIMEOUT_CNT)
            {
                //2 seconds timeout, handle the case of server is not listening
                rc = -2;
                break;
            }
            usleep(10000);
        }

        if (ret == Success)
        {
            ret = AbortConnectionSends(p->client, &abort_num);
            ret = AbortObjectReceives((Object)p->client, &abort_num);

            ret = CloseConnection(p->client);
            DeleteResource(name, NULL);
        }
        ret = CloseConnection(p->server);
    }

    if ((p->flag & CONNECTION_BF_CLIENT) != 0 && p->ref_cnt == 0)
    {
        //cannot use abort for clients.
        //ret = AbortConnectionSends(p->client, &abort_num);
        //ret = AbortObjectReceives(p->client, &abort_num);

        snprintf(name, sizeof(name), CONNECTION_CLIENT"_%d", p->id);
    }
    p->flag = 0;

    if (p->status != AIS_CONN_STAT_NONE)
    {
        struct timespec ts;

        //waiting for other APIs exit
        AIS_CONN_CALC_ABSTIME(ts, CLOCK_REALTIME, AIS_CONN_CLOSE_TIMEOUT);

        pthread_mutex_timedlock(&p->mutex, &ts);
        p->status = AIS_CONN_STAT_NONE;
        pthread_mutex_unlock(&p->mutex);

        //unmap buffers
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
 * indicates which connection should be closed in different mode
 * @param p points to a connection structure
 * @param p points to a spawned connection structure
 * @return none
 */
void ais_conn_copy(s_ais_conn *p, s_ais_conn *p_new, e_ais_conn_mode mode)
{
    p_new->status = p->status;
    p_new->id = p->id;
    p_new->max_cnt = p->max_cnt;

    pthread_mutex_init(&p_new->mutex, NULL);

    p_new->map_idx = 0;
    p_new->map_num[0] = 0;
    p_new->map_num[1] = 0;

    p_new->abort = 0;

    p_new->server = p->server;
    p_new->client = p->client;

    p_new->flag = p->flag;

    if (mode == AIS_CONN_MODE_LISTEN)
    {
        p->ref_cnt = 0;
        p_new->ref_cnt = 1;
    }
    else if (mode == AIS_CONN_MODE_WORK)
    {
        p->ref_cnt = 1;
        p_new->ref_cnt = 0;
    }

    return ;
}

/**
 * listens a connection from clients and spawns a new connection once a client requests.
 * in LISTEN mode, the listening connection is kept, and a new connections is spawned
 * in WORK mode, the listening connection is copied as the new connection
 * @param p points to a connection structure
 * @param p_new points to a spawned connection structure
 * @param mode indicates the mode of the listening connection
 * @return 0: success, others: failed
 */
int ais_conn_accept(s_ais_conn *p, s_ais_conn *p_new, e_ais_conn_mode mode)
{
    int rc = 0;
    char name[32];

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

    if (p->flag == 0)
    {
        rc = (int)CreateConnection(&p->client, &p->server);

        snprintf(name, sizeof(name), CONNECTION_CLIENT"_%d", p->id);
        rc = (int)RegisterLinkableResource((Object)p->client, name, NULL);

        p->flag = CONNECTION_BF_SERVER;
    }

    //make a new connection
    ais_conn_copy(p, p_new, mode);
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
    char name[32];

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

    snprintf(name, sizeof(name), CONNECTION_CLIENT"_%d", p->id);

    while ((rc = (int)RequestResource((Object*)&p->client, name, NULL)) != 0)
    {
        if (p->abort)
        {
            rc = -4;
            goto EXIT_FLAG;
        }
        else if (cnt++ >= CONNECTION_TIMEOUT_CNT)
        {
            //2 seconds timeout, handle the case of server is not listening
            rc = -5;
            goto EXIT_FLAG;
        }
        usleep(10000);
    }
    p->flag = CONNECTION_BF_CLIENT;
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
int ais_conn_send(s_ais_conn *p, void *p_data, unsigned int size)
{
    int rc = 0;
    Error ret;
    Buffer buffer;

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

    memset(&buffer, 0, sizeof(buffer));
    buffer.BufferType =  DataBuffer | LastBuffer;
    buffer.TheAddress = (Address)p_data;
    buffer.Length = size;
    buffer.Transferred = 0;

    if ((p->flag & CONNECTION_BF_SERVER) != 0)
    {
        ret = SynchronousSend(p->server, &buffer);
    }
    else
    {
        ret = SynchronousSend(p->client, &buffer);
    }

    if (ret != Success)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d", __func__, __LINE__, p, ret);
        rc = (ret == ObjectIsUseless) ? 1 : -4;
        goto EXIT_FLAG;
    }
    rc = 0;

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(AIS_LOG_LVL_DBG,
                "X %s 0x%p 0x%p %d %d", __func__, p, p_data, size, rc);

    return rc;
}

/**
 * receives data from the peer via connection with timeout
 * @param p points to connection structure
 * @param p_data points to a buffer to be sent
 * @param p_size points to the size of the buffer specified by p_data
 * @param timeout timed-out time in millisecond
 * @return 0: success, others: failed
 */
int ais_conn_recv(s_ais_conn *p, void *p_data, unsigned int *p_size, unsigned int timeout)
{
    int rc = 0;
    Error ret;
    Buffer buffer;

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

    memset(&buffer, 0, sizeof(buffer));
    buffer.BufferType =  DataBuffer | LastBuffer;
    buffer.TheAddress = (Address)p_data;
    buffer.Length = *p_size;
    buffer.Transferred = 0;

    if ((p->flag & CONNECTION_BF_SERVER) != 0)
    {
        ret = SynchronousReceive(p->server, &buffer);
    }
    else
    {
        ret = SynchronousReceive(p->client, &buffer);
    }

    if (ret != Success)
    {
        rc = (ret == ObjectIsUseless) ? 1 : -4;
        goto EXIT_FLAG;
    }

    *p_size = buffer.Transferred;
    rc = 0;

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(AIS_LOG_LVL_DBG,
                "X %s 0x%p 0x%p 0x%p | %d %d %d", __func__, p,
                p_data, p_size, (p_size != NULL) ? *p_size : 0, timeout, rc);

    return rc;
}


/**
 * exports the buffer from one process/OS to another
 * uses protable id to share buffers between process
 * @param p points to a connection structure
 * @param p_data points to the buffer which needs to be exported
 * @param size the size of the exported buffer
 * @return 0: success, others: failure
 *
 */
int ais_conn_export(s_ais_conn *p, void *p_data, unsigned int size)
{
    int rc = 0;
    Error ret;
    Buffer buffer;

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

    memset(&buffer, 0, sizeof(buffer));
    buffer.BufferType =  DataBuffer | LastBuffer;
    buffer.TheAddress = (Address)&p_data;
    buffer.Length = sizeof(void*);
    buffer.Transferred = 0;

    if ((p->flag & CONNECTION_BF_SERVER) != 0)
    {
        ret = SynchronousSend(p->server, &buffer);
    }
    else
    {
        ret = SynchronousSend(p->client, &buffer);
    }

    if (ret != Success)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d", __func__, __LINE__, p, ret);
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
 * uses protable id to share buffers between process
 * @param p points to a connection structure
 * @param pp_data points to the buffer which is used to hold the buffer pointer imported
 * @param size the size of the exported buffer
 * @param timeout timed-out time in millisecond
 * @return 0: success, others: failure
 *
 */
int ais_conn_import(s_ais_conn *p, void **pp_data, unsigned int size, unsigned int timeout)
{
    int rc = 0;
    Error ret;
    Buffer buffer;
    void* mem_handle = 0;

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
    buffer.BufferType =  DataBuffer | LastBuffer;
    buffer.TheAddress = (Address)&mem_handle;
    buffer.Length = sizeof(void*);
    buffer.Transferred = 0;

    if ((p->flag & CONNECTION_BF_SERVER) != 0)
    {
        ret = SynchronousReceive(p->server, &buffer);
    }
    else
    {
        ret = SynchronousReceive(p->client, &buffer);
    }

    if (ret != Success)
    {
        AIS_LOG_CONN_ERR("%s:%d 0x%p %d", __func__, __LINE__, p, ret);
        rc = (ret == ObjectIsUseless) ? 1 : -4;
        goto EXIT_FLAG;
    }
    rc = 0;

    // Pmem handle from client
    *pp_data = mem_handle;

    p->map[p->map_idx][p->map_num[p->map_idx]].p_buf = *pp_data;
    p->map[p->map_idx][p->map_num[p->map_idx]].size = size;
    p->map_num[p->map_idx]++;
    if (p->map_num[p->map_idx] >= AIS_CONN_BUFFER_MAP_MAX_NUM)
    {
        p->map_num[p->map_idx]--;
        AIS_LOG_CONN_ERR("%s:%d too many import", __func__, __LINE__);
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
        //unmap here
    }
    p->map_num[p->map_idx] = 0;

EXIT_FLAG:

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG2:

    AIS_LOG_CONN(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
                "X %s 0x%p %d %d", __func__, p, op, rc);

    return rc;
}

