////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipechromasuppression20.h
/// @brief IPEChromaSuppression class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPECHROMASUPPRESSION20_H
#define CAMXIPECHROMASUPPRESSION20_H

#include "camxispiqmodule.h"
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief: This structure contains chroma enhancement knee point related registers
struct IPEChromaSuppressionKneePoint
{
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_PT_LUT_CFG_0 cfg0;  ///< Luma kneepoints 0 and 1 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_PT_LUT_CFG_1 cfg1;  ///< Luma kneepoints 2 and 3 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_PT_LUT_CFG_2 cfg2;  ///< Luma kneepoints 4 and 5 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_PT_LUT_CFG_3 cfg3;  ///< Luma kneepoints 6 and 7 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_PT_LUT_CFG_4 cfg4;  ///< Luma kneepoints 8 and 9 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_PT_LUT_CFG_5 cfg5;  ///< Luma kneepoints 10 and 11 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_PT_LUT_CFG_6 cfg6;  ///< Luma kneepoints 12 and 13 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_PT_LUT_CFG_7 cfg7;  ///< Luma kneepoints 14 and 15 in the luma
} CAMX_PACKED;

/// @brief: This structure contains chroma enhancement weight related registers
struct IPEChromaSuppressionWeight
{
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_WEIGHT_LUT_CFG_0  cfg0;    ///< Luma suppression ratio weights 0 and 1 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_WEIGHT_LUT_CFG_1  cfg1;    ///< Luma suppression ratio weights 2 and 3 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_WEIGHT_LUT_CFG_2  cfg2;    ///< Luma suppression ratio weights 4 and 5 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_WEIGHT_LUT_CFG_3  cfg3;    ///< Luma suppression ratio weights 6 and 7 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_WEIGHT_LUT_CFG_4  cfg4;    ///< Luma suppression ratio weights 8 and 9 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_WEIGHT_LUT_CFG_5  cfg5;    ///< Luma suppression ratio weights 10 and 11 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_WEIGHT_LUT_CFG_6  cfg6;    ///< Luma suppression ratio weights 12 and 13 in the luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_WEIGHT_LUT_CFG_7  cfg7;    ///< Luma suppression ratio weights 14 and 15 in the luma
} CAMX_PACKED;

/// @brief: This structure contains chroma enhancement threshold related registers
struct IPEChromaSuppressionThreshold
{
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_TH_LUT_CFG_0 cfg0;  ///< Chroma threshold 0 and 1 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_TH_LUT_CFG_1 cfg1;  ///< Chroma threshold 2 and 3 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_TH_LUT_CFG_2 cfg2;  ///< Chroma threshold 4 and 5 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_TH_LUT_CFG_3 cfg3;  ///< Chroma threshold 6 and 7 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_TH_LUT_CFG_4 cfg4;  ///< Chroma threshold 8 and 9 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_TH_LUT_CFG_5 cfg5;  ///< Chroma threshold 10 and 11 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_TH_LUT_CFG_6 cfg6;  ///< Chroma threshold 12 and 13 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_TH_LUT_CFG_7 cfg7;  ///< Chroma threshold 14 and 15 values
} CAMX_PACKED;

/// @brief: This structure contains chroma enhancement slope related registers
struct IPEChromaSuppressionChromaSlope
{
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_SLP_LUT_CFG_0 cfg0;  ///< Chroma slope 0 and 1 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_SLP_LUT_CFG_1 cfg1;  ///< Chroma slope 2 and 3 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_SLP_LUT_CFG_2 cfg2;  ///< Chroma slope 4 and 5 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_SLP_LUT_CFG_3 cfg3;  ///< Chroma slope 6 and 7 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_SLP_LUT_CFG_4 cfg4;  ///< Chroma slope 8 and 9 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_SLP_LUT_CFG_5 cfg5;  ///< Chroma slope 10 and 11 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_SLP_LUT_CFG_6 cfg6;  ///< Chroma slope 12 and 13 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_CHROMA_SLP_LUT_CFG_7 cfg7;  ///< Chroma slope 14 and 15 values
} CAMX_PACKED;

