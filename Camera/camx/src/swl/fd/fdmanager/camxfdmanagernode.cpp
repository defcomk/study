////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxfdmanagernode.cpp
/// @brief FDManager Node class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxcslfddefs.h"
#include "camxmem.h"
#include "camxpipeline.h"
#include "camxtrace.h"
#include "camximagebuffer.h"
#include "camximagebuffermanager.h"
#include "camxtranslator.h"
#include "camxnode.h"
#include "camxfdproperty.h"
#include "camxfdmanagernode.h"
#include "camxfdutils.h"
#include "camxvendortags.h"
#include "camxhal3metadatautil.h"
#include "camxhal3module.h"
#include "camxtitan17xcontext.h"

CAMX_NAMESPACE_BEGIN

/// @brief This enum is used sequence execute process based on dependencies per request id
enum DependencySequence
{
    DependencySetSetup           = 0, ///< No dependencies set, default
    DependencySetProperties      = 1, ///< Property dependency set
    DependencySetPublish         = 2, ///< Latest point for publishing results
    DependencySetHwResults       = 3, ///< Hw results dependency set
};

static const UINT32 FDFaceMinConfidence = 540;  ///< Minimum face confidence to set for a face in the results.
                                                ///  Some applications expect confidence to be minimum of this value.
static const UINT32 FDLimitHWFaces      = 12;   ///< Max number of faces from HW to consider. +2 to consider false positives.

static const CHAR* pFDProceprocessibraryName = "libcamxswprocessalgo";  ///< SW processing algo library name

// @brief list of tags published by FD
static const UINT FDOutputTags[] =
{
    PropertyIDFDInternalPerFrameSettings,
    PropertyIDFDFrameSettings,
    StatisticsFaceDetectMode,
    StatisticsFaceRectangles,
    StatisticsFaceScores
};

