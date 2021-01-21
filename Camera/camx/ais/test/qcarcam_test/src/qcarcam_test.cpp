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
#include <signal.h>
#include <sys/select.h>

#ifndef C2D_DISABLED
#include "c2d2.h"
#endif
#include "qcarcam.h"

#include "test_util.h"

#define NUM_MAX_CAMERAS 16
#define NUM_MAX_DISP_BUFS 3
#define PAUSE_RESUME_USLEEP 1000000
#define START_STOP_USLEEP 1000000
#define QCARCAM_TEST_DEFAULT_GET_FRAME_TIMEOUT 500000000
#define DEFAULT_PRINT_DELAY_SEC 10
#define SIGNAL_CHECK_DELAY 33333;
#define CSI_ERR_CHECK_DELAY 100000;
#define NS_TO_MS 0.000001F

#define QCARCAM_TEST_INPUT_INJECTION 11
#define BUFSIZE 10

#define SIGWAIT_TIMEOUT_MS 100
#define TIMER_THREAD_USLEEP 1000000

#if defined(__INTEGRITY)
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif
extern "C" const Value __PosixServerPriority = CAM_POSIXSERVER_PRIORITY;
#endif


typedef enum
{
    QCARCAM_TEST_BUFFERS_OUTPUT = 0,
    QCARCAM_TEST_BUFFERS_DISPLAY,
    QCARCAM_TEST_BUFFERS_INPUT,
    QCARCAM_TEST_BUFFERS_MAX
} qcarcam_test_buffers_t;

typedef struct
{
    qcarcam_buffers_t p_buffers;

    qcarcam_color_fmt_t format;
    int n_buffers;
    unsigned int width;
    unsigned int height;
} qcarcam_buffers_param_t;

typedef struct
{
    void* thread_handle;
    int idx;
    int query_inputs_idx;

    /*qcarcam context*/
    qcarcam_hndl_t qcarcam_context;
    qcarcam_input_desc_t qcarcam_input_id;

    qcarcam_buffers_param_t buffers_param[QCARCAM_TEST_BUFFERS_MAX];

    qcarcam_buffers_t p_buffers_output;
    qcarcam_buffers_t p_buffers_disp;
    qcarcam_buffers_t p_buffers_input;

    qcarcam_res_t resolution;

    unsigned long long int frame_timeout;
    int use_event_callback;

    /* test util objects */
    test_util_ctxt_t *test_util_ctxt;
    test_util_window_t *qcarcam_window;
    test_util_window_t *display_window;
    test_util_window_param_t window_params;

    /* buffer management tracking */
    int buf_idx_qcarcam;
    int prev_buf_idx_qcarcam;

    int buf_idx_disp;
    int prev_buf_idx_disp;

    /* diag */
    int frameCnt;
    int prev_frame;
    int is_running;
    bool is_first_count;
    bool is_signal_lost;
    bool is_csi_error;
    int is_injection;
    test_util_diag_t diag;

    /* Exposure values */
    float exp_time;
    float gain;
    bool manual_exposure;

    /* frame rate parameters */
    qcarcam_frame_rate_t frame_rate_config;

    bool is_buf_dequeued[QCARCAM_MAX_NUM_BUFFERS];

    bool skip_post_display;
    qcarcam_field_t field_type_previous;

    bool signal_lost;
    int csi_err_cnt;
} qcarcam_test_input_t;

typedef struct
{
    int numInputs;
    int started_stream_cnt;

    /* 0 : post buffers directly to screen
     * 1 : blit buffers to display buffers through c2d
     */
    int enable_c2d;
#ifndef C2D_DISABLED
    pthread_mutex_t mutex_c2d;
#endif


    int dumpFrame;
    int enablePauseResume;
    int enableStartStop;
    int multithreaded;
    int enableStats;
    int enableMenuMode;
    int enableBridgeErrorDetect;
    int enableCSIErrorDetect;
    int enableRetry;
    int gpioNumber;
    int gpioMode;

    int exitSeconds;

    int fps_print_delay;
    int vis_value;

    /*abort condition*/
    pthread_mutex_t mutex_abort;
    pthread_cond_t cond_abort;
    pthread_mutex_t mutex_csi_err;
    pthread_mutex_t mutex_start_cnt;

    unsigned long t_start;
    unsigned long t_before;
    unsigned long t_after;
    int enable_deinterlace;
} qcarcam_test_ctxt_t;

///////////////////////////////
/// STATICS
///////////////////////////////
static qcarcam_test_input_t g_test_inputs[NUM_MAX_CAMERAS] = {};

static qcarcam_test_ctxt_t gCtxt = {};

char g_filename[128] = "qcarcam_config.xml";

static volatile int g_aborted = 0;

static const int exceptsigs[] = {
    SIGCHLD, SIGIO, SIGURG, SIGWINCH,
    SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGCONT,
    -1,
};

static void initialize_qcarcam_test_ctxt(void)
{
    gCtxt.numInputs = 0;
    gCtxt.started_stream_cnt = 0;

    gCtxt.enable_c2d = 0;
#ifndef C2D_DISABLED
    pthread_mutex_init(&gCtxt.mutex_c2d, NULL);
#endif

    gCtxt.dumpFrame = 0;
    gCtxt.enablePauseResume = 0;
    gCtxt.enableStartStop = 0;
    gCtxt.multithreaded = 1;
    gCtxt.enableStats = 1;
    gCtxt.enableMenuMode = 1;
    gCtxt.enableBridgeErrorDetect = 1;
    gCtxt.enableCSIErrorDetect = 0;
    gCtxt.enableRetry = 0;

    gCtxt.exitSeconds = 0;
    gCtxt.gpioNumber = 0;
    gCtxt.vis_value = 1;

    gCtxt.fps_print_delay = DEFAULT_PRINT_DELAY_SEC;

    pthread_mutex_init(&gCtxt.mutex_abort, NULL);
    pthread_cond_init(&gCtxt.cond_abort, NULL);
    pthread_mutex_init(&gCtxt.mutex_csi_err, NULL);
    pthread_mutex_init(&gCtxt.mutex_start_cnt, NULL);

    gCtxt.t_start = 0;
    gCtxt.t_before = 0;
    gCtxt.t_after = 0;
    gCtxt.enable_deinterlace = 0;
}

void display_valid_stream_ids()
{
    int i;

    printf("Valid stream ids are\n");
    printf("========================\n");
    printf("Camera id   stream id\n");
    printf("========================\n");
    for (i = 0; i < gCtxt.numInputs; ++i)
    {
        qcarcam_test_input_t *input_ctxt = &g_test_inputs[i];
        printf("%d            %d\n", input_ctxt->qcarcam_input_id, i);
    }
    printf("========================\n");
}

/**
 * Returns user entered stream idx
 */
int get_input_stream_id()
{
    int stream_id = gCtxt.numInputs;
    char buf[BUFSIZE];
    char *p = NULL;

    display_valid_stream_ids();

    printf("Enter stream id\n");

    if (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        stream_id = strtol(buf, &p, 10);
    }

    return stream_id;
}

void get_input_exposure(qcarcam_exposure_config_t* exposure_config)
{
    if (exposure_config != NULL)
    {
        char buf[BUFSIZE];

        printf("Enter camera exposure time value\n");
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            exposure_config->exposure_time = strtof(buf, NULL);
        }

        printf("Enter camera gain value\n");
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            exposure_config->gain = strtof(buf, NULL);
        }
    }
}

void get_input_framerate(qcarcam_frame_rate_t* frame_rate_config)
{
    if (frame_rate_config != NULL)
    {
        printf("\n ====================================================== \n");
        printf(" '0'...Keep all frames          '1'...Keep every 2 frames \n");
        printf(" '2'...Keep every 3 frames      '3'...Keep every 4 frames \n");
        printf(" '4'...Drop all frames          '5'...Raw period pattern mode \n");
        printf("\n ====================================================== \n");
        printf(" Enter camera framerate mode\n");

        int frame_drop_mode = 0;
        int frame_drop_period = 0;
        int frame_drop_pattern = 0;

        char buf[BUFSIZE];

        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            frame_drop_mode = strtol(buf, NULL, 10);
        }

        if (QCARCAM_FRAMEDROP_MANUAL == frame_drop_mode)
        {
            printf("Enter period value, maximal 32\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                frame_drop_period = strtol(buf, NULL, 10);
            }
            printf("Enter pattern hex value, e.g. 0x23\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                frame_drop_pattern = strtol(buf, NULL, 16);
            }
        }
        frame_rate_config->frame_drop_mode = (qcarcam_frame_drop_mode_t)frame_drop_mode;
        frame_rate_config->frame_drop_period = (unsigned char)frame_drop_period;
        frame_rate_config->frame_drop_pattern = frame_drop_pattern;
    }
}

int qcarcam_test_get_time(unsigned long *pTime)
{
    struct timespec time;
    unsigned long msec;

    if (clock_gettime(CLOCK_MONOTONIC, &time) == -1)
    {
        QCARCAM_ERRORMSG("Clock gettime failed");
        return 1;
    }
    msec = ((unsigned long)time.tv_sec * 1000) + (((unsigned long)time.tv_nsec / 1000) / 1000);
    *pTime = msec;

    return 0;
}

void qcarcam_test_get_frame_rate(unsigned long *timer1)
{
    float average_fps;
    int input_idx, frames_counted;
    unsigned long timer2;

    qcarcam_test_get_time(&timer2);

    for (input_idx = 0; input_idx < gCtxt.numInputs; ++input_idx)
    {
        if (g_test_inputs[input_idx].is_running)
        {
            if (!g_test_inputs[input_idx].is_first_count)
            {
                g_test_inputs[input_idx].prev_frame = g_test_inputs[input_idx].frameCnt;
                g_test_inputs[input_idx].is_first_count = 1;
            }
            else
            {
                frames_counted = g_test_inputs[input_idx].frameCnt - g_test_inputs[input_idx].prev_frame;
                average_fps = frames_counted / ((timer2 - *timer1) / 1000);
                printf("Average FPS: %.2f input_id: %d idx: %d\n ", average_fps, (int)g_test_inputs[input_idx].qcarcam_input_id, g_test_inputs[input_idx].idx);
                g_test_inputs[input_idx].prev_frame = g_test_inputs[input_idx].frameCnt;
            }
        }
    }
    fflush(stdout);
    qcarcam_test_get_time(timer1);
}

static int test_signal_loss(bool *signal_lost, bool *signal_lost_check, unsigned int idx)
{
    if (*signal_lost != *signal_lost_check)
    {
        // Check if signal status has changed
        *signal_lost_check = *signal_lost;
        g_test_inputs[idx].is_signal_lost = *signal_lost_check;
    }
    else
    {
        if (*signal_lost_check == 1)
        {
            // Posting empty frame to display
            test_util_post_window_buffer(g_test_inputs[idx].test_util_ctxt,
                g_test_inputs[idx].qcarcam_window,
                g_test_inputs[idx].p_buffers_output.n_buffers,
                NULL,
                g_test_inputs[idx].field_type_previous);
        }
    }
    return 0;
}

static int check_signal_loss_thread(void *arg)
{
    pthread_detach(pthread_self());

    bool signal_lost[NUM_MAX_CAMERAS] = {};
    bool signal_lost_check[NUM_MAX_CAMERAS] = {};
    unsigned int signal_check_delay_us = SIGNAL_CHECK_DELAY; // 33 milliseconds

    while (!g_aborted)
    {
        // Check if signal status has changed
        for (int i = 0; i < gCtxt.numInputs; i++)
        {
            signal_lost[i] = g_test_inputs[i].signal_lost;
            test_signal_loss(&signal_lost[i], &signal_lost_check[i], i);
        }
        usleep(signal_check_delay_us);
    }
    return 0;
}

