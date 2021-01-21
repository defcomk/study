////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxispiqmodule.h
/// @brief TITAN IQ Module base class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXISPIQMODULE_H
#define CAMXISPIQMODULE_H

#include "chistatsproperty.h"
#include "chitintlessinterface.h"

#include "camxcmdbuffermanager.h"
#include "camxdefs.h"
#include "camxformats.h"
#include "camxhal3types.h"
#include "camxhwcontext.h"
#include "camximageformatutils.h"
#include "camxifeproperty.h"
#include "camxmem.h"
#include "camxmetadatapool.h"
#include "camxpacketbuilder.h"
#include "camxsensorproperty.h"
#include "camxtitan17xdefs.h"
#include "camxtuningdump.h"
#include "chiiqmodulesettings.h"
#include "chiisphvxdefs.h"
#include "chipdlibinterface.h"
#include "ipe_data.h"
#include "iqcommondefs.h"
#include "titan170_base.h"
#include "titan170_bps.h"
#include "titan170_ife.h"
#include "titan170_ipe.h"
#include "stripeIFE.h"
#include "lsc34setting.h"
#include "dsv16stripe.h"

#include "chituningmodeparam.h"

CAMX_NAMESPACE_BEGIN

class  ISPIQModule;

/// @brief Fixed point number with 12 fractional bits
static const UINT32 Q12 = 1 << 12;

/// @brief Fixed point number with 7 fractional bits
static const UINT32 Q7 = 1 << 7;

/// @brief DMI Entry size, in bytes
static const UINT DMIEntrySize = 16;

typedef UINT32 IFEPath;

static const IFEPath MaxMNDSPath       = 3;    ///< Maximum paths for MNDS Output

static const IFEPath FDOutput          = 0;    ///< MNDS/RoundClamp FD output path
static const IFEPath FullOutput        = 1;    ///< MNDS/RoundClamp Full output path
static const IFEPath DS4Output         = 2;    ///< RoundClamp DS4 output path
static const IFEPath DS16Output        = 3;    ///< RoundClamp DS16 output path
static const IFEPath PixelRawOutput    = 4;    ///< RoundClamp pixel raw dump
static const IFEPath PDAFRawOutput     = 5;    ///< PDAF RAW Output
static const IFEPath DisplayFullOutput = 6;    ///< MNDS/RoundClamp Display Full output path
static const IFEPath DisplayDS4Output  = 7;    ///< RoundClamp Display DS4 output path
static const IFEPath DisplayDS16Output = 8;    ///< RoundClamp Display DS16 output path
static const IFEPath MaxRoundClampPath = 9;    ///< Maximum paths for Round and Clamp output

static const UINT32 MaxStatsPorts = CSLIFEPortIdStatIHIST - CSLIFEPortIdStatHDRBE + 1; ///< Maximum num of stats ports

static const UINT32 DS4Factor  = 4;    ///< Constant to calculate DS4 output with respect to Full out
static const UINT32 DS16Factor = 16;   ///< Constant to calculate DS16 output with respect to Full out
static const UINT32 DS64Factor = 64;   ///< Constant to calculate DS64 output with respect to Full out

static const UINT32 YUV420SubsampleFactor = 2;  ///< Subsample factor of chroma data with respect to Luma data

static const UINT32 ColorCorrectionRows     = 3;    ///< Color correction transform matrix rows
static const UINT32 ColorCorrectionColumns  = 3;    ///< Color correction transform matrix columns
static const UINT32 TotalChannels           = 4;    ///< Total IFE channels R, Gr, Gb and B

enum ISPChannel
{
    ISPChannelRed = 0,      ///< ISP red channel
    ISPChannelGreenOdd,     ///< ISP green odd channel
    ISPChannelGreenEven,    ///< ISP green even channel
    ISPChannelBlue,         ///< ISP blue channel
    ISPChannelMax           ///< Max ISP channel
};

/// @ brief enumeration defining IPE path
enum IPEPath
{
    INPUT = 0,    ///< Input
    REFERENCE,    ///< Reference
};

/// @brief ICAInputData
struct ICAConfigParameters
{
    IPEICAPerspectiveTransform  ICAInPerspectiveParams;       ///< Perspective parameters for ICA 1
    IPEICAGridTransform         ICAInGridParams;              ///< Grid parameters for ICA 1
    IPEICAInterpolationParams   ICAInInterpolationParams;     ///< Interpolation parameters for ICA 1
    IPEICAPerspectiveTransform  ICARefPerspectiveParams;      ///< Perspective parameters for ICA 2
    IPEICAGridTransform         ICARefGridParams;             ///< Grid parameters for ICA 2
    IPEICAInterpolationParams   ICARefInterpolationParams;    ///< Interpolation parameters for ICA 2
    IPEICAPerspectiveTransform  ICAReferenceParams;           ///< Reference (LRME) parameters for ICA 2
    VOID*                       pCurrICAInData;               ///< ICA input current frame Data used by NCLib
    VOID*                       pPrevICAInData;               ///< ICA reference current frame Data
    VOID*                       pCurWarpAssistData;           ///< Current Warp assist data output from NcLib
    VOID*                       pPrevWarpAssistData;          ///< Current Warp assist data output from NcLib
};

static const UINT ICAReferenceNumber = 2;

/// @brief ISP color correction gains
enum ISPColorCorrectionGain
{
    ISPColorCorrectionGainsRed,         ///< Red gain
    ISPColorCorrectionGainsGreenRed,    ///< Green-Red gain
    ISPColorCorrectionGainsGreenBlue,   ///< Green-Blue gain
    ISPColorCorrectionGainsBlue,        ///< Blue gain
    ISPColorCorrectionGainsMax,         ///< Total number of gains
};

/// @brief Command buffer managers in IPE node
enum IPECmdBufferId
{
    CmdBufferFrameProcess = 0,    ///< 0: Top level firmware payload (IpeFrameProcess)
    CmdBufferStriping,            ///< 1: Striping library output (IpeStripingLibOutMultiFrame)
    CmdBufferIQSettings,          ///< 2: IQsettings within top level payload (IpeIQSettings)
    CmdBufferPreLTM,              ///< 3: generic cdm payload of all pre LTM IQ modules
    CmdBufferPostLTM,             ///< 4: generic cdm payload of all post LTM IQ modules
    CmdBufferDMIHeader,           ///< 5: CDM headers for all DMI LUTs from IPE IQ modules
    CmdBufferIQPacket,            ///< 6: IQ Packet Manager
    CmdBufferNPS,                 ///< 7: CMD payload for NPS: ANR/TF modules
    CmdBufferBLMemory,            ///< 8: BL Memory Command buffer used by firmware
    CmdBufferGenericBlob,         ///< 9: KMD specific buffer
    IPECmdBufferMaxIds,           ///< 10: Max number of command buffers in IPQ node
};

/// @brief CDM Program Array Order : CDMProgramArrays are appended at the bottom of firmware payload (i.e. IpeFrameProcess)
/// For better coding and debugging of payload values, enforcing below order in which CDMProgramArrays shall be appended.
enum CDMProgramArrayOrder
{
    ProgramArrayANRFullPass = 0,             ///< 0: ANR Full Pass index
    ProgramArrayANRDS4,                      ///< 1: ANR DS4 Pass index
    ProgramArrayANRDS16,                     ///< 2: ANR DS16 Pass index
    ProgramArrayANRDS64,                     ///< 3: ANR DS64 Pass index
    ProgramArrayTFFullPass,                  ///< 4: TF Full Pass index
    ProgramArrayTFDS4,                       ///< 5: TF DS4 Pass index
    ProgramArrayTFDS16,                      ///< 6: TF DS16 Pass index
    ProgramArrayTFDS64,                      ///< 7: TF DS64 Pass index
    ProgramArrayPreLTM,                      ///< 8: Pre LTM data index
    ProgramArrayPostLTM,                     ///< 9: Post LTM data index
    ProgramArrayICA1,                        ///< 10; ICA1 data index
    ProgramArrayICA2 =
        ProgramArrayICA1 + MAX_HFR_GROUP,    ///< 18 ICA2 data index
    ProgramArrayMax,                         ///< 26: Max CDM Program Arrays mentioned in firmware payload
};

/// @brief CDM Program Order for Pre LTM
enum PreLTMCDMProgramOrder
{
    ProgramIndexPreLTMGeneric = 0,  ///< 0: Generic type of program
    ProgramIndexLTM,                ///< 1: PreLTM Module - LTM DMI index
    ProgramIndexMaxPreLTM,          ///< 2: Max number of PreLTM Cdm Programs
};

