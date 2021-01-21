////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxbpslinearization34.h
/// @brief bpslinearization34 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CAMXBPSLINEARIZATION34_H
#define CAMXBPSLINEARIZATION34_H

#include "camxispiqmodule.h"
#include "iqcommondefs.h"
#include "linearization_3_4_0.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED

/// @brief Linearization register Configuration
struct BPSLinearization34RegConfig
{
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_R_0_CFG   kneepointR0Config;  ///< Linearization kneepoint R 0 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_R_1_CFG   kneepointR1Config;  ///< Linearization kneepoint R 1 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_R_2_CFG   kneepointR2Config;  ///< Linearization kneepoint R 2 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_R_3_CFG   kneepointR3Config;  ///< Linearization kneepoint R 3 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_GR_0_CFG  kneepointGR0Config; ///< Linearization kneepoint GR 0 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_GR_1_CFG  kneepointGR1Config; ///< Linearization kneepoint GR 1 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_GR_2_CFG  kneepointGR2Config; ///< Linearization kneepoint GR 2 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_GR_3_CFG  kneepointGR3Config; ///< Linearization kneepoint GR 3 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_B_0_CFG   kneepointB0Config;  ///< Linearization kneepoint B 0 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_B_1_CFG   kneepointB1Config;  ///< Linearization kneepoint B 1 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_B_2_CFG   kneepointB2Config;  ///< Linearization kneepoint B 2 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_B_3_CFG   kneepointB3Config;  ///< Linearization kneepoint B 3 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_GB_0_CFG  kneepointGB0Config; ///< Linearization kneepoint GB 0 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_GB_1_CFG  kneepointGB1Config; ///< Linearization kneepoint GB 1 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_GB_2_CFG  kneepointGB2Config; ///< Linearization kneepoint GB 2 Config
    BPS_BPS_0_CLC_LINEARIZATION_KNEEPOINT_GB_3_CFG  kneepointGB3Config; ///< Linearization kneepoint GB 3 Config
} CAMX_PACKED;

CAMX_END_PACKED

/// @brief Output Data to Linearization33 IQ Algorithem
struct Linearization34OutputData
{
    BPSLinearization34RegConfig* pRegCmd;               ///< Pointer to the Linearization Register buffer
    UINT32*                      pDMIDataPtr;           ///< Pointer to the DMI table
    FLOAT                        stretchGainRed;        ///< Stretch Gain Red
    FLOAT                        stretchGainGreenEven;  ///< Stretch Gain Green Even
    FLOAT                        stretchGainGreenOdd;   ///< Stretch Gain Green Odd
    FLOAT                        stretchGainBlue;       ///< Stretch Gain Blue
};

static const UINT32 LinearizationLUT                 = BPS_BPS_0_CLC_LINEARIZATION_DMI_LUT_CFG_LUT_SEL_LUT;
static const UINT32 LinearizationLUTBank0            = BPS_BPS_0_CLC_LINEARIZATION_DMI_LUT_BANK_CFG_BANK_SEL_LUT_BANK0;
static const UINT32 LinearizationLUTBank1            = BPS_BPS_0_CLC_LINEARIZATION_DMI_LUT_BANK_CFG_BANK_SEL_LUT_BANK1;

static const UINT32 LinearizationMaxLUT              = 1;
static const UINT32 BPSLinearization34RegLengthDWord = sizeof(BPSLinearization34RegConfig) / sizeof(UINT32);
static const UINT32 BPSLinearization34LUTEntries     = 36;    ///< 9 slopes for each of the 4 channels (R/RG/BG/B)
static const UINT32 BPSLinearization34LUTLengthDword = (BPSLinearization34LUTEntries);
static const UINT32 BPSLinearization34LUTLengthBytes = BPSLinearization34LUTLengthDword * sizeof(UINT32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief BPS Linearization 34 Class Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class BPSLinearization34 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create BPSLinearization34 Object
    ///
    /// @param  pCreateData Pointer to the BPSLinearization34 Creation
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
    /// ~BPSLinearization34
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~BPSLinearization34();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSLinearization34
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit BPSLinearization34(
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
    /// @return TRUE if dependence is meet
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckDependenceChange(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculation
    ///
    /// @brief   Calculate the Register Value
    ///
    /// @param   pInputData Pointer to the Linearization Module Input data
    ///
    /// @return  CamxResult Indicates if configure DMI and Registers was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FetchDMIBuffer
    ///
    /// @brief  Fetch DMI buffer
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if fetch DMI was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FetchDMIBuffer(
        const ISPInputData* pInputData);

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
    /// @brief  Print register configuration of Linearization module for debug
    ///
    /// @param  pOutputData Pointer to the ISP input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig(
        const Linearization34OutputData* pOutputData
        ) const;

    BPSLinearization34(const BPSLinearization34&)            = delete; ///< Disallow the copy constructor
    BPSLinearization34& operator=(const BPSLinearization34&) = delete; ///< Disallow assignment operator

    const CHAR*                 m_pNodeIdentifier;                     ///< String identifier of this Node
    Linearization34IQInput      m_dependenceData;                      ///< Dependence/Input Data for this Module
    BPSLinearization34RegConfig m_regCmd;                              ///< Register list for Linearization module
    CmdBufferManager*           m_pLUTCmdBufferManager;                ///< Command buffer manager for all LUTs of Linearization
    CmdBuffer*                  m_pLUTDMICmdBuffer;                    ///< Command buffer for holding all LUTs

    UINT32*                     m_pLUTDMIBuffer;                       ///< Pointer to linearization LUT
    UINT8                       m_blacklevelLock;                      ///< manual update Black Level lock
    UINT32                      m_dynamicBlackLevel[4];                ///< Previous Dynamic Black Level
    linearization_3_4_0::chromatix_linearization34Type* m_pChromatix;  ///< Pointer to tuning mode data
    BOOL                        m_AWBLock;                             ///< Flag to track AWB state
};

CAMX_NAMESPACE_END

#endif // CAMXBPSLINEARIZATION34_H
