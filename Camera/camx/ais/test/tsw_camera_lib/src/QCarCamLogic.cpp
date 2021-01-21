/* ************************************************************************
* Copyright (c) Continental AG and subsidiaries
* All rights reserved
*
* The reproduction, transmission or use of this document or its contents is
* not permitted without express written authority.
* Offenders will be liable for damages. All rights, including rights created
* by patent grant or registration of a utility model or design, are reserved.
* ************************************************************************/

/**
@file     ChimeVehicleCallback.cpp
@brief    This file is a vendor implementation of ChimeVehicleCallback.h
@par      Project: DIDI CHJ MY20
@par      SW Component: Chime service
@par      SW Package: service
@author   Liang13 Wang
@par      Configuration Management:
@todo     list of things to do
*/

#define LOG_TAG "QCarCamLogic@CameraTSW"
#include "QCarCamLogic.h"

///////////////////////////////
/// STATICS
///////////////////////////////
std::mutex QCarCamLogic::mLock;

std::vector<QCarCamLogic*> QCarCamLogic::mQCamList;


QCarCamLogic::QCarCamLogic()//:mQCameraInfo()
{

    ais_log_init(NULL, (char *)"tsw_camera");
    mQCamList.clear();
    mInputID = 0;
    isRunning = false;

}

QCarCamLogic::~QCarCamLogic()
{
    ais_log_uninit();
}

int QCarCamLogic::init()
{
    std::lock_guard<std::mutex> g(mLock);

    int ret = 0;

    mQCamList.push_back(this);

    ret = queryCameraInfo();

    xmlInputCameraInfo();

    mpQCarCamDiaplay = new QCarCamDisplay(&mQCameraInfo);

    mpQCarCamDiaplay->init();

    return ret;
}

int QCarCamLogic::deinit()
{
    std::lock_guard<std::mutex> g(mLock);

    if(NULL != mpQCarCamDiaplay)
    {
        mpQCarCamDiaplay->deinit();

        mpQCarCamDiaplay = NULL;
    }


    mQCamList.clear();

    return TSW_HD_CAMERA_OK;
}

int QCarCamLogic::queryCameraInfo()
{
    int i;
    int ret;
    unsigned int queryFilled = 0;

    unsigned int qcarcam_version = (QCARCAM_VERSION_MAJOR << 8) | (QCARCAM_VERSION_MINOR);

    //query camera input number
    ret = qcarcam_query_inputs(NULL, 0, &(mQCameraInfo.queryNumInputs));
    if (QCARCAM_RET_OK != ret || mQCameraInfo.queryNumInputs == 0)
    {
        QCARCAM_ERRORMSG("Failed qcarcam_query_inputs number of inputs with ret %d", ret);

        return TSW_CAMERA_QUERY_CAMERA_NUM_FAILED;
    }

    QCARCAM_ALWZMSG("queryNumInputs %d", mQCameraInfo.queryNumInputs);
    
    //set qcarcam client version
    mQCameraInfo.pInputs[0].flags = qcarcam_version;

    //query each input camera information, in didi project only one camera
    ret = qcarcam_query_inputs(mQCameraInfo.pInputs, mQCameraInfo.queryNumInputs, &queryFilled);
    if (QCARCAM_RET_OK != ret || queryFilled != mQCameraInfo.queryNumInputs)
    {
        QCARCAM_ERRORMSG("Failed qcarcam_query_inputs with ret %d %d %d", ret, queryFilled, mQCameraInfo.queryNumInputs);
        return TSW_CAMERA_QUERY_CAMERA_INFO_FAILED;
    }

    QCARCAM_ALWZMSG("queryFilled %d ", queryFilled);

    QCARCAM_ALWZMSG("--- QCarCam Queried Inputs ----");
    for (i = 0; i < (int)queryFilled; i++)
    {
        QCARCAM_ALWZMSG("%d: input_id=%d, res=%dx%d fmt=0x%08x fps=%.2f", i, mQCameraInfo.pInputs[i].desc,
                        mQCameraInfo.pInputs[i].res[0].width, mQCameraInfo.pInputs[i].res[0].height,
                        mQCameraInfo.pInputs[i].color_fmt[0], mQCameraInfo.pInputs[i].res[0].fps);
    }
    QCARCAM_ALWZMSG("-------------------------------");

    return TSW_HD_CAMERA_OK;
}

