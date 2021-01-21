////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifer2pd10.cpp
/// @brief CAMXIFER2PD class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifer2pd10.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFER2PD10::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFER2PD10::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFER2PD10(pCreateData);
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
// IFER2PD10::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFER2PD10::CheckDependenceChange(
    ISPInputData* pInputData)
{
    CAMX_UNREFERENCED_PARAM(pInputData);
    /// @todo (CAMX-666) Update bitwidth based on HAL request and check for update
    m_bitWidth = BitWidthTen;

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFER2PD10::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFER2PD10::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    IFE_IFE_0_VFE_MODULE_ZOOM_EN* pR2PDEnable        =
        &pInputData->pCalculatedData->moduleEnable.frameProcessingModuleConfig;

    IFE_IFE_0_VFE_MODULE_DISP_EN* pR2PDDisplayEnable =
        &pInputData->pCalculatedData->moduleEnable.frameProcessingDisplayModuleConfig;

    switch (m_modulePath)
    {
        case IFEPipelinePath::DS4Path:
            pR2PDEnable->bitfields.R2PD_1ST_EN = TRUE;
            break;

        case IFEPipelinePath::DS16Path:
            pR2PDEnable->bitfields.R2PD_2ND_EN = TRUE;
            break;

        case IFEPipelinePath::DisplayDS4Path:
            pR2PDDisplayEnable->bitfields.R2PD_1ST_EN = TRUE;
            break;

        case IFEPipelinePath::DisplayDS16Path:
            pR2PDDisplayEnable->bitfields.R2PD_2ND_EN = TRUE;
            break;

        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Invalid module path");
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFER2PD10::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFER2PD10::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pInputData))
    {
        // Check if dependent has been updated compared to last request
        if (TRUE == CheckDependenceChange(pInputData))
        {
            RunCalculation(pInputData);
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
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFER2PD10::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFER2PD10::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFER2PD10::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFER2PD10::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    UINT32     reg;
    UINT32     numberOfValues;
    UINT32*    pValues;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        switch (m_modulePath)
        {
            case IFEPipelinePath::DS4Path:
                reg            = regIFE_IFE_0_VFE_R2PD_1ST_CFG;
                numberOfValues = sizeof(IFE_IFE_0_VFE_R2PD_1ST_CFG) / RegisterWidthInBytes;
                pValues        = reinterpret_cast<UINT32*>(&m_regCmd.DS4Config);
                break;

            case IFEPipelinePath::DS16Path:
                reg            = regIFE_IFE_0_VFE_R2PD_2ND_CFG;
                numberOfValues = sizeof(IFE_IFE_0_VFE_R2PD_2ND_CFG) / RegisterWidthInBytes;
                pValues        = reinterpret_cast<UINT32*>(&m_regCmd.DS16Config);
                break;

            case IFEPipelinePath::DisplayDS4Path:
                reg            = regIFE_IFE_0_VFE_DISP_R2PD_1ST_CFG;
                numberOfValues = sizeof(IFE_IFE_0_VFE_DISP_R2PD_1ST_CFG) / RegisterWidthInBytes;
                pValues        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS4Config);
                break;

            case IFEPipelinePath::DisplayDS16Path:
                reg            = regIFE_IFE_0_VFE_DISP_R2PD_2ND_CFG;
                numberOfValues = sizeof(IFE_IFE_0_VFE_DISP_R2PD_2ND_CFG) / RegisterWidthInBytes;
                pValues        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS16Config);
                break;

            default:
                CAMX_ASSERT_ALWAYS_MESSAGE("We would never runinto this case");
                return CamxResultEInvalidState;
                break;
        }

        result = PacketBuilder::WriteRegRange(pCmdBuffer, reg, numberOfValues, pValues);

        if (CamxResultSuccess != result)
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Failed to configure Crop FD path Register");
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
// IFER2PD10::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFER2PD10::DumpRegConfig()
{
    if (IFEPipelinePath::DS4Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 Config [0x%x]", m_regCmd.DS4Config);
    }

    if (IFEPipelinePath::DS16Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 Config [0x%x]", m_regCmd.DS16Config);
    }

    if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS4 Config [0x%x]", m_regCmd.displayDS4Config);
    }

    if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS16 Config [0x%x]", m_regCmd.displayDS16Config);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFER2PD10::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFER2PD10::RunCalculation(
    ISPInputData* pInputData)
{
    UINT32 packMode = 0;

    /// @todo (CAMX-666) Update packmode based on HAL request and check for update
    if (BitWidthEight == m_bitWidth)
    {
        packMode = 1;
    }

    if (IFEPipelinePath::DS4Path == m_modulePath)
    {
        // Flush count is updated as per HLD guidelines
        m_regCmd.DS4Config.bitfields.FLUSH_PACE_CNT = 0x3;
        m_regCmd.DS4Config.bitfields.PACK_MODE      = packMode;
    }

    if (IFEPipelinePath::DS16Path == m_modulePath)
    {
        // Flush count is updated as per HLD guidelines
        m_regCmd.DS16Config.bitfields.FLUSH_PACE_CNT = 0xf;
        m_regCmd.DS16Config.bitfields.PACK_MODE      = packMode;
    }

    if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
    {
        // Flush count is updated as per HLD guidelines
        m_regCmd.displayDS4Config.bitfields.FLUSH_PACE_CNT = 0x3;
        m_regCmd.displayDS4Config.bitfields.PACK_MODE      = packMode;
    }

    if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
    {
        // Flush count is updated as per HLD guidelines
        m_regCmd.displayDS16Config.bitfields.FLUSH_PACE_CNT = 0xf;
        m_regCmd.displayDS16Config.bitfields.PACK_MODE      = packMode;
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFER2PD10::IFER2PD10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFER2PD10::IFER2PD10(
    IFEModuleCreateData* pCreateData)
{
    m_type         = ISPIQModuleType::IFER2PD;
    m_moduleEnable = TRUE;
    m_cmdLength    =
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFE_IFE_0_VFE_R2PD_1ST_CFG) / RegisterWidthInBytes) +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFE_IFE_0_VFE_R2PD_2ND_CFG) / RegisterWidthInBytes);

    m_modulePath = pCreateData->pipelineData.IFEPath;

    m_32bitDMILength = 0;
    m_64bitDMILength = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFER2PD10::~IFER2PD10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFER2PD10::~IFER2PD10()
{
}

CAMX_NAMESPACE_END
