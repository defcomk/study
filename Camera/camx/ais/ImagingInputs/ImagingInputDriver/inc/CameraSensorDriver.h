#ifndef __CAMERASENSORDRIVER_H_
#define __CAMERASENSORDRIVER_H_

/**
 * @file CameraSensorDriver.h
 *
 * @brief This file contains constants and interface definitions for the
 * camera sensor driver
 *
 * Copyright (c) 2009-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#include "AEEStdDef.h"

#include "CameraResult.h"
#include "CameraCommonTypes.h"

#include "sensor_lib.h"

/* ===========================================================================
                        SHARED DATA DECLARATIONS
=========================================================================== */
#define INVALID_FOCUS_PARAM 0xFFFF

/* ===========================================================================
                        DATA DECLARATIONS
=========================================================================== */

/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */

/**********************************************************
 ** Camera Sensor Driver Class IDs
 *********************************************************/
/// This is the first of 5 class IDs to be used by OEM sensor drivers. The
/// client will attempt to create an instance of each of the corresponding 5
/// classes IDs. If the creation fails for a particular ID, the client will
/// ignore that ID. If the creation succeeds, the ID is added to the list of
/// available sensors.
#define Camera_Sensor_AEECLSID_COEMCameraSensorDriver1 0x1061014
/// This is the second of the 5 OEM camera sensor class IDs.
#define Camera_Sensor_AEECLSID_COEMCameraSensorDriver2 0x1061015
/// This is the third of the 5 OEM camera sensor class IDs.
#define Camera_Sensor_AEECLSID_COEMCameraSensorDriver3 0x1061016
/// This is the fourth of the 5 OEM camera sensor class IDs.
#define Camera_Sensor_AEECLSID_COEMCameraSensorDriver4 0x1061017
/// This is the fifth of the 5 OEM camera sensor class IDs.
#define Camera_Sensor_AEECLSID_COEMCameraSensorDriver5 0x1061018

/// This iS the maximum number of pixel and RDI interface CID
/// supported per sensor
#define Camera_Sensor_MAX_PIXEL_RDI_CID_PER_SENSOR 16

#define Camera_Sensor_MAX_NUMBER_OF_EXPOSURES 11

#define Camera_Sensor_EXPOSURE_BRACKETING_INDEX_INIT 1

#define Camera_Sensor_MAX_PDAF_WINDOW 200

#define MAX_NUMBER_OF_CHANNELS_PER_SENSOR_MODE Camera_Sensor_MAX_PIXEL_RDI_CID_PER_SENSOR

/* ---------------------------------------------------------------------------
** Type Declarations
** ------------------------------------------------------------------------ */
/**********************************************************
 ** Camera Sensor Driver Control IDs
 *********************************************************/
typedef enum
{
    /// This is the control ID for powering on the sensor.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_STATE_POWER_ON,

    /// This is the control ID for powering off the sensor.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_STATE_POWER_OFF,

    /// This is the control ID for suspending power to sensor.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_STATE_POWER_SUSPEND,

    /// This is the control ID for resuming power to sensor.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_STATE_POWER_RESUME,

    /// This is the control ID for starting frame output from the sensor.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_STATE_FRAME_OUTPUT_START,

    /// This is the control ID for stopping frame output from the sensor.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_STATE_FRAME_OUTPUT_STOP,

    /// This is the control ID for resetting the sensor to its POR state.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_STATE_RESET,

    /// This is the control ID for the Chromatix retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   Camera_Sensor_ChromatixConfigType
    /// @par Output Expected:
    ///   Pointer to chromatix_parms_type structure
    Camera_Sensor_AEEUID_CTL_INFO_CHROMATIX,

    /// This is the control ID for the extended Chromatix retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   Camera_Sensor_FutureChromatixConfigType
    /// @par Output Expected:
    ///   Pointer to chromatix_parms_type structure
    Camera_Sensor_AEEUID_CTL_INFO_FUTURE_CHROMATIX,

    /// This is the control ID for the embedded data retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_EmbeddedDataInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_EMBEDDED_DATA,

    /// This is the control ID for the pahse difference info data retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_PhaseDifferenceDataInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_PHASE_DIFFERENCE_DATA,

    // Static configuration information Control IDs
    /// This is the control ID for the feature retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_FeaturesInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_FEATURES,

    /// This is the control ID to retrieve focal length.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_CameraFocalLengthType
    Camera_Sensor_AEEUID_CTL_INFO_FOCAL_LENGTH,

    /// This is the control ID for the output format retrieval structure.
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to Size structure with the resolution that will be set in the
    ///   future
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_FutureOutputInfoType structure with the output
    ///   values that would be used if the resolution was changed to the given
    ///   resolution in the immediate future
    Camera_Sensor_AEEUID_CTL_INFO_FUTURE_OUTPUT,

    /// This is the control ID for the identification retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_IdentificationInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_IDENTIFICATION,

    /// This is the control ID for the interface info retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_InterfaceInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_INTERFACE,

    /// This is the control ID for the ISPIF info retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_InterfaceInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_ISPIF,

    /// This is the control ID for the mechanical retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_MechanicalInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_MECHANICAL,

    /// This is the control ID for the orientation retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_OrientationInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_ORIENTATION,

    /// This is the control ID for the output format retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_OutputFormatInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_OUTPUT_FORMAT,

    /// This is the control ID for the output rate retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_OutputRateInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_OUTPUT_RATE,

    /// This is the control ID for the output resolution retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_OutputResolutionInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_OUTPUT_RESOLUTION,

    /// This is the control ID for the output synchronization retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_OutputSynchronizationInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_OUTPUT_SYNCHRONIZATION,

    /// This is the control ID to retrieve sensor calibration data.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer only
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to a SensorCalibrationInfoType structure with the sensor
    ///   calibration info.
    Camera_Sensor_AEEUID_CTL_INFO_SENSOR_CALIBRATION,

    /// This is the control ID to retrieve sensor OTP data.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer only
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to a OTP data dump buffer
    Camera_Sensor_AEEUID_CTL_INFO_SENSOR_OTP_DATA,

    /// This is the control ID for the statistics retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_StatisticsInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_STATISTICS,

    /// This is the control ID to retrieve subject distance.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer only
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_SubjectDistanceRangeInfoType
    Camera_Sensor_AEEUID_CTL_INFO_SUBJECT_DISTANCE_RANGE,

    /// This is the control ID for the supported sensor modes retrieval array.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   A value of type Camera_Sensor_OperationalModeType
    /// @par Output Expected:
    ///   Pointer to array of Camera_Sensor_SupportedSensorModesInfoType
    ///   structure
    Camera_Sensor_AEEUID_CTL_INFO_SUPPORTED_SENSOR_MODES,

    /// This is the control ID for retrieving sensor focus data from driver.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   Pointer to Camera_Sensor_SensorFocusInfoType structure with entry
    ///   type specified.
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_SensorFocusInfoType structure with entry
    ///   value set in driver.
    Camera_Sensor_AEEUID_CTL_INFO_SENSOR_FOCUS,

    /// This is the control ID for querying if crop mode change is avaialble
    /// from the driver
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   Pointer to Camera_Sensor_ZoomConfigType structure with the values to
    ///   set zoom to
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_CropModeChangeNeededInfoType structure with
    ///   the flag to get if crop mode change is needed.
    Camera_Sensor_AEEUID_CTL_INFO_CROP_MODE_CHANGE_NEEDED,

    /// This is the control ID to retrieve focus bracketing info.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_FocusBracketingInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_FOCUS_BRACKETING,

    /// This is the control ID to determine number of frames to skip between exposure
    /// updates for the particular sensor
    /// @par Input Expected:
    ///   Pointer to Camera_Sensor_ExposureUpdateMethodConfigType  indicating the type of
    ///   exposure update.
    /// @par Output Expected
    ///   Pointer to a Camera_Sensor_AEEUID_CTL_INFO_EXPOSURE_BRACKETING indicating the number
    ///   of frames to skip.
    Camera_Sensor_AEEUID_CTL_INFO_EXPOSURE_BRACKETING,

    /// This is the control ID to determine the callback for bracketing.
    /// @par Input Expected
    /// None
    /// @par Output Expected
    ///   Pointer to the callback.
    Camera_Sensor_AEEUID_CTL_INFO_EXPOSURE_BRACKETING_CALLBACK,


    /// This is the control ID to retrive reference ISP clock rate to switch from single ISP to dual ISP capture (if ISP support dual ISP).
    Camera_Sensor_AEEUID_CTL_INFO_DUAL_ISP_REF_CLOCK_RATE,

    /// This is the control ID to configure the mode
    /// @par Type:
    ///   input
    /// @par Input Expected:
    ///   pointer to mode
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_CONFIG_MODE,

    // Dynamic configuration information Control IDs
    /// This is the control ID for the antibanding configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_AntibandingConfigType structure with the
    ///   values to set antibanding to.
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_AntibandingConfigType structure with the
    ///   current antibanding values.
    Camera_Sensor_AEEUID_CTL_CONFIG_ANTIBANDING,


    // Dynamic configuration information Control IDs
    /// This is the control ID for the antibanding configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_AutoframerateConfigType structure with the
    ///   values to set antibanding to.
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_AutoframerateType structure with the
    ///   current antibanding values.
    Camera_Sensor_AEEUID_CTL_CONFIG_AUTOFRAMERATE,

    /// This is the control ID for the brightness configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_BrightnessConfigType structure with the
    ///   values to set brightness to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_BrightnessConfigType structure with the
    ///   current brightness values.
    Camera_Sensor_AEEUID_CTL_CONFIG_BRIGHTNESS,

    /// This is the control ID for the contrast configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_ContrastConfigType structure with the
    ///   values to set contrast to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_ContrastConfigType structure with the
    ///   current contrast values.
    Camera_Sensor_AEEUID_CTL_CONFIG_CONTRAST,

    /// This is the control ID for the embedded data configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   Pointer to a byte array which holds the embedded data.
    /// @par Output Expected:
    //
    Camera_Sensor_AEEUID_CTL_CONFIG_EMBEDDED_DATA,

    /// This is the control ID for the exposure configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_ExposureConfigType structure with the
    ///   values to set exposure to.
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_ExposureConfigType structure with the
    ///   current exposure values.
    Camera_Sensor_AEEUID_CTL_CONFIG_EXPOSURE,

    /// This is the control ID for configuring exposure based on exposure time.
    /// @ par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input
    /// @par Input Expected :
    ///   Pointer to an Camera_Sensor_ExposureConfigType structure with the
    ///   set of exposure values.
    Camera_Sensor_AEEUID_CTL_CONFIG_EXPOSURE_TIME,

    /// This is the control ID for the exposure compensation configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_ExposureCompensationConfigType structure
    ///   with the values to set exposure compensation to.
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_ExposureCompensationConfigType structure
    ///   with the current exposure compensation values.
    Camera_Sensor_AEEUID_CTL_CONFIG_EXPOSURE_COMPENSATION,

    /// This is the control ID for the luma configuration for HDR.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_LumaForHDRConfigType structure with the
    ///   Luma values to set
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_LumaForHDRConfigType structure with the
    ///   current luma value.
    Camera_Sensor_AEEUID_CTL_CONFIG_LUMA_FOR_HDR,

    /// This is the control ID for the exposure metering configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_ExposureMeteringConfigType structure with
    ///   the values to set exposure metering to.
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_ExposureMeteringConfigType structure with
    ///   the current exposure metering values.
    Camera_Sensor_AEEUID_CTL_CONFIG_EXPOSURE_METERING,

    /// This is the control ID for the flash mode configuration on YUV sensor
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a SnapshotFlashConfigType structure with the values to set
    ///   flash mode to.
    /// @par Output Expected:
    ///   Pointer to a SnapshotFlashConfigType structure with the current flash
    ///   mode type.
    Camera_Sensor_AEEUID_CTL_CONFIG_FLASH_MODE,

    /// This is the control ID for the external focus configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_FocusExternalConfigType structure with the
    ///   values to set focus to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_FocusExternalConfigType structure with the
    ///   current focus values.
    Camera_Sensor_AEEUID_CTL_CONFIG_FOCUS_EXTERNAL,

    /// This is the control ID for configuring sensor focus settings
    /// if QMobiCat is enable. No input or output will be passed.
    /// @par Type:
    ///   None
    /// @par Input Expected:
    ///   Pointer to Camera_Sensor_SensorFocusInfoConfigType structure with values
    ///   and entry type to set to if configuring an entry.   If configuring an operation,
    ///   corresponding mode is specified.
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_CONFIG_SENSOR_FOCUS_INFO,

    /// This is the control ID for the on-sensor focus mode configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_FocusModeOnSensorConfigType structure with
    ///   the values to set focus to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_FocusModeOnSensorConfigType structure with
    ///   the current focus values.
    Camera_Sensor_AEEUID_CTL_CONFIG_FOCUS_MODE_ON_SENSOR,

    /// This is the control ID for the on-sensor focus state configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_FocusStateOnSensorConfigType structure
    ///   with the values to set focus to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_FocusStateOnSensorConfigType structure
    ///   with the current focus values.
    Camera_Sensor_AEEUID_CTL_CONFIG_FOCUS_STATE_ON_SENSOR,

    /// This is the control ID for the frame interval configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_FrameIntervalConfigType structure with
    ///   the values to set frame interval to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_FrameIntervalConfigType structure with
    ///   the current frame interval values.
    Camera_Sensor_AEEUID_CTL_CONFIG_FRAME_INTERVAL,

    /// This is the control ID for the gamma configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_GammaConfigType structure with the values
    ///   to set gamma to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_GammaConfigType structure with the current
    ///   gamma values.
    Camera_Sensor_AEEUID_CTL_CONFIG_GAMMA,

    /// This is the control ID for the hue configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_HueConfigType structure with the values to
    ///   set hue to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_HueConfigType structure with the current
    ///   hue values.
    Camera_Sensor_AEEUID_CTL_CONFIG_HUE,

    /// This is the control ID for the image effect configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_ImageEffectConfigType structure with the
    ///   values to set for an image effect.
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_ImageEffectConfigType structure with the
    ///   current image effect values.
    Camera_Sensor_AEEUID_CTL_CONFIG_IMAGE_EFFECT,

    /// This is the control ID for the image tuning configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_ImageTuningConfigType structure with the
    ///   values to set for image tuning.
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_ImageTuningConfigType structure with the
    ///   current image tuning values.
    Camera_Sensor_AEEUID_CTL_CONFIG_IMAGE_TUNING,

    /// This is the control ID for the ISO configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_ISOConfigType structure with the values
    ///   to set ISO to.
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_ISOConfigType structure with the current
    ///   ISO values.
    Camera_Sensor_AEEUID_CTL_CONFIG_ISO,

    /// This is the control ID for the manual exposure configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YUV
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_ManualExposureConfigType structure
    ///   with the manual snapshot exposure time to use.
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_CONFIG_MANUAL_EXPOSURE,

    /// This is the control ID for the mechanical shutter and aperture.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// configuration.
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_MechanicalConfigType structure with the
    ///   values to set mechanical shutter and aperture to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_MechanicalConfigType structure with the
    ///   current mechanical shutter and aperture values.
    Camera_Sensor_AEEUID_CTL_CONFIG_MECHANICAL,

    /// This is the control ID for the optical zoom configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to an Camera_Sensor_OpticalZoomConfigType structure with the
    ///   values to set optical zoom to.
    /// @par Output Expected:
    ///   Pointer to an Camera_Sensor_OpticalZoomConfigType structure with the
    ///   current optical zoom values.
    Camera_Sensor_AEEUID_CTL_CONFIG_OPTICAL_ZOOM,

    /// This is the control ID for the regions of interest configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_RegionsOfInterestConfigType structure
    ///   with the regions of interest to use.
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_CONFIG_REGIONS_OF_INTEREST,

    /// This is the control ID for the resolution configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_ResolutionConfigType structure with the
    ///   values to set resolution to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_ResolutionConfigType structure with the
    ///   current resolution values.
    Camera_Sensor_AEEUID_CTL_CONFIG_RESOLUTION,

    /// This is the control ID to query field type.
    /// @par Sensor Type Supporting This Configuration:
    ///   CVBS
    /// @par Type:
    ///   Output
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_FieldType structure with the
    ///   current field values and timestamp.
    Camera_Sensor_AEEUID_CTL_QUERY_FIELD,

    /// This is the control ID for the saturation configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_SaturationConfigType structure with the
    ///   values to set saturation to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_SaturationConfigType structure with the
    ///   current saturation values.
    Camera_Sensor_AEEUID_CTL_CONFIG_SATURATION,

    /// This is the control ID for the self-test configuration. The result of the
    /// self-test will be passed in the output buffer as a CameraResult.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_SelfTestConfigType structure with the
    ///   type of self test to do.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_SelfTestConfigType structure with the
    ///   type of self test to do.
    Camera_Sensor_AEEUID_CTL_CONFIG_SELF_TEST,

    /// This is the control ID for the sharpness configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_SharpnessConfigType structure with the
    ///   values to set sharpness to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_SharpnessConfigType structure with the
    ///   current sharpness values.
    Camera_Sensor_AEEUID_CTL_CONFIG_SHARPNESS,

    /// This is the control ID for the white balance configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_WhiteBalanceConfigType structure with the
    ///   values to set white balance to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_WhiteBalanceConfigType structure with the
    ///   current white balance values.
    Camera_Sensor_AEEUID_CTL_CONFIG_WHITE_BALANCE,

    /// This is the control ID for the Sensor Camera ID info.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer YCbCr
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///  None
    /// @par Output Expected:
    ///  Pointer to a CameraIDType structure with the ID values
    Camera_Sensor_AEEUID_CTL_INFO_CAMERA_ID,

    /// This is the control ID for the test pattern configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   Pointer to a TestPatternConfigType structure with the values to set
    ///   white balance to.
    /// @par Output Expected:
    ///   Pointer to a TestPatternConfigType structure with the current white
    ///   balance values.
    Camera_Sensor_AEEUID_CTL_CONFIG_TEST_PATTERN,

    /// This is the control ID for the AF FTM mode configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   Bayer YCbCr
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   Pointer to a AF FTM test Type structure with the values to set different FTM mode
    /// @par Output Expected:
    ///  None
    Camera_Sensor_AEEUID_CTL_CONFIG_AF_FTM_MODE,

    /// This is the control ID for the zoom configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   bayer and YCbCr
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_ZoomConfigType structure with the
    ///   values to set zoom to.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_ZoomConfigType structure with the
    ///   current zoom config values
    Camera_Sensor_AEEUID_CTL_CONFIG_ZOOM,

    /// This is the control ID for the focus brackeitng configureation.
    /// @ par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input
    /// @par Input Expected :
    ///   Pointer to an FocusBracketingConfigType structure with the
    ///   focus bracketing data.
    Camera_Sensor_AEEUID_CTL_CONFIG_FOCUS_BRACKETING,

    /// This is the control ID for the exposure brackeitng configureation.
    /// @ par Sensor Type Supporting This Configuration:
    ///   Bayer and YCbCr
    /// @par Type:
    ///   Input
    /// @par Input Expected :
    ///   Pointer to an BracketingExposureConfigType structure with the
    ///   set of exposure values.
    Camera_Sensor_AEEUID_CTL_CONFIG_EXPOSURE_BRACKETING,

    /// This is the control ID for starting bracketing.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_START_BRACKETING,

    /// This is the control ID for aborting bracketing.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_ABORT_BRACKETING,

    /// This is the control ID for Type 1 PD stat parsing.
    /// @par Type:
    ///   Input
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
	Camera_Sensor_AEEUID_CTL_PARSE_PD_STATS,

    /// This is the control ID to configure the optimization hint information
    /// @par Type:
    ///   input
    /// @par Input Expected:
    ///   pointer to Camera_Sensor_OptimizationHintType
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_CONFIG_OPTIMIZATION_HINT,

    /// This is the control ID for the HDR stats info structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_HdrStatsInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_HDR_STATS,

    /// This is the control ID for processing HDR stats.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_HdrStatsInfoType structure
    Camera_Sensor_AEEUID_CTL_PROCESS_HDR_STATS,

    /// Control ID for video capture resolution
    Camera_Sensor_AEEUID_CTL_CONFIG_VIDEO_CAPTURE_RESOLUTION,

    /// Control ID for OIS mode configuration
    Camera_Sensor_AEEUID_CTL_CONFIG_OIS_MODE,

    /// This is the control ID for the Common AF Actuator retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_OutputAFActuatorType structure
    Camera_Sensor_AEEUID_CTL_INFO_AF_ACTUATOR,

    /// This is the control ID for the Common AF Actuator retrieval structure.
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_OutputAFSensorType structure
    Camera_Sensor_AEEUID_CTL_INFO_SENSOR,

    /// This is the control ID for the pdaf configureation.
    /// @ par Type:
    ///   Output
    /// @par Type:
    ///   Input
    /// @par Input Expected :
    ///   Pointer to PDAF configuration
    Camera_Sensor_AEEUID_CTL_CONFIG_PDAF,

    /// This is the control ID for the pdaf configuration.
    /// @ par Type:
    ///   Output
    /// @par Type:
    ///   Input
    /// @par Input Expected :
    ///   Pointer to PDAF configuration
    Camera_Sensor_AEEUID_CTL_CONFIG_PDAF_LIB,

    /// This is the control ID for detecting device
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_DETECT_DEVICE,

    /// This is the control ID for detecting device Channels
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   None
    Camera_Sensor_AEEUID_CTL_DETECT_DEVICE_CHANNELS,

    /// This is the control ID for retrieving Channels information
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_ChannelsType structure
    Camera_Sensor_AEEUID_CTL_INFO_CHANNELS,

    /// This is the control ID for retrieving Channels information
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_SubchannelsInfoType structure
    Camera_Sensor_AEEUID_CTL_INFO_SUBCHANNELS,

    /// This is the control ID for retrieving csi information
    /// @par Type:
    ///   Output
    /// @par Input Expected:
    ///   None
    /// @par Output Expected:
    ///   Pointer to Camera_Sensor_MipiCsiInfoType structure
    Camera_Sensor_AEEUID_CTL_CSI_INFO_PARAMS,

    /// This is the control ID for param configuration
    /// such as exposure, saturation, hue, configuration.
    /// @par Sensor Type Supporting This Configuration:
    ///   YCbCr only
    /// @par Type:
    ///   Input/Output
    /// @par Input Expected:
    ///   Pointer to a Camera_Sensor_ParamConfigType structure with the
    ///   values to be set.
    /// @par Output Expected:
    ///   Pointer to a Camera_Sensor_ParamConfigType structure with the
    ///   current  values.
    Camera_Sensor_AEEUID_CTL_CONFIG_SENSOR_PARAMS,

    /// This forces the enumerated type to be 32-bits
    Camera_Sensor_AEEUID_CTL_SENSOR_DRIVER_ID_MAX = 0x7FFFFFFF
} SensorDriverControlID;

