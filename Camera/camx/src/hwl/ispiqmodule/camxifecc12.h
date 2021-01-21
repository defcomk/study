////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifecc12.h
/// @brief camxifecc12 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFECC12_H
#define CAMXIFECC12_H

#include "camxformats.h"
#include "camxispiqmodule.h"
#include "camxsensorproperty.h"
#include "cc_1_2_0.h"
#include "iqcommondefs.h"
#include "titan170_ife.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief IFE Color Correction Module Register Set
struct IFECC12RegCmd
{
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_0  coefficientRegister0;    ///< Coefficient Register 0
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_1  coefficientRegister1;    ///< Coefficient Register 1
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_2  coefficientRegister2;    ///< Coefficient Register 2
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_3  coefficientRegister3;    ///< Coefficient Register 3
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_4  coefficientRegister4;    ///< Coefficient Register 4
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_5  coefficientRegister5;    ///< Coefficient Register 5
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_6  coefficientRegister6;    ///< Coefficient Register 6
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_7  coefficientRegister7;    ///< Coefficient Register 7
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_8  coefficientRegister8;    ///< Coefficient Register 8
    IFE_IFE_0_VFE_COLOR_CORRECT_OFFSET_0 offsetRegister0;         ///< Offset Register 0
    IFE_IFE_0_VFE_COLOR_CORRECT_OFFSET_1 offsetRegister1;         ///< Offset Register 1
    IFE_IFE_0_VFE_COLOR_CORRECT_OFFSET_2 offsetRegister2;         ///< Offset Register 2
    IFE_IFE_0_VFE_COLOR_CORRECT_COEFF_Q  coefficientQRegister;    ///< Coefficient Q Register
} CAMX_PACKED;

CAMX_END_PACKED

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE CC12 Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFECC12 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create IFECC12 Object
    ///
    /// @param  pCreateData Pointer to data for CC Module Creation
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IFEModuleCreateData* pCreateData);

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
    /// @brief  Generate Settings for CC Object
    ///
    /// @param  pInputData Pointer to the Inputdata
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Execute(
        ISPInputData* pInputData);

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IFECC12
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFECC12();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFECC12
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    IFECC12();

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
    /// @brief  Check if the Dependence Data has changed
    ///
    /// @param  pInputData Pointer to the Input Data
    ///
    /// @return TRUE if dependencies met
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckDependenceChange(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculation
    ///
    /// @brief  Perform the Interpolation and Calculation
    ///
    /// @param  pInputData Pointer to the Input Data
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateCmdList
    ///
    /// @brief  Generate the Command List
    ///
    /// @param  pInputData Pointer to the Input Data
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ModuleInitialize
    ///
    /// @brief  Initialize the Module Data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ModuleInitialize();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetupInitialConfig
    ///
    /// @brief  Configure some of the register value based on the chromatix and image format
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SetupInitialConfig();

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
    /// UpdateAWBCCM
    ///
    /// @brief  Update CCM from AWB algo
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateAWBCCM();

    IFECC12(const IFECC12&)            = delete;     ///< Disallow the copy constructor
    IFECC12& operator=(const IFECC12&) = delete;     ///< Disallow assignment operator

    CC12InputData                 m_dependenceData;  ///< Dependence Data for this Module
    IFECC12RegCmd                 m_regCmd;          ///< Register List of this Module
    AWBCCMParams                  m_AWBCCM;          ///< CCM overide config from 3A
    cc_1_2_0::chromatix_cc12Type* m_pChromatix;      ///< Pointer to tuning mode data
};
CAMX_NAMESPACE_END
#endif // CAMXIFECC12_H
