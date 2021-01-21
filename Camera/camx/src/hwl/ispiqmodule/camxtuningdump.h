////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxispiqmodule.h
/// @brief TITAN IQ Module base class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXTUNINGDUMP_H
#define CAMXTUNINGDUMP_H

#include "camxstatsdebugdatawriter.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED
/// @brief Tuning face data
const UINT      TuningMaxFaceNumber     = 10;

/// @brief IFE HDR MAC & Recon register sizes
const UINT IFEHDRMACReconConfigRegisters    = 18;
const UINT IFEHDRMACReconBLKRegisters       = 1;
const UINT IFEHDRReconConfigRegisters       = 2;

/// @brief Gamma IFE & BPS v1.6 LUT details
const UINT GammaLUTEntries              = 64;

/// @brief Gamma IPE v1.5 LUT details
const UINT GammaIPELUTEntries           = 256;

/// @brief GTM v1.0 LUT details
const UINT GTMLUTEntries                = 64;

/// @brief LSC tuning data
const UINT      LSCConfigRegisters      = 7;
const UINT      LSCHorizontalMeshPoint  = 17;
const UINT      LSCVerticalMeshPoint    = 13;
const UINT32    LSCMeshSize             = LSCHorizontalMeshPoint * LSCVerticalMeshPoint;

/// @brief Pedestal v1.3 tuning data
const UINT      PedestalConfigRegisters = 5;
const UINT32    PedestalDMISetSizeDword = 130;

/// @brief BPS Linearization tuning data
const UINT      LinearizationConfigKneepoints   = 16;
const UINT32    BPSLinearizationLUTEntries      = 36;

/// @brief BPS: BPC & PDPC tuning data
const UINT      BPCPDPCConfigRegister0  = 10;
const UINT      BPCPDPCConfigRegister1  = 3;
const UINT32    BPSPDPCLUTEntries       = 64;

/// @brief BPS HDR tuning data
const UINT      BPSHDRReconRegisters    = 9;
const UINT      BPSHDRMACRegisters      = 7;

/// @brief GIC tuning data
const UINT      BPSGICRegisters         = 22;
const UINT32    BPSGICLUTEntries        = 64;

/// @brief ABF tuning data
const UINT      BPSABFRegisters             = 46;
const UINT32    BPSABFLUTEntriesNoise       = 64;
const UINT32    BPSABFLUTEntriesActivity    = 32;
const UINT32    BPSABFLUTEntriesDark        = 42;

/// @brief WB tuning data
const UINT      BPSWBConfigRegisters    = 4;

/// @brief Demosaic tuning data
const UINT      BPSDemosaicConfigRegisters  = 2;

/// @brief Demux tuning data
const UINT      BPSDemuxConfigRegisters = 7;

/// @brief Color Correction data
const UINT      BPSCCConfigRegisters    = 9;

/// @brief Color Space Transform data
const UINT      CSTConfigRegisters      = 12;

/// @brief HNR data
const UINT      HNRConfigRegisters          = 48;
const UINT32    BPSHNRLUTEntriesLNR         = 33;
const UINT32    BPSHNRLUTEntriesFNR         = 17;
const UINT32    BPSHNRLUTEntriesFNRAC       = 17;
const UINT32    BPSHNRLUTEntriesSNR         = 17;
const UINT32    BPSHNRLUTEntriesBlendLNR    = 17;
const UINT32    BPSHNRLUTEntriesBlendSNR    = 17;

/// @brief BPS all modules config data from registers that FW configure
const UINT      BPSModulesConfigFields      = 171;

/// @brief IPE all modules config data from registers that FW configure
const UINT      IPEModulesConfigFields      = 646;

/// @brief IPE ICA data
const UINT32    IPEICALUTEntriesPerspective = 72;
const UINT32    IPEICALUTEntriesGrid0       = 832;
const UINT32    IPEICALUTEntriesGrid1       = 832;

const UINT32    IPEICA20LUTEntriesGrid      = 1965;

/// @brief IPE ANR data
const UINT32 IPEANRDCBlend1ConfigRegisters          = 1;
const UINT32 IPEANRRNFConfigRegisters               = 32;
const UINT32 IPEANRDCUSConfigRegisters              = 1;
const UINT32 IPEANRCFilter2ConfigRegisters          = 12;
const UINT32 IPEANRDCBlend2ConfigRegisters          = 22;
const UINT32 IPEANRCYLPFPreLensGainConfigRegisters  = 14;
const UINT32 IPEANRCYLPFLensGainConfigRegisters     = 45;
const UINT32 IPEANRCYLPFPostLensGainConfigRegisters = 125;
const UINT32 IPEANRCNRConfigRegisters               = 75;

/// @brief IPE TF data
const UINT32 IPETFConfigRegisters       = 94;

/// @brief IPE CAC data
const UINT32 IPECACConfigRegisters      = 2;

/// @brief IPE LTM data
const UINT   IPELTMExposureIndexPrevious    = 0;
const UINT   IPELTMExposureIndexCurrent     = 1;
const UINT   IPELTMExposureIndexCount       = 2;
const UINT32 IPELTMLUTEntriesWeight         = 12;
const UINT32 IPELTMLUTEntriesLA0            = 64;
const UINT32 IPELTMLUTEntriesLA1            = 64;
const UINT32 IPELTMLUTEntriesCurve          = 64;
const UINT32 IPELTMLUTEntriesScale          = 64;
const UINT32 IPELTMLUTEntriesMask           = 64;
const UINT32 IPELTMLUTEntriesLCEPositive    = 16;
const UINT32 IPELTMLUTEntriesLCENegative    = 16;
const UINT32 IPELTMLUTEntriesRGamma0        = 64;
const UINT32 IPELTMLUTEntriesRGamma1        = 64;
const UINT32 IPELTMLUTEntriesRGamma2        = 64;
const UINT32 IPELTMLUTEntriesRGamma3        = 64;
const UINT32 IPELTMLUTEntriesRGamma4        = 64;
const UINT32 IPELTMLUTEntriesAverage        = 140;

/// @brief IPE Color correction data
const UINT32 IPECCConfigRegisters       = 9;

/// @brief IPE 2D LUT data
const UINT32 IPE2DLUTConfigRegisters            = 11;
const UINT32 IPE2DLUTEntriesHue0                = 112;
const UINT32 IPE2DLUTEntriesHue1                = 112;
const UINT32 IPE2DLUTEntriesHue2                = 112;
const UINT32 IPE2DLUTEntriesHue3                = 112;
const UINT32 IPE2DLUTEntriesSaturation0         = 112;
const UINT32 IPE2DLUTEntriesSaturation1         = 112;
const UINT32 IPE2DLUTEntriesSaturation2         = 112;
const UINT32 IPE2DLUTEntriesSaturation3         = 112;
const UINT32 IPE2DLUTEntriesInverseHue          = 24;
const UINT32 IPE2DLUTEntriesInverseSaturation   = 15;
const UINT32 IPE2DLUTEntries1DHue               = 25;
const UINT32 IPE2DLUTEntries1DSaturation        = 16;

