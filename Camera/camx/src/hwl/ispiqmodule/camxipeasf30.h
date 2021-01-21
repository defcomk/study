////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipeasf30.h
/// @brief IPE ASF class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPEASF30_H
#define CAMXIPEASF30_H

#include "camxispiqmodule.h"
#include "ipe_data.h"
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief: This structure contains ASF registers programmed by software
struct IPEASFRegCmd
{
    // Top Registers:
    // |
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_CFG                       ASFLayer1Config;                  ///< L1 BPF output clamping
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_CFG                       ASFLayer2Config;                  ///< L2 BPF output clamping
    IPE_IPE_0_PPS_CLC_ASF_NORM_SCALE_CFG                    ASFNormalScaleConfig;             ///< L1/2 Act. BPF output norm.
    IPE_IPE_0_PPS_CLC_ASF_GAIN_CFG                          ASFGainConfig;                    ///< Mdn. Blend offset, Gain cap.
    IPE_IPE_0_PPS_CLC_ASF_NZ_FLAG                           ASFNegativeAndZeroFlag;           ///< Neg&Zero-out flgs Fltr Krnls

    //  |--> Configure Layer-1 Parameters:
    //  |    |--> Symmetric sharpening filter coefficients
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_0         ASFLayer1SharpnessCoeffConfig0;   ///< L1 SSF Coefficients 0,1
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_1         ASFLayer1SharpnessCoeffConfig1;   ///< L1 SSF Coefficients 2,3
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_2         ASFLayer1SharpnessCoeffConfig2;   ///< L1 SSF Coefficients 4,5
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_3         ASFLayer1SharpnessCoeffConfig3;   ///< L1 SSF Coefficients 6,7
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_SHARP_COEFF_CFG_4         ASFLayer1SharpnessCoeffConfig4;   ///< L1 SSF Coefficients 8,9
    //  |    |--> Low-pass filter coefficients
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_0           ASFLayer1LPFCoeffConfig0;         ///< L1 LPF Coefficients 0,1
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_1           ASFLayer1LPFCoeffConfig1;         ///< L1 LPF Coefficients 2,3
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_2           ASFLayer1LPFCoeffConfig2;         ///< L1 LPF Coefficients 4,5
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_3           ASFLayer1LPFCoeffConfig3;         ///< L1 LPF Coefficients 6,7
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_LPF_COEFF_CFG_4           ASFLayer1LPFCoeffConfig4;         ///< L1 LPF Coefficients 8,9
    //  |    |--> Band-pass filter coefficients
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_ACT_BPF_COEFF_CFG_0       ASFLayer1ActivityBPFCoeffConfig0; ///< L1 BPF Coefficients 0,1
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_ACT_BPF_COEFF_CFG_1       ASFLayer1ActivityBPFCoeffConfig1; ///< L1 BPF Coefficients 2,3
    IPE_IPE_0_PPS_CLC_ASF_LAYER_1_ACT_BPF_COEFF_CFG_2       ASFLayer1ActivityBPFCoeffConfig2; ///< L1 BPF Coefficients 4,5

    //  |--> Configure Layer-2 Parameters:
    //  |    |--> Symmetric sharpening filter coefficients
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_SHARP_COEFF_CFG_0         ASFLayer2SharpnessCoeffConfig0;   ///< L2 SSF Coefficients 0,1
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_SHARP_COEFF_CFG_1         ASFLayer2SharpnessCoeffConfig1;   ///< L2 SSF Coefficients 2,3
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_SHARP_COEFF_CFG_2         ASFLayer2SharpnessCoeffConfig2;   ///< L2 SSF Coefficients 4,5
    //  |    |--> Low-pass filter coefficients
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_LPF_COEFF_CFG_0           ASFLayer2LPFCoeffConfig0;         ///< L2 LPF Coefficients 0,1
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_LPF_COEFF_CFG_1           ASFLayer2LPFCoeffConfig1;         ///< L2 LPF Coefficients 2,3
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_LPF_COEFF_CFG_2           ASFLayer2LPFCoeffConfig2;         ///< L2 LPF Coefficients 4,5
    //  |    |--> Band-pass filter coefficients
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_ACT_BPF_COEFF_CFG_0       ASFLayer2ActivityBPFCoeffConfig0; ///< L2 BPF Coefficients 0,1
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_ACT_BPF_COEFF_CFG_1       ASFLayer2ActivityBPFCoeffConfig1; ///< L2 BPF Coefficients 2,3
    IPE_IPE_0_PPS_CLC_ASF_LAYER_2_ACT_BPF_COEFF_CFG_2       ASFLayer2ActivityBPFCoeffConfig2; ///< L2 BPF Coefficients 4,5

