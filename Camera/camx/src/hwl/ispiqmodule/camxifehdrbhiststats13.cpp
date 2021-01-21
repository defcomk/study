////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifehdrbhiststats13.cpp
/// @brief HDR Bayer Histogram (HDRBHist) stats v1.3
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifehdrbhiststats13.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBHistStats13::Create(
    IFEStatsModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW HDRBHistStats13;
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
// HDRBHistStats13::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBHistStats13::UpdateIFEInternalData(
    ISPInputData* pInputData
    ) const
{
    IFE_IFE_0_VFE_STATS_CFG* pStatsConfig = &pInputData->pCalculatedData->moduleEnable.statsConfig;

    pInputData->pCalculatedData->metadata.HDRBHistStatsConfig = m_HDRBHistConfig;

    // Write to general stats configuration register
    pStatsConfig->bitfields.HDR_BHIST_CHAN_SEL  = m_greenChannelSelect;
    pStatsConfig->bitfields.HDR_BHIST_FIELD_SEL = m_inputFieldSelect;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL HDRBHistStats13::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL                    configRequired  = FALSE;
    const HDRBHistConfig*   pHDRBHistConfig = &pInputData->pStripeConfig->AECStatsUpdateData.statsConfig.HDRBHistConfig;
    HDRBHistConfig*         pCurrentConfig  = &m_HDRBHistConfig.HDRBHistConfig;

    if ((pCurrentConfig->ROI.left           != pHDRBHistConfig->ROI.left)           ||
        (pCurrentConfig->ROI.top            != pHDRBHistConfig->ROI.top)            ||
        (pCurrentConfig->ROI.width          != pHDRBHistConfig->ROI.width)          ||
        (pCurrentConfig->ROI.height         != pHDRBHistConfig->ROI.height)         ||
        (pCurrentConfig->greenChannelInput  != pHDRBHistConfig->greenChannelInput)  ||
        (pCurrentConfig->inputFieldSelect   != pHDRBHistConfig->inputFieldSelect)   ||
        (TRUE                               == pInputData->forceTriggerUpdate))
    {
        pCurrentConfig->ROI                 = pHDRBHistConfig->ROI;
        pCurrentConfig->greenChannelInput   = pHDRBHistConfig->greenChannelInput;
        pCurrentConfig->inputFieldSelect    = pHDRBHistConfig->inputFieldSelect;

        configRequired = TRUE;
    }
    else
    {
        configRequired              = FALSE;
        m_HDRBHistConfig.isAdjusted = FALSE;
    }

    return configRequired;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBHistStats13::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result       = CamxResultSuccess;
    CmdBuffer* pCmdBuffer   = pInputData->pCmdBuffer;

    result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                          regIFE_IFE_0_VFE_STATS_HDR_BHIST_RGN_OFFSET_CFG,
                                          (sizeof(IFEHDRBHistRegionConfig) / RegisterWidthInBytes),
                                          reinterpret_cast<UINT32*>(&m_regCmd));
    if (CamxResultSuccess != result)
    {
        CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::ConfigureRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBHistStats13::ConfigureRegisters(
    const HDRBHistRegionConfig* pRegionConfig)
{
    IFE_IFE_0_VFE_STATS_HDR_BHIST_RGN_OFFSET_CFG*   pRegionOffset   = &m_regCmd.regionOffset;
    IFE_IFE_0_VFE_STATS_HDR_BHIST_RGN_NUM_CFG*      pRegionNumber   = &m_regCmd.regionNumber;

    pRegionOffset->bitfields.RGN_H_OFFSET   = pRegionConfig->offsetHorizNum;
    pRegionOffset->bitfields.RGN_V_OFFSET   = pRegionConfig->offsetVertNum;
    pRegionNumber->bitfields.RGN_H_NUM      = pRegionConfig->regionHorizNum;
    pRegionNumber->bitfields.RGN_V_NUM      = pRegionConfig->regionVertNum;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::CalculateRegionConfiguration
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBHistStats13::CalculateRegionConfiguration(
    HDRBHistRegionConfig* pRegionConfig)
{
    HDRBHistConfig*     pHDRBHistConfig = &m_HDRBHistConfig.HDRBHistConfig;
    RectangleCoordinate newCoordinate;

    CAMX_ASSERT_MESSAGE(0 != pHDRBHistConfig->ROI.width, "Invalid ROI width");
    CAMX_ASSERT_MESSAGE(0 != pHDRBHistConfig->ROI.height, "Invalid ROI height");

    m_HDRBHistConfig.isAdjusted = FALSE;

    // Set region
    pRegionConfig->offsetHorizNum   = Utils::FloorUINT32(m_regionMultipleFactor, pHDRBHistConfig->ROI.left);
    pRegionConfig->offsetVertNum    = Utils::FloorUINT32(m_regionMultipleFactor, pHDRBHistConfig->ROI.top);

    pRegionConfig->regionHorizNum   = (pHDRBHistConfig->ROI.width / HDRBHistStats13RegionWidth) - 1;
    pRegionConfig->regionVertNum    = (pHDRBHistConfig->ROI.height / HDRBHistStats13RegionHeight) - 1;
    pRegionConfig->regionHorizNum   = Utils::MaxUINT32(pRegionConfig->regionHorizNum, HDRBHistStats13MinHorizregions);
    pRegionConfig->regionVertNum    = Utils::MaxUINT32(pRegionConfig->regionVertNum, HDRBHistStats13MinVertregions);

    // Verify if region was adjusted
    newCoordinate.left      = pRegionConfig->offsetHorizNum;
    newCoordinate.top       = pRegionConfig->offsetVertNum;
    newCoordinate.width     = (pRegionConfig->regionHorizNum + 1) * HDRBHistStats13RegionWidth;
    newCoordinate.height    = (pRegionConfig->regionVertNum + 1) * HDRBHistStats13RegionHeight;

    if ((newCoordinate.left     != pHDRBHistConfig->ROI.left)    ||
        (newCoordinate.top      != pHDRBHistConfig->ROI.top)     ||
        (newCoordinate.width    != pHDRBHistConfig->ROI.width)   ||
        (newCoordinate.height   != pHDRBHistConfig->ROI.height))
    {
        pHDRBHistConfig->ROI = newCoordinate;
        m_HDRBHistConfig.isAdjusted = TRUE;
    }

    // Set green channel
    switch (pHDRBHistConfig->greenChannelInput)
    {
        case HDRBHistSelectGB:
            m_greenChannelSelect = HDRBHistGreenChannelSelectGb;
            break;
        case HDRBHistSelectGR:
            m_greenChannelSelect = HDRBHistGreenChannelSelectGr;
            break;
        default:
            m_greenChannelSelect                                = HDRBHistGreenChannelSelectGb;
            m_HDRBHistConfig.HDRBHistConfig.greenChannelInput   = HDRBHistSelectGB;
            m_HDRBHistConfig.isAdjusted                         = TRUE;
            break;
    }

    // Set input field/mode
    switch (pHDRBHistConfig->inputFieldSelect)
    {
        case HDRBHistInputAll:
            m_inputFieldSelect = HDRBHistFieldSelectAll;
            break;
        case HDRBHistInputLongExposure:
            m_inputFieldSelect = HDRBHistFieldSelectT1;
            break;
        case HDRBHistInputShortExposure:
            m_inputFieldSelect = HDRBHistFieldSelectT2;
            break;
        default:
            m_inputFieldSelect                  = HDRBHistFieldSelectAll;
            pHDRBHistConfig->inputFieldSelect   = HDRBHistInputAll;
            m_HDRBHistConfig.isAdjusted         = TRUE;
            break;
    }

    m_HDRBHistConfig.numBins = HDRBHistBinsPerChannel;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBHistStats13::RunCalculation(
    ISPInputData* pInputData)
{
    CAMX_UNREFERENCED_PARAM(pInputData);
    HDRBHistRegionConfig regionConfig;

    CalculateRegionConfiguration(&regionConfig);

    ConfigureRegisters(&regionConfig);

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBHistStats13::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    if (NULL != pInputData)
    {
        if (NULL != pInputData->pStripingInput)
        {
            HDRBHistConfig* pHDRBHistCfg = &pInputData->pAECStatsUpdateData->statsConfig.HDRBHistConfig;

            pInputData->pStripingInput->stripingInput.hdrBhist_input.bihist_enabled      = m_moduleEnable;
            pInputData->pStripingInput->stripingInput.hdrBhist_input.bihist_roi_h_offset =
                Utils::FloorUINT32(m_regionMultipleFactor, pHDRBHistCfg->ROI.left);
            pInputData->pStripingInput->stripingInput.hdrBhist_input.bihist_rgn_h_num    =
                pHDRBHistCfg->ROI.width / HDRBHistStats13RegionWidth - 1;
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
// HDRBHistStats13::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBHistStats13::ValidateDependenceParams(
    ISPInputData* pInputData
    ) const
{
    CamxResult      result              = CamxResultSuccess;
    CropInfo*       pSensorCAMIFCrop    = &pInputData->pStripeConfig->CAMIFCrop;
    HDRBHistConfig* pHDRBHistConfig     = &pInputData->pStripeConfig->AECStatsUpdateData.statsConfig.HDRBHistConfig;
    INT32           inputWidth          = pSensorCAMIFCrop->lastPixel - pSensorCAMIFCrop->firstPixel + 1;
    INT32           inputHeight         = pSensorCAMIFCrop->lastLine - pSensorCAMIFCrop->firstLine + 1;
    INT32           top                 = 0;
    INT32           left                = 0;
    INT32           width               = 0;
    INT32           height              = 0;

    // CAMIF input width for YUV sensor would be twice that of sensor width, as each pixel accounts for Y and U/V data
    if (TRUE == pInputData->sensorData.isYUV)
    {
        inputWidth >>= 1;
    }

    top    = pHDRBHistConfig->ROI.top;
    left   = pHDRBHistConfig->ROI.left;
    width  = pHDRBHistConfig->ROI.width;
    height = pHDRBHistConfig->ROI.height;

    // Validate ROI from Stats
    if ((left            <  0)           ||
        (top             <  0)           ||
        (width           <=  0)          ||
        (height          <=  0)          ||
        ((left + width)  > inputWidth)   ||
        ((top  + height) > inputHeight)  ||
        (width           == 0)           ||
        (height          == 0))
    {
        CAMX_LOG_ERROR(CamxLogGroupISP,
                       "Invalid config: ROI %d, %d, %d, %d",
                       pHDRBHistConfig->ROI.left,
                       pHDRBHistConfig->ROI.top,
                       pHDRBHistConfig->ROI.width,
                       pHDRBHistConfig->ROI.height);

        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID HDRBHistStats13::DumpRegConfig()
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                     "Region Offset Config          [%dx%d]",
                     m_regCmd.regionOffset.bitfields.RGN_H_OFFSET,
                     m_regCmd.regionOffset.bitfields.RGN_V_OFFSET);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                     "Region Number Config          [%dx%d]",
                     m_regCmd.regionNumber.bitfields.RGN_H_NUM,
                     m_regCmd.regionNumber.bitfields.RGN_V_NUM);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Green channel selection Gr/Gb                [0x%x]", m_greenChannelSelect);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Field selection, non-HDR/long/short exposure [0x%x]", m_inputFieldSelect);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult HDRBHistStats13::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT_MESSAGE(NULL != pInputData, "HDRBHist invalid ISPInputData pointer");
    CAMX_ASSERT_MESSAGE(NULL != pInputData->pCmdBuffer, "HDRBHist invalid pCmdBuffer");

    if ((NULL != pInputData) && (NULL != pInputData->pCmdBuffer))
    {
        result = ValidateDependenceParams(pInputData);

        if ((CamxResultSuccess == result) &&
            (TRUE == CheckDependenceChange(pInputData)))
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
// HDRBHistStats13::HDRBHistStats13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
HDRBHistStats13::HDRBHistStats13()
{
    m_type          = ISPStatsModuleType::IFEHDRBHist;
    m_moduleEnable  = TRUE;
    m_cmdLength     = PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEHDRBHistRegionConfig) / RegisterWidthInBytes);

    m_greenChannelSelect    = HDRBHistGreenChannelSelectGr;
    m_inputFieldSelect      = HDRBHistFieldSelectAll;

    /// @note To keep simple configuration between non-HDR and HDR sensors, using worst multipe factor
    m_regionMultipleFactor  = MultipleFactorFour;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDRBHistStats13::~HDRBHistStats13
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
HDRBHistStats13::~HDRBHistStats13()
{
}

CAMX_NAMESPACE_END
