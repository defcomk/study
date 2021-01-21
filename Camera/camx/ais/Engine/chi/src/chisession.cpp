//******************************************************************************************************************************
// Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
//******************************************************************************************************************************
//==============================================================================================================================
// IMPLEMENTATION of ChiSession
//==============================================================================================================================

#include "chisession.h"

#define AIS_LOG_CHI_SESSION(level, fmt...) AIS_LOG(AIS_MOD_CHI_SESSION, level, fmt)
#define AIS_LOG_CHI_SESSION_ERR(fmt...)    AIS_LOG(AIS_MOD_CHI_SESSION, AIS_LOG_LVL_ERR, fmt)
#define AIS_LOG_CHI_SESSION_HIGH(fmt...)   AIS_LOG(AIS_MOD_CHI_SESSION, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CHI_SESSION_WARN(fmt...)   AIS_LOG(AIS_MOD_CHI_SESSION, AIS_LOG_LVL_WARN, fmt)
#define AIS_LOG_CHI_SESSION_HIGH(fmt...)   AIS_LOG(AIS_MOD_CHI_SESSION, AIS_LOG_LVL_HIGH, fmt)
#define AIS_LOG_CHI_SESSION_MED(fmt...)    AIS_LOG(AIS_MOD_CHI_SESSION, AIS_LOG_LVL_MED, fmt)
#define AIS_LOG_CHI_SESSION_LOW(fmt...)    AIS_LOG(AIS_MOD_CHI_SESSION, AIS_LOG_LVL_LOW, fmt)
#define AIS_LOG_CHI_SESSION_DBG(fmt...)    AIS_LOG(AIS_MOD_CHI_SESSION, AIS_LOG_LVL_DBG, fmt)
#define AIS_LOG_CHI_SESSION_DBG1(fmt...)   AIS_LOG(AIS_MOD_CHI_SESSION, AIS_LOG_LVL_DBG1, fmt)


/***************************************************************************************************************************
 *   ChiSession::ChiSession
 *
 *   @brief
 *       Constructor for ChiSession
 ***************************************************************************************************************************/
ChiSession::ChiSession()
{
    m_hSession = nullptr;
}

/***************************************************************************************************************************
 *   ChiSession::~ChiSession
 *
 *   @brief
 *       Destructor for ChiSession
 ***************************************************************************************************************************/
ChiSession::~ChiSession()
{
    m_hSession = nullptr;
}

/***************************************************************************************************************************
 *   ChiSession::Create
 *
 *   @brief
 *       static function to create an instance of the chisession
 *   @param
 *        [in]  ChiPipeline**   ppPipelines    pointer to all created pipelines
 *        [in]  int             numPipelines   number of pipelines present for the session
 *        [in]  ChiCallBacks*   pCallbacks     callback function for driver to return the result
 *        [in]  void*           pPrivateData   pointer to privatedata specific to the testcase
 *   @return
 *       ChiSession*  instance to the newly created session
 ***************************************************************************************************************************/
ChiSession* ChiSession::Create(ChiPipeline** ppPipelines, UINT32 numPipelines, ChiCallBacks* pCallbacks, void* pPrivateData)
{
    CDKResult result = CDKResultSuccess;
    ChiSession* pNewSession = SAFE_NEW() ChiSession();

    if(pNewSession != nullptr)
    {
        result = pNewSession->Initialize(ppPipelines, numPipelines, pCallbacks, pPrivateData);

        if(result != CDKResultSuccess)
        {
            delete pNewSession;
            pNewSession = nullptr;
        }
    }

    return pNewSession;
}

/***************************************************************************************************************************
 *   ChiSession::Initialize
 *
 *   @brief
 *       member function to initialize newly created instance of the chisession
 *   @param
 *        [in]  ChiPipeline**   ppPipelines    pointer to all created pipelines
 *        [in]  int             numPipelines   number of pipelines present for the session
 *        [in]  ChiCallBacks*   pCallbacks     callback function for driver to return the result
 *        [in]  void*           pPrivateData   pointer to privatedata specific to the testcase
 *   @return
 *       CDKResult  result code to determine outcome of the function
 ***************************************************************************************************************************/
