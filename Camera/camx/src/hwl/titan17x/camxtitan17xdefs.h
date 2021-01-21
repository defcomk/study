////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 - 2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxtitan17xdefs.h
/// @brief Titan17x definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXTITAN17XDEFS_H
#define CAMXTITAN17XDEFS_H

#include "camxcslresourcedefs.h"
#include "camxhwdefs.h"
#include "camxincs.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 RegisterWidthInBytes    = sizeof(UINT32);    ///< Register width for Titan17
static const UINT32 IFEPipelineBitWidth     = 14;                ///< Titan 1.7 pipeline width
static const UINT32 BPSPipelineBitWidth     = 14;                ///< Titan 1.7 pipeline width
static const INT32  MaxTonemapCurvePoints   = 64;               ///< Max tone map curve points

CAMX_STATIC_ASSERT(4 == sizeof(UINT32));

const UINT IFEInstanceName0             = 0;
const UINT IFEOutputPortFull            = 0;
const UINT IFEOutputPortDS4             = 1;
const UINT IFEOutputPortDS16            = 2;
const UINT IFEOutputPortCAMIFRaw        = 3;
const UINT IFEOutputPortLSCRaw          = 4;
const UINT IFEOutputPortGTMRaw          = 5;
const UINT IFEOutputPortFD              = 6;
const UINT IFEOutputPortPDAF            = 7;
const UINT IFEOutputPortRDI0            = 8;
const UINT IFEOutputPortRDI1            = 9;
const UINT IFEOutputPortRDI2            = 10;
const UINT IFEOutputPortRDI3            = 11;
const UINT IFEOutputPortStatsRS         = 12;
const UINT IFEOutputPortStatsCS         = 13;
const UINT IFEOutputPortStatsIHIST      = 15;
const UINT IFEOutputPortStatsBHIST      = 16;
const UINT IFEOutputPortStatsHDRBE      = 17;
const UINT IFEOutputPortStatsHDRBHIST   = 18;
const UINT IFEOutputPortStatsTLBG       = 19;
const UINT IFEOutputPortStatsBF         = 20;
const UINT IFEOutputPortStatsAWBBG      = 21;
const UINT IFEOutputPortDisplayFull     = 22;
const UINT IFEOutputPortDisplayDS4      = 23;
const UINT IFEOutputPortDisplayDS16     = 24;
const UINT IFEOutputPortStatsDualPD     = 25;
const UINT MaxDefinedIFEOutputPorts     = IFEOutputPortStatsDualPD + 1;

const UINT IFEInputPortCSIDTPG          = 0;
const UINT IFEInputPortCAMIFTPG         = 1;
const UINT IFEInputPortSensor           = 2;
const UINT IFEInputPortRDI0             = 3;
const UINT IFEInputPortRDI1             = 4;
const UINT IFEInputPortRDI2             = 5;
const UINT IFEInputPortDualPD           = 6;

const UINT JPEGInstanceName0            = 0;
const UINT JPEGInstanceName1            = 1;
const UINT JPEGInputPort0               = 0;
const UINT JPEGOutputPort0              = 1;

