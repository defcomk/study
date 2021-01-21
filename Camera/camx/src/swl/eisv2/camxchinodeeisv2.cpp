////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxchinodeeisv2.cpp
/// @brief Chi node for EISV2
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <system/camera_metadata.h>
#include "camxchinodeeisv2.h"
#include "chi.h"
#include "chincsdefs.h"
#include "eis_1_1_0.h"
#include "inttypes.h"
#include "parametertuningtypes.h"

// NOWHINE FILE CP040: Keyword new not allowed. Use the CAMX_NEW/CAMX_DELETE functions insteads

#undef LOG_TAG
#define LOG_TAG "CHIEISV2"

#define GYRO_SAMPLE_DUMP 0
#define PRINT_ALGO_INIT_DATA 0
#define PRINT_EIS2_PROCESS_INPUT 0
#define PRINT_EIS2_PROCESS_OUTPUT 0
#define PRINT_EIS2_OUTPUT 0
#define DUMP_EIS2_OUTPUT_TO_FILE 0

#define GYRO_FREQUENCY   200

ChiNodeInterface    g_ChiNodeInterface;         ///< The instance of the CAMX Chi interface
UINT32              g_vendorTagBase     = 0;    ///< Chi assigned runtime vendor tag base for the node

const CHAR* pEIS2LibName    = "com.qti.eisv2";

/// @todo (CAMX-1854) the major / minor version shall get from CHI
static const UINT32 ChiNodeMajorVersion = 0;    ///< The major version of CHI interface
static const UINT32 ChiNodeMinorVersion = 0;    ///< The minor version of CHI interface

static const UINT32 ChiNodeCapsEISv2       = 2;                       ///< Supports EIS2.0 algorithm
static const CHAR   EISv2NodeSectionName[] = "com.qti.node.eisv2";    ///< The section name for node

static const UINT32 NumSensors             = 1;        ///< Temp change - Number of camera sensors
static const UINT8  NumICA10Exterpolate    = 4;        ///< Num ICA10 exterpolate corners
static const FLOAT  GyroSamplingRate       = 500.0f;   ///< Gyro Sampling rate

///< EIS config
static const UINT32 EISV2FrameDelay        = 0;     ///< Default Frame Delay
static const FLOAT  EISV2Margin            = 0.2F;  ///< Default Stabilization margin
static const UINT32 GYRO_SAMPLES_BUF_SIZE  = 512;   ///< Max Gyro sample size

///< max gyro dumps alone per frame * 100 chars per gyro sample line per frame info
static const UINT32 GyroDumpSize = static_cast<UINT32>(GYRO_SAMPLES_BUF_SIZE * 103 + 150);

///< Buffer size required in order to dump EIS 3.x output matrices to file
static const UINT32 PerspectiveDumpSize    = static_cast<UINT32>(GYRO_SAMPLES_BUF_SIZE * 1.5 * 3);

///< Buffer size required in order to dump EIS 3.x output grids to file
static const UINT32 GridsDumpSize          = 64 * 1024;

///< Buffer size required in order to dump EIS 3.x init input parameters to file
static const UINT32 InitDumpSize           = 32 * 1024;

///< Virtual domain dimensions
static const uint32_t IcaVirtualDomainWidth  = 8192;
static const uint32_t IcaVirtualDomainHeight = 6144;

///< Virtual domain quantization
///< Todo: need to make following switch dynamically according to ICA version
static const uint32_t IcaVirtualDomainQuantization = 8;

///< Vendor tag section names
static const CHAR IPEICASectionName[]          = "org.quic.camera2.ipeicaconfigs";
static const CHAR IFECropInfoSectionName[]     = "org.quic.camera.ifecropinfo";
static const CHAR SensorMetaSectionName[]      = "org.codeaurora.qcamera3.sensor_meta_data";
static const CHAR QTimerSectionName[]          = "org.quic.camera.qtimer";
static const CHAR StreamDimensionSectionName[] = "org.quic.camera.streamDimension";
static const CHAR MultiCameraInfoSectionName[] = "com.qti.chi.multicamerainfo";
static const CHAR EISRealTimeSectionName[]     = "org.quic.camera.eisrealtime";

///< This is an array of all vendor tag section data
static CHIVENDORTAGDATA g_VendorTagSectionEISV2Node[] =
{
    { "AlgoComplete",         0, sizeof(BOOL) }
};

///< This is an array of all vendor tag section data
static CHIVENDORTAGSECTIONDATA g_VendorTagEISV2NodeSection[] =
{
    {
        EISv2NodeSectionName,  0,
        sizeof(g_VendorTagSectionEISV2Node) / sizeof(g_VendorTagSectionEISV2Node[0]), g_VendorTagSectionEISV2Node,
        CHITAGSECTIONVISIBILITY::ChiTagSectionVisibleToAll
    }
};

///< This is an array of all vendor tag section data
static ChiVendorTagInfo g_VendorTagInfoEISV2Node[] =
{
    {
        &g_VendorTagEISV2NodeSection[0],
        sizeof(g_VendorTagEISV2NodeSection) / sizeof(g_VendorTagEISV2NodeSection[0])
    }
};

///< Vendor tags of interest
static CHIVENDORTAGDATA g_VendorTagSectionICA[] =
{
    { "ICAInPerspectiveTransform", 0, sizeof(IPEICAPerspectiveTransform) },
    { "ICAInGridTransform"       , 0, sizeof(IPEICAGridTransform) },
};

static CHIVENDORTAGDATA g_VendorTagSectionIFECropInfo[] =
{
    { "ResidualCrop", 0, sizeof(IFECropInfo) },
    { "AppliedCrop",  0, sizeof(IFECropInfo) },
    { "SensorIFEAppliedCrop", 0, sizeof(IFECropInfo) }
};

static CHIVENDORTAGDATA g_VendorTagSectionSensorMeta[] =
{
    { "mountAngle",       0, sizeof(INT32) },
    { "cameraPosition",   0, sizeof(INT32) },
    { "sensor_mode_info", 0, sizeof(ChiSensorModeInfo) },
    { "current_mode",     0, sizeof(INT32) },
};

static CHIVENDORTAGDATA g_VendorTagSectionQTimer[] =
{
    { "timestamp", 0, sizeof(ChiTimestampInfo) },
};

static CHIVENDORTAGDATA g_VendorTagSectionStreamDimension[] =
{
    { "preview", 0, sizeof(ChiBufferDimension) },
    { "video",   0, sizeof(ChiBufferDimension) },
};

static CHIVENDORTAGDATA g_VendorTagSectionEISRealTime[] =
{
    { "Enabled",              0, 1 },
    { "RequestedMargin",      0, sizeof(MarginRequest) },
    { "StabilizationMargins", 0, sizeof(StabilizationMargin) },
    { "AdditionalCropOffset", 0, sizeof(ImageDimensions) },
    { "StabilizedOutputDims", 0, sizeof(CHIDimension) }
};

EISV2VendorTags g_vendorTagId = { 0 };

static const UINT32 NumMeshY           = 1;       //< Num of mesh Y
static const UINT32 MaxDimension       = 0xffff;  //< Max dimenion for input/output port
static const UINT32 LUTSize            = 32;      //< Look up table size
static const UINT32 MaxPerspMatrixSize = 9;       //< Max size if the perspective matrix

///< Default Unity perspective matrix
static const FLOAT defaultPerspArray[81] =
{
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
};

