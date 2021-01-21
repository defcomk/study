////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpsgic30.h
/// @brief bpsgic30 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXBPSGIC30_H
#define CAMXBPSGIC30_H

#include "camxispiqmodule.h"
#include "gic_3_0_0.h"
#include "iqcommondefs.h"
#include "titan170_bps.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED
/// @brief GIC register Configuration
struct BPSGIC30RegConfig
{
    BPS_BPS_0_CLC_GIC_GIC_FILTER_CFG          filterConfig;             ///< GIC Filter config
    BPS_BPS_0_CLC_GIC_THIN_LINE_NOISE_OFFSET  lineNoiseOffset;          ///< GIC thin line noise offset
    BPS_BPS_0_CLC_GIC_ANCHOR_BASE_SETTINGS_0  anchorBaseSetting0;       ///< GIC anchor and base setting 0
    BPS_BPS_0_CLC_GIC_ANCHOR_BASE_SETTINGS_1  anchorBaseSetting1;       ///< GIC anchor and base setting 1
    BPS_BPS_0_CLC_GIC_ANCHOR_BASE_SETTINGS_2  anchorBaseSetting2;       ///< GIC anchor and base setting 2
    BPS_BPS_0_CLC_GIC_ANCHOR_BASE_SETTINGS_3  anchorBaseSetting3;       ///< GIC anchor and base setting 3
    BPS_BPS_0_CLC_GIC_ANCHOR_BASE_SETTINGS_4  anchorBaseSetting4;       ///< GIC anchor and base setting 4
    BPS_BPS_0_CLC_GIC_ANCHOR_BASE_SETTINGS_5  anchorBaseSetting5;       ///< GIC anchor and base setting 5
    BPS_BPS_0_CLC_GIC_SLOPE_SHIFT_SETTINGS_0  slopeShiftSetting0;       ///< GIC slope shift setting 0
    BPS_BPS_0_CLC_GIC_SLOPE_SHIFT_SETTINGS_1  slopeShiftSetting1;       ///< GIC slope shift setting 1
    BPS_BPS_0_CLC_GIC_SLOPE_SHIFT_SETTINGS_2  slopeShiftSetting2;       ///< GIC slope shift setting 2
    BPS_BPS_0_CLC_GIC_SLOPE_SHIFT_SETTINGS_3  slopeShiftSetting3;       ///< GIC slope shift setting 3
    BPS_BPS_0_CLC_GIC_SLOPE_SHIFT_SETTINGS_4  slopeShiftSetting4;       ///< GIC slope shift setting 4
    BPS_BPS_0_CLC_GIC_SLOPE_SHIFT_SETTINGS_5  slopeShiftSetting5;       ///< GIC slope shift setting 5
    BPS_BPS_0_CLC_GIC_INIT_HV_OFFSET          horizontalVerticalOffset; ///< GIC horizontal and vertical offset
    BPS_BPS_0_CLC_GIC_R_SQUARE_INIT           squareInit;               ///< GIC R square init
    BPS_BPS_0_CLC_GIC_R_SCALE_SHIFT           scaleShift;               ///< GIC scale shift
    BPS_BPS_0_CLC_GIC_PNR_NOISE_SCALE_0       noiseRatioScale0;         ///< GIC PNR noise scale 0
    BPS_BPS_0_CLC_GIC_PNR_NOISE_SCALE_1       noiseRatioScale1;         ///< GIC PNR noise scale 1
    BPS_BPS_0_CLC_GIC_PNR_NOISE_SCALE_2       noiseRatioScale2;         ///< GIC PNR noise scale 2
    BPS_BPS_0_CLC_GIC_PNR_NOISE_SCALE_3       noiseRatioScale3;         ///< GIC PNR noise scale 3
    BPS_BPS_0_CLC_GIC_PNR_FILTER_STRENGTH     noiseRatioFilterStrength; ///< GIC noise filter strength
} CAMX_PACKED;

struct BPSGIC30ModuleConfig
{
    BPS_BPS_0_CLC_GIC_MODULE_CFG moduleConfig;  ///< GIC Module config
} CAMX_PACKED;

CAMX_END_PACKED

static const UINT   GICNoiseLUT = BPS_BPS_0_CLC_GIC_DMI_LUT_CFG_LUT_SEL_NOISE_LUT;
static const UINT   GICLUTBank0 = BPS_BPS_0_CLC_GIC_DMI_LUT_BANK_CFG_BANK_SEL_LUT_BANK0;
static const UINT   GICLUTBank1 = BPS_BPS_0_CLC_GIC_DMI_LUT_BANK_CFG_BANK_SEL_LUT_BANK1;

static const UINT32 BPSGIC30RegLengthDword   = sizeof(BPSGIC30RegConfig) / sizeof(UINT32);
static const UINT32 MaxGICNoiseLUT           = 1;
static const UINT32 BPSGIC30DMILengthDword   = 64;
static const UINT32 BPSGICNoiseLUTBufferSize = BPSGIC30DMILengthDword * sizeof(UINT32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief BPS GIC 34 Class Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class BPSGIC30 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create BPSGIC30 Object
    ///
    /// @param  pCreateData Pointer to the BPSGIC30 Creation
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
    /// ~BPSGIC30
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~BPSGIC30();

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
    /// BPSGIC30
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit BPSGIC30(
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
    /// ValidateDependenceParams
    ///
    /// @brief  Validate the input configuration from app
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
    /// @return TRUE if dependencies met
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckDependenceChange(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculation
    ///
    /// @brief   Calculate the Register Value
    ///
    /// @param   pInputData Pointer to the GIC module input data
    ///
    /// @return  CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureRegisters
    ///
    /// @brief  Configure GIC registers
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureRegisters();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FetchDMIBuffer
    ///
    /// @brief  Configure GIC DMI LUT
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
        const ISPInputData* pInputData
        ) const;

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
    /// @brief  Print register configuration of GIC module for debug
    ///
    /// @param  pInputData    Pointer to the ISP input data
    /// @param  pModuleConfig Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig(
        const ISPInputData* pInputData,
        const BPSGIC30ModuleConfig* pModuleConfig
        ) const;

    BPSGIC30(const BPSGIC30&)            = delete;           ///< Disallow the copy constructor
    BPSGIC30& operator=(const BPSGIC30&) = delete;           ///< Disallow assignment operator

    const CHAR*                     m_pNodeIdentifier;       ///< String identifier of this Node
    GIC30InputData                  m_dependenceData;        ///< Dependence/Input Data for this Module
    BPSGIC30RegConfig               m_regCmd;                ///< Register list for GIC module
    CmdBufferManager*               m_pLUTCmdBufferManager;  ///< Command buffer manager for all LUTs of GIC
    CmdBuffer*                      m_pLUTDMICmdBuffer;      ///< Command buffer for holding all LUTs
    BPSGIC30ModuleConfig            m_moduleConfig;          ///< Module configuration information
    UINT32*                         m_pGICNoiseLUT;          ///< Pointer for GIC LUT used for tuning data
    gic_3_0_0::chromatix_gic30Type* m_pChromatix;            ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END

#endif // CAMXBPSGIC30_H
