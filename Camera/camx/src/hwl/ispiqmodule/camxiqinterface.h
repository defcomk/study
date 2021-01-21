////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxiqinterface.h
/// @brief CamX IQ Interface class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIQINTERFACE_H
#define CAMXIQINTERFACE_H

/// IQ Chromatix Headers
#include "abf_3_4_0.h"
#include "asf_3_0_0.h"
#include "bls_1_2_0.h"
#include "bpcbcc_5_0_0.h"
#include "cc_1_2_0.h"
#include "cc_1_3_0.h"
#include "cst_1_2_0.h"
#include "cv_1_2_0.h"
#include "demux_1_3_0.h"
#include "demosaic_3_6_0.h"
#include "demosaic_3_7_0.h"
#include "gamma_1_5_0.h"
#include "gamma_1_6_0.h"
#include "gtm_1_0_0.h"
#include "linearization_3_3_0.h"
#include "lsc_3_4_0.h"
#include "pdpc_1_1_0.h"
#include "pedestal_1_3_0.h"
#include "tintless_2_0_0.h"
#include "upscale_2_0_0.h"
#include "ica_2_0_0.h"
/// Camx Headers
#include "bps_data.h"
#include "camxbpsabf40.h"
#include "camxbpsbpcpdpc20.h"
#include "camxbpscc13.h"
#include "camxbpscst12.h"
#include "camxbpsdemosaic36.h"
#include "camxbpsdemux13.h"
#include "camxbpsgic30.h"
#include "camxbpshdr22.h"
#include "camxbpshnr10.h"
#include "camxbpslinearization34.h"
#include "camxbpspedestal13.h"
#include "camxbpswb13.h"
#include "camxbpslsc34.h"
#include "camxformats.h"
#include "camxifeabf34.h"
#include "camxifebls12.h"
#include "camxifebpcbcc50.h"
#include "camxifecc12.h"
#include "camxifecst12.h"
#include "camxifedemosaic36.h"
#include "camxifedemosaic37.h"
#include "camxifedemux13.h"
#include "camxipegamma15.h"
#include "camxifegamma16.h"
#include "camxifegtm10.h"
#include "camxifehdr20.h"
#include "camxifehdr22.h"
#include "camxifehdr23.h"
#include "camxifelinearization33.h"
#include "camxifelsc34.h"
#include "camxifepdpc11.h"
#include "camxifepedestal13.h"
#include "camxifewb12.h"
#include "camxipeanr10.h"
#include "camxipeasf30.h"
#include "camxipecac22.h"
#include "camxipechromaenhancement12.h"
#include "camxipechromasuppression20.h"
#include "camxipecolorcorrection13.h"
#include "camxipecolortransform12.h"
#include "camxipegamma15.h"
#include "camxipeica.h"
#include "camxipeltm13.h"
#include "camxipesce11.h"
#include "camxipetf10.h"
#include "camxipe2dlut10.h"
#include "camxipegrainadder10.h"
#include "camxipeupscaler12.h"
#include "camxipeupscaler20.h"
#include "camxtypes.h"
#include "camxipeanr10.h"

// CHI Headers
#include "chitintlessinterface.h"

CAMX_NAMESPACE_BEGIN
static const UINT32 MAX_NUM_OF_CAMERA                   = 16; // max physical camera id
static const UINT32 MAX_SENSOR_ASPECT_RATIO             = 2; // mostly sensor aspect ratio can be 4:3 or 16:9
enum class PipelineType
{
    IFE,    ///< IFE HW Pipeline
    BPS,    ///< BPS HW Pipeline
    IPE     ///< IPE HW Pipeline
};

/// @brief OEM Trigger Tag Format
struct OEMTriggerListSet
{
    CHAR   tagSessionName[125];  ///< tag Session Name 1
    CHAR   tagName[125];         ///< tag Name
};

/// @brief Function Table of the IQ Modules
struct IQOperationTable
{
    BOOL (*IQModuleInitialize)(IQLibraryData* pInitializeData);
    BOOL (*IQModuleUninitialize)(IQLibraryData* pInitializeData);
    BOOL (*IQFillOEMTuningTriggerData)(const ISPIQTriggerData*  pInput,
                                       ISPIQTuningDataBuffer*   pTriggerOEMData);
    VOID (*IQTriggerDataDump)(ISPIQTriggerData* pTriggerData);

    BOOL (*IQSetHardwareVersion)(UINT32 titanVersion, UINT32 hardwareVersion);
    BOOL (*ASF30TriggerUpdate)(ISPIQTriggerData* pInput, ASF30InputData* pOutput);
    BOOL (*ASF30Interpolation)(const ASF30InputData* pInput, asf_3_0_0::asf30_rgn_dataType* pData);
    BOOL (*ASF30CalculateHWSetting)(const ASF30InputData*                                 pInput,
                                    asf_3_0_0::asf30_rgn_dataType*                        pData,
                                    asf_3_0_0::chromatix_asf30_reserveType*               pReserveType,
                                    asf_3_0_0::chromatix_asf30Type::enable_sectionStruct* pModuleEnable,
                                    VOID*                                                 pUnpackedField);

    BOOL (*BLS12TriggerUpdate)(ISPIQTriggerData* pInput, BLS12InputData* pOutput);
    BOOL (*BLS12Interpolation)(const BLS12InputData* pInput, bls_1_2_0::bls12_rgn_dataType* pData);
    BOOL (*BLS12CalculateHWSetting)(const BLS12InputData*                                 pInput,
                                    bls_1_2_0::bls12_rgn_dataType*                        pData,
                                    bls_1_2_0::chromatix_bls12Type::enable_sectionStruct* pModuleEnable,
                                    VOID*                                                 pUnpackedField);

    BOOL (*BPSABF40TriggerUpdate)(ISPIQTriggerData* pInput, ABF40InputData* pOutput);
    BOOL (*BPSABF40Interpolation)(const ABF40InputData* pInput, abf_4_0_0::abf40_rgn_dataType* pData);
    BOOL (*BPSABF40CalculateHWSetting)(ABF40InputData*                                       pInput,
                                       abf_4_0_0::abf40_rgn_dataType*                        pData,
                                       const abf_4_0_0::chromatix_abf40_reserveType*         pReserveType,
                                       abf_4_0_0::chromatix_abf40Type::enable_sectionStruct* pModuleEnable,
                                       VOID*                                                 pUnpackedField);

    BOOL (*BPSGIC30TriggerUpdate)(ISPIQTriggerData* pInput, GIC30InputData* pOutput);
    BOOL (*BPSGIC30Interpolation)(const GIC30InputData* pInput, gic_3_0_0::gic30_rgn_dataType* pData);
    BOOL (*BPSGIC30CalculateHWSetting)(const GIC30InputData*                                 pInput,
                                       gic_3_0_0::gic30_rgn_dataType*                        pData,
                                       gic_3_0_0::chromatix_gic30_reserveType*               pReserveType,
                                       gic_3_0_0::chromatix_gic30Type::enable_sectionStruct* pModuleEnable,
                                       VOID*                                                 pUnpackedField);

    BOOL (*HDR22TriggerUpdate)(ISPIQTriggerData* pInput, HDR22InputData* pOutput);
    BOOL (*HDR22Interpolation)(const HDR22InputData* pInput, hdr_2_2_0::hdr22_rgn_dataType* pData);
    BOOL (*HDR22CalculateHWSetting)(const HDR22InputData*                                 pInput,
                                       hdr_2_2_0::hdr22_rgn_dataType*                        pData,
                                       hdr_2_2_0::chromatix_hdr22_reserveType*               pReserveType,
                                       hdr_2_2_0::chromatix_hdr22Type::enable_sectionStruct* pModuleEnable,
                                       VOID*                                                 pUnpackedField);

    BOOL (*BPSLinearization34TriggerUpdate)(ISPIQTriggerData* pInput, Linearization34IQInput* pOutput);
    BOOL (*BPSLinearization34Interpolation)(const Linearization34IQInput* pInput,
                                            linearization_3_4_0::linearization34_rgn_dataType* pData);
    BOOL (*BPSLinearization34CalculateHWSetting)(const Linearization34IQInput*                      pInput,
                                                 linearization_3_4_0::linearization34_rgn_dataType* pData,
                                                 linearization_3_4_0::chromatix_linearization34Type::enable_sectionStruct*
                                                                                                    pModuleEnable,
                                                 VOID*                                              pUnpackedField);

