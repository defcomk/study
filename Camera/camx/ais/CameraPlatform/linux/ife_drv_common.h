/**
 * @file ife_drv_common.h
 *
 * Copyright (c) 2010-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _IFE_DRV_COMMON_H
#define _IFE_DRV_COMMON_H

#include "CameraDevice.h"
#include "CameraOSServices.h"
#include "CameraQueue.h"

#include "ife_drv_api.h"
#include "sync_mgr_i.h"
#include "req_mgr_drv.h"
#include "common_types.h"
#include <media/cam_isp.h>

#define AIS_ALIGN_SIZE(x, y) (((x) + ((y)-1)) & ~((y)-1))

#define IFE_RDI_MAX 4  //Up to 4 RDI

#define MAX_NUM_BUFF 12

#define MAX_CMD_BUFF 17 //Blob count based on camxifenode setting

#define PKT_INIT_CONFIG_IDX (MAX_CMD_BUFF-1)     //Reserve the last packet for configuration
#define CMDBUF_INIT_CONFIG_IDX (MAX_CMD_BUFF-1)  //Reserve the last Cmd buf for configuration

static const uint8 MaxIFEOutputPorts = 22;                     ///< Max output ports

/// @brief Used to detect region/buffer overruns
static const uint32 AisCanary = 0x43216789;

/// @brief Device resource request structure.
typedef struct
{
    uint32  resourceID;                 ///< The resourceID of the device resource to acquire.
    void*   pDeviceResourceParam;       ///< Untyped pointer to the resource specific structure, structure can be in/out.
    size_t  deviceResourceParamSize;    ///< The size of the data structure pointed to be pDeviceResourceParam. The size is
                                        ///  used to validate the correct structure is passed.
} CamDeviceResource;

/// @brief Handle to a device within the CAMSS that is exposed to the UMD

static const uint32 IFEOutputFull          = 0x3000;
static const uint32 IFEOutputDS4           = 0x3001;
static const uint32 IFEOutputDS16          = 0x3002;
static const uint32 IFEOutputRaw           = 0x3003;
static const uint32 IFEOutputFD            = 0x3004;
static const uint32 IFEOutputPDAF          = 0x3005;
static const uint32 IFEOutputRDI0          = 0x3006;
static const uint32 IFEOutputRDI1          = 0x3007;
static const uint32 IFEOutputRDI2          = 0x3008;
static const uint32 IFEOutputRDI3          = 0x3009;
static const uint32 IFEOutputStatsHDRBE    = 0x300A;
static const uint32 IFEOutputStatsHDRBHIST = 0x300B;
static const uint32 IFEOutputStatsTLBG     = 0x300C;
static const uint32 IFEOutputStatsBF       = 0x300D;
static const uint32 IFEOutputStatsAWBBG    = 0x300E;
static const uint32 IFEOutputStatsBHIST    = 0x300F;
static const uint32 IFEOutputStatsRS       = 0x3010;
static const uint32 IFEOutputStatsCS       = 0x3011;
static const uint32 IFEOutputStatsIHIST    = 0x3012;
static const uint32 IFEOutputDisplayFull   = 0x3013;
static const uint32 IFEOutputDisplayDS4    = 0x3014;
static const uint32 IFEOutputDisplayDS16   = 0x3015;
static const uint32 IFEOutputDualPD        = 0x3016;

// IFE input resource type
static const uint32 IFEInputTestGen        = 0x4000;
static const uint32 IFEInputPHY0           = 0x4001;
static const uint32 IFEInputPHY1           = 0x4002;
static const uint32 IFEInputPHY2           = 0x4003;
static const uint32 IFEInputPHY3           = 0x4004;

static const uint32 IFEInputInvalid        = 0xFFFF;

// IFE input resource Lane Type
static const uint32 IFELaneTypeDPHY        = 0;
static const uint32 IFELaneTypeCPHY        = 1;

// IFE Generic Blob types
static const uint32 IFEGenericBlobTypeHFRConfig             = 0;
static const uint32 IFEGenericBlobTypeResourceClockConfig   = 1;
static const uint32 IFEGenericBlobTypeResourceBWConfig      = 2;
static const uint32 IFEGenericBlobTypeUBWCConfig            = 3;


// ISP Resource Type
static const uint32 ISPResourceIdPort      = 0;
static const uint32 ISPResourceIdClk       = 1;

// ISP Usage Type
static const uint32 ISPResourceUsageSingle = 0;
static const uint32 ISPResourceUsageDual   = 1;

// ISP Color Pattern
static const uint32 ISPPatternBayerRGRGRG  = 0;
static const uint32 ISPPatternBayerGRGRGR  = 1;
static const uint32 ISPPatternBayerBGBGBG  = 2;
static const uint32 ISPPatternBayerGBGBGB  = 3;
static const uint32 ISPPatternYUVYCBYCR    = 4;
static const uint32 ISPPatternYUVYCRYCB    = 5;
static const uint32 ISPPatternYUVCBYCRY    = 6;
static const uint32 ISPPatternYUVCRYCBY    = 7;

// ISP Input Format
static const uint32 ISPFormatMIPIRaw6      = 1;
static const uint32 ISPFormatMIPIRaw8      = 2;
static const uint32 ISPFormatMIPIRaw10     = 3;
static const uint32 ISPFormatMIPIRaw12     = 4;
static const uint32 ISPFormatMIPIRaw14     = 5;
static const uint32 ISPFormatMIPIRaw16     = 6;
static const uint32 ISPFormatMIPIRaw20     = 7;
static const uint32 ISPFormatRaw8Private   = 8;
static const uint32 ISPFormatRaw10Private  = 9;
static const uint32 ISPFormatRaw12Private  = 10;
static const uint32 ISPFormatRaw14Private  = 11;
static const uint32 ISPFormatPlain8        = 12;
static const uint32 ISPFormatPlain168      = 13;
static const uint32 ISPFormatPlain1610     = 14;
static const uint32 ISPFormatPlain1612     = 15;
static const uint32 ISPFormatPlain1614     = 16;
static const uint32 ISPFormatPlain1616     = 17;
static const uint32 ISPFormatPlain3220     = 18;
static const uint32 ISPFormatPlain64       = 19;
static const uint32 ISPFormatPlain128      = 20;
static const uint32 ISPFormatARGB          = 21;
static const uint32 ISPFormatARGB10        = 22;
static const uint32 ISPFormatARGB12        = 23;
static const uint32 ISPormatARGB14         = 24;
static const uint32 ISPFormatDPCM10610     = 25;
static const uint32 ISPFormatDPCM10810     = 26;
static const uint32 ISPFormatDPCM12612     = 27;
static const uint32 ISPFormatDPCM12812     = 28;
static const uint32 ISPFormatDPCM14814     = 29;
static const uint32 ISPFormatDPCM141014    = 30;
static const uint32 ISPFormatNV21          = 31;
static const uint32 ISPFormatNV12          = 32;
static const uint32 ISPFormatTP10          = 33;
static const uint32 ISPFormatYUV422        = 34;
static const uint32 ISPFormatPD8           = 35;
static const uint32 ISPFormatPD10          = 36;
static const uint32 ISPFormatUBWCNV12      = 37;
static const uint32 ISPFormatUBWCNV124R    = 38;
static const uint32 ISPFormatUBWCTP10      = 39;
static const uint32 ISPFormatUBWCP010      = 40;
static const uint32 ISPFormatPlain8Swap    = 41;
static const uint32 ISPFormatPlain810      = 42;
static const uint32 ISPFormatPlain810Swap  = 43;
static const uint32 ISPFormatYV12          = 44;
static const uint32 ISPFormatY             = 45;
static const uint32 ISPFormatUndefined     = 0xFFFFFFFF;

// ISP output resource type
static const uint32 ISPOutputEncode        = 1000;
static const uint32 ISPOutputView          = 1001;
static const uint32 ISPOutputVideo         = 1002;
static const uint32 ISPOutputRDI0          = 1003;
static const uint32 ISPOutputRDI1          = 1004;
static const uint32 ISPOutputRDI2          = 1005;
static const uint32 ISPOutputRDI3          = 1006;
static const uint32 ISPOutputStatsAEC      = 1007;
static const uint32 ISPOutputStatsAF       = 1008;
static const uint32 ISPOutputStatsAWB      = 1009;
static const uint32 ISPOutputStatsRS       = 1010;
static const uint32 ISPOutputStatsCS       = 1011;
static const uint32 ISPOutputStatsIHIST    = 1012;
static const uint32 ISPOutputStatsSkin     = 1013;
static const uint32 ISPOutputStatsBG       = 1014;
static const uint32 ISPOutputStatsBF       = 1015;
static const uint32 ISPOutputStatsBE       = 1016;
static const uint32 ISPOutputStatsBHIST    = 1017;
static const uint32 ISPOutputStatsBFScale  = 1018;
static const uint32 ISPOutputStatsHDRBE    = 1019;
static const uint32 ISPOutputStatsHDRBHIST = 1020;
static const uint32 ISPOutputStatsAECBG    = 1021;
static const uint32 ISPOutputCAMIFRaw      = 1022;
static const uint32 ISPOutputIdealRaw      = 1023;

// ISP input resource type
static const uint32 ISPInputTestGen        = 1500;
static const uint32 ISPInputPHY0           = 1501;
static const uint32 ISPInputPHY1           = 1502;
static const uint32 ISPInputPHY2           = 1503;
static const uint32 ISPInputPHY3           = 1504;
static const uint32 ISPInputFE             = 1505;

// ISP input resource Lane Type
static const uint32 ISPLaneTypeDPHY        = 0;
static const uint32 ISPLaneTypeCPHY        = 1;

/* ISP Resurce Composite Group ID */
static const uint32  ISPOutputGroupIdNONE   = 0;
static const uint32  ISPOutputGroupId0      = 1;
static const uint32  ISPOutputGroupId1      = 2;
static const uint32  ISPOutputGroupId2      = 3;
static const uint32  ISPOutputGroupId3      = 4;
static const uint32  ISPOutputGroupId4      = 5;
static const uint32  ISPOutputGroupId5      = 6;
static const uint32  ISPOutputGroupIdMAX    = 6;

