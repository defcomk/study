////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxtitan17xcontext.cpp
/// @brief Titan17xContext class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// CamX Includes
#include "camxhal3defs.h"
#include "camxmem.h"
#include "camxvendortags.h"
#include "camxhwenvironment.h"

// HWL Includes
#include "chifdproperty.h"
#include "camxjpegquanttable.h"
#include "camxtitan17xcontext.h"
#include "camxtitan17xdefs.h"
#include "camxtitan17xfactory.h"
#include "camxtitan17xstatsparser.h"
#include "chiiqmodulesettings.h"
#include "chistatsproperty.h"
#include "camxchicomponent.h"
#include "chituningmodeparam.h"
#include "camximageformatutils.h"

CAMX_NAMESPACE_BEGIN

extern struct ComponentVendorTagsInfo  g_componentVendorTagsInfo;

/// @todo (CAMX-961) - Consider taking all these supported static metadata and resolution sizes from settings.
///                  - This would prevent the need for OEMs to change code to add a resolution for below tables.
static const CameraMetadataTag SupportedRequestKeys[] =
{
    ColorCorrectionMode,
    ColorCorrectionTransform,
    ColorCorrectionGains,
    ColorCorrectionAberrationMode,
    ControlAEAntibandingMode,
    ControlAEExposureCompensation,
    ControlAELock,
    ControlAEMode,
    ControlAERegions,
    ControlAETargetFpsRange,
    ControlAEPrecaptureTrigger,
    ControlAFMode,
    ControlAFRegions,
    ControlAFTrigger,
    ControlAWBLock,
    ControlAWBMode,
    ControlCaptureIntent,
    ControlEffectMode,
    ControlMode,
    ControlSceneMode,
    ControlVideoStabilizationMode,
    ControlPostRawSensitivityBoost,
    ControlZslEnable,
    EdgeMode,
    FlashMode,
    HotPixelMode,
    JPEGGpsCoordinates,
    JPEGGpsProcessingMethod,
    JPEGGpsTimestamp,
    JPEGOrientation,
    JPEGQuality,
    JPEGThumbnailQuality,
    JPEGThumbnailSize,
    LensAperture,
    LensFilterDensity,
    LensFocalLength,
    LensFocusDistance,
    LensOpticalStabilizationMode,
    NoiseReductionMode,
    RequestId,
    ScalerCropRegion,
    SensorExposureTime,
    SensorFrameDuration,
    SensorSensitivity,
    SensorTimestamp,
    SensorDynamicBlackLevel,
    SensorDynamicWhiteLevel,
    ShadingMode,
    StatisticsFaceDetectMode,
    StatisticsHotPixelMapMode,
    StatisticsLensShadingMapMode,
    TonemapCurveBlue,
    TonemapCurveGreen,
    TonemapCurveRed,
    TonemapMode,
    BlackLevelLock
    /// @todo (CAMX-961) This list is not final, can contain more entries
};

static const CameraMetadataTag SupportedResultKeys[] =
{
    ColorCorrectionMode,
    ColorCorrectionTransform,
    ColorCorrectionGains,
    ControlAEMode,
    ControlAERegions,
    ControlAEState,
    ControlAFMode,
    ControlAFRegions,
    ControlAFState,
    ControlAWBMode,
    ControlAWBState,
    ControlMode,
    ControlPostRawSensitivityBoost,
    ControlZslEnable,
    EdgeMode,
    FlashMode,
    FlashState,
    JPEGGpsCoordinates,
    JPEGGpsProcessingMethod,
    JPEGGpsTimestamp,
    JPEGOrientation,
    JPEGQuality,
    JPEGThumbnailQuality,
    JPEGThumbnailSize,
    LensAperture,
    LensFilterDensity,
    LensFocalLength,
    LensFocusDistance,
    LensFocusRange,
    LensState,
    LensOpticalStabilizationMode,
    NoiseReductionMode,
    RequestId,
    ScalerCropRegion,
    SensorGreenSplit,
    SensorNoiseProfile,
    SensorExposureTime,
    SensorFrameDuration,
    SensorSensitivity,
    SensorTimestamp,
    SensorDynamicBlackLevel,
    SensorDynamicWhiteLevel,
    SensorNeutralColorPoint,
    SensorProfileToneCurve,
    ShadingMode,
    StatisticsFaceDetectMode,
    StatisticsSharpnessMap,
    StatisticsPredictedColorGains,
    StatisticsPredictedColorTransform,
    StatisticsSceneFlicker,
    StatisticsFaceRectangles,
    StatisticsFaceScores,
    TonemapCurveBlue,
    TonemapCurveGreen,
    TonemapCurveRed,
    TonemapMode,
    BlackLevelLock
    /// @todo (CAMX-961) This list is not final, can contain more entries
};

static const CameraMetadataTag SupportedCharacteristicsKeys[] =
{
    ColorCorrectionAvailableAberrationModes,
    ControlAEAvailableAntibandingModes,
    ControlAEAvailableModes,
    ControlAEAvailableTargetFPSRanges,
    ControlAECompensationRange,
    ControlAECompensationStep,
    ControlAFAvailableModes,
    ControlAvailableEffects,
    ControlAvailableSceneModes,
    ControlAvailableVideoStabilizationModes,
    ControlAWBAvailableModes,
    ControlMaxRegions,
    ControlSceneModeOverrides,
    ControlAvailableHighSpeedVideoConfigurations,
    ControlAELockAvailable,
    ControlAWBLockAvailable,
    ControlAvailableModes,
    ControlPostRawSensitivityBoostRange,
    ControlZslEnable,
    DepthAvailableDepthMinFrameDurations,
    DepthAvailableDepthStreamConfigurations,
    DepthAvailableDepthStallDurations,
    EdgeAvailableEdgeModes,
    FlashInfoAvailable,
    HotPixelAvailableHotPixelModes,
    JPEGAvailableThumbnailSizes,
    JPEGMaxSize,
    LensInfoAvailableApertures,
    LensInfoAvailableFilterDensities,
    LensInfoAvailableFocalLengths,
    LensInfoAvailableOpticalStabilization,
    LensInfoHyperfocalDistance,
    LensInfoMinimumFocusDistance,
    LensInfoShadingMapSize,
    LensInfoFocusDistanceCalibration,
    LensFacing,
#if ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
    LogicalMultiCameraPhysicalIDs,
    LogicalMultiCameraSensorSyncType,
#endif // ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28))
    NoiseReductionAvailableNoiseReductionModes,
    RequestMaxNumOutputStreams,
    RequestMaxNumInputStreams,
    RequestPipelineMaxDepth,
    RequestPartialResultCount,
    RequestAvailableCapabilities,
    RequestAvailableRequestKeys,
    RequestAvailableResultKeys,
    RequestAvailableCharacteristicsKeys,
#if ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
    RequestAvailableSessionKeys,
#endif // ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28))
    ScalerAvailableMaxDigitalZoom,
    ScalerAvailableInputOutputFormatsMap,
    ScalerAvailableStreamConfigurations,
    ScalerAvailableMinFrameDurations,
    ScalerAvailableRawSizes,
    ScalerAvailableStallDurations,
    ScalerCroppingType,
    SensorInfoActiveArraySize,
    SensorGreenSplit,
    SensorNoiseProfile,
    SensorOpaqueRawSize,
    SensorInfoSensitivityRange,
    SensorInfoColorFilterArrangement,
    SensorInfoExposureTimeRange,
    SensorInfoMaxFrameDuration,
    SensorInfoPhysicalSize,
    SensorInfoPixelArraySize,
    SensorInfoWhiteLevel,
    SensorInfoTimestampSource,
    SensorInfoLensShadingApplied,
    SensorInfoPreCorrectionActiveArraySize,
    SensorReferenceIlluminant1,
    SensorReferenceIlluminant2,
    SensorCalibrationTransform1,
    SensorCalibrationTransform2,
    SensorColorTransform1,
    SensorColorTransform2,
    SensorForwardMatrix1,
    SensorForwardMatrix2,
    SensorBlackLevelPattern,
    SensorMaxAnalogSensitivity,
    SensorOrientation,
    SensorProfileHueSaturationMapDimensions,
    SensorAvailableTestPatternModes,
    ShadingAvailableModes,
    StatisticsInfoAvailableFaceDetectModes,
    StatisticsInfoMaxFaceCount,
    StatisticsInfoAvailableHotPixelMapModes,
    StatisticsInfoAvailableLensShadingMapModes,
    TonemapMaxCurvePoints,
    TonemapAvailableToneMapModes,
    InfoSupportedHardwareLevel,
    SyncMaxLatency,
    ReprocessMaxCaptureStall,
#if (defined(CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
    LensPoseReference,
    LensDistortion,
#endif // Android-P or better
    LensPoseTranslation,
    LensPoseRotation,
    LensIntrinsicCalibration
    /// @todo (CAMX-961) This list is not final, can contain more entries
};

#if ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
static const CameraMetadataTag SupportedSessionKeys[] =
{
    ControlAETargetFpsRange,
};
#endif // ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28))

// This is all the available scaler format
static const ScalerAvailableFormatsValues SupportedScalerFormats[] =
{
    ScalerAvailableFormatsImplementationDefined,
    ScalerAvailableFormatsYCbCr420888,
    ScalerAvailableFormatsBlob,
    ScalerAvailableFormatsRaw10,
    ScalerAvailableFormatsRaw16,
    ScalerAvailableFormatsRawOpaque
};

// This is all other available pixel format not list in SupportedScalerFormats
static const HALPixelFormat SupportedInternalPixelFormats[] =
{
    HALPixelFormatY8
};

static const DimensionCap SupportedThumbnailSizes[] =
{
    // This is the pre-defined table, and available thumbnail sizes capability
    // can be modified based on sensor capability. The sizes SHOULD be in the
    // ascending order of (width * height).
    { 0,   0 },
    { 176, 144 },
    { 240, 144 },
    { 256, 144 },
    { 240, 160 },
    { 256, 154 },
    { 246, 184 },
    { 240, 240 },
    { 320, 240 }
    /// @todo (CAMX-961) This list is not final, can contain more entries
};

static const INT32 SupportedInputOutputFormatMaps[] =
{
    // The mapping of image formats that are supported by this camera device for input streams,
    // to their corresponding output formats.
    // We support PRIVATE_REPROCESSING + YUV_REPROCESSING
    ScalerAvailableFormatsImplementationDefined, 2, ScalerAvailableFormatsYCbCr420888, ScalerAvailableFormatsBlob,
    ScalerAvailableFormatsYCbCr420888,           2, ScalerAvailableFormatsYCbCr420888, ScalerAvailableFormatsBlob
};

