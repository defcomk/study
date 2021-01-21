//*************************************************************************************************
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//*************************************************************************************************

#ifndef __CHI_SESSION__
#define __CHI_SESSION__

#include "chipipeline.h"


    class ChiSession
    {

    public:

        static ChiSession* Create(ChiPipeline** ppPipelines, UINT32 numPipelines, ChiCallBacks* pCallbacks, void* pPrivateData);
        CDKResult          Initialize(ChiPipeline** ppPipelines, UINT32 numPipelines, ChiCallBacks* pCallbacks, void* pPrivateData);
        static CHIHANDLE   CreateSession(SessionCreateData* pSessionCreate);
        CHIHANDLE          GetSessionHandle() const;
        CDKResult          FlushSession() const;
        void               DestroySession() const;

    private:
        CHIHANDLE              m_hSession;  // created session handle
        SessionCreateData      m_sessionCreateData; // data required for session creation

        ChiSession();
        ~ChiSession();

        /// Do not allow the copy constructor or assignment operator
        ChiSession(const ChiSession& ) = delete;
        ChiSession& operator= (const ChiSession& ) = delete;
    };


#endif  //#ifndef __CHI_SESSION_
