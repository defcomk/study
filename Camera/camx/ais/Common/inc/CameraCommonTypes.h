#ifndef __CAMERACOMMONTYPES_H__
#define __CAMERACOMMONTYPES_H__

/**
 * @file CameraCommonTypes.h
 *
 * @brief This file contains the common type declarations for
 *  the OpenMM camera interfaces
 *
 * Copyright (c) 2009-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#include "CameraTypes.h"

#include "qcarcam_types.h"

/* ---------------------------------------------------------------------------
** Constant / Define Declarations
** ------------------------------------------------------------------------ */
/// This is maximum size for sensor name
#define MAX_DEVICENAME_STR_SIZE (64)

/// This is the maximum number of sensor modes supported
#define MAX_NUMBER_OF_SENSOR_MODES 7

/// This is the maximum number of output formats supported
#define MAX_NUMBER_OF_OUTPUT_FORMATS 10

/// This is the maximum number of face ROIs supported
#define MAX_NUM_FACE_ROI 32

/// This is the maximum number of user ROIs supported
#define MAX_NUM_USER_ROI 32

/// This is the maximum ROI priority supported
#define MAX_ROI_PRIORITY 100

/// This is the maximum size of the circular buffer that holds the gyro samples
#define MAX_GYRO_SAMPLE_Q_SIZE 15

/*****************************************************************************
 ** Structured Data Types
 ****************************************************************************/
 /**
 * This structure defines the baf filter type
 */
typedef enum {
    BAF_FILTER_TYPE_H1,
    BAF_FILTER_TYPE_H2,
    BAF_FILTER_TYPE_V,
    BAF_FILTER_TYPE_MAX,
} Camera_BAFFilterType;

/**
* This structure defines the baf filter limitation which includes
* number of filters and if FIR or IIR filter is supported for any filter type
*/
typedef struct
{
    uint32 numOfFirAKernel;
    uint32 numOfIirAKernel;
    uint32 numOfIirBKernel;
    boolean firDisableSupported;
    boolean iirDisableSupported;
}Camera_BAFFilterLimitationInfo;

/**
* This structure defines all baf filters' limitation.
*/
typedef struct
{
    boolean isValid;
    uint32 numOfValidFilters;
    Camera_BAFFilterLimitationInfo filtersInfo[BAF_FILTER_TYPE_MAX];
}Camera_BAFFiltersLimitationInfo;

/**
* This structure defines the baf restrictions needed by 3A for validation
* space.
*/
typedef struct
{
    uint16 nMinROIW;
    uint16 nMinROIH;
    uint16 nMaxROIW; // This will be 0, if StripBuffer block hasn't been configured yet
    uint16 nMaxROIH;
    uint16 nMinLeftHOffset;
    uint16 nMinRightHOffset;
    uint16 nMinTopVOffset;
    uint16 nMinBottomVOffset;
    uint16 nMaxHOffset;
    uint16 nMaxVOffset;
    Camera_BAFFiltersLimitationInfo filtersInfo;
}Camera_BAFLimitationInfo;

/**
 * This structure defines the width and height of an object in two-dimensional
 * space.
 */
typedef qcarcam_res_t Camera_Size;


/**
 * This structure defines a rational number, where the numerator and
 * denominator are both signed integers.
 */
typedef struct
{
   /// The numerator of the fraction
   int32 num;

   /// The denominator of the fraction
   int32 denom;
} Camera_SRational;


/**
 * This structure defines a rational number, where the numerator and
 * denominator are both unsigned integers.
 */
typedef struct
{
   /// The numerator of the fraction
   uint32 num;

   /// The denominator of the fraction
   uint32 denom;
} Camera_URational;

/**
 * This structure defines a rectangle type. A rectangle with a width and
 * height of 10 is specified as {0, 0, 10, 10}, where the top left
 * corner is (0, 0) and the bottom right corner is (9, 9).
 */