static const DimensionCap SupportedImageSizes[] =
{
    // This is the pre-defined table, and available image sizes capability
    // can be modified base on sensor capability
    { 8000, 6000 }, // 48 MP
    { 5344, 4008 }, // 21 MP
    { 5312, 2988 },
    { 5184, 3880 },
    { 4608, 3456 }, // 16MP
    { 4608, 2592 }, // 12MP
    { 4160, 3120 }, // 13MP
    { 4096, 2304 }, // 9.4 MP
    { 4096, 2160 }, // 4k DCI
    { 4000, 3000 },
    { 3840, 2160 },
    { 3264, 2448 }, // 8MP wide 289
    { 3200, 2400 }, // 8MP  290
    { 2976, 2976 }, // Square   291
    { 2688, 1512 }, // 4MP  292
    { 2592, 1944 }, // 5MP
    { 2048, 1536 }, // 3MP
    { 1920, 1440 },
    { 1920, 1080 }, // 1080p
    { 1600, 1200 }, // UXGA
    { 1440, 1080 }, // Wide HD
    { 1280, 960  }, // SXGA
    { 1280, 768  }, // 1MP
    { 1280, 720  }, // 720p
    { 1080, 1080 }, // square
    { 1024, 738  },
    { 1024, 768  }, // XGA
    { 864,  480  },
    { 800,  600  }, // SVGA
    { 800,  480  }, // WVGA
    { 720,  1280 }, // Portrait for VT
    { 720,  480  },
    { 640,  480  },
    { 640,  400  }, // XR 6DOF Mono
    { 640,  360  },
    { 352,  288  },
    { 320,  240  },
    { 240,  320  }, // Portrait for VT
    { 176,  144  },
    /// @todo (CAMX-961) This list is not final, can contain more entries
};

static const HFRConfigurationParams SupportedHFRVideoSizes[] =
{
    // This is the pre-defined table, and available HFR video sizes capability
    // can be modified base on sensor capability
    { 1920, 1080, 30,  120,  4  },
    { 1920, 1080, 120, 120,  4  },
    { 1920, 1080, 30,  240,  8 },
    { 1920, 1080, 240, 240,  8 },
    { 1280, 720,  30,  120,  4  },
    { 1280, 720,  120, 120,  4  },
    { 1280, 720,  30,  240,  8  },
    { 1280, 720,  240, 240,  8  },
    { 1280, 720,  30,  480,  16 },
    { 1280, 720,  480, 480,  16 },
    { 720,  480,  30,  120,  4  },
    { 720,  480,  120, 120,  4  },
    { 720,  480,  30,  240,  8  },
    { 720,  480,  240, 240,  8  },
    { 640,  480,  30,  120,  4  },
    { 640,  480,  120, 120,  4  },
    { 640,  480,  30,  240,  8  },
    { 640,  480,  240, 240,  8  },
    /// @todo (CAMX-961) This list is not final, can contain more entries
};