/// @brief IPE Chroma Enhancement data
const UINT32 IPEChromaEnhancementConfigRegisters    = 8;

/// @brief IPE Chroma Suppression data
const UINT32 IPEChromaSuppressionConfigRegisters    = 42;

/// @brief IPE Skin Color Enhancement data
const UINT32 IPESCEConfigRegisters      = 39;

/// @brief IPE Adaptive Spatial Filter (ASF) data
const UINT32 IPEASFConfigRegisters                                      = 68;
const UINT32 IPE2DLUTEntriesLayer1ActivityNormalGainPosGainNegSoftThLUT = 256;
const UINT32 IPE2DLUTEntriesLayer1GainWeightLUT                         = 256;
const UINT32 IPE2DLUTEntriesLayer1SoftThresholdWeightLUT                = 256;
const UINT32 IPE2DLUTEntriesLayer2ActivityNormalGainPosGainNegSoftThLUT = 256;
const UINT32 IPE2DLUTEntriesLayer2GainWeightLUT                         = 256;
const UINT32 IPE2DLUTEntriesLayer2SoftThresholdWeightLUT                = 256;
const UINT32 IPE2DLUTEntriesChromaGradientPosNegLUT                     = 256;
const UINT32 IPE2DLUTEntriesContrastPosNegLUT                           = 256;
const UINT32 IPE2DLUTEntriesSkinActivityGainLUT                         = 17;

/// @brief IPE Upscaler data
const UINT32 IPEUpscalerEntriesALUT     = 168;
const UINT32 IPEUpscalerEntriesBLUT     = 96;
const UINT32 IPEUpscalerEntriesCLUT     = 96;
const UINT32 IPEUpscalerEntriesDLUT     = 80;

/// @brief IPE Grain Adder (GRA) data
const UINT32 IPEGrainAdderEntriesLUT    = 32;

/// @brief IQ OEM Costom tuning data
const UINT32 IQOEMTuningData            = 256;

/// @brief: Gamma LUT channels
enum GammaLUTChannel
{
    GammaLUTChannelG = 0,   ///< G channel
    GammaLUTChannelB = 1,   ///< B channel
    GammaLUTChannelR = 2,   ///< R channel
    GammaMaxChannel  = 3,   ///< Max LUTs
};

/// @brief Structure to encapsulate IFE & BPS Gamma curve data
struct GammaCurve
{
    UINT32 curve[GammaLUTEntries];
} CAMX_PACKED;

/// @brief Structure to encapsulate GTM curve data
struct GTMLUT
{
    UINT64  LUT[GTMLUTEntries];  ///< GTM LUT
} CAMX_PACKED;

/// @brief Structure to encapsulate LSC LUT data
struct LSCMeshLUT
{
    /// register info also from register dump
    UINT32 GRRLUT[LSCMeshSize];
    UINT32 GBBLUT[LSCMeshSize];
} CAMX_PACKED;

/// @brief Structure to encapsulate Pedestal LUT data
struct PedestalLUT
{
    /// register info also from register dump
    UINT32 GRRLUT[PedestalDMISetSizeDword];
    UINT32 GBBLUT[PedestalDMISetSizeDword];
} CAMX_PACKED;

/// @brief Structure to encapsulate Linearization LUT data
struct BPSLinearizationLUT
{
    UINT32 linearizationLUT[BPSLinearizationLUTEntries];
} CAMX_PACKED;

/// @brief Structure to encapsulate PDPC LUT data
struct BPSPDPCLUT
{
    UINT32 BPSPDPCMaskLUT[BPSPDPCLUTEntries];
} CAMX_PACKED;

/// @brief Structure to encapsulate GIC LUT data
struct BPSGICLUT
{
    UINT32 BPSGICNoiseLUT[BPSGICLUTEntries];
} CAMX_PACKED;

/// @brief Structure to encapsulate ABF LUTs data
struct BPSABFLUT
{
    UINT32 noiseLUT[BPSABFLUTEntriesNoise];
    UINT32 noise1LUT[BPSABFLUTEntriesNoise];
    UINT32 activityLUT[BPSABFLUTEntriesActivity];
    UINT32 darkLUT[BPSABFLUTEntriesDark];
} CAMX_PACKED;

/// @brief Structure to encapsulate HNR LUTs data
struct BPSHNRLUT
{
    UINT32 LNRLUT[BPSHNRLUTEntriesLNR];
    UINT32 FNRAndClampLUT[BPSHNRLUTEntriesFNR];
    UINT32 FNRAcLUT[BPSHNRLUTEntriesFNRAC];
    UINT32 SNRLUT[BPSHNRLUTEntriesSNR];
    UINT32 blendLNRLUT[BPSHNRLUTEntriesBlendLNR];
    UINT32 blendSNRLUT[BPSHNRLUTEntriesBlendSNR];
} CAMX_PACKED;

/// @brief Structure to encapsulate IPE Gamma curve data
struct IPEGammaCurve
{
    UINT32 curve[GammaIPELUTEntries];
} CAMX_PACKED;

/// @brief enumeration defining IPE path
enum IPETuningICAPath
{
    TuningICAInput      = 0,    ///< Input
    TuningICAReference,         ///< Reference
    TuningICAMaxPaths,          ///< Total number of ICE paths
};

/// @brief Structure to encapsulate ICA LUTs data
struct IPEICALUT
{
    UINT32 PerspectiveLUT[IPEICALUTEntriesPerspective];
    UINT32 grid0LUT[IPEICALUTEntriesGrid0];
    UINT32 grid1LUT[IPEICALUTEntriesGrid1];
} CAMX_PACKED;

/// @brief Structure to encapsulate LTM LUTs data
struct IPELTMLUT
{
    UINT32 WeightLUT[IPELTMLUTEntriesWeight];
    UINT32 LA0LUT[IPELTMLUTEntriesLA0];
    UINT32 LA1LUT[IPELTMLUTEntriesLA1];
    UINT32 CurveLUT[IPELTMLUTEntriesCurve];
    UINT32 ScaleLUT[IPELTMLUTEntriesScale];
    UINT32 MaskLUT[IPELTMLUTEntriesMask];
    UINT32 LCEPositiveLUT[IPELTMLUTEntriesLCEPositive];
    UINT32 LCENegativeLUT[IPELTMLUTEntriesLCENegative];
    UINT32 RGamma0LUT[IPELTMLUTEntriesRGamma0];
    UINT32 RGamma1LUT[IPELTMLUTEntriesRGamma1];
    UINT32 RGamma2LUT[IPELTMLUTEntriesRGamma2];
    UINT32 RGamma3LUT[IPELTMLUTEntriesRGamma3];
    UINT32 RGamma4LUT[IPELTMLUTEntriesRGamma4];
    UINT32 AverageLUT[IPELTMLUTEntriesAverage];
} CAMX_PACKED;

