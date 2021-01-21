////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifersstats14.cpp
/// @brief RS Stats v1.4 Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifersstats14.h"
#include "camxutils.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RSStats14::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult RSStats14::Create(
    IFEStatsModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW RSStats14;
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
// RSStats14::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult RSStats14::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_STATS_RS_RGN_OFFSET_CFG,
                                              (sizeof(IFERS14RegionConfig) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd.regionConfig));

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
// RSStats14::ConfigureRegs
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID RSStats14::ConfigureRegs(
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
    IFERS14RegionConfig*    pRegionConfig           = &m_regCmd.regionConfig;

    // Minimum ROI width - min horizontal regions * minimum region width
    minROIWidth = RSStats14MinHorizRegions * 1;
    // Minimum ROI height - min horizontal regions * minimum region height
    minROIHeight = RSStats14MinVertRegions * RSStats14MinRegionHeight;

    // Configure region offset
    // Horizontal offset - minimum is 0 and maximum is image_width - 1
    regionHorizOffset = m_RSConfig.RSConfig.statsHOffset;
    regionHorizOffset = Utils::ClampUINT32(regionHorizOffset, 0, (m_CAMIFWidth - minROIWidth) - 1);

    // Vertical offset - minimum is 0 and maximum is image_height - 1
    regionVertOffset = m_RSConfig.RSConfig.statsVOffset;
    regionVertOffset = Utils::ClampUINT32(regionVertOffset, 0, (m_CAMIFHeight - minROIHeight) - 1);

    effectiveFrameWidth     = m_CAMIFWidth - regionHorizOffset;
    effectiveFrameHeight    = m_CAMIFHeight - regionVertOffset;

    // Setting Horizontal and Vertical region numbers
    regionHorizNum = m_RSConfig.RSConfig.statsHNum;
    if ((0 == regionHorizNum) || (regionHorizNum > RSStats14MaxHorizRegions))
    {
        regionHorizNum = RSStats14MaxHorizRegions;
    }

    regionVertNum = m_RSConfig.RSConfig.statsVNum;
    if ((0 == regionVertNum) || (regionVertNum > RSStats14MaxVertRegions))
    {
        regionVertNum = RSStats14MaxVertRegions;
    }

    // Region width - minimum is 1 pixel and maximum is image_width pixels
    regionWidth = m_RSConfig.RSConfig.statsRgnWidth;
    if (0 == regionWidth)
    {
        regionWidth = effectiveFrameWidth / Utils::MaxUINT32(regionHorizNum, 1);
    }

    // Region height - Number of lines for a given region - minimum is 1 and maximum is 16
    regionHeight = m_RSConfig.RSConfig.statsRgnHeight;
    if (0 == regionHeight)
    {
        regionHeight = (effectiveFrameHeight + regionVertNum - 1) / Utils::MaxUINT32(regionVertNum, 1);
        // Adjust horizontal regions based on new regionWidth again
        regionVertNum = effectiveFrameHeight / Utils::MaxUINT32(regionHeight, 1);
    }

    // Also clamp parameters to Min and Max of Hardware limitations
    regionWidth     = Utils::ClampUINT32(regionWidth, 1, effectiveFrameWidth);
    regionHeight    = Utils::ClampUINT32(regionHeight, RSStats14MinRegionHeight, RSStats14MaxRegionHeight);
    regionHorizNum  = Utils::ClampUINT32(regionHorizNum, RSStats14MinHorizRegions, RSStats14MaxHorizRegions);
    regionVertNum   = Utils::ClampUINT32(regionVertNum, RSStats14MinVertRegions, RSStats14MaxVertRegions);

    // Sanity check to ensure configuration doesn't exceed camif boundaries
    if ((regionWidth * regionHorizNum) > effectiveFrameWidth)
    {
        // Adjust only HNum dont adjust regionWidth. If its other way then in case of
        // Dual IFE Scenario Left and Right stripe would endup in having different regionWidth
        regionHorizNum = effectiveFrameWidth / regionWidth;
    }

    if ((regionHeight * regionVertNum) > effectiveFrameHeight)
    {
        // Adjust only VNum dont adjust regionHeighth. If its other way then in case of
        // Dual IFE Scenario Left and Right stripe would endup in having different regionHeight
        regionVertNum = effectiveFrameHeight / regionHeight;
    }

    pRegionConfig->regionOffset.bitfields.RGN_H_OFFSET  = regionHorizOffset;
    pRegionConfig->regionOffset.bitfields.RGN_V_OFFSET  = regionVertOffset;
    pRegionConfig->regionNum.bitfields.RGN_H_NUM        = regionHorizNum - 1;
    pRegionConfig->regionNum.bitfields.RGN_V_NUM        = regionVertNum - 1;
    pRegionConfig->regionSize.bitfields.RGN_HEIGHT      = regionHeight - 1;
    pRegionConfig->regionSize.bitfields.RGN_WIDTH       = regionWidth - 1;

    // Calculate shift bits - used to yield 16 bits column sum by right shifting the accumulated column sum
    m_shiftBits = static_cast<UINT16>(Utils::CalculateShiftBits(regionWidth * regionHeight, RSInputDepth, RSOutputDepth));

    // If RS configuration has been adjusted and is different from input, update is adjusted flag
    m_RSConfig.isAdjusted = ((m_RSConfig.RSConfig.statsHNum != regionHorizNum) ||
                            (m_RSConfig.RSConfig.statsVNum != regionVertNum)) ? TRUE : FALSE;

    // Save the configuration data locally as well.
    m_RSConfig.RSConfig.statsHNum       = regionHorizNum;
    m_RSConfig.RSConfig.statsVNum       = regionVertNum;
    m_RSConfig.RSConfig.statsRgnWidth   = regionWidth;
    m_RSConfig.RSConfig.statsHOffset    = regionHorizOffset;
    m_RSConfig.RSConfig.statsVOffset    = regionVertOffset;
    m_RSConfig.RSConfig.statsRgnHeight  = regionHeight;

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "ISPRS: Hnum:%d Vnum:%d RegionH:%d RegionW:%d",
        m_RSConfig.RSConfig.statsHNum,
        m_RSConfig.RSConfig.statsVNum,
        m_RSConfig.RSConfig.statsRgnHeight,
        m_RSConfig.RSConfig.statsRgnWidth);

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RSStats14::CheckDependency
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL RSStats14::CheckDependency(
    ISPInputData* pInputData)
{
    BOOL                result              = TRUE;
    UINT32              inputWidth          = 0;
    UINT32              inputHeight         = 0;
    CropInfo*           pSensorCAMIFCrop    = &pInputData->pStripeConfig->CAMIFCrop;
    AFDStatsRSConfig*   pRSConfig           = &pInputData->pStripeConfig->AFDStatsUpdateData.statsConfig;

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
    if ((0 != Utils::Memcmp(&m_RSConfig.RSConfig, pRSConfig, sizeof(AFDStatsRSConfig))) ||
        (m_CAMIFHeight                  != inputHeight) ||
        (m_CAMIFWidth                   != inputWidth)  ||
        (TRUE                           == pInputData->forceTriggerUpdate))
    {
        m_RSConfig.RSConfig = *pRSConfig;
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
// RSStats14::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID RSStats14::DumpRegConfig()
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
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Input color conversion enable: %u, shift bits: %u",
                     m_RSConfig.RSConfig.statsRSCSColorConversionEnable,
                     m_shiftBits);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RSStats14::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult RSStats14::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        // Check if dependent is valid and been updated compared to last request
        if (TRUE == CheckDependency(pInputData))
        {
            if ((m_RSConfig.RSConfig.statsRgnWidth == 0) || (m_RSConfig.RSConfig.statsRgnHeight == 0))
            {
                m_RSConfig.RSConfig.statsRgnWidth = m_calculatedRegnWidth;
                m_RSConfig.RSConfig.statsRgnHeight = m_calculatedRegnHeight;
            }
            ConfigureRegs(pInputData);
            // In case of Dual IFE scenario we need to make sure region width and
            // height are made similar for both IFE Regions or else parsed stats
            // would incur inconsistency.
            m_calculatedRegnWidth = m_RSConfig.RSConfig.statsRgnWidth;
            m_calculatedRegnHeight = m_RSConfig.RSConfig.statsRgnHeight;
            result = CreateCmdList(pInputData);
        }

        // Always update
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
// RSStats14::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult RSStats14::PrepareStripingParameters(
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

            pInputData->pStripingInput->stripingInput.rscs_input.rs_enable          = m_moduleEnable;
            pInputData->pStripingInput->stripingInput.rscs_input.rs_rgn_h_num       = m_RSConfig.RSConfig.statsHNum - 1;
            pInputData->pStripingInput->stripingInput.rscs_input.rs_rgn_v_num       = m_RSConfig.RSConfig.statsVNum - 1;
            pInputData->pStripingInput->stripingInput.rscs_input.rs_rgn_width       = m_RSConfig.RSConfig.statsRgnWidth - 1;
            pInputData->pStripingInput->stripingInput.rscs_input.rs_rgn_h_offset    = m_RSConfig.RSConfig.statsHOffset;
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
// RSStats14::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID RSStats14::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    pInputData->pCalculatedData->moduleEnable.statsConfig.bitfields.RS_SHIFT_BITS = m_shiftBits;
    pInputData->pCalculatedData->moduleEnable.statsConfig.bitfields.COLOR_CONV_EN =
        (FALSE == m_RSConfig.RSConfig.statsRSCSColorConversionEnable) ? 0 : 1;

    // Update RS Stats configuration
    pInputData->pCalculatedData->metadata.RSStats = m_RSConfig;
    pInputData->pAFDStatsUpdateData->statsConfig.statsRgnWidth = m_RSConfig.RSConfig.statsRgnWidth;
    pInputData->pAFDStatsUpdateData->statsConfig.statsRgnHeight = m_RSConfig.RSConfig.statsRgnHeight;

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "ISPRS: RS En:%d Hnum:%d Vnum:%d RegionH:%d RegionW:%d HOffset:%d VOffset:%d",
        m_moduleEnable,
        m_RSConfig.RSConfig.statsHNum,
        m_RSConfig.RSConfig.statsVNum,
        m_RSConfig.RSConfig.statsRgnHeight,
        m_RSConfig.RSConfig.statsRgnWidth,
        m_RSConfig.RSConfig.statsHOffset,
        m_RSConfig.RSConfig.statsVOffset);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RSStats14::RSStats14
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RSStats14::RSStats14()
{
    m_type          = ISPStatsModuleType::IFERS;
    m_moduleEnable  = TRUE;
    m_cmdLength     =
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFERS14RegionConfig) / RegisterWidthInBytes);
    m_CAMIFWidth    = 0;
    m_CAMIFHeight   = 0;
    m_shiftBits     = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RSStats14::~RSStats14
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RSStats14::~RSStats14()
{
}

CAMX_NAMESPACE_END