const UINT IPEInstanceName0             = 0;
const UINT IPEInstanceName1             = 1;
const UINT IPEInstanceName2             = 2;
const UINT IPEInstanceName3             = 3;
const UINT IPEInstanceName4             = 4;
const UINT IPEInstanceName5             = 5;
const UINT IPEInstanceName6             = 6;
const UINT IPEInstanceName7             = 7;
const UINT IPEInstanceName8             = 8;
const UINT IPEInstanceName9             = 9;
const UINT IPEInstanceName10            = 10;
const UINT IPEInstanceName11            = 11;
const UINT IPEInstanceName12            = 12;
const UINT IPEInstanceName13            = 13;
const UINT IPEInstanceName14            = 14;
const UINT IPEInstanceName15            = 15;
const UINT IPEInstanceName16            = 16;
const UINT IPEInstanceName17            = 17;
const UINT IPEInstanceName18            = 18;
const UINT IPEInstanceName19            = 19;
const UINT IPEInstanceName20            = 20;
const UINT IPEInstanceName21            = 21;
const UINT IPENodePortInvalid           = static_cast<UINT>(CSLIPEInputPortIdFull - 1);
const UINT IPEInputPortFull             = CSLIPEInputPortIdFull;
const UINT IPEInputPortDS4              = CSLIPEInputPortIdDS4;
const UINT IPEInputPortDS16             = CSLIPEInputPortIdDS16;
const UINT IPEInputPortDS64             = CSLIPEInputPortIdDS64;
const UINT IPEInputPortFullRef          = CSLIPEInputPortIdFullRef;
const UINT IPEInputPortDS4Ref           = CSLIPEInputPortIdDS4Ref;
const UINT IPEInputPortDS16Ref          = CSLIPEInputPortIdDS16Ref;
const UINT IPEInputPortDS64Ref          = CSLIPEInputPortIdDS64Ref;
const UINT IPEOutputPortDisplay         = CSLIPEOutputPortIdDisplay;
const UINT IPEOutputPortVideo           = CSLIPEOutputPortIdVideo;
const UINT IPEOutputPortFullRef         = CSLIPEOutputPortIdFullRef;
const UINT IPEOutputPortDS4Ref          = CSLIPEOutputPortIdDS4Ref;
const UINT IPEOutputPortDS16Ref         = CSLIPEOutputPortIdDS16Ref;
const UINT IPEOutputPortDS64Ref         = CSLIPEOutputPortIdDS64Ref;
const UINT IPENodePortMaxNum            = CSLIPEOutputPortIdMaxNumPortResources;

const UINT BPSInstanceName0             = 0;
const UINT BPSInstanceName1             = 1;
const UINT BPSInputPort                 = CSLBPSInputPortImage;
const UINT BPSOutputPortFull            = CSLBPSOutputPortIdPDIImageFull;
const UINT BPSOutputPortDS4             = CSLBPSOutputPortIdPDIImageDS4;
const UINT BPSOutputPortDS16            = CSLBPSOutputPortIdPDIImageDS16;
const UINT BPSOutputPortDS64            = CSLBPSOutputPortIdPDIImageDS64;
const UINT BPSOutputPortStatsBG         = CSLBPSOutputPortIdStatsBG;
const UINT BPSOutputPortStatsHDRBHist   = CSLBPSOutputPortIdStatsHDRBHIST;
const UINT BPSOutputPortReg1            = CSLBPSOutputPortIdRegistrationImage1;
const UINT BPSOutputPortReg2            = CSLBPSOutputPortIdRegistrationImage2;

const UINT ChiNodeOutputFull            = 0;
const UINT ChiNodeOutputDS4             = 1;
const UINT ChiNodeOutputDS16            = 2;
const UINT ChiNodeOutputDS64            = 3;
const UINT ChiNodeOutputDS              = 4;

const UINT BPSMaxInput                  = 1;
const UINT BPSMaxOutput                 = 8;

const UINT FDHwInstanceName0            = 0;
const UINT FDHwInputPortImage           = 0;
const UINT FDHwOutputPortResults        = 0;
const UINT FDHwOutputPortRawResults     = 1;
const UINT FDHwOutputPortWorkBuffer     = 2;

const UINT FDHwMaxInput                 = 1;
const UINT FDHwMaxOutput                = 3;