    BOOL (*BPSPDPC20TriggerUpdate)(ISPIQTriggerData* pInput, PDPC20IQInput* pOutput);
    BOOL (*BPSPDPC20Interpolation)(const PDPC20IQInput* pInput, pdpc_2_0_0::pdpc20_rgn_dataType* pData);
    BOOL (*BPSPDPC20CalculateHWSetting)(const PDPC20IQInput*                                    pInput,
                                        pdpc_2_0_0::pdpc20_rgn_dataType*                        pData,
                                        pdpc_2_0_0::chromatix_pdpc20Type::enable_sectionStruct* pModuleEnable,
                                        VOID*                                                   pUnpackedField);

    BOOL(*CAC22Interpolation)(const CAC22InputData* pInput, cac_2_2_0::cac22_rgn_dataType* pData);
    BOOL(*CAC22CalculateHWSetting)(const CAC22InputData*                                 pInput,
                                   cac_2_2_0::cac22_rgn_dataType*                        pData,
                                   cac_2_2_0::chromatix_cac22Type::enable_sectionStruct* pModuleEnable,
                                   VOID*                                                 pUnpackedField);

    BOOL (*CC13TriggerUpdate)(ISPIQTriggerData* pInput, CC13InputData* pOutput);
    BOOL (*CC13Interpolation)(const CC13InputData* pInput, cc_1_3_0::cc13_rgn_dataType* pData);
    BOOL (*CC13CalculateHWSetting)(const CC13InputData*                                pInput,
                                   cc_1_3_0::cc13_rgn_dataType*                        pData,
                                   cc_1_3_0::chromatix_cc13_reserveType*               pReserveType,
                                   cc_1_3_0::chromatix_cc13Type::enable_sectionStruct* pModuleEnable,
                                   VOID*                                               pUnpackedField);

    BOOL (*CST12CalculateHWSetting)(const CST12InputData*                                 pInput,
                                    cst_1_2_0::chromatix_cst12_reserveType*               pReserveType,
                                    cst_1_2_0::chromatix_cst12Type::enable_sectionStruct* pModuleEnable,
                                    VOID*                                                 pUnpackedField);

    BOOL (*CV12TriggerUpdate)(ISPIQTriggerData* pInput, CV12InputData* pOutput);
    BOOL (*CV12Interpolation)(const CV12InputData* pInput, cv_1_2_0::cv12_rgn_dataType* pData);
    BOOL (*CV12CalculateHWSetting)(const CV12InputData*                                pInput,
                                   cv_1_2_0::cv12_rgn_dataType*                        pData,
                                   cv_1_2_0::chromatix_cv12Type::enable_sectionStruct* pModuleEnable,
                                   VOID*                                               pUnpackedField);

    BOOL (*demosaic36TriggerUpdate)(ISPIQTriggerData* pInput, Demosaic36InputData* pOutput);
    BOOL (*demosaic36Interpolation)(const Demosaic36InputData* pInput, demosaic_3_6_0::demosaic36_rgn_dataType* pData);
    BOOL (*demosaic36CalculateHWSetting)(const Demosaic36InputData*                                      pInput,
                                         demosaic_3_6_0::demosaic36_rgn_dataType*                        pData,
                                         demosaic_3_6_0::chromatix_demosaic36Type::enable_sectionStruct* pModuleEnable,
                                         VOID*                                                           pUnpackedField);

    BOOL(*demosaic37TriggerUpdate)(ISPIQTriggerData* pInput, Demosaic37InputData* pOutput);
    BOOL(*demosaic37Interpolation)(const Demosaic37InputData* pInput, demosaic_3_7_0::demosaic37_rgn_dataType* pData);
    BOOL(*demosaic37CalculateHWSetting)(const Demosaic37InputData*                                      pInput,
                                        demosaic_3_7_0::demosaic37_rgn_dataType*                        pData,
                                        demosaic_3_7_0::chromatix_demosaic37Type::enable_sectionStruct* pModuleEnable,
                                        VOID*                                                           pUnpackedField);

    BOOL (*demux13CalculateHWSetting)(const Demux13InputData*                                   pInput,
                                      const demux_1_3_0::chromatix_demux13_reserveType*         pReserveType,
                                      demux_1_3_0::chromatix_demux13Type::enable_sectionStruct* pModuleEnable,
                                      VOID*                                                     pUnpackedField);

    BOOL (*gamma15TriggerUpdate)(ISPIQTriggerData* pInput, Gamma15InputData* pOutput);
    BOOL (*gamma15Interpolation)(const Gamma15InputData*                                pInput,
                                 gamma_1_5_0::mod_gamma15_cct_dataType::cct_dataStruct* pData);
    BOOL (*gamma15CalculateHWSetting)(const Gamma15InputData*                                   pInput,
                                      gamma_1_5_0::mod_gamma15_cct_dataType::cct_dataStruct*    pData,
                                      gamma_1_5_0::chromatix_gamma15Type::enable_sectionStruct* pModuleEnable,
                                      VOID*                                                     pUnpackedField);

    BOOL (*gamma16TriggerUpdate)(ISPIQTriggerData* pInput, Gamma16InputData* pOutput);
    BOOL (*gamma16Interpolation)(const Gamma16InputData* pInput, gamma_1_6_0::mod_gamma16_channel_dataType* pData);
    BOOL (*gamma16CalculateHWSetting)(const Gamma16InputData*                                   pInput,
                                      gamma_1_6_0::mod_gamma16_channel_dataType*                pChannelData,
                                      gamma_1_6_0::chromatix_gamma16Type::enable_sectionStruct* pModuleEnable,
                                      VOID*                                                     pUnpackedField);

    BOOL (*gra10TriggerUpdate)(ISPIQTriggerData* pInput, GRA10IQInput* pOutput);
    BOOL (*gra10Interpolation)(const GRA10IQInput* pInput, gra_1_0_0::gra10_rgn_dataType* pData);
    BOOL (*gra10CalculateHWSetting)(const GRA10IQInput*                                   pInput,
                                    gra_1_0_0::gra10_rgn_dataType*                        pData,
                                    gra_1_0_0::chromatix_gra10_reserveType*               pReserveData,
                                    gra_1_0_0::chromatix_gra10Type::enable_sectionStruct* pModuleEnable,
                                    VOID*                                                 pUnpackedField);

    BOOL (*GTM10TriggerUpdate)(ISPIQTriggerData* pInput, GTM10InputData* pOutput);
    BOOL (*GTM10Interpolation)(const GTM10InputData* pInput, gtm_1_0_0::gtm10_rgn_dataType* pData);
    BOOL (*GTM10CalculateHWSetting)(GTM10InputData*                                       pInput,
                                    const gtm_1_0_0::gtm10_rgn_dataType*                  pData,
                                    const gtm_1_0_0::chromatix_gtm10_reserveType*         pReserveType,
                                    gtm_1_0_0::chromatix_gtm10Type::enable_sectionStruct* pModuleEnable,
                                    VOID*                                                 pUnpackedField);

    BOOL (*HNR10TriggerUpdate)(ISPIQTriggerData* pInput, HNR10InputData* pOutput);
    BOOL (*HNR10Interpolation)(const HNR10InputData* pInput, hnr_1_0_0::hnr10_rgn_dataType* pData);
    BOOL (*HNR10CalculateHWSetting)(const HNR10InputData*                                 pInput,
                                    hnr_1_0_0::hnr10_rgn_dataType*                        pData,
                                    hnr_1_0_0::chromatix_hnr10_reserveType*               pReserveType,
                                    hnr_1_0_0::chromatix_hnr10Type::enable_sectionStruct* pModuleEnable,
                                    VOID*                                                 pUnpackedField);

    BOOL (*ICA10Interpolation)(const ICAInputData* pInput, ica_1_0_0::ica10_rgn_dataType* pData);
    BOOL (*ICA20Interpolation)(const ICAInputData* pInput, ica_2_0_0::ica20_rgn_dataType* pData);
    BOOL (*ICACalculateHWSetting)(const ICAInputData*                     pInput,
                                   VOID*                                   pData,
                                   VOID*                                   pReserveData,
                                   VOID*                                   pUnpackedField);
    BOOL (*GetICAInitializationData)(ICANcLibOutputData* pData);

