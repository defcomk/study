/**********************************************************************//*!
* Copyright (c) Continental AG and subsidiaries
*
* \file     QCarCamBusiness.h
*
* \brief    Camera business head
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



#include "QCarCamLogic.h"
#include "IQCarCamInterface.h"

#ifndef camera_QCarCamBusiness_H_
#define camera_QCarCamBusiness_H_



class QCarCamBusiness : public IQCarCamInterface {
public:
    QCarCamBusiness();
    virtual ~QCarCamBusiness();

    /**
    * @brief        Gets the singleton instance for QCarCamBusiness
    * @return       a reference of QCarCamBusiness instance
    */
    static QCarCamBusiness &GetInstance();


    /**
    * @brief        open camera
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int openCamera();

    /**
    * @brief        close camera
    * @return       int error code refer to QCAR_TSW_TEST_CAMERA_ERROR_TYPE 
    */
    int closeCamera();

    QCarCamLogic* mpQCarCamLogic;

private:
};

#endif // camera_QCarCamBusiness_H_

