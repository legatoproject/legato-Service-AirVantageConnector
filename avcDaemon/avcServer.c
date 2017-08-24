/**
 * @file avcServer.c
 *
 * AirVantage Controller Daemon
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/lwm2mcore.h>
#include <avcClient.h>
#include <lwm2mcore/security.h>
#include "legato.h"
#include "interfaces.h"
#include "pa_avc.h"
#include "avcServer.h"
#include "avData.h"
#include "push.h"
#include "fsSys.h"
#include "le_print.h"
#include "avcAppUpdate.h"
#include "packageDownloader.h"
#include "packageDownloaderCallbacks.h"
#include "avcFsConfig.h"

//--------------------------------------------------------------------------------------------------
// Definitions
//--------------------------------------------------------------------------------------------------
#define AVC_SERVICE_CFG "/apps/avcService"

//--------------------------------------------------------------------------------------------------
/**
 * This ref is returned when a session request handler is added/registered.  It is used when the
 * handler is removed.  Only one ref is needed, because only one handler can be registered at a
 * time.
 */
//--------------------------------------------------------------------------------------------------
#define REGISTERED_SESSION_HANDLER_REF ((le_avc_SessionRequestEventHandlerRef_t)0xABCD)

//--------------------------------------------------------------------------------------------------
/**
 * This is the default defer time (in minutes) if an install is blocked by a user app.  Should
 * probably be a prime number.
 *
 * Use small number to ensure deferred installs happen quickly, once no longer deferred.
 */
//--------------------------------------------------------------------------------------------------
#define BLOCKED_DEFER_TIME 3

//--------------------------------------------------------------------------------------------------
/**
 *  Max number of bytes of a retry timer name.
 */
//--------------------------------------------------------------------------------------------------
#define RETRY_TIMER_NAME_BYTES 10

//--------------------------------------------------------------------------------------------------
/**
 * Number of seconds in a minute
 */
//--------------------------------------------------------------------------------------------------
#define SECONDS_IN_A_MIN 60

//--------------------------------------------------------------------------------------------------
/**
 * Default setting for user agreement
 *
 * NOTE: User agreement is enabled by default, if not configured
 */
//--------------------------------------------------------------------------------------------------
#define USER_AGREEMENT_DEFAULT  1

//--------------------------------------------------------------------------------------------------
/**
 * Value if polling timer is disabled
 */
//--------------------------------------------------------------------------------------------------
#define POLLING_TIMER_DISABLED  0

//--------------------------------------------------------------------------------------------------
/**
 * Current internal state.
 *
 * Used mainly to ensure that API functions don't do anything if in the wrong state.
 *
 * TODO: May need to revisit some of the state transitions here.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    AVC_IDLE,                    ///< No updates pending or in progress
    AVC_DOWNLOAD_PENDING,        ///< Received pending download; no response sent yet
    AVC_DOWNLOAD_IN_PROGRESS,    ///< Accepted download, and in progress
    AVC_INSTALL_PENDING,         ///< Received pending install; no response sent yet
    AVC_INSTALL_IN_PROGRESS,     ///< Accepted install, and in progress
    AVC_UNINSTALL_PENDING,       ///< Received pending uninstall; no response sent yet
    AVC_UNINSTALL_IN_PROGRESS    ///< Accepted uninstall, and in progress
}
AvcState_t;

//--------------------------------------------------------------------------------------------------
/**
 * Package download context
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint32_t bytesToDownload;               ///< Package size.
}
PkgDownloadContext_t;

//--------------------------------------------------------------------------------------------------
/**
 * Package install context
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    lwm2mcore_UpdateType_t  type;   ///< Update type.
    uint16_t instanceId;            ///< Instance Id (0 for FW, any value for SW)
}
PkgInstallContext_t;

//--------------------------------------------------------------------------------------------------
/**
 * SW uninstall context
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint16_t instanceId;            ///< Instance Id (0 for FW, any value for SW)
}
SwUninstallContext_t;

//--------------------------------------------------------------------------------------------------
/**
 * Data associated with the UpdateStatusEvent
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_avc_Status_t updateStatus;   ///< Update status
    int32_t totalNumBytes;          ///< Total number of bytes to download
    int32_t downloadProgress;       ///< Download Progress in %
    void* contextPtr;               ///< Context
}
UpdateStatusData_t;

//--------------------------------------------------------------------------------------------------
/**
 * Data associated with user agreement configuration
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    bool connect;                   ///< is auto connect?
    bool download;                  ///< is auto download?
    bool install;                   ///< is auto install?
    bool uninstall;                 ///< is auto uninstall?
    bool reboot;                    ///< is auto reboot?
}
UserAgreementConfig_t;

//--------------------------------------------------------------------------------------------------
/**
 * Data associated with apn configuration
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char apnName[LE_AVC_APN_NAME_MAX_LEN_BYTES];        ///< APN name
    char userName[LE_AVC_USERNAME_MAX_LEN_BYTES];       ///< User name
    char password[LE_AVC_PASSWORD_MAX_LEN_BYTES];       ///< Password
}
ApnConfig_t;

//--------------------------------------------------------------------------------------------------
/**
 * Data associated with avc configuration
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint16_t retryTimers[LE_AVC_NUM_RETRY_TIMERS];      ///< Retry timer configuration
    UserAgreementConfig_t ua;                           ///< User agreement configuration
    ApnConfig_t apn;                                    ///< APN configuration
    int32_t connectionEpochTime;                        ///< UNIX time when the last connection was
                                                        ///< made by the polling timer
}
AvcConfigData_t;

//--------------------------------------------------------------------------------------------------
// Data structures
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * The current state of any update.
 *
 * Although this variable is accessed both in API functions and in UpdateHandler(), access locks
 * are not needed.  This is because this is running as a daemon, and so everything runs in the
 * main thread.
 */
//--------------------------------------------------------------------------------------------------
static AvcState_t CurrentState = AVC_IDLE;

//--------------------------------------------------------------------------------------------------
/**
 * Event for sending update status notification to applications
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t UpdateStatusEvent;

//--------------------------------------------------------------------------------------------------
/**
 * Current download progress in percentage.
 */
//--------------------------------------------------------------------------------------------------
static int32_t CurrentDownloadProgress = -1;

//--------------------------------------------------------------------------------------------------
/**
 * Total number of bytes to download.
 */
//--------------------------------------------------------------------------------------------------
static int32_t CurrentTotalNumBytes = -1;

//--------------------------------------------------------------------------------------------------
/**
 * Download package agreement done
 */
//--------------------------------------------------------------------------------------------------
static bool DownloadAgreement = false;

//--------------------------------------------------------------------------------------------------
/**
 * The type of the current update.  Only valid if CurrentState is not AVC_IDLE
 */
//--------------------------------------------------------------------------------------------------
static le_avc_UpdateType_t CurrentUpdateType = LE_AVC_UNKNOWN_UPDATE;

//--------------------------------------------------------------------------------------------------
/**
 * Handler registered by control app to receive session open or close requests.
 */
//--------------------------------------------------------------------------------------------------
static le_avc_SessionRequestHandlerFunc_t SessionRequestHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Context pointer associated with the above user registered handler to receive session open or
 * close requests.
 */
//--------------------------------------------------------------------------------------------------
static void* SessionRequestHandlerContextPtr = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Number of registered status handlers
 */
//--------------------------------------------------------------------------------------------------
static uint32_t NumStatusHandlers = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Context pointer associated with the above user registered handler to receive status updates.
 */
//--------------------------------------------------------------------------------------------------
static void* StatusHandlerContextPtr = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Safe Reference Map for the block/unblock references
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t BlockRefMap;

//--------------------------------------------------------------------------------------------------
/**
 * Count of the number of allocated safe references from 'BlockRefMap' above.
 *
 * If there was a safeRef wrapper around le_hashmap_Size(), then this value would probably not be
 * needed, although we would then be dependent on the implementation of le_hashmap_Size() not
 * being overly complex.
 */
//--------------------------------------------------------------------------------------------------
static uint32_t BlockRefCount=0;

//--------------------------------------------------------------------------------------------------
/**
 * Handler registered from avcServer_QueryInstall() to receive notification when app install is
 * allowed. Only one registered handler is allowed, and will be set to NULL after being called.
 */
//--------------------------------------------------------------------------------------------------
static avcServer_InstallHandlerFunc_t QueryInstallHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Handler registered from avcServer_QueryDownload() to receive notification when app download is
 * allowed. Only one registered handler is allowed, and will be set to NULL after being called.
 */
//--------------------------------------------------------------------------------------------------
static avcServer_DownloadHandlerFunc_t QueryDownloadHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Handler registered from avcServer_QueryUninstall() to receive notification when app uninstall is
 * allowed. Only one registered handler is allowed, and will be set to NULL after being called.
 */
//--------------------------------------------------------------------------------------------------
static avcServer_UninstallHandlerFunc_t QueryUninstallHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Handler registered from avcServer_QueryReboot() to receive notification when device reboot is
 * allowed. Only one registered handler is allowed, and will be set to NULL after being called.
 */
//--------------------------------------------------------------------------------------------------
static avcServer_RebootHandlerFunc_t QueryRebootHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Timer used for deferring app install.
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t InstallDeferTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Timer used for deferring app download.
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t DownloadDeferTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Timer used for deferring app uninstall.
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t UninstallDeferTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Timer used for deferring device reboot.
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t RebootDeferTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Timer used for deferring Connection.
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t ConnectDeferTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Launch connect timer
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t LaunchConnectTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Error occurred during update via airvantage.
 */
//--------------------------------------------------------------------------------------------------
static le_avc_ErrorCode_t AvcErrorCode = LE_AVC_ERR_NONE;


//--------------------------------------------------------------------------------------------------
/**
 * Current package download context
 */
//--------------------------------------------------------------------------------------------------
static PkgDownloadContext_t PkgDownloadCtx;

//--------------------------------------------------------------------------------------------------
/**
 * Current package install context
 */
//--------------------------------------------------------------------------------------------------
static PkgInstallContext_t PkgInstallCtx;

//--------------------------------------------------------------------------------------------------
/**
 * Current SW uninstall context
 */
//--------------------------------------------------------------------------------------------------
static SwUninstallContext_t SwUninstallCtx;

//--------------------------------------------------------------------------------------------------
/**
 *  Convert avc session state to string.
 */
