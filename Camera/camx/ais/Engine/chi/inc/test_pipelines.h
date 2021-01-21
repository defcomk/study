////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef USECASEDEFS_H
#define USECASEDEFS_H

#include "chi.h"
#include "chicommon_native.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/// @brief ranges of buffer sizes supported
struct BufferDimension
{
    UINT minWidth;
    UINT minHeight;
    UINT maxWidth;
    UINT maxHeight;
};

/// @brief Collection of information describing how a buffer is used
struct ChiTarget
{
    ChiStreamType       direction;
    BufferDimension     dimension;
    UINT                numFormats;
    ChiBufferFormat*    pBufferFormats;
    ChiStream*          pChiStream;
};

/// @brief Information regarding where a port interacts with a buffer directly
struct ChiTargetPortDescriptor
{
    const CHAR*           pTargetName;
    ChiTarget*            pTarget;
    ChiLinkNodeDescriptor nodeport;
};

/// @brief List of port->buffer information
struct ChiTargetPortDescriptorInfo
{
    UINT                     numTargets;
    ChiTargetPortDescriptor* pTargetPortDesc;
};

/// @brief Combination of pipeline information with buffer information
struct ChiPipelineTargetCreateDescriptor
{
    const CHAR*                 pPipelineName;
    ChiPipelineCreateDescriptor pipelineCreateDesc;
    ChiTargetPortDescriptorInfo sinkTarget;
    ChiTargetPortDescriptorInfo sourceTarget;
};

/// @brief Collection of information summarizing a usecase
struct ChiUsecase
{
    const CHAR*                        pUsecaseName;
    UINT                               streamConfigMode;
    UINT                               numTargets;
    ChiTarget**                        ppChiTargets;
    UINT                               numPipelines;
    ChiPipelineTargetCreateDescriptor* pPipelineTargetCreateDesc;
};

/// @brief Collection of usecases with matching properties (target count at this point)
struct ChiTargetUsecases
{
    UINT        numUsecases;
    ChiUsecase* pChiUsecases;
};

/*==================== USECASE: TestIFEFull =======================*/

static ChiBufferFormat TestIFEFull_TARGET_BUFFER_FULL_formats[] =
{
    ChiFormatYUV420NV12,
};

static ChiTarget TestIFEFull_TARGET_BUFFER_FULL_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2160
    },
    1,
    TestIFEFull_TARGET_BUFFER_FULL_formats,
    NULL
}; // TARGET_BUFFER_FULL

static ChiTarget* TestIFEFull_Targets[] =
{
	&TestIFEFull_TARGET_BUFFER_FULL_target
};

/*****************************Pipeline IfeFull***************************/

static ChiLinkNodeDescriptor TestIFEFull_IfeFullLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestIFEFull_IfeFullLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestIFEFull_IfeFull_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_FULL", &TestIFEFull_TARGET_BUFFER_FULL_target, {65536, 0, 0}}, // TARGET_BUFFER_FULL
};

static ChiOutputPortDescriptor TestIFEFull_IfeFullNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestIFEFull_IfeFullNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestIFEFull_IfeFullNode65536_0OutputPortDescriptors[] =
{
    {0, 1, 1, 0, 0, NULL}, // IFEOutputPortFull
};

static ChiNode TestIFEFull_IfeFullNodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, TestIFEFull_IfeFullNode0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 0, { 1, TestIFEFull_IfeFullNode65536_0InputPortDescriptors, 1, TestIFEFull_IfeFullNode65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestIFEFull_IfeFullLinks[] =
{
    {{0, 0, 0, 0}, 1, TestIFEFull_IfeFullLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestIFEFull_IfeFullLink1DestDescriptors, {0}, {0}},
};

enum TestIFEFullPipelineIds
{
    IfeFull = 0,
};

static ChiPipelineTargetCreateDescriptor TestIFEFull_pipelines[] =
{
    {"IfeFull", { 0, 2, TestIFEFull_IfeFullNodes, 2, TestIFEFull_IfeFullLinks, 1}, {1, TestIFEFull_IfeFull_sink_TargetDescriptors}, {0, NULL}},  // IfeFull
};

/*==================== USECASE: TestIPEFull =======================*/

static ChiBufferFormat TestIPEFull_TARGET_BUFFER_PREVIEW_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiTarget TestIPEFull_TARGET_BUFFER_PREVIEW_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestIPEFull_TARGET_BUFFER_PREVIEW_formats,
    NULL
}; // TARGET_BUFFER_PREVIEW

static ChiTarget* TestIPEFull_Targets[] =
{
	&TestIPEFull_TARGET_BUFFER_PREVIEW_target
};

/*****************************Pipeline Ipefull***************************/

static ChiLinkNodeDescriptor TestIPEFull_IpefullLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestIPEFull_IpefullLink1DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestIPEFull_IpefullLink2DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestIPEFull_Ipefull_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_PREVIEW", &TestIPEFull_TARGET_BUFFER_PREVIEW_target, {65538, 0, 8}}, // TARGET_BUFFER_PREVIEW
};

static ChiOutputPortDescriptor TestIPEFull_IpefullNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestIPEFull_IpefullNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestIPEFull_IpefullNode65536_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // IFEOutputPortFull
};

static ChiInputPortDescriptor TestIPEFull_IpefullNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestIPEFull_IpefullNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiNode TestIPEFull_IpefullNodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, TestIPEFull_IpefullNode0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 0, { 1, TestIPEFull_IpefullNode65536_0InputPortDescriptors, 1, TestIPEFull_IpefullNode65536_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 1, TestIPEFull_IpefullNode65538_0InputPortDescriptors, 1, TestIPEFull_IpefullNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestIPEFull_IpefullLinks[] =
{
    {{0, 0, 0, 0}, 1, TestIPEFull_IpefullLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestIPEFull_IpefullLink1DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, TestIPEFull_IpefullLink2DestDescriptors, {0}, {0}},
};

enum TestIPEFullPipelineIds
{
    Ipefull = 0,
};

static ChiPipelineTargetCreateDescriptor TestIPEFull_pipelines[] =
{
    {"Ipefull", { 0, 3, TestIPEFull_IpefullNodes, 3, TestIPEFull_IpefullLinks, 1}, {1, TestIPEFull_Ipefull_sink_TargetDescriptors}, {0, NULL}},  // Ipefull
};

/*==================== USECASE: TestIFEFullTestgen =======================*/

static ChiBufferFormat TestIFEFullTestgen_TARGET_BUFFER_FULL_formats[] =
{
    ChiFormatYUV420NV12,
};

static ChiTarget TestIFEFullTestgen_TARGET_BUFFER_FULL_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2160
    },
    1,
    TestIFEFullTestgen_TARGET_BUFFER_FULL_formats,
    NULL
}; // TARGET_BUFFER_FULL

static ChiTarget* TestIFEFullTestgen_Targets[] =
{
	&TestIFEFullTestgen_TARGET_BUFFER_FULL_target
};

/*****************************Pipeline IfeFullTestgen***************************/

static ChiLinkNodeDescriptor TestIFEFullTestgen_IfeFullTestgenLink0DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestIFEFullTestgen_IfeFullTestgen_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_FULL", &TestIFEFullTestgen_TARGET_BUFFER_FULL_target, {65536, 0, 0}}, // TARGET_BUFFER_FULL
};

static ChiOutputPortDescriptor TestIFEFullTestgen_IfeFullTestgenNode65536_0OutputPortDescriptors[] =
{
    {0, 1, 1, 0, 0, NULL}, // IFEOutputPortFull
};

static ChiNode TestIFEFullTestgen_IfeFullTestgenNodes[] =
{
    {NULL, 65536, 0, { 0, NULL, 1, TestIFEFullTestgen_IfeFullTestgenNode65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestIFEFullTestgen_IfeFullTestgenLinks[] =
{
    {{65536, 0, 0, 0}, 1, TestIFEFullTestgen_IfeFullTestgenLink0DestDescriptors, {0}, {0}},
};

enum TestIFEFullTestgenPipelineIds
{
    IfeFullTestgen = 0,
};

static ChiPipelineTargetCreateDescriptor TestIFEFullTestgen_pipelines[] =
{
    {"IfeFullTestgen", { 0, 1, TestIFEFullTestgen_IfeFullTestgenNodes, 1, TestIFEFullTestgen_IfeFullTestgenLinks, 1}, {1, TestIFEFullTestgen_IfeFullTestgen_sink_TargetDescriptors}, {0, NULL}},  // IfeFullTestgen
};

/*==================== USECASE: UsecaseRaw =======================*/

static ChiBufferFormat UsecaseRaw_TARGET_BUFFER_RAW_formats[] =
{
    ChiFormatRawMIPI,
    ChiFormatRawPlain16,
};

static ChiTarget UsecaseRaw_TARGET_BUFFER_RAW_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        5488,
        4112
    },
    2,
    UsecaseRaw_TARGET_BUFFER_RAW_formats,
    NULL
}; // TARGET_BUFFER_RAW

static ChiTarget* UsecaseRaw_Targets[] =
{
	&UsecaseRaw_TARGET_BUFFER_RAW_target
};

/*****************************Pipeline RDI0***************************/

static ChiLinkNodeDescriptor UsecaseRaw_RDI0Link0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor UsecaseRaw_RDI0Link1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor UsecaseRaw_RDI0_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &UsecaseRaw_TARGET_BUFFER_RAW_target, {65536, 0, 8}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor UsecaseRaw_RDI0Node0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor UsecaseRaw_RDI0Node65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor UsecaseRaw_RDI0Node65536_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IFEOutputPortRDI0
};

static ChiNode UsecaseRaw_RDI0Nodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, UsecaseRaw_RDI0Node0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 0, { 1, UsecaseRaw_RDI0Node65536_0InputPortDescriptors, 1, UsecaseRaw_RDI0Node65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink UsecaseRaw_RDI0Links[] =
{
    {{0, 0, 0, 0}, 1, UsecaseRaw_RDI0Link0DestDescriptors, {0}, {0, 0}},
    {{65536, 0, 8, 0}, 1, UsecaseRaw_RDI0Link1DestDescriptors, {0}, {0, 0}},
};


/*****************************Pipeline RDI1***************************/
static ChiLinkNodeDescriptor UsecaseRaw_RDI1Link0DestDescriptors[] =
{
    {65536, 1, 2, 0}, // IFE IFEInstance0 Nodestance 0
};

static ChiLinkNodeDescriptor UsecaseRaw_RDI1Link1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor UsecaseRaw_RDI1_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &UsecaseRaw_TARGET_BUFFER_RAW_target, {65536, 1, 9}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor UsecaseRaw_RDI1Node0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor UsecaseRaw_RDI1Node65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor UsecaseRaw_RDI1Node65536_0OutputPortDescriptors[] =
{
    {9, 1, 1, 0, 0, NULL}, // IFEOutputPortRDI1
};

static ChiNode UsecaseRaw_RDI1Nodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, UsecaseRaw_RDI1Node0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 1, { 1, UsecaseRaw_RDI1Node65536_0InputPortDescriptors, 1, UsecaseRaw_RDI1Node65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink UsecaseRaw_RDI1Links[] =
{
    {{0, 0, 0, 0}, 1, UsecaseRaw_RDI1Link0DestDescriptors, {0}, {0, 0}},
    {{65536, 1, 9, 0}, 1, UsecaseRaw_RDI1Link1DestDescriptors, {0}, {0, 0}},
};


/*****************************Pipeline RDI2***************************/
static ChiLinkNodeDescriptor UsecaseRaw_RDI2Link0DestDescriptors[] =
{
    {65536, 2, 2, 0}, // IFE IFEInstance0 Node instance 0
};

static ChiLinkNodeDescriptor UsecaseRaw_RDI2Link1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor UsecaseRaw_RDI2_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &UsecaseRaw_TARGET_BUFFER_RAW_target, {65536, 2, 10}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor UsecaseRaw_RDI2Node0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor UsecaseRaw_RDI2Node65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor UsecaseRaw_RDI2Node65536_0OutputPortDescriptors[] =
{
    {10, 1, 1, 0, 0, NULL}, // IFEOutputPortRDI2
};

static ChiNode UsecaseRaw_RDI2Nodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, UsecaseRaw_RDI2Node0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 2, { 1, UsecaseRaw_RDI2Node65536_0InputPortDescriptors, 1, UsecaseRaw_RDI2Node65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink UsecaseRaw_RDI2Links[] =
{
    {{0, 0, 0, 0}, 1, UsecaseRaw_RDI2Link0DestDescriptors, {0}, {0, 0}},
    {{65536, 2, 10, 0}, 1, UsecaseRaw_RDI2Link1DestDescriptors, {0}, {0, 0}},
};

/*****************************Pipeline RDI3***************************/
static ChiLinkNodeDescriptor UsecaseRaw_RDI3Link0DestDescriptors[] =
{
    {65536, 3, 2, 0}, // IFE IFEInstance0 Node instance 0
};

static ChiLinkNodeDescriptor UsecaseRaw_RDI3Link1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor UsecaseRaw_RDI3_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &UsecaseRaw_TARGET_BUFFER_RAW_target, {65536, 3, 8}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor UsecaseRaw_RDI3Node0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor UsecaseRaw_RDI3Node65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor UsecaseRaw_RDI3Node65536_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IFEOutputPortRDI3
};

static ChiNode UsecaseRaw_RDI3Nodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, UsecaseRaw_RDI3Node0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 3, { 1, UsecaseRaw_RDI3Node65536_0InputPortDescriptors, 1, UsecaseRaw_RDI3Node65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink UsecaseRaw_RDI3Links[] =
{
    {{0, 0, 0, 0}, 1, UsecaseRaw_RDI3Link0DestDescriptors, {0}, {0, 0}},
    {{65536, 3, 8, 0}, 1, UsecaseRaw_RDI3Link1DestDescriptors, {0}, {0, 0}},
};

/*****************************Pipeline Camif***************************/

static ChiLinkNodeDescriptor UsecaseRaw_CamifLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor UsecaseRaw_CamifLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor UsecaseRaw_Camif_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &UsecaseRaw_TARGET_BUFFER_RAW_target, {65536, 0, 3}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor UsecaseRaw_CamifNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor UsecaseRaw_CamifNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor UsecaseRaw_CamifNode65536_0OutputPortDescriptors[] =
{
    {3, 1, 1, 0, 0, NULL}, // IFEOutputPortCAMIFRaw
};

static ChiNode UsecaseRaw_CamifNodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, UsecaseRaw_CamifNode0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 0, { 1, UsecaseRaw_CamifNode65536_0InputPortDescriptors, 1, UsecaseRaw_CamifNode65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink UsecaseRaw_CamifLinks[] =
{
    {{0, 0, 0, 0}, 1, UsecaseRaw_CamifLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 3, 0}, 1, UsecaseRaw_CamifLink1DestDescriptors, {0}, {0}},
};

/*****************************Pipeline Lsc***************************/

static ChiLinkNodeDescriptor UsecaseRaw_LscLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor UsecaseRaw_LscLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor UsecaseRaw_Lsc_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &UsecaseRaw_TARGET_BUFFER_RAW_target, {65536, 0, 4}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor UsecaseRaw_LscNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor UsecaseRaw_LscNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor UsecaseRaw_LscNode65536_0OutputPortDescriptors[] =
{
    {4, 1, 1, 0, 0, NULL}, // IFEOutputPortLSCRaw
};

static ChiNode UsecaseRaw_LscNodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, UsecaseRaw_LscNode0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 0, { 1, UsecaseRaw_LscNode65536_0InputPortDescriptors, 1, UsecaseRaw_LscNode65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink UsecaseRaw_LscLinks[] =
{
    {{0, 0, 0, 0}, 1, UsecaseRaw_LscLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 4, 0}, 1, UsecaseRaw_LscLink1DestDescriptors, {0}, {0}},
};

enum UsecaseRawPipelineIds
{
    RDI0 = 0,
    RDI1 = 1,
    RDI2 = 2,
    RDI3 = 3,
};

static ChiPipelineTargetCreateDescriptor UsecaseRaw_pipelines[] =
{
    {"RDI0", { 0, 2, UsecaseRaw_RDI0Nodes, 2, UsecaseRaw_RDI0Links, 1}, {1, UsecaseRaw_RDI0_sink_TargetDescriptors}, {0, NULL}},  // RDI0
    {"RDI1", { 0, 2, UsecaseRaw_RDI1Nodes, 2, UsecaseRaw_RDI1Links, 1}, {1, UsecaseRaw_RDI1_sink_TargetDescriptors}, {0, NULL}},  // RDI1
    {"RDI2", { 0, 2, UsecaseRaw_RDI2Nodes, 2, UsecaseRaw_RDI2Links, 1}, {1, UsecaseRaw_RDI2_sink_TargetDescriptors}, {0, NULL}},  // RDI2
    {"RDI3", { 0, 2, UsecaseRaw_RDI3Nodes, 2, UsecaseRaw_RDI3Links, 1}, {1, UsecaseRaw_RDI3_sink_TargetDescriptors}, {0, NULL}},  // RDI3
};

/*==================== USECASE: TestCustomNode =======================*/

static ChiBufferFormat TestCustomNode_TARGET_BUFFER_PREVIEW_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiTarget TestCustomNode_TARGET_BUFFER_PREVIEW_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestCustomNode_TARGET_BUFFER_PREVIEW_formats,
    NULL
}; // TARGET_BUFFER_PREVIEW

static ChiTarget* TestCustomNode_Targets[] =
{
	&TestCustomNode_TARGET_BUFFER_PREVIEW_target
};

/*****************************Pipeline VendorTagWrite***************************/

static ChiLinkNodeDescriptor TestCustomNode_VendorTagWriteLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestCustomNode_VendorTagWriteLink1DestDescriptors[] =
{
    {255, 0, 0, 0}, // ChiExternalNode
};

static ChiLinkNodeDescriptor TestCustomNode_VendorTagWriteLink2DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestCustomNode_VendorTagWriteLink3DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestCustomNode_VendorTagWrite_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_PREVIEW", &TestCustomNode_TARGET_BUFFER_PREVIEW_target, {65538, 0, 8}}, // TARGET_BUFFER_PREVIEW
};

static ChiOutputPortDescriptor TestCustomNode_VendorTagWriteNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestCustomNode_VendorTagWriteNode255_0InputPortDescriptors[] =
{
    {0, 0, 0}, // ChiNodeInputPortFull
};

static ChiOutputPortDescriptor TestCustomNode_VendorTagWriteNode255_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // ChiNodeOutputPortFull
};