const UINT LRMEInstanceName0            = 0;
const UINT LRMEOutputPortVector         = CSLLRMEOutputPortIdVector;
const UINT LRMEOutputPortDS2            = CSLLRMEOutputPortIdDS2;
const UINT LRMEMaxOutputPorts           = CSLLRMEOutputPortIdMax;
const UINT LRMEInputPortTARIFEFull      = CSLLRMEInputPortTARIFEFull;
const UINT LRMEInputPortREFIFEFull      = CSLLRMEInputPortREFIFEFull;
const UINT LRMEInputPortTARIFEDS4       = CSLLRMEInputPortTARIFEDS4;
const UINT LRMEInputPortREFIFEDS4       = CSLLRMEInputPortREFIFEDS4;
const UINT LRMEInputPortTARIFEDS16      = CSLLRMEInputPortTARIFEDS16;
const UINT LRMEInputPortREFIFEDS16      = CSLLRMEInputPortREFIFEDS16;
const UINT LRMEInputPortREFLRMEDS2      = CSLLRMEInputPortREFLRMEDS2;
const UINT LRMEMaxIntputPorts           = CSLLRMEInputPortIdMax;

const UINT RANSACInstanceName0          = 0;
const UINT RANSACOutputPort0            = 1;
const UINT RANSACInputPortVector        = 1;

const UINT RANSACMaxInput = 1;

/// @brief Common stats HW data
const UINT StatEntry16BitSizeToBytes        = 2;
const UINT StatEntry32BitSizeToBytes        = 4;
const UINT StatEntry64BitSizeToBytes        = 8;

/// @brief IFE UBWC compression ratios
static const FLOAT IFEUBWCTP10CompressionRatio    = 1.15f; ///< Compression ratio for UBWC TP10 writes by IFE
static const FLOAT IFEUBWCNV12CompressionRatio    = 1.31f; ///< Compression ratio for UBWC NV12 writes by IFE
static const FLOAT IFEUBWCTP10CompressionRatioUHD = 2.06f; ///< Compression ratio for UBWC TP10 writes by IFE
static const FLOAT IFEUBWCNV12CompressionRatioUHD = 1.95f; ///< Compression ratio for UBWC NV12 writes by IFE

/// @brief HDRBE HW data
const UINT32 HDRBEStatsROIMinWidth          = 384;
const UINT32 HDRBEStatsROIMinHeight         = 96;
const UINT HDRBEStatsMaxEntriesPerRegion    = 12;
const UINT HDRBEStatsMaxHorizontalRegions   = 64;
const UINT HDRBEStatsMaxVerticalRegions     = 48;
const UINT HDRBEStatsMaxWidth               =
    HDRBEStatsMaxEntriesPerRegion * StatEntry32BitSizeToBytes * HDRBEStatsMaxHorizontalRegions * HDRBEStatsMaxVerticalRegions;
const UINT HDRBEStatsMaxHeight              = 1;
const UINT HDRBEStatsOutputSizePerRegion    = HDRBEStatsMaxEntriesPerRegion * StatEntry32BitSizeToBytes;

/// @brief HDRBHist HW data
const UINT HDRBHistStatsChannels            = 3;
const UINT HDRBHistStatsBinsPerChannel      = 256;
const UINT HDRBHistStatsMaxWidth            = HDRBHistStatsBinsPerChannel * HDRBHistStatsChannels * StatEntry32BitSizeToBytes;
const UINT HDRBHistStatsMaxHeight           = 1;

/// @brief AWBGB HW data
const UINT AWBBGStatsMaxEntriesPerRegion    = 12;
const UINT AWBBGStatsMaxHorizontalRegions   = 160;
const UINT AWBBGStatsMaxVerticalRegions     = 90;
const UINT AWBBGStatsMaxWidth               =
    AWBBGStatsMaxEntriesPerRegion * StatEntry32BitSizeToBytes * AWBBGStatsMaxHorizontalRegions * AWBBGStatsMaxVerticalRegions;
const UINT AWBBGStatsMaxHeight              = 1;
const UINT AWBBGStatsOutputSizePerRegion    = AWBBGStatsMaxEntriesPerRegion * StatEntry32BitSizeToBytes;

