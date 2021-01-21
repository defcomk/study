////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifecst12.cpp
/// @brief CAMXIFECST12 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifecst12.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFECST12RegLengthDWord = sizeof(IFECST12RegCmd) / sizeof(UINT32);

CAMX_STATIC_ASSERT((12 * 4) == sizeof(IFECST12RegCmd));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECST12::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECST12::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFECST12;

        if (NULL == pCreateData->pModule)
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFECST12 object.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFECST12 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECST12::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECST12::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (FALSE == pInputData->useHardcodedRegisterValues)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);
                if (CamxResultSuccess == result)
                {
                    result = CreateCmdList(pInputData);
                }
            }
        }
        else
        {
            InitRegValues();

            result = CreateCmdList(pInputData);
        }

        if (CamxResultSuccess == result)
        {
            UpdateIFEInternalData(pInputData);
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
// IFECST12::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFECST12::GetRegCmd()
{
    return reinterpret_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECST12::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFECST12::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if (NULL != pInputData->pOEMIQSetting)
    {
        m_moduleEnable = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->CSTEnable;
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

            m_pChromatix = pTuningManager->GetChromatix()->GetModule_cst12_ife(
                               reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                               pInputData->pTuningData->noOfSelectionParameter);
        }

        CAMX_ASSERT(NULL != m_pChromatix);
        if (NULL != m_pChromatix)
        {
            if ((NULL == m_dependenceData.pChromatix) ||
                (m_pChromatix->SymbolTableID != m_dependenceData.pChromatix->SymbolTableID))
            {
                m_dependenceData.pChromatix = m_pChromatix;
                m_moduleEnable              = m_pChromatix->enable_section.cst_enable;
                if (TRUE == m_moduleEnable)
                {
                    isChanged = TRUE;
                }
            }
            if (TRUE == pInputData->forceTriggerUpdate)
            {
                isChanged = TRUE;
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get chromatix pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECST12::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECST12::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = NULL;

    if (NULL == pInputData->pCmdBuffer)
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input data or command buffer");
    }
    else
    {
        pCmdBuffer = pInputData->pCmdBuffer;
        result     = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_COLOR_XFORM_CH0_COEFF_CFG_0,
                                                  IFECST12RegLengthDWord,
                                                  reinterpret_cast<UINT32*>(&m_regCmd));
        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer. result = %d", result);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECST12::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFECST12::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult      result = CamxResultSuccess;
    CST12OutputData outputData;

    outputData.type                  = PipelineType::IFE;
    outputData.regCommand.pRegIFECmd = &m_regCmd;

    result = IQInterface::CST12CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "CST12 Calculation Failed. result %d", result);
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECST12::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFECST12::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    pInputData->pCalculatedData->moduleEnable.frameProcessingModuleConfig.bitfields.CST_EN = m_moduleEnable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECST12::IFECST12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFECST12::IFECST12()
{
    m_type                      = ISPIQModuleType::IFECST;
    m_cmdLength                 = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFECST12RegLengthDWord);
    m_32bitDMILength            = 0;
    m_64bitDMILength            = 0;
    m_moduleEnable              = TRUE;
    m_dependenceData.pChromatix = NULL;
    m_pChromatix                = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFECST12::~IFECST12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFECST12::~IFECST12()
{
    m_pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECST12::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFECST12::DumpRegConfig()
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch0ClampConfig           [0x%x]", m_regCmd.ch0ClampConfig.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch0Coefficient0          [0x%x]", m_regCmd.ch0Coefficient0.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch0Coefficient1          [0x%x]", m_regCmd.ch0Coefficient1.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch0OffsetConfig          [0x%x]", m_regCmd.ch0OffsetConfig.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch1ClampConfig           [0x%x]", m_regCmd.ch1ClampConfig.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch1CoefficientConfig0    [0x%x]", m_regCmd.ch1CoefficientConfig0.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch1CoefficientConfig1    [0x%x]", m_regCmd.ch1CoefficientConfig1.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch1OffsetConfig          [0x%x]", m_regCmd.ch1OffsetConfig.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch2ClampConfig           [0x%x]", m_regCmd.ch2ClampConfig.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch2CoefficientConfig0    [0x%x]", m_regCmd.ch2CoefficientConfig0.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch2CoefficientConfig1    [0x%x]", m_regCmd.ch2CoefficientConfig1.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "CST ch2OffsetConfig          [0x%x]", m_regCmd.ch2OffsetConfig.u32All);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFECST12::InitRegValues
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFECST12::InitRegValues()
{
    m_regCmd.ch0ClampConfig.u32All        = 0x03ff0000;
    m_regCmd.ch0Coefficient0.u32All       = 0x00750259;
    m_regCmd.ch0Coefficient1.u32All       = 0x00000132;
    m_regCmd.ch0OffsetConfig.u32All       = 0x00000000;
    m_regCmd.ch1ClampConfig.u32All        = 0x03ff0000;
    m_regCmd.ch1CoefficientConfig0.u32All = 0x01fe1eae;
    m_regCmd.ch1CoefficientConfig1.u32All = 0x00001f54;
    m_regCmd.ch1OffsetConfig.u32All       = 0x02000000;
    m_regCmd.ch2ClampConfig.u32All        = 0x03ff0000;
    m_regCmd.ch2CoefficientConfig0.u32All = 0x1fad1e55;
    m_regCmd.ch2CoefficientConfig1.u32All = 0x000001fe;
    m_regCmd.ch2OffsetConfig.u32All       = 0x02000000;
}

CAMX_NAMESPACE_END
