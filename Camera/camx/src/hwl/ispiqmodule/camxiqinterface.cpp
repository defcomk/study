////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxiqinterface.cpp
/// @brief CamX IQ interface implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Camx Headers
#include "camxnode.h"
#include "camxiqinterface.h"
#include "camxtuningdatamanager.h"
#include "camxutils.h"

/// IQ setting Headers
#include "anr10interpolation.h"
#include "anr10setting.h"
#include "asf30interpolation.h"
#include "asf30setting.h"
#include "bls12interpolation.h"
#include "bls12setting.h"
#include "bpsabf40interpolation.h"
#include "bpsabf40setting.h"
#include "bpsgic30interpolation.h"
#include "bpsgic30setting.h"
#include "hdr22interpolation.h"
#include "hdr22setting.h"
#include "hdr23interpolation.h"
#include "hdr23setting.h"
#include "bpslinearization34interpolation.h"
#include "bpslinearization34setting.h"
#include "bpspdpc20interpolation.h"
#include "bpspdpc20setting.h"
#include "camxhal3module.h"
#include "cac22interpolation.h"
#include "cac22setting.h"
#include "cc13interpolation.h"
#include "cc13setting.h"
#include "cst12setting.h"
#include "cv12interpolation.h"
#include "cv12setting.h"
#include "demosaic36interpolation.h"
#include "demosaic37interpolation.h"
#include "demosaic36setting.h"
#include "demosaic37setting.h"
#include "demux13setting.h"
#include "gamma15interpolation.h"
#include "gamma15setting.h"
#include "gamma16interpolation.h"
#include "gamma16setting.h"
#include "gra10interpolation.h"
#include "gra10setting.h"
#include "gtm10interpolation.h"
#include "gtm10setting.h"
#include "hnr10interpolation.h"
#include "hnr10setting.h"
#include "ica10interpolation.h"
#include "ica20interpolation.h"
#include "icasetting.h"
#include "ifeabf34interpolation.h"
#include "ifeabf34setting.h"
#include "ifebpcbcc50interpolation.h"
#include "ifebpcbcc50setting.h"
#include "ifecc12interpolation.h"
#include "ifecc12setting.h"
#include "ifehdr20interpolation.h"
#include "ifehdr20setting.h"
#include "ifelinearization33interpolation.h"
#include "ifelinearization33setting.h"
#include "ifepdpc11interpolation.h"
#include "ifepdpc11setting.h"
#include "iqcommondefs.h"
#include "ipecs20interpolation.h"
#include "ipecs20setting.h"
#include "ipe2dlut10interpolation.h"
#include "ipe2dlut10setting.h"
#include "iqcommondefs.h"
#include "iqsettingutil.h"
#include "lsc34interpolation.h"
#include "lsc34setting.h"
#include "ltm13interpolation.h"
#include "ltm13setting.h"
#include "pedestal13interpolation.h"
#include "pedestal13setting.h"
#include "sce11interpolation.h"
#include "sce11setting.h"
#include "tf10interpolation.h"
#include "tf10setting.h"
#include "tintless20interpolation.h"
#include "tmc10interpolation.h"
#include "upscale12setting.h"
#include "tmc11interpolation.h"
#include "upscale20interpolation.h"
#include "upscale20setting.h"
#include "wb12setting.h"
#include "wb13setting.h"
#include "Process_ICA.h"
#include "chiipedefs.h"

#if OEM1IQ
#include "oem1gamma15setting.h"
#include "oem1gamma16setting.h"
#include "oem1gtm10setting.h"
#include "oem1iqsettingutil.h"
#include "oem1asf30setting.h"
#include "oem1ltm13setting.h"
#endif // OEM1IQ

CAMX_NAMESPACE_BEGIN

// Install IQ Module Function Table
#if OEM1IQ
#include "oem1iqfunctiontable.h"
#else
#include "camxiqfunctiontable.h"
#endif // OEM1IQ

BOOL IQInterface::s_prevTintlessTableValid[MAX_NUM_OF_CAMERA][MAX_SENSOR_ASPECT_RATIO] = { {FALSE} };
LSC34UnpackedField IQInterface::s_prevTintlessOutput[MAX_NUM_OF_CAMERA][MAX_SENSOR_ASPECT_RATIO];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IQSettingModuleInitialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IQSettingModuleInitialize(
    IQLibInitialData* pLibData)
{
    CamxResult    result    = CamxResultSuccess;
    BOOL          isSucceed = FALSE;
    IQLibraryData libData;

    if (NULL == pLibData->pLibData)
    {
        isSucceed = IQInterface::s_interpolationTable.IQModuleInitialize(&libData);

        if (FALSE == isSucceed)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "IQ Setting Library Initialization Failed");
            result = CamxResultEFailed;
        }
        else
        {
            pLibData->pLibData = libData.pCustomticData;
        }

        pLibData->isSucceed = isSucceed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IQSettingModuleUninitialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IQSettingModuleUninitialize(
    IQLibInitialData* pData)
{
    CamxResult result    = CamxResultSuccess;
    BOOL       isSucceed = FALSE;
    IQLibraryData libData;

    if (NULL != pData->pLibData)
    {
        libData.pCustomticData = pData->pLibData;

        isSucceed = IQInterface::s_interpolationTable.IQModuleUninitialize(&libData);

        if (FALSE == isSucceed)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "IQ Setting Library Deinitialization Failed");
            result = CamxResultEFailed;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IQSetupTriggerData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IQInterface::IQSetupTriggerData(
    ISPInputData*           pInputData,
    Node*                   pNode,
    BOOL                    isRealTime,
    ISPIQTuningDataBuffer*  pIQOEMTriggerData)
{
    UINT32     metaTag          = 0;
    VOID*      pOEMData[]       = { 0 };
    UINT64     pOEMDataOffset[] = { 0 };
    CamxResult result           = CamxResultSuccess;

    CAMX_ASSERT(NULL != pInputData);

    FLOAT shortSensitivity = pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].sensitivity;
    FLOAT scalingFactor = 1.0f;

    if (0 != pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].exposureTime)
    {
        pInputData->triggerData.AECexposureTime     =
            static_cast<FLOAT>(pInputData->pAECUpdateData->exposureInfo[ExposureIndexLong].exposureTime) /
            static_cast<FLOAT>(pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].exposureTime);
    }
    else
    {
        pInputData->triggerData.AECexposureTime     = 1.0f;
    }

    if (FALSE == IQSettingUtils::FEqual(pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].linearGain, 0.0f))
    {
        pInputData->triggerData.AECexposureGainRatio = pInputData->pAECUpdateData->exposureInfo[ExposureIndexLong].linearGain
            / pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].linearGain;
    }
    else
    {
        pInputData->triggerData.AECexposureGainRatio = 1.0f;
    }

    if (FALSE == IQSettingUtils::FEqual(pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].sensitivity, 0.0f))
    {
        pInputData->triggerData.AECSensitivity      = pInputData->pAECUpdateData->exposureInfo[ExposureIndexLong].sensitivity
            / pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].sensitivity;
    }
    else
    {
        pInputData->triggerData.AECSensitivity     = 1.0f;
    }

    if (TRUE == pInputData->sensorData.isIHDR)
    {
        pInputData->triggerData.AECGain             = pInputData->pAECUpdateData->exposureInfo[ExposureIndexLong].linearGain;
    }
    else
    {
        pInputData->triggerData.AECGain             = pInputData->pAECUpdateData->exposureInfo[ExposureIndexSafe].linearGain;
    }
    pInputData->triggerData.AECShortGain        = pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].linearGain;
    pInputData->triggerData.AECLuxIndex         = pInputData->pAECUpdateData->luxIndex;
    pInputData->triggerData.AWBColorTemperature = static_cast<FLOAT>(pInputData->pAWBUpdateData->colorTemperature);
    pInputData->triggerData.lensZoom            = pInputData->lensZoom;
    pInputData->triggerData.postScaleRatio      = pInputData->postScaleRatio;
    pInputData->triggerData.preScaleRatio       = pInputData->preScaleRatio;
    pInputData->triggerData.totalScaleRatio     = pInputData->preScaleRatio * pInputData->postScaleRatio;

    ISPHALTagsData*     pHALTagsData             = pInputData->pHALTagsData;

    if ((NULL                               != pHALTagsData)                      &&
        (ColorCorrectionModeTransformMatrix == pHALTagsData->colorCorrectionMode) &&
        (((ControlAWBModeOff                == pHALTagsData->controlAWBMode)      &&
        (ControlModeAuto                    == pHALTagsData->controlMode))        ||
        (ControlModeOff                     == pHALTagsData->controlMode)))
    {
        pInputData->triggerData.AWBleftGGainWB = pHALTagsData->colorCorrectionGains.greenEven;
        pInputData->triggerData.AWBleftBGainWB = pHALTagsData->colorCorrectionGains.blue;
        pInputData->triggerData.AWBleftRGainWB = pHALTagsData->colorCorrectionGains.red;
    }
    else
    {
        pInputData->triggerData.AWBleftGGainWB  = pInputData->pAWBUpdateData->AWBGains.gGain;
        pInputData->triggerData.AWBleftBGainWB  = pInputData->pAWBUpdateData->AWBGains.bGain;
        pInputData->triggerData.AWBleftRGainWB  = pInputData->pAWBUpdateData->AWBGains.rGain;
        pInputData->triggerData.predictiveGain  = MAX2(pInputData->pAECUpdateData->predictiveGain, 1.0f);
    }

    if (NULL != pInputData->pAFUpdateData)
    {
        pInputData->triggerData.lensPosition =
            static_cast<FLOAT>(pInputData->pAFUpdateData->moveLensOutput.targetLensPosition);
    }

    if (pNode->Type() == BPS || pNode->Type() == IPE)
    {
        CAMX_LOG_INFO(CamxLogGroupPProc,
                      "NodeType = %d, preScaleRatio = %f, postScaleRatio = %f, totalScaleRatio = %f",
                      pNode->Type(),
                      pInputData->triggerData.preScaleRatio,
                      pInputData->triggerData.postScaleRatio,
                      pInputData->triggerData.totalScaleRatio);
    }

    if (NULL != pInputData->pCalculatedData)
    {
        pInputData->triggerData.blackLevelOffset = pInputData->pCalculatedData->BLSblackLevelOffset;
    }

    if (FALSE == IQSettingUtils::FEqual(shortSensitivity, 0.0f))
    {
        if (TRUE == pInputData->sensorData.isIHDR)
        {
            pInputData->triggerData.DRCGain = 1.0f;
        }
        else
        {
            pInputData->triggerData.DRCGain = pInputData->pAECUpdateData->exposureInfo[ExposureIndexSafe].sensitivity /
                pInputData->pAECUpdateData->exposureInfo[ExposureIndexShort].sensitivity;
        }

    }
    else
    {
        pInputData->triggerData.DRCGain = 0.0f;
    }

    if (FALSE == IQSettingUtils::FEqual(pInputData->pAECUpdateData->exposureInfo[ExposureIndexSafe].sensitivity, 0.0f))
    {
        if (TRUE == pInputData->sensorData.isIHDR)
        {
            pInputData->triggerData.DRCGainDark = 1.0f;
        }
        else
        {
            pInputData->triggerData.DRCGainDark = pInputData->pAECUpdateData->exposureInfo[ExposureIndexLong].sensitivity /
                pInputData->pAECUpdateData->exposureInfo[ExposureIndexSafe].sensitivity;
        }
    }
    else
    {
        pInputData->triggerData.DRCGainDark = 0.0f;
    }

    pInputData->triggerData.sensorImageWidth  = pInputData->sensorData.sensorOut.width;
    pInputData->triggerData.sensorImageHeight = pInputData->sensorData.sensorOut.height;

    if (NULL != pInputData->pStripeConfig)
    {
        pInputData->triggerData.CAMIFWidth        =
            pInputData->pStripeConfig->CAMIFCrop.lastPixel - pInputData->pStripeConfig->CAMIFCrop.firstPixel + 1;
        pInputData->triggerData.CAMIFHeight       =
            pInputData->pStripeConfig->CAMIFCrop.lastLine - pInputData->pStripeConfig->CAMIFCrop.firstLine + 1;
        if (NULL != pInputData->pStripeConfig->statsDataForISP.pParsedBHISTStats)
        {
            pInputData->triggerData.pParsedBHISTStats = pInputData->pStripeConfig->statsDataForISP.pParsedBHISTStats;
            pInputData->triggerData.maxPipelineDelay  = pInputData->maximumPipelineDelay;
        }
    }
    else
    {
        pInputData->triggerData.CAMIFWidth        = pInputData->sensorData.CAMIFCrop.lastPixel;
        pInputData->triggerData.CAMIFHeight       = pInputData->sensorData.CAMIFCrop.lastLine;
    }

    pInputData->triggerData.LEDSensitivity      = pInputData->pAECUpdateData->LEDInfluenceRatio;
    pInputData->triggerData.LEDFirstEntryRatio  = pInputData->pAECUpdateData->LEDFirstEntryRatio;
    pInputData->triggerData.numberOfLED         = pInputData->numberOfLED;

    result = IQInterface::GetPixelFormat(&pInputData->sensorData.format,
                                         &pInputData->triggerData.bayerPattern);

    scalingFactor                               =
        (pInputData->sensorData.sensorScalingFactor) * (pInputData->sensorData.sensorBinningFactor);
    pInputData->triggerData.sensorOffsetX       =
        static_cast<UINT32>((pInputData->sensorData.CAMIFCrop.firstPixel) * scalingFactor);
    pInputData->triggerData.sensorOffsetY       =
        static_cast<UINT32>((pInputData->sensorData.CAMIFCrop.firstLine) * scalingFactor);

    for (UINT16 count = 0; count < g_customaticTriggerNumber; count++)
    {
        result =
            VendorTagManager::QueryVendorTagLocation(g_pOEM1TriggerTagList[count].tagSessionName,
                                                     g_pOEM1TriggerTagList[count].tagName,
                                                     &metaTag);
        UINT OEMProperty[] = { 0 };

        if (FALSE == isRealTime)
        {
            OEMProperty[0] = metaTag | InputMetadataSectionMask;
        }
        else
        {
            OEMProperty[0] = metaTag;
        }

        CAMX_ASSERT(CamxResultSuccess == result);

        pNode->GetDataList(OEMProperty, pOEMData, pOEMDataOffset, 1);

        pInputData->triggerData.pOEMTrigger[count] = pOEMData[0];
    }
    if (0 < g_customaticTriggerNumber)
    {
        CAMX_ASSERT(NULL != IQInterface::s_interpolationTable.IQFillOEMTuningTriggerData);
        // Fill OEM custom trigger data
        IQInterface::s_interpolationTable.IQFillOEMTuningTriggerData(&pInputData->triggerData, pIQOEMTriggerData);
    }

    pInputData->triggerData.pLibInitialData = pInputData->pLibInitialData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IQInterface::GetADRCParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IQInterface::GetADRCParams(
    const ISPInputData* pInputData,
    BOOL*               pAdrcEnabled,
    FLOAT*              pGtmPercentage,
    const SWTMCVersion  tmcVersion)
{
    TuningDataManager*   pTuningManager = NULL;

    *pAdrcEnabled        = FALSE;
    *pGtmPercentage      = 0.0f;
    pTuningManager       = pInputData->pTuningDataManager;

    CAMX_ASSERT(NULL != pInputData->pTuningData);

    switch (tmcVersion)
    {
        case SWTMCVersion::TMC10:
            {
                tmc_1_0_0::chromatix_tmc10Type* ptmcChromatix = NULL;

                if (TRUE == pTuningManager->IsValidChromatix())
                {
                    ptmcChromatix = pTuningManager->GetChromatix()->GetModule_tmc10_sw(
                        reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                        pInputData->pTuningData->noOfSelectionParameter);
                }

                if (NULL != ptmcChromatix)
                {
                    if ((TRUE  == ptmcChromatix->enable_section.adrc_isp_enable) &&
                        (TRUE  == ptmcChromatix->chromatix_tmc10_reserve.use_gtm) &&
                        (FALSE == pInputData->sensorData.isIHDR))
                    {
                        *pAdrcEnabled   = TRUE;
                        *pGtmPercentage = ptmcChromatix->chromatix_tmc10_core.mod_tmc10_aec_data->tmc10_rgn_data.gtm_percentage;
                    }
                }
            }
            break;

        case SWTMCVersion::TMC11:
            {
                tmc_1_1_0::chromatix_tmc11Type* ptmcChromatix = NULL;
                if (TRUE == pTuningManager->IsValidChromatix())
                {
                    ptmcChromatix = pTuningManager->GetChromatix()->GetModule_tmc11_sw(
                        reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                        pInputData->pTuningData->noOfSelectionParameter);
                }

                if (NULL != ptmcChromatix)
                {
                    if ((TRUE  == ptmcChromatix->enable_section.tmc_enable) &&
                        (TRUE  == ptmcChromatix->chromatix_tmc11_reserve.use_gtm) &&
                        (FALSE == pInputData->sensorData.isIHDR))
                    {
                        *pAdrcEnabled   = TRUE;
                        *pGtmPercentage = ptmcChromatix->chromatix_tmc11_core.mod_tmc11_drc_gain_data->
                                          drc_gain_data.mod_tmc11_hdr_aec_data->hdr_aec_data.mod_tmc11_aec_data->
                                          tmc11_rgn_data.gtm_percentage;
                    }
                }
            }
            break;

        default:
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Unsupported TMC Version = %u", static_cast<UINT>(tmcVersion));

    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::UpdateAECGain()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IQInterface::UpdateAECGain(
    ISPIQModuleType mType,
    ISPInputData*   pInputData,
    FLOAT           gtmPercentage)
{
    FLOAT gain     = pInputData->triggerData.AECShortGain;
    FLOAT drc_gain = pInputData->triggerData.DRCGain;

    // Gain before GTM module is triggered by short gain
    pInputData->triggerData.AECGain = gain;

    if (mType == ISPIQModuleType::IFEGTM   ||
        mType == ISPIQModuleType::IFEGamma ||
        mType == ISPIQModuleType::IFECST   ||
        mType == ISPIQModuleType::BPSGTM   ||
        mType == ISPIQModuleType::BPSGamma ||
        mType == ISPIQModuleType::BPSCST   ||
        mType == ISPIQModuleType::BPSHNR   ||
        mType == ISPIQModuleType::IPECAC   ||
        mType == ISPIQModuleType::IPEICA   ||
        mType == ISPIQModuleType::IPEANR   ||
        mType == ISPIQModuleType::IPETF    ||
        mType == ISPIQModuleType::IPECST   ||
        mType == ISPIQModuleType::IPELTM)
    {
        // Gain betweem GTM & LTM ( includes ) will be triggered by shortGain*power(DRCGain,gtm_perc)
        pInputData->triggerData.AECGain = gain * static_cast<FLOAT>(CamX::Utils::Power(drc_gain, gtmPercentage));
    }

    if (mType == ISPIQModuleType::IPEColorCorrection   ||
        mType == ISPIQModuleType::IPEGamma             ||
        mType == ISPIQModuleType::IPE2DLUT             ||
        mType == ISPIQModuleType::IPEChromaEnhancement ||
        mType == ISPIQModuleType::IPEChromaSuppression ||
        mType == ISPIQModuleType::IPESCE               ||
        mType == ISPIQModuleType::IPEASF               ||
        mType == ISPIQModuleType::IPEUpscaler          ||
        mType == ISPIQModuleType::IPEGrainAdder)
    {
        // Gain post LTM will be triggered by shortGain*DRCGain
        pInputData->triggerData.AECGain = gain * drc_gain;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::GetADRCData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IQInterface::GetADRCData(
    TMC10InputData*   pTMCInput)
{
    BOOL                           commonIQResult = FALSE;
    tmc_1_0_0::tmc10_rgn_dataType  interpolationDataTMC;

    // Call the Interpolation Calculation
    commonIQResult = IQInterface::s_interpolationTable.TMC10Interpolation(pTMCInput, &interpolationDataTMC);

    return commonIQResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IQSetHardwareVersion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IQInterface::IQSetHardwareVersion(
    UINT32 titanVersion,
    UINT32 hardwareVersion)
{
    BOOL  commonIQResult = FALSE;
    commonIQResult = IQInterface::s_interpolationTable.IQSetHardwareVersion(titanVersion, hardwareVersion);
    return commonIQResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupDemuxReg
///
/// @brief  Setup Demux Register based on common library output
///
/// @param  pData      Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the Demux Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupDemuxReg(
    const Demux13UnpackedField* pData,
    IFEDemux13RegCmd*           pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->demuxConfig.bitfields.PERIOD                = pData->period;
        pRegCmd->demuxConfig.bitfields.BLK_OUT               = pData->blackLevelOut;
        pRegCmd->demuxConfig.bitfields.BLK_IN                = pData->blackLevelIn;
        pRegCmd->demuxGain0.bitfields.CH0_GAIN_EVEN          = pData->gainChannel0EevenLeft;
        pRegCmd->demuxGain0.bitfields.CH0_GAIN_ODD           = pData->gainChannel0OddLeft;
        pRegCmd->demuxGain1.bitfields.CH1_GAIN               = pData->gainChannel1Left;
        pRegCmd->demuxGain1.bitfields.CH2_GAIN               = pData->gainChannel2Left;
        pRegCmd->demuxRightGain0.bitfields.CH0_GAIN_EVEN     = pData->gainChannel0EvenRight;
        pRegCmd->demuxRightGain0.bitfields.CH0_GAIN_ODD      = pData->gainChannel0OddRight;
        pRegCmd->demuxRightGain1.bitfields.CH1_GAIN          = pData->gainChannel1Right;
        pRegCmd->demuxRightGain1.bitfields.CH2_GAIN          = pData->gainChannel2Right;
        pRegCmd->demuxEvenConfig.bitfields.EVEN_LINE_PATTERN = pData->evenConfig;
        pRegCmd->demuxOddConfig.bitfields.ODD_LINE_PATTERN   = pData->oddConfig;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input Data pData %p pRegCmd %p", pData, pRegCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFEGetSensorMode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IFEGetSensorMode(
    const PixelFormat*  pPixelFormat,
    SensorType*         pSensorType)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pPixelFormat) && (NULL != pSensorType))
    {
        switch (*pPixelFormat)
        {
            case PixelFormat::BayerRGGB:
            case PixelFormat::BayerBGGR:
            case PixelFormat::BayerGBRG:
            case PixelFormat::BayerGRBG:
                *pSensorType = SensorType::BAYER_RGGB;
                break;

            case PixelFormat::YUVFormatUYVY:
            case PixelFormat::YUVFormatYUYV:
            case PixelFormat::YUVFormatY:
                *pSensorType = SensorType::BAYER_RCCB;
                break;

            case PixelFormat::MetaStatsHDR:
                *pSensorType = SensorType::BAYER_HDR;
                break;

            case PixelFormat::MetaStatsPDAF:
                *pSensorType = SensorType::BAYER_PDAF;
                break;

            default:
                CAMX_ASSERT_ALWAYS_MESSAGE("No Corresponding Format. Format = %d", *pPixelFormat);
                result = CamxResultEUnsupported;
                break;
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null data ");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupDemuxReg
///
/// @brief  Setup Demux Register based on common library output
///
/// @param  pRegVal    Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the Demux Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupDemuxReg(
    const Demux13UnpackedField* pData,
    BPSDemux13RegCmd*           pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->config.bitfields.PERIOD                   = pData->period;
        pRegCmd->config.bitfields.BLK_OUT                  = pData->blackLevelOut;
        pRegCmd->config.bitfields.BLK_IN                   = pData->blackLevelIn;
        pRegCmd->gainChannel0.bitfields.CH0_GAIN_EVEN      = pData->gainChannel0EevenLeft;
        pRegCmd->gainChannel0.bitfields.CH0_GAIN_ODD       = pData->gainChannel0OddLeft;
        pRegCmd->gainChannel12.bitfields.CH1_GAIN          = pData->gainChannel1Left;
        pRegCmd->gainChannel12.bitfields.CH2_GAIN          = pData->gainChannel2Left;
        pRegCmd->gainRightChannel0.bitfields.CH0_GAIN_EVEN = pData->gainChannel0EvenRight;
        pRegCmd->gainRightChannel0.bitfields.CH0_GAIN_ODD  = pData->gainChannel0OddRight;
        pRegCmd->gainRightChannel12.bitfields.CH1_GAIN     = pData->gainChannel1Right;
        pRegCmd->gainRightChannel12.bitfields.CH2_GAIN     = pData->gainChannel2Right;
        pRegCmd->evenConfig.bitfields.EVEN_LINE_PATTERN    = pData->evenConfig;
        pRegCmd->oddConfig.bitfields.ODD_LINE_PATTERN      = pData->oddConfig;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input Data is pData %p pRegCmd %p", pData, pRegCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::GetPixelFormat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::GetPixelFormat(
    const PixelFormat*  pPixelFormat,
    UINT8*              pBayerPattern)
{
    CamxResult result = CamxResultSuccess;

    switch (*pPixelFormat)
    {
        case PixelFormat::BayerRGGB:
            *pBayerPattern = RGGB_PATTERN;
            break;

        case PixelFormat::BayerBGGR:
            *pBayerPattern = BGGR_PATTERN;
            break;

        case PixelFormat::BayerGBRG:
            *pBayerPattern = GBRG_PATTERN;
            break;

        case PixelFormat::BayerGRBG:
            *pBayerPattern = GRBG_PATTERN;
            break;

        case PixelFormat::YUVFormatUYVY:
            *pBayerPattern = CBYCRY422_PATTERN;
            break;

        case PixelFormat::YUVFormatYUYV:
            *pBayerPattern = YCBYCR422_PATTERN;
            break;

        case PixelFormat::YUVFormatY:
            *pBayerPattern = Y_PATTERN;
            break;

        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("No Corresponding Format. Format = %d", *pPixelFormat);
            result = CamxResultEUnsupported;
            break;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::Demux13CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::Demux13CalculateSetting(
    Demux13InputData*  pInput,
    VOID*              pOEMIQData,
    Demux13OutputData* pOutput,
    PixelFormat        pixelFormat)
{
    CamxResult                                                result         = CamxResultSuccess;
    BOOL                                                      commonIQResult = FALSE;
    demux_1_3_0::chromatix_demux13_reserveType*               pReserveType   = NULL;
    demux_1_3_0::chromatix_demux13Type::enable_sectionStruct* pModuleEnable  = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        result = GetPixelFormat(&pixelFormat, &(pInput->bayerPattern));

        if (NULL == pOEMIQData)
        {
            if (NULL != pInput->pChromatixInput)
            {
                pReserveType  = &(pInput->pChromatixInput->chromatix_demux13_reserve);
                pModuleEnable = &(pInput->pChromatixInput->enable_section);
            }
            else
            {
                result = CamxResultEFailed;
            }
        }
        else
        {
            if (PipelineType::IFE == pOutput->type)
            {
                OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pReserveType  = reinterpret_cast<demux_1_3_0::chromatix_demux13_reserveType*>(
                                     &(pOEMInput->DemuxSetting));
                pModuleEnable = reinterpret_cast<demux_1_3_0::chromatix_demux13Type::enable_sectionStruct*>(
                                     &(pOEMInput->DemuxEnableSection));
            }
            else if (PipelineType::BPS == pOutput->type)
            {
                OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pReserveType   = reinterpret_cast<demux_1_3_0::chromatix_demux13_reserveType*>(
                                     &(pOEMInput->DemuxSetting));
                pModuleEnable  = reinterpret_cast<demux_1_3_0::chromatix_demux13Type::enable_sectionStruct*>(
                                     &(pOEMInput->DemuxEnableSection));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for Demux13", pOutput->type);
            }
        }

        if (NULL != pReserveType)
        {
            commonIQResult = TRUE;
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("No Reserve Data");
        }


        if (TRUE == commonIQResult)
        {
            Demux13UnpackedField unpackedData;

            if (CamxResultSuccess == result)
            {
                // Call the Interpolation Calculation
                commonIQResult =
                    IQInterface::s_interpolationTable.demux13CalculateHWSetting(pInput,
                                                                                pReserveType,
                                                                                pModuleEnable,
                                                                                static_cast<VOID*>(&unpackedData));
            }

            if (TRUE == commonIQResult)
            {
                if (PipelineType::IFE == pOutput->type)
                {
                    IFESetupDemuxReg(&unpackedData, pOutput->regCommand.pRegIFECmd);
                }
                else if (PipelineType::BPS == pOutput->type)
                {
                    BPSSetupDemuxReg(&unpackedData, pOutput->regCommand.pRegBPSCmd);
                }
                else
                {
                    result = CamxResultEInvalidArg;
                    CAMX_ASSERT_ALWAYS_MESSAGE("invalid Pipeline");
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("Demux Calculate HW settings failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Demux interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null input/output parameter to Demux13 module");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupDemosaic36Register
///
/// @brief  Setup BPS Demosaic36 Register based on common library output
///
/// @param  pRegVal    Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the Demosaic36 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupDemosaic36Register(
    const Demosaic36UnpackedField* pData,
    Demosaic36OutputData*          pOutput)
{
    if ((NULL != pData) && (NULL != pOutput))
    {
        pOutput->BPS.pModuleConfig->moduleCfg.EN                = pData->enable;
        pOutput->BPS.pModuleConfig->moduleCfg.COSITED_RGB_EN    = pData->cositedRGB;
        pOutput->BPS.pModuleConfig->moduleCfg.DIR_G_INTERP_DIS  = pData->disableDirectionalG;
        pOutput->BPS.pModuleConfig->moduleCfg.DIR_RB_INTERP_DIS = pData->disableDirectionalRB;
        pOutput->BPS.pModuleConfig->moduleCfg.DYN_G_CLAMP_EN    = pData->enDynamicClampG;
        pOutput->BPS.pModuleConfig->moduleCfg.DYN_RB_CLAMP_EN   = pData->enDynamicClampRB;

        pOutput->BPS.pRegBPSCmd->interpolationCoefficientConfig.bitfields.LAMDA_RB = pData->lambdaRB;
        pOutput->BPS.pRegBPSCmd->interpolationCoefficientConfig.bitfields.LAMDA_G  = pData->lambdaG;
        pOutput->BPS.pRegBPSCmd->interpolationClassifierConfig.bitfields.A_K       = pData->ak;
        pOutput->BPS.pRegBPSCmd->interpolationClassifierConfig.bitfields.W_K       = pData->wk;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL ");
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupDemosaic36Register
///
/// @brief  Setup Demosaic36 Register based on common library output
///
/// @param  pRegVal    Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the Demosaic36 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupDemosaic36Register(
    const Demosaic36UnpackedField*  pData,
    IFEDemosaic36RegCmd*            pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->demosaic36RegCmd1.moduleConfig.bitfields.DIR_G_INTERP_DIS  = pData->disableDirectionalG;
        pRegCmd->demosaic36RegCmd1.moduleConfig.bitfields.DIR_RB_INTERP_DIS = pData->disableDirectionalRB;
        pRegCmd->demosaic36RegCmd1.moduleConfig.bitfields.COSITED_RGB_EN    = pData->cositedRGB;
        pRegCmd->demosaic36RegCmd1.moduleConfig.bitfields.DYN_G_CLAMP_EN    = pData->enDynamicClampG;
        pRegCmd->demosaic36RegCmd1.moduleConfig.bitfields.DYN_RB_CLAMP_EN   = pData->enDynamicClampRB;
        pRegCmd->demosaic36RegCmd1.moduleConfig.bitfields.LB_ONLY_EN        = !pData->enable; ///< Disable LB if module enable

        pRegCmd->demosaic36RegCmd2.interpolationCoeffConfig.bitfields.LAMDA_RB = pData->lambdaRB;
        pRegCmd->demosaic36RegCmd2.interpolationCoeffConfig.bitfields.LAMDA_G  = pData->lambdaG;

        pRegCmd->demosaic36RegCmd2.interpolationClassifier0.bitfields.A_N = pData->ak;
        pRegCmd->demosaic36RegCmd2.interpolationClassifier0.bitfields.W_N = pData->wk;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pRegCmd %p ", pData, pRegCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::Demosaic36CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::Demosaic36CalculateSetting(
    const Demosaic36InputData*  pInput,
    VOID*                       pOEMIQData,
    Demosaic36OutputData*       pOutput)
{
    CamxResult                                                      result             = CamxResultSuccess;
    BOOL                                                            commonIQResult     = FALSE;
    demosaic_3_6_0::demosaic36_rgn_dataType*                        pInterpolationData = NULL;
    demosaic_3_6_0::chromatix_demosaic36Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<demosaic_3_6_0::demosaic36_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult     = IQInterface::s_interpolationTable.demosaic36Interpolation(pInput, pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            if (PipelineType::IFE == pOutput->type)
            {
                OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pInterpolationData         =
                    reinterpret_cast<demosaic_3_6_0::demosaic36_rgn_dataType*>(&(pOEMInput->DemosaicSetting));
                pModuleEnable              =
                    reinterpret_cast<demosaic_3_6_0::chromatix_demosaic36Type::enable_sectionStruct*>(
                    &(pOEMInput->DemosaicEnableSection));
            }
            else if (PipelineType::BPS == pOutput->type)
            {
                OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pInterpolationData         =
                    reinterpret_cast<demosaic_3_6_0::demosaic36_rgn_dataType*>(&(pOEMInput->DemosaicSetting));
                pModuleEnable              =
                    reinterpret_cast<demosaic_3_6_0::chromatix_demosaic36Type::enable_sectionStruct*>(
                        &(pOEMInput->DemosaicEnableSection));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for Demux13", pOutput->type);
            }

            if ((NULL != pInterpolationData) && (NULL != pModuleEnable))
            {
                commonIQResult = TRUE;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            Demosaic36UnpackedField unpackedRegData;

            commonIQResult =
                IQInterface::s_interpolationTable.demosaic36CalculateHWSetting(pInput,
                                                                               pInterpolationData,
                                                                               pModuleEnable,
                                                                               static_cast<VOID*>(&unpackedRegData));
            if (TRUE == commonIQResult)
            {
                if (PipelineType::IFE == pOutput->type)
                {
                    IFESetupDemosaic36Register(&unpackedRegData, pOutput->IFE.pRegIFECmd);
                }
                else if (PipelineType::BPS == pOutput->type)
                {
                    BPSSetupDemosaic36Register(&unpackedRegData, pOutput);
                }
                else
                {
                    result = CamxResultEInvalidArg;
                    CAMX_ASSERT_ALWAYS_MESSAGE("invalid Pipeline");
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE(" Demosaic CalculateHWSetting failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Demosaic interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL ");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupDemosaic37Register
///
/// @brief  Setup Demosaic37 Register based on common library output
///
/// @param  pRegVal    Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the Demosaic37 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupDemosaic37Register(
    const Demosaic37UnpackedField*  pData,
    IFEDemosaic37RegCmd*            pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->demosaic37RegCmd1.moduleConfig.bitfields.DIR_G_INTERP_DIS     = pData->disableDirectionalG;
        pRegCmd->demosaic37RegCmd1.moduleConfig.bitfields.DIR_RB_INTERP_DIS    = pData->disableDirectionalRB;
        pRegCmd->demosaic37RegCmd1.moduleConfig.bitfields.COSITED_RGB_EN       = pData->cositedRGB;
        pRegCmd->demosaic37RegCmd1.moduleConfig.bitfields.DYN_G_CLAMP_EN       = pData->enDynamicClampG;
        pRegCmd->demosaic37RegCmd1.moduleConfig.bitfields.DYN_RB_CLAMP_EN      = pData->enDynamicClampRB;

        // Disable LB if module enable
        pRegCmd->demosaic37RegCmd1.moduleConfig.bitfields.LB_ONLY_EN           = !pData->enable;

        pRegCmd->demosaic37RegCmd2.interpolationCoeffConfig.bitfields.LAMDA_RB = pData->lambdaRB;
        pRegCmd->demosaic37RegCmd2.interpolationCoeffConfig.bitfields.LAMDA_G  = pData->lambdaG;

        pRegCmd->demosaic37RegCmd2.interpolationClassifier0.bitfields.A_N      = pData->ak;
        pRegCmd->demosaic37RegCmd2.interpolationClassifier0.bitfields.W_N      = pData->wk;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pRegCmd %p ", pData, pRegCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::Demosaic37CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::Demosaic37CalculateSetting(
    const Demosaic37InputData*  pInput,
    VOID*                       pOEMIQData,
    Demosaic37OutputData*       pOutput)
{
    CamxResult                                                      result             = CamxResultSuccess;
    BOOL                                                            commonIQResult     = FALSE;
    demosaic_3_7_0::demosaic37_rgn_dataType*                        pInterpolationData = NULL;
    demosaic_3_7_0::chromatix_demosaic37Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<demosaic_3_7_0::demosaic37_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult     = IQInterface::s_interpolationTable.demosaic37Interpolation(pInput, pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            if (PipelineType::IFE == pOutput->type)
            {
                OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pInterpolationData =
                    reinterpret_cast<demosaic_3_7_0::demosaic37_rgn_dataType*>(&(pOEMInput->DemosaicSetting));
                pModuleEnable      =
                    reinterpret_cast<demosaic_3_7_0::chromatix_demosaic37Type::enable_sectionStruct*>(
                        &(pOEMInput->DemosaicEnableSection));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for Demosaic37", pOutput->type);
            }

            if ((NULL != pInterpolationData) && (NULL != pModuleEnable))
            {
                commonIQResult = TRUE;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            Demosaic37UnpackedField unpackedRegData;

            commonIQResult =
                IQInterface::s_interpolationTable.demosaic37CalculateHWSetting(pInput,
                    pInterpolationData,
                    pModuleEnable,
                    static_cast<VOID*>(&unpackedRegData));
            if (TRUE == commonIQResult)
            {
                if (PipelineType::IFE == pOutput->type)
                {
                    IFESetupDemosaic37Register(&unpackedRegData, pOutput->IFE.pRegIFECmd);
                }
                else
                {
                    result = CamxResultEInvalidArg;
                    CAMX_ASSERT_ALWAYS_MESSAGE("invalid Pipeline");
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE(" Demosaic CalculateHWSetting failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Demosaic interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL ");
    }

    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupBPCBCC50Reg
///
/// @brief  Setup BPCBCC50 Register based on common library output
///
/// @param  pData     Pointer to the Common Library Calculation Result
/// @param  pRegCmd   Pointer to the Demux Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupBPCBCC50Reg(
    const BPCBCC50UnpackedField*  pData,
    IFEBPCBCC50RegCmd*            pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->configRegister0.bitfields.HOT_PIXEL_CORR_DISABLE  = pData->hotPixelCorrectionDisable;
        pRegCmd->configRegister0.bitfields.COLD_PIXEL_CORR_DISABLE = pData->coldPixelCorrectionDisable;
        pRegCmd->configRegister0.bitfields.SAME_CH_RECOVER         = pData->sameChannelRecovery;
        pRegCmd->configRegister0.bitfields.BLACK_LEVEL             = pData->black_level;

        pRegCmd->configRegister1.bitfields.RG_WB_GAIN_RATIO        = pData->rg_wb_gain_ratio;
        pRegCmd->configRegister2.bitfields.BG_WB_GAIN_RATIO        = pData->bg_wb_gain_ratio;
        pRegCmd->configRegister3.bitfields.GR_WB_GAIN_RATIO        = pData->gr_wb_gain_ratio;
        pRegCmd->configRegister4.bitfields.GB_WB_GAIN_RATIO        = pData->gb_wb_gain_ratio;

        pRegCmd->configRegister5.bitfields.BPC_OFFSET              = pData->bpcOffset;
        pRegCmd->configRegister5.bitfields.BCC_OFFSET              = pData->bccOffset;

        pRegCmd->configRegister6.bitfields.FMIN                    = pData->fmin;
        pRegCmd->configRegister6.bitfields.FMAX                    = pData->fmax;
        pRegCmd->configRegister6.bitfields.CORRECT_THRESHOLD       = pData->correctionThreshold;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pRegCmd %p", pData, pRegCmd);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupLinearization33Reg
///
/// @brief  Setup Linearization33 Register based on common library output
///
/// @param  pData    Pointer to Unpacked Data Set
/// @param  pOutput  Pointer to Output data set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupLinearization33Reg(
    const Linearization33UnpackedField*  pData,
    Linearization33OutputData*           pOutput)
{
    IFELinearization33RegCmd* pRegCmd        = NULL;
    UINT32                    DMILength      = 0;
    UINT                      count          = 0;

    if ((NULL != pOutput) && (NULL != pData) && (NULL != pOutput->pRegCmd) && (NULL != pOutput->pDMIDataPtr))
    {
        pRegCmd = pOutput->pRegCmd;

        pRegCmd->configRegister.bitfields.LUT_BANK_SEL = pData->lut_bank_sel;

        /// Left plane
        pRegCmd->interpolationB0Register.bitfields.LUT_P0  = pData->b_lut_p_l[0];
        pRegCmd->interpolationB0Register.bitfields.LUT_P1  = pData->b_lut_p_l[1];
        pRegCmd->interpolationB1Register.bitfields.LUT_P2  = pData->b_lut_p_l[2];
        pRegCmd->interpolationB1Register.bitfields.LUT_P3  = pData->b_lut_p_l[3];
        pRegCmd->interpolationB2Register.bitfields.LUT_P4  = pData->b_lut_p_l[4];
        pRegCmd->interpolationB2Register.bitfields.LUT_P5  = pData->b_lut_p_l[5];
        pRegCmd->interpolationB3Register.bitfields.LUT_P6  = pData->b_lut_p_l[6];
        pRegCmd->interpolationB3Register.bitfields.LUT_P7  = pData->b_lut_p_l[7];

        pRegCmd->interpolationR0Register.bitfields.LUT_P0  = pData->r_lut_p_l[0];
        pRegCmd->interpolationR0Register.bitfields.LUT_P1  = pData->r_lut_p_l[1];
        pRegCmd->interpolationR1Register.bitfields.LUT_P2  = pData->r_lut_p_l[2];
        pRegCmd->interpolationR1Register.bitfields.LUT_P3  = pData->r_lut_p_l[3];
        pRegCmd->interpolationR2Register.bitfields.LUT_P4  = pData->r_lut_p_l[4];
        pRegCmd->interpolationR2Register.bitfields.LUT_P5  = pData->r_lut_p_l[5];
        pRegCmd->interpolationR3Register.bitfields.LUT_P6  = pData->r_lut_p_l[6];
        pRegCmd->interpolationR3Register.bitfields.LUT_P7  = pData->r_lut_p_l[7];

        pRegCmd->interpolationGB0Register.bitfields.LUT_P0 = pData->gb_lut_p_l[0];
        pRegCmd->interpolationGB0Register.bitfields.LUT_P1 = pData->gb_lut_p_l[1];
        pRegCmd->interpolationGB1Register.bitfields.LUT_P2 = pData->gb_lut_p_l[2];
        pRegCmd->interpolationGB1Register.bitfields.LUT_P3 = pData->gb_lut_p_l[3];
        pRegCmd->interpolationGB2Register.bitfields.LUT_P4 = pData->gb_lut_p_l[4];
        pRegCmd->interpolationGB2Register.bitfields.LUT_P5 = pData->gb_lut_p_l[5];
        pRegCmd->interpolationGB3Register.bitfields.LUT_P6 = pData->gb_lut_p_l[6];
        pRegCmd->interpolationGB3Register.bitfields.LUT_P7 = pData->gb_lut_p_l[7];

        pRegCmd->interpolationGR0Register.bitfields.LUT_P0 = pData->gr_lut_p_l[0];
        pRegCmd->interpolationGR0Register.bitfields.LUT_P1 = pData->gr_lut_p_l[1];
        pRegCmd->interpolationGR1Register.bitfields.LUT_P2 = pData->gr_lut_p_l[2];
        pRegCmd->interpolationGR1Register.bitfields.LUT_P3 = pData->gr_lut_p_l[3];
        pRegCmd->interpolationGR2Register.bitfields.LUT_P4 = pData->gr_lut_p_l[4];
        pRegCmd->interpolationGR2Register.bitfields.LUT_P5 = pData->gr_lut_p_l[5];
        pRegCmd->interpolationGR3Register.bitfields.LUT_P6 = pData->gr_lut_p_l[6];
        pRegCmd->interpolationGR3Register.bitfields.LUT_P7 = pData->gr_lut_p_l[7];

        ///< packing the DMI table
        for (count = 0; count < MaxSlope ; count++, DMILength += 4)
        {
            pOutput->pDMIDataPtr[DMILength]    =
                ((static_cast<UINT64>(pData->r_lut_base_l[pData->lut_bank_sel][count])) & Utils::AllOnes64(14)) |
                ((static_cast<UINT64>(pData->r_lut_delta_l[pData->lut_bank_sel][count]) & Utils::AllOnes64(26)) << 14);

            pOutput->pDMIDataPtr[DMILength + 1] =
                ((static_cast<UINT64>(pData->gr_lut_base_l[pData->lut_bank_sel][count])) & Utils::AllOnes64(14)) |
                ((static_cast<UINT64>( pData->gr_lut_delta_l[pData->lut_bank_sel][count]) & Utils::AllOnes64(26)) << 14);

            pOutput->pDMIDataPtr[DMILength + 2] =
                ((static_cast<UINT64>(pData->gb_lut_base_l[pData->lut_bank_sel][count])) & Utils::AllOnes64(14)) |
                ((static_cast<UINT64>(pData->gb_lut_delta_l[pData->lut_bank_sel][count]) & Utils::AllOnes64(26)) << 14);

            pOutput->pDMIDataPtr[DMILength + 3] =
                ((static_cast<UINT64>(pData->b_lut_base_l[pData->lut_bank_sel][count])) & Utils::AllOnes64(14)) |
                ((static_cast<UINT64>(pData->b_lut_delta_l[pData->lut_bank_sel][count]) & Utils::AllOnes64(26)) << 14);

            if (TRUE != pOutput->registerBETEn)
            {
                const HwEnvironment*      pHwEnvironment = HwEnvironment::GetInstance();
                if (pHwEnvironment->IsHWBugWorkaroundEnabled(Titan17xWorkarounds::Titan17xWorkaroundsCDMDMI64EndiannessBug))
                {
                    pOutput->pDMIDataPtr[DMILength] = ((pOutput->pDMIDataPtr[DMILength] & CamX::Utils::AllOnes64(32)) << 32) |
                        (((pOutput->pDMIDataPtr[DMILength] >> 32) & CamX::Utils::AllOnes64(32)));
                    pOutput->pDMIDataPtr[DMILength + 1] =
                        ((pOutput->pDMIDataPtr[DMILength + 1] & CamX::Utils::AllOnes64(32)) << 32) |
                        (((pOutput->pDMIDataPtr[DMILength + 1] >> 32) & CamX::Utils::AllOnes64(32)));
                    pOutput->pDMIDataPtr[DMILength + 2] =
                        ((pOutput->pDMIDataPtr[DMILength + 2] & CamX::Utils::AllOnes64(32)) << 32) |
                        (((pOutput->pDMIDataPtr[DMILength + 2] >> 32) & CamX::Utils::AllOnes64(32)));
                    pOutput->pDMIDataPtr[DMILength + 3] =
                        ((pOutput->pDMIDataPtr[DMILength + 3] & CamX::Utils::AllOnes64(32)) << 32) |
                        (((pOutput->pDMIDataPtr[DMILength + 3] >> 32) & CamX::Utils::AllOnes64(32)));
                }
            }
        }

        /// Right plane
        pRegCmd->rightPlaneInterpolationB0Register.bitfields.LUT_P0  = pData->b_lut_p_r[0];
        pRegCmd->rightPlaneInterpolationB0Register.bitfields.LUT_P1  = pData->b_lut_p_r[1];
        pRegCmd->rightPlaneInterpolationB1Register.bitfields.LUT_P2  = pData->b_lut_p_r[2];
        pRegCmd->rightPlaneInterpolationB1Register.bitfields.LUT_P3  = pData->b_lut_p_r[3];
        pRegCmd->rightPlaneInterpolationB2Register.bitfields.LUT_P4  = pData->b_lut_p_r[4];
        pRegCmd->rightPlaneInterpolationB2Register.bitfields.LUT_P5  = pData->b_lut_p_r[5];
        pRegCmd->rightPlaneInterpolationB3Register.bitfields.LUT_P6  = pData->b_lut_p_r[6];
        pRegCmd->rightPlaneInterpolationB3Register.bitfields.LUT_P7  = pData->b_lut_p_r[7];

        pRegCmd->rightPlaneInterpolationR0Register.bitfields.LUT_P0  = pData->r_lut_p_r[0];
        pRegCmd->rightPlaneInterpolationR0Register.bitfields.LUT_P1  = pData->r_lut_p_r[1];
        pRegCmd->rightPlaneInterpolationR1Register.bitfields.LUT_P2  = pData->r_lut_p_r[2];
        pRegCmd->rightPlaneInterpolationR1Register.bitfields.LUT_P3  = pData->r_lut_p_r[3];
        pRegCmd->rightPlaneInterpolationR2Register.bitfields.LUT_P4  = pData->r_lut_p_r[4];
        pRegCmd->rightPlaneInterpolationR2Register.bitfields.LUT_P5  = pData->r_lut_p_r[5];
        pRegCmd->rightPlaneInterpolationR3Register.bitfields.LUT_P6  = pData->r_lut_p_r[6];
        pRegCmd->rightPlaneInterpolationR3Register.bitfields.LUT_P7  = pData->r_lut_p_r[7];

        pRegCmd->rightPlaneInterpolationGB0Register.bitfields.LUT_P0 = pData->gb_lut_p_r[0];
        pRegCmd->rightPlaneInterpolationGB0Register.bitfields.LUT_P1 = pData->gb_lut_p_r[1];
        pRegCmd->rightPlaneInterpolationGB1Register.bitfields.LUT_P2 = pData->gb_lut_p_r[2];
        pRegCmd->rightPlaneInterpolationGB1Register.bitfields.LUT_P3 = pData->gb_lut_p_r[3];
        pRegCmd->rightPlaneInterpolationGB2Register.bitfields.LUT_P4 = pData->gb_lut_p_r[4];
        pRegCmd->rightPlaneInterpolationGB2Register.bitfields.LUT_P5 = pData->gb_lut_p_r[5];
        pRegCmd->rightPlaneInterpolationGB3Register.bitfields.LUT_P6 = pData->gb_lut_p_r[6];
        pRegCmd->rightPlaneInterpolationGB3Register.bitfields.LUT_P7 = pData->gb_lut_p_r[7];

        pRegCmd->rightPlaneInterpolationGR0Register.bitfields.LUT_P0 = pData->gr_lut_p_r[0];
        pRegCmd->rightPlaneInterpolationGR0Register.bitfields.LUT_P1 = pData->gr_lut_p_r[1];
        pRegCmd->rightPlaneInterpolationGR1Register.bitfields.LUT_P2 = pData->gr_lut_p_r[2];
        pRegCmd->rightPlaneInterpolationGR1Register.bitfields.LUT_P3 = pData->gr_lut_p_r[3];
        pRegCmd->rightPlaneInterpolationGR2Register.bitfields.LUT_P4 = pData->gr_lut_p_r[4];
        pRegCmd->rightPlaneInterpolationGR2Register.bitfields.LUT_P5 = pData->gr_lut_p_r[5];
        pRegCmd->rightPlaneInterpolationGR3Register.bitfields.LUT_P6 = pData->gr_lut_p_r[6];
        pRegCmd->rightPlaneInterpolationGR3Register.bitfields.LUT_P7 = pData->gr_lut_p_r[7];
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input params are NULL pData=0x%p pOutput=0x%p", pData, pOutput);
    }

    return;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFEBPCBCC50CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IFEBPCBCC50CalculateSetting(
    const BPCBCC50InputData*  pInput,
    VOID*                     pOEMIQData,
    BPCBCC50OutputData*       pOutput)
{
    CamxResult                           result             = CamxResultSuccess;
    BOOL                                 commonIQResult     = FALSE;
    bpcbcc_5_0_0::bpcbcc50_rgn_dataType* pInterpolationData = NULL;
    OEMIFEIQSetting*                     pOEMInput          = NULL;
    globalelements::enable_flag_type     moduleEnable       = 0;
    BPCBCC50UnpackedField                unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<bpcbcc_5_0_0::bpcbcc50_rgn_dataType*>(pInput->pInterpolationData);
            // Call the Interpolation Calculation
            commonIQResult = IQInterface::s_interpolationTable.IFEBPCBCC50Interpolation(pInput, pInterpolationData);
            moduleEnable       = pInput->pChromatix->enable_section.bpcbcc_enable;
        }
        else
        {
            pOEMInput    = static_cast<OEMIFEIQSetting*>(pOEMIQData);
            moduleEnable = static_cast<globalelements::enable_flag_type>(pOEMInput->BPCBCCEnable);

            // Use OEM defined interpolation data
            pInterpolationData =
                reinterpret_cast<bpcbcc_5_0_0::bpcbcc50_rgn_dataType*>(&(pOEMInput->BPCBCCSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            commonIQResult = IQInterface::s_interpolationTable.IFEBPCBCC50CalculateHWSetting(
                pInput, pInterpolationData, moduleEnable, static_cast<VOID*>(&unpackedRegData));

            if (TRUE == commonIQResult)
            {
                IFESetupBPCBCC50Reg(&unpackedRegData, pOutput->pRegCmd);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("BPCBCC interpolation failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("BPCBCC interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::CopyLSCMapData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IQInterface::CopyLSCMapData(
    const ISPInputData*    pInputData,
    LSC34UnpackedField*    pUnpackedField)
{
    UINT16 bankS        = pUnpackedField->bank_sel;
    UINT32 k            = 0;
    UINT32 totalHorMesh = 0;
    UINT32 toatlVerMesh = 0;
    UINT32 dmiCount     = 0;


    // (NUM_MESHGAIN_H+2) * (NUM_MESHGAIN_V+2) LUT entries for a frame.
    totalHorMesh = pUnpackedField->num_meshgain_h + 2;
    toatlVerMesh = pUnpackedField->num_meshgain_v + 2;

    for (UINT16 i = 0; i < toatlVerMesh; i++)
    {
        for (UINT16 j = 0; j < totalHorMesh; j++)
        {
            // 0->R
            pInputData->pCalculatedData->lensShadingInfo.lensShadingMap[k++]     =
                ((static_cast<FLOAT> (pUnpackedField->mesh_table_l[bankS][0][i][j])) / (1 << 10));

            if (pInputData->sensorData.format == PixelFormat::BayerBGGR)
            {
                // 1->Gr
                pInputData->pCalculatedData->lensShadingInfo.lensShadingMap[k++] =
                    ((static_cast<FLOAT> (pUnpackedField->mesh_table_l[bankS][1][i][j])) / (1 << 10));
                pInputData->pCalculatedData->lensShadingInfo.lensShadingMap[k++] =
                    ((static_cast<FLOAT> (pUnpackedField->mesh_table_l[bankS][2][i][j])) / (1 << 10));
            }
            else
            {
                // 2->Gb
                pInputData->pCalculatedData->lensShadingInfo.lensShadingMap[k++] =
                    ((static_cast<FLOAT> (pUnpackedField->mesh_table_l[bankS][2][i][j])) / (1 << 10));
                // 1->Gr
                pInputData->pCalculatedData->lensShadingInfo.lensShadingMap[k++] =
                    ((static_cast<FLOAT> (pUnpackedField->mesh_table_l[bankS][1][i][j])) / (1 << 10));
            }

            // 3->B
            pInputData->pCalculatedData->lensShadingInfo.lensShadingMap[k++]     =
                ((static_cast<FLOAT> (pUnpackedField->mesh_table_l[bankS][3][i][j])) / (1 << 10));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFESetupLSC34Reg
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IQInterface::IFESetupLSC34Reg(
    const ISPInputData*        pInputData,
    const LSC34UnpackedField*  pData,
    LSC34OutputData*           pOutput)
{
    IFELSC34RegCmd*  pRegCmd      = NULL;
    UINT32           totalHorMesh = 0;
    UINT32           toatlVerMesh = 0;
    UINT32           dmiCount     = 0;
    UINT32           horMeshNum;
    UINT32           verMeshNum;

    if ((NULL != pInputData) && (NULL != pOutput) && (NULL != pData) && (NULL != pOutput->IFE.pRegIFECmd))
    {
        pRegCmd = pOutput->IFE.pRegIFECmd;

        pRegCmd->configRegister.bitfields.NUM_MESHGAIN_H               = pData->num_meshgain_h;
        pRegCmd->configRegister.bitfields.NUM_MESHGAIN_V               = pData->num_meshgain_v;
        pRegCmd->configRegister.bitfields.PCA_LUT_BANK_SEL             = pData->bank_sel;
        pRegCmd->configRegister.bitfields.PIXEL_OFFSET                 = pData->pixel_offset;

        pRegCmd->gridConfigRegister0.bitfields.BLOCK_HEIGHT            = pData->meshGridBheight_l;
        pRegCmd->gridConfigRegister0.bitfields.BLOCK_WIDTH             = pData->meshGridBwidth_l;

        pRegCmd->gridConfigRegister1.bitfields.INTERP_FACTOR           = pData->intp_factor_l;
        pRegCmd->gridConfigRegister1.bitfields.SUB_GRID_HEIGHT         = pData->bheight_l;
        pRegCmd->gridConfigRegister1.bitfields.SUB_GRID_Y_DELTA        = pData->y_delta_l;

        pRegCmd->gridConfigRegister2.bitfields.SUB_GRID_WIDTH          = pData->bwidth_l;
        pRegCmd->gridConfigRegister2.bitfields.SUB_GRID_X_DELTA        = pData->x_delta_l;

        pRegCmd->rightGridConfigRegister0.bitfields.BLOCK_HEIGHT       = pData->meshGridBheight_r;
        pRegCmd->rightGridConfigRegister0.bitfields.BLOCK_WIDTH        = pData->meshGridBwidth_r;

        pRegCmd->rightGridConfigRegister1.bitfields.INTERP_FACTOR      = pData->intp_factor_r;
        pRegCmd->rightGridConfigRegister1.bitfields.SUB_GRID_HEIGHT    = pData->bheight_r;
        pRegCmd->rightGridConfigRegister1.bitfields.SUB_GRID_Y_DELTA   = pData->y_delta_r;

        pRegCmd->rightGridConfigRegister2.bitfields.SUB_GRID_WIDTH     = pData->bwidth_r;
        pRegCmd->rightGridConfigRegister2.bitfields.SUB_GRID_X_DELTA   = pData->x_delta_r;

        pRegCmd->rightStripeConfigRegister0.bitfields.BLOCK_X_INDEX    = pData->lx_start_r;
        pRegCmd->rightStripeConfigRegister0.bitfields.BLOCK_Y_INDEX    = pData->ly_start_r;
        pRegCmd->rightStripeConfigRegister0.bitfields.Y_DELTA_ACCUM    = pData->y_delta_r * pData->by_e1_r;

        pRegCmd->rightStripeConfigRegister1.bitfields.PIXEL_X_INDEX    = pData->bx_d1_r;
        pRegCmd->rightStripeConfigRegister1.bitfields.PIXEL_Y_INDEX    = pData->by_e1_r;
        pRegCmd->rightStripeConfigRegister1.bitfields.SUB_GRID_X_INDEX = pData->bx_start_r;
        pRegCmd->rightStripeConfigRegister1.bitfields.SUB_GRID_Y_INDEX = pData->by_start_r;

        pRegCmd->stripeConfigRegister0.bitfields.BLOCK_X_INDEX         = pData->lx_start_l;
        pRegCmd->stripeConfigRegister0.bitfields.BLOCK_Y_INDEX         = pData->ly_start_l;
        pRegCmd->stripeConfigRegister0.bitfields.Y_DELTA_ACCUM         = pData->y_delta_l * pData->by_e1_l;

        pRegCmd->stripeConfigRegister1.bitfields.PIXEL_X_INDEX         = pData->bx_d1_l;
        pRegCmd->stripeConfigRegister1.bitfields.PIXEL_Y_INDEX         = pData->by_e1_l;
        pRegCmd->stripeConfigRegister1.bitfields.SUB_GRID_X_INDEX      = pData->bx_start_l;
        pRegCmd->stripeConfigRegister1.bitfields.SUB_GRID_Y_INDEX      = pData->by_start_l;

        // (NUM_MESHGAIN_H+2) * (NUM_MESHGAIN_V+2) LUT entries for a frame.
        totalHorMesh = pData->num_meshgain_h + 2;
        toatlVerMesh = pData->num_meshgain_v + 2;

        for (verMeshNum = 0; verMeshNum < toatlVerMesh; verMeshNum++)
        {
            for (horMeshNum = 0; horMeshNum < totalHorMesh; horMeshNum++)
            {
                pOutput->pGRRLUTDMIBuffer[dmiCount] =
                    ((pData->mesh_table_l[pData->bank_sel][0][verMeshNum][horMeshNum] & Utils::AllOnes32(13)) |
                    ((pData->mesh_table_l[pData->bank_sel][1][verMeshNum][horMeshNum]) << 13));

                pOutput->pGBBLUTDMIBuffer[dmiCount] =
                    ((pData->mesh_table_l[pData->bank_sel][3][verMeshNum][horMeshNum] & Utils::AllOnes32(13)) |
                    ((pData->mesh_table_l[pData->bank_sel][2][verMeshNum][horMeshNum]) << 13));
                dmiCount++;
            }
        }

        if (StatisticsLensShadingMapModeOn == pInputData->pHALTagsData->statisticsLensShadingMapMode)
        {
            CopyLSCMapData(pInputData, pOutput->pUnpackedField);
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid pointer: pData: %p, pInputData: %p, pOutput: %p", pData, pInputData, pOutput);

        if (NULL != pOutput)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid pointer: pOutput->IFE.pRegIFECmd: %p", pOutput->IFE.pRegIFECmd);
        }
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupLSC34Reg
///
/// @brief  Setup BPS LSC34 Register based on common library output
///
/// @param  pInput  Pointer to the input data
/// @param  pData   Pointer to the unpacked output data
/// @param  pRegCmd Pointer to the register output
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupLSC34Reg(
    LSC34UnpackedField*    pData,
    LSC34OutputData*       pOutput)
{
    UINT32 totalHorMesh = 0;
    UINT32 toatlVerMesh = 0;
    UINT32 dmiCount = 0;
    UINT32 horMeshNum;
    UINT32 verMeshNum;

    if ((NULL != pOutput) && (NULL != pData) && (NULL != pOutput->BPS.pRegBPSCmd))
    {
        pOutput->BPS.pModuleConfig->moduleCfg.EN            = pData->enable;
        pOutput->BPS.pModuleConfig->moduleCfg.NUM_SUBBLOCKS = pData->intp_factor_l;
        pOutput->BPS.pModuleConfig->subgridWidth            = pData->bwidth_l;
        pOutput->BPS.pModuleConfig->meshGridBwidth          = pData->meshGridBwidth_l;
        pOutput->BPS.pModuleConfig->startBlockIndex         = pData->lx_start_l;
        pOutput->BPS.pModuleConfig->startSubgridIndex       = pData->bx_start_l;
        pOutput->BPS.pModuleConfig->topLeftCoordinate       = pData->bx_d1_l;
        pOutput->BPS.pModuleConfig->numHorizontalMeshGains  = pData->num_meshgain_h;
        pOutput->BPS.pModuleConfig->initBlockY              = pData->ly_start_l;
        pOutput->BPS.pModuleConfig->initSubblockY           = pData->by_start_l;
        pOutput->BPS.pModuleConfig->initPixelY              = pData->by_e1_l;

        pOutput->BPS.pRegBPSCmd->config0.bitfields.NUM_BLOCKS_X        = pData->num_meshgain_h;
        pOutput->BPS.pRegBPSCmd->config0.bitfields.NUM_BLOCKS_Y        = pData->num_meshgain_v;
        pOutput->BPS.pRegBPSCmd->config1.bitfields.BLOCK_HEIGHT        = pData->meshGridBheight_l;
        pOutput->BPS.pRegBPSCmd->config1.bitfields.BLOCK_WIDTH         = pData->meshGridBwidth_l;
        pOutput->BPS.pRegBPSCmd->config2.bitfields.INV_SUBBLOCK_WIDTH  = pData->x_delta_l;
        pOutput->BPS.pRegBPSCmd->config2.bitfields.SUBBLOCK_WIDTH      = pData->bwidth_l;
        pOutput->BPS.pRegBPSCmd->config3.bitfields.INV_SUBBLOCK_HEIGHT = pData->y_delta_l;
        pOutput->BPS.pRegBPSCmd->config3.bitfields.SUBBLOCK_HEIGHT     = pData->bheight_l;
        pOutput->BPS.pRegBPSCmd->config4.bitfields.INIT_PIXEL_X        = pData->bx_d1_l;
        pOutput->BPS.pRegBPSCmd->config4.bitfields.INIT_PIXEL_Y        = pData->by_e1_l;
        pOutput->BPS.pRegBPSCmd->config5.bitfields.PIXEL_OFFSET        = pData->pixel_offset;
        pOutput->BPS.pRegBPSCmd->config6.bitfields.INIT_YDELTA         = pData->by_init_e1_l;


        // (NUM_MESHGAIN_H+2) * (NUM_MESHGAIN_V+2) LUT entries for a frame.
        totalHorMesh = pData->num_meshgain_h + 2;
        toatlVerMesh = pData->num_meshgain_v + 2;

        for (verMeshNum = 0; verMeshNum < toatlVerMesh; verMeshNum++)
        {
            for (horMeshNum = 0; horMeshNum < totalHorMesh; horMeshNum++)
            {
                pOutput->pGRRLUTDMIBuffer[dmiCount] =
                    ((pData->mesh_table_l[pData->bank_sel][0][verMeshNum][horMeshNum] & Utils::AllOnes32(13)) |
                    ((pData->mesh_table_l[pData->bank_sel][1][verMeshNum][horMeshNum]) << 13));

                pOutput->pGBBLUTDMIBuffer[dmiCount] =
                    ((pData->mesh_table_l[pData->bank_sel][3][verMeshNum][horMeshNum] & Utils::AllOnes32(13)) |
                    ((pData->mesh_table_l[pData->bank_sel][2][verMeshNum][horMeshNum]) << 13));
                dmiCount++;
            }
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL ");
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::LSC34CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::LSC34CalculateSetting(
    LSC34InputData*              pInput,
    VOID*                        pOEMIQData,
    const ISPInputData*          pInputData,
    LSC34OutputData*             pOutput)
{
    CamxResult                                            result             = CamxResultSuccess;
    BOOL                                                  commonIQResult     = FALSE;
    lsc_3_4_0::lsc34_rgn_dataType*                        pInterpolationData = NULL;
    tintless_2_0_0::tintless20_rgn_dataType               tintlessInterpolationData;
    lsc_3_4_0::chromatix_lsc34Type::enable_sectionStruct* pModuleEnable      = NULL;
    UINT32 cameraID;
    UINT32 sensorAR;

    if ((NULL != pInput) && (NULL != pOutput) && ((NULL != pInput->pChromatix) ||(NULL != pOEMIQData)) && (NULL != pInputData))
    {
        cameraID  = pInputData->sensorID;
        sensorAR  = pInputData->sensorData.sensorAspectRatioMode;
        pInterpolationData = static_cast<lsc_3_4_0::lsc34_rgn_dataType*>(pInput->pInterpolationData);

        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            if (NULL != pInterpolationData)
            {
                commonIQResult = IQInterface::s_interpolationTable.LSC34Interpolation(pInput, pInterpolationData);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid InterpolationData pointer");
            }

            pModuleEnable      = &(pInput->pChromatix->enable_section);

            if (NULL != pInput->pTintlessConfig &&
                NULL != pInput->pTintlessStats)
            {
                commonIQResult = IQInterface::s_interpolationTable.TINTLESS20Interpolation(pInput, &tintlessInterpolationData);
            }
        }
        else
        {
            if (PipelineType::IFE == pOutput->type)
            {
                OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);
                pModuleEnable              = reinterpret_cast<lsc_3_4_0::chromatix_lsc34Type::enable_sectionStruct*>(
                                                &(pOEMInput->LSCEnableSection));

                if (NULL != pInterpolationData)
                {
                    // Copy OEM defined interpolation data
                    Utils::Memcpy(pInterpolationData, &pOEMInput->LSCSetting, sizeof(lsc_3_4_0::lsc34_rgn_dataType));
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid InterpolationData pointer");
                }
            }
            else if (PipelineType::BPS == pOutput->type)
            {
                OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);
                pModuleEnable              = reinterpret_cast<lsc_3_4_0::chromatix_lsc34Type::enable_sectionStruct*>(
                                                &(pOEMInput->LSCEnableSection));

                if (NULL != pInterpolationData)
                {
                    // Copy OEM defined interpolation data
                    Utils::Memcpy(pInterpolationData, &pOEMInput->LSCSetting, sizeof(lsc_3_4_0::lsc34_rgn_dataType));
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid InterpolationData pointer");
                }
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for Demux13", pOutput->type);
            }

            if (NULL != pModuleEnable)
            {
                commonIQResult = TRUE;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            if (PipelineType::IFE == pOutput->type)
            {
                pOutput->pUnpackedField->bank_sel = pInput->bankSelect;
                commonIQResult = IQInterface::s_interpolationTable.LSC34CalculateHWSetting(
                    pInput,
                    pInterpolationData,
                    pModuleEnable,
                    &tintlessInterpolationData,
                    static_cast<VOID*>(pOutput->pUnpackedField));

                if (TRUE == commonIQResult)
                {
                    if (NULL != pInput->pTintlessStats)
                    {
                        memset(s_prevTintlessOutput[cameraID][sensorAR].mesh_table_l, 0,
                              sizeof(s_prevTintlessOutput[cameraID][sensorAR].mesh_table_l));
                        memset(s_prevTintlessOutput[cameraID][sensorAR].mesh_table_r, 0,
                               sizeof(s_prevTintlessOutput[cameraID][sensorAR].mesh_table_r));
                        memcpy(s_prevTintlessOutput[cameraID][sensorAR].mesh_table_l, pOutput->pUnpackedField->mesh_table_l,
                        sizeof(s_prevTintlessOutput[cameraID][sensorAR].mesh_table_l));
                        memcpy(s_prevTintlessOutput[cameraID][sensorAR].mesh_table_r, pOutput->pUnpackedField->mesh_table_r,
                        sizeof(s_prevTintlessOutput[cameraID][sensorAR].mesh_table_r));
                        s_prevTintlessTableValid[cameraID][sensorAR] = TRUE;
                    }
                    if (TRUE == pInput->fetchSettingOnly)
                    {
                        pOutput->lscState.bwidth_l         = static_cast<uint16_t>(pOutput->pUnpackedField->bwidth_l);
                        pOutput->lscState.bx_d1_l          = static_cast<uint16_t>(pOutput->pUnpackedField->bx_d1_l);
                        pOutput->lscState.bx_start_l       = static_cast<uint16_t>(pOutput->pUnpackedField->bx_start_l);
                        pOutput->lscState.lx_start_l       = static_cast<uint16_t>(pOutput->pUnpackedField->lx_start_l);
                        pOutput->lscState.meshGridBwidth_l =
                            static_cast<uint16_t>(pOutput->pUnpackedField->meshGridBwidth_l);
                        pOutput->lscState.num_meshgain_h   =
                            static_cast<uint16_t>(pOutput->pUnpackedField->num_meshgain_h);
                    }
                    else
                    {
                        IFESetupLSC34Reg(pInputData, pOutput->pUnpackedField, pOutput);
                    }
                }
                else
                {
                    result = CamxResultEFailed;
                    CAMX_ASSERT_ALWAYS_MESSAGE("LSC CalculateHWSetting failed.");
                }

            }
            else if (PipelineType::BPS == pOutput->type)
            {
                pOutput->pUnpackedField->bank_sel = pInput->bankSelect;
                commonIQResult           = IQInterface::s_interpolationTable.LSC34CalculateHWSetting(
                    pInput,
                    pInterpolationData,
                    pModuleEnable,
                    &tintlessInterpolationData,
                    static_cast<VOID*>(pOutput->pUnpackedField));
                if (TRUE == commonIQResult)
                {
                    if (TRUE == s_prevTintlessTableValid[cameraID][sensorAR] && NULL == pInput->pTintlessStats)
                    {
                        memcpy(pOutput->pUnpackedField->mesh_table_l, s_prevTintlessOutput[cameraID][sensorAR].mesh_table_l,
                        sizeof(pOutput->pUnpackedField->mesh_table_l));
                        memcpy(pOutput->pUnpackedField->mesh_table_r, s_prevTintlessOutput[cameraID][sensorAR].mesh_table_r,
                        sizeof(pOutput->pUnpackedField->mesh_table_r));
                        CAMX_LOG_INFO(CamxLogGroupISP, " UsePrevious Tintless camera %d sensorAR %d", cameraID, sensorAR);
                    }
                    BPSSetupLSC34Reg(pOutput->pUnpackedField, pOutput);
                }
                else
                {
                    result = CamxResultEFailed;
                    CAMX_ASSERT_ALWAYS_MESSAGE("LSC CalculateHWSetting failed.");
                }

            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("invalid Pipeline");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("LSC intepolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL ");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupCC12Reg
///
/// @brief  Setup CC12 Register based on common library output
///
/// @param  pData    Pointer to the Common Library Calculation Result
/// @param  pRegCmd  Pointer to the CC12 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupCC12Reg(
    const CC12UnpackedField*  pData,
    IFECC12RegCmd*            pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->coefficientRegister0.bitfields.CN            = pData->c0_l;
        pRegCmd->coefficientRegister0.bitfields.RIGHT_CN      = pData->c0_r;
        pRegCmd->coefficientRegister1.bitfields.CN            = pData->c1_l;
        pRegCmd->coefficientRegister1.bitfields.RIGHT_CN      = pData->c1_r;
        pRegCmd->coefficientRegister2.bitfields.CN            = pData->c2_l;
        pRegCmd->coefficientRegister2.bitfields.RIGHT_CN      = pData->c2_r;
        pRegCmd->coefficientRegister3.bitfields.CN            = pData->c3_l;
        pRegCmd->coefficientRegister3.bitfields.RIGHT_CN      = pData->c3_r;
        pRegCmd->coefficientRegister4.bitfields.CN            = pData->c4_l;
        pRegCmd->coefficientRegister4.bitfields.RIGHT_CN      = pData->c4_r;
        pRegCmd->coefficientRegister5.bitfields.CN            = pData->c5_l;
        pRegCmd->coefficientRegister5.bitfields.RIGHT_CN      = pData->c5_r;
        pRegCmd->coefficientRegister6.bitfields.CN            = pData->c6_l;
        pRegCmd->coefficientRegister6.bitfields.RIGHT_CN      = pData->c6_r;
        pRegCmd->coefficientRegister7.bitfields.CN            = pData->c7_l;
        pRegCmd->coefficientRegister7.bitfields.RIGHT_CN      = pData->c7_r;
        pRegCmd->coefficientRegister8.bitfields.CN            = pData->c8_l;
        pRegCmd->coefficientRegister8.bitfields.RIGHT_CN      = pData->c8_r;
        pRegCmd->offsetRegister0.bitfields.KN                 = pData->k0_l;
        pRegCmd->offsetRegister0.bitfields.RIGHT_KN           = pData->k0_r;
        pRegCmd->offsetRegister1.bitfields.KN                 = pData->k1_l;
        pRegCmd->offsetRegister1.bitfields.RIGHT_KN           = pData->k1_r;
        pRegCmd->offsetRegister2.bitfields.KN                 = pData->k2_l;
        pRegCmd->offsetRegister2.bitfields.RIGHT_KN           = pData->k2_r;
        pRegCmd->coefficientQRegister.bitfields.QFACTOR       = pData->qfactor_l;
        pRegCmd->coefficientQRegister.bitfields.RIGHT_QFACTOR = pData->qfactor_r;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pRegCmd %p ", pData, pRegCmd);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFECC12CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IFECC12CalculateSetting(
    const CC12InputData*  pInput,
    VOID*                 pOEMIQData,
    CC12OutputData*       pOutput)
{
    CamxResult                            result             = CamxResultSuccess;
    BOOL                                  commonIQResult     = FALSE;
    cc_1_2_0::cc12_rgn_dataType*          pInterpolationData = NULL;
    OEMIFEIQSetting*                      pOEMInput          = NULL;
    cc_1_2_0::chromatix_cc12_reserveType* pReserveType       = NULL;
    CC12UnpackedField                     unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        Utils::Memset(&unpackedRegData, 0, sizeof(CC12UnpackedField));

        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<cc_1_2_0::cc12_rgn_dataType*>(pInput->pInterpolationData);
            // Call the Interpolation Calculation
            commonIQResult     = IQInterface::s_interpolationTable.IFECC12Interpolation(pInput, pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_cc12_reserve);
        }
        else
        {
            pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);
            // Use OEM defined interpolation data
            pInterpolationData =
                reinterpret_cast<cc_1_2_0::cc12_rgn_dataType*>(&(pOEMInput->CCSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveType   = reinterpret_cast<cc_1_2_0::chromatix_cc12_reserveType*>(&(pOEMInput->CCReserveType));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            commonIQResult = IQInterface::s_interpolationTable.IFECC12CalculateHWSetting(
                pInput, pInterpolationData, pReserveType, static_cast<VOID*>(&unpackedRegData));
            if (TRUE == commonIQResult)
            {
                IFESetupCC12Reg(&unpackedRegData, pOutput->pRegCmd);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("CC1_2 Calculate HW setting failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("CC1_2 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFESetupCST12Reg
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupCST12Reg(
    const CST12UnpackedField*   pData,
    IFECST12RegCmd*             pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->ch0Coefficient0.bitfields.MATRIX_M00       = pData->m00;
        pRegCmd->ch0Coefficient0.bitfields.MATRIX_M01       = pData->m01;
        pRegCmd->ch0Coefficient1.bitfields.MATRIX_M02       = pData->m02;
        pRegCmd->ch0OffsetConfig.bitfields.OFFSET_O0        = pData->o0;
        pRegCmd->ch0OffsetConfig.bitfields.OFFSET_S0        = pData->s0;
        pRegCmd->ch0ClampConfig.bitfields.CLAMP_MIN         = pData->c00;
        pRegCmd->ch0ClampConfig.bitfields.CLAMP_MAX         = pData->c01;
        pRegCmd->ch1CoefficientConfig0.bitfields.MATRIX_M10 = pData->m10;
        pRegCmd->ch1CoefficientConfig0.bitfields.MATRIX_M11 = pData->m11;
        pRegCmd->ch1CoefficientConfig1.bitfields.MATRIX_M12 = pData->m12;
        pRegCmd->ch1OffsetConfig.bitfields.OFFSET_O1        = pData->o1;
        pRegCmd->ch1OffsetConfig.bitfields.OFFSET_S1        = pData->s1;
        pRegCmd->ch1ClampConfig.bitfields.CLAMP_MAX         = pData->c11;
        pRegCmd->ch1ClampConfig.bitfields.CLAMP_MIN         = pData->c10;
        pRegCmd->ch2CoefficientConfig0.bitfields.MATRIX_M20 = pData->m20;
        pRegCmd->ch2CoefficientConfig0.bitfields.MATRIX_M21 = pData->m21;
        pRegCmd->ch2CoefficientConfig1.bitfields.MATRIX_M22 = pData->m22;
        pRegCmd->ch2OffsetConfig.bitfields.OFFSET_O2        = pData->o2;
        pRegCmd->ch2OffsetConfig.bitfields.OFFSET_S2        = pData->s2;
        pRegCmd->ch2ClampConfig.bitfields.CLAMP_MAX         = pData->c21;
        pRegCmd->ch2ClampConfig.bitfields.CLAMP_MIN         = pData->c20;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pRegCmd %p ", pData, pRegCmd);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::BPSSetupCST12Reg
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupCST12Reg(
    const CST12UnpackedField* pData,
    BPSCST12RegCmd*           pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->channel0CoefficientConfig0.bitfields.MATRIX_M00 = pData->m00;
        pRegCmd->channel0CoefficientConfig0.bitfields.MATRIX_M01 = pData->m01;
        pRegCmd->channel0CoefficientConfig1.bitfields.MATRIX_M02 = pData->m02;
        pRegCmd->channel0OffsetConfig.bitfields.OFFSET_O0        = pData->o0;
        pRegCmd->channel0OffsetConfig.bitfields.OFFSET_S0        = pData->s0;
        pRegCmd->channel0ClampConfig.bitfields.CLAMP_MIN         = pData->c00;
        pRegCmd->channel0ClampConfig.bitfields.CLAMP_MAX         = pData->c01;
        pRegCmd->channel1CoefficientConfig0.bitfields.MATRIX_M10 = pData->m10;
        pRegCmd->channel1CoefficientConfig0.bitfields.MATRIX_M11 = pData->m11;
        pRegCmd->channel1CoefficientConfig1.bitfields.MATRIX_M12 = pData->m12;
        pRegCmd->channel1OffsetConfig.bitfields.OFFSET_O1        = pData->o1;
        pRegCmd->channel1OffsetConfig.bitfields.OFFSET_S1        = pData->s1;
        pRegCmd->channel1ClampConfig.bitfields.CLAMP_MAX         = pData->c11;
        pRegCmd->channel1ClampConfig.bitfields.CLAMP_MIN         = pData->c10;
        pRegCmd->channel2CoefficientConfig0.bitfields.MATRIX_M20 = pData->m20;
        pRegCmd->channel2CoefficientConfig0.bitfields.MATRIX_M21 = pData->m21;
        pRegCmd->channel2CoefficientConfig1.bitfields.MATRIX_M22 = pData->m22;
        pRegCmd->channel2OffsetConfig.bitfields.OFFSET_O2        = pData->o2;
        pRegCmd->channel2OffsetConfig.bitfields.OFFSET_S2        = pData->s2;
        pRegCmd->channel2ClampConfig.bitfields.CLAMP_MAX         = pData->c21;
        pRegCmd->channel2ClampConfig.bitfields.CLAMP_MIN         = pData->c20;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pRegCmd %p", pData, pRegCmd);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IPESetupCST12Reg
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupCST12Reg(
    const CST12UnpackedField* pData,
    IPEColorTransformRegCmd*  pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->configChannel0.coefficient0.bitfields.MATRIX_M00 = pData->m00;
        pRegCmd->configChannel0.coefficient0.bitfields.MATRIX_M01 = pData->m01;
        pRegCmd->configChannel0.coefficient1.bitfields.MATRIX_M02 = pData->m02;
        pRegCmd->configChannel0.offset.bitfields.OFFSET_O0        = pData->o0;
        pRegCmd->configChannel0.offset.bitfields.OFFSET_S0        = pData->s0;
        pRegCmd->configChannel0.clamp.bitfields.CLAMP_MIN         = pData->c00;
        pRegCmd->configChannel0.clamp.bitfields.CLAMP_MAX         = pData->c01;

        pRegCmd->configChannel1.coefficient0.bitfields.MATRIX_M10 = pData->m10;
        pRegCmd->configChannel1.coefficient0.bitfields.MATRIX_M11 = pData->m11;
        pRegCmd->configChannel1.coefficient1.bitfields.MATRIX_M12 = pData->m12;
        pRegCmd->configChannel1.offset.bitfields.OFFSET_O1        = pData->o1;
        pRegCmd->configChannel1.offset.bitfields.OFFSET_S1        = pData->s1;
        pRegCmd->configChannel1.clamp.bitfields.CLAMP_MIN         = pData->c10;
        pRegCmd->configChannel1.clamp.bitfields.CLAMP_MAX         = pData->c11;

        pRegCmd->configChannel2.coefficient0.bitfields.MATRIX_M20 = pData->m20;
        pRegCmd->configChannel2.coefficient0.bitfields.MATRIX_M21 = pData->m21;
        pRegCmd->configChannel2.coefficient1.bitfields.MATRIX_M22 = pData->m22;
        pRegCmd->configChannel2.offset.bitfields.OFFSET_O2        = pData->o2;
        pRegCmd->configChannel2.offset.bitfields.OFFSET_S2        = pData->s2;
        pRegCmd->configChannel2.clamp.bitfields.CLAMP_MIN         = pData->c20;
        pRegCmd->configChannel2.clamp.bitfields.CLAMP_MAX         = pData->c21;

    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pRegCmd %p", pData, pRegCmd);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::CST12CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::CST12CalculateSetting(
    const CST12InputData* pInput,
    VOID*                 pOEMIQData,
    CST12OutputData*      pOutput)
{
    CamxResult                                            result         = CamxResultSuccess;
    BOOL                                                  commonIQResult = FALSE;
    cst_1_2_0::chromatix_cst12_reserveType*               pReserveType   = NULL;
    cst_1_2_0::chromatix_cst12Type::enable_sectionStruct* pModuleEnable  = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            pReserveType  = &(pInput->pChromatix->chromatix_cst12_reserve);
            pModuleEnable = &(pInput->pChromatix->enable_section);
        }
        else
        {
            if (PipelineType::IFE == pOutput->type)
            {
                OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pReserveType               = reinterpret_cast<cst_1_2_0::chromatix_cst12_reserveType*>(
                                                &(pOEMInput->CSTSetting));
                pModuleEnable              = reinterpret_cast<cst_1_2_0::chromatix_cst12Type::enable_sectionStruct*>(
                                                &(pOEMInput->CSTEnableSection));
            }
            else if (PipelineType::BPS == pOutput->type)
            {
                OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pReserveType               = reinterpret_cast<cst_1_2_0::chromatix_cst12_reserveType*>(
                                                &(pOEMInput->CSTSetting));
                pModuleEnable              = reinterpret_cast<cst_1_2_0::chromatix_cst12Type::enable_sectionStruct*>(
                                                &(pOEMInput->CSTEnableSection));
            }
            else if (PipelineType::IPE == pOutput->type)
            {
                OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pReserveType               = reinterpret_cast<cst_1_2_0::chromatix_cst12_reserveType*>(
                    &(pOEMInput->CSTSetting));
                pModuleEnable              = reinterpret_cast<cst_1_2_0::chromatix_cst12Type::enable_sectionStruct*>(
                    &(pOEMInput->CSTEnableSection));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for Demux13", pOutput->type);
            }
        }

        if (NULL != pReserveType)
        {
            commonIQResult = TRUE;
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
        }

        if (TRUE == commonIQResult)
        {
            CST12UnpackedField unpackedRegData;

            commonIQResult =
                IQInterface::s_interpolationTable.CST12CalculateHWSetting(pInput,
                                                                          pReserveType,
                                                                          pModuleEnable,
                                                                          static_cast<VOID*>(&unpackedRegData));

            if (TRUE == commonIQResult)
            {
                if (PipelineType::IFE == pOutput->type)
                {
                    IFESetupCST12Reg(&unpackedRegData, pOutput->regCommand.pRegIFECmd);
                }
                else if (PipelineType::BPS == pOutput->type)
                {
                    BPSSetupCST12Reg(&unpackedRegData, pOutput->regCommand.pRegBPSCmd);
                }
                else if (PipelineType::IPE == pOutput->type)
                {
                    IPESetupCST12Reg(&unpackedRegData, pOutput->regCommand.pRegIPECmd);
                }
                else
                {
                    result = CamxResultEInvalidArg;
                    CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Pipeline");
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("CST12 HW calculate Settings failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("CST12 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFELinearization33CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IFELinearization33CalculateSetting(
    const Linearization33InputData*  pInput,
    VOID*                            pOEMIQData,
    Linearization33OutputData*       pOutput)
{
    CamxResult                                         result             = CamxResultSuccess;
    BOOL                                               commonIQResult     = FALSE;
    linearization_3_3_0::linearization33_rgn_dataType* pInterpolationData = NULL;
    OEMIFEIQSetting*                                   pOEMInput          = NULL;
    Linearization33UnpackedField                       unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<linearization_3_3_0::linearization33_rgn_dataType*>(pInput->pInterpolationData);

            // Call the Interpolation Calculation
            commonIQResult     =
                IQInterface::s_interpolationTable.IFELinearization33Interpolation(pInput, pInterpolationData);
        }
        else
        {
            pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);
            // Use OEM defined interpolation data
            pInterpolationData =
                reinterpret_cast<linearization_3_3_0::linearization33_rgn_dataType*>(&(pOEMInput->LinearizationSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            // Calculate the remaining Gain to apply in Demux
            if (FALSE == pInput->pedestalEnable)
            {
                FLOAT maxLUTValue = static_cast<FLOAT>(Linearization33MaximumLUTVal);

                pOutput->stretchGainRed =
                    static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->r_lut_p_tab.r_lut_p[0]));
                pOutput->stretchGainBlue =
                    static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->b_lut_p_tab.b_lut_p[0]));
                if (PixelFormat::BayerRGGB == static_cast<PixelFormat>(pInput->bayerPattern) ||
                    PixelFormat::BayerGRBG == static_cast<PixelFormat>(pInput->bayerPattern))
                {
                    pOutput->stretchGainGreenEven =
                        static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->gr_lut_p_tab.gr_lut_p[0]));
                    pOutput->stretchGainGreenOdd =
                        static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->gb_lut_p_tab.gb_lut_p[0]));
                }
                else if (PixelFormat::BayerBGGR == static_cast<PixelFormat>(pInput->bayerPattern) ||
                    PixelFormat::BayerGBRG == static_cast<PixelFormat>(pInput->bayerPattern))
                {
                    pOutput->stretchGainGreenEven =
                        static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->gb_lut_p_tab.gb_lut_p[0]));
                    pOutput->stretchGainGreenOdd =
                        static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->gr_lut_p_tab.gr_lut_p[0]));
                }
            }

            commonIQResult = IQInterface::s_interpolationTable.IFELinearization33CalculateHWSetting(
                                 pInput, pInterpolationData, static_cast<VOID*>(&unpackedRegData));
            if (TRUE == commonIQResult)
            {
                IFESetupLinearization33Reg(&unpackedRegData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("Linearization calculate HW settings failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Linearization interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("data is pInput %p pOutput %p", pInput, pOutput);
    }

    return result;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupPDPC11Reg
///
/// @brief  Setup PDPC11 Register based on common library output
///
/// @param  pInput
/// @param  pData
/// @param  pRegCmd
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupPDPC11Reg(
    const PDPC11UnpackedField*  pData,
    PDPC11OutputData*           pOutput)
{
    IFEPDPC11RegCmd* pRegCmd = NULL;

    if ((NULL != pData)   &&
        (NULL != pOutput) &&
        (NULL != pOutput->pRegCmd))
    {
        pRegCmd = pOutput->pRegCmd;

        pRegCmd->configRegister.bitfields.BLACK_LEVEL             = pData->pdaf_blacklevel;
        pRegCmd->configRegister.bitfields.PDAF_DSBPC_EN           = pData->pdaf_dsbpc_en;
        pRegCmd->configRegister.bitfields.PDAF_PDPC_EN            = pData->pdaf_pdpc_en;

        pRegCmd->gainBGWBRegister.bitfields.BG_WB_GAIN            = pData->bg_wb_gain;
        pRegCmd->gainGBWBRegister.bitfields.GB_WB_GAIN            = pData->gb_wb_gain;
        pRegCmd->gainGRWBRegister.bitfields.GR_WB_GAIN            = pData->gr_wb_gain;
        pRegCmd->gainRGWBRegister.bitfields.RG_WB_GAIN            = pData->rg_wb_gain;

        pRegCmd->offsetBPRegister.bitfields.OFFSET_G_PIXEL        = pData->bp_offset_g_pixel;
        pRegCmd->offsetBPRegister.bitfields.OFFSET_RB_PIXEL       = pData->bp_offset_rb_pixel;

        pRegCmd->offsetLOCConfigRegister.bitfields.X_OFFSET       = pData->pdaf_global_offset_x;
        pRegCmd->offsetLOCConfigRegister.bitfields.Y_OFFSET       = pData->pdaf_global_offset_y;

        pRegCmd->offsetT2BPRegister.bitfields.T2_OFFSET_G_PIXEL   = pData->t2_bp_offset_g_pixel;
        pRegCmd->offsetT2BPRegister.bitfields.T2_OFFSET_RB_PIXEL  = pData->t2_bp_offset_rb_pixel;

        pRegCmd->ratioHDRRegister.bitfields.EXP_RATIO             = pData->pdaf_hdr_exp_ratio;
        pRegCmd->ratioHDRRegister.bitfields.EXP_RATIO_RECIP       = pData->pdaf_hdr_exp_ratio_recip;

        pRegCmd->setBPTHregister.bitfields.FMAX                   = pData->fmax_pixel_Q6;
        pRegCmd->setBPTHregister.bitfields.FMIN                   = pData->fmin_pixel_Q6;

        pRegCmd->setLOCENDConfigRegister.bitfields.X_END          = pData->pdaf_x_end;
        pRegCmd->setLOCENDConfigRegister.bitfields.Y_END          = pData->pdaf_y_end;

        if (NULL != pOutput->pDMIData)
        {
            Utils::Memcpy(pOutput->pDMIData, pData->PDAF_PD_Mask, sizeof(UINT32)* DMIRAM_PDAF_LUT_LENGTH);
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid pointer: pData: %p, pOutput: %p", pData, pOutput);

        if (NULL != pOutput)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid pointer: pOutput->pRegCmd: %p", pOutput->pRegCmd);
        }
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFEPDPC11CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IFEPDPC11CalculateSetting(
    const PDPC11InputData*  pInput,
    VOID*                   pOEMIQData,
    PDPC11OutputData*       pOutput)
{
    CamxResult                       result                        = CamxResultSuccess;
    BOOL                             commonIQResult                = FALSE;
    pdpc_1_1_0::pdpc11_rgn_dataType* pInterpolationData            = NULL;
    OEMIFEIQSetting*                 pOEMInput                     = NULL;
    pdpc_1_1_0::chromatix_pdpc11Type::enable_sectionStruct*        pModuleEnable = NULL;
    PDPC11UnpackedField              unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<pdpc_1_1_0::pdpc11_rgn_dataType*>(pInput->pInterpolationData);
            // Call the Interpolation Calculation
            commonIQResult     = IQInterface::s_interpolationTable.IFEPDPC11Interpolation(pInput, pInterpolationData);
            pModuleEnable      = &pInput->pChromatix->enable_section;
        }
        else
        {
            pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);

            pModuleEnable =
               reinterpret_cast<pdpc_1_1_0::chromatix_pdpc11Type::enable_sectionStruct*>(&(pOEMInput->PDPCDataEnable));

            // Use OEM defined interpolation data
            pInterpolationData =
                reinterpret_cast<pdpc_1_1_0::pdpc11_rgn_dataType*>(&(pOEMInput->PDPCSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            commonIQResult = IQInterface::s_interpolationTable.IFEPDPC11CalculateHWSetting(
                pInput, pInterpolationData, pModuleEnable, static_cast<VOID*>(&unpackedRegData));

            if (TRUE == commonIQResult)
            {
                // store pdpc state
                pOutput->pdpcState.pdaf_global_offset_x    = unpackedRegData.pdaf_global_offset_x;
                pOutput->pdpcState.pdaf_x_end              = unpackedRegData.pdaf_x_end;
                pOutput->pdpcState.pdaf_zzHDR_first_rb_exp = unpackedRegData.pdaf_zzHDR_first_rb_exp;
                Utils::Memcpy(pOutput->pdpcState.PDAF_PD_Mask,
                    unpackedRegData.PDAF_PD_Mask,
                    sizeof(unpackedRegData.PDAF_PD_Mask));

                IFESetupPDPC11Reg(&unpackedRegData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("IFEPDPC11 calculate HW settings failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("PDPC interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SetupBLS12Register
///
/// @brief  Setup BLS12 Register based on common library output
///
/// @param  pData   Pointer to the Common Library Calculation Result
/// @param  pRegCmd Pointer to the BLS12 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID SetupBLS12Register(
    const BLS12UnpackedField* pData,
    IFEBLS12RegCmd*           pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->configRegister.bitfields.OFFSET           = pData->offset;
        pRegCmd->configRegister.bitfields.SCALE            = pData->scale;
        pRegCmd->thresholdRegister0.bitfields.GB_THRESHOLD = pData->thresholdGB;
        pRegCmd->thresholdRegister0.bitfields.GR_THRESHOLD = pData->thresholdGR;
        pRegCmd->thresholdRegister1.bitfields.B_THRESHOLD  = pData->thresholdB;
        pRegCmd->thresholdRegister1.bitfields.R_THRESHOLD  = pData->thresholdR;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("data is pData %p pRegCmd %p", pData, pRegCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::BLS12CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::BLS12CalculateSetting(
    const BLS12InputData* pInput,
    VOID*                 pOEMIQData,
    BLS12OutputData*      pOutput)
{
    CamxResult                                            result             = CamxResultSuccess;
    BOOL                                                  commonIQResult     = FALSE;
    bls_1_2_0::bls12_rgn_dataType*                        pInterpolationData = NULL;
    bls_1_2_0::chromatix_bls12Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<bls_1_2_0::bls12_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult     = IQInterface::s_interpolationTable.BLS12Interpolation(pInput, pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<bls_1_2_0::bls12_rgn_dataType*>(&(pOEMInput->BLSSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pModuleEnable  = reinterpret_cast<bls_1_2_0::chromatix_bls12Type::enable_sectionStruct*>(
                                     &(pOEMInput->BLSEnableSection));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            BLS12UnpackedField unpackedRegisterData;

            commonIQResult = IQInterface::s_interpolationTable.BLS12CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pModuleEnable,
                                 static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                SetupBLS12Register(&unpackedRegisterData, pOutput->pRegCmd);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("BLS12 calculate hardware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("BLS12 interpolation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupGamma16Reg
///
/// @brief  Setup Gamma16 Register based on common library output
///
/// @param  pData   Pointer to the Common Library Calculation Result
/// @param  pOutput Pointer to the Gamma16 Register Set
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupGamma16Reg(
    const Gamma16UnpackedField*  pData,
    Gamma16OutputData*           pOutput)
{
    if ((NULL != pData) && (NULL != pOutput))
    {
        pOutput->IFE.pRegCmd->rgb_lut_cfg.bitfields.CH0_BANK_SEL = pData->r_lut_in_cfg.tableSelect;
        pOutput->IFE.pRegCmd->rgb_lut_cfg.bitfields.CH1_BANK_SEL = pData->g_lut_in_cfg.tableSelect;
        pOutput->IFE.pRegCmd->rgb_lut_cfg.bitfields.CH2_BANK_SEL = pData->b_lut_in_cfg.tableSelect;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pOutput %p ", pData, pOutput);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupLinearization34Register
///
/// @brief  Setup Linearization34 Register based on common library output
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupLinearization34Register(
    const Linearization34IQInput*       pInput,
    const Linearization34UnpackedField* pData,
    Linearization34OutputData*          pOutput)
{
    BPSLinearization34RegConfig* pRegCmd        = NULL;
    UINT32                       DMILength      = 0;
    UINT                         count          = 0;

    if ((NULL  != pInput)  &&
        (NULL  != pData)   &&
        (NULL  != pOutput))
    {
        pRegCmd = pOutput->pRegCmd;

        /// @todo (CAMX-1399) Verify all the values from unpacked are falls below bitfields
        pRegCmd->kneepointR0Config.bitfields.P0    = pData->rLUTkneePointL[0];
        pRegCmd->kneepointR0Config.bitfields.P1    = pData->rLUTkneePointL[1];
        pRegCmd->kneepointR1Config.bitfields.P2    = pData->rLUTkneePointL[2];
        pRegCmd->kneepointR1Config.bitfields.P3    = pData->rLUTkneePointL[3];
        pRegCmd->kneepointR2Config.bitfields.P4    = pData->rLUTkneePointL[4];
        pRegCmd->kneepointR2Config.bitfields.P5    = pData->rLUTkneePointL[5];
        pRegCmd->kneepointR3Config.bitfields.P6    = pData->rLUTkneePointL[6];
        pRegCmd->kneepointR3Config.bitfields.P7    = pData->rLUTkneePointL[7];

        pRegCmd->kneepointGR0Config.bitfields.P0   = pData->grLUTkneePointL[0];
        pRegCmd->kneepointGR0Config.bitfields.P1   = pData->grLUTkneePointL[1];
        pRegCmd->kneepointGR1Config.bitfields.P2   = pData->grLUTkneePointL[2];
        pRegCmd->kneepointGR1Config.bitfields.P3   = pData->grLUTkneePointL[3];
        pRegCmd->kneepointGR2Config.bitfields.P4   = pData->grLUTkneePointL[4];
        pRegCmd->kneepointGR2Config.bitfields.P5   = pData->grLUTkneePointL[5];
        pRegCmd->kneepointGR3Config.bitfields.P6   = pData->grLUTkneePointL[6];
        pRegCmd->kneepointGR3Config.bitfields.P7   = pData->grLUTkneePointL[7];

        pRegCmd->kneepointGB0Config.bitfields.P0   = pData->gbLUTkneePointL[0];
        pRegCmd->kneepointGB0Config.bitfields.P1   = pData->gbLUTkneePointL[1];
        pRegCmd->kneepointGB1Config.bitfields.P2   = pData->gbLUTkneePointL[2];
        pRegCmd->kneepointGB1Config.bitfields.P3   = pData->gbLUTkneePointL[3];
        pRegCmd->kneepointGB2Config.bitfields.P4   = pData->gbLUTkneePointL[4];
        pRegCmd->kneepointGB2Config.bitfields.P5   = pData->gbLUTkneePointL[5];
        pRegCmd->kneepointGB3Config.bitfields.P6   = pData->gbLUTkneePointL[6];
        pRegCmd->kneepointGB3Config.bitfields.P7   = pData->gbLUTkneePointL[7];

        pRegCmd->kneepointB0Config.bitfields.P0    = pData->bLUTkneePointL[0];
        pRegCmd->kneepointB0Config.bitfields.P1    = pData->bLUTkneePointL[1];
        pRegCmd->kneepointB1Config.bitfields.P2    = pData->bLUTkneePointL[2];
        pRegCmd->kneepointB1Config.bitfields.P3    = pData->bLUTkneePointL[3];
        pRegCmd->kneepointB2Config.bitfields.P4    = pData->bLUTkneePointL[4];
        pRegCmd->kneepointB2Config.bitfields.P5    = pData->bLUTkneePointL[5];
        pRegCmd->kneepointB3Config.bitfields.P6    = pData->bLUTkneePointL[6];
        pRegCmd->kneepointB3Config.bitfields.P7    = pData->bLUTkneePointL[7];

        ///< packing the DMI table
        for (count = 0; count < MaxSlope; count++, DMILength += 4)
        {
            pOutput->pDMIDataPtr[DMILength]     =
                ((static_cast<UINT32>(pData->rLUTbaseL[pData->LUTbankSelection][count])) & Utils::AllOnes32(14)) |
                ((static_cast<UINT32>(pData->rLUTdeltaL[pData->LUTbankSelection][count]) & Utils::AllOnes32(14)) << 14);
            pOutput->pDMIDataPtr[DMILength + 1] =
                ((static_cast<UINT32>(pData->grLUTbaseL[pData->LUTbankSelection][count])) & Utils::AllOnes32(14)) |
                ((static_cast<UINT32>(pData->grLUTdeltaL[pData->LUTbankSelection][count]) & Utils::AllOnes32(14)) << 14);
            pOutput->pDMIDataPtr[DMILength + 2] =
                ((static_cast<UINT32>(pData->gbLUTbaseL[pData->LUTbankSelection][count])) & Utils::AllOnes32(14)) |
                ((static_cast<UINT32>(pData->gbLUTdeltaL[pData->LUTbankSelection][count]) & Utils::AllOnes32(14)) << 14);
            pOutput->pDMIDataPtr[DMILength + 3] =
                ((static_cast<UINT32>(pData->bLUTbaseL[pData->LUTbankSelection][count])) & Utils::AllOnes32(14)) |
                ((static_cast<UINT32>(pData->bLUTdeltaL[pData->LUTbankSelection][count]) & Utils::AllOnes32(14)) << 14);
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input params are NULL pInput=0x%p pData=0x%p pOutput=0x%p", pInput, pData, pOutput);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupBPCPDPC20Register
///
/// @brief  Setup BPCPDPC20 Register based on common library output
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupBPCPDPC20Register(
    const PDPC20IQInput*    pInput,
    PDPC20UnpackedField*    pData,
    BPSBPCPDPC20OutputData* pOutput)
{
    BPSBPCPDPC20RegCmd*       pRegCmd       = NULL;
    BPSBPCPDPC20ModuleConfig* pModuleConfig = NULL;

    if ((NULL != pInput) &&
        (NULL != pData)  &&
        (NULL != pOutput))
    {
        pRegCmd       = pOutput->pRegCmd;
        pModuleConfig = pOutput->pModuleConfig;
        /// @todo (CAMX-1399) Verify all the values from unpacked are falls below bitfields
        pRegCmd->config0.PDPCBlackLevel.bitfields.BLACK_LEVEL              = pData->blackLevel;

        pRegCmd->config0.HDRExposureRatio.bitfields.EXP_RATIO              = pData->PDAFHDRExposureRatio;
        pRegCmd->config0.HDRExposureRatio.bitfields.EXP_RATIO_RECIP        = pData->PDAFHDRExposureRatioRecip;

        pRegCmd->config0.BPCPixelThreshold.bitfields.CORRECTION_THRESHOLD  = pData->correctionThreshold;
        pRegCmd->config0.BPCPixelThreshold.bitfields.FMAX_PIXEL_Q6         = pData->fmaxPixelQ6;
        pRegCmd->config0.BPCPixelThreshold.bitfields.FMIN_PIXEL_Q6         = pData->fminPixelQ6;

        pRegCmd->config0.badPixelDetectionOffset.bitfields.BCC_OFFSET      = pData->bccOffset;
        pRegCmd->config0.badPixelDetectionOffset.bitfields.BPC_OFFSET      = pData->bpcOffset;

        pRegCmd->config0.PDAFRGWhiteBalanceGain.bitfields.RG_WB_GAIN       = pData->rgWbGain4096;
        pRegCmd->config0.PDAFBGWhiteBalanceGain.bitfields.BG_WB_GAIN       = pData->bgWbGain4096;
        pRegCmd->config0.PDAFGRWhiteBalanceGain.bitfields.GR_WB_GAIN       = pData->grWbGain4096;
        pRegCmd->config0.PDAFGBWhiteBalanceGain.bitfields.GB_WB_GAIN       = pData->gbWbGain4096;

        pRegCmd->config0.PDAFLocationOffsetConfig.bitfields.Y_OFFSET       = pData->PDAFGlobalOffsetY;
        pRegCmd->config0.PDAFLocationOffsetConfig.bitfields.X_OFFSET       = pData->PDAFGlobalOffsetX;

        pRegCmd->config0.PDAFLocationEndConfig.bitfields.Y_END             = pData->PDAFYend;
        pRegCmd->config0.PDAFLocationEndConfig.bitfields.X_END             = pData->PDAFXend;

        pRegCmd->config1.badPixelDetectionOffsetT2.bitfields.BCC_OFFSET_T2 = pData->bccOffsetT2;
        pRegCmd->config1.badPixelDetectionOffsetT2.bitfields.BPC_OFFSET_T2 = pData->bpcOffsetT2;

        pRegCmd->config1.PDPCSaturationThreshold.bitfields.SAT_THRESHOLD   = pData->saturationThreshold;

        pRegCmd->config1.PDAFTabOffsetConfig.bitfields.Y_OFFSET            = pData->PDAFTableYOffset;
        pRegCmd->config1.PDAFTabOffsetConfig.bitfields.X_OFFSET            = pData->PDAFTableXOffset;

        /// @todo (CAMX-2864) Before re-enabling BPS BPC PDPC, fix buffer overrun here (512 bytes copied into 256 byte buffer)
        Utils::Memcpy(static_cast<VOID*>(pOutput->pDMIDataPtr),
                      static_cast<const VOID*>(pData->PDAFPDMask),
                      sizeof(UINT32) * 64);

        pModuleConfig->moduleConfig.bitfields.BAYER_PATTERN                 = pData->bayerPattern;
        pModuleConfig->moduleConfig.bitfields.EN                            = pData->enable;
        pModuleConfig->moduleConfig.bitfields.STRIPE_AUTO_CROP_DIS          = 1;
        pModuleConfig->moduleConfig.bitfields.PDAF_PDPC_EN                  = pData->PDAFPDPCEnable;
        pModuleConfig->moduleConfig.bitfields.BPC_EN                        = pData->PDAFDSBPCEnable;
        pModuleConfig->moduleConfig.bitfields.LEFT_CROP_EN                  = pData->leftCropEnable;
        pModuleConfig->moduleConfig.bitfields.RIGHT_CROP_EN                 = pData->rightCropEnable;
        pModuleConfig->moduleConfig.bitfields.HOT_PIXEL_CORRECTION_DISABLE  = pData->hotPixelCorrectionDisable;
        pModuleConfig->moduleConfig.bitfields.COLD_PIXEL_CORRECTION_DISABLE = pData->coldPixelCorrectionDisable;
        pModuleConfig->moduleConfig.bitfields.USING_CROSS_CHANNEL_EN        = pData->usingCrossChannel;
        pModuleConfig->moduleConfig.bitfields.REMOVE_ALONG_EDGE_EN          = pData->removeAlongEdge;
        pModuleConfig->moduleConfig.bitfields.PDAF_HDR_SELECTION            = pData->PDAFHDRSelection;
        pModuleConfig->moduleConfig.bitfields.PDAF_ZZHDR_FIRST_RB_EXP       = pData->PDAFzzHDRFirstrbExposure;
        pModuleConfig->moduleConfig.bitfields.CHANNEL_BALANCE_EN            = 1;    // Demux always needs to be enabled
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL %p %p %p", pInput, pData, pOutput);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::Gamma16CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::Gamma16CalculateSetting(
    const Gamma16InputData*  pInput,
    VOID*                    pOEMIQData,
    Gamma16OutputData*       pOutput,
    VOID*                    pDebugBuffer)
{
    CamxResult                                                result             = CamxResultSuccess;
    BOOL                                                      commonIQResult     = FALSE;
    gamma_1_6_0::mod_gamma16_channel_dataType*                pInterpolationData = NULL;
    gamma_1_6_0::mod_gamma16_channel_dataType                 pChanneldata[GammaLUTMax];
    Gamma16UnpackedField                                      unpackedRegData;
    gamma_1_6_0::chromatix_gamma16Type::enable_sectionStruct* pModuleEnable      = NULL;

    Utils::Memset(&unpackedRegData, 0, sizeof(unpackedRegData));
    Utils::Memset(&pChanneldata, 0, sizeof(pChanneldata));
    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = &pChanneldata[0];
            commonIQResult     = IQInterface::s_interpolationTable.gamma16Interpolation(pInput, pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            if (PipelineType::IFE == pOutput->type)
            {
                OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);
                pModuleEnable              = reinterpret_cast<gamma_1_6_0::chromatix_gamma16Type::enable_sectionStruct*>(
                    &(pOEMInput->GammaEnableSection));
                // Use OEM defined interpolation data
                pInterpolationData         =
                    reinterpret_cast<gamma_1_6_0::mod_gamma16_channel_dataType*>(&(pOEMInput->GammaSetting[0]));
            }
            else if (PipelineType::BPS == pOutput->type)
            {
                OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);
                pModuleEnable              = reinterpret_cast<gamma_1_6_0::chromatix_gamma16Type::enable_sectionStruct*>(
                    &(pOEMInput->GammaEnableSection));
                // Use OEM defined interpolation data
                pInterpolationData         =
                    reinterpret_cast<gamma_1_6_0::mod_gamma16_channel_dataType*>(&(pOEMInput->GammaSetting[0]));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for Demux13", pOutput->type);
            }

            if ((NULL != pInterpolationData) && (NULL != pModuleEnable))
            {
                commonIQResult = TRUE;
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            unpackedRegData.r_lut_in_cfg.pGammaTable = pOutput->pRDMIDataPtr;
            unpackedRegData.g_lut_in_cfg.pGammaTable = pOutput->pGDMIDataPtr;
            unpackedRegData.b_lut_in_cfg.pGammaTable = pOutput->pBDMIDataPtr;

            commonIQResult =
                IQInterface::s_interpolationTable.gamma16CalculateHWSetting(pInput,
                                                                            pInterpolationData,
                                                                            pModuleEnable,
                                                                            static_cast<VOID*>(&unpackedRegData));

            if (TRUE == commonIQResult)
            {
                if (PipelineType::IFE == pOutput->type)
                {
                    IFESetupGamma16Reg(&unpackedRegData, pOutput);
                }
                else if (PipelineType::BPS != pOutput->type)
                {
                    result = CamxResultEInvalidArg;
                    CAMX_ASSERT_ALWAYS_MESSAGE("Gamma16 Invalid Pipeline");
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("Linearization34 calculate HW settings failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Gamma16 interpolation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Gamma Input data is pInput %p pOutput %p", pInput, pOutput);
    }

    if (NULL != pDebugBuffer && CamxResultSuccess == result)
    {
        Gamma16DebugBuffer* pBuffer = static_cast<Gamma16DebugBuffer*>(pDebugBuffer);

        Utils::Memcpy(&(pBuffer->interpolationResult),
                      pInterpolationData,
                      sizeof(gamma_1_6_0::mod_gamma16_channel_dataType) * GammaLUTMax);
        Utils::Memcpy(&(pBuffer->unpackedData), &unpackedRegData, sizeof(unpackedRegData));
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::BPSBPCPDPC20CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::BPSBPCPDPC20CalculateSetting(
    PDPC20IQInput*          pInput,
    VOID*                   pOEMIQData,
    BPSBPCPDPC20OutputData* pOutput,
    PixelFormat             pixelFormat)
{
    CamxResult                                              result             = CamxResultSuccess;
    BOOL                                                    commonIQResult     = FALSE;
    pdpc_2_0_0::pdpc20_rgn_dataType*                        pInterpolationData = NULL;
    pdpc_2_0_0::chromatix_pdpc20Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        result = GetPixelFormat(&pixelFormat, &(pInput->bayerPattern));

        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<pdpc_2_0_0::pdpc20_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult     = IQInterface::s_interpolationTable.BPSPDPC20Interpolation(pInput, pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<pdpc_2_0_0::pdpc20_rgn_dataType*>(&(pOEMInput->PDPCSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pModuleEnable  = reinterpret_cast<pdpc_2_0_0::chromatix_pdpc20Type::enable_sectionStruct*>(
                                     &(pOEMInput->PDPCEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            PDPC20UnpackedField unpackedRegisterData = { 0 };

            commonIQResult =
                IQInterface::s_interpolationTable.BPSPDPC20CalculateHWSetting(pInput,
                                                                              pInterpolationData,
                                                                              pModuleEnable,
                                                                              static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                BPSSetupBPCPDPC20Register(pInput, &unpackedRegisterData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("BPCPDP Calculate HW setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("BPCPDP intepolation failed.");
        }
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL %p %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::BPSLinearization34CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::BPSLinearization34CalculateSetting(
    const Linearization34IQInput* pInput,
    VOID*                         pOEMIQData,
    Linearization34OutputData*    pOutput)
{
    CamxResult                                                                result             = CamxResultSuccess;
    BOOL                                                                      commonIQResult     = FALSE;
    linearization_3_4_0::linearization34_rgn_dataType*                        pInterpolationData = NULL;
    linearization_3_4_0::chromatix_linearization34Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<linearization_3_4_0::linearization34_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult     = IQInterface::s_interpolationTable.BPSLinearization34Interpolation(pInput, pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData =
                reinterpret_cast<linearization_3_4_0::linearization34_rgn_dataType*>(&(pOEMInput->LinearizationSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pModuleEnable  = reinterpret_cast<linearization_3_4_0::chromatix_linearization34Type::enable_sectionStruct*>(
                                     &(pOEMInput->LinearizationEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            Linearization34UnpackedField unpackedRegisterData = { 0 };

            // Calculate the remaining Gain to apply in Demux
            if (FALSE == pInput->pedestalEnable)
            {
                FLOAT maxLUTValue = static_cast<FLOAT>(Linearization34MaximumLUTVal);

                pOutput->stretchGainRed  =
                    static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->r_lut_p_tab.r_lut_p[0]));
                pOutput->stretchGainBlue =
                    static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->b_lut_p_tab.b_lut_p[0]));
                if (PixelFormat::BayerRGGB == static_cast<PixelFormat>(pInput->bayerPattern) ||
                    PixelFormat::BayerGRBG == static_cast<PixelFormat>(pInput->bayerPattern))
                {
                    pOutput->stretchGainGreenEven =
                        static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->gr_lut_p_tab.gr_lut_p[0]));
                    pOutput->stretchGainGreenOdd  =
                        static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->gb_lut_p_tab.gb_lut_p[0]));
                }
                else if (PixelFormat::BayerBGGR == static_cast<PixelFormat>(pInput->bayerPattern) ||
                    PixelFormat::BayerGBRG == static_cast<PixelFormat>(pInput->bayerPattern))
                {
                    pOutput->stretchGainGreenEven =
                        static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->gb_lut_p_tab.gb_lut_p[0]));
                    pOutput->stretchGainGreenOdd  =
                        static_cast<FLOAT>(maxLUTValue / (maxLUTValue - pInterpolationData->gr_lut_p_tab.gr_lut_p[0]));
                }
            }

            commonIQResult = IQInterface::s_interpolationTable.BPSLinearization34CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pModuleEnable,
                                 static_cast<VOID*>(&unpackedRegisterData));


            if (TRUE == commonIQResult)
            {
                BPSSetupLinearization34Register(pInput, &unpackedRegisterData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("Linearization34 calculate HW settings failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Linearization34 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupPedestal13Register
///
/// @brief  Setup Pedestal13 Register/DMI based on common library output
///
/// @param  pData    Pointer to the Common Library Calculation Result
/// @param  pCmd     Pointer to the Pedestal13 Register/DMI Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupPedestal13Register(
    const Pedestal13UnpackedField* pData,
    Pedestal13OutputData*          pCmd)
{
    UINT32 DMICnt = 0;

    if ((NULL != pData) && (NULL != pCmd))
    {
        pCmd->regCommand.pRegIFECmd->config.bitfields.HDR_EN                       = pData->HDRen;
        pCmd->regCommand.pRegIFECmd->config.bitfields.LUT_BANK_SEL                 = pData->bankSel;
        pCmd->regCommand.pRegIFECmd->config.bitfields.SCALE_BYPASS                 = pData->scaleBypass;
        pCmd->regCommand.pRegIFECmd->config0.bitfields.BLOCK_HEIGHT                = pData->meshGridbHeightL;
        pCmd->regCommand.pRegIFECmd->config0.bitfields.BLOCK_WIDTH                 = pData->meshGridbWidthL;
        pCmd->regCommand.pRegIFECmd->config1.bitfields.INTERP_FACTOR               = pData->intpFactorL;
        pCmd->regCommand.pRegIFECmd->config1.bitfields.SUB_GRID_HEIGHT             = pData->bHeightL;
        pCmd->regCommand.pRegIFECmd->config1.bitfields.SUB_GRID_Y_DELTA            = pData->yDeltaL;
        pCmd->regCommand.pRegIFECmd->config2.bitfields.SUB_GRID_WIDTH              = pData->bWidthL;
        pCmd->regCommand.pRegIFECmd->config2.bitfields.SUB_GRID_X_DELTA            = pData->xDeltaL;
        pCmd->regCommand.pRegIFECmd->rightGridConfig0.bitfields.BLOCK_HEIGHT       = pData->meshGridbHeightR;
        pCmd->regCommand.pRegIFECmd->rightGridConfig0.bitfields.BLOCK_WIDTH        = pData->meshGridbWidthL;
        pCmd->regCommand.pRegIFECmd->rightGridConfig1.bitfields.INTERP_FACTOR      = pData->intpFactorL;
        pCmd->regCommand.pRegIFECmd->rightGridConfig1.bitfields.SUB_GRID_HEIGHT    = pData->bHeightL;
        pCmd->regCommand.pRegIFECmd->rightGridConfig1.bitfields.SUB_GRID_Y_DELTA   = pData->yDeltaL;
        pCmd->regCommand.pRegIFECmd->rightGridConfig2.bitfields.SUB_GRID_WIDTH     = pData->bWidthL;
        pCmd->regCommand.pRegIFECmd->rightGridConfig2.bitfields.SUB_GRID_X_DELTA   = pData->xDeltaL;
        pCmd->regCommand.pRegIFECmd->rightStripeConfig0.bitfields.BLOCK_X_INDEX    = pData->lxStartL;
        pCmd->regCommand.pRegIFECmd->rightStripeConfig0.bitfields.BLOCK_Y_INDEX    = pData->lyStartL;
        pCmd->regCommand.pRegIFECmd->rightStripeConfig1.bitfields.PIXEL_X_INDEX    = pData->bxD1L;
        pCmd->regCommand.pRegIFECmd->rightStripeConfig1.bitfields.PIXEL_Y_INDEX    = pData->byE1L;
        pCmd->regCommand.pRegIFECmd->rightStripeConfig1.bitfields.SUB_GRID_X_INDEX = pData->bxStartL;
        pCmd->regCommand.pRegIFECmd->rightStripeConfig1.bitfields.SUB_GRID_Y_INDEX = pData->byStartL;
        pCmd->regCommand.pRegIFECmd->stripeConfig0.bitfields.BLOCK_X_INDEX         = pData->lxStartL;
        pCmd->regCommand.pRegIFECmd->stripeConfig0.bitfields.BLOCK_Y_INDEX         = pData->lyStartL;
        pCmd->regCommand.pRegIFECmd->stripeConfig0.bitfields.Y_DELTA_ACCUM         = (pData->yDeltaL * pData->byE1L);
        pCmd->regCommand.pRegIFECmd->stripeConfig1.bitfields.PIXEL_X_INDEX         = pData->bxD1L;
        pCmd->regCommand.pRegIFECmd->stripeConfig1.bitfields.PIXEL_Y_INDEX         = pData->byE1L;
        pCmd->regCommand.pRegIFECmd->stripeConfig1.bitfields.SUB_GRID_X_INDEX      = pData->bxStartL;
        pCmd->regCommand.pRegIFECmd->stripeConfig1.bitfields.SUB_GRID_Y_INDEX      = pData->byStartL;

        for (UINT32 verMeshNum = 0; verMeshNum < PED_MESH_PT_V_V13; verMeshNum++)
        {
            for (UINT32 horMeshNum = 0; horMeshNum < PED_MESH_PT_H_V13; horMeshNum++)
            {
                // 0->R, 1->Gr, 2->Gb, 3->B
                pCmd->pGRRLUTDMIBuffer[DMICnt] =
                    ((pData->meshTblT1L[pData->bankSel][0][verMeshNum][horMeshNum] & Utils::AllOnes32(12)) |
                    ((pData->meshTblT1L[pData->bankSel][1][verMeshNum][horMeshNum] & Utils::AllOnes32(12)) << 12));
                pCmd->pGBBLUTDMIBuffer[DMICnt] =
                    ((pData->meshTblT1L[pData->bankSel][3][verMeshNum][horMeshNum] & Utils::AllOnes32(12)) |
                    ((pData->meshTblT1L[pData->bankSel][2][verMeshNum][horMeshNum] & Utils::AllOnes32(12)) << 12));
                DMICnt++;
            }
        }
        CAMX_ASSERT(PED_LUT_LENGTH == DMICnt);
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("data is pData %p pCmd %p ", pData, pCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupPedestal13Register
///
/// @brief  Setup Pedestal13 Register/DMI based on common library output
///
/// @param  pData    Pointer to the Common Library Calculation Result
/// @param  pCmd     Pointer to the Pedestal13 Register/DMI Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupPedestal13Register(
    Pedestal13UnpackedField* pData,
    Pedestal13OutputData*    pCmd)
{
    UINT32                  DMICnt             = 0;
    BPSPedestal13RegConfig* pRegBPSCmd         = NULL;
    PedestalParameters*     pPedestalParameters = NULL;

    if ((NULL != pData) && (NULL != pCmd))
    {
        pRegBPSCmd          = pCmd->regCommand.BPS.pRegBPSCmd;
        pPedestalParameters = pCmd->regCommand.BPS.pPedestalParameters;

        pPedestalParameters->moduleCfg.SCALE_BYPASS  = pData->scaleBypass;
        // Need to check on how to get these values
        pPedestalParameters->moduleCfg.NUM_SUBBLOCKS = pData->intpFactorL;
        pPedestalParameters->initBlockY              = pData->lyStartL;
        pPedestalParameters->initPixelY              = pData->lxStartL;
        pPedestalParameters->initSubblockY           = pData->byStartL;
        pPedestalParameters->meshGridBwidth          = pData->meshGridbWidthL;
        pPedestalParameters->startBlockIndex         = pData->bxStartL;
        pPedestalParameters->startSubgridIndex       = pData->bxStartL;
        pPedestalParameters->subgridWidth            = pData->bWidthL;
        pPedestalParameters->topLeftCoordinate       = pData->bxD1L;

        pRegBPSCmd->module1Config.bitfields.BLOCK_HEIGHT        = pData->meshGridbHeightL;
        pRegBPSCmd->module1Config.bitfields.BLOCK_WIDTH         = pData->meshGridbWidthL;
        pRegBPSCmd->module2Config.bitfields.INV_SUBBLOCK_WIDTH  = pData->xDeltaL;
        pRegBPSCmd->module2Config.bitfields.SUBBLOCK_WIDTH      = pData->bWidthL;
        pRegBPSCmd->module3Config.bitfields.INV_SUBBLOCK_HEIGHT = pData->yDeltaL;
        pRegBPSCmd->module3Config.bitfields.SUBBLOCK_HEIGHT     = pData->bHeightL;
        pRegBPSCmd->module4Config.bitfields.INIT_PIXEL_X        = pData->bxD1L;
        pRegBPSCmd->module4Config.bitfields.INIT_PIXEL_Y        = pData->byE1L;
        pRegBPSCmd->module5Config.bitfields.INIT_YDELTA         = pData->byInitE1L;

        for (UINT32 verMeshNum = 0; verMeshNum < PED_MESH_PT_V_V13; verMeshNum++)
        {
            for (UINT32 horMeshNum = 0; horMeshNum < PED_MESH_PT_H_V13; horMeshNum++)
            {
                // 0->R, 1->Gr, 2->Gb, 3->B
                pCmd->pGRRLUTDMIBuffer[DMICnt] =
                    ((pData->meshTblT1L[pData->bankSel][0][verMeshNum][horMeshNum] & Utils::AllOnes32(12)) |
                    ((pData->meshTblT1L[pData->bankSel][1][verMeshNum][horMeshNum] & Utils::AllOnes32(12)) << 12));
                pCmd->pGBBLUTDMIBuffer[DMICnt] =
                    ((pData->meshTblT1L[pData->bankSel][3][verMeshNum][horMeshNum] & Utils::AllOnes32(12)) |
                    ((pData->meshTblT1L[pData->bankSel][2][verMeshNum][horMeshNum] & Utils::AllOnes32(12)) << 12));
                DMICnt++;
            }
        }
        CAMX_ASSERT(PED_LUT_LENGTH == DMICnt);
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("data is  pData %p pCmd %p ", pData, pCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFESetupABF34Reg
///
/// @brief  Setup ABF34 Register based on common library output
///
/// @param  pData   Pointer to the Common Library Calculation Result
/// @param  pOutput Pointer to the ABF34 Output
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupABF34Reg(
    const ABF34UnpackedField* pData,
    ABF34OutputData*          pOutput)
{
    IFEABF34RegCmd1* pRegCmd1 = NULL;
    IFEABF34RegCmd2* pRegCmd2 = NULL;

    if ((NULL != pOutput)           &&
        (NULL != pData)             &&
        (NULL != pOutput->pRegCmd1) &&
        (NULL != pOutput->pRegCmd2))
    {
        pRegCmd1 = pOutput->pRegCmd1;
        pRegCmd2 = pOutput->pRegCmd2;

        pRegCmd1->configReg.bitfields.CROSS_PLANE_EN          = pData->cross_process_en;
        pRegCmd1->configReg.bitfields.DISTANCE_GRGB_0         = pData->distance_ker[0][0];
        pRegCmd1->configReg.bitfields.DISTANCE_GRGB_1         = pData->distance_ker[0][1];
        pRegCmd1->configReg.bitfields.DISTANCE_GRGB_2         = pData->distance_ker[0][2];
        pRegCmd1->configReg.bitfields.DISTANCE_RB_0           = pData->distance_ker[1][0];
        pRegCmd1->configReg.bitfields.DISTANCE_RB_1           = pData->distance_ker[1][1];
        pRegCmd1->configReg.bitfields.DISTANCE_RB_2           = pData->distance_ker[1][2];
        pRegCmd1->configReg.bitfields.FILTER_EN               = pData->filter_en;
        pRegCmd1->configReg.bitfields.LUT_BANK_SEL            = pData->lut_bank_sel;
        pRegCmd1->configReg.bitfields.PIXEL_MATCH_LEVEL_GRGB  = pData->blk_pix_matching_g;
        pRegCmd1->configReg.bitfields.PIXEL_MATCH_LEVEL_RB    = pData->blk_pix_matching_rb;
        pRegCmd1->configReg.bitfields.SINGLE_BPC_EN           = pData->single_bpc_en;

        pRegCmd2->configGRChannel.bitfields.CURVE_OFFSET      = pData->curve_offset[0];
        pRegCmd2->configGBChannel.bitfields.CURVE_OFFSET      = pData->curve_offset[1];
        pRegCmd2->configRChannel.bitfields.CURVE_OFFSET       = pData->curve_offset[2];
        pRegCmd2->configBChannel.bitfields.CURVE_OFFSET       = pData->curve_offset[3];
        pRegCmd2->configRNR0.bitfields.BX                     = pData->bx;
        pRegCmd2->configRNR0.bitfields.BY                     = pData->by;
        pRegCmd2->configRNR1.bitfields.INIT_RSQUARE           = pData->r_square_init;
        pRegCmd2->configRNR2.bitfields.ANCHOR_0               = pData->anchor_table[0];
        pRegCmd2->configRNR2.bitfields.ANCHOR_1               = pData->anchor_table[1];
        pRegCmd2->configRNR3.bitfields.ANCHOR_2               = pData->anchor_table[2];
        pRegCmd2->configRNR3.bitfields.ANCHOR_3               = pData->anchor_table[3];
        pRegCmd2->configRNR4.bitfields.COEFF_BASE_0           = pData->base_table[0][0];
        pRegCmd2->configRNR4.bitfields.COEFF_SHIFT_0          = pData->shift_table[0][0];
        pRegCmd2->configRNR4.bitfields.COEFF_SLOPE_0          = pData->slope_table[0][0];
        pRegCmd2->configRNR5.bitfields.COEFF_BASE_1           = pData->base_table[0][1];
        pRegCmd2->configRNR5.bitfields.COEFF_SHIFT_1          = pData->shift_table[0][1];
        pRegCmd2->configRNR5.bitfields.COEFF_SLOPE_1          = pData->slope_table[0][1];
        pRegCmd2->configRNR6.bitfields.COEFF_BASE_2           = pData->base_table[0][2];
        pRegCmd2->configRNR6.bitfields.COEFF_SHIFT_2          = pData->shift_table[0][2];
        pRegCmd2->configRNR6.bitfields.COEFF_SLOPE_2          = pData->slope_table[0][2];
        pRegCmd2->configRNR7.bitfields.COEFF_BASE_3           = pData->base_table[0][3];
        pRegCmd2->configRNR7.bitfields.COEFF_SHIFT_3          = pData->shift_table[0][3];
        pRegCmd2->configRNR7.bitfields.COEFF_SLOPE_3          = pData->slope_table[0][3];
        pRegCmd2->configRNR8.bitfields.THRESH_BASE_0          = pData->base_table[1][0];
        pRegCmd2->configRNR8.bitfields.THRESH_SHIFT_0         = pData->shift_table[1][0];
        pRegCmd2->configRNR8.bitfields.THRESH_SLOPE_0         = pData->slope_table[1][0];
        pRegCmd2->configRNR9.bitfields.THRESH_BASE_1          = pData->base_table[1][1];
        pRegCmd2->configRNR9.bitfields.THRESH_SHIFT_1         = pData->shift_table[1][1];
        pRegCmd2->configRNR9.bitfields.THRESH_SLOPE_1         = pData->slope_table[1][1];
        pRegCmd2->configRNR10.bitfields.THRESH_BASE_2         = pData->base_table[1][2];
        pRegCmd2->configRNR10.bitfields.THRESH_SHIFT_2        = pData->shift_table[1][2];
        pRegCmd2->configRNR10.bitfields.THRESH_SLOPE_2        = pData->slope_table[1][2];
        pRegCmd2->configRNR11.bitfields.THRESH_BASE_3         = pData->base_table[1][3];
        pRegCmd2->configRNR11.bitfields.THRESH_SHIFT_3        = pData->shift_table[1][3];
        pRegCmd2->configRNR11.bitfields.THRESH_SLOPE_3        = pData->slope_table[1][3];
        pRegCmd2->configRNR12.bitfields.RSQUARE_SHIFT         = pData->r_square_shft;
        pRegCmd2->configBPC0.bitfields.FMAX                   = pData->bpc_fmax;
        pRegCmd2->configBPC0.bitfields.FMIN                   = pData->bpc_fmin;
        pRegCmd2->configBPC0.bitfields.OFFSET                 = pData->bpc_offset;
        pRegCmd2->configBPC1.bitfields.BLS                    = pData->bpc_bls;
        pRegCmd2->configBPC1.bitfields.MAX_SHIFT              = pData->bpc_maxshft;
        pRegCmd2->configBPC1.bitfields.MIN_SHIFT              = pData->bpc_minshft;
        pRegCmd2->noisePreserveConfig0.bitfields.ANCHOR_GAP   = pData->noise_prsv_anchor_gap;
        pRegCmd2->noisePreserveConfig0.bitfields.ANCHOR_LO    = pData->noise_prsv_anchor_lo;
        pRegCmd2->noisePreserveConfig1.bitfields.LO_GRGB      = pData->noise_prsv_lo[0];
        pRegCmd2->noisePreserveConfig1.bitfields.SHIFT_GRGB   = pData->noise_prsv_shft[0];
        pRegCmd2->noisePreserveConfig1.bitfields.SLOPE_GRGB   = pData->noise_prsv_slope[0];
        pRegCmd2->noisePreserveConfig2.bitfields.LO_RB        = pData->noise_prsv_lo[0];
        pRegCmd2->noisePreserveConfig2.bitfields.SHIFT_RB     = pData->noise_prsv_shft[0];
        pRegCmd2->noisePreserveConfig2.bitfields.SLOPE_RB     = pData->noise_prsv_slope[0];

        for (UINT32 index = 0; index < DMIRAM_ABF34_NOISESTD_LENGTH; index++)
        {
            pOutput->pDMIData[index] = pData->noise_std_lut_level0[pData->lut_bank_sel][index];
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid data's %p %p %p %p", pOutput, pData, pRegCmd1, pRegCmd2);
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFEABF34CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IFEABF34CalculateSetting(
    const ABF34InputData* pInput,
    VOID*                 pOEMIQData,
    ABF34OutputData*      pOutput)
{
    CamxResult                                            result              = CamxResultSuccess;
    BOOL                                                  commonIQResult      = FALSE;
    abf_3_4_0::abf34_rgn_dataType*                        pInterpolationData  = NULL;
    OEMIFEIQSetting*                                      pOEMInput           = NULL;
    abf_3_4_0::chromatix_abf34Type::enable_sectionStruct* pModuleEnable       = NULL;
    abf_3_4_0::chromatix_abf34_reserveType*               pReserveType         = NULL;
    ABF34UnpackedField                                    unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<abf_3_4_0::abf34_rgn_dataType*>(pInput->pInterpolationData);

            commonIQResult     = IQInterface::s_interpolationTable.IFEABF34Interpolation(pInput, pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
            pReserveType       = &(pInput->pChromatix->chromatix_abf34_reserve);
        }
        else
        {
            pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);
            // Use OEM defined interpolation data
            pInterpolationData =
                reinterpret_cast<abf_3_4_0::abf34_rgn_dataType*>(&(pOEMInput->ABFSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pModuleEnable  =
                    reinterpret_cast<abf_3_4_0::chromatix_abf34Type::enable_sectionStruct*>(&(pOEMInput->ABFEnableSection));
                pReserveType   =
                    reinterpret_cast<abf_3_4_0::chromatix_abf34_reserveType*>(&(pOEMInput->ABFReserveType));
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            commonIQResult = IQInterface::s_interpolationTable.IFEABF34CalculateHWSetting(
                pInput, pInterpolationData, pModuleEnable, pReserveType, static_cast<VOID*>(&unpackedRegData));

            if (TRUE == commonIQResult)
            {
                IFESetupABF34Reg(&unpackedRegData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupPProc, "ABF calculate HW setting failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_LOG_ERROR(CamxLogGroupPProc, "ABF interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Input data is  pInput %p pOutput %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SetupIFEGTM10Register
///
/// @brief  Setup GTM10 Register based on common library output
///
/// @param  pData   Pointer to the Common Library Calculation Result
/// @param  pCmd    Pointer to the GTM10 Register and DMI Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID SetupIFEGTM10Register(
    GTM10UnpackedField* pData,
    GTM10OutputData*    pCmd)
{
    UINT32               LUTEntry       = 0;

    if ((NULL != pData) && (NULL != pCmd))
    {
        pCmd->regCmd.IFE.pRegCmd->configRegister.bitfields.LUT_BANK_SEL = pData->tableSel;

        for (LUTEntry = 0; LUTEntry < GTM10LUTSize; LUTEntry++)
        {
            pCmd->regCmd.IFE.pDMIDataPtr[LUTEntry] =
                (static_cast<UINT64>(pData->YratioBase[pData->tableSel][LUTEntry]) & Utils::AllOnes64(18)) |
                ((static_cast<UINT64>(pData->YratioSlope[pData->tableSel][LUTEntry]) & Utils::AllOnes64(26)) << 32);
        }
        if (TRUE != pCmd->registerBETEn)
        {
            const HwEnvironment* pHwEnvironment = HwEnvironment::GetInstance();
            if (pHwEnvironment->IsHWBugWorkaroundEnabled(Titan17xWorkarounds::Titan17xWorkaroundsCDMDMI64EndiannessBug))
            {

                for (LUTEntry = 0; LUTEntry < GTM10LUTSize; LUTEntry++)
                {
                    pCmd->regCmd.IFE.pDMIDataPtr[LUTEntry] =
                        ((pCmd->regCmd.IFE.pDMIDataPtr[LUTEntry] & CamX::Utils::AllOnes64(32)) << 32) |
                        (((pCmd->regCmd.IFE.pDMIDataPtr[LUTEntry] >> 32) & CamX::Utils::AllOnes64(32)));
                }
            }
        }

    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("data is NULL %p %p", pData, pCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SetupBPSGTM10Register
///
/// @brief  Setup GTM10 Register based on common library output
///
/// @param  pData   Pointer to the Common Library Calculation Result
/// @param  pCmd    Pointer to the GTM10 Register and DMI Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID SetupBPSGTM10Register(
    GTM10UnpackedField* pData,
    GTM10OutputData*    pCmd)
{
    UINT32 LUTEntry = 0;

    if ((NULL != pData) && (NULL != pCmd))
    {
        for (LUTEntry = 0; LUTEntry < GTM10LUTSize; LUTEntry++)
        {
            pCmd->regCmd.BPS.pDMIDataPtr[LUTEntry] =
                (static_cast<UINT64>(pData->YratioBase[pData->tableSel][LUTEntry]) & Utils::AllOnes64(18)) |
                ((static_cast<UINT64>(pData->YratioSlope[pData->tableSel][LUTEntry]) & Utils::AllOnes64(26)) << 18);
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("data is pData %p pCmd %p ", pData, pCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::GTM10CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::GTM10CalculateSetting(
    GTM10InputData*   pInput,
    VOID*             pOEMIQData,
    GTM10OutputData*  pOutput,
    TMC10InputData*   pTMCInput)
{

    CamxResult                                            result             = CamxResultSuccess;
    BOOL                                                  commonIQResult     = FALSE;
    gtm_1_0_0::gtm10_rgn_dataType*                        pInterpolationData = NULL;
    gtm_1_0_0::chromatix_gtm10_reserveType*               pReserveType       = NULL;
    gtm_1_0_0::chromatix_gtm10Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<gtm_1_0_0::gtm10_rgn_dataType*>(pInput->pInterpolationData);
            // Call the Interpolation Calculation
            commonIQResult = IQInterface::s_interpolationTable.GTM10Interpolation(pInput, pInterpolationData);

            if ((TRUE == commonIQResult) &&
                (TRUE == pTMCInput->adrcGTMEnable))
            {
                if (SWTMCVersion::TMC10 == pTMCInput->pAdrcOutputData->version)
                {
                    commonIQResult = IQInterface::GetADRCData(pTMCInput);
                }
                pInput->pAdrcInputData = pTMCInput->pAdrcOutputData;
            }

            pReserveType       = &(pInput->pChromatix->chromatix_gtm10_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            if (PipelineType::IFE == pOutput->type)
            {
                OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);
                // Use OEM defined interpolation data
                pInterpolationData = reinterpret_cast<gtm_1_0_0::gtm10_rgn_dataType*>(&(pOEMInput->GTMSetting));
                pReserveType = reinterpret_cast<gtm_1_0_0::chromatix_gtm10_reserveType*>(
                    &(pOEMInput->GTMReserveType));
                pModuleEnable = reinterpret_cast<gtm_1_0_0::chromatix_gtm10Type::enable_sectionStruct*>(
                    &(pOEMInput->GTMEnableSection));
            }
            else if (PipelineType::BPS == pOutput->type)
            {
                OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);
                // Use OEM defined interpolation data
                pInterpolationData = reinterpret_cast<gtm_1_0_0::gtm10_rgn_dataType*>(&(pOEMInput->GTMSetting));
                pReserveType = reinterpret_cast<gtm_1_0_0::chromatix_gtm10_reserveType*>(
                    &(pOEMInput->GTMReserveType));
                pModuleEnable = reinterpret_cast<gtm_1_0_0::chromatix_gtm10Type::enable_sectionStruct*>(
                    &(pOEMInput->GTMEnableSection));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for Demux13", pOutput->type);
            }

            if ((NULL != pInterpolationData) && (NULL != pReserveType) && (NULL !=  pModuleEnable))
            {
                commonIQResult = TRUE;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            GTM10UnpackedField unpackedRegisterData;

            commonIQResult = IQInterface::s_interpolationTable.GTM10CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pReserveType,
                                 pModuleEnable,
                                 static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                if (PipelineType::IFE == pOutput->type)
                {
                    SetupIFEGTM10Register(&unpackedRegisterData, pOutput);
                }
                else if (PipelineType::BPS == pOutput->type)
                {
                    SetupBPSGTM10Register(&unpackedRegisterData, pOutput);
                }
                else
                {
                    CAMX_ASSERT_ALWAYS_MESSAGE("invalid Pipeline");
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("GTM10 calculate hardware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("GTM10 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Data is pInput %p pOutput %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupCC13Reg
///
/// @brief  Setup BPS CC13 Register based on common library output
///
/// @param  pRegVal    Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the CC13 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupCC13Reg(
    const CC13UnpackedField*  pData,
    BPSCC13RegConfig*         pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->coefficientAConfig0.bitfields.MATRIX_A0 = pData->c0;
        pRegCmd->coefficientAConfig0.bitfields.MATRIX_A1 = pData->c3;
        pRegCmd->coefficientAConfig1.bitfields.MATRIX_A2 = pData->c6;
        pRegCmd->coefficientBConfig0.bitfields.MATRIX_B0 = pData->c1;
        pRegCmd->coefficientBConfig0.bitfields.MATRIX_B1 = pData->c4;
        pRegCmd->coefficientBConfig1.bitfields.MATRIX_B2 = pData->c7;
        pRegCmd->coefficientCConfig0.bitfields.MATRIX_C0 = pData->c2;
        pRegCmd->coefficientCConfig0.bitfields.MATRIX_C1 = pData->c5;
        pRegCmd->coefficientCConfig1.bitfields.MATRIX_C2 = pData->c8;
        pRegCmd->offsetKConfig0.bitfields.OFFSET_K0      = pData->k0;
        pRegCmd->offsetKConfig0.bitfields.OFFSET_K1      = pData->k1;
        pRegCmd->offsetKConfig1.bitfields.OFFSET_K2      = pData->k2;
        pRegCmd->shiftMConfig.bitfields.M_PARAM          = pData->qfactor;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pRegCmd %p ", pData, pRegCmd);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupCC13Reg
///
/// @brief  Setup IPE CC13 Register based on common library output
///
/// @param  pData      Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the CC13 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupCC13Reg(
    CC13UnpackedField*  pData,
    IPECC13RegConfig*   pRegCmd)
{

    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->greenCoefficients0.bitfields.MATRIX_A0 = pData->c0;
        pRegCmd->greenCoefficients0.bitfields.MATRIX_A1 = pData->c3;
        pRegCmd->greenCoefficients1.bitfields.MATRIX_A2 = pData->c6;
        pRegCmd->blueCoefficients0.bitfields.MATRIX_B0  = pData->c1;
        pRegCmd->blueCoefficients0.bitfields.MATRIX_B1  = pData->c4;
        pRegCmd->blueCoefficients1.bitfields.MATRIX_B2  = pData->c7;
        pRegCmd->redCoefficients0.bitfields.MATRIX_C0   = pData->c2;
        pRegCmd->redCoefficients0.bitfields.MATRIX_C1   = pData->c5;
        pRegCmd->redCoefficients1.bitfields.MATRIX_C2   = pData->c8;
        pRegCmd->offsetParam0.bitfields.OFFSET_K0       = pData->k0;
        pRegCmd->offsetParam0.bitfields.OFFSET_K1       = pData->k1;
        pRegCmd->offsetParam1.bitfields.OFFSET_K2       = pData->k2;
        pRegCmd->qFactor.bitfields.M_PARAM              = pData->qfactor;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pData %p pRegCmd %p ", pData, pRegCmd);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CC13ApplyEffect
///
/// @brief  Apply effects if any to the interpolated CC values
///
/// @param  pInput   Pointer to the input data
/// @param  pData    Pointer to the interpolated data to be updated
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID CC13ApplyEffect(
    const CC13InputData*         pInput,
    cc_1_3_0::cc13_rgn_dataType* pData)
{
    UINT8 matrixSize = 3;
    FLOAT out[3][3];
    FLOAT inputMatrix[3][3];

    // Copy interpolated data into a 3x3 matrix
    inputMatrix[0][0] = pData->c_tab.c[0];
    inputMatrix[0][1] = pData->c_tab.c[1];
    inputMatrix[0][2] = pData->c_tab.c[2];
    inputMatrix[1][0] = pData->c_tab.c[3];
    inputMatrix[1][1] = pData->c_tab.c[4];
    inputMatrix[1][2] = pData->c_tab.c[5];
    inputMatrix[2][0] = pData->c_tab.c[6];
    inputMatrix[2][1] = pData->c_tab.c[7];
    inputMatrix[2][2] = pData->c_tab.c[8];

    // Matrix multiplication of effects and interpolated data
    for (UINT8 i = 0; i < matrixSize; i++)
    {
        for (UINT8 j = 0; j < matrixSize; j++)
        {
            out[i][j] = 0;
            for (UINT8 k = 0; k < matrixSize; k++)
            {
                out[i][j] += (pInput->effectsMatrix[i][k] * inputMatrix[k][j]);
            }
        }
    }

    // Copy the updated co-efficients with applied effect into interpolated data
    pData->c_tab.c[0] = out[0][0];
    pData->c_tab.c[1] = out[0][1];
    pData->c_tab.c[2] = out[0][2];
    pData->c_tab.c[3] = out[1][0];
    pData->c_tab.c[4] = out[1][1];
    pData->c_tab.c[5] = out[1][2];
    pData->c_tab.c[6] = out[2][0];
    pData->c_tab.c[7] = out[2][1];
    pData->c_tab.c[8] = out[2][2];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::CC13CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::CC13CalculateSetting(
    const CC13InputData* pInput,
    VOID*                pOEMIQData,
    CC13OutputData*      pOutput)
{
    CamxResult                                          result             = CamxResultSuccess;
    BOOL                                                commonIQResult     = FALSE;
    cc_1_3_0::cc13_rgn_dataType*                        pInterpolationData = NULL;
    cc_1_3_0::chromatix_cc13_reserveType*               pReserveType       = NULL;
    cc_1_3_0::chromatix_cc13Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<cc_1_3_0::cc13_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult     = IQInterface::s_interpolationTable.CC13Interpolation(pInput, pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_cc13_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            // Apply effect only for non-OEM case since interpolation data is modified by effect matrix.
            if ((PipelineType::IPE == pOutput->type) && (DefaultSaturation != pInput->saturation))
            {
                CC13ApplyEffect(pInput, pInterpolationData);
            }
        }
        else
        {
            if (PipelineType::BPS == pOutput->type)
            {
                OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);

                pInterpolationData = reinterpret_cast<cc_1_3_0::cc13_rgn_dataType*>(&(pOEMInput->CCSetting));

                if (NULL != pInterpolationData)
                {
                    commonIQResult = TRUE;
                    pReserveType   = reinterpret_cast<cc_1_3_0::chromatix_cc13_reserveType*>(
                                         &(pOEMInput->CCReserveType));
                    pModuleEnable  = reinterpret_cast<cc_1_3_0::chromatix_cc13Type::enable_sectionStruct*>(
                                         &(pOEMInput->CCEnableSection));
                }
                else
                {
                    result = CamxResultEFailed;
                    CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
                }
            }
            else if (PipelineType::IPE == pOutput->type)
            {
                OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

                pInterpolationData = reinterpret_cast<cc_1_3_0::cc13_rgn_dataType*>(&(pOEMInput->CCSetting));

                if (NULL != pInterpolationData)
                {
                    commonIQResult = TRUE;
                    pReserveType   = reinterpret_cast<cc_1_3_0::chromatix_cc13_reserveType*>(
                                         &(pOEMInput->CCReserveType));
                    pModuleEnable  = reinterpret_cast<cc_1_3_0::chromatix_cc13Type::enable_sectionStruct*>(
                                         &(pOEMInput->CCEnableSection));
                }
                else
                {
                    result = CamxResultEFailed;
                    CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
                }
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for CC1_3", pOutput->type);
            }
        }

        if (TRUE == commonIQResult)
        {
            CC13UnpackedField unpackedRegData = { 0 };

            commonIQResult =
                IQInterface::s_interpolationTable.CC13CalculateHWSetting(pInput,
                                                                         pInterpolationData,
                                                                         pReserveType,
                                                                         pModuleEnable,
                                                                         static_cast<VOID*>(&unpackedRegData));

            if (TRUE == commonIQResult)
            {
                if (PipelineType::BPS == pOutput->type)
                {
                    BPSSetupCC13Reg(&unpackedRegData, pOutput->regCommand.BPS.pRegCmd);
                }
                else if (PipelineType::IPE == pOutput->type)
                {
                    IPESetupCC13Reg(&unpackedRegData, pOutput->regCommand.IPE.pRegCmd);
                }
                else
                {
                    CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for CC1_3", pOutput->type);
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("CC13 calculate hardware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("CC13 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput  %p  ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IPEGamma15CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPEGamma15CalculateSetting(
    const Gamma15InputData* pInput,
    VOID*                   pOEMIQData,
    Gamma15OutputData*      pOutput)
{
    CamxResult                                                result                = CamxResultSuccess;
    BOOL                                                      commonIQResult        = FALSE;
    gamma_1_5_0::mod_gamma15_cct_dataType::cct_dataStruct*    pInterpolationData    = NULL;
    gamma_1_5_0::mod_gamma15_channel_dataType                 channelData[GammaLUTMax];
    gamma_1_5_0::chromatix_gamma15Type::enable_sectionStruct* pModuleEnable         = NULL;

    // Check pInput->pInterpolationData also before dereferencing it.
    if ((NULL != pInput) && (NULL != pOutput) && (NULL != pInput->pInterpolationData))
    {
        Gamma15UnpackedField unpackedRegisterData = { 0 };

        // set the unpacked register data LUT pointers to DMI memory buffer
        unpackedRegisterData.gLUTInConfig.pGammaTable = pOutput->pLUT[GammaLUTChannel0];
        unpackedRegisterData.gLUTInConfig.numEntries  = Gamma15LUTNumEntriesPerChannel;
        unpackedRegisterData.bLUTInConfig.pGammaTable = pOutput->pLUT[GammaLUTChannel1];
        unpackedRegisterData.bLUTInConfig.numEntries  = Gamma15LUTNumEntriesPerChannel;
        unpackedRegisterData.rLUTInConfig.pGammaTable = pOutput->pLUT[GammaLUTChannel2];
        unpackedRegisterData.rLUTInConfig.numEntries  = Gamma15LUTNumEntriesPerChannel;

        pInterpolationData =
            static_cast<gamma_1_5_0::mod_gamma15_cct_dataType::cct_dataStruct*>(pInput->pInterpolationData);
        if (NULL != pInterpolationData)
        {
            if (NULL == pOEMIQData)
            {
                // allocate memory for channel data
                pInterpolationData->mod_gamma15_channel_data  = &channelData[0];
                pModuleEnable      = &(pInput->pChromatix->enable_section);

                // Call the Interpolation Calculation
                commonIQResult = IQInterface::s_interpolationTable.gamma15Interpolation(pInput, pInterpolationData);
            }
            else
            {
                OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

                pInterpolationData->mod_gamma15_channel_dataID    =
                    pOEMInput->GammaSetting.cct_data.mod_gamma15_channel_dataID;
                pInterpolationData->mod_gamma15_channel_dataCount =
                    pOEMInput->GammaSetting.cct_data.mod_gamma15_channel_dataCount;
                // Allocate memory
                pInterpolationData->mod_gamma15_channel_data = &channelData[0];

                Utils::Memcpy(channelData,
                    pOEMInput->GammaSetting.cct_data.mod_gamma15_channel_data,
                    sizeof(pOEMInput->GammaSetting.cct_data.mod_gamma15_channel_data));

                commonIQResult = TRUE;
                pModuleEnable  = reinterpret_cast<gamma_1_5_0::chromatix_gamma15Type::enable_sectionStruct*>(
                    &(pOEMInput->GammaEnableSection));
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Null Interpolation data");
        }

        if (TRUE == commonIQResult)
        {
            if (pOutput->type == PipelineType::IPE)
            {
                IQInterface::s_interpolationTable.gamma15CalculateHWSetting(pInput,
                                                                            pInterpolationData,
                                                                            pModuleEnable,
                                                                            static_cast<VOID*>(&unpackedRegisterData));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for Gamma1_5", pOutput->type);
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Gamma1_5 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL %p %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupGIC30Register
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupGIC30Register(
    const GIC30UnpackedField* pData,
    GIC30OutputData*          pOutput)
{
    if ((NULL != pData) && (NULL != pOutput))
    {
        pOutput->pModuleConfig->moduleConfig.bitfields.EN            = pData->enable;
        pOutput->pModuleConfig->moduleConfig.bitfields.GIC_EN        = pData->enableGIC;
        pOutput->pModuleConfig->moduleConfig.bitfields.PNR_EN        = pData->enablePNR;

        pOutput->pRegCmd->filterConfig.bitfields.GIC_FILTER_STRENGTH = pData->GICStrength;
        pOutput->pRegCmd->filterConfig.bitfields.GIC_NOISE_SCALE     = pData->GICNoiseScale;

        pOutput->pRegCmd->horizontalVerticalOffset.bitfields.BX      = pData->bx;
        pOutput->pRegCmd->horizontalVerticalOffset.bitfields.BY      = pData->by;

        pOutput->pRegCmd->lineNoiseOffset.bitfields.VALUE            = pData->thinLineNoiseOffset;

        pOutput->pRegCmd->anchorBaseSetting0.bitfields.ANCHOR_TABLE  = pData->RNRAnchor[0];
        pOutput->pRegCmd->anchorBaseSetting0.bitfields.BASE_TABLE    = pData->RNRBase[0];
        pOutput->pRegCmd->anchorBaseSetting1.bitfields.ANCHOR_TABLE  = pData->RNRAnchor[1];
        pOutput->pRegCmd->anchorBaseSetting1.bitfields.BASE_TABLE    = pData->RNRBase[1];
        pOutput->pRegCmd->anchorBaseSetting2.bitfields.ANCHOR_TABLE  = pData->RNRAnchor[2];
        pOutput->pRegCmd->anchorBaseSetting2.bitfields.BASE_TABLE    = pData->RNRBase[2];
        pOutput->pRegCmd->anchorBaseSetting3.bitfields.ANCHOR_TABLE  = pData->RNRAnchor[3];
        pOutput->pRegCmd->anchorBaseSetting3.bitfields.BASE_TABLE    = pData->RNRBase[3];
        pOutput->pRegCmd->anchorBaseSetting4.bitfields.ANCHOR_TABLE  = pData->RNRAnchor[4];
        pOutput->pRegCmd->anchorBaseSetting4.bitfields.BASE_TABLE    = pData->RNRBase[4];
        pOutput->pRegCmd->anchorBaseSetting5.bitfields.ANCHOR_TABLE  = pData->RNRAnchor[5];
        pOutput->pRegCmd->anchorBaseSetting5.bitfields.BASE_TABLE    = pData->RNRBase[5];

        pOutput->pRegCmd->slopeShiftSetting0.bitfields.SHIFT_TABLE   = pData->RNRShift[0];
        pOutput->pRegCmd->slopeShiftSetting0.bitfields.SLOPE_TABLE   = pData->RNRSlope[0];
        pOutput->pRegCmd->slopeShiftSetting1.bitfields.SHIFT_TABLE   = pData->RNRShift[1];
        pOutput->pRegCmd->slopeShiftSetting1.bitfields.SLOPE_TABLE   = pData->RNRSlope[1];
        pOutput->pRegCmd->slopeShiftSetting2.bitfields.SHIFT_TABLE   = pData->RNRShift[2];
        pOutput->pRegCmd->slopeShiftSetting2.bitfields.SLOPE_TABLE   = pData->RNRSlope[2];
        pOutput->pRegCmd->slopeShiftSetting3.bitfields.SHIFT_TABLE   = pData->RNRShift[3];
        pOutput->pRegCmd->slopeShiftSetting3.bitfields.SLOPE_TABLE   = pData->RNRSlope[3];
        pOutput->pRegCmd->slopeShiftSetting4.bitfields.SHIFT_TABLE   = pData->RNRShift[4];
        pOutput->pRegCmd->slopeShiftSetting4.bitfields.SLOPE_TABLE   = pData->RNRSlope[4];
        pOutput->pRegCmd->slopeShiftSetting5.bitfields.SHIFT_TABLE   = pData->RNRShift[5];
        pOutput->pRegCmd->slopeShiftSetting5.bitfields.SLOPE_TABLE   = pData->RNRSlope[5];

        pOutput->pRegCmd->noiseRatioFilterStrength.bitfields.VALUE   = pData->PNRStrength;
        pOutput->pRegCmd->noiseRatioScale0.bitfields.R_SQUARE_SCALE  = pData->PNRNoiseScale[0];
        pOutput->pRegCmd->noiseRatioScale1.bitfields.R_SQUARE_SCALE  = pData->PNRNoiseScale[1];
        pOutput->pRegCmd->noiseRatioScale2.bitfields.R_SQUARE_SCALE  = pData->PNRNoiseScale[2];
        pOutput->pRegCmd->noiseRatioScale3.bitfields.R_SQUARE_SCALE  = pData->PNRNoiseScale[3];

        pOutput->pRegCmd->scaleShift.bitfields.R_SQUARE_SCALE        = pData->rSquareScale;
        pOutput->pRegCmd->scaleShift.bitfields.R_SQUARE_SHIFT        = pData->rSquareShift;
        pOutput->pRegCmd->squareInit.bitfields.VALUE                 = pData->rSquareInit;

        Utils::Memcpy(pOutput->pDMIData,
                      &pData->noiseStdLUTLevel0[pData->LUTBankSel][0],
                      DMIRAM_GIC_NOISESTD_LENGTH_V30 * sizeof(UINT32));
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid data's %p %p", pOutput, pData);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupHDR20Reg
///
/// @brief  Setup HDR20 Reg based on common library output
///
/// @param  pData    Pointer to the Common Library Calculation Result
/// @param  pRegCmd  Pointer to the HDR20 Output
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupHDR20Reg(
    const HDR20UnpackedField*  pData,
    IFEHDR20RegCmd*            pRegCmd)
{
    IFEHDR20RegCmd1* pRegCmd1 = NULL;
    IFEHDR20RegCmd2* pRegCmd2 = NULL;
    IFEHDR20RegCmd3* pRegCmd3 = NULL;

    if ((NULL != pData) && (NULL != pRegCmd))
    {

        pRegCmd1 = &(pRegCmd->m_regCmd1);
        pRegCmd2 = &(pRegCmd->m_regCmd2);
        pRegCmd3 = &(pRegCmd->m_regCmd3);

        pRegCmd1->configRegister0.bitfields.EXP_RATIO                    = pData->hdr_exp_ratio;
        pRegCmd1->configRegister0.bitfields.RECON_FIRST_FIELD            = pData->hdr_first_field;

        pRegCmd1->configRegister1.bitfields.RG_WB_GAIN_RATIO             = pData->hdr_rg_wb_gain_ratio;
        pRegCmd1->configRegister2.bitfields.BG_WB_GAIN_RATIO             = pData->hdr_bg_wb_gain_ratio;
        pRegCmd1->configRegister3.bitfields.GR_WB_GAIN_RATIO             = pData->hdr_gr_wb_gain_ratio;
        pRegCmd1->configRegister4.bitfields.GB_WB_GAIN_RATIO             = pData->hdr_gb_wb_gain_ratio;

        pRegCmd1->macConfigRegister0.bitfields.MAC_MOTION_0_TH1          = pData->hdr_mac_motion0_th1;
        pRegCmd1->macConfigRegister0.bitfields.MAC_MOTION_0_TH2          = pData->hdr_mac_motion0_th2;
        pRegCmd1->macConfigRegister0.bitfields.R_MAC_MOTION_0_TH1        = pData->hdr_mac_motion0_th1;

        pRegCmd1->macConfigRegister1.bitfields.MAC_DILATION               = pData->hdr_mac_dilation;
        pRegCmd1->macConfigRegister1.bitfields.MAC_SQRT_ANALOG_GAIN       = pData->hdr_mac_sqrt_analog_gain;
        pRegCmd1->macConfigRegister1.bitfields.R_MAC_MOTION_0_TH2         = pData->hdr_mac_motion0_th2;
        pRegCmd1->macConfigRegister1.bitfields.R_MAC_SQRT_ANALOG_GAIN     = pData->hdr_mac_sqrt_analog_gain;

        pRegCmd1->macConfigRegister2.bitfields.MAC_MOTION_0_DT0           = pData->hdr_mac_motion0_dt0;
        pRegCmd1->macConfigRegister2.bitfields.MAC_MOTION_STRENGTH        = pData->hdr_mac_motion_strength;
        pRegCmd1->macConfigRegister2.bitfields.R_MAC_MOTION_0_DT0         = pData->hdr_mac_motion0_dt0;
        pRegCmd1->macConfigRegister2.bitfields.R_MAC_MOTION_STRENGTH      = pData->hdr_mac_motion_strength;

        pRegCmd1->macConfigRegister3.bitfields.MAC_LOW_LIGHT_TH1          = pData->hdr_mac_lowlight_th1;
        pRegCmd1->macConfigRegister3.bitfields.R_MAC_LOW_LIGHT_TH1        = pData->hdr_mac_lowlight_th1;

        pRegCmd1->macConfigRegister4.bitfields.MAC_HIGH_LIGHT_DTH_LOG2    = pData->hdr_mac_hilight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.MAC_LOW_LIGHT_DTH_LOG2     = pData->hdr_mac_lowlight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.MAC_LOW_LIGHT_STRENGTH     = pData->hdr_mac_lowlight_strength;
        pRegCmd1->macConfigRegister4.bitfields.R_MAC_HIGH_LIGHT_DTH_LOG2  = pData->hdr_mac_hilight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.R_MAC_LOW_LIGHT_DTH_LOG2   = pData->hdr_mac_lowlight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.R_MAC_LOW_LIGHT_STRENGTH   = pData->hdr_mac_lowlight_strength;

        pRegCmd1->macConfigRegister5.bitfields.MAC_HIGH_LIGHT_TH1         = pData->hdr_mac_hilight_th1;
        pRegCmd1->macConfigRegister5.bitfields.R_MAC_HIGH_LIGHT_TH1       = pData->hdr_mac_hilight_th1;

        pRegCmd1->macConfigRegister6.bitfields.MAC_SMOOTH_DTH_LOG2        = pData->hdr_mac_smooth_dth_log2;
        pRegCmd1->macConfigRegister6.bitfields.MAC_SMOOTH_ENABLE          = pData->hdr_mac_smooth_filter_en;
        pRegCmd1->macConfigRegister6.bitfields.MAC_SMOOTH_TH1             = pData->hdr_mac_smooth_th1;
        pRegCmd1->macConfigRegister6.bitfields.R_MAC_SMOOTH_DTH_LOG2      = pData->hdr_mac_smooth_dth_log2;
        pRegCmd1->macConfigRegister6.bitfields.R_MAC_SMOOTH_TH1           = pData->hdr_mac_smooth_th1;

        pRegCmd1->macConfigRegister7.bitfields.EXP_RATIO_RECIP            = pData->hdr_exp_ratio_recip;
        pRegCmd1->macConfigRegister7.bitfields.MAC_LINEAR_MODE            = pData->hdr_mac_linear_mode;
        pRegCmd1->macConfigRegister7.bitfields.MAC_SMOOTH_TAP0            = pData->hdr_mac_smooth_tap0;
        pRegCmd1->macConfigRegister7.bitfields.MSB_ALIGNED                = pData->hdr_MSB_align;
        pRegCmd1->macConfigRegister7.bitfields.R_MAC_SMOOTH_TAP0          = pData->hdr_mac_smooth_tap0;

        pRegCmd1->reconstructionConfig0.bitfields.RECON_H_EDGE_DTH_LOG2   = pData->hdr_rec_h_edge_dth_log2;
        pRegCmd1->reconstructionConfig0.bitfields.RECON_H_EDGE_TH1        = pData->hdr_rec_h_edge_th1;
        pRegCmd1->reconstructionConfig0.bitfields.RECON_MOTION_DTH_LOG2   = pData->hdr_rec_motion_dth_log2;
        pRegCmd1->reconstructionConfig0.bitfields.RECON_MOTION_TH1        = pData->hdr_rec_motion_th1;

        pRegCmd1->reconstructionConfig1.bitfields.RECON_DARK_DTH_LOG2     = pData->hdr_rec_dark_dth_log2;
        pRegCmd1->reconstructionConfig1.bitfields.RECON_DARK_TH1          = pData->hdr_rec_dark_th1;
        pRegCmd1->reconstructionConfig1.bitfields.RECON_EDGE_LPF_TAP0     = pData->hdr_rec_edge_lpf_tap0;
        pRegCmd1->reconstructionConfig1.bitfields.RECON_FLAT_REGION_TH    = pData->hdr_rec_flat_region_th;

        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_H_EDGE_DTH_LOG2 = pData->hdr_rec_h_edge_dth_log2;
        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_H_EDGE_TH1      = pData->hdr_rec_h_edge_th1;
        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_MOTION_DTH_LOG2 = pData->hdr_rec_motion_dth_log2;
        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_MOTION_TH1      = pData->hdr_rec_motion_th1;

        pRegCmd1->reconstructionConfig3.bitfields.RECON_LINEAR_MODE       = pData->hdr_rec_linear_mode;
        pRegCmd1->reconstructionConfig3.bitfields.RECON_MIN_FACTOR        = pData->hdr_rec_min_factor;
        pRegCmd1->reconstructionConfig3.bitfields.R_RECON_DARK_DTH_LOG2   = pData->hdr_rec_dark_dth_log2;
        pRegCmd1->reconstructionConfig3.bitfields.R_RECON_DARK_TH1        = pData->hdr_rec_dark_th1;
        pRegCmd1->reconstructionConfig3.bitfields.R_RECON_MIN_FACTOR      = pData->hdr_rec_min_factor;

        pRegCmd1->reconstructionConfig4.bitfields.R_RECON_FLAT_REGION_TH  = pData->hdr_rec_flat_region_th;

        pRegCmd2->configRegister5.bitfields.BLK_IN                        = pData->hdr_black_in;
        pRegCmd2->configRegister5.bitfields.BLK_OUT                       = pData->hdr_black_out;

        pRegCmd3->reconstructionConfig5.bitfields.ZREC_ENABLE             = pData->hdr_recon_en;
        pRegCmd3->reconstructionConfig5.bitfields.ZREC_FIRST_RB_EXP       = pData->hdr_first_field;
        pRegCmd3->reconstructionConfig5.bitfields.ZREC_PATTERN            = pData->hdr_zrec_pattern;
        pRegCmd3->reconstructionConfig5.bitfields.ZREC_PREFILT_TAP0       = pData->hdr_zrec_prefilt_tap0;

        pRegCmd3->reconstructionConfig6.bitfields.ZREC_G_GRAD_TH1         = pData->hdr_zrec_g_grad_th1;
        pRegCmd3->reconstructionConfig6.bitfields.ZREC_G_DTH_LOG2         = pData->hdr_zrec_g_grad_dth_log2;
        pRegCmd3->reconstructionConfig6.bitfields.ZREC_RB_GRAD_TH1        = pData->hdr_zrec_rb_grad_th1;
        pRegCmd3->reconstructionConfig6.bitfields.ZREC_RB_DTH_LOG2        = pData->hdr_zrec_rb_grad_dth_log2;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE(" Input data is NULL ");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupHDR22Reg
///
/// @brief  Setup HDR22 Reg based on common library output
///
/// @param  pData    Pointer to the Common Library Calculation Result
/// @param  pRegCmd  Pointer to the HDR22 Output
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupHDR22Reg(
    const HDR22UnpackedField*  pData,
    HDR22OutputData*    pOutput)
{
    IFEHDR22RegCmd1* pRegCmd1 = NULL;
    IFEHDR22RegCmd2* pRegCmd2 = NULL;
    IFEHDR22RegCmd3* pRegCmd3 = NULL;

    if ((NULL != pData) && (NULL != pOutput))
    {
        IFEHDR22RegCmd* pRegCmd   = pOutput->IFE.pRegCmd;

        pRegCmd1 = &(pRegCmd->m_regCmd1);
        pRegCmd2 = &(pRegCmd->m_regCmd2);
        pRegCmd3 = &(pRegCmd->m_regCmd3);

        pRegCmd1->configRegister0.bitfields.EXP_RATIO                    = pData->hdr_exp_ratio;
        pRegCmd1->configRegister0.bitfields.RECON_FIRST_FIELD            = pData->hdr_first_field;

        pRegCmd1->configRegister1.bitfields.RG_WB_GAIN_RATIO             = pData->hdr_rg_wb_gain_ratio;
        pRegCmd1->configRegister2.bitfields.BG_WB_GAIN_RATIO             = pData->hdr_bg_wb_gain_ratio;
        pRegCmd1->configRegister3.bitfields.GR_WB_GAIN_RATIO             = pData->hdr_gr_wb_gain_ratio;
        pRegCmd1->configRegister4.bitfields.GB_WB_GAIN_RATIO             = pData->hdr_gb_wb_gain_ratio;

        pRegCmd1->macConfigRegister0.bitfields.MAC_MOTION_0_TH1          = pData->hdr_mac_motion0_th1;
        pRegCmd1->macConfigRegister0.bitfields.MAC_MOTION_0_TH2          = pData->hdr_mac_motion0_th2;
        pRegCmd1->macConfigRegister0.bitfields.R_MAC_MOTION_0_TH1        = pData->hdr_mac_motion0_th1;

        pRegCmd1->macConfigRegister1.bitfields.MAC_DILATION               = pData->hdr_mac_dilation;
        pRegCmd1->macConfigRegister1.bitfields.MAC_SQRT_ANALOG_GAIN       = pData->hdr_mac_sqrt_analog_gain;
        pRegCmd1->macConfigRegister1.bitfields.R_MAC_MOTION_0_TH2         = pData->hdr_mac_motion0_th2;
        pRegCmd1->macConfigRegister1.bitfields.R_MAC_SQRT_ANALOG_GAIN     = pData->hdr_mac_sqrt_analog_gain;

        pRegCmd1->macConfigRegister2.bitfields.MAC_MOTION_0_DT0           = pData->hdr_mac_motion0_dt0;
        pRegCmd1->macConfigRegister2.bitfields.MAC_MOTION_STRENGTH        = pData->hdr_mac_motion_strength;
        pRegCmd1->macConfigRegister2.bitfields.R_MAC_MOTION_0_DT0         = pData->hdr_mac_motion0_dt0;
        pRegCmd1->macConfigRegister2.bitfields.R_MAC_MOTION_STRENGTH      = pData->hdr_mac_motion_strength;

        pRegCmd1->macConfigRegister3.bitfields.MAC_LOW_LIGHT_TH1          = pData->hdr_mac_lowlight_th1;
        pRegCmd1->macConfigRegister3.bitfields.R_MAC_LOW_LIGHT_TH1        = pData->hdr_mac_lowlight_th1;

        pRegCmd1->macConfigRegister4.bitfields.MAC_HIGH_LIGHT_DTH_LOG2    = pData->hdr_mac_hilight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.MAC_LOW_LIGHT_DTH_LOG2     = pData->hdr_mac_lowlight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.MAC_LOW_LIGHT_STRENGTH     = pData->hdr_mac_lowlight_strength;
        pRegCmd1->macConfigRegister4.bitfields.R_MAC_HIGH_LIGHT_DTH_LOG2  = pData->hdr_mac_hilight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.R_MAC_LOW_LIGHT_DTH_LOG2   = pData->hdr_mac_lowlight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.R_MAC_LOW_LIGHT_STRENGTH   = pData->hdr_mac_lowlight_strength;

        pRegCmd1->macConfigRegister5.bitfields.MAC_HIGH_LIGHT_TH1         = pData->hdr_mac_hilight_th1;
        pRegCmd1->macConfigRegister5.bitfields.R_MAC_HIGH_LIGHT_TH1       = pData->hdr_mac_hilight_th1;

        pRegCmd1->macConfigRegister6.bitfields.MAC_SMOOTH_DTH_LOG2        = pData->hdr_mac_smooth_dth_log2;
        pRegCmd1->macConfigRegister6.bitfields.MAC_SMOOTH_ENABLE          = pData->hdr_mac_smooth_filter_en;
        pRegCmd1->macConfigRegister6.bitfields.MAC_SMOOTH_TH1             = pData->hdr_mac_smooth_th1;
        pRegCmd1->macConfigRegister6.bitfields.R_MAC_SMOOTH_DTH_LOG2      = pData->hdr_mac_smooth_dth_log2;
        pRegCmd1->macConfigRegister6.bitfields.R_MAC_SMOOTH_TH1           = pData->hdr_mac_smooth_th1;

        pRegCmd1->macConfigRegister7.bitfields.EXP_RATIO_RECIP            = pData->hdr_exp_ratio_recip;
        pRegCmd1->macConfigRegister7.bitfields.MAC_LINEAR_MODE            = pData->hdr_mac_linear_mode;
        pRegCmd1->macConfigRegister7.bitfields.MAC_SMOOTH_TAP0            = pData->hdr_mac_smooth_tap0;
        pRegCmd1->macConfigRegister7.bitfields.MSB_ALIGNED                = pData->hdr_MSB_align;
        pRegCmd1->macConfigRegister7.bitfields.R_MAC_SMOOTH_TAP0          = pData->hdr_mac_smooth_tap0;

        pRegCmd1->reconstructionConfig0.bitfields.RECON_H_EDGE_DTH_LOG2   = pData->hdr_rec_h_edge_dth_log2;
        pRegCmd1->reconstructionConfig0.bitfields.RECON_H_EDGE_TH1        = pData->hdr_rec_h_edge_th1;
        pRegCmd1->reconstructionConfig0.bitfields.RECON_MOTION_DTH_LOG2   = pData->hdr_rec_motion_dth_log2;
        pRegCmd1->reconstructionConfig0.bitfields.RECON_MOTION_TH1        = pData->hdr_rec_motion_th1;

        pRegCmd1->reconstructionConfig1.bitfields.RECON_DARK_DTH_LOG2     = pData->hdr_rec_dark_dth_log2;
        pRegCmd1->reconstructionConfig1.bitfields.RECON_DARK_TH1          = pData->hdr_rec_dark_th1;
        pRegCmd1->reconstructionConfig1.bitfields.RECON_EDGE_LPF_TAP0     = pData->hdr_rec_edge_lpf_tap0;
        pRegCmd1->reconstructionConfig1.bitfields.RECON_FLAT_REGION_TH    = pData->hdr_rec_flat_region_th;

        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_H_EDGE_DTH_LOG2 = pData->hdr_rec_h_edge_dth_log2;
        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_H_EDGE_TH1      = pData->hdr_rec_h_edge_th1;
        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_MOTION_DTH_LOG2 = pData->hdr_rec_motion_dth_log2;
        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_MOTION_TH1      = pData->hdr_rec_motion_th1;

        pRegCmd1->reconstructionConfig3.bitfields.RECON_LINEAR_MODE       = pData->hdr_rec_linear_mode;
        pRegCmd1->reconstructionConfig3.bitfields.RECON_MIN_FACTOR        = pData->hdr_rec_min_factor;
        pRegCmd1->reconstructionConfig3.bitfields.R_RECON_DARK_DTH_LOG2   = pData->hdr_rec_dark_dth_log2;
        pRegCmd1->reconstructionConfig3.bitfields.R_RECON_DARK_TH1        = pData->hdr_rec_dark_th1;
        pRegCmd1->reconstructionConfig3.bitfields.R_RECON_MIN_FACTOR      = pData->hdr_rec_min_factor;

        pRegCmd1->reconstructionConfig4.bitfields.R_RECON_FLAT_REGION_TH  = pData->hdr_rec_flat_region_th;

        pRegCmd2->configRegister5.bitfields.BLK_IN                        = pData->hdr_black_in;
        pRegCmd2->configRegister5.bitfields.BLK_OUT                       = pData->hdr_black_out;

        pRegCmd3->reconstructionConfig5.bitfields.ZREC_ENABLE             = pData->hdr_recon_en;
        pRegCmd3->reconstructionConfig5.bitfields.ZREC_FIRST_RB_EXP       = pData->hdr_first_field;
        pRegCmd3->reconstructionConfig5.bitfields.ZREC_PATTERN            = pData->hdr_zrec_pattern;
        pRegCmd3->reconstructionConfig5.bitfields.ZREC_PREFILT_TAP0       = pData->hdr_zrec_prefilt_tap0;

        pRegCmd3->reconstructionConfig6.bitfields.ZREC_G_GRAD_TH1         = pData->hdr_zrec_g_grad_th1;
        pRegCmd3->reconstructionConfig6.bitfields.ZREC_G_DTH_LOG2         = pData->hdr_zrec_g_grad_dth_log2;
        pRegCmd3->reconstructionConfig6.bitfields.ZREC_RB_GRAD_TH1        = pData->hdr_zrec_rb_grad_th1;
        pRegCmd3->reconstructionConfig6.bitfields.ZREC_RB_DTH_LOG2        = pData->hdr_zrec_rb_grad_dth_log2;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE(" Input data is NULL ");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupHDR23Reg
///
/// @brief  Setup HDR23 Reg based on common library output
///
/// @param  pData    Pointer to the Common Library Calculation Result
/// @param  pRegCmd  Pointer to the HDR23 Output
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupHDR23Reg(
    const HDR23UnpackedField*  pData,
    IFEHDR23RegCmd*            pRegCmd)
{
    IFEHDR23RegCmd1* pRegCmd1 = NULL;
    IFEHDR23RegCmd2* pRegCmd2 = NULL;
    IFEHDR23RegCmd3* pRegCmd3 = NULL;

    if ((NULL != pData) && (NULL != pRegCmd))
    {

        pRegCmd1 = &(pRegCmd->m_regCmd1);
        pRegCmd2 = &(pRegCmd->m_regCmd2);
        pRegCmd3 = &(pRegCmd->m_regCmd3);

        pRegCmd1->configRegister0.bitfields.EXP_RATIO         = pData->hdr_exp_ratio;
        pRegCmd1->configRegister0.bitfields.RECON_FIRST_FIELD = pData->hdr_first_field;

        pRegCmd1->configRegister1.bitfields.RG_WB_GAIN_RATIO = pData->hdr_rg_wb_gain_ratio;
        pRegCmd1->configRegister2.bitfields.BG_WB_GAIN_RATIO = pData->hdr_bg_wb_gain_ratio;
        pRegCmd1->configRegister3.bitfields.GR_WB_GAIN_RATIO = pData->hdr_gr_wb_gain_ratio;
        pRegCmd1->configRegister4.bitfields.GB_WB_GAIN_RATIO = pData->hdr_gb_wb_gain_ratio;

        pRegCmd1->macConfigRegister0.bitfields.MAC_MOTION_0_TH1   = pData->hdr_mac_motion0_th1;
        pRegCmd1->macConfigRegister0.bitfields.MAC_MOTION_0_TH2   = pData->hdr_mac_motion0_th2;
        pRegCmd1->macConfigRegister0.bitfields.R_MAC_MOTION_0_TH1 = pData->hdr_mac_motion0_th1;

        pRegCmd1->macConfigRegister1.bitfields.MAC_DILATION           = pData->hdr_mac_dilation;
        pRegCmd1->macConfigRegister1.bitfields.MAC_SQRT_ANALOG_GAIN   = pData->hdr_mac_sqrt_analog_gain;
        pRegCmd1->macConfigRegister1.bitfields.R_MAC_MOTION_0_TH2     = pData->hdr_mac_motion0_th2;
        pRegCmd1->macConfigRegister1.bitfields.R_MAC_SQRT_ANALOG_GAIN = pData->hdr_mac_sqrt_analog_gain;

        pRegCmd1->macConfigRegister2.bitfields.MAC_MOTION_0_DT0      = pData->hdr_mac_motion0_dt0;
        pRegCmd1->macConfigRegister2.bitfields.MAC_MOTION_STRENGTH   = pData->hdr_mac_motion_strength;
        pRegCmd1->macConfigRegister2.bitfields.R_MAC_MOTION_0_DT0    = pData->hdr_mac_motion0_dt0;
        pRegCmd1->macConfigRegister2.bitfields.R_MAC_MOTION_STRENGTH = pData->hdr_mac_motion_strength;

        pRegCmd1->macConfigRegister3.bitfields.MAC_LOW_LIGHT_TH1   = pData->hdr_mac_lowlight_th1;
        pRegCmd1->macConfigRegister3.bitfields.R_MAC_LOW_LIGHT_TH1 = pData->hdr_mac_lowlight_th1;

        pRegCmd1->macConfigRegister4.bitfields.MAC_HIGH_LIGHT_DTH_LOG2   = pData->hdr_mac_hilight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.MAC_LOW_LIGHT_DTH_LOG2    = pData->hdr_mac_lowlight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.MAC_LOW_LIGHT_STRENGTH    = pData->hdr_mac_lowlight_strength;
        pRegCmd1->macConfigRegister4.bitfields.R_MAC_HIGH_LIGHT_DTH_LOG2 = pData->hdr_mac_hilight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.R_MAC_LOW_LIGHT_DTH_LOG2  = pData->hdr_mac_lowlight_dth_log2;
        pRegCmd1->macConfigRegister4.bitfields.R_MAC_LOW_LIGHT_STRENGTH  = pData->hdr_mac_lowlight_strength;

        pRegCmd1->macConfigRegister5.bitfields.MAC_HIGH_LIGHT_TH1   = pData->hdr_mac_hilight_th1;
        pRegCmd1->macConfigRegister5.bitfields.R_MAC_HIGH_LIGHT_TH1 = pData->hdr_mac_hilight_th1;

        pRegCmd1->macConfigRegister6.bitfields.MAC_SMOOTH_DTH_LOG2   = pData->hdr_mac_smooth_dth_log2;
        pRegCmd1->macConfigRegister6.bitfields.MAC_SMOOTH_ENABLE     = pData->hdr_mac_smooth_filter_en;
        pRegCmd1->macConfigRegister6.bitfields.MAC_SMOOTH_TH1        = pData->hdr_mac_smooth_th1;
        pRegCmd1->macConfigRegister6.bitfields.R_MAC_SMOOTH_DTH_LOG2 = pData->hdr_mac_smooth_dth_log2;
        pRegCmd1->macConfigRegister6.bitfields.R_MAC_SMOOTH_TH1      = pData->hdr_mac_smooth_th1;

        pRegCmd1->macConfigRegister7.bitfields.EXP_RATIO_RECIP   = pData->hdr_exp_ratio_recip;
        pRegCmd1->macConfigRegister7.bitfields.MAC_LINEAR_MODE   = pData->hdr_mac_linear_mode;
        pRegCmd1->macConfigRegister7.bitfields.MAC_SMOOTH_TAP0   = pData->hdr_mac_smooth_tap0;
        pRegCmd1->macConfigRegister7.bitfields.MSB_ALIGNED       = pData->hdr_MSB_align;
        pRegCmd1->macConfigRegister7.bitfields.R_MAC_SMOOTH_TAP0 = pData->hdr_mac_smooth_tap0;

        pRegCmd1->reconstructionConfig0.bitfields.RECON_H_EDGE_DTH_LOG2 = pData->hdr_rec_h_edge_dth_log2;
        pRegCmd1->reconstructionConfig0.bitfields.RECON_H_EDGE_TH1      = pData->hdr_rec_h_edge_th1;
        pRegCmd1->reconstructionConfig0.bitfields.RECON_MOTION_DTH_LOG2 = pData->hdr_rec_motion_dth_log2;
        pRegCmd1->reconstructionConfig0.bitfields.RECON_MOTION_TH1      = pData->hdr_rec_motion_th1;

        pRegCmd1->reconstructionConfig1.bitfields.RECON_DARK_DTH_LOG2  = pData->hdr_rec_dark_dth_log2;
        pRegCmd1->reconstructionConfig1.bitfields.RECON_DARK_TH1       = pData->hdr_rec_dark_th1;
        pRegCmd1->reconstructionConfig1.bitfields.RECON_EDGE_LPF_TAP0  = pData->hdr_rec_edge_lpf_tap0;
        pRegCmd1->reconstructionConfig1.bitfields.RECON_FLAT_REGION_TH = pData->hdr_rec_flat_region_th;

        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_H_EDGE_DTH_LOG2 = pData->hdr_rec_h_edge_dth_log2;
        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_H_EDGE_TH1      = pData->hdr_rec_h_edge_th1;
        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_MOTION_DTH_LOG2 = pData->hdr_rec_motion_dth_log2;
        pRegCmd1->reconstructionConfig2.bitfields.R_RECON_MOTION_TH1      = pData->hdr_rec_motion_th1;

        pRegCmd1->reconstructionConfig3.bitfields.RECON_LINEAR_MODE     = pData->hdr_rec_linear_mode;
        pRegCmd1->reconstructionConfig3.bitfields.RECON_MIN_FACTOR      = pData->hdr_rec_min_factor;
        pRegCmd1->reconstructionConfig3.bitfields.R_RECON_DARK_DTH_LOG2 = pData->hdr_rec_dark_dth_log2;
        pRegCmd1->reconstructionConfig3.bitfields.R_RECON_DARK_TH1      = pData->hdr_rec_dark_th1;
        pRegCmd1->reconstructionConfig3.bitfields.R_RECON_MIN_FACTOR    = pData->hdr_rec_min_factor;

        pRegCmd1->reconstructionConfig4.bitfields.R_RECON_FLAT_REGION_TH = pData->hdr_rec_flat_region_th;

        pRegCmd2->configRegister5.bitfields.BLK_IN  = pData->hdr_black_in;
        pRegCmd2->configRegister5.bitfields.BLK_OUT = pData->hdr_black_out;

        pRegCmd3->reconstructionConfig5.bitfields.ZREC_ENABLE       = pData->hdr_recon_en;
        pRegCmd3->reconstructionConfig5.bitfields.ZREC_FIRST_RB_EXP = pData->hdr_first_field;
        pRegCmd3->reconstructionConfig5.bitfields.ZREC_PATTERN      = pData->hdr_zrec_pattern;
        pRegCmd3->reconstructionConfig5.bitfields.ZREC_PREFILT_TAP0 = pData->hdr_zrec_prefilt_tap0;

        pRegCmd3->reconstructionConfig6.bitfields.ZREC_G_GRAD_TH1  = pData->hdr_zrec_g_grad_th1;
        pRegCmd3->reconstructionConfig6.bitfields.ZREC_G_DTH_LOG2  = pData->hdr_zrec_g_grad_dth_log2;
        pRegCmd3->reconstructionConfig6.bitfields.ZREC_RB_GRAD_TH1 = pData->hdr_zrec_rb_grad_th1;
        pRegCmd3->reconstructionConfig6.bitfields.ZREC_RB_DTH_LOG2 = pData->hdr_zrec_rb_grad_dth_log2;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE(" Input data is NULL ");
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupHNR10Register
///
/// @brief  Setup HNR10 Register based on common library output
///
/// @param  pRegVal    Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the HNR10 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupHNR10Register(
    const HNR10UnpackedField*  pData,
    HNR10OutputData*           pOutput)
{
    BPSHNR10RegConfig*  pRegCmd        = NULL;
    HnrParameters*      pHNRParameters = NULL;
    UINT                i              = 0;

    if ((NULL != pData) && (NULL != pOutput) && (NULL != pOutput->pRegCmd) && (NULL != pOutput->pHNRParameters))
    {
        pRegCmd        = pOutput->pRegCmd;
        pHNRParameters = pOutput->pHNRParameters;

        pRegCmd->faceCoefficient0.bitfields.FACE_CENTER_HORIZONTAL  = pData->face_center_horizontal[0];
        pRegCmd->faceCoefficient0.bitfields.FACE_CENTER_VERTICAL    = pData->face_center_vertical[0];
        pRegCmd->faceCoefficient1.bitfields.FACE_CENTER_HORIZONTAL  = pData->face_center_horizontal[1];
        pRegCmd->faceCoefficient1.bitfields.FACE_CENTER_VERTICAL    = pData->face_center_vertical[1];
        pRegCmd->faceCoefficient2.bitfields.FACE_CENTER_HORIZONTAL  = pData->face_center_horizontal[2];
        pRegCmd->faceCoefficient2.bitfields.FACE_CENTER_VERTICAL    = pData->face_center_vertical[2];
        pRegCmd->faceCoefficient3.bitfields.FACE_CENTER_HORIZONTAL  = pData->face_center_horizontal[3];
        pRegCmd->faceCoefficient3.bitfields.FACE_CENTER_VERTICAL    = pData->face_center_vertical[3];
        pRegCmd->faceCoefficient4.bitfields.FACE_CENTER_HORIZONTAL  = pData->face_center_horizontal[4];
        pRegCmd->faceCoefficient4.bitfields.FACE_CENTER_VERTICAL    = pData->face_center_vertical[4];

        pRegCmd->miscConfigCoefficient.bitfields.ABS_AMP_SHIFT      = pData->abs_amp_shift;
        pRegCmd->miscConfigCoefficient.bitfields.BLEND_CNR_ADJ_GAIN = pData->blend_cnr_adj_gain;
        pRegCmd->miscConfigCoefficient.bitfields.FNR_AC_SHIFT       = pData->fnr_ac_shift;
        pRegCmd->miscConfigCoefficient.bitfields.LNR_SHIFT          = pData->lnr_shift;

        pRegCmd->lpf3ConfigCoefficient.bitfields.LPF3_OFFSET        = pData->lpf3_offset;
        pRegCmd->lpf3ConfigCoefficient.bitfields.LPF3_PERCENT       = pData->lpf3_percent;
        pRegCmd->lpf3ConfigCoefficient.bitfields.LPF3_STRENGTH      = pData->lpf3_strength;

        pRegCmd->rnr_r_ScaleCoefficient.bitfields.RNR_R_SQUARE_SCALE = pData->rnr_r_square_scale;
        pRegCmd->rnr_r_ScaleCoefficient.bitfields.RNR_R_SQUARE_SHIFT = pData->rnr_r_square_shift;
        pRegCmd->rnr_r_squareCoefficient.bitfields.RNR_R_SQUARE_INIT = pData->rnr_r_square_init;

        pRegCmd->hnrCoefficient1.bitfields.RNR_BX = pData->rnr_bx;
        pRegCmd->hnrCoefficient1.bitfields.RNR_BY = pData->rnr_by;

        pRegCmd->rnrSlopeShiftCoefficient0.bitfields.RNR_SHIFT_0 = pData->rnr_shift[0];
        pRegCmd->rnrSlopeShiftCoefficient0.bitfields.RNR_SLOPE_0 = pData->rnr_slope[0];
        pRegCmd->rnrSlopeShiftCoefficient1.bitfields.RNR_SHIFT_1 = pData->rnr_shift[1];
        pRegCmd->rnrSlopeShiftCoefficient1.bitfields.RNR_SLOPE_1 = pData->rnr_slope[1];
        pRegCmd->rnrSlopeShiftCoefficient2.bitfields.RNR_SHIFT_2 = pData->rnr_shift[2];
        pRegCmd->rnrSlopeShiftCoefficient2.bitfields.RNR_SLOPE_2 = pData->rnr_slope[2];
        pRegCmd->rnrSlopeShiftCoefficient3.bitfields.RNR_SHIFT_3 = pData->rnr_shift[3];
        pRegCmd->rnrSlopeShiftCoefficient3.bitfields.RNR_SLOPE_3 = pData->rnr_slope[3];
        pRegCmd->rnrSlopeShiftCoefficient4.bitfields.RNR_SHIFT_4 = pData->rnr_shift[4];
        pRegCmd->rnrSlopeShiftCoefficient4.bitfields.RNR_SLOPE_4 = pData->rnr_slope[4];
        pRegCmd->rnrSlopeShiftCoefficient5.bitfields.RNR_SHIFT_5 = pData->rnr_shift[5];
        pRegCmd->rnrSlopeShiftCoefficient5.bitfields.RNR_SLOPE_5 = pData->rnr_slope[5];

        pRegCmd->rnrAnchorCoefficient0.bitfields.RNR_ANCHOR_0 = pData->rnr_anchor[0];
        pRegCmd->rnrAnchorCoefficient0.bitfields.RNR_BASE_0   = pData->rnr_base[0];
        pRegCmd->rnrAnchorCoefficient1.bitfields.RNR_ANCHOR_1 = pData->rnr_anchor[1];
        pRegCmd->rnrAnchorCoefficient1.bitfields.RNR_BASE_1   = pData->rnr_base[1];
        pRegCmd->rnrAnchorCoefficient2.bitfields.RNR_ANCHOR_2 = pData->rnr_anchor[2];
        pRegCmd->rnrAnchorCoefficient2.bitfields.RNR_BASE_2   = pData->rnr_base[2];
        pRegCmd->rnrAnchorCoefficient3.bitfields.RNR_ANCHOR_3 = pData->rnr_anchor[3];
        pRegCmd->rnrAnchorCoefficient3.bitfields.RNR_BASE_3   = pData->rnr_base[3];
        pRegCmd->rnrAnchorCoefficient4.bitfields.RNR_ANCHOR_4 = pData->rnr_anchor[4];
        pRegCmd->rnrAnchorCoefficient4.bitfields.RNR_BASE_4   = pData->rnr_base[4];
        pRegCmd->rnrAnchorCoefficient5.bitfields.RNR_ANCHOR_5 = pData->rnr_anchor[5];
        pRegCmd->rnrAnchorCoefficient5.bitfields.RNR_BASE_5   = pData->rnr_base[5];

        pRegCmd->radiusCoefficient0.bitfields.FACE_RADIUS_BOUNDARY = pData->face_radius_boundary[0];
        pRegCmd->radiusCoefficient0.bitfields.FACE_RADIUS_SHIFT    = pData->face_radius_shift[0];
        pRegCmd->radiusCoefficient0.bitfields.FACE_RADIUS_SLOPE    = pData->face_radius_slope[0];
        pRegCmd->radiusCoefficient0.bitfields.FACE_SLOPE_SHIFT     = pData->face_slope_shift[0];


        pRegCmd->radiusCoefficient1.bitfields.FACE_RADIUS_BOUNDARY = pData->face_radius_boundary[1];
        pRegCmd->radiusCoefficient1.bitfields.FACE_RADIUS_SHIFT    = pData->face_radius_shift[1];
        pRegCmd->radiusCoefficient1.bitfields.FACE_RADIUS_SLOPE    = pData->face_radius_slope[1];
        pRegCmd->radiusCoefficient1.bitfields.FACE_SLOPE_SHIFT     = pData->face_slope_shift[1];

        pRegCmd->radiusCoefficient2.bitfields.FACE_RADIUS_BOUNDARY = pData->face_radius_boundary[2];
        pRegCmd->radiusCoefficient2.bitfields.FACE_RADIUS_SHIFT    = pData->face_radius_shift[2];
        pRegCmd->radiusCoefficient2.bitfields.FACE_RADIUS_SLOPE    = pData->face_radius_slope[2];
        pRegCmd->radiusCoefficient2.bitfields.FACE_SLOPE_SHIFT     = pData->face_slope_shift[2];

        pRegCmd->radiusCoefficient3.bitfields.FACE_RADIUS_BOUNDARY = pData->face_radius_boundary[3];
        pRegCmd->radiusCoefficient3.bitfields.FACE_RADIUS_SHIFT    = pData->face_radius_shift[3];
        pRegCmd->radiusCoefficient3.bitfields.FACE_RADIUS_SLOPE    = pData->face_radius_slope[3];
        pRegCmd->radiusCoefficient3.bitfields.FACE_SLOPE_SHIFT     = pData->face_slope_shift[3];

        pRegCmd->radiusCoefficient4.bitfields.FACE_RADIUS_BOUNDARY = pData->face_radius_boundary[4];
        pRegCmd->radiusCoefficient4.bitfields.FACE_RADIUS_SHIFT    = pData->face_radius_shift[4];
        pRegCmd->radiusCoefficient4.bitfields.FACE_RADIUS_SLOPE    = pData->face_radius_slope[4];
        pRegCmd->radiusCoefficient4.bitfields.FACE_SLOPE_SHIFT     = pData->face_slope_shift[4];

        pHNRParameters->moduleCfg.BLEND_CNR_EN = pData->blend_cnr_en;
        pHNRParameters->moduleCfg.BLEND_ENABLE = pData->blend_enable;
        pHNRParameters->moduleCfg.BLEND_SNR_EN = pData->blend_snr_en;
        pHNRParameters->moduleCfg.CNR_EN       = pData->cnr_en;
        pHNRParameters->moduleCfg.FD_SNR_EN    = pData->fd_snr_en;
        pHNRParameters->moduleCfg.FNR_EN       = pData->fnr_en;
        pHNRParameters->moduleCfg.LNR_EN       = pData->lnr_en;
        pHNRParameters->moduleCfg.RNR_EN       = pData->rnr_en;
        pHNRParameters->moduleCfg.LPF3_EN      = pData->lpf3_en;
        pHNRParameters->moduleCfg.SNR_EN       = pData->snr_en;

        pRegCmd->nrGainCoefficient0.bitfields.FILTERING_NR_GAIN_ARR_0  = pData->filtering_nr_gain_arr[0];
        pRegCmd->nrGainCoefficient0.bitfields.FILTERING_NR_GAIN_ARR_1  = pData->filtering_nr_gain_arr[1];
        pRegCmd->nrGainCoefficient0.bitfields.FILTERING_NR_GAIN_ARR_2  = pData->filtering_nr_gain_arr[2];
        pRegCmd->nrGainCoefficient0.bitfields.FILTERING_NR_GAIN_ARR_3  = pData->filtering_nr_gain_arr[3];
        pRegCmd->nrGainCoefficient1.bitfields.FILTERING_NR_GAIN_ARR_4  = pData->filtering_nr_gain_arr[4];
        pRegCmd->nrGainCoefficient1.bitfields.FILTERING_NR_GAIN_ARR_5  = pData->filtering_nr_gain_arr[5];
        pRegCmd->nrGainCoefficient1.bitfields.FILTERING_NR_GAIN_ARR_6  = pData->filtering_nr_gain_arr[6];
        pRegCmd->nrGainCoefficient1.bitfields.FILTERING_NR_GAIN_ARR_7  = pData->filtering_nr_gain_arr[7];
        pRegCmd->nrGainCoefficient2.bitfields.FILTERING_NR_GAIN_ARR_8  = pData->filtering_nr_gain_arr[8];
        pRegCmd->nrGainCoefficient2.bitfields.FILTERING_NR_GAIN_ARR_9  = pData->filtering_nr_gain_arr[9];
        pRegCmd->nrGainCoefficient2.bitfields.FILTERING_NR_GAIN_ARR_10 = pData->filtering_nr_gain_arr[10];
        pRegCmd->nrGainCoefficient2.bitfields.FILTERING_NR_GAIN_ARR_11 = pData->filtering_nr_gain_arr[11];
        pRegCmd->nrGainCoefficient3.bitfields.FILTERING_NR_GAIN_ARR_12 = pData->filtering_nr_gain_arr[12];
        pRegCmd->nrGainCoefficient3.bitfields.FILTERING_NR_GAIN_ARR_13 = pData->filtering_nr_gain_arr[13];
        pRegCmd->nrGainCoefficient3.bitfields.FILTERING_NR_GAIN_ARR_14 = pData->filtering_nr_gain_arr[14];
        pRegCmd->nrGainCoefficient3.bitfields.FILTERING_NR_GAIN_ARR_15 = pData->filtering_nr_gain_arr[15];
        pRegCmd->nrGainCoefficient4.bitfields.FILTERING_NR_GAIN_ARR_16 = pData->filtering_nr_gain_arr[16];
        pRegCmd->nrGainCoefficient4.bitfields.FILTERING_NR_GAIN_ARR_17 = pData->filtering_nr_gain_arr[17];
        pRegCmd->nrGainCoefficient4.bitfields.FILTERING_NR_GAIN_ARR_18 = pData->filtering_nr_gain_arr[18];
        pRegCmd->nrGainCoefficient4.bitfields.FILTERING_NR_GAIN_ARR_19 = pData->filtering_nr_gain_arr[19];
        pRegCmd->nrGainCoefficient5.bitfields.FILTERING_NR_GAIN_ARR_20 = pData->filtering_nr_gain_arr[20];
        pRegCmd->nrGainCoefficient5.bitfields.FILTERING_NR_GAIN_ARR_21 = pData->filtering_nr_gain_arr[21];
        pRegCmd->nrGainCoefficient5.bitfields.FILTERING_NR_GAIN_ARR_22 = pData->filtering_nr_gain_arr[22];
        pRegCmd->nrGainCoefficient5.bitfields.FILTERING_NR_GAIN_ARR_23 = pData->filtering_nr_gain_arr[23];
        pRegCmd->nrGainCoefficient6.bitfields.FILTERING_NR_GAIN_ARR_24 = pData->filtering_nr_gain_arr[24];
        pRegCmd->nrGainCoefficient6.bitfields.FILTERING_NR_GAIN_ARR_25 = pData->filtering_nr_gain_arr[25];
        pRegCmd->nrGainCoefficient6.bitfields.FILTERING_NR_GAIN_ARR_26 = pData->filtering_nr_gain_arr[26];
        pRegCmd->nrGainCoefficient6.bitfields.FILTERING_NR_GAIN_ARR_27 = pData->filtering_nr_gain_arr[27];
        pRegCmd->nrGainCoefficient7.bitfields.FILTERING_NR_GAIN_ARR_28 = pData->filtering_nr_gain_arr[28];
        pRegCmd->nrGainCoefficient7.bitfields.FILTERING_NR_GAIN_ARR_29 = pData->filtering_nr_gain_arr[29];
        pRegCmd->nrGainCoefficient7.bitfields.FILTERING_NR_GAIN_ARR_30 = pData->filtering_nr_gain_arr[30];
        pRegCmd->nrGainCoefficient7.bitfields.FILTERING_NR_GAIN_ARR_31 = pData->filtering_nr_gain_arr[31];
        pRegCmd->nrGainCoefficient8.bitfields.FILTERING_NR_GAIN_ARR_32 = pData->filtering_nr_gain_arr[32];

        pRegCmd->cnrCFGCoefficient0.bitfields.CNR_LOW_THRD_U = pData->cnr_low_thrd_u;
        pRegCmd->cnrCFGCoefficient0.bitfields.CNR_LOW_THRD_V = pData->cnr_low_thrd_v;
        pRegCmd->cnrCFGCoefficient0.bitfields.CNR_THRD_GAP_U = pData->cnr_thrd_gap_u;
        pRegCmd->cnrCFGCoefficient0.bitfields.CNR_THRD_GAP_V = pData->cnr_thrd_gap_v;
        pRegCmd->cnrCFGCoefficient1.bitfields.CNR_ADJ_GAIN   = pData->cnr_adj_gain;
        pRegCmd->cnrCFGCoefficient1.bitfields.CNR_SCALE      = pData->cnr_scale;

        pRegCmd->cnrGaninCoeffincient0.bitfields.CNR_GAIN_ARR_0  = pData->cnr_gain_arr[0];
        pRegCmd->cnrGaninCoeffincient0.bitfields.CNR_GAIN_ARR_1  = pData->cnr_gain_arr[1];
        pRegCmd->cnrGaninCoeffincient0.bitfields.CNR_GAIN_ARR_2  = pData->cnr_gain_arr[2];
        pRegCmd->cnrGaninCoeffincient0.bitfields.CNR_GAIN_ARR_3  = pData->cnr_gain_arr[3];
        pRegCmd->cnrGaninCoeffincient0.bitfields.CNR_GAIN_ARR_4  = pData->cnr_gain_arr[4];
        pRegCmd->cnrGaninCoeffincient1.bitfields.CNR_GAIN_ARR_5  = pData->cnr_gain_arr[5];
        pRegCmd->cnrGaninCoeffincient1.bitfields.CNR_GAIN_ARR_6  = pData->cnr_gain_arr[6];
        pRegCmd->cnrGaninCoeffincient1.bitfields.CNR_GAIN_ARR_7  = pData->cnr_gain_arr[7];
        pRegCmd->cnrGaninCoeffincient1.bitfields.CNR_GAIN_ARR_8  = pData->cnr_gain_arr[8];
        pRegCmd->cnrGaninCoeffincient1.bitfields.CNR_GAIN_ARR_9  = pData->cnr_gain_arr[9];
        pRegCmd->cnrGaninCoeffincient2.bitfields.CNR_GAIN_ARR_10 = pData->cnr_gain_arr[10];
        pRegCmd->cnrGaninCoeffincient2.bitfields.CNR_GAIN_ARR_11 = pData->cnr_gain_arr[11];
        pRegCmd->cnrGaninCoeffincient2.bitfields.CNR_GAIN_ARR_12 = pData->cnr_gain_arr[12];
        pRegCmd->cnrGaninCoeffincient2.bitfields.CNR_GAIN_ARR_13 = pData->cnr_gain_arr[13];
        pRegCmd->cnrGaninCoeffincient2.bitfields.CNR_GAIN_ARR_14 = pData->cnr_gain_arr[14];
        pRegCmd->cnrGaninCoeffincient3.bitfields.CNR_GAIN_ARR_15 = pData->cnr_gain_arr[15];
        pRegCmd->cnrGaninCoeffincient3.bitfields.CNR_GAIN_ARR_16 = pData->cnr_gain_arr[16];
        pRegCmd->cnrGaninCoeffincient3.bitfields.CNR_GAIN_ARR_17 = pData->cnr_gain_arr[17];
        pRegCmd->cnrGaninCoeffincient3.bitfields.CNR_GAIN_ARR_18 = pData->cnr_gain_arr[18];
        pRegCmd->cnrGaninCoeffincient3.bitfields.CNR_GAIN_ARR_19 = pData->cnr_gain_arr[19];
        pRegCmd->cnrGaninCoeffincient4.bitfields.CNR_GAIN_ARR_20 = pData->cnr_gain_arr[20];
        pRegCmd->cnrGaninCoeffincient4.bitfields.CNR_GAIN_ARR_21 = pData->cnr_gain_arr[21];
        pRegCmd->cnrGaninCoeffincient4.bitfields.CNR_GAIN_ARR_22 = pData->cnr_gain_arr[22];
        pRegCmd->cnrGaninCoeffincient4.bitfields.CNR_GAIN_ARR_23 = pData->cnr_gain_arr[23];
        pRegCmd->cnrGaninCoeffincient4.bitfields.CNR_GAIN_ARR_24 = pData->cnr_gain_arr[24];

        pRegCmd->snrCoefficient0.bitfields.SNR_SKIN_HUE_MAX          = pData->snr_skin_hue_max;
        pRegCmd->snrCoefficient0.bitfields.SNR_SKIN_HUE_MIN          = pData->snr_skin_hue_min;
        pRegCmd->snrCoefficient0.bitfields.SNR_SKIN_SMOOTHING_STR    = pData->snr_skin_smoothing_str;
        pRegCmd->snrCoefficient0.bitfields.SNR_SKIN_Y_MIN            = pData->snr_skin_y_min;

        pRegCmd->snrCoefficient1.bitfields.SNR_SKIN_Y_MAX            = pData->snr_skin_y_max;
        pRegCmd->snrCoefficient1.bitfields.SNR_BOUNDARY_PROBABILITY  = pData->snr_boundary_probability;
        pRegCmd->snrCoefficient1.bitfields.SNR_QSTEP_NONSKIN         = pData->snr_qstep_nonskin;
        pRegCmd->snrCoefficient1.bitfields.SNR_QSTEP_SKIN            = pData->snr_qstep_skin;
        pRegCmd->snrCoefficient2.bitfields.SNR_SAT_MAX_SLOPE         = pData->snr_sat_max_slope;
        pRegCmd->snrCoefficient2.bitfields.SNR_SAT_MIN_SLOPE         = pData->snr_sat_min_slope;
        pRegCmd->snrCoefficient2.bitfields.SNR_SKIN_YMAX_SAT_MAX     = pData->snr_skin_ymax_sat_max;
        pRegCmd->snrCoefficient2.bitfields.SNR_SKIN_YMAX_SAT_MIN     = pData->snr_skin_ymax_sat_min;

        pRegCmd->faceConfigCoefficient.bitfields.FACE_NUM               = pData->face_num;
        pRegCmd->faceOffsetCoefficient.bitfields.FACE_HORIZONTAL_OFFSET = pData->face_horizontal_offset;
        pRegCmd->faceOffsetCoefficient.bitfields.FACE_VERTICAL_OFFSET   = pData->face_vertical_offset;

        pHNRParameters->snrSkinSmoothingStr = static_cast<INT32>(pData->snr_skin_smoothing_str);

        for (i = 0; i < HNR_V10_LNR_ARR_NUM; i++)
        {
            pOutput->pLNRDMIBuffer[i] = static_cast<UINT32>(pData->lnr_gain_arr[pData->lut_bank_sel][i]) & Utils::AllOnes32(12);
        }

        for (i = 0; i < HNR_V10_FNR_ARR_NUM; i++)
        {
            pOutput->pFNRAndClampDMIBuffer[i] = pData->merged_fnr_gain_arr_gain_clamp_arr[pData->lut_bank_sel][i] &
                                                Utils::AllOnes32(28);
            pOutput->pFNRAcDMIBuffer[i]       = static_cast<UINT32>(pData->fnr_ac_th_arr[pData->lut_bank_sel][i]) &
                                                Utils::AllOnes32(12);
        }

        for (i = 0; i < HNR_V10_SNR_ARR_NUM; i++)
        {
            pOutput->pSNRDMIBuffer[i] = static_cast<UINT32>(pData->snr_gain_arr[pData->lut_bank_sel][i]) &
                                        Utils::AllOnes32(10);
        }

        for (i = 0; i < HNR_V10_BLEND_LNR_ARR_NUM; i++)
        {
            pOutput->pBlendLNRDMIBuffer[i] = static_cast<UINT32>(pData->blend_lnr_gain_arr[pData->lut_bank_sel][i]) &
                                             Utils::AllOnes32(16);
        }

        for (i = 0; i < HNR_V10_BLEND_SNR_ARR_NUM; i++)
        {
            pOutput->pBlendSNRDMIBuffer[i] = static_cast<UINT32>(pData->blend_snr_gain_arr[pData->lut_bank_sel][i]) &
                                             Utils::AllOnes32(10);
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid pointer: pData: %p, pOutput: %p", pData, pOutput);

        if (NULL != pOutput)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid pointer: pOutput->pRegCmd: %p, pOutput->pHNRParameters: %p",
                           pOutput->pRegCmd, pOutput->pHNRParameters);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::BPSHNR10CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::BPSHNR10CalculateSetting(
    const HNR10InputData* pInput,
    VOID*                 pOEMIQData,
    HNR10OutputData*      pOutput)
{
    CamxResult                                            result             = CamxResultSuccess;
    BOOL                                                  commonIQResult     = FALSE;
    hnr_1_0_0::hnr10_rgn_dataType*                        pInterpolationData = NULL;
    hnr_1_0_0::chromatix_hnr10_reserveType*               pReserveType       = NULL;
    hnr_1_0_0::chromatix_hnr10Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<hnr_1_0_0::hnr10_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult     = IQInterface::s_interpolationTable.HNR10Interpolation(pInput, pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_hnr10_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<hnr_1_0_0::hnr10_rgn_dataType*>(&(pOEMInput->HNRSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveType   = reinterpret_cast<hnr_1_0_0::chromatix_hnr10_reserveType*>(
                                     &(pOEMInput->HNRReserveType));
                pModuleEnable  = reinterpret_cast<hnr_1_0_0::chromatix_hnr10Type::enable_sectionStruct*>(
                                     &(pOEMInput->HNREnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            HNR10UnpackedField unpackedRegData;
            Utils::Memset(&unpackedRegData, 0, sizeof(unpackedRegData));

            commonIQResult =
                IQInterface::s_interpolationTable.HNR10CalculateHWSetting(pInput,
                                                                          pInterpolationData,
                                                                          pReserveType,
                                                                          pModuleEnable,
                                                                          static_cast<VOID*>(&unpackedRegData));

            if (TRUE == commonIQResult)
            {
                BPSSetupHNR10Register(&unpackedRegData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("HNR10 Calculate HW setting Failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("HNR10 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::BPSGIC30CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::BPSGIC30CalculateSetting(
    const GIC30InputData*  pInput,
    VOID*                  pOEMIQData,
    GIC30OutputData*       pOutput)
{
    CamxResult                                            result             = CamxResultSuccess;
    BOOL                                                  commonIQResult     = FALSE;
    gic_3_0_0::gic30_rgn_dataType*                        pInterpolationData = NULL;
    gic_3_0_0::chromatix_gic30_reserveType*               pReserveType       = NULL;
    gic_3_0_0::chromatix_gic30Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<gic_3_0_0::gic30_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult     = IQInterface::s_interpolationTable.BPSGIC30Interpolation(pInput, pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_gic30_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<gic_3_0_0::gic30_rgn_dataType*>(&(pOEMInput->GICSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveType   = reinterpret_cast<gic_3_0_0::chromatix_gic30_reserveType*>(
                                     &(pOEMInput->GICReserveType));
                pModuleEnable  = reinterpret_cast<gic_3_0_0::chromatix_gic30Type::enable_sectionStruct*>(
                                     &(pOEMInput->GICEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            GIC30UnpackedField unpackedRegisterData;

            commonIQResult =
                IQInterface::s_interpolationTable.BPSGIC30CalculateHWSetting(pInput,
                                                                             pInterpolationData,
                                                                             pReserveType,
                                                                             pModuleEnable,
                                                                             static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                BPSSetupGIC30Register(&unpackedRegisterData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("GIC30 calculate HW setting failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("GIC30 intepolation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("GIC Input data is pInput %p pOutput  %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFEHDR20CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IFEHDR20CalculateSetting(
    const HDR20InputData*  pInput,
    VOID*                  pOEMIQData,
    HDR20OutputData*       pOutput)
{
    CamxResult                     result                = CamxResultSuccess;
    BOOL                           commonIQResult        = FALSE;
    hdr_2_0_0::hdr20_rgn_dataType* pInterpolationData    = NULL;
    OEMIFEIQSetting*               pOEMInput             = NULL;
    hdr_2_0_0::chromatix_hdr20_reserveType* pReserveType = NULL;
    // hdr_2_0_0::hdr20_rgn_dataType  interpolationData;
    HDR20UnpackedField             unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<hdr_2_0_0::hdr20_rgn_dataType*>(pInput->pInterpolationData);
            // Call the Interpolation Calculation
            commonIQResult     = IQInterface::s_interpolationTable.IFEHDR20Interpolation(pInput, pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_hdr20_reserve);
        }
        else
        {
            pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);
            // Use OEM defined interpolation data
            pInterpolationData =
                reinterpret_cast<hdr_2_0_0::hdr20_rgn_dataType*>(&(pOEMInput->HDRData.HDR20.HDRSetting));
            pReserveType       =
                reinterpret_cast<hdr_2_0_0::chromatix_hdr20_reserveType*>(&(pOEMInput->HDRData.HDR20.HDRReserveType));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            commonIQResult = IQInterface::s_interpolationTable.IFEHDR20CalculateHWSetting(
                pInput, pInterpolationData, pReserveType, static_cast<VOID*>(&unpackedRegData));

            if (TRUE == commonIQResult)
            {
                pOutput->hdr20State.hdr_zrec_first_rb_exp = unpackedRegData.hdr_zrec_first_rb_exp;
                IFESetupHDR20Reg(&unpackedRegData, pOutput->pRegCmd);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("HDR20 calculate hardware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("HDR20 intepolation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput  %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::HDR23CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::HDR23CalculateSetting(
    const HDR23InputData*  pInput,
    VOID*                  pOEMIQData,
    HDR23OutputData*       pOutput)
{
    CamxResult                     result                = CamxResultSuccess;
    BOOL                           commonIQResult        = FALSE;
    hdr_2_3_0::hdr23_rgn_dataType* pInterpolationData    = NULL;
    OEMIFEIQSetting*               pOEMInput             = NULL;
    hdr_2_3_0::chromatix_hdr23_reserveType* pReserveType = NULL;
    // hdr_2_3_0::hdr30_rgn_dataType  interpolationData;
    HDR23UnpackedField             unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<hdr_2_3_0::hdr23_rgn_dataType*>(pInput->pInterpolationData);
            // Call the Interpolation Calculation
            commonIQResult     = IQInterface::s_interpolationTable.IFEHDR23Interpolation(pInput, pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_hdr23_reserve);
        }
        else
        {
            pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);
            // Use OEM defined interpolation data
            pInterpolationData =
                reinterpret_cast<hdr_2_3_0::hdr23_rgn_dataType*>(&(pOEMInput->HDRData.HDR23.HDRSetting));
            pReserveType =
                reinterpret_cast<hdr_2_3_0::chromatix_hdr23_reserveType*>(&(pOEMInput->HDRData.HDR23.HDRReserveType));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            commonIQResult = IQInterface::s_interpolationTable.IFEHDR23CalculateHWSetting(
                pInput, pInterpolationData, pReserveType, static_cast<VOID*>(&unpackedRegData));

            if (TRUE == commonIQResult)
            {
                pOutput->hdr23State.hdr_zrec_first_rb_exp = unpackedRegData.hdr_zrec_first_rb_exp;
                IFESetupHDR23Reg(&unpackedRegData, pOutput->pRegCmd);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("HDR23 calculate hardware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("HDR23 intepolation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput  %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::Pedestal13CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::Pedestal13CalculateSetting(
    const Pedestal13InputData* pInput,
    VOID*                      pOEMIQData,
    Pedestal13OutputData*      pOutput)
{
    CamxResult                               result                               = CamxResultSuccess;
    BOOL                                     commonIQResult                       = FALSE;
    pedestal_1_3_0::pedestal13_rgn_dataType* pInterpolationData                   = NULL;
    pedestal_1_3_0::chromatix_pedestal13Type::enable_sectionStruct* pModuleEnable = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<pedestal_1_3_0::pedestal13_rgn_dataType*>(pInput->pInterpolationData);;
            commonIQResult     = IQInterface::s_interpolationTable.pedestal13Interpolation(pInput, pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            if (PipelineType::IFE == pOutput->type)
            {
                OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);
                // Use OEM defined interpolation data
                pInterpolationData         = reinterpret_cast<pedestal_1_3_0::pedestal13_rgn_dataType*>(
                                                &(pOEMInput->PedestalSetting));
                pModuleEnable              = reinterpret_cast<pedestal_1_3_0::chromatix_pedestal13Type::enable_sectionStruct*>(
                                                &(pOEMInput->PedestalEnableSection));
            }
            else if (PipelineType::BPS == pOutput->type)
            {
                OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);
                // Use OEM defined interpolation data
                pInterpolationData         = reinterpret_cast<pedestal_1_3_0::pedestal13_rgn_dataType*>(
                                                &(pOEMInput->PedestalSetting));
                pModuleEnable              = reinterpret_cast<pedestal_1_3_0::chromatix_pedestal13Type::enable_sectionStruct*>(
                                                &(pOEMInput->PedestalEnableSection));
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for Demux13", pOutput->type);
            }

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            Pedestal13UnpackedField unpackedRegisterData;

            commonIQResult =
                IQInterface::s_interpolationTable.pedestal13CalculateHWSetting(pInput,
                                                                               pInterpolationData,
                                                                               pModuleEnable,
                                                                               static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                if (PipelineType::IFE == pOutput->type)
                {
                    pOutput->pedState.bwidth_l         = static_cast<uint16_t>(unpackedRegisterData.bWidthL);
                    pOutput->pedState.bx_d1_l          = static_cast<uint16_t>(unpackedRegisterData.bxD1L);
                    pOutput->pedState.bx_start_l       = static_cast<uint16_t>(unpackedRegisterData.bxStartL);
                    pOutput->pedState.lx_start_l       = static_cast<uint16_t>(unpackedRegisterData.lxStartL);
                    pOutput->pedState.meshGridBwidth_l = static_cast<uint16_t>(unpackedRegisterData.meshGridbWidthL);

                    IFESetupPedestal13Register(&unpackedRegisterData, pOutput);
                }
                else if (PipelineType::BPS == pOutput->type)
                {
                    BPSSetupPedestal13Register(&unpackedRegisterData, pOutput);
                }
                else
                {
                    result = CamxResultEInvalidArg;
                    CAMX_ASSERT_ALWAYS_MESSAGE("invalid Pipeline type.  Type is %d ", pOutput->type);
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("Pedestal13 intepolation failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Pedestal13 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput  %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFESetupWB12Register
///
/// @brief  Setup WB12 Register based on common library output
///
/// @param  pData      Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the WB12 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IFESetupWB12Register(
    WB12UnpackedField*    pData,
    IFEWB12RegCmd*        pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->WBLeftConfig0.bitfields.G_GAIN          = pData->gainChannel0Left;
        pRegCmd->WBLeftConfig0.bitfields.B_GAIN          = pData->gainChannel1Left;
        pRegCmd->WBLeftConfig1.bitfields.R_GAIN          = pData->gainChannel2Left;

        pRegCmd->WBLeftOffsetConfig0.bitfields.G_OFFSET  = pData->offsetChannel0Left;
        pRegCmd->WBLeftOffsetConfig0.bitfields.B_OFFSET  = pData->offsetChannel1Left;
        pRegCmd->WBLeftOffsetConfig1.bitfields.R_OFFSET  = pData->offsetChannel2Left;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("data is NULL %p %p", pData, pRegCmd);
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IFEWB12CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IFEWB12CalculateSetting(
    const WB12InputData*   pInput,
    WB12OutputData*        pOutput)
{
    BOOL               commonIQResult = FALSE;
    CamxResult         result         = CamxResultSuccess;
    WB12UnpackedField  unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        commonIQResult = IQInterface::s_interpolationTable.WB12CalculateHWSetting(pInput, static_cast<VOID*>(&unpackedRegData));
        if (TRUE == commonIQResult)
        {
            IFESetupWB12Register(&unpackedRegData, pOutput->pRegCmd);
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("WB calculate HW setting failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL %p %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupWB13Register
///
/// @brief  Setup WB13 Register based on common library output
///
/// @param  pData      Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the WB13 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupWB13Register(
    WB13UnpackedField*    pData,
    BPSWB13RegCmd*        pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd))
    {
        pRegCmd->whiteBalanceConfig0.bitfields.G_GAIN  = pData->gainChannel0Left;
        pRegCmd->whiteBalanceConfig0.bitfields.B_GAIN  = pData->gainChannel1Left;
        pRegCmd->whiteBalanceConfig1.bitfields.R_GAIN  = pData->gainChannel2Left;
        pRegCmd->whiteBalaneOffset0.bitfields.G_OFFSET = pData->offsetChannel0Left;
        pRegCmd->whiteBalaneOffset0.bitfields.B_OFFSET = pData->offsetChannel1Left;
        pRegCmd->whiteBalaneOffset1.bitfields.R_OFFSET = pData->offsetChannel2Left;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("data is NULL %p %p", pData, pRegCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::BPSWB13CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::BPSWB13CalculateSetting(
    const WB13InputData*  pInput,
    VOID*                 pOEMIQData,
    WB13OutputData*       pOutput)
{
    BOOL       commonIQResult = FALSE;
    CamxResult result         = CamxResultSuccess;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        WB13UnpackedField                unpackedRegData;
        globalelements::enable_flag_type moduleEnable = TRUE;

        if (NULL != pOEMIQData)
        {
            OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);
            moduleEnable               = pOEMInput->WBEnable;
        }

        commonIQResult = IQInterface::s_interpolationTable.WB13CalculateHWSetting(pInput,
                                                                                  moduleEnable,
                                                                                  static_cast<VOID*>(&unpackedRegData));
        if (TRUE == commonIQResult)
        {
            BPSSetupWB13Register(&unpackedRegData, pOutput->pRegCmd);
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("WB calculate HW setting failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL %p %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupHDRRegister
///
/// @brief  Setup BPSHDR22 Register based on common library output
///
/// @param  pRegVal    Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the BPSHDR Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupHDRRegister(
    HDR22UnpackedField* pData,
    HDR22OutputData*    pOutput)
{
    if ((NULL != pData) && (NULL != pOutput))
    {
        BPSHDRRegCmd* pRegCmd = pOutput->BPS.pRegCmd;
        pRegCmd->MACConfig.config0.bitfields.EXP_RATIO               = pData->hdr_exp_ratio;
        pRegCmd->MACConfig.config0.bitfields.EXP_RATIO_RECIP         = pData->hdr_exp_ratio_recip;
        pRegCmd->MACConfig.config1.bitfields.BLK_IN                  = pData->hdr_black_in;
        pRegCmd->MACConfig.config1.bitfields.GB_WB_GAIN_RATIO        = pData->hdr_gb_wb_gain_ratio;
        pRegCmd->MACConfig.config2.bitfields.BLK_OUT                 = pData->hdr_black_out;
        pRegCmd->MACConfig.config2.bitfields.GR_WB_GAIN_RATIO        = pData->hdr_gr_wb_gain_ratio;
        pRegCmd->MACConfig.config3.bitfields.MAC_MOTION_0_TH1        = pData->hdr_mac_motion0_th1;
        pRegCmd->MACConfig.config3.bitfields.MAC_MOTION_0_TH2        = pData->hdr_mac_motion0_th2;
        pRegCmd->MACConfig.config3.bitfields.MAC_SQRT_ANALOG_GAIN    = pData->hdr_mac_sqrt_analog_gain;
        pRegCmd->MACConfig.config4.bitfields.MAC_DILATION            = pData->hdr_mac_dilation;
        pRegCmd->MACConfig.config4.bitfields.MAC_LOW_LIGHT_DTH_LOG2  = pData->hdr_mac_lowlight_dth_log2;
        pRegCmd->MACConfig.config4.bitfields.MAC_LOW_LIGHT_STRENGTH  = pData->hdr_mac_lowlight_strength;
        pRegCmd->MACConfig.config4.bitfields.MAC_MOTION_0_DT0        = pData->hdr_mac_motion0_dt0;
        pRegCmd->MACConfig.config4.bitfields.MAC_MOTION_STRENGTH     = pData->hdr_mac_motion_strength;
        pRegCmd->MACConfig.config5.bitfields.MAC_HIGH_LIGHT_TH1      = pData->hdr_mac_hilight_th1;
        pRegCmd->MACConfig.config5.bitfields.MAC_LOW_LIGHT_TH1       = pData->hdr_mac_lowlight_th1;
        pRegCmd->MACConfig.config6.bitfields.MAC_HIGH_LIGHT_DTH_LOG2 = pData->hdr_mac_hilight_dth_log2;
        pRegCmd->MACConfig.config6.bitfields.MAC_LINEAR_MODE         = pData->hdr_mac_linear_mode;
        pRegCmd->MACConfig.config6.bitfields.MAC_SMOOTH_DTH_LOG2     = pData->hdr_mac_smooth_dth_log2;
        pRegCmd->MACConfig.config6.bitfields.MAC_SMOOTH_ENABLE       = pData->hdr_mac_smooth_filter_en;
        pRegCmd->MACConfig.config6.bitfields.MAC_SMOOTH_TAP0         = pData->hdr_mac_smooth_tap0;
        pRegCmd->MACConfig.config6.bitfields.MAC_SMOOTH_TH1          = pData->hdr_mac_smooth_th1;
        pRegCmd->MACConfig.config6.bitfields.MSB_ALIGNED             = pData->hdr_MSB_align;

        pRegCmd->reconstructConfig.config0.bitfields.EXP_RATIO                       = pData->hdr_exp_ratio;
        pRegCmd->reconstructConfig.config0.bitfields.RECON_LINEAR_MODE               = pData->hdr_rec_linear_mode;
        pRegCmd->reconstructConfig.config0.bitfields.ZREC_ENABLE                     = pData->hdr_zrec_en;
        pRegCmd->reconstructConfig.config1.bitfields.RG_WB_GAIN_RATIO                = pData->hdr_rg_wb_gain_ratio;
        pRegCmd->reconstructConfig.config2.bitfields.BG_WB_GAIN_RATIO                = pData->hdr_bg_wb_gain_ratio;
        pRegCmd->reconstructConfig.config3.bitfields.BLK_IN                          = pData->hdr_black_in;
        pRegCmd->reconstructConfig.interlacedConfig0.bitfields.RECON_H_EDGE_DTH_LOG2 = pData->hdr_rec_h_edge_dth_log2;
        pRegCmd->reconstructConfig.interlacedConfig0.bitfields.RECON_H_EDGE_TH1      = pData->hdr_rec_h_edge_th1;
        pRegCmd->reconstructConfig.interlacedConfig0.bitfields.RECON_MOTION_DTH_LOG2 = pData->hdr_rec_motion_dth_log2;
        pRegCmd->reconstructConfig.interlacedConfig0.bitfields.RECON_MOTION_TH1      = pData->hdr_rec_motion_th1;
        pRegCmd->reconstructConfig.interlacedConfig1.bitfields.RECON_DARK_DTH_LOG2   = pData->hdr_rec_dark_dth_log2;
        pRegCmd->reconstructConfig.interlacedConfig1.bitfields.RECON_DARK_TH1        = pData->hdr_rec_dark_th1;
        pRegCmd->reconstructConfig.interlacedConfig1.bitfields.RECON_EDGE_LPF_TAP0   = pData->hdr_rec_edge_lpf_tap0;
        pRegCmd->reconstructConfig.interlacedConfig1.bitfields.RECON_FIRST_FIELD     = pData->hdr_first_field;
        pRegCmd->reconstructConfig.interlacedConfig1.bitfields.RECON_FLAT_REGION_TH  = pData->hdr_rec_flat_region_th;
        pRegCmd->reconstructConfig.interlacedConfig2.bitfields.RECON_MIN_FACTOR      = pData->hdr_rec_min_factor;
        pRegCmd->reconstructConfig.zigzagConfig0.bitfields.ZREC_FIRST_RB_EXP         = pData->hdr_zrec_first_rb_exp;
        pRegCmd->reconstructConfig.zigzagConfig0.bitfields.ZREC_PATTERN              = pData->hdr_zrec_pattern;
        pRegCmd->reconstructConfig.zigzagConfig0.bitfields.ZREC_PREFILT_TAP0         = pData->hdr_zrec_prefilt_tap0;
        pRegCmd->reconstructConfig.zigzagConfig1.bitfields.ZREC_G_DTH_LOG2           = pData->hdr_zrec_g_grad_dth_log2;
        pRegCmd->reconstructConfig.zigzagConfig1.bitfields.ZREC_G_GRAD_TH1           = pData->hdr_zrec_g_grad_th1;
        pRegCmd->reconstructConfig.zigzagConfig1.bitfields.ZREC_RB_DTH_LOG2          = pData->hdr_zrec_rb_grad_dth_log2;
        pRegCmd->reconstructConfig.zigzagConfig1.bitfields.ZREC_RB_GRAD_TH1          = pData->hdr_zrec_rb_grad_th1;

        pOutput->BPS.pHdrMacParameters->dilation            = pData->hdr_mac_dilation;
        pOutput->BPS.pHdrMacParameters->linearMode          = pData->hdr_mac_linear_mode;
        pOutput->BPS.pHdrMacParameters->smoothFilterEnable  = pData->hdr_mac_smooth_filter_en;
        pOutput->BPS.pHdrMacParameters->moduleCfg.EN        = pData->hdr_mac_en;
        pOutput->BPS.pHdrReconParameters->hdrExpRatio       = pData->hdr_exp_ratio;
        pOutput->BPS.pHdrReconParameters->hdrMode           = 1;
        pOutput->BPS.pHdrReconParameters->linearMode        = pData->hdr_rec_linear_mode;
        pOutput->BPS.pHdrReconParameters->moduleCfg.EN      = pData->hdr_recon_en;
        pOutput->BPS.pHdrReconParameters->zzHdrPattern      = pData->hdr_zrec_pattern;
        pOutput->BPS.pHdrReconParameters->zzHdrPrefilterTap = pData->hdr_zrec_prefilt_tap0;
        pOutput->BPS.pHdrReconParameters->zzHdrShift        = pData->hdr_zrec_first_rb_exp;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("%s: Input data is NULL ", __FUNCTION__);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::HDR22CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::HDR22CalculateSetting(
    const HDR22InputData* pInput,
    VOID*                 pOEMIQData,
    HDR22OutputData*      pOutput)
{
    CamxResult                                            result             = CamxResultSuccess;
    BOOL                                                  commonIQResult     = FALSE;
    hdr_2_2_0::hdr22_rgn_dataType*                        pInterpolationData = NULL;
    hdr_2_2_0::chromatix_hdr22_reserveType*               pReserveType       = NULL;
    hdr_2_2_0::chromatix_hdr22Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<hdr_2_2_0::hdr22_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult     = IQInterface::s_interpolationTable.HDR22Interpolation(pInput, pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_hdr22_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);
        }
        else
        {
            if (PipelineType::IFE == pOutput->type)
            {
                OEMIFEIQSetting* pOEMInput = static_cast<OEMIFEIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pInterpolationData = reinterpret_cast<hdr_2_2_0::hdr22_rgn_dataType*>(&(pOEMInput->HDRData.HDR22.HDRSetting));

                if (NULL != pInterpolationData)
                {
                    commonIQResult = TRUE;
                    pReserveType   = reinterpret_cast<hdr_2_2_0::chromatix_hdr22_reserveType*>(
                                        &(pOEMInput->HDRData.HDR22.HDRReserveType));
                    pModuleEnable  = reinterpret_cast<hdr_2_2_0::chromatix_hdr22Type::enable_sectionStruct*>(
                                        &(pOEMInput->HDRData.HDR22.HDREnableSection));
                }
                else
                {
                    result = CamxResultEFailed;
                    CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
                }
            }
            else if (PipelineType::BPS == pOutput->type)
            {
                OEMBPSIQSetting* pOEMInput = static_cast<OEMBPSIQSetting*>(pOEMIQData);

                // Use OEM defined interpolation data
                pInterpolationData = reinterpret_cast<hdr_2_2_0::hdr22_rgn_dataType*>(&(pOEMInput->HDRSetting));

                if (NULL != pInterpolationData)
                {
                    commonIQResult = TRUE;
                    pReserveType   = reinterpret_cast<hdr_2_2_0::chromatix_hdr22_reserveType*>(
                                        &(pOEMInput->HDRReserveType));
                    pModuleEnable  = reinterpret_cast<hdr_2_2_0::chromatix_hdr22Type::enable_sectionStruct*>(
                                        &(pOEMInput->HDREnableSection));
                }
                else
                {
                    result = CamxResultEFailed;
                    CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
                }
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Pipeline Type %d not supported for HDR22", pOutput->type);
            }
        }

        if (TRUE == commonIQResult)
        {
            HDR22UnpackedField  unpackedRegisterData;

            commonIQResult =
                IQInterface::s_interpolationTable.HDR22CalculateHWSetting(pInput,
                                                                          pInterpolationData,
                                                                          pReserveType,
                                                                          pModuleEnable,
                                                                          static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                if (PipelineType::IFE == pOutput->type)
                {
                    IFESetupHDR22Reg(&unpackedRegisterData, pOutput);
                }
                else if (PipelineType::BPS == pOutput->type)
                {
                    BPSSetupHDRRegister(&unpackedRegisterData, pOutput);
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("BPSHDR22 calculate HW settings failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("BPSHDR22 intepolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SetupSCE11Register
///
/// @brief  Setup SCE11 Register based on common library output
///
/// @param  pData   Pointer to the Common Library Calculation Result
/// @param  pCmd    Pointer to the SCE11 Register and DMI Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID SetupSCE11Reg(
    SCE11UnpackedField*   pDataSCE,
    SCE11OutputData*      pCmd)
{
    if ((NULL != pDataSCE) && (NULL != pCmd) && (NULL != pCmd->pRegCmd))
    {
        pCmd->pRegCmd->vertex1.cfg0.bitfields.CB_COORD_1 = pDataSCE->t0_Cb1;
        pCmd->pRegCmd->vertex1.cfg0.bitfields.CR_COORD_1 = pDataSCE->t0_Cr1;
        pCmd->pRegCmd->vertex1.cfg1.bitfields.CB_COORD_1 = pDataSCE->t1_Cb1;
        pCmd->pRegCmd->vertex1.cfg1.bitfields.CR_COORD_1 = pDataSCE->t1_Cr1;
        pCmd->pRegCmd->vertex1.cfg2.bitfields.CB_COORD_1 = pDataSCE->t2_Cb1;
        pCmd->pRegCmd->vertex1.cfg2.bitfields.CR_COORD_1 = pDataSCE->t2_Cr1;
        pCmd->pRegCmd->vertex1.cfg3.bitfields.CB_COORD_1 = pDataSCE->t3_Cb1;
        pCmd->pRegCmd->vertex1.cfg3.bitfields.CR_COORD_1 = pDataSCE->t3_Cr1;
        pCmd->pRegCmd->vertex1.cfg4.bitfields.CB_COORD_1 = pDataSCE->t4_Cb1;
        pCmd->pRegCmd->vertex1.cfg4.bitfields.CR_COORD_1 = pDataSCE->t4_Cr1;

        pCmd->pRegCmd->vertex2.cfg0.bitfields.CB_COORD_2 = pDataSCE->t0_Cb2;
        pCmd->pRegCmd->vertex2.cfg0.bitfields.CR_COORD_2 = pDataSCE->t0_Cr2;
        pCmd->pRegCmd->vertex2.cfg1.bitfields.CB_COORD_2 = pDataSCE->t1_Cb2;
        pCmd->pRegCmd->vertex2.cfg1.bitfields.CR_COORD_2 = pDataSCE->t1_Cr2;
        pCmd->pRegCmd->vertex2.cfg2.bitfields.CB_COORD_2 = pDataSCE->t2_Cb2;
        pCmd->pRegCmd->vertex2.cfg2.bitfields.CR_COORD_2 = pDataSCE->t2_Cr2;
        pCmd->pRegCmd->vertex2.cfg3.bitfields.CB_COORD_2 = pDataSCE->t3_Cb2;
        pCmd->pRegCmd->vertex2.cfg3.bitfields.CR_COORD_2 = pDataSCE->t3_Cr2;
        pCmd->pRegCmd->vertex2.cfg4.bitfields.CB_COORD_2 = pDataSCE->t4_Cb2;
        pCmd->pRegCmd->vertex2.cfg4.bitfields.CR_COORD_2 = pDataSCE->t4_Cr2;

        pCmd->pRegCmd->vertex3.cfg0.bitfields.CB_COORD_3 = pDataSCE->t0_Cb3;
        pCmd->pRegCmd->vertex3.cfg0.bitfields.CR_COORD_3 = pDataSCE->t0_Cr3;
        pCmd->pRegCmd->vertex3.cfg1.bitfields.CB_COORD_3 = pDataSCE->t1_Cb3;
        pCmd->pRegCmd->vertex3.cfg1.bitfields.CR_COORD_3 = pDataSCE->t1_Cr3;
        pCmd->pRegCmd->vertex3.cfg2.bitfields.CB_COORD_3 = pDataSCE->t2_Cb3;
        pCmd->pRegCmd->vertex3.cfg2.bitfields.CR_COORD_3 = pDataSCE->t2_Cr3;
        pCmd->pRegCmd->vertex3.cfg3.bitfields.CB_COORD_3 = pDataSCE->t3_Cb3;
        pCmd->pRegCmd->vertex3.cfg3.bitfields.CR_COORD_3 = pDataSCE->t3_Cr3;
        pCmd->pRegCmd->vertex3.cfg4.bitfields.CB_COORD_3 = pDataSCE->t4_Cb3;
        pCmd->pRegCmd->vertex3.cfg4.bitfields.CR_COORD_3 = pDataSCE->t4_Cr3;

        pCmd->pRegCmd->crCoefficient.cfg0.bitfields.COEFF_A = pDataSCE->t0_A;
        pCmd->pRegCmd->crCoefficient.cfg0.bitfields.COEFF_B = pDataSCE->t0_B;
        pCmd->pRegCmd->crCoefficient.cfg1.bitfields.COEFF_A = pDataSCE->t1_A;
        pCmd->pRegCmd->crCoefficient.cfg1.bitfields.COEFF_B = pDataSCE->t1_B;
        pCmd->pRegCmd->crCoefficient.cfg2.bitfields.COEFF_A = pDataSCE->t2_A;
        pCmd->pRegCmd->crCoefficient.cfg2.bitfields.COEFF_B = pDataSCE->t2_B;
        pCmd->pRegCmd->crCoefficient.cfg3.bitfields.COEFF_A = pDataSCE->t3_A;
        pCmd->pRegCmd->crCoefficient.cfg3.bitfields.COEFF_B = pDataSCE->t3_B;
        pCmd->pRegCmd->crCoefficient.cfg4.bitfields.COEFF_A = pDataSCE->t4_A;
        pCmd->pRegCmd->crCoefficient.cfg4.bitfields.COEFF_B = pDataSCE->t4_B;
        pCmd->pRegCmd->crCoefficient.cfg5.bitfields.COEFF_A = pDataSCE->t5_A;
        pCmd->pRegCmd->crCoefficient.cfg5.bitfields.COEFF_B = pDataSCE->t5_B;

        pCmd->pRegCmd->cbCoefficient.cfg0.bitfields.COEFF_D = pDataSCE->t0_D;
        pCmd->pRegCmd->cbCoefficient.cfg0.bitfields.COEFF_E = pDataSCE->t0_E;
        pCmd->pRegCmd->cbCoefficient.cfg1.bitfields.COEFF_D = pDataSCE->t1_D;
        pCmd->pRegCmd->cbCoefficient.cfg1.bitfields.COEFF_E = pDataSCE->t1_E;
        pCmd->pRegCmd->cbCoefficient.cfg2.bitfields.COEFF_D = pDataSCE->t2_D;
        pCmd->pRegCmd->cbCoefficient.cfg2.bitfields.COEFF_E = pDataSCE->t2_E;
        pCmd->pRegCmd->cbCoefficient.cfg3.bitfields.COEFF_D = pDataSCE->t3_D;
        pCmd->pRegCmd->cbCoefficient.cfg3.bitfields.COEFF_E = pDataSCE->t3_E;
        pCmd->pRegCmd->cbCoefficient.cfg4.bitfields.COEFF_D = pDataSCE->t4_D;
        pCmd->pRegCmd->cbCoefficient.cfg4.bitfields.COEFF_E = pDataSCE->t4_E;
        pCmd->pRegCmd->cbCoefficient.cfg5.bitfields.COEFF_D = pDataSCE->t5_D;
        pCmd->pRegCmd->cbCoefficient.cfg5.bitfields.COEFF_E = pDataSCE->t5_E;

        pCmd->pRegCmd->crOffset.cfg0.bitfields.MATRIX_SHIFT = pDataSCE->t0_Q1;
        pCmd->pRegCmd->crOffset.cfg0.bitfields.OFFSET_C     = pDataSCE->t0_C;
        pCmd->pRegCmd->crOffset.cfg1.bitfields.MATRIX_SHIFT = pDataSCE->t1_Q1;
        pCmd->pRegCmd->crOffset.cfg1.bitfields.OFFSET_C     = pDataSCE->t1_C;
        pCmd->pRegCmd->crOffset.cfg2.bitfields.MATRIX_SHIFT = pDataSCE->t2_Q1;
        pCmd->pRegCmd->crOffset.cfg2.bitfields.OFFSET_C     = pDataSCE->t2_C;
        pCmd->pRegCmd->crOffset.cfg3.bitfields.MATRIX_SHIFT = pDataSCE->t3_Q1;
        pCmd->pRegCmd->crOffset.cfg3.bitfields.OFFSET_C     = pDataSCE->t3_C;
        pCmd->pRegCmd->crOffset.cfg4.bitfields.MATRIX_SHIFT = pDataSCE->t4_Q1;
        pCmd->pRegCmd->crOffset.cfg4.bitfields.OFFSET_C     = pDataSCE->t4_C;
        pCmd->pRegCmd->crOffset.cfg5.bitfields.MATRIX_SHIFT = pDataSCE->t5_Q1;
        pCmd->pRegCmd->crOffset.cfg5.bitfields.OFFSET_C     = pDataSCE->t5_C;

        pCmd->pRegCmd->cbOffset.cfg0.bitfields.OFFSET_SHIFT = pDataSCE->t0_Q2;
        pCmd->pRegCmd->cbOffset.cfg0.bitfields.OFFSET_F     = pDataSCE->t0_F;
        pCmd->pRegCmd->cbOffset.cfg1.bitfields.OFFSET_SHIFT = pDataSCE->t1_Q2;
        pCmd->pRegCmd->cbOffset.cfg1.bitfields.OFFSET_F     = pDataSCE->t1_F;
        pCmd->pRegCmd->cbOffset.cfg2.bitfields.OFFSET_SHIFT = pDataSCE->t2_Q2;
        pCmd->pRegCmd->cbOffset.cfg2.bitfields.OFFSET_F     = pDataSCE->t2_F;
        pCmd->pRegCmd->cbOffset.cfg3.bitfields.OFFSET_SHIFT = pDataSCE->t3_Q2;
        pCmd->pRegCmd->cbOffset.cfg3.bitfields.OFFSET_F     = pDataSCE->t3_F;
        pCmd->pRegCmd->cbOffset.cfg4.bitfields.OFFSET_SHIFT = pDataSCE->t4_Q2;
        pCmd->pRegCmd->cbOffset.cfg4.bitfields.OFFSET_F     = pDataSCE->t4_C;
        pCmd->pRegCmd->cbOffset.cfg5.bitfields.OFFSET_SHIFT = pDataSCE->t5_Q2;
        pCmd->pRegCmd->cbOffset.cfg5.bitfields.OFFSET_F     = pDataSCE->t5_F;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL ");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::SCE11CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::SCE11CalculateSetting(
    const SCE11InputData* pInput,
    VOID*                 pOEMIQData,
    SCE11OutputData*      pOutput)
{
    CamxResult                                            result         = CamxResultSuccess;
    BOOL                                                  commonIQResult = FALSE;
    sce_1_1_0::sce11_rgn_dataType*                        pInterpolationDataSCE = NULL;
    sce_1_1_0::chromatix_sce11_reserveType*               pReserveType = NULL;
    sce_1_1_0::chromatix_sce11Type::enable_sectionStruct* pModuleEnable = NULL;

    if ((NULL != pInput) && (NULL != pOutput) && (NULL != pInput->pChromatix))
    {
        if (NULL == pOEMIQData)
        {
            pInterpolationDataSCE = static_cast<sce_1_1_0::sce11_rgn_dataType*>(pInput->pInterpolationData);;
            pReserveType          = &(pInput->pChromatix->chromatix_sce11_reserve);
            pModuleEnable         = &(pInput->pChromatix->enable_section);

            // Call the Interpolation Calculation
            commonIQResult = IQInterface::s_interpolationTable.SCE11Interpolation(pInput, pInterpolationDataSCE);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationDataSCE = reinterpret_cast<sce_1_1_0::sce11_rgn_dataType* >(&(pOEMInput->SkinEnhancementSetting));

            if (NULL != pInterpolationDataSCE)
            {
                commonIQResult = TRUE;
                pReserveType   = reinterpret_cast<sce_1_1_0::chromatix_sce11_reserveType*>(
                                     &(pOEMInput->SkinEnhancementReserveType));
                pModuleEnable  = reinterpret_cast<sce_1_1_0::chromatix_sce11Type::enable_sectionStruct*>(
                                     &(pOEMInput->SkinEnhancementEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            SCE11UnpackedField unpackedRegisterDataSCE;

            commonIQResult = IQInterface::s_interpolationTable.SCE11CalculateHWSetting(
                                 pInput,
                                 pInterpolationDataSCE,
                                 pReserveType,
                                 pModuleEnable,
                                 static_cast<VOID*>(&unpackedRegisterDataSCE));

            if (TRUE == commonIQResult)
            {
                SetupSCE11Reg(&unpackedRegisterDataSCE, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("SCE CalculateHWSetting failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("SCE intepolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL ");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSSetupABF40Register
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID BPSSetupABF40Register(
    ABF40UnpackedField*   pDataABF,
    BLS12UnpackedField*   pDataBLS,
    ABF40OutputData*      pCmd)
{
    if ((NULL != pDataABF) && (NULL != pDataBLS) && (NULL != pCmd))
    {
        pCmd->pRegCmd->config0.bitfields.EDGE_SOFTNESS_GR       = pDataABF->edgeSoftness[1]; ///< [4] =>[R,GR,GB,B]
        pCmd->pRegCmd->config0.bitfields.EDGE_SOFTNESS_GB       = pDataABF->edgeSoftness[2];

        pCmd->pRegCmd->config1.bitfields.EDGE_SOFTNESS_R        = pDataABF->edgeSoftness[0];
        pCmd->pRegCmd->config1.bitfields.EDGE_SOFTNESS_B        = pDataABF->edgeSoftness[3];

        pCmd->pRegCmd->config2.bitfields.DISTANCE_LEVEL0_GRGB_0 = pDataABF->distanceLevel[0][3];
        pCmd->pRegCmd->config2.bitfields.DISTANCE_LEVEL0_GRGB_1 = pDataABF->distanceLevel[0][4];
        pCmd->pRegCmd->config2.bitfields.DISTANCE_LEVEL0_GRGB_2 = pDataABF->distanceLevel[0][5];
        pCmd->pRegCmd->config2.bitfields.DISTANCE_LEVEL1_GRGB_0 = pDataABF->distanceLevel[1][3];
        pCmd->pRegCmd->config2.bitfields.DISTANCE_LEVEL1_GRGB_1 = pDataABF->distanceLevel[1][4];
        pCmd->pRegCmd->config2.bitfields.DISTANCE_LEVEL1_GRGB_2 = pDataABF->distanceLevel[1][5];
        pCmd->pRegCmd->config2.bitfields.DISTANCE_LEVEL2_GRGB_0 = pDataABF->distanceLevel[2][3];
        pCmd->pRegCmd->config2.bitfields.DISTANCE_LEVEL2_GRGB_1 = pDataABF->distanceLevel[2][4];
        pCmd->pRegCmd->config2.bitfields.DISTANCE_LEVEL2_GRGB_2 = pDataABF->distanceLevel[2][5];

        pCmd->pRegCmd->config3.bitfields.DISTANCE_LEVEL0_RB_0   = pDataABF->distanceLevel[0][0];
        pCmd->pRegCmd->config3.bitfields.DISTANCE_LEVEL0_RB_1   = pDataABF->distanceLevel[0][1];
        pCmd->pRegCmd->config3.bitfields.DISTANCE_LEVEL0_RB_2   = pDataABF->distanceLevel[0][2];
        pCmd->pRegCmd->config3.bitfields.DISTANCE_LEVEL1_RB_0   = pDataABF->distanceLevel[1][0];
        pCmd->pRegCmd->config3.bitfields.DISTANCE_LEVEL1_RB_1   = pDataABF->distanceLevel[1][1];
        pCmd->pRegCmd->config3.bitfields.DISTANCE_LEVEL1_RB_2   = pDataABF->distanceLevel[1][2];
        pCmd->pRegCmd->config3.bitfields.DISTANCE_LEVEL2_RB_0   = pDataABF->distanceLevel[2][0];
        pCmd->pRegCmd->config3.bitfields.DISTANCE_LEVEL2_RB_1   = pDataABF->distanceLevel[2][1];
        pCmd->pRegCmd->config3.bitfields.DISTANCE_LEVEL2_RB_2   = pDataABF->distanceLevel[2][2];

        ///< curve offse[4] =>[R,GR,GB,B]
        pCmd->pRegCmd->config4.bitfields.CURVE_OFFSET_R         = pDataABF->curveOffset[0];
        pCmd->pRegCmd->config4.bitfields.CURVE_OFFSET_GR        = pDataABF->curveOffset[1];
        pCmd->pRegCmd->config4.bitfields.CURVE_OFFSET_GB        = pDataABF->curveOffset[2];
        pCmd->pRegCmd->config4.bitfields.CURVE_OFFSET_B         = pDataABF->curveOffset[3];

        pCmd->pRegCmd->config5.bitfields.FILTER_STRENGTH_R      = pDataABF->filterStrength[0];
        pCmd->pRegCmd->config5.bitfields.FILTER_STRENGTH_GR     = pDataABF->filterStrength[1];
        pCmd->pRegCmd->config6.bitfields.FILTER_STRENGTH_GB     = pDataABF->filterStrength[2];
        pCmd->pRegCmd->config6.bitfields.FILTER_STRENGTH_B      = pDataABF->filterStrength[3];

        pCmd->pRegCmd->config7.bitfields.MINMAX_BLS             = pDataABF->minmaxBLS;
        pCmd->pRegCmd->config7.bitfields.MINMAX_MAX_SHIFT       = pDataABF->minmaxMaxShift;
        pCmd->pRegCmd->config7.bitfields.MINMAX_MIN_SHIFT       = pDataABF->minmaxMinShift;
        pCmd->pRegCmd->config7.bitfields.MINMAX_OFFSET          = pDataABF->minmaxOffset;

        pCmd->pRegCmd->config8.bitfields.RNR_BX                 = pDataABF->bx;
        pCmd->pRegCmd->config8.bitfields.RNR_BY                 = pDataABF->by;

        pCmd->pRegCmd->config9.bitfields.RNR_INIT_RSQUARE       = pDataABF->rSquareInit;

        pCmd->pRegCmd->config10.bitfields.RNR_RSQUARE_SCALE     = pDataABF->rSquareScale;
        pCmd->pRegCmd->config10.bitfields.RNR_RSQUARE_SHIFT     = pDataABF->rSquareShift;

        pCmd->pRegCmd->config11.bitfields.RNR_ANCHOR_0          = pDataABF->RNRAnchor[0];
        pCmd->pRegCmd->config11.bitfields.RNR_ANCHOR_1          = pDataABF->RNRAnchor[1];

        pCmd->pRegCmd->config12.bitfields.RNR_ANCHOR_2          = pDataABF->RNRAnchor[2];
        pCmd->pRegCmd->config12.bitfields.RNR_ANCHOR_3          = pDataABF->RNRAnchor[3];

        pCmd->pRegCmd->config13.bitfields.RNR_NOISE_BASE_0      = pDataABF->RNRBase0[0];
        pCmd->pRegCmd->config13.bitfields.RNR_NOISE_BASE_1      = pDataABF->RNRBase0[1];

        pCmd->pRegCmd->config14.bitfields.RNR_NOISE_BASE_2      = pDataABF->RNRBase0[2];
        pCmd->pRegCmd->config14.bitfields.RNR_NOISE_BASE_3      = pDataABF->RNRBase0[3];

        pCmd->pRegCmd->config15.bitfields.RNR_NOISE_SLOPE_0     = pDataABF->RNRSlope0[0];
        pCmd->pRegCmd->config15.bitfields.RNR_NOISE_SLOPE_1     = pDataABF->RNRSlope0[1];

        pCmd->pRegCmd->config16.bitfields.RNR_NOISE_SLOPE_2     = pDataABF->RNRSlope0[2];
        pCmd->pRegCmd->config16.bitfields.RNR_NOISE_SLOPE_3     = pDataABF->RNRSlope0[3];

        pCmd->pRegCmd->config17.bitfields.RNR_NOISE_SHIFT_0     = pDataABF->RNRShift0[0];
        pCmd->pRegCmd->config17.bitfields.RNR_NOISE_SHIFT_1     = pDataABF->RNRShift0[1];
        pCmd->pRegCmd->config17.bitfields.RNR_NOISE_SHIFT_2     = pDataABF->RNRShift0[2];
        pCmd->pRegCmd->config17.bitfields.RNR_NOISE_SHIFT_3     = pDataABF->RNRShift0[3];
        pCmd->pRegCmd->config17.bitfields.RNR_THRESH_SHIFT_0    = pDataABF->RNRShift1[0];
        pCmd->pRegCmd->config17.bitfields.RNR_THRESH_SHIFT_1    = pDataABF->RNRShift1[1];
        pCmd->pRegCmd->config17.bitfields.RNR_THRESH_SHIFT_2    = pDataABF->RNRShift1[2];
        pCmd->pRegCmd->config17.bitfields.RNR_THRESH_SHIFT_3    = pDataABF->RNRShift1[3];

        pCmd->pRegCmd->config18.bitfields.RNR_THRESH_BASE_0     = pDataABF->RNRBase1[0];
        pCmd->pRegCmd->config18.bitfields.RNR_THRESH_BASE_1     = pDataABF->RNRBase1[1];

        pCmd->pRegCmd->config19.bitfields.RNR_THRESH_BASE_2     = pDataABF->RNRBase1[2];
        pCmd->pRegCmd->config19.bitfields.RNR_THRESH_BASE_3     = pDataABF->RNRBase1[3];

        pCmd->pRegCmd->config20.bitfields.RNR_THRESH_SLOPE_0    = pDataABF->RNRSlope1[0];
        pCmd->pRegCmd->config20.bitfields.RNR_THRESH_SLOPE_1    = pDataABF->RNRSlope1[1];

        pCmd->pRegCmd->config21.bitfields.RNR_THRESH_SLOPE_2    = pDataABF->RNRSlope1[2];
        pCmd->pRegCmd->config21.bitfields.RNR_THRESH_SLOPE_3    = pDataABF->RNRSlope1[3];

        pCmd->pRegCmd->config22.bitfields.NP_ANCHOR_0           = pDataABF->nprsvAnchor[0];
        pCmd->pRegCmd->config22.bitfields.NP_ANCHOR_1           = pDataABF->nprsvAnchor[1];

        pCmd->pRegCmd->config23.bitfields.NP_ANCHOR_2           = pDataABF->nprsvAnchor[2];
        pCmd->pRegCmd->config23.bitfields.NP_ANCHOR_3           = pDataABF->nprsvAnchor[3];

        pCmd->pRegCmd->config24.bitfields.NP_BASE_RB_0          = pDataABF->nprsvBase[0][0];
        pCmd->pRegCmd->config24.bitfields.NP_BASE_RB_1          = pDataABF->nprsvBase[0][1];

        pCmd->pRegCmd->config25.bitfields.NP_BASE_RB_2          = pDataABF->nprsvBase[0][2];
        pCmd->pRegCmd->config25.bitfields.NP_BASE_RB_3          = pDataABF->nprsvBase[0][3];

        pCmd->pRegCmd->config26.bitfields.NP_SLOPE_RB_0         = pDataABF->nprsvSlope[0][0];
        pCmd->pRegCmd->config26.bitfields.NP_SLOPE_RB_1         = pDataABF->nprsvSlope[0][1];

        pCmd->pRegCmd->config27.bitfields.NP_SLOPE_RB_2         = pDataABF->nprsvSlope[0][2];
        pCmd->pRegCmd->config27.bitfields.NP_SLOPE_RB_3         = pDataABF->nprsvSlope[0][3];

        pCmd->pRegCmd->config28.bitfields.NP_SHIFT_GRGB_0       = pDataABF->nprsvShift[1][0];
        pCmd->pRegCmd->config28.bitfields.NP_SHIFT_GRGB_1       = pDataABF->nprsvShift[1][1];
        pCmd->pRegCmd->config28.bitfields.NP_SHIFT_GRGB_2       = pDataABF->nprsvShift[1][2];
        pCmd->pRegCmd->config28.bitfields.NP_SHIFT_GRGB_3       = pDataABF->nprsvShift[1][3];
        pCmd->pRegCmd->config28.bitfields.NP_SHIFT_RB_0         = pDataABF->nprsvShift[0][0];
        pCmd->pRegCmd->config28.bitfields.NP_SHIFT_RB_1         = pDataABF->nprsvShift[0][1];
        pCmd->pRegCmd->config28.bitfields.NP_SHIFT_RB_2         = pDataABF->nprsvShift[0][2];
        pCmd->pRegCmd->config28.bitfields.NP_SHIFT_RB_3         = pDataABF->nprsvShift[0][3];

        pCmd->pRegCmd->config29.bitfields.NP_BASE_GRGB_0        = pDataABF->nprsvBase[1][0];
        pCmd->pRegCmd->config29.bitfields.NP_BASE_GRGB_1        = pDataABF->nprsvBase[1][1];

        pCmd->pRegCmd->config30.bitfields.NP_BASE_GRGB_2        = pDataABF->nprsvBase[1][2];
        pCmd->pRegCmd->config30.bitfields.NP_BASE_GRGB_3        = pDataABF->nprsvBase[1][3];

        pCmd->pRegCmd->config31.bitfields.NP_SLOPE_GRGB_0       = pDataABF->nprsvSlope[1][0];
        pCmd->pRegCmd->config31.bitfields.NP_SLOPE_GRGB_1       = pDataABF->nprsvSlope[1][1];

        pCmd->pRegCmd->config32.bitfields.NP_SLOPE_GRGB_2       = pDataABF->nprsvSlope[1][2];
        pCmd->pRegCmd->config32.bitfields.NP_SLOPE_GRGB_3       = pDataABF->nprsvSlope[1][3];

        pCmd->pRegCmd->config33.bitfields.ACT_FAC0              = pDataABF->activityFactor0;
        pCmd->pRegCmd->config33.bitfields.ACT_FAC1              = pDataABF->activityFactor1;

        pCmd->pRegCmd->config34.bitfields.ACT_THD0              = pDataABF->activityThreshold0;
        pCmd->pRegCmd->config34.bitfields.ACT_THD1              = pDataABF->activityThreshold1;

        pCmd->pRegCmd->config35.bitfields.ACT_SMOOTH_THD0       = pDataABF->activitySmoothThreshold0;
        pCmd->pRegCmd->config35.bitfields.ACT_SMOOTH_THD1       = pDataABF->activitySmoothThreshold1;
        pCmd->pRegCmd->config35.bitfields.DARK_THD              = pDataABF->darkThreshold;

        pCmd->pRegCmd->config36.bitfields.GR_RATIO              = pDataABF->grRatio;
        pCmd->pRegCmd->config36.bitfields.RG_RATIO              = pDataABF->rgRatio;

        pCmd->pRegCmd->config37.bitfields.BG_RATIO              = pDataABF->bgRatio;
        pCmd->pRegCmd->config37.bitfields.GB_RATIO              = pDataABF->gbRatio;

        pCmd->pRegCmd->config38.bitfields.BR_RATIO              = pDataABF->brRatio;
        pCmd->pRegCmd->config38.bitfields.RB_RATIO              = pDataABF->rbRatio;

        pCmd->pRegCmd->config39.bitfields.EDGE_COUNT_THD              = pDataABF->edgeCountLow;
        pCmd->pRegCmd->config39.bitfields.EDGE_DETECT_THD             = pDataABF->edgeDetectThreshold;
        pCmd->pRegCmd->config39.bitfields.EDGE_DETECT_NOISE_SCALAR    = pDataABF->edgeDetectNoiseScalar;
        pCmd->pRegCmd->config39.bitfields.EDGE_SMOOTH_STRENGTH        = pDataABF->edgeSmoothStrength;

        pCmd->pRegCmd->config40.bitfields.EDGE_SMOOTH_NOISE_SCALAR_R  = pDataABF->edgeSmoothNoiseScalar[0];
        pCmd->pRegCmd->config40.bitfields.EDGE_SMOOTH_NOISE_SCALAR_GR = pDataABF->edgeSmoothNoiseScalar[1];

        pCmd->pRegCmd->config41.bitfields.EDGE_SMOOTH_NOISE_SCALAR_GB = pDataABF->edgeSmoothNoiseScalar[2];
        pCmd->pRegCmd->config41.bitfields.EDGE_SMOOTH_NOISE_SCALAR_B  = pDataABF->edgeSmoothNoiseScalar[3];

        /// Updating Data to fill in DMI Buffers
        for (UINT32 index = 0; index < DMIRAM_ABF40_NOISESTD_LENGTH; index++)
        {
            pCmd->pNoiseLUT[index]  = pDataABF->noiseStdLUT[pDataABF->LUTBankSel][index];
            pCmd->pNoiseLUT1[index] = pDataABF->noiseStdLUT[pDataABF->LUTBankSel][index];
        }

        for (UINT32 index = 0; index < DMIRAM_ABF40_ACTIVITY_LENGTH; index++)
        {
            pCmd->pActivityLUT[index] = pDataABF->activityFactorLUT[pDataABF->LUTBankSel][index];
        }

        for (UINT32 index = 0; index < DMIRAM_ABF40_DARK_LENGTH; index++)
        {
            pCmd->pDarkLUT[index] = pDataABF->darkFactorLUT[pDataABF->LUTBankSel][index];
        }

        pCmd->pRegCmd->config42.bitfields.BLS_THRESH_GR = pDataBLS->thresholdGR;
        pCmd->pRegCmd->config42.bitfields.BLS_THRESH_R  = pDataBLS->thresholdR;
        pCmd->pRegCmd->config43.bitfields.BLS_THRESH_B  = pDataBLS->thresholdB;
        pCmd->pRegCmd->config43.bitfields.BLS_THRESH_GB = pDataBLS->thresholdGB;
        pCmd->pRegCmd->config44.bitfields.BLS_OFFSET    = pDataBLS->offset;
        pCmd->pRegCmd->config45.bitfields.BLS_SCALE     = pDataBLS->scale;

        pCmd->pModuleConfig->moduleConfig.bitfields.ACT_ADJ_EN             = pDataABF->actAdjEnable;
        pCmd->pModuleConfig->moduleConfig.bitfields.BLOCK_MATCH_PATTERN_RB = pDataABF->blockOpt;
        pCmd->pModuleConfig->moduleConfig.bitfields.CROSS_PLANE_EN         = pDataABF->crossProcessEnable;
        pCmd->pModuleConfig->moduleConfig.bitfields.DARK_DESAT_EN          = pDataABF->darkDesatEnable;
        pCmd->pModuleConfig->moduleConfig.bitfields.DARK_SMOOTH_EN         = pDataABF->darkSmoothEnable;
        pCmd->pModuleConfig->moduleConfig.bitfields.DIR_SMOOTH_EN          = pDataABF->directSmoothEnable;
        pCmd->pModuleConfig->moduleConfig.bitfields.EN                     = pDataABF->enable;
        pCmd->pModuleConfig->moduleConfig.bitfields.FILTER_EN              = pDataABF->bilateralEnable;
        pCmd->pModuleConfig->moduleConfig.bitfields.MINMAX_EN              = pDataABF->minmaxEnable;
        pCmd->pModuleConfig->moduleConfig.bitfields.PIX_MATCH_LEVEL_RB     = pDataABF->blockPixLevel[0];
        pCmd->pModuleConfig->moduleConfig.bitfields.PIX_MATCH_LEVEL_G      = pDataABF->blockPixLevel[1];
        pCmd->pModuleConfig->moduleConfig.bitfields.BLS_EN                 = pDataBLS->enable;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pdataABF %p pDataBLS %p pCmd %p ", pDataABF, pDataBLS, pCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SetupCS20Register
///
/// @brief  Setup CS20 Register based on common library output
///
/// @param  pData   Pointer to the Common Library Calculation Result
/// @param  pCmd    Pointer to the CS20 Register and DMI Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID SetupCS20Register(
    const CS20UnpackedField* pData,
    CS20OutputData*          pOutput)
{
    IPEChromaSuppressionRegCmd* pRegCmd;

    if ((NULL != pOutput)  &&
        (NULL != pData) &&
        (NULL != pOutput->pRegCmd))
    {
        pRegCmd = pOutput->pRegCmd;

        pRegCmd->inverse.cfg0.bitfields.Y_KNEE_INV_LUT_0  = pData->knee_point_inverse_lut[0];
        pRegCmd->inverse.cfg0.bitfields.Y_KNEE_INV_LUT_1  = pData->knee_point_inverse_lut[1];
        pRegCmd->inverse.cfg1.bitfields.Y_KNEE_INV_LUT_2  = pData->knee_point_inverse_lut[2];
        pRegCmd->inverse.cfg1.bitfields.Y_KNEE_INV_LUT_3  = pData->knee_point_inverse_lut[3];
        pRegCmd->inverse.cfg2.bitfields.Y_KNEE_INV_LUT_4  = pData->knee_point_inverse_lut[4];
        pRegCmd->inverse.cfg2.bitfields.Y_KNEE_INV_LUT_5  = pData->knee_point_inverse_lut[5];
        pRegCmd->inverse.cfg3.bitfields.Y_KNEE_INV_LUT_6  = pData->knee_point_inverse_lut[6];
        pRegCmd->inverse.cfg3.bitfields.Y_KNEE_INV_LUT_7  = pData->knee_point_inverse_lut[7];
        pRegCmd->inverse.cfg4.bitfields.Y_KNEE_INV_LUT_8  = pData->knee_point_inverse_lut[8];
        pRegCmd->inverse.cfg4.bitfields.Y_KNEE_INV_LUT_9  = pData->knee_point_inverse_lut[9];
        pRegCmd->inverse.cfg5.bitfields.Y_KNEE_INV_LUT_10 = pData->knee_point_inverse_lut[10];
        pRegCmd->inverse.cfg5.bitfields.Y_KNEE_INV_LUT_11 = pData->knee_point_inverse_lut[11];
        pRegCmd->inverse.cfg6.bitfields.Y_KNEE_INV_LUT_12 = pData->knee_point_inverse_lut[12];
        pRegCmd->inverse.cfg6.bitfields.Y_KNEE_INV_LUT_13 = pData->knee_point_inverse_lut[13];
        pRegCmd->inverse.cfg7.bitfields.Y_KNEE_INV_LUT_14 = pData->knee_point_inverse_lut[14];
        pRegCmd->inverse.cfg7.bitfields.Y_KNEE_INV_LUT_15 = pData->knee_point_inverse_lut[15];

        pRegCmd->knee.cfg0.bitfields.Y_KNEE_PT_LUT_0  = pData->knee_point_lut[0];
        pRegCmd->knee.cfg0.bitfields.Y_KNEE_PT_LUT_1  = pData->knee_point_lut[1];
        pRegCmd->knee.cfg1.bitfields.Y_KNEE_PT_LUT_2  = pData->knee_point_lut[2];
        pRegCmd->knee.cfg1.bitfields.Y_KNEE_PT_LUT_3  = pData->knee_point_lut[3];
        pRegCmd->knee.cfg2.bitfields.Y_KNEE_PT_LUT_4  = pData->knee_point_lut[4];
        pRegCmd->knee.cfg2.bitfields.Y_KNEE_PT_LUT_5  = pData->knee_point_lut[5];
        pRegCmd->knee.cfg3.bitfields.Y_KNEE_PT_LUT_6  = pData->knee_point_lut[6];
        pRegCmd->knee.cfg3.bitfields.Y_KNEE_PT_LUT_7  = pData->knee_point_lut[7];
        pRegCmd->knee.cfg4.bitfields.Y_KNEE_PT_LUT_8  = pData->knee_point_lut[8];
        pRegCmd->knee.cfg4.bitfields.Y_KNEE_PT_LUT_9  = pData->knee_point_lut[9];
        pRegCmd->knee.cfg5.bitfields.Y_KNEE_PT_LUT_10 = pData->knee_point_lut[10];
        pRegCmd->knee.cfg5.bitfields.Y_KNEE_PT_LUT_11 = pData->knee_point_lut[11];
        pRegCmd->knee.cfg6.bitfields.Y_KNEE_PT_LUT_12 = pData->knee_point_lut[12];
        pRegCmd->knee.cfg6.bitfields.Y_KNEE_PT_LUT_13 = pData->knee_point_lut[13];
        pRegCmd->knee.cfg7.bitfields.Y_KNEE_PT_LUT_14 = pData->knee_point_lut[14];
        pRegCmd->knee.cfg7.bitfields.Y_KNEE_PT_LUT_15 = pData->knee_point_lut[15];

        pRegCmd->qRes.chroma.bitfields.Q_RES_CHROMA = pData->chroma_q;
        pRegCmd->qRes.luma.bitfields.Q_RES_LUMA     = pData->luma_q;

        pRegCmd->slope.cfg0.bitfields.CHROMA_SLP_LUT_0  = pData->c_slope_lut[0];
        pRegCmd->slope.cfg0.bitfields.CHROMA_SLP_LUT_1  = pData->c_slope_lut[1];
        pRegCmd->slope.cfg1.bitfields.CHROMA_SLP_LUT_2  = pData->c_slope_lut[2];
        pRegCmd->slope.cfg1.bitfields.CHROMA_SLP_LUT_3  = pData->c_slope_lut[3];
        pRegCmd->slope.cfg2.bitfields.CHROMA_SLP_LUT_4  = pData->c_slope_lut[4];
        pRegCmd->slope.cfg2.bitfields.CHROMA_SLP_LUT_5  = pData->c_slope_lut[5];
        pRegCmd->slope.cfg3.bitfields.CHROMA_SLP_LUT_6  = pData->c_slope_lut[6];
        pRegCmd->slope.cfg3.bitfields.CHROMA_SLP_LUT_7  = pData->c_slope_lut[7];
        pRegCmd->slope.cfg4.bitfields.CHROMA_SLP_LUT_8  = pData->c_slope_lut[8];
        pRegCmd->slope.cfg4.bitfields.CHROMA_SLP_LUT_9  = pData->c_slope_lut[9];
        pRegCmd->slope.cfg5.bitfields.CHROMA_SLP_LUT_10 = pData->c_slope_lut[10];
        pRegCmd->slope.cfg5.bitfields.CHROMA_SLP_LUT_11 = pData->c_slope_lut[11];
        pRegCmd->slope.cfg6.bitfields.CHROMA_SLP_LUT_12 = pData->c_slope_lut[12];
        pRegCmd->slope.cfg6.bitfields.CHROMA_SLP_LUT_13 = pData->c_slope_lut[13];
        pRegCmd->slope.cfg7.bitfields.CHROMA_SLP_LUT_14 = pData->c_slope_lut[14];
        pRegCmd->slope.cfg7.bitfields.CHROMA_SLP_LUT_15 = pData->c_slope_lut[15];

        pRegCmd->threshold.cfg0.bitfields.CHROMA_TH_LUT_0  = pData->c_thr_lut[0];
        pRegCmd->threshold.cfg0.bitfields.CHROMA_TH_LUT_1  = pData->c_thr_lut[1];
        pRegCmd->threshold.cfg1.bitfields.CHROMA_TH_LUT_2  = pData->c_thr_lut[2];
        pRegCmd->threshold.cfg1.bitfields.CHROMA_TH_LUT_3  = pData->c_thr_lut[3];
        pRegCmd->threshold.cfg2.bitfields.CHROMA_TH_LUT_4  = pData->c_thr_lut[4];
        pRegCmd->threshold.cfg2.bitfields.CHROMA_TH_LUT_5  = pData->c_thr_lut[5];
        pRegCmd->threshold.cfg3.bitfields.CHROMA_TH_LUT_6  = pData->c_thr_lut[6];
        pRegCmd->threshold.cfg3.bitfields.CHROMA_TH_LUT_7  = pData->c_thr_lut[7];
        pRegCmd->threshold.cfg4.bitfields.CHROMA_TH_LUT_8  = pData->c_thr_lut[8];
        pRegCmd->threshold.cfg4.bitfields.CHROMA_TH_LUT_9  = pData->c_thr_lut[9];
        pRegCmd->threshold.cfg5.bitfields.CHROMA_TH_LUT_10 = pData->c_thr_lut[10];
        pRegCmd->threshold.cfg5.bitfields.CHROMA_TH_LUT_11 = pData->c_thr_lut[11];
        pRegCmd->threshold.cfg6.bitfields.CHROMA_TH_LUT_12 = pData->c_thr_lut[12];
        pRegCmd->threshold.cfg6.bitfields.CHROMA_TH_LUT_13 = pData->c_thr_lut[13];
        pRegCmd->threshold.cfg7.bitfields.CHROMA_TH_LUT_14 = pData->c_thr_lut[14];
        pRegCmd->threshold.cfg7.bitfields.CHROMA_TH_LUT_15 = pData->c_thr_lut[15];
        pRegCmd->weight.cfg0.bitfields.Y_WEIGHT_LUT_0  = pData->y_weight_lut[0];
        pRegCmd->weight.cfg0.bitfields.Y_WEIGHT_LUT_1  = pData->y_weight_lut[1];
        pRegCmd->weight.cfg1.bitfields.Y_WEIGHT_LUT_2  = pData->y_weight_lut[2];
        pRegCmd->weight.cfg1.bitfields.Y_WEIGHT_LUT_3  = pData->y_weight_lut[3];
        pRegCmd->weight.cfg2.bitfields.Y_WEIGHT_LUT_4  = pData->y_weight_lut[4];
        pRegCmd->weight.cfg2.bitfields.Y_WEIGHT_LUT_5  = pData->y_weight_lut[5];
        pRegCmd->weight.cfg3.bitfields.Y_WEIGHT_LUT_6  = pData->y_weight_lut[6];
        pRegCmd->weight.cfg3.bitfields.Y_WEIGHT_LUT_7  = pData->y_weight_lut[7];
        pRegCmd->weight.cfg4.bitfields.Y_WEIGHT_LUT_8  = pData->y_weight_lut[8];
        pRegCmd->weight.cfg4.bitfields.Y_WEIGHT_LUT_9  = pData->y_weight_lut[9];
        pRegCmd->weight.cfg5.bitfields.Y_WEIGHT_LUT_10 = pData->y_weight_lut[10];
        pRegCmd->weight.cfg5.bitfields.Y_WEIGHT_LUT_11 = pData->y_weight_lut[11];
        pRegCmd->weight.cfg6.bitfields.Y_WEIGHT_LUT_12 = pData->y_weight_lut[12];
        pRegCmd->weight.cfg6.bitfields.Y_WEIGHT_LUT_13 = pData->y_weight_lut[13];
        pRegCmd->weight.cfg7.bitfields.Y_WEIGHT_LUT_14 = pData->y_weight_lut[14];
        pRegCmd->weight.cfg7.bitfields.Y_WEIGHT_LUT_15 = pData->y_weight_lut[15];

    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("data is NULL %p %p", pData, pOutput);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::BPSABF40CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::BPSABF40CalculateSetting(
    ABF40InputData*  pInput,
    VOID*            pOEMIQData,
    ABF40OutputData* pOutput)
{
    CamxResult                                            result                = CamxResultSuccess;
    BOOL                                                  commonIQResultABF     = FALSE;
    BOOL                                                  commonIQResultBLS     = FALSE;
    abf_4_0_0::abf40_rgn_dataType*                        pInterpolationDataABF = NULL;
    ABF40UnpackedField                                    unpackedRegisterDataABF;
    bls_1_2_0::bls12_rgn_dataType*                        pInterpolationDataBLS = NULL;
    bls_1_2_0::bls12_rgn_dataType                         interpolationDataBLS[BLS12MaxmiumNonLeafNode];
    BLS12UnpackedField                                    unpackedRegisterDataBLS;
    abf_4_0_0::chromatix_abf40_reserveType*               pReserveType          = NULL;
    abf_4_0_0::chromatix_abf40Type::enable_sectionStruct* pABFModuleEnable      = NULL;
    bls_1_2_0::chromatix_bls12Type::enable_sectionStruct* pBLSModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationDataABF = static_cast<abf_4_0_0::abf40_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResultABF     = IQInterface::s_interpolationTable.BPSABF40Interpolation(pInput,
                                                                                            pInterpolationDataABF);
            pReserveType          = &(pInput->pChromatix->chromatix_abf40_reserve);
            pABFModuleEnable      = &(pInput->pChromatix->enable_section);
            pBLSModuleEnable      = &(pInput->BLSData.pChromatix->enable_section);

            pInterpolationDataBLS = &interpolationDataBLS[0];
            commonIQResultBLS     = IQInterface::s_interpolationTable.BLS12Interpolation(&pInput->BLSData,
                                                                                         pInterpolationDataBLS);
        }
        else
        {
            OEMBPSIQSetting* pOEMInputBPS = static_cast<OEMBPSIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data for BPS::ABF
            pInterpolationDataABF = reinterpret_cast<abf_4_0_0::abf40_rgn_dataType*>(&(pOEMInputBPS->ABFSetting));

            if (NULL != pInterpolationDataABF)
            {
                commonIQResultABF = TRUE;
                pReserveType      = reinterpret_cast<abf_4_0_0::chromatix_abf40_reserveType*>(
                                        &(pOEMInputBPS->ABFReserveType));
                pABFModuleEnable  = reinterpret_cast<abf_4_0_0::chromatix_abf40Type::enable_sectionStruct*>(
                                        &(pOEMInputBPS->ABFEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }

            // Use OEM defined interpolation data for BPS::BLS
            pInterpolationDataBLS = reinterpret_cast<bls_1_2_0::bls12_rgn_dataType*>(&(pOEMInputBPS->BLSSetting));

            if (NULL != pInterpolationDataBLS)
            {
                commonIQResultBLS = TRUE;
                pBLSModuleEnable  = reinterpret_cast<bls_1_2_0::chromatix_bls12Type::enable_sectionStruct*>(
                                        &(pOEMInputBPS->BLSEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }

        }

        if ((TRUE == commonIQResultABF) || (TRUE == commonIQResultBLS))
        {
            commonIQResultABF =
                IQInterface::s_interpolationTable.BPSABF40CalculateHWSetting(pInput,
                                                                             pInterpolationDataABF,
                                                                             pReserveType,
                                                                             pABFModuleEnable,
                                                                             static_cast<VOID*>(&unpackedRegisterDataABF));

            if (TRUE != commonIQResultABF)
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("ABF40 Hardware setting failed.");
            }

            commonIQResultBLS =
                IQInterface::s_interpolationTable.BLS12CalculateHWSetting(&pInput->BLSData,
                                                                          pInterpolationDataBLS,
                                                                          pBLSModuleEnable,
                                                                          static_cast<VOID*>(&unpackedRegisterDataBLS));
            if (TRUE != commonIQResultBLS)
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("BLS12 Hardware setting failed.");
            }
        }

        if ((TRUE == commonIQResultABF) || (TRUE == commonIQResultBLS))
        {
            BPSSetupABF40Register(&unpackedRegisterDataABF, &unpackedRegisterDataBLS, pOutput);
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("ABF40 intepolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is pInput %p pOutput  %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IPECS20CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPECS20CalculateSetting(
    const CS20InputData* pInput,
    VOID*                pOEMIQData,
    CS20OutputData*      pOutput)
{
    CamxResult                                          result             = CamxResultSuccess;
    BOOL                                                commonIQResult     = FALSE;
    cs_2_0_0::cs20_rgn_dataType*                        pInterpolationData = NULL;
    cs_2_0_0::chromatix_cs20_reserveType*               pReserveType       = NULL;
    cs_2_0_0::chromatix_cs20Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<cs_2_0_0::cs20_rgn_dataType*>(pInput->pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_cs20_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            // Call the Interpolation Calculation
            commonIQResult = IQInterface::s_interpolationTable.IPECS20Interpolation(pInput, pInterpolationData);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<cs_2_0_0::cs20_rgn_dataType*>(&(pOEMInput->ChromaSuppressionSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveType   = reinterpret_cast<cs_2_0_0::chromatix_cs20_reserveType*>(
                                     &(pOEMInput->ChromaSuppressionReserveType));
                pModuleEnable  = reinterpret_cast<cs_2_0_0::chromatix_cs20Type::enable_sectionStruct*>(
                                     &(pOEMInput->ChromaSuppressionEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            CS20UnpackedField unpackedRegisterData;

            commonIQResult = IQInterface::s_interpolationTable.IPECS20CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pReserveType,
                                 pModuleEnable,
                                 static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                SetupCS20Register(&unpackedRegisterData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("CS20 calculate hardwware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("CS20 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Data is NULL %p %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupASF30Register
///
/// @brief  Setup ASF30 Register based on common library output
///
/// @param  pInput     Pointer to the Common Library Calculation Input
/// @param  pData      Pointer to the Common Library Calculation Result
/// @param  pOutput    Pointer to the ASF30 output data (register set, DMI data pointer, etc.)
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupASF30Register(
    const ASF30InputData*   pInput,
    ASF30UnpackedField*     pData,
    ASF30OutputData*        pOutput)
{
    if ((NULL != pData)            &&
        (NULL != pOutput)          &&
        (NULL != pInput)           &&
        (NULL != pOutput->pRegCmd) &&
        (NULL != pOutput->pDMIDataPtr))
    {
        // Fill up Registers
        IPEASFRegCmd* pRegCmd = pOutput->pRegCmd;
        pRegCmd->ASFLayer1Config.bitfields.ACTIVITY_CLAMP_THRESHOLD         = pData->layer1ActivityClampThreshold;
        pRegCmd->ASFLayer1Config.bitfields.CLAMP_LL                         = pData->layer1RegTL;
        pRegCmd->ASFLayer1Config.bitfields.CLAMP_UL                         = pData->layer1RegTH;

        pRegCmd->ASFLayer2Config.bitfields.ACTIVITY_CLAMP_THRESHOLD         = pData->layer2ActivityClampThreshold;
        pRegCmd->ASFLayer2Config.bitfields.CLAMP_LL                         = pData->layer2RegTL;
        pRegCmd->ASFLayer2Config.bitfields.CLAMP_UL                         = pData->layer2RegTH;

        pRegCmd->ASFNormalScaleConfig.bitfields.LAYER_1_L2_NORM_EN          = pData->layer1L2NormEn;
        pRegCmd->ASFNormalScaleConfig.bitfields.LAYER_1_NORM_SCALE          = pData->layer1NormScale;
        pRegCmd->ASFNormalScaleConfig.bitfields.LAYER_2_L2_NORM_EN          = pData->layer2L2NormEn;
        pRegCmd->ASFNormalScaleConfig.bitfields.LAYER_2_NORM_SCALE          = pData->layer2NormScale;

        pRegCmd->ASFGainConfig.bitfields.GAMMA_CORRECTED_LUMA_TARGET        = pData->gammaCorrectedLumaTarget;
        pRegCmd->ASFGainConfig.bitfields.GAIN_CAP                           = pData->gainCap;
        pRegCmd->ASFGainConfig.bitfields.MEDIAN_BLEND_LOWER_OFFSET          = pData->medianBlendLowerOffset;
        pRegCmd->ASFGainConfig.bitfields.MEDIAN_BLEND_UPPER_OFFSET          = pData->medianBlendUpperOffset;

        pRegCmd->ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_0                 = pData->nonZero[0];
        pRegCmd->ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_1                 = pData->nonZero[1];
        pRegCmd->ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_2                 = pData->nonZero[2];
        pRegCmd->ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_3                 = pData->nonZero[3];
        pRegCmd->ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_4                 = pData->nonZero[4];
        pRegCmd->ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_5                 = pData->nonZero[5];
        pRegCmd->ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_6                 = pData->nonZero[6];
        pRegCmd->ASFNegativeAndZeroFlag.bitfields.NZ_FLAG_7                 = pData->nonZero[7];

        pRegCmd->ASFLayer1SharpnessCoeffConfig0.bitfields.COEFF0            = pData->layer1CornerFilter[0];
        pRegCmd->ASFLayer1SharpnessCoeffConfig0.bitfields.COEFF1            = pData->layer1CornerFilter[1];
        pRegCmd->ASFLayer1SharpnessCoeffConfig1.bitfields.COEFF2            = pData->layer1CornerFilter[2];
        pRegCmd->ASFLayer1SharpnessCoeffConfig1.bitfields.COEFF3            = pData->layer1CornerFilter[3];
        pRegCmd->ASFLayer1SharpnessCoeffConfig2.bitfields.COEFF4            = pData->layer1CornerFilter[4];
        pRegCmd->ASFLayer1SharpnessCoeffConfig2.bitfields.COEFF5            = pData->layer1CornerFilter[5];
        pRegCmd->ASFLayer1SharpnessCoeffConfig3.bitfields.COEFF6            = pData->layer1CornerFilter[6];
        pRegCmd->ASFLayer1SharpnessCoeffConfig3.bitfields.COEFF7            = pData->layer1CornerFilter[7];
        pRegCmd->ASFLayer1SharpnessCoeffConfig4.bitfields.COEFF8            = pData->layer1CornerFilter[8];
        pRegCmd->ASFLayer1SharpnessCoeffConfig4.bitfields.COEFF9            = pData->layer1CornerFilter[9];

        pRegCmd->ASFLayer1LPFCoeffConfig0.bitfields.COEFF0                  = pData->layer1LowPassFilter[0];
        pRegCmd->ASFLayer1LPFCoeffConfig0.bitfields.COEFF1                  = pData->layer1LowPassFilter[1];
        pRegCmd->ASFLayer1LPFCoeffConfig1.bitfields.COEFF2                  = pData->layer1LowPassFilter[2];
        pRegCmd->ASFLayer1LPFCoeffConfig1.bitfields.COEFF3                  = pData->layer1LowPassFilter[3];
        pRegCmd->ASFLayer1LPFCoeffConfig2.bitfields.COEFF4                  = pData->layer1LowPassFilter[4];
        pRegCmd->ASFLayer1LPFCoeffConfig2.bitfields.COEFF5                  = pData->layer1LowPassFilter[5];
        pRegCmd->ASFLayer1LPFCoeffConfig3.bitfields.COEFF6                  = pData->layer1LowPassFilter[6];
        pRegCmd->ASFLayer1LPFCoeffConfig3.bitfields.COEFF7                  = pData->layer1LowPassFilter[7];
        pRegCmd->ASFLayer1LPFCoeffConfig4.bitfields.COEFF8                  = pData->layer1LowPassFilter[8];
        pRegCmd->ASFLayer1LPFCoeffConfig4.bitfields.COEFF9                  = pData->layer1LowPassFilter[9];

        pRegCmd->ASFLayer1ActivityBPFCoeffConfig0.bitfields.COEFF0          = pData->layer1ActivityBPF[0];
        pRegCmd->ASFLayer1ActivityBPFCoeffConfig0.bitfields.COEFF1          = pData->layer1ActivityBPF[1];
        pRegCmd->ASFLayer1ActivityBPFCoeffConfig1.bitfields.COEFF2          = pData->layer1ActivityBPF[2];
        pRegCmd->ASFLayer1ActivityBPFCoeffConfig1.bitfields.COEFF3          = pData->layer1ActivityBPF[3];
        pRegCmd->ASFLayer1ActivityBPFCoeffConfig2.bitfields.COEFF4          = pData->layer1ActivityBPF[4];
        pRegCmd->ASFLayer1ActivityBPFCoeffConfig2.bitfields.COEFF5          = pData->layer1ActivityBPF[5];

        pRegCmd->ASFLayer2SharpnessCoeffConfig0.bitfields.COEFF0            = pData->layer2CornerFilter[0];
        pRegCmd->ASFLayer2SharpnessCoeffConfig0.bitfields.COEFF1            = pData->layer2CornerFilter[1];
        pRegCmd->ASFLayer2SharpnessCoeffConfig1.bitfields.COEFF2            = pData->layer2CornerFilter[2];
        pRegCmd->ASFLayer2SharpnessCoeffConfig1.bitfields.COEFF3            = pData->layer2CornerFilter[3];
        pRegCmd->ASFLayer2SharpnessCoeffConfig2.bitfields.COEFF4            = pData->layer2CornerFilter[4];
        pRegCmd->ASFLayer2SharpnessCoeffConfig2.bitfields.COEFF5            = pData->layer2CornerFilter[5];

        pRegCmd->ASFLayer2LPFCoeffConfig0.bitfields.COEFF0                  = pData->layer2LowPassFilter[0];
        pRegCmd->ASFLayer2LPFCoeffConfig0.bitfields.COEFF1                  = pData->layer2LowPassFilter[1];
        pRegCmd->ASFLayer2LPFCoeffConfig1.bitfields.COEFF2                  = pData->layer2LowPassFilter[2];
        pRegCmd->ASFLayer2LPFCoeffConfig1.bitfields.COEFF3                  = pData->layer2LowPassFilter[3];
        pRegCmd->ASFLayer2LPFCoeffConfig2.bitfields.COEFF4                  = pData->layer2LowPassFilter[4];
        pRegCmd->ASFLayer2LPFCoeffConfig2.bitfields.COEFF5                  = pData->layer2LowPassFilter[5];

        pRegCmd->ASFLayer2ActivityBPFCoeffConfig0.bitfields.COEFF0          = pData->layer2ActivityBPF[0];
        pRegCmd->ASFLayer2ActivityBPFCoeffConfig0.bitfields.COEFF1          = pData->layer2ActivityBPF[1];
        pRegCmd->ASFLayer2ActivityBPFCoeffConfig1.bitfields.COEFF2          = pData->layer2ActivityBPF[2];
        pRegCmd->ASFLayer2ActivityBPFCoeffConfig1.bitfields.COEFF3          = pData->layer2ActivityBPF[3];
        pRegCmd->ASFLayer2ActivityBPFCoeffConfig2.bitfields.COEFF4          = pData->layer2ActivityBPF[4];
        pRegCmd->ASFLayer2ActivityBPFCoeffConfig2.bitfields.COEFF5          = pData->layer2ActivityBPF[5];

        pRegCmd->ASFRNRRSquareLUTEntry0.bitfields.R_SQUARE                  = pData->rSquareTable[0];
        pRegCmd->ASFRNRRSquareLUTEntry1.bitfields.R_SQUARE                  = pData->rSquareTable[1];
        pRegCmd->ASFRNRRSquareLUTEntry2.bitfields.R_SQUARE                  = pData->rSquareTable[2];

        pRegCmd->ASFRNRSquareConfig0.bitfields.R_SQUARE_SCALE               = pData->rSquareScale;
        pRegCmd->ASFRNRSquareConfig0.bitfields.R_SQUARE_SHIFT               = pData->rSquareShift;

        pRegCmd->ASFRNRActivityCFLUTEntry0.bitfields.ACTIVITY_CF            = pData->radialActivityCFTable[0];
        pRegCmd->ASFRNRActivityCFLUTEntry1.bitfields.ACTIVITY_CF            = pData->radialActivityCFTable[1];
        pRegCmd->ASFRNRActivityCFLUTEntry2.bitfields.ACTIVITY_CF            = pData->radialActivityCFTable[2];

        pRegCmd->ASFRNRActivitySlopeLUTEntry0.bitfields.ACTIVITY_SLOPE      = pData->radialActivitySlopeTable[0];
        pRegCmd->ASFRNRActivitySlopeLUTEntry1.bitfields.ACTIVITY_SLOPE      = pData->radialActivitySlopeTable[1];
        pRegCmd->ASFRNRActivitySlopeLUTEntry2.bitfields.ACTIVITY_SLOPE      = pData->radialActivitySlopeTable[2];

        pRegCmd->ASFRNRActivityCFShiftLUTEntry0.bitfields.ACTIVITY_CF_SHIFT = pData->radialActivityCFShift[0];
        pRegCmd->ASFRNRActivityCFShiftLUTEntry1.bitfields.ACTIVITY_CF_SHIFT = pData->radialActivityCFShift[1];
        pRegCmd->ASFRNRActivityCFShiftLUTEntry2.bitfields.ACTIVITY_CF_SHIFT = pData->radialActivityCFShift[2];

        pRegCmd->ASFRNRGainCFLUTEntry0.bitfields.GAIN_CF                    = pData->radialGainCFTable[0];
        pRegCmd->ASFRNRGainCFLUTEntry1.bitfields.GAIN_CF                    = pData->radialGainCFTable[1];
        pRegCmd->ASFRNRGainCFLUTEntry2.bitfields.GAIN_CF                    = pData->radialGainCFTable[2];

        pRegCmd->ASFRNRGainSlopeLUTEntry0.bitfields.GAIN_SLOPE              = pData->radialGainSlopeTable[0];
        pRegCmd->ASFRNRGainSlopeLUTEntry1.bitfields.GAIN_SLOPE              = pData->radialGainSlopeTable[1];
        pRegCmd->ASFRNRGainSlopeLUTEntry2.bitfields.GAIN_SLOPE              = pData->radialGainSlopeTable[2];

        pRegCmd->ASFRNRGainCFShiftLUTEntry0.bitfields.GAIN_CF_SHIFT         = pData->radialGainCFShift[0];
        pRegCmd->ASFRNRGainCFShiftLUTEntry1.bitfields.GAIN_CF_SHIFT         = pData->radialGainCFShift[1];
        pRegCmd->ASFRNRGainCFShiftLUTEntry2.bitfields.GAIN_CF_SHIFT         = pData->radialGainCFShift[2];

        pRegCmd->ASFThresholdConfig0.bitfields.FLAT_TH                      = pData->flatThreshold;
        pRegCmd->ASFThresholdConfig0.bitfields.TEXTURE_TH                   = pData->maxSmoothingClamp;
        pRegCmd->ASFThresholdConfig1.bitfields.SIMILARITY_TH                = pData->cornerThreshold;
        pRegCmd->ASFThresholdConfig1.bitfields.SMOOTH_STRENGTH              = pData->smoothingStrength;

        pRegCmd->ASFSkinConfig0.bitfields.H_MAX                             = pData->maxHue;
        pRegCmd->ASFSkinConfig0.bitfields.H_MIN                             = pData->minHue;
        pRegCmd->ASFSkinConfig0.bitfields.Y_MIN                             = pData->minY;
        pRegCmd->ASFSkinConfig1.bitfields.Y_MAX                             = pData->maxY;
        pRegCmd->ASFSkinConfig1.bitfields.BOUNDARY_PROB                     = pData->boundaryProbability;
        pRegCmd->ASFSkinConfig1.bitfields.Q_NON_SKIN                        = pData->nonskinQ;
        pRegCmd->ASFSkinConfig1.bitfields.Q_SKIN                            = pData->skinQ;
        pRegCmd->ASFSkinConfig2.bitfields.SHY_MAX                           = pData->maxShY;
        pRegCmd->ASFSkinConfig2.bitfields.SHY_MIN                           = pData->minShY;
        pRegCmd->ASFSkinConfig2.bitfields.SMAX_PARA                         = pData->paraSmax;
        pRegCmd->ASFSkinConfig2.bitfields.SMIN_PARA                         = pData->paraSmin;

        pRegCmd->ASFFaceConfig.bitfields.FACE_EN                            = pData->faceEnable;
        pRegCmd->ASFFaceConfig.bitfields.FACE_NUM                           = pData->faceNum;

        pRegCmd->ASFFace1CenterConfig.bitfields.FACE_CENTER_HORIZONTAL      = pData->faceCenterHorizontal[0];
        pRegCmd->ASFFace1CenterConfig.bitfields.FACE_CENTER_VERTICAL        = pData->faceCenterVertical[0];
        pRegCmd->ASFFace1RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY        = pData->faceRadiusBoundary[0];
        pRegCmd->ASFFace1RadiusConfig.bitfields.FACE_RADIUS_SHIFT           = pData->faceRadiusShift[0];
        pRegCmd->ASFFace1RadiusConfig.bitfields.FACE_RADIUS_SLOPE           = pData->faceRadiusSlope[0];
        pRegCmd->ASFFace1RadiusConfig.bitfields.FACE_SLOPE_SHIFT            = pData->faceSlopeShift[0];

        pRegCmd->ASFFace2CenterConfig.bitfields.FACE_CENTER_HORIZONTAL      = pData->faceCenterHorizontal[1];
        pRegCmd->ASFFace2CenterConfig.bitfields.FACE_CENTER_VERTICAL        = pData->faceCenterVertical[1];
        pRegCmd->ASFFace2RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY        = pData->faceRadiusBoundary[1];
        pRegCmd->ASFFace2RadiusConfig.bitfields.FACE_RADIUS_SHIFT           = pData->faceRadiusShift[1];
        pRegCmd->ASFFace2RadiusConfig.bitfields.FACE_RADIUS_SLOPE           = pData->faceRadiusSlope[1];
        pRegCmd->ASFFace2RadiusConfig.bitfields.FACE_SLOPE_SHIFT            = pData->faceSlopeShift[1];

        pRegCmd->ASFFace3CenterConfig.bitfields.FACE_CENTER_HORIZONTAL      = pData->faceCenterHorizontal[2];
        pRegCmd->ASFFace3CenterConfig.bitfields.FACE_CENTER_VERTICAL        = pData->faceCenterVertical[2];
        pRegCmd->ASFFace3RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY        = pData->faceRadiusBoundary[2];
        pRegCmd->ASFFace3RadiusConfig.bitfields.FACE_RADIUS_SHIFT           = pData->faceRadiusShift[2];
        pRegCmd->ASFFace3RadiusConfig.bitfields.FACE_RADIUS_SLOPE           = pData->faceRadiusSlope[2];
        pRegCmd->ASFFace3RadiusConfig.bitfields.FACE_SLOPE_SHIFT            = pData->faceSlopeShift[2];

        pRegCmd->ASFFace4CenterConfig.bitfields.FACE_CENTER_HORIZONTAL      = pData->faceCenterHorizontal[3];
        pRegCmd->ASFFace4CenterConfig.bitfields.FACE_CENTER_VERTICAL        = pData->faceCenterVertical[3];
        pRegCmd->ASFFace4RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY        = pData->faceRadiusBoundary[3];
        pRegCmd->ASFFace4RadiusConfig.bitfields.FACE_RADIUS_SHIFT           = pData->faceRadiusShift[3];
        pRegCmd->ASFFace4RadiusConfig.bitfields.FACE_RADIUS_SLOPE           = pData->faceRadiusSlope[3];
        pRegCmd->ASFFace4RadiusConfig.bitfields.FACE_SLOPE_SHIFT            = pData->faceSlopeShift[3];

        pRegCmd->ASFFace5CenterConfig.bitfields.FACE_CENTER_HORIZONTAL      = pData->faceCenterHorizontal[4];
        pRegCmd->ASFFace5CenterConfig.bitfields.FACE_CENTER_VERTICAL        = pData->faceCenterVertical[4];
        pRegCmd->ASFFace5RadiusConfig.bitfields.FACE_RADIUS_BOUNDARY        = pData->faceRadiusBoundary[4];
        pRegCmd->ASFFace5RadiusConfig.bitfields.FACE_RADIUS_SHIFT           = pData->faceRadiusShift[4];
        pRegCmd->ASFFace5RadiusConfig.bitfields.FACE_RADIUS_SLOPE           = pData->faceRadiusSlope[4];
        pRegCmd->ASFFace5RadiusConfig.bitfields.FACE_SLOPE_SHIFT            = pData->faceSlopeShift[4];

        {
            AsfParameters*  pASFParameters     = pOutput->pAsfParameters;
            ASF_MODULE_CFG* pASFModuleConfig   = &(pASFParameters->moduleCfg);

            pASFModuleConfig->EN               = pData->enable;
            pASFModuleConfig->SP_EFF_EN        = pData->specialEffectEnable;
            pASFModuleConfig->SP_EFF_ABS_EN    = pData->specialEffectAbsoluteEnable;
            pASFModuleConfig->LAYER_1_EN       = pData->layer1Enable;
            pASFModuleConfig->LAYER_2_EN       = pData->layer2Enable;
            pASFModuleConfig->CONTRAST_EN      = pData->contrastEnable;
            pASFModuleConfig->EDGE_ALIGN_EN    = pData->edgeAlignmentEnable;
            pASFModuleConfig->RNR_ENABLE       = pData->radialEnable;
            pASFModuleConfig->NEG_ABS_Y1       = pData->negateAbsoluteY1;
            pASFModuleConfig->SP               = pData->smoothPercentage;
            pASFModuleConfig->CHROMA_GRAD_EN   = pData->chromaGradientEnable;
            pASFModuleConfig->SKIN_EN          = pData->skinEnable;

            pASFParameters->faceVerticalOffset = pData->faceVerticalOffset;
        }

        // Fill up DMI Buffers
        UINT32*  pDMIDataPtr = pOutput->pDMIDataPtr;
        UINT32   data;
        UINT     pos = 0;
        UINT     i;

        for (i = 0; i < DMI_GAINPOSNEG_SIZE; i++)
        {
            data               = static_cast<UINT32>(pData->layer1SoftThresholdLut[i])                  & 0xFF;
            data              |= ((static_cast<UINT32>(pData->layer1GainNegativeLut[i]) << 8)           & 0xFF00);
            data              |= ((static_cast<UINT32>(pData->layer1GainPositiveLut[i]) << 16)          & 0xFF0000);
            data              |= ((static_cast<UINT32>(pData->layer1ActivityNormalizationLut[i]) << 24) & 0xFF000000);
            pDMIDataPtr[pos++] = data;
        }

        for (i = 0; i < DMI_GAINWEIGHT_SIZE; i++)
        {
            data               = static_cast<UINT32>(pData->layer1GainWeightLut[i]) & 0xFF;
            pDMIDataPtr[pos++] = data;
        }

        for (i = 0; i < DMI_SOFT_SIZE; i++)
        {
            data               = static_cast<UINT32>(pData->layer1SoftThresholdWeightLut[i]) & 0xFF;
            pDMIDataPtr[pos++] = data;
        }

        for (i = 0; i < DMI_GAINPOSNEG_SIZE; i++)
        {
            data               = static_cast<UINT32>(pData->layer2SoftThresholdLut[i])                  & 0xFF;
            data              |= ((static_cast<UINT32>(pData->layer2GainNegativeLut[i]) << 8 )          & 0xFF00);
            data              |= ((static_cast<UINT32>(pData->layer2GainPositiveLut[i]) << 16)          & 0xFF0000);
            data              |= ((static_cast<UINT32>(pData->layer2ActivityNormalizationLut[i]) << 24) & 0xFF000000);
            pDMIDataPtr[pos++] = data;
        }

        for (i = 0; i < DMI_GAINWEIGHT_SIZE; i++)
        {
            data               = static_cast<UINT32>(pData->layer2GainWeightLut[i]) & 0xFF;
            pDMIDataPtr[pos++] = data;
        }

        for (i = 0; i < DMI_SOFT_SIZE; i++)
        {
            data               = static_cast<UINT32>(pData->layer2SoftThresholdWeightLut[i]) & 0xFF;
            pDMIDataPtr[pos++] = data;
        }

        for (i = 0; i < DMI_GAINCHROMA_SIZE; i++)
        {
            data               = static_cast<UINT32>(pData->chromaGradientNegativeLut[i])          & 0xFF;
            data              |= ((static_cast<UINT32>(pData->chromaGradientPositiveLut[i]) << 8 ) & 0xFF00);
            pDMIDataPtr[pos++] = data;
        }

        for (i = 0; i < DMI_GAINCONTRAST_SIZE; i++)
        {
            data               = static_cast<UINT32>(pData->gainContrastNegativeLut[i])          & 0xFF;
            data              |= ((static_cast<UINT32>(pData->gainContrastPositiveLut[i]) << 8 ) & 0xFF00);
            pDMIDataPtr[pos++] = data;
        }

        for (i = 0; i < DMI_SKIN_SIZE; i++)
        {
            data               = static_cast<UINT32>(pData->skinGainLut[i])             & 0xFF;
            data              |= ((static_cast<UINT32>(pData->skinActivityLut[i]) << 8) & 0xFF00);
            pDMIDataPtr[pos++] = data;
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL ");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IPEASF30CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPEASF30CalculateSetting(
    const ASF30InputData* pInput,
    VOID*                 pOEMIQData,
    ASF30OutputData*      pOutput)
{
    CamxResult                                            result             = CamxResultSuccess;
    BOOL                                                  commonIQResult     = FALSE;
    asf_3_0_0::asf30_rgn_dataType*                        pInterpolationData = NULL;
    asf_3_0_0::chromatix_asf30_reserveType*               pReserveType       = NULL;
    asf_3_0_0::chromatix_asf30Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<asf_3_0_0::asf30_rgn_dataType*>(pInput->pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_asf30_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            commonIQResult     = IQInterface::s_interpolationTable.ASF30Interpolation(pInput, pInterpolationData);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<asf_3_0_0::asf30_rgn_dataType*>(&(pOEMInput->ASFSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveType   = reinterpret_cast<asf_3_0_0::chromatix_asf30_reserveType*>(
                                     &(pOEMInput->ASFReserveType));
                pModuleEnable  = reinterpret_cast<asf_3_0_0::chromatix_asf30Type::enable_sectionStruct*>(
                                     &(pOEMInput->ASFEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            ASF30UnpackedField unpackedRegisterData = { 0 };

            if (NULL == pOEMIQData)
            {
                // @todo (CAMX-2933) IQ vendor tags settings testing:
                //
                // Seems like the following control flags are sp_read across multiple places,
                // perhaps due to older datastructures. Hence, populating the reserved field
                // contents from Input field.
                //
                pReserveType->edge_alignment_enable = pInput->edgeAlignEnable;
                pReserveType->layer_1_enable        = pInput->layer1Enable;
                pReserveType->layer_2_enable        = pInput->layer2Enable;
                pReserveType->radial_enable         = pInput->radialEnable;
                pReserveType->contrast_enable       = pInput->contrastEnable;
            }

            commonIQResult = IQInterface::s_interpolationTable.ASF30CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pReserveType,
                                 pModuleEnable,
                                 static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                IPESetupASF30Register(pInput, &unpackedRegisterData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("IPEASF30 calculate hardwware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("ASF30 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL %p %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SetupTDL10Register
///
/// @brief  Setup TDL10 Register based on common library output
///
/// @param  pData   Pointer to the Common Library Calculation Result
/// @param  pRegCmd Pointer to the TDL10 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID SetupTDL10Register(
    const TDL10UnpackedField* pData,
    TDL10OutputData*          pOutput)
{
    IPER2DLUTRegCmd* pRegCmd = NULL;
    UINT hueCount            = 0;
    UINT satCount            = 0;
    UINT index               = 0;

    if ((NULL != pData) && (NULL != pOutput->pRegCmd))
    {
        pRegCmd = pOutput->pRegCmd;

        pRegCmd->blendFactor.bitfields.Y_BLEND_FACTOR_INTEGER = pData->y_blend_factor_integer;
        pRegCmd->endA.bitfields.L_BOUNDARY_END_A              = pData->l_boundary_end_A;
        pRegCmd->endB.bitfields.L_BOUNDARY_END_B              = pData->l_boundary_end_B;
        pRegCmd->endInv.bitfields.L_BOUNDARY_END_INV          = pData->l_boundary_end_inv;
        pRegCmd->hueShift.bitfields.H_SHIFT                   = pData->h_shift;
        pRegCmd->kbIntegeter.bitfields.K_B_INTEGER            = pData->k_b_integer;
        pRegCmd->krIntegeter.bitfields.K_R_INTEGER            = pData->k_r_integer;
        pRegCmd->satShift.bitfields.S_SHIFT                   = pData->s_shift;
        pRegCmd->startA.bitfields.L_BOUNDARY_START_A          = pData->l_boundary_start_A;
        pRegCmd->startB.bitfields.L_BOUNDARY_START_B          = pData->l_boundary_start_B;
        pRegCmd->startInv.bitfields.L_BOUNDARY_START_INV      = pData->l_boundary_start_inv;

        UINT index0 = 0;
        UINT index1 = 0;
        UINT index2 = 0;
        UINT index3 = 0;
        for (hueCount = 0; hueCount < H_GRID; hueCount++)
        {
            for (satCount = 0; satCount < S_GRID; satCount++)
            {
                if (TRUE == (Utils::IsUINT32Odd(hueCount)))
                {
                    if (TRUE == (Utils::IsUINT32Odd(satCount)))
                    {
                        // (odd, odd)
                        pOutput->pD2H3LUT[index3] = pData->hs_lut.lut_2d_h[index];
                        pOutput->pD2S3LUT[index3] = pData->hs_lut.lut_2d_s[index];
                        index3++;
                        index++;
                    }
                    else
                    {
                        // (odd, even)
                        pOutput->pD2H2LUT[index2] = pData->hs_lut.lut_2d_h[index];
                        pOutput->pD2S2LUT[index2] = pData->hs_lut.lut_2d_s[index];
                        index2++;
                        index++;
                    }
                }
                else
                {
                    if (TRUE == (Utils::IsUINT32Odd(satCount)))
                    {
                        // even, odd
                        pOutput->pD2H1LUT[index1] = pData->hs_lut.lut_2d_h[index];
                        pOutput->pD2S1LUT[index1] = pData->hs_lut.lut_2d_s[index];
                        index1++;
                        index++;
                    }
                    else
                    {
                        pOutput->pD2H0LUT[index0] = pData->hs_lut.lut_2d_h[index];
                        pOutput->pD2S0LUT[index0] = pData->hs_lut.lut_2d_s[index];
                        index0++;
                        index++;
                    }
                }
            }
        }

        Utils::Memcpy(pOutput->pD1HLUT, pData->hs_lut.lut_1d_h, sizeof(UINT32) * H_GRID);
        Utils::Memcpy(pOutput->pD1SLUT, pData->hs_lut.lut_1d_s, sizeof(UINT32) * S_GRID);
        Utils::Memcpy(pOutput->pD1IHLUT, pData->hs_lut.lut_1d_h_inv, sizeof(UINT32) * (H_GRID-1));
        Utils::Memcpy(pOutput->pD1ISLUT, pData->hs_lut.lut_1d_s_inv, sizeof(UINT32) * (S_GRID - 1));

    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("data is NULL %p %p", pData, pRegCmd);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::TDL10CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::TDL10CalculateSetting(
    const TDL10InputData* pInput,
    VOID*                 pOEMIQData,
    TDL10OutputData*      pOutput)
{
    CamxResult                                            result             = CamxResultSuccess;
    BOOL                                                  commonIQResult     = FALSE;
    tdl_1_0_0::tdl10_rgn_dataType*                        pInterpolationData = NULL;
    tdl_1_0_0::chromatix_tdl10_reserveType*               pReserveType       = NULL;
    tdl_1_0_0::chromatix_tdl10Type::enable_sectionStruct* pModuleEnable      = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<tdl_1_0_0::tdl10_rgn_dataType*>(pInput->pInterpolationData);
            pReserveType       = &(pInput->pChromatix->chromatix_tdl10_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            commonIQResult = IQInterface::s_interpolationTable.IPETDL10Interpolation(pInput, pInterpolationData);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);
            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<tdl_1_0_0::tdl10_rgn_dataType*>(&(pOEMInput->TDL10Setting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveType   = reinterpret_cast<tdl_1_0_0::chromatix_tdl10_reserveType*>(
                                     &(pOEMInput->TDL10ReserveType));
                pModuleEnable  = reinterpret_cast<tdl_1_0_0::chromatix_tdl10Type::enable_sectionStruct*>(
                                     &(pOEMInput->TDL10EnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            TDL10UnpackedField unpackedRegData;

            commonIQResult =
                IQInterface::s_interpolationTable.IPETDL10CalculateHWSetting(pInput,
                                                                             pInterpolationData,
                                                                             pReserveType,
                                                                             pModuleEnable,
                                                                             &unpackedRegData);

            if (TRUE == commonIQResult)
            {
                SetupTDL10Register(&unpackedRegData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("TDL10 calculate hardware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("TDL10 interpolation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL %p %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupCV12Register
///
/// @brief  Setup CV12 Register based on common library output
///
/// @param  pInput     Pointer to the Common Library Calculation Input
/// @param  pData      Pointer to the Common Library Calculation Result
/// @param  pRegCmd    Pointer to the CV12 Register Set
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupCV12Register(
    const CV12InputData*        pInput,
    CV12UnpackedField*          pData,
    IPEChromaEnhancementRegCmd* pRegCmd)
{
    if ((NULL != pData) && (NULL != pRegCmd) && (NULL != pInput))
    {
        pRegCmd->lumaConfig0.bitfields.V0 = static_cast<INT16>(pData->rToY);
        pRegCmd->lumaConfig0.bitfields.V1 = static_cast<INT16>(pData->gToY);
        pRegCmd->lumaConfig1.bitfields.V2 = static_cast<INT16>(pData->bToY);
        pRegCmd->lumaConfig1.bitfields.K  = static_cast<INT16>(pData->yOffset);
        pRegCmd->aConfig.bitfields.AM     = static_cast<INT16>(pData->am);
        pRegCmd->aConfig.bitfields.AP     = static_cast<INT16>(pData->ap);
        pRegCmd->bConfig.bitfields.BM     = static_cast<INT16>(pData->bm);
        pRegCmd->bConfig.bitfields.BP     = static_cast<INT16>(pData->bp);
        pRegCmd->cConfig.bitfields.CM     = static_cast<INT16>(pData->cm);
        pRegCmd->cConfig.bitfields.CP     = static_cast<INT16>(pData->cp);
        pRegCmd->dConfig.bitfields.DM     = static_cast<INT16>(pData->dm);
        pRegCmd->dConfig.bitfields.DP     = static_cast<INT16>(pData->dp);
        pRegCmd->cbConfig.bitfields.KCB   = static_cast<INT16>(pData->kcb);
        pRegCmd->crConfig.bitfields.KCR   = static_cast<INT16>(pData->kcr);
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL. pInput=%p, pData=%p, pRegCmd=%p", pInput, pData, pRegCmd);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IQInterface::IPECV12CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPECV12CalculateSetting(
    const CV12InputData* pInput,
    VOID*                pOEMIQData,
    CV12OutputData*      pOutput)
{
    CamxResult                                          result                = CamxResultSuccess;
    BOOL                                                commonIQResult        = FALSE;
    cv_1_2_0::cv12_rgn_dataType*                        pInterpolationData    = NULL;
    cv_1_2_0::chromatix_cv12Type::enable_sectionStruct* pModuleEnable         = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<cv_1_2_0::cv12_rgn_dataType*>(pInput->pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            // Call the Interpolation Calculation
            commonIQResult = IQInterface::s_interpolationTable.CV12Interpolation(pInput, pInterpolationData);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<cv_1_2_0::cv12_rgn_dataType*>(&(pOEMInput->ChromaEnhancementSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pModuleEnable  = reinterpret_cast<cv_1_2_0::chromatix_cv12Type::enable_sectionStruct*>(
                                     &(pOEMInput->ChromaEnhancementEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            CV12UnpackedField unpackedRegisterData = { 0 };
            commonIQResult = IQInterface::s_interpolationTable.CV12CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pModuleEnable,
                                 static_cast<VOID*>(&unpackedRegisterData));


            if (TRUE == commonIQResult)
            {
                IPESetupCV12Register(pInput, &unpackedRegisterData, pOutput->pRegCmd);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("IPECV12 calculate hardwware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("CV12 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Data is NULL. pInput=%p, pOutput=%p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupCAC22Register
///
/// @brief  Setup CAC22 Register based on common library output
///
/// @param  pData    Pointer to the unpacked CAC22 data
/// @param  pOutput  Pointer to the CAC22 output data structure
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupCAC22Register(
    CAC22UnpackedField* pData,
    CAC22OutputData*    pOutput)
{
    if ((NULL != pData) && (NULL != pOutput))
    {
        IPECACRegCmd* pRegCmd = pOutput->pRegCmd;
        if (NULL != pRegCmd)
        {
            pRegCmd->lumaThreshold.bitfields.Y_SPOT_THR         = pData->ySpotThreshold;
            pRegCmd->lumaThreshold.bitfields.Y_SATURATION_THR   = pData->ySaturationThreshold;
            pRegCmd->chromaThreshold.bitfields.C_SPOT_THR       = pData->cSpotThreshold;
            pRegCmd->chromaThreshold.bitfields.C_SATURATION_THR = pData->cSaturationThreshold;
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("pRegCmd is NULL");
        }
        pOutput->enableCAC2 = pData->enableCAC2;
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("CACdata is NULL pData = %p, pOutput = %p", pData, pOutput);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IQInterface::IPECAC22CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPECAC22CalculateSetting(
    const CAC22InputData* pInput,
    VOID*                 pOEMIQData,
    CAC22OutputData*      pOutput)
{
    CamxResult                                            result               = CamxResultSuccess;
    BOOL                                                  commonIQResult       = FALSE;
    cac_2_2_0::cac22_rgn_dataType*                        pInterpolationData = NULL;
    cac_2_2_0::chromatix_cac22Type::enable_sectionStruct* pModuleEnable = NULL;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        if (NULL == pOEMIQData)
        {
            // Call the Interpolation Calculation
            pInterpolationData = static_cast<cac_2_2_0::cac22_rgn_dataType*>(pInput->pInterpolationData);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            commonIQResult = IQInterface::s_interpolationTable.CAC22Interpolation(pInput, pInterpolationData);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<cac_2_2_0::cac22_rgn_dataType*>(&(pOEMInput->CACSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pModuleEnable  = reinterpret_cast<cac_2_2_0::chromatix_cac22Type::enable_sectionStruct*>(
                                     &(pOEMInput->CACEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            CAC22UnpackedField unpackedRegisterData = { 0 };

            commonIQResult =
                IQInterface::s_interpolationTable.CAC22CalculateHWSetting(
                    pInput,
                    pInterpolationData,
                    pModuleEnable,
                    static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                IPESetupCAC22Register(&unpackedRegisterData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("CAC2 calculate hardwware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("CAC22 intepolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL pInput = %p, pOutput = %p", pInput, pOutput);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IQInterface::IPETF10CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPETF10CalculateSetting(
    const TF10InputData* pInput,
    VOID*                pOEMIQData,
    TF10OutputData*      pOutput)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        BOOL                                                commonIQResult     = FALSE;
        tf_1_0_0::chromatix_tf10_reserveType*               pReserveData       = NULL;
        tf_1_0_0::mod_tf10_cct_dataType::cct_dataStruct*    pInterpolationData = NULL;
        tf_1_0_0::chromatix_tf10Type::enable_sectionStruct* pModuleEnable      = NULL;

        if (NULL == pOEMIQData)
        {
            pInterpolationData =
                static_cast<tf_1_0_0::mod_tf10_cct_dataType::cct_dataStruct*>(pInput->pInterpolationData);

            pReserveData       = &(pInput->pChromatix->chromatix_tf10_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            // Call the Interpolation Calculation
            commonIQResult = IQInterface::s_interpolationTable.TF10Interpolation(pInput, pInterpolationData);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<tf_1_0_0::mod_tf10_cct_dataType::cct_dataStruct*>(
                                     &(pOEMInput->TF10Setting.cct_data));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveData   = reinterpret_cast<tf_1_0_0::chromatix_tf10_reserveType*>(
                &(pOEMInput->TF10ReserveType));
                pModuleEnable  = reinterpret_cast<tf_1_0_0::chromatix_tf10Type::enable_sectionStruct*>(
                                     &(pOEMInput->TF10EnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {

            commonIQResult = IQInterface::s_interpolationTable.TF10CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pReserveData,
                                 pModuleEnable,
                                 pOutput->pRegCmd);

            if (TRUE != commonIQResult)
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("TF10 HW Calculation failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("TF10 intepolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL pInput = %p, pOutput = %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IQInterface::IPETF10GetInitializationData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPETF10GetInitializationData(
    struct TFNcLibOutputData* pData)
{
    CamxResult result         = CamxResultSuccess;
    BOOL       commonIQResult = FALSE;

    commonIQResult = IQInterface::s_interpolationTable.GetTF10InitializationData(pData);
    if (FALSE == commonIQResult)
    {
        result = CamxResultEFailed;
        CAMX_ASSERT_ALWAYS_MESSAGE("Get TF initialization datafailed.");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupICA10InterpolationParams
///
/// @brief  Setup ICA10 Register based on common library output
///
/// @param  pData                 Pointer to the unpacked ICA interpolation input data
/// @param  pReserveData          Pointer to reserve data structure
/// @param  pinterpolationData    Pointer to region data
///
/// @return BOOL TRUE if success
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static BOOL IPESetupICA10InterpolationParams(
    VOID*                                    pData,
    ica_1_0_0::chromatix_ica10_reserveType*  pReserveData,
    ica_1_0_0::ica10_rgn_dataType*           pinterpolationData)
{
    IPEICAInterpolationParams*  pInterpolationParams     = static_cast<IPEICAInterpolationParams*>(pData);
    pReserveData->opg_invalid_output_treatment_cb        = pInterpolationParams->outOfFramePixelFillConsCb;
    pReserveData->opg_invalid_output_treatment_cr        = pInterpolationParams->outOfFramePixelFillConsCr;
    pReserveData->opg_invalid_output_treatment_y         = pInterpolationParams->outOfFramePixelFillConstY;
    pReserveData->opg_invalid_output_treatment_calculate = pInterpolationParams->outOfFramePixelFillMode;

    for (UINT i = 0; i < ICAInterpolationCoeffSets; i++)
    {
        pinterpolationData->opg_interpolation_lut_0_tab.opg_interpolation_lut_0[i] =
            pInterpolationParams->customInterpolationCoefficients0[i];
        pinterpolationData->opg_interpolation_lut_1_tab.opg_interpolation_lut_1[i] =
            pInterpolationParams->customInterpolationCoefficients1[i];
        pinterpolationData->opg_interpolation_lut_2_tab.opg_interpolation_lut_2[i] =
            pInterpolationParams->customInterpolationCoefficients2[i];
    }
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPEDumpICA10Register
///
/// @brief  Dump ICA10 Register based on common library output
///
/// @param  pInput   Pointer to Input data
/// @param  pData    Pointer to the unpacked ICA data
/// @param  pOutput  Pointer to the ICA output data structure
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPEDumpICA10Register(
    const ICAInputData*  pInput,
    ICAUnpackedField*    pData,
    ICAOutputData*       pOutput)
{
    UINT32*  pPerspectivetransform =
        reinterpret_cast<UINT32*>(pOutput->pLUT[ICA10IndexPerspective]);
    UINT64*  pGrid0                =
        reinterpret_cast<UINT64*>(pOutput->pLUT[ICA10IndexGrid0]);
    UINT64*  pGrid1                =
        reinterpret_cast<UINT64*>(pOutput->pLUT[ICA10IndexGrid1]);
    ICA_REG_v30* pRegData              =
        static_cast<ICA_REG_v30*>(pData->pRegData);
    IcaParameters*    pIcaParams   =
        static_cast<IcaParameters*>(pData->pIcaParameter);

    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    UINT  j     = 0;
    UINT  k     = 0;

    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename),
                            "%s/ica10grid_Out_%d_path_%d.txt", ConfigFileDirectory,
                            pInput->frameNum, pInput->IPEPath);

    pFile = CamX::OsUtils::FOpen(dumpFilename, "w");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "Perspective Enable %d\n", pIcaParams->isPerspectiveEnable);
        CamX::OsUtils::FPrintF(pFile, "Num of rows %d\n", pIcaParams->perspGeomN);
        CamX::OsUtils::FPrintF(pFile, "Num of columns %d\n", pIcaParams->perspGeomM);
        for (UINT i = 0; i < ICAPerspectiveSize; i++)
        {
            CamX::OsUtils::FPrintF(pFile, "transform 0x%x, perspectiveM 0x%x, perspectiveE 0x%x\n",
                pPerspectivetransform[i],
                pRegData->CTC_PERSPECTIVE_PARAMS_M[i],
                pRegData->CTC_PERSPECTIVE_PARAMS_E[i]);
        }
        CamX::OsUtils::FPrintF(pFile, "--------------------------------\n");
        CamX::OsUtils::FPrintF(pFile, " Grid Enable %d\n", pIcaParams->isGridEnable);

        for (UINT i = 0; i < ICA10GridRegSize; i++)
        {
            if (0 == (i % 2))
            {
                CamX::OsUtils::FPrintF(pFile, "i %d,j %d,gridX 0x%x,grid Y 0x%x,LUT 0x%llx\n",
                    i, j, pRegData->CTC_GRID_X[i], pRegData->CTC_GRID_Y[i], pGrid0[j]);
                j++;
            }
            else
            {
                CamX::OsUtils::FPrintF(pFile, "i %d,k %d,gridRegX 0x%x,RegY 0x%x,LUT 0x%llx\n",
                    i, k, pRegData->CTC_GRID_X[i], pRegData->CTC_GRID_Y[i], pGrid1[k]);
                k++;
            }
        }
        CamX::OsUtils::FPrintF(pFile, "--------------------------------\n");
        CamX::OsUtils::FClose(pFile);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupICA10Register
///
/// @brief  Setup ICA10 Register based on common library output
///
/// @param  pInput   Pointer to Input data
/// @param  pData    Pointer to the unpacked ICA data
/// @param  pOutput  Pointer to the ICA output data structure
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupICA10Register(
    const ICAInputData*  pInput,
    ICAUnpackedField*    pData,
    ICAOutputData*       pOutput)
{
    UINT32*           pPerspectivetransform =
        reinterpret_cast<UINT32*>(pOutput->pLUT[ICA10IndexPerspective]);
    UINT64*           pGrid0                =
        reinterpret_cast<UINT64*>(pOutput->pLUT[ICA10IndexGrid0]);
    UINT64*           pGrid1                =
        reinterpret_cast<UINT64*>(pOutput->pLUT[ICA10IndexGrid1]);
    ICA_REG_v30*      pRegData              =
        static_cast<ICA_REG_v30*>(pData->pRegData);
    IcaParameters*    pIcaParams            =
        static_cast<IcaParameters*>(pData->pIcaParameter);

    if (1 == pIcaParams->isPerspectiveEnable)
    {
        for (UINT i = 0; i < ICAPerspectiveSize; i++)
        {
            pPerspectivetransform[i] =
                (((pRegData->CTC_PERSPECTIVE_PARAMS_M[i] & 0xFFFF)) |
                (pRegData->CTC_PERSPECTIVE_PARAMS_E[i] & 0x3F) << 16);
        }
    }

    if (1 == pIcaParams->isGridEnable)
    {
        UINT  j = 0;
        UINT  k = 0;
        for (UINT i = 0; i < ICA10GridRegSize; i++)
        {
            if (0 == (i % 2))
            {
                pGrid0[j] = (((static_cast<UINT64>(pRegData->CTC_GRID_X[i])) << 17) |
                    (static_cast<UINT64>(pRegData->CTC_GRID_Y[i]) & 0x1FFFF)) & 0x3FFFFFFFF;
                j++;
            }
            else
            {
                pGrid1[k] = (((static_cast<UINT64>(pRegData->CTC_GRID_X[i])) << 17) |
                    (static_cast<UINT64>(pRegData->CTC_GRID_Y[i]) & 0x1FFFF)) & 0x3FFFFFFFF;
                k++;
            }
        }
    }

    if (TRUE == pInput->dumpICAOut)
    {
        IPEDumpICA10Register(pInput, pData, pOutput);
    }

    // Assign Output data that needs to be stored by software
    pOutput->pCurrICAInData      = pData->pCurrICAInData;
    pOutput->pPrevICAInData      = pData->pPrevICAInData;
    pOutput->pCurrWarpAssistData = pData->pCurrWarpAssistData;
    pOutput->pPrevWarpAssistData = pData->pPrevWarpAssistData;
    pOutput->pWarpGeometryData   = pData->pWarpGeometryData;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupICA20InterpolationParams
///
/// @brief  Setup ICA20 Register based on common library output
///
/// @param  pData                 Pointer to the unpacked ICA interpolation input data
/// @param  pReserveData          Pointer to reserve data structure
/// @param  pinterpolationData    Pointer to region data
///
/// @return BOOL TRUE if success
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static BOOL IPESetupICA20InterpolationParams(
    VOID*                                    pData,
    ica_2_0_0::chromatix_ica20_reserveType*  pReserveData,
    ica_2_0_0::ica20_rgn_dataType*           pinterpolationData)
{
    IPEICAInterpolationParams*  pInterpolationParams     = static_cast<IPEICAInterpolationParams*>(pData);
    pReserveData->opg_invalid_output_treatment_cb        = pInterpolationParams->outOfFramePixelFillConsCb;
    pReserveData->opg_invalid_output_treatment_cr        = pInterpolationParams->outOfFramePixelFillConsCr;
    pReserveData->opg_invalid_output_treatment_y         = pInterpolationParams->outOfFramePixelFillConstY;
    pReserveData->opg_invalid_output_treatment_calculate = pInterpolationParams->outOfFramePixelFillMode;

    for (UINT i = 0; i < ICAInterpolationCoeffSets; i++)
    {
        pinterpolationData->opg_interpolation_lut_0_tab.opg_interpolation_lut_0[i] =
            pInterpolationParams->customInterpolationCoefficients0[i];
        pinterpolationData->opg_interpolation_lut_1_tab.opg_interpolation_lut_1[i] =
            pInterpolationParams->customInterpolationCoefficients1[i];
        pinterpolationData->opg_interpolation_lut_2_tab.opg_interpolation_lut_2[i] =
            pInterpolationParams->customInterpolationCoefficients2[i];
    }
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPEDumpICA20Register
///
/// @brief  Dump ICA10 Register based on common library output
///
/// @param  pInput   Pointer to Input data
/// @param  pData    Pointer to the unpacked ICA data
/// @param  pOutput  Pointer to the ICA output data structure
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPEDumpICA20Register(
    const ICAInputData*  pInput,
    ICAUnpackedField*    pData,
    ICAOutputData*       pOutput)
{
    UINT32*           pPerspectivetransform =
        reinterpret_cast<UINT32*>(pOutput->pLUT[ICA20IndexPerspective]);
    UINT64*           pGrid0                =
        reinterpret_cast<UINT64*>(pOutput->pLUT[ICA20IndexGrid]);
    ICA_REG_v30*      pRegData              =
        static_cast<ICA_REG_v30*>(pData->pRegData);
    IcaParameters*    pIcaParams            =
        static_cast<IcaParameters*>(pData->pIcaParameter);

    CHAR  dumpFilename[256];
    FILE* pFile = NULL;

    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename),
        "%s/ica20grid_Out_%d_path_%d.txt", ConfigFileDirectory,
        pInput->frameNum, pInput->IPEPath);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "w");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "Perspective Enable %d\n", pIcaParams->isPerspectiveEnable);
        CamX::OsUtils::FPrintF(pFile, "Num of rows %d\n", pIcaParams->perspGeomN);
        CamX::OsUtils::FPrintF(pFile, "Num of columns %d\n", pIcaParams->perspGeomM);
        for (UINT i = 0; i < ICAPerspectiveSize; i++)
        {
            CamX::OsUtils::FPrintF(pFile, "perspectiveM 0x%x, perspectiveE 0x%x, transform 0x%x\n",
                pPerspectivetransform[i],
                pRegData->CTC_PERSPECTIVE_PARAMS_M[i],
                pRegData->CTC_PERSPECTIVE_PARAMS_E[i]);
        }
        CamX::OsUtils::FPrintF(pFile, "--------------------------------\n");
        CamX::OsUtils::FPrintF(pFile, " Grid Enable %d\n", pIcaParams->isGridEnable);

        for (UINT i = 0; i < ICA20GridRegSize; i++)
        {
            CamX::OsUtils::FPrintF(pFile, "i %d,gridX 0x%x,grid Y 0x%x,LUT 0x%llx\n",
                i, pRegData->CTC_GRID_X[i], pRegData->CTC_GRID_Y[i], pGrid0[i]);
        }
        CamX::OsUtils::FPrintF(pFile, "--------------------------------\n");
        CamX::OsUtils::FClose(pFile);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupICA20Register
///
/// @brief  Setup ICA20 Register based on common library output
///
/// @param  pInput   Pointer to Input data
/// @param  pData    Pointer to the unpacked ICA data
/// @param  pOutput  Pointer to the ICA output data structure
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupICA20Register(
    const ICAInputData*  pInput,
    ICAUnpackedField*    pData,
    ICAOutputData*       pOutput)
{
    UINT32*           pPerspectivetransform =
        reinterpret_cast<UINT32*>(pOutput->pLUT[ICA20IndexPerspective]);
    UINT64*           pGrid                 =
        reinterpret_cast<UINT64*>(pOutput->pLUT[ICA20IndexGrid]);
    ICA_REG_v30*      pRegData              =
        static_cast<ICA_REG_v30*>(pData->pRegData);
    IcaParameters*    pIcaParams            =
        static_cast<IcaParameters*>(pData->pIcaParameter);

    if (1 == pIcaParams->isPerspectiveEnable)
    {
        for (UINT i = 0; i < ICAPerspectiveSize; i++)
        {
            pPerspectivetransform[i] =
                (((pRegData->CTC_PERSPECTIVE_PARAMS_M[i] & 0xFFFF)) |
                (pRegData->CTC_PERSPECTIVE_PARAMS_E[i] & 0x3F) << 16);
        }
    }

    if (1 == pIcaParams->isGridEnable)
    {
        for (UINT i = 0; i < ICA20GridRegSize; i++)
        {
            pGrid[i] = (((static_cast<UINT64>(pRegData->CTC_GRID_X[i])) << 17) |
               (static_cast<UINT64>(pRegData->CTC_GRID_Y[i]) & 0x1FFFF)) & 0x3FFFFFFFF;
        }
    }

    if (TRUE == pInput->dumpICAOut)
    {
        IPEDumpICA20Register(pInput, pData, pOutput);
    }

    // Assign Output data that needs to be stored by software
    pOutput->pCurrICAInData      = pData->pCurrICAInData;
    pOutput->pPrevICAInData      = pData->pPrevICAInData;
    pOutput->pCurrWarpAssistData = pData->pCurrWarpAssistData;
    pOutput->pPrevWarpAssistData = pData->pPrevWarpAssistData;
    pOutput->pWarpGeometryData   = pData->pWarpGeometryData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::ICA10CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::ICA10CalculateSetting(
    const ICAInputData*  pInput,
    ICAOutputData*       pOutput)
{
    IPEICAInterpolationParams*               pInterpolationParams = NULL;
    CamxResult                               result               = CamxResultSuccess;
    BOOL                                     commonIQResult       = FALSE;
    ica_1_0_0::chromatix_ica10_reserveType*  pReserveData         = NULL;
    ica_1_0_0::chromatix_ica10_reserveType   reserveData;
    ica_1_0_0::ica10_rgn_dataType            interpolationData;
    ica_1_0_0::ica10_rgn_dataType*           pInterpolationData   = NULL;
    ICAUnpackedField                         unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        // Call the Interpolation Calculation
        pInterpolationParams = static_cast<IPEICAInterpolationParams*>(pInput->pInterpolationParamters);
        if (TRUE == pInterpolationParams->customInterpolationEnabled)
        {
            commonIQResult      = IPESetupICA10InterpolationParams(pInterpolationParams, &reserveData, &interpolationData);
            pReserveData        = &reserveData;
            pInterpolationData  = &interpolationData;
        }
        else
        {
            pInterpolationData  = static_cast<ica_1_0_0::ica10_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult      = IQInterface::s_interpolationTable.ICA10Interpolation(pInput, pInterpolationData);
            pReserveData        =
                &((static_cast <ica_1_0_0::chromatix_ica10Type*>(pInput->pChromatix))->chromatix_ica10_reserve);
        }

        // Fill Unpacked register data. // Update this if this needs to be ICA Registers.
        unpackedRegData.pIcaParameter = pOutput->pICAParameter;
        unpackedRegData.pRegData      = pInput->pNCRegData;

        if (TRUE == commonIQResult)
        {
            commonIQResult = IQInterface::s_interpolationTable.ICACalculateHWSetting(
                pInput, pInterpolationData, pReserveData, static_cast<VOID*>(&unpackedRegData));

            // not required if unpacked accepts this pointer. if not copy from registers to DMI buffer
            if (FALSE == commonIQResult)
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("ICA10 calculate hardware setting failed");
            }
            else
            {
                IPESetupICA10Register(pInput, &unpackedRegData, pOutput);
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("ICA10 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL pInput = %p, pOutput = %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::ICA20CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::ICA20CalculateSetting(
    const ICAInputData*  pInput,
    ICAOutputData*       pOutput)
{
    IPEICAInterpolationParams*               pInterpolationParams = NULL;
    CamxResult                               result               = CamxResultSuccess;
    BOOL                                     commonIQResult       = FALSE;
    ica_2_0_0::chromatix_ica20_reserveType*  pReserveData         = NULL;
    ica_2_0_0::chromatix_ica20_reserveType   reserveData;
    ica_2_0_0::ica20_rgn_dataType            interpolationData;
    ica_2_0_0::ica20_rgn_dataType*           pInterpolationData   = NULL;
    ICAUnpackedField                         unpackedRegData;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        // Call the Interpolation Calculation
        pInterpolationParams = static_cast<IPEICAInterpolationParams*>(pInput->pInterpolationParamters);
        if (TRUE == pInterpolationParams->customInterpolationEnabled)
        {
            commonIQResult      = IPESetupICA20InterpolationParams(pInterpolationParams, &reserveData, &interpolationData);
            pReserveData        = &reserveData;
            pInterpolationData  = &interpolationData;
        }
        else
        {
            pInterpolationData  = static_cast<ica_2_0_0::ica20_rgn_dataType*>(pInput->pInterpolationData);
            commonIQResult      = IQInterface::s_interpolationTable.ICA20Interpolation(pInput, pInterpolationData);
            pReserveData        =
                &((static_cast <ica_2_0_0::chromatix_ica20Type*>(pInput->pChromatix))->chromatix_ica20_reserve);
        }

        // Fill Unpacked register data.
        unpackedRegData.pIcaParameter = pOutput->pICAParameter;
        unpackedRegData.pRegData      = pInput->pNCRegData;

        if (TRUE == commonIQResult)
        {
            commonIQResult = IQInterface::s_interpolationTable.ICACalculateHWSetting(
                pInput, pInterpolationData, pReserveData, static_cast<VOID*>(&unpackedRegData));

            if (FALSE == commonIQResult)
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("IC20 calculate hardware setting failed");
            }
            else
            {
                IPESetupICA20Register(pInput, &unpackedRegData, pOutput);
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("ICA20 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL pInput = %p, pOutput = %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IQInterface::IPEICAGetInitializationData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPEICAGetInitializationData(
    struct ICANcLibOutputData* pData)
{
    CamxResult result         = CamxResultSuccess;
    BOOL       commonIQResult = FALSE;

    commonIQResult = IQInterface::s_interpolationTable.GetICAInitializationData(pData);
    if (FALSE == commonIQResult)
    {
        result = CamxResultEFailed;
        CAMX_ASSERT_ALWAYS_MESSAGE("Get ICA initialization datafailed.");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IQInterface::IPEANR10CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPEANR10CalculateSetting(
    const ANR10InputData* pInput,
    VOID*                 pOEMIQData,
    ANR10OutputData*      pOutput)
{
    CamxResult result = CamxResultSuccess;
    BOOL       commonIQResult = FALSE;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        anr_1_0_0::mod_anr10_cct_dataType::cct_dataStruct*    pInterpolationData = NULL;
        anr_1_0_0::chromatix_anr10Type::enable_sectionStruct* pModuleEnable      = NULL;
        anr_1_0_0::chromatix_anr10_reserveType*               pReserveData       = NULL;

        if (NULL == pOEMIQData)
        {
            pInterpolationData =
                static_cast<anr_1_0_0::mod_anr10_cct_dataType::cct_dataStruct*>(pInput->pInterpolationData);
            pReserveData       = &(pInput->pChromatix->chromatix_anr10_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            // Call the Interpolation Calculation
            commonIQResult = IQInterface::s_interpolationTable.ANR10Interpolation(pInput, pInterpolationData);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<anr_1_0_0::mod_anr10_cct_dataType::cct_dataStruct*>(
                                     &(pOEMInput->ANR10Setting.cct_data));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveData   = reinterpret_cast<anr_1_0_0::chromatix_anr10_reserveType*>(
                                     &(pOEMInput->ANR10ReserveType));
                pModuleEnable  = reinterpret_cast<anr_1_0_0::chromatix_anr10Type::enable_sectionStruct*>(
                                     &(pOEMInput->ANR10EnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            commonIQResult = IQInterface::s_interpolationTable.ANR10CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pReserveData,
                                 pModuleEnable,
                                 pOutput->pRegCmd);

            if (TRUE != commonIQResult)
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("ANR10 HW Calculation failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("ANR10 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL pInput = %p, pOutput = %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IQInterface::IPEANR10GetInitializationData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPEANR10GetInitializationData(
    struct ANRNcLibOutputData* pData)
{
    CamxResult result         = CamxResultSuccess;
    BOOL       commonIQResult = FALSE;

    commonIQResult = IQInterface::s_interpolationTable.GetANR10InitializationData(pData);
    if (FALSE == commonIQResult)
    {
        result = CamxResultEFailed;
        CAMX_ASSERT_ALWAYS_MESSAGE("Get ANR initialization datafailed.");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupLTM13Register
///
/// @brief  Setup LTM13 Register based on common library output
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupLTM13Register(
    const LTM13InputData* pInput,
    LTM13UnpackedField*   pData,
    LTM13OutputData*      pOutput)
{
    IPELTM13RegConfig*    pRegCmd       = NULL;
    IPELTM13ModuleConfig* pModuleConfig = NULL;
    UINT32                offset        = 0;

    if ((NULL != pInput) && (NULL != pData) && (NULL != pOutput))
    {
        pRegCmd       = pOutput->pRegCmd;
        pModuleConfig = pOutput->pModuleConfig;

        pRegCmd->LTMDataCollectionConfig0.bitfields.BIN_INIT_CNT           = pData->bin_init_cnt;
        pRegCmd->LTMRGB2YConfig.bitfields.C1                               = pData->c1;
        pRegCmd->LTMRGB2YConfig.bitfields.C2                               = pData->c2;
        pRegCmd->LTMRGB2YConfig.bitfields.C3                               = pData->c3;
        pRegCmd->LTMRGB2YConfig.bitfields.C4                               = pData->c4;
        pRegCmd->LTMImageProcessingConfig6.bitfields.LCE_THD               = pData->lce_thd;
        pRegCmd->LTMImageProcessingConfig6.bitfields.YRATIO_MAX            = pData->y_ratio_max;
        pRegCmd->LTMMaskFilterCoefficientConfig0.bitfields.D0              = pData->mask_filter_kernel[0];
        pRegCmd->LTMMaskFilterCoefficientConfig0.bitfields.D1              = pData->mask_filter_kernel[1];
        pRegCmd->LTMMaskFilterCoefficientConfig0.bitfields.D2              = pData->mask_filter_kernel[2];
        pRegCmd->LTMMaskFilterCoefficientConfig0.bitfields.D3              = pData->mask_filter_kernel[3];
        pRegCmd->LTMMaskFilterCoefficientConfig0.bitfields.D4              = pData->mask_filter_kernel[4];
        pRegCmd->LTMMaskFilterCoefficientConfig0.bitfields.D5              = pData->mask_filter_kernel[5];
        pRegCmd->LTMMaskFilterCoefficientConfig1.bitfields.SCALE           = pData->mask_filter_scale;
        pRegCmd->LTMMaskFilterCoefficientConfig1.bitfields.SHIFT           = pData->mask_filter_shift;
        pRegCmd->LTMDownScaleMNYConfig.bitfields.HORIZONTAL_TERMINATION_EN = pData->scale_in_cfg.early_termination_h;
        pRegCmd->LTMDownScaleMNYConfig.bitfields.VERTICAL_TERMINATION_EN   = pData->scale_in_cfg.early_termination_v;

        pModuleConfig->moduleConfig.bitfields.DC_AVG_BANK_SEL              = pData->dc_3dtable_avg_pong_sel;
        pModuleConfig->moduleConfig.bitfields.DC_EN                        = pData->data_collect_en;
        pModuleConfig->moduleConfig.bitfields.EN                           = pData->enable;
        pModuleConfig->moduleConfig.bitfields.IP_AVG_BANK_SEL              = pData->ip_3dtable_avg_pong_sel;
        pModuleConfig->moduleConfig.bitfields.IP_DEBUG_SEL                 = pData->debug_out_sel;
        pModuleConfig->moduleConfig.bitfields.IP_EN                        = pData->img_process_en;
        pModuleConfig->moduleConfig.bitfields.LA_EN                        = pData->la_en;
        pModuleConfig->moduleConfig.bitfields.MASK_EN                      = pData->mask_filter_en;
        pModuleConfig->moduleConfig.bitfields.MN_Y_EN                      = pData->scale_in_cfg.enable;
        pModuleConfig->moduleConfig.bitfields.RGAMMA_EN                    = pData->igamma_en;

        offset += IPELTMLUTNumEntries[LTMIndexWeight];
        offset += IPELTMLUTNumEntries[LTMIndexLA0];
        Utils::Memcpy((pOutput->pDMIDataPtr + offset),
                      pData->la_curve.pLUTTable,
                      IPELTMLUTNumEntries[LTMIndexLA1] * sizeof(INT32));
        offset += IPELTMLUTNumEntries[LTMIndexLA1];
        offset += IPELTMLUTNumEntries[LTMIndexCurve];
        offset += IPELTMLUTNumEntries[LTMIndexScale];
        offset += IPELTMLUTNumEntries[LTMIndexMask];
        offset += IPELTMLUTNumEntries[LTMIndexLCEPositive];
        offset += IPELTMLUTNumEntries[LTMIndexLCENegative];
        offset += IPELTMLUTNumEntries[LTMIndexRGamma0];

        Utils::Memcpy((pOutput->pDMIDataPtr + offset),
                      pData->igamma64.pLUTTable,
                      IPELTMLUTNumEntries[LTMIndexRGamma1] * sizeof(INT32));
        offset += IPELTMLUTNumEntries[LTMIndexRGamma1];

        Utils::Memcpy((pOutput->pDMIDataPtr + offset),
                      pData->igamma64.pLUTTable,
                      IPELTMLUTNumEntries[LTMIndexRGamma2] * sizeof(INT32));
        offset += IPELTMLUTNumEntries[LTMIndexRGamma2];

        Utils::Memcpy((pOutput->pDMIDataPtr + offset),
                      pData->igamma64.pLUTTable,
                      IPELTMLUTNumEntries[LTMIndexRGamma3] * sizeof(INT32));
        offset += IPELTMLUTNumEntries[LTMIndexRGamma3];

        Utils::Memcpy((pOutput->pDMIDataPtr + offset),
                      pData->igamma64.pLUTTable,
                      IPELTMLUTNumEntries[LTMIndexRGamma4] * sizeof(INT32));
        offset += IPELTMLUTNumEntries[LTMIndexRGamma4];
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid data is NULL pInput %p pData %p pOutput %p", pInput, pData, pOutput);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPELTM13CalculateVendorSetting
///
/// @brief  Calculate and apply LTM vendor settings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPELTM13CalculateVendorSetting(
    LTM13InputData* pInput,
    ltm_1_3_0::ltm13_rgn_dataType* pInterpolationData)
{
    if ((NULL != pInput) && (NULL != pInterpolationData))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
            "ltmDarkBoostStrength = %f "
            "ltmBrightSupressStrength = %f "
            "ltmLceStrength = %f "
            "ltm_old_dark_boost = %f "
            "ltm_old_bright_suppress = %f "
            "ltm_old_lce_strength = %f ",
            pInput->ltmDarkBoostStrength,
            pInput->ltmBrightSupressStrength,
            pInput->ltmLceStrength,
            pInterpolationData->dark_boost,
            pInterpolationData->bright_suppress,
            pInterpolationData->lce_strength);

        if (pInput->ltmDarkBoostStrength >= 0.0f)
        {
            pInterpolationData->dark_boost += (4.0f - pInterpolationData->dark_boost) * pInput->ltmDarkBoostStrength / 100.0f;
        }
        else
        {
            pInterpolationData->dark_boost *= (pInput->ltmDarkBoostStrength + 100.0f) / 100.0f;
        }

        if (pInput->ltmBrightSupressStrength >= 0.0f)
        {
            pInterpolationData->bright_suppress +=
                (4.0f - pInterpolationData->bright_suppress) * pInput->ltmBrightSupressStrength / 100.0f;
        }
        else
        {
            pInterpolationData->bright_suppress *= (pInput->ltmBrightSupressStrength + 100.0f) / 100.0f;
        }

        if (pInput->ltmLceStrength >= 0.0f)
        {
            pInterpolationData->lce_strength += (4.0f - pInterpolationData->lce_strength) * pInput->ltmLceStrength / 100.0f;
        }
        else
        {
            pInterpolationData->lce_strength *= (pInput->ltmLceStrength + 100.0f) / 100.0f;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupPProc,
            "ltm_new_dark_boost = %f "
            "ltm_new_bright_suppress = %f "
            "ltm_new_lce_strength = %f ",
            pInterpolationData->dark_boost,
            pInterpolationData->bright_suppress,
            pInterpolationData->lce_strength);
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid data is NULL pInput %p pInterpolationData %p", pInput, pInterpolationData);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IPELTM13CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPELTM13CalculateSetting(
    LTM13InputData*       pInput,
    VOID*                 pOEMIQData,
    LTM13OutputData*      pOutput,
    TMC10InputData*       pTMCInput)
{
    CamxResult                                            result             = CamxResultSuccess;
    BOOL                                                  commonIQResult     = FALSE;
    ltm_1_3_0::chromatix_ltm13Type::enable_sectionStruct* pModuleEnable      = NULL;
    ltm_1_3_0::chromatix_ltm13_reserveType*               pReserveData       = NULL;
    ltm_1_3_0::ltm13_rgn_dataType*                        pInterpolationData = NULL;

    if ((NULL != pInput)           &&
        (NULL != pOutput)          &&
        (NULL != pOEMIQData || NULL != pInput->pChromatix))
    {
        pInterpolationData = static_cast<ltm_1_3_0::ltm13_rgn_dataType*>(pInput->pInterpolationData);

        if (NULL == pOEMIQData)
        {
            pModuleEnable      = &(pInput->pChromatix->enable_section);
            pReserveData       = &(pInput->pChromatix->chromatix_ltm13_reserve);

            if (NULL != pInterpolationData)
            {
                // Call the Interpolation Calculation
                commonIQResult = IQInterface::s_interpolationTable.LTM13Interpolation(pInput, pInterpolationData);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid InterpolationData pointer");
            }

            // If ADRC is enable for LTM module, update ADRC input data.
            if ((TRUE == commonIQResult) &&
                (TRUE == pTMCInput->adrcLTMEnable))
            {
                if (SWTMCVersion::TMC10 == pTMCInput->pAdrcOutputData->version)
                {
                    commonIQResult = IQInterface::GetADRCData(pTMCInput);
                }
                pInput->pAdrcInputData = pTMCInput->pAdrcOutputData;
            }
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            pReserveData       = reinterpret_cast<ltm_1_3_0::chromatix_ltm13_reserveType*>(
                                     &(pOEMInput->LTMReserveType));

            if (NULL != pInterpolationData)
            {
                // Copy OEM defined interpolation data
                Utils::Memcpy(pInterpolationData, &(pOEMInput->LTMSetting), sizeof(ltm_1_3_0::ltm13_rgn_dataType));

                commonIQResult = TRUE;
                pModuleEnable  = reinterpret_cast<ltm_1_3_0::chromatix_ltm13Type::enable_sectionStruct*>(
                                     &(pOEMInput->LTMEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            // make the lut table in unpackedRegisterData point to DMI Table
            UINT32             offset               = 0;
            LTM13UnpackedField unpackedRegisterData = { 0 };

            unpackedRegisterData.wt.numEntries = LTM_WEIGHT_LUT_SIZE;
            unpackedRegisterData.wt.pLUTTable = reinterpret_cast<INT32*>(pOutput->pDMIDataPtr + offset);

            offset += IPELTMLUTNumEntries[LTMIndexWeight];
            unpackedRegisterData.la_curve.numEntries = LTM_CURVE_LUT_SIZE - 1;
            unpackedRegisterData.la_curve.pLUTTable = reinterpret_cast<INT32*>(pOutput->pDMIDataPtr + offset);

            offset += IPELTMLUTNumEntries[LTMIndexLA0] + IPELTMLUTNumEntries[LTMIndexLA1];
            unpackedRegisterData.ltm_curve.numEntries = LTM_CURVE_LUT_SIZE - 1;
            unpackedRegisterData.ltm_curve.pLUTTable = reinterpret_cast<INT32*>(pOutput->pDMIDataPtr + offset);

            offset += IPELTMLUTNumEntries[LTMIndexCurve];
            unpackedRegisterData.ltm_scale.numEntries = LTM_SCALE_LUT_SIZE - 1;
            unpackedRegisterData.ltm_scale.pLUTTable = reinterpret_cast<INT32*>(pOutput->pDMIDataPtr + offset);

            offset += IPELTMLUTNumEntries[LTMIndexScale];
            unpackedRegisterData.mask_rect_curve.numEntries = LTM_SCALE_LUT_SIZE - 1;
            unpackedRegisterData.mask_rect_curve.pLUTTable = reinterpret_cast<INT32*>(pOutput->pDMIDataPtr + offset);

            offset += IPELTMLUTNumEntries[LTMIndexMask];
            unpackedRegisterData.lce_scale_pos.numEntries = LTM_SCALE_LUT_SIZE - 1;
            unpackedRegisterData.lce_scale_pos.pLUTTable = reinterpret_cast<INT32*>(pOutput->pDMIDataPtr + offset);

            offset += IPELTMLUTNumEntries[LTMIndexLCEPositive];
            unpackedRegisterData.lce_scale_neg.numEntries = LTM_SCALE_LUT_SIZE - 1;
            unpackedRegisterData.lce_scale_neg.pLUTTable = reinterpret_cast<INT32*>(pOutput->pDMIDataPtr + offset);

            offset += IPELTMLUTNumEntries[LTMIndexLCENegative];
            unpackedRegisterData.igamma64.numEntries = LTM_GAMMA_LUT_SIZE - 1;
            unpackedRegisterData.igamma64.pLUTTable = reinterpret_cast<INT32*>(pOutput->pDMIDataPtr + offset);

            IPELTM13CalculateVendorSetting(pInput, pInterpolationData);

            commonIQResult = IQInterface::s_interpolationTable.LTM13CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pReserveData,
                                 pModuleEnable,
                                 static_cast<VOID*>(&unpackedRegisterData));

            if (TRUE == commonIQResult)
            {
                IPESetupLTM13Register(pInput, &unpackedRegisterData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("LTM13 calculate HW settings failed.");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("LTM13 intepolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL pInput %p pOutput %p", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CalculateUpscale20LUT
///
/// @brief  Calculate Upscale20 LUT tables
///
/// @param  offset       Offset to the LUT table
/// @param  ppTwoDFilter Pointer to the Common Library Calculation Result
/// @param  numRows      size of row for ppTwoDFilter
/// @param  numColumns   size of column for ppTwoDFilter
/// @param  sizeLUT      size of LUT in this calculation
/// @param  pLUT         Pointer to the LUT table
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID CalculateUpscale20LUT(
    UINT32* pOffset,
    UINT32* pTwoDFilter,
    UINT32  numRows,
    UINT32  numColumns,
    UINT32  sizeLUT,
    UINT32* pLUT)
{
    UINT32 i = 0;
    UINT32 j = 0;
    UINT32 k = 0;
    for (i = 0; i < sizeLUT; i++)
    {
        // twoDFilter[j][k];
        pLUT[i + *pOffset] = *((pTwoDFilter + (j * numColumns)) + k);
        j++;
        if (j >= numRows)
        {
            k++;
            j = 0;
        }
    }

    *pOffset += sizeLUT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupUpscale20Register
///
/// @brief  Setup Upscale20 Register based on common library output
///
/// @param  pUnpackedRegisterData    Pointer to the Common Library Calculation Result
/// @param  pOutput                  Pointer to the Upscale20 module Output
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupUpscale20Register(
    Upscale20UnpackedField* pUnpackedRegisterData,
    Upscale20OutputData*    pOutput)
{
    IPEUpscaleRegCmd* pRegCmdUpscale = pOutput->pRegCmdUpscale;
    UpScaleHwConfig*  pHwConfig      = &(pUnpackedRegisterData->upscaleHwConfig);
    UINT32*           pLUT           = pOutput->pDMIPtr;

    // op mode register
    pRegCmdUpscale->opMode.bitfields.RGN_SEL                           = pHwConfig->opMode.alphaEnable;
    pRegCmdUpscale->opMode.bitfields.DE_EN                             = pHwConfig->opMode.detailEnhanceEnable;
    pRegCmdUpscale->opMode.bitfields.BIT_WIDTH                         = pHwConfig->opMode.bitWidth;
    pRegCmdUpscale->opMode.bitfields.DIR_EN                            = pHwConfig->opMode.directionEnable;
    pRegCmdUpscale->opMode.bitfields.Y_CFG                             = pHwConfig->opMode.yConfig;
    pRegCmdUpscale->opMode.bitfields.UV_CFG                            = pHwConfig->opMode.uvConfig;
    pRegCmdUpscale->opMode.bitfields.BLEND_CFG                         = pHwConfig->opMode.blendConfig;
    pRegCmdUpscale->opMode.bitfields.EN                                = pHwConfig->opMode.enable;
    pRegCmdUpscale->opMode.bitfields.WEIGHT_GAIN                       = pHwConfig->opMode.weightGain;

    // preload register
    pRegCmdUpscale->preload.bitfields.UV_PRELOAD_H                     = pHwConfig->preload.horizontalPreloadUV;
    pRegCmdUpscale->preload.bitfields.UV_PRELOAD_V                     = pHwConfig->preload.verticalPreloadUV;
    pRegCmdUpscale->preload.bitfields.Y_PRELOAD_H                      = pHwConfig->preload.horizontalPreloadY;
    pRegCmdUpscale->preload.bitfields.Y_PRELOAD_V                      = pHwConfig->preload.verticalPreloadY;

    // Y channel horizontal / vertical phase step register
    pRegCmdUpscale->horizontalPhaseStepY.bitfields.Y_STEP_H            = pHwConfig->horizontalPhaseStepY;
    pRegCmdUpscale->verticalPhaseStepY.bitfields.Y_STEP_V              = pHwConfig->verticalPhaseStepY;

    // UV channel horizontal / vertical phase step register
    pRegCmdUpscale->horizontalPhaseStepUV.bitfields.UV_STEP_H          = pHwConfig->horizontalPhaseStepUV;
    pRegCmdUpscale->verticalPhaseStepUV.bitfields.UV_STEP_V            = pHwConfig->verticalPhaseStepUV;

    // detail enhancer sharpen level register
    pRegCmdUpscale->detailEnhancerSharpen.bitfields.SHARPEN_LEVEL1     = pHwConfig->sharpenStrength.sharpenLevel1;
    pRegCmdUpscale->detailEnhancerSharpen.bitfields.SHARPEN_LEVEL2     = pHwConfig->sharpenStrength.sharpenLevel2;

    // detail enhancer control register
    pRegCmdUpscale->detailEnhancerSharpenControl.bitfields.DE_CLIP     = pHwConfig->detailEnhanceControl.detailEnhanceClip;
    pRegCmdUpscale->detailEnhancerSharpenControl.bitfields.DE_PREC     = pHwConfig->detailEnhanceControl.detailEnhancePrecision;

    // detail enhancer shape control register
    pRegCmdUpscale->detailEnhancerShapeControl.bitfields.THR_DIEOUT    = pHwConfig->curveShape.thresholdDieout;
    pRegCmdUpscale->detailEnhancerShapeControl.bitfields.THR_QUIET     = pHwConfig->curveShape.thresholdQuiet;

    // detail enhancer threshold register
    pRegCmdUpscale->detailEnhancerThreshold.bitfields.THR_HIGH         = pHwConfig->detailEnhanceThreshold.thresholdHigh;
    pRegCmdUpscale->detailEnhancerThreshold.bitfields.THR_LOW          = pHwConfig->detailEnhanceThreshold.thresholdLow;

    // detail enhancer adjust data registers
    pRegCmdUpscale->detailEnhancerAdjustData0.bitfields.ADJUST_A0      = pHwConfig->curveAs.adjustA0;
    pRegCmdUpscale->detailEnhancerAdjustData0.bitfields.ADJUST_A1      = pHwConfig->curveAs.adjustA1;
    pRegCmdUpscale->detailEnhancerAdjustData0.bitfields.ADJUST_A2      = pHwConfig->curveAs.adjustA2;
    pRegCmdUpscale->detailEnhancerAdjustData1.bitfields.ADJUST_B0      = pHwConfig->curveBs.adjustB0;
    pRegCmdUpscale->detailEnhancerAdjustData1.bitfields.ADJUST_B1      = pHwConfig->curveBs.adjustB1;
    pRegCmdUpscale->detailEnhancerAdjustData1.bitfields.ADJUST_B2      = pHwConfig->curveBs.adjustB2;
    pRegCmdUpscale->detailEnhancerAdjustData2.bitfields.ADJUST_C0      = pHwConfig->curveCs.adjustC0;
    pRegCmdUpscale->detailEnhancerAdjustData2.bitfields.ADJUST_C1      = pHwConfig->curveCs.adjustC1;
    pRegCmdUpscale->detailEnhancerAdjustData2.bitfields.ADJUST_C2      = pHwConfig->curveCs.adjustC2;

    // source / destination size registers
    pRegCmdUpscale->sourceSize.bitfields.SRC_HEIGHT                    = pUnpackedRegisterData->dataPath.inputHeight[2];
    pRegCmdUpscale->sourceSize.bitfields.SRC_WIDTH                     = pUnpackedRegisterData->dataPath.inputWidth[2];
    pRegCmdUpscale->destinationSize.bitfields.DST_HEIGHT               = pUnpackedRegisterData->dataPath.outputHeight[2];
    pRegCmdUpscale->destinationSize.bitfields.DST_WIDTH                = pUnpackedRegisterData->dataPath.outputWidth[2];

    // Y channel horizontal / vertical initial phase registers
    pRegCmdUpscale->horizontalInitialPhaseY.bitfields.Y_PHASE_INIT_H   = pHwConfig->phaseInit.horizontalPhaseInitY;
    pRegCmdUpscale->verticalInitialPhaseY.bitfields.Y_PHASE_INIT_V     = pHwConfig->phaseInit.verticalPhaseInitY;

    // UV channel horizontal / vertical initial phase registers
    pRegCmdUpscale->horizontalInitialPhaseUV.bitfields.UV_PHASE_INIT_H = pHwConfig->phaseInit.horizontalPhaseInitUV;
    pRegCmdUpscale->verticalInitialPhaseUV.bitfields.UV_PHASE_INIT_V   = pHwConfig->phaseInit.verticalPhaseInitUV;

    // DMI LUTs
    UINT32* pTwoDFilterA = reinterpret_cast<UINT32*>(pHwConfig->twoDFilterA);
    UINT32* pTwoDFilterB = reinterpret_cast<UINT32*>(pHwConfig->twoDFilterB);
    UINT32* pTwoDFilterC = reinterpret_cast<UINT32*>(pHwConfig->twoDFilterC);
    UINT32* pTwoDFilterD = reinterpret_cast<UINT32*>(pHwConfig->twoDFilterD);
    UINT32 offset = 0;
    CalculateUpscale20LUT(&offset, pTwoDFilterA, 4, 42, IPEUpscaleLUTNumEntries[DMI_LUT_A], pLUT);
    CalculateUpscale20LUT(&offset, pTwoDFilterB, 4, 24, IPEUpscaleLUTNumEntries[DMI_LUT_B], pLUT);
    CalculateUpscale20LUT(&offset, pTwoDFilterC, 4, 24, IPEUpscaleLUTNumEntries[DMI_LUT_C], pLUT);
    CalculateUpscale20LUT(&offset, pTwoDFilterD, 4, 20, IPEUpscaleLUTNumEntries[DMI_LUT_D], pLUT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IPEUpscale20CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPEUpscale20CalculateSetting(
    const Upscale20InputData* pInput,
    VOID*                     pOEMIQData,
    Upscale20OutputData*      pOutput)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        BOOL                                                          commonIQResult     = FALSE;
        upscale_2_0_0::chromatix_upscale20_reserveType*               pReserveData       = NULL;
        upscale_2_0_0::chromatix_upscale20Type::enable_sectionStruct* pModuleEnable      = NULL;
        upscale_2_0_0::upscale20_rgn_dataType*                        pInterpolationData = NULL;

        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<upscale_2_0_0::upscale20_rgn_dataType*>(pInput->pInterpolationData);;
            pReserveData       = &(pInput->pChromatix->chromatix_upscale20_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            // Call the Interpolation Calculation
            commonIQResult     = IQInterface::s_interpolationTable.upscale20Interpolation(pInput, pInterpolationData);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<upscale_2_0_0::upscale20_rgn_dataType*>(
                                     &(pOEMInput->US20Setting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveData   = reinterpret_cast<upscale_2_0_0::chromatix_upscale20_reserveType*>(
                                     &(pOEMInput->US20ReserveType));
                pModuleEnable  = reinterpret_cast<upscale_2_0_0::chromatix_upscale20Type::enable_sectionStruct*>(
                                     &(pOEMInput->US20EnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            Upscale20UnpackedField unpackedRegDataUpscale = { 0 };

            commonIQResult = IQInterface::s_interpolationTable.upscale20CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pReserveData,
                                 pModuleEnable,
                                 &unpackedRegDataUpscale);

            if (TRUE == commonIQResult)
            {
                IPESetupUpscale20Register(&unpackedRegDataUpscale, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("IPEUpscale20 calculate hardware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Upscale2_0 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL pInput = %p, pOutput = %p ", pInput, pOutput);
    }

    CAMX_ASSERT(result == CamxResultSuccess);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupUpscale12Register
///
/// @brief  Setup Upscale12 Register based on common library output
///
/// @param  pUnpackedRegisterData    Pointer to the Common Library Calculation Result
/// @param  pOutput                  Pointer to the Upscale12 module Output
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupUpscale12Register(
    Upscale12UnpackedField* pUnpackedRegisterData,
    Upscale12OutputData*    pOutput)
{
    IPEUpscaleLiteRegCmd* pRegCmdUpscale = pOutput->pRegCmdUpscale;

    pRegCmdUpscale->lumaScaleCfg0.bitfields.H_SCALE_FIR_ALGORITHM     = pUnpackedRegisterData->lumaHScaleFirAlgorithm;
    pRegCmdUpscale->lumaScaleCfg0.bitfields.V_SCALE_FIR_ALGORITHM     = pUnpackedRegisterData->lumaVScaleFirAlgorithm;
    pRegCmdUpscale->lumaScaleCfg1.bitfields.INPUT_DITHERING_DISABLE   = pUnpackedRegisterData->lumaInputDitheringDisable;
    pRegCmdUpscale->lumaScaleCfg1.bitfields.INPUT_DITHERING_MODE      = pUnpackedRegisterData->lumaInputDitheringMode;
    pRegCmdUpscale->chromaScaleCfg0.bitfields.H_SCALE_FIR_ALGORITHM   = pUnpackedRegisterData->chromaHScaleFirAlgorithm;
    pRegCmdUpscale->chromaScaleCfg0.bitfields.V_SCALE_FIR_ALGORITHM   = pUnpackedRegisterData->chromaVScaleFirAlgorithm;
    pRegCmdUpscale->chromaScaleCfg1.bitfields.INPUT_DITHERING_DISABLE = pUnpackedRegisterData->chromaInputDitheringDisable;
    pRegCmdUpscale->chromaScaleCfg1.bitfields.INPUT_DITHERING_MODE    = pUnpackedRegisterData->chromaInputDitheringMode;
    pRegCmdUpscale->chromaScaleCfg1.bitfields.OUTPUT_ROUNDING_MODE_H  = pUnpackedRegisterData->chromaRoundingModeH;
    pRegCmdUpscale->chromaScaleCfg1.bitfields.OUTPUT_ROUNDING_MODE_V  = pUnpackedRegisterData->chromaRoundingModeV;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IPEUpscale12CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPEUpscale12CalculateSetting(
    const Upscale12InputData* pInput,
    VOID*                     pOEMIQData,
    Upscale12OutputData*      pOutput)
{
    CamxResult            result           = CamxResultSuccess;
    BOOL                  commonIQResult   = FALSE;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        commonIQResult = TRUE;

        if (NULL != pOEMIQData)
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);
            // Use OEM defined interpolation data
            CAMX_UNREFERENCED_PARAM(pOEMIQData);
        }
    }

    if (TRUE == commonIQResult)
    {
        Upscale12UnpackedField unpackedRegDataUpscale = { 0 };

        commonIQResult = IQInterface::s_interpolationTable.upscale12CalculateHWSetting(
                             pInput,
                             &unpackedRegDataUpscale);

        if (TRUE == commonIQResult)
        {
            IPESetupUpscale12Register(&unpackedRegDataUpscale, pOutput);
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("IPEUpscale12 calculate hardware setting failed");
        }

    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL pInput = %p, pOutput = %p ", pInput, pOutput);
    }

    CAMX_ASSERT(result == CamxResultSuccess);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPESetupGRA10Register
///
/// @brief  Setup GrainAdder10 Register based on common library output
///
/// @param  pUnpackedRegisterData    Pointer to the Common Library Calculation Result
/// @param  pOutput                  Pointer to the GrainAdder10 module Output
///
/// @return VOID
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID IPESetupGRA10Register(
    GRA10UnpackedField* pUnpackedRegisterData,
    GRA10OutputData*    pOutput)
{
    CAMX_ASSERT(NULL != pUnpackedRegisterData);
    CAMX_ASSERT(NULL != pOutput);

    pOutput->enableDitheringC = pUnpackedRegisterData->enable_dithering_C;
    pOutput->enableDitheringY = pUnpackedRegisterData->enable_dithering_Y;
    pOutput->grainSeed        = pUnpackedRegisterData->grain_seed;
    pOutput->grainStrength    = pUnpackedRegisterData->grain_strength;
    pOutput->mcgA             = pUnpackedRegisterData->mcg_a;
    pOutput->skiAheadAJump    = pUnpackedRegisterData->skip_ahead_a_jump;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::IPEGRA10CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::IPEGRA10CalculateSetting(
    const GRA10IQInput* pInput,
    VOID*               pOEMIQData,
    GRA10OutputData*    pOutput)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pInput) && (NULL != pOutput))
    {
        BOOL                                                  commonIQResult     = FALSE;
        gra_1_0_0::chromatix_gra10_reserveType*               pReserveData       = NULL;
        gra_1_0_0::chromatix_gra10Type::enable_sectionStruct* pModuleEnable      = NULL;
        gra_1_0_0::gra10_rgn_dataType*                        pInterpolationData = NULL;

        if (NULL == pOEMIQData)
        {
            pInterpolationData = static_cast<gra_1_0_0::gra10_rgn_dataType*>(pInput->pInterpolationData);
            pReserveData       = &(pInput->pChromatix->chromatix_gra10_reserve);
            pModuleEnable      = &(pInput->pChromatix->enable_section);

            // Call the Interpolation Calculation
            commonIQResult     = IQInterface::s_interpolationTable.gra10Interpolation(pInput, pInterpolationData);
        }
        else
        {
            OEMIPEIQSetting* pOEMInput = static_cast<OEMIPEIQSetting*>(pOEMIQData);

            // Use OEM defined interpolation data
            pInterpolationData = reinterpret_cast<gra_1_0_0::gra10_rgn_dataType*>(
                                     &(pOEMInput->GrainAdderSetting));

            if (NULL != pInterpolationData)
            {
                commonIQResult = TRUE;
                pReserveData   = reinterpret_cast<gra_1_0_0::chromatix_gra10_reserveType*>(
                                     &(pOEMInput->GrainAdderReserveType));
                pModuleEnable  = reinterpret_cast<gra_1_0_0::chromatix_gra10Type::enable_sectionStruct*>(
                                     &(pOEMInput->GrainAdderEnableSection));
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("No OEM Input Data");
            }
        }

        if (TRUE == commonIQResult)
        {
            GRA10UnpackedField unpackedRegisterData = { 0 };

            // set the unpacked register data LUT pointers to DMI memory buffer
            unpackedRegisterData.ch0_LUT.pGRATable  = pOutput->pLUT[GRALUTChannel0];
            unpackedRegisterData.ch1_LUT.numEntries = GRA10LUTNumEntriesPerChannelSize;
            unpackedRegisterData.ch1_LUT.pGRATable  = pOutput->pLUT[GRALUTChannel1];
            unpackedRegisterData.ch1_LUT.numEntries = GRA10LUTNumEntriesPerChannelSize;
            unpackedRegisterData.ch2_LUT.pGRATable  = pOutput->pLUT[GRALUTChannel2];
            unpackedRegisterData.ch2_LUT.numEntries = GRA10LUTNumEntriesPerChannelSize;

            commonIQResult = IQInterface::s_interpolationTable.gra10CalculateHWSetting(
                                 pInput,
                                 pInterpolationData,
                                 pReserveData,
                                 pModuleEnable,
                                 &unpackedRegisterData);

            if (TRUE == commonIQResult)
            {
                IPESetupGRA10Register(&unpackedRegisterData, pOutput);
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("IPEGRA10 calculate hardware setting failed");
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("GRA1_0 interpolation failed.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Input data is NULL pInput: %p, pOutput: %p ", pInput, pOutput);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IQInterface::TMC11CalculateSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IQInterface::TMC11CalculateSetting(
    TMC11InputData*  pInput)
{
    CamxResult                    result         = CamxResultSuccess;
    BOOL                          commonIQResult = FALSE;
    tmc_1_1_0::tmc11_rgn_dataType interpolationDataTMC[15];

    if (NULL != pInput)
    {
        commonIQResult = IQInterface::s_interpolationTable.TMC11Interpolation(pInput, interpolationDataTMC);
    }

    return result;
}

CAMX_NAMESPACE_END