/// @brief Structure to encapsulate 2DLUT LUTs
struct IPE2DLUTLUT
{
    UINT32 Hue0LUT[IPE2DLUTEntriesHue0];
    UINT32 Hue1LUT[IPE2DLUTEntriesHue1];
    UINT32 Hue2LUT[IPE2DLUTEntriesHue2];
    UINT32 Hue3LUT[IPE2DLUTEntriesHue3];
    UINT32 Saturation0LUT[IPE2DLUTEntriesSaturation0];
    UINT32 Saturation1LUT[IPE2DLUTEntriesSaturation1];
    UINT32 Saturation2LUT[IPE2DLUTEntriesSaturation2];
    UINT32 Saturation3LUT[IPE2DLUTEntriesSaturation3];
    UINT32 InverseHueLUT[IPE2DLUTEntriesInverseHue];
    UINT32 InverseSaturationLUT[IPE2DLUTEntriesInverseSaturation];
    UINT32 Hue1DLUT[IPE2DLUTEntries1DHue];
    UINT32 Saturation1DLUT[IPE2DLUTEntries1DSaturation];
} CAMX_PACKED;

/// @brief Structure to encapsulate ASF LUTs
struct IPEASFLUT
{
    UINT32 Layer1ActivityNormalGainPosGainNegSoftThLUT[IPE2DLUTEntriesLayer1ActivityNormalGainPosGainNegSoftThLUT];
    UINT32 Layer1GainWeightLUT[IPE2DLUTEntriesLayer1GainWeightLUT];
    UINT32 Layer1SoftThresholdWeightLUT[IPE2DLUTEntriesLayer1SoftThresholdWeightLUT];
    UINT32 Layer2ActivityNormalGainPosGainNegSoftThLUT[IPE2DLUTEntriesLayer2ActivityNormalGainPosGainNegSoftThLUT];
    UINT32 Layer2GainWeightLUT[IPE2DLUTEntriesLayer2GainWeightLUT];
    UINT32 Layer2SoftThresholdWeightLUT[IPE2DLUTEntriesLayer2SoftThresholdWeightLUT];
    UINT32 ChromaGradientPosNegLUT[IPE2DLUTEntriesChromaGradientPosNegLUT];
    UINT32 ContrastPosNegLUT[IPE2DLUTEntriesContrastPosNegLUT];
    UINT32 SkinActivityGainLUT[IPE2DLUTEntriesSkinActivityGainLUT];
} CAMX_PACKED;

/// @brief Structure to encapsulate Upscaler LUTs
struct IPEUpscalerLUT
{
    UINT32 ALUT[IPEUpscalerEntriesALUT];
    UINT32 BLUT[IPEUpscalerEntriesBLUT];
    UINT32 CLUT[IPEUpscalerEntriesCLUT];
    UINT32 DLUT[IPEUpscalerEntriesDLUT];
} CAMX_PACKED;

/// @brief: CAMX node information, useful for multi-pass usecase
struct IQNodeInformation
{
    UINT32  instaceId;
    UINT64  requestId;
    UINT32  isRealTime;
    UINT32  profileId;
    UINT32  processingType;
} CAMX_PACKED;

/// @brief Structure to encapsulate Grain Adder (GRA)
struct IPEGrainAdderLUT
{
    UINT32 Channel1LUT[IPEGrainAdderEntriesLUT];
    UINT32 Channel2LUT[IPEGrainAdderEntriesLUT];
    UINT32 Channel3LUT[IPEGrainAdderEntriesLUT];
} CAMX_PACKED;

/// @brief: CAMX Tuning mode data
struct TuningModeDebugData
{
    UINT32  base;       ///< Default tuning data
    UINT32  sensor;     ///< Sensor mode
    UINT32  usecase;    ///< Usecase mode
    UINT32  feature1;   ///< Feature 1 mode
    UINT32  feature2;   ///< Feature 2 mode
    UINT32  scene;      ///< Scene mode
    UINT32  effect;     ///< Effect mode
} CAMX_PACKED;

/// @brief Structure to encapsulate IFE DMI data
struct IFEDMILUTData
{
    GammaCurve      gamma[GammaMaxChannel]; ///< Gamma LUT curve
    GTMLUT          GTM;                    ///< GTM LUT
    LSCMeshLUT      LSCMesh;                ///< LSC mesh LUT
} CAMX_PACKED;

/// @brief Structure to encapsulate BPS DMI data
struct BPSDMILUTData
{
    GTMLUT              GTM;                    ///< GTM LUT
    LSCMeshLUT          LSCMesh;                ///< LSC mesh LUT
    PedestalLUT         pedestalLUT;            ///< Pedestal LUT
    BPSLinearizationLUT linearizationLUT;       ///< Linarization BPS LUT
    BPSPDPCLUT          PDPCLUT;                ///< BPC & PDPC LUT
    BPSGICLUT           GICLUT;                 ///< GIC noise LUT
    BPSABFLUT           ABFLUT;                 ///< ABF noise, activity & dark LUTs
    GammaCurve          gamma[GammaMaxChannel]; ///< Gamma LUT curve
    BPSHNRLUT           HNRLUT;                 ///< HNR LUTs
} CAMX_PACKED;

/// @brief Structure to encapsulate IPE DMI data
struct IPEDMILUTData
{
    IPEGammaCurve       gamma[GammaMaxChannel];     ///< Gamma LUT curve
    IPEICALUT           ICALUT[TuningICAMaxPaths];  ///< ICA LUTs for input & reference
    IPELTMLUT           LTMLUT;                     ///< LTM LUTs
    IPE2DLUTLUT         LUT2D;                      ///< 2D LUT LUTs
    IPEASFLUT           ASFLUT;                     ///< Adaptive Spatial Filter LUTs
    IPEUpscalerLUT      upscalerLUT;                ///< Upscaler LUTs
    IPEGrainAdderLUT    grainAdderLUT;              ///< Grain Adder LUTs
} CAMX_PACKED;

/// @brief Structure to encapsulate IFE DMI metadata
struct IFEDMITuningMetadata
{
    IFEDMILUTData   packedLUT;
} CAMX_PACKED;

/// @brief Structure to encapsulate BPS DMI metadata
struct BPSDMITuningMetadata
{
    BPSDMILUTData   packedLUT;
} CAMX_PACKED;

/// @brief Pedestal Tuning Metadata
struct BPSPedestalTuningMetadata
{
    UINT32  pedestalConfig[PedestalConfigRegisters];
} CAMX_PACKED;

/// @brief Linearization kneepoint configuration
struct BPSLinearizationTuningMetadata
{
    UINT32 linearizationConfig[LinearizationConfigKneepoints];
} CAMX_PACKED;

