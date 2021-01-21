/**********************************************************************//*!
* Copyright (c) Continental AG and subsidiaries
*
* \file     CameraTypeDef.h
*
* \brief    Camera type define
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

#ifndef __Camera_Type_Def_H_
#define __Camera_Type_Def_H_

#include "CameraOSServices.h"
#include "CameraQueue.h"
#include "CameraTypes.h"
#include "qcarcam.h"
#include "test_util.h"

//if need to modify this enum type,
//please sync with device/qcom/sm6150_au/conti/d01/native/tsw/tsw_common/pub/ITswtTestCommonData.h
enum QCAR_TSW_TEST_CAMERA_ERROR_TYPE
{
    TSW_HD_CAMERA_OK = 0,
    TSW_CAMERA_HAS_BEEN_OPENED = 0x10,                  //camera has been opened
    TSW_CAMERA_HAS_BEEN_CLOSED = 0x11,                  //camera has been closed
    TSW_CAMERA_QUERY_CAMERA_NUM_FAILED = 0x12,          //query camera number failed
    TSW_CAMERA_QUERY_CAMERA_INFO_FAILED = 0X13,         //query camera info failed
    TSW_CAMERA_OPEN_CAMERA_FAILED = 0X14,               //open camera failed 
    TSW_CAMERA_SET_BUFFER_FAILED = 0X15,                //set camera buffer failed
    TSW_CAMERA_NOT_OPEN_CAMERA   = 0X16,                //no camera device was opened
    TSW_CAMERA_START_CAMERA_FAILED = 0X17,              //start camera failed
    TSW_CAMERA_STOP_CAMERA_FAILED = 0X18,               //stop camera failed
    TSW_CAMERA_CLOSE_CAMERA_FAILED = 0X19,              //close camera failed
    TSW_CAMERA_OPEN_TEST_LIB_FAILED = 0X1A,             //open camera tsw test lib failed
};

typedef enum
{
    QCARCAM_TEST_BUFFERS_OUTPUT = 0,
    QCARCAM_TEST_BUFFERS_DISPLAY,
    QCARCAM_TEST_BUFFERS_INPUT,
    QCARCAM_TEST_BUFFERS_MAX
} qcarcam_buffers_enum;

typedef struct
{
    qcarcam_buffers_t p_buffers;

    qcarcam_color_fmt_t format;
    int n_buffers;
    unsigned int width;
    unsigned int height;
} qcarcam_buffers_param_t;

#endif //__Camera_Type_Def_H_