static int test_csi_error_check(volatile int *csi_err_cnt, volatile int *csi_err_cnt_prev, unsigned int idx)
{
    int ret = 0;
    qcarcam_test_input_t *input_ctxt = &g_test_inputs[idx];

    pthread_mutex_lock(&gCtxt.mutex_csi_err);

    if (*csi_err_cnt != *csi_err_cnt_prev)
    {
        // csi error happens, check again next loop
        *csi_err_cnt_prev = *csi_err_cnt;
        input_ctxt->is_csi_error = 1;
    }
    else
    {
        // no new csi error comes, can re-start
        if (input_ctxt->is_csi_error)
        {
            if (input_ctxt->is_running == 1)
            {
                QCARCAM_ERRORMSG("Input %d already running, return", input_ctxt->qcarcam_input_id);
                pthread_mutex_unlock(&gCtxt.mutex_csi_err);
                return ret;
            }

            input_ctxt->is_running = 1;
            ret = qcarcam_resume(input_ctxt->qcarcam_context);
            if (ret == QCARCAM_RET_OK) {
                QCARCAM_ERRORMSG("Client %d Input %d qcarcam_resume successfully", idx, input_ctxt->qcarcam_input_id);
                input_ctxt->signal_lost = 0;
            }
            else
            {
                input_ctxt->is_running = 0;
                QCARCAM_ERRORMSG("Client %d Input %d qcarcam_resume failed: %d", idx, input_ctxt->qcarcam_input_id, ret);
            }

            input_ctxt->is_csi_error = 0;
        }
    }

    pthread_mutex_unlock(&gCtxt.mutex_csi_err);

    return ret;
}

static int check_csi_error_thread(void *arg)
{
    pthread_detach(pthread_self());

    volatile int csi_error_prev[NUM_MAX_CAMERAS] = { 0 };
    unsigned int error_check_delay_us = CSI_ERR_CHECK_DELAY; // 100 milliseconds

    while (!g_aborted)
    {
        // Check if csi error continues or not
        for (int i = 0; i < gCtxt.numInputs; i++)
        {
            test_csi_error_check(&g_test_inputs[i].csi_err_cnt, &csi_error_prev[i], i);
        }
        usleep(error_check_delay_us);
    }
    return 0;
}

static int framerate_thread(void *arg)
{
    pthread_detach(pthread_self());

    unsigned int fps_print_delay_us = gCtxt.fps_print_delay * 1000000;
    unsigned long timer1;
    qcarcam_test_get_time(&timer1);

    while (!g_aborted)
    {
        qcarcam_test_get_frame_rate(&timer1);
        usleep(fps_print_delay_us);
    }
    return 0;
}

static void abort_test(void)
{
    QCARCAM_ERRORMSG("Aborting test");
    pthread_mutex_lock(&gCtxt.mutex_abort);
    g_aborted = 1;
    pthread_cond_broadcast(&gCtxt.cond_abort);
    pthread_mutex_unlock(&gCtxt.mutex_abort);
}

static int timer_thread(void *arg)
{
    pthread_detach(pthread_self());

    unsigned long timer_start;
    unsigned long timer_test;
    qcarcam_test_get_time(&timer_start);
    while (!g_aborted)
    {
        qcarcam_test_get_time(&timer_test);
        if ((timer_test - timer_start) >= ((unsigned long)gCtxt.exitSeconds * 1000))
        {
            QCARCAM_ALWZMSG("TEST Aborted after running for %d secs successfully!", gCtxt.exitSeconds);
            abort_test();
            break;
        }
        usleep(TIMER_THREAD_USLEEP);
    }
    return 0;
}

static int signal_thread(void *arg)
{
    sigset_t sigset;
    struct timespec timeout;
    int i;

    pthread_detach(pthread_self());

    sigfillset(&sigset);
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    for (i = 0; exceptsigs[i] != -1; i++)
    {
        sigdelset(&sigset, exceptsigs[i]);
    }

    timeout.tv_sec = 0;
    timeout.tv_nsec = SIGWAIT_TIMEOUT_MS * 1000000;

    while (!g_aborted)
    {
        if (sigtimedwait(&sigset, NULL, &timeout) > 0)
        {
            abort_test();
            break;
        }
    }
    return 0;
}

static void process_deinterlace(qcarcam_test_input_t *input_ctxt, qcarcam_field_t field_type, int di_method)
{
    test_util_sw_di_t di_info;

    di_info.qcarcam_window = input_ctxt->qcarcam_window;
    di_info.display_window = input_ctxt->display_window;
    di_info.source_buf_idx = input_ctxt->frameCnt;
    di_info.field_type = field_type;

    switch (di_method) {
    case SW_WEAVE_30FPS:
        /* sw weave 30fps method deinterlacing */
        test_util_di_sw_weave_30fps(&di_info, test_util_get_buf_ptr);
        break;
    case SW_BOB_30FPS:
    case SW_BOB_60FPS:
        /* needn't process field into new buffer, display each field in post_window_buffer directly */
        break;
    default:
        /* unsupported deinterlacing method */
        QCARCAM_ERRORMSG("Unknown deinterlacing method");
        break;
    }
}

/**
 * Function to retrieve frame from qcarcam and increase frame_counter
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_get_frame(qcarcam_test_input_t *input_ctxt)
{
    qcarcam_ret_t ret;
    qcarcam_frame_info_t frame_info;
    ret = qcarcam_get_frame(input_ctxt->qcarcam_context, &frame_info, input_ctxt->frame_timeout, 0);
    if (ret == QCARCAM_RET_TIMEOUT)
    {
        QCARCAM_ERRORMSG("qcarcam_get_frame timeout context 0x%p ret %d\n", input_ctxt->qcarcam_context, ret);
        input_ctxt->signal_lost = 1;
        return -1;
    }

    if (QCARCAM_RET_OK != ret)
    {
        QCARCAM_ERRORMSG("Get frame context 0x%p ret %d\n", input_ctxt->qcarcam_context, ret);
        return -1;
    }

    if (frame_info.idx >= input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers)
    {
        QCARCAM_ERRORMSG("Get frame context 0x%p ret invalid idx %d\n", input_ctxt->qcarcam_context, frame_info.idx);
        return -1;
    }

    if (input_ctxt->frameCnt == 0)
    {
        unsigned long t_to;
        qcarcam_test_get_time(&t_to);
        ais_log_kpi(AIS_EVENT_KPI_CLIENT_FIRST_FRAME);
        input_ctxt->field_type_previous = QCARCAM_FIELD_UNKNOWN;
        printf("Success\n");
        fflush(stdout);
        QCARCAM_ALWZMSG("Get First Frame input_id %d buf_idx %i after : %lu ms, field type: %d\n", (int)input_ctxt->qcarcam_input_id, input_ctxt->buf_idx_qcarcam, (t_to - gCtxt.t_start), frame_info.field_type);
        if (gCtxt.enable_deinterlace)
        {
            if (frame_info.field_type == QCARCAM_FIELD_UNKNOWN)
                input_ctxt->skip_post_display = 1;
            else
                input_ctxt->field_type_previous = frame_info.field_type;
        }
    }
    else
    {
        if ((input_ctxt->field_type_previous != frame_info.field_type) && gCtxt.enable_deinterlace)
        {
            input_ctxt->skip_post_display = 0;
            QCARCAM_ERRORMSG("Field type changed: %d -> %d @frame_%d\n", input_ctxt->field_type_previous, frame_info.field_type, frame_info.seq_no);
            if (frame_info.field_type == QCARCAM_FIELD_UNKNOWN)
                frame_info.field_type = input_ctxt->field_type_previous;
            else
                input_ctxt->field_type_previous = frame_info.field_type;
        }
    }

    input_ctxt->buf_idx_qcarcam = frame_info.idx;
    input_ctxt->signal_lost = 0;

    input_ctxt->diag.frame_generate_time[TEST_PREV_BUFFER] = input_ctxt->diag.frame_generate_time[TEST_CUR_BUFFER];
    input_ctxt->diag.frame_generate_time[TEST_CUR_BUFFER] = frame_info.timestamp * NS_TO_MS;

    QCARCAM_DBGMSG("context:%d frameCnt:%d idx:%d, timestamp: %llu, sof qtimer timestamp: %llu", input_ctxt->idx, input_ctxt->frameCnt, frame_info.idx, frame_info.timestamp, frame_info.sof_qtimestamp);

    if (gCtxt.enable_deinterlace)
        process_deinterlace(input_ctxt, frame_info.field_type, gCtxt.enable_deinterlace);

    input_ctxt->is_buf_dequeued[input_ctxt->buf_idx_qcarcam] = 1;
    input_ctxt->frameCnt++;

    return 0;
}
/**
 * Function to post new frame to display. May also do color conversion and frame dumps.
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_post_to_display(qcarcam_test_input_t *input_ctxt)
{
    qcarcam_ret_t ret;
    /**********************
     * Composite to display
     ********************** */
    QCARCAM_DBGMSG("Post Frame before buf_idx %i", input_ctxt->buf_idx_qcarcam);
    /**********************
     * Dump raw if necessary
     ********************** */
    if (0 != gCtxt.dumpFrame)
    {
        if (0 == input_ctxt->frameCnt % gCtxt.dumpFrame)
        {
            snprintf(g_filename, sizeof(g_filename), DEFAULT_DUMP_LOCATION "frame_%d_%i.raw", input_ctxt->idx, input_ctxt->frameCnt);
            test_util_dump_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->qcarcam_window, input_ctxt->buf_idx_qcarcam, g_filename);
        }
    }
    /**********************
     * Color conversion if necessary
     ********************** */
    if (!gCtxt.enable_c2d)
    {
        /**********************
         * Post to screen
         ********************** */
        QCARCAM_DBGMSG("Post Frame %d", input_ctxt->buf_idx_qcarcam);
        if (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace != SW_BOB_30FPS && gCtxt.enable_deinterlace != SW_BOB_60FPS))
        {
            ret = test_util_post_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->display_window, input_ctxt->buf_idx_disp, &input_ctxt->prev_buf_idx_qcarcam, input_ctxt->field_type_previous);
            input_ctxt->buf_idx_disp++;
            input_ctxt->buf_idx_disp %= input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers;
        }
        else
        {
            ret = test_util_post_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->qcarcam_window, input_ctxt->buf_idx_qcarcam, &input_ctxt->prev_buf_idx_qcarcam, input_ctxt->field_type_previous);
        }
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_post_window_buffer failed");
        }
    }