/// @brief CDM Program Order for Post LTM
enum PostLTMCDMProgramOrder
{
    ProgramIndexPostLTMGeneric = 0, ///< 0: Generic type of program
    ProgramIndexGLUT,               ///< 1: PostLTM Module - Gamma DMI program index
    ProgramIndex2DLUT,              ///< 2: PostLTM Module - 2D LUT DMI program index
    ProgramIndexASF,                ///< 3: PostLTM Module - ASF DMI program index
    ProgramIndexUpscale,            ///< 4: PostLTM Module - Upscale DMI program index
    ProgramIndexGRA,                ///< 5: PostLTM Module - GRA DMI program index
    ProgramIndexMaxPostLTM,         ///< 6: Max number of PostLTM Cdm Programs
};

enum ICAProgramOrder
{
    ProgramIndexICA1 = 0,   ///< ICA program index 1
    ProgramIndexICA2,       ///< ICA program index 2
    ProgramIndexICAMax      ///< ICA program index count
};
// FD and Full out paths are output from MNDS path, it should not exceed 2
CAMX_STATIC_ASSERT((FDOutput < 2) && (FullOutput < 2));

// DS4 and DS16 out paths are output from RoundandClamp path, it should not exceed 4
CAMX_STATIC_ASSERT((DS4Output < 4) && (DS16Output < 4));

/// @brief Aspect ratio tolerence beyond which aspect ratio adjustment required
static const FLOAT AspectRatioTolerance = 0.01f;

/// @brief Bits per pixel for different formats
static const UINT32 BitWidthEight  = 8;
static const UINT32 BitWidthTen    = 10;
static const UINT32 Max8BitValue   = (1 << 8) - 1;   ///< 2^8 - 1
static const UINT32 Max10BitValue  = (1 << 10) - 1;  ///< 2^10 - 1

/// @brief Command buffer managers in BPS node
enum BPSCmdBufferId
{
    BPSCmdBufferFrameProcess = 0,    ///< 0: Top level firmware payload (BpsFrameProcess)
    BPSCmdBufferStriping,            ///< 1: Striping library output (BpsStripingLibOutMultiFrame)
    BPSCmdBufferIQSettings,          ///< 2: IQsettings within top level payload (BpsIQSettings)
    BPSCmdBufferDMIHeader,           ///< 3: CDM headers for all DMI LUTs from BPS IQ modules
    BPSCmdBufferCDMProgram,          ///< 4: CDM headers for all CDM programs in BPS
    BPSCmdBufferIQPacket,            ///< 5: IQ Packet
    BPSCmdBufferBLMemory,            ///< 6: BL Memory Command buffer used by firmware
    BPSCmdBufferGenericBlob,         ///< 7: KMD specific buffer
    BPSCmdBufferMaxIds,              ///< 8: Max number of command buffers in BPS node
};

/// @brief BPS CDM Program Order
enum BPSCDMProgramOrder
{
    BPSProgramIndexGeneric = 0,      ///< 0: Generic type of program for BPS
    BPSProgramIndexPEDESTAL,         ///< 1: Pedestal DMI program index
    BPSProgramIndexLINEARIZATION,    ///< 2: Linearization DMI program index
    BPSProgramIndexBPCPDPC,          ///< 3: BPC PDPC DMI program index
    BPSProgramIndexGIC,              ///< 4: GIC DMI program index
    BPSProgramIndexABF,              ///< 5: ABF DMI program index
    BPSProgramIndexRolloff,          ///< 6: Roll off DMI program index
    BPSProgramIndexGTM,              ///< 7: GTM program index
    BPSProgramIndexGLUT,             ///< 8: GLUT DMI program index
    BPSProgramIndexHNR,              ///< 9: HNR DMI program index
    BPSProgramIndexMax,              ///< 10: Max number of BPS Cdm Program
};

/// @brief TITAN IQ Module Type
enum class ISPIQModuleType
{
    IFECAMIF,              ///< Camera Interface Module of IFE
    IFECAMIFLite,          ///< Camera Interface Module of Dual PD
    IFEPedestalCorrection, ///< Pedestal Correction Module of IFE
    IFEBLS,                ///< Black Level Substraction Module of IFE
    IFELinearization,      ///< Linearization Module of IFE
    IFEDemux,              ///< Demux Module of IFE
    IFEHDR,                ///< HDR Module of IFE
    IFEPDPC,               ///< PDAF pixel correction Module of IFE
    IFEBPCBCC,             ///< Bad Pixel Corection Module of IFE
    IFEABF,                ///< Adaptive Bayer Filter Module of IFE
    IFELSC,                ///< Lens Shading Correction of IFE
    IFEDemosaic,           ///< Demosaic Module of IFE
    IFECC,                 ///< Color Correction Module of IFE
    IFEGTM,                ///< Global Tone Mapping of IFE
    IFEGamma,              ///< Gamma Correction Module of IFE
    IFECST,                ///< Color Space Transform Module of IFE
    IFEMNDS,               ///< Scaler Module of IFE
    IFEPreCrop,            ///< DS4 Pre Crop Module of IFE
    IFEDS4,                ///< DS4 Module of IFE
    IFECrop,               ///< Fov and Crop Module of IFE
    IFERoundClamp,         ///< Round and Clamp Module of IFE
    IFER2PD,               ///< R2PD Module of IFE
    IFEWB,                 ///< White Balance Module of IFE
    IFEHVX,                ///< Custom IQ module implemented in HVX
    IFEDUALPD,             ///< Dual PD HW

    IPEICA,                ///< Image Correction and Adjustment Module in NPS
    IPERaster22,           ///< Raster Module in NPS
    IPERasterPD,           ///< Raster Module in PD path in NPS
    IPEANR,                ///< Advanced Noise Reduction Module in NPS
    IPETF,                 ///< Temporal Filter Module in NPS
    IPECAC,                ///< Colour Aberration Correction Module in PPS
    IPECrop,               ///< Standalone or prescaler Crop Module in IPE
    IPEChromaUpsample,     ///< Chroma Upsampling Module in PPS
    IPECST,                ///< Color Space Transform Module in PPS
    IPELTM,                ///< Local Tone Map Module in PPS
    IPEColorCorrection,    ///< Color Correction Module in PPS
    IPEGamma,              ///< Gamma Correction Module
    IPE2DLUT,              ///< 2D LUT Module in PPS
    IPEChromaEnhancement,  ///< Chroma Enhancement Module in PPS
    IPEChromaSuppression,  ///< Chroma Suppression Module in PPS
    IPESCE,                ///< Skin Coloe Enhancement Module in PPS
    IPEASF,                ///< Adaptive Spatial Filter Module in PPS
    IPEUpscaler,           ///< Up Scaler Module in PPS
    IPEGrainAdder,         ///< Grain Adder Module in PPS
    IPEDownScaler,         ///< Down Scaler Module
    IPEFOVCrop,            ///< Fov and Crop Module after downscaler
    IPEClamp,              ///< Clamp Module

    BPSPedestalCorrection, ///< Pedestal Correction Module in BPS
    BPSLinearization,      ///< Linearization Module in BPS
    BPSBPCPDPC,            ///< Bad Pixel Corection & PDPC Module in BPS
    BPSDemux,              ///< Demux Module in BPS
    BPSHDR,                ///< HDR Motion artifcat/Reconstruction module in BPS
    BPSABF,                ///< Adaptive Bayer Filter Module in BPS
    BPSLSC,                ///< Lens shading module in BPS
    BPSGIC,                ///< GB/GR Imbalance correction module in BPS
    BPSDemosaic,           ///< Demosaic Module in BPS
    BPSCC,                 ///< Color Correction Module in BPS
    BPSGTM,                ///< Global Tone Mapping in BPS
    BPSGamma,              ///< Gamma Correction Module in BPS
    BPSCST,                ///< Color Space Transform Module in BPS
    BPSChromaSubSample,    ///< Chroma Sub sampling module in BPS
    BPSHNR,                ///< HNR module in BPS
    BPSWB,                 ///< White Balance Module in BPS

    SWTMC,                 ///< SW TMC module
};

/// @brief IFE pipeline path
enum class IFEPipelinePath: UINT
{
    FullPath = 0,     ///< Full path of IFE pipeline
    FDPath,           ///< FD path of IFE pipeline
    DS4Path,          ///< DS4 path of IFE pipeline
    DS16Path,         ///< DS16 path of IFE pipeline
    DisplayFullPath,  ///< Display full path of IFE pipeline
    DisplayDS4Path,   ///< Display DS4 path of IFE pipeline
    DisplayDS16Path,  ///< Display DS16 path of IFE pipeline
    CommonPath,       ///< Common path of IFE pipeline
    PixelRawDumpPath, ///< pixel raw path of IFE pipeline
};

