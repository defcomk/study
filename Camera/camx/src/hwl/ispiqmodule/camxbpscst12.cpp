
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 - 2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpscst12.cpp
/// @brief CAMXBPSCST12 class implementation
///        Color space transform (CST) as a standard 3x3 matrix multiplication to convert RGB to YUV or vice versa,
///        and Color conversion (CV) for converting RGB to YUV only with chroma enhancement capability.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxbpscst12.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 BPSCST12RegLengthDWord = sizeof(BPSCST12RegCmd) / sizeof(UINT32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCST12::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCST12::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        pCreateData->pModule = CAMX_NEW BPSCST12(pCreateData->pNodeIdentifier);
        if (NULL == pCreateData->pModule)
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCST12::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSCST12::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)           &&
        (NULL != pInputData->pHwContext))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->CSTEnable;

            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            if (NULL != pInputData->pTuningData)
            {
                TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
                CAMX_ASSERT(NULL != pTuningManager);

                // Search through the tuning data (tree), only when there
                // are changes to the tuning mode data as an optimization
                if ((TRUE == pInputData->tuningModeChanged)    &&
                    (TRUE == pTuningManager->IsValidChromatix()))
                {
                    m_pChromatix = pTuningManager->GetChromatix()->GetModule_cst12_bps(
                                       reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                       pInputData->pTuningData->noOfSelectionParameter);
                }

                CAMX_ASSERT(NULL != m_pChromatix);
                if (NULL != m_pChromatix)
                {
                    if (m_pChromatix != m_dependenceData.pChromatix)
                    {
                        m_dependenceData.pChromatix = m_pChromatix;
                        m_moduleEnable              = m_pChromatix->enable_section.cst_enable;

                        isChanged                   = (TRUE == m_moduleEnable);
                    }
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
                }
            }
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Input: pHwContext %p", pInputData->pHwContext);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCST12::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCST12::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_COLOR_XFORM_COLOR_XFORM_CH0_COEFF_CFG_0,
                                              BPSCST12RegLengthDWord,
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
// BPSCST12::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCST12::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        CST12OutputData outputData;

        outputData.type                  = PipelineType::BPS;
        outputData.regCommand.pRegBPSCmd = &m_regCmd;

        result = IQInterface::CST12CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "CST12 Calculation Failed. result %d", result);
        }
        if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
        {
            DumpRegConfig();
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
// BPSCST12::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSCST12::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    pBPSIQSettings->colorXformParameters.moduleCfg.EN = m_moduleEnable;

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(BPSCST12RegCmd) <=
                          sizeof(pInputData->pBPSTuningMetadata->BPSCSTData.CSTConfig));

        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSCSTData.CSTConfig, &m_regCmd, sizeof(BPSCST12RegCmd));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCST12::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSCST12::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == CheckDependenceChange(pInputData))
        {
            if (FALSE == pInputData->useHardcodedRegisterValues)
            {
                result = RunCalculation(pInputData);
            }
            else
            {
                InitRegValues();
            }
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
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCST12::BPSCST12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSCST12::BPSCST12(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier           = pNodeIdentifier;
    m_type                      = ISPIQModuleType::BPSCST;
    m_moduleEnable              = FALSE;
    m_pChromatix                = NULL;
    m_cmdLength                 = PacketBuilder::RequiredWriteRegRangeSizeInDwords(BPSCST12RegLengthDWord);
    m_32bitDMILength            = 0;
    m_64bitDMILength            = 0;
    m_dependenceData.pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCST12::~BPSCST12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSCST12::~BPSCST12()
{
    m_pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCST12::InitRegValues
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSCST12::InitRegValues()
{
    m_regCmd.channel0CoefficientConfig0.u32All = 0x00750259;
    m_regCmd.channel0CoefficientConfig1.u32All = 0x00000132;
    m_regCmd.channel0OffsetConfig.u32All       = 0x00000000;
    m_regCmd.channel0ClampConfig.u32All        = 0x03ff0000;
    m_regCmd.channel1CoefficientConfig0.u32All = 0x01fe1eae;
    m_regCmd.channel1CoefficientConfig1.u32All = 0x00001f54;
    m_regCmd.channel1OffsetConfig.u32All       = 0x02000000;
    m_regCmd.channel1ClampConfig.u32All        = 0x03ff0000;
    m_regCmd.channel2CoefficientConfig0.u32All = 0x1fad1e55;
    m_regCmd.channel2CoefficientConfig1.u32All = 0x000001fe;
    m_regCmd.channel2OffsetConfig.u32All       = 0x02000000;
    m_regCmd.channel2ClampConfig.u32All        = 0x03ff0000;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSCST12::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSCST12::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "********  BPS CST12 ********  \n");
        CamX::OsUtils::FPrintF(pFile, "channel0CoefficientConfig 0         = 0x%x\n", m_regCmd.channel0CoefficientConfig0);
        CamX::OsUtils::FPrintF(pFile, "channel0CoefficientConfig 1         = 0x%x\n", m_regCmd.channel0CoefficientConfig1);
        CamX::OsUtils::FPrintF(pFile, "channel0OffsetConfig                = 0x%x\n", m_regCmd.channel0OffsetConfig);
        CamX::OsUtils::FPrintF(pFile, "channel1CoefficientConfig 0         = 0x%x\n", m_regCmd.channel1CoefficientConfig0);
        CamX::OsUtils::FPrintF(pFile, "channel1CoefficientConfig 1         = 0x%x\n", m_regCmd.channel1CoefficientConfig1);
        CamX::OsUtils::FPrintF(pFile, "channel1OffsetConfig                = 0x%x\n", m_regCmd.channel1OffsetConfig);
        CamX::OsUtils::FPrintF(pFile, "channel1ClampConfig                 = 0x%x\n", m_regCmd.channel1ClampConfig);
        CamX::OsUtils::FPrintF(pFile, "channel2CoefficientConfig 0         = 0x%x\n", m_regCmd.channel2CoefficientConfig0);
        CamX::OsUtils::FPrintF(pFile, "channel2CoefficientConfig 1         = 0x%x\n", m_regCmd.channel2CoefficientConfig1);
        CamX::OsUtils::FPrintF(pFile, "channel2OffsetConfig                = 0x%x\n", m_regCmd.channel2OffsetConfig);
        CamX::OsUtils::FPrintF(pFile, "channel2ClampConfig                 = 0x%x\n", m_regCmd.channel2ClampConfig);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
}
CAMX_NAMESPACE_END
