//******************************************************************************************************************************
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//******************************************************************************************************************************
//==============================================================================================================================
// IMPLEMENTATION of chitestcase
//==============================================================================================================================

#include "chitestcase.h"

void ChiTestCase::SetUp()
{
    m_pChiModule = ChiModule::GetInstance();

    if (m_pChiModule == nullptr)
    {
        AIS_LOG_CHI_LOG_ERR("ChiModule could not be instantiated");
    }

    m_streamInfo.streamIds   = m_streamId;
    //TODO: Let common framework do the typecasting
    m_streamInfo.directions  = m_direction;
    m_streamInfo.formats     = m_format;
    m_streamInfo.resolutions = m_resolution;
    m_streamInfo.usageFlags  = m_usageFlag;
    m_streamInfo.isRealtime  = m_isRealtime;
    m_streamInfo.isJpeg      = true;
    m_streamInfo.filename    = nullptr;

    m_numStreams = 0;
    m_buffersRemaining = 0;
    m_streamIdMap.clear();
}

void ChiTestCase::TearDown()
{
    DestroyCommonResources();
    ChiModule::DestroyInstance();
    m_pChiModule = nullptr;
}

/***************************************************************************************************************************
*   ChiTestCase::CommonProcessCaptureResult
*
*   @brief
*       Common Process capture result redirected from the static method, this is base class implementation which can be used
*       for common operations done by all testcases
*   @param
*        [in]  CHICAPTURERESULT*     pResult                pointer to capture result
*        [in]  void*                 pPrivateCallbackData   pointer private callback data
*   @return
*       none
***************************************************************************************************************************/
void ChiTestCase::CommonProcessCaptureResult(CHICAPTURERESULT* pResult, SessionPrivateData* pPrivateCallbackData)
{
    NATIVETEST_UNUSED_PARAM(pResult);
    NATIVETEST_UNUSED_PARAM(pPrivateCallbackData);
}

/***************************************************************************************************************************
*   ChiTestCase::ProcessCaptureResult
*
*   @brief
*       Process capture result returned from driver, dispatch it to the correct instance of the testcase
*   @param
*        [in]  CHICAPTURERESULT*     pResult                pointer to capture result
*        [in]  void*                 pPrivateCallbackData   pointer private callback data
*   @return
*       none
***************************************************************************************************************************/
void ChiTestCase::ProcessCaptureResult(CHICAPTURERESULT* pResult, void* pPrivateCallbackData)
{
    SessionPrivateData* pSessionPrivateData = static_cast<SessionPrivateData*>(pPrivateCallbackData);
    pSessionPrivateData->pTestInstance->CommonProcessCaptureResult(pResult, pSessionPrivateData);
}

/***************************************************************************************************************************
*   ChiTestCase::QueuePartialCaptureResult
*
*   @brief
*       Copy and queue up capture results returned from driver for later processing in the main thread
*   @param
*        [in]  CHIPARTIALCAPTURERESULT*     pPartialResult          pointer to capture result
*        [in]  void*                        pPrivateCallbackData    pointer private callback data
*   @return
*       None
***************************************************************************************************************************/
void ChiTestCase::QueuePartialCaptureResult(CHIPARTIALCAPTURERESULT* pPartialResult, void* pPrivateCallbackData)
{
    NATIVETEST_UNUSED_PARAM(pPartialResult);
    NATIVETEST_UNUSED_PARAM(pPrivateCallbackData);
    // TODO TDEV-2292
}

/***************************************************************************************************************************
*   ChiTestCase::ProcessMessage
*
*   @brief
*       Process Notify message from driver
*   @param
*     [in]  CHIMESSAGEDESCRIPTOR*     pMessageDescriptor       pointer to message descriptor
*     [in]  void*                     pPrivateCallbackData     pointer private callback data
*   @return
*       none
***************************************************************************************************************************/
void ChiTestCase::ProcessMessage(
    const CHIMESSAGEDESCRIPTOR* pMessageDescriptor,
    void*                       pPrivateCallbackData)
{
    //NATIVETEST_UNUSED_PARAM(pPrivateCallbackData);

    SessionPrivateData* pSessionPrivateData = static_cast<SessionPrivateData*>(pPrivateCallbackData);

    AIS_LOG_CHI_LOG_DBG( "Received notify from driver test ID %d, pTestInstance %p", pSessionPrivateData->testId, pSessionPrivateData->pTestInstance);
    switch (pMessageDescriptor->messageType)
    {
        case CHIMESSAGETYPE::ChiMessageTypeError:
        {
            CHIERRORMESSAGE errMessage = pMessageDescriptor->message.errorMessage;
            AIS_LOG_CHI_LOG_ERR( "Error Msg when processing frame %d, stream %d", errMessage.frameworkFrameNum,
                errMessage.pErrorStream);
            switch (errMessage.errorMessageCode)
            {
                case CHIERRORMESSAGECODE::MessageCodeDevice:
                    AIS_LOG_CHI_LOG_ERR("A serious failure occurred and no further frames will be produced"
                        " by the device");
                    //TODO:Stop sending requests for this type of error
                    break;
                case CHIERRORMESSAGECODE::MessageCodeRequest:
                    AIS_LOG_CHI_LOG_ERR("An error has occurred in processing a request and no output will be "
                        "produced for this request");
                    break;
                case CHIERRORMESSAGECODE::MessageCodeResult:
                    AIS_LOG_CHI_LOG_ERR("An error has occurred in producing an output result metadata buffer"
                        " for a request");
                    break;
                case CHIERRORMESSAGECODE::MessageCodeBuffer:
                    AIS_LOG_CHI_LOG_ERR("An error has occurred in placing an output buffer into a stream for"
                        " a request");
                    break;
                default:
                    AIS_LOG_CHI_LOG_ERR("Unknown error message code %d", errMessage.errorMessageCode);
                    break;
            }

            break;
        }
    case CHIMESSAGETYPE::ChiMessageTypeShutter:
    {
        CHISHUTTERMESSAGE shutterMessage = pMessageDescriptor->message.shutterMessage;
        AIS_LOG_CHI_LOG_DBG("Shutter message frameNumber: %d timestamp: %llu",
            shutterMessage.frameworkFrameNum, shutterMessage.timestamp);
        break;
    }

    case CHIMESSAGETYPE::ChiMessageTypeSof:
        AIS_LOG_CHI_LOG_HIGH("Start of frame event triggered for testId = %d", pSessionPrivateData->testId);
        break;

    default:
        AIS_LOG_CHI_LOG_ERR("Unknown message type %d", pMessageDescriptor->messageType);
        break;
    }
}