static ChiInputPortDescriptor TestCustomNode_VendorTagWriteNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestCustomNode_VendorTagWriteNode65536_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // IFEOutputPortFull
};

static ChiInputPortDescriptor TestCustomNode_VendorTagWriteNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestCustomNode_VendorTagWriteNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiNodeProperty TestCustomNode_VendorTagWrite_node255_0_properties[] =
{
    {1, "com.bots.node.vendortagwrite"},
};

static ChiNode TestCustomNode_VendorTagWriteNodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, TestCustomNode_VendorTagWriteNode0_0OutputPortDescriptors }, 0},
    {TestCustomNode_VendorTagWrite_node255_0_properties, 255, 0, { 1, TestCustomNode_VendorTagWriteNode255_0InputPortDescriptors, 1, TestCustomNode_VendorTagWriteNode255_0OutputPortDescriptors }, 1},
    {NULL, 65536, 0, { 1, TestCustomNode_VendorTagWriteNode65536_0InputPortDescriptors, 1, TestCustomNode_VendorTagWriteNode65536_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 1, TestCustomNode_VendorTagWriteNode65538_0InputPortDescriptors, 1, TestCustomNode_VendorTagWriteNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestCustomNode_VendorTagWriteLinks[] =
{
    {{0, 0, 0, 0}, 1, TestCustomNode_VendorTagWriteLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestCustomNode_VendorTagWriteLink1DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable}, {0}},
    {{255, 0, 0, 0}, 1, TestCustomNode_VendorTagWriteLink2DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable}, {0}},
    {{65538, 0, 8, 0}, 1, TestCustomNode_VendorTagWriteLink3DestDescriptors, {0}, {0}},
};

enum TestCustomNodePipelineIds
{
    VendorTagWrite = 0,
};

static ChiPipelineTargetCreateDescriptor TestCustomNode_pipelines[] =
{
    {"VendorTagWrite", { 0, 4, TestCustomNode_VendorTagWriteNodes, 4, TestCustomNode_VendorTagWriteLinks, 1}, {1, TestCustomNode_VendorTagWrite_sink_TargetDescriptors}, {0, NULL}},  // VendorTagWrite
};

/*==================== USECASE: TestMemcpy =======================*/

static ChiBufferFormat TestMemcpy_TARGET_BUFFER_PREVIEW_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiTarget TestMemcpy_TARGET_BUFFER_PREVIEW_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestMemcpy_TARGET_BUFFER_PREVIEW_formats,
    NULL
}; // TARGET_BUFFER_PREVIEW

static ChiTarget* TestMemcpy_Targets[] =
{
	&TestMemcpy_TARGET_BUFFER_PREVIEW_target
};

/*****************************Pipeline PreviewWithMemcpy***************************/

static ChiLinkNodeDescriptor TestMemcpy_PreviewWithMemcpyLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestMemcpy_PreviewWithMemcpyLink1DestDescriptors[] =
{
    {255, 0, 0, 0}, // ChiExternalNode
};

static ChiLinkNodeDescriptor TestMemcpy_PreviewWithMemcpyLink2DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestMemcpy_PreviewWithMemcpyLink3DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestMemcpy_PreviewWithMemcpy_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_PREVIEW", &TestMemcpy_TARGET_BUFFER_PREVIEW_target, {65538, 0, 8}}, // TARGET_BUFFER_PREVIEW
};

static ChiOutputPortDescriptor TestMemcpy_PreviewWithMemcpyNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestMemcpy_PreviewWithMemcpyNode255_0InputPortDescriptors[] =
{
    {0, 0, 0}, // ChiNodeInputPortFull
};

static ChiOutputPortDescriptor TestMemcpy_PreviewWithMemcpyNode255_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // ChiNodeOutputPortFull
};

static ChiInputPortDescriptor TestMemcpy_PreviewWithMemcpyNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestMemcpy_PreviewWithMemcpyNode65536_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // IFEOutputPortFull
};

static ChiInputPortDescriptor TestMemcpy_PreviewWithMemcpyNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestMemcpy_PreviewWithMemcpyNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiNodeProperty TestMemcpy_PreviewWithMemcpy_node255_0_properties[] =
{
    {1, "com.qti.node.memcpy"},
    {9, "0"},
};

static ChiNode TestMemcpy_PreviewWithMemcpyNodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, TestMemcpy_PreviewWithMemcpyNode0_0OutputPortDescriptors }, 0},
    {TestMemcpy_PreviewWithMemcpy_node255_0_properties, 255, 0, { 1, TestMemcpy_PreviewWithMemcpyNode255_0InputPortDescriptors, 1, TestMemcpy_PreviewWithMemcpyNode255_0OutputPortDescriptors }, 2},
    {NULL, 65536, 0, { 1, TestMemcpy_PreviewWithMemcpyNode65536_0InputPortDescriptors, 1, TestMemcpy_PreviewWithMemcpyNode65536_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 1, TestMemcpy_PreviewWithMemcpyNode65538_0InputPortDescriptors, 1, TestMemcpy_PreviewWithMemcpyNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestMemcpy_PreviewWithMemcpyLinks[] =
{
    {{0, 0, 0, 0}, 1, TestMemcpy_PreviewWithMemcpyLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestMemcpy_PreviewWithMemcpyLink1DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable}, {0}},
    {{255, 0, 0, 0}, 1, TestMemcpy_PreviewWithMemcpyLink2DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable}, {0}},
    {{65538, 0, 8, 0}, 1, TestMemcpy_PreviewWithMemcpyLink3DestDescriptors, {0}, {0}},
};

/*****************************Pipeline PreviewFromMemcpy***************************/

static ChiLinkNodeDescriptor TestMemcpy_PreviewFromMemcpyLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestMemcpy_PreviewFromMemcpyLink1DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestMemcpy_PreviewFromMemcpyLink2DestDescriptors[] =
{
    {255, 0, 0, 0}, // ChiExternalNode
};

static ChiLinkNodeDescriptor TestMemcpy_PreviewFromMemcpyLink3DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestMemcpy_PreviewFromMemcpy_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_PREVIEW", &TestMemcpy_TARGET_BUFFER_PREVIEW_target, {255, 0, 0}}, // TARGET_BUFFER_PREVIEW
};

static ChiOutputPortDescriptor TestMemcpy_PreviewFromMemcpyNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestMemcpy_PreviewFromMemcpyNode255_0InputPortDescriptors[] =
{
    {0, 0, 0}, // ChiNodeInputPortFull
};

static ChiOutputPortDescriptor TestMemcpy_PreviewFromMemcpyNode255_0OutputPortDescriptors[] =
{
    {0, 1, 1, 0, 0, NULL}, // ChiNodeOutputPortFull
};

static ChiInputPortDescriptor TestMemcpy_PreviewFromMemcpyNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestMemcpy_PreviewFromMemcpyNode65536_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // IFEOutputPortFull
};

static ChiInputPortDescriptor TestMemcpy_PreviewFromMemcpyNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestMemcpy_PreviewFromMemcpyNode65538_0OutputPortDescriptors[] =
{
    {8, 0, 0, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiNodeProperty TestMemcpy_PreviewFromMemcpy_node255_0_properties[] =
{
    {1, "com.qti.node.memcpy"},
    {9, "0"},
};

static ChiNode TestMemcpy_PreviewFromMemcpyNodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, TestMemcpy_PreviewFromMemcpyNode0_0OutputPortDescriptors }, 0},
    {TestMemcpy_PreviewFromMemcpy_node255_0_properties, 255, 0, { 1, TestMemcpy_PreviewFromMemcpyNode255_0InputPortDescriptors, 1, TestMemcpy_PreviewFromMemcpyNode255_0OutputPortDescriptors }, 2},
    {NULL, 65536, 0, { 1, TestMemcpy_PreviewFromMemcpyNode65536_0InputPortDescriptors, 1, TestMemcpy_PreviewFromMemcpyNode65536_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 1, TestMemcpy_PreviewFromMemcpyNode65538_0InputPortDescriptors, 1, TestMemcpy_PreviewFromMemcpyNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestMemcpy_PreviewFromMemcpyLinks[] =
{
    {{0, 0, 0, 0}, 1, TestMemcpy_PreviewFromMemcpyLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestMemcpy_PreviewFromMemcpyLink1DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable}, {0}},
    {{65538, 0, 8, 0}, 1, TestMemcpy_PreviewFromMemcpyLink2DestDescriptors, {ChiFormatYUV420NV12, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable}, {0}},
    {{255, 0, 0, 0}, 1, TestMemcpy_PreviewFromMemcpyLink3DestDescriptors, {0}, {0}},
};

enum TestMemcpyPipelineIds
{
    PreviewWithMemcpy = 0,
    PreviewFromMemcpy = 1,
};

static ChiPipelineTargetCreateDescriptor TestMemcpy_pipelines[] =
{
    {"PreviewWithMemcpy", { 0, 4, TestMemcpy_PreviewWithMemcpyNodes, 4, TestMemcpy_PreviewWithMemcpyLinks, 1}, {1, TestMemcpy_PreviewWithMemcpy_sink_TargetDescriptors}, {0, NULL}},  // PreviewWithMemcpy
    {"PreviewFromMemcpy", { 0, 4, TestMemcpy_PreviewFromMemcpyNodes, 4, TestMemcpy_PreviewFromMemcpyLinks, 1}, {1, TestMemcpy_PreviewFromMemcpy_sink_TargetDescriptors}, {0, NULL}},  // PreviewFromMemcpy
};

/*==================== USECASE: TestOfflineIPE =======================*/

static ChiBufferFormat TestOfflineIPE_TARGET_BUFFER_IPE_INPUT_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiBufferFormat TestOfflineIPE_TARGET_BUFFER_OUTPUT_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiTarget TestOfflineIPE_TARGET_BUFFER_IPE_INPUT_target =
{
    ChiStreamTypeInput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestOfflineIPE_TARGET_BUFFER_IPE_INPUT_formats,
    NULL
}; // TARGET_BUFFER_IPE_INPUT

static ChiTarget TestOfflineIPE_TARGET_BUFFER_OUTPUT_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestOfflineIPE_TARGET_BUFFER_OUTPUT_formats,
    NULL
}; // TARGET_BUFFER_OUTPUT

static ChiTarget* TestOfflineIPE_Targets[] =
{
	&TestOfflineIPE_TARGET_BUFFER_IPE_INPUT_target,
	&TestOfflineIPE_TARGET_BUFFER_OUTPUT_target
};

/*****************************Pipeline IpeDisp***************************/

static ChiLinkNodeDescriptor TestOfflineIPE_IpeDispLink0DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestOfflineIPE_IpeDisp_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_OUTPUT", &TestOfflineIPE_TARGET_BUFFER_OUTPUT_target, {65538, 0, 8}}, // TARGET_BUFFER_OUTPUT
};

static ChiTargetPortDescriptor TestOfflineIPE_IpeDisp_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_IPE_INPUT", &TestOfflineIPE_TARGET_BUFFER_IPE_INPUT_target, {65538, 0, 0}}, // TARGET_BUFFER_IPE_INPUT
};

