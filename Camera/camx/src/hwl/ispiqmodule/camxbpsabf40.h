////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsabf40.h
/// @brief bpsabf40 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXBPSABF40_H
#define CAMXBPSABF40_H

#include "abf_4_0_0.h"
#include "camxformats.h"
#include "camxispiqmodule.h"
#include "camxsensorproperty.h"
#include "iqcommondefs.h"
#include "titan170_bps.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief ABF register Configuration
struct BPSABF40RegConfig
{
    BPS_BPS_0_CLC_ABF_ABF_0_CFG   config0;      ///< ABF Config 0
    BPS_BPS_0_CLC_ABF_ABF_1_CFG   config1;      ///< ABF Config 1
    BPS_BPS_0_CLC_ABF_ABF_2_CFG   config2;      ///< ABF Config 2
    BPS_BPS_0_CLC_ABF_ABF_3_CFG   config3;      ///< ABF Config 3
    BPS_BPS_0_CLC_ABF_ABF_4_CFG   config4;      ///< ABF Config 4
    BPS_BPS_0_CLC_ABF_ABF_5_CFG   config5;      ///< ABF Config 5
    BPS_BPS_0_CLC_ABF_ABF_6_CFG   config6;      ///< ABF Config 6
    BPS_BPS_0_CLC_ABF_ABF_7_CFG   config7;      ///< ABF Config 7
    BPS_BPS_0_CLC_ABF_ABF_8_CFG   config8;      ///< ABF Config 8
    BPS_BPS_0_CLC_ABF_ABF_9_CFG   config9;      ///< ABF Config 9
    BPS_BPS_0_CLC_ABF_ABF_10_CFG  config10;     ///< ABF Config 10
    BPS_BPS_0_CLC_ABF_ABF_11_CFG  config11;     ///< ABF Config 11
    BPS_BPS_0_CLC_ABF_ABF_12_CFG  config12;     ///< ABF Config 12
    BPS_BPS_0_CLC_ABF_ABF_13_CFG  config13;     ///< ABF Config 13
    BPS_BPS_0_CLC_ABF_ABF_14_CFG  config14;     ///< ABF Config 14
    BPS_BPS_0_CLC_ABF_ABF_15_CFG  config15;     ///< ABF Config 15
    BPS_BPS_0_CLC_ABF_ABF_16_CFG  config16;     ///< ABF Config 16
    BPS_BPS_0_CLC_ABF_ABF_17_CFG  config17;     ///< ABF Config 17
    BPS_BPS_0_CLC_ABF_ABF_18_CFG  config18;     ///< ABF Config 18
    BPS_BPS_0_CLC_ABF_ABF_19_CFG  config19;     ///< ABF Config 19
    BPS_BPS_0_CLC_ABF_ABF_20_CFG  config20;     ///< ABF Config 20
    BPS_BPS_0_CLC_ABF_ABF_21_CFG  config21;     ///< ABF Config 21
    BPS_BPS_0_CLC_ABF_ABF_22_CFG  config22;     ///< ABF Config 22
    BPS_BPS_0_CLC_ABF_ABF_23_CFG  config23;     ///< ABF Config 23
    BPS_BPS_0_CLC_ABF_ABF_24_CFG  config24;     ///< ABF Config 24
    BPS_BPS_0_CLC_ABF_ABF_25_CFG  config25;     ///< ABF Config 25
    BPS_BPS_0_CLC_ABF_ABF_26_CFG  config26;     ///< ABF Config 26
    BPS_BPS_0_CLC_ABF_ABF_27_CFG  config27;     ///< ABF Config 27
    BPS_BPS_0_CLC_ABF_ABF_28_CFG  config28;     ///< ABF Config 28
    BPS_BPS_0_CLC_ABF_ABF_29_CFG  config29;     ///< ABF Config 29
    BPS_BPS_0_CLC_ABF_ABF_30_CFG  config30;     ///< ABF Config 30
    BPS_BPS_0_CLC_ABF_ABF_31_CFG  config31;     ///< ABF Config 31
    BPS_BPS_0_CLC_ABF_ABF_32_CFG  config32;     ///< ABF Config 32
    BPS_BPS_0_CLC_ABF_ABF_33_CFG  config33;     ///< ABF Config 33
    BPS_BPS_0_CLC_ABF_ABF_34_CFG  config34;     ///< ABF Config 34
    BPS_BPS_0_CLC_ABF_ABF_35_CFG  config35;     ///< ABF Config 35
    BPS_BPS_0_CLC_ABF_ABF_36_CFG  config36;     ///< ABF Config 36
    BPS_BPS_0_CLC_ABF_ABF_37_CFG  config37;     ///< ABF Config 37
    BPS_BPS_0_CLC_ABF_ABF_38_CFG  config38;     ///< ABF Config 38
    BPS_BPS_0_CLC_ABF_ABF_39_CFG  config39;     ///< ABF Config 39
    BPS_BPS_0_CLC_ABF_ABF_40_CFG  config40;     ///< ABF Config 40
    BPS_BPS_0_CLC_ABF_ABF_41_CFG  config41;     ///< ABF Config 41
    BPS_BPS_0_CLC_ABF_ABF_42_CFG  config42;     ///< ABF Config 42
    BPS_BPS_0_CLC_ABF_ABF_43_CFG  config43;     ///< ABF Config 43
    BPS_BPS_0_CLC_ABF_ABF_44_CFG  config44;     ///< ABF Config 44
    BPS_BPS_0_CLC_ABF_ABF_45_CFG  config45;     ///< ABF Config 45
} CAMX_PACKED;

