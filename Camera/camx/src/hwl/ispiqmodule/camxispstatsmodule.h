////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxispstatsmodule.h
/// @brief Stats class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXISPSTATSMODULE_H
#define CAMXISPSTATSMODULE_H

#include "camxpacketbuilder.h"
#include "camxmem.h"
#include "camxdefs.h"
#include "camxispiqmodule.h"

CAMX_NAMESPACE_BEGIN

static const UINT32 TapoutHDRReconstruct            = 0;    ///< HDR BHIST/BE stats tap out at HDR Recon module
static const UINT32 TapoutDemosaic                  = 1;    ///< HDR BHIST/BE stats tap out at Demosaic module
static const UINT32 MultipleFactorTwo               = 2;    ///< Region offset and dimension multiple factor for
                                                            ///  demosaic tapout
static const UINT32 MultipleFactorFour              = 4;    ///< Region offset and dimension multiple factor for
                                                            ///  HDR Recon tapout

static const UINT32 FieldSelectAllLines             = 0;    ///< Collect HDR BHist and/or HDR BE stats from all the ROI lines
static const UINT32 FieldSelectLongExposureLines    = 1;    ///< Collect HDR BHist and/or HDR BE stats from long exposure ROI lines
static const UINT32 FieldSelectShortExposureLines   = 2;    ///< Collect HDR BHist and/or HDR BE stats from short exposure ROI lines

class ISPStatsModule;

/// @brief VFE Stats Module Type
enum class ISPStatsModuleType
{
    IFEAWBBG,       ///< AWB Bayer Grid  Stats Module
    IFEBF,          ///< Bayer Focus Stats Module
    IFECS,          ///< CS Stats Module
    IFEBHist,       ///< BHist stats
    IFEHDRBE,       ///< HDR BE Stats Module
    IFEHDRBHist,    ///< HDR Bayer Histogram module
    IFEIHist,       ///< Image Histogram Module
    IFERS,          ///< RS Stats Module
    IFETintlessBG,  ///< Tintless Bayer Grid Stats Module
    BPSAWBBG,       ///< BPS AWB BG stats
    BPSHDRBHist,    ///< HDR BHist stats
};

/// @brief IFEStatsModuleCreateData
struct IFEStatsModuleCreateData
{
    ISPStatsModule*    pModule;       ///< Pointer to the Stats Module
    IFEPipelineData    pipelineData;  ///< IFE pipeline data value
};

/// @brief IFEStatsModuleCreateData
struct BPSStatsModuleCreateData
{
    ISPStatsModule*    pModule;       ///< Pointer to the Stats Module
    BPSPipelineData    pipelineData;  ///< BPS pipeline data value
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Base Class for all the Stats Module
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ISPStatsModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Execute
    ///
    /// @brief  Execute process capture request to configure individual stats module
    ///
    /// @param  pInputData Pointer to the Inputdata
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Execute(
       ISPInputData* pInputData) = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrepareStripingParameters
    ///
    /// @brief  Prepare striping parameters for striping lib
    ///
    /// @param  pInputData Pointer to the Inputdata
    ///
    /// @return CamxResultSuccess if successful.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult PrepareStripingParameters(
        ISPInputData* pInputData)
    {
        CAMX_UNREFERENCED_PARAM(pInputData);
        return CamxResultSuccess;
    }

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

        pDualIFEData->dualIFESensitive      = TRUE;
        pDualIFEData->dualIFEDMI32Sensitive = TRUE;
        pDualIFEData->dualIFEDMI64Sensitive = TRUE;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetDMITable
    ///
    /// @brief  Retrieve the buffer of the DMI table
    ///
    /// @return Pointer of the register buffer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID GetDMITable(UINT32** ppDMITable)
    {
        CAMX_UNREFERENCED_PARAM(ppDMITable);
        return;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetIQCmdLen
    ///
    /// @brief  Get the Command Size
    ///
    /// @return Length of the command list
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT GetIQCmdLength()
    {
        return m_cmdLength;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetIQCmdLen
    ///
    /// @brief  Get the DMI Size in bytes
    ///
    /// @return Length of the command list
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT GetDMILen()
    {
        return m_DMITableLength;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Get32bitDMILength
    ///
    /// @brief  Get the 32 bit DMI Size
    ///
    /// @return Length of the DMI list
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT32 Get32bitDMILength()
    {
        return m_32bitDMILength;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Set32bitDMIBufferOffset
    ///
    /// @brief  Set Offset in 32bit DMI buffer
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID Set32bitDMIBufferOffset(UINT offset)
    {
        m_32bitDMIBufferOffsetDword = offset;

        return;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Get64bitDMILength
    ///
    /// @brief  Get the 64 bit DMI Size
    ///
    /// @return Length of the DMI list
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE UINT32 Get64bitDMILength()
    {
        return m_64bitDMILength;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Set64bitDMIBufferOffset
    ///
    /// @brief  Set Offset in 64bit DMI buffer
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID Set64bitDMIBufferOffset(UINT offset)
    {
        m_64bitDMIBufferOffsetDword = offset;

        return;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetStatsType
    ///
    /// @brief  Get the Type of this Module
    ///
    /// @return module type
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ISPStatsModuleType GetStatsType()
    {
        return m_type;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Destroy
    ///
    /// @brief  Destroy the object
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE VOID Destroy()
    {
        CAMX_DELETE this;
    };

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~ISPStatsModule
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~ISPStatsModule() = default;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ISPStatsModule
    ///
    /// @brief  Constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ISPStatsModule() = default;

    UINT                m_cmdLength;                    ///< Length of the Command List
    ISPStatsModuleType  m_type;                         ///< Type of this module
    BOOL                m_moduleEnable;                 ///< Flag to indicated if this module is enabled
    UINT                m_DMITableLength;               ///< Length of DMI table
    UINT                m_32bitDMILength;               ///< The length of the 32 bit DMI Table, in bytes
    UINT                m_32bitDMIBufferOffsetDword;    ///< Offset to the 32bit DMI buffer, in Dword
    UINT                m_64bitDMILength;               ///< The length of the 64 bit DMI Table, in Dword
    UINT                m_64bitDMIBufferOffsetDword;    ///< Offset to the 64bit DMI buffer, in Dword

private:
    ISPStatsModule(const ISPStatsModule&) = delete;              ///< Disallow the copy constructor
    ISPStatsModule& operator=(const ISPStatsModule&) = delete;   ///< Disallow assignment operator
};

CAMX_NAMESPACE_END

#endif // CAMXISPSTATSMODULE_H
