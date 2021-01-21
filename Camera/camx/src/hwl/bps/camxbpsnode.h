////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsnode.h
/// @brief BPSNode class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXBPSNODE_H
#define CAMXBPSNODE_H

#include "bps_data.h"
#include "bpsStripingLib.h"
#include "camxmem.h"
#include "camxnode.h"
#include "camxcmdbuffermanager.h"
#include "camxhwcontext.h"
#include "camxispiqmodule.h"
/// @todo (CAMX-1297) Have common implementation for stats and IQ modules
#include "camxispstatsmodule.h"
#include "chipinforeaderdefs.h"
#include "titan170_bps.h"
#include "camxcslicpdefs.h"
#include "camxtitan17xcontext.h"

CAMX_NAMESPACE_BEGIN

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_INPUT_IMAGE) ==
    static_cast<UINT32>(CSLBPSInputPortId::CSLBPSInputPortImage));

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_FULL) ==
    static_cast<UINT32>(CSLBPSOutputPortId::CSLBPSOutputPortIdPDIImageFull));

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_DS4) ==
    static_cast<UINT32>(CSLBPSOutputPortId::CSLBPSOutputPortIdPDIImageDS4));

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_DS16) ==
    static_cast<UINT32>(CSLBPSOutputPortId::CSLBPSOutputPortIdPDIImageDS16));

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_DS64) ==
    static_cast<UINT32>(CSLBPSOutputPortId::CSLBPSOutputPortIdPDIImageDS64));

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_STATS_BG) ==
    static_cast<UINT32>(CSLBPSOutputPortId::CSLBPSOutputPortIdStatsBG));

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_STATS_BHIST) ==
    static_cast<UINT32>(CSLBPSOutputPortId::CSLBPSOutputPortIdStatsHDRBHIST));

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_REG1) ==
    static_cast<UINT32>(CSLBPSOutputPortId::CSLBPSOutputPortIdRegistrationImage1));

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_OUTPUT_IMAGE_REG2) ==
    static_cast<UINT32>(CSLBPSOutputPortId::CSLBPSOutputPortIdRegistrationImage2));

CAMX_STATIC_ASSERT(static_cast<UINT32>(BPS_IO_IMAGES::BPS_IO_IMAGES_MAX) ==
    static_cast<UINT32>(CSLBPSOutputPortId::CSLBPSOutputPortIdMaxNumPortResources));

static UINT32 BPSMetadataTags[] =
{
    InputBlackLevelLock,
    InputColorCorrectionGains,
    InputColorCorrectionMode,
    InputColorCorrectionTransform,
    InputControlAEMode,
    InputControlAWBMode,
    InputControlMode,
    InputControlPostRawSensitivityBoost,
    InputHotPixelMode,
    InputNoiseReductionMode,
    InputShadingMode,
    InputStatisticsHotPixelMapMode,
    InputStatisticsLensShadingMapMode,
    InputTonemapMode,
};
static const UINT NumBPSMetadataTags = sizeof(BPSMetadataTags) / sizeof(UINT32);   ///< Number of vendor tags
// NOWHINE NC003a: We don't really want to name this g_
static UINT64 BPSMetadataTagReqOffset[NumBPSMetadataTags] = { 0 };

static const UINT BPSMetadataOutputTags[] =
{
    BlackLevelLock,
    ColorCorrectionGains,
    ControlPostRawSensitivityBoost,
    NoiseReductionMode,
    HotPixelMode,
    ShadingMode,
    StatisticsLensShadingMapMode,
    LensInfoShadingMapSize,
    StatisticsLensShadingMap,
    SensorDynamicBlackLevel,
    BlackLevelLock,
    StatisticsHotPixelMapMode,
    TonemapMode,
    PropertyIDIPEGamma15PreCalculation,
};
static const UINT NumBPSMetadataOutputTags = CAMX_ARRAY_SIZE(BPSMetadataOutputTags);   ///< Number of output vendor tags

static const CHAR* BPSPortName[] =
{
    "RDI",              // 0 - Input
    "Full",             // 1 - Output
    "DS4",              // 2 - Output
    "DS16",             // 3 - Output
    "DS64",             // 4 - Output
    "StatsBG",          // 5 - Output
    "StatsHDRBHist",    // 6 - Output
    "Reg1",             // 7 - Output
    "Reg2",             // 8 - Output
    "Unknown",          // 9 - Unknown
};

static const UINT BPSPortNameMaxIndex = CAMX_ARRAY_SIZE(BPSPortName) - 1; ///< Number of strings in BPSPortName array

/// @brief BPS IQ Module Information
struct BPSIQModuleInfo
{
    ISPIQModuleType   moduleType;                                      ///< IQ Module Type
    BOOL              isEnabled;                                       ///< Module Available or not
    CamxResult        (*IQCreate)(BPSModuleCreateData* pInputData);    ///< Static create fuction of this IQ Module
};

/// @brief BPS Stats Module Information
struct BPSStatsModuleInfo
{
    /// @todo (CAMX-1297) Have common implementation for stats and IQ modules
    ISPStatsModuleType moduleType;                                    ///< Stats Module Name
    BOOL               isEnabled;                                     ///< Module Available or not
    CamxResult(*StatsCreate)(BPSStatsModuleCreateData* pInputData);   ///< Static create fuction of this Stats Module
};

