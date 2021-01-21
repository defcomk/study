//*************************************************************************************************
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//*************************************************************************************************

#ifndef __CHI_PIPELINE_UTILS__
#define __CHI_PIPELINE_UTILS__

#include "test_pipelines.h"

#define AIS_LOG_CHI_PIPELINE(level, fmt...) AIS_LOG(AIS_MOD_CHI_PIPELINE, level, fmt)
#define AIS_LOG_CHI_PIPELINE_ERR(fmt...)    AIS_LOG(AIS_MOD_CHI_PIPELINE, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_CHI_PIPELINE_HIGH(fmt...)   AIS_LOG(AIS_MOD_CHI_PIPELINE, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CHI_PIPELINE_WARN(fmt...)   AIS_LOG(AIS_MOD_CHI_PIPELINE, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_CHI_PIPELINE_HIGH(fmt...)   AIS_LOG(AIS_MOD_CHI_PIPELINE, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CHI_PIPELINE_MED(fmt...)    AIS_LOG(AIS_MOD_CHI_PIPELINE, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_CHI_PIPELINE_LOW(fmt...)    AIS_LOG(AIS_MOD_CHI_PIPELINE, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_CHI_PIPELINE_DBG(fmt...)    AIS_LOG(AIS_MOD_CHI_PIPELINE, AIS_LOG_LVL_DBG, fmt)
#define AIS_LOG_CHI_PIPELINE_DBG1(fmt...)   AIS_LOG(AIS_MOD_CHI_PIPELINE, AIS_LOG_LVL_DBG1, fmt)


    class PipelineUtils
    {
    public:
        PipelineUtils() = default;
        ~PipelineUtils() = default;

        CDKResult SetupPipeline(
            std::map<StreamUsecases, ChiStream*> streamIdMap,
            PipelineCreateData* pPipelineCreate,
            PipelineType type);

    private:

        /// @brief Nodeid
        typedef enum NodeId
        {
            Sensor           = 0,
            StatsProcessing  = 1,
            JPEGAggregator   = 6,
            FDManager        = 8,
            StatsParse       = 9,
            AutoWhiteBalance = 12,
            External         = 255,
            IFE              = 65536,
            JPEG,
            IPE,
            BPS,
            FDHw,
        }nodeid_t;

        /// @brief Sensor ports
        typedef enum SensorPortId
        {
            SensorOutputPort0 =0
        }sensorportid_t;

        /// @brief IfePorts
        typedef enum IfePortId
        {
            IFEOutputPortFull          = 0,
            IFEOutputPortDS4           = 1,
            IFEOutputPortDS16          = 2 ,
            IFEOutputPortCAMIFRaw      = 3,
            IFEOutputPortLSCRaw        = 4,
            IFEOutputPortGTMRaw        = 5,
            IFEOutputPortFD            = 6,
            IFEOutputPortPDAF          = 7,
            IFEOutputPortRDI0          = 8,
            IFEOutputPortRDI1          = 9,
            IFEOutputPortRDI2          = 10,
            IFEOutputPortRDI3          = 11,
            IFEOutputPortStatsRS       = 12,
            IFEOutputPortStatsCS       = 13,
            IFEOutputPortStatsIHIST    = 15,
            IFEOutputPortStatsBHIST    = 16,
            IFEOutputPortStatsHDRBE    = 17,
            IFEOutputPortStatsHDRBHIST = 18,
            IFEOutputPortStatsTLBG     = 19,
            IFEOutputPortStatsBF       = 20,
            IFEOutputPortStatsAWBBG    = 21,
            IFEOutputPortDepth         = 22,
            IFEInputPortCSIDTPG = 0,
            IFEInputPortCAMIFTPG,
            IFEInputPortSensor
        }ifeportid_t;

        /// @brief IpePorts
        typedef enum IpePortId
        {
            IPEInputPortFull = 0,
            IPEInputPortDS4,
            IPEInputPortDS16,
            IPEInputPortDS64,
            IPEInputPortFullRef,
            IPEInputPortDS4Ref,
            IPEInputPortDS16Ref,
            IPEInputPortDS64Ref,
            IPEOutputPortDisplay,
            IPEOutputPortVideo,
            IPEOutputPortFullRef,
            IPEOutputPortDS4Ref,
            IPEOutputPortDS16Ref,
            IPEOutputPortDS64Ref
        }ipeportid_t;

        /// @brief BpsPorts
        typedef enum BpsPortId
        {
            BPSInputPort = 0,
            BPSOutputPortFull,
            BPSOutputPortDS4,
            BPSOutputPortDS16,
            BPSOutputPortDS64,
            BPSOutputPortStatsBG,
            BPSOutputPortStatsBHist,
            BPSOutputPortReg1,
            BPSOutputPortReg2
        }bpsportid_t;

        /// @brief GpuPorts
        typedef enum GPUPortId
        {
            GPUInputPort = 0,
            GPUOutputPortFull = 0,
            GPUOutputPortDS4,
            GPUOutputPortDS16,
            GPUOutputPortDS64,
        }gpuportid_t;

        /// @brief JpegPorts
        typedef enum JpegAggPortId
        {
            JPEGAggregatorInputPort0 = 0,
            JPEGAggregatorInputPort1 = 1,
            JPEGAggregatorOutputPort = 2

        }jpegaggportid_t;

        /// @brief JpegPorts
        typedef enum JpegPortId
        {
            JPEGInputPort = 0,
            JPEGOutputPort

        }jpegportid_t;

        /// @brief FDHw
        typedef enum FDHwPortId
        {
            FDHwInputPortImage = 0,

        }fdhwportid_t;

        /// @brief FD manager
        typedef enum FDMgrPortId
        {
            FDManagerInputPort = 0,
            FDManagerOutputPort = 0

        }fdmgrportid_t;

        /// @brief ChiExternalNode
        typedef enum ExternalNodeId
        {
            ChiNodeInputPort0  = 0,
            ChiNodeOutputPort0 = 0,
            ChiNodeOutputPort1
        }externalportid_t;

        static const UINT MAX_NODES = 20;
        static const UINT MAX_NODE_PORTS = 20;
        static const UINT MAX_LINKS = 20;
        static const UINT MAX_PORTS = 20;
        static const UINT MAX_OUTPUT_BUFFERS = 11;


        CHIPIPELINECREATEDESCRIPTOR  m_pipelineDescriptor;
        CHINODE                      m_nodes[MAX_NODES];
        CHINODEPORTS                 m_ports[MAX_NODE_PORTS];
        CHINODELINK                  m_links[MAX_LINKS];
        CHIINPUTPORTDESCRIPTOR       m_inputPorts[MAX_PORTS];
        CHIOUTPUTPORTDESCRIPTOR      m_outputPorts[MAX_PORTS];
        CHILINKNODEDESCRIPTOR        m_linkNodeDescriptors[MAX_LINKS];
        CHIPORTBUFFERDESCRIPTOR      m_pipelineOutputBuffer[MAX_OUTPUT_BUFFERS];
        CHIPORTBUFFERDESCRIPTOR      m_pipelineInputBuffer;
        CHIPIPELINEINPUTOPTIONS      m_pipelineInputBufferRequirements;
        const CHAR*                  m_pipelineName;

        CDKResult SetupRealtimePipelineIFERDIO(std::map<StreamUsecases, ChiStream*> streamIdMap);
        CDKResult SetupRealtimePipelineIFERDI1(std::map<StreamUsecases, ChiStream*> streamIdMap);
        CDKResult SetupRealtimePipelineIFERDI2(std::map<StreamUsecases, ChiStream*> streamIdMap);
        CDKResult SetupRealtimePipelineIFERDI3(std::map<StreamUsecases, ChiStream*> streamIdMap);
    };



#endif  //#ifndef CHI_PIPELINE_UTILS
