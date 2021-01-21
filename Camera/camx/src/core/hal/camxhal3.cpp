////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxhal3.cpp
/// @brief Landing methods for CamX implementation of HAL3
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOWHINE FILE NC010: Google Types
// NOWHINE FILE GR017: Google types
// NOWHINE FILE NC011: Google calls it 'init'

#include <system/camera_metadata.h>

#include "camera_metadata_hidden.h"

#include "chi.h"
#include "camxentry.h"
#include "camxhal3.h"
#include "camxhal3defaultrequest.h"
#include "camxhal3module.h"
#include "camxhaldevice.h"
#include "camxincs.h"
#include "camxvendortags.h"

CAMX_NAMESPACE_BEGIN

// Number of settings tokens
static const UINT32 NumExtendSettings = static_cast<UINT32>(ChxSettingsToken::CHX_SETTINGS_TOKEN_COUNT);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GetHALDevice
///
/// @brief  Extract the HAL device pointer from the camera3_device_t.
///         Precondition: pCamera3DeviceAPI has been checked for NULL
///
/// @param  pCamera3DeviceAPI The camera3_device_t pointer passed in from the application framework. Assumed that it has been
///                           checked against NULL.
///
/// @return A pointer to the HALSession object held in the opaque point in camera3_device_t.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static CAMX_INLINE HALDevice* GetHALDevice(
    const camera3_device_t* pCamera3DeviceAPI)
{
    CAMX_ASSERT(NULL != pCamera3DeviceAPI);

    return reinterpret_cast<HALDevice*>(pCamera3DeviceAPI->priv);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FormatToString
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char* FormatToString(
    int format)
{
    const char* pFormatStr;

    switch (format)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            pFormatStr = "HAL_PIXEL_FORMAT_RGBA_8888";
            break;
        case HAL_PIXEL_FORMAT_RGBX_8888:
            pFormatStr = "HAL_PIXEL_FORMAT_RGBX_8888";
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
            pFormatStr = "HAL_PIXEL_FORMAT_RGB_888";
            break;
        case HAL_PIXEL_FORMAT_RGB_565:
            pFormatStr = "HAL_PIXEL_FORMAT_RGB_565";
            break;
        case HAL_PIXEL_FORMAT_BGRA_8888:
            pFormatStr = "HAL_PIXEL_FORMAT_BGRA_8888";
            break;
        case HAL_PIXEL_FORMAT_YV12:
            pFormatStr = "HAL_PIXEL_FORMAT_YV12";
            break;
        case HAL_PIXEL_FORMAT_Y8:
            pFormatStr = "HAL_PIXEL_FORMAT_Y8";
            break;
        case HAL_PIXEL_FORMAT_Y16:
            pFormatStr = "HAL_PIXEL_FORMAT_Y16";
            break;
        case HAL_PIXEL_FORMAT_RAW16:
            pFormatStr = "HAL_PIXEL_FORMAT_RAW16";
            break;
        case HAL_PIXEL_FORMAT_RAW10:
            pFormatStr = "HAL_PIXEL_FORMAT_RAW10";
            break;
        case HAL_PIXEL_FORMAT_RAW12:
            pFormatStr = "HAL_PIXEL_FORMAT_RAW12";
            break;
        case HAL_PIXEL_FORMAT_RAW_OPAQUE:
            pFormatStr = "HAL_PIXEL_FORMAT_RAW_OPAQUE";
            break;
        case HAL_PIXEL_FORMAT_BLOB:
            pFormatStr = "HAL_PIXEL_FORMAT_BLOB";
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            pFormatStr = "HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED";
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            pFormatStr = "HAL_PIXEL_FORMAT_YCbCr_420_888";
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_888:
            pFormatStr = "HAL_PIXEL_FORMAT_YCbCr_422_888";
            break;
        case HAL_PIXEL_FORMAT_YCbCr_444_888:
            pFormatStr = "HAL_PIXEL_FORMAT_YCbCr_444_888";
            break;
        case HAL_PIXEL_FORMAT_FLEX_RGB_888:
            pFormatStr = "HAL_PIXEL_FORMAT_FLEX_RGB_888";
            break;
        case HAL_PIXEL_FORMAT_FLEX_RGBA_8888:
            pFormatStr = "HAL_PIXEL_FORMAT_FLEX_RGBA_8888";
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_SP:
            pFormatStr = "HAL_PIXEL_FORMAT_YCbCr_422_SP";
            break;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            pFormatStr = "HAL_PIXEL_FORMAT_YCrCb_420_SP";
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            pFormatStr = "HAL_PIXEL_FORMAT_YCbCr_422_I";
            break;
        default:
            pFormatStr = "Unknown Format";
            break;
    }

    return pFormatStr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// StreamTypeToString
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char* StreamTypeToString(
    int streamType)
{
    const char* pStreamTypeStr;

    switch (streamType)
    {
        case CAMERA3_STREAM_OUTPUT:
            pStreamTypeStr = "CAMERA3_STREAM_OUTPUT";
            break;
        case CAMERA3_STREAM_INPUT:
            pStreamTypeStr = "CAMERA3_STREAM_INPUT";
            break;
        case CAMERA3_STREAM_BIDIRECTIONAL:
            pStreamTypeStr = "CAMERA3_STREAM_BIDIRECTIONAL";
            break;
        default:
            pStreamTypeStr = "Unknown Stream Type";
            break;
    }

    return pStreamTypeStr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DataSpaceToString
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char* DataSpaceToString(
    android_dataspace_t dataSpace)
{
    const char* pDataSpaceStr;

    switch (dataSpace)
    {
        case HAL_DATASPACE_ARBITRARY:
            pDataSpaceStr = "HAL_DATASPACE_ARBITRARY";
            break;
        case HAL_DATASPACE_STANDARD_BT709:
            pDataSpaceStr = "HAL_DATASPACE_STANDARD_BT709";
            break;
        case HAL_DATASPACE_STANDARD_BT601_625:
            pDataSpaceStr = "HAL_DATASPACE_STANDARD_BT601_625";
            break;
        case HAL_DATASPACE_STANDARD_BT601_625_UNADJUSTED:
            pDataSpaceStr = "HAL_DATASPACE_STANDARD_BT601_625_UNADJUSTED";
            break;
        case HAL_DATASPACE_STANDARD_BT601_525:
            pDataSpaceStr = "HAL_DATASPACE_STANDARD_BT601_525";
            break;
        case HAL_DATASPACE_STANDARD_BT601_525_UNADJUSTED:
            pDataSpaceStr = "HAL_DATASPACE_STANDARD_BT601_525_UNADJUSTED";
            break;
        case HAL_DATASPACE_STANDARD_BT2020:
            pDataSpaceStr = "HAL_DATASPACE_STANDARD_BT2020";
            break;
        case HAL_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE:
            pDataSpaceStr = "HAL_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE";
            break;
        case HAL_DATASPACE_STANDARD_BT470M:
            pDataSpaceStr = "HAL_DATASPACE_STANDARD_BT470M";
            break;
        case HAL_DATASPACE_STANDARD_FILM:
            pDataSpaceStr = "HAL_DATASPACE_STANDARD_FILM";
            break;
        case HAL_DATASPACE_TRANSFER_LINEAR:
            pDataSpaceStr = "HAL_DATASPACE_TRANSFER_LINEAR";
            break;
        case HAL_DATASPACE_TRANSFER_SRGB:
            pDataSpaceStr = "HAL_DATASPACE_TRANSFER_SRGB";
            break;
        case HAL_DATASPACE_TRANSFER_SMPTE_170M:
            pDataSpaceStr = "HAL_DATASPACE_TRANSFER_SMPTE_170M";
            break;
        case HAL_DATASPACE_TRANSFER_GAMMA2_2:
            pDataSpaceStr = "HAL_DATASPACE_TRANSFER_GAMMA2_2";
            break;
        case HAL_DATASPACE_TRANSFER_GAMMA2_8:
            pDataSpaceStr = "HAL_DATASPACE_TRANSFER_GAMMA2_8";
            break;
        case HAL_DATASPACE_TRANSFER_ST2084:
            pDataSpaceStr = "HAL_DATASPACE_TRANSFER_ST2084";
            break;
        case HAL_DATASPACE_TRANSFER_HLG:
            pDataSpaceStr = "HAL_DATASPACE_TRANSFER_HLG";
            break;
        case HAL_DATASPACE_RANGE_FULL:
            pDataSpaceStr = "HAL_DATASPACE_RANGE_FULL";
            break;
        case HAL_DATASPACE_RANGE_LIMITED:
            pDataSpaceStr = "HAL_DATASPACE_RANGE_LIMITED";
            break;
        case HAL_DATASPACE_SRGB_LINEAR:
            pDataSpaceStr = "HAL_DATASPACE_SRGB_LINEAR";
            break;
        case HAL_DATASPACE_V0_SRGB_LINEAR:
            pDataSpaceStr = "HAL_DATASPACE_V0_SRGB_LINEAR";
            break;
        case HAL_DATASPACE_SRGB:
            pDataSpaceStr = "HAL_DATASPACE_SRGB";
            break;
        case HAL_DATASPACE_V0_SRGB:
            pDataSpaceStr = "HAL_DATASPACE_V0_SRGB";
            break;
        case HAL_DATASPACE_JFIF:
            pDataSpaceStr = "HAL_DATASPACE_JFIF";
            break;
        case HAL_DATASPACE_V0_JFIF:
            pDataSpaceStr = "HAL_DATASPACE_V0_JFIF";
            break;
        case HAL_DATASPACE_BT601_625:
            pDataSpaceStr = "HAL_DATASPACE_BT601_625";
            break;
        case HAL_DATASPACE_V0_BT601_625:
            pDataSpaceStr = "HAL_DATASPACE_V0_BT601_625";
            break;
        case HAL_DATASPACE_BT601_525:
            pDataSpaceStr = "HAL_DATASPACE_BT601_525";
            break;
        case HAL_DATASPACE_V0_BT601_525:
            pDataSpaceStr = "HAL_DATASPACE_V0_BT601_525";
            break;
        case HAL_DATASPACE_BT709:
            pDataSpaceStr = "HAL_DATASPACE_BT709";
            break;
        case HAL_DATASPACE_V0_BT709:
            pDataSpaceStr = "HAL_DATASPACE_V0_BT709";
            break;
        case HAL_DATASPACE_DEPTH:
            pDataSpaceStr = "HAL_DATASPACE_DEPTH";
            break;
        case HAL_DATASPACE_UNKNOWN:
            // case HAL_DATASPACE_STANDARD_UNSPECIFIED:
            // case HAL_DATASPACE_RANGE_UNSPECIFIED:
            // case HAL_DATASPACE_TRANSFER_UNSPECIFIED:
            // case HAL_DATASPACE_STANDARD_UNSPECIFIED:
        default:
            pDataSpaceStr = "HAL_DATASPACE_UNKNOWN";
            break;
    }

    return pDataSpaceStr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RotationToString
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* RotationToString(
    int rotation)
{
    const char* pRotationStr;

    switch (rotation)
    {
        case CAMERA3_STREAM_ROTATION_0:
            pRotationStr = "CAMERA3_STREAM_ROTATION_0";
            break;
        case CAMERA3_STREAM_ROTATION_90:
            pRotationStr = "CAMERA3_STREAM_ROTATION_90";
            break;
        case CAMERA3_STREAM_ROTATION_180:
            pRotationStr = "CAMERA3_STREAM_ROTATION_180";
            break;
        case CAMERA3_STREAM_ROTATION_270:
            pRotationStr = "CAMERA3_STREAM_ROTATION_270";
            break;
        default:
            pRotationStr = "Unknown Rotation Angle";
            break;
    }
    return pRotationStr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// hw_module_methods_t entry points
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GetCHIAppCallbacks
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static inline chi_hal_callback_ops_t* GetCHIAppCallbacks()
{
    return  HAL3Module::GetInstance()->GetCHIAppCallbacks();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// open
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int open(
    const struct hw_module_t*   pHwModuleAPI,
    const char*                 pCameraIdAPI,
    struct hw_device_t**        ppHwDeviceAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3Open);

    CamxResult result = CamxResultSuccess;
    CAMX_ASSERT(NULL != pHwModuleAPI);
    CAMX_ASSERT(NULL != pHwModuleAPI->id);
    CAMX_ASSERT(NULL != pHwModuleAPI->name);
    CAMX_ASSERT(NULL != pHwModuleAPI->author);
    CAMX_ASSERT(NULL != pHwModuleAPI->methods);
    CAMX_ASSERT('\0' != pCameraIdAPI[0]);
    CAMX_ASSERT(NULL != pCameraIdAPI);
    CAMX_ASSERT(NULL != ppHwDeviceAPI);

    if ((NULL != pHwModuleAPI) &&
        (NULL != pHwModuleAPI->id) &&
        (NULL != pHwModuleAPI->name) &&
        (NULL != pHwModuleAPI->author) &&
        (NULL != pHwModuleAPI->methods) &&
        (NULL != pCameraIdAPI) &&
        ('\0' != pCameraIdAPI[0]) &&
        (NULL != ppHwDeviceAPI))
    {
        UINT32  cameraId        = 0;
        UINT32  logicalCameraId = 0;
        CHAR*   pNameEnd    = NULL;

        cameraId = OsUtils::StrToUL(pCameraIdAPI, &pNameEnd, 10);
        if (*pNameEnd != '\0')
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid camera id: %s", pCameraIdAPI);
            // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
            result = CamxResultEInvalidArg;
        }

        if (CamxResultSuccess == result)
        {
            // Framework camera ID should only be known to these static landing functions, and the remap function
            logicalCameraId = GetCHIAppCallbacks()->chi_remap_camera_id(cameraId, IdRemapCamera);

            // Reserve the Torch resource for camera.
            // If torch already switched on, then turn it off and reserve for camera.
            HAL3Module::GetInstance()->ReserveTorchForCamera(
                GetCHIAppCallbacks()->chi_remap_camera_id(cameraId, IdRemapTorch), cameraId);

            // Sample code to show how the VOID* can be used in ExtendOpen
            ChiOverrideExtendOpen extend                  = { 0 };
            ChiOverrideToken tokenList[NumExtendSettings] = { { 0 } };
            extend.pTokens                                = tokenList;

            GenerateExtendOpenData(NumExtendSettings, &extend);

            // Reserve the camera to detect if it is already open or too many concurrent are open
            CAMX_LOG_CONFIG(CamxLogGroupHAL, "HalOp: Begin OPEN, logicalCameraId: %d, cameraId: %d",
                            logicalCameraId, cameraId);
            result = HAL3Module::GetInstance()->ProcessCameraOpen(logicalCameraId, &extend);
        }

        if (CamxResultSuccess == result)
        {
            // Sample code to show how the VOID* can be used in ModifySettings
            ChiOverrideModifySetting setting[NumExtendSettings] = { { { 0 } } };
            GenerateModifySettingsData(setting);

            for (UINT i = 0; i < NumExtendSettings; i++)
            {
                GetCHIAppCallbacks()->chi_modify_settings(&setting[i]);
            }

            const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

            CAMX_LOG_INFO(CamxLogGroupHAL, "Open: overrideCameraClose is %d , overrideCameraOpen is %d ",
                pStaticSettings->overrideCameraClose, pStaticSettings->overrideCameraOpen);

            const HwModule* pHwModule  = reinterpret_cast<const HwModule*>(pHwModuleAPI);
            HALDevice*      pHALDevice = HALDevice::Create(pHwModule, logicalCameraId, cameraId);

            if (NULL != pHALDevice)
            {
                camera3_device_t* pCamera3Device = reinterpret_cast<camera3_device_t*>(pHALDevice->GetCameraDevice());
                *ppHwDeviceAPI = &pCamera3Device->common;
            }
            else
            {
                // HAL interface requires -ENODEV (EFailed) for all other internal errors
                result = CamxResultEFailed;
                CAMX_LOG_ERROR(CamxLogGroupHAL, "Error while opening camera");

                ChiOverrideExtendClose extend = { 0 };
                ChiOverrideToken tokenList[NumExtendSettings] = { { 0 } };
                extend.pTokens = tokenList;

                GenerateExtendCloseData(NumExtendSettings, &extend);

                // Allow the camera to be reopened later
                HAL3Module::GetInstance()->ProcessCameraClose(logicalCameraId, &extend);

                ChiOverrideModifySetting setting[NumExtendSettings] = { { { 0 } } };
                GenerateModifySettingsData(setting);

                for (UINT i = 0; i < NumExtendSettings; i++)
                {
                    GetCHIAppCallbacks()->chi_modify_settings(&setting[i]);
                }
            }
        }

        if (CamxResultSuccess != result)
        {
            // If open fails, then release the Torch resource that we reserved.
            HAL3Module::GetInstance()->ReleaseTorchForCamera(
                GetCHIAppCallbacks()->chi_remap_camera_id(cameraId, IdRemapTorch), cameraId);
        }
        CAMX_LOG_CONFIG(CamxLogGroupHAL, "HalOp: End OPEN, logicalCameraId: %d, cameraId: %d",
                        logicalCameraId, cameraId);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument(s) for open()");
        // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
        result = CamxResultEInvalidArg;
    }

    return Utils::CamxResultToErrno(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// vendor_tag_ops_t entry points
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get_tag_count
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int get_tag_count(
    const vendor_tag_ops_t* pVendorTagOpsAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3GetTagCount);

    // Per Google, this parameter is extraneous
    CAMX_UNREFERENCED_PARAM(pVendorTagOpsAPI);

    return static_cast<int>(VendorTagManager::GetTagCount(TagSectionVisibility::TagSectionVisibleToAll));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get_all_tags
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void get_all_tags(
    const vendor_tag_ops_t* pVendorTagOpsAPI,
    uint32_t*               pTagArrayAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3GetAllTags);

    // Per Google, this parameter is extraneous
    CAMX_UNREFERENCED_PARAM(pVendorTagOpsAPI);

    CAMX_STATIC_ASSERT(sizeof(pTagArrayAPI[0]) == sizeof(VendorTag));

    CAMX_ASSERT(NULL != pTagArrayAPI);
    if (NULL != pTagArrayAPI)
    {
        VendorTag* pVendorTags = static_cast<VendorTag*>(pTagArrayAPI);
        VendorTagManager::GetAllTags(pVendorTags, TagSectionVisibility::TagSectionVisibleToAll);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument 2 for get_all_tags()");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get_section_name
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char* get_section_name(
    const vendor_tag_ops_t* pVendorTagOpsAPI,
    uint32_t                tagAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3GetSectionName);

    // Per Google, this parameter is extraneous
    CAMX_UNREFERENCED_PARAM(pVendorTagOpsAPI);

    CAMX_STATIC_ASSERT(sizeof(tagAPI) == sizeof(VendorTag));

    return VendorTagManager::GetSectionName(static_cast<VendorTag>(tagAPI));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get_tag_name
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char* get_tag_name(
    const vendor_tag_ops_t* pVendorTagOpsAPI,
    uint32_t                tagAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3GetTagName);

    // Per Google, this parameter is extraneous
    CAMX_UNREFERENCED_PARAM(pVendorTagOpsAPI);

    CAMX_STATIC_ASSERT(sizeof(tagAPI) == sizeof(VendorTag));

    return VendorTagManager::GetTagName(static_cast<VendorTag>(tagAPI));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get_tag_type
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int get_tag_type(
    const vendor_tag_ops_t* pVendorTagOpsAPI,
    uint32_t                tagAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3GetTagType);

    // Per Google, this parameter is extraneous
    CAMX_UNREFERENCED_PARAM(pVendorTagOpsAPI);

    CAMX_STATIC_ASSERT(sizeof(tagAPI) == sizeof(VendorTag));

    VendorTagType vendorTagType = VendorTagManager::GetTagType(static_cast<VendorTag>(tagAPI));
    return static_cast<int>(vendorTagType);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// camera_module_t entry points
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get_number_of_cameras
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int get_number_of_cameras(void)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3GetNumberOfCameras);

    return static_cast<int>(HAL3Module::GetInstance()->GetNumCameras());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get_camera_info
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int get_camera_info(
    int                 cameraIdAPI,
    struct camera_info* pInfoAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3GetCameraInfo);

    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pInfoAPI);

    if (NULL != pInfoAPI)
    {
        UINT32      cameraId        = static_cast<UINT32>(cameraIdAPI);
        UINT32      logicalCameraId = GetCHIAppCallbacks()->chi_remap_camera_id(cameraId, IdRemapCamera);

        CameraInfo* pInfo       = reinterpret_cast<CameraInfo*>(pInfoAPI);
        result = HAL3Module::GetInstance()->GetCameraInfo(logicalCameraId, pInfo);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL, "HAL3Module::GetCameraInfo() failed with result %s",
                           Utils::CamxResultToString(result));
            // HAL interface requires -ENODEV (EFailed) for all other failures
            result = CamxResultEFailed;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument 2 for get_camera_info()");
        // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
        result = CamxResultEInvalidArg;
    }

    return Utils::CamxResultToErrno(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set_callbacks
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int set_callbacks(
    const camera_module_callbacks_t* pModuleCbsAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3SetCallbacks);

    CamxResult result = CamxResultSuccess;

    result = HAL3Module::GetInstance()->SetCbs(pModuleCbsAPI);
    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "HAL3Module::SetCbs() failed with result %s", Utils::CamxResultToString(result));
        // HAL interface requires -ENODEV (EFailed) for all other failures
        result = CamxResultEFailed;
    }

    return Utils::CamxResultToErrno(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get_vendor_tag_ops
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void get_vendor_tag_ops(
    vendor_tag_ops_t* pVendorTagOpsAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3GetVendorTagOps);

    CAMX_ASSERT(NULL != pVendorTagOpsAPI);

    if (NULL != pVendorTagOpsAPI)
    {
        pVendorTagOpsAPI->get_tag_count     = get_tag_count;
        pVendorTagOpsAPI->get_all_tags      = get_all_tags;
        pVendorTagOpsAPI->get_section_name  = get_section_name;
        pVendorTagOpsAPI->get_tag_name      = get_tag_name;
        pVendorTagOpsAPI->get_tag_type      = get_tag_type;
        pVendorTagOpsAPI->reserved[0]       = NULL;
    }

    /// @todo (CAMX-1223) Remove below and set the vendor tag ops in hal3test
    // This is the workaround for presil HAL3test on Windows
    // On Device, set_camera_metadata_vendor_ops will be call the set the
    // static vendor tag operation in camera_metadata.c
    //
    // On Windows side, theoretically hal3test should mimic what Android framework
    // does and call the set_camera_metadata_vendor_ops function in libcamxext library
    // However, in Windows, if both hal3test.exe and hal.dll link to libcamxext library,
    // there are two different instance of static varibles sit in different memory location.
    // Even if set_camera_metadata_vendor_ops is called in hal3test, when hal try to
    // access to vendor tag ops, it is still not set.
    set_camera_metadata_vendor_ops(pVendorTagOpsAPI);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// open_legacy
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int open_legacy(
    const struct hw_module_t*   pHwModuleAPI,
    const char*                 pCameraIdAPI,
    uint32_t                    halVersion,
    struct hw_device_t**        ppHwDeviceAPI)
{
    CAMX_UNREFERENCED_PARAM(pHwModuleAPI);
    CAMX_UNREFERENCED_PARAM(pCameraIdAPI);
    CAMX_UNREFERENCED_PARAM(halVersion);
    CAMX_UNREFERENCED_PARAM(ppHwDeviceAPI);

    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3OpenLegacy);

    /// Intentionally do nothing. HAL interface requires -ENOSYS (ENotImplemented) result code.

    return Utils::CamxResultToErrno(CamxResultENotImplemented);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set_torch_mode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int set_torch_mode(
    const char* pCameraIdAPI,
    bool        enabledAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3SetTorchMode);

    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pCameraIdAPI);
    CAMX_ASSERT('\0' != pCameraIdAPI[0]);

    if ((NULL != pCameraIdAPI) &&
        ('\0' != pCameraIdAPI[0]))
    {
        UINT32  cameraId    = 0;
        CHAR*   pNameEnd    = NULL;
        cameraId = OsUtils::StrToUL(pCameraIdAPI, &pNameEnd, 10);

        if (*pNameEnd != '\0')
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid camera id: %s", pCameraIdAPI);
            // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
            result = CamxResultEInvalidArg;
        }

        if (CamxResultSuccess == result)
        {
            UINT32 logicalCameraId = GetCHIAppCallbacks()->chi_remap_camera_id(cameraId, IdRemapTorch);
            BOOL enableTorch = (true == enabledAPI) ? TRUE : FALSE;

            result = HAL3Module::GetInstance()->SetTorchMode(logicalCameraId, cameraId, enableTorch);
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument 1 for set_torch_mode()");
        // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
        result = CamxResultEInvalidArg;
    }

    return Utils::CamxResultToErrno(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// init
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int init()
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3Init);

    /// Intentionally do nothing for now. Left as a placeholder for future use.
    /// @todo (CAMX-264) - Determine what, if any, CSL initialization should be done after module load

    return Utils::CamxResultToErrno(CamxResultSuccess);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// hw_device_t entry points
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// close
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int close(
    struct hw_device_t* pHwDeviceAPI)
{
    CamxResult result = CamxResultSuccess;
    {
        CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3Close);

        CAMX_ASSERT(NULL != pHwDeviceAPI);

        CAMX_LOG_INFO(CamxLogGroupHAL, "close(): %p", pHwDeviceAPI);

        if (NULL != pHwDeviceAPI)
        {
            camera3_device_t* pCamera3Device  = reinterpret_cast<camera3_device_t*>(pHwDeviceAPI);
            HALDevice*     pHALDevice      = static_cast<HALDevice*>(pCamera3Device->priv);

            CAMX_LOG_CONFIG(CamxLogGroupHAL, "HalOp: Begin CLOSE, logicalCameraId: %d, cameraId: %d",
                            pHALDevice->GetCameraId(), pHALDevice->GetFwCameraId());

            CAMX_ASSERT(NULL != pHALDevice);

            if (NULL != pHALDevice)
            {
                // Sample code to show how the VOID* can be used in ExtendOpen
                ChiOverrideExtendClose extend = { 0 };
                ChiOverrideToken tokenList[NumExtendSettings] = { { 0 } };
                extend.pTokens = tokenList;

                GenerateExtendCloseData(NumExtendSettings, &extend);

                // Allow the camera to be reopened
                HAL3Module::GetInstance()->ProcessCameraClose(pHALDevice->GetCameraId(), &extend);

                // Sample code to show how the VOID* can be used in ModifySettings
                ChiOverrideModifySetting setting[NumExtendSettings] = { { { 0 } } };
                GenerateModifySettingsData(setting);

                for (UINT i = 0; i < NumExtendSettings; i++)
                {
                    GetCHIAppCallbacks()->chi_modify_settings(&setting[i]);
                }
                const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();
                CAMX_LOG_INFO(CamxLogGroupHAL, "Close: overrideCameraClose is %d , overrideCameraOpen is %d ",
                    pStaticSettings->overrideCameraClose, pStaticSettings->overrideCameraOpen);

                result = pHALDevice->Close();

                CAMX_LOG_VERBOSE(CamxLogGroupHAL, "Close done on cameraId %d", pHALDevice->GetCameraId());
                pHALDevice->CloseCachedSensorHandles(pHALDevice->GetCameraId());

                CAMX_LOG_CONFIG(CamxLogGroupHAL, "HalOp: End CLOSE, logicalCameraId: %d, cameraId: %d",
                                pHALDevice->GetCameraId(), pHALDevice->GetFwCameraId());
                // Unconditionally destroy the HALSession object
                pHALDevice->Destroy();
                pHALDevice = NULL;
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument 1 for close()");
                result = CamxResultEInvalidArg;
            }
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument 1 for close()");
            result = CamxResultEInvalidArg;
        }
    }

    return Utils::CamxResultToErrno(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// camera3_device_ops_t entry points
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int initialize(
    const struct camera3_device*    pCamera3DeviceAPI,
    const camera3_callback_ops_t*   pCamera3CbOpsAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3Initialize);

    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pCamera3DeviceAPI);
    CAMX_ASSERT(NULL != pCamera3DeviceAPI->priv);

    CAMX_LOG_INFO(CamxLogGroupHAL, "initialize(): %p, %p", pCamera3DeviceAPI, pCamera3CbOpsAPI);

    if ((NULL != pCamera3DeviceAPI) &&
        (NULL != pCamera3DeviceAPI->priv))
    {
        HALDevice* pHALDevice = GetHALDevice(pCamera3DeviceAPI);
        pHALDevice->SetCallbackOps(pCamera3CbOpsAPI);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument(s) for initialize()");
        // HAL interface requires -ENODEV (EFailed) if initialization fails for any reason, including invalid arguments.
        result = CamxResultEFailed;
    }

    return Utils::CamxResultToErrno(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// configure_streams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int configure_streams(
    const struct camera3_device*    pCamera3DeviceAPI,
    camera3_stream_configuration_t* pStreamConfigsAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3ConfigureStreams);

    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pCamera3DeviceAPI);
    CAMX_ASSERT(NULL != pCamera3DeviceAPI->priv);
    CAMX_ASSERT(NULL != pStreamConfigsAPI);
    CAMX_ASSERT(pStreamConfigsAPI->num_streams > 0);
    CAMX_ASSERT(NULL != pStreamConfigsAPI->streams);

    if ((NULL != pCamera3DeviceAPI)          &&
        (NULL != pCamera3DeviceAPI->priv)    &&
        (NULL != pStreamConfigsAPI)          &&
        (pStreamConfigsAPI->num_streams > 0) &&
        (NULL != pStreamConfigsAPI->streams))
    {
        CAMX_LOG_INFO(CamxLogGroupHAL, "Number of streams: %d", pStreamConfigsAPI->num_streams);

        HALDevice* pHALDevice = GetHALDevice(pCamera3DeviceAPI);

        CAMX_LOG_CONFIG(CamxLogGroupHAL, "HalOp: Begin CONFIG: %p, logicalCameraId: %d, cameraId: %d",
                        pCamera3DeviceAPI, pHALDevice->GetCameraId(), pHALDevice->GetFwCameraId());

        for (UINT32 stream = 0; stream < pStreamConfigsAPI->num_streams; stream++)
        {
            CAMX_ASSERT(NULL != pStreamConfigsAPI->streams[stream]);

            if (NULL == pStreamConfigsAPI->streams[stream])
            {
                CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument 2 for configure_streams()");
                // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
                result = CamxResultEInvalidArg;
                break;
            }
            else
            {
                CAMX_LOG_INFO(CamxLogGroupHAL, "  stream[%d] = %p - info:", stream,
                    pStreamConfigsAPI->streams[stream]);
                CAMX_LOG_INFO(CamxLogGroupHAL, "            format       : %d, %s",
                    pStreamConfigsAPI->streams[stream]->format,
                    FormatToString(pStreamConfigsAPI->streams[stream]->format));
                CAMX_LOG_INFO(CamxLogGroupHAL, "            width        : %d",
                    pStreamConfigsAPI->streams[stream]->width);
                CAMX_LOG_INFO(CamxLogGroupHAL, "            height       : %d",
                    pStreamConfigsAPI->streams[stream]->height);
                CAMX_LOG_INFO(CamxLogGroupHAL, "            stream_type  : %08x, %s",
                    pStreamConfigsAPI->streams[stream]->stream_type,
                    StreamTypeToString(pStreamConfigsAPI->streams[stream]->stream_type));
                CAMX_LOG_INFO(CamxLogGroupHAL, "            usage        : %08x",
                    pStreamConfigsAPI->streams[stream]->usage);
                CAMX_LOG_INFO(CamxLogGroupHAL, "            max_buffers  : %d",
                    pStreamConfigsAPI->streams[stream]->max_buffers);
                CAMX_LOG_INFO(CamxLogGroupHAL, "            rotation     : %08x, %s",
                    pStreamConfigsAPI->streams[stream]->rotation,
                    RotationToString(pStreamConfigsAPI->streams[stream]->rotation));
                CAMX_LOG_INFO(CamxLogGroupHAL, "            data_space   : %08x, %s",
                    pStreamConfigsAPI->streams[stream]->data_space,
                    DataSpaceToString(pStreamConfigsAPI->streams[stream]->data_space));
                CAMX_LOG_INFO(CamxLogGroupHAL, "            priv         : %p",
                    pStreamConfigsAPI->streams[stream]->priv);
#if (defined(CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
                CAMX_LOG_INFO(CamxLogGroupHAL, "            physical_camera_id         : %s",
                    pStreamConfigsAPI->streams[stream]->physical_camera_id);
#endif // Android-P or better
                pStreamConfigsAPI->streams[stream]->reserved[0] = NULL;
                pStreamConfigsAPI->streams[stream]->reserved[1] = NULL;
            }
        }
        CAMX_LOG_INFO(CamxLogGroupHAL, "  operation_mode: %d", pStreamConfigsAPI->operation_mode);


        Camera3StreamConfig* pStreamConfigs = reinterpret_cast<Camera3StreamConfig*>(pStreamConfigsAPI);

        result = pHALDevice->ConfigureStreams(pStreamConfigs);

        if ((CamxResultSuccess != result) && (CamxResultEInvalidArg != result))
        {
            // HAL interface requires -ENODEV (EFailed) if a fatal error occurs
            result = CamxResultEFailed;
        }

        if (CamxResultSuccess == result)
        {
            for (UINT32 stream = 0; stream < pStreamConfigsAPI->num_streams; stream++)
            {
                CAMX_ASSERT(NULL != pStreamConfigsAPI->streams[stream]);

                if (NULL == pStreamConfigsAPI->streams[stream])
                {
                    CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument 2 for configure_streams()");
                    // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
                    result = CamxResultEInvalidArg;
                    break;
                }
                else
                {
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, " FINAL stream[%d] = %p - info:", stream,
                        pStreamConfigsAPI->streams[stream]);
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            format       : %d, %s",
                        pStreamConfigsAPI->streams[stream]->format,
                        FormatToString(pStreamConfigsAPI->streams[stream]->format));
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            width        : %d",
                        pStreamConfigsAPI->streams[stream]->width);
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            height       : %d",
                        pStreamConfigsAPI->streams[stream]->height);
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            stream_type  : %08x, %s",
                        pStreamConfigsAPI->streams[stream]->stream_type,
                        StreamTypeToString(pStreamConfigsAPI->streams[stream]->stream_type));
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            usage        : %08x",
                        pStreamConfigsAPI->streams[stream]->usage);
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            max_buffers  : %d",
                        pStreamConfigsAPI->streams[stream]->max_buffers);
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            rotation     : %08x, %s",
                        pStreamConfigsAPI->streams[stream]->rotation,
                        RotationToString(pStreamConfigsAPI->streams[stream]->rotation));
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            data_space   : %08x, %s",
                        pStreamConfigsAPI->streams[stream]->data_space,
                        DataSpaceToString(pStreamConfigsAPI->streams[stream]->data_space));
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            priv         : %p",
                        pStreamConfigsAPI->streams[stream]->priv);
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            reserved[0]         : %p",
                        pStreamConfigsAPI->streams[stream]->reserved[0]);
                    CAMX_LOG_CONFIG(CamxLogGroupHAL, "            reserved[1]         : %p",
                        pStreamConfigsAPI->streams[stream]->reserved[1]);

                    Camera3HalStream* pHalStream =
                        reinterpret_cast<Camera3HalStream*>(pStreamConfigsAPI->streams[stream]->reserved[0]);
                    if (pHalStream != NULL)
                    {
                        CAMX_LOG_CONFIG(CamxLogGroupHAL,
                            "            pHalStream: %p format         : 0x%x, consumer usage: %llx, producer usage: %llx",
                            pHalStream, pHalStream->overrideFormat, pHalStream->consumerUsage, pHalStream->producerUsage);
                    }
                }
            }
        }
        CAMX_LOG_CONFIG(CamxLogGroupHAL, "HalOp: End CONFIG, logicalCameraId: %d, cameraId: %d",
                        pHALDevice->GetCameraId(), pHALDevice->GetFwCameraId());
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument(s) for configure_streams()");
        // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
        result = CamxResultEInvalidArg;
    }

    return Utils::CamxResultToErrno(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// construct_default_request_settings
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const camera_metadata_t* construct_default_request_settings(
    const struct camera3_device*    pCamera3DeviceAPI,
    int                             requestTemplateAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3ConstructDefaultRequestSettings);

    const Metadata* pMetadata = NULL;

    CAMX_ASSERT(NULL != pCamera3DeviceAPI);
    CAMX_ASSERT(NULL != pCamera3DeviceAPI->priv);

    if ((NULL != pCamera3DeviceAPI) && (NULL != pCamera3DeviceAPI->priv))
    {
        HALDevice*             pHALDevice        = GetHALDevice(pCamera3DeviceAPI);
        Camera3RequestTemplate requestTemplate   = static_cast<Camera3RequestTemplate>(requestTemplateAPI);

        pMetadata = pHALDevice->ConstructDefaultRequestSettings(requestTemplate);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument(s) for initialize()");
    }

    return reinterpret_cast<const camera_metadata_t*>(pMetadata);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// process_capture_request
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int process_capture_request(
    const struct camera3_device*    pCamera3DeviceAPI,
    camera3_capture_request_t*      pCaptureRequestAPI)
{
    UINT64 frameworkFrameNum = 0;

    if (NULL != pCaptureRequestAPI)
    {
        frameworkFrameNum = pCaptureRequestAPI->frame_number;
    }

    CAMX_ENTRYEXIT_SCOPE_ID(CamxLogGroupHAL, SCOPEEventHAL3ProcessCaptureRequest, frameworkFrameNum);

    CAMX_TRACE_ASYNC_BEGIN_F(CamxLogGroupHAL, frameworkFrameNum, "HAL3: RequestTrace");

    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pCamera3DeviceAPI);
    CAMX_ASSERT(NULL != pCamera3DeviceAPI->priv);
    CAMX_ASSERT(NULL != pCaptureRequestAPI);
    CAMX_ASSERT(pCaptureRequestAPI->num_output_buffers > 0);
    CAMX_ASSERT(NULL != pCaptureRequestAPI->output_buffers);

    if ((NULL != pCamera3DeviceAPI)                  &&
        (NULL != pCamera3DeviceAPI->priv)            &&
        (NULL != pCaptureRequestAPI)                 &&
        (pCaptureRequestAPI->num_output_buffers > 0) &&
        (NULL != pCaptureRequestAPI->output_buffers))
    {
        /// @todo (CAMX-337): Go deeper into camera3_capture_request_t struct for validation

        HALDevice*             pHALDevice = GetHALDevice(pCamera3DeviceAPI);
        Camera3CaptureRequest* pRequest   = reinterpret_cast<Camera3CaptureRequest*>(pCaptureRequestAPI);

        CAMX_LOG_INFO(CamxLogGroupHAL, "frame_number %d", pCaptureRequestAPI->frame_number);
        CAMX_LOG_INFO(CamxLogGroupHAL, "num_output_buffers %d", pCaptureRequestAPI->num_output_buffers);
        CAMX_LOG_INFO(CamxLogGroupHAL, "output_buffers %p", pCaptureRequestAPI->output_buffers);
        if (NULL != pCaptureRequestAPI->output_buffers)
        {
            for (UINT i = 0; i < pCaptureRequestAPI->num_output_buffers; i++)
            {
                CAMX_LOG_INFO(CamxLogGroupHAL, "    output_buffers[%d] : %p - info", i, &pCaptureRequestAPI->output_buffers[i]);
                CAMX_LOG_INFO(CamxLogGroupHAL, "        buffer  : %p", pCaptureRequestAPI->output_buffers[i].buffer);
                CAMX_LOG_INFO(CamxLogGroupHAL, "        status  : %08x", pCaptureRequestAPI->output_buffers[i].status);
                CAMX_LOG_INFO(CamxLogGroupHAL, "        stream  : %p", pCaptureRequestAPI->output_buffers[i].stream);
                if (HAL_PIXEL_FORMAT_BLOB == pCaptureRequestAPI->output_buffers[i].stream->format)
                {
                    CAMX_TRACE_ASYNC_BEGIN_F(CamxLogGroupNone, pCaptureRequestAPI->frame_number, "SNAPSHOT frameID: %d",
                        pCaptureRequestAPI->frame_number);
                    CAMX_TRACE_ASYNC_BEGIN_F(CamxLogGroupNone, pCaptureRequestAPI->frame_number, "SHUTTERLAG frameID: %d",
                        pCaptureRequestAPI->frame_number);
                }
            }
        }
        CAMX_LOG_INFO(CamxLogGroupHAL, "input_buffer %p", pCaptureRequestAPI->input_buffer);
        if (NULL != pCaptureRequestAPI->input_buffer)
        {
            CAMX_LOG_INFO(CamxLogGroupHAL, "        buffer  : %p", pCaptureRequestAPI->input_buffer->buffer);
            CAMX_LOG_INFO(CamxLogGroupHAL, "        status  : %08x", pCaptureRequestAPI->input_buffer->status);
            CAMX_LOG_INFO(CamxLogGroupHAL, "        stream  : %p", pCaptureRequestAPI->input_buffer->stream);
        }
        CAMX_LOG_INFO(CamxLogGroupHAL, "settings %p", pCaptureRequestAPI->settings);

        result = pHALDevice->ProcessCaptureRequest(pRequest);

        if ((CamxResultSuccess != result) && (CamxResultEInvalidArg != result))
        {
            // HAL interface requires -ENODEV (EFailed) if a fatal error occurs
            result = CamxResultEFailed;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument(s) for process_capture_request()");
        // HAL interface requires -EINVAL (EInvalidArg) for invalid arguments
        result = CamxResultEInvalidArg;
    }

    return Utils::CamxResultToErrno(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// dump
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void dump(
    const struct camera3_device*    pCamera3DeviceAPI,
    int                             fdAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3Dump);

    CAMX_LOG_TO_FILE(fdAPI, 0, "###############  Chi Snapshot  ###############");

    if ((NULL != pCamera3DeviceAPI) &&
        (NULL != pCamera3DeviceAPI->priv))
    {
        HAL3Module::GetInstance()->Dump(fdAPI);

        HALDevice* pHALDevice = GetHALDevice(pCamera3DeviceAPI);
        pHALDevice->Dump(fdAPI);

        ChiDumpState(fdAPI);
    }
    else
    {
        CAMX_LOG_TO_FILE(fdAPI, 2, "Invalid camera3_device pointer, cannot dump info");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// flush
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int flush(
    const struct camera3_device* pCamera3DeviceAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3Flush);

    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pCamera3DeviceAPI);
    CAMX_ASSERT(NULL != pCamera3DeviceAPI->priv);

    if ((NULL != pCamera3DeviceAPI) &&
        (NULL != pCamera3DeviceAPI->priv))
    {
        HALDevice* pHALDevice = GetHALDevice(pCamera3DeviceAPI);

        CAMX_LOG_CONFIG(CamxLogGroupHAL, "HalOp: Begin FLUSH: %p, logicalCameraId: %d, cameraId: %d",
                        pCamera3DeviceAPI, pHALDevice->GetCameraId(), pHALDevice->GetFwCameraId());

        result = pHALDevice->Flush();

        if ((CamxResultSuccess != result) && (CamxResultEInvalidArg != result))
        {
            // HAL interface requires -ENODEV (EFailed) if flush fails
            result = CamxResultEFailed;
        }
        CAMX_LOG_CONFIG(CamxLogGroupHAL, "HalOp: End FLUSH: %p with result %d, logicalCameraId: %d, cameraId: %d",
                        pCamera3DeviceAPI, result, pHALDevice->GetCameraId(), pHALDevice->GetFwCameraId());
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupHAL, "Invalid argument 1 for flush()");
        // HAL interface requires -EINVAL (EInvalidArg) if arguments are invalid
        result = CamxResultEInvalidArg;
    }

    return Utils::CamxResultToErrno(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// camera_module_callbacks_t exit points
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// camera_device_status_change
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void camera_device_status_change(
    const struct camera_module_callbacks*   pModuleCbsAPI,
    int                                     cameraIdAPI,
    int                                     newStatusAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3CameraDeviceStatusChange);

    pModuleCbsAPI->camera_device_status_change(pModuleCbsAPI, cameraIdAPI, newStatusAPI);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// torch_mode_status_change
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void torch_mode_status_change(
    const struct camera_module_callbacks*   pModuleCbsAPI,
    const char*                             pCameraIdAPI,
    int                                     newStatusAPI)
{
    CAMX_ENTRYEXIT_SCOPE(CamxLogGroupHAL, SCOPEEventHAL3TorchModeStatusChange);

    if (NULL != pModuleCbsAPI)
    {
        pModuleCbsAPI->torch_mode_status_change(pModuleCbsAPI, pCameraIdAPI, newStatusAPI);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// camera3_callback_ops_t exit points
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// process_capture_result
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void process_capture_result(
    const struct camera3_callback_ops*  pCamera3CbOpsAPI,
    const camera3_capture_result_t*     pCaptureResultAPI)
{
    UINT64 frameworkFrameNum = 0;

    if (NULL != pCaptureResultAPI)
    {
        frameworkFrameNum = pCaptureResultAPI->frame_number;

        CAMX_LOG_INFO(CamxLogGroupHAL, "frame_number %d", pCaptureResultAPI->frame_number);
        CAMX_LOG_INFO(CamxLogGroupHAL, "num_output_buffers %d", pCaptureResultAPI->num_output_buffers);
        CAMX_LOG_INFO(CamxLogGroupHAL, "output_buffers %p", pCaptureResultAPI->output_buffers);
        if (NULL != pCaptureResultAPI->output_buffers)
        {
            for (UINT i = 0; i < pCaptureResultAPI->num_output_buffers; i++)
            {
                CAMX_LOG_INFO(CamxLogGroupHAL, "    output_buffers[%d] : %p - info", i, &pCaptureResultAPI->output_buffers[i]);
                CAMX_LOG_INFO(CamxLogGroupHAL, "        buffer  : %p", pCaptureResultAPI->output_buffers[i].buffer);
                CAMX_LOG_INFO(CamxLogGroupHAL, "        status  : %08x", pCaptureResultAPI->output_buffers[i].status);
                CAMX_LOG_INFO(CamxLogGroupHAL, "        stream  : %p", pCaptureResultAPI->output_buffers[i].stream);

                if (HAL_PIXEL_FORMAT_BLOB == pCaptureResultAPI->output_buffers[i].stream->format)
                {
                    CAMX_TRACE_ASYNC_END_F(CamxLogGroupNone, pCaptureResultAPI->frame_number, "SNAPSHOT frameID: %d",
                        pCaptureResultAPI->frame_number);
                }
            }
        }
        CAMX_LOG_INFO(CamxLogGroupHAL, "input_buffer %p", pCaptureResultAPI->input_buffer);
        if (NULL != pCaptureResultAPI->input_buffer)
        {
            CAMX_LOG_INFO(CamxLogGroupHAL, "        buffer  : %p", pCaptureResultAPI->input_buffer->buffer);
            CAMX_LOG_INFO(CamxLogGroupHAL, "        status  : %08x", pCaptureResultAPI->input_buffer->status);
            CAMX_LOG_INFO(CamxLogGroupHAL, "        stream  : %p", pCaptureResultAPI->input_buffer->stream);
        }
        CAMX_LOG_INFO(CamxLogGroupHAL, "partial_result %d", pCaptureResultAPI->partial_result);
        CAMX_LOG_INFO(CamxLogGroupHAL, "result %p", pCaptureResultAPI->result);
#if (defined(CAMX_ANDROID_API) && (CAMX_ANDROID_API >= 28)) // Android-P or better
        CAMX_LOG_INFO(CamxLogGroupHAL, "num_physcam_metadata %d", pCaptureResultAPI->num_physcam_metadata);
#endif // Android-P or better
    }

    CAMX_ENTRYEXIT_SCOPE_ID(CamxLogGroupHAL, SCOPEEventHAL3ProcessCaptureResult, frameworkFrameNum);

    pCamera3CbOpsAPI->process_capture_result(pCamera3CbOpsAPI, pCaptureResultAPI);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// notify
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void notify(
    const struct camera3_callback_ops*  pCamera3CbOpsAPI,
    const camera3_notify_msg_t*         pNotifyMessageAPI)
{
    if (NULL != pNotifyMessageAPI)
    {
        CAMX_TRACE_SYNC_BEGIN_F(CamxLogGroupHAL, "HAL3 Notify Type: %u", pNotifyMessageAPI->type);
    }

    BOOL bNeedNotify = TRUE;
    if (NULL != pNotifyMessageAPI)
    {
        if (FALSE == (CAMERA3_MSG_ERROR == pNotifyMessageAPI->type || CAMERA3_MSG_SHUTTER == pNotifyMessageAPI->type))
        {
            bNeedNotify = FALSE;
        }
    }

    if (TRUE == bNeedNotify)
    {
        pCamera3CbOpsAPI->notify(pCamera3CbOpsAPI, pNotifyMessageAPI);
    }

    CAMX_TRACE_SYNC_END(CamxLogGroupHAL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Jump table for HAL3
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
JumpTableHAL3 g_jumpTableHAL3 =
{
    open,
    get_number_of_cameras,
    get_camera_info,
    set_callbacks,
    get_vendor_tag_ops,
    open_legacy,
    set_torch_mode,
    init,
    get_tag_count,
    get_all_tags,
    get_section_name,
    get_tag_name,
    get_tag_type,
    close,
    initialize,
    configure_streams,
    construct_default_request_settings,
    process_capture_request,
    dump,
    flush,
    camera_device_status_change,
    torch_mode_status_change,
    process_capture_result,
    notify
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FillTokenList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FillTokenList(
    ChiOverrideToken* pTokens)
{
#define REGISTER_TOKEN(inToken, data)                                        \
    pTokens[static_cast<UINT>(inToken)].id    = static_cast<UINT>(inToken);  \
    pTokens[static_cast<UINT>(inToken)].size  = sizeof(data);

#define REGISTER_BIT_TOKEN(inToken, data)                                        \
    pTokens[static_cast<UINT>(inToken)].id    = static_cast<UINT>(inToken);  \
    pTokens[static_cast<UINT>(inToken)].size  = sizeof(VOID*);

    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    REGISTER_TOKEN(ChxSettingsToken::OverrideForceUsecaseId,            pStaticSettings->overrideForceUsecaseId);
    REGISTER_TOKEN(ChxSettingsToken::OverrideDisableZSL,                pStaticSettings->overrideDisableZSL);
    REGISTER_TOKEN(ChxSettingsToken::OverrideGPURotationUsecase,        pStaticSettings->overrideGPURotationUsecase);
    REGISTER_TOKEN(ChxSettingsToken::OverrideEnableMFNR,                pStaticSettings->overrideEnableMFNR);
    REGISTER_TOKEN(ChxSettingsToken::AnchorSelectionAlgoForMFNR,        pStaticSettings->anchorSelectionAlgoForMFNR);
    REGISTER_TOKEN(ChxSettingsToken::OverrideHFRNo3AUseCase,            pStaticSettings->overrideHFRNo3AUseCase);
    REGISTER_TOKEN(ChxSettingsToken::OverrideForceSensorMode,           pStaticSettings->overrideForceSensorMode);
    REGISTER_TOKEN(ChxSettingsToken::DefaultMaxFPS,                     pStaticSettings->defaultMaxFPS);
    REGISTER_TOKEN(ChxSettingsToken::FovcEnable,                        pStaticSettings->fovcEnable);
    REGISTER_TOKEN(ChxSettingsToken::OverrideCameraClose,               pStaticSettings->overrideCameraClose);
    REGISTER_TOKEN(ChxSettingsToken::OverrideCameraOpen,                pStaticSettings->overrideCameraOpen);
    REGISTER_TOKEN(ChxSettingsToken::EISV2Enable,                       pStaticSettings->EISV2Enable);
    REGISTER_TOKEN(ChxSettingsToken::EISV3Enable,                       pStaticSettings->EISV3Enable);
    REGISTER_TOKEN(ChxSettingsToken::NumPCRsBeforeStreamOn,             pStaticSettings->numPCRsBeforeStreamOn);
    REGISTER_TOKEN(ChxSettingsToken::StatsProcessingSkipFactor,         pStaticSettings->statsProcessingSkipFactor);
    REGISTER_TOKEN(ChxSettingsToken::DumpDebugDataEveryProcessResult,   pStaticSettings->dumpDebugDataEveryProcessResult);
    REGISTER_TOKEN(ChxSettingsToken::MultiCameraVREnable,               pStaticSettings->multiCameraVREnable);
    REGISTER_TOKEN(ChxSettingsToken::OverrideGPUDownscaleUsecase,       pStaticSettings->overrideGPUDownscaleUsecase);
    REGISTER_TOKEN(ChxSettingsToken::AdvanceFeatureMask,                pStaticSettings->advanceFeatureMask);
    REGISTER_TOKEN(ChxSettingsToken::DisableASDStatsProcessing,         pStaticSettings->disableASDStatsProcessing);
    REGISTER_TOKEN(ChxSettingsToken::MultiCameraFrameSync,              pStaticSettings->multiCameraFrameSync);
    REGISTER_TOKEN(ChxSettingsToken::OutputFormat,                      pStaticSettings->outputFormat);
    REGISTER_TOKEN(ChxSettingsToken::EnableCHIPartialData,              pStaticSettings->enableCHIPartialData);
    REGISTER_TOKEN(ChxSettingsToken::EnableFDStreamInRealTime,          pStaticSettings->enableFDStreamInRealTime);
    REGISTER_TOKEN(ChxSettingsToken::SelectIHDRUsecase,                 pStaticSettings->selectIHDRUsecase);
    REGISTER_TOKEN(ChxSettingsToken::EnableUnifiedBufferManager,        pStaticSettings->enableUnifiedBufferManager);
    REGISTER_TOKEN(ChxSettingsToken::EnableCHIImageBufferLateBinding,   pStaticSettings->enableCHIImageBufferLateBinding);
    REGISTER_TOKEN(ChxSettingsToken::EnableCHIPartialDataRecovery,      pStaticSettings->enableCHIPartialDataRecovery);
    REGISTER_TOKEN(ChxSettingsToken::UseFeatureForQCFA,                 pStaticSettings->useFeatureForQCFA);
    REGISTER_TOKEN(ChxSettingsToken::EnableOfflineNoiseReprocess,       pStaticSettings->enableOfflineNoiseReprocess);
    REGISTER_TOKEN(ChxSettingsToken::EnableIHDRSnapshot,                pStaticSettings->enableIHDRSnapshot);
    REGISTER_TOKEN(ChxSettingsToken::OverrideLogLevels,                 pStaticSettings->overrideLogLevels);
    REGISTER_BIT_TOKEN(ChxSettingsToken::EnableRequestMapping,              pStaticSettings->logRequestMapping);

#undef REGISTER_SETTING
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GenerateExtendOpenData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GenerateExtendOpenData(
    UINT32                  numTokens,
    ChiOverrideExtendOpen*  pExtend)
{
    pExtend->numTokens         = numTokens;
    FillTokenList(pExtend->pTokens);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GenerateExtendCloseData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GenerateExtendCloseData(
    UINT32                  numTokens,
    ChiOverrideExtendClose*  pExtend)
{
    pExtend->numTokens = numTokens;
    FillTokenList(pExtend->pTokens);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GenerateModifySettingsData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GenerateModifySettingsData(
    ChiOverrideModifySetting* pSettings)
{
    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    // decltype()  returns the declaration type, the right hand side of the assignment in the code below is to let us use a
    // const cast in the macro, otherwise whiner whines and NOWHINEs are being ignored in the macro.
#define ADD_SETTING(inToken, data)                                                    \
    pSettings[static_cast<UINT>(inToken)].pData       = (decltype(data)*)(&(data));   \
    pSettings[static_cast<UINT>(inToken)].token.id    = static_cast<UINT32>(inToken); \
    pSettings[static_cast<UINT>(inToken)].token.size  = sizeof(data);

#define ADD_BIT_SETTING(inToken, data)                                                \
    pSettings[static_cast<UINT>(inToken)].pData       = (VOID*)(data == TRUE);        \
    pSettings[static_cast<UINT>(inToken)].token.id    = static_cast<UINT32>(inToken); \
    pSettings[static_cast<UINT>(inToken)].token.size  = sizeof(VOID*);


    ADD_SETTING(ChxSettingsToken::OverrideForceUsecaseId,          pStaticSettings->overrideForceUsecaseId);
    ADD_SETTING(ChxSettingsToken::OverrideDisableZSL,              pStaticSettings->overrideDisableZSL);
    ADD_SETTING(ChxSettingsToken::OverrideGPURotationUsecase,      pStaticSettings->overrideGPURotationUsecase);
    ADD_SETTING(ChxSettingsToken::OverrideEnableMFNR,              pStaticSettings->overrideEnableMFNR);
    ADD_SETTING(ChxSettingsToken::AnchorSelectionAlgoForMFNR,      pStaticSettings->anchorSelectionAlgoForMFNR);
    ADD_SETTING(ChxSettingsToken::OverrideHFRNo3AUseCase,          pStaticSettings->overrideHFRNo3AUseCase);
    ADD_SETTING(ChxSettingsToken::OverrideForceSensorMode,         pStaticSettings->overrideForceSensorMode);
    ADD_SETTING(ChxSettingsToken::DefaultMaxFPS,                   pStaticSettings->defaultMaxFPS);
    ADD_SETTING(ChxSettingsToken::FovcEnable,                      pStaticSettings->fovcEnable);
    ADD_SETTING(ChxSettingsToken::OverrideCameraClose,             pStaticSettings->overrideCameraClose);
    ADD_SETTING(ChxSettingsToken::OverrideCameraOpen,              pStaticSettings->overrideCameraOpen);
    ADD_SETTING(ChxSettingsToken::EISV2Enable,                     pStaticSettings->EISV2Enable);
    ADD_SETTING(ChxSettingsToken::EISV3Enable,                     pStaticSettings->EISV3Enable);
    ADD_SETTING(ChxSettingsToken::NumPCRsBeforeStreamOn,           pStaticSettings->numPCRsBeforeStreamOn);
    ADD_SETTING(ChxSettingsToken::StatsProcessingSkipFactor,       pStaticSettings->statsProcessingSkipFactor);
    ADD_SETTING(ChxSettingsToken::DumpDebugDataEveryProcessResult, pStaticSettings->dumpDebugDataEveryProcessResult);
    ADD_SETTING(ChxSettingsToken::MultiCameraVREnable,             pStaticSettings->multiCameraVREnable);
    ADD_SETTING(ChxSettingsToken::OverrideGPUDownscaleUsecase,     pStaticSettings->overrideGPUDownscaleUsecase);
    ADD_SETTING(ChxSettingsToken::AdvanceFeatureMask,              pStaticSettings->advanceFeatureMask);
    ADD_SETTING(ChxSettingsToken::DisableASDStatsProcessing,       pStaticSettings->disableASDStatsProcessing);
    ADD_SETTING(ChxSettingsToken::MultiCameraFrameSync,            pStaticSettings->multiCameraFrameSync);
    ADD_SETTING(ChxSettingsToken::OutputFormat,                    pStaticSettings->outputFormat);
    ADD_SETTING(ChxSettingsToken::EnableCHIPartialData,            pStaticSettings->enableCHIPartialData);
    ADD_SETTING(ChxSettingsToken::EnableFDStreamInRealTime,        pStaticSettings->enableFDStreamInRealTime);
    ADD_SETTING(ChxSettingsToken::SelectIHDRUsecase,               pStaticSettings->selectIHDRUsecase);
    ADD_SETTING(ChxSettingsToken::EnableUnifiedBufferManager,      pStaticSettings->enableUnifiedBufferManager);
    ADD_SETTING(ChxSettingsToken::EnableCHIImageBufferLateBinding, pStaticSettings->enableCHIImageBufferLateBinding);
    ADD_SETTING(ChxSettingsToken::EnableCHIPartialDataRecovery,    pStaticSettings->enableCHIPartialDataRecovery);
    ADD_SETTING(ChxSettingsToken::UseFeatureForQCFA,               pStaticSettings->useFeatureForQCFA);
    ADD_SETTING(ChxSettingsToken::EnableOfflineNoiseReprocess,     pStaticSettings->enableOfflineNoiseReprocess);
    ADD_SETTING(ChxSettingsToken::EnableIHDRSnapshot,              pStaticSettings->enableIHDRSnapshot);
    ADD_SETTING(ChxSettingsToken::OverrideLogLevels,               pStaticSettings->overrideLogLevels);

    ADD_BIT_SETTING(ChxSettingsToken::EnableRequestMapping,        pStaticSettings->logRequestMapping);

#undef ADD_SETTING
#undef ADD_BIT_SETTING

}

CAMX_NAMESPACE_END
