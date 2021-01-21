////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 - 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifemnds16.h
/// @brief camxIFEMNDS16 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFEMNDS16_H
#define CAMXIFEMNDS16_H

#include "camxispiqmodule.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 ScaleRatioLimit          = 105;    ///< Scaling ratio limit beyond which artificats are seen
static const UINT32 MNDS16InterpolationShift = 14;     ///< Interpolation shift value

static const UINT32 MNDS16ScalingLimit       = 128;    ///< Downscaling limit of MNDS 16 version
static const UINT32 MNDS16MaxScaleFactor     = 64;     ///< Max downscaling factor considering chroma needs to be
                                                       /// downscaled twice as much as luma channel

CAMX_BEGIN_PACKED
struct IFEMNDS16FDLumaReg
{
    IFE_IFE_0_VFE_SCALE_FD_Y_CFG                    config;               ///< FD out Luma Channel config
    IFE_IFE_0_VFE_SCALE_FD_Y_H_IMAGE_SIZE_CFG       horizontalSize;       ///< FD out Luma horizontal image size config
    IFE_IFE_0_VFE_SCALE_FD_Y_H_PHASE_CFG            horizontalPhase;      ///< FD out Luma horizontal phase config
    IFE_IFE_0_VFE_SCALE_FD_Y_H_STRIPE_CFG_0         horizontalStripe0;    ///< FD out Luma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_FD_Y_H_STRIPE_CFG_1         horizontalStripe1;    ///< FD out Luma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_FD_Y_H_PAD_CFG              horizontalPadding;    ///< FD out Luma horizontal padding config
    IFE_IFE_0_VFE_SCALE_FD_Y_V_IMAGE_SIZE_CFG       verticalSize;         ///< FD out Luma vertical image size config
    IFE_IFE_0_VFE_SCALE_FD_Y_V_PHASE_CFG            verticalPhase;        ///< FD out Luma vertical phase config
    IFE_IFE_0_VFE_SCALE_FD_Y_V_STRIPE_CFG_0         verticalStripe0;      ///< FD out Luma vertical stripe config
    IFE_IFE_0_VFE_SCALE_FD_Y_V_STRIPE_CFG_1         verticalStripe1;      ///< FD out Luma vertical stripe config
    IFE_IFE_0_VFE_SCALE_FD_Y_V_PAD_CFG              verticalPadding;      ///< FD out Luma vertical padding config
} CAMX_PACKED;

struct IFEMNDS16FDChromaReg
{
    IFE_IFE_0_VFE_SCALE_FD_CBCR_CFG                 config;               ///< FD out Chroma Channel config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_H_IMAGE_SIZE_CFG    horizontalSize;       ///< FD out Chroma horizontal image size config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_H_PHASE_CFG         horizontalPhase;      ///< FD out Chroma horizontal phase config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_H_STRIPE_CFG_0      horizontalStripe0;    ///< FD out Chroma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_H_STRIPE_CFG_1      horizontalStripe1;    ///< FD out Chroma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_H_PAD_CFG           horizontalPadding;    ///< FD out Chroma horizontal padding config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_V_IMAGE_SIZE_CFG    verticalSize;         ///< FD out Chroma vertical image size config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_V_PHASE_CFG         verticalPhase;        ///< FD out Chroma vertical phase config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_V_STRIPE_CFG_0      verticalStripe0;      ///< FD out Chroma vertical stripe config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_V_STRIPE_CFG_1      verticalStripe1;      ///< FD out Chroma vertical stripe config
    IFE_IFE_0_VFE_SCALE_FD_CBCR_V_PAD_CFG           verticalPadding;      ///< FD out Chroma vertical padding config
} CAMX_PACKED;

