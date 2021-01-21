////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipe2dlut10.h
/// @brief IPE2DLUT class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPE2DLUT10_H
#define CAMXIPE2DLUT10_H

#include "camxispiqmodule.h"
#include "ipe_data.h"
#include "tdl_1_0_0.h"
#include "iqcommondefs.h"
#include "titan170_ipe.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief: Registers programmed by software for 2D LUT module
struct IPER2DLUTRegCmd
{
    IPE_IPE_0_PPS_CLC_2D_LUT_H_SHIFT_CFG                hueShift;       ///< Hue Delta Range
    IPE_IPE_0_PPS_CLC_2D_LUT_S_SHIFT_CFG                satShift;       ///< Saturation Delta Range
    IPE_IPE_0_PPS_CLC_2D_LUT_L_BOUNDARY_START_A_CFG     startA;         ///< Boundary control based on L start threshold1
    IPE_IPE_0_PPS_CLC_2D_LUT_L_BOUNDARY_START_B_CFG     startB;         ///< Boundary control based on L start threshold2
    IPE_IPE_0_PPS_CLC_2D_LUT_L_BOUNDARY_END_A_CFG       endA;           ///< Boundary control based on L end threshold1
    IPE_IPE_0_PPS_CLC_2D_LUT_L_BOUNDARY_END_B_CFG       endB;           ///< Boundary control based on L end threshold2
    IPE_IPE_0_PPS_CLC_2D_LUT_L_BOUNDARY_START_INV_CFG   startInv;       ///< Start points inverse for L end to calculate ratio
    IPE_IPE_0_PPS_CLC_2D_LUT_L_BOUNDARY_END_INV_CFG     endInv;         ///< End points inverse for L end to calculate ratio
    IPE_IPE_0_PPS_CLC_2D_LUT_Y_BLEND_FACTOR_INTEGER_CFG blendFactor;    ///< Y offset weight for 0~1.0
    IPE_IPE_0_PPS_CLC_2D_LUT_K_B_INTEGER_CFG            kbIntegeter;    ///< Luma Delta Coefficent for Blue
    IPE_IPE_0_PPS_CLC_2D_LUT_K_R_INTEGER_CFG            krIntegeter;    ///< Luma Delta Coefficent for Red
} CAMX_PACKED;

CAMX_END_PACKED

static const UINT32 IPE2DLUTRegLength = sizeof(IPER2DLUTRegCmd) / RegisterWidthInBytes;
CAMX_STATIC_ASSERT((11 * 4) == sizeof(IPER2DLUTRegCmd));


/// @brief: This enumerator maps Look Up Tables indices with DMI LUT_SEL in 2DLUT module SWI.
enum IPELUT2DIndex
{
    LUT2DIndexHue0              = 0,   ///< Hue LUT when h_idx is even and s_idx is even
    LUT2DIndexHue1              = 1,   ///< Hue LUT when h_idx is even and s_idx is odd
    LUT2DIndexHue2              = 2,   ///< Hue LUT when h_idx is odd and s_idx is even
    LUT2DIndexHue3              = 3,   ///< Hue LUT when h_idx is odd and s_idx is odd
    LUT2DIndexSaturation0       = 4,   ///< Saturation LUT when h_idx is even and s_idx is even
    LUT2DIndexSaturation1       = 5,   ///< Saturation LUT when h_idx is even and s_idx is odd
    LUT2DIndexSaturation2       = 6,   ///< Saturation LUT when h_idx is odd and s_idx is even
    LUT2DIndexSaturation3       = 7,   ///< Saturation LUT when h_idx is odd and s_idx is odd
    LUT2DIndexInverseHue        = 8,   ///< Inverse hue
    LUT2DIndexInverseSaturation = 9,   ///< Inverse saturation
    LUT2DIndex1DHue             = 10,  ///< 1D hue
    LUT2DIndex1DSaturation      = 11,  ///< 1D saturation
    LUT2DIndexMax               = 12,  ///< Max LUTs
};

/// @brief: This structure has information of number of entried of each LUT.
static const UINT IPE2DLUTLUTNumEntries[LUT2DIndexMax] =
{
    112,    ///< Hue0
    112,    ///< Hue1
    112,    ///< Hue2
    112,    ///< Hue3
    112,    ///< Saturation0
    112,    ///< Saturation1
    112,    ///< Saturation2
    112,    ///< Saturation3
    24,     ///< Inverse Hue
    15,     ///< Inverse saturation
    25,     ///< 1D Hue
    16,     ///< 1D Saturation
};

