////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxchinodeeisv2.h
/// @brief Chi node for Image stabalization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXCHINODEEISV2_H
#define CAMXCHINODEEISV2_H

#include "camxchinodeutil.h"
#include "chieisdefs.h"
#include "chiipedefs.h"
#include "chiifedefs.h"
#include "chinode.h"
#include "eis2_interface.h"
#include "tuningsetmanager.h"
#if !_WINDOWS
#include <cutils/properties.h>
#endif

/// @brief structure to contain all the vendor tag Ids that EISv2 uses
struct EISV2VendorTags
{
    UINT32 ICAInPerspectiveTransformTagId;  ///< ICA In Perspective Transformation Vendor Tag Id
    UINT32 residualCropTagId;               ///< IFE Residual Crop Info Vendor Tag Id
    UINT32 appliedCropTagId;                ///< IFE Applied Crop Info Vendor Tag Id
    UINT32 mountAngleTagId;                 ///< Camera Sensor Mount Angle Vendor Tag Id
    UINT32 cameraPositionTagId;             ///< Camera Sensor Position Vendor Tag Id
    UINT32 QTimerTimestampTagId;            ///< QTimer SOF Timestamp Vendor Tag Id
    UINT32 sensorInfoTagId;                 ///< Sensor Info Vendor Tag Id
    UINT32 previewStreamDimensionsTagId;    ///< Output preview stream dimensions Vendor Tag Id
    UINT32 videoStreamDimensionsTagId;      ///< Output video stream dimensions Vendor Tag Id
    UINT32 EISV2EnabledTagId;               ///< EISv2 enabled flag Vendor Tag Id
    UINT32 EISV2FrameDelayTagId;            ///< EISv2 frame delay Vendor Tag Id
    UINT32 EISV2RequestedMarginTagId;       ///< EISv2 Requested Margin Vendor Tag Id
    UINT32 EISV2StabilizationMarginsTagId;  ///< EISv2 actual stabilization margins Vendor Tag Id
    UINT32 EISV2AdditionalCropOffsetTagId;  ///< EISv2 additional crop offset Vendor Tag Id
    UINT32 EISV2AlgoCompleteTagId;          ///< EISv2 Algorithm completed flag for EISV2 Vendor Tag Id
    UINT32 currentSensorModeTagId;          ///< Sensor's current mode index Vendor Tag Id
    UINT32 EISV2StabilizedOutputDimsTagId;  ///< EISv2 Stabilized Output Dimensions Vendor Tag Id
    UINT32 ICAInGridTransformTagId;         ///< ICA In Grid Transformation Vendor Tag Id
};

/// @brief structure to contain all the per sensor data required for EISv2 algortihm
struct EISV2PerSensorData
{
    CHINODEIMAGEFORMAT              inputSize;               ///< Image size
    CHINODEIMAGEFORMAT              outputSize;              ///< Output Image size
    CHINODEIMAGEFORMAT              sensorDimension;         ///< Output Sensor Dimension
    TuningSetManager*               pTuningManager;          ///< Pointer to tuning manager
    eis_1_1_0::chromatix_eis11Type* pEISChromatix;           ///< pointer to EIS chromatix
    ica_2_0_0::chromatix_ica20Type* pICAChromatix;           ///< pointer to ICA20 chromatix
    UINT32                          mountAngle;              ///< Sensor mount angle
    INT32                           cameraPosition;          ///< Camera sensor position
    UINT32                          targetFPS;               ///< Target Frame Rate
};

/// @brief structure to contain eisv3 override settings
struct EISV2OverrideSettings
{
    MarginRequest               margins;            ///< Margins
    cam_is_operation_mode_t     algoOperationMode;  ///< Calibration mode
    BOOL                        isGyroDumpEnabled;  ///< Flag to indicate if Gyro Dump enabled
    BOOL                        isLDCGridEnabled;   ///< Flag for LDC grid enable
};

