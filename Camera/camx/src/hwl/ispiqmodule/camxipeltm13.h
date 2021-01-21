////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipeltm13.h
/// @brief IPELTM class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPELTM13_H
#define CAMXIPELTM13_H

#include "camxispiqmodule.h"
#include "ltm_1_3_0.h"
#include "iqcommondefs.h"
#include "ltm13setting.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief LTM13 register Configuration
struct IPELTM13RegConfig
{
    IPE_IPE_0_PPS_CLC_LTM_MODULE_CFG                    LTMModuleConfig;                 ///< LTM module config
    IPE_IPE_0_PPS_CLC_LTM_DC_CFG_0                      LTMDataCollectionConfig0;        ///< LTM Data collector config 0
    IPE_IPE_0_PPS_CLC_LTM_DC_CFG_1                      LTMDataCollectionConfig1;        ///< LTM Data collector config 1
    IPE_IPE_0_PPS_CLC_LTM_DC_CFG_2                      LTMDataCollectionConfig2;        ///< LTM Data collector config 2
    IPE_IPE_0_PPS_CLC_LTM_RGB2Y_CFG                     LTMRGB2YConfig;                  ///< LTM RGB2Y config
    IPE_IPE_0_PPS_CLC_LTM_IP_0_CFG                      LTMImageProcessingConfig0;       ///< LTM Image processing config 0
    IPE_IPE_0_PPS_CLC_LTM_IP_1_CFG                      LTMImageProcessingConfig1;       ///< LTM Image processing config 1
    IPE_IPE_0_PPS_CLC_LTM_IP_2_CFG                      LTMImageProcessingConfig2;       ///< LTM Image processing config 2
    IPE_IPE_0_PPS_CLC_LTM_IP_3_CFG                      LTMImageProcessingConfig3;       ///< LTM Image processing config 3
    IPE_IPE_0_PPS_CLC_LTM_IP_4_CFG                      LTMImageProcessingConfig4;       ///< LTM Image processing config 4
    IPE_IPE_0_PPS_CLC_LTM_IP_5_CFG                      LTMImageProcessingConfig5;       ///< LTM Image processing config 5
    IPE_IPE_0_PPS_CLC_LTM_IP_6_CFG                      LTMImageProcessingConfig6;       ///< LTM Image processing config 6
    IPE_IPE_0_PPS_CLC_LTM_MASK_0_CFG                    LTMMaskFilterCoefficientConfig0; ///< LTM Mask config 0
    IPE_IPE_0_PPS_CLC_LTM_MASK_1_CFG                    LTMMaskFilterCoefficientConfig1; ///< LTM Mask config 1
    IPE_IPE_0_PPS_CLC_LTM_DOWNSCALE_MN_Y_CFG            LTMDownScaleMNYConfig;           ///< LTM downscale MN Y config
    IPE_IPE_0_PPS_CLC_LTM_DOWNSCALE_MN_Y_IMAGE_SIZE_CFG LTMDownScaleMNYimageSizeConfig;  ///< LTM downscale MN Y image
                                                                                         ///< size config
    IPE_IPE_0_PPS_CLC_LTM_DOWNSCALE_MN_Y_H_CFG          LTMDownScaleMNYHConfig;          ///< LTM downscale MN Y H config
    IPE_IPE_0_PPS_CLC_LTM_DOWNSCALE_MN_Y_H_PHASE_CFG    LTMDownScaleMNYHPhaseConfig;     ///< LTM downscale MN Y config
    IPE_IPE_0_PPS_CLC_LTM_DOWNSCALE_MN_Y_V_CFG          LTMDownScaleMNYVConfig;          ///< LTM downscale MN Y config
    IPE_IPE_0_PPS_CLC_LTM_DOWNSCALE_MN_Y_V_PHASE_CFG    LTMDownScaleMNYVPhaseConfig;     ///< LTM downscale MN Y config
} CAMX_PACKED;

/// @brief LTM13 Module Configuration
struct IPELTM13ModuleConfig
{
    IPE_IPE_0_PPS_CLC_LTM_MODULE_CFG moduleConfig;   ///< IPELTM13 Module config
} CAMX_PACKED;