#ifndef C2D_DISABLED
    else
    {
        //for now always go through c2d conversion instead of posting directly to  testsince gles composition cannot handle
        // uyvy buffers for now.
        QCARCAM_DBGMSG("%d converting through c2d %d -> %d", input_ctxt->idx, input_ctxt->buf_idx_qcarcam, input_ctxt->buf_idx_disp);

        C2D_STATUS c2d_status;
        C2D_OBJECT c2dObject;
        memset(&c2dObject, 0x0, sizeof(C2D_OBJECT));
        unsigned int target_id;
        ret = test_util_get_c2d_surface_id(input_ctxt->test_util_ctxt, input_ctxt->qcarcam_window, input_ctxt->buf_idx_qcarcam, &c2dObject.surface_id);
        ret = test_util_get_c2d_surface_id(input_ctxt->test_util_ctxt, input_ctxt->display_window, input_ctxt->buf_idx_disp, &target_id);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_get_c2d_surface_id failed");
        }

        pthread_mutex_lock(&gCtxt.mutex_c2d);
        c2d_status = c2dDraw(target_id, C2D_TARGET_ROTATE_0, 0x0, 0, 0, &c2dObject, 1);

        c2d_ts_handle c2d_timestamp;
        if (c2d_status == C2D_STATUS_OK)
        {
            c2d_status = c2dFlush(target_id, &c2d_timestamp);
        }
        pthread_mutex_unlock(&gCtxt.mutex_c2d);

        if (c2d_status == C2D_STATUS_OK)
        {
            c2d_status = c2dWaitTimestamp(c2d_timestamp);
        }

        QCARCAM_DBGMSG("c2d conversion finished");

        if (c2d_status != C2D_STATUS_OK)
        {
            QCARCAM_ERRORMSG("c2d conversion failed with error %d", c2d_status);
        }
        /**********************
         * Dump if necessary
         ********************** */
        if (0 != gCtxt.dumpFrame)
        {
            if (0 == input_ctxt->frameCnt % gCtxt.dumpFrame)
            {
                snprintf(g_filename, sizeof(g_filename), DEFAULT_DUMP_LOCATION "frame_display_%d_%i.raw", input_ctxt->idx, input_ctxt->frameCnt);
                test_util_dump_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->display_window, input_ctxt->buf_idx_disp, g_filename);
            }
        }
        /**********************
         * Post to screen
         ********************** */
        QCARCAM_DBGMSG("Post Frame %d", input_ctxt->buf_idx_disp);
        ret = test_util_post_window_buffer(input_ctxt->test_util_ctxt, input_ctxt->display_window, input_ctxt->buf_idx_disp, &input_ctxt->prev_buf_idx_qcarcam, input_ctxt->field_type_previous);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_post_window_buffer failed");
        }

        input_ctxt->buf_idx_disp++;
        input_ctxt->buf_idx_disp %= input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers;
    }
#endif
    QCARCAM_DBGMSG("Post Frame after buf_idx %i", input_ctxt->buf_idx_qcarcam);

    return 0;
}
/**
 * Release frame back to qcarcam
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_release_frame(qcarcam_test_input_t *input_ctxt)
{
    qcarcam_ret_t ret;
    if (input_ctxt->prev_buf_idx_qcarcam >= 0 && input_ctxt->prev_buf_idx_qcarcam < input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers)
    {
        if (input_ctxt->is_buf_dequeued[input_ctxt->prev_buf_idx_qcarcam])
        {
            ret = qcarcam_release_frame(input_ctxt->qcarcam_context, input_ctxt->prev_buf_idx_qcarcam);
            if (QCARCAM_RET_OK != ret)
            {
                QCARCAM_ERRORMSG("qcarcam_release_frame() %d failed", input_ctxt->prev_buf_idx_qcarcam);
                return -1;
            }
            input_ctxt->is_buf_dequeued[input_ctxt->prev_buf_idx_qcarcam] = 0;
        }
        else
        {
            QCARCAM_ERRORMSG("qcarcam_release_frame() skipped since buffer %d not dequeued", input_ctxt->prev_buf_idx_qcarcam);
        }
    }
    else
    {
        QCARCAM_ERRORMSG("qcarcam_release_frame() skipped");
    }

#ifdef ENABLE_INJECTION_SUPPORT
    //Requeue input buffer here for now...
    if (input_ctxt->is_injection)
    {
        qcarcam_param_value_t param;
        param.uint_value = 0; //TODO: fix this to be appropriate buffer
        qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_INJECTION_START, &param);
    }
#endif

    input_ctxt->prev_buf_idx_qcarcam = input_ctxt->buf_idx_qcarcam;
    return 0;
}

/**
 * Function to handle routine of fetching, displaying, and releasing frames when one is available
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
static int qcarcam_test_handle_new_frame(qcarcam_test_input_t *input_ctxt)
{
    qcarcam_ret_t ret;

    if (qcarcam_test_get_frame(input_ctxt))
    {
        /*if we fail to get frame, we silently continue...*/
        return 0;
    }

    if (gCtxt.enablePauseResume && 0 == (input_ctxt->frameCnt % gCtxt.enablePauseResume))
    {
        QCARCAM_INFOMSG("pause...");
        qcarcam_test_release_frame(input_ctxt);
        input_ctxt->is_running = 0;
        ret = qcarcam_pause(input_ctxt->qcarcam_context);
        if (ret == QCARCAM_RET_OK)
        {
            usleep(PAUSE_RESUME_USLEEP);
            QCARCAM_INFOMSG("resume...");
            input_ctxt->is_running = 1;
            ret = qcarcam_resume(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                input_ctxt->is_running = 0;
                QCARCAM_ERRORMSG("resume failed!");
            }
        }
        else
        {
            QCARCAM_ERRORMSG("pause failed!");
        }
        return ret;
    }
    else if (gCtxt.enableStartStop && 0 == (input_ctxt->frameCnt % gCtxt.enableStartStop))
    {
        QCARCAM_INFOMSG("stop...");
        qcarcam_test_release_frame(input_ctxt);
        input_ctxt->is_running = 0;
        ret = qcarcam_stop(input_ctxt->qcarcam_context);
        input_ctxt->prev_buf_idx_qcarcam = -1;
        memset(&input_ctxt->is_buf_dequeued, 0x0, sizeof(input_ctxt->is_buf_dequeued));
        if (ret == QCARCAM_RET_OK)
        {
            usleep(START_STOP_USLEEP);
            QCARCAM_INFOMSG("s_buffers...");
            ret = qcarcam_s_buffers(input_ctxt->qcarcam_context, &input_ctxt->p_buffers_output);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("s_buffers failed!");
            }

            QCARCAM_INFOMSG("start...");
            input_ctxt->is_running = 1;
            ret = qcarcam_start(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                input_ctxt->is_running = 0;
                QCARCAM_ERRORMSG("start failed!");
            }
        }
        else
        {
            QCARCAM_ERRORMSG("stop failed!");
        }
        return ret;
    }

    if (!input_ctxt->skip_post_display)
        qcarcam_test_post_to_display(input_ctxt);

    if (qcarcam_test_release_frame(input_ctxt))
        return -1;

    return 0;
}

static int qcarcam_test_handle_input_signal(qcarcam_test_input_t *input_ctxt, qcarcam_input_signal_t signal_type)
{
    qcarcam_ret_t ret = QCARCAM_RET_OK;

    switch (signal_type) {
    case QCARCAM_INPUT_SIGNAL_LOST:
        QCARCAM_ERRORMSG("LOST: idx: %d, input: %d", input_ctxt->idx, input_ctxt->qcarcam_input_id);

        if (input_ctxt->is_running == 0) {
            QCARCAM_ERRORMSG("Input %d already stop, break", input_ctxt->qcarcam_input_id);
            break;
        }

        input_ctxt->is_running = 0;
        ret = qcarcam_stop(input_ctxt->qcarcam_context);
        if (ret == QCARCAM_RET_OK) {
            QCARCAM_ERRORMSG("Input %d qcarcam_stop successfully", input_ctxt->qcarcam_input_id);
        }
        else
        {
            QCARCAM_ERRORMSG("Input %d qcarcam_stop failed: %d !", input_ctxt->qcarcam_input_id, ret);
        }

        input_ctxt->prev_buf_idx_qcarcam = -1;
        memset(&input_ctxt->is_buf_dequeued, 0x0, sizeof(input_ctxt->is_buf_dequeued));
        input_ctxt->signal_lost = 1;

        break;
    case QCARCAM_INPUT_SIGNAL_VALID:
        QCARCAM_ERRORMSG("VALID: idx: %d, input: %d", input_ctxt->idx, input_ctxt->qcarcam_input_id);

        if (input_ctxt->is_running == 1) {
            QCARCAM_ERRORMSG("Input %d already running, break", input_ctxt->qcarcam_input_id);
            break;
        }

        ret = qcarcam_s_buffers(input_ctxt->qcarcam_context, &input_ctxt->p_buffers_output);
        if (ret == QCARCAM_RET_OK)
            QCARCAM_ERRORMSG("Input %d qcarcam_s_buffers successfully", input_ctxt->qcarcam_input_id);
        else
            QCARCAM_ERRORMSG("Input %d qcarcam_s_buffers failed: %d", input_ctxt->qcarcam_input_id, ret);

        input_ctxt->is_running = 1;
        ret = qcarcam_start(input_ctxt->qcarcam_context);
        if (ret == QCARCAM_RET_OK) {
            QCARCAM_ERRORMSG("Input %d qcarcam_start successfully", input_ctxt->qcarcam_input_id);
            input_ctxt->signal_lost = 0;
        }
        else
        {
            input_ctxt->is_running = 0;
            QCARCAM_ERRORMSG("Input %d qcarcam_start failed: %d", input_ctxt->qcarcam_input_id, ret);
        }

        break;
    default:
        QCARCAM_ERRORMSG("Unknown Event type: %d", signal_type);
        break;
    }

    return ret;
}

static int qcarcam_test_handle_csi_error(qcarcam_test_input_t *input_ctxt)
{
    qcarcam_ret_t ret = QCARCAM_RET_OK;

    QCARCAM_ERRORMSG("CSI error: client idx - %d, input id - %d", input_ctxt->idx, input_ctxt->qcarcam_input_id);

    pthread_mutex_lock(&gCtxt.mutex_csi_err);

    input_ctxt->signal_lost = 1;
    input_ctxt->csi_err_cnt++;

    if (input_ctxt->is_running == 0) {
        QCARCAM_ERRORMSG("Input %d already stop, return", input_ctxt->qcarcam_input_id);
        pthread_mutex_unlock(&gCtxt.mutex_csi_err);
        return ret;
    }

    input_ctxt->is_running = 0;

    ret = qcarcam_pause(input_ctxt->qcarcam_context);
    if (ret == QCARCAM_RET_OK) {
        QCARCAM_ERRORMSG("Client %d Input %d qcarcam_pause successfully", input_ctxt->idx, input_ctxt->qcarcam_input_id);
    }
    else
    {
        QCARCAM_ERRORMSG("Client %d Input %d qcarcam_pause failed: %d !", input_ctxt->idx, input_ctxt->qcarcam_input_id, ret);
    }

    pthread_mutex_unlock(&gCtxt.mutex_csi_err);

    return ret;
}

/**
 * Qcarcam event callback function
 * @param hndl
 * @param event_id
 * @param p_payload
 */
static void qcarcam_test_event_cb(qcarcam_hndl_t hndl, qcarcam_event_t event_id, qcarcam_event_payload_t *p_payload)
{
    int i = 0;
    qcarcam_test_input_t *input_ctxt = NULL;
    for (i = 0; i < gCtxt.numInputs; i++)
    {
        if (hndl == g_test_inputs[i].qcarcam_context)
        {
            input_ctxt = &g_test_inputs[i];
            break;
        }
    }

    if (!input_ctxt)
    {
        QCARCAM_ERRORMSG("event_cb called with invalid qcarcam handle %p", hndl);
        return;
    }

    if (g_aborted)
    {
        QCARCAM_ERRORMSG("Test aborted");
        return;
    }

    switch (event_id)
    {
    case QCARCAM_EVENT_FRAME_READY:
        if (input_ctxt->is_running)
        {
            QCARCAM_DBGMSG("%d received QCARCAM_EVENT_FRAME_READY", input_ctxt->idx);
            qcarcam_test_handle_new_frame(input_ctxt);
        }
        break;
    case QCARCAM_EVENT_INPUT_SIGNAL:
        if (gCtxt.enableBridgeErrorDetect)
            qcarcam_test_handle_input_signal(input_ctxt, (qcarcam_input_signal_t)p_payload->uint_payload);
        break;
    case QCARCAM_EVENT_ERROR:
        if (p_payload->uint_payload == QCARCAM_CONN_ERROR)
        {
            QCARCAM_ERRORMSG("Connetion to server lost. input_ctxt %d", input_ctxt->idx);
            abort_test();
        }
        if (gCtxt.enableCSIErrorDetect)
        {
            qcarcam_test_handle_csi_error(input_ctxt);
        }
        break;
    default:
        QCARCAM_ERRORMSG("%d received unsupported event %d", input_ctxt->idx, event_id);
        break;
    }
}