/// @brief EIS2 path type
typedef enum EISV2PathType
{
    FullPath,           //< Input path full port
    DS4Path,            //< Input path DS4 port
    DS16Path,           //< Input path DS16 port
    FDPath,             //< Input path FD port
    DisplayFullPath,    //< Input path Display Full port
    DisplayDS4Path,     //< Input path Display DS4 port
    DisplayDS16Path     //< Input path Display DS16 port
} EISV2PATHTYPE;

static const INT    MaxMulticamSensors = 2;
static const UINT64 QtimerFrequency    = 19200000;  ///< QTimer Freq = 19.2 MHz

// NOWHINE FILE NC004c: Things outside the Camx namespace should be prefixed with Camx/CSL
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Chi node Class for Chi interface.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ChiEISV2Node
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialization required to create a node
    ///
    /// @param  pCreateInfo Pointer to a structure that defines create session information for the node.
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult Initialize(
        CHINODECREATEINFO* pCreateInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ProcessRequest
    ///
    /// @brief  Implementation of PFNNODEPROCREQUEST defined in chinode.h
    ///
    /// @param  pProcessRequestInfo Pointer to a structure that defines the information required for processing a request.
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult ProcessRequest(
        CHINODEPROCESSREQUESTINFO* pProcessRequestInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetBufferInfo
    ///
    /// @brief  Implementation of PFNNODESETBUFFERINFO defined in chinode.h
    ///
    /// @param  pSetBufferInfo  Pointer to a structure with information to set the output buffer resolution and type.
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult SetBufferInfo(
        CHINODESETBUFFERPROPERTIESINFO* pSetBufferInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PostPipelineCreate
    ///
    /// @brief  Implementation of PFNPOSTPIPELINECREATE defined in chinode.h
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult PostPipelineCreate();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetDataSource
    ///
    /// @brief  Inline function to Get data source
    ///
    /// @return Pointer to data source
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline CHIDATASOURCE* GetDataSource()
    {
        return &m_hChiDataSource;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ChiEISV2Node
    ///
    /// @brief  Constructor
    ///
    /// @param  overrideSettings  EISV2 override settings
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ChiEISV2Node(
        EISV2OverrideSettings overrideSettings);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~ChiEISV2Node
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~ChiEISV2Node();
private:
    ChiEISV2Node(const ChiEISV2Node&) = delete;               ///< Disallow the copy constructor
    ChiEISV2Node& operator=(const ChiEISV2Node&) = delete;    ///< Disallow assignment operator

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ExecuteAlgo
    ///
    /// @brief  Get input params from metadata in the pipeline
    ///
    /// @param  pProcessRequestInfo  Process request Info
    /// @param  pEIS2Output          EIS2 algo output
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult ExecuteAlgo(
        CHINODEPROCESSREQUESTINFO* pProcessRequestInfo,
        is_output_type*            pEIS2Output);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetGyroInterval
    ///
    /// @brief  Request Gyro data
    ///
    /// @param  frameNum       Process request frame number
    /// @param  pGyroInterval  Pointer to gyro interval structure to fill
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult GetGyroInterval(
        UINT64        frameNum,
        gyro_times_t* pGyroInterval);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetGyroDependency
    ///
    /// @brief  Set dependency to Request Gyro data from NCS
    ///
    /// @param  pProcessRequestInfo  Process request Info
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SetGyroDependency(
        CHINODEPROCESSREQUESTINFO* pProcessRequestInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetDependencies
    ///
    /// @brief  Set Dependencies
    ///
    /// @param  pProcessRequestInfo  Process request Info
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID SetDependencies(
        CHINODEPROCESSREQUESTINFO* pProcessRequestInfo);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateMetaData
    ///
    /// @brief  Update the metadata in the pipeline
    ///
    /// @param  requestId   The request id for current request
    /// @param  pAlgoResult Result from the algo process
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateMetaData(
        UINT64          requestId,
        is_output_type* pAlgoResult);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetMinTotalMargins
    ///
    /// @brief  Get the virtual margins that are defined in the tuning manager
    ///
    /// @param  virtualMarginX   The minimal total margin for width from Tuning manager
    /// @param  virtualMarginX   The minimal total margin for hight from Tuning manager
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID GetMinTotalMargins(
        FLOAT* virtualMarginX,
        FLOAT* virtualMarginY);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetAdditionalCropOffset
    ///
    /// @brief  Get the additional crop offset from the total margin
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID GetAdditionalCropOffset();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// LoadLib
    ///
    /// @brief  Load the algo lib and map function pointers
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult LoadLib();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UnLoadLib
    ///
    /// @brief  UnLoad the algo lib
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult UnLoadLib();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// InitializeLib
    ///
    /// @brief  Initialize the algo lib and get handle
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult InitializeLib();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillVendorTagIds
    ///
    /// @brief  Query all the vendor tag locations that EISv2 uses and save them
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult FillVendorTagIds();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrintAlgoInitData
    ///
    /// @brief  Print Algo Initialization data
    ///
    /// @param  pISInitDataCommon  Common Init data
    /// @param  pISInitDataSensors Per sensor Initi Data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PrintAlgoInitData(
        is_init_data_common * pISInitDataCommon,
        is_init_data_sensor * pISInitDataSensors);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrintEIS2InputParams
    ///
    /// @brief  Print Algo Input data
    ///
    /// @param  pEIS2Input Input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PrintEIS2InputParams(
        is_input_t* pEIS2Input);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrintEIS2OutputParams
    ///
    /// @brief  Print Algo Output data
    ///
    /// @param  pAlgoResult Output data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PrintEIS2OutputParams(
        is_output_type* pAlgoResult);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetChromatixTuningHandle
    ///
    /// @brief  Get Chromatix tuning handle
    ///
    /// @param  sensorIndex Sensor index
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult GetChromatixTuningHandle(
        UINT32 sensorIndex);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetChromatixData
    ///
    /// @brief  Get Chromatix data required for EIS Algo Input
    ///
    /// @param  pEISInitData EIS algo Init data
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult GetChromatixData(
        is_init_data_sensor* pEISInitData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetLDCGridFromICA20Chromatix
    ///
    /// @brief Get LDC grid from ICA Chromatix
    ///
    /// @param pEIS2Input EIS algo init sensor data
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult GetLDCGridFromICA20Chromatix(
        is_init_data_sensor* pEISInitData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetGyroFrequency
    ///
    /// @brief  Get gyro frequency from chromatix for the sensor index
    ///
    /// @param  activeSensorIndex active sensor index
    ///
    /// @return Gyro frequency in FLOAT
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    FLOAT GetGyroFrequency(
        UINT32 activeSensorIndex);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FillGyroData
    ///
    /// @brief  Get Gyro data required for EIS Algo Input
    ///
    /// @param  pProcessRequestInfo Process request Info
    /// @param  pEIS2Input          EIS algo input data
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult FillGyroData(
        CHINODEPROCESSREQUESTINFO* pProcessRequestInfo,
        is_input_t*                pEIS2Input);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QtimerTicksToQtimerNano
    ///
    /// @brief Convert timestamp from QTimer Ticks to QTimer Nano secs
    ///
    /// @param Timestamp in Qtimer Ticks
    ///
    /// @return Timestamp in QTimer Nano secs
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    UINT64 QtimerTicksToQtimerNano(UINT64 ticks)
    {
        return (UINT64(DOUBLE(ticks) * DOUBLE(NSEC_PER_SEC) / DOUBLE(QtimerFrequency)));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QtimerNanoToQtimerTicks
    ///
    /// @brief Convert timestamp from Qtimer Nano to QTimer Ticks
    ///
    /// @param Timestamp in QTimer Nano secs
    ///
    /// @return Timestamp in Qtimer Ticks
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline UINT64 QtimerNanoToQtimerTicks(UINT64 qtimerNano)
    {
        return static_cast<UINT64>(qtimerNano * DOUBLE(QtimerFrequency / DOUBLE(NSEC_PER_SEC)));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// IsEISv2Disabled
    ///
    /// @brief Check if EIS is disabled
    ///
    /// @param requestId Request ID
    ///
    /// @return TRUE if Disabled. False Otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL IsEISv2Disabled(
        UINT64 requestId);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteGyroDataToFile
    ///
    /// @brief  Write Gyro data to file
    ///
    /// @param  pEISInput EIS input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID WriteGyroDataToFile(
        is_input_t* pEISInput);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteEISVOutputMatricesToFile
    ///
    /// @brief  Write Algo Output to File
    ///
    /// @param  pAlgoResult Output data
    /// @param  pFileHandle File handle
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID WriteEISVOutputMatricesToFile(
        const is_output_type* pAlgoResult,
        FILE*                 pFileHandle);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteEISVOutputGridsToFile
    ///
    /// @brief  Write Algo Output to File
    ///
    /// @param  pAlgoResult Output data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID WriteEISVOutputGridsToFile(
        is_output_type* pAlgoResult);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateZoomWindow
    ///
    /// @brief  Update Crop window to handle stabilize margins
    ///
    /// @param  pCropRect Residual crop window
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateZoomWindow(
        CHIRectangle* pCropRect);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ConvertICA20GridToICA10Grid
    ///
    /// @brief  Convert ICA20 (size 35 x 27) Grid to ICA10 Grid (size 33 x 25) and also provide exterpolate corners for ICA10
    ///         todo: Move this function to a common utils for EIS nodes
    ///
    /// @param  pInICA20Grid Pointer to ICA20 grid
    /// @param  pInICA10Grid Pointer to ICA10 grid
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult ConvertICA20GridToICA10Grid(
        NcLibWarpGrid *pInICA20Grid,
        NcLibWarpGrid *pIutICA10Grid);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetCropRectfromCropInfo
    ///
    /// @brief  Get crop rectangle from crop info based on m_inputPortPathType
    ///
    /// @param  cropInfo Pointer to crop Info
    /// @param  cropRect Pointer to crop rectangle
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID GetCropRectfromCropInfo(
        IFECropInfo* pCropInfo,
        CHIRectangle* pCropRect);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// WriteInitDataToFile
    ///
    /// @brief  Write initialization input data to file
    ///
    /// @param  pISInitDataCommon       Common configuration data structure
    /// @param  pISInitDataSensors      Per sensor configuration data structure
    /// @param  numSensors              Number of sensors
    /// @param  fileHandle              File Handle
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID WriteInitDataToFile(
        const is_init_data_common*  pISInitDataCommon,
        const is_init_data_sensor*  pISInitDataSensors,
        uint32_t                    numSensors,
        FILE*                       fileHandle);

    ///< Typedefs for EIS2 interface functions
    typedef CDKResult (*EIS2_INITIALIZE)          (VOID**                       eis2_handle,
                                                   const  is_init_data_common*  init_common,
                                                   const  is_init_data_sensor*  init_sensors,
                                                   uint32_t                     num_sensors);
    typedef CDKResult (*EIS2_PROCESS)             (VOID*              eis2_handle,
                                                   const is_input_t*  is_input,
                                                   is_output_type*    is_output);
    typedef CDKResult (*EIS2_DEINITIALIZE)        (VOID** eis2_handle);
    typedef CDKResult (*EIS2_GET_GYRO_INTERVAL)   (VOID*                 eis2_handle,
                                                   const frame_times_t*  frame_time_input,
                                                   uint32_t              active_sensor_idx,
                                                   gyro_times_t*         gyro_req_interval);
    typedef CDKResult (*EIS2_GET_TOTAL_MARGIN)    (VOID*             eis2_handle,
                                                   uint32_t          active_sensor_idx,
                                                   ImageDimensions*  stabilizationMargins);
    typedef CDKResult (*EIS2_GET_TOTAL_MARGIN_EX) (const is_get_stabilization_margin* in,
                                                   ImageDimensions*                   stabilizationMargins);

    CHILIBRARYHANDLE          m_hEISv2Lib;                           ///< Handle for EISV2 library.
    EIS2_INITIALIZE           m_eis2Initialize;                      ///< Function pointer for eis2_initialize
    EIS2_PROCESS              m_eis2Process;                         ///< Function pointer for eis2_process
    EIS2_DEINITIALIZE         m_eis2Deinitialize;                    ///< Function pointer for eis2_deinitialize
    EIS2_GET_GYRO_INTERVAL    m_eis2GetGyroTimeInterval;             ///< Function pointer for eis2_get_gyro_time_interval
    EIS2_GET_TOTAL_MARGIN     m_eis2GetTotalMargin;                  ///< Function pointer for eis2_get_total_margin
    EIS2_GET_TOTAL_MARGIN_EX  m_eis2GetTotalMarginEx;                ///< Function pointer for eis2_get_total_margin_ex
    BOOL                      m_algoInitialized;                     ///< Flag indicating if Algo has been initialized
    CHIHANDLE                 m_hChiSession;                         ///< The Chi session handle
    UINT32                    m_nodeId;                              ///< The node's Id
    UINT32                    m_nodeCaps;                            ///< The selected node caps
    UINT32                    m_lookahead;                           ///< Number of future frames
    BOOL                      m_metaTagsInitialized;                 ///< Vendor Tag Ids queried and saved
    CHIDATASOURCE             m_hChiDataSource;                      ///< CHI Data Source Handle
    UINT32                    m_activeSensorIdx;                     ///< Active sensor index
    UINT32                    m_numOfMulticamSensors;                ///< Number of active linked sensor sessions
    EISV2PerSensorData        m_perSensorData[MaxMulticamSensors];   ///< Per camera data
    FILE*                     m_hGyroDumpFile;                       ///< Handle to gyro dump file
    FILE*                     m_hEisOutputMatricesDumpFile;          ///< Handle to EIS output dump file
    FILE*                     m_hEisOutputGridsDumpFile;             ///< Handle to output dump file
    FILE*                     m_hEisOutputMatricesMctfDumpFile;      ///< Handle to output dump file
    FILE*                     m_hEisOutputMatricesOrigDumpFile;      ///< Handle to output dump file
    StabilizationMargin       m_stabilizationMargins;                ///< Total stabilization margins in pixels
    ImageDimensions           m_additionalCropOffset;                ///< Additional crop offset for output image
    BOOL                      m_earlyInitialization;                 ///< Enable Early Initialization
    VOID*                     m_phEIS2Handle;                        ///< EIS2 Algo Handle
    ChiICAVersion             m_ICAVersion;                          ///< ICA version
    ChiTitanChipVersion       m_chiTitanVersion;                     ///< Titan version
    BOOL                      m_gyroDumpEnabled;                     ///< Flag to indicate if Gyro Dump enabled
    cam_is_operation_mode_t   m_algoOperationMode;                   ///< Calibration mode
    EISV2PathType             m_inputPortPathType;                   ///< EIS2 input port path type
    BOOL                      m_bIsLDCGridEnabled;                   ///< Flag for LDC grid enable
    NcLibWarpGrid             m_LDCIn2OutWarpGrid;                   ///< LDC in to out warp grid
    NcLibWarpGrid             m_LDCOut2InWarpGrid;                   ///< LDC out to in warp grid
    NcLibWarpGridCoord*       m_pLDCIn2OutGrid;                      ///< Pointer to LDC in to out grid
    NcLibWarpGridCoord*       m_pLDCOut2InGrid;                      ///< Pointer to LDC out to in grid
};

#endif // CAMXCHINODEEISV2_H
