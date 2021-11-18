/**
 * @file avcClient.c
 *
 * Client of the LwM2M stack.
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
#include <lwm2mcore/device.h>

#include "legato.h"
#include "interfaces.h"
#include "avcClient.h"
#include "tpf/tpfServer.h"
#include "avcServer/avcServer.h"
#include "packageDownloader/packageDownloader.h"
#include "workspace.h"
//--------------------------------------------------------------------------------------------------
/**
 * Initialize memory areas for Lwm2m
 */
//--------------------------------------------------------------------------------------------------
// Prototype of function in osPlatform.c -- included to avoid a header for one function.
extern void lwm2mcore_InitMem
(
    void
);

//--------------------------------------------------------------------------------------------------
// Definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Fwupdate object ID.
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_OBJECT_ID  5

//--------------------------------------------------------------------------------------------------
/**
 * Fwupdate instance object ID.
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_OBJECT_INSTANCE_ID  0
//--------------------------------------------------------------------------------------------------
/**
 * Fwupdate resource id.
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_WRITE_RESOURCE_ID  1

//--------------------------------------------------------------------------------------------------
/**
 * Fwupdate write resource id.
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_EXEC_RESOURCE_ID  2

//--------------------------------------------------------------------------------------------------
/**
 * Fwupdate execute resource instance id.
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_RESOURCE_INSTANCE_ID  0

//--------------------------------------------------------------------------------------------------
/**
 * Length of date/time buffer, including NULL-terminator.
 */
//--------------------------------------------------------------------------------------------------
#define DATE_TIME_LENGTH  200

//--------------------------------------------------------------------------------------------------
/**
 * Year used to determine if date is correctly set.
 */
//--------------------------------------------------------------------------------------------------
#define MINIMAL_YEAR  2017

//--------------------------------------------------------------------------------------------------
/**
 * Define value for the base used in strtoul.
 */
//--------------------------------------------------------------------------------------------------
#define BASE10  10

//--------------------------------------------------------------------------------------------------
/**
 * Default activity timer value.
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_ACTIVITY_TIMER  20

//--------------------------------------------------------------------------------------------------
/**
 * Size of activity timer events memory pool.
 */
//--------------------------------------------------------------------------------------------------
#define ACTIVITY_TIMER_EVENTS_POOL_SIZE  5

//--------------------------------------------------------------------------------------------------
// Local variables.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Static instance reference for LWM2MCore.
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Ref_t Lwm2mInstanceRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static data connection state for agent.
 */
//--------------------------------------------------------------------------------------------------
static bool DataConnected = false;

//--------------------------------------------------------------------------------------------------
/**
 * Static data reference.
 */
//--------------------------------------------------------------------------------------------------
static le_data_RequestObjRef_t DataRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static data connection handler.
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
#if LE_CONFIG_AVC_FEATURE_EDM
//--------------------------------------------------------------------------------------------------
/**
 * Session started array to keep track of sessions started on a specific server Id.
 * 0 for BS/AV server
 * 1 for any other server
 */
//--------------------------------------------------------------------------------------------------
static int SessionStarted[2] = {0, 0};
// Server ID used for Extended Device Management Server
#define EDM_SERVER_ID 1000
#else
static bool SessionStarted = false;
#endif // LE_CONFIG_AVC_FEATURE_EDM

//--------------------------------------------------------------------------------------------------
/**
 * Denoting if the device is in the authentication phase.
 * The authentication phase:
 *  - Starts when the authentication to BS or DM server starts.
 *  - Stops when the session to BS or DM server starts.
 */
//--------------------------------------------------------------------------------------------------
static bool AuthenticationPhase = false;

//--------------------------------------------------------------------------------------------------
/**
 * Authentication failure during bootstrapping
 */
//--------------------------------------------------------------------------------------------------
#if LE_CONFIG_LINUX
static bool BootstrapAuthFailed = false;
#endif

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
 * Store the Legato thread, since we might need to queue a function to this thread from the download
 * thread.
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t LegatoThread = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Used for reporting LE_AVC_NO_UPDATE if there has not been any activity between the device and
 * the server for a specific amount of time, after a session has been started.
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t ActivityTimerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Flag used to indicate a retry pending
 */
//--------------------------------------------------------------------------------------------------
static bool RetryPending = false;

//--------------------------------------------------------------------------------------------------
/**
 * Define static pool used to pass activity timer events to the main thread.
 */
//--------------------------------------------------------------------------------------------------
LE_MEM_DEFINE_STATIC_POOL(ActivityTimerEventsPool,
                          ACTIVITY_TIMER_EVENTS_POOL_SIZE,
                          sizeof(bool));

//--------------------------------------------------------------------------------------------------
/**
 * Pool used to pass activity timer events to the main thread
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t ActivityTimerEventsPool;

//--------------------------------------------------------------------------------------------------
/**
 * Server ID for the current session
 */
//--------------------------------------------------------------------------------------------------
static uint16_t ServerId = LE_AVC_SERVER_ID_AIRVANTAGE;

//--------------------------------------------------------------------------------------------------
// Local functions
//--------------------------------------------------------------------------------------------------

#if LE_CONFIG_AVC_FEATURE_EDM
//--------------------------------------------------------------------------------------------------
/**
 * Return the serverIdx used to identify session started flag for this specific server Id.
 */
