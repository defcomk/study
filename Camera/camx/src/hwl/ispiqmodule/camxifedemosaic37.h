////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifedemosaic37.h
/// @brief camxifedemosaic37 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFEDEMOSAIC37_H
#define CAMXIFEDEMOSAIC37_H

#include "camxformats.h"
#include "camxispiqmodule.h"
#include "demosaic_3_7_0.h"
#include "titan170_ife.h"

// Common Library Shared Head File
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED
/// @brief Register Set of the Demosaic36 Module
struct IFEDemosaic37RegCmd1
{
    IFE_IFE_0_VFE_DEMO_CFG                     moduleConfig;             ///< DEMO_CFG
}CAMX_PACKED;

struct IFEDemosaic37RegCmd2
{
    IFE_IFE_0_VFE_DEMO_INTERP_COEFF_CFG        interpolationCoeffConfig; ///< INTERP_COEFF_CFG
    IFE_IFE_0_VFE_DEMO_INTERP_CLASSIFIER_0     interpolationClassifier0; ///< INTERP_CLASSIFIER_0
} CAMX_PACKED;

struct IFEDemosaic37RegCmd
{
    struct IFEDemosaic37RegCmd1                demosaic37RegCmd1; ///< Demo Module config
    struct IFEDemosaic37RegCmd2                demosaic37RegCmd2; ///< Demo inter classifier
} CAMX_PACKED;
CAMX_END_PACKED

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE Demosaic37 Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFEDemosaic37 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create IFEDemosaic37 Object
    ///
    /// @param  pCreateData Pointer to data for Demosaic37 Module Creation
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
    /// @brief  Generate Settings for Demosaic37 Module
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
    /// IFEDemosaic37
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    IFEDemosaic37();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IFEDemosaic37
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFEDemosaic37();

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
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of Crop module for debug
    ///
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// InitRegValues
    ///
    /// @brief  Initialize registers with known set of values
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID InitRegValues();

    IFEDemosaic37(const IFEDemosaic37&)             = delete;    ///< Disallow the copy constructor
    IFEDemosaic37& operator=(const IFEDemosaic37&)  = delete;    ///< Disallow assignment operator

    Demosaic37InputData                       m_dependenceData;  ///< Dependence Data for this Module
    IFEDemosaic37RegCmd                       m_regCmd;          ///< Register List of this Module
    demosaic_3_7_0::chromatix_demosaic37Type* m_pChromatix;      ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END

#endif // CAMXIFEDEMOSAIC37_H
