//******************************************************************************
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//*******************************************************************************

#include "realtime_pipeline_test.h"
#include "ChiEngine.h"

#ifdef OS_ANDROID
#include <gralloc_priv.h>
#include "camera_metadata.h"
#endif

/***************************************************************************************************************************
*   RealtimePipelineTest::SetupStreams()
*
*   @brief
*       Overridden function implementation which defines the stream information based on the test Id
*   @param
*       None
*   @return
*       CDKResult success if stream objects could be created or failure
***************************************************************************************************************************/
CDKResult RealtimePipelineTest::SetupStreams()
{
    CDKResult result = CDKResultSuccess;
    int streamIndex = 0;

    AIS_LOG_CHI_LOG_ERR( "RealtimePipeline SetupStreams testcase Id provided: [%d]", m_testId);

    switch (m_testId)
    {
        case TestIFERDI0RawOpaque:
            m_numStreams = 1;

            //1. IFE RDI stream
            m_resolution[streamIndex] = RDI_MAX_NAPALI;
            m_direction[streamIndex]  = ChiStreamTypeOutput;
            m_format[streamIndex]     = ChiStreamFormatRawOpaque;
            m_streamId[streamIndex]   = IFEOutputPortRDI0;
            m_streamInfo.num_streams  = m_numStreams;
            break;

        case TestIFERDI0RawPlain16:
            m_numStreams = 1;

            //1. IFE RDI stream
            m_resolution[streamIndex] = RDI_MAX_NAPALI;
            m_direction[streamIndex]  = ChiStreamTypeOutput;
            m_format[streamIndex]     = ChiStreamFormatRawOpaque;//ChiStreamFormatRaw16;
            m_streamId[streamIndex]   = IFEOutputPortRDI0;
            m_streamInfo.num_streams  = m_numStreams;
            break;
        case TestIFERDI1RawPlain16:
            m_numStreams = 1;

            //1. IFE RDI stream
            m_resolution[streamIndex] = RDI_MAX_NAPALI;
            m_direction[streamIndex]  = ChiStreamTypeOutput;
            m_format[streamIndex]     = ChiStreamFormatRawOpaque;//ChiStreamFormatRaw16;
            m_streamId[streamIndex]   = IFEOutputPortRDI1;
            m_streamInfo.num_streams  = m_numStreams;
            break;
        case TestIFERDI2RawPlain16:
            m_numStreams = 1;

            //1. IFE RDI stream
            m_resolution[streamIndex] = RDI_MAX_NAPALI;
            m_direction[streamIndex]  = ChiStreamTypeOutput;
            m_format[streamIndex]     = ChiStreamFormatRawOpaque;//ChiStreamFormatRaw16;
            m_streamId[streamIndex]   = IFEOutputPortRDI2;
            m_streamInfo.num_streams  = m_numStreams;
            break;
        case TestIFERDI3RawPlain16:
            m_numStreams = 1;

            //1. IFE RDI stream
            m_resolution[streamIndex] = RDI_MAX_NAPALI;
            m_direction[streamIndex]  = ChiStreamTypeOutput;
            m_format[streamIndex]     = ChiStreamFormatRawOpaque ;//ChiStreamFormatRaw16;
            m_streamId[streamIndex]   = IFEOutputPortRDI3;
            m_streamInfo.num_streams  = m_numStreams;
            break;
        default:
            AIS_LOG_CHI_LOG_ERR( "Unknown testcase Id provided: [%d]", m_testId);
            result = CDKResultENoSuch;
            break;
    }

    if (result == CDKResultSuccess)
    {
        m_pRequiredStreams = reinterpret_cast<ChiStream*>(CreateStreams(m_streamInfo));

        if (m_pRequiredStreams == nullptr)
        {
            AIS_LOG_CHI_LOG_ERR( "Provided streams could not be configured for testcase Id: [%d]", m_testId);
            result = CDKResultEFailed;
        }
        else
        {
            for (int index = 0; index < m_numStreams; index++)
            {
                m_isRealtime[index] = true;
                m_streamIdMap[m_streamInfo.streamIds[index]] = &m_pRequiredStreams[index];
            }
            if (m_streamIdMap.size() != static_cast<size_t>(m_numStreams))
            {
                result = CDKResultEFailed;
            }
        }
    }

    return result;
}

