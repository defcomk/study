//*******************************************************************************************
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//*******************************************************************************************

#include "chipipelineutils.h"

/**************************************************************************************************
 *   PipelineUtils::SetupPipeline
 *
 *   @brief
 *       Setup Pipeline create parameters for a given usecase
 *   @param
 *       [in]  BufferManager *                      manager                Pointer to buffer manager
 *       [out] PipelineCreateData*                  pPipelineCreate        Pointer to PipelineCreateData
 *       [in]  PipelineType                         type                   PipeLine type for this usecase
 *   @return
 *       CDKResult result
 **************************************************************************************************/
CDKResult PipelineUtils::SetupPipeline(
    std::map<StreamUsecases, ChiStream*> streamIdMap,
    PipelineCreateData* pPipelineCreate,
    PipelineType type)
{
    NATIVETEST_UNUSED_PARAM(m_nodes);
    NATIVETEST_UNUSED_PARAM(m_ports);
    NATIVETEST_UNUSED_PARAM(m_links);
    NATIVETEST_UNUSED_PARAM(m_inputPorts);
    NATIVETEST_UNUSED_PARAM(m_outputPorts);
    NATIVETEST_UNUSED_PARAM(m_linkNodeDescriptors);

    CDKResult result = CDKResultSuccess;
    pPipelineCreate->pInputBufferRequirements  = &m_pipelineInputBufferRequirements;
    pPipelineCreate->pOutputDescriptors        = m_pipelineOutputBuffer;
    pPipelineCreate->pInputDescriptors         = &m_pipelineInputBuffer;
    pPipelineCreate->pPipelineCreateDescriptor = &m_pipelineDescriptor;

    switch (type)
    {
    case PipelineType::RealtimeIFERDI0:
        result = SetupRealtimePipelineIFERDIO(streamIdMap);
        pPipelineCreate->numInputs = 1;
        pPipelineCreate->numOutputs = 1;
        break;
    case PipelineType::RealtimeIFERDI1:
        result = SetupRealtimePipelineIFERDI1(streamIdMap);
        pPipelineCreate->numInputs = 1;
        pPipelineCreate->numOutputs = 1;
        break;
    case PipelineType::RealtimeIFERDI2:
        result = SetupRealtimePipelineIFERDI2(streamIdMap);
        pPipelineCreate->numInputs = 1;
        pPipelineCreate->numOutputs = 1;
        break;
    case PipelineType::RealtimeIFERDI3:
        result = SetupRealtimePipelineIFERDI3(streamIdMap);
        pPipelineCreate->numInputs = 1;
        pPipelineCreate->numOutputs = 1;
        break;

    default:
        result = CDKResultEInvalidArg;
        AIS_LOG_CHI_PIPELINE_ERR("Invalid pipeline type: [%d]", type);
    }
    return result;
}

/**************************************************************************************************
 *   PipelineUtils::SetupRealtimePipelineIFERDIO
 *
 *   @brief
 *       Setup Pipeline parameters for real time ife Raw RDI0 usecase
 *   @param
 *       [in]  std::map<StreamUsecases, ChiStream*>    streamIdMap     Map conatining stream objects
 *   @return
 *       CDKResult result
 **************************************************************************************************/
CDKResult PipelineUtils::SetupRealtimePipelineIFERDIO(std::map<StreamUsecases, ChiStream*> streamIdMap)
{
    m_pipelineDescriptor = Usecases1Target[UsecaseRawId].pPipelineTargetCreateDesc[RDI0].pipelineCreateDesc;

    UINT32 outputBufferIndex = 0;
    m_pipelineOutputBuffer[outputBufferIndex].size = sizeof(m_pipelineOutputBuffer);
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodeId = nodeid_t::IFE;
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodeInstanceId = 0;
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodePortId = ifeportid_t::IFEOutputPortRDI0;
    if (streamIdMap.find(StreamUsecases::IFEOutputPortRDI0) == streamIdMap.end())
    {
        AIS_LOG_CHI_PIPELINE_ERR("Cannot find chistream for IFEOutputPortRDI0");
        return CDKResultENoSuch;
    }
    m_pipelineOutputBuffer[outputBufferIndex].pStream = streamIdMap[StreamUsecases::IFEOutputPortRDI0];
    return CDKResultSuccess;
}


