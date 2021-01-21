#ifndef _AIS_EVENT_QUEUE_H_
#define _AIS_EVENT_QUEUE_H_

/**
 * @file ais_event_queue.h
 *
 * @brief defines all functions, structures and definitions of event queue.
 *        it supports priority for different events.
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdbool.h>
#include "CameraQueue.h"
#include "ais_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/** maximum number of queue which can be supported in one event queue */
#define AIS_EVENT_QUEUE_MAX_NUM 10

/**
 * event queue management structures
 */
typedef struct
{
    bool             status; /**< status, indicates if it is initialized, false: non-initialized, true: initialized */

    /** camera queue */
    CameraQueue      queue[AIS_EVENT_QUEUE_MAX_NUM];
    int              num; /**< number of camera queues which are created */

    unsigned int     cnt; /**< event counter, starting from 0 */

    int (*event_2_idx)(qcarcam_event_t event); /**< conversion function from event type to camera queue index */

    pthread_mutex_t mutex; /**< mutex for all queue operations */
    pthread_cond_t cond; /**< signal the other thread to dequeue a new event */

} s_ais_event_queue;


/**
 * converts event to camera queue index which holds events with different priorities
 *
 * @param event one event
 * @return camera queue index
 *
 */
int ais_event_queue_event_2_idx(qcarcam_event_t event);

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
int ais_event_queue_init(s_ais_event_queue *p, int num, int size, int (*event_2_idx)(qcarcam_event_t event));

/**
 * deinitializes an event queue, destroy internal queues and release all resources
 *
 * @param p points to evnet queue structures
 * @return 0: success, others: failed
 *
 */
int ais_event_queue_deinit(s_ais_event_queue *p);

/**
 * signal waiting thread
 *
 * @param p points to evnet queue structures
 * @return 0: success, others: failed
 *
 */
int ais_event_queue_signal(s_ais_event_queue *p);

/**
 * enqueues an event, and signals other thread
 *
 * @param p points to evnet queue structures
 * @param p_event points to an event structure
 * @return 0: success, others: failed
 *
 */
int ais_event_queue_enqueue(s_ais_event_queue *p, s_ais_event *p_event);

/**
 * dequeues an event with timed-out time.
 * if no event is available in timed-out time, it returns.
 *
 * @param p points to evnet queue structures
 * @param p_event points to an event structure
 * @param timeout timed-out time in millisecond
 * @return 0: success, others: failed
 *
 */
int ais_event_queue_dequeue(s_ais_event_queue *p, s_ais_event *p_event, unsigned int timeout);

#ifdef __cplusplus
}
#endif


#endif //_AIS_EVENT_QUEUE_H_
