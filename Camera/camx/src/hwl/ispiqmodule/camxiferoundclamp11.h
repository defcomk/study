////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 - 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxiferoundclamp11.h
/// @brief camxiferoundclamp11 class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIFEROUNDCLAMP11_H
#define CAMXIFEROUNDCLAMP11_H

#include "camxispiqmodule.h"

CAMX_NAMESPACE_BEGIN

CAMX_BEGIN_PACKED
struct IFERoundClamp11FullLumaReg
{
    IFE_IFE_0_VFE_FULL_OUT_Y_CH0_CLAMP_CFG       clamp;    ///< Full output path Luma Clamp config
    IFE_IFE_0_VFE_FULL_OUT_Y_CH0_ROUNDING_CFG    round;    ///< Full output path Luma Roound config
} CAMX_PACKED;

struct IFERoundClamp11FullChromaReg
{
    IFE_IFE_0_VFE_FULL_OUT_C_CH0_CLAMP_CFG       clamp;    ///< Full output path Chroma Clamp config
    IFE_IFE_0_VFE_FULL_OUT_C_CH0_ROUNDING_CFG    round;    ///< Full output path Chroma Roound config
} CAMX_PACKED;

struct IFERoundClamp11FDLumaReg
{
    IFE_IFE_0_VFE_FD_OUT_Y_CH0_CLAMP_CFG         clamp;    ///< FD output path Luma Clamp config
    IFE_IFE_0_VFE_FD_OUT_Y_CH0_ROUNDING_CFG      round;    ///< FD output path Luma Roound config
} CAMX_PACKED;

struct IFERoundClamp11FDChromaReg
{
    IFE_IFE_0_VFE_FD_OUT_C_CH0_CLAMP_CFG         clamp;    ///< FD output path Chroma Clamp config
    IFE_IFE_0_VFE_FD_OUT_C_CH0_ROUNDING_CFG      round;    ///< FD output path Chroma Roound config
} CAMX_PACKED;

struct IFERoundClamp11DS4LumaReg
{
    IFE_IFE_0_VFE_DS4_Y_CH0_CLAMP_CFG            clamp;    ///< DS4 output path Luma Clamp config
    IFE_IFE_0_VFE_DS4_Y_CH0_ROUNDING_CFG         round;    ///< DS4 output path Luma Roound config
} CAMX_PACKED;

struct IFERoundClamp11DS4ChromaReg
{
    IFE_IFE_0_VFE_DS4_C_CH0_CLAMP_CFG            clamp;    ///< DS4 output path Chroma Clamp config
    IFE_IFE_0_VFE_DS4_C_CH0_ROUNDING_CFG         round;    ///< DS4 output path Chroma Roound config
} CAMX_PACKED;

struct IFERoundClamp11DS16LumaReg
{
    IFE_IFE_0_VFE_DS16_Y_CH0_CLAMP_CFG           clamp;    ///< DS16 output path Luma Clamp config
    IFE_IFE_0_VFE_DS16_Y_CH0_ROUNDING_CFG        round;    ///< DS16 output path Luma Roound config
} CAMX_PACKED;

struct IFERoundClamp11DS16ChromaReg
{
    IFE_IFE_0_VFE_DS16_C_CH0_CLAMP_CFG           clamp;    ///< DS16 output path Chroma Clamp config
    IFE_IFE_0_VFE_DS16_C_CH0_ROUNDING_CFG        round;    ///< DS16 output path Chroma Roound config
} CAMX_PACKED;

struct IFERoundClamp11DisplayLumaReg
{
    IFE_IFE_0_VFE_DISP_OUT_Y_CH0_CLAMP_CFG       clamp;    ///< Display output path Luma Clamp config
    IFE_IFE_0_VFE_DISP_OUT_Y_CH0_ROUNDING_CFG    round;    ///< Display output path Luma Roound config
} CAMX_PACKED;

