////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxipeica.h
/// @brief IPEICA class declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXIPEICA_H
#define CAMXIPEICA_H

#include "camxispiqmodule.h"
#include "icasetting.h"
#include "ipe_data.h"
#include "camxosutils.h"

CAMX_NAMESPACE_BEGIN

struct ICA10ModuleData
{
    ica_1_0_0::chromatix_ica10Type* pChromatix;   ///< Pointer to ica10 chromatix data
};

struct ICA20ModuleData
{
    ica_2_0_0::chromatix_ica20Type* pChromatix;   ///< Pointer to ica20 chromatix data
};

struct ICAModuleData
{
    UINT32  titanVersion;                           ///< titan version
    UINT32  hwVersion;                              ///< hardware versions
    union
    {
        ICA20ModuleData ICA20ModData;               ///< ica20 specific module data
        ICA10ModuleData ICA10ModData;               ///< ica10 specific module data
    };
    UINT32  ICARegSize;                             ///< ICA register size
    UINT32  ICAChromatixSize;                       ///< ICA chromatix size
    UINT32  ICALUTBufferSize;                       ///< ICA LUT buffer size
    UINT32  ICAGridTransformWidth;                  ///< ICA Grid Transform width
    UINT32  ICAGridTransformHeight;                 ///< ICA Grid Transform height

