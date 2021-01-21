////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpswb13.cpp
/// @brief CAMXBPSWB13 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxbpswb13.h"
#include "camxdefs.h"
#include "camxhwenvironment.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtitan17xcontext.h"
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 BPSWB13RegLengthDWord = sizeof(BPSWB13RegCmd) / sizeof(UINT32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSWB13::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSWB13::Create(
    BPSModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        pCreateData->pModule = CAMX_NEW BPSWB13(pCreateData->pNodeIdentifier);

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
// BPSWB13::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL BPSWB13::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if (NULL != pInputData)
    {
        ISPHALTagsData* pHALTagsData = pInputData->pHALTagsData;

        if ((NULL                               == pInputData->pOEMIQSetting)         &&
            (NULL                               != pHALTagsData)                      &&
            (ColorCorrectionModeTransformMatrix == pHALTagsData->colorCorrectionMode) &&
            (((ControlAWBModeOff == pHALTagsData->controlAWBMode) &&
              (ControlModeAuto   == pHALTagsData->controlMode))   ||
             (ControlModeOff     == pHALTagsData->controlMode)))
        {
            m_manualGainOverride = TRUE;
            isChanged            = TRUE;
            m_moduleEnable       = TRUE;

            m_manualControl.rGain = pHALTagsData->colorCorrectionGains.red;
            // Use the green even and ignore green odd
            m_manualControl.gGain = pHALTagsData->colorCorrectionGains.greenEven;
            m_manualControl.bGain = pHALTagsData->colorCorrectionGains.blue;

            CAMX_LOG_VERBOSE(CamxLogGroupISP, "App Gains [%f, %f, %f, %f]",
                m_manualControl.rGain,
                m_manualControl.gGain,
                pHALTagsData->colorCorrectionGains.greenOdd,
                m_manualControl.bGain);
        }
        else if ((NULL != pInputData->pHwContext)   &&
                 (NULL != pInputData->pAWBUpdateData) &&
                 (NULL != pInputData->pAECUpdateData))
        {
            AWBFrameControl* pNewAWBUpdate = pInputData->pAWBUpdateData;
            AECFrameControl* pNewAECUpdate = pInputData->pAECUpdateData;
            m_manualGainOverride = FALSE;

            if ((FALSE == Utils::FEqual(m_dependenceData.leftGGainWB, pNewAWBUpdate->AWBGains.gGain)) ||
                (FALSE == Utils::FEqual(m_dependenceData.leftBGainWB, pNewAWBUpdate->AWBGains.bGain)) ||
                (FALSE == Utils::FEqual(m_dependenceData.leftRGainWB, pNewAWBUpdate->AWBGains.rGain)) ||
                (FALSE == Utils::FEqual(m_dependenceData.predictiveGain, pNewAECUpdate->predictiveGain)))
            {
                if (TRUE == pInputData->sensorData.isMono)
                {
                   // Set Unity Gains for Mono sensor
                    m_dependenceData.leftGGainWB = 1.0f;
                    m_dependenceData.leftBGainWB = 1.0f;
                    m_dependenceData.leftRGainWB = 1.0f;
                }
                else
                {
                    m_dependenceData.leftGGainWB  = pNewAWBUpdate->AWBGains.gGain;
                    m_dependenceData.leftBGainWB  = pNewAWBUpdate->AWBGains.bGain;
                    m_dependenceData.leftRGainWB  = pNewAWBUpdate->AWBGains.rGain;
                }

                m_dependenceData.predictiveGain = pNewAECUpdate->predictiveGain;
                isChanged = TRUE;
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSWB13::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSWB13::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regBPS_BPS_0_CLC_DEMOSAIC_WB_GAIN_CFG_0,
                                              BPSWB13RegLengthDWord,
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
// BPSWB13::UpdateManualGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSWB13::UpdateManualGain()
{
    m_regCmd.whiteBalanceConfig0.bitfields.G_GAIN = Utils::FloatToQNumber(m_manualControl.gGain, 1 << 10);
    m_regCmd.whiteBalanceConfig0.bitfields.B_GAIN = Utils::FloatToQNumber(m_manualControl.bGain, 1 << 10);
    m_regCmd.whiteBalanceConfig1.bitfields.R_GAIN = Utils::FloatToQNumber(m_manualControl.rGain, 1 << 10);

    m_regCmd.whiteBalaneOffset0.bitfields.G_OFFSET = 0;
    m_regCmd.whiteBalaneOffset0.bitfields.B_OFFSET = 0;
    m_regCmd.whiteBalaneOffset1.bitfields.R_OFFSET = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSWB13::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSWB13::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (TRUE == m_manualGainOverride)
    {
        UpdateManualGain();
    }
    else if (NULL != pInputData)
    {
        WB13OutputData outputData;

        outputData.pRegCmd = &m_regCmd;
        result             = IQInterface::BPSWB13CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "WB Calculation Failed. result %d", result);
        }
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    if ((NULL != pInputData) && (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod)))
    {
        DumpRegConfig();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSWB13::UpdateBPSInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSWB13::UpdateBPSInternalData(
    const ISPInputData* pInputData
    ) const
{
    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pBPSTuningMetadata)
    {
        CAMX_STATIC_ASSERT(sizeof(BPSWB13RegCmd) <= sizeof(pInputData->pBPSTuningMetadata->BPSWBData));

        Utils::Memcpy(&pInputData->pBPSTuningMetadata->BPSWBData,
                      &m_regCmd,
                      sizeof(BPSWB13RegCmd));
    }

    if (NULL != pInputData->pCalculatedData)
    {
        pInputData->pCalculatedData->colorCorrectionGains.blue      =
            static_cast<FLOAT>(m_regCmd.whiteBalanceConfig0.bitfields.B_GAIN) / (1 << 10);
        pInputData->pCalculatedData->colorCorrectionGains.greenEven =
            static_cast<FLOAT>(m_regCmd.whiteBalanceConfig0.bitfields.G_GAIN) / (1 << 10);
        pInputData->pCalculatedData->colorCorrectionGains.greenOdd  =
            static_cast<FLOAT>(m_regCmd.whiteBalanceConfig0.bitfields.G_GAIN) / (1 << 10);
        pInputData->pCalculatedData->colorCorrectionGains.red       =
            static_cast<FLOAT>(m_regCmd.whiteBalanceConfig1.bitfields.R_GAIN) / (1 << 10);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSWB13::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult BPSWB13::Execute(
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
        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("WB Operation failed %d", result);
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
// BPSWB13::BPSWB13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSWB13::BPSWB13(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type              = ISPIQModuleType::BPSWB;

    m_cmdLength    = PacketBuilder::RequiredWriteRegRangeSizeInDwords(BPSWB13RegLengthDWord);
    m_moduleEnable = TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSWB13::~BPSWB13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BPSWB13::~BPSWB13()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BPSWB13::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSWB13::DumpRegConfig() const
{
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/BPS_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** BPS WB13  ********\n");
        CamX::OsUtils::FPrintF(pFile, "* WB13 CFG [HEX] \n");
        CamX::OsUtils::FPrintF(pFile, "G_Gain   = %x\n", m_regCmd.whiteBalanceConfig0.bitfields.G_GAIN);
        CamX::OsUtils::FPrintF(pFile, "B_Gain   = %x\n", m_regCmd.whiteBalanceConfig0.bitfields.B_GAIN);
        CamX::OsUtils::FPrintF(pFile, "R_Gain   = %x\n", m_regCmd.whiteBalanceConfig1.bitfields.R_GAIN);
        CamX::OsUtils::FPrintF(pFile, "G_OFFSET = %x\n", m_regCmd.whiteBalaneOffset0.bitfields.G_OFFSET);
        CamX::OsUtils::FPrintF(pFile, "B_OFFSET = %x\n", m_regCmd.whiteBalaneOffset0.bitfields.B_OFFSET);
        CamX::OsUtils::FPrintF(pFile, "R_OFFSET = %x\n", m_regCmd.whiteBalaneOffset1.bitfields.R_OFFSET);
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BPSWB13::InitRegValues
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID BPSWB13::InitRegValues()
{
    m_regCmd.whiteBalanceConfig0.bitfields.G_GAIN   = 0x00000400;
    m_regCmd.whiteBalanceConfig0.bitfields.B_GAIN   = 0x00000827;
    m_regCmd.whiteBalanceConfig1.bitfields.R_GAIN   = 0x00000512;
    m_regCmd.whiteBalaneOffset0.bitfields.G_OFFSET  = 0x00000000;
    m_regCmd.whiteBalaneOffset0.bitfields.B_OFFSET  = 0x00000000;
    m_regCmd.whiteBalaneOffset1.bitfields.R_OFFSET  = 0x00000000;
}

CAMX_NAMESPACE_END