static const UINT IFEMaxNonCommonPaths = 9; ///< Max number of non-Common pipeline paths

enum class IFEModuleMode
{
    SingleIFENormal,    ///< Single IFE Normal Mode
    DualIFENormal       ///< Dual IFE Normal Mode
};

/// @brief structure to pack all IFE module flags
union IFEModuleFlags
{
    struct
    {
        BIT     isBayer             : 1;   ///< Bayer Sensor type
        BIT     isFDPathEnable      : 1;   ///< FD output path of IFE
        BIT     isFullPathEnable    : 1;   ///< Full output path of IFE
        BIT     isDS4PathEnable     : 1;   ///< DS4 output path of IFE
        BIT     isDS16PathEnable    : 1;   ///< DS16 output path of IFE
    };
    UINT    allFlags;
};

/// @brief IFE output paths
struct IFEOutputPath
{
    UINT32  path;   ///< Currently selected path
};

/// @brief DSP Mode
enum IFEDSPMode
{
    DSPModeNone = 0,          ///< No DSP mode is selected
    DSPModeOneWay,            ///< DSP mode one Way
    DSPModeRoundTrip,         ///< DSP mode Round trip
};

/// @brief This structure encapsulates data for a CAMIF crop request from the sensor
struct CropInfo
{
    UINT32    firstPixel;    ///< starting pixel for CAMIF crop
    UINT32    firstLine;     ///< starting line for CAMIF crop
    UINT32    lastPixel;     ///< last pixel for CAMIF crop
    UINT32    lastLine;      ///< last line for CAMIF crop
};

/// @brief SensorAspectRatioMode
enum SensorAspectRatioMode
{
    FULLMODE,                ///< 4:3 sensor
    NON_FULLMODE,            ///< other than 4:3, it can be 16:9
};

/// @ brief Structure to encapsulate sensor dependent data
struct ISPSensorConfigureData
{
    FLOAT              dGain;                       ///< Digital Gain Value
    PixelFormat        format;                      ///< Image Format
    CropInfo           CAMIFCrop;                   ///< CAMIF crop request from sensor
    StreamDimension    sensorOut;                   ///< Sensor output info
    BIT                isBayer;                     ///< Flag to indicate Bayer sensor
    BIT                isYUV;                       ///< Flag to indicate YUV sensor
    BIT                isMono;                      ///< Flag to indicate Mono sensor
    BIT                isIHDR;                      ///< Flag to indicate IHDR sensor
    FLOAT              sensorScalingFactor;         ///< Downscaling factor of sensor
    FLOAT              sensorBinningFactor;         ///< Sensor binning factor
    UINT               ZZHDRColorPattern;           ///< ZZHDR Color Pattern Information
    UINT               ZZHDRFirstExposurePattern;   ///< ZZHDR First Exposure Pattern
    SensorPDAFInfo     sensorPDAFInfo;              ///< Sensor PDAF information
    UINT32             fullResolutionWidth;         ///< Full resolution width
    UINT32             fullResolutionHeight;        ///< Full resolution height
    UINT32             sensorAspectRatioMode;       ///< for 4:3 senosr mode is FULLMODE(0) other mode NON_FULLMODE(1)
};

/// @ brief Structure to encapsulate HVX data
struct ISPHVXConfigureData
{
    StreamDimension    sensorInput;     ///< Sensor Input info
    PixelFormat        format;          ///< Image Format
    StreamDimension    HVXOut;          ///< HVX output info
    CropInfo           HVXCrop;         ///< HVX crop request
    CropInfo           origCAMIFCrop;   ///< Orig CAMIF
    CropWindow         origHALWindow;   ///< Orig Hal Window
    BOOL               DSEnabled;       ///< Down scale enabled
};

/// @ brief Structure to encapsulate CHI HVX interface
struct IFEHVXInfo
{
    CHIISPHVXALGORITHMCALLBACKS* pHVXAlgoCallbacks;  ///< Algo Calllback Interface for Chi to call into custom node.
    BOOL                         enableHVX;          ///< Enable HVX
    IFEDSPMode                   DSPMode;            ///< DSP mode
    ISPHVXConfigureData          HVXConfiguration;   ///< HVX configuration data
};

/// @ brief Structure to encapsulate CAMIF subsample pattern
struct ISPCAMIFSubsampleData
{
    UINT16 pixelSkipPattern; ///< Pixel Skip Pattern at CAMIF
    UINT16 lineSkipPattern;  ///< Line Skip pattern at CAMIF
};

/// @ brief Structure to encapsulate CAMIF Subsample data
struct IFECAMIFInfo
{
    BOOL                   enableCAMIFSubsample;         ///< Enable CAMIF SubSample
    ISPCAMIFSubsampleData  CAMIFSubSamplePattern;        ///< CAMIF subsample pattern
    CropInfo               PDAFCAMIFCrop;                ///< PDAF Camif Crop Information
};

/// @ brief Structure to encapsulate PDAF data
struct IFEPDAFInfo
{
    BOOL                 enableSubsample;         ///< Enable PDAF SubSample
    UINT16               pixelSkipPattern;        ///< Pixel Skip Pattern
    UINT16               lineSkipPattern;         ///< Line Skip Pattern
    UINT32               bufferWidth;             ///< Width of the Buffer
    UINT32               bufferHeight;            ///< Height of the buffer
    PDLibDataBufferInfo  pdafBufferInformation;   ///< Pdaf buffer information
};

/// @ brief Structure to encapsulate sensor dependent data
struct ISPHALConfigureData
{
    StreamDimension    stream[MaxRoundClampPath];    ///< HAL streams, corresponding to ISP output paths DS4, DS16, Full, FD
    Format             format[MaxRoundClampPath];    ///< output pixel format
};

/// @brief IPE has 2 main processing sections, noise processing and post processing. Within Post processing (PPS), they are
/// they are differentiated based on their position with respect to LT module as preLTM and postLTM.
enum class IPEProcessingSection
{
    IPEAll,             ///< All processing blocks
    IPENPS,             ///< Noise Processing blocks (ANR, TF etc.)
    IPEPPSPreLTM,       ///< Pre Processing section blocks before LTM (including LTM)
    IPEPPSPostLTM,      ///< Post Processing section blocks after LTM (Color correction to GRA)
    IPEPPS,             ///< Pre + Post processing blocks
    IPEScale,           ///< Scale Section
    IPENoZoomCrop,          ///< All processing blocks except for fill zoom window
    IPEInvalidSection,  ///< Invalid IPE IQ block
};

enum class BPSProcessingSection
{
    BPSAll,             ///< All processing blocks
    BPSHNR,             ///< BPS HNR processing block
    BPSLSCOut,          ///< BPS Preceding LSC processing block
    BPSPostLSC,         ///< BPS Post LSC processing block
    BPSInvalidSection,  ///< Invalid BPS IQ block
};

