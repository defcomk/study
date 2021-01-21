////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifelinearization33.h
/// @brief camxifelinearization33 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFELINEARIZATION33_H
#define CAMXIFELINEARIZATION33_H

#include "camxformats.h"
#include "camxispiqmodule.h"
#include "camxsensorproperty.h"
#include "iqcommondefs.h"
#include "linearization_3_3_0.h"
#include "titan170_base.h"
#include "titan170_ife.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief IFE Linearization Module Register Set
struct IFELinearization33RegCmd
{
    IFE_IFE_0_VFE_BLACK_CFG               configRegister;                     ///< BLS Configuration Register. 0x4dc
    IFE_IFE_0_VFE_BLACK_INTERP_R_0        interpolationR0Register;            ///< R Channel Register 0. 0x4e0
    IFE_IFE_0_VFE_BLACK_INTERP_R_1        interpolationR1Register;            ///< R Channel Register 1. 0x4e4
    IFE_IFE_0_VFE_BLACK_INTERP_R_2        interpolationR2Register;            ///< R Channel Register 2. 0x4e8
    IFE_IFE_0_VFE_BLACK_INTERP_R_3        interpolationR3Register;            ///< R Channel Register 3. 0x4ec
    IFE_IFE_0_VFE_BLACK_INTERP_GB_0       interpolationGB0Register;           ///< GB Channel Register 0. 0x4f0
    IFE_IFE_0_VFE_BLACK_INTERP_GB_1       interpolationGB1Register;           ///< GB Channel Register 1. 0x4f4
    IFE_IFE_0_VFE_BLACK_INTERP_GB_2       interpolationGB2Register;           ///< GB Channel Register 2. 0x4f8
    IFE_IFE_0_VFE_BLACK_INTERP_GB_3       interpolationGB3Register;           ///< GB Channel Register 3. 0x4fc
    IFE_IFE_0_VFE_BLACK_INTERP_B_0        interpolationB0Register;            ///< B Channel Register 0. 0x500
    IFE_IFE_0_VFE_BLACK_INTERP_B_1        interpolationB1Register;            ///< B Channel Register 1. 0x504
    IFE_IFE_0_VFE_BLACK_INTERP_B_2        interpolationB2Register;            ///< B Channel Register 2. 0x508
    IFE_IFE_0_VFE_BLACK_INTERP_B_3        interpolationB3Register;            ///< B Channel Register 3. 0x50c
    IFE_IFE_0_VFE_BLACK_INTERP_GR_0       interpolationGR0Register;           ///< GR Channel Register 0. 0x510
    IFE_IFE_0_VFE_BLACK_INTERP_GR_1       interpolationGR1Register;           ///< GR Channel Register 1. 0x514
    IFE_IFE_0_VFE_BLACK_INTERP_GR_2       interpolationGR2Register;           ///< GR Channel Register 2. 0x518
    IFE_IFE_0_VFE_BLACK_INTERP_GR_3       interpolationGR3Register;           ///< GR Channel Register 3. 0x51c
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_R_0  rightPlaneInterpolationR0Register;  ///< Right Plane R Channel Register 0. 0x520
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_R_1  rightPlaneInterpolationR1Register;  ///< Right Plane R Channel Register 1. 0x524
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_R_2  rightPlaneInterpolationR2Register;  ///< Right Plane R Channel Register 2. 0x528
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_R_3  rightPlaneInterpolationR3Register;  ///< Right Plane R Channel Register 3. 0x52c
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_GB_0 rightPlaneInterpolationGB0Register; ///< Right Plane GB Channel Register 0. 0x530
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_GB_1 rightPlaneInterpolationGB1Register; ///< Right Plane GB Channel Register 1. 0x534
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_GB_2 rightPlaneInterpolationGB2Register; ///< Right Plane GB Channel Register 2. 0x538
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_GB_3 rightPlaneInterpolationGB3Register; ///< Right Plane GB Channel Register 3. 0x53c
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_B_0  rightPlaneInterpolationB0Register;  ///< Right Plane B Channel Register 0. 0x540
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_B_1  rightPlaneInterpolationB1Register;  ///< Right Plane B Channel Register 1. 0x544
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_B_2  rightPlaneInterpolationB2Register;  ///< Right Plane B Channel Register 2. 0x548
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_B_3  rightPlaneInterpolationB3Register;  ///< Right Plane B Channel Register 3. 0x54c
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_GR_0 rightPlaneInterpolationGR0Register; ///< Right Plane GR Channel Register 0. 0x550
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_GR_1 rightPlaneInterpolationGR1Register; ///< Right Plane GR Channel Register 1. 0x554
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_GR_2 rightPlaneInterpolationGR2Register; ///< Right Plane GR Channel Register 2. 0x548
    IFE_IFE_0_VFE_BLACK_RIGHT_INTERP_GR_3 rightPlaneInterpolationGR3Register; ///< Right Plane GR Channel Register 3. 0x54c
} CAMX_PACKED;

CAMX_END_PACKED

static const UINT BlackLUTBank0 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_BLACK_LUT_RAM_BANK0;
static const UINT BlackLUTBank1 = IFE_IFE_0_VFE_DMI_CFG_DMI_RAM_SEL_BLACK_LUT_RAM_BANK1;

static const UINT32 IFELinearization33RegLengthDWord = sizeof(IFELinearization33RegCmd) / sizeof(UINT32);

static const UINT32 IFELinearizationLutTableSize     = 36;
static const UINT32 IFELinearization33DMILengthDWord = IFELinearizationLutTableSize * sizeof(UINT64) / sizeof(UINT32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE LINEARIZATION Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFELinearization33 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create IFELinearization33 Object
    ///
    /// @param  pCreateData Pointer to data for Linearization Module Creation
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
    /// @brief  Generate Settings for IFELinearization33 Object
    ///
    /// @param  pInputData Pointer to the Inputdata
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

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IFELinearization33
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFELinearization33();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFELinearization33
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    IFELinearization33();

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
    /// @param  pInputData Point to the Input Data
    ///
    /// @return TRUE if dependence is meet
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckDependenceChange(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculation
    ///
    /// @brief  Perform the Interpolation and Calculation
    ///
    /// @param  pInputData Pointer to the Linearization Module Input data
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
    /// @param  pInputData Point to the Input Data
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        ISPInputData* pInputData);

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

    IFELinearization33(const IFELinearization33&)            = delete;          ///< Disallow the copy constructor
    IFELinearization33& operator=(const IFELinearization33&) = delete;          ///< Disallow assignment operator

    Linearization33InputData                            m_dependenceData;       ///< Dependence/Input Data for this Module
    IFELinearization33RegCmd                            m_regCmd;               ///< Register List of this Module
    UINT32                                              m_dynamicBlackLevel[4]; ///< Previous Dynamic Black Level
    UINT8                                               m_blacklevelLock;       ///< manual update Black Level lock
    UINT8                                               m_bankSelect;           ///< Black LUT Bank Selection
    FLOAT                                               m_stretchGainRed;       ///< Stretch Gain Red
    FLOAT                                               m_stretchGainGreenEven; ///< Stretch Gain Green Even
    FLOAT                                               m_stretchGainGreenOdd;  ///< Stretch Gain Green Odd
    FLOAT                                               m_stretchGainBlue;      ///< Stretch Gain Blue
    linearization_3_3_0::chromatix_linearization33Type* m_pChromatix;           ///< Pointer to tuning mode data
    BOOL                                                m_AWBLock;                  ///< Flag to track AWB state
};

CAMX_NAMESPACE_END

#endif // CAMXIFELINEARIZATION33_H