/// @brief BPS Capability Information
struct BPSCapabilityInfo
{
    UINT                   numBPSIQModules;        ///< Number of BPS IQ Modules
    BPSIQModuleInfo*       pBPSIQModuleList;       ///< List of BPS IQ Module
    UINT                   numBPSStatsModules;     ///< Number of Stats Modules
    BPSStatsModuleInfo*    pBPSStatsModuleList;    ///< List of Stats Module
    BOOL                   swStriping;             ///< Striping done in UMD
};

/// @brief Instance Profile Id
enum BPSProfileId
{
    BPSProfileIdDefault = 0,    ///< All IQ Modules
    BPSProfileIdPrefilter,      ///< Pre Filter
    BPSProfileIdBlend,          ///< Blend
    BPSProfileIdBlendPost,      ///< BlendPost
    BPSProfileIdHNR,            ///< HNR Only
    BPSProfileIdIdealRawOutput, ///< Ideal raw Output
    BPSProfileIdIdealRawInput,  ///< Ideal raw Input
    BPSProfileIdMax,            ///< Invalid
};

/// @brief Instance Processing Type
enum BPSProcessingType
{
    BPSProcessingDefault = 0, ///< Default BPS
    BPSProcessingMFNR,        ///< MFNR
    BPSProcessingMFSR,        ///< MFSR
    BPSProcessingMax,         ///< Invalid

};

/// @brief BPS Vendor Tag Id
enum BPSVendorTagId
{
    BPSVendorTagAECStats = 0,       ///< Vendor Tag for AEC stats
    BPSVendorTagAWBStats,           ///< Vendor Tag for AWB stats
    BPSVendorTagAFStats,            ///< Vendor Tag for AF stats
    BPSVendorTagAECFrame,           ///< Vendor Tag for AEC frame
    BPSVendorTagAWBFrame,           ///< Vendor Tag for AWB frame
    BPSVendorTagMax                 ///< Max number of Vendor Tag
};

/// @brief BPS IQ Instance Property Information
struct BPSIntanceProperty
{
    BPSProfileId            profileId;          ///< HNR
    BPSProcessingType       processingType;     ///< MFNR, MFSR
};
/// @brief BPS Image Information
struct BPSImageInfo
{
    UINT  width;               ///< Width of the image
    UINT  height;              ///< Height of the image
    FLOAT bpp;                 ///< bpp
    BOOL  UBWCEnable;          ///< UBWC Flag
};

/// @brief BPS Bandwidth Information
struct BPSBandwidthSource
{
    UINT64 compressedBW;        ///< compressed bandwidth
    UINT64 unCompressedBW;      ///< unCompressed bandwidth
};

