/**********************************************************************//*!
* Copyright (c) Continental AG and subsidiaries
*
* \file     QCarCamLogic.h
*
* \brief    CameraLogic class head file
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
#include "CameraTypeDef.h"
#include "QCarCamInfo.h"
#include "QCarCamDiaplay.h"

#ifndef camera_QCarCamLogic_H_
#define camera_QCarCamLogic_H_



class QCarCamLogic  {
public:
    QCarCamLogic();
    virtual ~QCarCamLogic();

    /**
    * @brief        init camera
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int init();

    /**
    * @brief        deinit camera
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int deinit();

    /**
    * @brief        open camera device
    * @param[in]    camera input id
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int openCamera(qcarcam_input_desc_t input_id);

    /**
    * @brief        close camera device
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int closeCamera();

    /**
    * @brief        start camera
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int startCamera();

    /**
    * @brief        stop camera
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int stopCamera();


private:

    /**
    * @brief        query camera info
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int queryCameraInfo();

    /**
    * @brief        get input camera info from xml file
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int xmlInputCameraInfo();


    /**
    * @brief        handle new camera frame
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int handleCameraNewFrame();

    /**
    * @brief        get a new camera frame buffer
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int getCameraFrame();

    /**
    * @brief        post camera frame to display
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int postToDisplay();

    /**
    * @brief        release a camera frame buffer
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int releaseCameraFrame();

    /**
    * @brief        save opened camera input id
    */
    int mInputID;

    /**
    * @brief        save opened camera device handle
    */
    qcarcam_hndl_t qcarcam_context;

    /**
    * @brief        register camera event callback
    * @param[in]    camera device handle
    * @param[in]    camera event id
    * @param[in]    camera event info
    */
    static void qcarcam_test_event_cb(qcarcam_hndl_t hndl, qcarcam_event_t event_id, qcarcam_event_payload_t *p_payload);

    /**
    * @brief        save camera info 
    */
    QCarCamInfo mQCameraInfo;

    /**
    * @brief        dispaly camera instance
    */
    QCarCamDisplay* mpQCarCamDiaplay;

    static std::vector<QCarCamLogic*> mQCamList;

    /**
    * @brief        handle camera event
    * @param[in]    camera event id
    * @param[in]    camera event info
    */
    void handleCameraEvent(qcarcam_event_t event_id, int payload);

    /**
    * @brief        handle camera inpu signal
    * @param[in]    camera event info
    */
    void handleCameraInputSignal(int payload);

    /**
    @brief  mutex lock
    */
    static std::mutex mLock;

    /**
    @brief  camera run status
    */
    std::atomic<bool> isRunning;

};

#endif // camera_QCarCamLogic_H_

