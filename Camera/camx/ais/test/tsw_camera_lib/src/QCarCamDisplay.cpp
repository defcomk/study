/**********************************************************************//*!
* Copyright (c) Continental AG and subsidiaries
*
* \file     QCarCamDisplay.cpp
*
* \brief    Camera display class
*
*
* \author   Xu Liyan
*
* \date     25 Sep 2019
*
* \sa       

Module-History:
Date(dd.mm.year)| Author            | Reason
25.9.2019|Xu Liyan | new created for DIDI-3877 TSW: HD Camera video Test
The latest entry at the head of the history list.
*/

#define LOG_TAG "QCarCamDisplay@CameraDV"
#include "QCarCamDiaplay.h"

QCarCamDisplay::QCarCamDisplay(QCarCamInfo *pQCarCamInfo) 
{
    mPQCarCamInfo = pQCarCamInfo;
    test_util_ctxt = NULL;
    memset(&test_util_ctxt_params, 0x0, sizeof(test_util_ctxt_params));

    window_params = pQCarCamInfo->xml_inputs->window_params;
    window_params.buffer_size[0] = pQCarCamInfo->xml_inputs->output_params.buffer_size[0];
    window_params.buffer_size[1] = pQCarCamInfo->xml_inputs->output_params.buffer_size[1];


    /*@todo: move these buffer settings to be queried from qcarcam and not set externally*/
    pQCarCamInfo->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers = pQCarCamInfo->xml_inputs->output_params.n_buffers;


    pQCarCamInfo->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].width = pQCarCamInfo->pInputs[0].res[0].width;
    pQCarCamInfo->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].height = pQCarCamInfo->pInputs[0].res[0].height;
    pQCarCamInfo->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].format = pQCarCamInfo->pInputs[0].color_fmt[0];

    pQCarCamInfo->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].n_buffers = pQCarCamInfo->xml_inputs->window_params.n_buffers_display;
    pQCarCamInfo->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].width = pQCarCamInfo->xml_inputs->output_params.buffer_size[0];
    pQCarCamInfo->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].height = pQCarCamInfo->xml_inputs->output_params.buffer_size[1];
    pQCarCamInfo->buffers_param[QCARCAM_TEST_BUFFERS_DISPLAY].format = QCARCAM_FMT_RGB_888;

}

QCarCamDisplay::~QCarCamDisplay()
{
}

int QCarCamDisplay::init()
{
    int ret;

    ret = test_util_init(&test_util_ctxt, &test_util_ctxt_params);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_init failed");
        return false;
    }

    ret= test_util_init_window(test_util_ctxt, &(mPQCarCamInfo->qcarcam_window));

    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_init_window failed for qcarcam_window");
        return false;
    }

    ret = setWindowParam();

    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_set_window_param failed");
        return false;
    }

    ret = setWindowBuffer();
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_init_window_buffers failed");
        return false;
    }

    return true;

}

int QCarCamDisplay::deinit()
{

    test_util_deinit_window_buffer(test_util_ctxt, mPQCarCamInfo->qcarcam_window);
    test_util_deinit_window(test_util_ctxt, mPQCarCamInfo->qcarcam_window);

    test_util_deinit(test_util_ctxt);

    return true;

}


int QCarCamDisplay::setWindowParam()
{
    int ret;

    /**
    @brief  set display param function
    @param[in]  test util handle
    @param[out] store camera window params, the buffer was created by test_util
    @param[in] input window params
    */
    ret = test_util_set_window_param(test_util_ctxt, mPQCarCamInfo->qcarcam_window, &window_params);

    return ret;
}


int QCarCamDisplay::setWindowBuffer()
{
    int ret;


    if (mPQCarCamInfo->initOutputBuffer() == false)
    {
        QCARCAM_ERRORMSG("alloc qcarcam_buffer failed");
        return false;
    }

    ret = test_util_init_window_buffers(test_util_ctxt, mPQCarCamInfo->qcarcam_window, &mPQCarCamInfo->p_buffers_output);

    //recover buffer number 
    mPQCarCamInfo->p_buffers_output.n_buffers = mPQCarCamInfo->buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers;

    return ret;
}

int QCarCamDisplay::postToDisplay()
{
    qcarcam_ret_t ret;
    QCARCAM_INFOMSG("Post Frame before buf_idx %i", mPQCarCamInfo->buf_idx_qcarcam);

    /**********************
     * Post to screen
     ********************** */
    QCARCAM_INFOMSG("Post Frame %d", mPQCarCamInfo->buf_idx_qcarcam);

    ret = test_util_post_window_buffer(test_util_ctxt, mPQCarCamInfo->qcarcam_window, mPQCarCamInfo->buf_idx_qcarcam, &mPQCarCamInfo->prev_buf_idx_qcarcam, mPQCarCamInfo->field_type_previous);
    
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("test_util_post_window_buffer failed");
    }


    QCARCAM_INFOMSG("Post Frame after buf_idx %i", mPQCarCamInfo->buf_idx_qcarcam);

    return 0;
}