    BOOL (*IFEABF34TriggerUpdate)(ISPIQTriggerData* pInput, ABF34InputData* pOutput);
    BOOL (*IFEABF34Interpolation)(const ABF34InputData* pInput, abf_3_4_0::abf34_rgn_dataType* pData);
    BOOL (*IFEABF34CalculateHWSetting)(const ABF34InputData*                                 pInput,
                                       abf_3_4_0::abf34_rgn_dataType*                        pData,
                                       abf_3_4_0::chromatix_abf34Type::enable_sectionStruct* pModuleEnable,
                                       abf_3_4_0::chromatix_abf34_reserveType*               pReserveType,
                                       VOID*                                                 pUnpackedField);

    BOOL (*IFEBPCBCC50TriggerUpdate)(ISPIQTriggerData* pInput, BPCBCC50InputData* pOutput);
    BOOL (*IFEBPCBCC50Interpolation)(const BPCBCC50InputData* pInput, bpcbcc_5_0_0::bpcbcc50_rgn_dataType* pData);
    BOOL (*IFEBPCBCC50CalculateHWSetting)(const BPCBCC50InputData*              pInput,
                                          bpcbcc_5_0_0::bpcbcc50_rgn_dataType*  pData,
                                          globalelements::enable_flag_type      moduleEnable,
                                          VOID*                                 pUnpackedField);

    BOOL (*IFECC12TriggerUpdate)(ISPIQTriggerData* pInput, CC12InputData* pOutput);
    BOOL (*IFECC12Interpolation)(const CC12InputData* pInput, cc_1_2_0::cc12_rgn_dataType* pData);
    BOOL (*IFECC12CalculateHWSetting)(const CC12InputData*                  pInput,
                                      cc_1_2_0::cc12_rgn_dataType*          pData,
                                      cc_1_2_0::chromatix_cc12_reserveType* pReserveType,
                                      VOID*                                 pUnpackedField);

    BOOL (*IFEHDR20TriggerUpdate)(ISPIQTriggerData* pInput, HDR20InputData* pOutput);
    BOOL (*IFEHDR20Interpolation)(const HDR20InputData* pInput, hdr_2_0_0::hdr20_rgn_dataType* pData);
    BOOL (*IFEHDR20CalculateHWSetting)(const HDR20InputData*                   pInput,
                                       hdr_2_0_0::hdr20_rgn_dataType*          pData,
                                       hdr_2_0_0::chromatix_hdr20_reserveType* pReserveType,
                                       VOID*                                   pUnpackedField);

    BOOL(*IFEHDR23TriggerUpdate)(ISPIQTriggerData* pInput, HDR23InputData* pOutput);
    BOOL(*IFEHDR23Interpolation)(const HDR23InputData* pInput, hdr_2_3_0::hdr23_rgn_dataType* pData);
    BOOL(*IFEHDR23CalculateHWSetting)(const HDR23InputData*                                  pInput,
                                    hdr_2_3_0::hdr23_rgn_dataType*                        pData,
                                    hdr_2_3_0::chromatix_hdr23_reserveType*               pReserveType,
                                    VOID*                                                 pUnpackedField);

    BOOL (*IFELinearization33TriggerUpdate)(ISPIQTriggerData* pInput, Linearization33InputData* pOutput);
    BOOL (*IFELinearization33Interpolation)(const Linearization33InputData* pInput,
                                            linearization_3_3_0::linearization33_rgn_dataType* pData);
    BOOL (*IFELinearization33CalculateHWSetting)(const Linearization33InputData*                    pInput,
                                                  linearization_3_3_0::linearization33_rgn_dataType* pData,
                                                  VOID*                                              pUnpackedField);

    BOOL (*LSC34TriggerUpdate)(ISPIQTriggerData* pInput, LSC34InputData* pOutput);
    BOOL (*LSC34Interpolation)(const LSC34InputData* pInput, lsc_3_4_0::lsc34_rgn_dataType* pData);
    BOOL (*LSC34CalculateHWSetting)(LSC34InputData*                                       pInput,
                                    lsc_3_4_0::lsc34_rgn_dataType*                        pData,
                                    lsc_3_4_0::chromatix_lsc34Type::enable_sectionStruct* pModuleEnable,
                                    tintless_2_0_0::tintless20_rgn_dataType*              pTintlessData,
                                    VOID*                                                 pUnpackedField);

    BOOL (*IFEPDPC11TriggerUpdate)(ISPIQTriggerData* pInput, PDPC11InputData* pOutput);
    BOOL (*IFEPDPC11Interpolation)(const PDPC11InputData* pInput, pdpc_1_1_0::pdpc11_rgn_dataType*  pData);
    BOOL (*IFEPDPC11CalculateHWSetting)(const PDPC11InputData*                                  pInput,
                                        pdpc_1_1_0::pdpc11_rgn_dataType*                        pData,
                                        pdpc_1_1_0::chromatix_pdpc11Type::enable_sectionStruct* pModuleEnable,
                                        VOID*                                                   pUnpackedField);

    BOOL (*IPECS20TriggerUpdate)(ISPIQTriggerData* pInput, CS20InputData* pOutput);
    BOOL (*IPECS20Interpolation)(const CS20InputData* pInput, cs_2_0_0::cs20_rgn_dataType* pData);
    BOOL (*IPECS20CalculateHWSetting)(const CS20InputData*                                pInput,
                                      cs_2_0_0::cs20_rgn_dataType*                        pData,
                                      cs_2_0_0::chromatix_cs20_reserveType*               pReserveType,
                                      cs_2_0_0::chromatix_cs20Type::enable_sectionStruct* pModuleEnable,
                                      VOID*                                               pUnpackedField);

    BOOL(*IPETDL10TriggerUpdate)(ISPIQTriggerData* pInput, TDL10InputData* pOutput);
    BOOL(*IPETDL10Interpolation)(const TDL10InputData* pInput, tdl_1_0_0::tdl10_rgn_dataType* pData);
    BOOL(*IPETDL10CalculateHWSetting)(const TDL10InputData*                                 pInput,
                                      tdl_1_0_0::tdl10_rgn_dataType*                        pData,
                                      tdl_1_0_0::chromatix_tdl10_reserveType*               pReserveType,
                                      tdl_1_0_0::chromatix_tdl10Type::enable_sectionStruct* pModuleEnable,
                                      VOID*                                                 pUnpackedField);

    BOOL (*pedestal13TriggerUpdate)(ISPIQTriggerData* pInput, Pedestal13InputData* pOutput);
    BOOL (*pedestal13Interpolation)(const Pedestal13InputData* pInput, pedestal_1_3_0::pedestal13_rgn_dataType* pData);
    BOOL (*pedestal13CalculateHWSetting)(const Pedestal13InputData*                                      pInput,
                                         pedestal_1_3_0::pedestal13_rgn_dataType*                        pData,
                                         pedestal_1_3_0::chromatix_pedestal13Type::enable_sectionStruct* pModuleEnable,
                                         VOID*                                                           pRegCmd);

    BOOL (*WB12CalculateHWSetting)(const WB12InputData* pInput,
                                   VOID*                pUnpackedField);

    BOOL (*WB13CalculateHWSetting)(const WB13InputData*             pInput,
                                   globalelements::enable_flag_type moduleEnable,
                                   VOID*                            pUnpackedField);

    BOOL (*SCE11Interpolation)(const SCE11InputData* pInput, sce_1_1_0::sce11_rgn_dataType*  pData);
    BOOL (*SCE11CalculateHWSetting)(const SCE11InputData*                                 pInput,
                                    sce_1_1_0::sce11_rgn_dataType*                        pData,
                                    sce_1_1_0::chromatix_sce11_reserveType*               pReserveType,
                                    sce_1_1_0::chromatix_sce11Type::enable_sectionStruct* pModuleEnable,
                                    VOID*                                                 pUnpackedField);

    BOOL (*TF10TriggerUpdate)(ISPIQTriggerData* pInput, TF10InputData* pOutput);
    BOOL (*TF10Interpolation)(const TF10InputData* pInput, tf_1_0_0::mod_tf10_cct_dataType::cct_dataStruct* pData);
    BOOL (*TF10CalculateHWSetting)(const TF10InputData*                                pInput,
                                  tf_1_0_0::mod_tf10_cct_dataType::cct_dataStruct*    pData,
                                  tf_1_0_0::chromatix_tf10_reserveType*               pReserveData,
                                  tf_1_0_0::chromatix_tf10Type::enable_sectionStruct* pModuleEnable,
                                  VOID*                                               pUnpackedField);
    BOOL (*GetTF10InitializationData)(TFNcLibOutputData* pData);