    //  |--> Configure RNR Parameters:
    //  |    |--> Radial Square Control Points table entries
    IPE_IPE_0_PPS_CLC_ASF_RNR_R_SQUARE_LUT_ENTRY_0          ASFRNRRSquareLUTEntry0;           ///< RS LUT 0 for radial noise
    IPE_IPE_0_PPS_CLC_ASF_RNR_R_SQUARE_LUT_ENTRY_1          ASFRNRRSquareLUTEntry1;           ///< RS LUT 1 for radial noise
    IPE_IPE_0_PPS_CLC_ASF_RNR_R_SQUARE_LUT_ENTRY_2          ASFRNRRSquareLUTEntry2;           ///< RS LUT 2 for radial noise
    //  |    |--> Radial Square Scale & Shift entries for radial noise reduction
    IPE_IPE_0_PPS_CLC_ASF_RNR_SQUARE_CFG_0                  ASFRNRSquareConfig0;              ///< RS Scale Radius&Shift values

    //  |    |--> Activity Correction Factor for Control Points
    IPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_LUT_ENTRY_0       ASFRNRActivityCFLUTEntry0;        ///< RNR activity CF LUT 0
    IPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_LUT_ENTRY_1       ASFRNRActivityCFLUTEntry1;        ///< RNR activity CF LUT 1
    IPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_LUT_ENTRY_2       ASFRNRActivityCFLUTEntry2;        ///< RNR activity CF LUT 2
    //  |    |--> Table of Slopes between for Activity Control Points
    IPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_SLOPE_LUT_ENTRY_0    ASFRNRActivitySlopeLUTEntry0;     ///< RNR act.slps btwn CPs LUT0
    IPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_SLOPE_LUT_ENTRY_1    ASFRNRActivitySlopeLUTEntry1;     ///< RNR act.slps btwn CPs LUT1
    IPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_SLOPE_LUT_ENTRY_2    ASFRNRActivitySlopeLUTEntry2;     ///< RNR act.slps btwn CPs LUT2
    //  |    |--> Table of Shift value for Slopes between Activity Control Points
    IPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_SHIFT_LUT_ENTRY_0 ASFRNRActivityCFShiftLUTEntry0;   ///< RNR shifts for slps LUT 0
    IPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_SHIFT_LUT_ENTRY_1 ASFRNRActivityCFShiftLUTEntry1;   ///< RNR shifts for slps LUT 1
    IPE_IPE_0_PPS_CLC_ASF_RNR_ACTIVITY_CF_SHIFT_LUT_ENTRY_2 ASFRNRActivityCFShiftLUTEntry2;   ///< RNR shifts for slps LUT 2
    //  |    |--> Table of Correction Factor for Control Points
    IPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_LUT_ENTRY_0           ASFRNRGainCFLUTEntry0;            ///< RNR gain CF for CP LUT 0
    IPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_LUT_ENTRY_1           ASFRNRGainCFLUTEntry1;            ///< RNR gain CF for CP LUT 1
    IPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_LUT_ENTRY_2           ASFRNRGainCFLUTEntry2;            ///< RNR gain CF for CP LUT 2
    //  |    |--> Table of Slopes between for Gain Control Points
    IPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_SLOPE_LUT_ENTRY_0        ASFRNRGainSlopeLUTEntry0;         ///< RNR gain slps btwn CPs LUT0
    IPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_SLOPE_LUT_ENTRY_1        ASFRNRGainSlopeLUTEntry1;         ///< RNR gain slps btwn CPs LUT1
    IPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_SLOPE_LUT_ENTRY_2        ASFRNRGainSlopeLUTEntry2;         ///< RNR gain slps btwn CPs LUT2
    //  |    |--> Table of Shift value for Slopes between Gain Control Points
    IPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_SHIFT_LUT_ENTRY_0     ASFRNRGainCFShiftLUTEntry0;       ///< G-CF shifts for slps LUT0
    IPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_SHIFT_LUT_ENTRY_1     ASFRNRGainCFShiftLUTEntry1;       ///< G-CF shifts for slps LUT1
    IPE_IPE_0_PPS_CLC_ASF_RNR_GAIN_CF_SHIFT_LUT_ENTRY_2     ASFRNRGainCFShiftLUTEntry2;       ///< G-CF shifts for slps LUT2

