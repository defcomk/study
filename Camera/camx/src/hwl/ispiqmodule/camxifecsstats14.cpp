////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifecsstats14.cpp
/// @brief CS Stats v1.4 Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifecsstats14.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSStats14::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CSStats14::Create(
    IFEStatsModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW CSStats14;
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
// CSStats14::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CSStats14::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT_MESSAGE((NULL != pInputData->pCmdBuffer), "Invalid Input pointer!");

    CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_STATS_CS_RGN_OFFSET_CFG,
                                          (sizeof(IFECS14RegionConfig) / RegisterWidthInBytes),
                                          reinterpret_cast<UINT32*>(&m_regCmd.regionConfig));

    CAMX_ASSERT_MESSAGE((CamxResultSuccess == result), "Failed to write command buffer");

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSStats14::AdjustHorizontalRegionNumber
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 CSStats14::AdjustHorizontalRegionNumber(
    UINT32  regionHNum,
    UINT32  divider,
    UINT32  minRemainder)
{
    UINT32 remainder = regionHNum % divider;

    if ((regionHNum > minRemainder) && (remainder < minRemainder))
    {
        remainder = remainder + 1;
    }
    else
    {
        remainder = 0;
    }

    return (regionHNum - remainder);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSStats14::ConfigureRegs
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CSStats14::ConfigureRegs(
    ISPInputData* pInputData)
{
    UINT32                  regionHorizNum          = 0;
    UINT32                  regionVertNum           = 0;
    UINT32                  regionHeight            = 0;
    UINT32                  regionWidth             = 0;
    UINT32                  regionHorizOffset       = 0;
    UINT32                  regionVertOffset        = 0;
    UINT32                  effectiveFrameWidth     = 0;
    UINT32                  effectiveFrameHeight    = 0;
    UINT32                  minROIWidth             = 0;
    UINT32                  minROIHeight            = 0;
    IFECS14RegionConfig*    pRegionConfig           = &m_regCmd.regionConfig;

    // Minimum ROI width - min horizontal regions * minimum region width
    minROIWidth = CSStats14MinHorizRegions * CSStats14MinRegionWidth;
    // Minimum ROI height - min horizontal regions * minimum region height
    minROIHeight = CSStats14MinVertRegions * 1;

    // Configure region offset
    // Horizontal offset - minimum is 0 and maximum is image_width - 1
    regionHorizOffset = m_CSConfig.CSConfig.statsHOffset;
    regionHorizOffset = Utils::ClampUINT32(regionHorizOffset, 0, (m_CAMIFWidth - minROIWidth) - 1);

    // Vertical offset - minimum is 0 and maximum is image_height - 1
    regionVertOffset = m_CSConfig.CSConfig.statsVOffset;
    regionVertOffset = Utils::ClampUINT32(regionVertOffset, 0, (m_CAMIFHeight - minROIHeight) - 1);

    effectiveFrameWidth     = m_CAMIFWidth - regionHorizOffset;
    effectiveFrameHeight    = m_CAMIFHeight - regionVertOffset;

    // Setting Horizontal and Vertical region numbers
    regionHorizNum = m_CSConfig.CSConfig.statsHNum;
    if ((0 == regionHorizNum) || (regionHorizNum > CSStats14MaxHorizRegions))
    {
        regionHorizNum = CSStats14MaxHorizRegions;
    }

    regionVertNum = m_CSConfig.CSConfig.statsVNum;
    if ((0 == regionVertNum) || (regionVertNum > CSStats14MaxVertRegions))
    {
        regionVertNum = CSStats14MaxVertRegions;
    }

    // Region width - minimum is 2 pixels and maximum is 4 pixels
    // Get the ceiling value for regionWidth
    regionWidth = m_CSConfig.CSConfig.statsRgnWidth;
    if (0 == regionWidth)
    {
        regionWidth = (effectiveFrameWidth + regionHorizNum - 1) / regionHorizNum;
        // Adjust horizontal regions based on new regionWidth again
        regionHorizNum = effectiveFrameWidth / Utils::MaxUINT32(regionWidth, 1);
    }

    // Number of lines for a given region - minimum is 1 and maximum is image_height lines
    regionHeight = m_CSConfig.CSConfig.statsRgnHeight;
    if (0 == regionHeight)
    {
        regionHeight = effectiveFrameHeight / Utils::MaxUINT32(regionVertNum, 1);
    }

    // Also clamp parameters to Min and Max of Hardware limitations
    regionWidth     = Utils::ClampUINT32(regionWidth, CSStats14MinRegionWidth, CSStats14MaxRegionWidth);
    regionHeight    = Utils::ClampUINT32(regionHeight, 1, effectiveFrameHeight);
    regionHorizNum  = Utils::ClampUINT32(regionHorizNum, CSStats14MinHorizRegions, CSStats14MaxHorizRegions);
    regionVertNum   = Utils::ClampUINT32(regionVertNum, CSStats14MinVertRegions, CSStats14MaxVertRegions);

    // Sanity check to ensure configuration doesn't exceed camif boundaries
    if ((regionWidth * regionHorizNum) > effectiveFrameWidth)
    {
        regionWidth = effectiveFrameWidth / regionHorizNum;
    }

    if ((regionHeight * regionVertNum) > effectiveFrameHeight)
    {
        regionHeight = effectiveFrameHeight / regionVertNum;
    }

    pRegionConfig->regionOffset.bitfields.RGN_H_OFFSET  = regionHorizOffset;
    pRegionConfig->regionOffset.bitfields.RGN_V_OFFSET  = regionVertOffset;
    pRegionConfig->regionNum.bitfields.RGN_H_NUM        = regionHorizNum - 1;
    pRegionConfig->regionNum.bitfields.RGN_V_NUM        = regionVertNum - 1;
    pRegionConfig->regionSize.bitfields.RGN_HEIGHT      = regionHeight - 1;
    pRegionConfig->regionSize.bitfields.RGN_WIDTH       = regionWidth - 1;

    // Also we need to adjust rgnHNum to satisfy the hardware limitation
    // regionHNum when divided by 8 needs to have remainder [4, 7]
    pRegionConfig->regionNum.bitfields.RGN_H_NUM =
        AdjustHorizontalRegionNumber(pRegionConfig->regionNum.bitfields.RGN_H_NUM, 8, 4);

    // Calculate shift bits - used to yield 16 bits column sum by right shifting the accumulated column sum
    m_shiftBits = static_cast<UINT16>(Utils::CalculateShiftBits(regionHeight * regionWidth, CSInputDepth, CSOutputDepth));

    // Save the configuration data locally as well.
    m_CSConfig.CSConfig.statsHNum       = regionHorizNum;
    m_CSConfig.CSConfig.statsVNum       = regionVertNum;
    m_CSConfig.CSConfig.statsRgnWidth   = regionWidth;
    m_CSConfig.CSConfig.statsRgnHeight  = regionHeight;
    m_CSConfig.CSConfig.statsHOffset    = regionHorizOffset;
    m_CSConfig.CSConfig.statsVOffset    = regionVertOffset;

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSStats14::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CSStats14::DumpRegConfig()
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Offset Config HxV [%u x %u]",
                     m_regCmd.regionConfig.regionOffset.bitfields.RGN_H_OFFSET,
                     m_regCmd.regionConfig.regionOffset.bitfields.RGN_V_OFFSET);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Number Config HxV [%u x %u]",
                     m_regCmd.regionConfig.regionNum.bitfields.RGN_H_NUM,
                     m_regCmd.regionConfig.regionNum.bitfields.RGN_V_NUM);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Region Size Config WxH   [%u x %u]",
                     m_regCmd.regionConfig.regionSize.bitfields.RGN_WIDTH,
                     m_regCmd.regionConfig.regionSize.bitfields.RGN_HEIGHT);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Shift bits: %u", m_shiftBits);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSStats14::CheckDependency
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CSStats14::CheckDependency(
    ISPInputData* pInputData)
{
    BOOL            result              = TRUE;
    UINT32      inputWidth = 0;
    UINT32      inputHeight = 0;
    CropInfo*       pSensorCAMIFCrop    = &pInputData->pStripeConfig->CAMIFCrop;
    CSStatsConfig*  pCSConfig           = &pInputData->pStripeConfig->CSStatsUpdateData.statsConfig;

    // Calculate the CAMIF size
    inputWidth = pSensorCAMIFCrop->lastPixel - pSensorCAMIFCrop->firstPixel + 1;
    inputHeight = pSensorCAMIFCrop->lastLine - pSensorCAMIFCrop->firstLine + 1;

    // CAMIF input width for YUV sensor would be twice that of sensor width, as each pixel accounts for Y and U/V data
    if (TRUE == pInputData->sensorData.isYUV)
    {
        inputWidth >>= 1;
    }


    // @todo (CAMX-3035) Need to refactor this so as to avoid reconfiguration in DualIFE mode
    // even if no change in configuration for left and right stripes
    // Check if input configuration has changed from the last one
    if ((0 != Utils::Memcmp(&m_CSConfig.CSConfig, pCSConfig, sizeof(CSStatsConfig))) ||
        (m_CAMIFHeight                      != inputHeight) ||
        (m_CAMIFWidth                       != inputWidth)  ||
        (TRUE                               == pInputData->forceTriggerUpdate))
    {
        m_CSConfig.CSConfig = *pCSConfig;
        m_CAMIFWidth        = inputWidth;
        m_CAMIFHeight       = inputHeight;

        result = TRUE;
    }
    else
    {
        result = FALSE;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSStats14::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CSStats14::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        // Check if dependent is valid and been updated compared to last request
        if (TRUE == CheckDependency(pInputData))
        {
            ConfigureRegs(pInputData);
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
// CSStats14::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult CSStats14::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (NULL != pInputData->pStripingInput)
        {
            // Check if dependent is valid and been updated compared to last request
            if (TRUE == CheckDependency(pInputData))
            {
                ConfigureRegs(pInputData);
            }

            pInputData->pStripingInput->stripingInput.rscs_input.cs_enable          = m_moduleEnable;
            pInputData->pStripingInput->stripingInput.rscs_input.cs_rgn_h_num       = m_CSConfig.CSConfig.statsHNum - 1;
            pInputData->pStripingInput->stripingInput.rscs_input.cs_rgn_v_num       = m_CSConfig.CSConfig.statsVNum - 1;
            pInputData->pStripingInput->stripingInput.rscs_input.cs_rgn_width       = m_CSConfig.CSConfig.statsRgnWidth - 1;
            pInputData->pStripingInput->stripingInput.rscs_input.cs_rgn_h_offset    = m_CSConfig.CSConfig.statsHOffset;
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
// CSStats14::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CSStats14::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    pInputData->pCalculatedData->moduleEnable.statsConfig.bitfields.CS_SHIFT_BITS = m_shiftBits;

    /// Publish CS Configuration to be used by the parser
    pInputData->pCalculatedData->metadata.CSStats = m_CSConfig;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSStats14::CSStats14
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSStats14::CSStats14()
{
    m_type          = ISPStatsModuleType::IFECS;
    m_moduleEnable  = TRUE;
    m_cmdLength     =
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFECS14RegionConfig) / RegisterWidthInBytes);
    m_CAMIFWidth    = 0;
    m_CAMIFHeight   = 0;
    m_shiftBits     = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CSStats14::~CSStats14
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSStats14::~CSStats14()
{
}

CAMX_NAMESPACE_END
