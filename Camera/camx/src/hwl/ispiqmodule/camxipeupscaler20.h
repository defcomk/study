////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipeupscaler20.h
/// @brief IPEUpscaler class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPEUPSCALER20_H
#define CAMXIPEUPSCALER20_H

#include "camxispiqmodule.h"
#include "iqcommondefs.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

struct IPEUpscaleRegCmd
{
    IPE_IPE_0_PPS_CLC_UPSCALE_OP_MODE           opMode;                       ///< op mode register
    IPE_IPE_0_PPS_CLC_UPSCALE_PRELOAD           preload;                      ///< preload register
    IPE_IPE_0_PPS_CLC_UPSCALE_PHASE_STEP_Y_H    horizontalPhaseStepY;         ///< Y channel horizontal phase step
    IPE_IPE_0_PPS_CLC_UPSCALE_PHASE_STEP_Y_V    verticalPhaseStepY;           ///< Y channel vertical phase step
    IPE_IPE_0_PPS_CLC_UPSCALE_PHASE_STEP_UV_H   horizontalPhaseStepUV;        ///< UV channel horizontal phase step
    IPE_IPE_0_PPS_CLC_UPSCALE_PHASE_STEP_UV_V   verticalPhaseStepUV;          ///< UV channel vertical phase step
    IPE_IPE_0_PPS_CLC_UPSCALE_DE_SHARPEN        detailEnhancerSharpen;        ///< detail enhancer sharpen register
    IPE_IPE_0_PPS_CLC_UPSCALE_DE_SHARPEN_CTL    detailEnhancerSharpenControl; ///< detail enhancer sharpen control
    IPE_IPE_0_PPS_CLC_UPSCALE_DE_SHAPE_CTL      detailEnhancerShapeControl;   ///< detail enhancer shape control
    IPE_IPE_0_PPS_CLC_UPSCALE_DE_THRESHOLD      detailEnhancerThreshold;      ///< detail enhancer threshold
    IPE_IPE_0_PPS_CLC_UPSCALE_DE_ADJUST_DATA_0  detailEnhancerAdjustData0;    ///< detail enhancer adjust data 0
    IPE_IPE_0_PPS_CLC_UPSCALE_DE_ADJUST_DATA_1  detailEnhancerAdjustData1;    ///< detail enhancer adjust data 1
    IPE_IPE_0_PPS_CLC_UPSCALE_DE_ADJUST_DATA_2  detailEnhancerAdjustData2;    ///< detail enhancer adjust data 2
    IPE_IPE_0_PPS_CLC_UPSCALE_SRC_SIZE          sourceSize;                   ///< input before upscaler size
    IPE_IPE_0_PPS_CLC_UPSCALE_DST_SIZE          destinationSize;              ///< output after upscaler size
    IPE_IPE_0_PPS_CLC_UPSCALE_Y_PHASE_INIT_H    horizontalInitialPhaseY;      ///< Y channel horizontal initial phase
    IPE_IPE_0_PPS_CLC_UPSCALE_Y_PHASE_INIT_V    verticalInitialPhaseY;        ///< Y channel vertical initial phase
    IPE_IPE_0_PPS_CLC_UPSCALE_UV_PHASE_INIT_H   horizontalInitialPhaseUV;     ///< UV channel horizontal initial phase
    IPE_IPE_0_PPS_CLC_UPSCALE_UV_PHASE_INIT_V   verticalInitialPhaseUV;       ///< UV channel vertical initial phase
} CAMX_PACKED;

CAMX_END_PACKED

/// @brief DMI select index
enum UpscaleDMILUTSelIndex
{
    NO_LUT = 0x0,      ///< No LUT
    DMI_LUT_A,         ///< LUT A
    DMI_LUT_B,         ///< LUT B
    DMI_LUT_C,         ///< LUT C
    DMI_LUT_D,         ///< LUT D
    MAX_UPSCALE_LUT,   ///< MAX LUT
};

/// @brief DMI table number of entries
static const UINT IPEUpscaleLUTNumEntries[MAX_UPSCALE_LUT] =
{
    0,     ///< NO_LUT
    168,   ///< DMI_LUT_A
    96,    ///< DMI_LUT_B
    96,    ///< DMI_LUT_C
    80     ///< DMI_LUT_D
};

/// @brief DMI table size
static const UINT IPEUpscaleLUTSizes[MAX_UPSCALE_LUT] =
{
    0,                     ///< NO_LUT
    168 * sizeof(UINT32),  ///< DMI_LUT_A
    96 * sizeof(UINT32),   ///< DMI_LUT_B
    96 * sizeof(UINT32),   ///< DMI_LUT_C
    80 * sizeof(UINT32)    ///< DMI_LUT_D
};

static const UINT32 MaxLUTNum = 4;   ///< Maximum number of LUTs
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IPE Upscaler20 Module
///
/// This IQ block adds upscaler operations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPEUpscaler20 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create Upscaler Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for Upscaler20 Creation
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
    /// ~IPEUpscaler20
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPEUpscaler20();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEUpscaler20
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPEUpscaler20(
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
        const ISPInputData* pInputData);

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
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of Upscale module for debug
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig() const;

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
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteLUTtoDMI(
        const ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateIPEInternalData
    ///
    /// @brief  Update Pipeline input data, such as metadata, IQSettings.
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateIPEInternalData(
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

    IPEUpscaler20(const IPEUpscaler20&)            = delete;         ///< Disallow the copy constructor
    IPEUpscaler20& operator=(const IPEUpscaler20&) = delete;         ///< Disallow assignment operator

    const CHAR*                             m_pNodeIdentifier;       ///< String identifier for the Node that created this
    IPEUpscaleRegCmd                        m_regCmdUpscale;         ///< Register List of Upscale20 Module
    Upscale20InputData                      m_dependenceData;        ///< Dependence Data for this Module
    CmdBufferManager*                       m_pLUTCmdBufferManager;  ///< Command buffer manager for all LUTs of Upscaler
    CmdBuffer*                              m_pLUTCmdBuffer;         ///< Command buffer for holding all 4 LUTs

    UINT32*                                 m_pUpscalerLUTs;         ///< Tuning data LUTs holder
    upscale_2_0_0::chromatix_upscale20Type* m_pChromatix;            ///< Pointer to tuning mode data
};

CAMX_NAMESPACE_END

#endif // CAMXIPEUPSCALER20_H