// @brief list of vendor tags published by FD
static const struct NodeVendorTag FDOutputVendorTags[] =
{
    { "org.quic.camera2.oemfdresults", "OEMFDResults" }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::FDManagerNode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FDManagerNode::FDManagerNode()
    : m_hThread(InvalidJobHandle)
{
    FDBaseResolutionType    baseResolution = GetStaticSettings()->FDBaseResolution;

    m_pNodeName                  = "FDManager";
    m_baseFDHWDimension.width    = static_cast<UINT32>(baseResolution) & 0xFFFF;
    m_baseFDHWDimension.height   = (static_cast<UINT32>(baseResolution) >> 16) & 0xFFFF;

    m_derivedNodeHandlesMetaDone = TRUE;
    m_isNodeStreamedOff          = TRUE;
    m_enableSwIntermittent       = FALSE;

    m_hFDEngineFPFilterHandle    = InvalidFDEngineHandle;
    m_hFDEngineSWHandle          = InvalidFDEngineHandle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::~FDManagerNode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FDManagerNode::~FDManagerNode()
{
    CamxResult result = CamxResultSuccess;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "FD frames processed %d skipped %d total frames %d",
                     m_processedFrameCnt, m_skippedFrameCnt,
                     m_skippedFrameCnt + m_processedFrameCnt);

    // Un-register the job family.
    if ((InvalidJobHandle != m_hThread) && (NULL != m_pThreadManager))
    {
        m_pThreadManager->UnregisterJobFamily(FDManagerThreadCb, "FDManagerThread", m_hThread);
    }

    // Destroy Thread data pool
    if (NULL != m_pLock)
    {
        m_pLock->Lock();
    }

    LDLLNode* pNode = m_threadDataList.RemoveFromHead();

    while (NULL != pNode)
    {
        FDManagerThreadData* pThreadData = static_cast<FDManagerThreadData*>(pNode->pData);

        CAMX_DELETE pThreadData;
        pThreadData = NULL;

        CAMX_FREE(pNode);

        pNode = m_threadDataList.RemoveFromHead();
    }

    if (NULL != m_pLock)
    {
        m_pLock->Unlock();
    }

    if (InvalidFDEngineHandle != m_hFDEngineFPFilterHandle)
    {
        result = FDEngineDestroy(m_hFDEngineFPFilterHandle);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in destroying FD Engine handle, result=%d", result);
        }

        m_hFDEngineFPFilterHandle = InvalidFDEngineHandle;
    }

    if (InvalidFDEngineHandle != m_hFDEngineSWHandle)
    {
        result = FDEngineDestroy(m_hFDEngineSWHandle);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in destroying FD Engine handle, result=%d", result);
        }

        m_hFDEngineSWHandle = InvalidFDEngineHandle;
    }

    if (TRUE == GetStaticSettings()->useFDInternalBuffers)
    {
        for (UINT32 index = 0; index < MaxRequestQueueDepth; index++)
        {
            if (NULL != m_pImageAddress[index])
            {
                CAMX_FREE(m_pImageAddress[index]);
                m_pImageAddress[index] = NULL;
            }
            if ((NULL != m_pUVImageAddress[index]) && ((FD_DL_ARM == GetStaticSettings()->FDFilterEngine) ||
                (FD_DL_DSP == GetStaticSettings()->FDFilterEngine)))
            {
                CAMX_FREE(m_pUVImageAddress[index]);
                m_pUVImageAddress[index] = NULL;
            }
        }
    }

    if (NULL != m_pNCSSensorHandleGravity)
    {
        result = m_pNCSServiceObject->UnregisterService(m_pNCSSensorHandleGravity);
        CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Unable to unregister Gravity from the NCS %s",
                            Utils::CamxResultToString(result));
        m_pNCSSensorHandleGravity = NULL;
    }

    if (NULL != m_preprocessingData.hLibHandle)
    {
        OsUtils::LibUnmap(m_preprocessingData.hLibHandle);
        m_preprocessingData.hLibHandle = NULL;
    }

    if (NULL != m_pStabilizer)
    {
        m_pStabilizer->Destroy();
        m_pStabilizer = NULL;
    }

    if (NULL != m_pLock)
    {
        m_pLock->Destroy();
        m_pLock = NULL;
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FDManagerNode* FDManagerNode::Create(
    const NodeCreateInputData* pCreateInputData,
    NodeCreateOutputData*      pCreateOutputData)
{
    CAMX_UNREFERENCED_PARAM(pCreateInputData);
    CAMX_UNREFERENCED_PARAM(pCreateOutputData);

    return CAMX_NEW FDManagerNode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::Destroy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::Destroy()
{
    CAMX_DELETE this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::ProcessingNodeInitialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::ProcessingNodeInitialize(
    const NodeCreateInputData* pCreateInputData,
    NodeCreateOutputData*      pCreateOutputData)
{
    CAMX_UNREFERENCED_PARAM(pCreateInputData);

    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(FDManager == Type());
    CAMX_ASSERT(NULL      != pCreateOutputData);

    pCreateOutputData->maxOutputPorts = FDManagerMaxOutputPorts;
    pCreateOutputData->maxInputPorts  = FDManagerMaxInputPorts;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "FD manager node : max input ports %d, max output ports %d",
                     FDManagerMaxOutputPorts, FDManagerMaxInputPorts);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::ProcessingNodeFinalizeInitialization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::ProcessingNodeFinalizeInitialization(
    FinalizeInitializationData* pFinalizeInitializationData)
{
    m_pThreadManager = pFinalizeInitializationData->pThreadManager;

    CAMX_ASSERT(NULL != m_pThreadManager);

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::PostPipelineCreate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::PostPipelineCreate()
{
    CamxResult          result          = CamxResultSuccess;
    MetadataPool*       pUsecasePool    = GetPerFramePool(PoolType::PerUsecase);
    MetadataPool*       pResultPool     = GetPerFramePool(PoolType::PerFrameResult);
    const SensorMode*   pSensorMode     = NULL;
    UINT                cameraPosition  = 0;
    BOOL                hwHybrid        = TRUE;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "FD node created in pipeline");

    CAMX_ASSERT((NULL != pUsecasePool) && (NULL != pResultPool));

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "FD Use internal buffers %d engine type %d client %d,"
                     "enableSwIntermittent %d Titanversion 0x%x",
                     GetStaticSettings()->useFDInternalBuffers,
                     GetStaticSettings()->FDFilterEngine, GetStaticSettings()->FDClient,
                     m_enableSwIntermittent,
                     static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion());

    // CSLTitan150 is Talos
    if ((FDSWOnly == GetStaticSettings()->FDClient) ||
        (CSLCameraTitanVersion::CSLTitan150 == static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion()))
    {
        m_bEnableSwDetection = TRUE;
    }
    else
    {
        m_bEnableSwDetection = FALSE;
    }

    m_pLock = Mutex::Create("FDManagerLock");
    if (NULL == m_pLock)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in Mutex creation");
        result = CamxResultENoMemory;
    }

    if ((CamxResultSuccess == result) && (TRUE == GetStaticSettings()->useFDInternalBuffers))
    {
        for (UINT32 index = 0; index < MaxRequestQueueDepth; index++)
        {
            m_pImageAddress[index] =
                reinterpret_cast<BYTE*>(CAMX_CALLOC(m_baseFDHWDimension.width * m_baseFDHWDimension.height));

            CAMX_ASSERT(NULL != m_pImageAddress[index]);

            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Internal buffers - [%d] : %p", index, m_pImageAddress[index]);
            if ((FD_DL_ARM == GetStaticSettings()->FDFilterEngine) || (FD_DL_DSP == GetStaticSettings()->FDFilterEngine))
            {
                m_pUVImageAddress[index] =reinterpret_cast<BYTE*>(CAMX_CALLOC((m_baseFDHWDimension.width *
                                                                               m_baseFDHWDimension.height) / 2));
                CAMX_ASSERT(NULL != m_pUVImageAddress[index]);
                CAMX_LOG_VERBOSE(CamxLogGroupFD, "Internal buffers UV- [%d] : %p", index, m_pUVImageAddress[index]);
            }
        }
    }
    if (CamxResultSuccess == result)
    {
        m_pLock->Lock();

        for (UINT i = 0; i < DefaultFDManagerThreadDataListSize; i++)
        {
            FDManagerThreadData* pThreadData = CAMX_NEW FDManagerThreadData;
            if (NULL == pThreadData)
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "FDManagerThreadData Create failed for index: %d", i);
            }
            else
            {
                LDLLNode* pNode = static_cast<LDLLNode*>(CAMX_CALLOC(sizeof(LDLLNode)));

                if (NULL != pNode)
                {
                    pNode->pData = pThreadData;
                    m_threadDataList.InsertToTail(pNode);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "Out of memory");
                    CAMX_DELETE pThreadData;
                    pThreadData = NULL;
                }
            }
        }

        m_pLock->Unlock();
    }

    HwCameraInfo cameraInfo;

    HwEnvironment::GetInstance()->GetCameraInfo(GetPipeline()->GetCameraId(), &cameraInfo);

    CAMX_ASSERT(NULL != cameraInfo.pSensorCaps);

    m_activeArrayDimension.width  = cameraInfo.pSensorCaps->activeArraySize.width;
    m_activeArrayDimension.height = cameraInfo.pSensorCaps->activeArraySize.height;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "ActiveArray Dimensions width %d height %d",
                     m_activeArrayDimension.width, m_activeArrayDimension.height);

    if ((0 == m_activeArrayDimension.width) || (0 == m_activeArrayDimension.height))
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Invalid ActiveArray dimensions %d %d",
                       m_activeArrayDimension.width, m_activeArrayDimension.height);

        result = CamxResultEFailed;
    }

    if (CamxResultSuccess == result)
    {
        result = GetCameraConfiguration(&m_pCameraConfigInfo);

        if ((CamxResultSuccess == result) && (NULL != m_pCameraConfigInfo))
        {
            CAMX_LOG_VERBOSE(CamxLogGroupFD, "InstanceID[%d] Camera position mountAngle %d position %d imageOrientation %d",
                             InstanceID(), m_pCameraConfigInfo->mountAngle, m_pCameraConfigInfo->position + 1,
                             m_pCameraConfigInfo->imageOrientation);
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupFD, "GetCameraConfiguration failed status %d try later..", result);

            result = CamxResultSuccess;
        }
    }

    if (CamxResultSuccess == result)
    {
        result = GetSensorModeData(&pSensorMode);
    }

    if ((CamxResultSuccess == result) && (NULL != pSensorMode))
    {
        BOOL useCAMIFMapFromSensoreInfo = FALSE;

        m_sensorDimension.width  = pSensorMode->resolution.outputWidth;
        m_sensorDimension.height = pSensorMode->resolution.outputHeight;

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Sensor Dimensions width %d (x%d) height %d (x%d)",
                         m_sensorDimension.width, pSensorMode->binningTypeH,
                         m_sensorDimension.height, pSensorMode->binningTypeV);

        /// @todo (CAMX-2801) Check Android spec for exact translation of ROI from CAMIF to Active Array
        if (TRUE == useCAMIFMapFromSensoreInfo)
        {
            for (UINT i = 0; i < MaxSensorModeStreamConfigCount; i++)
            {
                if (StreamType::IMAGE == pSensorMode->streamConfig[i].type)
                {
                    m_CAMIFMap.left   = pSensorMode->streamConfig[i].frameDimension.xStart;
                    m_CAMIFMap.top    = pSensorMode->streamConfig[i].frameDimension.yStart;
                    m_CAMIFMap.width  = (pSensorMode->streamConfig[i].frameDimension.width  * pSensorMode->binningTypeH);
                    m_CAMIFMap.height = (pSensorMode->streamConfig[i].frameDimension.height * pSensorMode->binningTypeV);

                    CAMX_LOG_VERBOSE(CamxLogGroupFD, "CAMIF Map in ActiveArray (%d, %d), %dx%d",
                                     m_CAMIFMap.left, m_CAMIFMap.top, m_CAMIFMap.width, m_CAMIFMap.height);

                    break;
                }
            }
        }
        else
        {
            FLOAT   widthRatio;
            FLOAT   heightRatio;
            FLOAT   upscaleRatio;

            widthRatio  = static_cast<FLOAT>(m_activeArrayDimension.width) / m_sensorDimension.width;
            heightRatio = static_cast<FLOAT>(m_activeArrayDimension.height) / m_sensorDimension.height;

            CAMX_LOG_VERBOSE(CamxLogGroupFD, "widthRatio=%f, heightRatio=%f", widthRatio, heightRatio);

            if (widthRatio > heightRatio)
            {
                upscaleRatio = heightRatio;
            }
            else
            {
                upscaleRatio = widthRatio;
            }

            m_CAMIFMap.width  = m_sensorDimension.width  * upscaleRatio;
            m_CAMIFMap.height = m_sensorDimension.height * upscaleRatio;
            m_CAMIFMap.left   = (m_activeArrayDimension.width - m_CAMIFMap.width) / 2;
            m_CAMIFMap.top    = (m_activeArrayDimension.height - m_CAMIFMap.height) / 2;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "CAMIF Map in ActiveArray (%d, %d), %dx%d",
                         m_CAMIFMap.left, m_CAMIFMap.top, m_CAMIFMap.width, m_CAMIFMap.height);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Pointer to pSensorMode %p, result %d", pSensorMode, result);
        result = CamxResultEInvalidPointer;
    }

    if (CamxResultSuccess == result)
    {
        CSLCameraPlatform CSLPlatform  = {};

        result = CSLQueryCameraPlatform(&CSLPlatform);

        if (CamxResultSuccess == result)
        {
            m_FDHwVersion = m_FDHwUtils.GetFDHwVersion(&CSLPlatform);

            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Platform version %d.%d.%d, cpas version %d.%d.%d, FDHw version %d",
                             CSLPlatform.platformVersion.majorVersion, CSLPlatform.platformVersion.minorVersion,
                             CSLPlatform.platformVersion.revVersion,   CSLPlatform.CPASVersion.majorVersion,
                             CSLPlatform.CPASVersion.minorVersion,     CSLPlatform.CPASVersion.revVersion,
                             m_FDHwVersion);
        }
    }

    if (CamxResultSuccess == result)
    {
        // Register with thread manager.
        if (InvalidJobHandle == m_hThread)
        {
            result = m_pThreadManager->RegisterJobFamily(FDManagerThreadCb, "FDManagerThread", NULL, JobPriority::Normal,
                                                         TRUE, &m_hThread);
        }
    }

    /// @todo (CAMX-3310) Get the correct flags
    m_isFrontCamera = FALSE;
    m_isVideoMode   = FALSE;
    cameraPosition  = GetPipeline()->GetCameraId();
    // Convert CameraPosition to CHISENSORPOSITIONTYPE
    cameraPosition += 1;

    if ((TRUE == GetStaticSettings()->useDifferentTuningForFrontCamera) && (FRONT == cameraPosition))
    {
        m_isFrontCamera = TRUE;
    }
    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Is front camera %d", m_isFrontCamera);

    if (TRUE == m_bEnableSwDetection)
    {
        hwHybrid = FALSE;
    }

    if (CamxResultSuccess == result)
    {
        FDConfigSelector configSelector = (TRUE == m_isVideoMode) ? FDSelectorVideo : FDSelectorDefault;

        if (FDConfigVendorTag != GetStaticSettings()->FDConfigSource)
        {
            result = FDUtils::GetFDConfig(this, GetStaticSettings()->FDConfigSource, hwHybrid, m_isFrontCamera, configSelector,
                                          &m_FDConfig);
        }
        else
        {
            // Vendor tag wont be available by now. Read defualt config and save.
            result = FDUtils::GetFDConfig(this, FDConfigDefault, hwHybrid, m_isFrontCamera, configSelector, &m_FDConfig);
        }
    }

    if (CamxResultSuccess == result)
    {
        result = InitializeStabilization();
    }

    if (CamxResultSuccess == result)
    {
        result = OnSetUpFDEngine();
    }

    if (CamxResultSuccess == result)
    {
        static const UINT32 MaxFDMutexResourceStringLength = 255;
        CHAR                FDMutexResource[MaxFDMutexResourceStringLength];

        OsUtils::SNPrintF(FDMutexResource, MaxFDMutexResourceStringLength, "FDMutexResource");
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in post pipeline create, result=%d", result);
    }

    m_startTimeSaved = FALSE;

    if ((CamxResultSuccess == result) && (TRUE == GetStaticSettings()->enableNCSService))
    {
        result = SetupNCSLink();
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_WARN(CamxLogGroupFD, "(non-fatal) Failed setting up NCS link result=%d", result);
            // Overwrite the result with success since this is not a fatal error.
            result = CamxResultSuccess;
        }
    }

    if ((CamxResultSuccess == result) && (Disable != GetStaticSettings()->FDPreprocessing))
    {
        CHAR libFilename[FILENAME_MAX];

        OsUtils::SNPrintF(libFilename, FILENAME_MAX, "%s%s%s.%s",
                          VendorLibPath, PathSeparator, pFDProceprocessibraryName, SharedLibraryExtension);

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "sw process lib path %s", libFilename);

        m_preprocessingData.hLibHandle = OsUtils::LibMap(libFilename);

        if (NULL != m_preprocessingData.hLibHandle)
        {
            m_preprocessingData.pfnSWAlgoProcess = reinterpret_cast<SWAlgoProcessType>(
                OsUtils::LibGetAddr(m_preprocessingData.hLibHandle, "SWAlgoProcess"));

            if (NULL == m_preprocessingData.pfnSWAlgoProcess)
            {
                CAMX_LOG_WARN(CamxLogGroupFD, "Invalid SWAlgoProcess");

                // unload library
                OsUtils::LibUnmap(m_preprocessingData.hLibHandle);
                m_preprocessingData.hLibHandle = NULL;
            }
        }
        else
        {
            // Not a fatal, we can still continue, without preprocessing feature
            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Preprpocessing library not found");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::GetFdFilterEngine
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FDEngineType FDManagerNode::GetFdFilterEngine()
{
    FDEngineType enginetype;
    FDFilterEngineType filterengine = GetStaticSettings()->FDFilterEngine;
    switch (filterengine)
    {
        case FD_Standard_ARM:
            enginetype = Standard_ARM;
            break;
        case FD_Standard_DSP:
            enginetype = Standard_DSP;
            break;
        case FD_DL_ARM:
            enginetype = DL_ARM;
            break;
        case FD_DL_DSP:
            enginetype = DL_DSP;
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupFD, "No engine type selected");
            break;
    }
    return enginetype;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::ProcessingNodeFinalizeInputRequirement
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::ProcessingNodeFinalizeInputRequirement(
    BufferNegotiationData* pBufferNegotiationData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL == pBufferNegotiationData)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "NULL params pBufferNegotiationData");

        result = CamxResultEInvalidPointer;
    }

    if (CamxResultSuccess == result)
    {
        UINT32 numInputPorts = 0;
        UINT32 inputPortId[FDManagerMaxInputPorts];

        // Get Input Port List
        GetAllInputPortIds(&numInputPorts, &inputPortId[0]);

        pBufferNegotiationData->numInputPorts = numInputPorts;

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "numInputPorts=%d", numInputPorts);

        for (UINT input = 0; input < numInputPorts; input++)
        {
            pBufferNegotiationData->inputBufferOptions[input].nodeId        = Type();
            pBufferNegotiationData->inputBufferOptions[input].instanceId    = InstanceID();
            pBufferNegotiationData->inputBufferOptions[input].portId        = inputPortId[input];

            BufferRequirement* pInputBufferRequirement =
                &pBufferNegotiationData->inputBufferOptions[input].bufferRequirement;

            result = FillInputBufferRequirments(pInputBufferRequirement, inputPortId[input]);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "Fill input buffer requirement failed");
                break;
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::FinalizeBufferProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::FinalizeBufferProperties(
    BufferNegotiationData* pBufferNegotiationData)
{
    CAMX_UNREFERENCED_PARAM(pBufferNegotiationData);

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::ExecuteProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::ExecuteProcessRequest(
    ExecuteProcessRequestData* pExecuteProcessRequestData)
{
    CAMX_ASSERT(NULL != pExecuteProcessRequestData);
    CAMX_ASSERT(NULL != pExecuteProcessRequestData->pNodeProcessRequestData);
    CAMX_ASSERT(NULL != pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest);

    CamxResult              result                  = CamxResultSuccess;
    BOOL                    FDHwResultsReady        = FALSE;
    FDPerFrameSettings      perFrameSettings        = { 0 };
    BOOL                    requestHandlingDone     = FALSE;
    NodeProcessRequestData* pNodeRequestData        = pExecuteProcessRequestData->pNodeProcessRequestData;
    PerRequestActivePorts*  pPerRequestPorts        = pExecuteProcessRequestData->pEnabledPortsInfo;
    DependencySequence      dependencySequence      = static_cast<DependencySequence>(pNodeRequestData->processSequenceId);
    UINT64                  requestId               = pNodeRequestData->pCaptureRequest->requestId;

    m_pPerRequestPorts = pExecuteProcessRequestData->pEnabledPortsInfo;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] EPR-%d, bindIOBuffers=%d",
                     InstanceID(), requestId, dependencySequence, pNodeRequestData->bindIOBuffers);

    if (0 == m_FDFrameWidth)
    {
        UINT32                   numInputPorts = pExecuteProcessRequestData->pEnabledPortsInfo->numInputPorts;
        PerRequestInputPortInfo* pInputPorts   = pExecuteProcessRequestData->pEnabledPortsInfo->pInputPorts;
        for (UINT32 i = 0; i < numInputPorts; i++)
        {
            if ((FDManagerInputPortImage == pInputPorts[i].portId) && (NULL != pInputPorts[i].pImageBuffer) &&
                (NULL != pInputPorts[i].pImageBuffer->GetFormat()))
            {
                YUVFormat yuvFormat = pInputPorts[i].pImageBuffer->GetFormat()->formatParams.yuvFormat[0];
                m_FDFrameWidth      = yuvFormat.width;
                m_FDFrameHeight     = yuvFormat.height;
                m_FDFrameStride     = yuvFormat.planeStride;
                m_FDFrameScanline   = yuvFormat.sliceHeight;

                CAMX_LOG_VERBOSE(CamxLogGroupFD, "Input image dim width %d height %d stride %d scanline %d",
                                 m_FDFrameWidth, m_FDFrameHeight, m_FDFrameStride, m_FDFrameScanline);
                break;
            }
        }
    }

    if (DependencySequence::DependencySetSetup != dependencySequence)
    {
        // read back per frame settings from internal pool
        GetFDFrameSettings(&perFrameSettings);
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] [%llu] skipProcess=%d, fdModeReceived=%d, controlMode=%d "
                         "sceneMode=%d postResultsToMetaData=%d, postResultsToProperty=%d",
                         InstanceID(), requestId,                perFrameSettings.requestId,
                         perFrameSettings.skipProcess,           perFrameSettings.fdModeReceived,
                         perFrameSettings.controlMode,           perFrameSettings.sceneMode,
                         perFrameSettings.postResultsToMetaData, perFrameSettings.postResultsToProperty);
    }

    if (DependencySequence::DependencySetSetup == dependencySequence)
    {
        perFrameSettings.requestId = requestId;

        // Read input meta data settings from app for this request
        GetPerFrameMetaDataSettings(pNodeRequestData, &perFrameSettings);

        if (TRUE == CheckFDConfigChange(&perFrameSettings.fdConfig))
        {
            perFrameSettings.FDConfigUpdated    = TRUE;
            m_FDConfig                          = perFrameSettings.fdConfig;
        }
        else
        {
            perFrameSettings.fdConfig           = m_FDConfig;
            perFrameSettings.FDConfigUpdated    = FALSE;
        }

        // Determine any special frame settings specific to this request Id. Exa - frame skip based on fps
        DetermineFrameSettings(pNodeRequestData, &perFrameSettings);

        // If preprocessing is not required, publish frame settings here so that HW node starts preparing packet
        if ((TRUE    == perFrameSettings.skipProcess)         ||
            (Disable == GetStaticSettings()->FDPreprocessing) ||
            (NULL    == m_preprocessingData.hLibHandle))
        {
            PublishFDFrameSettings(&perFrameSettings);
            perFrameSettings.frameSettingsPublished = TRUE;
        }

        perFrameSettings.enablePreprocessing = FALSE;

        result = PublishFDInternalFrameSettings(&perFrameSettings);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Instance[%d] ReqId[%llu] Failed in publish frame settings result=%d",
                           InstanceID(), requestId, result);
        }
        else
        {
            SetDependencies(pNodeRequestData, pPerRequestPorts, &perFrameSettings);
            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] Manager node sets dependency",
                             InstanceID(), requestId);
        }
    }
    else if (DependencySequence::DependencySetProperties == dependencySequence)
    {
        if (FALSE == FDFramePreprocessingRequired(&perFrameSettings))
        {
            perFrameSettings.enablePreprocessing = FALSE;

            // If preprocessing is not required, and frame settings has not yet been published, publish it here so that HW
            // node starts preparing packet
            if (FALSE == perFrameSettings.frameSettingsPublished)
            {
                PublishFDFrameSettings(&perFrameSettings);
                perFrameSettings.frameSettingsPublished = TRUE;
            }
        }
        else
        {
            perFrameSettings.enablePreprocessing = TRUE;
        }

        result = PublishFDInternalFrameSettings(&perFrameSettings);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Instance[%d] ReqId[%llu] Failed in publish frame settings result=%d",
                           InstanceID(), requestId, result);
        }

    }
    else if ((DependencySequence::DependencySetPublish == dependencySequence) &&
             (FALSE == GetStaticSettings()->enableOfflineFD))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] Manager node publishing FD results",
                         InstanceID(), requestId);

        PublishFDResults(&perFrameSettings);

        ProcessPartialMetadataDone(requestId);
        ProcessMetadataDone(requestId);

        result = GetInputFrameImageBufferInfo(pPerRequestPorts, &perFrameSettings);

        if ((CamxResultSuccess == result)                               &&
            (TRUE              == perFrameSettings.enablePreprocessing) &&
            (FALSE             == perFrameSettings.skipProcess))
        {
            DoFDFramePreprocessing(&perFrameSettings);
        }

        if (FALSE == perFrameSettings.frameSettingsPublished)
        {
            PublishFDFrameSettings(&perFrameSettings);
        }

        if (TRUE == perFrameSettings.skipProcess)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] Skipping frame ", InstanceID(), requestId);
            requestHandlingDone = TRUE;
        }
        else if (TRUE == m_bEnableSwDetection)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Processing SW Detection ReqId[%llu] reqhandle %d",
                             requestId, requestHandlingDone);
            GetFrameCropInfo(pNodeRequestData, &perFrameSettings);
            GetDeviceOrientation(pNodeRequestData, &perFrameSettings);
            result = GetAndProcessFDResults(requestId, pPerRequestPorts, &perFrameSettings, &requestHandlingDone);
        }
        else if (TRUE == m_enableSwIntermittent)
        {
            perFrameSettings.FDManagerExecutionSequence = FDSWIntermittent;
            result = GetAndProcessFDResults(requestId, pPerRequestPorts, &perFrameSettings, &requestHandlingDone);
            requestHandlingDone = FALSE; // In this case we are not done with the buffer
        }
    }
    // If it is SW, EPR DependencySetHwResults will come before DependencySetPublish and we do processrequestid done
    else if ((DependencySequence::DependencySetHwResults == dependencySequence) &&
             (FALSE == m_bEnableSwDetection))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] FDHwDone, Manager node Processing", InstanceID(), requestId);

        GetFrameCropInfo(pNodeRequestData, &perFrameSettings);
        GetDeviceOrientation(pNodeRequestData, &perFrameSettings);
        perFrameSettings.FDManagerExecutionSequence = FDFPFiltering;

        // Process SW processing (false positive filtering) : this is offloaded to a different thread based on settings
        result = GetAndProcessFDResults(requestId, pPerRequestPorts, &perFrameSettings, &requestHandlingDone);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Instance[%d] ReqId[%llu] Failed in Process result=%d",
                           InstanceID(), requestId, result);
        }
        else
        {
            // Expect GetAndProcessFDResults to complete processing unless threaded without internal buffers, which requires
            // maintaining reference to the port link buffers until the thread completes processing. Thread will call
            // ProcessRequestIdDone
            CAMX_ASSERT((TRUE == requestHandlingDone) || ((TRUE  == GetStaticSettings()->enableFDManagerThreading) &&
                                                          (FALSE == GetStaticSettings()->useFDInternalBuffers)));
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] requestHandlingDone %d,"
                         "Manager node ProcessRequestDone result %d",
                         InstanceID(), requestId, requestHandlingDone, result);
    }

    if ((TRUE == requestHandlingDone) || (CamxResultSuccess != result))
    {
        /// @todo (CAMX-4037) add check and do cache op only if buffer has CACHE flags set
        if ((NULL != perFrameSettings.pImageBuffer) &&
            (TRUE == perFrameSettings.inputBufferInvalidateRequired))
        {
            perFrameSettings.pImageBuffer->CacheOps(true, false);
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] done : requestHandlingDone=%d result=%d",
                         InstanceID(), requestId, requestHandlingDone, result);

        if (FALSE == GetStaticSettings()->enableOfflineFD)
        {
            ProcessRequestIdDone(requestId);
        }
        else
        {
            for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
            {
                PerRequestOutputPortInfo* pOutputPort = &pPerRequestPorts->pOutputPorts[i];

                if (pOutputPort == NULL)
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "pOutputPort is NULL");
                    result = CamxResultEFailed;
                    return result;
                }

                if (TRUE == IsSinkPortNoBuffer(pOutputPort->portId))
                {
                    ProcessRequestIdDone(requestId);
                }
                else
                {
                    FaceROIInformation  faceROIInfo = { 0 };
                    GetFDResultsFromUsecasePool(&faceROIInfo);

                    Utils::Memcpy(pOutputPort->ppImageBuffer[0], &faceROIInfo, sizeof(FaceROIInformation));

                    if ((NULL != pOutputPort) && (NULL != pOutputPort->phFence))
                    {
                        result = CSLFenceSignal(*pOutputPort->phFence, CSLFenceResultSuccess);
                    }
                }
            }
        }

    }

    if (CamxResultSuccess != result)
    {
        if (TRUE == GetStaticSettings()->raisesigabrt)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD,
                           "ExecuteProcessRequest failed with %s; raising abort signal",
                           CamxResultStrings[result]);
            OsUtils::RaiseSignalAbort();
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::FillInputBufferRequirments()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::FillInputBufferRequirments(
    BufferRequirement* pInputBufferRequirement,
    UINT32             portId)
{
    CamxResult  result  = CamxResultSuccess;
    UINT32      width   = 0;
    UINT32      height  = 0;

    if (NULL == pInputBufferRequirement)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Invalid pointer %p", pInputBufferRequirement);
        result = CamxResultEInvalidPointer;
    }

    if (CamxResultSuccess == result)
    {
        CamxDimension previewDimension = { 0 };

        switch (portId)
        {
            case FDManagerInputPortHwResults:
                width   = sizeof(FDHwResults);
                height  = 1;
                break;
            case FDManagerInputPortHwPyramidBuffer:
                {
                    if (FDHwVersinoInvalid != m_FDHwVersion)
                    {
                        result = m_FDHwUtils.CalculatePyramidBufferSize(m_FDHwVersion,
                                                                        Utils::AlignGeneric32(m_baseFDHWDimension.width, 32) ,
                                                                        m_baseFDHWDimension.height,
                                                                        &width);
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupFD, "invalid FD Hw version");
                    }

                    height = 1;

                    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Pyramid buffer : FD Dim[%dx%d], Pyramid Dim[%dx%d]",
                                     m_baseFDHWDimension.width, m_baseFDHWDimension.height, width, height);

                    break;
                }
            case FDManagerInputPortImage:
                width  = m_baseFDHWDimension.width;
                height = m_baseFDHWDimension.height;

                result = GetPreviewDimension(&previewDimension);

                if ((CamxResultSuccess == result) && (0 != previewDimension.width) && (0 != previewDimension.height))
                {
                    CamxDimension frameDimension = m_baseFDHWDimension;

                    Utils::MatchAspectRatio(&previewDimension, &frameDimension);

                    width  = frameDimension.width;
                    height = frameDimension.height;
                }
                else
                {
                    CAMX_LOG_WARN(CamxLogGroupFD, "Failed to get preview dimension, result=%d, width=%d, height=%d",
                                  result, previewDimension.width, previewDimension.height);
                }

                width  = Utils::EvenFloorUINT32(width);
                height = Utils::EvenFloorUINT32(height);

                CAMX_LOG_VERBOSE(CamxLogGroupFD, "Dimensions :Base[%dx%d], Preview [%dx%d], Request [%dx%d]",
                                 m_baseFDHWDimension.width, m_baseFDHWDimension.height,
                                 previewDimension.width,    previewDimension.height,
                                 width,                     height);

                break;
            default:
                CAMX_LOG_ERROR(CamxLogGroupFD, "Invalid Port ID %d", portId);
                result = CamxResultEInvalidArg;
        }

        if (CamxResultSuccess == result)
        {
            pInputBufferRequirement->optimalWidth   = width;
            pInputBufferRequirement->optimalHeight  = height;
            pInputBufferRequirement->minWidth       = width;
            pInputBufferRequirement->maxWidth       = width;
            pInputBufferRequirement->minHeight      = height;
            pInputBufferRequirement->maxHeight      = height;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::GetFrameCropInfo()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::GetFrameCropInfo(
    const NodeProcessRequestData* const pNodeRequestData,
    FDPerFrameSettings*                 pPerFrameSettings)
{
    CamxResult      result              = CamxResultSuccess;
    UINT32          metaTagResidulaCrop = 0;
    UINT32          metaTagAppliedCrop  = 0;
    UINT            configTag[2]        = { 0 };
    VOID*           pData[2]            = { 0 };
    UINT            length              = CAMX_ARRAY_SIZE(configTag);
    UINT64          configDataOffset[2] = { 0 };

    CAMX_UNREFERENCED_PARAM(pNodeRequestData);

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.ifecropinfo",
                                                      "ResidualCrop",
                                                      &metaTagResidulaCrop);

    result |= VendorTagManager::QueryVendorTagLocation("org.quic.camera.ifecropinfo",
                                                       "AppliedCrop",
                                                       &metaTagAppliedCrop);

    if (CamxResultSuccess == result)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "tags %d %d", metaTagResidulaCrop, metaTagAppliedCrop);

        configTag[0] = metaTagAppliedCrop;
        configTag[1] = metaTagResidulaCrop;

        result = GetDataList(configTag, pData, configDataOffset, length);

        if (NULL != pData[0])
        {
            pPerFrameSettings->inputFrameMap = (reinterpret_cast<IFECropInfo*>(pData[0]))->FDPath;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Error getting input frame map information");
        }

        if (NULL != pData[1])
        {
            pPerFrameSettings->finalFOVCrop = (reinterpret_cast<IFECropInfo*>(pData[1]))->FDPath;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Error getting final crop information");
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Error getting IFE CropInfo vendor tag location %d", result);
    }

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "InputFrameMap [(%d %d), %d, %d], ResidualFOVCrop [(%d %d), %d, %d]",
                     pPerFrameSettings->inputFrameMap.left,  pPerFrameSettings->inputFrameMap.top,
                     pPerFrameSettings->inputFrameMap.width, pPerFrameSettings->inputFrameMap.height,
                     pPerFrameSettings->finalFOVCrop.left,   pPerFrameSettings->finalFOVCrop.top,
                     pPerFrameSettings->finalFOVCrop.width,  pPerFrameSettings->finalFOVCrop.height);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::GetPerFrameMetaDataSettings()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::GetPerFrameMetaDataSettings(
    const NodeProcessRequestData* const pNodeRequestData,
    FDPerFrameSettings*                 pPerFrameSettings)
{
    CAMX_ASSERT(NULL != pNodeRequestData);
    CAMX_ASSERT(NULL != pPerFrameSettings);

    GetFDPerFrameMetaDataSettings(0, NULL, pPerFrameSettings);

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Node[%s] ReqId[%llu]: "
                     "FD mode %d, Control mode %d, Scene mode %d, "
                     "FDProcessingControl=%d, enableFD=%d, postResultsToMetaData=%d, postResultsToProperty=%d",
                     NodeIdentifierString(), pPerFrameSettings->requestId,
                     pPerFrameSettings->fdModeReceived, pPerFrameSettings->controlMode, pPerFrameSettings->sceneMode,
                     GetStaticSettings()->FDProcessingControl, pPerFrameSettings->frameSettings.enableFD,
                     pPerFrameSettings->postResultsToMetaData, pPerFrameSettings->postResultsToProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::DetermineFrameSettings()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::DetermineFrameSettings(
    const NodeProcessRequestData* const pNodeRequestData,
    FDPerFrameSettings*                 pPerFrameSettings)
{
    UINT32      maxFPS          = 0;
    BOOL        skipProcess     = FALSE;
    FDConfig*   pFDConfig;
    FLOAT       currFPS         = 0;
    UINT64      requestId       = pNodeRequestData->pCaptureRequest->requestId;
    BOOL        skipDetermined  = FALSE;
    BOOL        skipRequired    = TRUE;
    UINT64      FDCSLSyncId     = requestId;

    CAMX_ASSERT(NULL != pNodeRequestData);
    CAMX_ASSERT(NULL != pPerFrameSettings);

    pFDConfig = &pPerFrameSettings->fdConfig;

    if (FALSE == pPerFrameSettings->frameSettings.enableFD)
    {
        // No need to process anything
        pPerFrameSettings->skipProcess                          = TRUE;
        pPerFrameSettings->frameSettings.hwSettings.skipProcess = TRUE;
        pPerFrameSettings->frameSettings.swSettings.skipProcess = TRUE;
    }
    else
    {
        // Determine whether we need to process this frame or skip

        BOOL    processMultiCameraFPS   = FALSE;
        BOOL    isMultiCameraUsecase;
        UINT32  numberOfCamerasRunning;
        UINT32  currentCameraId;
        BOOL    isMasterCamera;
        BOOL    FPSBasedFrameSkip = FALSE;

        GetMultiCameraInfo(&isMultiCameraUsecase, &numberOfCamerasRunning, &currentCameraId, &isMasterCamera);

        if ((TRUE == isMultiCameraUsecase) && (1 < numberOfCamerasRunning))
        {
            if (FDMultiCamProcessBoth == GetStaticSettings()->FDMultiCamProcess)
            {
                processMultiCameraFPS = TRUE;
            }
            else if (FDMultiCamProcessMasterOnly == GetStaticSettings()->FDMultiCamProcess)
            {
                if (FALSE == isMasterCamera)
                {
                    skipProcess     = TRUE;
                    skipDetermined  = TRUE;
                }
            }
            else if (FDMultiCamProcessAuxOnly == GetStaticSettings()->FDMultiCamProcess)
            {
                if (TRUE == isMasterCamera)
                {
                    skipProcess     = TRUE;
                    skipDetermined  = TRUE;
                }
            }
        }

        if (0 == m_numFacesDetected)
        {
            if (TRUE == processMultiCameraFPS)
            {
                maxFPS = pFDConfig->multiCameraMaxFPSWithNoFaces;
            }
            else
            {
                maxFPS = pFDConfig->maxFPSWithNoFaces;
            }
        }
        else
        {
            if (TRUE == processMultiCameraFPS)
            {
                maxFPS = pFDConfig->multiCameraMaxFPSWithFaces;
            }
            else
            {
                maxFPS = pFDConfig->maxFPSWithFaces;
            }
        }

        // 0 means max possible, no skip required
        if (0 == maxFPS)
        {
            skipRequired = FALSE;
        }
        else
        {
            FPSBasedFrameSkip = TRUE;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD,
                         "Instance[%d] ReqId[%llu][%llu]  isMultiCameraUsecase=%d, numberOfCamerasRunning=%d, "
                         "processMultiCameraFPS=%d, isMasterCamera=%d, skipRequired=%d, skipProcess=%d, "
                         "skipDetermined=%d, m_numFacesDetected=%d, maxFPS=%d, FPSBasedFrameSkip =%d",
                         InstanceID(), requestId, FDCSLSyncId, isMultiCameraUsecase, numberOfCamerasRunning,
                         processMultiCameraFPS, isMasterCamera, skipRequired, skipProcess,
                         skipDetermined, m_numFacesDetected, maxFPS, FPSBasedFrameSkip);

        if ((TRUE == skipRequired) && (skipDetermined == FALSE))
        {
            if ((FALSE == processMultiCameraFPS) && (m_processedFrameCnt <= pFDConfig->initialNoFrameSkipCount))
            {
                CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] Not skipping first %d frames, current processed %d",
                                 InstanceID(), requestId, pFDConfig->initialNoFrameSkipCount, m_processedFrameCnt);

                skipProcess = FALSE;
            }
            else if ((TRUE == GetStaticSettings()->useAlternateFrameSkip) && (TRUE == processMultiCameraFPS))
            {
                // Skip alternate frames (assuming 15fps)
                // In Dual camera cases - better skip odd numbered frames on one camera, even numbered frames on other camera

                if ((TRUE == isMasterCamera) && (FDCSLSyncId % 2 == 0))
                {
                    skipProcess = TRUE;
                }
                else if ((FALSE == isMasterCamera) && (FDCSLSyncId % 2 == 1))
                {
                    skipProcess = TRUE;
                }
            }
            else if ((skipDetermined == FALSE) && (TRUE == FPSBasedFrameSkip))
            {
                if (FALSE == m_startTimeSaved)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupFD,
                                     "Instance[%d] ReqId[%llu] Starting frame for FPS limit, m_processedFrameCnt=%d",
                                     InstanceID(), requestId, m_processedFrameCnt);

                    OsUtils::GetTime(&m_startTime);
                    skipProcess      = FALSE;
                    m_startTimeSaved = TRUE;
                }
                else
                {
                    // If maxFPS = 0 means max possible fps and no need to skip frame
                    if (maxFPS > 0)
                    {
                        OsUtils::GetTime(&m_currentTime);

                        UINT32 averageFrameInterval;
                        UINT32 numOfFramesProcessed;

                        numOfFramesProcessed = m_processedFrameCnt - pFDConfig->initialNoFrameSkipCount;

                        if (numOfFramesProcessed != 0)
                        {
                            averageFrameInterval = (OsUtils::CamxTimeToMillis(&m_currentTime) -
                                OsUtils::CamxTimeToMillis(&m_startTime)) /
                                numOfFramesProcessed;

                            currFPS = static_cast<FLOAT>(1000) / static_cast<FLOAT>(averageFrameInterval);

                            CAMX_LOG_VERBOSE(CamxLogGroupFD,
                                "Instance[%d] ReqId[%llu] averageFrameInterval=%d fps=%f, m_processedFrameCnt=%d",
                                InstanceID(), requestId, averageFrameInterval, currFPS, m_processedFrameCnt);

                            if (averageFrameInterval >= (1000 / maxFPS))
                            {
                                skipProcess = FALSE;
                            }
                            else
                            {
                                skipProcess = TRUE;
                            }
                        }
                        else
                        {
                            skipProcess = FALSE;
                            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] m_processedFrameCnt %d ,"
                                             "isMultiCameraUsecase %d, processMultiCameraFPS %d",
                                             InstanceID(), requestId, m_processedFrameCnt, isMultiCameraUsecase,
                                             processMultiCameraFPS);
                        }
                    }
                }
            }
        }

        if (FALSE == skipProcess)
        {
            pPerFrameSettings->skipProcess                          = FALSE;
            pPerFrameSettings->frameSettings.hwSettings.skipProcess = FALSE;
            pPerFrameSettings->frameSettings.swSettings.skipProcess = FALSE;
            m_processedFrameCnt++;
        }
        else
        {
            pPerFrameSettings->skipProcess                          = TRUE;
            pPerFrameSettings->frameSettings.hwSettings.skipProcess = TRUE;
            pPerFrameSettings->frameSettings.swSettings.skipProcess = TRUE;
            m_skippedFrameCnt++;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD,
                         "Instance[%d] ReqId[%llu][%llu] Skip %d, m_processedFrameCnt=%d, m_skippedFrameCnt=%d",
                         InstanceID(), requestId, FDCSLSyncId, pPerFrameSettings->skipProcess,
                         m_processedFrameCnt, m_skippedFrameCnt);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::ConvertResultsFromProcessingToReference
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::ConvertResultsFromProcessingToReference(
    FDResults*          pProcessingResults,
    FDResults*          pReferenceResults,
    FDPerFrameSettings* pPerFrameSettings)
{
    CAMX_ASSERT(NULL != pProcessingResults);
    CAMX_ASSERT(NULL != pReferenceResults);
    CAMX_ASSERT(NULL != pPerFrameSettings);

    CHIRectangle    currentFrameROI;
    CHIRectangle    referenceFrameROI;
    CHIDimension    currentFrameDimension;

    currentFrameDimension.width     = m_FDFrameWidth;
    currentFrameDimension.height    = m_FDFrameHeight;

    *pReferenceResults = *pProcessingResults;

    CAMX_ASSERT(0 != m_sensorDimension.width);
    CAMX_ASSERT(0 != m_sensorDimension.height);
    CAMX_ASSERT(0 != currentFrameDimension.width);
    CAMX_ASSERT(0 != currentFrameDimension.height);
    CAMX_ASSERT(0 != pPerFrameSettings->inputFrameMap.width);
    CAMX_ASSERT(0 != pPerFrameSettings->inputFrameMap.height);

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Current Frame (%dx%d), ReferenceFrame (%dx%d), Map [(%d, %d) %dx%d)",
                     currentFrameDimension.width,               currentFrameDimension.height,
                     m_sensorDimension.width,                   m_sensorDimension.height,
                     pPerFrameSettings->inputFrameMap.left,     pPerFrameSettings->inputFrameMap.top,
                     pPerFrameSettings->inputFrameMap.width,    pPerFrameSettings->inputFrameMap.height);

    for (INT32 i = 0; i < pProcessingResults->numFacesDetected; i++)
    {
        currentFrameROI.left   = static_cast<INT32>(pProcessingResults->faceInfo[i].faceROI.center.x) -
                                 static_cast<INT32>(pProcessingResults->faceInfo[i].faceROI.width / 2);
        currentFrameROI.top    = static_cast<INT32>(pProcessingResults->faceInfo[i].faceROI.center.y) -
                                 static_cast<INT32>(pProcessingResults->faceInfo[i].faceROI.height / 2);
        currentFrameROI.width  = pProcessingResults->faceInfo[i].faceROI.width;
        currentFrameROI.height = pProcessingResults->faceInfo[i].faceROI.height;

        referenceFrameROI = Translator::ConvertROIFromCurrentToReference(&m_sensorDimension,
                                                                         &currentFrameDimension,
                                                                         &pPerFrameSettings->inputFrameMap,
                                                                         &currentFrameROI);

        pReferenceResults->faceInfo[i].faceROI.center.x = referenceFrameROI.left + (referenceFrameROI.width / 2);
        pReferenceResults->faceInfo[i].faceROI.center.y = referenceFrameROI.top  + (referenceFrameROI.height / 2);
        pReferenceResults->faceInfo[i].faceROI.width    = referenceFrameROI.width;
        pReferenceResults->faceInfo[i].faceROI.height   = referenceFrameROI.height;

        if (FDFaceMinConfidence < pReferenceResults->faceInfo[i].faceROI.confidence)
        {
            pReferenceResults->faceInfo[i].faceROI.confidence = FDFaceMinConfidence;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Results CurrentFrame:[(%d, %d), %d %d] Reference : [(%d, %d), %d %d]",
                         pProcessingResults->faceInfo[i].faceROI.center.x, pProcessingResults->faceInfo[i].faceROI.center.y,
                         pProcessingResults->faceInfo[i].faceROI.width,    pProcessingResults->faceInfo[i].faceROI.height,
                         pReferenceResults->faceInfo[i].faceROI.center.x,  pReferenceResults->faceInfo[i].faceROI.center.y,
                         pReferenceResults->faceInfo[i].faceROI.width,     pReferenceResults->faceInfo[i].faceROI.height);
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::ConvertFromProcessingResultsToReferenceFaceROIInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::ConvertFromProcessingResultsToReferenceFaceROIInfo(
    FDResults*                    pUnstabilizedResults,
    FDResults*                    pStabilizedResults,
    FaceROIInformation*           pFaceROIInfo,
    const FDPerFrameSettings*     pPerFrameSettings)
{
    CAMX_ASSERT(NULL != pUnstabilizedResults);
    CAMX_ASSERT(NULL != pStabilizedResults);
    CAMX_ASSERT(NULL != pFaceROIInfo);
    CAMX_ASSERT(NULL != pPerFrameSettings);

    CHIRectangle    currentFrameROI;
    CHIRectangle    referenceFrameROI;
    CHIDimension    currentFrameDimension;

    currentFrameDimension.width     = m_FDFrameWidth;
    currentFrameDimension.height    = m_FDFrameHeight;

    CAMX_ASSERT(pUnstabilizedResults->numFacesDetected == pStabilizedResults->numFacesDetected);
    pFaceROIInfo->ROICount  = pUnstabilizedResults->numFacesDetected;
    pFaceROIInfo->requestId = pPerFrameSettings->requestId;

    CAMX_ASSERT(0 != m_sensorDimension.width);
    CAMX_ASSERT(0 != m_sensorDimension.height);
    CAMX_ASSERT(0 != currentFrameDimension.width);
    CAMX_ASSERT(0 != currentFrameDimension.height);
    CAMX_ASSERT(0 != pPerFrameSettings->inputFrameMap.width);
    CAMX_ASSERT(0 != pPerFrameSettings->inputFrameMap.height);

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Current Frame (%dx%d), ReferenceFrame (%dx%d), Map [(%d, %d) %dx%d)",
                     currentFrameDimension.width,               currentFrameDimension.height,
                     m_sensorDimension.width,                   m_sensorDimension.height,
                     pPerFrameSettings->inputFrameMap.left,     pPerFrameSettings->inputFrameMap.top,
                     pPerFrameSettings->inputFrameMap.width,    pPerFrameSettings->inputFrameMap.height);

    for (UINT32 i = 0; i < pFaceROIInfo->ROICount; i++)
    {
        // Convert Unstabilized face ROI
        currentFrameROI.left   = pUnstabilizedResults->faceInfo[i].faceROI.center.x -
                                 (pUnstabilizedResults->faceInfo[i].faceROI.width / 2);
        currentFrameROI.top    = pUnstabilizedResults->faceInfo[i].faceROI.center.y -
                                 (pUnstabilizedResults->faceInfo[i].faceROI.height / 2);
        currentFrameROI.width  = pUnstabilizedResults->faceInfo[i].faceROI.width;
        currentFrameROI.height = pUnstabilizedResults->faceInfo[i].faceROI.height;

        referenceFrameROI = Translator::ConvertROIFromCurrentToReference(&m_sensorDimension,
                                                                         &currentFrameDimension,
                                                                         &pPerFrameSettings->inputFrameMap,
                                                                         &currentFrameROI);

        pFaceROIInfo->unstabilizedROI[i].faceRect.left     = referenceFrameROI.left;
        pFaceROIInfo->unstabilizedROI[i].faceRect.top      = referenceFrameROI.top;
        pFaceROIInfo->unstabilizedROI[i].faceRect.width    = referenceFrameROI.width;
        pFaceROIInfo->unstabilizedROI[i].faceRect.height   = referenceFrameROI.height;
        pFaceROIInfo->unstabilizedROI[i].id                 = pUnstabilizedResults->faceInfo[i].faceID;
        pFaceROIInfo->unstabilizedROI[i].confidence         = pUnstabilizedResults->faceInfo[i].faceROI.confidence;

        if (FDFaceMinConfidence > pFaceROIInfo->unstabilizedROI[i].confidence)
        {
            pFaceROIInfo->unstabilizedROI[i].confidence = FDFaceMinConfidence;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD,
                         "UnstabilizedResults ID[%d] Conf[%d] CurrentFrame:[(%d, %d), %d %d] Reference : [(%d, %d), %d %d]",
                         pFaceROIInfo->unstabilizedROI[i].id,
                         pFaceROIInfo->unstabilizedROI[i].confidence,
                         currentFrameROI.left,   currentFrameROI.top,   currentFrameROI.width,   currentFrameROI.height,
                         referenceFrameROI.left, referenceFrameROI.top, referenceFrameROI.width, referenceFrameROI.height);

        // Convert Stabilized face ROI
        currentFrameROI.left   = pStabilizedResults->faceInfo[i].faceROI.center.x -
                                 (pStabilizedResults->faceInfo[i].faceROI.width / 2);
        currentFrameROI.top    = pStabilizedResults->faceInfo[i].faceROI.center.y -
                                 (pStabilizedResults->faceInfo[i].faceROI.height / 2);
        currentFrameROI.width  = pStabilizedResults->faceInfo[i].faceROI.width;
        currentFrameROI.height = pStabilizedResults->faceInfo[i].faceROI.height;

        referenceFrameROI = Translator::ConvertROIFromCurrentToReference(&m_sensorDimension,
                                                                         &currentFrameDimension,
                                                                         &pPerFrameSettings->inputFrameMap,
                                                                         &currentFrameROI);

        pFaceROIInfo->stabilizedROI[i].faceRect.left     = referenceFrameROI.left;
        pFaceROIInfo->stabilizedROI[i].faceRect.top      = referenceFrameROI.top;
        pFaceROIInfo->stabilizedROI[i].faceRect.width    = referenceFrameROI.width;
        pFaceROIInfo->stabilizedROI[i].faceRect.height   = referenceFrameROI.height;
        pFaceROIInfo->stabilizedROI[i].id                = pStabilizedResults->faceInfo[i].faceID;
        pFaceROIInfo->stabilizedROI[i].confidence        = pStabilizedResults->faceInfo[i].faceROI.confidence;

        if (FDFaceMinConfidence > pFaceROIInfo->stabilizedROI[i].confidence)
        {
            pFaceROIInfo->stabilizedROI[i].confidence = FDFaceMinConfidence;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD,
                         "StabilizedResults ID[%d] Conf[%d] CurrentFrame:[(%d, %d), %d %d] Reference : [(%d, %d), %d %d]",
                         pFaceROIInfo->stabilizedROI[i].id,
                         pFaceROIInfo->stabilizedROI[i].confidence,
                         currentFrameROI.left,   currentFrameROI.top,   currentFrameROI.width,   currentFrameROI.height,
                         referenceFrameROI.left, referenceFrameROI.top, referenceFrameROI.width, referenceFrameROI.height);
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::CheckROIBound
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::CheckROIBound(
    FDResults*  pResults,
    INT32       width,
    INT32       height)
{
    CHIRectangle    currentROI;
    INT32           faceCount   = pResults->numFacesDetected;
    INT32           index       = 0;

    for (INT32 i = 0; i < faceCount; i++)
    {
        currentROI.left   = pResults->faceInfo[i].faceROI.center.x - (pResults->faceInfo[i].faceROI.width / 2);
        currentROI.top    = pResults->faceInfo[i].faceROI.center.y - (pResults->faceInfo[i].faceROI.height / 2);
        currentROI.width  = pResults->faceInfo[i].faceROI.width;
        currentROI.height = pResults->faceInfo[i].faceROI.height;

        if (currentROI.left < 0)
        {
            currentROI.width = currentROI.left + currentROI.width;
            currentROI.left  = 0;
        }

        if (currentROI.top < 0)
        {
            currentROI.height = currentROI.top + currentROI.height;
            currentROI.top    = 0;
        }

        if ((currentROI.left + currentROI.width) > width)
        {
            currentROI.width = width - currentROI.left;
        }

        if ((currentROI.top + currentROI.height) > height)
        {
            currentROI.height = height - currentROI.top;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD,
                         "[i=%d][index=%d] : ROI input [center(%d, %d), width=%d, height=%d], "
                         "output [center(%d, %d), width=%d, height=%d]",
                         i, index,
                         pResults->faceInfo[i].faceROI.center.x,   pResults->faceInfo[i].faceROI.center.y,
                         pResults->faceInfo[i].faceROI.width,      pResults->faceInfo[i].faceROI.height,
                         currentROI.left + (currentROI.width / 2), currentROI.top  + (currentROI.height / 2),
                         currentROI.width,                         currentROI.height);

        if ((currentROI.width > 0) && (currentROI.height > 0))
        {

            if (index != i)
            {
                pResults->faceInfo[index] = pResults->faceInfo[i];
            }

            pResults->faceInfo[index].faceROI.center.x = currentROI.left + (currentROI.width / 2);
            pResults->faceInfo[index].faceROI.center.y = currentROI.top  + (currentROI.height / 2);
            pResults->faceInfo[index].faceROI.width    = currentROI.width;
            pResults->faceInfo[index].faceROI.height   = currentROI.height;

            index++;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "ROI out of bound! index = %d, center = (%d, %d), width = %d, height = %d",
                           i,
                           pResults->faceInfo[i].faceROI.center.x,
                           pResults->faceInfo[i].faceROI.center.y,
                           pResults->faceInfo[i].faceROI.width,
                           pResults->faceInfo[i].faceROI.height);

            pResults->numFacesDetected--;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::ExecuteFDResultsProcessing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::ExecuteFDResultsProcessing(
    const FDHwResults*      pHWResults,
    FDPerFrameSettings*     pPerFrameSettings)
{
    FDIntermediateResults   hwResults   = { 0 };
    FDResults               unstabilizedResults;
    FDResults               swFDResults;
    CamxResult              result      = CamxResultSuccess;

    CAMX_ASSERT(NULL != pHWResults);

    CAMX_TRACE_SYNC_BEGIN_F(CamxLogGroupFD, "ExecuteFDResultsProcessing");

    if (((TRUE  == m_bEnableSwDetection) && (NULL == m_hFDEngineSWHandle)) ||
        ((FALSE == m_bEnableSwDetection) && (NULL == m_hFDEngineFPFilterHandle)))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "FD Engine not ready, skip reqid %llu", pPerFrameSettings->requestId);
        m_skippedFrameCnt++;
        m_processedFrameCnt--;
    }
    else
    {
        if (TRUE == m_bEnableSwDetection)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupFD, "SW only Execution reqid %llu", pPerFrameSettings->requestId);
            // SW only processing
            result = FDSWOnlyExecution(pPerFrameSettings, &swFDResults);
            if (CamxResultSuccess == result)
            {
                PostProcessFDEngineResults(pPerFrameSettings, &swFDResults);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, " Failed in SWonly Execution");
            }
        }
        else
        {
            if (FDSWIntermittent == pPerFrameSettings->FDManagerExecutionSequence)
            {
                CAMX_LOG_VERBOSE(CamxLogGroupFD, "SW Intermittent Execution");
                result = FDSWOnlyExecution(pPerFrameSettings, &swFDResults);
                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, " Failed in SWIntermittent Execution");
                }
            }
            else if (FDFPFiltering == pPerFrameSettings->FDManagerExecutionSequence)
            {
                hwResults.numFacesDetected = pHWResults->faceCount.dnum;

                // If SWIntermittent is enabled we need to merge both HW and intermittent results
                if (FDLimitHWFaces < hwResults.numFacesDetected)
                {
                    hwResults.numFacesDetected = FDLimitHWFaces;
                }

                for (INT i = 0; i < hwResults.numFacesDetected; i++)
                {
                    hwResults.faceInfo[i].faceID             = 0;
                    hwResults.faceInfo[i].newFace            = TRUE;
                    hwResults.faceInfo[i].faceROI.confidence =
                        m_FDHwUtils.GetConfidenceFromRegValue(m_FDHwVersion, pHWResults->faces[i].sizeConf.conf);
                    hwResults.faceInfo[i].faceROI.center.x   = pHWResults->faces[i].centerX.centerX;
                    hwResults.faceInfo[i].faceROI.center.y   = pHWResults->faces[i].centerY.centerY;
                    hwResults.faceInfo[i].faceROI.width      = pHWResults->faces[i].sizeConf.size;
                    hwResults.faceInfo[i].faceROI.height     = pHWResults->faces[i].sizeConf.size;
                    hwResults.faceInfo[i].faceROI.rollAngle  = pHWResults->faces[i].anglePose.angle;
                    hwResults.faceInfo[i].faceROI.pose       =
                            m_FDHwUtils.GetPoseAngleFromRegValue(pHWResults->faces[i].anglePose.pose);

                    CAMX_LOG_VERBOSE(CamxLogGroupFD, "HW results : Face[%d] : ID=%d, confidence=%d,"
                                     "center=(%d,%d), width=%d, height=%d, roll=%d, pose=%d", i, 0,
                                     pHWResults->faces[i].sizeConf.conf,
                                     pHWResults->faces[i].centerX.centerX, pHWResults->faces[i].centerY.centerY,
                                     pHWResults->faces[i].sizeConf.size, pHWResults->faces[i].sizeConf.size,
                                     pHWResults->faces[i].anglePose.angle, pHWResults->faces[i].anglePose.pose);
                }

                PrintFDResults("HWFDResults", pPerFrameSettings->requestId, m_FDFrameWidth, m_FDFrameHeight,
                    hwResults.numFacesDetected, &hwResults.faceInfo[0]);
                // Process false positive filtering
                ProcessFDEngineResults(pPerFrameSettings, &hwResults, &unstabilizedResults);
                PostProcessFDEngineResults(pPerFrameSettings, &unstabilizedResults);
            }
        }
    }

    CAMX_TRACE_SYNC_END(CamxLogGroupFD);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::GetInputFrameImageBufferInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::GetInputFrameImageBufferInfo(
    PerRequestActivePorts* pEnabledPorts,
    FDPerFrameSettings*    pPerFrameSettings)
{
    PerRequestInputPortInfo*    pInputPort;
    CamxResult                  result  = CamxResultEFailed;
    ImageFormat                 imageFormat;

    CAMX_ASSERT(NULL != pEnabledPorts);
    CAMX_ASSERT(NULL != pPerFrameSettings);

    for (UINT i = 0; i < pEnabledPorts->numInputPorts; i++)
    {
        pInputPort = &pEnabledPorts->pInputPorts[i];

        if (FDManagerInputPortImage == pInputPort->portId)
        {
            if ((NULL != pInputPort->pImageBuffer) &&
                (NULL != pInputPort->pImageBuffer->GetFormat()) &&
                (NULL != pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 0)))
            {
                pPerFrameSettings->pImageBuffer = pInputPort->pImageBuffer;

                imageFormat = *pInputPort->pImageBuffer->GetFormat();
                if (FALSE == ImageFormatUtils::IsYUV(&imageFormat))
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "Buffer format is not YUV");
                }

                pPerFrameSettings->pImageAddress = pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 0);
                CAMX_LOG_VERBOSE(CamxLogGroupFD, "pImageAddress %p", pPerFrameSettings->pImageAddress);

                if ((NULL != pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 1)) &&
                    ((FD_DL_ARM == GetStaticSettings()->FDFilterEngine) ||
                     (FD_DL_DSP == GetStaticSettings()->FDFilterEngine)))
                {
                    pPerFrameSettings->pUVImageAddress = pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 1);
                }

                result = CamxResultSuccess;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "Input Port %p pInputPort->portId %d pInputPort->pImageBuffer %p ",
                               pInputPort, pInputPort->portId, pInputPort->pImageBuffer);

                result = CamxResultEInvalidArg;
            }

            break;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::GetAndProcessFDResults
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::GetAndProcessFDResults(
    UINT64                 requestId,
    PerRequestActivePorts* pEnabledPorts,
    FDPerFrameSettings*    pPerFrameSettings,
    BOOL*                  pRequestHandlingDone)
{
    CamxResult                  result  = CamxResultSuccess;
    FDHwResults                 hwResults;
    PerRequestInputPortInfo*    pInputPort;
    FDManagerProcessRequestData processRequestData = { 0 };
    ImageFormat                 imageFormat;

    CAMX_ASSERT(NULL != pEnabledPorts);
    CAMX_ASSERT(NULL != pPerFrameSettings);

    for (UINT i = 0; i < pEnabledPorts->numInputPorts; i++)
    {
        pInputPort = &pEnabledPorts->pInputPorts[i];

        if (FDManagerInputPortImage == pInputPort->portId)
        {
            if ((NULL != pInputPort->pImageBuffer) &&
                (NULL != pInputPort->pImageBuffer->GetFormat()))
            {
                imageFormat = *pInputPort->pImageBuffer->GetFormat();
                if (FALSE == ImageFormatUtils::IsYUV(&imageFormat))
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "Buffer format is not YUV");
                }
            }

            if ((NULL != pInputPort->pImageBuffer) &&
                (NULL != pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 0)))
            {
                if (TRUE == GetStaticSettings()->useFDInternalBuffers)
                {
                    UINT frameIndex = requestId % MaxRequestQueueDepth;

                    CAMX_LOG_VERBOSE(CamxLogGroupFD, "frameIndex=%d, internal=%p, incoming=%p incomingUV=%p",
                                     frameIndex,
                                     m_pImageAddress[frameIndex], pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 0),
                                     pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 1));

                    Utils::Memcpy(m_pImageAddress[frameIndex], pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 0),
                                  m_FDFrameStride * m_FDFrameScanline);

                    pPerFrameSettings->pImageAddress = m_pImageAddress[frameIndex];

                    if ((NULL != pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 1)) &&
                        ((FD_DL_ARM == GetStaticSettings()->FDFilterEngine) ||
                         (FD_DL_DSP == GetStaticSettings()->FDFilterEngine)))
                    {
                        Utils::Memcpy(m_pUVImageAddress[frameIndex], pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 1),
                                      (m_FDFrameStride * m_FDFrameScanline) / 2);
                        pPerFrameSettings->pUVImageAddress = m_pUVImageAddress[frameIndex];
                    }
                }
                else
                {
                    if ((NULL != pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 1)) &&
                        ((FD_DL_ARM == GetStaticSettings()->FDFilterEngine) ||
                         (FD_DL_DSP == GetStaticSettings()->FDFilterEngine)))
                    {
                        pPerFrameSettings->pUVImageAddress = pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 1);
                    }
                    pPerFrameSettings->pImageAddress = pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 0);
                    CAMX_LOG_VERBOSE(CamxLogGroupFD, "pImageAddress %p", pPerFrameSettings->pImageAddress);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "Input Port %p pInputPort->portId %d pInputPort->pImageBuffer %p ",
                               pInputPort, pInputPort->portId, pInputPort->pImageBuffer);

                result = CamxResultEInvalidArg;
            }
        }
        else if ((FDManagerInputPortHwResults == pInputPort->portId) && (FALSE == m_bEnableSwDetection))
        {
            if ((NULL != pInputPort->pImageBuffer) &&
                (NULL != pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 0)))
            {
                hwResults = *(reinterpret_cast<FDHwResults *>(pInputPort->pImageBuffer->GetPlaneVirtualAddr(0, 0)));
                CAMX_LOG_VERBOSE(CamxLogGroupFD, "face count %d", hwResults.faceCount.dnum);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "Input Port %p pInputPort->portId %d pInputPort->pImageBuffer %p ",
                               pInputPort, pInputPort->portId, pInputPort->pImageBuffer);

                result = CamxResultEInvalidArg;
            }
        }
    }

    CAMX_ASSERT(NULL != pPerFrameSettings->pImageAddress);

    processRequestData.requestId        = requestId;
    processRequestData.perFrameSettings = *pPerFrameSettings;
    processRequestData.hwResults        = hwResults;

    result = OnProcessRequest(&processRequestData, pRequestHandlingDone);

    if (TRUE == GetStaticSettings()->useFDInternalBuffers)
    {
        *pRequestHandlingDone = TRUE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::PublishFDInternalFrameSettings()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::PublishFDInternalFrameSettings(
    FDPerFrameSettings* pPerFrameSettings)
{
    CamxResult          result          = CamxResultSuccess;
    static const UINT   PropertyTag[1]  = { PropertyIDFDInternalPerFrameSettings };
    const VOID*         pData[1]        = { 0 };
    UINT                pDataCount[1]   = { 0 };

    // Publish per frame settings into internal pool
    pDataCount[0]   = sizeof(*pPerFrameSettings);
    pData[0]        = pPerFrameSettings;

    result = WriteDataList(PropertyTag, pData, pDataCount, 1);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in publishing FD frame settings, result=%d", result);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::PublishFDFrameSettings()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::PublishFDFrameSettings(
    FDPerFrameSettings* pPerFrameSettings)
{
    CamxResult          result          = CamxResultSuccess;
    static const UINT   PropertyTag[1]  = { PropertyIDFDFrameSettings };
    const VOID*         pData[1]        = { 0 };
    UINT                pDataCount[1]   = { 0 };

    // Publish settings into results pool. FDHw node reads this and process accordingly
    pDataCount[0]   = sizeof(pPerFrameSettings->frameSettings);
    pData[0]        = &pPerFrameSettings->frameSettings;

    result = WriteDataList(PropertyTag, pData, pDataCount, 1);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in publishing FD frame settings, result=%d", result);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::GetFDFrameSettings()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::GetFDFrameSettings(
    FDPerFrameSettings* pPerFrameSettings)
{
    CamxResult          result          = CamxResultSuccess;
    static const UINT   PropertyTag[1]  = { PropertyIDFDInternalPerFrameSettings };
    VOID*               pData[1]        = { 0 };
    UINT64              dataOffset[1]   = { 0 };

    result = GetDataList(PropertyTag, pData, dataOffset, 1);

    if (NULL != pData[0])
    {
        *pPerFrameSettings = *(reinterpret_cast<FDPerFrameSettings*>(pData[0]));
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Error getting FD internal per frame settings");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::PublishFDResults()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::PublishFDResults(
    FDPerFrameSettings* pPerFrameSettings)
{
    CamxResult          result      = CamxResultSuccess;
    FaceROIInformation  faceROIInfo = { 0 };

    GetFDResultsFromUsecasePool(&faceROIInfo);

    m_numFacesDetected = faceROIInfo.ROICount;

    if (TRUE == pPerFrameSettings->postResultsToProperty)
    {
        PublishFDResultsToVendorTag(pPerFrameSettings, &faceROIInfo);
    }

    if (TRUE == pPerFrameSettings->postResultsToMetaData)
    {
        PublishFDResultsToMetadata(pPerFrameSettings, &faceROIInfo);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::PublishFDResultsToMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::PublishFDResultsToMetadata(
    FDPerFrameSettings* pPerFrameSettings,
    FaceROIInformation* pFaceROIInfo)
{
    CamxResult          result            = CamxResultSuccess;
    FDMetaDataResults   metaDataResults   = {};
    CHIRectangle        currentFrameROI   = {};
    CHIRectangle        referenceFrameROI = {};

    CAMX_ASSERT(NULL != pPerFrameSettings);
    CAMX_ASSERT(NULL != pFaceROIInfo);
    CAMX_ASSERT(0    != m_sensorDimension.width);
    CAMX_ASSERT(0    != m_sensorDimension.height);
    CAMX_ASSERT(0    != m_activeArrayDimension.width);
    CAMX_ASSERT(0    != m_activeArrayDimension.height);

    metaDataResults.numFaces = pFaceROIInfo->ROICount;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] : MetaData result Face count %d",
                     InstanceID(), pPerFrameSettings->requestId, metaDataResults.numFaces);

    for (INT i = 0; i < metaDataResults.numFaces; i++)
    {
        // Use stabilized face information to post into metadata results
        currentFrameROI.left   = pFaceROIInfo->stabilizedROI[i].faceRect.left;
        currentFrameROI.top    = pFaceROIInfo->stabilizedROI[i].faceRect.top;
        currentFrameROI.width  = pFaceROIInfo->stabilizedROI[i].faceRect.width;
        currentFrameROI.height = pFaceROIInfo->stabilizedROI[i].faceRect.height;

        referenceFrameROI = Translator::ConvertROIFromCurrentToReference(&m_activeArrayDimension,
                                                                         &m_sensorDimension,
                                                                         &m_CAMIFMap,
                                                                         &currentFrameROI);

        metaDataResults.faceID[i]                   = pFaceROIInfo->stabilizedROI[i].id;
        metaDataResults.faceScore[i]                = (pFaceROIInfo->stabilizedROI[i].confidence / 10);
        metaDataResults.faceRect[i].topLeft.x       = referenceFrameROI.left;
        metaDataResults.faceRect[i].topLeft.y       = referenceFrameROI.top;
        metaDataResults.faceRect[i].bottomRight.x   = referenceFrameROI.left + referenceFrameROI.width;
        metaDataResults.faceRect[i].bottomRight.y   = referenceFrameROI.top  + referenceFrameROI.height;

        CAMX_LOG_VERBOSE(CamxLogGroupFD,
                         "Instance[%d] ReqId[%llu] MetaData Face[%d] : ID=%d, score=%d, top-left(%d, %d), bottomRight=(%d, %d)",
                         InstanceID(),                              pPerFrameSettings->requestId,               i,
                         metaDataResults.faceID[i],                 metaDataResults.faceScore[i],
                         metaDataResults.faceRect[i].topLeft.x,     metaDataResults.faceRect[i].topLeft.y,
                         metaDataResults.faceRect[i].bottomRight.x, metaDataResults.faceRect[i].bottomRight.y);
    }

    static const UINT PropertiesFDMetadataResults[] =
    {
        StatisticsFaceDetectMode,
        StatisticsFaceRectangles,
        StatisticsFaceScores
    };

    static const UINT   Length              = CAMX_ARRAY_SIZE(PropertiesFDMetadataResults);
    const  VOID*        pData[Length]       = { 0 };
    UINT                pDataCount[Length]  = { 0 };
    UINT                listlength          = Length;

    if (StatisticsFaceDetectModeValues::StatisticsFaceDetectModeOff == pPerFrameSettings->fdModeReceived)
    {
        listlength = 1;
    }

    for (UINT i = 0; i < listlength; i++)
    {
        switch (PropertiesFDMetadataResults[i])
        {
            case StatisticsFaceDetectMode:
                pDataCount[i] = 1;
                pData[i] = &pPerFrameSettings->fdModeReceived;
                break;
            case StatisticsFaceRectangles:
                pDataCount[i] = metaDataResults.numFaces * (sizeof(FDMetaDataFaceRect) / sizeof(INT32));
                pData[i] = &metaDataResults.faceRect[0];
                break;
            case StatisticsFaceScores:
                pDataCount[i] = metaDataResults.numFaces;
                pData[i] = &metaDataResults.faceScore[0];
                break;
            default:
                break;
        }
    }

    result = WriteDataList(PropertiesFDMetadataResults, pData, pDataCount, listlength);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::PublishFDResultsToVendorTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::PublishFDResultsToVendorTag(
    FDPerFrameSettings* pPerFrameSettings,
    FaceROIInformation* pFaceROIInfo)
{
    CAMX_UNREFERENCED_PARAM(pPerFrameSettings);
    CAMX_ASSERT(NULL != pFaceROIInfo);

    CamxResult  result            = CamxResultSuccess;
    UINT32      metaTag           = 0;
    UINT        resultTag[1]      = { 0 };
    const       VOID* pData[1]    = { 0 };
    UINT        pDataCount[1]     = { 0 };
    UINT        length            = CAMX_ARRAY_SIZE(resultTag);

    PrintFDFaceROIInfo("ReferenceFrameTag", pPerFrameSettings->requestId, m_sensorDimension.width,
                       m_sensorDimension.height, pFaceROIInfo);

    pDataCount[0] = sizeof(FaceROIInformation);
    pData[0]      = pFaceROIInfo;

    result = VendorTagManager::QueryVendorTagLocation(VendorTagSectionOEMFDResults,
                                                      VendorTagNameOEMFDResults,
                                                      &metaTag);

    if (CamxResultSuccess == result)
    {
        resultTag[0] = metaTag;

        result = WriteDataList(resultTag, pData, pDataCount, length);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::PublishFDResultstoUsecasePool
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::PublishFDResultstoUsecasePool(
    FaceROIInformation* pFaceROIInfo)
{
    if (NULL == pFaceROIInfo)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Falied to publish FD results to uscasepool, input NULL");
    }
    else
    {
        CamxResult  result          = CamxResultSuccess;
        const UINT  outputTags[]    = { PropertyIDUsecaseFDResults };
        const VOID* pOutputData[1]  = { 0 };
        UINT        pDataCount[1]   = { 0 };

        pDataCount[0]   = sizeof(FaceROIInformation);
        pOutputData[0]  = pFaceROIInfo;
        result          = WriteDataList(outputTags, pOutputData, pDataCount, 1);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Falied to publish FD results to uscasepool");
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Publish FD result reqId %llu count %u",
                             pFaceROIInfo->requestId,
                             pFaceROIInfo->ROICount);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::GetFDResultsFromUsecasePool
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::GetFDResultsFromUsecasePool(
    FaceROIInformation* pFaceROIInfo)
{
    MetadataPool* pPerUsecasePool = GetPerFramePool(PoolType::PerUsecase);
    MetadataSlot* pCurrentSlot    = pPerUsecasePool->GetSlot(0);

    CAMX_ASSERT(NULL != pFaceROIInfo);

    static const UINT FDResultTags[]      = { PropertyIDUsecaseFDResults };
    VOID*             pData[1]            = { 0 };
    UINT64            dataOffset[1]       = { 0 };

    GetDataList(FDResultTags, pData, dataOffset, 1);

    if (NULL != pData[0])
    {
        *pFaceROIInfo = *(static_cast<FaceROIInformation*>(pData[0]));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::SetDependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::SetDependencies(
    NodeProcessRequestData* pNodeRequestData,
    PerRequestActivePorts*  pEnabledPorts,
    FDPerFrameSettings*     pPerFrameSettings)
{
    UINT numDependencyUnits = 0;

    if (FALSE == pPerFrameSettings->skipProcess)
    {
        UINT* pFenceCount = &pNodeRequestData->dependencyInfo[numDependencyUnits].bufferDependency.fenceCount;

        for (UINT i = 0; i < pEnabledPorts->numInputPorts; i++)
        {
            PerRequestInputPortInfo* pPerRequestInputPort = &pEnabledPorts->pInputPorts[i];

            if (NULL != pPerRequestInputPort)
            {
                pNodeRequestData->dependencyInfo[numDependencyUnits].bufferDependency.phFences[*pFenceCount]            =
                    pPerRequestInputPort->phFence;
                pNodeRequestData->dependencyInfo[numDependencyUnits].bufferDependency.pIsFenceSignaled[*pFenceCount]    =
                    pPerRequestInputPort->pIsFenceSignaled;
                pNodeRequestData->dependencyInfo[numDependencyUnits].dependencyFlags.hasInputBuffersReadyDependency     =
                    TRUE;
                pNodeRequestData->dependencyInfo[numDependencyUnits].bufferDependency.fenceCount++;
                pNodeRequestData->dependencyInfo[numDependencyUnits].processSequenceId                                  =
                    static_cast<UINT32>(DependencySequence::DependencySetHwResults);
                pNodeRequestData->dependencyInfo[numDependencyUnits].dependencyFlags.hasIOBufferAvailabilityDependency  =
                    TRUE;
            }
        }

        numDependencyUnits++;
    }

    if (FALSE == GetStaticSettings()->fastFDMetadata)
    {
        // Post results when IFE buffer returns
        for (UINT i = 0; i < pEnabledPorts->numInputPorts; i++)
        {
            UINT* pFenceCount = &pNodeRequestData->dependencyInfo[numDependencyUnits].bufferDependency.fenceCount;

            PerRequestInputPortInfo* pPerRequestInputPort = &pEnabledPorts->pInputPorts[i];

            if ((NULL != pPerRequestInputPort) && (FDManagerInputPortImage == pPerRequestInputPort->portId))
            {
                pNodeRequestData->dependencyInfo[numDependencyUnits].bufferDependency.phFences[*pFenceCount]            =
                    pPerRequestInputPort->phFence;
                pNodeRequestData->dependencyInfo[numDependencyUnits].bufferDependency.pIsFenceSignaled[*pFenceCount]    =
                    pPerRequestInputPort->pIsFenceSignaled;
                pNodeRequestData->dependencyInfo[numDependencyUnits].dependencyFlags.hasInputBuffersReadyDependency     =
                    TRUE;
                pNodeRequestData->dependencyInfo[numDependencyUnits].bufferDependency.fenceCount++;
                pNodeRequestData->dependencyInfo[numDependencyUnits].processSequenceId =
                    static_cast<UINT32>(DependencySequence::DependencySetPublish);

                pNodeRequestData->dependencyInfo[numDependencyUnits].dependencyFlags.hasIOBufferAvailabilityDependency  = TRUE;
            }
        }
    }
    else
    {
        // Post results when IFE has finished frame setup
        pNodeRequestData->dependencyInfo[numDependencyUnits].propertyDependency.properties[0] = PropertyIDIFEDigitalZoom;
        pNodeRequestData->dependencyInfo[numDependencyUnits].propertyDependency.count = 1;

        pNodeRequestData->dependencyInfo[numDependencyUnits].dependencyFlags.hasPropertyDependency = TRUE;

        pNodeRequestData->dependencyInfo[numDependencyUnits].processSequenceId =
            static_cast<UINT32>(DependencySequence::DependencySetPublish);
    }

    // Only if a dependency is setup should the unit count be increased
    if (0 != pNodeRequestData->dependencyInfo[numDependencyUnits].dependencyFlags.dependencyFlagsMask)
    {
        numDependencyUnits++;
    }

    if ((FALSE   == pPerFrameSettings->skipProcess)       &&
        (Disable != GetStaticSettings()->FDPreprocessing) &&
        (NULL    != m_preprocessingData.hLibHandle))
    {
        pNodeRequestData->dependencyInfo[numDependencyUnits].propertyDependency.properties[0]      = PropertyIDAECFrameControl;
        pNodeRequestData->dependencyInfo[numDependencyUnits].propertyDependency.count              = 1;

        pNodeRequestData->dependencyInfo[numDependencyUnits].dependencyFlags.hasPropertyDependency = TRUE;

        pNodeRequestData->dependencyInfo[numDependencyUnits].processSequenceId                     =
            static_cast<UINT32>(DependencySequence::DependencySetProperties);

        numDependencyUnits++;
    }


    pNodeRequestData->numDependencyLists = numDependencyUnits;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::FDManagerThreadCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* FDManagerNode::FDManagerThreadCb(
    VOID* pArg)
{
    if (NULL != pArg)
    {
        CamxResult           result         = CamxResultSuccess;
        FDManagerThreadData* pThreadData    = static_cast<FDManagerThreadData*>(pArg);
        FDManagerNode*       pFDManagerNode = static_cast<FDManagerNode*>(pThreadData->pInstance);

        CAMX_ASSERT(NULL != pFDManagerNode);

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Job type: %d", pThreadData->jobType);

        switch (pThreadData->jobType)
        {
            case FDManagerThreadJobFDEngineSetup:
                pFDManagerNode->FDManagerJobFDEngineSetupHandler(pThreadData);
                break;
            case FDManagerThreadJobProcessRequest:
                pFDManagerNode->FDManagerJobProcessRequestHandler(pThreadData);
                break;
            default:
                CAMX_LOG_ERROR(CamxLogGroupFD, "Invalid job type: %d", pThreadData->jobType);
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "NULL arg in FDManagerThreadCb");
    }

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::FDManagerJobFDEngineSetupHandler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::FDManagerJobFDEngineSetupHandler(
    FDManagerThreadData* pThreadData)
{
    CamxResult            result                = CamxResultSuccess;
    FDEngineCreateParams* pFDEngineCreateParams = &(pThreadData->jobInfo.fdEngineCreateInfo.createParams);
    FDEngineHandle*       phFDEngineHandle      = NULL;
    FDEngineConfig        engineConfig;

    switch (pFDEngineCreateParams->clientType)
    {
        case ClientHW:
            phFDEngineHandle = &m_hFDEngineFPFilterHandle;
            break;
        case ClientSW:
            phFDEngineHandle = &m_hFDEngineSWHandle;
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupFD, "Invalid client type!");
    }

    SetUpFDEngine(pFDEngineCreateParams, phFDEngineHandle);

    ReleaseLocalThreadDataMemory(pThreadData);

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::FDManagerJobProcessRequestHandler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::FDManagerJobProcessRequestHandler(
    FDManagerThreadData* pThreadData)
{
    FDManagerNode*               pFDManagerNode      = static_cast<FDManagerNode*>(pThreadData->pInstance);
    FDManagerProcessRequestData* pProcessRequestInfo = &(pThreadData->jobInfo.processRequestInfo);

    CAMX_TRACE_SYNC_BEGIN_F(CamxLogGroupFD, "FDManagerNode::FDManagerJobProcessRequestHandler");

    if (FALSE == pFDManagerNode->IsFDManagerNodeStreamOff())
    {
        if (NULL != pProcessRequestInfo)
        {
            ExecuteFDResultsProcessing(&pProcessRequestInfo->hwResults,
                &pProcessRequestInfo->perFrameSettings);

            if (TRUE == GetStaticSettings()->enableOfflineFD)
            {
                CAMX_LOG_INFO(CamxLogGroupFD, "Offline FD: Publish FD results and metadataDone");
                PublishFDResults(&pProcessRequestInfo->perFrameSettings);

                ProcessPartialMetadataDone(pProcessRequestInfo->requestId);
                ProcessMetadataDone(pProcessRequestInfo->requestId);
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Prcoess request info a null pointer");
        }

        ReleaseLocalThreadDataMemory(pThreadData);
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Do not process as node streamed off");
    }

    if (FALSE == pFDManagerNode->GetStaticSettings()->useFDInternalBuffers)
    {
        if ((NULL != pProcessRequestInfo->perFrameSettings.pImageBuffer) &&
            (TRUE == pProcessRequestInfo->perFrameSettings.inputBufferInvalidateRequired))
        {
            pProcessRequestInfo->perFrameSettings.pImageBuffer->CacheOps(true, false);
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] ProcessRequestIdDone",
            pFDManagerNode->InstanceID(), pProcessRequestInfo->requestId);
        if (FALSE == pFDManagerNode->GetStaticSettings()->enableOfflineFD)
        {
            pFDManagerNode->ProcessRequestIdDone(pProcessRequestInfo->requestId);
        }
        else
        {
            for (UINT i = 0; i < pFDManagerNode->m_pPerRequestPorts->numOutputPorts; i++)
            {
                PerRequestOutputPortInfo* pOutputPort = &pFDManagerNode->m_pPerRequestPorts->pOutputPorts[i];

                if (pOutputPort == NULL)
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "pOutoutPort is NULL");
                    return;
                }

                if (TRUE == pFDManagerNode->IsSinkPortNoBuffer(pOutputPort->portId))
                {
                    pFDManagerNode->ProcessRequestIdDone(pProcessRequestInfo->requestId);
                }
                else
                {
                    FaceROIInformation  faceROIInfo = { 0 };
                    pFDManagerNode->GetFDResultsFromUsecasePool(&faceROIInfo);

                    Utils::Memcpy(pOutputPort->ppImageBuffer[0], &faceROIInfo, sizeof(FaceROIInformation));

                    if ((NULL != pOutputPort) && (NULL != pOutputPort->phFence))
                    {
                        CamxResult result = CamxResultSuccess;
                        result = CSLFenceSignal(*pOutputPort->phFence, CSLFenceResultSuccess);
                        if (CamxResultSuccess != result)
                        {
                            CAMX_LOG_ERROR(CamxLogGroupFD, "Fence signal error: %s", Utils::CamxResultToString(result));
                        }
                    }
                }
            }
        }
    }

    CAMX_TRACE_SYNC_END(CamxLogGroupFD);

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::OnProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::OnProcessRequest(
    FDManagerProcessRequestData*    pProcessRequestData,
    BOOL*                           pProcessHandled)
{
    CamxResult           result         = CamxResultSuccess;
    const FDConfig*      pFDConfig      = &pProcessRequestData->perFrameSettings.fdConfig;

    if ((TRUE == GetStaticSettings()->enableFDManagerThreading) && (FALSE == m_bEnableSwDetection))
    {
        UINT32 jobCount = m_pThreadManager->GetJobCount(m_hThread);

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Current JobCount=%d, InFlightCount=%d, allowMax=%d",
                         jobCount, m_pThreadManager->GetInFlightCount(m_hThread), pFDConfig->maxPendingFrames);

        if ((0 != pFDConfig->maxPendingFrames) && (jobCount >= pFDConfig->maxPendingFrames))
        {
            // If we already have 2 jobs pending in the queue(1 processing, 1 waiting), skip processing this frame.
            // We should actually remove the waiting job and add the current
            *pProcessHandled = TRUE;

            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Skipping frame ID %llu (Pending frames %d)",
                             pProcessRequestData->requestId, jobCount);

            m_skippedFrameCnt++;
            m_processedFrameCnt--;
        }
        else
        {
            FDManagerThreadData* pThreadData = GetLocalThreadDataMemory();
            if (NULL != pThreadData)
            {
                pThreadData->pInstance                  = this;
                pThreadData->jobType                    = FDManagerThreadJobProcessRequest;
                pThreadData->jobInfo.processRequestInfo = *pProcessRequestData;
                result = m_pThreadManager->PostJob(m_hThread,
                                                   NULL,
                                                   reinterpret_cast<VOID**>(&pThreadData),
                                                   FALSE,
                                                   FALSE);
                *pProcessHandled = FALSE;

                CAMX_LOG_VERBOSE(CamxLogGroupFD, "Request Scheduled FD Manager processing %llu (Pending frames %d)",
                                 pProcessRequestData->requestId, jobCount);
            }
            else
            {
                result = CamxResultENoMemory;
                CAMX_LOG_ERROR(CamxLogGroupFD, "FD manager thread data allocation error: %s",
                               Utils::CamxResultToString(result));
            }
        }
    }
    else
    {
        result = ExecuteFDResultsProcessing(&pProcessRequestData->hwResults,
                                            &pProcessRequestData->perFrameSettings);
        *pProcessHandled = TRUE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::GetLocalThreadDataMemory
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FDManagerThreadData* FDManagerNode::GetLocalThreadDataMemory()
{
    FDManagerThreadData* pThreadData = NULL;

    m_pLock->Lock();
    LDLLNode* pNode = m_threadDataList.RemoveFromHead();

    if (NULL != pNode)
    {
        pThreadData = static_cast<FDManagerThreadData*>(pNode->pData);
        CAMX_FREE(pNode);
        pNode = NULL;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("GetLocalThreadDataMemory ran out entries. Consider increasing the list size");

        pThreadData = CAMX_NEW FDManagerThreadData;

        if (NULL == pThreadData)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Failed to Create thread data - out of memory");
        }
    }

    m_pLock->Unlock();
    return pThreadData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::ReleaseLocalThreadDataMemory
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::ReleaseLocalThreadDataMemory(
    FDManagerThreadData* pThreadData)
{
    m_pLock->Lock();

    LDLLNode* pNode = static_cast<LDLLNode*>(CAMX_CALLOC(sizeof(LDLLNode)));

    if (NULL != pNode)
    {
        pNode->pData = pThreadData;

        // Always push the thread data back to the list.
        m_threadDataList.InsertToTail(pNode);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Failed to release memory to FD manager thread data list - no memory for list node");
        CAMX_DELETE pThreadData;
    }
    m_pLock->Unlock();

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::PostProcessFDEngineResults
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::PostProcessFDEngineResults(
    const FDPerFrameSettings* pPerFrameSettings,
    FDResults*                pUnstabilizedResults)
{
    FDResults               stabilizedResults;
    FaceROIInformation      faceROIInfo;
    CamxResult              result = CamxResultSuccess;

    PrintFDResults("FDEngineResults-Unstabilized", pPerFrameSettings->requestId, m_FDFrameWidth, m_FDFrameHeight,
                   pUnstabilizedResults->numFacesDetected, &(pUnstabilizedResults->faceInfo[0]));

    // Stabilize the data
    StabilizeFaces(pPerFrameSettings, pUnstabilizedResults, &stabilizedResults);
    PrintFDResults("StabilizedResults", pPerFrameSettings->requestId, m_FDFrameWidth, m_FDFrameHeight,
                   stabilizedResults.numFacesDetected, &stabilizedResults.faceInfo[0]);

    if (FALSE == GetStaticSettings()->enableOfflineFD)
    {
        // Check out of bounds face ROI values
        CheckROIBound(pUnstabilizedResults, m_FDFrameWidth, m_FDFrameHeight);   // Unstabilized data
        CheckROIBound(&stabilizedResults, m_FDFrameWidth, m_FDFrameHeight);     // Stabilized data
        PrintFDResults("ROIBoundResult-Stabilized", pPerFrameSettings->requestId, m_FDFrameWidth, m_FDFrameHeight,
                       stabilizedResults.numFacesDetected, &stabilizedResults.faceInfo[0]);

        // Sort faces from largest to smallest
        FDUtils::SortFaces(pUnstabilizedResults);   // Unstabilized data
        FDUtils::SortFaces(&stabilizedResults);     // Stabilized data
        PrintFDResults("AfterSorting-Stabilized", pPerFrameSettings->requestId, m_FDFrameWidth, m_FDFrameHeight,
                       stabilizedResults.numFacesDetected, &stabilizedResults.faceInfo[0]);
    }

    ConvertFromProcessingResultsToReferenceFaceROIInfo(pUnstabilizedResults, &stabilizedResults, &faceROIInfo,
        pPerFrameSettings);

    PublishFDResultstoUsecasePool(&faceROIInfo);

    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::StabilizeFaces
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::StabilizeFaces(
    const FDPerFrameSettings* pPerFrameSettings,
    FDResults*                pUnstabilized,
    FDResults*                pStabilized)
{
    CAMX_ASSERT(NULL != pPerFrameSettings);
    CAMX_ASSERT(NULL != pUnstabilized);
    CAMX_ASSERT(NULL != pStabilized);

    CamxResult result = CamxResultSuccess;

    if ((NULL != m_pStabilizer) && (TRUE == pPerFrameSettings->fdConfig.stabilization.enable))
    {
        if (TRUE == pPerFrameSettings->FDConfigUpdated)
        {
            StabilizationConfig stabilizationConfig = {};

            GetStabilizationConfig(&pPerFrameSettings->fdConfig, &stabilizationConfig);

            m_pStabilizer->SetConfig(&stabilizationConfig);
        }

        StabilizationData currentData    = { 0 };
        StabilizationData stabilizedData = { 0 };

        result = FDUtils::ConvertToStabilizationData(pUnstabilized, &currentData);

        if (CamxResultSuccess == result)
        {
            result = m_pStabilizer->ExecuteStabilization(&currentData, &stabilizedData);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in ExecuteStabilization, result=%d", result);
            }
        }

        if (CamxResultSuccess == result)
        {
            result = FDUtils::ConvertFromStabilizationData(&stabilizedData, pStabilized);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in ConvertFromStabilizationData, result=%d", result);
            }
        }
    }

    if ((FALSE             == pPerFrameSettings->fdConfig.stabilization.enable) ||
        (NULL              == m_pStabilizer)                                    ||
        (CamxResultSuccess != result))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Not applying stabilization, result=%d", result);
        *pStabilized = *pUnstabilized;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::FDSWOnlyExecution()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::FDSWOnlyExecution(
    FDPerFrameSettings*     pPerFrameSettings,
    FDResults*              pSWResults)
{
    CamxResult              result    = CamxResultSuccess;
    FDEnginePerFrameInfo    frameInfo = { 0 };

    if (NULL == pPerFrameSettings->pImageAddress)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Input frame address is NULL");
        result = CamxResultEInvalidPointer;
    }

    if ((CamxResultSuccess     == result)              &&
        (InvalidFDEngineHandle != m_hFDEngineSWHandle) &&
        (TRUE                  == pPerFrameSettings->fdConfig.swConfig.enable))
    {
        if (TRUE == pPerFrameSettings->FDConfigUpdated)
        {
            FDEngineConfig engineConfig;
            // Finzalize dynamically changing tuning values and call FDEngineSetConfig if they change
            if (CamxResultSuccess == result)
            {
                result = FDEngineGetConfig(m_hFDEngineSWHandle, &engineConfig);
                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in getting FDEgnine config, result=%d", result);
                }
            }
            if (CamxResultSuccess == result)
            {
                result = SetupFDEngineConfig(&pPerFrameSettings->fdConfig, &engineConfig);
            }
            if (CamxResultSuccess == result)
            {
                result = FDEngineSetConfig(m_hFDEngineSWHandle, &engineConfig);
                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in setting FDEgnine config, result=%d", result);
                }
            }
        }
        frameInfo.frameID        = pPerFrameSettings->requestId;
        frameInfo.deviceRotation = pPerFrameSettings->deviceOrientation;
        frameInfo.bufferCached   = FALSE;

        frameInfo.imageInfo.planeCount         = 1;
        frameInfo.imageInfo.width              = m_FDFrameWidth;
        frameInfo.imageInfo.height             = m_FDFrameHeight;
        frameInfo.imageInfo.planes[0].type     = Y;
        frameInfo.imageInfo.planes[0].pBuffer  = pPerFrameSettings->pImageAddress;
        frameInfo.imageInfo.planes[0].stride   = m_FDFrameStride;
        frameInfo.imageInfo.planes[0].scanline = m_FDFrameScanline;

        CAMX_LOG_VERBOSE(CamxLogGroupFD, " Buffer %p,. stride %d, scanline %d",
                         frameInfo.imageInfo.planes[0].pBuffer, frameInfo.imageInfo.planes[0].stride,
                         frameInfo.imageInfo.planes[0].scanline);
        result = FDEngineRunDetect(m_hFDEngineSWHandle, &frameInfo, pSWResults);
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Faces %d", pSWResults->numFacesDetected);
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in FD engine processing result = %d", result);
        pSWResults->numFacesDetected = 0;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::ProcessFDEngineResults()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::ProcessFDEngineResults(
    FDPerFrameSettings*     pPerFrameSettings,
    FDIntermediateResults*  pHWResults,
    FDResults*              pEngineResults)
{
    CamxResult              result      = CamxResultSuccess;
    FDEnginePerFrameInfo    frameInfo   = { 0 };

    if (NULL == pPerFrameSettings->pImageAddress)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Input frame address is NULL");

        result = CamxResultEInvalidPointer;
    }

    if ((CamxResultSuccess     == result)                    &&
        (InvalidFDEngineHandle != m_hFDEngineFPFilterHandle) &&
        (TRUE                  == pPerFrameSettings->fdConfig.managerConfig.enable))
    {
        if (TRUE == pPerFrameSettings->FDConfigUpdated)
        {
            FDEngineConfig engineConfig;

            result = FDEngineGetConfig(m_hFDEngineFPFilterHandle, &engineConfig);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in getting FDEgnine config, result=%d", result);
            }

            if (CamxResultSuccess == result)
            {
                result = SetupFDEngineConfig(&pPerFrameSettings->fdConfig, &engineConfig);
            }

            if (CamxResultSuccess == result)
            {
                result = FDEngineSetConfig(m_hFDEngineFPFilterHandle, &engineConfig);
                if (CamxResultSuccess != result)
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in setting FDEgnine config, result=%d", result);
                }
            }
        }

        frameInfo.frameID           = pPerFrameSettings->requestId;
        frameInfo.deviceRotation    = pPerFrameSettings->deviceOrientation;
        frameInfo.bufferCached      = FALSE;

        frameInfo.imageInfo.planeCount  = 1;
        frameInfo.imageInfo.width       = m_FDFrameWidth;
        frameInfo.imageInfo.height      = m_FDFrameHeight;

        frameInfo.imageInfo.planes[0].type      = Y;
        frameInfo.imageInfo.planes[0].pBuffer   = pPerFrameSettings->pImageAddress;
        frameInfo.imageInfo.planes[0].stride    = m_FDFrameStride;
        frameInfo.imageInfo.planes[0].scanline  = m_FDFrameScanline;

        if ((FD_DL_ARM == GetStaticSettings()->FDFilterEngine) || (FD_DL_DSP == GetStaticSettings()->FDFilterEngine))
        {
            frameInfo.imageInfo.planeCount         = 2;
            frameInfo.imageInfo.planes[1].type     = CB_CR;
            frameInfo.imageInfo.planes[1].pBuffer  = pPerFrameSettings->pUVImageAddress;
            frameInfo.imageInfo.planes[1].stride   = m_FDFrameStride;
            frameInfo.imageInfo.planes[1].scanline = m_FDFrameScanline / 2;
        }

        result = FDEngineProcessResult(m_hFDEngineFPFilterHandle, &frameInfo, pHWResults, NULL, pEngineResults);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in FD engine processing results, result=%d");
        }

        pPerFrameSettings->inputBufferInvalidateRequired = frameInfo.bufferCached;
    }

    if ((CamxResultSuccess     != result)                    ||
        (InvalidFDEngineHandle == m_hFDEngineFPFilterHandle) ||
        (FALSE                 == pPerFrameSettings->fdConfig.managerConfig.enable))
    {
        pEngineResults->numFacesDetected = pHWResults->numFacesDetected;

        if (FDMaxFaceCount < pEngineResults->numFacesDetected)
        {
            pEngineResults->numFacesDetected = FDMaxFaceCount;
        }

        for (UINT8 i = 0; i < pEngineResults->numFacesDetected; i++)
        {
            pEngineResults->faceInfo[i] = pHWResults->faceInfo[i];
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::GetStabilizationConfig()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::GetStabilizationConfig(
    const FDConfig*      pFDConfig,
    StabilizationConfig* pStabilizationConfig)
{
    pStabilizationConfig->historyDepth = pFDConfig->stabilization.historyDepth;

    // FD Tuning config for FD ROI center position
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].enable                            =
        pFDConfig->stabilization.position.enable;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].mode                              =
        static_cast<StabilizationMode>(pFDConfig->stabilization.position.mode);
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].threshold                         =
        pFDConfig->stabilization.position.threshold;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].stateCount                        =
        pFDConfig->stabilization.position.stateCount;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].stableThreshold                   =
        pFDConfig->stabilization.position.stableThreshold;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].minStableState                    =
        pFDConfig->stabilization.position.minStableState;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].filterType                        =
        static_cast<StabilizationFilter>(pFDConfig->stabilization.position.filterType);
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].averageFilter.historyLength       =
        pFDConfig->stabilization.position.averageFilter.historyLength;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].useReference                      =
        pFDConfig->stabilization.position.useReference;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].movingThreshold                   =
        pFDConfig->stabilization.position.movingThreshold;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].movingInitStateCount              =
        pFDConfig->stabilization.position.movingInitStateCount;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].movingLinkFactor                  =
        pFDConfig->stabilization.position.movingLinkFactor;
    pStabilizationConfig->attributeConfigs[ObjectPositionIndex].averageFilter.movingHistoryLength =
        pFDConfig->stabilization.position.averageFilter.movingHistoryLength;

    // FD Tuning config for FD ROI size
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].enable                            =
        pFDConfig->stabilization.size.enable;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].mode                              =
        static_cast<StabilizationMode>(pFDConfig->stabilization.size.mode);
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].threshold                         =
        pFDConfig->stabilization.size.threshold;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].stateCount                        =
        pFDConfig->stabilization.size.stateCount;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].stableThreshold                   =
        pFDConfig->stabilization.size.stableThreshold;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].minStableState                    =
        pFDConfig->stabilization.size.minStableState;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].filterType                        =
        static_cast<StabilizationFilter>(pFDConfig->stabilization.size.filterType);
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].averageFilter.historyLength       =
        pFDConfig->stabilization.size.averageFilter.historyLength;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].useReference                      =
        pFDConfig->stabilization.size.useReference;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].movingThreshold                   =
        pFDConfig->stabilization.size.movingThreshold;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].movingInitStateCount              =
        pFDConfig->stabilization.size.movingInitStateCount;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].movingLinkFactor                  =
        pFDConfig->stabilization.size.movingLinkFactor;
    pStabilizationConfig->attributeConfigs[ObjectSizeIndex].averageFilter.movingHistoryLength =
        pFDConfig->stabilization.size.averageFilter.movingHistoryLength;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Position(%d %d %d %d %d %d %d %d %d), Size(%d %d %d %d %d %d %d %d %d)",
                     pStabilizationConfig->attributeConfigs[ObjectPositionIndex].enable,
                     pStabilizationConfig->attributeConfigs[ObjectPositionIndex].mode,
                     pStabilizationConfig->attributeConfigs[ObjectPositionIndex].threshold,
                     pStabilizationConfig->attributeConfigs[ObjectPositionIndex].stateCount,
                     pStabilizationConfig->attributeConfigs[ObjectPositionIndex].stableThreshold,
                     pStabilizationConfig->attributeConfigs[ObjectPositionIndex].minStableState,
                     pStabilizationConfig->attributeConfigs[ObjectPositionIndex].filterType,
                     pStabilizationConfig->attributeConfigs[ObjectPositionIndex].averageFilter.historyLength,
                     pStabilizationConfig->attributeConfigs[ObjectPositionIndex].useReference,
                     pStabilizationConfig->attributeConfigs[ObjectSizeIndex].enable,
                     pStabilizationConfig->attributeConfigs[ObjectSizeIndex].mode,
                     pStabilizationConfig->attributeConfigs[ObjectSizeIndex].threshold,
                     pStabilizationConfig->attributeConfigs[ObjectSizeIndex].stateCount,
                     pStabilizationConfig->attributeConfigs[ObjectSizeIndex].stableThreshold,
                     pStabilizationConfig->attributeConfigs[ObjectSizeIndex].minStableState,
                     pStabilizationConfig->attributeConfigs[ObjectSizeIndex].filterType,
                     pStabilizationConfig->attributeConfigs[ObjectSizeIndex].averageFilter.historyLength,
                     pStabilizationConfig->attributeConfigs[ObjectSizeIndex].useReference);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::InitializeStabilization()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::InitializeStabilization()
{
    CamxResult result = CamxResultSuccess;

    if (TRUE == m_FDConfig.stabilization.enable)
    {
        m_pStabilizer = Stabilization::Create();

        if (NULL == m_pStabilizer)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Unable to create FD Stabilizer");
            result = CamxResultENoMemory;
        }

        if (CamxResultSuccess == result)
        {
            StabilizationConfig stabilizationConfig;

            GetStabilizationConfig(&m_FDConfig, &stabilizationConfig);

            result = m_pStabilizer->Initialize(&stabilizationConfig, FDFrameWidth, FDFrameHeight);
        }
    }

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::OnSetUpFDEngine()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::OnSetUpFDEngine()
{
    CamxResult result = CamxResultSuccess;

    if (FALSE == m_bEnableSwDetection)
    {
        FDEngineCreateParams createParams = { 0 };

        createParams.createFacialParts    = FALSE;
        createParams.createROIManager     = TRUE;
        createParams.createSWDetection    = FALSE;
        createParams.numFacialPartsHanle  = 1;
        createParams.maxNumInputFace      = FDMaxFaceCountIntermediate;
        createParams.maxNumOutputFace     = FDMaxFaceCount;
        createParams.detectionMode        = FullSearch;
        createParams.engineType           = GetFdFilterEngine();
        createParams.clientType           = ClientHW;

        if (((DL_ARM == createParams.engineType) || (DL_DSP == createParams.engineType)) &&
            (TRUE == GetStaticSettings()->enableFDManagerThreading))
        {
            FDManagerThreadData* pThreadData = GetLocalThreadDataMemory();
            if (NULL != pThreadData)
            {
                pThreadData->pInstance                               = this;
                pThreadData->jobType                                 = FDManagerThreadJobFDEngineSetup;
                pThreadData->jobInfo.fdEngineCreateInfo.createParams = createParams;

                result = m_pThreadManager->PostJob(m_hThread,
                                                   NULL,
                                                   reinterpret_cast<VOID**>(&pThreadData),
                                                   FALSE,
                                                   FALSE);

                CAMX_LOG_VERBOSE(CamxLogGroupFD, "FD Engine Setup job (HW detection) has been scheduled.");
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "FD manager thread data allocation error");

                // bail out if runs out of thread data memory
                result = SetUpFDEngine(&createParams, &m_hFDEngineFPFilterHandle);
            }
        }
        else
        {
            result = SetUpFDEngine(&createParams, &m_hFDEngineFPFilterHandle);
        }
    }

    if ((CamxResultSuccess == result) && (TRUE == m_bEnableSwDetection))
    {
        FDEngineCreateParams createParams = { 0 };

        createParams.createFacialParts    = FALSE;
        createParams.createROIManager     = FALSE;
        createParams.createSWDetection    = TRUE;
        createParams.numFacialPartsHanle  = 0;
        createParams.maxNumOutputFace     = m_FDConfig.maxNumberOfFaces;
        createParams.detectionMode        = ContinuousSearch;
        createParams.engineType           = Standard_ARM;
        createParams.clientType           = ClientSW;

        result = SetUpFDEngine(&createParams, &m_hFDEngineSWHandle);
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Failed to set up FD Engine, result = %d", result);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::SetUpFDEngine()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::SetUpFDEngine(
    FDEngineCreateParams*   pFDEngineCreateParams,
    FDEngineHandle*         phFDEngineHandle)
{
    FDEngineConfig      engineConfig;
    CamxResult          result = CamxResultSuccess;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "FD filter engine type %d", pFDEngineCreateParams->engineType);
    result = FDEngineCreate(pFDEngineCreateParams, phFDEngineHandle);
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in creating FDEgnine handle, error: %s", Utils::CamxResultToString(result));
    }

    if (CamxResultSuccess == result)
    {
        result = FDEngineGetConfig(*phFDEngineHandle, &engineConfig);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in getting FDEgnine config, error: %s", Utils::CamxResultToString(result));
        }
    }

    if (CamxResultSuccess == result)
    {
        result = SetupFDEngineConfig(&m_FDConfig, &engineConfig);
    }

    if (CamxResultSuccess == result)
    {
        result = FDEngineSetConfig(*phFDEngineHandle, &engineConfig);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Failed in setting FDEgnine config, result=%d", result);
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::SetupFDEngineConfig()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::SetupFDEngineConfig(
    const FDConfig* pFDConfig,
    FDEngineConfig* pEngineConfig)
{
    CAMX_ASSERT(NULL != pEngineConfig);

    // Set feature level configuration params
    pEngineConfig->enableFacialParts    = FALSE;
    pEngineConfig->enableContour        = FALSE;
    pEngineConfig->enableSmile          = FALSE;
    pEngineConfig->enableGaze           = FALSE;
    pEngineConfig->enableBlink          = FALSE;

    // Set overal solution level configuration params
    pEngineConfig->lockFacesEnable = pFDConfig->lockDetectedFaces;

    // Set False positive filter params
    pEngineConfig->enableSWFPFilter                                     = pFDConfig->FPFilter.enable;
    pEngineConfig->paramsFPFilter.faceSearchDensity                     = pFDConfig->FPFilter.searchDensity;
    pEngineConfig->paramsFPFilter.faceSizePerc                          = pFDConfig->FPFilter.expandFaceSizePercentage;
    pEngineConfig->paramsFPFilter.faceBoxBorderPerc                     = pFDConfig->FPFilter.expandBoxBorderPercentage;
    pEngineConfig->paramsFPFilter.faceSpreadTolerance                   = pFDConfig->FPFilter.faceSpreadTolerance;
    pEngineConfig->paramsFPFilter.thresholdInfo.targetThreshold         = pFDConfig->FPFilter.baseThreshold;
    pEngineConfig->paramsFPFilter.thresholdInfo.innerTargetThreshold    = pFDConfig->FPFilter.innerThreshold;

    CAMX_LOG_VERBOSE(CamxLogGroupFD,
                     "FDCONFIG-FPFilter : enableSWFPFilter=%d, faceSearchDensity=%d, faceSizePerc=%d, "
                     "faceBoxBorderPerc=%d, faceSpreadTolerance=%f, targetThreshold=%d, innerTargetThreshold=%d",
                     pEngineConfig->enableSWFPFilter,
                     pEngineConfig->paramsFPFilter.faceSearchDensity,
                     pEngineConfig->paramsFPFilter.faceSizePerc,
                     pEngineConfig->paramsFPFilter.faceBoxBorderPerc,
                     pEngineConfig->paramsFPFilter.faceSpreadTolerance,
                     pEngineConfig->paramsFPFilter.thresholdInfo.targetThreshold,
                     pEngineConfig->paramsFPFilter.thresholdInfo.innerTargetThreshold);

    // Set ROI generator params
    pEngineConfig->paramsROIGen.faceSearchDensity                   = pFDConfig->ROIGenerator.searchDensity;
    pEngineConfig->paramsROIGen.faceSizePerc                        = pFDConfig->ROIGenerator.expandFaceSizePercentage;
    pEngineConfig->paramsROIGen.faceBoxBorderPerc                   = pFDConfig->ROIGenerator.expandBoxBorderPercentage;
    pEngineConfig->paramsROIGen.faceSpreadTolerance                 = pFDConfig->ROIGenerator.faceSpreadTolerance;
    pEngineConfig->paramsROIGen.thresholdInfo.targetThreshold       = pFDConfig->ROIGenerator.baseThreshold;
    pEngineConfig->paramsROIGen.thresholdInfo.innerTargetThreshold  = pFDConfig->ROIGenerator.innerThreshold;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "FDCONFIG-ROIGenerator : faceSearchDensity=%d, faceSizePerc=%d, "
                     "faceBoxBorderPerc=%d, faceSpreadTolerance=%f, targetThreshold=%d, innerTargetThreshold=%d",
                     pEngineConfig->paramsROIGen.faceSearchDensity,
                     pEngineConfig->paramsROIGen.faceSizePerc,
                     pEngineConfig->paramsROIGen.faceBoxBorderPerc,
                     pEngineConfig->paramsROIGen.faceSpreadTolerance,
                     pEngineConfig->paramsROIGen.thresholdInfo.targetThreshold,
                     pEngineConfig->paramsROIGen.thresholdInfo.innerTargetThreshold);

    // Set face manager params
    pEngineConfig->ROIMgrConfig.holdCount                   = pFDConfig->holdCount;
    pEngineConfig->ROIMgrConfig.delayCount                  = pFDConfig->delayCount;
    pEngineConfig->ROIMgrConfig.swGoodFaceThreshold         = pFDConfig->managerConfig.newGoodFaceConfidence;
    pEngineConfig->ROIMgrConfig.swThreshold                 = pFDConfig->managerConfig.newNormalFaceConfidence;
    pEngineConfig->ROIMgrConfig.swTrackingThreshold         = pFDConfig->managerConfig.existingFaceConfidence;
    pEngineConfig->ROIMgrConfig.tolMoveDist                 = pFDConfig->managerConfig.faceLinkMoveDistanceRatio;
    pEngineConfig->ROIMgrConfig.tolSizeRatioMin             = pFDConfig->managerConfig.faceLinkMinSizeRatio;
    pEngineConfig->ROIMgrConfig.tolSizeRatioMax             = pFDConfig->managerConfig.faceLinkMaxSizeRatio;
    pEngineConfig->ROIMgrConfig.tolAngle                    = pFDConfig->managerConfig.faceLinkRollAngleDifference;
    pEngineConfig->ROIMgrConfig.strictSwGoodFaceThreshold   = pFDConfig->managerConfig.strictNewGoodFaceConfidence;
    pEngineConfig->ROIMgrConfig.strictSwThreshold           = pFDConfig->managerConfig.strictNewNormalFaceConfidence;
    pEngineConfig->ROIMgrConfig.strictSwTrackingThreshold   = pFDConfig->managerConfig.strictExistingFaceConfidence;
    pEngineConfig->ROIMgrConfig.angleDiffForStrictThreshold = pFDConfig->managerConfig.angleDiffForStrictConfidence;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "FDCONFIG-ROIManager : holdCount=%d, delayCount=%d, "
                     "Thresholds=(%d, %d, %d), StrictThreshold=(%d, %d, %d), angleDiffForStrictThreshold=%d, "
                     "tolMoveDist=%f, tolSizeRatioMin=%f, tolSizeRatioMax=%f, tolAngle=%f",
                     pEngineConfig->ROIMgrConfig.holdCount,
                     pEngineConfig->ROIMgrConfig.delayCount,
                     pEngineConfig->ROIMgrConfig.swGoodFaceThreshold,
                     pEngineConfig->ROIMgrConfig.swThreshold,
                     pEngineConfig->ROIMgrConfig.swTrackingThreshold,
                     pEngineConfig->ROIMgrConfig.strictSwGoodFaceThreshold,
                     pEngineConfig->ROIMgrConfig.strictSwThreshold,
                     pEngineConfig->ROIMgrConfig.strictSwTrackingThreshold,
                     pEngineConfig->ROIMgrConfig.angleDiffForStrictThreshold,
                     pEngineConfig->ROIMgrConfig.tolMoveDist,
                     pEngineConfig->ROIMgrConfig.tolSizeRatioMin,
                     pEngineConfig->ROIMgrConfig.tolSizeRatioMax,
                     pEngineConfig->ROIMgrConfig.tolAngle);

    if (TRUE == m_bEnableSwDetection)
    {
        // Set SW detection params
        pEngineConfig->paramsDT.searchDensity = pFDConfig->swConfig.newFaceSearchDensity;
        pEngineConfig->paramsDT.minFaceSize   = FDUtils::GetFaceSize(&(pFDConfig->swConfig.minFaceSize),
                                        FDHwUtils::FDMinFacePixels2,
                                        (m_baseFDHWDimension.width < m_baseFDHWDimension.height ?
                                        m_baseFDHWDimension.width : m_baseFDHWDimension.height));
        pEngineConfig->paramsDT.maxFaceSize   = (m_baseFDHWDimension.width < m_baseFDHWDimension.height ?
                                        m_baseFDHWDimension.width : m_baseFDHWDimension.height);
        pEngineConfig->paramsDT.threshold     = pFDConfig->swConfig.frontProfileConfig.priorityThreshold;

        // Front Profile Configuration
        pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFront].angle              =
                                                    pFDConfig->swConfig.frontProfileConfig.priorityAngleRange;
        pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFront].nonTargetThreshold =
                                                    pFDConfig->swConfig.frontProfileConfig.nonPriorityThreshold;
        pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFront].targetThreshold    =
                                                    pFDConfig->swConfig.frontProfileConfig.priorityThreshold;
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "FDCONFIG-SW DT : SearchDensity = %d, "
                         "minfacesize = %d, maxfacesize = %d,  Front profile(nonTargetThreshold = %d"
                         " targetThreshold = %d, angle =%d),",
                         pEngineConfig->paramsDT.searchDensity,
                         pEngineConfig->paramsDT.minFaceSize,
                         pEngineConfig->paramsDT.maxFaceSize,
                         pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFront].nonTargetThreshold,
                         pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFront].targetThreshold,
                         pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFront].angle);

        // HALF Profile Configuration
        pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleHalfProfile].angle              =
                                                    pFDConfig->swConfig.halfProfileConfig.priorityAngleRange;
        pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleHalfProfile].nonTargetThreshold =
                                                    pFDConfig->swConfig.halfProfileConfig.nonPriorityThreshold;
        pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleHalfProfile].targetThreshold    =
                                                    pFDConfig->swConfig.halfProfileConfig.priorityThreshold;

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "FDCONFIG-SW DT : HALF profile(nonTargetThreshold = %d"
                         "targetThreshold = %d, angle =%d),",
                         pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleHalfProfile].nonTargetThreshold,
                         pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleHalfProfile].targetThreshold,
                         pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleHalfProfile].angle);

        // FULL Profile Configuration
        pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFullProfile].angle              =
                                                    pFDConfig->swConfig.fullProfileConfig.priorityAngleRange;
        pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFullProfile].nonTargetThreshold =
                                                    pFDConfig->swConfig.fullProfileConfig.nonPriorityThreshold;
        pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFullProfile].targetThreshold    =
                                                    pFDConfig->swConfig.fullProfileConfig.priorityThreshold;

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "FDCONFIG-SW DT : FULL profile(nonTargetThreshold = %d"
                         "targetThreshold = %d, angle =%d),",
                         pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFullProfile].nonTargetThreshold,
                         pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFullProfile].targetThreshold,
                         pEngineConfig->paramsDT.thresholdInfo[FDEngineAngleFullProfile].angle);

        // Set Angle
        pEngineConfig->paramsDT.faceAngle[FDEngineAngleFront]       =
                                    pFDConfig->swConfig.frontProfileConfig.searchAngle;
        pEngineConfig->paramsDT.faceAngle[FDEngineAngleHalfProfile] =
                                    pFDConfig->swConfig.halfProfileConfig.searchAngle;
        pEngineConfig->paramsDT.faceAngle[FDEngineAngleFullProfile] =
                                    pFDConfig->swConfig.fullProfileConfig.searchAngle;

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "FDCONFIG-SW DT : Search Angle FrontProfileAngle=%d,"
                         "HalfprofileAngle = %d, FullprofileAngle = %d",
                         pEngineConfig->paramsDT.faceAngle[FDEngineAngleFront],
                         pEngineConfig->paramsDT.faceAngle[FDEngineAngleHalfProfile],
                         pEngineConfig->paramsDT.faceAngle[FDEngineAngleFullProfile]);

        // Set Upfront Angle
        pEngineConfig->paramsDT.upFrontFaceAngle[FDEngineAngleFront]       =
            pFDConfig->swConfig.frontProfileConfig.upFrontSearchAnggle;
        pEngineConfig->paramsDT.upFrontFaceAngle[FDEngineAngleHalfProfile] =
            pFDConfig->swConfig.halfProfileConfig.upFrontSearchAnggle;
        pEngineConfig->paramsDT.upFrontFaceAngle[FDEngineAngleFullProfile] =
            pFDConfig->swConfig.fullProfileConfig.upFrontSearchAnggle;

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "FDCONFIG-SW DT : Upfront Search Angle FrontProfileAngle=%d,"
                         " HalfprofileAngle = %d, FullprofileAngle = %d",
                         pEngineConfig->paramsDT.upFrontFaceAngle[FDEngineAngleFront],
                         pEngineConfig->paramsDT.upFrontFaceAngle[FDEngineAngleHalfProfile],
                         pEngineConfig->paramsDT.upFrontFaceAngle[FDEngineAngleFullProfile]);

        // params only for movie mode
        pEngineConfig->paramsDT.accuracy                     = pFDConfig->accuracy;
        pEngineConfig->paramsDT.delayCount                   = pFDConfig->delayCount;
        pEngineConfig->paramsDT.maxHoldCount                 = pFDConfig->holdCount;
        pEngineConfig->paramsDT.maxRetryCount                = pFDConfig->retryCount;
        pEngineConfig->paramsDT.initialFaceSearchCycle       = pFDConfig->swConfig.noFaceSearchCycle;
        pEngineConfig->paramsDT.newFaceSearchCycle           = pFDConfig->swConfig.newFaceSearchCycle;
        pEngineConfig->paramsDT.newFaceSearchInterval        = pFDConfig->swConfig.newFaceSearchInterval;
        pEngineConfig->paramsDT.upFrontNewFaceSearchCycle    = pFDConfig->swConfig.upFrontNewFaceSearchCycle;
        pEngineConfig->paramsDT.upFrontNoFaceSearchCycle     = pFDConfig->swConfig.upFrontNoFaceSearchCycle;
        pEngineConfig->paramsDT.upFrontNewFaceSearchInterval = pFDConfig->swConfig.upFrontNewFaceSearchInterval;
        pEngineConfig->paramsDT.posSteadinessParam           = pFDConfig->swConfig.positionSteadiness;
        pEngineConfig->paramsDT.sizeSteadinessParam          = pFDConfig->swConfig.sizeSteadiness;
        pEngineConfig->paramsDT.useHeadTracking              = FALSE;
        pEngineConfig->paramsDT.yawAngleExtension            = pFDConfig->swConfig.yawAngleExtension;
        pEngineConfig->paramsDT.rollAngleExtension           = pFDConfig->swConfig.rollAngleExtension;
        pEngineConfig->paramsDT.directionMask                = FALSE;
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::PrintFDResults()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::PrintFDResults(
    const CHAR* pLogTag,
    UINT64      frameId,
    UINT32      width,
    UINT32      height,
    UINT8       numFaces,
    FDFaceInfo* pFaceInfo)
{
    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] [%s] : Dimensions[%dx%d], numFaces=%d",
                     InstanceID(), frameId, pLogTag, width, height, numFaces);

    for (INT i = 0; i < numFaces; i++)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] [%s] : Face[%d] : faceID=%d, newFace=%d, confidence=%d, "
                         "center(%d, %d), width=%d, height=%d, rollAngle=%d, pose=%d",
                         InstanceID(),                      frameId,
                         pLogTag,                           i,                                  pFaceInfo[i].faceID,
                         pFaceInfo[i].newFace,              pFaceInfo[i].faceROI.confidence,    pFaceInfo[i].faceROI.center.x,
                         pFaceInfo[i].faceROI.center.y,     pFaceInfo[i].faceROI.width,         pFaceInfo[i].faceROI.height,
                         pFaceInfo[i].faceROI.rollAngle,    pFaceInfo[i].faceROI.pose);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::PrintFDFaceROIInfo()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDManagerNode::PrintFDFaceROIInfo(
    const CHAR*         pLogTag,
    UINT64              frameId,
    UINT32              width,
    UINT32              height,
    FaceROIInformation* pFaceROIInfo)
{
    CAMX_ASSERT(NULL != pFaceROIInfo);

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] [%s] : ActualReqId[%lld] Dimensions[%dx%d], numFaces=%d",
                     InstanceID(), frameId, pLogTag, pFaceROIInfo->requestId, width, height, pFaceROIInfo->ROICount);

    for (UINT32 i = 0; i < pFaceROIInfo->ROICount; i++)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD,
                         "Instance[%d] ReqId[%llu] [%s-Unstabilized] : Face[%d] : faceID=%d, confidence=%d, "
                         "top-left(%d, %d), width=%d, height=%d",
                         InstanceID(),                                    frameId,
                         pLogTag,                                         i,
                         pFaceROIInfo->unstabilizedROI[i].id,             pFaceROIInfo->unstabilizedROI[i].confidence,
                         pFaceROIInfo->unstabilizedROI[i].faceRect.left,  pFaceROIInfo->unstabilizedROI[i].faceRect.top,
                         pFaceROIInfo->unstabilizedROI[i].faceRect.width, pFaceROIInfo->unstabilizedROI[i].faceRect.height);
    }

    for (UINT32 i = 0; i < pFaceROIInfo->ROICount; i++)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD,
                         "Instance[%d] ReqId[%llu] [%s-Stabilized] : Face[%d] : faceID=%d, confidence=%d, "
                         "top-left(%d, %d), width=%d, height=%d",
                         InstanceID(),                                  frameId,
                         pLogTag,                                       i,
                         pFaceROIInfo->stabilizedROI[i].id,             pFaceROIInfo->stabilizedROI[i].confidence,
                         pFaceROIInfo->stabilizedROI[i].faceRect.left,  pFaceROIInfo->stabilizedROI[i].faceRect.top,
                         pFaceROIInfo->stabilizedROI[i].faceRect.width, pFaceROIInfo->stabilizedROI[i].faceRect.height);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::CheckFDConfigChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL FDManagerNode::CheckFDConfigChange(
    FDConfig* pFDConfig)
{
    CamxResult          result          = CamxResultSuccess;
    BOOL                isConfigUpdated = FALSE;
    FDConfigSelector    configSelector  = (TRUE == m_isVideoMode) ? FDSelectorVideo : FDSelectorDefault;

    if (FDConfigVendorTag == GetStaticSettings()->FDConfigSource)
    {
        // For vendor tag config loading, frontCamera, configSelector really doesn't matter
        result = FDUtils::GetFDConfig(this, FDConfigVendorTag, TRUE, m_isFrontCamera, configSelector, pFDConfig);
        if (CamxResultSuccess == result)
        {
            isConfigUpdated = TRUE;
        }
    }
    else
    {
        /// @todo (CAMX-2793) Check if there is anything that triggers FD config change. Exa - turbo mode.
        /// This will change the configSelector and need to read corresponding config header/binary
        BOOL reloadConfig = FALSE;

        if (TRUE == reloadConfig)
        {
            result = FDUtils::GetFDConfig(this, GetStaticSettings()->FDConfigSource, TRUE, m_isFrontCamera, configSelector,
                                          pFDConfig);
            if (CamxResultSuccess == result)
            {
                isConfigUpdated = TRUE;
            }
        }
    }

    return isConfigUpdated;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::GetDeviceOrientation()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::GetDeviceOrientation(
    const NodeProcessRequestData* const pNodeRequestData,
    FDPerFrameSettings*                 pPerFrameSettings)
{
    CamxResult  result              = CamxResultSuccess;
    UINT64      requestId           = pNodeRequestData->pCaptureRequest->requestId;
    INT32       deviceOrientation   = -1;

    if (TRUE == GetStaticSettings()->useDeviceOrientationInFD)
    {
        FLOAT accelValues[3] = { 0 };

        if (TRUE == GetStaticSettings()->getAccelInfoUsingVendorTag)
        {
            UINT32 metaTagAccel        = 0;
            UINT   configTag[1]        = { 0 };
            VOID*  pData[1]            = { 0 };
            UINT   length              = CAMX_ARRAY_SIZE(configTag);
            UINT64 configDataOffset[1] = { 0 };

            result = VendorTagManager::QueryVendorTagLocation(GetStaticSettings()->accelVendorTagSection,
                                                              GetStaticSettings()->accelVendorTagName,
                                                              &metaTagAccel);

            if (CamxResultSuccess == result)
            {
                configTag[0] = metaTagAccel;

                result = GetDataList(configTag, pData, configDataOffset, length);

                if (NULL != pData[0])
                {
                    Utils::Memcpy(accelValues, pData[0], (sizeof(FLOAT) * 3));
                }
                else
                {
                    result = CamxResultEFailed;
                    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Error getting accelerometer information");
                }
            }
            else
            {
                CAMX_LOG_VERBOSE(CamxLogGroupFD, "Error getting accelerometer vendor tag location %d", result);
            }
        }
        else
        {
            result = PopulateGravityData(accelValues);
        }

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "ReqId[%llu] : Accelerometer Data [%f] [%f] [%f]",
                         requestId, accelValues[0], accelValues[1], accelValues[2]);

        if (CamxResultSuccess == result)
        {
            if (NULL == m_pCameraConfigInfo)
            {
                result = GetCameraConfiguration(&m_pCameraConfigInfo);
            }

            if (CamxResultSuccess == result)
            {
                result = FDUtils::AlignAccelToCamera(accelValues, m_pCameraConfigInfo->imageOrientation,
                    m_pCameraConfigInfo->position + 1);
            }

            if (CamxResultSuccess == result)
            {
                result = FDUtils::GetOrientationFromAccel(accelValues, &deviceOrientation);
            }

            if (CamxResultSuccess == result)
            {
                /// @todo (CAMX-2779) Get the actual flip information
                FDUtils::ApplyFlipOnDeviceOrienation(FlipNone, &deviceOrientation);
            }
        }
    }

    if (CamxResultSuccess == result)
    {
        pPerFrameSettings->deviceOrientation = deviceOrientation;
    }
    else
    {
        pPerFrameSettings->deviceOrientation = -1;
    }

    if (-1 != pPerFrameSettings->deviceOrientation)
    {
        // Faceproc is operation on not rotated frame, make opposite rotation
        pPerFrameSettings->deviceOrientation = 360 - pPerFrameSettings->deviceOrientation;
    }

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "ReqId[%llu] : Device Orienation using for this request %d",
                     requestId, pPerFrameSettings->deviceOrientation);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::SetupNCSLink
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::SetupNCSLink()
{
    CamxResult result = CamxResultEFailed;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Setting up an NCS link");

    // Get the NCS service object handle
    m_pNCSServiceObject = reinterpret_cast<NCSService *>(HwEnvironment::GetInstance()->GetNCSObject());
    if (NULL != m_pNCSServiceObject)
    {
        result = SetupNCSLinkForSensor(NCSGravityType);
        if (result != CamxResultSuccess)
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Unable to Setup NCS Link For Gravity Sensor result %d", result);
        }
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupFD, "Unable to get NCS service object");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::SetupNCSLinkForSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::SetupNCSLinkForSensor(
    NCSSensorType  sensorType)
{
    CamxResult      result       = CamxResultEFailed;
    NCSSensorConfig sensorConfig = { 0 };
    NCSSensorCaps   sensorCaps   = {};

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Setting up an NCS link for sensor %d", sensorType);
    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    result = m_pNCSServiceObject->QueryCapabilites(&sensorCaps, sensorType);
    if (result == CamxResultSuccess)
    {
        switch (sensorType)
        {
            case NCSGravityType:
                // Clients responsibility to set it to that config which is supported
                sensorConfig.samplingRate = static_cast<FLOAT>(pStaticSettings->gravitySensorSamplingRate);
                sensorConfig.reportRate   = pStaticSettings->gravityDataReportRate;

                CAMX_LOG_VERBOSE(CamxLogGroupFD, "SR = %d. RR = %d",
                                 pStaticSettings->gravitySensorSamplingRate,
                                 pStaticSettings->gravityDataReportRate);

                sensorConfig.operationMode = 0;   // batched reporting mode
                sensorConfig.sensorType    = sensorType;
                m_pNCSSensorHandleGravity  = m_pNCSServiceObject->RegisterService(sensorType, &sensorConfig);
                if (NULL == m_pNCSSensorHandleGravity)
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "Unable to register %d with the NCS !!", sensorType);
                    result = CamxResultEFailed;
                }
                break;
            default:
                CAMX_ASSERT_ALWAYS_MESSAGE("Unexpected sensor ID: %d", sensorType);
                break;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Unable to Query caps sensor type %d error %s", sensorType,
                       Utils::CamxResultToString(result));
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::PopulateGravityData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::PopulateGravityData(
    float gravityValues[])
{
    CamxResult result = CamxResultSuccess;

    if (m_pNCSSensorHandleGravity != NULL)
    {
        NCSSensorData*  pDataObj     = NULL;
        NCSDataGravity* pGravityData = NULL;
        INT             sampleCount  = 0;
        UINT64          timeStamp[32];

        // Max number of gravity samples needed per frame is 1 as of now.
        pDataObj = m_pNCSSensorHandleGravity->GetLastNSamples(NSCSamplecount);
        if (NULL != pDataObj)
        {
            INT index = 0;

            sampleCount = static_cast<INT>(pDataObj->GetNumSamples());
            if ((0 > sampleCount) || (NSCSamplecount != sampleCount))
            {
                CAMX_LOG_ERROR(CamxLogGroupFD, "Samplecount not equal to required");
                result = CamxResultEFailed;
            }
            CAMX_LOG_VERBOSE(CamxLogGroupFD, "sampleCount %d ", sampleCount);
            if (CamxResultSuccess == result)
            {
                for (index = 0; index < sampleCount; index++)
                {
                    pDataObj->SetIndex(index);
                    pGravityData = reinterpret_cast<NCSDataGravity*>(pDataObj->GetCurrent());
                    if (NULL != pGravityData)
                    {
                        BOOL useDSPS = TRUE;
                        // change the axes in accordance  with that of DSPS.x<-->Y;  Z -> - Z
                        if (TRUE == useDSPS)
                        {
                            gravityValues[0] = pGravityData->y;
                            gravityValues[1] = pGravityData->x;
                            gravityValues[2] = -pGravityData->z;
                        }
                        else
                        {
                            gravityValues[0] = pGravityData->x;
                            gravityValues[1] = pGravityData->y;
                            gravityValues[2] = pGravityData->z;
                        }
                        timeStamp[index] = pGravityData->timestamp;
                        CAMX_LOG_VERBOSE(CamxLogGroupFD, "index xyz timestamp %d %f %f %f %llu", index,
                            gravityValues[0],
                            gravityValues[1],
                            gravityValues[2],
                            timeStamp[index]);
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupFD, "GetCurrrent returned null");
                        break;
                    }
                }
            }

            m_pNCSSensorHandleGravity->PutBackDataObj(pDataObj);
            if (index == sampleCount)
            {
                result = CamxResultSuccess;
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "NULL obj");
            result = CamxResultEFailed;
        }
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupFD, "NCS Gravity handle is NULL");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::IsFDManagerNodeStreamOff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL FDManagerNode::IsFDManagerNodeStreamOff()
{
    BOOL isStreamOffFlag;

    if (NULL != m_pLock)
    {
        m_pLock->Lock();
    }

    isStreamOffFlag = m_isNodeStreamedOff;

    if (NULL != m_pLock)
    {
        m_pLock->Unlock();
    }

    return isStreamOffFlag;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::OnStreamOn
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::OnStreamOn()
{
    CamxResult result = CamxResultSuccess;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Prepare stream on");

    // Restart JOB after flush
    if ((TRUE             == GetStaticSettings()->enableFDManagerThreading) &&
        (InvalidJobHandle != m_hThread)                                     &&
        (NULL             != m_pThreadManager))
    {
        m_pThreadManager->ResumeJobFamily(m_hThread);
    }

    m_pLock->Lock();
    m_isNodeStreamedOff = FALSE;
    m_pLock->Unlock();

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::OnStreamOff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::OnStreamOff(
    CHIDEACTIVATEPIPELINEMODE modeBitmask)
{
    CamxResult result = CamxResultSuccess;

    CAMX_UNREFERENCED_PARAM(modeBitmask);

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "Prepare stream off");

    if (NULL != m_pLock)
    {
        m_pLock->Lock();
    }

    m_isNodeStreamedOff = TRUE;

    if (NULL != m_pLock)
    {
        m_pLock->Unlock();
    }
    // Flush the pending jobs now
    if ((TRUE             == GetStaticSettings()->enableFDManagerThreading) &&
        (InvalidJobHandle != m_hThread)                                     &&
        (NULL             != m_pThreadManager))
    {
        m_pThreadManager->FlushJobFamily(m_hThread, this, TRUE);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::FDFramePreprocessingRequired
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL FDManagerNode::FDFramePreprocessingRequired(
    FDPerFrameSettings* pPerFrameSettings)
{
    BOOL bPreprocessingRequired = FALSE;

    CAMX_ASSERT(NULL != pPerFrameSettings);

    if ((TRUE    == pPerFrameSettings->skipProcess)       ||
        (Disable == GetStaticSettings()->FDPreprocessing) ||
        (NULL    == m_preprocessingData.hLibHandle))
    {
        bPreprocessingRequired = FALSE;
    }
    else if (TRUE == GetStaticSettings()->enableFDPreprocessingAlways)
    {
        bPreprocessingRequired = TRUE;
    }
    else
    {
        if (AdaptiveGTM == GetStaticSettings()->FDPreprocessing)
        {
            static const UINT PropertyTag[]       = { PropertyIDAECFrameControl };
            const UINT        propNum             = CAMX_ARRAY_SIZE(PropertyTag);
            VOID*             pData[propNum]      = { 0 };
            UINT64            dataOffset[propNum] = { 0 };

            GetDataList(PropertyTag, pData, dataOffset, propNum);
            if (NULL != pData[0])
            {
                // Check conditions to determine whether preprocessing is required or not
                static const FLOAT ExposureShortBySafeThreshold = m_FDConfig.swPreprocess.exposureShortBySafeThreshold;
                static const FLOAT DeltaEVFromTargetThreshold   = m_FDConfig.swPreprocess.deltaEVFromTargetThreshold;
                FLOAT              exposureShortBySafe          = 1.0f;
                FLOAT              safeDeltaEVFromTarget        = 0.0f;
                FLOAT              exposureShortSensitivity     = 1.0f;
                FLOAT              exposureSafeSensitivity      = 1.0f;
                BOOL               shortBySafeTrigger           = FALSE;
                BOOL               safeDeltaEVTrigger           = FALSE;
                AECFrameControl*   pAECFrameControlData         = reinterpret_cast<AECFrameControl*>(pData[0]);

                exposureSafeSensitivity  = pAECFrameControlData->exposureInfo[ExposureIndexSafe].sensitivity;
                exposureShortSensitivity = pAECFrameControlData->exposureInfo[ExposureIndexShort].sensitivity;

                exposureSafeSensitivity  = (exposureSafeSensitivity > 0.0f) ? exposureSafeSensitivity : 1.0f;
                exposureShortBySafe      = exposureShortSensitivity / exposureSafeSensitivity;

                safeDeltaEVFromTarget    = pAECFrameControlData->exposureInfo[ExposureIndexSafe].deltaEVFromTarget;

                shortBySafeTrigger       = (exposureShortBySafe < ExposureShortBySafeThreshold) ? TRUE : FALSE;
                safeDeltaEVTrigger       = (safeDeltaEVFromTarget < DeltaEVFromTargetThreshold) ? TRUE : FALSE;

                if ((TRUE == shortBySafeTrigger) || (TRUE == safeDeltaEVTrigger))
                {
                    bPreprocessingRequired = TRUE;
                }

                CAMX_LOG_VERBOSE(CamxLogGroupFD, "Instance[%d] ReqId[%llu] : exposureShortBySafe = %f, "
                                 "shortBySafeThreshold = %f, shortBySafeTrigger = %d, safeDeltaEVFromTarget = %f, "
                                 "deltaEVThreshold = %f, safeDeltaEVTrigger = %d, bPreprocessingRequired=%d",
                                 InstanceID(),
                                 pPerFrameSettings->requestId,
                                 exposureShortBySafe,
                                 ExposureShortBySafeThreshold,
                                 shortBySafeTrigger,
                                 safeDeltaEVFromTarget,
                                 DeltaEVFromTargetThreshold,
                                 safeDeltaEVTrigger,
                                 bPreprocessingRequired);
            }
            else
            {
                bPreprocessingRequired = FALSE;
                CAMX_LOG_ERROR(CamxLogGroupFD, "Instance[%d] ReqId[%llu] : failed to get AEC frame control data",
                               InstanceID(), pPerFrameSettings->requestId);
            }
        }
        else if (GTM == GetStaticSettings()->FDPreprocessing)
        {
            // Always set preprocessing flag to true in case of GTM.
            // Check ADRC enable at applying stage.
            bPreprocessingRequired = TRUE;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupFD, "Node %s, Invalid selection of preprocessing type %d",
                           NodeIdentifierString(), GetStaticSettings()->FDPreprocessing);
            bPreprocessingRequired = FALSE;
        }
    }

    return bPreprocessingRequired;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDManagerNode::DoFDFramePreprocessing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::DoFDFramePreprocessing(
    FDPerFrameSettings* pPerFrameSettings)
{
    UINT64      requestId   = pPerFrameSettings->requestId;
    CamxResult  result      = CamxResultSuccess;
    CamxTime    startTime;
    SWProcAlgoProcessParams processParams;

    if (TRUE == GetStaticSettings()->enableFDPreprocessDumping)
    {
        CHAR    inputFilename[FILENAME_MAX];
        FILE*   pInputFileFD;

        OsUtils::SNPrintF(inputFilename, sizeof(inputFilename),
                          "%s%sfdtm%d_%llu_input_width_%d_height_%d_stride_%d_scanline_%d.yuv",
                          FileDumpPath,   PathSeparator,   InstanceID(),    requestId,
                          m_FDFrameWidth, m_FDFrameHeight, m_FDFrameStride, m_FDFrameScanline);

        pInputFileFD  = OsUtils::FOpen(inputFilename, "wb");

        CAMX_LOG_INFO(CamxLogGroupFD, "input=%s (%p)", inputFilename, pInputFileFD);

        if (NULL != pInputFileFD)
        {
            OsUtils::FWrite(pPerFrameSettings->pImageAddress, m_FDFrameStride * m_FDFrameHeight, 1, pInputFileFD);

            OsUtils::FClose(pInputFileFD);
        }

        OsUtils::GetTime(&startTime);
    }

    if ((AdaptiveGTM == GetStaticSettings()->FDPreprocessing) ||
        (GTM         == GetStaticSettings()->FDPreprocessing))
    {
        BOOL            bSkipPreprocessing = FALSE;
        SWProcGTMParams paramsGTM          = { 0 };

        paramsGTM.bIsInlineProcessing      = TRUE;
        paramsGTM.image.width              = m_FDFrameWidth;
        paramsGTM.image.height             = m_FDFrameHeight;
        paramsGTM.image.planeCount         = 1;
        paramsGTM.image.planes[0].type     = SWProcPlaneType_Y;
        paramsGTM.image.planes[0].pBuffer  = pPerFrameSettings->pImageAddress;
        paramsGTM.image.planes[0].stride   = m_FDFrameStride;
        paramsGTM.image.planes[0].scanline = m_FDFrameScanline;
        paramsGTM.bIsAdaptiveGTM           = (AdaptiveGTM == GetStaticSettings()->FDPreprocessing) ? TRUE : FALSE;

        if (GTM == GetStaticSettings()->FDPreprocessing)
        {
            // Extra Gain factor for GTM, initialize to default value
            FLOAT  extraGain = GTMExtraGain;

            // Get ADRC parameter for current frame in case of GTM
            static const UINT      PropertyTag[]       = { PropertyIDIFEADRCParams };
            const UINT             propNum             = CAMX_ARRAY_SIZE(PropertyTag);
            VOID*                  pData[propNum]      = { 0 };
            UINT64                 dataOffset[propNum] = { 0 };
            BOOL                   bIsADRCEnabled      = FALSE;
            PropertyISPADRCParams* pADRCParams;

            GetDataList(PropertyTag, pData, dataOffset, propNum);

            if (NULL != pData[0])
            {
                pADRCParams   = reinterpret_cast<PropertyISPADRCParams*>(pData[0]);
                bIsADRCEnabled = pADRCParams->bIsADRCEnabled;

                if (TRUE == bIsADRCEnabled)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupFD,
                                     "ADRC parameters: gtmPercentage = %f, drcGain = %f, drcGainDark = %f",
                                     pADRCParams->GTMPercentage,
                                     pADRCParams->DRCGain,
                                     pADRCParams->DRCDarkGain);
                }
                else
                {
                    // if ADRC is disabled, skip SW preprocessing
                    bSkipPreprocessing = TRUE;
                    CAMX_LOG_VERBOSE(CamxLogGroupFD, "ADRC is disabled");
                }
            }
            else
            {
                bSkipPreprocessing = TRUE;
                CAMX_LOG_WARN(CamxLogGroupFD, "Failed to get ADRC parameters,skip FD frame preprocessing");
            }

            if (FALSE == bSkipPreprocessing)
            {
                // Calculate Gamma and Gain Factor for GTM processing
                if (8 <= pADRCParams->DRCGain)
                {
                    paramsGTM.gammaFactor = DRCGain8XGamma;
                }
                else if (4 <= pADRCParams->DRCGain)
                {
                    paramsGTM.gammaFactor
                        = (((pADRCParams->DRCGain - 4) / 4) * (DRCGain8XGamma - DRCGain4XGamma)) + DRCGain4XGamma;
                }
                else if (2 <= pADRCParams->DRCGain)
                {
                    paramsGTM.gammaFactor
                        = (((pADRCParams->DRCGain - 2) / 2) * (DRCGain4XGamma - DRCGain2XGamma)) + DRCGain2XGamma;
                }
                else
                {
                    paramsGTM.gammaFactor
                        = ((pADRCParams->DRCGain - 1) * (DRCGain2XGamma - DRCGainDefaultGamma)) + DRCGainDefaultGamma;
                }

                paramsGTM.gainFactor  = pADRCParams->DRCDarkGain * GTMExtraGain;
                paramsGTM.bIsFixedGTM = TRUE;
            }
            else
            {
                paramsGTM.bIsFixedGTM = FALSE;
            }
        }

        processParams.type              = SWProcTypeGTM;
        processParams.sizeOfSWProcData  = sizeof(paramsGTM);
        processParams.pSWProcData       = &paramsGTM;

        if (FALSE == bSkipPreprocessing)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupFD, "Node %s ReqId[%llu] Applying SW Preprocessing",
                             NodeIdentifierString(), requestId);
            result = m_preprocessingData.pfnSWAlgoProcess(&processParams);
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupFD, "Node %s ReqId[%llu] Skipping GTM because ADRC is disabled",
                          NodeIdentifierString(), requestId);
        }
    }
    else if (LTM == GetStaticSettings()->FDPreprocessing)
    {
        SWProcLTMParams paramsLTM = { 0 };

        paramsLTM.bIsInlineProcessing       = TRUE;
        paramsLTM.image.width               = m_FDFrameWidth;
        paramsLTM.image.height              = m_FDFrameHeight;
        paramsLTM.image.planeCount          = 1;
        paramsLTM.image.planes[0].type      = SWProcPlaneType_Y;
        paramsLTM.image.planes[0].pBuffer   = pPerFrameSettings->pImageAddress;
        paramsLTM.image.planes[0].stride    = m_FDFrameStride;
        paramsLTM.image.planes[0].scanline  = m_FDFrameScanline;
        paramsLTM.contrastFactor            = 25.0f;
        paramsLTM.normalizedGrayPoint       = 0.5f;
        paramsLTM.clipLimit                 = 4.0f;
        paramsLTM.gridX                     = 4;
        paramsLTM.gridY                     = 3;

        processParams.type                  = SWProcTypeLTM;
        processParams.sizeOfSWProcData      = sizeof(paramsLTM);
        processParams.pSWProcData           = &paramsLTM;

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Node %s ReqId[%llu] Applying LTM", NodeIdentifierString(), requestId);

        /// @todo (CAMX-4037) Add below hardcoded values as tuning params
        result = m_preprocessingData.pfnSWAlgoProcess(&processParams);
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Unexpected FDPreprocessing: %d", GetStaticSettings()->FDPreprocessing);
    }

    // Need to do cache flush to make sure contents are flushed to main memory so the FD HW reads the correct data
    if ((NULL != pPerFrameSettings->pImageBuffer))
    {
        pPerFrameSettings->pImageBuffer->CacheOps(false, true);
    }

    if (TRUE == GetStaticSettings()->enableFDPreprocessDumping)
    {
        CHAR        outputFilename[FILENAME_MAX];
        FILE*       pOutputFileFD;
        CamxTime    endTime;

        OsUtils::GetTime(&endTime);

        CAMX_LOG_INFO(CamxLogGroupFD, "Node %s ReqId[%llu] FDPreprocessing time = %d ms",
                      NodeIdentifierString(), requestId,
                      OsUtils::CamxTimeToMillis(&endTime) - OsUtils::CamxTimeToMillis(&startTime));

        OsUtils::SNPrintF(outputFilename, sizeof(outputFilename),
                          "%s%sfdtm%d_%llu_output_width_%d_height_%d_stride_%d_scanline_%d.yuv",
                          FileDumpPath,   PathSeparator,   InstanceID(),    requestId,
                          m_FDFrameWidth, m_FDFrameHeight, m_FDFrameStride, m_FDFrameScanline);

        pOutputFileFD = OsUtils::FOpen(outputFilename, "wb");

        CAMX_LOG_INFO(CamxLogGroupFD, "output=%s(%p)", outputFilename, pOutputFileFD);

        if (NULL != pOutputFileFD)
        {
            OsUtils::FWrite(pPerFrameSettings->pImageAddress, m_FDFrameStride * m_FDFrameHeight, 1, pOutputFileFD);

            OsUtils::FClose(pOutputFileFD);
        }
    }

    // We dont need to invalidate immediately as there might still be some sw processing such as false positive filter
    // set this flag so that we can invalidate the buffer at the end of sw processing
    pPerFrameSettings->inputBufferInvalidateRequired = TRUE;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDManagerNode::QueryMetadataPublishList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDManagerNode::QueryMetadataPublishList(
    NodeMetadataList* pPublistTagList)
{
    CamxResult result   = CamxResultSuccess;
    UINT32     tagCount = 0;
    UINT32     tagID;

    for (UINT32 tagIndex = 0; tagIndex < CAMX_ARRAY_SIZE(FDOutputTags); ++tagIndex)
    {
        pPublistTagList->tagArray[tagCount++] = FDOutputTags[tagIndex];
    }

    for (UINT32 tagIndex = 0; tagIndex < CAMX_ARRAY_SIZE(FDOutputVendorTags); ++tagIndex)
    {
        result = VendorTagManager::QueryVendorTagLocation(
            FDOutputVendorTags[tagIndex].pSectionName,
            FDOutputVendorTags[tagIndex].pTagName,
            &tagID);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot query vendor tag %s %s",
                FDOutputVendorTags[tagIndex].pSectionName,
                FDOutputVendorTags[tagIndex].pTagName);
            break;
        }

        pPublistTagList->tagArray[tagCount++] = tagID;
    }

    pPublistTagList->tagCount = tagCount;
    CAMX_LOG_VERBOSE(CamxLogGroupMeta, "%d tags will be published", tagCount);
    return result;
}

CAMX_NAMESPACE_END
