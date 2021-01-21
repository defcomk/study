//******************************************************************************************************************************
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//******************************************************************************************************************************

#ifndef __REALTIME_PIPELINETEST__
#define __REALTIME_PIPELINETEST__


#include "camxtypes.h"
#include "chipipeline.h"
#include "chisession.h"
#include "chitestcase.h"
#include "ais_chi_i.h"

#ifndef OS_ANDROID
struct private_handle_t {
    int fd;
};
#endif


class RealtimePipelineTest : public ChiTestCase
{

public:
    enum TestId
    {
        TestIFERDI0RawOpaque,
        TestIFERDI0RawPlain16,
        TestIFERDI1RawPlain16,
        TestIFERDI2RawPlain16,
        TestIFERDI3RawPlain16
    };

    RealtimePipelineTest() = default;
    virtual ~RealtimePipelineTest();

    void TestRealtimePipeline(TestId testType, int numFrames, int cameraId);
    CamxResult PrepareRDIBuffers(AisBufferList* p_buffer_list);
    CamxResult ReleaseRDIBuffers(AisBufferList* p_buffer_list);
    void SubmitBuffer(int idx);

protected:

    CDKResult SetupStreams() override;
    CDKResult SetupPipelines(int cameraId, ChiSensorModeInfo* sensorMode) override;
    CDKResult CreateSessions() override;
    void      CommonProcessCaptureResult(ChiCaptureResult* pCaptureResult, SessionPrivateData* pSessionPrivateData) override;
    void      DestroyResources() override;
    CDKResult GenerateCaptureRequestFromBuffList(uint64_t frameNumber, int idx);
    CamxResult FindBufferIdxFromStreamBuff(CHISTREAMBUFFER* chiStreamBuffer, uint32_t* idx);


private:

    enum SessionId
    {
        RealtimeSession = 0,
        MaxSessions
    };

    TestId                m_testId;            // testId to differentiate between realtime tests
    ChiPipeline*          m_pChiPipeline;      // pipeline instance for realtime tests
    ChiSession*           m_pChiSession;       // session instance for realtime tests
    SessionPrivateData    m_perSessionPvtData; // test specific private data
    ChiPrivateData        m_requestPvtData;    // request private data
    CHIPIPELINEREQUEST      m_submitRequest;                    // pipeline request
    CHICAPTUREREQUEST       m_realtimeRequest;    // capture requests
    CHIPIPELINEMETADATAINFO m_pPipelineMetadataInfo;            // pipeline metadata information

    CHIMETAHANDLE        m_pInputMetaHandle;
    CHIMETAHANDLE        m_pOutputMetaHandle;

    AisBufferList*        m_buffer_list;       // buffer list
    uint64_t              m_frameNumber;       //framenumber request
    void*                 m_pmeta;

    struct private_handle_t * private_hndl[QCARCAM_MAX_NUM_BUFFERS];
};


#endif  //#ifndef __REALTIMEPIPELINETEST__