CDKResult ChiSession::Initialize(ChiPipeline** ppPipelines, UINT32 numPipelines, ChiCallBacks* pCallbacks, void* pPrivateData)
{
    CDKResult         result = CDKResultSuccess;

    memset(&m_sessionCreateData, 0, sizeof(SessionCreateData));   // Zero out session create data
    m_sessionCreateData.numPipelines         = numPipelines;
    m_sessionCreateData.pCallbacks           = pCallbacks;
    m_sessionCreateData.pPrivateCallbackData = pPrivateData;

    if(numPipelines > MaxPipelinesPerSession)
    {
        AIS_LOG_CHI_SESSION_ERR("Session cannot have more than %d pipelines", MaxPipelinesPerSession);
        return CDKResultEFailed;
    }

    for (UINT32 i = 0; i < numPipelines; i++)
    {
        m_sessionCreateData.pPipelineInfos[i] = ppPipelines[i]->GetPipelineInfo();
    }

    m_hSession = CreateSession(&m_sessionCreateData);

    AIS_LOG_CHI_SESSION_DBG("Created session handle: %p", m_hSession);

    if (NULL == m_hSession)
    {
        result = CDKResultEFailed;
    }

    return result;
}

/***************************************************************************************************************************
 *   ChiSession::CreateSession
 *
 *   @brief
 *       Create a chi session based on the given parameters
 *   @param
 *     [in]  SessionCreateData*   pSessionCreate     pointer to SessionCreateData structure
 *   @return
 *       CHIHANDLE    handle to the created session
 ***************************************************************************************************************************/
CHIHANDLE ChiSession::CreateSession(SessionCreateData* pSessionCreate)
{
    AIS_LOG_CHI_SESSION_DBG("Creating session on context: %p", ChiModule::GetInstance()->GetContext());
    CHISESSIONFLAGS flags;
    flags.u.isNativeChi = 1;
    AIS_LOG_CHI_SESSION_DBG("ChiContext = %p, PipelineInfo = %p, Callback = %p", ChiModule::GetInstance()->GetContext(),
        pSessionCreate->pPipelineInfos, pSessionCreate->pCallbacks);

    return ChiModule::GetInstance()->GetChiOps()->pCreateSession(
        ChiModule::GetInstance()->GetContext(),
        pSessionCreate->numPipelines,
        pSessionCreate->pPipelineInfos,
        pSessionCreate->pCallbacks,
        pSessionCreate->pPrivateCallbackData,
        flags);
}

/***************************************************************************************************************************
 *   ChiSession::GetSessionHandle
 *
 *   @brief
 *       return the created session handle
 *   @return
 *       CHIHANDLE created session handle
 ***************************************************************************************************************************/
CHIHANDLE ChiSession::GetSessionHandle() const
{
    return m_hSession;
}

/***************************************************************************************************************************
 *   ChiSession::FlushSession
 *
 *   @brief
 *       Flush the session
 *   @return
 *       CDKResult
 ***************************************************************************************************************************/
CDKResult ChiSession::FlushSession() const
{
    AIS_LOG_CHI_SESSION_DBG("Flushing session: %p on context: %p", m_hSession, ChiModule::GetInstance()->GetContext());
    return ChiModule::GetInstance()->GetChiOps()->pFlushSession(ChiModule::GetInstance()->GetContext(), m_hSession);
}

/***************************************************************************************************************************
 *   ChiSession::DestroySession
 *
 *   @brief
 *       Destroy session
 *   @return
 *      none
 ***************************************************************************************************************************/
void ChiSession::DestroySession() const
{
    AIS_LOG_CHI_SESSION_DBG("Destroying session: %p on context: %p", m_hSession, ChiModule::GetInstance()->GetContext());
    ChiModule::GetInstance()->GetChiOps()->pDestroySession(ChiModule::GetInstance()->GetContext(), m_hSession, false);
    delete this;
}