    //  |--> Configure thresholds limits:
    IPE_IPE_0_PPS_CLC_ASF_THRESHOLD_CFG_0                   ASFThresholdConfig0;              ///< Texture, Flat rgn detection
    IPE_IPE_0_PPS_CLC_ASF_THRESHOLD_CFG_1                   ASFThresholdConfig1;              ///< Smooth strength,Similarity Th

    //  |--> Configure Skin Tone detection parameters:
    IPE_IPE_0_PPS_CLC_ASF_SKIN_CFG_0                        ASFSkinConfig0;                   ///< Luma(Y) Min, Hue Min & Max
    //  |    |--> boundary Prob. for Skin Tone, Quantization level for non skin, within pre-def. skin-tone regions
    IPE_IPE_0_PPS_CLC_ASF_SKIN_CFG_1                        ASFSkinConfig1;                   ///< Boundary Prob.,Qntzn.,LumaMax
    IPE_IPE_0_PPS_CLC_ASF_SKIN_CFG_2                        ASFSkinConfig2;                   ///< S Min, S Max on Y and on Luma

    //  |--> Configure Face Detection parameters:
    //  |    |-->  Faces detected (0 -> Whole image is part of a Face)
    IPE_IPE_0_PPS_CLC_ASF_FACE_CFG                          ASFFaceConfig;                    ///< # Faces detected 0-5,En/Di FD

    //  |    |-->  Face #1 center vertical co-ordinate
    IPE_IPE_0_PPS_CLC_ASF_FACE_1_CENTER_CFG                 ASFFace1CenterConfig;             ///< Face #1 Center vert. co-ord
    //  |    |-->  Face #1 transitional slope, slope shift, boundary and radius shift
    IPE_IPE_0_PPS_CLC_ASF_FACE_1_RADIUS_CFG                 ASFFace1RadiusConfig;             ///< F1 slp, slp.shft, bndr, r-shf
    IPE_IPE_0_PPS_CLC_ASF_FACE_2_CENTER_CFG                 ASFFace2CenterConfig;             ///< Face #2 Center vert. co-ord
    //  |    |-->  Face #2 transitional slope, slope shift, boundary and radius shift
    IPE_IPE_0_PPS_CLC_ASF_FACE_2_RADIUS_CFG                 ASFFace2RadiusConfig;             ///< F2 slp, slp.shft, bndr, r-shf
    IPE_IPE_0_PPS_CLC_ASF_FACE_3_CENTER_CFG                 ASFFace3CenterConfig;             ///< Face #3 Center vert. co-ord
    //  |    |-->  Face #3 transitional slope, slope shift, boundary and radius shift
    IPE_IPE_0_PPS_CLC_ASF_FACE_3_RADIUS_CFG                 ASFFace3RadiusConfig;             ///< F3 slp, slp.shft, bndr, r-shf
    IPE_IPE_0_PPS_CLC_ASF_FACE_4_CENTER_CFG                 ASFFace4CenterConfig;             ///< Face #4 Center vert. co-ord
    //  |    |-->  Face #4 transitional slope, slope shift, boundary and radius shift
    IPE_IPE_0_PPS_CLC_ASF_FACE_4_RADIUS_CFG                 ASFFace4RadiusConfig;             ///< F4 slp, slp.shft, bndr, r-shf
    IPE_IPE_0_PPS_CLC_ASF_FACE_5_CENTER_CFG                 ASFFace5CenterConfig;             ///< Face #5 Center vert. co-ord
    //  |    |-->  Face #5 transitional slope, slope shift, boundary and radius shift
    IPE_IPE_0_PPS_CLC_ASF_FACE_5_RADIUS_CFG                 ASFFace5RadiusConfig;             ///< F5 slp, slp.shft, bndr, r-shf
} CAMX_PACKED;

CAMX_END_PACKED