struct IFERoundClamp11DisplayChromaReg
{
    IFE_IFE_0_VFE_DISP_OUT_C_CH0_CLAMP_CFG       clamp;    ///< Display output path Chroma Clamp config
    IFE_IFE_0_VFE_DISP_OUT_C_CH0_ROUNDING_CFG    round;    ///< Display output path Chroma Roound config
} CAMX_PACKED;

struct IFERoundClamp11DisplayDS4LumaReg
{
    IFE_IFE_0_VFE_DISP_DS4_Y_CH0_CLAMP_CFG       clamp;    ///< DS4 Display output path Luma Clamp config
    IFE_IFE_0_VFE_DISP_DS4_Y_CH0_ROUNDING_CFG    round;    ///< DS4 Display output path Luma Roound config
} CAMX_PACKED;

struct IFERoundClamp11DisplayDS4ChromaReg
{
    IFE_IFE_0_VFE_DISP_DS4_C_CH0_CLAMP_CFG       clamp;    ///< DS4 Display output path Chroma Clamp config
    IFE_IFE_0_VFE_DISP_DS4_C_CH0_ROUNDING_CFG    round;    ///< DS4 Display output path Chroma Roound config
} CAMX_PACKED;

struct IFERoundClamp11displayDS16LumaReg
{
    IFE_IFE_0_VFE_DISP_DS16_Y_CH0_CLAMP_CFG      clamp;    ///< DS16 Display output path Luma Clamp config
    IFE_IFE_0_VFE_DISP_DS16_Y_CH0_ROUNDING_CFG   round;    ///< DS16 Display output path Luma Roound config
} CAMX_PACKED;

struct IFERoundClamp11displayDS16ChromaReg
{
    IFE_IFE_0_VFE_DISP_DS16_C_CH0_CLAMP_CFG      clamp;    ///< DS16 Display output path Chroma Clamp config
    IFE_IFE_0_VFE_DISP_DS16_C_CH0_ROUNDING_CFG   round;    ///< DS16 Display output path Chroma Roound config
} CAMX_PACKED;

CAMX_END_PACKED