/**********************************************************
 ** Control Structure Enumerated Types
 *********************************************************/
/**
 * This enumerates the possible antibanding modes.
 */
typedef enum
{
    Camera_Sensor_ANTIBANDING_MODE_INVALID, ///< Uninitialized value
    Camera_Sensor_ANTIBANDING_MODE_OFF,     ///< No antibanding used
    Camera_Sensor_ANTIBANDING_MODE_AUTO,    ///< Sensor's antibanding algorithm used
    Camera_Sensor_ANTIBANDING_MODE_MANUAL   ///< User-specified antibanding values used
} Camera_Sensor_AntibandingModeType;

/**
 * This enumerates the possible Optical Image Stalization (OIS) modes.
 */
typedef enum
{
    Camera_Sensor_OIS_OFF,                  /// OIS disabled
    Camera_Sensor_OIS_STILL_MODE,           /// STILL mode (only for snapshot)
    Camera_Sensor_OIS_MOVIE_MODE            /// MOVIE mode
} Camera_Sensor_OISModeType;

/**
 * This enumerates the auto exposure control sources.
 */
typedef enum
{
    Camera_Sensor_AUTO_EXPOSURE_CONTROL_INVALID,
    Camera_Sensor_AUTO_EXPOSURE_CONTROL_IN_SENSOR, ///< Auto-exposure performed in sensor
    Camera_Sensor_AUTO_EXPOSURE_CONTROL_EXTERNAL  ///< Auto-exposure performed externally
} Camera_Sensor_AutoExposureControlType;

/**
 * This enumerates the exposure mode.
 */
typedef enum
{
    Camera_Sensor_AUTO_EXPOSURE,  ///< Auto exposure mode
    Camera_Sensor_MANUAL_EXPOSURE ///< Manual exposure mode
} Camera_Sensor_ExposureModeType;

/**
 * This enumerates the auto-flash synchronization source.
 */
typedef enum
{
    Camera_Sensor_AUTO_FLASH_SYNC_INVALID,
    Camera_Sensor_AUTO_FLASH_SYNC_IN_SENSOR, ///< Auto-flash sync performed in sensor
    Camera_Sensor_AUTO_FLASH_SYNC_EXTERNAL   ///< Auto-flash sync performed externally
} Camera_Sensor_AutoFlashSyncType;

/**
 * This enumerates the auto-flicker compensation sources.
 */
typedef enum
{
    Camera_Sensor_AUTO_FLICKER_COMPENSATION_INVALID,
    Camera_Sensor_AUTO_FLICKER_COMPENSATION_IN_SENSOR, ///< Auto-flicker compensation performed in sensor
    Camera_Sensor_AUTO_FLICKER_COMPENSATION_EXTERNAL   ///< Auto-flicker compensation performed externally
} Camera_Sensor_AutoFlickerCompensationType;

/**
 * This enumerates the auto-flicker detection sources.
 */
typedef enum
{
    Camera_Sensor_AUTO_FLICKER_DETECTION_INVALID,
    Camera_Sensor_AUTO_FLICKER_DETECTION_IN_SENSOR, ///< Auto-flicker detection performed in sensor
    Camera_Sensor_AUTO_FLICKER_DETECTION_EXTERNAL   ///< Auto-flicker detection performed externally
} Camera_Sensor_AutoFlickerDetectionType;

/**
 * This enumerates the auto-focus control sources.
 */
typedef enum
{
    Camera_Sensor_AUTO_FOCUS_CONTROL_INVALID,
    Camera_Sensor_AUTO_FOCUS_CONTROL_NOT_SUPPORTED, ///< Auto-focus not supported by sensor
    Camera_Sensor_AUTO_FOCUS_CONTROL_IN_SENSOR,     ///< Auto-focus performed exclusively in sensor
    Camera_Sensor_AUTO_FOCUS_CONTROL_IN_SEN_OR_EXT, ///< Auto-focus performed either in sensor or externally
    Camera_Sensor_AUTO_FOCUS_CONTROL_EXTERNAL       ///< Auto-focus performed exclusively externally
} Camera_Sensor_AutoFocusControlType;

/**
 * This enumerates auto-frame rate control sources.
 */
