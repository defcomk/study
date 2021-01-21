////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeds410.h
/// @brief camxIFEDS410 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFEDS410_H
#define CAMXIFEDS410_H

#include "camxispiqmodule.h"
#include "ds4to1_1_0_0.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

struct IFEDS4Reg
{
    IFE_IFE_0_VFE_DS_4TO1_Y_1ST_CFG      lumaConfig;       ///< DS4 Luma channel configuration
    IFE_IFE_0_VFE_DS_4TO1_Y_1ST_COEFF    coefficients;     ///< DS4 Filter coefficients
    IFE_IFE_0_VFE_DS_4TO1_C_1ST_CFG      chromaConfig;     ///< DS4 Chroma channel configuration
} CAMX_PACKED;

struct IFEDS16Reg
{
    IFE_IFE_0_VFE_DS_4TO1_Y_2ND_CFG      lumaConfig;       ///< DS16 Luma channel configuration
    IFE_IFE_0_VFE_DS_4TO1_Y_2ND_COEFF    coefficients;     ///< DS16 Filter coefficients
    IFE_IFE_0_VFE_DS_4TO1_C_2ND_CFG      chromaConfig;     ///< DS16 Chroma channel configuration
} CAMX_PACKED;

struct IFEDisplayDS4Reg
{
    IFE_IFE_0_VFE_DISP_DS_4TO1_Y_1ST_CFG      lumaConfig;       ///< Display DS4 Luma channel configuration
    IFE_IFE_0_VFE_DISP_DS_4TO1_Y_1ST_COEFF    coefficients;     ///< Display DS4 Filter coefficients
    IFE_IFE_0_VFE_DISP_DS_4TO1_C_1ST_CFG      chromaConfig;     ///< Display DS4 Chroma channel configuration
} CAMX_PACKED;

struct IFEDisplayDS16Reg
{
    IFE_IFE_0_VFE_DISP_DS_4TO1_Y_2ND_CFG      lumaConfig;       ///< Display DS16 Luma channel configuration
    IFE_IFE_0_VFE_DISP_DS_4TO1_Y_2ND_COEFF    coefficients;     ///< Display DS16 Filter coefficients
    IFE_IFE_0_VFE_DISP_DS_4TO1_C_2ND_CFG      chromaConfig;     ///< Display DS16 Chroma channel configuration
} CAMX_PACKED;

CAMX_END_PACKED

struct IFEDS410RegCmd
{
    IFEDS4Reg           DS4;            ///< DS4 register configuration
    IFEDS16Reg          DS16;           ///< DS16 register configuration
    IFEDisplayDS4Reg    displayDS4;     ///< Display DS4 register configuration
    IFEDisplayDS16Reg   displayDS16;    ///< Display DS16 register configuration
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE DS4 1.0 Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFEDS410 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create DS4 1.0 Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for DS4 1.0 Creation
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IFEModuleCreateData* pCreateData);

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
    /// PrepareStripingParameters
    ///
    /// @brief  Prepare striping parameters for striping lib
    ///
    /// @param  pInputData Pointer to the Inputdata
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult PrepareStripingParameters(
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
    /// GetDualIFEData
    ///
    /// @brief  Provides information on how dual IFE mode affects the IQ module
    ///
    /// @param  pDualIFEData Pointer to dual IFE data the module will fill in
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID GetDualIFEData(
        IQModuleDualIFEData* pDualIFEData)
    {
        CAMX_ASSERT(NULL != pDualIFEData);

        pDualIFEData->dualIFESensitive = TRUE;
    }

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IFEDS410
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFEDS410();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEDS410
    ///
    /// @brief  Constructor
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for DS4 1.0 Creation
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IFEDS410(
        IFEModuleCreateData* pCreateData);

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
        ISPInputData* pInputData);

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
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDS4Registers
    ///
    /// @brief  Configure DS4 module registers
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDS4Registers();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDS16Registers
    ///
    /// @brief  Configure DS16 module registers
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDS16Registers();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDisplayDS4Registers
    ///
    /// @brief  Configure Display DS4 module registers
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDisplayDS4Registers();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDisplayDS16Registers
    ///
    /// @brief  Configure Display DS16 module registers
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDisplayDS16Registers();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateIFEInternalData
    ///
    /// @brief  Update IFE internal data
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateIFEInternalData(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of DS4 1.0 module for debug
    ///
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig();

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
        ISPInputData* pInputData);

    IFEDS410(const IFEDS410&)            = delete;    ///< Disallow the copy constructor
    IFEDS410& operator=(const IFEDS410&) = delete;    ///< Disallow assignment operator

    IFEDS410RegCmd     m_regCmd;                      ///< Register List of this Module
    DSState*           m_pState;                      ///< State
    IFEPipelinePath    m_modulePath;                  ///< IFE pipeline path for module

    ds4to1_1_0_0::chromatix_ds4to1v10Type* m_pDc4Chromatix;  ///< Chromatix pointer for DC4
    ds4to1_1_0_0::chromatix_ds4to1v10Type* m_pDc16Chromatix; ///< Chromatix pointer for DC16
};

CAMX_NAMESPACE_END

#endif // CAMXIFEDS410_H
