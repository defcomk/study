//******************************************************************************************************************************
// Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//******************************************************************************************************************************
//==============================================================================================================================
// IMPLEMENTATION of ChiModule
//==============================================================================================================================

#include <dlfcn.h>

#include "ais_log.h"
#include "chimodule.h"

#define EXPECT_CHI_API_MAJOR_VERSION   1
#define EXPECT_CHI_API_MINOR_VERSION   0

#define AIS_LOG_CHI_MOD(level, fmt...) AIS_LOG(AIS_MOD_CHI_MOD, level, fmt)
#define AIS_LOG_CHI_MOD_ERR(fmt...)    AIS_LOG(AIS_MOD_CHI_MOD, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_CHI_MOD_HIGH(fmt...)   AIS_LOG(AIS_MOD_CHI_MOD, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CHI_MOD_WARN(fmt...)   AIS_LOG(AIS_MOD_CHI_MOD, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_CHI_MOD_HIGH(fmt...)   AIS_LOG(AIS_MOD_CHI_MOD, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CHI_MOD_MED(fmt...)    AIS_LOG(AIS_MOD_CHI_MOD, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_CHI_MOD_LOW(fmt...)    AIS_LOG(AIS_MOD_CHI_MOD, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_CHI_MOD_DBG(fmt...)    AIS_LOG(AIS_MOD_CHI_MOD, AIS_LOG_LVL_DBG, fmt)
#define AIS_LOG_CHI_MOD_DBG1(fmt...)   AIS_LOG(AIS_MOD_CHI_MOD, AIS_LOG_LVL_DBG1, fmt)

///@brief ChiModule singleton
ChiModule* ChiModule::m_pModuleInstance = nullptr;

/***************************************************************************************************************************
 * ChiModule::GetInstance
 *
 * @brief
 *     Gets the singleton instance for ChiModule
 * @param
 *     None
 * @return
 *     ChiModule singleton
 ***************************************************************************************************************************/
ChiModule* ChiModule::GetInstance()
{
    if (m_pModuleInstance == nullptr)
    {
        m_pModuleInstance = SAFE_NEW() ChiModule();
        if (m_pModuleInstance->Initialize() != CDKResultSuccess)
        {
            AIS_LOG_CHI_MOD_ERR("Failed to initialize ChiModule singleton!");
            return nullptr;
        }
    }

    return m_pModuleInstance;
}

/***************************************************************************************************************************
 * ChiModule::DestroyInstance
 *
 * @brief
 *     Destroy the singleton instance of the ChiModule class
 * @param
 *     None
 * @return
 *     void
 ***************************************************************************************************************************/
void ChiModule::DestroyInstance()
{
    if (m_pModuleInstance != nullptr)
    {
        delete m_pModuleInstance;
        m_pModuleInstance = nullptr;
    }
}

/***************************************************************************************************************************
 * ChiModule::ChiModule
 *
 * @brief
 *     Constructor for ChiModule
 ***************************************************************************************************************************/
ChiModule::ChiModule() :
    m_pCameraInfo(nullptr),
    m_pLegacyCameraInfo(nullptr),
    m_pSensorInfo(nullptr),
    m_numOfCameras(0),
    m_hContext(nullptr),
    m_vendortagOps{},
    m_fenceOps{},
    hLibrary(nullptr),
    m_chiOps{}
#ifdef USE_VENDOTAGS
    ,m_vendoTagOps{}
#endif
#ifdef USE_HMI
    ,m_pCameraModule(nullptr)
#endif
{
}

/***************************************************************************************************************************
 * ChiModule::~ChiModule
 *
 * @brief
 *     Destructor for ChiModule
 ***************************************************************************************************************************/
