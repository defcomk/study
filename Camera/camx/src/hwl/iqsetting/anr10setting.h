// NOWHINE NC009 <- Shared file with system team so uses non-CamX file naming
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  anr10setting.h
/// @brief ANR1_0 module setting calculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ANR10SETTING_H
#define ANR10SETTING_H

#include "ipe_data.h"
#include "iqcommondefs.h"
#include "iqsettingutil.h"
#include "anr_1_0_0.h"
#include "ANR_Chromatix.h"
#include "ANR_Registers.h"
#include "NcLibContext.h"
#include "anr10regcmd.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class that implements ANR10 module setting calculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOWHINE NC004c: Share code with system team
class ANR10Setting
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CalculateHWSetting
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  pInput   Pointer to the input data
    /// @param  pData    Pointer to the interpolation result
    /// @param  pOutput  Pointer to the unpacked data
    ///
    /// @return TRUE for success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BOOL CalculateHWSetting(
        const ANR10InputData*                                 pInput,
        anr_1_0_0::mod_anr10_cct_dataType::cct_dataStruct*    pData,
        anr_1_0_0::chromatix_anr10_reserveType*               pReserveData,
        anr_1_0_0::chromatix_anr10Type::enable_sectionStruct* pModuleEnable,
        VOID*                                                 pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetInitializationData
    ///
    /// @brief  Get ANR initialization data
    ///
    /// @param  pData    Pointer to ANRNCLibOutputData
    ///
    /// @return TRUE for success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BOOL GetInitializationData(
        struct ANRNcLibOutputData* pData);

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// MapChromatix2ANRChromatix
    ///
    /// @brief  Map Chromatix Input to ANR defined chromatix data
    ///
    /// @param  pReserveData   Pointer to the input chromatix reserve data
    /// @param  pUnpackedData  Pointer to the input unpacked data
    /// @param  passNumMax     Maximum number of pass types used
    /// @param  pANRChromatix   Pointer to the output ANR chromatix data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID MapChromatix2ANRChromatix(
        anr_1_0_0::mod_anr10_pass_reserve_dataType*        pReserveData,
        anr_1_0_0::mod_anr10_cct_dataType::cct_dataStruct* pUnpackedData,
        UINT32                                             passNumMax,
        ANR_Chromatix*                                     pANRChromatix);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateGeometryParameters
    ///
    /// @brief  Update the geometry Parameters
    ///
    /// @param  pInput           Pointer to the input data
    /// @param  pNclibContext    Pointer to nclib context
    ///
    /// @return TRUE for success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BOOL UpdateGeometryParameters(
        const ANR10InputData* pInput,
        NCLIB_CONTEXT_ANR*    pNclibContext);
};
#endif // ANR10SETTING_H
