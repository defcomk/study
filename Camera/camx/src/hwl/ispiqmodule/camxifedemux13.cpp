////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifedemux13.cpp
/// @brief CAMXIFEDEMUX13 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifedemux13.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFEDemux13RegLengthDWord = sizeof(IFEDemux13RegCmd) / sizeof(UINT32);

CAMX_STATIC_ASSERT((7 * 4) == sizeof(IFEDemux13RegCmd));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDemux13::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEDemux13::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFEDemux13;

        if (NULL == pCreateData->pModule)
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to create IFEDemux13 object.");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data for IFEDemux13 Creation");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDemux13::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEDemux13::Execute(
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
// IFEDemux13::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEDemux13::GetRegCmd()
{
    return reinterpret_cast<VOID*>(&m_regCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDemux13::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDemux13::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    pInputData->pCalculatedData->moduleEnable.lensProcessingModuleConfig.bits.DEMUX_EN = m_moduleEnable;

    pInputData->pCalculatedData->controlPostRawSensitivityBoost = static_cast<INT32>(m_dependenceData.digitalGain * 100);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDemux13::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEDemux13::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if (NULL != pInputData->pOEMIQSetting)
    {
        m_moduleEnable  = (static_cast<OEMIFEIQSetting*>(pInputData->pOEMIQSetting))->DemuxEnable;
        isChanged      |= (TRUE == m_moduleEnable);
    }
    else
    {
        ISPHALTagsData*    pHALTagsData   = pInputData->pHALTagsData;
        TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
        CAMX_ASSERT(NULL != pTuningManager);

        // Search through the tuning data (tree), only when there
        // are changes to the tuning mode data as an optimization
        if ((TRUE == pInputData->tuningModeChanged)    &&
            (TRUE == pTuningManager->IsValidChromatix()))
        {
            CAMX_ASSERT(NULL != pInputData->pTuningData);

            m_pChromatix = pTuningManager->GetChromatix()->GetModule_demux13_ife(
                               reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                               pInputData->pTuningData->noOfSelectionParameter);
        }

        CAMX_ASSERT(NULL != m_pChromatix);
        if (NULL != m_pChromatix)
        {
            if ((NULL == m_dependenceData.pChromatixInput) ||
                (m_pChromatix->SymbolTableID != m_dependenceData.pChromatixInput->SymbolTableID))
            {
                m_dependenceData.pChromatixInput  = m_pChromatix;
                m_moduleEnable                    = m_pChromatix->enable_section.demux_enable;
                isChanged                        |= (TRUE == m_moduleEnable);
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
        }

            // If manual mode then override digital gain
        if (((ControlModeOff   == pHALTagsData->controlMode) ||
             (ControlAEModeOff == pHALTagsData->controlAEMode)) &&
             (pHALTagsData->controlPostRawSensitivityBoost > 0) &&
             (TRUE == m_moduleEnable))
        {
            m_dependenceData.digitalGain = static_cast<FLOAT>(pHALTagsData->controlPostRawSensitivityBoost) / 100.0f;
            isChanged = TRUE;
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "manual mode isp gain %f", pHALTagsData->controlPostRawSensitivityBoost);
        }
        else if ((TRUE == m_moduleEnable) &&
            ((m_pixelFormat != pInputData->sensorData.format) ||
             (FALSE == Utils::FEqual(m_dependenceData.digitalGain, pInputData->sensorData.dGain))))
        {
            m_pixelFormat                 = pInputData->sensorData.format;
            m_dependenceData.digitalGain  = pInputData->sensorData.dGain;
            isChanged                     = TRUE;
        }
    }

    if (TRUE == pInputData->forceTriggerUpdate)
    {
        isChanged = TRUE;
    }

    /// @todo (CAMX-561) Hard code Config Period and Config Config for now.
    m_dependenceData.demuxInConfigPeriod  = 1;
    m_dependenceData.demuxInConfigConfig  = 0;

    m_dependenceData.blackLevelOffset = pInputData->pCalculatedData->BLSblackLevelOffset;

    // Override with computed values in Linearization IQ module
    m_dependenceData.stretchGainRed       = (FALSE == Utils::FEqual(pInputData->pCalculatedData->stretchGainRed, 0.0f)) ?
                                            pInputData->pCalculatedData->stretchGainRed : 1.0f;
    m_dependenceData.stretchGainGreenEven = (FALSE == Utils::FEqual(pInputData->pCalculatedData->stretchGainGreenEven, 0.0f)) ?
                                            pInputData->pCalculatedData->stretchGainGreenEven : 1.0f;
    m_dependenceData.stretchGainGreenOdd  = (FALSE == Utils::FEqual(pInputData->pCalculatedData->stretchGainGreenOdd, 0.0f)) ?
                                            pInputData->pCalculatedData->stretchGainGreenOdd : 1.0f;
    m_dependenceData.stretchGainBlue      = (FALSE == Utils::FEqual(pInputData->pCalculatedData->stretchGainBlue, 0.0f)) ?
                                            pInputData->pCalculatedData->stretchGainBlue : 1.0f;

    if (NULL == pInputData->pOEMIQSetting)
    {
        m_moduleEnable  = TRUE;
        isChanged      |= TRUE;
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDemux13::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEDemux13::CreateCmdList(
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
                                                  regIFE_IFE_0_VFE_DEMUX_CFG,
                                                  IFEDemux13RegLengthDWord,
                                                  reinterpret_cast<UINT32*>(&m_regCmd));
        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDemux13::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEDemux13::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    Demux13OutputData outputData;

    outputData.type                  = PipelineType::IFE;
    outputData.regCommand.pRegIFECmd = &m_regCmd;

    result = IQInterface::Demux13CalculateSetting(&m_dependenceData,
                                                  pInputData->pOEMIQSetting,
                                                  &outputData,
                                                  m_pixelFormat);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Demux Calculation Failed.");
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDemux13::IFEDemux13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEDemux13::IFEDemux13()
{
    m_type           = ISPIQModuleType::IFEDemux;
    m_cmdLength      = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEDemux13RegLengthDWord);
    m_32bitDMILength = 0;
    m_64bitDMILength = 0;
    m_moduleEnable   = TRUE;
    m_pChromatix     = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEDemux13::~IFEDemux13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEDemux13::~IFEDemux13()
{
    m_pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDemux13::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDemux13::DumpRegConfig()
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Demux moduleConfig       [0x%x]", m_regCmd.demuxConfig.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Demux demuxGain0         [0x%x]", m_regCmd.demuxGain0.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Demux demuxGain1         [0x%x]", m_regCmd.demuxGain1.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Demux demuxRightGain0    [0x%x]", m_regCmd.demuxRightGain0.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Demux demuxRightGain1    [0x%x]", m_regCmd.demuxRightGain1.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Demux demuxEvenConfig    [0x%x]", m_regCmd.demuxEvenConfig.u32All);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Demux demuxOddConfig     [0x%x]", m_regCmd.demuxOddConfig.u32All);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDemux13::InitRegValues
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDemux13::InitRegValues()
{
    m_regCmd.demuxConfig.u32All     = 0x00000001;
    m_regCmd.demuxGain0.u32All      = 0x04430444;
    m_regCmd.demuxGain1.u32All      = 0x04440444;
    m_regCmd.demuxRightGain0.u32All = 0x04430444;
    m_regCmd.demuxRightGain1.u32All = 0x04440444;
    m_regCmd.demuxEvenConfig.u32All = 0x000000c9;
    m_regCmd.demuxOddConfig.u32All  = 0x000000ac;
}

CAMX_NAMESPACE_END
