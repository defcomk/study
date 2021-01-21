////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifecc12.cpp
/// @brief CAMXIFECC12 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifecc12.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFECC12RegLengthDWord = sizeof(IFECC12RegCmd) / sizeof(UINT32);

CAMX_STATIC_ASSERT((13 * 4) == sizeof(IFECC12RegCmd));

static const UINT32 CC12QFactor = 7;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECC12::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFECC12* pModule = CAMX_NEW IFECC12;

        if (NULL != pModule)
        {
            result = pModule->Initialize();
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Module initialization failed");
                pModule->Destroy();
                pModule = NULL;
            }
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFECC12 object.");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFECC12 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECC12::Initialize()
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
// IFECC12::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECC12::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            result = RunCalculation(pInputData);
            if (CamxResultSuccess == result)
            {
                result = CreateCmdList(pInputData);
            }
            else
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("CC module calculation Failed.");
            }
        }
        if (CamxResultSuccess == result)
        {
            UpdateIFEInternalData(pInputData);
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECC12::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    UINT interpolationSize = (sizeof(cc_1_2_0::cc12_rgn_dataType) * (CC12MaxmiumNonLeafNode + 1));

    if (NULL == m_dependenceData.pInterpolationData)
    {
        // Alloc for cc_1_2_0::cc12_rgn_dataType
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFECC12::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    INT32 ccQFactor = 1;

    if (NULL != pInputData->pCalculatedData)
    {
        pInputData->pCalculatedData->moduleEnable.colorProcessingModuleConfig.bitfields.COLOR_CORRECT_EN = m_moduleEnable;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFECC12::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    CAMX_ASSERT(NULL != pInputData);

    if ((NULL != pInputData->pHALTagsData)   &&
        (NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData))
    {
        ISPHALTagsData* pHALTagsData = pInputData->pHALTagsData;

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->CCEnable;

            if (TRUE == m_moduleEnable)
            {
                isChanged = TRUE;
            }
        }
        else
        {
            TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
            CAMX_ASSERT(NULL != pTuningManager);

            // Search through the tuning data (tree), only when there
            // are changes to the tuning mode data as an optimization
            if ((TRUE == pInputData->tuningModeChanged)    &&
                (TRUE == pTuningManager->IsValidChromatix()))
            {
                CAMX_ASSERT(NULL != pInputData->pTuningData);

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_cc12_ife(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);

            if (NULL != m_pChromatix)
            {
                if ((NULL == m_dependenceData.pChromatix)                                       ||
                    (m_pChromatix->SymbolTableID != m_dependenceData.pChromatix->SymbolTableID) ||
                    (m_moduleEnable              != m_pChromatix->enable_section.cc_enable))
                {
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.cc_enable;
                    if (TRUE == m_moduleEnable)
                    {
                        isChanged = TRUE;
                    }
                }
            }
            // Check for manual tone map mode, if yes disable 2dLUT
            if ((TRUE                               == m_moduleEnable)                    &&
                (ColorCorrectionModeTransformMatrix == pHALTagsData->colorCorrectionMode) &&
                (((ControlAWBModeOff                == pHALTagsData->controlAWBMode)      &&
                (ControlModeAuto                    == pHALTagsData->controlMode)) ||
                (ControlModeOff                     == pHALTagsData->controlMode)))
            {
                m_moduleEnable = FALSE;
                isChanged      = FALSE;
                CAMX_LOG_INFO(CamxLogGroupISP, "<CCM> manual m_moduleEnable %d", m_moduleEnable);
            }
        }

        if (TRUE == m_moduleEnable)
        {
            if ((TRUE ==
                IQInterface::s_interpolationTable.IFECC12TriggerUpdate(&pInputData->triggerData, &m_dependenceData)) ||
                (TRUE == pInputData->forceTriggerUpdate))
            {
                isChanged = TRUE;
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Input: pNewAECUpdate %x pNewAWBUpdate %x  HwContext %x",
            pInputData->pHwContext, pInputData->pAECUpdateData, pInputData->pAWBUpdateData, pInputData->pHwContext);
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECC12::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = NULL;

    if (NULL != pInputData->pCmdBuffer)
    {
        pCmdBuffer = pInputData->pCmdBuffer;
        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_COLOR_CORRECT_COEFF_0,
                                              IFECC12RegLengthDWord,
                                              reinterpret_cast<UINT32*>(&m_regCmd));
        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input data or command buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::UpdateAWBCCM
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFECC12::UpdateAWBCCM()
{
    UINT32 qFactor = CC12QFactor + m_dependenceData.pChromatix->chromatix_cc12_reserve.q_factor;
    // Consume CCM from AWB algo

    // Chromatix stores the coeffs in RGB order whereas VFE stores the coeffs in GBR order. Hence c0 maps to M[1][1]
    m_regCmd.coefficientRegister0.bitfields.CN = Utils::FloatToQNumber(m_AWBCCM.CCM[1][1], (1 << qFactor));
    m_regCmd.coefficientRegister1.bitfields.CN = Utils::FloatToQNumber(m_AWBCCM.CCM[1][2], (1 << qFactor));
    m_regCmd.coefficientRegister2.bitfields.CN = Utils::FloatToQNumber(m_AWBCCM.CCM[1][0], (1 << qFactor));
    m_regCmd.coefficientRegister3.bitfields.CN = Utils::FloatToQNumber(m_AWBCCM.CCM[2][1], (1 << qFactor));
    m_regCmd.coefficientRegister4.bitfields.CN = Utils::FloatToQNumber(m_AWBCCM.CCM[2][2], (1 << qFactor));
    m_regCmd.coefficientRegister5.bitfields.CN = Utils::FloatToQNumber(m_AWBCCM.CCM[2][0], (1 << qFactor));
    m_regCmd.coefficientRegister6.bitfields.CN = Utils::FloatToQNumber(m_AWBCCM.CCM[0][1], (1 << qFactor));
    m_regCmd.coefficientRegister7.bitfields.CN = Utils::FloatToQNumber(m_AWBCCM.CCM[0][2], (1 << qFactor));
    m_regCmd.coefficientRegister8.bitfields.CN = Utils::FloatToQNumber(m_AWBCCM.CCM[0][0], (1 << qFactor));

    m_regCmd.offsetRegister0.bitfields.KN = Utils::RoundFLOAT(m_AWBCCM.CCMOffset[1]);
    m_regCmd.offsetRegister1.bitfields.KN = Utils::RoundFLOAT(m_AWBCCM.CCMOffset[2]);
    m_regCmd.offsetRegister2.bitfields.KN = Utils::RoundFLOAT(m_AWBCCM.CCMOffset[0]);

    m_regCmd.coefficientQRegister.bitfields.QFACTOR = m_dependenceData.pChromatix->chromatix_cc12_reserve.q_factor;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECC12::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CC12OutputData outputData;

    if ((TRUE  == m_AWBCCM.isCCMOverrideEnabled) &&
        (FALSE == pInputData->disableManual3ACCM))
    {
        UpdateAWBCCM();
    }
    else
    {

        outputData.pRegCmd = &m_regCmd;

        result = IQInterface::IFECC12CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "CC12 Calculation Failed.");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFECC12::DeallocateCommonLibraryData()
{
    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECC12::IFECC12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFECC12::IFECC12()
{
    m_type           = ISPIQModuleType::IFECC;
    m_cmdLength      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFECC12RegLengthDWord);
    m_32bitDMILength = 0;
    m_64bitDMILength = 0;
    m_moduleEnable   = TRUE;
    m_pChromatix     = NULL;

    m_dependenceData.pChromatix       = NULL;
    m_dependenceData.DRCGain          = 0.0f;
    m_dependenceData.exposureTime     = 0.0f;
    m_dependenceData.AECSensitivity   = 0.0f;
    m_dependenceData.luxIndex         = 0.0f;
    m_dependenceData.AECGain          = 0.0f;
    m_dependenceData.colorTemperature = 0.0f;

    /// @todo (CAMX-677) This hardcode value is to support initial presil test. Will replace with memset to 0
    m_regCmd.coefficientRegister0.u32All = 0x80;
    m_regCmd.coefficientRegister1.u32All = 0x0;
    m_regCmd.coefficientRegister2.u32All = 0x0;
    m_regCmd.coefficientRegister3.u32All = 0x0;
    m_regCmd.coefficientRegister4.u32All = 0x80;
    m_regCmd.coefficientRegister5.u32All = 0x0;
    m_regCmd.coefficientRegister6.u32All = 0x0;
    m_regCmd.coefficientRegister7.u32All = 0x0;
    m_regCmd.coefficientRegister8.u32All = 0x80;
    m_regCmd.offsetRegister0.u32All      = 0x0;
    m_regCmd.offsetRegister1.u32All      = 0x0;
    m_regCmd.offsetRegister2.u32All      = 0x0;
    m_regCmd.coefficientQRegister.u32All = 0x0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFECC12::~IFECC12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFECC12::~IFECC12()
{
    m_pChromatix = NULL;
    DeallocateCommonLibraryData();
}

CAMX_NAMESPACE_END
