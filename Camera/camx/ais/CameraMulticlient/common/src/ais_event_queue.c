/**
 * @file ais_event_queue.c
 *
 * @brief implements event queue with priorities.
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "ais_event_queue.h"
#include "ais_log.h"

#define AIS_LOG_EQ(level, fmt...) AIS_LOG(AIS_MOD_ID_EVENT_QUEUE, level, fmt)

#define AIS_LOG_EQ_ERR(fmt...) AIS_LOG(AIS_MOD_ID_EVENT_QUEUE, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_EQ_WARN(fmt...) AIS_LOG(AIS_MOD_ID_EVENT_QUEUE, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_EQ_HIGH(fmt...) AIS_LOG(AIS_MOD_ID_EVENT_QUEUE, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_EQ_MED(fmt...) AIS_LOG(AIS_MOD_ID_EVENT_QUEUE, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_EQ_LOW(fmt...) AIS_LOG(AIS_MOD_ID_EVENT_QUEUE, AIS_LOG_LVL_LOW, fmt)


/**
 * converts event to camera queue index which holds events with different priorities
 *
 * @param event one event
 * @return camera queue index
 *
 */
int ais_event_queue_event_2_idx(qcarcam_event_t event)
{
    switch (event)
    {
    case QCARCAM_EVENT_INPUT_SIGNAL:
    case QCARCAM_EVENT_ERROR:
        return 0;
    case QCARCAM_EVENT_FRAME_READY:
    default:
        return 1;
    }
}

/**
 * pushes one event structures into the specific queue
 *
 * @param p points to evnet queue structures
 * @param p_event points to an event structure
 * @return 0: success, others: failed
 *
 */
static int ais_event_queue_push(s_ais_event_queue *p, s_ais_event *p_event)
{
    int rc = 0;
    CameraResult ret;
    boolean is_full = FALSE;
    int idx = 0;

    AIS_LOG_EQ_LOW("E %s 0x%p 0x%p", __func__, p, p_event);

    if (p->event_2_idx != NULL)
    {
        idx = p->event_2_idx(p_event->event_id);
        if (idx >= p->num)
        {
            idx = 0;
        }
    }

    ret = CameraQueueIsFull(p->queue[idx], &is_full);
    if (ret != CAMERA_SUCCESS)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    if (is_full)
    {
        CameraQueueDropHead(p->queue[idx]);
    }

    p_event->event_cnt = p->cnt;

    ret = CameraQueueEnqueue(p->queue[idx], p_event);
    if (rc != CAMERA_SUCCESS)
    {
        rc = -2;
        goto EXIT_FLAG;
    }
    else
    {
        p->cnt++;
    }

EXIT_FLAG:

    AIS_LOG_EQ(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_ERR,
            "x %s 0x%p 0x%p %d", __func__, p, p_event, rc);

    return rc;
}

/**
 * pops out one event structures from the specific queue
 *
 * @param p points to evnet queue structures
 * @param p_event points to an event structure
 * @return 0: success, others: failed
 *
 */
static int ais_event_queue_pop(s_ais_event_queue *p, s_ais_event *p_event)
{
    int rc = -1;
    int i;

    AIS_LOG_EQ_LOW("E %s 0x%p 0x%p", __func__, p, p_event);

    for (i = 0; i < p->num; i++)
    {
        CameraResult ret = CameraQueueDequeue(p->queue[i], p_event);
        if (ret == CAMERA_SUCCESS)
        {
            rc = 0;
            break;
        }
    }

    AIS_LOG_EQ(rc == 0 ? AIS_LOG_LVL_LOW : AIS_LOG_LVL_MED,
            "X %s 0x%p 0x%p %d", __func__, p, p_event, rc);

    return rc;
}

/**
 * initializes an event queue, and creates internal queues with specific size and priorites
 *
 * @param p points to evnet queue structures
 * @param num the number of internal queues which can hold events with different priorities
 * @param size the size of one internal queue
 * @param event_2_idx functions pointer which defines the priorities for different events
 * @return 0: success, others: failed
 *
 */
