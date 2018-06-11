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
#include "watchdogChain.h"
#include "timeseriesData.h"
#include "avcClient.h"

//--------------------------------------------------------------------------------------------------
// Definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * AVC configuration path
 */
//--------------------------------------------------------------------------------------------------
#define AVC_SERVICE_CFG "/apps/avcService"

//--------------------------------------------------------------------------------------------------
/**
 * AVC configuration file
 */
//--------------------------------------------------------------------------------------------------
#define AVC_CONFIG_FILE      AVC_CONFIG_PATH "/" AVC_CONFIG_PARAM

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
 * Default user agreement setting for connection notifications
 */
//--------------------------------------------------------------------------------------------------
#define DISABLE_CONNECTION_NOTIFICATION  0

//--------------------------------------------------------------------------------------------------
/**
 * Value if polling timer is disabled
 */
//--------------------------------------------------------------------------------------------------
#define POLLING_TIMER_DISABLED  0

//--------------------------------------------------------------------------------------------------
/**
 * Text content of the wakeup SMS
 */
//--------------------------------------------------------------------------------------------------
#define WAKEUP_SMS_TEXT "LWM2MWAKEUP"

//--------------------------------------------------------------------------------------------------
/**
 * Ratelimit interval of the wakeup SMS
 */
//--------------------------------------------------------------------------------------------------
static const le_clk_Time_t WakeUpSmsInterval = {.sec = 60, .usec = 0};

//--------------------------------------------------------------------------------------------------
/**
 * WakeUp SMS timeout
 *
 * SMS received before this timeout, will be ignored.
 */
//--------------------------------------------------------------------------------------------------
static le_clk_Time_t WakeUpSmsTimeout = {0, 0};

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
    AVC_IDLE,                   ///< No updates pending or in progress
    AVC_DOWNLOAD_PENDING,       ///< Received pending download; no response sent yet
    AVC_DOWNLOAD_IN_PROGRESS,   ///< Accepted download, and in progress
    AVC_DOWNLOAD_COMPLETE,      ///< Download is complete
    AVC_INSTALL_PENDING,        ///< Received pending install; no response sent yet
    AVC_INSTALL_IN_PROGRESS,    ///< Accepted install, and in progress
    AVC_UNINSTALL_PENDING,      ///< Received pending uninstall; no response sent yet
    AVC_UNINSTALL_IN_PROGRESS,  ///< Accepted uninstall, and in progress
    AVC_REBOOT_PENDING,         ///< Received pending reboot; no response sent yet
    AVC_REBOOT_IN_PROGRESS,     ///< Accepted reboot, and in progress
    AVC_CONNECTION_PENDING,     ///< Received pending connection; no response sent yet
    AVC_CONNECTION_IN_PROGRESS  ///< Accepted connection, and in progress
}
AvcState_t;

//--------------------------------------------------------------------------------------------------
// Data structures
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Package download context
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint64_t                bytesToDownload;                        ///< Package size.
    lwm2mcore_UpdateType_t  type;                                   ///< Update type.
    char                    uri[LWM2MCORE_PACKAGE_URI_MAX_BYTES];   ///< Update package URI.
    bool                    resume;                                 ///< Is it a download resume?
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
 * Data associated with client le_avc_StatusHandler handler
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_avc_StatusHandlerFunc_t statusHandlerPtr;   ///< Pointer on handler function
    void*                      contextPtr;         ///< Context
}
AvcClientStatusHandlerData_t;

//--------------------------------------------------------------------------------------------------
/**
 * Data associated with the AvcUpdateStatusEvent
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_avc_Status_t              updateStatus;    ///< Update status
    le_avc_UpdateType_t          updateType;      ///< Update type
    int32_t                      totalNumBytes;   ///< Total number of bytes to download
    int32_t                      progress;        ///< Progress in percent
    le_avc_ErrorCode_t           errorCode;       ///< Error code
    AvcClientStatusHandlerData_t clientData;      ///< Data associated with client
                                                  ///< le_avc_StatusHandler handler
}
AvcUpdateStatusData_t;

//--------------------------------------------------------------------------------------------------
/**
 * Data associated with the UpdateStatusEvent
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_avc_Status_t updateStatus;   ///< Update status
    int32_t totalNumBytes;          ///< Total number of bytes to download
    int32_t progress;               ///< Progress in percent
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
 * Event for reporting update status notification to AVC service
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t AvcUpdateStatusEvent;

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
 * Launch reboot timer
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t LaunchRebootTimer;

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

// -------------------------------------------------------------------------------------------------
/**
 * Is session initiated by user?
 */
// ------------------------------------------------------------------------------------------------
static bool IsUserSession = false;

//--------------------------------------------------------------------------------------------------
// Local functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 *  Convert avc session state to string.
 */
//--------------------------------------------------------------------------------------------------
static char* AvcSessionStateToStr
(
    le_avc_Status_t state  ///< The session state to convert.
)
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
        case LE_AVC_SESSION_FAILED:         result = "Session failed";          break;
        case LE_AVC_SESSION_BS_STARTED:     result = "Session with BS started"; break;
        case LE_AVC_SESSION_STOPPED:        result = "Session stopped";         break;
        case LE_AVC_REBOOT_PENDING:         result = "Reboot pending";          break;
        case LE_AVC_CONNECTION_PENDING:     result = "Connection pending";      break;
        case LE_AVC_AUTH_STARTED:           result = "Authentication started";  break;
        case LE_AVC_AUTH_FAILED:            result = "Authentication failed";   break;
        case LE_AVC_CERTIFICATION_OK:       result = "Package certified";       break;
        case LE_AVC_CERTIFICATION_KO:       result = "Package not certified";   break;
        default:                            result = "Unknown";                 break;

    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Convert AVC state enum to string
 */