int QCarCamLogic::xmlInputCameraInfo()
{
    int numInputs = 0;

    numInputs = test_util_parse_xml_config_file("/system/bin/1cam.xml", mQCameraInfo.xml_inputs, 1);
    if (numInputs <= 0)
    {
        QCARCAM_ERRORMSG("Failed to parse config file!");
        // exit(-1);
        mQCameraInfo.xml_inputs->properties.qcarcam_input_id = QCARCAM_INPUT_TYPE_EXT_REAR;
        mQCameraInfo.xml_inputs->properties.use_event_callback = 1;
        mQCameraInfo.xml_inputs->properties.frame_timeout =-1;
        mQCameraInfo.xml_inputs->output_params.buffer_size[0] = 1280;
        mQCameraInfo.xml_inputs->output_params.buffer_size[1] = 720;
        mQCameraInfo.xml_inputs->output_params.stride=-1;
        mQCameraInfo.xml_inputs->output_params.n_buffers = 6;
        mQCameraInfo.xml_inputs->window_params.display_id = 0;
        mQCameraInfo.xml_inputs->window_params.window_size[0] =0.25;
        mQCameraInfo.xml_inputs->window_params.window_size[1]=0.25;
        mQCameraInfo.xml_inputs->window_params.window_pos[0] = 0.35;
        mQCameraInfo.xml_inputs->window_params.window_pos[1] = 0;
        mQCameraInfo.xml_inputs->window_params.window_source_size[0] = 0.1;
        mQCameraInfo.xml_inputs->window_params.window_source_size[1] = 0.1;
        mQCameraInfo.xml_inputs->window_params.window_source_pos[0] =0;
        mQCameraInfo.xml_inputs->window_params.window_source_pos[1]=0;
        mQCameraInfo.xml_inputs->window_params.zorder=0;
        mQCameraInfo.xml_inputs->window_params.pipeline_id = -1;
        mQCameraInfo.xml_inputs->window_params.n_buffers_display = 6;
        mQCameraInfo.xml_inputs->window_params.format= TESTUTIL_FMT_UYVY_8;
        mQCameraInfo.xml_inputs->exp_params.exposure_time= 31.147f;
        mQCameraInfo.xml_inputs->exp_params.gain= 1.5f;
        mQCameraInfo.xml_inputs->exp_params.manual_exp= 0;
    }

    return TSW_HD_CAMERA_OK;
}

int QCarCamLogic::openCamera(qcarcam_input_desc_t input_id)
{
    int ret;
    mInputID = input_id;

    //open camera device
    qcarcam_context = qcarcam_open(input_id);
    if (qcarcam_context == NULL)
    {
        QCARCAM_ERRORMSG("qcarcam_open() failed");
        return TSW_CAMERA_OPEN_CAMERA_FAILED;
    }

    ret = qcarcam_s_buffers(qcarcam_context, &mQCameraInfo.p_buffers_output);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("qcarcam_s_buffers() failed ret %d", ret);
        return TSW_CAMERA_SET_BUFFER_FAILED;
    }

    //register camera callback
    qcarcam_param_value_t param;
    param.ptr_value = (void *)qcarcam_test_event_cb;
    ret = qcarcam_s_param(qcarcam_context, QCARCAM_PARAM_EVENT_CB, &param);

    //register event type
    param.uint_value = QCARCAM_EVENT_FRAME_READY | QCARCAM_EVENT_INPUT_SIGNAL | QCARCAM_EVENT_ERROR;
    ret = qcarcam_s_param(qcarcam_context, QCARCAM_PARAM_EVENT_MASK, &param);

    return TSW_HD_CAMERA_OK;

}

int QCarCamLogic::startCamera()
{
    int ret;

    if(qcarcam_context == 0)
    {
        QCARCAM_ALWZMSG("qcarcam_context is null");
        return TSW_CAMERA_NOT_OPEN_CAMERA;
    }
    
    ret = qcarcam_start(qcarcam_context);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("qcarcam_start() failed");
        return TSW_CAMERA_START_CAMERA_FAILED;
    }

    isRunning = true;
    return TSW_HD_CAMERA_OK;
}

