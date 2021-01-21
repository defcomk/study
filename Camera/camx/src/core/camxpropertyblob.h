////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file camxpropertyblob.h
/// @brief Define Qualcomm Technologies, Inc. proprietary blob for holding internal properties/events
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXPROPERTYBLOB_H
#define CAMXPROPERTYBLOB_H

#include "chi.h"
#include "chiispstatsdefs.h"
#include "chistatsproperty.h"
#include "chiifedefs.h"
#include "chiipedefs.h"
#include "chiafcommon.h"
#include "chituningmodeparam.h"
#include "chieisdefs.h"

#include "camxchidefs.h"
#include "camxdefs.h"
#include "camxifeproperty.h"
#include "camxsensorproperty.h"
#include "camxstatsinternalproperty.h"
#include "camxjpegproperty.h"
#include "camxfdproperty.h"
#include "camxlrmeproperty.h"
#include "camxtypes.h"

CAMX_NAMESPACE_BEGIN

/// @brief Property type
typedef UINT32 PropertyID;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Definition of Qualcomm Technologies, Inc. internal properties.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Additional classification of tags/properties
enum class PropertyGroup : UINT32
{
    Result    = 0x3000, ///< Properties found in the result pool
    Internal  = 0x4000, ///< Properties found in the internal pool
    Usecase   = 0x5000, ///< Properties found in the usecase pool
    DebugData = 0x6000, ///< Properties found in the debug data pool
};

static const PropertyID PropertyIDPerFrameResultBegin    = static_cast<UINT32>(PropertyGroup::Result)    << 16;
static const PropertyID PropertyIDPerFrameInternalBegin  = static_cast<UINT32>(PropertyGroup::Internal)  << 16;
static const PropertyID PropertyIDUsecaseBegin           = static_cast<UINT32>(PropertyGroup::Usecase)   << 16;
static const PropertyID PropertyIDPerFrameDebugDataBegin = static_cast<UINT32>(PropertyGroup::DebugData) << 16;

static const PropertyID PropertyIDInvalid                = 0xFFFFFFFF;

/// @brief Structure describing top-level TintlessBG property
struct PropertyISPTintlessBG
{
    ISPTintlessBGStatsConfig    statsConfig;                                    ///< Tintless BG stats frame level configuration
    ISPTintlessBGStatsConfig    stripeConfig[2];                                ///< if dual IFE mode is enabled, the array is
                                                                                ///<  filled strip configuration - 0 for left &
                                                                                ///< 1 for right
    BOOL                        dualIFEMode;                                    ///< Flag to indicate if it's a dual IFE mode
};

/// @brief Structure describing top level Bayer Histogram property data.
struct PropertyISPBHistStats
{
    ISPBHistStatsConfig     statsConfig;                                ///< BHist stats frame level configuration
    ISPBHistStatsConfig     stripeConfig[2];                            ///< if dual IFE mode is enabled, the array is
                                                                        ///<  filled strip configuration - 0 for left and
                                                                        ///< 1 for right
    BOOL                    dualIFEMode;                                ///< Flag to indicate if it's a dual IFE mode
};

/// @brief Structure describing top level Image Histogram property data.
struct PropertyISPIHistStats
{
    ISPIHistStatsConfig     statsConfig;                                ///< BHist stats frame level configuration
    ISPIHistStatsConfig     stripeConfig[2];                            ///< if dual IFE mode is enabled, the array is
                                                                        ///< filled strip configuration - 0 for left and
                                                                        ///< 1 for right
    BOOL                    dualIFEMode;                                ///< Flag to indicate if it's a dual IFE mode
};

/// @brief Structure describing top level Bayer Focus property data.
struct PropertyISPBFStats
{
    ISPBFStatsConfig            statsConfig;                                ///< BF stats frame level configuration
    ISPBFStatsConfig            stripeConfig[2];                            ///< if dual IFE mode is enabled, the array is
                                                                            ///< filled strip configuration - 0 for left and
                                                                            ///< 1 for right
    BOOL                        dualIFEMode;                                ///< Flag to indicate if it's a dual IFE mode
};

/// @brief Structure describing top level AWB BG configuration property data.
struct PropertyISPAWBBGStats
{
    ISPAWBBGStatsConfig     statsConfig;                                ///< AWB BG stats frame level configuration
    ISPAWBBGStatsConfig     stripeConfig[2];                            ///< if dual IFE mode is enabled, the array is
                                                                        ///< filled strip configuration - 0 for left and
                                                                        ///< 1 for right
    BOOL                    dualIFEMode;                                ///< Flag to indicate if it's a dual IFE mode
};

