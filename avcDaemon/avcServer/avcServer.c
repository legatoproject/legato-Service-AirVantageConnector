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
#include <lwm2mcore/security.h>
#include <lwm2mcore/lwm2mcorePackageDownloader.h>
#include "legato.h"
#include "interfaces.h"
#include "pa_avc.h"
#include "avcServer.h"
#include "tpf/tpfServer.h"
#if LE_CONFIG_ENABLE_AV_DATA
#   include "avData/avData.h"
#   include "push/push.h"
#   include "timeSeries/timeseriesData.h"
#endif
#include "updateInfo.h"
#include "le_print.h"
#include "avcAppUpdate/avcAppUpdate.h"
#include "packageDownloader/packageDownloader.h"
#include "avcFs/avcFsConfig.h"
#include "avcSim/avcSim.h"
#include "watchdogChain.h"
#include "avcClient/avcClient.h"
#include "coap/coap.h"

#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
#include <lwm2mcore/fileTransfer.h>
#include "avcFileTransfer/avFileTransfer.h"
#endif

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
 * @note User agreement is disabled by default which means that avcDaemon automatically accepts
 *       requests from the server without requesting user approval. Default value is used when
 *       there is no configuration file stored in the target.
 */
//--------------------------------------------------------------------------------------------------
#define USER_AGREEMENT_DEFAULT  0

//--------------------------------------------------------------------------------------------------
/**
 * Value if polling timer is disabled
 */
//--------------------------------------------------------------------------------------------------
#define POLLING_TIMER_DISABLED  0

//--------------------------------------------------------------------------------------------------
/**
 * Maximum expected number of blocks
 */
//--------------------------------------------------------------------------------------------------
#define HIGH_BLOCK_REF_COUNT    5

//--------------------------------------------------------------------------------------------------
/**
 * Default defer timer value: 30 minutes
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_DEFER_TIMER_VALUE   30

//--------------------------------------------------------------------------------------------------
/**
 * Prefix pattern of the wakeup SMS
 */
//--------------------------------------------------------------------------------------------------
#define WAKEUP_SMS_PREFIX "LWM2M"

//--------------------------------------------------------------------------------------------------
/**
 * Command pattern of the wakeup SMS
 */
//--------------------------------------------------------------------------------------------------
#define WAKEUP_COMMAND "WAKEUP"

//--------------------------------------------------------------------------------------------------
/**
 * Size of the decoded data buffer
 */
//--------------------------------------------------------------------------------------------------
#define WAKEUP_SMS_DECODED_DATA_BUF_SIZE    64

//--------------------------------------------------------------------------------------------------
/**
 * Timestamp for 2000 January 1st
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_TIMESTAMP                   946684800

#ifdef LE_CONFIG_SMS_SERVICE_ENABLED
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
 * Time stamp of the previously received wake-up SMS
 *
 */
//--------------------------------------------------------------------------------------------------
static int32_t LastSmsTimeStamp = 0;
#endif

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
    AVC_DOWNLOAD_TIMEOUT,       ///< Download is timeout
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
 * Event to launch a package download
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t LaunchDownloadEvent;

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
 * Static safe reference map for the block/unblock references
 */
//--------------------------------------------------------------------------------------------------
LE_REF_DEFINE_STATIC_MAP(BlockRef, HIGH_BLOCK_REF_COUNT);


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
 * Launch install timer
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t LaunchInstallTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Launch stop connection timer
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t StopCnxTimer;

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

// -------------------------------------------------------------------------------------------------
/**
 * Is update ready to install?
 */
// -------------------------------------------------------------------------------------------------
static bool IsPkgReadyToInstall = false;

// -------------------------------------------------------------------------------------------------
/**
 * Send install result notification to registered application
 */
// -------------------------------------------------------------------------------------------------
static le_avc_Status_t   UpdateStatusNotification  = LE_AVC_NO_UPDATE;
static bool              NotifyApplication = false;

//--------------------------------------------------------------------------------------------------
// Local functions
//--------------------------------------------------------------------------------------------------

#if LE_DEBUG_ENABLED || LE_INFO_ENABLED
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
        case LE_AVC_DOWNLOAD_TIMEOUT:       result = "Download timeout";        break;
        case LE_AVC_DOWNLOAD_FAILED:        result = "Download Failed";         break;
        case LE_AVC_DOWNLOAD_ABORTED:       result = "Download aborted";        break;
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
#endif // LE_DEBUG_ENABLED || LE_INFO_ENABLED

#if LE_DEBUG_ENABLED || LE_INFO_ENABLED || LE_ERROR_ENABLED
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
        case AVC_DOWNLOAD_TIMEOUT:          result = "Download timeout";        break;
        case AVC_DOWNLOAD_COMPLETE:         result = "Download complete";       break;
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
#endif // LE_DEBUG_ENABLED || LE_INFO_ENABLED || LE_ERROR_ENABLED

//--------------------------------------------------------------------------------------------------
/**
 *  Updates current AVC state and prints appropriate message
 */
//--------------------------------------------------------------------------------------------------
static void UpdateCurrentAvcState
(
    AvcState_t newAvcState     ///< The new AVC state.
)
{
    if (CurrentState != newAvcState)
    {
        LE_INFO("Transitioning from oldAvcState='%s' to newAvcState='%s'",
                ConvertAvcStateToString(CurrentState),
                ConvertAvcStateToString(newAvcState));
        CurrentState = newAvcState;
    }
}

#if LE_INFO_ENABLED || LE_WARN_ENABLED
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
#endif // LE_INFO_ENABLED || LE_WARN_ENABLED


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

#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
        case LWM2MCORE_FILE_TRANSFER_TYPE:
            avcType = LE_AVC_FILE_TRANSFER;
            break;
#endif

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
            LE_INFO("Deferring connection for %" PRIu32" minutes", deferMinutes);
            timerToStart = ConnectDeferTimer;
            break;
        case LE_AVC_USER_AGREEMENT_DOWNLOAD:
            LE_INFO("Deferring download for %" PRIu32 " minutes", deferMinutes);
            // Stop activity timer when download has been deferred
            avcClient_StopActivityTimer();
            timerToStart = DownloadDeferTimer;
            break;
        case LE_AVC_USER_AGREEMENT_INSTALL:
            LE_INFO("Deferring install for %" PRIu32" minutes", deferMinutes);
            // Stop activity timer when installation has been deferred
            avcClient_StopActivityTimer();
            timerToStart = InstallDeferTimer;
            break;
        case LE_AVC_USER_AGREEMENT_UNINSTALL:
            LE_INFO("Deferring uninstall for %" PRIu32" minutes", deferMinutes);
            // Stop activity timer when uninstall has been deferred
            avcClient_StopActivityTimer();
            timerToStart = UninstallDeferTimer;
            break;
        case LE_AVC_USER_AGREEMENT_REBOOT:
            LE_INFO("Deferring reboot for %" PRIu32" minutes", deferMinutes);
            // Stop activity timer when reboot has been deferred
            avcClient_StopActivityTimer();
            timerToStart = RebootDeferTimer;
            break;
        default:
            LE_ERROR("Unknown operation");
            return LE_FAULT;
    }

    if (le_timer_IsRunning(timerToStart))
    {
        le_timer_Stop(timerToStart);
    }
    le_timer_SetInterval(timerToStart, interval);
    le_timer_Start(timerToStart);
    return LE_OK;
}

#if LE_ERROR_ENABLED
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
#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
        case LE_AVC_FILE_TRANSFER:
            resultPtr = "LE_AVC_FILE_TRANSFER";
            break;
#endif
        default:
            resultPtr = "LE_AVC_UNKNOWN_UPDATE";
            break;

    }
    return resultPtr;
}
#endif // LE_ERROR_ENABLED

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

    for (userAgreement = LE_AVC_USER_AGREEMENT_CONNECTION;
         userAgreement <= LE_AVC_USER_AGREEMENT_REBOOT;
         userAgreement++)
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
 * Handler to launch a package download
 */
