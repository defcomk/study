////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxfdutils.cpp
/// @brief FD utility class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdebugprint.h"
#include "camxdefs.h"
#include "camxfdhwnode.h"
#include "camxfdproperty.h"
#include "camxfdutils.h"
#include "camxstabilization.h"
#include "camxtypes.h"
#include "camxutils.h"
#include "camxvendortags.h"

static const FDConfig FDConfigHwDefault =
{
#include "camxfdconfighwdefault.h"
};

static const FDConfig FDConfigHwVideo =
{
#include "camxfdconfighwvideo.h"
};

static const FDConfig FDConfigSwDefault =
{
#include "camxfdconfigswdefault.h"
};

static const FDConfig FDConfigSwVideo =
{
#include "camxfdconfigswvideo.h"
};
static const FDConfig FDConfigHwDefaultLite =
{
#include "camxfdconfighwdefaultlite.h"
};

static const FDConfig FDConfigHwVideoLite =
{
#include "camxfdconfighwvideolite.h"
};

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FaceSizeSort
///
/// @brief  Comparison function that will be used for sorting faces largest to smallest
///
/// @param  pArg0 Pointer to object to compare for sorting
/// @param  pArg1 Pointer to object to compare for sorting
///
/// @return 1 if pArg0 is greater than pArg1, -1 otherwise
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
INT FaceSizeSort(
    const VOID* pArg0,
    const VOID* pArg1)
{
    CAMX_ASSERT(NULL != pArg0);
    CAMX_ASSERT(NULL != pArg1);

    const FDFaceInfo* pFirst        = static_cast<const FDFaceInfo*>(pArg0);
    const FDFaceInfo* pSecond       = static_cast<const FDFaceInfo*>(pArg1);
    INT               comparison    = 0;

    if ((pFirst->faceROI.width * pFirst->faceROI.height) > (pSecond->faceROI.width * pSecond->faceROI.height))
    {
        comparison = -1;
    }
    else
    {
        comparison = 1;
    }

    return comparison;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDUtils::ConvertToStabilizationData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::ConvertToStabilizationData(
    FDResults*         pResults,
    StabilizationData* pStabilizationData)
{
    CamxResult result = CamxResultSuccess;
    UINT32     index  = 0;

    if (StabilizationMaxObjects < pResults->numFacesDetected)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Unsupported number of faces %d (%d)",
                       pResults->numFacesDetected, StabilizationMaxObjects);

        result = CamxResultEUnsupported;
    }

    if (CamxResultSuccess == result)
    {
        for (UINT i = 0; i < pResults->numFacesDetected; i++)
        {
            INT32               eyesDistance     = 0;
            StabilizationEntry  leftEyeCenter    = { 0 };
            StabilizationEntry  rightEyeCenter   = { 0 };
            StabilizationEntry  averageEyeCenter = { 0 };
            INT32               deltaEyesX       = 0;
            INT32               deltaEyesY       = 0;
            BOOL                validReference   = FALSE;

            pStabilizationData->objectData[i].id = Utils::AbsoluteINT32(pResults->faceInfo[i].faceID);

            // Calculate the reference positions for the facial parts
            if (TRUE == pResults->faceInfo[i].facialParts.valid)
            {
                leftEyeCenter.data0    = pResults->faceInfo[i].facialParts.facialPoint[FDFacialPointLeftEye].position.x;
                leftEyeCenter.data1    = pResults->faceInfo[i].facialParts.facialPoint[FDFacialPointLeftEye].position.y;
                rightEyeCenter.data0   = pResults->faceInfo[i].facialParts.facialPoint[FDFacialPointRightEye].position.x;
                rightEyeCenter.data1   = pResults->faceInfo[i].facialParts.facialPoint[FDFacialPointRightEye].position.y;

                averageEyeCenter.data0 = ((leftEyeCenter.data0 + rightEyeCenter.data0) / 2);
                averageEyeCenter.data1 = ((leftEyeCenter.data1 + rightEyeCenter.data1) / 2);

                deltaEyesX             = (leftEyeCenter.data0 - rightEyeCenter.data0);
                deltaEyesY             = (leftEyeCenter.data1 - rightEyeCenter.data1);
                eyesDistance           = static_cast<INT32>(Utils::Sqrt((deltaEyesX * deltaEyesX) + (deltaEyesY * deltaEyesY)));

                validReference         = TRUE;
            }

            // FD will always be trying to produce face center position data, this index is reserved for center
            index = ObjectPositionIndex;

            pStabilizationData->objectData[i].pFaceInfo = reinterpret_cast<VOID *>(&pResults->faceInfo[i]);

            // This is always valid, otherwise we would not get this as a detected face
            pStabilizationData->objectData[i].attributeData[index].entry.data0      = pResults->faceInfo[i].faceROI.center.x;
            pStabilizationData->objectData[i].attributeData[index].entry.data1      = pResults->faceInfo[i].faceROI.center.y;
            pStabilizationData->objectData[i].attributeData[index].valid            = TRUE;
            pStabilizationData->objectData[i].attributeReference[index].entry       = averageEyeCenter;
            pStabilizationData->objectData[i].attributeReference[index].valid       = validReference;
            pStabilizationData->objectData[i].numAttributes++;

            // FD will always be trying to produce faceROI data, this index is reserved for FaceROI size
            index = ObjectSizeIndex;

            // This is always valid, otherwise we would not get this as a detected face
            pStabilizationData->objectData[i].attributeData[index].entry.data0      = pResults->faceInfo[i].faceROI.width;
            pStabilizationData->objectData[i].attributeData[index].entry.data1      = pResults->faceInfo[i].faceROI.height;
            pStabilizationData->objectData[i].attributeData[index].valid            = TRUE;
            pStabilizationData->objectData[i].attributeReference[index].entry.data0 = eyesDistance;
            pStabilizationData->objectData[i].attributeReference[index].entry.data1 = eyesDistance;
            pStabilizationData->objectData[i].attributeReference[index].valid       = validReference;
            pStabilizationData->objectData[i].numAttributes++;

            // Below indexes can be used for other stabilization attributes for the face
            // Use the below index value to store attribute to the correct index, then increment index
            index = ObjectUnreservedIndex;
        }

        pStabilizationData->numObjects = pResults->numFacesDetected;

        CAMX_LOG_VERBOSE(CamxLogGroupFD, "numObjects = %d", pStabilizationData->numObjects);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDUtils::ConvertFromStabilizationData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::ConvertFromStabilizationData(
    StabilizationData* pStabilizationData,
    FDResults*         pResults)
{
    CamxResult result = CamxResultSuccess;
    UINT32     index  = 0;

    for (UINT i = 0; i < pStabilizationData->numObjects; i++)
    {
        Utils::Memcpy(&pResults->faceInfo[i], pStabilizationData->objectData[i].pFaceInfo, sizeof(FDFaceInfo));

        pResults->faceInfo[i].faceID = pStabilizationData->objectData[i].id;

        index = ObjectPositionIndex;
        if (TRUE == pStabilizationData->objectData[i].attributeData[index].valid)
        {
            pResults->faceInfo[i].faceROI.center.x = pStabilizationData->objectData[i].attributeData[index].entry.data0;
            pResults->faceInfo[i].faceROI.center.y = pStabilizationData->objectData[i].attributeData[index].entry.data1;
        }

        index = ObjectSizeIndex;
        if (TRUE == pStabilizationData->objectData[i].attributeData[index].valid)
        {
            pResults->faceInfo[i].faceROI.width  = pStabilizationData->objectData[i].attributeData[index].entry.data0;
            pResults->faceInfo[i].faceROI.height = pStabilizationData->objectData[i].attributeData[index].entry.data1;
        }

        // Below indexes can be used for other stabilization attributes for the face
        // Use the below index value to pull attribute from correct index, then increment index
        index = ObjectUnreservedIndex;
    }

    pResults->numFacesDetected = pStabilizationData->numObjects;

    CAMX_LOG_VERBOSE(CamxLogGroupFD, "numObjects = %d", pStabilizationData->numObjects);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDUtils::SortFaces
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::SortFaces(
    FDResults* pFaceResults)
{
    CAMX_ASSERT(NULL != pFaceResults);

    CamxResult result = CamxResultSuccess;

    for (UINT32 i = 0; i < pFaceResults->numFacesDetected; i++)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Unsorted faces [%d] : ID[%d]  center=(%d, %d) width=%d, height=%d",
                         i,
                         pFaceResults->faceInfo[i].faceID,
                         pFaceResults->faceInfo[i].faceROI.center.x, pFaceResults->faceInfo[i].faceROI.center.y,
                         pFaceResults->faceInfo[i].faceROI.width,    pFaceResults->faceInfo[i].faceROI.height);
    }

    Utils::Qsort(&pFaceResults->faceInfo[0], pFaceResults->numFacesDetected, sizeof(pFaceResults->faceInfo[0]), FaceSizeSort);

    for (UINT32 i = 0; i < pFaceResults->numFacesDetected; i++)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Sorted faces [%d] : ID[%d]  center=(%d, %d) width=%d, height=%d",
                         i,
                         pFaceResults->faceInfo[i].faceID,
                         pFaceResults->faceInfo[i].faceROI.center.x, pFaceResults->faceInfo[i].faceROI.center.y,
                         pFaceResults->faceInfo[i].faceROI.width,    pFaceResults->faceInfo[i].faceROI.height);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDUtils::GetFDConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::GetFDConfig(
    Node*               pNode,
    FDConfigSourceType  source,
    BOOL                hwHybrid,
    BOOL                frontCamera,
    FDConfigSelector    selector,
    FDConfig*           pFaceConfig)
{
    CamxResult result = CamxResultSuccess;

    CAMX_ASSERT(NULL != pNode);
    CAMX_ASSERT(NULL != pFaceConfig);

    switch (source)
    {
        case FDConfigDefault:
            result = GetDefaultFDConfig(hwHybrid, frontCamera, selector, pFaceConfig);
            break;
        case FDConfigVendorTag:
            result = GetFDConfigFromVendorTag(pNode, hwHybrid, frontCamera, selector, pFaceConfig);
            break;
        case FDConfigBinary:
            result = GetFDConfigFromBinary(hwHybrid, frontCamera, selector, pFaceConfig);
            break;
        default:
            CAMX_LOG_ERROR(CamxLogGroupFD, "Invalid source for FD tuning configuration %d", source);
            break;
    }

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "Invalid source for FD tuning configuration %d", source);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDUtils::GetDefaultFDConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::GetDefaultFDConfig(
    BOOL                hwHybrid,
    BOOL                frontCamera,
    FDConfigSelector    selector,
    FDConfig*           pFaceConfig)
{
    CAMX_UNREFERENCED_PARAM(frontCamera);

    if (TRUE == hwHybrid)
    {
        switch (selector)
        {
            case FDSelectorVideo:
                if (TRUE == frontCamera)
                {
                    *pFaceConfig = FDConfigHwVideoLite;
                }
                else
                {
                    *pFaceConfig = FDConfigHwVideo;
                }
                break;
            case FDSelectorTurbo:
            case FDSelectorDefault:
            case FDSelectorPreview:
            default:
                if (TRUE == frontCamera)
                {
                    *pFaceConfig = FDConfigHwDefaultLite;
                }
                else
                {
                    *pFaceConfig = FDConfigHwDefault;
                }
                break;
        }
    }
    else
    {
        switch (selector)
        {
            case FDSelectorVideo:
                *pFaceConfig = FDConfigSwVideo;
                break;
            case FDSelectorTurbo:
            case FDSelectorDefault:
            case FDSelectorPreview:
            default:
                *pFaceConfig = FDConfigSwDefault;
                break;
        }
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDUtils::GetFDConfigFromVendorTag
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::GetFDConfigFromVendorTag(
    Node*               pNode,
    BOOL                hwHybrid,
    BOOL                frontCamera,
    FDConfigSelector    selector,
    FDConfig*           pFaceConfig)
{
    CAMX_UNREFERENCED_PARAM(hwHybrid);
    CAMX_UNREFERENCED_PARAM(selector);
    CAMX_UNREFERENCED_PARAM(frontCamera);

    UINT32      metaTag                = 0;
    CamxResult  result                 = CamxResultSuccess;
    UINT        configTag[1]           = { 0 };
    VOID*       pData[1]               = { 0 };
    UINT        length                 = CAMX_ARRAY_SIZE(configTag);
    UINT64      configDataOffset[1]    = { 0 };

    result = VendorTagManager::QueryVendorTagLocation(VendorTagSectionOEMFDConfig,
                                                      VendorTagNameOEMFDConfig,
                                                      &metaTag);

    if (CamxResultSuccess == result)
    {
        configTag[0] = metaTag;

        result = pNode->GetDataList(configTag, pData, configDataOffset, length);
    }

    if (CamxResultSuccess == result)
    {
        if (NULL != pData[0])
        {
            Utils::Memcpy(pFaceConfig, pData[0], sizeof(FDConfig));
        }
        else
        {
            result = CamxResultEInvalidPointer;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDUtils::GetFDConfigFromBinary
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::GetFDConfigFromBinary(
    BOOL                hwHybrid,
    BOOL                frontCamera,
    FDConfigSelector    selector,
    FDConfig*           pFaceConfig)
{
    CamxResult  result = CamxResultSuccess;

    CAMX_UNREFERENCED_PARAM(hwHybrid);
    CAMX_UNREFERENCED_PARAM(selector);
    CAMX_UNREFERENCED_PARAM(frontCamera);

    result = LoadFDTuningData(reinterpret_cast<camxfdconfig::FaceDetectionCtrlType*>(pFaceConfig), frontCamera, selector);

    if (CamxResultSuccess != result)
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "not able to load tuning data %d", result);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDUtils::GetFaceSize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 FDUtils::GetFaceSize(
    const FDFaceSize* pFaceSizeInfo,
    UINT32            minSize,
    UINT32            dimension)
{
    UINT32 faceSize;

    CAMX_ASSERT(NULL != pFaceSizeInfo);

    switch (pFaceSizeInfo->type)
    {
        case FDFaceAdjFloating:
            faceSize = static_cast<UINT32>(static_cast<FLOAT>(dimension * pFaceSizeInfo->ratio));
            faceSize = (faceSize < minSize) ? minSize : faceSize;
            break;
        case FDFaceAdjFixed:
        default:
            faceSize = pFaceSizeInfo->size;
            break;
    }

    return faceSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDUtils::LoadFDTuningData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::LoadFDTuningData(
    camxfdconfig::FaceDetectionCtrlType* pFdCtrlType,
    BOOL                frontCamera,
    FDConfigSelector    selector)
{
    CamxResult  result          = CamxResultSuccess;
    FILE*       phFileHandle    = NULL;
    CHAR        binaryFileName[FILENAME_MAX];
    camxfdconfig::FaceDetectionCtrlType* pFdCtrl;

    if (selector == FDSelectorVideo)
    {
        if (TRUE == frontCamera)
        {
            OsUtils::SNPrintF(binaryFileName, sizeof(binaryFileName), "fdconfigvideolite.bin");
        }
        else
        {
            OsUtils::SNPrintF(binaryFileName, sizeof(binaryFileName), "fdconfigvideo.bin");
        }
    }
    else
    {
        if (TRUE == frontCamera)
        {
            OsUtils::SNPrintF(binaryFileName, sizeof(binaryFileName), "fdconfigpreviewlite.bin");
        }
        else
        {
            OsUtils::SNPrintF(binaryFileName, sizeof(binaryFileName), "fdconfigpreview.bin");
        }
    }

    CHAR fullName[FILENAME_MAX];

    OsUtils::GetBinaryFileName(fullName, FILENAME_MAX, binaryFileName);

    phFileHandle = OsUtils::FOpen(fullName, "rb");

    if (NULL != phFileHandle)
    {
        UINT64 fileSizeBytes        = OsUtils::GetFileSize(fullName);
        UCHAR* pBuffer              = static_cast<BYTE*>(CAMX_CALLOC(static_cast<SIZE_T>(fileSizeBytes)));
        TuningMode pSelectors[1]    = { { ModeType::Default, { 0 } } };
        UINT32 modeCount            = 1;

        if (NULL != pBuffer)
        {
            UINT64 sizeRead = OsUtils::FRead(pBuffer,
                                static_cast<SIZE_T>(fileSizeBytes),
                                1,
                                static_cast<SIZE_T>(fileSizeBytes),
                                phFileHandle);

            CAMX_ASSERT(fileSizeBytes == sizeRead);

            fdSetManager*   pFDSetManager = CAMX_NEW fdSetManager;

            if (NULL != pFDSetManager)
            {
                if (TRUE == pFDSetManager->LoadBinaryParameters(pBuffer, fileSizeBytes))
                {
                    pFdCtrl =
                        pFDSetManager->GetModule_fdconfigdata(&pSelectors[0], modeCount);
                    Utils::Memcpy(pFdCtrlType, pFdCtrl, sizeof(FDConfig));
                }
                else
                {
                    CAMX_LOG_ERROR(CamxLogGroupFD, "Could not LoadBinaryParameters");
                }

                CAMX_DELETE pFDSetManager;
                pFDSetManager = NULL;
            }
            else
            {
                result = CamxResultEFailed;
            }

            // Delete the buffer
            CAMX_FREE(pBuffer);
            pBuffer = NULL;
        }

        OsUtils::FClose(phFileHandle);
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupFD, "open of %s failed", fullName);
        result = CamxResultEUnableToLoad;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDUtils::GetOrientationFromAccel()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::GetOrientationFromAccel(
    FLOAT* pAccelValues,
    INT32* pOrientationAngle)
{
    CamxResult  result = CamxResultSuccess;

    if ((NULL == pAccelValues) || (NULL == pOrientationAngle))
    {
        result = CamxResultEInvalidArg;
    }

    if (CamxResultSuccess == result)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Accel vector [0]=%f, [1]=%f, [2]=%f",
                         pAccelValues[0], pAccelValues[1], pAccelValues[2]);

        if (Utils::AbsoluteFLOAT(pAccelValues[2]) > 6.0f)
        {
            // If the z-access value is more than threshold value, we cannot determine/use the orientation angle.
            // set -1 to say invalid device orientation
            *pOrientationAngle = -1;
        }
        else
        {
            // Accel values range : [-9.806287, 9.806287]
            FLOAT accelMax = 9.8f;
            FLOAT accelMin = -9.8f;

            pAccelValues[0] = Utils::ClampFLOAT(pAccelValues[0], accelMin, accelMax);
            pAccelValues[1] = Utils::ClampFLOAT(pAccelValues[1], accelMin, accelMax);

            if ((pAccelValues[0] >= 0.0f) && (pAccelValues[1] >= 0.0f))
            {
                // 0 -90 range
                // From 0 to 90 degress, the value move :
                // gravity[0] : from 0 to gravity_max
                // gravity[1] : from gravity_max to 0
                *pOrientationAngle = static_cast<INT32>((pAccelValues[0] / accelMax) * 90);
            }
            else if ((pAccelValues[0] >= 0.0f) && (pAccelValues[1] <= 0.0f))
            {
                // 90 - 180 range
                // From 90 to 180 degress, the value move :
                // gravity[0] : from gravity_max to 0
                // gravity[1] : from 0 to gravity_min
                *pOrientationAngle = 180 - static_cast<INT32>((pAccelValues[0] / accelMax) * 90);
            }
            else if ((pAccelValues[0] <= 0.0f) && (pAccelValues[1] <= 0.0f))
            {
                // 180 - 270 range
                // From 180 to 270 degress, the value move :
                // gravity[0] : from 0 to gravity_min
                // gravity[1] : from gravity_min to 0
                *pOrientationAngle = 180 + static_cast<INT32>((Utils::AbsoluteFLOAT(pAccelValues[0]) / accelMax) * 90);
            }
            else if ((pAccelValues[0] <= 0.0f) && (pAccelValues[1] >= 0.0f))
            {
                // 270 - 360 range
                // From 270 to 360 degress, the value move :
                // gravity[0] : from gravity_min to 0
                // gravity[1] : from 0 to gravity_max
                *pOrientationAngle = 360 - static_cast<INT32>((Utils::AbsoluteFLOAT(pAccelValues[0]) / accelMax) * 90);
            }
        }
    }

    if (NULL != pOrientationAngle)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupFD, "Accel angle = %d", *pOrientationAngle);
    }

    return CamxResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDUtils::AlignAccelToCamera()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult FDUtils::AlignAccelToCamera(
    FLOAT*  pAccelValues,
    UINT32  sensoMountAngle,
    UINT32  cameraPosition)
{
    CamxResult  result = CamxResultSuccess;
    FLOAT       temp;
    CAMX_LOG_VERBOSE(CamxLogGroupFD, "mount angle %d", sensoMountAngle);

    if (NULL == pAccelValues)
    {
        result = CamxResultEInvalidArg;
    }

    if (CamxResultSuccess == result)
    {
        if ((REAR == cameraPosition) || (REAR_AUX == cameraPosition))
        {
            switch (sensoMountAngle)
            {
                case 90:
                    // No Negation.  No axes swap
                    break;
                case 180:
                    // Negate x.  Swap x, y axes
                    temp = pAccelValues[0];
                    pAccelValues[0] = pAccelValues[1];
                    pAccelValues[1] = -temp;
                    break;
                case 270:
                    // Negate x, y.  No axes swap
                    pAccelValues[0] = -pAccelValues[0];
                    pAccelValues[1] = -pAccelValues[1];
                    break;
                case 0:
                    // Negate y.  Swap x, y axes
                    temp = pAccelValues[0];
                    pAccelValues[0] = -pAccelValues[1];
                    pAccelValues[1] = temp;
                    break;
                default:
                    CAMX_ASSERT_ALWAYS_MESSAGE("Invalid sensor mount angle %d", sensoMountAngle);
                    break;
            }
        }
        else if (FRONT == cameraPosition)
        {
            switch (sensoMountAngle)
            {
                case 90:
                    // Negate y, z.  No axes swap
                    pAccelValues[1] = -pAccelValues[1];
                    pAccelValues[2] = -pAccelValues[2];
                    break;
                case 180:
                    // Negate x, y, z.  Swap x, y axes
                    temp = pAccelValues[0];
                    pAccelValues[0] = -pAccelValues[1];
                    pAccelValues[1] = -temp;
                    pAccelValues[2] = -pAccelValues[2];
                    break;
                case 270:
                    // Negate x, z.  No axes swap
                    pAccelValues[0] = -pAccelValues[0];
                    pAccelValues[2] = -pAccelValues[2];
                    break;
                case 0:
                    // Negate z.  Swap x, y axes
                    temp = pAccelValues[0];
                    pAccelValues[0] = pAccelValues[1];
                    pAccelValues[1] = temp;
                    pAccelValues[2] = -pAccelValues[2];
                    break;
                default:
                    CAMX_ASSERT_ALWAYS_MESSAGE("Invalid sensor mount angle %d", sensoMountAngle);
                    break;
            }
        }
        else
        {
            result = CamxResultEInvalidArg;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDUtils::ApplyFlipOnDeviceOrienation()
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID FDUtils::ApplyFlipOnDeviceOrienation(
    FDFlipType  flipType,
    INT32*      pOrientationAngle)
{
    if ((NULL != pOrientationAngle) && (0 <= *pOrientationAngle) && (360 >= *pOrientationAngle))
    {
        switch (flipType)
        {
            case FlipHorizontal :
                CAMX_LOG_VERBOSE(CamxLogGroupFD, "Horizantal flip");
                *pOrientationAngle = (360 - *pOrientationAngle);
                break;
            case FlipVertical :
                CAMX_LOG_VERBOSE(CamxLogGroupFD, "Vertical flip");
                *pOrientationAngle = (360 + 180 - *pOrientationAngle) % 360;
                break;
            case FlipHorizontalVertical :
                CAMX_LOG_VERBOSE(CamxLogGroupFD, "HV flip");
                *pOrientationAngle = (360 - *pOrientationAngle);
                *pOrientationAngle = (360 + 180 - *pOrientationAngle) % 360;
                break;
            case FlipNone:
            default :
                break;
        }
    }
}

CAMX_NAMESPACE_END