/// @brief BPC & PDPC register configuration
struct BPSBPCPDPCTuningMetadata
{
    UINT32  BPCPDPConfig0[BPCPDPCConfigRegister0];
    UINT32  BPCPDPConfig1[BPCPDPCConfigRegister1];
} CAMX_PACKED;

/// @brief BPC & PDPC register configuration
struct BPSHDRTuningMetadata
{
    UINT32  HDRReconConfig[BPSHDRReconRegisters];
    UINT32  HDRMACConfig[BPSHDRMACRegisters];
} CAMX_PACKED;

/// @brief GIC register configuration
struct BPSGICTuningMetadata
{
    UINT32  filterConfig[BPSGICRegisters];
} CAMX_PACKED;

/// @brief ABF register configuration
struct BPSABFTuningMetadata
{
    UINT32  config[BPSABFRegisters];
} CAMX_PACKED;

/// @brief LSC registers configuration
struct BPSLSCTuningMetadata
{
    UINT32  rolloffConfig[LSCConfigRegisters];
} CAMX_PACKED;

/// @brief WB registers configuration
struct BPSWBTuningMetadata
{
    UINT32  WBConfig[BPSWBConfigRegisters];
} CAMX_PACKED;

/// @brief Demosaic registers configuration
struct BPSDemosaicTuningMetadata
{
    UINT32  demosaicConfig[BPSDemosaicConfigRegisters];
} CAMX_PACKED;

/// @brief Demux registers configuration
struct BPSDemuxTuningMetadata
{
    UINT32  demuxConfig[BPSDemuxConfigRegisters];
} CAMX_PACKED;

/// @brief Color correction registers configuration
struct BPSCCTuningMetadata
{
    UINT32  colorCorrectionConfig[BPSCCConfigRegisters];
} CAMX_PACKED;

/// @brief Color space transform registers configuration
struct CSTTuningMetadata
{
    UINT32  CSTConfig[CSTConfigRegisters];
} CAMX_PACKED;

/// @brief Tuning Face Data configuration
struct TuningFaceData
{
    INT32 numberOfFace;                     ///< Number of faces
    INT32 faceCenterX[TuningMaxFaceNumber]; ///< Face cordinates X
    INT32 faceCenterY[TuningMaxFaceNumber]; ///< Face coordinates Y
    INT32 faceRadius[TuningMaxFaceNumber];  ///< Face Radius
} CAMX_PACKED;

/// @brief Hybrid Noise Reduction (HNR) registers configuration
struct BPSHNRTuningMetadata
{
    UINT32  HNRConfig[HNRConfigRegisters];
} CAMX_PACKED;

/// @brief BPS all modules config data from registers that FW configure
struct BPSModulesConfigTuningMetadata
{
    UINT32 modulesConfigData[BPSModulesConfigFields];
} CAMX_PACKED;

/// @brief IPE pass names
enum TuningIPEPassName
{
    TuningIPEPassFull   = 0,                ///< FULL
    TuningIPEPassDC4    = 1,                ///< ANR/TF 1:4
    TuningIPEPassDC16   = 2,                ///< ANR/TF 1:16
    TuningIPEPassDC64   = 3,                ///< ANR/TF 1:64

    TuningIPEPassMax,

    TuningIPEPassDCBase = TuningIPEPassDC4,
};

/// @brief Structure to encapsulate IPE DMI metadata
struct IPEDMITuningMetadata
{
    IPEDMILUTData   packedLUT;
} CAMX_PACKED;

/// @brief IPE all modules config data from registers that FW configure
struct IPEModulesConfigTuningMetadata
{
    UINT32 modulesConfigData[IPEModulesConfigFields];
} CAMX_PACKED;

/// @brief IPE ANR register DCBlend1 reg cmds
struct IPEANRTuningDCBlend1RegCmd
{
    UINT32 DCBlend1[IPEANRDCBlend1ConfigRegisters];
} CAMX_PACKED;

/// @brief IPE ANR register RNF reg cmds
struct IPEANRTuningRNFRegCmd
{
    UINT32 RNF[IPEANRRNFConfigRegisters];
} CAMX_PACKED;

/// @brief IPE ANR register DCUS reg cmds
struct IPEANRTuningDCUSRegCmd
{
    UINT32 DCUS[IPEANRDCUSConfigRegisters];
} CAMX_PACKED;
/// @brief IPE ANR register Filter2 cmds
struct IPEANRTuningCFilter2RegCmd
{
    UINT32 CFilter2[IPEANRCFilter2ConfigRegisters];
} CAMX_PACKED;
/// @brief IPE ANR register DCBlend2 reg cmds
struct IPEANRTuningDCBlend2RegCmd
{
    UINT32 DCBlend2[IPEANRDCBlend2ConfigRegisters];
} CAMX_PACKED;

/// @brief IPE ANR register CYLPF pre Lens Gain reg cmds
struct IPEANRTuningCYLPFPreLensGainRegCmd
{
    UINT32 CYLPFPreLensGain[IPEANRCYLPFPreLensGainConfigRegisters];
} CAMX_PACKED;

/// @brief IPE ANR register CYLPF Lens Gain reg cmds
struct IPEANRTuningCYLPFLensGainRegCmd
{
    UINT32 CYLPFLensGain[IPEANRCYLPFLensGainConfigRegisters];
} CAMX_PACKED;

/// @brief IPE ANR register CYLPF post Lens Gain cmds
struct IPEANRTuningCYLPFPostLensGainRegCmd
{
    UINT32 CYLPFPostLensGain[IPEANRCYLPFPostLensGainConfigRegisters];
} CAMX_PACKED;
/// @brief IPE ANR register CNR reg cmds
struct IPEANRTuningCNRRegCmd
{
    UINT32 CNR[IPEANRCNRConfigRegisters];
} CAMX_PACKED;

/// @brief IPE ANR registers configuration per pass
struct IPEANRTuningPassData
{
    IPEANRTuningDCBlend1RegCmd          dcBlend1Cmd;            ///< ANR DCBlend1 reg cmds
    IPEANRTuningRNFRegCmd               rnfCmd;                 ///< ANR RNF reg cmds
    IPEANRTuningDCUSRegCmd              dcusCmd;                ///< ANR DCUS reg cmds
    IPEANRTuningCFilter2RegCmd          filter2Cmd;             ///< ANR Filter2 cmds
    IPEANRTuningDCBlend2RegCmd          dcBlend2Cmd;            ///< ANR DCBlend2 reg cmds
    IPEANRTuningCYLPFPreLensGainRegCmd  cylpfPreLensGainCmd;    ///< ANR CYLPF pre Lens Gain reg cmds
    IPEANRTuningCYLPFLensGainRegCmd     cylpfLensGainCmd;       ///< ANR CYLPF Lens Gain reg cmds
    IPEANRTuningCYLPFPostLensGainRegCmd cylpfPostLensGainCmd;   ///< ANR CYLPF post Lens Gain cmds
    IPEANRTuningCNRRegCmd               cnrCmd;                 ///< ANR CNR reg cmds
} CAMX_PACKED;

