////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsdemux13.cpp
/// @brief CAMXBPSDEMUX13 class implementation
///        Demultiplex Bayer mosaicked pixels into R/G/B channels with channel gain application, or simply demultiplex input
///        stream, e.g., interleaved YUV, to separate channels
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bps_data.h"
#include "camxiqinterface.h"
#include "camxtuningdatamanager.h"
#include "camxnode.h"
#include "camxbpsdemux13.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemux13::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemux13::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        pCreateData->pModule = CAMX_NEW BPSDemux13(pCreateData->pNodeIdentifier);
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
// BPSDemux13::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSDemux13::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)             &&
        (NULL != pInputData->pHwContext) &&
        (NULL != pInputData->pHALTagsData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMBPSIQSetting*>(pInputData->pOEMIQSetting))->DemuxEnable;

            isChanged      = (TRUE == m_moduleEnable);
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

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_demux13_bps(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if ((NULL == m_dependenceData.pChromatixInput) ||
                (m_pChromatix->SymbolTableID != m_dependenceData.pChromatixInput->SymbolTableID))
                {
                    m_dependenceData.pChromatixInput = m_pChromatix;
                    m_moduleEnable                   = m_pChromatix->enable_section.demux_enable;
                    isChanged                        = (TRUE == m_moduleEnable);
                }
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupISP, "Failed to get Chromatix");
            }
        }

        if ((TRUE == m_moduleEnable))
        {
            ISPHALTagsData* pHALTagsData = pInputData->pHALTagsData;

            if ((m_pixelFormat != pInputData->sensorData.format) ||
                (FALSE == Utils::FEqual(m_dependenceData.digitalGain, pInputData->sensorData.dGain)))
            {
                m_pixelFormat                = pInputData->sensorData.format;
                m_dependenceData.digitalGain = pInputData->sensorData.dGain;
                isChanged                    = TRUE;
            }
            // If manual mode then override digital gain
            if (NULL != pHALTagsData)
            {
                if (((ControlModeOff == pHALTagsData->controlMode) ||
                    (ControlAEModeOff == pHALTagsData->controlAEMode)) &&
                    (pHALTagsData->controlPostRawSensitivityBoost > 0) &&
                    (NULL == pInputData->pOEMIQSetting))
                {
                    m_dependenceData.digitalGain = pHALTagsData->controlPostRawSensitivityBoost / 100.0f;
                    isChanged                    = TRUE;

                    CAMX_LOG_VERBOSE(CamxLogGroupISP, "manual mode isp gain %d",
                        pHALTagsData->controlPostRawSensitivityBoost);
                }
            }
        }

        /// @todo (CAMX-561) Hard code Config Period and Config Config for now.
        m_dependenceData.demuxInConfigPeriod = 1;
        m_dependenceData.demuxInConfigConfig = 0;
        m_dependenceData.blackLevelOffset    = pInputData->pCalculatedData->BLSblackLevelOffset;

        // Get from chromatix
        m_dependenceData.digitalGain          =
            (FALSE == Utils::FEqual(m_dependenceData.digitalGain, 0.0f)) ?
            m_dependenceData.digitalGain : 1.0f;
        m_dependenceData.stretchGainRed       =
            (FALSE == Utils::FEqual(pInputData->pCalculatedData->stretchGainRed, 0.0f)) ?
            pInputData->pCalculatedData->stretchGainRed : 1.0f;
        m_dependenceData.stretchGainGreenEven =
            (FALSE == Utils::FEqual(pInputData->pCalculatedData->stretchGainGreenEven, 0.0f)) ?
            pInputData->pCalculatedData->stretchGainGreenEven : 1.0f;
        m_dependenceData.stretchGainGreenOdd  =
            (FALSE == Utils::FEqual(pInputData->pCalculatedData->stretchGainGreenOdd, 0.0f)) ?
            pInputData->pCalculatedData->stretchGainGreenOdd : 1.0f;
        m_dependenceData.stretchGainBlue      =
            (FALSE == Utils::FEqual(pInputData->pCalculatedData->stretchGainBlue, 0.0f)) ?
            pInputData->pCalculatedData->stretchGainBlue : 1.0f;
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid Input: pHwContext:%p", pInputData->pHwContext);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
        }
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemux13::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemux13::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_BPC_PDPC_DEMUX_CFG,
                                              (sizeof(BPSDemux13RegCmd) / RegisterWidthInBytes),
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
// BPSDemux13::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemux13::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        Demux13OutputData outputData;

        outputData.type                  = PipelineType::BPS;
        outputData.regCommand.pRegBPSCmd = &m_regCmd;

        result = IQInterface::Demux13CalculateSetting(&m_dependenceData,
                                                      pInputData->pOEMIQSetting,
                                                      &outputData,
                                                      m_pixelFormat);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Demux Calculation Failed. result %d", result);
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
// BPSDemux13::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSDemux13::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    BpsIQSettings* pBPSIQSettings = reinterpret_cast<BpsIQSettings*>(pInputData->pipelineBPSData.pIQSettings);
    CAMX_ASSERT(NULL != pBPSIQSettings);

    if (NULL != pBPSIQSettings)
    {
        pBPSIQSettings->pdpcParameters.moduleCfg.CHANNEL_BALANCE_EN = m_moduleEnable;
        pBPSIQSettings->pdpcParameters.moduleCfg.EN                 = m_moduleEnable;
    }

    pInputData->pCalculatedData->controlPostRawSensitivityBoost = static_cast<INT32>(m_dependenceData.digitalGain * 100);

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(BPSDemux13RegCmd) <= sizeof(pInputData->pBPSTuningMetadata->BPSDemuxData.demuxConfig));

        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSDemuxData.demuxConfig, &m_regCmd, sizeof(BPSDemux13RegCmd));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemux13::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSDemux13::Execute(
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
// BPSDemux13::BPSDemux13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSDemux13::BPSDemux13(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier     = pNodeIdentifier;
    m_type                = ISPIQModuleType::BPSDemux;
    m_moduleEnable        = FALSE;
    m_pChromatix          = NULL;

    m_cmdLength = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(BPSDemux13RegCmd) / RegisterWidthInBytes);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemux13::~BPSDemux13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSDemux13::~BPSDemux13()
{
    m_pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemux13::InitRegValues
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSDemux13::InitRegValues()
{
    m_regCmd.config.u32All             = 0x00000001;
    m_regCmd.gainChannel0.u32All       = 0x044c044d;
    m_regCmd.gainChannel12.u32All      = 0x044d044c;
    m_regCmd.gainRightChannel0.u32All  = 0x044c044d;
    m_regCmd.gainRightChannel12.u32All = 0x044d044c;
    m_regCmd.oddConfig.u32All          = 0x000000ac;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSDemux13::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSDemux13::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "********  BPS DEMUX13 ********  \n");
        CamX::OsUtils::FPrintF(pFile, "* DEMUX13 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "Module Config         = %x\n", m_regCmd.config);
        CamX::OsUtils::FPrintF(pFile, "Channel 0 gain        = %x\n", m_regCmd.gainChannel0);
        CamX::OsUtils::FPrintF(pFile, "Channel 12 gain       = %x\n", m_regCmd.gainChannel12);
        CamX::OsUtils::FPrintF(pFile, "Right Channel 0 gain  = %x\n", m_regCmd.gainRightChannel0);
        CamX::OsUtils::FPrintF(pFile, "Right Channel 12 gain = %x\n", m_regCmd.gainRightChannel12);
        CamX::OsUtils::FPrintF(pFile, "odd Config            = %x\n", m_regCmd.oddConfig);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
}

CAMX_NAMESPACE_END