    BOOL (*ANR10TriggerUpdate)(ISPIQTriggerData* pInput, ANR10InputData* pOutput);
    BOOL (*ANR10Interpolation)(const ANR10InputData* pInput, anr_1_0_0::mod_anr10_cct_dataType::cct_dataStruct* pData);
    BOOL (*ANR10CalculateHWSetting)(const ANR10InputData*                                 pInput,
                                   anr_1_0_0::mod_anr10_cct_dataType::cct_dataStruct*    pData,
                                   anr_1_0_0::chromatix_anr10_reserveType*               pReserveData,
                                   anr_1_0_0::chromatix_anr10Type::enable_sectionStruct* pModuleEnable,
                                   VOID*                                                 pUnpackedField);
    BOOL (*GetANR10InitializationData)(ANRNcLibOutputData* pData);

    BOOL(*LTM13TriggerUpdate)(ISPIQTriggerData* pInput, LTM13InputData* pOutput);
    BOOL(*LTM13Interpolation)(const LTM13InputData* pInput, ltm_1_3_0::ltm13_rgn_dataType* pData);
    BOOL(*LTM13CalculateHWSetting)(LTM13InputData*                                       pInput,
                                   ltm_1_3_0::ltm13_rgn_dataType*                        pData,
                                   ltm_1_3_0::chromatix_ltm13_reserveType*               pReserveData,
                                   ltm_1_3_0::chromatix_ltm13Type::enable_sectionStruct* pModuleEnable,
                                   VOID*                                                 pUnpackedField);

    BOOL (*upscale20Interpolation)(const Upscale20InputData* pInput, upscale_2_0_0::upscale20_rgn_dataType* pData);
    BOOL (*upscale20CalculateHWSetting)(const Upscale20InputData*                                     pInput,
                                        upscale_2_0_0::upscale20_rgn_dataType*                        pData,
                                        upscale_2_0_0::chromatix_upscale20_reserveType*               pReserveData,
                                        upscale_2_0_0::chromatix_upscale20Type::enable_sectionStruct* pModuleEnable,
                                        VOID*                                                         pUnpackedField);

    BOOL(*upscale12CalculateHWSetting)(const Upscale12InputData*                                      pInput,
                                        VOID*                                                         pUnpackedField);

    BOOL(*TMC10TriggerUpdate)(ISPIQTriggerData* pInput, TMC10InputData* pOutput);
    BOOL(*TMC10Interpolation)(const TMC10InputData* pInput, tmc_1_0_0::tmc10_rgn_dataType*  pData);

    BOOL(*TINTLESS20Interpolation)(const LSC34InputData* pInput, tintless_2_0_0::tintless20_rgn_dataType* pData);
    BOOL(*TMC11TriggerUpdate)(ISPIQTriggerData* pInput, TMC11InputData* pOutput);
    BOOL(*TMC11Interpolation)(const TMC11InputData* pInput, tmc_1_1_0::tmc11_rgn_dataType* pData);
};

/// @brief Output Data to Demux13 IQ Algorithm
struct Demux13OutputData
{
    PipelineType type;                                         ///< Identifies the pipeline type
    union
    {
        IFEDemux13RegCmd* pRegIFECmd;                          ///< Pointer to the IFE Demux Register buffer
        BPSDemux13RegCmd* pRegBPSCmd;                          ///< Pointer to the BPS Demux Register buffer
    } regCommand;
};

/// @brief Output Data to Demosaic36 IQ Algorithm
struct Demosaic36OutputData
{
    PipelineType type;                          ///< Identifies the pipeline type
    struct
    {
        IFEDemosaic36RegCmd* pRegIFECmd;        ///< Pointer to the IFE Demosaic Register buffer
    } IFE;

    struct
    {
        DemosaicParameters*     pModuleConfig;  ///< Pointer to the BPS Demosaic module configuration
        BPSDemosaic36RegConfig* pRegBPSCmd;     ///< Pointer to the BPS Demosaic Register buffer
    } BPS;
};

/// @brief Output Data to Demosaic37 IQ Algorithm
struct Demosaic37OutputData
{
    PipelineType type;                          ///< Identifies the pipeline type
    struct
    {
        IFEDemosaic37RegCmd* pRegIFECmd;        ///< Pointer to the IFE Demosaic Register buffer
    } IFE;

    struct
    {
        DemosaicParameters*     pModuleConfig;  ///< Pointer to the BPS Demosaic module configuration
        BPSDemosaic36RegConfig* pRegBPSCmd;     ///< Pointer to the BPS Demosaic Register buffer
    } BPS;
};

/// @brief Output Data to ABF34 IQ Algorithm
struct ABF34OutputData
{
    IFEABF34RegCmd1*  pRegCmd1;     ///< Pointer to the ABF Register buffer1
    IFEABF34RegCmd2*  pRegCmd2;     ///< Pointer to the ABF Register buffer2
    UINT32*           pDMIData;     ///< Pointer to the ABF DMI data
};

/// @brief Output Data to BPCBCC50 IQ Algorithm
struct BPCBCC50OutputData
{
    IFEBPCBCC50RegCmd* pRegCmd;      ///< Pointer to the BPCBCC Register buffer
};

/// @brief Output Data to CC12 IQ Algorithm
struct CC12OutputData
{
    IFECC12RegCmd*  pRegCmd;        ///< Pointer to the ColorCorrect Register buffer
};

/// @brief Output Data to CST12
struct CST12OutputData
{
    PipelineType type;                  ///< Identifies the pipeline type
    union
    {
        IFECST12RegCmd*          pRegIFECmd; ///< Pointer to the IFE CST Register buffer
        BPSCST12RegCmd*          pRegBPSCmd; ///< Pointer to the BPS CST Register buffer
        IPEColorTransformRegCmd* pRegIPECmd; ///< Pointer to the IPE CST Register buffer
    } regCommand;
};

/// @brief Output Data to Linearization33 IQ Algorithm
struct Linearization33OutputData
{
    IFELinearization33RegCmd*  pRegCmd;               ///< Pointer to the Linearization Register buffer
    UINT64*                    pDMIDataPtr;           ///< Pointer to the DMI table
    FLOAT                      stretchGainRed;        ///< Stretch Gain Red
    FLOAT                      stretchGainGreenEven;  ///< Stretch Gain Green Even
    FLOAT                      stretchGainGreenOdd;   ///< Stretch Gain Green Odd
    FLOAT                      stretchGainBlue;       ///< Stretch Gain Blue
    BOOL                       registerBETEn;         ///< Register BET test enabled
};

/// @brief Output Data to BLS12 IQ Algorithm
struct BLS12OutputData
{
    IFEBLS12RegCmd*  pRegCmd; ///< Pointer to the BLS12 Register buffer
};

/// @brief Output Data to CC13 IQ Algorithm
struct CC13OutputData
{
    PipelineType type;  ///< Identifies the pipeline type: IFE, BPS, IPE
    union
    {
        struct
        {
            BPSCC13RegConfig*  pRegCmd; ///< Pointer to the BPS ColorCorrect Register buffer
        } BPS;

        struct
        {
            IPECC13RegConfig*  pRegCmd; ///< Pointer to the IPE ColorCorrect Register buffer
        } IPE;
    } regCommand;
};

struct LSC34State
{
    uint16_t bwidth_l;                      ///< Subgrid width, 9u, 2*Bwidth is the real width, n mean n+1
    uint16_t meshGridBwidth_l;              ///< Meshgrid width, 9u, 2*MeshGridBwidth is the real width, n mean n+1
                                            ///< not used in rolloff implementation, only as HW counters
    uint16_t lx_start_l;                    ///< Start block x index, 6u
    uint16_t bx_start_l;                    ///< Start subgrid x index within start block, 3u
    uint16_t bx_d1_l;                       ///< x coordinate of top left pixel in start block/subgrid, 9u
    uint16_t num_meshgain_h;                ///< Number of horizontal mesh gains, n mean n+2
};

/// @brief Output Data of the LSC34 IQ Algorithm
struct LSC34OutputData
{
    PipelineType type;                        ///< Identifies the pipeline type
    struct
    {
        IFELSC34RegCmd*     pRegIFECmd;           ///< Pointer to the LSC Register buffer
    } IFE;

    struct
    {
        LensRollOffParameters* pModuleConfig;    ///< Pointer to the BPS LSC module configuration
        BPSLSC34RegConfig*     pRegBPSCmd;       ///< Pointer to the BPS LSC Register buffer
    } BPS;

    LSC34UnpackedField* pUnpackedField;           ///< Pointer to the LSC unpacked field
    UINT32*             pGRRLUTDMIBuffer;         ///< Pointer to GRR DMI buffer
    UINT32*             pGBBLUTDMIBuffer;         ///< Pointer to GBB DMI buffer
    LSC34State          lscState;                 ///< LSC state, needed for striping lib
};

