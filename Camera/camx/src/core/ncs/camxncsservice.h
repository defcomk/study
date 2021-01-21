////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxncsservice.h
/// @brief CamX Non-Camera Service Class Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CAMXNCSSERVICE_H
#define CAMXNCSSERVICE_H

#include "camxdefs.h"
#include "camxncsintf.h"
#include "camxncssensor.h"
#include "camxchicontext.h"

CAMX_NAMESPACE_BEGIN

const UINT NCSJobTypeFlush        = 0;        ///< NCS job type for flush

struct NCSJob
{
    UINT       jobType;  ///< Job Type
    VOID*      pPayload; ///< Pointer to the payload data.
};

class NCSService;
struct NCSThreadContext
{
    JobHandle                   hJobHandle;         ///< Job family handle
    ThreadManager*              pThreadManager;     ///< Thread manager pointer
    LightweightDoublyLinkedList NCSJobQueue;        ///< NCS Job queue
    Mutex*                      pNCSQMutex;         ///< NCS Queue mutex variable
    Condition*                  pNCSQCondVar;       ///< NCS Queue condition variable
    Semaphore*                  pNCSFlushVar;       ///< NCS Flush semaphore variable
    BOOL                        isRunning;          ///< Flag to indicate if NCS thread is running
    NCSService*                 pServiceObject;     ///< NCS Service object
};

struct NCSThreadData
{
    NCSThreadContext* pThreadContext; ///< Pointer to the thread context.
    NCSJob*           pJob;           ///< Pointer to the payload data.
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class that implements the NCS Service
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOWHINE CP044: Need to set visibility, false positive
class NCSService
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// NCSService
    ///
    /// @brief  the default constructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_VISIBILITY_PUBLIC NCSService() = default;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///  ~NCSService
    ///
    /// @brief  the destructor
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_VISIBILITY_PUBLIC ~NCSService();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Initialize
    ///
    /// @brief  Initialize the NCS service
    ///
    /// @param  pChiContext pointer to the chi context
    ///
    /// @return CamxResultSuccess in case of successfull execution
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_VISIBILITY_PUBLIC CamxResult Initialize(
        ChiContext* pChiContext);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QueryCapabilites
    ///
    /// @brief  Query the capabilities of a sensor type
    ///
    /// @param  pCaps      Capabilities pointer to be filled
    /// @param  sensorType Sensor type
    ///
    /// @return CamxResultSuccess in case of successfull execution
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_VISIBILITY_PUBLIC CamxResult QueryCapabilites(
        NCSSensorCaps* pCaps,
        NCSSensorType sensorType);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// RegisterService
    ///
    /// @brief  API for a client to register a service
    ///
    /// @param  sensorType    sensor type
    /// @param  pConfigParams Config params
    ///
    /// @return NCS sensor object handle
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_VISIBILITY_PUBLIC NCSSensor* RegisterService(
        NCSSensorType sensorType,
        VOID* pConfigParams);


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// UnregisterService
    ///
    /// @brief  Unregister the NCS sensor object
    ///
    /// @param  phNCSSensorHandle NCS Sensor handle
    ///
    /// @return CamxResultSuccess in case of successfull execution
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CAMX_VISIBILITY_PUBLIC CamxResult UnregisterService(
        NCSSensor* phNCSSensorHandle);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// EnqueueJob
    ///
    /// @brief  Enqueue a job to the job queue
    ///
    /// @param  pJob Pointer to the job payload
    ///
    /// @return CamxResultSuccess if enqueue successfull
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult EnqueueJob(
        NCSJob* pJob);

private:

    INCSIntfBase*  m_pNCSIntfObject[MaxNCSIntfType];            ///< Array of NCS interface objects
    Mutex*         m_pNCSServiceMutex;                          ///< Mutex for the NCS service
    NCSSensor*     m_pSensorClients[MaxSupportedSensorClients]; ///< List of sensor clients
    UINT           m_numActiveClients;                          ///< Num active clients has been registered
    NCSThreadData*     m_pThreadData;                           ///< NCS thread data
    NCSThreadContext   m_hNCSPollThHandle;                      ///< NCS thread related data handle
    NCSService(const NCSService&)                 = delete;     ///< Disallow the copy constructor.
    NCSService& operator=(const NCSService&)      = delete;     ///< Disallow assignment operator

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// StartNCSService
    ///
    /// @brief  Start NCS Service thread
    ///
    /// @return CamxResultSuccess if started successfully
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult StartNCSService();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// StopNCSService
    ///
    /// @brief  Stop NCS Service thread
    ///
    /// @return CamxResultSuccess if started successfully
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult StopNCSService();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// FlushNCSService
    ///
    /// @brief  Flush NCS Service jobs
    ///
    /// @return CamxResultSuccess if flush successfull
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult FlushNCSService();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// NCSServicePollThread
    ///
    /// @brief  NCS polling thread for inputs from QSEE sensors, service
    ///
    /// @param  pPayload Pointer to the payload
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static VOID* NCSServicePollThread(
        VOID* pPayload);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ProcessPendingJobs
    ///
    /// @brief  Process NCS Service jobs
    ///
    /// @param  pThreadContext NCS thread context
    ///
    /// @return CamxResultSuccess if processing successfull
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    CamxResult ProcessPendingJobs(
        NCSThreadContext* pThreadContext);
};

CAMX_NAMESPACE_END

#endif // CAMXNCSSERVICE_H