/**
 * Qcarcam gpio interrupt callback function for visibility mode
 */
void qcarcam_test_gpio_interrupt_cb()
{
    int idx;

    gCtxt.vis_value = !gCtxt.vis_value;

    qcarcam_test_get_time(&gCtxt.t_before);
    for (idx = 0; idx < gCtxt.numInputs; ++idx)
    {
        if (g_test_inputs[idx].qcarcam_window != NULL)
        {
            test_util_set_param(g_test_inputs[idx].qcarcam_window, TEST_UTIL_VISIBILITY, gCtxt.vis_value);
        }

        if (g_test_inputs[idx].display_window != NULL)
        {
            test_util_set_param(g_test_inputs[idx].display_window, TEST_UTIL_VISIBILITY, gCtxt.vis_value);
        }

        g_test_inputs[idx].window_params.visibility = gCtxt.vis_value;
    }

    qcarcam_test_get_time(&gCtxt.t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "Time for visbility toggle : %lu ms", (gCtxt.t_after - gCtxt.t_before));
}

/**
 * Qcarcam gpio interrupt callback function for play/pause mode
 */
void qcarcam_test_gpio_play_pause_cb()
{
    int ret = 0;
    int stream_id;
    qcarcam_test_input_t *input_ctxt = NULL;

    qcarcam_test_get_time(&gCtxt.t_before);
    for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
    {
        input_ctxt = &g_test_inputs[stream_id];
        if (input_ctxt->is_running == 0 && input_ctxt->qcarcam_context != NULL)
        {
            ret = qcarcam_resume(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                if (ret == QCARCAM_RET_BADSTATE && input_ctxt->is_running == 1)
                {
                    QCARCAM_ERRORMSG("The streams have already been resumed !");
                }
                else
                {
                    input_ctxt->is_running = 0;
                    QCARCAM_ERRORMSG("qcarcam_resume failed for stream id %d, ret = %d", input_ctxt->qcarcam_input_id, ret);
                }
            }
            else
            {
                QCARCAM_ERRORMSG("qcarcam_resume success for stream id %d", input_ctxt->qcarcam_input_id);
                input_ctxt->is_running = 1;
            }
        }
        else
        {
            input_ctxt->is_running = 0;
            ret = qcarcam_pause(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("qcarcam_pause failed for stream id %d, ret = %d", input_ctxt->qcarcam_input_id, ret);
            }
            else
            {
                QCARCAM_ERRORMSG("qcarcam_pause success for stream id %d", input_ctxt->qcarcam_input_id);
            }
        }
    }

    qcarcam_test_get_time(&gCtxt.t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "Time for play/pause toggle : %lu ms",  (gCtxt.t_after - gCtxt.t_before));
}

/**
 * Qcarcam gpio interrupt callback function for stop/start mode
 */
void qcarcam_test_gpio_start_stop_cb()
{
    int ret = 0;
    int stream_id;
    qcarcam_test_input_t *input_ctxt = NULL;

    qcarcam_test_get_time(&gCtxt.t_before);
    for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
    {
        input_ctxt = &g_test_inputs[stream_id];
        if (input_ctxt->is_running == 0 && input_ctxt->qcarcam_context != NULL)
        {
            ret = qcarcam_start(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                if (ret == QCARCAM_RET_BADSTATE && input_ctxt->is_running == 1)
                {
                    QCARCAM_ERRORMSG("The streams have already been started !");
                }
                else
                {
                    input_ctxt->is_running = 0;
                    QCARCAM_ERRORMSG("qcarcam_start failed for stream id %d, ret = %d", input_ctxt->qcarcam_input_id, ret);
                }
            }
            else
            {
                input_ctxt->is_running = 1;
                QCARCAM_ERRORMSG("qcarcam_start success for stream id %d", input_ctxt->qcarcam_input_id);
            }
        }
        else
        {
            input_ctxt->is_running = 0;
            ret = qcarcam_stop(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("qcarcam_stop failed for stream id %d, ret = %d", input_ctxt->qcarcam_input_id, ret);
            }
            else
            {
                input_ctxt->prev_buf_idx_qcarcam = -1;
                memset(&input_ctxt->is_buf_dequeued, 0x0, sizeof(input_ctxt->is_buf_dequeued));
                QCARCAM_ERRORMSG("qcarcam_stop success for stream id %d", input_ctxt->qcarcam_input_id);
            }
        }
    }

    qcarcam_test_get_time(&gCtxt.t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "Time for start/stop toggle : %lu ms", (gCtxt.t_after - gCtxt.t_before));
}

/**
 * Thread to setup and run qcarcam based on test input context
 *
 * @note For single threaded operation, this function only sets up qcarcam context.
 *      qcarcam_start and handling of frames is not executed.
 *
 * @param arg qcarcam_test_input_t* input_ctxt
 */
static int qcarcam_test_setup_input_ctxt_thread(void *arg)
{
    qcarcam_ret_t ret = QCARCAM_RET_OK;

    qcarcam_test_input_t *input_ctxt = (qcarcam_test_input_t *)arg;
    if (!input_ctxt)
        return -1;

    QCARCAM_INFOMSG("setup_input_ctxt_thread idx = %d, input_desc=%d\n", input_ctxt->idx, input_ctxt->qcarcam_input_id);

    qcarcam_test_get_time(&gCtxt.t_before);
    input_ctxt->qcarcam_context = qcarcam_open(input_ctxt->qcarcam_input_id);
    if (input_ctxt->qcarcam_context == 0)
    {
        QCARCAM_ERRORMSG("qcarcam_open() failed");
        goto qcarcam_thread_fail;
    }

    qcarcam_test_get_time(&gCtxt.t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam_open (input_id %d) : %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));

    QCARCAM_INFOMSG("render_thread idx = %d, input_desc=%d context=%p\n", input_ctxt->idx, input_ctxt->qcarcam_input_id, input_ctxt->qcarcam_context);

    // For HDMI/CVBS Input
    // NOTE: set HDMI IN resolution in qcarcam_config_single_hdmi.xml before
    // running the test
    if (input_ctxt->qcarcam_input_id == QCARCAM_INPUT_TYPE_DIGITAL_MEDIA)
    {
        qcarcam_test_get_time(&gCtxt.t_before);
        qcarcam_param_value_t param;
        param.res_value.width = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].width;
        param.res_value.height = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].height;

        ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_RESOLUTION, &param);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("qcarcam_s_param resolution() failed");
            goto qcarcam_thread_fail;
        }
        qcarcam_test_get_time(&gCtxt.t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam_s_param resolution (input_id %d) : %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));
    }

    qcarcam_test_get_time(&gCtxt.t_before);
    ret = qcarcam_s_buffers(input_ctxt->qcarcam_context, &input_ctxt->p_buffers_output);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("qcarcam_s_buffers() failed");
        goto qcarcam_thread_fail;
    }

#ifdef ENABLE_INJECTION_SUPPORT
    if (input_ctxt->is_injection)
    {
        QCARCAM_ERRORMSG("read from input_file - set input buffers");

        ret = qcarcam_s_input_buffers(input_ctxt->qcarcam_context, &input_ctxt->p_buffers_input);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("qcarcam_s_input_buffers() failed");
            goto qcarcam_thread_fail;
        }
    }
#endif

    qcarcam_test_get_time(&gCtxt.t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam_s_buffers (input_id %d) : %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));
    QCARCAM_INFOMSG("qcarcam_s_buffers done, qcarcam_start..");

    if (input_ctxt->use_event_callback)
    {
        qcarcam_param_value_t param;
        qcarcam_test_get_time(&gCtxt.t_before);
        param.ptr_value = (void *)qcarcam_test_event_cb;
        ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EVENT_CB, &param);

        param.uint_value = QCARCAM_EVENT_FRAME_READY | QCARCAM_EVENT_INPUT_SIGNAL | QCARCAM_EVENT_ERROR;
        ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EVENT_MASK, &param);

        qcarcam_test_get_time(&gCtxt.t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam_s_param (input_id %d) : %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));
    }

    if (input_ctxt->frame_rate_config.frame_drop_mode != QCARCAM_KEEP_ALL_FRAMES)
    {
        qcarcam_param_value_t param;
        qcarcam_test_get_time(&gCtxt.t_before);

        param.frame_rate_config = input_ctxt->frame_rate_config;

        ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_FRAME_RATE, &param);

        qcarcam_test_get_time(&gCtxt.t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam_s_param fps (input_id %d): %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));
        QCARCAM_INFOMSG("Provided QCARCAM_PARAM_FRAME_RATE: mode = %d, period = %d, pattern = 0x%x",
            param.frame_rate_config.frame_drop_mode,
            param.frame_rate_config.frame_drop_period,
            param.frame_rate_config.frame_drop_pattern);
    }

    /*single threaded handles frames outside this function*/
    if (gCtxt.multithreaded)
    {
        qcarcam_test_get_time(&gCtxt.t_before);
        input_ctxt->is_running = 1;
        ret = qcarcam_start(input_ctxt->qcarcam_context);
        pthread_mutex_lock(&gCtxt.mutex_start_cnt);
        gCtxt.started_stream_cnt++;
        pthread_mutex_unlock(&gCtxt.mutex_start_cnt);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("qcarcam_start() failed");
            goto qcarcam_thread_fail;
        }

#ifdef ENABLE_INJECTION_SUPPORT
        if (input_ctxt->is_injection)
        {
            qcarcam_param_value_t param;
            param.uint_value = 0; //TODO: fix this to be appropriate buffer
            qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_INJECTION_START, &param);
        }
#endif
        /* Set exposure configuration */
#ifdef ENABLE_EXPOSURE_CONTROL
        {
            qcarcam_param_value_t param;
            param.exposure_config.exposure_time = input_ctxt->exp_time;
            param.exposure_config.gain = input_ctxt->gain;
            param.exposure_config.exposure_mode_type = QCARCAM_EXPOSURE_MANUAL;
            if (input_ctxt->manual_exposure)
            {
                QCARCAM_INFOMSG("Provided QCARCAM_PARAM_MANUAL_EXPOSURE : time =%f ms, gain = %f", input_ctxt->exp_time, input_ctxt->gain);
                qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EXPOSURE, &param);
            }
            else
            {
                QCARCAM_INFOMSG("AUTO EXPOSURE MODE");
                param.exposure_config.exposure_mode_type = QCARCAM_EXPOSURE_AUTO;
                qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EXPOSURE, &param);
            }

        }
#endif
        qcarcam_test_get_time(&gCtxt.t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam_start (input_id %d) : %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));

        if (!input_ctxt->use_event_callback)
        {
            while (!g_aborted)
            {
                if (qcarcam_test_handle_new_frame(input_ctxt))
                    break;
            }
        }
        else
        {
            pthread_mutex_lock(&gCtxt.mutex_abort);
            if (!g_aborted)
            {
                pthread_cond_wait(&gCtxt.cond_abort, &gCtxt.mutex_abort);
            }
            pthread_mutex_unlock(&gCtxt.mutex_abort);
        }
    }

    QCARCAM_INFOMSG("exit setup_input_ctxt_thread idx = %d", input_ctxt->idx);
    return 0;

qcarcam_thread_fail:
    if (input_ctxt->qcarcam_context)
    {
        qcarcam_close(input_ctxt->qcarcam_context);
        input_ctxt->qcarcam_context = NULL;
    }

    return -1;
}

