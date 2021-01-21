////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipetf10.h
/// @brief camxipetf10 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPETF10_H
#define CAMXIPETF10_H

#include "camxformats.h"
#include "camxispiqmodule.h"
#include "ipe_data.h"
#include "iqcommondefs.h"
#include "tf10regcmd.h"

CAMX_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IPE TF10 Module
///
/// 2D filter for spatial noise reduction. Filtering done hierarchically in multi-pass approach using multiple scales of same
/// images and auxiliary information from previous scales as indications.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPETF10 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /// Create
     ///
     /// @brief  Create IPETF10 Object
     ///
     /// @param  pCreateData Pointer to data for TF Creation
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
    /// @param  pInputData Pointer to the Inputdata
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Execute
    ///
    /// @brief  Execute process capture request to configure module
    ///
    /// @param  pInputData Pointer to the IPE input data
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
    /// GetModuleData
    ///
    /// @brief  Get IQ module specific data
    ///
    /// @param  pModuleData    Pointer pointing to Module specific data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CAMX_INLINE VOID GetModuleData(
        VOID* pModuleData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /// ~IPETF10
     ///
     /// @brief  Destructor
     ///
     /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPETF10();

protected:
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
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult DeallocateCommonLibraryData();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPETF10
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    /// @param  pCreateData         Pointer to Create data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPETF10(
        const CHAR*          pNodeIdentifier,
        IPEModuleCreateData* pCreateData);

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
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetTFEnable
    ///
    /// @brief  Set module enable for TF
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE CamxResult SetTFEnable(
        const ISPInputData* pInputData)
    {
        CamxResult      result         = CamxResultSuccess;
        IpeIQSettings*  pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

        // So it needs to be set by "master enable" AND "region enable"
        for (UINT pass = 0; pass < m_dependenceData.maxUsedPasses; pass++)
        {
            if (TRUE == pIPEIQSettings->anrParameters.parameters[pass].moduleCfg.EN)
            {
                m_TFParams.parameters[pass].moduleCfg.EN &= m_moduleEnable;
            }
            else
            {
                m_TFParams.parameters[pass].moduleCfg.EN = 0;
            }
            pIPEIQSettings->tfParameters.parameters[pass].moduleCfg.EN =
                m_TFParams.parameters[pass].moduleCfg.EN;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, " #### Module enable on pass full - DC64 : %d, %d, %d, %d",
            pIPEIQSettings->tfParameters.parameters[PASS_NAME_FULL].moduleCfg.EN,
            pIPEIQSettings->tfParameters.parameters[PASS_NAME_DC_4].moduleCfg.EN,
            pIPEIQSettings->tfParameters.parameters[PASS_NAME_DC_16].moduleCfg.EN,
            pIPEIQSettings->tfParameters.parameters[PASS_NAME_DC_64].moduleCfg.EN);

        return result;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetRefinementEnable
    ///
    /// @brief  Set module enable for Refinement
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE CamxResult SetRefinementEnable(
        const ISPInputData* pInputData)
    {
        CamxResult      result         = CamxResultSuccess;
        IpeIQSettings*  pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);
        // So it needs to be set by "master enable" AND "region enable"
        if (FALSE == pIPEIQSettings->tfParameters.parameters[1].moduleCfg.EN)
        {
            pIPEIQSettings->refinementParameters.dc[0].refinementCfg.TRENABLE = 0;
        }

        if (FALSE == pIPEIQSettings->tfParameters.parameters[2].moduleCfg.EN)
        {
            pIPEIQSettings->refinementParameters.dc[1].refinementCfg.TRENABLE = 0;
        }


        if (FALSE == pIPEIQSettings->tfParameters.parameters[3].moduleCfg.EN)
        {
            pIPEIQSettings->refinementParameters.dc[2].refinementCfg.TRENABLE = 0;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, " #### Refinement enable on pass DC4 - DC64 : %d, %d, %d",
            pIPEIQSettings->refinementParameters.dc[0].refinementCfg.TRENABLE,
            pIPEIQSettings->refinementParameters.dc[1].refinementCfg.TRENABLE,
            pIPEIQSettings->refinementParameters.dc[2].refinementCfg.TRENABLE);

        return result;
    }

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
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculationFullPass
    ///
    /// @brief  Calculate the Register Value
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculationFullPass(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculationDS4Pass
    ///
    /// @brief  Calculate the Register Value
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculationDS4Pass(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculationDS16Pass
    ///
    /// @brief  Calculate the Register Value
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculationDS16Pass(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateAndSetParams
    ///
    /// @brief  Validate and set params
    ///
    /// @param  pParam         input parameter.
    /// @param  pass           parameter for particular pass.
    /// @param  expectedValue  expected value of input param.
    /// @param  pParamString   String refering to the parameter name.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID ValidateAndSetParams(
        UINT32*     pParam,
        UINT32      pass,
        UINT32      expectedValue,
        const CHAR* pParamString)
    {
        if (expectedValue != *pParam)
        {
            CAMX_LOG_WARN(CamxLogGroupPProc, "%s: pass %d incorrect, resetting", pParamString, pass);
            *pParam = expectedValue;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateAndCorrectTFParams
    ///
    /// @brief  Validate and correct MCTF pass params
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ValidateAndCorrectTFParams(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateAndCorrectMCTFParameters
    ///
    /// @brief  Validate and correct MCTF pass parameters
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ValidateAndCorrectMCTFParameters(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateAndCorrectStillModeParameters
    ///
    /// @brief  Validate and correct Still Mode parameters
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ValidateAndCorrectStillModeParameters(
        const ISPInputData* pInputData);

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
    /// FillFirmwareConfig0
    ///
    /// @brief  Fill Firmware with HW Config0 register data
    ///
    /// @param  pIPEIQSettings   Pointer to the Firmware IQ Setting Data
    /// @param  passType         pass type: FULL, DC4, DC16, DC64
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID FillFirmwareConfig0(
        IpeIQSettings*  pIPEIQSettings,
        UINT32          passType);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillFirmwareConfig1
    ///
    /// @brief  Fill Firmware with HW Config1 register data
    ///
    /// @param  pIPEIQSettings   Pointer to the Firmware IQ Setting Data
    /// @param  passType         pass type: FULL, DC4, DC16, DC64
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID FillFirmwareConfig1(
        IpeIQSettings*  pIPEIQSettings,
        UINT32          passType);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillFirmwareFStoA1A4Map
    ///
    /// @brief  Fill Firmware with HW FS to A1/A4 register data
    ///
    /// @param  pIPEIQSettings   Pointer to the Firmware IQ Setting Data
    /// @param  passType         pass type: FULL, DC4, DC16, DC64
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID FillFirmwareFStoA1A4Map(
        IpeIQSettings*  pIPEIQSettings,
        UINT32          passType);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckIPEInstanceProperty
    ///
    /// @brief  Check IPE Instance Property Data
    ///
    /// @param  pInput  Pointer to the ISP input data
    ///
    /// @return True if data changes
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckIPEInstanceProperty(
        ISPInputData* pInput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of Temporal Filtering module for debug
    ///
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig() const;

    IPETF10(const IPETF10&) = delete;                            ///< Disallow the copy constructor
    IPETF10& operator=(const IPETF10&) = delete;                 ///< Disallow assignment operator

    const CHAR*                   m_pNodeIdentifier;             ///< String identifier for the Node that created this object
    TF10InputData                 m_dependenceData;              ///< Dependence Data for this Module
    IPETFRegCmd                   m_regCmd[PASS_NAME_MAX];       ///< Register List of this Module
    UINT32                        m_IPETFCmdBufferSize;          ///< Size of single pass cmd buffer
    UINT                          m_offsetPass[PASS_NAME_MAX];   ///< Offset where pass information starts for multipass
    UINT                          m_singlePassCmdLength;         ///< The length of the Command List, in bytes
    BOOL                          m_bypassMode;                  ///< Bypass ANR and TF
    BOOL                          m_enableCommonIQ;              ///< EnableCommon IQ module
    BOOL                          m_validateTFParams;            ///< Validate and correct TF params
    tf_1_0_0::chromatix_tf10Type* m_pChromatix;                  ///< Pointer to tuning mode data
    RefinementParameters          m_refinementParams;             ///< Refinement Parameters
    TfParameters                  m_TFParams;                     ///< TF Parameters

};

CAMX_NAMESPACE_END

#endif // CAMXIPETF10_H