static const uint16  ISPAcquireCommonVersion0 = 0x1000;     ///< IFE Common Resource structure Version
static const uint16  ISPAcquireInputVersion0  = 0x2000;     ///< IFE Input Resource structure Version
static const uint16  ISPAcquireOutputVersion0 = 0x3000;     ///< IFE Output Resource structure Version

/// @brief ISP output Resource Information
struct ISPOutResourceInfo
{
    uint32 resourceType;       ///< Resource type
    uint32 format;             ///< Format of the output
    uint32 width;              ///< Width of the output image
    uint32 height;             ///< Height of the output image
    uint32 compositeGroupId;   ///< Composite Group id of the group
    uint32 splitPoint;         ///< Split point in pixels for Dual ISP case
    uint32 secureMode;         ///< Output port to be secure or non-secure
    uint32 reserved;           ///< Reserved field - filling with ifeId in AIS
};

/// @brief ISP input resource information
struct ISPInResourceInfo
{
    uint32             resourceType;      ///< Resource type
    uint32             laneType;          ///< Lane type (dphy/cphy)
    uint32             laneNum;           ///< Active lane number
    uint32             laneConfig;        ///< Lane configurations: 4 bits per lane
    uint32             VC;                ///< Virtual Channel
    uint32             DT;                ///< Data Type
    uint32             format;            ///< Input Image Format
    uint32             testPattern;       ///< Test Pattern for the TPG
    uint32             usageType;         ///< Single ISP or Dual ISP
    uint32             leftStart;         ///< Left input start offset in pixels
    uint32             leftStop;          ///< Left input stop offset in pixels
    uint32             leftWidth;         ///< Left input Width in pixels
    uint32             rightStart;        ///< Right input start offset in pixels
    uint32             rightStop;         ///< Right input stop offset in pixels
    uint32             rightWidth;        ///< Right input Width in pixels
    uint32             lineStart;         ///< Start offset of the pixel line
    uint32             lineStop;          ///< Stop offset of the pixel line
    uint32             height;            ///< Input height in lines
    uint32             pixleClock;        ///< Sensor output clock
    uint32             batchSize;         ///< Batch size for HFR mode
    uint32             DSPMode;           ///< DSP Mode
    uint32             HBICount;          ///< HBI Count
    uint32             reserved;          ///< Reserved field
    uint32             numberOutResource; ///< Number of the output resource associated
    ISPOutResourceInfo pDataField[1];     ///< Output Resource starting point
};
/// @brief ISP resource acquire Information
struct ISPAcquireHWInfo
{
    uint16 commonInfoVersion;             ///< Common Info Version represents the IFE common resource structure version
                                          ///< currently we dont have any common reosurce strcuture, It is reserved for future
                                          ///< HW changes in KMD
    uint16 commonInfoSize;                ///< Common Info Size represents IFE common resource strcuture Size
    uint32 commonInfoOffset;              ///< Common Info Offset represnts the Offset from where the IFE common resource
                                          ///< structure is stored in data
    uint32 numInputs;                     ///< Number of IFE inpiut resources
    uint32 inputInfoVersion;              ///< Input Info Strcure Version
    uint32 inputInfoSize;                 ///< Size of the Input resource
    uint32 inputInfoOffset;               ///< Offset where the Input resource structure starts in data
    uint64 data;                          ///< IFE resource data
};

