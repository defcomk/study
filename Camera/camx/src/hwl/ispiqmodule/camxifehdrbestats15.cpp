////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifehdrbestats15.cpp
/// @brief HDRBE Stats15 Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifehdrbestats15.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBEStats15::Create(
    IFEStatsModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW HDRBEStats15;
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
// HDRBEStats15::GetDMITable
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBEStats15::GetDMITable(
    UINT32** ppDMITable)
{
    CAMX_UNREFERENCED_PARAM(ppDMITable);

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL HDRBEStats15::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL        result          = FALSE;
    BGBEConfig* pHDRBEConfig    = &pInputData->pStripeConfig->AECStatsUpdateData.statsConfig.BEConfig;

    if ((m_HDRBEConfig.horizontalNum                        != pHDRBEConfig->horizontalNum)                         ||
        (m_HDRBEConfig.verticalNum                          != pHDRBEConfig->verticalNum)                           ||
        (m_HDRBEConfig.ROI.left                             != pHDRBEConfig->ROI.left)                              ||
        (m_HDRBEConfig.ROI.top                              != pHDRBEConfig->ROI.top)                               ||
        (m_HDRBEConfig.ROI.width                            != pHDRBEConfig->ROI.width)                             ||
        (m_HDRBEConfig.ROI.height                           != pHDRBEConfig->ROI.height)                            ||
        (m_HDRBEConfig.outputBitDepth                       != pHDRBEConfig->outputBitDepth)                        ||
        (m_HDRBEConfig.outputMode                           != pHDRBEConfig->outputMode)                            ||
        (m_HDRBEConfig.YStatsWeights[0]                     != pHDRBEConfig->YStatsWeights[0])                      ||
        (m_HDRBEConfig.YStatsWeights[1]                     != pHDRBEConfig->YStatsWeights[1])                      ||
        (m_HDRBEConfig.YStatsWeights[2]                     != pHDRBEConfig->YStatsWeights[2])                      ||
        (m_HDRBEConfig.greenType                            != pHDRBEConfig->greenType)                             ||
        (m_HDRBEConfig.channelGainThreshold[ChannelIndexR]  != pHDRBEConfig->channelGainThreshold[ChannelIndexR])   ||
        (m_HDRBEConfig.channelGainThreshold[ChannelIndexGR] != pHDRBEConfig->channelGainThreshold[ChannelIndexGR])  ||
        (m_HDRBEConfig.channelGainThreshold[ChannelIndexB]  != pHDRBEConfig->channelGainThreshold[ChannelIndexB])   ||
        (m_HDRBEConfig.channelGainThreshold[ChannelIndexGB] != pHDRBEConfig->channelGainThreshold[ChannelIndexGB])  ||
        (TRUE                                               == pInputData->forceTriggerUpdate))
    {
        Utils::Memcpy(&m_HDRBEConfig, pHDRBEConfig, sizeof(BGBEConfig));

        result = TRUE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBEStats15::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_STATS_HDR_BE_RGN_OFFSET_CFG,
                                              (sizeof(IFEHDRBE15RegionConfig) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd.regionConfig));

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_STATS_HDR_BE_CH_Y_CFG,
                                                  (sizeof(IFE_IFE_0_VFE_STATS_HDR_BE_CH_Y_CFG) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.lumaConfig));
        }

        if (CamxResultSuccess == result)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_STATS_HDR_BE_CFG,
                                                  (sizeof(IFE_IFE_0_VFE_STATS_HDR_BE_CFG) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.HDRBEStatsconfig));
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
// HDRBEStats15::ConfigureRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBEStats15::ConfigureRegisters()
{
    IFEHDRBE15RegionConfig*               pRegionConfig             = &m_regCmd.regionConfig;
    IFE_IFE_0_VFE_STATS_HDR_BE_CH_Y_CFG* pLumaConfigBitfields       = &m_regCmd.lumaConfig;
    IFE_IFE_0_VFE_STATS_HDR_BE_CFG*      pHDRBEStatsConfigBitfields = &m_regCmd.HDRBEStatsconfig;

    // The value n must be even for Demosaic tap out and 4 for HDR recon tapout to ensure correct Bayer pattern
    pRegionConfig->regionOffset.bitfields.RGN_H_OFFSET  = Utils::FloorUINT32(m_regionMultipleFactor, m_HDRBEConfig.ROI.left);
    pRegionConfig->regionOffset.bitfields.RGN_V_OFFSET  = Utils::FloorUINT32(m_regionMultipleFactor, m_HDRBEConfig.ROI.top);

    // The value is programmed starting from 0. Program a value of n means (n + 1)
    pRegionConfig->regionNumber.bitfields.RGN_H_NUM = m_HDRBEConfig.horizontalNum -1;
    pRegionConfig->regionNumber.bitfields.RGN_V_NUM = m_HDRBEConfig.verticalNum -1;
    pRegionConfig->regionSize.bitfields.RGN_WIDTH   = m_regionWidth - 1;
    pRegionConfig->regionSize.bitfields.RGN_HEIGHT  = m_regionHeight - 1;
    pRegionConfig->highThreshold0.bitfields.R_MAX   = m_HDRBEConfig.channelGainThreshold[ChannelIndexR];
    pRegionConfig->highThreshold0.bitfields.GR_MAX  = m_HDRBEConfig.channelGainThreshold[ChannelIndexGR];
    pRegionConfig->highThreshold1.bitfields.B_MAX   = m_HDRBEConfig.channelGainThreshold[ChannelIndexB];
    pRegionConfig->highThreshold1.bitfields.GB_MAX  = m_HDRBEConfig.channelGainThreshold[ChannelIndexGB];
    pRegionConfig->lowThreshold0.bitfields.R_MIN    = 0;
    pRegionConfig->lowThreshold0.bitfields.GR_MIN   = 0;
    pRegionConfig->lowThreshold1.bitfields.B_MIN    = 0;
    pRegionConfig->lowThreshold1.bitfields.GB_MIN   = 0;

    pLumaConfigBitfields->bitfields.COEF_A0    = Utils::FloatToQNumber(m_HDRBEConfig.YStatsWeights[0], Q7);
    pLumaConfigBitfields->bitfields.COEF_A1    = Utils::FloatToQNumber(m_HDRBEConfig.YStatsWeights[1], Q7);
    pLumaConfigBitfields->bitfields.COEF_A2    = Utils::FloatToQNumber(m_HDRBEConfig.YStatsWeights[2], Q7);
    pLumaConfigBitfields->bitfields.G_SEL      = m_HDRBEConfig.greenType;
    pLumaConfigBitfields->bitfields.Y_STATS_EN = (BGBEYStatsEnabled == m_HDRBEConfig.outputMode) ? 1 : 0;

    /// @todo (CAMX-856) Update HDRBE power optimization config after support from stats
    pHDRBEStatsConfigBitfields->bitfields.RGN_SAMPLE_PATTERN = 0xFFFF;
    pHDRBEStatsConfigBitfields->bitfields.SAT_STATS_EN       = (BGBESaturationEnabled == m_HDRBEConfig.outputMode) ? 1 : 0;;
    pHDRBEStatsConfigBitfields->bitfields.SHIFT_BITS         = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::AdjustROIParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBEStats15::AdjustROIParams()
{
    UINT32 regionWidth;
    UINT32 regionHeight;
    BOOL   isAdjusted = FALSE;

    CAMX_ASSERT_MESSAGE(0 != m_HDRBEConfig.horizontalNum, "Invalid horizontalNum");
    CAMX_ASSERT_MESSAGE(0 != m_HDRBEConfig.verticalNum, "Invalid verticalNum");

    CAMX_ASSERT_MESSAGE(0 != m_HDRBEConfig.ROI.width, "Invalid ROI Width");
    CAMX_ASSERT_MESSAGE(0 != m_HDRBEConfig.ROI.height, "Invalid ROI height");

    regionWidth  = m_HDRBEConfig.ROI.width / m_HDRBEConfig.horizontalNum;
    regionHeight = m_HDRBEConfig.ROI.height / m_HDRBEConfig.verticalNum;

    // The value n must be even for Demosaic tap out and 4 for HDR recon tapout to ensure correct Bayer pattern
    regionWidth  = Utils::FloorUINT32(m_regionMultipleFactor, regionWidth);
    regionHeight = Utils::FloorUINT32(m_regionMultipleFactor, regionHeight);

    // Check region minimum and maximum width criteria
    if (regionWidth < HDRBEStats15RegionMinWidth)
    {
        regionWidth                 = HDRBEStats15RegionMinWidth;
        m_HDRBEConfig.ROI.width     = m_HDRBEConfig.horizontalNum * regionWidth;
        isAdjusted                  = TRUE;
    }
    else if (regionWidth > HDRBEStats15RegionMaxWidth)
    {
        regionWidth                 = HDRBEStats15RegionMaxWidth;
        m_HDRBEConfig.ROI.width     = m_HDRBEConfig.horizontalNum * regionWidth;
        isAdjusted                  = TRUE;
    }

    // Check region minimum and maximum Height criteria
    if (regionHeight < HDRBEStats15RegionMinHeight)
    {
        regionHeight              = HDRBEStats15RegionMinHeight;
        m_HDRBEConfig.ROI.height  = m_HDRBEConfig.verticalNum * regionHeight;
        isAdjusted                = TRUE;
    }
    else if (regionHeight > HDRBEStats15RegionMaxHeight)
    {
        regionHeight              = HDRBEStats15RegionMaxHeight;
        m_HDRBEConfig.ROI.height  = m_HDRBEConfig.verticalNum * regionHeight;
        isAdjusted                = TRUE;
    }

    // Adjust Max Channel threshold values
    if (m_HDRBEConfig.channelGainThreshold[ChannelIndexR] > HDRBEStats15MaxChannelThreshold)
    {
        m_HDRBEConfig.channelGainThreshold[ChannelIndexR] = HDRBEStats15MaxChannelThreshold;
        isAdjusted                                        = TRUE;
    }

    if (m_HDRBEConfig.channelGainThreshold[ChannelIndexB] > HDRBEStats15MaxChannelThreshold)
    {
        m_HDRBEConfig.channelGainThreshold[ChannelIndexB] = HDRBEStats15MaxChannelThreshold;
        isAdjusted                                        = TRUE;
    }

    if (m_HDRBEConfig.channelGainThreshold[ChannelIndexGR] > HDRBEStats15MaxChannelThreshold)
    {
        m_HDRBEConfig.channelGainThreshold[ChannelIndexGR] = HDRBEStats15MaxChannelThreshold;
        isAdjusted                                         = TRUE;
    }

    if (m_HDRBEConfig.channelGainThreshold[ChannelIndexGB] > HDRBEStats15MaxChannelThreshold)
    {
        m_HDRBEConfig.channelGainThreshold[ChannelIndexGB] = HDRBEStats15MaxChannelThreshold;
        isAdjusted                                         = TRUE;
    }

    m_regionWidth  = regionWidth;
    m_regionHeight = regionHeight;
    m_isAdjusted   = isAdjusted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBEStats15::RunCalculation(
    ISPInputData* pInputData)
{
    CAMX_UNREFERENCED_PARAM(pInputData);

    AdjustROIParams();

    /// @todo (CAMX-887) Add Black level subtraction and scaler module support for tapping HDR BE stats before HDR recon module
    ConfigureRegisters();

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBEStats15::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (NULL != pInputData->pStripingInput)
        {
            if (pInputData->pAECStatsUpdateData->statsConfig.BEConfig.horizontalNum != 0 &&
                pInputData->pAECStatsUpdateData->statsConfig.BEConfig.verticalNum != 0)
            {

                pInputData->pStripingInput->enableBits.BE = m_moduleEnable;
                pInputData->pStripingInput->stripingInput.be_enable = static_cast<int16_t>(m_moduleEnable);
                pInputData->pStripingInput->stripingInput.be_input.be_enable = m_moduleEnable;
                pInputData->pStripingInput->stripingInput.be_input.be_rgn_h_num =
                    pInputData->pAECStatsUpdateData->statsConfig.BEConfig.horizontalNum - 1;
                pInputData->pStripingInput->stripingInput.be_input.be_rgn_v_num =
                    pInputData->pAECStatsUpdateData->statsConfig.BEConfig.verticalNum - 1;
                pInputData->pStripingInput->stripingInput.be_input.be_rgn_width =
                    (pInputData->pAECStatsUpdateData->statsConfig.BEConfig.ROI.width /
                        pInputData->pAECStatsUpdateData->statsConfig.BEConfig.horizontalNum) - 1;
                pInputData->pStripingInput->stripingInput.be_input.be_roi_h_offset =
                    pInputData->pAECStatsUpdateData->statsConfig.BEConfig.ROI.left;
                pInputData->pStripingInput->stripingInput.tappingPoint_be = 1;
                /// @todo (CAMX-856) Update HDRBE power optimization config after support from stats
                pInputData->pStripingInput->stripingInput.be_input.be_sat_output_enable =
                    (BGBESaturationEnabled == pInputData->pAECStatsUpdateData->statsConfig.BEConfig.outputMode) ? 1 : 0;

                pInputData->pStripingInput->stripingInput.be_input.be_Y_output_enable =
                    (BGBEYStatsEnabled == pInputData->pAECStatsUpdateData->statsConfig.BEConfig.outputMode) ? 1 : 0;
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
// HDRBEStats15::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBEStats15::ValidateDependenceParams(
    ISPInputData* pInputData)
{
    INT32       inputWidth;
    INT32       inputHeight;
    CropInfo*   pSensorCAMIFCrop = &pInputData->pStripeConfig->CAMIFCrop;
    BGBEConfig* pHDRBEConfig     = &pInputData->pStripeConfig->AECStatsUpdateData.statsConfig.BEConfig;
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

    top    = pHDRBEConfig->ROI.top;
    left   = pHDRBEConfig->ROI.left;
    width  = pHDRBEConfig->ROI.width;
    height = pHDRBEConfig->ROI.height;

    // Validate ROI from Stats
    if (((left + width) > inputWidth)                                                         ||
        ((top + height) > inputHeight)                                                        ||
        (width                                            <= 0)                               ||
        (height                                           <= 0)                               ||
        (top                                              <  0)                               ||
        (left                                             <  0)                               ||
        (pHDRBEConfig->horizontalNum                      == 0)                               ||
        (pHDRBEConfig->verticalNum                        == 0)                               ||
        (pHDRBEConfig->horizontalNum                      > HDRBEStats15MaxHorizontalregions) ||
        (pHDRBEConfig->verticalNum                        > HDRBEStats15MaxVerticalregions))
    {
        CAMX_LOG_ERROR(CamxLogGroupISP,
                       "Invalid config: ROI %d, %d, %d, %d, horizontalNum %d, verticalNum %d input [%d %d]",
                       pHDRBEConfig->ROI.left,
                       pHDRBEConfig->ROI.top,
                       pHDRBEConfig->ROI.width,
                       pHDRBEConfig->ROI.height,
                       pHDRBEConfig->horizontalNum,
                       pHDRBEConfig->verticalNum,
                       inputWidth,
                       inputHeight);
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS();
    }

    /// @todo (CAMX-856) Validate Region skip pattern, after support from stats
    /// @todo (CAMX-887) Bandwidth check to avoid overflow

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBEStats15::DumpRegConfig()
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Offset Config  [0x%x]", m_regCmd.regionConfig.regionOffset);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Number Config  [0x%x]", m_regCmd.regionConfig.regionNumber);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Size Config    [0x%x]", m_regCmd.regionConfig.regionSize);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region highThreshold0 [0x%x]", m_regCmd.regionConfig.highThreshold0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region highThreshold1 [0x%x]", m_regCmd.regionConfig.highThreshold1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region lowThreshold0  [0x%x]", m_regCmd.regionConfig.lowThreshold0);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region lowThreshold1  [0x%x]", m_regCmd.regionConfig.lowThreshold1);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Luma Config           [0x%x]", m_regCmd.lumaConfig);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "HDRBEStatsconfig      [0x%x]", m_regCmd.HDRBEStatsconfig);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBEStats15::Execute(
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
// HDRBEStats15::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBEStats15::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    IFE_IFE_0_VFE_STATS_CFG* pStatsConfig = &pInputData->pCalculatedData->moduleEnable.statsConfig;

    pStatsConfig->bitfields.HDR_BE_FIELD_SEL   = m_fieldSelect;
    pStatsConfig->bitfields.HDR_STATS_SITE_SEL = m_HDRBEStatsTapout;

    // Update HDR BE config Info for internal metadata
    pInputData->pCalculatedData->metadata.HDRBEStatsConfig.HDRBEConfig  = m_HDRBEConfig;
    pInputData->pCalculatedData->metadata.HDRBEStatsConfig.isAdjusted   = m_isAdjusted;
    pInputData->pCalculatedData->metadata.HDRBEStatsConfig.regionWidth  = m_regionWidth;
    pInputData->pCalculatedData->metadata.HDRBEStatsConfig.regionHeight = m_regionHeight;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::HDRBEStats15
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
HDRBEStats15::HDRBEStats15()
{
    m_type         = ISPStatsModuleType::IFEHDRBE;
    m_moduleEnable = TRUE;
    m_cmdLength    =
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEHDRBE15RegionConfig) / RegisterWidthInBytes)              +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFE_IFE_0_VFE_STATS_HDR_BE_CH_Y_CFG) / RegisterWidthInBytes) +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFE_IFE_0_VFE_STATS_HDR_BE_CFG) / RegisterWidthInBytes);

    /// @todo (CAMX-887) Support HDRBE stats tapout at HDR Recon module, Hardcode to tap out at Demosaic for now.
    m_HDRBEStatsTapout     = TapoutDemosaic;
    m_regionMultipleFactor = MultipleFactorTwo;
    m_fieldSelect          = FieldSelectAllLines;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBEStats15::~HDRBEStats15
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
HDRBEStats15::~HDRBEStats15()
{
}

CAMX_NAMESPACE_END
