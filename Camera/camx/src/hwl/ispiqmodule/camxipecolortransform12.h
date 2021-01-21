////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipecolortransform12.h
/// @brief IPEColorTransform class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPECOLORTRANSFORM12_H
#define CAMXIPECOLORTRANSFORM12_H

#include "camxispiqmodule.h"

CAMX_NAMESPACE_BEGIN

struct IPEColorTransformDependence
{
    FLOAT       digitalGain;    ///< Digital Gain Setting From Sensor
    ImageFormat sensorFormat;   ///< Sensor Format
};

CAMX_BEGIN_PACKED

/// @brief: This structure contains color transform channel 0 registers
/// Out_ch0 = (M10*(X0-S0) + M11*(X1-S1) + M12*(X2-S2)) rshift 9 + 1uQ1 + O1;
struct IPEColorTransformConfigChannel0
{
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH0_COEFF_CFG_0   coefficient0;   ///< transform matrix coefficents (M10, M11)
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH0_COEFF_CFG_1   coefficient1;   ///< transform matrix coefficents (M12)
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH0_OFFSET_CFG    offset;         ///< offset param (O1, S1)
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH0_CLAMP_CFG     clamp;          ///< clamp range (MAX, MIN)
} CAMX_PACKED;

/// @brief: This structure contains color transform channel 1 registers
/// Out_ch1 = (M10*(X0-S0) + M11*(X1-S1) + M12*(X2-S2)) rshift 9 + 1uQ1 + O1;
struct IPEColorTransformConfigChannel1
{
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH1_COEFF_CFG_0   coefficient0;   ///< transform matrix coefficents (M10, M11)
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH1_COEFF_CFG_1   coefficient1;   ///< transform matrix coefficents (M12)
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH1_OFFSET_CFG    offset;         ///< offset param (O1, S1)
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH1_CLAMP_CFG     clamp;          ///< clamp range (MAX, MIN)
} CAMX_PACKED;

/// @brief: This structure contains color transform channel 2 registers
/// Out_ch2 = (M10*(X0-S0) + M11*(X1-S1) + M12*(X2-S2)) rshift 9 + 1uQ1 + O1;
struct IPEColorTransformConfigChannel2
{
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH2_COEFF_CFG_0   coefficient0;   ///< transform matrix coefficents (M10, M11)
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH2_COEFF_CFG_1   coefficient1;   ///< transform matrix coefficents (M12)
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH2_OFFSET_CFG    offset;         ///< offset param (O1, S1)
    IPE_IPE_0_PPS_CLC_COLOR_XFORM_COLOR_XFORM_CH2_CLAMP_CFG     clamp;          ///< clamp range (MAX, MIN)
} CAMX_PACKED;

/// @brief: This structure contains Color transform registers programmed by software
struct IPEColorTransformRegCmd
{
    IPEColorTransformConfigChannel0 configChannel0; ///< Output Channel0 configuration parameters
    IPEColorTransformConfigChannel1 configChannel1; ///< Output Channel1 configuration parameters
    IPEColorTransformConfigChannel2 configChannel2; ///< Output Channel2 configuration parameters
} CAMX_PACKED;

CAMX_END_PACKED

static const UINT32 IPEColorTransformRegLength = sizeof(IPEColorTransformRegCmd) / RegisterWidthInBytes;
CAMX_STATIC_ASSERT((12 * 4) == sizeof(IPEColorTransformRegCmd));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Class for IPE Color Space Transform Module
///
///         Color Space Transform, some times abbrevated as xform or CST, has a standard 3x3 matrix multiplication to convert
///         RGB to YUV / vice versa, and Color conversion for converting RGB to YUV only with chroma enhancement capability.
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPEColorTransform12 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create ColorTransform Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for ColorTransform12 Creation
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
    /// GetRegCmd
    ///
    /// @brief  Retrieve the buffer of the register value
    ///
    /// @return Pointer of the register buffer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID* GetRegCmd();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IPEColorTransform12
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPEColorTransform12();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEColorTransform12
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPEColorTransform12(
        const CHAR* pNodeIdentifier);

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateDependenceParams
    ///
    /// @brief  Validate the input crop info from client
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if the input is valid or invalid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ValidateDependenceParams(
        const ISPInputData* pInputData
        ) const;

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
    /// @brief  Print register configuration of Color transform module for debug
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

    IPEColorTransform12(const IPEColorTransform12&)            = delete;  ///< Disallow the copy constructor
    IPEColorTransform12& operator=(const IPEColorTransform12&) = delete;  ///< Disallow assignment operator

    const CHAR*                     m_pNodeIdentifier;  ///< String identifier for the Node that created this object
    CST12InputData                  m_dependenceData;   ///< Input Data to CST Interpolation
    IPEColorTransformRegCmd         m_regCmd;           ///< Register List of this Module
    cst_1_2_0::chromatix_cst12Type* m_pChromatix;       ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END

#endif // CAMXIPECOLORTRANSFORM12_H