//--------------------------------------------------------------------------------------------------
static char* ConvertAvcStateToString
(
    AvcState_t avcState     ///< The state that need to be converted.
)
{
    char* result;

    switch (avcState)
    {
        case AVC_IDLE:                      result = "Idle";                    break;
        case AVC_DOWNLOAD_PENDING:          result = "Download pending";        break;
        case AVC_DOWNLOAD_IN_PROGRESS:      result = "Download in progress";    break;
        case AVC_INSTALL_PENDING:           result = "Install pending";         break;
        case AVC_INSTALL_IN_PROGRESS:       result = "Install in progress";     break;
        case AVC_UNINSTALL_PENDING:         result = "Uninstall pending";       break;
        case AVC_UNINSTALL_IN_PROGRESS:     result = "Uninstall in progress";   break;
        case AVC_REBOOT_PENDING:            result = "Reboot pending";          break;
        case AVC_REBOOT_IN_PROGRESS:        result = "Reboot in progress";      break;
        case AVC_CONNECTION_PENDING:        result = "Connection pending";      break;
        case AVC_CONNECTION_IN_PROGRESS:    result = "Connection in progress";  break;
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
 * Convert lwm2mcore update type to AVC update type
 */
//--------------------------------------------------------------------------------------------------
static le_avc_UpdateType_t ConvertToAvcType
(
    lwm2mcore_UpdateType_t type             ///< [IN] Lwm2mcore update type
)
{
    le_avc_UpdateType_t avcType;

    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            avcType = LE_AVC_FIRMWARE_UPDATE;
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            avcType = LE_AVC_APPLICATION_UPDATE;
            break;

        default:
            avcType = LE_AVC_UNKNOWN_UPDATE;
            break;
    }

    return avcType;
}

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
 * Start the defer timer.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t StartDeferTimer
(
    le_avc_UserAgreement_t userAgreement,   ///< [IN] Operation for which defer timer is launched
    uint32_t               deferMinutes     ///< [IN] Defer time in minutes
)
{
    le_timer_Ref_t timerToStart;
    le_clk_Time_t interval = { .sec = (deferMinutes * SECONDS_IN_A_MIN) };

    switch (userAgreement)
    {
        case LE_AVC_USER_AGREEMENT_CONNECTION:
            LE_INFO("Deferring connection for %d minutes", deferMinutes);
            timerToStart = ConnectDeferTimer;
            break;
        case LE_AVC_USER_AGREEMENT_DOWNLOAD:
            LE_INFO("Deferring download for %d minutes", deferMinutes);
            // Stop activity timer when download has been deferred
            avcClient_StopActivityTimer();
            timerToStart = DownloadDeferTimer;
            break;
        case LE_AVC_USER_AGREEMENT_INSTALL:
            LE_INFO("Deferring install for %d minutes", deferMinutes);
            // Stop activity timer when installation has been deferred
            avcClient_StopActivityTimer();
            timerToStart = InstallDeferTimer;
            break;
        case LE_AVC_USER_AGREEMENT_UNINSTALL:
            LE_INFO("Deferring uninstall for %d minutes", deferMinutes);
            // Stop activity timer when uninstall has been deferred
            avcClient_StopActivityTimer();
            timerToStart = UninstallDeferTimer;
            break;
        case LE_AVC_USER_AGREEMENT_REBOOT:
            LE_INFO("Deferring reboot for %d minutes", deferMinutes);
            // Stop activity timer when reboot has been deferred
            avcClient_StopActivityTimer();
            timerToStart = RebootDeferTimer;
            break;
        default:
            LE_ERROR("Unknown operation");
            return LE_FAULT;
    }

    le_timer_SetInterval(timerToStart, interval);
    le_timer_Start(timerToStart);
    return LE_OK;
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
        default:
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

    if (LE_AVC_DM_SESSION == le_avc_GetSessionType())
    {
        LE_DEBUG("Accept a package download while the device is connected to the server");
        // Notify the registered handler to proceed with the download; only called once.
        if (NULL != QueryDownloadHandlerRef)
        {
            CurrentState = AVC_DOWNLOAD_IN_PROGRESS;
            QueryDownloadHandlerRef(PkgDownloadCtx.uri, PkgDownloadCtx.type, PkgDownloadCtx.resume);
            QueryDownloadHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Download handler not valid");
            CurrentState = AVC_IDLE;
            return LE_FAULT;
        }
    }
    else
    {
        LE_DEBUG("Accept a package download while the device is not connected to the server");
        // When the device is connected, the package download will be launched by sending again
        // a download pending request. Reset the current download pending request.
        DownloadAgreement = true;
        QueryDownloadHandlerRef = NULL;
        CurrentState = AVC_IDLE;
        // Connect to the server.
        if (LE_OK != avcServer_StartSession())
        {
            LE_ERROR("Failed to start a new session");
            return LE_FAULT;
        }
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
    // If an user app is blocking the update, then just defer for some time.  Hopefully, the
    // next time this function is called, the user app will no longer be blocking the update.
    if (BlockRefCount > 0)
    {
        StartDeferTimer(LE_AVC_USER_AGREEMENT_INSTALL, BLOCKED_DEFER_TIME);
    }
    else
    {
        StopDeferTimer(LE_AVC_USER_AGREEMENT_INSTALL);

        // Notify the registered handler to proceed with the install; only called once.
        if (QueryInstallHandlerRef != NULL)
        {
            CurrentState = AVC_INSTALL_IN_PROGRESS;
            QueryInstallHandlerRef(PkgInstallCtx.type, PkgInstallCtx.instanceId);
            QueryInstallHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Install handler not valid");
            CurrentState = AVC_IDLE;
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
    if (BlockRefCount > 0)
    {
        StartDeferTimer(LE_AVC_USER_AGREEMENT_UNINSTALL, BLOCKED_DEFER_TIME);
    }
    else
    {
        StopDeferTimer(LE_AVC_USER_AGREEMENT_UNINSTALL);

        // Notify the registered handler to proceed with the uninstall; only called once.
        if (QueryUninstallHandlerRef != NULL)
        {
            CurrentState = AVC_UNINSTALL_IN_PROGRESS;
            QueryUninstallHandlerRef(SwUninstallCtx.instanceId);
            QueryUninstallHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Uninstall handler not valid");
            CurrentState = AVC_IDLE;
            return LE_FAULT;
        }
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending device reboot.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AcceptDeviceReboot
(
    void
)
{
    LE_DEBUG("Accept a device reboot");

    StopDeferTimer(LE_AVC_USER_AGREEMENT_REBOOT);

    // Run the reset timer to proceed with the reboot on expiry
    if (QueryRebootHandlerRef != NULL)
    {
        CurrentState = AVC_REBOOT_IN_PROGRESS;

        // Launch reboot function after 2 seconds
        le_clk_Time_t interval = { .sec = 2 };

        le_timer_SetInterval(LaunchRebootTimer, interval);
        le_timer_Start(LaunchRebootTimer);
    }
    else
    {
        LE_ERROR("Reboot handler not valid");
        CurrentState = AVC_IDLE;
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending connection to the server.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AcceptPendingConnection
(
    void
)
{
    StopDeferTimer(LE_AVC_USER_AGREEMENT_CONNECTION);

    CurrentState = AVC_CONNECTION_IN_PROGRESS;

    le_result_t result = avcServer_StartSession();

    if (LE_OK != result)
    {
        LE_ERROR("Error accepting connection: %s", LE_RESULT_TXT(result));
        return LE_FAULT;
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
    int32_t progress,               ///< [IN] Progress in percent
    void* contextPtr                ///< [IN] Context
)
{
    UpdateStatusData_t eventData;

    // Initialize the event data
    eventData.updateStatus = updateStatus;
    eventData.totalNumBytes = totalNumBytes;
    eventData.progress = progress;
    eventData.contextPtr = contextPtr;

    LE_DEBUG("Reporting %s", AvcSessionStateToStr(updateStatus));
    LE_DEBUG("Number of bytes to download %d", eventData.totalNumBytes);
    LE_DEBUG("Progress %d", eventData.progress);
    LE_DEBUG("ContextPtr %p", eventData.contextPtr);

    // Send the event to interested applications
    le_event_Report(UpdateStatusEvent, &eventData, sizeof(eventData));
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

    if (LE_OK != le_avc_GetUserAgreement(LE_AVC_USER_AGREEMENT_CONNECTION, &isUserAgreementEnabled))
    {
        // Use default configuration if read fails
        LE_WARN("Using default user agreement configuration");
        isUserAgreementEnabled = DISABLE_CONNECTION_NOTIFICATION;
    }

    if (!isUserAgreementEnabled)
    {
        // There is no control app; automatically accept any pending reboot
        LE_INFO("Automatically accepting connect");
        result = AcceptPendingConnection();
    }
    else if (NumStatusHandlers > 0)
    {
        // Notify registered control app.
        SendUpdateStatusEvent(LE_AVC_CONNECTION_PENDING, -1, -1, StatusHandlerContextPtr);
    }
    else
    {
        // No handler is registered, just ignore the notification.
        // The notification to send will be checked again when the control app registers a handler.
        LE_INFO("Ignoring connection pending notification, waiting for a registered handler");
        CurrentState = AVC_IDLE;
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

    // Check if download was already accepted.
    // This is necessary if an interrupted download was accepted without connection: accepting it
    // triggers a connection, and afterwards the download should start without user agreement.
    if (DownloadAgreement)
    {
        return AcceptDownloadPackage();
    }

    // Otherwise check user agreement
    if (LE_OK != le_avc_GetUserAgreement(LE_AVC_USER_AGREEMENT_DOWNLOAD, &isUserAgreementEnabled))
    {
        // Use default configuration if read fails
        LE_WARN("Using default user agreement configuration");
        isUserAgreementEnabled = USER_AGREEMENT_DEFAULT;
    }

    if (!isUserAgreementEnabled)
    {
        LE_INFO("Automatically accepting download");
        result = AcceptDownloadPackage();
    }
    else if (NumStatusHandlers > 0)
    {
        // Notify registered control app.
        SendUpdateStatusEvent(LE_AVC_DOWNLOAD_PENDING,
                              totalNumBytes,
                              dloadProgress,
                              StatusHandlerContextPtr);
    }
    else
    {
        // No handler is registered, just ignore the notification.
        // The notification to send will be checked again when the control app registers a handler.
        LE_INFO("Ignoring download pending notification, waiting for a registered handler");
        CurrentState = AVC_IDLE;
        QueryDownloadHandlerRef = NULL;
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

    if (LE_OK != le_avc_GetUserAgreement(LE_AVC_USER_AGREEMENT_INSTALL, &isUserAgreementEnabled))
    {
        // Use default configuration if read fails
        LE_WARN("Using default user agreement configuration");
        isUserAgreementEnabled = USER_AGREEMENT_DEFAULT;
    }

    if (!isUserAgreementEnabled)
    {
        LE_INFO("Automatically accepting install");
        result = AcceptInstallPackage();
    }
    else if (NumStatusHandlers > 0)
    {
        // Notify registered control app.
        SendUpdateStatusEvent(LE_AVC_INSTALL_PENDING, -1, -1, StatusHandlerContextPtr);
    }
    else
    {
        // No handler is registered, just ignore the notification.
        // The notification to send will be checked again when the control app registers a handler.
        LE_INFO("Ignoring install pending notification, waiting for a registered handler");
        CurrentState = AVC_IDLE;
        QueryInstallHandlerRef = NULL;
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

    LE_INFO("Stopping activity timer during uninstall pending.");
    avcClient_StopActivityTimer();

    if (LE_OK != le_avc_GetUserAgreement(LE_AVC_USER_AGREEMENT_UNINSTALL, &isUserAgreementEnabled))
    {
        // Use default configuration if read fails
        LE_WARN("Using default user agreement configuration");
        isUserAgreementEnabled = USER_AGREEMENT_DEFAULT;
    }

    if (!isUserAgreementEnabled)
    {
        LE_INFO("Automatically accepting uninstall");
        result = AcceptUninstallApplication();
    }
    else if (NumStatusHandlers > 0)
    {
        // Notify registered control app.
        SendUpdateStatusEvent(LE_AVC_UNINSTALL_PENDING, -1, -1, StatusHandlerContextPtr);
    }
    else
    {
        // No handler is registered, just ignore the notification.
        // The notification to send will be checked again when the control app registers a handler.
        LE_INFO("Ignoring uninstall pending notification, waiting for a registered handler");
        CurrentState = AVC_IDLE;
        QueryUninstallHandlerRef = NULL;
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

    LE_INFO("Stopping activity timer during reboot pending.");
    avcClient_StopActivityTimer();

    if (LE_OK != le_avc_GetUserAgreement(LE_AVC_USER_AGREEMENT_REBOOT, &isUserAgreementEnabled))
    {
        // Use default configuration if read fails
        LE_WARN("Using default user agreement configuration");
        isUserAgreementEnabled = USER_AGREEMENT_DEFAULT;
    }

     if (!isUserAgreementEnabled)
     {
         // There is no control app; automatically accept any pending reboot
         LE_INFO("Automatically accepting reboot");
         result = AcceptDeviceReboot();
     }
     else if (NumStatusHandlers > 0)
     {
         // Notify registered control app.
         SendUpdateStatusEvent(LE_AVC_REBOOT_PENDING, -1, -1, StatusHandlerContextPtr);
     }
     else
     {
         // No handler is registered, just ignore the notification.
         // The notification to send will be checked again when the control app registers a handler.
         LE_INFO("Ignoring reboot pending notification, waiting for a registered handler");
         CurrentState = AVC_IDLE;
         QueryRebootHandlerRef = NULL;
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
    // If the notification sent above is session started, the following block will send
    // another notification reporting the pending states.
    if (LE_AVC_SESSION_STARTED == updateStatus)
    {
        CurrentTotalNumBytes = -1;
        CurrentDownloadProgress = -1;

        // The currentState is really the previous state in case of session start, as we don't
        // do a state change.
        switch (CurrentState)
        {
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
    le_avc_Status_t            updateStatus,     ///< [IN] Update status
    int32_t                    totalNumBytes,    ///< [IN] Remaining number of bytes to download
    int32_t                    dloadProgress,    ///< [IN] Download progress
    le_avc_StatusHandlerFunc_t statusHandlerPtr, ///< [IN] Pointer on handler function
    void*                      contextPtr        ///< [IN] Context
)
{
    le_result_t result = LE_BUSY;

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

        case LE_AVC_SESSION_STOPPED:
            // Forward notifications unrelated to user agreement to interested applications.
            SendUpdateStatusEvent(updateStatus,
                                  totalNumBytes,
                                  dloadProgress,
                                  StatusHandlerContextPtr);

            // Report download pending user agreement again if the network was dropped when the
            // download was complete but was unable to send the update result to the server.
            if (CurrentState == AVC_DOWNLOAD_COMPLETE)
            {
                CurrentState = AVC_DOWNLOAD_PENDING;
                SendUpdateStatusEvent(LE_AVC_DOWNLOAD_PENDING,
                                      -1,
                                      -1,
                                      StatusHandlerContextPtr);
            }
            break;

        default:
            // Forward notifications unrelated to user agreement to interested applications.
            if (NULL != statusHandlerPtr)
            {
                LE_INFO("Forward notification directly to application");
                statusHandlerPtr(updateStatus,
                                 totalNumBytes,
                                 dloadProgress,
                                 contextPtr);
            }
            else
            {
                LE_INFO("Broadcast notification to applications");
                SendUpdateStatusEvent(updateStatus,
                                      totalNumBytes,
                                      dloadProgress,
                                      StatusHandlerContextPtr);
            }

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
static void ProcessUpdateStatus
(
    void* contextPtr
)
{
    AvcUpdateStatusData_t* data = (AvcUpdateStatusData_t*)contextPtr;

    // Keep track of the state of any pending downloads or installs.
    switch (data->updateStatus)
    {
        case LE_AVC_CONNECTION_PENDING:
            CurrentState = AVC_CONNECTION_PENDING;
            break;

        case LE_AVC_REBOOT_PENDING:
            CurrentState = AVC_REBOOT_PENDING;
            break;

        case LE_AVC_DOWNLOAD_PENDING:
            LE_DEBUG("Update type for DOWNLOAD is %d", data->updateType);
            CurrentState = AVC_DOWNLOAD_PENDING;
            CurrentDownloadProgress = data->progress;
            CurrentTotalNumBytes = data->totalNumBytes;
            if (LE_AVC_UNKNOWN_UPDATE != data->updateType)
            {
                CurrentUpdateType = data->updateType;
            }
            break;

        case LE_AVC_DOWNLOAD_IN_PROGRESS:
            LE_DEBUG("Update type for DOWNLOAD is %d", data->updateType);
            CurrentTotalNumBytes = data->totalNumBytes;
            CurrentDownloadProgress = data->progress;
            CurrentUpdateType = data->updateType;

            if ((LE_AVC_APPLICATION_UPDATE == data->updateType) && (data->totalNumBytes >= 0))
            {
                // Set the bytes downloaded to workspace for resume operation
                avcApp_SetSwUpdateBytesDownloaded();
            }
            break;

        case LE_AVC_DOWNLOAD_COMPLETE:
            LE_DEBUG("Update type for DOWNLOAD is %d", data->updateType);
            if (data->totalNumBytes > 0)
            {
                CurrentTotalNumBytes = data->totalNumBytes;
            }
            else
            {
                // Use last stored value
                data->totalNumBytes = CurrentTotalNumBytes;
            }
            if (data->progress > 0)
            {
                CurrentDownloadProgress = data->progress;
            }
            else
            {
                // Use last stored value
                data->progress = CurrentDownloadProgress;
            }
            CurrentUpdateType = data->updateType;

            CurrentState = AVC_DOWNLOAD_COMPLETE;
            avcClient_StartActivityTimer();
            DownloadAgreement = false;

            if (LE_AVC_APPLICATION_UPDATE == data->updateType)
            {
                // Set the bytes downloaded to workspace for resume operation
                avcApp_SetSwUpdateBytesDownloaded();

                // End download and start unpack
                avcApp_EndDownload();
            }
            break;

        case LE_AVC_INSTALL_PENDING:
            LE_DEBUG("Update type for INSTALL is %d", data->updateType);
            CurrentState = AVC_INSTALL_PENDING;
            if (LE_AVC_UNKNOWN_UPDATE != data->updateType)
            {
                // If the device resets during a FOTA download, then the CurrentUpdateType is lost
                // and needs to be assigned again. Since we don't easily know if a reset happened,
                // re-assign the value if possible.
                CurrentUpdateType = data->updateType;
            }
            break;

        case LE_AVC_UNINSTALL_PENDING:
            CurrentState = AVC_UNINSTALL_PENDING;
            if (LE_AVC_UNKNOWN_UPDATE != data->updateType)
            {
                LE_DEBUG("Update type for UNINSTALL is %d", data->updateType);
                CurrentUpdateType = data->updateType;
            }
            break;

        case LE_AVC_INSTALL_IN_PROGRESS:
        case LE_AVC_UNINSTALL_IN_PROGRESS:
            avcClient_StopActivityTimer();
            break;

        case LE_AVC_DOWNLOAD_FAILED:
        case LE_AVC_INSTALL_FAILED:
            // There is no longer any current update, so go back to idle
            CurrentState = AVC_IDLE;
            if (LE_AVC_APPLICATION_UPDATE == data->updateType)
            {
                avcApp_DeletePackage();
            }

            avcClient_StartActivityTimer();
            AvcErrorCode = data->errorCode;
            break;

        case LE_AVC_UNINSTALL_FAILED:
            // There is no longer any current update, so go back to idle
            CurrentState = AVC_IDLE;

            avcClient_StartActivityTimer();
            AvcErrorCode = data->errorCode;
            break;

        case LE_AVC_NO_UPDATE:
        case LE_AVC_INSTALL_COMPLETE:
        case LE_AVC_UNINSTALL_COMPLETE:
            // There is no longer any current update, so go back to idle
            CurrentState = AVC_IDLE;
            break;

        case LE_AVC_SESSION_STARTED:
            avcClient_StartActivityTimer();
            // Update object9 list managed by legato to lwm2mcore
            avcApp_NotifyObj9List();
            avData_ReportSessionState(LE_AVDATA_SESSION_STARTED);

            // Push items waiting in queue
            push_Retry();
            break;

        case LE_AVC_SESSION_STOPPED:
            DownloadAgreement = false;
            avcClient_StopActivityTimer();
            // These events do not cause a state transition
            avData_ReportSessionState(LE_AVDATA_SESSION_STOPPED);
            break;

        case LE_AVC_SESSION_FAILED:
            LE_DEBUG("Session failed");
            break;

        case LE_AVC_AUTH_STARTED:
            LE_DEBUG("Authentication started");
            break;

        case LE_AVC_AUTH_FAILED:
            LE_DEBUG("Authentication failed");
            break;

        case LE_AVC_SESSION_BS_STARTED:
            LE_DEBUG("Session with bootstrap server started");
            break;

        case LE_AVC_CERTIFICATION_OK:
            LE_DEBUG("Package certified");
            break;

        case LE_AVC_CERTIFICATION_KO:
            LE_DEBUG("Package not certified");
            break;

        default:
            LE_WARN("Unhandled updateStatus %d", data->updateStatus);
            return;
    }

    // Process user agreement or forward to control app if applicable.
    ProcessUserAgreement(data->updateStatus,
                         data->totalNumBytes,
                         data->progress,
                         data->clientData.statusHandlerPtr,
                         data->clientData.contextPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Send update status notifications to AVC server
 */
//--------------------------------------------------------------------------------------------------
void avcServer_UpdateStatus
(
    le_avc_Status_t              updateStatus,     ///< Update status
    le_avc_UpdateType_t          updateType,       ///< Update type
    int32_t                      totalNumBytes,    ///< Total number of bytes to download (-1 if
                                                   ///< not set)
    int32_t                      progress,         ///< Progress in percent (-1 if not set)
    le_avc_ErrorCode_t           errorCode,        ///< Error code
    le_avc_StatusHandlerFunc_t   statusHandlerPtr, ///< Pointer on handler function
    void*                        contextPtr        ///< Context
)
{
    AvcUpdateStatusData_t updateStatusData;

    updateStatusData.updateStatus                = updateStatus;
    updateStatusData.updateType                  = updateType;
    updateStatusData.totalNumBytes               = totalNumBytes;
    updateStatusData.progress                    = progress;
    updateStatusData.errorCode                   = errorCode;
    updateStatusData.clientData.statusHandlerPtr = statusHandlerPtr;
    updateStatusData.clientData.contextPtr       = contextPtr;

    le_event_Report(AvcUpdateStatusEvent, &updateStatusData, sizeof(updateStatusData));
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
 * Called when the download defer timer expires.
 */
//--------------------------------------------------------------------------------------------------
static void DownloadTimerExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    avcServer_UpdateStatus(LE_AVC_DOWNLOAD_PENDING,
                           ConvertToAvcType(PkgDownloadCtx.type),
                           PkgDownloadCtx.bytesToDownload,
                           0,
                           LE_AVC_ERR_NONE,
                           NULL,
                           NULL
                          );
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
    avcServer_UpdateStatus(LE_AVC_INSTALL_PENDING,
                           ConvertToAvcType(PkgInstallCtx.type),
                           -1,
                           -1,
                           LE_AVC_ERR_NONE,
                           NULL,
                           NULL
                          );
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
    avcServer_UpdateStatus(LE_AVC_UNINSTALL_PENDING,
                           LE_AVC_APPLICATION_UPDATE,
                           -1,
                           -1,
                           LE_AVC_ERR_NONE,
                           NULL,
                           NULL
                          );
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
    avcServer_UpdateStatus(LE_AVC_REBOOT_PENDING,
                           LE_AVC_UNKNOWN_UPDATE,
                           -1,
                           -1,
                           LE_AVC_ERR_NONE,
                           NULL,
                           NULL
                          );
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
    avcServer_UpdateStatus(LE_AVC_CONNECTION_PENDING,
                           LE_AVC_UNKNOWN_UPDATE,
                           -1,
                           -1,
                           LE_AVC_ERR_NONE,
                           NULL,
                           NULL
                          );
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
    avcServer_StartSession();
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the launch reboot timer expires
 */
//--------------------------------------------------------------------------------------------------
static void LaunchRebootExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    LE_DEBUG("Rebooting the device...");
    if (QueryRebootHandlerRef != NULL)
    {
        QueryRebootHandlerRef();
        QueryRebootHandlerRef = NULL;
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
    le_result_t result;
    size_t size = sizeof(AvcConfigData_t);

    if (NULL == configPtr)
    {
        LE_ERROR("Avc configuration pointer is null");
        return LE_FAULT;
    }

    result = WriteFs(AVC_CONFIG_FILE, (uint8_t*)configPtr, size);

    if (LE_OK == result)
    {
        return LE_OK;
    }
    else
    {
        LE_ERROR("Error writing to %s", AVC_CONFIG_FILE);
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
    le_result_t result;

    if (NULL == configPtr)
    {
        LE_ERROR("Avc configuration pointer is null");
        return LE_FAULT;
    }

    size_t size = sizeof(AvcConfigData_t);
    result = ReadFs(AVC_CONFIG_FILE, (uint8_t*)configPtr, &size);

    if (LE_OK == result)
    {
        return LE_OK;
    }
    else
    {
        LE_ERROR("Error reading from %s", AVC_CONFIG_FILE);
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
    if (LE_DUPLICATE == avcServer_StartSession())
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
      le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    LE_INFO("Polling timer expired");

    SaveCurrentEpochTime();

    ConnectToServer();

    // Restart the timer for the next interval
    uint32_t pollingTimerInterval;
    if (LE_OK != le_avc_GetPollingTimer(&pollingTimerInterval))
    {
        LE_ERROR("Unable to get the polling time interval");
        return;
    }

    if (POLLING_TIMER_DISABLED != pollingTimerInterval)
    {
        LE_INFO("A connection to server will be made in %d minutes", pollingTimerInterval);
        le_clk_Time_t interval = {.sec = pollingTimerInterval * SECONDS_IN_A_MIN};
        LE_ASSERT(LE_OK == le_timer_SetInterval(PollingTimerRef, interval));
        LE_ASSERT(LE_OK == le_timer_Start(PollingTimerRef));
    }
    else
    {
        LE_INFO("Polling disabled");
    }
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
 * In case a firmware was installed, check the install result and update the firmware update state
 * and result accordingly.
 *
 * @return
 *      - LE_OK     The function succeeded
 *      - LE_FAULT  An error occurred
 */
//--------------------------------------------------------------------------------------------------
static le_result_t CheckFwInstallResult
(
   le_avc_StatusHandlerFunc_t statusHandlerPtr,  ///< Pointer on handler function
   void*                      contextPtr         ///< Context
)
{
    lwm2mcore_FwUpdateState_t fwUpdateState = LWM2MCORE_FW_UPDATE_STATE_IDLE;
    lwm2mcore_FwUpdateResult_t fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;

    // Check if a FW update was ongoing
    if (   (LE_OK == packageDownloader_GetFwUpdateState(&fwUpdateState))
        && (LE_OK == packageDownloader_GetFwUpdateResult(&fwUpdateResult))
        && (LWM2MCORE_FW_UPDATE_STATE_UPDATING == fwUpdateState)
        && (LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL == fwUpdateResult)
       )
    {
        // Retrieve FW update result
        le_fwupdate_UpdateStatus_t fwUpdateStatus;
        lwm2mcore_FwUpdateResult_t newFwUpdateResult;
        char statusStr[LE_FWUPDATE_STATUS_LABEL_LENGTH_MAX];
        le_avc_ErrorCode_t errorCode;
        le_avc_Status_t updateStatus;

        if (LE_OK != le_fwupdate_GetUpdateStatus(&fwUpdateStatus, statusStr, sizeof(statusStr)))
        {
            LE_ERROR("Error while reading the FW update status");
            return LE_FAULT;
        }

        LE_DEBUG("Update status: %s (%d)", statusStr, fwUpdateStatus);

        // Set the update state to IDLE in all cases
        if (DWL_OK != packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE))
        {
            LE_ERROR("Error while setting FW update state");
            return LE_FAULT;
        }

        // Set the update result according to the FW update status
        if (LE_FWUPDATE_UPDATE_STATUS_OK == fwUpdateStatus)
        {
            newFwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_INSTALLED_SUCCESSFUL;
            updateStatus = LE_AVC_INSTALL_COMPLETE;
            errorCode = LE_AVC_ERR_NONE;
        }
        else
        {
            newFwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_INSTALL_FAILURE;
            updateStatus = LE_AVC_INSTALL_FAILED;
            if (LE_FWUPDATE_UPDATE_STATUS_PARTITION_ERROR == fwUpdateStatus)
            {
                errorCode = LE_AVC_ERR_BAD_PACKAGE;
            }
            else
            {
                errorCode = LE_AVC_ERR_INTERNAL;
            }
        }
        avcServer_UpdateStatus(updateStatus,
                               LE_AVC_FIRMWARE_UPDATE,
                               -1,
                               -1,
                               errorCode,
                               statusHandlerPtr,
                               contextPtr
                              );
        packageDownloader_SetFwUpdateNotification(true, updateStatus, errorCode);
        LE_DEBUG("Set FW update result to %d", newFwUpdateResult);
        if (DWL_OK != packageDownloader_SetFwUpdateResult(newFwUpdateResult))
        {
            LE_ERROR("Error while setting FW update result");
            return LE_FAULT;
        }
    }
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if a notification needs to be sent to the application after a reboot, a service restart or
 * a new registration to the event handler
 */
//--------------------------------------------------------------------------------------------------
static void CheckNotificationToSend
(
   le_avc_StatusHandlerFunc_t statusHandlerPtr,  ///< Pointer on handler function
   void*                      contextPtr         ///< Context
)
{
    bool notify = false;
    uint64_t numBytesToDownload = 0;
    le_avc_Status_t avcStatus;
    le_avc_ErrorCode_t errorCode;

    if (AVC_IDLE != CurrentState)
    {
        // Since FW install result notification is not reported when auto-connect is performed at
        // startup, this notification needs to be resent again to the newly registred applications
        avcStatus = LE_AVC_NO_UPDATE;
        errorCode = LE_AVC_ERR_NONE;
        if ((LE_OK == packageDownloader_GetFwUpdateNotification(&notify, &avcStatus, &errorCode))
            && (notify))
        {
            avcServer_UpdateStatus(avcStatus,
                                   LE_AVC_FIRMWARE_UPDATE,
                                   -1,
                                   -1,
                                   errorCode,
                                   statusHandlerPtr,
                                   contextPtr);
            LE_DEBUG("Reporting FW install notification (status: avcStatus)");
            return;
        }

        LE_DEBUG("Current state is %s, not checking notification to send",
                 ConvertAvcStateToString(CurrentState));

        // Something is already going on, no need to check the notification to send
        if ((CurrentState != AVC_DOWNLOAD_PENDING) &&
            (CurrentState != AVC_INSTALL_PENDING) &&
            (CurrentState != AVC_CONNECTION_PENDING))
        {
            return;
        }

        // Resend the notification as earlier notification might have been lost if
        // a receiving application was not yet listening.
    }

    // 1. Check if a connection is required to finish an ongoing FOTA:
    // check FW install result and notification flag
    if (LE_OK == CheckFwInstallResult(statusHandlerPtr, contextPtr))
    {
        notify = false;
        avcStatus = LE_AVC_NO_UPDATE;
        errorCode = LE_AVC_ERR_NONE;
        if ((LE_OK == packageDownloader_GetFwUpdateNotification(&notify, &avcStatus, &errorCode))
            && (notify))
        {
            avcServer_UpdateStatus(avcStatus,
                                   LE_AVC_FIRMWARE_UPDATE,
                                   -1,
                                   -1,
                                   errorCode,
                                   statusHandlerPtr,
                                   contextPtr);
            avcServer_QueryConnection(LE_AVC_FIRMWARE_UPDATE, statusHandlerPtr, contextPtr);
            return;
        }
    }

    // 2. Check if a FOTA install pending notification should be sent
    notify = false;
    if ((LE_OK == packageDownloader_GetFwUpdateInstallPending(&notify)) && (notify))
    {
        ResumeFwInstall();
        return;
    }

    // 3. Check if a SOTA install/uninstall pending notification should be sent
    if (LE_BUSY == avcApp_CheckNotificationToSend())
    {
        return;
    }

    // 4. Check if a download pending notification should be sent
    if (LE_OK == packageDownloader_BytesLeftToDownload(&numBytesToDownload))
    {
        LE_DEBUG("Bytes left to download: %"PRIu64, numBytesToDownload);

        char downloadUri[LWM2MCORE_PACKAGE_URI_MAX_BYTES];
        size_t uriSize = LWM2MCORE_PACKAGE_URI_MAX_BYTES;
        lwm2mcore_UpdateType_t updateType = LWM2MCORE_MAX_UPDATE_TYPE;
        memset(downloadUri, 0, sizeof(downloadUri));

        // Retrieve resume information if download is not complete
        if (numBytesToDownload)
        {
            if (LE_OK != packageDownloader_GetResumeInfo(downloadUri, &uriSize, &updateType))
            {
                LE_DEBUG("No download to resume.");
                return;
            }
        }

        if (QueryDownloadHandlerRef == NULL)
        {
            // Request user agreement for download
            avcServer_QueryDownload(packageDownloader_StartDownload,
                                    numBytesToDownload,
                                    updateType,
                                    downloadUri,
                                    true,
                                    statusHandlerPtr,
                                    contextPtr
                                   );
        }
        else
        {
            LE_DEBUG("Resending the download indication");
            avcServer_UpdateStatus(LE_AVC_DOWNLOAD_PENDING,
                                   ConvertToAvcType(PkgDownloadCtx.type),
                                   PkgDownloadCtx.bytesToDownload,
                                   0,
                                   LE_AVC_ERR_NONE,
                                   statusHandlerPtr,
                                   contextPtr
                                   );
        }
    }
    return;
}

//--------------------------------------------------------------------------------------------------
// Internal interface functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a server connection.
 *
 * For FOTA it should be called only after reboot, and for SOTA it should be called after the
 * update finishes. However, this function will request a connection to the server only if there
 * is no session going on.
 * If the connection can proceed right away, it will be launched.
 */
//--------------------------------------------------------------------------------------------------
void avcServer_QueryConnection
(
    le_avc_UpdateType_t        updateType,        ///< Update type
    le_avc_StatusHandlerFunc_t statusHandlerPtr,  ///< Pointer on handler function
    void*                      contextPtr         ///< Context
)
{
    if (LE_AVC_SESSION_INVALID != le_avc_GetSessionType())
    {
        LE_INFO("Session is already going on");
        return;
    }

    switch (updateType)
    {
        case LE_AVC_FIRMWARE_UPDATE:
            LE_DEBUG("Reporting status LE_AVC_CONNECTION_PENDING for FOTA");
            avcServer_UpdateStatus(LE_AVC_CONNECTION_PENDING,
                                   LE_AVC_FIRMWARE_UPDATE,
                                   -1,
                                   -1,
                                   LE_AVC_ERR_NONE,
                                   statusHandlerPtr,
                                   contextPtr
                                  );
            break;

        case LE_AVC_APPLICATION_UPDATE:
            LE_DEBUG("Reporting status LE_AVC_CONNECTION_PENDING for SOTA");
            avcServer_UpdateStatus(LE_AVC_CONNECTION_PENDING,
                                   LE_AVC_APPLICATION_UPDATE,
                                   -1,
                                   -1,
                                   LE_AVC_ERR_NONE,
                                   statusHandlerPtr,
                                   contextPtr
                                  );
            break;

        default:
            LE_ERROR("Unsupported updateType: %s", UpdateTypeToStr(updateType));
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with an application install.
 *
 * If an install can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with an install. Note that handlerRef will be called at most once.
 * If an install can proceed right away, it will be launched.
 *
 * @return None
 */
//--------------------------------------------------------------------------------------------------
void avcServer_QueryInstall
(
    avcServer_InstallHandlerFunc_t handlerRef,  ///< [IN] Handler to receive install response.
    lwm2mcore_UpdateType_t  type,               ///< [IN] update type.
    uint16_t instanceId                         ///< [IN] instance id (0 for fw install).
)
{
    if (NULL == QueryInstallHandlerRef)
    {
        // Update install handler
        CurrentUpdateType = ConvertToAvcType(type);
        PkgInstallCtx.type = type;
        PkgInstallCtx.instanceId = instanceId;
        QueryInstallHandlerRef = handlerRef;
    }

    avcServer_UpdateStatus(LE_AVC_INSTALL_PENDING,
                           ConvertToAvcType(PkgInstallCtx.type),
                           -1,
                           -1,
                           LE_AVC_ERR_NONE,
                           NULL,
                           NULL
                          );
}

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a package download.
 *
 * If a download can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with a download. Note that handlerRef will be called at most once.
 * If a download can proceed right away, it will be launched.
 *
 * @return None
 */
//--------------------------------------------------------------------------------------------------
void avcServer_QueryDownload
(
    avcServer_DownloadHandlerFunc_t handlerFunc,    ///< [IN] Download handler function
    uint64_t bytesToDownload,                       ///< [IN] Number of bytes to download
    lwm2mcore_UpdateType_t type,                    ///< [IN] Update type
    char* uriPtr,                                   ///< [IN] Update package URI
    bool resume,                                    ///< [IN] Is it a download resume?
    le_avc_StatusHandlerFunc_t statusHandlerPtr,    ///< Pointer on handler function
    void*                      contextPtr           ///< Context
)
{
    if (NULL != QueryDownloadHandlerRef)
    {
        LE_ERROR("Duplicate download attempt");
        return;
    }

    // Update download handler
    QueryDownloadHandlerRef = handlerFunc;
    memset(&PkgDownloadCtx, 0, sizeof(PkgDownloadCtx));
    PkgDownloadCtx.bytesToDownload = bytesToDownload;
    PkgDownloadCtx.type = type;
    memcpy(PkgDownloadCtx.uri, uriPtr, strlen(uriPtr));
    PkgDownloadCtx.resume = resume;

    avcServer_UpdateStatus( LE_AVC_DOWNLOAD_PENDING,
                            ConvertToAvcType(PkgDownloadCtx.type),
                            PkgDownloadCtx.bytesToDownload,
                            0,
                            LE_AVC_ERR_NONE,
                            statusHandlerPtr,
                            contextPtr
                           );
}

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a device reboot.
 *
 * If a reboot can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with a reboot. Note that handlerRef will be called at most once.
 *
 * If a reboot can proceed right away, a 2-second timer is immediatly launched and the handlerRef
 * function will be called when the timer expires.
 * @return None
 */
//--------------------------------------------------------------------------------------------------
void avcServer_QueryReboot
(
    avcServer_RebootHandlerFunc_t handlerFunc   ///< [IN] Reboot handler function.
)
{
    if (NULL != QueryRebootHandlerRef)
    {
        LE_ERROR("Duplicate reboot attempt");
        return;
    }

    // Update reboot handler
    QueryRebootHandlerRef = handlerFunc;

    avcServer_UpdateStatus(LE_AVC_REBOOT_PENDING,
                           LE_AVC_UNKNOWN_UPDATE,
                           -1,
                           -1,
                           LE_AVC_ERR_NONE,
                           NULL,
                           NULL
                          );
}

//--------------------------------------------------------------------------------------------------
/**
 * Resets user agreement query handlers of download, install and uninstall. This also stops
 * corresponding defer timers.
 */
//--------------------------------------------------------------------------------------------------
void avcServer_ResetQueryHandlers
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
 * Query the AVC Server if it's okay to proceed with an application uninstall.
 *
 * If an uninstall can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with an uninstall. Note that handlerRef will be called at most once.
 * If an uninstall can proceed right away, it will be launched.
 *
 * @return None
 */
//--------------------------------------------------------------------------------------------------
void avcServer_QueryUninstall
(
    avcServer_UninstallHandlerFunc_t handlerRef,  ///< [IN] Handler to receive install response.
    uint16_t instanceId                           ///< Instance Id (0 for FW, any value for SW)
)
{
    if (NULL != QueryUninstallHandlerRef)
    {
        LE_ERROR("Duplicate uninstall attempt");
        return;
    }

    // Update uninstall handler
    SwUninstallCtx.instanceId = instanceId;
    QueryUninstallHandlerRef = handlerRef;

    avcServer_UpdateStatus(LE_AVC_UNINSTALL_PENDING,
                           LE_AVC_APPLICATION_UPDATE,
                           -1,
                           -1,
                           LE_AVC_ERR_NONE,
                           NULL,
                           NULL
                          );
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

        // Session initiated by user.
        IsUserSession = true;
        result = avcServer_StartSession();
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start a session with the AirVantage server
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_FAULT on failure
 *      - LE_DUPLICATE if an AV session is already connected.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_StartSession
(
    void
)
{
    le_result_t result = avcClient_Connect();

    if ((LE_BUSY == result) && avcClient_IsRetryTimerActive())  // Retry timer is active
    {
        avcClient_ResetRetryTimer();
        return avcClient_Connect();
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

        // Session closed by user.
        IsUserSession = false;
        result = avcClient_Disconnect(true);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Is the current state AVC_IDLE?
 */
//--------------------------------------------------------------------------------------------------
bool avcServer_IsIdle
(
    void
)
{
    if (AVC_IDLE == CurrentState)
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Is the current session initiated by user app?
 */
//--------------------------------------------------------------------------------------------------
bool avcServer_IsUserSession
(
    void
)
{
    return IsUserSession;
}

//--------------------------------------------------------------------------------------------------
/**
 * Reset the stored download agreement
 */
//--------------------------------------------------------------------------------------------------
void avcServer_ResetDownloadAgreement
(
    void
)
{
    DownloadAgreement = false;
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
                      eventDataPtr->progress,
                      le_event_GetContextPtr());
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the default AVC config
 */
//--------------------------------------------------------------------------------------------------
static void SetDefaultConfig
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
    avcConfig.ua.connect = DISABLE_CONNECTION_NOTIFICATION;
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
// API functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * le_avc_StatusHandler handler ADD function
 */
//--------------------------------------------------------------------------------------------------
le_avc_StatusEventHandlerRef_t le_avc_AddStatusEventHandler
(
    le_avc_StatusHandlerFunc_t handlerPtr,  ///< [IN] Pointer on handler function
    void* contextPtr                        ///< [IN] Context pointer
)
{
    le_event_HandlerRef_t handlerRef;

    // handlerPtr must be valid
    if (NULL == handlerPtr)
    {
        LE_KILL_CLIENT("Null handlerPtr");
        return NULL;
    }

    LE_PRINT_VALUE("%p", handlerPtr);
    LE_PRINT_VALUE("%p", contextPtr);

    // Register the user app handler
    handlerRef = le_event_AddLayeredHandler("AvcUpdateStaus",
                                            UpdateStatusEvent,
                                            FirstLayerUpdateStatusHandler,
                                            (le_event_HandlerFunc_t)handlerPtr);
    le_event_SetContextPtr(handlerRef, contextPtr);

    // Number of user apps registered
    NumStatusHandlers++;

    // Check if any notification needs to be sent to the application concerning
    // firmware update and application update.
    CheckNotificationToSend(handlerPtr, contextPtr);

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
 *      - LE_DUPLICATE if already connected to AirVantage server.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_StartSession
(
    void
)
{
    IsUserSession = true;
    StopDeferTimer(LE_AVC_USER_AGREEMENT_CONNECTION);
    return avcServer_StartSession();
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
    IsUserSession = false;
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
    if (AVC_DOWNLOAD_PENDING != CurrentState)
    {
        LE_ERROR("Expected DOWNLOAD_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
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
    if (AVC_CONNECTION_PENDING != CurrentState)
    {
        LE_ERROR("Expected CONNECTION_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
        return LE_FAULT;
    }

    return StartDeferTimer(LE_AVC_USER_AGREEMENT_CONNECTION, deferMinutes);
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
    if (AVC_DOWNLOAD_PENDING != CurrentState)
    {
        LE_ERROR("Expected DOWNLOAD_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
        return LE_FAULT;
    }

    return StartDeferTimer(LE_AVC_USER_AGREEMENT_DOWNLOAD, deferMinutes);
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
    if (AVC_INSTALL_PENDING != CurrentState)
    {
        LE_ERROR("Expected INSTALL_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
        return LE_FAULT;
    }

    // Clear the error code.
    AvcErrorCode = LE_AVC_ERR_NONE;

    if (   (LE_AVC_FIRMWARE_UPDATE == CurrentUpdateType)
        || (LE_AVC_APPLICATION_UPDATE == CurrentUpdateType))
    {
        return AcceptInstallPackage();
    }

    LE_ERROR("Unknown update type %d", CurrentUpdateType);
    return LE_FAULT;
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
    if (AVC_INSTALL_PENDING != CurrentState)
    {
        LE_ERROR("Expected INSTALL_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
        return LE_FAULT;
    }

    return StartDeferTimer(LE_AVC_USER_AGREEMENT_INSTALL, deferMinutes);
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
    if (AVC_UNINSTALL_PENDING != CurrentState)
    {
        LE_ERROR("Expected UNINSTALL_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
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
    if (AVC_UNINSTALL_PENDING != CurrentState)
    {
        LE_ERROR("Expected UNINSTALL_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
        return LE_FAULT;
    }

    return StartDeferTimer(LE_AVC_USER_AGREEMENT_UNINSTALL, deferMinutes);
}

//--------------------------------------------------------------------------------------------------
/**
 * Accept the currently pending reboot
 *
 * @note When this function is called, a 2-second timer is launched and the reboot function is
 * called when the timer expires.
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
    if (AVC_REBOOT_PENDING != CurrentState)
    {
        LE_ERROR("Expected REBOOT_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
        return LE_FAULT;
    }

    return AcceptDeviceReboot();
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
    if (AVC_REBOOT_PENDING != CurrentState)
    {
        LE_ERROR("Expected REBOOT_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
        return LE_FAULT;
    }

    return StartDeferTimer(LE_AVC_USER_AGREEMENT_REBOOT, deferMinutes);
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
    if (updateTypePtr == NULL)
    {
        LE_KILL_CLIENT("updateTypePtr is NULL.");
        return LE_FAULT;
    }

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
    if (apnName == NULL)
    {
        LE_KILL_CLIENT("apnName is NULL.");
        return LE_FAULT;
    }

    if (userName == NULL)
    {
        LE_KILL_CLIENT("userName is NULL.");
        return LE_FAULT;
    }

    if (userPassword == NULL)
    {
        LE_KILL_CLIENT("userPassword is NULL.");
        return LE_FAULT;
    }

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

    if (NULL == timerValuePtr)
    {
        LE_ERROR("Retry timer array pointer is NULL!");
        return LE_FAULT;
    }

    if (NULL == numTimers)
    {
        LE_ERROR("numTimers pointer in NULL!");
        return LE_FAULT;
    }

    if (*numTimers < LE_AVC_NUM_RETRY_TIMERS)
    {
        LE_ERROR("Supplied retry timer array too small (%zd). Expected %d.",
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
        LE_ERROR("Supplied retry timer array too small (%zd). Expected %d.",
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
    if (pollingTimerPtr == NULL)
    {
        LE_KILL_CLIENT("pollingTimerPtr is NULL.");
        return LE_FAULT;
    }

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

    if (LWM2MCORE_LIFETIME_VALUE_DISABLED == lifetime)
    {
        pollingTimerCfg = POLLING_TIMER_DISABLED;
    }
    else
    {
        // lifetime is in seconds and polling timer is in minutes
        pollingTimerCfg = lifetime / SECONDS_IN_A_MIN;
    }

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

    // 0 is not a valid value for lifetime, a specific value needs to be used
    if (POLLING_TIMER_DISABLED == lifetime)
    {
        lifetime = LWM2MCORE_LIFETIME_VALUE_DISABLED;
    }

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
    if (isEnabledPtr == NULL)
    {
        LE_KILL_CLIENT("isEnabledPtr is NULL.");
        return LE_FAULT;
    }

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
 * Function to read a resource from a LwM2M object
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if failed.
 *      - LE_UNSUPPORTED if unsupported.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_ReadLwm2mResource
(
   uint16_t objectId,               ///< [IN] Object identifier
   uint16_t objectInstanceId,       ///< [IN] Object instance identifier
   uint16_t resourceId,             ///< [IN] Resource identifier
   uint16_t resourceInstanceId,     ///< [IN] Resource instance identifier
   char* dataPtr,                   ///< [IN/OUT] String of requested resources to be read
   size_t dataSize                  ///< [IN/OUT] Size of the array
)
{
   size_t size = dataSize;

   if (!lwm2mcore_ResourceRead(objectId, objectInstanceId, resourceId, resourceInstanceId, dataPtr,
                               &size))
   {
        LE_ERROR("Unable to read the specified resource");
        return LE_FAULT;
   }

   if (0 == size)
   {
        LE_ERROR("Empty resource");
        return LE_FAULT;
   }

   dataPtr[size] = '\0';

   return LE_OK;
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
 * Handler function for wake-up SMS message reception.
 *
 */
//--------------------- ----------------------------------------------------------------------------
void RxMessageHandler
(
    le_sms_MsgRef_t msgRef,     ///< [IN] Message object received from the modem.
    void*           contextPtr  ///< [IN] The handler's context.
)
{
    char text[LE_SMS_TEXT_MAX_BYTES] = {0};

    le_sms_Format_t Format = le_sms_GetFormat(msgRef);

    switch(Format)
    {
        case LE_SMS_FORMAT_TEXT :
            le_sms_GetText(msgRef, text, sizeof(text));
            if (strncmp(text, WAKEUP_SMS_TEXT, sizeof(text)) == 0)
            {
                le_clk_Time_t CurrentTime = le_clk_GetRelativeTime();
                if (le_clk_GreaterThan(CurrentTime, WakeUpSmsTimeout))
                {
                    LE_INFO("Wakeup SMS received - starting AV session");

                    // Update the ratelimiting timeout.
                    WakeUpSmsTimeout = le_clk_Add(CurrentTime, WakeUpSmsInterval);

                    // Start the AV session. If there is no activity, it will be
                    // torn down after 20 seconds.
                    if (LE_OK != avcServer_StartSession())
                    {
                        LE_ERROR("Failed to start a new session");
                    }

                    // Cleanup - the wakeup message doesn't need to be stored.
                    if (LE_OK != le_sms_DeleteFromStorage(msgRef))
                    {
                        LE_ERROR("Error deleting wakeup SMS from storage");
                    }
                }
                else
                {
                    LE_INFO("Wakeup SMS rate exceeds limit - ignoring");
                }
            }
            // If this is not a wakeup message - do nothing.
            break;
        default :
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialization function for AVC Daemon
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // Create update status events
    AvcUpdateStatusEvent = le_event_CreateId("AVC Update Status", sizeof(AvcUpdateStatusData_t));
    UpdateStatusEvent = le_event_CreateId("Update Status", sizeof(UpdateStatusData_t));

    // Register handler for AVC service update status
    le_event_AddHandler("AVC Update Status event", AvcUpdateStatusEvent, ProcessUpdateStatus);

    // Register handler for SMS wakeup
    le_sms_AddRxMessageHandler(RxMessageHandler, NULL);

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

    LaunchRebootTimer = le_timer_Create("launch reboot timer");
    le_timer_SetHandler(LaunchRebootTimer, LaunchRebootExpiryHandler);

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

    // Write default if configuration file doesn't exist
    if(LE_OK != ExistsFs(AVC_CONFIG_FILE))
    {
        LE_INFO("Set default configuration");
        SetDefaultConfig();
    }

    // Initialize user agreement.
    avcServer_ResetQueryHandlers();

    // Clear resume data if necessary
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
    }

    // Initialize application update module
    avcApp_Init();

    // Check if any notification needs to be sent to the application concerning
    // firmware update and application update
    CheckNotificationToSend(NULL, NULL);

    // Start watchdog on the main AVC event loop.
    // Try to kick a couple of times before each timeout.
    le_clk_Time_t watchdogInterval = { .sec = 8 };
    le_wdogChain_Init(1);
    le_wdogChain_MonitorEventLoop(0, watchdogInterval);
}