typedef enum
{
    Camera_Sensor_AUTO_FRAME_RATE_CONTROL_INVALID,
    Camera_Sensor_AUTO_FRAME_RATE_CONTROL_IN_SENSOR, ///< Auto-frame rate performed in sensor
    Camera_Sensor_AUTO_FRAME_RATE_CONTROL_EXTERNAL    ///< Auto-frame rate performed externally
} Camera_Sensor_AutoFrameRateControlType;

/**
 * This enumerates the auto-white balance control sources.
 */
typedef enum
{
    Camera_Sensor_AUTO_WHITE_BALANCE_CONTROL_INVALID,
    Camera_Sensor_AUTO_WHITE_BALANCE_CONTROL_IN_SENSOR, ///< Auto-white balance performed in sensor
    Camera_Sensor_AUTO_WHITE_BALANCE_CONTROL_EXTERNAL   ///< Auto-white balance performed externally
} Camera_Sensor_AutoWhiteBalanceControlType;

/**
 * This enumerates the chroma location modes.
 */
typedef enum
{
    Camera_Sensor_CHROMA_LOCATION_INVALID,
    Camera_Sensor_CHROMA_LOCATION_COSITED,     ///< Chroma samples co-sited with luma samples
    Camera_Sensor_CHROMA_LOCATION_INTERSTITIAL ///< Chroma samples in between luma samples
} Camera_Sensor_ChromaLocationType;

/**
 * This enumerates the continuous-focus control sources.
 */
typedef enum
{
    Camera_Sensor_CONTINUOUS_FOCUS_CONTROL_INVALID,
    Camera_Sensor_CONTINUOUS_FOCUS_CONTROL_NOT_SUPPORTED, ///< Continuous-focus not supported by sensor
    Camera_Sensor_CONTINUOUS_FOCUS_CONTROL_IN_SENSOR,     ///< Continuous-focus performed exclusively in sensor
    Camera_Sensor_CONTINUOUS_FOCUS_CONTROL_IN_SEN_OR_EXT, ///< Continuous-focus performed either in sensor or externally
    Camera_Sensor_CONTINUOUS_FOCUS_CONTROL_EXTERNAL       ///< Continuous-focus performed exclusively externally
} Camera_Sensor_ContinuousFocusControlType;

/**
 * This enumerates the possible CSI DT.
 */
typedef enum
{
    Camera_Sensor_CSI_DT_INVALID,       ///< Uninitialized value
    Camera_Sensor_CSI_DT_RAW6,          ///< Raw6
    Camera_Sensor_CSI_DT_RAW8,          ///< Raw8
    Camera_Sensor_CSI_DT_RAW10,         ///< Raw10
    Camera_Sensor_CSI_DT_RAW12,         ///< Raw12
    Camera_Sensor_CSI_DT_RGB888,        ///< RGB888
    Camera_Sensor_CSI_DT_EMBEDDED_DATA, ///< Embedded data
    Camera_Sensor_CSI_DT_JPGE_SOC,      ///< JPEG SOC
    Camera_Sensor_CSI_DT_USER_DEFINED,  ///< User Defined Data Type
    Camera_Sensor_CSI_DT_HDR_STATS,     ///< HDR Stat
    Camera_Sensor_CSI_DT_PD_STATS,      ///< PDAF Stat
    Camera_Sensor_CSI_DT_YUV422_8,      ///< YUV422 8
    Camera_Sensor_CSI_DT_YUV422_10,     ///< YUV422 10
    Camera_Sensor_CSI_DT_MAX            ///< Enum Gate Keeper
} Camera_Sensor_CSIDataType;

/**
 * This enumerates the data valid states.
 */
typedef enum
{
    Camera_Sensor_DATA_VALID_INVALID,
    Camera_Sensor_DATA_VALID_LOW, ///< Data valid when signal is in its low state
    Camera_Sensor_DATA_VALID_HIGH ///< Data valid when signal is in its high state
} Camera_Sensor_DataValidType;

/**
 * This enumerates the direction the lens should move to focus.
 */
typedef enum
{
    Camera_Sensor_FOCUS_DIRECTION_MOVE_NEAR, ///< Move lens to focus objects that are near
    Camera_Sensor_FOCUS_DIRECTION_MOVE_FAR   ///< Move lens to focus objects that are far
} Camera_Sensor_FocusDirectionType;

/**
 * This enumerates the SensorFocusInfo entry.
 */
typedef enum
{
    /* General Info*/
    Camera_Sensor_CURRENT_STEP_POSITION,
    Camera_Sensor_CURRENT_LENS_POSITION,
    Camera_Sensor_TOTAL_STEPS,
    Camera_Sensor_INITIAL_CODE,
    Camera_Sensor_INFINITY_CODE,
    Camera_Sensor_NUMBER_OF_STEPS_NL_REGION,
    Camera_Sensor_NUMBER_OF_CODE_PER_STEPS_NL_REGION,
    Camera_Sensor_NUMBER_OF_STEPS_L_REGION,
    Camera_Sensor_NUMBER_OF_CODE_PER_STEPS_L_REGION,
    Camera_Sensor_MAX_CODE,

    /* SW Damp */
    Camera_Sensor_SW_COLLISION_DAMP_ENABLED,
    Camera_Sensor_SW_COARSE_DAMP_ENABLED,
    Camera_Sensor_SW_DAMP_NEAR_FAR_THRESHOLD,
    Camera_Sensor_SW_DAMP_TIME_WAIT_NEAR_DISTANCE,
    Camera_Sensor_SW_DAMP_STEP_NEAR_DISTANCE,
    Camera_Sensor_SW_DAMP_TIME_WAIT_FAR_DISTANCE,
    Camera_Sensor_SW_DAMP_STEP_FAR_DISTANCE,

    /* HW Damp */
    Camera_Sensor_HW_DAMP_ENABLED,
    Camera_Sensor_HW_DAMP_MODE_MASK,
    Camera_Sensor_HW_DAMP_CODE_INCREMENT,
    Camera_Sensor_HW_DAMP_DELAY,

    /* Stop Stream Cut off AF Powers*/
    Camera_Sensor_STOP_STREAM_CUT_OFF_AF_PWR,
    Camera_Sensor_RESTORED_STEP_POSITION,

    Camera_Sensor_REQUIRE_RE_INITIALIZATION,
    Camera_Sensor_SENSOR_FOCUS_INFO_ENTRY_COUNT
} Camera_Sensor_SensorFocusInfoEntryType;

/**
 * This enumerates the position to test
 */
typedef enum
{
    Camera_Sensor_LENS_ALGORITHM_CONTROL_POSITION,                 ///< default lens position
    Camera_Sensor_LENS_CALIBRATED_INFINITY_POSITION,
    Camera_Sensor_LENS_CALIBRATED_MACRO_POSITION,
    Camera_Sensor_LENS_MECHANICAL_INFINITY_POSITION,
    Camera_Sensor_LENS_MECHANICAL_MACRO_POSITION,
    Camera_Sensor_LENS_REST_POSITION
} Camera_Sensor_FocusLensPositionType;

/**
 * This enumerates the lens movement status for focus.
 */
typedef enum
{
    Camera_Sensor_FOCUS_MOVEMENT_INVALID,
    Camera_Sensor_FOCUS_MOVEMENT_COMPLETE,    ///< Focus lens movement completed
    Camera_Sensor_FOCUS_MOVEMENT_IN_PROGRESS, ///< Focus lens movement incomplete
    Camera_Sensor_FOCUS_MOVEMENT_ERROR        ///< Encountered an error during lens movement
} Camera_Sensor_FocusMovementStatusType;

/**
 * This enumerates the on-sensor auto-focus mode.
 */
typedef enum
{
    Camera_Sensor_FOCUS_MODE_INVALID,
    Camera_Sensor_FOCUS_MODE_NORMAL,             ///< Cause on-sensor auto-focus to search normal range (normal - infinity)
    Camera_Sensor_FOCUS_MODE_MACRO,              ///< Cause on-sensor auto-focus to search entire range (near - infinity)
    Camera_Sensor_FOCUS_MODE_MACRO_LIMITED,      ///< Cause on-sensor auto-focus to search limited macro range (near - normal)
    Camera_Sensor_FOCUS_MODE_SUPER_MACRO_LIMITED ///< Cause on-sensor auto-focus to search limited super macro range (very near - near)
} Camera_Sensor_FocusModeType;

/**
 * This enumerates the on-sensor auto-focus state.
 */
typedef enum
{
    Camera_Sensor_FOCUS_STATE_INVALID,
    Camera_Sensor_FOCUS_STATE_STOP,  ///< Cause on-sensor auto-focus to go to idle
    Camera_Sensor_FOCUS_STATE_START  ///< Cause on-sensor auto-focus to start running
} Camera_Sensor_FocusStateType;

/**
 * This enumerates the imager technology types.
 */
typedef enum
{
    Camera_Sensor_IMAGER_TYPE_INVALID,
    Camera_Sensor_IMAGER_TYPE_CMOS, ///< Imager sensor is CMOS-type
    Camera_Sensor_IMAGER_TYPE_CCD   ///< Imager sensor is CCD-type
} Camera_Sensor_ImagerTechnologyType;

/**
 * This enumerates the possible output interface.
 */
typedef enum
{
    Camera_Sensor_INTERFACE_INVALID,        ///< Uninitialized value
    Camera_Sensor_PIXEL_INTERFACE,          ///< Pixel Interface
    Camera_Sensor_RAW_INTERFACE             ///< RDI Interface
} Camera_Sensor_OutputInterfaceType;

/**
 * This enumerates the possible operational modes of the sensor.
 */
typedef enum
{
    Camera_Sensor_OPERATIONAL_MODE_INVALID           = 0x00, ///< Sensor not configured
    Camera_Sensor_OPERATIONAL_MODE_PREVIEW           = 0x01, ///< Sensor configured for preview (viewfinder) mode
    Camera_Sensor_OPERATIONAL_MODE_SNAPSHOT          = 0x02, ///< Sensor configured for snapshot (still capture) mode
    Camera_Sensor_OPERATIONAL_MODE_VIDEO_CAPTURE     = 0x04, ///< Sensor configured for video capture (video record) mode
    Camera_Sensor_OPERATIONAL_MODE_BURST_CAPTURE     = 0x08, ///< Sensor configured for burst shot in video record mode
    Camera_Sensor_OPERATIONAL_MODE_VIDEO_HDR_CAPTURE = 0x10  ///< Sensor configured for HDR mode in video record mode
} Camera_Sensor_OperationalModeType;

/**
 * This enumerates the lens movement status for optical zoom.
 */
typedef enum
{
    Camera_Sensor_OPTICAL_ZOOM_MOVEMENT_INVALID,
    Camera_Sensor_OPTICAL_ZOOM_MOVEMENT_COMPLETE,   ///< Optical zoom lens movement completed
    Camera_Sensor_OPTICAL_ZOOM_MOVEMENT_IN_PROGRESS,///< Optical zoom lens movement incomplete
    Camera_Sensor_OPTICAL_ZOOM_MOVEMENT_ERROR       ///< Encountered an error during lens movement
} Camera_Sensor_OpticalZoomMovementStatusType;

/**
 * This enumerates the pixel data format types.
 */
typedef enum
{
    Camera_Sensor_OUTPUT_DATA_FORMAT_INVALID,
    Camera_Sensor_OUTPUT_DATA_FORMAT_BAYER_G_B, ///< Bayer with even lines: GB, odd lines: RG
    Camera_Sensor_OUTPUT_DATA_FORMAT_BAYER_B_G, ///< Bayer with even lines: BG, odd lines: GR
    Camera_Sensor_OUTPUT_DATA_FORMAT_BAYER_G_R, ///< Bayer with even lines: GR, odd lines: BG
    Camera_Sensor_OUTPUT_DATA_FORMAT_BAYER_R_G, ///< Bayer with even lines: RG, odd lines: GB
    Camera_Sensor_OUTPUT_DATA_FORMAT_Y, ///< Y plane for monochrome images
    Camera_Sensor_OUTPUT_DATA_FORMAT_YCBCR_Y_CB_Y_CR, ///< YCbCr 4:2:2 interleaved: YCbYCr
    Camera_Sensor_OUTPUT_DATA_FORMAT_YCBCR_Y_CR_Y_CB, ///< YCbCr 4:2:2 interleaved: YCrYCb
    Camera_Sensor_OUTPUT_DATA_FORMAT_YCBCR_CB_Y_CR_Y, ///< YCbCr 4:2:2 interleaved: CbYCrY
    Camera_Sensor_OUTPUT_DATA_FORMAT_YCBCR_CR_Y_CB_Y  ///< YCbCr 4:2:2 interleaved: CrYCbY
} Camera_Sensor_OutputDataFormatType;

/**
 * This enumerates the possible pixel clock edges on which the data is valid.
 */
typedef enum
{
    Camera_Sensor_PIXEL_CLOCK_DATA_VALID_INVALID,
    Camera_Sensor_PIXEL_CLOCK_DATA_VALID_RISING_EDGE, ///< Data valid on clock's rising edge
    Camera_Sensor_PIXEL_CLOCK_DATA_VALID_FALLING_EDGE ///< Data valid on clock's falling edge
} Camera_Sensor_PixelClockDataValidEdgeType;

/**
 * This enumerates the possible pixel interface.
 */
typedef enum
{
    Camera_Sensor_PIXEL_INTERFACE_INVALID,           ///< Uninitialized value
    Camera_Sensor_PIXEL_INTERFACE_PARALLEL,          ///< Parallel Interface
    Camera_Sensor_PIXEL_INTERFACE_MDDI,              ///< MDDI Interface
    Camera_Sensor_PIXEL_INTERFACE_MIPI,              ///< MIPI Interface
    Camera_Sensor_PIXEL_INTERFACE_VFE_TESTGEN        ///< VFE TESTGEN
} Camera_Sensor_PixelInterfaceType;