struct IFEMNDS16VideoLumaReg
{
    IFE_IFE_0_VFE_SCALE_VID_Y_CFG                   config;               ///< Video(Full) out Luma Channel config
    IFE_IFE_0_VFE_SCALE_VID_Y_H_IMAGE_SIZE_CFG      horizontalSize;       ///< Video(Full) out Luma horizontal image size config
    IFE_IFE_0_VFE_SCALE_VID_Y_H_PHASE_CFG           horizontalPhase;      ///< Video(Full) out Luma horizontal phase config
    IFE_IFE_0_VFE_SCALE_VID_Y_H_STRIPE_CFG_0        horizontalStripe0;    ///< Video(Full) out Luma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_VID_Y_H_STRIPE_CFG_1        horizontalStripe1;    ///< Video(Full) out Luma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_VID_Y_H_PAD_CFG             horizontalPadding;    ///< Video(Full) out Luma horizontal padding config
    IFE_IFE_0_VFE_SCALE_VID_Y_V_IMAGE_SIZE_CFG      verticalSize;         ///< Video(Full) out Luma vertical image size config
    IFE_IFE_0_VFE_SCALE_VID_Y_V_PHASE_CFG           verticalPhase;        ///< Video(Full) out Luma vertical phase config
    IFE_IFE_0_VFE_SCALE_VID_Y_V_STRIPE_CFG_0        verticalStripe0;      ///< Video(Full) out Luma vertical stripe config
    IFE_IFE_0_VFE_SCALE_VID_Y_V_STRIPE_CFG_1        verticalStripe1;      ///< Video(Full) out Luma vertical stripe config
    IFE_IFE_0_VFE_SCALE_VID_Y_V_PAD_CFG             verticalPadding;      ///< Video(Full) out Luma vertical padding config
} CAMX_PACKED;

struct IFEMNDS16VideoChromaReg
{
    IFE_IFE_0_VFE_SCALE_VID_CBCR_CFG                config;              ///< Video(Full) out Chroma Channel config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_H_IMAGE_SIZE_CFG   horizontalSize;      ///< Video(Full) out Chroma horizontal image size
                                                                         /// config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_H_PHASE_CFG        horizontalPhase;     ///< Video(Full) out Chroma horizontal phase config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_H_STRIPE_CFG_0     horizontalStripe0;   ///< Video(Full) out Chroma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_H_STRIPE_CFG_1     horizontalStripe1;   ///< Video(Full) out Chroma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_H_PAD_CFG          horizontalPadding;   ///< Video(Full) out Chroma horizontal padding config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_V_IMAGE_SIZE_CFG   verticalSize;        ///< Video(Full) out Chroma vertical image size config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_V_PHASE_CFG        verticalPhase;       ///< Video(Full) out Chroma vertical phase config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_V_STRIPE_CFG_0     verticalStripe0;     ///< Video(Full) out Chroma vertical stripe config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_V_STRIPE_CFG_1     verticalStripe1;     ///< Video(Full) out Chroma vertical stripe config
    IFE_IFE_0_VFE_SCALE_VID_CBCR_V_PAD_CFG          verticalPadding;     ///< Video(Full) out Chroma vertical padding config
} CAMX_PACKED;


struct IFEMNDS16DisplayLumaReg
{
    IFE_IFE_0_VFE_SCALE_DISP_Y_CFG                   config;            ///< Display(Full) out Luma Channel config
    IFE_IFE_0_VFE_SCALE_DISP_Y_H_IMAGE_SIZE_CFG      horizontalSize;    ///< Display(Full) out Luma horizontal image size config
    IFE_IFE_0_VFE_SCALE_DISP_Y_H_PHASE_CFG           horizontalPhase;   ///< Display(Full) out Luma horizontal phase config
    IFE_IFE_0_VFE_SCALE_DISP_Y_H_STRIPE_CFG_0        horizontalStripe0; ///< Display(Full) out Luma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_DISP_Y_H_STRIPE_CFG_1        horizontalStripe1; ///< Display(Full) out Luma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_DISP_Y_H_PAD_CFG             horizontalPadding; ///< Display(Full) out Luma horizontal padding config
    IFE_IFE_0_VFE_SCALE_DISP_Y_V_IMAGE_SIZE_CFG      verticalSize;      ///< Display(Full) out Luma vertical image size config
    IFE_IFE_0_VFE_SCALE_DISP_Y_V_PHASE_CFG           verticalPhase;     ///< Display(Full) out Luma vertical phase config
    IFE_IFE_0_VFE_SCALE_DISP_Y_V_STRIPE_CFG_0        verticalStripe0;   ///< Display(Full) out Luma vertical stripe config
    IFE_IFE_0_VFE_SCALE_DISP_Y_V_STRIPE_CFG_1        verticalStripe1;   ///< Display(Full) out Luma vertical stripe config
    IFE_IFE_0_VFE_SCALE_DISP_Y_V_PAD_CFG             verticalPadding;   ///< Display(Full) out Luma vertical padding config
} CAMX_PACKED;