typedef struct
{
    ife_rdi_config_t rdi_cfg;
    int32 input_format;
    int32 output_format;
    uint32 input_resourceType;
    uint32 output_resourceType;
}ife_rdi_info_t;



//*********************************************
// packet related defines
//*********************************************
/// @brief Command type constants reflecting CSL command types
/// @brief Command type constants reflecting CSL command types
enum class CmdType: uint32
{
    Invalid = 0,    ///< Invalid command type
    CDMDMI,         ///< DMI buffer (new format used in Titan)
    CDMDMI16,       ///< DMI buffer for 16-bit elements
    CDMDMI32,       ///< DMI buffer for 32-bit elements
    CDMDMI64,       ///< DMI buffer for 64-bit elements
    CDMDirect,      ///< Direct command buffer
    CDMIndirect,    ///< Indirect command buffer
    I2C,            ///< I2C command buffer
    FW,             ///< Firmware command buffer
    Generic,        ///< Generic command buffer
    Legacy          ///< Legacy blob
};




/// @brief  Mask and shift constants for packet opcode
static const uint32 CSLPacketOpcodeMask         = 0xffffff;     ///< Opcode mask
static const uint32 CSLPacketOpcodeShift        = 0;            ///< Opcode shift
static const uint32 CSLPacketOpcodeDeviceMask   = 0xff000000;   ///< Device tpye mask
static const uint32 CSLPacketOpcodeDeviceShift  = 24;           ///< Device type shift
static const uint32 IfeDeviceType               = 15;           ///< IfeDeviceType