struct IFERoundClamp11RegCmd
{
    IFERoundClamp11FullLumaReg                   fullLuma;          ///< FD path Luma config
    IFERoundClamp11FullChromaReg                 fullChroma;        ///< FD path Chroma config
    IFERoundClamp11FDLumaReg                     FDLuma;            ///< Full path Luma config
    IFERoundClamp11FDChromaReg                   FDChroma;          ///< Full path Luma config
    IFERoundClamp11DS4LumaReg                    DS4Luma;           ///< DS4 path Luma config
    IFERoundClamp11DS4ChromaReg                  DS4Chroma;         ///< DS4 path Luma config
    IFERoundClamp11DS16LumaReg                   DS16Luma;          ///< DS16 path Luma config
    IFERoundClamp11DS16ChromaReg                 DS16Chroma;        ///< DS16 path Luma config
    IFERoundClamp11DisplayLumaReg                displayLuma;       ///< Disp path Luma config
    IFERoundClamp11DisplayChromaReg              displayChroma;     ///< Disp path Chroma config
    IFERoundClamp11DisplayDS4LumaReg             displayDS4Luma;    ///< DS4 Disp path Luma config
    IFERoundClamp11DisplayDS4ChromaReg           displayDS4Chroma;  ///< DS4 Disp path Luma config
    IFERoundClamp11displayDS16LumaReg            displayDS16Luma;   ///< DS16 Disp path Luma config
    IFERoundClamp11displayDS16ChromaReg          displayDS16Chroma; ///< DS16 Disp path Luma config
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IFE CROP10 Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IFERoundClamp11 final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create RoundClamp11 Object
    ///
    /// @param  pCreateData  Pointer to data for RoundClamp object Creation
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
    /// ~IFERoundClamp11
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IFERoundClamp11();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFERoundClamp11
    ///
    /// @brief  Constructor
    ///
    /// @param  pCreateData Pointer to resource and intialization data for Round and Clamp Creation
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IFERoundClamp11(
        IFEModuleCreateData* pCreateData);

private:

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
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureRegisters
    ///
    /// @brief  Configure RoundClamp11 registers
    ///
    /// @param  pInputData Pointer to the ISP input data
    /// @param  path       RoundClamp output path type
    ///
    /// @return CamxResult Success if all configuration done, else error code
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ConfigureRegisters(
        ISPInputData* pInputData,
        IFEPath       path);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureFDPathRegisters
    ///
    /// @brief  Configure RoundClamp11 FD output path registers
    ///
    /// @param  minValue   Minimum threshold to clamp
    /// @param  maxValue   Maximum threshold to clamp
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureFDPathRegisters(
        UINT32 minValue,
        UINT32 maxValue);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureFullPathRegisters
    ///
    /// @brief  Configure RoundClamp11 Full output path registers
    ///
    /// @param  minValue   Minimum threshold to clamp
    /// @param  maxValue   Maximum threshold to clamp
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureFullPathRegisters(
        UINT32 minValue,
        UINT32 maxValue);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDS4PathRegisters
    ///
    /// @brief  Configure RoundClamp11 DS4 output path registers
    ///
    /// @param  minValue   Minimum threshold to clamp
    /// @param  maxValue   Maximum threshold to clamp
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDS4PathRegisters(
        UINT32 minValue,
        UINT32 maxValue);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDS16PathRegisters
    ///
    /// @brief  Configure RoundClamp11 DS16 output path registers
    ///
    /// @param  minValue   Minimum threshold to clamp
    /// @param  maxValue   Maximum threshold to clamp
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDS16PathRegisters(
        UINT32 minValue,
        UINT32 maxValue);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDisplayPathRegisters
    ///
    /// @brief  Configure RoundClamp11 Display output path registers
    ///
    /// @param  minValue   Minimum threshold to clamp
    /// @param  maxValue   Maximum threshold to clamp
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDisplayPathRegisters(
        UINT32 minValue,
        UINT32 maxValue);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDS4DisplayPathRegisters
    ///
    /// @brief  Configure RoundClamp11 DS4 Display output path registers
    ///
    /// @param  minValue   Minimum threshold to clamp
    /// @param  maxValue   Maximum threshold to clamp
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDS4DisplayPathRegisters(
        UINT32 minValue,
        UINT32 maxValue);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConfigureDS16DisplayPathRegisters
    ///
    /// @brief  Configure RoundClamp11 DS16 Display output path registers
    ///
    /// @param  minValue   Minimum threshold to clamp
    /// @param  maxValue   Maximum threshold to clamp
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID ConfigureDS16DisplayPathRegisters(
        UINT32 minValue,
        UINT32 maxValue);

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
    /// @param  pInputData Pointer to the ISP input data1
    ///
    /// @return CamxResult Success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpRegConfig
    ///
    /// @brief  Print register configuration of RoundClamp module for debug
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DumpRegConfig();

    IFERoundClamp11(const IFERoundClamp11&)            = delete;    ///< Disallow the copy constructor
    IFERoundClamp11& operator=(const IFERoundClamp11&) = delete;    ///< Disallow assignment operator

    IFERoundClamp11RegCmd    m_regCmd;                              ///< Register List of this Module
    RoundClampState*         m_pState;                              ///< Pointer to state
    Format                   m_pixelFormat;                         ///< Output pixel format
    UINT32                   m_bitWidth;                            ///< ISP output bit width based on ISP output format
    IFEPipelinePath          m_modulePath;                          ///< IFE pipeline path for module
    UINT32                   m_output;                              ///< IPE piepline output
};
CAMX_NAMESPACE_END

#endif // CAMXIFEROUNDCLAMP11_H
