////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxmultistatsoperator.h
/// @brief Define the multi statistics operator for multi camera framework.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXMULTISTATSOPERATOR_H
#define CAMXMULTISTATSOPERATOR_H

#include "camxutils.h"
#include "camxstatscommon.h"
#include "chistatsinterfacedefs.h"

CAMX_NAMESPACE_BEGIN

// Max number of properties can be used in cross-pipeline dependency
static const UINT32 MaxCrossPipelineProperties  = 5;

enum class StatsAlgoSyncType
{
    StatsAlgoSyncTypeAEC,           ///< The Stats algorithm sync type for AEC
    StatsAlgoSyncTypeAF,            ///< The Stats algorithm sync type for AF
    StatsAlgoSyncTypeAWB,           ///< The Stats algorithm sync type for AWB
    StatsAlgoTypeMax                ///< The Stats algorithm sync type max
};

struct PropertyPair
{
    PropertyID          propertyIDs;                        ///< Property ID
    BOOL                isPeerDependency;                   ///< Is property dependency on peer side
};

struct MultiStatsDependency
{
    StatsAlgoSyncType   algoSyncType;                                   ///< Statistics algorithm sync type
    StatsAlgoRole       algoRole;                                       ///< Statistics algorithm role
    PropertyPair        propertyPair[MaxCrossPipelineProperties];       ///< Property dependency paris
    StatsAlgoAction     algoAction;                                     ///< Statistics algorithm action
};

struct MultiStatsData
{
    StatsAlgoSyncType   algoSyncType;                       ///< Statistics algorithm sync type
    StatsAlgoRole       algoRole;                           ///< Statistics algorithm role
    UINT                pipelineId;                         ///< Pipeline ID to which the Stats/AF node belongs
    UINT                peerPipelineId;                     ///< Pipeline ID to which the peer Stats/AF node belongs
    HwContext*          pHwContext;                         ///< Pointer to hardware context
    StatsAlgoAction     algoAction;                         ///< Statistics algorithm action
    BOOL                isSlaveOperational;                 ///< Indicate if slave has operation under LPM mode
    INT64               requestIdDelta;                     ///< Record the request ID delta between peer pipelines
    Node*               pNode;                              ///< Pointer to owning node
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Define the multi statistics operator for multi camera framework.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MultiStatsOperator
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetStatsAlgoRole
    ///
    /// @brief  Set stats algo's role
    ///
    /// @param  algoRole stats algo's role
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SetStatsAlgoRole(
        StatsAlgoRole algoRole);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetStatsAlgoRole
    ///
    /// @brief  Get stats algo's role
    ///
    /// @return StatsAlgoRole
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    StatsAlgoRole GetStatsAlgoRole();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize the multi stats operator.
    ///
    /// @param  pMultiStatsData   Initialization data.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID Initialize(
        const MultiStatsData* pMultiStatsData) = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateStatsDependencies
    ///
    /// @brief  Get the the list of dependencies from all stats processors.
    ///
    /// @param  pExecuteProcessRequestData      Pointer to nodes process request data
    /// @param  pStatsRequestData               Pointer to stats process request data
    /// @param  requestIdOffsetFromLastFlush    Offset of request ID From LastFlush
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult UpdateStatsDependencies(
        ExecuteProcessRequestData*  pExecuteProcessRequestData,
        StatsProcessRequestData*    pStatsRequestData,
        UINT64                      requestIdOffsetFromLastFlush) = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetStatsAlgoAction
    ///
    /// @brief  Get statistics algo's action
    ///
    /// @return StatsAlgoAction Return stats algo's action
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual StatsAlgoAction GetStatsAlgoAction() = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrintDependency
    ///
    /// @brief  Print dependency information
    ///
    /// @param  pStatsDependencies Dependencies to be printed
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PrintDependency(
        DependencyUnit*     pStatsDependencies);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// MultiStatsOperator
    ///
    /// @brief  Default Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    MultiStatsOperator() = default;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~MultiStatsOperator
    ///
    /// @brief  Default Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~MultiStatsOperator() = default;

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AddDependency
    ///
    /// @brief  Add statistics algo's dependency
    ///
    /// @param  pPropertyPair       Pointer to statistics property pair
    /// @param  pDependencyUnit     Pointer to node's execution dependency unit
    /// @param  requestId           Request ID
    /// @param  offset              Offset to request ID
    /// @param  negative            Indicate if offset is negative
    ///
    /// @return CamxResultSuccess   if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult AddDependency(
        PropertyPair*           pPropertyPair,
        DependencyUnit*         pDependencyUnit,
        UINT64                  requestId,
        UINT64                  offset,
        BOOL                    negative);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RemoveDependency
    ///
    /// @brief  Remove statistics algo's dependency
    ///
    /// @param  pStatsDependency    Pointer to statistics dependency
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID RemoveDependency(
        StatsDependency* pStatsDependency) = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ParseMultiRequestInfo
    ///
    /// @brief  Parse multi request information
    ///
    /// @param  pExecuteProcessRequestData      Pointer to nodes process request data
    /// @param  pStatsRequestData               Pointer to Stats process request data
    ///
    /// @return CamxResultSuccess   if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ParseMultiRequestInfo(
        ExecuteProcessRequestData*  pExecuteProcessRequestData,
        StatsProcessRequestData*    pStatsRequestData) = 0;

    MultiStatsData              m_multiStatsData;           ///< Multi Stats Operator private data

