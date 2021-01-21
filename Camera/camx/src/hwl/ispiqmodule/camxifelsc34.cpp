////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifelsc34.cpp
/// @brief ifelsc34 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifelsc34.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT RolloffLGRRBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_ROLLOFF_RAM_L_GR_R_BANK0;
static const UINT RolloffLGBBBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_ROLLOFF_RAM_L_GB_B_BANK0;
static const UINT RolloffLGRRBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_ROLLOFF_RAM_L_GR_R_BANK1;
static const UINT RolloffLGBBBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_ROLLOFF_RAM_L_GB_B_BANK1;

static const UINT32 IFERolloffMeshPtHV34 = 17;  // MESH_PT_H = MESH_H + 1
static const UINT32 IFERolloffMeshPtVV34 = 13;  // MESH_PT_V = MESH_V + 1
static const UINT32 IFERolloffMeshSize   = IFERolloffMeshPtHV34 * IFERolloffMeshPtVV34;

static const UINT32 IFELSC34RegLengthDword  = sizeof(IFELSC34RegCmd) / sizeof(UINT32);
static const UINT8  IFELSC34NumDMITables    = 2;
static const UINT32 IFELSC34DMISetSizeDword = IFERolloffMeshSize;
static const UINT32 IFELSC34LUTTableSize    = IFELSC34DMISetSizeDword * sizeof(UINT32);
static const UINT32 IFELSC34DMILengthDword  = IFELSC34DMISetSizeDword * IFELSC34NumDMITables;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELSC34::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult          result         = CamxResultSuccess;
    CREATETINTLESS      pTintlessFunc  = NULL;
    UINT32              index          = 0;
    LSC34TintlessRatio* pTintlessRatio = NULL;

    if (NULL != pCreateData)
    {
        IFELSC34* pModule = CAMX_NEW IFELSC34;

        if (NULL != pModule)
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
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Error loading tintless library");
            }

            CamX::Utils::Memset(&pCreateData->initializationData.pStripeConfig->stateLSC,
                                0,
                                sizeof(pCreateData->initializationData.pStripeConfig->stateLSC));

            result = pModule->Initialize();

            // Allocate a Float array of 17 x 13 to store tintless ratio.
            // This will store the tintless ratio applied.
            // This will be used if tintless algo doesnt run
            pTintlessRatio = &(pModule->m_tintlessRatio);

            pTintlessRatio->pRGain  = static_cast<FLOAT*>(
                                           CAMX_CALLOC(sizeof(FLOAT) * IFERolloffMeshPtHV34 * IFERolloffMeshPtVV34));
            if (NULL == pTintlessRatio->pRGain)
            {
                result = CamxResultENoMemory;
            }

            if (CamxResultSuccess == result)
            {
                pTintlessRatio->pGrGain = static_cast<FLOAT*>(
                                               CAMX_CALLOC(sizeof(FLOAT) * IFERolloffMeshPtHV34 * IFERolloffMeshPtVV34));
            }

            if (NULL == pTintlessRatio->pGrGain)
            {
                result = CamxResultENoMemory;
            }

            if (CamxResultSuccess == result)
            {
                pTintlessRatio->pGbGain = static_cast<FLOAT*>(
                                               CAMX_CALLOC(sizeof(FLOAT) * IFERolloffMeshPtHV34 * IFERolloffMeshPtVV34));
            }

            if (NULL == pTintlessRatio->pGbGain)
            {
                result = CamxResultENoMemory;
            }

            if (CamxResultSuccess == result)
            {
                pTintlessRatio->pBGain  = static_cast<FLOAT*>(
                                               CAMX_CALLOC(sizeof(FLOAT) * IFERolloffMeshPtHV34 * IFERolloffMeshPtVV34));
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
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFELSC34 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELSC34::Initialize()
{
    CamxResult result = CamxResultSuccess;
    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Unable to initilize common library data, no memory");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELSC34::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(lsc_3_4_0::lsc34_rgn_dataType) * (LSC34MaxmiumNonLeafNode + 1));

    if (NULL == m_pInterpolationData)
    {
        // Alloc for lsc_3_4_0::lsc34_rgn_dataType
        m_pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELSC34::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        m_pState = &pInputData->pStripeConfig->stateLSC;

        if (TRUE == CheckDependenceChange(pInputData))
        {
            result = RunCalculation(pInputData);

            if (CamxResultSuccess == result)
            {
                result = CreateCmdList(pInputData);
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("LSC module calculation Failed.");
            }
        }

        if (CamxResultSuccess == result)
        {
            UpdateIFEInternalData(pInputData);
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
// IFELSC34::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFELSC34::GetRegCmd()
{
    return static_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::TranslateCalibrationTableToCommonLibrary
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFELSC34::TranslateCalibrationTableToCommonLibrary(
    const ISPInputData* pInputData)
{
    INT32 index = -1;

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

        if (NULL == m_pState->dependenceData.pCalibrationTable)
        {
            // This allocation will happen once per session
            if (NULL == m_pCalibrationTable)
            {
                m_pCalibrationTable =
                    static_cast<LSC34CalibrationDataTable*>(CAMX_CALLOC(MaxLightTypes *
                        sizeof(LSC34CalibrationDataTable)));
            }
            CAMX_ASSERT(NULL != m_pCalibrationTable);
            m_pState->dependenceData.pCalibrationTable = m_pCalibrationTable;
            m_pState->dependenceData.numTables         = MaxLightTypes;
        }

        if (NULL != m_pState->dependenceData.pCalibrationTable)
        {
            for (UINT32 lightIndex = 0; lightIndex < MaxLightTypes; lightIndex++)
            {
                if (TRUE == pInputData->pOTPData->LSCCalibration[lightIndex].isAvailable)
                {
                    // This memcpy will happen one first request and subsequent chromatix change
                    index++;
                    Utils::Memcpy(&(m_pLSCCalibrationData[index]),
                        &(pInputData->pOTPData->LSCCalibration[lightIndex]),
                        sizeof(LSCCalibrationData));

                    m_pState->dependenceData.pCalibrationTable[index].pBGain =
                        m_pLSCCalibrationData[index].bGain;
                    m_pState->dependenceData.pCalibrationTable[index].pRGain =
                        m_pLSCCalibrationData[index].rGain;
                    m_pState->dependenceData.pCalibrationTable[index].pGBGain =
                        m_pLSCCalibrationData[index].gbGain;
                    m_pState->dependenceData.pCalibrationTable[index].pGRGain =
                        m_pLSCCalibrationData[index].grGain;
                    m_pState->dependenceData.toCalibrate = TRUE;
                    m_pState->dependenceData.enableCalibration = TRUE;
                }
            }

            m_pState->dependenceData.numberOfEEPROMTable = index + 1;

            if (-1 == index)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid index for EEPROM data");
                m_pState->dependenceData.toCalibrate = FALSE;
                m_pState->dependenceData.enableCalibration = FALSE;
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Calibration data from sensor is not present");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFELSC34::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL   isChanged         = FALSE;
    /// @todo (CAMX-1813) These scale factor  comes from the Sensor
    UINT16 scale             = 1;
    UINT32 width;
    UINT32 height;
    BOOL   skipLSCProcessing = FALSE;

    if ((NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData) &&
        (NULL != pInputData->pHALTagsData))
    {
        ISPHALTagsData* pHALTagsData = pInputData->pHALTagsData;

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->LSCEnable;

            if (TRUE == m_moduleEnable)
            {
                isChanged = TRUE;
            }

            m_shadingMode        = ShadingModeFast;
            m_lensShadingMapMode = StatisticsLensShadingMapModeOff;
        }
        else
        {
            m_shadingMode        = pHALTagsData->shadingMode;
            m_lensShadingMapMode = pHALTagsData->statisticsLensShadingMapMode;

            tintless_2_0_0::chromatix_tintless20Type* pTintlessChromatix = NULL;
            TuningDataManager*                        pTuningManager     = pInputData->pTuningDataManager;
            CAMX_ASSERT(NULL != pTuningManager);

            if (TRUE == pTuningManager->IsValidChromatix())
            {
                CAMX_ASSERT(NULL != pInputData->pTuningData);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if (TRUE == pInputData->tuningModeChanged)
                {
                    m_pChromatix   = pTuningManager->GetChromatix()->GetModule_lsc34_ife(
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
                m_pState->dependenceData.pTintlessChromatix = pTintlessChromatix;
                m_pState->dependenceData.enableTintless     = pTintlessChromatix->enable_section.tintless_en;
                if (TRUE != pInputData->registerBETEn)
                {
                    if ((TRUE == pInputData->pHwContext->GetStaticSettings()->tintlessEnable)    ||
                        (TRUE == pInputData->tintlessEnable))
                    {
                        m_pState->dependenceData.enableTintless = TRUE;
                    }
                    else
                    {
                        m_pState->dependenceData.enableTintless = FALSE;
                    }
                }
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if ((NULL == m_pState->dependenceData.pChromatix)                                       ||
                    (m_pChromatix->SymbolTableID != m_pState->dependenceData.pChromatix->SymbolTableID) ||
                    (m_moduleEnable              != m_pChromatix->enable_section.rolloff_enable))
                {
                    m_pState->dependenceData.pChromatix         = m_pChromatix;
                    m_pState->dependenceData.pInterpolationData = m_pInterpolationData;
                    m_moduleEnable                              = m_pChromatix->enable_section.rolloff_enable;
                    if (TRUE == m_moduleEnable)
                    {
                        isChanged = TRUE;
                    }

                    TranslateCalibrationTableToCommonLibrary(pInputData);
                }
            }
        }

        if (TRUE == m_moduleEnable)
        {
            if (TRUE ==
                IQInterface::s_interpolationTable.LSC34TriggerUpdate(&pInputData->triggerData, &m_pState->dependenceData))
            {
                isChanged = TRUE;
            }
        }

        width  = pInputData->sensorData.sensorOut.width;
        height = pInputData->sensorData.sensorOut.height;

        if (TRUE == pInputData->HVXData.DSEnabled)
        {
            width  = pInputData->HVXData.HVXOut.width;
            height = pInputData->HVXData.HVXOut.height;
        }

        if ((TRUE == m_moduleEnable) &&
             ((m_pState->dependenceData.imageWidth != width)   ||
             (m_pState->dependenceData.imageHeight != height)  ||
             (m_pState->dependenceData.scalingFactor != scale) ||
             (TRUE  == pInputData->forceTriggerUpdate)))
        {
            /// @todo (CAMX-1813) Sensor need to send full res width and height too
            if (FALSE == Utils::FEqual(pInputData->sensorData.sensorBinningFactor, 0.0f))
            {
                m_pState->dependenceData.fullResWidth  = static_cast<UINT32>(
                    pInputData->sensorData.fullResolutionWidth / pInputData->sensorData.sensorBinningFactor);
                m_pState->dependenceData.fullResHeight = static_cast<UINT32>(
                    pInputData->sensorData.fullResolutionHeight / pInputData->sensorData.sensorBinningFactor);
            }
            else
            {
                m_pState->dependenceData.fullResWidth  = pInputData->sensorData.fullResolutionWidth;
                m_pState->dependenceData.fullResHeight = pInputData->sensorData.fullResolutionHeight;
                m_pState->dependenceData.scalingFactor = static_cast<UINT32>(pInputData->sensorData.sensorScalingFactor);
            }

            m_pState->dependenceData.offsetX           = pInputData->sensorData.sensorOut.offsetX;
            m_pState->dependenceData.offsetY           = pInputData->sensorData.sensorOut.offsetY;
            m_pState->dependenceData.imageWidth        = pInputData->sensorData.sensorOut.width;
            m_pState->dependenceData.imageHeight       = pInputData->sensorData.sensorOut.height;
            m_pState->dependenceData.scalingFactor     = static_cast<UINT32>(pInputData->sensorData.sensorScalingFactor);

            isChanged = TRUE;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP,
                       "Invalid Input: pAECUpdateData %p  pHwContext %p pNewAWBUpdate %p",
                       pInputData->pAECUpdateData,
                       pInputData->pHwContext,
                       pInputData->pAWBUpdateData);
    }

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

    m_tintlessConfig.tintlessParamConfig.applyTemporalFiltering = 1;

    m_pTintlessBGStats = pInputData->pStripeConfig->statsDataForISP.pParsedTintlessBGStats;

    if ((TRUE == m_moduleEnable) &&
        (NULL != pInputData->pHALTagsData) &&
        (ShadingModeOff == pInputData->pHALTagsData->shadingMode))
    {
        m_moduleEnable = FALSE;
        isChanged      = FALSE;
    }

    if ((TRUE == m_pState->dependenceData.enableTintless) &&
        (TRUE == m_moduleEnable))
    {
        if (TRUE == pInputData->skipTintlessProcessing)
        {
            isChanged = FALSE;
        }
    }

    if ((NULL != pInputData->pHALTagsData) &&
        pInputData->pHALTagsData->colorCorrectionMode == ColorCorrectionModeTransformMatrix &&
        ((pInputData->pHALTagsData->controlAWBMode    == ControlAWBModeOff &&
          pInputData->pHALTagsData->controlMode       == ControlModeAuto) ||
          pInputData->pHALTagsData->controlMode       == ControlModeOff))
    {
        isChanged = FALSE;
    }

    if ((NULL != pInputData->pHALTagsData) &&
        (ControlAWBLockOn == pInputData->pHALTagsData->controlAWBLock) &&
        (ControlAELockOn  == pInputData->pHALTagsData->controlAECLock))
    {
        isChanged = FALSE;
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELSC34::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    if (NULL != pInputData)
    {
        m_pState = &pInputData->pStripeConfig->stateLSC;
        if (TRUE == CheckDependenceChange(pInputData))
        {
            m_pState->dependenceData.fetchSettingOnly = TRUE;
            m_unpackedData.bIsDataReady               = FALSE;

            result = RunCalculation(pInputData);

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("LSC module calculation Failed.");
            }

            m_pState->dependenceData.fetchSettingOnly = FALSE;
            m_unpackedData.bIsDataReady               = TRUE;
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data");
        result = CamxResultEInvalidArg;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELSC34::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result                = CamxResultSuccess;
    CmdBuffer* pCmdBuffer            = NULL;
    UINT8      lRolloffGRRBankSelect = RolloffLGRRBank0;
    UINT8      lRolloffGBBBankSelect = RolloffLGBBBank0;
    UINT32     offset                =
        (m_32bitDMIBufferOffsetDword + (pInputData->pStripeConfig->stripeId * m_32bitDMILength)) * sizeof(UINT32);
    CmdBuffer* pDMIBuffer            = NULL;
    UINT32     lengthInByte          = IFELSC34DMISetSizeDword * sizeof(UINT32);

    pCmdBuffer = pInputData->pCmdBuffer;
    pDMIBuffer = pInputData->p32bitDMIBuffer;

    CAMX_ASSERT(NULL != pCmdBuffer);
    CAMX_ASSERT(NULL != pDMIBuffer);

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_ROLLOFF_CFG,
                                          IFELSC34RegLengthDword,
                                          reinterpret_cast<UINT32*>(&m_regCmd));

    CAMX_ASSERT(CamxResultSuccess == result);

    lRolloffGRRBankSelect =
        (m_regCmd.configRegister.bitfields.PCA_LUT_BANK_SEL == 0) ? RolloffLGRRBank0 : RolloffLGRRBank1;
    lRolloffGBBBankSelect =
        (m_regCmd.configRegister.bitfields.PCA_LUT_BANK_SEL == 0) ? RolloffLGBBBank0 : RolloffLGBBBank1;

    result = PacketBuilder::WriteDMI(pCmdBuffer,
                                     regIFE_IFE_0_VFE_DMI_CFG,
                                     lRolloffGRRBankSelect,
                                     pDMIBuffer,
                                     offset,
                                     lengthInByte);

    CAMX_ASSERT(CamxResultSuccess == result);

    offset += lengthInByte;

    result = PacketBuilder::WriteDMI(pCmdBuffer,
                                     regIFE_IFE_0_VFE_DMI_CFG,
                                     lRolloffGBBBankSelect,
                                     pDMIBuffer,
                                     offset,
                                     lengthInByte);

    CAMX_ASSERT(CamxResultSuccess == result);
    m_pState->dependenceData.bankSelect ^= 1;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFELSC34::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult      result         = CamxResultSuccess;
    LSC34OutputData outputData;
    UINT32*         pLSCDMIAddr    = reinterpret_cast<UINT32*>(pInputData->p32bitDMIBufferAddr +
                                                               m_32bitDMIBufferOffsetDword +
                                                               (pInputData->pStripeConfig->stripeId * m_32bitDMILength));

    outputData.IFE.pRegIFECmd     = &m_regCmd;
    outputData.pUnpackedField     = &m_unpackedData.unpackedData;
    outputData.type               = PipelineType::IFE;
    outputData.pGRRLUTDMIBuffer   = pLSCDMIAddr;
    outputData.pGBBLUTDMIBuffer   = reinterpret_cast<UINT32*>((reinterpret_cast<UCHAR*>(outputData.pGRRLUTDMIBuffer) +
                                                              IFELSC34LUTTableSize));
    if (TRUE == m_unpackedData.bIsDataReady)
    {
        m_unpackedData.unpackedData.lx_start_l = m_pState->dependenceData.stripeOut.lx_start_l;
        m_unpackedData.unpackedData.bx_d1_l    = m_pState->dependenceData.stripeOut.bx_d1_l;
        m_unpackedData.unpackedData.bx_start_l = m_pState->dependenceData.stripeOut.bx_start_l;
        m_unpackedData.unpackedData.bank_sel   = m_pState->dependenceData.bankSelect;
        IQInterface::IFESetupLSC34Reg(pInputData, &m_unpackedData.unpackedData, &outputData);
    }
    else
    {
        if ((pInputData->pHALTagsData->controlAWBLock == m_AWBLock)    &&
            (pInputData->pHALTagsData->controlAWBLock  == ControlAWBLockOn))
        {
            m_pState->dependenceData.enableTintless = FALSE;
        }
        else
        {
            m_AWBLock = pInputData->pHALTagsData->controlAWBLock;
        }

        if (TRUE == m_pState->dependenceData.enableTintless)
        {
            m_pState->dependenceData.pTintlessAlgo   = m_pTintlessAlgo;
            m_pState->dependenceData.pTintlessConfig = &m_tintlessConfig;
            m_pState->dependenceData.pTintlessStats  = m_pTintlessBGStats;
        }
        else
        {
            m_pState->dependenceData.pTintlessAlgo   = NULL;
            m_pState->dependenceData.pTintlessConfig = NULL;
            m_pState->dependenceData.pTintlessStats  = NULL;
        }

        m_pState->dependenceData.pTintlessRatio = &m_tintlessRatio;

        result = IQInterface::LSC34CalculateSetting(&m_pState->dependenceData,
                                                    pInputData->pOEMIQSetting,
                                                    pInputData,
                                                    &outputData);
        if (NULL != pInputData->pStripingInput)
        {
            pInputData->pStripingInput->enableBits.rolloff                          = m_moduleEnable;
            pInputData->pStripingInput->stripingInput.rollOffParam.Bwidth_l         = outputData.lscState.bwidth_l;
            pInputData->pStripingInput->stripingInput.rollOffParam.Bx_d1_l          = outputData.lscState.bx_d1_l;
            pInputData->pStripingInput->stripingInput.rollOffParam.Bx_start_l       = outputData.lscState.bx_start_l;
            pInputData->pStripingInput->stripingInput.rollOffParam.Lx_start_l       = outputData.lscState.lx_start_l;
            pInputData->pStripingInput->stripingInput.rollOffParam.MeshGridBwidth_l = outputData.lscState.meshGridBwidth_l;
            pInputData->pStripingInput->stripingInput.rollOffParam.num_meshgain_h   = outputData.lscState.num_meshgain_h;
        }
    }

    if (CamxResultSuccess == result)
    {

        if (TRUE == m_pState->dependenceData.toCalibrate)
        {
            m_pState->dependenceData.toCalibrate = FALSE;
        }

        if (NULL != pInputData->pIFETuningMetadata)
        {
            m_pGRRLUTDMIBuffer = outputData.pGRRLUTDMIBuffer;
            m_pGBBLUTDMIBuffer = outputData.pGBBLUTDMIBuffer;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "LSC Calculation Failed.");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::IFELSC34
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFELSC34::IFELSC34()
{
    m_type                  = ISPIQModuleType::IFELSC;
    m_32bitDMILength        = IFELSC34DMILengthDword;
    m_64bitDMILength        = 0;
    m_cmdLength             = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFELSC34RegLengthDword);
    m_pChromatix            = NULL;
    m_pGRRLUTDMIBuffer      = NULL;
    m_pGBBLUTDMIBuffer      = NULL;
    m_AWBLock               = ControlAWBLockOff;
    m_shadingMode           = ShadingModeFast;
    m_lensShadingMapMode    = StatisticsLensShadingMapModeOff;
    m_pInterpolationData    = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFELSC34::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    if (NULL != pInputData->pCalculatedData)
    {
        pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bits.ROLLOFF_EN = m_moduleEnable;

        if ((ShadingModeOff == m_shadingMode || m_moduleEnable == FALSE) &&
            (StatisticsLensShadingMapModeOn == m_lensShadingMapMode))
        {
            // Pass the unity matrix for shading mode is OFF.
            for (UINT16 i = 0; i < (TotalChannels * IFERolloffMeshPtVV34 * IFERolloffMeshPtHV34); i++)
            {
                pInputData->pCalculatedData->lensShadingInfo.lensShadingMap[i] = 1.0f;
            }
        }
        else if (StatisticsLensShadingMapModeOn == pInputData->pHALTagsData->statisticsLensShadingMapMode)
        {
            IQInterface::CopyLSCMapData(pInputData, &m_unpackedData.unpackedData);
        }

        pInputData->pCalculatedData->lensShadingInfo.lensShadingMapSize.width  = IFERolloffMeshPtHV34;
        pInputData->pCalculatedData->lensShadingInfo.lensShadingMapSize.height = IFERolloffMeshPtVV34;
        pInputData->pCalculatedData->lensShadingInfo.lensShadingMapMode        = m_lensShadingMapMode;
        pInputData->pCalculatedData->lensShadingInfo.shadingMode               = m_shadingMode;

        // Post tuning metadata if setting is enabled
        if (NULL != pInputData->pIFETuningMetadata)
        {
            if (NULL != m_pGRRLUTDMIBuffer)
            {
                Utils::Memcpy(&pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.LSCMesh.GRRLUT[0],
                              m_pGRRLUTDMIBuffer,
                              IFELSC34LUTTableSize);
            }
            if (NULL != m_pGBBLUTDMIBuffer)
            {
                Utils::Memcpy(&pInputData->pIFETuningMetadata->IFEDMIData.packedLUT.LSCMesh.GBBLUT[0],
                              m_pGBBLUTDMIBuffer,
                              IFELSC34LUTTableSize);
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Update Metadata failed");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFELSC34::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFELSC34::DeallocateCommonLibraryData()
{
    if (NULL != m_pInterpolationData)
    {
        CAMX_FREE(m_pInterpolationData);
        m_pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFELSC34::~IFELSC34
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFELSC34::~IFELSC34()
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

    if (NULL != m_pCalibrationTable)
    {
        CAMX_FREE(m_pCalibrationTable);
        m_pCalibrationTable = NULL;
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

    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