/**
 * This enumerates the possible sources of the regions of interest.
 */
typedef enum
{
    Camera_Sensor_ROI_SOURCE_USER,  ///< ROI is a user selected region.
    Camera_Sensor_ROI_SOURCE_FACE,  ///< ROI is a detected face.
    Camera_Sensor_ROI_SOURCE_MAX    ///< Invalid ROI source.
} Camera_Sensor_ROISourceType;

/**
 * This enumerates the synchronization mode types.
 */
typedef enum
{
    Camera_Sensor_SYNCHRONIZATION_MODE_INVALID,
    Camera_Sensor_SYNCHRONIZATION_MODE_APS, ///< Active physical sync (Hsync/Vsync)
    Camera_Sensor_SYNCHRONIZATION_MODE_EFS, ///< Embedded frame sync (SOF/SOL/EOL/EOF tokens)
    Camera_Sensor_SYNCHRONIZATION_MODE_ELS  ///< Embedded line sync (SAV/EAV/SAVVBI/EAVVBI tokens)
} Camera_Sensor_SynchronizationModeType;

/**
 * This enumerates the touch-focus control sources.
 */
typedef enum
{
    Camera_Sensor_TOUCH_FOCUS_CONTROL_INVALID,
    Camera_Sensor_TOUCH_FOCUS_CONTROL_NOT_SUPPORTED, ///< Touch-focus not supported by sensor
    Camera_Sensor_TOUCH_FOCUS_CONTROL_IN_SENSOR,     ///< Touch-focus performed exclusively in sensor
    Camera_Sensor_TOUCH_FOCUS_CONTROL_IN_SEN_OR_EXT, ///< Touch-focus performed either in sensor or externally
    Camera_Sensor_TOUCH_FOCUS_CONTROL_EXTERNAL       ///< Touch-focus performed exclusively externally
} Camera_Sensor_TouchFocusControlType;

/**
 * This configuration configures PDAF Data Path,
 * Use of this flag does not turn on/off PDAF output from the sensor
 * Sensor's current mode must support PDAF
 */
typedef enum
{
    /// PDAF Type Unknown
    Camera_Sensor_PDAF_Type_Unknown,
    /// PDAF Type I
    Camera_Sensor_PDAF_Type_1,
    /// PDAF Type III
    Camera_Sensor_PDAF_Type_3,
} Camera_Sensor_PADFType;

/**********************************************************
 ** Control Structure Structured Types
 *********************************************************/
/**
 * This is a structure containing the X and Y scaling information.
 * Note: if fXScalingRatio = 1.0f and fYScalingRatio = 2.0f, preview is stretched in Y direction
         if fXScalingRatio = 2.0f and fYScalingRatio = 1.0f, preview is stretched in X direction
         if fXScalingRatio = fYScalingRatio, preview has correct FOV
 */
typedef struct
{
    /// This is the scaling ratio in the X direction.
    float fXScalingRatio;

    /// This is the scaling ratio in the Y direction.
    float fYScalingRatio;

}Camera_Sensor_ScalingRatioType;

/**
 * This is a structure containing the EFS tokens.
 */
typedef struct
{
    /// This is the start-of-frame token.
    uint32 nSOFToken;

    /// This is the end-of-frame token.
    uint32 nEOFToken;

    /// This is the start-of-line token.
    uint32 nSOLToken;

    /// This is the end-of-line token.
    uint32 nEOLToken;
} Camera_Sensor_EFSTokensType;

/**
 * This is a structure containing the EFS tokens.
 */
typedef struct
{
    /// This is the start of active video during vertical blanking interval
    /// token.
    uint32 nSAVVBIToken;

    /// This is the end of active video during vertical blanking interval
    /// token.
    uint32 nEAVVBIToken;

    /// This is the start of active video token.
    uint32 nSAVToken;

    /// This is the end of active video token.
    uint32 nEAVToken;
} Camera_Sensor_ELSTokensType;

/**
 * This is a structure containing information on a single region of interest.
 */
typedef struct
{
    /// A unique identifier specifying this particular ROI.
    uint32 nID;

    /// The coordinates of the ROI in pixels.
    Camera_Rectangle pixelCoordinates;

    /// The source of the ROI.
    Camera_Sensor_ROISourceType source;

    /// The weight of the ROI expressed a percentage from 0 to 100. The sum
    /// weight of all ROIs will equal 100.
    uint32 nWeight;

    /// This specifies a mask of ROI_FUNCTION_MASK values designating the function
    /// area that this region of interest is used for.
    uint32 nFunctionType;
} Camera_Sensor_RegionOfInterestType;

/**
 * This is a structure containing supported sensor mode information.
 */
typedef struct
{
    /// This indicates a resolution the sensor supports
    Camera_Size resolution;

    /// This indicates the number of active lines per frame, including black
    /// lines.
    uint32 nActiveLinesPerFrame;

    /// This indicates the number of active pixel clock periods per line,
    /// including black pixel clock periods.
    uint32 nActivePixelClocksPerLine;

    /// This indicates the first line of the image data, after any black lines
    /// at the beginning of the frame.
    uint32 nFirstImageLine;

    /// This indicates the first pixel clock period of the image data, after
    /// any black pixels at the beginning of the line.
    uint32 nFirstImagePixelClock;

    /// This indicates the last line of the image data.
    uint32 nLastImageLine;

    /// This indicates the last pixel clock period of the image data.
    uint32 nLastImagePixelClock;

    /// This indicates what the minimum interval between frames is for the
    /// supported resolution.
    uint32 nMinimumFrameInterval;

    /// This indicates what the maximum  interval between frames is for the
    /// supported resolution.
    uint32 nMaximumFrameInterval;

    /// This indicates the ratio of the width of sensor mode FOV with respect
    /// to the maximum width of the sensor
    float fFOVWidthRatio;

    /// This indicates the ratio of the height of sensor mode FOV with respect
    /// to the maximum height of the sensor
    float fFOVHeightRatio;

    //This indicates the scalar ratio in the horizontal direction
    float fXScalingRatio;

    //This indicates the scalar ratio in the vertical direction
    float fYScalingRatio;

    /// This indicates the CSI data type of supported sensor channel
    Camera_Sensor_CSIDataType csiDT;

    /// This indicates the type of output interface from sensor
    Camera_Sensor_OutputInterfaceType outputInterface;
} Camera_Sensor_SupportedSensorChannelType;

/**
 * This is a structure containing supported sensor mode information.
 */
typedef struct
{
    /// This indicates the number of channels sensor output for
    /// this sensor mode
    uint32 nNumOutputChannels;

    /// This is an array of all supported sensor channels
    Camera_Sensor_SupportedSensorChannelType aSupportedSensorChannels[MAX_NUMBER_OF_CHANNELS_PER_SENSOR_MODE];

    /// Each sensor mode is required to support minimum of one channel
    /// Channel 0 data are replicated in sensor mode for legacy support
    /// This indicates a resolution the sensor supports
    Camera_Size resolution;

    /// This indicates the number of active lines per frame, including black
    /// lines.
    uint32 nActiveLinesPerFrame;

    /// This indicates the number of active pixel clock periods per line,
    /// including black pixel clock periods.
    uint32 nActivePixelClocksPerLine;

    /// This indicates the first line of the image data, after any black lines
    /// at the beginning of the frame.
    uint32 nFirstImageLine;

    /// This indicates the first pixel clock period of the image data, after
    /// any black pixels at the beginning of the line.
    uint32 nFirstImagePixelClock;

    /// This indicates the last line of the image data.
    uint32 nLastImageLine;

    /// This indicates the last pixel clock period of the image data.
    uint32 nLastImagePixelClock;

    /// This indicates what the minimum interval between frames is for the
    /// supported resolution.
    uint32 nMinimumFrameInterval;

    /// This indicates what the maximum interval between frames is for the
    /// supported resolution.
    uint32 nMaximumFrameInterval;

    /// This indicates the ratio of the width of sensor mode FOV with respect
    /// to the maximum width of the sensor
    float fFOVWidthRatio;

    /// This indicates the ratio of the height of sensor mode FOV with respect
    /// to the maximum height of the sensor
    float fFOVHeightRatio;

    //This indicates the scalar ratio in the horizontal direction
    float fXScalingRatio;

    //This indicates the scalar ratio in the vertical direction
    float fYScalingRatio;

} Camera_Sensor_SupportedSensorModeType;

/**
 * This is a structure containing the minmum and maximum exposure time
 */
typedef struct
{
    /// This is minimum exposure time
    uint32 nMinimumExposureTimeMicroSeconds;

    /// This is maximum exposure time
    uint32 nMaximumExposureTimeMicroSeconds;
} Camera_Sensor_ExposureTimeRange;

/**
 * This is a structure containing the minmum and maximum focus steps
 */
typedef struct
{
    /// This is minimum focus step
    uint32 nMinimumFocusStep;

    /// This is maximum exposure time
    uint32 nMaximumFocusStep;
} Camera_Sensor_FocusStepRange;

/**********************************************************
 ** Camera Sensor Driver Information Structures
 *********************************************************/
/**
 * This is a structure containing embedded data information of the sensor.
 */
typedef struct
{
    /// This indicates the current embedded data size output by the sensor.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nCurrentEmbeddedDataSize;

    /// This indicates the maximum embedded data output by the sensor.
    /// @par Information Type:
    ///   Static
    uint32 nMaximumEmbeddedDataSize;
} Camera_Sensor_EmbeddedDataInfoType;

/**
 * This is a structure containing embedded data information of the sensor.
 */
typedef struct
{
    Camera_Sensor_PADFType ePDAFType;
    /// This indicates the Phase Difference data size output by the sensor.
    /// @par Information Type:
    uint32 nPhaseDifferenceDataSize;

} Camera_Sensor_PhaseDifferenceDataInfoType;
/**
 * This is a structure containing feature support information of the sensor.
 */
typedef struct
{
    /// This indicates where auto-exposure is expected to be performed.
    /// AUTO_EXPOSURE_CONTROL_IN_SENSOR indicates that auto-exposure will be
    /// performed in the sensor with no external intervention required.
    /// AUTO_EXPOSURE_CONTROL_EXTERNAL indicates that the sensor expects
    /// exposure values from an external source.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_AutoExposureControlType autoExposureControl;

    /// This indicates where auto-flash synchronization is expected to be
    /// performed. AUTO_FLASH_SYNC_IN_SENSOR indicates that auto-flash
    /// synchronization will be performed in the sensor with no external
    /// intervention required. AUTO_FLASH_SYNC_EXTERNAL indicates that
    /// auto-flash synchronization is expected to be done externally.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_AutoFlashSyncType autoFlashSync;

    /// This indicates where auto-flicker compensation is expected to be
    /// performed. AUTO_FLICKER_COMPENSATION_IN_SENSOR indicates that the
    /// sensor will compensate for flicker with the available flicker
    /// detection information. AUTO_FLICKER_COMPENSATION_EXTERNAL indicates
    /// that the sensor expects flicker compensation to be performed
    /// externally.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_AutoFlickerCompensationType autoFlickerCompensation;

    /// This indicates where auto-flicker detection is expected to be
    /// performed. AUTO_FLICKER_DETECTION_IN_SENSOR indicates that the
    /// sensor will detect the flicker frequency with no external
    /// intervention required. AUTO_FLICKER_DETECTION_EXTERNAL indicates
    /// that the sensor expects the flicker frequency detection to be
    /// performed externally.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_AutoFlickerDetectionType autoFlickerDetection;

    /// This indicates where auto-focus is expected to be performed.
    /// AUTO_FOCUS_CONTROL_NOT_SUPPORTED indicates that auto-focus is not
    /// supported by the sensor. AUTO_FOCUS_CONTROL_IN_SENSOR indicates
    /// that auto-focus will be performed in the sensor with no external
    /// intervention required. AUTO_FOCUS_CONTROL_EXTERNAL indicates that
    /// the sensor expects focus values from an external source.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_AutoFocusControlType autoFocusControl;

    /// This indicates where auto-frame-rate control is expected to be
    /// performed. AUTO_FRAME_RATE_CONTROL_IN_SENSOR indicates that
    /// auto-frame-rate control will be performed in the sensor with no
    /// external intervention required. AUTO_FRAME_RATE_CONTROL_EXTERNAL
    /// indicates that the sensor expects auto-frame-rate control from an
    /// external source.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_AutoFrameRateControlType autoFrameRateControl;

    /// This indicates where auto-white balance control is expected to be
    /// performed. AUTO_WHITE_BALANCE_CONTROL_IN_SENSOR indicates that
    /// auto-white balance control will be performed in the sensor with no
    /// external intervention required. AUTO_WHITE_BALANCE_CONTROL_EXTERNAL
    /// indicates that the sensor expects white balance values from an
    /// external source.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_AutoWhiteBalanceControlType autoWhiteBalanceControl;

    /// This indicates where auto-focus is expected to be performed.
    /// TOUCH_FOCUS_CONTROL_NOT_SUPPORTED indicates that auto-focus is not
    /// supported by the sensor. TOUCH_FOCUS_CONTROL_IN_SENSOR indicates
    /// that auto-focus will be performed in the sensor with no external
    /// intervention required. TOUCH_FOCUS_CONTROL_EXTERNAL indicates that
    /// the sensor expects focus values from an external source.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_TouchFocusControlType touchFocusControl;


    /// This indicates where auto-focus is expected to be performed.
    /// CONTINUOUS_FOCUS_CONTROL_NOT_SUPPORTED indicates that auto-focus is not
    /// supported by the sensor. CONTINUOUS_FOCUS_CONTROL_IN_SENSOR indicates
    /// that auto-focus will be performed in the sensor with no external
    /// intervention required. CONTINUOUS_FOCUS_CONTROL_EXTERNAL indicates that
    /// the sensor expects focus values from an external source.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_ContinuousFocusControlType continuousFocusControl;

    /// This structure holds minimum and maximum exposure time
    /// @par Information Type:
    ///   The information will be dynamically changed, based on sensor mode
    Camera_Sensor_ExposureTimeRange exposureTimeRange;

    /// This structure holds minimum and maximum focus step
    /// @par Information Type:
    Camera_Sensor_FocusStepRange focusStepRange;

    /// This indicates whether the sensor supports aperture control.
    boolean bApertureControl;

    /// This indicates whether the sensor supports crop zoom control.
    boolean bCropZoomControl;

    /// This indicates whether the sensor supports embedded data.
    boolean bEmbeddedDataSupport;

    /// This indicates whether the sensor supports phase difference data.
    boolean bPhaseDifferenceDataSupport;

    /// This indicates whether the sensor supports optical zoom control.
    boolean bOpticalZoomControl;

    /// This indicates whether the sensor supports shutter control.
    boolean bShutterControl;

    /// This indicates whether the sensor handles the digital image stabilization control
    boolean bDISControl;

    /// This indicates whether the sensor supports hdr stats.
    boolean bHdrStatsSupport;

    /// This indicates whether the sensor supports Optical image stabilization.
    boolean bOISSupport;
} Camera_Sensor_FeaturesInfoType;

