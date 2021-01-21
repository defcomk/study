//*******************************************************************************************
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//*******************************************************************************************

#ifndef __CHI_TESTCASE__
#define __CHI_TESTCASE__

#include "chimodule.h"
#include <atomic>
#include <condition_variable>

//#define ASSERT_FALSE(a) a
//#define ASSERT_TRUE(a) a


static const int MAX_STREAMS = 10;

class ChiTestCase
{
public:

    // Perform base class functionality for processing the capture result
    static void ProcessCaptureResult(
        CHICAPTURERESULT* pResult,
        void*             pPrivateCallbackData);

    // Perform base class functionality for processing the capture result
    static void QueueCaptureResult(
        CHICAPTURERESULT* pResult,
        void*             pPrivateCallbackData);

    static void QueuePartialCaptureResult(
        CHIPARTIALCAPTURERESULT* pPartialResult,
        void*                    pPrivateCallbackData);

    // Perform any base class functionality for processing the message notification
    static void ProcessMessage(
        const CHIMESSAGEDESCRIPTOR* pMessageDescriptor,
        void*                       pPrivateCallbackData);


protected:
    /// Session private data
    struct SessionPrivateData
    {
        ChiTestCase* pTestInstance;    ///< Per usecase class
        uint32_t     sessionId;        ///< Session Id that is meaningful to the usecase in which the session belongs
        uint32_t     testId;
    };

    virtual void SetUp();
    virtual void TearDown();

    virtual void      CommonProcessCaptureResult(CHICAPTURERESULT* pResult,SessionPrivateData* pPrivateCallbackData);
    virtual CDKResult WaitForResults();
    // @brief functions to be implemented by derived class
    virtual CDKResult SetupStreams()   = 0;
    virtual CDKResult SetupPipelines(int cameraId, ChiSensorModeInfo* sensorMode) = 0;
    virtual CDKResult CreateSessions() = 0;
    virtual void      DestroyResources() = 0;

    void              DestroyCommonResources();
    void              AtomicIncrement(int count = 1);
    void              AtomicDecrement(int count = 1);
    uint32_t          GetBufferCount() const;
    static bool       IsRDIStream(CHISTREAMFORMAT format);

    // making constructor default since SetUp() will take care of initializing
    ChiTestCase()  = default;
    // making destructor default since TearDown() will take care of destroying
    ~ChiTestCase() = default;

    /* StreamCreateData structure and memory allocated for its variables */
    StreamCreateData         m_streamInfo;                    // stream info required for creating stream objects
    int                      m_numStreams;                    // total number of streams for given testcase
    CHISTREAMFORMAT          m_format[MAX_STREAMS];           // format for the streams
    CHISTREAMTYPE            m_direction[MAX_STREAMS];        // stream direction
    Size                     m_resolution[MAX_STREAMS];       // resolution for the streams
    StreamUsecases           m_streamId[MAX_STREAMS];         // unique identifier for the streams
    uint32_t                 m_usageFlag[MAX_STREAMS];        // gralloc usage flags for the streams
    bool                     m_isRealtime[MAX_STREAMS];       // bool to indicate if this stream is part of realtime session


    ChiStream*               m_pRequiredStreams;              // pointer to created stream objects

    ChiModule*               m_pChiModule;                    // pointer to the ChiModule singleton
    std::map<StreamUsecases, ChiStream*> m_streamIdMap;       // map storing unique streamId and stream object


private:
    std::atomic<uint32_t>    m_buffersRemaining;               // pending number of buffers
    std::mutex               m_bufferCountMutex;               // mutex associated with buffer remaining
    std::condition_variable  m_bufferCountCond;                // cond variable to signal/wait on buffer remaining

    // Do not allow the copy constructor or assignment operator
    ChiTestCase(const ChiTestCase& rUsecase) = delete;
    ChiTestCase& operator= (const ChiTestCase& rUsecase) = delete;
};


#endif  //#ifndef __CHI_TESTCASE__
