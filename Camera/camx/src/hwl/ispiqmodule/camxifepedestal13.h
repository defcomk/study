////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifepedestal13.h
/// @brief camxifepedestal13 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFEPEDESTAL13_H
#define CAMXIFEPEDESTAL13_H

#include "camxformats.h"
#include "camxispiqmodule.h"
#include "iqcommondefs.h"
#include "pedestal_1_3_0.h"
#include "titan170_ife.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief IFE Pedestal Module Dependence Data
struct IFEPedestal13RegCmd
{
    IFE_IFE_0_VFE_PEDESTAL_CFG                config;             ///< Configuration Register. Offset: 0x4b0
    IFE_IFE_0_VFE_PEDESTAL_GRID_CFG_0         config0;            ///< Grid Config0 Register. Offset: 0x4b4
    IFE_IFE_0_VFE_PEDESTAL_GRID_CFG_1         config1;            ///< Grid Config1 Register. Offset: 0x4b8
    IFE_IFE_0_VFE_PEDESTAL_GRID_CFG_2         config2;            ///< Grid Config2 Register. Offset: 0x4bc
    IFE_IFE_0_VFE_PEDESTAL_RIGHT_GRID_CFG_0   rightGridConfig0;   ///< Right Grid Config0 Register. Offset: 0x4c0
    IFE_IFE_0_VFE_PEDESTAL_RIGHT_GRID_CFG_1   rightGridConfig1;   ///< Right Grid Config1 Register. Offset: 0x4c4
    IFE_IFE_0_VFE_PEDESTAL_RIGHT_GRID_CFG_2   rightGridConfig2;   ///< Right Grid Config2 Register. Offset: 0x4c8
    IFE_IFE_0_VFE_PEDESTAL_STRIPE_CFG_0       stripeConfig0;      ///< Stripe Config0 Register. Offset: 0x4cc
    IFE_IFE_0_VFE_PEDESTAL_STRIPE_CFG_1       stripeConfig1;      ///< Stripe Config1 Register. Offset: 0x4d0
    IFE_IFE_0_VFE_PEDESTAL_RIGHT_STRIPE_CFG_0 rightStripeConfig0; ///< Right Stripe Config0 Register. Offset: 0x4d4
    IFE_IFE_0_VFE_PEDESTAL_RIGHT_STRIPE_CFG_1 rightStripeConfig1; ///< Right Stripe Config1 Register. Offset: 0x4d8
} CAMX_PACKED;

CAMX_END_PACKED


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE Pedestal Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFEPedestal13 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create IFEPedestal13 Object
    ///
    /// @param  pCreateData Pointer to data for Demux Creation
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
    /// @brief  Generate Settings for Pedestal Object
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

        pDualIFEData->dualIFESensitive = TRUE;
    }

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IFEPedestal13
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFEPedestal13();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEPedestal13
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    IFEPedestal13();

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

    IFEPedestal13(const IFEPedestal13&)            = delete;       ///< Disallow the copy constructor
    IFEPedestal13& operator=(const IFEPedestal13&) = delete;       ///< Disallow assignment operator

    Pedestal13InputData                       m_dependenceData;    ///< Dependence Data for this Module
    IFEPedestal13RegCmd                       m_regCmd;            ///< Register List of this Module
    UINT8                                     m_leftGRRBankSelect; ///< Left Plane, GR/R Bank Selection
    UINT8                                     m_leftGBBBankSelect; ///< Left Plane, GB/B Bank Selection
    UINT8                                     m_blacklevelLock;    ///< manual update Black Level lock
    pedestal_1_3_0::chromatix_pedestal13Type* m_pChromatix;        ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END

#endif // CAMXIFEPEDESTAL13_H
