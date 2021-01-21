////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeihiststats12.h
/// @brief IFE Image Histogram class v1.2
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFEIHISTSTATS12_H
#define CAMXIFEIHISTSTATS12_H

#include "camxispstatsmodule.h"

CAMX_NAMESPACE_BEGIN

// Register range from the SWI/HLD
static const UINT32 IHistStats12Channels        = 4;    ///< Total number of channels available at any given time
static const UINT32 IHistStats12BinsPerChannel  = 256;  ///< Number of bins per channel
static const UINT32 IHistStats12StatsBits       = 16;   ///< Image Histogram stats output significant bits per bin.
static const UINT32 IHistStats12MaxStatsShift   = 8;    ///< Image Histogram maximum stats output shifting bits

static const UINT32 IHistStats12RegionWidth     = 2;    ///< Image Histogram stats region width
static const UINT32 IHistStats12RegionHeight    = 2;    ///< Image Histogram stats region height

static const UINT32 IHistStats12RegionMinHNum   = 1;    ///< Min programable horizontal num of regions. If HNum means HNum+1
static const UINT32 IHistStats12RegionMinVNum   = 0;    ///< Min programable vertical num of regions. If VNum means VNum+1

// IHist HW buffer channel order. List the order of the data in the HW otput buffer.
static const UINT32 IHistStats12ChannelYCC      = 0;    ///< YCC channel, either Y, Cb or Cr option
static const UINT32 IHistStats12ChannelGreen    = 1;    ///< Green channel
static const UINT32 IHistStats12ChannelBlue     = 2;    ///< blue channel
static const UINT32 IHistStats12ChannelRed      = 3;    ///< Red channel

// Select for YCC image histogram channel. YCC channel could have one of the following data.
static const UINT32 IHistStats12ChannelYCC_Y    = 0;    ///< Y selection
static const UINT32 IHistStats12ChannelYCC_Cb   = 1;    ///< Cb selection
static const UINT32 IHistStats12ChannelYCC_Cr   = 2;    ///< Cr selection

// Site where the input source of image histogram is collected from
/// @note Tapout site is independent of the histogram HW version, it depends on IFE/VFE version. But keeping it here for the
///       time being. No changes expected.
static const UINT32 IHistTapoutBeforeGLUT       = 0x0;  ///< Image Histogram tap out before RGB LUT / GLUT
static const UINT32 IHistTapoutAfterGLUT        = 0x1;  ///< Image Histogram tap out before RGB LUT / GLUT

static const UINT32 IHistStats12MinNumberOfBins = 8;    ///< Worst case, all pixels fall into these number of bins

CAMX_BEGIN_PACKED
/// @brief Image Histogram region configuration
struct IFEIHist12RegionConfig
{
    IFE_IFE_0_VFE_STATS_IHIST_RGN_OFFSET_CFG    regionOffset;   ///< Image Histogram region offset
    IFE_IFE_0_VFE_STATS_IHIST_RGN_NUM_CFG       regionNumber;   ///< Image Histogram region number
} CAMX_PACKED;

CAMX_END_PACKED

/// @brief Image histogram register configuration
struct IFEIHist12RegCmd
{
    IFEIHist12RegionConfig      regionConfig;   ///< Image Histogram region selection
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Image Histogram Class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IHistStats12 final : public ISPStatsModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create IHistStats12 Object
    ///
    /// @param  pCreateData Pointer to the IFE stats module to create Image Histogram class
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IFEStatsModuleCreateData* pCreateData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Execute
    ///
    /// @brief  Execute process request to configure Image Histogram
    ///
    /// @param  pInputData  Pointer to the ISP input data
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
    /// ~IHistStats12
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IHistStats12();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IHistStats12
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    IHistStats12();

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateDependenceParams
    ///
    /// @brief  Validate region of interest configuration given as input
    ///
    /// @param  pInputData  Pointer to the ISP input data
    ///
    /// @return CamxResultSuccess if valid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ValidateDependenceParams(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckDependenceChange
    ///
    /// @brief  Check to see if the configuration Trigger Data Changed
    ///
    /// @param  pInputData  Pointer to the ISP input data
    ///
    /// @return TRUE if configuration has change
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckDependenceChange(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculation
    ///
    /// @brief  Perform the configuration of Image Histogram statistics
    ///
    /// @param  pInputData  Pointer to the ISP input data
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
    /// CalculateRegionConfiguration
    ///
    /// @brief  Calculate region width and height from stats to requirement based on hardware
    ///
    /// @param  pRegionWidth  pointer to Image Histogram region width
    /// @param  pRegionHeight pointer to Image Histogram region height
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID CalculateRegionConfiguration(
        UINT32* pRegionWidth,
        UINT32* pRegionHeight);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureRegisters
    ///
    /// @brief  Fill register configuration data from the class
    ///
    /// @param  pInputData  Pointer to the ISP input data
    /// @param  regionVNum  Image Histogram region width
    /// @param  regionHNum  Image Histogram ROI region height
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureRegisters(
        ISPInputData* pInputData,
        const UINT32  regionVNum,
        const UINT32  regionHNum);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateCmdList
    ///
    /// @brief  Create command list and write configuration to command buffer
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of Image Histogram module for debug
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
    /// @param  pInputData  Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateIFEInternalData(
        ISPInputData * pInputData);

    IHistStats12(const IHistStats12&) = delete;             ///< Disallow the copy constructor
    IHistStats12& operator=(const IHistStats12&) = delete;  ///< Disallow assignment operator

    BOOL                m_defaultConfig;            ///< Set if doing default configuration defined by this class.
    IFEIHist12RegCmd    m_regCmd;                   ///< Register list for Image Histogram stats
    UINT8               m_IHistYCCChannelSelection; ///< YCC channel selection:
                                                    ///  0: Luma (Y), 1: Chroma blue (Cb), 2: Chroma red (Cr)
    UINT8               m_IHistSiteSelection;       ///< Site where the source of iHist is collected/tap:
                                                    ///  0: Before RGB LUT, 0:After RGB LUT
    UINT8               m_IHistShiftBits;           ///< Programmable right shift bits for the accumulated statistics IHIST
                                                    ///  results. The value is programmed starting from 0 to 8. Program a value
                                                    ///  of n means n bit right shift.
    UINT32              m_maxPixelSumPerBin;        ///< Max histogram pixels sum to be represented per bin. Histogram pixels
                                                    ///  are the pixels consider by the histogram. If zero use default shift
                                                    ///  estimation.

    RectangleCoordinate m_currentROI;               ///< Current region of interest, if zero means it has not been set yet
};

CAMX_NAMESPACE_END

#endif // CAMXIFEIHISTSTATS12_H
