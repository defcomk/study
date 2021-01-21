#ifndef _AIS_RES_MGR_H_
#define _AIS_RES_MGR_H_

/*!
 * Copyright (c) 2016-2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include "ais_engine.h"

class AisUsrCtxt;

class AisResourceManager
{
protected:
    virtual ~AisResourceManager(){};

    static AisResourceManager* m_pResourceManagerInstance;

public:
    static AisResourceManager* GetInstance();
    static void DestroyInstance();

    virtual CameraResult Init(void) = 0;
    virtual CameraResult Deinit(void) = 0;

    virtual CameraResult Reserve(AisUsrCtxt* pUsrCtxt) = 0;
    virtual CameraResult Release(AisUsrCtxt* pUsrCtxt) = 0;
};

#endif /* _AIS_RES_MGR_H_ */