/**
 * This structure consists of focus tuning data.
 */
typedef struct
{
    uint16 Info[Camera_Sensor_SENSOR_FOCUS_INFO_ENTRY_COUNT];
} Camera_Sensor_SensorFocusInfoType;

/**
 * This is a structure containing identification information such as name
 * and type.
 */
typedef struct
{
    /// This indicates the sensor imager techonology.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_ImagerTechnologyType imagerTechnology;

    /// This indicates the sensor manufacturer name.
    /// @par Information Type:
    ///   Static
    char* pszManufacturerName;
    int pszManufacturerNameLen;
    int pszManufacturerNameLenReq;

    /// This indicates the sensor model name and number.
    /// @par Information Type:
    ///   Static
    char* pszModelNameAndNumber;
    int pszModelNameAndNumberLen;
    int pszModelNameAndNumberLenReq;

    /// This indicates the sensor manufacturer and model name and number in
    /// a file-system safe format.
    /// @par Information Type:
    ///   Static
    char* pszFSSafeName;
    int pszFSSafeNameLen;
    int pszFSSafeNameLenReq;
} Camera_Sensor_IdentificationInfoType;

/**
 * This is a structure containing interface information of sensor.
 * The information should be used to determine how to program HW
 * blocks received data from sensor.
 */
typedef struct
{
    /// This indicates the type of pixel interface sensor output
    Camera_Sensor_PixelInterfaceType pixelInterface;

    /// This indicates the index of pixel interface when there are more than one
    /// pixel interface of the same type.
    uint32 nPixelInterfaceIndex;
} Camera_Sensor_InterfaceInfoType;

/**
 * This is a structure containing mechanical information.
 */
typedef struct
{
    /// This flag indicates whether the lens is currently moving for focus.
    /// @par Information Type:
    ///   Dynamic - updated whenever the lens starts or stops moving.
    Camera_Sensor_FocusMovementStatusType focusMovementStatus;

    /// This flag indicates whether the lens is currently moving for optical
    /// zoom.
    /// @par Information Type:
    ///   Dynamic - updated whenever the lens starts or stops moving.
    Camera_Sensor_OpticalZoomMovementStatusType opticalZoomMovementStatus;

    /// This is the maximum aperture (minimum f-number) of the sensor.
    /// Positive real values are interpreted literally as the f-number. For
    /// example f/4.8 is stored as 4.8 decimal.
    /// @par Information Type:
    ///   Static
    float fMaximumAperture;

    /// This is the current aperture (f-number) of the sensor. For the
    /// majority of sensors, this is a static value. Positive real values are
    /// interpreted literally as the f-number. For example f/4.8 is stored as
    /// 4.8 decimal.
    /// @par Information Type:
    ///   Static - For sensors without a mechanical aperture.
    ///   Dynamic - For sensors with a mechanical aperature: updated whenever
    ///             the exposure settings cause the aperture to change.
    float fCurrentAperture;

    /// This is the minimum aperture (maximum f-number) of the sensor.
    /// Positive real values are interpreted literally as the f-number. For
    /// example f/4.8 is stored as 4.8 decimal.
    /// @par Information Type:
    ///   Static
    float fMinimumAperture;

    /// This is the distance in meters at which the focus is at infinity.
    /// @par Information Type:
    ///   Static
    float fHyperfocalDistance;

    /// This is the maximum shutter time of the sensor in microseconds.
    /// @par Information Type:
    ///   Static
    uint32 nMaximumShutterTime;

    /// This is the current shutter time of the sensor in microseconds.
    /// @par Information Type:
    ///   Static
    uint32 nCurrentShutterTime;

    /// This is the minimum shutter time of the sensor in microseconds.
    /// @par Information Type:
    ///   Static
    uint32 nMinimumShutterTime;
} Camera_Sensor_MechanicalInfoType;

/**
 * This is a structure containing sensor orientation information.
 */
typedef struct
{
    /// This indicates the pan angle at which the sensor is installed.
    /// @par Information Type:
    ///   Static
    float fPanAngle;

    /// This indicates the roll angle at which the sensor is installed.
    /// @par Information Type:
    ///   Static
    float fRollAngle;

    /// This indicates the tilt angle at which the sensor is installed.
    /// @par Information Type:
    ///   Static
    float fTiltAngle;
} Camera_Sensor_OrientationInfoType;

/**
 * This is a structure containing sensor output format information.
 */
typedef struct
{
    /// This indicates the output data format from the sensor, for example
    /// Bayer BG.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_OutputDataFormatType outputDataFormat;

    /// This indicates the location of the chroma samples relative to the
    /// Y samples - either cosited or interstitial.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_ChromaLocationType chromaLocation;

    /// This indicates the output data bit width from the sensor, usually
    /// 8, 10, or 12.
    /// @par Information Type:
    ///   Static
    uint32 nOutputDataBitWidth;
} Camera_Sensor_OutputFormatInfoType;

/**
 * This is a structure containing sensor output rate information.
 */
typedef struct
{
    /// This indicates the interval between frames in microseconds.
    /// @par Information Type:
    ///   Dynamic - updated when a new frame rate is configured.
    uint32 nCurrentFrameInterval;

    /// This indicates the minimum interval between frames in microseconds.
    /// @par Information Type:
    ///   Static
    uint32 nMinimumFrameInterval;

    /// This indicates the maximum interval between frames in microseconds.
    /// @par Information Type:
    ///   Static
    uint32 nMaximumFrameInterval;

    /// This indicates the video timing pixel clock frequency in Hertz. This
    /// is the frequency internal to the camera sensor, used by AEC, etc. for
    /// calculations requiring PCLK frequency.
    /// @par Information Type:
    ///   Dynamic - updated when a new frame rate is configured.
    int32 nVideoTimingPixelClockFrequency;

    /// This indicates the output pixel clock frequency in Hertz. This
    /// is the frequency output from the camera sensor to the CAMIF and can
    /// be used for AXI bus throttling, for example.
    /// @par Information Type:
    ///   Dynamic - updated when a new frame rate is configured.
    int32 nOutputPixelClockFrequency;
} Camera_Sensor_OutputRateInfoType;

/**
 * This is a structure containing sensor output resolution information.
 */

typedef struct
{
    /// This indicates the total number of lines per frame, including active
    /// lines, black lines, and inactive lines. This value is optional and may
    /// not be populated by sensor drivers.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nTotalLinesPerFrame;

    /// This indicates the total number of pixel clock periods per line,
    /// including active pixel, black pixel, and inactive pixel clock periods.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nTotalPixelClocksPerLine;

    /// This indicates the number of active lines per frame, including black
    /// lines.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nActiveLinesPerFrame;

    /// This indicates the number of active pixel clock periods per line,
    /// including black pixel clock periods.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nActivePixelClocksPerLine;

    /// This indicates the first line of the image data, after any black lines
    /// at the beginning of the frame.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nFirstImageLine;

    /// This indicates the first pixel clock period of the image data, after
    /// any black pixels at the beginning of the line.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nFirstImagePixelClock;

    /// This indicates the last line of the image data.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nLastImageLine;

    /// This indicates the last pixel clock period of the image data.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nLastImagePixelClock;

    /// This indicates the current resolution output by the sensor.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    Camera_Size nCurrentResolution;

    /// This indicates the number of blanking lines per frame.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    uint32 nBlankingHeight;

    /// This indicates the maximum resolution output by the sensor.
    /// @par Information Type:
    ///   Static
    Camera_Size nMaximumResolution;

    /// This indicates the scaling done in the sensor.
    /// @par Information Type:
    ///   Dynamic - updated when a new resolution is configured.
    Camera_Sensor_ScalingRatioType scaling;

    /// This indicates the ratio of the width of sensor mode FOV with respect
    /// to the maximum width of the sensor
    float fFOVWidthRatio;

    /// This indicates the ratio of the height of sensor mode FOV with respect
    /// to the maximum height of the sensor
    float fFOVHeightRatio;
} Camera_Sensor_OutputResolutionInfoType;

/**
 * This is a structure containing sensor output synchronization information.
 */
typedef struct
{
    /// This indicates the synchronization mode of the sensor.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_SynchronizationModeType synchronizationMode;

    /// This indicates the edge on which pixel clock data is valid and should
    /// be latched.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_PixelClockDataValidEdgeType pixelClockDataValidEdge;

    /// This indicates the state of the Hsync signal when data is valid.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_DataValidType hsyncValid;

    /// This indicates the state of the Vsync signal when data is valid.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_DataValidType vsyncValid;

    /// This indicates the EFS tokens. This is valid only when
    /// synchronizationMode is SYNCHRONIZATION_MODE_EFS.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_EFSTokensType EFSTokens;

    /// This indicates the ELS tokens.
    /// @par Information Type:
    ///   Static
    Camera_Sensor_ELSTokensType ELSTokens;

    /// This indicates the line at which all lines are integrating.
    /// @par Information Type:
    ///   Static
    uint32 nAllLinesIntegratingLine;

    /// This indicates the PCLK within a line at which all lines are
    /// integrating.
    /// @par Information Type:
    ///   Static
    uint32 nAllLinesIntegratingPCLK;
} Camera_Sensor_OutputSynchronizationInfoType;

/**
 * This is a structure containing sensor module calibration data.
 */
typedef struct
{
    /// This indicates R/G gain adjust ratio.
    /// @par Information Type:
    ///   Static
    float rgGainAdjustRatio;

    /// This indicates B/G gain adjust ratio.
    /// @par Information Type:
    ///   Static
    float bgGainAdjustRatio;

    /// This indicates even green adjust ratio.
    /// @par Information Type:
    ///   Static
    float gEvenGainAdjustRatio;

    /// This indicates odd green adjust ratio
    /// @par Information Type:
    ///   Static
    float gOddGainAdjustRatio;
} Camera_Sensor_SensorCalibrationInfoType;

/**
 * This is a structure containing sensor statistics information.
 */
typedef struct
{
    /// This indicates the detected flicker frequency in hertz. 0 specifies
    /// there is no detected flicker or the sensor does not support detection.
    /// @par Information Type:
    ///   Dynamic - updated when auto flicker detection is enabled.
    int32 nDetectedFlickerFreq;

    /// This is the current gain applied by the sensor.  This could be a
    /// mixture of analog and digital gains, depending on the sensor driver
    /// implementation. This value is in Q8 format.
    /// @par Information Type:
    ///   Dynamic - updated whenever statistics information is requested.
    uint32 nCurrentGain;

    /// This is the maximum gain that can be applied by the sensor. The sensor
    /// driver determines the best mix of analog and digital gains to apply in
    /// order to reach this value. This value is in Q8 format.
    /// @par Information Type:
    ///   Static
    uint32 nMaximumGain;

    /// This is the current integration line count that the sensor is using.
    /// @par Information Type:
    ///   Dynamic - updated whenever statistics information is requested.
    uint32 nCurrentIntegrationLineCount;

    /// This is the maximum integration line count that the sensor can use.
    /// @par Information Type:
    ///   Dynamic - updated whenever resolution is updated.
    uint32 nMaximumIntegrationLineCount;

    /// This is to find out whether flash is required or not
    boolean bFlashRequired;
} Camera_Sensor_StatisticsInfoType;

/**
 * This is a structure containing information on all supported sensor modes.
 */
typedef struct
{
    /// This indicates the number of modes the sensor supports
    uint32 nNumSupportedSensorModes;

    /// This is an array of all supported sensor modes
    Camera_Sensor_SupportedSensorModeType pSupportedSensorModes[MAX_NUMBER_OF_SENSOR_MODES];
} Camera_Sensor_SupportedSensorModesInfoType;

/**
 * This is a structure containing general output information used by AEC, etc.
 * The information should be interpreted as what the settings will be in the
 * future, if the provided resolution will be set immediately after this
 * structure is returned.
 */