typedef struct
{
   /// The x coordinate of the top left corner of the rectangle
   int32 left;
   /// The y coordinate of the top left corner of the rectangle
   int32 top;
   /// The x coordinate of the bottom right corner that lies 1 pixel outside
   /// of the rectangle
   int32 right;
   /// The y coordinate of the bottom right corner that lies 1 pixel outside
   /// of the rectangle
   int32 bottom;
} Camera_Rectangle;

/**
 * This structures defines a string type including its size.
 */
typedef struct
{
    /// The null-terminated string
    char* data;

    /// The length of the string in characters including null terminator
    int dataLen;

    /// Upon successful return this will indicate the number of characters,
    /// including the null terminator, which would have been required to store
    /// the entire result. If, upon successful return, the value is less than
    /// or equal to dataLen, then data contains the entire result; otherwise,
    /// data contains the truncated result.
    int dataLenReq;
} Camera_String;

/**
 * This structure contains information for an available camera sensor device.
 * The information can be used to differentiate between available camera
 * sensor devices.
 */
typedef struct
{
   /// This is the UID of the device driver.
   AEECLSID idCameraDeviceID;

   /// This is the device name in the format [manufacturer] [model]
   char pszDeviceName[MAX_DEVICENAME_STR_SIZE];

   /// This is the length of the device name.
   int pszDeviceNameLen;

   /// This is the required length of the device name.
   int pszDeviceNameLenReq;

   /// This is the maximum resolution output by the device.
   Camera_Size nMaximumResolution;

   /// This indicates the minimum frame interval of the device in microseconds.
   uint32 nMinimumFrameInterval;

   /// This property defines how many degrees the currently selected sensor
   /// has been panned in its installed orientation relative to a 0.0 degree
   /// default orientation, when looking at the sensor from the top.
   ///
   /// Valid values range from -180.0 to +180.0. Negative values indicate
   /// counterclockwise rotation, and positive values indicate clockwise
   /// rotation from 0.0. The 0.0 degree default orientation corresponds to
   /// the sensor's imaging plane being parallel to the plane of the display
   /// and facing the opposite direction, which is consistent with the
   /// position in the majority of digital still camera designs. +90.0
   /// degrees indicates that the sensor's imaging plane is perpendicular to
   /// the plane of the display and that it faces right (if originally
   /// looking at the back). -90.0 degrees indicates that the sensor's
   /// imaging plane is perpendicular to the plane of the display and that
   /// it faces left (if originally looking at the back).
   ///
   /// The axis of rotation is analogous to one's head looking left and right
   /// with eyes facing forward. This is a static value indicating one axis
   /// of the sensor's orientation relative to the device it is installed in.
   /// The value does not change if the orientation of the entire device
   /// changes. Any other rolling movement must be determined by an external
   /// source.
   float fPanAngle;

   /// This property defines how many degrees the currently selected sensor
   /// has been rolled in its installed orientation relative to a 0.0 degree
   /// default orientation, when looking at the sensor from the back.
   ///
   /// Valid values range from -180.0 to +180.0. Negative values indicate
   /// counterclockwise rotation, and positive values indicate clockwise
   /// rotation from 0.0. The 0.0 degree default orientation corresponds to
   /// the top edge of the sensor facing the top of the display, which is
   /// consistent with the position in the majority of digital still camera
   /// designs. +90.0 degrees indicates that the sensor has been rotated
   /// clockwise (from the back) such that its left side is facing the top of
   /// the display. -90.0 degrees indicates that the sensor has been rotated
   /// counterclockwise (from the back) such that its right side is facing
   /// the top of the display.
   ///
   /// The axis of rotation is analogous to one's left ear touching the left
   /// shoulder or one's right ear touching the right shoulder. This is a
   /// static value indicating one axis of the sensor's orientation relative
   /// to the device it is installed in. The value does not change if the
   /// orientation of the entire device changes. Any other rolling movement
   /// must be determined by an external source.

   float fRollAngle;
   /// This property defines how many degrees the currently selected sensor
   /// has been tilted in its installed orientation relative to a 0.0 degree
   /// default orientation, when looking at the sensor from the right side
   /// (if originally looking at the back).
   ///
   /// Valid values range from -180.0 to +180.0. Negative values indicate
   /// counterclockwise rotation, and positive values indicate clockwise
   /// rotation from 0.0. The 0.0 degree default orientation corresponds to
   /// the sensor's imaging plane being parallel to the plane of the display
   /// and facing the opposite direction, which is consistent with the
   /// majority of digital still camera designs. +90.0 degrees indicates that
   /// the sensor's imaging plane is perpendicular to the plane of the
   /// display and faces upward. -90.0 degrees indicates that the sensor's
   /// imaging plane is perpendicular to the plane of the display and faces
   /// downward.
   ///
   /// The axis of rotation is analogous to one's head looking up and down
   /// with eyes facing forward. This is a static value indicating one axis
   /// of the sensor's orientation relative to the device it is installed
   /// in. The value does not change if the orientation of the entire device
   /// changes. Any other rolling movement must be determined by an external
   /// source.
   float fTiltAngle;

   /// This shows whether the device supports auto focus
   boolean bAutoFocus;


   /// Flag to indicate touch focus support of the device.
   boolean bTouchFocus;

   /// Flag to indicate continuous focus support of the device.
   boolean bContinuousFocus;
} Camera_CameraSensorDeviceInfoType;