/// @brief Structure to hold Crop, Round and Clamp module enable registers
struct IFEModuleEnableConfig
{
    IFE_IFE_0_VFE_MODULE_LENS_EN                 lensProcessingModuleConfig;         ///< Lens processing Module enable
    IFE_IFE_0_VFE_MODULE_COLOR_EN                colorProcessingModuleConfig;        ///< Color processing Module enable
    IFE_IFE_0_VFE_MODULE_ZOOM_EN                 frameProcessingModuleConfig;        ///< Frame processing Module enable
    IFE_IFE_0_VFE_FD_OUT_Y_CROP_RND_CLAMP_CFG    FDLumaCropRoundClampConfig;         ///< FD Luma path Module enable
    IFE_IFE_0_VFE_FD_OUT_C_CROP_RND_CLAMP_CFG    FDChromaCropRoundClampConfig;       ///< FD Chroma path Module enable
    IFE_IFE_0_VFE_FULL_OUT_Y_CROP_RND_CLAMP_CFG  fullLumaCropRoundClampConfig;       ///< Full Luma path Module enable
    IFE_IFE_0_VFE_FULL_OUT_C_CROP_RND_CLAMP_CFG  fullChromaCropRoundClampConfig;     ///< Full Chroma path Module enable
    IFE_IFE_0_VFE_DS4_Y_CROP_RND_CLAMP_CFG       DS4LumaCropRoundClampConfig;        ///< DS4 Luma path Module enable
    IFE_IFE_0_VFE_DS4_C_CROP_RND_CLAMP_CFG       DS4ChromaCropRoundClampConfig;      ///< DS4 Chroma path Module enable
    IFE_IFE_0_VFE_DS16_Y_CROP_RND_CLAMP_CFG      DS16LumaCropRoundClampConfig;       ///< DS16 Luma path Module enable
    IFE_IFE_0_VFE_DS16_C_CROP_RND_CLAMP_CFG      DS16ChromaCropRoundClampConfig;     ///< DS16 Chroma path Module enable
    IFE_IFE_0_VFE_MODULE_STATS_EN                statsEnable;                        ///< Stats Module Enable
    IFE_IFE_0_VFE_STATS_CFG                      statsConfig;                        ///< Stats config
    IFE_IFE_0_VFE_DSP_TO_SEL                     dspConfig;                          ///< Dsp Config
    IFE_IFE_0_VFE_MODULE_DISP_EN                 frameProcessingDisplayModuleConfig;    ///< Frame processing Disp module enable
    IFE_IFE_0_VFE_DISP_OUT_Y_CROP_RND_CLAMP_CFG  displayFullLumaCropRoundClampConfig;   ///< Full Disp Luma path Module enable
    IFE_IFE_0_VFE_DISP_OUT_C_CROP_RND_CLAMP_CFG  displayFullChromaCropRoundClampConfig; ///< Full Disp Chroma path Module enable
    IFE_IFE_0_VFE_DISP_DS4_Y_CROP_RND_CLAMP_CFG  displayDS4LumaCropRoundClampConfig;    ///< DS4 Disp Luma path Module enable
    IFE_IFE_0_VFE_DISP_DS4_C_CROP_RND_CLAMP_CFG  displayDS4ChromaCropRoundClampConfig;  ///< DS4 Disp Chroma path Module enable
    IFE_IFE_0_VFE_DISP_DS16_Y_CROP_RND_CLAMP_CFG displayDS16LumaCropRoundClampConfig;   ///< DS16 Disp Luma path Module enable
    IFE_IFE_0_VFE_DISP_DS16_C_CROP_RND_CLAMP_CFG displayDS16ChromaCropRoundClampConfig; ///< DS16 Disp Chroma path Module enable
};

/// @brief MNDS state
struct MNDSState
{
    CropWindow         cropWindow;      ///< crop window for the client requested frame
    CropInfo           CAMIFCrop;       ///< Camif crop request from sensor
    UINT32             inputWidth;      ///< Camif output width in pixel
    UINT32             inputHeight;     ///< Camif output height in pixel
    StreamDimension    streamDimension; ///< FD path dimension
    IFEScalerOutput    MNDSOutput;      ///< MNDS output info fed to Crop module as input
    IFEModuleFlags     moduleFlags;     ///< MNDS module flags
};

/// @brief HVX state
struct HVXState
{
    UINT32             inputWidth;       ///< Input output width in pixel
    UINT32             inputHeight;      ///< Input output height in pixel
    CropInfo           cropWindow;       ///< Camif crop request
    StreamDimension    hvxOutDimension;  ///< HVX Out dimension
    FLOAT              scalingFactor;    ///< input to output scaling factor
    UINT32             overlap;          ///< OverLap
    UINT32             rightImageOffset; ///< Image Offset
};

/// @brief CROP state
struct CropState
{
    StreamDimension streamDimension;    ///< FD/Full/DS4/DS16 path dimension
    CropWindow      cropWindow;         ///< crop window for the requested frame
    CropWindow      modifiedCropWindow; ///< crop window for the requested frame
    IFEScalerOutput scalerOutput;       ///< Scaler output dimension and scale factor
    CHIRectangle    cropInfo;           ///< output path crop info to IPE
    CHIRectangle    appliedCropInfo;    ///< crop info IFE applied
};

/// @brief Round and Clamp state
struct RoundClampState
{
    StreamDimension FDDimension;            ///< RoundClamp FD path dimension
    StreamDimension fullDimension;          ///< RoundClamp (Video) Full path dimension
    StreamDimension DS4Dimension;           ///< RoundClamp (Video) DS4 path dimension
    StreamDimension DS16Dimension;          ///< RoundClamp (Video) DS16 path dimension
    StreamDimension displayFullDimension;   ///< RoundClamp Display Full path dimension
    StreamDimension displayDS4Dimension;    ///< RoundClamp Display DS4 path dimension
    StreamDimension displayDS16Dimension;   ///< RoundClamp Display DS16 path dimension
    IFEModuleFlags  moduleFlags;            ///< MNDS module flags
};

/// @brief DS4/16 state
struct DSState
{
    StreamDimension MNDSOutput;     ///< MNDS module output dimension
    StreamDimension DS4PathOutput;  ///< DS4 path output dimension
    StreamDimension DS16PathOutput; ///< DS16 path output dimension
    IFEModuleFlags  moduleFlags;    ///< DS4 1.0 module flags
};

/// @brief DS4 path PreCrop Luma(Y) and Chroma(Cr) crop params
struct DS4PreCropInfo
{
    CropInfo    YCrop;      ///< Crop info for Y
    CropInfo    CrCrop;     ///< Crop info for Cr
};

/// @brief ABF state
struct ABFState
{
    ABF34InputData dependenceData; ///< Dependence Data for this Module
};

/// @brief LSC state
struct LSCState
{
    LSC34InputData      dependenceData;  ///< Dependence Data for this Module
};

/// @brief BF state
struct BFStatsState
{
    UINT8 ROIIndexLUTBank;  ///< Index of ROI LUT bank currently selected
    UINT8 gammaLUTBank;     ///< Index of Gamma LUT bank currently selected
};

/// @ brief Structure to encapsulate IFE metadata
struct IFEMetadata
{
    IFECropInfo              cropInfo;                       ///< Remaining crop info for IPE to apply on IFE output
    IFECropInfo              appliedCropInfo;                ///< Crop info IFE applied
    ISPAWBBGStatsConfig      AWBBGStatsConfig;               ///< AWBBG Stats Config data for Parser Consumption
    ISPBFStatsConfig         BFStats;                        ///< BF configuration info
    ISPBHistStatsConfig      BHistStatsConfig;               ///< BHist Stats Config data for Parser Consumption
    ISPCSStatsConfig         CSStats;                        ///< CS stats configuration for stats parser
    ISPHDRBEStatsConfig      HDRBEStatsConfig;               ///< HDRBE Stats Config data for Parser Consumption
    ISPHDRBHistStatsConfig   HDRBHistStatsConfig;            ///< HDR BHist Stats Config data for Parser Consumption
    ISPIHistStatsConfig      IHistStatsConfig;               ///< IHist Stats Config data for Parser Consumption
    ISPRSStatsConfig         RSStats;                        ///< RS stats configuration for stats parser
    ISPTintlessBGStatsConfig tintlessBGStats;                ///< TintlessBG stats configuration for stats parser
    UINT8                    colorCorrectionAberrationMode;  ///< color correction aberration mode
    UINT32                   edgeMode;                       ///< edge mode value
    UINT8                    noiseReductionMode;             ///< noise reduction mode value
};

struct ISPInternalData;

/// @brief Structure to encapsulate IFE frame-level inter module dependent data
struct ISPFrameInternalData
{
    ISPInternalData* pFrameData; ///< Full frame ISP data
};

/// @brief Structure to encapsulate Lens shading attributes
struct LensShadingInfo
{
    DimensionCap lensShadingMapSize;                                ///< Lens Shading map size
    FLOAT        lensShadingMap[TotalChannels * MESH_ROLLOFF_SIZE]; ///< Lens Shading Map per channel
    UINT8        lensShadingMapMode;                                ///< Lens Shading map mode
    UINT8        shadingMode;                                       ///< Shading mode
};

/// @brief Tonamap point
struct ISPTonemapPoint
{
    FLOAT point[2]; ///< Input & output coordinates (IN, OUT), normalized from 0.0 to 1.0
};

struct ISPTonemapCurves
{
    UINT8           tonemapMode;                        ///< Tonemap mode
    INT32           curvePoints;                        ///< Number of tonemap curve points passed by framework
    ISPTonemapPoint tonemapCurveBlue[MaxCurvePoints];   ///< Pointer to tonemap curve blue
    ISPTonemapPoint tonemapCurveGreen[MaxCurvePoints];  ///< Pointer to tonemap curve green
    ISPTonemapPoint tonemapCurveRed[MaxCurvePoints];    ///< Pointer to tonemap curve red
};

