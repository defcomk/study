// NOWHINE NC009 <- Shared file with system team so uses non-CamX file naming
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  icasetting.cpp
/// @brief IPE ICA10 / ICA20 setting calculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "icasetting.h"
#include "Process_ICA.h"
#include "NcLibChipInfo.h"

CAMX_STATIC_ASSERT(sizeof(struct FDData) == sizeof(FD_CONFIG_CONTEXT));
static const UINT ICAMaxPerspectiveTransform           = 9;
static const UINT ICAParametersPerPerspectiveTransform = 9;
static const UINT GridAssistRows                       = 16;
static const UINT GridAssistColumns                    = 16;
static const UINT GridExtarpolateCornerSize            = 4;



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::ValidateContextParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ICASetting::ValidateContextParams(
    const ICAInputData*             pInput)
{
    CAMX_UNREFERENCED_PARAM(pInput);
    // Taken care by NClib currently

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::DumpContextParams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ICASetting::DumpContextParams(
    const ICAInputData*            pInput,
    VOID*                          pData)
{
    NcLibWarp* pWarp = static_cast<NcLibWarp*>(pInput->pCurrICAInData);
    ica_1_0_0::ica10_rgn_dataType*    pICA10Data = NULL;
    ica_2_0_0::ica20_rgn_dataType*    pICA20Data = NULL;

    if (TRUE == NcLibChipInfo::IsHana())
    {
        pICA20Data = static_cast<ica_2_0_0::ica20_rgn_dataType*>(pData);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "interpolation type %f", pICA20Data->y_interpolation_type);
    }
    else
    {
        pICA10Data = static_cast<ica_1_0_0::ica10_rgn_dataType*>(pData);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "interpolation type %f", pICA10Data->y_interpolation_type);
    }

    if (NULL != pWarp)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "warp enable %d , center %d, row %d, column %d",
                         pWarp->matrices.enable,
                         pWarp->matrices.centerType,
                         pWarp->matrices.numRows,
                         pWarp->matrices.numColumns);
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "width  %d, height %d",
                         pWarp->matrices.transformDefinedOn.widthPixels,
                         pWarp->matrices.transformDefinedOn.heightLines);
        for (UINT i = 0; i < ICAMaxPerspectiveTransform; i++)
        {
            for (UINT j = 0; j < ICAParametersPerPerspectiveTransform; j++)
            {
                CAMX_LOG_VERBOSE(CamxLogGroupIQMod, " matrices [%d] [%d] :  %f",
                                 i , j, pWarp->matrices.perspMatrices[i].T[j]);
            }
        }
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::MapChromatixMod2ICAChromatix
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL  ICASetting::MapChromatixMod2ICAChromatix(
    const ICAInputData*                     pInput,
    VOID*                                   pVReserveData,
    VOID*                                   pVData,
    VOID*                                   pChromatix)
{
    ica_1_0_0::chromatix_ica10_reserveType* pICA10ReserveData = NULL;
    ica_1_0_0::ica10_rgn_dataType*          pICA10Data        = NULL;
    ica_2_0_0::chromatix_ica20_reserveType* pICA20ReserveData = NULL;
    ica_2_0_0::ica20_rgn_dataType*          pICA20Data        = NULL;

    if (TRUE == NcLibChipInfo::IsHana())
    {
        pICA20ReserveData = static_cast<ica_2_0_0::chromatix_ica20_reserveType*>(pVReserveData);
        pICA20Data        = static_cast<ica_2_0_0::ica20_rgn_dataType*>(pVData);
        MapChromatixMod2ICAv20Chromatix(pInput, pICA20ReserveData, pICA20Data, pChromatix);
    }
    else
    {
        pICA10ReserveData = static_cast<ica_1_0_0::chromatix_ica10_reserveType*>(pVReserveData);
        pICA10Data = static_cast<ica_1_0_0::ica10_rgn_dataType*>(pVData);
        MapChromatixMod2ICAv10Chromatix(pInput, pICA10ReserveData, pICA10Data, pChromatix);
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::MapChromatixMod2ICAv10Chromatix
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL  ICASetting::MapChromatixMod2ICAv10Chromatix(
    const ICAInputData*                     pInput,
    ica_1_0_0::chromatix_ica10_reserveType* pReserveData,
    ica_1_0_0::ica10_rgn_dataType*          pData,
    VOID*                                   pChromatix)
{

    ICA_Chromatix*  pIcaChromatix = static_cast<ICA_Chromatix*>(pChromatix);

    pIcaChromatix->top.y_interpolation_type = IQSettingUtils::RoundFLOAT(pData->y_interpolation_type);

    pIcaChromatix->opg.opg_invalid_output_treatment_calculate = pReserveData->opg_invalid_output_treatment_calculate;
    pIcaChromatix->opg.opg_invalid_output_treatment_y         = pReserveData->opg_invalid_output_treatment_y;
    pIcaChromatix->opg.opg_invalid_output_treatment_cb        = pReserveData->opg_invalid_output_treatment_cb;
    pIcaChromatix->opg.opg_invalid_output_treatment_cr        = pReserveData->opg_invalid_output_treatment_cr;

    // Add static assert
    for (UINT idx = 0; idx < ICAInterpolationCoeffSets; idx++)
    {
        pIcaChromatix->opg.opg_interpolation_lut_0[idx] =
            IQSettingUtils::RoundFLOAT(pData->opg_interpolation_lut_0_tab.opg_interpolation_lut_0[idx]);
        pIcaChromatix->opg.opg_interpolation_lut_1[idx] =
            IQSettingUtils::RoundFLOAT(pData->opg_interpolation_lut_1_tab.opg_interpolation_lut_1[idx]);
        pIcaChromatix->opg.opg_interpolation_lut_2[idx] =
            IQSettingUtils::RoundFLOAT(pData->opg_interpolation_lut_2_tab.opg_interpolation_lut_2[idx]);
    }

    // Add static assert
    for (UINT idx = 0; idx < ICA10GridRegSize; idx++)
    {
        pIcaChromatix->ctc.ctc_grid_x[idx] = IQSettingUtils::RoundFLOAT(pData->ctc_grid_x_tab.ctc_grid_x[idx]);
        pIcaChromatix->ctc.ctc_grid_y[idx] = IQSettingUtils::RoundFLOAT(pData->ctc_grid_y_tab.ctc_grid_y[idx]);
        pIcaChromatix->distorted_input_to_undistorted_ldc_grid_x[idx] =
            IQSettingUtils::RoundFLOAT(
            pData->distorted_input_to_undistorted_ldc_grid_x_tab.distorted_input_to_undistorted_ldc_grid_x[idx]);
        pIcaChromatix->distorted_input_to_undistorted_ldc_grid_y[idx] =
            IQSettingUtils::RoundFLOAT(
            pData->distorted_input_to_undistorted_ldc_grid_y_tab.distorted_input_to_undistorted_ldc_grid_y[idx]);
        pIcaChromatix->undistorted_to_lens_distorted_output_ld_grid_x[idx] =
            IQSettingUtils::RoundFLOAT(
            pData->undistorted_to_lens_distorted_output_ld_grid_x_tab.undistorted_to_lens_distorted_output_ld_grid_x[idx]);
        pIcaChromatix->undistorted_to_lens_distorted_output_ld_grid_y[idx] =
            IQSettingUtils::RoundFLOAT(
            pData->undistorted_to_lens_distorted_output_ld_grid_y_tab.undistorted_to_lens_distorted_output_ld_grid_y[idx]);
    }

    if (TRUE == pInput->isGridFromChromatixEnabled)
    {
        pIcaChromatix->ctc.ctc_transform_grid_enable =
            (NULL != pInput->pChromatix) ? (static_cast <ica_1_0_0::chromatix_ica10Type*>(
                pInput->pChromatix))->enable_section.ctc_transform_grid_enable : 0;
    }
    else
    {
        pIcaChromatix->ctc.ctc_transform_grid_enable = 0;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::MapChromatixMod2ICAv20Chromatix
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL  ICASetting::MapChromatixMod2ICAv20Chromatix(
    const ICAInputData*                     pInput,
    ica_2_0_0::chromatix_ica20_reserveType* pReserveData,
    ica_2_0_0::ica20_rgn_dataType*          pData,
    VOID*                                   pChromatix)
{

    ICA_Chromatix*  pIcaChromatix = static_cast<ICA_Chromatix*>(pChromatix);

    pIcaChromatix->top.y_interpolation_type = IQSettingUtils::RoundFLOAT(pData->y_interpolation_type);

    pIcaChromatix->opg.opg_invalid_output_treatment_calculate         = pReserveData->opg_invalid_output_treatment_calculate;
    pIcaChromatix->opg.opg_invalid_output_treatment_y                 = pReserveData->opg_invalid_output_treatment_y;
    pIcaChromatix->opg.opg_invalid_output_treatment_cb                = pReserveData->opg_invalid_output_treatment_cb;
    pIcaChromatix->opg.opg_invalid_output_treatment_cr                = pReserveData->opg_invalid_output_treatment_cr;
    pIcaChromatix->distorted_input_to_undistorted_ldc_grid_valid      =
        pReserveData->distorted_input_to_undistorted_ldc_grid_valid;
    pIcaChromatix->undistorted_to_lens_distorted_output_ld_grid_valid =
        pReserveData->undistorted_to_lens_distorted_output_ld_grid_valid;

    // Add static assert
    for (UINT idx = 0; idx < ICAInterpolationCoeffSets; idx++)
    {
        pIcaChromatix->opg.opg_interpolation_lut_0[idx] =
            IQSettingUtils::RoundFLOAT(pData->opg_interpolation_lut_0_tab.opg_interpolation_lut_0[idx]);
        pIcaChromatix->opg.opg_interpolation_lut_1[idx] =
            IQSettingUtils::RoundFLOAT(pData->opg_interpolation_lut_1_tab.opg_interpolation_lut_1[idx]);
        pIcaChromatix->opg.opg_interpolation_lut_2[idx] =
            IQSettingUtils::RoundFLOAT(pData->opg_interpolation_lut_2_tab.opg_interpolation_lut_2[idx]);
    }

    // Add static assert
    for (UINT idx = 0; idx < ICA20GridRegSize; idx++)
    {
        pIcaChromatix->ctc.ctc_grid_x[idx] = IQSettingUtils::RoundFLOAT(pData->ctc_grid_x_tab.ctc_grid_x[idx]);
        pIcaChromatix->ctc.ctc_grid_y[idx] = IQSettingUtils::RoundFLOAT(pData->ctc_grid_y_tab.ctc_grid_y[idx]);

        pIcaChromatix->distorted_input_to_undistorted_ldc_grid_x[idx] =
            IQSettingUtils::RoundFLOAT(
                pData->distorted_input_to_undistorted_ldc_grid_x_tab.distorted_input_to_undistorted_ldc_grid_x[idx]);
        pIcaChromatix->distorted_input_to_undistorted_ldc_grid_y[idx] =
            IQSettingUtils::RoundFLOAT(
                pData->distorted_input_to_undistorted_ldc_grid_y_tab.distorted_input_to_undistorted_ldc_grid_y[idx]);
        pIcaChromatix->undistorted_to_lens_distorted_output_ld_grid_x[idx] =
            IQSettingUtils::RoundFLOAT(
                pData->undistorted_to_lens_distorted_output_ld_grid_x_tab.undistorted_to_lens_distorted_output_ld_grid_x[idx]);
        pIcaChromatix->undistorted_to_lens_distorted_output_ld_grid_y[idx] =
            IQSettingUtils::RoundFLOAT(
                pData->undistorted_to_lens_distorted_output_ld_grid_y_tab.undistorted_to_lens_distorted_output_ld_grid_y[idx]);
    }

    if (TRUE == pInput->isGridFromChromatixEnabled)
    {
        pIcaChromatix->ctc.ctc_transform_grid_enable =
            (NULL != pInput->pChromatix) ? (static_cast <ica_2_0_0::chromatix_ica20Type*>(
                pInput->pChromatix))->enable_section.ctc_transform_grid_enable : 0;
    }
    else
    {
        pIcaChromatix->ctc.ctc_transform_grid_enable = 0;
    }
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::CalculateHWSetting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ICASetting::CalculateHWSetting(
    const ICAInputData*                     pInput,
    VOID*                                   pData,
    VOID*                                   pReserveData,
    VOID*                                   pOutput)
{
    BOOL   result                 = TRUE;
    BOOL   icaIsGridEnabledByFlow = 0;
    BOOL   icaPerspectiveEnabled  = 0;
    UINT32 ret                    = 0;

    if ((NULL != pInput) && (NULL != pData) && (NULL != pOutput))
    {
        ICAUnpackedField*   pUnpackedField = static_cast<ICAUnpackedField*>(pOutput);
        IcaParameters*      pIcaParams     = static_cast<IcaParameters*>(pUnpackedField->pIcaParameter);

        ICA_REG_v30*        pRegData       = static_cast<ICA_REG_v30*>(pInput->pNCRegData);
        ICA_Chromatix*      pChromatixData = static_cast<ICA_Chromatix*>(pInput->pNCChromatix);

        // Intialize Warp stucture used by NcLib
        NcLibWarp*          pCurInWarp     = static_cast<NcLibWarp*>(pInput->pCurrICAInData);
        NcLibWarp*          pCurRefWarp    = static_cast<NcLibWarp*>(pInput->pCurrICARefData);
        NcLibWarp*          pPrevInWarp    = static_cast<NcLibWarp*>(pInput->pPrevICAInData);
        NcLibWarpGeomOut*   pGeomOut       = static_cast<NcLibWarpGeomOut*>(pInput->pWarpGeomOut);

        NcLibPerspTransformSingle  perspectiveMatrices[9];
        NcLibWarpGridCoord         grid[945];
        NcLibWarpGridCoord         gridExtrapolate[4];
        NcLibWarp                  warpOut;
        NcLibWarp*                 pRefWarp = pCurRefWarp;

        // Intialize Assist grid stucture used by NcLib
        NcLibWarpBuildAssistGridOut* pCurrAssist =
            static_cast<NcLibWarpBuildAssistGridOut*>(pInput->pCurWarpAssistData);
        NcLibWarpBuildAssistGridOut* pPrevAssist =
            static_cast<NcLibWarpBuildAssistGridOut*>(pInput->pPrevWarpAssistData);
        NcLibWarpBuildAssistGridIn      assistIn;
        NcLibWarpGeomIn                 geomIn;
        // Update with pointer from IQ module
        NcLibCalcMctfIn                 mctf;

        // Allocate and asssign
        IQSettingUtils::Memset(&assistIn, 0x0, sizeof(assistIn));
        IQSettingUtils::Memset(&geomIn, 0x0, sizeof(geomIn));
        IQSettingUtils::Memset(&mctf, 0x0, sizeof(mctf));

        // Set Defaults values for Firmware Struct
        SetDefaultsForIcaStruct(pIcaParams);

        // Set default values for ICA REG
        SetDefaultVal_ICA_REG_v30(pRegData, NcLibGetTypicalIcaVersion());

        // Set default chromatix data
        SetDefaultVal_ICA_Chromatix(pChromatixData);
        if (TRUE == pInput->dumpICAOut)
        {
            DumpContextParams(pInput, pData);
        }

        // map chromatix parameters or pick from input if provided
        MapChromatixMod2ICAChromatix(pInput, pReserveData, pData,  pChromatixData);

        pCurInWarp                  = static_cast<NcLibWarp*>(pInput->pCurrICAInData);

        geomIn.inputSize            = static_cast<ImageDimensions*>(pInput->pImageDimensions);
        geomIn.stabilizationMargins = static_cast<ImageDimensions*>(pInput->pMarginDimensions);
        geomIn.zoomWindow           = static_cast<IpeZoomWindow*>(pInput->pZoomWindow);
        geomIn.ifeZoomWindow        = static_cast<IpeZoomWindow*>(pInput->pIFEZoomWindow);

        // Need to get proper values for FD/ANR/TF/ alignment domain/ lnr domain
        geomIn.fdConfig                = reinterpret_cast<FD_CONFIG_CONTEXT*>(pInput->pFDData);
        geomIn.isFdConfigFromPrevFrame = 1;
        geomIn.alignmentDomain         = INPUT_IMAGE_DOMAIN;
        geomIn.ica1UpScaleRatio        = 1.0;
        geomIn.inputGridsPrevFrame     = (TRUE == geomIn.isFdConfigFromPrevFrame) ? pPrevAssist : NULL;
        geomIn.alignment               = static_cast<NcLibWarp*>(pInput->pPrevICAInData);
        geomIn.inputGrids              = pCurrAssist;

        // Pass optical center in format: 15uQ14,  where logic value 0 is start of the image and
        // OPTICAL_CENTER_LOGICAL_MAX is the end of the image.
        FLOAT optCenterXRatio = static_cast<FLOAT>(pInput->pImageDimensions->widthPixels)
                            / static_cast<FLOAT>(pInput->opticalCenterX);
        FLOAT optCenterYRatio = static_cast<FLOAT>(pInput->pImageDimensions->heightLines)
                            / static_cast<FLOAT>(pInput->opticalCenterY);

        geomIn.tf_lnr_opt_center_x  = IQSettingUtils::RoundFLOAT(OPTICAL_CENTER_LOGICAL_MAX / optCenterXRatio);
        geomIn.tf_lnr_opt_center_y  = IQSettingUtils::RoundFLOAT(OPTICAL_CENTER_LOGICAL_MAX / optCenterYRatio);
        geomIn.anr_lnr_opt_center_x = IQSettingUtils::RoundFLOAT(OPTICAL_CENTER_LOGICAL_MAX / optCenterXRatio);
        geomIn.anr_lnr_opt_center_y = IQSettingUtils::RoundFLOAT(OPTICAL_CENTER_LOGICAL_MAX / optCenterYRatio);
        geomIn.asf_opt_center_x     = IQSettingUtils::RoundFLOAT(OPTICAL_CENTER_LOGICAL_MAX / optCenterXRatio);
        geomIn.asf_opt_center_y     = IQSettingUtils::RoundFLOAT(OPTICAL_CENTER_LOGICAL_MAX / optCenterYRatio);

        if (1 == pInput->IPEPath)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "REF: %d perspective %d, perspective c %d, r %d, w %d , h %d",
                             pInput->frameNum,
                             pCurRefWarp->matrices.enable,
                             pCurRefWarp->matrices.numColumns,
                             pCurRefWarp->matrices.numRows,
                             pCurRefWarp->matrices.transformDefinedOn.widthPixels,
                             pCurRefWarp->matrices.transformDefinedOn.heightLines);

            icaIsGridEnabledByFlow = pCurRefWarp->grid.enable;
            icaPerspectiveEnabled  = pCurRefWarp->matrices.enable;
            if (TRUE == result)
            {
                // calculate MCTF matrix of current frame
                ret = NcLibWarpConvertToVirtualDomain(pCurRefWarp, pCurRefWarp);
                result = IsSuccess(ret, "NcLibWarpConvertToVirtualDomainICA2");
                CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "NcLibWarpConvertToVirtualDomain ref %d, path %d",
                                 result, pInput->IPEPath);
            }

            if (TRUE == result)
            {
                /* calculate MCTF matrix */
                warpOut.grid.grid = grid;
                warpOut.grid.gridExtrapolate   = gridExtrapolate;
                warpOut.matrices.perspMatrices = perspectiveMatrices;

                // geomIn.alignment          = pCurRefWarp; // check
                // set correct value

                mctf.alignmentDomain      = INPUT_IMAGE_DOMAIN;
                mctf.inputSize            = geomIn.inputSize;
                mctf.stabilizationMargins = geomIn.stabilizationMargins;
                mctf.alignment            = pCurRefWarp;
                mctf.inputWarp            = pCurInWarp;
                mctf.inputGridsPrevFrame  = pPrevAssist;
                mctf.inputGrids           = NULL;

                if (TRUE == CheckMctfTransformCondition(
                    static_cast<VOID*>(&mctf), static_cast<VOID*>(&warpOut), pInput->mctfEis,
                    pInput->byPassAlignmentMatrixAdjustement))
                {
                    pRefWarp = &warpOut;
                    ret = NcLibCalcMctfTransform(&mctf, &warpOut);
                    result = IsSuccess(ret, "NcLibCalcMctfTransform");
                    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "NcLibCalcMctfTransform %d, path %d", result, pInput->IPEPath);
                    if (TRUE == pInput->dumpICAOut)
                    {
                        DumpMCTFInputOutput(pInput, &mctf, &warpOut);
                    }
                }
            }

            if (TRUE == result)
            {
                // Validate parameters input to NCLib as debug settings/
                // Currently part of NCLib
                ret = ICA_ProcessNcLib(pChromatixData,
                                       static_cast<uint8_t>(icaIsGridEnabledByFlow),
                                       pRegData,
                                       static_cast<IcaParameters*>(pUnpackedField->pIcaParameter));
                result = IsSuccess(ret, "ICA_ProcessNcLib_ICA2");
                CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "ICA_ProcessNcLib : result %d", result);
            }

            if (TRUE == result)
            {
                if ((TRUE == pRefWarp->grid.enable) ||
                    (TRUE == pRefWarp->matrices.enable))
                {
                    ret = ICA_ProcessNonChromatixParams(&pRefWarp->grid,
                                                        &pRefWarp->matrices,
                                                        static_cast<uint8_t>(icaIsGridEnabledByFlow),
                                                        1,
                                                        NcLibGetTypicalIcaVersion(),
                                                        pRegData,
                                                        static_cast<IcaParameters*>(pUnpackedField->pIcaParameter));
                    result = IsSuccess(ret, "ICA_ProcessNonChromatixParams_ICA2");
                    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "ICA_ProcessNonChromatixParams : result %d", result);
                }
            }
        }
        else
        {
            CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "INPUT: %d,  perspective %d, perspective c %d, r %d, w %d , h %d",
                             pInput->frameNum,
                             pCurInWarp->matrices.enable,
                             pCurInWarp->matrices.numColumns,
                             pCurInWarp->matrices.numRows,
                             pCurInWarp->matrices.transformDefinedOn.widthPixels,
                             pCurInWarp->matrices.transformDefinedOn.heightLines);
            icaIsGridEnabledByFlow = pCurInWarp->grid.enable;
            icaPerspectiveEnabled = pCurInWarp->matrices.enable;
            if ((TRUE  == pInput->mctfEis)       &&
                ((TRUE == icaIsGridEnabledByFlow) ||
                (TRUE  == icaPerspectiveEnabled)))
            {
                // Convert input parameters to virtual domain
                ret    = NcLibWarpConvertToVirtualDomain(pCurInWarp, pCurInWarp);
                result = IsSuccess(ret, "NcLibWarpConvertToVirtualDomainICA1");
                CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "NcLibWarpConvertToVirtualDomain %d", result);
                if ((TRUE == result) && (NULL != pCurrAssist) && (NULL != pCurrAssist->assistGrid))
                {
                    if ((FALSE == icaIsGridEnabledByFlow) &&
                        (pCurInWarp->matrices.numRows * pCurInWarp->matrices.numColumns < 2))
                    {
                        pCurrAssist->assistGrid->enable = false;
                    }
                    else
                    {
                        // Build assist grid of current frame and calculate geometric parameters
                        assistIn.in         = pCurInWarp;
                        assistIn.numColumns = GridAssistColumns;
                        assistIn.numRows    = GridAssistRows;
                        ret = NcLibWarpBuildAssistGrid(&assistIn, pCurrAssist);
                        result = IsSuccess(ret, "NcLibWarpBuildAssistGrid");
                        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "NcLibWarpBuildAssistGrid %d, path %d",
                                         result, pInput->IPEPath);
                    }
                }

                if ((TRUE == result) && (NULL != pCurrAssist))
                {
                    pCurrAssist->inputWarp = pCurInWarp;
                    ret                    = NcLibWarpGeometries(&geomIn, pGeomOut);
                    result = IsSuccess(ret, "NcLibWarpGeometries");
                    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, " NcLibWarpGeometries %d, path %d",
                                     result, pInput->IPEPath);
                }
            }

            if (TRUE == result)
            {
                // Validate parameters input to NCLib as debug settings/
                // Currently part of NCLib
                ret = ICA_ProcessNcLib(pChromatixData,
                                       static_cast<uint8_t>(icaIsGridEnabledByFlow),
                                       pRegData,
                                       static_cast<IcaParameters*>(pUnpackedField->pIcaParameter));
                result = IsSuccess(ret, "ICA_ProcessNcLib_ICA1");
                CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "ICA_ProcessNcLib : result %d", result);

            }

            if (TRUE == result)
            {
                if ((TRUE == pCurInWarp->grid.enable) ||
                    (TRUE == pCurInWarp->matrices.enable))
                {
                    ret = ICA_ProcessNonChromatixParams(&pCurInWarp->grid,
                                                        &pCurInWarp->matrices,
                                                        static_cast<uint8_t>(icaIsGridEnabledByFlow),
                                                        1,
                                                        NcLibGetTypicalIcaVersion(),
                                                        pRegData,
                                                        static_cast<IcaParameters*>(pUnpackedField->pIcaParameter));
                    result = IsSuccess(ret, "ICA_ProcessNonChromatixParams_ICA1");
                    CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "ICA_ProcessNonChromatixParams : result %d", result);
                }
            }

            pUnpackedField->pCurrICAInData      = pCurInWarp;
            pUnpackedField->pPrevICAInData      = pPrevInWarp;
            pUnpackedField->pCurrWarpAssistData = pCurrAssist;
            pUnpackedField->pPrevWarpAssistData = pPrevAssist;
            pUnpackedField->pWarpGeometryData   = pGeomOut;
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::CheckMctfTransformCondition
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ICASetting::CheckMctfTransformCondition(
    VOID* pMCTF,
    VOID* pWarpOut,
    BOOL  mctfEis,
    BOOL  byPassAlignmentMatrixAdjustement)
{
    NcLibCalcMctfIn* pIn  = static_cast<NcLibCalcMctfIn*>(pMCTF);
    NcLibWarp*       pOut = static_cast<NcLibWarp*>(pWarpOut);

    if (NULL  == pIn->alignment                       ||
        NULL  == pIn->inputGridsPrevFrame             ||
        NULL  == pIn->inputGridsPrevFrame->assistGrid ||
        NULL  == pIn->inputGridsPrevFrame->inputWarp  ||
        NULL  == pIn->inputWarp                       ||
        NULL  == pIn->inputSize                       ||
        NULL  == pIn->stabilizationMargins            ||
        NULL  == pOut->matrices.perspMatrices         ||
        FALSE == mctfEis                              ||
        TRUE  == byPassAlignmentMatrixAdjustement)
    {
        CAMX_LOG_VERBOSE(CamxLogGroupIQMod, "alignment %p, inputGridsPrevFrame %p, inputWarp %p,"
                         "inputSize %p, stabilizationMargins %p, perspMatrices %p, mctfEis %d,"
                         "byPassAlignmentMatrixAdjustement %d",
                         pIn->alignment, pIn->inputGridsPrevFrame, pIn->inputWarp, pIn->inputSize,
                         pIn->stabilizationMargins, pOut->matrices.perspMatrices, mctfEis,
                         byPassAlignmentMatrixAdjustement);

        if (NULL != pIn->inputGridsPrevFrame)
        {
            CAMX_LOG_INFO(CamxLogGroupIQMod, " PassistG %p, PinputWarp %p,",
                          pIn->inputGridsPrevFrame->assistGrid,
                          pIn->inputGridsPrevFrame->inputWarp);

        }
        return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::GetInitializationData
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ICASetting::GetInitializationData(
    struct ICANcLibOutputData* pData)
{
    pData->ICAChromatixSize = (sizeof(ICA_Chromatix));
    pData->ICARegSize       = (sizeof(ICA_REG_v30));

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::DumpMatrices
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ICASetting::DumpMatrices(
    FILE* pFile,
    const NcLibWarpMatrices*    pMatrices)
{
    if (TRUE == pMatrices->enable)
    {
        CamX::OsUtils::FPrintF(pFile, "warp enable = %d\n", pMatrices->enable);
        CamX::OsUtils::FPrintF(pFile, "center      = %d\n", pMatrices->centerType);
        CamX::OsUtils::FPrintF(pFile, "confidence  = %d\n", pMatrices->confidence);
        CamX::OsUtils::FPrintF(pFile, "numColumns  = %d\n", pMatrices->numColumns);
        CamX::OsUtils::FPrintF(pFile, "numRows     = %d\n", pMatrices->numRows);
        CamX::OsUtils::FPrintF(pFile, "transformdefinedwidth = %d\n", pMatrices->transformDefinedOn.widthPixels);
        CamX::OsUtils::FPrintF(pFile, "transformdefinedwidth = %d\n", pMatrices->transformDefinedOn.heightLines);
        for (UINT i = 0; i < ICAMaxPerspectiveTransform; i++)
        {
            for (UINT j = 0; j < ICAParametersPerPerspectiveTransform; j++)
            {
                CamX::OsUtils::FPrintF(pFile, " matrices [%d] [%d] :  %f\n",
                    i, j, pMatrices->perspMatrices[i].T[j]);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::DumpGrid
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ICASetting::DumpGrid(
    FILE* pFile,
    const NcLibWarpGrid*  pGrid)
{
    if (TRUE == pGrid->enable)
    {
        CamX::OsUtils::FPrintF(pFile, "enable = %d\n", pGrid->enable);
        CamX::OsUtils::FPrintF(pFile, "num columns = %d\n", pGrid->numColumns);
        CamX::OsUtils::FPrintF(pFile, "num rows = %d\n", pGrid->numRows);
        CamX::OsUtils::FPrintF(pFile, "tranform width = %d\n", pGrid->transformDefinedOn.widthPixels);
        CamX::OsUtils::FPrintF(pFile, "tranform height = %d\n", pGrid->transformDefinedOn.heightLines);
        CamX::OsUtils::FPrintF(pFile, "extrapolateType = %d\n", pGrid->extrapolateType);
        for (UINT32 i = 0; i < (pGrid->numRows * pGrid->numColumns); i++)
        {
            CamX::OsUtils::FPrintF(pFile, "i %d, gridx %f\n", i, pGrid->grid[i].x);
            CamX::OsUtils::FPrintF(pFile, "i %d  gridy %f\n", i, pGrid->grid[i].y);
        }
        if (EXTRAPOLATION_TYPE_FOUR_CORNERS == pGrid->extrapolateType)
        {
            for (UINT32 i = 0; i < GridExtarpolateCornerSize; i++)
            {
                CamX::OsUtils::FPrintF(pFile, "corners i %d, gridx %f\n", i, pGrid->gridExtrapolate[i].x);
                CamX::OsUtils::FPrintF(pFile, "corners i %d, gridy %f\n", i, pGrid->gridExtrapolate[i].y);
            }
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ICASetting::DumpMCTFInputOutput
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID ICASetting::DumpMCTFInputOutput(
    const ICAInputData*  pInput,
    const NcLibCalcMctfIn* pIn,
    NcLibWarp* pOut)
{
    CHAR              dumpFilename[256];
    FILE*             pFile = NULL;
    const NcLibWarp*  pWarp = NULL;
    CamX::OsUtils::SNPrintF(dumpFilename, sizeof(dumpFilename),
        "%s/icamctfinout_Out_%d_path_%d_instance_%d.txt",
        CamX::ConfigFileDirectory, pInput->frameNum, pInput->IPEPath, pInput->instanceID);
    pFile = CamX::OsUtils::FOpen(dumpFilename, "w");

    if (NULL != pFile)
    {
        CamX::OsUtils::FPrintF(pFile, "--------------------------------\n");
        CamX::OsUtils::FPrintF(pFile, "-----------Input mctf parameters---------------------\n");
        CamX::OsUtils::FPrintF(pFile, "inputSize width  =  %d\n", pIn->inputSize->widthPixels);
        CamX::OsUtils::FPrintF(pFile, "inputSize height =  %d\n", pIn->inputSize->heightLines);
        CamX::OsUtils::FPrintF(pFile, "stabilizationMargins width  =  %d\n", pIn->stabilizationMargins->widthPixels);
        CamX::OsUtils::FPrintF(pFile, "stabilizationMargins height =  %d\n", pIn->stabilizationMargins->heightLines);
        CamX::OsUtils::FPrintF(pFile, "alignmentDomain  =  %d\n", pIn->alignmentDomain);
        pWarp = pIn->alignment;
        if (NULL != pWarp)
        {
            CamX::OsUtils::FPrintF(pFile, "-----------Alignment++---------------------\n");
            CamX::OsUtils::FPrintF(pFile, "-----------Alignment matrice---------------------\n");
            DumpMatrices(pFile, &pWarp->matrices);
            CamX::OsUtils::FPrintF(pFile, "-----------Alignment grid---------------------\n");
            DumpGrid(pFile, &pWarp->grid);
            CamX::OsUtils::FPrintF(pFile, "alignment direction %d\n", pWarp->direction);
            CamX::OsUtils::FPrintF(pFile, "-----------Alignment done---------------------\n");
        }
        pWarp = pIn->inputWarp;

        if (NULL != pWarp)
        {
            CamX::OsUtils::FPrintF(pFile, "-----------Input warp++---------------------\n");
            CamX::OsUtils::FPrintF(pFile, "-----------Input warp matrice---------------------\n");
            DumpMatrices(pFile, &pWarp->matrices);
            CamX::OsUtils::FPrintF(pFile, "-----------Input warp grid---------------------\n");
            DumpGrid(pFile, &pWarp->grid);
            CamX::OsUtils::FPrintF(pFile, "Input warp direction %d\n", pWarp->direction);
            CamX::OsUtils::FPrintF(pFile, "-----------Input warp done---------------------\n");
        }

        if (NULL != pIn->inputGridsPrevFrame)
        {
            if (NULL != pIn->inputGridsPrevFrame->assistGrid)
            {
                CamX::OsUtils::FPrintF(pFile, "-----------Previous Assist grid++---------------------\n");
                DumpGrid(pFile, pIn->inputGridsPrevFrame->assistGrid);
            }
            if (NULL != pIn->inputGridsPrevFrame->inputWarp)
            {
                CamX::OsUtils::FPrintF(pFile, "-----------input warp in GridsPrevFrame++---------------------\n");
                DumpMatrices(pFile, &pIn->inputGridsPrevFrame->inputWarp->matrices);
                CamX::OsUtils::FPrintF(pFile, "-----------Input warp in GridsPrevFrame grid---------------------\n");
                DumpGrid(pFile, &pIn->inputGridsPrevFrame->inputWarp->grid);
                CamX::OsUtils::FPrintF(pFile, "Input warp in GridsPrevFrame direction %d\n",
                    pIn->inputGridsPrevFrame->inputWarp->direction);
            }
            CamX::OsUtils::FPrintF(pFile, "-----------Previous Assist grid done---------------------\n");
        }
        CamX::OsUtils::FPrintF(pFile, "-----------Input mctf parameters done---------------------\n");

        CamX::OsUtils::FPrintF(pFile, "-----------Output mctf parameters---------------------\n");
        pWarp = pOut;

        if (NULL != pWarp)
        {
            CamX::OsUtils::FPrintF(pFile, "-----------output warp++---------------------\n");
            CamX::OsUtils::FPrintF(pFile, "-----------output warp matrice---------------------\n");
            DumpMatrices(pFile, &pWarp->matrices);
            CamX::OsUtils::FPrintF(pFile, "-----------output warp grid---------------------\n");
            DumpGrid(pFile, &pWarp->grid);
            CamX::OsUtils::FPrintF(pFile, "output warp direction %d\n", pWarp->direction);
            CamX::OsUtils::FPrintF(pFile, "-----------output warp done---------------------\n");
        }
        CamX::OsUtils::FPrintF(pFile, "-----------Output mctf parameters done---------------------\n");
        CamX::OsUtils::FPrintF(pFile, "--------------------------------\n");
        CamX::OsUtils::FClose(pFile);
    }
}
