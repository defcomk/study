////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpshnr10.h
/// @brief bpshnr10 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXBPSHNR10_H
#define CAMXBPSHNR10_H

#include "bps_data.h"
#include "camxispiqmodule.h"
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief HNR10 register Configuration
struct BPSHNR10RegConfig
{
    BPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_0            nrGainCoefficient0;        ///< HNR NR GAIN REGISTER0
    BPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_1            nrGainCoefficient1;        ///< HNR NR GAIN REGISTER1
    BPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_2            nrGainCoefficient2;        ///< HNR NR GAIN REGISTER2
    BPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_3            nrGainCoefficient3;        ///< HNR NR GAIN REGISTER3
    BPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_4            nrGainCoefficient4;        ///< HNR NR GAIN REGISTER4
    BPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_5            nrGainCoefficient5;        ///< HNR NR GAIN REGISTER5
    BPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_6            nrGainCoefficient6;        ///< HNR NR GAIN REGISTER6
    BPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_7            nrGainCoefficient7;        ///< HNR NR GAIN REGISTER7
    BPS_BPS_0_CLC_HNR_NR_GAIN_TABLE_8            nrGainCoefficient8;        ///< HNR NR GAIN REGISTER8
    BPS_BPS_0_CLC_HNR_CNR_CFG_0                  cnrCFGCoefficient0;        ///< HNR CNR CONFIG REGISTER0
    BPS_BPS_0_CLC_HNR_CNR_CFG_1                  cnrCFGCoefficient1;        ///< HNR CNR CONFIG REGISTER1
    BPS_BPS_0_CLC_HNR_CNR_GAIN_TABLE_0           cnrGaninCoeffincient0;     ///< HNR CNR GAIN REGISTER0
    BPS_BPS_0_CLC_HNR_CNR_GAIN_TABLE_1           cnrGaninCoeffincient1;     ///< HNR CNR GAIN REGISTER1
    BPS_BPS_0_CLC_HNR_CNR_GAIN_TABLE_2           cnrGaninCoeffincient2;     ///< HNR CNR GAIN REGISTER2
    BPS_BPS_0_CLC_HNR_CNR_GAIN_TABLE_3           cnrGaninCoeffincient3;     ///< HNR CNR GAIN REGISTER3
    BPS_BPS_0_CLC_HNR_CNR_GAIN_TABLE_4           cnrGaninCoeffincient4;     ///< HNR CNR GAIN REGISTER4
    BPS_BPS_0_CLC_HNR_SNR_CFG_0                  snrCoefficient0;           ///< HNR SNR CONFIG REGISTER0
    BPS_BPS_0_CLC_HNR_SNR_CFG_1                  snrCoefficient1;           ///< HNR SNR CONFIG REGISTER1
    BPS_BPS_0_CLC_HNR_SNR_CFG_2                  snrCoefficient2;           ///< HNR SNR CONFIG REGISTER2
    BPS_BPS_0_CLC_HNR_FACE_CFG                   faceConfigCoefficient;     ///< HNR FACE CONFIG REGISTER
    BPS_BPS_0_CLC_HNR_FACE_OFFSET_CFG            faceOffsetCoefficient;     ///< HNR FACE OFFSET CONFIG REGISTER
    BPS_BPS_0_CLC_HNR_FACE_0_CENTER_CFG          faceCoefficient0;          ///< HNR FACE CENTER CONFIG REGISTER0
    BPS_BPS_0_CLC_HNR_FACE_1_CENTER_CFG          faceCoefficient1;          ///< HNR FACE CENTER CONFIG REGISTER1
    BPS_BPS_0_CLC_HNR_FACE_2_CENTER_CFG          faceCoefficient2;          ///< HNR FACE CENTER CONFIG REGISTER2
    BPS_BPS_0_CLC_HNR_FACE_3_CENTER_CFG          faceCoefficient3;          ///< HNR FACE CENTER CONFIG REGISTER3
    BPS_BPS_0_CLC_HNR_FACE_4_CENTER_CFG          faceCoefficient4;          ///< HNR FACE CENTER CONFIG REGISTER4
    BPS_BPS_0_CLC_HNR_FACE_0_RADIUS_CFG          radiusCoefficient0;        ///< HNR FACE RADIUS CONFIG REGISTER0
    BPS_BPS_0_CLC_HNR_FACE_1_RADIUS_CFG          radiusCoefficient1;        ///< HNR FACE RADIUS CONFIG REGISTER0
    BPS_BPS_0_CLC_HNR_FACE_2_RADIUS_CFG          radiusCoefficient2;        ///< HNR FACE RADIUS CONFIG REGISTER0
    BPS_BPS_0_CLC_HNR_FACE_3_RADIUS_CFG          radiusCoefficient3;        ///< HNR FACE RADIUS CONFIG REGISTER0
    BPS_BPS_0_CLC_HNR_FACE_4_RADIUS_CFG          radiusCoefficient4;        ///< HNR FACE RADIUS CONFIG REGISTER0
    BPS_BPS_0_CLC_HNR_RNR_ANCHOR_BASE_SETTINGS_0 rnrAnchorCoefficient0;     ///< RNR ANCHOR BASE SETTING REGISTER0
    BPS_BPS_0_CLC_HNR_RNR_ANCHOR_BASE_SETTINGS_1 rnrAnchorCoefficient1;     ///< RNR ANCHOR BASE SETTING REGISTER1
    BPS_BPS_0_CLC_HNR_RNR_ANCHOR_BASE_SETTINGS_2 rnrAnchorCoefficient2;     ///< RNR ANCHOR BASE SETTING REGISTER2
    BPS_BPS_0_CLC_HNR_RNR_ANCHOR_BASE_SETTINGS_3 rnrAnchorCoefficient3;     ///< RNR ANCHOR BASE SETTING REGISTER3
    BPS_BPS_0_CLC_HNR_RNR_ANCHOR_BASE_SETTINGS_4 rnrAnchorCoefficient4;     ///< RNR ANCHOR BASE SETTING REGISTER4
    BPS_BPS_0_CLC_HNR_RNR_ANCHOR_BASE_SETTINGS_5 rnrAnchorCoefficient5;     ///< RNR ANCHOR BASE SETTING REGISTER5
    BPS_BPS_0_CLC_HNR_RNR_SLOPE_SHIFT_SETTINGS_0 rnrSlopeShiftCoefficient0; ///< RNR SLOPE SHIFT SETTING REGISTER0
    BPS_BPS_0_CLC_HNR_RNR_SLOPE_SHIFT_SETTINGS_1 rnrSlopeShiftCoefficient1; ///< RNR SLOPE SHIFT SETTING REGISTER1
    BPS_BPS_0_CLC_HNR_RNR_SLOPE_SHIFT_SETTINGS_2 rnrSlopeShiftCoefficient2; ///< RNR SLOPE SHIFT SETTING REGISTER2
    BPS_BPS_0_CLC_HNR_RNR_SLOPE_SHIFT_SETTINGS_3 rnrSlopeShiftCoefficient3; ///< RNR SLOPE SHIFT SETTING REGISTER3
    BPS_BPS_0_CLC_HNR_RNR_SLOPE_SHIFT_SETTINGS_4 rnrSlopeShiftCoefficient4; ///< RNR SLOPE SHIFT SETTING REGISTER4
    BPS_BPS_0_CLC_HNR_RNR_SLOPE_SHIFT_SETTINGS_5 rnrSlopeShiftCoefficient5; ///< RNR SLOPE SHIFT SETTING REGISTER5
    BPS_BPS_0_CLC_HNR_RNR_INIT_HV_OFFSET         hnrCoefficient1;           ///< RNR INIT HV OFFSET REGISTER
    BPS_BPS_0_CLC_HNR_RNR_R_SQUARE_INIT          rnr_r_squareCoefficient;   ///< RNR R SQUARE INIT REGISTER
    BPS_BPS_0_CLC_HNR_RNR_R_SCALE_SHIFT          rnr_r_ScaleCoefficient;    ///< RNR R SCALE SHIFT REGISTER
    BPS_BPS_0_CLC_HNR_LPF3_CFG                   lpf3ConfigCoefficient;     ///< HNR LPF3 CONFIG REGISTER
    BPS_BPS_0_CLC_HNR_MISC_CFG                   miscConfigCoefficient;     ///< HNR MIS CONFIG REGISTER
} CAMX_PACKED;

