////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxcslispdefs.h
/// @brief ISP Hardware Interface Definition
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXCSLISPDEFS_H
#define CAMXCSLISPDEFS_H
CAMX_NAMESPACE_BEGIN

// ISP Resource Type
static const UINT32 ISPResourceIdPort      = 0;
static const UINT32 ISPResourceIdClk       = 1;

// ISP Usage Type
static const UINT32 ISPResourceUsageSingle = 0;
static const UINT32 ISPResourceUsageDual   = 1;

// ISP Color Pattern
static const UINT32 ISPPatternBayerRGRGRG  = 0;
static const UINT32 ISPPatternBayerGRGRGR  = 1;
static const UINT32 ISPPatternBayerBGBGBG  = 2;
static const UINT32 ISPPatternBayerGBGBGB  = 3;
static const UINT32 ISPPatternYUVYCBYCR    = 4;
static const UINT32 ISPPatternYUVYCRYCB    = 5;
static const UINT32 ISPPatternYUVCBYCRY    = 6;
static const UINT32 ISPPatternYUVCRYCBY    = 7;

// ISP Input Format
static const UINT32 ISPFormatMIPIRaw6      = 1;
static const UINT32 ISPFormatMIPIRaw8      = 2;
static const UINT32 ISPFormatMIPIRaw10     = 3;
static const UINT32 ISPFormatMIPIRaw12     = 4;
static const UINT32 ISPFormatMIPIRaw14     = 5;
static const UINT32 ISPFormatMIPIRaw16     = 6;
static const UINT32 ISPFormatMIPIRaw20     = 7;
static const UINT32 ISPFormatRaw8Private   = 8;
static const UINT32 ISPFormatRaw10Private  = 9;
static const UINT32 ISPFormatRaw12Private  = 10;
static const UINT32 ISPFormatRaw14Private  = 11;
static const UINT32 ISPFormatPlain8        = 12;
static const UINT32 ISPFormatPlain168      = 13;
static const UINT32 ISPFormatPlain1610     = 14;
static const UINT32 ISPFormatPlain1612     = 15;
static const UINT32 ISPFormatPlain1614     = 16;
static const UINT32 ISPFormatPlain1616     = 17;
static const UINT32 ISPFormatPlain3220     = 18;
static const UINT32 ISPFormatPlain64       = 19;
static const UINT32 ISPFormatPlain128      = 20;
static const UINT32 ISPFormatARGB          = 21;
static const UINT32 ISPFormatARGB10        = 22;
static const UINT32 ISPFormatARGB12        = 23;
static const UINT32 ISPormatARGB14         = 24;
static const UINT32 ISPFormatDPCM10610     = 25;
static const UINT32 ISPFormatDPCM10810     = 26;
static const UINT32 ISPFormatDPCM12612     = 27;
static const UINT32 ISPFormatDPCM12812     = 28;
static const UINT32 ISPFormatDPCM14814     = 29;
static const UINT32 ISPFormatDPCM141014    = 30;
static const UINT32 ISPFormatNV21          = 31;
static const UINT32 ISPFormatNV12          = 32;
static const UINT32 ISPFormatTP10          = 33;
static const UINT32 ISPFormatYUV422        = 34;
static const UINT32 ISPFormatPD8           = 35;
static const UINT32 ISPFormatPD10          = 36;
static const UINT32 ISPFormatUBWCNV12      = 37;
static const UINT32 ISPFormatUBWCNV124R    = 38;
static const UINT32 ISPFormatUBWCTP10      = 39;
static const UINT32 ISPFormatUBWCP010      = 40;
static const UINT32 ISPFormatPlain8Swap    = 41;
static const UINT32 ISPFormatPlain810      = 42;
static const UINT32 ISPFormatPlain810Swap  = 43;
static const UINT32 ISPFormatYV12          = 44;
static const UINT32 ISPFormatY             = 45;
static const UINT32 ISPFormatUndefined     = 0xFFFFFFFF;

// ISP output resource type
static const UINT32 ISPOutputEncode        = 1000;
static const UINT32 ISPOutputView          = 1001;
static const UINT32 ISPOutputVideo         = 1002;
static const UINT32 ISPOutputRDI0          = 1003;
static const UINT32 ISPOutputRDI1          = 1004;
static const UINT32 ISPOutputRDI2          = 1005;
static const UINT32 ISPOutputRDI3          = 1006;
static const UINT32 ISPOutputStatsAEC      = 1007;
static const UINT32 ISPOutputStatsAF       = 1008;
static const UINT32 ISPOutputStatsAWB      = 1009;
static const UINT32 ISPOutputStatsRS       = 1010;
static const UINT32 ISPOutputStatsCS       = 1011;
static const UINT32 ISPOutputStatsIHIST    = 1012;
static const UINT32 ISPOutputStatsSkin     = 1013;
static const UINT32 ISPOutputStatsBG       = 1014;
static const UINT32 ISPOutputStatsBF       = 1015;
static const UINT32 ISPOutputStatsBE       = 1016;
static const UINT32 ISPOutputStatsBHIST    = 1017;
static const UINT32 ISPOutputStatsBFScale  = 1018;
static const UINT32 ISPOutputStatsHDRBE    = 1019;
static const UINT32 ISPOutputStatsHDRBHIST = 1020;
static const UINT32 ISPOutputStatsAECBG    = 1021;
static const UINT32 ISPOutputCAMIFRaw      = 1022;
static const UINT32 ISPOutputIdealRaw      = 1023;