/// default Luma Qtable
static const UINT16 DefaultJPEGQuantTableLuma[QuantTableSize] =
{
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

/// default Chroma Qtable
static const UINT16 DefaultJPEGQuantTableChroma[QuantTableSize] =
{
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

static const RangeINT32 SupportedDefaultTargetFpsRanges[MaxTagValues] =
{
    {  7, 30 },
    { 15, 15 },
    { 30, 30 }
};

// This is all the available depth format
static const DepthAvailableFormatsValues SupportedDepthFormats[] =
{
    DepthAvailableFormatsRawDepth,
    DepthAvailableFormatsY16,
    DepthAvailableFormatsBlob,
    DepthAvailableFormatsImplementationDefined
};

static const SIZE_T NumSupportedRequestKeys           = sizeof(SupportedRequestKeys) / sizeof(CameraMetadataTag);
static const SIZE_T NumSupportedResultKeys            = sizeof(SupportedResultKeys) / sizeof(CameraMetadataTag);
static const SIZE_T NumSupportedCharacteristicsKeys   = sizeof(SupportedCharacteristicsKeys) / sizeof(CameraMetadataTag);
#if ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
static const SIZE_T NumSupportedSessionKeys           = sizeof(SupportedSessionKeys) / sizeof(CameraMetadataTag);
#endif // ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28))
static const SIZE_T NumSupportedScalerFormats         = sizeof(SupportedScalerFormats) / sizeof(ScalerAvailableFormatsValues);
static const SIZE_T NumSupportedInternalPixelFormats  = sizeof(SupportedInternalPixelFormats) / sizeof(HALPixelFormat);
static const SIZE_T NumSupportedThumbnailSizes        = sizeof(SupportedThumbnailSizes) / sizeof(DimensionCap);
static const SIZE_T NumSupportedInputOutputFormatMaps = sizeof(SupportedInputOutputFormatMaps) / sizeof(INT32);
static const SIZE_T NumSupportedImageSizes            = sizeof(SupportedImageSizes) / sizeof(DimensionCap);
static const SIZE_T NumSupportedHFRVideoSizes         = sizeof(SupportedHFRVideoSizes) / sizeof(HFRConfigurationParams);
static const SIZE_T NumSupportedDefaultFPSRanges      = sizeof(SupportedDefaultTargetFpsRanges) / sizeof(RangeINT32);
static const SIZE_T NumSupportedDepthFormats          = sizeof(SupportedDepthFormats) / sizeof(DepthAvailableFormatsValues);

/// @todo (CAMX-961) Change this pre-defined value base on JPEG encoder profiling
///                  For MaxRawStreams, mention as 0 for now, and need to recheck.
static const FLOAT  JPEGStallFactorPerPixel           = 6.5f;
/// @todo (CAMX-842) Update to hardware-supported value when digital zoom is supported
static const FLOAT  MaxDigitalZoom                    = 8.0f;
static const SIZE_T MaxRawStreams                     = 1;
static const SIZE_T MaxProcessedStreams               = 3;
static const SIZE_T MaxProcessedStallingStreams       = 2;

// Maximum capture stall. It must be non-null and no larger than 4.
// Required for reprocessing support
static const INT32  MaxCaptureStall                   = 2;

// Maximum downscale ratio of the pipeline.
// This is considering the IPE capability
static const INT32  MaxDownscaleRatio                 = 24;

// Maximum and Minimum Sharpness Value.
static const INT32  MinSharpness                      = 0;
static const INT32  MaxSharpness                      = 6;
static const INT32  TotalSharpnessLevels              = 6;
static const INT32  DefaultSharpness                  = 2;

// Histogram Buckets and Max Count.
static const INT32  HistogramBuckets                  = 256;
static const INT32  HistogramMaxCount                 = 256;

// Maximum and Minimum Color Temperature.
static const INT32  MinColorTemperature = 0;
static const INT32  MaxColorTemperature = 10000;

// Maximum and Minimum White Balance Gains.
static const FLOAT  MinWBGain = 1.0f;
static const FLOAT  MaxWBGain = 31.99f;

// Maximum number of input streams.
// Required for reprocessing support
static const INT32  MaxInputStream          = 1;
static const UINT8  MaxPipelineDepth        = 8;
static const INT32  PartialResultCount      = 2;
static const INT32  MaxFaceCount            = 10;

// Maximum pixel clocks of the HW blocks of Titan in Hz
static const INT32  MaxBPSPixelClockNom             = 600000000;
static const INT32  MaxSingleIPEPixelClockNom       = 600000000;
static const INT32  MaxSingleIPEPixelClockSvsL1     = 480000000;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFE clock levels - all units in Hz
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SDM6150
static const UINT64  MaxIFETitan150LowSVSClock      = 360000000;
static const UINT64  MaxIFETitan150SVSClock         = 432000000;
static const UINT64  MaxIFETitan150SVSL1Clock       = 540000000;
static const UINT64  MaxIFETitan150NOMClock         = 540000000;
static const UINT64  MaxIFETitan150TURBOClock       = 600000000;

// SDM670, SDM845
static const UINT64  MaxIFETitan170LowSVSClock      = 404000000; // Force to 404MHz meet HW limitations
static const UINT64  MaxIFETitan170SVSClock         = 404000000;
static const UINT64  MaxIFETitan170SVSL1Clock       = 480000000;
static const UINT64  MaxIFETitan170NOMClock         = 600000000;
static const UINT64  MaxIFETitan170TURBOClock       = 600000000;

// SDM8150 V1
static const UINT64  MaxIFETitan175V1LowSVSClock    = 400000000;
static const UINT64  MaxIFETitan175V1SVSClock       = 558000000;
static const UINT64  MaxIFETitan175V1SVSL1Clock     = 637000000;
static const UINT64  MaxIFETitan175V1NOMClock       = 760000000;
static const UINT64  MaxIFETitan175V1TURBOClock     = 760000000;

// SDM8150 V2
static const UINT64  MaxIFETitan175V2LowSVSClock    = 400000000;
static const UINT64  MaxIFETitan175V2SVSClock       = 558000000;
static const UINT64  MaxIFETitan175V2SVSL1Clock     = 637000000;
static const UINT64  MaxIFETitan175V2NOMClock       = 847000000;
static const UINT64  MaxIFETitan175V2TURBOClock     = 950000000;

// SDM7150
static const UINT64  MaxIFETitan175V3LowSVSClock    = 380000000;
static const UINT64  MaxIFETitan175V3SVSClock       = 510000000;
static const UINT64  MaxIFETitan175V3SVSL1Clock     = 637000000;
static const UINT64  MaxIFETitan175V3NOMClock       = 760000000;
static const UINT64  MaxIFETitan175V3TURBOClock     = 760000000;

// IPEICA Transform Grid Size for Titan
static const INT32  IPEICATitan150GridSize      = 825;
static const INT32  IPEICATitan170GridSize      = 825;
static const INT32  IPEICATitan175GridSize      = 945;

// Maximum number of batches supported
static const INT32 MaxBatchedFrames          = 8;

// Default HFR Preview FPS
static const UINT32 DefaultHFRPreviewFPS       = 60;

// Contrast Range
static const FLOAT  MinContrast                      = -100.0f;
static const FLOAT  MaxContrast                      = 100.0f;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Vendor tags supported by this platform, @todo(CAMX-1388), review the tag unit when add the real implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// @todo (CAMX-2877) - Most of these VTs do not belong in the HWL, as thay are HW independent.  Move them when starting the
///                     next HWL...do not duplicate these in another HWL.

/// org.quic.camera.debugdata section
static VendorTagData g_VendorTagSectionDebugData[] =
{
    { "DebugDataAll", VendorTagType::Byte, sizeof(DebugData) },
    { "DebugDataAEC", VendorTagType::Byte, sizeof(DebugData) },
    { "DebugDataAWB", VendorTagType::Byte, sizeof(DebugData) },
    { "DebugDataAF",  VendorTagType::Byte, sizeof(DebugData) }
};

/// org.quic.camera.tuningdata section
static VendorTagData g_VendorTagSectionTuningdataDump[] =
{
    { "TuningDataDump",  VendorTagType::Byte, sizeof(DebugData) }
};

/// org.codeaurora.qcamera3.saturation section
static VendorTagData g_VendorTagSectionSaturation[] =
{
    { "use_saturation", VendorTagType::Int32, 1 },
    { "range",          VendorTagType::Int32, 4 }
};

/// org.codeaurora.qcamera3.sharpness section
static VendorTagData g_VendorTagSectionSharpness[] =
{
    { "strength", VendorTagType::Int32, 1 },
    { "range",    VendorTagType::Int32, 2 }
};

/// org.codeaurora.qcamera3.contrast section
static VendorTagData g_VendorTagSectionContrast[] =
{
    { "level", VendorTagType::Int32, 1 },
};

/// org.codeaurora.qcamera3.exposure_metering section
static VendorTagData g_VendorTagSectionExposureMetering[] =
{
    { "exposure_metering_mode", VendorTagType::Int32, 1 },
    { "available_modes",        VendorTagType::Int32, 7 }
};

/// org.codeaurora.qcamera3.aec_convergence_speed section
static VendorTagData g_VendorTagSectionAecConvergenceSpeed[] =
{
    { "aec_speed", VendorTagType::Float, 1 },
};

/// org.codeaurora.qcamera3.iso_exp_priority section
static VendorTagData g_VendorTagSectionIsoExpPriority[] =
{
    { "use_iso_exp_priority",   VendorTagType::Int64, 1 },
    { "use_iso_value",          VendorTagType::Int32, 1 },
    { "select_priority",        VendorTagType::Int32, 1 },
    { "iso_available_modes",    VendorTagType::Int32, 8 },
    { "exposure_time_range",    VendorTagType::Int64, 2 },
    { "use_gain_value",         VendorTagType::Float, 1 },

};

/// org.codeaurora.qcamera3.histogram section
static VendorTagData g_VendorTagSectionHistogram[] =
{
    { "enable",      VendorTagType::Byte,  1 },
    { "buckets",     VendorTagType::Int32, 1 },
    { "max_count",   VendorTagType::Int32, 1 },
    { "stats",       VendorTagType::Int32, MaxBHistBinNum },
    { "stats_type",  VendorTagType::Int32, 1 }
};

/// org.codeaurora.qcamera3.bayer_grid section
static VendorTagData g_VendorTagSectionBGStats[] =
{
    { "enable",      VendorTagType::Byte,  1 },
    { "width",       VendorTagType::Int32, 1 },
    { "height",      VendorTagType::Int32, 1 },
    { "r_stats",     VendorTagType::Int32, MaxAWBBGStatsNum },
    { "g_stats",     VendorTagType::Int32, MaxAWBBGStatsNum },
    { "b_stats",     VendorTagType::Int32, MaxAWBBGStatsNum },
    { "stats_type",  VendorTagType::Int32, 1 }
};

/// org.codeaurora.qcamera3.bayer_exposure section
static VendorTagData g_VendorTagSectionBEStats[] =
{
    { "enable",      VendorTagType::Byte,  1 },
    { "width",       VendorTagType::Int32, 1 },
    { "height",      VendorTagType::Int32, 1 },
    { "r_stats",     VendorTagType::Int32, MaxAWBBGStatsNum },
    { "g_stats",     VendorTagType::Int32, MaxAWBBGStatsNum },
    { "b_stats",     VendorTagType::Int32, MaxAWBBGStatsNum },
    { "stats_type",  VendorTagType::Int32, 1 }
};

/// org.codeaurora.qcamera3.instant_aec section
static VendorTagData g_VendorTagSectionInstanctAec[] =
{
    { "instant_aec_mode",            VendorTagType::Int32, 1 },
    { "instant_aec_available_modes", VendorTagType::Int32, 3 }
};

/// org.codeaurora.qcamera3.awb_convergence_speed section
static VendorTagData g_VendorTagSectionAwbConvSpeed[] =
{
    { "awb_speed",            VendorTagType::Float, 1 }
};

/// org.codeaurora.qcamera3.ae_bracket section
static VendorTagData g_VendorTagSectionAEBracket[] =
{
    { "mode",            VendorTagType::Byte,  1 },
    { "num_frames",      VendorTagType::Int32, 1 },
    { "exposure_values", VendorTagType::Int32, 16 }
};

/// org.codeaurora.qcamera3.temporal_denoise section
static VendorTagData g_VendorTagSectionTemporalDenoise[] =
{
    { "enable",         VendorTagType::Byte,  1 },
    { "process_type",   VendorTagType::Int32, 4 }
};

/// @todo (CAMX-1388), Add org.codeaurora.qcamera3.stats section after snapdragon camera app fix
static VendorTagData g_VendorTagSectionStats[] =
{
    { "is_hdr_scene",                  VendorTagType::Byte, 1 },
//    { "is_hdr_scene_values",           VendorTagType::Byte },
    { "is_hdr_scene_confidence",       VendorTagType::Float, 1 },
//    { "is_hdr_scene_confidence_range", VendorTagType::Float },
//    { "bsgc_available",                VendorTagType::Byte },
//    { "blink_detected",                VendorTagType::Byte },
//    { "blink_degree",                  VendorTagType::Byte },
//    { "smile_degree",                  VendorTagType::Byte },
//    { "smile_confidence",              VendorTagType::Byte },
//    { "gaze_angle",                    VendorTagType::Byte },
//    { "gaze_direction",                VendorTagType::Int32 },
//    { "gaze_degree",                   VendorTagType::Byte }
};

/// org.codeaurora.qcamera3.manualWB section
static VendorTagData g_VendorTagSectionManualWB[] =
{
    { "color_temperature",            VendorTagType::Int32, 1 },
    { "color_temperature_range",      VendorTagType::Int32, 2 },
    { "gains",                        VendorTagType::Float, 3 },
    { "gains_range",                  VendorTagType::Float, 2 },
    { "partial_mwb_mode",             VendorTagType::Int32, 1 },
};

/// org.codeaurora.qcamera3.jpeg_encode_crop section
static VendorTagData g_VendorTagSectionJpegEncodeCrop[] =
{
    { "enable", VendorTagType::Byte,  1 },
    { "rect",   VendorTagType::Int32, 4 },
    { "roi",    VendorTagType::Int32, 4 }
};

/// org.codeaurora.qcamera3.sensor_meta_data section
static VendorTagData g_VendorTagSectionSensorMeta[] =
{
    { "dynamic_black_level_pattern", VendorTagType::Float, 1 },
    { "is_mono_only",                VendorTagType::Byte,  1 },
    { "sensor_mode_index",           VendorTagType::Int32, MaxSensorModes},
    { "EEPROMInformation",           VendorTagType::Byte,  sizeof(EEPROMInformation)},
    { "integration_information",     VendorTagType::Int32, 3},
    { "mountAngle",                  VendorTagType::Int32, 1},
    { "cameraPosition",              VendorTagType::Int32, 1},
    { "sensor_mode_info",            VendorTagType::Byte,  sizeof(ChiSensorModeInfo)},
    { "sensor_pdaf_info",            VendorTagType::Byte,  sizeof(ChiSensorPDAFInfo)},
    { "current_mode",                VendorTagType::Int32, 1},
    { "targetFPS",                   VendorTagType::Int32, 1}
};

/// org.codeaurora.qcamera3.av_timer
static VendorTagData g_VendorTagSectionAVTimer[] =
{
    { "use_av_timer", VendorTagType::Byte, 1 }
};

/// org.quic.camera2.statsconfigs section
static VendorTagData g_VendorTagSectionStatsConfigs[] =
{
    { "AECStatsControl",        VendorTagType::Byte,    sizeof(AECStatsControl)   },
    { "AWBStatsControl",        VendorTagType::Byte,    sizeof(AWBStatsControl)   },
    { "AFStatsControl",         VendorTagType::Byte,    sizeof(AFStatsControl)    },
    { "AFDStatsControl",        VendorTagType::Byte,    sizeof(AFDStatsControl)   },
    { "IHistStatsControl",      VendorTagType::Byte,    sizeof(IHistStatsControl) },
    { "AECFrameControl",        VendorTagType::Byte,    sizeof(AECFrameControl)   },
    { "AWBFrameControl",        VendorTagType::Byte,    sizeof(AWBFrameControl)   },
    { "AFFrameControl",         VendorTagType::Byte,    sizeof(AFFrameControl)    },
    { "DigitalGainControl",     VendorTagType::Byte,    sizeof(FLOAT)             },
    { "AWBWarmstartGain",       VendorTagType::Float,   3                         },
    { "AWBWarmstartCCT",        VendorTagType::Float,   1                         },
    { "AWBFrameControlRGain",   VendorTagType::Float,   1                         },
    { "AWBFrameControlGGain",   VendorTagType::Float,   1                         },
    { "AWBFrameControlBGain",   VendorTagType::Float,   1                         },
    { "AWBFrameControlCCT",     VendorTagType::Int32,   1                         },
    { "AWBDecisionAfterTC",     VendorTagType::Float,   2                         },
    { "HistNodeLTCRatioIndex",  VendorTagType::Byte,    sizeof(UINT32)            },
    { "AECIsInsensorHDR",       VendorTagType::Byte,    1                         },
    { "AECStartUpSensitivity",  VendorTagType::Float,   3                         },
    { "AECSensitivity",         VendorTagType::Float,   3                         },
};

/// org.quic.camera2.iqsettings section
static VendorTagData g_VendorTagSectionIQSettings[] =
{
    { "OEMIFEIQSetting", VendorTagType::Byte, sizeof(OEMIFEIQSetting) },
    { "OEMBPSIQSetting", VendorTagType::Byte, sizeof(OEMBPSIQSetting) },
    { "OEMIPEIQSetting", VendorTagType::Byte, sizeof(OEMIPEIQSetting) }
};

/// org.quic.camera2.ipeicaconfigs section
static VendorTagData g_VendorTagSectionICAConfigs[] =
{
    { "ICAInPerspectiveTransform",               VendorTagType::Byte, sizeof(IPEICAPerspectiveTransform) },
    { "ICAInGridTransform",                      VendorTagType::Byte, sizeof(IPEICAGridTransform)        },
    { "ICAInInterpolationParams",                VendorTagType::Byte, sizeof(IPEICAInterpolationParams)  },
    { "ICARefPerspectiveTransform",              VendorTagType::Byte, sizeof(IPEICAPerspectiveTransform) },
    { "ICARefGridTransform",                     VendorTagType::Byte, sizeof(IPEICAGridTransform)        },
    { "ICARefInterpolationParams",               VendorTagType::Byte, sizeof(IPEICAInterpolationParams)  },
    { "ICAReferenceParams",                      VendorTagType::Byte, sizeof(IPEICAPerspectiveTransform) },
    { "ICAInPerspectiveTransformLookAhead",      VendorTagType::Byte, sizeof(IPEICAPerspectiveTransform) },
    { "ICAInGridTransformLookahead",             VendorTagType::Byte, sizeof(IPEICAGridTransform)        },
    { "ExtraFrameworkBuffers",                   VendorTagType::Byte, sizeof(UINT32)                     },
};

// org.quic.camera2.mfnrconfigs section
static VendorTagData g_VendorTagSectionMFNRConfigs[] =
{
    { "MFNRTotalNumFrames",                 VendorTagType::Int32, 1 },
};

// org.quic.camera2.mfsrconfigs section
static VendorTagData g_VendorTagSectionMFSRConfigs[] =
{
    { "MFSRTotalNumFrames",                 VendorTagType::Int32, 1 },
};

/// org.quic.camera2.streamconfigs section
static VendorTagData g_VendorTagSectionStreamConfigs[] =
{
    { "HDRVideoMode", VendorTagType::Byte, 1},
};

/// org.quic.camera2.jpegquantizationtables section
static VendorTagData g_VendorTagSectionJPEGQuantizationTables[] =
{
    { QuantizationTableLumaVendorTagName, VendorTagType::Byte, sizeof(UINT16) * QuantTableSize   },
    { QuantizationTableChromaVendorTagName, VendorTagType::Byte, sizeof(UINT16) * QuantTableSize }
};

/// org.quic.camera2.sensor_register_control
static VendorTagData g_VendorTagSectionSensorRegisterControl[] =
{
    { "sensor_register_control", VendorTagType::Int32, 16 }
};

/// org.quic.camera.bafstats
static VendorTagData g_VendorTagSectionBAFStats[] =
{
    { "stats",     VendorTagType::Byte, sizeof(ParsedBFStatsOutput) }
};

/// org.quic.camera.focusvalue
static VendorTagData g_VendorTagSectionFocusValue[] =
{
    { "FocusValue",  VendorTagType::Float, 1 }
};

/// org.quic.camera.isDepthFocus
static VendorTagData g_VendorTagSectionIsDepthFocus[] =
{
    { "isDepthFocus",  VendorTagType::Byte, sizeof(BOOL) }
};

/// org.quic.camera2.tuning mode
static VendorTagData g_VendorTagSectionTuningParam[] =
{
    { "TuningMode",  VendorTagType::Byte, sizeof(ChiTuningModeParameter)  }
};

/// org.quic.camera2.tuning.feature mode
static VendorTagData g_VendorTagSectionFeatureParam[] =
{
    { "Feature1Mode",  VendorTagType::Byte, 1  },
    { "Feature2Mode",  VendorTagType::Byte, 1  }
};

/// org.quic.camera2.ref.cropsize
static VendorTagData g_VendorTagSectionRefCropsize[] =
{
    { "RefCropSize",  VendorTagType::Byte, sizeof(RefCropWindowSize)},
    { "DisableZoomCrop", VendorTagType::Int32, 1}
};

/// org.quic.camera.ifecropinfo
static VendorTagData g_VendorTagSectionIFECropInfo[] =
{
    {"ResidualCrop",         VendorTagType::Byte, sizeof(IFECropInfo) },
    {"AppliedCrop",          VendorTagType::Byte, sizeof(IFECropInfo) },
    {"SensorIFEAppliedCrop", VendorTagType::Byte, sizeof(IFECropInfo) }
};

/// org.quic.camera.gammainfo
static VendorTagData g_VendorTagSectionGammaInfo[] =
{
    { "GammaInfo",  VendorTagType::Byte, sizeof(GammaInfo) }
};

/// org.quic.camera.intermediateDimension
static VendorTagData g_VendorTagSectionIntermediateDimension[] =
{
    { "IntermediateDimension",  VendorTagType::Byte, sizeof(IntermediateDimensions) }
};

/// org.quic.camera.qtimer
static VendorTagData g_VendorTagSectionQTimer[] =
{
    { "timestamp",  VendorTagType::Byte, sizeof(ChiTimestampInfo) },
};

/// org.quic.camera.eis3enable
static VendorTagData g_VendorTagSectionEISConfig[] =
{
    { "EISV3Enable",  VendorTagType::Byte, 1 }
};

/// org.quic.camera.streamTypePresent
static VendorTagData g_VendorTagSectionStreamTypePresent[] =
{
    { "preview",  VendorTagType::Byte, 1 }
};

/// org.codeaurora.qcamera3.quadra_cfa section
static VendorTagData g_VendorTagSectionQuadCFA[] =
{
    { "is_qcfa_sensor",  VendorTagType::Byte,  1 },
    { "qcfa_dimension",  VendorTagType::Int32, 2 }
};

/// org.codeaurora.qcamera3.available_video_hdr_modes
static VendorTagData g_VendorTagSectionVideoHDR[] =
{
    { "video_hdr_modes", VendorTagType::Int32, 2 }
};

/// org.quic.camera.streamDimension
static VendorTagData g_VendorTagSectionStreamDimension[] =
{
    { "preview",  VendorTagType::Byte, sizeof(ChiBufferDimension) },
    { "video",    VendorTagType::Byte, sizeof(ChiBufferDimension) },
    { "snapshot", VendorTagType::Byte, sizeof(ChiBufferDimension) },
    { "depth",    VendorTagType::Byte, sizeof(ChiBufferDimension) }
};

/// org.quic.camera2.sensormode.info
static VendorTagData g_VendorTagSectionSensorModeTable[] =
{
    { "SensorModeTable", VendorTagType::Int32, MaxSensorModeTableEntries }
};

/// org.quic.camera2.customhfrfps.info
static VendorTagData g_VendorTagSectionCustomFpsTable[] =
{
    { "CustomHFRFpsTable", VendorTagType::Int32, MaxCustomHFRSizes }
};

/// org.quic.camera.recording
static VendorTagData g_VendorTagSectionRecordingConfig[] =
{
    { "endOfStream",           VendorTagType::Byte,  1 },
    { "endOfStreamRequestId",  VendorTagType::Byte,  sizeof(UINT64) }
};

/// org.quic.camera.EarlyPCRenable
static VendorTagData g_VendorTagSectionEarlyPCRFlag[] =
{
    { "EarlyPCRenable", VendorTagType::Byte, 1 }
};

/// org.quic.camera.BurstFPS
static VendorTagData g_VendorTagSectionBurstFPSFlag[] =
{
    { "burstfps", VendorTagType::Byte, 1 }
};

/// org.quic.camera.CustomNoiseReduction
static VendorTagData g_VendorTagSectionCustomNoiseReductionFlag[] =
{
    { "CustomNoiseReduction", VendorTagType::Byte, 1 }
};

/// org.quic.camera.SensorModeFS
static VendorTagData g_VendorTagSectionSensorModeFSFlag[] =
{
    { "isFastShutterModeSupported",  VendorTagType::Byte,  1 },
    { "SensorModeFS",                VendorTagType::Byte,  1 }
};

/// org.quic.camera.AECDataPublisherPresent
static VendorTagData g_VendorTagSectionAECDataPublisherPresent[] =
{
    { "AECDataPublisherPresent", VendorTagType::Byte, 1 }
};

/// org.quic.camera.AECData
static VendorTagData g_VendorTagSectionAECData[] =
{
    { "AECData", VendorTagType::Byte, sizeof(ChiAECData) }
};

/// org.quic.camera.oemexifappdata section
static VendorTagData g_VendorTagSectionOEMEXIFAppData[] =
{
    { "OEMEXIFAppData1", VendorTagType::Byte, sizeof(OEMJPEGEXIFAppData) }
};

/// org.quic.camera.manualExposure
static VendorTagData g_VendorTagSectionManualExposure[] =
{
    { "disableFPSLimits",  VendorTagType::Byte,  1 },
};

/// org.codeaurora.qcamera3.logicalCameraType
static VendorTagData g_VendorTagSectionLogicalCameraType[] =
{
    { "logical_camera_type", VendorTagType::Byte, 1 }
};

/// org.codeaurora.qcamera3.platformCapabilities section
static VendorTagData g_VendorTagSectionPlatformCapabilities[] =
{
    { "IPEICACapabilities", VendorTagType::Byte, sizeof(IPEICACapability) }
};

/// org.codeaurora.qcamera3.exposureTable
static VendorTagData g_VendorTagSectionExposureTableType[] =
{
    { "isValid",                       VendorTagType::Byte,  1 },
    { "sensitivityCorrectionFactor",   VendorTagType::Float, 1 },
    { "kneeCount",                     VendorTagType::Int32, 1 },
    { "gainKneeEntries",               VendorTagType::Float, MaxTableKnees },
    { "expTimeKneeEntries",            VendorTagType::Int64, MaxTableKnees },
    { "incrementPriorityKneeEntries",  VendorTagType::Int32, MaxTableKnees },
    { "expIndexKneeEntries",           VendorTagType::Float, MaxTableKnees },
    { "thresAntiBandingMinExpTimePct", VendorTagType::Float, 1 },
};

/// org.codeaurora.qcamera3.ADRC
static VendorTagData g_VendorTagSectionADRC[] =
{
    { "disable", VendorTagType::Byte,  1 },
};

static VendorTagData g_VendorTagSectionTintless[] =
{
    { "enable", VendorTagType::Byte,  1 },
};

/// org.codeaurora.qcamera3.meteringtable
static VendorTagData g_VendorTagSectionMeteringTableType[] =
{
    { "meteringTable",     VendorTagType::Float, MaxMeteringTableSize },
    { "meteringTableSize", VendorTagType::Int32, 1 }
};

/// org.quic.camera.ltmDynamicContrast
static VendorTagData g_VendorTagSectionltmDynamicContrast[] =
{
    {"ltmDynamicContrastStrength",         VendorTagType::Float, 1 },
    {"ltmDarkBoostStrength",               VendorTagType::Float, 1 },
    {"ltmBrightSupressStrength",           VendorTagType::Float, 1 },
    {"ltmDynamicContrastStrengthRange",    VendorTagType::Float, 2 },
    {"ltmDarkBoostStrengthRange",          VendorTagType::Float, 2 },
    {"ltmBrightSupressStrengthRange",      VendorTagType::Float, 2 }
};

static VendorTagSectionData g_HwVendorTagSections[] =
{
    {
        "org.codeaurora.qcamera3.saturation", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionSaturation), g_VendorTagSectionSaturation,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.exposure_metering", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionExposureMetering), g_VendorTagSectionExposureMetering,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.aec_convergence_speed", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionAecConvergenceSpeed), g_VendorTagSectionAecConvergenceSpeed,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.sharpness", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionSharpness), g_VendorTagSectionSharpness,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.contrast", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionContrast), g_VendorTagSectionContrast,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.iso_exp_priority", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionIsoExpPriority), g_VendorTagSectionIsoExpPriority,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.histogram", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionHistogram), g_VendorTagSectionHistogram,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.bayer_grid", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionBGStats), g_VendorTagSectionBGStats,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.bayer_exposure", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionBEStats), g_VendorTagSectionBEStats,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.instant_aec", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionInstanctAec), g_VendorTagSectionInstanctAec,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.awb_convergence_speed", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionAwbConvSpeed), g_VendorTagSectionAwbConvSpeed,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.ae_bracket", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionAEBracket), g_VendorTagSectionAEBracket,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.jpeg_encode_crop", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionJpegEncodeCrop), g_VendorTagSectionJpegEncodeCrop,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.codeaurora.qcamera3.sensor_meta_data", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionSensorMeta), g_VendorTagSectionSensorMeta,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.codeaurora.qcamera3.av_timer",  0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionAVTimer), g_VendorTagSectionAVTimer,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera2.statsconfigs", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionStatsConfigs), g_VendorTagSectionStatsConfigs,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera2.iqsettings", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionIQSettings), g_VendorTagSectionIQSettings,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera2.streamconfigs", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionStreamConfigs), g_VendorTagSectionStreamConfigs,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        QuantizationTableVendorTagSection, 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionJPEGQuantizationTables), g_VendorTagSectionJPEGQuantizationTables,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera2.sensor_register_control", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionSensorRegisterControl), g_VendorTagSectionSensorRegisterControl,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera2.ipeicaconfigs", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionICAConfigs), g_VendorTagSectionICAConfigs,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera2.mfnrconfigs", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionMFNRConfigs), g_VendorTagSectionMFNRConfigs,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera2.mfsrconfigs", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionMFSRConfigs), g_VendorTagSectionMFSRConfigs,
        TagSectionVisibility::TagSectionVisibleToAll
    },
    {
        "org.quic.camera.debugdata", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionDebugData), g_VendorTagSectionDebugData,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera.tuningdata", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionTuningdataDump), g_VendorTagSectionTuningdataDump,
        TagSectionVisibility::TagSectionVisibleToOEM
    },
    {
        "org.quic.camera.focusvalue", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionFocusValue), g_VendorTagSectionFocusValue,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera.bafstats", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionBAFStats), g_VendorTagSectionBAFStats,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera2.tuning.mode", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionTuningParam), g_VendorTagSectionTuningParam,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera2.tuning.feature", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionFeatureParam), g_VendorTagSectionFeatureParam,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        VendorTagSectionOEMFDConfig, 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionOEMFDConfig), g_VendorTagSectionOEMFDConfig,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        VendorTagSectionOEMFDResults, 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionOEMFDResults), g_VendorTagSectionOEMFDResults,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera2.ref.cropsize", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionRefCropsize), g_VendorTagSectionRefCropsize,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.manualWB", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionManualWB), g_VendorTagSectionManualWB,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.temporal_denoise", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionTemporalDenoise), g_VendorTagSectionTemporalDenoise,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.ifecropinfo", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionIFECropInfo), g_VendorTagSectionIFECropInfo,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera.eis3enable", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionEISConfig), g_VendorTagSectionEISConfig,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.streamTypePresent", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionStreamTypePresent), g_VendorTagSectionStreamTypePresent,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        VendorTagSectionEISRealTimeConfig, 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionEISRealTimeConfig), g_VendorTagSectionEISRealTimeConfig,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        VendorTagSectionEISLookAheadConfig, 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionEISLookAheadConfig), g_VendorTagSectionEISLookAheadConfig,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.qtimer", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionQTimer), g_VendorTagSectionQTimer,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.codeaurora.qcamera3.stats", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionStats), g_VendorTagSectionStats,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.quadra_cfa", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionQuadCFA), g_VendorTagSectionQuadCFA,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.available_video_hdr_modes", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionVideoHDR), g_VendorTagSectionVideoHDR,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.streamDimension", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionStreamDimension), g_VendorTagSectionStreamDimension,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera2.sensormode.info", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionSensorModeTable), g_VendorTagSectionSensorModeTable,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera2.customhfrfps.info", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionCustomFpsTable), g_VendorTagSectionCustomFpsTable,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.gammainfo", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionGammaInfo), g_VendorTagSectionGammaInfo,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera.intermediateDimension", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionIntermediateDimension), g_VendorTagSectionIntermediateDimension,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.recording", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionRecordingConfig), g_VendorTagSectionRecordingConfig,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.manualExposure", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionManualExposure), g_VendorTagSectionManualExposure,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        VendorTagSectionASDResults, 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionASDResults), g_VendorTagSectionASDResults,
        TagSectionVisibility::TagSectionVisibleToOEM
    },

    {
        "org.quic.camera.EarlyPCRenable", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionEarlyPCRFlag), g_VendorTagSectionEarlyPCRFlag,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.BurstFPS", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionBurstFPSFlag), g_VendorTagSectionBurstFPSFlag,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.CustomNoiseReduction", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionCustomNoiseReductionFlag), g_VendorTagSectionCustomNoiseReductionFlag,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.SensorModeFS", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionSensorModeFSFlag), g_VendorTagSectionSensorModeFSFlag,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.AECDataPublisherPresent", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionAECDataPublisherPresent), g_VendorTagSectionAECDataPublisherPresent,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.AECData", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionAECData), g_VendorTagSectionAECData,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.oemexifappdata", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionOEMEXIFAppData), g_VendorTagSectionOEMEXIFAppData,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.quic.camera.isDepthFocus", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionIsDepthFocus), g_VendorTagSectionIsDepthFocus,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.logicalCameraType", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionLogicalCameraType), g_VendorTagSectionLogicalCameraType,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.platformCapabilities", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionPlatformCapabilities), g_VendorTagSectionPlatformCapabilities,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.exposuretable", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionExposureTableType), g_VendorTagSectionExposureTableType,
        TagSectionVisibility::TagSectionVisibleToAll
    },

    {
        "org.codeaurora.qcamera3.meteringtable", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionMeteringTableType), g_VendorTagSectionMeteringTableType,
        TagSectionVisibility::TagSectionVisibleToAll
    },
    {
        "org.codeaurora.qcamera3.adrc", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionADRC), g_VendorTagSectionADRC,
        TagSectionVisibility::TagSectionVisibleToAll
    },
    {
        "org.quic.camera.tintless", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionTintless), g_VendorTagSectionTintless,
        TagSectionVisibility::TagSectionVisibleToAll
    },
    {
        "org.quic.camera.ltmDynamicContrast", 0,
        CAMX_ARRAY_SIZE(g_VendorTagSectionltmDynamicContrast), g_VendorTagSectionltmDynamicContrast,
        TagSectionVisibility::TagSectionVisibleToAll
    },
};

