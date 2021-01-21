////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, 2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifetintlessbgstats15.h
/// @brief Tintless BG Stats V 1.5 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXIFETINTLESSBGSTATS15_H
#define CAMXIFETINTLESSBGSTATS15_H

#include "camxispstatsmodule.h"

CAMX_NAMESPACE_BEGIN

// Register range from the SWI/HPG
static const UINT32 TintlessBGStats15RegionMinWidth  = 6;   ///< Minimum TintlessBG stats region width that we could configure
static const UINT32 TintlessBGStats15RegionMinHeight = 2;   ///< Minimum TintlessBG stats region Height that we could configure
static const UINT32 TintlessBGStats15RegionMaxWidth  = 390; ///< Minimum TintlessBG stats region width that we could configure
static const UINT32 TintlessBGStats15RegionMaxHeight = 512; ///< Minimum TintlessBG stats region Height that we could configure

static const INT32 TintlessBGStats15MaxHorizontalregions = 64; ///< Maximum TintlessBG stats horizontal region
static const INT32 TintlessBGStats15MaxVerticalregions   = 48;  ///< Maximum TintlessBG stats vertical region

static const UINT32 TintlessBGStats15MaxChannelThreshold = ((1 << IFEPipelineBitWidth) - 1); ///< Max channel threshold

CAMX_BEGIN_PACKED
/// @brief TintlessBG Stats ROI Configuration
struct IFETintlessBG15RegionConfig
{
    IFE_IFE_0_VFE_STATS_AEC_BG_RGN_OFFSET_CFG     regionOffset;   ///< TintlessBG stats region offset config
    IFE_IFE_0_VFE_STATS_AEC_BG_RGN_NUM_CFG        regionNumber;   ///< TintlessBG stats region number config
    IFE_IFE_0_VFE_STATS_AEC_BG_RGN_SIZE_CFG       regionSize;     ///< TintlessBG stats region size config
    IFE_IFE_0_VFE_STATS_AEC_BG_HI_THRESHOLD_CFG_0 highThreshold0; ///< TintlessBG stats pixel upper threshold config
    IFE_IFE_0_VFE_STATS_AEC_BG_HI_THRESHOLD_CFG_1 highThreshold1; ///< TintlessBG stats pixel upper threshold config
    IFE_IFE_0_VFE_STATS_AEC_BG_LO_THRESHOLD_CFG_0 lowThreshold0;  ///< TintlessBG stats pixel lower threshold config
    IFE_IFE_0_VFE_STATS_AEC_BG_LO_THRESHOLD_CFG_1 lowThreshold1;  ///< TintlessBG stats pixel lower threshold config
} CAMX_PACKED;

/// @brief TintlessBG Stats Configuration
struct IFETintlessBG15RegCmd
{
    IFETintlessBG15RegionConfig         regionConfig;          ///< TintlessBG stats region and threshold config
    IFE_IFE_0_VFE_STATS_AEC_BG_CH_Y_CFG lumaConfig;            ///< TintlessBG stats luma channel config
    IFE_IFE_0_VFE_STATS_AEC_BG_CFG      tintlessBGStatsconfig; ///< TintlessBG stats config
}CAMX_PACKED;

CAMX_END_PACKED


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief TintlessBGStats15 Class Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TintlessBGStats15 final : public ISPStatsModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create TintlessBGStats15 Object
    ///
    /// @param  pCreateData Pointer to the TintlessBGStats15 Creation
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IFEStatsModuleCreateData* pCreateData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Execute
    ///
    /// @brief  Execute process capture request to configure module
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Execute(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~TintlessBGStats15
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~TintlessBGStats15();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// TintlessBGStats15
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    TintlessBGStats15();

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateDependenceParams
    ///
    /// @brief  Validate the stats region of interest configuration from stats module
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if the input is valid or invalid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ValidateDependenceParams(
        ISPInputData* pInputData) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateIFEInternalData
    ///
    /// @brief  Update TintlessBG configuration to the Internal Data
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateIFEInternalData(
        ISPInputData* pInputData) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckDependenceChange
    ///
    /// @brief  Check to see if the Dependence Trigger Data Changed
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return BOOL
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckDependenceChange(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculation
    ///
    /// @brief  Calculate the Register Value
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrepareStripingParameters
    ///
    /// @brief  Prepare striping parameters for striping lib
    ///
    /// @param  pInputData Pointer to the Inputdata
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult PrepareStripingParameters(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureRegisters
    ///
    /// @brief  Configure scaler registers
    ///
    /// @param  regionWidth  Tintless BG stats ROI region width
    /// @param  regionHeight Tintless BG stats ROI region height
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureRegisters(
        UINT32 regionWidth,
        UINT32 regionHeight);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AdjustROIParams
    ///
    /// @brief  Adjust ROI from stats to requirement based on hardware
    ///
    /// @param  pRegionWidth  pointer to Tintless BG stats ROI region width
    /// @param  pRegionHeight pointer to Tintless BG stats ROI region height
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID AdjustROIParams(
        UINT32* pRegionWidth,
        UINT32* pRegionHeight);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of Tintless BG module for debug
    ///
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig() const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateCmdList
    ///
    /// @brief  Create the Command List
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        ISPInputData* pInputData);

    TintlessBGStats15(const TintlessBGStats15&)            = delete;    ///< Disallow the copy constructor
    TintlessBGStats15& operator=(const TintlessBGStats15&) = delete;    ///< Disallow assignment operator

    IFETintlessBG15RegCmd    m_regCmd;           ///< Register list for Tintless TintlessBG stats
    ISPTintlessBGStatsConfig m_tintlessBGConfig; ///< TintlessBG configuration data from stats
};

CAMX_NAMESPACE_END

#endif // CAMXIFETINTLESSBGSTATS15_H
