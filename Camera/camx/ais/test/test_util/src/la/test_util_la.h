/* ===========================================================================
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
=========================================================================== */
#ifndef _TEST_UTIL_LA_H_
#define _TEST_UTIL_LA_H_

#include <map>
#include <fcntl.h>
#include <ui/DisplayInfo.h>
#include <ui/Rect.h>
#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gralloc_priv.h>
#include <errno.h>
#include "qcarcam.h"

#ifndef C2D_DISABLED
#include <linux/ion.h>
#include <linux/msm_ion.h>
#include "c2d2.h"
#endif

#define NUM_MAX_DISP_BUFS 3

using namespace android;


typedef struct
{
    int display_id;
    int size[2];
}test_util_display_prop_t;

typedef struct
{
    ANativeWindowBuffer* window_buf;
    int fenceFd;
    int fd;
    void* ptr;
    int size;
    unsigned int c2d_surface_id;
    int is_dequeud;
}test_util_buffer_t;

struct test_util_ctxt_t
{
    test_util_ctxt_params_t params;

    test_util_display_prop_t *display_property;
    int screen_ndisplays;
    sp<SurfaceComposerClient> m_surfaceComposer;
    DisplayInfo m_displayInfo;
};

typedef struct
{
    int size[2];
    int pos[2];
    int ssize[2];
    int spos[2];
    int visibility;
}test_util_window_param_cpy_t;

struct test_util_window_t
{
    char winid[64];
    ANativeWindow* nativeWindow;
    sp<SurfaceControl> m_surfaceControl;
    test_util_window_param_cpy_t params;

    /*buffers*/
    test_util_buffer_t* buffers;
    int n_buffers;

    int buffer_size[2];
    int stride;
    int native_format;
    void **pbuf;

    int min_num_buffers;
    sp<GraphicBuffer> *gfx_bufs;

    test_util_diag_t* diag;
};

#endif