private:
    MultiStatsOperator(const MultiStatsOperator&) = delete;                 ///< Disallow the copy constructor.
    MultiStatsOperator& operator=(const MultiStatsOperator&) = delete;      ///< Disallow assignment operator.
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Define the multi statistics operator for default QTI defined multi stats operator.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class QTIMultiStatsOperator final : public MultiStatsOperator
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize the derived multi stats operator.
    ///
    /// @param  pMultiStatsData   Initialization data.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID Initialize(
        const MultiStatsData* pMultiStatsData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateStatsDependencies
    ///
    /// @brief  Get the the list of dependencies from all stats processors for derived multi stats operator.
    ///
    /// @param  pExecuteProcessRequestData      Pointer to nodes process request data
    /// @param  pStatsRequestData               Pointer to stats process request data
    /// @param  requestIdOffsetFromLastFlush    Offset of request ID From LastFlush
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult UpdateStatsDependencies(
        ExecuteProcessRequestData*  pExecuteProcessRequestData,
        StatsProcessRequestData*    pStatsRequestData,
        UINT64                      requestIdOffsetFromLastFlush);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetStatsAlgoAction
    ///
    /// @brief  Get statistics algo's action
    ///
    /// @return StatsAlgoAction                 Return stats algo's action
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual StatsAlgoAction GetStatsAlgoAction();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// MultiStatsOperator
    ///
    /// @brief  Default Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    QTIMultiStatsOperator() = default;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~MultiStatsOperator
    ///
    /// @brief  Default Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~QTIMultiStatsOperator() = default;

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RemoveDependency
    ///
    /// @brief  Remove statistics algo's dependency for derived multi stats operator.
    ///
    /// @param  pStatsDependency    Pointer to statistics dependency
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID RemoveDependency(
        StatsDependency* pStatsDependency);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ParseMultiRequestInfo
    ///
    /// @brief  Parse multi request information
    ///
    /// @param  pExecuteProcessRequestData      Pointer to nodes process request data
    /// @param  pStatsRequestData               Pointer to Stats process request data
    ///
    /// @return CamxResultSuccess   if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ParseMultiRequestInfo(
        ExecuteProcessRequestData*  pExecuteProcessRequestData,
        StatsProcessRequestData*    pStatsRequestData);