/// @brief: This structure contains chroma enhancement knee inverse related registers
struct IPEChromaSuppressionKneeinverse
{
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_INV_LUT_CFG_0 cfg0;  ///< Inverse of luma knee points 0 and 1 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_INV_LUT_CFG_1 cfg1;  ///< Inverse of luma knee points 2 and 3 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_INV_LUT_CFG_2 cfg2;  ///< Inverse of luma knee points 4 and 5 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_INV_LUT_CFG_3 cfg3;  ///< Inverse of luma knee points 6 and 7 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_INV_LUT_CFG_4 cfg4;  ///< Inverse of luma knee points 8 and 9 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_INV_LUT_CFG_5 cfg5;  ///< Inverse of luma knee points 10 and 11 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_INV_LUT_CFG_6 cfg6;  ///< Inverse of luma knee points 12 and 13 values
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Y_KNEE_INV_LUT_CFG_7 cfg7;  ///< Inverse of luma knee points 14 and 15 values
} CAMX_PACKED;

/// @brief: This structure contains chroma enhancement Q factor related registers
struct IPEChromaSuppressionQRes
{
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Q_RES_LUMA_CFG      luma;   ///< Number of bits for Fractional part of Luma
    IPE_IPE_0_PPS_CLC_CHROMA_SUP_CS_Q_RES_CHROMA_CFG    chroma; ///< Number of bits for Fractional part of Chroma
} CAMX_PACKED;

/// @brief: This structure contains chroma enhancement registers programmed by software
struct IPEChromaSuppressionRegCmd
{
    IPEChromaSuppressionKneePoint    knee;       ///< Luma kneepoints
    IPEChromaSuppressionWeight       weight;     ///< Luma suppression ratio weights
    IPEChromaSuppressionThreshold    threshold;  ///< Chroma threshold
    IPEChromaSuppressionChromaSlope  slope;      ///< Chroma slope
    IPEChromaSuppressionKneeinverse  inverse;    ///< Inverse of luma knee points
    IPEChromaSuppressionQRes         qRes;       ///<  Number of bits for Fractional part
} CAMX_PACKED;

CAMX_END_PACKED

static const UINT32 IPEChromaSuppressionRegLength = sizeof(IPEChromaSuppressionRegCmd) / RegisterWidthInBytes;
CAMX_STATIC_ASSERT((42 * 4) == sizeof(IPEChromaSuppressionRegCmd));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Class for IPE Chroma Suppression Module
///
///         This IQ block suppresses unwanted chroma noise in the brighter and darker portions of the image
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPEChromaSuppression20 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create ChromaSuppression Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for ChromaSuppression20 Creation
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IPEModuleCreateData* pCreateData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize parameters
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Execute
    ///
    /// @brief  Execute process capture request to configure module
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Execute(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetRegCmd
    ///
    /// @brief  Retrieve the buffer of the register value
    ///
    /// @return Pointer of the register buffer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID* GetRegCmd();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IPEChromaSuppression20
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPEChromaSuppression20();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEChromaSuppression20
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPEChromaSuppression20(
        const CHAR* pNodeIdentifier);

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AllocateCommonLibraryData
    ///
    /// @brief  Allocate memory required for common library
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult AllocateCommonLibraryData();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DeallocateCommonLibraryData
    ///
    /// @brief  Deallocate memory required for common library
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DeallocateCommonLibraryData();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckDependenceChange
    ///
    /// @brief  Check to see if the Dependence Trigger Data Changed
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return BOOL Indicates if the settings have changed compared to last setting
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckDependenceChange(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculation
    ///
    /// @brief  Calculate the Register Value
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of Chroma suppression module for debug
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig() const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateIPEInternalData
    ///
    /// @brief  Update Pipeline input data, such as metadata, IQSettings.
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult UpdateIPEInternalData(
        const ISPInputData* pInputData
        ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateCmdList
    ///
    /// @brief  Create the Command List
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        const ISPInputData* pInputData);

    IPEChromaSuppression20(const IPEChromaSuppression20&)            = delete;  ///< Disallow the copy constructor
    IPEChromaSuppression20& operator=(const IPEChromaSuppression20&) = delete;  ///< Disallow assignment operator

    // Member variables
    const CHAR*                   m_pNodeIdentifier;    ///< String identifier for the Node that created this object
    IPEChromaSuppressionRegCmd    m_regCmd;             ///< Register List of this Module
    CS20InputData                 m_dependenceData;     ///< Dependence Data for this Module
    cs_2_0_0::chromatix_cs20Type* m_pChromatix;         ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END
#endif // CAMXIPECHROMASUPPRESSION20_H