/**************************************************************************************************
 *   PipelineUtils::SetupRealtimePipelineIFERDI1
 *
 *   @brief
 *       Setup Pipeline parameters for real time ife Raw RDI1 usecase
 *   @param
 *       [in]  std::map<StreamUsecases, ChiStream*>    streamIdMap     Map conatining stream objects
 *   @return
 *       CDKResult result
 **************************************************************************************************/
CDKResult PipelineUtils::SetupRealtimePipelineIFERDI1(std::map<StreamUsecases, ChiStream*> streamIdMap)
{
    m_pipelineDescriptor = Usecases1Target[UsecaseRawId].pPipelineTargetCreateDesc[RDI1].pipelineCreateDesc;

    UINT32 outputBufferIndex = 0;
    m_pipelineOutputBuffer[outputBufferIndex].size = sizeof(m_pipelineOutputBuffer);
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodeId = nodeid_t::IFE;
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodeInstanceId = 1;
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodePortId = ifeportid_t::IFEOutputPortRDI1;
    if (streamIdMap.find(StreamUsecases::IFEOutputPortRDI1) == streamIdMap.end())
    {
        AIS_LOG_CHI_PIPELINE_ERR("Cannot find chistream for IFEOutputPortRDI1");
        return CDKResultENoSuch;
    }
    m_pipelineOutputBuffer[outputBufferIndex].pStream = streamIdMap[StreamUsecases::IFEOutputPortRDI1];
    return CDKResultSuccess;
}

/**************************************************************************************************
 *   PipelineUtils::SetupRealtimePipelineIFERDI2
 *
 *   @brief
 *       Setup Pipeline parameters for real time ife Raw RDI2 usecase
 *   @param
 *       [in]  std::map<StreamUsecases, ChiStream*>    streamIdMap     Map conatining stream objects
 *   @return
 *       CDKResult result
 **************************************************************************************************/
CDKResult PipelineUtils::SetupRealtimePipelineIFERDI2(std::map<StreamUsecases, ChiStream*> streamIdMap)
{
    m_pipelineDescriptor = Usecases1Target[UsecaseRawId].pPipelineTargetCreateDesc[RDI2].pipelineCreateDesc;

    UINT32 outputBufferIndex = 0;
    m_pipelineOutputBuffer[outputBufferIndex].size = sizeof(m_pipelineOutputBuffer);
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodeId = nodeid_t::IFE;
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodeInstanceId = 2;
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodePortId = ifeportid_t::IFEOutputPortRDI2;
    if (streamIdMap.find(StreamUsecases::IFEOutputPortRDI2) == streamIdMap.end())
    {
        AIS_LOG_CHI_PIPELINE_ERR("Cannot find chistream for IFEOutputPortRDI2");
        return CDKResultENoSuch;
    }
    m_pipelineOutputBuffer[outputBufferIndex].pStream = streamIdMap[StreamUsecases::IFEOutputPortRDI2];
    return CDKResultSuccess;
}

/**************************************************************************************************
 *   PipelineUtils::SetupRealtimePipelineIFERDI3
 *
 *   @brief
 *       Setup Pipeline parameters for real time ife Raw RDI3 usecase
 *   @param
 *       [in]  std::map<StreamUsecases, ChiStream*>    streamIdMap     Map conatining stream objects
 *   @return
 *       CDKResult result
 **************************************************************************************************/
CDKResult PipelineUtils::SetupRealtimePipelineIFERDI3(std::map<StreamUsecases, ChiStream*> streamIdMap)
{
    m_pipelineDescriptor = Usecases1Target[UsecaseRawId].pPipelineTargetCreateDesc[RDI3].pipelineCreateDesc;

    UINT32 outputBufferIndex = 0;
    m_pipelineOutputBuffer[outputBufferIndex].size = sizeof(m_pipelineOutputBuffer);
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodeId = nodeid_t::IFE;
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodeInstanceId = 3;
    m_pipelineOutputBuffer[outputBufferIndex].nodePort.nodePortId = ifeportid_t::IFEOutputPortRDI0;
    if (streamIdMap.find(StreamUsecases::IFEOutputPortRDI3) == streamIdMap.end())
    {
        AIS_LOG_CHI_PIPELINE_ERR("Cannot find chistream for IFEOutputPortRDI3");
        return CDKResultENoSuch;
    }
    m_pipelineOutputBuffer[outputBufferIndex].pStream = streamIdMap[StreamUsecases::IFEOutputPortRDI3];
    return CDKResultSuccess;
}
