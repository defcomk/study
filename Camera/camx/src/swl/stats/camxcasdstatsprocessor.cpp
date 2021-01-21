////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxcasdstatsprocessor.cpp
/// @brief The class that implements IStatsProcessor for ASD.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxmem.h"
#include "camxtrace.h"
#include "camxcasdstatsprocessor.h"
#include "camxstatsdebuginternal.h"
#include "camxpropertyblob.h"
#include "camxcsljumptable.h"
#include "camxvendortags.h"

CAMX_NAMESPACE_BEGIN

const CHAR* pDefaultAlgorithmLibraryName = "com.qti.stats.asd";
#if defined(_LP64)
const CHAR* pDefaultAlgorithmPath        = "/vendor/lib64/camera/components/";
#else // _LP64
const CHAR* pDefaultAlgorithmPath        = "/vendor/lib/camera/components/";
#endif // _LP64
const CHAR* pFunctionName                = "CreateASDAlgorithm";

// @brief list of tags published
static const UINT32 ASDOutputMetadataTags[] =
{
    PropertyIDASD,
};

// @brief list of vendor tags published
static const struct NodeVendorTag ASDOutputVendorTags[] =
{
    { "org.quic.camera2.asdresults" , "ASDResults" }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::Create
///
/// @brief  Create the object for CASDStatsProcessor.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::Create(
    IStatsProcessor** ppASDStatsProcessor)
{
    CamxResult          result              = CamxResultSuccess;
    CASDStatsProcessor* pASDStatsProcessor  = NULL;

    if (NULL != ppASDStatsProcessor)
    {
        pASDStatsProcessor = CAMX_NEW CASDStatsProcessor;
        if (NULL != pASDStatsProcessor)
        {
            *ppASDStatsProcessor = pASDStatsProcessor;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupStats, "CASDStatsProcessor create failed");
            result = CamxResultENoMemory;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "CASDStatsProcessor::Create Invalid arguments");
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::CASDStatsProcessor
///
/// @brief  default constructor for CASDStatsProcessor.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CASDStatsProcessor::CASDStatsProcessor()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::Initialize
///
/// @brief  Create ASD instance, set param to algo and get param from algo .
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::Initialize(
    const StatsInitializeData* pInitializeData)
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);
    CamxResult  result        = CamxResultSuccess;
    const CHAR* pLibraryName  = NULL;
    const CHAR* pLibraryPath  = NULL;
    VOID*       pFNCreate     = NULL;

    m_pStaticSettings = pInitializeData->pHwContext->GetStaticSettings();
    m_pNode           = pInitializeData->pNode;

    if (NULL == m_pStaticSettings)
    {
        result = CamxResultEInvalidPointer;
        CAMX_LOG_ERROR(CamxLogGroupStats, "Static setting pointer is NULL");
    }

    m_BHistogramId                  = PropertyIDParsedHDRBHistStatsOutput;

    // Create an instance of the core agorithm
    if (CamxResultSuccess == result)
    {
        if (NULL == m_pCHIASDAlgorithm)
        {
            // create and load algorithm
            ASDAlgoCreateParamList createParamList = {};
            ASDAlgoCreateParam     createParams[ASDCreateParamTypeCount] = {};

            createParams[ASDAlgoCreateParamsLoggerFunctionPtr].createParamType =
                ASDAlgoCreateParamsLoggerFunctionPtr;
            createParams[ASDAlgoCreateParamsLoggerFunctionPtr].pCreateParam =
                reinterpret_cast<VOID*>(StatsLoggerFunction);
            createParams[ASDAlgoCreateParamsLoggerFunctionPtr].sizeOfCreateParam =
                sizeof(StatsLoggingFunction);

            createParamList.numberOfCreateParam = ASDCreateParamTypeCount;
            createParamList.pCreateParamList = &createParams[0];

            if (TRUE == m_pStaticSettings->disableASDAlgoCHIOverload)
            {
                if (FALSE == m_pStaticSettings->enableCustomAlgoASD)
                {
                    pLibraryName = pDefaultAlgorithmLibraryName;
                    pLibraryPath = pDefaultAlgorithmPath;
                }
                else
                {
                    pLibraryName = m_pStaticSettings->customAlgoASDName;
                    pLibraryPath = m_pStaticSettings->customAlgoASDPath;
                }

                // Create an instance of the core algorithm
                pFNCreate = StatsUtil::LoadAlgorithmLib(&m_hHandle, pLibraryPath, pLibraryName, pFunctionName);

                if (NULL == pFNCreate)
                {
                    result = CamxResultEUnableToLoad;

                    CAMX_LOG_ERROR(CamxLogGroupStats, "Unable to load the algo library: %s/%s", pLibraryPath, pLibraryName);

                    if (NULL != m_hHandle)
                    {
                        OsUtils::LibUnmap(m_hHandle);
                    }
                }
                else
                {
                    CREATEASD  pFNCreateASD = reinterpret_cast<CREATEASD>(pFNCreate);
                    result = (*pFNCreateASD)(&createParamList, &m_pCHIASDAlgorithm);
                }
            }
            else
            {
                if (NULL != pInitializeData->initializecallback.pASDCallback)
                {
                    m_pfnCreate = pInitializeData->initializecallback.pASDCallback->pfnCreate;

                    if (NULL != m_pfnCreate)
                    {
                        result = (*m_pfnCreate)(&createParamList, &m_pCHIASDAlgorithm);
                    }
                }
            }

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupStats,
                    "Failed to initialize result: %s, CHI Overload enabled: %d, m_pCHIASDAlgorithm: %p, Create pointer: %p",
                    Utils::CamxResultToString(result),
                    m_pStaticSettings->disableASDAlgoCHIOverload,
                    m_pCHIASDAlgorithm,
                    m_pStaticSettings->disableASDAlgoCHIOverload ? pFNCreate : reinterpret_cast<VOID*>(m_pfnCreate));
            }
        }
    }

    // Set the initial set params
    if (CamxResultSuccess == result && NULL != m_pCHIASDAlgorithm)
    {
        if ((CSLPresilEnabled == GetCSLMode()) || (CSLPresilRUMIEnabled == GetCSLMode()))
        {
            m_sensorDim.width = m_pStaticSettings->IFETestImageSizeWidth;
            m_sensorDim.height = m_pStaticSettings->IFETestImageSizeHeight;
        }
        else
        {
            const SensorMode* pSensorMode = NULL;
            // Local result since without dimension info, some scene may be detected.
            result = m_pNode->GetSensorModeData(&pSensorMode);
            if (CamxResultSuccess == result)
            {
                m_sensorDim.width = static_cast<UINT32>(pSensorMode->resolution.outputWidth);
                m_sensorDim.height = static_cast<UINT32>(pSensorMode->resolution.outputHeight);
            }
        }

        m_tuningMode[0].mode              = ModeType::Default;
        m_tuningData.pTuningModeSelectors = reinterpret_cast<VOID*>(&m_tuningMode[0]);
        m_tuningData.numSelectors         = NumOfTuningMode;
        result                            = SendAlgoSetParam(pInitializeData);
    }

    if (CamxResultSuccess == result)
    {
        m_pStatsParser  = pInitializeData->pHwContext->GetStatsParser();
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "ASD algorithm initialize failed!");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::ExecuteProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::ExecuteProcessRequest(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo)
{
    CAMX_ENTRYEXIT_SCOPE_ID(CamxLogGroupStats,
                            SCOPEEventCASDStatsProcessorExecuteProcessRequest,
                            pStatsProcessRequestDataInfo->requestId);

    CamxResult        result  = CamxResultSuccess;
    ASDAlgoInputList  input;
    ASDAlgoOutputList output;
    ASDAlgoInput      inputData[ASDAlgoInputCount]   = { {0} };
    ASDAlgoOutput     outputdata[ASDAlgoOutputCount] = { {0} };
    UINT64            requestIdOffsetFromLastFlush   =
        m_pNode->GetRequestIdOffsetFromLastFlush(pStatsProcessRequestDataInfo->requestId);

    m_currProcessingRequestId = pStatsProcessRequestDataInfo->requestId;
    input.numberOfASDInput    = ASDAlgoInputCount;
    input.pASDInputList       = inputData;
    output.numberOfOutput     = ASDAlgoInputCount;
    output.pASDOutputList     = &outputdata[0];

    if (TRUE == pStatsProcessRequestDataInfo->skipProcessing ||
        TRUE == m_pStaticSettings->disableASDStatsProcessing ||
        (StatsOperationModeFastConvergence == pStatsProcessRequestDataInfo->operationMode) ||
        m_pNode->GetMaximumPipelineDelay() >= requestIdOffsetFromLastFlush)
    {
        GetDefaultOutput(&output);
        CAMX_LOG_VERBOSE(CamxLogGroupStats,
                         "Skipping ASD Algo processing for RequestId=%llu requestIdOffsetFromLastFlush %llu",
                         pStatsProcessRequestDataInfo->requestId, requestIdOffsetFromLastFlush);
    }
    else
    {
        if (NULL == m_pCHIASDAlgorithm)
        {
            // If algorithm is not loaded, output hardcode results
            result = CamxResultEInvalidState;
            CAMX_LOG_ERROR(CamxLogGroupStats, "Invalid state");
        }

        if (CamxResultSuccess == result)
        {
            result = ParseISPStats(pStatsProcessRequestDataInfo);
        }

        if (CamxResultSuccess == result)
        {
            result = GetAlgorithmInput(&input);
        }

        if (CamxResultSuccess == result)
        {
            result = ProcessSetParams(pStatsProcessRequestDataInfo->pTuningModeData);
        }

        if (CamxResultSuccess == result)
        {
            result = GetAlgorithmOutputBuffers(pStatsProcessRequestDataInfo, &output);
        }

        // Process stats
        if (CamxResultSuccess == result)
        {
            result = m_pCHIASDAlgorithm->ASDProcess(m_pCHIASDAlgorithm, &input, &output);
        }

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupStats, "ASD ExecuteProcessRequest failed code: %d", result);
        }
    }

    if (CamxResultSuccess == result)
    {
        result = PublishMetadata(&output);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::GetDependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::GetDependencies(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo,
    StatsDependency*               pStatsDependency)
{
    CAMX_UNREFERENCED_PARAM(pStatsDependency);
    CAMX_UNREFERENCED_PARAM(pStatsProcessRequestDataInfo);

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::~CASDStatsProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CASDStatsProcessor::~CASDStatsProcessor()
{
    // Destroy all the created objects.
    if (NULL != m_pCHIASDAlgorithm)
    {
        m_pCHIASDAlgorithm->ASDDestroy(m_pCHIASDAlgorithm);
        m_pCHIASDAlgorithm = NULL;
    }

    if (NULL != m_hHandle)
    {
        OsUtils::LibUnmap(m_hHandle);
    }

    CAMX_LOG_VERBOSE(CamxLogGroupStats, "CASDStatsProcessor::Destroy");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::GetAlgorithmInput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::GetAlgorithmInput(
    ASDAlgoInputList*               pInput)
{
    CamxResult result       = CamxResultSuccess;
    UINT32     inputIndex   = 0;
    UINT32     metaTagFDROI = 0;

    if (TRUE == m_pNode->GetStaticSettings()->useFDUseCasePool)
    {
        metaTagFDROI = PropertyIDUsecaseFDResults;
    }
    else
    {
        result = VendorTagManager::QueryVendorTagLocation(VendorTagSectionOEMFDResults,
            VendorTagNameOEMFDResults,
            &metaTagFDROI);
    }

    UINT GetProps[] =
    {
        PropertyIDAECFrameInfo,
        m_BHistogramId,
        PropertyIDAECInternal,
        PropertyIDAWBInternal,
        metaTagFDROI
    };

    static const UINT GetPropsLength            = CAMX_ARRAY_SIZE(GetProps);
    VOID*             pData[GetPropsLength]     = { 0 };
    UINT64            offsets[GetPropsLength]   = { 0, 0, 0, PreviousFrameDependency, PreviousFrameDependency};

    m_pNode->GetDataList(GetProps, pData, offsets, GetPropsLength);

    Utils::Memset(&m_expInfo, 0, sizeof(m_expInfo));

    if (NULL != pData[0])
    {
        ReadAECframeInfo(reinterpret_cast<AECFrameInformation*>(pData[0]));
    }
    if (NULL != pData[1])
    {
        // Translate BHIST stats to interface understandable format
        result = ReadBHIST(pData[1], &m_statsBayerHistData);
        // Save the BHIST stats inside core's input structure
        pInput->pASDInputList[inputIndex].inputType      = ASDAlgoInputBayerHist;
        pInput->pASDInputList[inputIndex].pASDInput      = static_cast<VOID*>(&m_statsBayerHistData);
        pInput->pASDInputList[inputIndex].sizeOfASDInput = sizeof(StatsBayerHist);
        inputIndex++;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "PropertyID %d (BHIST Stats) has not been published!", m_BHistogramId);
    }

    if (NULL != pData[2])
    {
        ReadAECInternal(reinterpret_cast<AECOutputInternal*>(pData[2]));
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "PropertyID %d (AEC) has not been published!", PropertyIDAECInternal);
    }

    // Get AWB Information from Internal metadata pool
    if (NULL != pData[3])
    {
        ReadAWBInternal(reinterpret_cast<AWBOutputInternal*>(pData[3]));
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "PropertyID %d (AWB) has not been published!"
            "Landscape Detection would be incorrect", PropertyIDAWBInternal);
        m_expInfo.awbFinaldecision = StatsIlluminantInvalid;
    }

    // Get Face Information from property metadata pool
    if (NULL != pData[4])
    {
        SetFaceROISettings(&m_faceInfo, (reinterpret_cast<FaceROIInformation*>(pData[4])));
        // Save the Exposure info inside core's input structure
        pInput->pASDInputList[inputIndex].inputType      = ASDAlgoInputFaceInfo;
        pInput->pASDInputList[inputIndex].pASDInput      = static_cast<VOID*>(&m_faceInfo);
        pInput->pASDInputList[inputIndex].sizeOfASDInput = sizeof(m_faceInfo);
        inputIndex++;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "PropertyID %d (Face) has not been published!", metaTagFDROI);
    }

    if (CamxResultSuccess == result)
    {
        // Save the Exposure info inside core's input structure
        pInput->pASDInputList[inputIndex].inputType      = ASDAlgoInputAECAWBData;
        pInput->pASDInputList[inputIndex].pASDInput      = static_cast<VOID*>(&m_expInfo);
        pInput->pASDInputList[inputIndex].sizeOfASDInput = sizeof(m_expInfo);
        inputIndex++;
    }

    pInput->numberOfASDInput = inputIndex;
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CASDStatsProcessor::ReadAECframeInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CASDStatsProcessor::ReadAECframeInfo(
    AECFrameInformation* pFrameInfo)
{
    // save lux index inforamtion
    m_expInfo.luxIndex = pFrameInfo->luxIndex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CASDStatsProcessor::ReadAECInternal
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CASDStatsProcessor::ReadAECInternal(
    AECOutputInternal* pAECOutInternal)
{
    m_expInfo.asdExtremeBlueRatio  = pAECOutInternal->asdExtremeBlueRatio;
    m_expInfo.asdExtremeGreenRatio = pAECOutInternal->asdExtremeGreenRatio;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CASDStatsProcessor::ReadAWBInternal
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CASDStatsProcessor::ReadAWBInternal(
    AWBOutputInternal* pAWBOutputInternal)
{
    m_expInfo.awbFinaldecision  = static_cast<INT32>(pAWBOutputInternal->AWBDecision);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CASDStatsProcessor::SetFaceROISettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CASDStatsProcessor::SetFaceROISettings(
    StatsFaceInformation* pFaceInfo,
    FaceROIInformation*   pFaceROIInfo)
{
    pFaceInfo->faceCount = 0;
    for (UINT32 index = 0; index < pFaceROIInfo->ROICount; index++)
    {
        RectangleCoordinate* pRect = &pFaceROIInfo->stabilizedROI[index].faceRect;

        if (pRect->left + pRect->width > m_sensorDim.width ||
            pRect->top + pRect->height > m_sensorDim.height)
        {
            CAMX_LOG_ERROR(CamxLogGroupStats, "Ignoring invalid ROI SensorDim [%ux%u] < ROI[t:%u l:%u w:%u h:%u]",
                m_sensorDim.width,
                m_sensorDim.height,
                pRect->top,
                pRect->left,
                pRect->width,
                pRect->height);
            continue;
        }

        pFaceInfo->face[index].roi.height = pRect->height;
        pFaceInfo->face[index].roi.width  = pRect->width;
        pFaceInfo->face[index].roi.top    = pRect->top;
        pFaceInfo->face[index].roi.left   = pRect->left;
        pFaceInfo->faceCount++;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::GetAlgorithmOutputBuffers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::GetAlgorithmOutputBuffers(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo,
    ASDAlgoOutputList*             pOutput)
{
    CamxResult result = CamxResultSuccess;
    CAMX_UNREFERENCED_PARAM(pStatsProcessRequestDataInfo);

    pOutput->numberOfOutput = 0;

    m_sceneInfo             = { 0 };
    pOutput->pASDOutputList[pOutput->numberOfOutput].outputType      = ASDAlgoOutputSceneInfo;
    pOutput->pASDOutputList[pOutput->numberOfOutput].pASDOutput      = &m_sceneInfo;
    pOutput->pASDOutputList[pOutput->numberOfOutput].sizeOfASDOutput = sizeof(m_sceneInfo);
    CAMX_LOG_VERBOSE(CamxLogGroupStats, "pASDOutputList[%d] =%p, pASDOutput=%p, type =%d, size =%lu\n",
                     ASDAlgoOutputSceneInfo,
                     &pOutput->pASDOutputList[pOutput->numberOfOutput],
                     pOutput->pASDOutputList[pOutput->numberOfOutput].pASDOutput,
                     pOutput->pASDOutputList[pOutput->numberOfOutput].outputType,
                     pOutput->pASDOutputList[pOutput->numberOfOutput].sizeOfASDOutput);
    pOutput->numberOfOutput++;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::SendAlgoSetParam
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::SendAlgoSetParam(
    const StatsInitializeData* pInitializeData)
{
    CamxResult result  = CamxResultSuccess;

    ASDAlgoSetParamList setParams;
    ASDAlgoSetParam     setParam[ASDSetParamLastIndex];

    // Get tuning data manager for ASD tuning tuning data
    m_tuningData.pTuningSetManager =
        reinterpret_cast<VOID*>(pInitializeData->pTuningDataManager->GetChromatix());

    setParams.numberOfASDSetParam       = 0;
    UINT32 index = 0;
    // Set chromatix configuration to ASD algorithm with ASD tuning data
    setParam[index].setParamType      = ASDAlgoSetParamChromatixData;
    setParam[index].pASDSetParam      = static_cast<VOID*> (&m_tuningData);
    setParam[index].sizeOfASDSetParam = sizeof(StatsTuningData);
    index++;

    // Set ASD enable param to asd algorithm
    setParam[index].setParamType      = ASDAlgoSetParamFrameDim;
    setParam[index].pASDSetParam      = &m_sensorDim;
    setParam[index].sizeOfASDSetParam = sizeof(StatsDimension);
    index++;

    setParams.numberOfASDSetParam     = index;
    setParams.pASDSetParamList        = setParam;

    result = m_pCHIASDAlgorithm->ASDSetParam(m_pCHIASDAlgorithm, &setParams);
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "Error! Failed to configure the ASD algorithm");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::ParseISPStats
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::ParseISPStats(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo)
{
    CamxResult      result          = CamxResultSuccess;
    INT32           i;

    // Get the raw stats buffer type
    // Find the matching buffers for HDR-BE and BHist
    for (i = 0; i < pStatsProcessRequestDataInfo->bufferCount; i++)
    {
        if (ISPStatsTypeBHist == pStatsProcessRequestDataInfo->bufferInfo[i].statsType)
        {
            m_BHistogramId = PropertyIDParsedBHistStatsOutput;
            break;
        }
        else if (ISPStatsTypeHDRBHist == pStatsProcessRequestDataInfo->bufferInfo[i].statsType)
        {
            m_BHistogramId = PropertyIDParsedHDRBHistStatsOutput;
            break;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::GetDefaultOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::GetDefaultOutput(
    ASDAlgoOutputList* pOutput)
{
    CamxResult      result    = CamxResultSuccess;
    ASDAlgoGetParam getParam  = { };

    getParam.type                   = ASDAlgoGetParameterTypeSceneInfo;
    getParam.inputs.pInputData      = NULL;
    getParam.inputs.sizeOfInputData = 0;

    pOutput->numberOfOutput                  = 1;
    pOutput->pASDOutputList->outputType      = ASDAlgoOutputSceneInfo;
    pOutput->pASDOutputList->pASDOutput      = &m_sceneInfo;
    pOutput->pASDOutputList->sizeOfASDOutput = sizeof(m_sceneInfo);

    // Get initial data
    getParam.outputs.pOutputData      = pOutput->pASDOutputList->pASDOutput;
    getParam.outputs.sizeOfOutputData = static_cast<UINT32>(pOutput->pASDOutputList->sizeOfASDOutput);

    if (NULL != m_pCHIASDAlgorithm)
    {
        result = m_pCHIASDAlgorithm->ASDGetParam(m_pCHIASDAlgorithm, &getParam);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::ReadBHIST
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::ReadBHIST(
    VOID* pStats,
    StatsBayerHist* pStatsBayerHistData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL == pStats) || (NULL == pStatsBayerHistData))
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "pStats or statsBayerHistData is NULL!");
        return CamxResultEInvalidPointer;
    }

    if (m_BHistogramId == PropertyIDParsedHDRBHistStatsOutput)
    {
        ParsedHDRBHistStatsOutput* pBHDRStats = reinterpret_cast<ParsedHDRBHistStatsOutput*>(*static_cast<VOID**>(pStats));
        pStatsBayerHistData->channelCount     = 3;
        pStatsBayerHistData->binCount         = pBHDRStats->numBins;

        pStatsBayerHistData->histDataType[0]  = StatsColorChannelR;
        pStatsBayerHistData->pHistData[0]     = pBHDRStats->HDRBHistStats.redHistogram;

        pStatsBayerHistData->histDataType[1]  = StatsColorChannelG;
        pStatsBayerHistData->pHistData[1]     = pBHDRStats->HDRBHistStats.greenHistogram;

        pStatsBayerHistData->histDataType[2]  = StatsColorChannelB;
        pStatsBayerHistData->pHistData[2]     = pBHDRStats->HDRBHistStats.blueHistogram;
    }
    else
    {
        ParsedBHistStatsOutput* pBStats   = reinterpret_cast<ParsedBHistStatsOutput*>(*static_cast<VOID**>(pStats));
        pStatsBayerHistData->channelCount = 1;
        pStatsBayerHistData->binCount     = pBStats->numBins;
        switch (pBStats->channelType)
        {
            case ColorChannelR:
                pStatsBayerHistData->histDataType[0] = StatsColorChannelR;
                break;
            case ColorChannelB:
                pStatsBayerHistData->histDataType[0] = StatsColorChannelB;
                break;
            case ColorChannelGR:
                pStatsBayerHistData->histDataType[0] = StatsColorChannelGR;
                break;
            case ColorChannelGB:
                pStatsBayerHistData->histDataType[0] = StatsColorChannelGB;
                break;
            case ColorChannelG:
                pStatsBayerHistData->histDataType[0] = StatsColorChannelG;
                break;
            case ColorChannelY:
                pStatsBayerHistData->histDataType[0] = StatsColorChannelY;
                break;
            default:
                CAMX_LOG_ERROR(CamxLogGroupStats, "Unsupported bayer hist channel!");
                result = CamxResultEUnsupported;
                break;
        }
        pStatsBayerHistData->pHistData[0] = static_cast<const UINT32*>(pBStats->BHistogramStats);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::PublishExternalCameraMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::PublishExternalCameraMetadata(
    ASDOutput* pOutput)
{
    CamxResult result  = CamxResultSuccess;
    UINT32     metaTag = 0;

    result = VendorTagManager::QueryVendorTagLocation(VendorTagSectionASDResults,
                                                      VendorTagNameASDResults,
                                                      &metaTag);
    static const UINT   WriteProps[]                             = {metaTag};
    const VOID*         pOutputData[CAMX_ARRAY_SIZE(WriteProps)] = { pOutput };
    UINT                pDataCount[CAMX_ARRAY_SIZE(WriteProps)]  = { sizeof(ASDOutput) };

   // RetrieveOutput(pOutput, &output);

    return m_pNode->WriteDataList(WriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(WriteProps));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::PublishFrameControlToMainMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::PublishFrameControlToMainMetadata(
    ASDOutput* pOutput)
{
    static const UINT   WriteProps[]                             = {PropertyIDASD};
    const VOID*         pOutputData[CAMX_ARRAY_SIZE(WriteProps)] = { pOutput };
    UINT                pDataCount[CAMX_ARRAY_SIZE(WriteProps)]  = { sizeof(ASDOutput) };

    return m_pNode->WriteDataList(WriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(WriteProps));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::RetrieveOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CASDStatsProcessor::RetrieveOutput(
    const ASDAlgoOutputList* pAlgoOutput,
    ASDOutput*               pOutput)
{
    if ((NULL == pAlgoOutput) || (NULL == pAlgoOutput->pASDOutputList))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupStats, "Invalid input param!! pAlgoOutput %x", pAlgoOutput);
        return;
    }

    for (UINT32 i = 0; i < pAlgoOutput->numberOfOutput; i++)
    {
        if (pAlgoOutput->pASDOutputList[i].outputType == ASDAlgoOutputSceneInfo)
        {
            pOutput->detected[ASDPortrait]  = m_sceneInfo.portraitSeverity;
            pOutput->detected[ASDBacklight] = m_sceneInfo.histoBacklightDetected;
            pOutput->detected[ASDLandscape] = m_sceneInfo.landscapeSeverity;
            pOutput->detected[ASDHDR]       = m_sceneInfo.isHdrScene;

            pOutput->severity[ASDPortrait]  = m_sceneInfo.portraitSeverity;
            pOutput->severity[ASDBacklight] = m_sceneInfo.backlightSceneSeverity;
            pOutput->severity[ASDLandscape] = m_sceneInfo.landscapeSeverity;
            pOutput->severity[ASDHDR]       = static_cast<UINT32>(m_sceneInfo.hdrConfidence * 255);

            CAMX_LOG_VERBOSE(CamxLogGroupStats,
                             "RequestId: %llu "
                             "Detected (PT:%d, BL:%d, LS:%d HDR:%d) "
                             "Severity(PT:%d, BL:%d, LS:%d HDR:%d)",
                             m_currProcessingRequestId,
                             pOutput->detected[ASDPortrait],
                             pOutput->detected[ASDBacklight],
                             pOutput->detected[ASDLandscape],
                             pOutput->detected[ASDHDR],
                             pOutput->severity[ASDPortrait],
                             pOutput->severity[ASDBacklight],
                             pOutput->severity[ASDLandscape],
                             pOutput->severity[ASDHDR]);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::PublishMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::PublishMetadata(
    ASDAlgoOutputList* pOutput)
{
    CamxResult result = CamxResultSuccess;
    ASDOutput  output = { { 0 } };

    RetrieveOutput(pOutput, &output);

    result = PublishFrameControlToMainMetadata(&output);

    if (CamxResultSuccess == result)
    {
        result = PublishExternalCameraMetadata(&output);
    }

    if (CamxResultSuccess == result)
    {
        result = PublishToInternalMetadata();
    }

    if (CamxResultSuccess == result)
    {
        UINT32 isHdrtag         = 0;
        UINT32 hdrConfidencetag = 0;

        VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.stats",
            "is_hdr_scene",
            &isHdrtag);
        VendorTagManager::QueryVendorTagLocation("org.codeaurora.qcamera3.stats",
            "is_hdr_scene_confidence",
            &hdrConfidencetag);

        if (isHdrtag && hdrConfidencetag)
        {
            static const UINT   WriteProps[] = { isHdrtag, hdrConfidencetag };
            const VOID*         pOutputData[CAMX_ARRAY_SIZE(WriteProps)] =
            {
                &m_sceneInfo.isHdrScene,
                &m_sceneInfo.hdrConfidence
            };
            UINT pDataCount[CAMX_ARRAY_SIZE(WriteProps)] = { 1, 1 };

            result = m_pNode->WriteDataList(WriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(WriteProps));
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupStats, "Invalid vendor Tags Id: is_hdr_scene:%x is_hdr_scene_confidence:%x",
                isHdrtag, hdrConfidencetag);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::PublishToInternalMetadata
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::PublishToInternalMetadata()
{
    ASDOutputInternal   output                                   = {0};
    static const UINT   WriteProps[]                             = {PropertyIDASDInternal};
    const VOID*         pOutputData[CAMX_ARRAY_SIZE(WriteProps)] = { &output };
    UINT                pDataCount[CAMX_ARRAY_SIZE(WriteProps)]  = { sizeof(output) };

    output.severity = m_sceneInfo.backlightSceneSeverity;

    return m_pNode->WriteDataList(WriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(WriteProps));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CASDStatsProcessor::SetAlgoChromatix
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::SetAlgoChromatix(
    ChiTuningModeParameter* pInputTuningModeData,
    StatsTuningData*        pTuningData)
{
    CamxResult              result = CamxResultSuccess;

    if (NULL != pInputTuningModeData)
    {
        pTuningData->pTuningSetManager    = m_tuningData.pTuningSetManager;
        pTuningData->pTuningModeSelectors = reinterpret_cast<TuningMode*>(&pInputTuningModeData->TuningMode[0]);
        pTuningData->numSelectors         = pInputTuningModeData->noOfSelectionParameter;
        CAMX_LOG_VERBOSE(CamxLogGroupStats,
                         "Tuning data as mode: %d usecase %d  feature1 %d feature2 %d scene %d, effect %d,",
                         pInputTuningModeData->TuningMode[0].mode,
                         pInputTuningModeData->TuningMode[2].subMode.usecase,
                         pInputTuningModeData->TuningMode[3].subMode.feature1,
                         pInputTuningModeData->TuningMode[4].subMode.feature2,
                         pInputTuningModeData->TuningMode[5].subMode.scene,
                         pInputTuningModeData->TuningMode[6].subMode.effect);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "Input tuning data is NULL pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CASDStatsProcessor::ProcessSetParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::ProcessSetParams(
    ChiTuningModeParameter* pInputTuningModeData)
{

    CAMX_ENTRYEXIT(CamxLogGroupStats);

    CamxResult          result                               = CamxResultSuccess;
    ASDAlgoSetParam     setTuningParam[ASDSetParamLastIndex] = { { 0 } };
    StatsTuningData     statsTuningData                      = { 0 };
    ASDAlgoSetParamList setParams;

    SetAlgoChromatix(pInputTuningModeData, &statsTuningData);

    setParams.numberOfASDSetParam       = 0;
    setTuningParam[0].setParamType      = ASDAlgoSetParamChromatixData;
    setTuningParam[0].pASDSetParam      = static_cast<VOID*> (&statsTuningData);
    setTuningParam[0].sizeOfASDSetParam = sizeof(StatsTuningData);
    setParams.numberOfASDSetParam++;

    setParams.pASDSetParamList          = &setTuningParam[0];
    result = m_pCHIASDAlgorithm->ASDSetParam(m_pCHIASDAlgorithm, &setParams);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "ASDSetParam() failed!");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CASDStatsProcessor::GetPublishList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CASDStatsProcessor::GetPublishList(
    const UINT32    maxTagArraySize,
    UINT32*         pTagArray,
    UINT32*         pTagCount,
    UINT32*         pPartialTagCount)
{
    CamxResult  result          = CamxResultSuccess;
    UINT32      tagCount        = 0;
    UINT32      numMetadataTags = CAMX_ARRAY_SIZE(ASDOutputMetadataTags);
    UINT32      numVendorTags   = CAMX_ARRAY_SIZE(ASDOutputVendorTags);
    UINT32      tagID;


    if (numMetadataTags + numVendorTags <= maxTagArraySize)
    {
        for (UINT32 tagIndex = 0; tagIndex < numMetadataTags; ++tagIndex)
        {
            pTagArray[tagCount++] = ASDOutputMetadataTags[tagIndex];
        }

        for (UINT32 tagIndex = 0; tagIndex < numVendorTags; ++tagIndex)
        {
            result = VendorTagManager::QueryVendorTagLocation(
                ASDOutputVendorTags[tagIndex].pSectionName,
                ASDOutputVendorTags[tagIndex].pTagName,
                &tagID);

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupMeta, "Cannot query vendor tag %s %s",
                    ASDOutputVendorTags[tagIndex].pSectionName,
                    ASDOutputVendorTags[tagIndex].pTagName);
                break;
            }
            pTagArray[tagCount++] = tagID;
        }
    }
    else
    {
        result = CamxResultEOutOfBounds;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "ERROR %d %d tags cannot be published.",
            numMetadataTags,
            numVendorTags);
    }

    if (CamxResultSuccess == result)
    {
        *pTagCount        = tagCount;
        *pPartialTagCount = 0;
        CAMX_LOG_VERBOSE(CamxLogGroupMeta, "%d tags will be published.", tagCount);
    }
    return result;
}

CAMX_NAMESPACE_END