static const NodeProperty NodePropertyEIS2InputPortType = NodePropertyVendorStart;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2NodeGetCaps
///
/// @brief  Implementation of PFNNODEGETCAPS defined in chinode.h
///
/// @param  pCapsInfo   Pointer to a structure that defines the capabilities supported by the node.
///
/// @return CDKResultSuccess if success or appropriate error code.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult EISV2NodeGetCaps(
    CHINODECAPSINFO* pCapsInfo)
{
    CDKResult result = CDKResultSuccess;

    if (NULL == pCapsInfo)
    {
        result = CDKResultEInvalidPointer;
        LOG_ERROR(CamxLogGroupChi, "Invalid argument: pCapsInfo is NULL");
    }

    if (CDKResultSuccess == result)
    {
        if (pCapsInfo->size >= sizeof(CHINODECAPSINFO))
        {
            pCapsInfo->nodeCapsMask = ChiNodeCapsEISv2;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "CHINODECAPSINFO is smaller than expected");
            result = CDKResultEFailed;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2NodeQueryVendorTag
///
/// @brief  Implementation of PFNCHIQUERYVENDORTAG defined in chinode.h
///
/// @param  pQueryVendorTag Pointer to a structure that returns the exported vendor tag
///
/// @return CDKResultSuccess if success or appropriate error code.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult EISV2NodeQueryVendorTag(
    CHIQUERYVENDORTAG* pQueryVendorTag)
{
    CDKResult result = CDKResultSuccess;
    result = ChiNodeUtils::QueryVendorTag(pQueryVendorTag, g_VendorTagInfoEISV2Node);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2GetOverrideSettings
///
/// @brief  Function to fetch EISv2 overridesetting
///
/// @param  pOverrideSettings Pointer to overridesettings.
///
/// @return None
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID EISV2GetOverrideSettings(
    EISV2OverrideSettings* pOverrideSettings)
{
    CHAR    scratchString[128]      = { 0 };
    FILE*   pEISSettingsTextFile    = NULL;

    snprintf(scratchString,
             sizeof(scratchString),
             "%s",
             "/vendor/etc/camera/eisoverridesettings.txt");
    pEISSettingsTextFile = ChiNodeUtils::FOpen(scratchString, "r");

    if (NULL == pEISSettingsTextFile)
    {
        // We didn't find an override settings text file
        LOG_ERROR(CamxLogGroupChi, "Could not find EIS override settings text file at: %s", scratchString);
    }
    else
    {
        // We found an override settings text file.
        LOG_INFO(CamxLogGroupCore, "Opening override settings text file: %s", scratchString);

        CHAR*   pSettingString      = NULL;
        CHAR*   pValueString        = NULL;
        CHAR*   pContext            = NULL;
        CHAR    strippedLine[128];

        // Parse the settings file one line at a time
        while (NULL != fgets(scratchString, sizeof(scratchString), pEISSettingsTextFile))
        {
            // First strip off all whitespace from the line to make it easier to handle enum type settings with
            // combined values (e.g. A = B | C | D). After removing the whitespace, we only need to use '=' as the
            // delimiter to extract the setting/value string pair (e.g. setting string = "A", value string =
            // "B|C|D").
            memset(strippedLine, 0x0, sizeof(strippedLine));
            ChiNodeUtils::StrStrip(strippedLine, scratchString, sizeof(strippedLine));

            // Extract a setting/value string pair.
            pSettingString  = ChiNodeUtils::StrTokReentrant(strippedLine, "=", &pContext);
            pValueString    = ChiNodeUtils::StrTokReentrant(NULL,         "=", &pContext);

            // Check for invalid lines
            if ((NULL == pSettingString) || (NULL == pValueString) || ('\0' == pValueString[0]))
            {
                continue;
            }

            // Discard this line if the setting string starts with a semicolon, indicating a comment
            if (';' == pSettingString[0])
            {
                continue;
            }

            if (0 == strcmp("EISWidthMargin", pSettingString))
            {
                FLOAT widthMargin = atof(pValueString);
                if (EISV2Margin != widthMargin)
                {
                    pOverrideSettings->margins.widthMargin = widthMargin;
                }
                LOG_INFO(CamxLogGroupChi, "EISv2 width Margin %f", pOverrideSettings->margins.widthMargin);
            }
            else if (0 == strcmp("EISHeightMargin", pSettingString))
            {
                FLOAT heightMargin = atof(pValueString);
                if (EISV2Margin != heightMargin)
                {
                    pOverrideSettings->margins.heightMargin = heightMargin;
                }
                LOG_INFO(CamxLogGroupChi, "EISv2 height Margin %f", pOverrideSettings->margins.heightMargin);
            }
            else if (0 == strcmp("EISLDCGridEnabled", pSettingString))
            {
                pOverrideSettings->isLDCGridEnabled = (atoi(pValueString) == 1) ? 1 : 0;
                LOG_INFO(CamxLogGroupChi, "EISv2 LDC grid enabled %d", pOverrideSettings->isLDCGridEnabled);
            }
            else if (0 == strcmp("EISv2GyroDumpEnabled", pSettingString))
            {
                pOverrideSettings->isGyroDumpEnabled = (atoi(pValueString) == 1) ? 1 : 0;
                LOG_INFO(CamxLogGroupChi, "EISv2 gyro dump enabled %d", pOverrideSettings->isGyroDumpEnabled);
            }
            else if (0 == strcmp("EISv2OperationMode", pSettingString))
            {
                INT opMode = atoi(pValueString);
                switch (opMode)
                {
                    case 1:
                        pOverrideSettings->algoOperationMode = IS_OPT_CALIBRATION_SYMMETRIC;
                        break;
                    case 2:
                        pOverrideSettings->algoOperationMode = IS_OPT_CALIBRATION_ASYMMETRIC;
                        break;
                    default:
                        pOverrideSettings->algoOperationMode = IS_OPT_REGULAR;
                        break;
                }
                LOG_INFO(CamxLogGroupChi, "EISv2 operation mode %d", pOverrideSettings->algoOperationMode);
            }
        }

        ChiNodeUtils::FClose(pEISSettingsTextFile);
        pEISSettingsTextFile = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2NodeCreate
///
/// @brief  Implementation of PFNNODECREATE defined in chinode.h
///
/// @param  pCreateInfo Pointer to a structure that defines create session information for the node.
///
/// @return CDKResultSuccess if success or appropriate error code.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult EISV2NodeCreate(
    CHINODECREATEINFO* pCreateInfo)
{
    CDKResult      result                    = CDKResultSuccess;
    ChiEISV2Node*  pNode                     = NULL;
    EISV2OverrideSettings overridesettings   = { { 0 } };

    if (NULL == pCreateInfo)
    {
        result = CDKResultEInvalidPointer;
        LOG_ERROR(CamxLogGroupChi, "Invalid argument: pTagTypeInfo is NULL");
    }

    if (CDKResultSuccess == result)
    {
        if (pCreateInfo->size < sizeof(CHINODECREATEINFO))
        {
            LOG_ERROR(CamxLogGroupChi, "CHINODECREATEINFO is smaller than expected");
            result = CDKResultEFailed;
        }
    }

    if (CDKResultSuccess == result)
    {
        overridesettings.algoOperationMode      = IS_OPT_REGULAR;
        overridesettings.isGyroDumpEnabled      = 0;
        overridesettings.isLDCGridEnabled       = 0;
        overridesettings.margins.widthMargin    = EISV2Margin;
        overridesettings.margins.heightMargin   = EISV2Margin;

#if !_WINDOWS
        EISV2GetOverrideSettings(&overridesettings);
#endif
        pNode = new ChiEISV2Node(overridesettings);
        if (NULL == pNode)
        {
            result = CDKResultENoMemory;
        }
    }

    if (CDKResultSuccess == result)
    {
        result = pNode->Initialize(pCreateInfo);
    }

    if (CDKResultSuccess != result)
    {
        if (NULL != pNode)
        {
            delete pNode;
            pNode = NULL;
        }
    }

    if (CDKResultSuccess == result)
    {
        pCreateInfo->phNodeSession = reinterpret_cast<CHIHANDLE*>(pNode);

        CHIMETADATAINFO      metadataInfo     = { 0 };
        const UINT32         tagSize          = 2;
        UINT32               index            = 0;
        CHITAGDATA           tagData[tagSize] = { {0} };
        UINT32               tagList[tagSize];
        BOOL                 enabled          = TRUE;
        MarginRequest        requestedMargin  = { 0 };

        requestedMargin = overridesettings.margins;

        metadataInfo.size       = sizeof(CHIMETADATAINFO);
        metadataInfo.chiSession = pCreateInfo->hChiSession;
        metadataInfo.tagNum     = tagSize;
        metadataInfo.pTagList   = &tagList[0];
        metadataInfo.pTagData   = &tagData[0];

        tagList[index]           = (g_vendorTagId.EISV2EnabledTagId | UsecaseMetadataSectionMask);
        tagData[index].size      = sizeof(CHITAGDATA);
        tagData[index].requestId = 0;
        tagData[index].pData     = &enabled;
        tagData[index].dataSize  = g_VendorTagSectionEISRealTime[0].numUnits;
        index++;

        tagList[index]           = (g_vendorTagId.EISV2RequestedMarginTagId | UsecaseMetadataSectionMask);
        tagData[index].size      = sizeof(CHITAGDATA);
        tagData[index].requestId = 0;
        tagData[index].pData     = &requestedMargin;
        tagData[index].dataSize  = g_VendorTagSectionEISRealTime[1].numUnits;
        index++;

        LOG_VERBOSE(CamxLogGroupChi,
                    "Publishing EISv2 enabled flag %d, requested margin x = %f, y = %f",
                    enabled,
                    requestedMargin.widthMargin,
                    requestedMargin.heightMargin);
        g_ChiNodeInterface.pSetMetadata(&metadataInfo);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2NodeDestroy
///
/// @brief  Implementation of PFNNODEDESTROY defined in chinode.h
///
/// @param  pDestroyInfo    Pointer to a structure that defines the session destroy information for the node.
///
/// @return CDKResultSuccess if success or appropriate error code.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult EISV2NodeDestroy(
    CHINODEDESTROYINFO* pDestroyInfo)
{
    CDKResult      result       = CDKResultSuccess;
    ChiEISV2Node*  pNode        = NULL;
    CHIDATASOURCE* pDataSource  = NULL;

    if ((NULL == pDestroyInfo) || (NULL == pDestroyInfo->hNodeSession))
    {
        result = CDKResultEInvalidPointer;
        LOG_ERROR(CamxLogGroupChi, "Invalid argument: pDestroyInfo is NULL");
    }

    if (CDKResultSuccess == result)
    {
        if (pDestroyInfo->size >= sizeof(CHINODEDESTROYINFO))
        {
            pNode = static_cast<ChiEISV2Node*>(pDestroyInfo->hNodeSession);
            pDataSource = pNode->GetDataSource();
            if (NULL != pDataSource)
            {
                LOG_ERROR(CamxLogGroupChi, "DataSource %p", pNode->GetDataSource());
                g_ChiNodeInterface.pPutDataSource(pDestroyInfo->hNodeSession, pDataSource);
            }
            delete pNode;

            pNode = NULL;
            pDestroyInfo->hNodeSession = NULL;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "CHINODEDESTROYINFO is smaller than expected");
            result = CDKResultEFailed;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2NodeProcRequest
///
/// @brief  Implementation of PFNNODEPROCREQUEST defined in chinode.h
///
/// @param  pProcessRequestInfo Pointer to a structure that defines the information required for processing a request.
///
/// @return CDKResultSuccess if success or appropriate error code.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult EISV2NodeProcRequest(
    CHINODEPROCESSREQUESTINFO* pProcessRequestInfo)
{
    CDKResult result = CDKResultSuccess;

    LOG_VERBOSE(CamxLogGroupChi, "E: PCR for request %" PRIu64, pProcessRequestInfo->frameNum);

    if ((NULL == pProcessRequestInfo) || (NULL == pProcessRequestInfo->hNodeSession))
    {
        result = CDKResultEInvalidPointer;
        LOG_ERROR(CamxLogGroupChi, "Invalid argument");
    }

    if (CDKResultSuccess == result)
    {
        if (pProcessRequestInfo->size >= sizeof(CHINODEPROCESSREQUESTINFO))
        {
            ChiEISV2Node* pNode = static_cast<ChiEISV2Node*>(pProcessRequestInfo->hNodeSession);
            result = pNode->ProcessRequest(pProcessRequestInfo);
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "CHINODEPROCESSREQUESTINFO is smaller than expected");
            result = CDKResultEFailed;
        }
    }

    LOG_VERBOSE(CamxLogGroupChi, "X");
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2NodePostPipelineCreate
///
/// @brief  Implementation of PFNPOSTPIPELINECREATE defined in chinode.h
///
/// @param  hChiHandle Pointer to a CHI handle
///
/// @return CDKResultSuccess if success or appropriate error code.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static CDKResult EISV2NodePostPipelineCreate(
    CHIHANDLE hChiHandle)
{
    CDKResult              result              = CDKResultSuccess;
    ChiEISV2Node*          pNode               = NULL;

    if (NULL == hChiHandle)
    {
        LOG_ERROR(CamxLogGroupChi, "Invalid Chi Handle");
        result = CDKResultEInvalidPointer;
    }
    else
    {
        pNode  = static_cast<ChiEISV2Node*>(hChiHandle);
        result = pNode->PostPipelineCreate();
        if (CDKResultSuccess != result)
        {
            LOG_ERROR(CamxLogGroupChi, "Failed in PostPipelineCreate, result=%d", result);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2NodeSetNodeInterface
///
/// @brief  Implementation of PFCHINODESETNODEINTERFACE defined in chinode.h
///
/// @param  pProcessRequestInfo Pointer to a structure that defines the information required for processing a request.
///
/// @return None
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VOID EISV2NodeSetNodeInterface(
    ChiNodeInterface* pNodeInterface)
{
    CDKResult result = CDKResultSuccess;

    result = ChiNodeUtils::SetNodeInterface(pNodeInterface, EISv2NodeSectionName,
        &g_ChiNodeInterface, &g_vendorTagBase);

    if (CDKResultSuccess != result)
    {
        LOG_ERROR(CamxLogGroupChi, "Set Node Interface Failed");
    }
    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2NodeSetBufferInfo
///
/// @brief  Implementation of PFNNODESETBUFFERINFO defined in chinode.h
///
/// @param  pSetBufferInfo  Pointer to a structure with information to set the output buffer resolution and type.
///
/// @return CDKResultSuccess if success or appropriate error code.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult EISV2NodeSetBufferInfo(
    CHINODESETBUFFERPROPERTIESINFO* pSetBufferInfo)
{
    CDKResult     result = CDKResultSuccess;
    ChiEISV2Node* pNode  = NULL;

    if ((NULL == pSetBufferInfo) || (NULL == pSetBufferInfo->hNodeSession))
    {
        result = CDKResultEInvalidPointer;
        LOG_ERROR(CamxLogGroupChi, "Invalid argument");
    }

    if (CDKResultSuccess == result)
    {
        if (pSetBufferInfo->size >= sizeof(CHINODESETBUFFERPROPERTIESINFO))
        {
            pNode = static_cast<ChiEISV2Node*>(pSetBufferInfo->hNodeSession);
            result = pNode->SetBufferInfo(pSetBufferInfo);
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "CHINODESETBUFFERPROPERTIESINFO is smaller than expected");
            result = CDKResultEFailed;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EISV2NodeQueryBufferInfo
///
/// @brief  Implementation of PFNNODEQUERYBUFFERINFO defined in chinode.h
///
/// @param  pQueryBufferInfo    Pointer to a structure to query the input buffer resolution and type.
///
/// @return CDKResultSuccess if success or appropriate error code.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult EISV2NodeQueryBufferInfo(
    CHINODEQUERYBUFFERINFO* pQueryBufferInfo)
{
    CDKResult result                     = CDKResultSuccess;
    UINT      optimalInputWidth          = 0;
    UINT      optimalInputHeight         = 0;
    UINT32    minInputWidth              = 0;
    UINT32    minInputHeight             = 0;
    UINT32    maxInputWidth              = MaxDimension;
    UINT32    maxInputHeight             = MaxDimension;
    UINT      perOutputPortOptimalWidth  = 0;
    UINT      perOutputPortOptimalHeight = 0;
    UINT32    perOutputPortMinWidth      = 0;
    UINT32    perOutputPortMinHeight     = 0;
    UINT32    perOutputPortMaxWidth      = MaxDimension;
    UINT32    perOutputPortMaxHeight     = MaxDimension;

    if ((NULL == pQueryBufferInfo) || (NULL == pQueryBufferInfo->hNodeSession))
    {
        result = CDKResultEInvalidPointer;
        LOG_ERROR(CamxLogGroupChi, "Invalid argument");
    }

    if (CDKResultSuccess == result)
    {
        if (pQueryBufferInfo->size >= sizeof(CHINODEQUERYBUFFERINFO))
        {
            // For EISV2 node there is no real buffer connected to its output port.
            // But for the purpose of buffer negotiation, we fill buffer query info with default buffer data.
            // IFE needs these info to calculate correct buffer input requirement
            for (UINT outputIndex = 0; outputIndex < pQueryBufferInfo->numOutputPorts; outputIndex++)
            {
                ChiOutputPortQueryBufferInfo* pOutputPort = &pQueryBufferInfo->pOutputPortQueryInfo[outputIndex];

                pOutputPort->outputBufferOption.minW      = perOutputPortMinWidth;
                pOutputPort->outputBufferOption.minH      = perOutputPortMinHeight;
                pOutputPort->outputBufferOption.maxW      = perOutputPortMaxWidth;
                pOutputPort->outputBufferOption.maxH      = perOutputPortMaxHeight;
                pOutputPort->outputBufferOption.optimalW  = perOutputPortOptimalWidth;
                pOutputPort->outputBufferOption.optimalH  = perOutputPortOptimalHeight;
            }

            for (UINT inputIndex = 0; inputIndex < pQueryBufferInfo->numInputPorts; inputIndex++)
            {
                ChiInputPortQueryBufferInfo* pInputOptions = &pQueryBufferInfo->pInputOptions[inputIndex];

                pInputOptions->inputBufferOption.minW      = minInputWidth;
                pInputOptions->inputBufferOption.minH      = minInputHeight;
                pInputOptions->inputBufferOption.maxW      = maxInputWidth;
                pInputOptions->inputBufferOption.maxH      = maxInputHeight;
                pInputOptions->inputBufferOption.optimalW  = optimalInputWidth;
                pInputOptions->inputBufferOption.optimalH  = optimalInputHeight;
            }
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "CHINODEQUERYBUFFERINFO is smaller than expected");
            result = CDKResultEFailed;
        }
    }

    return result;
}

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiNodeEntry
///
/// @brief  Entry point called by the Chi driver to initialize the IS node.
///
/// @param pNodeCallbacks  Pointer to a structure that defines callbacks that the Chi driver sends to the node.
///                        The node must fill in these function pointers.
///
/// @return VOID.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDK_VISIBILITY_PUBLIC VOID ChiNodeEntry(
    CHINODECALLBACKS* pNodeCallbacks)
{
    if (NULL != pNodeCallbacks)
    {
        if (pNodeCallbacks->majorVersion == ChiNodeMajorVersion &&
            pNodeCallbacks->size >= sizeof(CHINODECALLBACKS))
        {
            pNodeCallbacks->majorVersion             = ChiNodeMajorVersion;
            pNodeCallbacks->minorVersion             = ChiNodeMinorVersion;
            pNodeCallbacks->pGetCapabilities         = EISV2NodeGetCaps;
            pNodeCallbacks->pQueryVendorTag          = EISV2NodeQueryVendorTag;
            pNodeCallbacks->pCreate                  = EISV2NodeCreate;
            pNodeCallbacks->pDestroy                 = EISV2NodeDestroy;
            pNodeCallbacks->pQueryBufferInfo         = EISV2NodeQueryBufferInfo;
            pNodeCallbacks->pSetBufferInfo           = EISV2NodeSetBufferInfo;
            pNodeCallbacks->pProcessRequest          = EISV2NodeProcRequest;
            pNodeCallbacks->pChiNodeSetNodeInterface = EISV2NodeSetNodeInterface;
            pNodeCallbacks->pPostPipelineCreate      = EISV2NodePostPipelineCreate;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Chi API major version doesn't match (%d:%d) vs (%d:%d)",
                pNodeCallbacks->majorVersion, pNodeCallbacks->minorVersion,
                ChiNodeMajorVersion, ChiNodeMinorVersion);
        }
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Invalid Argument: %p", pNodeCallbacks);
    }
}
#ifdef __cplusplus
}
#endif // __cplusplus

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::LoadLib
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::LoadLib()
{
    CDKResult   result          = CDKResultSuccess;
    INT         numCharWritten  = 0;
    CHAR        libFilePath[FILENAME_MAX];

    numCharWritten = ChiNodeUtils::SNPrintF(libFilePath,
        FILENAME_MAX,
        "%s%s%s.%s",
        CameraComponentLibPath,
        PathSeparator,
        pEIS2LibName,
        SharedLibraryExtension);
    LOG_INFO(CamxLogGroupChi, "loading EIS lib %s", pEIS2LibName);

    m_hEISv2Lib = ChiNodeUtils::LibMapFullName(libFilePath);

    if (NULL == m_hEISv2Lib)
    {
        LOG_ERROR(CamxLogGroupChi, "Error loading lib %s", libFilePath);
        result = CDKResultEUnableToLoad;
    }

    ///< Map function pointers
    if (CDKResultSuccess == result)
    {
        m_eis2Initialize          = reinterpret_cast<EIS2_INITIALIZE>(ChiNodeUtils::LibGetAddr(m_hEISv2Lib,
                                                                                               "eis2_initialize"));
        m_eis2Process             = reinterpret_cast<EIS2_PROCESS> (ChiNodeUtils::LibGetAddr(m_hEISv2Lib,
                                                                                             "eis2_process"));
        m_eis2Deinitialize        = reinterpret_cast<EIS2_DEINITIALIZE> (ChiNodeUtils::LibGetAddr(m_hEISv2Lib,
                                                                                                  "eis2_deinitialize"));
        m_eis2GetGyroTimeInterval = reinterpret_cast<EIS2_GET_GYRO_INTERVAL>(ChiNodeUtils::
                                                                             LibGetAddr(m_hEISv2Lib,
                                                                                        "eis2_get_gyro_time_interval"));
        m_eis2GetTotalMargin      = reinterpret_cast<EIS2_GET_TOTAL_MARGIN>(ChiNodeUtils::
                                                                            LibGetAddr(m_hEISv2Lib,
                                                                                       "eis2_get_stabilization_margin"));
        m_eis2GetTotalMarginEx    = reinterpret_cast<EIS2_GET_TOTAL_MARGIN_EX>(ChiNodeUtils::
                                                                               LibGetAddr(m_hEISv2Lib,
                                                                                          "eis2_get_stabilization_margin_ex"));
        if ((NULL == m_eis2Initialize)          ||
            (NULL == m_eis2Process)             ||
            (NULL == m_eis2Deinitialize)        ||
            (NULL == m_eis2GetGyroTimeInterval) ||
            (NULL == m_eis2GetTotalMargin)      ||
            (NULL == m_eis2GetTotalMarginEx))
        {
            LOG_ERROR(CamxLogGroupChi,
                      "Error Initializing one or more function pointers in Library: %s"
                      "m_eis2Initialize %p, m_eis2Process %p, m_eis2Deinitialize %p"
                      "m_eis2GetGyroTimeInterval %p, m_eis2GetTotalMargin %p, m_eis2GetTotalMarginEx %p",
                      libFilePath,
                      m_eis2Initialize,
                      m_eis2Process,
                      m_eis2Deinitialize,
                      m_eis2GetGyroTimeInterval,
                      m_eis2GetTotalMargin,
                      m_eis2GetTotalMarginEx);
            result = CDKResultENoSuch;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::UnLoadLib
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::UnLoadLib()
{
    CDKResult result = CDKResultSuccess;

    if (NULL != m_hEISv2Lib)
    {
        result = ChiNodeUtils::LibUnmap(m_hEISv2Lib);
        m_hEISv2Lib = NULL;
    }

    if (CDKResultSuccess != result)
    {
        LOG_ERROR(CamxLogGroupChi, "Uninitialize Failed to unmap lib %s: %d", pEIS2LibName, result);
        result = CDKResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::InitializeLib
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::InitializeLib()
{
    CDKResult            result = CDKResultSuccess;
    VOID*                pData  = NULL;
    is_init_data_common isInitDataCommon;
    is_init_data_sensor isInitDataSensors[NumSensors];

    // Get Sensor Mount Angle
    pData = ChiNodeUtils::GetMetaData(0,
                                      ANDROID_SENSOR_ORIENTATION,
                                      ChiMetadataStatic,
                                      &g_ChiNodeInterface,
                                      m_hChiSession);
    if (NULL != pData)
    {
        LOG_INFO(CamxLogGroupChi, "sensor mount angle %d", *reinterpret_cast<UINT32*>(pData));
        m_perSensorData[0].mountAngle = *reinterpret_cast<UINT32*>(pData);
    }

    // Get Camera Position
    pData = ChiNodeUtils::GetMetaData(0,
                                      g_vendorTagId.cameraPositionTagId,
                                      ChiMetadataStatic,
                                      &g_ChiNodeInterface,
                                      m_hChiSession);
    if (NULL != pData)
    {
        m_perSensorData[0].cameraPosition = *reinterpret_cast<INT32 *>(pData);
    }

    // Get Sensor Mode Info
    pData = ChiNodeUtils::GetMetaData(0,
                                      (g_vendorTagId.sensorInfoTagId | UsecaseMetadataSectionMask),
                                      ChiMetadataDynamic,
                                      &g_ChiNodeInterface,
                                      m_hChiSession);
    if (NULL != pData)
    {
        ChiSensorModeInfo* pSensorInfo = reinterpret_cast<ChiSensorModeInfo*>(pData);

        m_perSensorData[0].sensorDimension.width  = pSensorInfo->frameDimension.width;
        m_perSensorData[0].sensorDimension.height = pSensorInfo->frameDimension.height;
        m_perSensorData[0].targetFPS              = pSensorInfo->frameRate;
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Sensor Data is NULL");
    }

    memset(&isInitDataCommon, 0, sizeof(isInitDataCommon));
    memset(isInitDataSensors, 0, NumSensors * sizeof(*isInitDataSensors));

    isInitDataCommon.is_output_frame_width                   = m_perSensorData[0].outputSize.width;
    isInitDataCommon.is_output_frame_height                  = m_perSensorData[0].outputSize.height;
    isInitDataCommon.num_mesh_y                              = static_cast<uint16_t>(NumMeshY);
    isInitDataCommon.buffer_delay                            = static_cast<uint16_t>(m_lookahead + 1);
    isInitDataCommon.is_type                                 = IS_TYPE_EIS_2_0;
    isInitDataCommon.is_calc_transform_alignment_matrix_mctf = FALSE;
    isInitDataCommon.do_virtual_upscale_in_matrix            = FALSE;
    isInitDataCommon.is_calc_transform_alignment_matrix_orig = FALSE;
    isInitDataCommon.is_warping_using_ica                    = TRUE;
    isInitDataCommon.frame_fps                               = static_cast<uint16_t>(m_perSensorData[0].targetFPS);
    isInitDataCommon.is_sat_applied_in_ica                   = FALSE;
    isInitDataCommon.operation_mode                          = m_algoOperationMode;

    ///<  TBD: for multi-sensor - initialize all the entries
    isInitDataSensors[0].is_input_frame_width  = m_perSensorData[0].inputSize.width;
    isInitDataSensors[0].is_input_frame_height = m_perSensorData[0].inputSize.height;

    isInitDataSensors[0].sensor_mount_angle = m_perSensorData[0].mountAngle;
    isInitDataSensors[0].camera_position    = static_cast <CameraPosition_t> (m_perSensorData[0].cameraPosition);

    result = GetChromatixData(&isInitDataSensors[0]);

#if PRINT_ALGO_INIT_DATA
    PrintAlgoInitData(&isInitDataCommon, isInitDataSensors);
#endif // PRINT_ALGO_INIT_DATA

    if (CDKResultSuccess == result)
    {
        int32_t isResult = m_eis2Initialize(&m_phEIS2Handle, &isInitDataCommon,
                                            &isInitDataSensors[0], m_numOfMulticamSensors);
        if (IS_RET_SUCCESS != isResult)
        {
            LOG_ERROR(CamxLogGroupChi, "EISV2 Algo Init Failed");
            m_algoInitialized = FALSE;
            result            = CDKResultEFailed;
        }
        else
        {
            m_algoInitialized = TRUE;
        }
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "EISV2 Get Chromatix data failed. Algo not initilized");
        m_algoInitialized = FALSE;
        result            = CDKResultEFailed;
    }

    CHIDateTime dateTime    = { 0 };
    bool dateTimeWasUpdated = false;

    if ((TRUE == m_gyroDumpEnabled) && (CDKResultSuccess == result))
    {
        CHAR dumpFilename[FILENAME_MAX];
        ChiNodeUtils::GetDateTime(&dateTime);
        dateTimeWasUpdated = true;

        ChiNodeUtils::SNPrintF(dumpFilename,
                               sizeof(dumpFilename),
                               "%s%seis2_gyro_log_%04d%02d%02d_%02d%02d%02d_%dx%d.txt",
                               FileDumpPath, PathSeparator,
                               dateTime.year + 1900,
                               dateTime.month + 1,
                               dateTime.dayOfMonth,
                               dateTime.hours,
                               dateTime.minutes,
                               dateTime.seconds,
                               m_perSensorData[m_activeSensorIdx].outputSize.width,
                               m_perSensorData[m_activeSensorIdx].outputSize.height);

        m_hGyroDumpFile = ChiNodeUtils::FOpen(dumpFilename, "a");
        if (NULL == m_hGyroDumpFile)
        {
            LOG_ERROR(CamxLogGroupChi, "Can't open %s for gyro dump !", dumpFilename);
        }

        ChiNodeUtils::SNPrintF(dumpFilename,
                               sizeof(dumpFilename),
                               "%s%seis2_init_data_%04d%02d%02d_%02d%02d%02d_%dx%d.txt",
                               FileDumpPath,
                               PathSeparator,
                               dateTime.year + 1900,
                               dateTime.month + 1,
                               dateTime.dayOfMonth,
                               dateTime.hours,
                               dateTime.minutes,
                               dateTime.seconds,
                               m_perSensorData[m_activeSensorIdx].outputSize.width,
                               m_perSensorData[m_activeSensorIdx].outputSize.height);

        FILE* initDataDumpFile = ChiNodeUtils::FOpen(dumpFilename, "a");
        if (NULL == initDataDumpFile)
        {
            LOG_ERROR(CamxLogGroupChi, "Can't open %s for initialization data dump !", dumpFilename);
        }
        else
        {
            WriteInitDataToFile(&isInitDataCommon, isInitDataSensors, NumSensors, initDataDumpFile);

            if (NULL != initDataDumpFile)
            {
                result = ChiNodeUtils::FClose(initDataDumpFile);
                if (CDKResultSuccess != result)
                {
                    LOG_ERROR(CamxLogGroupChi, "initialization data dump file close failed");
                }
                initDataDumpFile = NULL;
            }
        }
    }

#if DUMP_EIS2_OUTPUT_TO_FILE
    if (CDKResultSuccess == result)
    {
        CHAR dumpFilename[FILENAME_MAX];
        if (false == dateTimeWasUpdated)
        {
            ChiNodeUtils::GetDateTime(&dateTime);
        }

        ChiNodeUtils::SNPrintF(dumpFilename,
                               sizeof(dumpFilename),
                               "%s%seis2_matrices_output_%04d%02d%02d_%02d%02d%02d_%dx%d.txt",
                               FileDumpPath,
                               PathSeparator,
                               dateTime.year + 1900,
                               dateTime.month + 1,
                               dateTime.dayOfMonth,
                               dateTime.hours,
                               dateTime.minutes,
                               dateTime.seconds,
                               m_perSensorData[m_activeSensorIdx].outputSize.width,
                               m_perSensorData[m_activeSensorIdx].outputSize.height);

        m_hEisOutputMatricesDumpFile = ChiNodeUtils::FOpen(dumpFilename, "a");
        if (NULL == m_hEisOutputMatricesDumpFile)
        {
            LOG_ERROR(CamxLogGroupChi, "Can't open %s for EIS 3.x output dump !", dumpFilename);
        }

        ChiNodeUtils::SNPrintF(dumpFilename,
                               sizeof(dumpFilename),
                               "%s%seis2_grids_output_%04d%02d%02d_%02d%02d%02d_%dx%d.txt",
                               FileDumpPath,
                               PathSeparator,
                               dateTime.year + 1900,
                               dateTime.month + 1,
                               dateTime.dayOfMonth,
                               dateTime.hours,
                               dateTime.minutes,
                               dateTime.seconds,
                               m_perSensorData[m_activeSensorIdx].outputSize.width,
                               m_perSensorData[m_activeSensorIdx].outputSize.height);

        m_hEisOutputGridsDumpFile = ChiNodeUtils::FOpen(dumpFilename, "a");
        if (NULL == m_hEisOutputGridsDumpFile)
        {
            LOG_ERROR(CamxLogGroupChi, "Can't open %s for EIS 2.x output dump !", dumpFilename);
        }

        if (false != isInitDataCommon.is_calc_transform_alignment_matrix_mctf)
        {
            ChiNodeUtils::SNPrintF(dumpFilename,
                                   sizeof(dumpFilename),
                                   "%s%seis2_matrices_output_mctf_%04d%02d%02d_%02d%02d%02d_%dx%d.txt",
                                   FileDumpPath,
                                   PathSeparator,
                                   dateTime.year + 1900,
                                   dateTime.month + 1,
                                   dateTime.dayOfMonth,
                                   dateTime.hours, dateTime.minutes,
                                   dateTime.seconds,
                                   m_perSensorData[m_activeSensorIdx].outputSize.width,
                                   m_perSensorData[m_activeSensorIdx].outputSize.height);

            m_hEisOutputMatricesMctfDumpFile = ChiNodeUtils::FOpen(dumpFilename, "a");
            if (NULL == m_hEisOutputMatricesMctfDumpFile)
            {
                LOG_ERROR(CamxLogGroupChi, "Can't open %s for EIS 3.x output dump !", dumpFilename);
            }
        }

        if (false != isInitDataCommon.is_calc_transform_alignment_matrix_orig)
        {
            ChiNodeUtils::SNPrintF(dumpFilename,
                                   sizeof(dumpFilename),
                                   "%s%seis2_matrices_output_orig_%04d%02d%02d_%02d%02d%02d_%dx%d.txt",
                                   FileDumpPath,
                                   PathSeparator,
                                   dateTime.year + 1900,
                                   dateTime.month + 1,
                                   dateTime.dayOfMonth,
                                   dateTime.hours,
                                   dateTime.minutes,
                                   dateTime.seconds,
                                   m_perSensorData[m_activeSensorIdx].outputSize.width,
                                   m_perSensorData[m_activeSensorIdx].outputSize.height);

            m_hEisOutputMatricesOrigDumpFile = ChiNodeUtils::FOpen(dumpFilename, "a");
            if (NULL == m_hEisOutputMatricesOrigDumpFile)
            {
                LOG_ERROR(CamxLogGroupChi, "Can't open %s for EIS 3.x output dump !", dumpFilename);
            }
        }
    }
#endif // DUMP_EIS2_OUTPUT_TO_FILE

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::FillVendorTagIds
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::FillVendorTagIds()
{
    CDKResult            result        = CDKResultSuccess;
    CHIVENDORTAGBASEINFO vendorTagBase = { 0 };

    // Get ICA In Perspective Transform Vendor Tag Id
    result = ChiNodeUtils::GetVendorTagBase(IPEICASectionName,
                                            g_VendorTagSectionICA[0].pVendorTagName,
                                            &g_ChiNodeInterface,
                                            &vendorTagBase);
    if (CDKResultSuccess == result)
    {
        // Save ICA In Perspective Transform Vendor Tag Id
        g_vendorTagId.ICAInPerspectiveTransformTagId = vendorTagBase.vendorTagBase;
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Unable to get ICA In Perspective Transform Vendor Tag Id");
    }

    // Get ICA In Grid Transform Vendor Tag Id
    result = ChiNodeUtils::GetVendorTagBase(IPEICASectionName,
                                            g_VendorTagSectionICA[1].pVendorTagName,
                                            &g_ChiNodeInterface,
                                            &vendorTagBase);
    if (CDKResultSuccess == result)
    {
        // Save ICA In Grid Transform Vendor Tag Id
        g_vendorTagId.ICAInGridTransformTagId = vendorTagBase.vendorTagBase;
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Unable to get ICA In Grid Transform Vendor Tag Id");
    }

    if (CDKResultSuccess == result)
    {
        // Get IFE Residual Crop Info Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(IFECropInfoSectionName,
                                                g_VendorTagSectionIFECropInfo[0].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save IFE Residual Crop Info Vendor Tag Id
            g_vendorTagId.residualCropTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get IFE Residual Crop Info Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get IFE Applied Crop Info Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(IFECropInfoSectionName,
                                                g_VendorTagSectionIFECropInfo[1].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save IFE Applied Crop Info Vendor Tag Id
            g_vendorTagId.appliedCropTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get IFE Applied Crop Info Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get Sensor Mount Angle Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(SensorMetaSectionName,
                                                g_VendorTagSectionSensorMeta[0].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save Sensor Mount Angle Vendor Tag Id
            g_vendorTagId.mountAngleTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get Sensor Mount Angle Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get Camera Sensor Position Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(SensorMetaSectionName,
                                                g_VendorTagSectionSensorMeta[1].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save Camera Sensor Position Vendor Tag Id
            g_vendorTagId.cameraPositionTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get Camera Sensor Position Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get Sensor Mode Info Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(SensorMetaSectionName,
                                                g_VendorTagSectionSensorMeta[2].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save Sensor Mode Info Vendor Tag Id
            g_vendorTagId.sensorInfoTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get Sensor Mode Info Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get QTimer SOF Timestamp Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(QTimerSectionName,
                                                g_VendorTagSectionQTimer[0].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save QTimer SOF Timestamp Vendor Tag Id
            g_vendorTagId.QTimerTimestampTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get QTimer SOF Timestamp Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get Preview Stream Dimension Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(StreamDimensionSectionName,
                                                g_VendorTagSectionStreamDimension[0].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save Preview Stream Dimension Vendor Tag Id
            g_vendorTagId.previewStreamDimensionsTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get Preview Stream Dimension Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get Video Stream Dimension Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(StreamDimensionSectionName,
                                                g_VendorTagSectionStreamDimension[1].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save Video Stream Dimension Vendor Tag Id
            g_vendorTagId.videoStreamDimensionsTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get Video Stream Dimension Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get EISv2 enabled flag Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(EISRealTimeSectionName,
                                                g_VendorTagSectionEISRealTime[0].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save EISv2 enabled flag Vendor Tag Id
            g_vendorTagId.EISV2EnabledTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get EISRealTime enabled flag Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get EISRealTimeSectionName Requested Margin Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(EISRealTimeSectionName,
                                                g_VendorTagSectionEISRealTime[1].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save EISv2 Requested Margin Vendor Tag Id
            g_vendorTagId.EISV2RequestedMarginTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get EISRealTime Requested Margin Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get EISRealTimeSectionName actual stabilization margins Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(EISRealTimeSectionName,
                                                g_VendorTagSectionEISRealTime[2].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save EISRealTimeSectionName actual stabilization margins Vendor Tag Id
            g_vendorTagId.EISV2StabilizationMarginsTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get EISRealTimeSectionName actual stabilization margins Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get EISRealTimeSectionName additional crop offset Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(EISRealTimeSectionName,
                                                g_VendorTagSectionEISRealTime[3].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save EISRealTimeSectionName additional crop offset Vendor Tag Id
            g_vendorTagId.EISV2AdditionalCropOffsetTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get EISRealTimeSectionName additional crop offset Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get EISRealTimeSectionName Stabilized Output Dimensions Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(EISRealTimeSectionName,
                                                g_VendorTagSectionEISRealTime[4].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save EISRealTimeSectionName Stabilized Output Dimensions Vendor Tag Id
            g_vendorTagId.EISV2StabilizedOutputDimsTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get EISRealTimeSectionName Stabilized Output Dimensions Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get EISv2 Algorithm completed flag for EISV2 Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(EISv2NodeSectionName,
                                                g_VendorTagSectionEISV2Node[0].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save EISv2 Algorithm completed flag for EISV2 Vendor Tag Id
            g_vendorTagId.EISV2AlgoCompleteTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get EISv2 Algorithm completed flag for EISV2 Vendor Tag Id");
        }
    }

    if (CDKResultSuccess == result)
    {
        // Get Current Sensor Mode Vendor Tag Id
        result = ChiNodeUtils::GetVendorTagBase(SensorMetaSectionName,
                                                g_VendorTagSectionSensorMeta[3].pVendorTagName,
                                                &g_ChiNodeInterface,
                                                &vendorTagBase);
        if (CDKResultSuccess == result)
        {
            // Save Current Sensor Mode Vendor Tag Id
            g_vendorTagId.currentSensorModeTagId = vendorTagBase.vendorTagBase;
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get Current Sensor Mode Vendor Tag Id");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::Initialize(
    CHINODECREATEINFO* pCreateInfo)
{
    CDKResult result                   = CDKResultSuccess;

    /// @todo (CAMX-1854) Check for Node Capabilities using NodeCapsMask
    m_hChiSession     = pCreateInfo->hChiSession;
    m_nodeId          = pCreateInfo->nodeId;
    m_nodeCaps        = pCreateInfo->nodeCaps.nodeCapsMask;
    m_ICAVersion      = pCreateInfo->chiICAVersion;
    m_chiTitanVersion = pCreateInfo->chiTitanVersion;

    // Set flag to indicate chi node wrapper that this node can set its own input buffer dependencies
    pCreateInfo->nodeFlags.canSetInputBufferDependency = TRUE;

    // Check the #define for backward compatibility
#if GYRO_SAMPLE_DUMP
    m_gyroDumpEnabled = TRUE;
#endif  //< GYRO_SAMPLE_DUMP

    //Fill Vendor Tag IDs
    result = FillVendorTagIds();

    if (CDKResultSuccess == result)
    {
        m_metaTagsInitialized = TRUE;
        result = LoadLib();
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Unable to query required vendor tag locations");
    }

    if (CDKResultSuccess == result)
    {
        LOG_INFO(CamxLogGroupChi, "Num node props %d", pCreateInfo->nodePropertyCount);
        for (UINT i = 0; i < pCreateInfo->nodePropertyCount; i++)
        {
            if (NodePropertyEIS2InputPortType == pCreateInfo->pNodeProperties[i].id)
            {
                const CHAR* temp = reinterpret_cast<const CHAR*>(pCreateInfo->pNodeProperties[i].pValue);
                m_inputPortPathType = static_cast<EISV2PATHTYPE>(atoi(temp));
                LOG_INFO(CamxLogGroupChi, "Input port path type %d", m_inputPortPathType);
                break;
            }
        }

        m_pLDCIn2OutGrid    = static_cast<NcLibWarpGridCoord*>(CHI_CALLOC(
                                                              ICA20GridTransformWidth * ICA20GridTransformHeight,
                                                              sizeof(NcLibWarpGridCoord)));
        m_pLDCOut2InGrid    = static_cast<NcLibWarpGridCoord*>(CHI_CALLOC(
                                                              ICA20GridTransformWidth * ICA20GridTransformHeight,
                                                              sizeof(NcLibWarpGridCoord)));

        if ((NULL == m_pLDCIn2OutGrid) || (NULL == m_pLDCOut2InGrid))
        {
            if (NULL != m_pLDCIn2OutGrid)
            {
                CHI_FREE(m_pLDCIn2OutGrid);
                m_pLDCIn2OutGrid = NULL;
            }

            if (NULL != m_pLDCOut2InGrid)
            {
                CHI_FREE(m_pLDCOut2InGrid);
                m_pLDCOut2InGrid = NULL;
            }

            UnLoadLib();
            result = CDKResultENoMemory;
            LOG_ERROR(CamxLogGroupChi, "Falied to allocate memory for final forward looking frame results");
        }
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Load EISv2 algo lib failed");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::SetDependencies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::SetDependencies(
    CHINODEPROCESSREQUESTINFO* pProcessRequestInfo)
{
    CHIDEPENDENCYINFO*   pDependencyInfo  = pProcessRequestInfo->pDependency;
    UINT16               depCount         = 0;

    pDependencyInfo->properties[depCount] = g_vendorTagId.residualCropTagId;
    pDependencyInfo->offsets[depCount]    = 0;
    depCount++;

    pDependencyInfo->properties[depCount] = g_vendorTagId.appliedCropTagId;
    pDependencyInfo->offsets[depCount]    = 0;
    depCount++;

    pDependencyInfo->count                = depCount;
    pDependencyInfo->hNodeSession         = m_hChiSession;

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::GetGyroInterval
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::GetGyroInterval(
    UINT64        frameNum,
    gyro_times_t* pGyroInterval)
{
    VOID*                pData           = NULL;
    CHITIMESTAMPINFO*    timestampInfo   = NULL;
    INT64                frameDuration   = 0;
    INT64                sensorExpTime   = 0;
    CDKResult            result          = CDKResultSuccess;
    frame_times_t        frameTimes      = { 0 };

    ///< Get SOF Timestamp from QTimer
    pData = ChiNodeUtils::GetMetaData(frameNum,
                                      g_vendorTagId.QTimerTimestampTagId,
                                      ChiMetadataDynamic,
                                      &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        timestampInfo = static_cast<CHITIMESTAMPINFO*>(pData);
        LOG_VERBOSE(CamxLogGroupChi, "QTimer timestamp(ns) %" PRIu64, timestampInfo->timestamp);
    }

    ///< Get Frame Duration
    pData = ChiNodeUtils::GetMetaData(frameNum, ANDROID_SENSOR_FRAME_DURATION,
                                      ChiMetadataDynamic, &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        frameDuration = *static_cast<INT64 *>(pData);
        LOG_VERBOSE(CamxLogGroupChi, "Frame Duration(ns) %" PRId64, frameDuration);
    }

    pData = ChiNodeUtils::GetMetaData(frameNum, ANDROID_SENSOR_EXPOSURE_TIME,
                                      ChiMetadataDynamic, &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        sensorExpTime = *(static_cast<INT64 *>(pData));
        LOG_VERBOSE(CamxLogGroupChi, "Sensor Exposure time(ns) %" PRId64, sensorExpTime);
    }

    ///< Initialize the lib if it hasn't been already
    if (FALSE == m_algoInitialized)
    {
        result = InitializeLib();
        if (CDKResultSuccess == result)
        {
            m_algoInitialized = TRUE;
        }
    }

    // Check timestampInfo before dereferencing it.
    if ((CDKResultSuccess == result) && (NULL != timestampInfo))
    {
        frameTimes.sof           = NanoToMicro(timestampInfo->timestamp);
        frameTimes.frame_time    = static_cast<uint32_t>(NanoToMicro(frameDuration));
        frameTimes.exposure_time = static_cast<FLOAT>(NanoToSec(static_cast<DOUBLE>(sensorExpTime)));

        result = m_eis2GetGyroTimeInterval(m_phEIS2Handle, &frameTimes, m_activeSensorIdx, pGyroInterval);

        LOG_VERBOSE(CamxLogGroupChi,
                    "Requested gyro time window (us) %" PRIu64 " %" PRIu64,
                    pGyroInterval->first_gyro_ts,
                    pGyroInterval->last_gyro_ts);

        if (CDKResultSuccess != result)
        {
            LOG_ERROR(CamxLogGroupChi, "Cannot get Gyro interval");
        }
    }
    else
    {
        m_algoInitialized = FALSE;
        LOG_ERROR(CamxLogGroupChi, "Error Initializing EIS2 Algo");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::SetGyroDependency
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::SetGyroDependency(
    CHINODEPROCESSREQUESTINFO* pProcessRequestInfo)
{
    CHIDATAREQUEST    dataRequest;
    gyro_times_t      gyroInterval    = { 0 };
    CDKResult         result          = CDKResultSuccess;
    ChiNCSDataRequest ncsRequest      = {0, {0}, NULL, 0};

    memset(&dataRequest, 0x0, sizeof(CHIDATAREQUEST));

    result = GetGyroInterval(pProcessRequestInfo->frameNum, &gyroInterval);

    if (CDKResultSuccess == result)
    {
        //< Create fence
        CHIFENCECREATEPARAMS chiFenceParams = { 0 };
        chiFenceParams.type   = ChiFenceTypeInternal;
        chiFenceParams.size   = sizeof(CHIFENCECREATEPARAMS);
        CHIFENCEHANDLE hFence = NULL;
        result = g_ChiNodeInterface.pCreateFence(m_hChiSession, &chiFenceParams, &hFence);
        if (CDKResultSuccess == result)
        {

            dataRequest.requestType                          = ChiFetchData;

            ncsRequest.size                    = sizeof(ChiNCSDataRequest);
            ncsRequest.numSamples              = 0;
            ncsRequest.windowRequest.tStart    = QtimerNanoToQtimerTicks(
                                                                   MicroToNano(gyroInterval.first_gyro_ts));
            ncsRequest.windowRequest.tEnd      = QtimerNanoToQtimerTicks(
                                                                   MicroToNano(gyroInterval.last_gyro_ts));
            ncsRequest.hChiFence               = hFence;

            LOG_VERBOSE(CamxLogGroupChi, "Getting gyro interval t1 %" PRIu64 " t2 %" PRIu64 " fence %p",
                      ncsRequest.windowRequest.tStart,
                      ncsRequest.windowRequest.tEnd,
                      ncsRequest.hChiFence);

            dataRequest.hRequestPd = &ncsRequest;

            ///< Request Data in Async mode and go into DRQ
            pProcessRequestInfo->pDependency->pChiFences[0]  = hFence;
            pProcessRequestInfo->pDependency->chiFenceCount  = 1;
            g_ChiNodeInterface.pGetData(m_hChiSession, GetDataSource(), &dataRequest, NULL);
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Create fence Failed on creation %p", hFence);
        }
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Cannot get Gyro interval. Not setting dependency");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::GetChromatixTuningHandle
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::GetChromatixTuningHandle(
    UINT32 sensorIndex)
{
    CHIDATASOURCE chiDataSource;
    TuningMode    tuningMode[2];
    VOID*         pData         = NULL;
    UINT32        sensorMode    = 0;
    CDKResult     result        = CDKResultSuccess;

    if (NULL == m_perSensorData[sensorIndex].pTuningManager)
    {
        chiDataSource.dataSourceType                = ChiTuningManager;
        chiDataSource.pHandle                       = NULL;
        m_perSensorData[sensorIndex].pTuningManager = static_cast<TuningSetManager*>(g_ChiNodeInterface.pGetData(m_hChiSession,
                                                                                     &chiDataSource,
                                                                                     NULL,
                                                                                     NULL));
    }

    if (NULL != m_perSensorData[sensorIndex].pTuningManager)
    {
        pData = ChiNodeUtils::GetMetaData(0,
                                          g_vendorTagId.currentSensorModeTagId | UsecaseMetadataSectionMask,
                                          ChiMetadataDynamic,
                                          &g_ChiNodeInterface,
                                          m_hChiSession);
        if (NULL != pData)
        {
            sensorMode = *reinterpret_cast<UINT32*>(pData);
        }
        else
        {
            result = CDKResultEFailed;
        }
    }

    if (CDKResultSuccess == result)
    {
        tuningMode[0].mode                         = ModeType::Default;
        tuningMode[0].subMode                      = { 0 };
        tuningMode[1].mode                         = ModeType::Sensor;
        tuningMode[1].subMode.value                = static_cast<UINT16>(sensorMode);
        m_perSensorData[sensorIndex].pEISChromatix = static_cast<eis_1_1_0::chromatix_eis11Type*>
                                                    (m_perSensorData[sensorIndex].pTuningManager->GetModule_eis11_sw(
                                                    tuningMode,
                                                    (sizeof(tuningMode) / sizeof(tuningMode[0]))));

        if (NULL == m_perSensorData[sensorIndex].pEISChromatix)
        {
            result = CDKResultEFailed;
        }

        if (CDKResultSuccess == result)
        {
            switch (m_ICAVersion)
            {
                case ChiICAVersion::ChiICA20:
                    m_perSensorData[sensorIndex].pICAChromatix = m_perSensorData[sensorIndex].pTuningManager->
                                                                 GetModule_ica20_ipe_module1(
                                                                 tuningMode,
                                                                 (sizeof(tuningMode) / sizeof(tuningMode[0])));
                    break;
                default:
                    LOG_ERROR(CamxLogGroupChi, "LDC grid not supported");
                    break;
            }
        }
    }

    if (CDKResultEFailed == result)
    {
        LOG_ERROR(CamxLogGroupChi, "Failed to get chromatix handle");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::GetChromatixData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::GetChromatixData(
    is_init_data_sensor* pEISInitData)
{
    eis_1_1_0::chromatix_eis11Type*                              pEISChromatix       = NULL;
    eis_1_1_0::chromatix_eis11_reserveType::generalStruct*       pGeneralStruct      = NULL;
    eis_1_1_0::chromatix_eis11_reserveType::timingStruct*        pTimingStruct       = NULL;
    eis_1_1_0::chromatix_eis11_reserveType::blur_maskingStruct*  pBlurMaskingStruct  = NULL;
    CDKResult                                                    result              = CDKResultSuccess;

    if ((NULL != m_perSensorData[m_activeSensorIdx].pEISChromatix) && (NULL != pEISInitData))
    {
        pEISChromatix       = m_perSensorData[m_activeSensorIdx].pEISChromatix;
        pGeneralStruct      = &pEISChromatix->chromatix_eis11_reserve.general;
        pTimingStruct       = &pEISChromatix->chromatix_eis11_reserve.timing;
        pBlurMaskingStruct  = &pEISChromatix->chromatix_eis11_reserve.blur_masking;

        pEISInitData->is_chromatix_info.general.focal_length           = pGeneralStruct->focal_length;
        if (m_chiTitanVersion == Titan150V100)
        {
            pEISInitData->is_chromatix_info.general.gyro_frequency     = GYRO_FREQUENCY;
            LOG_INFO(CamxLogGroupChi, "Gyro freq %d", pEISInitData->is_chromatix_info.general.gyro_frequency);
        }
        else
        {
            pEISInitData->is_chromatix_info.general.gyro_frequency     = (pGeneralStruct->gyro_frequency == 0) ?
                                                                         static_cast<uint32_t>(GyroSamplingRate) :
                                                                         pGeneralStruct->gyro_frequency;
        }

        pEISInitData->is_chromatix_info.general.gyro_noise_floor       = pGeneralStruct->gyro_noise_floor;
        pEISInitData->is_chromatix_info.general.res_param_1            = pGeneralStruct->res_param_1;
        pEISInitData->is_chromatix_info.general.res_param_2            = pGeneralStruct->res_param_2;
        pEISInitData->is_chromatix_info.general.res_param_3            = pGeneralStruct->res_param_3;
        pEISInitData->is_chromatix_info.general.res_param_4            = pGeneralStruct->res_param_4;
        pEISInitData->is_chromatix_info.general.res_param_5            = pGeneralStruct->res_param_5;
        pEISInitData->is_chromatix_info.general.res_param_6            = pGeneralStruct->res_param_6;
        pEISInitData->is_chromatix_info.general.res_param_7            = pGeneralStruct->res_param_7;
        pEISInitData->is_chromatix_info.general.res_param_8            = pGeneralStruct->res_param_8;
        pEISInitData->is_chromatix_info.general.res_param_9            = pGeneralStruct->res_param_9;
        pEISInitData->is_chromatix_info.general.res_param_10           = pGeneralStruct->res_param_10;
        pEISInitData->is_chromatix_info.general.minimal_total_margin_x = pGeneralStruct->minimal_total_margin_x;
        pEISInitData->is_chromatix_info.general.minimal_total_margin_y = pGeneralStruct->minimal_total_margin_y;

        for (UINT32 i = 0; i < LUTSize; i++)
        {
            pEISInitData->is_chromatix_info.general.res_lut_param_1[i] = pGeneralStruct->res_lut_param_1_tab.res_lut_param_1[i];
            pEISInitData->is_chromatix_info.general.res_lut_param_2[i] = pGeneralStruct->res_lut_param_2_tab.res_lut_param_2[i];
            if (i < (LUTSize / 2))
            {
                pEISInitData->is_chromatix_info.general.res_lut_param_3[i] =
                    pGeneralStruct->res_lut_param_3_tab.res_lut_param_3[i];
            }
        }

        pEISInitData->is_chromatix_info.timing.s3d_offset_1        = pTimingStruct->s3d_offset_1;
        pEISInitData->is_chromatix_info.timing.s3d_offset_2        = pTimingStruct->s3d_offset_2;
        pEISInitData->is_chromatix_info.timing.s3d_offset_3        = pTimingStruct->s3d_offset_3;
        pEISInitData->is_chromatix_info.timing.s3d_offset_4        = pTimingStruct->s3d_offset_4;
        pEISInitData->is_chromatix_info.timing.s3d_threshold_1     = pTimingStruct->s3d_threshold_1;
        pEISInitData->is_chromatix_info.timing.s3d_threshold_2     = pTimingStruct->s3d_threshold_2;
        pEISInitData->is_chromatix_info.timing.s3d_threshold_3     = pTimingStruct->s3d_threshold_3;
        pEISInitData->is_chromatix_info.timing.s3d_threshold_4_ext = pTimingStruct->s3d_threshold_4_ext;

        pEISInitData->is_chromatix_info.blur_masking.enable                 =
            ((pBlurMaskingStruct->enable & TRUE) == TRUE);
        pEISInitData->is_chromatix_info.blur_masking.min_strength           = pBlurMaskingStruct->min_strength;
        pEISInitData->is_chromatix_info.blur_masking.exposure_time_th       = pBlurMaskingStruct->exposure_time_th;
        pEISInitData->is_chromatix_info.blur_masking.start_decrease_at_blur = pBlurMaskingStruct->start_decrease_at_blur;
        pEISInitData->is_chromatix_info.blur_masking.end_decrease_at_blur   = pBlurMaskingStruct->end_decrease_at_blur;
        pEISInitData->is_chromatix_info.blur_masking.blur_masking_res1      = pBlurMaskingStruct->blur_masking_res1;
        pEISInitData->is_chromatix_info.blur_masking.blur_masking_res2      = pBlurMaskingStruct->blur_masking_res2;
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "EIS Chromatix ptr %p, EIS Init Data pointer %p",
                  m_perSensorData[m_activeSensorIdx].pEISChromatix,
                  pEISInitData);
        result = CDKResultEInvalidPointer;
    }

    if ((CDKResultSuccess == result) && (TRUE == m_bIsLDCGridEnabled))
    {
        result = GetLDCGridFromICA20Chromatix(pEISInitData);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::GetLDCGridFromICA20Chromatix
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::GetLDCGridFromICA20Chromatix(
    is_init_data_sensor* pEISInitData)
{
    CDKResult result = CDKResultSuccess;
    ica_2_0_0::chromatix_ica20Type* pICA20Chromatix = m_perSensorData[m_activeSensorIdx].pICAChromatix;
    ica_2_0_0::ica20_rgn_dataType*  pICA20RgnData   = NULL;

    if (ChiICAVersion::ChiICA20 == m_ICAVersion)
    {
        if ((NULL != pICA20Chromatix)   &&
            (NULL != pEISInitData)      &&
            (NULL != m_pLDCIn2OutGrid)  &&
            (NULL != m_pLDCOut2InGrid))
        {
            if (1 == pICA20Chromatix->enable_section.ctc_transform_grid_enable)
            {
                pICA20RgnData = &pICA20Chromatix->chromatix_ica20_core.mod_ica20_lens_posn_data[0].
                    lens_posn_data.mod_ica20_lens_zoom_data[0].
                    lens_zoom_data.mod_ica20_aec_data[0].
                    ica20_rgn_data;

                m_LDCIn2OutWarpGrid.enable                         = pICA20Chromatix->enable_section.ctc_transform_grid_enable;
                m_LDCIn2OutWarpGrid.extrapolateType                = EXTRAPOLATION_TYPE_EXTRA_POINT_ALONG_PERIMETER;
                m_LDCIn2OutWarpGrid.gridExtrapolate                = NULL;
                m_LDCIn2OutWarpGrid.numRows                        = ICA20GridTransformHeight;
                m_LDCIn2OutWarpGrid.numColumns                     = ICA20GridTransformWidth;
                m_LDCIn2OutWarpGrid.transformDefinedOn.widthPixels = IcaVirtualDomainWidth  * IcaVirtualDomainQuantization;
                m_LDCIn2OutWarpGrid.transformDefinedOn.heightLines = IcaVirtualDomainHeight * IcaVirtualDomainQuantization;
                m_LDCIn2OutWarpGrid.grid                           = m_pLDCIn2OutGrid;

                m_LDCOut2InWarpGrid.enable                         = pICA20Chromatix->enable_section.ctc_transform_grid_enable;
                m_LDCOut2InWarpGrid.extrapolateType                = EXTRAPOLATION_TYPE_EXTRA_POINT_ALONG_PERIMETER;
                m_LDCOut2InWarpGrid.gridExtrapolate                = NULL;
                m_LDCOut2InWarpGrid.numRows                        = ICA20GridTransformHeight;
                m_LDCOut2InWarpGrid.numColumns                     = ICA20GridTransformWidth;
                m_LDCOut2InWarpGrid.transformDefinedOn.widthPixels = IcaVirtualDomainWidth  * IcaVirtualDomainQuantization;
                m_LDCOut2InWarpGrid.transformDefinedOn.heightLines = IcaVirtualDomainHeight * IcaVirtualDomainQuantization;
                m_LDCOut2InWarpGrid.grid                           = m_pLDCOut2InGrid;

                for (UINT32 i = 0; i < (ICA20GridTransformWidth * ICA20GridTransformHeight); i++)
                {
                    // Fill m_pLDCIn2OutGrid from chromatix
                    m_pLDCIn2OutGrid[i].x =
                        pICA20RgnData->distorted_input_to_undistorted_ldc_grid_x_tab.
                        distorted_input_to_undistorted_ldc_grid_x[i];
                    m_pLDCIn2OutGrid[i].y =
                        pICA20RgnData->distorted_input_to_undistorted_ldc_grid_y_tab.
                        distorted_input_to_undistorted_ldc_grid_y[i];

                    // Fill m_pLDCOut2InGrid from chromatix
                    m_pLDCOut2InGrid[i].x = pICA20RgnData->ctc_grid_x_tab.ctc_grid_x[i];
                    m_pLDCOut2InGrid[i].y = pICA20RgnData->ctc_grid_y_tab.ctc_grid_y[i];
                }

                pEISInitData->ldc_in2out = &m_LDCIn2OutWarpGrid;
                pEISInitData->ldc_out2in = &m_LDCOut2InWarpGrid;
            }
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi,
                      "ICA Chromatix ptr %p, EIS Init Data pointer %p, m_pLDCIn2OutGrid %p, m_pLDCOut2InGrid %p",
                      pICA20Chromatix,
                      pEISInitData,
                      m_pLDCIn2OutGrid,
                      m_pLDCOut2InGrid);
            result = CDKResultEInvalidPointer;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::FillGyroData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::FillGyroData(
    CHINODEPROCESSREQUESTINFO* pProcessRequestInfo,
    is_input_t*                pEIS2Input)
{
    CHIDATAHANDLE     hNCSDataHandle  = NULL;
    CHIDATAREQUEST    gyroDataRequest;
    CHIDATAREQUEST    gyroDataIterator;
    gyro_times_t      gyroInterval    = { 0 };
    CDKResult         result          = CDKResultSuccess;
    UINT              dataSize        = 0;
    CHIDATASOURCE*    pGyroDataSource = GetDataSource();
    CHIDATASOURCE     accessorObject;
    ChiNCSDataGyro*   pGyroData       = NULL;
    ChiNCSDataRequest ncsRequest      = {0, {0}, NULL, 0};

    memset(&accessorObject, 0x0, sizeof(CHIDATASOURCE));
    memset(&gyroDataRequest, 0x0, sizeof(CHIDATAREQUEST));
    memset(&gyroDataIterator, 0x0, sizeof(CHIDATAREQUEST));

    result = GetGyroInterval(pProcessRequestInfo->frameNum, &gyroInterval);

    if (CDKResultSuccess == result)
    {
        gyroDataRequest.requestType                       = ChiFetchData;
        ncsRequest.size                 = sizeof(ChiNCSDataRequest);
        ncsRequest.numSamples           = 0;
        ncsRequest.windowRequest.tStart = QtimerNanoToQtimerTicks(
                                                                MicroToNano(gyroInterval.first_gyro_ts));
        ncsRequest.windowRequest.tEnd   = QtimerNanoToQtimerTicks(
                                                                MicroToNano(gyroInterval.last_gyro_ts));

        gyroDataRequest.hRequestPd = &ncsRequest;

        // get data accessor handle
        hNCSDataHandle = reinterpret_cast<CHIDATAHANDLE*> (g_ChiNodeInterface.pGetData(m_hChiSession,
                                                                                       pGyroDataSource,
                                                                                       &gyroDataRequest,
                                                                                       &dataSize));
        if (GYRO_SAMPLES_BUF_SIZE < dataSize)
        {
            dataSize = GYRO_SAMPLES_BUF_SIZE;
        }

        if ((NULL != pGyroDataSource) && (NULL != hNCSDataHandle))
        {
            accessorObject.dataSourceType = pGyroDataSource->dataSourceType;
            accessorObject.pHandle        = hNCSDataHandle;

            gyroDataIterator.requestType = ChiIterateData;
            // iterate over the samples
            for (UINT i = 0; i < dataSize; i++)
            {
                gyroDataIterator.index = i;
                pGyroData = NULL;

                pGyroData = static_cast<ChiNCSDataGyro*>(g_ChiNodeInterface.pGetData(m_hChiSession,
                                                                                     &accessorObject,
                                                                                     &gyroDataIterator,
                                                                                     NULL));
                if (NULL != pGyroData)
                {
                    pEIS2Input->gyro_data.gyro_data[i].data[0] = pGyroData->x;
                    pEIS2Input->gyro_data.gyro_data[i].data[1] = pGyroData->y;
                    pEIS2Input->gyro_data.gyro_data[i].data[2] = pGyroData->z;
                    pEIS2Input->gyro_data.gyro_data[i].ts      = NanoToMicro(QtimerTicksToQtimerNano(
                                                                     pGyroData->timestamp));
                    LOG_VERBOSE(CamxLogGroupChi,
                                "x %f y %f z %f ts %" PRIu64,
                                pEIS2Input->gyro_data.gyro_data[i].data[0],
                                pEIS2Input->gyro_data.gyro_data[i].data[1],
                                pEIS2Input->gyro_data.gyro_data[i].data[2],
                                pEIS2Input->gyro_data.gyro_data[i].ts);
                }
                else
                {
                    LOG_ERROR(CamxLogGroupChi, "Unable to get gyro sample from iterator");
                }
            }
            pEIS2Input->gyro_data.num_elements = dataSize;

            g_ChiNodeInterface.pPutData(m_hChiSession, pGyroDataSource, hNCSDataHandle);
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "Unable to get data");
            result = CDKResultEFailed;
        }
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Unable to get gyro time interval");
        result = CDKResultEFailed;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::UpdateZoomWindow
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::UpdateZoomWindow(
    CHIRectangle* pCropRect)
{
    UINT32 adjustedFullWidth;
    UINT32 adjustedFullHeight;
    FLOAT  cropFactor           = 1.0f;
    FLOAT  cropFactorOffsetLeft = 1.0f;
    FLOAT  cropFactorOffsetTop  = 1.0f;

    LOG_VERBOSE(CamxLogGroupChi,
                "Input Zoom Window [%d, %d, %d, %d] sensor size %d x %d margin %d x%d",
                pCropRect->left,
                pCropRect->top,
                pCropRect->width,
                pCropRect->height,
                m_perSensorData[m_activeSensorIdx].inputSize.width,
                m_perSensorData[m_activeSensorIdx].inputSize.height,
                m_stabilizationMargins.widthPixels,
                m_stabilizationMargins.heightLines);

    adjustedFullWidth  = m_perSensorData[m_activeSensorIdx].inputSize.width  - m_stabilizationMargins.widthPixels;
    adjustedFullHeight = m_perSensorData[m_activeSensorIdx].inputSize.height - m_stabilizationMargins.heightLines;

    cropFactor           = static_cast<FLOAT>(pCropRect->height) /
                           m_perSensorData[m_activeSensorIdx].inputSize.height;
    cropFactorOffsetLeft = static_cast<FLOAT>(pCropRect->left) / m_perSensorData[m_activeSensorIdx].inputSize.width;
    cropFactorOffsetTop  = static_cast<FLOAT>(pCropRect->top) / m_perSensorData[m_activeSensorIdx].inputSize.height;

    pCropRect->width  = static_cast<INT32>(adjustedFullWidth  * cropFactor);
    pCropRect->height = static_cast<INT32>(adjustedFullHeight * cropFactor);
    pCropRect->left   = static_cast<INT32>(adjustedFullWidth  * cropFactorOffsetLeft);
    pCropRect->top    = static_cast<INT32>(adjustedFullHeight * cropFactorOffsetTop);

    LOG_VERBOSE(CamxLogGroupChi,
                "Updated Zoom Window [%d, %d, %d, %d] full %d x %d cropFactor %f, left cropFactor %f, top cropFactor %f",
                pCropRect->left,
                pCropRect->top,
                pCropRect->width,
                pCropRect->height,
                adjustedFullWidth,
                adjustedFullHeight,
                cropFactor,
                cropFactorOffsetLeft,
                cropFactorOffsetTop);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::GetCropRectfromCropInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::GetCropRectfromCropInfo(
    IFECropInfo* cropInfo,
    CHIRectangle* cropRect)
{
    switch (m_inputPortPathType)
    {
    case EISV2PATHTYPE::FullPath:
        *cropRect = cropInfo->fullPath;
        break;
    case EISV2PATHTYPE::DS4Path:
        *cropRect = cropInfo->DS4Path;
        break;
    case EISV2PATHTYPE::DS16Path:
        *cropRect = cropInfo->DS16Path;
        break;
    case EISV2PATHTYPE::FDPath:
        *cropRect = cropInfo->FDPath;
        break;
    case EISV2PATHTYPE::DisplayFullPath:
        *cropRect = cropInfo->displayFullPath;
        break;
    case EISV2PATHTYPE::DisplayDS4Path:
        *cropRect = cropInfo->displayDS4Path;
        break;
    case EISV2PATHTYPE::DisplayDS16Path:
        *cropRect = cropInfo->displayDS16Path;
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::ExecuteAlgo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::ExecuteAlgo(
    CHINODEPROCESSREQUESTINFO* pProcessRequestInfo,
    is_output_type* pEIS2Output)
{
    UINT64                    requestId;
    is_input_t                eis2Input                                       = { 0 };
    VOID*                     pData                                           = NULL;
    CHIRectangle*             pIFEResidualCropRect                            = NULL;
    CHIRectangle*             pIFEAppliedCropRect                             = NULL;
    CHIRectangle              IFEResedualcropRect                             = { 0 };
    CHIRectangle              IFEAppliedcropRect                              = { 0 };
    UINT64                    timestamp                                       = 0;
    CDKResult                 result                                          = CDKResultSuccess;
    gyro_sample_t             input_gyro_data_t[GYRO_SAMPLES_BUF_SIZE]        = { { { 0 } } };

    requestId = pProcessRequestInfo->frameNum;

    ///< Get Residual Crop Size
    pData = ChiNodeUtils::GetMetaData(requestId,
                                      g_vendorTagId.residualCropTagId,
                                      ChiMetadataDynamic,
                                      &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        IFECropInfo* pIFEResedualCropInfo = static_cast<IFECropInfo*>(pData);
        pIFEResidualCropRect              = &IFEResedualcropRect;
        GetCropRectfromCropInfo(pIFEResedualCropInfo, pIFEResidualCropRect);

        LOG_VERBOSE(CamxLogGroupChi,
                    "Before update residual Crop info %d, %d, %d, %d",
                    pIFEResidualCropRect->left,
                    pIFEResidualCropRect->top,
                    pIFEResidualCropRect->width,
                    pIFEResidualCropRect->height);

        UpdateZoomWindow(pIFEResidualCropRect);

        LOG_VERBOSE(CamxLogGroupChi,
                    "After update residual Crop info %d, %d, %d, %d",
                    pIFEResidualCropRect->left,
                    pIFEResidualCropRect->top,
                    pIFEResidualCropRect->width,
                    pIFEResidualCropRect->height);
    }

    ///< Get Applied Crop Size
    pData = ChiNodeUtils::GetMetaData(requestId,
                                      g_vendorTagId.appliedCropTagId,
                                      ChiMetadataDynamic,
                                      &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        IFECropInfo* pIFEAppliedCropInfo = static_cast<IFECropInfo*>(pData);
        pIFEAppliedCropRect              = &IFEAppliedcropRect;
        GetCropRectfromCropInfo(pIFEAppliedCropInfo, pIFEAppliedCropRect);

        LOG_VERBOSE(CamxLogGroupChi,
                    "Applied Crop info %d, %d, %d, %d",
                    pIFEAppliedCropRect->left,
                    pIFEAppliedCropRect->top,
                    pIFEAppliedCropRect->width,
                    pIFEAppliedCropRect->height);
    }

    ///< Get SOF Timestamp from QTimer
    pData = ChiNodeUtils::GetMetaData(requestId,
                                      g_vendorTagId.QTimerTimestampTagId,
                                      ChiMetadataDynamic,
                                      &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        timestamp                 = *static_cast<INT64*>(pData);
        eis2Input.frame_times.sof = NanoToMicro(timestamp);

        LOG_VERBOSE(CamxLogGroupChi, "QTimer timestamp(ns) %" PRIu64 ", (us) %" PRIu64, timestamp, eis2Input.frame_times.sof);
    }

    ///< Get Frame Duration
    pData = ChiNodeUtils::GetMetaData(requestId, ANDROID_SENSOR_FRAME_DURATION,
                                      ChiMetadataDynamic, &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        eis2Input.frame_times.frame_time = static_cast<uint32_t>(NanoToMicro(*static_cast<INT64*>(pData)));
    }

    ///< Get Exposure time
    pData = ChiNodeUtils::GetMetaData(pProcessRequestInfo->frameNum, ANDROID_SENSOR_EXPOSURE_TIME,
                                      ChiMetadataDynamic, &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        INT64 exposureTime                  = *static_cast<INT64*>(pData);
        eis2Input.frame_times.exposure_time = static_cast<FLOAT>(NanoToSec(static_cast<DOUBLE>(exposureTime)));
    }

    eis2Input.frame_id                  = static_cast<uint32_t>(requestId);
    eis2Input.active_sensor_idx         = m_activeSensorIdx;
    eis2Input.sat                       = NULL;
    eis2Input.ife_crop.fullWidth        = m_perSensorData[m_activeSensorIdx].sensorDimension.width;
    eis2Input.ife_crop.fullHeight       = m_perSensorData[m_activeSensorIdx].sensorDimension.height;
    eis2Input.ipe_zoom.fullWidth        = m_perSensorData[m_activeSensorIdx].inputSize.width -
                                          m_stabilizationMargins.widthPixels;
    eis2Input.ipe_zoom.fullHeight       = m_perSensorData[m_activeSensorIdx].inputSize.height -
                                          m_stabilizationMargins.heightLines;


    if (NULL != pIFEAppliedCropRect)
    {
        eis2Input.ife_crop.windowWidth  = pIFEAppliedCropRect->width;
        eis2Input.ife_crop.windowHeight = pIFEAppliedCropRect->height;
        eis2Input.ife_crop.windowLeft   = pIFEAppliedCropRect->left;
        eis2Input.ife_crop.windowTop    = pIFEAppliedCropRect->top;
    }
    else
    {
        eis2Input.ife_crop.windowWidth  = m_perSensorData[m_activeSensorIdx].sensorDimension.width;
        eis2Input.ife_crop.windowHeight = m_perSensorData[m_activeSensorIdx].sensorDimension.height;
        eis2Input.ife_crop.windowLeft   = 0;
        eis2Input.ife_crop.windowTop    = 0;
    }

    if (NULL != pIFEResidualCropRect)
    {
        eis2Input.ipe_zoom.windowWidth  = pIFEResidualCropRect->width;
        eis2Input.ipe_zoom.windowHeight = pIFEResidualCropRect->height;
        eis2Input.ipe_zoom.windowLeft   = pIFEResidualCropRect->left;
        eis2Input.ipe_zoom.windowTop    = pIFEResidualCropRect->top;
    }
    else
    {
        eis2Input.ipe_zoom.windowWidth  = eis2Input.ipe_zoom.fullWidth;
        eis2Input.ipe_zoom.windowHeight = eis2Input.ipe_zoom.fullHeight;
        eis2Input.ipe_zoom.windowLeft   = 0;
        eis2Input.ipe_zoom.windowTop    = 0;
    }

    ///< Get rolling shutter skew aka sensor readout duration
    pData = ChiNodeUtils::GetMetaData(requestId, ANDROID_SENSOR_ROLLING_SHUTTER_SKEW,
        ChiMetadataDynamic, &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        INT64 sensorRollingShutterSkewNano    = *static_cast<INT64*>(pData);
        eis2Input.sensor_rolling_shutter_skew = (uint32_t)static_cast<UINT32>(NanoToMicro(sensorRollingShutterSkewNano));
        LOG_VERBOSE(CamxLogGroupChi, "Rolling shutter skew %d", eis2Input.sensor_rolling_shutter_skew);
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "ANDROID_SENSOR_ROLLING_SHUTTER_SKEW is NULL");
    }

    ///< Get Exposure time
    pData = ChiNodeUtils::GetMetaData(pProcessRequestInfo->frameNum, ANDROID_LENS_FOCUS_DISTANCE,
                                      ChiMetadataDynamic, &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        eis2Input.focus_distance = *static_cast<FLOAT*>(pData);
        LOG_VERBOSE(CamxLogGroupChi, "Lens Focus distance %f", eis2Input.focus_distance);
    }

    //< Get Gyro Data
    eis2Input.gyro_data.gyro_data    = &input_gyro_data_t[0];
    eis2Input.gyro_data.num_elements = 0;
    result = FillGyroData(pProcessRequestInfo, &eis2Input);

    if (CDKResultSuccess == result)
    {
        ///< Initialize the lib if it hasn't been already
        if (FALSE == m_algoInitialized)
        {
            result = InitializeLib();
        }

        if (CDKResultSuccess == result)
        {
            m_algoInitialized = TRUE;

#if PRINT_EIS2_PROCESS_INPUT
            PrintEIS2InputParams(&eis2Input);
#endif //< Print EIS2 Input Params

            if (NULL != m_hGyroDumpFile)
            {
                WriteGyroDataToFile(&eis2Input);
            }

            int32_t isResult = m_eis2Process(m_phEIS2Handle, &eis2Input, pEIS2Output);

            if (IS_RET_SUCCESS != isResult)
            {
                LOG_ERROR(CamxLogGroupChi, "EISv2 algo execution Failed (%x) for request %" PRIu64, result, requestId);
                result = CDKResultEFailed;
            }
            else
            {
                if (pEIS2Output->alignment_matrix_orig.enable ||
                    pEIS2Output->alignment_matrix_final.enable)
                {
                    LOG_ERROR(CamxLogGroupChi, "EIS 2.x invalid output");
                    result = CDKResultEFailed;
                }

                if (false == pEIS2Output->has_output)
                {
                    LOG_ERROR(CamxLogGroupChi, "Send default matrix for request %" PRIu64, requestId);
                    result = CDKResultEFailed;
                }
                else
                {
                    if (NULL != m_hEisOutputMatricesDumpFile)
                    {
                        WriteEISVOutputMatricesToFile(pEIS2Output, m_hEisOutputMatricesDumpFile);
                    }
                    if (NULL != m_hEisOutputMatricesMctfDumpFile)
                    {
                        WriteEISVOutputMatricesToFile(pEIS2Output, m_hEisOutputMatricesMctfDumpFile);
                    }
                    if (NULL != m_hEisOutputMatricesOrigDumpFile)
                    {
                        WriteEISVOutputMatricesToFile(pEIS2Output, m_hEisOutputMatricesOrigDumpFile);
                    }
                    if (NULL != m_hEisOutputGridsDumpFile)
                    {
                        WriteEISVOutputGridsToFile(pEIS2Output);
                    }
#if PRINT_EIS2_PROCESS_OUTPUT
                    PrintEIS2OutputParams(pEIS2Output);
#endif //<  PRINT_EIS2_PROCESS_OUTPUT
                }

            }
        }
        else
        {
            LOG_ERROR(CamxLogGroupChi, "EISv2 algo Initialization Failed for request %" PRIu64, requestId);
            result = CDKResultEFailed;
        }
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Cannot get Gyro Data. Not executing EISv2 algo for %" PRIu64, requestId);
        result = CDKResultEFailed;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::IsEISv2Disabled
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ChiEISV2Node::IsEISv2Disabled(
    UINT64 requestId)
{
    VOID* pData      = NULL;
    BOOL eisDisabled = FALSE;
    camera_metadata_enum_android_control_video_stabilization_mode eisMode;

    pData = ChiNodeUtils::GetMetaData(requestId, ANDROID_CONTROL_VIDEO_STABILIZATION_MODE | InputMetadataSectionMask,
                                      ChiMetadataDynamic, &g_ChiNodeInterface, m_hChiSession);
    if (NULL != pData)
    {
        eisMode = *(static_cast<camera_metadata_enum_android_control_video_stabilization_mode *>(pData));
        if (eisMode == ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF)
        {
            eisDisabled = TRUE;
            LOG_VERBOSE(CamxLogGroupChi, "Eisv2 Disabled %d", eisDisabled);
        }
    }
    return eisDisabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::ProcessRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::ProcessRequest(
    CHINODEPROCESSREQUESTINFO* pProcessRequestInfo)
{
    CDKResult          result          = CDKResultSuccess;
    BOOL               hasDependencies = TRUE;
    BOOL               isDisabled      = FALSE;
    INT32              sequenceNumber  = pProcessRequestInfo->pDependency->processSequenceId;
    CHIDEPENDENCYINFO* pDependencyInfo = pProcessRequestInfo->pDependency;
    UINT16             depCount        = 0;
    is_output_type     eis2Output;

    NcLibPerspTransformSingle perspectiveMatrix                                                   = { { 0 } };
    NcLibWarpGridCoord        perspectiveGrid[ICA20GridTransformWidth * ICA20GridTransformHeight] = { { 0 } };
    NcLibWarpGridCoord        gridExtrapolateICA10[NumICA10Exterpolate]                           = { { 0 } };

    LOG_VERBOSE(CamxLogGroupChi, "E.. request id %" PRIu64 ", seq id %d", pProcessRequestInfo->frameNum, sequenceNumber);
    memset(&eis2Output, 0, sizeof(is_output_type));

    if ((0 == sequenceNumber) && (TRUE == hasDependencies))
    {
        // Query Vendor Tag Ids
        if (FALSE == m_metaTagsInitialized)
        {
            result = FillVendorTagIds();

            if (CDKResultSuccess != result)
            {
                LOG_ERROR(CamxLogGroupChi, "Unable to query required vendor tag locations");
            }
            else
            {
                m_metaTagsInitialized = TRUE;
            }
        }

        ///< Get Input Buffer size
        if (pProcessRequestInfo->inputNum > 0)
        {
            m_perSensorData[m_activeSensorIdx].inputSize.width  = pProcessRequestInfo->phInputBuffer[0]->format.width;
            m_perSensorData[m_activeSensorIdx].inputSize.height = pProcessRequestInfo->phInputBuffer[0]->format.height;
        }

        ///< Check if EISv2 is disabled
        isDisabled = IsEISv2Disabled(pProcessRequestInfo->frameNum);
        if (isDisabled)
        {
            hasDependencies       = FALSE;
            eis2Output.has_output = 0;
            eis2Output.frame_id   = static_cast<uint32_t>(pProcessRequestInfo->frameNum);
            UpdateMetaData(pProcessRequestInfo->frameNum, &eis2Output);
        }
        else
        {
            //< If the sequence number is zero then it means we are not called from the DRQ, in which case we need to set
            //< dependencies
            depCount = 0;
            LOG_VERBOSE(CamxLogGroupChi, "Seq number %d", sequenceNumber);

            pDependencyInfo->properties[depCount] = g_vendorTagId.QTimerTimestampTagId;
            pDependencyInfo->offsets[depCount]    = 0;
            pDependencyInfo->count                = ++depCount;


            pDependencyInfo->properties[depCount] = ANDROID_SENSOR_EXPOSURE_TIME;
            pDependencyInfo->offsets[depCount]    = 0;
            pDependencyInfo->count                = ++depCount;

            pDependencyInfo->properties[depCount] = ANDROID_SENSOR_FRAME_DURATION;
            pDependencyInfo->offsets[depCount]    = 0;
            pDependencyInfo->count                = ++depCount;

            pDependencyInfo->hNodeSession = m_hChiSession;

            //< Set the dep property for the sequenece ID, used by the node to determine what state it is in.
            pProcessRequestInfo->pDependency->processSequenceId = 1;
        }
    }
    if ((1 == sequenceNumber) && (TRUE == hasDependencies))
    {
        LOG_VERBOSE(CamxLogGroupChi, "Seq number %d", sequenceNumber);
        SetGyroDependency(pProcessRequestInfo);
        pProcessRequestInfo->pDependency->processSequenceId = 2;
    }
    if ((2 == sequenceNumber) && (TRUE == hasDependencies))
    {
        LOG_VERBOSE(CamxLogGroupChi, "Seq number %d", sequenceNumber);
        // Ensure that algo execution for frame N - 1 has happened before we execute tha algo for frame N
        if (1 < pProcessRequestInfo->frameNum)
        {
            depCount        = 0;
            pDependencyInfo = pProcessRequestInfo->pDependency;

            pDependencyInfo->properties[depCount] = g_vendorTagId.EISV2AlgoCompleteTagId;
            pDependencyInfo->offsets[depCount]    = 1;
            depCount++;

            pDependencyInfo->count                = depCount;
            pDependencyInfo->hNodeSession         = m_hChiSession;
        }
        else
        {
            sequenceNumber = 3;
        }
        pProcessRequestInfo->pDependency->processSequenceId = 3;
    }
    if ((3 == sequenceNumber) && (TRUE == hasDependencies))
    {
        //< Properties dependencies should be met by now. Fill data.
        hasDependencies = FALSE;
        LOG_VERBOSE(CamxLogGroupChi, "Seq number %d", sequenceNumber);

        //< Initialize output matrix
        eis2Output.stabilizationTransform.matrices.perspMatrices = &perspectiveMatrix;
        eis2Output.stabilizationTransform.grid.grid              = &perspectiveGrid[0];
        eis2Output.stabilizationTransform.grid.gridExtrapolate   = &gridExtrapolateICA10[0];
        ///< Execute Algo
        result = ExecuteAlgo(pProcessRequestInfo, &eis2Output);
        if (CDKResultSuccess != result)
        {
            LOG_ERROR(CamxLogGroupChi, "EISv2 algo execution failed for request %" PRIu64, pProcessRequestInfo->frameNum);
            ///< Update metadata with default result
            eis2Output.has_output = FALSE;
            result                = CDKResultSuccess;
        }
        else
        {
            eis2Output.has_output = TRUE;
            LOG_VERBOSE(CamxLogGroupChi, "ICA version is %d", m_ICAVersion);

            // convert ICA20 grid to ICA10 if ICA version is 10
            if (ChiICAVersion::ChiICA10 == m_ICAVersion)
            {
                // converting grid inplace
                result = ConvertICA20GridToICA10Grid(&eis2Output.stabilizationTransform.grid,
                                                     &eis2Output.stabilizationTransform.grid);

                if (CDKResultSuccess != result)
                {
                    LOG_ERROR(CamxLogGroupChi, "Grid conversion failed for request %" PRIu64, pProcessRequestInfo->frameNum);
                    eis2Output.stabilizationTransform.grid.enable = FALSE;
                }
            }
        }

        UpdateMetaData(pProcessRequestInfo->frameNum, &eis2Output);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::PostPipelineCreate
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::PostPipelineCreate()
{
    CDKResult       result     = CDKResultSuccess;
    ImageDimensions marginDims = { 0 };

    CHIDATASOURCECONFIG    CHIDataSourceConfig;
    CHINCSDATASOURCECONFIG NCSDataSourceCfg;

    memset(&NCSDataSourceCfg, 0, sizeof(CHINCSDATASOURCECONFIG));
    NCSDataSourceCfg.sourceType    = ChiDataGyro;
    NCSDataSourceCfg.samplingRate  = GetGyroFrequency(m_activeSensorIdx);
    NCSDataSourceCfg.operationMode = 0;
    NCSDataSourceCfg.reportRate    = 10000;
    NCSDataSourceCfg.size          = sizeof(CHINCSDATASOURCECONFIG);

    memset(&CHIDataSourceConfig, 0, sizeof(CHIDATASOURCECONFIG));
    CHIDataSourceConfig.sourceType  = ChiDataGyro;
    CHIDataSourceConfig.pConfig     = &NCSDataSourceCfg;
    g_ChiNodeInterface.pGetDataSource(m_hChiSession, GetDataSource(), &CHIDataSourceConfig);

    if ((FALSE == m_algoInitialized) && (TRUE == m_earlyInitialization))
    {
        result = InitializeLib();
        if (CDKResultSuccess == result)
        {
            m_algoInitialized = TRUE;
        }
    }
    if (TRUE == m_algoInitialized)
    {
        m_eis2GetTotalMargin(m_phEIS2Handle, m_activeSensorIdx, &marginDims);
        if ((marginDims.widthPixels != m_stabilizationMargins.widthPixels) ||
            (marginDims.heightLines != m_stabilizationMargins.heightLines))
        {
            LOG_ERROR(CamxLogGroupChi,
                      "Unexpected EISv2 margin values. Using %ux%u, calculated %ux%u",
                      m_stabilizationMargins.widthPixels, m_stabilizationMargins.heightLines,
                      marginDims.widthPixels, marginDims.heightLines);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::ChiEISV2Node
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ChiEISV2Node::ChiEISV2Node(
    EISV2OverrideSettings overrideSettings)
    : m_hChiSession(NULL)
    , m_nodeId(0)
    , m_nodeCaps(0)
    , m_pLDCIn2OutGrid(NULL)
    , m_pLDCOut2InGrid(NULL)
{
    m_lookahead                         = 0;
    m_algoInitialized                   = FALSE;
    m_metaTagsInitialized               = FALSE;
    m_activeSensorIdx                   = 0;
    m_numOfMulticamSensors              = NumSensors;
    m_hGyroDumpFile                     = NULL;
    m_hEisOutputMatricesDumpFile        = NULL;
    m_hEisOutputGridsDumpFile           = NULL;
    m_hEisOutputMatricesMctfDumpFile    = NULL;
    m_hEisOutputMatricesOrigDumpFile    = NULL;
    m_earlyInitialization               = TRUE;
    m_hChiDataSource.pHandle            = NULL;
    m_phEIS2Handle                      = NULL;
    m_hChiDataSource.dataSourceType     = ChiDataMax;
    m_gyroDumpEnabled                   = overrideSettings.isGyroDumpEnabled;
    m_algoOperationMode                 = overrideSettings.algoOperationMode;
    m_inputPortPathType                 = EISV2PathType::FullPath;
    m_LDCIn2OutWarpGrid                 = { 0 };
    m_LDCOut2InWarpGrid                 = { 0 };
    m_bIsLDCGridEnabled                 = overrideSettings.isLDCGridEnabled;
    memset(&m_hChiDataSource, 0, sizeof(CHIDATASOURCE));
    memset(&m_perSensorData, 0, sizeof(EISV2PerSensorData));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::~ChiEISV2Node
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ChiEISV2Node::~ChiEISV2Node()
{
    CDKResult result = CDKResultSuccess;

    ///< Deinitialize algo
    if (TRUE == m_algoInitialized)
    {
        int32_t isResult = m_eis2Deinitialize(&m_phEIS2Handle);
        if (IS_RET_SUCCESS != result)
        {
            LOG_ERROR(CamxLogGroupChi, "EISv2 Algo Deinit failed");
        }
        m_algoInitialized = FALSE;
        m_phEIS2Handle    = NULL;
    }

    if (CDKResultSuccess != result)
    {
        LOG_ERROR(CamxLogGroupChi, "EISv2 Algo Deinit failed");
    }
    else
    {
        m_algoInitialized = FALSE;
    }

    //< Unload Lib
    if (NULL != m_hEISv2Lib)
    {
        result = UnLoadLib();
    }

    if (CDKResultSuccess != result)
    {
        LOG_ERROR(CamxLogGroupChi, "EISv2 Lib Unload failed");
    }

    m_hEISv2Lib      = NULL;
    m_hChiSession    = NULL;

    if (NULL != m_hGyroDumpFile)
    {
        result = ChiNodeUtils::FClose(m_hGyroDumpFile);
        if (CDKResultSuccess != result)
        {
            LOG_ERROR(CamxLogGroupChi, "Gyro sample dump file close failed");
        }
        m_hGyroDumpFile = NULL;
    }

    if (NULL != m_hEisOutputMatricesDumpFile)
    {
        result = ChiNodeUtils::FClose(m_hEisOutputMatricesDumpFile);
        if (CDKResultSuccess != result)
        {
            LOG_ERROR(CamxLogGroupChi, "matrices dump file close failed");
        }
        m_hEisOutputMatricesDumpFile = NULL;
    }

    if (NULL != m_hEisOutputMatricesMctfDumpFile)
    {
        result = ChiNodeUtils::FClose(m_hEisOutputMatricesMctfDumpFile);
        if (CDKResultSuccess != result)
        {
            LOG_ERROR(CamxLogGroupChi, "matrices MCTF dump file close failed");
        }
        m_hEisOutputMatricesMctfDumpFile = NULL;
    }

    if (NULL != m_hEisOutputMatricesOrigDumpFile)
    {
        result = ChiNodeUtils::FClose(m_hEisOutputMatricesOrigDumpFile);
        if (CDKResultSuccess != result)
        {
            LOG_ERROR(CamxLogGroupChi, "matrices orig dump file close failed");
        }
        m_hEisOutputMatricesOrigDumpFile = NULL;
    }

    if (NULL != m_hEisOutputGridsDumpFile)
    {
        result = ChiNodeUtils::FClose(m_hEisOutputGridsDumpFile);
        if (CDKResultSuccess != result)
        {
            LOG_ERROR(CamxLogGroupChi, "grids dump file close failed");
        }
        m_hEisOutputGridsDumpFile = NULL;
    }

    if (NULL != m_pLDCIn2OutGrid)
    {
        CHI_FREE(m_pLDCIn2OutGrid);
        m_pLDCIn2OutGrid = NULL;
    }

    if (NULL != m_pLDCOut2InGrid)
    {
        CHI_FREE(m_pLDCOut2InGrid);
        m_pLDCOut2InGrid = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::SetBufferInfo
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::SetBufferInfo(
    CHINODESETBUFFERPROPERTIESINFO* pSetBufferInfo)
{
    CDKResult                   result          = CDKResultSuccess;
    ImageDimensions             marginDims      = { 0 };
    is_get_stabilization_margin inputDims       = { 0 };
    FLOAT                       minTotalMarginX = 0.0F;
    FLOAT                       minTotalMarginY = 0.0F;
    CHIDimension                outDimension    = { 0 };
    VOID*                       pData           = NULL;

    m_perSensorData[m_activeSensorIdx].inputSize.width  = pSetBufferInfo->pFormat->width;
    m_perSensorData[m_activeSensorIdx].inputSize.height = pSetBufferInfo->pFormat->height;

    GetChromatixTuningHandle(m_activeSensorIdx);
    GetMinTotalMargins(&minTotalMarginX, &minTotalMarginY);

    pData = ChiNodeUtils::GetMetaData(0,
                                      (g_vendorTagId.EISV2StabilizedOutputDimsTagId | UsecaseMetadataSectionMask),
                                      ChiMetadataDynamic,
                                      &g_ChiNodeInterface,
                                      m_hChiSession);
    if (NULL != pData)
    {
        outDimension = *static_cast<CHIDimension*>(pData);
    }

    m_perSensorData[m_activeSensorIdx].outputSize.width  = outDimension.width;
    m_perSensorData[m_activeSensorIdx].outputSize.height = outDimension.height;

    inputDims.sensor_is_input_frame_width                = pSetBufferInfo->pFormat->width;
    inputDims.sensor_is_input_frame_height               = pSetBufferInfo->pFormat->height;
    inputDims.sensor_minimal_total_margin_x              = minTotalMarginX;
    inputDims.sensor_minimal_total_margin_y              = minTotalMarginY;
    inputDims.common_is_output_frame_width               = outDimension.width;
    inputDims.common_is_output_frame_height              = outDimension.height;
    inputDims.common_do_virtual_upscale_in_matrix        = FALSE;

    m_eis2GetTotalMarginEx(&inputDims, &marginDims);
    m_stabilizationMargins.widthPixels = marginDims.widthPixels;
    m_stabilizationMargins.heightLines = marginDims.heightLines;

    LOG_ERROR(CamxLogGroupChi, "From in %ux%u and out %ux%u using stabilization margins %ux%u with virtual margins %fx%f",
              pSetBufferInfo->pFormat->width, pSetBufferInfo->pFormat->height,
              outDimension.width, outDimension.height,
              m_stabilizationMargins.widthPixels, m_stabilizationMargins.heightLines,
              minTotalMarginX, minTotalMarginY);

    GetAdditionalCropOffset();

    StabilizationMargin actualMargin;
    memset(&actualMargin, 0, sizeof(StabilizationMargin));

    actualMargin.widthPixels = m_stabilizationMargins.widthPixels - m_additionalCropOffset.widthPixels;
    actualMargin.heightLines = m_stabilizationMargins.heightLines - m_additionalCropOffset.heightLines;

    CHIMETADATAINFO      metadataInfo     = { 0 };
    const UINT32         tagSize          = 2;
    UINT32               index            = 0;
    CHITAGDATA           tagData[tagSize] = { {0} };
    UINT32               tagList[tagSize];

    metadataInfo.size       = sizeof(CHIMETADATAINFO);
    metadataInfo.chiSession = m_hChiSession;
    metadataInfo.tagNum     = tagSize;
    metadataInfo.pTagList   = &tagList[index];
    metadataInfo.pTagData   = &tagData[index];

    tagList[index]           = (g_vendorTagId.EISV2StabilizationMarginsTagId | UsecaseMetadataSectionMask);
    tagData[index].size      = sizeof(CHITAGDATA);
    tagData[index].requestId = 0;
    tagData[index].pData     = &actualMargin;
    tagData[index].dataSize  = g_VendorTagSectionEISRealTime[2].numUnits;
    index++;

    tagList[index]           = (g_vendorTagId.EISV2AdditionalCropOffsetTagId | UsecaseMetadataSectionMask);
    tagData[index].size      = sizeof(CHITAGDATA);
    tagData[index].requestId = 0;
    tagData[index].pData     = &m_additionalCropOffset;
    tagData[index].dataSize  = g_VendorTagSectionEISRealTime[3].numUnits;

    LOG_VERBOSE(CamxLogGroupChi, "Publishing actual margins %ux%u and additional crop offset %ux%u from eisv2",
                actualMargin.widthPixels, actualMargin.heightLines,
                m_additionalCropOffset.widthPixels, m_additionalCropOffset.heightLines);

    g_ChiNodeInterface.pSetMetadata(&metadataInfo);

    return CDKResultSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::UpdateMetaData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::UpdateMetaData(
    UINT64          requestId,
    is_output_type* pAlgoResult)
{
    UINT32 tagIdList[] =
    {
        g_vendorTagId.ICAInPerspectiveTransformTagId,
        g_vendorTagId.ICAInGridTransformTagId,
        g_vendorTagId.EISV2AlgoCompleteTagId
    };

    CHIMETADATAINFO  metadataInfo          = { 0 };
    const UINT32     tagSize               = sizeof(tagIdList) / sizeof(tagIdList[0]);
    UINT32           index                 = 0;
    CHITAGDATA       tagData[tagSize]      = { { 0 } };
    UINT32           tagList[tagSize]      = { 0 };

    metadataInfo.size       = sizeof(CHIMETADATAINFO);
    metadataInfo.chiSession = m_hChiSession;
    metadataInfo.tagNum     = tagSize;
    metadataInfo.pTagList   = &tagList[0];
    metadataInfo.pTagData   = &tagData[0];

    ///< Update resulting perspective matrix
    IPEICAPerspectiveTransform perspectiveTrans;
    memset(&perspectiveTrans, 0, sizeof(IPEICAPerspectiveTransform));

    IPEICAGridTransform gridTrans;
    memset(&gridTrans, 0, sizeof(IPEICAGridTransform));

    if (TRUE == pAlgoResult->has_output)
    {
        if (TRUE == pAlgoResult->stabilizationTransform.matrices.enable)
        {
            NcLibWarpMatrices resultMatrices                  = pAlgoResult->stabilizationTransform.matrices;
            perspectiveTrans.perspectiveTransformEnable       = resultMatrices.enable;
            perspectiveTrans.perspectiveConfidence            = resultMatrices.confidence;
            perspectiveTrans.byPassAlignmentMatrixAdjustement = FALSE;
            perspectiveTrans.perspetiveGeometryNumColumns     = resultMatrices.numColumns;
            perspectiveTrans.perspectiveGeometryNumRows       = static_cast<UINT8>(resultMatrices.numRows);

            //TBD: Change from active sensor to what was passed to the algo
            perspectiveTrans.transformDefinedOnWidth     = m_perSensorData[m_activeSensorIdx].inputSize.width;
            perspectiveTrans.transformDefinedOnHeight    = m_perSensorData[m_activeSensorIdx].inputSize.height;
            perspectiveTrans.ReusePerspectiveTransform   = 0;

            memcpy(&perspectiveTrans.perspectiveTransformArray, &resultMatrices.perspMatrices[0].T[0],
                   sizeof(NcLibPerspTransformSingle));
        }
        else
        {
            LOG_INFO(CamxLogGroupChi, "Algo has perspective transform disabled for req id %" PRId64, requestId);
        }

        if (TRUE == pAlgoResult->stabilizationTransform.grid.enable)
        {
            NcLibWarpGrid resultGrid                        = pAlgoResult->stabilizationTransform.grid;
            gridTrans.gridTransformEnable                   = resultGrid.enable;
            gridTrans.gridTransformArrayExtrapolatedCorners =
                (resultGrid.extrapolateType == NcLibWarpGridExtrapolationType::EXTRAPOLATION_TYPE_FOUR_CORNERS) ? 1 : 0;

            if (1 == gridTrans.gridTransformArrayExtrapolatedCorners)
            {
                memcpy(&gridTrans.gridTransformArrayCorners[0],
                       resultGrid.gridExtrapolate,
                       sizeof(gridTrans.gridTransformArrayCorners));
            }

            //TBD: Change from active sensor to what was passed to the algo
            gridTrans.transformDefinedOnWidth   = m_perSensorData[m_activeSensorIdx].inputSize.width;
            gridTrans.transformDefinedOnHeight  = m_perSensorData[m_activeSensorIdx].inputSize.height;
            gridTrans.reuseGridTransform        = 0;

            memcpy(&gridTrans.gridTransformArray[0], resultGrid.grid,
                   sizeof(ICAGridArray) * resultGrid.numRows * resultGrid.numColumns);
        }
        else
        {
            LOG_INFO(CamxLogGroupChi, "Algo has grid transform disabled for req id %" PRId64, requestId);
        }

        if ((FALSE == pAlgoResult->stabilizationTransform.matrices.enable) &&
            (FALSE == pAlgoResult->stabilizationTransform.grid.enable))
        {
            LOG_ERROR(CamxLogGroupChi, "ERROR: Algo has no perspective or grid transform for req id %" PRId64, requestId);
        }
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Algo has no output for request id %" PRId64, requestId);
        perspectiveTrans.perspectiveTransformEnable       = TRUE;
        perspectiveTrans.perspectiveConfidence            = 1;
        perspectiveTrans.byPassAlignmentMatrixAdjustement = TRUE;
        perspectiveTrans.perspetiveGeometryNumColumns     = 1;
        perspectiveTrans.perspectiveGeometryNumRows       = 1;

        //TBD: Change from active sensor to what was passed to the algo
        perspectiveTrans.transformDefinedOnWidth   = m_perSensorData[m_activeSensorIdx].inputSize.width;
        perspectiveTrans.transformDefinedOnHeight  = m_perSensorData[m_activeSensorIdx].inputSize.height;
        perspectiveTrans.ReusePerspectiveTransform = 0;
        memcpy(&perspectiveTrans.perspectiveTransformArray, &defaultPerspArray, sizeof(NcLibPerspTransformSingle));
    }

#if PRINT_EIS2_OUTPUT
    PrintEIS2OutputParams(pAlgoResult);
#endif //< Print EIS2 Output Data

    // Publish matrix
    tagList[index]           = tagIdList[index];
    tagData[index].size      = sizeof(CHITAGDATA);
    tagData[index].requestId = requestId;
    tagData[index].pData     = &perspectiveTrans;
    tagData[index].dataSize  = g_VendorTagSectionICA[0].numUnits;
    index++;

    // Publish grid
    tagList[index]           = tagIdList[index];
    tagData[index].size      = sizeof(CHITAGDATA);
    tagData[index].requestId = requestId;
    tagData[index].pData     = &gridTrans;
    tagData[index].dataSize  = g_VendorTagSectionICA[1].numUnits;
    index++;

    // Publish EISv2 Algo Completion
    BOOL complete            = TRUE;
    tagList[index]           = tagIdList[index];
    tagData[index].size      = sizeof(CHITAGDATA);
    tagData[index].requestId = requestId;
    tagData[index].pData     = &complete;
    tagData[index].dataSize  = g_VendorTagSectionEISV2Node[0].numUnits;

    g_ChiNodeInterface.pSetMetadata(&metadataInfo);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::GetVirtualMargin
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::GetMinTotalMargins(
    FLOAT* minTotalMarginX,
    FLOAT* minTotalMarginY)
{
    eis_1_1_0::chromatix_eis11Type*                        pEISChromatix  = NULL;
    eis_1_1_0::chromatix_eis11_reserveType::generalStruct* pGeneralStruct = NULL;

    if (NULL != m_perSensorData[m_activeSensorIdx].pEISChromatix)
    {
        pEISChromatix               = m_perSensorData[m_activeSensorIdx].pEISChromatix;
        pGeneralStruct              = &pEISChromatix->chromatix_eis11_reserve.general;
        *minTotalMarginX            = pGeneralStruct->minimal_total_margin_x;
        *minTotalMarginY            = pGeneralStruct->minimal_total_margin_y;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::GetGyroFrequency
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FLOAT ChiEISV2Node::GetGyroFrequency(
    UINT32 activeSensorIndex)
{
    FLOAT                                                  gyroFrequency  = GyroSamplingRate;
    eis_1_1_0::chromatix_eis11Type*                        pEISChromatix  = NULL;
    eis_1_1_0::chromatix_eis11_reserveType::generalStruct* pGeneralStruct = NULL;

    if (NULL != m_perSensorData[activeSensorIndex].pEISChromatix)
    {
        pEISChromatix   = m_perSensorData[activeSensorIndex].pEISChromatix;
        pGeneralStruct  = &pEISChromatix->chromatix_eis11_reserve.general;
        gyroFrequency   = static_cast<FLOAT>(pGeneralStruct->gyro_frequency);
    }

    return (gyroFrequency == 0) ? GyroSamplingRate : gyroFrequency;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::GetAdditionalCropOffset
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::GetAdditionalCropOffset()
{
    UINT32 width  = 0;
    UINT32 height = 0;
    m_additionalCropOffset.widthPixels = 0;
    m_additionalCropOffset.heightLines = 0;

    // get the physical margin
    width  = m_perSensorData[m_activeSensorIdx].inputSize.width  - m_perSensorData[m_activeSensorIdx].outputSize.width;
    height = m_perSensorData[m_activeSensorIdx].inputSize.height - m_perSensorData[m_activeSensorIdx].outputSize.height;

    width  = ChiNodeUtils::AlignGeneric32(width, 2);
    height = ChiNodeUtils::AlignGeneric32(height, 2);

    if ((m_stabilizationMargins.widthPixels >= width) && (m_stabilizationMargins.heightLines >= height))
    {
        m_additionalCropOffset.widthPixels = m_stabilizationMargins.widthPixels - width;
        m_additionalCropOffset.heightLines = m_stabilizationMargins.heightLines - height;
    }
    else
    {
        LOG_ERROR(CamxLogGroupChi, "Unexpected physical margin: %ux%u", width, height);
    }
    LOG_VERBOSE(CamxLogGroupChi, "Additional Crop Offset: %ux%u",
                m_additionalCropOffset.widthPixels, m_additionalCropOffset.heightLines);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::PrintAlgoInitData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::PrintAlgoInitData(
    is_init_data_common * pISInitDataCommon,
    is_init_data_sensor * pISInitDataSensors)
{
    const is_init_data_common* common = pISInitDataCommon;
    UINT32 idx, i;

    LOG_VERBOSE(CamxLogGroupChi, "Initialize EISv2 lib");
    LOG_VERBOSE(CamxLogGroupChi, "Num Mesh Y %d", common->num_mesh_y);
    LOG_VERBOSE(CamxLogGroupChi, "Frame FPS %d", common->frame_fps);
    LOG_VERBOSE(CamxLogGroupChi, "Output Width %d", common->is_output_frame_width);
    LOG_VERBOSE(CamxLogGroupChi, "Output Height %d", common->is_output_frame_height);
    LOG_VERBOSE(CamxLogGroupChi, "Virtual Upscale in matrix %d", common->do_virtual_upscale_in_matrix);
    LOG_VERBOSE(CamxLogGroupChi, "EIS in ICA %d", common->is_warping_using_ica);
    LOG_VERBOSE(CamxLogGroupChi, "Alignment with MCTF %d", common->is_calc_transform_alignment_matrix_mctf);
    LOG_VERBOSE(CamxLogGroupChi, "Aignment matrix orig %d", common->is_calc_transform_alignment_matrix_orig);
    LOG_VERBOSE(CamxLogGroupChi, "SAT in ICA %d", common->is_sat_applied_in_ica);
    LOG_VERBOSE(CamxLogGroupChi, "Buffer Delay %d", common->buffer_delay);

    //< Print per sensor data

    for (idx = 0; idx < m_numOfMulticamSensors; idx++)
    {
        const is_init_data_sensor* sensor = &pISInitDataSensors[idx];
        const is_chromatix_info_t* chromatix = &sensor->is_chromatix_info;

        LOG_VERBOSE(CamxLogGroupChi, "Sensor Idx %d data:", idx);

        LOG_VERBOSE(CamxLogGroupChi, "Input Frame Width %d", sensor->is_input_frame_width);
        LOG_VERBOSE(CamxLogGroupChi, "Input Frame Height %d", sensor->is_input_frame_height);
        LOG_VERBOSE(CamxLogGroupChi, "Mount Angle %d", sensor->sensor_mount_angle);
        LOG_VERBOSE(CamxLogGroupChi, "Camera Position %d", sensor->camera_position);

        LOG_VERBOSE(CamxLogGroupChi, "Focal length %d", chromatix->general.focal_length);
        LOG_VERBOSE(CamxLogGroupChi, "Gyro Frequency %d", chromatix->general.gyro_frequency);

        LOG_VERBOSE(CamxLogGroupChi, "Min total margin X %.15lf", chromatix->general.minimal_total_margin_x);
        LOG_VERBOSE(CamxLogGroupChi, "Min total margin Y %.15lf", chromatix->general.minimal_total_margin_y);
        LOG_VERBOSE(CamxLogGroupChi, "Gyro Noise Floor %.15lf", chromatix->general.gyro_noise_floor);

        for (i = 0; i < LUTSize; i++)
        {
            LOG_VERBOSE(CamxLogGroupChi, "Res LUT Param1[%d]: %f", i, chromatix->general.res_lut_param_1[i]);
        }

        for (i = 0; i < LUTSize; i++)
        {
            LOG_VERBOSE(CamxLogGroupChi, "Res LUT Param2[%d]: %f", i, chromatix->general.res_lut_param_2[i]);
        }

        for (i = 0; i < (LUTSize / 2); i++)
        {
            LOG_VERBOSE(CamxLogGroupChi, "Res LUT Param3[%d]: %u", i, chromatix->general.res_lut_param_3[i]);
        }

        LOG_VERBOSE(CamxLogGroupChi, "S3D Offset 1 %d", chromatix->timing.s3d_offset_1);
        LOG_VERBOSE(CamxLogGroupChi, "S3D Offset 2 %d", chromatix->timing.s3d_offset_2);
        LOG_VERBOSE(CamxLogGroupChi, "S3D Offset 3 %d", chromatix->timing.s3d_offset_3);
        LOG_VERBOSE(CamxLogGroupChi, "S3D Offset 4 %d", chromatix->timing.s3d_offset_4);

        LOG_VERBOSE(CamxLogGroupChi, "S3D Threshold 1 %.15lf", chromatix->timing.s3d_threshold_1);
        LOG_VERBOSE(CamxLogGroupChi, "S3D Threshold 2 %.15lf", chromatix->timing.s3d_threshold_2);
        LOG_VERBOSE(CamxLogGroupChi, "S3D Threshold 3 %.15lf", chromatix->timing.s3d_threshold_3);
        LOG_VERBOSE(CamxLogGroupChi, "S3D Threshold 4 ext %.15lf", chromatix->timing.s3d_threshold_4_ext);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::PrintEIS2InputParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::PrintEIS2InputParams(
    is_input_t* pEIS2Input)
{
    LOG_VERBOSE(CamxLogGroupChi, "Execute EISv2 lib for request %d", pEIS2Input->frame_id);
    LOG_VERBOSE(CamxLogGroupChi, "Exposure time %f", pEIS2Input->frame_times.exposure_time);
    LOG_VERBOSE(CamxLogGroupChi, "SOF time %" PRIu64, pEIS2Input->frame_times.sof);
    LOG_VERBOSE(CamxLogGroupChi, "Frame Duration %u", pEIS2Input->frame_times.frame_time);
    LOG_VERBOSE(CamxLogGroupChi, "Num of gyro samples %d", pEIS2Input->gyro_data.num_elements);
    LOG_VERBOSE(CamxLogGroupChi, "ife_crop fullWidth %d, fullWidth %d", pEIS2Input->ife_crop.fullWidth,
                pEIS2Input->ife_crop.fullHeight);
    LOG_VERBOSE(CamxLogGroupChi, "ife_crop Width %d, Height %d, Left %d, Top %d",
                pEIS2Input->ife_crop.windowWidth,
                pEIS2Input->ife_crop.windowHeight,
                pEIS2Input->ife_crop.windowLeft,
                pEIS2Input->ife_crop.windowTop);
    LOG_VERBOSE(CamxLogGroupChi, "ZoomWindow fullWidth %d, fullWidth %d", pEIS2Input->ipe_zoom.fullWidth,
                pEIS2Input->ipe_zoom.fullHeight);
    LOG_VERBOSE(CamxLogGroupChi, "ZoomWindow Width %d, Height %d, Left %d, Top %d",
                pEIS2Input->ipe_zoom.windowWidth,
                pEIS2Input->ipe_zoom.windowHeight,
                pEIS2Input->ipe_zoom.windowLeft,
                pEIS2Input->ipe_zoom.windowTop);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::PrintEIS2OutputParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::PrintEIS2OutputParams(
    is_output_type* pAlgoResult)
{
    LOG_VERBOSE(CamxLogGroupChi, "EISv2 lib result for request %d", pAlgoResult->frame_id);
    LOG_VERBOSE(CamxLogGroupChi, "Has Output %d", pAlgoResult->has_output);
    if (TRUE == pAlgoResult->has_output)
    {
        LOG_VERBOSE(CamxLogGroupChi, "Transform matrix enabled: %d", pAlgoResult->stabilizationTransform.matrices.enable);
        LOG_VERBOSE(CamxLogGroupChi, "Num of rows: %d", pAlgoResult->stabilizationTransform.matrices.numRows);
        LOG_VERBOSE(CamxLogGroupChi, "Num of Columns: %d", pAlgoResult->stabilizationTransform.matrices.numColumns);
        LOG_VERBOSE(CamxLogGroupChi, "Perspective matrix: %.15lf %.15lf %.15lf %.15lf %.15lf %.15lf %.15lf %.15lf %.15lf",
                    pAlgoResult->stabilizationTransform.matrices.perspMatrices[0].T[0],
                    pAlgoResult->stabilizationTransform.matrices.perspMatrices[0].T[1],
                    pAlgoResult->stabilizationTransform.matrices.perspMatrices[0].T[2],
                    pAlgoResult->stabilizationTransform.matrices.perspMatrices[0].T[3],
                    pAlgoResult->stabilizationTransform.matrices.perspMatrices[0].T[4],
                    pAlgoResult->stabilizationTransform.matrices.perspMatrices[0].T[5],
                    pAlgoResult->stabilizationTransform.matrices.perspMatrices[0].T[6],
                    pAlgoResult->stabilizationTransform.matrices.perspMatrices[0].T[7],
                    pAlgoResult->stabilizationTransform.matrices.perspMatrices[0].T[8]);

        LOG_VERBOSE(CamxLogGroupChi, "Transform grid enabled: %d", pAlgoResult->stabilizationTransform.grid.enable);
        LOG_VERBOSE(CamxLogGroupChi, "Grid rows: %d", pAlgoResult->stabilizationTransform.grid.numRows);
        LOG_VERBOSE(CamxLogGroupChi, "Grid columns: %d", pAlgoResult->stabilizationTransform.grid.numColumns);
        LOG_VERBOSE(CamxLogGroupChi, "Grid transform defined on width: %d",
                    pAlgoResult->stabilizationTransform.grid.transformDefinedOn.widthPixels);
        LOG_VERBOSE(CamxLogGroupChi, "Grid transform defined on height: %d",
                    pAlgoResult->stabilizationTransform.grid.transformDefinedOn.heightLines);
        LOG_VERBOSE(CamxLogGroupChi, "Grid exterpolate type: %d", pAlgoResult->stabilizationTransform.grid.extrapolateType);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::WriteGyroDataToFile
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::WriteGyroDataToFile(
    is_input_t* pEISInput)
{
    CHAR   dumpData[GyroDumpSize] = { 0 };
    CHAR*  pos                    = &dumpData[0];
    UINT32 charWritten            = 0;

    charWritten = ChiNodeUtils::SNPrintF(pos, sizeof(dumpData),
                                         "# %u %u %u %u %.15f %llu %.2e %u %u %u %u %d %d %u %u %u %u %d %d\n",
                                         pEISInput->frame_id,
                                         pEISInput->gyro_data.num_elements,
                                         pEISInput->frame_times.frame_time,
                                         pEISInput->sensor_rolling_shutter_skew,
                                         pEISInput->frame_times.exposure_time,
                                         pEISInput->frame_times.sof,
                                         pEISInput->focus_distance,
                                         pEISInput->ife_crop.fullWidth,
                                         pEISInput->ife_crop.fullHeight,
                                         pEISInput->ife_crop.windowWidth,
                                         pEISInput->ife_crop.windowHeight,
                                         pEISInput->ife_crop.windowLeft,
                                         pEISInput->ife_crop.windowTop,
                                         pEISInput->ipe_zoom.fullWidth,
                                         pEISInput->ipe_zoom.fullHeight,
                                         pEISInput->ipe_zoom.windowWidth,
                                         pEISInput->ipe_zoom.windowHeight,
                                         pEISInput->ipe_zoom.windowLeft,
                                         pEISInput->ipe_zoom.windowTop);

    pos += charWritten;

    for (UINT32 gyro_id = 0; gyro_id < pEISInput->gyro_data.num_elements; gyro_id++)
    {
        if (pos >= dumpData + sizeof(dumpData))
        {
            pos = dumpData + sizeof(dumpData) - 1;
            break;
        }
        charWritten = ChiNodeUtils::SNPrintF(pos, sizeof(dumpData) - (pos - dumpData),
                                             "%d %llu %.22lf %.22lf %.22lf\n",
                                             gyro_id,
                                             pEISInput->gyro_data.gyro_data[gyro_id].ts,
                                             pEISInput->gyro_data.gyro_data[gyro_id].data[0],
                                             pEISInput->gyro_data.gyro_data[gyro_id].data[1],
                                             pEISInput->gyro_data.gyro_data[gyro_id].data[2]);

        pos += charWritten;
    }

    SIZE_T objCount = ChiNodeUtils::FWrite(dumpData, pos - dumpData, 1, m_hGyroDumpFile);

    if (1 != objCount)
    {
        LOG_ERROR(CamxLogGroupChi, "File write failed !");
    }
    else
    {
        LOG_VERBOSE(CamxLogGroupChi, "%zd byte successfully written to file", pos - dumpData);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::WriteEISVOutputMatricesToFile
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::WriteEISVOutputMatricesToFile(
    const is_output_type* pEIS3Output,
    FILE*                 pFileHandle)
{
    CHAR dumpData[PerspectiveDumpSize] = { 0 };
    CHAR*  pos                         = &dumpData[0];
    UINT32 charWritten                 = 0;
    UINT32 row                         = 0;
    UINT32 col                         = 0;

    if (NULL == pFileHandle)
    {
        return;
    }

    if (pEIS3Output->stabilizationTransform.matrices.enable == false)
    {
        return; // nothing to do
    }

    memset(dumpData, 0x00, sizeof(dumpData));

    for (row = 0; row < pEIS3Output->stabilizationTransform.matrices.numRows; row++)
    {
        for (col = 0; col < pEIS3Output->stabilizationTransform.matrices.numColumns; col++)
        {
            const UINT32 idx = row * pEIS3Output->stabilizationTransform.matrices.numColumns + col;
            charWritten = ChiNodeUtils::SNPrintF(pos,
                                                 sizeof(dumpData),
                                                 "%u, %u, %u, %u, %u, %.15lf, %.15lf, %.15lf, %.15lf, "
                                                 "%.15lf, %.15lf, %.15lf, %.15lf, %.15lf\n",
                                                 static_cast<UINT8>(pEIS3Output->has_output),
                                                 pEIS3Output->frame_id,
                                                 pEIS3Output->active_sensor_idx,
                                                 pEIS3Output->stabilizationTransform.matrices.numRows,
                                                 pEIS3Output->stabilizationTransform.matrices.numColumns,
                                                 pEIS3Output->stabilizationTransform.matrices.perspMatrices[idx].T[0],
                                                 pEIS3Output->stabilizationTransform.matrices.perspMatrices[idx].T[1],
                                                 pEIS3Output->stabilizationTransform.matrices.perspMatrices[idx].T[2],
                                                 pEIS3Output->stabilizationTransform.matrices.perspMatrices[idx].T[3],
                                                 pEIS3Output->stabilizationTransform.matrices.perspMatrices[idx].T[4],
                                                 pEIS3Output->stabilizationTransform.matrices.perspMatrices[idx].T[5],
                                                 pEIS3Output->stabilizationTransform.matrices.perspMatrices[idx].T[6],
                                                 pEIS3Output->stabilizationTransform.matrices.perspMatrices[idx].T[7],
                                                 pEIS3Output->stabilizationTransform.matrices.perspMatrices[idx].T[8]);
            pos += charWritten;
        }
    }

    SIZE_T objCount = ChiNodeUtils::FWrite(dumpData, pos - dumpData, 1, pFileHandle);

    if (1 != objCount)
    {
        LOG_ERROR(CamxLogGroupChi, "File write failed !");
    }
    else
    {
        LOG_VERBOSE(CamxLogGroupChi, "%zd byte successfully written to file", pos - dumpData);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::WriteEISVOutputGridsToFile
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::WriteEISVOutputGridsToFile(
    is_output_type* pEIS3Output)
{
    CHAR dumpData[GridsDumpSize]    = { 0 };
    CHAR*  pos                      = &dumpData[0];
    UINT32 charWritten              = 0;
    const NcLibWarpGrid* grid       = &pEIS3Output->stabilizationTransform.grid;

    if (grid->enable == false)
    {
        return; // nothing to do
    }

    memset(dumpData, 0x00, sizeof(dumpData));

    charWritten = ChiNodeUtils::SNPrintF(pos,
                                         sizeof(dumpData),
                                         "%u, %u, %u, %u, %u, %u, %u, ",
                                         pEIS3Output->frame_id,
                                         (uint32_t)grid->enable,
                                         grid->transformDefinedOn.widthPixels,
                                         grid->transformDefinedOn.heightLines,
                                         grid->numRows,
                                         grid->numColumns,
                                         (uint32_t)grid->extrapolateType);
    pos += charWritten;

    for (uint32_t row = 0; row < grid->numRows; row++)
    {
        for (uint32_t col = 0; col < grid->numColumns; col++)
        {
            charWritten = ChiNodeUtils::SNPrintF(pos,
                                                 sizeof(dumpData),
                                                 "%4.4lf, %4.4lf",
                                                 grid->grid[row*grid->numColumns + col].x,
                                                 grid->grid[row*grid->numColumns + col].y);

            pos += charWritten;

            if ((row != (grid->numRows - 1)) || (col != (grid->numColumns - 1))) /* not the last element */
            {
                charWritten = ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), ", ");
            }
            else
            {
                charWritten = ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "\n");
            }

            pos += charWritten;
        }
    }

    SIZE_T objCount = ChiNodeUtils::FWrite(dumpData, pos - dumpData, 1, m_hEisOutputGridsDumpFile);

    if (1 != objCount)
    {
        LOG_ERROR(CamxLogGroupChi, "File write failed !");
    }
    else
    {
        LOG_VERBOSE(CamxLogGroupChi, "%zd byte successfully written to file", pos - dumpData);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::WriteInitDataToFile
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ChiEISV2Node::WriteInitDataToFile(
    const is_init_data_common*  pISInitDataCommon,
    const is_init_data_sensor*  pISInitDataSensors,
    uint32_t                    numSensors,
    FILE*                       fileHandle)
{
    if (NULL == fileHandle)
    {
        return;
    }

    CHAR   dumpData[InitDumpSize] = { 0 };
    CHAR*  pos = &dumpData[0];
    SIZE_T objCount;

    // Save common configuration to file
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "; =========== common configuration ===========\n\n");
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "[common]\n");

    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "is_type = %u\n", (uint32_t)pISInitDataCommon->is_type);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "operation_mode = %u\n",
                                 (uint32_t)pISInitDataCommon->operation_mode);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "output_width = %u\n",
                                  pISInitDataCommon->is_output_frame_width);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "output_height = %u\n",
                                  pISInitDataCommon->is_output_frame_height);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "frame_fps = %u\n",
                                 (uint32_t)pISInitDataCommon->frame_fps);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "is_warping_using_ica = %u\n",
                                 (uint32_t)pISInitDataCommon->is_warping_using_ica);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "do_virtual_upscale_in_matrix = %u\n",
                                 (uint32_t)pISInitDataCommon->do_virtual_upscale_in_matrix);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "is_calc_transform_alignment_matrix_orig = %u\n",
                                 (uint32_t)pISInitDataCommon->is_calc_transform_alignment_matrix_orig);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "is_calc_transform_alignment_matrix_mctf = %u\n",
                                 (uint32_t)pISInitDataCommon->is_calc_transform_alignment_matrix_mctf);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "is_sat_applied_in_ica = %u\n",
                                 (uint32_t)pISInitDataCommon->is_sat_applied_in_ica);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "is_ois_enabled = %u\n",
                                 (uint32_t)pISInitDataCommon->is_ois_enabled);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "buffer_delay = %u\n",
                                 (uint32_t)pISInitDataCommon->buffer_delay);
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "num_h_slices = %u\n",
                                 (uint32_t)pISInitDataCommon->num_mesh_y);

    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "\n\n[sensor_data]\n");
    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "num_sensors = %u\n", numSensors);

    objCount = ChiNodeUtils::FWrite(dumpData, pos - dumpData, 1, fileHandle);
    if (1 != objCount)
    {
        LOG_ERROR(CamxLogGroupChi, "File write failed !");
    }
    else
    {
        LOG_VERBOSE(CamxLogGroupChi, "%zd byte successfully written to file", pos - dumpData);
    }

    // Save per sensor configuration to file
    for (uint32_t idx = 0; idx < numSensors; idx++)
    {
        pos = &dumpData[0];

        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "\n; =========== sensor #%u configuration ===========n\n", idx);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "[sensor_data\\idx%u]\n", idx);

        if ((NULL != pISInitDataSensors[idx].ldc_in2out) && (true == pISInitDataSensors[idx].ldc_in2out->enable))
        {
            const uint32_t numGridPoints = pISInitDataSensors[idx].ldc_in2out->numRows *
                                           pISInitDataSensors[idx].ldc_in2out->numColumns;

            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_in2out_width = %u\n",
                                          pISInitDataSensors[idx].ldc_in2out->transformDefinedOn.widthPixels);
            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_in2out_height = %u\n",
                                          pISInitDataSensors[idx].ldc_in2out->transformDefinedOn.heightLines);
            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_in2out_num_rows = %u\n",
                                          pISInitDataSensors[idx].ldc_in2out->numRows);
            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_in2out_num_cols = %u\n",
                                          pISInitDataSensors[idx].ldc_in2out->numColumns);
            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_in2out_extrapolate_type = %u\n",
                                         (uint32_t)pISInitDataSensors[idx].ldc_in2out->extrapolateType);

            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_in2out_grid_x = ");
            for (uint32_t gridIdx = 0; gridIdx < numGridPoints; gridIdx++)
            {
                pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "%4.4lf",
                                              pISInitDataSensors[idx].ldc_in2out->grid[gridIdx].x);
                if (gridIdx < numGridPoints - 1)
                {
                    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), ", ");
                }
                else
                {
                    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "\n");
                }
            }

            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_in2out_grid_y = ");
            for (uint32_t gridIdx = 0; gridIdx < numGridPoints; gridIdx++)
            {
                pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "%4.4lf",
                                              pISInitDataSensors[idx].ldc_in2out->grid[gridIdx].y);
                if (gridIdx < numGridPoints - 1)
                {
                    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), ", ");
                }
                else
                {
                    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "\n");
                }
            }

            objCount = ChiNodeUtils::FWrite(dumpData, pos - dumpData, 1, fileHandle);
            if (1 != objCount)
            {
                LOG_ERROR(CamxLogGroupChi, "File write failed !");
            }
            else
            {
                LOG_VERBOSE(CamxLogGroupChi, "%zd byte successfully written to file", pos - dumpData);
            }
            pos = &dumpData[0];
        }

        if ((NULL != pISInitDataSensors[idx].ldc_out2in) && (true == pISInitDataSensors[idx].ldc_out2in->enable))
        {
            const uint32_t numGridPoints = pISInitDataSensors[idx].ldc_out2in->numRows *
                                           pISInitDataSensors[idx].ldc_out2in->numColumns;

            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_out2in_width = %u\n",
                                          pISInitDataSensors[idx].ldc_out2in->transformDefinedOn.widthPixels);
            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_out2in_height = %u\n",
                                          pISInitDataSensors[idx].ldc_out2in->transformDefinedOn.heightLines);
            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_out2in_num_rows = %u\n",
                                          pISInitDataSensors[idx].ldc_out2in->numRows);
            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_out2in_num_cols = %u\n",
                                          pISInitDataSensors[idx].ldc_out2in->numColumns);
            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_out2in_extrapolate_type = %u\n",
                                         (uint32_t)pISInitDataSensors[idx].ldc_out2in->extrapolateType);

            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_out2in_grid_x = ");
            for (uint32_t gridIdx = 0; gridIdx < numGridPoints; gridIdx++)
            {
                pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "%4.4lf",
                                              pISInitDataSensors[idx].ldc_out2in->grid[gridIdx].x);
                if (gridIdx < numGridPoints - 1)
                {
                    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), ", ");
                }
                else
                {
                    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "\n");
                }
            }

            pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "ldc_out2in_grid_y = ");
            for (uint32_t gridIdx = 0; gridIdx < numGridPoints; gridIdx++)
            {
                pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "%4.4lf",
                                              pISInitDataSensors[idx].ldc_out2in->grid[gridIdx].y);
                if (gridIdx < numGridPoints - 1)
                {
                    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), ", ");
                }
                else
                {
                    pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "\n");
                }
            }

            objCount = ChiNodeUtils::FWrite(dumpData, pos - dumpData, 1, fileHandle);
            if (1 != objCount)
            {
                LOG_ERROR(CamxLogGroupChi, "File write failed !");
            }
            else
            {
                LOG_VERBOSE(CamxLogGroupChi, "%zd byte successfully written to file", pos - dumpData);
            }
            pos = &dumpData[0];
        }

        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "input_width = %u\n",
                                      pISInitDataSensors[idx].is_input_frame_width);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "input_height = %u\n",
                                      pISInitDataSensors[idx].is_input_frame_height);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "sensor_mount_angle = %u\n",
                                      pISInitDataSensors[idx].sensor_mount_angle);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "camera_position = %u\n",
                                     (uint32_t)pISInitDataSensors[idx].camera_position);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "optical_center_x = %lf\n",
                                      pISInitDataSensors[idx].optical_center_x);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "optical_center_y = %lf\n",
                                      pISInitDataSensors[idx].optical_center_y);

        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "minimal_total_margin_x = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.general.minimal_total_margin_x);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "minimal_total_margin_y = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.general.minimal_total_margin_y);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "focal_length = %u\n",
                                      pISInitDataSensors[idx].is_chromatix_info.general.focal_length);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "gyro_frequency = %u\n",
                                      pISInitDataSensors[idx].is_chromatix_info.general.gyro_frequency);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "gyro_noise_floor = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.general.gyro_noise_floor);

        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "s3d_offset_1 = %d\n",
                                      pISInitDataSensors[idx].is_chromatix_info.timing.s3d_offset_1);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "s3d_offset_2 = %d\n",
                                      pISInitDataSensors[idx].is_chromatix_info.timing.s3d_offset_2);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "s3d_offset_3 = %d\n",
                                      pISInitDataSensors[idx].is_chromatix_info.timing.s3d_offset_3);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "s3d_offset_4 = %d\n",
                                      pISInitDataSensors[idx].is_chromatix_info.timing.s3d_offset_4);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "s3d_threshold_1 = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.timing.s3d_threshold_1);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "s3d_threshold_2 = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.timing.s3d_threshold_2);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "s3d_threshold_3 = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.timing.s3d_threshold_3);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "s3d_threshold_4 = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.timing.s3d_threshold_4_ext);

        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "blur_masking_enable = %u\n",
                                      pISInitDataSensors[idx].is_chromatix_info.blur_masking.enable);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "blur_masking_exposure_time_th = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.blur_masking.exposure_time_th);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "blur_masking_start_decrease_at_blur = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.blur_masking.start_decrease_at_blur);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "blur_masking_end_decrease_at_blur = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.blur_masking.end_decrease_at_blur);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "blur_masking_min_strength = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.blur_masking.min_strength);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "blur_masking_res1 = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.blur_masking.blur_masking_res1);
        pos += ChiNodeUtils::SNPrintF(pos, sizeof(dumpData), "blur_masking_res2 = %lf\n",
                                      pISInitDataSensors[idx].is_chromatix_info.blur_masking.blur_masking_res2);

        objCount = ChiNodeUtils::FWrite(dumpData, pos - dumpData, 1, fileHandle);
        if (1 != objCount)
        {
            LOG_ERROR(CamxLogGroupChi, "File write failed !");
        }
        else
        {
            LOG_VERBOSE(CamxLogGroupChi, "%zd byte successfully written to file", pos - dumpData);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ChiEISV2Node::ConvertICA20GridToICA10Grid
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDKResult ChiEISV2Node::ConvertICA20GridToICA10Grid(
    NcLibWarpGrid *pInICA20Grid,
    NcLibWarpGrid *pOutICA10Grid)
{
    CDKResult result = CDKResultSuccess;

    if ((NULL == pInICA20Grid) || (NULL == pOutICA10Grid))
    {
        LOG_ERROR(CamxLogGroupChi, "Grid conversion failed %p, %p", pInICA20Grid, pOutICA10Grid);
        result = CDKResultEInvalidPointer;
    }

    if ((CDKResultSuccess == result) && (TRUE == pInICA20Grid->enable))
    {
        if ((NULL == pInICA20Grid->grid) ||
            (NULL == pOutICA10Grid->grid) ||
            (ICA20GridTransformHeight != pInICA20Grid->numRows) ||
            (ICA20GridTransformWidth != pInICA20Grid->numColumns) ||
            (EXTRAPOLATION_TYPE_EXTRA_POINT_ALONG_PERIMETER != pInICA20Grid->extrapolateType))
        {
            LOG_ERROR(CamxLogGroupChi, "Grid conversion invalid input, ICA20 grid %p, ICA10 Grid %p, rows %d, columns %d, %d",
                      pInICA20Grid->grid,
                      pOutICA10Grid->grid,
                      pInICA20Grid->numRows,
                      pInICA20Grid->numColumns,
                      pInICA20Grid->extrapolateType);
            result = CDKResultEInvalidArg;
        }

        if (CDKResultSuccess == result)
        {
            pOutICA10Grid->enable               = pInICA20Grid->enable;
            pOutICA10Grid->transformDefinedOn   = pInICA20Grid->transformDefinedOn;
            pOutICA10Grid->numRows              = ICA10GridTransformHeight;
            pOutICA10Grid->numColumns           = ICA10GridTransformWidth;

            if (NULL != pOutICA10Grid->gridExtrapolate)
            {
                pOutICA10Grid->extrapolateType = EXTRAPOLATION_TYPE_FOUR_CORNERS;

                // Top-left corner
                pOutICA10Grid->gridExtrapolate[0] = pInICA20Grid->grid[0];

                // Top-right corner
                pOutICA10Grid->gridExtrapolate[1] = pInICA20Grid->grid[ICA20GridTransformWidth * 1 - 1];

                // Bottom-left corner
                pOutICA10Grid->gridExtrapolate[2] = pInICA20Grid->grid[ICA20GridTransformWidth * (ICA20GridTransformHeight - 1)];

                // Bottom-right corner
                pOutICA10Grid->gridExtrapolate[3] = pInICA20Grid->grid[ICA20GridTransformWidth * ICA20GridTransformHeight - 1];
            }
            else
            {
                pOutICA10Grid->extrapolateType = EXTRAPOLATION_TYPE_NONE;
            }

            UINT32 indexICA10 = 0;
            UINT32 indexICA20 = ICA20GridTransformWidth + 1;

            for (UINT32 row = 0; row < ICA10GridTransformHeight; row++)
            {
                for (UINT32 column = 0; column < ICA10GridTransformWidth; column++)
                {
                    // optimized indexing
                    pOutICA10Grid->grid[indexICA10] = pInICA20Grid->grid[indexICA20];
                    indexICA10++;
                    indexICA20++;
                }
                indexICA20 += 2;
            }
        }
    }

    return result;
}