private:
    QTIMultiStatsOperator(const QTIMultiStatsOperator&) = delete;                 ///< Disallow the copy constructor.
    QTIMultiStatsOperator& operator=(const QTIMultiStatsOperator&) = delete;      ///< Disallow assignment operator.
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Define the multi statistics operator for singleton algorithm intance usecase.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SingletonStatsOperator final : public MultiStatsOperator
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize the derived multi stats operator.
    ///
    /// @param  pMultiStatsData   Initialization data.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID Initialize(
        const MultiStatsData* pMultiStatsData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateStatsDependencies
    ///
    /// @brief  Get the the list of dependencies from all stats processors for derived multi stats operator.
    ///
    /// @param  pExecuteProcessRequestData      Pointer to nodes process request data
    /// @param  pStatsRequestData               Pointer to stats process request data
    /// @param  requestIdOffsetFromLastFlush    Offset of request ID From LastFlush
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult UpdateStatsDependencies(
        ExecuteProcessRequestData*  pExecuteProcessRequestData,
        StatsProcessRequestData*    pStatsRequestData,
        UINT64                      requestIdOffsetFromLastFlush);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetStatsAlgoAction
    ///
    /// @brief  Get statistics algo's action
    ///
    /// @return StatsAlgoAction                 Return stats algo's action
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual StatsAlgoAction GetStatsAlgoAction();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// MultiStatsOperator
    ///
    /// @brief  Default Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    SingletonStatsOperator() = default;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~MultiStatsOperator
    ///
    /// @brief  Default Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~SingletonStatsOperator() = default;

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RemoveDependency
    ///
    /// @brief  Remove statistics algo's dependency for derived multi stats operator.
    ///
    /// @param  pStatsDependency    Pointer to statistics dependency
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID RemoveDependency(
        StatsDependency* pStatsDependency);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ParseMultiRequestInfo
    ///
    /// @brief  Parse multi request information
    ///
    /// @param  pExecuteProcessRequestData      Pointer to nodes process request data
    /// @param  pStatsRequestData               Pointer to Stats process request data
    ///
    /// @return CamxResultSuccess   if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ParseMultiRequestInfo(
        ExecuteProcessRequestData*  pExecuteProcessRequestData,
        StatsProcessRequestData*    pStatsRequestData);

private:
    SingletonStatsOperator(const SingletonStatsOperator&) = delete;                 ///< Disallow the copy constructor.
    SingletonStatsOperator& operator=(const SingletonStatsOperator&) = delete;      ///< Disallow assignment operator.
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Define the multi statistics operator for no 3A sync situation.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class NoSyncStatsOperator final : public MultiStatsOperator
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize the derived multi stats operator.
    ///
    /// @param  pMultiStatsData   Initialization data.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID Initialize(
        const MultiStatsData* pMultiStatsData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateStatsDependencies
    ///
    /// @brief  Get the the list of dependencies from all stats processors for derived multi stats operator.
    ///
    /// @param  pExecuteProcessRequestData      Pointer to nodes process request data
    /// @param  pStatsRequestData               Pointer to stats process request data
    /// @param  requestIdOffsetFromLastFlush    Offset of request ID From LastFlush
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult UpdateStatsDependencies(
        ExecuteProcessRequestData*  pExecuteProcessRequestData,
        StatsProcessRequestData*    pStatsRequestData,
        UINT64                      requestIdOffsetFromLastFlush);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetStatsAlgoAction
    ///
    /// @brief  Get statistics algo's action
    ///
    /// @return StatsAlgoAction                 Return stats algo's action
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual StatsAlgoAction GetStatsAlgoAction();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// MultiStatsOperator
    ///
    /// @brief  Default Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    NoSyncStatsOperator() = default;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~MultiStatsOperator
    ///
    /// @brief  Default Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~NoSyncStatsOperator() = default;

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RemoveDependency
    ///
    /// @brief  Remove statistics algo's dependency for derived multi stats operator.
    ///
    /// @param  pStatsDependency    Pointer to statistics dependency
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID RemoveDependency(
        StatsDependency* pStatsDependency);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ParseMultiRequestInfo
    ///
    /// @brief  Parse multi request information
    ///
    /// @param  pExecuteProcessRequestData      Pointer to nodes process request data
    /// @param  pStatsRequestData               Pointer to Stats process request data
    ///
    /// @return CamxResultSuccess   if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ParseMultiRequestInfo(
        ExecuteProcessRequestData*  pExecuteProcessRequestData,
        StatsProcessRequestData*    pStatsRequestData);

private:
    NoSyncStatsOperator(const NoSyncStatsOperator&) = delete;                 ///< Disallow the copy constructor.
    NoSyncStatsOperator& operator=(const NoSyncStatsOperator&) = delete;      ///< Disallow assignment operator.
};

CAMX_NAMESPACE_END

#endif // CAMXMULTISTATSOPERATOR_H