int QCarCamLogic::stopCamera()
{
    int ret = false;
    QCARCAM_ALWZMSG("qcarcam_stop start");

    if(qcarcam_context == 0)
    {
        QCARCAM_ALWZMSG("qcarcam_context is null");
        return TSW_CAMERA_NOT_OPEN_CAMERA;
    }

    ret = qcarcam_stop(qcarcam_context);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("qcarcam_stop failed ret %d", ret);
        return TSW_CAMERA_STOP_CAMERA_FAILED;
    }
    QCARCAM_ALWZMSG("qcarcam_stop end");

    isRunning = false;
    return TSW_HD_CAMERA_OK;
}

int QCarCamLogic::closeCamera()
{
    int ret = false;

    QCARCAM_ALWZMSG("qcarcam_close start");

    if(qcarcam_context == 0)
    {
        QCARCAM_ALWZMSG("qcarcam_context is null");
        return TSW_CAMERA_NOT_OPEN_CAMERA;
    }


    ret = qcarcam_close(qcarcam_context);
    if (ret != QCARCAM_RET_OK)
    {
        QCARCAM_ERRORMSG("qcarcam_close failed ret %d", ret);
        return TSW_CAMERA_CLOSE_CAMERA_FAILED;
    }
    QCARCAM_ALWZMSG("qcarcam_close end");

    return TSW_HD_CAMERA_OK;
}

void QCarCamLogic::handleCameraEvent(qcarcam_event_t event_id, int payload)
{
    switch (event_id)
    {
        case QCARCAM_EVENT_FRAME_READY:
            handleCameraNewFrame();
            break;
        case QCARCAM_EVENT_INPUT_SIGNAL:
            handleCameraInputSignal(payload);
            QCARCAM_INFOMSG("QCARCAM_EVENT_INPUT_SIGNAL payload %d", payload);
            break;
        case QCARCAM_EVENT_ERROR:
            QCARCAM_INFOMSG("QCARCAM_EVENT_ERROR payload %d", payload);
            break;
        default:
            QCARCAM_INFOMSG("received unsupported event %d", event_id);
            break;
    }
}

int QCarCamLogic::handleCameraNewFrame()
{
    int ret = false;
    ret = getCameraFrame();


    if(false == ret)
    {
        QCARCAM_ALWZMSG("getCameraFrame failed");
        return false;
    }

    mpQCarCamDiaplay->postToDisplay();

    releaseCameraFrame();

    return true;
}


int QCarCamLogic::getCameraFrame()
{
    qcarcam_ret_t ret;
    qcarcam_frame_info_t frame_info;
    ret = qcarcam_get_frame(qcarcam_context, &frame_info, mQCameraInfo.frame_timeout, 0);
    if (ret == QCARCAM_RET_TIMEOUT)
    {
        QCARCAM_ERRORMSG("input_id %d qcarcam_get_frame timeout context 0x%p ret %d\n", mInputID, qcarcam_context, ret);
        // input_ctxt->signal_lost = 1;
        return -1;
    }

    if (QCARCAM_RET_OK != ret)
    {
        QCARCAM_ERRORMSG("input_id %d Get frame context 0x%p ret %d\n", mInputID, qcarcam_context, ret);
        return -1;
    }

    if (frame_info.idx >= mQCameraInfo.buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers)
    {
        QCARCAM_ERRORMSG("input_id %d Get frame context 0x%p ret invalid idx %d\n", mInputID, qcarcam_context, frame_info.idx);
        return -1;
    }

    if (mQCameraInfo.frameCnt == 0)
    {
        // unsigned long t_to;
        // qcarcam_test_get_time(&t_to);
        // ais_log_kpi(AIS_EVENT_KPI_CLIENT_FIRST_FRAME);
        mQCameraInfo.field_type_previous = QCARCAM_FIELD_UNKNOWN;
        printf("Success\n");
        fflush(stdout);
        // QCARCAM_ALWZMSG("Get First Frame input_id %d buf_idx %i after : %lu ms, field type: %d\n", (int)input_ctxt->qcarcam_input_id, input_ctxt->mQCameraInfo.buf_idx_qcarcam, (t_to - gCtxt.t_start), frame_info.field_type);
    }

    mQCameraInfo.buf_idx_qcarcam = frame_info.idx;

    QCARCAM_INFOMSG("input_id:%d mQCameraInfo.frameCnt:%d idx:%d", mInputID, mQCameraInfo.frameCnt, frame_info.idx);

    mQCameraInfo.is_buf_dequeued[mQCameraInfo.buf_idx_qcarcam] = 1;
    mQCameraInfo.frameCnt++;

    return true;
}


