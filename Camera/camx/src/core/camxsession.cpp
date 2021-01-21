////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxsession.cpp
/// @brief Definitions for Session class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// @todo (CAMX-2499): Will move this to the OsUtil library later
#define USE_COLOR_METADATA


#if (!defined(LE_CAMERA)) // ANDROID
#include "qdMetaData.h"
#endif // ANDROID
#include "chi.h"
#include "camxincs.h"
#include "camxmem.h"
#include "camxmemspy.h"
#include "camxthreadmanager.h"
#include "camxdeferredrequestqueue.h"
#include "camxhal3defaultrequest.h"
#include "camxhal3stream.h"
#include "camxhal3metadatautil.h"
#include "camxhal3queue.h"
#include "camxhwcontext.h"
#include "camxchi.h"
#include "camxpipeline.h"
#include "camxnode.h"
#include "camxsession.h"
#include "camxvendortags.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constant Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const UINT32 MaxContentLightLevel                = 1000;         ///< Maximum content light level
static const UINT32 MaxFrameAverageLightLevel           = 200;          ///< Maximum frame average light level
static const UINT32 MaxDisplayLuminance                 = 1000;         ///< Maximum Luminance in cd/m^2
static const UINT32 MinDisplayLuminance                 = 50;           ///< Minimum Luminance in 1/10000 cd/m^2
static const UINT32 PrimariesRGB[3][2]                  = {{34000, 16000}, {13250, 34500}, {7500, 3000}};
static const UINT32 PrimariesWhitePoint[2]              = {15635, 16450};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::~Session
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Session::~Session()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::WaitTillAllResultsAvailable
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::WaitTillAllResultsAvailable()
{
    CamxResult result   = CamxResultSuccess;
    BOOL       timedOut = FALSE;

    if (FALSE == IsDoneProcessing())
    {
        m_pResultHolderListLock->Lock();
        if (0 != m_resultHolderList.NumNodes())
        {
            SessionResultHolder* pSessionResultHolderHead =
                reinterpret_cast<SessionResultHolder*>(m_resultHolderList.Head()->pData);
            SessionResultHolder* pSessionResultHolderTail =
                reinterpret_cast<SessionResultHolder*>(m_resultHolderList.Tail()->pData);

            CAMX_LOG_INFO(CamxLogGroupCore, "Waiting for all results minRequestIdPending: %u  maxRequestIdPending: %u "
                "minSequenceIdPending: %u  maxSequenceIdPending: %u",
                pSessionResultHolderHead->resultHolders[0].requestId,
                pSessionResultHolderTail->resultHolders[pSessionResultHolderTail->numResults - 1].requestId,
                pSessionResultHolderHead->resultHolders[0].sequenceId,
                pSessionResultHolderTail->resultHolders[pSessionResultHolderTail->numResults - 1].sequenceId);
        }
        m_pResultHolderListLock->Unlock();

        UINT waitTime = static_cast<UINT>(GetFlushResponseTime());

        CAMX_LOG_INFO(CamxLogGroupCore, "Session is not done processing, Waiting for %u", waitTime);

        m_pResultLock->Lock();

        if (FALSE == m_pWaitAllResultsAvailableSignaled)
        {
            result = m_pWaitAllResultsAvailable->TimedWait(m_pResultLock->GetNativeHandle(), waitTime);
        }
        m_pWaitAllResultsAvailableSignaled = FALSE;
        m_pResultLock->Unlock();

        if (CamxResultSuccess != result)
        {
            timedOut = TRUE;
            CAMX_LOG_ERROR(CamxLogGroupCore,
                           "TimedWait for results timed out at %u ms with error %s! Pending results: ",
                           waitTime, Utils::CamxResultToString(result));

            DumpSessionState(SessionDumpFlag::Flush);
        }
        else
        {
            CAMX_LOG_INFO(CamxLogGroupCore, "TimedWait returned with success");
        }
    }

    return (FALSE == timedOut);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::Flush
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::Flush()
{
    CamxResult result = CamxResultSuccess;
    UINT64     lastSubmittedRequestId = 0;
    UINT       maxRequestIdPending    = 0;

    if (FALSE == m_sesssionInitComplete)
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Flush is called before session %p is initialized", this);
        result = CamxResultEInvalidState;
    }
    else
    {
        m_pResultHolderListLock->Lock();
        if (0 != m_resultHolderList.NumNodes())
        {
            SessionResultHolder* pSessionResultHolderTail =
                reinterpret_cast<SessionResultHolder*>(m_resultHolderList.Tail()->pData);

            maxRequestIdPending = pSessionResultHolderTail->resultHolders[pSessionResultHolderTail->numResults - 1].requestId;
        }
        m_pResultHolderListLock->Unlock();

        UINT numPCR = m_pChiContext->GetStaticSettings()->numPCRsBeforeStreamOn;

        if ((numPCR == 0) && ((maxRequestIdPending > 0) && maxRequestIdPending <= 2))
        {
            // no time out return success
            if (TRUE == WaitTillAllResultsAvailable())
            {
                CAMX_LOG_INFO(CamxLogGroupCore, "Flush enableWaitBeforeFlushForAllResults");
                return CamxResultSuccess;
            }
        }

        UINT32      flushStartTime;
        UINT32      flushEndTime;
        CamxTime    pTime;

        OsUtils::GetTime(&pTime);
        flushStartTime = OsUtils::CamxTimeToMillis(&pTime);

        m_pFlushLock->Lock();
        CamxAtomicStoreU8(&m_aFlushStatus, TRUE);

        CAMX_LOG_VERBOSE(CamxLogGroupCore, "Flush called from session %p with pipelines %s", this, m_pipelineNames);

        // print nodes still processing, save last valid requestId
        for (UINT32 index = 0; index < m_numPipelines; index++)
        {
            Pipeline* pPipeline = m_pipelineData[index].pPipeline;
            if (PipelineStatus::STREAM_ON == pPipeline->GetPipelineStatus())
            {
                pPipeline->SaveLastValidRequestId();
                pPipeline->LogPendingNodes();
                lastSubmittedRequestId = Utils::MaxUINT64(pPipeline->GetLastSubmittedRequestId(), lastSubmittedRequestId);
            }
        }

        // Block submissions to the CSL.
        m_pChiContext->GetHwContext()->FlushLock(GetCSLSession(), m_lastCSLSyncId, lastSubmittedRequestId);

        // Wait for all the results. if times out report error
        if (FALSE == WaitTillAllResultsAvailable())
        {
            if (TRUE == HwEnvironment::GetInstance()->GetStaticSettings()->raisesigabrt)
            {
                CAMX_LOG_ERROR(CamxLogGroupCore, "FATAL ERROR: Flush could not clean up the requests in time");
                OsUtils::RaiseSignalAbort();
            }
            else
            {
                // Give the nodes more time to process
                UINT fallbackWaitTime = HwEnvironment::GetInstance()->GetStaticSettings()->sessionFallbackWaitTime;

                CAMX_LOG_ERROR(CamxLogGroupCore, "Flush timed out, Giving the nodes %u additional fallback wait time.",
                    fallbackWaitTime);

                m_pResultLock->Lock();

                if (FALSE == m_pWaitAllResultsAvailableSignaled)
                {
                    result = m_pWaitAllResultsAvailable->TimedWait(m_pResultLock->GetNativeHandle(), fallbackWaitTime);
                }
                m_pWaitAllResultsAvailableSignaled = FALSE;
                m_pResultLock->Unlock();

                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupCore,
                        "Fallback TimedWait for results timed out at %u ms with error %s!",
                        fallbackWaitTime, Utils::CamxResultToString(result));
                }
                else
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupCore, "Fallback TimedWait returned with success");
                }
            }

            for (UINT32 index = 0; index < m_numPipelines; index++)
            {
                if (PipelineStatus::STREAM_ON == m_pipelineData[index].pPipeline->GetPipelineStatus())
                {
                    m_pipelineData[index].pPipeline->FlushPendingNodes();
                }
            }
        }


        for (UINT32 index = 0; index < m_numPipelines; index++)
        {
            Pipeline* pPipeline = m_pipelineData[index].pPipeline;
            if (PipelineStatus::STREAM_ON == pPipeline->GetPipelineStatus())
            {
                pPipeline->ClearPendingResources();
                pPipeline->FlushMetadata();
            }
        }
        UpdateLastFlushedRequestId();

        // Call unlock after all the results are out
        m_pChiContext->GetHwContext()->FlushUnlock(GetCSLSession());

        // Empty the Hal3Queue
        FlushHALQueue();

        CamxAtomicStoreU8(&m_aFlushStatus, FALSE);

        m_qtimerErrIndicated = 0;

        for (UINT32 index = 0; index < m_numPipelines; index++)
        {
            if (PipelineStatus::STREAM_ON == m_pipelineData[index].pPipeline->GetPipelineStatus())
            {
                // Disable AE lock if already set
                if (TRUE == m_pipelineData[index].pPipeline->IsRealTime())
                {
                    SetAELockRange(index, 0, 0);
                }
            }
        }

        CAMX_LOG_INFO(CamxLogGroupCore, "Flush is done for session %p", this);
        m_pFlushLock->Unlock();

        CamxAtomicStoreU8(&m_aSessionDumpComplete, FALSE);
        OsUtils::GetTime(&pTime);
        flushEndTime = OsUtils::CamxTimeToMillis(&pTime);

        CAMX_LOG_CONFIG(CamxLogGroupCore, "Flushing %p took %u ms", this, (flushEndTime - flushStartTime));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::Destroy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::Destroy()
{
    CAMX_ENTRYEXIT(CamxLogGroupCore);

    CAMX_LOG_INFO(CamxLogGroupCore, "Destroying session %p", this);

    ClearPendingMBDoneQueue();

    Flush();

    this->FlushThreadJobCallback();

    // Due to drain logic we had better destroy anything that has a job registered first (including DRQ)
    // But - as the Nodes already have m_hNodeJobHandle with them, they may continue posting jobs (can happen if Flush timedout)
    // If we Unregister here and Nodes postJob using the same m_hNodeJobHandle - there can be 2 cases :
    // 1. This JobHandle slot is reset while Unregister, so PostJob will fail as the JobHandle slot validation fails because it
    //    is invalid - PostJob after Unregister is handled with error, no issues.
    // 2. This JobHandle slot is reset while Unregister, but say if Register from a different client comes immediately
    //    and the same JobHandle slot is given to that client (i.e now this slot of JobHandle becomes valid with that client's
    //    callback function, etc), and then Nodes post their jobs using the same handle, but they are actually going into the
    //    queue of different client which just acquired the same JobHandle slot. So whatever jobs that the Nodes are posting
    //    would be called into the callback function of different client - causing issues.
    // So, just do Flush here, so that we will flush all pending jobs and also do not allow any further jobs from Nodes
    // because, flush status is set to Flushed (so further PostJob will fail)
    // And do actual UnregisterJobFamily later after all Nodes are destroyed.

    if (InvalidJobHandle != m_hNodeJobHandle)
    {
        m_pThreadManager->FlushJobFamily(m_hNodeJobHandle, m_pThreadManager, TRUE);
    }

    /// @todo (CAMX-1797) Temporary workaround - Need to figure out the right place
    for (UINT i = 0; i < m_numPipelines; i++)
    {
        if (NULL != m_pipelineData[i].pPipeline)
        {
            // Disable AE lock if already set
            if (TRUE == m_pipelineData[i].pPipeline->IsRealTime())
            {
                SetAELockRange(i, 0, 0);
            }
            m_pipelineData[i].pPipeline->StreamOff(CHIDeactivateModeDefault);
            m_pipelineData[i].pPipeline->Unlink();
        }
    }

    if (NULL != m_pDeferredRequestQueue)
    {
        m_pDeferredRequestQueue->Destroy();
        m_pDeferredRequestQueue = NULL;
    }

    // We should have no more result waiting for this session, so we are good to move all the nodes
    // we allocated

    LightweightDoublyLinkedListNode* pNode = m_resultHolderList.RemoveFromHead();
    while (NULL != pNode)
    {
        CAMX_ASSERT(NULL != pNode->pData);
        if (NULL != pNode->pData)
        {
            CAMX_FREE(pNode->pData);
            pNode->pData = NULL;
        }
        CAMX_FREE(pNode);
        pNode = m_resultHolderList.RemoveFromHead();
    }


    if (NULL != m_pLivePendingRequestsLock)
    {
        m_pLivePendingRequestsLock->Destroy();
        m_pLivePendingRequestsLock = NULL;
    }

    if (NULL != m_pWaitLivePendingRequests)
    {
        m_pWaitLivePendingRequests->Destroy();
        m_pWaitLivePendingRequests = NULL;
    }

    for (UINT i = 0; i < m_numPipelines; i++)
    {
        if (NULL != m_pipelineData[i].pPipeline)
        {
            m_pipelineData[i].pPipeline->Destroy();
            m_pipelineData[i].pPipeline = NULL;
        }
    }

    this->UnregisterThreadJobCallback();

    // m_hNodeJobHandle is passed to Nodes while FinalizePipeline, so they have this handle within them, so they may still
    // continue posting jobs. So make sure to unregister this only after all the Nodes are destroyed.
    // In case we do not want to allow Nodes to PostJobs, set Flush status to Flushed by calling FlushJobFamily earlier
    if (InvalidJobHandle != m_hNodeJobHandle)
    {
        CHAR wrapperName[FILENAME_MAX];
        OsUtils::SNPrintF(&wrapperName[0], sizeof(wrapperName), "NodeCommonThreadJobFamily%p", this);
        m_pThreadManager->UnregisterJobFamily(NodeThreadJobFamilySessionCb, wrapperName, m_hNodeJobHandle);
        m_hNodeJobHandle = InvalidJobHandle;
    }

    for (UINT32 i = 0; i < m_requestQueueDepth * m_numPipelines * m_usecaseNumBatchedFrames; i++)
    {
        if (NULL != m_pCaptureResult[i].pOutputBuffers)
        {
            // NOWHINE CP036a: exception
            CAMX_FREE(const_cast<ChiStreamBuffer*>(m_pCaptureResult[i].pOutputBuffers));
            m_pCaptureResult[i].pOutputBuffers = NULL;
        }
    }

    if (NULL != m_pCaptureResult)
    {
        CAMX_FREE(m_pCaptureResult);
        m_pCaptureResult = NULL;
    }

    if (NULL != m_pRequestQueue)
    {
        m_pRequestQueue->Destroy();
        m_pRequestQueue = NULL;
    }

    if (NULL != m_pResultLock)
    {
        m_pResultLock->Destroy();
        m_pResultLock = NULL;
    }

    if (NULL != m_pRequestLock)
    {
        m_pRequestLock->Destroy();
        m_pRequestLock = NULL;
    }

    if (NULL != m_pFlushLock)
    {
        m_pFlushLock->Destroy();
        m_pFlushLock = NULL;
    }

    if (NULL != m_pStreamOnOffLock)
    {
        m_pStreamOnOffLock->Destroy();
        m_pStreamOnOffLock = NULL;
    }

    if (NULL != m_pSessionDumpLock)
    {
        m_pSessionDumpLock->Destroy();
        m_pSessionDumpLock = NULL;
    }

    if (NULL != m_pPerFrameDebugDataPool)
    {
        m_pPerFrameDebugDataPool->Destroy();
        m_pPerFrameDebugDataPool = NULL;
    }

    if (NULL != m_pStreamBufferInfo)
    {
        CAMX_FREE(m_pStreamBufferInfo);
        m_pStreamBufferInfo = NULL;
    }

    if (NULL != m_pWaitAllResultsAvailable)
    {
        m_pWaitAllResultsAvailable->Destroy();
        m_pWaitAllResultsAvailable = NULL;
    }

    if (NULL != m_pResultHolderListLock)
    {
        m_pResultHolderListLock->Destroy();
        m_pResultHolderListLock = NULL;
    }

    if (NULL != m_pPendingMBQueueLock)
    {
        m_pPendingMBQueueLock->Destroy();
        m_pPendingMBQueueLock = NULL;
    }

    if (NULL != m_resultStreamBuffers.pLock)
    {
        m_resultStreamBuffers.pLock->Destroy();
        m_resultStreamBuffers.pLock = NULL;
    }

    if (NULL != m_notifyMessages.pLock)
    {
        m_notifyMessages.pLock->Destroy();
        m_notifyMessages.pLock = NULL;
    }

    if (NULL != m_PartialDataMessages.pLock)
    {
        m_PartialDataMessages.pLock->Destroy();
        m_PartialDataMessages.pLock = NULL;
    }

    m_sesssionInitComplete = FALSE;

    CAMX_LOG_INFO(CamxLogGroupCore, "%s: CSLClose(0x%x)", m_pipelineNames, GetCSLSession());
    m_pChiContext->GetHwContext()->CloseSession(GetCSLSession());

    CAMX_LOG_CONFIG(CamxLogGroupCore, "Session (%p) Destroy", this);
    CAMX_DELETE this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::FinalizeDeferPipeline
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::FinalizeDeferPipeline(
    UINT32 pipelineIndex)
{
    CAMX_ENTRYEXIT(CamxLogGroupCore);
    CamxResult result = CamxResultSuccess;
    CAMX_ASSERT(pipelineIndex < m_numPipelines);

    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();
    m_requestBatchId[pipelineIndex]           = CamxInvalidRequestId;

    FinalizeInitializationData finalizeInitializationData = { 0 };

    finalizeInitializationData.pHwContext                 = m_pChiContext->GetHwContext();
    finalizeInitializationData.pDeferredRequestQueue      = m_pDeferredRequestQueue;
    finalizeInitializationData.pDebugDataPool             = m_pPerFrameDebugDataPool;
    finalizeInitializationData.pSession                   = this;
    finalizeInitializationData.pThreadManager             = m_pThreadManager;
    finalizeInitializationData.usecaseNumBatchedFrames    = m_usecaseNumBatchedFrames;
    finalizeInitializationData.enableQTimer               = pStaticSettings->enableQTimer;
    finalizeInitializationData.numSessionPipelines        = m_numPipelines;
    finalizeInitializationData.pSensorModeInfo            =
        &(m_pipelineData[pipelineIndex].pPipelineDescriptor->inputData[0].sensorInfo.sensorMode);
    finalizeInitializationData.resourcePolicy             =
        m_pipelineData[pipelineIndex].pipelineFinalizeData.pipelineResourcePolicy;


    if (InvalidJobHandle == m_hNodeJobHandle)
    {
        CHAR wrapperName[FILENAME_MAX];
        OsUtils::SNPrintF(&wrapperName[0], sizeof(wrapperName), "NodeCommonThreadJobFamily%p", this);
        result = m_pThreadManager->RegisterJobFamily(NodeThreadJobFamilySessionCb,
                                                     wrapperName,
                                                     NULL,
                                                     JobPriority::Normal,
                                                     TRUE,
                                                     &m_hNodeJobHandle);
    }

    if (CamxResultSuccess == result)
    {
        finalizeInitializationData.hThreadJobFamilyHandle     = m_hNodeJobHandle;

        result = m_pipelineData[pipelineIndex].pPipeline->FinalizePipeline(&finalizeInitializationData);

    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::FinalizePipeline
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::FinalizePipeline(
    SessionCreateData* pCreateData,
    UINT32             pipelineIndex,
    BIT                enableQTimer)
{
    CAMX_ENTRYEXIT_NAME(CamxLogGroupCore, "SessionFinalizePipeline");
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pCreateData);

    if (FALSE == pCreateData->pPipelineInfo[pipelineIndex].isDeferFinalizeNeeded)
    {
        FinalizeInitializationData finalizeInitializationData = { 0 };

        finalizeInitializationData.pHwContext                 = pCreateData->pHwContext;
        finalizeInitializationData.pDeferredRequestQueue      = m_pDeferredRequestQueue;
        finalizeInitializationData.pDebugDataPool             = m_pPerFrameDebugDataPool;
        finalizeInitializationData.pSession                   = this;
        finalizeInitializationData.pThreadManager             = m_pThreadManager;
        finalizeInitializationData.usecaseNumBatchedFrames    = m_usecaseNumBatchedFrames;
        finalizeInitializationData.enableQTimer               = enableQTimer;
        finalizeInitializationData.numSessionPipelines        = pCreateData->numPipelines;
        finalizeInitializationData.pSensorModeInfo            =
            pCreateData->pPipelineInfo[pipelineIndex].pipelineInputInfo.sensorInfo.pSensorModeInfo;
        finalizeInitializationData.resourcePolicy             =
            pCreateData->pPipelineInfo[pipelineIndex].pipelineResourcePolicy;

        if (InvalidJobHandle == m_hNodeJobHandle)
        {
            CHAR wrapperName[FILENAME_MAX];
            OsUtils::SNPrintF(&wrapperName[0], sizeof(wrapperName), "NodeCommonThreadJobFamily%p", this);
            result = m_pThreadManager->RegisterJobFamily(NodeThreadJobFamilySessionCb,
                                                         wrapperName,
                                                         NULL,
                                                         JobPriority::Normal,
                                                         TRUE,
                                                         &m_hNodeJobHandle);
        }

        if (CamxResultSuccess == result)
        {
            finalizeInitializationData.hThreadJobFamilyHandle     = m_hNodeJobHandle;
            CAMX_LOG_INFO(CamxLogGroupCore, "Finalize for pipeline %d, %s !",
                          pipelineIndex, m_pipelineData[pipelineIndex].pPipeline->GetPipelineName());
            result = m_pipelineData[pipelineIndex].pPipeline->FinalizePipeline(&finalizeInitializationData);
        }
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupCore, "deferred pipeline %d, %s finalization!",
                      pipelineIndex, m_pipelineData[pipelineIndex].pPipeline->GetPipelineName());
        m_pipelineData[pipelineIndex].pipelineFinalizeData.pipelineResourcePolicy =
            pCreateData->pPipelineInfo[pipelineIndex].pipelineResourcePolicy;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::GetNumInputSensors
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 Session::GetNumInputSensors(
    SessionCreateData* pSessionCreateData)
{
    UINT32 numOfInputSensors = 0;
    for (UINT i = 0; i < pSessionCreateData->numPipelines; i++)
    {
        ChiPipelineInfo*      pPipelineInfo       = &pSessionCreateData->pPipelineInfo[i];
        ChiPipelineInputInfo* pPipelineInput      = &pPipelineInfo->pipelineInputInfo;
        if (TRUE == pPipelineInput->isInputSensor)
        {
            numOfInputSensors++;
        }
    }
    return numOfInputSensors;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::GetSensorSyncMode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SensorSyncMode Session::GetSensorSyncMode(
    UINT32 cameraID)

{
    SensorSyncMode syncMode  = NoSync;
    CamxResult result        = CamxResultSuccess;

    // read hw sync mode from static meta
    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();
    if ((TRUE == pStaticSettings->enableSensorHWSync) && (m_numInputSensors > 1))
    {
        UINT32  metaTag                           = 0;
        VOID*   pSyncMode                         = NULL;
        SensorSyncModeMetadata* pSensorSyncConfig = NULL;
        MetadataSlot* pStaticMetadataSlot         = m_pChiContext->GetStaticMetadataPool(cameraID)->GetSlot(0);

        CAMX_ASSERT(pStaticMetadataSlot != NULL);

        result = VendorTagManager::QueryVendorTagLocation("com.qti.chi.multicamerasensorconfig",
            "sensorsyncmodeconfig", &metaTag);

        if (CamxResultSuccess == result)
        {
            pSensorSyncConfig = static_cast<SensorSyncModeMetadata*>(pStaticMetadataSlot->GetMetadataByTag(metaTag));
            if ((NULL != pSensorSyncConfig) && pSensorSyncConfig->isValid)
            {
                syncMode = pSensorSyncConfig->sensorSyncMode;
            }
            else
            {
                CAMX_LOG_WARN(CamxLogGroupCore, "Get Multi Camera hardware sync metadata failed %p", pSensorSyncConfig);
            }
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupCore, "No sensor sync tag found!");
        }
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupCore, "Disable sensor hardware sync for CameraId:%d", cameraID);
    }

    return syncMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::NodeThreadJobFamilySessionCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* Session::NodeThreadJobFamilySessionCb(
    VOID* pCbData)
{
    CAMX_ASSERT(NULL != pCbData);

    return Node::NodeThreadJobFamilyCb(pCbData);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::BuildSessionName
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::BuildSessionName(
    SessionCreateData* pCreateData)
{
    m_pipelineNames[0] = '\0';
    for (UINT i = 0; i < pCreateData->numPipelines; i++)
    {
        ChiPipelineInfo*      pPipelineInfo       = &pCreateData->pPipelineInfo[i];
        PipelineDescriptor*   pPipelineDescriptor = reinterpret_cast<PipelineDescriptor*>(pPipelineInfo->hPipelineDescriptor);
        if (0 < i)
        {
            OsUtils::StrLCat(m_pipelineNames, ", ", sizeof(m_pipelineNames));
        }
        OsUtils::StrLCat(m_pipelineNames, pPipelineDescriptor->pipelineName, sizeof(m_pipelineNames));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::Initialize(
    SessionCreateData* pCreateData)
{
    CamxResult           result                    = CamxResultSuccess;
    UINT32               additionalNeededRequests  = 0;
    UINT32               numMetadataSlots          = DefaultPerFramePoolWindowSize;

    BOOL requireExtraHalRequest = FALSE;

    CAMX_ASSERT(NULL != pCreateData);
    CAMX_ASSERT(NULL != pCreateData->pThreadManager);

    Utils::Memcpy(&m_chiCallBacks, pCreateData->pChiAppCallBacks, sizeof(ChiCallBacks));

    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    m_pThreadManager            = pCreateData->pThreadManager;
    m_pChiContext               = pCreateData->pChiContext;
    m_pPrivateCbData            = pCreateData->pPrivateCbData;
    m_sequenceId                = 0;
    m_syncSequenceId            = 1;
    m_numRealtimePipelines      = 0;
    m_numMetadataResults        = 1;

    BOOL isRealtimePipeline = SetRealtimePipeline(pCreateData);
    BOOL isActiveSensor     = CheckActiveSensor(pCreateData);

    BuildSessionName(pCreateData);

    // Checking to see if there is an active CSLSession for the camera
    if ((TRUE == isRealtimePipeline) && (TRUE == isActiveSensor))
    {
        result = GetActiveCSLSession(pCreateData, &m_hCSLSession);

        if (CamxResultSuccess == result)
        {
            result = CSLAddReference(GetCSLSession());
            CAMX_LOG_INFO(CamxLogGroupCore, "%s: CSLAddReference(0x%x)", m_pipelineNames, m_hCSLSession);
        }
    }
    else
    {
        result = m_pChiContext->GetHwContext()->OpenSession(&m_hCSLSession);
        CAMX_LOG_INFO(CamxLogGroupCore, "%s: CSLOpen(0x%x)", m_pipelineNames, m_hCSLSession);
    }

    if (CamxResultSuccess != result)
    {
        m_hCSLSession = CSLInvalidHandle;
        CAMX_LOG_ERROR(CamxLogGroupCore, "Could not obtain CSL Session handle.");
        return result;
    }

    m_hNodeJobHandle                        = InvalidJobHandle;
    m_numInputSensors                       = GetNumInputSensors(pCreateData);
    m_numPipelines                          = pCreateData->numPipelines;
    m_recordingEndOfStreamTagId             = 0;
    m_setVideoPerfModeFlag                  = FALSE;
    m_sesssionInitComplete                  = FALSE;
    m_longExposureTimeout                   = 0;
    m_recordingEndOfStreamRequestIdTagId    = 0;
    m_lastFPSCountTime                      = 0;
    m_numLinksSynced                        = 0;

    Utils::Memset(m_isInSyncedLinks, 0, sizeof(m_isInSyncedLinks));

    for (UINT pipelineIdx = 0; pipelineIdx < pCreateData->numPipelines; pipelineIdx++)
    {
        m_flushInfo.lastFlushRequestId[pipelineIdx] = CamxInvalidRequestId;
    }

    if (1 == pCreateData->flags.u.IsFastAecSession)
    {
        m_statsOperationMode = StatsOperationModeFastConvergence;
    }
    else
    {
        m_statsOperationMode = StatsOperationModeNormal;
    }

    for (UINT i = 0; i < pCreateData->numPipelines; i++)
    {
        ChiPipelineInfo*      pPipelineInfo       = &pCreateData->pPipelineInfo[i];
        PipelineDescriptor*   pPipelineDescriptor = reinterpret_cast<PipelineDescriptor*>(pPipelineInfo->hPipelineDescriptor);
        ChiPipelineInputInfo* pPipelineInput      = &pPipelineInfo->pipelineInputInfo;

        m_pipelineData[i].pPipelineDescriptor = pPipelineDescriptor;
        /// @todo (CAMX-3119) remove IsTorchWidgetSession setting below and handle this in generic way.
        m_isTorchWidgetSession                |= pPipelineDescriptor->flags.isTorchWidget;

        // Consume the input buffer info for offline pipelines and update the pipeline descriptor with that information
        if (FALSE == pPipelineInput->isInputSensor)
        {
            ChiInputBufferInfo* pChiInputBufferInfo = &pPipelineInput->inputBufferInfo;

            pPipelineDescriptor->numInputs = pChiInputBufferInfo->numInputBuffers;

            for (UINT input = 0; input < pChiInputBufferInfo->numInputBuffers; input++)
            {
                const ChiPortBufferDescriptor* pBufferDescriptor = &pChiInputBufferInfo->pInputBufferDescriptors[input];
                ChiStream*                     pChiStream        = pBufferDescriptor->pStream;
                Camera3Stream*                 pHAL3Stream       = reinterpret_cast<Camera3Stream*>(pChiStream);
                ChiStreamWrapper*              pChiStreamWrapper =
                    reinterpret_cast<ChiStreamWrapper*>(pChiStream->pPrivateInfo);

                if (ChiStreamTypeInput == pChiStream->streamType)
                {
                    OverrideOutputFormat overrideImpDefinedFormat = { {0} };
                    overrideImpDefinedFormat.isRaw                = pBufferDescriptor->bIsOverrideImplDefinedWithRaw;
                    Format selectedFormat                         = m_pChiContext->SelectFormat(pChiStream,
                                                                    overrideImpDefinedFormat);

                    pChiStreamWrapper = CAMX_NEW ChiStreamWrapper(pHAL3Stream, input, selectedFormat);

                    if (NULL != pChiStreamWrapper)
                    {
                        /// @todo (CAMX-1512) Session can contain all the created Wrappers that it can clean up when destroyed
                        // The wrapper is created by this session
                        BOOL isOwner = TRUE;

                        m_pChiContext->SetChiStreamInfo(pChiStreamWrapper, pPipelineDescriptor->numBatchedFrames, FALSE);

                        pChiStream->pPrivateInfo                                  = pChiStreamWrapper;

                        // pPipelineDescriptor->inputData[input].isWrapperOwner      = isOwner;

                        m_pChiContext->SetPipelineDescriptorInputStream(pPipelineDescriptor, pBufferDescriptor, isOwner);
                        pChiStreamWrapper->SetPortId(pBufferDescriptor->nodePort.nodePortId);
                        CAMX_LOG_VERBOSE(CamxLogGroupCore, "Input %d, portId %d wd ht %d x %d wrapper %x stream %x",
                            input, pBufferDescriptor->nodePort.nodePortId, pChiStream->width, pChiStream->height,
                            pChiStreamWrapper, pChiStream);
                    }
                    else
                    {
                        result = CamxResultENoMemory;
                        CAMX_LOG_ERROR(CamxLogGroupCore, "Out of memory!");
                        break;
                    }
                }
                else
                {
                    /// @todo (CAMX-1512) Session can contain all the created Wrappers that it can clean up when destroyed
                    // The wrapper was created by some other session and this session is simply using it as an input
                    BOOL isOwner = FALSE;

                    if ((TRUE == pCreateData->isNativeChi) && (NULL == pChiStreamWrapper))
                    {
                        OverrideOutputFormat overrideImpDefinedFormat = { {0} };
                        overrideImpDefinedFormat.isRaw                = pBufferDescriptor->bIsOverrideImplDefinedWithRaw;
                        Format selectedFormat                         = m_pChiContext->SelectFormat(pChiStream,
                                                                        overrideImpDefinedFormat);

                        pChiStreamWrapper        = CAMX_NEW ChiStreamWrapper(pHAL3Stream, input, selectedFormat);
                        pChiStream->pPrivateInfo = pChiStreamWrapper;
                        isOwner                  = TRUE;
                    }

                    if (NULL == pChiStreamWrapper)
                    {
                        CAMX_LOG_ERROR(CamxLogGroupCore, "ChiStreamWrapper cannot be NULL!");
                        result = CamxResultEFailed;
                        break;
                    }

                    // pPipelineDescriptor->inputData[input].isWrapperOwner      = isOwner;

                    m_pChiContext->SetPipelineDescriptorInputStream(pPipelineDescriptor, pBufferDescriptor, isOwner);

                    // Need to save only input port information
                    if (ChiStreamTypeBidirectional == pChiStream->streamType)
                    {
                        pChiStreamWrapper->SetPortId(pBufferDescriptor->nodePort.nodePortId);
                        CAMX_LOG_VERBOSE(CamxLogGroupCore, "Bidirectional %d, portId %d wd ht %d x %d wrapper %x stream %x",
                            input, pBufferDescriptor->nodePort.nodePortId, pChiStream->width, pChiStream->height,
                            pChiStreamWrapper, pChiStream);
                    }
                }
            }
        }
        else
        {
            SensorInfo* pSensorInfo = &pPipelineDescriptor->inputData[0].sensorInfo;

            pSensorInfo->cameraId = pPipelineInput->sensorInfo.cameraId;
            Utils::Memcpy(&pSensorInfo->sensorMode, pPipelineInput->sensorInfo.pSensorModeInfo, sizeof(ChiSensorModeInfo));
            if ((60 <= (pSensorInfo->sensorMode.frameRate / pSensorInfo->sensorMode.batchedFrames)) ||
                (8 <= pSensorInfo->sensorMode.batchedFrames))
            {
                // Extra HAL request is needed
                // 1. when effective frame rate is 60FPS or more.
                // 2. when batch size is 8 or moe
                requireExtraHalRequest = TRUE;
            }
        }
    }

    DeferredRequestQueueCreateData pDeferredCreateData;
    pDeferredCreateData.numPipelines   = pCreateData->numPipelines;
    pDeferredCreateData.pThreadManager = m_pThreadManager;

    for (UINT i = 0; i < pCreateData->numPipelines; i++)
    {
        ChiPipelineInfo*    pPipelineInfo       = &pCreateData->pPipelineInfo[i];
        PipelineDescriptor* pPipelineDescriptor = reinterpret_cast<PipelineDescriptor*>(pPipelineInfo->hPipelineDescriptor);

        m_pipelineData[i].pPipelineDescriptor             = pPipelineDescriptor;
        m_pipelineData[i].pPipeline                       = m_pChiContext->CreatePipelineFromDesc(pPipelineDescriptor, i);

        if (NULL == m_pipelineData[i].pPipeline)
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "Pipeline creation failed for %d pipeline", i);
            result = CamxResultEFailed;
            break;
        }

        pPipelineInfo->pipelineOutputInfo.hPipelineHandle = m_pipelineData[i].pPipeline;

        ChiPipelineInputInfo* pPipelineInput      = &pPipelineInfo->pipelineInputInfo;

        for (UINT j = 0; j < pPipelineDescriptor->pipelineInfo.numNodes; j++)
        {
            if (pPipelineDescriptor->pipelineInfo.pNodeInfo[j].nodeId == ExtSensorNodeId)
            {
                CAMX_LOG_CONFIG(CamxLogGroupCore, "Storing CSL session %p", GetCSLSession());
                HwEnvironment* pHWEnvironment = HwEnvironment::GetInstance();

                if (NULL != pHWEnvironment)
                {
                    if (CSLInvalidHandle != GetCSLSession())
                    {
                        pHWEnvironment->InitializeSensorHwDeviceCache(pPipelineInput->sensorInfo.cameraId, NULL,
                                                                      GetCSLSession(), 1, NULL, NULL);
                    }
                }
            }
        }

        UINT32 frameDelay = m_pipelineData[i].pPipeline->DetermineFrameDelay();
        UINT32 extraFrameworkBuffers = m_pipelineData[i].pPipeline->DetermineExtrabuffersNeeded();

        if (0 != frameDelay)
        {
            for (UINT output = 0; output < pPipelineDescriptor->numOutputs; output++)
            {
                ChiStreamWrapper* pChiStreamWrapper = pPipelineDescriptor->outputData[output].pOutputStreamWrapper;
                // After successfully creating the pipeline, need to set the maximum num of native buffers for the stream
                if (TRUE == pChiStreamWrapper->IsVideoStream())
                {
                    UINT32 maxNumBuffers = (pStaticSettings->maxHalRequests + frameDelay) *
                                           (pPipelineDescriptor->numBatchedFrames);
                    pChiStreamWrapper->SetNativeMaxNumBuffers(maxNumBuffers);
                }
                else if ((extraFrameworkBuffers > 0) && (TRUE == pChiStreamWrapper->IsPreviewStream()))
                {
                    // Need extra buffers in EIS usecase and dewarp chinode node will publish the required count
                    // If the dewarp node is not present in pipeline extraFrameworkBuffers is 0
                    UINT32 maxNumBuffers = pStaticSettings->maxHalRequests + extraFrameworkBuffers;
                    pChiStreamWrapper->SetNativeMaxNumBuffers(maxNumBuffers);
                }
            }
        }

        if (frameDelay > additionalNeededRequests)
        {
            additionalNeededRequests = frameDelay;
        }
    }

    if (CamxResultSuccess == result)
    {
        numMetadataSlots = DefaultPerFramePoolWindowSize + (additionalNeededRequests * 2);

        if (pCreateData->usecaseNumBatchedFrames > 1)
        {
            numMetadataSlots = DefaultPerFramePoolWindowSize * 2;
        }
        CAMX_ASSERT(numMetadataSlots <= MaxPerFramePoolWindowSize);

        for (UINT i = 0; i < pCreateData->numPipelines; i++)
        {
            ChiPipelineInfo* pPipelineInfo = &pCreateData->pPipelineInfo[i];

            if (TRUE == m_pipelineData[i].pPipeline->IsRealTime())
            {
                m_pipelineData[i].pPipeline->InitializeMetadataPools(numMetadataSlots);
            }
            else
            {
                m_pipelineData[i].pPipeline->InitializeMetadataPools(DefaultRequestQueueDepth);
            }

            if (pPipelineInfo->pipelineInputInfo.isInputSensor)
            {
                UINT32 cameraID = pPipelineInfo->pipelineInputInfo.sensorInfo.cameraId;
                m_pipelineData[i].pPipeline->SetSensorSyncMode(GetSensorSyncMode(cameraID));
            }

            pDeferredCreateData.pMainPools[i]     = m_pipelineData[i].pPipeline->GetPerFramePool(PoolType::PerFrameResult);

            if (TRUE == pPipelineInfo->pipelineInputInfo.isInputSensor)
            {
                MetadataPool* pUsecasePool    = m_pipelineData[i].pPipeline->GetPerFramePool(PoolType::PerUsecase);
                MetadataSlot* pSlot           = pUsecasePool->GetSlot(0);

                StatsStreamInitConfig statsStreamInitConfig = { };
                statsStreamInitConfig.operationMode         = m_statsOperationMode;

                pSlot->SetMetadataByTag(PropertyIDUsecaseStatsStreamInitConfig, &statsStreamInitConfig,
                                        sizeof(statsStreamInitConfig),
                                        "camx_session");

                pSlot->PublishMetadata(PropertyIDUsecaseStatsStreamInitConfig);

                m_numRealtimePipelines++;
            }
        }
    }

    if (CamxResultSuccess == result)
    {
        UINT32 currentRequestDepth            = DefaultRequestQueueDepth + additionalNeededRequests;
        pDeferredCreateData.requestQueueDepth = currentRequestDepth;

        m_requestQueueDepth    = currentRequestDepth * pCreateData->usecaseNumBatchedFrames;

        m_usecaseNumBatchedFrames  = pCreateData->usecaseNumBatchedFrames;
        /// @todo (CAMX-2876) The 8 limit is artificial, and based on the frame number remapping array (m_fwFrameNumberMap)
        CAMX_ASSERT(m_usecaseNumBatchedFrames < 8);

        m_numPipelines             = pCreateData->numPipelines;
        /// @todo (CAMX-1512) Metadata pools needs to be per pipeline
        m_pPerFrameDebugDataPool   = MetadataPool::Create(PoolType::PerFrameDebugData,
                                                          UINT_MAX,
                                                          m_pThreadManager,
                                                          numMetadataSlots,
                                                          "Session",
                                                          0);

        m_pResultLock                       = Mutex::Create("SessionResultLock");
        m_pRequestLock                      = Mutex::Create("SessionRequestLock");
        m_pFlushLock                        = Mutex::Create("SessionFlushLock");
        m_pStreamOnOffLock                  = Mutex::Create("SessionStreamOnOffLock");
        m_pSessionDumpLock                  = Mutex::Create("SessionDumpLock");
        m_pResultHolderListLock             = Mutex::Create("ResultHolderListLock");
        m_resultStreamBuffers.pLock         = Mutex::Create("ResultStreamBuffers.pLock");
        m_notifyMessages.pLock              = Mutex::Create("NotifyMessages.pLock");
        m_PartialDataMessages.pLock         = Mutex::Create("PartialDataMessages.pLock");
        m_pPendingMBQueueLock               = Mutex::Create("PendingMBDoneLock");

        m_pDeferredRequestQueue             = DeferredRequestQueue::Create(&pDeferredCreateData);
        m_pWaitAllResultsAvailable          = Condition::Create("SessionWaitAllResultsAvailable");
        m_pWaitAllResultsAvailableSignaled  = FALSE;
        m_pCaptureResult                    = static_cast<ChiCaptureResult*>(
                                                    CAMX_CALLOC(m_requestQueueDepth * m_usecaseNumBatchedFrames *
                                                    m_numPipelines * sizeof(ChiCaptureResult)));

        m_pWaitLivePendingRequests     = Condition::Create("WaitInFlightRequests");
        m_pLivePendingRequestsLock     = Mutex::Create("InFlightRequests");
        m_livePendingRequests = 0;

        if (TRUE == requireExtraHalRequest)
        {
            additionalNeededRequests += 1;
        }

        m_maxLivePendingRequests = (pStaticSettings->maxHalRequests + additionalNeededRequests) *
            m_usecaseNumBatchedFrames;
        m_defaultMaxLivePendingRequests = (pStaticSettings->maxHalRequests + additionalNeededRequests);

        CAMX_LOG_INFO(CamxLogGroupCore, "%s: m_requestQueueDepth=%u m_usecaseNumBatchedFrames=%u",
                      m_pipelineNames, m_requestQueueDepth, m_usecaseNumBatchedFrames);
        m_pRequestQueue = HAL3Queue::Create(m_requestQueueDepth, m_usecaseNumBatchedFrames, CreatedAs::Empty);

        if (NULL != m_pCaptureResult)
        {
            for (UINT32 i = 0; i < m_requestQueueDepth * m_numPipelines * m_usecaseNumBatchedFrames; i++)
            {
                m_pCaptureResult[i].pOutputBuffers =
                    static_cast<ChiStreamBuffer*>(CAMX_CALLOC(MaxNumOutputBuffers * sizeof(ChiStreamBuffer)));

                if (NULL == m_pCaptureResult[i].pOutputBuffers)
                {
                    CAMX_LOG_ERROR(CamxLogGroupCore, "Out of memory");
                    result = CamxResultENoMemory;
                    break;
                }
            }
        }

        if (CamxResultSuccess == result)
        {
            m_pStreamBufferInfo = static_cast<StreamBufferInfo*>(
                CAMX_CALLOC(sizeof(StreamBufferInfo) * MaxPipelinesPerSession * m_usecaseNumBatchedFrames));

            if (NULL != m_pStreamBufferInfo)
            {
                for (UINT32 i = 0; i < MaxPipelinesPerSession; i++)
                {
                    m_captureRequest.requests[i].pStreamBuffers = &m_pStreamBufferInfo[i * m_usecaseNumBatchedFrames];
                }
            }
            else
            {
                result = CamxResultENoMemory;
            }
        }

        if (CamxResultSuccess == result)
        {
            if ((NULL == m_pRequestQueue)                   ||
                (NULL == m_pPerFrameDebugDataPool)          ||
                (NULL == m_pResultLock)                     ||
                (NULL == m_pResultHolderListLock)           ||
                (NULL == m_resultStreamBuffers.pLock)       ||
                (NULL == m_notifyMessages.pLock )           ||
                (NULL == m_PartialDataMessages.pLock)       ||
                (NULL == m_pRequestLock)                    ||
                (NULL == m_pStreamBufferInfo)               ||
                (NULL == m_pStreamOnOffLock)                ||
                (NULL == m_pDeferredRequestQueue)           ||
                (NULL == m_pCaptureResult)                  ||
                (NULL == m_pWaitLivePendingRequests)        ||
                (NULL == m_pLivePendingRequestsLock)        ||
                (NULL == m_pPendingMBQueueLock)             ||
                (NULL == m_pWaitAllResultsAvailable))
            {
                CAMX_LOG_ERROR(CamxLogGroupCore, "Out of memory");
                result = CamxResultENoMemory;
            }
            else
            {
                if (CamxResultSuccess == result)
                {
                    // Publish ChiSensorModeInfo structure to Vendor tags
                    UINT32             metaTag         = 0;
                    MetadataPool*      pPool           = NULL;
                    MetadataSlot*      pSlot           = NULL;
                    ChiSensorModeInfo* pSensorModeInfo = NULL;
                    result = VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.sensor_meta_data",
                                                                      "sensor_mode_info",
                                                                      &metaTag);
                    CAMX_ASSERT_MESSAGE(CamxResultSuccess == result, "Failed to get vendor tag: sensor_mode_info");

                    for (UINT i = 0; i < m_numPipelines; i++)
                    {
                        pPool = m_pipelineData[i].pPipeline->GetPerFramePool(PoolType::PerUsecase);
                        pSlot = pPool->GetSlot(0);
                        pSensorModeInfo = &m_pipelineData[i].pPipelineDescriptor->inputData[0].sensorInfo.sensorMode;

                        pSlot->SetMetadataByTag(metaTag,
                                                static_cast<VOID*>(pSensorModeInfo),
                                                sizeof(ChiSensorModeInfo),
                                                "camx_session");

                        pSlot->PublishMetadataList(&metaTag, 1);

                    }
                }
                result = InitializeNewPipelines(pCreateData);
            }
        }
    }

    CamxResult resultCode = VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.sensor_meta_data",
                                                                     "sensor_mode_index",
                                                                     &m_vendorTagSensorModeIndex);
    if (CamxResultSuccess != resultCode)
    {
        CAMX_LOG_ERROR(CamxLogGroupCore,
                        "Failed to find org.codeaurora.qcamera3.sensor_meta_data.sensor_mode_index, resultCode=%s",
                       Utils::CamxResultToString(resultCode));
    }

    resultCode = VendorTagManager::QueryVendorTagLocation("org.quic.camera.qtimer", "timestamp", &m_vendorTagIndexTimestamp);
    if (CamxResultSuccess != resultCode)
    {
        CAMX_LOG_ERROR(CamxLogGroupCore,
                       "Failed to find org.quic.camera.qtimer.timestamp, encoder will fallback to system time, resultCode=%s",
                       Utils::CamxResultToString(resultCode));
    }

    resultCode = VendorTagManager::QueryVendorTagLocation("org.quic.camera.recording", "endOfStream",
                                                          &m_recordingEndOfStreamTagId);
    if (CamxResultSuccess != resultCode)
    {
        m_recordingEndOfStreamTagId = 0;
        CAMX_LOG_ERROR(CamxLogGroupCore,
                       "Failed to find org.quic.camera.recording.endOfStream, resultCode=%s",
                       Utils::CamxResultToString(resultCode));
    }

    resultCode = VendorTagManager::QueryVendorTagLocation("org.quic.camera.recording", "endOfStreamRequestId",
                                                          &m_recordingEndOfStreamRequestIdTagId);
    if (CamxResultSuccess != resultCode)
    {
        m_recordingEndOfStreamRequestIdTagId = 0;
        CAMX_LOG_ERROR(CamxLogGroupCore,
                       "Failed to find org.quic.camera.recording.endOfStreamRequestId, resultCode=%s",
                       Utils::CamxResultToString(resultCode));
    }

    resultCode = VendorTagManager::QueryVendorTagLocation("com.qti.chi.multicamerainfo", "SyncMode",
        &m_sessionSync.syncModeTagID);
    if (CamxResultSuccess != resultCode)
    {
        m_sessionSync.syncModeTagID = 0;
        CAMX_LOG_ERROR(CamxLogGroupCore,
            "Failed to find com.qti.chi.multicamerainfo.SyncMode, resultCode=%s",
            Utils::CamxResultToString(resultCode));
    }

    resultCode = VendorTagManager::QueryVendorTagLocation("org.quic.camera.streamTypePresent", "preview",
                                                          &m_previewStreamPresentTagId);
    if (CamxResultSuccess != resultCode)
    {
        m_previewStreamPresentTagId = 0;
        CAMX_LOG_ERROR(CamxLogGroupCore,
                       "Failed to find org.quic.camera.streamTypePresent.preview, resultCode=%s",
                       CamxResultStrings[resultCode]);
    }

    if (CamxResultSuccess == result)
    {
        // Reset the pending queue
        ResetMetabufferPendingQueue();
        CAMX_LOG_CONFIG(CamxLogGroupCore, "Session (%p) Initialized", this);
        m_sesssionInitComplete = TRUE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::InitializeNewPipelines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::InitializeNewPipelines(
    SessionCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pCreateData);
    CAMX_ASSERT(m_usecaseNumBatchedFrames == pCreateData->usecaseNumBatchedFrames);

    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    for (UINT32 i = 0; i < m_numPipelines; i++)
    {
        m_requestBatchId[i] = CamxInvalidRequestId;
    }

    for (UINT i = 0; i < pCreateData->numPipelines; i++)
    {
        if ((TRUE == m_pipelineData[i].pPipeline->IsRealTime()) || (TRUE == IsTorchWidgetSession()))
        {
            result = FinalizePipeline(pCreateData,
                                      i,
                                      pStaticSettings->enableQTimer);
        }
        else
        {
            result = m_pipelineData[i].pPipeline->CheckOfflinePipelineInputBufferRequirements();
        }

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL, "FinalizePipeline(%d) failed!", i);
            break;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::NotifyResult
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::NotifyResult(
    ResultsData* pResultsData)
{
    CAMX_ASSERT(NULL != pResultsData);

    switch (pResultsData->type)
    {
        case CbType::Error:
            HandleErrorCb(&pResultsData->cbPayload.error, pResultsData->pipelineIndex, pResultsData->pPrivData);
            break;

        case CbType::Async:
            HandleAsyncCb(&pResultsData->cbPayload.async, pResultsData->pPrivData);
            break;

        case CbType::Metadata:
            HandleMetadataCb(&pResultsData->cbPayload.metadata,
                             pResultsData->pPrivData,
                             pResultsData->pipelineIndex);
            break;

        case CbType::PartialMetadata:
            HandlePartialMetadataCb(&pResultsData->cbPayload.partialMetadata,
                pResultsData->pPrivData,
                pResultsData->pipelineIndex);
            break;

        case CbType::EarlyMetadata:
            HandleEarlyMetadataCb(&pResultsData->cbPayload.metadata, pResultsData->pipelineIndex, pResultsData->pPrivData);
            break;

        case CbType::Buffer:
            HandleBufferCb(&pResultsData->cbPayload.buffer, pResultsData->pipelineIndex,
                           pResultsData->pPrivData);
            break;

        case CbType::SOF:
            HandleSOFCb(&pResultsData->cbPayload.sof);
            break;

        case CbType::MetaBufferDone:
            HandleMetaBufferDoneCb(&pResultsData->cbPayload.metabufferDone);
            break;

        default:
            break;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::StreamOn
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::StreamOn(
    CHIPIPELINEHANDLE hPipelineDescriptor)
{
    UINT32     index  = 0;
    CamxResult result = CamxResultSuccess;

    // input pipelineIndex not really match the index recorded by Session, so use Descriptor to find it.
    for (index = 0; index < m_numPipelines; index++)
    {
        if (hPipelineDescriptor == m_pipelineData[index].pPipelineDescriptor)
        {
            // found corresponding pipeline can use index to get to it
            break;
        }
    }

    CAMX_ASSERT(index < m_numPipelines);

    Pipeline* pPipeline = m_pipelineData[index].pPipeline;

    if ((NULL != pPipeline) && (PipelineStatus::STREAM_ON != pPipeline->GetPipelineStatus()))
    {
        PipelineStatus pipelineStatus = pPipeline->GetPipelineStatus();

        if (PipelineStatus::FINALIZED > pipelineStatus)
        {
            result = FinalizeDeferPipeline(index);
            pipelineStatus = pPipeline->GetPipelineStatus();
            CAMX_LOG_INFO(CamxLogGroupCore, "FinalizeDeferPipeline result: %d pipelineStatus: %d",
                result, pipelineStatus);
        }

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "FinalizeDeferPipeline() unsuccessful, Session StreamOn() is failed !!");
        }
        else
        {
            m_pStreamOnOffLock->Lock();
            if (PipelineStatus::FINALIZED <= pipelineStatus)
            {
                pPipeline->StreamOn();
            }
            m_pStreamOnOffLock->Unlock();
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::StreamOff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::StreamOff(
    CHIPIPELINEHANDLE           hPipelineDescriptor,
    CHIDEACTIVATEPIPELINEMODE   modeBitmask)
{
    UINT32     index  = 0;
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != hPipelineDescriptor);
    if (NULL != hPipelineDescriptor)
    {
        // Input pipelineIndex not really match the index recorded by Session, so use Descriptor to find it.
        for (index = 0;
             ((index < m_numPipelines) && (hPipelineDescriptor != m_pipelineData[index].pPipelineDescriptor));
             index++);

        CAMX_ASSERT(index < m_numPipelines);
        if (index < m_numPipelines)
        {
            // Found corresponding pipeline
            Pipeline* pPipeline = m_pipelineData[index].pPipeline;

            if (NULL != pPipeline)
            {
                // if it is multicamera LPM or release buffer for offline pipeline, don't wait result available,
                // result waiting is handled in chi layer.
                if (FALSE == ((modeBitmask & CHIDeactivateModeSensorStandby) ||
                    (modeBitmask & CHIDeactivateModeReleaseBuffer)))
                {
                    result = pPipeline->WaitForAllNodesRequest();
                }

                if (CamxResultSuccess == result)
                {
                    m_pStreamOnOffLock->Lock();

                    // Disable AE lock if already set
                    if (TRUE == pPipeline->IsRealTime())
                    {
                        SetAELockRange(index, 0, 0);
                    }

                    result = pPipeline->StreamOff(modeBitmask);

                    m_pStreamOnOffLock->Unlock();
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupCore, "pPipeline: %p", pPipeline);
                result = CamxResultEFailed;
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "Couldn't locate Pipeline (Handle/Descriptor: %p)", hPipelineDescriptor);
            result = CamxResultEFailed;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Pipeline Handle/Descriptor: %p", hPipelineDescriptor);
        result = CamxResultEInvalidPointer;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::UpdateLastFlushedRequestId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::UpdateLastFlushedRequestId()
{
    for (UINT pipelineIdx = 0; pipelineIdx < m_numPipelines; pipelineIdx++)
    {
        Pipeline* pPipeline              = m_pipelineData[pipelineIdx].pPipeline;

        if (NULL == pPipeline)
        {
            m_flushInfo.lastFlushRequestId[pipelineIdx] = CamxInvalidRequestId;
        }
        else
        {
            m_flushInfo.lastFlushRequestId[pipelineIdx] = pPipeline->GetLastSubmittedRequestId();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::ProcessCaptureRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::ProcessCaptureRequest(
    const ChiPipelineRequest* pPipelineRequests)
{
    CamxResult  result      = CamxResultEFailed;
    CamxResult  resultWait  = CamxResultSuccess;

    UINT        numRequests = pPipelineRequests->numRequests;
    UINT32      pipelineIndexes[MaxPipelinesPerSession];

    const StaticSettings* pStaticSettings = m_pChiContext->GetStaticSettings();

    CAMX_ASSERT(NULL != pPipelineRequests);
    CAMX_ASSERT(NULL != pStaticSettings);

    // Prepare info for each request on each pipeline
    for (UINT requestIndex = 0; requestIndex < numRequests; requestIndex++)
    {
        // input pipelineIndex not really match the index recorded by Session, so use GetPipelineIndex to get corresponding
        // pipeline index.
        pipelineIndexes[requestIndex] = GetPipelineIndex(pPipelineRequests->pCaptureRequests[requestIndex].hPipelineHandle);
        CAMX_ASSERT(pipelineIndexes[requestIndex] < m_numPipelines);
        CAMX_LOG_VERBOSE(CamxLogGroupCore,
            "Received(%d/%d) for framework framenumber %llu, num outputs %d on %s: PipelineStatus:%d",
            requestIndex+1,
            numRequests,
            pPipelineRequests->pCaptureRequests[requestIndex].frameNumber,
            pPipelineRequests->pCaptureRequests[requestIndex].numOutputs,
            m_pipelineData[pipelineIndexes[requestIndex]].pPipeline->GetPipelineIdentifierString(),
            m_pipelineData[pipelineIndexes[requestIndex]].pPipeline->GetPipelineStatus());
    }

    m_pLivePendingRequestsLock->Lock();

    CAMX_ASSERT(m_maxLivePendingRequests > 0);

    while (m_livePendingRequests >= m_maxLivePendingRequests - 1)
    {
        if (TRUE == m_aDeviceInError)
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "Device in error state, returning failure for session:%p", this);
            return CamxResultEFailed;
        }

        UINT waitTime = LivePendingRequestTimeoutDefault;

        if (m_sequenceId < m_maxLivePendingRequests * 2)
        {
            waitTime = LivePendingRequestTimeoutDefault + (m_maxLivePendingRequests * LivePendingRequestTimeOutExtendor);
        }

        waitTime = static_cast<UINT>(m_longExposureTimeout) + waitTime;

        CAMX_LOG_VERBOSE(CamxLogGroupCore,
                         "Timed Wait Live Pending Requests(%u) "
                         "Sequence Id %u "
                         "Live Pending Requests %u "
                         "Max Live Pending Requests %u "
                         "Live Pending Request TimeOut Extendor %u",
                         waitTime,
                         m_sequenceId,
                         m_livePendingRequests,
                         m_maxLivePendingRequests,
                         LivePendingRequestTimeOutExtendor);

        resultWait = m_pWaitLivePendingRequests->TimedWait(m_pLivePendingRequestsLock->GetNativeHandle(), waitTime);

        CAMX_LOG_CONFIG(CamxLogGroupCore,
                         "Timed Wait Live Pending Requests(%u) ...DONE resultWait %i",
                         waitTime,
                         resultWait);

        if (CamxResultSuccess != resultWait)
        {
            break;
        }
    }

    if (CamxResultSuccess != resultWait)
    {
        m_pLivePendingRequestsLock->Unlock();
        if (TRUE == pStaticSettings->raiserecoverysigabrt)
        {
            DumpSessionState(SessionDumpFlag::ResetRecovery);
            CAMX_LOG_ERROR(CamxLogGroupCore, "FATAL ERROR: Raise SigAbort to debug"
                                             " the root cause of HAL recovery");
            OsUtils::RaiseSignalAbort();
        }
        else
        {
            CAMX_LOG_CONFIG(CamxLogGroupCore, "Lets do a Reset:UMD");
            DumpSessionState(SessionDumpFlag::ResetUMD);
            return CamxResultETimeout;
        }
    }

    m_livePendingRequests++;
    m_pLivePendingRequestsLock->Unlock();

    // Block process request while stream on in progress
    m_pStreamOnOffLock->Lock();

    ChiCaptureRequest requests[MaxPipelinesPerSession];
    m_captureRequest.numRequests = numRequests;

    if (MaxRealTimePipelines > m_numRealtimePipelines)
    {
        // In single camera use case, one CHI request should have only one request per pipeline so that incoming requests will
        // not be more than m_requestQueueDepth and the only exception is in Dual Camera use case to have two requests
        if (2 <= numRequests)
        {
            CAMX_LOG_WARN(CamxLogGroupCore, "In batch mode, number of pipeline requests are more than 1");
        }

        CAMX_ASSERT(2 > numRequests);
    }

    SyncProcessCaptureRequest(pPipelineRequests, pipelineIndexes);

    for (UINT requestIndex = 0; requestIndex < numRequests; requestIndex++)
    {
        const ChiCaptureRequest* pCaptureRequest    = &(pPipelineRequests->pCaptureRequests[requestIndex]);
        UINT32             pipelinIndex             = pipelineIndexes[requestIndex];
        Pipeline*          pPipeline                = m_pipelineData[pipelinIndex].pPipeline;
        MetadataPool*      pPerFrameInputPool       = NULL;
        MetadataPool*      pPerFrameResultPool      = NULL;
        MetadataPool*      pPerFrameInternalPool    = NULL;
        MetadataPool*      pPerFrameEarlyResultPool = NULL;
        MetadataPool*      pPerUsecasePool          = NULL;
        MetaBuffer*        pInputMetabuffer         = NULL;
        MetaBuffer*        pOutputMetabuffer        = NULL;

        if ((NULL != pPipeline) && (PipelineStatus::FINALIZED > pPipeline->GetPipelineStatus()))
        {
            FinalizeDeferPipeline(pipelinIndex);
        }

        if (NULL != pPipeline)
        {
            pPerFrameInputPool       = pPipeline->GetPerFramePool(PoolType::PerFrameInput);
            pPerFrameResultPool      = pPipeline->GetPerFramePool(PoolType::PerFrameResult);
            pPerFrameInternalPool    = pPipeline->GetPerFramePool(PoolType::PerFrameInternal);
            pPerFrameEarlyResultPool = pPipeline->GetPerFramePool(PoolType::PerFrameResultEarly);
            pPerUsecasePool          = pPipeline->GetPerFramePool(PoolType::PerUsecase);
        }

        if ((NULL != pPerFrameEarlyResultPool) &&
            (NULL != pPerFrameInputPool)       &&
            (NULL != pPerFrameResultPool)      &&
            (NULL != pPerFrameInternalPool)    &&
            (NULL != pPerUsecasePool))
        {
            // Replace the incoming frameNumber with m_sequenceId to protect against sparse input frameNumbers
            CamX::Utils::Memcpy(&requests[requestIndex], pCaptureRequest, sizeof(ChiCaptureRequest));

            pInputMetabuffer  = reinterpret_cast<MetaBuffer*>(requests[requestIndex].pInputMetadata);
            pOutputMetabuffer = reinterpret_cast<MetaBuffer*>(requests[requestIndex].pOutputMetadata);

            requests[requestIndex].frameNumber = m_sequenceId;
            m_sequenceId++;

            result = CanRequestProceed(&requests[requestIndex]);

            if (CamxResultSuccess == result)
            {
                result = WaitOnAcquireFence(&requests[requestIndex]);

                if (CamxResultSuccess == result)
                {
                    // Finally copy and enqueue the request and fire the threadpool

                    // m_batchedFrameIndex of respective pipelines should be less than m_usecaseNumBatchedFrames
                    CAMX_ASSERT(m_batchedFrameIndex[pipelinIndex] < m_usecaseNumBatchedFrames);

                    // m_batchedFrameIndex 0 implies a new requestId must be generated - irrespective of batching ON/OFF status
                    if (0 == m_batchedFrameIndex[pipelinIndex])
                    {
                        m_requestBatchId[pipelinIndex]++;

                        CAMX_ASSERT(m_usecaseNumBatchedFrames >= m_captureRequest.requests[requestIndex].numBatchedFrames);
                        CaptureRequest::PartialClearData(&m_captureRequest.requests[requestIndex]);

                        m_captureRequest.requests[requestIndex].requestId = m_requestBatchId[pipelinIndex];
                        m_captureRequest.requests[requestIndex].pMultiRequestData =
                            &m_requestSyncData[(m_syncSequenceId) % MaxQueueDepth];
                        CAMX_LOG_VERBOSE(CamxLogGroupCore, "m_syncSequenceId:%d", m_syncSequenceId);
                        CAMX_LOG_VERBOSE(CamxLogGroupCore, "%s is handling RequestID:%llu whose PeerRequestID:%llu"
                            " m_syncSequenceId:%llu",
                            pPipeline->GetPipelineIdentifierString(),
                            m_captureRequest.requests[requestIndex].requestId,
                            m_requestSyncData[(m_syncSequenceId) % MaxQueueDepth],
                            m_syncSequenceId);

                        pPerFrameInputPool->Invalidate(m_requestBatchId[pipelinIndex]);
                        pPerFrameResultPool->Invalidate(m_requestBatchId[pipelinIndex]);
                        pPerFrameEarlyResultPool->Invalidate(m_requestBatchId[pipelinIndex]);
                        pPerFrameInternalPool->Invalidate(m_requestBatchId[pipelinIndex]);

                        pPerFrameInputPool->UpdateRequestId(m_requestBatchId[pipelinIndex]);
                        pPerFrameResultPool->UpdateRequestId(m_requestBatchId[pipelinIndex]);
                        pPerFrameEarlyResultPool->UpdateRequestId(m_requestBatchId[pipelinIndex]);
                        pPerFrameInternalPool->UpdateRequestId(m_requestBatchId[pipelinIndex]);
                        m_pPerFrameDebugDataPool->UpdateRequestId(m_requestBatchId[pipelinIndex]);

                        if (TRUE == pStaticSettings->logMetaEnable)
                        {
                            CAMX_LOG_META("+----------------------------------------------------");
                            CAMX_LOG_META("| Input metadata for request: %lld", m_requestBatchId[pipelinIndex]);
                            CAMX_LOG_META("|     %d entries", pInputMetabuffer->Count());
                            CAMX_LOG_META("+----------------------------------------------------");

                            pInputMetabuffer->PrintDetails();
                        }


                        MetadataSlot* pMetadataSlot       = pPerFrameInputPool->GetSlot(m_requestBatchId[pipelinIndex]);
                        MetadataSlot* pResultMetadataSlot = pPerFrameResultPool->GetSlot(m_requestBatchId[pipelinIndex]);
                        MetadataSlot* pUsecasePoolSlot    = pPerUsecasePool->GetSlot(0);

                        if (pMetadataSlot != NULL)
                        {
                            result = pMetadataSlot->AttachMetabuffer(pInputMetabuffer);

                            if (CamxResultSuccess == result)
                            {
                                UINT dumpMetadata = HwEnvironment::GetInstance()->GetStaticSettings()->dumpMetadata;

                                if (((TRUE  == pPipeline->IsRealTime()) && (0 != (dumpMetadata & 0x2))) ||
                                    ((FALSE == pPipeline->IsRealTime()) && (0 != (dumpMetadata & 0x1))))
                                {
                                    CHAR metadataFileName[FILENAME_MAX];

                                    OsUtils::SNPrintF(metadataFileName, FILENAME_MAX, "inputMetadata_%s_%d.txt",
                                                      pPipeline->GetPipelineIdentifierString(),
                                                      m_requestBatchId[pipelinIndex]);

                                    pInputMetabuffer->DumpDetailsToFile(metadataFileName);
                                }

                                result = pResultMetadataSlot->AttachMetabuffer(pOutputMetabuffer);

                                if (CamxResultSuccess != result)
                                {
                                    CAMX_LOG_ERROR(CamxLogGroupCore, "Error attach output failed for slot %d",
                                                   m_requestBatchId[pipelinIndex]);
                                }
                            }
                            else
                            {
                                CAMX_LOG_ERROR(CamxLogGroupCore, "Error attach input failed for slot %d",
                                               m_requestBatchId[pipelinIndex]);
                            }
                            CAMX_LOG_INFO(CamxLogGroupCore, "AttachMetabuffer in pipeline %s InputMetaBuffer %p "
                                                            "OutputMetaBuffer %p reqId %llu",
                                          pPipeline->GetPipelineIdentifierString(),
                                          pInputMetabuffer,
                                          pOutputMetabuffer,
                                          m_requestBatchId[pipelinIndex]);

                            if (CamxResultSuccess == result)
                            {

                                UINT64* pSensorExposureTime = static_cast<UINT64*>(pMetadataSlot->GetMetadataByTag(
                                                                               SensorExposureTime));

                                if (NULL != pSensorExposureTime)
                                {
                                    UINT64 exposureTimeInMs = (*pSensorExposureTime) / 1000000;
                                    if ( (exposureTimeInMs >
                                         (LivePendingRequestTimeoutDefault/m_maxLivePendingRequests)) &&
                                         (TRUE == m_isRealTime) &&
                                         (TRUE == pStaticSettings->extendedTimeForLongExposure))
                                    {
                                        m_latestLongExposureFrame = requests[requestIndex].frameNumber;
                                        if (exposureTimeInMs > m_longExposureTimeout)
                                        {
                                            m_longExposureTimeout = exposureTimeInMs;
                                        }
                                    }
                                }

                                // m_batchedFrameIndex of 0 implies batching may be switched ON/OFF starting from this frame
                                if (TRUE == IsUsecaseBatchingEnabled())
                                {
                                    RangeINT32* pFPSRange = static_cast<RangeINT32*>(pMetadataSlot->GetMetadataByTag(
                                                                                     ControlAETargetFpsRange));

                                    // Must have been filled by GetMetadataByTag()
                                    CAMX_ASSERT(NULL != pFPSRange);

                                    BOOL hasBatchingModeChanged = FALSE;

                                    if ((NULL != pFPSRange) && (pFPSRange->min == pFPSRange->max))
                                    {
                                        if (FALSE == m_isRequestBatchingOn)
                                        {
                                            hasBatchingModeChanged = TRUE;
                                        }

                                        m_isRequestBatchingOn = TRUE;
                                    }
                                    else
                                    {
                                        if (TRUE == m_isRequestBatchingOn)
                                        {
                                            hasBatchingModeChanged = TRUE;
                                        }

                                        m_isRequestBatchingOn = FALSE;
                                    }

                                    // If batching mode changes from ON to OFF or OFF to ON we need to dynamically adjust
                                    // m_requestQueueDepth - because m_requestQueueDepth is different with batching ON or OFF
                                    // With batching OFF it is RequestQueueDepth and with ON it is
                                    // "RequestQueueDepth * usecaseNumBatchedFrames"
                                    if (TRUE == hasBatchingModeChanged)
                                    {
                                        // Before changing m_requestQueueDepth, we need to make sure:
                                        // 1. All the current pending requests are processed by the Pipeline
                                        // 2. All the results for all those processed requests are sent back to the framework
                                        //
                                        // (1) is done by waiting for the request queue to become empty
                                        // (2) is done by waiting on a condition variable that is signaled when all results are
                                        //     sent back to the framework
                                        m_pRequestQueue->WaitEmpty();

                                        m_pLivePendingRequestsLock->Lock();
                                        m_livePendingRequests--;
                                        m_pLivePendingRequestsLock->Unlock();

                                        if (FALSE == WaitTillAllResultsAvailable())
                                        {
                                            CAMX_LOG_WARN(CamxLogGroupCore,
                                                          "Failed to drain on batching mode change, calling flush");
                                            Flush();
                                        }

                                        m_pLivePendingRequestsLock->Lock();
                                        m_livePendingRequests++;
                                        m_pLivePendingRequestsLock->Unlock();

                                        // The request and result queues are completely empty at this point, and this function
                                        // is the only thing that can add to the request queue.  Safe to change
                                        // m_requestQueueDepth at this point
                                        if (TRUE == m_isRequestBatchingOn)
                                        {
                                            m_requestQueueDepth      = DefaultRequestQueueDepth * m_usecaseNumBatchedFrames;
                                            m_maxLivePendingRequests =
                                                    m_defaultMaxLivePendingRequests * m_usecaseNumBatchedFrames;
                                        }
                                        else
                                        {
                                            m_requestQueueDepth      = DefaultRequestQueueDepth;
                                            m_maxLivePendingRequests = m_defaultMaxLivePendingRequests;
                                        }
                                    }
                                    else
                                    {
                                        // Need to set default value if batch mode is enabled but request batching is off.
                                        // In this case we have only preview reqest.
                                        if (FALSE == m_isRequestBatchingOn)
                                        {
                                            m_requestQueueDepth      = DefaultRequestQueueDepth;
                                            m_maxLivePendingRequests = m_defaultMaxLivePendingRequests;
                                        }
                                    }
                                }

                                if ((0 != m_recordingEndOfStreamTagId) && (0 != m_recordingEndOfStreamRequestIdTagId))
                                {
                                    UINT8* pRecordingEndOfStream = static_cast<UINT8*>(pMetadataSlot->GetMetadataByTag(
                                        m_recordingEndOfStreamTagId));

                                    if ((FALSE == pStaticSettings->disableDRQPreemptionOnStopRecord) &&
                                        ((NULL != pRecordingEndOfStream) && (0 != *pRecordingEndOfStream)))
                                    {
                                        UINT64 requestId = m_requestBatchId[pipelinIndex];
                                        CAMX_LOG_INFO(CamxLogGroupCore, "Recording stopped on reqId %llu", requestId);

                                        UINT32 endOfStreamRequestIdTag = m_recordingEndOfStreamRequestIdTagId;

                                        pUsecasePoolSlot->SetMetadataByTag(endOfStreamRequestIdTag,
                                                                           static_cast<VOID*>(&requestId),
                                                                           sizeof(requestId),
                                                                           "camx_session");

                                        pUsecasePoolSlot->PublishMetadataList(&endOfStreamRequestIdTag, 1);

                                        m_setVideoPerfModeFlag = TRUE;
                                        m_pDeferredRequestQueue->SetPreemptDependencyFlag(TRUE);
                                        m_pDeferredRequestQueue->DispatchReadyNodes();
                                    }
                                    else
                                    {
                                        m_setVideoPerfModeFlag = FALSE;
                                        m_pDeferredRequestQueue->SetPreemptDependencyFlag(FALSE);
                                    }
                                }
                                else
                                {
                                    CAMX_LOG_INFO(CamxLogGroupCore, "No stop recording vendor tags");
                                }

                                ControlCaptureIntentValues* pCaptureIntent = static_cast<ControlCaptureIntentValues*>(
                                    pMetadataSlot->GetMetadataByTag(ControlCaptureIntent));

                                // Update dynamic pipeline depth metadata which is required in capture result.
                                pResultMetadataSlot->SetMetadataByTag(RequestPipelineDepth,
                                                                      static_cast<VOID*>(&(m_requestQueueDepth)),
                                                                      1,
                                                                      "camx_session");

                                if (NULL != pCaptureIntent)
                                {
                                    // Copy Intent to result
                                    result = pResultMetadataSlot->SetMetadataByTag(ControlCaptureIntent, pCaptureIntent, 1,
                                                                                   "camx_session");
                                }
                            }
                            else
                            {
                                CAMX_LOG_ERROR(CamxLogGroupCore, "Couldn't copy request metadata!");
                            }
                        }
                        else
                        {
                            CAMX_LOG_ERROR(CamxLogGroupCore,
                                            "Couldn't get metadata slot for request id: %d",
                                            requests[requestIndex].frameNumber);

                            result = CamxResultEFailed;
                        }

                        // Get the per frame sensor mode index
                        UINT* pSensorModeIndex  = NULL;

                        if (m_vendorTagSensorModeIndex > 0)
                        {
                            if (NULL != pMetadataSlot)
                            {
                                pSensorModeIndex = static_cast<UINT*>(pMetadataSlot->GetMetadataByTag(
                                    m_vendorTagSensorModeIndex));
                            }

                            if (NULL != pSensorModeIndex)
                            {
                                pResultMetadataSlot->WriteLock();

                                pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

                                if (TRUE == pStaticSettings->perFrameSensorMode)
                                {
                                    pResultMetadataSlot->SetMetadataByTag(PropertyIDSensorCurrentMode, pSensorModeIndex, 1,
                                                                          "camx_session");
                                    pResultMetadataSlot->PublishMetadata(PropertyIDSensorCurrentMode);
                                }

                                pResultMetadataSlot->Unlock();
                            }
                        }

                        // Check and update, if the preview stream is present in this request
                        if (m_previewStreamPresentTagId > 0)
                        {
                            BOOL isPreviewPresent = FALSE;
                            for (UINT32 i = 0; i < pCaptureRequest->numOutputs; i++)
                            {
                                CHISTREAM*        pStream    = pCaptureRequest->pOutputBuffers[i].pStream;
                                ChiStreamWrapper* pChiStream = static_cast<ChiStreamWrapper*>(pStream->pPrivateInfo);
                                if ((NULL != pChiStream) && (TRUE == pChiStream->IsPreviewStream()))
                                {
                                    isPreviewPresent = TRUE;
                                    break;
                                }
                            }
                            // Update the metadata tag.
                            pResultMetadataSlot->WriteLock();
                            pResultMetadataSlot->SetMetadataByTag(m_previewStreamPresentTagId,
                                                                  static_cast<VOID*>(&(isPreviewPresent)),
                                                                  1,
                                                                  "camx_session");
                            pResultMetadataSlot->PublishMetadata(m_previewStreamPresentTagId);
                            pResultMetadataSlot->Unlock();
                        }

                    }

                    if (CamxResultSuccess == result)
                    {
                        ChiStreamWrapper*   pChiStreamWrapper   = NULL;
                        ChiStream*          pChiStream          = NULL;

                        /// Adding 1 to avoid 0 as 0 is flagged as invalid
                        UINT64 cslsyncid = pCaptureRequest->frameNumber + 1;
                        CaptureRequest* pRequest = &(m_captureRequest.requests[requestIndex]);
                        UINT batchedFrameIndex = m_batchedFrameIndex[pipelinIndex];

                        m_lastCSLSyncId = cslsyncid;

                        pRequest->pStreamBuffers[batchedFrameIndex].sequenceId =
                            static_cast<UINT32>(requests[requestIndex].frameNumber);
                        pRequest->pStreamBuffers[batchedFrameIndex].originalFrameworkNumber =
                            pCaptureRequest->frameNumber;
                        pRequest->CSLSyncID = cslsyncid;
                        pRequest->pStreamBuffers[batchedFrameIndex].numInputBuffers =
                            requests[requestIndex].numInputs;
                        pRequest->pPrivData = reinterpret_cast<CbPrivateData *>(pCaptureRequest->pPrivData);

                        for (UINT i = 0; i < requests[requestIndex].numInputs; i++)
                        {
                            /// @todo (CAMX-1015): Avoid this memcpy.
                            Utils::Memcpy(&pRequest->pStreamBuffers[batchedFrameIndex].inputBufferInfo[i].inputBuffer,
                                            &requests[requestIndex].pInputBuffers[i],
                                            sizeof(ChiStreamBuffer));
                            // Below check is ideally not required but to avoid
                            // regressions making it applicable to only MFNR/MFSR
                            if (requests[requestIndex].numInputs > 1)
                            {
                                pChiStream = reinterpret_cast<ChiStream*>(
                                    pRequest->pStreamBuffers[batchedFrameIndex].inputBufferInfo[i].inputBuffer.pStream);
                                CAMX_ASSERT(NULL != pChiStream);
                                pChiStreamWrapper = reinterpret_cast<ChiStreamWrapper*>(pChiStream->pPrivateInfo);

                                pRequest->pStreamBuffers[batchedFrameIndex].inputBufferInfo[i].portId =
                                    pChiStreamWrapper->GetPortId();
                                CAMX_LOG_VERBOSE(CamxLogGroupCore,
                                    "input buffers #%d, port %d, dim %d x %d wrapper %x, stream %x",
                                    i, pRequest->pStreamBuffers[batchedFrameIndex].inputBufferInfo[i].portId,
                                    pChiStream->width, pChiStream->height, pChiStreamWrapper, pChiStream);
                            }
                        }

                        if (CamxResultSuccess == result)
                        {
                            /// @todo (CAMX-1797) Delete this
                            pRequest->pipelineIndex    = pipelinIndex;

                            CAMX_LOG_VERBOSE(CamxLogGroupCore,
                                             "Submit to pipeline index: %d / number of pipelines: %d batched index %d",
                                             pRequest->pipelineIndex, m_numPipelines, m_batchedFrameIndex[pipelinIndex]);

                            CAMX_ASSERT(requests[requestIndex].numOutputs <= MaxOutputBuffers);

                            pRequest->pStreamBuffers[m_batchedFrameIndex[pipelinIndex]].numOutputBuffers =
                                requests[requestIndex].numOutputs;

                            for (UINT i = 0; i < requests[requestIndex].numOutputs; i++)
                            {
                                /// @todo (CAMX-1015): Avoid this memcpy.
                                Utils::Memcpy(&pRequest->pStreamBuffers[m_batchedFrameIndex[pipelinIndex]].outputBuffers[i],
                                              &requests[requestIndex].pOutputBuffers[i],
                                              sizeof(ChiStreamBuffer));
                            }

                            // Increment batch index only if batch mode is on
                            if (TRUE == m_isRequestBatchingOn)
                            {
                                m_batchedFrameIndex[pipelinIndex]++;
                                pRequest->numBatchedFrames = m_batchedFrameIndex[pipelinIndex];
                            }
                            else
                            {
                                m_batchedFrameIndex[pipelinIndex] = 0;
                                pRequest->numBatchedFrames = 1;
                            }

                        }
                    }

                    if (CamxResultSuccess == result)
                    {
                        // Fill Color Metadata for output buffer
                        result = SetPerStreamColorMetadata(pCaptureRequest, pPerFrameInputPool, m_requestBatchId[pipelinIndex]);
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupCore, "Acquire fence failed for request");
                }
            }
            else
            {
                CAMX_LOG_INFO(CamxLogGroupCore, "Session unable to process request because of device state");
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "PerFrame MetadataPool is NULL");
            result = CamxResultEInvalidPointer;
        }
    }

    // Update multi request sync data
    if (TRUE == IsMultiCamera())
    {
        UpdateMultiRequestSyncData(pPipelineRequests);
    }

    if (CamxResultSuccess == result)
    {
        // Once we batch all the frames according to usecaseNumBatchedFrames we enqueue the capture request.
        // For non-batch mode m_usecaseNumBatchedFrames is 1 so we enqueue every request. If batching is ON
        // we enqueue the batched capture request only after m_usecaseBatchSize number of requests have been
        // received
        BOOL batchFrameReady = TRUE;
        for (UINT requestIndex = 0; requestIndex < numRequests; requestIndex++)
        {
            UINT32 pipelinIndex = pipelineIndexes[requestIndex];

            if (m_batchedFrameIndex[pipelinIndex] != m_usecaseNumBatchedFrames)
            {
                batchFrameReady = FALSE;
                break; // batch frame number must be same for all the pipelines in same session
            }
        }

        if ((FALSE == m_isRequestBatchingOn) || (TRUE == batchFrameReady))
        {
            result = m_pRequestQueue->EnqueueWait(&m_captureRequest);

            if (CamxResultSuccess == result)
            {
                // Check for good conditions once more, if enqueue had to wait
                for (UINT requestIndex = 0; requestIndex < numRequests; requestIndex++)
                {
                    result = CanRequestProceed(&requests[requestIndex]);
                    if (CamxResultSuccess != result)
                    {
                        break;
                    }
                }
            }

            if (CamxResultSuccess == result)
            {
                for (UINT requestIndex = 0; requestIndex < numRequests; requestIndex++)
                {
                    const ChiCaptureRequest* pCaptureRequest = &(pPipelineRequests->pCaptureRequests[requestIndex]);
                    CAMX_LOG_INFO(CamxLogGroupCore,
                        "Pipeline: %s Added Sequence ID %lld CHI framenumber %lld to request queue and launched job "
                        "with request id %llu",
                        m_pipelineData[pipelineIndexes[requestIndex]].pPipeline->GetPipelineIdentifierString(),
                        requests[requestIndex].frameNumber, pCaptureRequest->frameNumber,
                        m_requestBatchId[pipelineIndexes[requestIndex]]);
                }

                VOID* pData[] = {this, NULL};
                result        = m_pThreadManager->PostJob(m_hJobFamilyHandle,
                                                          NULL,
                                                          &pData[0],
                                                          FALSE,
                                                          FALSE);
            }
            else
            {
                CAMX_LOG_WARN(CamxLogGroupCore, "Session unable to process request because of device state");
            }

            for (UINT requestIndex = 0; requestIndex < numRequests; requestIndex++)
            {
                m_batchedFrameIndex[pipelineIndexes[requestIndex]] = 0;
            }
        }
    }

    m_pStreamOnOffLock->Unlock();

    CAMX_ASSERT(CamxResultSuccess == result);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::ProcessResultEarlyMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::ProcessResultEarlyMetadata(
    ResultHolder* pResultHolder,
    UINT*         pNumResults)
{
    UINT currentResult = *pNumResults;

    BOOL metadataAvailable = FALSE;

    // Dispatch metadata error, and no more metadata will be delivered for this frame
    if ((0 != pResultHolder->pendingMetadataCount) && (NULL != pResultHolder->pMetadataError))
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Metadata error for request: %d", pResultHolder->sequenceId);
        pResultHolder->pMetadataError->pPrivData = static_cast<CHIPRIVDATA*>(pResultHolder->pPrivData);
        DispatchNotify(pResultHolder->pMetadataError);
        pResultHolder->pendingMetadataCount = 0;
    }

    // There was no metadata error, concatenate and send all metadata results together in FIFO order
    if ((NULL == pResultHolder->pMetadataError) && (TRUE == MetadataReadyToFly(pResultHolder->sequenceId)) &&
        (FALSE == pResultHolder->isCancelled))
    {
        // The early metadata is always in slot [2].
        if (NULL != pResultHolder->pMetaBuffer[2])
        {
            // This change will be overridden once partial metadata is handled
            m_pCaptureResult[currentResult].pOutputMetadata = pResultHolder->pMetaBuffer[2];

            pResultHolder->pendingMetadataCount--;

            m_pCaptureResult[currentResult].numPartialMetadata = 1;
            m_pCaptureResult[currentResult].frameworkFrameNum = GetFrameworkFrameNumber(pResultHolder->sequenceId);

            pResultHolder->pMetaBuffer[1] = NULL;
            metadataAvailable = TRUE;

            CAMX_LOG_INFO(CamxLogGroupHAL,
                          "Finalized early metadata result for Sequence ID %d mapped to framework id %d",
                          pResultHolder->sequenceId,
                          m_pCaptureResult[currentResult].frameworkFrameNum);
        }
    }

    return metadataAvailable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::ProcessResultMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::ProcessResultMetadata(
    ResultHolder* pResultHolder,
    UINT*         pNumResults)
{
    UINT currentResult = *pNumResults;

    BOOL metadataAvailable = FALSE;

    // Dispatch metadata error, and no more metadata will be delivered for this frame
    if ((0 != pResultHolder->pendingMetadataCount) && (NULL != pResultHolder->pMetadataError))
    {
        if (FALSE == m_aFlushStatus)
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL, "Metadata error for sequenceId: %d", pResultHolder->sequenceId);
        }
        else
        {
            CAMX_LOG_INFO(CamxLogGroupHAL, "Metadata NULL during flush for sequenceId: %d", pResultHolder->sequenceId);
            pResultHolder->pMetadataError->pPrivData = static_cast<CHIPRIVDATA*>(pResultHolder->pPrivData);
            DispatchNotify(pResultHolder->pMetadataError);
            pResultHolder->pendingMetadataCount = 0;
        }
    }

    // There was no metadata error, concatenate and send all metadata results together in FIFO order
    if ((NULL == pResultHolder->pMetadataError) && (TRUE == MetadataReadyToFly(pResultHolder->sequenceId)))
    {
        // The main metadata is always in slot [0] and [1].
        if ((NULL != pResultHolder->pMetaBuffer[0]) && (NULL != pResultHolder->pMetaBuffer[1]))
        {
            /// @todo (CAMX-271) - Handle more than one (>1) partial metadata in pipeline/HAL -
            ///                    When we handle metadata in pipeline, we need to decide how we
            ///                    want to break the slot metadata into multiple result metadata
            ///                    components, as per the contract in MaxPartialMetadataHAL
            ///                    (i.e. android.request.partialResultCount)

            m_pCaptureResult[currentResult].pInputMetadata  = pResultHolder->pMetaBuffer[0];
            m_pCaptureResult[currentResult].pOutputMetadata = pResultHolder->pMetaBuffer[1];

            pResultHolder->pendingMetadataCount--;

            m_pCaptureResult[currentResult].numPartialMetadata = m_numMetadataResults;
            m_pCaptureResult[currentResult].frameworkFrameNum = GetFrameworkFrameNumber(pResultHolder->sequenceId);

            pResultHolder->pMetaBuffer[0] = NULL;
            pResultHolder->pMetaBuffer[1] = NULL;
            metadataAvailable = TRUE;

            UINT32 queueIndex = pResultHolder->sequenceId % MaxQueueDepth;

            m_pendingMetabufferDoneQueue.pendingMask[queueIndex]  |= MetaBufferDoneMetaReady;
            m_pendingMetabufferDoneQueue.pipelineIndex[queueIndex] = pResultHolder->pipelineIndex;
            m_pendingMetabufferDoneQueue.requestId[queueIndex]     = pResultHolder->requestId;

            if (pResultHolder->sequenceId > m_pendingMetabufferDoneQueue.latestSequenceId)
            {
                m_pendingMetabufferDoneQueue.latestSequenceId = pResultHolder->sequenceId;
            }

            CAMX_LOG_INFO(CamxLogGroupHAL,
                          "Finalized metadata result for Sequence ID %d mapped to framework id %d pipeline %d reqId %u",
                          pResultHolder->sequenceId,
                          m_pCaptureResult[currentResult].frameworkFrameNum,
                          pResultHolder->pipelineIndex,
                          pResultHolder->requestId);
        }
    }

    return metadataAvailable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::ProcessResultBuffers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::ProcessResultBuffers(
    ResultHolder* pResultHolder,
    BOOL          metadataAvailable,
    UINT*         pNumResults)
{
    BOOL hasResult        = FALSE;
    UINT currentResult    = *pNumResults;
    BOOL metadataReady    = (TRUE == metadataAvailable) || (0 == pResultHolder->pendingMetadataCount);

    // Dispatch buffers with both OK and Error status
    UINT numBuffersOut = 0;
    for (UINT32 bufIndex = 0; bufIndex < MaxNumOutputBuffers; bufIndex++)
    {
        ResultHolder::BufferResult* pBufferResult = &pResultHolder->bufferHolder[bufIndex];

        if (NULL != pBufferResult->pBuffer)
        {
            const BOOL bufferReady = (TRUE == pBufferResult->valid) && (TRUE == metadataReady);
            const BOOL bufferError = (NULL != pBufferResult->pBufferError);

            /// @todo (CAMX-3119) remove IsTorchWidgetSession check below and handle this in generic way.
            if (TRUE == IsTorchWidgetSession())
            {
                // Invalidate the pStream for torch, as there is no actual buffer with torch.
                pBufferResult->pStream = NULL;
            }
            if ((TRUE == bufferReady) || (TRUE == bufferError))
            {
                if ((NULL  != pBufferResult->pStream) &&
                    (TRUE == BufferReadyToFly(pResultHolder->sequenceId, pBufferResult->pStream)))
                {
                    ChiCaptureResult* pResult = &m_pCaptureResult[currentResult];
                    ChiStreamWrapper* pChiStreamWrapper = static_cast<ChiStreamWrapper*>(pBufferResult->pStream->pPrivateInfo);

                    /// @todo (CAMX-1797) Rethink this way of keeping track of which is the next expected frame number
                    ///                   for the stream
                    pChiStreamWrapper->MoveToNextExpectedResultFrame();

                    ChiStreamBuffer* pStreamBuffer =
                        // NOWHINE CP036a: Google API requires const type
                        const_cast<ChiStreamBuffer*>(&pResult->pOutputBuffers[pResult->numOutputBuffers]);

                    Utils::Memcpy(pStreamBuffer, pBufferResult->pBuffer, sizeof(ChiStreamBuffer));

                    pResult->numOutputBuffers++;
                    hasResult = TRUE;

                    // Need to use this local since buffers may not all come back right away
                    numBuffersOut++;

                    if (TRUE == bufferError)
                    {
                        // Got a buffer in an error state so dispatch error

                        pBufferResult->error = TRUE;
                        pBufferResult->valid = FALSE;
                        pBufferResult->pBufferError->pPrivData = static_cast<CHIPRIVDATA*>(pResultHolder->pPrivData);
                        pBufferResult->pBuffer->bufferStatus   = BufferStatusError;

                        CAMX_LOG_INFO(CamxLogGroupCore, "Result buffer for sequence ID %u framenumber %u in error state.\n"
                                     "Result buffer %p\n"
                                     "     error:   %d\n"
                                     "     valid:   %d\n"
                                     "     buffer: %p\n"
                                     "          pStream:\n"
                                     "     Message: %p\n"
                                     "          messageType: %d\n"
                                     "          messageUnion: %p\n"
                                     "               errorMessageCode: %d\n"
                                     "               frameworkFrameNum: %u\n"
                                     "               pErrorStream: %p",
                                     pResultHolder->sequenceId,
                                     GetFrameworkFrameNumber(pResultHolder->sequenceId), pBufferResult,
                                     pBufferResult->error, pBufferResult->valid, pBufferResult->pBuffer,
                                     pBufferResult->pBuffer->pStream, pBufferResult->pBufferError,
                                     pBufferResult->pBufferError->messageType,
                                     pBufferResult->pBufferError->message,
                                     pBufferResult->pBufferError->message.errorMessage.errorMessageCode,
                                     pBufferResult->pBufferError->message.errorMessage.frameworkFrameNum,
                                     pBufferResult->pBufferError->message.errorMessage.pErrorStream);

                        CAMX_ASSERT(pBufferResult->pBufferError->message.errorMessage.errorMessageCode ==
                            ChiErrorMessageCode::MessageCodeBuffer);

                        DispatchNotify(pBufferResult->pBufferError);
                    }

                    // Invalidate the stream, will help in determining FIFO order for next result
                    pBufferResult->pStream = NULL;
                    pBufferResult->valid   = FALSE;

                    if (numBuffersOut == pResultHolder->numOutBuffers)
                    {
                        // We got the number of output buffers that we were expecting
                        // For the input buffer, we should release input buffer fence
                        // once we have all reporocess outputs buffers.
                        if ((0 != pResultHolder->numInBuffers) &&
                            (NULL != pResultHolder->inputbufferHolder[0].pBuffer))
                        {
                            pResult->pInputBuffer = pResultHolder->inputbufferHolder[0].pBuffer;

                            ChiStreamBuffer* pStreamInputBuffer =
                                // NOWHINE CP036a: Google API requires const type
                                const_cast<ChiStreamBuffer*>(pResult->pInputBuffer);

                            // Driver no longer owns this and app will take ownership
                            pStreamInputBuffer->releaseFence.valid = FALSE;
                        }
                        break;
                    }
                }
            }
        }
    }

    return hasResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::ProcessResults
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::ProcessResults()
{
    CamxResult              result                  = CamxResultSuccess;
    UINT32                  i                       = 0;
    UINT32                  numResults              = 0;
    ResultHolder*           pResultHolder           = NULL;
    SessionResultHolder*    pSessionResultHolder    = NULL;

    if (!GetFlushStatus())
    {
        result = m_pResultLock->TryLock();
        if (CamxResultSuccess != result)
        {
            // This could happen if InjectResult is called around the same time as the request/result processing thread
            // is running.  If it happens, skip result processing and try again
            CAMX_LOG_PERF_WARN(CamxLogGroupCore, "m_pResultLock failed, schedule this thread for reprocessing");
            CamxAtomicStore32(&m_aCheckResults, TRUE);
            return CamxResultSuccess;
        }
    }
    // Waiting in Flush status to avoid skip the last frame
    else
    {
        m_pResultLock->Lock();
    }


    // Reset the essential fields of framework results, so that not to be taken in stale
    for (UINT32 index = 0; index < (m_requestQueueDepth * m_numPipelines); index++)
    {
        m_pCaptureResult[index].frameworkFrameNum  = 0;
        m_pCaptureResult[index].numOutputBuffers   = 0;
        m_pCaptureResult[index].numPartialMetadata = 0;
        m_pCaptureResult[index].pInputMetadata     = NULL;
        m_pCaptureResult[index].pOutputMetadata    = NULL;
        m_pCaptureResult[index].pInputBuffer       = NULL;
        m_pCaptureResult[index].pResultMetadata    = NULL;
    }

    // m_pResultHolderListLock should only locks the code that updates the result holder and
    // it should be in the inner lock if it is used with other lock such as m_pResultLock, m_pFlushLock.
    m_pResultHolderListLock->Lock();

    LightweightDoublyLinkedListNode* pNode = m_resultHolderList.Head();

    while (NULL != pNode)
    {
        BOOL earlyMetadataReady    = FALSE;
        BOOL metadataReady         = FALSE;
        BOOL bufferReady           = FALSE;

        CAMX_ASSERT(NULL != pNode->pData);

        if (NULL != pNode->pData)
        {
            pSessionResultHolder = reinterpret_cast<SessionResultHolder*>(pNode->pData);

            for (UINT32 index = 0; index < pSessionResultHolder->numResults; index++)
            {
                pResultHolder = &(pSessionResultHolder->resultHolders[index]);
                if (NULL != pResultHolder)
                {
                    if (FALSE == pResultHolder->isCancelled)
                    {
                        // Only do partial metadata processing when the setting has defined the number of results greater than 1
                        if (1 < m_numMetadataResults)
                        {
                            earlyMetadataReady = ProcessResultEarlyMetadata(pResultHolder, &numResults);
                        }

                        // If we ever have early metadata for a given result before anything else. Stop processing the rest
                        // and just make sure we send back the early metadata.
                        if ((FALSE == earlyMetadataReady))
                        {
                            metadataReady = ProcessResultMetadata(pResultHolder, &numResults);
                            bufferReady   = ProcessResultBuffers(pResultHolder, metadataReady, &numResults);
                        }
                    }
                    else
                    {
                        // process results without regards to metadata if the request was cancelled
                        bufferReady = ProcessResultBuffers(pResultHolder, metadataReady, &numResults);
                    }

                    UINT totalBuffersSent =
                        (NULL != pResultHolder) ? pResultHolder->numErrorBuffersSent + pResultHolder->numOkBuffersSent : 0;

                    if ((TRUE == bufferReady) &&
                        (TRUE == m_isRealTime) &&
                        (pResultHolder->numOutBuffers == totalBuffersSent) &&
                        (m_latestLongExposureFrame == pResultHolder->sequenceId))
                    {
                        m_longExposureTimeout = 0;
                    }

                    CAMX_LOG_INFO(CamxLogGroupCore,
                        "Processing Result - SequenceId %u Framenumber %llu - earlyMetadata %d metadataReady %d "
                        "bufferReady %d isCancelled %d",
                        pResultHolder->sequenceId, GetFrameworkFrameNumber(pResultHolder->sequenceId), earlyMetadataReady,
                        metadataReady, bufferReady, pResultHolder->isCancelled);

                    if ((TRUE == metadataReady) || (TRUE == bufferReady) || (TRUE == earlyMetadataReady) ||
                        (TRUE == pResultHolder->isCancelled))
                    {
                        m_pCaptureResult[numResults].pPrivData         = static_cast<CHIPRIVDATA *>(pResultHolder->pPrivData);
                        m_pCaptureResult[numResults].frameworkFrameNum = GetFrameworkFrameNumber(pResultHolder->sequenceId);

                        numResults++;
                        bufferReady      = FALSE;
                        metadataReady    = FALSE;
                    }


                    if ((NULL != pResultHolder) &&
                        (pResultHolder->numOutBuffers == totalBuffersSent) && (pResultHolder->numErrorBuffersSent > 0) &&
                        (FALSE == pResultHolder->isCancelled) && (pResultHolder->pendingMetadataCount > 0))
                    {
                        CAMX_LOG_INFO(CamxLogGroupCore,
                            "RequestId: %llu SequenceId: %llu Framenumber: %llu - All results were injected with buffer error,"
                            " but an error notify has not been dispatched, dispatching now.",
                            pResultHolder->requestId, pResultHolder->sequenceId,
                            GetFrameworkFrameNumber(pResultHolder->sequenceId));

                        ChiMessageDescriptor* pNotify = GetNotifyMessageDescriptor();

                        pNotify->messageType                            = ChiMessageTypeError;
                        pNotify->pPrivData                              = static_cast<CHIPRIVDATA*>(pResultHolder->pPrivData);
                        pNotify->message.errorMessage.errorMessageCode  = static_cast<ChiErrorMessageCode>(MessageCodeResult);
                        pNotify->message.errorMessage.frameworkFrameNum = GetFrameworkFrameNumber(pResultHolder->sequenceId);
                        pNotify->message.errorMessage.pErrorStream      = NULL; // No stream applicable

                        DispatchNotify(pNotify);
                        pResultHolder->isCancelled          = TRUE;
                        pResultHolder->pendingMetadataCount = 0;

                    }

                    // If the request is complete, update the buffer to set processing end time
                    if ((NULL != pResultHolder) && (pResultHolder->numOutBuffers == totalBuffersSent) &&
                        (FALSE == pResultHolder->isCancelled) && (pResultHolder->pendingMetadataCount == 0))
                    {
                        UpdateSessionRequestTimingBuffer(pResultHolder);
                    }
                }
            }
        }

        // Get the next result holder and see what's going on with it
        pNode = m_resultHolderList.NextNode(pNode);
    }

    m_pResultHolderListLock->Unlock();

    if (0 < numResults)
    {
        // Finally dispatch all the results to the Framework
        DispatchResults(&m_pCaptureResult[0], numResults);
    }

    // Error results can result in the minimum being advanced without dispatching results
    AdvanceMinExpectedResult();

    // process metabuffers
    ProcessMetabufferPendingQueue();

    m_pResultLock->Unlock();

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::ProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::ProcessRequest()
{
    CamxResult              result          = CamxResultSuccess;
    SessionCaptureRequest*  pSessionRequest = NULL;

    // This should only ever be called from threadpool, should never be reentrant, and nothing else  grabs the request lock.
    // If there is contention on this lock something very bad happened.
    result = m_pRequestLock->TryLock();
    if (CamxResultSuccess != result)
    {
        // Should never happen...return control back to the threadpool and this will eventually get called again
        CAMX_LOG_ERROR(CamxLogGroupCore, "Could not grab m_pRequestLock...undefined behavior possible");

        return CamxResultETryAgain;
    }

    // Initialize a result holder expected for the result coming out of this request
    // This information will be used in the result notification path

    pSessionRequest = static_cast<SessionCaptureRequest*>(m_pRequestQueue->Dequeue());

    if (NULL != pSessionRequest)
    {
        // If session request contain multiple pipeline request, it means pipelines need to be sync
        // and the batch frame number must be same.
        UINT32 numBatchedFrames = pSessionRequest->requests[0].numBatchedFrames;
        for (UINT requestIndex = 1; requestIndex < pSessionRequest->numRequests; requestIndex++)
        {
            if (numBatchedFrames != pSessionRequest->requests[requestIndex].numBatchedFrames)
            {
                CAMX_LOG_ERROR(CamxLogGroupCore,
                    "batch frame number are different in different pipline request");
                m_pRequestLock->Unlock();
                return CamxResultEInvalidArg;
            }
        }

        const SettingsManager* pSettingManager = HwEnvironment::GetInstance()->GetSettingsManager();

        if (TRUE == pSettingManager->GetStaticSettings()->dynamicPropertiesEnabled)
        {
            // NOWHINE CP036a: We're actually poking into updating the settings dynamically so we do want to do this
            const_cast<SettingsManager*>(pSettingManager)->UpdateOverrideProperties();
        }

        LightweightDoublyLinkedListNode** ppResultNodes         = NULL;
        SessionResultHolder**             ppSessionResultHolder = NULL;

        for (UINT requestIndex = 0; requestIndex < pSessionRequest->numRequests; requestIndex++)
        {
            CaptureRequest* pRequest = &(pSessionRequest->requests[requestIndex]);
            CAMX_ASSERT(pRequest->numBatchedFrames > 0);

            if (NULL == ppResultNodes)
            {
                ppResultNodes = reinterpret_cast<LightweightDoublyLinkedListNode**>(
                    CAMX_CALLOC(numBatchedFrames * sizeof(LightweightDoublyLinkedListNode*)));

                if (NULL == ppResultNodes)
                {
                    CAMX_LOG_ERROR(CamxLogGroupCore, "memory allocation failed for ppResultNodes for request %llu",
                        pRequest->requestId);
                    result = CamxResultENoMemory;
                    break;
                }
            }

            if (NULL == ppSessionResultHolder)
            {
                ppSessionResultHolder = reinterpret_cast<SessionResultHolder**>(
                    CAMX_CALLOC(numBatchedFrames * sizeof(SessionResultHolder*)));
                if (NULL == ppSessionResultHolder)
                {
                    CAMX_LOG_ERROR(CamxLogGroupCore, "memory allocation failed for ppSessionResultHolder for request %llu",
                        pRequest->requestId);
                    result = CamxResultENoMemory;
                    break;
                }
            }

            if ((NULL != ppResultNodes) && (NULL != ppSessionResultHolder))
            {
                // Add sequence id to framework frame number mapping after CheckRequestProcessingRate() = TRUE
                // This is to make sure new process request do not override old result has not sent to framework yet.
                for (UINT32 batchIndex = 0; batchIndex < pRequest->numBatchedFrames; batchIndex++)
                {
                    UINT32 sequenceId = pRequest->pStreamBuffers[batchIndex].sequenceId;

                    m_fwFrameNumberMap[sequenceId % MaxQueueDepth] =
                        pRequest->pStreamBuffers[batchIndex].originalFrameworkNumber;

                    CAMX_LOG_REQMAP(CamxLogGroupCore,
                                    "chiFrameNum: %llu  <==>  requestId: %llu  <==>  sequenceId: %u  <==> CSLSyncId: %llu "
                                    " -- %s",
                                    pRequest->pStreamBuffers[batchIndex].originalFrameworkNumber,
                                    pRequest->requestId,
                                    pRequest->pStreamBuffers[batchIndex].sequenceId,
                                    pRequest->CSLSyncID,
                                    m_pipelineData[pRequest->pipelineIndex].pPipeline->GetPipelineIdentifierString());
                }

                for (UINT batchIndex = 0; batchIndex < pRequest->numBatchedFrames; batchIndex++)
                {
                    UINT32 sequenceId = pRequest->pStreamBuffers[batchIndex].sequenceId;

                    CAMX_TRACE_MESSAGE_F(CamxLogGroupCore, "ProcessRequest: RequestId: %llu sequenceId: %u",
                        pRequest->requestId, sequenceId);

                    LightweightDoublyLinkedListNode* pNode = ppResultNodes[batchIndex];
                    if (NULL == pNode)
                    {
                        pNode = reinterpret_cast<LightweightDoublyLinkedListNode*>
                            (CAMX_CALLOC(sizeof(LightweightDoublyLinkedListNode)));
                        ppResultNodes[batchIndex] = pNode;
                    }

                    SessionResultHolder* pSessionResultHolder = ppSessionResultHolder[batchIndex];
                    if (NULL == pSessionResultHolder)
                    {
                        pSessionResultHolder = reinterpret_cast<SessionResultHolder*>
                            (CAMX_CALLOC(sizeof(SessionResultHolder)));
                        ppSessionResultHolder[batchIndex] = pSessionResultHolder;
                    }

                    if ((NULL == pNode) ||
                        (NULL == pSessionResultHolder))
                    {
                        CAMX_LOG_ERROR(CamxLogGroupCore, "Out of memory pNode=%p pSessionResultHolder=%p",
                            pNode, pSessionResultHolder);
                        result = CamxResultENoMemory;

                        if (NULL != pNode)
                        {
                            CAMX_FREE(pNode);
                            pNode = NULL;
                        }

                        if (NULL != pSessionResultHolder)
                        {
                            CAMX_FREE(pSessionResultHolder);
                            pSessionResultHolder = NULL;
                        }
                    }

                    if (CamxResultSuccess == result)
                    {
                        ResultHolder* pHolder = &(pSessionResultHolder->resultHolders[requestIndex]);
                        Utils::Memset(pHolder, 0x0, sizeof(ResultHolder));
                        pHolder->sequenceId           = sequenceId;
                        pHolder->numOutBuffers        = pRequest->pStreamBuffers[batchIndex].numOutputBuffers;
                        pHolder->numInBuffers         = pRequest->pStreamBuffers[batchIndex].numInputBuffers;
                        pHolder->pendingMetadataCount = m_numMetadataResults;
                        pHolder->pPrivData            = pRequest->pPrivData;
                        pHolder->requestId            = static_cast<UINT32>(pRequest->requestId);

                        // We may not get a result metadata for reprocess requests
                        // This logic may need to be expanded for multi-camera CHI override scenarios,
                        // as to designate what pipelines are exactly offline
                        if (pRequest->pipelineIndex > 0)
                        {
                            pHolder->tentativeMetadata = TRUE;
                        }

                        for (UINT32 buffer = 0; buffer < pHolder->numOutBuffers; buffer++)
                        {
                            UINT32 streamIndex = GetStreamIndex(reinterpret_cast<ChiStream*>(
                                pRequest->pStreamBuffers[batchIndex].outputBuffers[buffer].pStream));

                            if (streamIndex < MaxNumOutputBuffers)
                            {
                                pHolder->bufferHolder[streamIndex].pBuffer = GetResultStreamBuffer();

                                Utils::Memcpy(pHolder->bufferHolder[streamIndex].pBuffer,
                                              &(pRequest->pStreamBuffers[batchIndex].outputBuffers[buffer]),
                                              sizeof(ChiStreamBuffer));

                                pHolder->bufferHolder[streamIndex].valid = FALSE;

                                pHolder->bufferHolder[streamIndex].pStream = reinterpret_cast<ChiStream*>(
                                    pRequest->pStreamBuffers[batchIndex].outputBuffers[buffer].pStream);

                                ChiStreamWrapper* pChiStreamWrapper = static_cast<ChiStreamWrapper*>(
                                    pRequest->pStreamBuffers[batchIndex].outputBuffers[buffer].pStream->pPrivateInfo);

                                pChiStreamWrapper->AddEnabledInFrame(pRequest->pStreamBuffers[batchIndex].sequenceId);
                            }
                            else
                            {
                                CAMX_LOG_ERROR(CamxLogGroupCore, "stream index = %d < MaxNumOutputBuffers = %d",
                                               streamIndex, MaxNumOutputBuffers);
                            }
                        }

                        // Create internal private input buffer fences and relase them (below), so that
                        // input fence trigger mechanism would work same way that when the input fences
                        // released from previous/parent node output buffer

                        for (UINT32 buffer = 0; buffer < pHolder->numInBuffers; buffer++)
                        {
                            StreamInputBufferInfo* pInputBufferInfo   = pRequest->pStreamBuffers[batchIndex].inputBufferInfo;
                            ChiStreamBuffer*       pInputBuffer       = &(pInputBufferInfo[buffer].inputBuffer);
                            CSLFence*              phCSLFence         = NULL;
                            CHIFENCEHANDLE*        phAcquireFence     = NULL;
                            UINT32                 streamIndex        = 0;
                            ChiStream*             pInputBufferStream = reinterpret_cast<ChiStream*>(pInputBuffer->pStream);

                            /// @todo (CAMX-1797) Kernel currently requires us to pass a fence always even if we dont need it.
                            ///                   Fix that and also need to handle input fence mechanism
                            phCSLFence = &(pInputBufferInfo[buffer].fence);

                            if ((TRUE == pInputBuffer->acquireFence.valid) &&
                                (ChiFenceTypeInternal == pInputBuffer->acquireFence.type))
                            {
                                phAcquireFence = &(pInputBuffer->acquireFence.hChiFence);
                            }

                            if (FALSE == IsValidCHIFence(phAcquireFence))
                            {
                                result = CSLCreatePrivateFence("InputBufferFence_session", phCSLFence);
                                CAMX_ASSERT(CamxResultSuccess == result);

                                if (CamxResultSuccess != result)
                                {
                                    CAMX_LOG_ERROR(CamxLogGroupCore, "process request failed : result %d", result);
                                    break;
                                }
                                else
                                {
                                    CAMX_LOG_VERBOSE(CamxLogGroupCore, "CSLCreatePrivateFence:%d Used", *phCSLFence);
                                }
                                pInputBufferInfo[buffer].isChiFence = FALSE;
                            }
                            else
                            {
                                *phCSLFence = reinterpret_cast<ChiFence*>(*phAcquireFence)->hFence;
                                pInputBufferInfo[buffer].isChiFence = TRUE;
                                CAMX_LOG_VERBOSE(CamxLogGroupCore, "AcquireFence:%d Used", *phCSLFence);
                            }

                            streamIndex = GetStreamIndex(pInputBufferStream);
                            if (streamIndex < MaxNumInputBuffers)
                            {
                                ChiStreamBuffer* pChiResultStreamBuffer = GetResultStreamBuffer();

                                Utils::Memcpy(pChiResultStreamBuffer, pInputBuffer, sizeof(ChiStreamBuffer));
                                pHolder->inputbufferHolder[streamIndex].pBuffer = pChiResultStreamBuffer;
                                pHolder->inputbufferHolder[streamIndex].pStream = pInputBufferStream;
                            }
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        if ((NULL != ppResultNodes) && (NULL != ppSessionResultHolder))
        {
            // Now add the result holder to the linked list
            for (UINT batchIndex = 0; batchIndex < numBatchedFrames; batchIndex++)
            {
                LightweightDoublyLinkedListNode* pNode                  = ppResultNodes[batchIndex];
                SessionResultHolder*             pSessionResultHolder   = ppSessionResultHolder[batchIndex];
                pSessionResultHolder->numResults = pSessionRequest->numRequests;
                pNode->pData = pSessionResultHolder;
                m_pResultLock->Lock();
                m_resultHolderList.InsertToTail(pNode);
                m_pResultLock->Unlock();
            }
        }

        // De-acllocate the array of ppResultNodes and ppSessionResultHolder.
        // The actual node and session result holder will be free in processResult
        if (NULL != ppResultNodes)
        {
            CAMX_FREE(ppResultNodes);
            ppResultNodes = NULL;
        }
        if (NULL != ppSessionResultHolder)
        {
            CAMX_FREE(ppSessionResultHolder);
            ppSessionResultHolder = NULL;
        }
    }
    m_pRequestLock->Unlock();

    if (NULL != pSessionRequest)
    {
        if ((m_numRealtimePipelines > 1) && (pSessionRequest->numRequests > 1))
        {
            CheckAndSyncLinks(pSessionRequest);
        }

        BOOL isSyncMode = TRUE;

        if (pSessionRequest->numRequests <= 1)
        {
            isSyncMode = FALSE;
        }

        for (UINT requestIndex = 0; requestIndex < pSessionRequest->numRequests; requestIndex++)
        {
            CaptureRequest* pRequest = &(pSessionRequest->requests[requestIndex]);

            result = m_pipelineData[pRequest->pipelineIndex].pPipeline->OpenRequest(pRequest->requestId,
                pRequest->CSLSyncID, isSyncMode);

            CAMX_LOG_INFO(CamxLogGroupCore,
                "pipeline[%d] OpenRequest called for request id = %llu withCSLSyncID %llu",
                pRequest->pipelineIndex,
                pRequest->requestId,
                pRequest->CSLSyncID);

            if ((CamxResultSuccess != result) && (CamxResultECancelledRequest != result))
            {
                CAMX_LOG_ERROR(CamxLogGroupCore,
                    "pipeline[%d] OpenRequest failed for request id = %llu withCSLSyncID %llu result = %s",
                    pRequest->pipelineIndex,
                    pRequest->requestId,
                    pRequest->CSLSyncID,
                    Utils::CamxResultToString(result));
            }
            else if (CamxResultECancelledRequest == result)
            {
                CAMX_LOG_INFO(CamxLogGroupCore, "Session: %p is in Flush state, Canceling OpenRequest for pipeline[%d] "
                    "for request id = %llu", this, pRequest->pipelineIndex, pRequest->requestId);

                result = CamxResultSuccess;
            }
        }

        if (CamxResultSuccess == result)
        {
            for (UINT requestIndex = 0; requestIndex < pSessionRequest->numRequests; requestIndex++)
            {
                CaptureRequest*            pRequest                   = &(pSessionRequest->requests[requestIndex]);
                PipelineProcessRequestData pipelineProcessRequestData = {};

                result = SetupRequestData(pRequest, &pipelineProcessRequestData);

                // Set timestamp for start of request processing
                PopulateSessionRequestTimingBuffer(pRequest);

                if (CamxResultSuccess == result)
                {
                    result = m_pipelineData[pRequest->pipelineIndex].pPipeline->ProcessRequest(&pipelineProcessRequestData);
                }

                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupCore, "pipeline[%d] ProcessRequest failed for request %llu - %s",
                                   pRequest->pipelineIndex,
                                   pRequest->requestId,
                                   Utils::CamxResultToString(result));
                }

                if (NULL != pipelineProcessRequestData.pPerBatchedFrameInfo)
                {
                    CAMX_FREE(pipelineProcessRequestData.pPerBatchedFrameInfo);
                    pipelineProcessRequestData.pPerBatchedFrameInfo = NULL;
                }
            }
        }
        m_pRequestQueue->Release(pSessionRequest);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::PopulateSessionRequestTimingBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::PopulateSessionRequestTimingBuffer(
    CaptureRequest* pRequest)
{
    const StaticSettings* pStaticSettings = m_pChiContext->GetStaticSettings();

    if (TRUE == pStaticSettings->dumpSessionProcessingInfo)
    {
        CamxTime                pTime;
        PerResultHolderInfo*    pRequestInfo = GetSessionRequestData(pRequest->requestId, FALSE);

        OsUtils::GetTime(&pTime);
        pRequestInfo->startTime = OsUtils::CamxTimeToMillis(&pTime);
        pRequestInfo->requestId = pRequest->requestId;
        pRequestInfo->CSLSyncId = pRequest->CSLSyncID;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::UpdateSessionRequestTimingBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::UpdateSessionRequestTimingBuffer(
    ResultHolder* pResultHolder)
{
    const StaticSettings* pStaticSettings = m_pChiContext->GetStaticSettings();

    if (TRUE == pStaticSettings->dumpSessionProcessingInfo)
    {
        CamxTime                pTime;
        PerResultHolderInfo*    pRequestInfo = GetSessionRequestData(pResultHolder->requestId, TRUE);

        OsUtils::GetTime(&pTime);
        pRequestInfo->endTime = OsUtils::CamxTimeToMillis(&pTime);

        // Dump if we have populated the entire buffer
        if (0 == (pResultHolder->requestId % MaxSessionPerRequestInfo))
        {
            DumpSessionRequestProcessingTime();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::DumpSessionRequestProcessingTime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::DumpSessionRequestProcessingTime()
{
    UINT   latestSessionRequestIndex    = m_perRequestInfo.lastSessionRequestIndex;
    UINT32 count                        = MaxSessionPerRequestInfo;
    UINT32 minProcessingTime            = UINT32_MAX;
    UINT32 maxProcessingTime            = 0;
    UINT32 currProcessingTime           = 0;
    UINT32 averageProcessingTime        = 0;
    UINT32 divisor                      = MaxSessionPerRequestInfo;

    const StaticSettings* pStaticSettings = m_pChiContext->GetStaticSettings();

    if (TRUE == pStaticSettings->dumpSessionProcessingInfo)
    {
        CAMX_LOG_INFO(CamxLogGroupCore, "+------------------------------------------------------------------+");
        CAMX_LOG_INFO(CamxLogGroupCore, "+       DUMPING SESSION:%p REQUEST PROCESSING TIMES      +", this);

        while (count)
        {
            count--;

            // Check if session request entry is valid
            if ((0 != m_perRequestInfo.requestInfo[latestSessionRequestIndex].requestId)    &&
                (0 != m_perRequestInfo.requestInfo[latestSessionRequestIndex].endTime)      &&
                (0 != m_perRequestInfo.requestInfo[latestSessionRequestIndex].startTime)    &&
                (m_perRequestInfo.requestInfo[latestSessionRequestIndex].endTime            >=
                 m_perRequestInfo.requestInfo[latestSessionRequestIndex].startTime))
            {
                currProcessingTime      = (m_perRequestInfo.requestInfo[latestSessionRequestIndex].endTime -
                                            m_perRequestInfo.requestInfo[latestSessionRequestIndex].startTime);
                averageProcessingTime   += currProcessingTime;
                maxProcessingTime       = Utils::MaxUINT32(maxProcessingTime, currProcessingTime);
                minProcessingTime       = Utils::MinUINT32(minProcessingTime, currProcessingTime);

                CAMX_LOG_INFO(CamxLogGroupCore, "+   ReqId[%llu], SyncId[%llu], Start[%d], End[%d], Total[%d ms]",
                              m_perRequestInfo.requestInfo[latestSessionRequestIndex].requestId,
                              m_perRequestInfo.requestInfo[latestSessionRequestIndex].CSLSyncId,
                              m_perRequestInfo.requestInfo[latestSessionRequestIndex].startTime,
                              m_perRequestInfo.requestInfo[latestSessionRequestIndex].endTime,
                              currProcessingTime);
            }
            else
            {
                // If data wasn't valid, adjust how we calculate the average
                divisor--;
            }

            // Update the running index to reset correctly
            if (0 == latestSessionRequestIndex)
            {
                latestSessionRequestIndex = (MaxSessionPerRequestInfo - 1);
            }
            else
            {
                latestSessionRequestIndex--;
            }
        }

        averageProcessingTime = Utils::DivideAndCeil(averageProcessingTime, divisor);

        CAMX_LOG_INFO(CamxLogGroupCore, "+            AVG[%d ms],   MIN[%d ms],   MAX[%d ms]             +",
                      averageProcessingTime, minProcessingTime, maxProcessingTime);
        CAMX_LOG_INFO(CamxLogGroupCore, "+------------------------------------------------------------------+");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::HandleErrorCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::HandleErrorCb(
    CbPayloadError* pError,
    UINT            pipelineIndex,
    VOID*           pPrivData)
{
    // This should contain the number of output buffers and also the ChiMesssageDescriptor
    VOID*                 pPayloads[MaxPipelineOutputs+1] = {0};
    ChiMessageDescriptor* pNotify                         = GetNotifyMessageDescriptor();
    UINT                  streamId                        = 0;
    PipelineOutputData*   pPipelineOutputData             = NULL;
    ChiStreamBuffer*      pOutBuffer                      = NULL;

    CAMX_ASSERT(NULL != pNotify);
    CAMX_ASSERT(NULL != pError);

    pNotify->pPrivData                              = static_cast<CHIPRIVDATA*>(pPrivData);
    pNotify->messageType                            = ChiMessageTypeError;
    pNotify->message.errorMessage.frameworkFrameNum = GetFrameworkFrameNumber(pError->sequenceId);
    pNotify->message.errorMessage.errorMessageCode  = static_cast<ChiErrorMessageCode>(pError->code);

    switch (pError->code)
    {
        case MessageCodeDevice:
            /// @todo (CAMX-3266) Finalize error fence callbacks for on the flight results, we would depend on flush
            ///                implementation under normal conditions.
            ///                Yet, a device in error state might not be responsive; decide how to handle
            CAMX_LOG_ERROR(CamxLogGroupCore, "Device is in error condition!");
            // Set error state (block processing of capture requests and results), block Callbacks including torch
            // notify that would cause a segfault if device is resetting, etc.
            SetDeviceInError(TRUE);
            HAL3Module::GetInstance()->SetDropCallbacks();

            // Signal wait condition; Device error will cause camera to shut down, if we are waiting on this condition
            // after the close, we will hit NULL pointer when signaling destroyed condition after timed wait
            CAMX_LOG_CONFIG(CamxLogGroupCore, "Signaling m_pWaitLivePendingRequests condition");
            m_pWaitLivePendingRequests->Signal();

            pNotify->message.errorMessage.pErrorStream = NULL; // No stream applicable

            // Dispatch it immediately
            DispatchNotify(pNotify);

            // Trigger if caller was blocked on ProcessCaptureRequest
            m_pRequestQueue->CancelWait();

            // We expect close to be called at this point
            break;

        case MessageCodeRequest:
            // Dispatch it immediately

            pNotify->message.errorMessage.pErrorStream = NULL; // No stream applicable
            DispatchNotify(pNotify);

            pPayloads[0] = pNotify;
            InjectResult(ResultType::RequestError, pPayloads, pError->sequenceId, NULL, pipelineIndex);
            break;

        case MessageCodeResult:
            pNotify->message.errorMessage.pErrorStream = NULL; // No stream applicable

            // Notification will be dispatched along with other results
            InjectResult(ResultType::MetadataError, pNotify, pError->sequenceId, pPrivData, pipelineIndex);
            break;

        case MessageCodeBuffer:
            pOutBuffer = GetResultStreamBuffer();

            CAMX_ASSERT(NULL != pOutBuffer);

            streamId            = pError->streamId;
            pPipelineOutputData = &m_pipelineData[pipelineIndex].pPipelineDescriptor->outputData[streamId];

            pOutBuffer->size                = sizeof(ChiStreamBuffer);
            ///< @todo (CAMX-4113) Decouple CHISTREAM and camera3_stream
            pOutBuffer->pStream             =
                reinterpret_cast<ChiStream*>(pPipelineOutputData->pOutputStreamWrapper->GetNativeStream());
            pOutBuffer->bufferInfo          = pError->bufferInfo;
            pOutBuffer->bufferStatus        = BufferStatusError;

            if ((TRUE == isVideoStream(pPipelineOutputData->pOutputStreamWrapper->GetGrallocUsage())) &&
                (TRUE == CamX::IsGrallocBuffer(&pError->bufferInfo)))
            {
                BufferHandle* phNativeHandle = CamX::GetBufferHandleFromBufferInfo(&(pError->bufferInfo));

                if (NULL != phNativeHandle)
                {
                    SetPerFrameVTTimestampMetadata(*phNativeHandle,
                        GetIntraPipelinePerFramePool(PoolType::PerFrameResult, pipelineIndex),
                        pError->sequenceId + 1); // request ID starts from 1
                }
            }

            pNotify->message.errorMessage.pErrorStream =
                reinterpret_cast<ChiStream*>(pPipelineOutputData->pOutputStreamWrapper->GetNativeStream());

            pPayloads[0] = pOutBuffer;
            pPayloads[1] = pNotify;
            // Notification will be dispatched along with other results
            InjectResult(ResultType::BufferError, pPayloads, pError->sequenceId, pPrivData, pipelineIndex);
            break;
        case MessageCodeRecovery:
            // If we aren't flushing (aka draining) accept a new request
            if (FALSE == CamxAtomicLoadU8(&m_aFlushingPipeline))
            {
                if (TRUE == m_pChiContext->GetStaticSettings()->raiserecoverysigabrt)
                {
                    DumpSessionState(SessionDumpFlag::ResetRecovery);
                    CAMX_LOG_ERROR(CamxLogGroupCore, "FATAL ERROR: Raise SigAbort to debug"
                                                     " the root cause of UMD recovery");
                    OsUtils::RaiseSignalAbort();
                }
                else
                {
                    // Tell HAL queue we are recovering to handle enqueuing new requests properly during recovery
                    m_pRequestQueue->SetRecoveryStatus(TRUE);

                    // Do not hold onto wait condition
                    CAMX_LOG_CONFIG(CamxLogGroupCore, "Signaling m_pWaitLivePendingRequests condition");
                    m_pWaitLivePendingRequests->Signal();

                    CAMX_LOG_CONFIG(CamxLogGroupCore, "Sending recovery for frameworkFrameNum=%d",
                                    pNotify->message.errorMessage.frameworkFrameNum);
                    DumpSessionState(SessionDumpFlag::ResetRecovery);
                    DispatchNotify(pNotify);

                    // Trigger if caller was blocked on ProcessCaptureRequest
                    m_pRequestQueue->CancelWait();
                }
            }

            break;

        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::HandleAsyncCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::HandleAsyncCb(
    CbPayloadAsync* pAsync,
    VOID*           pPrivData)
{
    ChiMessageDescriptor* pNotify = GetNotifyMessageDescriptor();

    CAMX_ASSERT(NULL != pNotify);

    // We have to assume that any async callback has to be shutter message
    pNotify->messageType                              = ChiMessageTypeShutter;
    pNotify->message.shutterMessage.frameworkFrameNum = GetFrameworkFrameNumber(pAsync->sequenceId);
    pNotify->message.shutterMessage.timestamp         = pAsync->timestamp;
    pNotify->pPrivData                                = static_cast<CHIPRIVDATA *>(pPrivData);

    // Dispatch it immediately
    DispatchNotify(pNotify);

    // Mark the we sent out the shutter
    ResultHolder* pResHolder     = GetResultHolderBySequenceId(pAsync->sequenceId);
    if (NULL != pResHolder)
    {
        pResHolder->isShutterSentOut = TRUE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::HandleSOFCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::HandleSOFCb(
    CbPayloadSof* pSof)
{
    ChiMessageDescriptor* pNotify = GetNotifyMessageDescriptor();

    CAMX_ASSERT(NULL != pNotify);
    CAMX_ASSERT(NULL != pSof);

    // Send SOF event as a shutter message
    pNotify->messageType                                   = ChiMessageTypeSof;
    pNotify->message.sofMessage.timestamp                  = pSof->timestamp;
    pNotify->message.sofMessage.sofId                      = pSof->frameNum;
    pNotify->message.sofMessage.bIsFrameworkFrameNumValid  = pSof->bIsSequenceIdValid;
    if (pSof->bIsSequenceIdValid)
    {
        pNotify->message.sofMessage.frameworkFrameNum = GetFrameworkFrameNumber(pSof->sequenceId);
    }

    // Dispatch it immediately
    DispatchNotify(pNotify);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::HandleMetaBufferDoneCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::HandleMetaBufferDoneCb(
    CbPayloadMetaBufferDone* pMetaBufferCbPayload)
{
    ChiMessageDescriptor notify = { };

    if ((NULL != pMetaBufferCbPayload->pInputMetabuffer) ||
         (NULL != pMetaBufferCbPayload->pOutputMetabuffer))
    {
        // Send MetaBufferDone event
        notify.messageType                                     = ChiMessageTypeMetaBufferDone;
        notify.message.metaBufferDoneMessage.inputMetabuffer   = pMetaBufferCbPayload->pInputMetabuffer;
        notify.message.metaBufferDoneMessage.outputMetabuffer  = pMetaBufferCbPayload->pOutputMetabuffer;
        notify.message.metaBufferDoneMessage.frameworkFrameNum = GetFrameworkFrameNumber(pMetaBufferCbPayload->sequenceId);

        // Dispatch it immediately
        DispatchNotify(&notify);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot issue metabuffer done callback input %p output %p",
                      pMetaBufferCbPayload->pInputMetabuffer,
                      pMetaBufferCbPayload->pOutputMetabuffer);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::HandleMetadataCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::HandleMetadataCb(
    CbPayloadMetadata* pPayload,
    VOID*              pPrivData,
    UINT32             pipelineIndex)
{
    // We will use the pMetadata->pMetadata all the way upto the framework
    InjectResult(ResultType::MetadataOK, &pPayload->metaPayload, pPayload->sequenceId, pPrivData, pipelineIndex);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Session::HandlePartialMetadataCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::HandlePartialMetadataCb(
    CbPayloadPartialMetadata* pPayload,
    VOID*                     pPrivData,
    UINT32                    pipelineIndex)
{
    CAMX_UNREFERENCED_PARAM(pipelineIndex);

    ChiPartialCaptureResult* pPartialCaptureResult  = GetPartialMetadataMessageDescriptor();
    pPartialCaptureResult->frameworkFrameNum        = GetFrameworkFrameNumber(pPayload->sequenceId);
    pPartialCaptureResult->pPartialResultMetadata   = pPayload->partialMetaPayload.pMetaBuffer;
    pPartialCaptureResult->pPrivData                = static_cast<CHIPRIVDATA*>(pPrivData);
    // Dispatch it immediately
    DispatchPartialMetadata(pPartialCaptureResult);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::HandleEarlyMetadataCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::HandleEarlyMetadataCb(
    CbPayloadMetadata* pPayload,
    UINT               pipelineIndex,
    VOID*              pPrivData)
{
    // We will use the pMetadata->pMetadata all the way upto the framework
    InjectResult(ResultType::EarlyMetadataOK, &pPayload->metaPayload, pPayload->sequenceId, pPrivData, pipelineIndex);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::HandleBufferCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::HandleBufferCb(
    CbPayloadBuffer* pPayload,
    UINT             pipelineIndex,
    VOID*            pPrivData)
{
    ChiStreamBuffer outBuffer = { 0 };

    UINT                streamId            = pPayload->streamId;
    PipelineOutputData* pPipelineOutputData = &m_pipelineData[pipelineIndex].pPipelineDescriptor->outputData[streamId];

    outBuffer.size                  = sizeof(ChiStreamBuffer);
    outBuffer.pStream               = reinterpret_cast<ChiStream*>(
                                      pPipelineOutputData->pOutputStreamWrapper->GetNativeStream());
    outBuffer.bufferInfo            = pPayload->bufferInfo;
    outBuffer.bufferStatus          = BufferStatusOK;
    outBuffer.releaseFence.valid    = FALSE; // For the moment

    if ((TRUE == isVideoStream(pPipelineOutputData->pOutputStreamWrapper->GetGrallocUsage())) &&
        (TRUE == CamX::IsGrallocBuffer(&pPayload->bufferInfo)))
    {
        BufferHandle* phNativeHandle = CamX::GetBufferHandleFromBufferInfo(&(pPayload->bufferInfo));

        if (NULL != phNativeHandle)
        {
            SetPerFrameVTTimestampMetadata(*phNativeHandle,
                GetIntraPipelinePerFramePool(PoolType::PerFrameResult, pipelineIndex),
                pPayload->sequenceId + 1); // request ID starts from 1

            if ((FALSE == m_pChiContext->GetStaticSettings()->disableVideoPerfModeSetting) &&
               (TRUE == m_setVideoPerfModeFlag))
            {
                SetPerFrameVideoPerfModeMetadata(*phNativeHandle);
            }
        }
    }

    InjectResult(ResultType::BufferOK, &outBuffer, pPayload->sequenceId, pPrivData, pipelineIndex);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::InjectResult
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::InjectResult(
    ResultType  resultType,
    VOID*       pPayload,
    UINT32      sequenceId,
    VOID*       pPrivData,
    INT32       pipelineIndex)
{
    CamxResult result = CamxResultSuccess;

    m_pResultHolderListLock->Lock();

    ResultHolder* pHolder = GetResultHolderBySequenceId(sequenceId);

    if (NULL == pHolder)
    {
        CAMX_LOG_WARN(CamxLogGroupCore,
                       "Result holder NULL for seqId: %d, this request may be flushed out already.",
                       sequenceId);

        m_pResultHolderListLock->Unlock();

        return CamxResultSuccess;
    }

    CAMX_ASSERT(FALSE == GetDeviceInError());

    pHolder->pipelineIndex = pipelineIndex;

    // Bail out if device/request is in error
    if ((TRUE == GetDeviceInError()))
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Cannot inject results");
        m_pResultHolderListLock->Unlock();

        return CamxResultEFailed;
    }

    if (ResultType::RequestError == resultType)
    {
        pHolder->isCancelled         = TRUE;

        // Update the buffer error to be the notify passed so ProcessResultsBuffer can know to clean up accordingly.
        for (UINT bufIndex = 0; bufIndex < MaxNumOutputBuffers; bufIndex++)
        {
            if (NULL != pHolder->bufferHolder[bufIndex].pStream)
            {
                pHolder->bufferHolder[bufIndex].pBufferError          = static_cast<ChiMessageDescriptor*>(pPayload);
            }
        }
    }
    else if (ResultType::MetadataError == resultType)
    {
        pHolder->pMetadataError = static_cast<ChiMessageDescriptor*>(pPayload);
    }
    else if (ResultType::MetadataOK == resultType)
    {
        /// @todo (CAMX-271) - Handle more than one (>1) partial metadata in pipeline/HAL -
        ///                    When we handle metadata in pipeline, we need to decide how we
        ///                    want to break the slot metadata into multiple result metadata
        ///                    components, as per the contract in MaxPartialMetadataHAL
        ///                    (i.e. android.request.partialResultCount)
        CAMX_LOG_INFO(CamxLogGroupCore, "Inject result added metadata in result holder for sequence ID : %d", sequenceId);

        MetadataPayload* pMetaPayload = static_cast<MetadataPayload*>(pPayload);

        pHolder->pMetaBuffer[0] = pMetaPayload->pMetaBuffer[0];
        pHolder->pMetaBuffer[1] = pMetaPayload->pMetaBuffer[1];
        pHolder->requestId      = pMetaPayload->requestId;

        pHolder->metadataCbIndex++;
    }
    else if (ResultType::EarlyMetadataOK == resultType)
    {
        CAMX_LOG_INFO(CamxLogGroupHAL, "Inject result added early metadata in result holder for sequence ID : %d", sequenceId);

        MetadataPayload* pMetaPayload = static_cast<MetadataPayload*>(pPayload);

        // incase of partial metadata only one buffer is sent from the pipeline
        // partial metadata is kept in buffer index 2
        pHolder->pMetaBuffer[2]       = pMetaPayload->pMetaBuffer[0];

        pHolder->metadataCbIndex++;

    }
    else if (ResultType::BufferError == resultType)
    {
        VOID**           ppPayloads = static_cast<VOID**>(pPayload);
        ChiStreamBuffer* pBuffer    = static_cast<ChiStreamBuffer*>(ppPayloads[0]);
        ChiStream*       pStream    = pBuffer->pStream;

        UINT32 streamIndex = GetStreamIndex(pStream);

        if (FALSE == m_aFlushStatus)
        {
            CAMX_LOG_ERROR(CamxLogGroupCore,
                           "Reporting a buffer error to the framework for streamIndex %u SeqId: %d <-> ReqId: %d",
                           streamIndex,
                           sequenceId,
                           pHolder->requestId);
        }
        else
        {
            CAMX_LOG_INFO(CamxLogGroupCore,
               "Reporting a buffer error during flush to the framework for streamIndex %u SeqId: %d <-> ReqId: %d",
               streamIndex,
               sequenceId,
               pHolder->requestId);
        }

        if (MaxNumOutputBuffers != streamIndex)
        {
            pHolder->bufferHolder[streamIndex].error        = TRUE;
            pHolder->bufferHolder[streamIndex].pBuffer      = pBuffer;
            pHolder->bufferHolder[streamIndex].pBufferError = static_cast<ChiMessageDescriptor*>(ppPayloads[1]);
        }

        pHolder->numErrorBuffersSent++;
    }
    else if (ResultType::BufferOK == resultType)
    {
        ChiStreamBuffer* pBuffer = static_cast<ChiStreamBuffer*>(pPayload);
        ChiStream*       pStream = pBuffer->pStream;

        UINT32 streamIndex = GetStreamIndex(pStream);

        if (MaxNumOutputBuffers != streamIndex)
        {
            ChiStreamBuffer* pHoldBuffer = pHolder->bufferHolder[streamIndex].pBuffer;

            CAMX_LOG_INFO(CamxLogGroupCore,
                          "Inject result added request %d output buffer in result holder:Stream %p Fmt: %d W: %d H: %d",
                          sequenceId,
                          pStream,
                          pStream->format,
                          pStream->width,
                          pStream->height);


            if (NULL != pHoldBuffer)
            {
                if ((pHoldBuffer->pStream             == pStream) &&
                        (pHoldBuffer->bufferInfo.phBuffer == pBuffer->bufferInfo.phBuffer))
                {
                    Utils::Memcpy(pHoldBuffer, pBuffer, sizeof(ChiStreamBuffer));
                    pHolder->bufferHolder[streamIndex].valid = TRUE;
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupCore,
                            "Session %p: Sequence ID %d: Result bufferHolder[%d] pStream = %p, bufferType = %d phBuffer = %p"
                            " does not match buffer Cb pStream = %p, bufferType = %d phBuffer = %p",
                            this,
                            sequenceId,
                            streamIndex,
                            pHoldBuffer->pStream,
                            pHoldBuffer->bufferInfo.bufferType,
                            pHoldBuffer->bufferInfo.phBuffer,
                            pStream,
                            pBuffer->bufferInfo.bufferType,
                            pBuffer->bufferInfo.phBuffer);
                }
            }
        }

        pHolder->numOkBuffersSent++;
    }

    pHolder->pPrivData      = pPrivData;

    // Make the result holder slot live, it's ok to write it more than once
    pHolder->isAlive = TRUE;

    m_pResultHolderListLock->Unlock();

    if (TRUE == MeetFrameworkNotifyCriteria(pHolder))
    {
        // Worker thread needs to check results
        CamxAtomicStore32(&m_aCheckResults, TRUE);


        if (m_pThreadManager->GetJobCount(m_hJobFamilyHandle) <= 1)
        {
            // Check current HAL worker thread job count. If count is 0 or 1, it means we are in below two situations.
            // 1. Possible last request comes in and no job will posted by request anymore.
            // 2. Request is processed too fast and paused.
            // Both cases means there might not be any HALWorker jobs pending to consume the result.
            // And since we have a result that needs to be processed by the HALWorker, it needs to post a job now.

            VOID* pData[] = { this, NULL };
            result        = m_pThreadManager->PostJob(m_hJobFamilyHandle, NULL, &pData[0], FALSE, FALSE);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::ClearAllPendingItems
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::ClearAllPendingItems(
    ResultHolder* pHolder
    ) const
{
    // Request was in error, no result for this frame will be deemed valid but next result can proceed

    pHolder->pendingMetadataCount = 0;

    for (UINT32 i = 0; i < MaxNumOutputBuffers; i++)
    {
        pHolder->bufferHolder[i].pStream = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::MeetFrameworkNotifyCriteria
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::MeetFrameworkNotifyCriteria(
    ResultHolder* pHolder)
{
    CAMX_UNREFERENCED_PARAM(pHolder);

    BOOL metCriteria = FALSE;

    // To start with, we ensure that we have got at least as many metadata as HAL is expected to send in a result
    /// @todo (CAMX-1797) For offline pipelines this is not necessarily valid
    metCriteria = TRUE;

    // We leave this space open for optimization ---
    // We may want to wake the worker up only when we have a batch of results, OR
    // We have output buffers from specific streams in the result, OR
    // Move the burden of checking FIFO eligibility from ProcessResults to here

    return metCriteria;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::MetadataReadyToFly
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::MetadataReadyToFly(
    UINT32 sequenceId)
{
    // Nothing pending, good to go
    SessionResultHolder* pSessionResultHolder =
            reinterpret_cast<SessionResultHolder*>(m_resultHolderList.Head()->pData);
    CamxResult result = CamxResultEFailed;
    BOOL canDispatch = TRUE;
    for (UINT32 i = 0; i < pSessionResultHolder->numResults; i++)
    {
        ResultHolder* pHolder = &pSessionResultHolder->resultHolders[i];

        if (sequenceId == pHolder->sequenceId)
        {
            // Make sure the meta data is updated
            canDispatch = (pHolder->metadataCbIndex == 0) ? FALSE : TRUE;
            result = CamxResultSuccess;
            break;
        }
    }

    if (CamxResultSuccess != result)
    {
        ResultHolder* pLastFrameHolder = GetResultHolderBySequenceId(sequenceId - 1);

        // If we find a previous result AND its metadata count is the same as num partials, then we likely
        // still have processing to do. If pendingMetadataCount != numPartial, it means we've gotten some metadata backs
        if ((NULL != pLastFrameHolder) && (pLastFrameHolder->pendingMetadataCount == m_numMetadataResults))
        {
            canDispatch = FALSE;
        }
    }

    return canDispatch;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::BufferReadyToFly
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::BufferReadyToFly(
    UINT32      sequenceId,
    ChiStream*  pStream)
{
    BOOL isReadyToFly = FALSE;

    ChiStreamWrapper* pChiStreamWrapper = static_cast<ChiStreamWrapper*>(pStream->pPrivateInfo);

    if (TRUE == pChiStreamWrapper->IsNextExpectedResultFrame(sequenceId))
    {
        isReadyToFly = TRUE;
    }

    return isReadyToFly;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::CalculateResultFPS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::CalculateResultFPS()
{
    UINT64 currentTime = OsUtils::GetNanoSeconds();
    UINT64 elapsedTime;

    if (0 == m_lastFPSCountTime)
    {
        m_lastFPSCountTime  = currentTime;
        m_currentFrameCount = 0;
    }
    else
    {
        m_currentFrameCount++;
    }
    elapsedTime = currentTime - m_lastFPSCountTime;

    // Update FPS after 10 secs
    if (elapsedTime > (10 * NanoSecondsPerSecond))
    {
        FLOAT fps = (m_currentFrameCount * NanoSecondsPerSecond / static_cast<FLOAT> (elapsedTime));
        CAMX_LOG_PERF_INFO(CamxLogGroupCore, "FPS: %0.2f", fps);

        m_currentFrameCount = 0;
        m_lastFPSCountTime  = currentTime;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::AdvanceMinExpectedResult
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::AdvanceMinExpectedResult()
{
    /// @note This function can only be called from Session::ProcessRequest with m_pResultLock taken.  If it is called from
    ///       anywhere else, a deadlock is possible
    BOOL canAdvance = TRUE;
    BOOL moveNext   = TRUE;

    m_pResultHolderListLock->Lock();

    LightweightDoublyLinkedListNode* pNode = m_resultHolderList.Head();

    if ((NULL == pNode) && (0 < m_resultHolderList.NumNodes()))
    {
        CAMX_LOG_WARN(CamxLogGroupCore,
                "Session %p: Warning: Result Holder HEAD is NULL with size %d",
                this, m_resultHolderList.NumNodes());
    }

    while (NULL != pNode)
    {
        SessionResultHolder* pSessionResultHolder = reinterpret_cast<SessionResultHolder*>(pNode->pData);

        CAMX_ASSERT(NULL != pSessionResultHolder);


        // If there are multiple pipeline result map to same framework number
        // e.g. In dual camera both wide and tele pipeline need to generate output for same framework request
        // all the pipeline result need to be sent before it can advance.
        for (UINT resultIndex = 0; resultIndex < pSessionResultHolder->numResults; resultIndex++)
        {
            ResultHolder* pHolder = &pSessionResultHolder->resultHolders[resultIndex];

            if (NULL != pHolder)
            {

                // Make sure all of the pStreams have been consumed from the result bufferHolder, if any are NOT NULL it means
                // we haven't finished with the request and still have more results to return
                if (TRUE == canAdvance)
                {
                    for (UINT32 bufferIndex = 0; bufferIndex < MaxNumOutputBuffers; bufferIndex++)
                    {
                        if (NULL != pHolder->bufferHolder[bufferIndex].pStream)
                        {
                            CAMX_LOG_INFO(CamxLogGroupCore,
                                "Session %p: Can't advance because of buffer[%d] for sequence Id: %d",
                                this,
                                bufferIndex,
                                reinterpret_cast<ResultHolder*>(pNode->pData)->sequenceId);
                            canAdvance      = FALSE;
                            break;
                        }
                    }
                }

                if ((TRUE == canAdvance) && (FALSE == pHolder->isCancelled))
                {
                    if (pHolder->pendingMetadataCount > 0)
                    {
                        CAMX_LOG_INFO(CamxLogGroupCore,
                            "Session %p: Can't advance because of pending metadata for sequence id: %d",
                            this,
                            reinterpret_cast<ResultHolder*>(pNode->pData)->sequenceId);
                        canAdvance = FALSE;
                    }
                }

                if (FALSE == canAdvance)
                {
                    break;
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupCore, "Session %p: pHolder is NULL while trying to advance min result", this);
            }
        }

        if (TRUE == canAdvance)
        {
            for (UINT resultIndex = 0; resultIndex < pSessionResultHolder->numResults; resultIndex++)
            {
                ResultHolder* pHolder = &pSessionResultHolder->resultHolders[resultIndex];
                // Making it this far for a given pHolder means it's ready to be popped off the
                // list and we can move forward.

                // all buffers are done. update metabuffer done queue
                UINT32 index = pHolder->sequenceId % MaxQueueDepth;
                m_pendingMetabufferDoneQueue.pendingMask[index] |= MetaBufferDoneBufferReady;

                if (TRUE == m_pChiContext->GetStaticSettings()->enableFPSLog)
                {
                    CalculateResultFPS();
                }

                // If DRQ debugging is enabled dump it out.
                m_pDeferredRequestQueue->DumpDebugInfo(pHolder->requestId, FALSE);

                // Add some trace events
                CAMX_TRACE_ASYNC_END_F(CamxLogGroupCore, pHolder->sequenceId, "HAL3: RequestTrace");
                CAMX_LOG_INFO(CamxLogGroupCore,
                              "Session %p: Results processed for sequenceId: %d",
                              this, pHolder->sequenceId);
            }

            CAMX_FREE(pNode->pData);
            pNode->pData = NULL;

            // Since we've finished the requeset, remove the node from the list
            m_resultHolderList.RemoveNode(pNode);

            CAMX_FREE(pNode);
            pNode    = NULL;
            moveNext = FALSE;

            m_pResultHolderListLock->Unlock();

            // If we've gotten any buffer ready for this result then we can accept another
            m_pLivePendingRequestsLock->Lock();

            if (0 < m_livePendingRequests)
            {
                m_livePendingRequests--;

                m_pWaitLivePendingRequests->Signal();
            }

            m_pLivePendingRequestsLock->Unlock();

            NotifyProcessingDone();
            m_pResultHolderListLock->Lock();
        }
        else
        {
            break;
        }

        if (TRUE == moveNext)
        {
            pNode = m_resultHolderList.NextNode(pNode);
        }
        else
        {
            pNode = m_resultHolderList.Head();
        }
    }

    if ((0 != m_resultHolderList.NumNodes()) &&
        ((NULL != m_resultHolderList.Head()) &&
         (NULL != m_resultHolderList.Tail())))
    {
        SessionResultHolder* pSessionResultHolderHead =
            reinterpret_cast<SessionResultHolder*>(m_resultHolderList.Head()->pData);
        SessionResultHolder* pSessionResultHolderTail =
            reinterpret_cast<SessionResultHolder*>(m_resultHolderList.Tail()->pData);
        if ((NULL != pSessionResultHolderHead) && (NULL != pSessionResultHolderTail))
        {
            CAMX_LOG_INFO(CamxLogGroupCore,
                          "Session %p: Results processed, current queue state: minResultIdExpected = %llu, "
                          "maxResultIdExpected= %llu, minSequenceIdExpected: %u, maxSequenceIdExpected: %u, "
                          "and numResultsPending: %d",
                          this,
                          pSessionResultHolderHead->resultHolders[0].requestId,
                          pSessionResultHolderTail->resultHolders[pSessionResultHolderTail->numResults - 1].requestId,
                          pSessionResultHolderHead->resultHolders[0].sequenceId,
                          pSessionResultHolderTail->resultHolders[pSessionResultHolderTail->numResults - 1].sequenceId,
                          m_resultHolderList.NumNodes());
        }
        m_pResultHolderListLock->Unlock();
    }
    else
    {
        m_pResultHolderListLock->Unlock();

        CAMX_LOG_INFO(CamxLogGroupCore, "Session %p: All results processed, current queue state is empty.", this);

        // Now that we've emptied the queue, signal saying we're ready for a new request if they exist
        m_pWaitLivePendingRequests->Signal();

        // If flush and process_capture_request were racing, it's possible that we accepted the job
        // and added it to the HALQueue right as we were starting flush, in which case we've got a request
        // hanging out in the HALQueue that now needs to be processed, so this kicks the queue
        if (FALSE == m_pRequestQueue->IsEmpty())
        {
            VOID* pData[] = { this, NULL };
            m_pThreadManager->PostJob(m_hJobFamilyHandle, NULL, &pData[0], FALSE, FALSE);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DetermineActiveStreams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID DetermineActiveStreams(
    PipelineProcessRequestData*  pPipelineProcessRequestData)
{
    const CaptureRequest* pCaptureRequest = pPipelineProcessRequestData->pCaptureRequest;

    for (UINT frameIndex = 0; frameIndex < pCaptureRequest->numBatchedFrames; frameIndex++)
    {
        PerBatchedFrameInfo* pTopologyPerFrameInfo = &pPipelineProcessRequestData->pPerBatchedFrameInfo[frameIndex];

        pTopologyPerFrameInfo->activeStreamIdMask  = 0;
        pTopologyPerFrameInfo->sequenceId          = pCaptureRequest->pStreamBuffers[frameIndex].sequenceId;

        for (UINT i = 0; i < pCaptureRequest->pStreamBuffers[frameIndex].numOutputBuffers; i++)
        {
            const ChiStreamBuffer*  pOutputBuffer  =
                reinterpret_cast<const ChiStreamBuffer*>(&pCaptureRequest->pStreamBuffers[frameIndex].outputBuffers[i]);
            const ChiStreamWrapper* pStreamWrapper = static_cast<ChiStreamWrapper*>(pOutputBuffer->pStream->pPrivateInfo);

            UINT streamId          = pStreamWrapper->GetStreamIndex();
            UINT currentStreamMask = pTopologyPerFrameInfo->activeStreamIdMask;

            CAMX_ASSERT(TRUE == CamX::IsValidChiBuffer(&pOutputBuffer->bufferInfo));

            pTopologyPerFrameInfo->bufferInfo[streamId]  = pOutputBuffer->bufferInfo;
            pTopologyPerFrameInfo->activeStreamIdMask   |= Utils::BitSet(currentStreamMask, streamId);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::UpdateMultiRequestSyncData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::UpdateMultiRequestSyncData(
    const ChiPipelineRequest* pRequest)
{
    UINT        index           = 0;
    UINT32      pipelineIndex   = InvalidPipelineIndex;
    UINT64      comboID         = m_syncSequenceId;
    UINT32      comboIndex      = comboID % MaxQueueDepth;
    UINT32      lastComboIndex  = (comboID - 1) % MaxQueueDepth;

    // just multiple realtime pipeline request in one session
    if (m_numInputSensors >= 2)
    {
        /// refresh request data;
        for (index = 0; index < m_numPipelines; index++)
        {
            m_requestSyncData[comboIndex].prevReq.requestID[index] = m_requestSyncData[lastComboIndex].currReq.requestID[index];
            m_requestSyncData[comboIndex].prevReq.isActive[index]  = m_requestSyncData[lastComboIndex].currReq.isActive[index];
            m_requestSyncData[comboIndex].prevReq.isMaster[index]  = m_requestSyncData[lastComboIndex].currReq.isMaster[index];
            m_requestSyncData[comboIndex].prevReq.isMultiRequest   = m_requestSyncData[lastComboIndex].currReq.isMultiRequest;

            m_requestSyncData[comboIndex].currReq.requestID[index] = m_requestSyncData[comboIndex].prevReq.requestID[index];
            m_requestSyncData[comboIndex].currReq.isActive[index]  = FALSE;
            m_requestSyncData[comboIndex].currReq.isMaster[index]  = FALSE;
            m_requestSyncData[comboIndex].currReq.isMultiRequest   = FALSE;
        }

        m_requestSyncData[comboIndex].syncSequenceId    = m_syncSequenceId;
        m_requestSyncData[comboIndex].numPipelines      = m_numPipelines;

        UINT32     metaTag = 0;
        CamxResult result  = VendorTagManager::QueryVendorTagLocation("com.qti.chi.multicamerainfo", "MasterCamera", &metaTag);
        for (index = 0; index < pRequest->numRequests; index++)
        {
            if (0 != pRequest->pCaptureRequests[index].hPipelineHandle)
            {
                pipelineIndex = GetPipelineIndex(pRequest->pCaptureRequests[index].hPipelineHandle);

                if (pipelineIndex != InvalidPipelineIndex)
                {
                    m_requestSyncData[comboIndex].currReq.requestID[pipelineIndex]
                        = m_requestSyncData[lastComboIndex].currReq.requestID[pipelineIndex] + 1;
                    m_requestSyncData[comboIndex].currReq.isActive[pipelineIndex] = TRUE;

                    if (CamxResultSuccess == result)
                    {
                        Pipeline*       pPipeline   = m_pipelineData[pipelineIndex].pPipeline;
                        MetadataPool*   pInputPool  = pPipeline->GetPerFramePool(PoolType::PerFrameInput);
                        MetadataSlot*   pSlot       =
                            pInputPool->GetSlot(m_requestSyncData[comboIndex].currReq.requestID[pipelineIndex]);

                        BOOL* pMetaValue = reinterpret_cast<BOOL*>(pSlot->GetMetadataByTag(metaTag));
                        if (NULL != pMetaValue)
                        {
                            m_requestSyncData[comboIndex].currReq.isMaster[pipelineIndex] = *pMetaValue;
                        }
                        else
                        {
                            CAMX_LOG_ERROR(CamxLogGroupHAL, "Error when retrieving vendor tag: NULL pointer");
                            m_requestSyncData[comboIndex].currReq.isMaster[pipelineIndex] = FALSE;
                        }
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupHAL, "Error when querying vendor tag");
                        m_requestSyncData[comboIndex].currReq.isMaster[pipelineIndex] = FALSE;
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupHAL, "unkown pipeline handle request!");
                }
            }
        }

        // just number of ruquest is bigger than 2, it needs 3A sync.
        if (pRequest->numRequests >= 2)
        {
            m_requestSyncData[comboIndex].currReq.isMultiRequest = TRUE;
        }
        else
        {
            m_requestSyncData[comboIndex].currReq.isMultiRequest = FALSE;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupStats, "MultiReq: syncSequenceId:%llu comboIndex:%d Num:%d Multi:%d",
                         m_syncSequenceId, comboIndex,
                         m_requestSyncData[comboIndex].numPipelines,
                         m_requestSyncData[comboIndex].currReq.isMultiRequest);
        for (index = 0; index < m_requestSyncData[comboIndex].numPipelines; index++)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupHAL, "MultiReq: isActive[%d]:%d, Req[%d]:%lu, Master[%d]:%d",
                             index, m_requestSyncData[comboIndex].currReq.isActive[index],
                             index, m_requestSyncData[comboIndex].currReq.requestID[index],
                             index, m_requestSyncData[comboIndex].currReq.isMaster[index]);
        }

        for (index = 0; index < m_requestSyncData[comboIndex].numPipelines; index++)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupHAL, "MultiReq_Prev: isActive[%d]:%d, Req[%d]:%lu, Master[%d]:%d",
                             index, m_requestSyncData[comboIndex].prevReq.isActive[index],
                             index, m_requestSyncData[comboIndex].prevReq.requestID[index],
                             index, m_requestSyncData[comboIndex].prevReq.isMaster[index]);
        }

        m_syncSequenceId++;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::CanRequestProceed
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::CanRequestProceed(
    const ChiCaptureRequest* pRequest)
{
    CAMX_UNREFERENCED_PARAM(pRequest);

    CamxResult result = CamxResultSuccess;

    if (TRUE == static_cast<BOOL>(CamxAtomicLoad32(&m_aDeviceInError)))
    {
        result = CamxResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::CheckValidChiStreamBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::CheckValidChiStreamBuffer(
    const ChiStreamBuffer* pStreamBuffer
    ) const
{
    CamxResult result = CamxResultSuccess;

    if (NULL == pStreamBuffer)
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Invalid input");
        result = CamxResultEInvalidArg;
    }

    if (CamxResultSuccess == result)
    {
        if (sizeof(CHISTREAMBUFFER) != pStreamBuffer->size)
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "Invalid stream buffer - expected size %u incoming size %u",
                           static_cast<UINT32>(sizeof(CHISTREAMBUFFER)), pStreamBuffer->size);
            result = CamxResultEInvalidArg;
        }
    }

    if (CamxResultSuccess == result)
    {
        if (FALSE == CamX::IsValidChiBuffer(&pStreamBuffer->bufferInfo))
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "Invalid stream buffer - bufferType = %d, phBuffer = %p",
                           pStreamBuffer->bufferInfo.bufferType,
                           pStreamBuffer->bufferInfo.phBuffer);
            result = CamxResultEInvalidArg;
        }
    }

    // Print ChiStreamBuffer info - helps in debugging
    if (CamxResultSuccess == result)
    {
        CHISTREAM* pStream = pStreamBuffer->pStream;

        CAMX_LOG_VERBOSE(CamxLogGroupCore,
                         "ChiStreamBuffer : size=%d, pStream=%p, bufferType=%d, phBuffer=%p, "
                         "bufferStatus=%d, acquireFence=(valid=%d type=%d nativeFence=%d chiFence=%p), "
                         "releaseFence=(valid=%d type=%d nativeFence=%d chiFence=%p)",
                         pStreamBuffer->size,
                         pStreamBuffer->pStream,
                         pStreamBuffer->bufferInfo.bufferType,
                         pStreamBuffer->bufferInfo.phBuffer,
                         pStreamBuffer->bufferStatus,
                         pStreamBuffer->acquireFence.valid,
                         pStreamBuffer->acquireFence.type,
                         pStreamBuffer->acquireFence.nativeFenceFD,
                         pStreamBuffer->acquireFence.hChiFence,
                         pStreamBuffer->releaseFence.valid,
                         pStreamBuffer->releaseFence.type,
                         pStreamBuffer->releaseFence.nativeFenceFD,
                         pStreamBuffer->releaseFence.hChiFence);

        if (NULL != pStream)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupCore,
                             "ChiStream : streamType=%d, width=%d, height=%d, format=%d, grallocUsage=%d, "
                             "maxNumBuffers=%d, pPrivateInfo=%p, dataspace=%d, rotation=%d, planeStride=%d, sliceHeight=%d",
                             pStream->streamType,
                             pStream->width,
                             pStream->height,
                             pStream->format,
                             pStream->grallocUsage,
                             pStream->maxNumBuffers,
                             pStream->pPrivateInfo,
                             pStream->dataspace,
                             pStream->rotation,
                             pStream->streamParams.planeStride,
                             pStream->streamParams.sliceHeight);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::CheckValidInputRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::CheckValidInputRequest(
    const ChiCaptureRequest* pCaptureRequest
    ) const
{
    //// HAL interface requires -EINVAL (EInvalidArg) if the input request is malformed
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pCaptureRequest);

    // Validate input ChiStreamBuffer
    if ((0 < pCaptureRequest->numInputs) && (NULL != pCaptureRequest->pInputBuffers))
    {
        for (UINT32 bufferIndex = 0; bufferIndex < pCaptureRequest->numInputs; bufferIndex++)
        {
            result = CheckValidChiStreamBuffer(&pCaptureRequest->pInputBuffers[bufferIndex]);

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupCore, "Invalid input stream buffer bufferIndex=%d, result = %s",
                               bufferIndex, Utils::CamxResultToString(result));
                break;
            }
        }
    }

    // Validate output ChiStreamBuffer
    if ((CamxResultSuccess == result) &&
        (0 < pCaptureRequest->numOutputs) && (NULL != pCaptureRequest->pOutputBuffers))
    {
        for (UINT32 bufferIndex = 0; bufferIndex < pCaptureRequest->numOutputs; bufferIndex++)
        {
            result = CheckValidChiStreamBuffer(&pCaptureRequest->pOutputBuffers[bufferIndex]);

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupCore, "Invalid output stream buffer bufferIndex=%d, result = %s",
                               bufferIndex, Utils::CamxResultToString(result));
                break;
            }
        }
    }

    /// @todo (CAMX-1797) Add validation

    // BOOL              foundMatch        = FALSE;
    // ChiStreamWrapper* pChiStreamWrapper = NULL;
    //
    // CAMX_ASSERT(NULL != pPipelineRequest);
    //
    // for (UINT i = 0; i < pPipelineRequest->numRequests; i++)
    // {
    //     const ChiCaptureRequest* pRequest            = &pPipelineRequest->pCaptureRequests[i];
    //     PipelineDescriptor*      pPipelineDescriptor = GetPipelineDescriptor(pRequest->hPipelineHandle);
    //
    //     if ((NULL != pRequest) && (pRequest->numOutputs > 0))
    //     {
    //         for (UINT32 buffer = 0; buffer < pRequest->numOutputs; buffer++)
    //         {
    //             if (NULL != pRequest->pOutputBuffers)
    //             {
    //                 foundMatch        = FALSE;
    //                 pChiStreamWrapper =
    //                     reinterpret_cast<ChiStreamWrapper*>(pRequest->pOutputBuffers[buffer].pStream->pPrivateInfo);
    //
    //                 for (UINT32 stream = 0; stream < phPipelineHandle->numStreams; stream++)
    //                 {
    //                     if (pChiStreamWrapper == phPipelineHandle->ppChiStreamWrappers[stream])
    //                     {
    //                         foundMatch = TRUE;
    //                         break;
    //                     }
    //                 }
    //
    //                 if (FALSE == foundMatch)
    //                 {
    //                     CAMX_LOG_ERROR(CamxLogGroupHAL, "Not a valid request, o/p stream not configured!");
    //                     break;
    //                 }
    //             }
    //         }
    //
    //         if (TRUE == foundMatch)
    //         {
    //             for (UINT32 buffer = 0; buffer < pRequest->numInputs; buffer++)
    //             {
    //                 if (NULL != pRequest->pInputBuffers)
    //                 {
    //                     foundMatch        = FALSE;
    //                     pChiStreamWrapper =
    //                         reinterpret_cast<ChiStreamWrapper*>(pRequest->pInputBuffers[buffer].pStream->pPrivateInfo);
    //
    //                     for (UINT32 stream = 0; stream < phPipelineHandle->numStreams; stream++)
    //                     {
    //                         if (pChiStreamWrapper == phPipelineHandle->ppChiStreamWrappers[stream])
    //                         {
    //                             foundMatch = TRUE;
    //                             break;
    //                         }
    //                     }
    //
    //                     if (FALSE == foundMatch)
    //                     {
    //                         CAMX_LOG_ERROR(CamxLogGroupHAL, "Not a valid request, i/p stream not configured!");
    //                         break;
    //                     }
    //                 }
    //             }
    //         }
    //
    //         if (TRUE == foundMatch)
    //         {
    //             result = CamxResultSuccess;
    //
    //             if (NULL != pRequest->pMetadata)
    //             {
    //                 if (pRequest->numInputs > 0)
    //                 {
    //                     // result = check for valid reprocess settings
    //                     /// @todo (CAMX-356): Check validity of request
    //                 }
    //                 else
    //                 {
    //                     // result = check for valid capture settings
    //                     /// @todo (CAMX-356): Check validity of request
    //                 }
    //             }
    //         }
    //     }
    // }

    if ((NULL == pCaptureRequest->pInputMetadata) ||
        (NULL == pCaptureRequest->pOutputMetadata) ||
        (FALSE == MetaBuffer::IsValid(static_cast<MetaBuffer*>(pCaptureRequest->pInputMetadata))) ||
        (FALSE == MetaBuffer::IsValid(static_cast<MetaBuffer*>(pCaptureRequest->pOutputMetadata))))
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupHAL, "ERROR Invalid metadata provided input %p output %p",
                       pCaptureRequest->pInputMetadata,
                       pCaptureRequest->pOutputMetadata);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::WaitOnAcquireFence
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::WaitOnAcquireFence(
    const ChiCaptureRequest* pRequest)
{
    CamxResult result = CamxResultSuccess;
#if ANDROID
    for (UINT i = 0; i < pRequest->numOutputs; i++)
    {
        if ((TRUE               == pRequest->pOutputBuffers[i].acquireFence.valid) &&
            (ChiFenceTypeNative == pRequest->pOutputBuffers[i].acquireFence.type)  &&
            (InvalidNativeFence != pRequest->pOutputBuffers[i].acquireFence.nativeFenceFD))
        {
            NativeFence acquireFence = pRequest->pOutputBuffers[i].acquireFence.nativeFenceFD;

            CAMX_TRACE_SYNC_BEGIN_F(CamxLogGroupSync, "Waiting on Acquire Fence");
            /// @todo (CAMX-2491) Define constant for HalFenceTimeout
            result = OsUtils::NativeFenceWait(acquireFence, 5000);
            CAMX_TRACE_SYNC_END(CamxLogGroupSync);

            if (CamxResultSuccess != result)
            {
                break;
            }

            OsUtils::Close(acquireFence);
        }
    }
#else
    CAMX_UNREFERENCED_PARAM(pRequest);
#endif // ANDROID

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::GetIntraPipelinePerFramePool
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MetadataPool* Session::GetIntraPipelinePerFramePool(
    PoolType poolType,
    UINT     pipelineId)
{
    CAMX_ASSERT(pipelineId < m_numPipelines);
    return m_pipelineData[pipelineId].pPipeline->GetPerFramePool(poolType);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::GetStreamIndex
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE UINT32 Session::GetStreamIndex(
    ChiStream* pStream
    ) const
{
    ChiStreamWrapper* pChiStreamWrapper = static_cast<ChiStreamWrapper*>(pStream->pPrivateInfo);

    return pChiStreamWrapper->GetStreamIndex();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::UnregisterThreadJobCallback
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::UnregisterThreadJobCallback()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::FlushThreadJobCallback
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::FlushThreadJobCallback()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::GetCurrentSequenceId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 Session::GetCurrentSequenceId()
{
    return m_sequenceId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::GetCurrentPipelineRequestId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 Session::GetCurrentPipelineRequestId(
    UINT pipelineIndex)
{
    CAMX_ASSERT(pipelineIndex < m_numPipelines);
    return m_requestBatchId[pipelineIndex];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::IsResultHolderEmpty
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::IsResultHolderEmpty()
{
    BOOL result     = FALSE;
    UINT numNodes   = 0;

    m_pResultHolderListLock->Lock();
    numNodes = m_resultHolderList.NumNodes();
    m_pResultHolderListLock->Unlock();

    if (0 == numNodes)
    {
        result = TRUE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::IsPipelineRealTime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::IsPipelineRealTime(
    CHIPIPELINEHANDLE hPipelineDescriptor)
{
    UINT32     index = 0;

    // input pipelineIndex not really match the index recorded by Session, so use Descriptor to find it.
    for (index = 0; index < m_numPipelines; index++)
    {
        if (hPipelineDescriptor == m_pipelineData[index].pPipelineDescriptor)
        {
            // found corresponding pipeline can use index to get to it
            break;
        }
    }

    CAMX_ASSERT(index < m_numPipelines);

    Pipeline* pPipeline = m_pipelineData[index].pPipeline;
    CAMX_ASSERT(NULL != pPipeline);

    return pPipeline->IsRealTime();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::QueryStreamHDRMode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::QueryStreamHDRMode(
    HAL3Stream*              pStream,
    MetadataPool*            pInputPool,
    UINT64                   requestId)
{
    CamxResult                result = CamxResultSuccess;

    if (HDRModeMax == pStream->GetHDRMode())
    {
        // HDR mode can be only changed once when the first capture request given.
        UINT32 metaTag        = 0;
        UINT8  currentHDRMode = HDRModeNone;
        Format format         = pStream->GetInternalFormat();

        result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.streamconfigs", "HDRVideoMode", &metaTag);

        if (CamxResultSuccess == result)
        {
            MetadataSlot* pMetaDataSlot = pInputPool->GetSlot(requestId);
            UINT8* pMetaValue = static_cast<UINT8*>(pMetaDataSlot->GetMetadataByTag(metaTag));

            if (NULL != pMetaValue)
            {
                currentHDRMode = *pMetaValue;

                if (currentHDRMode >= HDRModeMax)
                {
                    CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid HDR mode :%d sent", currentHDRMode);
                    currentHDRMode = HDRModeNone;
                }
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL, "query HDR mode vendor tag failed");
        }

        switch (format)
        {
            case Format::UBWCTP10:
            case Format::P010:
                pStream->SetHDRMode(currentHDRMode);
                break;

            default:
                pStream->SetHDRMode(HDRModeNone);
                break;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupHAL, "Set stream HDR mode %d", pStream->GetHDRMode());
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::SetPerStreamColorMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::SetPerStreamColorMetadata(
    const ChiCaptureRequest* pRequest,
    MetadataPool*            pInputPool,
    UINT64                   requestId)
{
    CamxResult                result         = CamxResultSuccess;
#if (!defined(LE_CAMERA)) // ANDROID
    ColorMetaData             bufferMetadata   = {};
    CHISTREAM*                pStream        = NULL;
    struct private_handle_t*  phBufferHandle = NULL;

    CAMX_LOG_VERBOSE(CamxLogGroupHAL, "Request with outbuffer num %d", pRequest->numOutputs);

    for (UINT32 i = 0; i < pRequest->numOutputs; i++)
    {
        pStream = pRequest->pOutputBuffers[i].pStream;

        HAL3Stream*  pHAL3Stream = static_cast<HAL3Stream*>(pStream->pPrivateInfo);

        CAMX_ASSERT(pHAL3Stream != NULL);
        QueryStreamHDRMode(pHAL3Stream, pInputPool, requestId);

        switch (pHAL3Stream->GetHDRMode())
        {
            case HDRModeHLG:
                CAMX_LOG_VERBOSE(CamxLogGroupHAL, "Set HDR mode HLG for req %llu", requestId);

                // In HLG mode, UBWC 10bit-TP
                bufferMetadata.colorPrimaries           = ColorPrimaries_BT2020;
                bufferMetadata.range                    = Range_Full;
                bufferMetadata.transfer                 = Transfer_HLG;
                bufferMetadata.matrixCoefficients       = MatrixCoEff_BT2020;

                bufferMetadata.contentLightLevel.lightLevelSEIEnabled    = TRUE;
                bufferMetadata.contentLightLevel.maxContentLightLevel    = MaxContentLightLevel;
                bufferMetadata.contentLightLevel.minPicAverageLightLevel = MaxFrameAverageLightLevel;

                bufferMetadata.masteringDisplayInfo.colorVolumeSEIEnabled   = TRUE;
                bufferMetadata.masteringDisplayInfo.maxDisplayLuminance     = MaxDisplayLuminance;
                bufferMetadata.masteringDisplayInfo.minDisplayLuminance     = MinDisplayLuminance;

                Utils::Memcpy(&bufferMetadata.masteringDisplayInfo.primaries.rgbPrimaries, PrimariesRGB,
                    sizeof(bufferMetadata.masteringDisplayInfo.primaries.rgbPrimaries));
                Utils::Memcpy(&bufferMetadata.masteringDisplayInfo.primaries.whitePoint, PrimariesWhitePoint,
                    sizeof(bufferMetadata.masteringDisplayInfo.primaries.whitePoint));
                break;
            case HDRModeHDR10:
                CAMX_LOG_VERBOSE(CamxLogGroupHAL, "Set HDR mode HDR10 for req %llu", requestId);

                bufferMetadata.colorPrimaries       = ColorPrimaries_BT2020;
                bufferMetadata.range                = Range_Full;
                bufferMetadata.transfer             = Transfer_SMPTE_ST2084;
                bufferMetadata.matrixCoefficients   = MatrixCoEff_BT2020;

                bufferMetadata.contentLightLevel.lightLevelSEIEnabled    = TRUE;
                bufferMetadata.contentLightLevel.maxContentLightLevel    = MaxContentLightLevel;
                bufferMetadata.contentLightLevel.minPicAverageLightLevel = MaxFrameAverageLightLevel;

                bufferMetadata.masteringDisplayInfo.colorVolumeSEIEnabled   = TRUE;
                bufferMetadata.masteringDisplayInfo.maxDisplayLuminance     = MaxDisplayLuminance;
                bufferMetadata.masteringDisplayInfo.minDisplayLuminance     = MinDisplayLuminance;

                Utils::Memcpy(&bufferMetadata.masteringDisplayInfo.primaries.rgbPrimaries, PrimariesRGB,
                    sizeof(bufferMetadata.masteringDisplayInfo.primaries.rgbPrimaries));
                Utils::Memcpy(&bufferMetadata.masteringDisplayInfo.primaries.whitePoint, PrimariesWhitePoint,
                    sizeof(bufferMetadata.masteringDisplayInfo.primaries.whitePoint));
                break;
            default:
                CAMX_LOG_VERBOSE(CamxLogGroupHAL, "Set HDR mode default for req %llu", requestId);

                // default Color Metadata
                bufferMetadata.colorPrimaries        = ColorPrimaries_BT601_6_625;
                bufferMetadata.range                 = Range_Full;
                bufferMetadata.transfer              = Transfer_SMPTE_170M;
                bufferMetadata.matrixCoefficients    = MatrixCoEff_BT601_6_625;
                break;
        }

        /// @todo (CAMX-2499): Decouple the camx core dependency from the android display API.
        ChiStreamBuffer* pBuffers = pRequest->pOutputBuffers;

        if (FALSE == CamX::IsValidChiBuffer(&pBuffers[i].bufferInfo))
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "Output Buffer[%d] : unsupported buffer type %d",
                           i, pBuffers[i].bufferInfo.bufferType);

            result = CamxResultEInvalidArg;
            break;
        }
        else if (FALSE == IsChiNativeBufferType(&pBuffers[i].bufferInfo))
        {
            BufferHandle* phNativeHandle = CamX::GetBufferHandleFromBufferInfo(&pBuffers[i].bufferInfo);

            if (NULL != phNativeHandle)
            {
                // NOWHINE CP036a: exception
                phBufferHandle = const_cast<struct private_handle_t*>(
                                 reinterpret_cast<const struct private_handle_t*>(*phNativeHandle));

                setMetaData(phBufferHandle, COLOR_METADATA, &bufferMetadata);
            }
        }
    }
#endif // ANDROID
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::SetPerFrameVTTimestampMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::SetPerFrameVTTimestampMetadata(
    const NativeHandle* phNativeBufferHandle,
    MetadataPool*       pPool,
    UINT64              requestId)
{
    UINT64*                   pTimestamp        = NULL;
    struct private_handle_t*  phBufferHandle    = NULL;

    CAMX_ASSERT(NULL != pPool);
    MetadataSlot* pMetadataSlot = pPool->GetSlot(requestId);
    CAMX_ASSERT(NULL != pMetadataSlot);
    pTimestamp = static_cast<UINT64*>(pMetadataSlot->GetMetadataByTag(m_vendorTagIndexTimestamp));

    if (NULL != pTimestamp)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupHAL, "PerFrame Metadata VT timestamp=%llu", *pTimestamp);

        // NOWHINE CP036a: Google API requires const types
        phBufferHandle = const_cast<struct private_handle_t*>(
                reinterpret_cast<const struct private_handle_t*>(phNativeBufferHandle));
#if (!defined(LE_CAMERA)) // ANDROID
        setMetaData(phBufferHandle, SET_VT_TIMESTAMP, pTimestamp);
#endif // ANDROID
        m_qtimerErrIndicated = 0;
    }
    else
    {
        const UINT32 ErrLogThreshold = 20; // Stop spewing the error after this threshold

        if (m_qtimerErrIndicated < ErrLogThreshold)
        {
            m_qtimerErrIndicated++;
            CAMX_LOG_WARN(CamxLogGroupHAL,
                            "Failed to retrieve VT timestamp for requestId: %llu, encoder will fallback to system time",
                            requestId);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::SetPerFrameVideoPerfModeMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::SetPerFrameVideoPerfModeMetadata(
    const NativeHandle* phNativeBufferHandle)
{
    struct private_handle_t*  phBufferHandle = NULL;
    UINT32                    videoPerfMode  = 1;

    // NOWHINE CP036a: Google API requires const types
    phBufferHandle = const_cast<struct private_handle_t*>(
        reinterpret_cast<const struct private_handle_t*>(phNativeBufferHandle));

    CAMX_LOG_INFO(CamxLogGroupCore, "Set Video Perf mode %d", videoPerfMode);
#if (!defined(LE_CAMERA)) // ANDROID
    setMetaData(phBufferHandle, SET_VIDEO_PERF_MODE, &videoPerfMode);
#endif // ANDROID
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::DumpResultState
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::DumpResultState(
    INT     fd,
    UINT32  indent)
{
    /// @note Accessing with a TryLock since this is intended to be a post-mortem log.  If we try to enforce the lock, there's a
    ///       reasonable chance the post-mortem will deadlock. Failed locks will be noted.
    CamxResult result = m_pResultLock->TryLock();

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_TO_FILE(fd, indent, "WARNING: Lock failed with status: %s.  Stopping dump",
                         CamxResultStrings[result]);
        return;
    }

    LightweightDoublyLinkedListNode* pNode                = m_resultHolderList.Head();
    ResultHolder*                    pResultHolder        = NULL;
    SessionResultHolder*             pSessionResultHolder = NULL;

    CAMX_LOG_TO_FILE(fd, indent, "Num entries in holder list = %u", m_resultHolderList.NumNodes());

    while (NULL != pNode)
    {
        if (NULL != pNode->pData)
        {
            pSessionResultHolder = reinterpret_cast<SessionResultHolder*>(pNode->pData);

            CAMX_LOG_TO_FILE(fd, indent + 2, "Session Result Holder %p - numResults = %u",
                             pSessionResultHolder,
                             pSessionResultHolder->numResults);

            for (UINT resHolderIdx = 0; resHolderIdx < pSessionResultHolder->numResults; resHolderIdx++)
            {
                pResultHolder = &pSessionResultHolder->resultHolders[resHolderIdx];
                if (NULL != pResultHolder)
                {
                    CAMX_LOG_TO_FILE(fd, indent + 4,
                                    "Result Holder %u - RequestId = %u, SequenceId = %u, numInBuffers = %u, numOutBuffers = %u",
                                     resHolderIdx,
                                     pResultHolder->requestId,
                                     pResultHolder->sequenceId,
                                     pResultHolder->numInBuffers,
                                     pResultHolder->numOutBuffers);

                    CAMX_LOG_TO_FILE(fd, indent + 6, "numOkBuffersSent = %u, numErrorBuffersSent = %u, "
                                     "pendingMetadataCount = %u, isShutterSentOut = %s",
                                     pResultHolder->numOkBuffersSent,
                                     pResultHolder->numErrorBuffersSent,
                                     pResultHolder->pendingMetadataCount,
                                     Utils::BoolToString(pResultHolder->isShutterSentOut));

                    for (UINT i = 0; i < MaxNumOutputBuffers; i++)
                    {
                        if ((NULL != pResultHolder->bufferHolder[i].pStream) ||
                            (NULL != pResultHolder->bufferHolder[i].pBuffer))
                        {
                            CAMX_LOG_TO_FILE(fd, indent+ 6, "bufferHolder[%u] - pStream = %p  pBuffer = %p  error = %d",
                                             i,
                                             pResultHolder->bufferHolder[i].pStream,
                                             pResultHolder->bufferHolder[i].pBuffer,
                                             pResultHolder->bufferHolder[i].error);
                        }
                    }
                    for (UINT i = 0; i < MaxNumInputBuffers; i++)
                    {
                        if ((NULL != pResultHolder->inputbufferHolder[i].pStream) ||
                            (NULL != pResultHolder->inputbufferHolder[i].pBuffer))
                        {
                            CAMX_LOG_TO_FILE(fd, indent + 6, "inputbufferHolder[%u] - pStream = %p  pBuffer = %p ",
                                             i,
                                             pResultHolder->inputbufferHolder[i].pStream,
                                             pResultHolder->inputbufferHolder[i].pBuffer);
                        }
                    }
                }
            }
        }

        // Get the next result holder and see what's going on with it
        pNode = m_resultHolderList.NextNode(pNode);
    }

    if (CamxResultSuccess == result)
    {
        m_pResultLock->Unlock();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::DumpState
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::DumpState(
    INT     fd,
    UINT32  indent,
    SessionDumpFlag flag)
{
    BOOL isFlushing                             = CamxAtomicLoadU8(&m_aFlushStatus);
// Read from settings text file
    BOOL isPartialMetadataEnabled               = m_numMetadataResults > 1;

    static const CHAR* SessionDumpFlagStrings[] =
    { "Flush", "SigAbort", "ResetKMD", "ResetUMD", "ResetRecovery", "ChiContextDump" };

    CAMX_LOG_TO_FILE(fd, indent, "+-----------------------------------------------------------------------------------------+");
    CAMX_LOG_TO_FILE(fd, indent, "Session %p state:", this);
    CAMX_LOG_TO_FILE(fd, indent + 2, "reason                  = \"%s\"", SessionDumpFlagStrings[static_cast<UINT>(flag)]);
    CAMX_LOG_TO_FILE(fd, indent + 2, "numPipelines            = %u",   m_numPipelines);
    CAMX_LOG_TO_FILE(fd, indent + 2, "numRealtimePipelines    = %u",   m_numRealtimePipelines);
    CAMX_LOG_TO_FILE(fd, indent + 2, "partialMetadataEnabled  = %s",   Utils::BoolToString(isPartialMetadataEnabled));
    CAMX_LOG_TO_FILE(fd, indent + 2, "aDeviceInError          = %d",   CamxAtomicLoad32(&m_aDeviceInError));
    CAMX_LOG_TO_FILE(fd, indent + 2, "isFlushing              = %s",   Utils::BoolToString(isFlushing));
    CAMX_LOG_TO_FILE(fd, indent + 2, "isTorchWidgetSession    = %s",   Utils::BoolToString(m_isTorchWidgetSession));
    CAMX_LOG_TO_FILE(fd, indent + 2, "isRequestBatchingOn     = %s",   Utils::BoolToString(m_isRequestBatchingOn));
    CAMX_LOG_TO_FILE(fd, indent + 2, "usecaseNumBatchedFrames = %u",   m_usecaseNumBatchedFrames);
    CAMX_LOG_TO_FILE(fd, indent + 2, "livePendingRequests     = %u",   m_livePendingRequests);
    CAMX_LOG_TO_FILE(fd, indent + 2, "maxLivePendingRequests  = %u",   m_maxLivePendingRequests);
    CAMX_LOG_TO_FILE(fd, indent + 2, "sequenceId              = %u",   m_sequenceId);
    CAMX_LOG_TO_FILE(fd, indent + 2, "syncSequenceId          = %llu", m_syncSequenceId);
    CAMX_LOG_TO_FILE(fd, indent + 2, "requestQueueDepth       = %u",   m_requestQueueDepth);

    for (UINT32 i = 0; i < m_numPipelines; i++)
    {
        CAMX_LOG_TO_FILE(fd, indent+ 4,
                         "Pipeline_%s -- { cameraId = %u, requestBatchId = %llu, batchedFrameIndex = %u}",
                         m_pipelineData[i].pPipeline->GetPipelineIdentifierString(),
                         m_pipelineData[i].pPipelineDescriptor->cameraId,
                         m_requestBatchId[i],
                         m_batchedFrameIndex[i]);
    }

    // Dump the result holder list
    CAMX_LOG_TO_FILE(fd, indent, "\n+---------------------------------------------------------------------------------------+");
    CAMX_LOG_TO_FILE(fd, indent, "Result holder List:");
    DumpResultState(fd, indent + 2);

    // Dump pipeline info for this session
    CAMX_LOG_TO_FILE(fd, indent, "\n+---------------------------------------------------------------------------------------+");
    CAMX_LOG_TO_FILE(fd, indent, "Pipelines:");
    for (UINT i = 0; i < m_numPipelines; i++)
    {
        CAMX_LOG_TO_FILE(fd, indent + 2, "Pipeline %u:", i);
        m_pipelineData[i].pPipeline->DumpState(fd, indent + 2);
    }

    // Dump the session's request queue
    CAMX_LOG_TO_FILE(fd, indent, "\n+---------------------------------------------------------------------------------------+");
    CAMX_LOG_TO_FILE(fd, indent, "HAL3 RequestQueue");
    m_pRequestQueue->DumpState(fd, indent + 2);

    // Dump this session's DRQ
    CAMX_LOG_TO_FILE(fd, indent, "\n+---------------------------------------------------------------------------------------+");
    CAMX_LOG_TO_FILE(fd, indent, "DeferredRequestQueue");
    m_pDeferredRequestQueue->DumpState(fd, indent + 2);

    // Dump Job Family State
    CAMX_LOG_TO_FILE(fd, indent, "\n+---------------------------------------------------------------------------------------+");
    CAMX_LOG_TO_FILE(fd, indent, "ThreadManager");
    m_pThreadManager->DumpStateToFile(fd, indent + 2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::DumpDebugInfo()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::DumpDebugInfo(
    SessionDumpFlag flag)
{
    CAMX_LOG_DUMP(CamxLogGroupCore, "+----------------------------------------------------------+");
    switch (flag)
    {
        case SessionDumpFlag::Flush:
            CAMX_LOG_DUMP(CamxLogGroupCore, "+      CALLING SESSION DUMP %p FOR FLUSH         +", this);
            break;
        case SessionDumpFlag::SigAbort:
            CAMX_LOG_DUMP(CamxLogGroupCore, "+      CALLING SESSION DUMP %p FOR SIGABORT      +", this);
            break;
        case SessionDumpFlag::ResetKMD:
            CAMX_LOG_DUMP(CamxLogGroupCore, "+      CALLING SESSION DUMP %p FOR RESET-KMD     +", this);
            break;
        case SessionDumpFlag::ResetUMD:
            CAMX_LOG_DUMP(CamxLogGroupCore, "+      CALLING SESSION DUMP %p FOR RESET-UMD     +", this);
            break;
        case SessionDumpFlag::ResetRecovery:
            CAMX_LOG_DUMP(CamxLogGroupCore, "+      CALLING SESSION DUMP %p FOR RESET-Recovery     +", this);
            break;
        default:
            CAMX_LOG_DUMP(CamxLogGroupCore, "+            CALLING SESSION DUMP %p             +", this);
            break;
    }

    CAMX_LOG_DUMP(CamxLogGroupCore, "m_numPipelines: %d", m_numPipelines);
    CAMX_LOG_DUMP(CamxLogGroupCore, "m_livePendingRequests: %d", m_livePendingRequests);
    CAMX_LOG_DUMP(CamxLogGroupCore, "m_maxLivePendingRequests: %d", m_maxLivePendingRequests);
    CAMX_LOG_DUMP(CamxLogGroupCore, "m_requestQueueDepth: %d", m_requestQueueDepth);
    CAMX_LOG_DUMP(CamxLogGroupCore, "m_aFlushStatus: %d", CamxAtomicLoadU8(&m_aFlushStatus));
    CAMX_LOG_DUMP(CamxLogGroupCore, "m_usecaseNumBatchedFrames: %d", m_usecaseNumBatchedFrames);

    if ((NULL != m_resultHolderList.Head()) && (NULL != m_resultHolderList.Tail()))
    {
        SessionResultHolder* pSessionResultHolderHead =
            reinterpret_cast<SessionResultHolder*>(m_resultHolderList.Head()->pData);
        SessionResultHolder* pSessionResultHolderTail =
            reinterpret_cast<SessionResultHolder*>(m_resultHolderList.Tail()->pData);

        if ((NULL != pSessionResultHolderHead) && (NULL != pSessionResultHolderTail))
        {
            CAMX_LOG_DUMP(CamxLogGroupCore, "+------------------------------------------------------------------+");
            CAMX_LOG_DUMP(CamxLogGroupCore, "+ Waiting for all results  minResult:%d  maxRequest:%d",
                pSessionResultHolderHead->resultHolders[0].sequenceId,
                pSessionResultHolderTail->resultHolders[pSessionResultHolderTail->numResults - 1].sequenceId);
            CAMX_LOG_DUMP(CamxLogGroupCore, "+------------------------------------------------------------------+");
            CAMX_LOG_DUMP(CamxLogGroupCore, "+ Stuck on Sequence Id: %d Request Id: %d",
                pSessionResultHolderHead->resultHolders[0].sequenceId,
                pSessionResultHolderHead->resultHolders[0].requestId);
        }
    }

    for (UINT i = 0; i < m_numPipelines; i++)
    {
        if (NULL != m_pipelineData[i].pPipeline)
        {
            m_pipelineData[i].pPipeline->DumpDebugInfo();
        }
    }

    CamxResult result = m_pResultLock->TryLock();

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_DUMP(CamxLogGroupCore, "WARNING: Lock failed with status: %d.  Stopping dump",
                         result);
        return;
    }

    LightweightDoublyLinkedListNode*    pNode           = m_resultHolderList.Head();
    ResultHolder*                       pResultHolder   = NULL;

    while (NULL != pNode)
    {
        CAMX_ASSERT(NULL != pNode->pData);

        if (NULL != pNode->pData)
        {
            SessionResultHolder* pSessionResultHolder = reinterpret_cast<SessionResultHolder*>(pNode->pData);

            for (UINT32 i = 0; i < pSessionResultHolder->numResults; i++)
            {
                pResultHolder = &pSessionResultHolder->resultHolders[i];
                m_pDeferredRequestQueue->DumpDebugInfo(pResultHolder->requestId, TRUE);
            }
        }

        // Get the next result holder and see what's going on with it
        pNode = m_resultHolderList.NextNode(pNode);
    }

    // Dump Job Family State
    m_pThreadManager->DumpStateToLog();

    if (CamxResultSuccess == result)
    {
        m_pResultLock->Unlock();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::PrepareAndDispatchRequestError
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::PrepareAndDispatchRequestError(
    SessionCaptureRequest*  pSessionRequests)
{
    CAMX_ASSERT(NULL != pSessionRequests);

    UINT maxNumBatchedFrames = 0;
    UINT maxNumInputBuffers  = 0;
    UINT maxNumOutputBuffers = 0;

    for (UINT32 request = 0; request < pSessionRequests->numRequests; request++)
    {
        CaptureRequest* pRequest = static_cast<CaptureRequest*>(&pSessionRequests->requests[request]);
        UINT32          numBatchedFrames = pRequest->numBatchedFrames;

        if (NULL != pRequest)
        {
            maxNumBatchedFrames = Utils::MaxUINT(maxNumBatchedFrames, pRequest->numBatchedFrames);
            for (UINT32 index = 0; index < numBatchedFrames; index++)
            {
                maxNumInputBuffers = Utils::MaxUINT(maxNumInputBuffers, pRequest->pStreamBuffers[index].numInputBuffers);
                maxNumOutputBuffers = Utils::MaxUINT(maxNumOutputBuffers, pRequest->pStreamBuffers[index].numOutputBuffers);
            }
        }
    }

    // Allocate memory to be able to return the result
    ChiCaptureResult* pCaptureResults =
        static_cast<ChiCaptureResult*>(CAMX_CALLOC(maxNumBatchedFrames * sizeof(ChiCaptureResult)));

    ChiStreamBuffer* pInputBuffers  = static_cast<ChiStreamBuffer*>(CAMX_CALLOC(maxNumInputBuffers * sizeof(ChiStreamBuffer)));
    ChiStreamBuffer* pOutputBuffers = static_cast<ChiStreamBuffer*>(CAMX_CALLOC(maxNumOutputBuffers * sizeof(ChiStreamBuffer)));

    if ((NULL  == pCaptureResults) ||
        ((NULL == pInputBuffers)   && (0 < maxNumInputBuffers)) ||
        ((NULL == pOutputBuffers)  && (0 < maxNumOutputBuffers)))
    {
        CAMX_LOG_ERROR(CamxLogGroupCore,
                       "Failed to allocate enough memory. Allocated the following pointers - pCaptureResults: "
                       " %p pInputBuffers %p pOutputBuffers %p",  pCaptureResults, pInputBuffers, pOutputBuffers);
    }
    else
    {
        for (UINT32 request = 0; request < pSessionRequests->numRequests; request++)
        {
            CaptureRequest* pRequest         = static_cast<CaptureRequest*>(&pSessionRequests->requests[request]);

            if (NULL != pRequest)
            {
                CAMX_LOG_VERBOSE(CamxLogGroupCore, "Dispatching Request Error for request id %llu",
                    pRequest->pStreamBuffers[request].originalFrameworkNumber);

                for (UINT32 index = 0; index < pRequest->numBatchedFrames; index++)
                {
                    pCaptureResults[index].frameworkFrameNum  = 0;
                    pCaptureResults[index].numOutputBuffers   = 0;
                    pCaptureResults[index].numPartialMetadata = 0;
                    pCaptureResults[index].pInputMetadata     = NULL;
                    pCaptureResults[index].pOutputMetadata    = NULL;
                    pCaptureResults[index].pInputBuffer       = pInputBuffers;
                    pCaptureResults[index].pOutputBuffers     = pOutputBuffers;
                }

                for (UINT32 index = 0; index < pRequest->numBatchedFrames; index++)
                {

                    // Prepare and Dispatch Error immediately
                    ChiMessageDescriptor* pNotify = GetNotifyMessageDescriptor();
                    pNotify->messageType = ChiMessageTypeError;
                    pNotify->message.errorMessage.errorMessageCode = static_cast<ChiErrorMessageCode>(MessageCodeRequest);
                    pNotify->pPrivData = reinterpret_cast<CHIPRIVDATA*>(pRequest->pPrivData);
                    pNotify->message.errorMessage.pErrorStream = NULL;
                    pNotify->message.errorMessage.frameworkFrameNum =
                        static_cast<UINT32>(pRequest->pStreamBuffers[index].originalFrameworkNumber);

                    DispatchNotify(pNotify);

                    for (UINT32 buffer = 0; buffer < pRequest->pStreamBuffers[index].numOutputBuffers; buffer++)
                    {
                        CAMX_LOG_INFO(CamxLogGroupCore, "Returning error for pending  output buffers - %llu",
                            pRequest->pStreamBuffers[index].originalFrameworkNumber);

                        ChiStreamBuffer* pOutputBuffer = &pRequest->pStreamBuffers[index].outputBuffers[buffer];
                        pOutputBuffer->size = sizeof(ChiStreamBuffer);
                        pOutputBuffer->bufferStatus = BufferStatusError;
                        pOutputBuffer->releaseFence.valid = FALSE;

                        // Copy the stream to be delivered to the framework
                        ChiStreamBuffer* pStreamBuffer =
                            // NOWHINE CP036a: Google API requires const type
                            const_cast<ChiStreamBuffer*>(&pCaptureResults[index].pOutputBuffers[buffer]);
                        Utils::Memcpy(pStreamBuffer, pOutputBuffer, sizeof(ChiStreamBuffer));
                    }

                    for (UINT32 buffer = 0; buffer < pRequest->pStreamBuffers[index].numInputBuffers; buffer++)
                    {
                        CAMX_LOG_INFO(CamxLogGroupCore, "Returning error for pending input buffers - %llu",
                            pRequest->pStreamBuffers[index].originalFrameworkNumber);

                        ChiStreamBuffer* pInputBuffer = &pRequest->pStreamBuffers[index].inputBufferInfo[buffer].inputBuffer;
                        pInputBuffer->size = sizeof(ChiStreamBuffer);
                        pInputBuffer->releaseFence.valid = FALSE;
                        pInputBuffer->bufferStatus = BufferStatusError;

                        ChiStreamBuffer* pStreamBuffer =
                            // NOWHINE CP036a: Google API requires const type
                            const_cast<ChiStreamBuffer*>(&pCaptureResults[index].pInputBuffer[buffer]);
                        Utils::Memcpy(pStreamBuffer, pInputBuffer, sizeof(ChiStreamBuffer));
                    }

                    pCaptureResults[index].numOutputBuffers   = pRequest->pStreamBuffers[index].numOutputBuffers;
                    pCaptureResults[index].pResultMetadata    = NULL;
                    pCaptureResults[index].numPartialMetadata = 1;
                    pCaptureResults[index].pPrivData          = reinterpret_cast<CHIPRIVDATA*>(pRequest->pPrivData);
                    pCaptureResults[index].frameworkFrameNum  =
                        static_cast<UINT32>(pRequest->pStreamBuffers[index].originalFrameworkNumber);
                }

                m_flushInfo.lastFlushRequestId[pRequest->pipelineIndex] = pRequest->requestId;
                DispatchResults(pCaptureResults, 1);
            }
        }
    }

    // If we've gotten any buffer ready for this result then we can accept another
    m_pLivePendingRequestsLock->Lock();
    if (0 < m_livePendingRequests)
    {
        m_livePendingRequests--;
        m_pWaitLivePendingRequests->Signal();
    }
    m_pLivePendingRequestsLock->Unlock();

    CAMX_FREE(pCaptureResults);
    CAMX_FREE(pInputBuffers);
    CAMX_FREE(pOutputBuffers);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::FlushHALQueue
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::FlushHALQueue()
{
    m_pRequestLock->Lock();

    while (FALSE == m_pRequestQueue->IsEmpty())
    {
        SessionCaptureRequest* pSessionRequests = static_cast<SessionCaptureRequest*>(m_pRequestQueue->Dequeue());

        if (NULL != pSessionRequests)
        {
            PrepareAndDispatchRequestError(pSessionRequests);
            m_pRequestQueue->Release(pSessionRequests);
        }
    }

    m_pRequestLock->Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::QueryMetadataInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::QueryMetadataInfo(
    const CHIPIPELINEHANDLE hPipelineDescriptor,
    const UINT32            maxPublishTagArraySize,
    UINT32*                 pPublishTagArray,
    UINT32*                 pPublishTagCount,
    UINT32*                 pPartialPublishTagCount,
    UINT32*                 pMaxNumMetaBuffers)
{
    UINT32     index = 0;
    CamxResult result = CamxResultEInvalidArg;

    // input pipelineIndex not really match the index recorded by Session, so use Descriptor to find it.
    for (index = 0; index < m_numPipelines; index++)
    {
        if (hPipelineDescriptor == m_pipelineData[index].pPipelineDescriptor)
        {
            Pipeline* pPipeline = m_pipelineData[index].pPipeline;

            if (NULL != pPipeline)
            {
                result = pPipeline->QueryMetadataInfo(
                    maxPublishTagArraySize,
                    pPublishTagArray,
                    pPublishTagCount,
                    pPartialPublishTagCount,
                    pMaxNumMetaBuffers);
            }
            break;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::ResetMetabufferPendingQueue
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::ResetMetabufferPendingQueue()
{
    Utils::Memset(&m_pendingMetabufferDoneQueue, 0x0, sizeof(m_pendingMetabufferDoneQueue));

    for (UINT32 index = 0; index < m_numPipelines; ++index)
    {
        PipelineMetaBufferDoneEntry* pEntry = &m_pendingMetabufferDoneQueue.pipelineEntry[index];
        Pipeline*                    pPipeline = m_pipelineData[index].pPipeline;

        pEntry->pPipeline = pPipeline;
        // the delay in bacth needs to be updated considering skip
        pEntry->maxDelay =
            (1 == m_usecaseNumBatchedFrames) ? pPipeline->GetMetaBufferDelay() : pPipeline->GetMetaBufferDelay()* 2;
        pEntry->oldestRequestId = 1;
        pEntry->latestRequestId = 1;

        CAMX_LOG_INFO(CamxLogGroupHAL, "MetaBuffer done Maximum delay for %s is %u",
            pEntry->pPipeline->GetPipelineIdentifierString(),
            pEntry->maxDelay);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::ProcessMetabufferPendingQueue
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::ProcessMetabufferPendingQueue()
{
    m_pPendingMBQueueLock->Lock();

    for (UINT32 sequenceId = m_pendingMetabufferDoneQueue.oldestSequenceId;
        sequenceId <= m_pendingMetabufferDoneQueue.latestSequenceId;
        ++sequenceId)
    {
        UINT32 index = sequenceId % MaxQueueDepth;

        if ((MetaBufferDoneMetaReady | MetaBufferDoneBufferReady) == m_pendingMetabufferDoneQueue.pendingMask[index])
        {
            UINT32 pipelineIndex = m_pendingMetabufferDoneQueue.pipelineIndex[index];
            UINT32 curRequestId = m_pendingMetabufferDoneQueue.requestId[index];

            PipelineMetaBufferDoneEntry* pEntry = &(m_pendingMetabufferDoneQueue.pipelineEntry[pipelineIndex]);

            pEntry->isReady[curRequestId % MaxQueueDepth] = TRUE;

            // update latest request Id
            if (curRequestId > pEntry->latestRequestId)
            {
                pEntry->latestRequestId = curRequestId;
            }

            // Sending all the ready results for the pipeline
            CAMX_LOG_INFO(CamxLogGroupHAL, "Pending Queue pipeline %s request %u old = %u last %u delay %u",
                pEntry->pPipeline->GetPipelineIdentifierString(),
                curRequestId,
                pEntry->oldestRequestId,
                pEntry->latestRequestId,
                pEntry->maxDelay);

            for (UINT32 requestId = pEntry->oldestRequestId;
                requestId + pEntry->maxDelay <= pEntry->latestRequestId; ++requestId)
            {
                // NOWHINE CP010: used references for the parameters which can never be NULL
                BOOL& isReady = pEntry->isReady[requestId % MaxQueueDepth];

                if (TRUE == isReady)
                {
                    pEntry->pPipeline->ProcessMetadataBufferDone(requestId);
                    isReady = FALSE;
                }

                if (requestId == pEntry->oldestRequestId)
                {
                    pEntry->oldestRequestId++;
                }
            }

            if (sequenceId == m_pendingMetabufferDoneQueue.oldestSequenceId)
            {
                m_pendingMetabufferDoneQueue.oldestSequenceId++;
            }

            m_pendingMetabufferDoneQueue.pendingMask[index] = 0;
        }
    }
    m_pPendingMBQueueLock->Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::DumpDebugData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::DumpDebugData(
    SessionDumpFlag flag)
{
    CHAR         dumpFilename[256];
    CamxDateTime systemDateTime;
    FILE*        pFile = NULL;

    OsUtils::GetDateTime(&systemDateTime);
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/%04d%02d%02d_%02d%02d%02d%_session_debug_dump.txt",
        ConfigFileDirectory,
        systemDateTime.year + 1900,
        systemDateTime.month + 1,
        systemDateTime.dayOfMonth,
        systemDateTime.hours,
        systemDateTime.minutes,
        systemDateTime.seconds);

    pFile = CamX::OsUtils::FOpen(dumpFilename, "w");

    if (NULL != pFile)
    {
        INT fileDesc = CamX::OsUtils::FileNo(pFile);

        CAMX_LOG_DUMP(CamxLogGroupCore,
            "Session dumped, please check the session dump at %s",
            dumpFilename);

        DumpState(fileDesc, 0, flag);
        CamX::OsUtils::FClose(pFile);
    }
    else
    {
        CAMX_LOG_DUMP(CamxLogGroupCore, "File failed to open, could not dump session");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::DumpSessionState
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::DumpSessionState(
    SessionDumpFlag flag)
{
    UINT numResults = 0;

    if (CamxResultSuccess == m_pResultLock->TryLock())
    {
        numResults = m_resultHolderList.NumNodes();
        m_pResultLock->Unlock();
    }

    if (0 != numResults)
    {
        if (CamxResultSuccess != m_pSessionDumpLock->TryLock())
        {
            CAMX_LOG_INFO(CamxLogGroupHAL, "Unable to Get Dump Lock, no session dump happening");
        }
        else
        {
            if (static_cast<BOOL>(CamxAtomicLoadU8(&m_aSessionDumpComplete) != TRUE))
            {
                CamxAtomicStoreU8(&m_aSessionDumpComplete, TRUE);
                const StaticSettings* pStaticSettings = m_pChiContext->GetStaticSettings();
                if (TRUE == pStaticSettings->sessionDumpToFile)
                {
                    DumpDebugData(flag);
                }
                if (TRUE == pStaticSettings->sessionDumpToLog)
                {
                    if ((SessionDumpFlag::Flush == flag) && (TRUE == pStaticSettings->sessionDumpForFlush))
                    {
                        DumpDebugInfo(flag);
                    }
                    else if ((SessionDumpFlag::ResetRecovery == flag) && (TRUE == pStaticSettings->sessionDumpForRecovery))
                    {
                        DumpDebugInfo(flag);
                    }
                    else
                    {
                        DumpDebugInfo(flag);
                    }
                }
                CAMX_LOG_INFO(CamxLogGroupHAL, "Session Dump complete");
            }
            m_pSessionDumpLock->Unlock();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::SetRealtimePipeline
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::SetRealtimePipeline(
    SessionCreateData* pCreateData)
{
    m_isRealTime = FALSE;

    for (UINT i = 0; i < pCreateData->numPipelines; i++)
    {
        ChiPipelineInfo*      pPipelineInfo       = &pCreateData->pPipelineInfo[i];
        PipelineDescriptor*   pPipelineDescriptor = reinterpret_cast<PipelineDescriptor*>(pPipelineInfo->hPipelineDescriptor);

        if (TRUE == pPipelineDescriptor->flags.isRealTime)
        {
            m_isRealTime = TRUE;
            break;
        }
    }

    return m_isRealTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::CheckActiveSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::CheckActiveSensor(
    SessionCreateData* pCreateData)
{
    BOOL         isActiveSensor = FALSE;
    CSLHandle    hCSLHandle     = CSLInvalidHandle;
    CamxResult   result;

    result = GetActiveCSLSession(pCreateData, &hCSLHandle);

    if (CSLInvalidHandle != hCSLHandle)
    {
        isActiveSensor = TRUE;
    }

    return isActiveSensor;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::GetActiveCSLSession
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::GetActiveCSLSession(
    SessionCreateData* pCreateData,
    CSLHandle*         phCSLSession)
{
    CamxResult           result         = CamxResultEFailed;
    const HwEnvironment* pHWEnvironment = HwEnvironment::GetInstance();

    *phCSLSession = CSLInvalidHandle;

    CAMX_ASSERT(NULL != pCreateData);
    CAMX_ASSERT(NULL != pHWEnvironment);

    if (NULL != pHWEnvironment)
    {
        UINT32 cameraID = 0;
        for (UINT i = 0; i < pCreateData->numPipelines; i++)
        {
            ChiPipelineInfo*    pPipelineInfo = &pCreateData->pPipelineInfo[i];
            PipelineDescriptor* pPipelineDescriptor = reinterpret_cast<PipelineDescriptor*>(pPipelineInfo->hPipelineDescriptor);
            HwCameraInfo        cameraInfo;

            cameraID = pPipelineDescriptor->cameraId;
            result = pHWEnvironment->GetCameraInfo(cameraID, &cameraInfo);

            if ((CamxResultSuccess == result) && (1 != pCreateData->flags.u.IsSSM) &&
                (CSLInvalidHandle != cameraInfo.pSensorCaps->hCSLSession[0]))
            {
                CAMX_LOG_CONFIG(CamxLogGroupCore, "cameraId=%d, session=%p, cslsession=%p", cameraID, this,
                                cameraInfo.pSensorCaps->hCSLSession[0]);
                *phCSLSession = cameraInfo.pSensorCaps->hCSLSession[0];
                break;
            }
            else if ((CamxResultSuccess == result) && (1 == pCreateData->flags.u.IsSSM) &&
                     (CSLInvalidHandle != cameraInfo.pSensorCaps->hCSLSession[1]))
            {
                CAMX_LOG_CONFIG(CamxLogGroupCore, "cameraId=%d, session=%p, cslsession=%p", cameraID, this,
                                cameraInfo.pSensorCaps->hCSLSession[1]);
                *phCSLSession = cameraInfo.pSensorCaps->hCSLSession[1];
                break;
            }
        }
        if (CSLInvalidHandle == *phCSLSession)
        {
            CAMX_LOG_WARN(CamxLogGroupCore, "Was not able to retrieve camera info for cameraId=%d, session=%p", cameraID, this);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::GetFlushResponseTime
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 Session::GetFlushResponseTime()
{
    UINT64 pipelineWaittime = 0;
    UINT64 totalWaitTime    = 0;
    UINT64 paddingTime      = m_pChiContext->GetStaticSettings()->sessionResponseTimePadding;
    UINT64 maxWaitTime      = m_pChiContext->GetStaticSettings()->sessionMaxFlushWaitTime;

    paddingTime = Utils::MaxUINT64(paddingTime, paddingTime*m_livePendingRequests);

    for (UINT index = 0; index < m_numPipelines; index++)
    {
        pipelineWaittime = m_pipelineData[index].pPipeline->GetFlushResponseTimeInMs();
        totalWaitTime    = Utils::MaxUINT64(pipelineWaittime, totalWaitTime);
    }

    return Utils::MinUINT64(maxWaitTime, totalWaitTime + paddingTime) + m_longExposureTimeout;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SetupRequestData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult SetupRequestData(
    CaptureRequest* pRequest,
    PipelineProcessRequestData* pOutRequestData)
{
    // Pipeline to process this Request
    CamxResult result                     = CamxResultSuccess;
    pOutRequestData->pCaptureRequest      = pRequest;
    pOutRequestData->pPerBatchedFrameInfo =
        static_cast<PerBatchedFrameInfo*>(CAMX_CALLOC(sizeof(PerBatchedFrameInfo) * pRequest->numBatchedFrames));

    if (NULL == pOutRequestData->pPerBatchedFrameInfo)
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Failed to allocate memory for pPerBatchedFrameInfo!");
        result = CamxResultENoMemory;
    }
    else
    {
        CAMX_ASSERT(NULL != pOutRequestData->pPerBatchedFrameInfo);

        DetermineActiveStreams(pOutRequestData);

        for (UINT batchIndex = 0; batchIndex < pRequest->numBatchedFrames; batchIndex++)
        {
            CAMX_ASSERT(pOutRequestData->pPerBatchedFrameInfo[batchIndex].activeStreamIdMask > 0);
        }

        // Trigger the fences on the input buffers
        for (UINT batchIndex = 0; batchIndex < pRequest->numBatchedFrames; batchIndex++)
        {
            UINT32 numInputBuffers = pRequest->pStreamBuffers[batchIndex].numInputBuffers;

            for (UINT32 buffer = 0; buffer < numInputBuffers; buffer++)
            {
                StreamInputBufferInfo* pInputBufferInfo =
                    &(pRequest->pStreamBuffers[batchIndex].inputBufferInfo[buffer]);
                CSLFence               hInternalCSLFence = pInputBufferInfo->fence;
                CSLFence               hExternalCSLFence = CSLInvalidHandle;

                if ((TRUE == pInputBufferInfo->inputBuffer.acquireFence.valid) &&
                    (ChiFenceTypeInternal == pInputBufferInfo->inputBuffer.acquireFence.type))
                {
                    hExternalCSLFence =
                        reinterpret_cast<ChiFence*>(pInputBufferInfo->inputBuffer.acquireFence.hChiFence)->hFence;
                }

                if ((FALSE == IsValidCSLFence(hExternalCSLFence)) &&
                    (TRUE == IsValidCSLFence(hInternalCSLFence)))
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupCore,
                        "pRequest->streamBuffers[%u].inputBufferInfo[%u].inputBuffer.phBuffer:%p "
                        "hInternalCSLFence:%llu Signalled",
                        batchIndex,
                        buffer,
                        pRequest->pStreamBuffers[batchIndex].inputBufferInfo[buffer].inputBuffer.bufferInfo.phBuffer,
                        hInternalCSLFence);

                    CSLFenceSignal(hInternalCSLFence, CSLFenceResultSuccess);
                }
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::IsDoneProcessing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL Session::IsDoneProcessing()
{
    BOOL isDone = TRUE;

    // make sure all the pipelines have caught up their processing with the session (completed requests we need them to)
    for (UINT32 pipelineIndex = 0; pipelineIndex < m_numPipelines; pipelineIndex++)
    {
        Pipeline* pPipeline = m_pipelineData[pipelineIndex].pPipeline;

        if ((pPipeline->GetPipelineStatus() == PipelineStatus::STREAM_ON) &&
            (FALSE == pPipeline->AreAllNodesDone()))
        {
            CAMX_LOG_INFO(CamxLogGroupCore, "Session %p is not done processing, Waiting for pipeline: %s:%u",
                          this, m_pipelineData[pipelineIndex].pPipelineDescriptor->pipelineName,
                          m_pipelineData[pipelineIndex].pPipeline->GetPipelineId());

            isDone = FALSE;
            break;
        }
    }

    if (TRUE == isDone)
    {
        m_pLivePendingRequestsLock->Lock();
        if (m_livePendingRequests != 0)
        {
            isDone = FALSE;
        }
        m_pLivePendingRequestsLock->Unlock();
    }

    if (TRUE == isDone)
    {
        m_pResultHolderListLock->Lock();
        UINT numNodesInResHolder = m_resultHolderList.NumNodes();
        m_pResultHolderListLock->Unlock();

        if (0 != numNodesInResHolder)
        {
            isDone = FALSE;
            CAMX_LOG_INFO(CamxLogGroupCore,
                          "Session %p is not done processing, Waiting for results. m_resultHolderList.NumNodes: %u",
                          this, m_resultHolderList.NumNodes());
        }
    }

    return isDone;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::NotifyProcessingDone
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID Session::NotifyProcessingDone()
{

    // Check if we can tell the flush thread that it can continue
    if (TRUE == IsDoneProcessing())
    {
        CAMX_LOG_INFO(CamxLogGroupCore, "Session %p is done processing Waking up flush thread", this);
        m_pResultLock->Lock();
        m_pWaitAllResultsAvailableSignaled = TRUE;
        m_pWaitAllResultsAvailable->Signal();
        m_pResultLock->Unlock();
    }
    else if (m_livePendingRequests != 0)
    {
        CAMX_LOG_INFO(CamxLogGroupCore, "Session %p is not done processing, Waiting for m_livePendingRequests", this);
        m_pResultLock->Lock();
        m_pWaitAllResultsAvailableSignaled = FALSE;
        m_pResultLock->Unlock();
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::CheckAndSyncLinks
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::CheckAndSyncLinks(
    SessionCaptureRequest* pSessionRequest)
{
    CamxResult           result         = CamxResultSuccess;

    // 1. Check if all the pipelines in this request have links synced.
    // If not, need to sync the links.
    BOOL isSyncLinksNeeded = FALSE;
    for (UINT requestIndex = 0; requestIndex < pSessionRequest->numRequests; requestIndex++)
    {
        CaptureRequest* pRequest = &(pSessionRequest->requests[requestIndex]);
        // If any one of the pipeline's link is not synced, then we need to do a new sync.
        if (FALSE == m_isInSyncedLinks[pRequest->pipelineIndex])
        {
            isSyncLinksNeeded = TRUE;
            break;
        }
    }

    // 2. Check, if there is any change in the number of links synced and the number of pipeline requests.
    // If yes, then there should be change in the links that need to be synced.
    if (m_numLinksSynced != pSessionRequest->numRequests)
    {
        isSyncLinksNeeded = TRUE;
        CAMX_LOG_CONFIG(CamxLogGroupHAL, "isSyncLinksNeeded %d numRequests %d m_numLinksSynced %d",
            isSyncLinksNeeded, pSessionRequest->numRequests, m_numLinksSynced);
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupHAL, "isSyncLinksNeeded %d numRequests %d m_numLinksSynced %d",
            isSyncLinksNeeded, pSessionRequest->numRequests, m_numLinksSynced);

    }

    // Now, sync the necessary links,
    if (TRUE == isSyncLinksNeeded)
    {
        UINT32        numLinks   = 0;
        CSLLinkHandle hLinkHandles[MaxRealTimePipelines];

        // Reset m_isInSyncedLinks of all pipelines to FALSE
        Utils::Memset(m_isInSyncedLinks, 0, sizeof(m_isInSyncedLinks));

        // Get the link handles of all the necessary pipelines.
        for (UINT requestIndex = 0; requestIndex < pSessionRequest->numRequests; requestIndex++)
        {
            CaptureRequest* pRequest  = &(pSessionRequest->requests[requestIndex]);
            Pipeline* pPipelineObject = m_pipelineData[pRequest->pipelineIndex].pPipeline;

            if (NULL != pPipelineObject)
            {
                if (0 == *pPipelineObject->GetCSLLink())
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupCore, "Link is not yet created for pipeline %d",
                        pRequest->pipelineIndex);
                }
            }
            hLinkHandles[numLinks++] = *pPipelineObject->GetCSLLink();
            m_isInSyncedLinks[pRequest->pipelineIndex] = TRUE;
        }

        // sync the links
        if ((numLinks > 1) && (numLinks <= MaxSyncLinkCount))
        {
            result = CSLSyncLinks(GetCSLSession(),
                hLinkHandles, numLinks,
                hLinkHandles[0], CSLSyncLinkModeSync);

            if (CamxResultSuccess == result)
            {
                m_numLinksSynced = numLinks;
                CAMX_LOG_ERROR(CamxLogGroupCore, "CSLSyncLinks Success! links (%d, %d)",
                    hLinkHandles[0], hLinkHandles[1]);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupCore, "CSLSyncLinks Failed! links (%d, %d)",
                    hLinkHandles[0], hLinkHandles[1]);
            }
        }

        // Reset m_isInSyncedLinks of all pipelines to FALSE, if something fails while syncing.
        if (CamxResultSuccess != result)
        {
            m_numLinksSynced = 0;
            Utils::Memset(m_isInSyncedLinks, 0, sizeof(m_isInSyncedLinks));
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::SyncProcessCaptureRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::SyncProcessCaptureRequest(
    const ChiPipelineRequest* pPipelineRequests,
    UINT32*                   pPipelineIndexes)
{
    CamxResult result                                           = CamxResultSuccess;
    UINT       numOfRealtimePipelines                           = 0;
    UINT       realTimePipelineIndex[MaxPipelinesPerSession]    = {};

    // Handle real-time session with HW sync enabled; otherwise nothing to do here
    if ((TRUE == IsRealtimeSession()) && (TRUE == HwEnvironment::GetInstance()->GetStaticSettings()->enableSensorHWSync))
    {
        // Get the number of real time request
        for (UINT i = 0; i < pPipelineRequests->numRequests; i++)
        {
            UINT pipelineIndex = pPipelineIndexes[i];
            if (TRUE == m_pipelineData[pipelineIndex].pPipeline->IsRealTime())
            {
                realTimePipelineIndex[numOfRealtimePipelines++] = i;
            }
        }

        const ChiCaptureRequest* pCaptureRequest;
        StreamStatus             streamStatus = StreamStatus::NON_SYNC_STREAMING;
        if (1 == numOfRealtimePipelines)
        {
            if (PipelineStatus::STREAM_ON == m_pipelineData[realTimePipelineIndex[0]].pPipeline->GetPipelineStatus())
            {
                // If there is only one real time pipeline request then we should not be doing any sync
                pCaptureRequest = &(pPipelineRequests->pCaptureRequests[realTimePipelineIndex[0]]);
                // NOWHINE CP036a: exception as we explictly need to modify the request to add the addiotonal info
                SetMultiCameraSync(const_cast<ChiCaptureRequest*>(pCaptureRequest), FALSE);
                SetSyncStreamStatus(realTimePipelineIndex[0], StreamStatus::NON_SYNC_STREAMING);
            }
            else
            {
                CAMX_LOG_INFO(CamxLogGroupCore, "Pipeline is Not streamed on");
            }
        }
        else if (2 == numOfRealtimePipelines)
        {
            BOOL bSync              = FALSE;
            BOOL enableAELock       = FALSE;
            UINT pipelineIndex      = 0;

            // If there are two real time requests and both pipelines has been streamed on then we should do sync
            if ((PipelineStatus::STREAM_ON == m_pipelineData[realTimePipelineIndex[0]].pPipeline->GetPipelineStatus()) &&
                (PipelineStatus::STREAM_ON == m_pipelineData[realTimePipelineIndex[1]].pPipeline->GetPipelineStatus()))
            {
                streamStatus    = StreamStatus::SYNC_STREAMING;
                bSync           = TRUE;
            }
            else
            {
                CAMX_LOG_INFO(CamxLogGroupCore, "Both Pipelines are not Streamed on and we are in Non-Sync Streaming mode");
                streamStatus    = StreamStatus::NON_SYNC_STREAMING;
                enableAELock    = TRUE;
            }

            if (TRUE == enableAELock)
            {
                UINT   aeLockFrames     = HwEnvironment::GetInstance()->GetStaticSettings()->numberOfAELockFrames;
                UINT64 startRequestID   = 0;
                UINT64 stopRequestID    = 0;

                for (UINT i = 0; i < numOfRealtimePipelines; i++)
                {
                    pipelineIndex   = realTimePipelineIndex[i];
                    startRequestID  = m_requestBatchId[pipelineIndex];
                    stopRequestID   = startRequestID + aeLockFrames;
                    if (startRequestID == 0)
                    {
                        // Request ID has not been initialized yet and request ID always starts from 1
                        startRequestID++;
                    }
                    SetAELockRange(pipelineIndex, startRequestID, stopRequestID);
                }
            }

            for (UINT i = 0; i < numOfRealtimePipelines; i++)
            {
                pipelineIndex   = realTimePipelineIndex[i];
                pCaptureRequest = &(pPipelineRequests->pCaptureRequests[pipelineIndex]);
                // NOWHINE CP036a: exception as we explictly need to modify the request to add the addiotonal info
                SetMultiCameraSync(const_cast<ChiCaptureRequest*>(pCaptureRequest), bSync);
                SetSyncStreamStatus(pipelineIndex, streamStatus);
            }
        }
        else if (3 <= numOfRealtimePipelines)
        {
            CAMX_LOG_ERROR(CamxLogGroupCore, "More than 2 real time pipeline request. How to handle?");
            result = CamxResultEFailed;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::SetMultiCameraSync
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::SetMultiCameraSync(
    ChiCaptureRequest* pCaptureRequest,
    BOOL enable)
{
    CamxResult      result                      = CamxResultSuccess;
    MetaBuffer*     pInputMetabuffer            = reinterpret_cast<MetaBuffer*>(pCaptureRequest->pInputMetadata);
    SyncModeInfo    modeInfo;

    modeInfo.isSyncModeEnabled  = enable;
    result                      = pInputMetabuffer->SetTag(m_sessionSync.syncModeTagID, &modeInfo, 1, sizeof(SyncModeInfo));
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Failed to set Sync");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Session::SetAELockRange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Session::SetAELockRange(
    UINT pipelineIndex,
    UINT64 startRequestID,
    UINT64 stopRequestID)
{
    MetadataPool*   pPerUsecasePool      = m_pipelineData[pipelineIndex].pPipeline->GetPerFramePool(PoolType::PerUsecase);
    MetadataSlot*   pUsecasePoolSlot     = pPerUsecasePool->GetSlot(0);
    CamxResult      result;

    result = pUsecasePoolSlot->SetMetadataByTag(PropertyIDUsecaseAESyncStartLockTagID, &startRequestID, 1, "camx_session");

    if (result == CamxResultSuccess)
    {
        result = pUsecasePoolSlot->SetMetadataByTag(PropertyIDUsecaseAESyncStopLockTagID, &stopRequestID, 1, "camx_session");
    }

    if (result == CamxResultSuccess)
    {
        result = pUsecasePoolSlot->PublishMetadata(PropertyIDUsecaseAESyncStartLockTagID);
    }

    if (result == CamxResultSuccess)
    {
        result = pUsecasePoolSlot->PublishMetadata(PropertyIDUsecaseAESyncStopLockTagID);
    }

    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Failed to set AE Lock Range");
    }
    else
    {
        CAMX_LOG_CONFIG(CamxLogGroupCore, "Set AE lock Range from %llu - %llu for PipelineIndex:%d",
            startRequestID,
            stopRequestID,
            pipelineIndex);
    }

    return result;
}

CAMX_NAMESPACE_END
