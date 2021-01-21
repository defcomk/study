////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifewb12.cpp
/// @brief CAMXIFEWB12 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxifewb12.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IFEWB12RegLengthDWord = sizeof(IFEWB12RegCmd) / sizeof(UINT32);

CAMX_STATIC_ASSERT((4 * 4) == sizeof(IFEWB12RegCmd));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEWB12::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEWB12::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFEWB12;

        if (NULL == pCreateData->pModule)
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Input Null pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEWB12::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEWB12::UpdateIFEInternalData(
    const ISPInputData* pInputData
    ) const
{
    if (NULL != pInputData->pCalculatedData)
    {
        pInputData->pCalculatedData->colorCorrectionGains.blue      =
            static_cast<FLOAT>(m_regCmd.WBLeftConfig0.bitfields.B_GAIN) / Q7;
        pInputData->pCalculatedData->colorCorrectionGains.greenEven =
            static_cast<FLOAT>(m_regCmd.WBLeftConfig0.bitfields.G_GAIN) / Q7;
        pInputData->pCalculatedData->colorCorrectionGains.greenOdd  =
            static_cast<FLOAT>(m_regCmd.WBLeftConfig0.bitfields.G_GAIN) / Q7;
        pInputData->pCalculatedData->colorCorrectionGains.red       =
            static_cast<FLOAT>(m_regCmd.WBLeftConfig1.bitfields.R_GAIN) / Q7;

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Reporting Gains [%f, %f, %f, %f]",
           pInputData->pCalculatedData->colorCorrectionGains.red,
           pInputData->pCalculatedData->colorCorrectionGains.greenEven,
           pInputData->pCalculatedData->colorCorrectionGains.greenOdd,
           pInputData->pCalculatedData->colorCorrectionGains.red);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEWB12::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEWB12::Execute(
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
// IFEWB12::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEWB12::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL             isChanged     = FALSE;
    AWBFrameControl* pNewAWBUpdate = NULL;
    AECFrameControl* pNewAECUpdate = NULL;
    ISPHALTagsData*  pHALTagsData  = pInputData->pHALTagsData;

    if ((NULL                               == pInputData->pOEMIQSetting)         &&
        (NULL                               != pHALTagsData)                      &&
        (ColorCorrectionModeTransformMatrix == pHALTagsData->colorCorrectionMode) &&
        (((ControlAWBModeOff                == pHALTagsData->controlAWBMode) &&
          (ControlModeAuto                  == pHALTagsData->controlMode))   ||
         (ControlModeOff                    == pHALTagsData->controlMode)))
    {
        m_manualGainOverride = TRUE;
        isChanged           = TRUE;

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
        m_manualGainOverride = FALSE;
        pNewAWBUpdate        = pInputData->pAWBUpdateData;
        pNewAECUpdate        = pInputData->pAECUpdateData;

        if ((FALSE == Utils::FEqual(m_dependenceData.leftGGainWB, pNewAWBUpdate->AWBGains.gGain))       ||
            (FALSE == Utils::FEqual(m_dependenceData.leftBGainWB, pNewAWBUpdate->AWBGains.bGain))       ||
            (FALSE == Utils::FEqual(m_dependenceData.leftRGainWB, pNewAWBUpdate->AWBGains.rGain))       ||
            (FALSE == Utils::FEqual(m_dependenceData.predictiveGain, pNewAECUpdate->predictiveGain))    ||
            (TRUE  == pInputData->forceTriggerUpdate))
        {
            m_dependenceData.predictiveGain = pNewAECUpdate->predictiveGain;

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
            isChanged = TRUE;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Null Input Pointer");
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEWB12::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEWB12::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = NULL;

    if (NULL != pInputData->pCmdBuffer)
    {
        pCmdBuffer = pInputData->pCmdBuffer;
        result     = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_DEMO_WB_LEFT_CFG_0,
                                                  IFEWB12RegLengthDWord,
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
// IFEWB12::UpdateManualGain
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEWB12::UpdateManualGain()
{
    m_regCmd.WBLeftConfig0.bitfields.G_GAIN = Utils::FloatToQNumber(m_manualControl.gGain, Q7);
    m_regCmd.WBLeftConfig0.bitfields.B_GAIN = Utils::FloatToQNumber(m_manualControl.bGain, Q7);
    m_regCmd.WBLeftConfig1.bitfields.R_GAIN = Utils::FloatToQNumber(m_manualControl.rGain, Q7);

    m_regCmd.WBLeftOffsetConfig0.bitfields.G_OFFSET = 0;
    m_regCmd.WBLeftOffsetConfig0.bitfields.B_OFFSET = 0;
    m_regCmd.WBLeftOffsetConfig1.bitfields.R_OFFSET = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEWB12::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEWB12::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult     result = CamxResultSuccess;
    WB12OutputData outputData;

    if (TRUE == m_manualGainOverride)
    {
        UpdateManualGain();
    }
    else
    {
        outputData.pRegCmd = &m_regCmd;
        result             = IQInterface::IFEWB12CalculateSetting(&m_dependenceData, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "WB Calculation Failed. result %d", result);
        }
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEWB12::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEWB12::DumpRegConfig() const
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "G_Gain   [0x%x]", m_regCmd.WBLeftConfig0.bitfields.G_GAIN);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "B_Gain   [0x%x]", m_regCmd.WBLeftConfig0.bitfields.B_GAIN);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "R_Gain   [0x%x]", m_regCmd.WBLeftConfig1.bitfields.R_GAIN);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "G_OFFSET [0x%x]", m_regCmd.WBLeftOffsetConfig0.bitfields.G_OFFSET);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "B_OFFSET [0x%x]", m_regCmd.WBLeftOffsetConfig0.bitfields.B_OFFSET);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "R_OFFSET [0x%x]", m_regCmd.WBLeftOffsetConfig1.bitfields.R_OFFSET);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEWB12::InitRegValues
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEWB12::InitRegValues()
{
    m_regCmd.WBLeftConfig0.bitfields.G_GAIN         = 0x0080;
    m_regCmd.WBLeftConfig0.bitfields.B_GAIN         = 0x00bf;
    m_regCmd.WBLeftConfig1.bitfields.R_GAIN         = 0x0106;
    m_regCmd.WBLeftOffsetConfig0.bitfields.G_OFFSET = 0x0000;
    m_regCmd.WBLeftOffsetConfig0.bitfields.B_OFFSET = 0x0000;
    m_regCmd.WBLeftOffsetConfig1.bitfields.R_OFFSET = 0x0000;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEWB12::IFEWB12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEWB12::IFEWB12()
{
    m_type      = ISPIQModuleType::IFEWB;
    m_cmdLength = PacketBuilder::RequiredWriteRegRangeSizeInDwords(IFEWB12RegLengthDWord);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEWB12::~IFEWB12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEWB12::~IFEWB12()
{
}

CAMX_NAMESPACE_END
