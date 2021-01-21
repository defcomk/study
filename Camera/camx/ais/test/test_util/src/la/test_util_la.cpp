/* ===========================================================================
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#include <sys/mman.h>

#include "test_util.h"
#include "test_util_la.h"

#define KGSL_USER_MEM_TYPE_ION 3

#ifdef USE_STANDALONE_AIS
qcarcam_ret_t test_util_standalone_init(test_util_ctxt_t *ctxt);
qcarcam_ret_t test_util_standalone_deinit(test_util_ctxt_t *ctxt);
#endif

static qcarcam_ret_t get_display_info(test_util_ctxt_t *ctxt)
{
    sp<IBinder> displayToken(SurfaceComposerClient::getBuiltInDisplay(
                ISurfaceComposer::eDisplayIdMain));

    status_t status = SurfaceComposerClient::getDisplayInfo(displayToken, &ctxt->m_displayInfo);
    if (status)
    {
        QCARCAM_ERRORMSG("getDisplayInfo() failed");
        return QCARCAM_RET_FAILED;
    }
    else
    {
        if (ctxt->m_displayInfo.orientation != DISPLAY_ORIENTATION_0 &&
                ctxt->m_displayInfo.orientation != DISPLAY_ORIENTATION_180) {
            // flip width/height
            ctxt->m_displayInfo.w ^= ctxt->m_displayInfo.h;
            ctxt->m_displayInfo.h ^= ctxt->m_displayInfo.w;
            ctxt->m_displayInfo.w ^= ctxt->m_displayInfo.h;
        }
        ctxt->m_surfaceComposer = new SurfaceComposerClient();
        if (0 == ctxt->m_surfaceComposer.get())
        {
            QCARCAM_ERRORMSG("Failed to create SurfaceComposerClient");
            return QCARCAM_RET_FAILED;
        }

        // update display projection info
        android::Rect destRect(ctxt->m_displayInfo.w, ctxt->m_displayInfo.h);
#ifndef TESTUTIL_ANDROID_P
        ctxt->m_surfaceComposer->setDisplayProjection(displayToken, ctxt->m_displayInfo.orientation, destRect, destRect);
#else
        SurfaceComposerClient::Transaction t;
        t.setDisplayProjection(displayToken, ctxt->m_displayInfo.orientation, destRect, destRect);
#endif
    }
    return QCARCAM_RET_OK;
}

static unsigned int qcarcam_get_stride(qcarcam_color_fmt_t fmt, unsigned int width)
{
    unsigned int stride = 0;
    switch(fmt)
    {
        case QCARCAM_FMT_RGB_888:
            stride = width*3;
            break;
        case QCARCAM_FMT_MIPIRAW_8:
            stride = width;
            break;
        case QCARCAM_FMT_MIPIRAW_10:
            if (0 == (width % 4))
                stride = width*5/4;
            break;
        case QCARCAM_FMT_MIPIRAW_12:
            if (0 == (width % 2))
            {
                stride = width*3/2;
            }
            break;
        case QCARCAM_FMT_UYVY_8:
            stride = width*2;
            break;
        case QCARCAM_FMT_UYVY_10:
            if (0 == (width % 4))
            {
                stride = width*2*5/4;
            }
            break;
        case QCARCAM_FMT_UYVY_12:
            if (0 == (width % 2))
            {
                stride = width*2*3/2;
            }
            break;
        default:
            break;
    }

    return stride;
}

static int test_util_get_native_format(test_util_color_fmt_t fmt)
{
    switch(fmt)
    {
        case TESTUTIL_FMT_MIPIRAW_8:
        case TESTUTIL_FMT_RGB_565:
        case TESTUTIL_FMT_RGB_888:
        case TESTUTIL_FMT_RGBX_8888:
        case TESTUTIL_FMT_UYVY_8:
        default:
            return HAL_PIXEL_FORMAT_CbYCrY_422_I;
            break;
    }
    return 0;
}

static void alloc_graphic_buffers(test_util_window_t *user_ctxt, qcarcam_buffers_t *buffers)
{
    const int graphicsUsage  = android::GraphicBuffer::USAGE_HW_TEXTURE |
            android::GraphicBuffer::USAGE_SW_WRITE_OFTEN;

    user_ctxt->n_buffers = buffers->n_buffers;
    user_ctxt->buffer_size[0] = buffers->buffers[0].planes[0].width;
    user_ctxt->buffer_size[1] = buffers->buffers[0].planes[0].height;
    user_ctxt->stride = qcarcam_get_stride(buffers->color_fmt, buffers->buffers[0].planes[0].width);

    QCARCAM_DBGMSG("Buffers %d width = %d height = %d \n",
            user_ctxt->n_buffers, user_ctxt->buffer_size[0], user_ctxt->buffer_size[1]);

    user_ctxt->buffers = (test_util_buffer_t*)calloc(user_ctxt->n_buffers, sizeof(*user_ctxt->buffers));
    user_ctxt->pbuf = (void**)calloc(user_ctxt->n_buffers, sizeof(void*));
    user_ctxt->gfx_bufs = (sp<GraphicBuffer>*)calloc(user_ctxt->n_buffers, sizeof(sp<GraphicBuffer>));

    if(NULL == user_ctxt->buffers)
    {
        QCARCAM_ERRORMSG("Value of user_ctxt->buffers is NULL");
        return;
    }

    if(NULL == user_ctxt->pbuf)
    {
        QCARCAM_ERRORMSG("Value of user_ctxt->pbuf is NULL");
        return;
    }

    if(NULL == user_ctxt->gfx_bufs)
    {
        QCARCAM_ERRORMSG("Value of user_ctxt->gfx_bufs is NULL");
        return;
    }


    for (int i=0; i<user_ctxt->n_buffers; i++)
    {
        buffers->buffers[i].planes[0].stride = qcarcam_get_stride(buffers->color_fmt, buffers->buffers[i].planes[0].width);
        buffers->buffers[i].planes[0].size = buffers->buffers[i].planes[0].height * buffers->buffers[i].planes[0].stride;

        QCARCAM_ERRORMSG("%dx%d %d %d", buffers->buffers[i].planes[0].width, buffers->buffers[i].planes[0].height, buffers->buffers[i].planes[0].stride, buffers->buffers[i].planes[0].size);

        user_ctxt->gfx_bufs[i] = new GraphicBuffer(buffers->buffers[i].planes[0].width,
                buffers->buffers[i].planes[0].height,
                user_ctxt->native_format,
                graphicsUsage);

        struct private_handle_t * private_hndl = (struct private_handle_t *)(user_ctxt->gfx_bufs[i]->getNativeBuffer()->handle);
        QCARCAM_DBGMSG("%d %p\n",i, private_hndl);
        buffers->buffers[i].planes[0].p_buf = (void*)(uintptr_t)(private_hndl->fd);

        if (user_ctxt->pbuf)
        {
            user_ctxt->pbuf[i] = buffers->buffers[i].planes[0].p_buf;
        }

        if (user_ctxt->buffers)
        {
            user_ctxt->buffers[i].fd = private_hndl->fd;
        }

        user_ctxt->buffers[i].is_dequeud = 1;
        user_ctxt->buffers[i].size = buffers->buffers[i].planes[0].size;

        user_ctxt->buffers[i].ptr =
                mmap(NULL, user_ctxt->buffers[i].size,
                        PROT_READ | PROT_WRITE, MAP_SHARED,
                        user_ctxt->buffers[i].fd, 0);
    }
}

static void alloc_native_window_buffers(test_util_window_t *user_ctxt, qcarcam_buffers_t *buffers)
{
    user_ctxt->n_buffers = buffers->n_buffers;
    user_ctxt->buffer_size[0] = buffers->buffers[0].planes[0].width;
    user_ctxt->buffer_size[1] = buffers->buffers[0].planes[0].height;

    QCARCAM_DBGMSG("Buffers %d width = %d height = %d \n",
            user_ctxt->n_buffers, user_ctxt->buffer_size[0], user_ctxt->buffer_size[1]);

    user_ctxt->nativeWindow->query(user_ctxt->nativeWindow,
            NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &user_ctxt->min_num_buffers);
    QCARCAM_DBGMSG("NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS %d\n", user_ctxt->min_num_buffers);

    native_window_set_buffer_count(user_ctxt->nativeWindow, user_ctxt->n_buffers);

    native_window_set_buffers_dimensions(user_ctxt->nativeWindow, user_ctxt->buffer_size[0], user_ctxt->buffer_size[1]);

    native_window_set_scaling_mode(user_ctxt->nativeWindow,
            NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);

    native_window_api_connect(user_ctxt->nativeWindow, NATIVE_WINDOW_API_CAMERA);

    user_ctxt->buffers = (test_util_buffer_t*)calloc(user_ctxt->n_buffers, sizeof(*user_ctxt->buffers));
    user_ctxt->pbuf = (void**)calloc(user_ctxt->n_buffers, sizeof(void*));

    if(NULL == user_ctxt->buffers)
    {
        QCARCAM_ERRORMSG("Value of user_ctxt->buffers is NULL");
        return;
    }

    if(NULL == user_ctxt->pbuf)
    {
        QCARCAM_ERRORMSG("Value of user_ctxt->pbuf is NULL");
        return;
    }

    for (int i=0; i<user_ctxt->n_buffers; i++)
    {
        ANativeWindowBuffer* window_buf;
        int err = 0;
        err = user_ctxt->nativeWindow->dequeueBuffer(user_ctxt->nativeWindow,
                &window_buf, &user_ctxt->buffers[i].fenceFd);
        QCARCAM_DBGMSG("window_buf[%d]=%p", i, window_buf);
        if(err)
        {
            QCARCAM_ERRORMSG("Dequeue buffer failed for, fence =%d, err = %d",
                    user_ctxt->buffers[i].fenceFd, err);
        }

        buffers->buffers[i].planes[0].stride = qcarcam_get_stride(buffers->color_fmt, window_buf->stride);
        buffers->buffers[i].planes[0].size = window_buf->height * buffers->buffers[i].planes[0].stride;

        QCARCAM_DBGMSG("%dx%d %d %d", window_buf->width, window_buf->height, buffers->buffers[i].planes[0].stride, buffers->buffers[i].planes[0].size);

        struct private_handle_t * private_hndl = (struct private_handle_t *)window_buf->handle;
        buffers->buffers[i].planes[0].p_buf = (void*)(uintptr_t)(private_hndl->fd);

        if (user_ctxt->pbuf)
        {
            user_ctxt->pbuf[i] = buffers->buffers[i].planes[0].p_buf;
        }

        if (user_ctxt->buffers)
        {
            user_ctxt->buffers[i].fd = private_hndl->fd;
            user_ctxt->buffers[i].is_dequeud = 1;
            user_ctxt->buffers[i].size = buffers->buffers[i].planes[0].size;

            user_ctxt->buffers[i].window_buf = window_buf;
            user_ctxt->buffers[i].ptr =
                mmap(NULL, user_ctxt->buffers[i].size,
                        PROT_READ | PROT_WRITE, MAP_SHARED,
                        user_ctxt->buffers[i].fd, 0);
        }
    }

    user_ctxt->stride = buffers->buffers[0].planes[0].stride;
    for (int i=0; i<user_ctxt->min_num_buffers;i++)
    {
        int err = 0;
        err = user_ctxt->nativeWindow->cancelBuffer(user_ctxt->nativeWindow,
                user_ctxt->buffers[i].window_buf, user_ctxt->buffers[i].fenceFd);
        if(err)
            QCARCAM_ERRORMSG("Cancel buffer failed. Error = %d \n",err);

        user_ctxt->buffers[i].is_dequeud = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_init
///
/// @brief Initialize context that is to be used to display content on the screen.
///
/// @param ctxt   Pointer to context to be initialized
/// @param params Parameters to init ctxt
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_init(test_util_ctxt_t **ctxt, test_util_ctxt_params_t *params)
{
    qcarcam_ret_t rc;
    test_util_ctxt_t* pCtxt;

    *ctxt = NULL;

    MM_Debug_Initialize();

    if (!params)
    {
        return QCARCAM_RET_BADPARAM;
    }

    pCtxt = (test_util_ctxt_t*)calloc(1, sizeof(struct test_util_ctxt_t));
    if (!pCtxt)
    {
        return QCARCAM_RET_NOMEM;
    }

    pCtxt->params = *params;


#ifdef USE_STANDALONE_AIS
    test_util_standalone_init(pCtxt);
#endif
    if (!pCtxt->params.disable_display)
    {
        rc = get_display_info(pCtxt);
        if (rc != QCARCAM_RET_OK)
        {
            goto fail;
        }
    }


    *ctxt = pCtxt;

    return QCARCAM_RET_OK;

fail:
    free(pCtxt);
    return QCARCAM_RET_FAILED;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_init_window
///
/// @brief Initialize window for display
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_init_window(test_util_ctxt_t *ctxt, test_util_window_t **user_ctxt)
{
    *user_ctxt = (test_util_window_t*)calloc(1, sizeof(struct test_util_window_t));
    if (!*user_ctxt)
    {
        return QCARCAM_RET_NOMEM;
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_init_window_buffers
///
/// @brief Initialize buffers for display
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param buffers          Pointer to qcarcam buffers
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_init_window_buffers(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, qcarcam_buffers_t *buffers)
{
    if (!ctxt->params.disable_display)
    {
        alloc_native_window_buffers(user_ctxt, buffers);
    }
    else
    {
        alloc_graphic_buffers(user_ctxt, buffers);
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_post_window_buffer
///
/// @brief Send frame to display
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param idx              Frame ID number
/// @param rel_idx          Pointer to previous frame ID number
/// @param field_type     Field type in current frame buffer if interlaced
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_post_window_buffer(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx,
        int* rel_idx, qcarcam_field_t field_type)
{
    if (!ctxt->params.disable_display)
    {
        int err = 0;
        int fenceFd = 0;
        ANativeWindowBuffer* window_buf = NULL;

        if (user_ctxt->buffers[idx].is_dequeud)
        {
            if (ctxt->params.enable_di == SW_BOB_30FPS)
            {
                android_native_rect_t rect = {0, 14, user_ctxt->buffer_size[0], (DE_INTERLACE_HEIGHT/2) + 14};
                native_window_set_crop(user_ctxt->nativeWindow, &rect);

                err = user_ctxt->nativeWindow->queueBuffer(user_ctxt->nativeWindow,
                        user_ctxt->buffers[idx].window_buf, user_ctxt->buffers[idx].fenceFd);
                if(err)
                {
                    QCARCAM_ERRORMSG("Queuebuffer %d returned with err = %d", idx, err);
                }
                else
                {
                    user_ctxt->buffers[idx].is_dequeud = 0;
                }
            }
            else if (ctxt->params.enable_di == SW_BOB_60FPS)
            {
                if (field_type == QCARCAM_FIELD_ODD_EVEN)
                {
                    android_native_rect_t rect = {0, 13, user_ctxt->buffer_size[0], (DE_INTERLACE_HEIGHT/2) + 13};
                    native_window_set_crop(user_ctxt->nativeWindow, &rect);

                    err = user_ctxt->nativeWindow->queueBuffer(user_ctxt->nativeWindow,
                            user_ctxt->buffers[idx].window_buf, user_ctxt->buffers[idx].fenceFd);
                    if(err)
                    {
                        QCARCAM_ERRORMSG("Queuebuffer %d returned with err = %d", idx, err);
                    }

                    err = user_ctxt->nativeWindow->dequeueBuffer(user_ctxt->nativeWindow,
                            &window_buf, &fenceFd);
                    if(err)
                    {
                        QCARCAM_ERRORMSG("Dequeuebuffer %d returned with err = %d", idx, err);
                    }

                    rect = {0, (DE_INTERLACE_HEIGHT/2) + 27,user_ctxt->buffer_size[0], DE_INTERLACE_HEIGHT + 27};
                    native_window_set_crop(user_ctxt->nativeWindow, &rect);

                    err = user_ctxt->nativeWindow->queueBuffer(user_ctxt->nativeWindow,
                            user_ctxt->buffers[idx].window_buf, user_ctxt->buffers[idx].fenceFd);
                    if(err)
                    {
                        QCARCAM_ERRORMSG("Queuebuffer %d returned with err = %d", idx, err);
                    }
                    else
                    {
                        user_ctxt->buffers[idx].is_dequeud = 0;
                    }
                }
                else if (field_type == QCARCAM_FIELD_EVEN_ODD)
                {
                    android_native_rect_t rect = {0, 14, user_ctxt->buffer_size[0], (DE_INTERLACE_HEIGHT/2) + 14};
                    native_window_set_crop(user_ctxt->nativeWindow, &rect);

                    err = user_ctxt->nativeWindow->queueBuffer(user_ctxt->nativeWindow,
                            user_ctxt->buffers[idx].window_buf, user_ctxt->buffers[idx].fenceFd);
                    if(err)
                    {
                        QCARCAM_ERRORMSG("Queuebuffer %d returned with err = %d", idx, err);
                    }

                    err = user_ctxt->nativeWindow->dequeueBuffer(user_ctxt->nativeWindow,
                            &window_buf, &fenceFd);
                    if(err)
                    {
                        QCARCAM_ERRORMSG("Dequeuebuffer %d returned with err = %d", idx, err);
                    }

                    rect = {0, (DE_INTERLACE_HEIGHT/2) + 27,user_ctxt->buffer_size[0], DE_INTERLACE_HEIGHT + 27};
                    native_window_set_crop(user_ctxt->nativeWindow, &rect);

                    err = user_ctxt->nativeWindow->queueBuffer(user_ctxt->nativeWindow,
                            user_ctxt->buffers[idx].window_buf, user_ctxt->buffers[idx].fenceFd);
                    if(err)
                    {
                        QCARCAM_ERRORMSG("Queuebuffer %d returned with err = %d", idx, err);
                    }
                    else
                    {
                        user_ctxt->buffers[idx].is_dequeud = 0;
                    }
                }
            }
            else
            {
                err = user_ctxt->nativeWindow->queueBuffer(user_ctxt->nativeWindow,
                        user_ctxt->buffers[idx].window_buf, user_ctxt->buffers[idx].fenceFd);
                if(err)
                {
                    QCARCAM_ERRORMSG("Queuebuffer %d returned with err = %d", idx, err);
                }
                else
                {
                    user_ctxt->buffers[idx].is_dequeud = 0;
                }
            }
        }

        err = user_ctxt->nativeWindow->dequeueBuffer(user_ctxt->nativeWindow, &window_buf, &fenceFd);
        if(err)
        {
            QCARCAM_ERRORMSG("Dequeuebuffer %d returned with err = %d", idx, err);
        }

        if (err == 0)
        {
            for (int i = 0; i < user_ctxt->n_buffers; i++) {
                if (user_ctxt->buffers[i].window_buf == window_buf)
                {
                    user_ctxt->buffers[i].is_dequeud = 1;
                    user_ctxt->buffers[i].fenceFd = fenceFd;
                    if(rel_idx != NULL)
                    {
                        *rel_idx = i;
                    }
                    break;
                }
            }
        }
    }

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_dump_window_buffer
///
/// @brief Dump frame to a file
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param idx              Frame ID number
/// @param filename         Char pointer to file name to be dumped
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_dump_window_buffer(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx, const char *filename)
{
    FILE *fp;
    size_t numBytesWritten = 0;
    size_t numByteToWrite = 0;
    unsigned char *pBuf = 0;

    if (!user_ctxt->buffers[idx].ptr)
    {
        QCARCAM_ERRORMSG("buffer is not mapped");
        return QCARCAM_RET_FAILED;
    }

    fp = fopen(filename, "w+");

    QCARCAM_ERRORMSG("dumping qcarcam frame %s numbytes %d", filename,
        user_ctxt->buffers[idx].size);

    if (0 != fp)
    {
        pBuf = (unsigned char *)user_ctxt->buffers[idx].ptr;
        numByteToWrite = user_ctxt->buffers[idx].size;

        numBytesWritten = fwrite(pBuf, 1, numByteToWrite, fp);

        if (numBytesWritten != numByteToWrite)
        {
            QCARCAM_ERRORMSG("error no data written to file");
        }
        fclose(fp);
    }
    else
    {
        QCARCAM_ERRORMSG("failed to open file");
        return QCARCAM_RET_FAILED;
    }
    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_get_buf_ptr
///
/// @brief Get source and destination buffer virtual address
///
/// @param di_info         Input souce/dest buffer context
/// @param buf_ptr        Pointer to output virtual address
/// @param stride          Pointer to output line size
///
/// @return Void
///////////////////////////////////////////////////////////////////////////////
void test_util_get_buf_ptr(test_util_sw_di_t *di_info, test_util_buf_ptr_t *buf_ptr, uint32_t *stride)
{
    // Todo

    //buf_ptr->source = (unsigned char*)di_info->qcarcam_window->...;
    //buf_ptr->dest = (unsigned char*)di_info->display_window->...;
    //*stride = di_info->qcarcam_window->stride;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_deinit_window
///
/// @brief Destroy window
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_deinit_window(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt)
{
    // Deinit window/surface if any
    free(user_ctxt);

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_deinit_window_buffer
///
/// @brief Destroy window buffers
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_deinit_window_buffer(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt)
{
    int i = 0;

    for (i=0; i<user_ctxt->n_buffers; i++)
    {
        if (user_ctxt->buffers[i].ptr)
        {
            munmap(user_ctxt->buffers[i].ptr,
                    user_ctxt->buffers[i].size);
        }
    }

    if (user_ctxt->buffers)
    {
        free(user_ctxt->buffers);
    }

    if (user_ctxt->pbuf)
    {
        free(user_ctxt->pbuf);
    }

    if (user_ctxt->gfx_bufs)
    {
        free(user_ctxt->gfx_bufs);
    }
    
    ctxt->m_surfaceComposer = NULL;
    user_ctxt->m_surfaceControl = NULL;

    memset(user_ctxt, 0x0, sizeof(*user_ctxt));

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_deinit
///
/// @brief Destroy context and free memory.
///
/// @param ctxt   Pointer to context to be destroyed
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_deinit(test_util_ctxt_t *ctxt)
{
#ifdef USE_STANDALONE_AIS
    test_util_standalone_deinit(ctxt);
#endif

    MM_Debug_Deinitialize();

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_create_c2d_surface
///
/// @brief Create a C2D surface
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param idx              Frame ID number
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_create_c2d_surface(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx)
{
#ifndef C2D_DISABLED
    void *gpuaddr = NULL;
    int stride = qcarcam_get_stride(QCARCAM_FMT_UYVY_8, user_ctxt->buffer_size[0]);
    int length = stride * user_ctxt->buffer_size[1];
    length = (length + 4096 - 1) & ~(4096 - 1);

    C2D_STATUS c2d_status;
    C2D_YUV_SURFACE_DEF c2d_yuv_surface_def;
    c2d_yuv_surface_def.format = C2D_COLOR_FORMAT_422_UYVY;
    c2d_yuv_surface_def.width = user_ctxt->buffer_size[0];
    c2d_yuv_surface_def.height = user_ctxt->buffer_size[1];
    c2d_yuv_surface_def.stride0 = stride;
    c2d_yuv_surface_def.plane0 = user_ctxt->buffers[idx].ptr;
    c2d_status = c2dMapAddr((int)(uintptr_t)user_ctxt->pbuf[idx], user_ctxt->buffers[idx].ptr, length, 0, KGSL_USER_MEM_TYPE_ION, &gpuaddr);
    c2d_yuv_surface_def.phys0 = gpuaddr;
    C2D_SURFACE_TYPE surface_type = (C2D_SURFACE_TYPE)(C2D_SURFACE_YUV_HOST | C2D_SURFACE_WITH_PHYS);

    c2d_status = c2dCreateSurface(&user_ctxt->buffers[idx].c2d_surface_id,
            C2D_SOURCE | C2D_TARGET,
            surface_type,
            &c2d_yuv_surface_def);

    if (c2d_status != C2D_STATUS_OK)
    {
        QCARCAM_ERRORMSG("c2dCreateSurface %d buf %d failed %d", 0, idx, c2d_status);
        return QCARCAM_RET_FAILED;
    }
#endif
    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_get_c2d_surface_id
///
/// @brief Get the ID from a C2D surface
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param idx              Frame ID number
/// @param surface_id       Pointer to C2D sruface ID number
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_get_c2d_surface_id(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx, unsigned int *surface_id)
{
    if (!surface_id)
        return QCARCAM_RET_BADPARAM;

    *surface_id = user_ctxt->buffers[idx].c2d_surface_id;

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_set_window_param
///
/// @brief Send window parameters to display
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param window_params    Pointer to structure with window properties
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_set_window_param(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, test_util_window_param_t *window_params)
{
    user_ctxt->native_format = test_util_get_native_format(window_params->format);

    if (!ctxt->params.disable_display)
    {
        unsigned int w = ctxt->m_displayInfo.w * window_params->window_size[0];
        unsigned int h = ctxt->m_displayInfo.h * window_params->window_size[1];
        int x          = ctxt->m_displayInfo.w * window_params->window_pos[0];
        int y          = ctxt->m_displayInfo.h * window_params->window_pos[1];
        int src_width  = ctxt->m_displayInfo.w * window_params->window_source_size[0];
        int src_height = ctxt->m_displayInfo.h * window_params->window_source_size[1];
        int src_x      = ctxt->m_displayInfo.w * window_params->window_source_pos[0];
        int src_y      = ctxt->m_displayInfo.h * window_params->window_source_pos[1];

        QCARCAM_DBGMSG("w = %u h = %u\n", w, h);
        QCARCAM_DBGMSG("x = %d y= %d\n", x, y);

        if (ctxt->m_surfaceComposer.get())
        {
            user_ctxt->m_surfaceControl = ctxt->m_surfaceComposer->createSurface(android::String8("qcarcam_test"),
                    ctxt->m_displayInfo.w,
                    ctxt->m_displayInfo.h,
                    user_ctxt->native_format);
            if (0 == user_ctxt->m_surfaceControl.get())
            {
                QCARCAM_ERRORMSG("createSurface() failed\n");
                return QCARCAM_RET_FAILED;
            }
        }
        else
        {
            QCARCAM_ERRORMSG("ctxt->m_surfaceComposer.get() failed\n");
            return QCARCAM_RET_FAILED;
        }
#ifndef TESTUTIL_ANDROID_P
        android::SurfaceComposerClient::openGlobalTransaction();
        user_ctxt->m_surfaceControl->setSize(w, h);
        user_ctxt->m_surfaceControl->setLayer(0x40000000);
        user_ctxt->m_surfaceControl->setPosition(x, y);
        android::SurfaceComposerClient::closeGlobalTransaction();
#else
        android::SurfaceComposerClient::Transaction{}
                .setCrop(user_ctxt->m_surfaceControl, Rect(src_x, src_y, src_width, src_height))
                .setSize(user_ctxt->m_surfaceControl, w, h)
                .setLayer(user_ctxt->m_surfaceControl, 0x40000000)
                .setPosition(user_ctxt->m_surfaceControl, x, y)
                .show(user_ctxt->m_surfaceControl)
                .apply();
#endif
        sp<ANativeWindow> anw = user_ctxt->m_surfaceControl->getSurface();
        user_ctxt->nativeWindow = anw.get();
    }

    user_ctxt->buffer_size[0] = window_params->buffer_size[0];
    user_ctxt->buffer_size[1] = window_params->buffer_size[1];

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_set_diag
///
/// @brief set the diagnostic structure to test_util_window_t
///
/// @param ctxt             Pointer to display context
/// @param user_ctxt        Pointer to structure containing window parameters
/// @param diag             diagnostic structure
///
/// @return Void
///////////////////////////////////////////////////////////////////////////////
void test_util_set_diag(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, test_util_diag_t* diag)
{
    if(user_ctxt)
    {
        user_ctxt->diag = diag;
    }
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_gpio_config
///
/// @brief enable IO privileges, configure the gpio and set it up for interrupts
///
/// @param intr             Pointer for the IRQ to be stored
/// @param gpio_number      Specific gpio that is being utilised
/// @param trigger          Instance of the signal which shall causes the interrupt
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_gpio_interrupt_config(uint32_t *intr, int gpio_number, test_util_trigger_type_t trigger)
{
    return QCARCAM_RET_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_interrupt_attach
///
/// @brief attach the interrupt event to an interrupt id
///
/// @param arguments    arguments to pass to the newly created thread
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_interrupt_attach(test_util_intr_thrd_args_t *arguments)
{
    return QCARCAM_RET_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_interrupt_wait_and_unmask
///
/// @brief wait for a GPIO interrupt and then unmask it
///
/// @param irq              IRQ to unmask
/// @param interrupt_id     interrupt id to unmask
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_interrupt_wait_and_unmask(uint32_t irq, int interrupt_id)
{
    return QCARCAM_RET_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_get_param
///
/// @brief get the value of the window parameter of the window
///
/// @param user_ctxt        window we want to use
/// @param param            window parameter you are trying to access
/// @param value            value of parameter will be stored here
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_get_param(test_util_window_t *user_ctxt, test_util_params_t param, int *value)
{
    return QCARCAM_RET_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_set_param
///
/// @brief set the value of the window parameter
///
/// @param user_ctxt        window we want to use
/// @param param            window parameter you want to change
/// @param value            value you want to set the param to
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_set_param(test_util_window_t *user_ctxt, test_util_params_t param, int value)
{
    return QCARCAM_RET_UNSUPPORTED;
}
