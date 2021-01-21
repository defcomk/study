#ifndef _AIS_CONFIGURER_H_
#define _AIS_CONFIGURER_H_

/*!
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "ais_engine.h"

class AisUsrCtxt;

class AisEngineConfigurer
{
protected:
    virtual ~AisEngineConfigurer(){};
public:
    virtual CameraResult Init(void) = 0;
    virtual CameraResult Deinit(void) = 0;
    virtual CameraResult PowerSuspend(void) = 0;
    virtual CameraResult PowerResume(void) = 0;
    virtual CameraResult Config(AisUsrCtxt* pUsrCtxt) = 0;
    virtual CameraResult Start(AisUsrCtxt* pUsrCtxt) = 0;
    virtual CameraResult Stop(AisUsrCtxt* pUsrCtxt) = 0;
    virtual CameraResult Resume(AisUsrCtxt* pUsrCtxt) = 0;
    virtual CameraResult Pause(AisUsrCtxt* pUsrCtxt) = 0;
    virtual CameraResult SetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param) = 0;
    virtual CameraResult GetParam(AisUsrCtxt* pUsrCtxt, uint32 nCtrl, void *param) = 0;
};

#endif /* _AIS_CONFIGURER_H_ */