    // change this guy to max of ICA10 and ICA20
    UINT    offsetLUTCmdBuffer[ICA10Indexmax];      ///< Offset of all tables within LUT CmdBuffer
    VOID*   pLUT[ICA10Indexmax];                    ///< Pointer to array of LUTs supported by ICA
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class for IPE ICA Module for ICA10 and ICA20
///
/// This IQ block adds ICA operations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IPEICA final : public ISPIQModule
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Create
    ///
    /// @brief  Create ICA Object
    ///
    /// @param  pCreateData  Pointer to resource and intialization data for ICA Creation
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static CamxResult Create(
        IPEModuleCreateData* pCreateData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize parameters
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult Initialize();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillCmdBufferManagerParams
    ///
    /// @brief  Fills the command buffer manager params needed by IQ Module
    ///
    /// @param  pInputData Pointer to the IQ Module Input data structure
    /// @param  pParam     Pointer to the structure containing the command buffer manager param to be filled by IQ Module
    ///
    /// @return CamxResultSuccess if successful
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CamxResult FillCmdBufferManagerParams(
       const ISPInputData*     pInputData,
       IQModuleCmdBufferParam* pParam);

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
    /// GetModuleData
    ///
    /// @brief  Get IQ module specific data
    ///
    /// @param  pModuleData    Pointer pointing to Module specific data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual CAMX_INLINE VOID GetModuleData(
        VOID* pModuleData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetRegCmd
    ///
    /// @brief  Retrieve the buffer of the register value
    ///
    /// @return Pointer of the register buffer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual VOID* GetRegCmd();

protected:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~IPEICA
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~IPEICA();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IPEICA
    ///
    /// @brief  Constructor
    ///
    /// @param  pNodeIdentifier     String identifier for the Node that creating this IQ Module object
    /// @param  pCreateData         Initialization data for IPEICA
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    explicit IPEICA(
        const CHAR*          pNodeIdentifier,
        IPEModuleCreateData* pCreateData);

private:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AllocateCommonLibraryData
    ///
    /// @brief  Allocate memory required for common library
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult AllocateCommonLibraryData();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DeallocateCommonLibraryData
    ///
    /// @brief  Deallocate memory required for common library
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DeallocateCommonLibraryData();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ValidateDependenceParams
    ///
    /// @brief  Validate the input dependency info from client
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if the input is valid or invalid
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ValidateDependenceParams(
        ISPInputData* pInputData);

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
    /// CheckPerspectiveDependencyChange
    ///
    /// @brief  Check to see if the Dependence Trigger Data Changed
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return BOOL Indicates if the settings have changed compared to last setting
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckPerspectiveDependencyChange(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckGridDependencyChange
    ///
    /// @brief  Check to see if the Dependence Trigger Data Changed
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return BOOL Indicates if the settings have changed compared to last setting
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckGridDependencyChange(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateLUTFromChromatix
    ///
    /// @brief  Check to see if the Dependence Trigger Data Changed
    ///
    /// @param  pLUT Pointer to start of command buffer where Look up tables are present
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateLUTFromChromatix(
        VOID* pLUT);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RunCalculation
    ///
    /// @brief  Calculate the Register Value
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult RunCalculation(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteLUTtoDMI
    ///
    /// @brief  Write Look up table data pointer into DMI header
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteLUTtoDMI(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteICA10LUTtoDMI
    ///
    /// @brief  Write Look up table data pointer into DMI header for ICA10
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteICA10LUTtoDMI(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteICA20LUTtoDMI
    ///
    /// @brief  Write Look up table data pointer into DMI header for ICA10
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult Indicates if configuration calculation was success or failure
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult WriteICA20LUTtoDMI(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckAndUpdateICA10ChromatixData
    ///
    /// @brief  Check and update the ICA10 chromatix data and return if it changed
    ///
    /// @param  pInputData     Pointer to the ISP input data
    /// @param  pTuningManager Pointer to TuningManager
    ///
    /// @return BOOL flag indicating if the chromatix pointer as changed
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckAndUpdateICA10ChromatixData(
        ISPInputData* pInputData,
        TuningDataManager* pTuningManager);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckAndUpdateICA20ChromatixData
    ///
    /// @brief  Check and update the ICA20 chromatix data and return if it changed
    ///
    /// @param  pInputData     Pointer to the ISP input data
    /// @param  pTuningManager Pointer to TuningManager
    ///
    /// @return BOOL flag indicating if the chromatix pointer as changed
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckAndUpdateICA20ChromatixData(
        ISPInputData* pInputData,
        TuningDataManager* pTuningManager);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CheckAndUpdateICAChromatixData
    ///
    /// @brief  Check and update the ICA chromatix data and return if it changed
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return BOOL flag indicating if the chromatix pointer as changed
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL CheckAndUpdateICAChromatixData(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateIPEInternalData
    ///
    /// @brief  Update Pipeline input data, such as metadata, IQSettings.
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult UpdateIPEInternalData(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// CreateCmdList
    ///
    /// @brief  Create the Command List
    ///
    /// @param  pInputData Pointer to the ISP input data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult CreateCmdList(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdatePerspectiveParamsToContext
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  pWrap                    Pointer to ICA input context
    /// @param  pICAPerspectiveParams    Pointer to ICA configuration data
    ///
    /// @return CamxResult               success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult UpdatePerspectiveParamsToContext(
        VOID* pWrap,
        VOID* pICAPerspectiveParams);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateGridParamsToContext
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  pWrap             Pointer to ICA input context
    /// @param  pICAGridParams    Pointer to ICA configuration data
    ///
    /// @return CamxResult        success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult UpdateGridParamsToContext(
        VOID* pWrap,
        VOID* pICAGridParams);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetTransformData
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  pISPInputData    Pointer to ICA configuration data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult SetTransformData(
        ISPInputData* pISPInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AllocateWarpData
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  ppWarpData    Pointer to ICA WarpData
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult AllocateWarpData(
        VOID** ppWarpData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DeAllocateWarpData
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  ppWarpData    Pointer to ICA WarpData
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DeAllocateWarpData(
        VOID** ppWarpData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AllocateWarpAssistData
    ///
    /// @brief  Allocate ICA Warp Assist Data Memory
    ///
    /// @param  ppWarpAssistData    Pointer to ICA Warp Assist Data NcLibWarpBuildAssistGridOut
    ///
    /// @return CamxResult success if the operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult AllocateWarpAssistData(
        VOID** ppWarpAssistData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DeAllocateWarpAssistData
    ///
    /// @brief  Free ICA Warp Assist Data Memory
    ///
    /// @param  ppWarpAssistData    Pointer to ICA Warp Assist Data NcLibWarpBuildAssistGridOut
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DeAllocateWarpAssistData(
        VOID** ppWarpAssistData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// AllocateWarpGeomOut
    ///
    /// @brief  Allocate memory for ICA Warp Geometry Output
    ///
    /// @param  ppWarpGeomOut    Pointer to ICA Warp Geometry Output
    ///
    /// @return CamxResult success if the operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult AllocateWarpGeomOut(
        VOID** ppWarpGeomOut);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DeAllocateWarpGeomOut
    ///
    /// @brief  Free memory allocated for ICA Warp Geometry Output
    ///
    /// @param  ppWarpGeomOut    Pointer to ICA Warp Geometry Output
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID DeAllocateWarpGeomOut(
        VOID** ppWarpGeomOut);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpInputConfiguration
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  pInputData       Pointer to ICA Input Data
    /// @param  pICAInputData    Pointer to ICA10 Input Data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    CamxResult DumpInputConfiguration(
        ISPInputData*   pInputData,
        ICAInputData*   pICAInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetModuleEnable
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  pInputData    Pointer to ICA Input Data
    ///
    /// @return CamxResult success if the write operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE CamxResult SetModuleEnable(
        ISPInputData* pInputData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// InitializeModuleDataParameters
    ///
    /// @brief  Initialize module data parameters
    ///
    /// @param  pModuleData    Pointer pointing to ICA Module specific data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID InitializeModuleDataParameters(
        ICAModuleData*    pModuleData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// DumpTuningMetadata
    ///
    /// @brief  Dump tuning meta data parameters
    ///
    /// @param  pInputData    Pointer pointing to ISPInputData
    /// @param  pLUT          ICA LUT data pointer
    ///
    /// @return CamxResult success if the dump operation is success
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult DumpTuningMetadata(
        ISPInputData*   pInputData,
        UINT32*         pLUT);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsPerspectiveEnabled
    ///
    /// @brief  Calculate the unpacked register value
    ///
    /// @param  pICAPerspectiveTransform    Pointer to perspective transform
    ///
    /// @return TRUE if perspective is enabled, Otherwise FALSE
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsPerspectiveEnabled(
        IPEICAPerspectiveTransform* pICAPerspectiveTransform)
    {
        BOOL bPerspectiveEnabled = FALSE;

        if (NULL != pICAPerspectiveTransform)
        {
            if (TRUE == pICAPerspectiveTransform->perspectiveTransformEnable)
            {
                bPerspectiveEnabled = TRUE;
            }
        }

        return bPerspectiveEnabled;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsGridEnabled
    ///
    /// @brief  is Grid Parameters Enabled
    ///
    /// @param  pICAGridTransform    Pointer to grid transform
    ///
    /// @return TRUE if the grid is enabled, Otherwise FALSE
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsGridEnabled(
        IPEICAGridTransform* pICAGridTransform)
    {
        BOOL bGridEnabled = FALSE;

        if (NULL != pICAGridTransform)
        {
            if (TRUE == pICAGridTransform->gridTransformEnable)
            {
                bGridEnabled = TRUE;
            }
        }

        return bGridEnabled;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsSupportdVersion
    ///
    /// @brief  Check to see if module supports a corresponding titan version
    ///
    ///
    /// @return BOOL TRUE if supported version
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE BOOL IsSupportdVersion()
    {
        BOOL bSupportedVersion = FALSE;

        if ((CSLTitan175 == m_moduleData.titanVersion) ||
            (CSLTitan170 == m_moduleData.titanVersion) ||
            (CSLTitan160 == m_moduleData.titanVersion) ||
            (CSLTitan150 == m_moduleData.titanVersion))
        {
            bSupportedVersion = TRUE;
        }

        return bSupportedVersion;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetChipsetVersion
    ///
    /// @brief  Check to see if SOC version for BET use case
    ///
    ///
    /// @return INT32 SOC Version
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_INLINE INT32 GetChipsetVersion()
    {
        INT32  socFd;
        CHAR   buf[32]  = { 0 };
        INT32  chipsetVersion = -1;
        CHAR   fileName[] = "/sys/devices/soc0/soc_id";
        INT    ret = 0;
#if _WINDOWS
        socFd = _open(fileName, O_RDONLY);
#else
        socFd = open(fileName, O_RDONLY);
#endif // _WINDOWS

        if (socFd > 0)
        {
#if _WINDOWS
            ret = _read(socFd, buf, sizeof(buf) - 1);
#else
            ret = read(socFd, buf, sizeof(buf) - 1);
#endif // _WINDOWS
            if (-1 == ret)
            {
                CAMX_LOG_ERROR(CamxLogGroupApp, "Unable to read soc_id");
            }
            else
            {
                chipsetVersion = atoi(buf);
            }
#if _WINDOWS
            _close(socFd);
#else
            close(socFd);
#endif // _WINDOWS
        }
        return chipsetVersion;
    }

    IPEICA(const IPEICA&)            = delete;              ///< Disallow the copy constructor
    IPEICA& operator=(const IPEICA&) = delete;              ///< Disallow assignment operator

    const CHAR*         m_pNodeIdentifier;                      ///< String identifier for the Node that created this object
    CmdBufferManager*   m_pLUTCmdBufferManager;                 ///< Command buffer manager for all LUTs of GRA
    CmdBuffer*          m_pLUTCmdBuffer;                        ///< Command buffer for holding all 4 LUTs
    INT                 m_IPEPath;                              ///< IPE path indicating input or reference.
    VOID*               m_pICAInWarpData[ICAReferenceNumber];   ///< ICA Input data for current and past frame
    VOID*               m_pICARefWarpData;                      ///< ICA Reference data
    VOID*               m_pWarpAssistData[ICAReferenceNumber];  ///< ICA warp assist data for current and past frame
    BOOL                m_enableCommonIQ;                       ///< EnableCommon IQ module
    ICAInputData        m_dependenceData;                       ///< Dependency data for ICA
    IcaParameters       m_ICAParameter;                         ///< ICA parameter

    UINT32*             m_pICAPerspectiveLUT;                   ///< Tuning LUT data
    UINT32*             m_pICAGrid0LUT;                         ///< Tuning LUT data
    UINT32*             m_pICAGrid1LUT;                         ///< Tuning LUT data
    VOID*               m_pWarpGeometryData;                    ///< Warp Geometry data
    BOOL                m_dumpICAOutput;                        ///< Dump ICA output
    BOOL                m_isICA20;                              ///< Flag to indicate ICA v2.0 h/w
    ICAModuleData       m_moduleData;                           ///< Module data for ICA
};

CAMX_NAMESPACE_END

#endif // CAMXIPEICA_H