/// @brief Output Data to Gamma16 IQ Algorithm
struct Gamma16OutputData
{
    PipelineType type;                  ///< Identifies the pipeline type

    struct
    {
        IFEGamma16RegCmd*  pRegCmd;     ///< Pointer to the Gamma16 Register buffer
    } IFE;

    UINT32*            pRDMIDataPtr;    ///< Pointer to the R DMI table
    UINT32*            pGDMIDataPtr;    ///< Pointer to the G DMI table
    UINT32*            pBDMIDataPtr;    ///< Pointer to the B DMI table
};

/// @brief Output Data to HDR IQ Algorithem
struct HDR22OutputData
{
    PipelineType type;                  ///< Identifies the pipeline type

    struct
    {
        IFEHDR22RegCmd*  pRegCmd;       ///< Pointer to the HDR22 Register buffer
    } IFE;

    struct
    {
        BPSHDRRegCmd*       pRegCmd;                    ///< Pointer to the BPSHDR Register buffer
        HDRReconParameters* pHdrReconParameters;        ///< Pointer to FW Recon data
        HDRMacParameters*   pHdrMacParameters;          ///< Pointer to FW Mac data
    } BPS;

};

struct PDPC11State
{
    int32_t  pdaf_global_offset_x;        ///< 14u; PD pattern start global offset x
    int32_t  pdaf_x_end;                  ///< 14u; horizontal PDAF pixel end location (0 means first  pixel from left)
    int32_t  pdaf_zzHDR_first_rb_exp;     ///< 1u; 0x0: T1 (long exp), 0x1: T2 (short exp)
    uint32_t PDAF_PD_Mask[64];            ///< PD location mask for 64 32-bit words;
                                          ///< for each bit 0: not PD pixel; 1: PD pixel
};
/// @brief Output Data to PDPC11 IQ Algorithm
struct PDPC11OutputData
{
    IFEPDPC11RegCmd*  pRegCmd;       ///< Pointer to the PDPC Register buffer
    UINT32*           pDMIData;      ///< Pointer to the DMI data
    PDPC11State       pdpcState;     ///< PDPC state, needed for striping lib

};

/// @brief Output Data to BPSHNR10 IQ Algorithm
struct HNR10OutputData
{
    BPSHNR10RegConfig*  pRegCmd;                 ///< Pointer to the HNR10 Register buffer
    HnrParameters*      pHNRParameters;          ///< Pointer to the HNR parameters
    UINT32*             pLNRDMIBuffer;           ///< Pointer to the LNR Dmi data
    UINT32*             pFNRAndClampDMIBuffer;   ///< Pointer to the Merged FNR and Gain Clamp data
    UINT32*             pFNRAcDMIBuffer;         ///< Pointer to the FNR ac data
    UINT32*             pSNRDMIBuffer;           ///< Pointer to the SNR data
    UINT32*             pBlendLNRDMIBuffer;      ///< Pointer to the Blend LNR data
    UINT32*             pBlendSNRDMIBuffer;      ///< Pointer to the Blend SNR data
};

/// @brief Output Data to GTM10 IQ Algorithm
struct GTM10OutputData
{
    PipelineType type;                       ///< Identifies the pipeline type

    union
    {
        struct
        {
            IFEGTM10RegCmd* pRegCmd;          ///< Pointer to the GTM Register buffer
            UINT64*         pDMIDataPtr;      ///< Pointer to the DMI table
        } IFE;

        struct
        {
            UINT64*        pDMIDataPtr;       ///< Pointer to the DMI table
        } BPS;
    } regCmd;

    BOOL                       registerBETEn;         ///< Register BET test enabled
};

/// @brief Output Data to SCE11 IQ Algorithem
struct SCE11OutputData
{
    IPESCERegCmd* pRegCmd;  ///< Pointer to the SCE Register Buffer
};

/// @brief Output Data to GIC30 IQ Algorithm
struct GIC30OutputData
{
    BPSGIC30RegConfig*    pRegCmd;          ///< Pointer to the GIC Register buffer
    BPSGIC30ModuleConfig* pModuleConfig;    ///< Pointer to the GIC Module configuration
    UINT32*               pDMIData;         ///< Pointer to the DMI table
};

struct HDR20State
{
    UINT16 hdr_zrec_first_rb_exp;           ///< 0~1, first R/B exposure 0=T1, 1=T2
};

/// @brief Output Data to HDR20 IQ Algorithm
struct HDR20OutputData
{
    IFEHDR20RegCmd*  pRegCmd;       ///< Pointer to the HDR20 Register buffer
    HDR20State       hdr20State;    ///< HDR state, for striping lib
};

struct HDR23State
{
    UINT16 hdr_zrec_first_rb_exp;           ///< 0~1, first R/B exposure 0=T1, 1=T2
};

/// @brief Output Data to HDR20 IQ Algorithm
struct HDR23OutputData
{
    IFEHDR23RegCmd*  pRegCmd;       ///< Pointer to the HDR20 Register buffer
    HDR23State       hdr23State;    ///< HDR state, for striping lib
};

struct Ped13State
{
    uint16_t bwidth_l;                      ///< Subgrid width, 11u, 2*Bwidth is the real width   (n means n+1)
    uint16_t meshGridBwidth_l;              ///< Meshgrid width, 11u, 2*MeshGridBwidth is the real width  (n means n+1)
    uint16_t lx_start_l;                    ///< Start block x index, 4u
    uint16_t bx_start_l;                    ///< Start subgrid x index within start block, 3u
    uint16_t bx_d1_l;                       ///< x coordinate of top left pixel in start block/subgrid, 9u
};

/// @brief Output Data to Pedestal13 IQ Algorithm
struct Pedestal13OutputData
{
    PipelineType type;                                     ///< Identifies the pipeline type

    union
    {
        IFEPedestal13RegCmd*    pRegIFECmd;                ///< Pointer to the Pedestal Register buffer
        struct
        {
            BPSPedestal13RegConfig* pRegBPSCmd;            ///< Pointer to the Pedestal Register buffer
            PedestalParameters*     pPedestalParameters;   ///< Pointer to the Pedestal Register that firmware updates
        } BPS;
    } regCommand;

    UINT32* pGRRLUTDMIBuffer;                              ///< GRR LUT DMI Buffer Pointer
    UINT32* pGBBLUTDMIBuffer;                              ///< GBB LUT DMI Buffer Pointer

    Ped13State pedState;                                   ///< Pedestal state, needed for striping lib
};

/// @brief Output Data to WhiteBalance12 IQ Algorithem
struct WB12OutputData
{
    IFEWB12RegCmd*  pRegCmd;             ///< Point to the  Register buffer
};

/// @brief Output Data to WhiteBalance13 IQ Algorithem
struct WB13OutputData
{
    BPSWB13RegCmd*  pRegCmd;             ///< Point to the  Register buffer
};

/// @brief Output Data to CS20 IQ Algorithem
struct CS20OutputData
{
    IPEChromaSuppressionRegCmd* pRegCmd;        ///< Pointer to the CS20 Register Buffer
};

/// @brief Output Data to ASF30 IQ Algorithm
struct ASF30OutputData
{
    IPEASFRegCmd*  pRegCmd;           ///< Pointer to the ASF30 Register buffer
    UINT32*        pDMIDataPtr;       ///< ASF30 LUT DMI Buffer Pointer
    AsfParameters* pAsfParameters;    ///< Pointer to ASF30 Parameters for firmware
};

/// @brief Output Data to TDL10 IQ Algorithm
struct TDL10OutputData
{
    IPER2DLUTRegCmd* pRegCmd;      ///< Pointer to the TDL10 Register buffer
    UINT32*          pD2H0LUT;     ///< Pointer to D2_H_0 data
    UINT32*          pD2H1LUT;     ///< Pointer to D2_H_1 data
    UINT32*          pD2H2LUT;     ///< Pointer to D2_H_2 data
    UINT32*          pD2H3LUT;     ///< Pointer to D2_H_3 data
    UINT32*          pD2S0LUT;     ///< Pointer to D2_S_0 data
    UINT32*          pD2S1LUT;     ///< Pointer to D2_S_1 data
    UINT32*          pD2S2LUT;     ///< Pointer to D2_S_2 data
    UINT32*          pD2S3LUT;     ///< Pointer to D2_S_3 data
    UINT32*          pD1IHLUT;     ///< Pointer to D1_IH data
    UINT32*          pD1ISLUT;     ///< Pointer to D1_IS data
    UINT32*          pD1HLUT;      ///< Pointer to D1_H data
    UINT32*          pD1SLUT;      ///< Pointer to D1_S data
};

