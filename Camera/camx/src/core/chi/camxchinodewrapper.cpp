////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxchinodewrapper.cpp
/// @brief Chi node wrapper class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !_WINDOWS
#include <cutils/properties.h>
#endif // _WINDOWS

#include "camxchi.h"
#include "camxchinodewrapper.h"
#include "camxsettingsmanager.h"
#include "camxhal3metadatautil.h"
#include "camxhwenvironment.h"
#include "camxpipeline.h"
#include "inttypes.h"
#include "camximagebuffer.h"
#include "camxthreadmanager.h"
#include "camxtuningdatamanager.h"
#include "chincsdefs.h"
#include "camxtitan17xcontext.h"
#include "camxtitan17xdefs.h"
#include "camxcsl.h"

// NOWHINE FILE CP036a: const_cast<> is prohibited

CAMX_NAMESPACE_BEGIN

static const INT    FenceCompleteProcessSequenceId   = -2;                    ///< Fence complete sequence id
static const UINT32 MaximumChiNodeWrapperInputPorts  = 32;                    ///< The maximum input ports for Chi node wrapper
static const UINT32 MaximumChiNodeWrapperOutputPorts = 32;                    ///< The maximum output ports for Chi node wrapper
static const CHAR*  pMemcpyNodeName                  = "com.qti.node.memcpy"; ///< memcpy node name string

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ChiNodeWrapper* ChiNodeWrapper::Create(
    const NodeCreateInputData*   pCreateInputData,
    NodeCreateOutputData*        pCreateOutputData)
{
    CAMX_UNREFERENCED_PARAM(pCreateOutputData);
    UINT32 enableDummynode = 0;

#if !_WINDOWS
    CHAR   prop[PROPERTY_VALUE_MAX];
    property_get("persist.vendor.camera.enabledummynode", prop, "0");
    enableDummynode = atoi(prop);
#endif // _WINDOWS

    CamxResult      result       = CamxResultSuccess;
    ChiNodeWrapper* pChiNode     = CAMX_NEW ChiNodeWrapper();
    if (NULL != pChiNode)
    {
        ExternalComponentInfo externalComponentInfo = {0};

        if (1 == enableDummynode)
        {
            // NOWHINE CP036a: Const cast allowed for conversion to interfaces
            externalComponentInfo.pComponentName = const_cast<CHAR*>(pMemcpyNodeName);
        }
        else
        {
            externalComponentInfo.pComponentName = static_cast<CHAR*>(pCreateInputData->pNodeInfo->pNodeProperties->pValue);
        }

        result = HwEnvironment::GetInstance()->SearchExternalComponent(&externalComponentInfo, 1);
        if (CamxResultSuccess == result)
        {
            memcpy(&pChiNode->m_nodeCallbacks, &externalComponentInfo.nodeCallbacks, sizeof(ChiNodeCallbacks));

            pChiNode->m_numInputPort  = pCreateInputData->pNodeInfo->inputPorts.numPorts;
            pChiNode->m_numOutputPort = pCreateInputData->pNodeInfo->outputPorts.numPorts;

            CAMX_ASSERT(0 < pChiNode->m_numInputPort);
            CAMX_ASSERT(0 < pChiNode->m_numOutputPort);

            pChiNode->m_phInputBuffer   = pChiNode->AllocateChiBufferHandlePool(
                                            pChiNode->m_numInputPort * MaxRequestQueueDepth);
            pChiNode->m_phOutputBuffer  = pChiNode->AllocateChiBufferHandlePool(pChiNode->m_numOutputPort * MaxOutputBuffers);

            UINT32 size = (sizeof(CHINODEBYPASSINFO) * pChiNode->m_numOutputPort);
            pChiNode->m_pBypassData = static_cast<CHINODEBYPASSINFO*>(CAMX_CALLOC(size));
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupChi, "Failed to load Chi interface for %s", externalComponentInfo.pComponentName);
        }

        pChiNode->m_pChiContext = pCreateInputData->pChiContext;

        if ((NULL == pChiNode->m_phInputBuffer) || (NULL == pChiNode->m_phOutputBuffer))
        {
            CAMX_LOG_ERROR(CamxLogGroupChi, "Failed to allocate memory");
            CAMX_DELETE pChiNode;
            pChiNode = NULL;
        }

    }

    return pChiNode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNCacheOps
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNCacheOps(
    CHINODEBUFFERHANDLE hChiBuffer,
    BOOL                invalidata,
    BOOL                clean)
{
    CDKResult     result        = CDKResultEFailed;
    ChiImageList* pImageBuffers = static_cast<ChiImageList*>(hChiBuffer);
    INT32         hLastHandle   = 0;

    for (UINT i = 0; i < pImageBuffers->numberOfPlanes; i++)
    {
        if ((0 != pImageBuffers->handles[i]) && (hLastHandle != pImageBuffers->handles[i]))
        {
            CSLMemHandle hCSLHandle = static_cast<CSLMemHandle>(pImageBuffers->handles[i]);

            result = CSLBufferCacheOp(hCSLHandle, invalidata, clean);
            if ( result !=  CDKResultSuccess)
            {
                CAMX_LOG_ERROR(CamxLogGroupChi, "Buffer cache op failed %d", result);
                break;
            }
            hLastHandle = pImageBuffers->handles[i];
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNGetMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNGetMetadata(
    CHIMETADATAINFO* pMetadataInfo)
{
    CAMX_ASSERT(NULL != pMetadataInfo);
    CAMX_ASSERT(NULL != pMetadataInfo->chiSession);

    CDKResult result = CDKResultSuccess;
    if (pMetadataInfo->size <= sizeof(CHIMETADATAINFO))
    {
        ChiNodeWrapper* pNode   = static_cast<ChiNodeWrapper*>(pMetadataInfo->chiSession);
        result                  = pNode->GetMetadata(pMetadataInfo);
    }
    else
    {
        result = CDKResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupChi, "MetadataInfo size mismatch");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNGetMultiCamDynamicMetaByCamId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNGetMultiCamDynamicMetaByCamId(
    CHIMETADATAINFO* pMetadataInfo,
    UINT32           cameraId)
{
    CAMX_ASSERT(NULL != pMetadataInfo);
    CAMX_ASSERT(NULL != pMetadataInfo->chiSession);

    CDKResult result = CDKResultSuccess;
    if (pMetadataInfo->size <= sizeof(CHIMETADATAINFO))
    {
        ChiNodeWrapper* pNode   = static_cast<ChiNodeWrapper*>(pMetadataInfo->chiSession);
        result                  = pNode->GetMulticamDynamicMetaByCamId(pMetadataInfo, cameraId);
    }
    else
    {
        result = CDKResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupChi, "MedatadataInfo size mismatch");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNSetMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNSetMetadata(
    CHIMETADATAINFO* pMetadataInfo)
{
    CAMX_ASSERT(NULL != pMetadataInfo);
    CAMX_ASSERT(NULL != pMetadataInfo->chiSession);

    CDKResult result = CDKResultSuccess;
    if (pMetadataInfo->size <= sizeof(CHIMETADATAINFO))
    {
        ChiNodeWrapper* pNode   = static_cast<ChiNodeWrapper*>(pMetadataInfo->chiSession);
        result                  = pNode->SetMetadata(pMetadataInfo);
    }
    else
    {
        result = CDKResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupChi, "MedatadataInfo size mismatch");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNGetTuningmanager
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* ChiNodeWrapper::FNGetTuningmanager(
    CHIHANDLE hChiSession)
{
    CAMX_ASSERT(NULL != hChiSession);

    TuningDataManager* pTuningManager     = NULL;
    TuningSetManager*  pTuningDataManager = NULL;

    ChiNodeWrapper* pNode = static_cast<ChiNodeWrapper*>(hChiSession);
    if (NULL != pNode)
    {
        pTuningManager = pNode->GetTuningDataManager();
        if ((NULL != pTuningManager) &&
            (TRUE == pTuningManager->IsValidChromatix()))
        {
            pTuningDataManager = static_cast<TuningSetManager*>(pTuningManager->GetChromatix());
        }
    }
    return pTuningDataManager;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNGetVendorTagBase
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNGetVendorTagBase(
    CHIVENDORTAGBASEINFO* pVendorTagBaseInfo)
{
    CAMX_ASSERT(NULL != pVendorTagBaseInfo);
    CDKResult result = CDKResultSuccess;
    if (pVendorTagBaseInfo->size <= sizeof(CHIVENDORTAGBASEINFO))
    {
        UINT32 sectionBase      = 0;
        CamxResult resultCamx   = CamxResultSuccess;
        if (NULL == pVendorTagBaseInfo->pTagName)
        {
            resultCamx = VendorTagManager::QueryVendorTagSectionBase(pVendorTagBaseInfo->pComponentName, &sectionBase);
        }
        else
        {
            resultCamx = VendorTagManager::QueryVendorTagLocation(pVendorTagBaseInfo->pComponentName,
                                                                  pVendorTagBaseInfo->pTagName, &sectionBase);
        }

        if (CamxResultSuccess != resultCamx)
        {
            result = CDKResultEFailed;
        }
        else
        {
            pVendorTagBaseInfo->vendorTagBase = sectionBase;
        }
    }
    else
    {
        result = CDKResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupChi, "CHIVENDORTAGBASEINFO size mismatch");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNProcRequestDone
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNProcRequestDone(
    CHINODEPROCESSREQUESTDONEINFO* pInfo)
{
    CAMX_ASSERT(NULL != pInfo);
    CAMX_ASSERT(NULL != pInfo->hChiSession);

    CDKResult result = CDKResultSuccess;
    if (pInfo->size <= sizeof(CHINODEPROCESSREQUESTDONEINFO))
    {
        ChiNodeWrapper* pNode   = static_cast<ChiNodeWrapper*>(pInfo->hChiSession);
        result                  = pNode->ProcRequestDone(pInfo);
    }
    else
    {
        result = CDKResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupChi, "RequestDone Info size mismatch");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNProcMetadataDone
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNProcMetadataDone(
    CHINODEPROCESSMETADATADONEINFO* pInfo)
{
    CAMX_ASSERT(NULL != pInfo);
    CAMX_ASSERT(NULL != pInfo->hChiSession);

    CDKResult result = CDKResultSuccess;
    if (pInfo->size <= sizeof(CHINODEPROCESSMETADATADONEINFO))
    {
        ChiNodeWrapper* pNode = static_cast<ChiNodeWrapper*>(pInfo->hChiSession);
        result                = pNode->ProcMetadataDone(pInfo);
    }
    else
    {
        result = CDKResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupChi, "MetadataDone Info size mismatch");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNCreateFence
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNCreateFence(
    CHIHANDLE               hChiSession,
    CHIFENCECREATEPARAMS*   pInfo,
    CHIFENCEHANDLE*         phChiFence)
{
    CAMX_ASSERT(NULL != pInfo);
    CAMX_ASSERT(NULL != hChiSession);

    CDKResult result = CDKResultSuccess;
    if (NULL == phChiFence)
    {
        CAMX_LOG_ERROR(CamxLogGroupChi, "phChiFence is NULL");
        result = CDKResultEInvalidArg;
    }
    else if (pInfo->size > sizeof(CHIFENCECREATEPARAMS))
    {
        result = CDKResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupChi, "CreateFence Info size mismatch");
    }
    else
    {
        ChiNodeWrapper* pNode   = static_cast<ChiNodeWrapper*>(hChiSession);
        // do we need to convert result?
        result = pNode->GetChiContext()->CreateChiFence(pInfo, phChiFence);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNReleaseFence
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNReleaseFence(
    CHIHANDLE       hChiSession,
    CHIFENCEHANDLE  hChiFence)
{
    CAMX_ASSERT(NULL != hChiSession);

    CDKResult   result  = CDKResultSuccess;
    ChiFence*   pChiFence   = static_cast<ChiFence*>(hChiFence);
    if (NULL != pChiFence)
    {
        ChiNodeWrapper* pNode   = static_cast<ChiNodeWrapper*>(hChiSession);
        result = pNode->GetChiContext()->ReleaseChiFence(hChiFence);
    }
    else
    {
        result = CDKResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupChi, "hChiFence is Invalid");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNGetDataSource
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNGetDataSource(
    CHIHANDLE            hChiSession,
    CHIDATASOURCE*       pDataSource,
    CHIDATASOURCECONFIG* pDataSourceConfig)
{
    CAMX_UNREFERENCED_PARAM(hChiSession);

    CamxResult              result               = CDKResultSuccess;
    CHINCSDATASOURCECONFIG* pNCSDataSourceConfig = NULL;
    NCSSensorConfig         lNCSConfig           = { 0 };
    BOOL                    isSensorValid        = TRUE;

    if ((NULL != pDataSource) && (NULL != pDataSourceConfig))
    {
        switch (pDataSourceConfig->sourceType)
        {
            case ChiDataGyro:
                pNCSDataSourceConfig = reinterpret_cast<CHINCSDATASOURCECONFIG*>(pDataSourceConfig->pConfig);
                lNCSConfig.sensorType = NCSGyroType;
                break;
            case ChiDataAccel:
                pNCSDataSourceConfig = reinterpret_cast<CHINCSDATASOURCECONFIG*>(pDataSourceConfig->pConfig);
                lNCSConfig.sensorType = NCSAccelerometerType;
                break;
            default:
                CAMX_LOG_ERROR(CamxLogGroupCore, "Unsupported data source");
                result = CDKResultEUnsupported;
                lNCSConfig.sensorType = NCSMaxType;
                isSensorValid = FALSE;
                break;
        }

        if (TRUE == isSensorValid)
        {
            NCSSensor* pSensorObject = NULL;

            CAMX_ASSERT_MESSAGE(pNCSDataSourceConfig->size == sizeof(CHINCSDATASOURCECONFIG),
                                "Data Source config structure of invalid size !!");

            lNCSConfig.operationMode = pNCSDataSourceConfig->operationMode;
            lNCSConfig.reportRate    = pNCSDataSourceConfig->reportRate;
            lNCSConfig.samplingRate  = pNCSDataSourceConfig->samplingRate;

            pSensorObject = SetupNCSLink(&lNCSConfig);
            if (NULL != pSensorObject)
            {
                pDataSource->dataSourceType = pDataSourceConfig->sourceType;
                pDataSource->pHandle        = static_cast<VOID*>(pSensorObject);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupCore, "Setup NCS link failed");
                result               = CDKResultEFailed;
                pSensorObject        = NULL;
                pDataSource->pHandle = NULL;
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupChi, "Invalid pointer to the data source pointer!!");
        result               = CDKResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNGetData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* ChiNodeWrapper::FNGetData(
    CHIHANDLE            hChiSession,
    CHIDATASOURCEHANDLE  hDataSourceHandle,
    CHIDATAREQUEST*      pDataRequest,
    UINT*                pSize)
{
    VOID* pData = NULL;

    CAMX_ASSERT_MESSAGE((NULL != hChiSession) && (NULL != hDataSourceHandle) && (NULL != pDataRequest),
                        "Invalid input params");

    switch (hDataSourceHandle->dataSourceType)
    {
        case ChiDataGyro:
        case ChiDataAccel:
            pData = FNGetDataNCS(hChiSession, hDataSourceHandle, pDataRequest, pSize);
            break;
        case ChiTuningManager:
            pData = FNGetTuningmanager(hChiSession);
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupChi, "Unsupported data source type");
            break;
    }

    return pData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNPutData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNPutData(
    CHIHANDLE            hChiSession,
    CHIDATASOURCEHANDLE  hDataSourceHandle,
    CHIDATAHANDLE        hData)
{
    CamxResult result        = CamxResultSuccess;
    NCSSensor* pSensorObject = NULL;

    CAMX_ASSERT_MESSAGE((NULL != hChiSession) && (NULL != hDataSourceHandle),
                        "Invalid input params");

    if ((NULL != hDataSourceHandle->pHandle) && (NULL != hData))
    {
        switch (hDataSourceHandle->dataSourceType)
        {
            case ChiDataGyro:
            case ChiDataAccel:
                pSensorObject = static_cast<NCSSensor*>(hDataSourceHandle->pHandle);
                result = pSensorObject->PutBackDataObj(
                    static_cast<NCSSensorData*>(hData));
                break;
            default:
                CAMX_LOG_ERROR(CamxLogGroupChi, "Unsupported data source type");
                result = CamxResultEUnsupported;
                break;
        }
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_LOG_ERROR(CamxLogGroupChi, "Invalid accessor object");
    }

    return CamxResultToCDKResult(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNGetDataNCS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* ChiNodeWrapper::FNGetDataNCS(
    CHIHANDLE            hChiSession,
    CHIDATASOURCEHANDLE  hDataSourceHandle,
    CHIDATAREQUEST*      pDataRequest,
    UINT*                pSize)
{
    NCSSensor*      pSensorObject      = NULL;
    NCSSensorData*  pSensorDataObject  = NULL;
    ChiNodeWrapper* pNode              = NULL;
    VOID*           pData              = NULL;
    ChiFence*       pChiFence          = NULL;
    CamxResult      result             = CamxResultSuccess;
    ChiNCSDataRequest* pNCSDataRequest = NULL;

    pNode = static_cast<ChiNodeWrapper*>(hChiSession);

    if (NULL != pDataRequest)
    {
        pNCSDataRequest = reinterpret_cast<ChiNCSDataRequest*>(pDataRequest->hRequestPd);

        switch (pDataRequest->requestType)
        {
            case ChiFetchData:
                pSensorObject = static_cast<NCSSensor*>(hDataSourceHandle->pHandle);
                if ((NULL != pNCSDataRequest) && (pNCSDataRequest->size == sizeof(ChiNCSDataRequest)))
                {
                    if (NULL != pSensorObject)
                    {
                        /// Just get N Recent samples
                        if ( 0 < pNCSDataRequest->numSamples)
                        {
                            pData = pSensorObject->GetLastNSamples(pNCSDataRequest->numSamples);
                        }
                        /// ASynchrounous windowed request
                        else if (NULL != pNCSDataRequest->hChiFence)
                        {
                            pChiFence = static_cast<ChiFence*>(pNCSDataRequest->hChiFence);
                            result = pSensorObject->GetDataAsync(
                                pNCSDataRequest->windowRequest.tStart,
                                pNCSDataRequest->windowRequest.tEnd,
                                pChiFence);
                            pNCSDataRequest->result = CamxResultToCDKResult(result);
                        }
                        /// Synchrounous windowed request
                        else
                        {
                            pData = pSensorObject->GetDataSync(
                                pNCSDataRequest->windowRequest.tStart,
                                pNCSDataRequest->windowRequest.tEnd);
                        }

                        // Populate size if pData is valid
                        if ((NULL != pSize) && (NULL != pData))
                        {
                            *pSize = static_cast<NCSSensorData*>(pData)->GetNumSamples();
                        }
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupChi, "Invalid sensor object");
                        pData = NULL;
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Invalid request payload");
                    pData = NULL;
                }
                break;

            case ChiIterateData:
                pSensorDataObject = static_cast<NCSSensorData*>(hDataSourceHandle->pHandle);
                if (NULL != pSensorDataObject)
                {
                    if (0 <= pDataRequest->index)
                    {
                        pSensorDataObject->SetIndex(pDataRequest->index);
                        pData = pSensorDataObject->GetCurrent();
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupChi, "Invalid index to iterate");
                        pData = NULL;
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Sensor object is NULL");
                    pData = NULL;
                }
                break;
            default:
                break;
        }

    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Invalid input request pointer");
    }

    return pData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNPutDataSource
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNPutDataSource(
    CHIHANDLE           hChiSession,
    CHIDATASOURCEHANDLE hDataSourceHandle)
{
    CDKResult          result     = CDKResultSuccess;

    CAMX_ASSERT_MESSAGE((NULL != hChiSession) || (NULL != hDataSourceHandle),
                        "Invalid input params");

    CAMX_LOG_VERBOSE(CamxLogGroupChi, "Unregister");

    switch (hDataSourceHandle->dataSourceType)
    {
        case ChiDataGyro:
        case ChiDataAccel:
            result = CamxResultToCDKResult(DestroyNCSLink(hDataSourceHandle->pHandle));
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupChi, "Unsupported data source type");
            result = CDKResultSuccess;
            break;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNWaitFenceAsync
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNWaitFenceAsync(
    CHIHANDLE               hChiSession,
    PFNCHIFENCECALLBACK     pCallbackFn,
    CHIFENCEHANDLE          hChiFence,
    VOID*                   pData)
{
    CamxResult result = CamxResultEFailed;

    CAMX_ASSERT((NULL != hChiSession) && (NULL != pData) && (NULL != hChiFence) && (NULL != pCallbackFn));

    ChiNodeWrapper* pNode = static_cast<ChiNodeWrapper*>(hChiSession);

    result = pNode->GetChiContext()->WaitChiFence(hChiFence, pCallbackFn, pData, UINT64_MAX);
    if (CamxResultSuccess != result)
    {
        result = CDKResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNSignalFence
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNSignalFence(
    CHIHANDLE               hChiSession,
    CHIFENCEHANDLE          hChiFence,
    CDKResult               statusResult)
{
    CamxResult result = CamxResultEFailed;

    CAMX_ASSERT((NULL != hChiSession) && (NULL != hChiFence));

    ChiNodeWrapper* pNode = static_cast<ChiNodeWrapper*>(hChiSession);

    result = pNode->GetChiContext()->SignalChiFence(hChiFence,
                                                    CDKResultToCamxResult(statusResult));
    if (CamxResultSuccess != result)
    {
        result = CDKResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNGetFenceStatus
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNGetFenceStatus(
    CHIHANDLE               hChiSession,
    CHIFENCEHANDLE          hChiFence,
    CDKResult*              pFenceResult)
{
    CDKResult     result     = CDKResultEFailed;
    CDKResult     lResult;


    CAMX_ASSERT((NULL != hChiSession) && (NULL != hChiFence) && (NULL != pFenceResult));

    ChiNodeWrapper* pNode = static_cast<ChiNodeWrapper*>(hChiSession);

    result = pNode->GetChiContext()->GetChiFenceResult(hChiFence, &lResult);
    if (CamxResultSuccess != result)
    {
        result        = CDKResultEFailed;
        *pFenceResult = CDKResultEInvalidState;
    }
    else
    {
        if (CDKResultSuccess == lResult)
        {
            *pFenceResult = CDKResultSuccess;
        }
        else if (CDKResultEFailed == lResult)
        {
            *pFenceResult = CDKResultEFailed;
        }
        else
        {
            *pFenceResult = CDKResultEInvalidState;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNBufferManagerCreate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CHIBUFFERMANAGERHANDLE ChiNodeWrapper::FNBufferManagerCreate(
    const CHAR*                     pBufferManagerName,
    CHINodeBufferManagerCreateData* pCreateData)
{

    CHIHALSTREAM               dummyHalStream;
    CHIBufferManagerCreateData createData;
    CHIBUFFERMANAGERHANDLE     hBufferManager = NULL;
    ChiStream                  dummyStream    = {};

    dummyStream.dataspace         = DataspaceUnknown;
    dummyStream.rotation          = ChiStreamRotation::StreamRotationCCW0;
    dummyStream.streamType        = ChiStreamTypeBidirectional;
    dummyStream.format            = static_cast<CHISTREAMFORMAT>(pCreateData->format);
    dummyStream.grallocUsage      = pCreateData->consumerFlags | pCreateData->producerFlags;
    dummyStream.width             = pCreateData->width;
    dummyStream.height            = pCreateData->height;
    dummyStream.maxNumBuffers     = pCreateData->maxBufferCount;
    dummyStream.pHalStream        = &dummyHalStream;

    dummyHalStream.overrideFormat = dummyStream.format;
    dummyHalStream.maxBuffers     = dummyStream.maxNumBuffers;
    dummyHalStream.consumerUsage  = pCreateData->consumerFlags;
    dummyHalStream.producerUsage  = pCreateData->producerFlags;


    createData.width                 = pCreateData->width;
    createData.height                = pCreateData->height;
    createData.format                = pCreateData->format;
    createData.producerFlags         = pCreateData->producerFlags;
    createData.consumerFlags         = pCreateData->consumerFlags;
    createData.bufferStride          = pCreateData->bufferStride;
    createData.maxBufferCount        = pCreateData->maxBufferCount;
    createData.immediateBufferCount  = pCreateData->immediateBufferCount;
    createData.bEnableLateBinding    = pCreateData->bEnableLateBinding;
    createData.bufferHeap            = pCreateData->bufferHeap;
    createData.pChiStream            = &dummyStream;

    hBufferManager = ChiBufferManagerCreate(pBufferManagerName, &createData);

    return hBufferManager;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNBufferManagerDestroy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiNodeWrapper::FNBufferManagerDestroy(
    CHIBUFFERMANAGERHANDLE hBufferManager)
{
    if (NULL != hBufferManager)
    {
        ImageBufferManager* pBufferManager = static_cast<ImageBufferManager*>(hBufferManager);
        pBufferManager->Destroy();
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupMemMgr, "Invalid input! hBufferManager is NULL");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNBufferManagerGetImageBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CHINODEBUFFERHANDLE ChiNodeWrapper::FNBufferManagerGetImageBuffer(
    CHIBUFFERMANAGERHANDLE hBufferManager)
{
    ChiNodeBufferHandleWrapper* pBufferHandleWrapper = NULL;

    if (NULL != hBufferManager)
    {
        ImageBufferManager* pBufferManager = static_cast<ImageBufferManager*>(hBufferManager);

        pBufferHandleWrapper = reinterpret_cast<ChiNodeBufferHandleWrapper*>(CAMX_CALLOC(sizeof(ChiNodeBufferHandleWrapper)));

        if (NULL != pBufferHandleWrapper)
        {
            pBufferHandleWrapper->pImageBuffer = pBufferManager->GetImageBuffer();
            pBufferHandleWrapper->canary       = CANARY;
            if (pBufferHandleWrapper->pImageBuffer != NULL)
            {
                ImageBufferToChiBuffer(pBufferHandleWrapper->pImageBuffer, &pBufferHandleWrapper->hBuffer, TRUE);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupMemMgr, "pImageBuffer is Null");
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupMemMgr, "Out of memory");
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupMemMgr, "Invalid input! hBufferManager is NULL");
    }

    return static_cast<CHINODEBUFFERHANDLE>(&pBufferHandleWrapper->hBuffer);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNBufferManagerReleaseReference
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNBufferManagerReleaseReference(
    CHIBUFFERMANAGERHANDLE hBufferManager,
    CHINODEBUFFERHANDLE    hNodeBufferHandle)
{
    CDKResult result = CDKResultSuccess;

    if ((NULL == hBufferManager) || (NULL == hNodeBufferHandle))
    {
        result = CDKResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupChi, "Invalid input! hBufferManager or hNodeBufferHandle is NULL");
    }
    else
    {
        ImageBufferManager*         pBufferManager       = static_cast<ImageBufferManager*>(hBufferManager);
        ChiNodeBufferHandleWrapper* pBufferHandleWrapper = reinterpret_cast<ChiNodeBufferHandleWrapper*>(hNodeBufferHandle);
        CAMX_ASSERT(CANARY == pBufferHandleWrapper->canary);

        if (0 == pBufferManager->ReleaseReference(pBufferHandleWrapper->pImageBuffer))
        {
            CAMX_LOG_VERBOSE(CamxLogGroupChi, "Freeing wrapper");
            CAMX_FREE(pBufferHandleWrapper);
        }

    }
    return result;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FNGetFlushInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::FNGetFlushInfo(
    CHIHANDLE     hChiSession,
    CHIFLUSHINFO* pFlushInfo)
{
    CDKResult result = CDKResultEFailed;

    if ((NULL != hChiSession) && (NULL != pFlushInfo))
    {
        ChiNodeWrapper* pNode = static_cast<ChiNodeWrapper*>(hChiSession);

        pFlushInfo->lastFlushedRequestId = pNode->GetFlushInfo()->lastFlushRequestId[pNode->GetPipeline()->GetPipelineId()];
        // Temporary Change, modify after rest of flush changes gets merged
        pFlushInfo->flushInProgress = FALSE; // pNode->GetPipeline()->GetFlushStatus();

        result = CDKResultSuccess;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::GetMulticamDynamicMetaByCamId
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::GetMulticamDynamicMetaByCamId(
    CHIMETADATAINFO* pMetadataInfo,
    UINT32           cameraId)
{
    return GetDynamicMetadata(pMetadataInfo, cameraId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::GetDynamicMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::GetDynamicMetadata(
    CHIMETADATAINFO* pMetadataInfo,
    UINT32           cameraId)
{
    CDKResult result = CDKResultSuccess;
    CAMX_ASSERT(NULL != pMetadataInfo);

    if (pMetadataInfo->metadataType == ChiMetadataDynamic)
    {
        UINT64* pOffset = static_cast<UINT64 *>(CAMX_CALLOC(
                sizeof(UINT64) * pMetadataInfo->tagNum));
        VOID** ppData = static_cast<VOID **>(CAMX_CALLOC(
                sizeof(VOID *) * pMetadataInfo->tagNum));
        BOOL* pNegate = static_cast<BOOL *>(CAMX_CALLOC(
                sizeof(BOOL) * pMetadataInfo->tagNum));

        if (NULL == pOffset || NULL == ppData || NULL == pNegate)
        {
            result = CDKResultENoMemory;
        }

        if (CDKResultSuccess == result)
        {
            for (UINT32 i = 0; i < pMetadataInfo->tagNum; i++)
            {
                INT64 requestTemp =
                        static_cast<INT64>(GetCurrentRequest())
                                - static_cast<INT64>(pMetadataInfo->pTagData[i].requestId);

                if (pMetadataInfo->pTagData[i].requestId
                        <= GetCurrentRequest())
                {
                    pOffset[i] = static_cast<UINT64>(requestTemp);
                    pNegate[i] = FALSE;
                }
                else
                {
                    pOffset[i] = static_cast<UINT64>(-1 * requestTemp);
                    pNegate[i] = TRUE;
                }
            }

            GetDataListFromPipelineByCameraId(pMetadataInfo->pTagList, ppData, pOffset,
                    pMetadataInfo->tagNum, pNegate,
                    GetPipeline()->GetPipelineId(),
                    cameraId);

            for (UINT32 i = 0; i < pMetadataInfo->tagNum; i++)
            {
                pMetadataInfo->pTagData[i].pData = ppData[i];
            }
        }

        if (NULL != ppData)
        {
            CAMX_FREE(ppData);
            ppData = NULL;
        }

        if (NULL != pOffset)
        {
            CAMX_FREE(pOffset);
            pOffset = NULL;
        }

        if (NULL != pNegate)
        {
            CAMX_FREE(pNegate);
            pNegate = NULL;
        }
    } else
    {
        CAMX_LOG_ERROR(CamxLogGroupChi, "Unsupported metadatatype %d", pMetadataInfo->metadataType);
        result = CDKResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::GetMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::GetMetadata(
    CHIMETADATAINFO* pMetadataInfo)
{
    CDKResult result  = CDKResultSuccess;
    CAMX_ASSERT(NULL != pMetadataInfo);

    if (pMetadataInfo->metadataType == ChiMetadataDynamic)
    {
        result = GetDynamicMetadata(pMetadataInfo, InvalidCameraId);
    }
    else if (pMetadataInfo->metadataType == ChiMetadataStatic)
    {
        UINT32  tagNum = pMetadataInfo->tagNum;
        for (UINT32 i = 0; i < tagNum; i++)
        {
            CHITAGDATA* pTagData = &pMetadataInfo->pTagData[i];
            HAL3MetadataUtil::GetMetadata(const_cast<Metadata *>(m_pStaticMetadata),
                                          pMetadataInfo->pTagList[i],
                                          &pTagData->pData);
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupChi, "Unsupported metadatatype %d", pMetadataInfo->metadataType);
        result = CDKResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::SetMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::SetMetadata(
    CHIMETADATAINFO* pMetadataInfo)
{
    UINT32 tagNum       = pMetadataInfo->tagNum;
    CDKResult result    = CDKResultSuccess;

    UINT*         pDataSize = static_cast<UINT *>(CAMX_CALLOC(sizeof(UINT) * tagNum));
    const VOID**  ppData    = static_cast<const VOID **>(CAMX_CALLOC(sizeof(VOID *) * tagNum));

    if (NULL == pDataSize|| NULL == ppData)
    {
        result = CDKResultENoMemory;
    }

    if (CDKResultSuccess == result)
    {
        for (UINT32 i = 0; i < pMetadataInfo->tagNum; i++)
        {
            ppData[i]    = pMetadataInfo->pTagData[i].pData;
            pDataSize[i] = static_cast<UINT>(pMetadataInfo->pTagData[i].dataSize);
        }

        WriteDataList(pMetadataInfo->pTagList, ppData, pDataSize, tagNum);
    }

    if (NULL != ppData)
    {
        CAMX_FREE(ppData);
        ppData = NULL;
    }

    if (NULL != pDataSize)
    {
        CAMX_FREE(pDataSize);
        pDataSize = NULL;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::GetVendorTagBase
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::GetVendorTagBase(
    CHIVENDORTAGBASEINFO* pVendorTagBaseInfo)
{
    CAMX_UNREFERENCED_PARAM(pVendorTagBaseInfo);
    return CDKResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ProcRequestDone
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::ProcRequestDone(
    CHINODEPROCESSREQUESTDONEINFO* pInfo)
{
    CAMX_UNREFERENCED_PARAM(pInfo);

    UINT32 requestIdIndex = (pInfo->frameNum % MaxRequestQueueDepth);

    for (UINT i = 0; i < m_perRequestData[requestIdIndex].numFences; i++)
    {
        if (NULL != m_perRequestData[requestIdIndex].phFence[i])
        {
            CSLFenceSignal(*m_perRequestData[requestIdIndex].phFence[i], CSLFenceResultSuccess);
        }
    }

    if (FALSE == pInfo->isEarlyMetadataDone)
    {
        ProcessPartialMetadataDone(pInfo->frameNum);
        ProcessMetadataDone(pInfo->frameNum);
    }

    if (TRUE == IsSinkNoBufferNode())
    {
        ProcessRequestIdDone(pInfo->frameNum);
    }

    return CDKResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ProcMetadataDone
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::ProcMetadataDone(
    CHINODEPROCESSMETADATADONEINFO* pInfo)
{
    ProcessPartialMetadataDone(pInfo->frameNum);
    ProcessMetadataDone(pInfo->frameNum);
    return CDKResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ProcessingNodeFinalizeInitialization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::ProcessingNodeFinalizeInitialization(
    FinalizeInitializationData*  pFinalizeInitializationData)
{
    CAMX_UNREFERENCED_PARAM(pFinalizeInitializationData);

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::GetChiNodeCapsMask
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiNodeWrapper::GetChiNodeCapsMask(
    const NodeCreateInputData*  pCreateInputData,
    CHINODECREATEINFO*          pNodeCreateInfo)
{
    UINT32 propertyCount = pCreateInputData->pNodeInfo->nodePropertyCount;
    CDKResult result    = CDKResultSuccess;

    if (0 == OsUtils::StrCmp(m_pComponentName, "com.qti.node.gpu"))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupChi, "nodePropertyCount %d", pCreateInputData->pNodeInfo->nodePropertyCount);

        for (UINT32 count = 0; count < propertyCount; count++)
        {
            if (NodePropertyGPUCapsMaskType == pCreateInputData->pNodeInfo->pNodeProperties[count].id)
            {
                if (NULL != pCreateInputData->pNodeInfo->pNodeProperties[count].pValue)
                {
                    m_instancePropertyCapsMask = static_cast<UINT>(
                                                 atoi(static_cast<const CHAR*>(
                                                 pCreateInputData->pNodeInfo->pNodeProperties[count].pValue)));

                    CAMX_LOG_VERBOSE(CamxLogGroupChi, "m_instancePropertyCapsMask 0x%x", m_instancePropertyCapsMask);
                    switch (m_instancePropertyCapsMask)
                    {
                        case ChiNodeCapsScale:
                        case ChiNodeCapsGpuMemcpy:
                        case ChiNodeCapsGPUGrayscale:
                        case ChiNodeCapsGPURotate:
                        case ChiNodeCapsGPUDownscale:
                        case ChiNodeCapsGPUFlip:
                        case ChiNodeCapsGPUSkipProcessing:
                        case ChiNodeCapsGPUEnableMapping:
                            pNodeCreateInfo->nodeCaps.nodeCapsMask += m_instancePropertyCapsMask;
                            break;
                        default:
                            pNodeCreateInfo->nodeCaps.nodeCapsMask += ChiNodeCapsGpuMemcpy;
                            break;
                    }
                }
                else
                {
                    result = CamxResultEInvalidPointer;
                    CAMX_LOG_ERROR(CamxLogGroupChi, "NodeProperties pValue is NUll");
                }
            }
        }
        CAMX_LOG_INFO(CamxLogGroupCore, "GPU node capsMask is 0x%x", pNodeCreateInfo->nodeCaps.nodeCapsMask);
    }
    if (0 == OsUtils::StrCmp(m_pComponentName, "com.qti.node.dewarp"))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupChi, "nodePropertyCount %d", pCreateInputData->pNodeInfo->nodePropertyCount);

        for (UINT32 count = 0; count < propertyCount; count++)
        {
            if (NodePropertyGPUCapsMaskType == pCreateInputData->pNodeInfo->pNodeProperties[count].id)
            {
                if (NULL != pCreateInputData->pNodeInfo->pNodeProperties[count].pValue)
                {
                    m_instancePropertyCapsMask = static_cast<UINT>(
                        atoi(static_cast<const CHAR*>(
                            pCreateInputData->pNodeInfo->pNodeProperties[count].pValue)));
                }
                else
                {
                    result = CamxResultEInvalidPointer;
                    CAMX_LOG_ERROR(CamxLogGroupChi, "NodeProperties pValue is NUll");
                }
            }
        }

        switch (m_instancePropertyCapsMask)
        {
            case ChiNodeCapsDewarpEISV2:
            case ChiNodeCapsDewarpEISV3:
                pNodeCreateInfo->nodeCaps.nodeCapsMask = m_instancePropertyCapsMask;
                break;
            default:
                pNodeCreateInfo->nodeCaps.nodeCapsMask = ChiNodeCapsDewarpEISV2;
                break;
        }
        CAMX_LOG_INFO(CamxLogGroupCore, "Dewarp node capsMask is 0x%x", pNodeCreateInfo->nodeCaps.nodeCapsMask);
    }

    if (0 == OsUtils::StrCmp(m_pComponentName, "com.qti.node.fcv"))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupChi, "nodePropertyCount %d", pCreateInputData->pNodeInfo->nodePropertyCount);

        for (UINT32 count = 0; count < propertyCount; count++)
        {
            if (NodePropertyGPUCapsMaskType == pCreateInputData->pNodeInfo->pNodeProperties[count].id)
            {
                if (NULL != pCreateInputData->pNodeInfo->pNodeProperties[count].pValue)
                {
                    m_instancePropertyCapsMask = static_cast<UINT>(
                        atoi(static_cast<const CHAR*>(
                            pCreateInputData->pNodeInfo->pNodeProperties[count].pValue)));
                }
                else
                {
                    result = CamxResultEInvalidPointer;
                    CAMX_LOG_ERROR(CamxLogGroupChi, "NodeProperties pValue is NUll");
                }
            }
        }

        pNodeCreateInfo->nodeCaps.nodeCapsMask = m_instancePropertyCapsMask;
        CAMX_LOG_INFO(CamxLogGroupCore, "FCV node capsMask is 0x%x", pNodeCreateInfo->nodeCaps.nodeCapsMask);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ProcessingNodeInitialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::ProcessingNodeInitialize(
    const NodeCreateInputData*   pCreateInputData,
    NodeCreateOutputData*        pCreateOutputData)
{
    CamxResult          result         = CamxResultSuccess;
    CHINODECREATEINFO   nodeCreateInfo = {0};

    ChiCameraInfo cameraInfo           = {0};
    CameraInfo    legacyCameraInfo;

    cameraInfo.pLegacy  = static_cast<VOID*>(&legacyCameraInfo);
    result              = m_pChiContext->GetCameraInfo(GetPipeline()->GetCameraId(), &cameraInfo);
    if (CamxResultSuccess == result)
    {
        m_pStaticMetadata = legacyCameraInfo.pStaticCameraInfo;
        CAMX_LOG_INFO(CamxLogGroupCore,
                      "device version %d static info %p",
                      legacyCameraInfo.deviceVersion,
                      legacyCameraInfo.pStaticCameraInfo);
        CAMX_ASSERT(NULL != m_pStaticMetadata);
    }

    if (pCreateInputData->pNodeInfo->nodePropertyCount > 0)
    {
        // Expecting the component name to always be the first property with an id equal to 1.
        const CHAR* pComponentName = static_cast<CHAR*>(pCreateInputData->pNodeInfo->pNodeProperties[0].pValue);
        CAMX_ASSERT(pCreateInputData->pNodeInfo->pNodeProperties[0].id == NodePropertyCustomLib);

        SIZE_T componentNameLen = OsUtils::StrLen(pComponentName) + 1;
        m_pComponentName = static_cast<CHAR*>(CAMX_CALLOC(componentNameLen));

        if (NULL != m_pComponentName)
        {
            OsUtils::StrLCpy(m_pComponentName, pComponentName, componentNameLen);
        }
        else
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS();
        }
    }

    if (CamxResultSuccess == result)
    {
        ChiNodeInterface nodeInterface;

        nodeInterface.size                           = sizeof(ChiNodeInterface);
        nodeInterface.majorVersion                   = 0;
        nodeInterface.minorVersion                   = 0;
        nodeInterface.pGetMetadata                   = FNGetMetadata;
        nodeInterface.pGetMultiCamDynamicMetaByCamId = FNGetMultiCamDynamicMetaByCamId;
        nodeInterface.pSetMetadata                   = FNSetMetadata;
        nodeInterface.pGetVendorTagBase              = FNGetVendorTagBase;
        nodeInterface.pProcessRequestDone            = FNProcRequestDone;
        nodeInterface.pCreateFence                   = FNCreateFence;
        nodeInterface.pReleaseFence                  = FNReleaseFence;
        nodeInterface.pWaitFenceAsync                = FNWaitFenceAsync;
        nodeInterface.pSignalFence                   = FNSignalFence;
        nodeInterface.pGetFenceStatus                = FNGetFenceStatus;
        nodeInterface.pGetDataSource                 = FNGetDataSource;
        nodeInterface.pPutDataSource                 = FNPutDataSource;
        nodeInterface.pGetData                       = FNGetData;
        nodeInterface.pPutData                       = FNPutData;
        nodeInterface.pCacheOps                      = FNCacheOps;
        nodeInterface.pProcessMetadataDone           = FNProcMetadataDone;
        nodeInterface.pGetFlushInfo                  = FNGetFlushInfo;
        nodeInterface.pCreateBufferManager           = FNBufferManagerCreate;
        nodeInterface.pDestroyBufferManager          = FNBufferManagerDestroy;
        nodeInterface.pBufferManagerGetImageBuffer   = FNBufferManagerGetImageBuffer;
        nodeInterface.pBufferManagerReleaseReference = FNBufferManagerReleaseReference;

        m_nodeCallbacks.pChiNodeSetNodeInterface(&nodeInterface);
    }

    if ((NULL != m_nodeCallbacks.pCreate) && (CamxResultSuccess == result))
    {
        nodeCreateInfo.size                   = sizeof(nodeCreateInfo);
        nodeCreateInfo.hChiSession            = static_cast<CHIHANDLE>(this);
        nodeCreateInfo.phNodeSession          = NULL;
        nodeCreateInfo.nodeId                 = pCreateInputData->pNodeInfo->nodeId;
        nodeCreateInfo.nodeInstanceId         = pCreateInputData->pNodeInfo->instanceId;
        nodeCreateInfo.nodeFlags.isRealTime   = IsRealTime();
        nodeCreateInfo.nodeFlags.isBypassable = IsBypassableNode();
        nodeCreateInfo.nodeFlags.isSecureMode = IsSecureMode();
        nodeCreateInfo.chiTitanVersion =
            static_cast<ChiTitanChipVersion>(static_cast<Titan17xContext *>(GetHwContext())->GetTitanChipVersion());
        /// @todo (CAMX-1806) fill the node caps
        /// nodeCreateInfo.nodeCaps = xx;

        GetChiNodeCapsMask(pCreateInputData, &nodeCreateInfo);

        nodeCreateInfo.chiICAVersion =
            ((CSLCameraTitanVersion::CSLTitan175 == static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion()) ||
             (CSLCameraTitanVersion::CSLTitan160 == static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion())) ?
            ChiICAVersion::ChiICA20 : ChiICAVersion::ChiICA10;


        if (CSLCameraTitanVersion::CSLTitan150 == static_cast<Titan17xContext *>(GetHwContext())->GetTitanVersion())
        {
            // As Talos ICAVersion is 10. However, ICA is not used for Dewarping. Hence, setting at max/invalid.
            nodeCreateInfo.chiICAVersion = ChiICAVersion::ChiICAMax;
        }

        // Pass node properties to chi node
        CAMX_STATIC_ASSERT(sizeof(PerNodeProperty) == sizeof(CHINODEPROPERTY));
        nodeCreateInfo.nodePropertyCount = pCreateInputData->pNodeInfo->nodePropertyCount;
        nodeCreateInfo.pNodeProperties   = reinterpret_cast<CHINODEPROPERTY*>(pCreateInputData->pNodeInfo->pNodeProperties);

        CDKResult cdkResult = m_nodeCallbacks.pCreate(&nodeCreateInfo);
        result              = CDKResultToCamxResult(cdkResult);
    }

    if (CamxResultSuccess == result)
    {
        pCreateOutputData->createFlags.canDRQPreemptOnStopRecording = nodeCreateInfo.nodeFlags.canDRQPreemptOnStopRecording;
        m_canNodeSetBufferDependency                                = nodeCreateInfo.nodeFlags.canSetInputBufferDependency;
        m_hNodeSession                                              = nodeCreateInfo.phNodeSession;

        if (NULL != m_nodeCallbacks.pGetCapabilities)
        {
            m_nodeCapsMask.size = sizeof(CHINODECAPSINFO);
            m_nodeCallbacks.pGetCapabilities(&m_nodeCapsMask);
            if ( (0 == OsUtils::StrCmp(m_pComponentName, "com.qti.node.gpu")) ||
                (0 == OsUtils::StrCmp(m_pComponentName, "com.qti.node.dewarp")))
            {
                m_nodeCapsMask.nodeCapsMask = nodeCreateInfo.nodeCaps.nodeCapsMask;
            }
            if (m_nodeCapsMask.nodeCapsMask & ChiNodeCapsParallelReq)
            {
                m_parallelProcessRequests = TRUE;
                CAMX_LOG_INFO(CamxLogGroupChi, "Node supports parallel process request\n");
            }
        }
    }

    UINT32  groupID         = 1;
    UINT    numOutputPorts  = 0;
    UINT    outputPortId[MaxBufferComposite];

    GetAllOutputPortIds(&numOutputPorts, &outputPortId[0]);

    for (UINT outputPortIndex = 0; outputPortIndex < numOutputPorts; outputPortIndex++)
    {
        pCreateOutputData->bufferComposite.portGroupID[outputPortId[outputPortIndex]] = groupID++;
    }

    pCreateOutputData->bufferComposite.hasCompositeMask = FALSE;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ProcessingNodeFinalizeInputRequirement
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::ProcessingNodeFinalizeInputRequirement(
    BufferNegotiationData* pBufferNegotiationData)
{
    CAMX_ASSERT(NULL != pBufferNegotiationData);

    CamxResult                   result                                                  = CamxResultSuccess;
    OutputPortNegotiationData*   pOutputPortNegotiationData                              = NULL;
    ChiNodeQueryBufferInfo       chiNodeQueryBufferInfo                                  = {0};
    UINT32                       totalInputPorts                                         = 0;
    UINT32                       totalOutputPorts                                        = 0;
    UINT32                       inputPortId[MaximumChiNodeWrapperInputPorts]            = {};
    ChiInputPortQueryBufferInfo  nodeInputPortOptions[MaximumChiNodeWrapperInputPorts]   = {};
    ChiOutputPortQueryBufferInfo outputPortInfo[MaximumChiNodeWrapperOutputPorts]        = {};
    ChiNodeBufferRequirement     outputPortRequirement[MaximumChiNodeWrapperOutputPorts][MaximumChiNodeWrapperInputPorts] = {};
    AlignmentInfo                alignmentLCM[FormatsMaxPlanes]                          = { {0} };
    const ImageFormat*           pFormat                                                 = NULL;

    // Get Input Port List
    GetAllInputPortIds(&totalInputPorts, &inputPortId[0]);
    totalOutputPorts = pBufferNegotiationData->numOutputPortsNotified;

    chiNodeQueryBufferInfo.size                 = sizeof(CHINODEQUERYBUFFERINFO);
    chiNodeQueryBufferInfo.hNodeSession         = m_hNodeSession;
    chiNodeQueryBufferInfo.numOutputPorts       = totalOutputPorts;
    chiNodeQueryBufferInfo.numInputPorts        = totalInputPorts;
    chiNodeQueryBufferInfo.pInputOptions        = &nodeInputPortOptions[0];
    chiNodeQueryBufferInfo.pOutputPortQueryInfo = &outputPortInfo[0];

    for (UINT outputIndex = 0; outputIndex < totalOutputPorts && outputIndex < MaximumChiNodeWrapperOutputPorts; outputIndex++)
    {
        pOutputPortNegotiationData = &pBufferNegotiationData->pOutputPortNegotiationData[outputIndex];

        chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].outputPortId           =
            GetOutputPortId(pOutputPortNegotiationData->outputPortIndex);
        chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].numConnectedInputPorts =
            pOutputPortNegotiationData->numInputPortsNotification;
        chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].pBufferRequirement     =
            &outputPortRequirement[outputIndex][0];

        pFormat = GetOutputPortImageFormat(outputIndex);
        if (NULL != pFormat)
        {
            chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].outputBufferOption.format =
                static_cast<ChiFormat>(pFormat->format);
        }
        else
        {
            chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].outputBufferOption.format =
                ChiFormat::YUV420NV21; ///< NV21 as default format
        }

        for (UINT inputIndex = 0; inputIndex < pOutputPortNegotiationData->numInputPortsNotification; inputIndex++)
        {
            chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].pBufferRequirement[inputIndex].minW     =
                pOutputPortNegotiationData->inputPortRequirement[inputIndex].minWidth;
            chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].pBufferRequirement[inputIndex].minH     =
                pOutputPortNegotiationData->inputPortRequirement[inputIndex].minHeight;
            chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].pBufferRequirement[inputIndex].maxW     =
                pOutputPortNegotiationData->inputPortRequirement[inputIndex].maxWidth;
            chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].pBufferRequirement[inputIndex].maxH     =
                pOutputPortNegotiationData->inputPortRequirement[inputIndex].maxHeight;
            chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].pBufferRequirement[inputIndex].optimalW =
                pOutputPortNegotiationData->inputPortRequirement[inputIndex].optimalWidth;
            chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].pBufferRequirement[inputIndex].optimalH =
                pOutputPortNegotiationData->inputPortRequirement[inputIndex].optimalHeight;

            for (UINT planeIdx = 0; planeIdx < FormatsMaxPlanes; planeIdx++)
            {
                alignmentLCM[planeIdx].strideAlignment   =
                    Utils::CalculateLCM(static_cast<INT32>(alignmentLCM[planeIdx].strideAlignment),
                                        static_cast<INT32>(pOutputPortNegotiationData->inputPortRequirement[inputIndex]
                                                           .planeAlignment[planeIdx].strideAlignment));
                alignmentLCM[planeIdx].scanlineAlignment =
                    Utils::CalculateLCM(static_cast<INT32>(alignmentLCM[planeIdx].scanlineAlignment),
                                        static_cast<INT32>(pOutputPortNegotiationData->inputPortRequirement[inputIndex]
                                                           .planeAlignment[planeIdx].scanlineAlignment));
            }
        }
    }

    for (UINT inputIndex = 0; inputIndex < chiNodeQueryBufferInfo.numInputPorts; inputIndex++)
    {
        ChiInputPortQueryBufferInfo* pInputOptions = &chiNodeQueryBufferInfo.pInputOptions[inputIndex];

        pInputOptions->inputPortId = inputPortId[inputIndex];

        pFormat = GetInputPortImageFormat(inputIndex);
        if (NULL != pFormat)
        {
            pInputOptions->inputBufferOption.format = static_cast<ChiFormat>(pFormat->format);
        }
        else
        {
            pInputOptions->inputBufferOption.format = ChiFormat::YUV420NV21; ///< NV21 as default format
        }
    }

    // call into the chi node using the QueryBufferInfo to query for the requirement of input buffer
    m_nodeCallbacks.pQueryBufferInfo(&chiNodeQueryBufferInfo);

    // Sink no buffer node doesn't have any buffer to output, so skip the logic
    if (FALSE == IsSinkNoBufferNode())
    {
        for (UINT outputIndex = 0; outputIndex < pBufferNegotiationData->numOutputPortsNotified; outputIndex++)
        {
            ChiNodeBufferRequirement* pOutputOption =
                &chiNodeQueryBufferInfo.pOutputPortQueryInfo[outputIndex].outputBufferOption;

            pOutputPortNegotiationData = &pBufferNegotiationData->pOutputPortNegotiationData[outputIndex];

            // Store the buffer requirements for this output port which will be reused to set, during forward walk.
            // The values stored here could be final output dimensions unless it is overridden by forward walk.
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth  = pOutputOption->optimalW;
            pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight = pOutputOption->optimalH;
            pOutputPortNegotiationData->outputBufferRequirementOptions.minWidth      = pOutputOption->minW;
            pOutputPortNegotiationData->outputBufferRequirementOptions.minHeight     = pOutputOption->minH;
            pOutputPortNegotiationData->outputBufferRequirementOptions.maxWidth      = pOutputOption->maxW;
            pOutputPortNegotiationData->outputBufferRequirementOptions.maxHeight     = pOutputOption->maxH;

            for (UINT planeIdx = 0; planeIdx < FormatsMaxPlanes; planeIdx++)
            {
                alignmentLCM[planeIdx].strideAlignment   =
                Utils::CalculateLCM(static_cast<INT32>(alignmentLCM[planeIdx].strideAlignment),
                                    static_cast<INT32>(pOutputOption->planeAlignment[planeIdx].strideAlignment));
                alignmentLCM[planeIdx].scanlineAlignment =
                Utils::CalculateLCM(static_cast<INT32>(alignmentLCM[planeIdx].scanlineAlignment),
                                    static_cast<INT32>(pOutputOption->planeAlignment[planeIdx].scanlineAlignment));
            }

            CAMX_STATIC_ASSERT(sizeof(AlignmentInfo) == sizeof(ChiAlignmentInfo));
            Utils::Memcpy(&pOutputPortNegotiationData->outputBufferRequirementOptions.planeAlignment[0],
                          &alignmentLCM[0],
                          sizeof(AlignmentInfo) * FormatsMaxPlanes);

            if ((pOutputOption->optimalW == 0) ||
                (pOutputOption->optimalH == 0) ||
                (pOutputOption->minW     == 0) ||
                (pOutputOption->minH     == 0) ||
                (pOutputOption->maxW     == 0) ||
                (pOutputOption->maxH     == 0))
            {
                result = CamxResultEFailed;
                break;
            }
        }
    }

    if (result == CamxResultEFailed)
    {
        CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s ERROR: Buffer Negotiation Failed", NodeIdentifierString());
    }
    else
    {
        pBufferNegotiationData->numInputPorts = totalInputPorts;

        for (UINT input = 0; input < totalInputPorts; input++)
        {
            ChiInputPortQueryBufferInfo* pInput = &chiNodeQueryBufferInfo.pInputOptions[input];

            pBufferNegotiationData->inputBufferOptions[input].nodeId     = Type();
            pBufferNegotiationData->inputBufferOptions[input].instanceId = InstanceID();
            pBufferNegotiationData->inputBufferOptions[input].portId     = pInput->inputPortId;

            BufferRequirement* pInputBufferRequirement = &pBufferNegotiationData->inputBufferOptions[input].bufferRequirement;

            pInputBufferRequirement->optimalWidth  = pInput->inputBufferOption.optimalW;
            pInputBufferRequirement->optimalHeight = pInput->inputBufferOption.optimalH;
            pInputBufferRequirement->minWidth      = pInput->inputBufferOption.minW;
            pInputBufferRequirement->minHeight     = pInput->inputBufferOption.minH;
            pInputBufferRequirement->maxWidth      = pInput->inputBufferOption.maxW;
            pInputBufferRequirement->maxHeight     = pInput->inputBufferOption.maxH;

            for (UINT planeIdx = 0; planeIdx < FormatsMaxPlanes; planeIdx++)
            {
                alignmentLCM[planeIdx].strideAlignment   =
                Utils::CalculateLCM(static_cast<INT32>(alignmentLCM[planeIdx].strideAlignment),
                                    static_cast<INT32>(pInput->inputBufferOption.planeAlignment[planeIdx].strideAlignment));
                alignmentLCM[planeIdx].scanlineAlignment =
                Utils::CalculateLCM(static_cast<INT32>(alignmentLCM[planeIdx].scanlineAlignment),
                                    static_cast<INT32>(pInput->inputBufferOption.planeAlignment[planeIdx].scanlineAlignment));
            }

            CAMX_STATIC_ASSERT(sizeof(AlignmentInfo) == sizeof(ChiAlignmentInfo));
            Utils::Memcpy(&pInputBufferRequirement->planeAlignment[0],
                          &alignmentLCM[0],
                          sizeof(AlignmentInfo) * FormatsMaxPlanes);

            CAMX_LOG_INFO(CamxLogGroupChi,
                          "Buffer Negotiation dims, Port %d Optimal %d x %d, Min %d x %d, Max %d x %d\n",
                          pInput->inputPortId,
                          pInputBufferRequirement->optimalWidth,
                          pInputBufferRequirement->optimalHeight,
                          pInputBufferRequirement->minWidth,
                          pInputBufferRequirement->minHeight,
                          pInputBufferRequirement->maxWidth,
                          pInputBufferRequirement->maxHeight);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::FinalizeBufferProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiNodeWrapper::FinalizeBufferProperties(
    BufferNegotiationData* pBufferNegotiationData)
{
    UINT32                      width                       = 0;
    UINT32                      height                      = 0;

    CAMX_ASSERT(NULL != pBufferNegotiationData);

    for (UINT index = 0; index < pBufferNegotiationData->numOutputPortsNotified; index++)
    {
        OutputPortNegotiationData* pOutputPortNegotiationData   = &pBufferNegotiationData->pOutputPortNegotiationData[index];
        InputPortNegotiationData*  pInputPortNegotiationData    = &pBufferNegotiationData->pInputPortNegotiationData[0];
        BufferProperties*          pFinalOutputBufferProperties = pOutputPortNegotiationData->pFinalOutputBufferProperties;
        CHINODESETBUFFERPROPERTIESINFO  bufferInfo      = {0};
        CHINODEIMAGEFORMAT              bufferFormat    = {0};

        // Option-1: Check the node's capability in ChiNodeWrapper and decide based on the capability whether it can scale.
        // Option-2: Pass all of the parameters from BufferNegotiationData to the ChiNode and let it decide the output.
        // Option-2 needs interface changes, so Option-1 can be considered for Phase-1 and Option-2 is Phase-2.

        CAMX_LOG_INFO(CamxLogGroupChi, "nodeCapsMask : %d, ChiNodeCapsScale:%d",
            m_nodeCapsMask.nodeCapsMask, ChiNodeCapsScale);

        if (m_nodeCapsMask.nodeCapsMask & ChiNodeCapsScale)
        {
            UINT32 outWidth  = pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth;
            UINT32 outHeight = pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight;
            // Ensure output width/height is not greater than input width/height
            width  = (pInputPortNegotiationData->pImageFormat->width < outWidth) ?
                      pInputPortNegotiationData->pImageFormat->width : outWidth;
            height = (pInputPortNegotiationData->pImageFormat->height < outHeight) ?
                      pInputPortNegotiationData->pImageFormat->height : outHeight;
        }
        else if ( (m_nodeCapsMask.nodeCapsMask & ChiNodeCapsGPURotate)      |
                    (m_nodeCapsMask.nodeCapsMask & ChiNodeCapsDewarpEISV3)  |
                    (m_nodeCapsMask.nodeCapsMask & ChiNodeCapsDewarpEISV2)  )
        {
            width  = pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth;
            height = pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight;
        }
        else
        {
            width  = pInputPortNegotiationData->pImageFormat->width;
            height = pInputPortNegotiationData->pImageFormat->height;
        }

        if ((FALSE == IsSinkNoBufferNode()) &&
            (FALSE == IsSinkPortWithBuffer(pOutputPortNegotiationData->outputPortIndex)) &&
            (FALSE == IsNonSinkHALBufferOutput(pOutputPortNegotiationData->outputPortIndex)))
        {
            UINT outputPortId = GetOutputPortId(pOutputPortNegotiationData->outputPortIndex);

            switch (outputPortId)
            {
                case ChiNodeOutputFull:
                    pFinalOutputBufferProperties->imageFormat.width  = width;
                    pFinalOutputBufferProperties->imageFormat.height = height;
                    break;

                // DS port is used for normal downscale where output buffers should be same as made in buffer negotiation
                case ChiNodeOutputDS:
                    pFinalOutputBufferProperties->imageFormat.width =
                        pOutputPortNegotiationData->outputBufferRequirementOptions.optimalWidth;
                    pFinalOutputBufferProperties->imageFormat.height =
                        pOutputPortNegotiationData->outputBufferRequirementOptions.optimalHeight;
                    break;

                case ChiNodeOutputDS4:
                    pFinalOutputBufferProperties->imageFormat.width  =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(width, 4) / 4);
                    pFinalOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(height, 4) / 4);
                    break;

                case ChiNodeOutputDS16:
                    pFinalOutputBufferProperties->imageFormat.width  =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(width, 16) / 16);
                    pFinalOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(height, 16) / 16);
                    break;

                case ChiNodeOutputDS64:
                    pFinalOutputBufferProperties->imageFormat.width  =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(width, 64) / 64);
                    pFinalOutputBufferProperties->imageFormat.height =
                        Utils::EvenCeilingUINT32(Utils::AlignGeneric32(height, 64) / 64);
                    break;

                default:
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s Unsupported port %d", NodeIdentifierString(), outputPortId);
                    break;
            }

            width  = pFinalOutputBufferProperties->imageFormat.width;
            height = pFinalOutputBufferProperties->imageFormat.height;

            Utils::Memcpy(&pFinalOutputBufferProperties->imageFormat.planeAlignment[0],
                          &pOutputPortNegotiationData->outputBufferRequirementOptions.planeAlignment[0],
                          sizeof(AlignmentInfo) * FormatsMaxPlanes);
        }

        bufferInfo.hNodeSession = m_hNodeSession;
        bufferInfo.size         = sizeof(CHINODESETBUFFERPROPERTIESINFO);
        bufferInfo.portId       = GetOutputPortId(pOutputPortNegotiationData->outputPortIndex);
        bufferInfo.pFormat      = &bufferFormat;

        bufferFormat.width      = width;
        bufferFormat.height     = height;

        CAMX_LOG_INFO(CamxLogGroupChi, "index %d portId %d bufferInfo %d x %d",
                      index, bufferInfo.portId, bufferFormat.width, bufferFormat.height);

        if (NULL != m_nodeCallbacks.pSetBufferInfo)
        {
            m_nodeCallbacks.pSetBufferInfo(&bufferInfo);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::PostPipelineCreate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::PostPipelineCreate()
{
    CDKResult cdkResult = CDKResultSuccess;

    if (NULL != m_nodeCallbacks.pPostPipelineCreate)
    {
        cdkResult = m_nodeCallbacks.pPostPipelineCreate(m_hNodeSession);
    }
    else if (NULL != m_nodeCallbacks.pPipelineCreated)
    {
        // This is to maintain backward compatibility for now.
        // pPipelineCreated is deprecated and will be removed soon
        CAMX_LOG_WARN(CamxLogGroupChi, "Node::%s pPipelineCreated is deprecated, use pPostPipelineCreate",
                      NodeIdentifierString());

        m_nodeCallbacks.pPipelineCreated(m_hNodeSession);
    }

    return CDKResultToCamxResult(cdkResult);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ExecuteProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::ExecuteProcessRequest(
    ExecuteProcessRequestData*   pExecuteProcessRequestData)
{

    CAMX_ASSERT(NULL != pExecuteProcessRequestData);
    CAMX_ASSERT(NULL != m_hNodeSession);

    CamxResult                result              = CamxResultSuccess;
    PerRequestActivePorts*    pPerRequestPorts    = pExecuteProcessRequestData->pEnabledPortsInfo;
    CHINODEPROCESSREQUESTINFO info                = {0};
    PerRequestOutputPortInfo* pOutputPort         = NULL;
    NodeProcessRequestData*   pNodeRequestData    = pExecuteProcessRequestData->pNodeProcessRequestData;
    CHIDEPENDENCYINFO         dependency          = {0};
    UINT32                    requestIdIndex      = 0;
    BOOL                      hasBufferDependency = FALSE;

    CSLFence hDependentFence[MaxBufferComposite] = {CSLInvalidHandle};

    info.size           = sizeof(info);
    info.hNodeSession   = m_hNodeSession;
    info.frameNum       = pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest->requestId;
    info.inputNum       = pPerRequestPorts->numInputPorts;
    info.phInputBuffer  = m_phInputBuffer;
    info.outputNum      = pPerRequestPorts->numOutputPorts;
    info.phOutputBuffer = m_phOutputBuffer;
    info.pBypassData    = m_pBypassData;
    info.pDependency    = &dependency;

    dependency.size                 = sizeof(CHIDEPENDENCYINFO);
    dependency.processSequenceId    = pNodeRequestData->processSequenceId;
    dependency.hNodeSession         = m_hNodeSession;

    UINT32 inputIndex  = (info.frameNum *  pPerRequestPorts->numInputPorts) %
                         (MaxRequestQueueDepth * pPerRequestPorts->numInputPorts);
    UINT32 outputIndex = (info.frameNum *  pPerRequestPorts->numOutputPorts) %
                         (MaxOutputBuffers * pPerRequestPorts->numOutputPorts);

    CAMX_ASSERT(m_numInputPort >= pPerRequestPorts->numInputPorts);
    CAMX_ASSERT(m_numOutputPort >= pPerRequestPorts->numOutputPorts);

    requestIdIndex = (pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest->requestId % MaxRequestQueueDepth);

    if (pNodeRequestData->dependencyInfo[0].dependencyFlags.dependencyFlagsMask)
    {
        hasBufferDependency = TRUE;
    }

    // If LateBinding is not enabled, we will have buffer information always, populate buffer information as well to avoid
    // unnecessary dependencies
    if (FALSE == HwEnvironment::GetInstance()->GetStaticSettings()->enableImageBufferLateBinding)
    {
        pNodeRequestData->bindIOBuffers = TRUE;
    }

    if ((0 == dependency.processSequenceId) && (FALSE == hasBufferDependency))
    {
        m_perRequestData[requestIdIndex].numFences = 0;

        if (FALSE == IsSinkNoBufferNode())
        {
            for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
            {
                pOutputPort = &pPerRequestPorts->pOutputPorts[i];

                if ((NULL != pOutputPort) && (NULL != pOutputPort->ppImageBuffer[0]))
                {
                    m_perRequestData[requestIdIndex].phFence[i] = pOutputPort->phFence;
                    m_perRequestData[requestIdIndex].numFences++;

                    ImageBufferToChiBuffer(pOutputPort->ppImageBuffer[0],
                                           m_phOutputBuffer[outputIndex + i],
                                           pNodeRequestData->bindIOBuffers);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s i=%d, Output Port/Image Buffer is Null ",
                                   NodeIdentifierString(), i);
                    result = CamxResultEInvalidArg;
                    break;
                }
            }

            info.phOutputBuffer = &m_phOutputBuffer[outputIndex];
        }

        if (CamxResultSuccess == result)
        {
            info.phInputBuffer = &m_phInputBuffer[inputIndex];

            // First request always waits for input fences
            UINT i = 0;
            for (i = 0; i < pPerRequestPorts->numInputPorts; i++)
            {
                PerRequestInputPortInfo* pInputPort = &pPerRequestPorts->pInputPorts[i];

                if ((NULL != pInputPort) && (NULL != pInputPort->pImageBuffer))
                {
                    ImageBufferToChiBuffer(pInputPort->pImageBuffer,
                                           m_phInputBuffer[inputIndex + i],
                                           pNodeRequestData->bindIOBuffers);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s i=%d Input Port/Image Buffer is Null ",
                                   NodeIdentifierString(), i);
                    result = CamxResultEInvalidArg;
                    break;
                }

                // Irrespective of SourceBuffer input port or not, its better to add input fence dependencies always.
                // For exa, if chi creates fences and attaches to input buffers, there is no gaurantee that input fences are
                // signalled by the time Chi submits the request to this pipeline.
                // Also, with late binding, we anyway have to go back to DRQ to make sure input, output buffers are
                // allocated/bound to corresponding ImageBuffers
                // if (FALSE == IsSourceBufferInputPort(i))
                {
                    if (TRUE == m_canNodeSetBufferDependency)
                    {
                        if (CSLInvalidFence != *pInputPort->phFence)
                        {
                            m_phInputBuffer[inputIndex + i]->pfenceHandle     = pInputPort->phFence;
                            m_phInputBuffer[inputIndex + i]->pIsFenceSignaled = pInputPort->pIsFenceSignaled;
                        }
                        else
                        {
                            m_phInputBuffer[inputIndex + i]->pfenceHandle     = NULL;
                            m_phInputBuffer[inputIndex + i]->pIsFenceSignaled = NULL;
                        }

                        CAMX_LOG_VERBOSE(CamxLogGroupChi, "Node::%s Req[%llu] [%d] Fence=%p(%d) Signalled=%p(%d)",
                                         NodeIdentifierString(), info.frameNum, i,
                                         m_phInputBuffer[inputIndex + i]->pfenceHandle, *pInputPort->phFence,
                                         m_phInputBuffer[inputIndex + i]->pIsFenceSignaled, *pInputPort->pIsFenceSignaled);
                    }
                    else
                    {
                        // Looking at index 0, because we don't have more than one dependency list. Need to update here,
                        // if we plan to change to more than one dependency list.
                        UINT fenceCount = pNodeRequestData->dependencyInfo[0].bufferDependency.fenceCount;

                        pNodeRequestData->dependencyInfo[0].bufferDependency.phFences[fenceCount] =
                            pInputPort->phFence;
                        pNodeRequestData->dependencyInfo[0].bufferDependency.pIsFenceSignaled[fenceCount] =
                            pInputPort->pIsFenceSignaled;

                        // Iterate to check if a particular fence is already added as dependency in case of composite,
                        // and skip adding double dependency
                        for (UINT fenceIndex = 0; fenceIndex <= i; fenceIndex++)
                        {
                            if (hDependentFence[fenceIndex] == *pInputPort->phFence)
                            {
                                CAMX_LOG_VERBOSE(CamxLogGroupStats, "Fence handle %d, already added dependency portIndex %d",
                                               *pInputPort->phFence,
                                               i);
                                break;
                            }
                            else if (hDependentFence[fenceIndex] == CSLInvalidHandle)
                            {
                                hDependentFence[fenceIndex] = *pInputPort->phFence;
                                pNodeRequestData->dependencyInfo[0].bufferDependency.fenceCount++;

                                CAMX_LOG_VERBOSE(CamxLogGroupStats, "Add dependency for Fence handle %d, portIndex %d",
                                               *pInputPort->phFence,
                                               i);

                                break;
                            }
                        }
                    }
                }
            }

            // reset all the temp fence bookkeeper to invalid
            for (UINT fenceIndex = 0; fenceIndex <= i; fenceIndex++)
            {
                hDependentFence[fenceIndex] = CSLInvalidHandle;
            }
        }

        if (pNodeRequestData->dependencyInfo[0].bufferDependency.fenceCount > 0)
        {
            pNodeRequestData->dependencyInfo[0].dependencyFlags.hasInputBuffersReadyDependency    = TRUE;
            pNodeRequestData->dependencyInfo[0].dependencyFlags.hasIOBufferAvailabilityDependency = TRUE;
            pNodeRequestData->dependencyInfo[0].processSequenceId                                 =
                FenceCompleteProcessSequenceId;
            pNodeRequestData->numDependencyLists = 1;

            CAMX_LOG_VERBOSE(CamxLogGroupChi,
                             "Node:%s Added buffer dependencies. reqId = %llu Number of fences = %d",
                             NodeIdentifierString(),
                             pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest->requestId,
                             pNodeRequestData->dependencyInfo[0].bufferDependency.fenceCount);
        }
    }
    else if (FenceCompleteProcessSequenceId == pNodeRequestData->processSequenceId)
    {
        // Internal callback should result in the CHI node getting 0, as the first time it will be invoked
        // Other (than 0 and -2) sequenceIds are passed through
        dependency.processSequenceId = 0;
    }

    if ((0 == pNodeRequestData->numDependencyLists) && (FALSE == hasBufferDependency))
    {
        // If this node requested IOBufferAvailability, base node would have bound buffers to ImageBuffers by now.
        // Populate the Image buffer information to ChiImageList now.
        if (TRUE == pNodeRequestData->bindIOBuffers)
        {
            for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
            {
                pOutputPort = &pPerRequestPorts->pOutputPorts[i];

                if ((NULL != pOutputPort) && (NULL != pOutputPort->ppImageBuffer[0]))
                {
                    ImageBufferToChiBuffer(pOutputPort->ppImageBuffer[0],
                                           m_phOutputBuffer[outputIndex + i],
                                           pNodeRequestData->bindIOBuffers);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s i=%d, Output Port/Image Buffer is Null ",
                                   NodeIdentifierString(), i);
                    result = CamxResultEInvalidArg;
                    break;
                }
            }

            for (UINT i = 0; i < pPerRequestPorts->numInputPorts; i++)
            {
                PerRequestInputPortInfo* pInputPort = &pPerRequestPorts->pInputPorts[i];

                if ((NULL != pInputPort) && (NULL != pInputPort->pImageBuffer))
                {
                    ImageBufferToChiBuffer(pInputPort->pImageBuffer,
                                           m_phInputBuffer[inputIndex + i],
                                           pNodeRequestData->bindIOBuffers);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s i=%d Input Port/Image Buffer is Null ",
                                   NodeIdentifierString(), i);
                    result = CamxResultEInvalidArg;
                    break;
                }
            }
        }

        info.phOutputBuffer = &m_phOutputBuffer[outputIndex];
        info.phInputBuffer  = &m_phInputBuffer[inputIndex];

        for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
        {
            m_phOutputBuffer[outputIndex + i]->portId = pPerRequestPorts->pOutputPorts[i].portId;
        }
        for (UINT i = 0; i < pPerRequestPorts->numInputPorts; i++)
        {
            m_phInputBuffer[inputIndex + i]->portId = pPerRequestPorts->pInputPorts[i].portId;
        }

        CDKResult cdkResult = m_nodeCallbacks.pProcessRequest(&info);
        result              = CDKResultToCamxResult(cdkResult);

        m_perRequestData[requestIdIndex].isDelayedRequestDone = info.isDelayedRequestDone;

        CAMX_LOG_VERBOSE(CamxLogGroupChi, "Node::%s Req[%llu] Dependency count for Properties=%d , for Buffers=%u",
                         NodeIdentifierString(), info.frameNum,
                         info.pDependency->count, info.pDependency->inputBufferFenceCount);

        CAMX_ASSERT_MESSAGE(-1 != info.pDependency->processSequenceId, "-1 is a reserved ProcessSequenceId");
        CAMX_ASSERT_MESSAGE(-2 != info.pDependency->processSequenceId, "-2 is a reserved ProcessSequenceId");

        // Satisfy sequential execution dependency if chi node requests for it,
        // else dependency will be satisfied on request done
        if (TRUE == info.pDependency->satisfySequentialExecutionDependency)
        {
            UINT        tag     = GetNodeCompleteProperty();
            const UINT  one     = 1;
            const VOID* pOne[1] = { &one };
            WriteDataList(&tag, pOne, &one, 1);
        }

        // Set a dependency on the completion of the previous ExecuteProcessRequest() call
        // so that we can guarantee serialization of all ExecuteProcessRequest() calls for this node.
        // Do this when Chi node requests for sequential execution
        if (TRUE == info.pDependency->sequentialExecutionNeeded)
        {
            if (FirstValidRequestId < GetRequestIdOffsetFromLastFlush(info.frameNum))
            {
                info.pDependency->properties[info.pDependency->count] = GetNodeCompleteProperty();
                info.pDependency->offsets[info.pDependency->count]    = 1;
                info.pDependency->count++;
            }
        }

        if (0 < info.pDependency->count)
        {
            UINT count = 0;

            for (UINT i = 0; i < info.pDependency->count; i++)
            {
                UINT32 tag = info.pDependency->properties[i];

                // check if the dependency's tag is for internal result pool
                if ((0 != (tag & DriverInternalGroupMask)) && (tag != GetNodeCompleteProperty()))
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s Only support depending on tag in result pool %x",
                                   NodeIdentifierString(), tag);
                    continue;
                }

                UINT32 propertyID = VendorTagManager::GetMappedPropertyID(tag);
                // if the dependency maps to property, then use the propertyID, otherwise
                // just use the tag id
                pNodeRequestData->dependencyInfo[0].propertyDependency.properties[count]    =
                    propertyID > 0 ? propertyID : tag;
                pNodeRequestData->dependencyInfo[0].propertyDependency.offsets[count]       =
                    info.pDependency->offsets[i];
                pNodeRequestData->dependencyInfo[0].propertyDependency.negate[count]        =
                    info.pDependency->negate[i];

                CAMX_LOG_VERBOSE(CamxLogGroupChi, "Node::%s vendor tag value %x, mapped property value %x, offset %d",
                                 NodeIdentifierString(), tag,
                                 pNodeRequestData->dependencyInfo[0].propertyDependency.properties[i],
                                 info.pDependency->offsets[i]);
                count++;
            }

            if (0 < count)
            {
                // Update dependency request data for topology to consume
                pNodeRequestData->dependencyInfo[0].dependencyFlags.hasPropertyDependency = TRUE;
                pNodeRequestData->numDependencyLists                                      = 1;
                pNodeRequestData->dependencyInfo[0].propertyDependency.count              = count;
                pNodeRequestData->dependencyInfo[0].processSequenceId                     = info.pDependency->processSequenceId;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s All dependencies are invalid", NodeIdentifierString());
            }
        }

        if (0 < info.pDependency->inputBufferFenceCount)
        {
            UINT i = 0;
            for (i = 0; i < info.pDependency->inputBufferFenceCount; i++)
            {
                if ((NULL != info.pDependency->pInputBufferFence[i])            &&
                    (NULL != info.pDependency->pInputBufferFenceIsSignaled[i]))
                {
                    UINT fenceCount = pNodeRequestData->dependencyInfo[0].bufferDependency.fenceCount;

                    pNodeRequestData->dependencyInfo[0].bufferDependency.phFences[fenceCount] =
                        static_cast<CSLFence*>(info.pDependency->pInputBufferFence[i]);
                    pNodeRequestData->dependencyInfo[0].bufferDependency.pIsFenceSignaled[fenceCount] =
                        static_cast<UINT*>(info.pDependency->pInputBufferFenceIsSignaled[i]);

                    CAMX_LOG_VERBOSE(CamxLogGroupChi, "Node::%s Req[%llu] [%d] Fence=%p(%d) Signalled=%p(%d)",
                                     NodeIdentifierString(), info.frameNum, i,
                                     pNodeRequestData->dependencyInfo[0].bufferDependency.phFences[fenceCount],
                                     *(pNodeRequestData->dependencyInfo[0].bufferDependency.phFences[fenceCount]),
                                     pNodeRequestData->dependencyInfo[0].bufferDependency.pIsFenceSignaled[fenceCount],
                                     *(pNodeRequestData->dependencyInfo[0].bufferDependency.pIsFenceSignaled[fenceCount]));

                    // Iterate to check if a particular fence is already added as dependency in case of composite,
                    // and skip adding double dependency
                    for (UINT fenceIndex = 0; fenceIndex <= i; fenceIndex++)
                    {
                        if (hDependentFence[fenceIndex] == *static_cast<CSLFence*>(info.pDependency->pInputBufferFence[i]))
                        {
                            CAMX_LOG_VERBOSE(CamxLogGroupStats, "Fence handle %d, already added dependency portIndex %d",
                                            *static_cast<CSLFence*>(info.pDependency->pInputBufferFence[i]),
                                           i);
                            break;
                        }
                        else if (hDependentFence[fenceIndex] == CSLInvalidHandle)
                        {
                            hDependentFence[fenceIndex] = *static_cast<CSLFence*>(info.pDependency->pInputBufferFence[i]);
                            pNodeRequestData->dependencyInfo[0].bufferDependency.fenceCount++;

                            CAMX_LOG_VERBOSE(CamxLogGroupStats, "Add dependency for Fence handle %d, portIndex %d",
                                            *static_cast<CSLFence*>(info.pDependency->pInputBufferFence[i]),
                                           i);

                            break;
                        }
                    }
                }
            }

            // reset all the temp fence bookkeeper to invalid
            for (UINT fenceIndex = 0; fenceIndex <= i; fenceIndex++)
            {
                hDependentFence[fenceIndex] = CSLInvalidHandle;
            }

            if (pNodeRequestData->dependencyInfo[0].bufferDependency.fenceCount > 0)
            {
                pNodeRequestData->dependencyInfo[0].dependencyFlags.hasInputBuffersReadyDependency = TRUE;
                pNodeRequestData->dependencyInfo[0].processSequenceId = info.pDependency->processSequenceId;
                pNodeRequestData->numDependencyLists = 1;

                CAMX_LOG_VERBOSE(CamxLogGroupChi,
                                 "Node::%s Req[%llu] Added buffer dependencies. reqId = %llu Number of fences = %d",
                                 NodeIdentifierString(), info.frameNum,
                                 pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest->requestId,
                                 pNodeRequestData->dependencyInfo[0].bufferDependency.fenceCount);
            }
        }

        if (0 < info.pDependency->chiFenceCount)
        {
            UINT  fenceCount      = 0;
            VOID* pFenceUserdata  = NULL;

            for (UINT i = 0; i < info.pDependency->chiFenceCount; i++)
            {
                CHIFENCEHANDLE hChiFence = info.pDependency->pChiFences[i];

                // check if the dependency's chi fence is valid
                if (NULL == hChiFence)
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s Only support valid chi fences for dependency",
                                   NodeIdentifierString());
                    continue;
                }

                pNodeRequestData->dependencyInfo[0].chiFenceDependency.pChiFences[i] = static_cast<ChiFence*>(hChiFence);
                // not iterating to check if a particular fence is already added or not, as chi fences are not composited
                fenceCount++;
            }

            if (0 < fenceCount)
            {
                CHIFENCECALLBACKINFO* pFenceCallbackInfo =
                    static_cast<CHIFENCECALLBACKINFO*>(CAMX_CALLOC(sizeof(CHIFENCECALLBACKINFO)));

                if (NULL != pFenceCallbackInfo)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupChi,
                                     "Node::%s depending on chi fence. fenceCount=%d, frameNum=%llu, processSequenceId=%d",
                                     NodeIdentifierString(), fenceCount, info.frameNum, info.pDependency->processSequenceId);

                    pFenceCallbackInfo->frameNum          = info.frameNum;
                    pFenceCallbackInfo->processSequenceId = info.pDependency->processSequenceId;
                    pFenceCallbackInfo->hChiSession       = static_cast<CHIHANDLE>(this);
                    pFenceCallbackInfo->size              = sizeof(CHIFENCECALLBACKINFO);
                    pFenceCallbackInfo->pUserData         = GetChiContext();
                    pFenceUserdata                        = pFenceCallbackInfo;
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s Out of memory", NodeIdentifierString());
                }

                pNodeRequestData->dependencyInfo[0].dependencyFlags.hasFenceDependency   = TRUE;
                pNodeRequestData->numDependencyLists                                     = 1;
                pNodeRequestData->dependencyInfo[0].chiFenceDependency.chiFenceCount     = fenceCount;
                pNodeRequestData->dependencyInfo[0].chiFenceDependency.pUserData         = pFenceUserdata;
                pNodeRequestData->dependencyInfo[0].chiFenceDependency.pChiFenceCallback = ChiFenceDependencyCallback;
                pNodeRequestData->dependencyInfo[0].processSequenceId                    = info.pDependency->processSequenceId;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s All dependencies are invalid!!!", NodeIdentifierString());
            }
        }

        if (TRUE == info.pDependency->hasIOBufferAvailabilityDependency)
        {
            pNodeRequestData->dependencyInfo[0].dependencyFlags.hasIOBufferAvailabilityDependency   = TRUE;
            pNodeRequestData->dependencyInfo[0].processSequenceId                                   =
                info.pDependency->processSequenceId;
            pNodeRequestData->numDependencyLists                                                    = 1;
        }

        if ((0 == info.pDependency->count) &&
            (0 == info.pDependency->chiFenceCount) &&
            (0 == info.pDependency->inputBufferFenceCount))
        {
            if (FALSE == info.isDelayedRequestDone)
            {
                if (FALSE == info.isEarlyMetadataDone)
                {
                    ProcessPartialMetadataDone(info.frameNum);
                    ProcessMetadataDone(info.frameNum);
                }

                if (NULL != info.doneFence)
                {
                    CHIFENCECALLBACKINFO* pFenceCallbackInfo =
                        static_cast<CHIFENCECALLBACKINFO*>(CAMX_CALLOC(sizeof(CHIFENCECALLBACKINFO)));

                    if (NULL != pFenceCallbackInfo)
                    {
                        pFenceCallbackInfo->frameNum    = info.frameNum;
                        pFenceCallbackInfo->hChiSession = static_cast<CHIHANDLE>(this);
                        pFenceCallbackInfo->pUserData   = pPerRequestPorts;
                        pFenceCallbackInfo->size        = sizeof(CHIFENCECALLBACKINFO);
                        GetChiContext()->WaitChiFence(info.doneFence, ChiFenceCallback, pFenceCallbackInfo, UINT64_MAX);
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s Unable to allocate structure, out of memory",
                                       NodeIdentifierString());
                        result = CamxResultENoMemory;
                    }
                }
                else
                {
                    // For Sink nodes signal that request is done.
                    if (TRUE == IsSinkNoBufferNode())
                    {
                        ProcessRequestIdDone(info.frameNum);
                    }

                    for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
                    {
                        pOutputPort = &pPerRequestPorts->pOutputPorts[i];

                        if ((NULL != pOutputPort) && (NULL != pOutputPort->phFence))
                        {
                            CSLFenceSignal(*pOutputPort->phFence, CSLFenceResultSuccess);
                        }
                    }

                    for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
                    {
                        pOutputPort = &pPerRequestPorts->pOutputPorts[i];

                        if ((NULL != pOutputPort) && (TRUE == pOutputPort->flags.isDelayedBuffer))
                        {
                            DelayedOutputBufferInfo* pDelayedBufferData = pOutputPort->pDelayedOutputBufferData;
                            BOOL                     isBypassNeeded     = (TRUE == info.pBypassData[i].isBypassNeeded);
                            CSLFenceResult           fenceResult        = CSLFenceResultSuccess;

                            if (TRUE == isBypassNeeded)
                            {
                                UINT bypassIndex = info.pBypassData[i].selectedInputPortIndex;

                                if (bypassIndex < pPerRequestPorts->numInputPorts)
                                {
                                    // Select input port data
                                    const PerRequestInputPortInfo* pInputInfo = &pPerRequestPorts->pInputPorts[bypassIndex];

                                    pDelayedBufferData->hFence              = *pInputInfo->phFence;
                                    pDelayedBufferData->ppImageBuffer[0]    = pInputInfo->pImageBuffer;
                                    pDelayedBufferData->isParentInputBuffer = TRUE;
                                }
                                else
                                {
                                    isBypassNeeded = FALSE; // Fall back on non-bypass output. Most likely a green frame
                                    result         = CamxResultEOutOfBounds;
                                    fenceResult    = CSLFenceResultFailed;
                                    CAMX_LOG_ERROR(CamxLogGroupChi,
                                                   "[%s] Cannot bypass request: %llu - Input Idx: %u OOB | NumInputs: %u"
                                                   "Output Port Id: %u",
                                                   NodeIdentifierString(), info.frameNum, bypassIndex,
                                                   pPerRequestPorts->numInputPorts, pOutputPort->portId);
                                }
                            }
                            if (FALSE == isBypassNeeded)
                            {
                                // Select output port data
                                if (NULL != pOutputPort->phFence)
                                {
                                    pOutputPort->pDelayedOutputBufferData->hFence = *pOutputPort->phFence;
                                }
                                else
                                {
                                    CAMX_LOG_ERROR(CamxLogGroupChi, "Node::%s pOutputPort->phFence is NULL",
                                                   NodeIdentifierString());
                                }
                                pOutputPort->pDelayedOutputBufferData->ppImageBuffer[0] = pOutputPort->ppImageBuffer[0];
                                pOutputPort->pDelayedOutputBufferData->isParentInputBuffer = FALSE;
                            }

                            CAMX_LOG_VERBOSE(CamxLogGroupChi,
                                "Node::%s Signaled Fence %08x(%08x) Image Buffer %x for request= %llu"
                                " parent buffer = %d input index = %d",
                                NodeIdentifierString(),
                                pOutputPort->phDelayedBufferFence,
                                *pOutputPort->phDelayedBufferFence,
                                pOutputPort->pDelayedOutputBufferData->ppImageBuffer[0],
                                pExecuteProcessRequestData->pNodeProcessRequestData->pCaptureRequest->requestId,
                                pOutputPort->pDelayedOutputBufferData->isParentInputBuffer,
                                info.pBypassData[i].selectedInputPortIndex);

                            CSLFenceSignal(*pOutputPort->phDelayedBufferFence, fenceResult);
                        }
                    }
                }
            }
        }
    }
    // CSL signal fence of outputport
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ChiNodeWrapper
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ChiNodeWrapper::ChiNodeWrapper()
{
    m_pNodeName = "ChiNodeWrapper";
    m_derivedNodeHandlesMetaDone = TRUE;
    m_canNodeSetBufferDependency = FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::~ChiNodeWrapper
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ChiNodeWrapper::~ChiNodeWrapper()
{
    if (NULL != m_phInputBuffer)
    {
        ReleaseChiBufferHandlePool(m_phInputBuffer);
        m_phInputBuffer = NULL;
        m_numInputPort  = 0;
    }

    if (NULL != m_phOutputBuffer)
    {
        ReleaseChiBufferHandlePool(m_phOutputBuffer);
        m_phOutputBuffer = NULL;
        m_numOutputPort  = 0;
    }

    if (NULL != m_pBypassData)
    {
        CAMX_FREE(m_pBypassData);
        m_pBypassData = NULL;
    }

    if (NULL != m_hNodeSession)
    {
        CHINODEDESTROYINFO info;
        info.size           = sizeof(info);
        info.hNodeSession   = m_hNodeSession;
        m_nodeCallbacks.pDestroy(&info);

        m_hNodeSession = NULL;
    }

    if (NULL != m_pComponentName)
    {
        CAMX_FREE(m_pComponentName);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::AllocateChiBufferHandlePool
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CHINODEBUFFERHANDLE* ChiNodeWrapper::AllocateChiBufferHandlePool(
    INT32 size)
{
    ChiImageList*        pImageList      = static_cast<ChiImageList*>(CAMX_CALLOC(sizeof(ChiImageList) * size));
    CHINODEBUFFERHANDLE* phBufferHandle  = static_cast<CHINODEBUFFERHANDLE*>(CAMX_CALLOC(sizeof(CHINODEBUFFERHANDLE) * size));

    if (NULL != pImageList && NULL != phBufferHandle)
    {
        for (INT32 i = 0; i < size; i++)
        {
            phBufferHandle[i]   = &pImageList[i];
        }
    }
    else
    {
        // one of the memory allocation is failed
        if (NULL != pImageList)
        {
            CAMX_FREE(pImageList);
            pImageList = NULL;
        }

        if (NULL != phBufferHandle)
        {
            CAMX_FREE(phBufferHandle);
            phBufferHandle = NULL;
        }
    }

    return phBufferHandle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ReleaseChiBufferHandlePool
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiNodeWrapper::ReleaseChiBufferHandlePool(
    CHINODEBUFFERHANDLE* phBufferHandle)
{
    if (NULL != phBufferHandle)
    {
        ChiImageList* pImageList = static_cast<ChiImageList*>(phBufferHandle[0]);
        if (NULL != pImageList)
        {
            CAMX_FREE(pImageList);
            pImageList = NULL;
        }
        CAMX_FREE(phBufferHandle);
        phBufferHandle = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ImageFormatToChiFormat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiNodeWrapper::ImageFormatToChiFormat(
    const ImageFormat*  pFormat,
    CHIIMAGEFORMAT*     pChiFormat)
{
    if (NULL != pFormat)
    {
        pChiFormat->alignment                           = pFormat->alignment;
        pChiFormat->colorSpace                          = static_cast<ChiColorSpace>(pFormat->colorSpace);
        pChiFormat->height                              = pFormat->height;
        pChiFormat->planeAlignment[0].scanlineAlignment = pFormat->planeAlignment->scanlineAlignment;
        pChiFormat->planeAlignment[0].strideAlignment   = pFormat->planeAlignment->strideAlignment;
        pChiFormat->rotation                            = static_cast<ChiRotation>(pFormat->rotation);
        pChiFormat->width                               = pFormat->width;
        pChiFormat->format                              = static_cast<ChiFormat>(pFormat->format);

        for (UINT32 index = 0; index < ChiNodeFormatsMaxPlanes; index++)
        {
            CHIYUVFORMAT*       pChiYUVFormat   = &pChiFormat->formatParams.yuvFormat[index];
            const YUVFormat*    pCamxYUVFormat  = &pFormat->formatParams.yuvFormat[index];

            pChiYUVFormat->width            = pCamxYUVFormat->width;
            pChiYUVFormat->height           = pCamxYUVFormat->height;
            pChiYUVFormat->planeStride      = pCamxYUVFormat->planeStride;
            pChiYUVFormat->sliceHeight      = pCamxYUVFormat->sliceHeight;
            pChiYUVFormat->metadataStride   = pCamxYUVFormat->metadataStride;
            pChiYUVFormat->metadataHeight   = pCamxYUVFormat->metadataHeight;
            pChiYUVFormat->metadataSize     = pCamxYUVFormat->metadataSize;
            pChiYUVFormat->pixelPlaneSize   = pCamxYUVFormat->pixelPlaneSize;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupChi, "NULL Image format pointer");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ImageBufferToChiBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiNodeWrapper::ImageBufferToChiBuffer(
    const ImageBuffer*  pImageBuffer,
    ChiImageList*       pChilImageList,
    BOOL                bPopulateBufferHandles)
{
    CAMX_ASSERT(pImageBuffer != NULL);

    // @todo (CAMX-1806) check if Chi could use the Camx::ImageFormat directly
    // packing mismatch !!!! Utils::Memcpy(&hHandle->format, pFormat, sizeof(ImageFormat));
    ImageFormatToChiFormat(pImageBuffer->GetFormat(), &pChilImageList->format);

    pChilImageList->imageCount     = pImageBuffer->GetNumFramesInBatch();
    pChilImageList->numberOfPlanes = pImageBuffer->GetNumberOfPlanes();

    CAMX_ASSERT(pChilImageList->numberOfPlanes <= ChiNodeFormatsMaxPlanes);

    for (UINT i = 0; i < pChilImageList->numberOfPlanes; i++)
    {
        CSLMemHandle     hMemHandle     = 0;
        SIZE_T           offset         = 0;
        SIZE_T           metadataSize   = 0;

        if (TRUE == bPopulateBufferHandles)
        {
            // This may return error if this ImageBuffer is not backed up with buffers yet, we can ignore the error as we want
            // to populate with default 0s in that case.
            pImageBuffer->GetPlaneCSLMemHandle(0, i, &hMemHandle, &offset, &metadataSize);
        }

        pChilImageList->metadataSize[i] = metadataSize;
        pChilImageList->handles[i]      = hMemHandle;

        pChilImageList->planeSize[i]    = pImageBuffer->GetPlaneSize(i);
    }

    for (UINT i = 0; i < pChilImageList->imageCount; i++)
    {
        ChiImage* pImage = &pChilImageList->pImageList[i];

        Utils::Memset(pImage, 0x0, sizeof(ChiImage));

        pImage->size = sizeof(ChiImage);

        if (TRUE == bPopulateBufferHandles)
        {
            pImage->pNativeHandle   = pImageBuffer->GetNativeBufferHandle();

            for (UINT j = 0; j < pChilImageList->numberOfPlanes; j++)
            {
                pImage->fd[j]       = pImageBuffer->GetFileDescriptor();
                pImage->pAddr[j]    = pImageBuffer->GetPlaneVirtualAddr(i, j);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::CDKResultToCamxResult
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE CamxResult ChiNodeWrapper::CDKResultToCamxResult(
    CDKResult cdkResult)
{
    CamxResult result = CamxResultSuccess;
    if (CDKResultSuccess != cdkResult)
    {
        result = CamxResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::CamxResultToCDKResult
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE CDKResult ChiNodeWrapper::CamxResultToCDKResult(
    CamxResult camxResult)
{
    CDKResult result = CDKResultSuccess;
    if (CamxResultSuccess != camxResult)
    {
        result = CDKResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ChiFenceCallback
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiNodeWrapper::ChiFenceCallback(
    CHIFENCEHANDLE  hChiFence,
    VOID*           pUserData)
{
    CAMX_UNREFERENCED_PARAM(hChiFence);

    PerRequestOutputPortInfo* pOutputPort      = NULL;
    CHIFENCECALLBACKINFO*     pCallbackInfo    = reinterpret_cast<CHIFENCECALLBACKINFO*>(pUserData);

    CAMX_ASSERT(NULL != pCallbackInfo);
    CAMX_ASSERT(sizeof(CHIFENCECALLBACKINFO) == (pCallbackInfo->size));

    PerRequestActivePorts*    pPerRequestPorts = reinterpret_cast<PerRequestActivePorts*>(pCallbackInfo->pUserData);
    ChiNodeWrapper*           pNode            = static_cast<ChiNodeWrapper*>(pCallbackInfo->hChiSession);

    pNode->GetChiContext()->ReleaseChiFence(hChiFence);
    hChiFence = NULL;

    CAMX_ASSERT(NULL != pPerRequestPorts);

    // For Sink nodes signal that request is done.
    if (TRUE == pNode->IsSinkNoBufferNode())
    {
        pNode->ProcessRequestIdDone(pCallbackInfo->frameNum);
    }

    for (UINT i = 0; i < pPerRequestPorts->numOutputPorts; i++)
    {
        pOutputPort = &pPerRequestPorts->pOutputPorts[i];
        if ((NULL != pOutputPort) && (NULL != pOutputPort->phFence))
        {
            CSLFenceSignal(*pOutputPort->phFence, CSLFenceResultSuccess);
        }
    }

    CAMX_FREE(pCallbackInfo);
    pCallbackInfo = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::ChiFenceDependencyCallback
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiNodeWrapper::ChiFenceDependencyCallback(
    CHIFENCEHANDLE  hChiFence,
    VOID*           pUserData)
{
    CHIFENCECALLBACKINFO* pCallbackInfo = reinterpret_cast<CHIFENCECALLBACKINFO*>(pUserData);

    CAMX_ASSERT(NULL != pCallbackInfo);
    CAMX_ASSERT(sizeof(CHIFENCECALLBACKINFO) == (pCallbackInfo->size));

    ChiContext* pContext = static_cast<ChiContext*>(pCallbackInfo->pUserData);

    pContext->ReleaseChiFence(hChiFence);
    hChiFence = NULL;

    CAMX_FREE(pCallbackInfo);
    pCallbackInfo = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::SetupNCSLink
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
NCSSensor* ChiNodeWrapper::SetupNCSLink(
    NCSSensorConfig* pSensorConfig)
{
    CamxResult      result            = CamxResultEFailed;
    NCSService*     pNCSServiceObject = NULL;
    NCSSensorConfig sensorConfig      = { 0 };
    NCSSensorCaps   sensorCapsGyro    = {};
    NCSSensor*      pSensorObject     = NULL;

    CAMX_LOG_VERBOSE(CamxLogGroupChi, "Setting up an NCS link");
    if (NULL != pSensorConfig)
    {
        // Get the NCS service object handle
        pNCSServiceObject = reinterpret_cast<NCSService *>(HwEnvironment::GetInstance()->GetNCSObject());
        if (NULL != pNCSServiceObject)
        {
            result = pNCSServiceObject->QueryCapabilites(&sensorCapsGyro, pSensorConfig->sensorType);
            if (result == CamxResultSuccess)
            {
                // Clients responsibility to set it to that config which is supported
                sensorConfig.samplingRate  = pSensorConfig->samplingRate;   // in Hertz
                sensorConfig.reportRate    = pSensorConfig->reportRate;     // in micorsecs
                sensorConfig.operationMode = pSensorConfig->operationMode;  // Batched reporting mode
                sensorConfig.sensorType    = pSensorConfig->sensorType;
                pSensorObject = pNCSServiceObject->RegisterService(NCSGyroType, &sensorConfig);
                if (NULL == pSensorObject)
                {
                    CAMX_LOG_ERROR(CamxLogGroupChi, "Unable to register with the NCS !!");
                    result = CamxResultEFailed;
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupChi, "Unable to Query caps error %s", Utils::CamxResultToString(result));
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupChi, "NULL ncs input config pointer");
        result = CamxResultEInvalidPointer;
    }

    return pSensorObject;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::DestroyNCSLink
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::DestroyNCSLink(
    VOID* pSensorObj)
{
    CamxResult      result            = CamxResultEFailed;
    NCSSensor*      pSensorObject     = NULL;
    NCSService*     pNCSServiceObject = NULL;

    CAMX_LOG_VERBOSE(CamxLogGroupChi, "Setting up an NCS link");
    if (NULL != pSensorObj)
    {
        pSensorObject = static_cast<NCSSensor*>(pSensorObj);

        // Get the NCS service object handle
        pNCSServiceObject = reinterpret_cast<NCSService *>(HwEnvironment::GetInstance()->GetNCSObject());
        if (NULL != pNCSServiceObject)
        {
            result = pNCSServiceObject->UnregisterService(pSensorObject);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupChi, "Unregister %s", Utils::CamxResultToString(result));
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupChi, "Invalid service object %s", Utils::CamxResultToString(result));
        }

    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupChi, "NULL Sensor object handle");
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ChiNodeWrapper::PrepareStreamOn
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::PrepareStreamOn()
{
    CamxResult result = CamxResultSuccess;

    CAMX_LOG_VERBOSE(CamxLogGroupChi, "Node::%s Comp::%s Prepare stream on", NodeIdentifierString(), ComponentName());

    if (NULL != m_nodeCallbacks.pPrepareStreamOn)
    {
        CHINODEPREPARESTREAMONINFO info = { 0 };
        info.size                       = sizeof(info);
        info.hNodeSession               = m_hNodeSession;

        result = CDKResultToCamxResult(m_nodeCallbacks.pPrepareStreamOn(&info));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ChiNodeWrapper::OnStreamOn
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::OnStreamOn()
{
    CamxResult result = CamxResultSuccess;

    CAMX_LOG_VERBOSE(CamxLogGroupChi, "Node::%s Comp::%s On stream on", NodeIdentifierString(), ComponentName());

    if (NULL != m_nodeCallbacks.pOnStreamOn)
    {
        CHINODEONSTREAMONINFO info = { 0 };
        info.size                  = sizeof(info);
        info.hNodeSession          = m_hNodeSession;

        result = CDKResultToCamxResult(m_nodeCallbacks.pOnStreamOn(&info));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ChiNodeWrapper::OnStreamOff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::OnStreamOff(
    CHIDEACTIVATEPIPELINEMODE modeBitmask)
{
    CamxResult result = CamxResultSuccess;

    CAMX_UNREFERENCED_PARAM(modeBitmask);

    CAMX_LOG_VERBOSE(CamxLogGroupChi, "Node::%s Comp::%s On stream off", NodeIdentifierString(), ComponentName());

    if (NULL != m_nodeCallbacks.pOnStreamOff)
    {
        CHINODEONSTREAMOFFINFO info = { 0 };
        info.size                   = sizeof(info);
        info.hNodeSession           = m_hNodeSession;

        result = CDKResultToCamxResult(m_nodeCallbacks.pOnStreamOff(&info));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ChiNodeWrapper::QueryMetadataPublishList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::QueryMetadataPublishList(
    NodeMetadataList* pPublistTagList)
{
    CamxResult result = CamxResultSuccess;
    if (NULL == pPublistTagList)
    {
        CAMX_LOG_ERROR(CamxLogGroupMeta, "pPublistTagList NULL");
        result = CamxResultEInvalidArg;
    }
    else if (NULL == m_nodeCallbacks.pQueryMetadataPublishList)
    {
        pPublistTagList->tagCount = 0;
        CAMX_LOG_WARN(CamxLogGroupMeta, "Node::%s Comp::%s Publish list is not implemented",
                      NodeIdentifierString(), ComponentName());
    }
    else
    {
        ChiNodeMetadataList chiMetadataList;

        chiMetadataList.hNodeSession = m_hNodeSession;
        chiMetadataList.size         = sizeof(ChiNodeMetadataList);

        result = m_nodeCallbacks.pQueryMetadataPublishList(&chiMetadataList);

        if ((CamxResultSuccess == result) && (0 < chiMetadataList.tagCount))
        {
            pPublistTagList->tagCount        = chiMetadataList.tagCount;
            pPublistTagList->partialTagCount = chiMetadataList.partialTagCount;
            Utils::Memcpy(pPublistTagList->tagArray, chiMetadataList.tagArray,
                          chiMetadataList.tagCount * sizeof(UINT32));
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::CancelRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult ChiNodeWrapper::CancelRequest(
    UINT64 requestId)
{
    CamxResult result = CamxResultEFailed;

    if (NULL != m_nodeCallbacks.pFlushRequest)
    {
        CHINODEFLUSHREQUESTINFO requestInfo;

        requestInfo.size         = sizeof(CHINODEFLUSHREQUESTINFO);
        requestInfo.hNodeSession = m_hNodeSession;
        requestInfo.frameNum     = requestId;
        result                   = m_nodeCallbacks.pFlushRequest(&requestInfo);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeWrapper::GetFlushResponseTimeInMs
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT64 ChiNodeWrapper::GetFlushResponseTimeInMs()
{
    UINT64 responseTimeInMillisec = 0;

    if (NULL != m_nodeCallbacks.pGetFlushResponse)
    {
        CHINODERESPONSEINFO nodeResponseInfo;

        nodeResponseInfo.hChiSession = m_hNodeSession;
        nodeResponseInfo.size        = sizeof(CHINODERESPONSEINFO);

        m_nodeCallbacks.pGetFlushResponse(&nodeResponseInfo);
        responseTimeInMillisec = nodeResponseInfo.responseTimeInMillisec;
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupChi, "Get Flush Response Time not implemented for %s!", NodeIdentifierString())
    }

    return responseTimeInMillisec;
}

CAMX_NAMESPACE_END
