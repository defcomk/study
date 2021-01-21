////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifebfstats23.h
/// @brief IFEBFStats23 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFEBFSTATS23_H
#define CAMXIFEBFSTATS23_H

#include "chistatsproperty.h"

#include "camxispstatsmodule.h"

CAMX_NAMESPACE_BEGIN

/// @todo (CAMX-1404): BF Stats Pass GammaSupported and Scaler supported from OEM
// BF stats modular support selection
static const UINT32 GammaSupported            = TRUE;
static const UINT32 DownScalerSupported       = TRUE;

// BF minimum margins
static const UINT32 HorizontalFIRMargin       = 12;
static const UINT32 VerticalFIRMargin         = 4;
static const UINT32 HorizontalIIRMargin       = 64;
static const UINT32 VerticalIIRMargin         = 32;
static const UINT32 MinXOffset                = HorizontalFIRMargin + HorizontalIIRMargin;
static const UINT32 MinYOffset                = VerticalFIRMargin + VerticalIIRMargin;
static const UINT32 MinWidth                  = 6;
static const UINT32 MinHeight                 = 8;

// BF stats ROI overlap constrains
static const UINT32 MinStartPixelOverlap      = 6;
static const UINT32 MinEndPixelOverlap        = 6;
static const UINT32 MaxHorizontalRegions      = 21;
static const UINT32 MaxVerticalSpacing        = 8;
static const UINT32 MaxOverlappingRegions     = 0;

// BF stats Gamma Constats offset
static const UINT32 BFGammaEntries                = 32;
static const UINT32 BFGammaDMIDeltaShift          = 14;
static const UINT32 BFGammaDownscaleFactor        = 1;
static const UINT32 BFGammaDownscaleFactorTwo     = 2;
static const UINT32 BFGammaDownscaleFactorFour    = 4;
static const UINT32 BFGammaDownscaleFactorEight   = 8;
static const UINT32 BFGammaDownscaleFactorSixteen = 16;
static const UINT32 BFGammaUseYChannel            = FALSE;
static const UINT32 BFGammaMaxValue               = (1 << IFEPipelineBitWidth) - 1;    ///< 14 bit value (1 << 14) - 1
static const INT32  BFGammaDeltaMinValue          = -8192;
static const INT32  BFGammaDeltaMaxValue          = 8191;
static const UINT32 LumaConversionConfigShift     = 11;
static const UINT32 NumLumaCoefficients           = 3;
static const UINT32 ConfigGSelectGR               = 0x0;
static const UINT32 ConfigGSelectGB               = 0x1;
static const UINT32 ConfigChannelSelectG          = 0x0;
static const UINT32 ConfigChannelSelectY          = 0x1;
static const FLOAT  BFGammaScaleRatioMinValue     = 0.125f;
static const FLOAT  BFGammaScaleRatioMaxValue     = 8.0f;

// BF stats Dowm Scaler constants
static const UINT32 ScaleMaxHorizontalConfig  = 16383;
static const UINT32 ScaleMaxVerticalConfig    = 16383;
static const UINT32 PhaseAdder                = 14;

// LUT DMI description
static const UINT32 DMILeftBits               = 0x1FFF; // 13-bit mask
static const UINT32 DMITopBits                = 0x3FFF; // 14-bit mask
static const UINT32 DMIWidthBits              = 0xFFF;  // 12-bit mask
static const UINT32 DMIHeightBits             = 0x1FFF; // 13-bit mask
static const UINT32 DMIIDBits                 = 0;
static const UINT32 DMISelShift               = 63;
static const UINT32 DMIIndexShift             = 54;
static const UINT32 DMILeftShift              = 41;
static const UINT32 DMITopShift               = 27;
static const UINT32 DMIWidthShift             = 14;
static const UINT32 DMIHeightShift            = 0;
static const UINT32 DMIIDShift                = 0;

// BF stats FIR, IIR coring config bits
static const UINT8  FIRBits                   = 6;
static const UINT8  IIRBits                   = 16;
static const UINT8  ShifterBits               = 4;
static const UINT32 ThresholdMask             = 0x3FFFF;
static const UINT32 CoringBits                = 0x3FF;
static const UINT32 BF_GAMMA_ENTRIES          = 32;