static const UINT32 IPEASFRegLength = sizeof(IPEASFRegCmd) / RegisterWidthInBytes;
CAMX_STATIC_ASSERT((65) == IPEASFRegLength);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Class for IPE ASF Module
///
///         IPE::PPS::ASF  component operates  on YUV  Color Space  Format  (YUV 4:4:4)  10bit input  to perform  image
///         enhancement features  such as Edge Enhancement,  Edge Alignment, Sharpening of  Skin-tone Texture, Contrast
///         Improvement of the Darker details to near by  White objects/boundaries and Special Effects. Output would be
///         in YUV 4:4:4 format  to be used in Viewfinder, Video Capture and  Snapshot processing modes. These features
///         are achieved by operating on the Luminance/Luma (Y)  channel, with the help of various mechanisms supported
///         -  Cross-Media Filter:  3x3, the  Sharpening Filters  - Layer-1:  7x7, Layer-2:  13x13, parameters  such as
///         Thresholds and  Gain to control sharpeneing  and smoothing effects, Radial  Correction, Clamping, Blending,
///         local Y  & activity (busyness)  values for contrast  and texture, besides  sketch, emboss and  neon special
///         effects. Chroma component of the image are used to  sharpen objects with skin tone but around the face map,
///         gain reduction based on chroma gradient for color edges.
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPEASF30 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create ASF Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for ASF Creation
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IPEModuleCreateData* pCreateData);

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
    /// ~IPEASF30
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPEASF30();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEASF30
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPEASF30(
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
    /// Initialize
    ///
    /// @brief  Initialize ASF Object
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateDependenceParams
    ///
    /// @brief  Validate the input info from client
    ///
    /// @param  pInputData Pointer to the IPE/PPS/ASF input data
    ///
    /// @return CamxResult Indicates if the input is valid or invalid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ValidateDependenceParams(
        const ISPInputData* pInputData
        ) const;

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
    /// FetchDMIBuffer
    ///
    /// @brief  Fetch ASF30 DMI LUT
    ///
    /// @return CamxResult Indicates if fetch DMI was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FetchDMIBuffer();

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
        const ISPInputData* pInputData
        ) const;

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
    /// WriteLUTEntrytoDMI
    ///
    /// @brief  Create the Command List
    ///
    /// @param  pDMICmdBuffer   Pointer to the DMI command buffer used for programming HFI CDM packet/payload
    /// @param  LUTIndex        ASF LUT Index [1..MaxASFLUT-1]
    /// @param  pLUTOffset      [io] Pointer to LUT offset into DMI command buffer; incremented by LUTSize, if not-NULL
    /// @param  LUTSize         Size of LUT (in bytes)
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteLUTEntrytoDMI(
        CmdBuffer*   pDMICmdBuffer,
        const UINT8  LUTIndex,
        UINT32*      pLUTOffset,
        const UINT32 LUTSize);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteLUTtoDMI
    ///
    /// @brief  Write Configuration, Look Up Table Data through DMI
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteLUTtoDMI(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteConfigCmds
    ///
    /// @brief  Write Configuration Commands
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteConfigCmds(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetAdditionalASF30Input
    ///
    /// @brief  Initialize ASF 30 additional Input
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SetAdditionalASF30Input(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of ASF module for debug
    ///
    /// @param  pDMIDataPtr Pointer to the DMI data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig(
        UINT32* pDMIDataPtr
        ) const;

    IPEASF30(const IPEASF30&)            = delete;           ///< Disallow the copy constructor
    IPEASF30& operator=(const IPEASF30&) = delete;           ///< Disallow copy assignment operator

    // Member variables
    const CHAR*                     m_pNodeIdentifier;       ///< String identifier for the Node that created this object
    IPEASFRegCmd                    m_regCmd;                ///< Register list of this module
    ASF30InputData                  m_dependenceData;        ///< Dependence Data for this Module
    AsfParameters                   m_ASFParameters;         ///< IQ parameters

    CmdBufferManager*               m_pLUTCmdBufferManager;  ///< DMI Command buffer manager for all LUTs
    CmdBuffer*                      m_pLUTDMICmdBuffer;      ///< LUT DMI Command buffer

    UINT32*                         m_pASFLUTs;              ///< Tuning ASF, holder of LUTs
    BOOL                            m_bypassMode;            ///< Bypass ASF

    asf_3_0_0::chromatix_asf30Type* m_pChromatix;            ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END

#endif // CAMXIPEASF30_H
