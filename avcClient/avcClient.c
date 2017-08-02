/**
 * @file avcClient.c
 *
 * client of the LWM2M stack
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

/* include files */
#include <stdbool.h>
#include <stdint.h>
#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/timer.h>
#include <lwm2mcore/security.h>

#include "legato.h"
#include "interfaces.h"
#include "avcClient.h"

//--------------------------------------------------------------------------------------------------
/**
 * Static instance reference for LWM2MCore
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Ref_t Lwm2mInstanceRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static data connection state for agent
 */
//--------------------------------------------------------------------------------------------------
static bool DataConnected = false;

//--------------------------------------------------------------------------------------------------
/**
 * Static data reference
 */
//--------------------------------------------------------------------------------------------------
static le_data_RequestObjRef_t DataRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static data connection handler
 */
//--------------------------------------------------------------------------------------------------
static le_data_ConnectionStateHandlerRef_t DataHandler;

//--------------------------------------------------------------------------------------------------
/**
 * Event ID on bootstrap connection failure.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t BsFailureEventId;

//--------------------------------------------------------------------------------------------------
/**
 * Denoting a session is established to the DM server.
 */
//--------------------------------------------------------------------------------------------------
static bool SessionStarted = false;

//--------------------------------------------------------------------------------------------------
/**
 * Retry timers related data. RetryTimersIndex is index to the array of RetryTimers.
 * RetryTimersIndex of -1 means the timers are to be retrieved. A timer of value 0 means it's
 * disabled. The timers values are in minutes.
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t RetryTimerRef = NULL;
static int RetryTimersIndex = -1;
static uint16_t RetryTimers[LE_AVC_NUM_RETRY_TIMERS] = {0};

//--------------------------------------------------------------------------------------------------
/**
 * Used for reporting LE_AVC_NO_UPDATE if there has not been any activity between the device and
 * the server for a specific amount of time, after a session has been started.
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t ActivityTimerRef;

//--------------------------------------------------------------------------------------------------
/**
 * Default activity timer value
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_ACTIVITY_TIMER 20

//--------------------------------------------------------------------------------------------------
/**
 * Convert an OMA FUMO (Firmware Update Management Object) error to an AVC error code
 */
