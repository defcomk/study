////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifetintlessbgstats15.cpp
/// @brief Tintless BG Stats v1.5 Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifetintlessbgstats15.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult TintlessBGStats15::Create(
    IFEStatsModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW TintlessBGStats15;
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
// TintlessBGStats15::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL TintlessBGStats15::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL        result            = FALSE;
    BGBEConfig* pTintlessBGConfig = &pInputData->pStripeConfig->AECStatsUpdateData.statsConfig.TintlessBGConfig;


    if ((m_tintlessBGConfig.tintlessBGConfig.horizontalNum != pTintlessBGConfig->horizontalNum)                        ||
        (m_tintlessBGConfig.tintlessBGConfig.verticalNum   != pTintlessBGConfig->verticalNum)                          ||
        (m_tintlessBGConfig.tintlessBGConfig.ROI.left      != pTintlessBGConfig->ROI.left)                             ||
        (m_tintlessBGConfig.tintlessBGConfig.ROI.top       != pTintlessBGConfig->ROI.top)                              ||
        (m_tintlessBGConfig.tintlessBGConfig.ROI.width     != pTintlessBGConfig->ROI.width)                            ||
        (m_tintlessBGConfig.tintlessBGConfig.ROI.height    != pTintlessBGConfig->ROI.height)                           ||
        (m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexR]
                                                           != pTintlessBGConfig->channelGainThreshold[ChannelIndexR])  ||
        (m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGR]
                                                           != pTintlessBGConfig->channelGainThreshold[ChannelIndexGR]) ||
        (m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexB]
                                                           != pTintlessBGConfig->channelGainThreshold[ChannelIndexB])  ||
        (m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGB]
                                                           != pTintlessBGConfig->channelGainThreshold[ChannelIndexGB]) ||
        (TRUE                                              == pInputData->forceTriggerUpdate))
    {
        m_tintlessBGConfig.tintlessBGConfig.horizontalNum = pTintlessBGConfig->horizontalNum;
        m_tintlessBGConfig.tintlessBGConfig.verticalNum   = pTintlessBGConfig->verticalNum;
        m_tintlessBGConfig.tintlessBGConfig.ROI.left      = pTintlessBGConfig->ROI.left;
        m_tintlessBGConfig.tintlessBGConfig.ROI.top       = pTintlessBGConfig->ROI.top;
        m_tintlessBGConfig.tintlessBGConfig.ROI.height    = pTintlessBGConfig->ROI.height;
        m_tintlessBGConfig.tintlessBGConfig.ROI.width     = pTintlessBGConfig->ROI.width;
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexR]  =
            pTintlessBGConfig->channelGainThreshold[ChannelIndexR];
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGR] =
            pTintlessBGConfig->channelGainThreshold[ChannelIndexGR];
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexB]  =
            pTintlessBGConfig->channelGainThreshold[ChannelIndexB];
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGB] =
            pTintlessBGConfig->channelGainThreshold[ChannelIndexGB];

        result = TRUE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult TintlessBGStats15::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;
    CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_STATS_AEC_BG_RGN_OFFSET_CFG,
                                          (sizeof(IFETintlessBG15RegionConfig) / RegisterWidthInBytes),
                                          reinterpret_cast<UINT32*>(&m_regCmd.regionConfig));

    if (CamxResultSuccess == result)
    {
        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_STATS_AEC_BG_CH_Y_CFG,
                                              (sizeof(IFE_IFE_0_VFE_STATS_AEC_BG_CH_Y_CFG) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd.lumaConfig));
    }

    if (CamxResultSuccess == result)
    {
        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_STATS_AEC_BG_CFG,
                                              (sizeof(IFE_IFE_0_VFE_STATS_AEC_BG_CFG) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd.tintlessBGStatsconfig));
    }

    if (CamxResultSuccess != result)
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::ConfigureRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID TintlessBGStats15::ConfigureRegisters(
    UINT32 regionWidth,
    UINT32 regionHeight)
{
    IFETintlessBG15RegionConfig*         pRegionConfig          = &m_regCmd.regionConfig;
    IFE_IFE_0_VFE_STATS_AEC_BG_CH_Y_CFG* pLumaConfig            = &m_regCmd.lumaConfig;
    IFE_IFE_0_VFE_STATS_AEC_BG_CFG*      pTintlessBGStatsConfig = &m_regCmd.tintlessBGStatsconfig;

    // The value offset must be even to ensure correct Bayer pattern.
    pRegionConfig->regionOffset.bitfields.RGN_H_OFFSET = Utils::FloorUINT32(2, m_tintlessBGConfig.tintlessBGConfig.ROI.left);
    pRegionConfig->regionOffset.bitfields.RGN_V_OFFSET = Utils::FloorUINT32(2, m_tintlessBGConfig.tintlessBGConfig.ROI.top);

    // The value is programmed starting from 0. Program a value of n means (n + 1)
    pRegionConfig->regionNumber.bitfields.RGN_H_NUM = m_tintlessBGConfig.tintlessBGConfig.horizontalNum - 1;
    pRegionConfig->regionNumber.bitfields.RGN_V_NUM = m_tintlessBGConfig.tintlessBGConfig.verticalNum - 1;
    pRegionConfig->regionSize.bitfields.RGN_WIDTH   = regionWidth - 1;
    pRegionConfig->regionSize.bitfields.RGN_HEIGHT  = regionHeight - 1;
    pRegionConfig->highThreshold0.bitfields.R_MAX   = m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexR];
    pRegionConfig->highThreshold0.bitfields.GR_MAX  = m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGR];
    pRegionConfig->highThreshold1.bitfields.B_MAX   = m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexB];
    pRegionConfig->highThreshold1.bitfields.GB_MAX  = m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGB];
    pRegionConfig->lowThreshold0.bitfields.R_MIN    = 0;
    pRegionConfig->lowThreshold0.bitfields.GR_MIN   = 0;
    pRegionConfig->lowThreshold1.bitfields.B_MIN    = 0;
    pRegionConfig->lowThreshold1.bitfields.GB_MIN   = 0;

    /// @todo (CAMX-856) Update luma config support after support from stats
    pLumaConfig->bitfields.COEF_A0    = 0;
    pLumaConfig->bitfields.COEF_A1    = 0;
    pLumaConfig->bitfields.COEF_A2    = 0;
    pLumaConfig->bitfields.G_SEL      = 0;
    pLumaConfig->bitfields.Y_STATS_EN = 0;

    /// @todo (CAMX-856) Update TintlessBG power optimization config after support from stats
    pTintlessBGStatsConfig->bitfields.RGN_SAMPLE_PATTERN = 0xFFFF;
    pTintlessBGStatsConfig->bitfields.SAT_STATS_EN       = 0;
    pTintlessBGStatsConfig->bitfields.SHIFT_BITS         = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::AdjustROIParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID TintlessBGStats15::AdjustROIParams(
    UINT32* pRegionWidth,
    UINT32* pRegionHeight)
{
    UINT32 regionWidth;
    UINT32 regionHeight;

    CAMX_ASSERT_MESSAGE(NULL != pRegionWidth, "Invalid input pointer");
    CAMX_ASSERT_MESSAGE(NULL != pRegionHeight, "Invalid input pointer");

    regionWidth  = (m_tintlessBGConfig.tintlessBGConfig.ROI.width / m_tintlessBGConfig.tintlessBGConfig.horizontalNum);
    regionHeight = (m_tintlessBGConfig.tintlessBGConfig.ROI.height / m_tintlessBGConfig.tintlessBGConfig.verticalNum);

    m_tintlessBGConfig.isAdjusted                      = FALSE;
    m_tintlessBGConfig.tintlessBGConfig.outputBitDepth = IFEPipelineBitWidth;

    // The value n must be even to ensure correct Bayer pattern
    regionWidth  = Utils::FloorUINT32(2, regionWidth);
    regionHeight = Utils::FloorUINT32(2, regionHeight);

    // Check region minimum width criteria
    if (TintlessBGStats15RegionMinWidth > regionWidth)
    {
        regionWidth                                       = TintlessBGStats15RegionMinWidth;
        m_tintlessBGConfig.tintlessBGConfig.horizontalNum = (m_tintlessBGConfig.tintlessBGConfig.ROI.width / regionWidth);
        m_tintlessBGConfig.isAdjusted                     = TRUE;
    }
    else if (TintlessBGStats15RegionMaxWidth < regionWidth)
    {
        regionWidth                                       = TintlessBGStats15RegionMaxWidth;
        m_tintlessBGConfig.tintlessBGConfig.horizontalNum = (m_tintlessBGConfig.tintlessBGConfig.ROI.width / regionWidth);
        m_tintlessBGConfig.isAdjusted                     = TRUE;
    }
    // Check region minimum Height criteria
    if (TintlessBGStats15RegionMinHeight > regionHeight)
    {
        regionHeight                                    = TintlessBGStats15RegionMinHeight;
        m_tintlessBGConfig.tintlessBGConfig.verticalNum = (m_tintlessBGConfig.tintlessBGConfig.ROI.height / regionHeight);
        m_tintlessBGConfig.isAdjusted                   = TRUE;
    }
    else if (TintlessBGStats15RegionMaxHeight < regionHeight)
    {
        regionHeight                                    = TintlessBGStats15RegionMaxHeight;
        m_tintlessBGConfig.tintlessBGConfig.verticalNum = (m_tintlessBGConfig.tintlessBGConfig.ROI.height / regionHeight);
        m_tintlessBGConfig.isAdjusted                   = TRUE;
    }

    CAMX_ASSERT(TintlessBGStats15RegionMaxWidth       > regionWidth);
    CAMX_ASSERT(TintlessBGStats15RegionMaxHeight      > regionHeight);
    CAMX_ASSERT(TintlessBGStats15RegionMinHeight      < regionHeight);
    CAMX_ASSERT(TintlessBGStats15RegionMinWidth       < regionWidth);
    CAMX_ASSERT(TintlessBGStats15MaxVerticalregions   >= m_tintlessBGConfig.tintlessBGConfig.verticalNum);
    CAMX_ASSERT(TintlessBGStats15MaxHorizontalregions >= m_tintlessBGConfig.tintlessBGConfig.horizontalNum);
    CAMX_ASSERT(0                                     != m_tintlessBGConfig.tintlessBGConfig.verticalNum);
    CAMX_ASSERT(0                                     != m_tintlessBGConfig.tintlessBGConfig.horizontalNum);
    // Adjust Max Channel threshold values
    if (TintlessBGStats15MaxChannelThreshold < m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexR])
    {
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexR] = TintlessBGStats15MaxChannelThreshold;
        m_tintlessBGConfig.isAdjusted                                           = TRUE;
    }

    if (TintlessBGStats15MaxChannelThreshold < m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexB])
    {
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexB] = TintlessBGStats15MaxChannelThreshold;
        m_tintlessBGConfig.isAdjusted                                           = TRUE;
    }

    if (TintlessBGStats15MaxChannelThreshold < m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGR])
    {
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGR] = TintlessBGStats15MaxChannelThreshold;
        m_tintlessBGConfig.isAdjusted                                            = TRUE;
    }

    if (TintlessBGStats15MaxChannelThreshold < m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGB])
    {
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGB] = TintlessBGStats15MaxChannelThreshold;
        m_tintlessBGConfig.isAdjusted                                            = TRUE;
    }

    *pRegionWidth  = regionWidth;
    *pRegionHeight = regionHeight;

    regionWidth = Utils::FloorUINT32(2, regionWidth);
    regionHeight = Utils::FloorUINT32(2, regionHeight);

    m_tintlessBGConfig.tintlessBGConfig.regionHeight = regionHeight;
    m_tintlessBGConfig.tintlessBGConfig.regionWidth  = regionWidth;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID TintlessBGStats15::RunCalculation(
    ISPInputData* pInputData)
{
    CAMX_UNREFERENCED_PARAM(pInputData);

    UINT32 regionWidth;
    UINT32 regionHeight;

    AdjustROIParams(&regionWidth, &regionHeight);

    ConfigureRegisters(regionWidth, regionHeight);

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID TintlessBGStats15::UpdateIFEInternalData(
    ISPInputData* pInputData
    ) const
{
    pInputData->pCalculatedData->metadata.tintlessBGStats                        = m_tintlessBGConfig;
    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.regionWidth    =
        m_tintlessBGConfig.tintlessBGConfig.regionWidth;
    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.regionHeight   =
        m_tintlessBGConfig.tintlessBGConfig.regionHeight;
    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.outputBitDepth = IFEPipelineBitWidth;

    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.channelGainThreshold[ChannelIndexR] =
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexR];
    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.channelGainThreshold[ChannelIndexB] =
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexB];
    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.channelGainThreshold[ChannelIndexGR] =
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGR];
    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.channelGainThreshold[ChannelIndexGB] =
        m_tintlessBGConfig.tintlessBGConfig.channelGainThreshold[ChannelIndexGB];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult TintlessBGStats15::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (NULL != pInputData->pStripingInput)
        {
            if (pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.horizontalNum != 0 &&
                pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.verticalNum != 0)
            {
                pInputData->pStripingInput->enableBits.BGTintless = m_moduleEnable;
                pInputData->pStripingInput->stripingInput.bg_tintless_enable =
                    static_cast<int16_t>(m_moduleEnable);
                pInputData->pStripingInput->stripingInput.bg_tintless_input.bg_enabled = m_moduleEnable;
                pInputData->pStripingInput->stripingInput.bg_tintless_input.bg_rgn_h_num =
                    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.horizontalNum - 1;
                pInputData->pStripingInput->stripingInput.bg_tintless_input.bg_rgn_v_num =
                    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.verticalNum - 1;
                pInputData->pStripingInput->stripingInput.bg_tintless_input.bg_rgn_width =
                    (pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.ROI.width /
                        pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.horizontalNum) - 1;
                pInputData->pStripingInput->stripingInput.bg_tintless_input.bg_roi_h_offset =
                    pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.ROI.left;
                pInputData->pStripingInput->stripingInput.bg_tintless_input.bg_Y_output_enable =
                    (BGBEYStatsEnabled == pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.outputMode) ? 1 : 0;
                pInputData->pStripingInput->stripingInput.bg_tintless_input.bg_sat_output_enable =
                    (BGBESaturationEnabled ==
                        pInputData->pAECStatsUpdateData->statsConfig.TintlessBGConfig.outputMode) ? 1 : 0;
                pInputData->pStripingInput->stripingInput.bg_tintless_input.bg_region_sampling = 0xFFFF;
            }
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
// TintlessBGStats15::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult TintlessBGStats15::ValidateDependenceParams(
    ISPInputData* pInputData
    ) const
{
    UINT32      inputWidth;
    UINT32      inputHeight;
    CropInfo*   pSensorCAMIFCrop  = NULL;
    BGBEConfig* pTintlessBGConfig = NULL;
    CamxResult  result            = CamxResultSuccess;

    CAMX_ASSERT(NULL != pInputData);
    CAMX_ASSERT(NULL != pInputData->pStripeConfig);

    pSensorCAMIFCrop  = &pInputData->pStripeConfig->CAMIFCrop;
    pTintlessBGConfig = &pInputData->pStripeConfig->AECStatsUpdateData.statsConfig.TintlessBGConfig;

    inputWidth  = (pSensorCAMIFCrop->lastPixel - pSensorCAMIFCrop->firstPixel + 1);
    inputHeight = (pSensorCAMIFCrop->lastLine - pSensorCAMIFCrop->firstLine + 1);

    // CAMIF input width for YUV sensor would be twice that of sensor width, as each pixel accounts for Y and U/V data
    if (TRUE == pInputData->sensorData.isYUV)
    {
        inputWidth >>= 1;
    }

    // Validate ROI from Stats
    if (((pTintlessBGConfig->ROI.left + pTintlessBGConfig->ROI.width) > inputWidth)                        ||
        ((pTintlessBGConfig->ROI.top + pTintlessBGConfig->ROI.height) > inputHeight)                       ||
        (0                                                            == pTintlessBGConfig->ROI.width)     ||
        (0                                                            == pTintlessBGConfig->ROI.height)    ||
        (0                                                            == pTintlessBGConfig->horizontalNum) ||
        (0                                                            == pTintlessBGConfig->verticalNum)   ||
        (TintlessBGStats15MaxHorizontalregions                        <  pTintlessBGConfig->horizontalNum) ||
        (TintlessBGStats15MaxVerticalregions                          <  pTintlessBGConfig->verticalNum))
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "ROI %d, %d, %d, %d, horizontalNum %d, verticalNum %d",
                       pTintlessBGConfig->ROI.left,
                       pTintlessBGConfig->ROI.top,
                       pTintlessBGConfig->ROI.width,
                       pTintlessBGConfig->ROI.height,
                       pTintlessBGConfig->horizontalNum,
                       pTintlessBGConfig->verticalNum);
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS();
    }

    /// @todo (CAMX-856) Validate Region skip pattern, after support from stats

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID TintlessBGStats15::DumpRegConfig() const
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Offset Config  [0x%x]", m_regCmd.regionConfig.regionOffset);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Number Config  [0x%x]", m_regCmd.regionConfig.regionNumber);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Size Config    [0x%x]", m_regCmd.regionConfig.regionSize);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region highThreshold0 [0x%x]", m_regCmd.regionConfig.highThreshold0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region highThreshold1 [0x%x]", m_regCmd.regionConfig.highThreshold1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region lowThreshold0  [0x%x]", m_regCmd.regionConfig.lowThreshold0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region lowThreshold1  [0x%x]", m_regCmd.regionConfig.lowThreshold1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Luma Config           [0x%x]", m_regCmd.lumaConfig);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "tintlessBGStatsconfig [0x%x]", m_regCmd.tintlessBGStatsconfig);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult TintlessBGStats15::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);
        if (CamxResultSuccess == result)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                RunCalculation(pInputData);
                CAMX_ASSERT(NULL != pInputData->pCmdBuffer);
                result = CreateCmdList(pInputData);
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
// TintlessBGStats15::TintlessBGStats15
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TintlessBGStats15::TintlessBGStats15()
{
    m_type         = ISPStatsModuleType::IFETintlessBG;
    m_moduleEnable = TRUE;
    m_cmdLength    =
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFETintlessBG15RegionConfig) / RegisterWidthInBytes)         +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFE_IFE_0_VFE_STATS_AEC_BG_CH_Y_CFG) / RegisterWidthInBytes) +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFE_IFE_0_VFE_STATS_AEC_BG_CFG) / RegisterWidthInBytes);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TintlessBGStats15::~TintlessBGStats15
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TintlessBGStats15::~TintlessBGStats15()
{
}

CAMX_NAMESPACE_END