#define AIS_PACKED                  __attribute__((__packed__))
#define AIS_BEGIN_PACKED
#define AIS_PACKED_ALIGN_N(N)       __attribute__((packed, aligned(N)))
#define AIS_BEGIN_PACKED_ALIGN_N(N)
#define AIS_END_PACKED


/// @brief Command buffer identifier for (dual) IFE
typedef enum
{
    CSLIFECmdBufferIdLeft        = 1,   ///< Left IFE's command buffer
    CSLIFECmdBufferIdRight       = 2,   ///< Right IFE's command buffer
    CSLIFECmdBufferIdCommon      = 3,   ///< Common (main) command buffer
    CSLIFECmdBufferIdDualConfig  = 9,   ///< Dual IFE configuration info
    CSLIFECmdBufferIdGenericBlob = 12,  ///< Generic Blob configuration info
} CSLIFECmdBufferId;

/// @brief  Mask and shift constants for GenericBlob CmdBuffer header
static const uint32 CSLGenericBlobHeaderSizeInDwords    = 1;            ///< Blob header size in dwords
static const uint32 CSLGenericBlobCmdSizeMask           = 0xffffff00;   ///< Mask value for Blob Size in Blob Header
static const uint32 CSLGenericBlobCmdSizeShift          = 8;            ///< Shift value for Blob Size in Blob Header
static const uint32 CSLGenericBlobCmdTypeMask           = 0xff;         ///< Mask value for Blob Type in Blob Header
static const uint32 CSLGenericBlobCmdTypeShift          = 0;            ///< Shift value for Blob Type in Blob Header

static const uint8 CSLMaxNumPlanes       = 3;   ///< Max number of IO ports that an IO config may refer to

/// @brief Define a single plane geometry
typedef struct
{
    uint32 width;          ///< Width of the YUV plane in bytes., tile aligned width in bytes for UBWC
    uint32 height;         ///< Height of the YUV plane in bytes.
    uint32 planeStride;    ///< The number of bytes between the first byte of two sequential lines on plane 1. It may be
                           ///  greater than nWidth * nDepth / 8 if the line includes padding.
    uint32 sliceHeight;    ///< The number of lines in the plane which can be equal to or larger than actual frame height.

    uint32 metadataStride; ///< UBWC metadata stride
    uint32 metadataSize;   ///< UBWC metadata plane size
    uint32 metadataOffset; ///< UBWC metadata offset
    uint32 packerConfig;   ///< UBWC packer config
    uint32 modeConfig;     ///< UBWC mode config
    uint32 tileConfig;     ///< UBWC tile config
    uint32 hInit;          ///< UBWC H_INIT
    uint32 vInit;          ///< UBWC V_INIT
} AIS_PACKED CSLPlane;


