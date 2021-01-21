/**********************************************************************//*!
* Copyright (c) Continental AG and subsidiaries
*
* \file     IQCarCamInterface.h
*
* \brief    Camera Interface
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

#ifndef camera_IQCarCamInterface_H_
#define camera_IQCarCamInterface_H_


#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief        init light hal 
* @return       int error code refer to <errno.h> 
*/
int openCamera(void);

/**
* @brief        deinit light hal 
* @return       int error code refer to <errno.h> 
*/
int closeCamera(void);

#ifdef __cplusplus
};
#endif

#ifdef __cplusplus


/**
 * This is a light hal interface class
 */
class IQCarCamInterface {
public:
    virtual ~IQCarCamInterface(){};

    /**
    * @brief        init light hal 
    * @return       int error code refer to <errno.h> 
    */
    virtual int openCamera();

    /**
    * @brief        deinit light hal 
    * @return       int error code refer to <errno.h> 
    */
    virtual int closeCamera();
    
private:

};


#endif /* __cplusplus */

#endif //camera_IQCarCamInterface_H_
