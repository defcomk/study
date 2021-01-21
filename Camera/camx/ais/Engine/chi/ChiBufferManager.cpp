/*!
 * Copyright (c) 2016-2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ChiBufferManager.h"

#define USE_LATE_MAPPING 0xA15B0000  //used to indicate linux late mapping required


void* CameraSMMUMapCameraBuffer (void * out_buffer, uint32 size)
{
#ifdef OS_ANDROID
    return (void*)USE_LATE_MAPPING;
#else
    return out_buffer;
#endif
}

void CameraSMMUUnmapCameraBuffer(void*)
{

}


CameraResult ChiBufferManager::Init(void)
{
    //TODO: implement
    return CAMERA_SUCCESS;
}

CameraResult ChiBufferManager::Deinit(void)
{
    //TODO: implement
    return CAMERA_SUCCESS;
}

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
CameraResult ChiBufferManager::MapBuffers(ChiUsrCtxt* usr_ctxt,
        qcarcam_buffers_t* p_buffers,
        AisBufferList* p_buffer_list)
{
    CameraResult rc = CAMERA_SUCCESS;
    int i = 0;

    p_buffer_list->m_colorFmt = p_buffers->color_fmt;

    for (i = 0; i < (int)p_buffers->n_buffers; i++)
    {
        /*@todo: flush and cleanup if failure */
        p_buffer_list->m_pBuffers[i].index = i;

        p_buffer_list->m_pBuffers[i].qcarmcam_buf = p_buffers->buffers[i];

        p_buffer_list->m_pBuffers[i].buffer_state = AIS_BUFFER_INITIALIZED;
        p_buffer_list->m_pBuffers[i].p_va = p_buffers->buffers[i].planes[0].p_buf;
        p_buffer_list->m_pBuffers[i].p_da = (SMMU_ADDR_T)((uintptr_t)CameraSMMUMapCameraBuffer(p_buffer_list->m_pBuffers[i].p_va, p_buffers->buffers[i].planes[0].size));
        if (!p_buffer_list->m_pBuffers[i].p_da)
        {
            CAM_MSG(ERROR, "Failed to smmu map buffer idx %d for user %p", i, usr_ctxt);
            /*cleanup by unmapping previously mapped buffers*/
            i--;
            for (; i >= 0; i--)
            {
                CameraSMMUUnmapCameraBuffer(p_buffer_list->m_pBuffers[i].p_va);
            }
            rc = CAMERA_EFAILED;
            break;
        }
    }

    if (CAMERA_SUCCESS == rc)
    {
        p_buffer_list->m_nBuffers = p_buffers->n_buffers;
        CAM_MSG(HIGH, "User %p mapped %d buffers", usr_ctxt, p_buffer_list->m_nBuffers);
    }

    return rc;
}

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
CameraResult ChiBufferManager::UnmapBuffers(ChiUsrCtxt* usr_ctxt, AisBufferList* p_buffer_list)
{
    uint32 i = 0;

    for (i = 0; i < p_buffer_list->m_nBuffers; i++)
    {
        CameraSMMUUnmapCameraBuffer(p_buffer_list->m_pBuffers[i].p_va);
    }
    std_memset(p_buffer_list->m_pBuffers, 0x0, sizeof(p_buffer_list->m_pBuffers));
    p_buffer_list->m_nBuffers = 0;

    return CAMERA_SUCCESS;
}

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
CameraResult ChiBufferManager::GetAvailableBufferList(ChiUsrCtxt* usr_ctxt, uint32* buffer_list_idx)
{
    CameraResult rc = CAMERA_EFAILED;
    uint32 i;

    if (!buffer_list_idx)
    {
        CAM_MSG(ERROR, "buffer_list_idx NULL ptr");
        return CAMERA_EMEMPTR;
    }

    for (i = AIS_BUFFER_LIST_INTERNAL_START; i < AIS_BUFFER_LIST_INTERNAL_START + AIS_BUFFER_LIST_MAX_INTERNAL; i++)
    {
        if (usr_ctxt->m_bufferList[i].m_inUse == 0)
        {
            CAM_MSG(LOW, "usr_ctxt %p get buffer_list_idx %d", usr_ctxt, i);
            usr_ctxt->m_bufferList[i].m_inUse = TRUE;
            *buffer_list_idx = i;
            rc = CAMERA_SUCCESS;
            break;
        }
    }

    CAM_MSG_ON_ERR(rc, "%s failed rc %d no buffer list available", __func__, rc);

    return rc;
}

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
CameraResult ChiBufferManager::FreeBufferList(ChiUsrCtxt* usr_ctxt, uint32 buffer_list_idx)
{
    CameraResult rc = CAMERA_EFAILED;

    if (buffer_list_idx >= AIS_BUFFER_LIST_MAX)
    {
        CAM_MSG(ERROR, "invalid buffer_list_idx");
        return CAMERA_EBADPARM;
    }

    if (usr_ctxt->m_bufferList[buffer_list_idx].m_inUse == TRUE)
    {
        CAM_MSG(LOW, "usr_ctxt %p free buffer list idx %d", usr_ctxt, buffer_list_idx);
        usr_ctxt->m_bufferList[buffer_list_idx].m_inUse = FALSE;
        rc = CAMERA_SUCCESS;
    }

    CAM_MSG_ON_ERR(rc, "%s failed rc %d buffer list idx %d is not in use", __func__, rc, buffer_list_idx);

    return rc;
}

/**
 * ais_config_get_buffer_state
 *
 * @brief Get state of a buffer in buffer list
 *
 * @param p_buffer_list
 * @param idx
 *
 * @return ais_buffer_state_t
 */
ais_buffer_state_t AisBufferList::GetBufferState(uint32 idx)
{
    if (idx >= m_nBuffers)
    {
        CAM_MSG(ERROR, "p_buffer_list %p : invalid buf_idx %u ", this, idx);
        return AIS_BUFFER_UNITIALIZED;
    }

    return m_pBuffers[idx].buffer_state;
}

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
CameraResult AisBufferList::SetBufferState(uint32 idx, ais_buffer_state_t state)
{
    if (idx >= m_nBuffers)
    {
        CAM_MSG(ERROR, "p_buffer_list %p : invalid buf_idx %u ", this, idx);
        return CAMERA_EBADPARM;
    }

    m_pBuffers[idx].buffer_state = state;

    return CAMERA_SUCCESS;
}