/***************************************************************************************************************************
*   RealtimePipelineTest::SetupPipelines()
*
*   @brief
*       Overridden function implementation which creates pipeline objects based on the test Id
*   @param
*       [in]  int                 cameraId    cameraId to be associated with the pipeline
*       [in]  ChiSensorModeInfo*  sensorMode  sensor mode to be used for the pipeline
*   @return
*       CDKResult success if pipelines could be created or failure
***************************************************************************************************************************/
CDKResult RealtimePipelineTest::SetupPipelines(int cameraId, ChiSensorModeInfo * sensorMode)
{
    CDKResult result = CDKResultSuccess;
    switch (m_testId)
    {

        case TestId::TestIFERDI0RawOpaque:
        case TestId::TestIFERDI0RawPlain16:
            m_pChiPipeline = ChiPipeline::Create(cameraId, sensorMode, m_streamIdMap, PipelineType::RealtimeIFERDI0);
            break;
        case TestId::TestIFERDI1RawPlain16:
            m_pChiPipeline = ChiPipeline::Create(cameraId, sensorMode, m_streamIdMap, PipelineType::RealtimeIFERDI1);
            break;
        case TestId::TestIFERDI2RawPlain16:
            m_pChiPipeline = ChiPipeline::Create(cameraId, sensorMode, m_streamIdMap, PipelineType::RealtimeIFERDI2);
            break;
        case TestId::TestIFERDI3RawPlain16:
            m_pChiPipeline = ChiPipeline::Create(cameraId, sensorMode, m_streamIdMap, PipelineType::RealtimeIFERDI3);
            break;
        default:
            AIS_LOG_CHI_LOG_ERR( "Unknown testcase Id provided: [%d]", m_testId);
            return CDKResultENoSuch;
    }

    result = (m_pChiPipeline != nullptr) ? m_pChiPipeline->CreatePipelineDesc() : CDKResultEInvalidPointer;
    return result;
}

/***************************************************************************************************************************
*   RealtimePipelineTest::CreateSessions()
*
*   @brief
*       Overridden function implementation which creates required number of sessions based on the test Id
*   @param
*       None
*   @return
*       CDKResult success if sessions could be created or failure
***************************************************************************************************************************/
CDKResult RealtimePipelineTest::CreateSessions()
{
    CDKResult result = CDKResultSuccess;
    ChiCallBacks callbacks = { 0 };

    callbacks.ChiNotify               = ChiTestCase::ProcessMessage;
    callbacks.ChiProcessCaptureResult = ChiTestCase::ProcessCaptureResult;
    callbacks.ChiProcessPartialCaptureResult = ChiTestCase::QueuePartialCaptureResult;

    m_perSessionPvtData.pTestInstance = this;
    m_perSessionPvtData.sessionId = RealtimeSession;
    m_perSessionPvtData.testId = m_testId;

    m_pChiSession = ChiSession::Create(&m_pChiPipeline,
                                       1,
                                       &callbacks,
                                       &m_perSessionPvtData);

    if(m_pChiSession == nullptr)
    {
        AIS_LOG_CHI_LOG_ERR( "Realtime session could not be created");
        result = CDKResultEFailed;
    }
    else
    {
        result |= m_pChiPipeline->ActivatePipeline(m_pChiSession->GetSessionHandle());

#ifndef OLD_METADATA
        if (result == CDKResultSuccess)
        {
            result = m_pChiModule->GetChiOps()->pQueryPipelineMetadataInfo(m_pChiModule->GetContext(),
                m_pChiSession->GetSessionHandle(), m_pChiPipeline->GetPipelineHandle(), &m_pPipelineMetadataInfo);
        }

        CHIMETADATAOPS MetadaOps;
        m_pChiModule->GetChiOps()->pMetadataOps(&MetadaOps);
        MetadaOps.pCreate(&m_pInputMetaHandle, NULL);
        MetadaOps.pCreate(&m_pOutputMetaHandle, NULL);
#endif
    }
    return result;
}

CamxResult RealtimePipelineTest::FindBufferIdxFromStreamBuff(
    CHISTREAMBUFFER* chiStreamBuffer,
    uint32_t* idx)
{
    CamxResult rc = CamxResultENoSuch;

    for (uint32_t i = 0; i < m_buffer_list->m_nBuffers; i++)
    {
        if (reinterpret_cast<CHIBUFFERHANDLE> (m_buffer_list->m_pBuffers[i].p_da) == chiStreamBuffer->bufferInfo.phBuffer)
        {
            AIS_LOG_CHI_LOG_WARN( "found buffer idx %d for stream %p", i, chiStreamBuffer);
            *idx = i;
            rc = CamxResultSuccess;
        }
    }

    return rc;
}