/**
 * This enumerates the possible flash device types.
 */
typedef enum
{
   Camera_FLASH_DEVICE_TYPE_INVALID,           ///< Uninitialized value
   Camera_FLASH_DEVICE_TYPE_NONE,              ///< No flash
   Camera_FLASH_DEVICE_TYPE_LED,               ///< LED flash
   Camera_FLASH_DEVICE_TYPE_STROBE,            ///< Strobe flash
   Camera_FLASH_DEVICE_TYPE_LED_AND_STROBE     ///< LED and strobe flash
} Camera_FlashDeviceType;

/**
 * This structure contains information for an available flash device. The
 * information can be used to differentiate between available flash devices.
 */
typedef struct
{
    /// This is the type of flash device that is available.
   Camera_FlashDeviceType deviceType;
   /// This is the UID of the flash device driver.
   AEECLSID idFlashDeviceID;
} Camera_FlashDeviceInfoType;

/// This property defines the best-shot mode, e.g., beach, flower, landscape.
/// These modes adjust camera settings to preset values, overriding other
/// camera properties.
/// @par Default Value:
///   BEST_SHOT_MODE_OFF
typedef enum
{
    /// Do not use best-shot mode or autoscene scene detection
    Camera_BEST_SHOT_MODE_OFF,
    /// Auto scene detection only
    Camera_BEST_SHOT_MODE_AUTO,
    /// Antishake best-shot mode
    Camera_BEST_SHOT_MODE_ANTISHAKE,
    /// Backlight best-shot mode
    Camera_BEST_SHOT_MODE_BACKLIGHT,
    /// Beach best-shot mode
    Camera_BEST_SHOT_MODE_BEACH,
    /// Candle-light best-shot mode
    Camera_BEST_SHOT_MODE_CANDLELIGHT,
    /// Flower best-shot mode
    Camera_BEST_SHOT_MODE_FLOWER,
    /// Landscape best-shot mode
    Camera_BEST_SHOT_MODE_LANDSCAPE,
    /// Night best-shot mode
    Camera_BEST_SHOT_MODE_NIGHT,
    /// Portrait best-shot mode
    Camera_BEST_SHOT_MODE_PORTRAIT,
    /// Snow best-shot mode
    Camera_BEST_SHOT_MODE_SNOW,
    /// Sports best-shot mode
    Camera_BEST_SHOT_MODE_SPORTS,
    /// Sunset best-shot mode
    Camera_BEST_SHOT_MODE_SUNSET,
    /// Whiteboard best-shot mode
    Camera_BEST_SHOT_MODE_WHITEBOARD,
    /// Barcode best-shot mode
    Camera_BEST_SHOT_MODE_BARCODE
} Camera_BestShotMode;

