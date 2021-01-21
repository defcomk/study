////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxchinodeeisv3.h
/// @brief Chi node for Image stabalization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXCHINODEEISV3_H
#define CAMXCHINODEEISV3_H


#include "camxchinodeutil.h"
#include "chieisdefs.h"
#include "chiipedefs.h"
#include "chiifedefs.h"
#include "chinode.h"
#include "eis3_interface.h"
#include "tuningsetmanager.h"
#if !_WINDOWS
#include <cutils/properties.h>
#endif

/// @brief structure to contain all the vendor tag Ids that EISv3 uses
struct EISV3VendorTags
{
    UINT32 ICAInPerspectiveTransformTagId;     ///< ICA In Perspective Transformation Vendor Tag Id
    UINT32 residualCropTagId;                  ///< IFE Residual Crop Info Vendor Tag Id
    UINT32 appliedCropTagId;                   ///< IFE Applied Crop Info Vendor Tag Id
    UINT32 mountAngleTagId;                    ///< Camera Sensor Mount Angle Vendor Tag Id
    UINT32 cameraPositionTagId;                ///< Camera Sensor Position Vendor Tag Id
    UINT32 QTimerTimestampTagId;               ///< QTimer SOF Timestamp Vendor Tag Id
    UINT32 sensorInfoTagId;                    ///< Sensor Info Vendor Tag Id
    UINT32 previewStreamDimensionsTagId;       ///< Output preview stream dimensions Vendor Tag Id
    UINT32 videoStreamDimensionsTagId;         ///< Output video stream dimensions Vendor Tag Id
    UINT32 EISV3EnabledTagId;                  ///< EISv3 enabled flag Vendor Tag Id
    UINT32 EISV3FrameDelayTagId;               ///< EISv3 frame delay Vendor Tag Id
    UINT32 EISV3RequestedMarginTagId;          ///< EISv3 Requested Margin Vendor Tag Id
    UINT32 EISV3StabilizationMarginsTagId;     ///< EISv3 actual stabilization margins Vendor Tag Id
    UINT32 EISV3AdditionalCropOffsetTagId;     ///< EISv3 additional crop offset Vendor Tag Id
    UINT32 EISV3PerspectiveGridTransformTagId; ///< EISv3 Perspective and Grid Transforms Vendor Tag Id
    UINT32 currentSensorModeTagId;             ///< Sensor's current mode index Vendor Tag Id
    UINT32 targetFPSTagId;                     ///< Target FPS for the usecase Vendor Tag Id
    UINT32 endOfStreamRequestTagId;            ///< End of stream request tag Id
    UINT32 EISV3StabilizedOutputDimsTagId;     ///< EISv3 Stabilized Output Dimensions Vendor Tag Id
    UINT32 ICAInGridTransformTagId;            ///< ICA In Grid Transformation Vendor Tag Id
};

/// @brief structure to contain all the per sensor data required for EISv3 algortihm
struct EISV3PerSensorData
{
    CHINODEIMAGEFORMAT              inputSize;          ///< Image size
    CHINODEIMAGEFORMAT              outputSize;         ///< Output Image size
    CHINODEIMAGEFORMAT              sensorDimension;    ///< Output Sensor Dimension
    TuningSetManager*               pTuningManager;     ///< Pointer to tuning manager
    eis_1_1_0::chromatix_eis11Type* pEISChromatix;      ///< pointer to EIS chromatix
    ica_2_0_0::chromatix_ica20Type* pICAChromatix;      ///< pointer to ICA20 chromatix
    UINT32                          mountAngle;         ///< Sensor mount angle
    INT32                           cameraPosition;     ///< Camera sensor position
    UINT32                          targetFPS;          ///< Target Frame Rate
};

/// @brief structure to contain eisv3 algo perspective matrix and grid
struct EISV3PerspectiveAndGridTransforms
{
    IPEICAPerspectiveTransform perspective;
    IPEICAGridTransform        grid;
};

/// @brief structure to contain eisv3 override settings
struct EISV3OverrideSettings
{
    MarginRequest               margins;            ///< Margins
    cam_is_operation_mode_t     algoOperationMode;  ///< Calibration mode
    BOOL                        isGyroDumpEnabled;  ///< Flag to indicate if Gyro Dump enabled
    BOOL                        isLDCGridEnabled;   ///< Flag for LDC grid enable
};

static const INT    MaxMulticamSensors = 2;
static const UINT64 QtimerFrequency    = 19200000;  ///< QTimer Freq = 19.2 MHz