typedef struct
{
    CSLBufferInfo* pCSLBufferInfo; // The associated CSLbuffer that was allocated from kernel
    void*          pVirtualAddr;   // Virtual address to start of this buffer
    uint32 offset;                 // The offset into CSLbuffer where this command buffer starts
    uint32 size;                   // Size of this command buffer
    uint32 length;                 // length of actual data in this command buffer
    uint32 type;                   // Type of command buffer
    uint32 metadata;
} AISCmdBuffer;

typedef struct
{
    CSLBufferInfo* pCSLBufferInfo;
    void*          pVirtualAddr;
    uint32         dataSize;
    uint32         offset;
} AISPktBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  A command buffer descriptor that contains information about memory used as well as metadata and a fence that will
///         signal when the command buffer is consumed.
///         Must be 64-bit aligned.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    int32           hMem;       ///< Memory handle
    uint32          offset;     ///< The offset from the start of the memory described by mem
    uint32          size;       ///< The max size of this buffer
    uint32          length;     ///< The (valid) length of the commands
    uint32          type;       ///< Indicates the type of the command buffer
    uint32          metadata;   ///< Packet-specific metadata between nodes and KMD
} AIS_PACKED CSLCmdMemDesc;

/// @brief UBWC configuration for a Port
typedef struct
{
    uint32  portResourceId;   ///< Identifies port resource
    uint32  metadataStride;   ///< UBWC metadata stride
    uint32  metadataSize;     ///< UBWC metadata plane size
    uint32  metadataOffset;   ///< UBWC metadata offset
    uint32  packerConfig;     ///< UBWC packer config
    uint32  modeConfig;       ///< UBWC mode config
    uint32  modeConfig1;      ///< UBWC mode config1
    uint32  tileConfig;       ///< UBWC tile config
    uint32  hInitialVal;      ///< UBWC H_INIT
    uint32  vInitialVal;      ///< UBWC V_INIT
}AIS_PACKED CSLPortUBWCConfig;

/// @brief UBWC Resource Configuration
typedef struct
{
    uint32              UBWCAPIVersion;                          ///< Supported UBWC API Version
    uint32              numPorts;                                ///< Number of ports to configure for UBWC
    CSLPortUBWCConfig   portUBWCConfig[1][CSLMaxNumPlanes - 1];  ///< Starting point for UBWC Config parameters
} AIS_PACKED CSLResourceUBWCConfig;


typedef struct
{
    CSLMemHandle            hMems[CSLMaxNumPlanes];             ///< Memory handles per plane
    uint32                  offsets[CSLMaxNumPlanes];           ///< Memory offset from the corresponding handle
    CSLPlane                planes[CSLMaxNumPlanes];            ///< Plane descriptors
    uint32                  format;                             ///< Format of the image
    uint32                  colorSpace;                         ///< The color space of the image
    uint32                  colorFilterPattern;                 ///< Color filter pattern of the RAW format
    uint32                  bitsPerPixel;                       ///< Bits per pixel for raw formats
    uint32                  rotation;                           ///< Rotation applied to the image
    uint32                  portResourceId;                     ///< Identifies port resource from
    CSLFence                hSync;                              ///< If output buffer: bufdone; if input buffer: buf ready
    CSLFence                hEarlySync;                         ///< "Early done" sync
    CSLCmdMemDesc           auxCmdBuffer;                       ///< An auxiliary command buffer that may be used for
                                                                ///  programming the IO (node/opcode-specific)
    uint32                  direction;                          ///< Direction of the config
    uint32                  batchSize;                          ///< Batch size in HFR mode
    uint32                  subsamplePattern;                   ///< Subsample pattern. Used in HFR mode. It should be
                                                                ///  consistent with batchSize and CAMIF programming.
    uint32                  subsamplePeriod;                    ///< Subsample period. Used in HFR mode. It should be consistent
                                                                ///  with batchSize and CAMIF programming.
    uint32                  framedropPattern;                   ///< Framedrop pattern.
    uint32                  framedropPeriod;                    ///< Framedrop period. Must be multiple of subsamplePeriod if
                                                                ///  in HFR mode.
    uint32                  flags;                              ///< For providing extra information
    uint32                  padding;                            ///< Padding
} AIS_PACKED CSLBufferIOConfig;


typedef struct
{
    uint32  opcode;     ///< Code that identifies the operation (device type + opcode)
    uint32  size;       ///< The size of the packet (total dynamic size of the packet not just the struct)
    uint64  requestId;  ///< Request ID
    uint32  flags;      ///< Flags for this packet
    uint32  padding;    ///< Padding
} AIS_PACKED CSLPacketHeader;


