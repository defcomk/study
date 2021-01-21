/* ===========================================================================
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <linux/msm_ion.h>
#include <linux/ion.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "gbm.h"
#include "gbm_priv.h"

#include "c2d2.h"
#include "qcarcam.h"
#include "test_util.h"
#include "test_util_agl.h"
#include <wayland-client.h>
#include "ivi-application-client-protocol.h"

static bool display_buffer = 0;
static struct display_wl *display_pointer;
static enum wl_shm_format support_format = WL_SHM_FORMAT_XRGB8888;
static int input_num = 0;
static volatile int aborted = 0;
static bool b_post = false;

static int wl_buf_render(unsigned int stream_idx, int buf_idx);

static int test_util_create_uyvy_wlbuffer(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx, int w, int h);
static int test_util_create_uyvy_wlbuffer_gbm(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx, int w, int h);

static void notify_frame_done(test_util_window_t *user_ctxt);

static void handle_ping(void *data, struct wl_shell_surface *shell_surface,
                        uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void handle_configure(void *data, struct wl_shell_surface *shell_surface,
                             uint32_t edges, int32_t width, int32_t height)
{
}

static void handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static void handle_ivi_surface_configure(void *data, struct ivi_surface *ivi_surface, int32_t width, int32_t height)
{
}

static const struct ivi_surface_listener ivi_surface_listener = {
    handle_ivi_surface_configure,
};

static const struct wl_shell_surface_listener shell_surface_listener = {
    handle_ping,
    handle_configure,
    handle_popup_done};

static void
output_handle_geometry(void *data, struct wl_output *wl_output,
                       int32_t x, int32_t y,
                       int32_t physical_width, int32_t physical_height,
                       int32_t subpixel,
                       const char *make, const char *model,
                       int32_t output_transform)
{
}

static void
output_handle_mode(void *data, struct wl_output *wl_output,
                   uint32_t flags, int32_t width, int32_t height,
                   int32_t refresh)
{
    struct display_wl *d = (display_wl*)data;

    if (width != 0)
        d->output_width = width;
    if (height != 0)
        d->output_height = height;
}

static const struct wl_output_listener output_listener = {
    output_handle_geometry,
    output_handle_mode,
};

static void
frame_done(void *data, struct wl_callback *callback, uint32_t time)
{
    test_util_window_t *user_ctxt = (test_util_window_t *)data;
    notify_frame_done(user_ctxt);
    if (callback)
        wl_callback_destroy(callback);
}
static const struct wl_callback_listener frame_listener = {
    frame_done
};

static void wl_shm_fmt(void *dis, struct wl_shm *s, uint32_t f)
{
    struct display_wl *d = (display_wl*)dis;

    if (f == support_format)
        d->formats = support_format;
}

static struct wl_shm_listener qcarcam_shm_lsn = {
    wl_shm_fmt};

static void wl_registry_handle(void *display_data, struct wl_registry *reg,
                               uint32_t id, const char *intf, uint32_t ver)
{
    struct display_wl *d = (display_wl*)display_data;

    if (strcmp(intf, "wl_compositor") == 0)
    {
        d->compositor = (wl_compositor*)wl_registry_bind(reg, id, &wl_compositor_interface, 1);
    }
    else if (strcmp(intf, "wl_shm") == 0)
    {
        d->shm = (wl_shm*)wl_registry_bind(reg, id, &wl_shm_interface, 1);
        wl_shm_add_listener(d->shm, &qcarcam_shm_lsn, d);
    }
    else if (strcmp(intf, "wl_shell") == 0)
    {
        d->shell = (wl_shell*)wl_registry_bind(reg, id, &wl_shell_interface, 1);
    }
    else if (strcmp(intf, "ivi_application") == 0)
    {
        d->ivi_application = (ivi_application*)wl_registry_bind(reg, id, &ivi_application_interface, 1);
    }
    else if (strcmp(intf, "wl_scaler") == 0)
    {
        d->scaler_version = ver < 2 ? ver : 2;
        d->scaler = (wl_scaler*)wl_registry_bind(reg, id,
                                   &wl_scaler_interface,
                                   d->scaler_version);
    }
    else if (strcmp(intf, "wl_output") == 0)
    {
        d->output = (wl_output*)wl_registry_bind(reg, id, &wl_output_interface, 1);
        wl_output_add_listener(d->output, &output_listener, d);
    }
    else if (strcmp(intf, "gbm_buffer_backend") == 0)
    {
        d->gbmbuf = (gbm_buffer_backend*)wl_registry_bind(reg, id, &gbm_buffer_backend_interface, 1);
    }
    else if (strcmp(intf, "wp_viewporter") == 0)
    {
        d->viewporter = (wp_viewporter*)wl_registry_bind(reg, id, &wp_viewporter_interface, 1);
    }
}
static void wl_registry_handle_remove(void *display_data, struct wl_registry *reg, uint32_t n)
{
}
static const struct wl_registry_listener wl_registry_lsn = {
    wl_registry_handle,
    wl_registry_handle_remove};

static void buffer_release(void *data, struct wl_buffer *buffer)
{
    test_util_window_t *user_ctxt = (test_util_window_t *)data;
    if (user_ctxt) {
        //user_ctxt->buf_rel_time[TEST_PREV_BUFFER] = user_ctxt->buf_rel_time[TEST_CUR_BUFFER];
        test_util_get_time(&user_ctxt->diag->buf_rel_time[TEST_CUR_BUFFER]);
        notify_frame_done(user_ctxt);

        if (user_ctxt->diag->bprint) {
            QCARCAM_ERRORMSG("buffer release commit_interval %lu draw %lu transfer %lu total %lu",
                    (user_ctxt->diag->buf_commit_time[TEST_CUR_BUFFER] - user_ctxt->diag->buf_commit_time[TEST_PREV_BUFFER] ),
                    (user_ctxt->diag->buf_rel_time[TEST_CUR_BUFFER] - user_ctxt->diag->buf_commit_time[TEST_PREV_BUFFER]),
                    (user_ctxt->diag->buf_commit_time[TEST_PREV_BUFFER] - user_ctxt->diag->frame_generate_time[TEST_PREV_BUFFER] ),
                    (user_ctxt->diag->buf_rel_time[TEST_CUR_BUFFER] - user_ctxt->diag->frame_generate_time[TEST_PREV_BUFFER] )
                    );
        }
    }


}
static const struct wl_buffer_listener buffer_listener = {
    buffer_release};


static void
gbmcreate_buffer_succeeded(void *data, struct gbm_buffer_params *params,
                                  struct wl_buffer *new_buffer)
{
    struct gbm_buffer *gbm_buf = (struct gbm_buffer *)data;
    if (gbm_buf) {
        gbm_buf->wl_buf = new_buffer;
        gbm_buf->state = GBM_BUF_STATE_INIT_WL;
    }

    gbm_buffer_params_destroy(params);
    QCARCAM_INFOMSG( "gbm_buffer_params_create.create succeed.");
}

static void
gbmcreate_buffer_failed(void *data, struct gbm_buffer_params *params)
{
    struct gbm_buffer *gbm_buf = (struct gbm_buffer *)data;
    if (gbm_buf) {
        gbm_buf->wl_buf = NULL;
    }

    gbm_buffer_params_destroy(params);

    QCARCAM_ERRORMSG( "Error:gbm_buffer_params_create.create failed.");
}

static const struct gbm_buffer_params_listener gbm_params_listener = {
    gbmcreate_buffer_succeeded,
    gbmcreate_buffer_failed
};

static void *helper_thread(void *arg)
{
    int ret;
    pthread_setname_np(pthread_self(), "helper_thread");
    pthread_detach(pthread_self());

    while (!b_post)
    {
        usleep(3000);
    }

    while (!aborted)
    {
        ret = wl_display_dispatch(display_pointer->display);
        if (ret == -1)
        {
            QCARCAM_ERRORMSG("wl_display_dispatch failed");
        }
    }
    return NULL;
}

static unsigned int qcarcam_get_stride(qcarcam_color_fmt_t fmt, unsigned int width)
{
    unsigned int stride = 0;
    switch (fmt)
    {
    case QCARCAM_FMT_RGB_888:
        stride = width * 3;
        break;
    case QCARCAM_FMT_MIPIRAW_8:
        stride = width;
        break;
    case QCARCAM_FMT_MIPIRAW_10:
        if (0 == (width % 4))
            stride = width * 5 / 4;
        break;
    case QCARCAM_FMT_MIPIRAW_12:
        if (0 == (width % 2))
            stride = width * 3 / 2;
        break;
    case QCARCAM_FMT_UYVY_8:
        return width * 2;
    case QCARCAM_FMT_UYVY_10:
        if (0 == (width % 4))
            stride = width * 2 * 5 / 4;
        break;
    case QCARCAM_FMT_UYVY_12:
        if (0 == (width % 2))
            stride = width * 2 * 3 / 2;
        break;
    default:
        break;
    }

    return stride;
}

static struct display_wl *create_wl_display(void)
{
    struct display_wl *display;

    display = (display_wl*)calloc(1, sizeof(*display));
    if (display == NULL)
    {
        QCARCAM_ERRORMSG("out of memory");
        return NULL;
    }
    display->display = wl_display_connect(NULL);
    if (display->display == NULL)
    {
        QCARCAM_ERRORMSG("wl_display_connect failed.");
        free(display);
        return NULL;
    }

    display->formats = 0;
    display->registry = wl_display_get_registry(display->display);
    wl_registry_add_listener(display->registry, &wl_registry_lsn, display);
    wl_display_dispatch(display->display);
    wl_display_roundtrip(display->display);
    if (display->shm == NULL)
    {
        QCARCAM_ERRORMSG("No wl_shm global");
        free(display);
        return NULL;
    }

    wl_display_get_fd(display->display);

    return display;
}

static int init_mutex(test_util_window_t *user_ctxt)
{
    int rc = 0;
    pthread_condattr_t attr;
    if ( 0 != (rc = pthread_condattr_init(&attr)))
    {
        QCARCAM_ERRORMSG("%s: pthread_cond_attr_init failed: %s",
            __func__, strerror(rc));
    }
    else {
        if ( 0 != (rc = pthread_cond_init(&user_ctxt->cond_frame_done, &attr)))
        {
            QCARCAM_ERRORMSG( "%s: pthread_cond_init(cond_frame_done) failed: %s",
               __func__, strerror(rc));
        }
        else if  ( 0 != (rc = pthread_mutex_init(&user_ctxt->m_mutex, 0)))
        {
            pthread_cond_destroy(&user_ctxt->cond_frame_done);
        }

        pthread_condattr_destroy(&attr);
    }

    return rc;
}

static int uninit_mutex(test_util_window_t *user_ctxt)
{
    int rc;
    int tmprc;
    rc = pthread_cond_destroy(&user_ctxt->cond_frame_done);
    tmprc = pthread_mutex_destroy(&user_ctxt->m_mutex);
    if (0 == rc) {rc = tmprc;}

    return rc;
}

#define WAIT_MAX_FRAMES    2
#define WAIT_MAX_TIME    150000000
static void wait_last_frame_done(test_util_window_t *user_ctxt, bool has_buf_rel_cb)
{
    struct timespec to;
    clock_gettime(CLOCK_REALTIME, &to);
    to.tv_nsec += WAIT_MAX_TIME;
    pthread_mutex_lock(&user_ctxt->m_mutex);
    if (has_buf_rel_cb)
    {
        ++user_ctxt->commit_frames;
    }
    while (user_ctxt->commit_frames >= WAIT_MAX_FRAMES)
    {
        if (pthread_cond_timedwait(&user_ctxt->cond_frame_done,
                            &user_ctxt->m_mutex, &to))
        {
            break;
        }
    }
    pthread_mutex_unlock(&user_ctxt->m_mutex);
}

void notify_frame_done(test_util_window_t *user_ctxt)
{
    int rc = 0;
    pthread_mutex_lock(&user_ctxt->m_mutex);
    user_ctxt->commit_frames--;
    if (0 != (rc = pthread_cond_signal(&user_ctxt->cond_frame_done)))
    {
        QCARCAM_ERRORMSG("notify_frame_done: pthread_cond_signal failed: %s", strerror(rc));
    }
    pthread_mutex_unlock(&user_ctxt->m_mutex);
}

static bool can_commit_frames(test_util_window_t *user_ctxt)
{
    return (user_ctxt->commit_frames < WAIT_MAX_FRAMES);
}

static void post_window(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx, bool has_buf_rel_cb)
{
    if (can_commit_frames(user_ctxt))
    {
        struct wl_callback *callback =NULL;
        wl_surface_damage(user_ctxt->wl_window->surface, 0, 0, user_ctxt->wl_window->width, user_ctxt->wl_window->height);
        wl_surface_attach(user_ctxt->wl_window->surface, user_ctxt->gbm_buf_wl[idx].wl_buf, 0, 0);

        user_ctxt->diag->buf_commit_time[TEST_PREV_BUFFER] = user_ctxt->diag->buf_commit_time[TEST_CUR_BUFFER];
        test_util_get_time(&user_ctxt->diag->buf_commit_time[TEST_CUR_BUFFER]);
        wl_surface_commit(user_ctxt->wl_window->surface);
        wl_display_flush(ctxt->wl_display->display);

        wait_last_frame_done(user_ctxt, has_buf_rel_cb);
        user_ctxt->wl_window->buffer_index = idx;
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

    if (!pCtxt->params.disable_display)
    {
        int rc = 0;
        pthread_t pthread_object;
        pthread_attr_t pthread_attr;
        int major = 0, minor = 0;

        pCtxt->wl_display = create_wl_display();
        display_pointer = pCtxt->wl_display;

        if (!pCtxt->wl_display)
        {
            QCARCAM_ERRORMSG("create_wl_display failed");
            return QCARCAM_RET_FAILED;
        }
        pCtxt->egldpy = eglGetDisplay((EGLNativeDisplayType) display_pointer->display);
        if(!pCtxt->egldpy) {
            QCARCAM_ERRORMSG("eglGetDisplay failed\n");
            return QCARCAM_RET_FAILED;
        }

        if(!eglInitialize(pCtxt->egldpy, &major, &minor)) {
            QCARCAM_ERRORMSG("eglInitialize failed\n");
            pCtxt->egldpy = EGL_NO_DISPLAY;
            return QCARCAM_RET_FAILED;
        }
        QCARCAM_ERRORMSG("egl dsp %p\n", pCtxt->egldpy);

        rc = pthread_attr_init(&pthread_attr);
        if (rc)
        {
            QCARCAM_ERRORMSG("pthread_attr_init failed");
            return QCARCAM_RET_FAILED;
        }

        /*signal handler thread*/
        rc = pthread_create(&pthread_object, &pthread_attr, &helper_thread, NULL);
        if (rc)
        {
            QCARCAM_ERRORMSG("pthread_create failed");
            return QCARCAM_RET_FAILED;
        }
    }

    pCtxt->drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (pCtxt->drm_fd < 0) {
        QCARCAM_ERRORMSG("/dev/dri/card0 open failed: %d\n", pCtxt->drm_fd);
        return QCARCAM_RET_FAILED;
    }

    pCtxt->gbm = gbm_create_device(pCtxt->drm_fd);
    if (pCtxt->gbm == NULL)
    {
        QCARCAM_ERRORMSG("gbm_create_device failed\n");
        return QCARCAM_RET_FAILED;
    }

    *ctxt = pCtxt;

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
    if (ctxt)
    {
        if(ctxt->gbm ){
            gbm_device_destroy(ctxt->gbm);
            ctxt->gbm = NULL;
        }
        close(ctxt->drm_fd);
        if (!ctxt->params.disable_display)
        {
            eglTerminate(ctxt->egldpy);
            if (ctxt->wl_display->shm)
            {
                wl_shm_destroy(ctxt->wl_display->shm);
            }

            if (ctxt->wl_display->ivi_application)
            {
                ivi_application_destroy(ctxt->wl_display->ivi_application);
                ctxt->wl_display->ivi_application = NULL;
            }

            if (ctxt->wl_display->compositor)
            {
                wl_compositor_destroy(ctxt->wl_display->compositor);
            }

            if (ctxt->wl_display->viewport)
            {
                wl_viewport_destroy(ctxt->wl_display->viewport);
                ctxt->wl_display->viewport = NULL;
            }

            if (ctxt->wl_display->scaler)
            {
                wl_scaler_destroy(ctxt->wl_display->scaler);
                ctxt->wl_display->scaler = NULL;
            }

            if (ctxt->wl_display->viewport_wp)
            {
                wp_viewport_destroy(ctxt->wl_display->viewport_wp);
                ctxt->wl_display->viewport_wp = NULL;
            }

            if (ctxt->wl_display->viewporter)
            {
                wp_viewporter_destroy(ctxt->wl_display->viewporter);
                ctxt->wl_display->viewporter = NULL;
            }

            wl_registry_destroy(ctxt->wl_display->registry);
            wl_display_flush(ctxt->wl_display->display);
            wl_display_disconnect(ctxt->wl_display->display);
            free(ctxt->wl_display);
            ctxt->wl_display = NULL;

        }

        free(ctxt);
    }

    aborted = 1;
    b_post = true;

    MM_Debug_Deinitialize();

    return QCARCAM_RET_OK;
}