/// @ brief Structure to encapsulate IFE inter module dependent data
struct ISPInternalData
{
    FLOAT                   clamping;                                       ///< Clamping Value from Linearization Mode
    IFEScalerOutput         scalerOutput[MaxRoundClampPath];                ///< MNDS/DS4/DS16 output
    CropInfo                fullOutCrop;                                    ///< Full Luma post crop
    CropInfo                dispOutCrop;                                    ///< Display Luma post crop
    CropInfo                fdOutCrop;                                      ///< FD Luma post crop
    BIT                     isDS4Enable;                                    ///< Bool to check if DS4 path is enabled
    BIT                     isDisplayDS4Enable;                             ///< Bool to check if Display DS4 path is enabled
    IFEModuleEnableConfig   moduleEnable;                                   ///< IFE module enable register configuration
    IFEMetadata             metadata;                                       ///< IFE metadata
    UINT32                  blackLevelOffset;                               ///< Black level offset
    GammaInfo               gammaOutput;                                    ///< IFE / BPS Gamma output table
    FLOAT                   stretchGainRed;                                 ///< Stretch Gain Red
    FLOAT                   stretchGainGreenEven;                           ///< Stretch Gain Green Even
    FLOAT                   stretchGainGreenOdd;                            ///< Stretch Gain Green Odd
    FLOAT                   stretchGainBlue;                                ///< Stretch Gain Blue
    FLOAT                   dynamicBlackLevel[ISPChannelMax];               ///< Dynamic Black Level
    LensShadingInfo         lensShadingInfo;                                ///< Lens Shading attributes
    DS4PreCropInfo          preCropInfo;                                    ///< DS4 path PreCrop module
    DS4PreCropInfo          preCropInfoDS16;                                ///< DS16 path PreCrop module
    DS4PreCropInfo          dispPreCropInfo;                                ///< DS4 display path PreCrop module
    DS4PreCropInfo          dispPreCropInfoDS16;                            ///< DS16 display path PreCrop module
    BOOL                    scalerDependencyChanged[MaxRoundClampPath];     ///< TRUE if full frame scaler dependency changed
    BOOL                    cropDependencyChanged[MaxRoundClampPath];       ///< TRUE if full frame scaler dependency changed
    Rational                CCTransformMatrix[3][3];                        ///< Color Correction transformation matrix
    UINT8                   colorCorrectionMode;                            ///< color correction mode
    ColorCorrectionGain     colorCorrectionGains;                           ///< color correction gains applied by WB
    UINT8                   blackLevelLock;                                 ///< Calculated black level lock
    UINT32                  linearizationAppliedBlackLevel[ISPChannelMax];  ///< Dynamic Black Level
    UINT32                  BLSblackLevelOffset;                            ///< Black level offset
    HotPixelModeValues      hotPixelMode;                                   ///< hot pixel mode
    INT32                   controlPostRawSensitivityBoost;                 ///< Applied isp gain
    UINT8                   noiseReductionMode;                             ///< Noise reduction mode
    ISPTonemapCurves        toneMapData;                                    ///< tone map mode, curve points and curve
    FLOAT                   percentageOfGTM;                                ///< gtmPercentage for adrc
    IPEGammaPreOutput       IPEGamma15PreCalculationOutput;                 ///< Pre-calculation output for IPE gamma15
};

/// @brief Instance Profile Id
enum IPEProfileId
{
    IPEProfileIdDefault = 0,    ///< All IQ Modules
    IPEProfileIdNPS,            ///< Noise Profile (ANR, TF, ICA1, ICA2)
    IPEProfileIdPPS,            ///< Post Processing Profile (All IQ blocks expect ANR, TF, ICA1, ICA2)
    IPEProfileIdScale,          ///< ScaleProfile (None)
    IPEProfileIdNoZoomCrop,     ///< All IQ Modules except fill zoom window
    IPEProfileIdHDR10   = 5,    ///< All IQ Modules process in HDR10 mode
    IPEProfileIdMax             ///< Invalid
};

/// @brief Instance Stabilization Id
enum IPEStabilizationType
{
    IPEStabilizationTypeNone     = 0,   ///< No Stabilization
    IPEStabilizationTypeEIS2     = 2,   ///< EIS2.0
    IPEStabilizationTypeEIS3     = 4,   ///< EIS3.0
    IPEStabilizationMCTF         = 8,   ///< Motion Controlled alignment
    IPEStabilizationTypeSWEIS2   = 16,  ///< SW EIS2.0
    IPEStabilizationTypeSWEIS3   = 32,  ///< SW EIS3.0
    IPEStabilizationTypeMax      = 64,  ///< Invalid/max
};

/// @brief Instance Processing Id
enum IPEProcessingType
{
    IPEProcessingTypeDefault = 0,   ///< No Processing Type
    IPEMFNRPrefilter,               ///< Prefilter
    IPEMFNRBlend,                   ///< Blend
    IPEMFNRScale,                   ///< Scale
    IPEMFNRPostfilter,              ///< Postfilter
    IPEProcessingPreview,           ///< Preview Processing type
    IPEMFSRPrefilter,               ///< Prefilter
    IPEMFSRBlend,                   ///< Blend
    IPEMFSRPostfilter,              ///< Postfilter
};

/// @brief IPE IQ Instance Property Information
struct IPEInstanceProperty
{
    UINT                    ipeOnlyDownscalerMode;                ///< IPE only downscaler enabled
    UINT                    ipeDownscalerWidth;                   ///< IPE only downscaler enabled width
    UINT                    ipeDownscalerHeight;                  ///< IPE only downscaler enabled height
    IPEProfileId            profileId;                            ///< NPS/PPS/Scale
    UINT32                  stabilizationType;                    ///< EIS2.0/ EIS3.0 / MCTF with EIS
    IPEProcessingType       processingType;                       ///< prefilter, blend, scale, postfilter
    BOOL                    enableCHICropInfoPropertyDependency;  ///< ensable CHI crop property dependency
    UINT                    enableFOVC;                           ///< FOVC Enable
};

struct IFEInstanceProperty
{
    UINT                    IFECSIDHeight;     ///< IFE Dynamic CSID Height
    UINT                    IFECSIDWidth;      ///< IFE Dynamic CSID Width
    UINT                    IFECSIDLeft;       ///< IFE Dynamic CSID Left
    UINT                    IFECSIDTop;        ///< IFE Dynamic CSID Top
    UINT                    IFEStatsSkipPattern; ///< IFE Stats Skip Pattern
    UINT                    IFESingleOn;       ///< IFE flag to force IFE to single IFE
};

/// @brief IPEPipelineData, IPE pipeline configuration data
struct IPEPipelineData
{
    const INT32*         pDeviceIndex;              ///< Pointer to ICP device index
    VOID*                pFrameProcessData;         ///< Pointer to firmware payload of type  IpeFrameProcessData
    VOID*                pIPEIQSettings;            ///< Pointer to IpeIQSettings
    VOID*                pWarpGeometryData;         ///< Warp geometry Data
    CmdBuffer**          ppIPECmdBuffer;            ///< Pointer to pointers of IPE command buffer managers
    UINT                 batchFrameNum;             ///< Number of frames in a batch
    UINT                 numPasses;                 ///< number of passess
    UINT8                hasTFRefInput;             ///< 1: TF has reference input; 0: TF has no reference input
    UINT8                isDigitalZoomEnabled;      ///< 1: digital zoom enable, 0: disable
    FLOAT                upscalingFactorMFSR;       ///< MFSR upscaling factor
    UINT32               configIOTopology;          ///< IPE configIO topology type
    UINT32               digitalZoomStartX;         ///< Digital zoom horizontal start offset
    UINT32               digitalZoomStartY;         ///< Digital zoom vertical start offset
    UINT32               instanceID;                ///< instance ID;
    UINT32               numOfFrames;               ///< Number of frames used in MFNR/MFSR case
    UINT32               numOutputRefPorts;         ///< Number of Output Reference Ports enabled for IPE
    UINT32               realtimeFlag;              ///< Flag for Realtime stream
    Format               fullInputFormat;           ///< Imageformat
    IPEInstanceProperty  instanceProperty;          ///< IPE Node Instance Property
    ImageDimensions      inputDimension;            ///< Input Dimension
    ImageDimensions      marginDimension;           ///< Margin Dimension
    BOOL                 compressiononOutput;       ///< Compression enabled on Output
    BOOL                 mctfEis;                   ///< Enable MCTF and EIS together
    BOOL                 isLowResolution;           ///< Check the reolution is low or Higher for R manula control
};

/// @brief Dual IFE stripe Id
enum DualIFEStripeId
{
    DualIFEStripeIdLeft = 0,    ///< Left stripe
    DualIFEStripeIdRight        ///< Right stripe
};

/// @brief Dual IFE impact for an IQ module
struct IQModuleDualIFEData
{
    BOOL dualIFESensitive;      ///< Indicates if the IQ module is affected by dual IFE mode
    BOOL dualIFEDMI32Sensitive; ///< Indicates if the left and right stripes needs separate DMI32 tables
    BOOL dualIFEDMI64Sensitive; ///< Indicates if the left and right stripes needs separate DMI64 tables
};

