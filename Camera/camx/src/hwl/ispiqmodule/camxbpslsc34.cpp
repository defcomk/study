////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpslsc34.cpp
/// @brief CAMXBPSLSC34 class implementation
///        Corrects image intensity rolloff from center to corners due to lens optics by applying inverse gain to
///        compensate the lens rolloff effect
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpslsc34.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 BPSRolloffMeshPtHV34    = 17;  // MESH_PT_H = MESH_H + 1
static const UINT32 BPSRolloffMeshPtVV34    = 13;  // MESH_PT_V = MESH_V + 1
static const UINT32 BPSRolloffMeshSize      = (BPSRolloffMeshPtHV34 * BPSRolloffMeshPtVV34);

static const UINT32 RedLUT                  = BPS_BPS_0_CLC_LENS_ROLLOFF_DMI_LUT_CFG_LUT_SEL_RED_LUT;
static const UINT32 BlueLUT                 = BPS_BPS_0_CLC_LENS_ROLLOFF_DMI_LUT_CFG_LUT_SEL_BLUE_LUT;

static const UINT8  MaxLSC34NumDMITables    = 2;
static const UINT32 BPSLSC34DMISetSizeDword = BPSRolloffMeshSize;
static const UINT32 BPSLSC34LUTTableSize    = (BPSLSC34DMISetSizeDword * sizeof(UINT32));
static const UINT32 BPSLSC34DMILengthDword  = (BPSLSC34DMISetSizeDword * MaxLSC34NumDMITables);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLSC34::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult          result         = CamxResultSuccess;
    CREATETINTLESS      pTintlessFunc  = NULL;
    LSC34TintlessRatio* pTintlessRatio = NULL;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        BPSLSC34* pModule = CAMX_NEW BPSLSC34(pCreateData->pNodeIdentifier);

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (CamxResultSuccess == result)
            {
                // Load tintless library
                pTintlessFunc = reinterpret_cast<CREATETINTLESS>
                    (OsUtils::LoadPreBuiltLibrary(pTintlessLibName,
                        pTintlessFuncName,
                        &pModule->m_hHandle));

                if (NULL != pTintlessFunc)
                {
                    (*pTintlessFunc)(&pModule->m_pTintlessAlgo);
                }
                else
                {
                    result = CamxResultEInvalidPointer;
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Error loading tintless library");
                }
                // Allocate a Float array of 17 x 13 to store tintless ratio.
                // This will store the tintless ratio applied.
                // This will be used if tintless algo doesnt run
                pTintlessRatio = &(pModule->m_tintlessRatio);

                pTintlessRatio->pRGain  = static_cast<FLOAT*>(
                                               CAMX_CALLOC(sizeof(FLOAT) * BPSRolloffMeshPtHV34 * BPSRolloffMeshPtVV34));
                if (NULL == pTintlessRatio->pRGain)
                {
                    result = CamxResultENoMemory;
                }

                if (CamxResultSuccess == result)
                {
                    pTintlessRatio->pGrGain = static_cast<FLOAT*>(
                                                   CAMX_CALLOC(sizeof(FLOAT) * BPSRolloffMeshPtHV34 * BPSRolloffMeshPtVV34));
                }

                if (NULL == pTintlessRatio->pGrGain)
                {
                    result = CamxResultENoMemory;
                }

                if (CamxResultSuccess == result)
                {
                    pTintlessRatio->pGbGain = static_cast<FLOAT*>(
                                                   CAMX_CALLOC(sizeof(FLOAT) * BPSRolloffMeshPtHV34 * BPSRolloffMeshPtVV34));
                }

                if (NULL == pTintlessRatio->pGbGain)
                {
                    result = CamxResultENoMemory;
                }

                if (CamxResultSuccess == result)
                {
                    pTintlessRatio->pBGain  = static_cast<FLOAT*>(
                                                   CAMX_CALLOC(sizeof(FLOAT) * BPSRolloffMeshPtHV34 * BPSRolloffMeshPtVV34));
                }

                if (NULL == pTintlessRatio->pBGain)
                {
                    result = CamxResultENoMemory;
                }

                if (result != CamxResultSuccess)
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initialize common library data, no memory");
                    pModule->Destroy();
                    pModule = NULL;
                }
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupISP, "Module initialization failed !!");
            }

            if (CamxResultSuccess != result)
            {
                pModule->Destroy();
                pModule = NULL;
            }
        }
        else
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLSC34::Initialize()
{
    CamxResult     result          = CamxResultSuccess;

    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initilizee common library data, no memory");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::FillCmdBufferManagerParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLSC34::FillCmdBufferManagerParams(
    const ISPInputData*     pInputData,
    IQModuleCmdBufferParam* pParam)
{
    CamxResult result                  = CamxResultSuccess;
    ResourceParams* pResourceParams    = NULL;
    CHAR*           pBufferManagerName = NULL;

    if ((NULL != pParam) && (NULL != pParam->pCmdBufManagerParam) && (NULL != pInputData))
    {
        // The Resource Params and Buffer Manager Name will be freed by caller Node
        pResourceParams = static_cast<ResourceParams*>(CAMX_CALLOC(sizeof(ResourceParams)));
        if (NULL != pResourceParams)
        {
            pBufferManagerName = static_cast<CHAR*>(CAMX_CALLOC((sizeof(CHAR) * MaxStringLength256)));
            if (NULL != pBufferManagerName)
            {
                OsUtils::SNPrintF(pBufferManagerName, (sizeof(CHAR) * MaxStringLength256), "CBM_%s_%s",
                                  (NULL != m_pNodeIdentifier) ? m_pNodeIdentifier : " ", "BPSLSC34");
                pResourceParams->resourceSize                 = (BPSLSC34DMILengthDword * sizeof(UINT32));
                pResourceParams->poolSize                     = (pInputData->requestQueueDepth * pResourceParams->resourceSize);
                pResourceParams->usageFlags.cmdBuffer         = 1;
                pResourceParams->cmdParams.type               = CmdType::CDMDMI;
                pResourceParams->alignment                    = CamxCommandBufferAlignmentInBytes;
                pResourceParams->cmdParams.enableAddrPatching = 0;
                pResourceParams->cmdParams.maxNumNestedAddrs  = 0;
                pResourceParams->memFlags                     = CSLMemFlagUMDAccess;
                pResourceParams->pDeviceIndices               = pInputData->pipelineBPSData.pDeviceIndex;
                pResourceParams->numDevices                   = 1;

                pParam->pCmdBufManagerParam[pParam->numberOfCmdBufManagers].pBufferManagerName = pBufferManagerName;
                pParam->pCmdBufManagerParam[pParam->numberOfCmdBufManagers].pParams            = pResourceParams;
                pParam->pCmdBufManagerParam[pParam->numberOfCmdBufManagers].ppCmdBufferManager = &m_pLUTCmdBufferManager;
                pParam->numberOfCmdBufManagers++;

            }
            else
            {
                result = CamxResultENoMemory;
                CAMX_FREE(pResourceParams);
                pResourceParams = NULL;
                CAMX_LOG_ERROR(CamxLogGroupISP, "Out Of Memory");
            }
        }
        else
        {
            result = CamxResultENoMemory;
            CAMX_LOG_ERROR(CamxLogGroupISP, "Out Of Memory");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Input Error %p", pParam);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLSC34::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(lsc_3_4_0::lsc34_rgn_dataType) * (LSC34MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for lsc_3_4_0::lsc34_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::TranslateCalibrationTableToCommonLibrary
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSLSC34::TranslateCalibrationTableToCommonLibrary(
    const ISPInputData* pInputData)
{
    INT32 index                = -1;

    CAMX_ASSERT(NULL != pInputData);

    if (NULL != pInputData->pOTPData)
    {
        if (NULL == m_pLSCCalibrationData)
        {
            // This allocation will happen once per session
            m_pLSCCalibrationData =
                static_cast<LSCCalibrationData*>(CAMX_CALLOC(MaxLightTypes *
                                                              sizeof(LSCCalibrationData)));
        }

        if (NULL == m_dependenceData.pCalibrationTable)
        {
            // This allocation will happen once per session
            m_dependenceData.numTables          = MaxLightTypes;
            m_dependenceData.pCalibrationTable =
                static_cast<LSC34CalibrationDataTable*>(CAMX_CALLOC(MaxLightTypes *
                                                                    sizeof(LSC34CalibrationDataTable)));
        }

        if (NULL != m_dependenceData.pCalibrationTable)
        {
            for (UINT32 lightIndex = 0; lightIndex < MaxLightTypes; lightIndex++)
            {
                if (TRUE == pInputData->pOTPData->LSCCalibration[lightIndex].isAvailable)
                {
                    index++;
                    // This memcpy will happen one first request and subsequent chromatix change
                    Utils::Memcpy(&(m_pLSCCalibrationData[index]),
                        &(pInputData->pOTPData->LSCCalibration[lightIndex]),
                        sizeof(LSCCalibrationData));

                    m_dependenceData.pCalibrationTable[index].pBGain =
                        m_pLSCCalibrationData[index].bGain;
                    m_dependenceData.pCalibrationTable[index].pRGain =
                        m_pLSCCalibrationData[index].rGain;
                    m_dependenceData.pCalibrationTable[index].pGBGain =
                        m_pLSCCalibrationData[index].gbGain;
                    m_dependenceData.pCalibrationTable[index].pGRGain =
                        m_pLSCCalibrationData[index].grGain;
                    m_dependenceData.toCalibrate = TRUE;
                    m_dependenceData.enableCalibration = TRUE;
                }
            }

            m_dependenceData.numberOfEEPROMTable = index + 1;

            if (-1 == index)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid index for EEPROM data");
                m_dependenceData.toCalibrate = FALSE;
                m_dependenceData.enableCalibration = FALSE;
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Calibration data from sensor is not present");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSLSC34::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)             &&
        (NULL != pInputData->pHwContext) &&
        (NULL != pInputData->pHALTagsData))
    {
        ISPHALTagsData* pHALTagsData = pInputData->pHALTagsData;

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->LSCEnable;
            isChanged      = (TRUE == m_moduleEnable);

            m_shadingMode        = ShadingModeFast;
            m_lensShadingMapMode = StatisticsLensShadingMapModeOff;
        }
        else
        {
            m_shadingMode        = pHALTagsData->shadingMode;
            m_lensShadingMapMode = pHALTagsData->statisticsLensShadingMapMode;
            tintless_2_0_0::chromatix_tintless20Type* pTintlessChromatix = NULL;

            TuningDataManager*                        pTuningManager    = pInputData->pTuningDataManager;
            CAMX_ASSERT(NULL != pTuningManager);

            if (TRUE == pTuningManager->IsValidChromatix())
            {
                CAMX_ASSERT(NULL != pInputData->pTuningData);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if (TRUE == pInputData->tuningModeChanged)
                {
                    m_pChromatix   = pTuningManager->GetChromatix()->GetModule_lsc34_bps(
                                          reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                          pInputData->pTuningData->noOfSelectionParameter);
                }
                pTintlessChromatix = pTuningManager->GetChromatix()->GetModule_tintless20_sw(
                                          reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                          pInputData->pTuningData->noOfSelectionParameter);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupIQMod, "Invaild chromatix");
            }

            CAMX_ASSERT(NULL != pTintlessChromatix);
            if (NULL != pTintlessChromatix)
            {
                m_dependenceData.pTintlessChromatix = pTintlessChromatix;
                m_dependenceData.enableTintless     = pTintlessChromatix->enable_section.tintless_en;
                if (TRUE == m_dependenceData.enableTintless)
                {
                    if ( TRUE != pInputData->registerBETEn )
                    {
                        if ((TRUE == pInputData->pHwContext->GetStaticSettings()->tintlessEnable) ||
                            (TRUE == pInputData->tintlessEnable))
                        {
                            m_dependenceData.enableTintless = TRUE;
                        }
                        else
                        {
                            m_dependenceData.enableTintless = FALSE;
                        }
                    }
                }
            }


            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if (m_pChromatix != m_dependenceData.pChromatix)
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.rolloff_enable;

                    isChanged                   = (TRUE == m_moduleEnable);

                    TranslateCalibrationTableToCommonLibrary(pInputData);
                }
                if ((TRUE == m_moduleEnable)  &&
                    (ShadingModeOff == pHALTagsData->shadingMode))
                {
                    m_moduleEnable = FALSE;
                    isChanged      = FALSE;
                }
                else if (FALSE == m_moduleEnable)
                {
                    m_shadingMode = ShadingModeOff;
                }

                /// @todo (CAMX-1164) Dual IFE changes for LSC to set the right sensor width/height.
                if (TRUE == m_moduleEnable)
                {
                    /// @todo (CAMX-1813) Sensor need to send full res width and height too
                    /// @todo (CAMX-1164) Dual IFE changes for LSC to set the right width/height.
                    if (FALSE == Utils::FEqual(pInputData->sensorData.sensorBinningFactor, 0.0f))
                    {
                        m_dependenceData.fullResWidth  = static_cast<UINT32>(
                            pInputData->sensorData.fullResolutionWidth / pInputData->sensorData.sensorBinningFactor);
                        m_dependenceData.fullResHeight = static_cast<UINT32>(
                            pInputData->sensorData.fullResolutionHeight / pInputData->sensorData.sensorBinningFactor);
                    }
                    else
                    {
                        m_dependenceData.fullResWidth  = pInputData->sensorData.fullResolutionWidth;
                        m_dependenceData.fullResHeight = pInputData->sensorData.fullResolutionHeight;
                    }

                    m_dependenceData.offsetX       = pInputData->sensorData.sensorOut.offsetX;
                    m_dependenceData.offsetY       = pInputData->sensorData.sensorOut.offsetY;
                    m_dependenceData.imageWidth    = pInputData->sensorData.sensorOut.width;
                    m_dependenceData.imageHeight   = pInputData->sensorData.sensorOut.height;
                    m_dependenceData.scalingFactor = static_cast<UINT32>(pInputData->sensorData.sensorScalingFactor);

                    if (TRUE == IQInterface::s_interpolationTable.LSC34TriggerUpdate(&(pInputData->triggerData),
                                                                                     &m_dependenceData))
                    {
                        isChanged = TRUE;
                    }
                }
                CAMX_LOG_VERBOSE(CamxLogGroupISP, "colorCorrectionMode = %d, controlAWBMode = %d, controlMode = %d",
                                 pHALTagsData->colorCorrectionMode,
                                 pHALTagsData->controlAWBMode,
                                 pHALTagsData->controlMode);

            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Input: pInputData %p", pInputData);
    }

    if (TRUE == isChanged)
    {
        m_tintlessConfig.statsConfig.camifWidth =
            pInputData->sensorData.CAMIFCrop.lastPixel - pInputData->sensorData.CAMIFCrop.firstPixel + 1;
        m_tintlessConfig.statsConfig.camifHeight =
            pInputData->sensorData.CAMIFCrop.lastLine - pInputData->sensorData.CAMIFCrop.firstLine + 1;
        m_tintlessConfig.statsConfig.postBayer = 0;
        m_tintlessConfig.statsConfig.statsBitDepth =
            pInputData->pStripeConfig->statsDataForISP.tintlessBGConfig.tintlessBGConfig.outputBitDepth;
        m_tintlessConfig.statsConfig.statsRegionHeight =
            pInputData->pStripeConfig->statsDataForISP.tintlessBGConfig.tintlessBGConfig.regionHeight;
        m_tintlessConfig.statsConfig.statsRegionWidth =
            pInputData->pStripeConfig->statsDataForISP.tintlessBGConfig.tintlessBGConfig.regionWidth;
        m_tintlessConfig.statsConfig.statsNumberOfHorizontalRegions =
            pInputData->pStripeConfig->statsDataForISP.tintlessBGConfig.tintlessBGConfig.horizontalNum;
        m_tintlessConfig.statsConfig.statsNumberOfVerticalRegions =
            pInputData->pStripeConfig->statsDataForISP.tintlessBGConfig.tintlessBGConfig.verticalNum;

        m_tintlessConfig.tintlessParamConfig.applyTemporalFiltering = 0;

        m_pTintlessBGStats = pInputData->pStripeConfig->statsDataForISP.pParsedTintlessBGStats;
    }

    isChanged = TRUE;

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::WriteLUTtoDMI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLSC34::WriteLUTtoDMI(
    const ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pDMIBuffer = pInputData->pDMICmdBuffer;
    UINT32     LUTOffset;

    CAMX_ASSERT(NULL != pDMIBuffer);

    // CDM pack the DMI buffer and patch the Red and Blue LUT DMI buffer into CDM pack
    LUTOffset = 0;
    result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                     regBPS_BPS_0_CLC_LENS_ROLLOFF_DMI_CFG,
                                     RedLUT,
                                     m_pLUTDMICmdBuffer,
                                     LUTOffset,
                                     BPSLSC34LUTTableSize);
    CAMX_ASSERT(CamxResultSuccess == result);

    LUTOffset += BPSLSC34LUTTableSize;

    result = PacketBuilder::WriteDMI(pInputData->pDMICmdBuffer,
                                     regBPS_BPS_0_CLC_LENS_ROLLOFF_DMI_CFG,
                                     BlueLUT,
                                     m_pLUTDMICmdBuffer,
                                     LUTOffset,
                                     BPSLSC34LUTTableSize);
    CAMX_ASSERT(CamxResultSuccess == result);
    m_dependenceData.bankSelect ^= 1;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLSC34::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    result = WriteLUTtoDMI(pInputData);

    if ((CamxResultSuccess == result) && (NULL != pInputData->pCmdBuffer))
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_LENS_ROLLOFF_LENS_ROLLOFF_0_CFG,
                                              (sizeof(BPSLSC34RegConfig) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd));

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
        }
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::FetchDMIBuffer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLSC34::FetchDMIBuffer(
    const ISPInputData* pInputData)
{
    CamxResult      result          = CamxResultSuccess;
    PacketResource* pPacketResource = NULL;

    CAMX_UNREFERENCED_PARAM(pInputData);

    if (NULL != m_pLUTCmdBufferManager)
    {
        // Recycle the last updated LUT DMI cmd buffer
        if (NULL != m_pLUTDMICmdBuffer)
        {
            m_pLUTCmdBufferManager->Recycle(m_pLUTDMICmdBuffer);
        }

        // fetch a fresh LUT DMI cmd buffer
        result = m_pLUTCmdBufferManager->GetBuffer(&pPacketResource);
    }
    else
    {
        result = CamxResultEFailed;
        CAMX_LOG_ERROR(CamxLogGroupISP, "LUT Command Buffer Manager is NULL");
    }

    if (CamxResultSuccess == result)
    {
        m_pLUTDMICmdBuffer = static_cast<CmdBuffer*>(pPacketResource);
    }
    else
    {
        m_pLUTDMICmdBuffer = NULL;

        result             = CamxResultEUnableToLoad;
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to fetch DMI Buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLSC34::RunCalculation(
    ISPInputData*    pInputData,
    LensRollOffParameters* pLscModuleConfig)
{
    CamxResult      result         = CamxResultSuccess;

    if ((NULL != pInputData) /* && (NULL != pLscModuleConfig) */)
    {
        result = FetchDMIBuffer(pInputData);

        if (CamxResultSuccess == result)
        {
            LSC34OutputData outputData;

            // Update the LUT DMI buffer with Red LUT data
            outputData.pGRRLUTDMIBuffer  =
                reinterpret_cast<UINT32*>(m_pLUTDMICmdBuffer->BeginCommands(BPSLSC34DMILengthDword));
            CAMX_ASSERT(NULL != outputData.pGRRLUTDMIBuffer);
            // BET ONLY - InputData is different per module tested
            pInputData->pBetDMIAddr = static_cast<VOID*>(outputData.pGRRLUTDMIBuffer);

            // Update the LUT DMI buffer with Blue LUT data
            outputData.pGBBLUTDMIBuffer  =
                reinterpret_cast<UINT32*>(reinterpret_cast<UCHAR*>(outputData.pGRRLUTDMIBuffer) + BPSLSC34LUTTableSize);
            CAMX_ASSERT(NULL != outputData.pGBBLUTDMIBuffer);

            outputData.type              = PipelineType::BPS;
            outputData.BPS.pRegBPSCmd    = &m_regCmd;
            outputData.BPS.pModuleConfig = pLscModuleConfig;
            outputData.pUnpackedField    = &m_unpackedData.unpackedData;

            if ((pInputData->pHALTagsData->controlAWBLock == m_AWBLock)    &&
                (pInputData->pHALTagsData->controlAWBLock == ControlAWBLockOn))
            {
                m_dependenceData.enableTintless = FALSE;
            }
            else
            {
                m_AWBLock = pInputData->pHALTagsData->controlAWBLock;
            }

            if (TRUE == m_dependenceData.enableTintless && NULL == pInputData->pOEMIQSetting)
            {
                m_dependenceData.pTintlessAlgo   = m_pTintlessAlgo;
                m_dependenceData.pTintlessConfig = &m_tintlessConfig;
                m_dependenceData.pTintlessStats  = m_pTintlessBGStats;
            }
            else
            {
                m_dependenceData.pTintlessAlgo   = NULL;
                m_dependenceData.pTintlessConfig = NULL;
                m_dependenceData.pTintlessStats  = NULL;
            }

            m_dependenceData.pTintlessRatio = &m_tintlessRatio;

            result = IQInterface::LSC34CalculateSetting(&m_dependenceData,
                                                        pInputData->pOEMIQSetting,
                                                        pInputData,
                                                        &outputData);

            if (CamxResultSuccess == result)
            {
                m_pLUTDMICmdBuffer->CommitCommands();

                if (TRUE == m_dependenceData.toCalibrate)
                {
                    m_dependenceData.toCalibrate = FALSE;
                }

                if (NULL != pInputData->pBPSTuningMetadata)
                {
                    m_pGRRLUTDMIBuffer = outputData.pGRRLUTDMIBuffer;
                    m_pGBBLUTDMIBuffer = outputData.pGBBLUTDMIBuffer;
                }
                if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
                {
                    DumpRegConfig(outputData.pGRRLUTDMIBuffer);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "LSC Calculation Failed %d", result);
            }
        }
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSLSC34::UpdateBPSInternalData(
    const ISPInputData* pInputData)
{
    BpsIQSettings*  pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->lensRollOffParameters.moduleCfg.EN = m_moduleEnable;

    if ((ShadingModeOff == m_shadingMode) &&
        (StatisticsLensShadingMapModeOn == m_lensShadingMapMode))
    {
        // Pass the unity matrix for shading mode is OFF.
        for (UINT16 i = 0; i < (4 * BPSRolloffMeshPtVV34 * BPSRolloffMeshPtHV34); i++)
        {
            pInputData->pCalculatedData->lensShadingInfo.lensShadingMap[i] = 1.0f;
        }
    }
    else if (StatisticsLensShadingMapModeOn == pInputData->pHALTagsData->statisticsLensShadingMapMode)
    {
        IQInterface::CopyLSCMapData(pInputData, &m_unpackedData.unpackedData);
    }

    pInputData->pCalculatedData->lensShadingInfo.lensShadingMapSize.width  = BPSRolloffMeshPtHV34;
    pInputData->pCalculatedData->lensShadingInfo.lensShadingMapSize.height = BPSRolloffMeshPtVV34;
    pInputData->pCalculatedData->lensShadingInfo.lensShadingMapMode        = m_lensShadingMapMode;
    pInputData->pCalculatedData->lensShadingInfo.shadingMode               = m_shadingMode;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(BPSLSC34LUTTableSize <=
                           sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.LSCMesh.GRRLUT));
        CAMX_STATIC_ASSERT(BPSLSC34LUTTableSize <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.LSCMesh.GBBLUT));
        CAMX_STATIC_ASSERT(sizeof(BPSLSC34RegConfig) <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSLSCData.rolloffConfig));

        if (NULL != m_pGRRLUTDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.LSCMesh.GRRLUT[0],
                          m_pGRRLUTDMIBuffer,
                          BPSLSC34LUTTableSize);
        }
        if (NULL != m_pGBBLUTDMIBuffer)
        {
            Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDMIData.packedLUT.LSCMesh.GBBLUT[0],
                          m_pGBBLUTDMIBuffer,
                          BPSLSC34LUTTableSize);
        }
        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSLSCData.rolloffConfig,
                      &m_regCmd,
                      sizeof(pInputData->pBPSTuningMetadata->BPSLSCData.rolloffConfig));
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSLSC34::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        /// @todo (CAMX-1164) Dual IFE changes for LSC.
        if (TRUE == CheckDependenceChange(pInputData))
        {
            BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);

            result = RunCalculation(pInputData, &pBPSIQSettings->lensRollOffParameters);
        }

        if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
        {
            result = CreateCmdList(pInputData);
        }

        if (CamxResultSuccess == result)
        {
            UpdateBPSInternalData(pInputData);
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Operation failed %d", result);
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Input: pInputData %p", pInputData);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSLSC34::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::BPSLSC34
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSLSC34::BPSLSC34(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type              = ISPIQModuleType::BPSLSC;
    m_moduleEnable      = FALSE;
    // m_dependenceData.bankSelect = 0;
    m_numLUT            = MaxLSC34NumDMITables;
    m_cmdLength         =
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSLSC34RegConfig) / RegisterWidthInBytes);
    m_pGRRLUTDMIBuffer  = NULL;
    m_pGBBLUTDMIBuffer  = NULL;
    m_pChromatix        = NULL;
    m_pTinlessChromatix = NULL;
    m_AWBLock           = ControlAWBLockOff;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::~BPSLSC34
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSLSC34::~BPSLSC34()
{
    m_tintlessRatio.isValid = FALSE;

    CAMX_FREE(m_tintlessRatio.pRGain);
    m_tintlessRatio.pRGain = NULL;
    CAMX_FREE(m_tintlessRatio.pGrGain);
    m_tintlessRatio.pGrGain = NULL;
    CAMX_FREE(m_tintlessRatio.pGbGain);
    m_tintlessRatio.pGbGain = NULL;
    CAMX_FREE(m_tintlessRatio.pBGain);
    m_tintlessRatio.pBGain = NULL;

    if (NULL != m_pLUTCmdBufferManager)
    {
        if (NULL != m_pLUTDMICmdBuffer)
        {
            m_pLUTCmdBufferManager->Recycle(m_pLUTDMICmdBuffer);
            m_pLUTDMICmdBuffer = NULL;
        }

        m_pLUTCmdBufferManager->Uninitialize();
        CAMX_DELETE m_pLUTCmdBufferManager;
        m_pLUTCmdBufferManager = NULL;
    }

    if (NULL != m_dependenceData.pCalibrationTable)
    {
        CAMX_FREE(m_dependenceData.pCalibrationTable);
        m_dependenceData.pCalibrationTable = NULL;
    }

    if (NULL != m_pLSCCalibrationData)
    {
        CAMX_FREE(m_pLSCCalibrationData);
        m_pLSCCalibrationData = NULL;
    }

    if (NULL != m_pTintlessAlgo)
    {
        m_pTintlessAlgo->TintlessDestroy(m_pTintlessAlgo);
        m_pTintlessAlgo = NULL;
    }

    if (NULL != m_hHandle)
    {
        CamX::OsUtils::LibUnmap(m_hHandle);
        m_hHandle = NULL;
    }

    m_pChromatix        = NULL;
    m_pTinlessChromatix = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSLSC34::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSLSC34::DumpRegConfig(
    const UINT32* pGRRLUTDMIBuffer
    ) const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** BPS LSC34 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* LSC34 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "Config 0 = %x\n", m_regCmd.config0);
        CamX::OsUtils::FPrintF(pFile, "Config 1 = %x\n", m_regCmd.config1);
        CamX::OsUtils::FPrintF(pFile, "Config 2 = %x\n", m_regCmd.config2);
        CamX::OsUtils::FPrintF(pFile, "Config 3 = %x\n", m_regCmd.config3);
        CamX::OsUtils::FPrintF(pFile, "Config 4 = %x\n", m_regCmd.config4);
        CamX::OsUtils::FPrintF(pFile, "Config 5 = %x\n", m_regCmd.config5);
        CamX::OsUtils::FPrintF(pFile, "Config 6 = %x\n", m_regCmd.config6);

        if (TRUE == m_moduleEnable)
        {
            UINT32 DMICnt = 0;

            CamX::OsUtils::FPrintF(pFile, "* channel[0] = \n");
            CamX::OsUtils::FPrintF(pFile, "    ** channel[0][0] = \n");
            for (UINT32 verMeshNum = 0; verMeshNum < ROLLOFF_MESH_PT_V_V34; verMeshNum++)
            {
                for (UINT32 horMeshNum = 0; horMeshNum < ROLLOFF_MESH_PT_H_V34; horMeshNum++)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d",
                        ((*(pGRRLUTDMIBuffer + DMICnt)) & Utils::AllOnes32(13)));
                    DMICnt++;
                }
                CamX::OsUtils::FPrintF(pFile, "\n");
            }
            DMICnt = 0;
            CamX::OsUtils::FPrintF(pFile, "    ** channel[0][1] = \n");
            for (UINT32 verMeshNum = 0; verMeshNum < ROLLOFF_MESH_PT_V_V34; verMeshNum++)
            {
                for (UINT32 horMeshNum = 0; horMeshNum < ROLLOFF_MESH_PT_H_V34; horMeshNum++)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d",
                        (((*(pGRRLUTDMIBuffer + DMICnt)) >> 13) & Utils::AllOnes32(13)));
                    DMICnt++;
                }
                CamX::OsUtils::FPrintF(pFile, "\n");
            }
            DMICnt = 0;
            CamX::OsUtils::FPrintF(pFile, "    ** channel[0][2] = \n");
            for (UINT32 verMeshNum = 0; verMeshNum < ROLLOFF_MESH_PT_V_V34; verMeshNum++)
            {
                for (UINT32 horMeshNum = 0; horMeshNum < ROLLOFF_MESH_PT_H_V34; horMeshNum++)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d",
                        (((*(pGRRLUTDMIBuffer + (ROLLOFF_MESH_PT_V_V34*ROLLOFF_MESH_PT_H_V34) + DMICnt)) >> 13)
                        & Utils::AllOnes32(13)));
                    DMICnt++;
                }
                CamX::OsUtils::FPrintF(pFile, "\n");
            }
            DMICnt = 0;
            CamX::OsUtils::FPrintF(pFile, "    ** channel[0][3] = \n");
            for (UINT32 verMeshNum = 0; verMeshNum < ROLLOFF_MESH_PT_V_V34; verMeshNum++)
            {
                for (UINT32 horMeshNum = 0; horMeshNum < ROLLOFF_MESH_PT_H_V34; horMeshNum++)
                {
                    CamX::OsUtils::FPrintF(pFile, "           %d",
                        ((*(pGRRLUTDMIBuffer + (ROLLOFF_MESH_PT_V_V34*ROLLOFF_MESH_PT_H_V34) + DMICnt))
                        & Utils::AllOnes32(13)));
                    DMICnt++;
                }
                CamX::OsUtils::FPrintF(pFile, "\n");
            }
        }
        else
        {
            CamX::OsUtils::FPrintF(pFile, "NOT ENABLED");
        }
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }

}

CAMX_NAMESPACE_END