void display_menu()
{
    printf("\n ========================================================== \n");
    printf(" 'a'.....Open a stream            'b'.....Open all streams \n");
    printf(" 'c'.....Close a stream           'd'.....Close all streams \n");
    printf(" 'e'.....Start a stream           'f'.....Start all streams \n");
    printf(" 'g'.....Stop a stream            'h'.....Stop all streams \n");
    printf(" 'i'.....Pause a stream           'j'.....Pause all streams \n");
    printf(" 'k'.....Resume a stream          'l'.....Resume all streams \n");
    printf(" 'm'.....Set Automatic exposure   'n'.....Set Manual exposure \n");
    printf(" 'o'.....Enable event callback    'p'.....Disable Event callback \n");
    printf(" 'q'.....Set a stream frame rate  'r'.....Set color param \n");
    printf(" 's'.....Get color param \n");
    printf(" 'y'.....Dump Image \n");
    printf(" 'z'.....Exit \n");
    printf("\n =========================================================== \n");
    printf(" Enter your choice\n");
}

qcarcam_test_input_t* get_input_ctxt(int stream_id)
{
    if (stream_id >= 0 && stream_id < gCtxt.numInputs)
        return &g_test_inputs[stream_id];
    printf("Wrong stream id entered, please check the input xml\n");
    return NULL;
}

void process_cmds(char choice)
{
    int ret = 0;
    int stream_id;
    qcarcam_test_input_t *input_ctxt = NULL;

    switch (choice)
    {
        /* Open a stream */
    case 'a':
        printf("This feature is not enabled yet!!!!!!\n");
#if 0
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt)
        {
            input_ctxt->qcarcam_context = qcarcam_open(input_ctxt->qcarcam_input_id);
            if (input_ctxt->qcarcam_context == 0)
                QCARCAM_ERRORMSG("qcarcam_open() failed for stream %d\n", stream_id);
        }
#endif
        break;

        /* Open all streams */
    case 'b':
        printf("This feature is not enabled yet!!!!!!\n");
#if 0
        for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
        {
            input_ctxt = &g_test_inputs[stream_id];
            input_ctxt->qcarcam_context = qcarcam_open(input_ctxt->qcarcam_input_id);
            if (input_ctxt->qcarcam_context == 0)
                QCARCAM_ERRORMSG("qcarcam_open() failed for stream %d\n", stream_id);
        }
#endif
        break;

        /* Close a stream */
    case 'c':
        printf("This feature is not enabled yet!!!!!!\n");
#if 0
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            ret = qcarcam_close(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("qcarcam_close failed for stream id %d! ret = %d\n",input_ctxt->qcarcam_input_id, ret);
            }

            input_ctxt->qcarcam_context = NULL;
        }
#endif
        break;

        /* Close all streams */
    case 'd':
        printf("This feature is not enabled yet!!!!!!\n");
#if 0
        for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
        {
            input_ctxt = &g_test_inputs[stream_id];
            if (input_ctxt->qcarcam_context)
            {
                ret = qcarcam_close(input_ctxt->qcarcam_context);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("qcarcam_close failed for stream id %d! ret = %d\n",input_ctxt->qcarcam_input_id, ret);
                }

                input_ctxt->qcarcam_context = NULL;
            }
        }
#endif
        break;

        /* Start a stream */
    case 'e':
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            ret = qcarcam_start(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                if (ret == QCARCAM_RET_BADSTATE && input_ctxt->is_running == 1)
                {
                    QCARCAM_ERRORMSG("The stream has already been started !");
                }
                else
                {
                    input_ctxt->is_running = 0;
                    QCARCAM_ERRORMSG("qcarcam_start failed for stream id %d, ret = %d", input_ctxt->qcarcam_input_id, ret);
                }
            }
            else
            {
                input_ctxt->is_running = 1;
                QCARCAM_ERRORMSG("qcarcam_start success for stream id %d", input_ctxt->qcarcam_input_id);
            }
        }
        break;

    /* Start all streams */
    case 'f':
        for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
        {
            input_ctxt = &g_test_inputs[stream_id];
            if (input_ctxt->qcarcam_context)
            {
                ret = qcarcam_start(input_ctxt->qcarcam_context);
                if (ret != QCARCAM_RET_OK)
                {
                    if (ret == QCARCAM_RET_BADSTATE && input_ctxt->is_running == 1)
                    {
                        QCARCAM_ERRORMSG("The streams have already been started !");
                    }
                    else
                    {
                        input_ctxt->is_running = 0;
                        QCARCAM_ERRORMSG("qcarcam_start failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
                    }
                }
                else
                {
                    input_ctxt->is_running = 1;
                    QCARCAM_ERRORMSG("qcarcam_start success for stream id %d",input_ctxt->qcarcam_input_id);
                }
            }
        }
        break;

    /* Stop a stream */
    case 'g':
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            input_ctxt->is_running = 0;
            ret = qcarcam_stop(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("qcarcam_stop failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
            }
            else
            {
                input_ctxt->prev_buf_idx_qcarcam = -1;
                memset(&input_ctxt->is_buf_dequeued, 0x0, sizeof(input_ctxt->is_buf_dequeued));
                QCARCAM_ERRORMSG("qcarcam_stop success for stream id %d",input_ctxt->qcarcam_input_id);
            }
        }
        break;

    /* Stop all streams */
    case 'h':
        for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
        {
            input_ctxt = &g_test_inputs[stream_id];
            if (input_ctxt->qcarcam_context)
            {
                input_ctxt->is_running = 0;
                ret = qcarcam_stop(input_ctxt->qcarcam_context);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("qcarcam_stop failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
                }
                else
                {
                    input_ctxt->prev_buf_idx_qcarcam = -1;
                    memset(&input_ctxt->is_buf_dequeued, 0x0, sizeof(input_ctxt->is_buf_dequeued));
                    QCARCAM_ERRORMSG("qcarcam_stop success for stream id %d",input_ctxt->qcarcam_input_id);
                }
            }
        }
        break;

        /* Pause a stream */
    case 'i':
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            input_ctxt->is_running = 0;
            ret = qcarcam_pause(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("qcarcam_pause failed for stream id %d, ret = %d", input_ctxt->qcarcam_input_id, ret);
            }
            else
            {
                QCARCAM_ERRORMSG("qcarcam_pause success for stream id %d", input_ctxt->qcarcam_input_id);
            }
        }
        break;

    /* Pause all streams */
    case 'j':
        for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
        {
            input_ctxt = &g_test_inputs[stream_id];
            if (input_ctxt->qcarcam_context)
            {
                input_ctxt->is_running = 0;
                ret = qcarcam_pause(input_ctxt->qcarcam_context);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("qcarcam_pause failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
                }
                else
                {
                    QCARCAM_ERRORMSG("qcarcam_pause success for stream id %d",input_ctxt->qcarcam_input_id);
                }
            }
        }
        break;

    /* Resume a stream */
    case 'k':
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            ret = qcarcam_resume(input_ctxt->qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                if (ret == QCARCAM_RET_BADSTATE && input_ctxt->is_running == 1)
                {
                    QCARCAM_ERRORMSG("The stream has already been resumed !");
                }
                else
                {
                    input_ctxt->is_running = 0;
                    QCARCAM_ERRORMSG("qcarcam_resume failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
                }
            }
            else
            {
                QCARCAM_ERRORMSG("qcarcam_resume success for stream id %d",input_ctxt->qcarcam_input_id);
                input_ctxt->is_running = 1;
            }
        }
        break;

    /* Resume all streams */
    case 'l':
        for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
        {
            input_ctxt = &g_test_inputs[stream_id];
            if (input_ctxt->qcarcam_context)
            {
                ret = qcarcam_resume(input_ctxt->qcarcam_context);
                if (ret != QCARCAM_RET_OK)
                {
                    if (ret == QCARCAM_RET_BADSTATE && input_ctxt->is_running == 1)
                    {
                        QCARCAM_ERRORMSG("The streams have already been resumed !");
                    }
                    else
                    {
                        input_ctxt->is_running = 0;
                        QCARCAM_ERRORMSG("qcarcam_resume failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
                    }
                }
                else
                {
                    QCARCAM_ERRORMSG("qcarcam_resume success for stream id %d",input_ctxt->qcarcam_input_id);
                    input_ctxt->is_running = 1;
                }
            }
        }
        break;

    /* Set Automatic Exposure */
    case 'm':
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            qcarcam_param_value_t param;
            param.exposure_config.exposure_mode_type = QCARCAM_EXPOSURE_AUTO;
            ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EXPOSURE, &param);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("qcarcam_s_param failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
            }
            else
            {
                QCARCAM_ERRORMSG("qcarcam_s_param success for stream id %d",input_ctxt->qcarcam_input_id);
            }
        }
        break;

    /* Set manual exposure */
    case 'n':
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            qcarcam_param_value_t param;
            get_input_exposure(&param.exposure_config);
            param.exposure_config.exposure_mode_type = QCARCAM_EXPOSURE_MANUAL;
            ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EXPOSURE, &param);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("qcarcam_s_param failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
            }
            else
            {
                QCARCAM_ERRORMSG("qcarcam_s_param success for stream id %d",input_ctxt->qcarcam_input_id);
            }
        }
        break;

    /* Enable event callbacks */
    case 'o':
        for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
        {
            input_ctxt = &g_test_inputs[stream_id];
            if (input_ctxt->qcarcam_context && input_ctxt->use_event_callback)
            {
                qcarcam_param_value_t param;
                param.ptr_value = (void *)qcarcam_test_event_cb;
                ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EVENT_CB, &param);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("qcarcam_s_param failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
                    break;
                }

                param.uint_value = QCARCAM_EVENT_FRAME_READY | QCARCAM_EVENT_INPUT_SIGNAL | QCARCAM_EVENT_ERROR;
                ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EVENT_MASK, &param);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("qcarcam_s_param failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
                }
                else
                {
                    QCARCAM_ERRORMSG("qcarcam_s_param success for stream id %d",input_ctxt->qcarcam_input_id);
                }
            }
            else
            {
                QCARCAM_ERRORMSG("Callback is disabled in xml, please check the input xml");
            }
        }
        break;

    /* Disable event callbacks */
    case 'p':
        for (stream_id = 0; stream_id < gCtxt.numInputs; ++stream_id)
        {
            input_ctxt = &g_test_inputs[stream_id];
            if (input_ctxt->qcarcam_context && input_ctxt->use_event_callback)
            {
                qcarcam_param_value_t param;
                param.ptr_value = NULL;
                ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EVENT_CB, &param);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("qcarcam_s_param failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
                    break;
                }

                param.uint_value = 0x0;
                ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_EVENT_MASK, &param);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("qcarcam_s_param failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
                }
                else
                {
                    QCARCAM_ERRORMSG("qcarcam_s_param success for stream id %d",input_ctxt->qcarcam_input_id);
                }
            }
            else
            {
                QCARCAM_ERRORMSG("Callback is disabled in xml, please check the input xml");
            }
        }
        break;

    /* Set Frame Rate */
    case 'q':
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            qcarcam_param_value_t param;
            get_input_framerate(&param.frame_rate_config);
            ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_FRAME_RATE, &param);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("qcarcam_s_param framerate failed for stream id %d, ret = %d",input_ctxt->qcarcam_input_id, ret);
            }
            else
            {
                QCARCAM_ERRORMSG("qcarcam_s_param framerate success for stream id %d",input_ctxt->qcarcam_input_id);
            }
            QCARCAM_INFOMSG("Provided QCARCAM_PARAM_FRAME_RATE: mode = %d, period = %d, pattern = 0x%x",
                param.frame_rate_config.frame_drop_mode,
                param.frame_rate_config.frame_drop_period,
                param.frame_rate_config.frame_drop_pattern);
        }
        break;
    /* Change color params */
    case 'r':
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            char buf[BUFSIZE];
            char *p = NULL;
            int param_option = 0;
            float param_value = 0.0;
            qcarcam_param_value_t param;

            printf("Select param: [0] Saturation, [1] Hue.\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                /* 10 indicates decimal number*/
                param_option = strtol(buf, &p, 10);
            }

            switch (param_option)
            {
                case 0:
                    printf("Enter saturation value. Valid range (-1.0 to 1.0)\n");
                    if (fgets(buf, sizeof(buf), stdin) != NULL)
                    {
                        param_value = strtof(buf, NULL);
                    }

                    param.float_value = param_value;
                    ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_SATURATION, &param);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("qcarcam_s_param failed for stream id %d! ret = %d\n",
                        input_ctxt->qcarcam_input_id, ret);
                    }
                    break;
                case 1:
                    printf("Enter hue value. Valid range (-1.0 to 1.0)\n");
                    if (fgets(buf, sizeof(buf), stdin) != NULL)
                    {
                        param_value = strtof(buf, NULL);
                    }

                    param.float_value = param_value;
                    ret = qcarcam_s_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_HUE, &param);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("qcarcam_s_param failed for stream id %d! ret = %d\n",
                        input_ctxt->qcarcam_input_id, ret);
                    }
                    break;
                default:
                    printf("Param not available\n");
                    break;
            }
        }
        break; //Case "r"
    case 's':
        stream_id = get_input_stream_id();
        input_ctxt = get_input_ctxt(stream_id);
        if (input_ctxt && input_ctxt->qcarcam_context)
        {
            char buf[BUFSIZE];
            char *p = NULL;
            int param_option = 0;
            qcarcam_param_value_t param;

            printf("Select param: [0] Saturation, [1] Hue.\n");
            if (fgets(buf, sizeof(buf), stdin) != NULL)
            {
                /* 10 indicates decimal number*/
                param_option = strtol(buf, &p, 10);
            }

            switch (param_option)
            {
                case 0:
                    ret = qcarcam_g_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_SATURATION, &param);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("qcarcam_g_param failed for stream id %d! ret = %d\n",
                        input_ctxt->qcarcam_input_id, ret);
                    }
                    break;
                case 1:
                    ret = qcarcam_g_param(input_ctxt->qcarcam_context, QCARCAM_PARAM_HUE, &param);
                    if (ret != QCARCAM_RET_OK)
                    {
                        QCARCAM_ERRORMSG("qcarcam_g_param failed for stream id %d! ret = %d\n",
                        input_ctxt->qcarcam_input_id, ret);
                    }
                    break;
                default:
                    printf("Param not available\n");
                    ret = -1;
                    break;
            }

            if (ret == 0)
            {
                printf("Param value returned = %.2f\n", param.float_value);
                QCARCAM_ALWZMSG("qcarcam_g_param value returned = %.2f", param.float_value);
            }
        }
        break; //Case "s"

    /* Exit app */
    case 'z':
        abort_test();
        break;

    case 'y':
        gCtxt.dumpFrame = 1;
        usleep(33000);
        gCtxt.dumpFrame = 0;
        break;

    default:
        QCARCAM_ERRORMSG("Invalid Input");
        break;
    }
}