ChiModule::~ChiModule()
{
    if (m_hContext)
    {
        if (CloseContext() != CDKResultSuccess)
        {
            AIS_LOG_CHI_MOD_ERR("Failed to close camera context!");
        }
    }

#ifdef USE_HMI
    // free the library
    if (nullptr != hLibrary)
    {
        if (!FreeLibrary(static_cast<HMODULE>(hLibrary)))
        {
            AIS_LOG_CHI_MOD_ERR("Failed to free library handle");
        }
    }
#endif

    for (int currCamera = 0; currCamera < m_numOfCameras; currCamera++)
    {
        if (m_pSensorInfo[currCamera])
        {
            delete[] m_pSensorInfo[currCamera];
            m_pSensorInfo[currCamera] = nullptr;
        }
    }

    if (m_pLegacyCameraInfo)
    {
        delete[] m_pLegacyCameraInfo;
        m_pLegacyCameraInfo = nullptr;
    }

    if (m_pSensorInfo)
    {
        delete[] m_pSensorInfo;
        m_pSensorInfo = nullptr;
    }

    if (m_pCameraInfo)
    {
        delete[] m_pCameraInfo;
        m_pCameraInfo = nullptr;
    }
}

/***************************************************************************************************************************
 * ChiModule::Initialize
 *
 * @brief
 *     Initialize member variables using driver calls
 * @param
 *     None
 * @return
 *     CDKResult result code
 ***************************************************************************************************************************/
CDKResult ChiModule::Initialize()
{
    CDKResult result = CDKResultSuccess;

    result = LoadLibraries();
    if (result != CDKResultSuccess)
    {
        return result;
    }
    result = OpenContext();
    if (result != CDKResultSuccess)
    {
        return result;
    }

    m_numOfCameras = m_chiOps.pGetNumCameras(m_hContext);

    m_pCameraInfo = SAFE_NEW() CHICAMERAINFO[m_numOfCameras];
    m_pLegacyCameraInfo = SAFE_NEW() CamX::CameraInfo[m_numOfCameras];
    m_pSensorInfo = SAFE_NEW() CHISENSORMODEINFO*[m_numOfCameras];

    for (int currCamera = 0; currCamera < m_numOfCameras; currCamera++)
    {
        m_pCameraInfo[currCamera].pLegacy = &m_pLegacyCameraInfo[currCamera];

        m_chiOps.pGetCameraInfo(m_hContext, currCamera, &m_pCameraInfo[currCamera]);

        uint32_t numSensorModes = m_pCameraInfo[currCamera].numSensorModes;

        m_pSensorInfo[currCamera] = SAFE_NEW() CHISENSORMODEINFO[numSensorModes];

        m_chiOps.pEnumerateSensorModes(m_hContext, currCamera, numSensorModes, m_pSensorInfo[currCamera]);

        for (uint32_t sensorMode = 0; sensorMode < numSensorModes; sensorMode++)
        {
            AIS_LOG_CHI_MOD_DBG("Resolution for camera: %d sensormode: %d, width: %d, height:%d ",
                currCamera,
                sensorMode,
                m_pSensorInfo[currCamera][sensorMode].frameDimension.width,
                m_pSensorInfo[currCamera][sensorMode].frameDimension.height);
        }
    }

    return result;
}

/***************************************************************************************************************************
 * ChiModule::LoadLibraries
 *
 * @brief
 *     Load chi and vendor tag operation libraries
 * @param
 *     None
 * @return
 *     CDKResult result code
 ***************************************************************************************************************************/
CDKResult ChiModule::LoadLibraries()
{
    PCHIENTRY pChiHalOpen;

#ifdef OS_ANDROID
#if defined(_LP64)
    hLibrary = dlopen("/vendor/lib64/hw/camera.qcom.so", RTLD_NOW);
#else
    hLibrary = dlopen("/vendor/lib/hw/camera.qcom.so", RTLD_NOW);
#endif
    if (hLibrary == NULL)
    {
        AIS_LOG_CHI_MOD_ERR("Failed to load android library");
        return CDKResultEUnableToLoad;
    }
    pChiHalOpen = reinterpret_cast<PCHIENTRY>(dlsym(hLibrary, "ChiEntry"));
    if (pChiHalOpen == NULL)
    {
        AIS_LOG_CHI_MOD_ERR("ChiEntry missing in library");
        return CDKResultEUnableToLoad;
    }

#ifdef USE_HMI
    m_pCameraModule = reinterpret_cast<camera_module_t*>(dlsym(hLibrary, "HMI"));
    if (m_pCameraModule == NULL)
    {
        AIS_LOG_CHI_MOD_ERR("CameraModule missing in library");
        return CDKResultEUnableToLoad;
    }
#endif

    if (nullptr != pChiHalOpen)
    {
        (*pChiHalOpen)(&m_chiOps);
    }

#else
    AIS_LOG_CHI_MOD_ERR("ChiEntry Init");
    ChiEntry(&m_chiOps);

#endif // OS_ANDROID

#ifdef USE_VENDO_TAG
    m_pCameraModule->get_vendor_tag_ops(&m_vendoTagOps);
    int ret = set_camera_metadata_vendor_ops(const_cast<vendor_tag_ops_t*>(&m_vendoTagOps));
    if (ret != 0)
    {
        AIS_LOG_CHI_MOD_ERR("Failed to set vendor tag ops");
        return CDKResultEFailed;
    }
#endif

    return CDKResultSuccess;
}

