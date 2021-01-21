////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeawbbgstats14.cpp
/// @brief AWBBG Stats14 Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifeawbbgstats14.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult AWBBGStats14::Create(
    IFEStatsModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW AWBBGStats14;
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
// AWBBGStats14::GetDMITable
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID AWBBGStats14::GetDMITable(
    UINT32** ppDMITable)
{
    CAMX_UNREFERENCED_PARAM(ppDMITable);

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL AWBBGStats14::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL        result          = FALSE;
    BGBEConfig* pAWBBGConfig    = &pInputData->pStripeConfig->AWBStatsUpdateData.statsConfig.BGConfig;

    if ((m_AWBBGConfig.horizontalNum                        != pAWBBGConfig->horizontalNum)                         ||
        (m_AWBBGConfig.verticalNum                          != pAWBBGConfig->verticalNum)                           ||
        (m_AWBBGConfig.ROI.left                             != pAWBBGConfig->ROI.left)                              ||
        (m_AWBBGConfig.ROI.top                              != pAWBBGConfig->ROI.top)                               ||
        (m_AWBBGConfig.ROI.width                            != pAWBBGConfig->ROI.width)                             ||
        (m_AWBBGConfig.ROI.height                           != pAWBBGConfig->ROI.height)                            ||
        (m_AWBBGConfig.outputBitDepth                       != pAWBBGConfig->outputBitDepth)                        ||
        (m_AWBBGConfig.outputMode                           != pAWBBGConfig->outputMode)                            ||
        (m_AWBBGConfig.YStatsWeights[0]                     != pAWBBGConfig->YStatsWeights[0])                      ||
        (m_AWBBGConfig.YStatsWeights[1]                     != pAWBBGConfig->YStatsWeights[1])                      ||
        (m_AWBBGConfig.YStatsWeights[2]                     != pAWBBGConfig->YStatsWeights[2])                      ||
        (m_AWBBGConfig.greenType                            != pAWBBGConfig->greenType)                             ||
        (m_AWBBGConfig.channelGainThreshold[ChannelIndexR]  != pAWBBGConfig->channelGainThreshold[ChannelIndexR])   ||
        (m_AWBBGConfig.channelGainThreshold[ChannelIndexGR] != pAWBBGConfig->channelGainThreshold[ChannelIndexGR])  ||
        (m_AWBBGConfig.channelGainThreshold[ChannelIndexB]  != pAWBBGConfig->channelGainThreshold[ChannelIndexB])   ||
        (m_AWBBGConfig.channelGainThreshold[ChannelIndexGB] != pAWBBGConfig->channelGainThreshold[ChannelIndexGB])  ||
        (TRUE                                               == pInputData->forceTriggerUpdate))
    {
        Utils::Memcpy(&m_AWBBGConfig, pAWBBGConfig, sizeof(BGBEConfig));
        result = TRUE;

        CAMX_ASSERT_MESSAGE(MaxAWBBGStatsNum >= (m_AWBBGConfig.horizontalNum * m_AWBBGConfig.verticalNum),
                            "AWBBGStatsNum out of bound");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult AWBBGStats14::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_STATS_AWB_BG_RGN_OFFSET_CFG,
                                              (sizeof(IFEAWBBG14RegionConfig) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd.regionConfig));

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_STATS_AWB_BG_CH_Y_CFG,
                                                  (sizeof(IFE_IFE_0_VFE_STATS_AWB_BG_CH_Y_CFG) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.lumaConfig));
        }

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_STATS_AWB_BG_CFG,
                                                  (sizeof(IFE_IFE_0_VFE_STATS_AWB_BG_CFG) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.AWBBGStatsconfig));
        }

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
// AWBBGStats14::ConfigureRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID AWBBGStats14::ConfigureRegisters()
{
    IFEAWBBG14RegionConfig*               pRegionConfig              = &m_regCmd.regionConfig;
    _ife_ife_0_vfe_stats_awb_bg_ch_y_cfg* pLumaConfigBitfields       = &m_regCmd.lumaConfig.bitfields;
    _ife_ife_0_vfe_stats_awb_bg_cfg*      pAWBBGStatsCOnfigBitfields = &m_regCmd.AWBBGStatsconfig.bitfields;

    // The value offset must be even to ensure correct Bayer pattern.
    pRegionConfig->regionOffset.bitfields.RGN_H_OFFSET  = Utils::FloorUINT32(2, m_AWBBGConfig.ROI.left);
    pRegionConfig->regionOffset.bitfields.RGN_V_OFFSET  = Utils::FloorUINT32(2, m_AWBBGConfig.ROI.top);

    // The value is programmed starting from 0. Program a value of n means (n + 1)
    pRegionConfig->regionNumber.bitfields.RGN_H_NUM = m_AWBBGConfig.horizontalNum -1;
    pRegionConfig->regionNumber.bitfields.RGN_V_NUM = m_AWBBGConfig.verticalNum -1;
    pRegionConfig->regionSize.bitfields.RGN_WIDTH   = m_regionWidth - 1;
    pRegionConfig->regionSize.bitfields.RGN_HEIGHT  = m_regionHeight - 1;
    pRegionConfig->highThreshold0.bitfields.R_MAX   = m_AWBBGConfig.channelGainThreshold[ChannelIndexR];
    pRegionConfig->highThreshold0.bitfields.GR_MAX  = m_AWBBGConfig.channelGainThreshold[ChannelIndexGR];
    pRegionConfig->highThreshold1.bitfields.B_MAX   = m_AWBBGConfig.channelGainThreshold[ChannelIndexB];
    pRegionConfig->highThreshold1.bitfields.GB_MAX  = m_AWBBGConfig.channelGainThreshold[ChannelIndexGB];
    pRegionConfig->lowThreshold0.bitfields.R_MIN    = 0;
    pRegionConfig->lowThreshold0.bitfields.GR_MIN   = 0;
    pRegionConfig->lowThreshold1.bitfields.B_MIN    = 0;
    pRegionConfig->lowThreshold1.bitfields.GB_MIN   = 0;

    /// @todo (CAMX-856) Update luma config support after support from stats
    pLumaConfigBitfields->COEF_A0    = Utils::FloatToQNumber(m_AWBBGConfig.YStatsWeights[0], Q7);
    pLumaConfigBitfields->COEF_A1    = Utils::FloatToQNumber(m_AWBBGConfig.YStatsWeights[1], Q7);
    pLumaConfigBitfields->COEF_A2    = Utils::FloatToQNumber(m_AWBBGConfig.YStatsWeights[2], Q7);
    pLumaConfigBitfields->G_SEL      = m_AWBBGConfig.greenType;
    pLumaConfigBitfields->Y_STATS_EN = (BGBEYStatsEnabled == m_AWBBGConfig.outputMode) ? 1 : 0;

    /// @todo (CAMX-856) Update AWBBG power optimization config after support from stats
    pAWBBGStatsCOnfigBitfields->RGN_SAMPLE_PATTERN = 0xFFFF;
    pAWBBGStatsCOnfigBitfields->SAT_STATS_EN       = (BGBESaturationEnabled == m_AWBBGConfig.outputMode) ? 1 : 0;
    pAWBBGStatsCOnfigBitfields->SHIFT_BITS         = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::AdjustROIParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID AWBBGStats14::AdjustROIParams()
{
    UINT32 regionWidth;
    UINT32 regionHeight;
    BOOL   isAdjusted = FALSE;

    CAMX_ASSERT_MESSAGE(0 != m_AWBBGConfig.horizontalNum, "Invalid horizontalNum");
    CAMX_ASSERT_MESSAGE(0 != m_AWBBGConfig.verticalNum, "Invalid verticalNum");

    CAMX_ASSERT_MESSAGE(0 != m_AWBBGConfig.ROI.width, "Invalid ROI Width");
    CAMX_ASSERT_MESSAGE(0 != m_AWBBGConfig.ROI.height, "Invalid ROI height");

    regionWidth  = m_AWBBGConfig.ROI.width / m_AWBBGConfig.horizontalNum;
    regionHeight = m_AWBBGConfig.ROI.height / m_AWBBGConfig.verticalNum;

    // The value n must be even to ensure correct Bayer pattern
    regionWidth  = Utils::FloorUINT32(2, regionWidth);
    regionHeight = Utils::FloorUINT32(2, regionHeight);

    // Check region minimum and maximum width criteria
    if (regionWidth < AWBBGStats14RegionMinWidth)
    {
        regionWidth                 = AWBBGStats14RegionMinWidth;
        m_AWBBGConfig.horizontalNum = m_AWBBGConfig.ROI.width / regionWidth;
        isAdjusted                  = TRUE;
    }
    else if (regionWidth > AWBBGStats14RegionMaxWidth)
    {
        regionWidth                 = AWBBGStats14RegionMaxWidth;
        m_AWBBGConfig.horizontalNum = m_AWBBGConfig.ROI.width / regionWidth;
        isAdjusted                  = TRUE;
    }

    // Check region minimum and maximum Height criteria
    if (regionHeight < AWBBGStats14RegionMinHeight)
    {
        regionHeight              = AWBBGStats14RegionMinHeight;
        m_AWBBGConfig.verticalNum = m_AWBBGConfig.ROI.height / regionHeight;
        isAdjusted                  = TRUE;
    }
    else if (regionHeight > AWBBGStats14RegionMaxHeight)
    {
        regionHeight              = AWBBGStats14RegionMaxHeight;
        m_AWBBGConfig.verticalNum = m_AWBBGConfig.ROI.height / regionHeight;
        isAdjusted                  = TRUE;
    }

    // Adjust Max Channel threshold values
    if (m_AWBBGConfig.channelGainThreshold[ChannelIndexR] > AWBBGStats14MaxChannelThreshold)
    {
        m_AWBBGConfig.channelGainThreshold[ChannelIndexR] = AWBBGStats14MaxChannelThreshold;
        isAdjusted                                        = TRUE;
    }

    if (m_AWBBGConfig.channelGainThreshold[ChannelIndexB] > AWBBGStats14MaxChannelThreshold)
    {
        m_AWBBGConfig.channelGainThreshold[ChannelIndexB] = AWBBGStats14MaxChannelThreshold;
        isAdjusted                                        = TRUE;
    }

    if (m_AWBBGConfig.channelGainThreshold[ChannelIndexGR] > AWBBGStats14MaxChannelThreshold)
    {
        m_AWBBGConfig.channelGainThreshold[ChannelIndexGR] = AWBBGStats14MaxChannelThreshold;
        isAdjusted                                         = TRUE;
    }

    if (m_AWBBGConfig.channelGainThreshold[ChannelIndexGB] > AWBBGStats14MaxChannelThreshold)
    {
        m_AWBBGConfig.channelGainThreshold[ChannelIndexGB] = AWBBGStats14MaxChannelThreshold;
        isAdjusted                                         = TRUE;
    }

    m_regionWidth  = regionWidth;
    m_regionHeight = regionHeight;
    m_isAdjusted   = isAdjusted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID AWBBGStats14::RunCalculation(
    ISPInputData* pInputData)
{
    AdjustROIParams();

    ConfigureRegisters();

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult AWBBGStats14::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (NULL != pInputData->pStripingInput)
        {
            if (pInputData->pAWBStatsUpdateData->statsConfig.BGConfig.horizontalNum != 0 &&
                pInputData->pAWBStatsUpdateData->statsConfig.BGConfig.verticalNum != 0)
            {

                pInputData->pStripingInput->enableBits.BGAWB = m_moduleEnable;
                pInputData->pStripingInput->stripingInput.bg_awb_enable = static_cast<int16_t>(m_moduleEnable);
                pInputData->pStripingInput->stripingInput.bg_awb_input.bg_enabled = m_moduleEnable;
                pInputData->pStripingInput->stripingInput.bg_awb_input.bg_rgn_h_num =
                    pInputData->pAWBStatsUpdateData->statsConfig.BGConfig.horizontalNum - 1;
                pInputData->pStripingInput->stripingInput.bg_awb_input.bg_rgn_v_num =
                    pInputData->pAWBStatsUpdateData->statsConfig.BGConfig.verticalNum - 1;
                pInputData->pStripingInput->stripingInput.bg_awb_input.bg_rgn_width =
                    (pInputData->pAWBStatsUpdateData->statsConfig.BGConfig.ROI.width /
                        pInputData->pAWBStatsUpdateData->statsConfig.BGConfig.horizontalNum) - 1;
                pInputData->pStripingInput->stripingInput.bg_awb_input.bg_roi_h_offset =
                    pInputData->pAWBStatsUpdateData->statsConfig.BGConfig.ROI.left;
                /// @todo (CAMX-856) Update HDRBE power optimization config after support from stats
                pInputData->pStripingInput->stripingInput.bg_awb_input.bg_sat_output_enable =
                    (BGBESaturationEnabled == pInputData->pAWBStatsUpdateData->statsConfig.BGConfig.outputMode) ? 1 : 0;
                pInputData->pStripingInput->stripingInput.bg_awb_input.bg_Y_output_enable =
                    (BGBEYStatsEnabled == pInputData->pAWBStatsUpdateData->statsConfig.BGConfig.outputMode) ? 1 : 0;
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
// AWBBGStats14::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult AWBBGStats14::ValidateDependenceParams(
    ISPInputData* pInputData)
{
    INT32       inputWidth;
    INT32       inputHeight;
    CropInfo*   pSensorCAMIFCrop = &pInputData->pStripeConfig->CAMIFCrop;
    BGBEConfig* pAWBBGConfig     = &pInputData->pStripeConfig->AWBStatsUpdateData.statsConfig.BGConfig;
    CamxResult  result           = CamxResultSuccess;
    INT32       top              = 0;
    INT32       left             = 0;
    INT32       width            = 0;
    INT32       height           = 0;

    inputWidth  = pSensorCAMIFCrop->lastPixel - pSensorCAMIFCrop->firstPixel + 1;
    inputHeight = pSensorCAMIFCrop->lastLine - pSensorCAMIFCrop->firstLine + 1;

    // CAMIF input width for YUV sensor would be twice that of sensor width, as each pixel accounts for Y and U/V data
    if (TRUE == pInputData->sensorData.isYUV)
    {
        inputWidth >>= 1;
    }
    top  = pAWBBGConfig->ROI.top;
    left = pAWBBGConfig->ROI.left;
    width = pAWBBGConfig->ROI.width;
    height = pAWBBGConfig->ROI.height;

    // Validate ROI from Stats
    if ((left                                              < 0)                               ||
        (top                                               < 0)                               ||
        (width                                            <= 0)                               ||
        (height                                           <= 0)                               ||
        (left + width > inputWidth)                                                           ||
        (top  + height > inputHeight)                                                         ||
        (pAWBBGConfig->ROI.width                          == 0)                               ||
        (pAWBBGConfig->ROI.height                         == 0)                               ||
        (pAWBBGConfig->horizontalNum                      == 0)                               ||
        (pAWBBGConfig->verticalNum                        == 0)                               ||
        (pAWBBGConfig->horizontalNum                      > AWBBGStats15MaxHorizontalregions) ||
        (pAWBBGConfig->verticalNum                        > AWBBGStats15MaxVerticalregions))
    {
        CAMX_LOG_ERROR(CamxLogGroupISP,
                       "ROI %d, %d, %d, %d, horizontalNum %d, verticalNum %d inputWidth %d inputHeight %d ",
                       pAWBBGConfig->ROI.left,
                       pAWBBGConfig->ROI.top,
                       pAWBBGConfig->ROI.width,
                       pAWBBGConfig->ROI.height,
                       pAWBBGConfig->horizontalNum,
                       pAWBBGConfig->verticalNum,
                       inputWidth, inputHeight);
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS();
    }

    /// @todo (CAMX-856) Validate Region skip pattern, after support from stats

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID AWBBGStats14::DumpRegConfig()
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Offset Config  [0x%x]", m_regCmd.regionConfig.regionOffset);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Number Config  [0x%x]", m_regCmd.regionConfig.regionNumber);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Size Config    [0x%x]", m_regCmd.regionConfig.regionSize);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region highThreshold0 [0x%x]", m_regCmd.regionConfig.highThreshold0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region highThreshold1 [0x%x]", m_regCmd.regionConfig.highThreshold1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region lowThreshold0  [0x%x]", m_regCmd.regionConfig.lowThreshold0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region lowThreshold1  [0x%x]", m_regCmd.regionConfig.lowThreshold1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Luma Config           [0x%x]", m_regCmd.lumaConfig);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "AWBBGStatsconfig      [0x%x]", m_regCmd.AWBBGStatsconfig);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult AWBBGStats14::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);
        if ((CamxResultSuccess == result) &&
            (TRUE              == CheckDependenceChange(pInputData)))
        {
            RunCalculation(pInputData);
            result = CreateCmdList(pInputData);
        }

        // Update CAMX metadata with the values for the current frame
        UpdateIFEInternalData(pInputData);
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID AWBBGStats14::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    // Update AWB BG config to metadata, for parser and 3A to consume
    pInputData->pCalculatedData->metadata.AWBBGStatsConfig.AWBBGConfig  = m_AWBBGConfig;
    pInputData->pCalculatedData->metadata.AWBBGStatsConfig.isAdjusted   = m_isAdjusted;
    pInputData->pCalculatedData->metadata.AWBBGStatsConfig.regionWidth  = m_regionWidth;
    pInputData->pCalculatedData->metadata.AWBBGStatsConfig.regionHeight = m_regionHeight;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::AWBBGStats14
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
AWBBGStats14::AWBBGStats14()
{
    m_type         = ISPStatsModuleType::IFEAWBBG;
    m_moduleEnable = TRUE;
    m_cmdLength    =
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEAWBBG14RegionConfig) / RegisterWidthInBytes)              +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFE_IFE_0_VFE_STATS_AWB_BG_CH_Y_CFG) / RegisterWidthInBytes) +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFE_IFE_0_VFE_STATS_AWB_BG_CFG) / RegisterWidthInBytes);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWBBGStats14::~AWBBGStats14
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
AWBBGStats14::~AWBBGStats14()
{
}

CAMX_NAMESPACE_END
