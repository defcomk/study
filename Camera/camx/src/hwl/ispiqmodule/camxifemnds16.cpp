////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxifemnds16.cpp
/// @brief CAMXIFEMNDS16 class implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "camxifemnds16.h"
#include "camxhal3module.h"

CAMX_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::Create
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEMNDS16::Create(
    IFEModuleCreateData* pCreateData)
{
    CamxResult result = CamxResultSuccess;

    if (NULL != pCreateData)
    {
        pCreateData->pModule = CAMX_NEW IFEMNDS16(pCreateData);
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
// IFEMNDS16::CalculateScalerOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEMNDS16::CalculateScalerOutput(
    ISPInputData* pInputData)
{
    UINT32 downScalingFactor;
    UINT32 cropScaleFactor;
    UINT32 outputWidth;
    UINT32 outputHeight;
    FLOAT  inAspectRatio;
    FLOAT  outAspectRatio;
    UINT32 maxOutputWidth;

    CAMX_ASSERT_MESSAGE(0 != m_pState->cropWindow.height, "Invalid Crop height");
    CAMX_ASSERT_MESSAGE(0 != m_pState->streamDimension.height, "Invalid output height");
    CAMX_ASSERT_MESSAGE(0 != m_pState->inputHeight, "Invalid input height");
    const StaticSettings* pStaticSettings = HwEnvironment::GetInstance()->GetStaticSettings();

    // Calculate the scaling factor, based on the crop window and Sensor output.
    // Every time the crop_window decreases the crop_factor increases (Scalar output increases)
    const UINT32 cropWidthScaleFactor   = (m_pState->inputWidth * Q12) / m_pState->cropWindow.width;
    const UINT32 cropheightScaleFactor  = (m_pState->inputHeight * Q12) / m_pState->cropWindow.height;

    // Calculate scaler input and IFE output aspect ratio, to calculate scaler output
    outAspectRatio = static_cast<FLOAT>(m_pState->streamDimension.width) / m_pState->streamDimension.height;
    inAspectRatio  = static_cast<FLOAT>(m_pState->inputWidth) / m_pState->inputHeight;

    // 1. Calculate Scaler output based on IFE output based input aspect ratio.
    if (inAspectRatio == outAspectRatio)
    {
        cropScaleFactor = Utils::MinUINT32(cropWidthScaleFactor, cropheightScaleFactor);
        outputWidth     = (m_pState->streamDimension.width * cropScaleFactor) / Q12;
        outputHeight    = (m_pState->streamDimension.height * cropScaleFactor) / Q12;
    }
    else if (inAspectRatio < outAspectRatio)
    {
        cropScaleFactor = cropWidthScaleFactor;
        outputWidth     = (m_pState->streamDimension.width * cropScaleFactor) / Q12;
        outputHeight    = (outputWidth * m_pState->inputHeight) / m_pState->inputWidth;
    }
    else
    {
        cropScaleFactor = cropheightScaleFactor;
        outputHeight    = (m_pState->streamDimension.height * cropScaleFactor) / Q12;
        outputWidth     = (outputHeight * m_pState->inputWidth) / m_pState->inputHeight;
    }

    // 2. Cannot do upscale, HW limitation
    if ((outputWidth > m_pState->inputWidth) || (outputHeight > m_pState->inputHeight))
    {
        outputWidth     = m_pState->inputWidth;
        outputHeight    = m_pState->inputHeight;
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "Path %d[0 - FD, 1 Full], output dimension > input, set it to input dim [%d * %d]\n",
            m_output,
            outputWidth,
            outputHeight);
    }
    // 3. Check scale factor limitation, Output dimension can not be scaled beyond scaler capability
    if (FDOutput == m_output)
    {
        UINT32 maxFDScalerWidth = pInputData->maxOutputWidthFD * 2;
        if (IFEModuleMode::SingleIFENormal == pInputData->pipelineIFEData.moduleMode)
        {
            maxFDScalerWidth = pInputData->maxOutputWidthFD;
        }
        if (outputWidth > maxFDScalerWidth)
        {
            outputWidth = maxFDScalerWidth;
            outputHeight  = (outputWidth * m_pState->inputHeight) / m_pState->inputWidth;
            if (outputHeight < m_pState->streamDimension.height)
            {
                outputHeight= m_pState->streamDimension.height;
            }
        }
    }

    if ((outputWidth * MNDS16MaxScaleFactor) < m_pState->inputWidth)
    {
        outputWidth     = (m_pState->inputWidth + MNDS16MaxScaleFactor - 1) / MNDS16MaxScaleFactor;
        outputHeight    = (outputWidth * m_pState->inputHeight) / m_pState->inputWidth;
    }
    if ((outputHeight * MNDS16MaxScaleFactor) < m_pState->inputHeight)
    {
        outputHeight    = (m_pState->inputHeight + MNDS16MaxScaleFactor - 1) / MNDS16MaxScaleFactor;
        outputWidth     = (outputHeight * m_pState->inputWidth) / m_pState->inputHeight;
    }

    // 4. Check scale ratio limitation, Scaling between 105 to 100 could introduce artifacts
    downScalingFactor = (m_pState->inputWidth * 100) / outputWidth;

    if ((downScalingFactor < ScaleRatioLimit) && (downScalingFactor >= 100))
    {
        if (Q12 != cropScaleFactor)
        {
            outputWidth  = (m_pState->inputWidth * 100) / ScaleRatioLimit;
            outputHeight = (m_pState->inputHeight * 100) / ScaleRatioLimit;

            if ((outputWidth  < m_pState->streamDimension.width) ||
                 (outputHeight < m_pState->streamDimension.height))
            {
                outputWidth     = m_pState->inputWidth;
                outputHeight    = m_pState->inputHeight;
            }
            CAMX_LOG_VERBOSE(CamxLogGroupISP, "Path %d[0-FD,1-Full], Scale ration 100-105, Dimension [%d * %d]\n",
                m_output,
                outputWidth,
                outputHeight);
        }
        else
        {
            outputWidth     = m_pState->inputWidth;
            outputHeight    = m_pState->inputHeight;
        }
    }
    if (TRUE == pStaticSettings->capResolutionForSingleIFE)
    {
        maxOutputWidth = outputWidth;
        switch (m_modulePath)
        {
            case IFEPipelinePath::FDPath:
                maxOutputWidth = pInputData->maxOutputWidthFD;
                break;

            case IFEPipelinePath::FullPath:
                maxOutputWidth = IFEMaxOutputWidthFull;
                break;

            default:
                maxOutputWidth = outputWidth;
                CAMX_ASSERT_ALWAYS_MESSAGE("Invalid path");
                break;
        }

        if (outputWidth > maxOutputWidth)
        {
            outputWidth = maxOutputWidth;
            outputHeight = m_pState->inputHeight * maxOutputWidth / m_pState->inputWidth;
        }
    }
    // 5. make sure its even number so CbCr will match Y instead of rounding down
    m_pState->MNDSOutput.dimension.height   = Utils::EvenFloorUINT32(outputHeight);
    m_pState->MNDSOutput.dimension.width    = Utils::EvenFloorUINT32(outputWidth);


    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Path %d[0 - FD, 1 Full], cropType %d Scaler output[%d * %d]\n",
                     m_output, pInputData->pStripeConfig->cropType,
                     outputWidth,
                     outputHeight);

    m_pState->MNDSOutput.input.width    = m_pState->inputWidth;
    m_pState->MNDSOutput.input.height   = m_pState->inputHeight;

    // Only if this is left or right stripe processing calculate MNDS padding to handle boundary pixels. This is currently
    // used in dual IFE, but single IFE mode also can take advantage of it if it becomes necessary.
    if (((CropTypeFromLeft == pInputData->pStripeConfig->cropType) ||
        (CropTypeFromRight == pInputData->pStripeConfig->cropType)) &&
        (FALSE == m_pInputData->pStripeConfig->overwriteStripes))
    {
        ISPInternalData*    pFrameLevel = pInputData->pStripeConfig->pFrameLevelData->pFrameData;
        UINT                scaleM      = pFrameLevel->scalerOutput[m_output].dimension.width;
        UINT                scaleN      = pFrameLevel->scalerOutput[m_output].input.width;
        stripeROI_1D        input_ROI   = {0, 0};
        stripeROI_1D        output_ROI  = {0, 0};

        if ((m_pState->MNDSOutput.dimension.width * static_cast<FLOAT>(scaleN) /
                scaleM) > static_cast<FLOAT>(m_pState->inputWidth))
        {
            m_pState->MNDSOutput.dimension.width =
                static_cast<UINT>(m_pState->inputWidth * static_cast<FLOAT>(scaleM) / scaleN);
        }
        m_pState->MNDSOutput.dimension.width = Utils::EvenFloorUINT32(m_pState->MNDSOutput.dimension.width);

        if (CropTypeFromRight == pInputData->pStripeConfig->cropType)
        {
            input_ROI.start_x   = static_cast<uint16_t>(scaleN - m_pState->inputWidth);
            input_ROI.end_x     = static_cast<uint16_t>(scaleN - 1);
            output_ROI.start_x  = static_cast<uint16_t>(scaleM - m_pState->MNDSOutput.dimension.width);
            output_ROI.end_x    = static_cast<uint16_t>(scaleM - 1);
        }
        else if (CropTypeFromLeft == pInputData->pStripeConfig->cropType)
        {
            input_ROI.start_x   = 0;
            input_ROI.end_x     = static_cast<uint16_t>(m_pState->inputWidth - 1);
            output_ROI.start_x  = 0;
            output_ROI.end_x    = static_cast<uint16_t>(m_pState->MNDSOutput.dimension.width - 1);
        }

        // MNScaleDownInStruct_V16_1D* pMNDSOut = NULL;
        if (FullOutput == m_output)
        {
            // pMNDSOut = &pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_full;
            m_pState->MNDSOutput.mnds_config_y = pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_full;
            m_pState->MNDSOutput.mnds_config_c = pInputData->pStripeConfig->pStripeOutput->mndsConfig_c_full;
        }
        else if (FDOutput == m_output)
        {
            // pMNDSOut = &pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_fd;
            m_pState->MNDSOutput.mnds_config_y = pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_fd;
            m_pState->MNDSOutput.mnds_config_c = pInputData->pStripeConfig->pStripeOutput->mndsConfig_c_fd;
        }
        else if (DisplayFullOutput == m_output)
        {
            // pMNDSOut = &pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_full;
            m_pState->MNDSOutput.mnds_config_y = pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_disp;
            m_pState->MNDSOutput.mnds_config_c = pInputData->pStripeConfig->pStripeOutput->mndsConfig_c_disp;
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Invalid output path");
        }

        StandAloneIfeMndsV16Config1d(static_cast<int16_t>(scaleN),
                                     static_cast<int16_t>(scaleM),
                                     0,
                                     0,
                                     input_ROI,
                                     output_ROI,
                                     &m_pState->MNDSOutput.mnds_config_y,
                                     &m_pState->MNDSOutput.mnds_config_c);

        if (outAspectRatio > inAspectRatio)
        {
            m_pState->MNDSOutput.scalingFactor = static_cast<FLOAT>(scaleN) / scaleM;
        }
        else
        {
            m_pState->MNDSOutput.scalingFactor = static_cast<FLOAT>(m_pState->inputHeight) /
                m_pState->MNDSOutput.dimension.height;
        }
    }
    else
    {
        if (outAspectRatio > inAspectRatio)
        {
            m_pState->MNDSOutput.scalingFactor = static_cast<FLOAT>(m_pState->inputWidth) /
                m_pState->MNDSOutput.dimension.width;
        }
        else
        {
            m_pState->MNDSOutput.scalingFactor = static_cast<FLOAT>(m_pState->inputHeight) /
                m_pState->MNDSOutput.dimension.height;
        }
    }

    // Only if this is left or right stripe processing and over write stripe then copy mnds values from stripe
    if (((CropTypeFromLeft == pInputData->pStripeConfig->cropType) ||
        (CropTypeFromRight == pInputData->pStripeConfig->cropType)) &&
        (TRUE == m_pInputData->pStripeConfig->overwriteStripes))
    {
        ISPInternalData* pFrameLevel = pInputData->pStripeConfig->pFrameLevelData->pFrameData;
        m_pState->MNDSOutput.dimension.height = pFrameLevel->scalerOutput[m_output].dimension.height;
        if (FullOutput == m_output)
        {
            // pMNDSOut = &pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_full;
            m_pState->MNDSOutput.mnds_config_y = pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_full;
            m_pState->MNDSOutput.mnds_config_c = pInputData->pStripeConfig->pStripeOutput->mndsConfig_c_full;
        }
        else if (FDOutput == m_output)
        {
            // pMNDSOut = &pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_fd;
            m_pState->MNDSOutput.mnds_config_y = pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_fd;
            m_pState->MNDSOutput.mnds_config_c = pInputData->pStripeConfig->pStripeOutput->mndsConfig_c_fd;
        }
        else if (DisplayFullOutput == m_output)
        {
            // pMNDSOut = &pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_full;
            m_pState->MNDSOutput.mnds_config_y = pInputData->pStripeConfig->pStripeOutput->mndsConfig_y_disp;
            m_pState->MNDSOutput.mnds_config_c = pInputData->pStripeConfig->pStripeOutput->mndsConfig_c_disp;
        }
        else
        {
            CAMX_ASSERT_ALWAYS_MESSAGE("Invalid output path");
        }
        if (outAspectRatio > inAspectRatio)
        {
            m_pState->MNDSOutput.scalingFactor = static_cast<FLOAT>(m_pState->MNDSOutput.mnds_config_y.input_l) /
                m_pState->MNDSOutput.mnds_config_y.output_l;
        }
        else
        {
            m_pState->MNDSOutput.scalingFactor = static_cast<FLOAT>(m_pState->inputHeight) /
                m_pState->MNDSOutput.dimension.height;
        }
    }

    CAMX_LOG_VERBOSE(CamxLogGroupISP, "Path %d[0-FD,1-Full], CropType %d, MNDS output[%dx%d] input [%dx%d] fac %f",
                     m_output,
                     pInputData->pStripeConfig->cropType,
                     m_pState->MNDSOutput.dimension.width,
                     m_pState->MNDSOutput.dimension.height,
                     m_pState->inputWidth,
                     m_pState->inputHeight,
                     m_pState->MNDSOutput.scalingFactor);

    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.width, "Invalid MNDS output width");
    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.height, "Invalid MNDS output height");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::CalculateInterpolationResolution
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEMNDS16::CalculateInterpolationResolution(
    UINT32  scaleFactorHorizontal,
    UINT32  scaleFactorVertical,
    UINT32* pHorizontalInterpolationResolution,
    UINT32* pVerticalInterpolationResolution)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pHorizontalInterpolationResolution) &&
        (NULL != pVerticalInterpolationResolution))
    {
        if ((scaleFactorHorizontal < 1) || (scaleFactorVertical < 1))
        {
            CAMX_LOG_WARN(CamxLogGroupISP, "Scaler output larger than sensor out, Scale factor clamped to 1x");
            scaleFactorHorizontal = 1;
            scaleFactorVertical   = 1;
        }

        // Constrains from SWI document
        if (scaleFactorHorizontal < 4)
        {
            *pHorizontalInterpolationResolution = 3;
        }
        else if (scaleFactorHorizontal < 8)
        {
            *pHorizontalInterpolationResolution = 2;
        }
        else if (scaleFactorHorizontal < 16)
        {
            *pHorizontalInterpolationResolution = 1;
        }
        else if (scaleFactorHorizontal < MNDS16MaxScaleFactor)
        {
            *pHorizontalInterpolationResolution = 0;
        }
        else
        {
            result = CamxResultEFailed;
            CAMX_ASSERT_ALWAYS_MESSAGE("scaleFactorHorizontal %d is greater than max supported ratio %d",
                                       scaleFactorHorizontal, MNDS16MaxScaleFactor);
        }

        if (CamxResultSuccess == result)
        {
            // Constrains from SWI document
            if (scaleFactorVertical < 4)
            {
                *pVerticalInterpolationResolution = 3;
            }
            else if (scaleFactorVertical < 8)
            {
                *pVerticalInterpolationResolution = 2;
            }
            else if (scaleFactorVertical < 16)
            {
                *pVerticalInterpolationResolution = 1;
            }
            else if (scaleFactorVertical < MNDS16MaxScaleFactor)
            {
                *pVerticalInterpolationResolution = 0;
            }
            else
            {
                result = CamxResultEFailed;
                CAMX_ASSERT_ALWAYS_MESSAGE("scaleFactorVertical %d is greater than max supported ratio %d",
                                           scaleFactorVertical, MNDS16MaxScaleFactor);
            }
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Input Null pointer");
        result = CamxResultEInvalidArg;
    }

    return  result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::GetChromaSubsampleFactor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEMNDS16::GetChromaSubsampleFactor(
    FLOAT* pHorizontalSubSampleFactor,
    FLOAT* pVerticalSubsampleFactor)
{
    CamxResult result = CamxResultSuccess;

    if ((NULL != pHorizontalSubSampleFactor) &&
        (NULL != pVerticalSubsampleFactor))
    {
        switch (m_pixelFormat)
        {
            /// @todo (CAMX-526) Check for all the formats and subsample factors
            case Format::YUV420NV12TP10:
            case Format::YUV420NV21TP10:
            case Format::YUV422NV16TP10:
            case Format::YUV420NV12:
            case Format::YUV420NV21:
            case Format::YUV422NV16:
            case Format::UBWCNV124R:
            case Format::UBWCTP10:
            case Format::PD10:
            case Format::FDYUV420NV12:
            case Format::P010:
                *pHorizontalSubSampleFactor = 2.0f;
                *pVerticalSubsampleFactor   = 2.0f;
                break;

            case Format::Y8:
            case Format::Y16:
            case Format::FDY8:
                break;
            default:
                CAMX_LOG_ERROR(CamxLogGroupISP, "Incompatible Format: %d, path %d",  m_pixelFormat, m_modulePath);
                *pHorizontalSubSampleFactor = 1.0f;
                *pVerticalSubsampleFactor   = 1.0f;
                result = CamxResultEInvalidArg;
                break;
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Input Null pointer");
        result = CamxResultEInvalidPointer;
    }

    return  result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::ConfigureFDLumaRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEMNDS16::ConfigureFDLumaRegisters(
    UINT32 horizontalInterpolationResolution,
    UINT32 verticalInterpolationResolution)
{
    IFEMNDS16FDLumaReg* pFDLuma = &m_regCmd.FDLuma;

    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.width, "Invalid MNDS output width");
    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.height, "Invalid MNDS output height");

    pFDLuma->config.bitfields.H_MN_EN                     = TRUE;
    pFDLuma->config.bitfields.V_MN_EN                     = TRUE;
    pFDLuma->horizontalPadding.bitfields.H_SKIP_CNT       = 0;
    pFDLuma->horizontalPadding.bitfields.RIGHT_PAD_EN     = 0;
    pFDLuma->horizontalPadding.bitfields.ROUNDING_PATTERN = 0;
     // Programming n means n+1 to the Hw, for all the width and height registers
    pFDLuma->horizontalPadding.bitfields.SCALE_Y_IN_WIDTH = m_pState->inputWidth - 1;
    pFDLuma->horizontalPhase.bitfields.H_INTERP_RESO      = horizontalInterpolationResolution;
    pFDLuma->horizontalPhase.bitfields.H_PHASE_MULT       = (m_pState->inputWidth <<
        (horizontalInterpolationResolution + MNDS16InterpolationShift)) / m_pState->MNDSOutput.dimension.width;

    pFDLuma->horizontalSize.bitfields.H_IN                = m_pState->inputWidth - 1;
    pFDLuma->horizontalSize.bitfields.H_OUT               = m_pState->MNDSOutput.dimension.width - 1;
    pFDLuma->horizontalStripe0.bitfields.H_MN_INIT        = 0;
    pFDLuma->horizontalStripe1.bitfields.H_PHASE_INIT     = 0;

    pFDLuma->verticalPadding.bitfields.BOTTOM_PAD_EN      = 0;
    pFDLuma->verticalPadding.bitfields.ROUNDING_PATTERN   = 0;
    pFDLuma->verticalPadding.bitfields.SCALE_Y_IN_HEIGHT  = m_pState->inputHeight - 1;
    pFDLuma->verticalPadding.bitfields.V_SKIP_CNT         = 0;
    pFDLuma->verticalPhase.bitfields.V_INTERP_RESO        = verticalInterpolationResolution;
    pFDLuma->verticalPhase.bitfields.V_PHASE_MULT         = (m_pState->inputHeight <<
        (verticalInterpolationResolution + MNDS16InterpolationShift)) / m_pState->MNDSOutput.dimension.height;

    pFDLuma->verticalSize.bitfields.V_IN                  = m_pState->inputHeight - 1;
    pFDLuma->verticalSize.bitfields.V_OUT                 = m_pState->MNDSOutput.dimension.height -1;
    pFDLuma->verticalStripe0.bitfields.V_MN_INIT          = 0;
    pFDLuma->verticalStripe1.bitfields.V_PHASE_INIT       = 0;

    if (TRUE == m_pInputData->pStripeConfig->overwriteStripes)
    {
        pFDLuma->horizontalPadding.bitfields.RIGHT_PAD_EN     = m_pState->MNDSOutput.mnds_config_y.right_pad_en;
        pFDLuma->horizontalPadding.bitfields.H_SKIP_CNT       = m_pState->MNDSOutput.mnds_config_y.pixel_offset;
        pFDLuma->horizontalStripe0.bitfields.H_MN_INIT        = m_pState->MNDSOutput.mnds_config_y.cnt_init;
        pFDLuma->horizontalStripe1.bitfields.H_PHASE_INIT     = m_pState->MNDSOutput.mnds_config_y.phase_init;
        pFDLuma->horizontalSize.bitfields.H_IN                = m_pState->MNDSOutput.mnds_config_y.input_l - 1;
        pFDLuma->horizontalSize.bitfields.H_OUT               = m_pState->MNDSOutput.mnds_config_y.output_l - 1;
        pFDLuma->horizontalPadding.bitfields.SCALE_Y_IN_WIDTH =
            m_pState->MNDSOutput.mnds_config_y.input_processed_length - 1;
        pFDLuma->horizontalPhase.bitfields.H_PHASE_MULT       = m_pState->MNDSOutput.mnds_config_y.phase_step;
        pFDLuma->horizontalPhase.bitfields.H_INTERP_RESO      = m_pState->MNDSOutput.mnds_config_y.interp_reso;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::ConfigureFullLumaRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEMNDS16::ConfigureFullLumaRegisters(
    UINT32 horizontalInterpolationResolution,
    UINT32 verticalInterpolationResolution)
{
    IFEMNDS16VideoLumaReg* pFullLuma = &m_regCmd.videoLuma;

    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.width, "Invalid MNDS output width");
    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.height, "Invalid MNDS output height");

    pFullLuma->config.bitfields.H_MN_EN                     = TRUE;
    pFullLuma->config.bitfields.V_MN_EN                     = TRUE;
    pFullLuma->horizontalPadding.bitfields.H_SKIP_CNT       = 0;
    pFullLuma->horizontalPadding.bitfields.RIGHT_PAD_EN     = 0;
    pFullLuma->horizontalPadding.bitfields.ROUNDING_PATTERN = 0;
    // Programming n means n+1 to the Hw, for all the width and height registers
    pFullLuma->horizontalPadding.bitfields.SCALE_Y_IN_WIDTH = m_pState->inputWidth - 1;
    pFullLuma->horizontalPhase.bitfields.H_INTERP_RESO      = horizontalInterpolationResolution;
    pFullLuma->horizontalPhase.bitfields.H_PHASE_MULT       = (m_pState->inputWidth <<
        (horizontalInterpolationResolution + MNDS16InterpolationShift)) / m_pState->MNDSOutput.dimension.width;

    pFullLuma->horizontalSize.bitfields.H_IN                = m_pState->inputWidth - 1;
    pFullLuma->horizontalSize.bitfields.H_OUT               = m_pState->MNDSOutput.dimension.width - 1;
    pFullLuma->horizontalStripe0.bitfields.H_MN_INIT        = 0;
    pFullLuma->horizontalStripe1.bitfields.H_PHASE_INIT     = 0;

    pFullLuma->verticalPadding.bitfields.BOTTOM_PAD_EN      = 0;
    pFullLuma->verticalPadding.bitfields.ROUNDING_PATTERN   = 0;
    pFullLuma->verticalPadding.bitfields.SCALE_Y_IN_HEIGHT  = m_pState->inputHeight - 1;
    pFullLuma->verticalPadding.bitfields.V_SKIP_CNT         = 0;
    pFullLuma->verticalPhase.bitfields.V_INTERP_RESO        = verticalInterpolationResolution;
    pFullLuma->verticalPhase.bitfields.V_PHASE_MULT         = (m_pState->inputHeight <<
        (verticalInterpolationResolution + MNDS16InterpolationShift)) / m_pState->MNDSOutput.dimension.height;

    pFullLuma->verticalSize.bitfields.V_IN                  = m_pState->inputHeight - 1;
    pFullLuma->verticalSize.bitfields.V_OUT                 = m_pState->MNDSOutput.dimension.height - 1;
    pFullLuma->verticalStripe0.bitfields.V_MN_INIT          = 0;
    pFullLuma->verticalStripe1.bitfields.V_PHASE_INIT       = 0;

    if (TRUE == m_pInputData->pStripeConfig->overwriteStripes)
    {
        pFullLuma->horizontalPadding.bitfields.RIGHT_PAD_EN     = m_pState->MNDSOutput.mnds_config_y.right_pad_en;
        pFullLuma->horizontalPadding.bitfields.H_SKIP_CNT       = m_pState->MNDSOutput.mnds_config_y.pixel_offset;
        pFullLuma->horizontalStripe0.bitfields.H_MN_INIT        = m_pState->MNDSOutput.mnds_config_y.cnt_init;
        pFullLuma->horizontalStripe1.bitfields.H_PHASE_INIT     = m_pState->MNDSOutput.mnds_config_y.phase_init;
        pFullLuma->horizontalSize.bitfields.H_IN                = m_pState->MNDSOutput.mnds_config_y.input_l - 1;
        pFullLuma->horizontalSize.bitfields.H_OUT               = m_pState->MNDSOutput.mnds_config_y.output_l - 1;
        pFullLuma->horizontalPadding.bitfields.SCALE_Y_IN_WIDTH =
            m_pState->MNDSOutput.mnds_config_y.input_processed_length - 1;
        pFullLuma->horizontalPhase.bitfields.H_PHASE_MULT       = m_pState->MNDSOutput.mnds_config_y.phase_step;
        pFullLuma->horizontalPhase.bitfields.H_INTERP_RESO      = m_pState->MNDSOutput.mnds_config_y.interp_reso;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::ConfigureDisplayLumaRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEMNDS16::ConfigureDisplayLumaRegisters(
    UINT32 horizontalInterpolationResolution,
    UINT32 verticalInterpolationResolution)
{
    IFEMNDS16DisplayLumaReg* pFullLuma = &m_regCmd.displayLuma;

    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.width, "Invalid MNDS output width");
    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.height, "Invalid MNDS output height");

    pFullLuma->config.bitfields.H_MN_EN                     = TRUE;
    pFullLuma->config.bitfields.V_MN_EN                     = TRUE;
    pFullLuma->horizontalPadding.bitfields.H_SKIP_CNT       = 0;
    pFullLuma->horizontalPadding.bitfields.RIGHT_PAD_EN     = 0;
    pFullLuma->horizontalPadding.bitfields.ROUNDING_PATTERN = 0;
    // Programming n means n+1 to the Hw, for all the width and height registers
    pFullLuma->horizontalPadding.bitfields.SCALE_Y_IN_WIDTH = m_pState->inputWidth - 1;
    pFullLuma->horizontalPhase.bitfields.H_INTERP_RESO      = horizontalInterpolationResolution;
    pFullLuma->horizontalPhase.bitfields.H_PHASE_MULT       = (m_pState->inputWidth <<
        (horizontalInterpolationResolution + MNDS16InterpolationShift)) / m_pState->MNDSOutput.dimension.width;

    pFullLuma->horizontalSize.bitfields.H_IN                = m_pState->inputWidth - 1;
    pFullLuma->horizontalSize.bitfields.H_OUT               = m_pState->MNDSOutput.dimension.width - 1;
    pFullLuma->horizontalStripe0.bitfields.H_MN_INIT        = 0;
    pFullLuma->horizontalStripe1.bitfields.H_PHASE_INIT     = 0;

    pFullLuma->verticalPadding.bitfields.BOTTOM_PAD_EN      = 0;
    pFullLuma->verticalPadding.bitfields.ROUNDING_PATTERN   = 0;
    pFullLuma->verticalPadding.bitfields.SCALE_Y_IN_HEIGHT  = m_pState->inputHeight - 1;
    pFullLuma->verticalPadding.bitfields.V_SKIP_CNT         = 0;
    pFullLuma->verticalPhase.bitfields.V_INTERP_RESO        = verticalInterpolationResolution;
    pFullLuma->verticalPhase.bitfields.V_PHASE_MULT         = (m_pState->inputHeight <<
        (verticalInterpolationResolution + MNDS16InterpolationShift)) / m_pState->MNDSOutput.dimension.height;

    pFullLuma->verticalSize.bitfields.V_IN                  = m_pState->inputHeight - 1;
    pFullLuma->verticalSize.bitfields.V_OUT                 = m_pState->MNDSOutput.dimension.height - 1;
    pFullLuma->verticalStripe0.bitfields.V_MN_INIT          = 0;
    pFullLuma->verticalStripe1.bitfields.V_PHASE_INIT       = 0;

    if (TRUE == m_pInputData->pStripeConfig->overwriteStripes)
    {
        pFullLuma->horizontalPadding.bitfields.RIGHT_PAD_EN     = m_pState->MNDSOutput.mnds_config_y.right_pad_en;
        pFullLuma->horizontalPadding.bitfields.H_SKIP_CNT       = m_pState->MNDSOutput.mnds_config_y.pixel_offset;
        pFullLuma->horizontalStripe0.bitfields.H_MN_INIT        = m_pState->MNDSOutput.mnds_config_y.cnt_init;
        pFullLuma->horizontalStripe1.bitfields.H_PHASE_INIT     = m_pState->MNDSOutput.mnds_config_y.phase_init;
        pFullLuma->horizontalSize.bitfields.H_IN                = m_pState->MNDSOutput.mnds_config_y.input_l - 1;
        pFullLuma->horizontalSize.bitfields.H_OUT               = m_pState->MNDSOutput.mnds_config_y.output_l - 1;
        pFullLuma->horizontalPadding.bitfields.SCALE_Y_IN_WIDTH =
            m_pState->MNDSOutput.mnds_config_y.input_processed_length - 1;
        pFullLuma->horizontalPhase.bitfields.H_PHASE_MULT       = m_pState->MNDSOutput.mnds_config_y.phase_step;
        pFullLuma->horizontalPhase.bitfields.H_INTERP_RESO      = m_pState->MNDSOutput.mnds_config_y.interp_reso;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::ConfigureFDChromaRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEMNDS16::ConfigureFDChromaRegisters(
    UINT32 horizontalInterpolationResolution,
    UINT32 verticalInterpolationResolution,
    FLOAT  horizontalSubSampleFactor,
    FLOAT  verticalSubsampleFactor)
{
    IFEMNDS16FDChromaReg* pFDChroma = &m_regCmd.FDChroma;

    CAMX_ASSERT_MESSAGE(0 != horizontalSubSampleFactor, "Invalid horizontalSubSampleFactor");
    CAMX_ASSERT_MESSAGE(0 != verticalSubsampleFactor, "Invalid verticalSubsampleFactor");

    pFDChroma->config.bitfields.H_MN_EN                        = TRUE;
    pFDChroma->config.bitfields.V_MN_EN                        = TRUE;
    pFDChroma->horizontalPadding.bitfields.H_SKIP_CNT          = 0;
    pFDChroma->horizontalPadding.bitfields.RIGHT_PAD_EN        = 0;
    pFDChroma->horizontalPadding.bitfields.ROUNDING_PATTERN    = 0;
    // Programming n means n+1 to the Hw, for all the width and height registers
    pFDChroma->horizontalPadding.bitfields.SCALE_CBCR_IN_WIDTH = m_pState->inputWidth - 1;
    pFDChroma->horizontalSize.bitfields.H_IN                   = m_pState->inputWidth - 1;
    pFDChroma->horizontalSize.bitfields.H_OUT                  =
        static_cast<UINT32> (m_pState->MNDSOutput.dimension.width / horizontalSubSampleFactor) - 1;
    pFDChroma->horizontalPhase.bitfields.H_INTERP_RESO         = horizontalInterpolationResolution;

    // Add 1 to H_OUT as values of hIn, hOut, vIn and vOut are decreased by 1, Value of n means n+1 in hardware
    pFDChroma->horizontalPhase.bitfields.H_PHASE_MULT          =
        (m_pState->inputWidth << (horizontalInterpolationResolution + MNDS16InterpolationShift)) /
        (pFDChroma->horizontalSize.bitfields.H_OUT + 1);

    pFDChroma->horizontalStripe0.bitfields.H_MN_INIT           = 0;
    pFDChroma->horizontalStripe1.bitfields.H_PHASE_INIT        = 0;
    pFDChroma->verticalPadding.bitfields.BOTTOM_PAD_EN         = 0;
    pFDChroma->verticalPadding.bitfields.ROUNDING_PATTERN      = 0;
    pFDChroma->verticalPadding.bitfields.SCALE_CBCR_IN_HEIGHT  = m_pState->inputHeight - 1;
    pFDChroma->verticalPadding.bitfields.V_SKIP_CNT            = 0;
    pFDChroma->verticalSize.bitfields.V_IN                     = m_pState->inputHeight -1;
    pFDChroma->verticalSize.bitfields.V_OUT                    =
        static_cast<UINT32>(m_pState->MNDSOutput.dimension.height / verticalSubsampleFactor) -1;
    pFDChroma->verticalPhase.bitfields.V_INTERP_RESO           = verticalInterpolationResolution;

    // Add 1 to V_OUT as values of hIn, hOut, vIn and vOut are decreased by 1, Value of n means n+1 in hardware
    pFDChroma->verticalPhase.bitfields.V_PHASE_MULT            =
        (m_pState->inputHeight << (verticalInterpolationResolution + MNDS16InterpolationShift)) /
        (m_regCmd.FDChroma.verticalSize.bitfields.V_OUT + 1);
    pFDChroma->verticalStripe0.bitfields.V_MN_INIT             = 0;
    pFDChroma->verticalStripe1.bitfields.V_PHASE_INIT          = 0;

    if (TRUE == m_pInputData->pStripeConfig->overwriteStripes)
    {
        pFDChroma->horizontalPadding.bitfields.RIGHT_PAD_EN           = m_pState->MNDSOutput.mnds_config_c.right_pad_en;
        pFDChroma->horizontalPadding.bitfields.H_SKIP_CNT             = m_pState->MNDSOutput.mnds_config_c.pixel_offset;
        pFDChroma->horizontalStripe0.bitfields.H_MN_INIT              = m_pState->MNDSOutput.mnds_config_c.cnt_init;
        pFDChroma->horizontalStripe1.bitfields.H_PHASE_INIT           = m_pState->MNDSOutput.mnds_config_c.phase_init;
        pFDChroma->horizontalSize.bitfields.H_IN                      = m_pState->MNDSOutput.mnds_config_c.input_l - 1;
        pFDChroma->horizontalSize.bitfields.H_OUT                     =
            static_cast<UINT32> (m_pState->MNDSOutput.mnds_config_c.output_l) - 1;
        pFDChroma->horizontalPadding.bitfields.SCALE_CBCR_IN_WIDTH    =
            m_pState->MNDSOutput.mnds_config_c.input_processed_length - 1;
        // Add 1 to H_OUT as values of hIn, hOut, vIn and vOut are decreased by 1, Value of n means n+1 in hardware
        pFDChroma->horizontalPhase.bitfields.H_PHASE_MULT             = m_pState->MNDSOutput.mnds_config_c.phase_step;
        pFDChroma->horizontalPhase.bitfields.H_INTERP_RESO            = m_pState->MNDSOutput.mnds_config_c.interp_reso;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::ConfigureFullChromaRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEMNDS16::ConfigureFullChromaRegisters(
    UINT32 horizontalInterpolationResolution,
    UINT32 verticalInterpolationResolution,
    FLOAT  horizontalSubSampleFactor,
    FLOAT  verticalSubsampleFactor)
{
    IFEMNDS16VideoChromaReg* pFullChroma = &m_regCmd.videoChroma;

    CAMX_ASSERT_MESSAGE(0 != horizontalSubSampleFactor, "Invalid horizontalSubSampleFactor");
    CAMX_ASSERT_MESSAGE(0 != verticalSubsampleFactor, "Invalid verticalSubsampleFactor");

    pFullChroma->config.bitfields.H_MN_EN                           = TRUE;
    pFullChroma->config.bitfields.V_MN_EN                           = TRUE;
    pFullChroma->horizontalPadding.bitfields.H_SKIP_CNT             = 0;
    pFullChroma->horizontalPadding.bitfields.RIGHT_PAD_EN           = 0;
    pFullChroma->horizontalPadding.bitfields.ROUNDING_PATTERN       = 0;
    // Programming n means n+1 to the Hw, for all the width and height registers
    pFullChroma->horizontalPadding.bitfields.SCALE_CBCR_IN_WIDTH    = m_pState->inputWidth - 1;
    pFullChroma->horizontalSize.bitfields.H_IN                      = m_pState->inputWidth - 1;
    pFullChroma->horizontalSize.bitfields.H_OUT                     =
        static_cast<UINT32> (m_pState->MNDSOutput.dimension.width / horizontalSubSampleFactor) - 1;
    pFullChroma->horizontalPhase.bitfields.H_INTERP_RESO            = horizontalInterpolationResolution;

    // Add 1 to H_OUT as values of hIn, hOut, vIn and vOut are decreased by 1, Value of n means n+1 in hardware
    pFullChroma->horizontalPhase.bitfields.H_PHASE_MULT             =
        (m_pState->inputWidth << (horizontalInterpolationResolution + MNDS16InterpolationShift)) /
        (pFullChroma->horizontalSize.bitfields.H_OUT + 1);

    pFullChroma->horizontalStripe0.bitfields.H_MN_INIT              = 0;
    pFullChroma->horizontalStripe1.bitfields.H_PHASE_INIT           = 0;
    pFullChroma->verticalPadding.bitfields.BOTTOM_PAD_EN            = 0;
    pFullChroma->verticalPadding.bitfields.ROUNDING_PATTERN         = 0;
    pFullChroma->verticalPadding.bitfields.SCALE_CBCR_IN_HEIGHT     = m_pState->inputHeight - 1;
    pFullChroma->verticalPadding.bitfields.V_SKIP_CNT               = 0;
    pFullChroma->verticalSize.bitfields.V_IN                        = m_pState->inputHeight -1;
    pFullChroma->verticalSize.bitfields.V_OUT                       =
        static_cast<UINT32>(m_pState->MNDSOutput.dimension.height / verticalSubsampleFactor) -1;
    pFullChroma->verticalPhase.bitfields.V_INTERP_RESO              = verticalInterpolationResolution;

    // Add 1 to V_OUT as values of hIn, hOut, vIn and vOut are decreased by 1, Value of n means n+1 in hardware
    pFullChroma->verticalPhase.bitfields.V_PHASE_MULT               =
        (m_pState->inputHeight << (verticalInterpolationResolution + MNDS16InterpolationShift)) /
        (m_regCmd.videoChroma.verticalSize.bitfields.V_OUT + 1);
    pFullChroma->verticalStripe0.bitfields.V_MN_INIT                = 0;
    pFullChroma->verticalStripe1.bitfields.V_PHASE_INIT             = 0;

    if (TRUE == m_pInputData->pStripeConfig->overwriteStripes)
    {
        pFullChroma->horizontalPadding.bitfields.RIGHT_PAD_EN           = m_pState->MNDSOutput.mnds_config_c.right_pad_en;
        pFullChroma->horizontalPadding.bitfields.H_SKIP_CNT             = m_pState->MNDSOutput.mnds_config_c.pixel_offset;
        pFullChroma->horizontalStripe0.bitfields.H_MN_INIT              = m_pState->MNDSOutput.mnds_config_c.cnt_init;
        pFullChroma->horizontalStripe1.bitfields.H_PHASE_INIT           = m_pState->MNDSOutput.mnds_config_c.phase_init;
        pFullChroma->horizontalSize.bitfields.H_IN                      = m_pState->MNDSOutput.mnds_config_c.input_l - 1;
        pFullChroma->horizontalSize.bitfields.H_OUT                     =
            static_cast<UINT32> (m_pState->MNDSOutput.mnds_config_c.output_l)- 1;
        pFullChroma->horizontalPadding.bitfields.SCALE_CBCR_IN_WIDTH    =
            m_pState->MNDSOutput.mnds_config_c.input_processed_length - 1;
        // Add 1 to H_OUT as values of hIn, hOut, vIn and vOut are decreased by 1, Value of n means n+1 in hardware
        pFullChroma->horizontalPhase.bitfields.H_PHASE_MULT             = m_pState->MNDSOutput.mnds_config_c.phase_step;
        pFullChroma->horizontalPhase.bitfields.H_INTERP_RESO            = m_pState->MNDSOutput.mnds_config_c.interp_reso;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::ConfigureDisplayChromaRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEMNDS16::ConfigureDisplayChromaRegisters(
    UINT32 horizontalInterpolationResolution,
    UINT32 verticalInterpolationResolution,
    FLOAT  horizontalSubSampleFactor,
    FLOAT  verticalSubsampleFactor)
{
    IFEMNDS16DisplayChromaReg* pFullChroma = &m_regCmd.displayChroma;

    CAMX_ASSERT_MESSAGE(0 != horizontalSubSampleFactor, "Invalid horizontalSubSampleFactor");
    CAMX_ASSERT_MESSAGE(0 != verticalSubsampleFactor, "Invalid verticalSubsampleFactor");

    pFullChroma->config.bitfields.H_MN_EN                           = TRUE;
    pFullChroma->config.bitfields.V_MN_EN                           = TRUE;
    pFullChroma->horizontalPadding.bitfields.H_SKIP_CNT             = 0;
    pFullChroma->horizontalPadding.bitfields.RIGHT_PAD_EN           = 0;
    pFullChroma->horizontalPadding.bitfields.ROUNDING_PATTERN       = 0;
    // Programming n means n+1 to the Hw, for all the width and height registers
    pFullChroma->horizontalPadding.bitfields.SCALE_CBCR_IN_WIDTH    = m_pState->inputWidth - 1;
    pFullChroma->horizontalSize.bitfields.H_IN                      = m_pState->inputWidth - 1;
    pFullChroma->horizontalSize.bitfields.H_OUT                     =
        static_cast<UINT32> (m_pState->MNDSOutput.dimension.width / horizontalSubSampleFactor) - 1;
    pFullChroma->horizontalPhase.bitfields.H_INTERP_RESO            = horizontalInterpolationResolution;

    // Add 1 to H_OUT as values of hIn, hOut, vIn and vOut are decreased by 1, Value of n means n+1 in hardware
    pFullChroma->horizontalPhase.bitfields.H_PHASE_MULT             =
        (m_pState->inputWidth << (horizontalInterpolationResolution + MNDS16InterpolationShift)) /
        (pFullChroma->horizontalSize.bitfields.H_OUT + 1);

    pFullChroma->horizontalStripe0.bitfields.H_MN_INIT              = 0;
    pFullChroma->horizontalStripe1.bitfields.H_PHASE_INIT           = 0;
    pFullChroma->verticalPadding.bitfields.BOTTOM_PAD_EN            = 0;
    pFullChroma->verticalPadding.bitfields.ROUNDING_PATTERN         = 0;
    pFullChroma->verticalPadding.bitfields.SCALE_CBCR_IN_HEIGHT     = m_pState->inputHeight - 1;
    pFullChroma->verticalPadding.bitfields.V_SKIP_CNT               = 0;
    pFullChroma->verticalSize.bitfields.V_IN                        = m_pState->inputHeight -1;
    pFullChroma->verticalSize.bitfields.V_OUT                       =
        static_cast<UINT32>(m_pState->MNDSOutput.dimension.height / verticalSubsampleFactor) -1;
    pFullChroma->verticalPhase.bitfields.V_INTERP_RESO              = verticalInterpolationResolution;

    // Add 1 to V_OUT as values of hIn, hOut, vIn and vOut are decreased by 1, Value of n means n+1 in hardware
    pFullChroma->verticalPhase.bitfields.V_PHASE_MULT               =
        (m_pState->inputHeight << (verticalInterpolationResolution + MNDS16InterpolationShift)) /
        (m_regCmd.displayChroma.verticalSize.bitfields.V_OUT + 1);
    pFullChroma->verticalStripe0.bitfields.V_MN_INIT                = 0;
    pFullChroma->verticalStripe1.bitfields.V_PHASE_INIT             = 0;

    if (TRUE == m_pInputData->pStripeConfig->overwriteStripes)
    {
        pFullChroma->horizontalPadding.bitfields.RIGHT_PAD_EN           = m_pState->MNDSOutput.mnds_config_c.right_pad_en;
        pFullChroma->horizontalPadding.bitfields.H_SKIP_CNT             = m_pState->MNDSOutput.mnds_config_c.pixel_offset;
        pFullChroma->horizontalStripe0.bitfields.H_MN_INIT              = m_pState->MNDSOutput.mnds_config_c.cnt_init;
        pFullChroma->horizontalStripe1.bitfields.H_PHASE_INIT           = m_pState->MNDSOutput.mnds_config_c.phase_init;
        pFullChroma->horizontalSize.bitfields.H_IN                      = m_pState->MNDSOutput.mnds_config_c.input_l - 1;
        pFullChroma->horizontalSize.bitfields.H_OUT                     =
            static_cast<UINT32> (m_pState->MNDSOutput.mnds_config_c.output_l)- 1;
        pFullChroma->horizontalPadding.bitfields.SCALE_CBCR_IN_WIDTH    =
            m_pState->MNDSOutput.mnds_config_c.input_processed_length - 1;
        // Add 1 to H_OUT as values of hIn, hOut, vIn and vOut are decreased by 1, Value of n means n+1 in hardware
        pFullChroma->horizontalPhase.bitfields.H_PHASE_MULT             = m_pState->MNDSOutput.mnds_config_c.phase_step;
        pFullChroma->horizontalPhase.bitfields.H_INTERP_RESO            = m_pState->MNDSOutput.mnds_config_c.interp_reso;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::ConfigureRegisters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEMNDS16::ConfigureRegisters()
{
    CamxResult result = CamxResultSuccess;

    UINT32 scaleFactorHorizontal;                    // Horizontal scale factor
    UINT32 scaleFactorVertical;                      // vertical scale factor
    UINT32 horizontalInterpolationResolution = 1;    // horizontal interpolation resolution
    UINT32 verticalInterpolationResolution   = 1;    // vertical interpolation resolution
    FLOAT  horizontalSubSampleFactor         = 1.0f; // Horizontal chroma subsampling factor
    FLOAT  verticalSubsampleFactor           = 1.0f; // Verical chroma subsampling factor

    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.width, "Invalid MNDS output width");
    CAMX_ASSERT_MESSAGE(0 != m_pState->MNDSOutput.dimension.height, "Invalid MNDS output height");

    // Calculate the interpolation resolution based on the scaling ratio
    scaleFactorHorizontal = m_pState->inputWidth / m_pState->MNDSOutput.dimension.width;
    scaleFactorVertical   = m_pState->inputHeight / m_pState->MNDSOutput.dimension.height;

    result = CalculateInterpolationResolution(scaleFactorHorizontal,
                                              scaleFactorVertical,
                                              &horizontalInterpolationResolution,
                                              &verticalInterpolationResolution);

    // Luma register configuration
    if (CamxResultSuccess == result)
    {
        if (FDOutput == m_output)
        {
            ConfigureFDLumaRegisters(horizontalInterpolationResolution, verticalInterpolationResolution);
        }
        else if (DisplayFullOutput == m_output)
        {
            ConfigureDisplayLumaRegisters(horizontalInterpolationResolution, verticalInterpolationResolution);
        }
        else
        {
            ConfigureFullLumaRegisters(horizontalInterpolationResolution, verticalInterpolationResolution);
        }
    }

    // Chroma register configuration
    if (CamxResultSuccess == result)
    {
        // Get the chroma channel subsample factor
        result = GetChromaSubsampleFactor(&horizontalSubSampleFactor, &verticalSubsampleFactor);

        if (CamxResultSuccess == result)
        {
            // Calculate the interpolation resolution based on the Chroma scaling ratio
            scaleFactorHorizontal = static_cast<UINT32>((m_pState->inputWidth * horizontalSubSampleFactor) /
                                                        m_pState->MNDSOutput.dimension.width);
            scaleFactorVertical   = static_cast<UINT32>((m_pState->inputHeight * verticalSubsampleFactor) /
                                                        m_pState->MNDSOutput.dimension.height);

            result = CalculateInterpolationResolution(scaleFactorHorizontal,
                                                      scaleFactorVertical,
                                                      &horizontalInterpolationResolution,
                                                      &verticalInterpolationResolution);
        }
    }

    if (CamxResultSuccess == result)
    {
        if (FDOutput == m_output)
        {
            ConfigureFDChromaRegisters(horizontalInterpolationResolution,
                                       verticalInterpolationResolution,
                                       horizontalSubSampleFactor,
                                       verticalSubsampleFactor);
        }
        else if (DisplayFullOutput == m_output)
        {
            ConfigureDisplayChromaRegisters(horizontalInterpolationResolution,
                                         verticalInterpolationResolution,
                                         horizontalSubSampleFactor,
                                         verticalSubsampleFactor);
        }
        else
        {
            ConfigureFullChromaRegisters(horizontalInterpolationResolution,
                                         verticalInterpolationResolution,
                                         horizontalSubSampleFactor,
                                         verticalSubsampleFactor);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::CreateCmdList
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEMNDS16::CreateCmdList(
    ISPInputData* pInputData)
{
    CamxResult result     = CamxResultSuccess;

    if (NULL != pInputData->pCmdBuffer)
    {
        CmdBuffer* pCmdBuffer = pInputData->pCmdBuffer;
        // Write FD output path registers
        if (FDOutput == m_output)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_SCALE_FD_Y_CFG,
                                                  (sizeof(IFEMNDS16FDLumaReg) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.FDLuma));
            if (CamxResultSuccess == result)
            {
                result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                      regIFE_IFE_0_VFE_SCALE_FD_CBCR_CFG,
                                                      (sizeof(IFEMNDS16FDChromaReg) / RegisterWidthInBytes),
                                                      reinterpret_cast<UINT32*>(&m_regCmd.FDChroma));
            }

            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to configure Scaler FD path Register");
            }
        }

        // Write Video(Full) output path registers
        if (FullOutput == m_output)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_SCALE_VID_Y_CFG,
                                                  (sizeof(IFEMNDS16VideoLumaReg) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.videoLuma));
            if (CamxResultSuccess == result)
            {
                result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                      regIFE_IFE_0_VFE_SCALE_VID_CBCR_CFG,
                                                      (sizeof(IFEMNDS16VideoChromaReg) / RegisterWidthInBytes),
                                                      reinterpret_cast<UINT32*>(&m_regCmd.videoChroma));

            }
            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to configure Scaler Video(Full) path Register");
            }
        }

        // Write Display(Full) output path registers
        if (DisplayFullOutput == m_output)
        {
            result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                  regIFE_IFE_0_VFE_SCALE_DISP_Y_CFG,
                                                  (sizeof(IFEMNDS16DisplayLumaReg) / RegisterWidthInBytes),
                                                  reinterpret_cast<UINT32*>(&m_regCmd.displayLuma));
            if (CamxResultSuccess == result)
            {
                result = PacketBuilder::WriteRegRange(pCmdBuffer,
                                                      regIFE_IFE_0_VFE_SCALE_DISP_CBCR_CFG,
                                                      (sizeof(IFEMNDS16DisplayChromaReg) / RegisterWidthInBytes),
                                                      reinterpret_cast<UINT32*>(&m_regCmd.displayChroma));
            }
            if (CamxResultSuccess != result)
            {
                CAMX_ASSERT_ALWAYS_MESSAGE("Failed to configure Scaler Display(Full) path Register");
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
// IFEMNDS16::ValidateDependenceParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEMNDS16::ValidateDependenceParams(
    ISPInputData* pInputData)
{
    INT32       inputWidth;
    INT32       inputHeight;
    CamxResult  result           = CamxResultSuccess;
    CropInfo*   pSensorCAMIFCrop = &pInputData->pStripeConfig->CAMIFCrop;
    CropWindow* pInputCropWindow = &pInputData->pStripeConfig->HALCrop[m_output];

    inputWidth  = pSensorCAMIFCrop->lastPixel - pSensorCAMIFCrop->firstPixel + 1;
    inputHeight = pSensorCAMIFCrop->lastLine - pSensorCAMIFCrop->firstLine + 1;

    // CAMIF input width for YUV sensor would be twice that of sensor width, as each pixel accounts for Y and U/V data
    if (TRUE == pInputData->sensorData.isYUV)
    {
        inputWidth >>= 1;
    }

    // Validate Crop window from HAL
    if (((pInputCropWindow->left + pInputCropWindow->width) > inputWidth)   ||
        ((pInputCropWindow->top + pInputCropWindow->height) >  inputHeight) ||
        (pSensorCAMIFCrop->lastLine  < pSensorCAMIFCrop->firstLine)         ||
        (pSensorCAMIFCrop->lastPixel < pSensorCAMIFCrop->firstPixel)        ||
        (0 == pInputCropWindow->width)                                      ||
        (0 == pInputCropWindow->height)                                     ||
        (0 == inputWidth)                                                   ||
        (0 == inputHeight))
    {
        CAMX_LOG_ERROR(CamxLogGroupISP, "Crop window %d, %d, %d, %d, input dimension %d * %d",
                       pInputCropWindow->left,
                       pInputCropWindow->top,
                       pInputCropWindow->width,
                       pInputCropWindow->height,
                       inputWidth, inputHeight);
        result = CamxResultEInvalidArg;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::DumpRegConfig
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEMNDS16::DumpRegConfig()
{
    if (FDOutput == m_output)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma PAD_EN        [%d] ",
            m_regCmd.FDLuma.horizontalPadding.bitfields.RIGHT_PAD_EN);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma H_SKIP_CNT    [%d] ",
            m_regCmd.FDLuma.horizontalPadding.bitfields.H_SKIP_CNT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma H_MN_INIT     [%d] ",
            m_regCmd.FDLuma.horizontalStripe0.bitfields.H_MN_INIT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma H_PHASE_INIT  [%d] ",
            m_regCmd.FDLuma.horizontalStripe1.bitfields.H_PHASE_INIT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma H_IN          [%d] ",
            m_regCmd.FDLuma.horizontalSize.bitfields.H_IN);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma H_OUT         [%d] ",
            m_regCmd.FDLuma.horizontalSize.bitfields.H_OUT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma V_IN          [%d] ",
            m_regCmd.FDLuma.verticalSize.bitfields.V_IN);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma V_OUT         [%d] ",
            m_regCmd.FDLuma.verticalSize.bitfields.V_OUT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma SCALE_Y_IN_WIDTH      [%d] ",
            m_regCmd.FDLuma.horizontalPadding.bitfields.SCALE_Y_IN_WIDTH);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma H_PHASE_MULT  [%d] ",
            m_regCmd.FDLuma.horizontalPhase.bitfields.H_PHASE_MULT);

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma Config            [0x%x] ", m_regCmd.FDLuma.config);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma horizontalSize    [0x%x] ", m_regCmd.FDLuma.horizontalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma horizontalPhase   [0x%x] ", m_regCmd.FDLuma.horizontalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma horizontalStripe0 [0x%x] ", m_regCmd.FDLuma.horizontalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma horizontalStripe1 [0x%x] ", m_regCmd.FDLuma.horizontalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma horizontalPadding [0x%x] ", m_regCmd.FDLuma.horizontalPadding);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma verticalSize      [0x%x] ", m_regCmd.FDLuma.verticalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma verticalPhase     [0x%x] ", m_regCmd.FDLuma.verticalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma verticalStripe0   [0x%x] ", m_regCmd.FDLuma.verticalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma verticalStripe1   [0x%x] ", m_regCmd.FDLuma.verticalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Luma verticalPadding   [0x%x] ", m_regCmd.FDLuma.verticalPadding);

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma Config            [0x%x] ", m_regCmd.FDChroma.config);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma horizontalSize    [0x%x] ", m_regCmd.FDChroma.horizontalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma horizontalPhase   [0x%x] ", m_regCmd.FDChroma.horizontalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma horizontalStripe0 [0x%x] ", m_regCmd.FDChroma.horizontalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma horizontalStripe1 [0x%x] ", m_regCmd.FDChroma.horizontalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma horizontalPadding [0x%x] ", m_regCmd.FDChroma.horizontalPadding);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma verticalSize      [0x%x] ", m_regCmd.FDChroma.verticalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma verticalPhase     [0x%x] ", m_regCmd.FDChroma.verticalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma verticalStripe0   [0x%x] ", m_regCmd.FDChroma.verticalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma verticalStripe1   [0x%x] ", m_regCmd.FDChroma.verticalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS FD Chroma verticalPadding   [0x%x] ", m_regCmd.FDChroma.verticalPadding);
    }

    if (FullOutput == m_output)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma PAD_EN        [%d] ",
            m_regCmd.videoLuma.horizontalPadding.bitfields.RIGHT_PAD_EN);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma H_SKIP_CNT    [%d] ",
            m_regCmd.videoLuma.horizontalPadding.bitfields.H_SKIP_CNT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma H_MN_INIT     [%d] ",
            m_regCmd.videoLuma.horizontalStripe0.bitfields.H_MN_INIT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma H_PHASE_INIT  [%d] ",
            m_regCmd.videoLuma.horizontalStripe1.bitfields.H_PHASE_INIT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma H_IN          [%d] ",
            m_regCmd.videoLuma.horizontalSize.bitfields.H_IN);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma H_OUT         [%d] ",
            m_regCmd.videoLuma.horizontalSize.bitfields.H_OUT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma V_IN          [%d] ",
            m_regCmd.videoLuma.verticalSize.bitfields.V_IN);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma V_OUT         [%d] ",
            m_regCmd.videoLuma.verticalSize.bitfields.V_OUT);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma SCALE_Y_IN_WIDTH      [%d] ",
            m_regCmd.videoLuma.horizontalPadding.bitfields.SCALE_Y_IN_WIDTH);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma H_PHASE_MULT  [%d] ",
            m_regCmd.videoLuma.horizontalPhase.bitfields.H_PHASE_MULT);

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma Config            [0x%x] ", m_regCmd.videoLuma.config);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma horizontalSize    [0x%x] ", m_regCmd.videoLuma.horizontalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma horizontalPhase   [0x%x] ", m_regCmd.videoLuma.horizontalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma horizontalStripe0 [0x%x] ", m_regCmd.videoLuma.horizontalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma horizontalStripe1 [0x%x] ", m_regCmd.videoLuma.horizontalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma horizontalPadding [0x%x] ", m_regCmd.videoLuma.horizontalPadding);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma verticalSize      [0x%x] ", m_regCmd.videoLuma.verticalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma verticalPhase     [0x%x] ", m_regCmd.videoLuma.verticalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma verticalStripe0   [0x%x] ", m_regCmd.videoLuma.verticalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma verticalStripe1   [0x%x] ", m_regCmd.videoLuma.verticalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Luma verticalPadding   [0x%x] ", m_regCmd.videoLuma.verticalPadding);

        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Chroma Config            [0x%x] ", m_regCmd.videoChroma.config);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Chroma horizontalSize    [0x%x] ", m_regCmd.videoChroma.horizontalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Chroma horizontalPhase   [0x%x] ", m_regCmd.videoChroma.horizontalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
            "MNDS Full Chroma horizontalStripe0 [0x%x] ", m_regCmd.videoChroma.horizontalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
            "MNDS Full Chroma horizontalStripe1 [0x%x] ", m_regCmd.videoChroma.horizontalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod,
            "MNDS Full Chroma horizontalPadding [0x%x] ", m_regCmd.videoChroma.horizontalPadding);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Chroma verticalSize      [0x%x] ", m_regCmd.videoChroma.verticalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Chroma verticalPhase     [0x%x] ", m_regCmd.videoChroma.verticalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Chroma verticalStripe0   [0x%x] ", m_regCmd.videoChroma.verticalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Chroma verticalStripe1   [0x%x] ", m_regCmd.videoChroma.verticalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "MNDS Full Chroma verticalPadding   [0x%x] ", m_regCmd.videoChroma.verticalPadding);
    }

    if (DisplayFullOutput == m_output)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma PAD_EN        [%d] ",
            m_regCmd.displayLuma.horizontalPadding.bitfields.RIGHT_PAD_EN);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma H_SKIP_CNT    [%d] ",
            m_regCmd.displayLuma.horizontalPadding.bitfields.H_SKIP_CNT);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma H_MN_INIT     [%d] ",
            m_regCmd.displayLuma.horizontalStripe0.bitfields.H_MN_INIT);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma H_PHASE_INIT  [%d] ",
            m_regCmd.displayLuma.horizontalStripe1.bitfields.H_PHASE_INIT);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma H_IN          [%d] ",
            m_regCmd.displayLuma.horizontalSize.bitfields.H_IN);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma H_OUT         [%d] ",
            m_regCmd.displayLuma.horizontalSize.bitfields.H_OUT);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma V_IN          [%d] ",
            m_regCmd.displayLuma.verticalSize.bitfields.V_IN);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma V_OUT         [%d] ",
            m_regCmd.displayLuma.verticalSize.bitfields.V_OUT);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma SCALE_Y_IN_WIDTH      [%d] ",
            m_regCmd.displayLuma.horizontalPadding.bitfields.SCALE_Y_IN_WIDTH);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma H_PHASE_MULT  [%d] ",
            m_regCmd.displayLuma.horizontalPhase.bitfields.H_PHASE_MULT);

        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma Config            [0x%x]", m_regCmd.displayLuma.config);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma horizontalSize    [0x%x]", m_regCmd.displayLuma.horizontalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma horizontalPhase   [0x%x]", m_regCmd.displayLuma.horizontalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma horizontalStripe0 [0x%x]", m_regCmd.displayLuma.horizontalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma horizontalStripe1 [0x%x]", m_regCmd.displayLuma.horizontalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma horizontalPadding [0x%x]", m_regCmd.displayLuma.horizontalPadding);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma verticalSize      [0x%x]", m_regCmd.displayLuma.verticalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma verticalPhase     [0x%x]", m_regCmd.displayLuma.verticalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma verticalStripe0   [0x%x]", m_regCmd.displayLuma.verticalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma verticalStripe1   [0x%x]", m_regCmd.displayLuma.verticalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupISP, "MNDS Disp Luma verticalPadding   [0x%x]", m_regCmd.displayLuma.verticalPadding);

        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma Config            [0x%x]", m_regCmd.displayChroma.config);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma horizontalSize    [0x%x]", m_regCmd.displayChroma.horizontalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma horizontalPhase   [0x%x]", m_regCmd.displayChroma.horizontalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma horizontalStripe0 [0x%x]", m_regCmd.displayChroma.horizontalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma horizontalStripe1 [0x%x]", m_regCmd.displayChroma.horizontalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma horizontalPadding [0x%x]", m_regCmd.displayChroma.horizontalPadding);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma verticalSize      [0x%x]", m_regCmd.displayChroma.verticalSize);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma verticalPhase     [0x%x]", m_regCmd.displayChroma.verticalPhase);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma verticalStripe0   [0x%x]", m_regCmd.displayChroma.verticalStripe0);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma verticalStripe1   [0x%x]", m_regCmd.displayChroma.verticalStripe1);
        CAMX_LOG_VERBOSE(CamxLogGroupISP,
                         "MNDS Display Chroma verticalPadding   [0x%x]", m_regCmd.displayChroma.verticalPadding);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::GetRegCmd
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID* IFEMNDS16::GetRegCmd()
{
    return &m_regCmd;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::CheckDependenceChange
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL IFEMNDS16::CheckDependenceChange(
    ISPInputData* pInputData)
{
    BOOL                    result           = FALSE;
    CropInfo*               pSensorCAMIFCrop = &pInputData->pStripeConfig->CAMIFCrop;
    CropWindow*             pInputCropWindow = &pInputData->pStripeConfig->HALCrop[m_output];
    ISPHALConfigureData*    pHALData         = &pInputData->HALData;
    ISPSensorConfigureData* pSensorData      = &pInputData->sensorData;
    StreamDimension*        pHALStream       = &pInputData->pStripeConfig->stream[0];

    if ((pInputCropWindow->left       != m_pState->cropWindow.left)       ||
        (pInputCropWindow->top        != m_pState->cropWindow.top)        ||
        (pInputCropWindow->width      != m_pState->cropWindow.width)      ||
        (pInputCropWindow->height     != m_pState->cropWindow.height)     ||
        (pSensorCAMIFCrop->firstLine  != m_pState->CAMIFCrop.firstLine)   ||
        (pSensorCAMIFCrop->firstPixel != m_pState->CAMIFCrop.firstPixel)  ||
        (pSensorCAMIFCrop->lastLine   != m_pState->CAMIFCrop.lastLine)    ||
        (pSensorCAMIFCrop->lastPixel  != m_pState->CAMIFCrop.lastPixel)   ||
        (pHALStream[m_output].width   != m_pState->streamDimension.width) ||
        (pHALStream[m_output].height  != m_pState->streamDimension.height)||
        (pHALData->format[m_output]   != m_pixelFormat)           ||
        (pSensorData->isBayer         != m_isBayer)                       ||
        (TRUE                         == pInputData->forceTriggerUpdate))
    {
        m_pState->inputWidth  = pSensorCAMIFCrop->lastPixel - pSensorCAMIFCrop->firstPixel + 1;
        m_pState->inputHeight = pSensorCAMIFCrop->lastLine - pSensorCAMIFCrop->firstLine + 1;

        // CAMIF input width for YUV sensor would be twice that of sensor width, as each pixel accounts for Y and U/V data
        if (TRUE == pSensorData->isYUV)
        {
            m_pState->inputWidth >>= 1;
        }

        // Update module class variables with new data
        m_pState->cropWindow      = *pInputCropWindow;
        m_pState->streamDimension = pHALStream[m_output];
        m_pixelFormat     = pHALData->format[m_output];
        m_isBayer         = pSensorData->isBayer;
        m_pState->CAMIFCrop       = *pSensorCAMIFCrop;
        result = TRUE;

        if ((0 == m_pState->streamDimension.width) || (0 == m_pState->streamDimension.height))
        {
            m_moduleEnable = FALSE;
            result         = FALSE;
        }
        else
        {
            m_moduleEnable = TRUE;
        }
    }

    if (TRUE == pInputData->pStripeConfig->overwriteStripes)
    {
        ISPInternalData* pFrameLevel = pInputData->pStripeConfig->pFrameLevelData->pFrameData;
        if (TRUE == pFrameLevel->scalerDependencyChanged[m_output])
        {
            m_moduleEnable = TRUE;
            result = TRUE;
        }
    }

    pInputData->pCalculatedData->scalerDependencyChanged[m_output] = result;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::RunCalculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEMNDS16::RunCalculation(
    ISPInputData* pInputData)
{
    CamxResult result = CamxResultSuccess;

    CalculateScalerOutput(pInputData);
    result = ConfigureRegisters();

    if (CamxLogGroupIQMod == (pInputData->dumpRegConfig & CamxLogGroupIQMod))
    {
        DumpRegConfig();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::UpdateIFEInternalData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID IFEMNDS16::UpdateIFEInternalData(
    ISPInputData* pInputData)
{
    IFEScalerOutput*  pCalculatedScalerOutput = &pInputData->pCalculatedData->scalerOutput[0];

    // Update the upsteam modules with previous scaler output
    pCalculatedScalerOutput[m_output].dimension.width  = m_pState->MNDSOutput.dimension.width ;
    pCalculatedScalerOutput[m_output].dimension.height = m_pState->MNDSOutput.dimension.height;
    pCalculatedScalerOutput[m_output].scalingFactor    = m_pState->MNDSOutput.scalingFactor;
    pCalculatedScalerOutput[m_output].input.width      = m_pState->inputWidth;
    pCalculatedScalerOutput[m_output].input.height     = m_pState->inputHeight;

    if (FDOutput == m_output)
    {
        pInputData->pCalculatedData->moduleEnable.frameProcessingModuleConfig.bitfields.SCALE_FD_EN = m_moduleEnable;
    }

    if (FullOutput == m_output)
    {
        pInputData->pCalculatedData->moduleEnable.frameProcessingModuleConfig.bitfields.SCALE_VID_EN = m_moduleEnable;
    }

    if (DisplayFullOutput == m_output)
    {
        pInputData->pCalculatedData->moduleEnable.frameProcessingDisplayModuleConfig.bitfields.SCALE_VID_EN = m_moduleEnable;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IFEMNDS16::Execute
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEMNDS16::Execute(
    ISPInputData* pInputData)
{
    CamxResult result;

    CAMX_ASSERT(static_cast<UINT>(m_modulePath) < IFEMaxNonCommonPaths);
    if (NULL != pInputData)
    {
        m_pState = &pInputData->pStripeConfig->stateMNDS[static_cast<UINT>(m_modulePath)];
    }
    m_pInputData = pInputData;

    if (NULL != pInputData)
    {
        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);
        if (CamxResultSuccess == result)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);
                if (CamxResultSuccess == result)
                {
                    result = CreateCmdList(pInputData);
                }
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
// IFEMNDS16::PrepareStripingParameters
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CamxResult IFEMNDS16::PrepareStripingParameters(
    ISPInputData* pInputData)
{
    CamxResult result;

    CAMX_ASSERT(static_cast<UINT>(m_modulePath) < IFEMaxNonCommonPaths);

    if (NULL != pInputData)
    {
        m_pState     = &pInputData->pStripeConfig->stateMNDS[static_cast<UINT>(m_modulePath)];
        m_pInputData = pInputData;
        // Check if dependent is valid and been updated compared to last request
        result = ValidateDependenceParams(pInputData);
        if (CamxResultSuccess == result)
        {
            if (TRUE == CheckDependenceChange(pInputData))
            {
                result = RunCalculation(pInputData);
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
// IFEMNDS16::IFEMNDS16
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEMNDS16::IFEMNDS16(
    IFEModuleCreateData* pCreateData)
{
    m_type              = ISPIQModuleType::IFEMNDS;
    m_moduleEnable      = TRUE;
    m_32bitDMILength    = 0;
    m_64bitDMILength    = 0;
    m_output            = 0;

    m_cmdLength         =
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEMNDS16FDLumaReg) / RegisterWidthInBytes)    +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEMNDS16FDChromaReg) / RegisterWidthInBytes)  +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEMNDS16VideoLumaReg) / RegisterWidthInBytes) +
        PacketBuilder::RequiredWriteRegRangeSizeInDwords(sizeof(IFEMNDS16VideoChromaReg) / RegisterWidthInBytes);

    m_modulePath = pCreateData->pipelineData.IFEPath;

    switch (m_modulePath)
    {
        case IFEPipelinePath::FDPath:
            m_output = FDOutput;
            break;

        case IFEPipelinePath::FullPath:
            m_output = FullOutput;
            break;

        case IFEPipelinePath::DS4Path:
            m_output = DS4Output;
            break;

        case IFEPipelinePath::DS16Path:
            m_output = DS16Output;
            break;

        case IFEPipelinePath::DisplayFullPath:
            m_output = DisplayFullOutput;
            break;

        case IFEPipelinePath::DisplayDS4Path:
            m_output = DisplayDS4Output;
            break;

        case IFEPipelinePath::DisplayDS16Path:
            m_output = DisplayDS16Output;
            break;

        default:
            CAMX_ASSERT_ALWAYS_MESSAGE("Invalid path");
            break;
    }

    pCreateData->initializationData.pStripeConfig->stateMNDS[static_cast<UINT>(m_modulePath)].cropWindow.left   = 1;
    pCreateData->initializationData.pStripeConfig->stateMNDS[static_cast<UINT>(m_modulePath)].cropWindow.top    = 1;
    pCreateData->initializationData.pStripeConfig->stateMNDS[static_cast<UINT>(m_modulePath)].cropWindow.width  = 1;
    pCreateData->initializationData.pStripeConfig->stateMNDS[static_cast<UINT>(m_modulePath)].cropWindow.height = 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// IFEMNDS16::~IFEMNDS16
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IFEMNDS16::~IFEMNDS16()
{
}

CAMX_NAMESPACE_END