/// @brief Output Data to CV12 IQ Algorithem
struct CV12OutputData
{
    IPEChromaEnhancementRegCmd* pRegCmd;        ///< Pointer to the Color Enhancement Register buffer
};

/// @brief Output Data to Gamma15 IQ Algorithem
struct Gamma15OutputData
{
    PipelineType type;                  ///< Pipeline type
    UINT32*      pLUT[MaxGammaLUTNum];  ///< Pointer to the DMI table
};

/// @brief Output Data to Upscale20 and ChromaUp20 IQ Algorithm
struct Upscale20OutputData
{
    IPEUpscaleRegCmd* pRegCmdUpscale;        ///< Point to the Upscale20 Register buffer
    UINT32*           pDMIPtr;               ///< Point to the Upscale20 DMI data
};

/// @brief Output Data to Upscale12 IQ Algorithm
struct Upscale12OutputData
{
    IPEUpscaleLiteRegCmd* pRegCmdUpscale;        ///< Point to the Upscale12 Register buffer
};

/// @brief Output Data to CAC22
struct CAC22OutputData
{
    IPECACRegCmd* pRegCmd;    ///< Pointer to the CAC22 Register buffer
    UINT32        enableCAC2; ///< a flag after interpolation
};

/// @brief Output Data to TF10
struct TF10OutputData
{
    IPETFRegCmd*       pRegCmd;                    ///< Pointer to the TF10 All Register buffer
    UINT32             numPasses;                  ///< number of passes stored in TF10 Register buffer
};

/// @brief Output Data to ICA10 /ICA20 IQ Algorithm
struct ICAOutputData
{
    VOID*    pICAParameter;          ///< ICA parameter
    VOID*    pLUT[ICA10Indexmax];      ///< Pointer to the DMI table
    // Output of ICA1 consumed by ICA2 only
    VOID*    pCurrICAInData;         ///< ICA input current frame Data
    VOID*    pPrevICAInData;         ///< ICA reference current frame Data
    VOID*    pCurrWarpAssistData;    ///< Current Warp assist data
    VOID*    pPrevWarpAssistData;    ///< Current Warp assist data
    // Warp geometry data rrequired by ANR/TF
    VOID*    pWarpGeometryData;      ///< Warp Geometry data
};

/// @brief Output Data to ANR10
struct ANR10OutputData
{
    IPEANRRegCmd*       pRegCmd;                    ///< Pointer to the ANR10 All Register buffer
    UINT32              numPasses;                  ///< number of passes stored in TF10 Register buffer
};