void RealtimePipelineTest::CommonProcessCaptureResult(
    ChiCaptureResult * pCaptureResult,
    SessionPrivateData * pSessionPrivateData)
{

    CDKResult result = CDKResultSuccess;
    uint32_t  resultFrameNum = pCaptureResult->frameworkFrameNum;
    uint32_t  numOutputBuffers = pCaptureResult->numOutputBuffers;
    SessionId sessionId = static_cast<SessionId>(pSessionPrivateData->sessionId);
    TestId    testId = static_cast<TestId>(pSessionPrivateData->testId);

    AIS_LOG_CHI_LOG_DBG( "TEHO CommonProcessCaptureResult session ID %d, pTestInstance %p", pSessionPrivateData->sessionId, pSessionPrivateData->pTestInstance);

    AIS_LOG_CHI_LOG_ERR( "TEHO Recevied chistreambuffer: numbufs %d in frame: %d testId %d",
            numOutputBuffers, resultFrameNum, testId);

    if (sessionId == RealtimeSession)
    {
        for (uint32_t bufferIndex = 0; bufferIndex < numOutputBuffers; bufferIndex++)
        {
            CamxResult rc;
            uint32_t idx;
            CHISTREAMBUFFER* outBuffer = const_cast<CHISTREAMBUFFER*>(&pCaptureResult->pOutputBuffers[bufferIndex]);

            AIS_LOG_CHI_LOG_ERR( "TEHO Recevied chistreambuffer: %p in frame: %d",
                    outBuffer, resultFrameNum);

            rc = FindBufferIdxFromStreamBuff(outBuffer, &idx);

            if (rc == CamxResultSuccess)
            {
                ais_engine_event_msg_t msg;

                STD_ZEROAT(&msg);
                msg.event_id = AIS_EVENT_FRAME_DONE;

                msg.payload.ife_frame_done.pipeline_id = testId;
                msg.payload.ife_frame_done.frame_info.idx = idx;
                msg.payload.ife_frame_done.frame_info.seq_no = resultFrameNum;
                msg.payload.ife_frame_done.frame_info.timestamp = 0;  //TODO - this comes from the SOF need to keep array of timestamp for framenum
#if 0
                if( ais_diag_isenable(AIS_DIAG_LATENCY) )
                {
    	            int64 tm = 0;
    	            CameraGetTime(&tm);
    	            msg.payload.ife_frame_done.us_starttime = tm;
                }
#endif

                CAM_MSG(MEDIUM, "IFE pipeline_id %d Frame Done buffer %d",
                        msg.payload.ife_frame_done.pipeline_id, msg.payload.ife_frame_done.frame_info.idx);

                //queue the buffer to the engine
                ChiEngine::GetInstance()->QueueEvent(&msg);
            }
            else
            {
                AIS_LOG_CHI_LOG_ERR( "TEHO  Chistream %p framenumber %d has no matching index",
                        outBuffer, resultFrameNum);
            }
        }
    }
    else
    {
        AIS_LOG_CHI_LOG_ERR( "Unknown Session Id: %d obtained", sessionId);
        result = CDKResultEInvalidState;
    }

    if (result == CDKResultSuccess)
    {
        AtomicDecrement(numOutputBuffers);
        AIS_LOG_CHI_LOG_ERR( "Number of buffers remamining after recieving the result: %d", GetBufferCount());
    }
}



/***************************************************************************************************************************
*   RealtimePipelineTest::DestroyResources()
*
*   @brief
*       Overridden function implementation which destroys all created resources / objects
*   @param
*       None
*   @return
*       None
    ***********************************************************************************************************************/
void RealtimePipelineTest::DestroyResources()
{
    m_pChiPipeline->DeactivatePipeline(m_pChiSession->GetSessionHandle());

    if (m_pChiSession != nullptr)
    {
        m_pChiSession->DestroySession();
        m_pChiSession = nullptr;
    }

    if (m_pChiPipeline != nullptr)
    {
        m_pChiPipeline->DestroyPipeline();
        m_pChiPipeline = nullptr;
    }

    DestroyCommonResources();
}