/***************************************************************************************************************************
 * ChiModule::OpenContext
 *
 * @brief
 *     Opens a camera context
 * @param
 *     None
 * @return
 *     CDKResult result code
 ***************************************************************************************************************************/
CDKResult ChiModule::OpenContext()
{
    AIS_LOG_CHI_MOD_ERR("Checking inteface compatibility, expected version %d %d", EXPECT_CHI_API_MAJOR_VERSION,
        EXPECT_CHI_API_MINOR_VERSION);

    UINT32 majorVersion = m_chiOps.majorVersion;
    UINT32 minorVersion = m_chiOps.minorVersion;

    if (majorVersion == EXPECT_CHI_API_MAJOR_VERSION &&
        minorVersion == EXPECT_CHI_API_MINOR_VERSION)
    {
        AIS_LOG_CHI_MOD_ERR("Chi API version matches expected version");
    }

    else
    {
        AIS_LOG_CHI_MOD_ERR("Chi API version does not match expected version");
    }

    AIS_LOG_CHI_MOD_DBG("Opening chi context");
    m_hContext = m_chiOps.pOpenContext();
    m_chiOps.pTagOps(&m_vendortagOps);
    m_chiOps.pGetFenceOps(&m_fenceOps);
    if (m_hContext == nullptr)
    {
        AIS_LOG_CHI_MOD_ERR("Open context failed!");
        return CDKResultEFailed;
    }
    return CDKResultSuccess;
}

/***************************************************************************************************************************
 * ChiModule::CloseContext
 *
 * @brief
 *     Close camera context
 * @param
 *     None
 * @return
 *     CDKResult result code
 ***************************************************************************************************************************/
CDKResult ChiModule::CloseContext()
{
    CDKResult result = CDKResultSuccess;

    AIS_LOG_CHI_MOD_DBG("Closing Context: %p", m_hContext);
    if (m_hContext != nullptr)
    {
        m_chiOps.pCloseContext(m_hContext);
        m_hContext = nullptr;
    }
    else
    {
        AIS_LOG_CHI_MOD_ERR("Requested context %p is not open", m_hContext);
        result = CDKResultEInvalidState;
    }
    return result;
}

/***************************************************************************************************************************
 * ChiModule::GetNumCams
 *
 * @brief
 *     Gets number of cameras reported by the driver
 * @param
 *     None
 * @return
 *     int number of cameras reported by the module
 ***************************************************************************************************************************/
int ChiModule::GetNumberCameras() const
{
    return m_numOfCameras;
}

/***************************************************************************************************************************
 * ChiModule::GetCameraInfo
 *
 * @brief
 *     Gets camera info for given camera Id
 * @param
 *     [in]  uint32_t   cameraId    cameraid associated
 * @return
 *     CHICAMERAINFO* camerainfo for given camera Id
 ***************************************************************************************************************************/
const CHICAMERAINFO* ChiModule::GetCameraInfo(uint32_t cameraId) const
{
    const CHICAMERAINFO* pCameraInfo = nullptr;
    if ((m_pCameraInfo != nullptr) && (&m_pCameraInfo[cameraId] != nullptr))
    {
        pCameraInfo = &m_pCameraInfo[cameraId];
    }

    return pCameraInfo;
}

