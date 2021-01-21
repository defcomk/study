////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  camxncssscconnection.cpp
/// @brief CamX NCS SSC connection implementaion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOWHINE FILE CP003:  QSEE Sensor interface requires us to use std string class
// NOWHINE FILE CP006:  QSEE Sensor interface requires us to use std string class
// NOWHINE FILE CP009:  Needed by QMI interface
// NOWHINE FILE CP010:  Needed by QMI interface
// NOWHINE FILE GR017:  Needed by QMI interface
// NOWHINE FILE PR007b: QSEE Sensor interface
// NOWHINE FILE PR009:  QSEE Sensor interface



#include <string>
#include <stdexcept>
#include <cinttypes>
#include <mutex>
#include <stdint.h>
#include <sched.h>
#include <map>
#include "utils/SystemClock.h"
#include "camxncssscconnection.h"
#include "qmi_client.h"
#include "sns_client_api_v01.h"
#include "worker.h"
#include "camxncssscutils.h"
#include "camxutils.h"


CAMX_NAMESPACE_BEGIN

using namespace std;
using namespace google::protobuf::io;
#define MAX_SVC_INFO_ARRAY_SIZE 5
/* number of times to wait for sensors service */
static const int SENSORS_SERVICE_DISCOVERY_TRIES = 4;
static const int SENSORS_SERVICE_DISCOVERY_TRIES_POST_SENSORLIST = 2;

static auto g_sensorsServiceDiscoveryTimeout = 2;
static auto g_sensorsServiceDiscoveryTimeoutPostSensorlist = 500;

static uint32_t g_qmierr_cnt;

struct QmiError : public runtime_error
{
    QmiError(int error_code, const std::string& what = "") :
        runtime_error(what + ": " + ErrorCodeToString(error_code)) { }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ErrorCodeToString
    ///
    /// @brief   covert error code to string
    ///
    /// @param   code        error code
    ///
    /// @return  None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static string ErrorCodeToString(
        int code)
    {
        static const map<int, string> error_map =
        {
            { QMI_NO_ERR, "qmi no error" },
            { QMI_INTERNAL_ERR, "qmi internal error" },
            { QMI_TIMEOUT_ERR, "qmi timeout" },
            { QMI_XPORT_BUSY_ERR, "qmi transport busy" },
        };
        string msg;
        try
        {
            msg = error_map.at(code);
        }
        catch (out_of_range& e)
        {
            msg = "qmi error";
        }
        return msg + " (" + to_string(code) + ")";
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Class that implements the NCS SSC qmi connection
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SSCQmiConnection
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SSCQmiConnection
    ///
    /// @brief   Constructor
    ///
    /// @param   eventCb     callback function to client
    ///
    /// @return  None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NOWHINE CP019:
    SSCQmiConnection(
        ssc_event_cb eventCb);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// ~SSCQmiConnection
    ///
    /// @brief   Destructor
    ///
    /// @return  None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ~SSCQmiConnection();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// SendRequest
    ///
    /// @brief   Send encoded request message to QMI
    ///
    /// @param   msg      encoded qmi message
    ///
    /// @return  None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void SendRequest(
        const string& msg);