/// @brief Output Data to LTM13 IQ Algorithem
struct LTM13OutputData
{
    IPELTM13RegConfig*    pRegCmd;          ///< Pointer to the IPELTM13 Register buffer
    IPELTM13ModuleConfig* pModuleConfig;    ///< Ponter to  IPELTM13 Module config register
    UINT32*               pDMIDataPtr;      ///< Pointer to the DMI table
}CAMX_PACKED;

CAMX_END_PACKED

/// @brief: This enumerator maps Look Up Tables indices with DMI LUT_SEL in LTM module SWI.
enum IPELTMLUTIndex
{
    LTMIndexWeight = (IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_WEIGHT_LUT - 1),  ///< Weight Curve
    LTMIndexLA0,                                                                  ///< LA0 Curve
    LTMIndexLA1,                                                                  ///< LA1 Curve
    LTMIndexCurve,                                                                ///< Curve
    LTMIndexScale,                                                                ///< Scale Curve
    LTMIndexMask,                                                                 ///< Mask Curve
    LTMIndexLCEPositive,                                                          ///< LCE positive Curve
    LTMIndexLCENegative,                                                          ///< LCE Negative Curve
    LTMIndexRGamma0,                                                              ///< Gamma0 Curve
    LTMIndexRGamma1,                                                              ///< Gamma1 Curve
    LTMIndexRGamma2,                                                              ///< Gamma2 Curve
    LTMIndexRGamma3,                                                              ///< Gamma3 Curve
    LTMIndexRGamma4,                                                              ///< Gamma4 Curve
    LTMIndexMax                                                                   ///< Max LUTs
};

/// @brief: This structure has information of number of entries of each LUT.
static const UINT IPELTMLUTNumEntries[LTMIndexMax] =
{
    12,     ///< Weight Curve
    64,     ///< LA0 Curve
    64,     ///< LA1 Curve
    64,     ///< Curve
    64,     ///< Scale Curve
    64,     ///< Mask Curve
    16,     ///< LCE positive Curve
    16,     ///< LCE Negative Curve
    64,     ///< Gamma0 Curve
    64,     ///< Gamma1 Curve
    64,     ///< Gamma2 Curve
    64,     ///< Gamma3 Curve
    64,     ///< Gamma4 Curve
};

static const UINT32 IPELTM13RegLengthDWord        = (sizeof(IPELTM13RegConfig) / sizeof(UINT32));
static const UINT32 IPELTM13LUTBufferSizeInDwords = 824; // Total Entries 824 (Sum of all entries from all LUT curves)