//--------------------------------------------------------------------------------------------------
static void LaunchDownload
(
    void* contextPtr
)
{
    if (NULL != QueryDownloadHandlerRef)
    {
        UpdateCurrentAvcState(AVC_DOWNLOAD_IN_PROGRESS);
        QueryDownloadHandlerRef(PkgDownloadCtx.type, PkgDownloadCtx.resume);
        QueryDownloadHandlerRef = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Query a download.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t QueryDownload
(
    void
)
{
        LE_DEBUG("Accept a package download while the device is connected to the server");
        // Notify the registered handler to proceed with the download; only called once.
        if (NULL != QueryDownloadHandlerRef)
        {
            le_event_Report(LaunchDownloadEvent, NULL, 0);
            return LE_OK;
        }
        else
        {
            LE_ERROR("Download handler not valid");
            UpdateCurrentAvcState(AVC_IDLE);
            return LE_FAULT;
        }
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if TPF mode is currently enabled
 *
 * @return
 *      - TRUE if TPF mode is enabled, FALSE otherwise
 */
//--------------------------------------------------------------------------------------------------
static bool IsTpfOngoing
(
    void
)
{
    bool state = false;

    if (LE_OK != tpfServer_GetTpfState(&state))
    {
        return false;
    }

    return state;
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

    if (IsTpfOngoing())
    {
        if(LE_OK != QueryDownload())
        {
            return LE_FAULT;
        }
    }
    else
    {
        if (LE_AVC_DM_SESSION == le_avc_GetSessionType())
        {
            if(LE_OK != QueryDownload())
            {
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
            UpdateCurrentAvcState(AVC_IDLE);
            // Connect to the server.
            if (LE_OK != avcServer_StartSession(LE_AVC_SERVER_ID_AIRVANTAGE))
            {
                LE_ERROR("Failed to start a new session");
                return LE_FAULT;
            }
        }
    }
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Trigger a 2-sec timer and launch install routine on expiry
 */
//--------------------------------------------------------------------------------------------------
static void StartInstall
(
    void
)
{
    LE_DEBUG("Starting install timer");

    // Trigger a 2-sec timer and call the install routine on expiry
    CurrentState = AVC_INSTALL_IN_PROGRESS;
    le_clk_Time_t interval = { .sec = 2, .usec = 0 };
    le_timer_SetInterval(LaunchInstallTimer, interval);
    le_timer_Start(LaunchInstallTimer);
    IsPkgReadyToInstall = false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Trigger a 2-sec timer and stop the connection in order to launch install
 */
//--------------------------------------------------------------------------------------------------
static void LaunchStopCnxTimer
(
    void
)
{
    LE_DEBUG("Starting stop cnx timer");

    // Trigger a 2-sec timer
    le_clk_Time_t interval = { .sec = 2, .usec = 0 };
    le_timer_SetInterval(StopCnxTimer, interval);
    le_timer_Start(StopCnxTimer);
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
    le_avc_SessionType_t    sessionType;

    // If an user app is blocking the update, then just defer for some time.  Hopefully, the
    // next time this function is called, the user app will no longer be blocking the update.
    if (BlockRefCount > 0)
    {
        StartDeferTimer(LE_AVC_USER_AGREEMENT_INSTALL, BLOCKED_DEFER_TIME);
        return LE_OK;
    }

    if ( (LE_AVC_FIRMWARE_UPDATE == CurrentUpdateType)
      && (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_SetUpdateAccepted()))
    {
        LE_ERROR("Issue to indicate the FW update acceptance to LwM2MCore");
    }

    StopDeferTimer(LE_AVC_USER_AGREEMENT_INSTALL);


    switch (PkgInstallCtx.type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:

            // Notify that an install is on progress
            avcServer_UpdateStatus(LE_AVC_INSTALL_IN_PROGRESS, LE_AVC_FIRMWARE_UPDATE, -1, 0,
                                   LE_AVC_ERR_NONE);

            IsPkgReadyToInstall = true;

            if (IsTpfOngoing())
            {
                LE_INFO("Accept a package install in TPF mode");
                le_avc_StopSession();
                StartInstall();
            }
            else
            {
                sessionType = le_avc_GetSessionType();
                if (sessionType == LE_AVC_BOOTSTRAP_SESSION || sessionType == LE_AVC_DM_SESSION)
                {
                    // Stop the active session before trying to install package.
                    // Launch a timer in order to treat remaining commands from server
                    LaunchStopCnxTimer();
                }
                else
                {
                    LE_INFO("StartInstall in AVC mode");
                    StartInstall();
                }
            }
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            LE_INFO("Installing SW");
            StartInstall();
            break;

        default:
            LE_ERROR("Unknown update type");
            break;
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
            UpdateCurrentAvcState(AVC_UNINSTALL_IN_PROGRESS);
            QueryUninstallHandlerRef(SwUninstallCtx.instanceId);
            QueryUninstallHandlerRef = NULL;
        }
        else
        {
            LE_ERROR("Uninstall handler not valid");
            UpdateCurrentAvcState(AVC_IDLE);
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
        UpdateCurrentAvcState(AVC_REBOOT_IN_PROGRESS);

        // Launch reboot function after 2 seconds
        le_clk_Time_t interval = { .sec = 2 };

        le_timer_SetInterval(LaunchRebootTimer, interval);
        le_timer_Start(LaunchRebootTimer);
    }
    else
    {
        LE_ERROR("Reboot handler not valid");
        UpdateCurrentAvcState(AVC_IDLE);
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

    UpdateCurrentAvcState(AVC_CONNECTION_IN_PROGRESS);
    packageDownloader_SetConnectionNotificationState(false);

    le_result_t result = avcServer_StartSession(LE_AVC_SERVER_ID_AIRVANTAGE);
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
    LE_DEBUG("Number of bytes to download %"PRId32, eventData.totalNumBytes);
    LE_DEBUG("Progress %"PRId32, eventData.progress);
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
        isUserAgreementEnabled = USER_AGREEMENT_DEFAULT;
    }

    if (!isUserAgreementEnabled)
    {
        // There is no control app; automatically accept any pending reboot
        LE_INFO("Automatically accepting connect");
        result = AcceptPendingConnection();
    }
    else if (NumStatusHandlers > 0)
    {
        // Start default defer timer
        StartDeferTimer(LE_AVC_USER_AGREEMENT_CONNECTION, DEFAULT_DEFER_TIMER_VALUE);
        // Notify registered control app.
        SendUpdateStatusEvent(LE_AVC_CONNECTION_PENDING, -1, -1, StatusHandlerContextPtr);
    }
    else
    {
        // No handler is registered, just ignore the notification.
        // The notification to send will be checked again when the control app registers a handler.
        LE_INFO("Ignoring connection pending notification, waiting for a registered handler");
        UpdateCurrentAvcState(AVC_IDLE);
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
    le_avc_UpdateType_t     updateType,     ///< [IN] Update type
    int32_t                 totalNumBytes,          ///< [IN] Remaining number of bytes to download
    int32_t                 dloadProgress           ///< [IN] Download progress
)
{
    le_result_t result = LE_BUSY;
    bool isUserAgreementEnabled;

    LE_INFO("Stopping activity timer during download pending.");
    avcClient_StopActivityTimer();

    // Check if download was already accepted.
    // This is necessary if an interrupted download was accepted without connection: accepting it
    // triggers a connection, and afterwards the download should start without user agreement.
    if ((DownloadAgreement) && (-1 != totalNumBytes))
    {
        return AcceptDownloadPackage();
    }

    // Otherwise check user agreement
#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
    if(LE_AVC_FILE_TRANSFER == updateType)
    {
#   ifdef LE_CONFIG_LINUX
        if (LE_OK != le_avtransfer_GetUserAgreement(LE_AVTRANSFER_USER_AGREEMENT_DOWNLOAD,
                                                    &isUserAgreementEnabled))
        {
            // Use default configuration if read fails
            LE_WARN("Using default user agreement configuration");
            isUserAgreementEnabled = USER_AGREEMENT_DEFAULT;
        }
#   else // RTOS
        // On RTOS, by default enable user agreement which requires user to take action to accept
        // file transfer
        isUserAgreementEnabled = true;
#   endif // LE_CONFIG_LINUX
    }
    else
#endif /* LE_CONFIG_AVC_FEATURE_FILETRANSFER */
    {
        if (LE_OK != le_avc_GetUserAgreement(LE_AVC_USER_AGREEMENT_DOWNLOAD,
                                             &isUserAgreementEnabled))
        {
            // Use default configuration if read fails
            LE_WARN("Using default user agreement configuration");
            isUserAgreementEnabled = USER_AGREEMENT_DEFAULT;
        }
    }

#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
    if (LE_AVC_FILE_TRANSFER == updateType)
    {
        le_fileStreamServer_DownloadStatus(LE_FILESTREAMCLIENT_DOWNLOAD_PENDING,
                                           totalNumBytes,
                                           dloadProgress);
    }
#endif /* LE_CONFIG_AVC_FEATURE_FILETRANSFER */

    if ((!isUserAgreementEnabled) && (-1 != totalNumBytes))
    {
        LE_INFO("Automatically accepting download");
        result = AcceptDownloadPackage();
        return result;
    }



    if (((NumStatusHandlers > 0) && (LE_AVC_FILE_TRANSFER != updateType))
#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
     || ((LE_AVC_FILE_TRANSFER == updateType)))
#else
    )
#endif
    {
        // Start default defer timer
        StartDeferTimer(LE_AVC_USER_AGREEMENT_DOWNLOAD, DEFAULT_DEFER_TIMER_VALUE);
#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
        // Notify registered control app.
        if(LE_AVC_FILE_TRANSFER == updateType)
        {
            char fileName[LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR+1];
            size_t len = LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR;
            if (LE_OK == avFileTransfer_GetTransferName(fileName, &len))
            {
                avFileTransfer_SendStatusEvent(LE_AVTRANSFER_PENDING,
                                               fileName,
                                               totalNumBytes,
                                               dloadProgress,
                                               StatusHandlerContextPtr);
            }
            else
            {
                LE_ERROR("Failed to get file name");
            }
        }
        else
#endif
        {
            SendUpdateStatusEvent(LE_AVC_DOWNLOAD_PENDING,
                                  totalNumBytes,
                                  dloadProgress,
                                  StatusHandlerContextPtr);
        }
        result = LE_OK;
    }
    else
    {
        // No handler is registered, just ignore the notification.
        // The notification to send will be checked again when the control app registers a handler.
        LE_INFO("Ignoring download pending notification, waiting for a registered handler");
        UpdateCurrentAvcState(AVC_IDLE);
        QueryDownloadHandlerRef = NULL;
        result = LE_OK;
    }
    LE_DEBUG("RespondToDownloadPending %d", result);
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
        // Start default defer timer
        StartDeferTimer(LE_AVC_USER_AGREEMENT_INSTALL, DEFAULT_DEFER_TIMER_VALUE);
        // Notify registered control app.
        SendUpdateStatusEvent(LE_AVC_INSTALL_PENDING, -1, -1, StatusHandlerContextPtr);
    }
    else
    {
        // No handler is registered, just ignore the notification.
        // The notification to send will be checked again when the control app registers a handler.
        LE_INFO("Ignoring install pending notification, waiting for a registered handler");
        UpdateCurrentAvcState(AVC_IDLE);
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
        // Start default defer timer
        StartDeferTimer(LE_AVC_USER_AGREEMENT_UNINSTALL, DEFAULT_DEFER_TIMER_VALUE);
        // Notify registered control app.
        SendUpdateStatusEvent(LE_AVC_UNINSTALL_PENDING, -1, -1, StatusHandlerContextPtr);
    }
    else
    {
        // No handler is registered, just ignore the notification.
        // The notification to send will be checked again when the control app registers a handler.
        LE_INFO("Ignoring uninstall pending notification, waiting for a registered handler");
        UpdateCurrentAvcState(AVC_IDLE);
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
        // Start default defer timer
        StartDeferTimer(LE_AVC_USER_AGREEMENT_REBOOT, DEFAULT_DEFER_TIMER_VALUE);
        // Notify registered control app.
        SendUpdateStatusEvent(LE_AVC_REBOOT_PENDING, -1, -1, StatusHandlerContextPtr);
     }
     else
     {
         // No handler is registered, just ignore the notification.
         // The notification to send will be checked again when the control app registers a handler.
         LE_INFO("Ignoring reboot pending notification, waiting for a registered handler");
         UpdateCurrentAvcState(AVC_IDLE);
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
    le_avc_UpdateType_t        updateType,       ///< [IN] Update type
    int32_t                    totalNumBytes,    ///< [IN] Remaining number of bytes to download
    int32_t                    dloadProgress     ///< [IN] Download progress
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
            result = RespondToDownloadPending(updateType, totalNumBytes, dloadProgress);
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
                UpdateCurrentAvcState(AVC_DOWNLOAD_PENDING);
                SendUpdateStatusEvent(LE_AVC_DOWNLOAD_PENDING,
                                      -1,
                                      -1,
                                      StatusHandlerContextPtr);
            }
            break;

        default:

#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
            // TODO: is it really called ?
            if (LE_AVC_FILE_TRANSFER == updateType)
            {
                if ((LE_AVC_DOWNLOAD_PENDING == updateStatus)
                 || (LE_AVC_DOWNLOAD_IN_PROGRESS == updateStatus)
                 || (LE_AVC_DOWNLOAD_COMPLETE == updateStatus)
                 || (LE_AVC_DOWNLOAD_FAILED == updateStatus))
                {
                    char fileName[LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR+1];
                    size_t len = LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR;
                    if (LE_OK == avFileTransfer_GetTransferName(fileName, &len))
                    {
                        // Forward notifications unrelated to user agreement to interested
                        // applications.
                        avFileTransfer_SendStatusEvent(avFileTransfer_ConvertAvcState(updateStatus),
                                                       fileName,
                                                       totalNumBytes,
                                                       dloadProgress,
                                                       StatusHandlerContextPtr);
                        if (LE_AVC_DOWNLOAD_PENDING == updateStatus)
                        {
                            le_fileStreamServer_DownloadStatus(LE_FILESTREAMCLIENT_DOWNLOAD_PENDING,
                                                               totalNumBytes,
                                                               dloadProgress);
                        }
                    }
                    else
                    {
                        LE_ERROR("Failed to get file name");
                    }
                    break;
                }
            }
#endif

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

//-------------------------------------------------------------------------------------------------
/**
 * Connect to AirVantage or other Device Management server (specified by Server ID)
 */
//-------------------------------------------------------------------------------------------------
static void ConnectToServer
(
    uint16_t    serverId    ///< [IN] Server ID.
)
{
    // Start a session.
    if (LE_DUPLICATE == avcServer_StartSession(serverId))
    {
        // Session is already connected, but wireless network could have been de-provisioned
        // due to NAT timeout. Do a registration update to re-establish connection.
        if (LE_OK != avcClient_Update())
        {
            // Restart the session if registration update fails.
            avcClient_Disconnect(true);

            // Connect after 2 seconds
            le_clk_Time_t interval = { .sec = 2 };
            le_timer_SetContextPtr(LaunchConnectTimer, (void *) (uintptr_t) serverId);
            le_timer_SetInterval(LaunchConnectTimer, interval);
            le_timer_Start(LaunchConnectTimer);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Write AVC configuration parameter to platform memory
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
        LE_ERROR("AVC configuration pointer is null");
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
 * Read AVC configuration parameter to platform memory
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
        LE_ERROR("AVC configuration pointer is null");
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
 * Initialize the polling timer at startup.
 *
 * Note: This function reads the polling timer configuration and if enabled starts the polling
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

        if (currentTime < DEFAULT_TIMESTAMP)
        {
            LE_ERROR("Can't retrieve time");
            return;
        }

        // Time elapsed since last poll
        int32_t timeElapsed = 0;

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
            // Polling timer initiates connection to AirVantage server only
            ConnectToServer(LE_AVC_SERVER_ID_AIRVANTAGE);
        }

        remainingPollingTimer = ((pollingTimer * SECONDS_IN_A_MIN) - timeElapsed);

        LE_INFO("Polling Timer is set to start AVC session every %" PRIu32" minutes.", pollingTimer);
        LE_INFO("The current Polling Timer will start a session in %" PRIu32" seconds.",
                                                                remainingPollingTimer);

        // Set a timer to start the next session.
        le_clk_Time_t interval = {.sec = remainingPollingTimer};

        LE_ASSERT(LE_OK == le_timer_SetInterval(PollingTimerRef, interval));
        result = le_timer_Start(PollingTimerRef);
        if (result == LE_BUSY)
        {
            LE_WARN("Polling timer is already running.");
        }
        else if (result != LE_OK)
        {
            LE_FATAL("Setting polling timer failed with result %d %s", result, LE_RESULT_TXT(result));
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to initialize the polling timer
 */
//--------------------------------------------------------------------------------------------------
void avcServer_InitPollingTimer
(
    void
)
{
    if (!le_timer_IsRunning(PollingTimerRef))
    {
        InitPollingTimer();
    }
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

    LE_INFO("Current session state: %s", AvcSessionStateToStr(data->updateStatus));
    // Keep track of the state of any pending downloads or installs.
    switch (data->updateStatus)
    {
        case LE_AVC_CONNECTION_PENDING:
            UpdateCurrentAvcState(AVC_CONNECTION_PENDING);
            break;

        case LE_AVC_REBOOT_PENDING:
            UpdateCurrentAvcState(AVC_REBOOT_PENDING);
            break;

        case LE_AVC_DOWNLOAD_PENDING:
            LE_DEBUG("Update type for DOWNLOAD is %d", data->updateType);
            LE_DEBUG("totalNumBytes %d", data->totalNumBytes);

            if (-1 != data->totalNumBytes)
            {
                UpdateCurrentAvcState(AVC_DOWNLOAD_PENDING);
                CurrentTotalNumBytes = data->totalNumBytes;
            }

            if (LE_AVC_UNKNOWN_UPDATE != data->updateType)
            {
                CurrentUpdateType = data->updateType;
            }
            CurrentDownloadProgress = data->progress;
            AvcErrorCode = data->errorCode;
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

#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
            if ((LE_AVC_FILE_TRANSFER == data->updateType) && (data->totalNumBytes >= 0))
            {
                avFileTransfer_TreatProgress(true, data->progress);
                le_fileStreamServer_DownloadStatus(LE_FILESTREAMCLIENT_DOWNLOAD_IN_PROGRESS,
                                                   data->totalNumBytes,
                                                   data->progress);
            }
#endif
            break;

        case LE_AVC_DOWNLOAD_TIMEOUT:
            UpdateCurrentAvcState(AVC_DOWNLOAD_TIMEOUT);
            ConnectToServer(LE_AVC_SERVER_ID_AIRVANTAGE);
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

            UpdateCurrentAvcState(AVC_DOWNLOAD_COMPLETE);
            avcClient_StartActivityTimer();
            DownloadAgreement = false;

            if (IsTpfOngoing())
            {
                LE_INFO("Download complete in TPF mode, launch FW install");
                // Call the TPF callback.
                avcClient_LaunchFwUpdate();
            }
            else
            {
                if ((LE_AVC_FIRMWARE_UPDATE == data->updateType)
                 || (LE_AVC_APPLICATION_UPDATE == data->updateType))
                {
                    packageDownloader_SetConnectionNotificationState(true);
                }
            }

            if (LE_AVC_APPLICATION_UPDATE == data->updateType)
            {
                // Set the bytes downloaded to workspace for resume operation
                avcApp_SetSwUpdateBytesDownloaded();

                // End download and start unpack
                avcApp_EndDownload();
            }
#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
            else if (LE_AVC_FILE_TRANSFER == data->updateType)
            {
                avFileTransfer_TreatProgress(false, 0);
                le_fileStreamServer_DownloadStatus(LE_FILESTREAMCLIENT_DOWNLOAD_COMPLETED,
                                                   data->totalNumBytes,
                                                   data->progress);
                // Update the supported object instances list
                avFileTransfer_InitFileInstanceList();
            }
#endif
            break;

        case LE_AVC_INSTALL_PENDING:
            LE_DEBUG("Update type for INSTALL is %d", data->updateType);
            UpdateCurrentAvcState(AVC_INSTALL_PENDING);
            if (LE_AVC_UNKNOWN_UPDATE != data->updateType)
            {
                // If the device resets during a FOTA download, then the CurrentUpdateType is lost
                // and needs to be assigned again. Since we don't easily know if a reset happened,
                // re-assign the value if possible.
                CurrentUpdateType = data->updateType;
            }
            packageDownloader_SetConnectionNotificationState(false);
            break;

        case LE_AVC_UNINSTALL_PENDING:
            UpdateCurrentAvcState(AVC_UNINSTALL_PENDING);
            if (LE_AVC_UNKNOWN_UPDATE != data->updateType)
            {
                LE_DEBUG("Update type for UNINSTALL is %d", data->updateType);
                CurrentUpdateType = data->updateType;
            }
            break;

        case LE_AVC_INSTALL_IN_PROGRESS:
        case LE_AVC_UNINSTALL_IN_PROGRESS:
            packageDownloader_SetConnectionNotificationState(false);
            avcClient_StopActivityTimer();
            break;

        case LE_AVC_DOWNLOAD_FAILED:
            DownloadAgreement = false;
            // There is no longer any current update, so go back to idle
            UpdateCurrentAvcState(AVC_IDLE);

            if (IsTpfOngoing())
            {
                // If a download or install fails, stop the session to not block AVC connection
                le_avc_StopSession();
                tpfServer_SetTpfState(false);
            }
            else
            {
                avcClient_StartActivityTimer();
            }

            if (LE_AVC_APPLICATION_UPDATE == data->updateType)
            {
                avcApp_DeletePackage();
            }
#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
            else if (LE_AVC_FILE_TRANSFER == data->updateType)
            {
                avFileTransfer_TreatProgress(false, 0);
                le_fileStreamServer_DownloadStatus(LE_FILESTREAMCLIENT_DOWNLOAD_FAILED,
                                                   data->totalNumBytes,
                                                   data->progress);
            }
#endif
            AvcErrorCode = data->errorCode;
            break;

        case LE_AVC_INSTALL_FAILED:
            // There is no longer any current update, so go back to idle
            UpdateCurrentAvcState(AVC_IDLE);

            if (IsTpfOngoing())
            {
                // If a download or install is failed, stop the session to not block AVC connection
                le_avc_StopSession();
                tpfServer_SetTpfState(false);
            }

            if (LE_AVC_APPLICATION_UPDATE == data->updateType)
            {
                avcApp_DeletePackage();
            }

            avcClient_StartActivityTimer();
            AvcErrorCode = data->errorCode;
            break;

        case LE_AVC_UNINSTALL_FAILED:
            // There is no longer any current update, so go back to idle
            UpdateCurrentAvcState(AVC_IDLE);

            if (IsTpfOngoing())
            {
                // If a uninstall is failed, stop the session to not block AVC connection
                tpfServer_SetTpfState(false);
            }

            avcClient_StartActivityTimer();
            AvcErrorCode = data->errorCode;
            // Forward notifications unrelated to user agreement to interested applications
            NotifyApplication = true;
            UpdateStatusNotification = data->updateStatus;
            break;

        case LE_AVC_NO_UPDATE:
            if (AVC_DOWNLOAD_PENDING != CurrentState)
            {
                // There is no longer any current update, so go back to idle
                UpdateCurrentAvcState(AVC_IDLE);
                packageDownloader_SetConnectionNotificationState(false);
            }
            break;

        case LE_AVC_INSTALL_COMPLETE:
        case LE_AVC_UNINSTALL_COMPLETE:
            // There is no longer any current update, so go back to idle
            UpdateCurrentAvcState(AVC_IDLE);

            // If a download or install is complete, stop the session to not block AVC connection
            if (IsTpfOngoing())
            {
                tpfServer_SetTpfState(false);
            }
            // Forward notifications unrelated to user agreement to interested applications
            NotifyApplication = true;
            UpdateStatusNotification = data->updateStatus;
            break;

        case LE_AVC_SESSION_STARTED:
            if (PollingTimerRef != NULL)
            {
                if (le_timer_IsRunning(PollingTimerRef))
                {
                    if(LE_OK != le_timer_Stop(PollingTimerRef))
                    {
                        LE_ERROR("polling timer can't be stopped");
                    }
                }
            }
            avcClient_StartActivityTimer();
            // Update object9 list managed by legato to lwm2mcore
            avcApp_NotifyObj9List();
#if LE_CONFIG_ENABLE_AV_DATA
            avData_ReportSessionState(LE_AVDATA_SESSION_STARTED);

            // Push items waiting in queue
            push_Retry();
#endif /* end LE_CONFIG_ENABLE_AV_DATA */
            break;

        case LE_AVC_SESSION_STOPPED:
            avcClient_StopActivityTimer();
            if (!le_timer_IsRunning(PollingTimerRef))
            {
                InitPollingTimer();
            }
            // These events do not cause a state transition
#if LE_CONFIG_ENABLE_AV_DATA
            avData_ReportSessionState(LE_AVDATA_SESSION_STOPPED);
#endif
            // If any download ongoing, suspend it.
            if (avcServer_IsDownloadInProgress())
            {
                LE_INFO("Suspending on-going download");
                lwm2mcore_SuspendDownload();
            }
            // If a package is waiting to be installed, trigger the install.
            if (IsPkgReadyToInstall)
            {
                StartInstall();
            }

            break;

        case LE_AVC_SESSION_FAILED:
            // In case of session faild , stop the TPF session to not bloc AVC connection
            if (IsTpfOngoing())
            {
                le_avc_StopSession();
                tpfServer_SetTpfState(false);
            }
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

            if (!IsTpfOngoing())
            {
                // Query connection to server if module reboot
                packageDownloader_SetConnectionNotificationState(true);
            }

#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
            if (LE_AVC_FILE_TRANSFER == data->updateType)
            {
                LE_ERROR("No certification check for file transfer");
            }
#endif

            break;

        case LE_AVC_CERTIFICATION_KO:
            // In case of certification failed, stop the TPF session to not block AVC connection
            if (IsTpfOngoing())
            {
                le_avc_StopSession();
                tpfServer_SetTpfState(false);
            }
            else
            {
                // Query connection to server if module reboot
                packageDownloader_SetConnectionNotificationState(true);
            }

#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
            if (LE_AVC_FILE_TRANSFER == data->updateType)
            {
                LE_ERROR("No certification check for file transfer");
            }
#endif

            LE_DEBUG("Package not certified");
            break;
#if MK_CONFIG_TPF_TERMINATE_DOWNLOAD
        case LE_AVC_DOWNLOAD_ABORTED:
            if (IsTpfOngoing())
            {
                le_fwupdate_InitDownload(); //Delete resume context
                tpfServer_SetTpfState(false);
                le_avc_StopSession();
                UpdateCurrentAvcState(AVC_IDLE);
            }
            LE_DEBUG("Download aborted");
            break;
#endif
        default:
            LE_WARN("Unhandled updateStatus %d", data->updateStatus);
            return;
    }

    // Process user agreement or forward to control app if applicable.
    ProcessUserAgreement(data->updateStatus, data->updateType, data->totalNumBytes, data->progress);
}

//--------------------------------------------------------------------------------------------------
/**
 * Send update status notifications to AVC server
 */
//--------------------------------------------------------------------------------------------------
void avcServer_UpdateStatus
(
    le_avc_Status_t updateStatus,   ///< Update status
    le_avc_UpdateType_t updateType, ///< Update type
    int32_t totalNumBytes,          ///< Total number of bytes to download (-1 if not set)
    int32_t progress,               ///< Progress in percent (-1 if not set)
    le_avc_ErrorCode_t errorCode    ///< Error code
)
{
    AvcUpdateStatusData_t updateStatusData;

    updateStatusData.updateStatus  = updateStatus;
    updateStatusData.updateType    = updateType;
    updateStatusData.totalNumBytes = totalNumBytes;
    updateStatusData.progress = progress;
    updateStatusData.errorCode     = errorCode;

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
                           LE_AVC_ERR_NONE
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
                           LE_AVC_ERR_NONE
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
                           LE_AVC_ERR_NONE
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
                           LE_AVC_ERR_NONE
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
                           LE_AVC_ERR_NONE
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
#if LE_CONFIG_AVC_FEATURE_EDM
    uint16_t serverId = (uint16_t) (uintptr_t) le_timer_GetContextPtr(timerRef);
    avcServer_StartSession(serverId);
#else
    avcServer_StartSession(LE_AVC_SERVER_ID_AIRVANTAGE);
#endif
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
 * Called when the launch install timer expires
 */
//--------------------------------------------------------------------------------------------------
static void LaunchInstallExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    // Notify the registered handler to proceed with the install; only called once.
    if (QueryInstallHandlerRef != NULL)
    {
        LE_DEBUG("Triggering installation");
        QueryInstallHandlerRef(PkgInstallCtx.type, PkgInstallCtx.instanceId);
        QueryInstallHandlerRef = NULL;
    }
    else
    {
        LE_ERROR("Install handler not valid");
        UpdateCurrentAvcState(AVC_IDLE);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the stop connection timer expires
 */
//--------------------------------------------------------------------------------------------------
static void StopConnectionExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    le_avc_StopSession();
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
le_result_t avcServer_SaveCurrentEpochTime
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

    if (IsTpfOngoing())
    {
        LE_ERROR("Ignore polling timer when TPF running.");
        return;
    }

    avcServer_SaveCurrentEpochTime();

    // Connect to AirVantage server only.
    ConnectToServer(LE_AVC_SERVER_ID_AIRVANTAGE);

    // Restart the timer for the next interval
    uint32_t pollingTimerInterval;
    if (LE_OK != le_avc_GetPollingTimer(&pollingTimerInterval))
    {
        LE_ERROR("Unable to get the polling time interval");
        return;
    }

    if (POLLING_TIMER_DISABLED != pollingTimerInterval)
    {
        LE_INFO("A connection to server will be made in %" PRIu32" minutes", pollingTimerInterval);
        le_clk_Time_t interval = {.sec = pollingTimerInterval * SECONDS_IN_A_MIN};
        LE_ASSERT(LE_OK == le_timer_SetInterval(PollingTimerRef, interval));
        le_result_t result = le_timer_Start(PollingTimerRef);
        if (result == LE_BUSY)
        {
            LE_WARN("Polling timer is already running.");
        }
        else if (result != LE_OK)
        {
            LE_FATAL("Setting polling timer failed with result %d %s", result, LE_RESULT_TXT(result));
        }
    }
    else
    {
        LE_INFO("Polling disabled");
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
    bool*                       isFwUpdateToNotifyPtr,  ///< [INOUT] Is a FW update needed to be
                                                        ///< notified to the server?
    le_avc_StatusHandlerFunc_t  statusHandlerPtr,       ///< [IN] Pointer on handler function
    void*                       contextPtr              ///< [IN] Context
)
{
    bool isFwUpdateOnGoing = false;
    bool notify = false;
    *isFwUpdateToNotifyPtr = false;

    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_IsFwUpdateOnGoing(&isFwUpdateOnGoing))
    {
        LE_ERROR("Fail to check FW update state");
        return LE_FAULT;
    }

    // Check if a FW update was ongoing
    if (isFwUpdateOnGoing)
    {
        // Retrieve FW update result
        le_fwupdate_UpdateStatus_t fwUpdateStatus;
        char statusStr[LE_FWUPDATE_STATUS_LABEL_LENGTH_MAX];
        le_avc_ErrorCode_t errorCode = LE_AVC_ERR_NONE;
        le_avc_Status_t updateStatus = LE_AVC_NO_UPDATE;

        if (LE_OK != le_fwupdate_GetUpdateStatus(&fwUpdateStatus, statusStr, sizeof(statusStr)))
        {
            LE_ERROR("Error while reading the FW update status");
            return LE_FAULT;
        }

        // Check if a FOTA install pending notification was accepted but the install was interupted
        // so send a new notification to accept the install pending
        if ((LE_OK == packageDownloader_GetFwUpdateInstallPending(&notify)) && (notify))
        {
            LE_INFO("Firmware Package is available , the install is in pending state");
        }
        LE_DEBUG("Update status: %s (%d)", statusStr, fwUpdateStatus);

        // Indicates the update result
        if (LE_FWUPDATE_UPDATE_STATUS_OK == fwUpdateStatus)
        {
            if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_SetUpdateResult(true))
            {
                LE_ERROR("Issue to indicate the FW update success to LwM2MCore");
            }
             if (LE_OK != packageDownloader_SetFwUpdateInstallPending(false))
            {
                LE_ERROR("Unable to clear the fw update install Pending flag");
            }
            updateStatus = LE_AVC_INSTALL_COMPLETE;
            errorCode = LE_AVC_ERR_NONE;
        }

        // in case of the package is available but in the u boot it was not installed
        else if (LE_FWUPDATE_UPDATE_STATUS_DWL_ONGOING == fwUpdateStatus)
        {
            if( notify)
            {
                ResumeFwInstall();
                return LE_OK;
            }
        }
        else
        {
            if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_SetUpdateResult(false))
            {
                LE_ERROR("Issue to indicate the FW update failure to LwM2MCore");
            }
            updateStatus = LE_AVC_INSTALL_FAILED;
              if (LE_OK != packageDownloader_SetFwUpdateInstallPending(false))
            {
                LE_ERROR("Unable to clear the fw update install Pending flag");
            }

            if (LE_FWUPDATE_UPDATE_STATUS_PARTITION_ERROR == fwUpdateStatus)
            {
                errorCode = LE_AVC_ERR_BAD_PACKAGE;
            }
            else
            {
                errorCode = LE_AVC_ERR_INTERNAL;
            }
        }
        LE_DEBUG("Send notif FW updateStatus %d", updateStatus);

        // fwupdate done, It may either fail or pass, clear the resume information. We clean
        // resume info at the beginning of avcDaemon start but that part of the code may not
        // executed if only modem/yocto is upgraded. Clear it again as it is harmless to do so.

#ifndef LE_CONFIG_CUSTOM_OS
        packageDownloader_DeleteResumeInfo();
#endif /* !LE_CONFIG_CUSTOM_OS */

        lwm2mcore_DeletePackageDownloaderResumeInfo();

        *isFwUpdateToNotifyPtr = true;
        avcServer_UpdateStatus(updateStatus, LE_AVC_FIRMWARE_UPDATE, -1, -1, errorCode);

        if (IsTpfOngoing())
        {
            LE_INFO("Ignoring query connection in TPF mode");
            return LE_OK;
        }

        packageDownloader_SetFwUpdateNotification(true, updateStatus, errorCode, fwUpdateStatus);
        avcServer_QueryConnection(LE_AVC_FIRMWARE_UPDATE, statusHandlerPtr, contextPtr);
    }
    else
    {
        if (IsTpfOngoing())
        {
            LE_INFO("Ignoring query connection in TPF mode");
            return LE_OK;
        }

        // Check if a connection is required because the update result was not notified to
        // the server
        bool notifRequested = false;
        le_avc_ErrorCode_t errorCode = LE_AVC_ERR_NONE;
        le_avc_Status_t updateStatus = LE_AVC_NO_UPDATE;
        le_fwupdate_UpdateStatus_t fwUpdateErrorCode = LE_FWUPDATE_UPDATE_STATUS_OK;
        le_result_t result = packageDownloader_GetFwUpdateNotification(&notifRequested,
                                                                       &updateStatus,
                                                                       &errorCode,
                                                                       &fwUpdateErrorCode);
        if (IsTpfOngoing())
        {
            LE_INFO("Ignoring query connection in TPF mode");
            return LE_OK;
        }
        else if ((LE_OK == result) && (notifRequested))
        {
            avcServer_QueryConnection(LE_AVC_FIRMWARE_UPDATE, statusHandlerPtr, contextPtr);
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
   le_avc_StatusHandlerFunc_t statusHandlerPtr,  ///< [IN] Pointer on handler function
   void*                      contextPtr         ///< [IN] Context
)
{
    bool notify = false;
    bool connectionState = false;
    bool isFwUpdateToNotify = false;
    uint64_t numBytesToDownload = 0;
    le_avc_Status_t avcStatus;
    le_avc_ErrorCode_t errorCode;

    if (IsTpfOngoing())
    {
        LE_INFO("Ignoring check notification in TPF mode");
        return;
    }

    if (AVC_IDLE != CurrentState)
    {
        // Since FW install result notification is not reported when auto-connect is performed at
        // startup, this notification needs to be resent again to the newly registred applications
        avcStatus = LE_AVC_NO_UPDATE;
        errorCode = LE_AVC_ERR_NONE;
        le_fwupdate_UpdateStatus_t fwUpdateErrorCode = LE_FWUPDATE_UPDATE_STATUS_OK;
        if ((LE_OK == packageDownloader_GetFwUpdateNotification(&notify,
                                                                &avcStatus,
                                                                &errorCode,
                                                                &fwUpdateErrorCode))
            && (notify))
        {
            avcServer_UpdateStatus(avcStatus, LE_AVC_FIRMWARE_UPDATE, -1, -1, errorCode);


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
    // check if a connection to server is needed to notify the end of download
    if((LE_OK == packageDownloader_GetConnectionNotificationState(&connectionState))
        && connectionState)
    {
        avcServer_QueryConnection(LE_AVC_FIRMWARE_UPDATE, statusHandlerPtr, contextPtr);
        return;
    }
    // check FW install result and notification flag
    if ((LE_OK == CheckFwInstallResult(&isFwUpdateToNotify, statusHandlerPtr, contextPtr))
         && isFwUpdateToNotify)
    {
        return;
    }

    // 2. Check if a FOTA install pending notification should be sent because it was not accepted
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
        lwm2mcore_UpdateType_t updateType = LWM2MCORE_MAX_UPDATE_TYPE;
        lwm2mcore_Sid_t infoResult;
        uint64_t packageSize = 0;
        char downloadUri[LWM2MCORE_PACKAGE_URI_MAX_BYTES];
        memset(downloadUri, 0, sizeof(downloadUri));
        LE_DEBUG("Bytes left to download: %"PRIu64, numBytesToDownload);

        // Get download info
        infoResult = lwm2mcore_GetDownloadInfo (&updateType, &packageSize);
        if (LWM2MCORE_ERR_COMPLETED_OK != infoResult)
        {
            LE_DEBUG("Error to get package info");
            return;
        }

        // Check if a download can be resumed
        if ((!numBytesToDownload) && (!packageSize))
        {
            LE_DEBUG("No download to resume");
            return;
        }

        if (QueryDownloadHandlerRef == NULL)
        {
            // Request user agreement for download
            avcServer_QueryDownload(packageDownloader_StartDownload,
                                    numBytesToDownload,
                                    updateType,
                                    true,
                                    LE_AVC_ERR_NONE);
        }
        else
        {
            LE_DEBUG("Resending the download indication");
            avcServer_UpdateStatus(LE_AVC_DOWNLOAD_PENDING,
                                   ConvertToAvcType(PkgDownloadCtx.type),
                                   PkgDownloadCtx.bytesToDownload,
                                   0,
                                   LE_AVC_ERR_NONE
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
 * In case a firmware was installed, check the install result and update the firmware update state
 * and result accordingly.
 *
 * @return
 *      - LE_OK     The function succeeded
 *      - LE_FAULT  An error occurred
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_CheckFwInstallResult
(
    bool*                       isFwUpdateToNotifyPtr,  ///< [INOUT] Is a FW update needed to be
                                                        ///< notified to the server?
    le_avc_StatusHandlerFunc_t  statusHandlerPtr,       ///< [IN] Pointer on handler function
    void*                       contextPtr              ///< [IN] Context
)
{
    return CheckFwInstallResult(isFwUpdateToNotifyPtr, statusHandlerPtr, contextPtr);
}

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
                                   LE_AVC_ERR_NONE
                                  );
            break;

        case LE_AVC_APPLICATION_UPDATE:
            LE_DEBUG("Reporting status LE_AVC_CONNECTION_PENDING for SOTA");
            avcServer_UpdateStatus(LE_AVC_CONNECTION_PENDING,
                                   LE_AVC_APPLICATION_UPDATE,
                                   -1,
                                   -1,
                                   LE_AVC_ERR_NONE
                                  );
            break;

#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
        case LE_AVC_FILE_TRANSFER:
            LE_DEBUG("Reporting status LE_AVC_CONNECTION_PENDING for file transfer");
            avcServer_UpdateStatus(LE_AVC_CONNECTION_PENDING,
                                   updateType,
                                   -1,
                                   -1,
                                   LE_AVC_ERR_NONE
                                  );
            break;
#endif

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
                           LE_AVC_ERR_NONE
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
    avcServer_DownloadHandlerFunc_t handlerFunc,        ///< [IN] Download handler function
    uint64_t                        bytesToDownload,    ///< [IN] Number of bytes to download
    lwm2mcore_UpdateType_t          type,               ///< [IN] Update type
    bool                            resume,             ///< [IN] Is it a download resume?
    le_avc_ErrorCode_t              errorCode           ///< [IN] AVC error code if download was
                                                        ///<      suspended
)
{
    if (NULL != QueryDownloadHandlerRef)
    {
        LE_ERROR("Duplicate download attempt");
        return;
    }

    // Update download handler
    if(bytesToDownload != INT64_MAX)
    {
        QueryDownloadHandlerRef = handlerFunc;
    }
    memset(&PkgDownloadCtx, 0, sizeof(PkgDownloadCtx));
    PkgDownloadCtx.bytesToDownload = bytesToDownload;
    PkgDownloadCtx.type = type;
    PkgDownloadCtx.resume = resume;

    avcServer_UpdateStatus( LE_AVC_DOWNLOAD_PENDING,
                            ConvertToAvcType(PkgDownloadCtx.type),
                            (bytesToDownload == INT64_MAX) ? -1 : (int32_t)PkgDownloadCtx.bytesToDownload,
                            0,
                            errorCode
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
                           LE_AVC_ERR_NONE
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
                           LE_AVC_ERR_NONE
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
        result = avcServer_StartSession(LE_AVC_SERVER_ID_AIRVANTAGE);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start a session with the AirVantage or other DM server.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_FAULT on failure
 *      - LE_DUPLICATE if an AV session is already connected.
 *      - LE_BUSY if currently retrying or authenticating.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_StartSession
(
    uint16_t    serverId    ///< [IN] Server ID. Can be LE_AVC_SERVER_ID_ALL_SERVERS.
)
{
#if !LE_CONFIG_AVC_FEATURE_EDM
    serverId = LE_AVC_SERVER_ID_AIRVANTAGE;
#endif
    le_result_t result = avcClient_Connect(serverId);

    if ((LE_BUSY == result) && avcClient_IsRetryTimerActive())  // Retry timer is active
    {
        avcClient_ResetRetryTimer();
        return avcClient_Connect(serverId);
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
 * Is the current state AVC_DOWNLOAD_IN_PROGRESS?
 */
//--------------------------------------------------------------------------------------------------
bool avcServer_IsDownloadInProgress
(
    void
)
{
    return (AVC_DOWNLOAD_IN_PROGRESS == CurrentState);
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
    le_avc_StatusHandlerFunc_t clientHandlerFunc = (le_avc_StatusHandlerFunc_t)secondLayerHandlerFunc;

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
    if (NotifyApplication)
    {
        handlerPtr(UpdateStatusNotification, -1, -1, contextPtr);
    }
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
 *      - LE_BUSY if currently retrying or authenticating.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_StartSession
(
    void
)
{
    IsUserSession = true;
    StopDeferTimer(LE_AVC_USER_AGREEMENT_CONNECTION);
    return avcServer_StartSession(LE_AVC_SERVER_ID_AIRVANTAGE);
}


//--------------------------------------------------------------------------------------------------
/**
 * Start a session with a specific Device Management (DM) server.
 *
 * This function is similar to le_avc_StartSession(), with the main difference of adding extra
 * parameter to specify the Server ID of the DM server; this way, it provides flexibility to
 * connect to any DM server, not just AirVantage.
 *
 * For example, the device may need to communicate with EDM server that is providing support
 * for the SIM Reachability features (LWM2M proprietory object 33408).
 *
 * Reserved Server IDs are:
 * 0 for Bootstrap server
 * 1 for AirVantage server
 * 1000 for EDM server
 *
 * @note DM servers may have different capabilities in terms of which LWM2M objects they support.
 * For instance, EDM server supports only one specific type of object (Object 33408), and does
 * not support Objects 5 and 9, which means it doesn't allow SOTA/FOTA operations.
 *
 * @note To initiate a session with AirVantage server, it's preferable to use le_avc_StartSession()
 * which exists specifically for this purpose.
 *
 * @note If the device doesn't have credentials for the specificed DM server, the boostrapping
 * process will be automatically initiated.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_FAULT on failure
 *      - LE_DUPLICATE if already connected to the server.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_StartDmSession
(
    uint16_t    serverId,           ///< [IN] Short ID of the DM server.
                                    ///<      Can be LE_AVC_SERVER_ID_ALL_SERVERS.
    bool        isAutoDisconnect    ///< [IN] Whether the session should be auto disconnected
)
{
    LE_INFO("Starting DM session with server %" PRIu16 " auto-disconnect: %s",
            serverId, isAutoDisconnect ? "yes" : "no");
    StopDeferTimer(LE_AVC_USER_AGREEMENT_CONNECTION);

    IsUserSession = IsUserSession || !isAutoDisconnect;
    return avcServer_StartSession(serverId);
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
    bool isDownloadThreadAlive;

    if (AVC_DOWNLOAD_PENDING != CurrentState)
    {
        LE_ERROR("Expected DOWNLOAD_PENDING state; current state is '%s'",
                 ConvertAvcStateToString(CurrentState));
        return LE_FAULT;
    }

    // Check if the download thread is not running
    if ((LE_OK == packageDownloader_IsDownloadInProgress(&isDownloadThreadAlive))
         && (isDownloadThreadAlive))
    {
        LE_ERROR("Download thread is still running");
        return LE_FAULT;
    }

    // Accept download indirectly open session if there is no session. In that it should be
    // considered as user initiated session.
    IsUserSession = true;

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
        LE_DEBUG("In AVC_IDLE state; no update pending or in progress");
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
    uint16_t httpErrorCode;
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_GetLastHttpErrorCode(&httpErrorCode))
    {
        return LE_AVC_HTTP_STATUS_INVALID;
    }

    if (!httpErrorCode)
    {
        httpErrorCode = LE_AVC_HTTP_STATUS_INVALID;
    }
    return httpErrorCode;
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
    uint16_t* timerValuePtr,  ///< [OUT] Array of retry timer intervals, minutes.
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

        if (retryTimersCfg[i] > LE_AVC_RETRY_TIMER_MAX_VAL)
        {
            LE_ERROR("The stored Retry Timer value %d is out of range. Max %d.",
                     retryTimersCfg[i], LE_AVC_RETRY_TIMER_MAX_VAL);

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
    const uint16_t* timerValuePtr, ///< [IN] Array of retry timer intervals, minutes.
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
        if (timerValuePtr[i] > LE_AVC_RETRY_TIMER_MAX_VAL)
        {
            LE_ERROR("Attemping to set an out-of-range Retry Timer value of %d. Max %d.",
                     timerValuePtr[i], LE_AVC_RETRY_TIMER_MAX_VAL);
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
    uint32_t* pollingTimerPtr  ///< [OUT] Polling timer interval, minutes
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
    if (pollingTimerCfg > LE_AVC_POLLING_TIMER_MAX_VAL)
    {
        LE_ERROR("The stored Polling Timer value %" PRIu32" is out of range. Max %d.",
                 pollingTimerCfg, LE_AVC_POLLING_TIMER_MAX_VAL);
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
 * Function to set the polling timer to a value in minutes
 *
 * @return
 *      - LE_OK on success.
 *      - LE_OUT_OF_RANGE if the polling timer value is out of range (0 to 525600).
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_SetPollingTimer
(
    uint32_t pollingTimer ///< [IN] Polling timer interval, minutes
)
{
    return avcServer_SetPollingTimerInSeconds(pollingTimer * SECONDS_IN_A_MIN);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the polling timer to a value in seconds
 *
 * @return
 *      - LE_OK on success.
 *      - LE_OUT_OF_RANGE if the polling timer value is out of range (0 to 525600).
 *      - LE_FAULT upon failure to set it.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_SetPollingTimerInSeconds
(
    uint32_t pollingTimeSecs ///< [IN] Polling timer interval, seconds
)
{
    le_result_t result = LE_OK;
    lwm2mcore_Sid_t sid;
    bool disabled = false;
    uint32_t pollingTimeMins = pollingTimeSecs / SECONDS_IN_A_MIN;

    // lifetime in the server object is in seconds and polling timer is in minutes
    uint32_t lifetime = pollingTimeSecs;

    // Stop polling timer if running
    if (PollingTimerRef != NULL)
    {
        if (le_timer_IsRunning(PollingTimerRef))
        {
            LE_ASSERT(LE_OK == le_timer_Stop(PollingTimerRef));
        }
    }

    // Disabled state is represented by either of 2 constants: 0 or 7300 days (20 years).
    if ((POLLING_TIMER_DISABLED == lifetime) ||
        (LWM2MCORE_LIFETIME_VALUE_DISABLED == lifetime))
    {
        disabled = true;
        // 0 is not a valid value for lifetime, a specific value (7300 days) has to be used
        lifetime = LWM2MCORE_LIFETIME_VALUE_DISABLED;
    }
    else if (pollingTimeMins > LE_AVC_POLLING_TIMER_MAX_VAL)
    {
        LE_ERROR("Attemping to set an out-of-range Polling Timer value of %"PRIu32" in seconds. "
                 "Min %"PRIu32", Max %"PRIu32, pollingTimeSecs,
                 LE_AVC_POLLING_TIMER_MIN_VAL * SECONDS_IN_A_MIN,
                 LE_AVC_POLLING_TIMER_MAX_VAL * SECONDS_IN_A_MIN);
        return LE_OUT_OF_RANGE;
    }

    // set lifetime in lwm2mcore
    sid = lwm2mcore_SetLifetime(lifetime);

    if (LWM2MCORE_ERR_COMPLETED_OK != sid)
    {
        LE_ERROR("Failed to set polling time to %"PRIu32" seconds; status ID %d", lifetime, sid);
        return LE_FAULT;
    }

    // Store the current time to avc config
    result = avcServer_SaveCurrentEpochTime();
    if (result != LE_OK)
    {
       LE_ERROR("Failed to set lifetime to %"PRIu32" seconds", lifetime);
       return LE_FAULT;
    }

    // Start the polling timer if enabled
    if (!disabled)
    {
        // Only set the polling timer if the platform is not connected
        if (LE_AVC_SESSION_INVALID != le_avc_GetSessionType())
        {
            LE_DEBUG("Connected to server: do not launch polling timer");
            return LE_OK;
        }

        LE_INFO("Polling Timer is set to start AVC session every %"PRIu32" seconds.", lifetime);

        // Set a timer to start the next session.
        le_clk_Time_t interval = {.sec = lifetime};

        LE_ASSERT(LE_OK == le_timer_SetInterval(PollingTimerRef, interval));
        le_result_t startResult = le_timer_Start(PollingTimerRef);
        if (startResult == LE_BUSY)
        {
            LE_WARN("Polling timer is already running.");
        }
        else if (startResult != LE_OK)
        {
            LE_FATAL("Setting polling timer failed with result %d %s", startResult,
                     LE_RESULT_TXT(startResult));
        }
    }
    else
    {
        LE_INFO("Polling timer disabled");
    }

    return result;
}


#if LE_CONFIG_AVC_FEATURE_EDM
//--------------------------------------------------------------------------------------------------
/**
 * Function to set the EDM polling timer to a value in seconds
 *
 * @return
 *      - LE_OK on success.
 *      - LE_OUT_OF_RANGE if the polling timer value is out of range (0 to 525600).
 *      - LE_TIMEOUT if timeout occured.
 *      - LE_FAULT upon failure to set it.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_SetEdmPollingTimerInSeconds
(
    uint32_t pollingTimeSecs ///< [IN] Polling timer interval, seconds
)
{
    return pa_avc_SetEdmPollingTimerInSeconds(pollingTimeSecs);
}
#endif // LE_CONFIG_AVC_FEATURE_EDM

//--------------------------------------------------------------------------------------------------
/**
 * Function to get the user agreement state
 *
 * @return
 *      - LE_OK on success
 *      - LE_UNAVAILABLE if reading the config file failed
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
            *isEnabledPtr = false;
            result = LE_FAULT;
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to verify the wake-up SMS digital signature.
 *
 * @return
 *      - true if this message is a valid wakeup SMS
 *      - false otherwise
 */
//--------------------------------------------------------------------------------------------------
bool VerifyWakeupSmsSignature
(
    uint8_t*    data,           ///< [IN] Digitally signed data
    size_t      dataLen,        ///< [IN] Data length
    uint8_t*    signature,      ///< [IN] Digital signature
    size_t      signatureLen    ///< [IN] Digital signature length
)
{
    uint8_t     digest[WAKEUP_SMS_DECODED_DATA_BUF_SIZE] = {0};
    size_t      digestLen = sizeof(digest);

    // Calculate the HMAC SHA256 digest
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_ComputeHmacSHA256(data, dataLen,
                                                            LWM2MCORE_CREDENTIAL_DM_SECRET_KEY,
                                                            digest, &digestLen))
    {
        LE_ERROR("Error calculating HMAC SHA256 for the wake-up SMS");
        return false;
    }

    // Check whether the digest matches the signature provided
    if (signatureLen != digestLen)
    {
        LE_ERROR("Signature length doesn't match expected: %zu, %zu", signatureLen, digestLen);
        return false;
    }
    if (0 == memcmp(signature, digest, signatureLen))
    {
        return true;
    }

    return false;
}


#ifdef LE_CONFIG_SMS_SERVICE_ENABLED
//--------------------------------------------------------------------------------------------------
/**
 * Function to process the SMS and check whether it's a valid wakeup command.
 *
 * @return
 *      - true if this message is a valid wakeup SMS
 *      - false otherwise
 *
 * @note Wake-up SMS Format:
 *       "LWM2M" + base64("WAKEUP" + '\0' + timestamp + hmac_sha256_signature)
 *       The "WAKEUP" order is followed by a null byte. This allows to support other orders than
 *       WAKEUP which may have a different size.
 *       Time stamp is a signed int32, representing epoch time in seconds. We do not expect
 *       to support this beyond 2038.
 *       The signature applies to the concatenation of the order (WAKEUP), the null byte and the
 *       timestamp.
 *       The key is the DM Pre-Shared Key (LWM2MCORE_CREDENTIAL_DM_SECRET_KEY).
 *       Signed data length is 11 bytes, signature size is 32 bytes.
 *
 */
//--------------------------------------------------------------------------------------------------
bool ProcessWakeupSms
(
    le_sms_MsgRef_t msgRef     ///< [IN] Message object received from the modem.
)
{
    char            text[LE_SMS_TEXT_MAX_BYTES] = {0};
    bool            isValid = false;
    le_clk_Time_t   CurrentTime = le_clk_GetRelativeTime();
    char            *encodedText;
    uint8_t         decodedData[WAKEUP_SMS_DECODED_DATA_BUF_SIZE] = {0};
    size_t          decodedLen;
    char*           commandPtr;
    int32_t         timeStamp;
    uint8_t         *signaturePtr;
    size_t          dataLen;
    size_t          signatureLen;

    if (LE_OK != le_sms_GetText(msgRef, text, sizeof(text)))
    {
        LE_ERROR("Can't get SMS text");
        return false;
    }

    // Check whether SMS starts with LWM2M
    if ((strlen(text) <= strlen(WAKEUP_SMS_PREFIX)) ||
        (strncmp(text, WAKEUP_SMS_PREFIX, strlen(WAKEUP_SMS_PREFIX)) != 0))
    {
        LE_INFO("SMS is too short or doesn't start with prefix '%s', ignoring", WAKEUP_SMS_PREFIX);
        return false;
    }

    // Decode the part of SMS that goes after prefix
    encodedText = text + strlen(WAKEUP_SMS_PREFIX);
    decodedLen = sizeof(decodedData);

    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_Base64Decode(encodedText, decodedData, &decodedLen))
    {
        LE_ERROR("Error Decoding data");
        return false;
    }

    // Check the command (located first in the decoded content)
    commandPtr = (char *) decodedData;
    LE_INFO("Message decoded: length %zu", decodedLen);
    if (strncmp(commandPtr, WAKEUP_COMMAND, strlen(WAKEUP_COMMAND)) != 0)
    {
        LE_INFO("Not a wakeup SMS - ignoring");
        return false;
    }

    // Extract the timestamp: located after the command and terminating zero.
    memcpy(&timeStamp, commandPtr + strlen(commandPtr) + 1, sizeof(timeStamp));

    // Convert the timestamp from little endian to host endianness if necessary.
    timeStamp = le32toh(timeStamp);

    LE_INFO("Wakeup SMS detected: timestamp is %d (last %d)", timeStamp, LastSmsTimeStamp);

    // Timestamp should be greater than previous one (protection from capturing and re-sending SMS)
    if (timeStamp <= LastSmsTimeStamp)
    {
        LE_ERROR("SMS timestamp check failed: current %u last %u", timeStamp, LastSmsTimeStamp);
        isValid = false;
    } // Check if incoming rate of wake-up SMS exceeds the limit
    else if (!le_clk_GreaterThan(CurrentTime, WakeUpSmsTimeout))
    {
        LE_INFO("Ratelimit exceeded: curr time %ld old %ld", CurrentTime.sec, WakeUpSmsTimeout.sec);
        isValid = false;
    }
    else // This is a valid Wake-up SMS
    {
        // Update the locally stored time stamp of the previous wakeup message
        LastSmsTimeStamp = timeStamp;

        // Signature starts right after the time stamp
        signaturePtr = (uint8_t *) commandPtr + strlen(commandPtr) + 1 + sizeof(timeStamp);

        // Digitally signed data includes: "WAKEUP" + '\0' + timestamp
        dataLen = strlen(WAKEUP_COMMAND) + 1 + sizeof(timeStamp);
        signatureLen = decodedLen - dataLen;

        // Verify the signature
        isValid = VerifyWakeupSmsSignature(decodedData, dataLen, signaturePtr, signatureLen);

        // Update the ratelimiting timeout
        WakeUpSmsTimeout = le_clk_Add(CurrentTime, WakeUpSmsInterval);
    }

    // Cleanup - the wakeup message doesn't need to be stored.
    // If it's not a wakeup command, the function will return earlier and message
    // won't be deleted from storage.
    if (LE_OK != le_sms_DeleteFromStorage(msgRef))
    {
        LE_ERROR("Error deleting wakeup SMS from storage");
    }

    return isValid;
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for wake-up SMS message reception.
 *
 */
//--------------------------------------------------------------------------------------------------
void RxMessageHandler
(
    le_sms_MsgRef_t msgRef,     ///< [IN] Message object received from the modem.
    void*           contextPtr  ///< [IN] The handler's context.
)
{
    bool isWakeup;
    le_sms_Format_t Format = le_sms_GetFormat(msgRef);

    switch(Format)
    {
        case LE_SMS_FORMAT_TEXT :
            isWakeup = ProcessWakeupSms(msgRef);
            // Start the session
            if (isWakeup)
            {
                LE_INFO("Wakeup SMS received - starting AV session");
                if (LE_OK != avcServer_StartSession(LE_AVC_SERVER_ID_AIRVANTAGE))
                {
                    LE_ERROR("Failed to start a new session");
                }
            }
            break;
        default :
            break;
    }

    // Dereference the message
    le_sms_Delete(msgRef);
}
#endif

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
 * Function to set the NAT timeout
 *
 * This function sets the NAT timeout in volatile memory.
 * When data need to be sent by the client, a check is made between this NAT timeout value and the
 * time when last data were received from the server or sent to the server.
 * If one of these times is greater than the NAT timeout, a DTLS resume is initiated.
 * Default value if this function is not called: 40 seconds (set in LwM2MCore).
 * Value 0 will deactivate any DTLS resume.
 * This function can be called at any time.
 */
//--------------------------------------------------------------------------------------------------
void le_avc_SetNatTimeout
(
    uint32_t timeout        ///< [IN] Timeout (unit: seconds)
)
{
    lwm2mcore_SetNatTimeout(timeout);
}

//--------------------------------------------------------------------------------------------------
/**
 * Check whether the session is started for a given Server Id.
 *
 * @return
 *      - true if session is started
 *      - false otherwise
 */
//--------------------------------------------------------------------------------------------------
bool le_avc_IsSessionStarted
(
    uint16_t serverId       ///< [IN] Short Server ID
)
{
    return avcClient_IsSessionStarted(serverId);
}

//--------------------------------------------------------------------------------------------------
/**
 * Provision a credential used for connecting to AirVantage.
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_SetCredential
(
    le_avc_CredentialType_t credType, ///< [IN] Credential type
    uint16_t                serverId, ///< [IN] LwM2M server identity
    const uint8_t*          credPtr,  ///< [IN] Credential
    size_t                  credSize  ///< [IN] Credential size in bytes
)
{
    lwm2mcore_Credentials_t lwm2mCredType;
    lwm2mcore_Sid_t         lwm2mStatus = LWM2MCORE_ERR_GENERAL_ERROR;

    // Map le_avc credential type to lwm2mcore credential type
    switch (credType)
    {
        case LE_AVC_FW_PUBLIC_KEY:
        {
            lwm2mCredType = LWM2MCORE_CREDENTIAL_FW_KEY;
            break;
        }
#if defined(LE_CONFIG_SOTA)
        case LE_AVC_SW_PUBLIC_KEY:
        {
            lwm2mCredType = LWM2MCORE_CREDENTIAL_SW_KEY;
            break;
        }
#endif // defined(LE_CONFIG_SOTA)
        case LE_AVC_BS_SERVER_ADDRESS:
        {
            lwm2mCredType = LWM2MCORE_CREDENTIAL_BS_ADDRESS;
            break;
        }
        case LE_AVC_BS_PSK_ID:
        {
            lwm2mCredType = LWM2MCORE_CREDENTIAL_BS_PUBLIC_KEY;
            break;
        }
        case LE_AVC_BS_PSK:
        {
            lwm2mCredType = LWM2MCORE_CREDENTIAL_BS_SECRET_KEY;
            break;
        }
        default:
        {
            LE_ERROR("API does not support setting credential type %u", credType);
            goto exit;
        }
    }

    lwm2mStatus = lwm2mcore_SetCredential(lwm2mCredType,
                                          serverId,
                                          (char*)credPtr,
                                          credSize);
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mStatus)
    {
        LE_ERROR("Failed to write LwM2M credential: %u", lwm2mStatus);
        goto exit;
    }

    LE_INFO("LwM2M cred %u successfully written", lwm2mCredType);

exit:
    return (LWM2MCORE_ERR_COMPLETED_OK == lwm2mStatus) ? LE_OK : LE_FAULT;
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

    // Create download start event
    LaunchDownloadEvent = le_event_CreateId("AVC launch download", 0);

    // Register handler for AVC service update status
    le_event_AddHandler("AVC Update Status event", AvcUpdateStatusEvent, ProcessUpdateStatus);

    // Register handler for download lauch
    le_event_AddHandler("AVC download launch event", LaunchDownloadEvent, LaunchDownload);

#ifdef LE_CONFIG_SMS_SERVICE_ENABLED
    // Register handler for SMS wakeup
    le_sms_AddRxMessageHandler(RxMessageHandler, NULL);
#endif

    // Create safe reference map for block references. The size of the map should be based on
    // the expected number of simultaneous block requests, so take a reasonable guess.
    BlockRefMap = le_ref_InitStaticMap(BlockRef, HIGH_BLOCK_REF_COUNT);

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

    LaunchInstallTimer = le_timer_Create("launch install timer");
    le_timer_SetHandler(LaunchInstallTimer, LaunchInstallExpiryHandler);

    LaunchRebootTimer = le_timer_Create("launch reboot timer");
    le_timer_SetHandler(LaunchRebootTimer, LaunchRebootExpiryHandler);

    LaunchConnectTimer = le_timer_Create("launch connection timer");
    le_timer_SetHandler(LaunchConnectTimer, LaunchConnectExpiryHandler);

    PollingTimerRef = le_timer_Create("polling Timer");
    le_timer_SetHandler(PollingTimerRef, PollingTimerExpiryHandler);

    StopCnxTimer = le_timer_Create("launch stop connection timer");
    le_timer_SetHandler(StopCnxTimer, StopConnectionExpiryHandler);

    // Initialize the sub-components
    if (LE_OK != packageDownloader_Init())
    {
        LE_ERROR("failed to initialize package downloader");
    }
#if LE_CONFIG_ENABLE_AV_DATA
#if LE_CONFIG_SOTA
    assetData_Init();
    timeSeries_Init();
    push_Init();
#endif /* end LE_CONFIG_SOTA */
    avData_Init();
#endif /* end LE_CONFIG_ENABLE_AV_DATA */
#if !MK_CONFIG_AVC_DISABLE_COAP
    coap_Init();
#endif /* end MK_CONFIG_AVC_DISABLE_COAP */
#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
    avFileTransfer_Init();
#endif /* LE_CONFIG_AVC_FEATURE_FILETRANSFER */
    avcClient_Init();
    tpfServer_Init();
    downloader_Init();

    // Read the user defined timeout from config tree @ /apps/avcService/activityTimeout
    int timeout = 20;
#if LE_CONFIG_ENABLE_CONFIG_TREE
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateReadTxn(AVC_SERVICE_CFG);
    timeout = le_cfg_GetInt(iterRef, "activityTimeout", 20);
    le_cfg_CancelTxn(iterRef);
#endif
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
    if (updateInfo_IsNewSys())
    {
        bool isFwUpdateOnGoing = false;
        LE_INFO("New system installed. Removing old SOTA/FOTA resume info");
#ifndef LE_CONFIG_CUSTOM_OS
        // New system installed, all old(SOTA or FOTA) resume info are invalid. Delete them.
        // Also packageDownloader workspace should be cleaned
        packageDownloader_DeleteResumeInfo();
#endif /* !LE_CONFIG_CUSTOM_OS */
#if LE_CONFIG_SOTA
        // Delete SOTA states and unfinished package if there exists any
        avcApp_DeletePackage();
#endif /* end LE_CONFIG_SOTA */

        // For FOTA new firmware upgrade cause device reboot. In that case, FW update state and
        // should be notified to server. In that case, don't delete FW update installation info.
        // Otherwise delete all FW update info.
        if ((LWM2MCORE_ERR_COMPLETED_OK == lwm2mcore_IsFwUpdateOnGoing(&isFwUpdateOnGoing)) &&
            (isFwUpdateOnGoing))
        {
            // FOTA installation on progress, keep only installation info and delete all resume
            // info.
            lwm2mcore_DeletePackageDownloaderResumeInfo();
        }
        else
        {
            // No FOTA/stale FOTA. Clear all FOTA related information include state and result
            packageDownloader_DeleteFwUpdateInfo();
            lwm2mcore_PackageDownloaderInit();
        }

        // Remove new system flag.
        updateInfo_RemoveNewSysFlag();
    }

    // Initialize application update module
#if LE_CONFIG_SOTA
    avcApp_Init();
#endif
    // Check if any notification needs to be sent to the application concerning
    // firmware update and application update
#if LE_CONFIG_SOTA
    CheckNotificationToSend(NULL, NULL);
#endif /* end LE_CONFIG_SOTA */
    LE_INFO("avcDaemon is ready");

    // Start watchdog on the main AVC event loop.
    // Try to kick a couple of times before each timeout.
    le_clk_Time_t watchdogInterval = { .sec = 8 };
    le_wdogChain_Init(1);
    le_wdogChain_MonitorEventLoop(0, watchdogInterval);
}