/***************************************************************************************************************************
 * ChiModule::GetCameraSensorModeInfo
 *
 * @brief
 *     Gets sensormode for given index
 * @param
 *     [in] uint32_t cameraId id of the camera
 *     [in] uint32_t modeId   index of the mode to use
 * @return
 *     CHISENSORMODEINFO for given cameraId and index
 ****************************************************************************************************************************/
CHISENSORMODEINFO* ChiModule::GetCameraSensorModeInfo(uint32_t cameraId, uint32_t modeId) const
{
    CHISENSORMODEINFO* pSensorModeInfo = nullptr;
    if ((m_pSensorInfo != nullptr) && (&m_pSensorInfo[cameraId] != nullptr))
    {
        pSensorModeInfo = &m_pSensorInfo[cameraId][modeId];
    }
    return pSensorModeInfo;
}

/***************************************************************************************************************************
* ChiModule::GetClosestSensorMode
*
* @brief
*     Chooses a sensormode closest to given resolution
* @param
*     [in] uint32_t cameraId id of the camera
*     [in] Size res          resolution used
* @return
*     CHISENSORMODEINFO for given resolution which is closest
****************************************************************************************************************************/
CHISENSORMODEINFO* ChiModule::GetClosestSensorMode(uint32_t cameraId, Size resolution) const
{
    CHISENSORMODEINFO* pSensorModeInfo = m_pSensorInfo[cameraId];
    long long givenRes = resolution.width*resolution.height;
    long long min = LONG_MAX;
    uint32_t index = 0;
    if ((m_pSensorInfo != nullptr) && (&m_pSensorInfo[cameraId] != nullptr))
    {
        for(uint32_t mode = 0; mode < m_pCameraInfo[cameraId].numSensorModes; mode++)
        {
            long long currRes = pSensorModeInfo[mode].frameDimension.width*pSensorModeInfo[mode].frameDimension.height;
            if((currRes - givenRes) <= min && currRes >= givenRes)
            {
                min = currRes - givenRes;
                index = mode;
            }
        }
    }
    return &pSensorModeInfo[index];
}

/***************************************************************************************************************************
 *   ChiModule::SubmitPipelineRequest
 *
 *   @brief
 *       Submit the pipeline request (capture)
 *   @param
 *     [in]  CHIPIPELINEREQUEST            pRequest       pointer to the capture request
 *   @return
 *       CDKResult result code
 ****************************************************************************************************************************/
CDKResult ChiModule::SubmitPipelineRequest(CHIPIPELINEREQUEST * pRequest) const
{
    for (uint32_t requestCount = 0; requestCount < pRequest->numRequests; requestCount++)
    {
        uint64_t frameNum = pRequest->pCaptureRequests[requestCount].frameNumber;
        AIS_LOG_CHI_MOD_DBG("Sending pipeline request for frame number: %ll", frameNum);
    }

    return m_chiOps.pSubmitPipelineRequest(m_hContext, pRequest);
}

/***************************************************************************************************************************
 * ChiModule::GetContext
 *
 * @brief
 *     Gets the camera context
 * @param
 *     None
 * @return
 *     CHIHANDLE  camera context
 ***************************************************************************************************************************/
CHIHANDLE ChiModule::GetContext() const
{
    return m_hContext;
}

/***************************************************************************************************************************
 * ChiModule::GetChiOps
 *
 * @brief
 *     Gets pointer to ChiOps
 * @param
 *     None
 * @return
 *     CHICONTEXTOPS*  pointer to chi APIs
 ***************************************************************************************************************************/
const CHICONTEXTOPS* ChiModule::GetChiOps() const
{
    return &m_chiOps;
}

/***************************************************************************************************************************
* ChiModule::GetFenceOps
*
* @brief
*     Gets pointer to fenceOps
* @param
*     None
* @return
*     CHIFENCEOPS*  pointer to chi fence APIs
***************************************************************************************************************************/
const CHIFENCEOPS* ChiModule::GetFenceOps() const
{
    return &m_fenceOps;
}

/***************************************************************************************************************************
 * ChiModule::GetLibrary
 *
 * @brief
 *     Gets symbols loaded through the dll/.so
 * @param
 *     None
 * @return
 *     void* Function pointer to library loaded
 ***************************************************************************************************************************/
void * ChiModule::GetLibrary() const
{
    return hLibrary;
}

