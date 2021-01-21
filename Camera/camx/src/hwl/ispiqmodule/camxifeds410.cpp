////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifeds410.cpp
/// @brief CAMXIFEDS410 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifeds410.h"
#include "camxnode.h"
#include "camxtuningdatamanager.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEDS410::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFEDS410(pCreateData);
        if (NULL == pCreateData->pModule)
        {
            result = CamxResultENoMemory;
            CAMX_ASSERT_ALWAYS_MESSAGE("Memory allocation failed");
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_LOG_ERROR(CamxLogGroupISP, "Input Null pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::ConfigureDS4Registers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDS410::ConfigureDS4Registers()
{
    /// @todo (CAMX-919) Replace hardcoded filter coefficients from Chromatix
    // Flush count is updated as per HLD guidelines
    m_regCmd.DS4.lumaConfig.bitfields.FLUSH_PACE_CNT   = 3;

    m_regCmd.DS4.lumaConfig.bitfields.HEIGHT           = Utils::AlignGeneric32(m_pState->MNDSOutput.height, 2);
    m_regCmd.DS4.coefficients.bitfields.COEFF_07 =
        m_pDc4Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_07;
    m_regCmd.DS4.coefficients.bitfields.COEFF_16 =
        m_pDc4Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_16;
    m_regCmd.DS4.coefficients.bitfields.COEFF_25 =
        m_pDc4Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_25;
    m_regCmd.DS4.chromaConfig.bitfields.FLUSH_PACE_CNT = 3;
    // Input to this module is YUV420
    m_regCmd.DS4.chromaConfig.bitfields.HEIGHT         = m_regCmd.DS4.lumaConfig.bitfields.HEIGHT / YUV420SubsampleFactor;

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "<Scaler> DS4 path Y_Height %d Cr_Height %d",
                     m_regCmd.DS4.lumaConfig.bitfields.HEIGHT,
                     m_regCmd.DS4.chromaConfig.bitfields.HEIGHT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::ConfigureDS16Registers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDS410::ConfigureDS16Registers()
{
    /// @todo (CAMX-919) Replace hardcoded filter coefficients from Chromatix
    // Flush count is updated as per HLD guidelines
    m_regCmd.DS16.lumaConfig.bitfields.FLUSH_PACE_CNT   = 0xf;

    // Input to DS16 is downsampled by DS4 by 1:4
    m_regCmd.DS16.lumaConfig.bitfields.HEIGHT           = Utils::AlignGeneric32(m_pState->MNDSOutput.height, 2);

    m_regCmd.DS16.coefficients.bitfields.COEFF_07 =
        m_pDc16Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_07;
    m_regCmd.DS16.coefficients.bitfields.COEFF_16 =
        m_pDc16Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_16;
    m_regCmd.DS16.coefficients.bitfields.COEFF_25 =
        m_pDc16Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_25;
    m_regCmd.DS16.chromaConfig.bitfields.FLUSH_PACE_CNT = 0xf;
    // Input to this module is YUV420 and need to account for downscale by DS4
    m_regCmd.DS16.chromaConfig.bitfields.HEIGHT         = m_regCmd.DS16.lumaConfig.bitfields.HEIGHT / YUV420SubsampleFactor;

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "<Scaler> DS16 path Y_Height %d Cr_Height %d",
                     m_regCmd.DS16.lumaConfig.bitfields.HEIGHT,
                     m_regCmd.DS16.chromaConfig.bitfields.HEIGHT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::ConfigureDisplayDS4Registers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDS410::ConfigureDisplayDS4Registers()
{
    /// @todo (CAMX-919) Replace hardcoded filter coefficients from Chromatix
    // Flush count is updated as per HLD guidelines
    m_regCmd.displayDS4.lumaConfig.bitfields.FLUSH_PACE_CNT   = 3;

    m_regCmd.displayDS4.lumaConfig.bitfields.HEIGHT           = Utils::AlignGeneric32(m_pState->MNDSOutput.height, 2);
    /// @todo (CAMX-4027) Changes for new IFE display o/p. Is chromatix the same ?
    m_regCmd.displayDS4.coefficients.bitfields.COEFF_07 =
        m_pDc4Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_07;
    m_regCmd.displayDS4.coefficients.bitfields.COEFF_16 =
        m_pDc4Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_16;
    m_regCmd.displayDS4.coefficients.bitfields.COEFF_25 =
        m_pDc4Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_25;
    m_regCmd.displayDS4.chromaConfig.bitfields.FLUSH_PACE_CNT = 3;
    // Input to this module is YUV420
    m_regCmd.displayDS4.chromaConfig.bitfields.HEIGHT         = m_regCmd.displayDS4.lumaConfig.bitfields.HEIGHT /
                                                                YUV420SubsampleFactor;

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "<Scaler> Display DS4 path Y_Height %d Cr_Height %d",
        m_regCmd.displayDS4.lumaConfig.bitfields.HEIGHT,
        m_regCmd.displayDS4.chromaConfig.bitfields.HEIGHT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::ConfigureDisplayDS16Registers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDS410::ConfigureDisplayDS16Registers()
{
    /// @todo (CAMX-919) Replace hardcoded filter coefficients from Chromatix
    // Flush count is updated as per HLD guidelines
    m_regCmd.displayDS16.lumaConfig.bitfields.FLUSH_PACE_CNT   = 0xf;

    // Input to DS16Disp is downsampled by DS4 by 1:4
    m_regCmd.displayDS16.lumaConfig.bitfields.HEIGHT           = Utils::AlignGeneric32(m_pState->MNDSOutput.height, 2);

    /// @todo (CAMX-4027) Changes for new IFE display o/p. Is chromatix the same ?
    m_regCmd.displayDS16.coefficients.bitfields.COEFF_07        =
        m_pDc16Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_07;
    m_regCmd.displayDS16.coefficients.bitfields.COEFF_16        =
        m_pDc16Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_16;
    m_regCmd.displayDS16.coefficients.bitfields.COEFF_25        =
        m_pDc16Chromatix->chromatix_ds4to1v10_reserve.mod_ds4to1v10_pass_reserve_data->pass_data.coeff_25;
    m_regCmd.displayDS16.chromaConfig.bitfields.FLUSH_PACE_CNT  = 0xf;
    // Input to this module is YUV420 and need to account for downscale by DS4
    m_regCmd.displayDS16.chromaConfig.bitfields.HEIGHT          = m_regCmd.displayDS16.lumaConfig.bitfields.HEIGHT /
                                                                  YUV420SubsampleFactor;

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "<Scaler> Display DS16 path Y_Height %d Cr_Height %d",
        m_regCmd.displayDS16.lumaConfig.bitfields.HEIGHT,
        m_regCmd.displayDS16.chromaConfig.bitfields.HEIGHT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDS410::DumpRegConfig()
{
    if (IFEPipelinePath::DS4Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 lumaConfig          [0x%x] ", m_regCmd.DS4.lumaConfig);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 Filter coefficients [0x%x] ", m_regCmd.DS4.coefficients);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS4 chromaConfig        [0x%x] ", m_regCmd.DS4.chromaConfig);
    }

    if (IFEPipelinePath::DS16Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 lumaConfig          [0x%x] ", m_regCmd.DS16.lumaConfig);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 Filter coefficients [0x%x] ", m_regCmd.DS16.coefficients);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "DS16 chromaConfig        [0x%x] ", m_regCmd.DS16.chromaConfig);
    }

    if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS4 lumaConfig          [0x%x] ", m_regCmd.displayDS4.lumaConfig);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS4 Filter coefficients [0x%x] ", m_regCmd.displayDS4.coefficients);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS4 chromaConfig        [0x%x] ", m_regCmd.displayDS4.chromaConfig);
    }

    if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS16 lumaConfig          [0x%x] ", m_regCmd.displayDS16.lumaConfig);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS16 Filter coefficients [0x%x] ", m_regCmd.displayDS16.coefficients);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "Display DS16 chromaConfig        [0x%x] ", m_regCmd.displayDS16.chromaConfig);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEDS410::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDS410::RunCalculation(
    ISPInputData* pInputData)
{
    if (IFEPipelinePath::DS4Path  == m_modulePath)
    {
        ConfigureDS4Registers();
    }

    if (IFEPipelinePath::DS16Path  == m_modulePath)
    {
        ConfigureDS16Registers();
    }

    if (IFEPipelinePath::DisplayDS4Path  == m_modulePath)
    {
        ConfigureDisplayDS4Registers();
    }

    if (IFEPipelinePath::DisplayDS16Path  == m_modulePath)
    {
        ConfigureDisplayDS16Registers();
    }

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEDS410::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    IFE_IFE_0_VFE_MODULE_ZOOM_EN* pModuleEnable  = &pInputData->pCalculatedData->moduleEnable.frameProcessingModuleConfig;
    IFE_IFE_0_VFE_MODULE_DISP_EN* pDisplayEnable =
        &pInputData->pCalculatedData->moduleEnable.frameProcessingDisplayModuleConfig;

    IFEScalerOutput* pCalculatedScalerOutput = &pInputData->pCalculatedData->scalerOutput[0];

    if (IFEPipelinePath::DS4Path  == m_modulePath)
    {
        pModuleEnable->bitfields.DS_4TO1_Y_1ST_EN = m_pState->moduleFlags.isDS4PathEnable;
        pModuleEnable->bitfields.DS_4TO1_C_1ST_EN = m_pState->moduleFlags.isDS4PathEnable;

        // DS16 need to know if DS4 path is enabled or not
        pInputData->pCalculatedData->isDS4Enable = m_pState->moduleFlags.isDS4PathEnable;

        // Post Crop module on DS4 path needs to know DS4 output dimension
        pCalculatedScalerOutput[DS4Output].dimension.width  = Utils::AlignGeneric32(
            pCalculatedScalerOutput[FullOutput].dimension.width , DS4Factor * 2 ) / DS4Factor;
        pCalculatedScalerOutput[DS4Output].dimension.height = Utils::AlignGeneric32(
            pCalculatedScalerOutput[FullOutput].dimension.height , DS4Factor * 2 ) / DS4Factor;
        pCalculatedScalerOutput[DS4Output].scalingFactor    = pCalculatedScalerOutput[FullOutput].scalingFactor * DS4Factor;
    }

    if (IFEPipelinePath::DS16Path  == m_modulePath)
    {
        pModuleEnable->bitfields.DS_4TO1_Y_2ND_EN = m_pState->moduleFlags.isDS16PathEnable;
        pModuleEnable->bitfields.DS_4TO1_C_2ND_EN = m_pState->moduleFlags.isDS16PathEnable;

        // Post Crop module on DS16 path needs to know DS4 output dimension
        pCalculatedScalerOutput[DS16Output].dimension.width  = Utils::AlignGeneric32(
            pCalculatedScalerOutput[FullOutput].dimension.width, DS16Factor * 2 ) / DS16Factor;
        pCalculatedScalerOutput[DS16Output].dimension.height = Utils::AlignGeneric32(
            pCalculatedScalerOutput[FullOutput].dimension.height, DS16Factor * 2 ) / DS16Factor;
        pCalculatedScalerOutput[DS16Output].scalingFactor    = pCalculatedScalerOutput[FullOutput].scalingFactor * DS16Factor;
    }

    if (IFEPipelinePath::DisplayDS4Path  == m_modulePath)
    {
        pDisplayEnable->bitfields.DS_4TO1_Y_1ST_EN = m_pState->moduleFlags.isDS4PathEnable;
        pDisplayEnable->bitfields.DS_4TO1_C_1ST_EN = m_pState->moduleFlags.isDS4PathEnable;

        pInputData->pCalculatedData->isDisplayDS4Enable = m_pState->moduleFlags.isDS4PathEnable;

        // Post Crop module on DS4 path needs to know DS4 output dimension
        pCalculatedScalerOutput[DisplayDS4Output].dimension.width  = Utils::AlignGeneric32(
            pCalculatedScalerOutput[DisplayFullOutput].dimension.width , DS4Factor * 2 ) / DS4Factor;
        pCalculatedScalerOutput[DisplayDS4Output].dimension.height = Utils::AlignGeneric32(
            pCalculatedScalerOutput[DisplayFullOutput].dimension.height , DS4Factor * 2 ) / DS4Factor;

        pCalculatedScalerOutput[DisplayDS4Output].scalingFactor =
            pCalculatedScalerOutput[DisplayFullOutput].scalingFactor * DS4Factor;
    }

    if (IFEPipelinePath::DisplayDS16Path  == m_modulePath)
    {
        pDisplayEnable->bitfields.DS_4TO1_Y_2ND_EN = m_pState->moduleFlags.isDS16PathEnable;
        pDisplayEnable->bitfields.DS_4TO1_C_2ND_EN = m_pState->moduleFlags.isDS16PathEnable;

        // Post Crop module on DS16 path needs to know DS4 output dimension
        pCalculatedScalerOutput[DisplayDS16Output].dimension.width  = Utils::AlignGeneric32(
            pCalculatedScalerOutput[DisplayFullOutput].dimension.width, DS16Factor * 2 ) / DS16Factor;
        pCalculatedScalerOutput[DisplayDS16Output].dimension.height = Utils::AlignGeneric32(
            pCalculatedScalerOutput[DisplayFullOutput].dimension.height, DS16Factor * 2 ) / DS16Factor;

        pCalculatedScalerOutput[DisplayDS16Output].scalingFactor =
            pCalculatedScalerOutput[DisplayFullOutput].scalingFactor * DS16Factor;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEDS410::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL                                   result                   = FALSE;
    StreamDimension*                       pHALStream               = &pInputData->pStripeConfig->stream[0];
    IFEScalerOutput*                       pCalculatedScalerOutput  = &pInputData->pCalculatedData->scalerOutput[0];
    TuningDataManager*                     pTuningManager           = NULL;

    pTuningManager = pInputData->pTuningDataManager;
    CAMX_ASSERT(NULL != pTuningManager);

    CAMX_ASSERT(NULL != pInputData->pTuningData);

        /// @todo (CAMX-919) Need to check if chromatix data has changed
    if ((IFEPipelinePath::DS4Path == m_modulePath) || (IFEPipelinePath::DisplayDS4Path == m_modulePath))
    {
        DS4PreCropInfo* pPreCropInfo = &pInputData->pCalculatedData->preCropInfo;
        UINT32          fullPortIdx  = FullOutput;
        UINT32          DS4PortIdx   = DS4Output;
        if (IFEPipelinePath::DisplayDS4Path == m_modulePath)
        {
            pPreCropInfo = &pInputData->pCalculatedData->dispPreCropInfo;
            fullPortIdx  = DisplayFullOutput;
            DS4PortIdx   = DisplayDS4Output;
        }
        UINT32 width  = (pPreCropInfo->YCrop.lastPixel - pPreCropInfo->YCrop.firstPixel) + 1;
        UINT32 height = (pPreCropInfo->YCrop.lastLine -  pPreCropInfo->YCrop.firstLine) + 1;

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Idx: %u lastPixel - first pixel + 1 = width [%d - %d] = %d",
            DS4PortIdx, pPreCropInfo->YCrop.lastPixel, pPreCropInfo->YCrop.firstPixel, width);

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Idx: %u lastline - first line + 1 = height  [%d - %d] = %d",
            DS4PortIdx, pPreCropInfo->YCrop.lastLine, pPreCropInfo->YCrop.firstLine, height);

        if ((width  != m_pState->MNDSOutput.width)                               ||
            (height != m_pState->MNDSOutput.height)                              ||
            (pHALStream[DS4PortIdx].width  != m_pState->DS4PathOutput.width)     ||
            (pHALStream[DS4PortIdx].height != m_pState->DS4PathOutput.height)    ||
            (TRUE == pInputData->forceTriggerUpdate))
        {
            if (TRUE == pTuningManager->IsValidChromatix())
            {
                m_pDc4Chromatix = pTuningManager->GetChromatix()->GetModule_ds4to1v10_ife_full_dc4(
                    reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                    pInputData->pTuningData->noOfSelectionParameter);
            }
            if (pInputData->pStripeConfig->cropType == CropTypeCentered)
            {
                m_pState->MNDSOutput.width  = width;
                m_pState->MNDSOutput.height = height;
            }
            else
            {
                m_pState->MNDSOutput.width  = pCalculatedScalerOutput[fullPortIdx].dimension.width;
                m_pState->MNDSOutput.height = pCalculatedScalerOutput[fullPortIdx].dimension.height;
            }

            m_pState->DS4PathOutput.width  = pHALStream[DS4PortIdx].width;
            m_pState->DS4PathOutput.height = pHALStream[DS4PortIdx].height;

            m_pState->moduleFlags.isDS4PathEnable = TRUE;

            result = TRUE;
        }

        if (TRUE == pInputData->pStripeConfig->overwriteStripes)
        {
            ISPInternalData* pFrameLevel = pInputData->pStripeConfig->pFrameLevelData->pFrameData;
            if (TRUE == pFrameLevel->scalerDependencyChanged[DS4PortIdx])
            {
                m_moduleEnable = TRUE;
                result = TRUE;
            }

            m_pState->MNDSOutput.width  = width;
            m_pState->MNDSOutput.height = height;
        }
        pInputData->pCalculatedData->cropDependencyChanged[DS4PortIdx] = result;
    }

    if ((IFEPipelinePath::DS16Path == m_modulePath) || (IFEPipelinePath::DisplayDS16Path == m_modulePath))
    {
        DS4PreCropInfo* pPreCropInfo     = &pInputData->pCalculatedData->preCropInfo;
        DS4PreCropInfo* pPreCropInfoDS16 = &pInputData->pCalculatedData->preCropInfoDS16;
        UINT32          fullPortIdx      = FullOutput;
        UINT32          DS16PortIdx      = DS16Output;
        if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
        {
            pPreCropInfo     = &pInputData->pCalculatedData->dispPreCropInfo;
            pPreCropInfoDS16 = &pInputData->pCalculatedData->dispPreCropInfoDS16;
            fullPortIdx      = DisplayFullOutput;
            DS16PortIdx      = DisplayDS16Output;
        }
        UINT32 width  = (pInputData->pStripeConfig->cropType == CropTypeCentered) ?
            (pPreCropInfo->YCrop.lastPixel - pPreCropInfo->YCrop.firstPixel) + 1 :
            (pPreCropInfoDS16->YCrop.lastPixel - pPreCropInfoDS16->YCrop.firstPixel) + 1;
        UINT32 height = (pInputData->pStripeConfig->cropType == CropTypeCentered) ?
            (pPreCropInfo->YCrop.lastLine - pPreCropInfo->YCrop.firstLine) + 1 :
            (pPreCropInfoDS16->YCrop.lastLine - pPreCropInfoDS16->YCrop.firstLine) + 1;

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Idx: %u lastPixel - first pixel + 1 = width [%d - %d] = %d",
            DS16PortIdx, pPreCropInfo->YCrop.lastPixel, pPreCropInfo->YCrop.firstPixel, width);

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Idx: %u lastline - first line + 1 = height [%d - %d] = %d",
            DS16PortIdx, pPreCropInfo->YCrop.lastLine, pPreCropInfo->YCrop.firstLine, height);

        if ((width  != m_pState->MNDSOutput.width)     ||
            (height != m_pState->MNDSOutput.height)    ||
            (pHALStream[DS16PortIdx].width  != m_pState->DS16PathOutput.width)  ||
            (pHALStream[DS16PortIdx].height != m_pState->DS16PathOutput.height) ||
            (TRUE == pInputData->forceTriggerUpdate))
        {
            if (TRUE == pTuningManager->IsValidChromatix())
            {
                m_pDc16Chromatix = pTuningManager->GetChromatix()->GetModule_ds4to1v10_ife_dc4_dc16(
                    reinterpret_cast<TuningMode*>(&pInputData->pTuningData->TuningMode[0]),
                    pInputData->pTuningData->noOfSelectionParameter);
            }

            if (pInputData->pStripeConfig->cropType == CropTypeCentered)
            {
                m_pState->MNDSOutput.width  =  Utils::AlignGeneric32(width, DS4Factor ) / DS4Factor;

                m_pState->MNDSOutput.height = Utils::AlignGeneric32(height, DS4Factor ) / DS4Factor;

            }
            else
            {
                m_pState->MNDSOutput.width  =  pCalculatedScalerOutput[fullPortIdx].dimension.width;
                m_pState->MNDSOutput.height =  pCalculatedScalerOutput[fullPortIdx].dimension.height;
            }

            m_pState->DS16PathOutput.width  = pHALStream[DS16PortIdx].width;
            m_pState->DS16PathOutput.height = pHALStream[DS16PortIdx].height;

            m_pState->moduleFlags.isDS16PathEnable = TRUE;

            result = TRUE;
        }

        if (TRUE == pInputData->pStripeConfig->overwriteStripes)
        {
            ISPInternalData* pFrameLevel = pInputData->pStripeConfig->pFrameLevelData->pFrameData;
            if (TRUE == pFrameLevel->scalerDependencyChanged[DS16PortIdx])
            {
                m_moduleEnable = TRUE;
                result = TRUE;
            }

            m_pState->MNDSOutput.width  = width;
            m_pState->MNDSOutput.height = height;
        }
        pInputData->pCalculatedData->cropDependencyChanged[DS16PortIdx] = result;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEDS410::ValidateDependenceParams(
    ISPInputData* pInputData)
{
    CamxResult       result                  = CamxResultSuccess;
    IFEScalerOutput* pCalculatedScalerOutput = &pInputData->pCalculatedData->scalerOutput[0];

    // Full path MNDS output Validation for DS4 path
    if ((IFEPipelinePath::DS4Path == m_modulePath)                 &&
        (0 != pInputData->pStripeConfig->stream[DS4Output].width)  &&
        (0 != pInputData->pStripeConfig->stream[DS4Output].height) &&
        ((0 == pCalculatedScalerOutput[FullOutput].dimension.width)  ||
         (0 == pCalculatedScalerOutput[FullOutput].dimension.height)))
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Full path MNDS ouput for DS4 path");
    }

    // Full path MNDS output Validation, Both MNDS and DS4 should be enabled for DS16 path
    if ((IFEPipelinePath::DS16Path == m_modulePath)                 &&
        (0 != pInputData->pStripeConfig->stream[DS16Output].width)  &&
        (0 != pInputData->pStripeConfig->stream[DS16Output].height) &&
        ((0     == pCalculatedScalerOutput[FullOutput].dimension.width)  ||
         (0     == pCalculatedScalerOutput[FullOutput].dimension.height) ||
         (FALSE == pInputData->pCalculatedData->isDS4Enable)))
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Full path MNDS ouput for DS16 path");
    }

    // Full Display path MNDS output Validation for DS4 path
    if ((IFEPipelinePath::DisplayDS4Path == m_modulePath)                 &&
        (0 != pInputData->pStripeConfig->stream[DisplayDS4Output].width)  &&
        (0 != pInputData->pStripeConfig->stream[DisplayDS4Output].height) &&
        ((0 == pCalculatedScalerOutput[DisplayFullOutput].dimension.width)  ||
         (0 == pCalculatedScalerOutput[DisplayFullOutput].dimension.height)))
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Display Full path MNDS ouput for Display DS4 path");
    }

    // Full Display path MNDS output Validation, Both Display MNDS and Display DS4 should be enabled for DS16 path
    if ((IFEPipelinePath::DisplayDS16Path == m_modulePath)                 &&
        (0 != pInputData->pStripeConfig->stream[DisplayDS16Output].width)  &&
        (0 != pInputData->pStripeConfig->stream[DisplayDS16Output].height) &&
        ((0     == pCalculatedScalerOutput[DisplayFullOutput].dimension.width)  ||
         (0     == pCalculatedScalerOutput[DisplayFullOutput].dimension.height) ||
         (FALSE == pInputData->pCalculatedData->isDisplayDS4Enable)))
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Display Full path MNDS ouput for Display DS16 path");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEDS410::Execute(
    ISPInputData* pInputData)
{
    CamxResult result;

    CAMX_ASSERT(static_cast<UINT>(m_modulePath) < IFEMaxNonCommonPaths);

    if (NULL != pInputData)
    {
        m_pState = &pInputData->pStripeConfig->stateDS[static_cast<UINT>(m_modulePath)];

        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);
        if (CamxResultSuccess == result)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                RunCalculation(pInputData);
                result = CreateCmdList(pInputData);
            }

            if (CamxResultSuccess == result)
            {
                UpdateIFEInternalData(pInputData);
            }
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEDS410::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result;

    CAMX_ASSERT(static_cast<UINT>(m_modulePath) < IFEMaxNonCommonPaths);

    if (NULL != pInputData)
    {
        m_pState = &pInputData->pStripeConfig->stateDS[static_cast<UINT>(m_modulePath)];

        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);
        if (CamxResultSuccess == result)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                RunCalculation(pInputData);
            }

            if (CamxResultSuccess == result)
            {
                UpdateIFEInternalData(pInputData);
            }
        }
    }
    else
    {
        result = CamxResultEInvalidArg;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEDS410::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;
        // Write DS4 output path registers
        if ((IFEPipelinePath::DS4Path == m_modulePath))
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_DS_4TO1_Y_1ST_CFG,
                                                  (sizeof(IFEDS4Reg) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.DS4));

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to configure (Video) DS4 path Register");
            }
        }

        // Write DS16 output path registers
        if (IFEPipelinePath::DS16Path == m_modulePath)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_DS_4TO1_Y_2ND_CFG,
                                                  (sizeof(IFEDS16Reg) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.DS16));
            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to configure (Video) DS16 path Register");
            }
        }

        // Write Display DS4 output path registers
        if ((IFEPipelinePath::DisplayDS4Path == m_modulePath))
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_DISP_DS_4TO1_Y_1ST_CFG,
                                                  (sizeof(IFEDisplayDS4Reg) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.displayDS4));

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to configure Display DS4 path Register");
            }
        }

        // Write Display DS16 output path registers
        if (IFEPipelinePath::DisplayDS16Path == m_modulePath)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_DISP_DS_4TO1_Y_2ND_CFG,
                                                  (sizeof(IFEDisplayDS16Reg) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.displayDS16));
            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to configure Display DS16 path Register");
            }
        }
    }
    else
    {
        result = CamxResultEInvalidPointer;
        CAMX_ASSERT_ALWAYS_MESSAGE("Invalid Input pointer");
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEDS410::IFEDS410
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEDS410::IFEDS410(
    IFEModuleCreateData* pCreateData)
{
    m_type           = ISPIQModuleType::IFEDS4;
    m_moduleEnable   = TRUE;
    m_32bitDMILength = 0;
    m_64bitDMILength = 0;
    m_cmdLength      =
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEDS4Reg) / RegisterWidthInBytes) +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEDS16Reg) / RegisterWidthInBytes);

    m_modulePath = pCreateData->pipelineData.IFEPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEDS410::~IFEDS410
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEDS410::~IFEDS410()
{
}

CAMX_NAMESPACE_END