// @brief Output Data to GRA10 IQ Algorithem
struct GRA10OutputData
{
    PipelineType       type;               ///< Pipeline type
    UINT32*            pLUT[GRALUTMax];    ///< Pointer to the DMI table
    UINT16             enableDitheringY;   ///< Dithering Y Enable
    UINT16             enableDitheringC;   ///< Dithering C Enable
    UINT32             grainSeed;          ///< Grain Seed Value
    UINT32             mcgA;               ///< Multiplier parameter for MCG calculation
    UINT32             skiAheadAJump;      ///< It will be equal to mcg_an mod m.
    UINT16             grainStrength;      ///< Grain Strength Value
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class that implements the CAMX IQ common library interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IQInterface
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IQSetupTriggerData
    ///
    /// @brief  Setup Trigger Data for current frame
    ///
    /// @param  pInputData          Pointer to the input data from Node
    /// @param  pNode               Pointer to the Node which calls this function
    /// @param  isRealTime          Realtime or offline
    /// @param  pIQOEMTriggerData   Pointer to tuning metadata, to be use by OEM to fill trigger data from vendor tags.
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID IQSetupTriggerData(
        ISPInputData*           pInputData,
        Node*                   pNode,
        BOOL                    isRealTime,
        ISPIQTuningDataBuffer*  pIQOEMTriggerData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetADRCParams
    ///
    /// @brief  get ADRC params.
    ///
    /// @param  pInputData          Pointer to the input data from Node
    /// @param  pAdrcEnabled        Pointer to update ADRC Enablement status
    /// @param  pGtmPercentage      Pointer to update ADRC GTM tuning percentage
    /// @param  tmcVersion          TMC version
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID GetADRCParams(
        const ISPInputData* pInputData,
        BOOL*               pAdrcEnabled,
        FLOAT*              pGtmPercentage,
        const SWTMCVersion  tmcVersion);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateAECGain
    ///
    /// @brief  update AEC Gain
    ///
    /// @param  mType          ISP IQModule Type
    /// @param  pInputData     Pointer to the input data from Node
    /// @param  gtmPercentage  ADRC GTM tuning percentage
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID UpdateAECGain(
        ISPIQModuleType mType,
        ISPInputData*   pInputData,
        FLOAT           gtmPercentage);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IQSettingModuleInitialize
    ///
    /// @brief  Initiate IQ Setting Module
    ///
    /// @param  pData Pointer to the IQ Library Data
    ///
    /// @return return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IQSettingModuleInitialize(
        IQLibInitialData* pData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IQSettingModuleUninitialize
    ///
    /// @brief  Uninitialize IQ Setting Module
    ///
    /// @param  pData Pointer to the IQ Library Data
    ///
    /// @return return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IQSettingModuleUninitialize(
        IQLibInitialData* pData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEGetSensorMode
    ///
    /// @brief  Convert Camx sensor pixel format based on common lib definition
    ///
    /// @param  pPixelFormat Pointer to the Camx Pixel Format
    /// @param  pSensorType  Pointer to the Sensor type in common library
    ///
    /// @return return       CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IFEGetSensorMode(
        const PixelFormat*  pPixelFormat,
        SensorType*         pSensorType);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetPixelFormat
    ///
    /// @brief  Convert Camx sensor pixel format based on common lib definition
    ///
    /// @param  pPixelFormat  Point to the Camx Pixel Format
    /// @param  pBayerPattern Point to the Bayer Pattern of the Sensor Input
    ///
    /// @return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult GetPixelFormat(
        const PixelFormat*  pPixelFormat,
        UINT8*              pBayerPattern);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetADRCData
    ///
    /// @brief  get adrc specific parsed data from sw chromatix.
    ///
    /// @param  pTMCInput Pointer to the TMC parsed output.
    ///
    /// @return return  BOOL
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BOOL GetADRCData(
        TMC10InputData*   pTMCInput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IQSetHardwareVersion
    ///
    /// @brief  get adrc specific parsed data from sw chromatix.
    ///
    /// @param  titanVersion    variable holding titan version.
    /// @param  hardwareVersion variable holding hardware version.
    ///
    /// @return return  BOOL
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BOOL IQSetHardwareVersion(
        UINT32 titanVersion, UINT32 hardwareVersion);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEABF34CalculateSetting
    ///
    /// @brief  API function.  Call ABF34 module in common lib for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable ABF34 operation.
    ///
    /// @param  pInput       Pointer to the Input data to the ABF34 module for calculation
    /// @param  pOEMIQData   Pointer to the OEM Input IQ Setting
    /// @param  pOutput      Pointer to the Calculation output from the ABF34 module
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IFEABF34CalculateSetting(
        const ABF34InputData*  pInput,
        VOID*                  pOEMIQData,
        ABF34OutputData*       pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Demux13CalculateSetting
    ///
    /// @brief  API function. Call IFEDemux13 Module for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable Demux operation.
    ///
    /// @param  pInput      Pointer to the Input data to the Demux13 module for calculation
    /// @param  pOEMIQData  Pointer to the OEM Input IQ Setting
    /// @param  pOutput     Pointer to the Calculation output from the Demux13 module
    /// @param  pixelFormat Pixel Format indicating the sensor output
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Demux13CalculateSetting(
        Demux13InputData*  pInput,
        VOID*              pOEMIQData,
        Demux13OutputData* pOutput,
        PixelFormat        pixelFormat);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Demosaic36CalculateSetting
    ///
    /// @brief  API function.  Call Demosaic36 module in common lib for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable Demosaic operation.
    ///
    /// @param  pInput       Pointer to the Input data to the Demosaic36 module for calculation
    /// @param  pOEMIQData   Pointer to the OEM Input IQ Setting
    /// @param  pOutput      Pointer to the Calculation output from the Demosaic36 module
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Demosaic36CalculateSetting(
        const Demosaic36InputData*  pInput,
        VOID*                       pOEMIQData,
        Demosaic36OutputData*       pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Demosaic37CalculateSetting
    ///
    /// @brief  API function.  Call Demosaic36 module in common lib for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable Demosaic operation.
    ///
    /// @param  pInput       Pointer to the Input data to the Demosaic37 module for calculation
    /// @param  pOEMIQData   Pointer to the OEM Input IQ Setting
    /// @param  pOutput      Pointer to the Calculation output from the Demosaic37 module
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Demosaic37CalculateSetting(
        const Demosaic37InputData*  pInput,
        VOID*                       pOEMIQData,
        Demosaic37OutputData*       pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEBPCBCC50CalculateSetting
    ///
    /// @brief  API function Call BPCBCC50 module in common lib for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable BPCBCC operation.
    ///
    /// @param  pInput     Pointer to the Input data to the BPCBCC50 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the Demosaic36 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IFEBPCBCC50CalculateSetting(
        const BPCBCC50InputData*  pInput,
        VOID*                     pOEMIQData,
        BPCBCC50OutputData*       pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFESetupLSC34Reg
    ///
    /// @brief  Setup LSC34 Register based on common library output
    ///
    /// @param  pInputData  Pointer to the ISP input data
    /// @param  pData       Pointer to the unpacked output data
    /// @param  pOutput     Pointer to the register output
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID IFESetupLSC34Reg(
        const ISPInputData*        pInputData,
        const LSC34UnpackedField*  pData,
        LSC34OutputData*           pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CopyLSCMapData
    ///
    /// @brief  Copy LSC map data based on LSC34 module unpacked data
    ///
    /// @param  pInputData      Pointer to the ISP input data
    /// @param  pUnpackedField  Pointer to the LSC34 module unpacked data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID CopyLSCMapData(
        const ISPInputData*    pInputData,
        LSC34UnpackedField*    pUnpackedField);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFECC12CalculateSetting
    ///
    /// @brief  API function. Call CC12 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the CC12 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the CC12 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IFECC12CalculateSetting(
        const CC12InputData* pInput,
        VOID*                pOEMIQData,
        CC12OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CST12CalculateSetting
    ///
    /// @brief  API function. Call CST12 module in common lib for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable CST12 operation.
    ///
    /// @param  pInput     Pointer to the Input data to the CST12 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the CST12 module
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult CST12CalculateSetting(
        const CST12InputData* pInput,
        VOID*                 pOEMIQData,
        CST12OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFELinearization33CalculateSetting
    ///
    /// @brief  API function. Call Linearization33 module in common lib for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable Linearization operation.
    ///
    /// @param  pInput     Pointer to the Input data to the Linearization33 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the Linearization33 module
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IFELinearization33CalculateSetting(
        const Linearization33InputData*  pInput,
        VOID*                            pOEMIQData,
        Linearization33OutputData*       pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEPDPC11CalculateSetting
    ///
    /// @brief  API function.  Call PDPC11 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the PDPC11 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the PDPC11 module
    ///
    /// @return Return CamxResult. For error code: CamxResultEUnsupported, Node should
    ///         disable PDPC11 operation.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IFEPDPC11CalculateSetting(
        const PDPC11InputData*    pInput,
        VOID*                     pOEMIQData,
        PDPC11OutputData*         pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BLS12CalculateSetting
    ///
    /// @brief  API function. Call BLS12 module in common lib for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable BLS12 operation.
    ///
    /// @param  pInput     Pointer to the Input data to the BLS12 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the BLS12 module
    ///
    /// @return Return  CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult BLS12CalculateSetting(
        const BLS12InputData* pInput,
        VOID*                 pOEMIQData,
        BLS12OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// LSC34CalculateSetting
    ///
    /// @brief  API function.  Call LSC34 module in common lib for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable LSC34 operation.
    ///
    /// @param  pInput           Pointer to the Input data to the LSC34 module for calculation
    /// @param  pOEMIQData       Pointer to the OEM Input IQ Setting
    /// @param  pInputData       Pointer to the ISP input data
    /// @param  pOutput          Pointer to the Calculation output from the LSC34 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult LSC34CalculateSetting(
        LSC34InputData*              pInput,
        VOID*                        pOEMIQData,
        const ISPInputData*          pInputData,
        LSC34OutputData*             pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Gamma16CalculateSetting
    ///
    /// @brief  API function.  Call Gamma16 module in common lib for calculation
    ///
    /// @param  pInput       Pointer to the Input data to the Gamma module for calculation
    /// @param  pOEMIQData   Pointer to the OEM Input IQ Setting
    /// @param  pOutput      Pointer to the Calculated output from the Gamma module
    /// @param  pDebugBuffer Pointer to the Debugging Buffer. In Non-debugging mode, this pointer is NULL
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Gamma16CalculateSetting(
        const Gamma16InputData*  pInput,
        VOID*                    pOEMIQData,
        Gamma16OutputData*       pOutput,
        VOID*                    pDebugBuffer);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSHNR10CalculateSetting
    ///
    /// @brief  API function.  Call HNR10 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the HNR10 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the HNR10 module
    ///
    /// @return Return  CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult BPSHNR10CalculateSetting(
        const HNR10InputData*  pInput,
        VOID*                  pOEMIQData,
        HNR10OutputData*       pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GTM10CalculateSetting
    ///
    /// @brief  API function.  Call GTM10 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the GTM10 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the GTM10 module
    /// @param  pTMCInput  Pointer to the Calculation output from the TMC10 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult GTM10CalculateSetting(
        GTM10InputData*   pInput,
        VOID*             pOEMIQData,
        GTM10OutputData*  pOutput,
        TMC10InputData*   pTMCInput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SCE11CalculateSetting
    ///
    /// @brief  API function.  Call SCE11 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the SCE11 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the SCE11 module
    ///
    /// @return Return CamxResult
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult SCE11CalculateSetting(
        const SCE11InputData* pInput,
        VOID*                 pOEMIQData,
        SCE11OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CC13CalculateSetting
    ///
    /// @brief  API function. Call CC13 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the CC13 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the CC13 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult CC13CalculateSetting(
        const CC13InputData* pInput,
        VOID*                pOEMIQData,
        CC13OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSGIC30CalculateSetting
    ///
    /// @brief  API function.  Call GIC30 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the input data to the GIC30 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the calculated output from the GIC30 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult BPSGIC30CalculateSetting(
        const GIC30InputData* pInput,
        VOID*                 pOEMIQData,
        GIC30OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEHDR20CalculateSetting
    ///
    /// @brief  API function.  Call HDR20 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the HDR20 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the HDR20 module
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IFEHDR20CalculateSetting(
        const HDR20InputData* pInput,
        VOID*                 pOEMIQData,
        HDR20OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPECS20CalculateSetting
    ///
    /// @brief  API function.  Call CS20 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the CS20 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the CS20 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPECS20CalculateSetting(
        const CS20InputData* pInput,
        VOID*                pOEMIQData,
        CS20OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IFEWB12CalculateSetting
    ///
    /// @brief  API function.  Call WB12 module in common lib for calculation
    ///
    /// @param  pInput  Pointer to the Input data to the WB12 module for calculation
    /// @param  pOutput Pointer to the Calculation output from the WB12 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IFEWB12CalculateSetting(
        const WB12InputData*  pInput,
        WB12OutputData*       pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSWB13CalculateSetting
    ///
    /// @brief  API function.  Call WB13 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the WB13 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the WB13 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult BPSWB13CalculateSetting(
        const WB13InputData*  pInput,
        VOID*                 pOEMIQData,
        WB13OutputData*       pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Pedestal13CalculateSetting
    ///
    /// @brief  API function.  Call Pedestal36 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the Pedestal13 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the Pedestal13 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Pedestal13CalculateSetting(
        const Pedestal13InputData* pInput,
        VOID*                      pOEMIQData,
        Pedestal13OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// HDR22CalculateSetting
    ///
    /// @brief  API function.  Call HDR22 module in common lib for calculation
    ///
    /// @param  pInput     Point to the Input data to the HDR22 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Point to the Calculation output from the BPSHDR22 module
    ///
    /// @return Return CamxResult. For error code: CamxResultEUnsupported, Node should
    ///         disable BPSHDR22 operation.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult HDR22CalculateSetting(
        const HDR22InputData* pInput,
        VOID*                 pOEMIQData,
        HDR22OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// HDR23CalculateSetting
    ///
    /// @brief  API function.  Call HDR23 module in common lib for calculation
    ///
    /// @param  pInput     Point to the Input data to the HDR23 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Point to the Calculation output from the HDR23 module
    ///
    /// @return Return CamxResult. For error code: CamxResultEUnsupported, Node should
    ///         disable HDR23 operation.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult HDR23CalculateSetting(
        const HDR23InputData* pInput,
        VOID*                 pOEMIQData,
        HDR23OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSLinearization34CalculateSetting
    ///
    /// @brief  API function.  Call Linearization34 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the Linearization34 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the Linearization34 module
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult BPSLinearization34CalculateSetting(
        const Linearization34IQInput* pInput,
        VOID*                         pOEMIQData,
        Linearization34OutputData*    pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSABF40CalculateSetting
    ///
    /// @brief  API function.  Call ABF40 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the input data to the ABF40 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the calculated output from the ABF40 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult BPSABF40CalculateSetting(
        ABF40InputData*  pInput,
        VOID*            pOEMIQData,
        ABF40OutputData* pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// BPSBPCPDPC20CalculateSetting
    ///
    /// @brief  API function. Call BPCPDPC20 module in common lib for calculation
    ///
    /// @param  pInput      Pointer to the Input data to the BPCPDPC module for calculation
    /// @param  pOEMIQData  Pointer to the OEM Input IQ Setting
    /// @param  pOutput     Pointer to the Calculation output from the BPCPDPC20 module
    /// @param  pixelFormat Pixel format from the sensor
    ///
    /// @return Return CamxResult. For error code: CamxResultEUnsupported, Node should
    ///         disable BPCPDPC operation.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult BPSBPCPDPC20CalculateSetting(
        PDPC20IQInput*          pInput,
        VOID*                   pOEMIQData,
        BPSBPCPDPC20OutputData* pOutput,
        PixelFormat             pixelFormat);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEASF30CalculateSetting
    ///
    /// @brief  API function.  Call ASF30 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the ASF30 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the Linearization34 module
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPEASF30CalculateSetting(
        const ASF30InputData* pInput,
        VOID*                 pOEMIQData,
        ASF30OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// TDL10CalculateSetting
    ///
    /// @brief  API function. Call TDL10 module in common lib for calculation
    ///         For error code: CamxResultEUnsupported, Node should disable TDL10 operation.
    ///
    /// @param  pInput     Pointer to the Input data to the TDL10 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the TDL10 module
    ///
    /// @return Return  CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult TDL10CalculateSetting(
        const TDL10InputData* pInput,
        VOID*                 pOEMIQData,
        TDL10OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPECV12CalculateSetting
    ///
    /// @brief  API function. Call CV12 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the CV12 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the CV12 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPECV12CalculateSetting(
        const CV12InputData* pInput,
        VOID*                pOEMIQData,
        CV12OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEGamma15CalculateSetting
    ///
    /// @brief  API function. Call Gamma15 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the Gamma15 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the Gamma15 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPEGamma15CalculateSetting(
        const Gamma15InputData* pInput,
        VOID*                   pOEMIQData,
        Gamma15OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPECAC22CalculateSetting
    ///
    /// @brief  API function.  Call CAC22 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the CAC22 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the CAC22 module
    ///
    /// @return Return CamxResult. For error code: CamxResultEUnsupported, Node should
    ///         disable CAC22 operation.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPECAC22CalculateSetting(
        const CAC22InputData* pInput,
        VOID*                 pOEMIQData,
        CAC22OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPETF10CalculateSetting
    ///
    /// @brief  API function.  Call TF10 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the TF10 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the TF10 module
    ///
    /// @return Return CamxResult. For error code: CamxResultEUnsupported, Node should
    ///         disable TF10 operation.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPETF10CalculateSetting(
        const TF10InputData* pInput,
        VOID*                pOEMIQData,
        TF10OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPETF10GetInitializationData
    ///
    /// @brief  API function.  Call TF10 module in common lib to get Initialization Data
    ///
    /// @param  pData     Pointer to variable that holds initialization data from common library
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPETF10GetInitializationData(
        struct TFNcLibOutputData* pData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ICA10CalculateSetting
    ///
    /// @brief  API function. Call ICA module in common lib for calculation
    ///
    /// @param  pInput         Pointer to the Input data to the ICA module for calculation
    /// @param  pOutput        Pointer to the Calculation output from the ICA module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult ICA10CalculateSetting(
        const ICAInputData*  pInput,
        ICAOutputData*       pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ICA20CalculateSetting
    ///
    /// @brief  API function. Call ICA20 module in common lib for calculation
    ///
    /// @param  pInput         Pointer to the Input data to the ICA module for calculation
    /// @param  pOutput        Pointer to the Calculation output from the ICA module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult ICA20CalculateSetting(
        const ICAInputData*   pInput,
        ICAOutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEICAGetInitializationData
    ///
    /// @brief  API function.  Call ICA module in common lib to get Initialization Data
    ///
    /// @param  pData     Pointer to variable that holds initialization data from common library
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPEICAGetInitializationData(
        struct ICANcLibOutputData* pData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEANR10CalculateSetting
    ///
    /// @brief  API function. Call ANR10 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the ANR10 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the ANR10 module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPEANR10CalculateSetting(
        const ANR10InputData* pInput,
        VOID*                 pOEMIQData,
        ANR10OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEANR10GetInitializationData
    ///
    /// @brief  API function.  Call ANR10 module in common lib to get Initialization Data
    ///
    /// @param  pData     Pointer to variable that holds initialization data from common library
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPEANR10GetInitializationData(
        struct ANRNcLibOutputData* pData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPELTM13CalculateSetting
    ///
    /// @brief  API function. Call IPELTM13 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the IPELTM13 module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the IPELTM13 module
    /// @param  pTMCInput  Pointer to the Calculation output from the TMC10 module
    ///
    /// @return Return CamxResult. For error code: CamxResultEUnsupported, Node should
    ///         disable IPELTM13 operation.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPELTM13CalculateSetting(
        LTM13InputData*       pInput,
        VOID*                 pOEMIQData,
        LTM13OutputData*      pOutput,
        TMC10InputData*       pTMCInput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEUpscale12CalculateSetting
    ///
    /// @brief  API function.  Call Upscale12/ChromaUp12 modules in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the Upscale12/ChromaUp12 modules for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the Upscale12/ChromaUp12 modules
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPEUpscale12CalculateSetting(
        const Upscale12InputData* pInput,
        VOID*                     pOEMIQData,
        Upscale12OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// TMC11CalculateSetting
    ///
    /// @brief  API function. Call TMC11 module in common lib for calculation
    ///
    /// @param  pInput  Pointer to the Input data to the TMC11 module for calculation
    ///
    /// @return Return CamxResult. For error code: CamxResultEUnsupported, Node should disable TMC11 operation.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult TMC11CalculateSetting(
        TMC11InputData*  pInput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEUpscale20CalculateSetting
    ///
    /// @brief  API function.  Call Upscale20/ChromaUp20 modules in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the Upscale20/ChromaUp20 modules for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculation output from the Upscale20/ChromaUp20 modules
    ///
    /// @return Return CamxResult.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPEUpscale20CalculateSetting(
        const Upscale20InputData* pInput,
        VOID*                     pOEMIQData,
        Upscale20OutputData*      pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEGRA10CalculateSetting
    ///
    /// @brief  API function.  Call GRA10 module in common lib for calculation
    ///
    /// @param  pInput     Pointer to the Input data to the GRA module for calculation
    /// @param  pOEMIQData Pointer to the OEM Input IQ Setting
    /// @param  pOutput    Pointer to the Calculated output from the GRA module
    ///
    /// @return Return CamxResult
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult IPEGRA10CalculateSetting(
        const GRA10IQInput* pInput,
        VOID*               pOEMIQData,
        GRA10OutputData*    pOutput);

    // Data members
    static IQOperationTable s_interpolationTable;    ///< IQ Module Function Table
    static LSC34UnpackedField s_prevTintlessOutput[MAX_NUM_OF_CAMERA][MAX_SENSOR_ASPECT_RATIO];  ///< Store prev Tintless
    static BOOL s_prevTintlessTableValid[MAX_NUM_OF_CAMERA][MAX_SENSOR_ASPECT_RATIO];
};

CAMX_NAMESPACE_END

#endif // CAMXIQINTERFACE_H