/// @brief Structure describing top level HDR BE configuration property data.
struct PropertyISPHDRBEStats
{
    ISPHDRBEStatsConfig     statsConfig;                                ///< HDR BE stats frame level configuration
    ISPHDRBEStatsConfig     stripeConfig[2];                            ///< if dual IFE mode is enabled, the array is
                                                                        ///< filled strip configuration - 0 for left and
                                                                        ///< 1 for right
    BOOL                    dualIFEMode;                                ///< Flag to indicate if it's a dual IFE mode
};

/// @brief Structure describing top level HDR BHist configuration property data.
struct PropertyISPHDRBHistStats
{
    ISPHDRBHistStatsConfig  statsConfig;                                    ///< HDR BHist stats frame level configuration
    ISPHDRBHistStatsConfig  stripeConfig[2];                                ///< if dual IFE mode is enabled, the array is
                                                                            ///< filled strip configuration - 0 for left and
                                                                            ///< 1 for right
    BOOL                    dualIFEMode;                                    ///< Flag to indicate if it's a dual IFE mode
};

/// @brief Structure describing top level CS Stats configuration property data.
struct PropertyISPCSStats
{
    ISPCSStatsConfig        statsConfig;                                ///< CS stats frame level configuration
    ISPCSStatsConfig        stripeConfig[2];                            ///< if dual IFE mode is enabled, the array is
                                                                        ///< filled strip configuration - 0 for left and
                                                                        ///< 1 for right
    BOOL                    dualIFEMode;                                ///< Flag to indicate if it's a dual IFE mode
};

/// @brief Structure describing top level RS stats configuration property data.
struct PropertyISPRSStats
{
    ISPRSStatsConfig        statsConfig;                                ///< RS stats frame level configuration
    ISPRSStatsConfig        stripeConfig[2];                            ///< if dual IFE mode is enabled, the array is
                                                                        ///< filled strip configuration - 0 for left and
                                                                        ///< 1 for right
    BOOL                    dualIFEMode;                                ///< Flag to indicate if it's a dual IFE mode
};

/// @brief Structure for per pipeline output dimensions
struct PropertyPipelineOutputDimensions
{
    CHIPIPELINEOUTPUTDIMENSION dimensions[MaxPipelineOutputs];          ///< Pipeline output dimensions
    UINT                       numberOutputs;                           ///< number of outputs
};

/// @brief Union of Knee points for ADRC.
union KneePoints
{
    struct KneePoints10
    {
        FLOAT  kneeX[4];                ///< kneex TMC10
        FLOAT  kneeY[4];                ///< kneey TMC10
    } KneePointsTMC10;

    struct KneePoints11
    {
        FLOAT  kneeX[5];                ///< kneex TMC11
        FLOAT  kneeY[5];                ///< kneey TMC11
    } KneePointsTMC11;
};

/// @brief Structure for ISP ADRC Info
struct PropertyISPADRCInfo
{
    UINT32     enable;                                         ///< global enable
    UINT32     version;                                        ///< TMC version
    UINT32     gtmEnable;                                      ///< gtm enable
    UINT32     ltmEnable;                                      ///< ltm enable
    KneePoints kneePoints;                                     ///< knee points
    FLOAT      drcGainDark;                                    ///< drcGainDark
    FLOAT      gtmPercentage;                                  ///< GtmPercentage
    FLOAT      ltmPercentage;                                  ///< LtmPercentage

    // Valid for version 1.0
    FLOAT      coef[6];                                        ///< coef

    // Valid for version 1.1
    FLOAT      pchipCoef[9];                                   ///< PCHIP curve
    FLOAT      contrastEnhanceCurve[1024];                     ///< contrast enhance Curve
    UINT32     curveModel;                                     ///< curve model
    FLOAT      contrastHEBright;                               ///< contrast HE bright
    FLOAT      contrastHEDark;                                 ///< contrast HE Dark
};

/// @brief Structure for ISP ADRC parameters
struct PropertyISPADRCParams
{
    BOOL  bIsADRCEnabled;                                      ///< Whether ADRC is enabled or not.
    FLOAT GTMPercentage;                                       ///< GTM percentage
    FLOAT DRCGain;                                             ///< DRC gain
    FLOAT DRCDarkGain;                                         ///< DRC dark gain
};

/// @brief Structure for ISP intermediate size dimension
struct PropertyISPIntermediateDimension
{
    UINT32  intermediateWidth;   ///< intermediate width
    UINT32  intermediateHeight;  ///< intermediate height
    FLOAT   intermediateRatio;   ///< intermediate ratio
};

CAMX_NAMESPACE_END

#include "g_camxproperties.h"

#endif // CAMXPROPERTYBLOB_H