// BF stats YA config limit
static const FLOAT  YAConfigMin                = 0.0f;
static const FLOAT  YAConfigMax                = 1.0f;
static const FLOAT  YAConfigCoEffSum           = 1.0f;
static const FLOAT  YAConfigOverrideRatioMax   = 1.999f;
static const FLOAT  YAConfigNoOverrideRatioMax = 1.0f;

// BF stats interpolation const
static const UINT32 InterpolationResolution0 = 0;
static const UINT32 InterpolationResolution1 = 1;
static const UINT32 InterpolationResolution2 = 2;
static const UINT32 InterpolationResolution3 = 3;
static const UINT32 InterpolationResolution4 = 4;

// BF stats interpolation const
static const UINT32 ScaleRatio2  = 2;
static const UINT32 ScaleRatio4  = 4;
static const UINT32 ScaleRatio8  = 8;
static const UINT32 ScaleRatio16 = 16;

// BF stats buffer output size
static const UINT32 BFStatsMaximumRegion   = 180;
static const UINT32 BFStatsNumberOfEntries = 4;
static const UINT32 BFStatsSizePerEntries  = 4;

// BF stats DMI length in DWord
static const UINT32 IFEBF23ROIDMILengthDWord      = (BFStatsMaximumRegion + 2) * sizeof(UINT64) / sizeof(UINT32);
static const UINT32 IFEBF23GammaLUTDMILengthDWord = MaxBFGammaEntries;

// BF stats region index LUT bank select
static const UINT IFEBF23RegionIndexLUTBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_STATS_BAF_RGN_IND_LUT_BANK0;
static const UINT IFEBF23RegionIndexLUTBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_STATS_BAF_RGN_IND_LUT_BANK1;

// BF stats gamma LUT bank select
static const UINT IFEBF23GammeLUTBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_STATS_BAF_GAMMA_LUT_BANK0;
static const UINT IFEBF23GammeLUTBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_STATS_BAF_GAMMA_LUT_BANK1;

// BF stats region index LUT bank select values
static const UINT IFEBF23RegionIndexLUTBank0SelectValue = 0;
static const UINT IFEBF23RegionIndexLUTBank1SelectValue = 1;

// BF stats gamma LUT bank select values
static const UINT IFEBF23GammaLUTBank0SelectValue = 0;
static const UINT IFEBF23GammaLUTBank1SelectValue = 1;


CAMX_BEGIN_PACKED

/// @brief IFE BAF Stats Configuration
struct IFEBF23Config
{
    IFE_IFE_0_VFE_STATS_BAF_CFG                       config;          ///< Config
    IFE_IFE_0_VFE_STATS_BAF_Y_CONV_CFG_0              yConvConfig0;    ///< Y color conversion config 0
    IFE_IFE_0_VFE_STATS_BAF_Y_CONV_CFG_1              yConvConfig1;    ///< Y color conversion config 1
} CAMX_PACKED;

/// @todo (CAMX-1468): Discuss if we want to change the variable names in BF stats
/// @brief IFE Register set for H FIR and IIR config
struct IFEBF23HFIRIIRConfig
{
    IFE_IFE_0_VFE_STATS_BAF_H_1_FIR_CFG_0             h1FIRConfig0;    ///< Horizontal1 FIR config 0
    IFE_IFE_0_VFE_STATS_BAF_H_1_FIR_CFG_1             h1FIRConfig1;    ///< Horizontal1 FIR config 1
    IFE_IFE_0_VFE_STATS_BAF_H_1_FIR_CFG_2             h1FIRConfig2;    ///< Horizontal1 FIR config 2
    IFE_IFE_0_VFE_STATS_BAF_H_1_FIR_CFG_3             h1FIRConfig3;    ///< Horizontal1 FIR config 3
    IFE_IFE_0_VFE_STATS_BAF_H_1_IIR_CFG_0             h1IIRConfig0;    ///< Horizontal1 IIR config 0
    IFE_IFE_0_VFE_STATS_BAF_H_1_IIR_CFG_1             h1IIRConfig1;    ///< Horizontal1 IIR config 1
    IFE_IFE_0_VFE_STATS_BAF_H_1_IIR_CFG_2             h1IIRConfig2;    ///< Horizontal1 IIR config 2
    IFE_IFE_0_VFE_STATS_BAF_H_1_IIR_CFG_3             h1IIRConfig3;    ///< Horizontal1 IIR config 3
    IFE_IFE_0_VFE_STATS_BAF_H_1_IIR_CFG_4             h1IIRConfig4;    ///< Horizontal1 IIR config 4
} CAMX_PACKED;

