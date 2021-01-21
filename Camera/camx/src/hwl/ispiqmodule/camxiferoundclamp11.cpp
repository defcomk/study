////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxiferoundclamp11.cpp
/// @brief CAMXIFEROUNDCLAMP class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxiferoundclamp11.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFERoundClamp11::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        IFEPipelinePath modulePath = pCreateData->pipelineData.IFEPath;

        if ((IFEPipelinePath::FDPath          == modulePath) ||
            (IFEPipelinePath::FullPath        == modulePath) ||
            (IFEPipelinePath::DS4Path         == modulePath) ||
            (IFEPipelinePath::DS16Path        == modulePath) ||
            (IFEPipelinePath::DisplayFullPath == modulePath) ||
            (IFEPipelinePath::DisplayDS4Path  == modulePath) ||
            (IFEPipelinePath::DisplayDS16Path == modulePath))
        {
            pCreateData->pModule = CAMX_NEW IFERoundClamp11(pCreateData);
            if (NULL == pCreateData->pModule)
            {
                result = CamxResultENoMemory;
                CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
            }
        }
        else
        {
            result = CamxResultEInvalidArg;
            CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid IFE Pipeline Path");
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
// IFERoundClamp11::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFERoundClamp11::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL             result     = FALSE;
    StreamDimension* pHALStream = &pInputData->pStripeConfig->stream[m_output];
    StreamDimension* pDimension = NULL;

    switch (m_modulePath)
    {
        case IFEPipelinePath::FDPath:
            pDimension = &m_pState->FDDimension;
            break;

        case IFEPipelinePath::FullPath:
            pDimension = &m_pState->fullDimension;
            break;

        case IFEPipelinePath::DS4Path:
            pDimension = &m_pState->DS4Dimension;
            break;

        case IFEPipelinePath::DS16Path:
            pDimension = &m_pState->DS16Dimension;
            break;

        case IFEPipelinePath::DisplayFullPath:
            pDimension = &m_pState->displayFullDimension;
            break;

        case IFEPipelinePath::DisplayDS4Path:
            pDimension = &m_pState->displayDS4Dimension;
            break;

        case IFEPipelinePath::DisplayDS16Path:
            pDimension = &m_pState->displayDS16Dimension;
            break;

        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("We would never runinto this case");
            return FALSE;
            break;
    }

    // Check if Full/FD/DS/DS16 dimensions have changed
    if ((pHALStream->width  != pDimension->width) ||
        (pHALStream->height != pDimension->height))
    {
        *pDimension = *pHALStream;

        result = TRUE;
    }

    if (pInputData->HALData.format[m_output] != m_pixelFormat)
    {
        m_pixelFormat = pInputData->HALData.format[m_output];

        result = TRUE;

        if (TRUE == ImageFormatUtils::Is10BitFormat(m_pixelFormat))
        {
            m_bitWidth = BitWidthTen;
        }
        else
        {
            m_bitWidth = BitWidthEight;
        }
    }

    if (TRUE == pInputData->forceTriggerUpdate)
    {
        result = TRUE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::DumpRegConfig()
{
    if (IFEPipelinePath::FDPath == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "FD Luma Clamp config   [0x%x]", m_regCmd.FDLuma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "FD Luma Round config   [0x%x]", m_regCmd.FDLuma.round);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "FD Chroma Clamp config [0x%x]", m_regCmd.FDChroma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "FD Chroma Round config [0x%x]", m_regCmd.FDChroma.round);
    }

    if (IFEPipelinePath::FullPath == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Full Luma Clamp config   [0x%x]", m_regCmd.fullLuma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Full Luma Round config   [0x%x]", m_regCmd.fullLuma.round);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Full Chroma Clamp config [0x%x]", m_regCmd.fullChroma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Full Chroma Round config [0x%x]", m_regCmd.fullChroma.round);
    }

    if (IFEPipelinePath::DS4Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 Luma Clamp config   [0x%x]", m_regCmd.DS4Luma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 Luma Round config   [0x%x]", m_regCmd.DS4Luma.round);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 Chroma Clamp config [0x%x]", m_regCmd.DS4Chroma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 Chroma Round config [0x%x]", m_regCmd.DS4Chroma.round);
    }

    if (IFEPipelinePath::DS16Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 Luma Clamp config   [0x%x]", m_regCmd.DS16Luma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 Luma Round config   [0x%x]", m_regCmd.DS16Luma.round);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 Chroma Clamp config [0x%x]", m_regCmd.DS16Chroma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 Chroma Round config [0x%x]", m_regCmd.DS16Chroma.round);
    }

    if (IFEPipelinePath::DisplayFullPath == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display Luma Clamp config   [0x%x]", m_regCmd.displayLuma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display Luma Round config   [0x%x]", m_regCmd.displayLuma.round);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display Chroma Clamp config [0x%x]", m_regCmd.displayChroma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display Chroma Round config [0x%x]", m_regCmd.displayChroma.round);
    }

    if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS4 Luma Clamp config   [0x%x]", m_regCmd.displayDS4Luma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS4 Luma Round config   [0x%x]", m_regCmd.displayDS4Luma.round);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS4 Chroma Clamp config [0x%x]", m_regCmd.displayDS4Chroma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS4 Chroma Round config [0x%x]", m_regCmd.displayDS4Chroma.round);
    }

    if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS16 Luma Clamp config   [0x%x]", m_regCmd.displayDS16Luma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS16 Luma Round config   [0x%x]", m_regCmd.displayDS16Luma.round);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS16 Chroma Clamp config [0x%x]", m_regCmd.displayDS16Chroma.clamp);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS16 Chroma Round config [0x%x]", m_regCmd.displayDS16Chroma.round);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    IFEModuleEnableConfig* pModuleEnable = &pInputData->pCalculatedData->moduleEnable;

    if (IFEPipelinePath::FDPath == m_modulePath)
    {
        pModuleEnable->FDLumaCropRoundClampConfig.bitfields.CH0_CLAMP_EN   = TRUE;
        pModuleEnable->FDLumaCropRoundClampConfig.bitfields.CH0_ROUND_EN   = TRUE;
        pModuleEnable->FDChromaCropRoundClampConfig.bitfields.CH0_CLAMP_EN = TRUE;
        pModuleEnable->FDChromaCropRoundClampConfig.bitfields.CH0_ROUND_EN = TRUE;
    }

    if (IFEPipelinePath::FullPath == m_modulePath)
    {
        pModuleEnable->fullLumaCropRoundClampConfig.bitfields.CH0_CLAMP_EN   = TRUE;
        pModuleEnable->fullLumaCropRoundClampConfig.bitfields.CH0_ROUND_EN   = TRUE;
        pModuleEnable->fullChromaCropRoundClampConfig.bitfields.CH0_CLAMP_EN = TRUE;
        pModuleEnable->fullChromaCropRoundClampConfig.bitfields.CH0_ROUND_EN = TRUE;
    }

    if (IFEPipelinePath::DS4Path == m_modulePath)
    {
        pModuleEnable->DS4LumaCropRoundClampConfig.bitfields.CH0_CLAMP_EN   = TRUE;
        pModuleEnable->DS4LumaCropRoundClampConfig.bitfields.CH0_ROUND_EN   = TRUE;
        pModuleEnable->DS4ChromaCropRoundClampConfig.bitfields.CH0_CLAMP_EN = TRUE;
        pModuleEnable->DS4ChromaCropRoundClampConfig.bitfields.CH0_ROUND_EN = TRUE;
    }

    if (IFEPipelinePath::DS16Path == m_modulePath)
    {
        pModuleEnable->DS16LumaCropRoundClampConfig.bitfields.CH0_CLAMP_EN   = TRUE;
        pModuleEnable->DS16LumaCropRoundClampConfig.bitfields.CH0_ROUND_EN   = TRUE;
        pModuleEnable->DS16ChromaCropRoundClampConfig.bitfields.CH0_CLAMP_EN = TRUE;
        pModuleEnable->DS16ChromaCropRoundClampConfig.bitfields.CH0_ROUND_EN = TRUE;
    }

    if (IFEPipelinePath::DisplayFullPath == m_modulePath)
    {
        pModuleEnable->displayFullLumaCropRoundClampConfig.bitfields.CH0_CLAMP_EN   = TRUE;
        pModuleEnable->displayFullLumaCropRoundClampConfig.bitfields.CH0_ROUND_EN   = TRUE;
        pModuleEnable->displayFullChromaCropRoundClampConfig.bitfields.CH0_CLAMP_EN = TRUE;
        pModuleEnable->displayFullChromaCropRoundClampConfig.bitfields.CH0_ROUND_EN = TRUE;
    }

    if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
    {
        pModuleEnable->displayDS4LumaCropRoundClampConfig.bitfields.CH0_CLAMP_EN   = TRUE;
        pModuleEnable->displayDS4LumaCropRoundClampConfig.bitfields.CH0_ROUND_EN   = TRUE;
        pModuleEnable->displayDS4ChromaCropRoundClampConfig.bitfields.CH0_CLAMP_EN = TRUE;
        pModuleEnable->displayDS4ChromaCropRoundClampConfig.bitfields.CH0_ROUND_EN = TRUE;
    }

    if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
    {
        pModuleEnable->displayDS16LumaCropRoundClampConfig.bitfields.CH0_CLAMP_EN   = TRUE;
        pModuleEnable->displayDS16LumaCropRoundClampConfig.bitfields.CH0_ROUND_EN   = TRUE;
        pModuleEnable->displayDS16ChromaCropRoundClampConfig.bitfields.CH0_CLAMP_EN = TRUE;
        pModuleEnable->displayDS16ChromaCropRoundClampConfig.bitfields.CH0_ROUND_EN = TRUE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFERoundClamp11::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(static_cast<UINT>(m_modulePath) < IFEMaxNonCommonPaths);

    if ((NULL != pInputData))
    {
        m_pState = &pInputData->pStripeConfig->stateRoundClamp[static_cast<UINT>(m_modulePath)];

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
// IFERoundClamp11::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFERoundClamp11::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::ConfigureFDPathRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::ConfigureFDPathRegisters(
    UINT32        minValue,
    UINT32        maxValue)
{
    IFERoundClamp11FDLumaReg*   pFDLuma       = &m_regCmd.FDLuma;
    IFERoundClamp11FDChromaReg* pFDChroma     = &m_regCmd.FDChroma;

    pFDLuma->clamp.bitfields.CH0_CLAMP_MIN          = minValue;
    pFDLuma->clamp.bitfields.CH0_CLAMP_MAX          = maxValue;
    pFDLuma->round.bitfields.CH0_INTERLEAVED        = 0;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pFDLuma->round.bitfields.CH0_ROUND_OFF_BITS     = BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pFDLuma->round.bitfields.CH0_ROUNDING_PATTERN   = 0x3;
    pFDChroma->clamp.bitfields.CH0_CLAMP_MIN        = minValue;
    pFDChroma->clamp.bitfields.CH0_CLAMP_MAX        = maxValue;
    pFDChroma->round.bitfields.CH0_INTERLEAVED      = 1;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pFDChroma->round.bitfields.CH0_ROUND_OFF_BITS   = BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pFDChroma->round.bitfields.CH0_ROUNDING_PATTERN = 0x3;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::ConfigureFullPathRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::ConfigureFullPathRegisters(
    UINT32        minValue,
    UINT32        maxValue)
{
    IFERoundClamp11FullLumaReg*   pFullLuma     = &m_regCmd.fullLuma;
    IFERoundClamp11FullChromaReg* pFullChroma   = &m_regCmd.fullChroma;

    pFullLuma->clamp.bitfields.CH0_CLAMP_MIN          = minValue;
    pFullLuma->clamp.bitfields.CH0_CLAMP_MAX          = maxValue;
    pFullLuma->round.bitfields.CH0_INTERLEAVED        = 0;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pFullLuma->round.bitfields.CH0_ROUND_OFF_BITS     = BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pFullLuma->round.bitfields.CH0_ROUNDING_PATTERN   = 0x3;
    pFullChroma->clamp.bitfields.CH0_CLAMP_MIN        = minValue;
    pFullChroma->clamp.bitfields.CH0_CLAMP_MAX        = maxValue;
    pFullChroma->round.bitfields.CH0_INTERLEAVED      = 1;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pFullChroma->round.bitfields.CH0_ROUND_OFF_BITS   = BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pFullChroma->round.bitfields.CH0_ROUNDING_PATTERN = 0x3;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::ConfigureDS4PathRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::ConfigureDS4PathRegisters(
    UINT32        minValue,
    UINT32        maxValue)
{
    IFERoundClamp11DS4LumaReg*   pDS4Luma      = &m_regCmd.DS4Luma;
    IFERoundClamp11DS4ChromaReg* pDS4Chroma    = &m_regCmd.DS4Chroma;

    pDS4Luma->clamp.bitfields.CH0_CLAMP_MIN          = minValue;
    pDS4Luma->clamp.bitfields.CH0_CLAMP_MAX          = maxValue;
    pDS4Luma->round.bitfields.CH0_INTERLEAVED        = 0;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pDS4Luma->round.bitfields.CH0_ROUND_OFF_BITS     = BitWidthTen - m_bitWidth;
    pDS4Luma->round.bitfields.CH0_ROUNDING_PATTERN   = 0x3;
    pDS4Chroma->clamp.bitfields.CH0_CLAMP_MIN        = minValue;
    pDS4Chroma->clamp.bitfields.CH0_CLAMP_MAX        = maxValue;
    pDS4Chroma->round.bitfields.CH0_INTERLEAVED      = 1;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pDS4Chroma->round.bitfields.CH0_ROUND_OFF_BITS   = BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pDS4Chroma->round.bitfields.CH0_ROUNDING_PATTERN = 0x3;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::ConfigureDS16PathRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::ConfigureDS16PathRegisters(
    UINT32        minValue,
    UINT32        maxValue)
{
    IFERoundClamp11DS16LumaReg*   pDS16Luma     = & m_regCmd.DS16Luma;
    IFERoundClamp11DS16ChromaReg* pDS16Chroma   = &m_regCmd.DS16Chroma;

    pDS16Luma->clamp.bitfields.CH0_CLAMP_MIN          =  minValue;
    pDS16Luma->clamp.bitfields.CH0_CLAMP_MAX          =  maxValue;
    pDS16Luma->round.bitfields.CH0_INTERLEAVED        =  0;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pDS16Luma->round.bitfields.CH0_ROUND_OFF_BITS     =  BitWidthTen - m_bitWidth;
    pDS16Luma->round.bitfields.CH0_ROUNDING_PATTERN   =  0x3;
    pDS16Chroma->clamp.bitfields.CH0_CLAMP_MIN        =  minValue;
    pDS16Chroma->clamp.bitfields.CH0_CLAMP_MAX        =  maxValue;
    pDS16Chroma->round.bitfields.CH0_INTERLEAVED      =  1;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pDS16Chroma->round.bitfields.CH0_ROUND_OFF_BITS   =  BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pDS16Chroma->round.bitfields.CH0_ROUNDING_PATTERN =  0x3;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::ConfigureDisplayPathRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::ConfigureDisplayPathRegisters(
    UINT32        minValue,
    UINT32        maxValue)
{
    IFERoundClamp11DisplayLumaReg*   pFullLuma     = &m_regCmd.displayLuma;
    IFERoundClamp11DisplayChromaReg* pFullChroma   = &m_regCmd.displayChroma;

    pFullLuma->clamp.bitfields.CH0_CLAMP_MIN          = minValue;
    pFullLuma->clamp.bitfields.CH0_CLAMP_MAX          = maxValue;
    pFullLuma->round.bitfields.CH0_INTERLEAVED        = 0;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pFullLuma->round.bitfields.CH0_ROUND_OFF_BITS     = BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pFullLuma->round.bitfields.CH0_ROUNDING_PATTERN   = 0x3;
    pFullChroma->clamp.bitfields.CH0_CLAMP_MIN        = minValue;
    pFullChroma->clamp.bitfields.CH0_CLAMP_MAX        = maxValue;
    pFullChroma->round.bitfields.CH0_INTERLEAVED      = 1;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pFullChroma->round.bitfields.CH0_ROUND_OFF_BITS   = BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pFullChroma->round.bitfields.CH0_ROUNDING_PATTERN = 0x3;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::ConfigureDS4DisplayPathRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::ConfigureDS4DisplayPathRegisters(
    UINT32        minValue,
    UINT32        maxValue)
{
    IFERoundClamp11DisplayDS4LumaReg*   pDS4Luma      = &m_regCmd.displayDS4Luma;
    IFERoundClamp11DisplayDS4ChromaReg* pDS4Chroma    = &m_regCmd.displayDS4Chroma;

    pDS4Luma->clamp.bitfields.CH0_CLAMP_MIN          = minValue;
    pDS4Luma->clamp.bitfields.CH0_CLAMP_MAX          = maxValue;
    pDS4Luma->round.bitfields.CH0_INTERLEAVED        = 0;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pDS4Luma->round.bitfields.CH0_ROUND_OFF_BITS     = BitWidthTen - m_bitWidth;
    pDS4Luma->round.bitfields.CH0_ROUNDING_PATTERN   = 0x3;
    pDS4Chroma->clamp.bitfields.CH0_CLAMP_MIN        = minValue;
    pDS4Chroma->clamp.bitfields.CH0_CLAMP_MAX        = maxValue;
    pDS4Chroma->round.bitfields.CH0_INTERLEAVED      = 1;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pDS4Chroma->round.bitfields.CH0_ROUND_OFF_BITS   = BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pDS4Chroma->round.bitfields.CH0_ROUNDING_PATTERN = 0x3;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::ConfigureDS16DisplayPathRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::ConfigureDS16DisplayPathRegisters(
    UINT32        minValue,
    UINT32        maxValue)
{
    IFERoundClamp11displayDS16LumaReg*   pDS16Luma     = & m_regCmd.displayDS16Luma;
    IFERoundClamp11displayDS16ChromaReg* pDS16Chroma   = &m_regCmd.displayDS16Chroma;

    pDS16Luma->clamp.bitfields.CH0_CLAMP_MIN          =  minValue;
    pDS16Luma->clamp.bitfields.CH0_CLAMP_MAX          =  maxValue;
    pDS16Luma->round.bitfields.CH0_INTERLEAVED        =  0;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pDS16Luma->round.bitfields.CH0_ROUND_OFF_BITS     =  BitWidthTen - m_bitWidth;
    pDS16Luma->round.bitfields.CH0_ROUNDING_PATTERN   =  0x3;
    pDS16Chroma->clamp.bitfields.CH0_CLAMP_MIN        =  minValue;
    pDS16Chroma->clamp.bitfields.CH0_CLAMP_MAX        =  maxValue;
    pDS16Chroma->round.bitfields.CH0_INTERLEAVED      =  1;
    /// @todo (CAMX-666) Update round off bits based on input and output bits per pixel
    pDS16Chroma->round.bitfields.CH0_ROUND_OFF_BITS   =  BitWidthTen - m_bitWidth;
    /// @todo (CAMX-666) Update rounding pattren from Chromatix
    pDS16Chroma->round.bitfields.CH0_ROUNDING_PATTERN =  0x3;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFERoundClamp11::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    UINT32     register1;
    UINT32     numberOfValues1;
    UINT32*    pValues1;
    UINT32     register2;
    UINT32     numberOfValues2;
    UINT32*    pValues2;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        switch (m_modulePath)
        {
            case IFEPipelinePath::FDPath:
                register1       = regIFE_IFE_0_VFE_FD_OUT_Y_CH0_CLAMP_CFG;
                numberOfValues1 = sizeof(IFERoundClamp11FDLumaReg) / RegisterWidthInBytes;
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.FDLuma);

                register2       = regIFE_IFE_0_VFE_FD_OUT_C_CH0_CLAMP_CFG;
                numberOfValues2 = sizeof(IFERoundClamp11FDChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.FDChroma);
                break;

            case IFEPipelinePath::FullPath:
                register1       = regIFE_IFE_0_VFE_FULL_OUT_Y_CH0_CLAMP_CFG;
                numberOfValues1 = (sizeof(IFERoundClamp11FullLumaReg) / RegisterWidthInBytes);
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.fullLuma);

                register2       = regIFE_IFE_0_VFE_FULL_OUT_C_CH0_CLAMP_CFG;
                numberOfValues2 = sizeof(IFERoundClamp11FullChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.fullChroma);
                break;

            case IFEPipelinePath::DS4Path:
                register1       = regIFE_IFE_0_VFE_DS4_Y_CH0_CLAMP_CFG;
                numberOfValues1 = sizeof(IFERoundClamp11DS4LumaReg) / RegisterWidthInBytes;
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.DS4Luma);

                register2       = regIFE_IFE_0_VFE_DS4_C_CH0_CLAMP_CFG;
                numberOfValues2 = sizeof(IFERoundClamp11DS4ChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.DS4Chroma);;
                break;

            case IFEPipelinePath::DS16Path:
                register1       = regIFE_IFE_0_VFE_DS16_Y_CH0_CLAMP_CFG;
                numberOfValues1 = sizeof(IFERoundClamp11DS16LumaReg) / RegisterWidthInBytes;
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.DS16Luma);

                register2       = regIFE_IFE_0_VFE_DS16_C_CH0_CLAMP_CFG;
                numberOfValues2 = sizeof(IFERoundClamp11DS16ChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.DS16Chroma);
                break;

            case IFEPipelinePath::DisplayFullPath:
                register1       = regIFE_IFE_0_VFE_DISP_OUT_Y_CH0_CLAMP_CFG;
                numberOfValues1 = (sizeof(IFERoundClamp11DisplayLumaReg) / RegisterWidthInBytes);
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.displayLuma);

                register2       = regIFE_IFE_0_VFE_DISP_OUT_C_CH0_CLAMP_CFG;
                numberOfValues2 = sizeof(IFERoundClamp11DisplayChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.displayChroma);
                break;

            case IFEPipelinePath::DisplayDS4Path:
                register1       = regIFE_IFE_0_VFE_DISP_DS4_Y_CH0_CLAMP_CFG;
                numberOfValues1 = sizeof(IFERoundClamp11DisplayDS4LumaReg) / RegisterWidthInBytes;
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS4Luma);

                register2       = regIFE_IFE_0_VFE_DISP_DS4_C_CH0_CLAMP_CFG;
                numberOfValues2 = sizeof(IFERoundClamp11DisplayDS4ChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS4Chroma);;
                break;

            case IFEPipelinePath::DisplayDS16Path:
                register1       = regIFE_IFE_0_VFE_DISP_DS16_Y_CH0_CLAMP_CFG;
                numberOfValues1 = sizeof(IFERoundClamp11displayDS16LumaReg) / RegisterWidthInBytes;
                pValues1        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS16Luma);

                register2       = regIFE_IFE_0_VFE_DISP_DS16_C_CH0_CLAMP_CFG;
                numberOfValues2 = sizeof(IFERoundClamp11displayDS16ChromaReg) / RegisterWidthInBytes;
                pValues2        = reinterpret_cast<UINT32*>(&m_regCmd.displayDS16Chroma);
                break;

            default:
                CAMX_ASSERT_ALWAYS_MESSAGE("We would never runinto this case");
                return CamxResultEInvalidState;
                break;
        }

        result = PacketBuilder::WriteRegRange(pCmdBuffer, register1, numberOfValues1, pValues1);

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer, register2, numberOfValues2, pValues2);
        }

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
// IFERoundClamp11::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFERoundClamp11::RunCalculation(
    ISPInputData* pInputData)
{
    UINT32     maxValue = 0;
    UINT32     minValue = 0;

    CAMX_ASSERT((m_bitWidth ==  BitWidthEight) || (m_bitWidth ==  BitWidthTen));

    if (BitWidthEight == m_bitWidth)
    {
        maxValue = Max8BitValue;
    }
    else if (BitWidthTen == m_bitWidth)
    {
        maxValue = Max10BitValue;
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Bad bitWidth %d, defaulting to 8 bits", m_bitWidth);
        maxValue = Max8BitValue;
    }

    /// @todo (CAMX-666) Configure registers specific to path based on object type
    // Full output  path configuration
    if (IFEPipelinePath::FullPath == m_modulePath)
    {
        ConfigureFullPathRegisters(minValue, maxValue);
    }

    // FD output path configuration
    if (IFEPipelinePath::FDPath == m_modulePath)
    {
        ConfigureFDPathRegisters(minValue, maxValue);
    }

    // DS4 output path configuration
    if (IFEPipelinePath::DS4Path == m_modulePath)
    {
        ConfigureDS4PathRegisters(minValue, maxValue);
    }

    // DS16 output path configuration
    if (IFEPipelinePath::DS16Path == m_modulePath)
    {
        ConfigureDS16PathRegisters(minValue, maxValue);
    }

    // Disp output  path configuration
    if (IFEPipelinePath::DisplayFullPath == m_modulePath)
    {
        ConfigureDisplayPathRegisters(minValue, maxValue);
    }

    // DS4 output path configuration
    if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
    {
        ConfigureDS4DisplayPathRegisters(minValue, maxValue);
    }

    // DS16 output path configuration
    if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
    {
        ConfigureDS16DisplayPathRegisters(minValue, maxValue);
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFERoundClamp11::IFERoundClamp11
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFERoundClamp11::IFERoundClamp11(
    IFEModuleCreateData* pCreateData)
{
    m_type         = ISPIQModuleType::IFERoundClamp;
    m_moduleEnable = TRUE;

    m_32bitDMILength = 0;
    m_64bitDMILength = 0;

    m_modulePath = pCreateData->pipelineData.IFEPath;
    m_output     = FullOutput;
    switch (m_modulePath)
    {
        case IFEPipelinePath::FDPath:
            m_output = FDOutput;
            m_cmdLength =
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11FDLumaReg) / RegisterWidthInBytes) +
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11FDChromaReg) / RegisterWidthInBytes);
            break;

        case IFEPipelinePath::FullPath:
            m_output = FullOutput;
            m_cmdLength =
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11FullLumaReg) / RegisterWidthInBytes) +
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11FullChromaReg) / RegisterWidthInBytes);
            break;

        case IFEPipelinePath::DS4Path:
            m_output = DS4Output;
            m_cmdLength =
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11DS4LumaReg) / RegisterWidthInBytes) +
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11DS4ChromaReg) / RegisterWidthInBytes);
            break;

        case IFEPipelinePath::DS16Path:
            m_output = DS16Output;
            m_cmdLength =
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11DS16LumaReg) / RegisterWidthInBytes) +
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11DS16ChromaReg) / RegisterWidthInBytes);
            break;

        case IFEPipelinePath::DisplayFullPath:
            m_output = DisplayFullOutput;
            m_cmdLength =
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11DisplayLumaReg) / RegisterWidthInBytes) +
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11DisplayChromaReg) /
                                                                 RegisterWidthInBytes);
            break;

        case IFEPipelinePath::DisplayDS4Path:
            m_output = DisplayDS4Output;
            m_cmdLength =
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11DisplayDS4LumaReg) /
                                                                 RegisterWidthInBytes) +
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11DisplayDS4ChromaReg) /
                                                                 RegisterWidthInBytes);
            break;

        case IFEPipelinePath::DisplayDS16Path:
            m_output = DisplayDS16Output;
            m_cmdLength =
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11displayDS16LumaReg) /
                                                                 RegisterWidthInBytes) +
                PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERoundClamp11displayDS16ChromaReg) /
                                                                 RegisterWidthInBytes);
            break;

        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Invalid path");
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFERoundClamp11::~IFERoundClamp11
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFERoundClamp11::~IFERoundClamp11()
{
}

CAMX_NAMESPACE_END
