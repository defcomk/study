////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipetf10.cpp
/// @brief CAMXIPETF10 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxcdmdefs.h"
#include "camxdefs.h"
#include "camxtuningdatamanager.h"
#include "parametertuningtypes.h"
#include "camxiqinterface.h"
#include "camxispiqmodule.h"
#include "camxnode.h"
#include "ipe_data.h"
#include "camxipetf10.h"
#include "camxtitan17xcontext.h"
#include "NcLibContext.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 IPETFRegCmdLength = sizeof(IPETFRegCmd) / 4;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::Create(
    IPEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pCreateData) && (NULL != pCreateData->pNodeIdentifier))
    {
        IPETF10* pModule = CAMX_NEW IPETF10(pCreateData->pNodeIdentifier, pCreateData);

        if (NULL != pModule)
        {
            result = pModule->Initialize(&pCreateData->initializationData);
            if (CamxResultSuccess != result)
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Module initialization failed !!");
                pModule->Destroy();
                pModule = NULL;
            }
        }
        else
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
// IPETF10::Initialize
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::Initialize(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;
    CAMX_UNREFERENCED_PARAM(pInputData);

    result = AllocateCommonLibraryData();

    CAMX_ASSERT(CamxResultSuccess == result);
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::CreateCmdList(
    const ISPInputData* pInputData)
{
    /// @todo (CAMX-729) Implement IQ block and hook up with Common library
    /// @todo (CAMX-735) Link IQ module with Chromatix adapter
    CamxResult result = CamxResultSuccess;

    if (NULL != pInputData)
    {
        CmdBuffer* pCmdBuffer = pInputData->pipelineIPEData.ppIPECmdBuffer[CmdBufferNPS];

        if (NULL != pCmdBuffer)
        {
            for (UINT passNumber = PASS_NAME_FULL; passNumber <= PASS_NAME_DC_64; passNumber++)
            {
                m_offsetPass[passNumber] = (pCmdBuffer->GetResourceUsedDwords() * RegisterWidthInBytes);

                result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                      regIPE_IPE_0_NPS_CLC_TF_TF_CONFIG_0,
                                                      IPETFRegCmdLength,
                                                      reinterpret_cast<UINT32*>(&m_regCmd[passNumber].config0));

                if (CamxResultSuccess != result)
                {
                    CAMX_ASSERT_ALWAYS_MESSAGE("Failed to write command buffer");
                }
            }
        }
        else
        {
            result = CamxResultEInvalidArg;
            CAMX_ASSERT_ALWAYS_MESSAGE("Invalid command buffer");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupPProc, "Invalid input data");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::ValidateDependenceParams(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    /// @todo (CAMX-730) validate dependency parameters
    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "IQSettingsPtr [0x%p] ", pInputData->pipelineIPEData.pIPEIQSettings);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IPETF10::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPETF10::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL isChanged = FALSE;

    if ((NULL != pInputData)                 &&
        (NULL != pInputData->pAECUpdateData) &&
        (NULL != pInputData->pAWBUpdateData) &&
        (NULL != pInputData->pHwContext)     &&
        (NULL != pInputData->pHALTagsData))
    {
        if (NULL != pInputData->pOEMIQSetting)
        {
            m_moduleEnable = (static_cast<OEMIPEIQSetting*>(pInputData->pOEMIQSetting))->TF10Enable;
            isChanged      = (TRUE == m_moduleEnable);
        }
        else
        {
            TuningDataManager* pTuningManager = pInputData->pTuningDataManager;
            CAMX_ASSERT(NULL != pTuningManager);

            // Search through the tuning data (tree), only when there
            // are changes to the tuning mode data as an optimization
            if ((TRUE == pInputData->tuningModeChanged)    &&
                (TRUE == pTuningManager->IsValidChromatix()))
            {
                CAMX_ASSERT(NULL != pInputData->pTuningData);

                m_pChromatix = pTuningManager->GetChromatix()->GetModule_tf10_ipe(
                                   reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                                   pInputData->pTuningData->noOfSelectionParameter);
            }

            CAMX_ASSERT(NULL != m_pChromatix);
            if (NULL != m_pChromatix)
            {
                if (m_pChromatix != m_dependenceData.pChromatix)
                {
                    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "updating chromatix pointer");
                    m_dependenceData.pChromatix = m_pChromatix;
                    m_moduleEnable              = m_pChromatix->enable_section.master_en;
                    isChanged                   = (TRUE == m_moduleEnable);
                }

                m_bypassMode = FALSE;
                m_dependenceData.bypassMode = FALSE;
                if ((NoiseReductionModeOff     == pInputData->pHALTagsData->noiseReductionMode) ||
                    (NoiseReductionModeMinimal == pInputData->pHALTagsData->noiseReductionMode) ||
                    ((FALSE == pInputData->pipelineIPEData.isLowResolution) &&
                    (NoiseReductionModeZeroShutterLag == pInputData->pHALTagsData->noiseReductionMode)))
                {
                    m_bypassMode                = TRUE;
                    m_dependenceData.bypassMode = TRUE;
                }

                CAMX_LOG_INFO(CamxLogGroupPProc, "NR mode %d, m_bypassMode %d isLowResolution %d",
                    pInputData->pHALTagsData->noiseReductionMode, m_bypassMode, pInputData->pipelineIPEData.isLowResolution);
            }
            else
            {
                CAMX_LOG_ERROR(CamxLogGroupPProc, "Failed to get chromatix pointer");
            }
        }

        // Check for trigger update status
        if ((TRUE == m_moduleEnable) &&
            (TRUE == IQInterface::s_interpolationTable.TF10TriggerUpdate(&pInputData->triggerData, &m_dependenceData)))
        {
            if (NULL == pInputData->pOEMIQSetting)
            {
                // Check for module dynamic enable trigger hysterisis
                m_moduleEnable = IQSettingUtils::GetDynamicEnableFlag(
                    m_dependenceData.pChromatix->dynamic_enable_triggers.master_en.enable,
                    m_dependenceData.pChromatix->dynamic_enable_triggers.master_en.hyst_control_var,
                    m_dependenceData.pChromatix->dynamic_enable_triggers.master_en.hyst_mode,
                    &(m_dependenceData.pChromatix->dynamic_enable_triggers.master_en.hyst_trigger),
                    static_cast<VOID*>(&pInputData->triggerData),
                    &m_dependenceData.moduleEnable);

                // Set the module status to avoid calling RunCalculation if it is disabled
                isChanged = (TRUE == m_moduleEnable);
            }
        }

        if (TRUE == m_moduleEnable)
        {
            if ((TRUE != CheckIPEInstanceProperty(pInputData))                                              ||
                (m_dependenceData.fullPassIcaOutputFrameHeight !=
                    pInputData->pipelineIPEData.inputDimension.heightLines)                                 ||
                (m_dependenceData.fullPassIcaOutputFrameWidth !=
                    pInputData->pipelineIPEData.inputDimension.widthPixels)                                 ||
                (m_dependenceData.numOfFrames != pInputData->pipelineIPEData.numOfFrames)                   ||
                (m_dependenceData.mfFrameNum != pInputData->mfFrameNum)                                     ||
                (m_dependenceData.hasTFRefInput != pInputData->pipelineIPEData.hasTFRefInput)               ||
                (m_dependenceData.upscalingFactorMFSR != pInputData->pipelineIPEData.upscalingFactorMFSR)   ||
                (m_dependenceData.isDigitalZoomEnabled != pInputData->pipelineIPEData.isDigitalZoomEnabled) ||
                (m_dependenceData.digitalZoomStartX != pInputData->pipelineIPEData.digitalZoomStartX)       ||
                (m_dependenceData.digitalZoomStartY != pInputData->pipelineIPEData.digitalZoomStartY))
            {
                m_dependenceData.fullPassIcaOutputFrameHeight = pInputData->pipelineIPEData.inputDimension.heightLines;
                m_dependenceData.fullPassIcaOutputFrameWidth  = pInputData->pipelineIPEData.inputDimension.widthPixels;
                m_dependenceData.mfFrameNum                   = pInputData->mfFrameNum;
                m_dependenceData.numOfFrames                  = pInputData->pipelineIPEData.numOfFrames;
                m_dependenceData.upscalingFactorMFSR          = pInputData->pipelineIPEData.upscalingFactorMFSR;
                m_dependenceData.isDigitalZoomEnabled         = pInputData->pipelineIPEData.isDigitalZoomEnabled;
                m_dependenceData.digitalZoomStartX            = pInputData->pipelineIPEData.digitalZoomStartX;
                m_dependenceData.digitalZoomStartY            = pInputData->pipelineIPEData.digitalZoomStartY;
                m_dependenceData.pWarpGeometriesOutput        = pInputData->pipelineIPEData.pWarpGeometryData;
                m_dependenceData.hasTFRefInput                = pInputData->pipelineIPEData.hasTFRefInput;
                isChanged = TRUE;
            }
            // get address of TF parameters and refinement parameter from firmware data
            m_dependenceData.pRefinementParameters = &m_refinementParams;
            m_dependenceData.pTFParameters         = &m_TFParams;
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupPProc, "TF10 Module is not enabled");
        }
    }
    else
    {
        if (NULL != pInputData)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc,
                           "Invalid Input: pNewAECUpdate %p pNewAWBUpdate %p pHwContext %p",
                           pInputData->pAECUpdateData,
                           pInputData->pAWBUpdateData,
                           pInputData->pHwContext);
        }
        else
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "Null Input Pointer");
        }
    }

    return isChanged;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::RunCalculation(
    const ISPInputData* pInputData)
{
    CamxResult     result     = CamxResultSuccess;
    TF10OutputData outputData = { NULL, 0 };

    CAMX_LOG_VERBOSE(CamxLogGroupPProc, "Number of Passes [0x%d] with MF Config %d",
                  m_dependenceData.maxUsedPasses,
                  pInputData->pipelineIPEData.instanceProperty.processingType);

    if (TRUE == m_enableCommonIQ)
    {
        outputData.pRegCmd = &m_regCmd[0];
        outputData.numPasses = m_dependenceData.maxUsedPasses;

        // running calculation
        result = IQInterface::IPETF10CalculateSetting(&m_dependenceData, pInputData->pOEMIQSetting, &outputData);

        if (CamxResultSuccess != result)
        {
            CAMX_LOG_ERROR(CamxLogGroupPProc, "IPE TF10 Calculation Failed.");
        }
    }
    else
    {
        RunCalculationFullPass(pInputData);
        RunCalculationDS4Pass(pInputData);
        RunCalculationDS16Pass(pInputData);
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::RunCalculationFullPass
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::RunCalculationFullPass(
    const ISPInputData* pInputData)
{

    CAMX_UNREFERENCED_PARAM(pInputData);
    CamxResult result = CamxResultSuccess;

    m_regCmd[PASS_NAME_FULL].config0.u32All                = 0x4004401e;
    m_regCmd[PASS_NAME_FULL].config1.u32All                = 0x00004040;
    m_regCmd[PASS_NAME_FULL].erodeConfig.u32All            = 0x000086cb;
    m_regCmd[PASS_NAME_FULL].dilateConfig.u32All           = 0x000086cd;
    m_regCmd[PASS_NAME_FULL].cropInHorizStart.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_FULL].cropInHorizEnd.u32All         = 0x00046d1b;
    m_regCmd[PASS_NAME_FULL].lnrStartIDXH.u32All           = 0x00000000;
    m_regCmd[PASS_NAME_FULL].usCrop.u32All                 = 0x00000080;
    m_regCmd[PASS_NAME_FULL].indCropConfig.u32All          = 0x00000000;
    m_regCmd[PASS_NAME_FULL].prngSeed.u32All               = 0x00000001;
    m_regCmd[PASS_NAME_FULL].refCfg0.u32All                = 0x000000f0;
    m_regCmd[PASS_NAME_FULL].refCfg1.u32All                = 0x00000000;
    m_regCmd[PASS_NAME_FULL].refCfg2.u32All                = 0x00080401;
    m_regCmd[PASS_NAME_FULL].cropInvertStart.u32All        = 0x00000000;
    m_regCmd[PASS_NAME_FULL].cropInvertEnd.u32All          = 0x021bc86f;
    m_regCmd[PASS_NAME_FULL].lnrStartIDXV.u32All           = 0x00000000;
    m_regCmd[PASS_NAME_FULL].lnrScale.u32All               = 0x00000000;
    m_regCmd[PASS_NAME_FULL].cropOutVert.u32All            = 0x00000000;
    m_regCmd[PASS_NAME_FULL].refYCfg.u32All                = 0x01010001;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib0.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib1.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib2.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib3.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib4.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib5.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib6.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib7.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib8.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib9.u32All        = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib10.u32All       = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib11.u32All       = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib12.u32All       = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib13.u32All       = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib14.u32All       = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib15.u32All       = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpYContrib16.u32All       = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribY0.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribY1.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribY2.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribY3.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribCB0.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribCB1.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribCB2.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribCB3.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribCR0.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribCR1.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribCR2.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpCContribCR3.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_FULL].tdNtNpUVLimit.u32All          = 0x07ffffff;
    m_regCmd[PASS_NAME_FULL].tdNtNpTopLimit.u32All         = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtNpBottomLimit.u32All      = 0x06018048;
    m_regCmd[PASS_NAME_FULL].tdNtLnrLutY0.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].tdNtLnrLutY1.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].tdNtLnrLutY2.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].tdNtLnrLutY3.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].tdNtLnrLutC0.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].tdNtLnrLutC1.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].tdNtLnrLutC2.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].tdNtLnrLutC3.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsY0.u32All     = 0x00880008;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsY1.u32All     = 0x01680008;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsY2.u32All     = 0x02680008;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsY3.u32All     = 0x03480008;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsY4.u32All     = 0x04480008;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsY5.u32All     = 0x0529800a;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsY6.u32All     = 0x062b800c;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsY7.u32All     = 0x070d800e;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsY8.u32All     = 0x080f8010;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsOOFY.u32All   = 0x02020001;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsC0.u32All     = 0x08000000;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsC1.u32All     = 0x08018002;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsC2.u32All     = 0x08038004;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsC3.u32All     = 0x08058006;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsC4.u32All     = 0x08078008;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsC5.u32All     = 0x0809800a;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsC6.u32All     = 0x080b800c;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsC7.u32All     = 0x080d800e;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsC8.u32All     = 0x080f8010;
    m_regCmd[PASS_NAME_FULL].fsDecisionParamsOOFC.u32All   = 0x02020001;
    m_regCmd[PASS_NAME_FULL].a3T1T2Scale.u32All            = 0x001c71ef;
    m_regCmd[PASS_NAME_FULL].a3T1OFFS.u32All               = 0x00000000;
    m_regCmd[PASS_NAME_FULL].a3T2OFFS.u32All               = 0x00000000;
    m_regCmd[PASS_NAME_FULL].a2MinMax.u32All               = 0xfab4fa7d;
    m_regCmd[PASS_NAME_FULL].a2Slope.u32All                = 0x00002440;
    m_regCmd[PASS_NAME_FULL].fsToA1A4Map0.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].fsToA1A4Map1.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].fsToA1A4Map2.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].fsToA1A4Map3.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].fsToA1A4Map4.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].fsToA1A4Map5.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].fsToA1A4Map6.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].fsToA1A4Map7.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].fsToA1A4Map8.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_FULL].constantBlendingFactor.u32All = 0x00008080;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::RunCalculationDS4Pass
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::RunCalculationDS4Pass(
    const ISPInputData* pInputData)
{
    CAMX_UNREFERENCED_PARAM(pInputData);
    CamxResult result = CamxResultSuccess;

    m_regCmd[PASS_NAME_DC_4].config0.u32All                = 0x401a4624;
    m_regCmd[PASS_NAME_DC_4].config1.u32All                = 0x00008140;
    m_regCmd[PASS_NAME_DC_4].erodeConfig.u32All            = 0x000821cb;
    m_regCmd[PASS_NAME_DC_4].dilateConfig.u32All           = 0x000021cd;
    m_regCmd[PASS_NAME_DC_4].cropInHorizStart.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].cropInHorizEnd.u32All         = 0x0003e4f9;
    m_regCmd[PASS_NAME_DC_4].lnrStartIDXH.u32All           = 0x001ab280;
    m_regCmd[PASS_NAME_DC_4].usCrop.u32All                 = 0x00002042;
    m_regCmd[PASS_NAME_DC_4].indCropConfig.u32All          = 0x0003a401;
    m_regCmd[PASS_NAME_DC_4].prngSeed.u32All               = 0x00000001;
    m_regCmd[PASS_NAME_DC_4].refCfg0.u32All                = 0x000000f1;
    m_regCmd[PASS_NAME_DC_4].refCfg1.u32All                = 0x00001474;
    m_regCmd[PASS_NAME_DC_4].refCfg2.u32All                = 0x000ee804;
    m_regCmd[PASS_NAME_DC_4].cropInvertStart.u32All        = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].cropInvertEnd.u32All          = 0x0086c21b;
    m_regCmd[PASS_NAME_DC_4].lnrStartIDXV.u32All           = 0x001d0468;
    m_regCmd[PASS_NAME_DC_4].lnrScale.u32All               = 0x02d402d4;
    m_regCmd[PASS_NAME_DC_4].cropOutVert.u32All            = 0x00043600;
    m_regCmd[PASS_NAME_DC_4].refYCfg.u32All                = 0x02120006;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib0.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib1.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib2.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib3.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib4.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib5.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib6.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib7.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib8.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib9.u32All        = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib10.u32All       = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib11.u32All       = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib12.u32All       = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib13.u32All       = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib14.u32All       = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib15.u32All       = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpYContrib16.u32All       = 0x01c07015;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribY0.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribY1.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribY2.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribY3.u32All       = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribCB0.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribCB1.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribCB2.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribCB3.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribCR0.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribCR1.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribCR2.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpCContribCR3.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].tdNtNpUVLimit.u32All          = 0x07ffffff;
    m_regCmd[PASS_NAME_DC_4].tdNtNpTopLimit.u32All         = 0x01c07005;
    m_regCmd[PASS_NAME_DC_4].tdNtNpBottomLimit.u32All      = 0x01c07005;
    m_regCmd[PASS_NAME_DC_4].tdNtLnrLutY0.u32All           = 0x86838180;
    m_regCmd[PASS_NAME_DC_4].tdNtLnrLutY1.u32All           = 0x98948f89;
    m_regCmd[PASS_NAME_DC_4].tdNtLnrLutY2.u32All           = 0xafa8a19d;
    m_regCmd[PASS_NAME_DC_4].tdNtLnrLutY3.u32All           = 0xe1d2c4b8;
    m_regCmd[PASS_NAME_DC_4].tdNtLnrLutC0.u32All           = 0x86838180;
    m_regCmd[PASS_NAME_DC_4].tdNtLnrLutC1.u32All           = 0x97938e89;
    m_regCmd[PASS_NAME_DC_4].tdNtLnrLutC2.u32All           = 0xafa8a19c;
    m_regCmd[PASS_NAME_DC_4].tdNtLnrLutC3.u32All           = 0xe1d3c4b9;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsY0.u32All     = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsY1.u32All     = 0x00020002;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsY2.u32All     = 0x00040004;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsY3.u32All     = 0x00060006;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsY4.u32All     = 0x00080008;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsY5.u32All     = 0x0009800A;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsY6.u32All     = 0x000B800C;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsY7.u32All     = 0x000D800E;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsY8.u32All     = 0x000F8010;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsOOFY.u32All   = 0x02020001;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsC0.u32All     = 0x08000000;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsC1.u32All     = 0x08018002;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsC2.u32All     = 0x08038004;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsC3.u32All     = 0x08058006;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsC4.u32All     = 0x08078008;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsC5.u32All     = 0x0809800a;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsC6.u32All     = 0x080b800c;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsC7.u32All     = 0x080d800e;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsC8.u32All     = 0x080f8010;
    m_regCmd[PASS_NAME_DC_4].fsDecisionParamsOOFC.u32All   = 0x02020001;
    m_regCmd[PASS_NAME_DC_4].a3T1T2Scale.u32All            = 0x001c71ef;
    m_regCmd[PASS_NAME_DC_4].a3T1OFFS.u32All               = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].a3T2OFFS.u32All               = 0x00000000;
    m_regCmd[PASS_NAME_DC_4].a2MinMax.u32All               = 0xfab4fa19;
    m_regCmd[PASS_NAME_DC_4].a2Slope.u32All                = 0x00002472;
    m_regCmd[PASS_NAME_DC_4].fsToA1A4Map0.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_DC_4].fsToA1A4Map1.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_DC_4].fsToA1A4Map2.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_DC_4].fsToA1A4Map3.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_DC_4].fsToA1A4Map4.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_DC_4].fsToA1A4Map5.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_DC_4].fsToA1A4Map6.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_DC_4].fsToA1A4Map7.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_DC_4].fsToA1A4Map8.u32All           = 0x80808080;
    m_regCmd[PASS_NAME_DC_4].constantBlendingFactor.u32All = 0x00008080;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::RunCalculationDS16Pass
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::RunCalculationDS16Pass(
    const ISPInputData* pInputData)
{
    CAMX_UNREFERENCED_PARAM(pInputData);
    CamxResult result = CamxResultSuccess;

    m_regCmd[PASS_NAME_DC_16].config0.u32All              = 0x401a4230;
    m_regCmd[PASS_NAME_DC_16].config1.u32All              = 0x00008140;
    m_regCmd[PASS_NAME_DC_16].erodeConfig.u32All          = 0x00000003;
    m_regCmd[PASS_NAME_DC_16].dilateConfig.u32All         = 0x00000005;
    m_regCmd[PASS_NAME_DC_16].cropInHorizStart.u32All     = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].cropInHorizEnd.u32All       = 0x0003bcef;
    m_regCmd[PASS_NAME_DC_16].lnrStartIDXH.u32All         = 0x001ab280;
    m_regCmd[PASS_NAME_DC_16].usCrop.u32All               = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].indCropConfig.u32All        = 0x0003bc01;
    m_regCmd[PASS_NAME_DC_16].prngSeed.u32All             = 0x00000001;
    m_regCmd[PASS_NAME_DC_16].refCfg0.u32All              = 0x000010f1;
    m_regCmd[PASS_NAME_DC_16].refCfg1.u32All              = 0x000004D7;
    m_regCmd[PASS_NAME_DC_16].refCfg2.u32All              = 0x000ee801;
    m_regCmd[PASS_NAME_DC_16].cropInvertStart.u32All      = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].cropInvertEnd.u32All        = 0x0021c087;
    m_regCmd[PASS_NAME_DC_16].lnrStartIDXV.u32All         = 0x001d0468;
    m_regCmd[PASS_NAME_DC_16].lnrScale.u32All             = 0x0b500b50;
    m_regCmd[PASS_NAME_DC_16].cropOutVert.u32All          = 0x00010e00;
    m_regCmd[PASS_NAME_DC_16].refYCfg.u32All              = 0x02108002;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib0.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib1.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib2.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib3.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib4.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib5.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib6.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib7.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib8.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib9.u32All      = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib10.u32All     = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib11.u32All     = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib12.u32All     = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib13.u32All     = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib14.u32All     = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib15.u32All     = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpYContrib16.u32All     = 0x00c03008;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribY0.u32All     = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribY1.u32All     = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribY2.u32All     = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribY3.u32All     = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribCB0.u32All    = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribCB1.u32All    = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribCB2.u32All    = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribCB3.u32All    = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribCR0.u32All    = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribCR1.u32All    = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribCR2.u32All    = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpCContribCR3.u32All    = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].tdNtNpUVLimit.u32All        = 0x07ffffff;
    m_regCmd[PASS_NAME_DC_16].tdNtNpTopLimit.u32All       = 0x00c03004;
    m_regCmd[PASS_NAME_DC_16].tdNtNpBottomLimit.u32All    = 0x00c03004;
    m_regCmd[PASS_NAME_DC_16].tdNtLnrLutY0.u32All         = 0x86838180;
    m_regCmd[PASS_NAME_DC_16].tdNtLnrLutY1.u32All         = 0x98948f89;
    m_regCmd[PASS_NAME_DC_16].tdNtLnrLutY2.u32All         = 0xafa8a19d;
    m_regCmd[PASS_NAME_DC_16].tdNtLnrLutY3.u32All         = 0xe1d2c4b8;
    m_regCmd[PASS_NAME_DC_16].tdNtLnrLutC0.u32All         = 0x86838180;
    m_regCmd[PASS_NAME_DC_16].tdNtLnrLutC1.u32All         = 0x97938e89;
    m_regCmd[PASS_NAME_DC_16].tdNtLnrLutC2.u32All         = 0xafa8a19c;
    m_regCmd[PASS_NAME_DC_16].tdNtLnrLutC3.u32All         = 0xe1d3c4b9;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsY0.u32All   = 0x00880008;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsY1.u32All   = 0x01680008;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsY2.u32All   = 0x02680008;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsY3.u32All   = 0x03480008;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsY4.u32All   = 0x04480008;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsY5.u32All   = 0x0529800a;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsY6.u32All   = 0x062b800c;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsY7.u32All   = 0x070d800e;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsY8.u32All   = 0x080f8010;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsOOFY.u32All = 0x02020001;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsC0.u32All   = 0x08000000;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsC1.u32All   = 0x08018002;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsC2.u32All   = 0x08038004;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsC3.u32All   = 0x08058006;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsC4.u32All   = 0x08078008;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsC5.u32All   = 0x0809800a;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsC6.u32All   = 0x080b800c;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsC7.u32All   = 0x080d800e;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsC8.u32All   = 0x080f8010;
    m_regCmd[PASS_NAME_DC_16].fsDecisionParamsOOFC.u32All = 0x02020001;
    m_regCmd[PASS_NAME_DC_16].a3T1T2Scale.u32All          = 0x001c71ef;
    m_regCmd[PASS_NAME_DC_16].a3T1OFFS.u32All             = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].a3T2OFFS.u32All             = 0x00000000;
    m_regCmd[PASS_NAME_DC_16].a2MinMax.u32All             = 0xfab4fa19;
    m_regCmd[PASS_NAME_DC_16].a2Slope.u32All              = 0x00002472;
    m_regCmd[PASS_NAME_DC_16].fsToA1A4Map0.u32All         = 0x80808080;
    m_regCmd[PASS_NAME_DC_16].fsToA1A4Map1.u32All         = 0x80808080;
    m_regCmd[PASS_NAME_DC_16].fsToA1A4Map2.u32All         = 0x80808080;
    m_regCmd[PASS_NAME_DC_16].fsToA1A4Map3.u32All         = 0x80808080;
    m_regCmd[PASS_NAME_DC_16].fsToA1A4Map4.u32All         = 0x80808080;
    m_regCmd[PASS_NAME_DC_16].fsToA1A4Map5.u32All         = 0x80808080;
    m_regCmd[PASS_NAME_DC_16].fsToA1A4Map6.u32All         = 0x80808080;
    m_regCmd[PASS_NAME_DC_16].fsToA1A4Map7.u32All         = 0x80808080;
    m_regCmd[PASS_NAME_DC_16].fsToA1A4Map8.u32All         = 0x80808080;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::UpdateIPEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::UpdateIPEInternalData(
    const ISPInputData* pInputData)
{
    CamxResult      result         = CamxResultSuccess;
    IpeIQSettings*  pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

    if (CSLCameraTitanVersion::CSLTitan150 == pInputData->titanVersion)
    {
        // For Talos Refinement should be on ICA2
        m_refinementParams.icaNum = 1;
    }

    for (UINT32 passType = PASS_NAME_FULL; passType <= PASS_NAME_DC_64; passType++)
    {
        FillFirmwareConfig0(pIPEIQSettings, passType);
        FillFirmwareConfig1(pIPEIQSettings, passType);

        pIPEIQSettings->tfParameters.parameters[passType].morphErode     =
            static_cast<TfIndMorphology>(m_regCmd[passType].erodeConfig.bitfields.MORPH_ERODE_SIZE);
        pIPEIQSettings->tfParameters.parameters[passType].morphDilate    =
            static_cast<TfIndMorphology>(m_regCmd[passType].dilateConfig.bitfields.MORPH_DILATE_SIZE);

        pIPEIQSettings->tfParameters.parameters[passType].lnrStartIdxH   =
            (0x1fffff & m_regCmd[passType].lnrStartIDXH.bitfields.LNRSTARTIDXH) |
            (0x100000 & m_regCmd[passType].lnrStartIDXH.bitfields.LNRSTARTIDXH ? 0xffe00000 : 0);
        pIPEIQSettings->tfParameters.parameters[passType].lnrStartIdxV   =
            (0x1fffff & m_regCmd[passType].lnrStartIDXV.bitfields.LNRSTARTIDXV) |
            (0x100000 & m_regCmd[passType].lnrStartIDXV.bitfields.LNRSTARTIDXV ? 0xffe00000 : 0);
        pIPEIQSettings->tfParameters.parameters[passType].lnrScaleH      =
            static_cast<INT32>(m_regCmd[passType].lnrScale.bitfields.LNRSCALEH);
        pIPEIQSettings->tfParameters.parameters[passType].lnrScaleV      =
            static_cast<INT32>(m_regCmd[passType].lnrScale.bitfields.LNRSCALEV);

        if (m_bypassMode == FALSE)
        {
            pIPEIQSettings->tfParameters.parameters[passType].a2MaxC =
                m_regCmd[passType].a2MinMax.bitfields.A2MAXC;
            pIPEIQSettings->tfParameters.parameters[passType].a2MinC =
                m_regCmd[passType].a2MinMax.bitfields.A2MINC;
            pIPEIQSettings->tfParameters.parameters[passType].a2MaxY =
                m_regCmd[passType].a2MinMax.bitfields.A2MAXY;
            pIPEIQSettings->tfParameters.parameters[passType].a2MinY =
                m_regCmd[passType].a2MinMax.bitfields.A2MINY;
            pIPEIQSettings->tfParameters.parameters[passType].a2SlopeC =
                m_regCmd[passType].a2Slope.bitfields.A2SLOPEC;
            pIPEIQSettings->tfParameters.parameters[passType].a2SlopeY =
                m_regCmd[passType].a2Slope.bitfields.A2SLOPEY;
        }
        else
        {
            pIPEIQSettings->tfParameters.parameters[passType].a2MaxC   = 0;
            pIPEIQSettings->tfParameters.parameters[passType].a2MinC   = 0;
            pIPEIQSettings->tfParameters.parameters[passType].a2MaxY   = 0;
            pIPEIQSettings->tfParameters.parameters[passType].a2MinY   = 0;
            pIPEIQSettings->tfParameters.parameters[passType].a2SlopeC = 0;
            pIPEIQSettings->tfParameters.parameters[passType].a2SlopeY = 0;
        }

        FillFirmwareFStoA1A4Map(pIPEIQSettings, passType);
        pIPEIQSettings->tfParameters.parameters[passType].transformConfidenceVal = 256;
        pIPEIQSettings->tfParameters.parameters[passType].enableTransformConfidence = 0;

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, " pass %d transformConfidenceVal %d,  enableTransformConfidence %d",
                         passType,
                         pIPEIQSettings->tfParameters.parameters[passType].transformConfidenceVal,
                         pIPEIQSettings->tfParameters.parameters[passType].enableTransformConfidence);

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "pass %d invalidMctfpImage  %d, disable output %d, disableindica %d"
                         "useImgAsMctfpInput %d, useAnroAsImgInput %d, bneding mode %d",
                         passType, pIPEIQSettings->tfParameters.parameters[passType].invalidMctfpImage,
                         pIPEIQSettings->tfParameters.parameters[passType].disableOutputIndications,
                         pIPEIQSettings->tfParameters.parameters[passType].disableUseIndications,
                         pIPEIQSettings->tfParameters.parameters[passType].useImgAsMctfpInput,
                         pIPEIQSettings->tfParameters.parameters[passType].useAnroAsImgInput,
                         pIPEIQSettings->tfParameters.parameters[passType].blendingMode);
    }

    ValidateAndCorrectTFParams(pInputData);
    Utils::Memcpy(&pIPEIQSettings->refinementParameters, &m_refinementParams,
        sizeof(pIPEIQSettings->refinementParameters));
    // Each TF pass enable is set in CalculateHWSetting from Chromatix region data.
    SetTFEnable(pInputData);
    SetRefinementEnable(pInputData);

    // Post tuning metadata if setting is enabled
    if (NULL != pInputData->pIPETuningMetadata)
    {
        CAMX_STATIC_ASSERT((sizeof(IPETFRegCmd) * PASS_NAME_MAX) <=
                           sizeof(pInputData->pIPETuningMetadata->IPETFData.TFData));

        for (UINT passNumber = PASS_NAME_FULL; passNumber < PASS_NAME_MAX; passNumber++)
        {
            Utils::Memcpy(&pInputData->pIPETuningMetadata->IPETFData.TFData[passNumber],
                          &m_regCmd[passNumber],
                          sizeof(IPETFRegCmd));
        }
    }

    // Post noise reduction mode metadat
    if (NULL != pInputData->pCalculatedData)
    {
        pInputData->pCalculatedData->metadata.noiseReductionMode = pInputData->pHALTagsData->noiseReductionMode;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::Execute(
    ISPInputData* pInputData)
{
    CamxResult result;

    if (NULL != pInputData)
    {
        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);

        if (CamxResultSuccess == result)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);
            }

            if ((CamxResultSuccess == result) && (TRUE == m_moduleEnable))
            {
                result = CreateCmdList(pInputData);
            }

            if (CamxResultSuccess == result)
            {
                result = UpdateIPEInternalData(pInputData);
            }

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Operation failed %d", result);
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
/// IPETF10::GetModuleData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPETF10::GetModuleData(
    VOID* pModuleData)