/// @brief BPS AWBGB HW data
const UINT BPSAWBBGStatsMaxWidth            =
    AWBBGStatsMaxEntriesPerRegion * StatEntry32BitSizeToBytes * AWBBGStatsMaxHorizontalRegions;
const UINT BPSAWBBGStatsMaxHeight           = AWBBGStatsMaxVerticalRegions;


/// @brief BF HW data
const UINT BFStatsMaxEntriesPerRegion       = 4;
const UINT BFStatsNumOfFramgTagEntry        = 1;
const UINT BFStatsMaxRegions                = 180;
const UINT BFStatsMaxEntries                = (BFStatsMaxEntriesPerRegion * BFStatsMaxRegions) + BFStatsNumOfFramgTagEntry;
const UINT BFStatsMaxWidth                  = BFStatsMaxEntries * StatEntry64BitSizeToBytes;
const UINT BFStatsMaxHeight                 = 1;
const UINT BFStatsOutputSizePerRegion       = BFStatsMaxEntriesPerRegion * StatEntry64BitSizeToBytes;

/// @brief BHist HW data
const UINT BHistStatsBinsPerChannel         = 1024;
const UINT BHistStatsChannels               = 4;
const UINT BHistStatsWidth                  = BHistStatsBinsPerChannel * BHistStatsChannels * StatEntry32BitSizeToBytes;
const UINT BHistStatsHeight                 = 1;

/// @brief IHist HW data
const UINT IHistStatsBinsPerChannel         = 256;
const UINT IHistStatsChannels               = 4;
const UINT IHistStatsWidth                  = IHistStatsBinsPerChannel * IHistStatsChannels * StatEntry16BitSizeToBytes;
const UINT IHistStatsHeight                 = 1;

/// @brief CS HW data
const UINT CSStatsMaxHorizontalRegions      = 1560;
const UINT CSStatsMaxVerticalRegions        = 4;
const UINT CSStatsWidth                     =
    CSStatsMaxHorizontalRegions * CSStatsMaxVerticalRegions * StatEntry16BitSizeToBytes;
const UINT CSStatsHeight                    = 1;

/// @brief RS HW data
const UINT RSStatsMaxHorizontalRegions      = 1024;
const UINT RSStatsMaxVerticalRegions        = 16;
const UINT RSStatsWidth                     =
    RSStatsMaxHorizontalRegions * RSStatsMaxVerticalRegions * StatEntry16BitSizeToBytes;
const UINT RSStatsHeight                    = 1;

/// @brief Tintless BG HW data
const UINT TintlessBGStatsMaxHorizontalRegions = 64;
const UINT TintlessBGStatsMaxVerticalRegions   = 48;
const UINT TintlessBGStatsNumberOfEntries      = 12;
const UINT TintlessBGStatsWidth                = TintlessBGStatsMaxHorizontalRegions *
                                                 TintlessBGStatsMaxVerticalRegions   *
                                                 TintlessBGStatsNumberOfEntries      *
                                                 StatEntry32BitSizeToBytes;
const UINT TintlessBGStatsHeight               = 1;
const UINT TintlessStatsOutputSizePerRegion    = TintlessBGStatsNumberOfEntries * StatEntry32BitSizeToBytes;

/// @brief IFE I/O limitatinos
static const UINT32 IFEMaxOutputWidthFull        = 4928;
static const UINT32 IFEMaxOutputWidthFD          = 1920;
static const UINT32 IFEMaxOutputHeightFD         = 1080;
static const UINT32 IFEMaxOutputWidthFDTalos     = 2304;
static const UINT32 IFEMaxOutputHeightFDTalos    = 1296;
static const UINT32 IFEMAXOutputWidthRDIOnly     = 9856;
static const UINT32 IFEMaxOutputHeight           = 8192;
static const UINT32 IFEMAXOutputHeightRDIOnly    = 8192;
static const FLOAT  IFEMaxDownscaleLimt          = 32.0;