typedef struct
{
    /// This indicates the number of frames to skip after the resolution is
    /// changed to the value provided
    uint32 nFutureFramesToSkipAfterResolutionChange;

    /// This indicates the maximum integration line count that the sensor
    /// could use
    uint32 nFutureMaximumIntegrationLineCount;

    /// This indicates the total number of lines per frame, including active
    /// lines, black lines, and inactive lines.This value is optional and may
    /// not be populated by sensor drivers.
    uint32 nFutureTotalLinesPerFrame;

    /// This indicates the total number of pixel clock periods each line
    /// would have, including active pixel, black pixel, and inactive pixel
    /// clock periods.
    uint32 nFutureTotalPixelClocksPerLine;

    /// This indicates the actual resolution the sensor would be set to
    Camera_Size futureResolution;

    /// This indicates the future scaling ratio of the sensor
    Camera_Sensor_ScalingRatioType futureScaling;

    /// This indicates the video timing pixel clock frequency the sensor
    /// would be set to
    int32 nFutureVideoTimingPixelClockFrequency;

    /// This indicates what the interval between frames will be in
    /// microseconds.
    uint32 nFutureCurrentFrameInterval;

    /// This indicates what the minimum interval between frames will be in
    /// microseconds.
    uint32 nFutureMinimumFrameInterval;

    /// This indicates the ratio of the width of sensor mode FOV with respect
    /// to the maximum width of the sensor
    float fFOVWidthRatio;

    /// This indicates the ratio of the height of sensor mode FOV with respect
    /// to the maximum height of the sensor
    float fFOVHeightRatio;

    /// This indicates sensitivity factor of a sensor mode.
    float fSensitivityFactor;
} Camera_Sensor_FutureOutputInfoType;

/**
* This enumerates the possible AutoFocus Actuator types.
*/
typedef enum
{
    Camera_Sensor_AUTO_FOCUS_ACTUATOR_INVALID = 0,        ///< Actuator not available
    Camera_Sensor_AUTO_FOCUS_ACTUATOR_VCM,                ///< VCM
    Camera_Sensor_AUTO_FOCUS_ACTUATOR_PIEZO,              ///< Piezo
    Camera_Sensor_AUTO_FOCUS_ACTUATOR_STEP_MOTOR,         ///< Step Motor
    Camera_Sensor_AUTO_FOCUS_ACTUATOR_BIVCM,                ///< VCM
} Camera_Sensor_AutoFocusActuatorType;

/**
* This is a structure containing sensor AFActuator information.
*/
typedef struct
{
    uint32         start_code;                        /**< starting dac value */
    uint32         calibrated_macro;                  /**< calibrated dac value for lens at macro end,  usually 10cm*/
    uint32         calibrated_infy;                   /**< calibrated dac value for lens at infinity. */
    uint32         end_limit_macro;                   /**< mechanical stop at macro end (in dac value) */
    uint32         end_limit_infy;                    /**< mechanical stop at infinity (in dac value) */
    uint32         hyperfocal_ff;                     /**< face forward hyperfocal lens position in dac */
    uint32         hyperfocal_fu;                     /**< face up hyperfocal lens position in dac */
    uint32         hyperfocal_fd;                     /**< face down hyperfocal lens position in dac */
} Camera_Sensor_AFActuatorInfoType_VCM;

typedef struct
{
    uint32         member_1;                          /**< dummy member for testing */
} Camera_Sensor_AFActuatorInfoType_BIVCM;

typedef struct
{
    uint32         member_1;                          /**< dummy member for testing */
} Camera_Sensor_AFActuatorInfoType_Piezo;

typedef struct
{
    uint32         member_1;                          /**< dummy member for testing */
} Camera_Sensor_AFActuatorInfoType_StepMotor;


//#pragma warning( push )
//#pragma warning( disable: 4201 )
typedef struct
{
    Camera_Sensor_AutoFocusActuatorType afType;
    union
    {
        Camera_Sensor_AFActuatorInfoType_VCM        vcm;
        Camera_Sensor_AFActuatorInfoType_Piezo      piezo;
        Camera_Sensor_AFActuatorInfoType_StepMotor  stepMotor;
        Camera_Sensor_AFActuatorInfoType_BIVCM      biVcm;
//#pragma warning( suppress: 4201 )
    };
} Camera_Sensor_AFActuatorInfoType;
//#pragma warning( pop )

/**
* This enumerates the possible AutoFocus types.
*/
typedef enum
{
    Camera_Sensor_AUTO_FOCUS_SUPPORTED_NONE,                    ///< Internal (Fixed focus module)
    Camera_Sensor_AUTO_FOCUS_SUPPORTED_EXTERNAL,                ///< External (QC AF)
    Camera_Sensor_AUTO_FOCUS_SUPPORTED_INTERNAL,                 ///< Internal (Senaor AF)
} Camera_Sensor_AutoFocusSupportedType;

/**
* This is a structure containing sensor AFSensor information.
*/
typedef struct
{
    float          pixel_size;                        /**< pixel size of the sensor. */
} Camera_Sensor_SensorInfoType;

/**********************************************************
 ** Camera Sensor Driver Configuration Structures
 *********************************************************/

/**
 * This is a structure containting information about the callback to be called on start
 * of every frame
 */
typedef struct
{
    /// This is the callback registered by sensor.
    /// This will be called on every start of frame.
    void  (*sensorBracketingCallback)(void);
} Camera_Sensor_BracketingCallbackInfoType;

/**
 * This is a structure containing focus bracketing info
 */
typedef struct
{
    /// This value determines if bracketing is supported.
    boolean bSupported;

    /// This is the minimum number of frames that must be skipped
    /// before capturing the first setting because it takes some time for lens to settle.
    uint32 nInitialFramesToSkip;

    /// This is the minimum number of frames that must be skipped
    /// for the subsequent settings (between any two settings).
    uint32 nMinimumFramesToSkip;

    /// This is the maximum number of frames that can be skipped.
    uint32 nMaximumFramesToSkip;
} Camera_Sensor_FocusBracketingInfoType;

/**
 * This is a structure containing Exposure bracketing info such as whether Exposure bracketing is
 * supported or not and the minimum number of frames required for rxposure to update
 */
typedef struct
{
    /// This value determines if bracketing is supported.
    boolean bSupported;

    /// This is the minimum number of frames that must be skipped
    /// for the subsequent settings (between any two settings).
    uint32 nMinimumFramesToSkip;

    /// This is the minimum number of frames that must be skipped
    /// before capturing the first setting because sensor takes these many frames to apply first exposure.
    uint32 nInitialFramesToSkip;

    /// This is the maximum number of frames that can be skipped.
    uint32 nMaximumFramesToSkip;
} Camera_Sensor_ExposureBracketingInfoType;


/**
 * This configuration structure is used for controlling the antibanding of
 * the sensor.
 */
typedef struct
{
    /// This is the antibanding mode.
    Camera_Sensor_AntibandingModeType antibandingMode;

    /// This is the user-specified flicker frequency of the lighting in the
    /// scene. This value is only used if antibandingMode is set to
    /// ANTIBANDING_MODE_MANUAL.
    int32 nUserFlickerFrequency;
} Camera_Sensor_AntibandingConfigType;

/**
 * This configuration structure is used for controlling the Autoframerate of
 * the sensor.
 */
typedef struct
{
    /// This is a flag indicating whether the sensor should Limit auto frame rate
    /// based on user input between some range
    boolean bLimitAutoFrameRate;

    /// This is the desired interval between frames in microseconds. If
    /// bLimitAutoFrameRate is set, then this is the minimum frame interval
    /// the camera sensor should output.
    uint32 nMinFrameInterval;

    /// This is the desired interval between frames in microseconds. If
    /// bLimitAutoFrameRate is set, then this is the maximum frame interval
    /// the camera sensor should output.
    uint32 nMaxFrameInterval;
} Camera_Sensor_AutoframerateConfigType;

/**
 * This configuration is used for controlling the contrast of the sensor.
 * The values are expressed as a gain multiplied by 100. Thus, a value of
 * 100 is equivalent to a 1x gain.
 */
typedef struct
{
    /// This is the minimum contrast setting. This value must be 0.
    int32 nMinimumContrast;

    /// This is the current contrast setting.
    int32 nContrast;

    /// This is the maximum contrast setting.
    int32 nMaximumContrast;
} Camera_Sensor_ContrastConfigType;

/**
 * This is used for determining the integration line count or exposure time
 * needed for exposure.
 */
typedef struct
{
    /// This is the integration time in number of lines.
    uint32 nIntegrationLineCount;

    /// This is the integration time in ms
    float fExposureTime;
}Camera_Sensor_ExposureIntegrationParamType;

/**
 * This configuration is used for controlling the exposure of the
 * sensor.
 */
typedef struct
{
    /// This is the sensor gain.
    float fGain;

    /// This determines the integration line count or exposure time needed for exposure
    Camera_Sensor_ExposureIntegrationParamType integrationParam;

    /// This is the number of frames to skip after exposure update. Sensor might take several frames to update exposure
    /// This has to be greater than or equal to nMinimumFramesToSkip.
    uint32 nFramesToSkip;
    uint32 src_id; ///Sensor number
    qcarcam_exposure_mode_t exposure_mode_type; ///Auto or manual exposure
} Camera_Sensor_ExposureConfigType;

/**
 * This configuration is used for controlling the luma value of the sensor if video HDR is enabled
 *
 */
typedef struct
{
    /// Average Luma value required for HDR exposure.
    uint8 nLumaAverage;

    /// Average of max 20 Luma values required for HDR exposure.
    uint8 nLumaMaxStatsAverage;

    /// Average of lower 20 Luma values required for HDR exposure.
    uint8 nLumaMinStatsAverage;

} Camera_Sensor_LumaForHDRConfigType;

/**
 * This configuration is used for controlling the exposure compensation
 * of the sensor.
 */
typedef struct
{
    /// This is the exposure compensation fraction.
    Camera_SRational exposureCompensation;
} Camera_Sensor_ExposureCompensationConfigType;

/**
 * This enumerates the possible exposure metering modes.
 */
typedef enum
{
    Camera_Sensor_EXPOSURE_METERING_INVALID,        ///< Uninitialized value
    Camera_Sensor_EXPOSURE_METERING_FRAME_AVERAGE,  ///< Average regions throughout frame
    Camera_Sensor_EXPOSURE_METERING_CENTER_WEIGHTED,///< Weight center more heavily
    Camera_Sensor_EXPOSURE_METERING_CENTER_SPOT,    ///< Use center exclusively
    Camera_Sensor_EXPOSURE_METERING_SKIN_PRIORITY   ///< Weight skin tones more heavily
} Camera_Sensor_ExposureMeteringModeType;

/**
 * This configuration is used for controlling the exposure metering
 * of the sensor.
 */
typedef struct
{
    /// This is the exposure metering mode.
    Camera_Sensor_ExposureMeteringModeType exposureMeteringMode;

    /// This is the center metering rectangle to use. The dimensions represent
    /// percentage of the full imager frame and the center of the rectangle
    /// is the center of the imager's frame. Top and left sides must be set to
    /// 0, bottom and right sides must between 0 and 100.
    Camera_Rectangle centerRectangle;

    /// This is the weighting used on the center metering rectangle.
    float nCenterRectWeighting;
} Camera_Sensor_ExposureMeteringConfigType;

/**
 * This configuration is used for externally controlling the focus of the
 * sensor.
 */
typedef struct
{
    /// If autoFocusControl in the FeaturesInfoType struct is set to
    /// AUTO_FOCUS_CONTROL_EXTERNAL, this is how the client specifies the
    /// direction to move the lens for focusing; otherwise this value is
    /// ignored.
    Camera_Sensor_FocusDirectionType focusDirection;

    /// If autoFocusControl in the FeaturesInfoType struct is set to
    /// AUTO_FOCUS_CONTROL_EXTERNAL, this is how the client specifies the
    /// relative number of steps to move the lens for focusing; otherwise this
    /// value is ignored.
    int32 nFocusSteps;

    /// This is a flag indicating whether to reset the lens position for focus
    boolean bResetFocus;

    /// if lensPosition is LENS_ALGORITHM_CONTROL_POSITION, lens will be set
    /// to direction and steps configured in FocusExternalConfigType. For other
    /// lensPosition, direction and steps are ignored.
    Camera_Sensor_FocusLensPositionType lensPosition;

} Camera_Sensor_FocusExternalConfigType;

typedef struct
{
    uint32 nDistanceInCM;
    uint16 nRange;
}Camera_Sensor_SubjectDistanceRangeInfoType;

typedef struct
{
    float fFocalLength;
    float fNormalizedFocalLength35mmWidthX;
    float fNormalizedFocalLength35mmHeightY;
    float fNormalizedDiagonalFocalLength35mm;
}Camera_Sensor_CameraFocalLengthType;

/**
 * This configuration is used for controlling sensor focus tuning.
 */
typedef struct
{
    uint16 nValue;
    Camera_Sensor_SensorFocusInfoEntryType Entry;
    boolean bReInit;
    boolean bCustomFn1;
} Camera_Sensor_SensorFocusInfoConfigType;

/**
 * This configuration is used for controlling the on-sensor auto-focus mode.
 */
typedef struct
{
    /// This is the mode to configure the on-sensor autofocus to.
    Camera_Sensor_FocusModeType focusMode;
} Camera_Sensor_FocusModeOnSensorConfigType;

/**
 * This configuration is used for controlling the on-sensor auto-focus state.
 */
typedef struct
{
    /// This is the state to configure the on-sensor auto-focus to.
    Camera_Sensor_FocusStateType focusState;
} Camera_Sensor_FocusStateOnSensorConfigType;

/**
 * This configuration is used for controlling the frame interval of the sensor.
 */
