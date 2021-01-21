////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifelsc34.h
/// @brief camxifelsc34 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXIFELSC34_H
#define CAMXIFELSC34_H

// Camx Includes
#include "camxispiqmodule.h"
#include "chitintlessinterface.h"
#include "lsc34setting.h"
#include "lsc_3_4_0.h"
#include "tintless_2_0_0.h"

// Common library includes
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

static const CHAR* pTintlessLibName  = "libcamxtintlessalgo";
static const CHAR* pTintlessFuncName = "CreateTintless";

CAMX_BEGIN_PACKED

/// @brief IFE LSC Module Register Set
struct IFELSC34RegCmd
{
    IFE_IFE_0_VFE_ROLLOFF_CFG                configRegister;             ///< Config Register. 0x6bc
    IFE_IFE_0_VFE_ROLLOFF_GRID_CFG_0         gridConfigRegister0;        ///< Grid Config Register 0. 0x6c0
    IFE_IFE_0_VFE_ROLLOFF_GRID_CFG_1         gridConfigRegister1;        ///< Grid Config Register 1. 0x6c4
    IFE_IFE_0_VFE_ROLLOFF_GRID_CFG_2         gridConfigRegister2;        ///< Grid Config Register 2. 0x6c8
    IFE_IFE_0_VFE_ROLLOFF_RIGHT_GRID_CFG_0   rightGridConfigRegister0;   ///< Right Grid Config Register 0. 0x6cc
    IFE_IFE_0_VFE_ROLLOFF_RIGHT_GRID_CFG_1   rightGridConfigRegister1;   ///< Right Grid Config Register 1. 0x6d0
    IFE_IFE_0_VFE_ROLLOFF_RIGHT_GRID_CFG_2   rightGridConfigRegister2;   ///< Right Grid Config Register 2. 0x6d4
    IFE_IFE_0_VFE_ROLLOFF_STRIPE_CFG_0       stripeConfigRegister0;      ///< Stripe Config Register 0. 0x6d8
    IFE_IFE_0_VFE_ROLLOFF_STRIPE_CFG_1       stripeConfigRegister1;      ///< Stripe Config Register 1. 0x6DC
    IFE_IFE_0_VFE_ROLLOFF_RIGHT_STRIPE_CFG_0 rightStripeConfigRegister0; ///< Right Stripe Config Register 0. 0x6e0
    IFE_IFE_0_VFE_ROLLOFF_RIGHT_STRIPE_CFG_1 rightStripeConfigRegister1; ///< Right Stripe Config Register 1. 0x6e4
} CAMX_PACKED;

CAMX_END_PACKED

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE LSC Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFELSC34 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create IFELSC34 Object
    ///
    /// @param  pCreateData Pointer to data for LSC Creation
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
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Execute
    ///
    /// @brief  Generate Settings for LSC Object
    ///
    /// @param  pInputData Pointer to the Inputdata
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

        pDualIFEData->dualIFESensitive      = TRUE;
        pDualIFEData->dualIFEDMI32Sensitive = TRUE;
    }

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IFELSC34
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFELSC34();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFELSC34
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    IFELSC34();

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
    /// TranslateCalibrationTableToCommonLibrary
    ///
    /// @brief  Translate calibration table to common library
    ///
    /// @param  pInputData Pointer to the Input Data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID TranslateCalibrationTableToCommonLibrary(
        const ISPInputData* pInputData);

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
    /// @brief   Calculate the Register Value
    ///
    /// @param   pInputData Pointer to the ISP input data
    ///
    /// @return  CamxResultSuccess if successful
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

    IFELSC34(const IFELSC34&)            = delete;          ///< Disallow the copy constructor
    IFELSC34& operator=(const IFELSC34&) = delete;          ///< Disallow assignment operator

    LSCState*                       m_pState;               ///< Pointer to state
    IFELSC34RegCmd                  m_regCmd;               ///< Register List of this Module
    LSCCalibrationData*             m_pLSCCalibrationData;  ///< LSC Calibration table
    LSC34CalibrationDataTable*      m_pCalibrationTable;    ///< LSC calibration Data table
    UINT32*                         m_pGRRLUTDMIBuffer;     ///< Pointer to GRR LSC mesh table
    UINT32*                         m_pGBBLUTDMIBuffer;     ///< Pointer to GBB SC mesh table
    OSLIBRARYHANDLE                 m_hHandle;              ///< Tintless Algo Library handle
    CHITintlessAlgorithm*           m_pTintlessAlgo;        ///< Tintless Algorithm Instance
    TintlessConfig                  m_tintlessConfig;       ///< Tintless Config
    ParsedTintlessBGStatsOutput*    m_pTintlessBGStats;     ///< Pointer to TintlessBG stats
    LSC34UnpackedData               m_unpackedData;         ///< unpacked data
    UINT8                           m_shadingMode;          ///< Shading mode
    UINT8                           m_lensShadingMapMode;   ///< Lens shading map mode
    lsc_3_4_0::chromatix_lsc34Type* m_pChromatix;           ///< Pointer to tuning mode data
    BOOL                            m_AWBLock;              ///< Flag to track AWB state
    VOID*                           m_pInterpolationData;   ///< Interpolation Parameters
    LSC34TintlessRatio              m_tintlessRatio;        ///< Tintless ratio
};

CAMX_NAMESPACE_END

#endif // CAMXIFELSC34_H