/// @brief IFEPipelineData
struct IFEPipelineData
{
    BOOL             programCAMIF;       ///< Set to True, if need to program CAMIF
    IFEModuleMode    moduleMode;         ///< IFE IQ Module Operation Mode Single IFE/ Dual IFE
    UINT             reserved;           ///< IFE pipeline configuration data
    IFEPipelinePath  IFEPath;            ///< IFE pipeline path
    UINT             numBatchedFrames;   ///< Number of Frames in the batch mode, otherwise it is 1
};

/// @brief IFEPipelineData
struct IFEDualPDPipelineData
{
    BOOL             programCAMIFLite;   ///< Set to True, if need to program CAMIF
    UINT             numBatchedFrames;   ///< Number of Frames in the batch mode, otherwise it is 1
};

/// @brief BPSPipelineData, BPS pipeline configuration data
struct BPSPipelineData
{
    VOID*           pIQSettings;     ///< Pointer to FW BPS IQ settings structure
    CmdBuffer**     ppBPSCmdBuffer;  ///< Pointer to pointers of BPS command buffer managers
    const INT32*    pDeviceIndex;    ///< Pointer to ICP device index
    UINT32          width;           ///< Stream width in pixels
    UINT32          height;          ///< Stream Height in pixels
    UINT32*         pBPSPathEnabled; ///< Active BPS input/output info.
};

/// @brief Indicates how the exessive crop window should be adjusted to match output size
enum CropType
{
    CropTypeCentered = 0,   ///< Crop equally from left and right
    CropTypeFromLeft,       ///< Crop from left
    CropTypeFromRight       ///< Crop from right
};

/// @brief ISPAlgoStatsData, Contains the stats data required by ISP algorithms
struct ISPAlgoStatsData
{
    ParsedBHistStatsOutput*      pParsedBHISTStats;          ///< Parsed BHIST stats
    ParsedTintlessBGStatsOutput* pParsedTintlessBGStats;     ///< Parsed tintless BG stats
    ISPTintlessBGStatsConfig     tintlessBGConfig;           ///< TintlessBG config
    ISPBHistStatsConfig          BHistConfig;                ///< BHIST config
};

/// @brief ISP input configuration per stripe
struct ISPStripeConfig
{
    CropInfo            CAMIFCrop;                              ///< CAMIF crop request from sensor
    CropWindow          HALCrop[MaxRoundClampPath];             ///< Crop region from the client per stripe
    StreamDimension     stream[MaxRoundClampPath];              ///< Stream dimensions per stripe
    AECFrameControl     AECUpdateData;                          ///< AEC_Update_Data
    AECStatsControl     AECStatsUpdateData;                     ///< AEC stats module config data
    AWBFrameControl     AWBUpdateData;                          ///< AWB_Update_Data
    AWBStatsControl     AWBStatsUpdateData;                     ///< AWB stats Update Data
    AFFrameControl      AFUpdateData;                           ///< AF_Update_Data
    AFStatsControl      AFStatsUpdateData;                      ///< AF stats Update Data
    AFDStatsControl     AFDStatsUpdateData;                     ///< AFD_Update_Data
    CSStatsControl      CSStatsUpdateData;                      ///< CS Stats update data
    IHistStatsControl   IHistStatsUpdateData;                   ///< IHist stats module config data
    PDHwConfig          pdHwConfig;                             ///< PD HW Config Data
    HVXState            stateHVX;                               ///< HVX state
    MNDSState           stateMNDS[IFEMaxNonCommonPaths];        ///< MNDS state
    CropState           stateCrop[IFEMaxNonCommonPaths];        ///< Crop state
    RoundClampState     stateRoundClamp[IFEMaxNonCommonPaths];  ///< Round and clamp state
    DSState             stateDS[IFEMaxNonCommonPaths];          ///< DS4/16 state
    ABFState            stateABF;                               ///< ABF state
    LSCState            stateLSC;                               ///< LSC state
    BFStatsState        stateBF;                                ///< BF stats persistent data over multiple frames
    UINT32              stripeId;                               ///< Zero-indexed stripe identifier
    CropType            cropType;                               ///< Crop type
    StreamDimension     stats[MaxStatsPorts];                   ///< Stats output dimensions per stripe
    ISPAlgoStatsData    statsDataForISP;                        ///< Stats data for ISP
    /// @note This will point to a common area where all IFE stripes see the same data. Only dual-insensitive IQ modules
    ///       may write into it, but all module may read from it.
    ISPFrameInternalData*   pFrameLevelData;                    ///< Frame-level internal ISP data
    IFEStripeOutput*        pStripeOutput;                      ///< Stripe config from striping lib
    BOOL                    overwriteStripes;                   ///< Overwrite module calculation using striping lib
    IFECAMIFInfo            CAMIFSubsampleInfo;                 ///< Camif subsample Info for Pdaf
};

/// @brief List of framework tags
static UINT32 ISPMetadataTags[] =
{
    BlackLevelLock                  ,
    ColorCorrectionAberrationMode   ,
    ColorCorrectionGains            ,
    ColorCorrectionMode             ,
    ColorCorrectionTransform        ,
    ControlAEMode                   ,
    ControlAWBMode                  ,
    ControlMode                     ,
    ControlPostRawSensitivityBoost  ,
    NoiseReductionMode              ,
    ShadingMode                     ,
    StatisticsHotPixelMapMode       ,
    StatisticsLensShadingMapMode    ,
    TonemapMode                     ,
    TonemapCurveBlue                ,
    TonemapCurveGreen               ,
    TonemapCurveRed                 ,
    ScalerCropRegion                ,
};



static const UINT NumISPMetadataTags = sizeof(ISPMetadataTags)/sizeof(UINT32);   ///< Number of vendor tags

static const UINT8 ISPMetadataTagReqOffset[NumISPMetadataTags] = { 0 };

/// @brief Framework tags requiring fer frame update by ISP
static const UINT ISPMetadataOutputTags[] =
{
    BlackLevelLock,
    ColorCorrectionAberrationMode,
    ColorCorrectionGains,
    ColorCorrectionMode,
    ColorCorrectionTransform,
    ControlPostRawSensitivityBoost,
    NoiseReductionMode,
    ShadingMode,
    StatisticsHotPixelMapMode,
    StatisticsLensShadingMapMode,
    ScalerCropRegion,
    TonemapMode,
    TonemapCurveBlue,
    TonemapCurveGreen,
    TonemapCurveRed,
};

static const UINT NumISPMetadataOutputTags = sizeof(ISPMetadataOutputTags) / sizeof(UINT32);   ///< Number of output vendor tags


/// @brief Color correction transform matrix
struct ISPColorCorrectionTransform
{
    CamxRational transformMatrix[ColorCorrectionRows][ColorCorrectionColumns];  ///< Color correction transform matrix
};

/// @brief Color correction gains
struct ISPColorCorrectionGains
{
    FLOAT gains[ISPColorCorrectionGainsMax];    ///< RGbGrB gains
};

/// @brief ISP tags
struct ISPHALTagsData
{
    UINT8                       blackLevelLock;                 ///< black level lock value
    UINT8                       colorCorrectionAberrationMode;  ///< color correction aberration mode
    ColorCorrectionGain         colorCorrectionGains;           ///< color correction gains
    UINT8                       colorCorrectionMode;            ///< color correction mode
    ISPColorCorrectionTransform colorCorrectionTransform;       ///< color correction transform matrix
    UINT8                       controlAEMode;                  ///< AE mode
    UINT8                       controlAWBMode;                 ///< AWB mode
    UINT8                       controlAWBLock;                 ///< AWB lock
    UINT8                       controlAECLock;                 ///< AEC lock
    UINT8                       controlMode;                    ///< main control mode switch
    INT32                       controlPostRawSensitivityBoost; ///< Raw sensitivity boost control
    HotPixelModeValues          hotPixelMode;                   ///< hot pixel mode
    UINT8                       noiseReductionMode;             ///< noise reduction mode
    UINT8                       shadingMode;                    ///< shading mode
    UINT8                       statisticsHotPixelMapMode;      ///< hot pixel map mode
    UINT8                       statisticsLensShadingMapMode;   ///< lens shading map mode
    ISPTonemapCurves            tonemapCurves;                  ///< tonemap curves points
    CropWindow                  HALCrop;                        ///< HAL crop window for zoom
    INT32                       saturation;                     ///< saturation value
    UINT32                      edgeMode;                       ///< edge mode value
    UINT8                       controlVideoStabilizationMode;  ///< EIS mode value
    FLOAT                       sharpness;                      ///< sharpness value
    UINT8                       contrastLevel;                  ///< contrast level value
    LtmConstrast                ltmContrastStrength;            ///< ltmContrastStrength value
};