CDKResult RealtimePipelineTest::GenerateCaptureRequestFromBuffList(uint64_t frameNumber, int idx)
{
    CDKResult             result = CDKResultSuccess;
    CHISTREAMBUFFER*      pOutputBuffers;
    static const uint32_t NumOutputBuffers = m_numStreams;

    pOutputBuffers = static_cast<CHISTREAMBUFFER*>(m_buffer_list->m_pBuffers[idx].chi_stream_buffer);
    struct private_handle_t * private_hndl = *reinterpret_cast <struct private_handle_t **>(pOutputBuffers->bufferInfo.phBuffer);

    AIS_LOG_CHI_LOG_ERR( "GenerateCaptureRequeset %i %p %p fd %i",
        idx, m_buffer_list->m_pBuffers[idx].chi_stream_buffer,  pOutputBuffers->bufferInfo.phBuffer, private_hndl->fd);


    CHICAPTUREREQUEST realTimeRequest = { 0 };
    realTimeRequest.frameNumber       = frameNumber;
    realTimeRequest.hPipelineHandle   = m_pChiPipeline->GetPipelineHandle();
    realTimeRequest.numOutputs        = NumOutputBuffers;
    realTimeRequest.pOutputBuffers    = pOutputBuffers; //outputBuffers;
    realTimeRequest.pPrivData         = &m_requestPvtData;

#ifdef OLD_METADATA
#if defined(ANDROID)
    //Need to compile this for android
    realTimeRequest.pMetadata         = allocate_camera_metadata(METADATA_ENTRIES, 100 * 1024);
#else
    realTimeRequest.pMetadata         = malloc(200*100*1024); //how big does this need to be allocate_camera_metadata(METADATA_ENTRIES, 100 * 1024);
#endif
#else /* OLD_METADATA */
    realTimeRequest.pInputMetadata = m_pInputMetaHandle;
    realTimeRequest.pOutputMetadata = m_pOutputMetaHandle;
#endif /* OLD_METADATA*/

    CHIPIPELINEREQUEST submitRequest = { 0 };
    submitRequest.pSessionHandle   = m_pChiSession->GetSessionHandle();
    submitRequest.numRequests      = 1;
    submitRequest.pCaptureRequests = &realTimeRequest;

    AtomicIncrement(NumOutputBuffers);

    AIS_LOG_CHI_LOG_ERR( "Number of buffers remamining: %d after sending request: %llu",
        GetBufferCount(), frameNumber);

    result = m_pChiModule->SubmitPipelineRequest(&submitRequest);

    //delete[] outputBuffers;
    return result;
}


RealtimePipelineTest::~RealtimePipelineTest()
{
}

/**************************************************************************************************
*   RealtimePipelineTest::TestRealtimePipeline
*
*   @brief
*       Test a realtime pipeline involving Sensor+IFE or Sensor+IFE+IPE depending on pipelineType
*   @param
*       [in] TestType             testType       Test identifier
*       [in] int                  numFrames      number of frames to capture
*   @return
*       None
**************************************************************************************************/
void RealtimePipelineTest::TestRealtimePipeline(TestId testType, int numFrames, int cameraId)
{

    SetUp();

    int numOfCameras = m_pChiModule->GetNumberCameras();

    if (cameraId >= numOfCameras)
    {
        AIS_LOG_CHI_LOG_ERR( "Invalid cameraId: %d", cameraId);
    }
    else
    {

        ChiSensorModeInfo* testSensorMode = nullptr;

#if NAPALI
        if (cameraId == 0)
        {
            testSensorMode = m_pChiModule->GetCameraSensorModeInfo(cameraId, 10);
        }
        else
#endif
        {
            testSensorMode = m_pChiModule->GetCameraSensorModeInfo(cameraId, 0);
        }

        if (testSensorMode == nullptr)
        {
            AIS_LOG_CHI_LOG_ERR( "Sensor mode is NULL for cameraId: %d", cameraId);
        }

        AIS_LOG_CHI_LOG_WARN( "Chosen Sensor mode with width: %d, height: %d ",
            testSensorMode->frameDimension.width, testSensorMode->frameDimension.height);

        //Set number of batched frames in sensormode
        testSensorMode->batchedFrames = 1;

        m_testId = testType;

        //1. Create chistream objects from streamcreatedata
        if (SetupStreams() != CDKResultSuccess)
        {
            AIS_LOG_CHI_LOG_ERR( "Could not setup required streams");
        }

        //For RDI cases, we need to make sure stream resolution matches sensor mode

        for (int streamIndex = 0; streamIndex < m_numStreams; streamIndex++)
        {
            if (IsRDIStream(m_pRequiredStreams[streamIndex].format))
            {
                AIS_LOG_CHI_LOG_ERR( "RDI stream found, match resolution to sensormode resolution width: %d, height: %d ",
                    testSensorMode->frameDimension.width, testSensorMode->frameDimension.height);

                m_pRequiredStreams[streamIndex].width  = testSensorMode->frameDimension.width;
                m_pRequiredStreams[streamIndex].height = testSensorMode->frameDimension.height;
            }
        }

        //2. Create pipelines
        if (SetupPipelines(cameraId, testSensorMode) != CDKResultSuccess)
        {
            AIS_LOG_CHI_LOG_ERR( "Pipelines setup failed");
        }
        //3. Create sessions
        if (CreateSessions() != CDKResultSuccess)
        {
            AIS_LOG_CHI_LOG_ERR( "Necessary sessions could not be created");
        }
    }
}

