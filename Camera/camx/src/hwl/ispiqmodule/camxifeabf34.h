////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeabf34.h
/// @brief camxifeabf34 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFEABF34_H
#define CAMXIFEABF34_H

#include "camxformats.h"
#include "camxispiqmodule.h"
#include "camxsensorproperty.h"
#include "abf_3_4_0.h"
#include "ifeabf34setting.h"
#include "titan170_ife.h"

CAMX_NAMESPACE_BEGIN


CAMX_BEGIN_PACKED

/// @brief IFE ABF34 Register Command Set 1
struct IFEABF34RegCmd1
{
    IFE_IFE_0_VFE_ABF_CFG       configReg;        ///< Config Register. Offset: 0x5e8
} CAMX_PACKED;

/// @brief IFE ABF34 Register Command Set 2
struct IFEABF34RegCmd2
{
    IFE_IFE_0_VFE_ABF_GR_CFG               configGRChannel;      ///< Config Register for GR Channel. Offset: 0x5f4
    IFE_IFE_0_VFE_ABF_GB_CFG               configGBChannel;      ///< Config Register for GB Channel. Offset: 0x5f8
    IFE_IFE_0_VFE_ABF_R_CFG                configRChannel;       ///< Config Register for R Channel. Offset: 0x5fc
    IFE_IFE_0_VFE_ABF_B_CFG                configBChannel;       ///< Config Register for B Channel. Offset: 0x600
    IFE_IFE_0_VFE_ABF_RNR_CFG_0            configRNR0;           ///< RNR Config Register 0 Channel. Offset: 0x604
    IFE_IFE_0_VFE_ABF_RNR_CFG_1            configRNR1;           ///< RNR Config Register 1 Channel. Offset: 0x608
    IFE_IFE_0_VFE_ABF_RNR_CFG_2            configRNR2;           ///< RNR Config Register 2 Channel. Offset: 0x60C
    IFE_IFE_0_VFE_ABF_RNR_CFG_3            configRNR3;           ///< RNR Config Register 3 Channel. Offset: 0x610
    IFE_IFE_0_VFE_ABF_RNR_CFG_4            configRNR4;           ///< RNR Config Register 4 Channel. Offset: 0x614
    IFE_IFE_0_VFE_ABF_RNR_CFG_5            configRNR5;           ///< RNR Config Register 5 Channel. Offset: 0x618
    IFE_IFE_0_VFE_ABF_RNR_CFG_6            configRNR6;           ///< RNR Config Register 6 Channel. Offset: 0x61C
    IFE_IFE_0_VFE_ABF_RNR_CFG_7            configRNR7;           ///< RNR Config Register 7 Channel. Offset: 0x620
    IFE_IFE_0_VFE_ABF_RNR_CFG_8            configRNR8;           ///< RNR Config Register 8 Channel. Offset: 0x624
    IFE_IFE_0_VFE_ABF_RNR_CFG_9            configRNR9;           ///< RNR Config Register 9 Channel. Offset: 0x628
    IFE_IFE_0_VFE_ABF_RNR_CFG_10           configRNR10;          ///< RNR Config Register 10 Channel. Offset: 0x62C
    IFE_IFE_0_VFE_ABF_RNR_CFG_11           configRNR11;          ///< RNR Config Register 11 Channel. Offset: 0x630
    IFE_IFE_0_VFE_ABF_RNR_CFG_12           configRNR12;          ///< RNR Config Register 12 Channel. Offset: 0x634
    IFE_IFE_0_VFE_ABF_BPC_CFG_0            configBPC0;           ///< BPC Config Register 0 Channel. Offset: 0x638
    IFE_IFE_0_VFE_ABF_BPC_CFG_1            configBPC1;           ///< BPC Config Register 1 Channel. Offset: 0x63C
    IFE_IFE_0_VFE_ABF_NOISE_PRESERVE_CFG_0 noisePreserveConfig0; ///< Noise Preserve Config Register 0. Offset: 0x640
    IFE_IFE_0_VFE_ABF_NOISE_PRESERVE_CFG_1 noisePreserveConfig1; ///< Noise Preserve Config Register 1. Offset: 0x644
    IFE_IFE_0_VFE_ABF_NOISE_PRESERVE_CFG_2 noisePreserveConfig2; ///< Noise Preserve Config Register 2. Offset: 0x648
} CAMX_PACKED;

CAMX_END_PACKED

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE ABF Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFEABF34 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create IFEABF34 Object
    ///
    /// @param  pCreateData Pointer to data for ABF Creation
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
    /// @brief  Generate Settings for ABF Object
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

        pDualIFEData->dualIFESensitive      = TRUE;
        pDualIFEData->dualIFEDMI32Sensitive = TRUE;
    }

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IFEABF34
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFEABF34();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEABF34
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    IFEABF34();

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
    /// UpdateIFEInternalData
    ///
    /// @brief  Update the IFE internal data
    ///
    /// @param  pInputData Pointer to the Input Data
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

    IFEABF34RegCmd1                 m_regCmd1;              ///< Register Set 1 of this Module
    IFEABF34RegCmd2                 m_regCmd2;              ///< Register Set 2 of this Module
    UINT8                           m_ABFBankSelect;        ///< ABF Lut Bank Selection
    ABFState*                       m_pState;               ///< Pointer to state
    UINT8                           m_noiseReductionMode;   ///< noise reduction mode
    abf_3_4_0::chromatix_abf34Type* m_pChromatix;           ///< Pointer to tuning mode data
    VOID*                           m_pInterpolationData;   ///< Interpolation Parameters
    BOOL                            m_moduleSBPCEnable;     ///< Single BPC enable

    IFEABF34(const IFEABF34&)            = delete;          ///< Disallow the copy constructor
    IFEABF34& operator=(const IFEABF34&) = delete;          ///< Disallow assignment operator

};

CAMX_NAMESPACE_END

#endif // CAMXIFEABF34_H