int main(int argc, char **argv)
{
#if defined(__INTEGRITY)
    // Set priority of main thread
    // Task needs to be linked to an object defined in corresponding INT file
    Task mainTask = TaskObjectNumber(QCARCAM_OBJ_NUM);
    SetTaskPriority(mainTask, CameraTranslateThreadPriority(QCARCAM_THRD_PRIO), false);
#endif

    initialize_qcarcam_test_ctxt();

    ais_log_init(NULL, (char *)"qcarcam_test");

    qcarcam_test_get_time(&gCtxt.t_start);

    const char *tok;
    qcarcam_ret_t ret;

    int rval = EXIT_FAILURE;
    int i, rc;
    int input_idx;

    void* timer_thread_handle = NULL;
    void *signal_thread_handle;
    void *fps_thread_handle;
    void *signal_loss_thread_handle;
    void *csi_error_thread_handle;
    char thread_name[64];
    int bprint_diag = 0;

    test_util_intr_thrd_args_t intr_thrd_args;

    int remote_attach_loop = 0; // no looping for remote process attach
    qcarcam_init_t qcarcam_init = { 0 };
    qcarcam_init.version = QCARCAM_VERSION;

    ret = qcarcam_initialize(&qcarcam_init);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("qcarcam_initialize failed %d", ret);
        exit(-1);
    }

    test_util_ctxt_t* test_util_ctxt = NULL;
    test_util_ctxt_params_t test_util_ctxt_params;
    memset(&test_util_ctxt_params, 0x0, sizeof(test_util_ctxt_params));

    QCARCAM_DBGMSG("Arg parse Begin");

    for (i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "-config=", strlen("-config=")))
        {
            tok = argv[i] + strlen("-config=");
            snprintf(g_filename, sizeof(g_filename), "%s", tok);
        }
        else if (!strncmp(argv[i], "-dumpFrame=", strlen("-dumpFrame=")))
        {
            tok = argv[i] + strlen("-dumpFrame=");
            gCtxt.dumpFrame = atoi(tok);
        }
        else if (!strncmp(argv[i], "-startStop=", strlen("-startStop=")))
        {
            tok = argv[i] + strlen("-startStop=");
            gCtxt.enableStartStop = atoi(tok);
        }
        else if (!strncmp(argv[i], "-pauseResume=", strlen("-pauseResume=")))
        {
            tok = argv[i] + strlen("-pauseResume=");
            gCtxt.enablePauseResume = atoi(tok);
        }
        else if (!strncmp(argv[i], "-noDisplay", strlen("-noDisplay")))
        {
            test_util_ctxt_params.disable_display = 1;
        }
        else if (!strncmp(argv[i], "-singlethread", strlen("-singlethread")))
        {
            gCtxt.multithreaded = 0;
        }
        else if (!strncmp(argv[i], "-printfps", strlen("-printfps")))
        {
            tok = argv[i] + strlen("-printfps=");
            gCtxt.fps_print_delay = atoi(tok);
        }
        else if (!strncmp(argv[i], "-showStats=", strlen("-showStats=")))
        {
            tok = argv[i] + strlen("-showStats=");
            gCtxt.enableStats = atoi(tok);
        }
        else if (!strncmp(argv[i], "-c2d=", strlen("-c2d=")))
        {
            tok = argv[i] + strlen("-c2d=");
            gCtxt.enable_c2d = atoi(tok);
            test_util_ctxt_params.enable_c2d = gCtxt.enable_c2d;
        }
        else if (!strncmp(argv[i], "-di=", strlen("-di=")))
        {
            tok = argv[i] + strlen("-di=");
            gCtxt.enable_deinterlace = atoi(tok);
            test_util_ctxt_params.enable_di = gCtxt.enable_deinterlace;
        }
        else if (!strncmp(argv[i], "-nonInteractive", strlen("-nonInteractive")))
        {
            gCtxt.enableMenuMode = 0;
        }
        else if (!strncmp(argv[i], "-csiErrDetect", strlen("-csiErrDetect")))
        {
            /* only one of them enabled in real scenario */
            gCtxt.enableBridgeErrorDetect = 0;
            gCtxt.enableCSIErrorDetect = 1;
        }
        else if (!strncmp(argv[i], "-printlatency", strlen("-printlatency")))
        {
            bprint_diag = 1;
        }
        else if (!strncmp(argv[i], "-seconds=", strlen("-seconds=")))
        {
            tok = argv[i] + strlen("-seconds=");
            gCtxt.exitSeconds = atoi(tok);
        }
        else if (!strncmp(argv[i], "-retry", strlen("-retry")))
        {
            /* Used to wait for query inputs to handoff early RVC */
            gCtxt.enableRetry = 1;
        }
        else if (!strncmp(argv[i], "-gpioNumber=", strlen("-gpioNumber=")))
        {
            tok = argv[i]+ strlen("-gpioNumber=");
            gCtxt.gpioNumber = atoi(tok);
            test_util_ctxt_params.enable_gpio_config = 1;
        }
        else if (!strncmp(argv[i], "-gpioMode=", strlen("-gpioMode=")))
        {
            /**
            *   Mode 0 : Visiblity toggle
            *   Mode 1 : Play/Pause toggle
            *   Mode 2 : Start/Stop toggle
            */
            tok = argv[i] + strlen("-gpioMode=");
            gCtxt.gpioMode = atoi(tok);
        }
        else
        {
            QCARCAM_ERRORMSG("Invalid argument, argv[%d]=%s", i, argv[i]);
            exit(-1);
        }
    }

    QCARCAM_DBGMSG("Arg parse End");

    qcarcam_test_get_time(&gCtxt.t_before);

    sigset_t sigset;
    sigfillset(&sigset);
    for (i = 0; exceptsigs[i] != -1; i++)
    {
        sigdelset(&sigset, exceptsigs[i]);
    }
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    QCARCAM_DBGMSG("test_util_init");
    ret = test_util_init(&test_util_ctxt, &test_util_ctxt_params);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_init failed");
        exit(-1);
    }

    qcarcam_test_get_time(&gCtxt.t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "test_util_init : %lu ms", (gCtxt.t_after - gCtxt.t_before));

    qcarcam_test_get_time(&gCtxt.t_before);

    /*parse xml*/
    test_util_xml_input_t *xml_inputs = (test_util_xml_input_t *)calloc(NUM_MAX_CAMERAS, sizeof(test_util_xml_input_t));
    if (!xml_inputs)
    {
        QCARCAM_ERRORMSG("Failed to allocate xml input struct");
        exit(-1);
    }

    gCtxt.numInputs = test_util_parse_xml_config_file(g_filename, xml_inputs, NUM_MAX_CAMERAS);
    if (gCtxt.numInputs <= 0)
    {
        QCARCAM_ERRORMSG("Failed to parse config file!");
        exit(-1);
    }

    qcarcam_test_get_time(&gCtxt.t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "xml parsing : %lu ms", (gCtxt.t_after - gCtxt.t_before));

    qcarcam_test_get_time(&gCtxt.t_before);

    /*query qcarcam*/
    qcarcam_input_t *pInputs;
    unsigned int queryNumInputs = 0, queryFilled = 0;
    /*retry used for early camera handoff for RVC*/
    do {
        ret = qcarcam_query_inputs(NULL, 0, &queryNumInputs);
        if (QCARCAM_RET_OK != ret || queryNumInputs == 0)
        {
            QCARCAM_ERRORMSG("Failed qcarcam_query_inputs number of inputs with ret %d", ret);
            if (gCtxt.enableRetry == 0)
            {
                exit(-1);
            }
        }
        else
        {
            break;
        }
        if (gCtxt.enableRetry == 1)
        {
            usleep(500000);
        }
    } while (gCtxt.enableRetry == 1);

    pInputs = (qcarcam_input_t *)calloc(queryNumInputs, sizeof(*pInputs));
    if (!pInputs)
    {
        QCARCAM_ERRORMSG("Failed to allocate pInputs");
        exit(-1);
    }

    ret = qcarcam_query_inputs(pInputs, queryNumInputs, &queryFilled);
    if (QCARCAM_RET_OK != ret || queryFilled != queryNumInputs)
    {
        QCARCAM_ERRORMSG("Failed qcarcam_query_inputs with ret %d %d %d", ret, queryFilled, queryNumInputs);
        exit(-1);
    }

    QCARCAM_INFOMSG("--- QCarCam Queried Inputs ----");
    for (i = 0; i < (int)queryFilled; i++)
    {
        QCARCAM_INFOMSG("%d: input_id=%d, res=%dx%d fmt=0x%08x fps=%.2f", i, pInputs[i].desc,
            pInputs[i].res[0].width, pInputs[i].res[0].height,
            pInputs[i].color_fmt[0], pInputs[i].res[0].fps);
    }
    qcarcam_test_get_time(&gCtxt.t_after);
    QCARCAM_PERFMSG(gCtxt.enableStats, "query inputs : %lu ms", (gCtxt.t_after - gCtxt.t_before));
    QCARCAM_INFOMSG("-------------------------------");

    while (remote_attach_loop)
    {
        volatile int i;
        for (i = 0; i < 1000; i++)
            ;
    }