/// @brief IFE Register set for V IIR config 1
struct IFEBF23VIIRConfig1
{
    IFE_IFE_0_VFE_STATS_BAF_V_IIR_CFG_0               vIIRConfig0;    ///< Vertical IIR config 0
    IFE_IFE_0_VFE_STATS_BAF_V_IIR_CFG_1               vIIRConfig1;    ///< Vertical IIR config 1
    IFE_IFE_0_VFE_STATS_BAF_V_IIR_CFG_2               vIIRConfig2;    ///< Vertical IIR config 2
} CAMX_PACKED;

/// @brief IFE Register set for V IIR config 2
struct IFEBF23VIIRConfig2
{
    IFE_IFE_0_VFE_STATS_BAF_V_IIR_CFG_3               vIIRConfig3;    ///< Vertical IIR config 3
    IFE_IFE_0_VFE_STATS_BAF_V_IIR_CFG_4               vIIRConfig4;    ///< Vertical IIR config 4
} CAMX_PACKED;

/// @brief IFE Register set for H Threshold config
struct IFEBF23HThresholdCoringConfig
{
    IFE_IFE_0_VFE_STATS_BAF_H_1_TH_CFG                h1ThresholdConfig;    ///< Horizontal1 Threshold Config
    IFE_IFE_0_VFE_STATS_BAF_H_1_CORING_CFG_0          h1CoringConfig0;      ///< Horizontal1 Coring Config 0
    IFE_IFE_0_VFE_STATS_BAF_H_1_CORING_CFG_1          h1CoringConfig1;      ///< Horizontal1 Coring Config 1
    IFE_IFE_0_VFE_STATS_BAF_H_1_CORING_CFG_2          h1CoringConfig2;      ///< Horizontal1 Coring Config 2
    IFE_IFE_0_VFE_STATS_BAF_H_1_CORING_CFG_3          h1CoringConfig3;      ///< Horizontal1 Coring Config 3
} CAMX_PACKED;

/// @brief IFE Register set for V Threshold config
struct IFEBF23VThresholdCoringConfig
{
    IFE_IFE_0_VFE_STATS_BAF_V_TH_CFG                  vThresholdConfig;      ///< Vertical Threshold Config
    IFE_IFE_0_VFE_STATS_BAF_V_CORING_CFG_0            vCoringConfig0;        ///< Vertical Coring Config 0
    IFE_IFE_0_VFE_STATS_BAF_V_CORING_CFG_1            vCoringConfig1;        ///< Vertical Coring Config 1
    IFE_IFE_0_VFE_STATS_BAF_V_CORING_CFG_2            vCoringConfig2;        ///< Vertical Coring Config 2
    IFE_IFE_0_VFE_STATS_BAF_V_CORING_CFG_3            vCoringConfig3;        ///< Vertical Coring Config 3
} CAMX_PACKED;

/// @brief IFE Register set for Coring Config
struct IFEBF23CoringConfig
{
    IFE_IFE_0_VFE_STATS_BAF_CORING_GAIN_CFG_0         coringGainConfig0;         ///< Vertical Coring Gain Config 0
    IFE_IFE_0_VFE_STATS_BAF_CORING_GAIN_CFG_1         coringGainConfig1;         ///< Vertical Coring Gain Config 1
} CAMX_PACKED;