/**
 * Release frame back to qcarcam
 * @param input_ctxt
 * @return 0 on success, -1 on failure
 */
int QCarCamLogic::releaseCameraFrame()
{
    qcarcam_ret_t ret;
    if (mQCameraInfo.prev_buf_idx_qcarcam >= 0 && mQCameraInfo.prev_buf_idx_qcarcam < mQCameraInfo.buffers_param[QCARCAM_TEST_BUFFERS_OUTPUT].n_buffers)
    {
        if (mQCameraInfo.is_buf_dequeued[mQCameraInfo.prev_buf_idx_qcarcam])
        {
            ret = qcarcam_release_frame(qcarcam_context, mQCameraInfo.prev_buf_idx_qcarcam);
            if (QCARCAM_RET_OK != ret)
            {
                QCARCAM_ERRORMSG("input_id %d qcarcam_release_frame() %d failed", mInputID, mQCameraInfo.prev_buf_idx_qcarcam);
                return -1;
            }
            mQCameraInfo.is_buf_dequeued[mQCameraInfo.prev_buf_idx_qcarcam] = 0;
        }
        else
        {
            QCARCAM_ERRORMSG("input_id %d qcarcam_release_frame() skipped since buffer %d not dequeued", mInputID, mQCameraInfo.prev_buf_idx_qcarcam);
        }
    }
    else
    {
        QCARCAM_ERRORMSG("input_id %d qcarcam_release_frame() skipped", mInputID);
    }

    mQCameraInfo.prev_buf_idx_qcarcam = mQCameraInfo.buf_idx_qcarcam;
    return 0;
}

void QCarCamLogic::handleCameraInputSignal(int signal_type)
{
    qcarcam_ret_t ret = QCARCAM_RET_OK;

    switch (signal_type) 
    {
        case QCARCAM_INPUT_SIGNAL_LOST:
        {
            QCARCAM_ERRORMSG("LOST: input: %d", mInputID);

            if (isRunning == false) 
            {
                QCARCAM_ERRORMSG("Input %d already stop, break", mInputID);
                break;
            }
            isRunning = false;
            ret = qcarcam_stop(qcarcam_context);
            if (ret == QCARCAM_RET_OK) 
            {
                QCARCAM_ERRORMSG("Input %d qcarcam_stop successfully", mInputID);
            } 
            else 
            {
                QCARCAM_ERRORMSG("Input %d qcarcam_stop failed: %d !", mInputID, ret);
            }

            mQCameraInfo.prev_buf_idx_qcarcam = -1;
            memset(&mQCameraInfo.is_buf_dequeued, 0x0, sizeof(mQCameraInfo.is_buf_dequeued));
            // input_ctxt->signal_lost = 1;

            break;
        }
        case QCARCAM_INPUT_SIGNAL_VALID:
        {
            QCARCAM_ERRORMSG("VALID: input: %d", mInputID);

            if (isRunning == true) 
            {
                QCARCAM_ERRORMSG("Input %d already running, break", mInputID);
                break;
            }
            isRunning = true;

            ret = qcarcam_s_buffers(qcarcam_context, &mQCameraInfo.p_buffers_output);

            if (ret == QCARCAM_RET_OK)
                QCARCAM_ERRORMSG("Input %d qcarcam_s_buffers successfully", mInputID);
            else
                QCARCAM_ERRORMSG("Input %d qcarcam_s_buffers failed: %d", mInputID, ret);

            ret = qcarcam_start(qcarcam_context);

            if (ret == QCARCAM_RET_OK) 
            {
                QCARCAM_ERRORMSG("Input %d qcarcam_start successfully", mInputID);
            } 
            else 
            {
                QCARCAM_ERRORMSG("Input %d qcarcam_start failed: %d", mInputID, ret);
            }

            break;
        }
        default:
            QCARCAM_ERRORMSG("Unknown Event type: %d", signal_type);
            break;
    }
}

void QCarCamLogic::qcarcam_test_event_cb(qcarcam_hndl_t hndl, qcarcam_event_t event_id, qcarcam_event_payload_t *p_payload)
{
    std::lock_guard<std::mutex> g(mLock);
    QCARCAM_INFOMSG("hndl %p , event_id %d", hndl, event_id);

    for (const auto &it : mQCamList)
    {
        if(it->qcarcam_context == hndl)
        {
            it->handleCameraEvent(event_id, p_payload->uint_payload);
        }
    }
}