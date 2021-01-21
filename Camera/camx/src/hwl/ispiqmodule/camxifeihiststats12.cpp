////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeihiststats12.cpp
/// @brief Image Histogram (IHist) class definition v1.2
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifeihiststats12.h"

CAMX_NAMESPACE_BEGIN

class Utils;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IHistStats12::Create(
    IFEStatsModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IHistStats12;
        if (NULL == pCreateData->pModule)
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("iHist Memory allocation failed");
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
// IHistStats12::GetDMITable
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IHistStats12::GetDMITable(
    UINT32** ppDMITable)
{
    CAMX_UNREFERENCED_PARAM(ppDMITable);

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IHistStats12::ValidateDependenceParams(
    ISPInputData* pInputData)
{
    CamxResult          result          = CamxResultSuccess;
    IHistStatsConfig*   pIHistConfig    = &pInputData->pStripeConfig->IHistStatsUpdateData.statsConfig;

    if ((0 == pIHistConfig->ROI.left)    &&
        (0 == pIHistConfig->ROI.top)     &&
        (0 == pIHistConfig->ROI.width)   &&
        (0 == pIHistConfig->ROI.height))
    {
        // If zero do default
        m_defaultConfig = TRUE;
        result          = CamxResultSuccess;
    }
    else
    {
        CropInfo*           pSensorCAMIFCrop    = &pInputData->pStripeConfig->CAMIFCrop;
        UINT32              inputWidth          = pSensorCAMIFCrop->lastPixel - pSensorCAMIFCrop->firstPixel + 1;
        UINT32              inputHeight         = pSensorCAMIFCrop->lastLine - pSensorCAMIFCrop->firstLine + 1;

        m_defaultConfig = FALSE;

        // CAMIF input width for YUV sensor would be twice that of sensor width, as each pixel accounts for Y and U/V data
        if (TRUE == pInputData->sensorData.isYUV)
        {
            inputWidth >>= 1;
        }

        // Validate ROI from Stats
        if (((pIHistConfig->ROI.left + pIHistConfig->ROI.width) > inputWidth)                         ||
            ((pIHistConfig->ROI.top + pIHistConfig->ROI.height) > inputHeight)                        ||
            (0                                                                == pIHistConfig->ROI.width)    ||
            (0                                                                == pIHistConfig->ROI.height)   ||
            ((IHistStats12ChannelYCC_Y                                        != pIHistConfig->channelYCC)          &&
            (IHistStats12ChannelYCC_Cb                                        != pIHistConfig->channelYCC)          &&
            (IHistStats12ChannelYCC_Cr                                        != pIHistConfig->channelYCC)))
        {
            CAMX_LOG_ERROR(CamxLogGroupISP,
                "Invalid config: ROI %d, %d, %d, %d. YCC: %u, max pixels per bin: %u",
                pIHistConfig->ROI.left,
                pIHistConfig->ROI.top,
                pIHistConfig->ROI.width,
                pIHistConfig->ROI.height,
                pIHistConfig->channelYCC,
                pIHistConfig->maxPixelSumPerBin);
            result = CamxResultEInvalidArg;
            CAMX_ASSERT_ALWAYS();
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IHistStats12::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL result = FALSE;

    if (TRUE == m_defaultConfig)
    {
        CropInfo*           pSensorCAMIFCrop    = &pInputData->pStripeConfig->CAMIFCrop;
        UINT32              inputWidth          = pSensorCAMIFCrop->lastPixel - pSensorCAMIFCrop->firstPixel + 1;
        UINT32              inputHeight         = pSensorCAMIFCrop->lastLine - pSensorCAMIFCrop->firstLine + 1;
        // Setting defaults
        // CAMIF input width for YUV sensor would be twice that of sensor width, as each pixel accounts for Y and U/V data
        if (TRUE == pInputData->sensorData.isYUV)
        {
            inputWidth >>= 1;
        }

        if ((inputWidth  != m_currentROI.width) ||
            (inputHeight != m_currentROI.height))
        {
            m_currentROI.left           = 0;
            m_currentROI.top            = 0;
            m_currentROI.width          = inputWidth;
            m_currentROI.height         = inputHeight;
            m_IHistYCCChannelSelection  = IHistStats12ChannelYCC_Y;
            m_maxPixelSumPerBin         = 0;

            result = TRUE;
        }
    }
    else
    {
        IHistStatsConfig* pIHistConfig = &pInputData->pStripeConfig->IHistStatsUpdateData.statsConfig;

        if ((m_currentROI.left          != pIHistConfig->ROI.left)   ||
            (m_currentROI.top           != pIHistConfig->ROI.top)    ||
            (m_currentROI.width         != pIHistConfig->ROI.width)  ||
            (m_currentROI.height        != pIHistConfig->ROI.height) ||
            (m_maxPixelSumPerBin        != pIHistConfig->maxPixelSumPerBin) ||
            (m_IHistYCCChannelSelection != pIHistConfig->channelYCC) ||
            (TRUE                       == pInputData->forceTriggerUpdate))
        {
            m_currentROI                = pIHistConfig->ROI;
            m_IHistYCCChannelSelection  = static_cast<UINT8>(pIHistConfig->channelYCC);
            m_maxPixelSumPerBin         = pIHistConfig->maxPixelSumPerBin;

            result = TRUE;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::CalculateRegionConfiguration
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IHistStats12::CalculateRegionConfiguration(
    UINT32* pRegionHNum,
    UINT32* pRegionVNum)
{
    CAMX_ASSERT_MESSAGE(NULL != pRegionHNum, "iHist Invalid input pointer");
    CAMX_ASSERT_MESSAGE(NULL != pRegionVNum, "iHist Invalid input pointer");
    CAMX_ASSERT_MESSAGE(0 != m_currentROI.width, "iHist Invalid ROI Width");
    CAMX_ASSERT_MESSAGE(0 != m_currentROI.height, "iHist Invalid ROI height");

    UINT32 maxPixelSumPerBin    = 0;
    UINT32 shiftBits            = 0;
    UINT32 regionHNum           = (m_currentROI.width  / IHistStats12RegionWidth)  - 1;
    UINT32 regionVNum           = (m_currentROI.height / IHistStats12RegionHeight) - 1;

    // Check region minimum requirement criteria. Max value is based on CAMIF output which is the current config value.
    regionHNum = Utils::MaxUINT32(regionHNum, IHistStats12RegionMinHNum);
    regionVNum = Utils::MaxUINT32(regionVNum, IHistStats12RegionMinVNum);

    *pRegionHNum = regionHNum;
    *pRegionVNum = regionVNum;

    if (0 == m_maxPixelSumPerBin)
    {
        // Configure shift value base on total number of pixels used for Image Histogram
        maxPixelSumPerBin = static_cast<UINT32>(static_cast<FLOAT>(((regionHNum + 1) * (regionVNum + 1))) * 2.0);
        // From HLD, worst case scenario, all pixels fall into these number of bins
        maxPixelSumPerBin /= IHistStats12MinNumberOfBins;

        // Save max pixel value
        m_maxPixelSumPerBin = maxPixelSumPerBin;
    }
    else
    {
        maxPixelSumPerBin = m_maxPixelSumPerBin;
    }

    UINT32 minBits = Utils::MinBitsUINT32(maxPixelSumPerBin); // Get the minimum number of bits to represent all the pixels
    if (IHistStats12StatsBits < minBits)
    {
        minBits     -= IHistStats12StatsBits;
        shiftBits   = Utils::ClampUINT32(minBits, 0, IHistStats12MaxStatsShift);

        if (minBits != shiftBits)
        {
            UINT32 newMaxPixelSumPerBin = 0;

            // Calculate the saturation pixel values used per bin
            newMaxPixelSumPerBin    = Utils::AllOnes32LeftShift(32, IHistStats12StatsBits + shiftBits);
            newMaxPixelSumPerBin    = newMaxPixelSumPerBin ^ (~0);
            m_maxPixelSumPerBin     = newMaxPixelSumPerBin;
        }
    }
    else
    {
        shiftBits = 0;
    }

    m_IHistShiftBits = static_cast<UINT8>(shiftBits);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::ConfigureRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IHistStats12::ConfigureRegisters(
    ISPInputData* pInputData,
    const UINT32  regionHNum,
    const UINT32  regionVNum)
{
    CAMX_UNREFERENCED_PARAM(pInputData);
    IFE_IFE_0_VFE_STATS_IHIST_RGN_OFFSET_CFG*  pRegionOffset   = &m_regCmd.regionConfig.regionOffset;
    IFE_IFE_0_VFE_STATS_IHIST_RGN_NUM_CFG*     pRegionNumber   = &m_regCmd.regionConfig.regionNumber;

    pRegionOffset->bitfields.RGN_H_OFFSET  = m_currentROI.left;
    pRegionOffset->bitfields.RGN_V_OFFSET  = m_currentROI.top;
    pRegionNumber->bitfields.RGN_H_NUM     = regionHNum;
    pRegionNumber->bitfields.RGN_V_NUM     = regionVNum;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IHistStats12::CreateCmdList(
    ISPInputData* pInputData)
{
    CAMX_ASSERT_MESSAGE(NULL != pInputData->pCmdBuffer, "iHist invalid cmd buffer pointer");
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;

        result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                              regIFE_IFE_0_VFE_STATS_IHIST_RGN_OFFSET_CFG,
                                              (sizeof(IFEIHist12RegionConfig) / RegisterWidthInBytes),
                                              reinterpret_cast<UINT32*>(&m_regCmd.regionConfig));
    }
    else
    {
        result = CamxResultEInvalidPointer;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IHistStats12::RunCalculation(
    ISPInputData* pInputData)
{
    UINT32  regionHNum = 0;
    UINT32  regionVNum = 0;

    CalculateRegionConfiguration(&regionHNum, &regionVNum);

    ConfigureRegisters(pInputData, regionHNum, regionVNum);

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IHistStats12::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (NULL != pInputData->pStripingInput)
        {
            IHistStatsConfig*   pIHistConfig    = &pInputData->pStripeConfig->IHistStatsUpdateData.statsConfig;
            UINT32              imageWidth      = pInputData->pStripeConfig->CAMIFCrop.lastPixel -
                                                  pInputData->pStripeConfig->CAMIFCrop.firstPixel + 1;
            UINT32              regionWidth     = (0 == pIHistConfig->ROI.width) ? imageWidth : pIHistConfig->ROI.width;
            UINT32              regionHNum      = regionWidth / IHistStats12RegionWidth;

            regionHNum = Utils::MaxUINT32(regionHNum, IHistStats12RegionMinHNum);

            pInputData->pStripingInput->stripingInput.iHist_input.enable            = m_moduleEnable;
            pInputData->pStripingInput->stripingInput.iHist_input.hist_rgn_h_num    = regionHNum - 1;
            pInputData->pStripingInput->stripingInput.iHist_input.hist_rgn_h_offset = pIHistConfig->ROI.left;
        }
    }
    else
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Data");
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IHistStats12::DumpRegConfig()
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "IHist region offset HxV     [%u x %u]",
                     m_regCmd.regionConfig.regionOffset.bitfields.RGN_H_OFFSET,
                     m_regCmd.regionConfig.regionOffset.bitfields.RGN_V_OFFSET);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "IHist region num HxV        [%u x %u]",
                     m_regCmd.regionConfig.regionNumber.bitfields.RGN_H_NUM,
                     m_regCmd.regionConfig.regionNumber.bitfields.RGN_V_NUM);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "IHist shift bits            [0x%x]", m_IHistShiftBits)
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "IHist YCC channel select    [0x%x]", m_IHistYCCChannelSelection);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "IHist site/tap select       [0x%x]", m_IHistSiteSelection);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IHistStats12::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    if (NULL != pInputData)
    {
        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);

        if (CamxResultSuccess == result &&
            TRUE == CheckDependenceChange(pInputData))
        {
            RunCalculation(pInputData);
            result = CreateCmdList(pInputData);
        }

        if (CamxResultSuccess == result)
        {
            // Update CAMX metadata with the values for the current frame
            UpdateIFEInternalData(pInputData);
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("IHist invalid ISPInputData pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IHistStats12::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    IFE_IFE_0_VFE_STATS_CFG*    pStatsConfig                = &pInputData->pCalculatedData->moduleEnable.statsConfig;

    // Configure per-frame register configuration
    pStatsConfig->bitfields.IHIST_CHAN_SEL      = m_IHistYCCChannelSelection;
    pStatsConfig->bitfields.IHIST_SITE_SEL      = m_IHistSiteSelection;
    pStatsConfig->bitfields.IHIST_SHIFT_BITS    = m_IHistShiftBits;

    // Update metadata info
    pInputData->pCalculatedData->metadata.IHistStatsConfig.IHistConfig.maxPixelSumPerBin    = m_maxPixelSumPerBin;
    pInputData->pCalculatedData->metadata.IHistStatsConfig.IHistConfig.channelYCC           =
        static_cast<IHistChannelSelection>(m_IHistYCCChannelSelection);
    pInputData->pCalculatedData->metadata.IHistStatsConfig.IHistConfig.ROI                  = m_currentROI;
    pInputData->pCalculatedData->metadata.IHistStatsConfig.numBins                          = IHistStats12BinsPerChannel;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::IHistStats12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IHistStats12::IHistStats12()
{
    m_type         = ISPStatsModuleType::IFEIHist;
    m_moduleEnable = TRUE;
    m_cmdLength    = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEIHist12RegionConfig) / RegisterWidthInBytes);

    // Set fixed default configuration
    m_IHistYCCChannelSelection  = IHistStats12ChannelYCC_Y;
    m_IHistSiteSelection        = IHistTapoutBeforeGLUT;
    m_maxPixelSumPerBin         = 0; // Use default estimation
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IHistStats12::~IHistStats12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IHistStats12::~IHistStats12()
{
}

CAMX_NAMESPACE_END
