////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeawbbgstats14.h
/// @brief AWB BG Stats14 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXIFEAWBBGSTATS14_H
#define CAMXIFEAWBBGSTATS14_H

#include "camxispstatsmodule.h"

CAMX_NAMESPACE_BEGIN

// Register range from the SWI/HPG
static const UINT32 AWBBGStats14RegionMinWidth  = 6;      ///< Minimum AWB BG stats region width that we could configure
static const UINT32 AWBBGStats14RegionMaxWidth  = 390;    ///< Maximum AWB BG stats region width that we could configure
static const UINT32 AWBBGStats14RegionMinHeight = 2;      ///< Minimum AWB BG stats region Height that we could configure
static const UINT32 AWBBGStats14RegionMaxHeight = 512;    ///< Maximum AWB BG stats region Height that we could configure

static const INT32 AWBBGStats15MaxHorizontalregions = 160;   ///< Maximum AWB BG stats horizontal region
static const INT32 AWBBGStats15MaxVerticalregions   = 90;    ///< Maximum AWB BG stats vertical region

static const UINT32 AWBBGStats14MaxChannelThreshold = (1 << IFEPipelineBitWidth) - 1;   ///< Max AWB BG stats channel threshold

CAMX_BEGIN_PACKED
/// @brief AWB BG Stats Configuration
struct IFEAWBBG14RegionConfig
{
    IFE_IFE_0_VFE_STATS_AWB_BG_RGN_OFFSET_CFG        regionOffset;     ///< AWB BG stats region offset config
    IFE_IFE_0_VFE_STATS_AWB_BG_RGN_NUM_CFG           regionNumber;     ///< AWB BG stats region number config
    IFE_IFE_0_VFE_STATS_AWB_BG_RGN_SIZE_CFG          regionSize;       ///< AWB BG stats region size config
    IFE_IFE_0_VFE_STATS_AWB_BG_HI_THRESHOLD_CFG_0    highThreshold0;   ///< AWB BG stats pixel upper threshold config
    IFE_IFE_0_VFE_STATS_AWB_BG_HI_THRESHOLD_CFG_1    highThreshold1;   ///< AWB BG stats pixel upper threshold config
    IFE_IFE_0_VFE_STATS_AWB_BG_LO_THRESHOLD_CFG_0    lowThreshold0;    ///< AWB BG stats pixel lower threshold config
    IFE_IFE_0_VFE_STATS_AWB_BG_LO_THRESHOLD_CFG_1    lowThreshold1;    ///< AWB BG stats pixel lower threshold config
} CAMX_PACKED;

CAMX_END_PACKED

struct IFEAWBBG14RegCmd
{
    IFEAWBBG14RegionConfig                 regionConfig;    ///< AWB BG stats region and threshold config
    IFE_IFE_0_VFE_STATS_AWB_BG_CH_Y_CFG    lumaConfig;      ///< AWB BG stats luma channel config
    IFE_IFE_0_VFE_STATS_AWB_BG_CFG         AWBBGStatsconfig;   ///< AWB BG stats config
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief AWB BG Stats14 Class Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AWBBGStats14 final : public ISPStatsModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create AWBBGStats14 Object
    ///
    /// @param  pCreateData Pointer to the AWBBGStats14 Creation
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
    /// GetDMITable
    ///
    /// @brief  Retrieve the DMI LUT
    ///
    /// @param  ppDMITable Pointer to which the module should update different DMI tables
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID GetDMITable(
       UINT32** ppDMITable);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~AWBBGStats14
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~AWBBGStats14();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AWBBGStats14
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    AWBBGStats14();

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
        ISPInputData* pInputData);

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
        ISPInputData* pInputData);

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
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureRegisters();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AdjustROIParams
    ///
    /// @brief  Adjust ROI from stats to requirement based on hardware
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID AdjustROIParams();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of AWB BG module for debug
    ///
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateIFEInternalData
    ///
    /// @brief  Update IFE internal data
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateIFEInternalData(
        ISPInputData * pInputData);

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

    AWBBGStats14(const AWBBGStats14&)            = delete;    ///< Disallow the copy constructor
    AWBBGStats14& operator=(const AWBBGStats14&) = delete;    ///< Disallow assignment operator

    IFEAWBBG14RegCmd    m_regCmd;                             ///< Register list for AWB AWBBG stats
    BGBEConfig          m_AWBBGConfig;                        ///< AWBBG configuration data from stats
    UINT32              m_regionWidth;                        ///< AWB individual region width
    UINT32              m_regionHeight;                       ///< AWB individual region height
    BOOL                m_isAdjusted;                         ///< Flag to indicate if ROI from stats was modified
};

CAMX_NAMESPACE_END

#endif // CAMXIFEAWBBGSTATS14_H