typedef struct
{
    CSLMemHandle    hDstBuffer; ///< Memory handle of the destination buffer that will be patched
    uint32          dstOffset;  ///< Byte offset of the destination where to-be-patched address resides
    CSLMemHandle    hSrcBuffer; ///< Memory handle of the source buffer whose address needs to be used
    uint32          srcOffset;  ///< Byte offset that needs to be added to the source address
} AIS_PACKED CSLAddrPatch;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Most packets can be represented using just the common packet fields.
///         The reason the CSLPacketHeader is still nested within Packet is to allow all the subpackets
///         to have a uniform access path to the base fields.
///         Must be 64-bit aligned.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    CSLPacketHeader header;             ///< Common portion of packets that will be the same for all packet types
    uint32          cmdBuffersOffset;   ///< Byte offset within packet's data section where command buffer descs start
    uint32          numCmdBuffers;      ///< Number of command buffers
    uint32          ioConfigsOffset;    ///< Byte offset within packet's data section where IO configs start
    uint32          numBufferIOConfigs; ///< Number of IO configs
    uint32          patchsetOffset;     ///< Byte offset within packet's data section where patchset data start
    uint32          numPatches;         ///< Number of patches in the patchset
    uint32          kmdCmdBufferIndex;  ///< Index to the command buffer that KMD may use for commanding CDM
    uint32          kmdCmdBufferOffset; ///< Offset in the command buffer where KMD commands begins
    uint64          data[1];            ///< CSLCmdMemDesc instances followed by
} AIS_PACKED CSLPacket;


/// @brief HFR configuration for a Port
struct IFEPortHFRConfig
{
    uint32  portResourceId;     ///< Identifies port resource
    uint32  subsamplePattern;   ///< Subsample pattern. Used in HFR mode. It should be
                                ///  consistent with batchSize and CAMIF programming.
    uint32  subsamplePeriod;    ///< Subsample period. Used in HFR mode. It should be consistent
                                ///  with batchSize and CAMIF programming.
    uint32  framedropPattern;   ///< Framedrop pattern.
    uint32  framedropPeriod;    ///< Framedrop period. Must be multiple of subsamplePeriod if in HFR mode.
    uint32  reserved;           ///< Reserved for alignment
} AIS_PACKED;

/// @brief HFR configuration
struct IFEResourceHFRConfig
{
    uint32              numPorts;           ///< Number of Ports to configure for HFR
    uint32              reserved;           ///< Reserved for alignment
    IFEPortHFRConfig    portHFRConfig[1];   ///< Starting point for HFR Resource config information
} AIS_PACKED;

static const uint32 RDIMaxNum = 4;          ///< Max Number of RDIs

/// @brief IFE Clock and settings to configure the IFE core clocks
struct IFEResourceClockConfig
{
    uint32  usageType;           ///< Single IFE or Dual IFE mode
    uint32  numRdi;              ///< Number of RDIs - KMD expects this to match RDIMaxNum
    uint64  leftPixelClockHz;    ///< Pixel clock for "left" IFE. If Single IFE in use, only this value is populated.
    uint64  rightPixelClockHz;   ///< Pixel clock for "right" IFE. Only valid if dual IFEs are in use.
    uint64  rdiClockHz[4];       ///< Clock rate for RDI path
} AIS_PACKED;

/// @brief IFE Bandwidth votes needed by the IFE for the active use case
struct IFEResourceBWVote
{
    uint32  usageType;           ///< Single IFE or Dual IFE mode
    uint32  reserved;            ///< Number of RDIs
    uint64  camnocBWbytes;       ///< Uncompressed BW in bytes required within CAMNOC
    uint64  externalBWbytes;     ///< BW in bytes required outside of CAMNOC (MNOC etc). Accounts for compression if enabled.
} AIS_PACKED;

/// @brief IFE Bandwidth votes needed by the IFE for the active use case
struct IFEResourceBWConfig
{
    uint32             usageType;        ///< Single IFE or Dual IFE mode
    uint32             numRdi;           ///< Number of RDIs
    IFEResourceBWVote  leftPixelVote;    ///< Bandwidth vote for "left" IFE, valid for single IFE
    IFEResourceBWVote  rightPixelVote;   ///< Bandwidth vote for "right" IFE, valid for dual IFEs
    IFEResourceBWVote  rdiVote[4];       ///< Bandwidth vote for RDI path
} AIS_PACKED;

#endif /* _IFE_DRV_COMMON_H */
