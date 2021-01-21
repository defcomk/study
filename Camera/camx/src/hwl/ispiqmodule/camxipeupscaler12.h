////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipeupscaler12.h
/// @brief IPEUpscaler class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPEUPSCALER12_H
#define CAMXIPEUPSCALER12_H

#include "camxispiqmodule.h"
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

struct IPEUpscaleLiteRegCmd
{
    IPE_IPE_0_PPS_CLC_UPSCALE_LUMA_SCALE_CFG_0          lumaScaleCfg0;                     ///< luma config0
    IPE_IPE_0_PPS_CLC_UPSCALE_LUMA_SCALE_OUTPUT_CFG     lumaScaleOutputCfg;                ///< luma output Config0
    IPE_IPE_0_PPS_CLC_UPSCALE_LUMA_SCALE_PHASEH_INIT    lumaScaleHorizontalInitialPhase;   ///< luma hortizontal Initial Phase
    IPE_IPE_0_PPS_CLC_UPSCALE_LUMA_SCALE_PHASEH_STEP    lumaScaleHorizontalStepPhase;      ///< luma horizontal step phase
    IPE_IPE_0_PPS_CLC_UPSCALE_LUMA_SCALE_PHASEV_INIT    lumaScaleVerticalInitialPhase;     ///< luma verticial initial phase
    IPE_IPE_0_PPS_CLC_UPSCALE_LUMA_SCALE_PHASEV_STEP    lumaScaleVerticalStepPhase;        ///< luma verticial step phase
    IPE_IPE_0_PPS_CLC_UPSCALE_LUMA_SCALE_CFG_1          lumaScaleCfg1;                     ///< luma config1
    IPE_IPE_0_PPS_CLC_UPSCALE_CHROMA_SCALE_CFG_0        chromaScaleCfg0;                   ///< chroma config0
    IPE_IPE_0_PPS_CLC_UPSCALE_CHROMA_SCALE_OUTPUT_CFG   chromaScaleOutputCfg;              ///< chroma output Config0
    IPE_IPE_0_PPS_CLC_UPSCALE_CHROMA_SCALE_PHASEH_INIT  chromaScaleHorizontalInitialPhase; ///< chroma hortizontal Initial Phase
    IPE_IPE_0_PPS_CLC_UPSCALE_CHROMA_SCALE_PHASEH_STEP  chromaScaleHorizontalStepPhase;    ///< chroma horizontal step phase
    IPE_IPE_0_PPS_CLC_UPSCALE_CHROMA_SCALE_PHASEV_INIT  chromaScaleVerticalInitialPhase;   ///< chroma verticial initial phase
    IPE_IPE_0_PPS_CLC_UPSCALE_CHROMA_SCALE_PHASEV_STEP  chromaScaleVerticalStepPhase;      ///< chroma verticial step phase
    IPE_IPE_0_PPS_CLC_UPSCALE_CHROMA_SCALE_CFG_1        chromaScaleCfg1;                   ///< chroma config1
} CAMX_PACKED;

CAMX_END_PACKED

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IPE Upscaler20 Module
///
/// This IQ block adds upscaler operations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPEUpscaler12 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create Upscaler Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for Upscaler20 Creation
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IPEModuleCreateData* pCreateData);

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
    /// ~IPEUpscaler12
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPEUpscaler12();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEUpscaler12
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPEUpscaler12(
        const CHAR* pNodeIdentifier);

private:
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
        const ISPInputData* pInputData);

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
    /// @brief  Print register configuration of Upscale module for debug
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
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateIPEInternalData(
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

    IPEUpscaler12(const IPEUpscaler12&)            = delete;         ///< Disallow the copy constructor
    IPEUpscaler12& operator=(const IPEUpscaler12&) = delete;         ///< Disallow assignment operator

    const CHAR*             m_pNodeIdentifier;      ///< String identifier for the Node that created this object
    IPEUpscaleLiteRegCmd    m_regCmdUpscaleLite;    ///< Register List of Upscale12 Module
    Upscale12InputData      m_dependenceData;       ///< Dependence Data for this Module
};

CAMX_NAMESPACE_END

#endif // CAMXIPEUPSCALER12_H