/***************************************************************************************************************************
*   ChiTestCase::WaitForResults
*
*   @brief
*       Common waiting condition for all testcases, Once the test sends required number of requests this waiting condition
*       is invoked to wait for all results to be returned back by driver
*   @param
*        None
*   @return
*       CDKResult success if all results returned by driver/ fail if buffers were not returned within buffer timeout
***************************************************************************************************************************/
CDKResult ChiTestCase::WaitForResults()
{
    std::unique_lock<std::mutex> lk(m_bufferCountMutex);
    CDKResult result = CDKResultSuccess;
    int retries = 0;
    while(m_buffersRemaining.load() != 0 && retries < TIMEOUT_RETRIES)
    {
        AIS_LOG_CHI_LOG_DBG("Waiting for buffers to be returned, pending buffer count: %d", m_buffersRemaining.load());
        m_bufferCountCond.wait_for(lk, std::chrono::seconds(buftimeOut));
        retries++;
    }
    lk.unlock();

    if(m_buffersRemaining.load() != 0)
    {
        AIS_LOG_CHI_LOG_ERR("Buffer wait condition timed out");
        result = CDKResultETimeout;
    }

    return result;
}

/**************************************************************************************************
*   ChiTestCase::IsRDIStream
*
*   @brief
*       Helper function to determine if stream is RDI
*   @param
*       [in] CHISTREAMFORMAT    format    Stream format
*   @return
*       bool indicating whether the stream is RDI
**************************************************************************************************/
bool ChiTestCase::IsRDIStream(CHISTREAMFORMAT format)
{
    return (format == ChiStreamFormatRaw10     ||
            format == ChiStreamFormatRawOpaque ||
            format == ChiStreamFormatRaw16);
}

/***************************************************************************************************************************
*   ChiTestCase::DestroyCommonResources
*
*   @brief
*       Destroy member variables present in the base class, i.e., common to all testcases
*   @param
*        None
*   @return
*       None
***************************************************************************************************************************/
void ChiTestCase::DestroyCommonResources()
{
    if (m_pRequiredStreams != nullptr)
    {
        delete[] m_pRequiredStreams;
        m_pRequiredStreams = nullptr;
    }

    m_streamIdMap.clear();
    m_numStreams = 0;
    m_buffersRemaining = 0;
}

/***************************************************************************************************************************
*   ChiTestCase::AtomicIncrement
*
*   @brief
*       Thread safe increment of buffers remaining
*   @param
*        [in]   int count   increment by count, this is an optional variable by default it will be incremented by 1
*   @return
*       None
***************************************************************************************************************************/
void ChiTestCase::AtomicIncrement(int count)
{
    m_buffersRemaining += count;
}

/***************************************************************************************************************************
*   ChiTestCase::AtomicDecrement
*
*   @brief
*       Thread safe decerement of buffers remaining, if buffers remaining reached zero then it will signal all results
*       obtained
*   @param
*        [in]   int count   decrement by count, this is an optional variable by default it will be decremented by 1
*   @return
*       None
***************************************************************************************************************************/
void ChiTestCase::AtomicDecrement(int count)
{
    m_buffersRemaining -= count;
    if(m_buffersRemaining == 0)
    {
        m_bufferCountCond.notify_one();
    }
}

/***************************************************************************************************************************
*   ChiTestCase::GetBufferCount
*
*   @brief
*       gets the buffer remaining count in a thread safe manner
*   @param
*        None
*   @return
*       uint32_t buffer remaining count
***************************************************************************************************************************/
uint32_t ChiTestCase::GetBufferCount() const
{
    return m_buffersRemaining.load();
}