// NOWHINE FILE NC004c: Things outside the Camx namespace should be prefixed with Camx/CSL
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Chi node class for Chi interface.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ChiEISV3Node
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

    /// ChiEISV3Node
    ///
    /// @brief  Constructor
    ///
    /// @param  overrideSettings  EISV3 override settings
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ChiEISV3Node(
        EISV3OverrideSettings overrideSettings);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~ChiEISV3Node
    ///
    /// @brief  Destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual ~ChiEISV3Node();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SetFrameDelay
    ///
    /// @brief  Set the frame/buffer delay for the EISv3 usecase
    ///
    /// @param  Frame delay
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline VOID SetFrameDelay(
        UINT32 frameDelay)
    {
        m_lookahead = frameDelay;
    }

private:
    ChiEISV3Node(const ChiEISV3Node&) = delete;               ///< Disallow the copy constructor
    ChiEISV3Node& operator=(const ChiEISV3Node&) = delete;    ///< Disallow assignment operator

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ExecuteAlgo
    ///
    /// @brief  Get input params from metadata in the pipeline
    ///
    /// @param  requestId   Request Id
    /// @param  pEIS3Output EIS3 algo output
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult ExecuteAlgo(
        UINT64          requestId,
        is_output_type* pEIS3Output);

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
    /// PublishICAMatrix
    ///
    /// @brief  Publish ICA metadata in the pipeline
    ///
    /// @param  requestId       The request id for current request
    /// @param  stopRecording   Flag indicating if recording has stopped
    /// @param  isEISv3Disabled Flag to indicate if EISv3 is disabled
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PublishICAMatrix(
        UINT64 requestId,
        BOOL   stopRecording,
        BOOL   isEISv3Disabled);

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
    /// @brief  Query all the vendor tag locations that EISv3 uses and save them
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult FillVendorTagIds();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrintAlgoInitData
    ///
    /// @brief  Print Algo Initialization data
    ///
    /// @param  pISInitDataCommon  Init data common for all sensors
    /// @param  pISInitDataSensors Per-sensor init data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PrintAlgoInitData(
        is_init_data_common* pISInitDataCommon,
        is_init_data_sensor* pISInitDataSensors);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrintEIS3InputParams
    ///
    /// @brief  Print Algo Input data
    ///
    /// @param  pEIS2Input Input data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PrintEIS3InputParams(
        is_input_t* pEIS3Input);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// PrintEIS3OutputParams
    ///
    /// @brief  Print Algo Output data
    ///
    /// @param  pAlgoResult Output data
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID PrintEIS3OutputParams(
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
    /// @brief Get Chromatix data required for EIS Algo Input
    ///
    /// @param pEIS2Input EIS algo input data
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
    /// @param  requestId   Request Id
    /// @param  pEIS2Input  EIS algo input data
    ///
    /// @return CDKResultSuccess if success or appropriate error code.
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CDKResult FillGyroData(
        UINT64                   requestId,
        is_input_t*              pEIS3Input);

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
        return UINT64(DOUBLE(ticks) * DOUBLE(NSEC_PER_SEC) / DOUBLE(QtimerFrequency));
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
    /// IsEISv3Disabled
    ///
    /// @brief Check if EIS is disabled
    ///
    /// @param requestId Request ID
    ///
    /// @return TRUE if Disabled. False Otherwise
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    BOOL IsEISv3Disabled(
        UINT64 requestId,
        INT64  offset);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UpdateZoomWindow
    ///
    /// @brief  Update Crop window to handle stabilize margins
    ///
    /// @param  pCropInfo Residual crop window
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    VOID UpdateZoomWindow(
        IFECropInfo* pCropInfo);

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

    ///< Typedefs for EIS3 interface functions
    typedef CDKResult (*EIS3_INITIALIZE)          (VOID**   eis3_handle,
                                                   const    is_init_data_common* init_common,
                                                   const    is_init_data_sensor* init_sensors,
                                                   uint32_t num_sensors);
    typedef CDKResult (*EIS3_PROCESS)             (VOID*           eis3_handle,
                                                   const           is_input_t* is_input,
                                                   is_output_type* is_output);
    typedef CDKResult (*EIS3_DEINITIALIZE)        (VOID** eis3_handle);
    typedef CDKResult (*EIS3_GET_GYRO_INTERVAL)   (VOID*                eis3_handle,
                                                   const frame_times_t* frame_time_input,
                                                   uint32_t             active_sensor_idx,
                                                   gyro_times_t*        gyro_req_interval);
    typedef CDKResult (*EIS3_GET_TOTAL_MARGIN)    (VOID*            eis,
                                                   uint32_t         active_sensor_idx,
                                                   ImageDimensions* stabilizationMargins);
    typedef CDKResult (*EIS3_GET_TOTAL_MARGIN_EX) (const is_get_stabilization_margin* in,
                                                   ImageDimensions*                   stabilizationMargins);

    CHILIBRARYHANDLE                   m_hEISv3Lib;                         ///< Handle for EISV3 library.
    EIS3_INITIALIZE                    m_eis3Initialize;                    ///< Function pointer for eis3_initialize
    EIS3_PROCESS                       m_eis3Process;                       ///< Function pointer for eis3_process
    EIS3_DEINITIALIZE                  m_eis3Deinitialize;                  ///< Function pointer for eis3_deinitialize
    EIS3_GET_GYRO_INTERVAL             m_eis3GetGyroTimeInterval;           ///< Function pointer for
                                                                            ///< eis3_get_gyro_time_interval
    EIS3_GET_TOTAL_MARGIN              m_eis3GetTotalMargin;                ///< Function pointer for eis3_get_total_margin
    EIS3_GET_TOTAL_MARGIN_EX           m_eis3GetTotalMarginEx;              ///< Function pointer for eis3_get_total_margin_ex
    BOOL                               m_algoInitialized;                   ///< Flag indicating if Algo has been initialized
    CHIHANDLE                          m_hChiSession;                       ///< The Chi session handle
    UINT32                             m_nodeId;                            ///< The node's Id
    UINT32                             m_nodeCaps;                          ///< The selected node caps
    UINT32                             m_lookahead;                         ///< Number of future frames
    VOID*                              m_phEIS3Handle;                      ///< EIS3 Algo Handle
    BOOL                               m_metaTagsInitialized;               ///< Vendor Tag Ids queried and saved
    CHIDATASOURCE                      m_hChiDataSource;                    ///< CHI Data Source Handle
    UINT64                             m_readoutDuration;                   ///< sensor readout duration
    UINT32                             m_activeSensorIdx;                   ///< Active sensor index
    UINT32                             m_numOfMulticamSensors;              ///< Number of active linked sensor sessions
    EISV3PerSensorData                 m_perSensorData[MaxMulticamSensors]; ///< Per camera data
    FILE*                              m_hGyroDumpFile;                     ///< Handle to gyro dump file
    FILE*                              m_hEisOutputMatricesDumpFile;        ///< Handle to output dump file
    FILE*                              m_hEisOutputGridsDumpFile;           ///< Handle to output dump file
    FILE*                              m_hEisOutputMatricesMctfDumpFile;    ///< Handle to output dump file
    FILE*                              m_hEisOutputMatricesOrigDumpFile;    ///< Handle to output dump file
    StabilizationMargin                m_stabilizationMargins;              ///< Total stabilization margins in pixels
    ImageDimensions                    m_additionalCropOffset;              ///< Additional crop offset for output image
    BOOL                               m_earlyInitialization;               ///< Enable Early Initialization
    EISV3PerspectiveAndGridTransforms* m_pEndOfStreamOutputArray;           ///< last set of eis outputs should be of size of
                                                                            ///< request queue depth
    BOOL                               m_endOfStreamOutputArrayFilled;      ///< Flag indicates if end of stream out array full
    CamX::Mutex*                       m_pEndOfStreamLock;                  ///< End of stream lock
    UINT64                             m_lastEIS3publishedRequest;          ///< last published request before end of stream
    ChiICAVersion                      m_ICAVersion;                        ///< ICA version
    ChiTitanChipVersion                m_chiTitanVersion;                   ///< Titan version
    BOOL                               m_gyroDumpEnabled;                   ///< Flag to indicate if Gyro Dump enabled
    cam_is_operation_mode_t            m_algoOperationMode;                 ///< Calibration mode
    BOOL                               m_bIsLDCGridEnabled;                 ///< Flag for LDC grid enable
    NcLibWarpGrid                      m_LDCIn2OutWarpGrid;                 ///< LDC in to out warp grid
    NcLibWarpGrid                      m_LDCOut2InWarpGrid;                 ///< LDC out to in warp grid
    NcLibWarpGridCoord*                m_pLDCIn2OutGrid;                    ///< Pointer to LDC in to out grid
    NcLibWarpGridCoord*                m_pLDCOut2InGrid;                    ///< Pointer to LDC out to in grid
};

#endif // CAMXCHINODEEISV3_H