int ais_event_queue_init(s_ais_event_queue *p, int num, int size, int (*event_2_idx)(qcarcam_event_t event))
{
    int rc = -1;
    int i = 0;
#ifndef __INTEGRITY
    pthread_condattr_t cattr;
#endif
    CameraQueueCreateParamType param;

    AIS_LOG_EQ_HIGH("E %s 0x%p %d %d %p", __func__, p, num, size, event_2_idx);

    if (p == NULL || num <= 0 || num > AIS_EVENT_QUEUE_MAX_NUM || size <= 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    if (p->status)
    {
        rc = 0;
        goto EXIT_FLAG;
    }

    memset(p, 0, sizeof(s_ais_event_queue));
    p->num = num;
    p->event_2_idx = event_2_idx;

    memset(&param, 0, sizeof(param));
    param.nCapacity = size;
    param.nDataSizeInBytes = sizeof(s_ais_event);
    param.eLockType = CAMERAQUEUE_LOCK_NONE;

    for (i = 0; i < p->num; i++)
    {
        CameraResult ret = CameraQueueCreate(p->queue + i, &param);
        if (ret != CAMERA_SUCCESS)
        {
            rc = -2;
            goto EXIT_FLAG;
        }
    }

#ifndef __INTEGRITY
    rc = pthread_condattr_init(&cattr);
    if (rc != 0)
    {
        rc = -5;
        goto EXIT_FLAG;
    }

    rc = pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
    if (rc != 0)
    {
        rc = -6;
        goto EXIT_FLAG;
    }

    rc = pthread_cond_init(&p->cond, &cattr);
#else
    rc = pthread_cond_init(&p->cond, NULL);
#endif
    if (rc != 0)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    rc = pthread_mutex_init(&p->mutex, NULL);
    if (rc != 0)
    {
        pthread_cond_destroy(&p->cond);
        rc = -4;
    }

    p->status = true;

EXIT_FLAG:

    if (rc != 0 && p != NULL)
    {
        for (i--; i >= 0; i--)
        {
            CameraQueueDestroy(p->queue[i]);
        }
    }

    AIS_LOG_EQ(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
            "X %s 0x%p %d %d %p %d", __func__, p, num, size, event_2_idx, rc);

    return rc;
}

/**
 * deinitializes an event queue, destroy internal queues and release all resources
 *
 * @param p points to evnet queue structures
 * @return 0: success, others: failed
 *
 */
int ais_event_queue_deinit(s_ais_event_queue *p)
{
	int rc = 0;
	int i;

    AIS_LOG_EQ_HIGH("E %s 0x%p", __func__, p);

    if (p == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    if (!p->status)
    {
        rc = 0;
        goto EXIT_FLAG;
    }

	for (i = 0; i < p->num; i++)
	{
		rc = CameraQueueDestroy(p->queue[i]);
	}

    pthread_cond_destroy(&p->cond);
    pthread_mutex_destroy(&p->mutex);

    p->status = false;

EXIT_FLAG:

    AIS_LOG_EQ(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
            "X %s 0x%p %d", __func__, p, rc);

	return rc;
}

/**
 * signal waiting thread
 *
 * @param p points to evnet queue structures
 * @return 0: success, others: failed
 *
 */
int ais_event_queue_signal(s_ais_event_queue *p)
{
	int rc = 0;

    AIS_LOG_EQ_HIGH("E %s 0x%p\n", __func__, p);

    if (p == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    if (!p->status)
    {
        rc = 0;
        goto EXIT_FLAG;
    }

    rc = pthread_cond_signal(&p->cond);

EXIT_FLAG:

    AIS_LOG_EQ(rc == 0 ? AIS_LOG_LVL_HIGH : AIS_LOG_LVL_ERR,
            "X %s 0x%p %d", __func__, p, rc);

	return rc;
}

/**
 * enqueues an event, and signals other thread
 *
 * @param p points to evnet queue structures
 * @param p_event points to an event structure
 * @return 0: success, others: failed
 *
 */
int ais_event_queue_enqueue(s_ais_event_queue *p, s_ais_event *p_event)
{
    int rc;

    AIS_LOG_EQ_MED("E %s 0x%p 0x%p", __func__, p, p_event);

    if (p == NULL || p_event == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    rc = pthread_mutex_lock(&p->mutex);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    rc = ais_event_queue_push(p, p_event);

    pthread_mutex_unlock(&p->mutex);

    if (rc == 0)
    {
        //signal the other pthread to get the event
        pthread_cond_signal(&p->cond);
    }

EXIT_FLAG:

    AIS_LOG_EQ(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_ERR,
            "X %s 0x%p 0x%p %d", __func__, p, p_event, rc);

    return rc;
}

/**
 * dequeues an event with timed-out time.
 * if no event is available in timed-out time, it returns.
 *
 * @param p points to event queue structures
 * @param p_event points to an event structure
 * @param timeout timed-out time in millisecond
 * @return 0: success, others: failed
 *
 */
int ais_event_queue_dequeue(s_ais_event_queue *p, s_ais_event *p_event, unsigned int timeout)
{
    int rc;
    struct timespec ts;

    AIS_LOG_EQ_MED("E %s 0x%p 0x%p %d", __func__, p, p_event, timeout);

    if (p == NULL || p_event == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    rc = pthread_mutex_lock(&p->mutex);
    if (rc != 0)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    //try to get a new event
    rc = ais_event_queue_pop(p, p_event);
    if (rc == 0)
    {
        pthread_mutex_unlock(&p->mutex);
        goto EXIT_FLAG;
    }

    memset(&ts, 0, sizeof(ts));

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += (timeout / 1000);
    ts.tv_nsec += (timeout % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000)
    {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec++;
    }

    //get one signal for a new event
    rc = pthread_cond_timedwait(&p->cond, &p->mutex, &ts);
    if (rc == 0)
    {
        rc = ais_event_queue_pop(p, p_event);
    }
    else
    {
        rc = -3;
    }

    pthread_mutex_unlock(&p->mutex);

EXIT_FLAG:

    AIS_LOG_EQ(rc == 0 ? AIS_LOG_LVL_MED : AIS_LOG_LVL_MED,
            "X %s 0x%p 0x%p %d %d", __func__, p, p_event, timeout, rc);

    return rc;
}

