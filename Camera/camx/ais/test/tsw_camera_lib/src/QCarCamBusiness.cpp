/**********************************************************************//*!
* Copyright (c) Continental AG and subsidiaries
*
* \file     QCarCamBusiness.cpp
*
* \brief    Camera business class
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

#define LOG_TAG "TswCameraTest@QCarCamBusiness"
#include "QCarCamBusiness.h"

QCarCamBusiness::QCarCamBusiness() 
{
    ALOGI("QCarCamBusiness ctor");
    mpQCarCamLogic = NULL;
}

QCarCamBusiness::~QCarCamBusiness()
{
    ALOGI("QCarCamBusiness dector");
}
/**********************************************************************
 * create a singleton class of LightImpl
 **********************************************************************/
QCarCamBusiness &QCarCamBusiness::GetInstance()
{
    static QCarCamBusiness QCarCamBusinessIns;

    return QCarCamBusinessIns;
}


int  QCarCamBusiness::openCamera()
{
    int ret = 0;
    if(NULL != mpQCarCamLogic)
    {
        ALOGI("mpQCarCamLogic is not null");

        return TSW_CAMERA_HAS_BEEN_OPENED;
    }

    do
    {
        ALOGI("create QCarCamLogic ins");
        mpQCarCamLogic = new QCarCamLogic();

        ret = mpQCarCamLogic->init();

        if(TSW_HD_CAMERA_OK != ret) break;

        ret = mpQCarCamLogic->openCamera(QCARCAM_INPUT_TYPE_EXT_REAR);

        if(TSW_HD_CAMERA_OK != ret) break;

        ret = mpQCarCamLogic->startCamera();
        
    } while (0);    

    return ret;
}

int  QCarCamBusiness::closeCamera()
{
    int ret = 0;

    if(NULL == mpQCarCamLogic)
    {
        ALOGI("mpQCarCamLogic is not null");

        return TSW_CAMERA_HAS_BEEN_CLOSED;
    }

    do
    {
        ALOGI("delete QCarCamLogic ins");

        ret = mpQCarCamLogic->stopCamera();

        ret = mpQCarCamLogic->closeCamera();

        ret = mpQCarCamLogic->deinit();


        delete mpQCarCamLogic; 

        mpQCarCamLogic = NULL;
    }while(0);    

    return ret;
}

int openCamera(void)
{
    ALOGI("openCamera start");

    QCarCamBusiness& QCarCam = QCarCamBusiness::GetInstance();

    return QCarCam.openCamera();
}

int closeCamera(void)
{
    ALOGI("closeCamera start");

    QCarCamBusiness& QCarCam = QCarCamBusiness::GetInstance();

    return QCarCam.closeCamera();
}