{
    CAMX_ASSERT(NULL != pModuleData);
    IPEIQModuleData* pData = reinterpret_cast<IPEIQModuleData*>(pModuleData);

    // data is expected to be filled after execute
    pData->singlePassCmdLength = m_singlePassCmdLength;
    Utils::Memcpy(pData->offsetPass, m_offsetPass, sizeof(pData->offsetPass));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::FillFirmwareConfig0
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPETF10::FillFirmwareConfig0(
    IpeIQSettings*  pIPEIQSettings,
    UINT32          passType)
{
    pIPEIQSettings->tfParameters.parameters[passType].invalidMctfpImage               =
        m_regCmd[passType].config0.bitfields.INVALIDMCTFPIMAGE;
    pIPEIQSettings->tfParameters.parameters[passType].disableOutputIndications        =
        m_regCmd[passType].config0.bitfields.DISABLEOUTPUTINDICATIONS;
    pIPEIQSettings->tfParameters.parameters[passType].disableUseIndications           =
        (m_regCmd[passType].config0.bitfields.USEINDICATIONS == 0) ? 1 : 0;
    pIPEIQSettings->tfParameters.parameters[passType].smearInputsForDecisions         =
        m_regCmd[passType].config0.bitfields.SMEARINPUTSFORDECISIONS;
    pIPEIQSettings->tfParameters.parameters[passType].useAnrForDecisions_Y            =
        m_regCmd[passType].config0.bitfields.USEANRFORDECISIONS_Y;
    pIPEIQSettings->tfParameters.parameters[passType].useAnrForDecisions_C            =
        m_regCmd[passType].config0.bitfields.USEANRFORDECISIONS_C;
    pIPEIQSettings->tfParameters.parameters[passType].enableLNR                       =
        m_regCmd[passType].config0.bitfields.ENABLELNR;
    pIPEIQSettings->tfParameters.parameters[passType].enableNoiseEstByLuma            =
        m_regCmd[passType].config0.bitfields.ENABLENOISEESTBYLUMA;
    pIPEIQSettings->tfParameters.parameters[passType].enableNoiseEstByChroma          =
        m_regCmd[passType].config0.bitfields.ENABLENOISEESTBYCHROMA;
    pIPEIQSettings->tfParameters.parameters[passType].paddingByReflection             =
        m_regCmd[passType].config0.bitfields.PADDINGBYREFLECTION;
    pIPEIQSettings->tfParameters.parameters[passType].sadYCalcMode                    =
        static_cast<TfSadYCalcMode>(m_regCmd[passType].config0.bitfields.SADYCALCMODE);
    pIPEIQSettings->tfParameters.parameters[passType].isSameBlendingForAllFrequencies =
        m_regCmd[passType].config0.bitfields.ISSAMEBLENDINGFORALLFREQUENCIES;
    pIPEIQSettings->tfParameters.parameters[passType].isSadC5x3                       =
        m_regCmd[passType].config0.bitfields.ISSADC5X3;
    pIPEIQSettings->tfParameters.parameters[passType].lnrLutShiftY                    =
        m_regCmd[passType].config0.bitfields.LNRLUTSHIFTY;
    pIPEIQSettings->tfParameters.parameters[passType].lnrLutShiftC                    =
        m_regCmd[passType].config0.bitfields.LNRLUTSHIFTC;
    pIPEIQSettings->tfParameters.parameters[passType].isDCI                           =
        m_regCmd[passType].config0.bitfields.ISDCI;
    pIPEIQSettings->tfParameters.parameters[passType].indicationsDominateFsDecision   =
        m_regCmd[passType].config0.bitfields.INDICATIONSDOMINATEFSDECISION;
    pIPEIQSettings->tfParameters.parameters[passType].applyFsRankFilter               =
        m_regCmd[passType].config0.bitfields.APPLYFSRANKFILTER;
    pIPEIQSettings->tfParameters.parameters[passType].applyFsLpf                      =
        m_regCmd[passType].config0.bitfields.APPLYFSLPF;
    pIPEIQSettings->tfParameters.parameters[passType].takeOofIndFrom                  =
        m_regCmd[passType].config0.bitfields.TAKEOOFINDFROM;
    pIPEIQSettings->tfParameters.parameters[passType].isSameBlendingForAllFrequencies =
        m_regCmd[passType].config0.bitfields.ISSAMEBLENDINGFORALLFREQUENCIES;
    pIPEIQSettings->tfParameters.parameters[passType].fsDecisionFreeParamShiftC       =
        m_regCmd[passType].config0.bitfields.FSDECISIONFREEPARAMSHIFTC;
    pIPEIQSettings->tfParameters.parameters[passType].fsDecisionFreeParamShiftY       =
        m_regCmd[passType].config0.bitfields.FSDECISIONFREEPARAMSHIFTY;
    pIPEIQSettings->tfParameters.parameters[passType].outOfFramePixelsConfidence      =
        m_regCmd[passType].config0.bitfields.OUTOFFRAMEPIXELSCONFIDENCE;
    pIPEIQSettings->tfParameters.parameters[passType].disableChromaGhostDetection     =
        m_regCmd[passType].config0.bitfields.DISABLECHROMAGHOSTDETECTION;
    pIPEIQSettings->tfParameters.parameters[passType].disableLumaGhostDetection       =
        m_regCmd[passType].config0.bitfields.DISABLELUMAGHOSTDETECTION;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::FillFirmwareConfig1
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPETF10::FillFirmwareConfig1(
    IpeIQSettings*  pIPEIQSettings,
    UINT32          passType)
{
    pIPEIQSettings->tfParameters.parameters[passType].useImgAsMctfpInput              =
        m_regCmd[passType].config1.bitfields.USEIMGASMCTFPINPUT;
    pIPEIQSettings->tfParameters.parameters[passType].useAnroAsImgInput               =
        m_regCmd[passType].config1.bitfields.USEANROASIMGINPUT;
    pIPEIQSettings->tfParameters.parameters[passType].ditherCr                        =
        m_regCmd[passType].config1.bitfields.DITHERCR;
    pIPEIQSettings->tfParameters.parameters[passType].ditherCb                        =
        m_regCmd[passType].config1.bitfields.DITHERCB;
    pIPEIQSettings->tfParameters.parameters[passType].ditherY                         =
        m_regCmd[passType].config1.bitfields.DITHERY;
    pIPEIQSettings->tfParameters.parameters[passType].blendingMode                    =
        static_cast<TfBlendMode>(m_regCmd[passType].config1.bitfields.BLENDINGMODE);

    if (FALSE == m_bypassMode)
    {
        pIPEIQSettings->tfParameters.parameters[passType].invertTemporalBlendingWeights =
            m_regCmd[passType].config1.bitfields.INVERTTEMPORALBLENDINGWEIGHTS;
    }
    else
    {
        pIPEIQSettings->tfParameters.parameters[passType].invertTemporalBlendingWeights   = 0;
    }

    pIPEIQSettings->tfParameters.parameters[passType].enableIndicationsDecreaseFactor =
        (m_regCmd[passType].config1.bitfields.INDICATIONSDECREASEFACTOR == 16) ? 0 : 1;
    pIPEIQSettings->tfParameters.parameters[passType].indicationsDecreaseFactor       =
        m_regCmd[passType].config1.bitfields.INDICATIONSDECREASEFACTOR;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::FillFirmwareFStoA1A4Map
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPETF10::FillFirmwareFStoA1A4Map(
    IpeIQSettings*  pIPEIQSettings,
    UINT32          passType)
{
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapC[0] =
        m_regCmd[passType].fsToA1A4Map0.bitfields.FSTOA4MAPC_0;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapY[0] =
        m_regCmd[passType].fsToA1A4Map0.bitfields.FSTOA4MAPY_0;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapC[0] =
        m_regCmd[passType].fsToA1A4Map0.bitfields.FSTOA1MAPC_0;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapY[0] =
        m_regCmd[passType].fsToA1A4Map0.bitfields.FSTOA1MAPY_0;

    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapC[1] =
        m_regCmd[passType].fsToA1A4Map1.bitfields.FSTOA4MAPC_1;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapY[1] =
        m_regCmd[passType].fsToA1A4Map1.bitfields.FSTOA4MAPY_1;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapC[1] =
        m_regCmd[passType].fsToA1A4Map1.bitfields.FSTOA1MAPC_1;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapY[1] =
        m_regCmd[passType].fsToA1A4Map1.bitfields.FSTOA1MAPY_1;

    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapC[2] =
        m_regCmd[passType].fsToA1A4Map2.bitfields.FSTOA4MAPC_2;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapY[2] =
        m_regCmd[passType].fsToA1A4Map2.bitfields.FSTOA4MAPY_2;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapC[2] =
        m_regCmd[passType].fsToA1A4Map2.bitfields.FSTOA1MAPC_2;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapY[2] =
        m_regCmd[passType].fsToA1A4Map2.bitfields.FSTOA1MAPY_2;

    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapC[3] =
        m_regCmd[passType].fsToA1A4Map3.bitfields.FSTOA4MAPC_3;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapY[3] =
        m_regCmd[passType].fsToA1A4Map3.bitfields.FSTOA4MAPY_3;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapC[3] =
        m_regCmd[passType].fsToA1A4Map3.bitfields.FSTOA1MAPC_3;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapY[3] =
        m_regCmd[passType].fsToA1A4Map3.bitfields.FSTOA1MAPY_3;

    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapC[4] =
        m_regCmd[passType].fsToA1A4Map4.bitfields.FSTOA4MAPC_4;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapY[4] =
        m_regCmd[passType].fsToA1A4Map4.bitfields.FSTOA4MAPY_4;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapC[4] =
        m_regCmd[passType].fsToA1A4Map4.bitfields.FSTOA1MAPC_4;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapY[4] =
        m_regCmd[passType].fsToA1A4Map4.bitfields.FSTOA1MAPY_4;

    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapC[5] =
        m_regCmd[passType].fsToA1A4Map5.bitfields.FSTOA4MAPC_5;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapY[5] =
        m_regCmd[passType].fsToA1A4Map5.bitfields.FSTOA4MAPY_5;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapC[5] =
        m_regCmd[passType].fsToA1A4Map5.bitfields.FSTOA1MAPC_5;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapY[5] =
        m_regCmd[passType].fsToA1A4Map5.bitfields.FSTOA1MAPY_5;

    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapC[6] =
        m_regCmd[passType].fsToA1A4Map6.bitfields.FSTOA4MAPC_6;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapY[6] =
        m_regCmd[passType].fsToA1A4Map6.bitfields.FSTOA4MAPY_6;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapC[6] =
        m_regCmd[passType].fsToA1A4Map6.bitfields.FSTOA1MAPC_6;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapY[6] =
        m_regCmd[passType].fsToA1A4Map6.bitfields.FSTOA1MAPY_6;

    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapC[7] =
        m_regCmd[passType].fsToA1A4Map7.bitfields.FSTOA4MAPC_7;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapY[7] =
        m_regCmd[passType].fsToA1A4Map7.bitfields.FSTOA4MAPY_7;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapC[7] =
        m_regCmd[passType].fsToA1A4Map7.bitfields.FSTOA1MAPC_7;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapY[7] =
        m_regCmd[passType].fsToA1A4Map7.bitfields.FSTOA1MAPY_7;

    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapC[8] =
        m_regCmd[passType].fsToA1A4Map8.bitfields.FSTOA4MAPC_8;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA4MapY[8] =
        m_regCmd[passType].fsToA1A4Map8.bitfields.FSTOA4MAPY_8;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapC[8] =
        m_regCmd[passType].fsToA1A4Map8.bitfields.FSTOA1MAPC_8;
    pIPEIQSettings->tfParameters.parameters[passType].fsToA1MapY[8] =
        m_regCmd[passType].fsToA1A4Map8.bitfields.FSTOA1MAPY_8;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::CheckIPEInstanceProperty
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IPETF10::CheckIPEInstanceProperty(
    ISPInputData* pInput)
{
    BOOL   isChanged   = FALSE;
    UINT8  useCase     = CONFIG_STILL;
    UINT8  configMF    = MF_CONFIG_NONE;
    UINT32 maxUsedPass = pInput->pipelineIPEData.numPasses;

    UINT32 perspectiveConfidence = 256;

    CAMX_ASSERT(NULL != pInput);

    IPEInstanceProperty* pInstanceProperty = &pInput->pipelineIPEData.instanceProperty;
    if (pInstanceProperty->processingType == IPEProcessingType::IPEMFNRBlend ||
        pInstanceProperty->processingType == IPEProcessingType::IPEMFSRBlend)
    {
        configMF = MF_CONFIG_TEMPORAL;
    }
    else if (pInstanceProperty->processingType == IPEProcessingType::IPEMFNRPrefilter ||
             pInstanceProperty->processingType == IPEProcessingType::IPEMFSRPrefilter)
    {
        configMF    = MF_CONFIG_PREFILT;
        maxUsedPass = 1;
    }
    else if (pInstanceProperty->processingType == IPEProcessingType::IPEMFNRPostfilter ||
             pInstanceProperty->processingType == IPEProcessingType::IPEMFSRPostfilter)
    {
        if (pInstanceProperty->profileId == IPEProfileId::IPEProfileIdNPS)
        {
            configMF = MF_CONFIG_TEMPORAL;
        }
        else if (pInstanceProperty->profileId == IPEProfileId::IPEProfileIdDefault)
        {
            configMF = MF_CONFIG_POSTPROCESS;
            maxUsedPass = 1;
        }
    }
    else if (pInstanceProperty->processingType == IPEProcessingType::IPEMFNRScale)
    {
        configMF = MF_CONFIG_NONE;
    }
    else if ((pInstanceProperty->processingType == IPEProcessingType::IPEProcessingTypeDefault) ||
             (pInstanceProperty->processingType == IPEProcessingType::IPEProcessingPreview))
    {
        if (pInput->pipelineIPEData.numOutputRefPorts == 4)
        {
            useCase  = CONFIG_STILL;
            configMF = MF_CONFIG_NONE;
        }
        else if (pInput->pipelineIPEData.numOutputRefPorts == 3)
        {
            useCase     = CONFIG_VIDEO;
            configMF    = MF_CONFIG_NONE;
            maxUsedPass = PASS_NAME_MAX - 1;
        }
        else
        {
            CAMX_LOG_WARN(CamxLogGroupPProc,
                          "Wrong number of output reference ports %d = ",
                           pInput->pipelineIPEData.numOutputRefPorts);
            useCase = CONFIG_STILL;
            configMF = MF_CONFIG_NONE;
            maxUsedPass = 1;
            CAMX_LOG_INFO(CamxLogGroupPProc, "TF Blend on Full pass");
        }
    }
    else
    {
        CAMX_LOG_WARN(CamxLogGroupPProc, "Wrong processingType %d = ", pInstanceProperty->processingType);
        useCase     = CONFIG_STILL;
        configMF    = MF_CONFIG_NONE;
        maxUsedPass = 1;
    }

    if (0 != (IPEStabilizationType::IPEStabilizationMCTF & pInstanceProperty->stabilizationType))
    {
        perspectiveConfidence = pInput->ICAConfigData.ICAReferenceParams.perspectiveConfidence;
    }
    else
    {
        perspectiveConfidence = 256;
    }

    if (m_dependenceData.useCase != useCase)
    {
        isChanged                = TRUE;
        m_dependenceData.useCase = useCase;
    }

    if (m_dependenceData.configMF != configMF)
    {
        isChanged                 = TRUE;
        m_dependenceData.configMF = configMF;
    }

    if (m_dependenceData.maxUsedPasses != maxUsedPass)
    {
        isChanged                      = TRUE;
        m_dependenceData.maxUsedPasses = maxUsedPass;
    }

    if (m_dependenceData.perspectiveConfidence != perspectiveConfidence)
    {
        isChanged                              = TRUE;
        m_dependenceData.perspectiveConfidence = perspectiveConfidence;
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MCTF case: enable TransformConfidence");
    }

    return isChanged;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::AllocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::AllocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;
    TFNcLibOutputData data;

    result = IQInterface::IPETF10GetInitializationData(&data);
    CAMX_ASSERT(CamxResultSuccess == result);

    // +1 for ouput of interpolation library
    UINT interpolationSize = (sizeof(tf_1_0_0::mod_tf10_cct_dataType::cct_dataStruct) *
        (TFMaxNonLeafNode + 1));
    if (NULL == m_dependenceData.pInterpolationData)
    {
        m_dependenceData.pInterpolationData = CAMX_CALLOC(interpolationSize);
        if (NULL == m_dependenceData.pInterpolationData)
        {
            result = CamxResultENoMemory;
        }
    }

    if ((NULL == m_dependenceData.pNCChromatix) && (CamxResultSuccess == result))
    {
        m_dependenceData.pNCChromatix = CAMX_CALLOC(data.TF10ChromatixSize);
        if (NULL == m_dependenceData.pNCChromatix)
        {
            result = CamxResultENoMemory;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::DeallocateCommonLibraryData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::DeallocateCommonLibraryData()
{
    CamxResult result = CamxResultSuccess;

    if (NULL != m_dependenceData.pInterpolationData)
    {
        CAMX_FREE(m_dependenceData.pInterpolationData);
        m_dependenceData.pInterpolationData = NULL;
    }

    if (NULL != m_dependenceData.pNCChromatix)
    {
        CAMX_FREE(m_dependenceData.pNCChromatix);
        m_dependenceData.pNCChromatix = NULL;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::ValidateAndCorrectTFParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::ValidateAndCorrectTFParams(
    const ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    if (TRUE == m_validateTFParams)
    {
        switch (m_dependenceData.useCase)
        {
            case CONFIG_VIDEO:
                ValidateAndCorrectMCTFParameters(pInputData);
                break;
            case CONFIG_STILL:
                ValidateAndCorrectStillModeParameters(pInputData);
                break;
            default:
                break;
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::ValidateAndCorrectStillModeParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::ValidateAndCorrectStillModeParameters(
    const ISPInputData* pInputData)
{
    CamxResult      result         = CamxResultSuccess;
    IpeIQSettings*  pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

    // Regular Still Mode/Prefilter and Postfilter
    if (MF_CONFIG_TEMPORAL != m_dependenceData.configMF)
    {
        // Regular blending is post process and default mode and constant blending in prefiltering
        UINT32 blendingMode = ((MF_CONFIG_NONE == m_dependenceData.configMF) ||
                              (MF_CONFIG_POSTPROCESS == m_dependenceData.configMF)) ?
                              TF_BLEND_MODE_REGULAR : TF_BLEND_MODE_CONSTANT_BLENDING;

        if (0 != m_dependenceData.hasTFRefInput)
        {
            CAMX_LOG_WARN(CamxLogGroupPProc, "Reference input present for still mode , invalid config");
        }

        if (TRUE == pIPEIQSettings->tfParameters.parameters[0].moduleCfg.EN)
        {
            ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[PASS_NAME_FULL].disableUseIndications,
                                 PASS_NAME_FULL, 1, "Still-disableUseIndications");

            ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[PASS_NAME_FULL].disableOutputIndications,
                                 PASS_NAME_FULL, 1, "Still-disableOutputIndications");

            ValidateAndSetParams(reinterpret_cast<UINT32*>(&pIPEIQSettings->tfParameters.parameters[0].blendingMode),
                                 PASS_NAME_FULL, blendingMode, "Still-blendingMode");

            ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[PASS_NAME_FULL].useImgAsMctfpInput,
                                 PASS_NAME_FULL, 1, "Still-useImgAsMctfpInput");

            ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[PASS_NAME_FULL].useAnroAsImgInput,
                                 PASS_NAME_FULL, 1, "Still-useAnroAsImgInput");

            ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[PASS_NAME_FULL].invalidMctfpImage,
                                 PASS_NAME_FULL, 0, "Still-invalidMctfpImage");
        }

        ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[PASS_NAME_DC_4].moduleCfg.EN,
                             PASS_NAME_DC_4, 0, "Still-DC4 pass");

        ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[PASS_NAME_DC_16].moduleCfg.EN,
                             PASS_NAME_DC_16, 1, "Still-DC16 pass");

        ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[PASS_NAME_DC_64].moduleCfg.EN,
                             PASS_NAME_DC_64, 0, "Still-DC64 pass");
    }
    // Still mode blend
    else
    {
        for (UINT32 pass = 0; pass < m_dependenceData.maxUsedPasses; pass++)
        {
            if (TRUE == pIPEIQSettings->tfParameters.parameters[pass].moduleCfg.EN)
            {
                if (PASS_NAME_FULL == pass)
                {
                    ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableUseIndications,
                                         pass, 0, "Blend-disableUseIndications");

                    ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableOutputIndications,
                                         pass, 1, "Blend-disableOutputIndications");

                    ValidateAndSetParams(reinterpret_cast<UINT32*>(&pIPEIQSettings->tfParameters.parameters[pass].blendingMode),
                                         pass, 0, "Blend-blendingMode");
                }
                else if (PASS_NAME_DC_4 == pass)
                {
                    ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableUseIndications,
                                         pass, 0, "Blend-disableUseIndications");

                    ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableOutputIndications,
                                         pass, 0, "Blend-disableOutputIndications");

                    ValidateAndSetParams(reinterpret_cast<UINT32*>(&pIPEIQSettings->tfParameters.parameters[pass].blendingMode),
                                         pass, 1, "Blend-blendingMode");
                }
                else if (PASS_NAME_DC_16 == pass)
                {
                    ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableUseIndications,
                                         pass, 0, "Blend-disableUseIndications");

                    ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableOutputIndications,
                                         pass, 0, "Blend-disableOutputIndications");

                    ValidateAndSetParams(reinterpret_cast<UINT32*>(&pIPEIQSettings->tfParameters.parameters[pass].blendingMode),
                                         pass, 1, "Blend-blendingMode");
                }
                else
                {
                    ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableUseIndications,
                                         pass, 1, "Blend-disableUseIndications");

                    ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableOutputIndications,
                                         pass, 0, "Blend-disableOutputIndications");

                    ValidateAndSetParams(reinterpret_cast<UINT32*>(&pIPEIQSettings->tfParameters.parameters[pass].blendingMode),
                                         pass, 1, "Blend-blendingMode");
                }

                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].useImgAsMctfpInput,
                                     pass, 0, "Blend-useImgAsMctfpInput");

                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].useAnroAsImgInput,
                                     pass, 0, "Blend-useAnroAsImgInput");

                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].invalidMctfpImage,
                                     pass, 0, "Blend-invalidMctfpImage");
            }
        }
    }

    return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::ValidateAndCorrectMCTFParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IPETF10::ValidateAndCorrectMCTFParameters(
    const ISPInputData* pInputData)
{

    CamxResult      result         = CamxResultSuccess;
    IpeIQSettings*  pIPEIQSettings = reinterpret_cast<IpeIQSettings*>(pInputData->pipelineIPEData.pIPEIQSettings);

    if ((PASS_NAME_MAX - 1) != m_dependenceData.maxUsedPasses)
    {
        CAMX_LOG_WARN(CamxLogGroupPProc, "Max passes for MCTF is incorrect");
    }

    UINT32 validInvalidMCTFpImageValue = (TRUE == m_dependenceData.hasTFRefInput) ? 0 : 1;

    for (UINT32 pass = 0; pass < m_dependenceData.maxUsedPasses; pass++)
    {
        if (TRUE == pIPEIQSettings->tfParameters.parameters[pass].moduleCfg.EN)
        {
            if (PASS_NAME_FULL == pass)
            {
                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableUseIndications,
                                     pass, 0, "MCTF-disableUseIndications");

                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableOutputIndications,
                                     pass, 1, "MCTF-disableOutputIndications");

                ValidateAndSetParams(reinterpret_cast<UINT32*>(&pIPEIQSettings->tfParameters.parameters[pass].blendingMode),
                                     pass, 0, "MCTF-blendingMode");
            }
            else if (PASS_NAME_DC_4 == pass)
            {
                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableUseIndications,
                                     pass, 0, "MCTF-disableUseIndications");

                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableOutputIndications,
                                     pass, 0, "MCTF-disableOutputIndications");

                ValidateAndSetParams(reinterpret_cast<UINT32*>(&pIPEIQSettings->tfParameters.parameters[pass].blendingMode),
                                     pass, 1, "MCTF-blendingMode");
            }
            else if (PASS_NAME_DC_16 == pass)
            {
                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableUseIndications,
                                     pass, 1, "MCTF-disableUseIndications");

                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].disableOutputIndications,
                                     pass, 0, "MCTF-disableOutputIndications");

                ValidateAndSetParams(reinterpret_cast<UINT32*>(&pIPEIQSettings->tfParameters.parameters[pass].blendingMode),
                                     pass, 1, "MCTF-blendingMode");
            }
            else
            {
                ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[PASS_NAME_DC_64].moduleCfg.EN,
                                     PASS_NAME_DC_64, 0, "MCTF-DC64 pass");
            }

            ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].useImgAsMctfpInput,
                                 pass, 0, "MCTF-useImgAsMctfpInput");

            ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].useAnroAsImgInput,
                                 pass, 0, "MCTF-useAnroAsImgInput");

            ValidateAndSetParams(&pIPEIQSettings->tfParameters.parameters[pass].invalidMctfpImage,
                                 pass, validInvalidMCTFpImageValue, "MCTF-invalidMctfpImage");
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::IPETF10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPETF10::IPETF10(
    const CHAR*          pNodeIdentifier,
    IPEModuleCreateData* pCreateData)
{
    m_pNodeIdentifier       = pNodeIdentifier;
    m_type                  = ISPIQModuleType::IPETF;
    m_moduleEnable          = TRUE;
    m_32bitDMILength        = 0;
    m_64bitDMILength        = 0;
    m_IPETFCmdBufferSize    = (sizeof(IPETFRegCmd) + (cdm_get_cmd_header_size(CDMCmdRegContinuous) * 4));
    m_cmdLength             = m_IPETFCmdBufferSize * PASS_NAME_MAX;
    m_singlePassCmdLength   = m_IPETFCmdBufferSize;
    m_bypassMode            = FALSE;
    m_pChromatix            = NULL;

    m_dependenceData.moduleEnable = FALSE;   ///< First frame is always FALSE

    Utils::Memset(&m_refinementParams, 0x0, sizeof(m_refinementParams));
    Utils::Memset(&m_TFParams, 0x0, sizeof(m_TFParams));


    if (TRUE == pCreateData->initializationData.registerBETEn)
    {
        m_enableCommonIQ   = TRUE;
        m_validateTFParams = FALSE;
    }
    else
    {
        Titan17xContext* pContext = static_cast<Titan17xContext*>(pCreateData->initializationData.pHwContext);
        m_enableCommonIQ          = pContext->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->enableTFCommonIQModule;
        m_validateTFParams        = pContext->GetTitan17xSettingsManager()->GetTitan17xStaticSettings()->validateTFParams;
    }

    CAMX_LOG_INFO(CamxLogGroupPProc, "IPE TF m_cmdLength %d ", m_cmdLength);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IPETF10::~IPETF10
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IPETF10::~IPETF10()
{
    DeallocateCommonLibraryData();
    m_pChromatix = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IPETF10::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IPETF10::DumpRegConfig() const
{
    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DumpRegConfig ");
    CHAR  dumpFilename[256];
    FILE* pFile = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename), "%s/IPE_regdump.txt", ConfigFileDirectory);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "a+");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "******** IPE TF10 [HEX] ********\n");
        for (UINT passType = 0; passType < PASS_NAME_MAX; passType++)
        {
            CamX::OsUtils::FPrintF(pFile, "== passType = %d ================================================\n", passType);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].config0.u32All                            = %x\n",
                passType, m_regCmd[passType].config0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].config1.u32All                            = %x\n",
                passType, m_regCmd[passType].config1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].erodeConfig.u32All                        = %x\n",
                passType, m_regCmd[passType].erodeConfig.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].dilateConfig.u32All                       = %x\n",
                passType, m_regCmd[passType].dilateConfig.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].cropInHorizStart.u32All                   = %x\n",
                passType, m_regCmd[passType].cropInHorizStart.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].cropInHorizStart.u32All                   = %x\n",
                passType, m_regCmd[passType].cropInHorizStart.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].cropInHorizEnd.u32All                     = %x\n",
                passType, m_regCmd[passType].cropInHorizEnd.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].lnrStartIDXH.u32All                       = %x\n",
                passType, m_regCmd[passType].lnrStartIDXH.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].usCrop.u32All                             = %x\n",
                passType, m_regCmd[passType].usCrop.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].indCropConfig.u32All                      = %x\n",
                passType, m_regCmd[passType].indCropConfig.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].prngSeed.u32All                           = %x\n",
                passType, m_regCmd[passType].prngSeed.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].refCfg0.u32All                            = %x\n",
                passType, m_regCmd[passType].refCfg0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].refCfg1.u32All                            = %x\n",
                passType, m_regCmd[passType].refCfg1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].refCfg2.u32All                            = %x\n",
                passType, m_regCmd[passType].refCfg2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].cropInvertStart.u32All                    = %x\n",
                passType, m_regCmd[passType].cropInvertStart.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].cropInvertEnd.u32All                      = %x\n",
                passType, m_regCmd[passType].cropInvertEnd.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].lnrStartIDXV.u32All                       = %x\n",
                passType, m_regCmd[passType].lnrStartIDXV.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].lnrScale.u32All                           = %x\n",
                passType, m_regCmd[passType].lnrScale.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].cropOutVert.u32All                        = %x\n",
                passType, m_regCmd[passType].cropOutVert.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].refYCfg.u32All                            = %x\n",
                passType, m_regCmd[passType].refYCfg.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib0.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib1.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib2.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib3.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib3.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib4.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib4.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib5.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib5.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib6.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib6.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib7.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib7.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib8.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib8.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib9.u32All                    = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib9.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib10.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib10.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib11.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib12.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib12.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib12.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib13.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib13.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib14.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib14.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib15.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib15.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpYContrib16.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpYContrib16.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribY0.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribY0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribY1.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribY1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribY2.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribY2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribY3.u32All                   = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribY3.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribCB0.u32All                  = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribCB0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribCB1.u32All                  = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribCB1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribCB2.u32All                  = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribCB2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribCB3.u32All                  = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribCB3.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribCR0.u32All                  = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribCR0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribCR1.u32All                  = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribCR1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribCR2.u32All                  = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribCR2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpCContribCR3.u32All                  = %x\n",
                passType, m_regCmd[passType].tdNtNpCContribCR3.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpUVLimit.u32All                      = %x\n",
                passType, m_regCmd[passType].tdNtNpUVLimit.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpTopLimit.u32All                     = %x\n",
                passType, m_regCmd[passType].tdNtNpTopLimit.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtNpBottomLimit.u32All                  = %x\n",
                passType, m_regCmd[passType].tdNtNpBottomLimit.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtLnrLutY0.u32All                       = %x\n",
                passType, m_regCmd[passType].tdNtLnrLutY0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtLnrLutY1.u32All                       = %x\n",
                passType, m_regCmd[passType].tdNtLnrLutY1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtLnrLutY2.u32All                       = %x\n",
                passType, m_regCmd[passType].tdNtLnrLutY2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtLnrLutY3.u32All                       = %x\n",
                passType, m_regCmd[passType].tdNtLnrLutY3.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtLnrLutC0.u32All                       = %x\n",
                passType, m_regCmd[passType].tdNtLnrLutC0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtLnrLutC1.u32All                       = %x\n",
                passType, m_regCmd[passType].tdNtLnrLutC1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtLnrLutC2.u32All                       = %x\n",
                passType, m_regCmd[passType].tdNtLnrLutC2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].tdNtLnrLutC3.u32All                       = %x\n",
                passType, m_regCmd[passType].tdNtLnrLutC3.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsY0.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsY0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsY1.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsY1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsY2.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsY2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsY3.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsY3.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsY4.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsY4.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsY5.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsY5.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsY6.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsY6.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsY7.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsY7.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsY8.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsY8.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsOOFY.u32All               = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsOOFY.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsC0.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsC0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsC1.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsC1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsC2.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsC2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsC3.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsC3.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsC4.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsC4.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsC5.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsC5.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsC6.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsC6.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsC7.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsC7.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsC8.u32All                 = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsC8.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsDecisionParamsOOFC.u32All               = %x\n",
                passType, m_regCmd[passType].fsDecisionParamsOOFC.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].a3T1T2Scale.u32All                        = %x\n",
                passType, m_regCmd[passType].a3T1T2Scale.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].a3T1OFFS.u32All                           = %x\n",
                passType, m_regCmd[passType].a3T1OFFS.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].a3T2OFFS.u32All                           = %x\n",
                passType, m_regCmd[passType].a3T2OFFS.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].a2MinMax.u32All                           = %x\n",
                passType, m_regCmd[passType].a2MinMax.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].a2Slope.u32All                            = %x\n",
                passType, m_regCmd[passType].a2Slope.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsToA1A4Map0.u32All                       = %x\n",
                passType, m_regCmd[passType].fsToA1A4Map0.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsToA1A4Map1.u32All                       = %x\n",
                passType, m_regCmd[passType].fsToA1A4Map1.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsToA1A4Map2.u32All                       = %x\n",
                passType, m_regCmd[passType].fsToA1A4Map2.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsToA1A4Map3.u32All                       = %x\n",
                passType, m_regCmd[passType].fsToA1A4Map3.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsToA1A4Map4.u32All                       = %x\n",
                passType, m_regCmd[passType].fsToA1A4Map4.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsToA1A4Map5.u32All                       = %x\n",
                passType, m_regCmd[passType].fsToA1A4Map5.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsToA1A4Map6.u32All                       = %x\n",
                passType, m_regCmd[passType].fsToA1A4Map6.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsToA1A4Map7.u32All                       = %x\n",
                passType, m_regCmd[passType].fsToA1A4Map7.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].fsToA1A4Map8.u32All                       = %x\n",
                passType, m_regCmd[passType].fsToA1A4Map8.u32All);
            CamX::OsUtils::FPrintF(pFile, "m_regCmd[%d].constantBlendingFactor.u32All             = %x\n",
                passType, m_regCmd[passType].constantBlendingFactor.u32All);
            CamX::OsUtils::FPrintF(pFile, "\n");
        }
        CamX::OsUtils::FPrintF(pFile, "\n\n");
        CamX::OsUtils::FClose(pFile);
    }

    for (UINT passType = 0; passType < PASS_NAME_MAX; passType++)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].config0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].config0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].config1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].config1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].erodeConfig.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].erodeConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].dilateConfig.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].dilateConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].cropInHorizStart.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].cropInHorizStart.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].cropInHorizEnd.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].cropInHorizEnd.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].lnrStartIDXH.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].lnrStartIDXH.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].usCrop.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].usCrop.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].indCropConfig.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].indCropConfig.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].prngSeed.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].prngSeed.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].refCfg0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].refCfg0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].refCfg1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].refCfg1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].refCfg2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].refCfg2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].cropInvertStart.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].cropInvertStart.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].cropInvertEnd.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].cropInvertEnd.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].lnrStartIDXV.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].lnrStartIDXV.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].lnrScale.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].lnrScale.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].cropOutVert.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].cropOutVert.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].refYCfg.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].refYCfg.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib3.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib3.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib4.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib4.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib5.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib5.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib6.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib6.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib7.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib7.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib8.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib8.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib9.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib9.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib10.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib10.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib11.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib11.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib12.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib12.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib13.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib13.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib14.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib14.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib15.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib15.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpYContrib16.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpYContrib16.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribY0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribY0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribY1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribY1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribY2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribY2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribY3.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribY3.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribCB0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribCB0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribCB1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribCB1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribCB2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribCB2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribCB3.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribCB3.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribCR0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribCR0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribCR1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribCR1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribCR2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribCR2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpCContribCR3.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpCContribCR3.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpUVLimit.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpUVLimit.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpTopLimit.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpTopLimit.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtNpBottomLimit.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtNpBottomLimit.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtLnrLutY0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtLnrLutY0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtLnrLutY1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtLnrLutY1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtLnrLutY2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtLnrLutY2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtLnrLutY3.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtLnrLutY3.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtLnrLutC0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtLnrLutC0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtLnrLutC1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtLnrLutC1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtLnrLutC2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtLnrLutC2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].tdNtLnrLutC3.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].tdNtLnrLutC3.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsY0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsY0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsY1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsY1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsY2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsY2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsY3.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsY3.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsY4.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsY4.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsY5.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsY5.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsY6.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsY6.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsY7.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsY7.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsY8.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsY8.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsOOFY.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsOOFY.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsC0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsC0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsC1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsC1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsC2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsC2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsC3.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsC3.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsC4.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsC4.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsC5.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsC5.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsC6.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsC6.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsC7.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsC7.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsC8.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsC8.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsDecisionParamsOOFC.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsDecisionParamsOOFC.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].a3T1T2Scale.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].a3T1T2Scale.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].a3T1OFFS.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].a3T1OFFS.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].a3T2OFFS.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].a3T2OFFS.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].a2MinMax.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].a2MinMax.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].a2Slope.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].a2Slope.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsToA1A4Map0.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsToA1A4Map0.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsToA1A4Map1.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsToA1A4Map1.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsToA1A4Map2.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsToA1A4Map2.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsToA1A4Map3.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsToA1A4Map3.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsToA1A4Map4.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsToA1A4Map4.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsToA1A4Map5.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsToA1A4Map5.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsToA1A4Map6.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsToA1A4Map6.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsToA1A4Map7.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsToA1A4Map7.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].fsToA1A4Map8.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].fsToA1A4Map8.u32All);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
                      "m_regCmd[%d].constantBlendingFactor.u32All = 0x%x",
                      passType,
                      m_regCmd[passType].constantBlendingFactor.u32All);
    }

    return;
}
CAMX_NAMESPACE_END
