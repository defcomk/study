////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file camxpdafdata.cpp
/// @brief Implements PDAFData methods.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxpdafconfig.h"
#include "camxpropertyblob.h"
#include "camxmetadatapool.h"
#include "camxactuatordata.h"
#include "camximagesensormoduledata.h"
#include "camximagesensordata.h"
#include "chipdlibinterface.h"
#include "camxpdafdata.h"
#include "camxnode.h"
#include "camxtitan17xcontext.h"


CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::PDAFData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PDAFData::PDAFData(
    PDAFConfigurationData* pPDAFConfigData)
{
    if (NULL != pPDAFConfigData)
    {
        m_pPDAFConfigData = pPDAFConfigData;
        m_isPdafEnabled   = TRUE;
    }
    else
    {
        m_pPDAFConfigData = NULL;
        m_isPdafEnabled   = FALSE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::PDAFInit
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::PDAFInit(
    CHIPDLib*              pPDLib,
    UINT32                 resIdx,
    ActuatorData*          pActuatorData,
    ImageSensorData*       pSensorData,
    const EEPROMOTPData*   pOTPData,
    StatsCameraInfo*       pCameraInfo,
    VOID*                  pTuningManager,
    VOID*                  pCAMIFT3DataPattern,
    HwContext*             pHwContext)
{
    CamxResult           result         = CamxResultSuccess;
    PDLibDataBufferInfo* pT3DataPattern = NULL;

    if (FALSE == m_isPdafEnabled)
    {
        // No need to log an error when PDAF data is not present
        // This indicates that PDAF is not supported in the sensor or is disabled
        CAMX_LOG_INFO(CamxLogGroupSensor, "PDAF Disabled");
        result = CamxResultEFailed;
    }

    if ((NULL != pPDLib)      && (NULL != pActuatorData) &&
        (NULL != pSensorData) && (NULL != pOTPData)      &&
        (NULL != m_pPDAFConfigData))
    {
        PDLibCreateParams PDLibInitParams = {};

        PDLibInitParams.initParam.cameraInfo               = *pCameraInfo;
        PDLibInitParams.tuningStats                        = { 0 };
        PDLibInitParams.tuningStats.pTuningSetManager      = pTuningManager;

        // Initializing Tuning Mode Parameter for Sensor
        PDLibInitParams.tuningStats.numSelectors = MaxTuningMode;

        ChiTuningModeParameter chiTuningModeParameter = { 0 };

        // Setting default values for tuning file struture
        chiTuningModeParameter.TuningMode[0].mode = ChiModeType::Default;
        chiTuningModeParameter.TuningMode[1].mode = ChiModeType::Sensor;
        chiTuningModeParameter.TuningMode[2].mode = ChiModeType::Usecase;
        chiTuningModeParameter.TuningMode[3].mode = ChiModeType::Feature1;
        chiTuningModeParameter.TuningMode[4].mode = ChiModeType::Feature2;
        chiTuningModeParameter.TuningMode[5].mode = ChiModeType::Scene;
        chiTuningModeParameter.TuningMode[6].mode = ChiModeType::Effect;

        // Setting Sensor Mode
        chiTuningModeParameter.TuningMode[1].subMode.value = static_cast<UINT16>(resIdx);


        PDLibInitParams.tuningStats.pTuningModeSelectors = reinterpret_cast<TuningMode*>(&chiTuningModeParameter.TuningMode[0]);

        result = GetPDAFOTPData(&PDLibInitParams, pOTPData);

        if (CamxResultSuccess == result)
        {
            result = GetPDAFActuatorData(pActuatorData, &PDLibInitParams);
        }

        if (CamxResultSuccess == result)
        {
            result = GetPDAFSensorData(pSensorData, &PDLibInitParams, resIdx);
        }

        if ((CamxResultSuccess == result) &&
            (TRUE == m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockPatternExists))
        {
            result = FillPDAFPixelCoordinates(m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockPattern,
                                              &PDLibInitParams.initParam.nativePatternInfo.patternInfo.blockPattern);
            if (CamxResultSuccess == result)
            {
                result = AdjustNativePDAFOffsets(&PDLibInitParams.initParam.nativePatternInfo.patternInfo.blockPattern);
            }
        }

        if (CamxResultSuccess == result)
        {

            PDLibInitParams.initParam.nativePatternInfo.horizontalDownscaleFactor =
                m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDDownscaleFactorHorizontal;

            PDLibInitParams.initParam.nativePatternInfo.verticalDownscaleFactor =
                m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDDownscaleFactorVertical;

            PDLibInitParams.initParam.pixelOrderType =
                static_cast<PDLibPixelOrderType>(m_pPDAFConfigData->PDInfo.PDPixelOrderType);

            PDLibInitParams.initParam.blackLevel =
                m_pPDAFConfigData->PDInfo.PDBlackLevel;
            PDLibInitParams.initParam.defocusBitShift =
                m_pPDAFConfigData->PDInfo.PDDefocusBitShift;
            PDLibInitParams.initParam.defocusConfidenceThreshold =
                m_pPDAFConfigData->PDInfo.PDDefocusConfidenceThreshold;
            PDLibInitParams.initParam.sensorPDStatsFormat =
                static_cast<PDLibSensorPDStatsFormat>(m_pPDAFConfigData->PDInfo.PDSensorPDStatsFormat);

            PDLibInitParams.initParam.nativePatternInfo.PDOffsetCorrection =
                m_pPDAFConfigData->PDInfo.PDOffsetCorrection;
            PDLibInitParams.initParam.nativePatternInfo.lcrPDOffset =
                m_pPDAFConfigData->PDInfo.lcrPDOffsetCorrection;

            if (TRUE == m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockPatternExists)
            {
                Utils::Memcpy(&PDLibInitParams.initParam.nativePatternInfo.patternInfo.blockPattern.blockDimension,
                    &m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockPattern.PDBlockDimensions,
                    (sizeof(PDLibDimensionInfo)));
            }

            PDLibInitParams.initParam.nativePatternInfo.patternInfo.horizontalBlockCount =
                m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockCountHorizontal;

            PDLibInitParams.initParam.nativePatternInfo.patternInfo.verticalBlockCount =
                m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockCountVertical;

            PDLibInitParams.initParam.nativePatternInfo.orientation =
                static_cast<PDLibSensorOrientation>(m_pPDAFConfigData->PDInfo.PDOrientation);

            switch(m_pPDAFConfigData->PDInfo.PDType)
            {
                case PDAFType::PDType3:
                    pT3DataPattern = reinterpret_cast<PDLibDataBufferInfo*>(pCAMIFT3DataPattern);
                    if (NULL == pT3DataPattern)
                    {
                        CAMX_LOG_WARN(CamxLogGroupSensor, "PDAF Error: pT3DataPattern is NULL");
                        result = CamxResultEFailed;
                        break;
                    }

                    result = GetPDAFSensorType(m_pPDAFConfigData->PDInfo.PDType,
                        static_cast<VOID*>(&PDLibInitParams.initParam.bufferDataInfo.sensorType));

                    if (CamxResultSuccess == result)
                    {
                        PDLibInitParams.initParam.bufferDataInfo.imageOverlap = pT3DataPattern->imageOverlap;
                        PDLibInitParams.initParam.bufferDataInfo.bufferFormat = pT3DataPattern->bufferFormat;
                        PDLibInitParams.initParam.bufferDataInfo.ispBufferHeight = pT3DataPattern->ispBufferHeight;
                        PDLibInitParams.initParam.bufferDataInfo.isp1BufferStride = pT3DataPattern->isp1BufferStride;
                        PDLibInitParams.initParam.bufferDataInfo.isp1BufferWidth = pT3DataPattern->isp1BufferWidth;
                        PDLibInitParams.initParam.bufferDataInfo.isp2BufferStride = pT3DataPattern->isp2BufferStride;
                        PDLibInitParams.initParam.bufferDataInfo.isp2BufferWidth = pT3DataPattern->isp2BufferWidth;

                        PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern.verticalPDOffset =
                            pT3DataPattern->isp1BlockPattern.verticalPDOffset;
                        PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern.horizontalPDOffset =
                            pT3DataPattern->isp1BlockPattern.horizontalPDOffset;
                        PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern.pixelCount =
                            pT3DataPattern->isp1BlockPattern.pixelCount;
                        PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern.blockDimension.height =
                            pT3DataPattern->isp1BlockPattern.blockDimension.height;
                        PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern.blockDimension.width =
                            pT3DataPattern->isp1BlockPattern.blockDimension.width;

                        for (UINT index = 0; index < pT3DataPattern->isp1BlockPattern.pixelCount; index++)
                        {
                            PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern.pixelCoordinate[index].x =
                                pT3DataPattern->isp1BlockPattern.pixelCoordinate[index].x;
                            PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern.pixelCoordinate[index].y =
                                pT3DataPattern->isp1BlockPattern.pixelCoordinate[index].y;

                            PDAFPixelShieldInformation PDPixelType =
                                m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].
                                PDBlockPattern.PDPixelCoordinates[index].PDPixelShieldInformation;

                            result = GetPDAFPixelType(
                                PDPixelType,
                                &PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern.pixelCoordinate[index].type);
                            if (CamxResultEFailed == result)
                            {
                                CAMX_LOG_WARN(CamxLogGroupSensor, "PDAF Error: GetPDAFPixelType failed for Type3 Data");
                                break;
                            }
                        }

                        if (CamxResultSuccess == result)
                        {
                            PDLibInitParams.initParam.bufferDataInfo.isp2BlockPattern.verticalPDOffset =
                                pT3DataPattern->isp2BlockPattern.verticalPDOffset;
                            PDLibInitParams.initParam.bufferDataInfo.isp2BlockPattern.horizontalPDOffset =
                                pT3DataPattern->isp2BlockPattern.horizontalPDOffset;
                            PDLibInitParams.initParam.bufferDataInfo.isp2BlockPattern.pixelCount =
                                pT3DataPattern->isp2BlockPattern.pixelCount;
                            PDLibInitParams.initParam.bufferDataInfo.isp2BlockPattern.blockDimension.height =
                                pT3DataPattern->isp2BlockPattern.blockDimension.height;
                            PDLibInitParams.initParam.bufferDataInfo.isp2BlockPattern.blockDimension.width =
                                pT3DataPattern->isp2BlockPattern.blockDimension.width;

                            for (UINT index = 0; index < pT3DataPattern->isp2BlockPattern.pixelCount; index++)
                            {
                                PDLibInitParams.initParam.bufferDataInfo.isp2BlockPattern.pixelCoordinate[index].x =
                                    pT3DataPattern->isp2BlockPattern.pixelCoordinate[index].x;
                                PDLibInitParams.initParam.bufferDataInfo.isp2BlockPattern.pixelCoordinate[index].y =
                                    pT3DataPattern->isp2BlockPattern.pixelCoordinate[index].y;

                                if (TRUE == m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockPatternExists)
                                {
                                    PDAFPixelShieldInformation PDPixelType =
                                        m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].
                                        PDBlockPattern.PDPixelCoordinates[index].PDPixelShieldInformation;
                                    result = GetPDAFPixelType(PDPixelType,
                                                              &PDLibInitParams.initParam.bufferDataInfo.isp2BlockPattern.
                                                                pixelCoordinate[index].type);
                                    if (CamxResultEFailed == result)
                                    {
                                        CAMX_LOG_WARN(CamxLogGroupSensor, "PDAF Error: GetPDAFPixelType failed for Type3");
                                        break;
                                    }
                                }
                                else
                                {
                                    CAMX_LOG_WARN(CamxLogGroupSensor,
                                                   "PDAF Error: PDBlockPattern not available for Type3");
                                    result = CamxResultEFailed;
                                    break;
                                }
                            }
                        }
                    }
                    break;
                case PDAFType::PDType2:
                    // Type 2 Buffer Pattern Information

                    result = GetPDAFSensorType(m_pPDAFConfigData->PDInfo.PDType,
                        static_cast<VOID*>(&PDLibInitParams.initParam.bufferDataInfo.sensorType));

                    if (CamxResultSuccess == result)
                    {
                        result = GetPDAFBufferDataFormat(
                                m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDNativeBufferFormat,
                                &PDLibInitParams.initParam.nativePatternInfo.bufferFormat);
                    }

                    if ((CamxResultSuccess == result) &&
                        (TRUE == m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDBlockPatternExists))
                    {
                        result = FillPDAFPixelCoordinates(
                                 m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDBlockPattern,
                                 &PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern);
                    }

                    if (CamxResultSuccess == result)
                    {
                        result = GetPDAFBufferDataFormat(
                                m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDBufferFormat,
                                &PDLibInitParams.initParam.bufferDataInfo.bufferFormat);
                    }

                    if (CamxResultSuccess == result)
                    {
                        PDLibInitParams.initParam.bufferDataInfo.isp1BufferStride =
                            m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDStride;

                        if (TRUE == m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDBlockPatternExists)
                        {
                            PDLibInitParams.initParam.bufferDataInfo.ispBufferHeight =
                                m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockCountVertical *
                                m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDBlockPattern.PDBlockDimensions.height;

                            PDLibInitParams.initParam.bufferDataInfo.isp1BufferWidth =
                                m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockCountHorizontal *
                                m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDBlockPattern.PDBlockDimensions.width;

                            Utils::Memcpy(&PDLibInitParams.initParam.bufferDataInfo.isp1BlockPattern.blockDimension,
                                        &m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDBlockPattern.PDBlockDimensions,
                                        (sizeof(PDLibDimensionInfo)));
                        }
                        else
                        {
                            CAMX_LOG_ERROR(CamxLogGroupSensor,
                                           "PDAF Error: PDBlockPattern not available for Type2");
                            result = CamxResultEFailed;
                            break;
                        }

                        // Image overlap only valid for Type3 where there is a possibility of two ISPs
                        PDLibInitParams.initParam.bufferDataInfo.imageOverlap = 0;
                    }
                    break;
                case PDAFType::PDType2PD:
                    // 2PD Buffer Pattern Information
                    result = GetPDAFSensorType(m_pPDAFConfigData->PDInfo.PDType,
                        static_cast<VOID*>(&PDLibInitParams.initParam.bufferDataInfo.sensorType));

                    if (CamxResultSuccess == result)
                    {
                        result = GetPDAFBufferDataFormat(
                                m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDNativeBufferFormat,
                                &PDLibInitParams.initParam.nativePatternInfo.bufferFormat);
                    }

                    if (CamxResultSuccess == result)
                    {
                        result = GetPDAFBufferDataFormat(
                                m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDBufferFormat,
                                &PDLibInitParams.initParam.bufferDataInfo.bufferFormat);
                    }

                    if (CamxResultSuccess == result)
                    {
                        PDLibInitParams.initParam.bufferDataInfo.isp1BufferStride =
                            m_pPDAFConfigData->PDBufferBlockPatternInfo[resIdx].PDStride;

                        PDLibInitParams.initParam.bufferDataInfo.ispBufferHeight =
                            m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockCountVertical;

                        PDLibInitParams.initParam.bufferDataInfo.isp1BufferWidth =
                            m_pPDAFConfigData->PDSensorNativePatternInfo[resIdx].PDBlockCountHorizontal;
                    }
                    break;
                case PDAFType::PDType1:
                    result = CamxResultSuccess;
                    break;
                default:
                    CAMX_LOG_WARN(CamxLogGroupSensor, "Invalid PDAF Type %d", m_pPDAFConfigData->PDInfo.PDType);
                    result = CamxResultEFailed;
                    break;
            }

            BOOL isHWSupportPDHW =
                (((static_cast<Titan17xContext *>(pHwContext)->GetTitanVersion()) == CSLCameraTitanVersion::CSLTitan175) ||
                 ((static_cast<Titan17xContext *>(pHwContext)->GetTitanVersion()) == CSLCameraTitanVersion::CSLTitan160));
            const StaticSettings*   pSettings = pHwContext->GetStaticSettings();

            // PD settings
            PDLibInitParams.initParam.settingsInfo.isPDHWEnabled          = isHWSupportPDHW &&
                                                                            (pSettings->pdafHWEnableMask == 0x2);
            PDLibInitParams.initParam.settingsInfo.enablePDLibLog         = pSettings->enablePDLibLog;
            PDLibInitParams.initParam.settingsInfo.enablePDLibProfiling   = pSettings->enablePDLibProfiling;
            PDLibInitParams.initParam.settingsInfo.enablePDLibTestMode    = pSettings->enablePDLibTestMode;
            PDLibInitParams.initParam.settingsInfo.disableLCR             = pSettings->disablePDLibLCR;
            PDLibInitParams.initParam.settingsInfo.enablePDLibDump        = pSettings->enablePDLibDump;

            if (CamxResultSuccess == result)
            {
                CDKResult returnValue = CDKResultSuccess;

                PrintDebugPDAFData(PDLibInitParams);
                CAMX_LOG_INFO(CamxLogGroupSensor, "Initializing PDAF %s", m_pPDAFConfigData->PDInfo.PDAFName);
                returnValue = pPDLib->PDLibInitialize(pPDLib, &PDLibInitParams);
                if (CDKResultSuccess != returnValue)
                {
                    CAMX_LOG_WARN(CamxLogGroupSensor, "PDAF Error: PD Library Initialization failed");
                    result = CamxResultEFailed;
                }
            }
        }
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupSensor, "PDAF Error: Invalid Handle, Failed to initialize PD Library");
        result = CamxResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::GetInitialHWPDConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::GetInitialHWPDConfig(
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
/// PDAFData::GetPDAFOTPData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::GetPDAFOTPData(
    PDLibCreateParams*      pPDLibInitParams,
    const EEPROMOTPData*    pOTPData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pPDLibInitParams)
    {
        if (NULL != pOTPData)
        {
            pPDLibInitParams->initParam.infinityDac = pOTPData->AFCalibration.infinityDAC;
            pPDLibInitParams->initParam.macroDac = pOTPData->AFCalibration.macroDAC;

            switch (m_pPDAFConfigData->PDInfo.PDType)
            {
                case PDAFType::PDType1:
                    {
                        m_calibrationDataType1 = pOTPData->PDAFDCCCalibration;
                        pPDLibInitParams->initParam.pCalibrationParam = static_cast<void *>(&m_calibrationDataType1);
                        break;
                    }
                case PDAFType::PDType2PD:
                case PDAFType::PDType2:
                case PDAFType::PDType3:
                default:
                    {
                        m_calibrationDataType2 = pOTPData->PDAF2DCalibration;
                        pPDLibInitParams->initParam.pCalibrationParam = static_cast<void *>(&m_calibrationDataType2);
                        break;
                    }
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: OTP data not present");
            result = CamxResultEFailed;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: Invalid pointer");
        result = CamxResultEFailed;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::GetPDAFActuatorData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::GetPDAFActuatorData(
    ActuatorData*      pActuatorData,
    PDLibCreateParams* pPDLibInitParams)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pActuatorData) && (NULL != pPDLibInitParams))
    {
        pPDLibInitParams->initParam.actuatorSensitivity =
            pActuatorData->GetSensitivity();
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: Actuator data not present");
        result = CamxResultEFailed;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::GetPDAFSensorData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::GetPDAFSensorData(
    ImageSensorData*   pSensorData,
    PDLibCreateParams* pPDLibInitParams,
    UINT32             resIdx)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pSensorData) && (NULL != pPDLibInitParams))
    {
        const ResolutionInformation* pResolutionInfo  = pSensorData->GetResolutionInfo();
        const PixelArrayInfo*        pActiveArrayInfo = pSensorData->GetActiveArraySize();

        pPDLibInitParams->initParam.isHdrModeEnabled =
            IsHDREnabled(&pResolutionInfo->resolutionData[resIdx]);

        pPDLibInitParams->initParam.nativePatternInfo.bayerPattern =
            static_cast<PDLibSensorBayerPattern>(pResolutionInfo->resolutionData[resIdx].colorFilterArrangement);

        pPDLibInitParams->initParam.nativePatternInfo.currentImageWidth=
            pResolutionInfo->resolutionData[resIdx].streamInfo.streamConfiguration[0].frameDimension.width;

        pPDLibInitParams->initParam.nativePatternInfo.currentImageHeight =
            pResolutionInfo->resolutionData[resIdx].streamInfo.streamConfiguration[0].frameDimension.height;

        pPDLibInitParams->initParam.pixelDepth =
            pResolutionInfo->resolutionData[resIdx].streamInfo.streamConfiguration[0].bitWidth;

        pPDLibInitParams->initParam.nativePatternInfo.cropRegion.x =
            pResolutionInfo->resolutionData[resIdx].streamInfo.streamConfiguration[0].frameDimension.xStart;

        pPDLibInitParams->initParam.nativePatternInfo.cropRegion.y =
            pResolutionInfo->resolutionData[resIdx].streamInfo.streamConfiguration[0].frameDimension.yStart;

        pPDLibInitParams->initParam.nativePatternInfo.cropRegion.width =
            pResolutionInfo->resolutionData[resIdx].streamInfo.streamConfiguration[0].frameDimension.width;

        pPDLibInitParams->initParam.nativePatternInfo.cropRegion.height =
            pResolutionInfo->resolutionData[resIdx].streamInfo.streamConfiguration[0].frameDimension.height;

        pPDLibInitParams->initParam.nativePatternInfo.originalImageHeight =
            pActiveArrayInfo->activeDimension.height;

        pPDLibInitParams->initParam.nativePatternInfo.originalImageWidth=
            pActiveArrayInfo->activeDimension.width;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: Sensor data not present");
        result = CamxResultEFailed;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::IsHDREnabled
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT PDAFData::IsHDREnabled(
    ResolutionData* pResolutionData)
{
    UINT   isHDREnabled    = 0;
    UINT32 capabilityCount = 0;

    CAMX_ASSERT(NULL != pResolutionData);
    for (capabilityCount = 0; capabilityCount <= pResolutionData->capabilityCount; capabilityCount++)
    {
        if ((pResolutionData->capability[capabilityCount] == SensorCapability::IHDR) ||
            (pResolutionData->capability[capabilityCount] == SensorCapability::ZZHDR))
        {
            isHDREnabled = 1;
        }
    }
    return isHDREnabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::FillPDAFPixelCoordinates
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::FillPDAFPixelCoordinates(
    PDAFBlockPattern PDBlockPattern,
    VOID*            pData)
{
    CamxResult         result          = CamxResultSuccess;
    UINT32             i               = 0;
    PDLibBlockPattern* pPDLibPixelInfo = static_cast<PDLibBlockPattern*>(pData);

    CAMX_ASSERT(NULL != pData);

    pPDLibPixelInfo->horizontalPDOffset = PDBlockPattern.PDOffsetHorizontal;
    pPDLibPixelInfo->verticalPDOffset   = PDBlockPattern.PDOffsetVertical;
    pPDLibPixelInfo->pixelCount         = PDBlockPattern.PDPixelCount;

    for (i = 0; i < pPDLibPixelInfo->pixelCount; i++)
    {
        pPDLibPixelInfo->pixelCoordinate[i].x = PDBlockPattern.PDPixelCoordinates[i].PDXCoordinate;

        pPDLibPixelInfo->pixelCoordinate[i].y = PDBlockPattern.PDPixelCoordinates[i].PDYCoordinate;

        result = GetPDAFPixelType(PDBlockPattern.PDPixelCoordinates[i].PDPixelShieldInformation,
            &pPDLibPixelInfo->pixelCoordinate[i].type);
        if (CamxResultEFailed == result)
        {
            CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: FillPDAFPixelCoordinates failed");
            break;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::AdjustNativePDAFOffsets
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::AdjustNativePDAFOffsets(
    VOID*            pData)
{
    CamxResult         result          = CamxResultSuccess;
    UINT               i               = 0;
    PDLibBlockPattern* pPDLibPixelInfo = static_cast<PDLibBlockPattern*>(pData);

    if (NULL != pPDLibPixelInfo)
    {
        for (i = 0; i < pPDLibPixelInfo->pixelCount; i++)
        {
            pPDLibPixelInfo->pixelCoordinate[i].x =
                pPDLibPixelInfo->pixelCoordinate[i].x - pPDLibPixelInfo->horizontalPDOffset;

            pPDLibPixelInfo->pixelCoordinate[i].y =
                pPDLibPixelInfo->pixelCoordinate[i].y - pPDLibPixelInfo->verticalPDOffset;
        }
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: Could not adjust the PD native offset");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::GetPDAFSensorType
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::GetPDAFSensorType(
    PDAFType PDType,
    VOID*    pData)
{
    CamxResult      result      = CamxResultSuccess;
    PDLibSensorType sensorType;

    CAMX_ASSERT(NULL != pData);

    switch (PDType)
    {
        case PDAFType::PDType1:
            {
                sensorType = PDLibSensorType::PDLibSensorType1;
                break;
            }
        case PDAFType::PDType2:
            {
                sensorType = PDLibSensorType::PDLibSensorType2;
                break;
            }
        case PDAFType::PDType3:
            {
                sensorType = PDLibSensorType::PDLibSensorType3;
                break;
            }
        case PDAFType::PDType2PD:
            {
                sensorType = PDLibSensorType::PDLibSensorDualPD;
                break;
            }
        case PDAFType::PDTypeUnknown:
        default:
            {
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: Invalid PDAF Type");
                sensorType = PDLibSensorType::PDLibSensorInvalid;
                break;
            }
    }

    Utils::Memcpy(pData, &sensorType, sizeof(PDLibSensorType));
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::GetPDAFPixelType
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::GetPDAFPixelType(
    PDAFPixelShieldInformation PDPixelType,
    VOID*                      pData)
{
    CamxResult           result = CamxResultSuccess;
    PDLibPixelShieldInfo pixelType;

    CAMX_ASSERT(NULL != pData);

    switch (PDPixelType)
    {
        case PDAFPixelShieldInformation::LEFTDIODE:
            {
                pixelType = PDLibPixelShieldInfo::PDLibLeftDiodePixel;
                break;
            }
        case PDAFPixelShieldInformation::RIGHTDIODE:
            {
                pixelType = PDLibPixelShieldInfo::PDLibRightDiodePixel;
                break;
            }
        case PDAFPixelShieldInformation::LEFTSHIELDED:
            {
                pixelType = PDLibPixelShieldInfo::PDLibLeftShieldedPixel;
                break;
            }
        case PDAFPixelShieldInformation::RIGHTSHIELDED:
            {
                pixelType = PDLibPixelShieldInfo::PDLibRightShieldedPixel;
                break;
            }
        default:
            {
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: Invalid PDAF Pixel Shield Type");
                pixelType = PDLibPixelShieldInfo::PDLibRightShieldedPixel;
                break;
            }
    }
    Utils::Memcpy(pData, &pixelType, sizeof(PDLibPixelShieldInfo));
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::GetPDAFBufferDataFormat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult PDAFData::GetPDAFBufferDataFormat(
    PDAFBufferDataFormat PDBufferDataFormat,
    VOID*                pData)
{
    CamxResult        result = CamxResultSuccess;
    PDLibBufferFormat PDBufferFormat;
    CAMX_ASSERT(NULL != pData);

    switch (PDBufferDataFormat)
    {
        case PDAFBufferDataFormat::MIPI8:
            {
                PDBufferFormat = PDLibBufferFormat::PDLibBufferFormatMipi8;
                break;
            }
        case PDAFBufferDataFormat::MIPI10:
            {
                PDBufferFormat = PDLibBufferFormat::PDLibBufferFormatMipi10;
                break;
            }
        case PDAFBufferDataFormat::PACKED10:
            {
                PDBufferFormat = PDLibBufferFormat::PDLibBufferFormatPacked10;
                break;
            }
        case PDAFBufferDataFormat::UNPACKED16:
            {
                PDBufferFormat = PDLibBufferFormat::PDLibBufferFormatUnpacked16;
                break;
            }
        default:
            {
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupSensor, "PDAF Error: Invalid PDAF Buffer Data Format");
                PDBufferFormat = PDLibBufferFormat::PDLibBufferFormatUnpacked16;
                break;
            }
    }
    Utils::Memcpy(pData, &PDBufferFormat, sizeof(PDLibBufferFormat));
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::PrintDebugPDAFData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID PDAFData::PrintDebugPDAFData(
    PDLibCreateParams  PDAFData)
{
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "PDAF Debug Data for %s", m_pPDAFConfigData->PDInfo.PDAFName);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\t=====PDAF Information:=====");
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tActuator Sensitivity: %f",
        PDAFData.initParam.actuatorSensitivity);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tBlack Level: %d",
        PDAFData.initParam.blackLevel);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tPixel Depth: %d",
        PDAFData.initParam.pixelDepth);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tIs Hdr Mode Enabled: %d",
        PDAFData.initParam.isHdrModeEnabled);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tPDPixelOverflowThreshold: %d",
        PDAFData.initParam.PDPixelOverflowThreshold);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tMacro DAC: %d",
        PDAFData.initParam.macroDac);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tInfinity DAC: %d",
        PDAFData.initParam.infinityDac);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tPixel order Type: %d",
        PDAFData.initParam.pixelOrderType);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\t=====Native Pattern Information:=====");
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\toriginalImageWidth: %d",
        PDAFData.initParam.nativePatternInfo.originalImageWidth);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\toriginalImageHeight: %d",
        PDAFData.initParam.nativePatternInfo.originalImageHeight);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tcurrentImageWidth: %d",
        PDAFData.initParam.nativePatternInfo.currentImageWidth);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tcurrentImageHeight: %d",
        PDAFData.initParam.nativePatternInfo.currentImageHeight);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\thorizontalDownscaleFactor: %f",
        PDAFData.initParam.nativePatternInfo.horizontalDownscaleFactor);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tverticalDownscaleFactor: %f",
        PDAFData.initParam.nativePatternInfo.verticalDownscaleFactor);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tPDOffsetCorrection: %f",
        PDAFData.initParam.nativePatternInfo.PDOffsetCorrection);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tlcrOffset: %f",
        PDAFData.initParam.nativePatternInfo.lcrPDOffset);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tcropRegion.height: %d",
        PDAFData.initParam.nativePatternInfo.cropRegion.height);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tcropRegion.width: %d",
        PDAFData.initParam.nativePatternInfo.cropRegion.width);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tcropRegion.x: %d",
        PDAFData.initParam.nativePatternInfo.cropRegion.x);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tcropRegion.y: %d",
        PDAFData.initParam.nativePatternInfo.cropRegion.y);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tbufferFormat: %d",
        PDAFData.initParam.nativePatternInfo.bufferFormat);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\torientation: %d",
        PDAFData.initParam.nativePatternInfo.orientation);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tpatternInfo.horizontalBlockCount: %d",
        PDAFData.initParam.nativePatternInfo.patternInfo.horizontalBlockCount);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tpatternInfo.verticalBlockCount: %d",
        PDAFData.initParam.nativePatternInfo.patternInfo.verticalBlockCount);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tpatternInfo.blockPattern.horizontalPDOffset: %d",
        PDAFData.initParam.nativePatternInfo.patternInfo.blockPattern.horizontalPDOffset);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tpatternInfo.blockPattern.verticalPDOffset: %d",
        PDAFData.initParam.nativePatternInfo.patternInfo.blockPattern.verticalPDOffset);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tpatternInfo.blockPattern.blockDimension.height: %d",
        PDAFData.initParam.nativePatternInfo.patternInfo.blockPattern.blockDimension.height);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tpatternInfo.blockPattern.blockDimension.width: %d",
        PDAFData.initParam.nativePatternInfo.patternInfo.blockPattern.blockDimension.width);
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tpatternInfo.blockPattern.pixelCount: %d",
        PDAFData.initParam.nativePatternInfo.patternInfo.blockPattern.pixelCount);

    for (UINT i = 0; i < PDAFData.initParam.nativePatternInfo.patternInfo.blockPattern.pixelCount; i++)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupSensor,
            "\tpatternInfo.blockPattern.pixelCount[%d].x = %d",
            i,
            PDAFData.initParam.nativePatternInfo.patternInfo.blockPattern.pixelCoordinate[i].x);

        CAMX_LOG_VERBOSE(CamxLogGroupSensor,
            "\tpatternInfo.blockPattern.pixelCount[%d].y = %d",
            i,
            PDAFData.initParam.nativePatternInfo.patternInfo.blockPattern.pixelCoordinate[i].y);

        CAMX_LOG_VERBOSE(CamxLogGroupSensor,
            "\tpatternInfo.blockPattern.pixelCount[%d].type = %d",
            i,
            PDAFData.initParam.nativePatternInfo.patternInfo.blockPattern.pixelCoordinate[i].type);
    }

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\t=====PDAF Buffer data pattern=====");
    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tBuffer format: %d",
        PDAFData.initParam.bufferDataInfo.bufferFormat);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\timageOverlap: %d",
        PDAFData.initParam.bufferDataInfo.imageOverlap);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tsensorType: %d",
        PDAFData.initParam.bufferDataInfo.sensorType);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tispBufferHeight: %d",
        PDAFData.initParam.bufferDataInfo.ispBufferHeight);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tisp1BufferWidth: %d",
        PDAFData.initParam.bufferDataInfo.isp1BufferWidth);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tisp1BufferStride: %d",
        PDAFData.initParam.bufferDataInfo.isp1BufferStride);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tisp1BlockPattern.verticalPDOffset: %d",
        PDAFData.initParam.bufferDataInfo.isp1BlockPattern.verticalPDOffset);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tisp1BlockPattern.horizontalPDOffset: %d",
        PDAFData.initParam.bufferDataInfo.isp1BlockPattern.horizontalPDOffset);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tisp1BlockPattern.blockDimension.height: %d",
        PDAFData.initParam.bufferDataInfo.isp1BlockPattern.blockDimension.height);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tisp1BlockPattern.blockDimension.width: %d",
        PDAFData.initParam.bufferDataInfo.isp1BlockPattern.blockDimension.width);

    CAMX_LOG_VERBOSE(CamxLogGroupSensor, "\tisp1BlockPattern.pixelCount: %d",
        PDAFData.initParam.bufferDataInfo.isp1BlockPattern.pixelCount);

    for (UINT i = 0; i < PDAFData.initParam.bufferDataInfo.isp1BlockPattern.pixelCount; i++)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupSensor,
            "\tpatternInfo.blockPattern.pixelCount[%d].x = %d",
            i,
            PDAFData.initParam.bufferDataInfo.isp1BlockPattern.pixelCoordinate[i].x);

        CAMX_LOG_VERBOSE(CamxLogGroupSensor,
            "\tpatternInfo.blockPattern.pixelCount[%d].y = %d",
            i,
            PDAFData.initParam.bufferDataInfo.isp1BlockPattern.pixelCoordinate[i].y);

        CAMX_LOG_VERBOSE(CamxLogGroupSensor,
            "\tpatternInfo.blockPattern.pixelCount[%d].type = %d",
            i,
            PDAFData.initParam.bufferDataInfo.isp1BlockPattern.pixelCoordinate[i].type);
    }
    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PDAFData::~PDAFData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PDAFData::~PDAFData()
{
    m_pPDAFConfigData = NULL;
    m_isPdafEnabled   = FALSE;
}

CAMX_NAMESPACE_END
