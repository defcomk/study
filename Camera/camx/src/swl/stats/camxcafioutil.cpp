////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxcafioutil.cpp
/// @brief This class provides utility functions to handle input and output of AF algorithm
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxcafstatsprocessor.h"
#include "camxhal3metadatautil.h"
#include "camxhal3module.h"
#include "camximagesensormoduledata.h"
#include "camximagesensordata.h"
#include "camxvendortags.h"
#include "camxcafioutil.h"
#include "camxtitan17xdefs.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 DefaultReadPDMargin     = 10;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::CAFIOUtil
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAFIOUtil::CAFIOUtil()
    : m_controlSceneMode(ControlSceneModeDisabled)
    , m_pPDLib(NULL)
    , m_AFLock(FALSE)
    , m_pNCSSensorHandleGyro(NULL)
    , m_pNCSSensorHandleGravity(NULL)
    , m_pNCSServiceObject(NULL)
{
    PrepareAFOutputBuffers();
    m_PDAFDataInput.pDefocus.xWindowCount   = 0;
    m_PDAFDataInput.pDefocus.yWindowCount   = 0;
    m_isPDAFEnabled                         = FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::~CAFIOUtil
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAFIOUtil::~CAFIOUtil()
{
    // free memory allocated for holding vendor tag data
    for (UINT32 i = 0; i < m_vendorTagPublishDataList.vendorTagCount; i++)
    {
        if (NULL != m_vendorTagPublishDataList.vendorTag[i].pData)
        {
            CAMX_FREE(m_vendorTagPublishDataList.vendorTag[i].pData);
            m_vendorTagPublishDataList.vendorTag[i].pData = NULL;
        }
    }

    if (NULL != m_focusRegionInfo.pWeightedROI)
    {
        CAMX_FREE(m_focusRegionInfo.pWeightedROI);
        m_focusRegionInfo.pWeightedROI = NULL;
    }

    if (NULL != m_pNCSSensorHandleGyro)
    {
        m_pNCSServiceObject->UnregisterService(m_pNCSSensorHandleGyro);
        m_pNCSSensorHandleGyro = NULL;
    }

    if (NULL != m_pNCSSensorHandleGravity)
    {
        m_pNCSServiceObject->UnregisterService(m_pNCSSensorHandleGravity);
        m_pNCSSensorHandleGravity = NULL;
    }

    if (NULL != m_pTOFInterfaceObject)
    {
        m_pTOFInterfaceObject->Destroy();
        m_pTOFInterfaceObject = NULL;
    }

    if (NULL != m_pBFStatsOutput)
    {
        CAMX_DELETE[] m_pBFStatsOutput;
        m_pBFStatsOutput = NULL;
    }

    if (NULL != m_pHDRBEStatsOutput)
    {
        CAMX_DELETE[] m_pHDRBEStatsOutput;
        m_pHDRBEStatsOutput = NULL;
    }

    if (NULL != m_pStatsAFPropertyReadWrite)
    {
        CAMX_DELETE m_pStatsAFPropertyReadWrite;
        m_pStatsAFPropertyReadWrite = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::Initialize(
    const StatsInitializeData* pInitializeData)
{
    CAMX_ASSERT(NULL != pInitializeData);
    CAMX_ASSERT(NULL != pInitializeData->pNode);
    CAMX_ASSERT(NULL != pInitializeData->pHwContext);

    const HwEnvironment*  pHwEnvironment  = HwEnvironment::GetInstance();
    CamxResult            result          = CamxResultSuccess;
    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    m_pDebugDataPool    = pInitializeData->pDebugDataPool;
    m_pNode             = pInitializeData->pNode;

    SetStatsParser(pInitializeData->pHwContext->GetStatsParser());
    m_cameraID = pInitializeData->pPipeline->GetCameraId();
    if (TRUE == pStaticSettings->enableNCSService)
    {
        result = SetupNCSLink();

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_WARN(CamxLogGroupAF, "Failed setting up NCS link");
        }
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupAF, "NCS interface not enabled");
    }

    if (TRUE == pStaticSettings->enableTOFInterface)
    {
        result = SetupTOFLink(pInitializeData->pChiContext);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_WARN(CamxLogGroupAF, "Failed setting up TOF link");
        }
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupAF, "TOF interface not enabled");
    }

    // Retrieve the static capabilities for this camera
    UINT32 cameraID = pInitializeData->pPipeline->GetCameraId();
    result = pHwEnvironment->GetCameraInfo(cameraID, &m_hwCameraInfo);

    if (CamxResultSuccess == result)
    {
        m_maxFocusRegions = m_hwCameraInfo.pPlatformCaps->maxRegionsAF;

        // reading AF Calibration OTP data (lenspos and distance)
        m_AFCalibrationOTPData.numberOfDistances = m_hwCameraInfo.pSensorCaps->OTPData.AFCalibration.numberOfDistances;
        for (UINT32 i = 0; i <  m_AFCalibrationOTPData.numberOfDistances; i++)
        {
            m_AFCalibrationOTPData.calibrationInfo[i].chartDistanceCM =
                m_hwCameraInfo.pSensorCaps->OTPData.AFCalibration.calibrationInfo[i].chartDistanceCM;
            m_AFCalibrationOTPData.calibrationInfo[i].lenspos =
                m_hwCameraInfo.pSensorCaps->OTPData.AFCalibration.calibrationInfo[i].stepPosition;
        }
    }

    // Allocate memory for ROIs being sent by HAL
    m_focusRegionInfo.pWeightedROI =
        reinterpret_cast<StatsWeightedROI*>(CAMX_CALLOC((sizeof(StatsWeightedROI) * m_maxFocusRegions)));
    if (NULL == m_focusRegionInfo.pWeightedROI)
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "Allocating memory for WeightedROI failed.");
        result = CamxResultENoMemory;
    }


    if (CamxResultSuccess == result)
    {
        m_requestQueueDepth = pInitializeData->pPipeline->GetRequestQueueDepth();

        CAMX_LOG_VERBOSE(CamxLogGroupAF, "GetRequestQueueDepth() returned %u", m_requestQueueDepth);


        m_pBFStatsOutput = CAMX_NEW ParsedBFStatsOutput[m_requestQueueDepth];
        if (NULL == m_pBFStatsOutput)
        {
            result = CamxResultENoMemory;
        }


        if (CamxResultSuccess == result)
        {
            m_pHDRBEStatsOutput = CAMX_NEW ParsedHDRBEStatsOutput[m_requestQueueDepth];
            if (NULL == m_pHDRBEStatsOutput)
            {
                result = CamxResultENoMemory;
            }
        }
    }

    if (CamxResultSuccess == result)
    {
        SensorModuleStaticCaps       sensorStaticCaps;
        const ImageSensorModuleData* pSensorModuleData =
            pInitializeData->pHwContext->GetImageSensorModuleData(cameraID);

        CAMX_ASSERT(NULL != pSensorModuleData);

        pSensorModuleData->GetSensorDataObject()->GetSensorStaticCapability(&sensorStaticCaps, cameraID);

        m_sensorActiveArrayWidth  = sensorStaticCaps.activeArraySize.width;
        m_sensorActiveArrayHeight = sensorStaticCaps.activeArraySize.height;

        m_statsSession.Initialize(pInitializeData);
    }

    if (CamxResultSuccess == result)
    {
        m_pStatsAFPropertyReadWrite = CAMX_NEW StatsPropertyReadAndWrite(pInitializeData);
    }

    InitializeReadProperties();
    InitializeWriteProperties();

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::InitializeReadProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::InitializeReadProperties()
{
    m_AFReadProperties[AFReadTypePropertyIDUsecaseChiTuningModeParameter] =
    {
        PropertyIDUsecaseChiTuningModeParameter,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypeInputScalerCropRegion] =
    {
        InputScalerCropRegion,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDParsedBFStatsOutput] =
    {
        PropertyIDParsedBFStatsOutput,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDISPHDRBEConfig] =
    {
        PropertyIDISPHDRBEConfig,
        m_pNode->GetMaximumPipelineDelay() + 1,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDParsedHDRBEStatsOutput] =
    {
        PropertyIDParsedHDRBEStatsOutput,
        1,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDAECInternal] =
    {
        PropertyIDAECInternal,
        1,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDAECFrameInfo] =
    {
        PropertyIDAECFrameInfo,
        1,
        NULL
    };
    m_AFReadProperties[AFReadTypeSensorFrameDuration] =
    {
        SensorFrameDuration,
        1,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDAFPDFrameInfo] =
    {
        PropertyIDAFPDFrameInfo,
        1,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDUsecaseSensorCurrentMode] =
    {
        PropertyIDUsecaseSensorCurrentMode,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDUsecaseLensInfo] =
    {
        PropertyIDUsecaseLensInfo,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDUsecaseSensorModes] =
    {
        PropertyIDUsecaseSensorModes,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDUsecaseIFEInputResolution] =
    {
        PropertyIDUsecaseIFEInputResolution,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypeInputControlAFMode] =
    {
        InputControlAFMode,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypeInputControlSceneMode] =
    {
        InputControlSceneMode,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypeInputControlCaptureIntent] =
    {
        InputControlCaptureIntent,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypeInputControlAFRegions] =
    {
        InputControlAFRegions,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypeInputControlAFTrigger] =
    {
        InputControlAFTrigger,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypeInputLensFocusDistance] =
    {
        InputLensFocusDistance,
        0,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDAFFrameControl] =
    {
        PropertyIDAFFrameControl,
        1,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDAFFrameInfo] =
    {
        PropertyIDAFFrameInfo,
        1,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDAFStatsControl] =
    {
        PropertyIDAFStatsControl,
        1,
        NULL
    };
    m_AFReadProperties[AFReadTypePropertyIDDebugDataAll] =
    {
        PropertyIDDebugDataAll,
        0,
        NULL
    };


    m_pStatsAFPropertyReadWrite->SetReadProperties(m_AFReadProperties);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::InitializeWriteProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::InitializeWriteProperties()
{
    m_AFWriteProperties[0] =
    {
        PropertyIDAFFrameControl,
        sizeof(m_frameControl),
        &m_frameControl,
    };
    m_AFWriteProperties[1] =
    {
        PropertyIDAFStatsControl,
        sizeof(m_statsControl),
        &m_statsControl,
    };
    m_AFWriteProperties[2] =
    {
        PropertyIDAFFrameInfo,
        sizeof(m_AFFrameInfo),
        &m_AFFrameInfo,
    };
    m_AFWriteProperties[3] =
    {
        ControlAFState,
        sizeof(ControlAFStateValues) / sizeof(INT32),
        &m_AFHALData.afState,
    };
    m_AFWriteProperties[4] =
    {
        ControlAFMode,
        sizeof(ControlAFModeValues) / sizeof(INT32),
        &m_AFHALData.afMode,
    };
    m_AFWriteProperties[5] =
    {
        ControlAFRegions,
        sizeof(m_AFHALData.weightedRegion) / sizeof(INT32),
        &m_AFHALData.weightedRegion,
    };
    m_AFWriteProperties[6] =
    {
        LensFocusDistance,
        sizeof(m_focusDistanceInfo) / sizeof(FLOAT),
        &m_AFHALData.lensFocusDistance,
    };
    m_AFWriteProperties[7] =
    {
        ControlAFTrigger,
        sizeof(ControlAFTriggerValues) / sizeof(INT32),
        &m_AFHALData.afTriggerValues,
    };

    m_pStatsAFPropertyReadWrite->SetWriteProperties(m_AFWriteProperties);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadMultiCameraInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::ReadMultiCameraInfo(
    StatsCameraInfo* pStatsCameraInfo)
{
    CamxResult result = CamxResultSuccess;
    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    if ((TRUE == pStaticSettings->multiCameraEnable) && (MultiCamera3ASyncDisabled != pStaticSettings->multiCamera3ASync))
    {
        MultiCameraIdRole* pMCCInfo  = NULL;
        BOOL*              pIsMaster = NULL;

        // Query information sent by multi camera controller
        StatsUtil::ReadVendorTag(m_pNode, "com.qti.chi.multicamerainfo", "MultiCameraIdRole",
            reinterpret_cast<VOID**>(&pMCCInfo));
        StatsUtil::ReadVendorTag(m_pNode, "com.qti.chi.multicamerainfo", "MasterCamera",
            reinterpret_cast<VOID**>(&pIsMaster));

        if (NULL != pMCCInfo && NULL != pIsMaster)
        {
            pStatsCameraInfo->cameraId = pMCCInfo->currentCameraId;
            switch (pMCCInfo->currentCameraRole)
            {
                case CameraRoleTypeUltraWide:
                    pStatsCameraInfo->cameraType = StatsCameraTypeUltraWide;
                    break;
                case CameraRoleTypeWide:
                    pStatsCameraInfo->cameraType = StatsCameraTypeWide;
                    break;
                case CameraRoleTypeTele:
                    pStatsCameraInfo->cameraType = StatsCameraTypeTele;
                    break;
                case CameraRoleTypeVR:
                case CameraRoleTypeDefault:
                    pStatsCameraInfo->cameraType = StatsCameraTypeDefault;
                    break;
                default:
                    CAMX_LOG_ERROR(CamxLogGroupAF, "Invalid camera type");
                    pStatsCameraInfo->cameraType = StatsCameraTypeDefault;
                    break;
            }

            if (StatsCameraTypeDefault == pStatsCameraInfo->cameraType)
            {
                pStatsCameraInfo->algoRole = StatsAlgoRoleDefault;
            }
            else
            {
                pStatsCameraInfo->algoRole =
                    (*pIsMaster) ? StatsAlgoRoleMaster : StatsAlgoRoleSlave;
            }
        }
        else
        {
            CAMX_LOG_INFO(CamxLogGroupAF, "Multi camera metadata not published");
        }
    }
    else
    {
        pStatsCameraInfo->cameraId      = m_cameraID;
        pStatsCameraInfo->algoRole      = StatsAlgoRoleDefault;
        pStatsCameraInfo->cameraType    = StatsCameraTypeDefault;
    }

    SetCameraInfo(pStatsCameraInfo);

    CAMX_LOG_INFO(CamxLogGroupAF, "AF Camera info:ID:%d type:%d role:%d",
        pStatsCameraInfo->cameraId,
        pStatsCameraInfo->cameraType,
        pStatsCameraInfo->algoRole);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::SetStatsParser
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::SetStatsParser(
    StatsParser* pStatsParser)
{
    m_pStatsParser = pStatsParser;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::SetCameraInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::SetCameraInfo(
    StatsCameraInfo* pCameraInfo)
{
    m_cameraInfo = *pCameraInfo;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetCameraInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::GetCameraInfo(
    StatsCameraInfo* pCameraInfo)
{
    *pCameraInfo = m_cameraInfo;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetGyroData
//
// @brief  Function to get the gyro data
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const AFGyroData CAFIOUtil::GetGyroData()
{
    return m_AFOutputGyroValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetGravityData
//
// @brief  Function to get the gravity data
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const AFGravityData CAFIOUtil::GetGravityData()
{
    return m_AFOutputGravityValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetTOFData
//
// @brief  Function to get the TOF data
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const AFTOFData CAFIOUtil::GetTOFData()
{
    return m_AFOutputTOFValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::RetrieveBFStat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::RetrieveBFStat(
    ParsedBFStatsOutput* pParsedBFStatsOutput,
    AFBAFStatistics*     pBFStat)
{
    CamxResult result = CamxResultEFailed;
    if (NULL != pParsedBFStatsOutput)
    {
        pBFStat->pH1Number      = pParsedBFStatsOutput->horizontal1Num;
        pBFStat->pH1Sharpness   = pParsedBFStatsOutput->horizontal1Sharpness;
        pBFStat->pH1Sum         = pParsedBFStatsOutput->horizontal1Sum;
        pBFStat->pH2Number      = pParsedBFStatsOutput->horizontal2Num;
        pBFStat->pH2Sharpness   = pParsedBFStatsOutput->horizontal2Sharpness;
        pBFStat->pH2Sum         = pParsedBFStatsOutput->horizontal2Sum;
        pBFStat->pVNumber       = pParsedBFStatsOutput->verticalNum;
        pBFStat->pVSharpness    = pParsedBFStatsOutput->verticalSharpness;
        pBFStat->pVSum          = pParsedBFStatsOutput->verticalSum;
        pBFStat->numberOfBAFROI = pParsedBFStatsOutput->numOfROIRegions;

        result = CamxResultSuccess;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetDebugData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::GetDebugData(
    UINT64      requestId,
    DebugData** ppDebugDataOut)
{
    if (NULL != ppDebugDataOut)
    {
        (void)StatsUtil::GetDebugDataBuffer(m_pDebugDataPool, requestId, PropertyIDDebugDataAF, ppDebugDataOut);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::RetrieveBGStat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::RetrieveBGStat(
    PropertyISPHDRBEStats*  pISPHDRStats,
    ParsedHDRBEStatsOutput* pHDRBEOutput,
    StatsBayerGrid*         pBayerGrid)
{
    CAMX_ENTRYEXIT(CamxLogGroupAF);

    CamxResult  result = CamxResultSuccess;
    BGBEConfig* pHDRBEConfig = &pISPHDRStats->statsConfig.HDRBEConfig;

    pBayerGrid->horizontalRegionCount   = pHDRBEConfig->horizontalNum;
    pBayerGrid->verticalRegionCount     = pHDRBEConfig->verticalNum;
    pBayerGrid->totalRegionCount        = (pHDRBEConfig->horizontalNum * pHDRBEConfig->verticalNum);
    pBayerGrid->regionWidth             = pISPHDRStats->statsConfig.regionWidth;
    pBayerGrid->regionHeight            = pISPHDRStats->statsConfig.regionHeight;
    pBayerGrid->bitDepth                = IFEPipelineBitWidth;

    CAMX_STATIC_ASSERT(sizeof(pBayerGrid->satThreshold) == sizeof(pHDRBEConfig->channelGainThreshold));
    CamX::Utils::Memcpy(pBayerGrid->satThreshold,
        pHDRBEConfig->channelGainThreshold,
        sizeof(pBayerGrid->satThreshold));

    pBayerGrid->flags.hasSatInfo = pHDRBEOutput->flags.hasSatInfo;
    pBayerGrid->flags.usesY = pHDRBEOutput->flags.usesY;
    pBayerGrid->numValidRegions = pHDRBEOutput->numROIs;

    CAMX_STATIC_ASSERT(sizeof(*pBayerGrid->GetChannelData(0)) == sizeof(pHDRBEOutput->GetChannelData(0)));
    pBayerGrid->SetChannelDataArray(reinterpret_cast<StatsBayerGridChannelInfo*>(pHDRBEOutput->GetChannelDataArray()));

    CAMX_LOG_VERBOSE(CamxLogGroupAF, "numvalid %d, totalreg %d, h %d, v %d", pBayerGrid->numValidRegions,
        pBayerGrid->totalRegionCount, pBayerGrid->horizontalRegionCount, pBayerGrid->verticalRegionCount);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::RetrieveAECFrameInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::RetrieveAECFrameInfo(
    AECFrameInformation* pAECFrameInfo,
    AFAECInfo*           pAECInfo)
{
    pAECInfo->convergePercent   = pAECFrameInfo->AECSettled ? 100.0f : 0.0f;
    pAECInfo->luxIndex          = pAECFrameInfo->luxIndex;
    pAECInfo->exposureIndex     = static_cast<UINT32>(pAECFrameInfo->luxIndex);
    pAECInfo->currentGain       = pAECFrameInfo->exposureInfo[ExposureIndexShort].linearGain;
    pAECInfo->exposureTime      = pAECFrameInfo->exposureInfo[ExposureIndexShort].exposureTime;
    pAECInfo->isLEDAssisted     = (pAECFrameInfo->AECPreFlashState == PreFlashStateTriggerAF) ? TRUE : FALSE;
    pAECInfo->AECPreFlashState  = pAECFrameInfo->AECPreFlashState;
    pAECInfo->currentLuma       = pAECFrameInfo->frameLuma;
    CAMX_LOG_VERBOSE(CamxLogGroupStats, "LED AF: %d, AECPreFlashState %d",
                     pAECInfo->isLEDAssisted,
                     pAECInfo->AECPreFlashState);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::RetrieveFocusRegions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::RetrieveFocusRegions(
    WeightedRegion* pWeightedRegion,
    AFROIInfo*      pAFROI)
{

    if (NULL != pWeightedRegion)
    {
        if (0 == pWeightedRegion->weight)
        {
            pAFROI->ROIType     = AFROITypeGeneral;
            pAFROI->numberOfROI = 0;
            Utils::Memset(pAFROI->pWeightedROI, 0, (m_maxFocusRegions * sizeof(StatsWeightedROI)));
        }
        else
        {
            pAFROI->ROIType     = AFROITypeTouch;
            pAFROI->numberOfROI = m_maxFocusRegions;

            // Read scale & crop params
            WeightedRegion AFRegionScale                = {0};
            CropWindow     HALCropWindow                = { 0 };
            static const UINT StatsData[] =
            {
                InputScalerCropRegion,
            };

            static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
            VOID*             pData[StatsLength] = { 0 };
            pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputScalerCropRegion);

            if (NULL != pData[0])
            {
                HALCropWindow    = (*reinterpret_cast<CropWindow*>(pData[0]));
                CAMX_LOG_VERBOSE(CamxLogGroupAF,
                    "INPUT HALCropWindow (L:%u,T:%u,W:%u,H:%u)",
                    HALCropWindow.left,
                    HALCropWindow.top,
                    HALCropWindow.width,
                    HALCropWindow.height);
            }


            FLOAT widthRatio =  static_cast<FLOAT>(m_sensorInfoInput.resolutionWidth) /
                static_cast<FLOAT>(m_sensorActiveArrayWidth);

            FLOAT heightRatio = static_cast<FLOAT>(m_sensorInfoInput.resolutionHeight) /
                static_cast<FLOAT>(m_sensorActiveArrayHeight);


            for (UINT32 i = 0; i < pAFROI->numberOfROI; i++)
            {
                BOOL isOverlappingRegion = TRUE;
                // According to Android Documentation. If the AF region is entirely outside the crop region,
                // it should be ignored and not reported in the result metadata. We will write zeros to it

                // If Crop rect is top or below AF Region rect
                if (HALCropWindow.top > pWeightedRegion[i].yMax ||
                    pWeightedRegion[i].yMin > (HALCropWindow.top + HALCropWindow.height))
                {
                    isOverlappingRegion = FALSE;
                }

                // If Crop rectangle is to the left or right of AF Region
                if (HALCropWindow.left > pWeightedRegion[i].xMax ||
                    pWeightedRegion[i].xMin > (HALCropWindow.left + HALCropWindow.width))
                {
                    isOverlappingRegion = FALSE;
                }

                CAMX_LOG_VERBOSE(CamxLogGroupAF,
                    "AF REGION ROI Weight %d (xmin:%u,ymin:%u,xmax:%u,ymax:%u)",
                    pWeightedRegion[i].weight,
                    pWeightedRegion[i].xMin,
                    pWeightedRegion[i].yMin,
                    pWeightedRegion[i].xMax,
                    pWeightedRegion[i].yMax);

                // If there is no overlap then write zeros to AF Region
                if (isOverlappingRegion == FALSE)
                {
                    pAFROI->ROIType                     = AFROITypeGeneral;
                    pAFROI->numberOfROI                 = 0;
                    pAFROI->pWeightedROI[i].ROIWeight   = 0;
                    pAFROI->pWeightedROI[i].ROI.left    = 0;
                    pAFROI->pWeightedROI[i].ROI.top     = 0;
                    pAFROI->pWeightedROI[i].ROI.width   = 0;
                    pAFROI->pWeightedROI[i].ROI.height  = 0;
                    CAMX_LOG_VERBOSE(CamxLogGroupAF, "AF REGION and Crop Region do not overlap. Writing zeros to AF REGION");
                }
                else
                {
                    // take intersection of AF region and cropScale
                    // Touch ROI is already scaled as per zoomfactor by APP
                    // All co-ordinates are in active pixel array
                    AFRegionScale.weight                = pWeightedRegion[i].weight;
                    AFRegionScale.xMin                  = Utils::MaxINT32(HALCropWindow.left, pWeightedRegion[i].xMin);
                    AFRegionScale.yMin                  = Utils::MaxINT32(HALCropWindow.top, pWeightedRegion[i].yMin);
                    AFRegionScale.xMax                  =
                        Utils::MinINT32((HALCropWindow.left + HALCropWindow.width), pWeightedRegion[i].xMax);
                    AFRegionScale.yMax                  =
                        Utils::MinINT32((HALCropWindow.top + HALCropWindow.height), pWeightedRegion[i].yMax);

                    // Convert into CAMIF co-ordinates
                    pAFROI->pWeightedROI[i].ROIWeight   = AFRegionScale.weight;
                    pAFROI->pWeightedROI[i].ROI.left    = Utils::RoundFLOAT(AFRegionScale.xMin * widthRatio);
                    pAFROI->pWeightedROI[i].ROI.top     = Utils::RoundFLOAT(AFRegionScale.yMin * heightRatio);
                    pAFROI->pWeightedROI[i].ROI.width   =
                        Utils::RoundFLOAT((AFRegionScale.xMax - AFRegionScale.xMin) * widthRatio);

                    pAFROI->pWeightedROI[i].ROI.height  =
                        Utils::RoundFLOAT((AFRegionScale.yMax - AFRegionScale.yMin) * heightRatio);
                    CAMX_LOG_VERBOSE(CamxLogGroupAF,
                        "ROI type %d ROIWeight %d (L:%u,T:%u,W:%u,H:%u)",
                        pAFROI->ROIType,
                        pWeightedRegion->weight,
                        pAFROI->pWeightedROI[i].ROI.left,
                        pAFROI->pWeightedROI[i].ROI.top,
                        pAFROI->pWeightedROI[i].ROI.width,
                        pAFROI->pWeightedROI[i].ROI.height);
                }
            }


        }
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::RetrieveFaceRegions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::RetrieveFaceRegions(
    FaceROIInformation*  pFaceROIInfo,
    AFFaceROIInfo*       pAFFaceROI,
    UnstabilizedFaceROIInformation* pUnstabilizedFaceROIInfo)
{
    pAFFaceROI->count = Utils::MinUINT32(MaxFaceROIs, pFaceROIInfo->ROICount);

    // When face is lost and the we need to clear the FD ROI
    // structure to avoid stale data
    if (pAFFaceROI->count == 0)
    {
        Utils::Memset(pAFFaceROI, 0, sizeof(AFFaceROIInfo));
    }

    for (UINT32 i = 0; i < pFaceROIInfo->ROICount; i++)
    {
        pAFFaceROI->weightedROI[i].ROIWeight  = 0;
        pAFFaceROI->id[i]                     = pFaceROIInfo->stabilizedROI[i].id;
        pAFFaceROI->weightedROI[i].ROI.left   = static_cast<INT32>(pFaceROIInfo->stabilizedROI[i].faceRect.left);
        pAFFaceROI->weightedROI[i].ROI.top    = static_cast<INT32>(pFaceROIInfo->stabilizedROI[i].faceRect.top);
        pAFFaceROI->weightedROI[i].ROI.width  = static_cast<INT32>(pFaceROIInfo->stabilizedROI[i].faceRect.width);
        pAFFaceROI->weightedROI[i].ROI.height = static_cast<INT32>(pFaceROIInfo->stabilizedROI[i].faceRect.height);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "Face ROI[%u]: face-id: %u (left: %d, top: %d, width: %d, height: %d)",
            i,
            pAFFaceROI->id[i],
            pAFFaceROI->weightedROI[i].ROI.left,
            pAFFaceROI->weightedROI[i].ROI.top,
            pAFFaceROI->weightedROI[i].ROI.width,
            pAFFaceROI->weightedROI[i].ROI.height);
    }

    // new face interface for uniform 3A ROI
    pUnstabilizedFaceROIInfo->requestID = pFaceROIInfo->requestId;
    pUnstabilizedFaceROIInfo->roiCount  = Utils::MinUINT32(MaxFaceROIs, pFaceROIInfo->ROICount);

    if (pUnstabilizedFaceROIInfo->roiCount == 0)
    {
        Utils::Memset(pUnstabilizedFaceROIInfo, 0, sizeof(UnstabilizedFaceROIInformation));
    }

    for (UINT32 i = 0; i < pUnstabilizedFaceROIInfo->roiCount; i++)
    {
        pUnstabilizedFaceROIInfo->roi[i].roiID = pFaceROIInfo->unstabilizedROI[i].id;
        pUnstabilizedFaceROIInfo->roi[i].weight = 0;
        pUnstabilizedFaceROIInfo->roi[i].rect.left = static_cast<INT32>(pFaceROIInfo->unstabilizedROI[i].faceRect.left);
        pUnstabilizedFaceROIInfo->roi[i].rect.top = static_cast<INT32>(pFaceROIInfo->unstabilizedROI[i].faceRect.top);
        pUnstabilizedFaceROIInfo->roi[i].rect.width = static_cast<INT32>(pFaceROIInfo->unstabilizedROI[i].faceRect.width);
        pUnstabilizedFaceROIInfo->roi[i].rect.height = static_cast<INT32>(pFaceROIInfo->unstabilizedROI[i].faceRect.height);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "Face ROI[%u]: face-id: %u (left: %d, top: %d, width: %d, height: %d)",
            i,
            pUnstabilizedFaceROIInfo->roi[i].roiID,
            pUnstabilizedFaceROIInfo->roi[i].rect.left,
            pUnstabilizedFaceROIInfo->roi[i].rect.top,
            pUnstabilizedFaceROIInfo->roi[i].rect.width,
            pUnstabilizedFaceROIInfo->roi[i].rect.height);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetCropWindowInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::GetCropWindowInfo(
    StatsRectangle*     pStatsRectangle)
{
    CropWindow          HALCropWindow           = { 0 };

    FLOAT widthRatio  = static_cast<FLOAT>(m_sensorInfoInput.resolutionWidth) / (m_sensorActiveArrayWidth);
    FLOAT heightRatio = static_cast<FLOAT>(m_sensorInfoInput.resolutionHeight) / (m_sensorActiveArrayHeight);
    static const UINT StatsData[] =
    {
        InputScalerCropRegion,
    };

    static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
    VOID*             pData[StatsLength] = { 0 };
    pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputScalerCropRegion);

    if (NULL != pData[0])
    {
        HALCropWindow    = (*reinterpret_cast<CropWindow*>(pData[0]));
    }

    if (HALCropWindow.width != 0 &&
        HALCropWindow.height != 0)
    {
        // mapping crop window to CAMIF size from Sensor Active pixel size
        pStatsRectangle->width  = Utils::RoundFLOAT(HALCropWindow.width * widthRatio);
        pStatsRectangle->height = Utils::RoundFLOAT(HALCropWindow.height * heightRatio);
        pStatsRectangle->left   = Utils::RoundFLOAT(HALCropWindow.left * widthRatio);
        pStatsRectangle->top    = Utils::RoundFLOAT(HALCropWindow.top * heightRatio);
    }
    else
    {
        pStatsRectangle->left   = 0;
        pStatsRectangle->top    = 0;
        pStatsRectangle->width  = m_sensorActiveArrayWidth;
        pStatsRectangle->height = m_sensorActiveArrayHeight;
        CAMX_LOG_WARN(CamxLogGroupAF, "Wrong input: HALCropWindow width,height=%d %d",
            HALCropWindow.width, HALCropWindow.height);
    }

    CAMX_LOG_VERBOSE(CamxLogGroupAF,
        "Stats crop window: HALCrop %d %d %d %d, AdjustCrop %d %d %d %d, SensorRes %d %d, ActiveArray %d %d, w h ratio %f %f",
        HALCropWindow.left,
        HALCropWindow.top,
        HALCropWindow.width,
        HALCropWindow.height,
        pStatsRectangle->left,
        pStatsRectangle->top,
        pStatsRectangle->width,
        pStatsRectangle->height,
        m_sensorInfoInput.resolutionWidth,
        m_sensorInfoInput.resolutionHeight,
        m_sensorActiveArrayWidth,
        m_sensorActiveArrayHeight,
        widthRatio,
        heightRatio);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::MapFocusModeToHALType
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::MapFocusModeToHALType(
    const AFFocusMode*   pAFMode,
    ControlAFModeValues* pHALAFModeType)
{

    CAMX_ASSERT_MESSAGE(NULL != pAFMode, "AF Mode NULL pointer");
    CAMX_ASSERT_MESSAGE(NULL != pHALAFModeType, "HAL AF Mode NULL pointer");

    switch (*pAFMode)
    {
        case AFFocusModeManual:
            *pHALAFModeType = ControlAFModeOff;
            break;
        case AFFocusModeAuto:
            *pHALAFModeType = ControlAFModeAuto;
            break;
        case AFFocusModeMacro:
            *pHALAFModeType = ControlAFModeMacro;
            break;
        // Intentional fallthrough
        case AFFocusModeContinuousVideo:
        case AFFocusModeContinuousPicture:
            if (m_isVideoCAFMode == TRUE)
            {
                *pHALAFModeType = ControlAFModeContinuousVideo;
            }
            else
            {
                *pHALAFModeType = ControlAFModeContinuousPicture;
            }
            break;
        case AFFocusModeInifinity:
            *pHALAFModeType = ControlAFModeEdof;
            break;
        default:
            *pHALAFModeType = ControlAFModeEnd;
            break;
    }
    CAMX_LOG_VERBOSE(CamxLogGroupAF, "HALFocusMode = %d, AlgoFocusMode = %d, m_isVideoCAFMode = %d",
                                       *pHALAFModeType, *pAFMode, m_isVideoCAFMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadBFStat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::ReadBFStat(
    AFBAFStatistics* pBFStats)
{
    CamxResult result                           = CamxResultEFailed;
    static const UINT StatsData[] =
    {
        PropertyIDParsedBFStatsOutput,
    };

    static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
    VOID*             pData[StatsLength] = { 0 };
    pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDParsedBFStatsOutput);

    if (NULL != pData[0])
    {
        result = RetrieveBFStat(reinterpret_cast<ParsedBFStatsOutput*>(*static_cast<VOID**>(pData[0])), pBFStats);
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupAF,
            "PropertyID %d (ISP BFStats) has not been published! ",
            PropertyIDParsedBFStatsOutput);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadBGStat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::ReadBGStat(
    StatsBayerGrid* pBGStats)
{
    CAMX_LOG_VERBOSE(CamxLogGroupAF, "ReadBGStat enter");
    CamxResult result = CamxResultEFailed;
    static const UINT StatsData[] =
    {
        PropertyIDISPHDRBEConfig,
        PropertyIDParsedHDRBEStatsOutput
    };

    static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
    VOID*             pData[StatsLength] = { 0 };
    pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDISPHDRBEConfig);
    pData[1] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDParsedHDRBEStatsOutput);

    // Check for presence of HDRBEStats
    if (NULL != pData[1])
    {
        if (NULL != pData[0])
        {
            // Save the BHist stats inside core's input structure
            result = RetrieveBGStat(
                reinterpret_cast<PropertyISPHDRBEStats*>(pData[0]),
                reinterpret_cast<ParsedHDRBEStatsOutput*>(*static_cast<VOID**>(pData[1])),
                pBGStats);
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupAF,
                "PropertyID %d (ISP HDR-BE Config) has not been published! ",
                PropertyIDISPHDRBEConfig);
        }
    }
    else
    {
        /// @todo (CAMX-523): Fix the comment after ISP BHist stats are available
        CAMX_LOG_WARN(CamxLogGroupAF,
            "PropertyID %d (AEC HDR-BE stats) has not been published! "
            "HDRBEStats are not available yet!",
            PropertyIDParsedHDRBEStatsOutput);
        /// @todo  (CAMX-523): determine if this should be considered a failure or not; for now consider success
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadVendorTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::ReadVendorTag(
    StatsVendorTagList* pVendorTagList)
{
    for (UINT32 i = 0; i < m_vendorTagInputList.vendorTagCount; i++)
    {
        UINT32 tagId = m_vendorTagInputList.vendorTag[i].vendorTagId;

        CAMX_ASSERT(0 == (tagId & InputMetadataSectionMask)); // Ensures the metadata tags arent overlapping our mask use

        // have to add the InputMetadataSectionMask to ensure the input pool is used for read
        UINT              GetProps[]                = {tagId | InputMetadataSectionMask};
        static const UINT GetPropsLength            = CAMX_ARRAY_SIZE(GetProps);
        VOID*             pData[GetPropsLength]     = { 0 };
        UINT64            offsets[GetPropsLength]   = { 0 };

        m_pNode->GetDataList(GetProps, pData, offsets, GetPropsLength);

        pVendorTagList->vendorTag[i].vendorTagId        = tagId;
        pVendorTagList->vendorTag[i].dataSize           = static_cast<UINT32>(HAL3MetadataUtil::GetMaxSizeByTag(tagId));
        pVendorTagList->vendorTag[i].pData              = pData[0];
        pVendorTagList->vendorTag[i].appliedDelay       = 0;
        pVendorTagList->vendorTag[i].sizeOfWrittenData  = 0;

        if ((pVendorTagList->vendorTag[i].pData != NULL) && (pVendorTagList->vendorTag[i].dataSize > 0))
        {
            pVendorTagList->vendorTagCount++;
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupAF,
                           "Failed to retrieve input vendor tag data. Id: %u pData:%p dataSize:%u",
                            tagId,
                            pVendorTagList->vendorTag[i].pData,
                            pVendorTagList->vendorTag[i].dataSize);

            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadAECInput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::ReadAECInput(
    AFAECInfo* pAECInfo)
{
    CamxResult        result                  = CamxResultEFailed;
    static const UINT StatsData[] =
    {
        PropertyIDAECInternal,
        PropertyIDAECFrameInfo,
        SensorFrameDuration
    };

    static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
    VOID*             pData[StatsLength] = { 0 };
    pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDAECInternal);
    pData[1] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDAECFrameInfo);
    pData[2] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeSensorFrameDuration);

    if (NULL != pData[0])
    {
        AECOutputInternal* pAECPropertyPool = reinterpret_cast<AECOutputInternal*>(pData[0]);

        pAECInfo->brightnessValue   = static_cast<FLOAT>(pAECPropertyPool->APEXValues.brightness);
        pAECInfo->apertureValue     = static_cast<FLOAT>(pAECPropertyPool->APEXValues.aperture);
        pAECInfo->exposureValue     = static_cast<FLOAT>(pAECPropertyPool->APEXValues.exposure);
        pAECInfo->pLumaValues       = pAECPropertyPool->legacyYStats;
        pAECInfo->lumaValuesCount   = sizeof(pAECPropertyPool->legacyYStats) / sizeof(FLOAT);
        result = CamxResultSuccess;
    }

    if (NULL != pData[1])
    {
        RetrieveAECFrameInfo(reinterpret_cast<AECFrameInformation*>(pData[1]), pAECInfo);
    }
    else
    {
        result = CamxResultEFailed;
    }

    if (NULL != pData[2])
    {
        UINT64 frameDuration = *reinterpret_cast<UINT64*>(pData[2]);
        // Convert frameduration that is in nanoseconds to FPS
        pAECInfo->currentFPS = static_cast<INT>(static_cast<DOUBLE>(NanoSecondsPerSecond/frameDuration));
    }
    else
    {
        result = CamxResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadPDAFDataInput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
AFPDAFData* CAFIOUtil::ReadPDAFDataInput()
{
    // This method will be called just in early interrupt after a call to PDLib
    // m_PDAFDataInput include PDLib output of processing current frame early interrupt
    Utils::Memcpy(&m_PDAFDataInputs[PDTriggerEarly], &m_PDAFDataInput, sizeof(m_PDAFDataInput));
    static const UINT StatsData[] =
    {
        PropertyIDAFPDFrameInfo,
    };

    static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
    VOID*             pData[StatsLength] = { 0 };
    pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDAFPDFrameInfo);

    // Reading previous frame end of frame PDstats from main property pool
    // In publishPDLibOutput previous EOD PD stat has been published to previous main property pool

    if (NULL != pData[0])
    {
        Utils::Memcpy(&m_PDAFDataInputs[PDTriggerNormal], pData[0], sizeof(m_PDAFDataInput));
    }
    else
    {
        CAMX_LOG_INFO(CamxLogGroupAF, "m_pStatsAFPropertyReadWrite->GetPropertyData(8) for PDNormal is null");
        Utils::Memset(&m_PDAFDataInputs[PDTriggerNormal], 0, sizeof(m_PDAFDataInputs[PDTriggerNormal]));
    }

    // m_PDAFDataInputs is an array of two. It now contains current frame early interrupt PDLib output as first element and
    // and Previous frame EOD PDLib output as its second element.
    return &m_PDAFDataInputs[PDTriggerEarly];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadSensorInput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::ReadSensorInput(
    AFSensorInfo* pSensorInfo)
{

    UINT                sensorModeIndex = 1; // Default sensor mode index to use for initial frames before sensor publishes
    LensInfo*           pLensInfo       = reinterpret_cast<LensInfo*>(
        m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDUsecaseLensInfo));
    UsecaseSensorModes* pSensorModes    =
        reinterpret_cast<UsecaseSensorModes*>(
            m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDUsecaseSensorModes));
    IFEInputResolution* pIFEInput       =
        reinterpret_cast<IFEInputResolution*>(
            m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDUsecaseIFEInputResolution));

    if (NULL != m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDUsecaseSensorCurrentMode))
    {
        sensorModeIndex = *reinterpret_cast<UINT*>(
            m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDUsecaseSensorCurrentMode));
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "PropertyIDUsecaseSensorCurrentMode is NULL");
    }

    if (NULL != pLensInfo)
    {
        pSensorInfo->actuatorSensitivity    = pLensInfo->actuatorSensitivity;
        pSensorInfo->fNumber                = pLensInfo->fNumber;
        pSensorInfo->focalLength            = pLensInfo->focalLength;
        pSensorInfo->pixelSize              = pLensInfo->pixelSize;
        pSensorInfo->totalFDistance         = pLensInfo->totalFDistance;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "PropertyIDUsecaseLensInfo is NULL");
    }

    if (NULL != pIFEInput)
    {
        pSensorInfo->resolutionHeight       =
           static_cast<UINT32>(pIFEInput->resolution.height);
        pSensorInfo->resolutionWidth        =
            static_cast<UINT32>(pIFEInput->resolution.width);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "PropertyIDUsecaseIFEInputResolution is NULL");
    }

    if (NULL != pSensorModes)
    {
        pSensorInfo->cropFirstLine =
            static_cast<UINT32>(pSensorModes->allModes[m_sensorModeIndex].cropInfo.firstLine);
        pSensorInfo->cropLastLine =
            static_cast<UINT32>(pSensorModes->allModes[m_sensorModeIndex].cropInfo.lastLine);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "PropertyIDUsecaseSensorModes is NULL");
    }

    pSensorInfo->isPDAFEnabled = m_isPDAFEnabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadHardwareCapability
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
AFHardwareCapability* CAFIOUtil::ReadHardwareCapability()
{
    /// @todo  (CAMX-523): Fill in the data here once it's available in the property pool
    m_kernelInfoInput[0].mask                   = 3;
    m_kernelInfoInput[0].FIRKernelCount         = 13;
    m_kernelInfoInput[0].IIRKernelACount        = 4;
    m_kernelInfoInput[0].IIRKernelBCount        = 6;
    m_kernelInfoInput[1].mask                   = 3;
    m_kernelInfoInput[1].FIRKernelCount         = 0;
    m_kernelInfoInput[1].IIRKernelACount        = 4;
    m_kernelInfoInput[1].IIRKernelBCount        = 6;
    m_floatingWindowInfo.maxWindowCount         = 180;
    m_floatingWindowInfo.maxWindowWidth         = 2672;
    m_floatingWindowInfo.maxWindowHeight        = 2008;
    m_floatingWindowInfo.maxHorizontalOffset    = static_cast<UINT16>(m_sensorInfoInput.resolutionWidth - 12);
    m_floatingWindowInfo.maxVerticalOffset      = static_cast<UINT16>(m_sensorInfoInput.resolutionHeight);
    m_floatingWindowInfo.minHorizontalOffset    = 76;
    m_floatingWindowInfo.minVerticalOffset      = 64;
    m_hardwareCapability.kernelCount            = 2;
    m_hardwareCapability.pKernelInfo            = m_kernelInfoInput;
    m_hardwareCapability.pWindowInfo            = &m_floatingWindowInfo;

    return &m_hardwareCapability;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadFocusMode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::ReadFocusMode(
    AFFocusMode* pAFMode)
{
    static const UINT StatsData[] =
    {
        InputControlAFMode,
        InputControlSceneMode
    };

    static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
    VOID*             pData[StatsLength] = { 0 };
    pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputControlAFMode);
    pData[1] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputControlSceneMode);

    if (NULL != pData[0] && NULL != pData[1])
    {
        switch (*reinterpret_cast<ControlAFModeValues*>(pData[0]))
        {
            case ControlAFModeOff:
                *pAFMode = AFFocusModeManual;
                break;
            case ControlAFModeAuto:
                *pAFMode = AFFocusModeAuto;
                break;
            case ControlAFModeMacro:
                *pAFMode = AFFocusModeMacro;
                break;
            case ControlAFModeContinuousVideo:
                m_isVideoCAFMode = TRUE;
                *pAFMode = AFFocusModeContinuousVideo;
                break;
            case ControlAFModeContinuousPicture:
                m_isVideoCAFMode = FALSE;
                *pAFMode = AFFocusModeContinuousPicture;
                break;
            case ControlAFModeEdof:
                *pAFMode = AFFocusModeInifinity;
                break;
            default:
                CAMX_ASSERT_ALWAYS_MESSAGE("Invalid AF mode: %d", *reinterpret_cast<ControlAFModeValues*>(pData[0]));
                *pAFMode = AFFocusModeInvalid;
                break;
        }

        m_controlSceneMode = *reinterpret_cast<ControlSceneModeValues*>(pData[1]);

        switch (m_controlSceneMode)
        {
            case ControlSceneModeAction:
            case ControlSceneModeTheatre:
            case ControlSceneModeSports:
                // Set focus mode to AUTO
                *pAFMode = AFFocusModeAuto;
                break;
            case ControlSceneModeLandscape:
            case ControlSceneModeSunset:
                // Set focus mode to INFINITY
                *pAFMode = AFFocusModeInifinity;
                break;
            case ControlSceneModeParty:
            case ControlSceneModeSteadyphoto:
            case ControlSceneModeBeach:
            case ControlSceneModeSnow:
            case ControlSceneModeCandlelight:
            case ControlSceneModePortrait:
            case ControlSceneModeNight:
            case ControlSceneModeNightPortrait:
            case ControlSceneModeDisabled:
            default:
                // Set by UI. No need to override focus mode which is read from Meta data.
                break;
        }

        m_focusMode = *pAFMode;

        CAMX_LOG_VERBOSE(CamxLogGroupAF, "HALFocusMode = %d, AlgoFocusMode = %d, SceneMode = %d",
                         *reinterpret_cast<ControlAFModeValues*>(pData[0]),
                         m_focusMode,
                         m_controlSceneMode);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "Error in reading property AFmode %p Scenemode %p",
                       pData[0], pData[1]);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFIOUtil::ReadCameraOperationalMode()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
AFRunMode CAFIOUtil::ReadCameraOperationalMode()
{
    AFRunMode cameraOperationalMode = AFRunModePreview;
    static const UINT StatsData[] =
    {
        InputControlCaptureIntent,
    };

    static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
    VOID*             pData[StatsLength] = { 0 };
    pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputControlCaptureIntent);

    if (NULL != pData[0])
    {
        switch (*reinterpret_cast<ControlCaptureIntentValues*>(pData[0]))
        {
            case ControlCaptureIntentManual:
            case ControlCaptureIntentCustom:
            case ControlCaptureIntentZeroShutterLag:
            case ControlCaptureIntentPreview:
                cameraOperationalMode = AFRunModePreview;
                break;
            case ControlCaptureIntentStillCapture:
                cameraOperationalMode = AFRunModeSnapshot;
                break;
            case ControlCaptureIntentVideoRecord:
            case ControlCaptureIntentVideoSnapshot:
                cameraOperationalMode = AFRunModeVideo;
                break;
            default:
                CAMX_ASSERT_ALWAYS_MESSAGE("Invalid run mode: %d", *reinterpret_cast<ControlCaptureIntentValues*>(pData[0]));
                cameraOperationalMode = AFRunModeInvalid;
                break;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "Error in reading property operation mode %p",
                       pData[0]);
    }

    return cameraOperationalMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadFaceROIInformation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::ReadFaceROIInformation(
    AFFaceROIInfo* pFaceROIInfo,
    UnstabilizedFaceROIInformation* pUnstabilizedFaceROIInfo)
{
    CamxResult  result           = CamxResultSuccess;
    UINT32      metaTagFDROI     = 0;

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

    UINT              GetProps[]              = { metaTagFDROI };
    static const UINT GetPropsLength          = CAMX_ARRAY_SIZE(GetProps);
    VOID*             pData[GetPropsLength]   = { 0 };
    UINT64            offsets[GetPropsLength] = { m_pNode->GetMaximumPipelineDelay() };

    if (CamxResultSuccess == result)
    {
        m_pNode->GetDataList(GetProps, pData, offsets, GetPropsLength);
    }

    if (NULL != pData[0])
    {
        CAMX_LOG_VERBOSE(CamxLogGroupAF, "Face ROI is published");
        RetrieveFaceRegions(reinterpret_cast<FaceROIInformation*>(pData[0]), &m_faceROIInfo, &m_statsROIInformation);
        *pUnstabilizedFaceROIInfo = m_statsROIInformation;
        *pFaceROIInfo = m_faceROIInfo;
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupAF, "Face ROI is not published");
        result = CamxResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadFocusRegions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::ReadFocusRegions(
    AFROIInfo* pROIInfo)
{
    static const UINT StatsData[] =
    {
        InputControlAFRegions,
    };

    static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
    VOID*             pData[StatsLength] = { 0 };
    pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputControlAFRegions);
    if (NULL != pData[0])
    {
        RetrieveFocusRegions(reinterpret_cast<WeightedRegion*>(pData[0]), &m_focusRegionInfo);
        *pROIInfo = m_focusRegionInfo;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "ROI is NULL");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadControlAFTrigger
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ControlAFTriggerValues CAFIOUtil::ReadControlAFTrigger()
{
    m_controlAFTriggerValues = ControlAFTriggerIdle;

    if (NULL != m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputControlAFTrigger))
    {
        m_controlAFTriggerValues = *reinterpret_cast<ControlAFTriggerValues*>(
            m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputControlAFTrigger));
    }

    return m_controlAFTriggerValues;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ReadFocusDistanceInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::ReadFocusDistanceInfo(
    AFFocusDistanceInfo* pFocusDistance)
{
    if (NULL != m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputLensFocusDistance))
    {
        pFocusDistance->lensFocusDistance =
            *reinterpret_cast<FLOAT*>(m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypeInputLensFocusDistance));

        m_focusDistanceInfo.lensFocusDistance = pFocusDistance->lensFocusDistance;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "Error in reading property Input Lens Focus Distance");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///  CAFIOUtil::GetAFOutputBuffers()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::GetAFOutputBuffers(
    AFAlgoOutputList**  ppOutputList,
    AFHALOutputList**   ppHALOutput)
{
    *ppOutputList   = &m_outputList;
    *ppHALOutput    = &m_HALoutputList;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///  CAFIOUtil::PrepareBAFConfigBuffers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::PrepareBAFConfigBuffers()
{
    AFBAFFilterConfig*       pBAFFilterConfig       = NULL;
    AFBAFFIRFilterConfig*    pBAFFIRFilterConfig    = NULL;
    AFBAFFilterCoringConfig* pBAFFilterCoringConfig = NULL;
    AFBAFGammaLUTConfig*     pBAFGammaLUTConfig     = NULL;
    AFBAFInputConfig*        pBAFInputConfig        = NULL;

    // Prepare output buffer to hold Floating Window configuration
    // Prepare BAF Horizontal Filter - 1
    pBAFFilterConfig                            = &m_BFOut.BAFFilterConfigH1;
    pBAFFIRFilterConfig                         = &(m_BFOut.filter[BAFFilterTypeHorizontal1].FIRFilterConfig);
    pBAFFIRFilterConfig->pFIRFilterCoefficients = m_BFOut.filter[BAFFilterTypeHorizontal1].FIRFilterCoeff;
    pBAFFilterCoringConfig                      = &(m_BFOut.filter[BAFFilterTypeHorizontal1].filterCoringConfig);
    pBAFFilterCoringConfig->pCoringFilter       = m_BFOut.filter[BAFFilterTypeHorizontal1].coring;
    pBAFFilterConfig->pBAFFIRFilterConfig       = pBAFFIRFilterConfig;
    pBAFFilterConfig->pBAFIIRFilterConfig       = &(m_BFOut.filter[BAFFilterTypeHorizontal1].IIRFilterConfig);
    pBAFFilterConfig->pBAFFilterCoringConfig    = &(m_BFOut.filter[BAFFilterTypeHorizontal1].filterCoringConfig);
    m_AFOutputBAFFWConfig.pBAFFilterConfigH1    = pBAFFilterConfig;

    // Prepare BAF Horizontal Filter - 2
    pBAFFilterConfig                            = &m_BFOut.BAFFilterConfigH2;
    pBAFFIRFilterConfig                         = &(m_BFOut.filter[BAFFilterTypeHorizontal2].FIRFilterConfig);
    pBAFFIRFilterConfig->pFIRFilterCoefficients = m_BFOut.filter[BAFFilterTypeHorizontal2].FIRFilterCoeff;
    pBAFFilterCoringConfig                      = &(m_BFOut.filter[BAFFilterTypeHorizontal2].filterCoringConfig);
    pBAFFilterCoringConfig->pCoringFilter       = m_BFOut.filter[BAFFilterTypeHorizontal2].coring;
    pBAFFilterConfig->pBAFFIRFilterConfig       = pBAFFIRFilterConfig;
    pBAFFilterConfig->pBAFIIRFilterConfig       = &(m_BFOut.filter[BAFFilterTypeHorizontal2].IIRFilterConfig);
    pBAFFilterConfig->pBAFFilterCoringConfig    = &(m_BFOut.filter[BAFFilterTypeHorizontal2].filterCoringConfig);
    m_AFOutputBAFFWConfig.pBAFFilterConfigH2    = pBAFFilterConfig;

    // Prepare BAF Vertical Filter
    pBAFFilterConfig                            = &m_BFOut.BAFFilterConfigV;
    pBAFFIRFilterConfig                         = &(m_BFOut.filter[BAFFilterTypeVertical].FIRFilterConfig);
    pBAFFIRFilterConfig->pFIRFilterCoefficients = m_BFOut.filter[BAFFilterTypeVertical].FIRFilterCoeff;
    pBAFFilterCoringConfig                      = &(m_BFOut.filter[BAFFilterTypeVertical].filterCoringConfig);
    pBAFFilterCoringConfig->pCoringFilter       = m_BFOut.filter[BAFFilterTypeVertical].coring;
    pBAFFilterConfig->pBAFFIRFilterConfig       = pBAFFIRFilterConfig;
    pBAFFilterConfig->pBAFIIRFilterConfig       = &(m_BFOut.filter[BAFFilterTypeVertical].IIRFilterConfig);
    pBAFFilterConfig->pBAFFilterCoringConfig    = &(m_BFOut.filter[BAFFilterTypeVertical].filterCoringConfig);
    m_AFOutputBAFFWConfig.pBAFFilterConfigV     = pBAFFilterConfig;

    // Prepare Gamma Lookup-table buffer
    pBAFGammaLUTConfig                          = &(m_BFOut.BAFGammaLUTConfig);
    pBAFGammaLUTConfig->pGammaLUT               = m_BFOut.gammaLUT;
    m_AFOutputBAFFWConfig.pBAFGammaLUTConfig    = pBAFGammaLUTConfig;

    // Prepare BAF Input configuration buffer
    pBAFInputConfig                             = &m_BFOut.BAFInputConfig;
    pBAFInputConfig->pYConfig                   = m_BFOut.yChannel;
    m_AFOutputBAFFWConfig.pBAFInputConfig       = pBAFInputConfig;

    // Prepare BAF scalar buffer
    m_AFOutputBAFFWConfig.pBAFScaleConfig       = &m_BFOut.BAFScaleConfig;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///  CAFIOUtil::PrepareAFOutputBuffers()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::PrepareAFOutputBuffers()
{
    // Top level output structure
    m_outputList.outputCount    = AFOutputTypeLastIndex;
    m_outputList.pAFOutputs     = m_outputArray;

    // Prepare output buffer to hold BAF configuration
    PrepareBAFConfigBuffers();
    m_outputList.pAFOutputs[AFOutputTypeBAFFloatingWindowConfig].outputType         = AFOutputTypeBAFFloatingWindowConfig;
    m_outputList.pAFOutputs[AFOutputTypeBAFFloatingWindowConfig].pAFOutput          = &m_AFOutputBAFFWConfig;
    m_outputList.pAFOutputs[AFOutputTypeBAFFloatingWindowConfig].sizeOfAFOutput     = sizeof(m_AFOutputBAFFWConfig);

    // Prepare output buffer to hold Floating Window ROI configuration
    m_AFOutputBAFFWROIConfig.pBAFFwROIDimension                                     = m_AFOutputFWROIDimensions;
    m_outputList.pAFOutputs[AFOutputTypeBAFFloatingWindowROIConfig].outputType      = AFOutputTypeBAFFloatingWindowROIConfig;
    m_outputList.pAFOutputs[AFOutputTypeBAFFloatingWindowROIConfig].pAFOutput       = &m_AFOutputBAFFWROIConfig;
    m_outputList.pAFOutputs[AFOutputTypeBAFFloatingWindowROIConfig].sizeOfAFOutput  = sizeof(m_AFOutputBAFFWROIConfig);

    // Prepare output buffer to hold exposure compensation enable/disable flag
    m_outputList.pAFOutputs[AFOutputTypeExposureCompensation].outputType            = AFOutputTypeExposureCompensation;
    m_outputList.pAFOutputs[AFOutputTypeExposureCompensation].pAFOutput             = &m_AFOutputExposureCompensation;
    m_outputList.pAFOutputs[AFOutputTypeExposureCompensation].sizeOfAFOutput        = sizeof(m_AFOutputExposureCompensation);

    // Prepare output buffer to hold focus value calculated by algorithm from BF stats
    m_outputList.pAFOutputs[AFOutputTypeFocusValue].outputType                      = AFOutputTypeFocusValue;
    m_outputList.pAFOutputs[AFOutputTypeFocusValue].pAFOutput                       = &m_AFOutputFocusValue;
    m_outputList.pAFOutputs[AFOutputTypeFocusValue].sizeOfAFOutput                  = sizeof(m_AFOutputFocusValue);

    // Prepare output buffer to hold spotlight detection flag
    m_outputList.pAFOutputs[AFOutputTypeSpotLightDetected].outputType               = AFOutputTypeSpotLightDetected;
    m_outputList.pAFOutputs[AFOutputTypeSpotLightDetected].pAFOutput                = &m_AFOutputSpotLightDetected;
    m_outputList.pAFOutputs[AFOutputTypeSpotLightDetected].sizeOfAFOutput           = sizeof(m_AFOutputSpotLightDetected);

    // Prepare output buffer for FOV compensation crop factor
    m_outputList.pAFOutputs[AFOutputTypeCropMagnificationFactor].outputType         = AFOutputTypeCropMagnificationFactor;
    m_outputList.pAFOutputs[AFOutputTypeCropMagnificationFactor].pAFOutput          = &m_AFOutputCropMagnificationFactor;
    m_outputList.pAFOutputs[AFOutputTypeCropMagnificationFactor].sizeOfAFOutput     = sizeof(m_AFOutputCropMagnificationFactor);

    // Prepare output buffer for depth focus indicator
    m_outputList.pAFOutputs[AFOutputTypeIsDepthFocus].outputType                    = AFOutputTypeIsDepthFocus;
    m_outputList.pAFOutputs[AFOutputTypeIsDepthFocus].pAFOutput                     = &m_AFOutputIsDepthFocus;
    m_outputList.pAFOutputs[AFOutputTypeIsDepthFocus].sizeOfAFOutput                = sizeof(m_AFOutputIsDepthFocus);

    // Prepare output buffer to hold ROI size
    m_outputList.pAFOutputs[AFOutputTypeROIDim].outputType                          = AFOutputTypeROIDim;
    m_outputList.pAFOutputs[AFOutputTypeROIDim].pAFOutput                           = &m_AFOutputROIDim;
    m_outputList.pAFOutputs[AFOutputTypeROIDim].sizeOfAFOutput                      = sizeof(m_AFOutputROIDim);

    // Prepare output buffer to hold PDAF ROI configuration
    m_outputList.pAFOutputs[AFOutputTypePDAFConfig].outputType                      = AFOutputTypePDAFConfig;
    m_outputList.pAFOutputs[AFOutputTypePDAFConfig].pAFOutput                       = &m_AFOutputPDAFWindowConfig;
    m_outputList.pAFOutputs[AFOutputTypePDAFConfig].sizeOfAFOutput                  = sizeof(PDLibWindowConfig);

    // Prepare output buffer to hold lens move information
    m_outputList.pAFOutputs[AFOutputTypeMoveLensParam].outputType                   = AFOutputTypeMoveLensParam;
    m_outputList.pAFOutputs[AFOutputTypeMoveLensParam].pAFOutput                    = &m_AFOutputLensMoveInfo;
    m_outputList.pAFOutputs[AFOutputTypeMoveLensParam].sizeOfAFOutput               = sizeof(m_AFOutputLensMoveInfo);

    // Prepare output buffer to hold fovc info
    m_outputList.pAFOutputs[AFOutputTypeFOVCParam].outputType                       = AFOutputTypeFOVCParam;
    m_outputList.pAFOutputs[AFOutputTypeFOVCParam].pAFOutput                        = &m_AFFOVCInfo;
    m_outputList.pAFOutputs[AFOutputTypeFOVCParam].sizeOfAFOutput                   = sizeof(AFFOVCInfo);

    /// @todo (CAMX-1217) dependency on (CAMX-1180)
    //  getting debug data pointer from property pool
    CamX::Utils::Memset(&m_AFOutputDebugData, 0, sizeof(m_AFOutputDebugData));
    m_outputList.pAFOutputs[AFOutputTypeDebugData].outputType                       = AFOutputTypeDebugData;
    m_outputList.pAFOutputs[AFOutputTypeDebugData].pAFOutput                        = &m_AFOutputDebugData;
    m_outputList.pAFOutputs[AFOutputTypeDebugData].sizeOfAFOutput                   = sizeof(m_AFOutputDebugData);

    // Prepare output buffer for software stats
    m_AFOutputSoftwareStatsFilterConfig.pCoefficientsIIRA                           = m_AFOutputSWIIRCoeffA;
    m_AFOutputSoftwareStatsFilterConfig.pCoefficientsIIRB                           = m_AFOutputSWIIRCoeffB;
    m_AFOutputSoftwareStatsFilterConfig.pCoefficientsFIR                            = m_AFOutputSWFIRCoeff;
    m_outputList.pAFOutputs[AFOutputTypeDebugData].outputType                       = AFOutputTypeDebugData;
    m_outputList.pAFOutputs[AFOutputTypeDebugData].pAFOutput                        = &m_AFOutputSoftwareStatsFilterConfig;
    m_outputList.pAFOutputs[AFOutputTypeDebugData].sizeOfAFOutput                   =
        sizeof(m_AFOutputSoftwareStatsFilterConfig);

    // Prepare output buffer for focus status
    m_outputList.pAFOutputs[AFOutputTypeStatusParam].outputType                     = AFOutputTypeStatusParam;
    m_outputList.pAFOutputs[AFOutputTypeStatusParam].pAFOutput                      = &m_AFOutputAFStatus;
    m_outputList.pAFOutputs[AFOutputTypeStatusParam].sizeOfAFOutput                 = sizeof(m_AFOutputAFStatus);

    // Prepare output buffer for focus mode set by algorithm
    m_outputList.pAFOutputs[AFOutputTypeFocusMode].outputType                       = AFOutputTypeFocusMode;
    m_outputList.pAFOutputs[AFOutputTypeFocusMode].pAFOutput                        = &m_focusMode;
    m_outputList.pAFOutputs[AFOutputTypeFocusMode].sizeOfAFOutput                   = sizeof(m_focusMode);

    // Prepare output buffer for vendor tag
    m_outputList.pAFOutputs[AFOutputPublishingVendorTagsInfo].outputType            = AFOutputPublishingVendorTagsInfo;
    m_outputList.pAFOutputs[AFOutputPublishingVendorTagsInfo].pAFOutput             = &m_vendorTagPublishDataList;
    m_outputList.pAFOutputs[AFOutputPublishingVendorTagsInfo].sizeOfAFOutput        = sizeof(m_vendorTagPublishDataList);

    // Prepare output buffer for depth map information send via vendor tag
    m_outputList.pAFOutputs[AFOutputTypeVendorTag].outputType                       = AFOutputTypeVendorTag;
    m_outputList.pAFOutputs[AFOutputTypeVendorTag].pAFOutput                        = &m_depthMapInfo;
    m_outputList.pAFOutputs[AFOutputTypeVendorTag].sizeOfAFOutput                   = sizeof(m_depthMapInfo);

    // Toplevel output structure
    m_HALoutputList.outputCount                                                     = AFHALOutputTypeLastIndex;
    m_HALoutputList.pAFOutputs                                                      = m_HALoutputArray;

    // Prepare output buffer for Control AF State
    m_HALoutputList.pAFOutputs[AFHALOutputTypeControlAFState].outputType            = AFHALOutputTypeControlAFState;
    m_HALoutputList.pAFOutputs[AFHALOutputTypeControlAFState].pAFOutput             = &m_AFState;

    // Prepare output buffer for Control AF Mode
    m_HALoutputList.pAFOutputs[AFHALOutputTypeControlFocusMode].outputType          = AFHALOutputTypeControlFocusMode;
    m_HALoutputList.pAFOutputs[AFHALOutputTypeControlFocusMode].pAFOutput           = &m_focusMode;

    // Prepare output buffer for Control AF Region
    m_HALoutputList.pAFOutputs[AFHALOutputTypeControlAFRegions].outputType          = AFHALOutputTypeControlAFRegions;
    m_HALoutputList.pAFOutputs[AFHALOutputTypeControlAFRegions].pAFOutput           = &m_focusRegionInfo;

    // Prepare output buffer for Lens Focus Distance
    m_HALoutputList.pAFOutputs[AFHALOutputTypeLensFocusDistance].outputType         = AFHALOutputTypeLensFocusDistance;
    m_HALoutputList.pAFOutputs[AFHALOutputTypeLensFocusDistance].pAFOutput          = &m_focusDistanceInfo;

    // Prepare output buffer for Focus value
    m_HALoutputList.pAFOutputs[AFHALOutputTypeFocusValue].outputType                = AFHALOutputTypeFocusValue;
    m_HALoutputList.pAFOutputs[AFHALOutputTypeFocusValue].pAFOutput                 = &m_AFOutputFocusValue;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///  CAFIOUtil::ResetOutputs()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::ResetOutputs(
    const AFAlgoOutputList* pOutput)
{
    CAMX_ENTRYEXIT(CamxLogGroupAF);

    CAMX_ASSERT(NULL != pOutput);

    // AF node process an output if client has updated sizeOfWrittenAFOutput to non zero value.
    // Before each call to algo, this field will be set to 0.
    // If algo update an ouput, it needs to update its corresponding sizeOfWrittenAFOutput field with proper value as well.
    for (UINT32 i = 0; i < pOutput->outputCount; i++)
    {
        pOutput->pAFOutputs[i].sizeOfWrittenAFOutput = 0;
    }

    for (UINT32 i = 0; i < m_vendorTagPublishDataList.vendorTagCount; i++)
    {
        m_vendorTagPublishDataList.vendorTag[i].sizeOfWrittenData = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///  CAFIOUtil::GetAlgoOutputGetParamBuffers()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::GetAlgoOutputGetParamBuffers(
    AFAlgoGetParam* pGetParam)
{
    pGetParam->outputInfo.getParamOutputCount   = m_getParamOutputList.getParamOutputCount;
    pGetParam->outputInfo.pGetParamOutputs      = m_getParamOutputList.pGetParamOutputs;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///  CAFIOUtil::PopulateAFDefaultGetParamBuffers()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::PopulateAFDefaultGetParamBuffers(
    AFAlgoGetParam*  pGetParam)
{
    UINT32 getParamOutArrayIndex    = 0;
    UINT32 getParamInputArrayIndex  = 0;

    Utils::Memset(&m_getParamOutputArray, 0, sizeof(m_getParamOutputArray));
    Utils::Memset(&m_getParamInputArray, 0, sizeof(m_getParamInputArray));

    // Prepare output buffer to hold BAF configuration
    PrepareBAFConfigBuffers();
    m_getParamInputArray[getParamInputArrayIndex].type                          = AFGetParamBAFFloatingWindowConfig;
    getParamInputArrayIndex +=1;
    m_getParamOutputArray[getParamOutArrayIndex].getParamOutputType             = AFGetParamBAFFloatingWindowConfig;
    m_getParamOutputArray[getParamOutArrayIndex].pGetParamOutput                = &m_AFOutputBAFFWConfig;
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfGetParamOutput           = sizeof(m_AFOutputBAFFWConfig);
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfWrittenGetParamOutput    = 0;
    getParamOutArrayIndex +=1;

    // Prepare output buffer to hold default floating window ROI configuration
    m_getParamInputArray[getParamInputArrayIndex].type                          = AFGetParamBAFFloatingWindowROIConfig;
    getParamInputArrayIndex +=1;
    m_getParamOutputArray[getParamOutArrayIndex].pGetParamOutput                = &m_AFOutputBAFFWROIConfig;
    m_getParamOutputArray[getParamOutArrayIndex].getParamOutputType             = AFGetParamBAFFloatingWindowROIConfig;
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfGetParamOutput           = sizeof(m_AFOutputBAFFWROIConfig);
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfWrittenGetParamOutput    = 0;
    getParamOutArrayIndex += 1;

    // Prepare output buffer to hold default lens position
    m_getParamInputArray[getParamInputArrayIndex].type                          = AFGetParamCurrentLensPosition;
    getParamInputArrayIndex +=1;
    m_getParamOutputArray[getParamOutArrayIndex].pGetParamOutput                = &m_AFOutputLensMoveInfo;
    m_getParamOutputArray[getParamOutArrayIndex].getParamOutputType             = AFGetParamCurrentLensPosition;
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfGetParamOutput           = sizeof(m_AFOutputLensMoveInfo);
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfWrittenGetParamOutput    = 0;
    getParamOutArrayIndex += 1;


    // Prepare output buffer to hold default ROI for PDAF
    m_getParamInputArray[getParamInputArrayIndex].type                          = AFGetParamROIDim;
    getParamInputArrayIndex +=1;
    m_getParamOutputArray[getParamOutArrayIndex].pGetParamOutput                = &m_AFOutputROIDim;
    m_getParamOutputArray[getParamOutArrayIndex].getParamOutputType             = AFGetParamROIDim;
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfGetParamOutput           = sizeof(m_AFOutputROIDim);
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfWrittenGetParamOutput    = 0;
    getParamOutArrayIndex += 1;

    // Prepare output buffer to hold PDAF window config.
    m_getParamInputArray[getParamInputArrayIndex].type                          = AFGetParamPDAFWindowConfig;
    getParamInputArrayIndex +=1;
    m_getParamOutputArray[getParamOutArrayIndex].pGetParamOutput                = &m_AFOutputPDAFWindowConfig;
    m_getParamOutputArray[getParamOutArrayIndex].getParamOutputType             = AFGetParamPDAFWindowConfig;
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfGetParamOutput           = sizeof(PDLibWindowConfig);
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfWrittenGetParamOutput    = 0;
    getParamOutArrayIndex += 1;

    // Add camera infomation
    m_getParamInputArray[getParamInputArrayIndex].type                          = AFGetParamPassCameraInfo;
    m_getParamInputArray[getParamInputArrayIndex].pInputData                    = reinterpret_cast<VOID*>(&m_cameraInfo);
    m_getParamInputArray[getParamInputArrayIndex].sizeOfInputData               = sizeof(StatsCameraInfo);
    getParamInputArrayIndex += 1;
    getParamOutArrayIndex += 1;

    // Prepare output buffer for FOVC Info
    m_getParamInputArray[getParamInputArrayIndex].type                          = AFGetParamFOVCInfo;
    getParamInputArrayIndex +=1;
    m_getParamOutputArray[getParamOutArrayIndex].pGetParamOutput                = &m_AFFOVCInfo;
    m_getParamOutputArray[getParamOutArrayIndex].getParamOutputType             = AFGetParamFOVCInfo;
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfGetParamOutput           = sizeof(AFFOVCInfo);
    m_getParamOutputArray[getParamOutArrayIndex].sizeOfWrittenGetParamOutput    = 0;
    getParamOutArrayIndex += 1;

    // Top level output structure
    pGetParam->outputInfo.pGetParamOutputs                                      = m_getParamOutputArray;
    pGetParam->outputInfo.getParamOutputCount                                   = getParamOutArrayIndex;
    pGetParam->inputInfo.pGetParamInputs                                        = m_getParamInputArray;
    pGetParam->inputInfo.getParamInputCount                                     = getParamInputArrayIndex;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///  CAFIOUtil::PopulateVendorTagGetParam()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::PopulateVendorTagGetParam(
    AFAlgoGetParamType  getParamType,
    AFAlgoGetParam*     pGetParam)
{
    UINT32 getParamOutArrayIndex   = 0;
    UINT32 getParamInputArrayIndex = 0;

    Utils::Memset(&m_getParamOutputArray, 0, sizeof(m_getParamOutputArray));
    Utils::Memset(&m_getParamInputArray, 0, sizeof(m_getParamInputArray));

    switch (getParamType)
    {
        case AFGetParamDependentVendorTags:
            Utils::Memset(&m_vendorTagInputList, 0, sizeof(m_vendorTagInputList));
            m_getParamInputArray[getParamInputArrayIndex].type = AFGetParamDependentVendorTags;
            getParamInputArrayIndex += 1;

            // Prepare get param output to fetch OEM vendor tag dependencies
            m_getParamOutputArray[getParamOutArrayIndex].pGetParamOutput                 = &m_vendorTagInputList;
            m_getParamOutputArray[getParamOutArrayIndex].getParamOutputType              = AFGetParamDependentVendorTags;
            m_getParamOutputArray[getParamOutArrayIndex].sizeOfGetParamOutput            = sizeof(m_vendorTagInputList);
            m_getParamOutputArray[getParamOutArrayIndex].sizeOfWrittenGetParamOutput     = 0;

            getParamOutArrayIndex += 1;
            break;

        case AFGetParamVendorTagList:
            Utils::Memset(&m_vendorTagOutputList, 0, sizeof(m_vendorTagOutputList));
            m_getParamInputArray[getParamInputArrayIndex].type = AFGetParamVendorTagList;
            getParamInputArrayIndex += 1;

            // Prepare get param output to fetch OEM vendor tag outputs
            m_getParamOutputArray[getParamOutArrayIndex].pGetParamOutput                 = &m_vendorTagOutputList;
            m_getParamOutputArray[getParamOutArrayIndex].getParamOutputType              = AFGetParamVendorTagList;
            m_getParamOutputArray[getParamOutArrayIndex].sizeOfGetParamOutput            = sizeof(m_vendorTagOutputList);
            m_getParamOutputArray[getParamOutArrayIndex].sizeOfWrittenGetParamOutput     = 0;

            getParamOutArrayIndex += 1;
            break;

        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Unknown getParamType %d", getParamType);
            break;
    }
    // Add camera infomation
    m_getParamInputArray[getParamInputArrayIndex].type            = AFGetParamPassCameraInfo;
    m_getParamInputArray[getParamInputArrayIndex].pInputData      = reinterpret_cast<VOID*>(&m_cameraInfo);
    m_getParamInputArray[getParamInputArrayIndex].sizeOfInputData = sizeof(StatsCameraInfo);
    getParamInputArrayIndex += 1;
    // Top level output structure
    pGetParam->outputInfo.pGetParamOutputs    = m_getParamOutputArray;
    pGetParam->outputInfo.getParamOutputCount = getParamOutArrayIndex;
    pGetParam->inputInfo.pGetParamInputs      = m_getParamInputArray;
    pGetParam->inputInfo.getParamInputCount   = getParamInputArrayIndex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishFrameControl
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishFrameControl(
    const AFAlgoOutputList*     pOutput,
    AFFrameControl*             pFrameControl
    ) const
{
    CAMX_ASSERT(NULL != pFrameControl);
    CamxResult     result       = CamxResultSuccess;

    UINT32 maxIndex = static_cast<UINT32>(CamX::Utils::MaxUINT32(AFOutputTypeLastIndex, pOutput->outputCount));

    for (UINT32 i = 0; i < maxIndex; i++)
    {
        if (0 == pOutput->pAFOutputs[i].sizeOfWrittenAFOutput)
        {
            continue;
        }

        switch (pOutput->pAFOutputs[i].outputType)
        {
            case AFOutputTypeExposureCompensation:
                {
                    pFrameControl->exposureCompensationEnable =
                        *(static_cast<BOOL*>(pOutput->pAFOutputs[i].pAFOutput));

                }
                break;
            case AFOutputTypeFOVCParam:
                {
                    pFrameControl->fovcOutput =
                        *(static_cast<FOVCOutput*>(pOutput->pAFOutputs[i].pAFOutput));
                }
                break;
            case AFOutputTypePDAFConfig:
                {
                    pFrameControl->PDLibROI =
                        *(static_cast<PDLibWindowConfig*>(pOutput->pAFOutputs[i].pAFOutput));
                    CAMX_LOG_VERBOSE(CamxLogGroupAF, "PDLibROI type %d, [%f %f %f %f], hnum %d vnum %d",
                    pFrameControl->PDLibROI.roiType,
                    pFrameControl->PDLibROI.fixedAFWindow.startX,
                    pFrameControl->PDLibROI.fixedAFWindow.startY,
                    pFrameControl->PDLibROI.fixedAFWindow.endX,
                    pFrameControl->PDLibROI.fixedAFWindow.endY,
                    pFrameControl->PDLibROI.horizontalWindowCount,
                    pFrameControl->PDLibROI.verticalWindowCount);
                }
                break;

            // Intentional Fallthrough
            case AFOutputTypeStatusParam:
            case AFOutputTypeBAFFloatingWindowROIConfig:
            case AFOutputTypeBAFFloatingWindowConfig:
            case AFOutputTypeROIDim:
            case AFOutputTypeMoveLensParam:
            case AFOutputTypeSoftwareStatsConfig:
            case AFOutputTypeFocusMode:
            case AFOutputTypeFocusValue:
            case AFOutputTypeDebugData:
            case AFOutputTypeSpotLightDetected:
            case AFOutputTypeVendorTag:
            case AFOutputPublishingVendorTagsInfo:
            case AFOutputTypeCropMagnificationFactor:
            case AFOutputTypeIsDepthFocus:
                break;

            default:
                CAMX_LOG_INFO(CamxLogGroupAF, "Invalid output type! %d", pOutput->pAFOutputs[i].outputType);
                break;
        }
    }

    // Publish Vendor Tag
    UINT32 metadataAFFrameControl;

    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera2.statsconfigs", "AFFrameControl",
        &metadataAFFrameControl);

    if (CamxResultSuccess == result)
    {
        static const UINT PropertiesAFFrame[]               = { metadataAFFrameControl };
        const static UINT length                            = CAMX_ARRAY_SIZE(PropertiesAFFrame);
        const VOID*       pFrameControlData[length]         = { pFrameControl };
        UINT              pFrameControlDataCount[length]    = { sizeof(AFFrameControl) };

        result = m_pNode->WriteDataList(PropertiesAFFrame, pFrameControlData, pFrameControlDataCount, length);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupAF, "cannot write to AF Frame control vendor tag");
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "cannot query AF Frame control vendor tag");
    }


    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishStatsControl
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishStatsControl(
    const AFAlgoOutputList* pOutput,
    AFStatsControl*         pStatsControl)
{
    CAMX_ASSERT(NULL != pStatsControl);

    UINT32 maxIndex = static_cast<UINT32>(CamX::Utils::MaxUINT32(AFOutputTypeLastIndex, pOutput->outputCount));

    for (UINT32 i = 0; i < maxIndex; i++)
    {
        if (0 == pOutput->pAFOutputs[i].sizeOfWrittenAFOutput)
        {
            continue;
        }

        switch (pOutput->pAFOutputs[i].outputType)
        {
            case AFOutputTypeBAFFloatingWindowROIConfig:
                {
                    BFStatsConfigParams*          pBFConfig       = &pStatsControl->statsConfig.BFStats;
                    AFBAFFloatingWindowROIConfig* pBAFFwROIConfig =
                        static_cast<AFBAFFloatingWindowROIConfig *>(pOutput->pAFOutputs[i].pAFOutput);
                    UpdatePropertyROIOutput(pBFConfig, pBAFFwROIConfig);
                }
                break;

            case AFOutputTypeBAFFloatingWindowConfig:
                {
                    AFConfigParams*            pBFConfig    = &pStatsControl->statsConfig;
                    AFBAFFloatingWindowConfig* pBAFFwConfig =
                        static_cast<AFBAFFloatingWindowConfig*>(pOutput->pAFOutputs[i].pAFOutput);
                    UpdatePropertyBFConfigOutput(pBFConfig, pBAFFwConfig);
                }
                break;

            // Intentional Fallthrough
            case AFOutputTypeExposureCompensation:
            case AFOutputTypePDAFConfig:
            case AFOutputTypeStatusParam:
            case AFOutputTypeROIDim:
            case AFOutputTypeMoveLensParam:
            case AFOutputTypeSoftwareStatsConfig:
            case AFOutputTypeFocusMode:
            case AFOutputTypeFocusValue:
            case AFOutputTypeDebugData:
            case AFOutputTypeSpotLightDetected:
            case AFOutputTypeVendorTag:
            case AFOutputPublishingVendorTagsInfo:
            case AFOutputTypeCropMagnificationFactor:
            case AFOutputTypeIsDepthFocus:
            case AFOutputTypeFOVCParam:
                break;

            default:
                CAMX_LOG_INFO(CamxLogGroupAF, "Invalid output type: %d", pOutput->pAFOutputs[i].outputType);
                break;
        }
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishFrameInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishFrameInfo(
    const AFAlgoOutputList* pOutput,
    AFFrameInformation*     pAFFrameInfo
    ) const
{
    CAMX_ASSERT(NULL != pAFFrameInfo);

    for (UINT32 i = 0; i < pOutput->outputCount; i++)
    {
        if (0 == pOutput->pAFOutputs[i].sizeOfWrittenAFOutput)
        {
            continue;
        }

        switch (pOutput->pAFOutputs[i].outputType)
        {
            case AFOutputTypeStatusParam:
                pAFFrameInfo->focusStatus.focusDone                                   =
                    m_AFOutputAFStatus.focusDone;
                pAFFrameInfo->focusStatus.status                                      =
                    static_cast<AFStatus>(m_AFOutputAFStatus.status);
                pAFFrameInfo->focusStatus.focusDistance[FocusDistanceNearIndex]       =
                    m_AFOutputAFStatus.focusDistanceNear;
                pAFFrameInfo->focusStatus.focusDistance[FocusDistanceOptimalIndex]    =
                    m_AFOutputAFStatus.focusDistanceOptimal;
                pAFFrameInfo->focusStatus.focusDistance[FocusDistanceFarIndex]        =
                    m_AFOutputAFStatus.focusDistanceFar;
                break;
            case AFOutputTypeFocusValue:
                pAFFrameInfo->focusValue = *(static_cast<FLOAT *>(pOutput->pAFOutputs[i].pAFOutput));
                break;

            case AFOutputTypeSpotLightDetected:
                pAFFrameInfo->spotLightDetected = *(static_cast<BOOL*>(pOutput->pAFOutputs[i].pAFOutput));
                break;

            case AFOutputTypeCropMagnificationFactor:
                pAFFrameInfo->cropMagnificationFactor =
                    *(static_cast<FLOAT *>(pOutput->pAFOutputs[i].pAFOutput));
                break;
            case AFOutputTypeIsDepthFocus:
                {
                    pAFFrameInfo->isDepthFocus =
                        *(static_cast<BOOL *>(pOutput->pAFOutputs[i].pAFOutput));
                    // Publish Vendor Tag
                    UINT32      metadataAFisDepthFocus;
                    CamxResult  result = CamxResultSuccess;
                    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.isDepthFocus", "isDepthFocus",
                        &metadataAFisDepthFocus);
                    CAMX_LOG_VERBOSE(CamxLogGroupAF, "isDepthFocus:%d", pAFFrameInfo->isDepthFocus);
                    if (CamxResultSuccess == result)
                    {
                        static const UINT PropertiesAFFrame[]               = { metadataAFisDepthFocus };
                        const static UINT length                            = CAMX_ARRAY_SIZE(PropertiesAFFrame);
                        const VOID*       pFrameControlData[length]         = { &(pAFFrameInfo->isDepthFocus) };
                        UINT              pFrameControlDataCount[length]    = { sizeof(BOOL) };

                        result = m_pNode->WriteDataList(PropertiesAFFrame, pFrameControlData, pFrameControlDataCount, length);

                        if (CamxResultSuccess != result)
                        {
                            CAMX_LOG_ERROR(CamxLogGroupAF, "cannot write to AF isDepthFocus vendor tag");
                        }
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupAF, "cannot query AF isDepthFocus vendor tag");
                    }
                }
                break;
            case AFOutputTypeFOVCParam:
                pAFFrameInfo->fovcOutput.fieldOfViewCompensationFactor =
                    *(static_cast<FLOAT *>(pOutput->pAFOutputs[i].pAFOutput));
                break;
            case AFOutputTypeROIDim:
                {
                    StatsRectangle*      pROIDimOut     =
                        static_cast<StatsRectangle *>(pOutput->pAFOutputs[i].pAFOutput);
                    RectangleCoordinate* pCoordinate    = &pAFFrameInfo->ROICoordinate;

                    pCoordinate->left                   = pROIDimOut->left;
                    pCoordinate->top                    = pROIDimOut->top;
                    pCoordinate->width                  = pROIDimOut->width;
                    pCoordinate->height                 = pROIDimOut->height;
                }
                break;

            case AFOutputTypeMoveLensParam:
                {
                    MoveLensOutput*     pMoveLensOutput = &pAFFrameInfo->moveLensOutput;
                    AFLensPositionInfo* pLensMoveInfo   =
                        static_cast<AFLensPositionInfo *>(pOutput->pAFOutputs[i].pAFOutput);

                    UpdatePropertyMoveLensOutput(pMoveLensOutput, pLensMoveInfo);
                }
                break;
            case AFOutputTypeBAFFloatingWindowConfig:
            case AFOutputTypeBAFFloatingWindowROIConfig:
            case AFOutputTypePDAFConfig:
            case AFOutputTypeExposureCompensation:
            case AFOutputTypeFocusMode:
            case AFOutputTypeDebugData:
                break;
            default:
                CAMX_LOG_INFO(CamxLogGroupAF, "Invalid output type: %d", pOutput->pAFOutputs[i].outputType);
                break;
        }
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishSkippedFrameOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishSkippedFrameOutput() const
{
    CamxResult result                                       = CamxResultSuccess;
    static const UINT AFPropertyList[]                      =
    {
        PropertyIDAFFrameControl,
        PropertyIDAFFrameInfo,
        // PropertyIDAFInternal,    // Currently AF Node is not publishing this property
        PropertyIDAFStatsControl,
        PropertyIDPDHwConfig,
        PropertyIDAFPDFrameInfo
    };
    static const UINT AFPropertyListLength                  = CAMX_ARRAY_SIZE(AFPropertyList);
    VOID* pData[AFPropertyListLength]                       = { NULL };
    UINT64 AFPropertyListDataOffset[AFPropertyListLength]   = { 1, 1, 1, 1, 1 }; // Read from previous request id

    m_pNode->GetDataList(AFPropertyList, pData, AFPropertyListDataOffset, AFPropertyListLength);

    if ((NULL == pData[0]) || (NULL == pData[1]) || (NULL == pData[2]))
    {
        CAMX_LOG_ERROR(CamxLogGroupAF,
                       "PropertyIDAFFrameControl(0x%x) PropertyIDAFFrameInfo(0x%x) "
                       "PropertyIDAFStatsControl(0x%x) is not published",
                        pData[0],
                        pData[1],
                        pData[2]);
        result = CamxResultEInvalidState;
    }
    else
    {
        UINT AFPropertyWriteListLength = AFPropertyListLength;
        UINT pDataSize[AFPropertyListLength] =
        {
            sizeof(AFFrameControl),
            sizeof(AFFrameInformation),
            sizeof(AFStatsControl),
            sizeof(PDHwConfig),
            sizeof(AFPDAFData)
        };
        const VOID* pOutputData[] =
        {
            pData[0],
            pData[1],
            pData[2],
            pData[3],
            pData[4]
        };

        if (FALSE == m_isPDAFEnabled)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupAF, "PDAF is disabled, don't publish PDHwConfig and FPDFrameInfo");
            AFPropertyWriteListLength -= 2;
        }

        result = m_pNode->WriteDataList(AFPropertyList, pOutputData, pDataSize, AFPropertyWriteListLength);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PopulateStatsControl
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::PopulateStatsControl(
    const AFAlgoGetParam*   pGetParam,
    AFStatsControl*         pControl)
{
    const AFAlgoGetParamOutputList* pGetParamOutputList = &pGetParam->outputInfo;
    CAMX_ASSERT(NULL != pGetParamOutputList);
    for (UINT32 i = 0; i < pGetParamOutputList->getParamOutputCount; i++)
    {
        if (0 == pGetParamOutputList->pGetParamOutputs[i].sizeOfWrittenGetParamOutput)
        {
            continue;
        }

        switch (pGetParamOutputList->pGetParamOutputs[i].getParamOutputType)
        {
            case AFGetParamBAFFloatingWindowROIConfig:
                {
                    BFStatsConfigParams* pBFConfig = &pControl->statsConfig.BFStats;
                    AFBAFFloatingWindowROIConfig* pBAFFwROIConfig =
                        static_cast<AFBAFFloatingWindowROIConfig*>
                        (pGetParamOutputList->pGetParamOutputs[i].pGetParamOutput);
                    UpdatePropertyROIOutput(pBFConfig, pBAFFwROIConfig);
                    break;
                }
            case AFGetParamBAFFloatingWindowConfig:
                {
                    AFConfigParams* pAFConfig               = &pControl->statsConfig;
                    AFBAFFloatingWindowConfig* pBAFFwConfig =
                        static_cast<AFBAFFloatingWindowConfig*>
                        (pGetParamOutputList->pGetParamOutputs[i].pGetParamOutput);
                    UpdatePropertyBFConfigOutput(pAFConfig, pBAFFwConfig);
                    break;
                }

            // Intentional Fallthrough
            case AFGetParamCurrentLensPosition:
            case AFGetParamROIDim:
                break;

            default:
                CAMX_LOG_INFO(CamxLogGroupAF, "Invalid output type!");
                break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PopulateFrameControl
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::PopulateFrameControl(
    const AFAlgoGetParam*   pGetParam,
    AFFrameControl*         pControl
    ) const
{
    const AFAlgoGetParamOutputList* pGetParamOutputList = &pGetParam->outputInfo;
    CAMX_ASSERT(NULL != pGetParamOutputList);
    for (UINT32 i = 0; i < pGetParamOutputList->getParamOutputCount; i++)
    {
        if (0 == pGetParamOutputList->pGetParamOutputs[i].sizeOfWrittenGetParamOutput)
        {
            continue;
        }

        switch (pGetParamOutputList->pGetParamOutputs[i].getParamOutputType)
        {
            // Intentional Fallthrough
            case AFGetParamBAFFloatingWindowROIConfig:
            case AFGetParamBAFFloatingWindowConfig:
                break;

            case AFGetParamCurrentLensPosition:
                {
                    MoveLensOutput*     pMoveLensOutput = &pControl->moveLensOutput;
                    AFLensPositionInfo* pLensMoveInfo   =
                        static_cast<AFLensPositionInfo *>(pGetParamOutputList->pGetParamOutputs[i].pGetParamOutput);
                    UpdatePropertyMoveLensOutput(pMoveLensOutput, pLensMoveInfo);
                }
                break;

            case AFGetParamROIDim:
                {
                    StatsRectangle*      pROIDimOut     =
                        static_cast<StatsRectangle *>(pGetParamOutputList->pGetParamOutputs[i].pGetParamOutput);
                    RectangleCoordinate* pCoordinate    = &pControl->ROICoordinate;

                    pCoordinate->left   = pROIDimOut->left;
                    pCoordinate->top    = pROIDimOut->top;
                    pCoordinate->width  = pROIDimOut->width;
                    pCoordinate->height = pROIDimOut->height;
                }
                break;

            default:
                CAMX_LOG_INFO(CamxLogGroupAF, "Invalid output type!");
                break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PopulateFrameInformation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::PopulateFrameInformation(
    const AFAlgoGetParam*   pGetParam,
    AFFrameInformation*     pInformation
    ) const
{
    const AFAlgoGetParamOutputList* pGetParamOutputList = &pGetParam->outputInfo;
    CAMX_ASSERT(NULL != pGetParamOutputList);
    for (UINT32 i = 0; i < pGetParamOutputList->getParamOutputCount; i++)
    {
        if (0 == pGetParamOutputList->pGetParamOutputs[i].sizeOfWrittenGetParamOutput)
        {
            continue;
        }

        switch (pGetParamOutputList->pGetParamOutputs[i].getParamOutputType)
        {
            case AFGetParamCurrentLensPosition:
                {
                    MoveLensOutput*     pMoveLensOutput = &pInformation->moveLensOutput;
                    AFLensPositionInfo* pLensMoveInfo =
                        static_cast<AFLensPositionInfo *>(pGetParamOutputList->pGetParamOutputs[i].pGetParamOutput);
                    UpdatePropertyMoveLensOutput(pMoveLensOutput, pLensMoveInfo);
                }
                break;

            case AFGetParamROIDim:
                {
                    StatsRectangle*      pROIDimOut =
                        static_cast<StatsRectangle *>(pGetParamOutputList->pGetParamOutputs[i].pGetParamOutput);
                    RectangleCoordinate* pCoordinate = &pInformation->ROICoordinate;

                    pCoordinate->left   = pROIDimOut->left;
                    pCoordinate->top    = pROIDimOut->top;
                    pCoordinate->width  = pROIDimOut->width;
                    pCoordinate->height = pROIDimOut->height;
                }
                break;

            case AFGetParamFOVCInfo:
                {
                    AFFOVCInfo*         pFOVCInfoOut =
                        static_cast<AFFOVCInfo *>(pGetParamOutputList->pGetParamOutputs[i].pGetParamOutput);
                    pInformation->fovcOutput.fieldOfViewCompensationFactor = pFOVCInfoOut->fieldOfViewCompensationFactor;
                    CAMX_LOG_INFO(CamxLogGroupAF, " FOV CompensationFactor is %f", pFOVCInfoOut->fieldOfViewCompensationFactor);
                }
                break;

            default:
                CAMX_LOG_INFO(CamxLogGroupAF, "Invalid output type!");
                break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishPreRequestOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishPreRequestOutput(
    const AFAlgoGetParam* pGetParam)
{
    AFFrameControl      frameControl        = {{0}};
    AFStatsControl      statsControl        = {};
    AFFrameInformation  frameInformation    = {};
    PDHwConfig          pdHWConfiguration   = {};

    CAMX_ASSERT_MESSAGE(NULL != pGetParam, "pGetParam NULL pointer");

    PopulateFrameControl(pGetParam, &frameControl);
    PopulateStatsControl(pGetParam, &statsControl);
    PopulateFrameInformation(pGetParam, &frameInformation);

    if (TRUE == m_isPDAFEnabled)
    {
        PopulatePDHWConfiguration(&pdHWConfiguration);
    }

    static const UINT WriteProps[] =
    {
        PropertyIDUsecaseAFFrameControl,
        PropertyIDUsecaseAFStatsControl,
        PropertyIDUsecaseAFFrameInformation,
        PropertyIDUsecaseHWPDConfig
    };
    const VOID* pOutputData[CAMX_ARRAY_SIZE(WriteProps)] =
    {
        &frameControl,
        &statsControl,
        &frameInformation,
        &pdHWConfiguration
    };
    UINT pDataCount[CAMX_ARRAY_SIZE(WriteProps)] =
    {
        sizeof(frameControl),
        sizeof(statsControl),
        sizeof(frameInformation),
        sizeof(pdHWConfiguration)
    };
    CAMX_LOG_INFO(CamxLogGroupISP, "Writing to UsecasePool by AF");
    // Writing only usecase, so can write outside EPR
    return m_pNode->WriteDataList(WriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(WriteProps));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::CopyInputSettings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::CopyInputSettings()
{
    // Copy the same input focus regions
    m_AFOutputROIDim.left   = m_focusRegionInfo.pWeightedROI->ROI.left;
    m_AFOutputROIDim.top    = m_focusRegionInfo.pWeightedROI->ROI.top;
    m_AFOutputROIDim.width  = m_focusRegionInfo.pWeightedROI->ROI.width;
    m_AFOutputROIDim.height = m_focusRegionInfo.pWeightedROI->ROI.height;
    m_AFOutputROIDim.weight = m_focusRegionInfo.pWeightedROI->ROIWeight;

    // Map the input focus mode back to metadata
    ControlAFModeValues* pTempFocusMode = reinterpret_cast<ControlAFModeValues*>(&m_focusMode);
    MapFocusModeToHALType(&m_focusMode, pTempFocusMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishDefaultOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishDefaultOutput()
{
    // Fetch the data in the usecase pool and copy to the result
    static const UINT GetProps[] =
    {
        PropertyIDUsecaseAFFrameControl,
        PropertyIDUsecaseAFStatsControl,
        PropertyIDUsecaseAFFrameInformation,
        PropertyIDUsecaseHWPDConfig
    };
    static const UINT WriteProps[] =
    {
        PropertyIDAFFrameControl,
        PropertyIDAFStatsControl,
        PropertyIDAFFrameInfo,
        PropertyIDPDHwConfig
    };
    UINT pDataCount[CAMX_ARRAY_SIZE(WriteProps)] =
    {
        sizeof(AFFrameControl),
        sizeof(AFStatsControl),
        sizeof(AFFrameInformation),
        sizeof(PDHwConfig)
    };
    VOID*  pData[CAMX_ARRAY_SIZE(WriteProps)]   = { 0 };
    UINT64 offsets[CAMX_ARRAY_SIZE(WriteProps)] = { 0 };

    m_pNode->GetDataList(GetProps, pData, offsets, CAMX_ARRAY_SIZE(WriteProps));

    // NOWHINE CP036a:
    m_pNode->WriteDataList(WriteProps, const_cast<const VOID**>(pData), pDataCount, CAMX_ARRAY_SIZE(WriteProps));

    static const UINT WriteFOVCProps[]                    = { PropertyIDFOVCFrameInfo };
    UINT pFOVCDataCount[CAMX_ARRAY_SIZE(WriteFOVCProps)]  = { sizeof(FOVCOutput) };
    if (NULL != pData[2])
    {
        FOVCOutput* pOutput = &reinterpret_cast<AFFrameInformation*>(pData[2])->fovcOutput;
        const VOID* pFOVCData[CAMX_ARRAY_SIZE(WriteFOVCProps)] = { pOutput };
        m_pNode->WriteDataList(WriteFOVCProps, pFOVCData, pFOVCDataCount, CAMX_ARRAY_SIZE(WriteFOVCProps));
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "Failed to find valid AFFrameInfo");
        FOVCOutput output                                      = { 0 };
        const VOID* pFOVCData[CAMX_ARRAY_SIZE(WriteFOVCProps)] = { &output };
        m_pNode->WriteDataList(WriteFOVCProps, pFOVCData, pFOVCDataCount, CAMX_ARRAY_SIZE(WriteFOVCProps));
    }


    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishPDLibOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::PublishPDLibOutput(
    PDLibInputs*  pPDInputData,
    PDLibOutputs* pPDOutputData)
{
    BOOL                isEarlyPDPublished                          = FALSE;
    static const UINT   WriteProps[]                                = { PropertyIDBasePDInternal };
    const VOID*         pOutputData[CAMX_ARRAY_SIZE(WriteProps)]    = { &isEarlyPDPublished };
    UINT                pDataCount[CAMX_ARRAY_SIZE(WriteProps)]     = { sizeof(isEarlyPDPublished) };
    UINT32              PDIndex;

    if (PDTriggerEarly == pPDInputData->trigger)
    {
        isEarlyPDPublished          = TRUE;
        m_PDAFDataInput.triggerType = PDTriggerEarly;
        PDIndex                     = PDTriggerEarly;
        CAMX_LOG_INFO(CamxLogGroupAF, "PDAF log early interrupt.");
    }
    else
    {
        isEarlyPDPublished          = FALSE;
        m_PDAFDataInput.triggerType = PDTriggerNormal;
        PDIndex                     = PDTriggerNormal;
        CAMX_LOG_INFO(CamxLogGroupAF, "PDAF log end of frame.");
    }
    m_pNode->WriteDataList(WriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(WriteProps));

    // Save main defocus value
    for (UINT32 idx = 0; idx < PDLibMaxWindowCount; idx++)
    {
        m_PDAlgoOutputs[PDIndex].defocus.defocus[idx]           = pPDOutputData->libOutput.defocus[idx].defocus;
        m_PDAlgoOutputs[PDIndex].defocus.confidenceLevel[idx]   = pPDOutputData->libOutput.defocus[idx].confidence;
        m_PDAlgoOutputs[PDIndex].defocus.phaseDifference[idx]   = pPDOutputData->libOutput.defocus[idx].phaseDiff;
    }
    m_PDAFDataInput.pDefocus.pDefocus                    = m_PDAlgoOutputs[PDIndex].defocus.defocus;
    m_PDAFDataInput.pDefocus.pConfidenceLevel            = m_PDAlgoOutputs[PDIndex].defocus.confidenceLevel;
    m_PDAFDataInput.pDefocus.pPhaseDifference            = m_PDAlgoOutputs[PDIndex].defocus.phaseDifference;
    m_PDAFDataInput.pDefocus.xWindowCount                 = pPDOutputData->libOutput.horizontalWindowCount;
    m_PDAFDataInput.pDefocus.yWindowCount                 = pPDOutputData->libOutput.verticalWindowCount;

    CAMX_LOG_VERBOSE(CamxLogGroupAF, "PDaf input pDefocus %d pConfidenceLevel %d pPhaseDifference %f",
    m_PDAFDataInput.pDefocus.pDefocus[0], m_PDAFDataInput.pDefocus.pConfidenceLevel[0],
    m_PDAFDataInput.pDefocus.pPhaseDifference[0]);

    // Save peripheral defocus value
    for (UINT32 idx = 0; idx < PDLibPeripheralWindowMax; idx++)
    {
        m_PDAlgoOutputs[PDIndex].peripheralDefocus.defocus[idx]         =
            pPDOutputData->libOutput.resultPeripheral[idx].defocus;
        m_PDAlgoOutputs[PDIndex].peripheralDefocus.confidenceLevel[idx] =
            pPDOutputData->libOutput.resultPeripheral[idx].confidence;
        m_PDAlgoOutputs[PDIndex].peripheralDefocus.phaseDifference[idx] =
            pPDOutputData->libOutput.resultPeripheral[idx].phaseDiff;
    }
    m_PDAFDataInput.pPeripheralDefocus.pDefocus          = m_PDAlgoOutputs[PDIndex].peripheralDefocus.defocus;
    m_PDAFDataInput.pPeripheralDefocus.pConfidenceLevel  = m_PDAlgoOutputs[PDIndex].peripheralDefocus.confidenceLevel;
    m_PDAFDataInput.pPeripheralDefocus.pPhaseDifference  = m_PDAlgoOutputs[PDIndex].peripheralDefocus.phaseDifference;
    if (pPDOutputData->libOutput.isPeripheralValid)
    {
        m_PDAFDataInput.pPeripheralDefocus.xWindowCount  = PDLibPeripheralWindowMax;
        m_PDAFDataInput.pPeripheralDefocus.yWindowCount  = 1;
    }
    else
    {
        m_PDAFDataInput.pPeripheralDefocus.xWindowCount  = 0;
        m_PDAFDataInput.pPeripheralDefocus.yWindowCount  = 0;
    }

    if (PDTriggerNormal == m_PDAFDataInput.triggerType)
    {
        // Assigning output to output map
        for (UINT32 i = 0; i < PDLibMaxMapGridCount; i++)
        {
            m_PDAlgoOutputs[PDIndex].depthMapInfo.PDMap[i] =
                pPDOutputData->libOutput.depthMapInfo.PDMap[i];
            m_PDAlgoOutputs[PDIndex].depthMapInfo.ConfidenceMap[i] =
                pPDOutputData->libOutput.depthMapInfo.ConfidenceMap[i];
            m_PDAlgoOutputs[PDIndex].depthMapInfo.DefocusRangeNearMap[i] =
                pPDOutputData->libOutput.depthMapInfo.DefocusRangeNearMap[i];
            m_PDAlgoOutputs[PDIndex].depthMapInfo.DefocusRangeFarMap[i] =
                pPDOutputData->libOutput.depthMapInfo.DefocusRangeFarMap[i];
            m_PDAlgoOutputs[PDIndex].depthMapInfo.DefocusMap[i] =
                pPDOutputData->libOutput.depthMapInfo.DefocusMap[i];
        }


        m_PDAFDataInput.pDepthMap.PDMap = m_PDAlgoOutputs[PDIndex].depthMapInfo.PDMap;
        m_PDAFDataInput.pDepthMap.ConfidenceMap = m_PDAlgoOutputs[PDIndex].depthMapInfo.ConfidenceMap;
        m_PDAFDataInput.pDepthMap.DefocusRangeNearMap = m_PDAlgoOutputs[PDIndex].depthMapInfo.DefocusRangeNearMap;
        m_PDAFDataInput.pDepthMap.DefocusRangeFarMap = m_PDAlgoOutputs[PDIndex].depthMapInfo.DefocusRangeFarMap;
        m_PDAFDataInput.pDepthMap.DefocusMap = m_PDAlgoOutputs[PDIndex].depthMapInfo.DefocusMap;

        m_PDAFDataInput.pDepthMap.horiNumOfRegion = pPDOutputData->libOutput.depthMapInfo.horiNumOfRegion;
        m_PDAFDataInput.pDepthMap.vertNumOfRegion = pPDOutputData->libOutput.depthMapInfo.vertNumOfRegion;
        m_PDAFDataInput.pDepthMap.widthOfRegion = pPDOutputData->libOutput.depthMapInfo.widthOfRegion;
        m_PDAFDataInput.pDepthMap.heightOfRegion = pPDOutputData->libOutput.depthMapInfo.heightOfRegion;
        m_PDAFDataInput.pDepthMap.horiRegionOffset = pPDOutputData->libOutput.depthMapInfo.horiRegionOffset;
        m_PDAFDataInput.pDepthMap.vertRegionOffset = pPDOutputData->libOutput.depthMapInfo.vertRegionOffset;
    }

    if (PDTriggerNormal == m_PDAFDataInput.triggerType)
    {
        static const UINT WritePropsInner[]                                  = { PropertyIDAFPDFrameInfo };
        const VOID*       pOutputDataInner[CAMX_ARRAY_SIZE(WritePropsInner)] = { &m_PDAFDataInput };
        UINT              pDataCountInner[CAMX_ARRAY_SIZE(WritePropsInner)]  = { sizeof(m_PDAFDataInput) };

        m_pNode->WriteDataList(WritePropsInner, pOutputDataInner, pDataCountInner, CAMX_ARRAY_SIZE(WritePropsInner));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishOutput(
    const AFAlgoOutputList*         pOutput,
    const AFHALOutputList*          pHALOutput)
{
    CamxResult result = PublishFrameControl(pOutput, &m_frameControl);

    if (CamxResultSuccess == result)
    {
        result = PublishStatsControl(pOutput, &m_statsControl);
        DumpAFStats();
    }

    if (CamxResultSuccess == result)
    {
        result = PublishFrameInfo(pOutput, &m_AFFrameInfo);
        DumpFrameInfo();
    }

    if (CamxResultSuccess == result)
    {
        result = PublishExternalCameraMetadata(pOutput, pHALOutput);
    }

    if ((CamxResultSuccess == result) && (TRUE == m_isPDAFEnabled))
    {
        result = PublishHWPDConfig();
    }

    if (CamxResultSuccess == result)
    {
        result = PublishPropertyDebugData();
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "Failed to publish the AF output!");
    }

    m_pStatsAFPropertyReadWrite->WriteProperties(NumAFPropertyWriteTags);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFIOUtil::PublishHALMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishHALMetadata(
    const AFHALOutputList*  pHALOutput,
    AFHALData*              pAFHALData)
{
    CAMX_ASSERT_MESSAGE(NULL != pHALOutput, "pHALOutput NULL pointer");
    CAMX_ASSERT_MESSAGE(NULL != pAFHALData, "pAFHALData NULL pointer");

    CamxResult            result          = CamxResultSuccess;
    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    // publish hal output
    for (UINT32 i = 0; i < pHALOutput->outputCount; i++)
    {
        switch (pHALOutput->pAFOutputs[i].outputType)
        {
            case AFHALOutputTypeControlAFState:
                {
                    ControlAFStateValues afState = m_AFState;

                    if (TRUE == pStaticSettings->force3ALockedResult)
                    {
                        afState = ControlAFStateFocusedLocked;
                    }
                    pAFHALData->afState = afState;
                    break;
                }
            case AFHALOutputTypeControlFocusMode:
                {
                    pAFHALData->afMode = *static_cast<ControlAFModeValues*>(pHALOutput->pAFOutputs[i].pAFOutput);
                    break;
                }
            case AFHALOutputTypeControlAFRegions:
                {

                    AFROIDimension* pAFROIInfo = &m_AFOutputROIDim;
                    FLOAT widthRatio           = static_cast <FLOAT> (m_sensorActiveArrayWidth) /
                        static_cast<FLOAT>(m_sensorInfoInput.resolutionWidth);
                    FLOAT heightRatio          = static_cast<FLOAT>(m_sensorActiveArrayHeight) /
                        static_cast<FLOAT>(m_sensorInfoInput.resolutionHeight);

                    WeightedRegion weightedRegion;

                    weightedRegion.weight = pAFROIInfo->weight;
                    weightedRegion.xMin = Utils::RoundFLOAT(pAFROIInfo->left * widthRatio);
                    weightedRegion.yMin = Utils::RoundFLOAT(pAFROIInfo->top * heightRatio);
                    weightedRegion.xMax = Utils::RoundFLOAT((pAFROIInfo->left * widthRatio)) +
                        Utils::RoundFLOAT((pAFROIInfo->width * widthRatio));
                    weightedRegion.yMax = Utils::RoundFLOAT((pAFROIInfo->top * heightRatio)) +
                        Utils::RoundFLOAT((pAFROIInfo->height) * (heightRatio));

                    CAMX_LOG_VERBOSE(CamxLogGroupAF,
                        "Published ROI weight %d x %d y %d dx %d dy %d",
                        weightedRegion.weight,
                        weightedRegion.xMin,
                        weightedRegion.yMin,
                        weightedRegion.xMax - weightedRegion.xMin,
                        weightedRegion.yMax - weightedRegion.yMin);

                    pAFHALData->weightedRegion = weightedRegion;
                    break;
                }
            case AFHALOutputTypeLensFocusDistance:
                {
                    pAFHALData->lensFocusDistance = *static_cast<FLOAT*>(pHALOutput->pAFOutputs[i].pAFOutput);
                    break;
                }
            case AFHALOutputTypeFocusValue:
                {
                    UINT32        metaTag = 0;

                    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.focusvalue",
                                                                      "FocusValue",
                                                                      &metaTag);
                    if (CamxResultSuccess == result)
                    {
                        static const UINT WriteProps[]                             = {metaTag};
                        const VOID*       pOutputData[CAMX_ARRAY_SIZE(WriteProps)] = { pHALOutput->pAFOutputs[i].pAFOutput };
                        UINT              pDataCount[CAMX_ARRAY_SIZE(WriteProps)]  =
                        { sizeof(m_AFOutputFocusValue)/sizeof(FLOAT) };

                        result = m_pNode->WriteDataList(WriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(WriteProps));
                    }
                    else
                    {
                        CAMX_LOG_ERROR(CamxLogGroupAF, "VendorTag Query failed");
                    }
                }
                break;
            default:
                CAMX_LOG_ERROR(CamxLogGroupAF, "Invalid output type: %d", pHALOutput->pAFOutputs[i].outputType);
                break;
        }
    }

    pAFHALData->afTriggerValues = m_controlAFTriggerValues;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFIOUtil::PublishVendorTagMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishVendorTagMetadata(
    const AFAlgoOutputList* pOutput)
{
    CamxResult result = CamxResultSuccess;

    if (0 != pOutput->pAFOutputs[AFOutputPublishingVendorTagsInfo].sizeOfWrittenAFOutput)
    {
        StatsVendorTagList* pVendorTagOutput =
            static_cast<StatsVendorTagList*>(pOutput->pAFOutputs[AFOutputPublishingVendorTagsInfo].pAFOutput);
        CAMX_ASSERT_MESSAGE(NULL != pVendorTagOutput, "pVendorTagOutput NULL pointer");

        for (UINT32 i = 0; i < pVendorTagOutput->vendorTagCount; i++)
        {
            if (0 == pVendorTagOutput->vendorTag[i].sizeOfWrittenData)
            {
                continue;
            }

            static const UINT WriteProps[]                             = {pVendorTagOutput->vendorTag[i].vendorTagId};
            const VOID*       pOutputData[CAMX_ARRAY_SIZE(WriteProps)] = { pVendorTagOutput->vendorTag[i].pData };
            UINT              pDataCount[CAMX_ARRAY_SIZE(WriteProps)]  = { 1 };

            result = m_pNode->WriteDataList(WriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(WriteProps));
            CAMX_LOG_VERBOSE(CamxLogGroupAF,
                             "Published VendorTag(%u) size(%d) pData(%p). Result = %d",
                              pVendorTagOutput->vendorTag[i].vendorTagId,
                              pVendorTagOutput->vendorTag[i].dataSize,
                              pVendorTagOutput->vendorTag[i].pData,
                              result);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFIOUtil::PublishExternalCameraMetadata()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishExternalCameraMetadata(
    const AFAlgoOutputList* pOutput,
    const AFHALOutputList*  pHALOutput)
{
    CamxResult result = CamxResultSuccess;

    result = PublishHALMetadata(pHALOutput, &m_AFHALData);
    if (CamxResultSuccess == result)
    {
        result = PublishVendorTagMetadata(pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishPropertyDebugData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishPropertyDebugData()
{
    CamxResult          result                      = CamxResultSuccess;
    UINT32              metaTag                     = 0;
    static const UINT StatsData[] =
    {
        PropertyIDDebugDataAll,
    };

    static const UINT StatsLength = CAMX_ARRAY_SIZE(StatsData);
    VOID*             pData[StatsLength] = { 0 };
    pData[0] = m_pStatsAFPropertyReadWrite->GetPropertyData(AFReadTypePropertyIDDebugDataAll);

    // Current framework implementation support only posting DebugDataAll
    result = VendorTagManager::QueryVendorTagLocation("org.quic.camera.debugdata", "DebugDataAll", &metaTag);
    if (CamxResultSuccess == result)
    {
        if (NULL != pData[0])
        {
            const UINT  DebugDataVendorTag[]    = { metaTag };
            const VOID* pDstData[1]             = { pData[0] };
            UINT        pDataCount[1]           = { sizeof(DebugData) };
            result = m_pNode->WriteDataList(DebugDataVendorTag, pDstData, pDataCount, CAMX_ARRAY_SIZE(DebugDataVendorTag));
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupAF, "PropertyIDDebugDataAll is NULL");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFIOUtil::PopulatePDHWConfiguration
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::PopulatePDHWConfiguration(
    PDHwConfig* pPDHWConfig)
{
    CamX::Utils::Memset(pPDHWConfig, 0, sizeof(PDHwConfig));

    GetHWPDConfig(m_pPDLib, pPDHWConfig);

    CAMX_LOG_VERBOSE(CamxLogGroupAF, "PD HW settings"
               "hOffset %x "
               "vOffset %x"
               "verticalBinningLineCount %x "
               "lastPixelCrop %x",
               pPDHWConfig->SADConfig.horizontalOffset,
               pPDHWConfig->SADConfig.verticalOffset,
               pPDHWConfig->binConfig.verticalBinningLineCount,
               pPDHWConfig->cropConfig.lastPixelCrop);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFIOUtil::PublishHWPDConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishHWPDConfig()
{
    CamxResult  result      = CamxResultSuccess;
    PDHwConfig  PDHwConfig;

    CamX::Utils::Memset(&PDHwConfig, 0 , sizeof(PDHwConfig));

    result = GetHWPDConfig(m_pPDLib, &PDHwConfig);

    if (CamxResultSuccess == result)
    {
        static const UINT WriteProps[] =
        {
            PropertyIDPDHwConfig
        };

        UINT pDataCount[CAMX_ARRAY_SIZE(WriteProps)] =
        {
            sizeof(PDHwConfig)
        };

        const VOID* pPDHwConfigData[CAMX_ARRAY_SIZE(WriteProps)]    = { &PDHwConfig };

        m_pNode->WriteDataList(WriteProps, pPDHwConfigData,
            pDataCount, CAMX_ARRAY_SIZE(WriteProps));

        CAMX_LOG_VERBOSE(CamxLogGroupAF, "PD HW settings"
            "hOffset %x "
            "vOffset %x"
            "verticalBinningLineCount %x "
            "lastPixelCrop %x",
            PDHwConfig.SADConfig.horizontalOffset,
            PDHwConfig.SADConfig.verticalOffset,
            PDHwConfig.binConfig.verticalBinningLineCount,
            PDHwConfig.cropConfig.lastPixelCrop);

        DumpHWPDConfig(&PDHwConfig);
    }

    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::UpdatePropertyMoveLensOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::UpdatePropertyMoveLensOutput(
    MoveLensOutput*     pMoveLensOutput,
    AFLensPositionInfo* pLensMoveInfo
    ) const
{
    CAMX_ASSERT_MESSAGE(NULL != pMoveLensOutput, "Move Lens Output NULL pointer");
    CAMX_ASSERT_MESSAGE(NULL != pLensMoveInfo, "Lens Move Info NULL pointer");

    if (AFLensMoveTypeLogical == pLensMoveInfo->moveType)
    {
        pMoveLensOutput->targetLensPosition = pLensMoveInfo->logicalLensMoveInfo.lensPositionInLogicalUnits;
        pMoveLensOutput->useDACValue        = FALSE;
    }
    else if (AFLensMoveTypeDAC == pLensMoveInfo->moveType)
    {
        pMoveLensOutput->targetLensPosition = pLensMoveInfo->digitalLensMoveInfo.lensPositionInDACUnits;
        pMoveLensOutput->useDACValue        = TRUE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::UpdatePropertyBFConfigOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::UpdatePropertyBFConfigOutput(
    AFConfigParams*             pAFConfig,
    AFBAFFloatingWindowConfig*  pBAFFwConfig
    ) const
{
    CAMX_ASSERT(NULL != pAFConfig);
    CAMX_ASSERT(NULL != pBAFFwConfig);

    SIZE_T paramSize = 0;

    CAMX_ASSERT(NULL != pBAFFwConfig->pBAFInputConfig);

    pAFConfig->mask                                 = BFStats;
    pAFConfig->BFStats.BFInputConfig.BFInputGSel    =
        static_cast<BFInputGSelectType>(pBAFFwConfig->pBAFInputConfig->BAFInputGSel);
    pAFConfig->BFStats.BFInputConfig.BFChannelSel   =
        static_cast<BFChannelSelectType>(pBAFFwConfig->pBAFInputConfig->BAFChannelSel);
    pAFConfig->BFStats.BFInputConfig.isValid        = pBAFFwConfig->pBAFInputConfig->isValid;

    paramSize = CamX::Utils::MinUINT32(pBAFFwConfig->pBAFInputConfig->numberOfYConfig, MaxYConfig);

    CamX::Utils::Memcpy(pAFConfig->BFStats.BFInputConfig.YAConfig,
                        pBAFFwConfig->pBAFInputConfig->pYConfig,
                        paramSize * sizeof(FLOAT));

    CAMX_ASSERT(NULL != pBAFFwConfig->pBAFGammaLUTConfig);
    paramSize = pBAFFwConfig->pBAFGammaLUTConfig->numGammaLUT;

    CamX::Utils::Memcpy(pAFConfig->BFStats.BFGammaLUTConfig.gammaLUT,
                        pBAFFwConfig->pBAFGammaLUTConfig->pGammaLUT,
                        paramSize * sizeof(UINT32));

    pAFConfig->BFStats.BFGammaLUTConfig.isValid         = (pBAFFwConfig->pBAFGammaLUTConfig->numGammaLUT > 0) ? TRUE : FALSE;
    pAFConfig->BFStats.BFGammaLUTConfig.numGammaLUT     = pBAFFwConfig->pBAFGammaLUTConfig->numGammaLUT;

    CAMX_ASSERT(NULL != pBAFFwConfig->pBAFScaleConfig);
    pAFConfig->BFStats.BFScaleConfig.BFScaleEnable      = pBAFFwConfig->pBAFScaleConfig->BAFScaleEnable;
    pAFConfig->BFStats.BFScaleConfig.scaleM             = pBAFFwConfig->pBAFScaleConfig->scaleM;
    pAFConfig->BFStats.BFScaleConfig.scaleN             = pBAFFwConfig->pBAFScaleConfig->scaleN;
    pAFConfig->BFStats.BFScaleConfig.isValid            = TRUE;

    AFBAFFilterConfig*    pFilterConfig     = NULL;
    BFFilterConfigParams* pBFFilterConfig   = NULL;

    // Fill BF filter configuration parameters
    for (UINT8 j = 0; j < BFFilterTypeCount; j++)
    {
        pBFFilterConfig     = &pAFConfig->BFStats.BFFilterConfig[j];
        if (j == 0)
        {
            pFilterConfig   = pBAFFwConfig->pBAFFilterConfigH1;
        }
        else if (j == 1)
        {
            pFilterConfig   = pBAFFwConfig->pBAFFilterConfigH2;
        }
        else
        {
            pFilterConfig   = pBAFFwConfig->pBAFFilterConfigV;
        }

        pBFFilterConfig->isValid                                    = pFilterConfig->isValid;
        pBFFilterConfig->horizontalScaleEnable                      = pFilterConfig->horizontalScaleEnable;
        pBFFilterConfig->BFFIRFilterConfig.enable                   = pFilterConfig->pBAFFIRFilterConfig->enable;
        pBFFilterConfig->BFFIRFilterConfig.numOfFIRCoefficients     = pFilterConfig->pBAFFIRFilterConfig->numOfFIRCoefficients;

        for (UINT8 i = 0; i < pFilterConfig->pBAFFIRFilterConfig->numOfFIRCoefficients; i++)
        {
            pBFFilterConfig->BFFIRFilterConfig.FIRFilterCoefficients[i] =
                pFilterConfig->pBAFFIRFilterConfig->pFIRFilterCoefficients[i];
        }

        pBFFilterConfig->BFIIRFilterConfig.enable                   = pFilterConfig->pBAFIIRFilterConfig->enable;
        pBFFilterConfig->BFIIRFilterConfig.a11                      = pFilterConfig->pBAFIIRFilterConfig->a11;
        pBFFilterConfig->BFIIRFilterConfig.a12                      = pFilterConfig->pBAFIIRFilterConfig->a12;
        pBFFilterConfig->BFIIRFilterConfig.a21                      = pFilterConfig->pBAFIIRFilterConfig->a21;
        pBFFilterConfig->BFIIRFilterConfig.a22                      = pFilterConfig->pBAFIIRFilterConfig->a22;
        pBFFilterConfig->BFIIRFilterConfig.b10                      = pFilterConfig->pBAFIIRFilterConfig->b10;
        pBFFilterConfig->BFIIRFilterConfig.b11                      = pFilterConfig->pBAFIIRFilterConfig->b11;
        pBFFilterConfig->BFIIRFilterConfig.b12                      = pFilterConfig->pBAFIIRFilterConfig->b12;
        pBFFilterConfig->BFIIRFilterConfig.b20                      = pFilterConfig->pBAFIIRFilterConfig->b20;
        pBFFilterConfig->BFIIRFilterConfig.b21                      = pFilterConfig->pBAFIIRFilterConfig->b21;
        pBFFilterConfig->BFIIRFilterConfig.b22                      = pFilterConfig->pBAFIIRFilterConfig->b22;
        pBFFilterConfig->shiftBits                                  = pFilterConfig->shiftBits;
        pBFFilterConfig->BFFilterCoringConfig.threshold             = pFilterConfig->pBAFFilterCoringConfig->threshold;
        pBFFilterConfig->BFFilterCoringConfig.gain                  = pFilterConfig->pBAFFilterCoringConfig->gain;

        for (UINT8 i = 0; i < pFilterConfig->pBAFFilterCoringConfig->numberOfCoringFilter; i++)
        {
            pBFFilterConfig->BFFilterCoringConfig.core[i] = pFilterConfig->pBAFFilterCoringConfig->pCoringFilter[i];
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::UpdatePropertyROIOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::UpdatePropertyROIOutput(
    BFStatsConfigParams*            pBFConfig,
    AFBAFFloatingWindowROIConfig*   pBAFFwROIConfig)
{
    CAMX_ASSERT(NULL != pBFConfig);
    CAMX_ASSERT(NULL != pBAFFwROIConfig);

    UINT32 height           = 0;
    UINT32 top              = 0;
    m_maxPrimaryROIHeight   = 0;

    pBFConfig->BFStatsROIConfig.BFROIType                               =
        static_cast<BFStatsROIType>(pBAFFwROIConfig->BAFROIType);
    pBFConfig->BFStatsROIConfig.numBFStatsROIDimension                  =
        (TRUE == pBAFFwROIConfig->isValid) ? pBAFFwROIConfig->numberOfROI : 0;

    for (UINT16 i = 0; i < pBFConfig->BFStatsROIConfig.numBFStatsROIDimension; i++)
    {
        pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].isValid      = pBAFFwROIConfig->pBAFFwROIDimension[i].isValid;
        pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].region       =
            static_cast<BFStatsRegionType>(pBAFFwROIConfig->pBAFFwROIDimension[i].regionType);
        pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].regionNum    = pBAFFwROIConfig->pBAFFwROIDimension[i].regionNum;
        pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].ROI.height   = pBAFFwROIConfig->pBAFFwROIDimension[i].ROI.height;
        pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].ROI.width    = pBAFFwROIConfig->pBAFFwROIDimension[i].ROI.width;
        pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].ROI.left     = pBAFFwROIConfig->pBAFFwROIDimension[i].ROI.left;
        pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].ROI.top      = pBAFFwROIConfig->pBAFFwROIDimension[i].ROI.top;
    }

    // Update maximum primary roi height
    for (UINT16 i = 0; i < pBFConfig->BFStatsROIConfig.numBFStatsROIDimension; i++)
    {
        if (TRUE == pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].isValid &&
            BFStatsPrimaryRegion == pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].region)
        {
            height  = pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].ROI.height;
            top     = pBFConfig->BFStatsROIConfig.BFStatsROIDimension[i].ROI.top;

            if (height + top > m_maxPrimaryROIHeight)
            {
                m_maxPrimaryROIHeight = height + top;
            }
        }

    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::UpdatePropertyFocusStatus
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::UpdatePropertyFocusStatus(
    AFStatusParams*    pFocusStatus,
    AFAlgoStatusParam* pAlgoOutSatus)
{

    CAMX_ASSERT_MESSAGE(NULL != pFocusStatus, "Focus Status NULL pointer");
    CAMX_ASSERT_MESSAGE(NULL != pAlgoOutSatus, "Algo Status Param NULL pointer");

    pFocusStatus->focusDone                                 = pAlgoOutSatus->focusDone;
    pFocusStatus->focusDistance[FocusDistanceNearIndex]     = pAlgoOutSatus->focusDistanceNear;
    pFocusStatus->focusDistance[FocusDistanceOptimalIndex]  = pAlgoOutSatus->focusDistanceOptimal;
    pFocusStatus->focusDistance[FocusDistanceFarIndex]      = pAlgoOutSatus->focusDistanceFar;

    switch (pAlgoOutSatus->status)
    {
        case AFAlgoStatusTypeInitialized:
            pFocusStatus->status = AFStatusInitialized;
            break;
        case AFAlgoStatusTypeFocusing:
            pFocusStatus->status = AFStatusFocusing;
            break;
        case AFAlgoStatusTypeFocused:
            pFocusStatus->status = AFStatusFocused;
            break;
        case AFAlgoStatusTypeNotFocused:
            pFocusStatus->status = AFStatusNotFocused;
            break;
        default:
            pFocusStatus->status = AFStatusInvalid;
            break;
    }

    pFocusStatus->status = static_cast<AFStatus>(pAlgoOutSatus->status);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ParseISPStats
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::ParseISPStats(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo)
{
    CAMX_ENTRYEXIT(CamxLogGroupAF);

    CamxResult result       = CamxResultSuccess;
    VOID*      pRawBuffer   = NULL;

    CAMX_ASSERT_MESSAGE(NULL != pStatsProcessRequestDataInfo, "Stats Process Request Data Info NULL pointer");

    // Get the raw stats buffer
    if (NULL == m_pStatsParser)
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "ISP Parser is NULL!");
        result = CamxResultEInvalidState;
    }

    for (INT i = 0; i < pStatsProcessRequestDataInfo->bufferCount; i++)
    {
        if (pStatsProcessRequestDataInfo->bufferInfo[i].statsType != ISPStatsTypeRDIRaw)
        {
            pRawBuffer = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->GetPlaneVirtualAddr(0, 0);

            // Get the raw stats buffer
            if (NULL == pRawBuffer)
            {
                CAMX_LOG_ERROR(CamxLogGroupAF, "Invalid stats buffer to parse");
                result = CamxResultEInvalidPointer;
            }
            else
            {
                result = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->CacheOps(TRUE, FALSE);
            }

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupAF, "Failed to invalidate cached stat buffer.");
            }
            else
            {
                switch (pStatsProcessRequestDataInfo->bufferInfo[i].statsType)
                {
                    case ISPStatsTypeBF:
                        result = ParseStats(PropertyIDParsedBFStatsOutput, pRawBuffer,
                                pStatsProcessRequestDataInfo->requestId);
                        break;
                    case ISPStatsTypeHDRBE:
                        result = ParseStats(PropertyIDParsedHDRBEStatsOutput, pRawBuffer,
                                pStatsProcessRequestDataInfo->requestId);
                        break;
                        // PDAF stats are handled seperately in ParsePDStats function
                    case ISPStatsTypeRDIPDAF:
                    case ISPStatsTypeDualPDHWPDAF:
                    case ISPStatsTypeRDIRaw:
                        break;
                    default:
                        CAMX_LOG_ERROR(
                                CamxLogGroupAF,
                                "Unsupported stats type %d",
                                pStatsProcessRequestDataInfo->bufferInfo[i].statsType);
                        break;
                }
            }
        }
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "Failed to parse the ISP stats!");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishCrossProperty
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishCrossProperty(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo)
{
    CamxResult result = CamxResultSuccess;


    // Publish AF cross pipeline property for dual camera usecase.
    UINT64            requestId                                 = pStatsProcessRequestDataInfo->requestId;
    static const UINT pWriteProps[]                             = { PropertyIDCrossAFStats };
    const VOID*       pOutputData[CAMX_ARRAY_SIZE(pWriteProps)] = { &requestId };
    UINT              pDataCount[CAMX_ARRAY_SIZE(pWriteProps)]  = { sizeof(requestId) };

    result            = m_pNode->WriteDataList(pWriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(pWriteProps));
    CAMX_LOG_VERBOSE(CamxLogGroupAF, "Published PropertyIDCrossAFStats:%X for Req:%llu result:%d",
                     PropertyIDCrossAFStats, requestId, result);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishBAFStatDependencyMet
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishBAFStatDependencyMet(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo)
{
    CamxResult  result      = CamxResultSuccess;
    UINT64      requestId   = pStatsProcessRequestDataInfo->requestId;

    static const UINT pWriteProps[]                             = { PropertyIDAFBAFDependencyMet };
    const VOID*       pOutputData[CAMX_ARRAY_SIZE(pWriteProps)] = { &requestId };
    UINT              pDataCount[CAMX_ARRAY_SIZE(pWriteProps)]  = { sizeof(requestId) };

    result            = m_pNode->WriteDataList(pWriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(pWriteProps));
    CAMX_LOG_VERBOSE(CamxLogGroupAF, "Published PropertyIDAFBAFDependencyMet:%X for Req:%llu result:%d",
                     PropertyIDAFBAFDependencyMet, requestId, result);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PopulatePDInputData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PopulatePDInputData(
    const StatsProcessRequestData*  pStatsProcessRequestDataInfo,
    PDLibInputs*                    pPDInput,
    VOID*                           pPDBuffer,
    INT                             PDBufferFd,
    SIZE_T                          PDBufferSize,
    BOOL                            isPDHWEnabled)
{
    CamxResult              result              = CamxResultSuccess;
    AFAECInfo               AECInfo             = { 0 };
    UINT32                  CAMIFHeight         = 0;
    VOID*                   pRawBuffer          = NULL;

    if (0 == m_sensorInfoInput.cropLastLine)
    {
        CAMIFHeight = m_sensorInfoInput.resolutionHeight;
    }
    else
    {
        CAMIFHeight = m_sensorInfoInput.cropLastLine - m_sensorInfoInput.cropFirstLine + 1;
    }

    // When the BAF dependency is met, we are processing the early PD buffer
    // and hence mark the trigger appropriately
    if (BAFStatsDependencyMet == pStatsProcessRequestDataInfo->processSequenceId)
    {    // Check if it is early interrupt
        pPDInput->trigger               = PDTriggerEarly;
        if (m_maxPrimaryROIHeight < DefaultReadPDMargin)
        {
            pPDInput->numOfWrittenPDlines = 0;
        }
        else if (m_maxPrimaryROIHeight > CAMIFHeight - 30)
        {
            pPDInput->numOfWrittenPDlines = CAMIFHeight;
        }
        else
        {
            pPDInput->numOfWrittenPDlines   = m_maxPrimaryROIHeight - DefaultReadPDMargin;
        }
        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "Early request(%llu) buffer %p numOfLine %d", pStatsProcessRequestDataInfo->requestId,
            static_cast<UINT16*>(pPDBuffer), pPDInput->numOfWrittenPDlines);
    }

    // When PDAF dependency is met, we are processing the end of frame PD buffer
    // and hence mark the trigger appropriately
    if (PDStatDependencyMet == pStatsProcessRequestDataInfo->processSequenceId)
    {
        pPDInput->trigger               = PDTriggerNormal;
        pPDInput->numOfWrittenPDlines   = CAMIFHeight;

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "Normal request(%llu) buffer %p numOfLine %d %d", pStatsProcessRequestDataInfo->requestId,
            static_cast<UINT16*>(pPDBuffer), pPDInput->numOfWrittenPDlines, *static_cast<UINT16*>(pPDBuffer));
    }

    ReadAECInput(&AECInfo);

    // Check if PD port is enabled and if so find the matching PD buffer and parse stats
    for (INT32 i = 0; i < pStatsProcessRequestDataInfo->bufferCount; i++)
    {
        if (ISPStatsTypeRDIRaw == pStatsProcessRequestDataInfo->bufferInfo[i].statsType)
        {
            pRawBuffer = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->GetPlaneVirtualAddr(0, 0);

            // Get the raw stats buffer
            if (NULL != pRawBuffer)
            {
                if (BAFStatsDependencyMet == pStatsProcessRequestDataInfo->processSequenceId)
                {
                    result = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->CacheOps(TRUE, FALSE);
                }

                if (CamxResultSuccess != result)
                {
                    pRawBuffer = NULL;
                    CAMX_LOG_ERROR(CamxLogGroupAF, "Failed to invalidate cached stat buffer.");
                }
            }
            else
            {
                CAMX_LOG_WARN(CamxLogGroupAF, "Failed to get RDIRawPixel Buffer");
            }
            break;
        }
    }

    pPDInput->requestId                 = pStatsProcessRequestDataInfo->requestId;
    UINT64 requestIdOffsetFromLastFlush =
        m_pNode->GetRequestIdOffsetFromLastFlush(pStatsProcessRequestDataInfo->requestId);

    // Update Process Param
    if (TRUE == isPDHWEnabled)
    {
        pPDInput->processParam.pPDHWBuffer = static_cast<UINT64*>(pPDBuffer);
        pPDInput->processParam.PDHWBufferFd = PDBufferFd;
        pPDInput->processParam.PDHWBufferSize = PDBufferSize;
    }
    else
    {
        pPDInput->processParam.pPDBuffer                            = static_cast<UINT16*>(pPDBuffer);
        pPDInput->processParam.PDBufferFd                           = PDBufferFd;
        pPDInput->processParam.PDBufferSize                         = PDBufferSize;
    }
    // @todo (CAMX-2042) fps is not hooked from AEC.
    pPDInput->processParam.currentFps                           = static_cast<FLOAT>(AECInfo.currentFPS);
    pPDInput->processParam.imageAnalogGain                      = static_cast<FLOAT>(AECInfo.currentGain);
    pPDInput->processParam.integrationTime                      = static_cast<FLOAT>(AECInfo.exposureTime);
    pPDInput->processParam.pRawBuffer                           = static_cast<UINT16*>(pRawBuffer);

    pPDInput->processParam.windowConfig                         = m_AFOutputPDAFWindowConfig;
    pPDInput->processParam.lensPosition                         =
        static_cast<INT16>(m_AFOutputLensMoveInfo.logicalLensMoveInfo.lensPositionInLogicalUnits);

    UpdateChiStatsSession(pStatsProcessRequestDataInfo);
    pPDInput->pChiStatsSession = GetChiStatsSession();
    // Update camera information
    pPDInput->processParam.cameraInfo = m_cameraInfo;
    pPDInput->startupMode             =
        (requestIdOffsetFromLastFlush >  m_pNode->GetMaximumPipelineDelay()) ?
        StatisticsStartUpValid : StatisticsStartUpInvalid;
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ParsePDStats
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::ParsePDStats(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo)
{
    CAMX_ENTRYEXIT(CamxLogGroupAF);

    CamxResult      result                  = CamxResultSuccess;
    VOID*           pPDBuffer               = NULL;
    INT             PDBufferFd              = -1;
    SIZE_T          PDBufferSize            = 0;
    VOID*           pPDHWBuffer             = NULL;
    INT             PDHWBufferFd            = -1;
    SIZE_T          PDHWBufferSize          = 0;
    PDLibInputs     PDInput                 = {};
    PDLibOutputs    PDOutput                = {};
    BOOL            isPDHWEnabled           = FALSE;
    ImageBuffer*    pPDImageBuffer          = NULL;
    ImageBuffer*    pPDHWImageBuffer        = NULL;
    UINT64 requestIdOffsetFromLastFlush      =
        m_pNode->GetRequestIdOffsetFromLastFlush(pStatsProcessRequestDataInfo->requestId);

    CAMX_ASSERT_MESSAGE(NULL != pStatsProcessRequestDataInfo, "Stats Process Request Data Info NULL pointer");

    // Get the raw stats buffer
    if (NULL == m_pPDLib)
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "PD Lib is NULL!");
        return CamxResultEInvalidState;
    }

    // Check if PD port is enabled and if so find the matching PD buffer and parse stats
    for (INT32 i = 0; i < pStatsProcessRequestDataInfo->bufferCount; i++)
    {
        if (ISPStatsTypeDualPDHWPDAF == pStatsProcessRequestDataInfo->bufferInfo[i].statsType)
        {
            pPDHWBuffer = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->GetPlaneVirtualAddr(0, 0);
            PDHWBufferFd = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->GetFileDescriptor();
            PDHWBufferSize = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->GetPlaneSize(0);

            isPDHWEnabled = TRUE;
            CAMX_LOG_VERBOSE(CamxLogGroupAF, "ISPStatsTypeDualPDHWPDAF pPDHWBuffer %p", pPDHWBuffer);

            // Get the raw stats buffer
            if (NULL != pPDHWBuffer)
            {
                result = PopulatePDInputData(
                            pStatsProcessRequestDataInfo, &PDInput, pPDHWBuffer, PDHWBufferFd, PDHWBufferSize, isPDHWEnabled);
                pPDHWImageBuffer = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer;
            }
        }
        else if (ISPStatsTypeRDIPDAF == pStatsProcessRequestDataInfo->bufferInfo[i].statsType)
        {
            pPDBuffer   = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->GetPlaneVirtualAddr(0, 0);
            PDBufferFd  = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->GetFileDescriptor();
            PDBufferSize = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer->GetPlaneSize(0);
            isPDHWEnabled   = FALSE;

            CAMX_LOG_VERBOSE(CamxLogGroupAF, "ISPStatsTypeRDIPDAF pPDBuffer %p", pPDBuffer);

            // Get the raw stats buffer
            if (NULL != pPDBuffer)
            {
                result = PopulatePDInputData(
                    pStatsProcessRequestDataInfo, &PDInput, pPDBuffer, PDBufferFd, PDBufferSize, isPDHWEnabled);
                pPDImageBuffer = pStatsProcessRequestDataInfo->bufferInfo[i].pBuffer;

            }
        }
    }

    if ((NULL == pPDBuffer && NULL == pPDHWBuffer))
    {
        if (m_pNode->GetMaximumPipelineDelay() < requestIdOffsetFromLastFlush)
        {
            CAMX_LOG_ERROR(CamxLogGroupAF, "Invalid PD buffer to parse");
            result = CamxResultEInvalidPointer;
        }
    }
    else if (CamxResultSuccess == result)
    {
        m_pPDLib->PDLibProcess(m_pPDLib, &PDInput, &PDOutput);
        PublishPDLibOutput(&PDInput, &PDOutput);

        CAMX_LOG_VERBOSE(CamxLogGroupAF, "PDlib process called for Req %llu",
            pStatsProcessRequestDataInfo->requestId);

    }
    if (PDInput.trigger == PDTriggerNormal)
    {
        if (NULL != pPDHWImageBuffer)
        {
            pPDHWImageBuffer->CacheOps(true, false);
        }
        if (NULL != pPDImageBuffer)
        {
            pPDImageBuffer->CacheOps(true, false);
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::ParseStats
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::ParseStats(
    UINT32        propertyId,
    VOID*         pUnparsedBuffer,
    UINT64        requestId)
{
    CamxResult result    = CamxResultEFailed;
    ParseData  parseData = {m_pNode, FALSE, pUnparsedBuffer, NULL};

    if (0 != m_requestQueueDepth)
    {
        switch (propertyId)
        {
            case PropertyIDParsedBFStatsOutput:
                parseData.pStatsOutput = &m_pBFStatsOutput[requestId % m_requestQueueDepth];
                result = m_pStatsParser->Parse(ISPStatsTypeBF, &parseData);
                break;
            case PropertyIDParsedHDRBEStatsOutput:
                parseData.pStatsOutput = &m_pHDRBEStatsOutput[requestId % m_requestQueueDepth];
                result = m_pStatsParser->Parse(ISPStatsTypeHDRBE, &parseData);
                break;
            default:
                CAMX_ASSERT_ALWAYS_MESSAGE("Unexpected property ID: %d", propertyId);
                break;
        }
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "Failed to parse ISP stats for prop %08x", propertyId);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::AllocateMemoryVendorTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::AllocateMemoryVendorTag(
    AFAlgoGetParam* pAlgoGetParam)
{
    CAMX_ENTRYEXIT(CamxLogGroupAF);
    CAMX_ASSERT_MESSAGE(NULL != pAlgoGetParam, "pAlgoGetParam NULL pointer");

    CamxResult                  result          = CamxResultSuccess;
    AFAlgoGetParamOutputList*   pGetParamOutput = &pAlgoGetParam->outputInfo;

    Utils::Memset(&m_vendorTagPublishDataList, 0, sizeof(m_vendorTagPublishDataList));

    // Allocating mmeory for all vendor tags
    for (UINT32 i = 0; i < pGetParamOutput->getParamOutputCount; i++)
    {
        if (0 == pGetParamOutput->pGetParamOutputs[i].sizeOfWrittenGetParamOutput)
        {
            continue;
        }

        // going through the list of vendor tags, calculating the size and allocating memory
        if (AFGetParamVendorTagList == pGetParamOutput->pGetParamOutputs[i].getParamOutputType)
        {
            StatsVendorTagInfoList* pVendorTagInfoList  =
                static_cast<StatsVendorTagInfoList*>(pGetParamOutput->pGetParamOutputs[i].pGetParamOutput);
            for (UINT32 j = 0; j < pVendorTagInfoList->vendorTagCount; j++)
            {
                SIZE_T size = HAL3MetadataUtil::GetMaxSizeByTag(pVendorTagInfoList->vendorTag[j].vendorTagId);
                if (0 != size)
                {
                    m_vendorTagPublishDataList.vendorTag[j].vendorTagId     = pVendorTagInfoList->vendorTag[j].vendorTagId;
                    m_vendorTagPublishDataList.vendorTag[j].appliedDelay    = pVendorTagInfoList->vendorTag[j].appliedDelay;
                    m_vendorTagPublishDataList.vendorTag[j].pData           = CAMX_CALLOC(size);

                    if (NULL == m_vendorTagPublishDataList.vendorTag[j].pData)
                    {
                        CAMX_LOG_ERROR(CamxLogGroupAF,
                                       "Allocating memory for vendor tagID[%d]: %d failed.",
                                       j,
                                       pVendorTagInfoList->vendorTag[j].vendorTagId);
                        result = CamxResultENoMemory;
                    }
                    else
                    {
                        m_vendorTagPublishDataList.vendorTag[j].dataSize = static_cast<UINT32>(size);
                        m_vendorTagPublishDataList.vendorTagCount++;
                        CAMX_LOG_VERBOSE(CamxLogGroupAF,
                                        "Allocating memory for vendor tagID[%d]: %u ",
                                         j,
                                        pVendorTagInfoList->vendorTag[j].vendorTagId);
                    }
                }
            }
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetDefaultSensorResolution
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::GetDefaultSensorResolution(
    AFSensorInfo** ppSensorInfo)
{
    CamxResult        result                    = CamxResultSuccess;
    UINT              GetProps[] =
    {
        PropertyIDUsecaseSensorCurrentMode,
        PropertyIDUsecaseSensorModes,
        PropertyIDUsecaseIFEInputResolution,
        PropertyIDUsecaseLensInfo
    };
    static const UINT GetPropsLength          = CAMX_ARRAY_SIZE(GetProps);
    VOID*             pData[GetPropsLength]   = { 0 };
    UINT64            offsets[GetPropsLength] = { 0, 0, 0, 0};

    m_pNode->GetDataList(GetProps, pData, offsets, GetPropsLength);

    if (NULL != pData[0])
    {
        m_sensorModeIndex = *reinterpret_cast <UINT*> (pData[0]);

        if (NULL != pData[1])
        {
            UsecaseSensorModes* pSensorModes = reinterpret_cast<UsecaseSensorModes*>(pData[1]);

            m_sensorInfoInput.resolutionHeight =
                static_cast<UINT32>(pSensorModes->allModes[m_sensorModeIndex].resolution.outputHeight);
            m_sensorInfoInput.resolutionWidth =
                static_cast<UINT32>(pSensorModes->allModes[m_sensorModeIndex].resolution.outputWidth);
            *ppSensorInfo = &m_sensorInfoInput;

            m_sensorInfoInput.isPDAFEnabled = m_isPDAFEnabled;
        }
        if (NULL != pData[2])
        {
            IFEInputResolution* pIFEInput  = reinterpret_cast<IFEInputResolution*>(pData[2]);
            m_sensorInfoInput.resolutionHeight =
                static_cast<UINT32>(pIFEInput->resolution.height);
            m_sensorInfoInput.resolutionWidth =
                static_cast<UINT32>(pIFEInput->resolution.width);
        }
        if (NULL != pData[3])
        {
            LensInfo* pLensInfo                   = reinterpret_cast<LensInfo*>(pData[3]);
            m_sensorInfoInput.actuatorSensitivity = pLensInfo->actuatorSensitivity;
            m_sensorInfoInput.fNumber             = pLensInfo->fNumber;
            m_sensorInfoInput.focalLength         = pLensInfo->focalLength;
            m_sensorInfoInput.pixelSize           = pLensInfo->pixelSize;
            m_sensorInfoInput.totalFDistance      = pLensInfo->totalFDistance;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "Sensor modes/Sensor Mode index are not published yet!");
        result = CamxResultEFailed;
    }

    CAMX_LOG_INFO(CamxLogGroupAF, "Default Sensor Resolution: %d W:%u H:%u",
        m_sensorModeIndex,
        m_sensorInfoInput.resolutionHeight,
        m_sensorInfoInput.resolutionWidth);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::LoadPDLibHandleFromUsecasePool
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::LoadPDLibHandleFromUsecasePool()
{

    CAMX_ENTRYEXIT(CamxLogGroupAF);

    CamxResult        result                    = CamxResultSuccess;
    static const UINT GetProps[]                = { PropertyIDUsecasePDLibInfo };
    static const UINT GetPropsLength            = CAMX_ARRAY_SIZE(GetProps);
    VOID*             pData[GetPropsLength]     = { 0 };
    UINT64            offsets[GetPropsLength]   = { 0 };
    UINTPTR_T         PDLibAddr                 = 0;

    m_pNode->GetDataList(GetProps, pData, offsets, GetPropsLength);

    if (NULL != pData[0])
    {
        PDLibAddr = *reinterpret_cast<UINTPTR_T*>(pData[0]);
        m_pPDLib = reinterpret_cast<CHIPDLib*>(PDLibAddr);
    }
    else
    {
        m_pPDLib = NULL;
        CAMX_LOG_ERROR(CamxLogGroupAF, "PropertyIDUsecasePDLibInfo hasn't been published check if sensor has published it.");

    }

    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::IsEarlyPDPublished
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CAFIOUtil::IsEarlyPDPublished()
{
    BOOL          isPublished   = FALSE;

    static const UINT GetProps[]                = { PropertyIDBasePDInternal };
    static const UINT GetPropsLength            = CAMX_ARRAY_SIZE(GetProps);
    VOID*             pData[GetPropsLength]     = { 0 };
    UINT64            offsets[GetPropsLength]   = { 0 };

    m_pNode->GetDataList(GetProps, pData, offsets, GetPropsLength);

    if (NULL != pData[0])
    {
        isPublished = *reinterpret_cast<BOOL*>(pData[0]);
    }

    return isPublished;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PopulateGyroData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PopulateGyroData()
{
    CamxResult result = CamxResultEFailed;
    if (m_pNCSSensorHandleGyro != NULL)
    {
        NCSSensorData* pDataObj = NULL;
        NCSDataGyro* pGyroData = NULL;

        // Max number of gyro samples needed per frame is 3 as of now for HJ AF.
        // For scene detection we are using the first/ latest sample
        pDataObj = m_pNCSSensorHandleGyro->GetLastNSamples(3);

        if (NULL != pDataObj)
        {
            INT index;
            m_AFOutputGyroValue.enabled = TRUE;
            m_AFOutputGyroValue.sampleCount = static_cast<UINT16>(pDataObj->GetNumSamples());
            CAMX_LOG_VERBOSE(CamxLogGroupAF, "sampleCount %d ", m_AFOutputGyroValue.sampleCount);
            for (index = 0; index < m_AFOutputGyroValue.sampleCount; index++)
            {
                pDataObj->SetIndex(index);
                pGyroData = reinterpret_cast<NCSDataGyro*>(pDataObj->GetCurrent());
                if (NULL != pGyroData)
                {
                    m_AFOutputGyroValue.pAngularVelocityX[index] = pGyroData->x;
                    m_AFOutputGyroValue.pAngularVelocityY[index] = pGyroData->y;
                    m_AFOutputGyroValue.pAngularVelocityZ[index] = pGyroData->z;
                    m_AFOutputGyroValue.timeStamp[index] = pGyroData->timestamp;
                    CAMX_LOG_VERBOSE(CamxLogGroupAF, "xyz timestamp  %f %f %f %llu",
                        m_AFOutputGyroValue.pAngularVelocityX[index],
                        m_AFOutputGyroValue.pAngularVelocityY[index],
                        m_AFOutputGyroValue.pAngularVelocityZ[index],
                        m_AFOutputGyroValue.timeStamp[index]);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupAF, "GetCurrrent returned null");
                    break;
                }
            }
            m_pNCSSensorHandleGyro->PutBackDataObj(pDataObj);

            if (index == m_AFOutputGyroValue.sampleCount)
            {
                m_bGyroErrIndicated = FALSE;
                result              = CamxResultSuccess;
            }
        }
        else
        {
            if (FALSE == m_bGyroErrIndicated)
            {
                m_bGyroErrIndicated = TRUE;
                CAMX_LOG_WARN(CamxLogGroupAF, "Gyro samples not available");
            }
            m_AFOutputGyroValue.enabled = FALSE;
            m_AFOutputGyroValue.timeStamp[0] = 0;
            m_AFOutputGyroValue.pAngularVelocityX[0] = 0.0f;
            m_AFOutputGyroValue.pAngularVelocityY[0] = 0.0f;
            m_AFOutputGyroValue.pAngularVelocityZ[0] = 0.0f;
            m_AFOutputGyroValue.sampleCount = 1;
        }
    }
    else
    {
        if (FALSE == m_bGyroErrIndicated)
        {
            m_bGyroErrIndicated = TRUE;
            CAMX_LOG_VERBOSE(CamxLogGroupAF, "NCS Gyro handle is NULL");
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PopulateGravityData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PopulateGravityData()
{
    CamxResult result = CamxResultEFailed;

    if (m_pNCSSensorHandleGravity != NULL)
    {
        NCSSensorData*  pDataObj = NULL;
        NCSDataGravity* pGravityData = NULL;

        // Max number of gravity samples needed per frame is 1 as of now.
        pDataObj = m_pNCSSensorHandleGravity->GetLastNSamples(1);

        if (NULL != pDataObj)
        {
            INT index;
            m_AFOutputGravityValue.enabled = TRUE;
            m_AFOutputGravityValue.sampleCount = static_cast<UINT16>(pDataObj->GetNumSamples());
            CAMX_LOG_VERBOSE(CamxLogGroupAF, "sampleCount %d ", m_AFOutputGravityValue.sampleCount);

            for (index = 0; index < m_AFOutputGravityValue.sampleCount; index++)
            {
                pDataObj->SetIndex(index);
                pGravityData = reinterpret_cast<NCSDataGravity*>(pDataObj->GetCurrent());
                if (NULL != pGravityData)
                {
                    m_AFOutputGravityValue.gravityX[index] = pGravityData->x;
                    m_AFOutputGravityValue.gravityY[index] = pGravityData->y;
                    m_AFOutputGravityValue.gravityZ[index] = pGravityData->z;
                    m_AFOutputGravityValue.timeStamp[index] = pGravityData->timestamp;
                    CAMX_LOG_VERBOSE(CamxLogGroupAF, "index xyz timestamp %d %f %f %f %llu", index,
                        m_AFOutputGravityValue.gravityX[index],
                        m_AFOutputGravityValue.gravityY[index],
                        m_AFOutputGravityValue.gravityZ[index],
                        m_AFOutputGravityValue.timeStamp[index]);
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupAF, "GetCurrrent returned null");
                    break;
                }
            }

            m_pNCSSensorHandleGravity->PutBackDataObj(pDataObj);
            if (index == m_AFOutputGravityValue.sampleCount)
            {
                result = CamxResultSuccess;
            }
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupAF, "Gravity samples not available");
            m_AFOutputGravityValue.enabled = FALSE;
            m_AFOutputGravityValue.timeStamp[0] = 0;
            m_AFOutputGravityValue.gravityX[0] = 0.0f;
            m_AFOutputGravityValue.gravityY[0] = 0.0f;
            m_AFOutputGravityValue.gravityZ[0] = 0.0f;
            m_AFOutputGravityValue.sampleCount = 1;
        }
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupAF, "NCS Gravity handle is NULL");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PopulateTOFData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PopulateTOFData(
    UINT64 requestId)
{
    CamxResult result = CamxResultEFailed;
    DataTOF    data   = {0};

    if (m_pTOFInterfaceObject != NULL)
    {

        // We need only one sample per frame for TOF sensor.
        result = m_pTOFInterfaceObject->GetLastNSamples(&data, 1);
        if (CamxResultSuccess == result)
        {
            m_AFOutputTOFValue.isValid               = TRUE;
            m_AFOutputTOFValue.confidence            = static_cast<INT32>(data.confidence);
            m_AFOutputTOFValue.distanceInMilliMeters = static_cast<INT32>(data.distance);
            m_AFOutputTOFValue.nearLimit             = static_cast<INT32>(data.nearLimit);
            m_AFOutputTOFValue.farLimit              = static_cast<INT32>(data.farLimit);
            m_AFOutputTOFValue.maxDistance           = data.maxRange;
            m_AFOutputTOFValue.timestamp             = data.timestamp;
            m_AFOutputTOFValue.requestId             = static_cast<UINT32>(requestId);
            CAMX_LOG_VERBOSE(CamxLogGroupAF, "TOF data confidence %d Distance %d"
                "Limit %d %d maxDistance %d timestamp %lld",
                m_AFOutputTOFValue.confidence,
                m_AFOutputTOFValue.distanceInMilliMeters,
                m_AFOutputTOFValue.nearLimit,
                m_AFOutputTOFValue.farLimit,
                m_AFOutputTOFValue.maxDistance,
                m_AFOutputTOFValue.timestamp);
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupAF, "TOF samples not available");
            m_AFOutputTOFValue.isValid               = FALSE;
            m_AFOutputTOFValue.confidence            = 0;
            m_AFOutputTOFValue.distanceInMilliMeters = 0;
            m_AFOutputTOFValue.nearLimit             = 0;
            m_AFOutputTOFValue.farLimit              = 0;
            m_AFOutputTOFValue.maxDistance           = 0;
            m_AFOutputTOFValue.timestamp             = 0;
            m_AFOutputTOFValue.requestId             = 0;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "TOF interface object is NULL");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::SetupTOFLink
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::SetupTOFLink(
    ChiContext* pChiContext)
{
    CamxResult result = CamxResultSuccess;

    if (NULL == m_pTOFInterfaceObject)
    {
        m_pTOFInterfaceObject = TOFSensorIntf::GetInstance(pChiContext);
        if (NULL != m_pTOFInterfaceObject)
        {
            TOFSensorConfig sensorConfig = { 0 };

            const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

            CAMX_LOG_VERBOSE(CamxLogGroupAF, "Setting up an TOF interface link SR = %f, RR = %d",
                pStaticSettings->TOFSensorSamplingRate,
                pStaticSettings->TOFDataReportRate);

            sensorConfig.samplingRate = pStaticSettings->TOFSensorSamplingRate;
            sensorConfig.reportRate   = pStaticSettings->TOFDataReportRate;
            result                    = m_pTOFInterfaceObject->ConfigureTOFSensor(&sensorConfig);

            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupAF, "Unable to configure TOF sensor !!");
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupAF, "Failed to create TOF interface m_pTOFInterfaceObject %p result %d",
                m_pTOFInterfaceObject, result);
            result = CamxResultEFailed;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::SetupNCSLink
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::SetupNCSLink()
{
    CamxResult result = CamxResultEFailed;

    CAMX_LOG_VERBOSE(CamxLogGroupAF, "Setting up an NCS link");

    // Get the NCS service object handle
    m_pNCSServiceObject = reinterpret_cast<NCSService *>(HwEnvironment::GetInstance()->GetNCSObject());
    if (NULL != m_pNCSServiceObject)
    {
        result = SetupNCSLinkForSensor(NCSGyroType);
        if (result != CamxResultSuccess)
        {
            CAMX_LOG_ERROR(CamxLogGroupAF, "Unable to Setup NCS Link For Gyro Sensor");
        }
        result = SetupNCSLinkForSensor(NCSGravityType);
        if (result != CamxResultSuccess)
        {
            CAMX_LOG_ERROR(CamxLogGroupAF, "Unable to Setup NCS Link For Gravity Sensor");
        }
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupAF, "Unable to get NCS service object");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::SetupNCSLinkForSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::SetupNCSLinkForSensor(
    NCSSensorType  sensorType)
{
    CamxResult      result = CamxResultEFailed;
    NCSSensorConfig sensorConfig = { 0 };
    NCSSensorCaps   sensorCaps;

    CAMX_LOG_VERBOSE(CamxLogGroupAF, "Setting up an NCS link for sensor %d", sensorType);

    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    result = m_pNCSServiceObject->QueryCapabilites(&sensorCaps, sensorType);
    if (result == CamxResultSuccess)
    {
        switch (sensorType)
        {
            case NCSGyroType:
                // Clients responsibility to set it to that config which is supported
                sensorConfig.samplingRate = pStaticSettings->gyroSensorSamplingRate;
                sensorConfig.reportRate   = pStaticSettings->gyroDataReportRate;
                CAMX_LOG_VERBOSE(CamxLogGroupAF, "SR = %f. RR = %d",
                    pStaticSettings->gyroSensorSamplingRate,
                    pStaticSettings->gyroDataReportRate);
                sensorConfig.operationMode = 0;   // batched reporting mode
                sensorConfig.sensorType = sensorType;
                m_pNCSSensorHandleGyro = m_pNCSServiceObject->RegisterService(sensorType, &sensorConfig);
                if (NULL == m_pNCSSensorHandleGyro)
                {
                    CAMX_LOG_ERROR(CamxLogGroupAF, "Unable to register %d with the NCS !!", sensorType);
                    result = CamxResultEFailed;
                }
                break;
            case NCSGravityType:
                // Clients responsibility to set it to that config which is supported
                sensorConfig.samplingRate = pStaticSettings->gravitySensorSamplingRate;
                sensorConfig.reportRate   = pStaticSettings->gravityDataReportRate;
                CAMX_LOG_VERBOSE(CamxLogGroupAF, "SR = %f. RR = %d",
                    pStaticSettings->gravitySensorSamplingRate,
                    pStaticSettings->gravityDataReportRate);
                sensorConfig.operationMode = 0;   // batched reporting mode
                sensorConfig.sensorType = sensorType;
                m_pNCSSensorHandleGravity = m_pNCSServiceObject->RegisterService(sensorType, &sensorConfig);
                if (NULL == m_pNCSSensorHandleGravity)
                {
                    CAMX_LOG_ERROR(CamxLogGroupAF, "Unable to register %d with the NCS !!", sensorType);
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
        CAMX_LOG_ERROR(CamxLogGroupAF, "Unable to Query caps sensor type %d error %s", sensorType,
            Utils::CamxResultToString(result));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::PublishPeerFocusInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::PublishPeerFocusInfo(
    VOID*  pPeerInfo)
{
    static const UINT WriteProps[] = { PropertyIDAFPeerInfo };
    const VOID*       pOutputData[CAMX_ARRAY_SIZE(WriteProps)] = { &pPeerInfo };
    UINT              pDataCount[CAMX_ARRAY_SIZE(WriteProps)] = { sizeof(VOID*) };
    return m_pNode->WriteDataList(WriteProps, pOutputData, pDataCount, CAMX_ARRAY_SIZE(WriteProps));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetPeerInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* CAFIOUtil::GetPeerInfo(
    const StatsProcessRequestData* pStatsProcessRequestDataInfo)
{
    static const UINT GetProps[]              = { PropertyIDAFPeerInfo };
    static const UINT GetPropsLength          = CAMX_ARRAY_SIZE(GetProps);
    VOID*             pData[GetPropsLength]   = { 0 };
    CamxResult        result                  = CamxResultEFailed;
    VOID*             pPeerInfo               = NULL;
    BOOL              needSync                = pStatsProcessRequestDataInfo->peerSyncInfo.needSync;
    UINT64            requestID               = pStatsProcessRequestDataInfo->requestId;
    INT64             requestDelta            = pStatsProcessRequestDataInfo->peerSyncInfo.requestDelta;
    UINT              peerPipelineId          = pStatsProcessRequestDataInfo->peerSyncInfo.peerPipelineID;

    if (TRUE == needSync)
    {
        BOOL     negates[GetPropsLength] = { (requestDelta < 0) ? TRUE : FALSE };
        UINT64   offsets[GetPropsLength] = { static_cast<UINT64>(abs(requestDelta)) };

        result = m_pNode->GetDataListFromPipeline(GetProps, pData, offsets, GetPropsLength, negates, peerPipelineId);
        if (NULL != pData[0])
        {
            CAMX_LOG_VERBOSE(CamxLogGroupAF, "Peer pPeerInfo %p peerPipelineId %d requestdelta %lld!",
                        *(static_cast<VOID**>(pData[0])),
                        peerPipelineId,
                        requestDelta);
            pPeerInfo = *(static_cast<VOID**>(pData[0]));
        }
    }
    else
    {
        pPeerInfo = NULL;
        CAMX_LOG_VERBOSE(CamxLogGroupAF, "No need to sync for Req:%llu on pipeline:%d",
                         requestID, m_pNode->GetPipelineId());

    }

    return pPeerInfo;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetOTPData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::GetOTPData(
    AFCalibrationOTPData*   pCalibrationAFOTPdata)
{
    if (NULL != pCalibrationAFOTPdata)
    {
        *pCalibrationAFOTPdata = m_AFCalibrationOTPData;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupAF, "AF Calibration OTP data not found");
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetAFLock
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CAFIOUtil::GetAFLock()
{
    return m_AFLock;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::GetAFReadProperties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::GetAFReadProperties()
{
    m_pStatsAFPropertyReadWrite->GetReadProperties(AFReadTypePropertyIDCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CAFIOUtil::GetHWPDConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CAFIOUtil::GetHWPDConfig(
    CHIPDLib*   pPDLib,
    PDHwConfig* pHWConfig)
{
    PDLibGetParams  getParams   = {};
    CDKResult       returnValue = CDKResultSuccess;
    CamxResult      result      = CamxResultSuccess;

    if (NULL != pPDLib && NULL != pPDLib->PDLibGetParam)
    {
        getParams.type = PDLibGetParamHWConfig;

        returnValue = pPDLib->PDLibGetParam(pPDLib, &getParams);

        if (CDKResultSuccess != returnValue)
        {
            CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: PD Library Get Param failed type %d", getParams.type);
            result = CamxResultEFailed;
        }
        else
        {
            *pHWConfig = getParams.outputData.config;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: Invalid Handle, Failed to Get PDHw config Library");
        result = CamxResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::SetAFPDAFInformation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::SetAFPDAFInformation(
    BOOL isPDAFEnabled)
{
    m_isPDAFEnabled = isPDAFEnabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::DumpAFStats
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::DumpAFStats()
{
    UINT32 outputMask = HwEnvironment::GetInstance()->GetStaticSettings()->dumpAF;
    if (TRUE == (outputMask & AFStatsDump))
    {
        CAMX_LOG_VERBOSE(
            CamxLogGroupAF, "[AF STATS] AF Config Mask: %d, BF Input Channel selection : %d, G Channel Selection config: %d",
            m_statsControl.statsConfig.mask,
            m_statsControl.statsConfig.BFStats.BFInputConfig.BFChannelSel,
            m_statsControl.statsConfig.BFStats.BFInputConfig.BFInputGSel);

        CAMX_LOG_VERBOSE(CamxLogGroupAF, "[AF STATS] BF Gamma number of entries : %d, BF Gamma Look up table, %f",
            m_statsControl.statsConfig.BFStats.BFGammaLUTConfig.numGammaLUT,
            m_statsControl.statsConfig.BFStats.BFGammaLUTConfig.gammaLUT);

        CAMX_LOG_VERBOSE(CamxLogGroupAF, "[AF STATS] BF Scale Config : M scalar value : %d, N scalar value: %d, "
            "pixel offset: %d, mnInit: %d, interpolation resolution: %d, phase init: %d, phase step: %d,"
            " input image width: %d ",
            m_statsControl.statsConfig.BFStats.BFScaleConfig.scaleM,
            m_statsControl.statsConfig.BFStats.BFScaleConfig.scaleN,
            m_statsControl.statsConfig.BFStats.BFScaleConfig.pixelOffset,
            m_statsControl.statsConfig.BFStats.BFScaleConfig.mnInit,
            m_statsControl.statsConfig.BFStats.BFScaleConfig.interpolationResolution,
            m_statsControl.statsConfig.BFStats.BFScaleConfig.phaseInit,
            m_statsControl.statsConfig.BFStats.BFScaleConfig.phaseStep,
            m_statsControl.statsConfig.BFStats.BFScaleConfig.inputImageWidth);

        for (UINT i = 0; i < BFFilterTypeCount; i++)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupAF, "[AF STATS] BF Filter Config: FIR Filter Coefficient: %d,"
                " Number of filter coefficients: %d",
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFFIRFilterConfig.FIRFilterCoefficients,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFFIRFilterConfig.numOfFIRCoefficients);

            CAMX_LOG_VERBOSE(CamxLogGroupAF, "[AF STATS] IIR Filter coefficient b10: %f, b11: %f, b12:"
                "%f, a11: %f, a12: %f, b20: %f, b21: %f, b22: %f, a21: %f, a22: %f, "
                " number of bits to shift: %d",
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.b10,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.b11,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.b12,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.a11,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.a12,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.b20,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.b21,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.b22,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.a21,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFIIRFilterConfig.a22,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].shiftBits);

            CAMX_LOG_VERBOSE(CamxLogGroupAF, "[AF STATS] BF filter coring config: filter threshold: %d, "
                "filter core: %d, filter gain: %d",
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFFilterCoringConfig.threshold,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFFilterCoringConfig.core,
                m_statsControl.statsConfig.BFStats.BFFilterConfig[i].BFFilterCoringConfig.gain);
        }
        UINT32 numBFStatsROIDimension = m_statsControl.statsConfig.BFStats.BFStatsROIConfig.numBFStatsROIDimension;
        for (UINT j = 0; j < numBFStatsROIDimension && j <  BFMaxROIRegions; j++)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupAF, "[AF STATS] BF ROI config: rectangle coordinates left: %d, "
                "top: %d, width:%d, height:%d, number of regions: %d, last primary region: %d",
                m_statsControl.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[j].ROI.left,
                m_statsControl.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[j].ROI.top,
                m_statsControl.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[j].ROI.width,
                m_statsControl.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[j].ROI.height,
                m_statsControl.statsConfig.BFStats.BFStatsROIConfig.BFStatsROIDimension[j].regionNum,
                m_statsControl.statsConfig.BFStats.BFStatsROIConfig.lastPrimaryRegion);
        }

    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::DumpFrameInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::DumpFrameInfo()
{
    UINT32 outputMask = HwEnvironment::GetInstance()->GetStaticSettings()->dumpAF;

    if (outputMask & AFFrameInfoDump)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF INFO] AF status focusDone: %d, status= %d, focusDistance(nearIndex: %f, optimalIndex: %f, farIndex: %f",
            m_AFFrameInfo.focusStatus.focusDone,
            m_AFFrameInfo.focusStatus.status,
            m_AFFrameInfo.focusStatus.focusDistance[FocusDistanceNearIndex],
            m_AFFrameInfo.focusStatus.focusDistance[FocusDistanceOptimalIndex],
            m_AFFrameInfo.focusStatus.focusDistance[FocusDistanceFarIndex]);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF INFO] AF value: %f, spotLightDetected: %d, crop magnification factor: %f, ROI: (L: %d T: %d, W: %d, H: %d)",
            m_AFFrameInfo.focusValue,
            m_AFFrameInfo.spotLightDetected,
            m_AFFrameInfo.cropMagnificationFactor,
            m_AFFrameInfo.ROICoordinate.left,
            m_AFFrameInfo.ROICoordinate.top,
            m_AFFrameInfo.ROICoordinate.width,
            m_AFFrameInfo.ROICoordinate.height);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF INFO] target lens position: %d, useDACValue: %d, numIntervals: %d, fovCompensationFactor: %f, "
            " isDepthFocus: %d",
            m_AFFrameInfo.moveLensOutput.targetLensPosition,
            m_AFFrameInfo.moveLensOutput.useDACValue,
            m_AFFrameInfo.moveLensOutput.numIntervals,
            m_AFFrameInfo.fovcOutput.fieldOfViewCompensationFactor,
            m_AFFrameInfo.isDepthFocus);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFIOUtil::DumpHWPDConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CAFIOUtil::DumpHWPDConfig(
    PDHwConfig* pPDHwConfig){

    UINT32 outputMask = HwEnvironment::GetInstance()->GetStaticSettings()->dumpAF;

    if (outputMask & AFHWPDConfigDump)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF HWPD] enablePDHw: %d, PDHW path select: %d, enableEarlyInterrupt: %d,"
            "earlyInterruptTransactionCount: %d, firstPixelSelect: %d",
            pPDHwConfig->enablePDHw,
            pPDHwConfig->pathSelect,
            pPDHwConfig->enableEarlyInterrupt,
            pPDHwConfig->earlyInterruptTransactionCount,
            pPDHwConfig->firstPixelSelect);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF HWPD] Dual PD crop info: enablePDCrop: %d, firstPixelCrop: %d, lastPixelCrop: %d,"
            " firstLineCrop: %d, lastLineCrop: %d ",
            pPDHwConfig->cropConfig.enablePDCrop,
            pPDHwConfig->cropConfig.firstPixelCrop,
            pPDHwConfig->cropConfig.lastPixelCrop,
            pPDHwConfig->cropConfig.firstLineCrop,
            pPDHwConfig->cropConfig.lastLineCrop);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF HWPD] Gain map config: gainMapEnable: %d, numberHorizontalGrids: %d, "
            "numberVerticalGrids: %d, horizontalPhaseInit: %d, verticalPhaseInit: %d, "
            "horizontalPhaseStep: %d, verticalPhaseStep: %d, ",
            pPDHwConfig->gainMapConfig.gainMapEnable,
            pPDHwConfig->gainMapConfig.numberHorizontalGrids,
            pPDHwConfig->gainMapConfig.numberVerticalGrids,
            pPDHwConfig->gainMapConfig.horizontalPhaseInit,
            pPDHwConfig->gainMapConfig.verticalPhaseInit,
            pPDHwConfig->gainMapConfig.horizontalPhaseStep,
            pPDHwConfig->gainMapConfig.verticalPhaseStep);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF HWPD] Bin config: enableSkipBinning: %d, verticalBinningLineCount: %d, "
            "verticalDecimateCount: %d, horizontalBinningPixelCount: %d, "
            "horizontalBinningSkip: %d, verticalBinningSkip: %d ",
            pPDHwConfig->binConfig.enableSkipBinning,
            pPDHwConfig->binConfig.verticalBinningLineCount,
            pPDHwConfig->binConfig.verticalDecimateCount,
            pPDHwConfig->binConfig.horizontalBinningPixelCount,
            pPDHwConfig->binConfig.horizontalBinningSkip,
            pPDHwConfig->binConfig.verticalBinningSkip);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF HWPD] BLS config: leftPixelBLSCorrection: %d, rightPixelBLSCorrection: %d",
            pPDHwConfig->BLSConfig.leftPixelBLSCorrection,
            pPDHwConfig->BLSConfig.rightPixelBLSCorrection);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF HWPD] HDR config: hdrEnable: %d, hdrFirstPixel: %d, hdrModeType: %d, hdrThreshold: %f, hdrExposureRatio: %f",
            pPDHwConfig->HDRConfig.hdrEnable,
            pPDHwConfig->HDRConfig.hdrFirstPixel,
            pPDHwConfig->HDRConfig.hdrModeSel,
            pPDHwConfig->HDRConfig.hdrThreshhold,
            pPDHwConfig->HDRConfig.hdrExposureRatio);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF HWPD] IIR config: IIREnable: %d, init0: %f, init1: %f, a0: %f, a1: %f, b0: %f, b1: %f, b2: %f",
            pPDHwConfig->IIRConfig.IIREnable,
            pPDHwConfig->IIRConfig.init0,
            pPDHwConfig->IIRConfig.init1,
            pPDHwConfig->IIRConfig.a0,
            pPDHwConfig->IIRConfig.a1,
            pPDHwConfig->IIRConfig.b0,
            pPDHwConfig->IIRConfig.b1,
            pPDHwConfig->IIRConfig.b2);

        CAMX_LOG_VERBOSE(CamxLogGroupAF,
            "[AF HWPD] SAD config: sadEnable: %d, outputSelect: %d, horizontalOffset: %d, "
            "verticalOffset: %d, regionWidth: %d, regionHeight: %d, horizontalNumber: %d, "
            "verticalNumber: %d, sadShift: %d, config0Phase: %d, config1Phase: %d",
            pPDHwConfig->SADConfig.sadEnable,
            pPDHwConfig->SADConfig.sadSelect,
            pPDHwConfig->SADConfig.horizontalOffset,
            pPDHwConfig->SADConfig.verticalOffset,
            pPDHwConfig->SADConfig.regionWidth,
            pPDHwConfig->SADConfig.regionHeight,
            pPDHwConfig->SADConfig.horizontalNumber,
            pPDHwConfig->SADConfig.verticalNumber,
            pPDHwConfig->SADConfig.sadShift,
            pPDHwConfig->SADConfig.config0Phase,
            pPDHwConfig->SADConfig.config1Phase);
    }
}

CAMX_NAMESPACE_END
