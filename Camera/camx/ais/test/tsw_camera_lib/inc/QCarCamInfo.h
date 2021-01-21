/**********************************************************************//*!
* Copyright (c) Continental AG and subsidiaries
*
* \file     QCarCamInfo.h
*
* \brief    Camera info class head file
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

#include <log/log.h>
#include <string>
#include <thread>
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
#include <vector>
#include <atomic>

#include "qcarcam.h"

#include "test_util.h"

#include "CameraTypeDef.h"

#ifndef camera_QCarCamInfo_H_
#define camera_QCarCamInfo_H_

class QCarCamInfo  {
public:
    QCarCamInfo();
    virtual ~QCarCamInfo();

    /**
    * @brief        create output buffer to store buffer infomation,
                    not camera buffer.
    */
    int  initOutputBuffer();

    /**
    * @brief        a test util windos buffer 
                    created by test util.
    */
    test_util_window_t *qcarcam_window;

    /**
    * @brief        store camera input camera info from xml file
    */
    test_util_xml_input_t *xml_inputs;

    /**
    * @brief        sotre camera info from from qcarcam_query_inputs
    */
    qcarcam_input_t *pInputs;

    /**
    * @brief        the pointer of camera output buffer info
                    not camera buffer.
    */
    qcarcam_buffers_t p_buffers_output;

    /**
    * @brief        store buffer infomation
                    not camera buffer.
    */
    qcarcam_buffers_param_t buffers_param[QCARCAM_TEST_BUFFERS_MAX];

    /**
    * @brief        store input camera number
                    in didi project only one camera
    */
    unsigned int queryNumInputs;

    //post display
    unsigned long long int frame_timeout;
    int frameCnt;
    int idx;
    int buf_idx_qcarcam;
    int prev_buf_idx_qcarcam;

    bool is_buf_dequeued[QCARCAM_MAX_NUM_BUFFERS];
    
    qcarcam_field_t field_type_previous;


private:


};

#endif // camera_QCarCamInfo_H_

