////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxcawbstatsprocessor.h
/// @brief The class that implements IStatsProcessor for AWB.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXCAWBSTATSPROCESSOR_H
#define CAMXCAWBSTATSPROCESSOR_H

#include "chiawbinterface.h"
#include "chistatsproperty.h"
#include "camxtuningdatamanager.h"
#include "camxistatsprocessor.h"
#include "camxutils.h"

CAMX_NAMESPACE_BEGIN

/// Forward Declarations
class CAWBIOUtil;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief The class that implements IStatsProcessor for AWB.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAWBStatsProcessor final : public IStatsProcessor
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create the object for CAWBStatsProcessor.
    ///
    /// @param  ppAWBStatsProcessor Pointer to CAWBStatsProcessor
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IStatsProcessor**   ppAWBStatsProcessor);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Used to initialize the class.
    ///
    /// @param  pInitializeData Pointer to initial settings
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize(
        const StatsInitializeData*  pInitializeData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ExecuteProcessRequest
    ///
    /// @brief  Executes the algorithm for a request.
    ///
    /// @param  pStatsProcessRequestDataInfo Pointer to process request information
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ExecuteProcessRequest(
        const StatsProcessRequestData*  pStatsProcessRequestDataInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetDependencies
    ///
    /// @brief  Get the list of dependencies from all stats processors.
    ///
    /// @param  pStatsProcessRequestDataInfo    Pointer to process request information
    /// @param  pStatsDependency                Pointer to stats dependencies
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult GetDependencies(
        const StatsProcessRequestData*  pStatsProcessRequestDataInfo,
        StatsDependency*                pStatsDependency);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetPublishList
    ///
    /// @brief  Get the the list of tags published by all stats processors.
    ///
    /// @param  maxTagArraySize  Maximum size of pTagArray
    /// @param  pTagArray        Array of tags that are published by the stats processor.
    /// @param  pTagCount        Number of tags published by the stats processor
    /// @param  pPartialTagCount Number of Partialtags published by the stats processor
    ///
    /// @return CamxResultSuccess if successful, return failure if the number of tags to be published exceeds maxTagArraySize
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult GetPublishList(
        const UINT32    maxTagArraySize,
        UINT32*         pTagArray,
        UINT32*         pTagCount,
        UINT32*         pPartialTagCount);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsDependenciesSatisfied
    ///
    /// @brief  Checks if the dependencies is satisfied for a family.
    ///
    /// @param  pStatsProcessRequestDataInfo    Pointer to process request information
    /// @param  pIsSatisfied                    Pointer to Is dependencies satisfied
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult IsDependenciesSatisfied(
        const StatsProcessRequestData*  pStatsProcessRequestDataInfo,
        BOOL*                           pIsSatisfied);

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~CAWBStatsProcessor
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ~CAWBStatsProcessor();

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CAWBStatsProcessor
    ///
    /// @brief  Constructor.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAWBStatsProcessor();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// VendorTagListAllocation
    ///
    /// @brief  Function to fetch vendor tag list and allocate memory for them
    ///
    /// @return Success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult VendorTagListAllocation();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AddOEMDependencies
    ///
    /// @brief  Get the list of OEM dependencies from all stats processors.
    ///
    /// @param  pStatsProcessRequestDataInfo    Pointer to process request information
    /// @param  pStatsDependency                Pointer to list of property dependencies
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult AddOEMDependencies(
        const StatsProcessRequestData*  pStatsProcessRequestDataInfo,
        StatsDependency*                pStatsDependency);


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AlgoGetParam
    ///
    /// @brief  Populate the input/output for the get param type and query algo.
    ///
    /// @param  pStatsProcessRequestDataInfo    Pointer to process request information
    /// @param  pGetParam                       Pointer to get param inputs and outputs
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult AlgoGetParam(
        const StatsProcessRequestData*  pStatsProcessRequestDataInfo,
        AWBAlgoGetParam*                pGetParam);


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// OverwriteAWBOutput
    ///
    /// @brief  Hard codes AWB outputs
    ///
    /// @param  pStatsProcessRequestDataInfo Pointer to process request information
    /// @param  pOutputList                  Pointer to overwrote result
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID OverwriteAWBOutput(
        const StatsProcessRequestData* pStatsProcessRequestDataInfo,
        AWBAlgoOutputList*             pOutputList);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetDefaultSensorResolution
    ///
    /// @brief  Set Default sensor resolution for first frame.
    ///
    /// @return Success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult SetDefaultSensorResolution();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PublishPreRequestOutput
    ///
    /// @brief  Publishes the output for first two slots. This will be done even before the first
    ///         process request comes. The output from stats node will be used by the dependent nodes.
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult PublishPreRequestOutput();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetOperationModetoAlgo
    ///
    /// @brief  Set the operation Mode to algo
    ///
    /// @param  opMode Stats Operation Mode
    ///
    /// @return Success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult SetOperationModetoAlgo(
        StatsOperationMode opMode);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsValidAlgoOutput
    ///
    /// @brief  Check if algo returned all the outputs and with valid size
    ///
    /// @param  pAlgoOutputList Pointer to output list from the algorithm
    /// @param  requestId       Request ID
    ///
    /// @return Success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult IsValidAlgoOutput(
        AWBAlgoOutputList* pAlgoOutputList,
        UINT64             requestId);

    CAWBStatsProcessor(const CAWBStatsProcessor&)               = delete;   ///< Do not implement copy constructor
    CAWBStatsProcessor& operator=(const CAWBStatsProcessor&)    = delete;   ///< Do not implement assignment operator
    CAWBIOUtil*             m_pAWBIOUtil;                       ///< Pointer to instance of AWB Input/Output utility class
    CHIAWBAlgorithm*        m_pAWBAlgorithm;                    ///< Pointer to instance of AWB algorithm interface
    CamX::OSLIBRARYHANDLE   m_hHandle;                          ///< handle for custom algo.
    CREATEAWB               m_pfnCreate;                        ///< Pointer to algorithm entry function
    StatsCameraInfo         m_cameraInfo;                       ///< Camera Info for Dual Camera Sync
    Node*                   m_pNode;                            ///< Pointer to owning StatsProcessor node
    BOOL                    m_isFixedFocus;                     ///< Flag indicating whether sensor is fixed focus.
    StatsVendorTagInfoList* m_pOutputVendorTagInfoList;         ///< List of Algo's output vendor tags
    const StaticSettings*   m_pStaticSettings;                  ///< Camx static settings
};

CAMX_NAMESPACE_END

#endif // CAMXCAWBSTATSPROCESSOR_H
