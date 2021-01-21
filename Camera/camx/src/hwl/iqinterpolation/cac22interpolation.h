// NOWHINE NC009 <- Shared file with system team so uses non-CamX file naming
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// @file  cac22interpolation.h
// @brief CAC22 tuning data interpolation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAC22INTERPOLATION_H
#define CAC22INTERPOLATION_H

#include "cac_2_2_0.h"
#include "iqcommondefs.h"
#include "iqsettingutil.h"

// NOWHINE NC004c: Share code with system team
struct CAC22TriggerList
{
    cac_2_2_0::chromatix_cac22Type::control_methodStruct controlType;            ///< chromatix data
    FLOAT                                                triggerTotalScaleRatio; ///< TotalScaleRatio trigger
    FLOAT                                                triggerAEC;             ///< AEC trigger
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class that implements CAC22 module setting calculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOWHINE NC004c: Share code with system team
class CAC22Interpolation
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunInterpolation
    ///
    /// @brief  Perform the interpolation.
    ///
    /// @param  pInput Input data to the CAC22 Module
    /// @param  pData  The output of the interpolation algorithem
    ///
    /// @return TRUE for success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BOOL RunInterpolation(
        const CAC22InputData*          pInput,
        cac_2_2_0::cac22_rgn_dataType* pData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DoInterpolation
    ///
    /// @brief  CAC22 Interpolation function
    ///
    /// @param  pData1   Pointer to the input data set 1
    /// @param  pData2   Pointer to the input data set 2
    /// @param  ratio    Interpolation Ratio
    /// @param  pOutput  Pointer to the result set
    ///
    /// @return TRUE for success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BOOL DoInterpolation(
        VOID* pData1,
        VOID* pData2,
        FLOAT ratio,
        VOID* pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AECSearchNode
    ///
    /// @brief  Search Function for AEC Nodes
    ///
    /// @param  pParentNode     Pointer to the Parent Node
    /// @param  pTriggerData    Pointer to the Trigger Value List
    /// @param  pChildNode      Pointer to the Child Node
    ///
    /// @return TRUE for success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static UINT AECSearchNode(
        TuningNode* pParentNode,
        VOID*       pTriggerData,
        TuningNode* pChildNode);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// TotalScaleRatioSearchNode
    ///
    /// @brief  Search Function for AEC Nodes
    ///
    /// @param  pParentNode     Pointer to the Parent Node
    /// @param  pTriggerData    Pointer to the Trigger Value List
    /// @param  pChildNode      Pointer to the Child Node
    ///
    /// @return TRUE for success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static UINT TotalScaleRatioSearchNode(
        TuningNode* pParentNode,
        VOID*       pTriggerData,
        TuningNode* pChildNode);

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // InterpolationData
    //
    // @brief  Interpolation Function for CAC22 Module
    //
    // @param  pInput1
    // @param  pInput2
    // @param  ratio
    // @param  pOutput
    //
    // @return VOID
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID InterpolationData(
        cac_2_2_0::cac22_rgn_dataType* pInput1,
        cac_2_2_0::cac22_rgn_dataType* pInput2,
        FLOAT                          ratio,
        cac_2_2_0::cac22_rgn_dataType* pOutput);
};

#endif // CAC22INTERPOLATION_H