struct IFEMNDS16DisplayChromaReg
{
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_CFG                config;            ///< Display(Full) out Chroma Channel config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_H_IMAGE_SIZE_CFG   horizontalSize;    ///< Display(Full) out Chroma horizontal image size
                                                                        /// config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_H_PHASE_CFG        horizontalPhase;   ///< Display(Full) out Chroma horizontal phase config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_H_STRIPE_CFG_0     horizontalStripe0; ///< Display(Full) out Chroma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_H_STRIPE_CFG_1     horizontalStripe1; ///< Display(Full) out Chroma horizontal stripe config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_H_PAD_CFG          horizontalPadding; ///< Display(Full) out Chroma horizontal padding config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_V_IMAGE_SIZE_CFG   verticalSize;      ///< Display(Full) out Chroma vertical image size config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_V_PHASE_CFG        verticalPhase;     ///< Display(Full) out Chroma vertical phase config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_V_STRIPE_CFG_0     verticalStripe0;   ///< Display(Full) out Chroma vertical stripe config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_V_STRIPE_CFG_1     verticalStripe1;   ///< Display(Full) out Chroma vertical stripe config
    IFE_IFE_0_VFE_SCALE_DISP_CBCR_V_PAD_CFG          verticalPadding;   ///< Display(Full) out Chroma vertical padding config
} CAMX_PACKED;

CAMX_END_PACKED