/// @brief IPE I/O limitations
/// @todo (CAMX-2013) update values based on finalized details on HW limitations.
static const FLOAT  IPEMaxUpscaleLinear          = 8.0;
static const FLOAT  IPEMaxUpscaleUBWC            = 2.0;
static const FLOAT  IPEMaxDownscaleLinear        = 24.0;
static const FLOAT  IPEMaxDownscaleUBWC          = 2.0;

static const UINT32 IPEMinInputWidth             = 640;
static const UINT32 IPEMinInputHeight            = 480;

/// @todo (CAMX-2013) update values based on finalized details on HW limitations.
static const UINT32 IPEMinInputWidthMultiPass    = 640;
static const UINT32 IPEMinInputHeightMultiPass   = 480;

static const UINT32 IPEMinInputWidthMultiPassOffline    = 1920;
static const UINT32 IPEMinInputHeightMultiPassOffline   = 1080;

static const UINT32 IPEMaxInputWidthICAEnabled   = 8192;
static const UINT32 IPEMaxInputHeightICAEnabled  = 8192;

static const UINT32 IPEMaxInputWidthICADisabled  = 16383;
static const UINT32 IPEMaxInputHeightICADisabled = 16383;

static const UINT32 TITAN170IPEMinOutputWidthUBWC  = 1280;
static const UINT32 TITAN170IPEMinOutputHeightUBWC = 720;

// height and width are 144 to support both portrait and landscape
static const UINT32 TITAN175IPEMinOutputWidthUBWC  = 144;
static const UINT32 TITAN175IPEMinOutputHeightUBWC = 144;

static const UINT32 TITAN150IPEMinOutputWidthUBWC  = 144;
static const UINT32 TITAN150IPEMinOutputHeightUBWC = 144;

static const UINT32 UHDResolutionWidth  = 3840;            ///< UHD resolution width
static const UINT32 UHDResolutionHeight = 2160;            ///< UHD resolution height

/// @brief IPE Clock calculation parameters
static const FLOAT     IPEClockOverhead        = 1.2F;         ///< IPE Processing overhead
static const FLOAT     IPEClockOverheadFor4K60 = 1.8F;         ///< IPE Processing overhead for 4K 60FPS
static const FLOAT     IPEClockEfficiency      = 1.46F;        ///< IPE processing efficiency
static const UINT      DefaultFPS              = 30;           ///< Default FPS
static const UINT64    NanoSecondMult          = 1000000000;   ///< Nano Seconds Multiplication factor

/// @brief IPE Bandwidth calculation parameters
static const FLOAT     IPEBpp8Bit                              = 1.5F;          ///< Bytes per Pixel for YUV420
static const FLOAT     IPEBpp10Bit                             = 1.5F * 1.33F;  ///< Bytes per Pixel for YUV420
static const FLOAT     IPEBandwidthOverhead                    = 1.12F * 1.06F; ///< Read overhead
static const FLOAT     IPESnapshotOverhead                     = 1.2F;          ///< Jpeg overhead
static const FLOAT     IPESnapshotRdBPP10bit                   = 1.5F * 1.33F;  ///< jpegRdBPP = 10 bpp = 1.5*1.33
static const FLOAT     IPESnapshotWrBPP8bit                    = 1.5F;          ///< jpegWrBPP = 8 bpp = 1.5
static const FLOAT     IPEEISOverhead                          = 1.0F;          ///< EIS Read overhead

static const FLOAT     IPEUBWCRdCompressionRatio               = 1.0F;          ///< UBWC Read Compression Ratio
static const FLOAT     IPEUBWCvr2RdCompressionRatio8Bit        = 1.33F;         ///< UBWC Read Compression Ratio
static const FLOAT     IPEUBWCvr2RdCompressionRatio10Bit       = 1.25F;         ///< UBWC Read Compression Ratio
static const FLOAT     IPEUBWCvr3RdCompressionRatio8Bit        = 1.36F;         ///< UBWC Read Compression Ratio
static const FLOAT     IPEUBWCvr3RdCompressionRatio10Bit       = 1.31F;         ///< UBWC Read Compression Ratio
static const FLOAT     IPEUBWCvr3RdCompressionRatio8BitUHD     = 1.95F;         ///< UBWC Read Compression Ratio
static const FLOAT     IPEUBWCvr3RdCompressionRatio10BitUHD    = 2.06F;         ///< UBWC Read Compression Ratio