/// @brief: This structure has information of number of entries of each LUT each LUT is a UINT32.
static const UINT IPE2DLUTLUTSize[LUT2DIndexMax] =
{
    IPE2DLUTLUTNumEntries[LUT2DIndexHue0],                  ///< Hue0
    IPE2DLUTLUTNumEntries[LUT2DIndexHue1],                  ///< Hue1
    IPE2DLUTLUTNumEntries[LUT2DIndexHue2],                  ///< Hue2
    IPE2DLUTLUTNumEntries[LUT2DIndexHue3],                  ///< Hue3
    IPE2DLUTLUTNumEntries[LUT2DIndexSaturation0],           ///< Saturation0
    IPE2DLUTLUTNumEntries[LUT2DIndexSaturation1],           ///< Saturation1
    IPE2DLUTLUTNumEntries[LUT2DIndexSaturation2],           ///< Saturation2
    IPE2DLUTLUTNumEntries[LUT2DIndexSaturation3],           ///< Saturation3
    IPE2DLUTLUTNumEntries[LUT2DIndexInverseHue],            ///< Inverse Hue
    IPE2DLUTLUTNumEntries[LUT2DIndexInverseSaturation],     ///< Inverse saturation
    IPE2DLUTLUTNumEntries[LUT2DIndex1DHue],                 ///< 1D Hue
    IPE2DLUTLUTNumEntries[LUT2DIndex1DSaturation],          ///< 1D Saturation
};

static const UINT IPEMax2DLUTLUTNumEntries = 976; // Total Entries 976 (Sum of all entries from all LUT curves)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Class for IPE 2DLUT10 Module
///
///         This IQ block does mapping of wide dynamic range captured image to a more visible data range. Supports two separate
///         functions: Data Collection (statistic collection) during 1:4/1:16 passes and tri-linear image interpolation during
///         1:1 pass.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPE2DLUT10 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create Local Tone Map (2DLUT) Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for 2DLUT10 Creation
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
    /// FillCmdBufferManagerParams
    ///
    /// @brief  Fills the command buffer manager params needed by IQ Module
    ///
    /// @param  pInputData Pointer to the IQ Module Input data structure
    /// @param  pParam     Pointer to the structure containing the command buffer manager param to be filled by IQ Module
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult FillCmdBufferManagerParams(
       const ISPInputData*     pInputData,
       IQModuleCmdBufferParam* pParam);

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
    /// ~IPE2DLUT10
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPE2DLUT10();
protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPE2DLUT10
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPE2DLUT10(
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
    /// ValidateDependenceParams
    ///
    /// @brief  Validate the 2DLUT dependencies viz 3A
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
        ISPInputData* pInputData);

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
    /// RunCalculation
    ///
    /// @brief  Calculate the Register Value
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FetchDMIBuffer
    ///
    /// @brief  Fetch the DMI buffer
    ///
    /// @return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FetchDMIBuffer();

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

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteLUTtoDMI
    ///
    /// @brief  writes all 14 LUTs from 2DLUT into cmd buffer
    ///
    /// @param  pInputData Pointer to IPE module input data
    ///
    /// @return CamxResult Indicates if LUTs are successfully written into DMI cdm header
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteLUTtoDMI(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of 2DLUT module for debug
    ///
    /// @param  p2DLUTLUTs Pointer to the LUT
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig(
        UINT32* p2DLUTLUTs
        ) const;

    IPE2DLUT10(const IPE2DLUT10&)            = delete;       ///< Disallow the copy constructor
    IPE2DLUT10& operator=(const IPE2DLUT10&) = delete;       ///< Disallow assignment operator

    const CHAR*                     m_pNodeIdentifier;       ///< String identifier for the Node that created this object
    IPER2DLUTRegCmd                 m_regCmd;                ///< Register List of this Module
    TDL10InputData                  m_dependenceData;        ///< Dependence Data for this Module
    CmdBufferManager*               m_pLUTCmdBufferManager;  ///< Command buffer manager for all LUTs of GRA
    CmdBuffer*                      m_pLUTDMICmdBuffer;      ///< Command buffer for holding all 3 LUTs
    UINT32*                         m_p2DLUTLUTs;            ///< Tuning hold pointer to 2D LUT LUTs data
    tdl_1_0_0::chromatix_tdl10Type* m_pChromatix;            ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END

#endif // CAMXIPE2DLUT10_H