//--------------------------------------------------------------------------------------------------
static int GetServerIdx
(
    uint16_t serverId
)
{
    if (serverId <= LE_AVC_SERVER_ID_AIRVANTAGE)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Reset session started flag for all server Id when session is disconnected.
 */
//--------------------------------------------------------------------------------------------------
static void ResetSessionStarted
(
    void
)
{
    int i = 0;
    for (i = 0; i < NUM_ARRAY_MEMBERS(SessionStarted); i++)
    {
        SessionStarted[i] = 0;
    }
}
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Convert an OMA FUMO (Firmware Update Management Object) error to an AVC error code.
 */
//--------------------------------------------------------------------------------------------------
static le_avc_ErrorCode_t ConvertFumoErrorCode
(
    uint32_t fumoError   ///< OMA FUMO error code.
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
 * Check if the date/time is valid and synchronize it if necessary.
 */
//--------------------------------------------------------------------------------------------------
static void CheckDateTimeValidity
(
    void
)
{
#if LE_CONFIG_LINUX

    char dateTimeBuf[DATE_TIME_LENGTH] = {0};
    uint32_t deviceYear;

    if (LE_OK != le_clk_GetUTCDateTimeString("%Y", dateTimeBuf, sizeof(dateTimeBuf), NULL))
    {
        LE_ERROR("Unable to retrieve current date/time");
        return;
    }

    deviceYear = (uint32_t) strtoul(dateTimeBuf, NULL, BASE10);

    // The date is considered as incorrect if the year is before 2017.
    if (deviceYear < MINIMAL_YEAR)
    {
        uint16_t year, month, day, hour, minute, second, millisecond;

        // Retrieve the date and time from a server.
        if (LE_OK != le_data_GetDateTime(&year, &month, &day,
                                         &hour, &minute, &second, &millisecond))
        {
            LE_ERROR("Unable to retrieve date or time from server");
            return;
        }

        // Set the date and time.
        memset(dateTimeBuf, 0, sizeof(dateTimeBuf));
        snprintf(dateTimeBuf, sizeof(dateTimeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
                 year, month, day, hour, minute, second);
        LE_DEBUG("Set date/time: %s", dateTimeBuf);

        if (LE_OK != le_clk_SetUTCDateTimeString("%Y-%m-%d %H:%M:%S", dateTimeBuf))
        {
            LE_ERROR("Unable to retrieve date or time from server");
            return;
        }
    }
#endif
}

//--------------------------------------------------------------------------------------------------
/**
 *  Callback registered in LwM2M client for bearer related events.
 */
//--------------------------------------------------------------------------------------------------
static void BearerEventCb
(
    bool connected,     ///< [IN] Indicates if the bearer is connected or disconnected.
    void* contextPtr    ///< [IN] User data.
)
{
    LE_INFO("Connected %d", connected);
    if (connected)
    {
        char endpointPtr[LWM2MCORE_ENDPOINT_LEN] = {0};
        size_t endpointLength = LWM2MCORE_ENDPOINT_LEN;

        // Register objects to LwM2M and set the device endpoint:
        // - Endpoint shall be unique for each client: IMEI/ESN/MEID.
        // - The number of objects we will be passing through and the objects array.

        // Get the device endpoint: IMEI.
        if (LWM2MCORE_ERR_COMPLETED_OK !=
                lwm2mcore_GetDeviceImei((char*)endpointPtr, &endpointLength))
        {
            LE_ERROR("Error to retrieve the device IMEI");
            return;
        }

        // Register to the LwM2M agent.
        if (!lwm2mcore_ObjectRegister(Lwm2mInstanceRef, endpointPtr, NULL, NULL))
        {
            LE_ERROR("ERROR in LwM2M obj reg");
            return;
        }

        if (!lwm2mcore_Connect(Lwm2mInstanceRef))
        {
            LE_ERROR("Connect error");
        }
    }
    else
    {
        if (NULL != Lwm2mInstanceRef)
        {
            // If the LWM2MCORE_TIMER_STEP timer is running, this means that a connection is active.
            if (lwm2mcore_TimerIsRunning(LWM2MCORE_TIMER_STEP))
            {
                avcClient_Disconnect(false);
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Callback registered in LwM2M client for bearer related TPF Server events.
 */
//--------------------------------------------------------------------------------------------------
static void TpfBearerEventCb
(
    bool connected,     ///< [IN] Indicates if the bearer is connected or disconnected.
    void* contextPtr    ///< [IN] User data.
)
{
    LE_INFO("Connected %d", connected);
    if (connected)
    {
        char endpointPtr[LWM2MCORE_ENDPOINT_LEN] = {0};
        size_t endpointLength = LWM2MCORE_ENDPOINT_LEN;
        uint16_t nbrObject = 0;
        // Register objects to LwM2M and set the device endpoint:
        // - Endpoint shall be unique for each client: IMEI/ESN/MEID.
        // - The number of objects we will be passing through and the objects array.

        // Get the device endpoint: IMEI.
        if (LWM2MCORE_ERR_COMPLETED_OK !=
                lwm2mcore_GetDeviceImei((char*)endpointPtr, &endpointLength))
        {
            LE_ERROR("Error to retrieve the device IMEI");
            return;
        }

        // Register to the LwM2M agent.
        nbrObject = lwm2mcore_ObjectRegister(Lwm2mInstanceRef, endpointPtr, NULL, NULL);
        if (!nbrObject)
        {
            LE_ERROR("ERROR in LwM2M obj reg");
            return;
        }
        //check if the FwupdateObj is registered
        if (nbrObject >= FW_UPDATE_OBJECT_ID)
        {
            LE_INFO("The FwUpdateObj is successfully registered, then write in ressource 5/0/1");
            //After all the LWM2M object are registerd , launch an Fwupdate
            lwm2mcore_Uri_t uri;

            char buffer[LE_TPF_URI_PACKAGE_MAX_SIZE] = { 0 };
            size_t bufferLen = LE_TPF_URI_PACKAGE_MAX_SIZE;
            le_tpf_GetPackageUri(buffer,bufferLen);
            bufferLen = strlen(buffer);
            LE_DEBUG("Package adress : %s",buffer);
            LE_DEBUG("URL length : %zd",bufferLen);

            // Check if TPF download should be resumed
            size_t offset = 0;
            if (LE_OK != le_fwupdate_GetResumePosition(&offset))
            {
                offset = 0;
            }

            if (offset)
            {
                LE_INFO("Resume TPF download at offset: %zu", offset);
                lwm2mcore_ResumePackageDownloader(LWM2MCORE_FW_UPDATE_TYPE);
                return;
            }

            // Define the URI
            uri.oid = FW_UPDATE_OBJECT_ID;   //object 5
            uri.oiid = FW_UPDATE_OBJECT_INSTANCE_ID;  // only one ressource
            uri.rid = FW_UPDATE_WRITE_RESOURCE_ID;
            uri.riid = FW_UPDATE_RESOURCE_INSTANCE_ID;
            uri.op = LWM2MCORE_OP_WRITE; // select write operation
            if(!lwm2mcore_ResourceWrite(uri.oid, uri.oiid, uri.rid, uri.riid, buffer, &bufferLen))
            {
                LE_ERROR("Failed to write in fwupdate object");
                return;
            }
        }

    }
    else
    {
        if (NULL != Lwm2mInstanceRef)
        {
            // If the LWM2MCORE_TIMER_STEP timer is running, this means that a connection is active.
            if (lwm2mcore_TimerIsRunning(LWM2MCORE_TIMER_STEP))
            {
                avcClient_Disconnect(false);
            }
        }
    }
}
//--------------------------------------------------------------------------------------------------
/**
 * Callback for the connection state.
 */
//--------------------------------------------------------------------------------------------------
static void ConnectionStateHandler
(
    const char* intfNamePtr,    ///< [IN] Interface name.
    bool connected,             ///< [IN] connection state (true = connected, else false).
    void* contextPtr            ///< [IN] User data.
)
{
    bool isEnabled = false;
    if (connected)
    {
        LE_DEBUG("Connected through interface '%s'", intfNamePtr);
        DataConnected = true;

        // Check if date/time is valid when connected.
        CheckDateTimeValidity();
        if ((LE_OK == tpfServer_GetTpfState(&isEnabled)) && (isEnabled))
        {
            LE_INFO("Third party FOTA is activated !");
            // Call the TPF callback.
            TpfBearerEventCb(connected, contextPtr);
        }
        else
        {
            // Call the callback.
            BearerEventCb(connected, contextPtr);
        }
    }
    else
    {
        LE_WARN("Disconnected from data connection service, current state %d", DataConnected);
        if (DataConnected)
        {
            DataConnected = false;
            if ((LE_OK == tpfServer_GetTpfState(&isEnabled)) && (isEnabled))
            {
                LE_INFO("Third party FOTA is activated !");
                // Call the TPF callback.
                TpfBearerEventCb(connected, contextPtr);
            }
            else
            {
                // Call the callback.
                BearerEventCb(connected, contextPtr);
            }
#if LE_CONFIG_AVC_FEATURE_EDM
            ResetSessionStarted();
#else
            SessionStarted = false;
#endif
            AuthenticationPhase = false;
        }
        else
        {
            // This can happen if initial connection made for sending notification to avms
            // fails after boot up
           LE_WARN("AVC: Disconnected even though we are not connected\n");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback for the LwM2M events linked to package download and update.
 *
 * @return
 *      - 0 on success.
 *      - negative value on failure.

 */
//--------------------------------------------------------------------------------------------------
static int PackageEventHandler
(
    lwm2mcore_Status_t status              ///< [IN] event status.
)
{
    int result = 0;

    LE_DEBUG("PackageEventHandler event %d", status.event);

    switch (status.event)
    {
        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_DETAILS:
            // Received a new download request: clear all query handler references which might be
            // left by previous aborted or stale SOTA/FOTA jobs.
            avcServer_ResetQueryHandlers();

            // Request user agreement before proceeding with download
            avcServer_QueryDownload(packageDownloader_StartDownload,
                                    status.u.pkgStatus.numBytes,
                                    status.u.pkgStatus.pkgType,
                                    false,
                                    LE_AVC_ERR_NONE);
            break;

        case LWM2MCORE_EVENT_DOWNLOAD_PROGRESS:
            if (LWM2MCORE_FW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_DOWNLOAD_IN_PROGRESS, LE_AVC_FIRMWARE_UPDATE,
                                       status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                       ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else if (LWM2MCORE_SW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_DOWNLOAD_IN_PROGRESS, LE_AVC_APPLICATION_UPDATE,
                                       status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                       ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FINISHED:
            // The download thread finished the file download without any error, but the FOTA
            // update package still might be rejected by the store thread, e.g. if the received
            // file is incomplete or contains any error.
            // The download complete event is therefore not sent now and will be sent only when
            // the store thread also exits without error.

            // For SOTA there is no store thread, but status will be sent by the same function
            // that deals with finalizing the download.
            LE_INFO("PackageDownloader finished. PackageType: %d", status.u.pkgStatus.pkgType);

            break;

        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FAILED:
            if (LWM2MCORE_FW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED, LE_AVC_FIRMWARE_UPDATE,
                                       status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                       ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else if (LWM2MCORE_SW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED, LE_AVC_APPLICATION_UPDATE,
                                       status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                       ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_UPDATE_STARTED:
            if (LWM2MCORE_FW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_INSTALL_IN_PROGRESS, LE_AVC_FIRMWARE_UPDATE,
                                       -1, 0, LE_AVC_ERR_NONE);
            }
            else if (LWM2MCORE_SW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_INSTALL_IN_PROGRESS, LE_AVC_APPLICATION_UPDATE,
                                       -1, 0, LE_AVC_ERR_NONE);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_UPDATE_FINISHED:
            if (LWM2MCORE_FW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_INSTALL_COMPLETE, LE_AVC_FIRMWARE_UPDATE,
                                       -1, -1, LE_AVC_ERR_NONE);
            }
            else if (LWM2MCORE_SW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_INSTALL_COMPLETE, LE_AVC_APPLICATION_UPDATE,
                                       -1, -1, LE_AVC_ERR_NONE);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_UPDATE_FAILED:
            if (LWM2MCORE_FW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_INSTALL_FAILED, LE_AVC_FIRMWARE_UPDATE,
                                       -1, -1, ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else if (LWM2MCORE_SW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_INSTALL_FAILED, LE_AVC_APPLICATION_UPDATE,
                                       -1, -1, ConvertFumoErrorCode(status.u.pkgStatus.errorCode));
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_CERTIFICATION_OK:
            if (LWM2MCORE_FW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_CERTIFICATION_OK, LE_AVC_FIRMWARE_UPDATE,
                                       -1, -1, LE_AVC_ERR_NONE);
            }
            else if (LWM2MCORE_SW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_CERTIFICATION_OK, LE_AVC_APPLICATION_UPDATE,
                                       -1, -1, LE_AVC_ERR_NONE);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_CERTIFICATION_NOT_OK:
            if (LWM2MCORE_FW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_CERTIFICATION_KO, LE_AVC_FIRMWARE_UPDATE,
                                       -1, -1, LE_AVC_ERR_BAD_PACKAGE);
            }
            else if (LWM2MCORE_SW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateStatus(LE_AVC_CERTIFICATION_KO, LE_AVC_APPLICATION_UPDATE,
                                       -1, -1, LE_AVC_ERR_BAD_PACKAGE);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_SIZE_ERROR:
            LE_DEBUG("error code %"PRIu32, status.u.pkgStatus.errorCode);
            switch(status.u.pkgStatus.errorCode)
            {
                case DWL_MEM_ERROR:
                    avcServer_QueryDownload(packageDownloader_StartDownload,
                                            INT64_MAX,
                                            status.u.pkgStatus.pkgType,
                                            false,
                                            LE_AVC_ERR_RAM);
                    break;
                case DWL_NETWORK_ERROR:
                    avcServer_QueryDownload(packageDownloader_StartDownload,
                                            INT64_MAX,
                                            status.u.pkgStatus.pkgType,
                                            false,
                                            LE_AVC_ERR_NETWORK);
                    break;
                case DWL_BAD_ADDR:
                    if (LWM2MCORE_FW_UPDATE_TYPE == status.u.pkgStatus.pkgType)
                    {
                        avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED,
                                               LE_AVC_FIRMWARE_UPDATE,
                                               -1,
                                               -1,
                                               LE_AVC_ERR_BAD_PACKAGE);
                    }
                    else
                    {
                        avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED,
                                               LE_AVC_APPLICATION_UPDATE,
                                               -1,
                                               -1,
                                               LE_AVC_ERR_BAD_PACKAGE);
                    }
                    break;

                default:
                    break;
            }
            break;

        default:
            if (LWM2MCORE_EVENT_LAST <= status.event)
            {
                LE_ERROR("Unsupported event %d", status.event);
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
    LE_DEBUG("Stop retry timer");
    RetryTimersIndex = -1;
    memset(RetryTimers, 0, sizeof(RetryTimers));
    le_timer_Stop(RetryTimerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop the bearer - undo what StartBearer does.
 */
//--------------------------------------------------------------------------------------------------
static void StopBearer
(
    void
)
{
    LE_INFO("Stop bearer %p", DataRef);
    if (DataRef)
    {
        // Close the data connection.
        le_data_Release(DataRef);

        // Remove the data handler.
        le_data_RemoveConnectionStateHandler(DataHandler);
        DataRef = NULL;
        DataHandler = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback for the LwM2M events.
 *
 * @return
 *      - 0 on success.
 *      - negative value on failure.

 */
//--------------------------------------------------------------------------------------------------
static int EventHandler
(
    lwm2mcore_Status_t status              ///< [IN] event status.
)
{
    int result = 0;

    switch (status.event)
    {
        case LWM2MCORE_EVENT_SESSION_STARTED:
            LE_DEBUG("Session start");
            break;

        case LWM2MCORE_EVENT_SESSION_FAILED:
            LE_ERROR("Session failure");
            // If the device is connected to the bootstrap server, disconnect from server.
            // If the device is connected to the DM server, a bootstrap connection will be
            // automatically initiated (session is not stopped).
            if (LE_AVC_BOOTSTRAP_SESSION == le_avc_GetSessionType())
            {
                avcServer_UpdateStatus(LE_AVC_SESSION_FAILED, LE_AVC_UNKNOWN_UPDATE,
                                       -1, -1, LE_AVC_ERR_NONE);
                LE_ERROR("Session failure on bootstrap server");
                le_event_Report(BsFailureEventId, NULL, 0);
            }
#if LE_CONFIG_AVC_FEATURE_EDM
            ResetSessionStarted();
#else
            SessionStarted = false;
#endif
            break;

        case LWM2MCORE_EVENT_SESSION_FINISHED:
            // If an AVC session retry is ongoing, do not report SESSION_STOPPED
            if (!RetryPending)
            {
                LE_DEBUG("Session finished");
                avcServer_UpdateStatus(LE_AVC_SESSION_STOPPED, LE_AVC_UNKNOWN_UPDATE,
                                       -1, -1, LE_AVC_ERR_NONE);
#if LE_CONFIG_AVC_FEATURE_EDM
                if (!(SessionStarted[GetServerIdx(ServerId)]))
#else
                if (false == SessionStarted)
#endif
                {
                    // In this case, check if a package download is not pending
                    lwm2mcore_UpdateType_t type;
                    uint64_t packageSize = 0;
                    lwm2mcore_Sid_t sID = lwm2mcore_GetDownloadInfo(&type, &packageSize);
                    if ((LWM2MCORE_ERR_COMPLETED_OK == sID) && packageSize)
                    {
                        // A package download pending notification needs to be sent
                        le_avc_UpdateType_t updateType = LE_AVC_UNKNOWN_UPDATE;
                        switch (type)
                        {
                            case LWM2MCORE_FW_UPDATE_TYPE:
                                updateType = LE_AVC_FIRMWARE_UPDATE;
                                break;

                            case LWM2MCORE_SW_UPDATE_TYPE:
                                updateType = LE_AVC_APPLICATION_UPDATE;
                                break;

                            default:
                                LE_ERROR("Incorrect update type %d", type);
                                break;
                        }

                        if (LE_AVC_UNKNOWN_UPDATE != updateType)
                        {
                            uint64_t numBytesToDownload = 0;
                            if (LE_OK != packageDownloader_BytesLeftToDownload(&numBytesToDownload))
                            {
                                LE_ERROR("Issue to get remaining bytes to be downloaded");
                                numBytesToDownload = packageSize;
                            }
                            avcServer_UpdateStatus(LE_AVC_DOWNLOAD_PENDING,
                                                   updateType,
                                                   numBytesToDownload,
                                                   -1,
                                                   LE_AVC_ERR_INTERNAL);
                        }
                    }
                }

                if (Lwm2mInstanceRef)
                {
                    lwm2mcore_Free(Lwm2mInstanceRef);
                    Lwm2mInstanceRef = NULL;
                }
                StopBearer();
            }
#if LE_CONFIG_AVC_FEATURE_EDM
            ResetSessionStarted();
#else
            SessionStarted = false;
#endif
            AuthenticationPhase = false;
            break;

        case LWM2MCORE_EVENT_LWM2M_SESSION_TYPE_START:
            if (status.u.session.type == LWM2MCORE_SESSION_BOOTSTRAP)
            {
                LE_DEBUG("Connected to bootstrap");
                avcServer_UpdateStatus(LE_AVC_SESSION_BS_STARTED, LE_AVC_UNKNOWN_UPDATE,
                                       -1, -1, LE_AVC_ERR_NONE);
            }
            else if (!avcServer_IsDownloadInProgress())
            {
                LE_DEBUG("Connected to DM");
#if LE_CONFIG_AVC_FEATURE_EDM
                if (!(SessionStarted[GetServerIdx(ServerId)]))
#else
                if (!SessionStarted)
#endif
                {
                    avcServer_UpdateStatus(LE_AVC_SESSION_STARTED, LE_AVC_UNKNOWN_UPDATE,
                                           -1, -1, LE_AVC_ERR_NONE);
#if LE_CONFIG_AVC_FEATURE_EDM
                    SessionStarted[GetServerIdx(ServerId)] = 1;
#else
                    SessionStarted = true;
#endif
                }
            }
            else
            {
                LE_WARN("FOTA Download is in progress, ignore duplicate start session request");
            }
            ResetRetryTimers();
            AuthenticationPhase = false;
            break;

        case LWM2MCORE_EVENT_LWM2M_SESSION_INACTIVE:
            // There is no activity in CoAP layer at this point.
            // If the session is not initiated by user and avc service is in idle i.e.,
            // no SOTA or FOTA operation in progress then tear down the session.
            if (avcServer_IsIdle() && !avcServer_IsUserSession() && !AuthenticationPhase)
            {
                LE_DEBUG("Disconnecting polling timer initiated session");
                avcClient_Disconnect(true);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_DETAILS:
        case LWM2MCORE_EVENT_DOWNLOAD_PROGRESS:
        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FINISHED:
        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FAILED:
        case LWM2MCORE_EVENT_UPDATE_STARTED:
        case LWM2MCORE_EVENT_UPDATE_FINISHED:
        case LWM2MCORE_EVENT_UPDATE_FAILED:
        case LWM2MCORE_EVENT_PACKAGE_CERTIFICATION_OK:
        case LWM2MCORE_EVENT_PACKAGE_CERTIFICATION_NOT_OK:
        case LWM2MCORE_EVENT_PACKAGE_SIZE_ERROR:
            result = PackageEventHandler(status);
            break;

        case LWM2MCORE_EVENT_AUTHENTICATION_STARTED:
#if LE_CONFIG_LINUX
            BootstrapAuthFailed = false;
#endif
            if (status.u.session.type == LWM2MCORE_SESSION_BOOTSTRAP)
            {
                LE_DEBUG("Authentication to BS started");
            }
            else
            {
                LE_DEBUG("Authentication to DM started");
            }
            AuthenticationPhase = true;
#if LE_CONFIG_AVC_FEATURE_EDM
            if (!(SessionStarted[GetServerIdx(ServerId)]))
#else
            if (!SessionStarted)
#endif
            {
                avcServer_UpdateStatus(LE_AVC_AUTH_STARTED, LE_AVC_UNKNOWN_UPDATE,
                                       -1, -1, LE_AVC_ERR_NONE);
            }
            break;

        case LWM2MCORE_EVENT_AUTHENTICATION_FAILED:
            if (status.u.session.type == LWM2MCORE_SESSION_BOOTSTRAP)
            {
                LE_WARN("Authentication to BS failed");
#if LE_CONFIG_LINUX
                BootstrapAuthFailed = true;
#endif
            }
            else
            {
                LE_WARN("Authentication to DM failed");
            }
            AuthenticationPhase = false;
            avcServer_UpdateStatus(LE_AVC_AUTH_FAILED, LE_AVC_UNKNOWN_UPDATE,
                                   -1, -1, LE_AVC_ERR_NONE);
            break;
        case LWM2MCORE_EVENT_REG_UPDATE_DONE:
            if (avcServer_SaveCurrentEpochTime() != LE_OK)
            {
                LE_ERROR("Failed to save the current time in AVC config");
            }
            break;

        default:
            if (LWM2MCORE_EVENT_LAST <= status.event)
            {
                LE_ERROR("Unsupported event %d", status.event);
                result = -1;
            }
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start the bearer.
 */
//--------------------------------------------------------------------------------------------------
static void StartBearer
(
    void
)
{
    // Attempt to connect.
    Lwm2mInstanceRef = lwm2mcore_Init(EventHandler);

#if LE_CONFIG_AVC_FEATURE_EDM
    lwm2mcore_SetEdmEnabled(Lwm2mInstanceRef, true);
#endif
    lwm2mcore_SetServer(Lwm2mInstanceRef, ServerId);

    LE_INFO("Start Bearer");
    // Initialize the bearer and open a data connection.
    le_data_ConnectService();

    DataHandler = le_data_AddConnectionStateHandler(ConnectionStateHandler, NULL);
    // Request data connection.
    DataRef = le_data_Request();
    LE_ASSERT(NULL != DataRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for ActivityTimerRef expiry.
 */
//--------------------------------------------------------------------------------------------------
static void ActivityTimerHandler
(
    le_timer_Ref_t timerRef    ///< This timer has expired.
)
{
    LE_DEBUG("Activity timer expired; reporting LE_AVC_NO_UPDATE");
    avcServer_UpdateStatus(LE_AVC_NO_UPDATE,
                           LE_AVC_UNKNOWN_UPDATE,
                           -1,
                           -1,
                           LE_AVC_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function queued onto Legato Thread to toggle the activity timer.
 */
//--------------------------------------------------------------------------------------------------
static void ToggleActivityTimerHandler
(
    void* param1Ptr,    ///< [IN] User data that holds activity timer flag.
    void* param2Ptr     ///< [IN] Unused user data.
)
{
    LE_DEBUG("Toggling Activity timer");
    bool toggleFlag = *((bool*)param1Ptr);

    if (!toggleFlag)
    {
        LE_DEBUG("Trying to stop activity timer");
        if (le_timer_IsRunning(ActivityTimerRef))
        {
            LE_DEBUG("Stopping Activity timer");
            le_timer_Stop(ActivityTimerRef);
        }
    }
    else
    {
        LE_DEBUG("Starting activity timer");
        le_timer_Start(ActivityTimerRef);
    }

    le_mem_Release(param1Ptr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler to terminate a connection to bootstrap on failure.
 */
//--------------------------------------------------------------------------------------------------
static void BsFailureHandler
(
    void* reportPtr    ///< [IN] Pointer to the event report payload
)
{
#if LE_CONFIG_LINUX
    // Restore bootstrap.
    LE_DEBUG("Fix BS credentials: %d", BootstrapAuthFailed);
    avcClient_FixBootstrapCredentials(BootstrapAuthFailed);
    BootstrapAuthFailed = false; // Reset flag
#endif
    avcClient_Disconnect(true);
}

//--------------------------------------------------------------------------------------------------
/**
 * Timer handler to periodically perform a connection attempt.
 */
//--------------------------------------------------------------------------------------------------
static void avcClient_RetryTimer
(
    le_timer_Ref_t timerRef    ///< [IN] Expired timer reference
)
{
    le_result_t rc = avcClient_Connect(ServerId);
    if (rc != LE_OK)
    {
        LE_ERROR("Unable to request a connection to the server");
    }
}

//--------------------------------------------------------------------------------------------------
// Public functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Starts a periodic connection attempt to the AirVantage or other DM server.
 *
 * @note - After a user-initiated call, this function registers itself inside a timer expiry handler
 *         to perform retries. On connection success, this function deinitializes the timer.
 *       - If this function is called when another connection is in the middle of being initiated
 *         or when the device is authenticating then LE_BUSY will be returned.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_BUSY if currently retrying or authenticating.
 *      - LE_DUPLICATE if already connected to AirVantage server.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Connect
(
    uint16_t    serverId    ///< [IN] Server ID. Can be LE_AVC_SERVER_ID_ALL_SERVERS.
)
{
    // Check if a session is already started.
#if LE_CONFIG_AVC_FEATURE_EDM
    if (SessionStarted[GetServerIdx(serverId)])
#else
    if (SessionStarted)
#endif
    {
        // No need to start a retry timer. Perform reset/cleanup.
        ResetRetryTimers();

        LE_INFO("Session already started");
        return LE_DUPLICATE;
    }

    // Check if a retry is in progress.
    if (le_timer_IsRunning(RetryTimerRef))
    {
        LE_INFO("Retry timer already running");
        return LE_BUSY;
    }

    // Check if the device is currently authenticating.
    if (AuthenticationPhase)
    {
        LE_INFO("Authentication is ongoing");
        return LE_BUSY;
    }

    // Set the Server ID global (assuming only one session can happen at a time)
    ServerId = serverId;

    // If Lwm2mInstanceRef exists, then that means the current call is a "retry", which is
    // performed by stopping the previous data connection first.
    if (NULL != Lwm2mInstanceRef)
    {
        // Disconnect LwM2M session
        if (true == lwm2mcore_TimerIsRunning(LWM2MCORE_TIMER_STEP))
        {
            __attribute__((unused)) bool result = 0;

            RetryPending = true;
            result = lwm2mcore_DisconnectWithDeregister(Lwm2mInstanceRef);
            LE_DEBUG("lwm2mcore_DisconnectWithDeregister %d", result);
            RetryPending = false;
        }

        StopBearer();
    }

    StartBearer();

    // Attempt to start a retry timer.
    // if index is less than 0, then get the retry timers config. The implication is that while
    // a retry timer is running, changes to retry timers aren't applied. They are applied when
    // retry timers are being reset.
    if (0 > RetryTimersIndex)
    {
        size_t numTimers = NUM_ARRAY_MEMBERS(RetryTimers);

        if (LE_OK != le_avc_GetRetryTimers(RetryTimers, &numTimers))
        {
            LE_WARN("Failed to retrieve retry timers config. Failed session start is not retried.");
            return LE_OK;
        }

        LE_ASSERT(LE_AVC_NUM_RETRY_TIMERS == numTimers);

        RetryTimersIndex = 0;
    }
    else
    {
        RetryTimersIndex++;
    }

    // Get the next valid retry timer.
    // See which timer we are at by looking at RetryTimersIndex:
    // - if the timer is 0, get the next one. (0 means disabled / not used)
    // - if we run out of timers, do nothing. Perform reset/cleanup.
    while (   (RetryTimersIndex < LE_AVC_NUM_RETRY_TIMERS)
           && (0 == RetryTimers[RetryTimersIndex]))
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

        LE_ASSERT_OK(le_timer_SetInterval(RetryTimerRef, interval));
        LE_ASSERT_OK(le_timer_SetHandler(RetryTimerRef, avcClient_RetryTimer));
        LE_ASSERT_OK(le_timer_Start(RetryTimerRef));
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Network registration state handler
 */
//--------------------------------------------------------------------------------------------------
static void NetRegHandler
(
    le_mrc_NetRegState_t state,         ///< [IN] MRC state
    void*                contextPtr     ///< [IN] Context
)
{
    if ((LE_MRC_REG_HOME == state) || (LE_MRC_REG_ROAMING == state))
    {
        avcServer_InitPollingTimer();
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * LwM2M client entry point to close a connection.
 *
 * @return
 *      - LE_OK in case of success.
 *      - LE_FAULT in case of failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Disconnect
(
    bool resetRetry  ///< [IN] if true, reset the retry timers.
)
{
    LE_DEBUG("Disconnect");

    le_result_t result = LE_OK;

    avcServer_ResetDownloadAgreement();

#ifdef LE_CONFIG_AVC_FEATURE_EDM
    // Do not send DEREGISTER message for EDM server
    lwm2mcore_SkipDeregister(Lwm2mInstanceRef, EDM_SERVER_ID);
#endif

    // If the LWM2MCORE_TIMER_STEP timer is running, this means that a connection is active.
    // In that case, attempt to disconnect.
    if (lwm2mcore_TimerIsRunning(LWM2MCORE_TIMER_STEP))
    {
        if (DataConnected)
        {
            result = (lwm2mcore_DisconnectWithDeregister(Lwm2mInstanceRef)) ? LE_OK : LE_FAULT;
        }
        else
        {
            result = (lwm2mcore_Disconnect(Lwm2mInstanceRef)) ? LE_OK : LE_FAULT;
        }
    }
    else
    {
        // Although AVC session may not be established at this moment, but the retry mechanism of
        // DCS may establish data connection, hence AVC session after the request to stop AVC
        // session. So cancel the data connection request here.
        StopBearer();
        result = LE_DUPLICATE;
    }

    if (resetRetry)
    {
        ResetRetryTimers();
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check the session started flag for a given server Id.
 *
 * @return
 *      - true if session is started
 *      - false otherwise
 */
//--------------------------------------------------------------------------------------------------
bool avcClient_IsSessionStarted
(
    uint16_t serverId
)
{
#if LE_CONFIG_AVC_FEATURE_EDM
    return SessionStarted[GetServerIdx(serverId)];
#else
    LE_UNUSED(serverId);
    return SessionStarted;
#endif
}


//--------------------------------------------------------------------------------------------------
/**
 * LwM2M client entry point to send a registration update.
 *
 * @return
 *      - LE_OK in case of success.
 *      - LE_UNAVAILABLE when session closed.
 *      - LE_FAULT in case of failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Update
(
    void
)
{
    bool isEnabled = false;
    LE_DEBUG("Registration update");

    if (NULL == Lwm2mInstanceRef)
    {
        LE_DEBUG("Session closed");
        return LE_UNAVAILABLE;
    }

    if ((LE_OK == tpfServer_GetTpfState(&isEnabled)) && (isEnabled))
    {
        return LE_OK ;
    }

    else
    {
        if (lwm2mcore_Update(Lwm2mInstanceRef))
        {
            return LE_OK;
        }
        else
        {
            return LE_FAULT;
        }
    }
 }

//--------------------------------------------------------------------------------------------------
/**
 * LwM2M client entry point to exucute fwupdate.
 *
 * @return
 *      - LE_OK in case of success.
 *      - LE_FAULT in case of failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_LaunchFwUpdate
(
    void
)
{

    LE_INFO("The FwUpdateObj is successfully registered, then write in ressource 5/0/2");
    //After all the LWM2M object are registerd , launch an Fwupdate
    lwm2mcore_Uri_t uri;
    size_t bufferLen = 0;

    // Define the URI
    uri.oid = FW_UPDATE_OBJECT_ID;   //Object 5
    uri.oiid = FW_UPDATE_OBJECT_INSTANCE_ID;  //Only one ressource
    uri.rid = FW_UPDATE_EXEC_RESOURCE_ID; //Select resource ID "2"
    uri.riid = FW_UPDATE_RESOURCE_INSTANCE_ID; //Only one ressource
    if(!lwm2mcore_ResourceExec(uri.oid, uri.oiid, uri.rid, uri.riid, NULL, &bufferLen))
    {
        LE_INFO("lwm2mcore_ResourceExec failed !!");
        return LE_FAULT;
    }
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * LwM2M client entry point to push data.
 *
 * @return
 *      - LE_OK in case of success.
 *      - LE_BUSY if busy pushing data.
 *      - LE_FAULT in case of failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Push
(
    uint8_t* payload,                       ///< [IN] Payload to push.
    size_t payloadLength,                   ///< [IN] Payload length.
    lwm2mcore_PushContent_t contentType,    ///< [IN] Content type.
    uint16_t* midPtr                        ///< [OUT] Message identifier.
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
 * Notify LwM2M of supported object instance list for software and asset data.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_SendList
(
    char* lwm2mObjListPtr,      ///< [IN] Object instances list.
    size_t objListLen           ///< [IN] List length.
)
{
    lwm2mcore_UpdateSwList(Lwm2mInstanceRef, lwm2mObjListPtr, objListLen);
}

//--------------------------------------------------------------------------------------------------
/**
 * Returns the instance reference of this client.
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
 * LwM2M client entry point to get session status.
 *
 * @return
 *      - LE_AVC_DM_SESSION when the device is connected to the DM server.
 *      - LE_AVC_BOOTSTRAP_SESSION when the device is connected to the BS server.
 *      - LE_AVC_SESSION_INVALID in other cases.
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
 * This function sets up the activity timer.
 *
 * @note The timeout will default to DEFAULT_ACTIVITY_TIMER if user defined value is less or equal
 * to 0.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_SetActivityTimeout
(
    int timeout     ///< [IN] Timeout for activity timer.
)
{
    // After a session is started, if there has been no activity within the timer
    // interval, then report LE_AVC_NO_UPDATE.
    le_clk_Time_t timerInterval = { .sec=DEFAULT_ACTIVITY_TIMER, .usec=0 };

    if (timeout > 0)
    {
        timerInterval.sec = timeout;
    }

    LE_DEBUG("Activity timeout set to %d seconds", (int)timerInterval.sec);

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
    if (NULL != LegatoThread)
    {
        bool* activityTimerFlag = le_mem_ForceAlloc(ActivityTimerEventsPool);
        *activityTimerFlag = true;
        le_event_QueueFunctionToThread(LegatoThread,
                                       ToggleActivityTimerHandler,
                                       activityTimerFlag,
                                       NULL);
    }
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
    if (NULL != LegatoThread)
    {
        bool* activityTimerFlag = le_mem_ForceAlloc(ActivityTimerEventsPool);
        *activityTimerFlag = false;
        le_event_QueueFunctionToThread(LegatoThread,
                                       ToggleActivityTimerHandler,
                                       activityTimerFlag,
                                       NULL);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Checks whether retry timer is active
 */
//--------------------------------------------------------------------------------------------------
bool avcClient_IsRetryTimerActive
(
    void
)
{
    bool isRetryTimerRunning = false;

    if (NULL != RetryTimerRef)
    {
        isRetryTimerRunning = le_timer_IsRunning(RetryTimerRef);
    }

    return isRetryTimerRunning;
}

//--------------------------------------------------------------------------------------------------
/**
 * Reset the retry timers by resetting the retrieved reset timer config and stopping the current
 * retry timer.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_ResetRetryTimer
(
    void
)
{
    ResetRetryTimers();
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the AVC client sub-component.
 *
 * @note This function should be called during the initializaion phase of the AVC daemon.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_Init
(
   void
)
{
    // Create event for bootstrap connection failure.
    BsFailureEventId = le_event_CreateId("BsFailure", 0);
    le_event_AddHandler("BsFailureHandler", BsFailureEventId, BsFailureHandler);

    // Create retry timer for avcClient connection.
    RetryTimerRef = le_timer_Create("AvcRetryTimer");

    // Store the calling thread reference.
    LegatoThread = le_thread_GetCurrent();

    // Create pool to report activity timer events.
    ActivityTimerEventsPool = le_mem_InitStaticPool(ActivityTimerEventsPool,
                                                    ACTIVITY_TIMER_EVENTS_POOL_SIZE,
                                                    sizeof(bool));

    // Register for network Service state changes
    le_mrc_AddNetRegStateEventHandler(NetRegHandler, NULL);

    avcClient_UpdateInit();
    avcClient_DeviceInit();

    lwm2mcore_InitMem();

    if(false == lwm2mcore_SetEventHandler(EventHandler))
    {
        LE_ERROR("Can not subscribe to LwM2MCore events");
    }
}