static const FLOAT     IPEUBWCMctfReadCompressionRatio            = 1.0F;       ///< UBWC MCTF Read Compression Ration
static const FLOAT     IPEUBWCvr2MctfReadCompressionRatio8Bit     = 1.33F;      ///< UBWC MCTF Read Compression Ration
static const FLOAT     IPEUBWCvr2MctfReadCompressionRatio10Bit    = 1.25F;      ///< UBWC MCTF Read Compression Ration
static const FLOAT     IPEUBWCvr3MctfReadCompressionRatio8Bit     = 1.36F;      ///< UBWC MCTF Read Compression Ration
static const FLOAT     IPEUBWCvr3MctfReadCompressionRatio10Bit    = 1.31F;      ///< UBWC MCTF Read Compression Ration
static const FLOAT     IPEUBWCvr3MctfReadCompressionRatio8BitUHD  = 2.19F;      ///< UBWC MCTF Read Compression Ration
static const FLOAT     IPEUBWCvr3MctfReadCompressionRatio10BitUHD = 2.07F;      ///< UBWC MCTF Read Compression Ration

static const FLOAT     IPESwMargin                              = 1.1F;      ///< Software Margin
static const UINT      IPEPartialRdSourceHeight                 = 2160;      ///< Source height threshold for partial bandwidth
static const FLOAT     IPEPartialRdMultiplication               = 3.22F;     ///< Multiplication factor for partial bandwidth

static const FLOAT     IPEUBWCWrPreviewCompressionRatio            = 1.0F;   ///< Preview UBWC write compression ratio
static const FLOAT     IPEUBWCvr2WrPreviewCompressionRatio8Bit     = 1.21F;  ///< Preview UBWC write compression ratio for 8bit
static const FLOAT     IPEUBWCvr2WrPreviewCompressionRatio10Bit    = 1.15F;  ///< Preview UBWC write compression ratio for 10bt
static const FLOAT     IPEUBWCvr2WrPreviewCompressionRatio8BitUHD  = 1.21F;  ///< Preview UBWC write compression ratio for 8bit
static const FLOAT     IPEUBWCvr2WrPreviewCompressionRatio10BitUHD = 1.15F;  ///< Preview UBWC write compression ratio for 10bt

static const FLOAT     IPEUBWCWrVideoCompressionRatio             = 1.0F;    ///< Video UBWC write compression ratio
static const FLOAT     IPEUBWCvr2WrVideoCompressionRatio8Bit      = 1.21F;   ///< Video UBWC write compression ratio for 8 bit
static const FLOAT     IPEUBWCvr2WrVideoCompressionRatio10Bit     = 1.15F;   ///< Video UBWC write compression ratio for 10 bit
static const FLOAT     IPEUBWCvr2WrVideoCompressionRatio8BitUHD   = 1.23F;   ///< Video UBWC write compression ratio for 8 bit
static const FLOAT     IPEUBWCvr2WrVideoCompressionRatio10BitUHD  = 1.17F;   ///< Video UBWC write compression ratio for 10 bit