/// @brief IFE IQ module enable
struct IFEIQModuleEnable
{
    BIT pedestal;   ///< Pedestal enable bit
    BIT rolloff;    ///< rolloff enable bit
    BIT BAF;        ///< BAF enable bit
    BIT BGTintless; ///< BGTintless enable bit
    BIT BGAWB;      ///< BGAWB enable bit
    BIT BE;         ///< BE enable bit
    BIT PDAF;       ///< PDAF enable bit
    BIT HDR;        ///< HDR enable bit
    BIT BPC;        ///< BPC enable bit
    BIT ABF;        ///< ABF enable bit
};

/// @brief Striping input
struct IFEStripingInput
{
    IFEIQModuleEnable       enableBits;     ///< Enable bits
    stripingInput_titanIFE  stripingInput;  ///< Striping library's input
};

/// @brief ISP Titan HW version
enum ISPHWTitanVersion
{
    ISPHwTitanInvalid = 0x0,       ///< Invalid Titan HW version
    ISPHwTitan170     = 0x1,       ///< Titan HW version for SDM845/SDM710/SDM670 only
    ISPHwTitan175     = 0x2,       ///< Titan HW version for SDM855 (SM8150) only
    ISPHwTitan150     = 0x4,       ///< Titan HW version for SDM640 (SM6150) only.
};

/// @brief ISPInputData
struct ISPInputData
{
    AECFrameControl*        pAECUpdateData;               ///< Pointer to the AEC_Update_Data
    AECStatsControl*        pAECStatsUpdateData;          ///< Pointer to the AEC stats Update Data
    AWBFrameControl*        pAWBUpdateData;               ///< Pointer to the AWB_Update_Data
    AWBStatsControl*        pAWBStatsUpdateData;          ///< Pointer to the AWB stats Update_Data
    AFFrameControl*         pAFUpdateData;                ///< Pointer to the AF_Update_Data
    AFStatsControl*         pAFStatsUpdateData;           ///< Pointer to the AF stats Update_Data
    AFDStatsControl*        pAFDStatsUpdateData;          ///< Pointer to AFD stats update data
    CSStatsControl*         pCSStatsUpdateData;           ///< Pointer to CS stats update data
    IHistStatsControl*      pIHistStatsUpdateData;        ///< Pointer to IHist stat config update data
    PDHwConfig*             pPDHwConfig;                  ///< Pointer to PD HW Config Data
    CmdBuffer*              pCmdBuffer;                   ///< Pointer to the Command Buffer Object
    CmdBuffer*              p32bitDMIBuffer;              ///< Pointer to the 32 bit DMI Buffer Manager
    UINT32*                 p32bitDMIBufferAddr;          ///< Pointer to the 32 bit DMI Buffer
    CmdBuffer*              p64bitDMIBuffer;              ///< Pointer to the 64 bit DMI Buffer Manager
    UINT32*                 p64bitDMIBufferAddr;          ///< Pointer to the 64 bit DMI Buffer
    CmdBuffer*              pDMICmdBuffer;                ///< Pointer to the CDM packets of LUT DMI buffers
    UINT32                  sensorID;                     ///< Sensor ID Number
    ISPSensorConfigureData  sensorData;                   ///< Configuration Data from Sensor Module
    ISPHALConfigureData     HALData;                      ///< Configuration Data from HAL/App
    ISPHALTagsData*         pHALTagsData;                 ///< HAL framework tag
    ISPStripeConfig*        pStripeConfig;                ///< Points to ISP input configuration
    ISPInternalData*        pCalculatedData;              ///< Data Calculated by IQ Modules
    IFEPipelineData         pipelineIFEData;              ///< IFE pipeline settings
    IFEDualPDPipelineData   dualPDPipelineData;           ///< DualPD pipeline data value
    IPEPipelineData         pipelineIPEData;              ///< Data Needed by IPE IQ Modules
    BPSPipelineData         pipelineBPSData;              ///< Data Needed by BPS IQ Modules
    TuningDataManager*      pTuningDataManager;           ///< Tuning data manager
    HwContext*              pHwContext;                   ///< Pointer to the Hardware Context
    UINT64                  frameNum;                     ///< the unique frame number for this capture request
    UINT                    mfFrameNum;                   ///< frame number for MFNR/MFSR blending
    VOID*                   pOEMIQSetting;                ///< Pointer to the OEM IQ Interpolation Data
    VOID*                   pLibInitialData;              ///< Customtized Library initial data block
    IFEStripingInput*       pStripingInput;               ///< Striping library's input
    IFETuningMetadata*      pIFETuningMetadata;           ///< Metadata for IFE tuning support
    BPSTuningMetadata*      pBPSTuningMetadata;           ///< Metadata for BPS tuning support
    IPETuningMetadata*      pIPETuningMetadata;           ///< Metadata for IPE tuning support
    ChiTuningModeParameter* pTuningData;                  ///< pointer to tuning data selectors
    BOOL                    tuningModeChanged;            ///< TRUE: tuning mode parameter(s) changed; FALSE: Otherwise
    const EEPROMOTPData*    pOTPData;                     ///< OTP Calibration Data
    UINT16                  numberOfLED;                  ///< Number of LED
    ISPIQTriggerData        triggerData;                  ///< Interpolation library Trigger Data
    BOOL                    disableManual3ACCM;           ///< Override to disable manual 3A CCM
    FLOAT                   LEDFirstEntryRatio;           ///< First Entry Ratio for Dual LED
    FLOAT                   lensPosition;                 ///< lens position value
    FLOAT                   lensZoom;                     ///< lens zoom value
    FLOAT                   postScaleRatio;               ///< post scale ratio
    FLOAT                   preScaleRatio;                ///< pre Scale Ratio
    ICAConfigParameters     ICAConfigData;                ///< ICA Inputdata
    UINT64                  parentNodeID;                 ///< whether it comes from IFE or BPS
    UINT                    opticalCenterX;               ///< Optical center X
    UINT                    opticalCenterY;               ///< Optical center Y
    UINT64                  minRequiredSingleIFEClock;    ///< Minimum IFE clock needed to process current mode
    UINT32                  sensorBitWidth;               ///< Sensor bit width
    UINT32                  resourcePolicy;               ///< Prefered resource vs power trade-off
    ISPHVXConfigureData     HVXData;                      ///< HVX configuration data
    struct FDData           fDData;                       ///< FD Data
    BOOL                    isDualcamera;                 ///< Dualcamera check
    BOOL                    skipTintlessProcessing;       ///< Skip Tintless Algo Processing
    BOOL                    useHardcodedRegisterValues;   ///< TRUE: Use hardcoded reg. values; FALSE: Use calculate settings
    BOOL                    enableIFEDualStripeLog;       ///< TRUE: Enable Dual IFE Stripe In/Out info
    UINT32                  dumpRegConfig;                ///< Dump IQ
    UINT32                  requestQueueDepth;            ///< Depth of the request queue
    VOID*                   pBetDMIAddr;                  ///< BET DNI Debug address
    BOOL                    registerBETEn;                ///< BET enabled
    BOOL                    MFNRBET;                      ///< BET enabled
    UINT32                  forceIFEflag;                 ///< Flag to force IFE to single IFE
    const IFEOutputPath*    pIFEOutputPathInfo;           ///< Pointer to IFEOutputPath data of array in IFE node
    UINT32                  maxOutputWidthFD;             ///< Max Supported FD Output Width
    BOOL                    isHDR10On;                    ///< Flag for HDR10
    UINT32                  titanVersion;                 ///< titan version
    UINT                    maximumPipelineDelay;         ///< Maximum Pipeline delay
    BOOL                    forceTriggerUpdate;           ///< Force Trigger Update
    CmdBufferManager**      ppCmdBufferManager;           ///< Pointer to the pointer of Ommand Buffer Manager
    BOOL                    tintlessEnable;               ///< Tintless enable flag
};

/// @brief IQ Library specific data
struct IQLibInitialData
{
    BOOL                    isSucceed;      ///< Indicator if the initialization is succeed
    VOID*                   pLibData;       ///< Pointer to the library data
    ChiTuningModeParameter* pTuningData;    ///< pointer to tuning data selectors
};

/// @brief IFEModuleCreateData
struct IFEModuleCreateData
{
    ISPIQModule*    pModule;               ///< Pointer to the created IQ Module
    IFEPipelineData pipelineData;          ///< VFE pipeline setting
    ISPInputData    initializationData;    ///< default values to configure the modules
    IFEHVXInfo*     pHvxInitializeData;     ///< Values from CHI about algo information.
};

