#ifndef _AIS_I_H_
#define _AIS_I_H_

/* ===========================================================================
 * Copyright (c) 2016, 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * @file ais.h
 * @brief AIS internal header
 *
=========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif
#include <string.h>
#include "AEEstd.h"
#include "CameraResult.h"
#include "CameraCommonTypes.h"
#include "CameraOSServices.h"
#include "CameraQueue.h"

#include "chi.h"

#define AIS_MAX_WM_PER_OUTPUT 3
#define AIS_USER_PATHS_MAX 4
#define AIS_BUFFER_MAX_HOPS 4

#define AIS_BUFFER_LIST_USER_START 0
#define AIS_BUFFER_LIST_MAX_USER 2

#define AIS_BUFFER_LIST_INTERNAL_START AIS_BUFFER_LIST_USER_START+AIS_BUFFER_LIST_MAX_USER
#define AIS_BUFFER_LIST_MAX_INTERNAL 2

#define AIS_BUFFER_LIST_MAX AIS_BUFFER_LIST_MAX_USER + AIS_BUFFER_LIST_MAX_INTERNAL

#define AIS_FIELD_BUFFER_SIZE 6

/*reserve indexes for user input/output */
#define AIS_USER_OUTPUT_IDX  AIS_BUFFER_LIST_USER_START

/**
 * Buffer state for tracking
 * the location of the buffer.
 */
typedef enum
{
    AIS_BUFFER_UNITIALIZED = 0, /**< Buffer entry is uninitialized */
    AIS_BUFFER_INITIALIZED,     /**< Buffer is initialized but not queued */
    AIS_IFE_OUTPUT_QUEUE,       /**< Buffer enqueued to IFE output queue  */
    AIS_IFE_INPUT_QUEUE,        /**< Buffer enqueued to IFE input queue */
    AIS_ISP_SCHEDULER,          /**< Buffer enqueued to isp scheduler */
    AIS_EXTERNAL_PP_QUEUE,      /**< Buffer is being processed by external library*/
    AIS_DELIVERY_QUEUE,         /**< Buffer is on queue waiting for user acquire */
    AIS_USER_ACQUIRED,          /**< Buffer acquired by the user */
} ais_buffer_state_t;

/**
 * This structure describes the IFE resources
 * used for a specific user IFE path.
 * A path is defined as:
 *   IFE core 0/1
 *   IFE interface (camif/rdi0/rdi1/rdi2/bus read)
 *   IFE output path (raw/viewfinder/encoder)
 *   IFE writemasters - writemasters used per output
 */
typedef struct
{
    IfeCoreType ife_core;        /**<IFE core */
    IfeInterfaceType ife_interf; /**< IFE interface */
    IfeOutputPathType    ife_output; /**< IFE output path */
    uint32 buffer_list_idx; /**< idx of buffer list associated with this path */
} ais_ife_user_path_info;


/**
 * AIS IFE buffer information needed
 */
typedef struct
{
    uint32 index;   /**< index of buffer */

    qcarcam_buffer_t qcarmcam_buf;

    void* p_va;         /**< virtual addr */
    SMMU_ADDR_T p_da;   /**< mapped device addr */
    uint32 size;        /**< overall size */
    ais_buffer_state_t buffer_state; /**< current buffer state*/

    CHISTREAMBUFFER*   chi_stream_buffer;
}ais_config_ife_buffer_t;


/**
 * Buffer list for inputs/outputs
 * associated with a user context.
 */
class AisBufferList
{
public:
    AisBufferList() {
        m_inUse = 0;
        m_colorFmt = (qcarcam_color_fmt_t)0;
        m_nBuffers = 0;
        memset(m_pBuffers, 0x0, sizeof(m_pBuffers));
    };

    inline boolean IsFree() {
        return !m_inUse;
    }

    inline boolean IsUsed() {
        return m_inUse;
    }

    /**
     * GetBufferState
     *
     * @brief Get state of a buffer in buffer list
     *
     * @param p_buffer_list
     * @param idx
     *
     * @return ais_buffer_state_t
     */
    ais_buffer_state_t GetBufferState(uint32 idx);

    /**
     * SetBufferState
     *
     * @brief Set state of a buffer in a buffer list
     *
     * @param p_buffer_list
     * @param idx
     * @param ais_buffer_state_t
     *
     * @return CameraResult
     */
    CameraResult SetBufferState(uint32 idx, ais_buffer_state_t state);

    boolean m_inUse; /**< specifies if the buffer list is in use or free */

    qcarcam_color_fmt_t m_colorFmt;
    uint32 m_nBuffers;                                           /**< number of buffers set */
    ais_config_ife_buffer_t m_pBuffers[QCARCAM_MAX_NUM_BUFFERS]; /**< array of buffer information needed for ife */
};



/**
 * AIS Internal Events
 */
typedef enum
{
    AIS_EVENT_FRAME_DONE = 0,
    AIS_EVENT_CSI_WARNING,
    AIS_EVENT_CSI_FATAL_ERROR,
    AIS_EVENT_INPUT_STATUS,
    AIS_EVENT_SOF,
    AIS_EVENT_SUBMIT_BUFFER,
    AIS_EVENT_MAP_BUFFERS,
}ais_engine_event_id_t;

/**
 * AIS internal frame done event
 */
typedef struct
{
    unsigned int pipeline_id;
    qcarcam_frame_info_t frame_info;
    unsigned long long us_starttime;
    unsigned long long after_sendtime;
}ais_frame_done_t;

/**
 * AIS internal csid fatal error event
 */
typedef struct
{
    uint32 csid;
}ais_csid_fatal_error_t;

/**
 * AIS internal input status event
 */
typedef struct
{
    uint32 dev_id;
    uint32 src_id;
    qcarcam_input_signal_t signal_status;
}ais_input_status_payload_t;

/**
 * AIS internal sof event
 */
typedef struct
{
    IfeCoreType       ife_core;
    IfeInterfaceType  ife_input;
    uint32   target_frame_id;
    uint64_t  sof_ts;
}ais_sof_t;

typedef struct
{
    uint32   chiCamIdx;
    uint32   idx;
}ais_chi_submitbuff_t;

typedef struct
{
    uint32         chiCamIdx;
    AisBufferList* pBufferList;
}ais_chi_map_buff_t;


typedef struct
{
    ais_engine_event_id_t event_id;
    union {
        ais_frame_done_t ife_frame_done;
        ais_csid_fatal_error_t csid_fatal_error;
        ais_input_status_payload_t input_status;
        ais_sof_t sof_info;
        ais_chi_submitbuff_t submit_buf;
        ais_chi_map_buff_t map_buf;
    }payload;
}ais_engine_event_msg_t;


#ifdef __cplusplus
};
#endif

#endif /* _AIS_I_H_ */

