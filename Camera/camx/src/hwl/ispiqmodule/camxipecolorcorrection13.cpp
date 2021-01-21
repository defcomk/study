////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipecolorcorrection13.cpp
/// @brief IPEColorCorrection class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxipecolorcorrection13.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "ipe_data.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorCorrection13::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPEColorCorrection13* pModule = CAMX_NEW IPEColorCorrection13(pCreateData->pNodeIdentifier);

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Module initialization failed !!");
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
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorCorrection13::Initialize()
{
    CamxResult result = CamxResultSuccess;
    result = AllocateCommonLibraryData();
    if (result != CamxResultSuccess)
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Unable to initilize common library data, no memory");
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorCorrection13::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(cc_1_3_0::cc13_rgn_dataType) * (CC13MaxNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for cc_1_3_0::cc13_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorCorrection13::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    CamxResult      result          = CamxResultSuccess;
    IpeIQSettings*  pIPEIQSettings  = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

    pIPEIQSettings->colorCorrectParameters.moduleCfg.EN = m_moduleEnable;
    /// @todo (CAMX-738) Add metadata information

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pIPETuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(IPECC13RegConfig) <=
                           sizeof(pInputData->pIPETuningMetadata->IPECCData.colorCorrectionConfig));

        Utils::Memcpy(&pInputData->pIPETuningMetadata->IPECCData.colorCorrectionConfig,
                      &m_regCmd,
                      sizeof(IPECC13RegConfig));
    }

    if (pInputData->pCalculatedData)
    {
        pInputData->pCalculatedData->CCTransformMatrix[0][0].numerator   = Utils::RoundFLOAT(static_cast<FLOAT>(
           Utils::SignExtendQnumber(m_regCmd.redCoefficients1.bitfields.MATRIX_C2, 12)));
        pInputData->pCalculatedData->CCTransformMatrix[0][0].denominator = Q7;
        pInputData->pCalculatedData->CCTransformMatrix[0][1].numerator   = Utils::RoundFLOAT(static_cast<FLOAT>(
           Utils::SignExtendQnumber(m_regCmd.greenCoefficients1.bitfields.MATRIX_A2, 12)));
        pInputData->pCalculatedData->CCTransformMatrix[0][1].denominator = Q7;
        pInputData->pCalculatedData->CCTransformMatrix[0][2].numerator   = Utils::RoundFLOAT(static_cast<FLOAT>(
           Utils::SignExtendQnumber(m_regCmd.blueCoefficients1.bitfields.MATRIX_B2, 12)));
        pInputData->pCalculatedData->CCTransformMatrix[0][2].denominator = Q7;

        pInputData->pCalculatedData->CCTransformMatrix[1][0].numerator   = Utils::RoundFLOAT(static_cast<FLOAT>(
           Utils::SignExtendQnumber(m_regCmd.redCoefficients0.bitfields.MATRIX_C0, 12)));
        pInputData->pCalculatedData->CCTransformMatrix[1][0].denominator = Q7;
        pInputData->pCalculatedData->CCTransformMatrix[1][1].numerator   = Utils::RoundFLOAT(static_cast<FLOAT>(
           Utils::SignExtendQnumber(m_regCmd.greenCoefficients0.bitfields.MATRIX_A0, 12)));
        pInputData->pCalculatedData->CCTransformMatrix[1][1].denominator = Q7;
        pInputData->pCalculatedData->CCTransformMatrix[1][2].numerator   = Utils::RoundFLOAT(static_cast<FLOAT>(
           Utils::SignExtendQnumber(m_regCmd.blueCoefficients0.bitfields.MATRIX_B0, 12)));
        pInputData->pCalculatedData->CCTransformMatrix[1][2].denominator = Q7;

        pInputData->pCalculatedData->CCTransformMatrix[2][0].numerator   = Utils::RoundFLOAT(static_cast<FLOAT>(
           Utils::SignExtendQnumber(m_regCmd.redCoefficients0.bitfields.MATRIX_C1, 12)));
        pInputData->pCalculatedData->CCTransformMatrix[2][0].denominator = Q7;
        pInputData->pCalculatedData->CCTransformMatrix[2][1].numerator   = Utils::RoundFLOAT(static_cast<FLOAT>(
           Utils::SignExtendQnumber(m_regCmd.greenCoefficients0.bitfields.MATRIX_A1, 12)));
        pInputData->pCalculatedData->CCTransformMatrix[2][1].denominator = Q7;
        pInputData->pCalculatedData->CCTransformMatrix[2][2].numerator   = Utils::RoundFLOAT(static_cast<FLOAT>(
           Utils::SignExtendQnumber(m_regCmd.blueCoefficients0.bitfields.MATRIX_B1, 12)));
        pInputData->pCalculatedData->CCTransformMatrix[2][2].denominator = Q7;

        pInputData->pCalculatedData->colorCorrectionMode = m_colorCorrectionMode;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::PopulateEffectMatrix
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID PopulateEffectMatrix(FLOAT matrix[3][3], FLOAT saturation)
{
    matrix[0][0] = static_cast<FLOAT>(0.2990 + 1.4075 * 0.498 * saturation);
    matrix[0][1] = static_cast<FLOAT>(0.5870 - 1.4075 * 0.417 * saturation);
    matrix[0][2] = static_cast<FLOAT>(0.1140 - 1.4075 * 0.081 * saturation);
    matrix[1][0] = static_cast<FLOAT>(0.2990 + 0.3455 * 0.168 * saturation - 0.7169 * 0.498 * saturation);
    matrix[1][1] = static_cast<FLOAT>(0.5870 + 0.3455 * 0.330 * saturation + 0.7169 * 0.417 * saturation);
    matrix[1][2] = static_cast<FLOAT>(0.1140 - 0.3455 * 0.498 * saturation + 0.7169 * 0.081 * saturation);
    matrix[2][0] = static_cast<FLOAT>(0.2990 - 1.7790 * 0.168 * saturation);
    matrix[2][1] = static_cast<FLOAT>(0.5870 - 1.7790 * 0.330 * saturation);
    matrix[2][2] = static_cast<FLOAT>(0.1140 + 1.7790 * 0.498 * saturation);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorCorrection13::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPostLTM];

    if (NULL != pCmdBuffer)
    {
        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIPE_IPE_0_PPS_CLC_COLOR_CORRECT_COLOR_CORRECT_COEFF_A_CFG_0,
                                              IPEColorCorrectionRegLength,
                                              reinterpret_cast<UINT32*>(&m_regCmd));
        CAMX_ASSERT(CamxResultSuccess == result);

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write Color Correction coefficient config. data");
        }
        else
        {
            m_validCCM = TRUE;
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorCorrection13::ValidateDependenceParams(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-730) validate dependency parameters
    if ((NULL == pInputData)                                                   ||
        (NULL == pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferPostLTM]) ||
        (NULL == pInputData->pipelineIPEData.pIPEIQSettings))
    {
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IPEColorCorrection13::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPEColorCorrection13::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                  &&
        (NULL != pInputData->pAECUpdateData)  &&
        (NULL != pInputData->pAWBUpdateData)  &&
        (NULL != pInputData->pHwContext)      &&
        (NULL != pInputData->pTuningData)     &&
        (NULL != pInputData->pHALTagsData))
    {
        m_manualCCMOverride = FALSE;
        ISPHALTagsData* pHALTagsData = pInputData->pHALTagsData;

        if ((ControlAWBLockOn == pHALTagsData->controlAWBLock) && (TRUE == m_validCCM))
        {
            CAMX_LOG_INFO(CamxLogGroupPProc, "AWBLock enabled");
        }
               // Check for the manual update
        else if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->CCEnable;
            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            m_colorCorrectionMode = pHALTagsData->colorCorrectionMode;
            CAMX_LOG_INFO(CamxLogGroupPProc, "m_colorCorrectionMode %d ", m_colorCorrectionMode);
                   // Check for the manual update
            if ((ColorCorrectionModeTransformMatrix == pHALTagsData->colorCorrectionMode) &&
               (((ControlAWBModeOff                 == pHALTagsData->controlAWBMode)      &&
                (ControlModeAuto                    == pHALTagsData->controlMode))        ||
                (ControlModeOff                     == pHALTagsData->controlMode)))
            {
                m_manualCCMOverride = TRUE;
                isChanged           = TRUE;
                m_moduleEnable      = TRUE;

                // Translate the transform matrix
                ISPColorCorrectionTransform* pCCMatrix = &pHALTagsData->colorCorrectionTransform;
                for (UINT32 i = 0; i < 3; i++ )
                {
                    m_manualCCM[i][0] = static_cast<FLOAT>(pCCMatrix->transformMatrix[i][0].numerator) /
                        static_cast<FLOAT>(pCCMatrix->transformMatrix[i][0].denominator);

                    m_manualCCM[i][1] = static_cast<FLOAT>(pCCMatrix->transformMatrix[i][1].numerator) /
                        static_cast<FLOAT>(pCCMatrix->transformMatrix[i][1].denominator);

                    m_manualCCM[i][2] = static_cast<FLOAT>(pCCMatrix->transformMatrix[i][2].numerator) /
                        static_cast<FLOAT>(pCCMatrix->transformMatrix[i][2].denominator);
                }

                CAMX_LOG_VERBOSE(CamxLogGroupPProc, "APP CCM [%f, %f, %f, %f, %f, %f, %f, %f, %f]",
                     m_manualCCM[0][0],
                     m_manualCCM[0][1],
                     m_manualCCM[0][2],
                     m_manualCCM[1][0],
                     m_manualCCM[1][1],
                     m_manualCCM[1][2],
                     m_manualCCM[2][0],
                     m_manualCCM[2][1],
                     m_manualCCM[2][2]);
            }
            else
            {
                AECFrameControl*   pNewAECUpdate  = pInputData->pAECUpdateData;
                AWBFrameControl*   pNewAWBUpdate  = pInputData->pAWBUpdateData;
                TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
                CAMX_ASSERT(NULL != pTuningManager);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if ((TRUE == pInputData->tuningModeChanged)    &&
                    (TRUE == pTuningManager->IsValidChromatix()))
                {
                    CAMX_ASSERT(NULL != pInputData->pTuningData);

                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_cc13_ipe(
                                       reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                       pInputData->pTuningData->noOfSelectionParameter);
                }

                CAMX_ASSERT(NULL != m_pChromatix);
                if (NULL != m_pChromatix)
                {
                    if ((m_pChromatix   != m_dependenceData.pChromatix)          ||
                        (m_moduleEnable != m_pChromatix->enable_section.cc_enable))
                    {
                        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "updating chromatix pointer");
                        m_dependenceData.pChromatix = m_pChromatix;
                        m_moduleEnable              = m_pChromatix->enable_section.cc_enable;

                        isChanged = (TRUE == m_moduleEnable);
                    }

                    if (TRUE == m_moduleEnable)
                    {

                        m_numValidCCMs = pNewAWBUpdate->numValidCCMs;
                        if (m_numValidCCMs > MaxCCMs - 1)
                        {
                            CAMX_ASSERT_ALWAYS_MESSAGE("Invalid CCM value %d", m_numValidCCMs);
                            m_numValidCCMs = MaxCCMs - 1;
                        }
                        for (UINT i = 0; i < m_numValidCCMs; i++)
                        {
                            m_AWBCCM[i] = pNewAWBUpdate->AWBCCM[i];
                            if (TRUE == m_AWBCCM[i].isCCMOverrideEnabled)
                            {
                                isChanged = TRUE;
                            }
                        }

                        if (TRUE ==
                            IQInterface::s_interpolationTable.CC13TriggerUpdate(&pInputData->triggerData, &m_dependenceData))
                        {
                            isChanged = TRUE;
                        }
                    }
                    else
                    {
                        CAMX_LOG_INFO(CamxLogGroupPProc, "CC13 Module is not enabled");
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to get Chromatix");
                }
            }
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc,
                           "Invalid Input: pNewAECUpdate %p pNewAWBUpdate %p pHwContext %p",
                           pInputData->pAECUpdateData,
                           pInputData->pAWBUpdateData,
                           pInputData->pHwContext);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::UpdateManualCCM
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEColorCorrection13::UpdateManualCCM()
{
    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "m_colorCorrectionMode %d ", m_colorCorrectionMode);


    m_regCmd.greenCoefficients0.bitfields.MATRIX_A0 = Utils::FloatToQNumber(m_manualCCM[1][1], Q7);
    m_regCmd.greenCoefficients0.bitfields.MATRIX_A1 = Utils::FloatToQNumber(m_manualCCM[2][1], Q7);
    m_regCmd.greenCoefficients1.bitfields.MATRIX_A2 = Utils::FloatToQNumber(m_manualCCM[0][1], Q7);

    m_regCmd.blueCoefficients0.bitfields.MATRIX_B0  = Utils::FloatToQNumber(m_manualCCM[1][2], Q7);
    m_regCmd.blueCoefficients0.bitfields.MATRIX_B1  = Utils::FloatToQNumber(m_manualCCM[2][2], Q7);
    m_regCmd.blueCoefficients1.bitfields.MATRIX_B2  = Utils::FloatToQNumber(m_manualCCM[0][2], Q7);

    m_regCmd.redCoefficients0.bitfields.MATRIX_C0   = Utils::FloatToQNumber(m_manualCCM[1][0], Q7);
    m_regCmd.redCoefficients0.bitfields.MATRIX_C1   = Utils::FloatToQNumber(m_manualCCM[2][0], Q7);
    m_regCmd.redCoefficients1.bitfields.MATRIX_C2   = Utils::FloatToQNumber(m_manualCCM[0][0], Q7);

    m_regCmd.offsetParam0.bitfields.OFFSET_K0  = 0;
    m_regCmd.offsetParam0.bitfields.OFFSET_K1  = 0;
    m_regCmd.offsetParam1.bitfields.OFFSET_K2  = 0;
    m_regCmd.qFactor.bitfields.M_PARAM = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::UpdateAWBCCM
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEColorCorrection13::UpdateAWBCCM(
    AWBCCMParams* pAWBCCM)
{
    const static UINT32 CC13_Q_FACTOR = 7;
    UINT32 qFactor = CC13_Q_FACTOR + m_dependenceData.pChromatix->chromatix_cc13_reserve.q_factor;
    UINT32 mFactor = 1 << qFactor;
    // Consume CCM from AWB algo
    // Chromatix stores the coeffs in RGB order whereas VFE stores the coeffs in GBR order.Hence c0 maps to M[1][1]
    m_regCmd.greenCoefficients0.bitfields.MATRIX_A0 = Utils::RoundFLOAT((static_cast<FLOAT>(mFactor)) * pAWBCCM->CCM[1][1]);
    m_regCmd.greenCoefficients0.bitfields.MATRIX_A1 = Utils::RoundFLOAT((static_cast<FLOAT>(mFactor)) * pAWBCCM->CCM[2][1]);
    m_regCmd.greenCoefficients1.bitfields.MATRIX_A2 = Utils::RoundFLOAT((static_cast<FLOAT>(mFactor)) * pAWBCCM->CCM[0][1]);
    m_regCmd.blueCoefficients0.bitfields.MATRIX_B0  = Utils::RoundFLOAT((static_cast<FLOAT>(mFactor)) * pAWBCCM->CCM[1][2]);
    m_regCmd.blueCoefficients0.bitfields.MATRIX_B1  = Utils::RoundFLOAT((static_cast<FLOAT>(mFactor)) * pAWBCCM->CCM[2][2]);
    m_regCmd.blueCoefficients1.bitfields.MATRIX_B2  = Utils::RoundFLOAT((static_cast<FLOAT>(mFactor)) * pAWBCCM->CCM[0][2]);
    m_regCmd.redCoefficients0.bitfields.MATRIX_C0   = Utils::RoundFLOAT((static_cast<FLOAT>(mFactor)) * pAWBCCM->CCM[1][0]);
    m_regCmd.redCoefficients0.bitfields.MATRIX_C1   = Utils::RoundFLOAT((static_cast<FLOAT>(mFactor)) * pAWBCCM->CCM[2][0]);
    m_regCmd.redCoefficients1.bitfields.MATRIX_C2   = Utils::RoundFLOAT((static_cast<FLOAT>(mFactor)) * pAWBCCM->CCM[0][0]);
    m_regCmd.offsetParam0.bitfields.OFFSET_K0       = Utils::RoundFLOAT(pAWBCCM->CCMOffset[1]);
    m_regCmd.offsetParam0.bitfields.OFFSET_K1       = Utils::RoundFLOAT(pAWBCCM->CCMOffset[2]);
    m_regCmd.offsetParam1.bitfields.OFFSET_K2       = Utils::RoundFLOAT(pAWBCCM->CCMOffset[0]);
    m_regCmd.qFactor.bitfields.M_PARAM              = m_dependenceData.pChromatix->chromatix_cc13_reserve.q_factor;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorCorrection13::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult     result                        = CamxResultSuccess;
    CC13OutputData outputData                    = { PipelineType::IPE, { { 0 } } };
    FLOAT          saturation                    = 0.0f;
    const IPEInstanceProperty* pInstanceProperty = &pInputData->pipelineIPEData.instanceProperty;

    BOOL isCCMOverrideEnabled = FALSE;
    UINT indexCCM             = 0;
    if (1 == pInputData->pAWBUpdateData->numValidCCMs)
    {
        isCCMOverrideEnabled = m_AWBCCM[0].isCCMOverrideEnabled;
    }
    else
    {
        // 3 valid CCMs
        if (pInputData->parentNodeID == IFE)
        {
            if ((TRUE == pInputData->isHDR10On) &&
                (pInstanceProperty->processingType != IPEProcessingType::IPEProcessingPreview))
            {
                isCCMOverrideEnabled = m_AWBCCM[2].isCCMOverrideEnabled;
                indexCCM             = 2;
            }
            else
            {
                isCCMOverrideEnabled = m_AWBCCM[0].isCCMOverrideEnabled;
            }
        }
        else
        {
            isCCMOverrideEnabled = m_AWBCCM[1].isCCMOverrideEnabled;
            indexCCM             = 1;
        }
    }

    if (NULL != pInputData->pHALTagsData)
    {
        m_dependenceData.saturation = pInputData->pHALTagsData->saturation;
    }
    else
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "failed to get HAL Tags Data. Use default Saturation Value");
    }

    if (TRUE == m_manualCCMOverride)
    {
        UpdateManualCCM();
    }
    else if (TRUE == isCCMOverrideEnabled)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "isCCMOverrideEnabled is true");
        UpdateAWBCCM(&m_AWBCCM[indexCCM]);
    }
    else
    {
        if (DefaultSaturation != m_dependenceData.saturation)
        {
            saturation = (static_cast<FLOAT>(m_dependenceData.saturation) / 10.0f) * 2.0f;
            PopulateEffectMatrix(m_dependenceData.effectsMatrix, saturation);
        }

        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", m_dependenceData.luxIndex);

        outputData.regCommand.IPE.pRegCmd = &m_regCmd;

        result = IQInterface::CC13CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE CC13 Calculation Failed.");
        }
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorCorrection13::Execute(
    ISPInputData* pInputData)
{
    CamxResult result;

    if (NULL != pInputData)
    {
        // Check if dependency is published and valid
        result = ValidateDependenceParams(pInputData);

        if ((CamxResultSuccess == result))
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);
            }

            // Regardless of any update in dependency parameters, command buffers and IQSettings/Metadata shall be updated.
            if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
            {
                result = CreateCmdList(pInputData);
            }

            if (CamxResultSuccess == result)
            {
                result = UpdateIPEInternalData(pInputData);
            }

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Operation failed %d", result);
            }
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEColorCorrection13::DeallocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::IPEColorCorrection13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEColorCorrection13::IPEColorCorrection13(
    const CHAR* pNodeIdentifier)
{
    UINT cmdSize    = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IPECC13RegConfig) / RegisterWidthInBytes);

    m_pNodeIdentifier           = pNodeIdentifier;
    m_type                      = ISPIQModuleType::IPEColorCorrection;
    m_moduleEnable              = TRUE;
    m_cmdLength                 = cmdSize * sizeof(UINT32);
    m_numLUT                    = 0;
    m_offsetLUT                 = 0;
    m_pChromatix                = NULL;
    m_dependenceData.saturation = DefaultSaturation;
    m_validCCM                  = FALSE;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IPE Color Correction m_cmdLength %d ", m_cmdLength);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPEColorCorrection13::~IPEColorCorrection13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEColorCorrection13::~IPEColorCorrection13()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEColorCorrection13::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEColorCorrection13::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE Color Correction13 ********\n");
        CamX::OsUtils::FPrintF(pFile, "* Pedestal13 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "Green coeefficients0 = %x\n", m_regCmd.greenCoefficients0.bits);
        CamX::OsUtils::FPrintF(pFile, "Green coeefficients1 = %x\n", m_regCmd.greenCoefficients1.bits);
        CamX::OsUtils::FPrintF(pFile, "Blue  coeefficients0 = %x\n", m_regCmd.blueCoefficients0.bits);
        CamX::OsUtils::FPrintF(pFile, "Blue  coeefficients1 = %x\n", m_regCmd.blueCoefficients1.bits);
        CamX::OsUtils::FPrintF(pFile, "Red   coeefficients0 = %x\n", m_regCmd.redCoefficients0.bits);
        CamX::OsUtils::FPrintF(pFile, "Red   coeefficients1 = %x\n", m_regCmd.redCoefficients1.bits);
        CamX::OsUtils::FPrintF(pFile, "Offset Param0        = %x\n", m_regCmd.offsetParam0.bits);
        CamX::OsUtils::FPrintF(pFile, "Offset Param1        = %x\n", m_regCmd.offsetParam1.bits);
        CamX::OsUtils::FPrintF(pFile, "Q Factor             = %x\n", m_regCmd.qFactor.bits);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }

    return;
}

CAMX_NAMESPACE_END
