#ifndef _AIS_BUFFER_MANAGER_H_
#define _AIS_BUFFER_MANAGER_H_

/*!
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ais_engine.h"

class AisBufferManager
{
public:
    /**
     * MapBuffers
     *
     * @brief Map buffers to a pUsrCtxt buffer_list
     *
     * @param pUsrCtxt
     * @param pBuffers - buffers to map
     * @param pBufferList - buffer list to hold mapping
     *
     * @return CameraResult
     */
    static CameraResult MapBuffers(AisBufferList* pBufferList, qcarcam_buffers_t* pBuffers);

    /**
     * UnmapBuffers
     *
     * @brief UnMap buffers from a pUsrCtxt buffer_list
     *
     * @param pUsrCtxt
     * @param pBufferList
     *
     * @return CameraResult
     */
    static CameraResult UnmapBuffers(AisBufferList* pBufferList);

    /**
     * GetAvailableBufferList
     *
     * @brief Get available buffer list index
     *
     * @param pUsrCtxt
     * @param *nBufferListIdx - returned buffer list index
     *
     * @return CameraResult
     */
    static CameraResult GetAvailableBufferList(AisUsrCtxt* pUsrCtxt, uint32* nBufferListIdx);

    /**
     * FreeBufferList
     *
     * @brief Release buffer list index
     *
     * @param pUsrCtxt
     * @param nBufferListIdx
     *
     * @return CameraResult
     */
    static CameraResult FreeBufferList(AisUsrCtxt* pUsrCtxt, uint32 nBufferListIdx);
};



#endif /* _AIS_BUFFER_MANAGER_H_ */