/// @brief IPE ANR all registers configuration
struct IPEANRTuningMetadata
{
    IPEANRTuningPassData    ANRData[TuningIPEPassMax];
} CAMX_PACKED;

/// @brief IPE ANR registers configuration per pass
struct IPETFTuningPassData
{
    UINT32  TFRegister[IPETFConfigRegisters];
} CAMX_PACKED;

/// @brief IPE TF all registers configuration
struct IPETFTuningMetadata
{
    IPETFTuningPassData    TFData[TuningIPEPassMax];
} CAMX_PACKED;

/// @brief IPE TF all registers configuration
struct IPECACTuningMetadata
{
    UINT32  CACConfig[IPECACConfigRegisters];
} CAMX_PACKED;

/// @brief IPE LTM exposure index data, previous and current
struct IPELTMTuningExpData
{
    FLOAT  exposureIndex[IPELTMExposureIndexCount];
} CAMX_PACKED;

/// @brief IPE Color correction registers configuration
struct IPECCTuningMetadata
{
    UINT32  colorCorrectionConfig[IPECCConfigRegisters];
} CAMX_PACKED;

/// @brief IPE 2D LUT registers configuration
struct IPE2DLUTTuningMetadata
{
    UINT32  LUT2DConfig[IPE2DLUTConfigRegisters];
} CAMX_PACKED;

/// @brief IPE Advance Chroma Enhancement registers configuration
struct IPEChromaEnhancementMetadata
{
    UINT32  CEConfig[IPEChromaEnhancementConfigRegisters];
} CAMX_PACKED;

/// @brief IPE Chroma Suppression registers configuration
struct IPEChromaSuppressionMetadata
{
    UINT32  CSConfig[IPEChromaSuppressionConfigRegisters];
} CAMX_PACKED;

/// @brief IPE Skin Color Enhancement registers configuration
struct IPESCEMetadata
{
    UINT32  SCEConfig[IPESCEConfigRegisters];
} CAMX_PACKED;

/// @brief IPE Adaptive Spatial Filter (ASF) registers configuration
struct IPEASFMetadata
{
    UINT32  ASFConfig[IPEASFConfigRegisters];
} CAMX_PACKED;

/// @brief IFE Structure to hold Crop, Round and Clamp module enable registers
struct IFETuningModuleEnableConfig
{
    UINT32  lensProcessingModuleConfig;             ///< Lens processing Module enable
    UINT32  colorProcessingModuleConfig;            ///< Color processing Module enable
    UINT32  frameProcessingModuleConfig;            ///< Frame processing Module enable
    UINT32  FDLumaCropRoundClampConfig;             ///< FD Luma path Module enable
    UINT32  FDChromaCropRoundClampConfig;           ///< FD Chroma path Module enable
    UINT32  fullLumaCropRoundClampConfig;           ///< Full Luma path Module enable
    UINT32  fullChromaCropRoundClampConfig;         ///< Full Chroma path Module enable
    UINT32  DS4LumaCropRoundClampConfig;            ///< DS4 Luma path Module enable
    UINT32  DS4ChromaCropRoundClampConfig;          ///< DS4 Chroma path Module enable
    UINT32  DS16LumaCropRoundClampConfig;           ///< DS16 Luma path Module enable
    UINT32  DS16ChromaCropRoundClampConfig;         ///< DS16 Chroma path Module enable
    UINT32  statsEnable;                            ///< Stats Module Enable
    UINT32  statsConfig;                            ///< Stats config
    UINT32  dspConfig;                              ///< Dsp Config
    UINT32  frameProcessingDisplayModuleConfig;     ///< Frame processing Disp module enable
    UINT32  displayFullLumaCropRoundClampConfig;    ///< Full Disp Luma path Module enable
    UINT32  displayFullChromaCropRoundClampConfig;  ///< Full Disp Chroma path Module enable
    UINT32  displayDS4LumaCropRoundClampConfig;     ///< DS4 Disp Luma path Module enable
    UINT32  displayDS4ChromaCropRoundClampConfig;   ///< DS4 Disp Chroma path Module enable
    UINT32  displayDS16LumaCropRoundClampConfig;    ///< DS16 Disp Luma path Module enable
    UINT32  displayDS16ChromaCropRoundClampConfig;  ///< DS16 Disp Chroma path Module enable
} CAMX_PACKED;

/// @brief Defines the exposure parameters to control the frame exposure setting to sensor and ISP
struct TuningAECExposureData
{
    UINT64  exposureTime;         ///< The exposure time in nanoseconds used by sensor
    FLOAT   linearGain;           ///< The total gain consumed by sensor only
    FLOAT   sensitivity;          ///< The total exposure including exposure time and gain
    FLOAT   deltaEVFromTarget;    ///< The current exposure delta compared with the desired target
} CAMX_PACKED;

/// @brief Structure to encapsulate AEC Frame Control
struct TuningAECFrameControl
{
    TuningAECExposureData   exposureInfo[ExposureIndexCount];    ///< Exposure info including gain and exposure time
    FLOAT                   luxIndex;                            ///< Future frame lux index,  consumed by ISP
    UINT8                   flashInfo;                           ///< Flash information if it is main or preflash
    UINT8                   preFlashState;                       ///< Preflash state from AEC, consumed by Sensor
    UINT32                  LEDCurrents[LEDSettingCount];        ///< The LED currents value for the use case of LED snapshot
} CAMX_PACKED;

/// @brief Structure to encapsulate AWB Frame Control
struct TuningAWBFrameControl
{
    FLOAT   AWBGains[RGBChannelCount];          ///< R/G/B gains
    UINT32  colorTemperature;                   ///< Color temperature
    UINT8   isCCMOverrideEnabled;               ///< Flag indicates if CCM override is enabled.
    FLOAT   CCMOffset[AWBNumCCMRows];           ///< The offsets for color correction matrix
    FLOAT   CCM[AWBNumCCMRows][AWBNumCCMCols]; ///< The color correction matrix
} CAMX_PACKED;

/// @brief Structure to encapsulate Sensor data
struct SensorTuningData
{
    PixelFormat         format;            ///< Pixel format
    UINT                numPixelsPerLine;  ///< Number of pixels per line (lineLengthPclk)
    UINT                numLinesPerFrame;  ///< Number of lines per frame (frameLengthLines)
    UINT                maxLineCount;      ///< Max line count
    SensorResolution    resolution;        ///< Sensor output resolution for this mode
    SensorCropInfo      cropInfo;          ///< Crop info for this modes
    UINT                binningTypeH;      ///< Binning horizontal
    UINT                binningTypeV;      ///< Binning vertical
} CAMX_PACKED;

