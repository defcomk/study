////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 - 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeprecrop10.h
/// @brief camxifePreCrop10 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFEPRECROP10_H
#define CAMXIFEPRECROP10_H

#include "camxispiqmodule.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED
struct IFEPreCrop10DS4LumaReg
{
    IFE_IFE_0_VFE_DS4_Y_PRE_CROP_LINE_CFG     lineConfig;     ///< DS4precrop output path Luma Crop line config
    IFE_IFE_0_VFE_DS4_Y_PRE_CROP_PIXEL_CFG    pixelConfig;    ///< DS4precrop output path Luma Crop pixel config
} CAMX_PACKED;

struct IFEPreCrop10DS4ChromaReg
{
    IFE_IFE_0_VFE_DS4_C_PRE_CROP_LINE_CFG     lineConfig;     ///< DS4precrop output path chroma Crop line config
    IFE_IFE_0_VFE_DS4_C_PRE_CROP_PIXEL_CFG    pixelConfig;    ///< DS4precrop output path chroma Crop pixel config
} CAMX_PACKED;

struct IFEPreCrop10DisplayDS4LumaReg
{
    IFE_IFE_0_VFE_DISP_DS4_Y_PRE_CROP_LINE_CFG     lineConfig;     ///< DS4precrop Display output path Luma Crop line config
    IFE_IFE_0_VFE_DISP_DS4_Y_PRE_CROP_PIXEL_CFG    pixelConfig;    ///< DS4precrop Display output path Luma Crop pixel config
} CAMX_PACKED;

struct IFEPreCrop10DisplayDS4ChromaReg
{
    IFE_IFE_0_VFE_DISP_DS4_C_PRE_CROP_LINE_CFG     lineConfig;     ///< DS4precrop Display output path chroma Crop line config
    IFE_IFE_0_VFE_DISP_DS4_C_PRE_CROP_PIXEL_CFG    pixelConfig;    ///< DS4precrop Display output path chroma Crop pixel config
} CAMX_PACKED;

CAMX_END_PACKED

struct IFEPreCrop10RegCmd
{
    IFEPreCrop10DS4LumaReg           DS4Luma;           ///< DS4 path precrop Luma config
    IFEPreCrop10DS4ChromaReg         DS4Chroma;         ///< DS4 path precrop chroma config
    IFEPreCrop10DS4LumaReg           DS16Luma;          ///< DS16 path precrop Luma config
    IFEPreCrop10DS4ChromaReg         DS16Chroma;        ///< DS16 path precrop chroma config
    IFEPreCrop10DisplayDS4LumaReg    displayDS4Luma;    ///< DS4 Display path precrop Luma config
    IFEPreCrop10DisplayDS4ChromaReg  displayDS4Chroma;  ///< DS4 Display path precrop chroma config
    IFEPreCrop10DisplayDS4LumaReg    displayDS16Luma;   ///< DS16 Display path precrop Luma config
    IFEPreCrop10DisplayDS4ChromaReg  displayDS16Chroma; ///< DS16 Display path precrop chroma config
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE CROP10 Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFEPreCrop10 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create Crop10 Object
    ///
    /// @param  pCreateData  Pointer to data for Crop Creation
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
    /// ~IFEPreCrop10
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFEPreCrop10();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEPreCrop10
    ///
    /// @brief  Constructor
    ///
    /// @param  pCreateData Pointer to resource and intialization data for Crop Creation
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IFEPreCrop10(
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
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateCmdList
    ///
    /// @brief  Create the Command List
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Success if the write operation is success
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
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of Crop module for debug
    ///
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig();

    IFEPreCrop10(const IFEPreCrop10&)            = delete; ///< Disallow the copy constructor
    IFEPreCrop10& operator=(const IFEPreCrop10&) = delete; ///< Disallow assignment operator

    IFEPreCrop10RegCmd m_regCmd;        ///< Register List of this Module
    IFEPipelinePath    m_modulePath;    ///< IFE pipeline path for module
    ISPInputData*      m_pInputData;    ///< Pointer to current input data
};
CAMX_NAMESPACE_END

#endif // CAMXIFECROP10_H
