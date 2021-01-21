/*!
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ais_buffer_manager.h"
#include "CameraPlatform.h"

/**
 * MapBuffers
 *
 * @brief Map buffers to a usr_ctxt buffer_list
 *
 * @param usr_ctxt
 * @param p_buffers - buffers to map
 * @param p_buffer_list - buffer list to hold mapping
 *
 * @return CameraResult
 */
CameraResult AisBufferManager::MapBuffers(AisBufferList* pBufferList,
        qcarcam_buffers_t* pQcarcamBuffers)
{
    CameraHwBlockType hwBlock = CAMERA_HWBLOCK_IFE;
    CameraResult rc = CAMERA_SUCCESS;
    int i = 0;

    pBufferList->m_colorFmt = pQcarcamBuffers->color_fmt;

    for (i = 0; i < (int)pQcarcamBuffers->n_buffers; i++)
    {
        uint64_t devAddr = 0;

        rc = CameraSMMUMapCameraBuffer(
            pQcarcamBuffers->buffers[i].planes[0].p_buf,
            pQcarcamBuffers->buffers[i].planes[0].size,
            0x0,
            0x0,
            &hwBlock,
            1);

        if (CAMERA_SUCCESS == rc)
        {
            devAddr = CameraSMMUGetDeviceAddress(pQcarcamBuffers->buffers[i].planes[0].p_buf, hwBlock);
        }

        if (!devAddr)
        {
            CAM_MSG(ERROR, "Failed to smmu map buffer idx %d", i);
            /*cleanup by unmapping previously mapped buffers*/
            i--;
            for (; i >= 0; i--)
            {
                CameraSMMUUnmapCameraBuffer(pQcarcamBuffers->buffers[i].planes[0].p_buf);
            }
            std_memset(pBufferList->m_pBuffers, 0x0, sizeof(pBufferList->m_pBuffers));
            rc = CAMERA_EFAILED;
            break;
        }

        pBufferList->m_pBuffers[i].index = i;

        pBufferList->m_pBuffers[i].qcarmcam_buf = pQcarcamBuffers->buffers[i];

        pBufferList->m_pBuffers[i].buffer_state = AIS_BUFFER_INITIALIZED;
        pBufferList->m_pBuffers[i].mem_hndl = pQcarcamBuffers->buffers[i].planes[0].p_buf;
        pBufferList->m_pBuffers[i].p_da = devAddr;
    }

    if (CAMERA_SUCCESS == rc)
    {
        pBufferList->m_nBuffers = pQcarcamBuffers->n_buffers;
        CAM_MSG(HIGH, "Mapped %d buffers", pBufferList->m_nBuffers);
    }

    return rc;
}

/**
 * UnmapBuffers
 *
 * @brief Unap buffers from a usr_ctxt buffer_list
 *
 * @param usr_ctxt
 * @param p_buffer_list
 *
 * @return CameraResult
 */
CameraResult AisBufferManager::UnmapBuffers(AisBufferList* pBufferList)
{
    uint32 i = 0;

    for (i = 0; i < pBufferList->m_nBuffers; i++)
    {
        CameraSMMUUnmapCameraBuffer(pBufferList->m_pBuffers[i].mem_hndl);
    }
    std_memset(pBufferList->m_pBuffers, 0x0, sizeof(pBufferList->m_pBuffers));
    pBufferList->m_nBuffers = 0;

    return CAMERA_SUCCESS;
}

/**
 * GetAvailableBufferList
 *
 * @brief Get available buffer list index
 *
 * @param usr_ctxt
 * @param *buffer_list_idx - returned buffer list index
 *
 * @return CameraResult
 */
CameraResult AisBufferManager::GetAvailableBufferList(AisUsrCtxt* usr_ctxt, uint32* buffer_list_idx)
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
 * FreeBufferList
 *
 * @brief Release buffer list index
 *
 * @param usr_ctxt
 * @param buffer_list_idx
 *
 * @return CameraResult
 */
CameraResult AisBufferManager::FreeBufferList(AisUsrCtxt* usr_ctxt, uint32 buffer_list_idx)
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

    CAM_MSG_ON_ERR(rc, "failed rc %d buffer list idx %d is not in use",
                      rc, buffer_list_idx);

    return rc;
}

/**
 * GetBufferState
 *
 * @brief Get state of a buffer in buffer list
 *
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

