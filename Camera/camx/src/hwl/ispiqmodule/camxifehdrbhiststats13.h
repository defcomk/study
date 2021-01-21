////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, 2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifehdrbhiststats13.h
/// @brief HDR Bayer Histogram (HDRBHist) stats v1.3
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXIFEHDRBHISTSTATS13_H
#define CAMXIFEHDRBHISTSTATS13_H

#include "camxispstatsmodule.h"

CAMX_NAMESPACE_BEGIN

// Register range from the SWI/HPG
static const UINT32 HDRBHistStats13RegionWidth      = 2;    ///< HDR BHist stats region width
static const UINT32 HDRBHistStats13RegionHeight     = 2;    ///< HDR BHist stats region Height

/// @note To keep simple configuration between non-HDR and HDR sensors, use worst case minimum horizontal/vertical region
// Region program a value of n means: n + 1
static const INT32 HDRBHistStats13MinHorizregions  = 3;   ///< Minimum HDR mode HDRBHist stats horizontal region
static const INT32 HDRBHistStats13MinVertregions   = 1;   ///< Minimum HDR mode HDRBHist stats vertical region

// Green channel source options: Gr or Gb
static const UINT32 HDRBHistGreenChannelSelectGr    = 0x0;  ///< Channel selection for Gr channel
static const UINT32 HDRBHistGreenChannelSelectGb    = 0x1;  ///< Channel selection for Gb channel

// Input Field selection for HDR_BHIST: non-HDR(use All) / HDR mode (use T1 or T2) mode
static const UINT32 HDRBHistFieldSelectAll          = 0x0;  ///< Non-HDR: All lines
static const UINT32 HDRBHistFieldSelectT1           = 0x1;  ///< HDR: T1 long exposure
static const UINT32 HDRBHistFieldSelectT2           = 0x2;  ///< HDR: T2 short exposure

static const UINT32 HDRBHistBinsPerChannel          = 256;  ///< Number of bins per channel
static const UINT32 HDRBHistStatsSingleBinSize      = 4;    ///< Bin size in bytes, but only 25 LSB bits have data

CAMX_BEGIN_PACKED

/// @brief HDRBHist Stats Configuration
struct IFEHDRBHistRegionConfig
{
    IFE_IFE_0_VFE_STATS_HDR_BHIST_RGN_OFFSET_CFG    regionOffset;   ///< BHist stats region offset config
    IFE_IFE_0_VFE_STATS_HDR_BHIST_RGN_NUM_CFG       regionNumber;   ///< BHist stats region number config
} CAMX_PACKED;

CAMX_END_PACKED

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief BHist Stats14 Class Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class HDRBHistStats13 final : public ISPStatsModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create HDRBHistStats13 Object
    ///
    /// @param  pCreateData Pointer to the HDRBHistStats13 Creation
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
    /// ~HDRBHistStats13
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~HDRBHistStats13();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// HDRBHistStats13
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    HDRBHistStats13();

private:
    /// @brief HDRBHist region configuration
    struct HDRBHistRegionConfig
    {
        UINT32  offsetHorizNum; ///< HDRBHist stats num of horizontal regions
        UINT32  offsetVertNum;  ///< HDRBHist stats num of vertical regions
        UINT32  regionHorizNum; ///< HDRBHist stats num of horizontal regions
        UINT32  regionVertNum;  ///< HDRBHist stats num of vertical regions
    };

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
    /// @brief  Update BHist configuration to the Internal Data
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
    /// @param  pInputData   Pointer to the ISP input data
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
    /// @param  pRegionConfig    Region configuration to be set
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureRegisters(
        const HDRBHistRegionConfig* pRegionConfig);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CalculateRegionConfiguration
    ///
    /// @brief  Adjust ROI from stats to requirement based on hardware, gets required region configuration from ROI
    ///
    /// @param  pRegionConfig   Outputs region configuration for HDRBHist
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID CalculateRegionConfiguration(
        HDRBHistRegionConfig* pRegionConfig);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of HDR BHist module for debug
    ///
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig();

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

    HDRBHistStats13(const HDRBHistStats13&)            = delete;    ///< Disallow the copy constructor
    HDRBHistStats13& operator=(const HDRBHistStats13&) = delete;    ///< Disallow assignment operator

    IFEHDRBHistRegionConfig m_regCmd;               ///< HDRBHist stats region configuration
    ISPHDRBHistStatsConfig  m_HDRBHistConfig;       ///< HDRBHist configuration data from stats
    UINT32                  m_regionMultipleFactor; ///< HDRBHist region offset and dimension multiple factor
    UINT32                  m_greenChannelSelect;   ///< Green channel selection between Gr and Gb
    UINT32                  m_inputFieldSelect;     ///< Input Field selection for stats in non-HDR(All) and HDR(T1/T2) mode
};

CAMX_NAMESPACE_END

#endif // CAMXIFEHDRBHISTSTATS13_H