static const FLOAT     IPEUBWCWrMctfCompressionRatio            = 1.0F;      ///< Source UBWC write compression ratio
static const FLOAT     IPEUBWCvr2WrMctfCompressionRatio8Bit     = 1.21F;     ///< Source UBWC write compression ratio for 8 bit
static const FLOAT     IPEUBWCvr2WrMctfCompressionRatio10Bit    = 1.15F;     ///< Source UBWC write compression ratio for 10 bt
static const FLOAT     IPEUBWCvr3WrMctfCompressionRatio8Bit     = 1.36F;     ///< Source UBWC write compression ratio for 8 bit
static const FLOAT     IPEUBWCvr3WrMctfCompressionRatio10Bit    = 1.31F;     ///< Source UBWC write compression ratio for 10 bt
static const FLOAT     IPEUBWCvr3WrMctfCompressionRatio8BitUHD  = 2.19F;     ///< Source UBWC write compression ratio for 8 bit
static const FLOAT     IPEUBWCvr3WrMctfCompressionRatio10BitUHD = 2.07F;     ///< Source UBWC write compression ratio for 10 bt


// @brief BPS Clock calculation parameters
static const FLOAT     BPSClockOverhead   = 1.2F;              ///< BPS Processing overhead
static const FLOAT     BPSClockEfficiency = 0.76F;             ///< BPS processing efficiency

// @breif BPS Bandwidth calculation parameters
static const FLOAT     BPSBpp8Bit                     = 1.5F;          ///< Bytes per Pixel for YUV420
static const FLOAT     BPSBpp10Bit                    = 1.5F * 1.33F;  ///< Bytes per Pixel for YUV420
static const FLOAT     BPSBandwidthOverhead           = 1.2F;          ///< Read overhead
static const FLOAT     BPSSwMargin                    = 1.1F;          ///< Software Margin
static const FLOAT     BPSUBWCWrCompressionRatio      = 1.0F;          ///< UBWC Write
static const FLOAT     BPSUBWCWrCompressionRatio8Bit  = 1.31F;         ///< UBWC write 8bit
static const FLOAT     BPSUBWCWrCompressionRatio10Bit = 1.18F;         ///< UBWC write 10bit

/// @brief I/O limitations
static const UINT32 BPSMinInputWidth             = 640;
static const UINT32 BPSMinInputHeight            = 480;

static const UINT32 BPSMaxInputWidth             = 16383;
static const UINT32 BPSMaxInputHeight            = 16383;

/// @brief UBWC mode config
static const UINT32 NapaliUBWCHighestBankValue    = 15;
static const UINT32 NonNaplaiUBWCHighestBankValue = 14;

static const FLOAT  JPEGMaxDownScaling           = 16.0F;

static const UINT32 IsoMultiplyFactor            = 100;
static const UINT32 MaxSensitivityBoost          = 3199;
static const UINT32 MinGain                      = 1;

static const uint32_t  TITAN175UBWCBandwidthLimit  = 10;
static const uint32_t  TITAN175UBWCThresholdLossy0 = 12;
static const uint32_t  TITAN175UBWCThresholdLossy1 = 14;

static const uint32_t  TITAN175UBWC8BitBandwidthLimit  = 0x16;
static const uint32_t  TITAN175UBWC8BitThresholdLossy0 = 0x6210022;
static const uint32_t  TITAN175UBWC8BitThresholdLossy1 = 0xE0E;

static const uint32_t  TITAN175UBWC10BitBandwidthLimit  = 0x10;
static const uint32_t  TITAN175UBWC10BitThresholdLossy0 = 0x8330002;
static const uint32_t  TITAN175UBWC10BitThresholdLossy1 = 0x20204;

static const uint32_t  TITAN175UBWC10BitThresholdLossy0TF = 0x8330002;
static const uint32_t  TITAN175UBWC10BitThresholdLossy1TF = 0x8080A;

/// @brief Enumrating HW bug workarounds
enum Titan17xWorkarounds
{
    Titan17xWorkaroundsCDMDMI64EndiannessBug = 0,   ///< CDMDMI64EndiannessBug
    Titan17xWorkaroundsCDMDMICGCBug                 ///< CDMDMICGCBug
};

CAMX_NAMESPACE_END

#endif // CAMXTITAN17XDEFS_H