//--------------------------------------------------------------------------------------------------
static le_avc_ErrorCode_t ConvertFumoErrorCode
(
    uint32_t fumoError
)
{
    switch (fumoError)
    {
        case 0:
            return LE_AVC_ERR_NONE;

        case LWM2MCORE_FUMO_CORRUPTED_PKG:
        case LWM2MCORE_FUMO_UNSUPPORTED_PKG:
            return LE_AVC_ERR_BAD_PACKAGE;

        case LWM2MCORE_FUMO_FAILED_VALIDATION:
            return LE_AVC_ERR_SECURITY_FAILURE;

        case LWM2MCORE_FUMO_INVALID_URI:
        case LWM2MCORE_FUMO_ALTERNATE_DL_ERROR:
        case LWM2MCORE_FUMO_NO_SUFFICIENT_MEMORY:
        default:
            return LE_AVC_ERR_INTERNAL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Call back registered in LWM2M client for bearer related events
 */
//--------------------------------------------------------------------------------------------------
static void BearerEventCb
(
    bool connected,     ///< [IN] Indicates if the bearer is connected or disconnected
    void* contextPtr    ///< [IN] User data
)
{
    LE_INFO("connected %d", connected);
    if (connected)
    {
        char endpointPtr[LWM2MCORE_ENDPOINT_LEN];
        bool result = false;

        /* Register objects to LWM2M and set the device endpoint:
         * - endpoint shall be unique for each client: IMEI/ESN/MEID
         * - the number of objects we will be passing through and the objects array
         */

        /* Get the device endpoint: IMEI */
        memset(endpointPtr, 0, LWM2MCORE_ENDPOINT_LEN);
        if (LE_OK != le_info_GetImei((char*)endpointPtr, (uint32_t) LWM2MCORE_ENDPOINT_LEN))
        {
            LE_ERROR("Error to retrieve the device IMEI");
            return;
        }

        // Register to the LWM2M agent
        if (!lwm2mcore_ObjectRegister(Lwm2mInstanceRef, endpointPtr, NULL, NULL))
        {
            LE_ERROR("ERROR in LWM2M obj reg");
            return;
        }

        result = lwm2mcore_Connect(Lwm2mInstanceRef);
        if (result != true)
        {
            LE_ERROR("connect error");
        }
    }
    else
    {
        if (NULL != Lwm2mInstanceRef)
        {
            // If the LWM2MCORE_TIMER_STEP timer is running, this means that a connection is active
            if (true == lwm2mcore_TimerIsRunning(LWM2MCORE_TIMER_STEP))
            {
                avcClient_Disconnect(false);
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback for the connection state
 */
//--------------------------------------------------------------------------------------------------
static void ConnectionStateHandler
(
    const char* intfNamePtr,    ///< [IN] Interface name
    bool connected,             ///< [IN] connection state (true = connected, else false)
    void* contextPtr            ///< [IN] User data
)
{
    if (connected)
    {
        LE_DEBUG("Connected through interface '%s'", intfNamePtr);
        DataConnected = true;
        /* Call the callback */
        BearerEventCb(connected, contextPtr);
    }
    else
    {
        LE_WARN("Disconnected from data connection service, current state %d", DataConnected);
        if (DataConnected)
        {
            /* Call the callback */
            BearerEventCb(connected, contextPtr);
            DataConnected = false;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback for the LWM2M events linked to package download and update
 *
 * @return
 *      - 0 on success
 *      - negative value on failure.

 */
//--------------------------------------------------------------------------------------------------
static int PackageEventHandler
(
    lwm2mcore_Status_t status              ///< [IN] event status
)
{
    int result = 0;

    switch (status.event)
    {
        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_DETAILS:
            // Notification of download pending is sent from user agreement callback.
            break;

        case LWM2MCORE_EVENT_DOWNLOAD_PROGRESS:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_IN_PROGRESS, LE_AVC_FIRMWARE_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_IN_PROGRESS, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FINISHED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                // The download thread finished the file download without any error, but the FOTA
                // update package still might be rejected by the store thread, e.g. if the received
                // file is incomplete or contains any error.
                // The download complete event is therefore not sent now and will be sent only when
                // the store thread also exits without error.
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_COMPLETE, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else
            {
                LE_ERROR("Not yet supported package download type %d",
                         status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FAILED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_FAILED, LE_AVC_FIRMWARE_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_FAILED, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_UPDATE_STARTED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_IN_PROGRESS, LE_AVC_FIRMWARE_UPDATE,
                                        -1, -1, LE_AVC_ERR_NONE);
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_IN_PROGRESS, LE_AVC_APPLICATION_UPDATE,
                                        -1, -1, LE_AVC_ERR_NONE);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_UPDATE_FINISHED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_COMPLETE, LE_AVC_FIRMWARE_UPDATE,
                                        -1, -1, LE_AVC_ERR_NONE);
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_COMPLETE, LE_AVC_APPLICATION_UPDATE,
                                        -1, -1, LE_AVC_ERR_NONE);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_UPDATE_FAILED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_FAILED, LE_AVC_FIRMWARE_UPDATE,
                                        -1, -1, ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_FAILED, LE_AVC_APPLICATION_UPDATE,
                                        -1, -1, ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        default:
            if (LWM2MCORE_EVENT_LAST <= status.event)
            {
                LE_ERROR("unsupported event %d", status.event);
                result = -1;
            }
            break;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback for the LWM2M events
 *
 * @return
 *      - 0 on success
 *      - negative value on failure.

 */
//--------------------------------------------------------------------------------------------------
static int EventHandler
(
    lwm2mcore_Status_t status              ///< [IN] event status
)
{
    int result = 0;
    static bool authStartedSent = false;

    switch (status.event)
    {
        case LWM2MCORE_EVENT_SESSION_STARTED:
            LE_DEBUG("Session start");
            break;

        case LWM2MCORE_EVENT_SESSION_FAILED:
            LE_ERROR("Session failure");
            // If the device is connected to the bootstrap server, disconnect from server
            // If the device is connected to the DM server, a bootstrap connection will be
            // automatically initiated (session is not stopped)
            if (LE_AVC_BOOTSTRAP_SESSION == le_avc_GetSessionType())
            {
                LE_ERROR("Session failure on bootstrap server");
                le_event_Report(BsFailureEventId, NULL, 0);
            }
            break;

        case LWM2MCORE_EVENT_SESSION_FINISHED:
            LE_DEBUG("Session finished");
            avcServer_UpdateHandler(LE_AVC_SESSION_STOPPED, LE_AVC_UNKNOWN_UPDATE,
                                    -1, -1, LE_AVC_ERR_NONE);

            SessionStarted = false;

            break;

        case LWM2MCORE_EVENT_LWM2M_SESSION_TYPE_START:
            if (status.u.session.type == LWM2MCORE_SESSION_BOOTSTRAP)
            {
                LE_DEBUG("Connected to bootstrap");
            }
            else
            {
                LE_DEBUG("Connected to DM");
                avcServer_UpdateHandler(LE_AVC_SESSION_STARTED, LE_AVC_UNKNOWN_UPDATE,
                                        -1, -1, LE_AVC_ERR_NONE);

                SessionStarted = true;
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_DETAILS:
        case LWM2MCORE_EVENT_DOWNLOAD_PROGRESS:
        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FINISHED:
        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FAILED:
        case LWM2MCORE_EVENT_UPDATE_STARTED:
        case LWM2MCORE_EVENT_UPDATE_FINISHED:
        case LWM2MCORE_EVENT_UPDATE_FAILED:
            result = PackageEventHandler(status);
            break;

        case LWM2MCORE_EVENT_AUTHENTICATION_STARTED:
            // Send only the first "authentication started" notification in case the device
            // authenticates first with the BS then the DM server
            if (!authStartedSent)
            {
                avcServer_UpdateHandler(LE_AVC_AUTH_STARTED, LE_AVC_UNKNOWN_UPDATE,
                                        -1, -1, LE_AVC_ERR_NONE);
                authStartedSent = true;
            }
            if (status.u.session.type == LWM2MCORE_SESSION_BOOTSTRAP)
            {
                LE_DEBUG("Authentication to BS started");
            }
            else
            {
                LE_DEBUG("Authentication to DM started");
                // Authentication with the DM server started, reset the flag
                authStartedSent = false;
            }
            break;

        case LWM2MCORE_EVENT_AUTHENTICATION_FAILED:
            if (status.u.session.type == LWM2MCORE_SESSION_BOOTSTRAP)
            {
                LE_WARN("Authentication to BS failed");
            }
            else
            {
                LE_WARN("Authentication to DM failed");
            }
            avcServer_UpdateHandler(LE_AVC_AUTH_FAILED, LE_AVC_UNKNOWN_UPDATE,
                                    -1, -1, LE_AVC_ERR_NONE);
            break;

        default:
            if (LWM2MCORE_EVENT_LAST <= status.event)
            {
                LE_ERROR("unsupported event %d", status.event);
                result = -1;
            }
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Reset the retry timers by resetting the retrieved reset timer config, and stopping the current
 * retry timer.
 */
//--------------------------------------------------------------------------------------------------
static void ResetRetryTimers
(
    void
)
{
    RetryTimersIndex = -1;
    memset(RetryTimers, 0, sizeof(RetryTimers));
    le_timer_Stop(RetryTimerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Start the bearer
 */
//--------------------------------------------------------------------------------------------------
static void StartBearer
(
    void
)
{
    // Attempt to connect
    Lwm2mInstanceRef = lwm2mcore_Init(EventHandler);

    /* Initialize the bearer */
    /* Open a data connection */
    le_data_ConnectService();

    DataHandler = le_data_AddConnectionStateHandler(ConnectionStateHandler, NULL);

    /* Request data connection */
    DataRef = le_data_Request();
    LE_ASSERT(NULL != DataRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * Stop the bearer - undo what StartBearer does
 */
//--------------------------------------------------------------------------------------------------
static void StopBearer
(
    void
)
{
    if (NULL != Lwm2mInstanceRef)
    {
        if (DataRef)
        {
            /* Close the data connection */
            le_data_Release(DataRef);
            /* Remove the data handler */
            le_data_RemoveConnectionStateHandler(DataHandler);
            DataRef = NULL;
        }
        /* The data connection is closed */
        lwm2mcore_Free(Lwm2mInstanceRef);
        Lwm2mInstanceRef = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Connect to the server. Connection is retried according to the configured retry timers, if session
 * isn't started. If this function is called while one of the retry timers is running, retry isn't
 * performed and LE_BUSY is returned.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_DUPLICATE if already connected.
 *      - LE_BUSY if currently retrying.
 *      - LE_NOT_PERMITTED if device is in airplane mode
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Connect
(
    void
)
{
    le_onoff_t radioStatus;
    if ((le_mrc_GetRadioPower(&radioStatus) == LE_OK) && radioStatus == LE_OFF)
    {
        LE_INFO("Device in airplane mode.");
        return LE_NOT_PERMITTED;
    }

    if (SessionStarted)
    {
        LE_INFO("Session already started.");

        // No need to start a retry timer. Perform reset/cleanup.
        ResetRetryTimers();

        return LE_DUPLICATE;
    }
    else
    {
        // Retry is in progress.
        if (le_timer_IsRunning(RetryTimerRef))
        {
            return LE_BUSY;
        }

        // If Lwm2mInstanceRef exists, then that means the current call is a "retry", which is
        // performed by stopping the previous data connection first.
        if (NULL != Lwm2mInstanceRef)
        {
            StopBearer();
        }

        StartBearer();

        // attempt to start a retry timer.

        // if index is -1, then get the retry timers config. The implication is that while a retry
        // timer is running, changes to retry timers aren't applied. They are applied when retry
        // timers are being reset.
        if (-1 == RetryTimersIndex)
        {
            size_t numTimers = NUM_ARRAY_MEMBERS(RetryTimers);

            if (LE_OK != le_avc_GetRetryTimers(RetryTimers, &numTimers))
            {
                LE_WARN("Failed to retrieve retry timers config. Failed session start is not \
                         retried.");

                return LE_OK;
            }

            LE_ASSERT(LE_AVC_NUM_RETRY_TIMERS == numTimers);

            RetryTimersIndex = 0;
        }
        else
        {
            RetryTimersIndex++;
        }

        // Get the next valid retry timer

        // see which timer we are at by looking at RetryTimersIndex
        // if the timer is 0, get the next one. (0 means disabled / not used)
        // if we run out of timers, do nothing.  Perform reset/cleanup
        while((RetryTimersIndex < LE_AVC_NUM_RETRY_TIMERS) &&
              (0 == RetryTimers[RetryTimersIndex]))
        {
                RetryTimersIndex++;
        }

        // This is the case when we've run out of timers. Reset/cleanup, and don't start the next
        // retry timer (since there aren't any left).
        if ((RetryTimersIndex >= LE_AVC_NUM_RETRY_TIMERS) || (RetryTimersIndex < 0))
        {
            ResetRetryTimers();
        }
        // Start the next retry timer.
        else
        {
            LE_INFO("Starting retry timer of %d min at index %d",
                    RetryTimers[RetryTimersIndex], RetryTimersIndex);

            le_clk_Time_t interval = {RetryTimers[RetryTimersIndex] * 60, 0};

            LE_ASSERT(LE_OK == le_timer_SetInterval(RetryTimerRef, interval));
            LE_ASSERT(LE_OK == le_timer_SetHandler(RetryTimerRef, avcClient_Connect));
            LE_ASSERT(LE_OK == le_timer_Start(RetryTimerRef));
        }

        return LE_OK;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to close a connection
 *
 * @return
 *      - LE_OK in case of success
 *      - LE_FAULT in case of failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Disconnect
(
    bool resetRetry  ///< [IN] if true, reset the retry timers
)
{
    LE_DEBUG("Disconnect");

    le_result_t result = LE_OK;

    /* If the LWM2MCORE_TIMER_STEP timer is running, this means that a connection is active.
       In that case, attempt to disconnect. */
    if (true == lwm2mcore_TimerIsRunning(LWM2MCORE_TIMER_STEP))
    {
        result = (true == lwm2mcore_Disconnect(Lwm2mInstanceRef)) ? LE_OK : LE_FAULT;
    }

    StopBearer();

    if (true == resetRetry)
    {
        ResetRetryTimers();
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to send a registration update
 *
 * @return
 *      - LE_OK in case of success
 *      - LE_UNAVAILABLE when session closed.
 *      - LE_FAULT in case of failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Update
(
    void
)
{
    LE_DEBUG("Registration update");

    if (Lwm2mInstanceRef == NULL)
    {
        LE_DEBUG("Session closed");
        return LE_UNAVAILABLE;
    }

    if (true == lwm2mcore_Update(Lwm2mInstanceRef))
    {
        return LE_OK;
    }
    else
    {
        return LE_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to push data
 *
 * @return
 *      - LE_OK in case of success
 *      - LE_BUSY if busy pushing data
 *      - LE_FAULT in case of failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Push
(
    uint8_t* payload,
    size_t payloadLength,
    lwm2mcore_PushContent_t contentType,
    uint16_t* midPtr
)
{
    LE_DEBUG("Push data");

    lwm2mcore_PushResult_t rc = lwm2mcore_Push(Lwm2mInstanceRef,
                                               payload,
                                               payloadLength,
                                               contentType,
                                               midPtr);

    switch (rc)
    {
        case LWM2MCORE_PUSH_INITIATED:
            return LE_OK;
        case LWM2MCORE_PUSH_BUSY:
            return LE_BUSY;
        case LWM2MCORE_PUSH_FAILED:
        default:
            return LE_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Send instances of object 9 and the Legato objects for all currently installed applications.
 *
 */
//--------------------------------------------------------------------------------------------------
void avcClient_SendList
(
    char* lwm2mObjListPtr,      ///< [IN] Object instances list
    size_t objListLen           ///< [IN] List length
)
{
    lwm2mcore_UpdateSwList(Lwm2mInstanceRef, lwm2mObjListPtr, objListLen);
}

//--------------------------------------------------------------------------------------------------
/**
 * Returns the instance reference of this client
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Ref_t avcClient_GetInstance
(
    void
)
{
    return Lwm2mInstanceRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to get session status
 *
 * @return
 *      - LE_AVC_DM_SESSION when the device is connected to the DM server
 *      - LE_AVC_BOOTSTRAP_SESSION when the device is connected to the BS server
 *      - LE_AVC_SESSION_INVALID in other cases
 */
//--------------------------------------------------------------------------------------------------
le_avc_SessionType_t avcClient_GetSessionType
(
    void
)
{
    bool isDeviceManagement = false;

    if (lwm2mcore_ConnectionGetType(Lwm2mInstanceRef, &isDeviceManagement))
    {
        return (isDeviceManagement ? LE_AVC_DM_SESSION : LE_AVC_BOOTSTRAP_SESSION);
    }
    return LE_AVC_SESSION_INVALID;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Handler to terminate a connection to bootstrap on failure
 */
//--------------------------------------------------------------------------------------------------
void BsFailureHandler
(
    void* reportPtr
)
{
    avcClient_Disconnect(true);
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for ActivityTimerRef expiry
 */
//--------------------------------------------------------------------------------------------------
static void ActivityTimerHandler
(
    le_timer_Ref_t timerRef    ///< This timer has expired
)
{
    LE_DEBUG("Activity timer expired; reporting LE_AVC_NO_UPDATE");
    avcServer_UpdateHandler(LE_AVC_NO_UPDATE, LE_AVC_UNKNOWN_UPDATE, -1, -1, LE_AVC_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function sets up the activity timer. The timeout will default to 20 seconds if
 * user defined value doesn't exist or if the defined value is less than 0.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_SetActivityTimeout
(
    int timeout               ///< [IN] Timeout for activity timer
)
{
    // After a session is started, if there has been no activity within the timer
    // interval, then report LE_AVC_NO_UPDATE.
    le_clk_Time_t timerInterval = { .sec=DEFAULT_ACTIVITY_TIMER, .usec=0 };

    if (timeout > 0)
    {
        timerInterval.sec = timeout;
    }

    LE_DEBUG("Activity timeout set to %d seconds.", (int)timerInterval.sec);

    ActivityTimerRef = le_timer_Create("Activity timer");
    le_timer_SetInterval(ActivityTimerRef, timerInterval);
    le_timer_SetHandler(ActivityTimerRef, ActivityTimerHandler);
}

//--------------------------------------------------------------------------------------------------
/**
 * Start a timer to monitor the activity between device and server.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_StartActivityTimer
(
    void
)
{
    le_timer_Start(ActivityTimerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop a timer to monitor the activity between device and server.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_StopActivityTimer
(
    void
)
{
    if (le_timer_IsRunning(ActivityTimerRef))
    {
        LE_DEBUG("Stopping Activity timer");
        le_timer_Stop(ActivityTimerRef);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Restart a timer to monitor the activity between device and server.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_RestartActivityTimer
(
    void
)
{
    if (le_timer_IsRunning(ActivityTimerRef))
    {
        LE_DEBUG("Restarting Activity timer");
        le_timer_Restart(ActivityTimerRef);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialization function avcClient. Should be called only once.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_Init
(
   void
)
{
    BsFailureEventId = le_event_CreateId("BsFailure", 0);
    le_event_AddHandler("BsFailureHandler", BsFailureEventId, BsFailureHandler);
    RetryTimerRef = le_timer_Create("AvcRetryTimer");
}
