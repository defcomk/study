/**********************************************************************//*!
* Copyright (c) Continental AG and subsidiaries
*
* \file     QCarCamDiaplay.h
*
* \brief    Camera display class head file
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

#include "QCarCamInfo.h"

#ifndef camera_QCarCamDisplay_H_
#define camera_QCarCamDisplay_H_



class QCarCamDisplay  {
public:
    QCarCamDisplay(QCarCamInfo *mPQCarCamInfo);
    virtual ~QCarCamDisplay();

    /**
    * @brief        post camera frame buffer to display
    * @return       true is successful, false is failed
    */
    int postToDisplay();

    /**
    * @brief       init window buffer
    * @return      true is successful, false is failed
    */
    int init();

    /**
    * @brief       deinit window buffer
    * @return      true is successful, false is failed
    */
    int deinit();


private:

    /**
    * @brief       set window params
    * @return      true is successful, false is failed
    */
    int setWindowParam();

    /**
    * @brief       allocate window buffer
    * @return      true is successful, false is failed
    */
    int setWindowBuffer();

    QCarCamInfo *mPQCarCamInfo;

    //save windos params
    test_util_window_param_t window_params;

    //the pointer of test util handle
    test_util_ctxt_t *test_util_ctxt;

    //test util init params
    test_util_ctxt_params_t test_util_ctxt_params;
};

#endif // camera_QCarCamDisplay_H_