struct BPSABF40ModuleConfig
{
    BPS_BPS_0_CLC_ABF_MODULE_CFG  moduleConfig;   ///< ABF Module config
} CAMX_PACKED;

CAMX_END_PACKED

static const UINT32 NoiseLUT0                    = BPS_BPS_0_CLC_ABF_DMI_LUT_CFG_LUT_SEL_NOISE_LUT0;
static const UINT32 NoiseLUT1                    = BPS_BPS_0_CLC_ABF_DMI_LUT_CFG_LUT_SEL_NOISE_LUT1;
static const UINT32 ActivityLUT                  = BPS_BPS_0_CLC_ABF_DMI_LUT_CFG_LUT_SEL_ACTIVITY_LUT;
static const UINT32 DarkLUT                      = BPS_BPS_0_CLC_ABF_DMI_LUT_CFG_LUT_SEL_DARK_LUT;
static const UINT32 ABFLUTBank0                  = BPS_BPS_0_CLC_ABF_DMI_LUT_BANK_CFG_BANK_SEL_LUT_BANK0;
static const UINT32 ABFLUTBank1                  = BPS_BPS_0_CLC_ABF_DMI_LUT_BANK_CFG_BANK_SEL_LUT_BANK1;
static const UINT32 MaxABFLUT                    = 4;
static const UINT32 BPSABF40NoiseLUTSizeDword    = 64;  // 64 entries in the DMI table
static const UINT32 BPSABF40NoiseLUTSize         = (BPSABF40NoiseLUTSizeDword * sizeof(UINT32));
static const UINT32 BPSABF40ActivityLUTSizeDword = 32;  // 32 entries in the DMI table
static const UINT32 BPSABF40ActivityLUTSize      = (BPSABF40ActivityLUTSizeDword * sizeof(UINT32));
static const UINT32 BPSABF40DarkLUTSizeDword     = 42;  // 48 entries in the DMI table
static const UINT32 BPSABF40DarkLUTSize          = (BPSABF40DarkLUTSizeDword * sizeof(UINT32));
static const UINT32 BPSABFTotalDMILengthDword    = ((BPSABF40NoiseLUTSizeDword * 2) + BPSABF40ActivityLUTSizeDword +
                                                    BPSABF40DarkLUTSizeDword);
static const UINT32 BPSABFTotalLUTBufferSize     = (BPSABFTotalDMILengthDword * sizeof(UINT32));

