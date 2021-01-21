// NOWHINE NC009 <- Shared file with system team so uses non-CamX file naming
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  tf10setting.h
/// @brief TF1_0 module setting calculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef TF10SETTING_H
#define TF10SETTING_H

#include "ipe_data.h"
#include "iqcommondefs.h"
#include "iqsettingutil.h"
#include "tf_1_0_0.h"
#include "Process_TF.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class that implements TF10 module setting calculation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOWHINE NC004c: Share code with system team
class TF10Setting
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CalculateHWSetting
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  pInput        Pointer to the input data
    /// @param  pData         Pointer to the interpolation result
    /// @param  pReserveData  Pointer to the Chromatix ReserveType field
    /// @param  pModuleEnable Pointer to the variable(s) to enable this module
    /// @param  pOutput       Pointer to the unpacked data
    ///
    /// @return TRUE for success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BOOL CalculateHWSetting(
        const TF10InputData*                                pInput,
        tf_1_0_0::mod_tf10_cct_dataType::cct_dataStruct*    pData,
        tf_1_0_0::chromatix_tf10_reserveType*               pReserveData,
        tf_1_0_0::chromatix_tf10Type::enable_sectionStruct* pModuleEnable,
        VOID*                                               pOutput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetInitializationData
    ///
    /// @brief  Get TF initialization data
    ///
    /// @param  pData         Pointer to TFNCLibOutputData
    ///
    /// @return TRUE for success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static BOOL GetInitializationData(
        struct TFNcLibOutputData* pData);

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// MapChromatix2TFChromatix
    ///
    /// @brief  Map Chromatix Input to TF defined chromatix data
    ///
    /// @param  pReserveData   Pointer to the input chromatix reserve data
    /// @param  pUnpackedData  Pointer to the input unpacked data
    /// @param  passNumMax     Maximum number of pass types used
    /// @param  pTFChromatix   Pointer to the output TF chromatix data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID MapChromatix2TFChromatix(
        tf_1_0_0::mod_tf10_pass_reserve_dataType*        pReserveData,
        tf_1_0_0::mod_tf10_cct_dataType::cct_dataStruct* pUnpackedData,
        UINT32                                           passNumMax,
        TF_Chromatix*                                    pTFChromatix);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CopyUINT32toINT32ArrayWithSize
    ///
    /// @brief  This template function copy UINT32 array to INT32 array with arraySize
    ///
    /// @param  pDestArrayName  Pointer to destination array
    /// @param  pSrcArrayName   Pointer to source array
    /// @param  arraySize       Size of array to be copied
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID CopyUINT32toINT32ArrayWithSize(
        INT32*        pDestArrayName,
        const UINT32* pSrcArrayName,
        INT           arraySize);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CopyINT32toINT32ArrayWithSize
    ///
    /// @brief  This template function copy INT32 array to INT32 array with arraySize
    ///
    /// @param  pDestArrayName  Pointer to destination array
    /// @param  pSrcArrayName   Pointer to source array
    /// @param  arraySize       Size of array to be copied
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID CopyINT32toINT32ArrayWithSize(
        INT32*       pDestArrayName,
        const INT32* pSrcArrayName,
        INT          arraySize);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CopyUINT32toUINT8ArrayWithSize
    ///
    /// @brief  This template function copy UINT32 array to UINT8 array with arraySize
    ///
    /// @param  pDestArrayName  Pointer to destination array
    /// @param  pSrcArrayName   Pointer to source array
    /// @param  arraySize       Size of array to be copied
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID CopyUINT32toUINT8ArrayWithSize(
        UINT8*        pDestArrayName,
        const UINT32* pSrcArrayName,
        INT           arraySize);
};

#endif // TF10SETTING_H