CAMX_END_PACKED

static const UINT32 HNRMaxLUT                    = 6;
static const UINT32 LNRGainLUT                   = BPS_BPS_0_CLC_HNR_DMI_LUT_CFG_LUT_SEL_LNR_GAIN_ARR;
static const UINT32 MergedFNRGainAndGainClampLUT = BPS_BPS_0_CLC_HNR_DMI_LUT_CFG_LUT_SEL_MERGED_FNR_GAIN_ARR_GAIN_CLAMP_ARR;
static const UINT32 FNRAcLUT                     = BPS_BPS_0_CLC_HNR_DMI_LUT_CFG_LUT_SEL_FNR_AC_TH_ARR;
static const UINT32 SNRGainLUT                   = BPS_BPS_0_CLC_HNR_DMI_LUT_CFG_LUT_SEL_SNR_GAIN_ARR;
static const UINT32 BlendLNRGainLUT              = BPS_BPS_0_CLC_HNR_DMI_LUT_CFG_LUT_SEL_BLEND_LNR_GAIN_ARR;
static const UINT32 BlendSNRGainLUT              = BPS_BPS_0_CLC_HNR_DMI_LUT_CFG_LUT_SEL_BLEND_SNR_GAIN_ARR;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief BPSHNR10 Class Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class BPSHNR10 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create BPSHNR10 Object
    ///
    /// @param  pCreateData Pointer to the BPSHNR10 Creation
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
    /// ~BPSHNR10
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~BPSHNR10();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSHNR10
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit BPSHNR10(
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
    /// @return TRUE if dependencies met
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckDependenceChange(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculation
    ///
    /// @brief  Calculate the Register Value
    ///
    /// @param  pInputData Pointer to the HNR Module input data
    ///
    /// @return CamxResult Indicates if interpolation and configure registers was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FetchDMIBuffer
    ///
    /// @brief  Fetch DMI buffer
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
    /// @brief  Print register configuration of HNR module for debug
    ///
    /// @param  pInputData    Pointer to the ISP input data
    /// @param  pModuleConfig Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig(
        const ISPInputData* pInputData,
        const HnrParameters* pModuleConfig
        ) const;

    BPSHNR10(const BPSHNR10&)            = delete;            ///< Disallow the copy constructor
    BPSHNR10& operator=(const BPSHNR10&) = delete;            ///< Disallow assignment operator

    const CHAR*                     m_pNodeIdentifier;        ///< String identifier of this Node
    HNR10InputData                  m_dependenceData;         ///< InputData
    BPSHNR10RegConfig               m_regCmd;                 ///< Register list for HNR module
    CmdBufferManager*               m_pLUTCmdBufferManager;   ///< Command buffer manager for all LUTs of HNR
    CmdBuffer*                      m_pLUTDMICmdBuffer;       ///< Command buffer for holding all LUTs
    HnrParameters                   m_HNRParameters;          ///< HNR parameters needed for the Firmware

    UINT32*                         m_pLNRDMIBuffer;          ///< Tuning pointer to the LNR Dmi data
    UINT32*                         m_pFNRAndClampDMIBuffer;  ///< Tuning pointer to the Merged FNR and Gain Clamp data
    UINT32*                         m_pFNRAcDMIBuffer;        ///< Tuning pointer to the FNR ac data
    UINT32*                         m_pSNRDMIBuffer;          ///< Tuning pointer to the SNR data
    UINT32*                         m_pBlendLNRDMIBuffer;     ///< Tuning pointer to the Blend LNR data
    UINT32*                         m_pBlendSNRDMIBuffer;     ///< Tuning pointer to the Blend SNR data
    UINT8                           m_noiseReductionMode;     ///< noise reduction mode
    hnr_1_0_0::chromatix_hnr10Type* m_pChromatix;             ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END

#endif // CAMXBPSHNR10_H