static ChiInputPortDescriptor TestOfflineIPE_IpeDispNode65538_0InputPortDescriptors[] =
{
    {0, 1, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestOfflineIPE_IpeDispNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiNode TestOfflineIPE_IpeDispNodes[] =
{
    {NULL, 65538, 0, { 1, TestOfflineIPE_IpeDispNode65538_0InputPortDescriptors, 1, TestOfflineIPE_IpeDispNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestOfflineIPE_IpeDispLinks[] =
{
    {{65538, 0, 8, 0}, 1, TestOfflineIPE_IpeDispLink0DestDescriptors, {0}, {0}},
};

/*****************************Pipeline IpeVideo***************************/

static ChiLinkNodeDescriptor TestOfflineIPE_IpeVideoLink0DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestOfflineIPE_IpeVideo_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_OUTPUT", &TestOfflineIPE_TARGET_BUFFER_OUTPUT_target, {65538, 0, 9}}, // TARGET_BUFFER_OUTPUT
};

static ChiTargetPortDescriptor TestOfflineIPE_IpeVideo_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_IPE_INPUT", &TestOfflineIPE_TARGET_BUFFER_IPE_INPUT_target, {65538, 0, 0}}, // TARGET_BUFFER_IPE_INPUT
};

static ChiInputPortDescriptor TestOfflineIPE_IpeVideoNode65538_0InputPortDescriptors[] =
{
    {0, 1, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestOfflineIPE_IpeVideoNode65538_0OutputPortDescriptors[] =
{
    {9, 1, 1, 0, 0, NULL}, // IPEOutputPortVideo
};

static ChiNode TestOfflineIPE_IpeVideoNodes[] =
{
    {NULL, 65538, 0, { 1, TestOfflineIPE_IpeVideoNode65538_0InputPortDescriptors, 1, TestOfflineIPE_IpeVideoNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestOfflineIPE_IpeVideoLinks[] =
{
    {{65538, 0, 9, 0}, 1, TestOfflineIPE_IpeVideoLink0DestDescriptors, {0}, {0}},
};

enum TestOfflineIPEPipelineIds
{
    IpeDisp = 0,
    IpeVideo = 1,
};

static ChiPipelineTargetCreateDescriptor TestOfflineIPE_pipelines[] =
{
    {"IpeDisp", { 0, 1, TestOfflineIPE_IpeDispNodes, 1, TestOfflineIPE_IpeDispLinks, 0}, {1, TestOfflineIPE_IpeDisp_sink_TargetDescriptors}, {1, TestOfflineIPE_IpeDisp_source_TargetDescriptors}},  // IpeDisp
    {"IpeVideo", { 0, 1, TestOfflineIPE_IpeVideoNodes, 1, TestOfflineIPE_IpeVideoLinks, 0}, {1, TestOfflineIPE_IpeVideo_sink_TargetDescriptors}, {1, TestOfflineIPE_IpeVideo_source_TargetDescriptors}},  // IpeVideo
};

/*==================== USECASE: TestOfflineJPEG =======================*/

static ChiBufferFormat TestOfflineJPEG_TARGET_BUFFER_JPEG_INPUT_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiBufferFormat TestOfflineJPEG_TARGET_BUFFER_SNAPSHOT_formats[] =
{
    ChiFormatBlob,
};

static ChiTarget TestOfflineJPEG_TARGET_BUFFER_JPEG_INPUT_target =
{
    ChiStreamTypeInput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestOfflineJPEG_TARGET_BUFFER_JPEG_INPUT_formats,
    NULL
}; // TARGET_BUFFER_JPEG_INPUT

static ChiTarget TestOfflineJPEG_TARGET_BUFFER_SNAPSHOT_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    TestOfflineJPEG_TARGET_BUFFER_SNAPSHOT_formats,
    NULL
}; // TARGET_BUFFER_SNAPSHOT

static ChiTarget* TestOfflineJPEG_Targets[] =
{
	&TestOfflineJPEG_TARGET_BUFFER_JPEG_INPUT_target,
	&TestOfflineJPEG_TARGET_BUFFER_SNAPSHOT_target
};

/*****************************Pipeline JpegSnapshot***************************/

static ChiLinkNodeDescriptor TestOfflineJPEG_JpegSnapshotLink0DestDescriptors[] =
{
    {6, 0, 0, 0}, // JPEGAggregator
};

static ChiLinkNodeDescriptor TestOfflineJPEG_JpegSnapshotLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestOfflineJPEG_JpegSnapshot_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_SNAPSHOT", &TestOfflineJPEG_TARGET_BUFFER_SNAPSHOT_target, {6, 0, 1}}, // TARGET_BUFFER_SNAPSHOT
};

static ChiTargetPortDescriptor TestOfflineJPEG_JpegSnapshot_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_JPEG_INPUT", &TestOfflineJPEG_TARGET_BUFFER_JPEG_INPUT_target, {65537, 0, 0}}, // TARGET_BUFFER_JPEG_INPUT
};

static ChiInputPortDescriptor TestOfflineJPEG_JpegSnapshotNode6_0InputPortDescriptors[] =
{
    {0, 0, 0}, // JPEGAggregatorInputPort0
};

static ChiOutputPortDescriptor TestOfflineJPEG_JpegSnapshotNode6_0OutputPortDescriptors[] =
{
    {1, 1, 1, 0, 0, NULL}, // JPEGAggregatorOutputPort0
};

static ChiInputPortDescriptor TestOfflineJPEG_JpegSnapshotNode65537_0InputPortDescriptors[] =
{
    {0, 1, 0}, // JPEGInputPort0
};

static ChiOutputPortDescriptor TestOfflineJPEG_JpegSnapshotNode65537_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // JPEGOutputPort0
};

static ChiNode TestOfflineJPEG_JpegSnapshotNodes[] =
{
    {NULL, 6, 0, { 1, TestOfflineJPEG_JpegSnapshotNode6_0InputPortDescriptors, 1, TestOfflineJPEG_JpegSnapshotNode6_0OutputPortDescriptors }, 0},
    {NULL, 65537, 0, { 1, TestOfflineJPEG_JpegSnapshotNode65537_0InputPortDescriptors, 1, TestOfflineJPEG_JpegSnapshotNode65537_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestOfflineJPEG_JpegSnapshotLinks[] =
{
    {{65537, 0, 1, 0}, 1, TestOfflineJPEG_JpegSnapshotLink0DestDescriptors, {ChiFormatYUV420NV12, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable|BufferMemFlagCache}, {0}},
    {{6, 0, 1, 0}, 1, TestOfflineJPEG_JpegSnapshotLink1DestDescriptors, {0}, {0}},
};

enum TestOfflineJPEGPipelineIds
{
    JpegSnapshot = 0,
};

static ChiPipelineTargetCreateDescriptor TestOfflineJPEG_pipelines[] =
{
    {"JpegSnapshot", { 0, 2, TestOfflineJPEG_JpegSnapshotNodes, 2, TestOfflineJPEG_JpegSnapshotLinks, 0}, {1, TestOfflineJPEG_JpegSnapshot_sink_TargetDescriptors}, {1, TestOfflineJPEG_JpegSnapshot_source_TargetDescriptors}},  // JpegSnapshot
};

/*==================== USECASE: TestOfflineBPSIPE =======================*/

static ChiBufferFormat TestOfflineBPSIPE_TARGET_BUFFER_RAW_IN_formats[] =
{
    ChiFormatRawMIPI,
    ChiFormatRawPlain16,
};

static ChiBufferFormat TestOfflineBPSIPE_TARGET_BUFFER_SNAPSHOT_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiTarget TestOfflineBPSIPE_TARGET_BUFFER_RAW_IN_target =
{
    ChiStreamTypeInput,
    {
        0,
        0,
        4608,
        2592
    },
    2,
    TestOfflineBPSIPE_TARGET_BUFFER_RAW_IN_formats,
    NULL
}; // TARGET_BUFFER_RAW_IN

static ChiTarget TestOfflineBPSIPE_TARGET_BUFFER_SNAPSHOT_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4608,
        2592
    },
    3,
    TestOfflineBPSIPE_TARGET_BUFFER_SNAPSHOT_formats,
    NULL
}; // TARGET_BUFFER_SNAPSHOT

static ChiTarget* TestOfflineBPSIPE_Targets[] =
{
	&TestOfflineBPSIPE_TARGET_BUFFER_RAW_IN_target,
	&TestOfflineBPSIPE_TARGET_BUFFER_SNAPSHOT_target
};

/*****************************Pipeline OfflineBPSFull***************************/

static ChiLinkNodeDescriptor TestOfflineBPSIPE_OfflineBPSFullLink0DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestOfflineBPSIPE_OfflineBPSFull_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_SNAPSHOT", &TestOfflineBPSIPE_TARGET_BUFFER_SNAPSHOT_target, {65539, 0, 1}}, // TARGET_BUFFER_SNAPSHOT
};

static ChiTargetPortDescriptor TestOfflineBPSIPE_OfflineBPSFull_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW_IN", &TestOfflineBPSIPE_TARGET_BUFFER_RAW_IN_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW_IN
};

static ChiInputPortDescriptor TestOfflineBPSIPE_OfflineBPSFullNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor TestOfflineBPSIPE_OfflineBPSFullNode65539_0OutputPortDescriptors[] =
{
    {1, 1, 1, 0, 0, NULL}, // BPSOutputPortFull
};

static ChiNode TestOfflineBPSIPE_OfflineBPSFullNodes[] =
{
    {NULL, 65539, 0, { 1, TestOfflineBPSIPE_OfflineBPSFullNode65539_0InputPortDescriptors, 1, TestOfflineBPSIPE_OfflineBPSFullNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestOfflineBPSIPE_OfflineBPSFullLinks[] =
{
    {{65539, 0, 1, 0}, 1, TestOfflineBPSIPE_OfflineBPSFullLink0DestDescriptors, {0}, {0}},
};

/*****************************Pipeline OfflineBPSIPEDisp***************************/

static ChiLinkNodeDescriptor TestOfflineBPSIPE_OfflineBPSIPEDispLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestOfflineBPSIPE_OfflineBPSIPEDispLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestOfflineBPSIPE_OfflineBPSIPEDisp_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_SNAPSHOT", &TestOfflineBPSIPE_TARGET_BUFFER_SNAPSHOT_target, {65538, 0, 8}}, // TARGET_BUFFER_SNAPSHOT
};

static ChiTargetPortDescriptor TestOfflineBPSIPE_OfflineBPSIPEDisp_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW_IN", &TestOfflineBPSIPE_TARGET_BUFFER_RAW_IN_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW_IN
};

static ChiInputPortDescriptor TestOfflineBPSIPE_OfflineBPSIPEDispNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestOfflineBPSIPE_OfflineBPSIPEDispNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiInputPortDescriptor TestOfflineBPSIPE_OfflineBPSIPEDispNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor TestOfflineBPSIPE_OfflineBPSIPEDispNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
};

static ChiNode TestOfflineBPSIPE_OfflineBPSIPEDispNodes[] =
{
    {NULL, 65538, 0, { 1, TestOfflineBPSIPE_OfflineBPSIPEDispNode65538_0InputPortDescriptors, 1, TestOfflineBPSIPE_OfflineBPSIPEDispNode65538_0OutputPortDescriptors }, 0},
    {NULL, 65539, 0, { 1, TestOfflineBPSIPE_OfflineBPSIPEDispNode65539_0InputPortDescriptors, 1, TestOfflineBPSIPE_OfflineBPSIPEDispNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestOfflineBPSIPE_OfflineBPSIPEDispLinks[] =
{
    {{65539, 0, 1, 0}, 1, TestOfflineBPSIPE_OfflineBPSIPEDispLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, TestOfflineBPSIPE_OfflineBPSIPEDispLink1DestDescriptors, {0}, {0}},
};

enum TestOfflineBPSIPEPipelineIds
{
    OfflineBPSFull = 0,
    OfflineBPSIPEDisp = 1,
};

static ChiPipelineTargetCreateDescriptor TestOfflineBPSIPE_pipelines[] =
{
    {"OfflineBPSFull", { 0, 1, TestOfflineBPSIPE_OfflineBPSFullNodes, 1, TestOfflineBPSIPE_OfflineBPSFullLinks, 0}, {1, TestOfflineBPSIPE_OfflineBPSFull_sink_TargetDescriptors}, {1, TestOfflineBPSIPE_OfflineBPSFull_source_TargetDescriptors}},  // OfflineBPSFull
    {"OfflineBPSIPEDisp", { 0, 2, TestOfflineBPSIPE_OfflineBPSIPEDispNodes, 2, TestOfflineBPSIPE_OfflineBPSIPEDispLinks, 0}, {1, TestOfflineBPSIPE_OfflineBPSIPEDisp_sink_TargetDescriptors}, {1, TestOfflineBPSIPE_OfflineBPSIPEDisp_source_TargetDescriptors}},  // OfflineBPSIPEDisp
};

/*==================== USECASE: TestZSLUseCase =======================*/

static ChiBufferFormat TestZSLUseCase_TARGET_BUFFER_PREVIEW_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiBufferFormat TestZSLUseCase_TARGET_BUFFER_RAW_formats[] =
{
    ChiFormatRawMIPI,
    ChiFormatRawPlain16,
};

static ChiBufferFormat TestZSLUseCase_TARGET_BUFFER_SNAPSHOT_formats[] =
{
    ChiFormatJpeg,
    ChiFormatYUV420NV12,
};

static ChiTarget TestZSLUseCase_TARGET_BUFFER_PREVIEW_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestZSLUseCase_TARGET_BUFFER_PREVIEW_formats,
    NULL
}; // TARGET_BUFFER_PREVIEW

static ChiTarget TestZSLUseCase_TARGET_BUFFER_RAW_target =
{
    ChiStreamTypeBidirectional,
    {
        0,
        0,
        5488,
        4112
    },
    2,
    TestZSLUseCase_TARGET_BUFFER_RAW_formats,
    NULL
}; // TARGET_BUFFER_RAW

static ChiTarget TestZSLUseCase_TARGET_BUFFER_SNAPSHOT_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    2,
    TestZSLUseCase_TARGET_BUFFER_SNAPSHOT_formats,
    NULL
}; // TARGET_BUFFER_SNAPSHOT

static ChiTarget* TestZSLUseCase_Targets[] =
{
	&TestZSLUseCase_TARGET_BUFFER_PREVIEW_target,
	&TestZSLUseCase_TARGET_BUFFER_RAW_target,
	&TestZSLUseCase_TARGET_BUFFER_SNAPSHOT_target
};

/*****************************Pipeline ZSLPreviewRaw***************************/

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawLink1DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawLink2DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawLink3DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLPreviewRaw_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_PREVIEW", &TestZSLUseCase_TARGET_BUFFER_PREVIEW_target, {65538, 0, 8}}, // TARGET_BUFFER_PREVIEW
    {"TARGET_BUFFER_RAW", &TestZSLUseCase_TARGET_BUFFER_RAW_target, {65536, 0, 8}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLPreviewRawNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawNode65536_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // IFEOutputPortFull
    {8, 1, 1, 0, 0, NULL}, // IFEOutputPortRDI0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLPreviewRawNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiNode TestZSLUseCase_ZSLPreviewRawNodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, TestZSLUseCase_ZSLPreviewRawNode0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 0, { 1, TestZSLUseCase_ZSLPreviewRawNode65536_0InputPortDescriptors, 2, TestZSLUseCase_ZSLPreviewRawNode65536_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 1, TestZSLUseCase_ZSLPreviewRawNode65538_0InputPortDescriptors, 1, TestZSLUseCase_ZSLPreviewRawNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestZSLUseCase_ZSLPreviewRawLinks[] =
{
    {{0, 0, 0, 0}, 1, TestZSLUseCase_ZSLPreviewRawLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestZSLUseCase_ZSLPreviewRawLink1DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, TestZSLUseCase_ZSLPreviewRawLink2DestDescriptors, {0}, {0}},
    {{65536, 0, 8, 0}, 1, TestZSLUseCase_ZSLPreviewRawLink3DestDescriptors, {0}, {0}},
};

/*****************************Pipeline ZSLPreviewRawWithTestGen***************************/

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithTestGenLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithTestGenLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithTestGenLink2DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLPreviewRawWithTestGen_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_PREVIEW", &TestZSLUseCase_TARGET_BUFFER_PREVIEW_target, {65538, 0, 8}}, // TARGET_BUFFER_PREVIEW
    {"TARGET_BUFFER_RAW", &TestZSLUseCase_TARGET_BUFFER_RAW_target, {65536, 0, 8}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithTestGenNode65536_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // IFEOutputPortFull
    {8, 1, 1, 0, 0, NULL}, // IFEOutputPortRDI0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithTestGenNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithTestGenNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiNode TestZSLUseCase_ZSLPreviewRawWithTestGenNodes[] =
{
    {NULL, 65536, 0, { 0, NULL, 2, TestZSLUseCase_ZSLPreviewRawWithTestGenNode65536_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 1, TestZSLUseCase_ZSLPreviewRawWithTestGenNode65538_0InputPortDescriptors, 1, TestZSLUseCase_ZSLPreviewRawWithTestGenNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestZSLUseCase_ZSLPreviewRawWithTestGenLinks[] =
{
    {{65536, 0, 0, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithTestGenLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithTestGenLink1DestDescriptors, {0}, {0}},
    {{65536, 0, 8, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithTestGenLink2DestDescriptors, {0}, {0}},
};

/*****************************Pipeline ZSLSnapshotJpegWithDS***************************/

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSLink1DestDescriptors[] =
{
    {65538, 0, 1, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSLink2DestDescriptors[] =
{
    {65538, 0, 2, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSLink3DestDescriptors[] =
{
    {65538, 0, 3, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSLink4DestDescriptors[] =
{
    {65537, 0, 0, 0}, // JPEG
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSLink5DestDescriptors[] =
{
    {6, 0, 0, 0}, // JPEGAggregator
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSLink6DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDS_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_SNAPSHOT", &TestZSLUseCase_TARGET_BUFFER_SNAPSHOT_target, {6, 0, 1}}, // TARGET_BUFFER_SNAPSHOT
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDS_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &TestZSLUseCase_TARGET_BUFFER_RAW_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSNode6_0InputPortDescriptors[] =
{
    {0, 0, 0}, // JPEGAggregatorInputPort0
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSNode6_0OutputPortDescriptors[] =
{
    {1, 1, 1, 0, 0, NULL}, // JPEGAggregatorOutputPort0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSNode65537_0InputPortDescriptors[] =
{
    {0, 0, 0}, // JPEGInputPort0
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSNode65537_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // JPEGOutputPort0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
    {1, 0, 0}, // IPEInputPortDS4
    {2, 0, 0}, // IPEInputPortDS16
    {3, 0, 0}, // IPEInputPortDS64
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSNode65538_0OutputPortDescriptors[] =
{
    {8, 0, 0, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegWithDSNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
    {2, 0, 0, 0, 0, NULL}, // BPSOutputPortDS4
    {3, 0, 0, 0, 0, NULL}, // BPSOutputPortDS16
    {4, 0, 0, 0, 0, NULL}, // BPSOutputPortDS64
};

static ChiNode TestZSLUseCase_ZSLSnapshotJpegWithDSNodes[] =
{
    {NULL, 6, 0, { 1, TestZSLUseCase_ZSLSnapshotJpegWithDSNode6_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSNode6_0OutputPortDescriptors }, 0},
    {NULL, 65537, 0, { 1, TestZSLUseCase_ZSLSnapshotJpegWithDSNode65537_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSNode65537_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 4, TestZSLUseCase_ZSLSnapshotJpegWithDSNode65538_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSNode65538_0OutputPortDescriptors }, 0},
    {NULL, 65539, 0, { 1, TestZSLUseCase_ZSLSnapshotJpegWithDSNode65539_0InputPortDescriptors, 4, TestZSLUseCase_ZSLSnapshotJpegWithDSNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestZSLUseCase_ZSLSnapshotJpegWithDSLinks[] =
{
    {{65539, 0, 1, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65539, 0, 2, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSLink1DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65539, 0, 3, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSLink2DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65539, 0, 4, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSLink3DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSLink4DestDescriptors, {ChiFormatYUV420NV12, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65537, 0, 1, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSLink5DestDescriptors, {ChiFormatYUV420NV12, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable|BufferMemFlagCache}, {0}},
    {{6, 0, 1, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegWithDSLink6DestDescriptors, {0}, {0}},
};

/*****************************Pipeline ZSLSnapshotJpeg***************************/

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegLink1DestDescriptors[] =
{
    {65537, 0, 0, 0}, // JPEG
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegLink2DestDescriptors[] =
{
    {6, 0, 0, 0}, // JPEGAggregator
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotJpegLink3DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLSnapshotJpeg_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_SNAPSHOT", &TestZSLUseCase_TARGET_BUFFER_SNAPSHOT_target, {6, 0, 1}}, // TARGET_BUFFER_SNAPSHOT
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLSnapshotJpeg_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &TestZSLUseCase_TARGET_BUFFER_RAW_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegNode6_0InputPortDescriptors[] =
{
    {0, 0, 0}, // JPEGAggregatorInputPort0
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegNode6_0OutputPortDescriptors[] =
{
    {1, 1, 1, 0, 0, NULL}, // JPEGAggregatorOutputPort0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegNode65537_0InputPortDescriptors[] =
{
    {0, 0, 0}, // JPEGInputPort0
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegNode65537_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // JPEGOutputPort0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegNode65538_0OutputPortDescriptors[] =
{
    {8, 0, 0, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotJpegNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
};

static ChiNode TestZSLUseCase_ZSLSnapshotJpegNodes[] =
{
    {NULL, 6, 0, { 1, TestZSLUseCase_ZSLSnapshotJpegNode6_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotJpegNode6_0OutputPortDescriptors }, 0},
    {NULL, 65537, 0, { 1, TestZSLUseCase_ZSLSnapshotJpegNode65537_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotJpegNode65537_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 1, TestZSLUseCase_ZSLSnapshotJpegNode65538_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotJpegNode65538_0OutputPortDescriptors }, 0},
    {NULL, 65539, 0, { 1, TestZSLUseCase_ZSLSnapshotJpegNode65539_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotJpegNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestZSLUseCase_ZSLSnapshotJpegLinks[] =
{
    {{65539, 0, 1, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegLink1DestDescriptors, {ChiFormatYUV420NV12, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65537, 0, 1, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegLink2DestDescriptors, {ChiFormatYUV420NV12, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable|BufferMemFlagCache}, {0}},
    {{6, 0, 1, 0}, 1, TestZSLUseCase_ZSLSnapshotJpegLink3DestDescriptors, {0}, {0}},
};

/*****************************Pipeline ZSLSnapshotYUVWithDS***************************/

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDSLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDSLink1DestDescriptors[] =
{
    {65538, 0, 1, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDSLink2DestDescriptors[] =
{
    {65538, 0, 2, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDSLink3DestDescriptors[] =
{
    {65538, 0, 3, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDSLink4DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDS_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_SNAPSHOT", &TestZSLUseCase_TARGET_BUFFER_SNAPSHOT_target, {65538, 0, 8}}, // TARGET_BUFFER_SNAPSHOT
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDS_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &TestZSLUseCase_TARGET_BUFFER_RAW_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDSNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
    {1, 0, 0}, // IPEInputPortDS4
    {2, 0, 0}, // IPEInputPortDS16
    {3, 0, 0}, // IPEInputPortDS64
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDSNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDSNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotYUVWithDSNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
    {2, 0, 0, 0, 0, NULL}, // BPSOutputPortDS4
    {3, 0, 0, 0, 0, NULL}, // BPSOutputPortDS16
    {4, 0, 0, 0, 0, NULL}, // BPSOutputPortDS64
};

static ChiNode TestZSLUseCase_ZSLSnapshotYUVWithDSNodes[] =
{
    {NULL, 65538, 0, { 4, TestZSLUseCase_ZSLSnapshotYUVWithDSNode65538_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotYUVWithDSNode65538_0OutputPortDescriptors }, 0},
    {NULL, 65539, 0, { 1, TestZSLUseCase_ZSLSnapshotYUVWithDSNode65539_0InputPortDescriptors, 4, TestZSLUseCase_ZSLSnapshotYUVWithDSNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestZSLUseCase_ZSLSnapshotYUVWithDSLinks[] =
{
    {{65539, 0, 1, 0}, 1, TestZSLUseCase_ZSLSnapshotYUVWithDSLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65539, 0, 2, 0}, 1, TestZSLUseCase_ZSLSnapshotYUVWithDSLink1DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65539, 0, 3, 0}, 1, TestZSLUseCase_ZSLSnapshotYUVWithDSLink2DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65539, 0, 4, 0}, 1, TestZSLUseCase_ZSLSnapshotYUVWithDSLink3DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, TestZSLUseCase_ZSLSnapshotYUVWithDSLink4DestDescriptors, {0}, {0}},
};

/*****************************Pipeline ZSLSnapshotYUV***************************/

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotYUVLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLSnapshotYUVLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLSnapshotYUV_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_SNAPSHOT", &TestZSLUseCase_TARGET_BUFFER_SNAPSHOT_target, {65538, 0, 8}}, // TARGET_BUFFER_SNAPSHOT
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLSnapshotYUV_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &TestZSLUseCase_TARGET_BUFFER_RAW_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotYUVNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotYUVNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLSnapshotYUVNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLSnapshotYUVNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
};

static ChiNode TestZSLUseCase_ZSLSnapshotYUVNodes[] =
{
    {NULL, 65538, 0, { 1, TestZSLUseCase_ZSLSnapshotYUVNode65538_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotYUVNode65538_0OutputPortDescriptors }, 0},
    {NULL, 65539, 0, { 1, TestZSLUseCase_ZSLSnapshotYUVNode65539_0InputPortDescriptors, 1, TestZSLUseCase_ZSLSnapshotYUVNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestZSLUseCase_ZSLSnapshotYUVLinks[] =
{
    {{65539, 0, 1, 0}, 1, TestZSLUseCase_ZSLSnapshotYUVLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, TestZSLUseCase_ZSLSnapshotYUVLink1DestDescriptors, {0}, {0}},
};

/*****************************Pipeline ZSLPreviewRawWithStats***************************/

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink1DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink2DestDescriptors[] =
{
    {65538, 0, 1, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink3DestDescriptors[] =
{
    {65538, 0, 2, 0}, // IPE
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink4DestDescriptors[] =
{
    {3, 0, 0, 0}, // SinkNoBuffer
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink5DestDescriptors[] =
{
    {1, 0, 0, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink6DestDescriptors[] =
{
    {1, 0, 1, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink7DestDescriptors[] =
{
    {1, 0, 2, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink8DestDescriptors[] =
{
    {1, 0, 7, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink9DestDescriptors[] =
{
    {5, 0, 4, 0}, // AutoFocus
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink10DestDescriptors[] =
{
    {5, 0, 8, 0}, // AutoFocus
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink11DestDescriptors[] =
{
    {3, 3, 0, 0}, // SinkNoBuffer
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink12DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsLink13DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStats_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_PREVIEW", &TestZSLUseCase_TARGET_BUFFER_PREVIEW_target, {65538, 0, 8}}, // TARGET_BUFFER_PREVIEW
    {"TARGET_BUFFER_RAW", &TestZSLUseCase_TARGET_BUFFER_RAW_target, {65536, 0, 9}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsNode1_0InputPortDescriptors[] =
{
    {0, 0, 0}, // StatsProcessingInputPortHDRBE
    {1, 0, 0}, // StatsInputPortAWBBG
    {2, 0, 0}, // StatsInputPortHDRBHist
    {7, 0, 0}, // StatsInputPortRS
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsNode1_0OutputPortDescriptors[] =
{
    {0, 1, 0, 0, 0, NULL}, // StatsProcessingOutputPort0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsNode5_0InputPortDescriptors[] =
{
    {4, 0, 0}, // StatsInputPortBF
    {8, 0, 0}, // StatsInputPortRDIPDAF
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsNode5_0OutputPortDescriptors[] =
{
    {0, 1, 0, 0, 0, NULL}, // AutoFocusOutputPort0
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsNode65536_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // IFEOutputPortFull
    {1, 0, 0, 0, 0, NULL}, // IFEOutputPortDS4
    {2, 0, 0, 0, 0, NULL}, // IFEOutputPortDS16
    {17, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsHDRBE
    {21, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsAWBBG
    {18, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsHDRBHIST
    {12, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsRS
    {20, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsBF
    {8, 0, 0, 2, 0, NULL}, // IFEOutputPortRDI0
    {9, 1, 1, 0, 0, NULL}, // IFEOutputPortRDI1
};

static ChiInputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
    {1, 0, 0}, // IPEInputPortDS4
    {2, 0, 0}, // IPEInputPortDS16
};

static ChiOutputPortDescriptor TestZSLUseCase_ZSLPreviewRawWithStatsNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiNodeProperty TestZSLUseCase_ZSLPreviewRawWithStats_node0_0_properties[] =
{
    {1, "com.qti.stats.pdlib"},
};

static ChiNodeProperty TestZSLUseCase_ZSLPreviewRawWithStats_node1_0_properties[] =
{
    {1, "com.qti.stats.aec"},
    {1, "com.qti.stats.awb"},
    {1, "com.qti.stats.afd"},
};

static ChiNodeProperty TestZSLUseCase_ZSLPreviewRawWithStats_node5_0_properties[] =
{
    {1, "com.qti.stats.af"},
    {1, "com.qti.stats.pdlib"},
};

static ChiNode TestZSLUseCase_ZSLPreviewRawWithStatsNodes[] =
{
    {TestZSLUseCase_ZSLPreviewRawWithStats_node0_0_properties, 0, 0, { 0, NULL, 1, TestZSLUseCase_ZSLPreviewRawWithStatsNode0_0OutputPortDescriptors }, 1},
    {TestZSLUseCase_ZSLPreviewRawWithStats_node1_0_properties, 1, 0, { 4, TestZSLUseCase_ZSLPreviewRawWithStatsNode1_0InputPortDescriptors, 1, TestZSLUseCase_ZSLPreviewRawWithStatsNode1_0OutputPortDescriptors }, 3},
    {TestZSLUseCase_ZSLPreviewRawWithStats_node5_0_properties, 5, 0, { 2, TestZSLUseCase_ZSLPreviewRawWithStatsNode5_0InputPortDescriptors, 1, TestZSLUseCase_ZSLPreviewRawWithStatsNode5_0OutputPortDescriptors }, 2},
    {NULL, 65536, 0, { 1, TestZSLUseCase_ZSLPreviewRawWithStatsNode65536_0InputPortDescriptors, 10, TestZSLUseCase_ZSLPreviewRawWithStatsNode65536_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 3, TestZSLUseCase_ZSLPreviewRawWithStatsNode65538_0InputPortDescriptors, 1, TestZSLUseCase_ZSLPreviewRawWithStatsNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestZSLUseCase_ZSLPreviewRawWithStatsLinks[] =
{
    {{0, 0, 0, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink1DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65536, 0, 1, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink2DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65536, 0, 2, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink3DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{1, 0, 0, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink4DestDescriptors, {0}, {0}},
    {{65536, 0, 17, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink5DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 21, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink6DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 18, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink7DestDescriptors, {ChiFormatBlob, 3072, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 12, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink8DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 20, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink9DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 8, 2}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink10DestDescriptors, {ChiFormatBlob, 307200, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{5, 0, 0, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink11DestDescriptors, {0}, {0}},
    {{65538, 0, 8, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink12DestDescriptors, {0}, {0}},
    {{65536, 0, 9, 0}, 1, TestZSLUseCase_ZSLPreviewRawWithStatsLink13DestDescriptors, {0}, {0}},
};

enum TestZSLUseCasePipelineIds
{
    ZSLPreviewRaw = 0,
    ZSLPreviewRawWithTestGen = 1,
    ZSLSnapshotJpegWithDS = 2,
    ZSLSnapshotJpeg = 3,
    ZSLSnapshotYUVWithDS = 4,
    ZSLSnapshotYUV = 5,
    ZSLPreviewRawWithStats = 6,
};

static ChiPipelineTargetCreateDescriptor TestZSLUseCase_pipelines[] =
{
    {"ZSLPreviewRaw", { 0, 3, TestZSLUseCase_ZSLPreviewRawNodes, 4, TestZSLUseCase_ZSLPreviewRawLinks, 1}, {2, TestZSLUseCase_ZSLPreviewRaw_sink_TargetDescriptors}, {0, NULL}},  // ZSLPreviewRaw
    {"ZSLPreviewRawWithTestGen", { 0, 2, TestZSLUseCase_ZSLPreviewRawWithTestGenNodes, 3, TestZSLUseCase_ZSLPreviewRawWithTestGenLinks, 1}, {2, TestZSLUseCase_ZSLPreviewRawWithTestGen_sink_TargetDescriptors}, {0, NULL}},  // ZSLPreviewRawWithTestGen
    {"ZSLSnapshotJpegWithDS", { 0, 4, TestZSLUseCase_ZSLSnapshotJpegWithDSNodes, 7, TestZSLUseCase_ZSLSnapshotJpegWithDSLinks, 0}, {1, TestZSLUseCase_ZSLSnapshotJpegWithDS_sink_TargetDescriptors}, {1, TestZSLUseCase_ZSLSnapshotJpegWithDS_source_TargetDescriptors}},  // ZSLSnapshotJpegWithDS
    {"ZSLSnapshotJpeg", { 0, 4, TestZSLUseCase_ZSLSnapshotJpegNodes, 4, TestZSLUseCase_ZSLSnapshotJpegLinks, 0}, {1, TestZSLUseCase_ZSLSnapshotJpeg_sink_TargetDescriptors}, {1, TestZSLUseCase_ZSLSnapshotJpeg_source_TargetDescriptors}},  // ZSLSnapshotJpeg
    {"ZSLSnapshotYUVWithDS", { 0, 2, TestZSLUseCase_ZSLSnapshotYUVWithDSNodes, 5, TestZSLUseCase_ZSLSnapshotYUVWithDSLinks, 0}, {1, TestZSLUseCase_ZSLSnapshotYUVWithDS_sink_TargetDescriptors}, {1, TestZSLUseCase_ZSLSnapshotYUVWithDS_source_TargetDescriptors}},  // ZSLSnapshotYUVWithDS
    {"ZSLSnapshotYUV", { 0, 2, TestZSLUseCase_ZSLSnapshotYUVNodes, 2, TestZSLUseCase_ZSLSnapshotYUVLinks, 0}, {1, TestZSLUseCase_ZSLSnapshotYUV_sink_TargetDescriptors}, {1, TestZSLUseCase_ZSLSnapshotYUV_source_TargetDescriptors}},  // ZSLSnapshotYUV
    {"ZSLPreviewRawWithStats", { 0, 5, TestZSLUseCase_ZSLPreviewRawWithStatsNodes, 14, TestZSLUseCase_ZSLPreviewRawWithStatsLinks, 1}, {2, TestZSLUseCase_ZSLPreviewRawWithStats_sink_TargetDescriptors}, {0, NULL}},  // ZSLPreviewRawWithStats
};

/*==================== USECASE: TestOfflineIPESIMO =======================*/

static ChiBufferFormat TestOfflineIPESIMO_TARGET_BUFFER_IPE_INPUT_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiBufferFormat TestOfflineIPESIMO_TARGET_BUFFER_DISPLAY_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiBufferFormat TestOfflineIPESIMO_TARGET_BUFFER_VIDEO_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiTarget TestOfflineIPESIMO_TARGET_BUFFER_IPE_INPUT_target =
{
    ChiStreamTypeInput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestOfflineIPESIMO_TARGET_BUFFER_IPE_INPUT_formats,
    NULL
}; // TARGET_BUFFER_IPE_INPUT

static ChiTarget TestOfflineIPESIMO_TARGET_BUFFER_DISPLAY_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestOfflineIPESIMO_TARGET_BUFFER_DISPLAY_formats,
    NULL
}; // TARGET_BUFFER_DISPLAY

static ChiTarget TestOfflineIPESIMO_TARGET_BUFFER_VIDEO_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestOfflineIPESIMO_TARGET_BUFFER_VIDEO_formats,
    NULL
}; // TARGET_BUFFER_VIDEO

static ChiTarget* TestOfflineIPESIMO_Targets[] =
{
	&TestOfflineIPESIMO_TARGET_BUFFER_IPE_INPUT_target,
	&TestOfflineIPESIMO_TARGET_BUFFER_DISPLAY_target,
	&TestOfflineIPESIMO_TARGET_BUFFER_VIDEO_target
};

/*****************************Pipeline OfflineDispVideo***************************/

static ChiLinkNodeDescriptor TestOfflineIPESIMO_OfflineDispVideoLink0DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestOfflineIPESIMO_OfflineDispVideoLink1DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestOfflineIPESIMO_OfflineDispVideo_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_DISPLAY", &TestOfflineIPESIMO_TARGET_BUFFER_DISPLAY_target, {65538, 0, 8}}, // TARGET_BUFFER_DISPLAY
    {"TARGET_BUFFER_VIDEO", &TestOfflineIPESIMO_TARGET_BUFFER_VIDEO_target, {65538, 0, 9}}, // TARGET_BUFFER_VIDEO
};

static ChiTargetPortDescriptor TestOfflineIPESIMO_OfflineDispVideo_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_IPE_INPUT", &TestOfflineIPESIMO_TARGET_BUFFER_IPE_INPUT_target, {65538, 0, 0}}, // TARGET_BUFFER_IPE_INPUT
};

static ChiInputPortDescriptor TestOfflineIPESIMO_OfflineDispVideoNode65538_0InputPortDescriptors[] =
{
    {0, 1, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor TestOfflineIPESIMO_OfflineDispVideoNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
    {9, 1, 1, 0, 0, NULL}, // IPEOutputPortVideo
};

static ChiNode TestOfflineIPESIMO_OfflineDispVideoNodes[] =
{
    {NULL, 65538, 0, { 1, TestOfflineIPESIMO_OfflineDispVideoNode65538_0InputPortDescriptors, 2, TestOfflineIPESIMO_OfflineDispVideoNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestOfflineIPESIMO_OfflineDispVideoLinks[] =
{
    {{65538, 0, 8, 0}, 1, TestOfflineIPESIMO_OfflineDispVideoLink0DestDescriptors, {0}, {0}},
    {{65538, 0, 9, 0}, 1, TestOfflineIPESIMO_OfflineDispVideoLink1DestDescriptors, {0}, {0}},
};

enum TestOfflineIPESIMOPipelineIds
{
    OfflineDispVideo = 0,
};

static ChiPipelineTargetCreateDescriptor TestOfflineIPESIMO_pipelines[] =
{
    {"OfflineDispVideo", { 0, 1, TestOfflineIPESIMO_OfflineDispVideoNodes, 2, TestOfflineIPESIMO_OfflineDispVideoLinks, 0}, {2, TestOfflineIPESIMO_OfflineDispVideo_sink_TargetDescriptors}, {1, TestOfflineIPESIMO_OfflineDispVideo_source_TargetDescriptors}},  // OfflineDispVideo
};

/*==================== USECASE: OfflineBPSSingleStatWithIPE =======================*/

static ChiBufferFormat OfflineBPSSingleStatWithIPE_TARGET_BUFFER_RAW_IN_formats[] =
{
    ChiFormatRawMIPI,
    ChiFormatRawPlain16,
};

static ChiBufferFormat OfflineBPSSingleStatWithIPE_TARGET_BUFFER_IPE_DISPLAY_formats[] =
{
    ChiFormatYUV420NV12,
};

static ChiBufferFormat OfflineBPSSingleStatWithIPE_TARGET_BUFFER_BPS_STAT_formats[] =
{
    ChiFormatBlob,
};

static ChiTarget OfflineBPSSingleStatWithIPE_TARGET_BUFFER_RAW_IN_target =
{
    ChiStreamTypeInput,
    {
        0,
        0,
        4096,
        2448
    },
    2,
    OfflineBPSSingleStatWithIPE_TARGET_BUFFER_RAW_IN_formats,
    NULL
}; // TARGET_BUFFER_RAW_IN

static ChiTarget OfflineBPSSingleStatWithIPE_TARGET_BUFFER_IPE_DISPLAY_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    OfflineBPSSingleStatWithIPE_TARGET_BUFFER_IPE_DISPLAY_formats,
    NULL
}; // TARGET_BUFFER_IPE_DISPLAY

static ChiTarget OfflineBPSSingleStatWithIPE_TARGET_BUFFER_BPS_STAT_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    OfflineBPSSingleStatWithIPE_TARGET_BUFFER_BPS_STAT_formats,
    NULL
}; // TARGET_BUFFER_BPS_STAT

static ChiTarget* OfflineBPSSingleStatWithIPE_Targets[] =
{
	&OfflineBPSSingleStatWithIPE_TARGET_BUFFER_RAW_IN_target,
	&OfflineBPSSingleStatWithIPE_TARGET_BUFFER_IPE_DISPLAY_target,
	&OfflineBPSSingleStatWithIPE_TARGET_BUFFER_BPS_STAT_target
};

/*****************************Pipeline BPSBGStats***************************/

static ChiLinkNodeDescriptor OfflineBPSSingleStatWithIPE_BPSBGStatsLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor OfflineBPSSingleStatWithIPE_BPSBGStatsLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor OfflineBPSSingleStatWithIPE_BPSBGStatsLink2DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor OfflineBPSSingleStatWithIPE_BPSBGStats_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_IPE_DISPLAY", &OfflineBPSSingleStatWithIPE_TARGET_BUFFER_IPE_DISPLAY_target, {65538, 0, 8}}, // TARGET_BUFFER_IPE_DISPLAY
    {"TARGET_BUFFER_BPS_STAT", &OfflineBPSSingleStatWithIPE_TARGET_BUFFER_BPS_STAT_target, {65539, 0, 5}}, // TARGET_BUFFER_BPS_STAT
};

static ChiTargetPortDescriptor OfflineBPSSingleStatWithIPE_BPSBGStats_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW_IN", &OfflineBPSSingleStatWithIPE_TARGET_BUFFER_RAW_IN_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW_IN
};

static ChiInputPortDescriptor OfflineBPSSingleStatWithIPE_BPSBGStatsNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor OfflineBPSSingleStatWithIPE_BPSBGStatsNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiInputPortDescriptor OfflineBPSSingleStatWithIPE_BPSBGStatsNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor OfflineBPSSingleStatWithIPE_BPSBGStatsNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
    {5, 1, 1, 0, 0, NULL}, // BPSOutputPortStatsBG
};

static ChiNode OfflineBPSSingleStatWithIPE_BPSBGStatsNodes[] =
{
    {NULL, 65538, 0, { 1, OfflineBPSSingleStatWithIPE_BPSBGStatsNode65538_0InputPortDescriptors, 1, OfflineBPSSingleStatWithIPE_BPSBGStatsNode65538_0OutputPortDescriptors }, 0},
    {NULL, 65539, 0, { 1, OfflineBPSSingleStatWithIPE_BPSBGStatsNode65539_0InputPortDescriptors, 2, OfflineBPSSingleStatWithIPE_BPSBGStatsNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink OfflineBPSSingleStatWithIPE_BPSBGStatsLinks[] =
{
    {{65539, 0, 1, 0}, 1, OfflineBPSSingleStatWithIPE_BPSBGStatsLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, OfflineBPSSingleStatWithIPE_BPSBGStatsLink1DestDescriptors, {0}, {0}},
    {{65539, 0, 5, 0}, 1, OfflineBPSSingleStatWithIPE_BPSBGStatsLink2DestDescriptors, {0}, {0}},
};

/*****************************Pipeline BPSHistStats***************************/

static ChiLinkNodeDescriptor OfflineBPSSingleStatWithIPE_BPSHistStatsLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor OfflineBPSSingleStatWithIPE_BPSHistStatsLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor OfflineBPSSingleStatWithIPE_BPSHistStatsLink2DestDescriptors[] =
{
    {2, 2, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor OfflineBPSSingleStatWithIPE_BPSHistStats_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_IPE_DISPLAY", &OfflineBPSSingleStatWithIPE_TARGET_BUFFER_IPE_DISPLAY_target, {65538, 0, 8}}, // TARGET_BUFFER_IPE_DISPLAY
    {"TARGET_BUFFER_BPS_STAT", &OfflineBPSSingleStatWithIPE_TARGET_BUFFER_BPS_STAT_target, {65539, 0, 6}}, // TARGET_BUFFER_BPS_STAT
};

static ChiTargetPortDescriptor OfflineBPSSingleStatWithIPE_BPSHistStats_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW_IN", &OfflineBPSSingleStatWithIPE_TARGET_BUFFER_RAW_IN_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW_IN
};

static ChiInputPortDescriptor OfflineBPSSingleStatWithIPE_BPSHistStatsNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor OfflineBPSSingleStatWithIPE_BPSHistStatsNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiInputPortDescriptor OfflineBPSSingleStatWithIPE_BPSHistStatsNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor OfflineBPSSingleStatWithIPE_BPSHistStatsNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
    {6, 1, 1, 0, 0, NULL}, // BPSOutputPortStatsBHist
};

static ChiNode OfflineBPSSingleStatWithIPE_BPSHistStatsNodes[] =
{
    {NULL, 65538, 0, { 1, OfflineBPSSingleStatWithIPE_BPSHistStatsNode65538_0InputPortDescriptors, 1, OfflineBPSSingleStatWithIPE_BPSHistStatsNode65538_0OutputPortDescriptors }, 0},
    {NULL, 65539, 0, { 1, OfflineBPSSingleStatWithIPE_BPSHistStatsNode65539_0InputPortDescriptors, 2, OfflineBPSSingleStatWithIPE_BPSHistStatsNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink OfflineBPSSingleStatWithIPE_BPSHistStatsLinks[] =
{
    {{65539, 0, 1, 0}, 1, OfflineBPSSingleStatWithIPE_BPSHistStatsLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, OfflineBPSSingleStatWithIPE_BPSHistStatsLink1DestDescriptors, {0}, {0}},
    {{65539, 0, 6, 0}, 1, OfflineBPSSingleStatWithIPE_BPSHistStatsLink2DestDescriptors, {0}, {0}},
};

enum OfflineBPSSingleStatWithIPEPipelineIds
{
    BPSBGStats = 0,
    BPSHistStats = 1,
};

static ChiPipelineTargetCreateDescriptor OfflineBPSSingleStatWithIPE_pipelines[] =
{
    {"BPSBGStats", { 0, 2, OfflineBPSSingleStatWithIPE_BPSBGStatsNodes, 3, OfflineBPSSingleStatWithIPE_BPSBGStatsLinks, 0}, {2, OfflineBPSSingleStatWithIPE_BPSBGStats_sink_TargetDescriptors}, {1, OfflineBPSSingleStatWithIPE_BPSBGStats_source_TargetDescriptors}},  // BPSBGStats
    {"BPSHistStats", { 0, 2, OfflineBPSSingleStatWithIPE_BPSHistStatsNodes, 3, OfflineBPSSingleStatWithIPE_BPSHistStatsLinks, 0}, {2, OfflineBPSSingleStatWithIPE_BPSHistStats_sink_TargetDescriptors}, {1, OfflineBPSSingleStatWithIPE_BPSHistStats_source_TargetDescriptors}},  // BPSHistStats
};

/*==================== USECASE: OfflineBPSStats =======================*/

static ChiBufferFormat OfflineBPSStats_TARGET_BUFFER_RAW_IN_formats[] =
{
    ChiFormatRawMIPI,
    ChiFormatRawPlain16,
};

static ChiBufferFormat OfflineBPSStats_TARGET_BUFFER_BPS_FULL_formats[] =
{
    ChiFormatUBWCTP10,
};

static ChiBufferFormat OfflineBPSStats_TARGET_BUFFER_BPS_BG_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat OfflineBPSStats_TARGET_BUFFER_BPS_BHIST_formats[] =
{
    ChiFormatBlob,
};

static ChiTarget OfflineBPSStats_TARGET_BUFFER_RAW_IN_target =
{
    ChiStreamTypeInput,
    {
        0,
        0,
        4096,
        2448
    },
    2,
    OfflineBPSStats_TARGET_BUFFER_RAW_IN_formats,
    NULL
}; // TARGET_BUFFER_RAW_IN

static ChiTarget OfflineBPSStats_TARGET_BUFFER_BPS_FULL_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    OfflineBPSStats_TARGET_BUFFER_BPS_FULL_formats,
    NULL
}; // TARGET_BUFFER_BPS_FULL

static ChiTarget OfflineBPSStats_TARGET_BUFFER_BPS_BG_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    OfflineBPSStats_TARGET_BUFFER_BPS_BG_formats,
    NULL
}; // TARGET_BUFFER_BPS_BG

static ChiTarget OfflineBPSStats_TARGET_BUFFER_BPS_BHIST_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    OfflineBPSStats_TARGET_BUFFER_BPS_BHIST_formats,
    NULL
}; // TARGET_BUFFER_BPS_BHIST

static ChiTarget* OfflineBPSStats_Targets[] =
{
	&OfflineBPSStats_TARGET_BUFFER_RAW_IN_target,
	&OfflineBPSStats_TARGET_BUFFER_BPS_FULL_target,
	&OfflineBPSStats_TARGET_BUFFER_BPS_BG_target,
	&OfflineBPSStats_TARGET_BUFFER_BPS_BHIST_target
};

/*****************************Pipeline BPSStats***************************/

static ChiLinkNodeDescriptor OfflineBPSStats_BPSStatsLink0DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor OfflineBPSStats_BPSStatsLink1DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor OfflineBPSStats_BPSStatsLink2DestDescriptors[] =
{
    {2, 2, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor OfflineBPSStats_BPSStats_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_BPS_FULL", &OfflineBPSStats_TARGET_BUFFER_BPS_FULL_target, {65539, 0, 1}}, // TARGET_BUFFER_BPS_FULL
    {"TARGET_BUFFER_BPS_BG", &OfflineBPSStats_TARGET_BUFFER_BPS_BG_target, {65539, 0, 5}}, // TARGET_BUFFER_BPS_BG
    {"TARGET_BUFFER_BPS_BHIST", &OfflineBPSStats_TARGET_BUFFER_BPS_BHIST_target, {65539, 0, 6}}, // TARGET_BUFFER_BPS_BHIST
};

static ChiTargetPortDescriptor OfflineBPSStats_BPSStats_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW_IN", &OfflineBPSStats_TARGET_BUFFER_RAW_IN_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW_IN
};

static ChiInputPortDescriptor OfflineBPSStats_BPSStatsNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor OfflineBPSStats_BPSStatsNode65539_0OutputPortDescriptors[] =
{
    {1, 1, 1, 0, 0, NULL}, // BPSOutputPortFull
    {5, 1, 1, 0, 0, NULL}, // BPSOutputPortStatsBG
    {6, 1, 1, 0, 0, NULL}, // BPSOutputPortStatsBHist
};

static ChiNode OfflineBPSStats_BPSStatsNodes[] =
{
    {NULL, 65539, 0, { 1, OfflineBPSStats_BPSStatsNode65539_0InputPortDescriptors, 3, OfflineBPSStats_BPSStatsNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink OfflineBPSStats_BPSStatsLinks[] =
{
    {{65539, 0, 1, 0}, 1, OfflineBPSStats_BPSStatsLink0DestDescriptors, {0}, {0}},
    {{65539, 0, 5, 0}, 1, OfflineBPSStats_BPSStatsLink1DestDescriptors, {0}, {0}},
    {{65539, 0, 6, 0}, 1, OfflineBPSStats_BPSStatsLink2DestDescriptors, {0}, {0}},
};

enum OfflineBPSStatsPipelineIds
{
    BPSStats = 0,
};

static ChiPipelineTargetCreateDescriptor OfflineBPSStats_pipelines[] =
{
    {"BPSStats", { 0, 1, OfflineBPSStats_BPSStatsNodes, 3, OfflineBPSStats_BPSStatsLinks, 0}, {3, OfflineBPSStats_BPSStats_sink_TargetDescriptors}, {1, OfflineBPSStats_BPSStats_source_TargetDescriptors}},  // BPSStats
};

/*==================== USECASE: OfflineBPSStatsWithIPE =======================*/

static ChiBufferFormat OfflineBPSStatsWithIPE_TARGET_BUFFER_RAW_IN_formats[] =
{
    ChiFormatRawMIPI,
    ChiFormatRawPlain16,
};

static ChiBufferFormat OfflineBPSStatsWithIPE_TARGET_BUFFER_IPE_DISPLAY_formats[] =
{
    ChiFormatYUV420NV12,
};

static ChiBufferFormat OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BG_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BHIST_formats[] =
{
    ChiFormatBlob,
};

static ChiTarget OfflineBPSStatsWithIPE_TARGET_BUFFER_RAW_IN_target =
{
    ChiStreamTypeInput,
    {
        0,
        0,
        4096,
        2448
    },
    2,
    OfflineBPSStatsWithIPE_TARGET_BUFFER_RAW_IN_formats,
    NULL
}; // TARGET_BUFFER_RAW_IN

static ChiTarget OfflineBPSStatsWithIPE_TARGET_BUFFER_IPE_DISPLAY_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    OfflineBPSStatsWithIPE_TARGET_BUFFER_IPE_DISPLAY_formats,
    NULL
}; // TARGET_BUFFER_IPE_DISPLAY

static ChiTarget OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BG_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BG_formats,
    NULL
}; // TARGET_BUFFER_BPS_BG

static ChiTarget OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BHIST_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BHIST_formats,
    NULL
}; // TARGET_BUFFER_BPS_BHIST

static ChiTarget* OfflineBPSStatsWithIPE_Targets[] =
{
	&OfflineBPSStatsWithIPE_TARGET_BUFFER_RAW_IN_target,
	&OfflineBPSStatsWithIPE_TARGET_BUFFER_IPE_DISPLAY_target,
	&OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BG_target,
	&OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BHIST_target
};

/*****************************Pipeline BPSAllStats***************************/

static ChiLinkNodeDescriptor OfflineBPSStatsWithIPE_BPSAllStatsLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor OfflineBPSStatsWithIPE_BPSAllStatsLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor OfflineBPSStatsWithIPE_BPSAllStatsLink2DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor OfflineBPSStatsWithIPE_BPSAllStatsLink3DestDescriptors[] =
{
    {2, 2, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor OfflineBPSStatsWithIPE_BPSAllStats_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_IPE_DISPLAY", &OfflineBPSStatsWithIPE_TARGET_BUFFER_IPE_DISPLAY_target, {65538, 0, 8}}, // TARGET_BUFFER_IPE_DISPLAY
    {"TARGET_BUFFER_BPS_BG", &OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BG_target, {65539, 0, 5}}, // TARGET_BUFFER_BPS_BG
    {"TARGET_BUFFER_BPS_BHIST", &OfflineBPSStatsWithIPE_TARGET_BUFFER_BPS_BHIST_target, {65539, 0, 6}}, // TARGET_BUFFER_BPS_BHIST
};

static ChiTargetPortDescriptor OfflineBPSStatsWithIPE_BPSAllStats_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW_IN", &OfflineBPSStatsWithIPE_TARGET_BUFFER_RAW_IN_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW_IN
};

static ChiInputPortDescriptor OfflineBPSStatsWithIPE_BPSAllStatsNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
};

static ChiOutputPortDescriptor OfflineBPSStatsWithIPE_BPSAllStatsNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
};

static ChiInputPortDescriptor OfflineBPSStatsWithIPE_BPSAllStatsNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor OfflineBPSStatsWithIPE_BPSAllStatsNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
    {5, 1, 1, 0, 0, NULL}, // BPSOutputPortStatsBG
    {6, 1, 1, 0, 0, NULL}, // BPSOutputPortStatsBHist
};

static ChiNode OfflineBPSStatsWithIPE_BPSAllStatsNodes[] =
{
    {NULL, 65538, 0, { 1, OfflineBPSStatsWithIPE_BPSAllStatsNode65538_0InputPortDescriptors, 1, OfflineBPSStatsWithIPE_BPSAllStatsNode65538_0OutputPortDescriptors }, 0},
    {NULL, 65539, 0, { 1, OfflineBPSStatsWithIPE_BPSAllStatsNode65539_0InputPortDescriptors, 3, OfflineBPSStatsWithIPE_BPSAllStatsNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink OfflineBPSStatsWithIPE_BPSAllStatsLinks[] =
{
    {{65539, 0, 1, 0}, 1, OfflineBPSStatsWithIPE_BPSAllStatsLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, OfflineBPSStatsWithIPE_BPSAllStatsLink1DestDescriptors, {0}, {0}},
    {{65539, 0, 5, 0}, 1, OfflineBPSStatsWithIPE_BPSAllStatsLink2DestDescriptors, {0}, {0}},
    {{65539, 0, 6, 0}, 1, OfflineBPSStatsWithIPE_BPSAllStatsLink3DestDescriptors, {0}, {0}},
};

enum OfflineBPSStatsWithIPEPipelineIds
{
    BPSAllStats = 0,
};

static ChiPipelineTargetCreateDescriptor OfflineBPSStatsWithIPE_pipelines[] =
{
    {"BPSAllStats", { 0, 2, OfflineBPSStatsWithIPE_BPSAllStatsNodes, 4, OfflineBPSStatsWithIPE_BPSAllStatsLinks, 0}, {3, OfflineBPSStatsWithIPE_BPSAllStats_sink_TargetDescriptors}, {1, OfflineBPSStatsWithIPE_BPSAllStats_source_TargetDescriptors}},  // BPSAllStats
};

/*==================== USECASE: TestPreviewCallbackSnapshotWithThumbnail =======================*/

static ChiBufferFormat TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiBufferFormat TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_CALLBACK_formats[] =
{
    ChiFormatUBWCNV12,
    ChiFormatUBWCTP10,
    ChiFormatYUV420NV12,
};

static ChiBufferFormat TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_RAW_formats[] =
{
    ChiFormatRawMIPI,
    ChiFormatRawPlain16,
};

static ChiBufferFormat TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_SNAPSHOT_formats[] =
{
    ChiFormatJpeg,
};

static ChiBufferFormat TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_YUV_THUMBNAIL_formats[] =
{
    ChiFormatYUV420NV12,
};

static ChiTarget TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_formats,
    NULL
}; // TARGET_BUFFER_PREVIEW

static ChiTarget TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_CALLBACK_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    3,
    TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_CALLBACK_formats,
    NULL
}; // TARGET_BUFFER_PREVIEW_CALLBACK

static ChiTarget TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_RAW_target =
{
    ChiStreamTypeBidirectional,
    {
        0,
        0,
        5488,
        4112
    },
    2,
    TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_RAW_formats,
    NULL
}; // TARGET_BUFFER_RAW

static ChiTarget TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_SNAPSHOT_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        5488,
        4112
    },
    1,
    TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_SNAPSHOT_formats,
    NULL
}; // TARGET_BUFFER_SNAPSHOT

static ChiTarget TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_YUV_THUMBNAIL_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_YUV_THUMBNAIL_formats,
    NULL
}; // TARGET_BUFFER_YUV_THUMBNAIL

static ChiTarget* TestPreviewCallbackSnapshotWithThumbnail_Targets[] =
{
	&TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_target,
	&TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_CALLBACK_target,
	&TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_RAW_target,
	&TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_SNAPSHOT_target,
	&TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_YUV_THUMBNAIL_target
};

/*****************************Pipeline ZSLSnapshotJpegWithThumbnail***************************/

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink0DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink1DestDescriptors[] =
{
    {65538, 0, 1, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink2DestDescriptors[] =
{
    {65538, 0, 2, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink3DestDescriptors[] =
{
    {65538, 0, 3, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink4DestDescriptors[] =
{
    {65537, 0, 0, 0}, // JPEG
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink5DestDescriptors[] =
{
    {6, 0, 0, 0}, // JPEGAggregator
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink6DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink7DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnail_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_SNAPSHOT", &TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_SNAPSHOT_target, {6, 0, 1}}, // TARGET_BUFFER_SNAPSHOT
    {"TARGET_BUFFER_YUV_THUMBNAIL", &TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_YUV_THUMBNAIL_target, {65538, 0, 9}}, // TARGET_BUFFER_YUV_THUMBNAIL
};

static ChiTargetPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnail_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW", &TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_RAW_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode6_0InputPortDescriptors[] =
{
    {0, 0, 0}, // JPEGAggregatorInputPort0
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode6_0OutputPortDescriptors[] =
{
    {1, 1, 1, 0, 0, NULL}, // JPEGAggregatorOutputPort0
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65537_0InputPortDescriptors[] =
{
    {0, 0, 0}, // JPEGInputPort0
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65537_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // JPEGOutputPort0
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
    {1, 0, 0}, // IPEInputPortDS4
    {2, 0, 0}, // IPEInputPortDS16
    {3, 0, 0}, // IPEInputPortDS64
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65538_0OutputPortDescriptors[] =
{
    {8, 0, 0, 0, 0, NULL}, // IPEOutputPortDisplay
    {9, 1, 1, 0, 0, NULL}, // IPEOutputPortVideo
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
    {2, 0, 0, 0, 0, NULL}, // BPSOutputPortDS4
    {3, 0, 0, 0, 0, NULL}, // BPSOutputPortDS16
    {4, 0, 0, 0, 0, NULL}, // BPSOutputPortDS64
};

static ChiNode TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNodes[] =
{
    {NULL, 6, 0, { 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode6_0InputPortDescriptors, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode6_0OutputPortDescriptors }, 0},
    {NULL, 65537, 0, { 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65537_0InputPortDescriptors, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65537_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 4, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65538_0InputPortDescriptors, 2, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65538_0OutputPortDescriptors }, 0},
    {NULL, 65539, 0, { 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65539_0InputPortDescriptors, 4, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLinks[] =
{
    {{65539, 0, 1, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink0DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65539, 0, 2, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink1DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65539, 0, 3, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink2DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65539, 0, 4, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink3DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65538, 0, 8, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink4DestDescriptors, {ChiFormatYUV420NV12, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65537, 0, 1, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink5DestDescriptors, {ChiFormatYUV420NV12, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable|BufferMemFlagCache}, {0}},
    {{6, 0, 1, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink6DestDescriptors, {0}, {0}},
    {{65538, 0, 9, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLink7DestDescriptors, {0}, {0}},
};

/*****************************Pipeline RealtimePrevwithCallback***************************/

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink1DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink2DestDescriptors[] =
{
    {65538, 0, 1, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink3DestDescriptors[] =
{
    {65538, 0, 2, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink4DestDescriptors[] =
{
    {3, 0, 0, 0}, // SinkNoBuffer
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink5DestDescriptors[] =
{
    {1, 0, 0, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink6DestDescriptors[] =
{
    {1, 0, 1, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink7DestDescriptors[] =
{
    {1, 0, 2, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink8DestDescriptors[] =
{
    {1, 0, 7, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink9DestDescriptors[] =
{
    {5, 0, 4, 0}, // AutoFocus
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink10DestDescriptors[] =
{
    {5, 0, 8, 0}, // AutoFocus
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink11DestDescriptors[] =
{
    {3, 3, 0, 0}, // SinkNoBuffer
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink12DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink13DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallback_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_PREVIEW", &TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_target, {65538, 0, 8}}, // TARGET_BUFFER_PREVIEW
    {"TARGET_BUFFER_PREVIEW_CALLBACK", &TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_CALLBACK_target, {65538, 0, 9}}, // TARGET_BUFFER_PREVIEW_CALLBACK
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode1_0InputPortDescriptors[] =
{
    {0, 0, 0}, // StatsProcessingInputPortHDRBE
    {1, 0, 0}, // StatsInputPortAWBBG
    {2, 0, 0}, // StatsInputPortHDRBHist
    {7, 0, 0}, // StatsInputPortRS
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode1_0OutputPortDescriptors[] =
{
    {0, 1, 0, 0, 0, NULL}, // StatsProcessingOutputPort0
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode5_0InputPortDescriptors[] =
{
    {4, 0, 0}, // StatsInputPortBF
    {8, 0, 0}, // StatsInputPortRDIPDAF
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode5_0OutputPortDescriptors[] =
{
    {0, 1, 0, 0, 0, NULL}, // AutoFocusOutputPort0
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode65536_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // IFEOutputPortFull
    {1, 0, 0, 0, 0, NULL}, // IFEOutputPortDS4
    {2, 0, 0, 0, 0, NULL}, // IFEOutputPortDS16
    {17, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsHDRBE
    {21, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsAWBBG
    {18, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsHDRBHIST
    {12, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsRS
    {20, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsBF
    {8, 0, 0, 2, 0, NULL}, // IFEOutputPortRDI0
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
    {1, 0, 0}, // IPEInputPortDS4
    {2, 0, 0}, // IPEInputPortDS16
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
    {9, 1, 1, 0, 0, NULL}, // IPEOutputPortVideo
};

static ChiNodeProperty TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallback_node0_0_properties[] =
{
    {1, "com.qti.stats.pdlib"},
};

static ChiNodeProperty TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallback_node1_0_properties[] =
{
    {1, "com.qti.stats.aec"},
    {1, "com.qti.stats.awb"},
    {1, "com.qti.stats.afd"},
};

static ChiNodeProperty TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallback_node5_0_properties[] =
{
    {1, "com.qti.stats.af"},
    {1, "com.qti.stats.pdlib"},
};

static ChiNode TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNodes[] =
{
    {TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallback_node0_0_properties, 0, 0, { 0, NULL, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode0_0OutputPortDescriptors }, 1},
    {TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallback_node1_0_properties, 1, 0, { 4, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode1_0InputPortDescriptors, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode1_0OutputPortDescriptors }, 3},
    {TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallback_node5_0_properties, 5, 0, { 2, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode5_0InputPortDescriptors, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode5_0OutputPortDescriptors }, 2},
    {NULL, 65536, 0, { 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode65536_0InputPortDescriptors, 9, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode65536_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 3, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode65538_0InputPortDescriptors, 2, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLinks[] =
{
    {{0, 0, 0, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink1DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65536, 0, 1, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink2DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65536, 0, 2, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink3DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{1, 0, 0, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink4DestDescriptors, {0}, {0}},
    {{65536, 0, 17, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink5DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 21, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink6DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 18, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink7DestDescriptors, {ChiFormatBlob, 3072, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 12, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink8DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 20, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink9DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 8, 2}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink10DestDescriptors, {ChiFormatBlob, 307200, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{5, 0, 0, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink11DestDescriptors, {0}, {0}},
    {{65538, 0, 8, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink12DestDescriptors, {0}, {0}},
    {{65538, 0, 9, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLink13DestDescriptors, {0}, {0}},
};

/*****************************Pipeline RealtimePrevwithCallbackAndRaw***************************/

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink1DestDescriptors[] =
{
    {65538, 0, 0, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink2DestDescriptors[] =
{
    {65538, 0, 1, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink3DestDescriptors[] =
{
    {65538, 0, 2, 0}, // IPE
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink4DestDescriptors[] =
{
    {3, 0, 0, 0}, // SinkNoBuffer
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink5DestDescriptors[] =
{
    {1, 0, 0, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink6DestDescriptors[] =
{
    {1, 0, 1, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink7DestDescriptors[] =
{
    {1, 0, 2, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink8DestDescriptors[] =
{
    {1, 0, 7, 0}, // StatsProcessing
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink9DestDescriptors[] =
{
    {5, 0, 4, 0}, // AutoFocus
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink10DestDescriptors[] =
{
    {5, 0, 8, 0}, // AutoFocus
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink11DestDescriptors[] =
{
    {3, 3, 0, 0}, // SinkNoBuffer
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink12DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink13DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink14DestDescriptors[] =
{
    {2, 2, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRaw_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_PREVIEW", &TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_target, {65538, 0, 8}}, // TARGET_BUFFER_PREVIEW
    {"TARGET_BUFFER_PREVIEW_CALLBACK", &TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_PREVIEW_CALLBACK_target, {65538, 0, 9}}, // TARGET_BUFFER_PREVIEW_CALLBACK
    {"TARGET_BUFFER_RAW", &TestPreviewCallbackSnapshotWithThumbnail_TARGET_BUFFER_RAW_target, {65536, 0, 9}}, // TARGET_BUFFER_RAW
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode1_0InputPortDescriptors[] =
{
    {0, 0, 0}, // StatsProcessingInputPortHDRBE
    {1, 0, 0}, // StatsInputPortAWBBG
    {2, 0, 0}, // StatsInputPortHDRBHist
    {7, 0, 0}, // StatsInputPortRS
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode1_0OutputPortDescriptors[] =
{
    {0, 1, 0, 0, 0, NULL}, // StatsProcessingOutputPort0
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode5_0InputPortDescriptors[] =
{
    {4, 0, 0}, // StatsInputPortBF
    {8, 0, 0}, // StatsInputPortRDIPDAF
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode5_0OutputPortDescriptors[] =
{
    {0, 1, 0, 0, 0, NULL}, // AutoFocusOutputPort0
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode65536_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // IFEOutputPortFull
    {1, 0, 0, 0, 0, NULL}, // IFEOutputPortDS4
    {2, 0, 0, 0, 0, NULL}, // IFEOutputPortDS16
    {17, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsHDRBE
    {21, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsAWBBG
    {18, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsHDRBHIST
    {12, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsRS
    {20, 0, 0, 0, 0, NULL}, // IFEOutputPortStatsBF
    {8, 0, 0, 2, 0, NULL}, // IFEOutputPortRDI0
    {9, 1, 1, 0, 0, NULL}, // IFEOutputPortRDI1
};

static ChiInputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode65538_0InputPortDescriptors[] =
{
    {0, 0, 0}, // IPEInputPortFull
    {1, 0, 0}, // IPEInputPortDS4
    {2, 0, 0}, // IPEInputPortDS16
};

static ChiOutputPortDescriptor TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode65538_0OutputPortDescriptors[] =
{
    {8, 1, 1, 0, 0, NULL}, // IPEOutputPortDisplay
    {9, 1, 1, 0, 0, NULL}, // IPEOutputPortVideo
};

static ChiNodeProperty TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRaw_node0_0_properties[] =
{
    {1, "com.qti.stats.pdlib"},
};

static ChiNodeProperty TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRaw_node1_0_properties[] =
{
    {1, "com.qti.stats.aec"},
    {1, "com.qti.stats.awb"},
    {1, "com.qti.stats.afd"},
};

static ChiNodeProperty TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRaw_node5_0_properties[] =
{
    {1, "com.qti.stats.af"},
    {1, "com.qti.stats.pdlib"},
};

static ChiNode TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNodes[] =
{
    {TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRaw_node0_0_properties, 0, 0, { 0, NULL, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode0_0OutputPortDescriptors }, 1},
    {TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRaw_node1_0_properties, 1, 0, { 4, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode1_0InputPortDescriptors, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode1_0OutputPortDescriptors }, 3},
    {TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRaw_node5_0_properties, 5, 0, { 2, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode5_0InputPortDescriptors, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode5_0OutputPortDescriptors }, 2},
    {NULL, 65536, 0, { 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode65536_0InputPortDescriptors, 10, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode65536_0OutputPortDescriptors }, 0},
    {NULL, 65538, 0, { 3, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode65538_0InputPortDescriptors, 2, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNode65538_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLinks[] =
{
    {{0, 0, 0, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink1DestDescriptors, {ChiFormatUBWCTP10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65536, 0, 1, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink2DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{65536, 0, 2, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink3DestDescriptors, {ChiFormatPD10, 0, 8, 8, BufferHeapIon, BufferMemFlagHw}, {0}},
    {{1, 0, 0, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink4DestDescriptors, {0}, {0}},
    {{65536, 0, 17, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink5DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 21, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink6DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 18, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink7DestDescriptors, {ChiFormatBlob, 3072, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 12, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink8DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 20, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink9DestDescriptors, {ChiFormatBlob, 16384, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{65536, 0, 8, 2}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink10DestDescriptors, {ChiFormatBlob, 307200, 10, 10, BufferHeapIon, BufferMemFlagLockable|BufferMemFlagHw}, {0}},
    {{5, 0, 0, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink11DestDescriptors, {0}, {0}},
    {{65538, 0, 8, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink12DestDescriptors, {0}, {0}},
    {{65538, 0, 9, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink13DestDescriptors, {0}, {0}},
    {{65536, 0, 9, 0}, 1, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLink14DestDescriptors, {0}, {0}},
};

enum TestPreviewCallbackSnapshotWithThumbnailPipelineIds
{
    ZSLSnapshotJpegWithThumbnail = 0,
    RealtimePrevwithCallback = 1,
    RealtimePrevwithCallbackAndRaw = 2,
};

static ChiPipelineTargetCreateDescriptor TestPreviewCallbackSnapshotWithThumbnail_pipelines[] =
{
    {"ZSLSnapshotJpegWithThumbnail", { 0, 4, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailNodes, 8, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnailLinks, 0}, {2, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnail_sink_TargetDescriptors}, {1, TestPreviewCallbackSnapshotWithThumbnail_ZSLSnapshotJpegWithThumbnail_source_TargetDescriptors}},  // ZSLSnapshotJpegWithThumbnail
    {"RealtimePrevwithCallback", { 0, 5, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackNodes, 14, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackLinks, 1}, {2, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallback_sink_TargetDescriptors}, {0, NULL}},  // RealtimePrevwithCallback
    {"RealtimePrevwithCallbackAndRaw", { 0, 5, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawNodes, 15, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRawLinks, 1}, {3, TestPreviewCallbackSnapshotWithThumbnail_RealtimePrevwithCallbackAndRaw_sink_TargetDescriptors}, {0, NULL}},  // RealtimePrevwithCallbackAndRaw
};

/*==================== USECASE: TestGPUDSPorts =======================*/

static ChiBufferFormat TestGPUDSPorts_TARGET_BUFFER_RAW_IN_formats[] =
{
    ChiFormatRawMIPI,
    ChiFormatRawPlain16,
};

static ChiBufferFormat TestGPUDSPorts_TARGET_BUFFER_FULL_formats[] =
{
    ChiFormatP010,
};

static ChiBufferFormat TestGPUDSPorts_TARGET_BUFFER_DS4_formats[] =
{
    ChiFormatP010,
};

static ChiBufferFormat TestGPUDSPorts_TARGET_BUFFER_DS16_formats[] =
{
    ChiFormatP010,
};

static ChiBufferFormat TestGPUDSPorts_TARGET_BUFFER_DS64_formats[] =
{
    ChiFormatP010,
};

static ChiTarget TestGPUDSPorts_TARGET_BUFFER_RAW_IN_target =
{
    ChiStreamTypeInput,
    {
        0,
        0,
        4608,
        2592
    },
    2,
    TestGPUDSPorts_TARGET_BUFFER_RAW_IN_formats,
    NULL
}; // TARGET_BUFFER_RAW_IN

static ChiTarget TestGPUDSPorts_TARGET_BUFFER_FULL_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    TestGPUDSPorts_TARGET_BUFFER_FULL_formats,
    NULL
}; // TARGET_BUFFER_FULL

static ChiTarget TestGPUDSPorts_TARGET_BUFFER_DS4_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    TestGPUDSPorts_TARGET_BUFFER_DS4_formats,
    NULL
}; // TARGET_BUFFER_DS4

static ChiTarget TestGPUDSPorts_TARGET_BUFFER_DS16_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    TestGPUDSPorts_TARGET_BUFFER_DS16_formats,
    NULL
}; // TARGET_BUFFER_DS16

static ChiTarget TestGPUDSPorts_TARGET_BUFFER_DS64_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2448
    },
    1,
    TestGPUDSPorts_TARGET_BUFFER_DS64_formats,
    NULL
}; // TARGET_BUFFER_DS64

static ChiTarget* TestGPUDSPorts_Targets[] =
{
	&TestGPUDSPorts_TARGET_BUFFER_RAW_IN_target,
	&TestGPUDSPorts_TARGET_BUFFER_FULL_target,
	&TestGPUDSPorts_TARGET_BUFFER_DS4_target,
	&TestGPUDSPorts_TARGET_BUFFER_DS16_target,
	&TestGPUDSPorts_TARGET_BUFFER_DS64_target
};

/*****************************Pipeline OfflineBPSGPU***************************/

static ChiLinkNodeDescriptor TestGPUDSPorts_OfflineBPSGPULink0DestDescriptors[] =
{
    {255, 0, 0, 0}, // ChiGPUNode
};

static ChiLinkNodeDescriptor TestGPUDSPorts_OfflineBPSGPULink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestGPUDSPorts_OfflineBPSGPULink2DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestGPUDSPorts_OfflineBPSGPULink3DestDescriptors[] =
{
    {2, 2, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestGPUDSPorts_OfflineBPSGPULink4DestDescriptors[] =
{
    {2, 3, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestGPUDSPorts_OfflineBPSGPU_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_FULL", &TestGPUDSPorts_TARGET_BUFFER_FULL_target, {255, 0, 0}}, // TARGET_BUFFER_FULL
    {"TARGET_BUFFER_DS4", &TestGPUDSPorts_TARGET_BUFFER_DS4_target, {255, 0, 1}}, // TARGET_BUFFER_DS4
    {"TARGET_BUFFER_DS16", &TestGPUDSPorts_TARGET_BUFFER_DS16_target, {255, 0, 2}}, // TARGET_BUFFER_DS16
    {"TARGET_BUFFER_DS64", &TestGPUDSPorts_TARGET_BUFFER_DS64_target, {255, 0, 3}}, // TARGET_BUFFER_DS64
};

static ChiTargetPortDescriptor TestGPUDSPorts_OfflineBPSGPU_source_TargetDescriptors[] =
{
    {"TARGET_BUFFER_RAW_IN", &TestGPUDSPorts_TARGET_BUFFER_RAW_IN_target, {65539, 0, 0}}, // TARGET_BUFFER_RAW_IN
};

static ChiInputPortDescriptor TestGPUDSPorts_OfflineBPSGPUNode255_0InputPortDescriptors[] =
{
    {0, 0, 0}, // ChiNodeInputPortFull
};

static ChiOutputPortDescriptor TestGPUDSPorts_OfflineBPSGPUNode255_0OutputPortDescriptors[] =
{
    {0, 1, 1, 0, 0, NULL}, // ChiNodeOutputPortFull
    {1, 1, 1, 0, 0, NULL}, // ChiNodeOutputPortDS4
    {2, 1, 1, 0, 0, NULL}, // ChiNodeOutputPortDS16
    {3, 1, 1, 0, 0, NULL}, // ChiNodeOutputPortDS64
};

static ChiInputPortDescriptor TestGPUDSPorts_OfflineBPSGPUNode65539_0InputPortDescriptors[] =
{
    {0, 1, 0}, // BPSInputPort
};

static ChiOutputPortDescriptor TestGPUDSPorts_OfflineBPSGPUNode65539_0OutputPortDescriptors[] =
{
    {1, 0, 0, 0, 0, NULL}, // BPSOutputPortFull
};

static ChiNodeProperty TestGPUDSPorts_OfflineBPSGPU_node255_0_properties[] =
{
    {1, "com.qti.node.gpu"},
    {2048, "8"},
};

static ChiNode TestGPUDSPorts_OfflineBPSGPUNodes[] =
{
    {TestGPUDSPorts_OfflineBPSGPU_node255_0_properties, 255, 0, { 1, TestGPUDSPorts_OfflineBPSGPUNode255_0InputPortDescriptors, 4, TestGPUDSPorts_OfflineBPSGPUNode255_0OutputPortDescriptors }, 2},
    {NULL, 65539, 0, { 1, TestGPUDSPorts_OfflineBPSGPUNode65539_0InputPortDescriptors, 1, TestGPUDSPorts_OfflineBPSGPUNode65539_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestGPUDSPorts_OfflineBPSGPULinks[] =
{
    {{65539, 0, 1, 0}, 1, TestGPUDSPorts_OfflineBPSGPULink0DestDescriptors, {ChiFormatP010, 0, 8, 8, BufferHeapIon, BufferMemFlagHw|BufferMemFlagLockable}, {0}},
    {{255, 0, 0, 0}, 1, TestGPUDSPorts_OfflineBPSGPULink1DestDescriptors, {0}, {0}},
    {{255, 0, 1, 0}, 1, TestGPUDSPorts_OfflineBPSGPULink2DestDescriptors, {0}, {0}},
    {{255, 0, 2, 0}, 1, TestGPUDSPorts_OfflineBPSGPULink3DestDescriptors, {0}, {0}},
    {{255, 0, 3, 0}, 1, TestGPUDSPorts_OfflineBPSGPULink4DestDescriptors, {0}, {0}},
};

enum TestGPUDSPortsPipelineIds
{
    OfflineBPSGPU = 0,
};

static ChiPipelineTargetCreateDescriptor TestGPUDSPorts_pipelines[] =
{
    {"OfflineBPSGPU", { 0, 2, TestGPUDSPorts_OfflineBPSGPUNodes, 5, TestGPUDSPorts_OfflineBPSGPULinks, 0}, {4, TestGPUDSPorts_OfflineBPSGPU_sink_TargetDescriptors}, {1, TestGPUDSPorts_OfflineBPSGPU_source_TargetDescriptors}},  // OfflineBPSGPU
};

/*==================== USECASE: TestIFEFullStatsTestgen =======================*/

static ChiBufferFormat TestIFEFullStatsTestgen_TARGET_BUFFER_FULL_formats[] =
{
    ChiFormatYUV420NV12,
};

static ChiBufferFormat TestIFEFullStatsTestgen_TARGET_BUFFER_HDRBE_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStatsTestgen_TARGET_BUFFER_AWBBG_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStatsTestgen_TARGET_BUFFER_BF_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStatsTestgen_TARGET_BUFFER_RS_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStatsTestgen_TARGET_BUFFER_CS_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStatsTestgen_TARGET_BUFFER_IHIST_formats[] =
{
    ChiFormatBlob,
};

static ChiTarget TestIFEFullStatsTestgen_TARGET_BUFFER_FULL_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2160
    },
    1,
    TestIFEFullStatsTestgen_TARGET_BUFFER_FULL_formats,
    NULL
}; // TARGET_BUFFER_FULL

static ChiTarget TestIFEFullStatsTestgen_TARGET_BUFFER_HDRBE_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        196676,
        1
    },
    1,
    TestIFEFullStatsTestgen_TARGET_BUFFER_HDRBE_formats,
    NULL
}; // TARGET_BUFFER_HDRBE

static ChiTarget TestIFEFullStatsTestgen_TARGET_BUFFER_AWBBG_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        921668,
        1
    },
    1,
    TestIFEFullStatsTestgen_TARGET_BUFFER_AWBBG_formats,
    NULL
}; // TARGET_BUFFER_AWBBG

static ChiTarget TestIFEFullStatsTestgen_TARGET_BUFFER_BF_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        17368,
        1
    },
    1,
    TestIFEFullStatsTestgen_TARGET_BUFFER_BF_formats,
    NULL
}; // TARGET_BUFFER_BF

static ChiTarget TestIFEFullStatsTestgen_TARGET_BUFFER_RS_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        98420,
        1
    },
    1,
    TestIFEFullStatsTestgen_TARGET_BUFFER_RS_formats,
    NULL
}; // TARGET_BUFFER_RS

static ChiTarget TestIFEFullStatsTestgen_TARGET_BUFFER_CS_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        37456,
        1
    },
    1,
    TestIFEFullStatsTestgen_TARGET_BUFFER_CS_formats,
    NULL
}; // TARGET_BUFFER_CS

static ChiTarget TestIFEFullStatsTestgen_TARGET_BUFFER_IHIST_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4104,
        1
    },
    1,
    TestIFEFullStatsTestgen_TARGET_BUFFER_IHIST_formats,
    NULL
}; // TARGET_BUFFER_IHIST

static ChiTarget* TestIFEFullStatsTestgen_Targets[] =
{
	&TestIFEFullStatsTestgen_TARGET_BUFFER_FULL_target,
	&TestIFEFullStatsTestgen_TARGET_BUFFER_HDRBE_target,
	&TestIFEFullStatsTestgen_TARGET_BUFFER_AWBBG_target,
	&TestIFEFullStatsTestgen_TARGET_BUFFER_BF_target,
	&TestIFEFullStatsTestgen_TARGET_BUFFER_RS_target,
	&TestIFEFullStatsTestgen_TARGET_BUFFER_CS_target,
	&TestIFEFullStatsTestgen_TARGET_BUFFER_IHIST_target
};

/*****************************Pipeline IfeFullStatsTestgen***************************/

static ChiLinkNodeDescriptor TestIFEFullStatsTestgen_IfeFullStatsTestgenLink0DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStatsTestgen_IfeFullStatsTestgenLink1DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStatsTestgen_IfeFullStatsTestgenLink2DestDescriptors[] =
{
    {2, 2, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStatsTestgen_IfeFullStatsTestgenLink3DestDescriptors[] =
{
    {2, 3, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStatsTestgen_IfeFullStatsTestgenLink4DestDescriptors[] =
{
    {2, 4, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStatsTestgen_IfeFullStatsTestgenLink5DestDescriptors[] =
{
    {2, 5, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStatsTestgen_IfeFullStatsTestgenLink6DestDescriptors[] =
{
    {2, 6, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestIFEFullStatsTestgen_IfeFullStatsTestgen_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_FULL", &TestIFEFullStatsTestgen_TARGET_BUFFER_FULL_target, {65536, 0, 0}}, // TARGET_BUFFER_FULL
    {"TARGET_BUFFER_HDRBE", &TestIFEFullStatsTestgen_TARGET_BUFFER_HDRBE_target, {65536, 0, 17}}, // TARGET_BUFFER_HDRBE
    {"TARGET_BUFFER_AWBBG", &TestIFEFullStatsTestgen_TARGET_BUFFER_AWBBG_target, {65536, 0, 21}}, // TARGET_BUFFER_AWBBG
    {"TARGET_BUFFER_BF", &TestIFEFullStatsTestgen_TARGET_BUFFER_BF_target, {65536, 0, 20}}, // TARGET_BUFFER_BF
    {"TARGET_BUFFER_RS", &TestIFEFullStatsTestgen_TARGET_BUFFER_RS_target, {65536, 0, 12}}, // TARGET_BUFFER_RS
    {"TARGET_BUFFER_CS", &TestIFEFullStatsTestgen_TARGET_BUFFER_CS_target, {65536, 0, 13}}, // TARGET_BUFFER_CS
    {"TARGET_BUFFER_IHIST", &TestIFEFullStatsTestgen_TARGET_BUFFER_IHIST_target, {65536, 0, 15}}, // TARGET_BUFFER_IHIST
};

static ChiOutputPortDescriptor TestIFEFullStatsTestgen_IfeFullStatsTestgenNode65536_0OutputPortDescriptors[] =
{
    {0, 1, 1, 0, 0, NULL}, // IFEOutputPortFull
    {17, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsHDRBE
    {21, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsAWBBG
    {20, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsBF
    {12, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsRS
    {13, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsCS
    {15, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsIHIST
};

static ChiNode TestIFEFullStatsTestgen_IfeFullStatsTestgenNodes[] =
{
    {NULL, 65536, 0, { 0, NULL, 7, TestIFEFullStatsTestgen_IfeFullStatsTestgenNode65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestIFEFullStatsTestgen_IfeFullStatsTestgenLinks[] =
{
    {{65536, 0, 0, 0}, 1, TestIFEFullStatsTestgen_IfeFullStatsTestgenLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 17, 0}, 1, TestIFEFullStatsTestgen_IfeFullStatsTestgenLink1DestDescriptors, {0}, {0}},
    {{65536, 0, 21, 0}, 1, TestIFEFullStatsTestgen_IfeFullStatsTestgenLink2DestDescriptors, {0}, {0}},
    {{65536, 0, 20, 0}, 1, TestIFEFullStatsTestgen_IfeFullStatsTestgenLink3DestDescriptors, {0}, {0}},
    {{65536, 0, 12, 0}, 1, TestIFEFullStatsTestgen_IfeFullStatsTestgenLink4DestDescriptors, {0}, {0}},
    {{65536, 0, 13, 0}, 1, TestIFEFullStatsTestgen_IfeFullStatsTestgenLink5DestDescriptors, {0}, {0}},
    {{65536, 0, 15, 0}, 1, TestIFEFullStatsTestgen_IfeFullStatsTestgenLink6DestDescriptors, {0}, {0}},
};

enum TestIFEFullStatsTestgenPipelineIds
{
    IfeFullStatsTestgen = 0,
};

static ChiPipelineTargetCreateDescriptor TestIFEFullStatsTestgen_pipelines[] =
{
    {"IfeFullStatsTestgen", { 0, 1, TestIFEFullStatsTestgen_IfeFullStatsTestgenNodes, 7, TestIFEFullStatsTestgen_IfeFullStatsTestgenLinks, 1}, {7, TestIFEFullStatsTestgen_IfeFullStatsTestgen_sink_TargetDescriptors}, {0, NULL}},  // IfeFullStatsTestgen
};

/*==================== USECASE: TestIFEFullStats =======================*/

static ChiBufferFormat TestIFEFullStats_TARGET_BUFFER_FULL_formats[] =
{
    ChiFormatYUV420NV12,
};

static ChiBufferFormat TestIFEFullStats_TARGET_BUFFER_HDRBE_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStats_TARGET_BUFFER_AWBBG_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStats_TARGET_BUFFER_BF_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStats_TARGET_BUFFER_RS_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStats_TARGET_BUFFER_CS_formats[] =
{
    ChiFormatBlob,
};

static ChiBufferFormat TestIFEFullStats_TARGET_BUFFER_IHIST_formats[] =
{
    ChiFormatBlob,
};

static ChiTarget TestIFEFullStats_TARGET_BUFFER_FULL_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4096,
        2160
    },
    1,
    TestIFEFullStats_TARGET_BUFFER_FULL_formats,
    NULL
}; // TARGET_BUFFER_FULL

static ChiTarget TestIFEFullStats_TARGET_BUFFER_HDRBE_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        196676,
        1
    },
    1,
    TestIFEFullStats_TARGET_BUFFER_HDRBE_formats,
    NULL
}; // TARGET_BUFFER_HDRBE

static ChiTarget TestIFEFullStats_TARGET_BUFFER_AWBBG_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        921668,
        1
    },
    1,
    TestIFEFullStats_TARGET_BUFFER_AWBBG_formats,
    NULL
}; // TARGET_BUFFER_AWBBG

static ChiTarget TestIFEFullStats_TARGET_BUFFER_BF_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        17368,
        1
    },
    1,
    TestIFEFullStats_TARGET_BUFFER_BF_formats,
    NULL
}; // TARGET_BUFFER_BF

static ChiTarget TestIFEFullStats_TARGET_BUFFER_RS_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        98420,
        1
    },
    1,
    TestIFEFullStats_TARGET_BUFFER_RS_formats,
    NULL
}; // TARGET_BUFFER_RS

static ChiTarget TestIFEFullStats_TARGET_BUFFER_CS_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        37456,
        1
    },
    1,
    TestIFEFullStats_TARGET_BUFFER_CS_formats,
    NULL
}; // TARGET_BUFFER_CS

static ChiTarget TestIFEFullStats_TARGET_BUFFER_IHIST_target =
{
    ChiStreamTypeOutput,
    {
        0,
        0,
        4104,
        1
    },
    1,
    TestIFEFullStats_TARGET_BUFFER_IHIST_formats,
    NULL
}; // TARGET_BUFFER_IHIST

static ChiTarget* TestIFEFullStats_Targets[] =
{
	&TestIFEFullStats_TARGET_BUFFER_FULL_target,
	&TestIFEFullStats_TARGET_BUFFER_HDRBE_target,
	&TestIFEFullStats_TARGET_BUFFER_AWBBG_target,
	&TestIFEFullStats_TARGET_BUFFER_BF_target,
	&TestIFEFullStats_TARGET_BUFFER_RS_target,
	&TestIFEFullStats_TARGET_BUFFER_CS_target,
	&TestIFEFullStats_TARGET_BUFFER_IHIST_target
};

/*****************************Pipeline IfeFullStats***************************/

static ChiLinkNodeDescriptor TestIFEFullStats_IfeFullStatsLink0DestDescriptors[] =
{
    {65536, 0, 2, 0}, // IFE
};

static ChiLinkNodeDescriptor TestIFEFullStats_IfeFullStatsLink1DestDescriptors[] =
{
    {2, 0, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStats_IfeFullStatsLink2DestDescriptors[] =
{
    {2, 1, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStats_IfeFullStatsLink3DestDescriptors[] =
{
    {2, 2, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStats_IfeFullStatsLink4DestDescriptors[] =
{
    {2, 3, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStats_IfeFullStatsLink5DestDescriptors[] =
{
    {2, 4, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStats_IfeFullStatsLink6DestDescriptors[] =
{
    {2, 5, 0, 0}, // SinkBuffer
};

static ChiLinkNodeDescriptor TestIFEFullStats_IfeFullStatsLink7DestDescriptors[] =
{
    {2, 6, 0, 0}, // SinkBuffer
};

static ChiTargetPortDescriptor TestIFEFullStats_IfeFullStats_sink_TargetDescriptors[] =
{
    {"TARGET_BUFFER_FULL", &TestIFEFullStats_TARGET_BUFFER_FULL_target, {65536, 0, 0}}, // TARGET_BUFFER_FULL
    {"TARGET_BUFFER_HDRBE", &TestIFEFullStats_TARGET_BUFFER_HDRBE_target, {65536, 0, 17}}, // TARGET_BUFFER_HDRBE
    {"TARGET_BUFFER_AWBBG", &TestIFEFullStats_TARGET_BUFFER_AWBBG_target, {65536, 0, 21}}, // TARGET_BUFFER_AWBBG
    {"TARGET_BUFFER_BF", &TestIFEFullStats_TARGET_BUFFER_BF_target, {65536, 0, 20}}, // TARGET_BUFFER_BF
    {"TARGET_BUFFER_RS", &TestIFEFullStats_TARGET_BUFFER_RS_target, {65536, 0, 12}}, // TARGET_BUFFER_RS
    {"TARGET_BUFFER_CS", &TestIFEFullStats_TARGET_BUFFER_CS_target, {65536, 0, 13}}, // TARGET_BUFFER_CS
    {"TARGET_BUFFER_IHIST", &TestIFEFullStats_TARGET_BUFFER_IHIST_target, {65536, 0, 15}}, // TARGET_BUFFER_IHIST
};

static ChiOutputPortDescriptor TestIFEFullStats_IfeFullStatsNode0_0OutputPortDescriptors[] =
{
    {0, 0, 0, 0, 0, NULL}, // SensorOutputPort0
};

static ChiInputPortDescriptor TestIFEFullStats_IfeFullStatsNode65536_0InputPortDescriptors[] =
{
    {2, 0, 0}, // IFEInputPortSensor
};

static ChiOutputPortDescriptor TestIFEFullStats_IfeFullStatsNode65536_0OutputPortDescriptors[] =
{
    {0, 1, 1, 0, 0, NULL}, // IFEOutputPortFull
    {17, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsHDRBE
    {21, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsAWBBG
    {20, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsBF
    {12, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsRS
    {13, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsCS
    {15, 1, 1, 0, 0, NULL}, // IFEOutputPortStatsIHIST
};

static ChiNode TestIFEFullStats_IfeFullStatsNodes[] =
{
    {NULL, 0, 0, { 0, NULL, 1, TestIFEFullStats_IfeFullStatsNode0_0OutputPortDescriptors }, 0},
    {NULL, 65536, 0, { 1, TestIFEFullStats_IfeFullStatsNode65536_0InputPortDescriptors, 7, TestIFEFullStats_IfeFullStatsNode65536_0OutputPortDescriptors }, 0},
};

static ChiNodeLink TestIFEFullStats_IfeFullStatsLinks[] =
{
    {{0, 0, 0, 0}, 1, TestIFEFullStats_IfeFullStatsLink0DestDescriptors, {0}, {0}},
    {{65536, 0, 0, 0}, 1, TestIFEFullStats_IfeFullStatsLink1DestDescriptors, {0}, {0}},
    {{65536, 0, 17, 0}, 1, TestIFEFullStats_IfeFullStatsLink2DestDescriptors, {0}, {0}},
    {{65536, 0, 21, 0}, 1, TestIFEFullStats_IfeFullStatsLink3DestDescriptors, {0}, {0}},
    {{65536, 0, 20, 0}, 1, TestIFEFullStats_IfeFullStatsLink4DestDescriptors, {0}, {0}},
    {{65536, 0, 12, 0}, 1, TestIFEFullStats_IfeFullStatsLink5DestDescriptors, {0}, {0}},
    {{65536, 0, 13, 0}, 1, TestIFEFullStats_IfeFullStatsLink6DestDescriptors, {0}, {0}},
    {{65536, 0, 15, 0}, 1, TestIFEFullStats_IfeFullStatsLink7DestDescriptors, {0}, {0}},
};

enum TestIFEFullStatsPipelineIds
{
    IfeFullStats = 0,
};

static ChiPipelineTargetCreateDescriptor TestIFEFullStats_pipelines[] =
{
    {"IfeFullStats", { 0, 2, TestIFEFullStats_IfeFullStatsNodes, 8, TestIFEFullStats_IfeFullStatsLinks, 1}, {7, TestIFEFullStats_IfeFullStats_sink_TargetDescriptors}, {0, NULL}},  // IfeFullStats
};

static ChiUsecase Usecases1Target[] =
{
    {"TestIFEFull"                                                         , 0, 1, TestIFEFull_Targets                                                   , 1, TestIFEFull_pipelines                                                 },
    {"TestIPEFull"                                                         , 0, 1, TestIPEFull_Targets                                                   , 1, TestIPEFull_pipelines                                                 },
    {"TestIFEFullTestgen"                                                  , 0, 1, TestIFEFullTestgen_Targets                                            , 1, TestIFEFullTestgen_pipelines                                          },
    {"UsecaseRaw"                                                          , 0, 1, UsecaseRaw_Targets                                                    , 3, UsecaseRaw_pipelines                                                  },
    {"TestCustomNode"                                                      , 0, 1, TestCustomNode_Targets                                                , 1, TestCustomNode_pipelines                                              },
    {"TestMemcpy"                                                          , 0, 1, TestMemcpy_Targets                                                    , 2, TestMemcpy_pipelines                                                  },
};

static ChiUsecase Usecases2Target[] =
{
    {"TestOfflineIPE"                                                      , 0, 2, TestOfflineIPE_Targets                                                , 2, TestOfflineIPE_pipelines                                              },
    {"TestOfflineJPEG"                                                     , 0, 2, TestOfflineJPEG_Targets                                               , 1, TestOfflineJPEG_pipelines                                             },
    {"TestOfflineBPSIPE"                                                   , 0, 2, TestOfflineBPSIPE_Targets                                             , 2, TestOfflineBPSIPE_pipelines                                           },
};

static ChiUsecase Usecases3Target[] =
{
    {"TestZSLUseCase"                                                      , 0, 3, TestZSLUseCase_Targets                                                , 7, TestZSLUseCase_pipelines                                              },
    {"TestOfflineIPESIMO"                                                  , 0, 3, TestOfflineIPESIMO_Targets                                            , 1, TestOfflineIPESIMO_pipelines                                          },
    {"OfflineBPSSingleStatWithIPE"                                         , 0, 3, OfflineBPSSingleStatWithIPE_Targets                                   , 2, OfflineBPSSingleStatWithIPE_pipelines                                 },
};

static ChiUsecase Usecases4Target[] =
{
    {"OfflineBPSStats"                                                     , 0, 4, OfflineBPSStats_Targets                                               , 1, OfflineBPSStats_pipelines                                             },
    {"OfflineBPSStatsWithIPE"                                              , 0, 4, OfflineBPSStatsWithIPE_Targets                                        , 1, OfflineBPSStatsWithIPE_pipelines                                      },
};

static ChiUsecase Usecases5Target[] =
{
    {"TestPreviewCallbackSnapshotWithThumbnail"                            , 0, 5, TestPreviewCallbackSnapshotWithThumbnail_Targets                      , 3, TestPreviewCallbackSnapshotWithThumbnail_pipelines                    },
    {"TestGPUDSPorts"                                                      , 0, 5, TestGPUDSPorts_Targets                                                , 1, TestGPUDSPorts_pipelines                                              },
};

static ChiUsecase Usecases7Target[] =
{
    {"TestIFEFullStatsTestgen"                                             , 0, 7, TestIFEFullStatsTestgen_Targets                                       , 1, TestIFEFullStatsTestgen_pipelines                                     },
    {"TestIFEFullStats"                                                    , 0, 7, TestIFEFullStats_Targets                                              , 1, TestIFEFullStats_pipelines                                            },
};

static const UINT ChiMaxNumTargets = 7;

static struct ChiTargetUsecases PerNumTargetUsecases[] =
{	{6, Usecases1Target},
	{3, Usecases2Target},
	{3, Usecases3Target},
	{2, Usecases4Target},
	{2, Usecases5Target},
	{0, NULL},
	{2, Usecases7Target},
};


enum UsecaseId1Target
{
    TestIFEFullId  = 0,
    TestIPEFullId,
    TestIFEFullTestgenId,
    UsecaseRawId,
    TestCustomNodeId,
    TestMemcpyId,
};

enum UsecaseId2Target
{
    TestOfflineIPEId  = 0,
    TestOfflineJPEGId,
    TestOfflineBPSIPEId,
};

enum UsecaseId3Target
{
    TestZSLUseCaseId  = 0,
    TestOfflineIPESIMOId,
    OfflineBPSSingleStatWithIPEId,
};

enum UsecaseId4Target
{
    OfflineBPSStatsId  = 0,
    OfflineBPSStatsWithIPEId,
};

enum UsecaseId5Target
{
    TestPreviewCallbackSnapshotWithThumbnailId  = 0,
    TestGPUDSPortsId,
};

enum UsecaseId7Target
{
    TestIFEFullStatsTestgenId  = 0,
    TestIFEFullStatsId,
};

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // USECASEDEFS_H