/// @brief IPEModuleCreateData
struct IPEModuleCreateData
{
    ISPIQModule*            pModule;                ///< Pointer to the created IQ Module
    ISPInputData            initializationData;     ///< Default values to configure the modules
    IPEPath                 path;                   ///< IPE path information
    const CHAR*             pNodeIdentifier;        ///< String identifier for the Node that creating this IQ Module object
};

/// @brief BPSModuleCreateData
struct BPSModuleCreateData
{
    ISPIQModule*    pModule;            ///< Pointer to the created IQ Module
    ISPInputData    initializationData; ///< Default values to configure the modules
    const CHAR*     pNodeIdentifier;    ///< String identifier for the Node that creating this IQ Module object
};

struct IPEIQModuleData
{
    INT  IPEPath;                    ///< IPE Path information
    UINT offsetPass[PASS_NAME_MAX];  ///< Offset where pass information starts for multipass used by ANR
    UINT singlePassCmdLength;        ///< The length of the Command List, in bytes used by ANR
};

struct IQModuleCmdBufferParam
{
    CmdBufferManagerParam*  pCmdBufManagerParam;    ///< Pointer to the Cmd Buffer Manager params to be filled by IQ Modules
    UINT32                  numberOfCmdBufManagers; ///< Number Of Command Buffers created to be filled by IQ modules
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Base Class for all the ISP IQModule
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Execute
    ///
    /// @brief  Execute process capture request to configure individual image quality module
    ///
    /// @param  pInputData Pointer to the Inputdata
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Execute(
        ISPInputData* pInputData) = 0;

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
        ISPInputData* pInputData)
    {
        CAMX_UNREFERENCED_PARAM(pInputData);
        return CamxResultSuccess;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize parameters
    ///
    /// @param  hDevice  Device handle with which module LUT memory shall be associated
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize(
        CSLDeviceHandle hDevice)
    {
        CAMX_UNREFERENCED_PARAM(hDevice);
        return CamxResultENotImplemented;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize parameters
    ///
    /// @param  pInputData Pointer to the Inputdata
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NOWHINE CP028: Method overloaded
    virtual CamxResult Initialize(
        const ISPInputData* pInputData)
    {
        CAMX_UNREFERENCED_PARAM(pInputData);
        return CamxResultENotImplemented;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillCmdBufferManagerParams
    ///
    /// @brief  Fills the command buffer manager params needed by IQ Module
    ///
    /// @param  pInputData Pointer to the IQ Module Input data structure
    /// @param  pParam     Pointer to the structure containing the command buffer manager param to be filled by IQ Module
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult FillCmdBufferManagerParams(
       const ISPInputData*     pInputData,
       IQModuleCmdBufferParam* pParam)
    {
        CAMX_UNREFERENCED_PARAM(pInputData);
        CAMX_UNREFERENCED_PARAM(pParam);
        return CamxResultSuccess;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetRegCmd
    ///
    /// @brief  Retrieve the buffer of the register value
    ///
    /// @return Pointer of the register buffer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID* GetRegCmd()
    {
        return NULL;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetDualIFEData
    ///
    /// @brief  Provides information on how dual IFE mode affects the IQ module
    ///
    /// @param  pDualIFEData Pointer to dual IFE data the module will fill in
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID GetDualIFEData(
        IQModuleDualIFEData* pDualIFEData)
    {
        CAMX_ASSERT(NULL != pDualIFEData);

        pDualIFEData->dualIFESensitive      = FALSE;
        pDualIFEData->dualIFEDMI32Sensitive = FALSE;
        pDualIFEData->dualIFEDMI64Sensitive = FALSE;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize parameters
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NOWHINE CP028: Method overloaded
    virtual CamxResult Initialize()
    {
        return CamxResultENotImplemented;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetIQCmdLength
    ///
    /// @brief  Get the Command Size
    ///
    /// @return Length of the command list
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT GetIQCmdLength()
    {
        return m_cmdLength;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Get32bitDMILength
    ///
    /// @brief  Get the 32 bit DMI Size
    ///
    /// @return Length of the DMI list
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT32 Get32bitDMILength()
    {
        return m_32bitDMILength;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Set32bitDMIBufferOffset
    ///
    /// @brief  Set Offset in 32bit DMI buffer
    ///
    /// @param  offset The 64-bit DMI buffer offset.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID Set32bitDMIBufferOffset(
        UINT offset)
    {
        m_32bitDMIBufferOffsetDword = offset;

        return;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Get64bitDMILength
    ///
    /// @brief  Get the 64 bit DMI Size
    ///
    /// @return Length of the DMI list
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT32 Get64bitDMILength()
    {
        return m_64bitDMILength;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Set64bitDMIBufferOffset
    ///
    /// @brief  Set Offset in 64bit DMI buffer
    ///
    /// @param  offset The 64-bit DMI buffer offset.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID Set64bitDMIBufferOffset(
        UINT offset)
    {
        m_64bitDMIBufferOffsetDword = offset;

        return;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetNumLUT
    ///
    /// @brief  Get the number of Look Up Tables in IQ block
    ///
    /// @return Number of tables
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT GetNumLUT()
    {
        return m_numLUT;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetLUTOffset
    ///
    /// @brief  Get the offset where DMI CDM command for IQ module is present within DMI header cmd buffer
    ///
    /// @return Offset of LUT DMI header
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT GetLUTOffset()
    {
        return m_offsetLUT;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetIQType
    ///
    /// @brief  Get the Type of this Module
    ///
    /// @return module type
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE ISPIQModuleType GetIQType()
    {
        return m_type;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetModuleData
    ///
    /// @brief  Get IQ module specific data
    ///
    /// @param  pModuleData    Pointer pointing to Module specific data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CAMX_INLINE VOID GetModuleData(
        VOID* pModuleData)
    {
        CAMX_UNREFERENCED_PARAM(pModuleData);
        return;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Destroy
    ///
    /// @brief  Destroy the object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID Destroy()
    {
        CAMX_DELETE this;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsTuningModeDataChanged
    ///
    /// @brief  Determine if there is/are change(s) between the current and previously sets fo tuning mode/selector data
    ///
    /// @param  pCurrTuningModeParams   Pointer to current  tuning mode data
    /// @param  pPrevTuningModeParams   Pointer to previous tuning mode data
    ///
    /// @return BOOL    TRUE: Changed, FALSE: Otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CAMX_INLINE BOOL IsTuningModeDataChanged(
        const ChiTuningModeParameter* pCurrTuningModeParams,
        const ChiTuningModeParameter* pPrevTuningModeParams)
    {
        CAMX_STATIC_ASSERT_MESSAGE((static_cast<UINT32>(ChiModeType::Effect) == (MaxTuningMode - 1)),
            "Tuning Mode Structured Changed");

        BOOL tuningModeChanged = FALSE;

        if (NULL != pCurrTuningModeParams)
        {
            if (NULL != pPrevTuningModeParams)
            {
                tuningModeChanged = (0 != Utils::Memcmp(pCurrTuningModeParams,
                                                        pPrevTuningModeParams,
                                                        sizeof(ChiTuningModeParameter)));
            }
            // Always treat the initial mode as changed
            else
            {
                tuningModeChanged = TRUE;
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupISP, "Invalid pointer to current tuning mode parameters (%p)", pCurrTuningModeParams);
        }

        return tuningModeChanged;
    }

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~ISPIQModule
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~ISPIQModule() = default;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ISPIQModule
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ISPIQModule() = default;

    ISPIQModuleType m_type;                         ///< IQ Module Type
    BOOL            m_moduleEnable;                 ///< Flag to indicated if this module is enabled
    BOOL            m_dsBPCEnable;                  ///< Flag to indicated if DSBPC module is enabled
    UINT            m_cmdLength;                    ///< The length of the Command List, in dwords
    UINT            m_32bitDMILength;               ///< The length of the 32 bit DMI Table, in dwords
    UINT            m_32bitDMIBufferOffsetDword;    ///< Offset to the 32bit DMI buffer, in Dword
    UINT            m_64bitDMILength;               ///< The length of the 64 bit DMI Table, in Dword
    UINT            m_64bitDMIBufferOffsetDword;    ///< Offset to the 64bit DMI buffer, in Dword
    UINT            m_numLUT;                       ///< The number of look up tables
    UINT            m_offsetLUT;                    ///< Offset where DMI header starts for LUTs

private:
    ISPIQModule(const ISPIQModule&)            = delete;   ///< Disallow the copy constructor
    ISPIQModule& operator=(const ISPIQModule&) = delete;   ///< Disallow assignment operator
};

CAMX_NAMESPACE_END

#endif // CAMXISPIQMODULE_H