// ISP input resource type
static const UINT32 ISPInputTestGen        = 1500;
static const UINT32 ISPInputPHY0           = 1501;
static const UINT32 ISPInputPHY1           = 1502;
static const UINT32 ISPInputPHY2           = 1503;
static const UINT32 ISPInputPHY3           = 1504;
static const UINT32 ISPInputFE             = 1505;

// ISP input resource Lane Type
static const UINT32 ISPLaneTypeDPHY        = 0;
static const UINT32 ISPLaneTypeCPHY        = 1;

/* ISP Resurce Composite Group ID */
static const UINT32  ISPOutputGroupIdNONE   = 0;
static const UINT32  ISPOutputGroupId0      = 1;
static const UINT32  ISPOutputGroupId1      = 2;
static const UINT32  ISPOutputGroupId2      = 3;
static const UINT32  ISPOutputGroupId3      = 4;
static const UINT32  ISPOutputGroupId4      = 5;
static const UINT32  ISPOutputGroupId5      = 6;
static const UINT32  ISPOutputGroupId6      = 7;
static const UINT32  ISPOutputGroupIdMAX    = 8;

static const UINT16  ISPAcquireCommonVersion0 = 0x1000;     ///< IFE Common Resource structure Version
static const UINT16  ISPAcquireInputVersion0  = 0x2000;     ///< IFE Input Resource structure Version
static const UINT16  ISPAcquireOutputVersion0 = 0x3000;     ///< IFE Output Resource structure Version

/// @brief ISP output Resource Information
struct ISPOutResourceInfo
{
    UINT32 resourceType;       ///< Resource type
    UINT32 format;             ///< Format of the output
    UINT32 width;              ///< Width of the output image
    UINT32 height;             ///< Height of the output image
    UINT32 compositeGroupId;   ///< Composite Group id of the group
    UINT32 splitPoint;         ///< Split point in pixels for Dual ISP case
    UINT32 secureMode;         ///< Output port to be secure or non-secure
    UINT32 reserved;           ///< Reserved field
};

/// @brief ISP input resource information
struct ISPInResourceInfo
{
    UINT32             resourceType;      ///< Resource type
    UINT32             laneType;          ///< Lane type (dphy/cphy)
    UINT32             laneNum;           ///< Active lane number
    UINT32             laneConfig;        ///< Lane configurations: 4 bits per lane
    UINT32             VC;                ///< Virtual Channel
    UINT32             DT;                ///< Data Type
    UINT32             format;            ///< Input Image Format
    UINT32             testPattern;       ///< Test Pattern for the TPG
    UINT32             usageType;         ///< Single ISP or Dual ISP
    UINT32             leftStart;         ///< Left input start offset in pixels
    UINT32             leftStop;          ///< Left input stop offset in pixels
    UINT32             leftWidth;         ///< Left input Width in pixels
    UINT32             rightStart;        ///< Right input start offset in pixels
    UINT32             rightStop;         ///< Right input stop offset in pixels
    UINT32             rightWidth;        ///< Right input Width in pixels
    UINT32             lineStart;         ///< Start offset of the pixel line
    UINT32             lineStop;          ///< Stop offset of the pixel line
    UINT32             height;            ///< Input height in lines
    UINT32             pixleClock;        ///< Sensor output clock
    UINT32             batchSize;         ///< Batch size for HFR mode
    UINT32             DSPMode;           ///< DSP Mode
    UINT32             HBICount;          ///< HBI Count
    UINT32             reserved;          ///< Reserved field
    UINT32             numberOutResource; ///< Number of the output resource associated
    ISPOutResourceInfo pDataField[1];     ///< Output Resource starting point
};
/// @brief ISP resource acquire Information
struct ISPAcquireHWInfo
{
    UINT16 commonInfoVersion;             ///< Common Info Version represents the IFE common resource structure version
                                          ///< currently we dont have any common reosurce strcuture, It is reserved for future
                                          ///< HW changes in KMD
    UINT16 commonInfoSize;                ///< Common Info Size represents IFE common resource strcuture Size
    UINT16 commonInfoOffset;              ///< Common Info Offset represnts the Offset from where the IFE common resource
                                          ///< structure is stored in data
    UINT32 numInputs;                     ///< Number of IFE inpiut resources
    UINT32 inputInfoVersion;              ///< Input Info Strcure Version
    UINT32 inputInfoSize;                 ///< Size of the Input resource
    UINT32 inputInfoOffset;               ///< Offset where the Input resource structure starts in data
    UINT64 data;                          ///< IFE resource data
};

CAMX_NAMESPACE_END
#endif // CAMXCSLISPDEFS_H