private:
    // Do not implement the copy constructor or assignment operator
    SSCQmiConnection(const SSCQmiConnection& qmiConnection)                  = delete;
    SSCQmiConnection& operator= (const SSCQmiConnection& qmiConnection)      = delete;


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QmiConnect
    ///
    /// @brief  connect to QMI service
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void QmiConnect();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QmiDisconnect
    ///
    /// @brief  Disconnect qmi service
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void QmiDisconnect();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QmiWaitForService
    ///
    /// @brief  wati for the service to be connected
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void QmiWaitForService();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QmiNotifyCb
    ///
    /// @brief  Notify callback from QMI
    ///
    /// @param  type               client type
    /// @param  serviceObj         service object
    /// @param  serviceEvent       service event
    /// @param  pNotifyCbData      context for the callback
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static void QmiNotifyCb(
        qmi_client_type              type,
        qmi_idl_service_object_type  serviceObj,
        qmi_client_notify_event_type serviceEvent,
        void*                        pNotifyCbData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QmiIndicationCb
    ///
    /// @brief  Handle indication callback from QMI
    ///
    /// @param  type         client type
    /// @param  msgId        message id
    /// @param  pBuf         indication buffer
    /// @param  bufLength    indication buffer size
    /// @param  pCbData      context for the callback
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static void QmiIndicationCb(
        qmi_client_type  type,
        unsigned int     msgId,
        void*            pBuf,
        unsigned int     bufLength,
        void*            pCbData);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// QmiErrorCb
    ///
    /// @brief  Handle errot callback from QMI
    ///
    /// @param  type        client type
    /// @param  error       error recevied
    /// @param  pErrCbData  context for the callback
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static void QmiErrorCb(
        qmi_client_type        type,
        qmi_client_error_type  error,
        void*                  pErrCbData);


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// HandleIndication
    ///
    /// @brief  Handle a sns_client_report_ind_msg_v01 or sns_client_jumbod_report_ind_msg_v01
    ///         indication message received from SEE's Client Manager.
    ///
    /// @param  msgId      One of SNS_CLIENT_REPORT_IND_V01 or SNS_CLIENT_JUMBO_REPORT_IND_V01
    /// @param  pIndBuf    The encoded QMI message buffer
    /// @param  indBufLen  Length of ind_buf
    ///
    /// @return None
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void HandleIndication(
        unsigned int  msgId,
        void*         pIndBuf,
        unsigned int  indBufLen);

    static const uint32_t     QMI_ERRORS_HANDLED_ATTEMPTS = 10000;      ///< error handle number constant
    static const uint32_t     QMI_RESPONSE_TIMEOUT_MS     = 2000;       ///< response timeout constant

    worker                    m_worker;                                 ///< worker for error handling
    ssc_event_cb              m_eventCb;                                ///< event callback function to client
    qmi_client_type           m_qmihandle;                              ///< qmi handle
    bool                      m_serviceReady;                           ///< service ready flag
    bool                      m_serviceAccessed;                        ///< service accessed flag
    std::mutex                m_mutex;                                  ///< mutex
    std::condition_variable   m_cv;                                     ///< condition variable
    qmi_cci_os_signal_type    m_osParams;                               ///< qmi os parameers
    atomic<bool>              m_reconnecting;                           ///< flag to see if connection is being reset
    bool                      m_connectionClosed;                       ///< flag to check disconnection

};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::SSCQmiConnection
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SSCQmiConnection::SSCQmiConnection(
    ssc_event_cb event_cb)
    : m_eventCb(event_cb)
    , m_qmihandle(NULL)
    , m_serviceAccessed(false)
    , m_reconnecting(false)
    , m_connectionClosed(false)
{
    QmiConnect();
    CAMX_LOG_INFO(CamxLogGroupNCS, "_service_accessed %d", m_serviceAccessed);
    m_worker.setname("ssc_qmi_connection");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::~SSCQmiConnection
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SSCQmiConnection::~SSCQmiConnection()
{
    m_connectionClosed = true;
    QmiDisconnect();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::QmiNotifyCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSCQmiConnection::QmiNotifyCb(
    qmi_client_type              type,
    qmi_idl_service_object_type  serviceObj,
    qmi_client_notify_event_type serviceEvent,
    void*                        pNotifyCbData)
{
    CAMX_UNREFERENCED_PARAM(type);
    CAMX_UNREFERENCED_PARAM(serviceObj);
    CAMX_UNREFERENCED_PARAM(serviceEvent);

    SSCQmiConnection* pConn = static_cast<SSCQmiConnection*>(pNotifyCbData);
    unique_lock<mutex> lk(pConn->m_mutex);
    pConn->m_serviceReady = true;
    pConn->m_cv.notify_one();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::QmiWaitForService
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSCQmiConnection::QmiWaitForService()
{
    qmi_client_type notifier_handle;
    qmi_client_error_type qmi_err;
    qmi_cci_os_signal_type os_params;
    int num_tries = SENSORS_SERVICE_DISCOVERY_TRIES;

    qmi_err = qmi_client_notifier_init(SNS_CLIENT_SVC_get_service_object_v01(), &os_params, &notifier_handle);
    if (QMI_NO_ERR != qmi_err)
    {
        throw QmiError(qmi_err, "qmi_client_notifier_init() failed");
    }

    /* register a callback and wait until service becomes available */
    m_serviceReady = false;
    qmi_err = qmi_client_register_notify_cb(notifier_handle, QmiNotifyCb, this);
    if (qmi_err != QMI_NO_ERR)
    {
        qmi_client_release(notifier_handle);
        throw QmiError(qmi_err, "qmi_client_register_notify_cb() failed: %d");
    }

    if (m_serviceAccessed == true)
    {
        num_tries = SENSORS_SERVICE_DISCOVERY_TRIES_POST_SENSORLIST;
    }

    std::unique_lock<std::mutex> lk(m_mutex);
    bool timeout = false;
    while (num_tries > 0 && (m_serviceReady != true))
    {
        num_tries--;

        if (m_serviceAccessed == true)
        {
            // NOWHINE CF010,CF012:
            timeout = !m_cv.wait_for(lk, std::chrono::milliseconds(g_sensorsServiceDiscoveryTimeoutPostSensorlist),
                [this]{ return m_serviceReady; });
        }
        else
        {
            // NOWHINE CF010,CF012:
            timeout = !m_cv.wait_for(lk, std::chrono::seconds(g_sensorsServiceDiscoveryTimeout),
                [this]{ return m_serviceReady; });
        }

        if (timeout == true)
        {
            if (num_tries == 0)
            {
                lk.unlock();
                qmi_client_release(notifier_handle);
                throw runtime_error("FATAL: could not find sensors QMI service");
            }
            CAMX_LOG_ERROR(CamxLogGroupNCS, "timeout while waiting for sensors QMI service: "
                     "will try %d more time(s)", num_tries);
        } else
        {
            m_serviceAccessed = true;
        }
    }
    lk.unlock();
    qmi_client_release(notifier_handle);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::QmiConnect
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSCQmiConnection::QmiConnect()
{
    qmi_idl_service_object_type svc_obj =
        SNS_CLIENT_SVC_get_service_object_v01();

    qmi_client_error_type  qmi_err;
    qmi_service_info       svc_info_array[MAX_SVC_INFO_ARRAY_SIZE];
    uint32_t               num_services;
    uint32_t               num_entries = MAX_SVC_INFO_ARRAY_SIZE;

    /* svc_info_array[5] - Initialized to 0 to avoid static analysis errors */
    for (uint32_t i = 0 ; i < num_entries ; i++)
    {
        memset(&svc_info_array[i], 0, sizeof(svc_info_array[i]));
    }

    CAMX_LOG_VERBOSE(CamxLogGroupNCS, "waiting for sensors qmi service");
    QmiWaitForService();
    CAMX_LOG_VERBOSE(CamxLogGroupNCS, "connecting to qmi service");
    qmi_err = qmi_client_get_service_list(svc_obj, svc_info_array,
                                          &num_entries, &num_services);
    if (QMI_NO_ERR != qmi_err)
    {
        throw QmiError(qmi_err, "qmi_client_get_service_list() failed");
    }

    if (num_entries == 0)
    {
        throw runtime_error("sensors service has no available instances");
    }

    if (m_connectionClosed == true)
    {
        CAMX_LOG_INFO(CamxLogGroupNCS, "connection got closed do not open qmi_channel");
        return ;
    }

    /* As only one qmi service is expected for sensors, use the 1st instance */
    qmi_service_info svc_info = svc_info_array[0];


    std::unique_lock<std::mutex> lk(m_mutex);
    qmi_err = qmi_client_init(&svc_info,
                               svc_obj,
                               QmiIndicationCb,
                               static_cast<void*>(this),
                              &m_osParams,
                              &m_qmihandle);
    if (qmi_err != QMI_IDL_LIB_NO_ERR)
    {
        lk.unlock();
        throw QmiError(qmi_err, "qmi_client_init() failed");
    }

    qmi_err = qmi_client_register_error_cb(m_qmihandle, QmiErrorCb, this);
    if (QMI_NO_ERR != qmi_err)
    {
        lk.unlock();
        qmi_client_release(m_qmihandle);
        m_qmihandle = NULL;
        throw QmiError(qmi_err, "qmi_client_register_error_cb() failed");
    }
    lk.unlock();
    CAMX_LOG_VERBOSE(CamxLogGroupNCS, "connected to ssc for %p", this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::QmiDisconnect
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSCQmiConnection::QmiDisconnect()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_qmihandle != NULL)
    {
        qmi_client_release(m_qmihandle);
        m_qmihandle = NULL;
        /* in ssr call back and sensor disabled , so notify qmi_connect to comeout */
        if (m_connectionClosed == true)
        {
            m_cv.notify_one();
        }
    }
    /* explicit unlock not required , just added not to miss the logi c*/
    lk.unlock();
    CAMX_LOG_VERBOSE(CamxLogGroupNCS, "disconnected from ssc for %p", this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::HandleIndication
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSCQmiConnection::HandleIndication(
    unsigned int msgId,
    void*        pIndBuf,
    unsigned int ind_buf_len)
{
    int32_t  qmi_err;
    uint32_t ind_size;
    size_t   buffer_len;
    uint8_t* pBuffer = NULL;

    CAMX_LOG_VERBOSE(CamxLogGroupNCS, "msg_id %d " , msgId);
    if (SNS_CLIENT_REPORT_IND_V01 == msgId)
    {
        sns_client_report_ind_msg_v01* pInd = static_cast<sns_client_report_ind_msg_v01*>(
            CAMX_CALLOC(sizeof(sns_client_report_ind_msg_v01)));
        if (pInd == NULL)
        {
            CAMX_LOG_ERROR(CamxLogGroupNCS, "Can not allocate memory for incoming indication");
            return;
        }

        ind_size = sizeof(sns_client_report_ind_msg_v01);
        qmi_err = qmi_idl_message_decode(SNS_CLIENT_SVC_get_service_object_v01(),
                                         QMI_IDL_INDICATION,
                                         msgId,
                                         pIndBuf,
                                         ind_buf_len,
                                         static_cast<void*>(pInd),
                                         ind_size);
        if (QMI_IDL_LIB_NO_ERR != qmi_err)
        {
            CAMX_LOG_ERROR(CamxLogGroupNCS, "qmi_idl_message_decode() failed. qmi_err=%d", qmi_err);
            CAMX_FREE(pInd);
            pInd = NULL;
            return;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupNCS, "indication, payload_len=%u", pInd->payload_len);
        buffer_len = pInd->payload_len;
        if (buffer_len > SNS_CLIENT_REPORT_LEN_MAX_V01)
        {
            CAMX_LOG_ERROR(CamxLogGroupNCS, "received invalid payload length %d", buffer_len);
        }
        else
        {
            pBuffer = static_cast<uint8_t*>(CAMX_CALLOC(buffer_len));
            if (pBuffer == NULL)
            {
                CAMX_LOG_ERROR(CamxLogGroupNCS, "buffer failed to creat ");
                CAMX_FREE(pInd);
                pInd = NULL;
                return;
            }
            else
            {
                memcpy(pBuffer, pInd->payload, buffer_len);
                CAMX_FREE(pInd);
                this->m_eventCb(pBuffer, buffer_len);
                CAMX_FREE(pBuffer);
            }
        }
    }
    else if (SNS_CLIENT_JUMBO_REPORT_IND_V01 == msgId)
    {
        sns_client_jumbo_report_ind_msg_v01* pIndJumbo = static_cast<sns_client_jumbo_report_ind_msg_v01*>(
            CAMX_CALLOC(sizeof(sns_client_jumbo_report_ind_msg_v01)));
        if (pIndJumbo == NULL)
        {
            CAMX_LOG_ERROR(CamxLogGroupNCS, "Error while creating memory for ind_jumbo");
            return;
        }

        ind_size = sizeof(sns_client_jumbo_report_ind_msg_v01);
        qmi_err = qmi_idl_message_decode(SNS_CLIENT_SVC_get_service_object_v01(),
                                         QMI_IDL_INDICATION,
                                         msgId,
                                         pIndBuf,
                                         ind_buf_len,
                                         static_cast<void*>(pIndJumbo),
                                         ind_size);
        if (QMI_IDL_LIB_NO_ERR != qmi_err)
        {
            CAMX_LOG_ERROR(CamxLogGroupNCS, "qmi_idl_message_decode() failed. qmi_err=%d", qmi_err);
            CAMX_FREE(pIndJumbo);
            pIndJumbo = NULL;
            return;
        }

        CAMX_LOG_VERBOSE(CamxLogGroupNCS, "indication, payload_len=%u", pIndJumbo->payload_len);
        buffer_len = pIndJumbo->payload_len;
        if (buffer_len > SNS_CLIENT_JUMBO_REPORT_LEN_MAX_V01)
        {
            CAMX_LOG_ERROR(CamxLogGroupNCS, "received invalid payload length %d", buffer_len);
        }
        else
        {
            pBuffer = static_cast<uint8_t*>(CAMX_CALLOC(buffer_len));
            if (pBuffer == NULL)
            {
                CAMX_LOG_ERROR(CamxLogGroupNCS, "buffer failed to creat ind_jumbo ");
                CAMX_FREE(pIndJumbo);
                pIndJumbo = NULL;
                return;
            }
            memcpy(pBuffer, pIndJumbo->payload, buffer_len);
            CAMX_FREE(pIndJumbo);
            this->m_eventCb(pBuffer, buffer_len);
            CAMX_FREE(pBuffer);
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupNCS, "Unknown indication message ID %i", msgId);
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::QmiIndicationCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSCQmiConnection::QmiIndicationCb(
    qmi_client_type type,
    unsigned int    msgId,
    void*           pBuf,
    unsigned int    bufLength,
    void*           pCbData)
{
    CAMX_UNREFERENCED_PARAM(type);

    SSCQmiConnection* pConnection = static_cast<SSCQmiConnection*>(pCbData);
    pConnection->HandleIndication(msgId, pBuf, bufLength);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::QmiErrorCb
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSCQmiConnection::QmiErrorCb(
    qmi_client_type       type,
    qmi_client_error_type error,
    void*                 pErrorCbData)
{

    CAMX_UNREFERENCED_PARAM(type);

    SSCQmiConnection* pConnection = static_cast<SSCQmiConnection*>(pErrorCbData);

    CAMX_LOG_ERROR(CamxLogGroupNCS, "error=%d", error);
    if (error != QMI_NO_ERR)
    {
        pConnection->m_reconnecting = true;
        /* handle error asynchronously using worker thread */
        pConnection->m_serviceAccessed = false;
        pConnection->m_worker.add_task(NULL, [pConnection]{
            CAMX_LOG_INFO(CamxLogGroupNCS, "qmi error, qmi_disconnect %p", pConnection);
            /* re-establish qmi connection */
            pConnection->QmiDisconnect();
            try {
                CAMX_LOG_INFO(CamxLogGroupNCS, "qmi error, trying to reconnect");
                pConnection->QmiConnect();
            } catch (const exception& e) {
                CAMX_LOG_ERROR(CamxLogGroupNCS, "could not reconnect: %s", e.what());
                return;
            }
            pConnection->m_reconnecting = false;
            CAMX_LOG_INFO(CamxLogGroupNCS, "qmi connection re-established");
            /* notify user about connection reset */
            if (pConnection->m_connectionClosed == true) {
                CAMX_LOG_INFO(CamxLogGroupNCS, "sensor deactivated during ssr");
                return;
            }
        });
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCQmiConnection::SendRequest
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSCQmiConnection::SendRequest(
    const string& pb_req_msg_encoded)
{

    if (m_reconnecting == true)
    {
        CAMX_LOG_ERROR(CamxLogGroupNCS, "qmi connection failed, cannot send data");
        return;
    }

    sns_client_req_msg_v01 req_msg;

    if (pb_req_msg_encoded.size() > SNS_CLIENT_REQ_LEN_MAX_V01)
    {
        throw runtime_error("error: payload size too large");
    }

    memcpy(req_msg.payload, pb_req_msg_encoded.c_str(),
           pb_req_msg_encoded.size());

    req_msg.use_jumbo_report_valid = true;
    req_msg.use_jumbo_report = true;
    req_msg.payload_len = pb_req_msg_encoded.size();
    qmi_client_error_type qmi_err;
    sns_client_resp_msg_v01 resp = {};
    /* send a sync message to ssc */
    qmi_err = qmi_client_send_msg_sync(m_qmihandle,
                                       SNS_CLIENT_REQ_V01,
                                       (void*)&req_msg,
                                       sizeof(req_msg),
                                       &resp,
                                       sizeof(sns_client_resp_msg_v01),
                                       QMI_RESPONSE_TIMEOUT_MS);
    if (qmi_err != QMI_NO_ERR)
    {
        g_qmierr_cnt++;
        if (g_qmierr_cnt > QMI_ERRORS_HANDLED_ATTEMPTS)
        {
            CAMX_LOG_VERBOSE(CamxLogGroupNCS, "triggred ssr _qmi_err_cnt %d", g_qmierr_cnt);
            g_qmierr_cnt = 0;

            /* if QMI_NO_ERR and ssr triggered it is surely ssr_simulate */
            throw QmiError(qmi_err,
                            "ssr triggered after qmi_client_send_msg_sync (client_id=)"
                            + to_string(resp.client_id) + ", result="
                            + to_string(resp.result));
        }
        else
        {
            throw QmiError(qmi_err,
                            "qmi_client_send_msg_sync() failed, (client_id=)"
                            + to_string(resp.client_id) + ", result="
                            + to_string(resp.result));
        }
    }
    else
    {
        /* occassional failure of QMI , recovered with in QMI_ERRORS_HANDLED_ATTEMPTS */
        if (g_qmierr_cnt != 0)
        {
            g_qmierr_cnt = 0;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCConnection::SSCConnection
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SSCConnection::SSCConnection(
    ssc_event_cb event_cb)
    : _qmi_conn(make_unique<SSCQmiConnection>(event_cb))
{
    CAMX_LOG_VERBOSE(CamxLogGroupNCS, "ssc connected");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCConnection::~SSCConnection
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SSCConnection::~SSCConnection()
{
    CAMX_LOG_VERBOSE(CamxLogGroupNCS, "ssc disconnected");
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SSCConnection::send_request
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SSCConnection::send_request(
    const string& pb_req_message_encoded)
{
    if (_qmi_conn != NULL)
    {
        try {
            _qmi_conn->SendRequest(pb_req_message_encoded);
        }
        catch (const exception& e) {
            CAMX_LOG_ERROR(CamxLogGroupNCS, "Failed to send request %s", e.what());
        }
    }
    else
    {
        CAMX_LOG_ERROR(CamxLogGroupNCS, "_qmi_conn is NULL");
    }
}

CAMX_NAMESPACE_END