/// @brief IFE Register set for Interrupt Config
struct IFEBF23PhaseStripeInterruptConfig
{
    IFE_IFE_0_VFE_STATS_BAF_SCALE_H_IMAGE_SIZE_CFG    hScaleImageSizeConfig;     ///< Horizontal image size config
    IFE_IFE_0_VFE_STATS_BAF_SCALE_H_PHASE_CFG         hScalePhaseConfig;         ///< Horizontal Phase config
    IFE_IFE_0_VFE_STATS_BAF_SCALE_H_STRIPE_CFG_0      hScaleStripeConfig0;       ///< Horizontal Scale stripe config0
    IFE_IFE_0_VFE_STATS_BAF_SCALE_H_STRIPE_CFG_1      hScaleStripeConfig1;       ///< Horizontal Scale stripe config1
    IFE_IFE_0_VFE_STATS_BAF_SCALE_H_PAD_CFG           hScalePadConfig;           ///< Horizontal Scale pad config
    IFE_IFE_0_VFE_STATS_BAF_EARLY_INTR_CFG            earlyInterruptConfig;      ///< early interrupt config
} CAMX_PACKED;

/// @brief IFE Register set for active window config
struct IFEBF23ActiveWindow
{
    IFE_IFE_0_VFE_STATS_BAF_ACTIVE_WINDOW_CFG_0       bafActiveWindow1;          ///< Active window config0
    IFE_IFE_0_VFE_STATS_BAF_ACTIVE_WINDOW_CFG_1       bafActiveWindow2;          ///< Active window config1
} CAMX_PACKED;

CAMX_END_PACKED