/// @brief BPS Total Bandwidth
struct BPSBandwidth
{
    BPSBandwidthSource readBW;            ///< read Bandwidth
    BPSBandwidthSource writeBW;           ///< write Bandwidth
    UINT               FPS;               ///< FPS
    FLOAT              partialBW;         ///< partial Bandwidth
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class that implements the BPS node class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class BPSNode final : public Node
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief     Static method to create BPSNode Object.
    ///
    /// @param     pCreateInputData  Node create input data
    /// @param     pCreateOutputData Node create output data
    ///
    /// @return    Pointer to the concrete BPSNode object
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BPSNode* Create(
        const NodeCreateInputData* pCreateInputData,
        NodeCreateOutputData*      pCreateOutputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetNumberOfPixels
    ///
    /// @brief  Get the number of Bits per pixels in RAW image.
    ///
    /// @param  pFormat The image format.
    ///
    /// @Return The number of Bits per pixels in RAW image
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline UINT GetNumberOfPixels(
    const ImageFormat* pFormat)
    {
        UINT bitsPerPixel = 0;

        CAMX_ASSERT(NULL != pFormat);

        if (NULL != pFormat)
        {
            bitsPerPixel = pFormat->formatParams.rawFormat.bitsPerPixel;
        }

        return bitsPerPixel;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Destroy
    ///
    /// @brief     This method destroys the derived instance of the interface
    ///
    /// @return    None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID Destroy();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ProcessingNodeInitialize
    ///
    /// @brief     Initialize the hwl object
    ///
    /// @param     pCreateInputData     Node create input data
    /// @param     pCreateOutputData    Node create output data
    ///
    /// @return    CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ProcessingNodeInitialize(
        const NodeCreateInputData* pCreateInputData,
        NodeCreateOutputData*      pCreateOutputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PostPipelineCreate
    ///
    /// @brief     virtual method to be called at NotifyTopologyCreated time; node should take care of updates and initialize
    ///            blocks that has dependency on other nodes in the topology at this time.
    ///
    /// @return    CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult PostPipelineCreate();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ExecuteProcessRequest
    ///
    /// @brief     Pure virtual method to trigger process request for the hwl node object.
    ///
    /// @param     pExecuteProcessRequestData Process request data
    ///
    /// @return    CamxResultSuccess if successful and 0 dependencies, dependency information otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ExecuteProcessRequest(
        ExecuteProcessRequestData* pExecuteProcessRequestData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetInputPortName
    ///
    /// @brief  Get input port name for the given port id.
    ///
    /// @param  portId  Port Id for which name is required
    ///
    /// @return Pointer to Port name string
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual const CHAR* GetInputPortName(
        UINT portId) const
    {
        if (portId > BPSPortNameMaxIndex)
        {
            portId = BPSPortNameMaxIndex;
        }

        return BPSPortName[portId];
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetOutputPortName
    ///
    /// @brief  Get output port name for the given port id.
    ///
    /// @param  portId  Port Id for which name is required
    ///
    /// @return Pointer to Port name string
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual const CHAR* GetOutputPortName(
        UINT portId) const
    {
        if (portId > BPSPortNameMaxIndex)
        {
            portId = BPSPortNameMaxIndex;
        }

        return BPSPortName[portId];
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSNode
    ///
    /// @brief     Constructor
    ///
    /// @return    None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BPSNode();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~BPSNode
    ///
    /// @brief     Destructor
    ///
    /// @return    None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~BPSNode();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckToUseHardcodedRegValues
    ///
    /// @brief  Check and [en|dis]able the use of hardcoded register values
    ///
    /// @param  pHwContext Pointer to the ISP HW context
    ///
    /// @return TRUE/FALSE   TRUE -> Enabled, FALSE -> Disabled
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL CheckToUseHardcodedRegValues(
        HwContext* pHwContext)
    {
        Titan17xContext* pContext = static_cast<Titan17xContext*>(pHwContext);
        CAMX_ASSERT(NULL != pContext);

        return pContext->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->BPSUseHardcodedRegisterValues;
    }

private:
    BPSNode(const BPSNode&) = delete;                 ///< Disallow the copy constructor.
    BPSNode& operator=(const BPSNode&) = delete;      ///< Disallow assignment operator.

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// TranslateToFirmwarePortId
    ///
    /// @brief     Helper method to find the firmware port id
    ///
    /// @param     portId             Input port id
    /// @param     pFirmwarePortId    pointer holding the firmware port id
    ///
    /// @return    None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID TranslateToFirmwarePortId(
        UINT32         portId,
        BPS_IO_IMAGES* pFirmwarePortId);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AcquireDevice
    ///
    /// @brief     Helper method to acquire BPS device
    ///
    /// @return    CamxResultSuccess
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult AcquireDevice();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ReleaseDevice
    ///
    /// @brief     Helper method to release BPS device
    ///
    /// @return    CamxResultSuccess
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ReleaseDevice();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureBPSCapability
    ///
    /// @brief     Set up BPS capability based on BPS revision number
    ///
    /// @return    CamxResultSuccess
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ConfigureBPSCapability();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateBPSIQModules
    ///
    /// @brief     Create IQ Modules of the BPS Block
    ///
    /// @return    CamxResultSuccess
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateBPSIQModules();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetBPSSWTMCModuleInstanceVersion
    ///
    /// @brief  Helper function to get BPS SWTMC installed for the current IFE node
    ///
    /// @return Version of SW TMC module
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    SWTMCVersion GetBPSSWTMCModuleInstanceVersion();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateBPSStatsModules
    ///
    /// @brief     Create Stats Modules of the BPS Block
    ///
    /// @return    CamxResultSuccess
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateBPSStatsModules();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PopulateGeneralTuningMetadata
    ///
    /// @brief  Helper to populate tuning data easily obtain at BPS node level
    ///
    /// @param  pInputData Pointer to the data input and output stuctures are inside
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PopulateGeneralTuningMetadata(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpTuningMetadata
    ///
    /// @brief  Helper to publish tuning metadata
    ///
    /// @param  pInputData Pointer to the input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpTuningMetadata(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ProgramIQConfig
    ///
    /// @brief     Reprogram the settings for the IQ Modules
    ///
    /// @param     pInputData Pointer to the input data
    ///
    /// @return    CamxResult True on successful execution else False on failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ProgramIQConfig(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ProgramIQModuleEnableConfig
    ///
    /// @brief     Reprogram the Module enable settings for the IQ Modules
    ///
    /// @param     pInputData    Pointer to the input data
    ///
    /// @return    CamxResult True on successful execution else False on failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ProgramIQModuleEnableConfig(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateCommandBufferSize
    ///
    /// @brief     Calculate Maximum Command Buffer size required for all the available Command Buffers / IQ Modules
    ///
    /// @return    None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateCommandBufferSize();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Cleanup
    ///
    /// @brief     Clean up the allocated IQ Modules
    ///
    /// @return    None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID Cleanup();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetStaticMetadata
    ///
    /// @brief  Get all static information from HAL metadata tags for all BPS modules
    ///
    /// @param  cameraId Camera Id for which the static metadata must be initialized
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID GetStaticMetadata(
        UINT32 cameraId);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// InitializeDefaultHALTags
    ///
    /// @brief  Initialized default HAL metadata tags for all BPS modules
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID InitializeDefaultHALTags();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetMetadataTags
    ///
    /// @brief  Get all information from HAL metadata tags for all IFE modules
    ///
    /// @param  pModuleInput Pointer to the input data
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult GetMetadataTags(
        ISPInputData* pModuleInput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PostMetadata
    ///
    /// @brief  Post BPS metadata to main metadata pool
    ///
    /// @param  pInputData Pointer to the input data
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult PostMetadata(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetOEMStatsConfig
    ///
    /// @brief  Returns OEM's custom stats configuration
    ///
    /// @param  pInputData    Pointer to the ISP input data for setting updates
    ///
    /// @return CamxResultSuccess if success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult GetOEMStatsConfig(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// NotifyRequestProcessingError
    ///
    /// @brief  Function that does any error processing required for a fence callback. When CSL notifies the Node of a fence we
    ///         post a job in the threadpool. And when threadpool calls us to process the error fence-signal notification, this
    ///         function does the necessary processing.
    ///
    /// @param  pFenceHandlerData     Pointer to struct FenceHandlerData that belongs to this node
    /// @param  unSignaledFenceCount  Number of un-signalled fence count
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID NotifyRequestProcessingError(
        NodeFenceHandlerData* pFenceHandlerData,
        UINT unSignaledFenceCount);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetDependencies
    ///
    /// @brief     Check the availability of the dependence data. Update the data if it is available
    ///
    /// @param     pExecuteProcessRequestData   Request specific data
    /// @param     sensorConnected              Sensor connected
    /// @param     statsConnected               Stats connected
    ///
    /// @return    None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SetDependencies(
        ExecuteProcessRequestData*  pExecuteProcessRequestData,
        BOOL                        sensorConnected,
        BOOL                        statsConnected);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// HardcodeSettings
    ///
    /// @brief  Set the hardcode input values until 3A is completly integrated
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID HardcodeSettings(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Set3AInputData
    ///
    /// @brief  Hook up the AEC/AWB settings for the IQ Modules
    ///
    /// @param  pInputData Pointer to the ISP input data for AAA setting updates
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID Set3AInputData(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetFaceROI
    ///
    /// @brief  Returns FD ROI information
    ///
    /// @param  pInputData Pointer to the input data
    ///
    /// @return CamxResultSuccess if success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult GetFaceROI(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetSensorModeData
    ///
    /// @brief  Get the Sensor mode for a requestId
    ///
    /// @param  sensorConnected sensor connected
    ///
    /// @return SensorMode      pointer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    const SensorMode* GetSensorModeData(
        BOOL sensorConnected);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetPDAFInformation
    ///
    /// @brief  Get sensor PDAF related data
    ///
    /// @param  pSensorPDAFInfo  Pointer to the PDAF information
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID GetPDAFInformation(
        SensorPDAFInfo* pSensorPDAFInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsSensorModeFormatBayer
    ///
    /// @brief  Indicate if the given Pixel format is a Bayer type or not.
    ///
    /// @param  format  PixelFormat format
    ///
    /// @return TRUE if the PixelFormat is a Bayer type
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL IsSensorModeFormatBayer(
        PixelFormat format);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsSensorModeFormatMono
    ///
    /// @brief  Indicate if the given Pixel format is a Mono type or not.
    ///
    /// @param  format  PixelFormat format
    ///
    /// @return TRUE if the PixelFormat is a Mono type
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL IsSensorModeFormatMono(
        PixelFormat format);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ReadSensorDigitalGain
    ///
    /// @brief  Get the Sensor digital gain for a requestId
    ///
    /// @param  sensorConnected sensorConnected
    ///
    /// @return digitalgain
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    FLOAT ReadSensorDigitalGain(
        BOOL sensorConnected);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetupDeviceResource
    ///
    /// @brief     Setup List of the Required Resource
    ///
    /// @param     pConfigIOMem    Pointer to config IO
    /// @param     pResource       Pointer to an arry of resource
    ///
    /// @return    Number of CSI resource that has been allocated
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult SetupDeviceResource(
        CSLBufferInfo*     pConfigIOMem,
        CSLDeviceResource* pResource);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillIQSetting
    ///
    /// @brief     API to Fill common IQ settings payload to firmware.
    ///
    /// @param     pInputData        Pointer to ISP Input Data
    /// @param     pBPSIQsettings    IQSettings payload for firmware
    ///
    /// @return    return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FillIQSetting(
        ISPInputData*  pInputData,
        BpsIQSettings* pBPSIQsettings);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillFrameBufferData
    ///
    /// @brief     Fill the frame data for all ports
    ///
    /// @param     pFrameProcessCmdBuffer    Pointer to top level firmware payload command buffer
    /// @param     pImageBuffer              Pointer to Image buffer
    /// @param     portId                    Port ID
    ///
    /// @return    return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FillFrameBufferData(
        CmdBuffer*   pFrameProcessCmdBuffer,
        ImageBuffer* pImageBuffer,
        UINT32       portId);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillFrameCDMArray
    ///
    /// @brief     Find the OutputChannel based on the port number
    ///
    /// @param     ppBPSCmdBuffer       Pointer array of all command buffers managed by BPS node
    /// @param     pFrameProcessData    Top level payload for firmware
    ///
    /// @return    return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FillFrameCDMArray(
        CmdBuffer**          ppBPSCmdBuffer,
        BpsFrameProcessData* pFrameProcessData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CommitAllCommandBuffers
    ///
    /// @brief     Find the OutputChannel based on the port number
    ///
    /// @param     ppBPSCmdBuffer    Pointer array of all command buffers managed by BPS node
    ///
    /// @return    return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CommitAllCommandBuffers(
        CmdBuffer** ppBPSCmdBuffer);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// InitializeStripingParams
    ///
    /// @brief     Initialize the striping library and create a command buffer
    ///
    /// @param     pConfigIOData    Bps IO configuration data for firmware
    ///
    /// @return    return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult InitializeStripingParams(
        BpsConfigIoData* pConfigIOData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DeInitializeStripingParams
    ///
    /// @brief     DeInitialize the striping library and delete command buffers
    ///
    /// @return    return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult DeInitializeStripingParams();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillStripingParams
    ///
    /// @brief     Get the striping params from striping library and fill the command buffer
    ///
    /// @param     pFrameProcessData          Top level payload for firmware
    /// @param     pBPSIQsettings             IQSettings payload for firmware
    /// @param     ppBPSCmdBuffer             Pointer array of all command buffers managed by BPS node
    /// @param     pICPClockBandwidthRequest  Pointer to Clock and Bandwidth request
    ///
    /// @return    return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FillStripingParams(
        BpsFrameProcessData*         pFrameProcessData,
        BpsIQSettings*               pBPSIQsettings,
        CmdBuffer**                  ppBPSCmdBuffer,
        CSLICPClockBandwidthRequest* pICPClockBandwidthRequest);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // InitializeBPSIQSettings
    ///
    /// @brief  Disable the IQ modules by accessing the moduleCfgEn bit in IQ modules
    ///
    /// @param  pBPSIQsettings                IQSettings payload for firmware
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID InitializeBPSIQSettings(
        BpsIQSettings*               pBPSIQsettings);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateLUTData
    ///
    /// @brief    function updates LUT information for use during CDM programming
    ///
    /// @param    pBPSIQSettings    IQ module settings information
    ///
    /// @return   None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void UpdateLUTData(
        BpsIQSettings* pBPSIQSettings);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // PatchBLMemoryBuffer
    ///
    /// @brief  patch the BL command buffer
    ///
    /// @param  pFrameProcessData             Top level payload for firmware
    /// @param  ppBPSCmdBuffer                Pointer to array of all command buffers managed by BPS node
    ///
    /// @return return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult PatchBLMemoryBuffer(
        BpsFrameProcessData* pFrameProcessData,
        CmdBuffer**          ppBPSCmdBuffer);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetIQModuleNumLUT
    ///
    /// @brief    function returns  number of LUT in IQ modules
    ///
    /// @param    type       IQ module type
    /// @param    numLUTs    number of LUTs
    ///
    /// @return   None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void SetIQModuleNumLUT(
        ISPIQModuleType type,
        UINT            numLUTs);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetIQModuleLUTOffset
    ///
    /// @brief    function sets the LUT offsets in IQ modules
    ///
    /// @param    type              IQ module type
    /// @param    pBPSIQSettings    Pointer to IQ IQ module settings information
    /// @param    LUTOffset         LUT offset
    /// @param    pLUTCount         Pointer to keep track of LUTs for IQ modules enabled
    ///
    /// @return   None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void SetIQModuleLUTOffset(
        ISPIQModuleType type,
        BpsIQSettings*  pBPSIQSettings,
        UINT            LUTOffset,
        UINT32*         pLUTCount);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // GetCDMProgramOffset
    ///
    /// @brief  Calculate the offset in bytes, of the CDMProgram from start of IpeFrameProcess
    ///
    /// @param  cdmProgramIndex CDM Program index (PreLTMCDMProgramOrder or PostLTMCDMProgramOrder)
    ///
    /// @return return Offset of CDM program from start of IpeFrameProcess, -1 for failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static INT GetCDMProgramOffset(
        UINT cdmProgramIndex);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillCDMProgram
    ///
    /// @brief  Fill the frame data for all ports, cosider batch size as well for HFR mode
    ///
    /// @param  ppBPSCmdBuffer      Pointer to array of command buffers for given frame
    /// @param  pCdmProgramArray    Pointer to array of CDMProgramArray
    /// @param  pCdmProgram         Pointer to CdmProgram to be configured
    /// @param  programType         Program type of CDM Program defined by firmware interface
    /// @param  programIndex        Program index which determines order of CdmProgram
    ///
    /// @return return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FillCDMProgram(
        CmdBuffer**      ppBPSCmdBuffer,
        CDMProgramArray* pCdmProgramArray,
        CdmProgram*      pCdmProgram,
        UINT32           programType,
        UINT32           programIndex);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ProcessingNodeFinalizeInputRequirement
    ///
    /// @brief  Virtual method implemented by BPS node to determine its input buffer requirements based on all the output
    ///         buffer requirements
    ///
    /// @param  pBufferNegotiationData  Negotiation data for all output ports of a node
    ///
    /// @return Success if the negotiation was successful, Failure otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ProcessingNodeFinalizeInputRequirement(
        BufferNegotiationData* pBufferNegotiationData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FinalizeBufferProperties
    ///
    /// @brief  Finalize the buffer properties of each output port
    ///
    /// @param  pBufferNegotiationData Buffer negotiation data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID FinalizeBufferProperties(
        BufferNegotiationData* pBufferNegotiationData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QueryMetadataPublishList
    ///
    /// @brief  Method to query the publish list from the node
    ///
    /// @param  pPublistTagList List of tags published by the node
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult QueryMetadataPublishList(
        NodeMetadataList* pPublistTagList);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsDownscaleOutputPort
    ///
    /// @brief  Helper method to determine if the output port is a downscale port or not
    ///
    /// @param  outputPortId OutputportId to check
    ///
    /// @return TRUE if the output port is a downscale port, FALSE otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsDownscaleOutputPort(
        UINT outputPortId) const
    {
        BOOL isDownscalePort = FALSE;

        switch (outputPortId)
        {
            case BPSOutputPortDS4:
            case BPSOutputPortDS16:
            case BPSOutputPortDS64:
                isDownscalePort = TRUE;
                break;
            default:
                isDownscalePort = FALSE;
                break;
        }

        return isDownscalePort;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsPostLSCModule
    ///
    /// @brief  Helper method to determine if the given module type is post-LSC module in BPS pipeline
    ///
    /// @param  moduleType the given BPS module type to be checked
    ///
    /// @return TRUE if the output port is a stats port, FALSE otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsPostLSCModule(
        ISPIQModuleType moduleType) const
    {
        BOOL isPostLSCModule = TRUE;

        switch (moduleType)
        {
            case ISPIQModuleType::BPSPedestalCorrection:
            case ISPIQModuleType::BPSABF:
            case ISPIQModuleType::BPSLinearization:
            case ISPIQModuleType::BPSBPCPDPC:
            case ISPIQModuleType::BPSDemux:
            case ISPIQModuleType::BPSHDR:
            case ISPIQModuleType::BPSGIC:
            case ISPIQModuleType::BPSLSC:
                isPostLSCModule = FALSE;
                break;
            default:
                break;
        }

        return isPostLSCModule;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsStatsPort
    ///
    /// @brief  Helper method to determine if the output port is a stats port or not
    ///
    /// @param  outputPortId OutputportId to check
    ///
    /// @return TRUE if the output port is a stats port, FALSE otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsStatsPort(
        UINT outputPortId) const
    {
        BOOL isStatsPort = FALSE;

        switch (outputPortId)
        {
            case BPSOutputPortStatsBG:
            case BPSOutputPortStatsHDRBHist:
                isStatsPort = TRUE;
                break;
            default:
                break;
        }

        return isStatsPort;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// OverrideDefaultIQModuleEnablement
    ///
    /// @brief  Helper method to force enable/disable of individual IQ modules
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID OverrideDefaultIQModuleEnablement();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///  UpdateClock
    ///
    /// @brief  Update clock parameters in a command buffer
    ///
    /// @param  pExecuteProcessRequestData       Request specific data
    /// @param  pICPClockBandwidthRequest        clock data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateClock(
        ExecuteProcessRequestData*   pExecuteProcessRequestData,
        CSLICPClockBandwidthRequest* pICPClockBandwidthRequest);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //  UpdateBandwidth
    ///
    /// @brief  Update Bandwidth parameters in a command buffer
    ///
    /// @param  pExecuteProcessRequestData Request specific data
    /// @param  pICPClockBandwidthRequest  Bandwidth data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateBandwidth(
        ExecuteProcessRequestData*   pExecuteProcessRequestData,
        CSLICPClockBandwidthRequest* pICPClockBandwidthRequest);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //  CheckAndUpdateClockBW
    ///
    /// @brief  Update Bandwidth and Clock parameters in a command buffer
    ///
    /// @param  pCmdBuffer                 Command Buffer
    /// @param  pExecuteProcessRequestData Request specific data
    /// @param  pICPClockBandwidthRequest  Bandwidth data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID CheckAndUpdateClockBW(
        CmdBuffer*                   pCmdBuffer,
        ExecuteProcessRequestData*   pExecuteProcessRequestData,
        CSLICPClockBandwidthRequest* pICPClockBandwidthRequest);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //  CalculateBPSRdBandwidth
    ///
    /// @brief  Calculate Read Bandwidth
    ///
    /// @param  pPerRequestPorts Request specific data
    /// @param  pBandwidth       Bandwidth data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID CalculateBPSRdBandwidth(
        PerRequestActivePorts*  pPerRequestPorts,
        BPSBandwidth*           pBandwidth);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //  CalculateBPSWrBandwidth
    ///
    /// @brief  Calculate Write Bandwidth
    ///
    /// @param  pPerRequestPorts Request specific data
    /// @param  pBandwidth       Bandwidth data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID CalculateBPSWrBandwidth(
        PerRequestActivePorts*  pPerRequestPorts,
        BPSBandwidth*           pBandwidth);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetScaleRatios
    ///
    /// @brief  Set Scale Ratios for BPS triggers
    ///
    /// @param  pInputData        Pointer to the ISP input data for setting updates
    /// @param  pCmdBuffer        Command Buffer
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL SetScaleRatios(
        ISPInputData* pInputData,
        CmdBuffer*    pCmdBuffer);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateConfigIO
    ///
    /// @brief     Update ConfigIO data
    ///
    /// @param     pCmdBuffer          Command Buffer
    /// @param     intermediateWidth   Intermediate Width for MFSR
    /// @param     intermediateHeight  Intermediate Height for MFSR
    ///
    /// @return    CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult UpdateConfigIO(
        CmdBuffer*  pCmdBuffer,
        UINT32      intermediateWidth,
        UINT32      intermediateHeight);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpConfigIOData
    ///
    /// @brief  Dump Config IO data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpConfigIOData();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateFWCommandBufferManagers
    ///
    /// @brief  Create Command buffer mangers which are needed by FW
    ///
    /// @return return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateFWCommandBufferManagers();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SendFWCmdRegionInfo
    ///
    /// @brief  Maps the firmware command buffer region to Kernel
    ///
    /// @param  opcode           Determines whther to map or unmap
    /// @param  ppBufferInfo     Pointer to the Buffer informaion pointer to be mapped to FW
    /// @param  numberOfMappings number of memory mappings
    ///
    /// @return return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult SendFWCmdRegionInfo(
        UINT32          opcode,
        CSLBufferInfo** ppBufferInfo,
        UINT32          numberOfMappings);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetFWBufferAlignedSize
    ///
    /// @brief  Calculates the FW accssible buffer alignment
    ///
    /// @param  sizeInBytes size of the memory in bytes, which is to be aligned
    ///
    /// @return return aligned buffer size
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE SIZE_T GetFWBufferAlignedSize(
        SIZE_T sizeInBytes)
    {
        SIZE_T alignment  = 0;
        SIZE_T bytesPerKb = 1024;

        if (sizeInBytes < (4 * bytesPerKb))
        {
            alignment = 4 * bytesPerKb;
        }
        else
        {
            if (0 != (sizeInBytes % (64 * bytesPerKb)))
            {
                alignment  = ((sizeInBytes / (64 * bytesPerKb)) + 1) * (64 * bytesPerKb);
            }
            else
            {
                alignment = sizeInBytes;
            }
        }
        return alignment;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetCorrespondingInputPortForOutputPort
    ///
    /// @brief  method to get the corresponding Input Port for the output port.
    ///
    /// @param  outputPortId to check
    /// @param  pInputPortId to update
    ///
    /// @return True if the Input port matched the corresponding output port
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual BOOL GetCorrespondingInputPortForOutputPort(
        UINT outputPortId,
        UINT* pInputPortId
    ) const
    {
        CAMX_ASSERT(outputPortId <= CSLBPSOutputPortIdMaxNumPortResources);

        BOOL dumpInputPort = FALSE;

        if (NULL != pInputPortId)
        {
            switch (outputPortId)
            {
                case CSLBPSOutputPortIdPDIImageFull:
                    *pInputPortId = CSLBPSInputPortImage;
                    dumpInputPort = TRUE;
                    break;
                default:
                    break;
            }
        }
        return dumpInputPort;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillCmdBufferParams
    ///
    /// @brief  Packs the Resource Params with the values specified
    ///
    /// @param  pParams           Pointer to the Resource Params structure that needs to be filled
    /// @param  resourceSize      size of the resource
    /// @param  type              Type of the resource
    /// @param  memFlags          Memory Flags of the resource
    /// @param  maxNumNestedAddrs max number of nested buffers
    /// @param  pDeviceIndex      Pointer to the Devices of this resource
    /// @param  blobCount         Number of resources required
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID FillCmdBufferParams(
        ResourceParams* pParams,
        UINT            resourceSize,
        CmdType         type,
        UINT32          memFlags,
        UINT            maxNumNestedAddrs,
        const INT32*    pDeviceIndex,
        UINT            blobCount)
    {
        *pParams                                = { 0 };
        pParams->resourceSize                   = resourceSize;
        pParams->poolSize                       = blobCount*resourceSize;
        pParams->usageFlags.cmdBuffer           = 1;
        pParams->cmdParams.type                 = type;
        pParams->alignment                      = CamxCommandBufferAlignmentInBytes;
        pParams->cmdParams.enableAddrPatching   = (maxNumNestedAddrs > 0) ? 1 : 0;
        pParams->cmdParams.maxNumNestedAddrs    = maxNumNestedAddrs;
        pParams->memFlags                       = memFlags;
        pParams->pDeviceIndices                 = pDeviceIndex;
        pParams->numDevices                     = (pParams->pDeviceIndices != NULL) ? 1 : 0;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetTintlessStatus
    ///
    /// @brief  Updates Tintless Enable Status
    ///
    /// @param  pModuleInput Pointer to the module input Data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID GetTintlessStatus(
        ISPInputData* pModuleInput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetMetadataContrastLevel
    ///
    /// @brief  Read level vendor tag and decide the contrast level
    ///
    /// @param  pHALTagsData Pointer to fill contrast level
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID GetMetadataContrastLevel(
        ISPHALTagsData* pHALTagsData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetMetadataTonemapCurve
    ///
    /// @brief  Read tonemap curve from vendor tag
    ///
    /// @param  pHALTagsData Pointer to fill tonemap curve
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID GetMetadataTonemapCurve(
        ISPHALTagsData* pHALTagsData);

    /// @todo (CAMX-1117) Update based on BPS IQ module size
    static const UINT MaxBPSIQModules    = 30;                          ///< Max Number of IQ Modules
    static const UINT MaxBPSStatsModules = 2;                           ///< Max Number of Stats Modules

    ChiContext*           m_pChiContext;                                ///< Chi context pointer
    CmdBufferManager*     m_pIQPacketManager;                           ///< IQ Packet buffer manager
    CmdBufferManager*     m_pBPSCmdBufferManager[BPSCmdBufferMaxIds];   ///< Array Cmd buffer managers for BPS node
    UINT                  m_maxCmdBufferSizeBytes[BPSCmdBufferMaxIds];  ///< Array for size of command buffers for BPS node
    INT32                 m_deviceIndex;                                ///< ICP device index
    CSLDeviceHandle       m_hDevice;                                    ///< BPS device handle
    ISPIQModule*          m_pBPSIQModules[MaxBPSIQModules];             ///< List of IQ Modules
    ISPStatsModule*       m_pBPSStatsModules[MaxBPSStatsModules];       ///< List of IQ Modules
    UINT32                m_LUTCnt[BPSProgramIndexMax];                 ///< Number of LUTs for modules
    UINT32                m_LUTOffset[BPSProgramIndexMax];              ///< Array of offsets within DMI Header cmd buffer
    UINT32                m_moduleChromatixEnable[BPSProgramIndexMax];  ///< Indicates if the IQ module is enabled in chromatix
    UINT32                m_maxLUT;                                     ///< LUT count ,remove and calculate manually
    CSLVersion            m_version;                                    ///< BPS Hardware Revision
    BPSCapabilityInfo     m_capability;                                 ///< BPS Capability Configuration
    BPSIntanceProperty    m_instanceProperty;                           ///< BPS Node Instance Property
    UINT                  m_numBPSStatsModuleEnabled;                   ///< Number of BPS Stats Modules
    UINT                  m_numBPSIQModuleEnabled;                      ///< Number of BPS IQ Modules
    UINT                  m_FPS;                                        ///< FPS requested
    SL_HANDLE             m_hStripingLib;                               ///< Striping Library Handle
    static FrameBuffers   s_frameBufferOffset[BPS_IO_IMAGES_MAX];       ///< Offset to buffer pointer based on port Id
    ISPHALTagsData        m_HALTagsData;                                ///< Keep all input coming from tags
    BPSTuningMetadata*    m_pTuningMetadata;                            ///< Metadata for tuning support
    TuningDataManager*    m_pPreTuningDataManager;                      ///< Store previous tuningDataManager
                                                                        /// to compare if changed
    BOOL                  m_OEMIQSettingEnable;                         ///< indicator for OEM IQ Setting Mode
    IQLibInitialData      m_libInitialData;                             ///< IQ Library initial data
    BOOL                  m_OEMStatsSettingEnable;                      ///< indicator for OEM Stats Setting Mode
    const EEPROMOTPData*  m_pOTPData;                                   ///< OTP Data read from EEPROM to be used
                                                                        /// for calibration
    UINT32                m_BPSPathEnabled[BPSOutputPortReg2 + 1];      ///< output paths enabled based on output ports
    static DebugDataWriter* s_pDebugDataWriter;                         ///< Pointer to the debug data pointer
    static UINT32           s_debugDataWriterCounter;                   ///< Debug data intance tracker
    UINT                    m_BPSHangDumpEnable;                        ///< BPS hang dump enable
    BOOL                    m_BPSStripeDumpEnable;                      ///< BPS stripe dump enable
    BOOL                    m_initConfig;                               ///< initial config
    UINT                    m_BPSCmdBlobCount;                          ///< Number of blob command buffers in circulation
    ChiTuningModeParameter  m_tuningData;                               ///< Cached tuning mode selector data
    CSLDeviceResource       m_deviceResourceRequest;                    ///< Copy of device resource request for
                                                                        /// later Config IO update
    CSLBufferInfo           m_configIOMem;                              ///< Config IO memory
    ConfigIORequest         m_configIORequest;                          ///< Config IO Request
    IntermediateDimensions  m_curIntermediateDimension;                 ///< Current Intermediate Dimension in MFSR usecase
    IntermediateDimensions  m_prevIntermediateDimension;                ///< Previous Intermediate Dimension in MFSR usecase
};

CAMX_NAMESPACE_END

#endif // CAMXBPSNODE_H