struct IFEMNDS16RegCmd
{
    IFEMNDS16FDLumaReg          FDLuma;         ///< FD out Luma register config
    IFEMNDS16FDChromaReg        FDChroma;       ///< FD out Chroma register config
    IFEMNDS16VideoLumaReg       videoLuma;      ///< Video(Full) out Luma register config
    IFEMNDS16VideoChromaReg     videoChroma;    ///< Video(Full) out Chroma register config
    IFEMNDS16DisplayLumaReg     displayLuma;    ///< Display(Full) out Luma register config
    IFEMNDS16DisplayChromaReg   displayChroma;  ///< Display(Full) out Chroma register config
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE MNDS16 Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFEMNDS16 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create MNDS16 Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for MNDS Creation
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
    /// ~IFEMNDS16
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFEMNDS16();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEMNDS16
    ///
    /// @brief  Constructor
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for MNDS Creation
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IFEMNDS16(
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
    /// CalculateScalerOutput
    ///
    /// @brief  Calculate scaler output dimensions
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID CalculateScalerOutput(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureRegisters
    ///
    /// @brief  Configure scaler registers
    ///
    /// @return CamxResult success if all configuration done, else error code
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ConfigureRegisters();


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureFDLumaRegisters
    ///
    /// @brief  Configure scaler FD Luma output path registers
    ///
    /// @param  horizontalInterpolationResolution horizontal interpolation resolution
    /// @param  verticalInterpolationResolution   vertical interpolation resolution
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureFDLumaRegisters(
        UINT32 horizontalInterpolationResolution,
        UINT32 verticalInterpolationResolution);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureFullLumaRegisters
    ///
    /// @brief  Configure scaler Full Luma output path registers
    ///
    /// @param  horizontalInterpolationResolution horizontal interpolation resolution
    /// @param  verticalInterpolationResolution   vertical interpolation resolution
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureFullLumaRegisters(
        UINT32 horizontalInterpolationResolution,
        UINT32 verticalInterpolationResolution);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDisplayLumaRegisters
    ///
    /// @brief  Configure scaler Display Luma output path registers
    ///
    /// @param  horizontalInterpolationResolution horizontal interpolation resolution
    /// @param  verticalInterpolationResolution   vertical interpolation resolution
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDisplayLumaRegisters(
        UINT32 horizontalInterpolationResolution,
        UINT32 verticalInterpolationResolution);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureFDChromaRegisters
    ///
    /// @brief  Configure scaler FD Chroma output path registers
    ///
    /// @param  horizontalInterpolationResolution horizontal interpolation resolution
    /// @param  verticalInterpolationResolution   vertical interpolation resolution
    /// @param  horizontalSubSampleFactor         horizontal subsampling factor
    /// @param  verticalSubsampleFactor           vertical subsampling factor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureFDChromaRegisters(
       UINT32 horizontalInterpolationResolution,
       UINT32 verticalInterpolationResolution,
       FLOAT  horizontalSubSampleFactor,
       FLOAT  verticalSubsampleFactor);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureFullChromaRegisters
    ///
    /// @brief  Configure scaler Full Chroma output path registers
    ///
    /// @param  horizontalInterpolationResolution horizontal interpolation resolution
    /// @param  verticalInterpolationResolution   vertical interpolation resolution
    /// @param  horizontalSubSampleFactor         horizontal subsampling factor
    /// @param  verticalSubsampleFactor           vertical subsampling factor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureFullChromaRegisters(
        UINT32 horizontalInterpolationResolution,
        UINT32 verticalInterpolationResolution,
        FLOAT  horizontalSubSampleFactor,
        FLOAT  verticalSubsampleFactor);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDisplayChromaRegisters
    ///
    /// @brief  Configure scaler Display Chroma output path registers
    ///
    /// @param  horizontalInterpolationResolution horizontal interpolation resolution
    /// @param  verticalInterpolationResolution   vertical interpolation resolution
    /// @param  horizontalSubSampleFactor         horizontal subsampling factor
    /// @param  verticalSubsampleFactor           vertical subsampling factor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDisplayChromaRegisters(
        UINT32 horizontalInterpolationResolution,
        UINT32 verticalInterpolationResolution,
        FLOAT  horizontalSubSampleFactor,
        FLOAT  verticalSubsampleFactor);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CalculateInterpolationResolution
    ///
    /// @brief  Calculate the interpolation resolution for scaling
    ///
    /// @param  scaleFactorHorizontal              horizontal scale factor
    /// @param  scaleFactorVertical                vertical scale factor
    /// @param  pHorizontalInterpolationResolution Pointer to horizontal interpolation resolution
    /// @param  pVerticalInterpolationResolution   Pointer to vertical interpolation resolution
    ///
    /// @return CamxResult Invalid input Error code for invalid scaling or format, else success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CalculateInterpolationResolution(
        UINT32  scaleFactorHorizontal,
        UINT32  scaleFactorVertical,
        UINT32* pHorizontalInterpolationResolution,
        UINT32* pVerticalInterpolationResolution);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetChromaSubsampleFactor
    ///
    /// @brief  Get the subsample factor based on the pixel format
    ///
    /// @param  pHorizontalSubSampleFactor Pointer to horizontal chroma subsample factor
    /// @param  pVerticalSubsampleFactor   Pointer to Vertical chroma subsample factor
    ///
    /// @return CamxResult success if the format is handled
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult GetChromaSubsampleFactor(
        FLOAT* pHorizontalSubSampleFactor,
        FLOAT* pVerticalSubsampleFactor);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of MNDS module for debug
    ///
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig();

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
    /// CreateCmdList
    ///
    /// @brief  Create the Command List
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        ISPInputData* pInputData);

    IFEMNDS16(const IFEMNDS16&)            = delete;    ///< Disallow the copy constructor
    IFEMNDS16& operator=(const IFEMNDS16&) = delete;    ///< Disallow assignment operator

    IFEMNDS16RegCmd    m_regCmd;                        ///< Register List of this Module
    MNDSState*         m_pState;                        ///< State
    Format             m_pixelFormat;                   ///< Output pixel format
    BIT                m_isBayer;                       ///< Flag to indicate Bayer sensor
    IFEPipelinePath    m_modulePath;                    ///< IFE pipeline path for module
    UINT32             m_output;                        ///< IPE piepline output
    ISPInputData*      m_pInputData;                    ///< Pointer to current input data
};

CAMX_NAMESPACE_END

#endif // CAMXIFEMNDS16_H