/// @brief Structure encapsulating Trigger data
struct IFETuningTriggerData
{
    FLOAT       AECexposureGainRatio;       ///< Input trigger value:  AEC exposure/gain ratio
    FLOAT       AECexposureTime;            ///< Input trigger value:  AEC exposure time
    FLOAT       AECSensitivity;             ///< Input trigger value:  AEC sensitivity data
    FLOAT       AECGain;                    ///< Input trigger value:  AEC Gain Value
    FLOAT       AECLuxIndex;                ///< Input trigger value:  Lux index
    FLOAT       AWBleftGGainWB;             ///< Input trigger value:  AWB G channel gain
    FLOAT       AWBleftBGainWB;             ///< Input trigger value:  AWB B channel gain
    FLOAT       AWBleftRGainWB;             ///< Input trigger value:  AWB R channel gain
    FLOAT       AWBColorTemperature;        ///< Input Trigger value:  AWB CCT value
    FLOAT       DRCGain;                    ///< Input trigger value:  DRCGain
    FLOAT       DRCGainDark;                ///< Input trigger value:  DRCGainDark
    FLOAT       lensPosition;               ///< Input trigger value:  Lens position
    FLOAT       lensZoom;                   ///< Input trigger value:  Lens Zoom
    FLOAT       postScaleRatio;             ///< Input trigger value:  Post scale ratio
    FLOAT       preScaleRatio;              ///< Input trigger value:  Pre scale ratio
    UINT32      sensorImageWidth;           ///< Current Sensor Image Output Width
    UINT32      sensorImageHeight;          ///< Current Sensor Image Output Height
    UINT32      CAMIFWidth;                 ///< Width of CAMIF Output
    UINT32      CAMIFHeight;                ///< Height of CAMIF Output
    UINT16      numberOfLED;                ///< Number of LED
    INT32       LEDSensitivity;             ///< LED Sensitivity
    UINT32      bayerPattern;               ///< Bayer pattern
    UINT32      sensorOffsetX;              ///< Current Sensor Image Output horizontal offset
    UINT32      sensorOffsetY;              ///< Current Sensor Image Output vertical offset
    UINT32      blackLevelOffset;           ///< Black level offset
} CAMX_PACKED;

/// @brief Structure encapsulating Sensor configuration data
struct IFEBPSSensorConfigData
{
    INT32   isBayer;                    ///< Flag to indicate Bayer sensor
    INT32   format;                     ///< Image Format
    FLOAT   digitalGain;                ///< Digital Gain Value
    UINT32  ZZHDRColorPattern;          ///< ZZHDR Color Pattern Information
    UINT32  ZZHDRFirstExposurePattern;  ///< ZZHDR First Exposure Pattern
} CAMX_PACKED;

struct IFEHDRMACReconData
{
    UINT32 HDRMACReconConfig[IFEHDRMACReconConfigRegisters];    ///< HDR MAC and Recon registers data
    UINT32 HDRBLK[IFEHDRMACReconBLKRegisters];                  ///< HDR BLK data
    UINT32 HDRReconConfig[IFEHDRReconConfigRegisters];          ///< HDR Recon only register data
} CAMX_PACKED;

/// @brief Structure to encapsulate metadata from sensor, 3A and IFE
struct IFETuningMetadata
{
    SensorTuningData            sensorData;         ///< Sensor tuning data information
    IFETuningModuleEnableConfig IFEEnableConfig;    ///< IFE Structure to hold Crop, Round and Clamp module enable registers
    IFEBPSSensorConfigData      IFESensorConfig;    ///< Sensor configuration data
    IFEDMITuningMetadata        IFEDMIData;         ///< IFE DMI metadata
    IFETuningTriggerData        IFETuningTriggers;  ///< IFE input trigger data
    IFEHDRMACReconData          IFEHDRData;         ///< IFE HDR MAC & Recon data
} CAMX_PACKED;

/// @brief Structure encapsulating Trigger data
struct BPSTuningTriggerData
{
    FLOAT       AECexposureTime;            ///< Input trigger value:  exposure time
    FLOAT       AECSensitivity;             ///< Input trigger value:  AEC sensitivity data
    FLOAT       AECGain;                    ///< Input trigger value:  AEC Gain Value
    FLOAT       AECLuxIndex;                ///< Input trigger value:  Lux index
    FLOAT       AWBleftGGainWB;             ///< Input trigger value:  AWB G channel gain
    FLOAT       AWBleftBGainWB;             ///< Input trigger value:  AWB B channel gain
    FLOAT       AWBleftRGainWB;             ///< Input trigger value:  AWB R channel gain
    FLOAT       AWBColorTemperature;        ///< Input Trigger value:  AWB CCT value
    FLOAT       DRCGain;                    ///< Input trigger value:  DRCGain
    FLOAT       DRCGainDark;                ///< Input trigger value:  DRCGainDark
    FLOAT       lensPosition;               ///< Input trigger value:  Lens position
    FLOAT       lensZoom;                   ///< Input trigger value:  Lens Zoom
    FLOAT       postScaleRatio;             ///< Input trigger value:  Post scale ratio
    FLOAT       preScaleRatio;              ///< Input trigger value:  Pre scale ratio
    UINT32      sensorImageWidth;           ///< Current Sensor Image Output Width
    UINT32      sensorImageHeight;          ///< Current Sensor Image Output Height
    UINT32      CAMIFWidth;                 ///< Width of CAMIF Output
    UINT32      CAMIFHeight;                ///< Height of CAMIF Output
    UINT16      numberOfLED;                ///< Number of LED
    INT32       LEDSensitivity;             ///< LED Sensitivity
    UINT32      bayerPattern;               ///< Bayer pattern
    UINT32      sensorOffsetX;              ///< Current Sensor Image Output horizontal offset
    UINT32      sensorOffsetY;              ///< Current Sensor Image Output vertical offset
    UINT32      blackLevelOffset;           ///< Black level offset
} CAMX_PACKED;

/// @brief Structure to encapsulate custom OEM data from IQ modules
struct IQOEMTuningMetadata
{
    FLOAT   IQOEMTuningData[IQOEMTuningData];
} CAMX_PACKED;

