/**********************************************************************//*!
* Copyright (c) Continental AG and subsidiaries
*
* \file     QCarCamInfo.h
*
* \brief    QCarCamInfo class
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
#include "QCarCamInfo.h"


QCarCamInfo::QCarCamInfo() 
{
    qcarcam_window = NULL;

    queryNumInputs = 0;

    frameCnt = 0;

    frame_timeout = 500000000;

    xml_inputs = (test_util_xml_input_t *)calloc(1, sizeof(test_util_xml_input_t));

    pInputs = (qcarcam_input_t *)calloc(16, sizeof(qcarcam_input_t));

    memset(&is_buf_dequeued, 0x0, sizeof(is_buf_dequeued));
}

QCarCamInfo::~QCarCamInfo()
{
    free(xml_inputs); 
    free(pInputs); 

    if(NULL == p_buffers_output.buffers)
    {
        free(p_buffers_output.buffers);
    }
}


int  QCarCamInfo::initOutputBuffer()
{
    int output_n_buffers = 0;

    // Allocate an additional buffer to be shown in case of signal loss
    // input_ctxt->prev_buf_idx_qcarcam = -1;
    output_n_buffers = buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers + 1;
    p_buffers_output.n_buffers = output_n_buffers;
    p_buffers_output.color_fmt = buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].format;
    p_buffers_output.buffers = (qcarcam_buffer_t *)calloc(p_buffers_output.n_buffers, sizeof(*p_buffers_output.buffers));

    if (p_buffers_output.buffers == NULL)
    {
        QCARCAM_ERRORMSG("alloc qcarcam_buffer failed");
        return false;
    }

    for (int i = 0; i < output_n_buffers; ++i)
    {
        p_buffers_output.buffers[i].n_planes = 1;
        p_buffers_output.buffers[i].planes[0].width = buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].width;
        p_buffers_output.buffers[i].planes[0].height = buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].height;
    }

    return true;
}