///////////////////////////////////////////////////////////////////////////////
/// test_util_init_window
///
/// @brief Initialize new window
///
/// @param ctxt             Pointer to util context
/// @param user_ctxt        Pointer to new window to be initialized
///
/// @return QCARCAM_RET_OK if successful
///////////////////////////////////////////////////////////////////////////////
qcarcam_ret_t test_util_init_window(test_util_ctxt_t *ctxt, test_util_window_t **user_ctxt)
{
    int rc = 0;
    *user_ctxt = NULL;
    test_util_window_t* p_window = NULL;

    p_window = (test_util_window_t*)calloc(1, sizeof(struct test_util_window_t));
    if (!p_window)
    {
        return QCARCAM_RET_NOMEM;
    }

    if (!ctxt->params.disable_display)
    {
        p_window->wl_window = (window_wl *)calloc(1, sizeof(*p_window->wl_window));
        if (!p_window->wl_window)
        {
            free(p_window);
            return QCARCAM_RET_FAILED;
        }
        p_window->commit_frames = 0;
        p_window->wl_window->buffer_index = -1;

        if (0 != init_mutex(p_window))
        {
            free(p_window);
            return QCARCAM_RET_FAILED;
        }
    }

    *user_ctxt = p_window;

    return QCARCAM_RET_OK;
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
    int rc = 0;

    if (!ctxt->params.disable_display && user_ctxt)
    {
        notify_frame_done(user_ctxt);

        if (user_ctxt->wl_window)
        {
            if (user_ctxt->wl_window->shell_surface)
            {
                wl_shell_surface_destroy(user_ctxt->wl_window->shell_surface);
                user_ctxt->wl_window->shell_surface = NULL;
            }

            if (user_ctxt->wl_window->ivi_surface)
            {
                ivi_surface_destroy(user_ctxt->wl_window->ivi_surface);
                user_ctxt->wl_window->ivi_surface = NULL;
            }

            if (user_ctxt->wl_window->surface)
            {
                wl_surface_destroy(user_ctxt->wl_window->surface);
            }

            free(user_ctxt->wl_window);
            user_ctxt->wl_window = NULL;
        }

        rc = uninit_mutex(user_ctxt);
    }

    if (user_ctxt)
        free(user_ctxt);

    return (rc == 0) ? QCARCAM_RET_OK : QCARCAM_RET_FAILED;
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
    if (!ctxt->params.disable_display)
    {
        struct wl_region *region;

        if ( (ctxt->params.enable_c2d && user_ctxt->format != QCARCAM_FMT_UYVY_8) ||
                !ctxt->params.enable_c2d )
        {
            user_ctxt->buffer_size[0] = window_params->buffer_size[0];
            user_ctxt->buffer_size[1] = window_params->buffer_size[1];
            user_ctxt->src_rec.src_width  = user_ctxt->buffer_size[0] * window_params->window_source_size[0];
            user_ctxt->src_rec.src_height = user_ctxt->buffer_size[1] * window_params->window_source_size[1];
            user_ctxt->src_rec.src_x = user_ctxt->buffer_size[0] * window_params->window_source_pos[0];
            user_ctxt->src_rec.src_y = user_ctxt->buffer_size[1] * window_params->window_source_pos[1];
            user_ctxt->wl_window->callback = NULL;
            user_ctxt->wl_window->display = ctxt->wl_display;
            user_ctxt->wl_window->egldpy = ctxt->egldpy;
            user_ctxt->wl_window->width = user_ctxt->buffer_size[0] * window_params->window_size[0];
            user_ctxt->wl_window->height = user_ctxt->buffer_size[1] * window_params->window_size[1];
            user_ctxt->wl_window->fullscreen = 0;
            user_ctxt->wl_window->surface = wl_compositor_create_surface(ctxt->wl_display->compositor);

            if (ctxt->wl_display->ivi_application)
            {
                uint32_t id_ivisurf = getpid() + IVI_SURFACE_ID + input_num;

                user_ctxt->wl_window->ivi_surface = ivi_application_surface_create(
                    ctxt->wl_display->ivi_application,
                    id_ivisurf, user_ctxt->wl_window->surface);

                if (user_ctxt->wl_window->ivi_surface == NULL)
                {
                    QCARCAM_ERRORMSG("Failed to create ivi_client_surface");
                    return QCARCAM_RET_FAILED;
                }
                ivi_surface_add_listener(user_ctxt->wl_window->ivi_surface, &ivi_surface_listener, user_ctxt->wl_window);
                input_num++;
            }
            else if (ctxt->wl_display->shell)
            {
                user_ctxt->wl_window->shell_surface = wl_shell_get_shell_surface(ctxt->wl_display->shell, user_ctxt->wl_window->surface);
                if (user_ctxt->wl_window->shell_surface == NULL)
                {
                    QCARCAM_ERRORMSG("wl_shell_get_shell_surface failed.");
                    return QCARCAM_RET_FAILED;
                }

                if (ctxt->params.enable_di)
                {
                    wl_shell_surface_set_fullscreen(user_ctxt->wl_window->shell_surface, WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE, 0, NULL);
                }
                else
                {
                    wl_shell_surface_set_toplevel(user_ctxt->wl_window->shell_surface);
                }
                wl_shell_surface_add_listener(user_ctxt->wl_window->shell_surface, &shell_surface_listener, NULL);

                user_ctxt->wl_window->callback = wl_surface_frame(user_ctxt->wl_window->surface);
            }
            else
            {
                QCARCAM_ERRORMSG("display->shell NULL.");
            }

            if (ctxt->wl_display->viewporter)
            {
                ctxt->wl_display->viewport_wp = wp_viewporter_get_viewport(ctxt->wl_display->viewporter, user_ctxt->wl_window->surface);
                if (user_ctxt->wl_window->width != user_ctxt->buffer_size[0] || user_ctxt->wl_window->height != user_ctxt->buffer_size[1])
                {
                    wp_viewport_set_destination(ctxt->wl_display->viewport_wp, user_ctxt->wl_window->width, user_ctxt->wl_window->height);
                }
                /* we have to scale to fullscreen manually because of the ratio of one field resolution on BOB 60fps case */
                if (ctxt->params.enable_di == SW_BOB_30FPS || ctxt->params.enable_di == SW_BOB_60FPS)
                {
                    wp_viewport_set_destination(ctxt->wl_display->viewport_wp, ctxt->wl_display->output_width, ctxt->wl_display->output_height);
                }
            }
            else if (ctxt->wl_display->scaler)
            {
                ctxt->wl_display->viewport = wl_scaler_get_viewport(ctxt->wl_display->scaler, user_ctxt->wl_window->surface);
                if (user_ctxt->wl_window->width != user_ctxt->buffer_size[0] || user_ctxt->wl_window->height != user_ctxt->buffer_size[1])
                {
                    wl_viewport_set_destination(ctxt->wl_display->viewport, user_ctxt->wl_window->width, user_ctxt->wl_window->height);
                }
                /* we have to scale to fullscreen manually because of the ratio of one field resolution on BOB 60fps case */
                if (ctxt->params.enable_di == SW_BOB_30FPS || ctxt->params.enable_di == SW_BOB_60FPS)
                {
                    wl_viewport_set_destination(ctxt->wl_display->viewport, ctxt->wl_display->output_width, ctxt->wl_display->output_height);
                }
            }
            wl_surface_damage(user_ctxt->wl_window->surface, 0, 0, user_ctxt->wl_window->width, user_ctxt->wl_window->height);
            region = wl_compositor_create_region(user_ctxt->wl_window->display->compositor);
            wl_region_add(region, 0, 0, user_ctxt->wl_window->width, user_ctxt->wl_window->height);
            wl_surface_set_opaque_region(user_ctxt->wl_window->surface, region);
            wl_region_destroy(region);
        }
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
    unsigned int length = 0;
    int fd;
    struct gbm_bo *bo = NULL;
    int rc = -1;
    int bpp = 4;
    uint32_t align_w;
    uint32_t align_h;
    struct gbm_buf_info bufInfo;

    struct wl_shm_pool *pool;

    user_ctxt->buffer_size[0] = buffers->buffers[0].planes[0].width;
    user_ctxt->buffer_size[1] = buffers->buffers[0].planes[0].height;

    if (buffers->color_fmt == QCARCAM_FMT_UYVY_8)
        user_ctxt->format = buffers->color_fmt;
    user_ctxt->n_buffers = buffers->n_buffers;

    user_ctxt->buffers = (test_util_buffer_t*)calloc(user_ctxt->n_buffers, sizeof(*user_ctxt->buffers));
    if (!user_ctxt->buffers)
    {
        QCARCAM_ERRORMSG("Failed to allocate buffers structure");
        return QCARCAM_RET_NOMEM;
    }

    if (user_ctxt->format == QCARCAM_FMT_UYVY_8)
    {
        struct gbm_buffer *gbm_buf = (gbm_buffer*)calloc(buffers->n_buffers, sizeof(*gbm_buf));
        if (!gbm_buf)
        {
            QCARCAM_ERRORMSG("Failed to allocate gbm_buf");
            return QCARCAM_RET_NOMEM;
        }
        memset(gbm_buf, 0, sizeof(gbm_buffer) * buffers->n_buffers);
        for (int i = 0; i < buffers->n_buffers; i++)
        {
            bo = gbm_bo_create(ctxt->gbm, buffers->buffers[i].planes[0].width, buffers->buffers[i].planes[0].height, GBM_FORMAT_UYVY, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
            if (bo == NULL)
            {
                QCARCAM_ERRORMSG("Failed to gbm_bo_create");
                return QCARCAM_RET_FAILED;
            }
            gbm_buf[i].bo = bo;

            fd = gbm_bo_get_fd(bo);
            if (fd < 0)
            {
                QCARCAM_ERRORMSG("Failed to gbm_bo_get_fd");
                return QCARCAM_RET_FAILED;
            }

            gbm_perform(GBM_PERFORM_GET_BO_ALIGNED_WIDTH, bo, &align_w);
            gbm_perform(GBM_PERFORM_GET_BO_ALIGNED_HEIGHT, bo, &align_h);

            user_ctxt->stride = qcarcam_get_stride(buffers->color_fmt, align_w);

            buffers->buffers[i].n_planes = 1;
            buffers->buffers[i].planes[0].stride = qcarcam_get_stride(QCARCAM_FMT_UYVY_8, align_w);
            buffers->buffers[i].planes[0].size = buffers->buffers[i].planes[0].height * buffers->buffers[i].planes[0].stride;

            bufInfo.width = user_ctxt->buffer_size[0];
            bufInfo.height = user_ctxt->buffer_size[1];
            bufInfo.format = GBM_FORMAT_UYVY;
            gbm_perform(GBM_PERFORM_GET_BUFFER_SIZE_DIMENSIONS, &bufInfo, 0, &align_w, &align_h, &length);

            QCARCAM_INFOMSG("w %u h %u aign_w %u align_h %u stride %u length %u",
                    buffers->buffers[i].planes[0].width,
                    buffers->buffers[i].planes[0].height,
                    align_w, align_h,
                    buffers->buffers[i].planes[0].stride, length);

            gbm_buf[i].fd = fd;
            gbm_buf[i].buflen = length;
            buffers->buffers[i].planes[0].p_buf = (void *)(uintptr_t)fd;
            gbm_buf[i].p_data = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

            gbm_buf[i].state = GBM_BUF_STATE_INIT_BUF;

            user_ctxt->buffers[i].ptr = buffers->buffers[i].planes[0].p_buf;
        }
        user_ctxt->gbm_buf = gbm_buf;

        if (!ctxt->params.enable_c2d && !ctxt->params.disable_display)
        {
            struct gbm_buffer *gbm_buf_wl = (gbm_buffer *)calloc(buffers->n_buffers, sizeof(*gbm_buf_wl));
            if (!gbm_buf_wl)
            {
                QCARCAM_ERRORMSG("Failed to allocate gbmbuf_wl");
                return QCARCAM_RET_NOMEM;
            }
            memset(gbm_buf_wl, 0, sizeof(gbm_buffer) * buffers->n_buffers);
            user_ctxt->gbm_buf_wl = gbm_buf_wl;

            if ((ctxt->params.enable_di == 0) ||
                (ctxt->params.enable_di == SW_BOB_30FPS) ||
                (ctxt->params.enable_di == SW_BOB_60FPS) ||
                ((ctxt->params.enable_di == SW_WEAVE_30FPS) && (user_ctxt->buffer_size[1] == DE_INTERLACE_HEIGHT)))
            {
                for (int i = 0; i < buffers->n_buffers; i++)
                {
#if USE_GBM_BACKEND
                    rc = test_util_create_uyvy_wlbuffer_gbm(ctxt, user_ctxt, i,
                            buffers->buffers[0].planes[0].width,
                            buffers->buffers[0].planes[0].height);
#else
                    rc = test_util_create_uyvy_wlbuffer(ctxt, user_ctxt, i,
                            buffers->buffers[0].planes[0].width,
                            buffers->buffers[0].planes[0].height);
#endif
                    if (rc)
                    {
                        QCARCAM_ERRORMSG("test_util_create_uyvy_wlbuffer failed");
                    }
                }

                QCARCAM_INFOMSG("window buffer %p", user_ctxt);
            }
        }
    }
    else
    {
        if (!ctxt->params.disable_display)
        {
            struct gbm_buffer *gbm_buf_wl = (gbm_buffer *)calloc(buffers->n_buffers, sizeof(*gbm_buf_wl));
            if (!gbm_buf_wl)
            {
                QCARCAM_ERRORMSG("Failed to allocate gbm_buf_wl");
                return QCARCAM_RET_NOMEM;
            }
            memset(gbm_buf_wl, 0, sizeof(gbm_buffer) * buffers->n_buffers);

            for (int i = 0; i < buffers->n_buffers; i++)
            {
                length = user_ctxt->wl_window->width * user_ctxt->wl_window->height * bpp;

                bo = gbm_bo_create(ctxt->gbm, buffers->buffers[i].planes[0].width, buffers->buffers[i].planes[0].height, GBM_FORMAT_UYVY, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
                if(bo == NULL)
                {
                    QCARCAM_ERRORMSG("Failed to gbm_bo_create");
                    return QCARCAM_RET_FAILED;
                }
                gbm_buf_wl[i].bo = bo;

                fd = gbm_bo_get_fd(bo);
                if(fd < 0)
                {
                    QCARCAM_ERRORMSG("Failed to gbm_bo_get_fd");
                    return QCARCAM_RET_FAILED;
                }

                gbm_buf_wl[i].buflen = length;
                gbm_buf_wl[i].fd = fd;
                gbm_buf_wl[i].p_data = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

                pool = wl_shm_create_pool(user_ctxt->wl_window->display->shm, fd, length);
                gbm_buf_wl[i].wl_buf = wl_shm_pool_create_buffer(pool, 0, user_ctxt->wl_window->width, user_ctxt->wl_window->height, user_ctxt->wl_window->width * bpp, support_format);
                wl_buffer_add_listener(gbm_buf_wl[i].wl_buf, &buffer_listener, &gbm_buf_wl[i]);
                wl_shm_pool_destroy(pool);

                user_ctxt->buffers[i].wl_ptr = (void *)(uintptr_t)fd;
            }
            user_ctxt->gbm_buf_wl = gbm_buf_wl;
        }
    }

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
    if (user_ctxt->buffers)
    {
        free(user_ctxt->buffers);
        user_ctxt->buffers = NULL;
    }

    if (user_ctxt->gbm_buf_wl)
    {
        for (int i = 0; i < user_ctxt->n_buffers; i++){
            if ( user_ctxt->gbm_buf_wl[i].wl_buf ){
                wl_buffer_destroy(user_ctxt->gbm_buf_wl[i].wl_buf);
                user_ctxt->gbm_buf_wl[i].wl_buf = NULL;
            }

            if( user_ctxt->gbm_buf_wl[i].p_data ) {
                munmap(user_ctxt->gbm_buf_wl[i].p_data,user_ctxt->gbm_buf_wl[i].buflen);
                user_ctxt->gbm_buf_wl[i].p_data = NULL;
            }

            if( user_ctxt->gbm_buf_wl[i].bo ) {
                gbm_bo_destroy(user_ctxt->gbm_buf_wl[i].bo);
                user_ctxt->gbm_buf_wl[i].bo = NULL;
            }
        }
        free(user_ctxt->gbm_buf_wl);
        user_ctxt->gbm_buf_wl = NULL;
    }

    if (user_ctxt->gbm_buf)
    {
        for (int i = 0; i < user_ctxt->n_buffers; i++){
            if( user_ctxt->gbm_buf[i].p_data ) {
                munmap(user_ctxt->gbm_buf[i].p_data,user_ctxt->gbm_buf[i].buflen);
                user_ctxt->gbm_buf[i].p_data = NULL;
            }

            if( user_ctxt->gbm_buf[i].bo ) {
                gbm_bo_destroy(user_ctxt->gbm_buf[i].bo);
                user_ctxt->gbm_buf[i].bo = NULL;
            }

        }
        free(user_ctxt->gbm_buf);
        user_ctxt->gbm_buf = NULL;
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
qcarcam_ret_t test_util_post_window_buffer(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt,
                                                  unsigned int idx, int *rel_idx, qcarcam_field_t field_type)
{
    b_post = true;
    if (!ctxt->params.disable_display)
    {

      if(user_ctxt->wl_window->buffer_index == idx)
      {
         QCARCAM_DBGMSG("Skip this frame to display");
      }
      else
      {
        if (ctxt->params.enable_di == SW_BOB_30FPS)
        {
            if (field_type == QCARCAM_FIELD_ODD_EVEN)
            {
                // 13 + 240 + 14 + 240
                if (ctxt->wl_display->viewport_wp)
                {
                    wp_viewport_set_source(ctxt->wl_display->viewport_wp, wl_fixed_from_int(0), wl_fixed_from_int(13), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                else if (ctxt->wl_display->viewport)
                {
                    wl_viewport_set_source(ctxt->wl_display->viewport, wl_fixed_from_int(0), wl_fixed_from_int(13), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                post_window(ctxt, user_ctxt, idx, 1);
            }
            else
            {
                // 14 + 240 + 13 + 240
                if (ctxt->wl_display->viewport_wp)
                {
                    wp_viewport_set_source(ctxt->wl_display->viewport_wp, wl_fixed_from_int(0), wl_fixed_from_int(14), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                else if (ctxt->wl_display->viewport)
                {
                    wl_viewport_set_source(ctxt->wl_display->viewport, wl_fixed_from_int(0), wl_fixed_from_int(14), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                post_window(ctxt, user_ctxt, idx, 1);
            }
        }
        else if (ctxt->params.enable_di == SW_BOB_60FPS)
        {
            if (field_type == QCARCAM_FIELD_ODD_EVEN)
            {
                // 13 + 240 + 14 + 240
                if (ctxt->wl_display->viewport_wp)
                {
                    wp_viewport_set_source(ctxt->wl_display->viewport_wp, wl_fixed_from_int(0), wl_fixed_from_int(13), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                else if (ctxt->wl_display->viewport)
                {
                    wl_viewport_set_source(ctxt->wl_display->viewport, wl_fixed_from_int(0), wl_fixed_from_int(13), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                post_window(ctxt, user_ctxt, idx, 1);

                if (ctxt->wl_display->viewport_wp)
                {
                    wp_viewport_set_source(ctxt->wl_display->viewport_wp, wl_fixed_from_int(0), wl_fixed_from_int(267), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                else if (ctxt->wl_display->viewport)
                {
                    wl_viewport_set_source(ctxt->wl_display->viewport, wl_fixed_from_int(0), wl_fixed_from_int(267), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                post_window(ctxt, user_ctxt, idx, 0);
            }
            else
            {
                // 14 + 240 + 13 + 240
                if (ctxt->wl_display->viewport_wp)
                {
                    wp_viewport_set_source(ctxt->wl_display->viewport_wp, wl_fixed_from_int(0), wl_fixed_from_int(14), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                else if (ctxt->wl_display->viewport)
                {
                    wl_viewport_set_source(ctxt->wl_display->viewport, wl_fixed_from_int(0), wl_fixed_from_int(14), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                post_window(ctxt, user_ctxt, idx, 1);

                if (ctxt->wl_display->viewport_wp)
                {
                    wp_viewport_set_source(ctxt->wl_display->viewport_wp, wl_fixed_from_int(0), wl_fixed_from_int(267), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                else if (ctxt->wl_display->viewport)
                {
                    wl_viewport_set_source(ctxt->wl_display->viewport, wl_fixed_from_int(0), wl_fixed_from_int(267), wl_fixed_from_int(720), wl_fixed_from_int(240));
                }
                post_window(ctxt, user_ctxt, idx, 0);
            }
        }
        else
        {
            if (ctxt->wl_display->viewport_wp)
            {
                wp_viewport_set_source(ctxt->wl_display->viewport_wp, wl_fixed_from_int(user_ctxt->src_rec.src_x), wl_fixed_from_int(user_ctxt->src_rec.src_y), wl_fixed_from_int(user_ctxt->src_rec.src_width), wl_fixed_from_int(user_ctxt->src_rec.src_height));
            }
            else if (ctxt->wl_display->viewport)
            {
                wl_viewport_set_source(ctxt->wl_display->viewport, wl_fixed_from_int(user_ctxt->src_rec.src_x), wl_fixed_from_int(user_ctxt->src_rec.src_y), wl_fixed_from_int(user_ctxt->src_rec.src_width), wl_fixed_from_int(user_ctxt->src_rec.src_height));
            }
            post_window(ctxt, user_ctxt, idx, 1);
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
    unsigned char *buf_ptr = NULL;

    numByteToWrite = user_ctxt->stride * user_ctxt->buffer_size[1];

    fp = fopen(filename, "w+b");
    QCARCAM_ERRORMSG("dumping qcarcam frame %s", filename);

    if (0 != fp)
    {
        buf_ptr = (unsigned char*)user_ctxt->gbm_buf[idx].p_data;
        numBytesWritten = fwrite((void *)buf_ptr, sizeof(byte), numByteToWrite, fp);
        fclose(fp);

        if (numBytesWritten != numByteToWrite )
        {
             QCARCAM_ERRORMSG("numByteToWrite %lu, numBytesWritten %lu, failed to write file",numByteToWrite,numBytesWritten);
             return QCARCAM_RET_FAILED;
        }
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
    int qcarcam_idx = di_info->source_buf_idx%di_info->qcarcam_window->n_buffers;
    int display_idx = di_info->source_buf_idx%di_info->display_window->n_buffers;

    buf_ptr->source = (unsigned char*)di_info->qcarcam_window->gbm_buf[qcarcam_idx].p_data;
    buf_ptr->dest = (unsigned char*)di_info->display_window->gbm_buf[display_idx].p_data;
    *stride = di_info->qcarcam_window->stride;
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
    if (user_ctxt)
    {
        user_ctxt->diag = diag;
    }
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
    void *gpuaddr = NULL;

    if (QCARCAM_FMT_UYVY_8 == user_ctxt->format)
    {
        int stride = qcarcam_get_stride(QCARCAM_FMT_UYVY_8, user_ctxt->buffer_size[0]);
        int length = stride * user_ctxt->buffer_size[1];
        length = (length + 4096 - 1) & ~(4096 - 1);


        C2D_STATUS c2d_status;
        C2D_YUV_SURFACE_DEF c2d_yuv_surface_def;
        c2d_yuv_surface_def.format = C2D_COLOR_FORMAT_422_UYVY;
        c2d_yuv_surface_def.width = user_ctxt->buffer_size[0];
        c2d_yuv_surface_def.height = user_ctxt->buffer_size[1];
        c2d_yuv_surface_def.stride0 = stride;
        c2d_yuv_surface_def.plane0 = user_ctxt->gbm_buf[idx].p_data;
        c2d_status = c2dMapAddr((intptr_t)user_ctxt->buffers[idx].ptr, user_ctxt->gbm_buf[idx].p_data, length, 0, KGSL_USER_MEM_TYPE_ION, &gpuaddr);
        c2d_yuv_surface_def.phys0 = gpuaddr;

        c2d_status = c2dCreateSurface(&user_ctxt->buffers[idx].c2d_surface_id,
                                      C2D_SOURCE | C2D_TARGET,
                                      (C2D_SURFACE_TYPE)(C2D_SURFACE_YUV_HOST | C2D_SURFACE_WITH_PHYS),
                                      &c2d_yuv_surface_def);

        if (c2d_status != C2D_STATUS_OK)
        {
            QCARCAM_ERRORMSG("c2dCreateSurface %d buf %d failed %d", 0, idx, c2d_status);
            return QCARCAM_RET_FAILED;
        }
    }
    else
    {
        if (!ctxt->params.disable_display)
        {
            int length = user_ctxt->wl_window->width * user_ctxt->wl_window->height * 4;
            int stride = user_ctxt->buffer_size[0] * 4;

            C2D_STATUS c2d_status;
            C2D_YUV_SURFACE_DEF c2d_yuv_surface_def;
            c2d_yuv_surface_def.format = C2D_COLOR_FORMAT_8888_ARGB;
            c2d_yuv_surface_def.width = user_ctxt->buffer_size[0];
            c2d_yuv_surface_def.height = user_ctxt->buffer_size[1];
            c2d_yuv_surface_def.stride0 = stride;
            c2d_yuv_surface_def.plane0 = user_ctxt->gbm_buf_wl[idx].p_data;
            c2d_status = c2dMapAddr((intptr_t)user_ctxt->buffers[idx].wl_ptr, user_ctxt->gbm_buf_wl[idx].p_data, length, 0, KGSL_USER_MEM_TYPE_ION, &gpuaddr);
            c2d_yuv_surface_def.phys0 = gpuaddr;

            c2d_status = c2dCreateSurface(&user_ctxt->buffers[idx].c2d_surface_id,
                                          C2D_SOURCE | C2D_TARGET,
                                          (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST | C2D_SURFACE_WITH_PHYS),
                                          &c2d_yuv_surface_def);

            if (c2d_status != C2D_STATUS_OK)
            {
                QCARCAM_ERRORMSG("c2dCreateSurface %d buf %d failed %d", 0, idx, c2d_status);
                return QCARCAM_RET_FAILED;
            }
        }
    }
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
qcarcam_ret_t test_util_get_c2d_surface_id(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx, uint32 *surface_id)
{
    if (!surface_id)
    {
        return QCARCAM_RET_BADPARAM;
    }

    *surface_id = user_ctxt->buffers[idx].c2d_surface_id;

    return QCARCAM_RET_OK;
}

static int test_util_create_uyvy_wlbuffer(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx, int w, int h)
{
    int bufferFd = (int)(uintptr_t)user_ctxt->buffers[idx].ptr;
    int offset = 0;
    struct wl_buffer* buffer = NULL;

    EGLImageKHR eglimg;
    PFNEGLCREATEIMAGEKHRPROC create_image;
    PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL create_wlbuf;
    PFNEGLDESTROYIMAGEKHRPROC destroy_image;

    EGLint attribs[] = {
                 EGL_WIDTH, 0,
                 EGL_HEIGHT, 0,
                 EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_UYVY,
                 EGL_DMA_BUF_PLANE0_FD_EXT, 0,
                 EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                 EGL_NONE
    };

    attribs[1] = w;
    attribs[3] = h;
    attribs[7] = bufferFd;
    attribs[9] = offset;

    create_image = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    if (create_image == NULL)
    {
        QCARCAM_ERRORMSG("can't find eglCreateImageKHR");
        return 1;
    }

    create_wlbuf = (PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL) eglGetProcAddress("eglCreateWaylandBufferFromImageWL");
    if (create_wlbuf == NULL)
    {
        QCARCAM_ERRORMSG("can't find eglCreateWaylandBufferFromImageWL");
        return 1;
    }

    destroy_image = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
    if (destroy_image == NULL)
    {
        QCARCAM_ERRORMSG("can't find eglDestroyImageKHR");
        return 1;
    }

    eglimg = create_image(ctxt->egldpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
    if (eglimg == NULL)
    {
        QCARCAM_ERRORMSG("can't create_image %p fd=%d", ctxt->egldpy, bufferFd);
        return 1;
    }

    buffer = create_wlbuf(ctxt->egldpy, eglimg);
    if (buffer == NULL)
    {
        QCARCAM_ERRORMSG("can't create_wlbuf egldpy %p img %p", ctxt->egldpy, eglimg);
        return 1;
    }

    user_ctxt->gbm_buf_wl[idx].wl_buf = buffer;
    wl_buffer_add_listener(buffer, &buffer_listener, &user_ctxt);
    destroy_image(ctxt->egldpy, eglimg);

    return 0;
}

int test_util_create_uyvy_wlbuffer_gbm(test_util_ctxt_t *ctxt, test_util_window_t *user_ctxt, unsigned int idx, int w, int h)
{
#if USE_GBM_BACKEND
    int bufferFd = user_ctxt->gbm_buf[idx].fd;
    int offset = 0;
    struct wl_buffer* buffer = NULL;
    int meta_fd;
    struct gbm_bo *bo = user_ctxt->gbm_buf[idx].bo;
    struct gbm_buffer_params *params = NULL;
    uint32_t flags = GBM_BUFFER_PARAMS_FLAGS_Y_INVERT;
    uint32_t wait_cb_times = 10;
    uint32_t i = 0;

    QCARCAM_INFOMSG("use gbm backend bo %p", bo);
    gbm_perform(GBM_PERFORM_GET_METADATA_ION_FD, bo, &meta_fd);
    if(meta_fd < 0){
        QCARCAM_ERRORMSG("Get bo meta fd failed \n");
        return 1;
    }

    params = gbm_buffer_backend_create_params(user_ctxt->wl_window->display->gbmbuf);
    if( params == NULL )
    {
        QCARCAM_ERRORMSG("gbm_buffer_backend_create_params fail");
        return 1;
    }
    gbm_buffer_params_add_listener(params, &gbm_params_listener, (void*)&user_ctxt->gbm_buf_wl[idx]);
    gbm_buffer_params_create(params,
                                    bufferFd,
                                     meta_fd,
                                     w,
                                     h,
                                     DRM_FORMAT_UYVY,
                                     flags);

    wl_display_roundtrip(ctxt->wl_display->display);

    while( user_ctxt->gbm_buf_wl[idx].state != GBM_BUF_STATE_INIT_WL &&
            i < wait_cb_times )
    {
        usleep(2000);
        ++i;
    }

    if( user_ctxt->gbm_buf_wl[idx].state == GBM_BUF_STATE_INIT_WL &&
        user_ctxt->gbm_buf_wl[idx].wl_buf != NULL)
    {
        wl_buffer_add_listener(user_ctxt->gbm_buf_wl[idx].wl_buf, &buffer_listener, user_ctxt);
    }
    else
    {
        QCARCAM_ERRORMSG("test_util_create_uyvy_wlbuffer_gbm fail");
    }

    QCARCAM_INFOMSG("use gbm backend end wl_buf = %p", user_ctxt->gbm_buf_wl[idx].wl_buf);
#endif
    return 0;
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