CamxResult RealtimePipelineTest::PrepareRDIBuffers(AisBufferList* p_buffer_list)
{
    CamxResult result = CamxResultSuccess;
    CHISTREAM* currentStream       = &m_pRequiredStreams[0];  //Only one stream for RDI
    CHISTREAMBUFFER* pStreamBuffer = NULL;

    m_buffer_list = p_buffer_list;

#if defined(ANDROID)
    m_pmeta = allocate_camera_metadata(METADATA_ENTRIES, 100 * 1024);
#else
    m_pmeta = malloc(100*1024);
#endif

    for (uint32 i = 0; i < p_buffer_list->m_nBuffers; i++)
    {
        pStreamBuffer = SAFE_NEW() CHISTREAMBUFFER();
        p_buffer_list->m_pBuffers[i].chi_stream_buffer = pStreamBuffer;

        private_hndl[i] = reinterpret_cast <struct private_handle_t *>(malloc(sizeof(struct private_handle_t)));

#ifdef OS_ANDROID
        private_hndl[i]->fd = static_cast<int>(reinterpret_cast <uintptr_t>(p_buffer_list->m_pBuffers[i].p_va));
#endif
        AIS_LOG_CHI_LOG_ERR( "There are pending buffers not returned by driver after several retries");

        pStreamBuffer->size = sizeof(*pStreamBuffer);
        pStreamBuffer->pStream = currentStream;

#ifdef OS_ANDROID
        pStreamBuffer->bufferInfo.phBuffer = reinterpret_cast<native_handle_t**>(&private_hndl[i]);
        pStreamBuffer->bufferInfo.bufferType = CHIBUFFERTYPE::ChiGralloc;

        //set the p_da to the phBuffer value for mapping
        p_buffer_list->m_pBuffers[i].p_da = reinterpret_cast <SMMU_ADDR_T>(pStreamBuffer->bufferInfo.phBuffer);
#endif

        pStreamBuffer->acquireFence.valid = FALSE; //no waiting for now
        pStreamBuffer->releaseFence.valid = FALSE;


        AIS_LOG_CHI_LOG_ERR( "RDI map buffer %i fd %d %p streambuffer %p",i, private_hndl[i]->fd, pStreamBuffer->bufferInfo.phBuffer, pStreamBuffer);

    }

    return result;
}


CamxResult RealtimePipelineTest::ReleaseRDIBuffers(AisBufferList* p_buffer_list)
{
    CamxResult result = CamxResultSuccess;

    free(m_pmeta);

    for (uint32 i = 0; i < p_buffer_list->m_nBuffers; i++)
    {
        if (p_buffer_list->m_pBuffers[i].chi_stream_buffer)
        {
            free(p_buffer_list->m_pBuffers[i].chi_stream_buffer->bufferInfo.phBuffer);
            delete p_buffer_list->m_pBuffers[i].chi_stream_buffer;
        }
    }

    return result;
}


void RealtimePipelineTest::SubmitBuffer(int idx)
{
    if (GenerateCaptureRequestFromBuffList(m_frameNumber, idx) != CDKResultSuccess)
    {
        AIS_LOG_CHI_LOG_ERR( "Submit pipeline request failed for frameNumber: %lld", m_frameNumber);
    }
    else
    {
        m_frameNumber++;
    }
#if 0 /*Do not need to wait*/
    //6. Wait for all results
    if (WaitForResults() != CDKResultSuccess)
    {
        AIS_LOG_CHI_LOG_ERR( "There are pending buffers not returned by driver after several retries");
    }
#endif
}