typedef struct
{
    /// This is a flag indicating whether the sensor should automatically
    /// determine the frame rate based on lighting conditions. This is only
    /// valid if the sensor is performing the auto-frame-rate control, as
    /// indicated in FeaturesInfoType.autoFrameRateControl.
    boolean bAutoFrameInterval;

    /// This is the desired interval between frames in microseconds. If
    /// bAutoFrameInterval is set, then this is the minimum frame interval
    /// the camera sensor should output.
    uint32 nFrameInterval;
} Camera_Sensor_FrameIntervalConfigType;

/**
 * This configuration is used for controlling the gamma of the sensor.
 * The values are expressed directly as the gamme curve number, i.e.
 * a gamma of 1.8 is expressed as 1.8.
 */
typedef struct
{
    /// This is the minimum gamma curve setting.
    float fMinimumGamma;

    /// This is the current gamma curve setting.
    float fGamma;

    /// This is the maximum gamma curve setting.
    float fMaximumGamma;
} Camera_Sensor_GammaConfigType;

/**
 * This enumerates the possible image effects.
 */
typedef enum
{
    Camera_Sensor_IMAGE_EFFECT_MODE_INVALID,  ///< Uninitialized value
    Camera_Sensor_IMAGE_EFFECT_MODE_OFF,      ///< No color effect used
    Camera_Sensor_IMAGE_EFFECT_MODE_CB_CR_OFFSET, ///< Cb/Cr offset effect
    Camera_Sensor_IMAGE_EFFECT_MODE_MONO,     ///< Monochrome effect
    Camera_Sensor_IMAGE_EFFECT_MODE_NEGATIVE, ///< Negative effect
    Camera_Sensor_IMAGE_EFFECT_MODE_SEPIA,    ///< Sepia effect
    Camera_Sensor_IMAGE_EFFECT_MODE_SOLARIZE  ///< Solarize effect
} Camera_Sensor_ImageEffectModeType;

/**
 * This enumerates the snapshot flash mode.
 */
typedef enum
{
    /// Do not use the flash specified.
    Camera_Sensor_SNAPSHOT_FLASH_MODE_OFF,
    /// Always use the flash specified.
    Camera_Sensor_SNAPSHOT_FLASH_MODE_ON,
    /// Automatically determine when to use the flash specified.
    Camera_Sensor_SNAPSHOT_FLASH_MODE_AUTO
} Camera_Sensor_SnapshotFlashModeType;

/**
 * This configuration is used for controlling flash for YUV sensor.
 */
typedef struct
{
    /// This is the mode to configure Flash for YUV sensor.
    Camera_Sensor_SnapshotFlashModeType snapshotFlashMode;
} Camera_Sensor_SnapshotFlashConfigType;


/**
 * This enumerates the possible ways of updating exposure
 */
typedef enum
{
    Camera_Sensor_EXPOSURE_UPDATE_BY_GAIN,             /// Exposure update is by changing gain
    Camera_Sensor_EXPOSURE_UPDATE_BY_EXPTIME,        /// Exposure update is by changing exposure time
    Camera_Sensor_EXPOSURE_UPDATE_BY_BOTH_GAIN_AND_EXPTIME  /// Exposure update is by changing both gain and exposure time
} Camera_Sensor_ExposureUpdateMethodType;

/**
* This configuration is used to specify the method of exposure update
*/
typedef struct
{
    /// This mentions how the exposure update is done.
    Camera_Sensor_ExposureUpdateMethodType expUpdateType;
} Camera_Sensor_ExposureUpdateMethodConfigType;

/**
 * This configuration is used for controlling the image effect of
 * the sensor.
 */
typedef struct
{
    /// This is the image effect mode.
    Camera_Sensor_ImageEffectModeType imageEffectMode;

    /// This is the minimum cb offset if IMAGE_EFFECT_CB_CR_OFFSET is used.
    int32 nMinimumCbOffset;

    /// This is the cb offset if IMAGE_EFFECT_CB_CR_OFFSET is used.
    int32 nCbOffset;

    /// This is the maximum cb offset if IMAGE_EFFECT_CB_CR_OFFSET is used.
    int32 nMaximumCbOffset;

    /// This is the minimum cr offset if IMAGE_EFFECT_CB_CR_OFFSET is used.
    int32 nMinimumCrOffset;

    /// This is the cr offset if IMAGE_EFFECT_CB_CR_OFFSET is used.
    int32 nCrOffset;

    /// This is the maximum cr offset if IMAGE_EFFECT_CB_CR_OFFSET is used.
    int32 nMaximumCrOffset;
} Camera_Sensor_ImageEffectConfigType;

/**
 * This configuration is used for parameters used to tune the image from the
 * sensor.
 */
typedef struct
{
    /// This is the AWB decision in degrees Kelvin
    uint32 nAWBDecisionDegreesK;

    /// This is the red gain in Q8 format
    uint32 nRedGain;

    /// This is the green gain in Q8 format
    uint32 nGreenGain;

    /// This is the blue gain in Q8 format
    uint32 nBlueGain;
} Camera_Sensor_ImageTuningConfigType;

/**
 * This enumerates the possible ISO modes.
 */
typedef enum
{
    Camera_Sensor_ISO_MODE_INVALID, ///< Uninitialized value
    Camera_Sensor_ISO_MODE_AUTO,    ///< Sensor's ISO algorithm used
    Camera_Sensor_ISO_MODE_MANUAL   ///< User-specified ISO values used
} Camera_Sensor_ISOModeType;

/**
 * This configuration is used for controlling the ISO speed rating of the
 * sensor.
 */
typedef struct
{
    /// This is the ISO mode
    Camera_Sensor_ISOModeType ISOMode;

    /// This is the ISO speed rating value
    int32 nISOValue;
} Camera_Sensor_ISOConfigType;

/**
 * This configuration is used for controlling the manual snapshot exposure time
 * of the sensor.
 */
typedef struct
{
    /// This is the manual snapshot exposure time value
    uint32 nExposureTimeMicroSeconds;

    /// This is Exposure Mode
    Camera_Sensor_ExposureModeType ExposureMode;
} Camera_Sensor_ManualExposureConfigType;

/**
 * This configuration is used for controlling the mechanical aperture and
 * shutter of the sensor.
 */
typedef struct
{
    /// This is the aperture stop setting
    float fApertureStop;

    /// This is the time the shutter is to be open
    uint32 nShutterTime;
} Camera_Sensor_MechanicalConfigType;

/**
 * This configuration structure is used for controlling the OIS of
 * the sensor.
 */
typedef struct
{
    /// This is the OIS mode.
    Camera_Sensor_OISModeType oisMode;
} Camera_Sensor_OISConfigType;

/**
 * This enumerates the possible optical zoom modes.
 */
typedef enum
{
    Camera_Sensor_OPTICAL_ZOOM_MODE_INVALID,  ///< Unitialized value
    Camera_Sensor_OPTICAL_ZOOM_MODE_MAGNIFICATION, ///< Optical zoom based on magnification
    Camera_Sensor_OPTICAL_ZOOM_MODE_STEP ///< Optical zoom based on zoom step
} Camera_Sensor_OpticalZoomModeType;

/**
 * This configuration is used for controlling the optical zoom of the
 * sensor.
 */
typedef struct
{
    /// This is the minimum zoom step setting.
    int32 nMinimumZoomStep;

    /// This is the current zoom step setting.
    int32 nZoomStep;

    /// This is the maximum zoom step setting.
    int32 nMaximumZoomStep;
} Camera_Sensor_OpticalZoomStepType;

/**
 * This union defines a zoom setting type and is either a zoom magnification
 * factor or a zoom step.
 */
typedef struct
{
    Camera_Sensor_OpticalZoomModeType _d;
    union
    {
        /// This zoom setting type is a zoom magnification factor.
        float fMagnificationFactor;

        /// This zoom setting type is a zoom step.
        Camera_Sensor_OpticalZoomStepType nZoomStep;
    } _u;
} Camera_Sensor_OpticalZoomSettingType;

/**
 * This configuration is used for controlling the optical zoom of the
 * sensor.
 */
typedef struct
{
    /// This is the optical zoom setting.
    Camera_Sensor_OpticalZoomSettingType opticalZoomSetting;
} Camera_Sensor_OpticalZoomConfigType;

/**
 * This configuration is used for controlling the resolution of the sensor.
 */
typedef struct
{
    /// The dimensions of the frame of reference for the ROI in pixels.
    Camera_Size imageDimensions;

    /// This is the number of valid regions of interest in the ROIs array
    uint32 nROIs;

    /// This is the array of regions of interest
    Camera_Sensor_RegionOfInterestType ROIs[MAX_NUM_USER_ROI];
} Camera_Sensor_RegionsOfInterestConfigType;

/**
 * This configuration is used for controlling the resolution of the sensor.
 */
typedef struct
{
    /// This is the desired output resolution from the sensor. A larger
    /// resolution may actually be used by the sensor, requiring cropping
    /// in the VFE. OutputResolutionInfoType indicates what resolution the
    /// sensor is actually outputting.
    Camera_Size resolution;

    /// This is the operational mode for the sensor to go into. It is not
    /// sufficient to use the resolution to determine the operational mode
    /// within the camera sensor driver and the camera sensor driver may
    /// need to know the operational mode in order to apply certain settings
    /// to different modes.
    Camera_Sensor_OperationalModeType operationalMode;

    /// This is the exposure configuration to set simultaneously with the
    /// resolution
    Camera_Sensor_ExposureConfigType exposureConfig;

    /// This is the frame interval configuration to set simultaneously with
    /// the resolution.
    Camera_Sensor_FrameIntervalConfigType frameIntervalConfig;

    /// This is the scaling to be done in the sensor.
    Camera_Sensor_ScalingRatioType scaling;
} Camera_Sensor_ResolutionConfigType;

/**
 * This configuration is used for storing field type and timestamp.
 */
typedef struct
{
    boolean even_field;

    uint64_t field_ts;   ///< time unit: ns
} Camera_Sensor_FieldType;


/**
* Video resolution configuration is used to hold actual video resolution being used during HQ video
*/
typedef struct
{
    Camera_Size Resolution;
}Camera_Sensor_VideoResolutionConfigType;

/**
 * This enumerates the possible self-tests.
 */
typedef enum
{
    Camera_Sensor_SELF_TEST_MODE_SENSOR_INTERFACE,
    Camera_Sensor_SELF_TEST_MODE_SENSOR_BRIDGE_INTERFACE
} Camera_Sensor_SelfTestModeType;

/**
 * This configuration is used for controlling the self-test to be performed
 * by the sensor driver.
 */
typedef struct
{
    Camera_Sensor_SelfTestModeType selfTestMode;
    CameraResult selfTestResult;
} Camera_Sensor_SelfTestConfigType;

/**
 * This is the config structure used to set the input bracketing values in the
 * driver.
 */
typedef struct
{
    /// This value determines if bracketing is enabled
    boolean bEnable;

    /// This is the pointer to array of bracketing exposures
    Camera_Sensor_ExposureConfigType* exposureConfig;

    /// This is the total number of exposures
    uint32 nExposures;

    /// This value determines if the exposure update is by
    /// changing gain or integration count or both
    Camera_Sensor_ExposureUpdateMethodType expUpdateType;

    //  Start index which is configured by engine.
    //  Sensor driver will start enqueuing from this index.
    uint32 startIndex;

    //  Initial frame skip configured by upper layer for 1st iteration for first bracketing
    uint32 nInitialFramesToSkip;
} Camera_Sensor_BracketingExposureConfigType;

/**
 * This configuration is used for controlling the focus for bracketing
 */
typedef struct
{
    /// Number of frames to skip before capture.
    /// This must be greater than or equal to nMinimumFramesToSkip in Camera_Sensor_FocusBracketingInfoType.
    uint32 nFramesToSkip;

    /// Focus position for the captured frame
    uint32 nPosition;
} Camera_Sensor_FocusConfigType;

/**
 * This is the config structure to set the input bracketing values in the driver
 */
typedef struct
{
    /// Enable or disable focus bracketing
    boolean bEnable;

    /// Pointer to focus bracketing configurations
    Camera_Sensor_FocusConfigType* focusConfig;

    /// Total number of focus positions
    uint32 nPositions;

    /// The index from which to start bracketing
    uint32 startIndex;

    /// This is the frame skip for the very first bracketing setting.
    uint32 nInitialFramesToSkip;
} Camera_Sensor_FocusBracketingConfigType;

/**
 * This configuration is used for controlling the sharpness of the sensor.
 * The values are expressed in arbitrary units but the scale MUST be linear.
 */
typedef struct
{
    /// This is the minimum sharpness setting.
    int32 nMinimumSharpness;

    /// This is the current sharpness setting.
    int32 nSharpness;

    /// This is the maximum sharpness setting.
    int32 nMaximumSharpness;
} Camera_Sensor_SharpnessConfigType;


/**
 * This configuration configures PDAF Data Path,
 * Use of this flag does not turn on/off PDAF output from the sensor
 * Sensor's current mode must support PDAF
 */
typedef enum
{
    /// Request sensor to enable PDAF datapath
    Camera_Sensor_PDAF_Data_Path_Enable,
    /// Request sensor to disable PDAF datapath
    Camera_Sensor_PDAF_Data_Path_Disable,
} Camera_Sensor_PADFDatapathConfigType;

/**
 * This configuration is used for controlling the sharpness of the sensor.
 * The values are expressed in arbitrary units but the scale MUST be linear.
 */
typedef struct
{
    /// This is PDAF Datapath Configuration
    Camera_Sensor_PADFDatapathConfigType ePDAFDatapathConfig;

} Camera_Sensor_PDAFConfigType;

