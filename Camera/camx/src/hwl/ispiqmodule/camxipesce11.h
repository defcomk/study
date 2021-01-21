////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipesce11.h
/// @brief IPE SCE class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPESCE11_H
#define CAMXIPESCE11_H

#include "camxispiqmodule.h"

// Common Library Shared Head File
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief: This structure contains skin enhancement vertex 1 set of registers
struct IPESCEVertex1
{
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX1_COORD_CFG_0 cfg0;  ///< Cb, Cr Coordinate of vertex1 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX1_COORD_CFG_1 cfg1;  ///< Cb, Cr Coordinate of vertex1 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX1_COORD_CFG_2 cfg2;  ///< Cb, Cr Coordinate of vertex1 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX1_COORD_CFG_3 cfg3;  ///< Cb, Cr Coordinate of vertex1 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX1_COORD_CFG_4 cfg4;  ///< Cb, Cr Coordinate of vertex1 triange
} CAMX_PACKED;

/// @brief: This structure contains skin enhancement vertex 2 set of registers
struct IPESCEVertex2
{
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX2_COORD_CFG_0 cfg0;  ///< Cb, Cr Coordinate of vertex2 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX2_COORD_CFG_1 cfg1;  ///< Cb, Cr Coordinate of vertex2 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX2_COORD_CFG_2 cfg2;  ///< Cb, Cr Coordinate of vertex2 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX2_COORD_CFG_3 cfg3;  ///< Cb, Cr Coordinate of vertex2 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX2_COORD_CFG_4 cfg4;  ///< Cb, Cr Coordinate of vertex2 triange
} CAMX_PACKED;

/// @brief: This structure contains skin enhancement vertex 3 set of registers
struct IPESCEVertex3
{
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX3_COORD_CFG_0 cfg0;  ///< Cb, Cr Coordinate of vertex3 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX3_COORD_CFG_1 cfg1;  ///< Cb, Cr Coordinate of vertex3 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX3_COORD_CFG_2 cfg2;  ///< Cb, Cr Coordinate of vertex3 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX3_COORD_CFG_3 cfg3;  ///< Cb, Cr Coordinate of vertex3 triange
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_VERTEX3_COORD_CFG_4 cfg4;  ///< Cb, Cr Coordinate of vertex3 triange
} CAMX_PACKED;

/// @brief: This structure contains skin enhancement Cr coefficient set of registers
struct IPESCECrCoefficient
{
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_COEFF_CFG_0 cfg0;   ///< Cr component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_COEFF_CFG_1 cfg1;   ///< Cr component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_COEFF_CFG_2 cfg2;   ///< Cr component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_COEFF_CFG_3 cfg3;   ///< Cr component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_COEFF_CFG_4 cfg4;   ///< Cr component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_COEFF_CFG_5 cfg5;   ///< Cr component of the transform matrix
} CAMX_PACKED;

/// @brief: This structure contains skin enhancement Cb coefficient set of registers
struct IPESCECbCoefficient
{
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_COEFF_CFG_0 cfg0;   ///< Cb component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_COEFF_CFG_1 cfg1;   ///< Cb component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_COEFF_CFG_2 cfg2;   ///< Cb component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_COEFF_CFG_3 cfg3;   ///< Cb component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_COEFF_CFG_4 cfg4;   ///< Cb component of the transform matrix
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_COEFF_CFG_5 cfg5;   ///< Cb component of the transform matrix
} CAMX_PACKED;

/// @brief: This structure contains skin enhancement Cr offset set of registers
struct IPESCECrOffset
{
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_OFFSET_CFG_0 cfg0;  ///< Offset of the transform matrix for Cr coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_OFFSET_CFG_1 cfg1;  ///< Offset of the transform matrix for Cr coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_OFFSET_CFG_2 cfg2;  ///< Offset of the transform matrix for Cr coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_OFFSET_CFG_3 cfg3;  ///< Offset of the transform matrix for Cr coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_OFFSET_CFG_4 cfg4;  ///< Offset of the transform matrix for Cr coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CR_OFFSET_CFG_5 cfg5;  ///< Offset of the transform matrix for Cr coordinate
} CAMX_PACKED;

/// @brief: This structure contains skin enhancement Cb offset set of registers
struct IPESCECbOffset
{
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_OFFSET_CFG_0 cfg0;  ///< Offset of the transform matrix for Cb coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_OFFSET_CFG_1 cfg1;  ///< Offset of the transform matrix for Cb coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_OFFSET_CFG_2 cfg2;  ///< Offset of the transform matrix for Cb coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_OFFSET_CFG_3 cfg3;  ///< Offset of the transform matrix for Cb coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_OFFSET_CFG_4 cfg4;  ///< Offset of the transform matrix for Cb coordinate
    IPE_IPE_0_PPS_CLC_SKIN_ENHAN_CB_OFFSET_CFG_5 cfg5;  ///< Offset of the transform matrix for Cb coordinate
} CAMX_PACKED;

/// @brief: This structure contains skin color enhancement registers programmed by software
/// There are 6 transform matrices for each of the triangles, starting from triangle 0. Triangle 5 is not
/// really a triangle but the plane outside of all triangles. CR_COEFF_n, n=[0,5]
struct IPESCERegCmd
{
    IPESCEVertex1       vertex1;        ///< Cb, Cr Coordinate of vertex1 triange
    IPESCEVertex2       vertex2;        ///< Cb, Cr Coordinate of vertex2 triange
    IPESCEVertex3       vertex3;        ///< Cb, Cr Coordinate of vertex3 triange
    IPESCECrCoefficient crCoefficient;  ///< Cr component of the transform matrix
    IPESCECbCoefficient cbCoefficient;  ///< Cb component of the transform matrix
    IPESCECrOffset      crOffset;       ///< Offset of the transform matrix for Cr coordinate
    IPESCECbOffset      cbOffset;       ///< Offset of the transform matrix for Cb coordinate
} CAMX_PACKED;

CAMX_END_PACKED

static const UINT32 IPESCERegLength = sizeof(IPESCERegCmd) / RegisterWidthInBytes;
CAMX_STATIC_ASSERT((39 * 4)  == sizeof(IPESCERegCmd));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Class for IPE Skin Color Enhancement Module
///
/// This IQ block does adjustment of skin tone colors in the image.
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPESCE11 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create SCE Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for SCE11 Creation
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
    /// @return None
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
    /// ~IPESCE11
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPESCE11();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPESCE11
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPESCE11(
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
    /// @brief  Validate the input crop info from client
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if the input is valid or invalid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ValidateDependenceParams(
        const ISPInputData* pInputData);

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
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of Skin Enhancement module for debug
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig() const;

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

    IPESCE11(const IPESCE11&)            = delete;     ///< Disallow the copy constructor
    IPESCE11& operator=(const IPESCE11&) = delete;     ///< Disallow assignment operator

    const CHAR*                     m_pNodeIdentifier; ///< String identifier for the Node that created this object
    IPESCERegCmd                    m_regCmd;          ///< Register List of this Module
    SCE11InputData                  m_dependenceData;  ///< Dependence Data for this Module
    sce_1_1_0::chromatix_sce11Type* m_pChromatix;      ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END

#endif // CAMXIPESCE11_H
