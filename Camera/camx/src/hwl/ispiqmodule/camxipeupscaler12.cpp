////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipeupscaler12.cpp
/// @brief CAMXIPEUpscaler class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxdefs.h"
#include "camxipeupscaler12.h"
#include "camxiqinterface.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"
#include "ipe_data.h"
#include "parametertuningtypes.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler12::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler12::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPEUpscaler12* pModule = CAMX_NEW IPEUpscaler12(pCreateData->pNodeIdentifier);

        if (NULL == pModule)
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }

        pCreateData->pModule = pModule;
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler12::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEUpscaler12::UpdateIPEInternalData(
    const ISPInputData* pInputData
    ) const
{
    IpeIQSettings*            pIPEIQSettings     = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);
    UpscalerLiteParameters*   pUpscalerLiteParam = &pIPEIQSettings->upscalerLiteParameters;

    pUpscalerLiteParam->lumaVScaleFirAlgorithm      = m_regCmdUpscaleLite.lumaScaleCfg0.bitfields.V_SCALE_FIR_ALGORITHM;
    pUpscalerLiteParam->lumaHScaleFirAlgorithm      = m_regCmdUpscaleLite.lumaScaleCfg0.bitfields.H_SCALE_FIR_ALGORITHM;
    pUpscalerLiteParam->lumaInputDitheringDisable   = m_regCmdUpscaleLite.lumaScaleCfg1.bitfields.INPUT_DITHERING_DISABLE;
    pUpscalerLiteParam->lumaInputDitheringMode      = m_regCmdUpscaleLite.lumaScaleCfg1.bitfields.INPUT_DITHERING_MODE;
    pUpscalerLiteParam->chromaVScaleFirAlgorithm    = m_regCmdUpscaleLite.chromaScaleCfg0.bitfields.V_SCALE_FIR_ALGORITHM;
    pUpscalerLiteParam->chromaHScaleFirAlgorithm    = m_regCmdUpscaleLite.chromaScaleCfg0.bitfields.H_SCALE_FIR_ALGORITHM;
    pUpscalerLiteParam->chromaInputDitheringDisable = m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.INPUT_DITHERING_DISABLE;
    pUpscalerLiteParam->chromaInputDitheringMode    = m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.INPUT_DITHERING_MODE;
    pUpscalerLiteParam->chromaRoundingModeV         = m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.OUTPUT_ROUNDING_MODE_V;
    pUpscalerLiteParam->chromaRoundingModeH         = m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.OUTPUT_ROUNDING_MODE_H;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler12::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler12::CreateCmdList(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    CAMX_UNREFERENCED_PARAM(pInputData);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler12::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPEUpscaler12::CheckDependenceChange(
    const ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                  &&
        (NULL != pInputData->pHwContext))
    {
        CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", pInputData->pAECUpdateData->luxIndex);

        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->US12Enable;
            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            m_moduleEnable                               = TRUE;
            isChanged                                    = (TRUE == m_moduleEnable);
            m_dependenceData.lumaVScaleFirAlgorithm      = UpscaleBiCubicInterpolation;
            m_dependenceData.lumaHScaleFirAlgorithm      = UpscaleBiCubicInterpolation;
            m_dependenceData.lumaInputDitheringDisable   = UpscaleDitheringDisable;
            m_dependenceData.lumaInputDitheringMode      = UpscaleRegularRound;
            m_dependenceData.chromaVScaleFirAlgorithm    = UpscaleBiCubicInterpolation;
            m_dependenceData.chromaHScaleFirAlgorithm    = UpscaleBiCubicInterpolation;
            m_dependenceData.chromaInputDitheringDisable = UpscaleDitheringDisable;
            m_dependenceData.chromaInputDitheringMode    = UpscaleCheckerBoardABBA;
            m_dependenceData.chromaRoundingModeV         = UpscaleCheckerBoardABBA;
            m_dependenceData.chromaRoundingModeH         = UpscaleCheckerBoardABBA;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Null Input Pointer");
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler12::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler12::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Lux Index [0x%f] ", pInputData->pAECUpdateData->luxIndex);

    if (CamxResultSuccess == result)
    {
        Upscale12OutputData outputData;

        outputData.pRegCmdUpscale = &m_regCmdUpscaleLite;

        result = IQInterface::IPEUpscale12CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);
        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Operation failed %d", result);
        }

        if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
        {
            DumpRegConfig();
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler12::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPEUpscaler12::DumpRegConfig() const
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg0.bitfields.HSCALE_ENABLE [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg0.bitfields.HSCALE_ENABLE);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg0.bitfields.H_SCALE_FIR_ALGORITHM [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg0.bitfields.H_SCALE_FIR_ALGORITHM);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg0.bitfields.VSCALE_ENABLE [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg0.bitfields.VSCALE_ENABLE);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg0.bitfields.V_SCALE_FIR_ALGORITHM [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg0.bitfields.V_SCALE_FIR_ALGORITHM);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg1.bitfields.INPUT_DITHERING_DISABLE [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.INPUT_DITHERING_DISABLE);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg1.bitfields.INPUT_DITHERING_MODE [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.INPUT_DITHERING_MODE);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg1.bitfields.OUTPUT_ROUNDING_MODE_H [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.OUTPUT_ROUNDING_MODE_H);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg1.bitfields.OUTPUT_ROUNDING_MODE_V [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.OUTPUT_ROUNDING_MODE_V);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg1.bitfields.H_PIXEL_OFFSET [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.H_PIXEL_OFFSET);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleCfg1.bitfields.V_PIXEL_OFFSET [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleCfg1.bitfields.V_PIXEL_OFFSET);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleHorizontalInitialPhase.bitfields.PHASE_INIT [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleHorizontalInitialPhase.bitfields.PHASE_INIT);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleHorizontalStepPhase.bitfields.PHASE_STEP [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleHorizontalStepPhase.bitfields.PHASE_STEP);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleOutputCfg.bitfields.BLOCK_HEIGHT [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleOutputCfg.bitfields.BLOCK_HEIGHT);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleOutputCfg.bitfields.BLOCK_WIDTH [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleOutputCfg.bitfields.BLOCK_WIDTH);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleVerticalInitialPhase.bitfields.PHASE_INIT [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleVerticalInitialPhase.bitfields.PHASE_INIT);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "chromaScaleVerticalStepPhase.bitfields.PHASE_STEP [0x%x] ",
        m_regCmdUpscaleLite.chromaScaleVerticalStepPhase.bitfields.PHASE_STEP);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleCfg0.bitfields.HSCALE_ENABLE [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleCfg0.bitfields.HSCALE_ENABLE);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleCfg0.bitfields.H_SCALE_FIR_ALGORITHM [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleCfg0.bitfields.H_SCALE_FIR_ALGORITHM);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleCfg0.bitfields.VSCALE_ENABLE [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleCfg0.bitfields.VSCALE_ENABLE);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleCfg0.bitfields.V_SCALE_FIR_ALGORITHM [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleCfg0.bitfields.V_SCALE_FIR_ALGORITHM);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleCfg1.bitfields.INPUT_DITHERING_DISABLE [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleCfg1.bitfields.INPUT_DITHERING_DISABLE);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleCfg1.bitfields.INPUT_DITHERING_MODE [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleCfg1.bitfields.INPUT_DITHERING_MODE);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleCfg1.bitfields.H_PIXEL_OFFSET [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleCfg1.bitfields.H_PIXEL_OFFSET);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleCfg1.bitfields.V_PIXEL_OFFSET [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleCfg1.bitfields.V_PIXEL_OFFSET);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleHorizontalInitialPhase.bitfields.PHASE_INIT [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleHorizontalInitialPhase.bitfields.PHASE_INIT);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleHorizontalStepPhase.bitfields.PHASE_STEP [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleHorizontalStepPhase.bitfields.PHASE_STEP);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleOutputCfg.bitfields.BLOCK_HEIGHT [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleOutputCfg.bitfields.BLOCK_HEIGHT);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleOutputCfg.bitfields.BLOCK_WIDTH [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleOutputCfg.bitfields.BLOCK_WIDTH);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleVerticalInitialPhase.bitfields.PHASE_INIT [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleVerticalInitialPhase.bitfields.PHASE_INIT);
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "lumaScaleVerticalStepPhase.bitfields.PHASE_STEP [0x%x] ",
        m_regCmdUpscaleLite.lumaScaleVerticalStepPhase.bitfields.PHASE_STEP);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler12::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPEUpscaler12::Execute(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        if (TRUE == m_moduleEnable)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);
            }

            // Regardless of any update in dependency parameters, command buffers and IQSettings/Metadata shall be updated.
            if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
            {
                result = CreateCmdList(pInputData);
            }

            if (CamxResultSuccess == result)
            {
                UpdateIPEInternalData(pInputData);
            }
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Null Input Pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPEUpscaler12::IPEUpscaler12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEUpscaler12::IPEUpscaler12(
    const CHAR* pNodeIdentifier)
{
    m_pNodeIdentifier   = pNodeIdentifier;
    m_type              = ISPIQModuleType::IPEUpscaler;
    m_moduleEnable      = TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPEUpscaler12::~IPEUpscaler12
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPEUpscaler12::~IPEUpscaler12()
{

}

CAMX_NAMESPACE_END