/// @brief Structure to encapsulate metadata from BPS
struct BPSTuningMetadata
{
    BPSDMITuningMetadata            BPSDMIData;
    BPSPedestalTuningMetadata       BPSPedestalData;
    BPSLinearizationTuningMetadata  BPSLinearizationData;
    BPSBPCPDPCTuningMetadata        BPSBBPCPDPCData;
    BPSHDRTuningMetadata            BPSHDRData;
    BPSGICTuningMetadata            BPSGICData;
    BPSABFTuningMetadata            BPSABFData;
    BPSLSCTuningMetadata            BPSLSCData;
    BPSWBTuningMetadata             BPSWBData;
    BPSDemosaicTuningMetadata       BPSDemosaicData;
    BPSDemuxTuningMetadata          BPSDemuxData;
    BPSCCTuningMetadata             BPSCCData;
    CSTTuningMetadata               BPSCSTData;
    BPSHNRTuningMetadata            BPSHNRData;
    BPSModulesConfigTuningMetadata  BPSModuleConfigData;
    BPSTuningTriggerData            BPSTuningTriggers;
    IFEBPSSensorConfigData          BPSSensorConfig;
    FLOAT                           BPSExposureGainRatio;
    IQOEMTuningMetadata             oemTuningData;
    TuningFaceData                  BPSHNRFaceDetection;
    IQNodeInformation               BPSNodeInformation;
} CAMX_PACKED;

/// @brief Structure to encapsulate metadata from IPE
struct IPETuningMetadata
{
    IPEDMITuningMetadata            IPEDMIData;
    IPEModulesConfigTuningMetadata  IPEModuleConfigData;
    IPEANRTuningMetadata            IPEANRData;
    IPETFTuningMetadata             IPETFData;
    IPECACTuningMetadata            IPECACData;
    CSTTuningMetadata               IPECSTData;
    IPELTMTuningExpData             IPELTMExposureData;
    IPECCTuningMetadata             IPECCData;
    IPE2DLUTTuningMetadata          IPE2DLUTData;
    IPEChromaEnhancementMetadata    IPEChromaEnhancementData;
    IPEChromaSuppressionMetadata    IPEChromaSuppressionData;
    IPESCEMetadata                  IPESCEData;
    IPEASFMetadata                  IPEASFData;
    TuningFaceData                  IPEASFFaceDetection;
    TuningFaceData                  IPEANRFaceDetection;
    IQNodeInformation               IPENodeInformation;
    TuningModeDebugData             IPETuningModeDebugData;
} CAMX_PACKED;

CAMX_END_PACKED