/// @brief Output Data to ABF40
struct ABF40OutputData
{
    BPSABF40RegConfig*    pRegCmd;         ///< Point to the ABF40 Register buffer
    BPSABF40ModuleConfig* pModuleConfig;   ///< Pointer to the GIC Module configuration
    UINT32*               pNoiseLUT;       ///< Point to the ABF40 Noise DMI data
    UINT32*               pNoiseLUT1;      ///< Point to the ABF40 Noise DMI data LUT1
    UINT32*               pActivityLUT;    ///< Point to the ABF40 Activity DMI data
    UINT32*               pDarkLUT;        ///< Point to the ABF40 Dark DMI data
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief BPS ABF 40 Class Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class BPSABF40 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create BPSABF40 Object
    ///
    /// @param  pCreateData Pointer to the BPSABF40 Creation
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        BPSModuleCreateData* pCreateData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize parameters
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillCmdBufferManagerParams
    ///
    /// @brief  Fills the command buffer manager params needed by IQ Module
    ///
    /// @param  pInputData Pointer to the IQ Module Input data structure
    /// @param  pParam     Pointer to the structure containing the command buffer manager param to be filled by IQ Module
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult FillCmdBufferManagerParams(
       const ISPInputData*     pInputData,
       IQModuleCmdBufferParam* pParam);

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
    /// ~BPSABF40
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~BPSABF40();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSABF40
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier   String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit BPSABF40(
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
    /// CheckDependenceChange
    ///
    /// @brief  Check to see if the Dependence Trigger Data Changed
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return BOOL
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
    /// @return  CamxResult Indicates if configure DMI and Registers was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FetchDMIBuffer
    ///
    /// @brief  Fetch ABF DMI LUT
    ///
    /// @return CamxResult Indicates if fetch DMI was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FetchDMIBuffer();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteLUTtoDMI
    ///
    /// @brief  Write Look up table data pointer into DMI header
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if write to DMI header was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteLUTtoDMI(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateBPSInternalData
    ///
    /// @brief  Update BPS internal data
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateBPSInternalData(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateCmdList
    ///
    /// @brief  Create the Command List
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of ABF module for debug
    ///
    /// @param  pOutputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig(
        const ABF40OutputData* pOutputData
        ) const;

    BPSABF40(const BPSABF40&)            = delete;           ///< Disallow the copy constructor
    BPSABF40& operator=(const BPSABF40&) = delete;           ///< Disallow assignment operator

    const CHAR*                     m_pNodeIdentifier;       ///< String identifier of this Node
    ABF40InputData                  m_dependenceData;        ///< Dependence Data for this Module
    BPSABF40RegConfig               m_regCmd;                ///< Register list for ABF module
    CmdBufferManager*               m_pLUTCmdBufferManager;  ///< Command buffer manager for all LUTs of ABF
    CmdBuffer*                      m_pLUTDMICmdBuffer;      ///< Command buffer for holding all LUTs
    BPSABF40ModuleConfig            m_moduleConfig;          ///< Module configuration information
    UINT8                           m_noiseReductionMode;    ///< noise reduction mode
    BOOL                            m_bilateralEnable;       ///< enable bilateral filtering
    BOOL                            m_minmaxEnable;          ///< enable built-in min-max pixel filter
    BOOL                            m_dirsmthEnable;         ///< enable Directional Smoothing filter

    UINT32*                         m_pABFNoiseLUT;          ///< Pointer for ABF noise LUT used for tuning data
    UINT32*                         m_pABFNoiseLUT1;         ///< Pointer for ABF noise LUT1 used for tuning data
    UINT32*                         m_pABFActivityLUT;       ///< Pointer for ABF activity LUT used for tuning data
    UINT32*                         m_pABFDarkLUT;           ///< Pointer for ABF dark LUT used for tuning data
    abf_4_0_0::chromatix_abf40Type* m_pChromatix;            ///< Pointer to tuning mode data
    bls_1_2_0::chromatix_bls12Type* m_pChromatixBLS;         ///
};

CAMX_NAMESPACE_END
#endif // CAMXBPSABF40_H