/// @brief IFE RegCmd for BF stats
struct IFEBF23RegCmd
{
    IFEBF23Config                                     BFConfig;                  ///< baf config
    IFEBF23HFIRIIRConfig                              hFIRIIRConfig;             ///< Horizontal FIR IIR config
    IFEBF23VIIRConfig1                                vIIRConfig1;               ///< Vertical IIR config 1
    IFEBF23VIIRConfig2                                vIIRConfig2;               ///< Vertical IIR config 1
    IFEBF23ActiveWindow                               activeWindow;              ///< Active Window
    IFE_IFE_0_VFE_STATS_BAF_SHIFT_BITS_CFG            shiftBitsConfig;           ///< shift bits config
    IFEBF23HThresholdCoringConfig                     hThresholdCoringConfig;    ///< Horizontal Coring config
    IFEBF23VThresholdCoringConfig                     vThresholdCoringConfig;    ///< Vertical Coring config
    IFEBF23CoringConfig                               coringConfig;              ///< Coring Config
    IFEBF23PhaseStripeInterruptConfig                 phaseStripeConfig;         ///< Phase Stripe Config
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief enum identify filter type
enum class IFEBFFilterType
{
    Horizontal1 = 1,    ///< Horizontal1 filter type
    Horizontal2,        ///< Horizontal2 filter type
    Vertical            ///< Vertical filter type
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief IFEBFStats23 Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFEBFStats23 final : public ISPStatsModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create IFEBFStats23 Object
    ///
    /// @param  pCreateData Pointer to the IFEBFStats23 Creation
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
    /// ~IFEBFStats23
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFEBFStats23();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEBFStats23
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    IFEBFStats23();

private:
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
        ISPInputData* pInputData);

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
    /// @return CamxResult Indicates if the input is valid or invalid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CheckDependenceChange(
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
    /// CreateCmdList
    ///
    /// @brief  Create the Command List
    ///
    /// @param  pInputData Pointer to the IFE input data
    ///
    /// @return CamxResult - Indicates if the cmdlist creation was successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateAndAdjustROIBoundary
    ///
    /// @brief  Validate and Adjust if the number of ROIs in in any line does not exceed the max limitation of hardware
    ///
    /// @param  pInputData      Pointer to ISP input data
    /// @param  pBFStatsConfig  BF stats configuration
    ///
    /// @return True if ROI is adjusted
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL ValidateAndAdjustROIBoundary(
        ISPInputData*   pInputData,
        AFConfigParams* pBFStatsConfig);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AdjustScaledROIBoundary
    ///
    /// @brief  Validate and Adjust ROI based on BAF Scaling
    ///
    /// @param  pInputData      Pointer to ISP input data
    /// @param  pRoi            Pointer to the ROI to be adjusted
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID AdjustScaledROIBoundary(
        ISPInputData*   pInputData,
        RectangleCoordinate* pRoi);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SortRegionsStartPixelOrderBased
    ///
    /// @brief  Bubble sort ROI in starting pixel order
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SortRegionsStartPixelOrderBased();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckAndAdjustStartPixelROILimitation
    ///
    /// @brief  Check whether both ROI starts on same pixel or starting line apart by min, and then adjust
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID CheckAndAdjustStartPixelROILimitation();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RegionsOverlapping
    ///
    /// @brief  Check if the input ROI's overlap
    ///
    /// @param  currentROI Current ROI parameters
    /// @param  nextROI    Next ROI parameters
    ///
    /// @return BOOL       Return TRUE if ROI's overlap FALSE otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL RegionsOverlapping(
        BFStatsROIDimensionParams currentROI,
        BFStatsROIDimensionParams nextROI);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckMaxHorizontalLimitationPerRow
    ///
    /// @brief  Validate if the number of ROIs in in any line does not exceed the max limitation of hardware
    ///
    /// @return CamxResult  Returns success if the ROI config to be comsumed is valid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CheckMaxHorizontalLimitationPerRow();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckMaxVerticalSpacingAndDisjoint
    ///
    /// @brief  Validate max vertical spacing between i'th and i + 21st region and validate disjoint ROI's
    ///
    /// @return CamxResult  Return success if the vertical spacing between disjoint ROI is valid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CheckMaxVerticalSpacingAndDisjoint();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckMaxRegionOverlap
    ///
    /// @brief  Validate max overlapping region specified by hardware limitation
    ///
    /// @return CamxResult Returns success if the max overlapping region is valid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CheckMaxRegionOverlap();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// LumaConversionConfig
    ///
    /// @brief  Configure BF stats Luma conversion registers
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID LumaConversionConfig();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// YCoefficientSumCheck
    ///
    /// @brief  Check the sum of Y conversion co-efficients
    ///
    /// @return BOOL Return true if the sum of co-efficients are less than equal to 1.0f
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL YCoefficientSumCheck() const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DownscalerConfig
    ///
    /// @brief  Configure BF stats Down scaler registers
    ///
    /// @param  pInputData       Pointer to the InputData
    /// @param  pBFStatsConfig   BF stats configuration detail
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DownscalerConfig(
        ISPInputData*   pInputData,
        AFConfigParams* pBFStatsConfig);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DownscalerCalculateInterpolationResolution
    ///
    /// @brief  Create the Command List
    ///
    /// @param  pBFStatsConfig  BF stats configuration detail
    ///
    /// @return UINT32 Interpolation resolution value
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    UINT32 DownscalerCalculateInterpolationResolution(
        AFConfigParams* pBFStatsConfig
        ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureFilters
    ///
    /// @brief  Configure BF stats FIR, IIR and coring registers
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureFilters();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetActiveWindow
    ///
    /// @brief  Configure BF stats active window
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SetActiveWindow();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureFIRFilters
    ///
    /// @brief  Configure BF stats FIR registers
    ///
    /// @param  pFIRConfig BF stats FIR configuration data
    /// @param  filter     type of filter
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureFIRFilters(
        BFFIRFilterConfigType*  pFIRConfig,
        IFEBFFilterType         filter);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureIIRFilters
    ///
    /// @brief  Configure BF stats IIR registers
    ///
    /// @param  pIIRConfig BF stats IIR configuration data
    /// @param  filter     type of filter
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureIIRFilters(
        BFIIRFilterConfigType*  pIIRConfig,
        IFEBFFilterType         filter);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateROIDMITable
    ///
    /// @brief  Update the DMI LUT for ROI
    ///
    /// @param  pHWROIConfig    BF ROI Config
    /// @param  pROIDMIConfig   DMI configuration
    /// @param  sizeOfDMIConfig size of DMI configuraiton array
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateROIDMITable(
        BFStatsROIConfigType*   pHWROIConfig,
        UINT64*                 pROIDMIConfig,
        UINT32                  sizeOfDMIConfig);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateHWROI
    ///
    /// @brief  Update the valid list of HW ROI config
    ///
    /// @param  pInputROIConfig Input ROI configuration
    /// @param  pHWROIConfig    HW configuration to be updated
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateHWROI(
        BFStatsROIConfigType* pInputROIConfig,
        BFStatsROIConfigType* pHWROIConfig);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SortRegionsEndPixelOrderBased
    ///
    /// @brief  Bubble sort ROI in ending pixel order
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SortRegionsEndPixelOrderBased();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckAndAdjustEndPixelROILimitation
    ///
    /// @brief  Check whether both ROI ends on same pixel or ending line apart by min, adn then adjust
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID CheckAndAdjustEndPixelROILimitation();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureCoring
    ///
    /// @brief  Configure coring registers
    ///
    /// @param  pCoringConfig BF stats coring configuration data
    /// @param  filter        type of filter
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureCoring(
        BFFilterCoringConfigParams* pCoringConfig,
        IFEBFFilterType             filter);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureGamma
    ///
    /// @brief  Configure BF stats Gamma registers
    ///
    /// @param  pInputData Pointer to the IFE input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureGamma(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GammaGetHighLowBits
    ///
    /// @brief  Pack the higher 12 bits in the configuration contains the delta between the current GammaTable value and
    ///         the next value, while the lower 12 bits contains the current GammaTable value
    ///
    /// @param  pGamma Pointer to the gamma table
    /// @param  index  Current index in the gamma table
    ///
    /// @return INT32  Packed HW LUT entry
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    INT32 GammaGetHighLowBits(
        UINT32* pGamma,
        UINT32  index
        ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GammaGetLastValue
    ///
    /// @brief  Fetch the last HW LUT gamma value packed higher 12 bits with delta between the last GammaTable value and
    ///         the max value, while the lower 12 bits contains the current GammaTable value
    ///
    /// @param  pGamma Pointer to the gamma table
    ///
    /// @return UINT32  Packed HW LUT entry
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    UINT32 GammaGetLastValue(
        UINT32* pGamma
        ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// HandleEmptyROI
    ///
    /// @brief  Handle empty ROI dimensions
    ///
    /// @param  pInputData  Pointer to ISP input data
    /// @param  pROIOut     ROI configuration to be used
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID HandleEmptyROI(
        ISPInputData*           pInputData,
        BFStatsROIConfigType*   pROIOut);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ResetPerFrameData
    ///
    /// @brief  Reset per frame data before processing current stats configuration
    ///
    /// @param  pInputData  Pointer to ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ResetPerFrameData(
        ISPInputData* pInputData);

    /// @todo (CAMX-1301): Enable BF stats meta reporting

    IFEBFStats23(const IFEBFStats23&)            = delete;          ///< Disallow the copy constructor
    IFEBFStats23& operator=(const IFEBFStats23&) = delete;          ///< Disallow assignment operator

    IFEBF23RegCmd           m_regCmd;                               ///< Register list for IFE BF stats
    UINT64                  m_ROIDMIConfig[BFMaxROIRegions + 1];    ///< DMI ROI configuration
    UINT32                  m_gammaLUT[MaxBFGammaEntries];          ///< Gamma LUT
    UINT32                  m_gammaSupported;                       ///< BF stats gamma supported flag
    UINT32                  m_downScalerSupported;                  ///< BF stats Down scaler supported flag
    UINT32*                 m_pHorizontalRegionCount;               ///< Counter to keep to track of no of ROI per line
    AFConfigParams          m_BFStatsConfig;                        ///< BF configuration data from stats
    FLOAT                   m_refSensitivity;                       ///< Exposure Index, Same as Lux index for non ADRC cases
    BOOL                    m_isInputConfigAdjusted;                ///< Flag to check if input configuration has been adjusted
    BOOL                    m_inputConfigUpdate;                    ///< Track if the input config has been updated
    BOOL                    m_ROIConfigUpdate;                      ///< Track if we need to update the ROI
    UINT32                  m_CAMIFWidth;                           ///< CAMIF width to compare against ROI width
    UINT32                  m_CAMIFHeight;                          ///< CAMIF height to compare against ROI height
    UINT32                  m_gammaDownscaleFactor;                 ///< Down scale factor for BF stats gamma
    BFStatsROIConfigType    m_endOrderROI;                          ///< End pixel reorder of the ROI
    BFStatsROIConfigType    m_hwROIConfig;                          ///< Valid list of HW ROI config
    AFConfigParams          m_previousAFConfig;                     ///< Previous working AF config
};

CAMX_NAMESPACE_END

#endif // CAMXIFEBFSTATS23_H