//--------------------------------------------------------------------------------------------------
static char* AvcSessionStateToStr
(
    le_avc_Status_t state  ///< The session state to convert.
)
//--------------------------------------------------------------------------------------------------
{
    char* result;

    switch (state)
    {
        case LE_AVC_NO_UPDATE:              result = "No update";               break;
        case LE_AVC_DOWNLOAD_PENDING:       result = "Download Pending";        break;
        case LE_AVC_DOWNLOAD_IN_PROGRESS:   result = "Download in Progress";    break;
        case LE_AVC_DOWNLOAD_COMPLETE:      result = "Download complete";       break;
        case LE_AVC_DOWNLOAD_FAILED:        result = "Download Failed";         break;
        case LE_AVC_INSTALL_PENDING:        result = "Install Pending";         break;
        case LE_AVC_INSTALL_IN_PROGRESS:    result = "Install in progress";     break;
        case LE_AVC_INSTALL_COMPLETE:       result = "Install completed";       break;
        case LE_AVC_INSTALL_FAILED:         result = "Install failed";          break;
        case LE_AVC_UNINSTALL_PENDING:      result = "Uninstall pending";       break;
        case LE_AVC_UNINSTALL_IN_PROGRESS:  result = "Uninstall in progress";   break;
        case LE_AVC_UNINSTALL_COMPLETE:     result = "Uninstall complete";      break;
        case LE_AVC_UNINSTALL_FAILED:       result = "Uninstall failed";        break;
        case LE_AVC_SESSION_STARTED:        result = "Session started";         break;
        case LE_AVC_SESSION_STOPPED:        result = "Session stopped";         break;
        case LE_AVC_REBOOT_PENDING:         result = "Reboot pending";          break;
        case LE_AVC_CONNECTION_PENDING:     result = "Connection pending";      break;
        case LE_AVC_AUTH_STARTED:           result = "Authentication started";  break;
        case LE_AVC_AUTH_FAILED:            result = "Authentication failed";   break;
        default:                            result = "Unknown";                 break;

    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Convert user agreement enum to string
 */
//--------------------------------------------------------------------------------------------------
static char* ConvertUserAgreementToString
(
    le_avc_UserAgreement_t userAgreement  ///< The operation that need to be converted.
)
//--------------------------------------------------------------------------------------------------
{
    char* result;

    switch (userAgreement)
    {
        case LE_AVC_USER_AGREEMENT_CONNECTION:  result = "Connection";   break;
        case LE_AVC_USER_AGREEMENT_DOWNLOAD:    result = "Download";     break;
        case LE_AVC_USER_AGREEMENT_INSTALL:     result = "Install";      break;
        case LE_AVC_USER_AGREEMENT_UNINSTALL:   result = "Uninstall";    break;
        case LE_AVC_USER_AGREEMENT_REBOOT:      result = "Reboot";       break;
        default:                                result = "Unknown";      break;

    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Default values for the Retry Timers. Unit is minute. 0 means disabled.
 */
//--------------------------------------------------------------------------------------------------
static uint16_t RetryTimers[LE_AVC_NUM_RETRY_TIMERS] = {15, 60, 240, 480, 1440, 2880, 0, 0};

// -------------------------------------------------------------------------------------------------
/**
 *  Polling Timer reference. Time interval to automatically start an AVC session.
 */
// ------------------------------------------------------------------------------------------------
static le_timer_Ref_t PollingTimerRef = NULL;

//--------------------------------------------------------------------------------------------------
// Local functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Stop the defer timer if it is running.
 */
//--------------------------------------------------------------------------------------------------
static void StopDeferTimer
(
    le_avc_UserAgreement_t userAgreement   ///< [IN] Operation for which user agreement is read
)
{
    switch (userAgreement)
    {
        case LE_AVC_USER_AGREEMENT_CONNECTION:
            // Stop the defer timer, if user starts a session before the defer timer expires.
            LE_DEBUG("Stop connect defer timer.");
            le_timer_Stop(ConnectDeferTimer);
            break;
        case LE_AVC_USER_AGREEMENT_DOWNLOAD:
            // Stop the defer timer, if user accepts download before the defer timer expires.
            LE_DEBUG("Stop download defer timer.");
            le_timer_Stop(DownloadDeferTimer);
            break;
        case LE_AVC_USER_AGREEMENT_INSTALL:
            // Stop the defer timer, if user accepts install before the defer timer expires.
            LE_DEBUG("Stop install defer timer.");
            le_timer_Stop(InstallDeferTimer);
            break;
        case LE_AVC_USER_AGREEMENT_UNINSTALL:
            // Stop the defer timer, if user accepts uninstall before the defer timer expires.
            LE_DEBUG("Stop uninstall defer timer.");
            le_timer_Stop(UninstallDeferTimer);
            break;
        case LE_AVC_USER_AGREEMENT_REBOOT:
            // Stop the defer timer, if user accepts reboot before the defer timer expires.
            LE_DEBUG("Stop reboot defer timer.");
            le_timer_Stop(RebootDeferTimer);
            break;
        default:
            LE_ERROR("Unknown operation");
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Convert an le_avc_UpdateType_t value to a string for debugging.
 *
 *  @return string version of the supplied enumeration value.
 */
//--------------------------------------------------------------------------------------------------
static const char* UpdateTypeToStr
(
    le_avc_UpdateType_t updateType  ///< The enumeration value to convert.
)
{
    const char* resultPtr;

    switch (updateType)
    {
        case LE_AVC_FIRMWARE_UPDATE:
            resultPtr = "LE_AVC_FIRMWARE_UPDATE";
            break;
        case LE_AVC_FRAMEWORK_UPDATE:
            resultPtr = "LE_AVC_FRAMEWORK_UPDATE";
            break;
        case LE_AVC_APPLICATION_UPDATE:
            resultPtr = "LE_AVC_APPLICATION_UPDATE";
            break;
        case LE_AVC_UNKNOWN_UPDATE:
            resultPtr = "LE_AVC_UNKNOWN_UPDATE";
            break;
    }
    return resultPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to read user agreement configuration
 */
//--------------------------------------------------------------------------------------------------
static void ReadUserAgreementConfiguration
(
    void
)
{
    bool UserAgreementStatus;
    le_avc_UserAgreement_t userAgreement;

    for (userAgreement = 0; userAgreement <= LE_AVC_USER_AGREEMENT_REBOOT; userAgreement++)
    {
        // Read user agreement configuration
        le_result_t result = le_avc_GetUserAgreement(userAgreement, &UserAgreementStatus);

        if (result == LE_OK)
        {
            LE_INFO("User agreement for %s is %s", ConvertUserAgreementToString(userAgreement),
                                                   UserAgreementStatus ? "ENABLED" : "DISABLED");
        }
        else
        {
            LE_WARN("User agreement for %s enabled by default",
                                                ConvertUserAgreementToString(userAgreement));
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending download.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AcceptDownloadPackage
(
    void
)
{
    StopDeferTimer(LE_AVC_USER_AGREEMENT_DOWNLOAD);

    if(LE_AVC_DM_SESSION == le_avc_GetSessionType())
    {
        LE_DEBUG("Accept a package download while the device is connected to the server");
        // Notify the registered handler to proceed with the download; only called once.
        CurrentState = AVC_DOWNLOAD_IN_PROGRESS;
        if (QueryDownloadHandlerRef !=  NULL)
        {
            QueryDownloadHandlerRef();
            QueryDownloadHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Download handler not valid.");
            return LE_FAULT;
        }
    }
    else
    {
        LE_DEBUG("Accept a package download while the device is not connected to the server");
        // Connect to the server.
        // When the device is connected, the package download will be launched.
        DownloadAgreement = true;
        avcClient_Connect();
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending package install
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AcceptInstallPackage
(
    void
)
{
    // If a user app is blocking the update, then just defer for some time.  Hopefully, the
    // next time this function is called, the user app will no longer be blocking the update.
    if ( BlockRefCount > 0 )
    {
        // Since the decision is not to install at this time, go back to idle
        CurrentState = AVC_IDLE;

        // Try the install later
        le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME * SECONDS_IN_A_MIN };

        le_timer_SetInterval(InstallDeferTimer, interval);
        le_timer_Start(InstallDeferTimer);
    }
    else
    {
        StopDeferTimer(LE_AVC_USER_AGREEMENT_INSTALL);

        // Notify the registered handler to proceed with the install; only called once.
        CurrentState = AVC_INSTALL_IN_PROGRESS;
        if (QueryInstallHandlerRef != NULL)
        {
            QueryInstallHandlerRef(PkgInstallCtx.type, PkgInstallCtx.instanceId);
            QueryInstallHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Install handler not valid");
            return LE_FAULT;
        }
    }
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending application uninstall
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AcceptUninstallApplication
(
    void
)
{
    // If an user app is blocking the update, then just defer for some time.  Hopefully, the
    // next time this function is called, the user app will no longer be blocking the update.
    if ( BlockRefCount > 0 )
    {
        // Since the decision is not to uninstall at this time, go back to idle
        CurrentState = AVC_IDLE;

        // Try the uninstall later
        le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME * SECONDS_IN_A_MIN };

        le_timer_SetInterval(UninstallDeferTimer, interval);
        le_timer_Start(UninstallDeferTimer);
    }
    else
    {
        StopDeferTimer(LE_AVC_USER_AGREEMENT_UNINSTALL);

        // Notify the registered handler to proceed with the uninstall; only called once.
        CurrentState = AVC_UNINSTALL_IN_PROGRESS;
        if (QueryUninstallHandlerRef != NULL)
        {
            QueryUninstallHandlerRef(SwUninstallCtx.instanceId);
            QueryUninstallHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Uninstall handler not valid");
            return LE_FAULT;
        }
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send update status event to registered applications
 */
//--------------------------------------------------------------------------------------------------
static void SendUpdateStatusEvent
(
    le_avc_Status_t updateStatus,   ///< [IN] Update status
    int32_t totalNumBytes,          ///< [IN] Total number of bytes to download
    int32_t downloadProgress,       ///< [IN] Download Progress in %
    void* contextPtr                ///< [IN] Context
)
{
    UpdateStatusData_t eventData;

    // Initialize the event data
    eventData.updateStatus = updateStatus;
    eventData.totalNumBytes = totalNumBytes;
    eventData.downloadProgress = downloadProgress;
    eventData.contextPtr = contextPtr;

    LE_DEBUG("Reporting %s", AvcSessionStateToStr(updateStatus));
    LE_DEBUG("Number of bytes to download %d", eventData.totalNumBytes);
    LE_DEBUG("Progress %d", eventData.downloadProgress);
    LE_DEBUG("ContextPtr %p", eventData.contextPtr);

    // Send the event to interested applications
    le_event_Report(UpdateStatusEvent, &eventData, sizeof(eventData));
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer the currently pending connection, for the given number of minutes
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t DeferConnect
(
     uint32_t deferMinutes        ///< [IN] Defer time in minutes
)
{
    LE_INFO("Deferring connection for %d minutes", deferMinutes);

    // Try the connection later
    le_clk_Time_t interval = { .sec = (deferMinutes * SECONDS_IN_A_MIN) };

    le_timer_SetInterval(ConnectDeferTimer, interval);
    le_timer_Start(ConnectDeferTimer);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer the currently pending download, for the given number of minutes
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t DeferDownload
(
     uint32_t deferMinutes          ///< [IN] Defer time in minutes
)
{
    if ( CurrentState != AVC_DOWNLOAD_PENDING )
    {
        LE_ERROR("Expected AVC_DOWNLOAD_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    // stop activity timer when download has been deferred
    avcClient_StopActivityTimer();

    LE_DEBUG("Deferring download");

    // Try the download later
    le_clk_Time_t interval = { .sec = (deferMinutes * SECONDS_IN_A_MIN) };

    le_timer_SetInterval(DownloadDeferTimer, interval);
    le_timer_Start(DownloadDeferTimer);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer the currently pending install
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t DeferInstall
(
    uint32_t deferMinutes           ///< [IN] Defer time in minutes
)
{
    if ( CurrentState != AVC_INSTALL_PENDING )
    {
        LE_ERROR("Expected AVC_INSTALL_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    // stop activity timer when installation has been deferred
    avcClient_StopActivityTimer();

    LE_DEBUG("Deferring install");

    // Try the install later
    le_clk_Time_t interval = { .sec = (deferMinutes * SECONDS_IN_A_MIN) };

    le_timer_SetInterval(InstallDeferTimer, interval);
    le_timer_Start(InstallDeferTimer);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer the currently pending uninstall, for the given number of minutes
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t DeferUninstall
(
    uint32_t deferMinutes               ///< [IN] Defer time in minutes
)
{
    if ( CurrentState != AVC_UNINSTALL_PENDING )
    {
        LE_ERROR("Expected AVC_UNINSTALL_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    // stop activity timer when uninstall has been deferred
    avcClient_StopActivityTimer();

    LE_DEBUG("Deferring Uninstall for %d minute.", deferMinutes);

    // Try the uninstall later
    le_clk_Time_t interval = { .sec = (deferMinutes * SECONDS_IN_A_MIN) };

    le_timer_SetInterval(UninstallDeferTimer, interval);
    le_timer_Start(UninstallDeferTimer);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Respond to connection pending notification
 *
 * @return
 *      - LE_OK if connection can proceed right away
 *      - LE_BUSY if waiting for response
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t RespondToConnectionPending
(
    void
)
{
    le_result_t result = LE_BUSY;
    bool isUserAgreementEnabled;

    LE_FATAL_IF((LE_OK != le_avc_GetUserAgreement(
                                        LE_AVC_USER_AGREEMENT_CONNECTION,
                                        &isUserAgreementEnabled)),
                                        "Failed to read user agreement configuration");
    if (!isUserAgreementEnabled)
    {
        // There is no control app; automatically accept any pending reboot
        LE_INFO("Automatically accepting connect");
        StopDeferTimer(LE_AVC_USER_AGREEMENT_CONNECTION);

        result = avcClient_Connect();

        if (LE_OK != result)
        {
            LE_ERROR("Error accepting connection");
            result = LE_FAULT;
        }
    }
    else if (NumStatusHandlers > 0)
    {
        // Notify registered control app.
        SendUpdateStatusEvent(LE_AVC_CONNECTION_PENDING, -1, -1, StatusHandlerContextPtr);
    }
    else
    {
        // There is a control app installed, but the handler is not yet registered.
        // Defer the decision to allow the control app time to register.
        LE_INFO("Automatically deferring connect, "
                "while waiting for control app to register");

        // Try the connection later
        le_clk_Time_t interval = {.sec = BLOCKED_DEFER_TIME * SECONDS_IN_A_MIN };

        le_timer_SetInterval(ConnectDeferTimer, interval);
        le_timer_Start(ConnectDeferTimer);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Respond to download pending notification
 *
 * @return
 *      - LE_OK if download can proceed right away
 *      - LE_BUSY if waiting for response
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t RespondToDownloadPending
(
    int32_t totalNumBytes,          ///< [IN] Remaining number of bytes to download
    int32_t dloadProgress           ///< [IN] Download progress
)
{
    le_result_t result = LE_BUSY;
    bool isUserAgreementEnabled;

    LE_INFO("Stopping activity timer during download pending.");
    avcClient_StopActivityTimer();

    LE_FATAL_IF((LE_OK != le_avc_GetUserAgreement(
                                       LE_AVC_USER_AGREEMENT_DOWNLOAD,
                                       &isUserAgreementEnabled)),
                                       "Failed to read user agreement configuration");
    if (!isUserAgreementEnabled)
    {
       result = AcceptDownloadPackage();
    }
    else if (NumStatusHandlers > 0)
    {
       // Notify registered control app.
       CurrentState = AVC_DOWNLOAD_PENDING;
       SendUpdateStatusEvent(LE_AVC_DOWNLOAD_PENDING,
                             totalNumBytes,
                             dloadProgress,
                             StatusHandlerContextPtr);
    }
    else
    {
       LE_INFO("Automatically deferring download, "
               "while waiting for control app to register");
       DeferDownload(BLOCKED_DEFER_TIME);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Respond to install pending notification
 *
 * @return
 *      - LE_OK if install can proceed right away
 *      - LE_BUSY if waiting for response
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t RespondToInstallPending
(
    void
)
{
    le_result_t result = LE_BUSY;
    bool isUserAgreementEnabled;

    LE_INFO("Stopping activity timer during install pending.");
    avcClient_StopActivityTimer();

    LE_FATAL_IF((LE_OK != le_avc_GetUserAgreement(
                                        LE_AVC_USER_AGREEMENT_INSTALL,
                                        &isUserAgreementEnabled)),
                                        "Failed to read user agreement configuration");
    if (!isUserAgreementEnabled)
    {
        LE_INFO("Automatically accepting install");
        result = AcceptInstallPackage();
    }
    else if (NumStatusHandlers > 0)
    {
        // Notify registered control app.
        CurrentState = AVC_INSTALL_PENDING;
        SendUpdateStatusEvent(LE_AVC_INSTALL_PENDING, -1, -1, StatusHandlerContextPtr);
    }
    else
    {
        LE_INFO("Automatically deferring install, while waiting for control app to register");
        DeferInstall(BLOCKED_DEFER_TIME);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Respond to uninstall pending notification
 *
 * @return
 *      - LE_OK if uninstall can proceed right away
 *      - LE_BUSY if waiting for response
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t RespondToUninstallPending
(
    void
)
{
    le_result_t result = LE_BUSY;
    bool isUserAgreementEnabled;

    LE_FATAL_IF((LE_OK != le_avc_GetUserAgreement(
                                        LE_AVC_USER_AGREEMENT_UNINSTALL,
                                        &isUserAgreementEnabled)),
                                        "Failed to read user agreement configuration");
    if (!isUserAgreementEnabled)
    {
        LE_INFO("Automatically accepting uninstall");
        result = AcceptUninstallApplication();
    }
    else if (NumStatusHandlers > 0)
    {
        // Notify registered control app.
        CurrentState = AVC_UNINSTALL_PENDING;
        SendUpdateStatusEvent(LE_AVC_UNINSTALL_PENDING, -1, -1, StatusHandlerContextPtr);
    }
    else
    {
        // There is a control app installed, but the handler is not yet registered.  Defer
        // the decision to allow the control app time to register.
        LE_INFO("Automatically deferring uninstall, while waiting for control app to register");
        DeferUninstall(BLOCKED_DEFER_TIME);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Respond to reboot pending notification
 *
 * @return
 *      - LE_OK if reboot can proceed right away
 *      - LE_BUSY if waiting for response
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t RespondToRebootPending
(
    void
)
{
    le_result_t result = LE_BUSY;
    bool isUserAgreementEnabled;

    LE_FATAL_IF((LE_OK != le_avc_GetUserAgreement(
                                         LE_AVC_USER_AGREEMENT_REBOOT,
                                         &isUserAgreementEnabled)),
                                         "Failed to read user agreement configuration");
     if (!isUserAgreementEnabled)
     {
         // There is no control app; automatically accept any pending reboot
         LE_INFO("Automatically accepting reboot");
         StopDeferTimer(LE_AVC_USER_AGREEMENT_REBOOT);
         result = LE_OK;
     }
     else if (NumStatusHandlers > 0)
     {
         // Notify registered control app.
         SendUpdateStatusEvent(LE_AVC_REBOOT_PENDING, -1, -1, StatusHandlerContextPtr);
     }
     else
     {
         // There is a control app installed, but the handler is not yet registered.
         // Defer the decision to allow the control app time to register.
         LE_INFO("Automatically deferring reboot, "
                 "while waiting for control app to register");

         // Try the reboot later
         le_clk_Time_t interval = {.sec = BLOCKED_DEFER_TIME * SECONDS_IN_A_MIN };

         le_timer_SetInterval(RebootDeferTimer, interval);
         le_timer_Start(RebootDeferTimer);
     }

     return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Re-send pending notification after session start
 */
//--------------------------------------------------------------------------------------------------
static void ResendPendingNotification
(
    le_avc_Status_t updateStatus
)
{
    le_avc_Status_t reportStatus = LE_AVC_NO_UPDATE;

    // If the notification sent above is session started, the following block will send
    // another notification reporting the pending states.
    if ( updateStatus == LE_AVC_SESSION_STARTED )
    {
        // The currentState is really the previous state in case of session start, as we don't
        // do a state change.
        switch ( CurrentState )
        {
            CurrentTotalNumBytes = -1;
            CurrentDownloadProgress = -1;

            case AVC_INSTALL_PENDING:
                RespondToInstallPending();
                break;

            case AVC_UNINSTALL_PENDING:
                RespondToUninstallPending();
                break;

            // Download pending is initiated by the package downloader
            case AVC_DOWNLOAD_PENDING:
            default:
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Process user agreement queries and take an action or forward to interested application which
 * can take decision.
 *
 * @return
 *      - LE_OK if operation can proceed right away
 *      - LE_BUSY if operation is deferred
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ProcessUserAgreement
(
    le_avc_Status_t updateStatus,   ///< [IN] Update status
    int32_t totalNumBytes,          ///< [IN] Remaining number of bytes to download
    int32_t dloadProgress           ///< [IN] Download progress
)
{
    le_result_t result = LE_BUSY;
    bool isUserAgreementEnabled;

    // Depending on user agreement configuration either process the operation
    // within avcService or forward to control app for acceptance.
    switch (updateStatus)
    {
        case LE_AVC_CONNECTION_PENDING:
            result = RespondToConnectionPending();
            break;

        case LE_AVC_DOWNLOAD_PENDING:
            result = RespondToDownloadPending(totalNumBytes, dloadProgress);
            break;

        case LE_AVC_INSTALL_PENDING:
            result = RespondToInstallPending();
            break;

        case LE_AVC_UNINSTALL_PENDING:
            result = RespondToUninstallPending();
            break;

        case LE_AVC_REBOOT_PENDING:
             result = RespondToRebootPending();
             break;

        default:
            LE_INFO("Update status is %s", AvcSessionStateToStr(updateStatus));
            // Forward notifications unrelated to user agreement to interested applications.
            SendUpdateStatusEvent(updateStatus,
                                  totalNumBytes,
                                  dloadProgress,
                                  StatusHandlerContextPtr);

            // Resend pending notification after session start
            ResendPendingNotification(updateStatus);
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler to receive update status notifications
 */
//--------------------------------------------------------------------------------------------------
void avcServer_UpdateHandler
(
    le_avc_Status_t updateStatus,
    le_avc_UpdateType_t updateType,
    int32_t totalNumBytes,
    int32_t dloadProgress,
    le_avc_ErrorCode_t errorCode
)
{
    LE_INFO("Update state: %s", AvcSessionStateToStr(updateStatus));

    // Keep track of the state of any pending downloads or installs.
    switch ( updateStatus )
    {
        case LE_AVC_CONNECTION_PENDING:
            LE_DEBUG("Process user agreement for connection");
            break;

        case LE_AVC_DOWNLOAD_PENDING:
            CurrentState = AVC_DOWNLOAD_PENDING;

            CurrentDownloadProgress = dloadProgress;
            CurrentTotalNumBytes = totalNumBytes;

            LE_DEBUG("Update type for DOWNLOAD is %d", updateType);
            CurrentUpdateType = updateType;
            break;

        case LE_AVC_INSTALL_PENDING:
            CurrentState = AVC_INSTALL_PENDING;

            // If the device resets during a FOTA download, then the CurrentUpdateType is lost
            // and needs to be assigned again.  Since we don't easily know if a reset happened,
            // always re-assign the value.
            LE_DEBUG("Update type for INSTALL is %d", updateType);
            CurrentUpdateType = updateType;
            break;

        case LE_AVC_UNINSTALL_PENDING:
            CurrentState = AVC_UNINSTALL_PENDING;
            LE_DEBUG("Update type for Uninstall is %d", updateType);
            CurrentUpdateType = updateType;
            break;

        case LE_AVC_REBOOT_PENDING:
            LE_DEBUG("Process user agreement for connection");
            break;

        case LE_AVC_DOWNLOAD_IN_PROGRESS:
            LE_DEBUG("Update type for DOWNLOAD is %d", updateType);
            CurrentTotalNumBytes = totalNumBytes;
            CurrentDownloadProgress = dloadProgress;
            CurrentUpdateType = updateType;

            if ((LE_AVC_APPLICATION_UPDATE == updateType) && (totalNumBytes >= 0))
            {
                // Set the bytes downloaded to workspace for resume operation
                avcApp_SetSwUpdateBytesDownloaded();
            }
            break;

        case LE_AVC_DOWNLOAD_COMPLETE:
            LE_DEBUG("Update type for DOWNLOAD is %d", updateType);
            avcClient_StartActivityTimer();
            DownloadAgreement = false;
            if (totalNumBytes > 0)
            {
                CurrentTotalNumBytes = totalNumBytes;
            }
            else
            {
                // Use last stored value
                totalNumBytes = CurrentTotalNumBytes;
            }
            if (dloadProgress > 0)
            {
                CurrentDownloadProgress = dloadProgress;
            }
            else
            {
                // Use last stored value
                dloadProgress = CurrentDownloadProgress;
            }
            CurrentUpdateType = updateType;

            if (LE_AVC_APPLICATION_UPDATE == updateType)
            {
                // Set the bytes downloaded to workspace for resume operation
                avcApp_SetSwUpdateBytesDownloaded();

                // End download and start unpack
                avcApp_EndDownload();
            }
            break;

        case LE_AVC_UNINSTALL_IN_PROGRESS:
        case LE_AVC_UNINSTALL_FAILED:
        case LE_AVC_UNINSTALL_COMPLETE:
            LE_ERROR("Received unexpected update status.");
            break;

        case LE_AVC_NO_UPDATE:
        case LE_AVC_INSTALL_COMPLETE:
            // There is no longer any current update, so go back to idle
            CurrentState = AVC_IDLE;
            break;

        case LE_AVC_DOWNLOAD_FAILED:
        case LE_AVC_INSTALL_FAILED:
            // There is no longer any current update, so go back to idle
            avcClient_StartActivityTimer();
            AvcErrorCode = errorCode;
            CurrentState = AVC_IDLE;

            if (LE_AVC_APPLICATION_UPDATE == updateType)
            {
                avcApp_DeletePackage();
            }
            break;

        case LE_AVC_SESSION_STARTED:
            // Update object9 list managed by legato to lwm2mcore
            avcClient_StartActivityTimer();
            avcApp_NotifyObj9List();
            avData_ReportSessionState(LE_AVDATA_SESSION_STARTED);
            break;

        case LE_AVC_INSTALL_IN_PROGRESS:
        case LE_AVC_SESSION_STOPPED:
            avcClient_StopActivityTimer();
            // These events do not cause a state transition
            avData_ReportSessionState(LE_AVDATA_SESSION_STOPPED);
            break;

        case LE_AVC_AUTH_STARTED:
            LE_DEBUG("Authenticated started");
            break;

        case LE_AVC_AUTH_FAILED:
            LE_DEBUG("Authenticated failed");
            break;

        default:
            LE_DEBUG("Unhandled updateStatus %s", AvcSessionStateToStr(updateStatus));
            break;
    }

    // Process user agreement or forward to control app if applicable.
    ProcessUserAgreement(updateStatus, totalNumBytes, dloadProgress);
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler for client session closes for clients that use the block/unblock API.
 *
 * Note: if the registered control app has closed then the associated data is cleaned up by
 * le_avc_RemoveStatusEventHandler(), since the remove handler is automatically called.
 */
//--------------------------------------------------------------------------------------------------
static void ClientCloseSessionHandler
(
    le_msg_SessionRef_t sessionRef,
    void*               contextPtr
)
{
    if ( sessionRef == NULL )
    {
        LE_ERROR("sessionRef is NULL");
        return;
    }

    LE_INFO("Client %p closed, remove allocated resources", sessionRef);

    // Search for the block reference(s) used by the closed client, and clean up any data.
    le_ref_IterRef_t iterRef = le_ref_GetIterator(BlockRefMap);

    while ( le_ref_NextNode(iterRef) == LE_OK )
    {
        if ( le_ref_GetValue(iterRef) == sessionRef )
        {
            le_ref_DeleteRef( BlockRefMap, (void*)le_ref_GetSafeRef(iterRef) );
            BlockRefCount--;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Query if it's okay to proceed with an application install
 *
 * @return
 *      - LE_OK if install can proceed right away
 *      - LE_BUSY if install is deferred
 */
//--------------------------------------------------------------------------------------------------
static le_result_t QueryInstall
(
    void
)
{
    return ProcessUserAgreement(LE_AVC_INSTALL_PENDING, -1, -1);
}

//--------------------------------------------------------------------------------------------------
/**
 * Query if it's okay to proceed with a package download
 *
 * @return
 *      - LE_OK if download can proceed right away
 *      - LE_BUSY if download is deferred
 */
//--------------------------------------------------------------------------------------------------
static le_result_t QueryDownload
(
    uint32_t totalNumBytes          ///< Number of bytes to download.
)
{
    return ProcessUserAgreement(LE_AVC_DOWNLOAD_PENDING, PkgDownloadCtx.bytesToDownload, 0);
}

//--------------------------------------------------------------------------------------------------
/**
 * Query if it's okay to proceed with an application uninstall
 *
 * @return
 *      - LE_OK if uninstall can proceed right away
 *      - LE_BUSY if uninstall is deferred
 */
//--------------------------------------------------------------------------------------------------
static le_result_t QueryUninstall
(
    void
)
{
    return ProcessUserAgreement(LE_AVC_UNINSTALL_PENDING, -1, -1);
}

//--------------------------------------------------------------------------------------------------
/**
 * Query if it's okay to proceed with a device reboot
 *
 * @return
 *      - LE_OK if reboot can proceed right away
 *      - LE_BUSY if reboot is deferred
 */
//--------------------------------------------------------------------------------------------------
static le_result_t QueryReboot
(
    void
)
{
    return ProcessUserAgreement(LE_AVC_REBOOT_PENDING, -1, -1);
}

//--------------------------------------------------------------------------------------------------
/**
 * Query if it's okay to proceed with a device connect
 *
 * @return
 *      - LE_OK if connect can proceed right away
 *      - LE_BUSY if connect is deferred
 */
//--------------------------------------------------------------------------------------------------
static le_result_t QueryConnect
(
    void
)
{
    return ProcessUserAgreement(LE_AVC_CONNECTION_PENDING, -1, -1);
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the download defer timer expires.
 */
//--------------------------------------------------------------------------------------------------
static void DownloadTimerExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    if ( QueryDownload(PkgDownloadCtx.bytesToDownload) == LE_OK )
    {
        // Notify the registered handler to proceed with the download; only called once.
        if (QueryDownloadHandlerRef != NULL)
        {
            QueryDownloadHandlerRef();
            QueryDownloadHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Download handler not valid");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the install defer timer expires.
 */
//--------------------------------------------------------------------------------------------------
static void InstallTimerExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    if ( QueryInstall() == LE_OK )
    {
        // Notify the registered handler to proceed with the install; only called once.
        if (QueryInstallHandlerRef != NULL)
        {
            QueryInstallHandlerRef(PkgInstallCtx.type, PkgInstallCtx.instanceId);
            QueryInstallHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Install handler not valid");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the uninstall defer timer expires.
 */
//--------------------------------------------------------------------------------------------------
static void UninstallTimerExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    if ( QueryUninstall() == LE_OK )
    {
        // Notify the registered handler to proceed with the uninstall; only called once.
        if (QueryUninstallHandlerRef != NULL)
        {
            QueryUninstallHandlerRef(SwUninstallCtx.instanceId);
            QueryUninstallHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Uninstall handler not valid");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the reboot defer timer expires.
 */
//--------------------------------------------------------------------------------------------------
static void RebootTimerExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    if (LE_OK == QueryReboot())
    {
        // Notify the registered handler to proceed with the reboot; only called once.
        if (NULL != QueryRebootHandlerRef)
        {
            QueryRebootHandlerRef();
            QueryRebootHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Reboot handler not valid");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the connection defer timer expires.
 */
//--------------------------------------------------------------------------------------------------
static void ConnectTimerExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    if (LE_OK == QueryConnect())
    {
        avcClient_Connect();
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the launch connection timer expires
 */
//--------------------------------------------------------------------------------------------------
static void LaunchConnectExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    avcClient_Connect();
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert lwm2m core update type to avc update type
 */
//--------------------------------------------------------------------------------------------------
static le_avc_UpdateType_t ConvertToAvcType
(
    lwm2mcore_UpdateType_t type             ///< [IN] Lwm2mcore update type
)
{
    if (LWM2MCORE_FW_UPDATE_TYPE == type)
    {
        return LE_AVC_FIRMWARE_UPDATE;
    }
    else if (LWM2MCORE_SW_UPDATE_TYPE == type)
    {
        return LE_AVC_APPLICATION_UPDATE;
    }
    else
    {
        return LE_AVC_UNKNOWN_UPDATE;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Write avc configuration parameter to platform memory
 *
 * @return
 *      - LE_OK if successful
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SetAvcConfig
(
    AvcConfigData_t* configPtr   ///< [IN] configuration data buffer
)
{
    if (NULL == configPtr)
    {
        LE_ERROR("Avc configuration pointer is null");
        return LE_FAULT;
    }

    le_result_t result;
    int pathLen;
    char path[LE_FS_PATH_MAX_LEN];
    memset(path, 0, LE_FS_PATH_MAX_LEN);
    pathLen = snprintf(path, LE_FS_PATH_MAX_LEN, "%s/%s", AVC_CONFIG_PATH, AVC_CONFIG_PARAM);

    if (pathLen > LE_FS_PATH_MAX_LEN)
    {
        LE_ERROR("Buffer overflow in config path");
        return LE_FAULT;
    }

    result = WriteFs(path, configPtr, sizeof(AvcConfigData_t));

    if (LE_OK == result)
    {
        return LE_OK;
    }
    else
    {
        LE_ERROR("Error writing to le_fs");
        return LE_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Read avc configuration parameter to platform memory
 *
 * @return
 *      - LE_OK if successful
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetAvcConfig
(
    AvcConfigData_t* configPtr   ///< [INOUT] configuration data buffer
)
{
    if (NULL == configPtr)
    {
        LE_ERROR("Avc configuration pointer is null");
        return LE_FAULT;
    }

    le_result_t result;
    int pathLen;
    char path[LE_FS_PATH_MAX_LEN];
    memset(path, 0, LE_FS_PATH_MAX_LEN);
    pathLen = snprintf(path, LE_FS_PATH_MAX_LEN, "%s/%s", AVC_CONFIG_PATH, AVC_CONFIG_PARAM);
    if (pathLen > LE_FS_PATH_MAX_LEN)
    {
        LE_ERROR("Buffer overflow in config path");
        return LE_OVERFLOW;
    }

    size_t size = sizeof(AvcConfigData_t);
    result = ReadFs(path, configPtr, &size);

    if (LE_OK == result)
    {
        return LE_OK;
    }
    else
    {
        LE_ERROR("Error reading from %s", path);
        return LE_UNAVAILABLE;
    }
}

//-------------------------------------------------------------------------------------------------
/**
 * Connect to AV server
 */
//-------------------------------------------------------------------------------------------------
static void ConnectToServer
(
    void
)
{
    // Start a session.
    if (LE_DUPLICATE == avcClient_Connect())
    {
        // Session is already connected, but wireless network could have been de-provisioned
        // due to NAT timeout. Do a registration update to re-establish connection.
        if (LE_OK != avcClient_Update())
        {
            // Restart the session if registration update fails.
            avcClient_Disconnect(true);

            // Connect after 2 seconds
            le_clk_Time_t interval = { .sec = 2 };

            le_timer_SetInterval(LaunchConnectTimer, interval);
            le_timer_Start(LaunchConnectTimer);
        }
    }
}

//-------------------------------------------------------------------------------------------------
/**
 * Save current epoch time to le_fs
 *
 * @return
 *      - LE_OK if successful
 *      - LE_FAULT if otherwise
 */
//-------------------------------------------------------------------------------------------------
static le_result_t SaveCurrentEpochTime
(
    void
)
{
    le_result_t result;
    AvcConfigData_t avcConfig;

    // Retrieve configuration from le_fs
    result = GetAvcConfig(&avcConfig);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to retrieve avc config from le_fs");
       return LE_FAULT;
    }

    // Set the last time since epoch
    avcConfig.connectionEpochTime = time(NULL);

    // Write configuration to le_fs
    result = SetAvcConfig(&avcConfig);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to write avc config from le_fs");
       return LE_FAULT;
    }

    return LE_OK;
}

//-------------------------------------------------------------------------------------------------
/**
 * Called when the polling timer expires.
 */
//-------------------------------------------------------------------------------------------------
static void PollingTimerExpiryHandler
(
    void
)
{
    le_result_t result;
    AvcConfigData_t avcConfig;

    LE_INFO("Polling timer expired");

    SaveCurrentEpochTime();

    ConnectToServer();

    // Restart the timer for the next interval
    uint32_t pollingTimerInterval;
    LE_ASSERT(LE_OK == le_avc_GetPollingTimer(&pollingTimerInterval));

    LE_INFO("A connection to server will be made in %d minutes", pollingTimerInterval);

    le_clk_Time_t interval = {.sec = pollingTimerInterval * SECONDS_IN_A_MIN};
    LE_ASSERT(LE_OK == le_timer_SetInterval(PollingTimerRef, interval));
    LE_ASSERT(LE_OK == le_timer_Start(PollingTimerRef));
}

//-------------------------------------------------------------------------------------------------
/**
 * Initialize the polling timer at startup.
 *
 * Note: This functions reads the polling timer configuration and if enabled starts the polling
 * timer based on the current time and the last time since a connection was established.
 */
//-------------------------------------------------------------------------------------------------
static void InitPollingTimer
(
    void
)
{
    le_result_t result;
    AvcConfigData_t avcConfig;

    // Polling timer, in minutes.
    uint32_t pollingTimer = 0;

    if(LE_OK != le_avc_GetPollingTimer(&pollingTimer))
    {
        LE_ERROR("Polling timer not configured");
        return;
    }

    if (POLLING_TIMER_DISABLED == pollingTimer)
    {
        LE_INFO("Polling Timer disabled. AVC session will not be started periodically.");
    }
    else
    {
        // Remaining polling timer, in seconds.
        uint32_t remainingPollingTimer = 0;

        // Current time, in seconds since Epoch.
        time_t currentTime = time(NULL);

        // Time elapsed since last poll
        time_t timeElapsed = 0;

        // Retrieve configuration from le_fs
        result = GetAvcConfig(&avcConfig);
        if (result != LE_OK)
        {
           LE_ERROR("Failed to retrieve avc config from le_fs");
           return;
        }

        // Connect to server if polling timer elapsed
        timeElapsed = currentTime - avcConfig.connectionEpochTime;

        // If time difference is negative, maybe the system time was altered.
        // If the time difference exceeds the polling timer, then that means the current polling
        // timer runs to the end.
        // In both cases set timeElapsed to 0 which effectively start the polling timer fresh.
        if ((timeElapsed < 0) || (timeElapsed >= (pollingTimer * SECONDS_IN_A_MIN)))
        {
            timeElapsed = 0;

            // Set the last time since epoch
            avcConfig.connectionEpochTime = currentTime;

            // Write configuration to le_fs
            result = SetAvcConfig(&avcConfig);
            if (result != LE_OK)
            {
                LE_ERROR("Failed to write avc config from le_fs");
                return;
            }

            ConnectToServer();
        }

        remainingPollingTimer = ((pollingTimer * SECONDS_IN_A_MIN) - timeElapsed);

        LE_INFO("Polling Timer is set to start AVC session every %d minutes.", pollingTimer);
        LE_INFO("The current Polling Timer will start a session in %d seconds.",
                                                                remainingPollingTimer);

        // Set a timer to start the next session.
        le_clk_Time_t interval = {.sec = remainingPollingTimer};

        LE_ASSERT(LE_OK == le_timer_SetInterval(PollingTimerRef, interval));
        LE_ASSERT(LE_OK == le_timer_Start(PollingTimerRef));
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Init ongoing FOTA/SOTA job on resume (i.e. device restart, legato restart etc)
 */
//--------------------------------------------------------------------------------------------------
static void InitAvcJobOnResume
(
    void
)
{
    // If Current State is not idle, this means that a user agreement is required after
    // reboot. Notify the application for this event.
    if (AVC_DOWNLOAD_PENDING == CurrentState)
    {
        uint64_t numBytesToDownload = 0;

        if (LE_OK == packageDownloader_BytesLeftToDownload(&numBytesToDownload))
        {
            LE_DEBUG("Bytes left to download: %llu", numBytesToDownload);
            // Notify the application of package download
            avcServer_QueryDownload(lwm2mcore_PackageDownloaderAcceptDownload, numBytesToDownload);
        }
    }
    // Only FOTA install case will be handled next. SOTA case is handled separately in SOTA
    // initialization function.
    else if (AVC_INSTALL_PENDING == CurrentState)
    {
        bool isInstallRequest = false;
        if ( (LE_OK == packageDownloader_GetFwUpdateInstallPending(&isInstallRequest))
          && (isInstallRequest) )
        {
            // FOTA use case
            ResumeFwInstall();
        }
    }
    else
    {
        // As firmware update causes device LE_AVC_INSTALL_COMPLETE or LE_AVC_INSTALL_FAILED
        // notification should be sent after reboot to end FOTA job. Check FW update result and set
        // server notification flag if necessary.
        if (LWM2MCORE_ERR_COMPLETED_OK == lwm2mcore_GetFirmwareUpdateInstallResult())
        {
            bool isNotificationRequest = false;
            // Request connection to server if notification is set.
            if (   (LE_OK == packageDownloader_GetFwUpdateNotification(&isNotificationRequest))
                && (true == isNotificationRequest))
            {
                avcServer_RequestConnection(LE_AVC_FIRMWARE_UPDATE);
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
// Internal interface functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Function to check if the agent needs to connect to the server. For FOTA it should be called
 * only after reboot, and for SOTA it should be called after update finishes. However, this function
 * will request connection to server only if there is no session going on.
 */
//--------------------------------------------------------------------------------------------------
void avcServer_RequestConnection
(
    le_avc_UpdateType_t updateType
)
{
    if (LE_AVC_SESSION_INVALID != le_avc_GetSessionType())
    {
        LE_INFO("Already session is going on");
        return;
    }

    if (NumStatusHandlers <= 0)
    {
        LE_ERROR("No control app status handler registered");
        return;
    }

    switch(updateType)
    {
        case LE_AVC_FIRMWARE_UPDATE:
            // Notify registered control app.
            LE_DEBUG("Reporting status LE_AVC_CONNECTION_PENDING for FOTA");
            SendUpdateStatusEvent(LE_AVC_CONNECTION_PENDING, -1, -1, StatusHandlerContextPtr);
            break;

        case LE_AVC_APPLICATION_UPDATE:
            // Notify registered control app.
            LE_DEBUG("Reporting status LE_AVC_CONNECTION_PENDING for SOTA");
            SendUpdateStatusEvent(LE_AVC_CONNECTION_PENDING, -1, -1, StatusHandlerContextPtr);
            break;

        default:
            LE_ERROR("Unsupported updateType: %s", UpdateTypeToStr(updateType));
            break;

    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with an application install
 *
 * If an install can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with an install. Note that handlerRef will be called at most once.
 *
 * @return
 *      - LE_OK if install can proceed right away (handlerRef will not be called)
 *      - LE_BUSY if handlerRef will be called later to notify when install can proceed
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_QueryInstall
(
    avcServer_InstallHandlerFunc_t handlerRef,  ///< [IN] Handler to receive install response.
    lwm2mcore_UpdateType_t  type,               ///< [IN] update type.
    uint16_t instanceId                         ///< [IN] instance id (0 for fw install).
)
{
    le_result_t result;

    if ( QueryInstallHandlerRef != NULL )
    {
        LE_ERROR("Duplicate install attempt");
        return LE_FAULT;
    }

    // Update install handler
    CurrentUpdateType = ConvertToAvcType(type);
    PkgInstallCtx.type = type;
    PkgInstallCtx.instanceId = instanceId;
    QueryInstallHandlerRef = handlerRef;

    result = QueryInstall();

    // Reset the handler as install can proceed now.
    if (LE_BUSY != result)
    {
        QueryInstallHandlerRef = NULL;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a package download
 *
 * If a download can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with a download. Note that handlerRef will be called at most once.
 *
 * @return
 *      - LE_OK if download can proceed right away (handlerRef will not be called)
 *      - LE_BUSY if handlerRef will be called later to notify when download can proceed
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_QueryDownload
(
    avcServer_DownloadHandlerFunc_t handlerFunc,    ///< [IN] Download handler function.
    uint32_t bytesToDownload                        ///< [IN] Number of bytes to download
)
{
    le_result_t result;

    if ( QueryDownloadHandlerRef != NULL )
    {
        LE_ERROR("Duplicate download attempt");
        return LE_FAULT;
    }

    // Update download handler
    PkgDownloadCtx.bytesToDownload = bytesToDownload;
    QueryDownloadHandlerRef = handlerFunc;

    result = QueryDownload(bytesToDownload);

    // Reset the handler as download can proceed now.
    if (LE_BUSY != result)
    {
        QueryDownloadHandlerRef = NULL;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a device reboot
 *
 * If a reboot can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with a reboot. Note that handlerRef will be called at most once.
 *
 * @return
 *      - LE_OK if reboot can proceed right away (handlerRef will not be called)
 *      - LE_BUSY if handlerRef will be called later to notify when reboot can proceed
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_QueryReboot
(
    avcServer_RebootHandlerFunc_t handlerFunc   ///< [IN] Reboot handler function.
)
{
    le_result_t result;

    if (NULL != QueryRebootHandlerRef)
    {
        LE_ERROR("Duplicate reboot attempt");
        return LE_FAULT;
    }

    // Update reboot handler
    QueryRebootHandlerRef = handlerFunc;

    result = QueryReboot();

    // Reset the handler as reboot can proceed now.
    if (LE_BUSY != result)
    {
        QueryRebootHandlerRef = NULL;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initializes user agreement queries of download, install and uninstall. Used after a session
 * start for SOTA resume.
 */
//--------------------------------------------------------------------------------------------------
void avcServer_InitUserAgreement
(
    void
)
{
    StopDeferTimer(LE_AVC_USER_AGREEMENT_DOWNLOAD);
    QueryDownloadHandlerRef = NULL;

    StopDeferTimer(LE_AVC_USER_AGREEMENT_INSTALL);
    QueryInstallHandlerRef = NULL;

    StopDeferTimer(LE_AVC_USER_AGREEMENT_UNINSTALL);
    QueryUninstallHandlerRef = NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with an application uninstall
 *
 * If an uninstall can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with an uninstall. Note that handlerRef will be called at most once.
 *
 * @return
 *      - LE_OK if uninstall can proceed right away (handlerRef will not be called)
 *      - LE_BUSY if handlerRef will be called later to notify when uninstall can proceed
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_QueryUninstall
(
    avcServer_UninstallHandlerFunc_t handlerRef,  ///< [IN] Handler to receive install response.
    uint16_t instanceId                           ///< Instance Id (0 for FW, any value for SW)
)
{
    le_result_t result;

    // Return busy, if user tries to uninstall multiple apps together
    // As the query is already in progress, both the apps will be removed after we get permission
    // for a single uninstall
    if ( QueryUninstallHandlerRef != NULL )
    {
        LE_ERROR("Duplicate uninstall attempt");
        return LE_BUSY;
    }

    // Update uninstall handler
    SwUninstallCtx.instanceId = instanceId;
    QueryUninstallHandlerRef = handlerRef;

    result = QueryUninstall();

    // Reset the handler as uninstall can proceed now.
    if (LE_BUSY != result)
    {
        QueryUninstallHandlerRef = NULL;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Receive the report from avcAppUpdate and pass it to the control APP
 *
 *
 * @return
 *      - void
 */
//--------------------------------------------------------------------------------------------------
void avcServer_ReportInstallProgress
(
    le_avc_Status_t updateStatus,
    uint installProgress,
    le_avc_ErrorCode_t errorCode
)
{
    LE_DEBUG("Report install progress to registered handler.");

    // Notify registered control app
    SendUpdateStatusEvent(updateStatus, -1, installProgress, StatusHandlerContextPtr);

    if (updateStatus == LE_AVC_INSTALL_FAILED)
    {
        AvcErrorCode = errorCode;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Request the avcServer to open a AV session.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_DUPLICATE if already connected.
 *      - LE_BUSY if currently retrying.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_RequestSession
(
    void
)
{
    le_result_t result = LE_OK;

    if ( SessionRequestHandlerRef != NULL )
    {
        // Notify registered control app.
        LE_DEBUG("Forwarding session open request to control app.");
        SessionRequestHandlerRef(LE_AVC_SESSION_ACQUIRE, SessionRequestHandlerContextPtr);
    }
    else
    {
        LE_DEBUG("Unconditionally accepting request to open session.");
        result = avcClient_Connect();
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Request the avcServer to close a AV session.
 *
 * @return
 *      - LE_OK if able to initiate a session close
 *      - LE_FAULT on error
 *      - LE_BUSY if session is owned by control app
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_ReleaseSession
(
    void
)
{
    le_result_t result = LE_OK;

    if ( SessionRequestHandlerRef != NULL )
    {
        // Notify registered control app.
        LE_DEBUG("Forwarding session release request to control app.");
        SessionRequestHandlerRef(LE_AVC_SESSION_RELEASE, SessionRequestHandlerContextPtr);
    }
    else
    {
        LE_DEBUG("Releasing session opened by user app.");
        result = avcClient_Disconnect(true);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer Update Status Handler
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerUpdateStatusHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    UpdateStatusData_t* eventDataPtr = reportPtr;
    le_avc_StatusHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    clientHandlerFunc(eventDataPtr->updateStatus,
                      eventDataPtr->totalNumBytes,
                      eventDataPtr->downloadProgress,
                      le_event_GetContextPtr());
}

//--------------------------------------------------------------------------------------------------
// API functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * le_avc_StatusHandler handler ADD function
 */
//--------------------------------------------------------------------------------------------------
le_avc_StatusEventHandlerRef_t le_avc_AddStatusEventHandler
(
    le_avc_StatusHandlerFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
)
{
    LE_DEBUG("le_avc_AddStatusEventHandler CurrentState %d", CurrentState);
    // handlerPtr must be valid
    if ( handlerPtr == NULL )
    {
        LE_KILL_CLIENT("Null handlerPtr");
    }

    LE_PRINT_VALUE("%p", handlerPtr);
    LE_PRINT_VALUE("%p", contextPtr);

    // Register the user app handler
    le_event_HandlerRef_t handlerRef = le_event_AddLayeredHandler(
                                                    "AvcUpdateStaus",
                                                    UpdateStatusEvent,
                                                    FirstLayerUpdateStatusHandler,
                                                    (le_event_HandlerFunc_t)handlerPtr);

    le_event_SetContextPtr(handlerRef, contextPtr);

    // Number of user apps registered
    NumStatusHandlers++;

    // Initialize any ongoing SOTA/FOTA job.
    InitAvcJobOnResume();

    return (le_avc_StatusEventHandlerRef_t)handlerRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_avc_StatusHandler handler REMOVE function
 */
//--------------------------------------------------------------------------------------------------
void le_avc_RemoveStatusEventHandler
(
    le_avc_StatusEventHandlerRef_t addHandlerRef
        ///< [IN]
)
{
    LE_PRINT_VALUE("%p", addHandlerRef);

    le_event_RemoveHandler((le_event_HandlerRef_t)addHandlerRef);

    // Decrement number of registered handlers
    NumStatusHandlers--;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_avc_SessionRequestHandler handler ADD function
 */
//--------------------------------------------------------------------------------------------------
le_avc_SessionRequestEventHandlerRef_t le_avc_AddSessionRequestEventHandler
(
    le_avc_SessionRequestHandlerFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
)
{
    // handlerPtr must be valid
    if ( handlerPtr == NULL )
    {
        LE_KILL_CLIENT("Null handlerPtr");
    }

    // Only allow the handler to be registered, if nothing is currently registered. In this way,
    // only one user app is allowed to register at a time.
    if ( SessionRequestHandlerRef == NULL )
    {
        SessionRequestHandlerRef = handlerPtr;
        SessionRequestHandlerContextPtr = contextPtr;

        return REGISTERED_SESSION_HANDLER_REF;
    }
    else
    {
        LE_KILL_CLIENT("Handler already registered");
        return NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * le_avc_SessionRequestHandler handler REMOVE function
 */
//--------------------------------------------------------------------------------------------------
void le_avc_RemoveSessionRequestEventHandler
(
    le_avc_SessionRequestEventHandlerRef_t addHandlerRef
        ///< [IN]
)
{
    if ( addHandlerRef != REGISTERED_SESSION_HANDLER_REF )
    {
        if ( addHandlerRef == NULL )
        {
            LE_ERROR("NULL ref ignored");
            return;
        }
        else
        {
            LE_KILL_CLIENT("Invalid ref = %p", addHandlerRef);
        }
    }

    if ( SessionRequestHandlerRef == NULL )
    {
        LE_KILL_CLIENT("Handler not registered");
    }

    // Clear all info related to the registered handler.
    SessionRequestHandlerRef = NULL;
    SessionRequestHandlerContextPtr = NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start a session with the AirVantage server
 *
 * This will also cause a query to be sent to the server, for pending updates.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_FAULT on failure
 *      - LE_DUPLICATE if already connected.
 *      - LE_BUSY if currently retrying.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_StartSession
(
    void
)
{
    StopDeferTimer(LE_AVC_USER_AGREEMENT_CONNECTION);
    return avcClient_Connect();
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop a session with the AirVantage server
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_StopSession
(
    void
)
{
    return avcClient_Disconnect(true);
}

//--------------------------------------------------------------------------------------------------
/**
 * Send a specific message to the server to be sure that the route between the device and the server
 * is available.
 * This API needs to be called when any package download is over (successfully or not) and before
 * sending any notification on asset data to the server.
 *
 * @return
 *      - LE_OK when the treatment is launched
 *      - LE_FAULT on failure
 *      - LE_UNSUPPORTED when this API is not supported
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_CheckRoute
(
    void
)
{
    return avcClient_Update();
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending download
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_AcceptDownload
(
    void
)
{
    if ( CurrentState != AVC_DOWNLOAD_PENDING )
    {
        LE_ERROR("Expected AVC_DOWNLOAD_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    return AcceptDownloadPackage();
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer the currently pending connection, for the given number of minutes
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_DeferConnect
(
    uint32_t deferMinutes                   ///< [IN] Defer time in minutes
)
{
    // Defer the connection.
    return DeferConnect(deferMinutes);
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer the currently pending download, for the given number of minutes
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_DeferDownload
(
    uint32_t deferMinutes                   ///< [IN] Defer time in minutes
)
{
    // Defer the download.
    return DeferDownload(deferMinutes);
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending install
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_AcceptInstall
(
    void
)
{
    if ( CurrentState != AVC_INSTALL_PENDING )
    {
        LE_ERROR("Expected AVC_INSTALL_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    // Clear the error code.
    AvcErrorCode = LE_AVC_ERR_NONE;

    if ( (CurrentUpdateType == LE_AVC_FIRMWARE_UPDATE)
      || (CurrentUpdateType == LE_AVC_APPLICATION_UPDATE) )
    {
        return AcceptInstallPackage();
    }
    else
    {
        LE_ERROR("Unknown update type %d", CurrentUpdateType);
        return LE_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer the currently pending install
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_DeferInstall
(
    uint32_t deferMinutes           ///< [IN] Defer time in minutes
)
{
    return DeferInstall(deferMinutes);
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending uninstall
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_AcceptUninstall
(
    void
)
{
    if ( CurrentState != AVC_UNINSTALL_PENDING )
    {
        LE_ERROR("Expected AVC_UNINSTALL_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    return AcceptUninstallApplication();
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer the currently pending uninstall
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_DeferUninstall
(
    uint32_t deferMinutes           ///< [IN] Defer time in minutes
)
{
    return DeferUninstall(deferMinutes);
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending reboot
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_AcceptReboot
(
    void
)
{
    StopDeferTimer(LE_AVC_USER_AGREEMENT_REBOOT);

    LE_DEBUG("Accept a device reboot");

    // Notify the registered handler to proceed with the reboot; only called once.
    if (QueryRebootHandlerRef !=  NULL)
    {
        QueryRebootHandlerRef();
        QueryRebootHandlerRef = NULL;
    }
    else
    {
        LE_ERROR("Reboot handler not valid.");
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer the currently pending reboot
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_DeferReboot
(
    uint32_t deferMinutes   ///< [IN] Minutes to defer the reboot
)
{
    LE_DEBUG("Deferring reboot for %d minute.", deferMinutes);

    // Try the reboot later
    le_clk_Time_t interval = {.sec = (deferMinutes * SECONDS_IN_A_MIN)};

    le_timer_SetInterval(RebootDeferTimer, interval);
    le_timer_Start(RebootDeferTimer);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the error code of the current update.
 */
//--------------------------------------------------------------------------------------------------
le_avc_ErrorCode_t le_avc_GetErrorCode
(
    void
)
{
    return AvcErrorCode;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the update type of the currently pending update
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT if not available
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_GetUpdateType
(
    le_avc_UpdateType_t* updateTypePtr
        ///< [OUT]
)
{
    if ( CurrentState == AVC_IDLE )
    {
        LE_ERROR("In AVC_IDLE state; no update pending or in progress");
        return LE_FAULT;
    }

    *updateTypePtr = CurrentUpdateType;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the update type of the currently pending update
 *
 */
//--------------------------------------------------------------------------------------------------
void avcServer_SetUpdateType
(
    le_avc_UpdateType_t updateType  ///< [IN]
)
{
    CurrentUpdateType = updateType;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the name for the currently pending application update
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT if not available, or is not APPL_UPDATE type
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_GetAppUpdateName
(
    char* updateName,
        ///< [OUT]

    size_t updateNameNumElements
        ///< [IN]
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Prevent any pending updates from being installed.
 *
 * @return
 *      - Reference for block update request (to be used later for unblocking updates)
 *      - NULL if the operation was not successful
 */
//--------------------------------------------------------------------------------------------------
le_avc_BlockRequestRef_t le_avc_BlockInstall
(
    void
)
{
    // Need to return a unique reference that will be used by Unblock. Use the client session ref
    // as the data, since we need to delete the ref when the client closes.
    le_avc_BlockRequestRef_t blockRef = le_ref_CreateRef(BlockRefMap,
                                                         le_avc_GetClientSessionRef());

    // Keep track of how many refs have been allocated
    BlockRefCount++;

    return blockRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * Allow any pending updates to be installed
 */
//--------------------------------------------------------------------------------------------------
void le_avc_UnblockInstall
(
    le_avc_BlockRequestRef_t blockRef
        ///< [IN]
        ///< block request ref returned by le_avc_BlockInstall
)
{
    // Look up the reference.  If it is NULL, then the reference is not valid.
    // Otherwise, delete the reference and update the count.
    void* dataRef = le_ref_Lookup(BlockRefMap, blockRef);
    if ( dataRef == NULL )
    {
        LE_KILL_CLIENT("Invalid block request reference %p", blockRef);
    }
    else
    {
        LE_PRINT_VALUE("%p", blockRef);
        le_ref_DeleteRef(BlockRefMap, blockRef);
        BlockRefCount--;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to read the last http status.
 *
 * @return
 *      - HttpStatus as defined in RFC 7231, Section 6.
 */
//--------------------------------------------------------------------------------------------------
uint16_t le_avc_GetHttpStatus
(
    void
)
{
    return pkgDwlCb_GetHttpStatus();
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to read the current session type, or the last session type if there is no
 * active session.
 *
 * @return
 *      - Session type
 */
//--------------------------------------------------------------------------------------------------
le_avc_SessionType_t le_avc_GetSessionType
(
    void
)
{
    return avcClient_GetSessionType();
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to retrieve status of the credentials provisioned on the device.
 *
 * @return
 *     LE_AVC_NO_CREDENTIAL_PROVISIONED
 *          - If neither Bootstrap nor Device Management credential is provisioned.
 *     LE_AVC_BS_CREDENTIAL_PROVISIONED
 *          - If Bootstrap credential is provisioned but Device Management credential is
              not provisioned.
 *     LE_AVC_DM_CREDENTIAL_PROVISIONED
 *          - If Device Management credential is provisioned.
 */
//--------------------------------------------------------------------------------------------------
le_avc_CredentialStatus_t le_avc_GetCredentialStatus
(
    void
)
{
    le_avc_CredentialStatus_t credStatus;
    lwm2mcore_CredentialStatus_t lwm2mcoreStatus = lwm2mcore_GetCredentialStatus();

    // Convert lwm2mcore credential status to avc credential status
    switch (lwm2mcoreStatus)
    {
        case LWM2MCORE_DM_CREDENTIAL_PROVISIONED:
            credStatus = LE_AVC_DM_CREDENTIAL_PROVISIONED;
            break;
        case LWM2MCORE_BS_CREDENTIAL_PROVISIONED:
            credStatus = LE_AVC_BS_CREDENTIAL_PROVISIONED;
            break;
        default:
            credStatus = LE_AVC_NO_CREDENTIAL_PROVISIONED;
            break;
    }

    return credStatus;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to read APN configuration.
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if there is any error while reading.
 *      - LE_OVERFLOW if the buffer provided is too small.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_GetApnConfig
(
    char* apnName,                 ///< [OUT] APN name
    size_t apnNameNumElements,     ///< [IN]  APN name max bytes
    char* userName,                ///< [OUT] User name
    size_t userNameNumElements,    ///< [IN]  User name max bytes
    char* userPassword,            ///< [OUT] Password
    size_t userPasswordNumElements ///< [IN]  Password max bytes
)
{
    AvcConfigData_t config;
    le_result_t result;

    // Retrieve apn configuration from le_fs
    result = GetAvcConfig(&config);
    if (result != LE_OK)
    {
        LE_ERROR("Failed to retrieve avc config from le_fs");
        return result;
    }

    // Copy apn name
    result = le_utf8_Copy(apnName, config.apn.apnName, apnNameNumElements, NULL);
    if (result != LE_OK)
    {
        LE_ERROR("Buffer overflow in copying apn name");
        return result;
    }

    // if apn name is empty, we don't need to return username or password
    if (strcmp(apnName, "") == 0)
    {
        le_utf8_Copy(userName, "", userNameNumElements, NULL);
        le_utf8_Copy(userPassword, "", userPasswordNumElements, NULL);
        goto done;
    }

    // Copy user name
    result = le_utf8_Copy(userName, config.apn.userName, userNameNumElements, NULL);
    if (result != LE_OK)
    {
        LE_ERROR("Buffer overflow in copying user name");
        return result;
    }

    // if username is empty, we don't need to return password
    if (strcmp(userName, "") == 0)
    {
        // if user name is empty, we don't need to return password
        le_utf8_Copy(userPassword, "", userPasswordNumElements, NULL);
        goto done;
    }

    // Copy password
    result = le_utf8_Copy(userPassword, config.apn.password, userPasswordNumElements, NULL);
    if (result != LE_OK)
    {
        LE_ERROR("Buffer overflow in copying password");
        return result;
    }

done:
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to write APN configuration.
 *
 * @return
 *      - LE_OK on success.
 *      - LE_OVERFLOW if one of the input strings is too long.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_SetApnConfig
(
    const char* apnName,     ///< [IN] APN name
    const char* userName,    ///< [IN] User name
    const char* userPassword ///< [IN] Password
)
{
    le_result_t result = LE_OK;
    AvcConfigData_t config;

    if ((strlen(apnName) > LE_AVC_APN_NAME_MAX_LEN) ||
        (strlen(userName) > LE_AVC_USERNAME_MAX_LEN) ||
        (strlen(userPassword) > LE_AVC_PASSWORD_MAX_LEN))
    {
        return LE_OVERFLOW;
    }

    // Retrieve configuration from le_fs
    result = GetAvcConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to retrieve avc config from le_fs");
       return result;
    }

    // Copy apn name
    result = le_utf8_Copy(config.apn.apnName, apnName, LE_AVC_APN_NAME_MAX_LEN, NULL);
    if (result != LE_OK)
    {
        LE_ERROR("Buffer overflow in copying apn name");
        return result;
    }

    // Copy user name
    result = le_utf8_Copy(config.apn.userName, userName, LE_AVC_USERNAME_MAX_LEN, NULL);
    if (result != LE_OK)
    {
        LE_ERROR("Buffer overflow in copying user name");
        return result;
    }

    // Copy password
    result = le_utf8_Copy(config.apn.password, userPassword, LE_AVC_PASSWORD_MAX_LEN, NULL);
    if (result != LE_OK)
    {
        LE_ERROR("Buffer overflow in copying password");
        return result;
    }

    // Write configuration to le_fs
    result = SetAvcConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to write avc config from le_fs");
       return result;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to read the retry timers.
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if not able to read the timers.
 *      - LE_OUT_OF_RANGE if one of the retry timers is out of range (0 to 20160).
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_GetRetryTimers
(
    uint16_t* timerValuePtr,  ///< [OUT] Retry timer array
    size_t* numTimers         ///< [IN/OUT] Max num of timers to get/num of timers retrieved
)
{
    AvcConfigData_t config;
    le_result_t result;

    if (*numTimers < LE_AVC_NUM_RETRY_TIMERS)
    {
        LE_ERROR("Supplied retry timer array too small (%d). Expected %d.",
                 *numTimers, LE_AVC_NUM_RETRY_TIMERS);
        return LE_FAULT;
    }

    // Retrieve configuration from le_fs
    result = GetAvcConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to retrieve avc config from le_fs");
       return result;
    }

    uint16_t retryTimersCfg[LE_AVC_NUM_RETRY_TIMERS] = {0};
    char timerName[RETRY_TIMER_NAME_BYTES] = {0};
    int i;
    for (i = 0; i < LE_AVC_NUM_RETRY_TIMERS; i++)
    {
        snprintf(timerName, RETRY_TIMER_NAME_BYTES, "%d", i);
        retryTimersCfg[i] = config.retryTimers[i];

        if ((retryTimersCfg[i] < LE_AVC_RETRY_TIMER_MIN_VAL) ||
            (retryTimersCfg[i] > LE_AVC_RETRY_TIMER_MAX_VAL))
        {
            LE_ERROR("The stored Retry Timer value %d is out of range. Min %d, Max %d.",
                     retryTimersCfg[i], LE_AVC_RETRY_TIMER_MIN_VAL, LE_AVC_RETRY_TIMER_MAX_VAL);

            return LE_OUT_OF_RANGE;
        }
    }

    for (i = 0; i < LE_AVC_NUM_RETRY_TIMERS; i++)
    {
        timerValuePtr[i] = retryTimersCfg[i];
    }

    *numTimers = LE_AVC_NUM_RETRY_TIMERS;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the retry timers.
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if not able to set the timers.
 *      - LE_OUT_OF_RANGE if one of the retry timers is out of range (0 to 20160).
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_SetRetryTimers
(
    const uint16_t* timerValuePtr, ///< [IN] Retry timer array
    size_t numTimers               ///< [IN] Number of retry timers
)
{
    le_result_t result = LE_OK;
    AvcConfigData_t config;

    if (numTimers < LE_AVC_NUM_RETRY_TIMERS)
    {
        LE_ERROR("Supplied retry timer array too small (%d). Expected %d.",
                 numTimers, LE_AVC_NUM_RETRY_TIMERS);
        return LE_FAULT;
    }

    int i;
    for (i = 0; i < LE_AVC_NUM_RETRY_TIMERS; i++)
    {
        if ((timerValuePtr[i] < LE_AVC_RETRY_TIMER_MIN_VAL) ||
            (timerValuePtr[i] > LE_AVC_RETRY_TIMER_MAX_VAL))
        {
            LE_ERROR("Attemping to set an out-of-range Retry Timer value of %d. Min %d, Max %d.",
                     timerValuePtr[i], LE_AVC_RETRY_TIMER_MIN_VAL, LE_AVC_RETRY_TIMER_MAX_VAL);
            return LE_OUT_OF_RANGE;
        }
    }

    // Retrieve configuration from le_fs
    result = GetAvcConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to retrieve avc config from le_fs");
       return result;
    }

    // Copy the retry timer values
    char timerName[RETRY_TIMER_NAME_BYTES] = {0};
    for (i = 0; i < LE_AVC_NUM_RETRY_TIMERS; i++)
    {
        snprintf(timerName, RETRY_TIMER_NAME_BYTES, "%d", i);
        config.retryTimers[i] = timerValuePtr[i];
    }

    // Write configuration to le_fs
    result = SetAvcConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to write avc config from le_fs");
       return result;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to read the polling timer.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT if not available
 *      - LE_OUT_OF_RANGE if the polling timer value is out of range (0 to 525600).
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_GetPollingTimer
(
    uint32_t* pollingTimerPtr  ///< [OUT] Polling timer
)
{
    uint32_t pollingTimerCfg;
    uint32_t lifetime;
    lwm2mcore_Sid_t sid;

    // read the lifetime from the server object
    sid = lwm2mcore_GetLifetime(&lifetime);
    if (LWM2MCORE_ERR_COMPLETED_OK != sid)
    {
        LE_ERROR("Unable to read lifetime from server configuration");
        return LE_FAULT;
    }

    // lifetime is in seconds and polling timer is in minutes
    pollingTimerCfg = lifetime / SECONDS_IN_A_MIN;

    // check if it this configuration is allowed
    if ((pollingTimerCfg < LE_AVC_POLLING_TIMER_MIN_VAL) ||
        (pollingTimerCfg > LE_AVC_POLLING_TIMER_MAX_VAL))
    {
        LE_ERROR("The stored Polling Timer value %d is out of range. Min %d, Max %d.",
                 pollingTimerCfg, LE_AVC_POLLING_TIMER_MIN_VAL, LE_AVC_POLLING_TIMER_MAX_VAL);
        return LE_OUT_OF_RANGE;
    }
    else
    {
        *pollingTimerPtr = pollingTimerCfg;
        return LE_OK;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the polling timer.
 *
 * @return
 *      - LE_OK on success.
 *      - LE_OUT_OF_RANGE if the polling timer value is out of range (0 to 525600).
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_SetPollingTimer
(
    uint32_t pollingTimer ///< [IN] Polling timer
)
{
    le_result_t result = LE_OK;
    lwm2mcore_Sid_t sid;

    // Stop polling timer if running
    if (PollingTimerRef != NULL)
    {
        if (le_timer_IsRunning(PollingTimerRef))
        {
            LE_ASSERT(LE_OK == le_timer_Stop(PollingTimerRef));
        }
    }

    // check if this configuration is allowed
    if ((pollingTimer < LE_AVC_POLLING_TIMER_MIN_VAL) ||
        (pollingTimer > LE_AVC_POLLING_TIMER_MAX_VAL))
    {
        LE_ERROR("Attemping to set an out-of-range Polling Timer value of %d. Min %d, Max %d.",
                 pollingTimer, LE_AVC_POLLING_TIMER_MIN_VAL, LE_AVC_POLLING_TIMER_MAX_VAL);
        return LE_OUT_OF_RANGE;
    }

    // lifetime in the server object is in seconds and polling timer is in minutes
    uint32_t lifetime = pollingTimer * SECONDS_IN_A_MIN;

    // set lifetime in lwm2mcore
    sid = lwm2mcore_SetLifetime(lifetime);

    if (LWM2MCORE_ERR_COMPLETED_OK != sid)
    {
        LE_ERROR("Failed to set lifetime");
        result = LE_FAULT;
    }

    // Store the current time to avc config
    result = SaveCurrentEpochTime();
    if (result != LE_OK)
    {
       LE_ERROR("Failed to set polling timer");
       return LE_FAULT;
    }

    // Start the polling timer if enabled
    if (pollingTimer != POLLING_TIMER_DISABLED)
    {
        uint32_t pollingTimeSeconds = pollingTimer * SECONDS_IN_A_MIN;

        LE_INFO("Polling Timer is set to start AVC session every %d minutes.", pollingTimer);
        LE_INFO("A connection to server will be made in %d seconds.", pollingTimeSeconds);

        // Set a timer to start the next session.
        le_clk_Time_t interval = {.sec = pollingTimeSeconds};

        LE_ASSERT(LE_OK == le_timer_SetInterval(PollingTimerRef, interval));
        LE_ASSERT(LE_OK == le_timer_Start(PollingTimerRef));
    }
    else
    {
        LE_INFO("Polling timer disabled");
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get the user agreement state
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_GetUserAgreement
(
    le_avc_UserAgreement_t userAgreement,   ///< [IN] Operation for which user agreement is read
    bool* isEnabledPtr                      ///< [OUT] true if enabled
)
{
    le_result_t result = LE_OK;
    AvcConfigData_t config;

    // Retrieve configuration from le_fs
    result = GetAvcConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to retrieve avc config from le_fs");
       return result;
    }

    switch (userAgreement)
    {
        case LE_AVC_USER_AGREEMENT_CONNECTION:
            *isEnabledPtr = config.ua.connect;
            break;
        case LE_AVC_USER_AGREEMENT_DOWNLOAD:
            *isEnabledPtr = config.ua.download;
            break;
        case LE_AVC_USER_AGREEMENT_INSTALL:
            *isEnabledPtr = config.ua.install;
            break;
        case LE_AVC_USER_AGREEMENT_UNINSTALL:
            *isEnabledPtr = config.ua.uninstall;
            break;
        case LE_AVC_USER_AGREEMENT_REBOOT:
            *isEnabledPtr = config.ua.reboot;
            break;
        default:
            *isEnabledPtr = 0;
            result = LE_FAULT;
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the default AVC config
 */
//--------------------------------------------------------------------------------------------------
void le_avc_SetDefaultAvcConfig
(
    void
)
{
    AvcConfigData_t avcConfig;
    int count;

    memset(&avcConfig, 0, sizeof(avcConfig));

    // set default retry timer values
    for(count = 0; count < LE_AVC_NUM_RETRY_TIMERS; count++)
    {
        avcConfig.retryTimers[count] = RetryTimers[count];
    }

    // set user agreement to default
    avcConfig.ua.connect = USER_AGREEMENT_DEFAULT;
    avcConfig.ua.download = USER_AGREEMENT_DEFAULT;
    avcConfig.ua.install = USER_AGREEMENT_DEFAULT;
    avcConfig.ua.uninstall = USER_AGREEMENT_DEFAULT;
    avcConfig.ua.reboot = USER_AGREEMENT_DEFAULT;

    // save current time
    avcConfig.connectionEpochTime = time(NULL);

    // write the config file
    SetAvcConfig(&avcConfig);

    // set lifetime
    le_avc_SetPollingTimer(POLLING_TIMER_DISABLED);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the user agreement state
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_SetUserAgreement
(
    le_avc_UserAgreement_t userAgreement,   ///< [IN] Operation for which user agreement
                                            ///  is configured
    bool  isEnabled                         ///< [IN] true if enabled
)
{
    le_result_t result = LE_OK;
    AvcConfigData_t config;

    // Retrieve configuration from le_fs
    result = GetAvcConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to retrieve avc config from le_fs");
       return result;
    }

    switch (userAgreement)
    {
        case LE_AVC_USER_AGREEMENT_CONNECTION:
            config.ua.connect = isEnabled;
            break;
        case LE_AVC_USER_AGREEMENT_DOWNLOAD:
            config.ua.download = isEnabled;
            break;
        case LE_AVC_USER_AGREEMENT_INSTALL:
            config.ua.install = isEnabled;
            break;
        case LE_AVC_USER_AGREEMENT_UNINSTALL:
            config.ua.uninstall = isEnabled;
            break;
        case LE_AVC_USER_AGREEMENT_REBOOT:
            config.ua.reboot = isEnabled;
            break;
        default:
            LE_ERROR("User agreement configuration invalid");
            break;
    }

    // Write configuration to le_fs
    result = SetAvcConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to write avc config from le_fs");
       return result;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Checks whether a FOTA installation is going on or not.
 *
 * @return
 *     - True if FOTA installation is going on.
 *     - False otherwise.
 */
//--------------------------------------------------------------------------------------------------
static bool IsFotaInstalling
(
    void
)
{
    lwm2mcore_FwUpdateState_t fwUpdateState = LWM2MCORE_FW_UPDATE_STATE_IDLE;
    lwm2mcore_FwUpdateResult_t fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;

    // Check if a FW update was ongoing
    return    (LE_OK == packageDownloader_GetFwUpdateState(&fwUpdateState))
           && (LE_OK == packageDownloader_GetFwUpdateResult(&fwUpdateResult))
           && (LWM2MCORE_FW_UPDATE_STATE_UPDATING == fwUpdateState)
           && (LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL == fwUpdateResult);

}

//--------------------------------------------------------------------------------------------------
/**
 * Check a initialization if a notification needs to be sent to the application
 */
//--------------------------------------------------------------------------------------------------
static void CheckNotificationAtStartup
(
    void
)
{
    if (fsSys_IsNewSys())
    {
        LE_INFO("New system installed. Removing old SOTA/FOTA resume info");
        // New system installed, all old(SOTA or FOTA) resume info are invalid. Delete them.
        packageDownloader_DeleteResumeInfo();
        // Delete SOTA states and unfinished package if there exists any
        avcApp_DeletePackage();
        // For FOTA new firmware installation cause device reboot. In that case, FW update state and
        // should be notified to server. In that case, don't delete FW update installation info.
        // Otherwise delete all FW update info.
        if (!IsFotaInstalling())
        {
            packageDownloader_DeleteFwUpdateInfo();
        }
        // Remove new system flag.
        fsSys_RemoveNewSysFlag();

        return;
    }

    le_avc_Status_t updateStatus = LE_AVC_NO_UPDATE;
    bool isInstallRequest = false;
    lwm2mcore_UpdateType_t updateType = LWM2MCORE_MAX_UPDATE_TYPE;

    // Check LE_AVC_INSTALL_PENDING notification for FOTA
    if ( (LE_OK == packageDownloader_GetFwUpdateInstallPending(&isInstallRequest))
      && isInstallRequest)
    {
        updateStatus = LE_AVC_INSTALL_PENDING;
        updateType = LWM2MCORE_FW_UPDATE_TYPE;
    }
    else
    {
        lwm2mcore_FwUpdateState_t fwUpdateState = LWM2MCORE_FW_UPDATE_STATE_IDLE;
        lwm2mcore_FwUpdateResult_t fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;

        lwm2mcore_SwUpdateState_t swUpdateState = LWM2MCORE_SW_UPDATE_STATE_INITIAL;
        lwm2mcore_SwUpdateResult_t swUpdateResult = LWM2MCORE_SW_UPDATE_RESULT_INITIAL;

        if (   (LE_OK != packageDownloader_GetFwUpdateState(&fwUpdateState))
            || (LE_OK != packageDownloader_GetFwUpdateResult(&fwUpdateResult))
            || (LE_OK != packageDownloader_GetSwUpdateState(&swUpdateState))
            || (LE_OK != packageDownloader_GetSwUpdateResult(&swUpdateResult)))
        {
            LE_ERROR("Can't retrieve suspend information");
            return;
        }

        LE_DEBUG("swUpdateState: %d, swUpdateResult: %d, fwUpdateState: %d, fwUpdateResult: %d",
                  swUpdateState,
                  swUpdateResult,
                  fwUpdateState,
                  fwUpdateResult);

        uint8_t downloadUri[LWM2MCORE_PACKAGE_URI_MAX_LEN+1];
        size_t uriLen = LWM2MCORE_PACKAGE_URI_MAX_LEN+1;

        // Check if an update package URI is stored
        if (LE_OK == packageDownloader_GetResumeInfo(downloadUri, &uriLen, &updateType))
        {
            // Resume info can successfully be retrieved, i.e. there should be some data to download
            updateStatus = LE_AVC_DOWNLOAD_PENDING;
        }
        else if (((swUpdateState == LWM2MCORE_SW_UPDATE_STATE_DOWNLOADED) ||
                  (swUpdateState == LWM2MCORE_SW_UPDATE_STATE_DELIVERED)) &&
                 (swUpdateResult == LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED))
        {
            avcApp_InternalState_t internalState;
            // Check whether any install request was sent from server, if no request sent
            // then reboot happened on LE_AVC_DOWNLOAD_COMPLETE but before LE_AVC_INSTALL_PENDING.
            if ((LE_OK == avcApp_GetSwUpdateInternalState(&internalState)) &&
                (INTERNAL_STATE_INSTALL_REQUESTED != internalState))
            {
                updateStatus = LE_AVC_DOWNLOAD_PENDING;
                updateType = LWM2MCORE_SW_UPDATE_TYPE;
            }
        }
        else if ((fwUpdateState == LWM2MCORE_FW_UPDATE_STATE_DOWNLOADED) &&
                 (fwUpdateResult == LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL))
        {
            updateStatus = LE_AVC_DOWNLOAD_PENDING;
            updateType = LWM2MCORE_FW_UPDATE_TYPE;
        }
    }

    LE_INFO("Init: updateStatus %d, updateType %d", updateStatus, updateType);

    if (LE_AVC_NO_UPDATE != updateStatus)
    {
        // Send a notification to the application
        avcServer_UpdateHandler(updateStatus,
                                ConvertToAvcType(updateType),
                                -1,
                                -1,
                                LE_AVC_ERR_NONE);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to check the user agreement for download
 */
//--------------------------------------------------------------------------------------------------
bool IsDownloadAccepted
(
    void
)
{
    return DownloadAgreement;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialization function for AVC Daemon
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // Initialize update status event
    UpdateStatusEvent = le_event_CreateId("Update Status", sizeof(UpdateStatusData_t));

    // Create safe reference map for block references. The size of the map should be based on
    // the expected number of simultaneous block requests, so take a reasonable guess.
    BlockRefMap = le_ref_CreateMap("BlockRef", 5);

    // Add a handler for client session closes
    le_msg_AddServiceCloseHandler( le_avc_GetServiceRef(), ClientCloseSessionHandler, NULL );

    // Init shared timer for deferring app install
    InstallDeferTimer = le_timer_Create("install defer timer");
    le_timer_SetHandler(InstallDeferTimer, InstallTimerExpiryHandler);

    UninstallDeferTimer = le_timer_Create("uninstall defer timer");
    le_timer_SetHandler(UninstallDeferTimer, UninstallTimerExpiryHandler);

    DownloadDeferTimer = le_timer_Create("download defer timer");
    le_timer_SetHandler(DownloadDeferTimer, DownloadTimerExpiryHandler);

    RebootDeferTimer = le_timer_Create("reboot defer timer");
    le_timer_SetHandler(RebootDeferTimer, RebootTimerExpiryHandler);

    ConnectDeferTimer = le_timer_Create("connect defer timer");
    le_timer_SetHandler(ConnectDeferTimer, ConnectTimerExpiryHandler);

    LaunchConnectTimer = le_timer_Create("launch connection timer");
    le_timer_SetHandler(LaunchConnectTimer, LaunchConnectExpiryHandler);

    PollingTimerRef = le_timer_Create("polling Timer");
    le_timer_SetHandler(PollingTimerRef, PollingTimerExpiryHandler);

    // Initialize the sub-components
    if (LE_OK != packageDownloader_Init())
    {
        LE_ERROR("failed to initialize package downloader");
    }

    assetData_Init();
    avData_Init();
    timeSeries_Init();
    push_Init();
    avcClient_Init();

    // Read the user defined timeout from config tree @ /apps/avcService/activityTimeout
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateReadTxn(AVC_SERVICE_CFG);
    int timeout = le_cfg_GetInt(iterRef, "activityTimeout", 20);
    le_cfg_CancelTxn(iterRef);
    avcClient_SetActivityTimeout(timeout);

    // Display user agreement configuration
    ReadUserAgreementConfiguration();

    // Start an AVC session periodically according to the Polling Timer config.
    InitPollingTimer();

    // Initialize user agreement.
    avcServer_InitUserAgreement();

    // Check if any notification needs to be sent to the application concerning
    // firmware update and application update
    CheckNotificationAtStartup();

    avcApp_Init();
}