static const UINT32 LTMWeightLUT                  = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_WEIGHT_LUT;
static const UINT32 LTMLaLUT0                     = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_LA_LUT0;
static const UINT32 LTMLaLUT1                     = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_LA_LUT1;
static const UINT32 LTMCurveLUT                   = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_CURVE_LUT;
static const UINT32 LTMScaleLUT                   = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_SCALE_LUT;
static const UINT32 LTMMaskLUT                    = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_MASK_LUT;
static const UINT32 LTMLCEPositiveLUT             = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_LCE_POS_SCALE_LUT;
static const UINT32 LTMLCENegativeLUT             = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_LCE_NEG_SCALE_LUT;
static const UINT32 LTMRGammaLUT0                 = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_RGAMMA_LUT0;
static const UINT32 LTMRGammaLUT1                 = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_RGAMMA_LUT1;
static const UINT32 LTMRGammaLUT2                 = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_RGAMMA_LUT2;
static const UINT32 LTMRGammaLUT3                 = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_RGAMMA_LUT3;
static const UINT32 LTMRGammaLUT4                 = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_RGAMMA_LUT4;
static const UINT32 LTMAverageLUT                 = IPE_IPE_0_PPS_CLC_LTM_DMI_LUT_CFG_LUT_SEL_AVG_LUT;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Class for IPE LTM13 Module
///
///         This IQ block does mapping of wide dynamic range captured image to a more visible data range. Supports two separate
///         functions: Data Collection (statistic collection) during 1:4/1:16 passes and tri-linear image interpolation during
///         1:1 pass.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPELTM13 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create Local Tone Map (LTM) Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for LTM13 Creation
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IPEModuleCreateData* pCreateData);

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
    /// ~IPELTM13
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPELTM13();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPELTM13
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    /// @param  pCreateData         Initialization data for IPEICA
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPELTM13(
        const CHAR*          pNodeIdentifier,
        IPEModuleCreateData* pCreateData);

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
    /// @return BOOL Indicates if the settings have changed compared to last setting
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckDependenceChange(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteLUTInDMI
    ///
    /// @brief  writes all 14 LUTs from LTM into cmd buffer
    ///
    /// @param  pInputData Pointer to IPE module input data
    ///
    /// @return CamxResult Indicates if LUTs are successfully written into DMI cdm header
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteLUTInDMI(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FetchDMIBuffer
    ///
    /// @brief  Fetch DMI buffer
    ///
    /// @return CamxResult Indicates if fetch DMI was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FetchDMIBuffer();

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
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of LTM module for debug
    ///
    /// @param  pDMIDataPtr Pointer to the DMI data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig(
        UINT32* pDMIDataPtr
        ) const;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetLTMEnableStatus
    ///
    /// @brief  Helper method to check condition to enable/disable LTM module and return the status
    ///
    /// @param  pInputData       pointer to ISP Input data
    /// @param  pIPEIQSettings   pointer to IPE IQ settings
    ///
    /// @return TRUE if LTM IQ module is enabled, otherwise FALSE
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL GetLTMEnableStatus(
        const ISPInputData*  pInputData,
        IpeIQSettings* pIPEIQSettings) const
    {
        BOOL bEnableLTM = FALSE;

        if ((TRUE == pIPEIQSettings->anrParameters.parameters[PASS_NAME_DC_4].moduleCfg.EN) ||
            (IpeTopologyType::TOPOLOGY_TYPE_NO_NPS_LTM == pInputData->pipelineIPEData.configIOTopology))
        {
            bEnableLTM = m_moduleEnable;
        }
        else
        {
            bEnableLTM = 0;
        }

        return bEnableLTM;
    }

    IPELTM13(const IPELTM13&)            = delete;          ///< Disallow the copy constructor
    IPELTM13& operator=(const IPELTM13&) = delete;          ///< Disallow assignment operator

    const CHAR*          m_pNodeIdentifier;                 ///< String identifier for the Node that created this object
    ADRCData*            m_pADRCData;                       ///< ADRC Data
    TMC10InputData       m_pTMCInput;                       ///< TMC Input data for ADRC
    LTM13InputData       m_dependenceData;                  ///< Dependence Data for this Module
    IPELTM13RegConfig    m_regCmd;                          ///< Register list for LTM module
    IPELTM13ModuleConfig m_moduleConfig;                    ///< Module configuration for IPELTM13
    CmdBufferManager*    m_pLUTCmdBufferManager;            ///< Command buffer manager for all LUTs of LTM
    CmdBuffer*           m_pLUTCmdBuffer;                   ///< Command buffer for holding all 3 LUTs
    UINT                 m_offsetLUTCmdBuffer[LTMIndexMax]; ///< Offset of all tables within LUT CmdBuffer
    BOOL                 m_ignoreChromatixRGammaFlag;       ///< TRUE to hardcode RGAMMA_EN to 0; FALSE to use Chromatix data
    BOOL                 m_useHardcodedGamma;               ///< TRUE to use hardcode Gamma; FALSE to use published Gamma
    INT32                m_gammaPrev[LTM_GAMMA_LUT_SIZE];   ///< Cached version of the InverseGamma() input array gamma
    INT32                m_igammaPrev[LTM_GAMMA_LUT_SIZE];  ///< Cached version of the InverseGamma() output array igamma

    UINT32*              m_pLTMLUTs;                        ///< Tuning LTM LUTs place holder
    ltm_1_3_0::chromatix_ltm13Type* m_pChromatix;           ///< Pointers to tuning mode data
    tmc_1_0_0::chromatix_tmc10Type* m_ptmcChromatix;        ///
};

CAMX_NAMESPACE_END

#endif // CAMXIPELTM13_H