#ifndef C2D_DISABLED
    if (gCtxt.enable_c2d) {
        QCARCAM_DBGMSG("C2D Setup Begin");

        /*init C2D*/
        C2D_DRIVER_SETUP_INFO set_driver_op = {
            .max_surface_template_needed = (NUM_MAX_DISP_BUFS + QCARCAM_MAX_NUM_BUFFERS) * NUM_MAX_CAMERAS,
        };
        C2D_STATUS c2d_status = c2dDriverInit(&set_driver_op);
        if (c2d_status != C2D_STATUS_OK)
        {
            QCARCAM_ERRORMSG("c2dDriverInit failed");
            goto fail;
        }
    }
#endif

    /*signal handler thread*/
    snprintf(thread_name, sizeof(thread_name), "signal_thrd");
    rc = CameraCreateThread(QCARCAM_THRD_PRIO, 0, &signal_thread, 0, 0, thread_name, &signal_thread_handle);
    if (rc)
    {
        QCARCAM_ERRORMSG("CameraCreateThread failed : %s", thread_name);
        goto fail;
    }

    snprintf(thread_name, sizeof(thread_name), "signal_loss_thrd");
    rc = CameraCreateThread(QCARCAM_THRD_PRIO, 0, &check_signal_loss_thread, 0, 0, thread_name, &signal_loss_thread_handle);
    if (rc)
    {
        QCARCAM_ERRORMSG("CameraCreateThread failed : %s", thread_name);
        goto fail;
    }

    if (gCtxt.enableCSIErrorDetect)
    {
        snprintf(thread_name, sizeof(thread_name), "check_csi_error_thread");
        rc = CameraCreateThread(QCARCAM_THRD_PRIO, 0, &check_csi_error_thread, 0, 0, thread_name, &csi_error_thread_handle);
        if (rc)
        {
            QCARCAM_ERRORMSG("CameraCreateThread failed : %s", thread_name);
            goto fail;
        }
    }

    /*if GPIO interrupts have been enabled*/
    if (gCtxt.gpioNumber > 0)
    {
        uint32_t irq;
        test_util_trigger_type_t trigger;

        switch (gCtxt.gpioMode)
        {
        case TESTUTIL_GPIO_MODE_VISIBILITY:
            // gpioMode 0: visibility toggle
            QCARCAM_INFOMSG("Selected GPIO Mode: Visibility toggle");
            trigger = TESTUTIL_GPIO_INTERRUPT_TRIGGER_EDGE;
            intr_thrd_args.cb_func = qcarcam_test_gpio_interrupt_cb;
            break;
        case TESTUTIL_GPIO_MODE_PLAYPAUSE:
            // gpioMode 1: play/pause toggle
            QCARCAM_INFOMSG("Selected GPIO Mode: Play/Pause toggle");
            trigger = TESTUTIL_GPIO_INTERRUPT_TRIGGER_FALLING;
            intr_thrd_args.cb_func = qcarcam_test_gpio_play_pause_cb;
            break;
        case TESTUTIL_GPIO_MODE_STARTSTOP:
            // gpioMode 2: start/stop toggle
            QCARCAM_INFOMSG("Selected GPIO Mode: Start/Stop toggle");
            trigger = TESTUTIL_GPIO_INTERRUPT_TRIGGER_FALLING;
            intr_thrd_args.cb_func = qcarcam_test_gpio_start_stop_cb;
            break;
        default:
            QCARCAM_ERRORMSG("Invalid mode");
            goto fail;
        }

        if (test_util_gpio_interrupt_config(&irq, gCtxt.gpioNumber, trigger) != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_gpio_interrupt_config failed");
            goto fail;
        }

        intr_thrd_args.irq = irq;

        if (test_util_interrupt_attach(&intr_thrd_args) != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_interrupt_attach failed");
            goto fail;
        }
    }

    /*thread used frame rate measurement*/
    if (gCtxt.fps_print_delay)
    {
        snprintf(thread_name, sizeof(thread_name), "framerate_thrd");
        rc = CameraCreateThread(QCARCAM_THRD_PRIO, 0, &framerate_thread, 0, 0, thread_name, &fps_thread_handle);
        if (rc)
        {
            QCARCAM_ERRORMSG("CameraCreateThread failed : %s", thread_name);
            goto fail;
        }
    }

    for (input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
    {
        qcarcam_test_input_t* input_ctxt = &g_test_inputs[input_idx];
        test_util_xml_input_t* p_xml_input = &xml_inputs[input_idx];

        input_ctxt->qcarcam_input_id = p_xml_input->properties.qcarcam_input_id;
        input_ctxt->use_event_callback = p_xml_input->properties.use_event_callback;
        input_ctxt->frame_timeout = (p_xml_input->properties.frame_timeout == -1) ?
            QCARCAM_TEST_DEFAULT_GET_FRAME_TIMEOUT :
            p_xml_input->properties.frame_timeout * 1000 * 1000;

        for (i = 0; i < (int)queryFilled; i++)
        {
            if (pInputs[i].desc == input_ctxt->qcarcam_input_id)
            {
                input_ctxt->query_inputs_idx = i;
            }
        }

        /*@todo: move these buffer settings to be queried from qcarcam and not set externally*/
        input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers = p_xml_input->output_params.n_buffers;
        if (p_xml_input->output_params.buffer_size[0] == -1 || p_xml_input->output_params.buffer_size[1] == -1)
        {
            input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].width = pInputs[input_ctxt->query_inputs_idx].res[0].width;
            input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].height = pInputs[input_ctxt->query_inputs_idx].res[0].height;
            input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].format = pInputs[input_ctxt->query_inputs_idx].color_fmt[0];
        }
        else
        {
            input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].width = p_xml_input->output_params.buffer_size[0];
            input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].height = p_xml_input->output_params.buffer_size[1];
            input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].format = pInputs[input_ctxt->query_inputs_idx].color_fmt[0];
        }

        input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers = p_xml_input->window_params.n_buffers_display;
        input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].width = p_xml_input->output_params.buffer_size[0];
        input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].height = p_xml_input->output_params.buffer_size[1];
        input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].format = QCARCAM_FMT_RGB_888;

        input_ctxt->window_params = p_xml_input->window_params;
        input_ctxt->window_params.buffer_size[0] = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].width;
        input_ctxt->window_params.buffer_size[1] = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].height;

        input_ctxt->window_params.visibility = 1;

        //Capture exposure params if any
        input_ctxt->exp_time = p_xml_input->exp_params.exposure_time;
        input_ctxt->gain = p_xml_input->exp_params.gain;
        input_ctxt->manual_exposure = ((p_xml_input->exp_params.manual_exp != 0) ? 1 : 0);

        //Capture frame rate params if any
        input_ctxt->frame_rate_config = p_xml_input->output_params.frame_rate_config;

        if (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace != SW_BOB_30FPS && gCtxt.enable_deinterlace != SW_BOB_60FPS))
        {
            if (gCtxt.enable_c2d)
            {
                QCARCAM_ERRORMSG("c2d & deinterlace can't be used at the same time");
                goto fail;
            }

            input_ctxt->window_params.buffer_size[1] = DE_INTERLACE_HEIGHT;
            input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].height = DE_INTERLACE_HEIGHT;
        }

        //Use input 11 for injection
        if (input_ctxt->qcarcam_input_id == QCARCAM_TEST_INPUT_INJECTION)
        {
#ifdef ENABLE_INJECTION_SUPPORT
            size_t size;

            input_ctxt->is_injection = 1;

            input_ctxt->p_buffers_input.n_buffers = 1;
            input_ctxt->p_buffers_input.color_fmt = (qcarcam_color_fmt_t)QCARCAM_COLOR_FMT(p_xml_input->inject_params.pattern, p_xml_input->inject_params.bitdepth, p_xml_input->inject_params.pack);
            input_ctxt->p_buffers_input.buffers = (qcarcam_buffer_t *)calloc(input_ctxt->p_buffers_input.n_buffers, sizeof(*input_ctxt->p_buffers_input.buffers));

            if (input_ctxt->p_buffers_input.buffers == 0)
            {
                QCARCAM_ERRORMSG("alloc qcarcam_buffer input buffers failed");
                goto fail;
            }

            input_ctxt->p_buffers_input.buffers[0].n_planes = 1;
            input_ctxt->p_buffers_input.buffers[0].planes[0].width = p_xml_input->inject_params.buffer_size[0];
            input_ctxt->p_buffers_input.buffers[0].planes[0].height = p_xml_input->inject_params.buffer_size[1];
            input_ctxt->p_buffers_input.buffers[0].planes[0].stride = p_xml_input->inject_params.stride;

            size = input_ctxt->p_buffers_input.buffers[0].planes[0].stride * input_ctxt->p_buffers_input.buffers[0].planes[0].height;

            ret = test_util_allocate_input_buffers(&input_ctxt->p_buffers_input, size);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_alloc_input_buffers failed");
                goto fail;
            }

            //TODO: this only works for single buffer for now...
            ret = test_util_read_input_data(&input_ctxt->p_buffers_input, 0, p_xml_input->inject_params.filename, size);
#else
            QCARCAM_ERRORMSG("Injection not supported!");
            goto fail;