/**
 * This configuration is used for loading PDAF Type I library
 */
typedef struct
{
    /// This is the PDAFT type of Sensor
    boolean bLoadPDAFTypeILibrary;

} Camera_Sensor_PDAFLibConfigType;
/**
 * This enumerates the possible white balance.
 */
typedef enum
{
    Camera_Sensor_WHITE_BALANCE_MODE_INVALID,
    /// Automatically determine white balance value
    Camera_Sensor_WHITE_BALANCE_MODE_AUTO,
    /// Automatically determine white balance value, limited to outdoor illuminates
    Camera_Sensor_WHITE_BALANCE_MODE_OUTDOOR_AUTO,
    /// Automatically determine white balance value using the simple grey world algorithm
    Camera_Sensor_WHITE_BALANCE_MODE_SIMPLE_GREY_WORLD,
    /// Automatically determine white balance value unless flash is fired, in which
    /// case use appropriate flash manual white balance.
    Camera_Sensor_WHITE_BALANCE_MODE_AUTO_OR_FLASH,
    /// Horizon (H) white balance - 2350K
    Camera_Sensor_WHITE_BALANCE_MODE_HORIZON,
    /// Indandescent (A) white balance - 2800K
    Camera_Sensor_WHITE_BALANCE_MODE_INCANDESCENT,
    /// Flourescent (TL84) white balance - 4000K
    Camera_Sensor_WHITE_BALANCE_MODE_TL84_FLUORESCENT,
    /// Strobe flash white balance - 5000K
    Camera_Sensor_WHITE_BALANCE_MODE_STROBE_FLASH,
    /// Daylight (D50) white balance - 5000K
    Camera_Sensor_WHITE_BALANCE_MODE_DAYLIGHT,
    /// LED flash white balance - 5500K
    Camera_Sensor_WHITE_BALANCE_MODE_LED_FLASH,
    /// Cloudy daylight (D65) white balance - 6500K
    Camera_Sensor_WHITE_BALANCE_MODE_CLOUDY_DAYLIGHT,
    /// Shade white (D75) balance - 7500K
    Camera_Sensor_WHITE_BALANCE_MODE_SHADE,
    /// DEPRECATED - Use predefined value above
    Camera_Sensor_WHITE_BALANCE_MODE_MANUAL = (int)0xDEADDEAD
} Camera_Sensor_WhiteBalanceModeType;

/**
 * This configuration is used for controlling the white balance of
 * the sensor. The values are expressed in degrees Kelvin.
 */
typedef struct
{
    /// This is the white balance mode.
    Camera_Sensor_WhiteBalanceModeType whiteBalanceMode;

    /// DEPRECATED - Use predefined mode above
    int32 nWhiteBalance;
} Camera_Sensor_WhiteBalanceConfigType;

/**
 * This is a structure containing desired resolution and frame rate
 * information to be used when passing information to the driver to determine
 * future output information. This is not a normal configuration as it is used
 * only to pass information to the driver, not configure settings on the
 * driver.
 */
typedef struct
{
    /// This is the desired resolution used to determine future output
    /// information.
    Camera_Size desiredResolution;

    /// This is the desired operational mode used to determine future output
    /// information.
    Camera_Sensor_OperationalModeType desiredOperationalMode;

    /// This contains the maximum allowed frame interval and auto flag used
    /// to determine future output information.
    Camera_Sensor_FrameIntervalConfigType maximumAllowedFrameInterval;
} Camera_Sensor_FutureConfigType;

/**
 * This struct is used to pass information to the sensor driver needed to select
 * the appropriate instance of Chromatix tuning data.
 */
typedef struct
{
    Camera_BestShotMode bestShotMode; ///< The current best shot mode.
    Camera_OperationalMode operationMode; ///< The current operational mode.
    uint16 chromatixVersion; ///< The current chromatix version.

    Camera_ChromatixType chromatixType; ///< Chromatix data type
    uint16  chromatixTypeVersion;

    boolean bForceLoad; ///< Unconditionally load the chromatix file if true
} Camera_Sensor_ChromatixConfigType;

/**
 * This struct is used to pass information to the sensor driver needed to select
 * the appropriate instance of extended Chromatix tuning data.
 */
typedef struct
{
    Camera_BestShotMode bestShotMode; ///< The current best shot mode.
    Camera_OperationalMode operationMode; ///< The current opertational mode.
    uint16  chromatixVersion; ///< The current chromatix version.
    uint16  chromatixExtVersion; ///< The current chromatix extension version. 0 indicates no extension.
    boolean bForceLoad; ///< Unconditionally load the chromatix file if true
} Camera_Sensor_ChromatixExtConfigType;

/**
 * This struct is used to pass information to the sensor driver needed to select
 * the appropriate instance of extended Future Chromatix tuning data.
 */
typedef struct
{
    Camera_BestShotMode bestShotMode; ///< The current best shot mode.
    Camera_Sensor_FutureConfigType futureConfiguration; ///< The future configuration
    uint16  chromatixVersion; ///< The current chromatix version.
    boolean bForceLoad; ///< Unconditionally load the chromatix file if true
} Camera_Sensor_FutureChromatixConfigType;

/**
 * This enumerates the possible RDI interface.
 */
typedef enum
{
    Camera_Sensor_RDI_INVALID,         ///< Uninitialized value
    Camera_Sensor_RDI_LINE_BASED,      ///< Line based RDI
    Camera_Sensor_RDI_FRAME_BASED      ///< Frame based RDI
} Camera_Sensor_RDIInterfaceType;

/**
 * This enumerates the possible Stereo Camera.
 */
typedef enum
{
    Camera_Sensor_STEREO_INVALID,        ///< Uninitialized value
    Camera_Sensor_STEREO_LEFT,           ///< Uninitialized value
    Camera_Sensor_STEREO_RIGHT           ///< Uninitialized value
} Camera_Sensor_StereoCameraType;

typedef enum
{
    Camera_Sensor_OTP_NUMBYTES,
    Camera_Sensor_OTP_FORMAT,
    Camera_Sensor_OTP_CALIBRATE,
    Camera_Sensor_OTP_CALIBRATE_POWERON,
    Camera_Sensor_OTP_CALIBRATE_INIT,
    Camera_Sensor_OTP_CALIBRATE_START,
}Camera_Sensor_OTPAction;

typedef enum
{
    Camera_Sensor_TDMLoadTuningData,        // Execute Call to LoadTuningData()
    Camera_Sensor_TDMGetChromatixParams,    // Execute Call to GetChromatixParams()
    Camera_Sensor_TDMGetChromatix3AParams,  // Execute Call to GetChromatix3AParams()
    Camera_Sensor_TDMGetChromatixVFEParams, // Execute Call to GetChromatixVFEParams()
    Camera_Sensor_TDMGetChromatixCPPParams, // Execute Call to GetChromatixCPPParams()
    Camera_Sensor_TDMGetChromatixPostProcParams, // Execute Call to GetChromatixPostProcParams()
}Camera_Sensor_TuningDataCallType;

typedef union
{
    /// This indicates the MIPI Data Rate in pixels per second
    /// per channel ID, applicable for pixel interface
    uint32 nPIDataRatePPS;

    /// or indicates the MIPI Data Rate in kilo bits per second
    /// per channel ID, applicable for RDI interface
    uint32 nRDIDataRatekbps;

} Camera_Sensor_DataRateType;

/*
 * This is a union containing RDI frame size information
 */
typedef struct
{
    /// RDI line based frame size
    Camera_Size resolution;

    /// RDI frame size
    uint32 nRDIFrameSize;

} Camera_Sensor_RDIFrameInfoType;

/**
 * This is a structure containing ISPIF information from the sensor.
 * The information should be used to determine how to program HW
 * blocks received data from sensor.
 */
typedef struct
{
    /// This indicates the index of CSID Core
    uint32 nCSIDIndex;

    /// This indicates the channel ID output of CSID Core
    uint32 nCID;

    /// This indicates the MIPI Data Rate in pps or MIPI Data Rate
    /// per lane in bps per channel ID, applicable for Pixel /RDI interface
    Camera_Sensor_DataRateType DataRate;

    /// This indicates the type of output interface from sensor
    Camera_Sensor_OutputInterfaceType outputInterface;

    /// This indicates the type of RDI interface from sensor
    Camera_Sensor_RDIInterfaceType rdiInterface;

    /// RDI frame size
    Camera_Sensor_RDIFrameInfoType rdiFrameInfo;

    /// This indicates the type of CSI DT from sensor
    Camera_Sensor_CSIDataType csiDT;

    /// This indicates the type of stereo camera
    Camera_Sensor_StereoCameraType stereoCamera;

} Camera_Sensor_ISPIFInfoType_Struct;

typedef struct
{
    /// This indicates an array of maximum number of pixel and RDI
    /// interface CID supported per sensor
    Camera_Sensor_ISPIFInfoType_Struct Info[Camera_Sensor_MAX_PIXEL_RDI_CID_PER_SENSOR];

} Camera_Sensor_ISPIFInfoType;

/**
 * This configuration is used for controlling the resolution of the sensor.
 */
typedef struct
{
    /// This is the desired output resolution from the sensor. A larger
    /// resolution may actually be used by the sensor, requiring cropping
    /// in the VFE. OutputResolutionInfoType indicates what resolution the
    /// sensor is actually outputting.
    Camera_Size resolution;

    /// This is the operational mode for the sensor to go into. It is not
    /// sufficient to use the resolution to determine the operational mode
    /// within the camera sensor driver and the camera sensor driver may
    /// need to know the operational mode in order to apply certain settings
    /// to different modes.
    Camera_Sensor_OperationalModeType operationalMode;

    /// This is the frame interval configuration to set simultaneously with
    /// the resolution.
    Camera_Sensor_FrameIntervalConfigType frameIntervalConfig;

    /// This is the zoom factor that is requested by user. Sensor driver
    /// has to set to a mode which can achieve the desried zoom
    float fZoomFactor;
} Camera_Sensor_ZoomConfigType;

/**
 * This is a structure containing the flag that specifies if a mode change
 * is needed to support requested zoom
 */
typedef struct
{
    /// This is the flag to return if crop mode switch is available
    boolean bModeChangeAvailable;
} Camera_Sensor_CropModeChangeNeededInfoType;

typedef struct
{
    /// This indicates hdr stats data size output by the sensor.
    uint32 nHdrStatsDataSize;

    /// This indicates max number of regions for sensor hdr stats.
    uint32 nHdrStatsMaxRegionNum;

    /// This indicates number of pixels per region for sensor hdr stats.
    uint32 nHdrStatsNumPixelsPerRegion;

} Camera_Sensor_HdrStatsInfoType;

/**
* This is a structure containing the OptimizationHint configuration info for camera sensor
*
*/
typedef struct
{
    uint64  nOptimizationHint;
} Camera_Sensor_OptimizationHintType;

typedef struct{
    int32 defocus;
    int8 df_confidence; /* 0-good, (-1)-not good*/
    uint32 df_conf_level; /* = 1024*ConfidentLevel/Threshold */
    float phase_diff;
} Camera_Sensor_PDAF_DefocusType;

typedef struct{
    uint8 * pd_stats;
    /* sensor output */
    uint32 x_offset;
    uint32 y_offset;
    uint32 x_win_num;
    uint32 y_win_num;
    Camera_Sensor_PDAF_DefocusType defocus[Camera_Sensor_MAX_PDAF_WINDOW];
    boolean status;

    void * eeprom_data;
    int cur_res;
    float sensor_real_gain;
} Camera_Sensor_PDAF_UpdateType;


typedef struct {
    int32 nFrameId;
    boolean bIsReady;
    Camera_Sensor_PDAF_UpdateType pdaf_update;
} Camera_Sensor_PDAF_StatsType;

/**
 * This configuration is used for controlling params as saturation, hue
 * of the sensor. Values can range from -1.0f to 1.0f.
 */
typedef struct
{
    float nVal;                 /// This is the value to be set.
    uint32 src_id;              /// Sensor number
} Camera_Sensor_ColorConfigType;

/**
 * This configuration is used for controlling sensor params.
 * E.g. exposure, hue, saturation.
 */
typedef struct
{
    union
    {
        /// Exposure control settings
        Camera_Sensor_ExposureConfigType sensor_exposure_config;
        /// Color control settings
        Camera_Sensor_ColorConfigType sensor_color_config;

        /// Ensure struct size will not change
        uint32_t arr_padding[32];
    }sensor_params;
    qcarcam_param_t param;
} Camera_Sensor_ParamConfigType;

/*sensor_lib types*/
typedef img_src_channel_t Camera_Sensor_ChannelType;
typedef img_src_subchannel_t Camera_Sensor_SubchannelType;

typedef struct {
    Camera_Sensor_SubchannelType subchannels[16];
    uint32 num_subchannels;
} Camera_Sensor_SubchannelsInfoType;

typedef struct {
    Camera_Sensor_ChannelType channels[16];
    uint32 num_channels;
} Camera_Sensor_ChannelsInfoType;

typedef struct {
    uint32 num_lanes;
    uint32 lane_mask;
    uint32 settle_count;
} Camera_Sensor_MipiCsiInfoType;

typedef enum
{
    INPUT_MSG_ID_LOCK_STATUS = 0,
    INPUT_MSG_ID_MODE_CHANGE,
    INPUT_MSG_ID_FATAL_ERROR
} input_message_id_t;

typedef struct
{
    uint32 device_id;                     /** input device identifier **/
    input_message_id_t message_id;  /** message id **/

    struct camera_input_status_t payload;
} input_message_t;

#endif /* __CAMERASENSORDRIVER_H_ */
