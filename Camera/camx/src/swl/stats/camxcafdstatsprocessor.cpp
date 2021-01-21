////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxcafdstatsprocessor.cpp
/// @brief The class that implements IStatsProcessor for AFD.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxhal3module.h"
#include "camxmem.h"
#include "camxstatsdebuginternal.h"
#include "camxtrace.h"
#include "camxtuningdatamanager.h"
#include "camxcafdstatsprocessor.h"

CAMX_NAMESPACE_BEGIN

static const CHAR* pDefaultAlgorithmLibraryName = "com.qti.stats.afd";
#if defined(_LP64)
static const CHAR* pDefaultAlgorithmPath        = "/vendor/lib64/camera/components/";
#else // _LP64
static const CHAR* pDefaultAlgorithmPath        = "/vendor/lib/camera/components/";
#endif // _LP64
static const CHAR* pFunctionName                = "CreateAFDAlgorithm";

// @brief list of tags published
static const UINT32 AFDOutputMetadataTags[] =
{
    ControlAEAntibandingMode,
    PropertyIDAFDStatsControl,
    PropertyIDAFDFrameInfo
};

// Global AFD algo pointer
static CHIAFDAlgorithm* g_pCHIAFDAlgorithm = NULL;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::Create
///
/// @brief  Create the object for CAFDStatsProcessor.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFDStatsProcessor::Create(
    IStatsProcessor** ppAFDStatsProcessor)
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);

    CamxResult          result             = CamxResultSuccess;
    CAFDStatsProcessor* pAFDStatsProcessor = NULL;

    if (NULL != ppAFDStatsProcessor)
    {
        pAFDStatsProcessor = CAMX_NEW CAFDStatsProcessor;

        if (NULL != pAFDStatsProcessor)
        {
            *ppAFDStatsProcessor = pAFDStatsProcessor;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupStats, "CAFDStatsProcessor create failed");
            result = CamxResultENoMemory;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "CAFDStatsProcessor::Create Invalid arguments");
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::CAFDStatsProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAFDStatsProcessor::CAFDStatsProcessor()
    : m_pAFDAlgorithmHandler(NULL)
    , m_pAFDIOHandler(NULL)
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::SetAFDAlgorithmHandler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFDStatsProcessor::SetAFDAlgorithmHandler(
    CAFDAlgorithmHandler* pAlgoHandler)
{
    m_pAFDAlgorithmHandler = pAlgoHandler;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::SetAFDIOHandler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFDStatsProcessor::SetAFDIOHandler(
    CAFDIOHandler* pAFDIOHandler)
{
    m_pAFDIOHandler = pAFDIOHandler;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFDStatsProcessor::Initialize(
    const StatsInitializeData* pInitializeData)
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);

    CamxResult  result       = CamxResultSuccess;
    const CHAR* pLibraryName = NULL;
    const CHAR* pLibraryPath = NULL;

    m_pfnCreate              = NULL;

    if (NULL != pInitializeData &&
        NULL != pInitializeData->pHwContext &&
        NULL != pInitializeData->pTuningDataManager)
    {
        m_pStaticSettings   = pInitializeData->pHwContext->GetStaticSettings();
        m_pNode             = pInitializeData->pNode;
        if (NULL != m_pStaticSettings)
        {
            // Create an instance of the core agorithm
            if (NULL == g_pCHIAFDAlgorithm)
            {
                // create and load algorithm
                AFDAlgoCreateParamList createParamList = {};
                AFDAlgoCreateParam     createParams[AFDCreateParamTypeCount] = {};

                createParams[AFDAlgoCreateParamsLoggerFunctionPtr].createParamType =
                    AFDAlgoCreateParamsLoggerFunctionPtr;
                createParams[AFDAlgoCreateParamsLoggerFunctionPtr].pCreateParam =
                    reinterpret_cast<VOID*>(StatsLoggerFunction);
                createParams[AFDAlgoCreateParamsLoggerFunctionPtr].sizeOfCreateParam =
                    sizeof(StatsLoggingFunction);

                createParamList.paramCount = AFDCreateParamTypeCount;
                createParamList.pCreateParamList = &createParams[0];

                if (FALSE == m_pStaticSettings->enableCustomAlgoAFD)
                {
                    pLibraryName = pDefaultAlgorithmLibraryName;
                    pLibraryPath = pDefaultAlgorithmPath;
                }
                else
                {
                    pLibraryName = m_pStaticSettings->customAlgoAFDName;
                    pLibraryPath = m_pStaticSettings->customAlgoAFDPath;
                }

                // Create an instance of the core algorithm
                VOID* pAddr = StatsUtil::LoadAlgorithmLib(&m_hHandle, pLibraryPath, pLibraryName, pFunctionName);


                if (NULL == pAddr)
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
                    CREATEAFD  pAFD = reinterpret_cast<CREATEAFD>(pAddr);
                    result = (*pAFD)(&createParamList, &g_pCHIAFDAlgorithm);
                }
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupStats, "GetStaticSettings failed");
        }
        m_pTuningDataManager = pInitializeData->pTuningDataManager->GetChromatix();
    }
    else
    {
        if (NULL == pInitializeData)
        {
            CAMX_LOG_ERROR(CamxLogGroupStats, "StatsInitializeData is Invalid(NULL)");
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupStats, "pHwContext: %p or pTuningDataManager is NULL",
                pInitializeData->pHwContext,
                pInitializeData->pTuningDataManager);
        }
    }

    m_pAFDIOHandler = CAMX_NEW CAFDIOHandler();

    if (NULL == m_pAFDIOHandler)
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "Auto Flicker Detection IO Handler Allocation failed");
        result = CamxResultENoMemory;
    }

    if (CamxResultSuccess == result)
    {
        result = m_pAFDIOHandler->Initialize(pInitializeData);

    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::ExecuteProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFDStatsProcessor::ExecuteProcessRequest(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo)
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);

    CamxResult         result           = CamxResultSuccess;
    AFDAlgoInputList   input            = { 0 };
    AFDAlgoOutputList* pOutput          = NULL;
    UINT64             requestId        = pStatsProcessRequestDataInfo->requestId;
    UINT64 requestIdOffsetFromLastFlush =
        m_pNode->GetRequestIdOffsetFromLastFlush(pStatsProcessRequestDataInfo->requestId);

    GetPrevConfigFromAlgo(); // Always call this function before Read and Set HAL param
    ReadHALAFDParam(&m_HALParam);

    /// Get an output buffer to hold AFD algorithm output
    if (m_pAFDIOHandler != NULL)
    {
        pOutput = m_pAFDIOHandler->GetAFDOutputBuffers();
        m_pAFDIOHandler->GetAlgoOutputBuffers(pOutput);
    }

    if (TRUE == pStatsProcessRequestDataInfo->skipProcessing ||
        StatsOperationModeFastConvergence == pStatsProcessRequestDataInfo->operationMode ||
        m_pNode->GetMaximumPipelineDelay() >= requestIdOffsetFromLastFlush ||
        TRUE == m_pStaticSettings->disableAFDStatsProcessing)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupStats,
                         "Skip algo processing, RequestId=%llu requestIdOffsetFromLastFlush = %llu operationMode:%d "
                         "skipProcessing:%d disableAFDStatsProcessing:%d"
                          "maximumPipelineDelay:%d",
                         pStatsProcessRequestDataInfo->requestId,
                         requestIdOffsetFromLastFlush,
                         pStatsProcessRequestDataInfo->operationMode,
                         pStatsProcessRequestDataInfo->skipProcessing,
                         m_pStaticSettings->disableAFDStatsProcessing,
                         m_pNode->GetMaximumPipelineDelay());
    }
    else
    {

        result = PrepareInputParams(&requestId, &input);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupStats,
                "RequestId=%llu Failed to PrepareInputParams",
                pStatsProcessRequestDataInfo->requestId);
        }

        if (CamxResultSuccess == result)
        {
            result = ProcessSetParams(pStatsProcessRequestDataInfo->pTuningModeData);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupStats,
                    "RequestId=%llu Failed to ProcessSetParams",
                    pStatsProcessRequestDataInfo->requestId);
            }
        }

        if (CamxResultSuccess == result)
        {
            result = g_pCHIAFDAlgorithm->AFDProcess(g_pCHIAFDAlgorithm, &input, pOutput);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupStats,
                    "RequestId=%llu Failed AFDProcess",
                    pStatsProcessRequestDataInfo->requestId);
            }
        }

    }

    // Publish Output even in error case
    if (NULL != pOutput)
    {
        m_pAFDIOHandler->PublishOutput(pOutput, requestId);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::GetPrevConfigFromAlgo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFDStatsProcessor::GetPrevConfigFromAlgo()
{
    AFDAlgoGetParam         getParam[2];
    AFDAlgoGetParamList     getParamList;
    UINT32                  index = 0;

    // Get RS Config data
    getParam[index].type                    = AFDAlgoGetParamStatsConfig;
    getParam[index].input.pInputData        = NULL;
    getParam[index].input.sizeOfInputData   = 0;
    getParam[index].output.pOutputData      = &m_pAFDIOHandler->m_rowSumConfig;
    getParam[index].output.sizeOfOutputData = static_cast<UINT32>(sizeof(m_pAFDIOHandler->m_rowSumConfig));
    index++;

    // Get last detect AFD mode
    getParam[index].type                    = AFDAlgoGetParamLastDetectedMode;
    getParam[index].input.pInputData        = NULL;
    getParam[index].input.sizeOfInputData   = 0;
    getParam[index].output.pOutputData      = &m_pAFDIOHandler->m_lastAFDMode;
    getParam[index].output.sizeOfOutputData = static_cast<UINT32>(sizeof(m_pAFDIOHandler->m_lastAFDMode));
    index++;

    getParamList.numberOfAFDGetParams = index;
    getParamList.pAFDGetParamList     = &getParam[0];

    if (NULL != g_pCHIAFDAlgorithm)
    {
        g_pCHIAFDAlgorithm->AFDGetParam(g_pCHIAFDAlgorithm, &getParamList);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::PrepareInputParams()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFDStatsProcessor::PrepareInputParams(
    UINT64*           pRequestId,
    AFDAlgoInputList* pInputList)
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);
    CamxResult result = CamxResultSuccess;

    pInputList->pAFDInputList = m_inputArray;
    pInputList->inputCount    = 0;

    if (NULL == m_pAFDIOHandler)
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "AFD IO Handler NULL pointer");
        return CamxResultEInvalidPointer;
    }


    StatsBayerGrid* pBGStat   = m_pAFDIOHandler->ReadBGStat();
    if (NULL == pBGStat)
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "BG Stat NULL pointer");
        return CamxResultEInvalidPointer;
    }
    else
    {
        UpdateInputParam(pInputList, AFDAlgoInputType::AFDAlgoInputBayerGrid,
            sizeof(StatsBayerGrid), static_cast<VOID*>(pBGStat));
    }

    if (NULL == pRequestId)
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "Request Id NULL pointer");
        return CamxResultEInvalidPointer;
    }
    else
    {
        UpdateInputParam(pInputList, AFDAlgoInputType::AFDAlgoInputFrameID,
            sizeof(UINT64), static_cast<VOID*>(pRequestId));
    }

    StatsRowSum* pRStat    = m_pAFDIOHandler->ReadRowSumStat();
    if (NULL == pRStat)
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "pRStat NULL pointer");
        return CamxResultEInvalidPointer;
    }
    else
    {
        UpdateInputParam(pInputList, AFDAlgoInputType::AFDAlgoInputRowSum,
            sizeof(StatsRowSum), static_cast<VOID*>(pRStat));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::UpdateInputParam
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAMX_INLINE VOID CAFDStatsProcessor::UpdateInputParam(
    AFDAlgoInputList* pInputList,
    AFDAlgoInputType  inputType,
    UINT32            inputSize,
    VOID*             pValue)
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);
    pInputList->pAFDInputList[pInputList->inputCount].inputType      = inputType;
    pInputList->pAFDInputList[pInputList->inputCount].sizeOfAFDInput = static_cast<UINT32>(inputSize);
    pInputList->pAFDInputList[pInputList->inputCount].pAFDInput      = pValue;
    pInputList->inputCount++;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::ProcessSetParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFDStatsProcessor::ProcessSetParams(
    ChiTuningModeParameter* pInputTuningModeData)
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);

    CamxResult          result                         = CamxResultSuccess;
    AFDAlgoSetParam     setParam[AFDSetParamLastIndex] = { { 0 } };
    AFDAECInfo*         pAFDAECInfo                    = NULL;
    AFDAFInfo*          pAFDAFInfo                     = NULL;
    AFDSensorInfo*      pAFDSensorInfo                 = NULL;
    AFDAlgoSetParamList setParamList;
    UINT                binningTypeVertical;
    FLOAT               maxNumOfSensorLinesPerSecond;
    UINT32              numOfLinesCoveredByRowStats;

    setParamList.numberOfSetParam = 0;
    setParamList.pAFDSetParamList = setParam;

    if (NULL == m_pAFDIOHandler)
    {
        CAMX_LOG_ERROR(CamxLogGroupStats, "AFD IO Handler NULL pointer");
        result = CamxResultEInvalidPointer;
        return result;
    }

    pAFDAECInfo                                         = m_pAFDIOHandler->ReadAECInput();
    setParam[AFDAlgoSetParamAECInfo].pAFDSetParam       = static_cast<VOID*>(pAFDAECInfo);
    setParam[AFDAlgoSetParamAECInfo].setParamType       = AFDAlgoSetParamAECInfo;
    setParam[AFDAlgoSetParamAECInfo].sizeOfAFDSetParam  = sizeof(AFDAECInfo);
    setParamList.numberOfSetParam                       = setParamList.numberOfSetParam + 1;

    pAFDAFInfo                                          = m_pAFDIOHandler->ReadAFInput();
    setParam[AFDAlgoSetParamAFInfo].pAFDSetParam        = static_cast<VOID*>(pAFDAFInfo);
    setParam[AFDAlgoSetParamAFInfo].setParamType        = AFDAlgoSetParamAFInfo;
    setParam[AFDAlgoSetParamAFInfo].sizeOfAFDSetParam   = sizeof(AFDAFInfo);
    setParamList.numberOfSetParam                       = setParamList.numberOfSetParam + 1;

    pAFDSensorInfo = m_pAFDIOHandler->ReadSensorInput();
    if (pAFDSensorInfo->binningTypeV == 0)
    {
        binningTypeVertical = 1;
    }
    else
    {
        binningTypeVertical = pAFDSensorInfo->binningTypeV;
    }

    /* compute timing settings: compute number of sensor lines in one second (sensor lines are configured based
    on max FPS and not changing with respect to cur FPS, hence use max FPS in computing), then multiple by number
    of lines covered row stats array */
    maxNumOfSensorLinesPerSecond = pAFDSensorInfo->maxFPS * static_cast<FLOAT>(pAFDSensorInfo->numLinesPerFrame);
    if (maxNumOfSensorLinesPerSecond <= 0)
    {
        maxNumOfSensorLinesPerSecond = 1;
    }

    m_pAFDIOHandler->m_rowSumInfo.rowSumCount =
        m_pAFDIOHandler->m_rowSumInfo.horizontalRegionCount * m_pAFDIOHandler->m_rowSumInfo.verticalRegionCount;
    numOfLinesCoveredByRowStats = m_pAFDIOHandler->m_rowSumInfo.verticalRegionCount
                                  * m_pAFDIOHandler->m_rowSumInfo.regionHeight;

    pAFDSensorInfo->rowSumTime = (numOfLinesCoveredByRowStats / maxNumOfSensorLinesPerSecond);
    CAMX_LOG_VERBOSE(CamxLogGroupStats, "maxNumOfSensorLinesPerSecond:%f rowSumCount:%d rowSumTime:%lf "
        "numOfLinesCoveredByRowStats %u horizontalRegionCount %d, verticalRegionCount %d, regionHeight %d",
        maxNumOfSensorLinesPerSecond,
        m_pAFDIOHandler->m_rowSumInfo.rowSumCount,
        pAFDSensorInfo->rowSumTime,
        numOfLinesCoveredByRowStats,
        m_pAFDIOHandler->m_rowSumInfo.horizontalRegionCount,
        m_pAFDIOHandler->m_rowSumInfo.verticalRegionCount,
        m_pAFDIOHandler->m_rowSumInfo.regionHeight);

    setParam[AFDAlgoSetParamAFDMode].pAFDSetParam            = static_cast<VOID*>(&m_HALParam.antiBandingMode);
    setParam[AFDAlgoSetParamAFDMode].setParamType            = AFDAlgoSetParamAFDMode;
    setParam[AFDAlgoSetParamAFDMode].sizeOfAFDSetParam       = sizeof(m_HALParam.antiBandingMode);
    setParamList.numberOfSetParam                            = setParamList.numberOfSetParam + 1;

    setParam[AFDAlgoSetParamSensorInfo].pAFDSetParam         = static_cast<VOID*>(pAFDSensorInfo);
    setParam[AFDAlgoSetParamSensorInfo].setParamType         = AFDAlgoSetParamSensorInfo;
    setParam[AFDAlgoSetParamSensorInfo].sizeOfAFDSetParam    = sizeof(AFDSensorInfo);
    setParamList.numberOfSetParam                            = setParamList.numberOfSetParam + 1;

    StatsTuningData statsTuningData = { 0 };
    SetAlgoChromatix(pInputTuningModeData, &statsTuningData);
    setParam[AFDAlgoSetParamChromatixData].pAFDSetParam      = static_cast<VOID*>(&statsTuningData);
    setParam[AFDAlgoSetParamChromatixData].setParamType      = AFDAlgoSetParamChromatixData;
    setParam[AFDAlgoSetParamChromatixData].sizeOfAFDSetParam = sizeof(StatsTuningData);
    setParamList.numberOfSetParam                            = setParamList.numberOfSetParam + 1;

    CDKResult cdkResult = g_pCHIAFDAlgorithm->AFDSetParam(g_pCHIAFDAlgorithm, &setParamList);
    result              = StatsUtil::CdkResultToCamXResult(cdkResult);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::GetDependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFDStatsProcessor::GetDependencies(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo,
    StatsDependency*               pStatsDependency)
{
    CAMX_UNREFERENCED_PARAM(pStatsDependency);
    CAMX_ENTRYEXIT(CamxLogGroupStats);
    CamxResult result           = CamxResultSuccess;
    UINT       maxPipelineDelay = m_pNode->GetMaximumPipelineDelay();

    if (TRUE == pStatsProcessRequestDataInfo->skipProcessing)
    {
        // We need to publish previous frame data to property pool when skipProcessing flag is set.
        // Add previous frame(offset = 1) AFD Frame/Stats Control properties as dependency.
        pStatsDependency->properties[pStatsDependency->propertyCount++] = { PropertyIDAFDStatsControl, 1};

        // For AF Frame Info and AEC Frame info dependency for offset = pipeline delay
        pStatsDependency->properties[pStatsDependency->propertyCount++] = { PropertyIDAFFrameInfo, maxPipelineDelay };

        pStatsDependency->properties[pStatsDependency->propertyCount++] = { PropertyIDAECFrameInfo, maxPipelineDelay };
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::IsDependenciesSatisfied
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFDStatsProcessor::IsDependenciesSatisfied(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo,
    BOOL*                          pIsSatisfied)
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);

    CamxResult   result           = CamxResultSuccess;
    BOOL         isSatisfiedHDRBE = FALSE;
    BOOL         isSatisfiedRS    = FALSE;
    ISPStatsType statsType;

    for (INT32 i = 0; i < pStatsProcessRequestDataInfo->bufferCount; i++)
    {
        statsType = pStatsProcessRequestDataInfo->bufferInfo[i].statsType;
        switch (statsType)
        {
            case ISPStatsTypeHDRBE:
                isSatisfiedHDRBE = TRUE;
                break;
            case ISPStatsTypeRS:
                isSatisfiedRS = TRUE;
                break;
            default:
                break;
        }
    }

    *pIsSatisfied = (isSatisfiedHDRBE && isSatisfiedRS);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFDStatsProcessor::~CAFDStatsProcessor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAFDStatsProcessor::~CAFDStatsProcessor()
{
    CAMX_ENTRYEXIT(CamxLogGroupStats);

    if (NULL != m_pAFDAlgorithmHandler)
    {
        CAMX_DELETE m_pAFDAlgorithmHandler;
        m_pAFDAlgorithmHandler = NULL;
    }

    if (NULL != m_pAFDIOHandler)
    {
        CAMX_DELETE m_pAFDIOHandler;
        m_pAFDIOHandler = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFDStatsProcessor::ReadHALAFDParam
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFDStatsProcessor::ReadHALAFDParam(
    AFDHALParam* pHALParam
    ) const
{

    UINT       HALAFDData[] =
    {
        InputControlAEAntibandingMode,
    };
    static const UINT HALAFDDataLength        = CAMX_ARRAY_SIZE(HALAFDData);
    VOID*  pData[HALAFDDataLength]            = { 0 };
    UINT64 HALAFDDataOffset[HALAFDDataLength] = { 0 };

    m_pAFDIOHandler->m_pNode->GetDataList(HALAFDData, pData, HALAFDDataOffset, HALAFDDataLength);

    Utils::Memcpy(&pHALParam->antiBandingMode, (pData[0]), sizeof(pHALParam->antiBandingMode));

    m_pAFDIOHandler->SetHALParam(pHALParam);


}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFDStatsProcessor::SetAlgoChromatix
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFDStatsProcessor::SetAlgoChromatix(
    ChiTuningModeParameter* pInputTuningModeData,
    StatsTuningData*        pTuningData)
{
    CamxResult              result = CamxResultSuccess;

    if (NULL != pInputTuningModeData)
    {
        pTuningData->pTuningSetManager    = m_pTuningDataManager;
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
/// CAFDStatsProcessor::GetPublishList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFDStatsProcessor::GetPublishList(
    const UINT32    maxTagArraySize,
    UINT32*         pTagArray,
    UINT32*         pTagCount,
    UINT32*         pPartialTagCount)
{
    CamxResult result          = CamxResultSuccess;
    UINT32     numMetadataTags = CAMX_ARRAY_SIZE(AFDOutputMetadataTags);

    if (numMetadataTags <= maxTagArraySize)
    {
        for (UINT32 tagIndex = 0; tagIndex < numMetadataTags; ++tagIndex)
        {
            pTagArray[tagIndex] = AFDOutputMetadataTags[tagIndex];
        }
    }
    else
    {
        result = CamxResultEOutOfBounds;
        CAMX_LOG_ERROR(CamxLogGroupMeta, "ERROR %d tags cannot be published.", numMetadataTags);
    }

    if (CamxResultSuccess == result)
    {
        *pTagCount        = numMetadataTags;
        *pPartialTagCount = 0;
        CAMX_LOG_VERBOSE(CamxLogGroupMeta, "%d tags will be published.", numMetadataTags);
    }
    return result;
}

CAMX_NAMESPACE_END