#endif
        }
    }

    QCARCAM_DBGMSG("Screen Setup Begin");

    for (input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
    {
        qcarcam_test_input_t *input_ctxt = &g_test_inputs[input_idx];
        input_ctxt->test_util_ctxt = test_util_ctxt;
        int output_n_buffers = 0;

        // Allocate an additional buffer to be shown in case of signal loss
        input_ctxt->prev_buf_idx_qcarcam = -1;
        output_n_buffers = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers + 1;
        input_ctxt->p_buffers_output.n_buffers = output_n_buffers;
        input_ctxt->p_buffers_output.color_fmt = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].format;
        input_ctxt->p_buffers_output.buffers = (qcarcam_buffer_t *)calloc(input_ctxt->p_buffers_output.n_buffers, sizeof(*input_ctxt->p_buffers_output.buffers));
        input_ctxt->diag.bprint = bprint_diag;
        if (input_ctxt->p_buffers_output.buffers == 0)
        {
            QCARCAM_ERRORMSG("alloc qcarcam_buffer failed");
            goto fail;
        }

        qcarcam_test_get_time(&gCtxt.t_before);
        for (i = 0; i < output_n_buffers; ++i)
        {
            input_ctxt->p_buffers_output.buffers[i].n_planes = 1;
            input_ctxt->p_buffers_output.buffers[i].planes[0].width = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].width;
            input_ctxt->p_buffers_output.buffers[i].planes[0].height = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].height;
        }

        ret = test_util_init_window(test_util_ctxt, &input_ctxt->qcarcam_window);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_init_window failed for qcarcam_window");
            goto fail;
        }

        test_util_set_diag(test_util_ctxt, input_ctxt->qcarcam_window, &input_ctxt->diag);

        if (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace != SW_BOB_30FPS && gCtxt.enable_deinterlace != SW_BOB_60FPS))
        {
            ret = test_util_init_window(test_util_ctxt, &input_ctxt->display_window);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_init_window failed for display_window");
                goto fail;
            }
            test_util_set_diag(test_util_ctxt, input_ctxt->display_window, &input_ctxt->diag);
        }

        if (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace != SW_BOB_30FPS && gCtxt.enable_deinterlace != SW_BOB_60FPS))
        {
            ret = test_util_set_window_param(test_util_ctxt, input_ctxt->display_window, &input_ctxt->window_params);
        }
        else
        {
            ret = test_util_set_window_param(test_util_ctxt, input_ctxt->qcarcam_window, &input_ctxt->window_params);
        }

        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_set_window_param failed");
            goto fail;
        }

        ret = test_util_init_window_buffers(test_util_ctxt, input_ctxt->qcarcam_window, &input_ctxt->p_buffers_output);
        if (ret != QCARCAM_RET_OK)
        {
            QCARCAM_ERRORMSG("test_util_init_window_buffers failed");
            goto fail;
        }

        if (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace != SW_BOB_30FPS && gCtxt.enable_deinterlace != SW_BOB_60FPS))
        {
            input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].format = QCARCAM_FMT_UYVY_8;
            input_ctxt->p_buffers_disp.color_fmt = (qcarcam_color_fmt_t)input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].format;
            input_ctxt->p_buffers_disp.n_buffers = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers;
            input_ctxt->p_buffers_disp.buffers = (qcarcam_buffer_t *)calloc(input_ctxt->p_buffers_disp.n_buffers, sizeof(*input_ctxt->p_buffers_disp.buffers));
            if (input_ctxt->p_buffers_disp.buffers == 0)
            {
                QCARCAM_ERRORMSG("alloc qcarcam_buffer failed");
                goto fail;
            }

            for (i = 0; i < input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers; ++i)
            {
                input_ctxt->p_buffers_disp.buffers[i].n_planes = 1;
                input_ctxt->p_buffers_disp.buffers[i].planes[0].width = input_ctxt->window_params.buffer_size[0];
                input_ctxt->p_buffers_disp.buffers[i].planes[0].height = input_ctxt->window_params.buffer_size[1];
            }

            ret = test_util_init_window_buffers(test_util_ctxt, input_ctxt->display_window, &input_ctxt->p_buffers_disp);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_init_window_buffers failed");
                goto fail;
            }
        }

        qcarcam_test_get_time(&gCtxt.t_after);
        QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam window init (input_id %d) : %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));

        input_ctxt->p_buffers_output.n_buffers = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers;

        if (gCtxt.enable_c2d)
        {
            qcarcam_test_get_time(&gCtxt.t_before);
            for (i = 0; i < input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers; ++i)
            {
                test_util_create_c2d_surface(input_ctxt->test_util_ctxt, input_ctxt->qcarcam_window, i);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("test_util_create_c2d_surface failed");
                    goto fail;
                }
            }

            qcarcam_test_get_time(&gCtxt.t_after);
            QCARCAM_PERFMSG(gCtxt.enableStats, "qcarcam create_c2d_surface (input_id %d) : %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));

            qcarcam_test_get_time(&gCtxt.t_before);

            /*display window*/
            input_ctxt->p_buffers_disp.n_buffers = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers;
            input_ctxt->p_buffers_disp.color_fmt = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].format;
            input_ctxt->p_buffers_disp.buffers = (qcarcam_buffer_t *)calloc(input_ctxt->p_buffers_disp.n_buffers, sizeof(*input_ctxt->p_buffers_disp.buffers));
            if (input_ctxt->p_buffers_disp.buffers == 0)
            {
                QCARCAM_ERRORMSG("alloc qcarcam_buffer failed");
                goto fail;
            }

            for (i = 0; i < input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers; ++i)
            {
                input_ctxt->p_buffers_disp.buffers[i].n_planes = 1;
                input_ctxt->p_buffers_disp.buffers[i].planes[0].width = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].width;
                input_ctxt->p_buffers_disp.buffers[i].planes[0].height = input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].height;
            }

            ret = test_util_init_window(test_util_ctxt, &input_ctxt->display_window);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_init_window failed");
                goto fail;
            }

            test_util_set_diag(test_util_ctxt, input_ctxt->display_window, &input_ctxt->diag);

            ret = test_util_set_window_param(test_util_ctxt, input_ctxt->display_window, &input_ctxt->window_params);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_set_window_param failed");
                goto fail;
            }

            ret = test_util_init_window_buffers(test_util_ctxt, input_ctxt->display_window, &input_ctxt->p_buffers_disp);
            if (ret != QCARCAM_RET_OK)
            {
                QCARCAM_ERRORMSG("test_util_init_window_buffers failed");
                goto fail;
            }

            qcarcam_test_get_time(&gCtxt.t_after);
            QCARCAM_PERFMSG(gCtxt.enableStats, "display window init (input_id %d) : %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));

            qcarcam_test_get_time(&gCtxt.t_before);

            for (i = 0; i < input_ctxt->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers; ++i)
            {
                test_util_create_c2d_surface(input_ctxt->test_util_ctxt, input_ctxt->display_window, i);
                if (ret != QCARCAM_RET_OK)
                {
                    QCARCAM_ERRORMSG("test_util_create_c2d_surface failed");
                    goto fail;
                }
            }

            qcarcam_test_get_time(&gCtxt.t_after);
            QCARCAM_PERFMSG(gCtxt.enableStats, "display create_c2d_surface (input_id %d) : %lu ms", (int)input_ctxt->qcarcam_input_id, (gCtxt.t_after - gCtxt.t_before));
        }
    }


    QCARCAM_DBGMSG("Screen Setup End");
    QCARCAM_DBGMSG("Create qcarcam_context Begin");

    /*launch threads to do the work for multithreaded*/
    if (gCtxt.multithreaded)
    {
        for (input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
        {
            g_test_inputs[input_idx].idx = input_idx;
            snprintf(thread_name, sizeof(thread_name), "inpt_ctxt_%d", input_idx);

            rc = CameraCreateThread(QCARCAM_THRD_PRIO, 0, &qcarcam_test_setup_input_ctxt_thread, &g_test_inputs[input_idx], 0, thread_name, &g_test_inputs[input_idx].thread_handle);
            if (rc)
            {
                QCARCAM_ERRORMSG("CameraCreateThread failed : %s", thread_name);
                goto fail;
            }
        }

        if (gCtxt.exitSeconds)
        {
            snprintf(thread_name, sizeof(thread_name), "timer_thread");
            rc = CameraCreateThread(QCARCAM_THRD_PRIO, 0, &timer_thread, 0, 0, thread_name, &timer_thread_handle);
            if (rc)
            {
                QCARCAM_ERRORMSG("CameraCreateThread failed, rc=%d", rc);
                goto fail;
            }
        }

        if (gCtxt.enableMenuMode)
        {
            while (!g_aborted)
            {
                if (gCtxt.started_stream_cnt < gCtxt.numInputs)
                {
                    // Wait till all streams starts
                    usleep(10000);
                    continue;
                }
                fflush(stdin);
                display_menu();

                while (!g_aborted)
                {
                    int ch = 0;
                    char buf[BUFSIZE];
                    fd_set readfds;
                    struct timeval tv;
                    FD_ZERO(&readfds);
                    FD_SET(fileno(stdin), &readfds);

                    tv.tv_sec = 0;
                    tv.tv_usec = 100000;

                    int num = select(1, &readfds, NULL, NULL, &tv);
                    if (num > 0)
                    {
                        if (fgets(buf, sizeof(buf), stdin) != NULL)
                            ch = buf[0];
                        if (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z'))
                        {
                            process_cmds(ch);
                            break;
                        }
                    }
                }
            }
        }

        /*Wait on all of them to join*/
        for (input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
        {
            CameraJoinThread(g_test_inputs[input_idx].thread_handle, NULL);
        }
    }
    else
    {
        /*singlethreaded - init and start each camera sequentially*/
        for (input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
        {
            g_test_inputs[input_idx].idx = input_idx;
            if (0 != qcarcam_test_setup_input_ctxt_thread(&g_test_inputs[input_idx]))
            {
                QCARCAM_ERRORMSG("qcarcam_test_setup_input_ctxt_thread failed");
                goto fail;
            }
        }

        for (input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
        {
            g_test_inputs[input_idx].is_running = 1;
            ret = qcarcam_start(g_test_inputs[input_idx].qcarcam_context);
            if (ret != QCARCAM_RET_OK)
            {
                g_test_inputs[input_idx].is_running = 0;
                QCARCAM_ERRORMSG("qcarcam_start() failed");
                if (g_test_inputs[input_idx].qcarcam_context)
                {
                    qcarcam_close(g_test_inputs[input_idx].qcarcam_context);
                    g_test_inputs[input_idx].qcarcam_context = NULL;
                }
            }
        }

        if (gCtxt.exitSeconds)
        {
            snprintf(thread_name, sizeof(thread_name), "timer_thread");
            rc = CameraCreateThread(QCARCAM_THRD_PRIO, 0, &timer_thread, 0, 0, thread_name, &timer_thread_handle);
            if (rc)
            {
                QCARCAM_ERRORMSG("CameraCreateThread failed, rc=%d", rc);
                goto fail;
            }
        }

        while (!g_aborted)
        {
            for (input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
            {
                if (!g_test_inputs[input_idx].use_event_callback)
                {
                    if (qcarcam_test_handle_new_frame(&g_test_inputs[input_idx]))
                        break;
                }
                else
                {
                    sched_yield();
                }
            }
        }
    }

fail:
    // cleanup
    for (input_idx = 0; input_idx < gCtxt.numInputs; input_idx++)
    {
        if (g_test_inputs[input_idx].qcarcam_context != NULL)
        {
            (void)qcarcam_stop(g_test_inputs[input_idx].qcarcam_context);
            (void)qcarcam_close(g_test_inputs[input_idx].qcarcam_context);
            g_test_inputs[input_idx].qcarcam_context = NULL;
        }

        test_util_deinit_window_buffer(test_util_ctxt, g_test_inputs[input_idx].qcarcam_window);
        test_util_deinit_window(test_util_ctxt, g_test_inputs[input_idx].qcarcam_window);

        if (gCtxt.enable_c2d || (gCtxt.enable_deinterlace && (gCtxt.enable_deinterlace != SW_BOB_30FPS && gCtxt.enable_deinterlace != SW_BOB_60FPS)))
        {
            test_util_deinit_window_buffer(test_util_ctxt, g_test_inputs[input_idx].display_window);
            test_util_deinit_window(test_util_ctxt, g_test_inputs[input_idx].display_window);
        }
        if (g_test_inputs[input_idx].p_buffers_output.buffers)
        {
            free(g_test_inputs[input_idx].p_buffers_output.buffers);
            g_test_inputs[input_idx].p_buffers_output.buffers = NULL;
        }

        if (g_test_inputs[input_idx].p_buffers_disp.buffers)
        {
            free(g_test_inputs[input_idx].p_buffers_disp.buffers);
            g_test_inputs[input_idx].p_buffers_disp.buffers = NULL;
        }
        memset(&g_test_inputs[input_idx], 0x0, sizeof(g_test_inputs[input_idx]));
    }

    if (pInputs)
    {
        free(pInputs);
        pInputs = NULL;
    }

    if (xml_inputs)
    {
        free(xml_inputs);
        xml_inputs = NULL;
    }

    if (!g_aborted)
    {
        abort_test();
    }

    test_util_deinit(test_util_ctxt);

    qcarcam_uninitialize();

    ais_log_uninit();

    return rval;
}