class TDDebugDataWriter : public DebugDataWriter
{
public:
    TDDebugDataWriter()
        : DebugDataWriter(WriterId::OtherWriter)
    {
    }

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PopulateTypeDefinitionTag
    ///
    /// @brief  Adds all fields for this specific type of tag. This is an implementation of a pure virtual function.
    ///
    /// @param  type    The tag type to be added
    ///
    /// @return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult PopulateTypeDefinitionTag(
        DebugDataTagType type)
    {
        BOOL bSuccess = FALSE;

        switch (type)
        {
            case DebugDataTagType::TuningIFEEnableConfig:
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                break;
            case DebugDataTagType::TuningIFETriggerData:
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt16);
                bSuccess |= AddTypedefField(DebugDataTagType::Int32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                break;
            case DebugDataTagType::TuningAECExposureData:
                bSuccess |= AddTypedefField(DebugDataTagType::UInt64);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                break;
            case DebugDataTagType::TuningAECFrameControl:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::TuningAECExposureData,
                                                 CAMX_ARRAY_SIZE(TuningAECFrameControl::exposureInfo));
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt8);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt8);
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(TuningAECFrameControl::LEDCurrents));
                break;
           case DebugDataTagType::TuningAWBFrameControl:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::Float,
                                                 CAMX_ARRAY_SIZE(TuningAWBFrameControl::AWBGains));
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::Bool);
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::Float,
                                                 CAMX_ARRAY_SIZE(TuningAWBFrameControl::CCMOffset));
                bSuccess |= AddTypedefField2DArray(DebugDataTagType::Float,
                                                   CAMX_ARRAY_ROWS(TuningAWBFrameControl::CCM),
                                                   CAMX_ARRAY_COLS(TuningAWBFrameControl::CCM));
                break;
            case DebugDataTagType::TuningSensorResolution:
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                break;
            case DebugDataTagType::TuningSensorCropInfo:
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                break;
            case DebugDataTagType::TuningLSCMeshLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(LSCMeshLUT::GRRLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(LSCMeshLUT::GBBLUT));
                break;
            case DebugDataTagType::TuningGammaCurve:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(GammaCurve::curve));
                break;
            case DebugDataTagType::TuningGTMLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt64,
                                                 CAMX_ARRAY_SIZE(GTMLUT::LUT));
                break;
            case DebugDataTagType::TuningPedestalLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(PedestalLUT::GRRLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(PedestalLUT::GBBLUT));
                break;
            case DebugDataTagType::TuningIFEHDR:
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                break;
            case DebugDataTagType::TuningBPSBPCPDPCConfig:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSBPCPDPCTuningMetadata::BPCPDPConfig0));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSBPCPDPCTuningMetadata::BPCPDPConfig1));
                break;
            case DebugDataTagType::TuningBPSHDRConfig:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSHDRTuningMetadata::HDRReconConfig));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSHDRTuningMetadata::HDRMACConfig));
                break;
            case DebugDataTagType::TuningABFLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSABFLUT::noiseLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSABFLUT::noise1LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSABFLUT::activityLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSABFLUT::darkLUT));
                break;
            case DebugDataTagType::TuningBPSHNRLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSHNRLUT::LNRLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSHNRLUT::FNRAndClampLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSHNRLUT::FNRAcLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSHNRLUT::SNRLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSHNRLUT::blendLNRLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(BPSHNRLUT::blendSNRLUT));
                break;
            case DebugDataTagType::TuningBPSTriggerData:
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt16);
                bSuccess |= AddTypedefField(DebugDataTagType::Int32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                break;
            case DebugDataTagType::TuningIFESensorConfig:
            case DebugDataTagType::TuningBPSSensorConfig:
                bSuccess |= AddTypedefField(DebugDataTagType::Int32);
                bSuccess |= AddTypedefField(DebugDataTagType::Int32);
                bSuccess |= AddTypedefField(DebugDataTagType::Float);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                break;
            case DebugDataTagType::TuningICALUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEICALUT::PerspectiveLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEICALUT::grid0LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEICALUT::grid1LUT));
                break;
            case DebugDataTagType::TuningANRConfig:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEANRTuningDCBlend1RegCmd::DCBlend1));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEANRTuningRNFRegCmd::RNF));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEANRTuningDCUSRegCmd::DCUS));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEANRTuningCFilter2RegCmd::CFilter2));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEANRTuningDCBlend2RegCmd::DCBlend2));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEANRTuningCYLPFPreLensGainRegCmd::CYLPFPreLensGain));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEANRTuningCYLPFLensGainRegCmd::CYLPFLensGain));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEANRTuningCYLPFPostLensGainRegCmd::CYLPFPostLensGain));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEANRTuningCNRRegCmd::CNR));
                break;
            case DebugDataTagType::TuningTFConfig:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPETFTuningPassData::TFRegister));
                break;
            case DebugDataTagType::TuningLTMLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::WeightLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::LA0LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::LA1LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::CurveLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::ScaleLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::MaskLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::LCEPositiveLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::LCENegativeLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::RGamma0LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::RGamma1LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::RGamma2LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::RGamma3LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::RGamma4LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPELTMLUT::AverageLUT));
                break;
            case DebugDataTagType::Tuning2DLUTLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Hue0LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Hue1LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Hue2LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Hue3LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Saturation0LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Saturation1LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Saturation2LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Saturation3LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::InverseHueLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::InverseSaturationLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Hue1DLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPE2DLUTLUT::Saturation1DLUT));
                break;
            case DebugDataTagType::TuningASFLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEASFLUT::Layer1ActivityNormalGainPosGainNegSoftThLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEASFLUT::Layer1GainWeightLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEASFLUT::Layer1SoftThresholdWeightLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEASFLUT::Layer2ActivityNormalGainPosGainNegSoftThLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEASFLUT::Layer2GainWeightLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEASFLUT::Layer2SoftThresholdWeightLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEASFLUT::ChromaGradientPosNegLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEASFLUT::ContrastPosNegLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEASFLUT::SkinActivityGainLUT));
                break;
            case DebugDataTagType::TuningUpscalerLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEUpscalerLUT::ALUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEUpscalerLUT::BLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEUpscalerLUT::CLUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEUpscalerLUT::DLUT));
                break;
            case DebugDataTagType::TuningGrainAdderLUT:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEGrainAdderLUT::Channel1LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEGrainAdderLUT::Channel2LUT));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEGrainAdderLUT::Channel3LUT));
                break;
            case DebugDataTagType::TuningFaceData:
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(TuningFaceData::faceCenterX));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(TuningFaceData::faceCenterY));
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(TuningFaceData::faceRadius));
                break;
            case DebugDataTagType::TuningGammaIPECurve:
                bSuccess |= AddTypedefFieldArray(DebugDataTagType::UInt32,
                                                 CAMX_ARRAY_SIZE(IPEGammaCurve::curve));
                break;
            case DebugDataTagType::TuningIQNodeInfo:
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt64);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                break;
            case DebugDataTagType::TuningModeInfo:
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                bSuccess |= AddTypedefField(DebugDataTagType::UInt32);
                break;
            default:
                return CamxResultEUnsupported;
        }

        if (FALSE == bSuccess)
        {
            return CamxResultENoMemory;
        }

        return CamxResultSuccess;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetSizeOfType
    ///
    /// @brief  Get the size of customer type
    ///
    /// @param  type    The data type to get the size of
    ///
    /// @return Size of data
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    SIZE_T GetSizeOfType(
        DebugDataTagType type)
    {
        SIZE_T size;

        // Short circuit this method if its a built-in type
        size = GetSizeOfBuiltInType(type);
        if (0 != size)
        {
            return size;
        }

        switch (type)
        {
            case DebugDataTagType::TuningIFEEnableConfig:
                size = sizeof(IFETuningModuleEnableConfig);
                break;
            case DebugDataTagType::TuningIFETriggerData:
                size = sizeof(IFETuningTriggerData);
                break;
            case DebugDataTagType::TuningAECExposureData:
                size = sizeof(TuningAECExposureData);
                break;
            case DebugDataTagType::TuningAECFrameControl:
                size = sizeof(TuningAECFrameControl);
                break;
            case DebugDataTagType::TuningAWBFrameControl:
                size = sizeof(TuningAWBFrameControl);
                break;
            case DebugDataTagType::TuningSensorResolution:
                size = sizeof(SensorResolution);
                break;
            case DebugDataTagType::TuningSensorCropInfo:
                size = sizeof(SensorCropInfo);
                break;
            case DebugDataTagType::TuningLSCMeshLUT:
                size = sizeof(LSCMeshLUT);
                break;
            case DebugDataTagType::TuningGammaCurve:
                size = sizeof(GammaCurve);
                break;
            case DebugDataTagType::TuningGTMLUT:
                size = sizeof(GTMLUT);
                break;
            case DebugDataTagType::TuningPedestalLUT:
                size = sizeof(PedestalLUT);
                break;
            case DebugDataTagType::TuningIFEHDR:
                size = sizeof(IFEHDRMACReconData);
                break;
            case DebugDataTagType::TuningBPSBPCPDPCConfig:
                size = sizeof(BPSBPCPDPCTuningMetadata);
                break;
            case DebugDataTagType::TuningBPSHDRConfig:
                size = sizeof(BPSHDRTuningMetadata);
                break;
            case DebugDataTagType::TuningABFLUT:
                size = sizeof(BPSABFLUT);
                break;
            case DebugDataTagType::TuningBPSHNRLUT:
                size = sizeof(BPSHNRLUT);
                break;
            case DebugDataTagType::TuningBPSTriggerData:
                size = sizeof(BPSTuningTriggerData);
                break;
            case DebugDataTagType::TuningIFESensorConfig:
            case DebugDataTagType::TuningBPSSensorConfig:
                size = sizeof(IFEBPSSensorConfigData);
                break;
            case DebugDataTagType::TuningICALUT:
                size = sizeof(IPEICALUT);
                break;
            case DebugDataTagType::TuningANRConfig:
                size = sizeof(IPEANRTuningPassData);
                break;
            case DebugDataTagType::TuningTFConfig:
                size = sizeof(IPETFTuningPassData);
                break;
            case DebugDataTagType::TuningLTMLUT:
                size = sizeof(IPELTMLUT);
                break;
            case DebugDataTagType::Tuning2DLUTLUT:
                size = sizeof(IPE2DLUTLUT);
                break;
            case DebugDataTagType::TuningASFLUT:
                size = sizeof(IPEASFLUT);
                break;
            case DebugDataTagType::TuningUpscalerLUT:
                size = sizeof(IPEUpscalerLUT);
                break;
            case DebugDataTagType::TuningGrainAdderLUT:
                size = sizeof(IPEGrainAdderLUT);
                break;
            case DebugDataTagType::TuningFaceData:
                size = sizeof(TuningFaceData);
                break;
            case DebugDataTagType::TuningGammaIPECurve:
                size = sizeof(IPEGammaCurve);
                break;
            case DebugDataTagType::TuningIQNodeInfo:
                size = sizeof(IQNodeInformation);
                break;
            case DebugDataTagType::TuningModeInfo:
                size = sizeof(TuningModeDebugData);
                break;
            default:
                break;
        }

        return size;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AddTypeDefinitionTags
    ///
    /// @brief  Add a set of custom type definition tags based on the implementation requirements
    ///
    /// @return  CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult AddTypeDefinitionTags()
    {
        return DebugDataWriter::AddTypeDefinitionTags(DebugDataTagType::TuningTypeBegin, DebugDataTagType::TuningTypeEnd);
    }
};


CAMX_NAMESPACE_END
#endif // CAMXTUNINGDUMP_H