/// This enumerates the operational modes
typedef enum
{
    /// Video preview operational mode
    Camera_OPERATIONAL_MODE_VIDEO_PREVIEW,

    /// Video capture operational mode
    Camera_OPERATIONAL_MODE_VIDEO_CAPTURE,

    /// Still operational mode
    Camera_OPERATIONAL_MODE_STILL,

    /// Offline operational mode
    Camera_OPERATIONAL_MODE_OFFLINE,

    /// Invalid mode
    Camera_OPERATIONAL_MODE_INVALID
} Camera_OperationalMode;

/// This property is used to inform the core camera if the camera is expected
/// to operate in preview mode or video capture mode when OnStartVideo is
/// called. While it is not necessary to set this property to be able to
/// encode frames or take snapshots, by setting this property the camera can
/// optimize its performance for the given use case.
/// @par Default Value:
///   VIDEO_MODE_PREVIEW
typedef enum
{
    /// Preview (aka viewfinder) video mode
    Camera_VIDEO_MODE_PREVIEW,
    /// Capture (aka, camcorder or video record) video mode
    Camera_VIDEO_MODE_CAPTURE
} Camera_VideoMode;

/// This property defines the status of the connection between the camera
/// and ADSP sensor sub-system
typedef enum
{
    /// No connection has been made yet
    Camera_ADSPSENSOR_NOT_CONNECTED = 0,

    /// Connection established and is active
    Camera_ADSPSENSOR_CONNECTED,

    /// Connection broken
    Camera_ADSPSENSOR_CONNECTION_BROKEN
} Camera_ADSPSensor_Connection_Status;

/// This property defines a gyro sample as obtained from the ADSP
/// sensor sub-system
typedef struct
{
    // Index 0 -> X, Index 1 -> Y, Index 2 -> Z
    int32 nGyroData[3];

    // Timestamp value expressed in nanoseconds
    uint64  nTimeStamp;
} Camera_GyroSample;

/// This property defines a time interval as specified by the start
/// and end time values
typedef struct
{
    // Start time
    uint64 nStartTime;

    // End time
    uint64 nEndTime;
} Camera_TimeInterval;

typedef enum
{
    CAMERA_CHROMATIX_PARAMS = 0,
    CAMERA_CHROMATIX_VFE_COMMON_PARAMS,
    CAMERA_CHROMATIX_3A_PARAMS,
    CAMERA_CHROMATIX_ALL,
    CAMERA_CHROMATIX_MAX
}Camera_ChromatixType;

typedef enum
{
    CSIPHY_CORE_0 = 0,
    CSIPHY_CORE_1,
    CSIPHY_CORE_2,
#ifndef TARGET_SM6150
    CSIPHY_CORE_3,
#endif
    CSIPHY_CORE_MAX
} CsiphyCoreType;

typedef enum
{
    IFE_CORE_0 = 0,
    IFE_CORE_1,
    IFE_CORE_2,
#ifndef TARGET_SM6150
    IFE_CORE_3,
#endif
    IFE_CORE_MAX
}IfeCoreType;

typedef enum
{
    IFE_OUTPUT_PATH_RDI0,
    IFE_OUTPUT_PATH_RDI1,
    IFE_OUTPUT_PATH_RDI2,
    IFE_OUTPUT_PATH_RDI3,
    IFE_OUTPUT_PATH_MAX,
    IFE_OUTPUT_PATH_INVALID = 0X7FFFFFFF
}IfeOutputPathType;

typedef enum
{
    IFE_INTF_PIX = 0,
    IFE_INTF_RDI0,
    IFE_INTF_RDI1,
    IFE_INTF_RDI2,
    IFE_INTF_RDI3,
    IFE_INTF_ALL,
    IFE_INTF_MAX
}IfeInterfaceType;

#endif /* __CAMERACOMMONTYPES_H__ */