/// @brief Number of workarounds
static const UINT32 Titan17xNumWorkarounds = 2;

/// @brief HW bug workaround table
const static HWBugWorkaround g_pTitan17xWorkarounds[Titan17xNumWorkarounds] =
{
    {Titan17xWorkaroundsCDMDMI64EndiannessBug, "CDM DMI64 command endianness bug", TRUE, FALSE, TRUE},  ///< Bus present in
                                                                                                        ///  HW/RUMI, not CSIM
    {Titan17xWorkaroundsCDMDMICGCBug, "CDM IFE legacy DMI CGC requirement", TRUE, TRUE, TRUE}           ///< CGC Bug
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::GetStaticMetadataKeysInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Titan17xContext::GetStaticMetadataKeysInfo(
    StaticMetadataKeysInfo* pKeysInfo,
    CameraMetadataTag tag)
{
    CamxResult result = CamxResultSuccess;

    switch (tag)
    {
        case RequestAvailableRequestKeys:
            pKeysInfo->pStaticKeys = &(SupportedRequestKeys[0]);
            pKeysInfo->numKeys     = NumSupportedRequestKeys;
            break;

        case RequestAvailableResultKeys:
            pKeysInfo->pStaticKeys = &(SupportedResultKeys[0]);
            pKeysInfo->numKeys     = NumSupportedResultKeys;
            break;

        case RequestAvailableCharacteristicsKeys:
            pKeysInfo->pStaticKeys = &(SupportedCharacteristicsKeys[0]);
            pKeysInfo->numKeys     = NumSupportedCharacteristicsKeys;
            break;
#if ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
        case RequestAvailableSessionKeys:
            pKeysInfo->pStaticKeys = &(SupportedSessionKeys[0]);
            pKeysInfo->numKeys     = NumSupportedSessionKeys;
            break;
#endif // ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28))
        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Got unsupported metadata tag");
            result = CamxResultEUnsupported;
            break;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Titan17xContext::Create(
    HwContextCreateData* pCreateData)
{
    CamxResult       result           = CamxResultSuccess;
    Titan17xContext* pTitan17xContext = CAMX_NEW Titan17xContext;

    if (NULL != pTitan17xContext)
    {
        result = pTitan17xContext->HwInitialize();

        if (CamxResultSuccess == result)
        {
            pCreateData->pHwContext = pTitan17xContext;
        }
        else
        {
            CAMX_DELETE pTitan17xContext;
            pTitan17xContext = NULL;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Out of memory; cannot create Titan17xContext");
        result = CamxResultENoMemory;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::QueryVendorTagsInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Titan17xContext::QueryVendorTagsInfo(
    VendorTagInfo* pVendorTagInfo)
{
    CAMX_ASSERT(pVendorTagInfo != NULL);

    pVendorTagInfo->pVendorTagDataArray = g_HwVendorTagSections;
    pVendorTagInfo->numSections         = CAMX_ARRAY_SIZE(g_HwVendorTagSections);

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::QueryExternalComponentVendorTagsInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Titan17xContext::QueryExternalComponentVendorTagsInfo(
    ComponentVendorTagsInfo* pComponentVendorTagsInfo)
{
    CamxResult result = CamxResultSuccess;
    CAMX_ASSERT(pComponentVendorTagsInfo != NULL);

    pComponentVendorTagsInfo->pVendorTagInfoArray   = g_componentVendorTagsInfo.pVendorTagInfoArray;
    pComponentVendorTagsInfo->numVendorTagInfo      = g_componentVendorTagsInfo.numVendorTagInfo;

    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::GetStaticCaps
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Titan17xContext::GetStaticCaps(
    PlatformStaticCaps* pCaps)
{
    CamxResult          result        = CamxResultSuccess;
    CSLCameraPlatform   CSLPlatform   = {};
    UINT32              size          = 0;
    UINT32              numIPEs       = 2;
    UINT32              HFRPreviewFPS = DefaultHFRPreviewFPS;

    // Initialize platform specific static capabilities

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Color Correction
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Available abberation modes
    size                            = 0;
    pCaps->abberationModes[size++]  = ColorCorrectionAberrationModeOff;
    pCaps->abberationModes[size++]  = ColorCorrectionAberrationModeFast;
    pCaps->abberationModes[size++]  = ColorCorrectionAberrationModeHighQuality;
    pCaps->numAbberationsModes      = size;


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // 3A
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Available antibanding modes
    // Since AFD is purely a software module, all modes are available
    size                            = 0;
    pCaps->antibandingModes[size++] = ControlAEAntibandingModeOff;
    pCaps->antibandingModes[size++] = ControlAEAntibandingMode50Hz;
    pCaps->antibandingModes[size++] = ControlAEAntibandingMode60Hz;
    pCaps->antibandingModes[size++] = ControlAEAntibandingModeAuto;
    CAMX_ASSERT(size <= ControlAEAntibandingModeEnd);
    pCaps->numAntibandingModes      = size;

    // Available AE modes
    // Add AE flash modes in HAL module, depending on the availability of the flash.
    size                    = 0;
    pCaps->AEModes[size++]  = ControlAEModeOff;
    pCaps->AEModes[size++]  = ControlAEModeOn;
    pCaps->AEModes[size++] = ControlAEModeOnAutoFlash;
    pCaps->AEModes[size++] = ControlAEModeOnAlwaysFlash;
    pCaps->AEModes[size++] = ControlAEModeOnAutoFlashRedeye;
    CAMX_ASSERT(size <= ControlAEModeEnd);
    pCaps->numAEModes       = size;

    // AE compensation range.
    pCaps->minAECompensationValue = -12;
    pCaps->maxAECompensationValue = 12;

    // AE compensation step
    pCaps->AECompensationSteps.numerator   = 1;
    pCaps->AECompensationSteps.denominator = 6;

    // Available AF modes
    size                    = 0;
    pCaps->AFModes[size++]  = ControlAFModeOff;
    pCaps->AFModes[size++]  = ControlAFModeAuto;
    pCaps->AFModes[size++]  = ControlAFModeMacro;
    pCaps->AFModes[size++]  = ControlAFModeContinuousVideo;
    pCaps->AFModes[size++]  = ControlAFModeContinuousPicture;
    CAMX_ASSERT(size <= ControlAFModeEnd);
    pCaps->numAFModes       = size;

    // Available AWB modes
    size                    = 0;
    pCaps->AWBModes[size++] = ControlAWBModeAuto;
    pCaps->AWBModes[size++] = ControlAWBModeIncandescent;
    pCaps->AWBModes[size++] = ControlAWBModeFluorescent;
    pCaps->AWBModes[size++] = ControlAWBModeWarmFluorescent;
    pCaps->AWBModes[size++] = ControlAWBModeDaylight;
    pCaps->AWBModes[size++] = ControlAWBModeCloudyDaylight;
    pCaps->AWBModes[size++] = ControlAWBModeTwilight;
    pCaps->AWBModes[size++] = ControlAWBModeShade;
    pCaps->AWBModes[size++] = ControlAWBModeOff;
    CAMX_ASSERT(size <= ControlAWBModeEnd);
    pCaps->numAWBModes      = size;

    // Available maxRegions for AE, AWB, AF.
    // For limited mode, keep all the values 0.
    pCaps->maxRegionsAE     = 1;
    pCaps->maxRegionsAWB    = 0;
    pCaps->maxRegionsAF     = 1;

    // Available AE and AWB locks. Update them as FALSE for limited mode.
    pCaps->lockAEAvailable  = TRUE;
    pCaps->lockAWBAvailable = TRUE;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Controls
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Available Effect mode values
    size                        = 0;
    pCaps->effectModes[size++]  = ControlEffectModeOff;
    pCaps->effectModes[size++]  = ControlEffectModeMono;
    pCaps->effectModes[size++]  = ControlEffectModeNegative;
    pCaps->effectModes[size++]  = ControlEffectModeSolarize;
    pCaps->effectModes[size++]  = ControlEffectModeSepia;
    pCaps->effectModes[size++]  = ControlEffectModePosterize;
    pCaps->effectModes[size++]  = ControlEffectModeAqua;
    pCaps->effectModes[size++]  = ControlEffectModeBlackboard;
    pCaps->effectModes[size++]  = ControlEffectModeWhiteboard;
    CAMX_ASSERT(size <= ControlEffectModeEnd);
    pCaps->numEffectModes       = size;

    // Available Scene mode values
    size                        = 0;
    pCaps->sceneModes[size++]   = ControlSceneModeDisabled;
    pCaps->sceneModes[size++]   = ControlSceneModeFacePriority;
    pCaps->sceneModes[size++]   = ControlSceneModeAction;
    pCaps->sceneModes[size++]   = ControlSceneModePortrait;
    pCaps->sceneModes[size++]   = ControlSceneModeLandscape;
    pCaps->sceneModes[size++]   = ControlSceneModeNight;
    pCaps->sceneModes[size++]   = ControlSceneModeNightPortrait;
    pCaps->sceneModes[size++]   = ControlSceneModeTheatre;
    pCaps->sceneModes[size++]   = ControlSceneModeBeach;
    pCaps->sceneModes[size++]   = ControlSceneModeSnow;
    pCaps->sceneModes[size++]   = ControlSceneModeSunset;
    pCaps->sceneModes[size++]   = ControlSceneModeFireworks;
    pCaps->sceneModes[size++]   = ControlSceneModeSports;
    pCaps->sceneModes[size++]   = ControlSceneModeParty;
    pCaps->sceneModes[size++]   = ControlSceneModeCandlelight;
    pCaps->sceneModes[size++]   = ControlSceneModeHdr;
    CAMX_ASSERT(size <= ControlSceneModeEnd);
    pCaps->numSceneModes        = size;

    // Available scene override modes for AE, AWB and AF.
    for (UINT8 i = 0; i < pCaps->numSceneModes; i++)
    {
        // For now hardcoding the values for limited mode.
        pCaps->sceneModeOverride[i].AEModeOverride  = ControlAEModeOn;
        pCaps->sceneModeOverride[i].AWBModeOverride = ControlAWBModeAuto;
        pCaps->sceneModeOverride[i].AFModeOverride  = ControlAFModeOff;
    }

    // Available Video stabilization modes
    size                                    = 0;
    pCaps->videoStabilizationsModes[size++] = ControlVideoStabilizationModeOff;
    pCaps->videoStabilizationsModes[size++] = ControlVideoStabilizationModeOn;
    CAMX_ASSERT(size <= ControlVideoStabilizationModeEnd);
    pCaps->numVideoStabilizationModes       = size;

    // Available 3A modes
    /// @todo (CAMX-1359) Populate available 3A modes once available
    size                            = 0;
    pCaps->availableModes[size++]   = ControlModeOff;
    pCaps->availableModes[size++]   = ControlModeAuto;
    pCaps->availableModes[size++]   = ControlModeUseSceneMode;
    pCaps->numAvailableModes        = size;

    // Available post raw sensitivity boost range
    /// @todo (CAMX-1359) Populate isp sensitivity boost range once available
    pCaps->minPostRawSensitivityBoost = MinGain * IsoMultiplyFactor;
    pCaps->maxPostRawSensitivityBoost = MaxSensitivityBoost;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Edge
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Available edge modes in HAL module
    size                        = 0;
    pCaps->edgeModes[size++]    = EdgeModeFast;
    pCaps->edgeModes[size++]    = EdgeModeHighQuality;
    pCaps->edgeModes[size++]    = EdgeModeOff;
    pCaps->edgeModes[size++]    = EdgeModeZeroShutterLag;
    CAMX_ASSERT(size <= EdgeModeEnd);
    pCaps->numEdgeModes         = size;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Hot Pixel
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    size                            = 0;
    pCaps->hotPixelModes[size++]    = HotPixelModeOff;
    pCaps->hotPixelModes[size++]    = HotPixelModeFast;
    pCaps->hotPixelModes[size++]    = HotPixelModeHighQuality;
    pCaps->numHotPixelModes         = size;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // JPEG
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Available thumbnail sizes
    pCaps->numJPEGThumbnailSizes = NumSupportedThumbnailSizes;

    for (UINT8 i = 0; i < pCaps->numJPEGThumbnailSizes; i++)
    {
        pCaps->JPEGThumbnailSizes[i].width  = SupportedThumbnailSizes[i].width;
        pCaps->JPEGThumbnailSizes[i].height = SupportedThumbnailSizes[i].height;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Noise Reduction
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Available noise reduction modes in HAL module
    size                                = 0;
    pCaps->noiseReductionModes[size++]  = NoiseReductionModeOff;
    pCaps->noiseReductionModes[size++]  = NoiseReductionModeFast;
    pCaps->noiseReductionModes[size++]  = NoiseReductionModeHighQuality;
    pCaps->noiseReductionModes[size++]  = NoiseReductionModeMinimal;
    pCaps->noiseReductionModes[size++]  = NoiseReductionModeZeroShutterLag;
    CAMX_ASSERT(size <= NoiseReductionModeEnd);
    pCaps->numNoiseReductionModes       = size;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Request
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Available maximum number of output streams that can be configured for different types.
    pCaps->maxRawStreams                = MaxRawStreams;
    pCaps->maxProcessedStreams          = MaxProcessedStreams;
    pCaps->maxProcessedStallingStreams  = MaxProcessedStallingStreams;
    pCaps->maxInputStreams              = MaxInputStream;
    pCaps->maxPipelineDepth             = MaxPipelineDepth;
    pCaps->partialResultCount           = PartialResultCount;

    /// @todo (CAMX-586) - Populate static capabilities for FULL mode for Titan17x HWcontext.
    // Available request capabilities
    size                        = 0;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesBackwardCompatible;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesConstrainedHighSpeedVideo;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesRaw;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesYuvReprocessing;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesPrivateReprocessing;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesReadSensorSettings;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesManualSensor;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesBurstCapture;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesManualPostProcessing;
    pCaps->requestCaps[size++]  = RequestAvailableCapabilitiesDepthOutput;
    CAMX_ASSERT(size <= RequestAvailableCapabilitiesEnd);
    pCaps->numRequestCaps       = size;

    // Available request keys
    pCaps->numRequestKeys = NumSupportedRequestKeys;
    for (UINT8 i = 0; i < pCaps->numRequestKeys; i++)
    {
        pCaps->requestKeys[i] = SupportedRequestKeys[i];
    }

    // Available result keys
    pCaps->numResultKeys = NumSupportedResultKeys;
    for (UINT8 i = 0; i < pCaps->numResultKeys; i++)
    {
        pCaps->resultKeys[i] = SupportedResultKeys[i];
    }

    // Available Characteristics keys
    pCaps->numCharacteristicsKeys = NumSupportedCharacteristicsKeys;
    for (UINT8 i = 0; i < pCaps->numCharacteristicsKeys; i++)
    {
        pCaps->characteristicsKeys[i] = SupportedCharacteristicsKeys[i];
    }

#if ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
    pCaps->numSessionKeys = NumSupportedSessionKeys;
    for (UINT8 i = 0; i < pCaps->numSessionKeys; i++)
    {
        pCaps->sessionKeys[i] = SupportedSessionKeys[i];
    }
#endif // ((CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28))

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Scaler
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Available max digital zoom
    pCaps->maxDigitalZoom   = MaxDigitalZoom;

    // Available scaler format
    pCaps->numScalerFormats = NumSupportedScalerFormats;
    for (UINT8 i = 0; i < pCaps->numScalerFormats; i++)
    {
        pCaps->scalerFormats[i] = SupportedScalerFormats[i];
    }

    // Available scaler input output format map
    pCaps->numInputOutputFormatMaps = NumSupportedInputOutputFormatMaps;
    for (UINT8 i = 0; i < pCaps->numInputOutputFormatMaps; i++)
    {
        pCaps->inputOutputFormatMap[i] = SupportedInputOutputFormatMaps[i];
    }

    // Available scaler default image sizes
    pCaps->numDefaultImageSizes = NumSupportedImageSizes;
    for (UINT8 i = 0; i < pCaps->numDefaultImageSizes; i++)
    {
        pCaps->defaultImageSizes[i].width   = SupportedImageSizes[i].width;
        pCaps->defaultImageSizes[i].height  = SupportedImageSizes[i].height;
    }

    // Available scaler default fps ranges
    pCaps->numDefaultFpsRanges = NumSupportedDefaultFPSRanges;
    for (UINT8 i = 0; i < pCaps->numDefaultFpsRanges; i++)
    {
        pCaps->defaultTargetFpsRanges[i].min   = SupportedDefaultTargetFpsRanges[i].min;
        pCaps->defaultTargetFpsRanges[i].max   = SupportedDefaultTargetFpsRanges[i].max;
    }

    // Find Device Type
    result = CSLQueryCameraPlatform(&CSLPlatform);

    if (CamxResultSuccess == result)
    {
        if ((CSLCameraTitanSocSDM670  == CSLPlatform.socId) ||
            (CSLCameraTitanSocSM6150  == CSLPlatform.socId) ||
            (CSLCameraTitanSocSM7150  == CSLPlatform.socId) ||
            (CSLCameraTitanSocSM7150P == CSLPlatform.socId))
        {
            HFRPreviewFPS = 30;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHWL, "CSLQueryCameraPlatform failed with error %d", result);
    }

    CAMX_LOG_INFO(CamxLogGroupHWL, "HFR Preview FPS %d", HFRPreviewFPS);

    // Availiable HFR configurations
    pCaps->numDefaultHFRVideoSizes = NumSupportedHFRVideoSizes;
    for (UINT8 i = 0; i < pCaps->numDefaultHFRVideoSizes; i++)
    {
        pCaps->defaultHFRVideoSizes[i].width        = SupportedHFRVideoSizes[i].width;
        pCaps->defaultHFRVideoSizes[i].height       = SupportedHFRVideoSizes[i].height;
        pCaps->defaultHFRVideoSizes[i].minFPS       = SupportedHFRVideoSizes[i].minFPS;
        pCaps->defaultHFRVideoSizes[i].maxFPS       = SupportedHFRVideoSizes[i].maxFPS;
        pCaps->defaultHFRVideoSizes[i].batchSizeMax = SupportedHFRVideoSizes[i].maxFPS / HFRPreviewFPS;

        if (MaxBatchedFrames < pCaps->defaultHFRVideoSizes[i].batchSizeMax)
        {
            pCaps->defaultHFRVideoSizes[i].batchSizeMax = SupportedHFRVideoSizes[i].maxFPS / DefaultHFRPreviewFPS;
            HFRPreviewFPS                               = DefaultHFRPreviewFPS;
        }
    }
    pCaps->JPEGFormatStallFactorPerPixel    = JPEGStallFactorPerPixel;
    pCaps->croppingSupport                  = ScalerCroppingTypeCenterOnly;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Sensor
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // This means the timebase should be CLOCK_BOOTTIME
    pCaps->timestampSource = SensorInfoTimestampSourceRealtime;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Shading
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    size                        = 0;
    pCaps->shadingModes[size++] = ShadingModeOff;
    pCaps->shadingModes[size++] = ShadingModeFast;
    pCaps->shadingModes[size++] = ShadingModeHighQuality;
    pCaps->numShadingModes      = size;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Statistics
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    size                            = 0;
    pCaps->faceDetectModes[size++]  = StatisticsFaceDetectModeOff;
    pCaps->faceDetectModes[size++]  = StatisticsFaceDetectModeSimple;
    pCaps->numFaceDetectModes       = size;
    pCaps->maxFaceCount             = MaxFaceCount;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Tonemap
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    size                            = 0;
    pCaps->tonemapModes[size++]     = TonemapModeContrastCurve;
    pCaps->tonemapModes[size++]     = TonemapModeFast;
    pCaps->tonemapModes[size++]     = TonemapModeHighQuality;
    pCaps->numTonemapModes          = size;
    pCaps->maxTonemapCurvePoints    = MaxTonemapCurvePoints;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Support level
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Supported Hardware level
    pCaps->supportedHwLevel = InfoSupportedHardwareLevel3;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Sync
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    pCaps->syncMaxLatency   = SyncMaxLatencyPerFrameControl;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Reprocess
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Maximum capture stall
    pCaps->maxCaptureStall  = MaxCaptureStall;

    // Available Exposure Metering mode values
    size                                  = 0;
    pCaps->exposureMeteringModes[size++]  = ExposureMeteringFrameAverage;
    pCaps->exposureMeteringModes[size++]  = ExposureMeteringCenterWeighted;
    pCaps->exposureMeteringModes[size++]  = ExposureMeteringSpotMetering;
    CAMX_ASSERT(size <= ExposureMeteringEnd);
    pCaps->numExposureMeteringModes       = size;

    // Saturation Range
    pCaps->saturationRange.minValue       = 0;
    pCaps->saturationRange.maxValue       = 10;
    pCaps->saturationRange.defaultValue   = 5;
    pCaps->saturationRange.step           = 1;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Depth
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Available depth format
    pCaps->numDepthFormats = NumSupportedDepthFormats;
    for (UINT8 i = 0; i < pCaps->numDepthFormats; i++)
    {
        pCaps->depthFormats[i] = SupportedDepthFormats[i];
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Additional
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Available internal HAL pixel format
    pCaps->numInternalPixelFormats = NumSupportedInternalPixelFormats;
    for (UINT8 i = 0; i < pCaps->numInternalPixelFormats; i++)
    {
        pCaps->internalPixelFormats[i] = SupportedInternalPixelFormats[i];
    }

    // Max downscale ratio of the pipeline.
    pCaps->maxDownscaleRatio = MaxDownscaleRatio;

    // ISO Modes
    size                              = 0;
    pCaps->ISOAvailableModes[size++]  = ISOModeAuto;
    pCaps->ISOAvailableModes[size++]  = ISOModeDeblur;
    pCaps->ISOAvailableModes[size++]  = ISOMode100;
    pCaps->ISOAvailableModes[size++]  = ISOMode200;
    pCaps->ISOAvailableModes[size++]  = ISOMode400;
    pCaps->ISOAvailableModes[size++]  = ISOMode800;
    pCaps->ISOAvailableModes[size++]  = ISOMode1600;
    pCaps->ISOAvailableModes[size++]  = ISOMode3200;
    CAMX_ASSERT(size <= ISOModeEnd);
    pCaps->numISOAvailableModes       = size;

    // JPEG Quantization Tables
    Utils::Memcpy(pCaps->defaultJPEGQuantTableLuma, DefaultJPEGQuantTableLuma, sizeof(pCaps->defaultJPEGQuantTableLuma));
    Utils::Memcpy(pCaps->defaultJPEGQuantTableChroma, DefaultJPEGQuantTableChroma, sizeof(pCaps->defaultJPEGQuantTableChroma));

    // Sharpness Range
    pCaps->sharpnessRange.minValue        = MinSharpness;
    pCaps->sharpnessRange.maxValue        = MaxSharpness;
    pCaps->sharpnessRange.step            = (MaxSharpness - MinSharpness) / TotalSharpnessLevels;
    pCaps->sharpnessRange.defValue        = DefaultSharpness;

    // Contrast Range
    pCaps->ltmContrastRange.min           = MinContrast;
    pCaps->ltmContrastRange.max           = MaxContrast;

    // Histogram
    pCaps->histogramBuckets               = HistogramBuckets;
    pCaps->histogramCount                 = HistogramMaxCount;

    // Available Instant AEC mode values
    size                                    = 0;
    pCaps->instantAecAvailableModes[size++] = InstantAecNormalConvergence;
    pCaps->instantAecAvailableModes[size++] = InstantAecAggressiveConvergence;
    pCaps->instantAecAvailableModes[size++] = InstantAecFastConvergence;
    CAMX_ASSERT(size <= InstantAecEnd);
    pCaps->numInstantAecModes               = size;

    // Color Temperature Range
    pCaps->colorTemperatureRange.minValue    = MinColorTemperature;
    pCaps->colorTemperatureRange.maxValue    = MaxColorTemperature;

    // White Balance Gains Range
    pCaps->whiteBalanceGainsRange.minValue = MinWBGain;
    pCaps->whiteBalanceGainsRange.maxValue = MaxWBGain;

    // Fastshutter support on device
    switch (CSLPlatform.socId)
    {
        case CSLCameraTitanSocSM7150:
        case CSLCameraTitanSocSM7150P:
            pCaps->sensorModeFastShutter        = TRUE;
            break;
        default:
            pCaps->sensorModeFastShutter        = FALSE;
            break;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Platform Capabilities
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // ICA Capabilities
    switch (CSLPlatform.socId)
    {
        case CSLCameraTitanSocSM6150:
            pCaps->IPEICACapability.supportedIPEICATransformType   = ReferenceTransform;
            pCaps->IPEICACapability.IPEICATransformGridSize        = IPEICATitan150GridSize;
            break;
        case CSLCameraTitanSocSDM855:
        case CSLCameraTitanSocSDM855P:
        case CSLCameraTitanSocSM7150:
        case CSLCameraTitanSocSM7150P:
            pCaps->IPEICACapability.supportedIPEICATransformType   = DefaultTransform;
            pCaps->IPEICACapability.IPEICATransformGridSize        = IPEICATitan175GridSize;
            break;
        case CSLCameraTitanSocSDM845:
        default:
            pCaps->IPEICACapability.supportedIPEICATransformType   = DefaultTransform;
            pCaps->IPEICACapability.IPEICATransformGridSize        = IPEICATitan170GridSize;
            break;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Pixel Clock values for HW blocks.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // For BPS
    // calculate the max processing clock from BPS in Nom.
    pCaps->maxBPSProcessingClockNom = static_cast<UINT>((MaxBPSPixelClockNom * BPSClockEfficiency) / (BPSClockOverhead));

    // For IPE
    // calculate the max processing clock from IPE for single IPE.
    // IPEClockEfficiency was for two IPEs.
    pCaps->maxIPEProcessingClockNom   =
        static_cast<UINT>((MaxSingleIPEPixelClockNom * (IPEClockEfficiency/2)) /(IPEClockOverhead));
    pCaps->maxIPEProcessingClockSvsL1 =
        static_cast<UINT>((MaxSingleIPEPixelClockSvsL1 * (IPEClockEfficiency/2)) /(IPEClockOverhead));

    CSLCameraTitanChipVersion titanChipVersion = Titan17xContext::GetTitanChipVersion();

    // For IFE
    switch (titanChipVersion)
    {
        case CSLCameraTitanChipVersion::CSLTitan150V100:
            pCaps->maxIFELowSVSClock     = MaxIFETitan150LowSVSClock;
            pCaps->maxIFESVSClock        = MaxIFETitan150SVSClock;
            pCaps->maxIFESVSL1Clock      = MaxIFETitan150SVSL1Clock;
            pCaps->maxIFENOMClock        = MaxIFETitan150NOMClock;
            pCaps->maxIFETURBOClock      = MaxIFETitan150TURBOClock;
            pCaps->minIFEHWClkRate       = MaxIFETitan150SVSClock;
            break;
        case CSLCameraTitanChipVersion::CSLTitan170V100:
        case CSLCameraTitanChipVersion::CSLTitan170V110:
        case CSLCameraTitanChipVersion::CSLTitan170V120:
            pCaps->maxIFELowSVSClock     = MaxIFETitan170LowSVSClock;
            pCaps->maxIFESVSClock        = MaxIFETitan170SVSClock;
            pCaps->maxIFESVSL1Clock      = MaxIFETitan170SVSL1Clock;
            pCaps->maxIFENOMClock        = MaxIFETitan170NOMClock;
            pCaps->maxIFETURBOClock      = MaxIFETitan170TURBOClock;
            pCaps->minIFEHWClkRate       = MaxIFETitan170SVSClock;
            break;
        case CSLCameraTitanChipVersion::CSLTitan175V100:
            pCaps->maxIFELowSVSClock     = MaxIFETitan175V1LowSVSClock;
            pCaps->maxIFESVSClock        = MaxIFETitan175V1SVSClock;
            pCaps->maxIFESVSL1Clock      = MaxIFETitan175V1SVSL1Clock;
            pCaps->maxIFENOMClock        = MaxIFETitan175V1NOMClock;
            pCaps->maxIFETURBOClock      = MaxIFETitan175V1TURBOClock;
            pCaps->minIFEHWClkRate       = MaxIFETitan175V1LowSVSClock;
            break;
        case CSLCameraTitanChipVersion::CSLTitan175V101:
            pCaps->maxIFELowSVSClock     = MaxIFETitan175V2LowSVSClock;
            pCaps->maxIFESVSClock        = MaxIFETitan175V2SVSClock;
            pCaps->maxIFESVSL1Clock      = MaxIFETitan175V2SVSL1Clock;
            pCaps->maxIFENOMClock        = MaxIFETitan175V2NOMClock;
            pCaps->maxIFETURBOClock      = MaxIFETitan175V2TURBOClock;
            pCaps->minIFEHWClkRate       = MaxIFETitan175V2LowSVSClock;
            break;
        case CSLCameraTitanChipVersion::CSLTitan175V120:
            pCaps->maxIFELowSVSClock     = MaxIFETitan175V3LowSVSClock;
            pCaps->maxIFESVSClock        = MaxIFETitan175V3SVSClock;
            pCaps->maxIFESVSL1Clock      = MaxIFETitan175V3SVSL1Clock;
            pCaps->maxIFENOMClock        = MaxIFETitan175V3NOMClock;
            pCaps->maxIFETURBOClock      = MaxIFETitan175V3TURBOClock;
            pCaps->minIFEHWClkRate       = MaxIFETitan175V3LowSVSClock;
            break;
        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Unsupported Titan version %u", titanChipVersion);
            CAMX_LOG_WARN(CamxLogGroupHWL, "Unsupported Titan version %u", titanChipVersion);
            result = CamxResultENotImplemented;
            break;
    }


    // Number of IPEs are different for different SOCs,
    // For SDM845 SOC Id it is 2
    // For SDM670 SOC Id and SM6150 SOC Id it is 1
    if (CamxResultSuccess == result)
    {
        if ((CSLCameraTitanSocSDM670 == CSLPlatform.socId) ||
            (CSLCameraTitanSocSM6150 == CSLPlatform.socId))
        {
            numIPEs = 1;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHWL, "Invalid Soc ");
    }
    pCaps->maxIPEProcessingClockNom   = pCaps->maxIPEProcessingClockNom * numIPEs;
    pCaps->maxIPEProcessingClockSvsL1 = pCaps->maxIPEProcessingClockSvsL1 * numIPEs;

    CAMX_LOG_INFO(CamxLogGroupHWL, "maxBPSProcessingClockNom = %d maxIPEProcessingClockNom = %d"
        "maxIPEProcessingClockSvsL1 = %d",
        pCaps->maxBPSProcessingClockNom, pCaps->maxIPEProcessingClockNom, pCaps->maxIPEProcessingClockSvsL1);

    // Init to defult then get supported SOC format
    switch (CSLPlatform.socId)
    {
        case CSLCameraTitanSocSDM855:
        case CSLCameraTitanSocSDM855P:
            pCaps->ubwcVersion               = UBWCver3;
            pCaps->ubwcLossyPreviewSupported = FALSE;
            pCaps->ubwcLossyPreviewMinWidth  = UINT_MAX;
            pCaps->ubwcLossyPreviewMinHeight = UINT_MAX;
            pCaps->ubwcLossyVideoSupported   = TRUE;
            pCaps->ubwcLossyVideoMinWidth    = 3840;
            pCaps->ubwcLossyVideoMinHeight   = 2160;
            break;
        case CSLCameraTitanSocSM7150:
        case CSLCameraTitanSocSM7150P:
            pCaps->ubwcVersion               = UBWCver2;
            pCaps->ubwcLossyPreviewSupported = FALSE;
            pCaps->ubwcLossyPreviewMinWidth  = UINT_MAX;
            pCaps->ubwcLossyPreviewMinHeight = UINT_MAX;
            pCaps->ubwcLossyVideoSupported   = TRUE;
            pCaps->ubwcLossyVideoMinWidth    = 3840;
            pCaps->ubwcLossyVideoMinHeight   = 2160;
            break;
        default:
            pCaps->ubwcVersion               = UBWCver2;
            pCaps->ubwcLossyPreviewSupported = FALSE;
            pCaps->ubwcLossyPreviewMinWidth  = UINT_MAX;
            pCaps->ubwcLossyPreviewMinHeight = UINT_MAX;
            pCaps->ubwcLossyVideoSupported   = FALSE;
            pCaps->ubwcLossyVideoMinWidth    = UINT_MAX;
            pCaps->ubwcLossyVideoMinHeight   = UINT_MAX;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::GetHWBugWorkarounds
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Titan17xContext::GetHWBugWorkarounds(
    HWBugWorkarounds* pWorkarounds)
{
    CAMX_ASSERT (NULL != pWorkarounds);

    pWorkarounds->numWorkarounds    = Titan17xNumWorkarounds;
    pWorkarounds->pWorkarounds      = g_pTitan17xWorkarounds;

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::GetNumberOfIPE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 Titan17xContext::GetNumberOfIPE()
{
    CSLCameraPlatform CSLPlatform = {};
    CamxResult        result      = CamxResultSuccess;
    UINT32            numIPE      = 0;

    result = CSLQueryCameraPlatform(&CSLPlatform);

    if (CamxResultSuccess == result)
    {
        if (CSLPlatform.CPASHardwareCaps & CPAS_TOP_CPAS_0_HW_CAPABILITY_IPE_0_MASK)
        {
            numIPE++;
        }

        if (CSLPlatform.CPASHardwareCaps & CPAS_TOP_CPAS_0_HW_CAPABILITY_IPE_1_MASK)
        {
            numIPE++;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupCore, "Titan17xContext returned invalid IPE number");
    }

    return numIPE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::InitializeSOCDependentParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Titan17xContext::InitializeSOCDependentParams()
{
    UINT32 socID                           = this->GetCameraSoc();
    UBWCModeConfig1  UBWCConfig1           = { 0, 0 };
    UBWCModeConfig1  UBWCConfig2           = { 0, 0 };
    UBWCPlaneModeConfig UBWCPlaneConfig    = { NapaliUBWCHighestBankValue, 0, 1, 1 };
    BOOL  UBWCSupportedScaleRatioIsLimited = TRUE;

    if ((CSLCameraTitanSocSDM670  == socID) ||
        (CSLCameraTitanSocQCS605  == socID) ||
        (CSLCameraTitanSocSM6150  == socID) ||
        (CSLCameraTitanSocSDM710  == socID) ||
        (CSLCameraTitanSocSXR1120 == socID) ||
        (CSLCameraTitanSocSXR1130 == socID) ||
        (CSLCameraTitanSocSDM712  == socID) ||
        (CSLCameraTitanSocSM7150  == socID) ||
        (CSLCameraTitanSocSM7150P == socID))
    {
        UBWCPlaneConfig.highestValueBit = NonNaplaiUBWCHighestBankValue;
    }
    ImageFormatUtils::SetUBWCModeConfig(&UBWCPlaneConfig);
    switch (socID)
    {
        case CSLCameraTitanSocSDM845:
            // UBWC 2.0 don't support lossy/lossless
            UBWCConfig1.UBWCVersion = 0;
            // UBWC 130B limitation
            UBWCSupportedScaleRatioIsLimited = TRUE;
            break;
        case CSLCameraTitanSocSDM855:
        case CSLCameraTitanSocSDM855P:
            if (FALSE == GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->disableUBWCv3)
            {
                UBWCConfig1.lossyMode   = 0;
                UBWCConfig1.UBWCVersion = 1;

                // Use UBWC 3.0 for external outputs as well
                UBWCConfig2.lossyMode   = 0;
                UBWCConfig2.UBWCVersion = 1;
            }
            // No Limitation w.r.t UBWC Buffer
            UBWCSupportedScaleRatioIsLimited = FALSE;
            break;
        case CSLCameraTitanSocSM7150:
        case CSLCameraTitanSocSM7150P:
            if (FALSE == GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->disableUBWCv3)
            {
                // Use UBWC 3.0 for internal ports
                UBWCConfig1.lossyMode   = 0;
                UBWCConfig1.UBWCVersion = 1;
            }
            // No Limitation w.r.t UBWC Buffer
            UBWCSupportedScaleRatioIsLimited = FALSE;
            break;
        case CSLCameraTitanSocSM6150:
            // No Limitation w.r.t UBWC Buffer
            UBWCSupportedScaleRatioIsLimited = FALSE;
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupCore, "Invalid ubwc version");
    }

    ImageFormatUtils::SetUBWCLimitation(UBWCSupportedScaleRatioIsLimited);
    CAMX_LOG_INFO(CamxLogGroupCore, "Initializing ubwc version %d:0=>UBWC 2.0,1=>UBWC 3.0",
        UBWCConfig1.UBWCVersion);
    ImageFormatUtils::SetUBWCModeConfig1(&UBWCConfig1);
    ImageFormatUtils::SetUBWCModeConfig2(&UBWCConfig2);

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::Titan17xContext
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Titan17xContext::Titan17xContext()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::~Titan17xContext
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Titan17xContext::~Titan17xContext()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::HwInitialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult Titan17xContext::HwInitialize()
{
    CamxResult result = CamxResultSuccess;

    result = InitializeSOCDependentParams();

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Titan17xContext::GetStatsParser
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
StatsParser* Titan17xContext::GetStatsParser()
{
    return Titan17xStatsParser::GetInstance();
}

CAMX_NAMESPACE_END
