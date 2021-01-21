#ifndef _AIS_BUFFER_MANAGER_H_
#define _AIS_BUFFER_MANAGER_H_

/*!
 * Copyright (c) 2016-2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ChiEngine.h"

class ChiBufferManager
{
public:
    CameraResult Init(void);
    CameraResult Deinit(void);

    /**
     * ais_config_map_buffers
     *
     * @brief Map buffers to a usr_ctxt buffer_list
     *
     * @param usr_ctxt
     * @param p_buffers - buffers to map
     * @param p_buffer_list - buffer list to hold mapping
     *
     * @return CameraResult
     */
    static CameraResult MapBuffers(ChiUsrCtxt* hndl, qcarcam_buffers_t* p_buffers, AisBufferList* p_buffer_list);

    /**
     * ais_config_unmap_buffers
     *
     * @brief Unap buffers from a usr_ctxt buffer_list
     *
     * @param usr_ctxt
     * @param p_buffer_list
     *
     * @return CameraResult
     */
    static CameraResult UnmapBuffers(ChiUsrCtxt* usr_ctxt, AisBufferList* p_buffer_list);


    /**
     * ais_config_get_available_buffer_list
     *
     * @brief Get available buffer list index
     *
     * @param usr_ctxt
     * @param *buffer_list_idx - returned buffer list index
     *
     * @return CameraResult
     */
    static CameraResult GetAvailableBufferList(ChiUsrCtxt* usr_ctxt, uint32* buffer_list_idx);

    /**
     * ais_config_free_buffer_list
     *
     * @brief Release buffer list index
     *
     * @param usr_ctxt
     * @param buffer_list_idx
     *
     * @return CameraResult
     */
    static CameraResult FreeBufferList(ChiUsrCtxt* usr_ctxt, uint32 buffer_list_idx);
};



#endif /* _AIS_BUFFER_MANAGER_H_ */
