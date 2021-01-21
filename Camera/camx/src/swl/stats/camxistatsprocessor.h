////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxistatsprocessor.h
/// @brief The interface class for stats processor.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXISTATSPROCESSOR_H
#define CAMXISTATSPROCESSOR_H

#include "camxmem.h"
#include "camxutils.h"
#include "camxstatscommon.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Interface class for stats processor.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IStatsProcessor
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Used to initialize the class. This function
    ///         will load OEM or default algoritm, if the
    ///         statistics node is not getting destructed
    ///         while switching between the Cameras.
    ///
    /// @param  pInitializeData Pointer to initial settings
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize(
        const StatsInitializeData* pInitializeData) = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ExecuteProcessRequest
    ///
    /// @brief  Executes the algoritm for a request.
    ///
    /// @param  pStatsProcessRequestDataInfo Pointer to process request information
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult ExecuteProcessRequest(
        const StatsProcessRequestData* pStatsProcessRequestDataInfo) = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetDependencies
    ///
    /// @brief  Get the the list of dependencies from all stats processors.
    ///
    /// @param  pStatsProcessRequestDataInfo    Pointer to process request information
    /// @param  pStatsDependency            Pointer to stats dependencies
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult GetDependencies(
        const StatsProcessRequestData*  pStatsProcessRequestDataInfo,
        StatsDependency*                pStatsDependency) = 0;

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
        UINT32*         pPartialTagCount) = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Destroy
    ///
    /// @brief  Destroy the concreate class
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline VOID Destroy()
    {
        CAMX_DELETE this;
    }

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IStatsProcessor
    ///
    /// @brief  Protected destructor. Should not call delete on IStatsProcessor object.
    ///
    /// @param  None
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IStatsProcessor() = default;
};

CAMX_NAMESPACE_END

#endif // CAMXISTATSPROCESSOR_H
