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
#include "legato.h"
#include "interfaces.h"
#include "pa_avc.h"
#include "avcServer.h"
#include "avData.h"
#include "push.h"
#include "le_print.h"
#include "avcAppUpdate.h"
#include "packageDownloader.h"

//--------------------------------------------------------------------------------------------------
// Definitions
//--------------------------------------------------------------------------------------------------

#define AVC_SERVICE_CFG "/apps/avcService"

//--------------------------------------------------------------------------------------------------
/**
 *  Path to the lwm2m configurations in the Config Tree.
 */
//--------------------------------------------------------------------------------------------------
#define CFG_AVC_CONFIG_PATH "system:/apps/avcService/config"

//--------------------------------------------------------------------------------------------------
/**
 * This ref is returned when a status handler is added/registered.  It is used when the handler is
 * removed.  Only one ref is needed, because only one handler can be registered at a time.
 */
//--------------------------------------------------------------------------------------------------
#define REGISTERED_HANDLER_REF ((le_avc_StatusEventHandlerRef_t)0x1234)

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
#define TIMER_NAME_BYTES 10


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
    AVC_UNINSTALL_PENDING,         ///< Received pending uninstall; no response sent yet
    AVC_UNINSTALL_IN_PROGRESS      ///< Accepted uninstall, and in progress
}
AvcState_t;


//--------------------------------------------------------------------------------------------------
/**
 * Package download context
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint32_t pkgSize;               ///< Package size.
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
 * Handler registered by control app to receive status updates.  Only one is allowed.
 */
//--------------------------------------------------------------------------------------------------
static le_avc_StatusHandlerFunc_t StatusHandlerRef = NULL;

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
 * Is there a control app installed?  If so, we don't want to take automatic actions, even if
 * the control app has not yet registered a handler.  This flag is updated at COMPONENT_INIT,
 * and also when the control app explicitly registers.
 *
 * One case that is not currently handled is if the control app is uninstalled.  Thus, once this
 * flag is set to true, it will never be set to false. This is not expected to be a problem, but
 * if it becomes an issue, we could register for app installs and uninstalls.
 */
//--------------------------------------------------------------------------------------------------
static bool IsControlAppInstalled = false;

//--------------------------------------------------------------------------------------------------
/**
 * Is the current session owned by the control app?
 */
//--------------------------------------------------------------------------------------------------
static bool IsControlAppSession = false;

//--------------------------------------------------------------------------------------------------
/**
 * Context pointer associated with the above user registered handler to receive status updates.
 */
//--------------------------------------------------------------------------------------------------
static void* StatusHandlerContextPtr = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Reference for the registered control app.  Only one is allowed
 */
//--------------------------------------------------------------------------------------------------
static le_msg_SessionRef_t RegisteredControlAppRef = NULL;

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
        default:                            result = "Unknown";                 break;

    }
    return result;
}


//--------------------------------------------------------------------------------------------------
// Local functions
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Check to see if le_avc is bound to a client.
 */
//--------------------------------------------------------------------------------------------------
static bool IsAvcBound
(
    void
)
{
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateReadTxn("system:/apps");

    // If there are no apps, then there's no bindings.
    if (le_cfg_GoToFirstChild(iterRef) != LE_OK)
    {
        le_cfg_CancelTxn(iterRef);
        return false;
    }

    // Loop through all installed applications.
    do
    {
        // Check out all of the bindings for this application.
        le_cfg_GoToNode(iterRef, "./bindings");

        if (le_cfg_GoToFirstChild(iterRef) == LE_OK)
        {
            do
            {
                // Check to see if this binding is for the <root>.le_avc service.
                char strBuffer[LE_CFG_STR_LEN_BYTES] = "";

                le_cfg_GetString(iterRef, "./interface", strBuffer, sizeof(strBuffer), "");
                if (0 == strcmp(strBuffer, "le_avc"))
                {
                    // The app can be bound to the AVC app directly, or through the root user.
                    // so check for both.
                    le_cfg_GetString(iterRef, "./app", strBuffer, sizeof(strBuffer), "");
                    if (0 == strcmp(strBuffer, "avcService"))
                    {
                        // Success.
                        le_cfg_CancelTxn(iterRef);
                        return true;
                    }

                    le_cfg_GetString(iterRef, "./user", strBuffer, sizeof(strBuffer), "");
                    if (0 == strcmp(strBuffer, "root"))
                    {
                        // Success.
                        le_cfg_CancelTxn(iterRef);
                        return true;
                    }
                }
            }
            while (le_cfg_GoToNextSibling(iterRef) == LE_OK);

            le_cfg_GoToParent(iterRef);
        }

        le_cfg_GoToParent(iterRef);
    }
    while (le_cfg_GoToNextSibling(iterRef) == LE_OK);

    // The binding was not found.
    le_cfg_CancelTxn(iterRef);
    return false;
}


//--------------------------------------------------------------------------------------------------
/**
 * Stop the install defer timer if it is running.
 */
//--------------------------------------------------------------------------------------------------
static void StopInstallDeferTimer
(
    void
)
{
    // Stop the defer timer, if user accepts install before the defer timer expires.
    LE_DEBUG("Stop install defer timer.");
    le_timer_Stop(InstallDeferTimer);
}



//--------------------------------------------------------------------------------------------------
/**
 * Stop the download defer timer if it is running.
 */
//--------------------------------------------------------------------------------------------------
static void StopDownloadDeferTimer
(
    void
)
{
    // Stop the defer timer, if user accepts download before the defer timer expires.
    LE_DEBUG("Stop download defer timer.");
    le_timer_Stop(DownloadDeferTimer);
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop the uninstall defer timer if it is running.
 */
//--------------------------------------------------------------------------------------------------
static void StopUninstallDeferTimer
(
    void
)
{
    // Stop the defer timer, if user accepts uninstall before the defer timer expires.
    LE_DEBUG("Stop uninstall defer timer.");
    le_timer_Stop(UninstallDeferTimer);
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
    // If a user app is blocking the download, then just defer for some time.  Hopefully, the
    // next time this function is called, the user app will no longer be blocking the download.
    if ( BlockRefCount > 0 )
    {
        // Since the decision is not to install at this time, go back to idle
        CurrentState = AVC_IDLE;

        // Try the install later
        le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME*60 };

        le_timer_SetInterval(DownloadDeferTimer, interval);
        le_timer_Start(DownloadDeferTimer);
    }
    else
    {
        StopDownloadDeferTimer();

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
    // If a user app is blocking the install, then just defer for some time.  Hopefully, the
    // next time this function is called, the user app will no longer be blocking the install.
    if ( BlockRefCount > 0 )
    {
        // Since the decision is not to install at this time, go back to idle
        CurrentState = AVC_IDLE;

        // Try the install later
        le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME*60 };

        le_timer_SetInterval(InstallDeferTimer, interval);
        le_timer_Start(InstallDeferTimer);
    }
    else
    {
        StopInstallDeferTimer();

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
 * Handler to receive update status notifications from PA
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
    le_avc_Status_t reportStatus = LE_AVC_NO_UPDATE;

    // Keep track of the state of any pending downloads or installs.
    switch ( updateStatus )
    {
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

        case LE_AVC_DOWNLOAD_IN_PROGRESS:
            LE_DEBUG("Update type for DOWNLOAD is %d", updateType);
            CurrentTotalNumBytes = totalNumBytes;
            CurrentDownloadProgress = dloadProgress;
            CurrentUpdateType = updateType;

            if (LE_AVC_APPLICATION_UPDATE == updateType)
            {
                // Set the bytes downloaded to workspace for resume operation
                avcApp_SetSwUpdateBytesDownloaded();
            }
            break;

        case LE_AVC_DOWNLOAD_COMPLETE:
            LE_DEBUG("Update type for DOWNLOAD is %d", updateType);
            DownloadAgreement = false;
            CurrentTotalNumBytes = totalNumBytes;
            CurrentDownloadProgress = dloadProgress;
            CurrentUpdateType = updateType;

            if (LE_AVC_APPLICATION_UPDATE == updateType)
            {
                // Set the bytes downloaded to workspace for resume operation
                avcApp_SetSwUpdateBytesDownloaded();

                // End download and start unpack
                avcApp_EndDownload();
            }
            break;

        case LE_AVC_UNINSTALL_PENDING:
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
            AvcErrorCode = errorCode;
            CurrentState = AVC_IDLE;

            if (LE_AVC_APPLICATION_UPDATE == updateType)
            {
                avcApp_DeletePackage();
            }
            break;

        case LE_AVC_SESSION_STARTED:
            // Update object9 list managed by legato to lwm2mcore
            avcApp_NotifyObj9List();
            avData_ReportSessionState(LE_AVDATA_SESSION_STARTED);
            break;

        case LE_AVC_INSTALL_IN_PROGRESS:
        case LE_AVC_SESSION_STOPPED:
            // These events do not cause a state transition
            avData_ReportSessionState(LE_AVDATA_SESSION_STOPPED);
            break;

        default:
            LE_DEBUG("Unsupported updateStatus %d", updateStatus);
            break;
    }

    if ( StatusHandlerRef != NULL )
    {
        LE_DEBUG("Reporting status %d", updateStatus);
        LE_DEBUG("Total number of Bytes to download = %d", totalNumBytes);
        LE_DEBUG("Download progress = %d%%", dloadProgress);

        // Notify registered control app
        StatusHandlerRef( updateStatus,
                          totalNumBytes,
                          dloadProgress,
                          StatusHandlerContextPtr);
    }
    else if ( IsControlAppInstalled )
    {
        // There is a control app installed, but the handler is not yet registered.  Defer
        // the decision to allow the control app time to register.
        if ( updateStatus == LE_AVC_DOWNLOAD_PENDING )
        {
            LE_INFO("Automatically deferring download, while waiting for control app to register");
            DeferDownload(BLOCKED_DEFER_TIME);
        }
        else if ( updateStatus == LE_AVC_INSTALL_PENDING )
        {
            LE_INFO("Automatically deferring install, while waiting for control app to register");
            DeferInstall(BLOCKED_DEFER_TIME);
        }
        else
        {
            LE_DEBUG("No handler registered to receive status %i", updateStatus);
        }
    }

    else
    {
        // There is no control app; automatically accept any pending downloads.
        if ( updateStatus == LE_AVC_DOWNLOAD_PENDING )
        {
            LE_INFO("Automatically accepting download");
            AcceptDownloadPackage();
            CurrentState = AVC_DOWNLOAD_IN_PROGRESS;
        }

        // There is no control app; automatically accept any pending installs,
        // if there are no blocking apps, otherwise, defer the install.
        else if ( updateStatus == LE_AVC_INSTALL_PENDING )
        {
            if ( 0 == BlockRefCount )
            {
                LE_INFO("Automatically accepting install");
                AcceptInstallPackage();
                StopInstallDeferTimer();
                CurrentState = AVC_INSTALL_IN_PROGRESS;
            }
            else
            {
                LE_INFO("Automatically deferring install");
                DeferInstall(BLOCKED_DEFER_TIME);
            }
        }

        // Otherwise, log a message
        else
        {
            LE_DEBUG("No handler registered to receive status %i", updateStatus);
        }
    }
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

    // Release session owned by control app (only when control app closes).
    if (RegisteredControlAppRef == sessionRef)
    {
        LE_DEBUG("Close session owned by control app.");
        RegisteredControlAppRef = NULL;
        avcClient_Disconnect();
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

    // Release session owned by control app.
    if (IsControlAppSession)
    {
        avcClient_Disconnect();
        IsControlAppSession = false;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Determine whether the current client is the registered control app client.
 *
 * As a side-effect, will kill the client if it is not the registered control app client.
 */
//--------------------------------------------------------------------------------------------------
static bool IsValidControlAppClient
(
    void
)
{
    if ( (RegisteredControlAppRef == NULL) ||
         (RegisteredControlAppRef != le_avc_GetClientSessionRef()) )
    {
        LE_KILL_CLIENT("Client is not registered as control app");
        return false;
    }
    else
    {
        return true;
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
    le_result_t result = LE_BUSY;

    if ( StatusHandlerRef != NULL )
    {
        // Notify registered control app.
        LE_DEBUG("Reporting status LE_AVC_INSTALL_PENDING");
        CurrentState = AVC_INSTALL_PENDING;
        StatusHandlerRef(LE_AVC_INSTALL_PENDING,
                         -1,
                         -1,
                         StatusHandlerContextPtr);
    }

    else if ( IsControlAppInstalled )
    {
        // There is a control app installed, but the handler is not yet registered.  Defer
        // the decision to allow the control app time to register.
        LE_INFO("Automatically deferring install, while waiting for control app to register");

        // Try the install later
        le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME*60 };

        le_timer_SetInterval(InstallDeferTimer, interval);
        le_timer_Start(InstallDeferTimer);
    }

    else
    {
        // There is no control app; automatically accept any pending installs,
        // if there are no blocking apps, otherwise, defer the install.
        if ( 0 == BlockRefCount )
        {
            LE_INFO("Automatically accepting install");
            StopInstallDeferTimer();
            CurrentState = AVC_INSTALL_IN_PROGRESS;
            result = LE_OK;
        }
        else
        {
            LE_INFO("Automatically deferring install");

            // Try the install later
            le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME*60 };

            le_timer_SetInterval(InstallDeferTimer, interval);
            le_timer_Start(InstallDeferTimer);
        }
    }

    return result;
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
    le_result_t result = LE_BUSY;

    if (StatusHandlerRef != NULL)
    {
        // Notify registered control app.
        LE_DEBUG("Report status LE_AVC_DOWNLOAD_PENDING");
        CurrentState = AVC_DOWNLOAD_PENDING;
        StatusHandlerRef(LE_AVC_DOWNLOAD_PENDING, totalNumBytes, 0, StatusHandlerContextPtr);
    }
    else if (IsControlAppInstalled)
    {
        // There is a control app installed, but the handler is not yet registered.  Defer
        // the decision to allow the control app time to register.
        LE_INFO("Automatically deferring download, while waiting for control app to register");

        // Since the decision is not to download at this time, go back to idle
        CurrentState = AVC_IDLE;

        // Try the download later
        le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME*60 };

        le_timer_SetInterval(DownloadDeferTimer, interval);
        le_timer_Start(DownloadDeferTimer);
    }
    else
    {
        // There is no control app; automatically accept any pending downloads,
        // if there are no blocking apps, otherwise, defer the download.
        if (0 == BlockRefCount)
        {
            LE_INFO("Automatically accepting download");
            CurrentState = AVC_DOWNLOAD_IN_PROGRESS;
            result = LE_OK;
        }
        else
        {
            LE_INFO("Automatically deferring download");

            // Since the decision is not to download at this time, go back to idle
            CurrentState = AVC_IDLE;

            // Try the download later
            le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME*60 };

            le_timer_SetInterval(DownloadDeferTimer, interval);
            le_timer_Start(DownloadDeferTimer);
        }
    }

    return result;
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
    le_result_t result = LE_BUSY;

    if ( StatusHandlerRef != NULL )
    {
        // Notify registered control app.
        LE_DEBUG("Reporting status LE_AVC_UNINSTALL_PENDING");
        CurrentState = AVC_UNINSTALL_PENDING;
        StatusHandlerRef( LE_AVC_UNINSTALL_PENDING,
                          -1,
                          -1,
                          StatusHandlerContextPtr);
    }

    else if ( IsControlAppInstalled )
    {
        // There is a control app installed, but the handler is not yet registered.  Defer
        // the decision to allow the control app time to register.
        LE_INFO("Automatically deferring uninstall, while waiting for control app to register");

        // Try the uninstall later
        le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME*60 };

        le_timer_SetInterval(UninstallDeferTimer, interval);
        le_timer_Start(UninstallDeferTimer);
    }

    else
    {
        // There is no control app; automatically accept any pending uninstalls,
        // if there are no blocking apps, otherwise, defer the uninstall.
        if ( 0 == BlockRefCount )
        {
            LE_INFO("Automatically accepting uninstall");
            StopUninstallDeferTimer();
            CurrentState = AVC_UNINSTALL_IN_PROGRESS;
            result = LE_OK;
        }
        else
        {
            LE_INFO("Automatically deferring uninstall");

            // Try the uninstall later
            le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME*60 };

            le_timer_SetInterval(UninstallDeferTimer, interval);
            le_timer_Start(UninstallDeferTimer);
        }
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Called when the download defer timer expires.
 */
//--------------------------------------------------------------------------------------------------
void DownloadTimerExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    if ( QueryDownload(PkgDownloadCtx.pkgSize) == LE_OK )
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
void InstallTimerExpiryHandler
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
void UninstallTimerExpiryHandler
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
// Internal interface functions
//--------------------------------------------------------------------------------------------------


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
    uint32_t pkgSize                                ///< [IN] Package size.
)
{
    le_result_t result;

    if ( QueryDownloadHandlerRef != NULL )
    {
        LE_ERROR("Duplicate download attempt");
        return LE_FAULT;
    }

    // Update download handler
    PkgDownloadCtx.pkgSize = pkgSize;
    QueryDownloadHandlerRef = handlerFunc;

    result = QueryDownload(pkgSize);

    // Reset the handler as download can proceed now.
    if (LE_BUSY != result)
    {
        QueryDownloadHandlerRef = NULL;
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
    StopDownloadDeferTimer();
    QueryDownloadHandlerRef = NULL;

    StopInstallDeferTimer();
    QueryInstallHandlerRef = NULL;

    StopUninstallDeferTimer();
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
    if (StatusHandlerRef != NULL)
    {
        LE_DEBUG("Report install progress to registered handler.");

        // Notify registered control app
        StatusHandlerRef(updateStatus,
                         -1,
                         installProgress,
                         StatusHandlerContextPtr);
    }
    else
    {
        LE_DEBUG("No handler registered to receive install progress.");
    }

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
 *      - LE_OK if able to initiate a session open
 *      - LE_FAULT on error
 *      - LE_BUSY if session is owned by control app
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
    else if (!IsControlAppSession)
    {
        LE_DEBUG("Automatically accepting request to open session.");
        result = avcClient_Connect();
    }
    else
    {
        LE_DEBUG("Session owned by control app.");
        result = LE_BUSY;
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
    else if (!IsControlAppSession)
    {
        LE_DEBUG("Releasing session opened by user app.");
        result = avcClient_Disconnect();
    }
    else
    {
        LE_DEBUG("Session owned by control app.");
        result = LE_BUSY;
    }

    return result;
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

    // Only allow the handler to be registered, if nothing is currently registered. In this way,
    // only one user app is allowed to register at a time.
    if ( StatusHandlerRef != NULL )
    {
        LE_KILL_CLIENT("Handler already registered");
        return NULL;
    }

    StatusHandlerRef = handlerPtr;
    StatusHandlerContextPtr = contextPtr;

    // Store the client session ref, to ensure only the registered client can call the other
    // control related API functions.
    RegisteredControlAppRef = le_avc_GetClientSessionRef();

    // We only check at startup if the control app is installed, so this flag could be false
    // if the control app is installed later.  Obviously control app is installed now, so set
    // it to true, in case it is currently false.
    IsControlAppInstalled = true;

    // If Current State is not idle, this means that a user agreement is required before the
    // reboot. Notify the application for this event.
    if ( (AVC_DOWNLOAD_PENDING == CurrentState) || le_timer_IsRunning(DownloadDeferTimer))
    {
        // A user agreement for package download is required
        uint8_t downloadUri[LWM2MCORE_PACKAGE_URI_MAX_LEN+1];
        size_t uriLen = LWM2MCORE_PACKAGE_URI_MAX_LEN+1;
        lwm2mcore_UpdateType_t updateType = LWM2MCORE_MAX_UPDATE_TYPE;

        // Check if an update package URI is stored
        if (LE_OK == packageDownloader_GetResumeInfo(downloadUri, &uriLen, &updateType))
        {
            uint64_t packageSize = 0;
            // Get the package size
            if (LWM2MCORE_FW_UPDATE_TYPE == updateType)
            {
                if (LE_OK != packageDownloader_GetFwUpdatePackageSize(&packageSize))
                {
                    packageSize = 0;
                }
            }
            else if (LWM2MCORE_SW_UPDATE_TYPE == updateType)
            {
                // TODO: SOTA use case
            }
            else
            {
                // Issue
                packageSize = 0;
                CurrentState = AVC_IDLE;
            }
            LE_INFO("packageSize %llu", packageSize);

            if (packageSize)
            {
                // Notify the application of package download
                pkgDwlCb_UserAgreement((uint32_t)packageSize);
            }
        }
        else
        {
            LE_INFO("packageDownloader_GetResumeInfo ERROR");
        }
    }

    // Check for LE_AVC_INSTALL_COMPLETE or LE_AVC_INSTALL_FAILED notification for FOTA
    lwm2mcore_GetFirmwareUpdateInstallResult();

    if (AVC_INSTALL_PENDING == CurrentState)
    {
        bool isInstallRequest = false;
        if ( (LE_OK == packageDownloader_GetFwUpdateInstallPending(&isInstallRequest))
          && (isInstallRequest) )
        {
            // FOTA use case
            ResumeFwInstall();
        }
    }

    return REGISTERED_HANDLER_REF;
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
    if ( addHandlerRef != REGISTERED_HANDLER_REF )
    {
        if ( addHandlerRef == NULL )
        {
            // If le_avc_AddStatusEventHandler() returns NULL, the value is still stored by the
            // generated code and cleaned up when the client dies, thus this check is necessary.
            // TODO: Fix the generated code.
            LE_ERROR("NULL ref ignored");
            return;
        }
        else
        {
            LE_KILL_CLIENT("Invalid ref = %p", addHandlerRef);
        }
    }

    if ( StatusHandlerRef == NULL )
    {
        LE_KILL_CLIENT("Handler not registered");
    }

    // Clear all info related to the registered handler.  Note that our local 'UpdateHandler'
    // must stay registered with the PA to ensure that automatic actions are performed, and
    // the state is properly tracked.
    StatusHandlerRef = NULL;
    StatusHandlerContextPtr = NULL;
    RegisteredControlAppRef = NULL;

    // After the status handler is removed automatic (default) actions will be enabled.
    IsControlAppInstalled = false;
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
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_StartSession
(
    void
)
{
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

    IsControlAppSession = true;
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
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

    IsControlAppSession = false;
    return avcClient_Disconnect();
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
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

#ifdef LEGATO_LWM2M_CLIENT
    return avcClient_Update();
#else
    return LE_UNSUPPORTED;
#endif
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
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

    if ( CurrentState != AVC_DOWNLOAD_PENDING )
    {
        LE_ERROR("Expected AVC_DOWNLOAD_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    return AcceptDownloadPackage();
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
le_result_t DeferDownload
(
     uint32_t deferMinutes
        ///< [IN]
)
{
    if ( CurrentState != AVC_DOWNLOAD_PENDING )
    {
        LE_ERROR("Expected AVC_DOWNLOAD_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    // Since the decision is not to download at this time, go back to idle
    CurrentState = AVC_IDLE;

    // Try the download later
    le_clk_Time_t interval = { .sec = (deferMinutes*60) };

    le_timer_SetInterval(DownloadDeferTimer, interval);
    le_timer_Start(DownloadDeferTimer);

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
le_result_t le_avc_DeferDownload
(
    uint32_t deferMinutes
        ///< [IN]
)
{
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

    // Defer the download.
    return DeferDownload(deferMinutes);
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
    // If a user app is blocking the install, then just defer for some time.  Hopefully, the
    // next time this function is called, the user app will no longer be blocking the install.
    if ( BlockRefCount > 0 )
    {
        // Since the decision is not to install at this time, go back to idle
        CurrentState = AVC_IDLE;

        // Try the install later
        le_clk_Time_t interval = { .sec = BLOCKED_DEFER_TIME*60 };

        le_timer_SetInterval(UninstallDeferTimer, interval);
        le_timer_Start(UninstallDeferTimer);
    }
    else
    {
        StopUninstallDeferTimer();

        // Notify the registered handler to proceed with the install; only called once.
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
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

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
le_result_t DeferInstall
(
    uint32_t deferMinutes
        ///< [IN]
)
{
    if ( CurrentState != AVC_INSTALL_PENDING )
    {
        LE_ERROR("Expected AVC_INSTALL_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    if ( CurrentUpdateType == LE_AVC_FIRMWARE_UPDATE )
    {
        return LE_OK;
    }
    else if ( CurrentUpdateType == LE_AVC_APPLICATION_UPDATE )
    {
        // Try the install later
        le_clk_Time_t interval = { .sec = (deferMinutes*60) };

        le_timer_SetInterval(InstallDeferTimer, interval);
        le_timer_Start(InstallDeferTimer);

        return LE_OK;
    }
    else
    {
        LE_ERROR("Unknown update type");
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
    uint32_t deferMinutes
        ///< [IN]
)
{
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

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
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

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
    uint32_t deferMinutes
        ///< [IN]
)
{
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

    if ( CurrentState != AVC_UNINSTALL_PENDING )
    {
        LE_ERROR("Expected AVC_UNINSTALL_PENDING state; current state is %i", CurrentState);
        return LE_FAULT;
    }

    LE_DEBUG("Deferring Uninstall for %d minute.", deferMinutes);

    // Try the uninstall later
    le_clk_Time_t interval = { .sec = (deferMinutes*60) };

    le_timer_SetInterval(UninstallDeferTimer, interval);
    le_timer_Start(UninstallDeferTimer);

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
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

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
    if ( ! IsValidControlAppClient() )
    {
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
    if ( ! IsValidControlAppClient() )
    {
        return LE_FAULT;
    }

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
    return LE_AVC_HTTP_STATUS_INVALID;
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
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateReadTxn(CFG_AVC_CONFIG_PATH);
    le_result_t result;

    if (le_cfg_IsEmpty(iterRef, "apn"))
    {
        le_cfg_CancelTxn(iterRef);
        return LE_FAULT;
    }

    le_cfg_GoToNode(iterRef, "apn");

    result = le_cfg_GetString(iterRef, "name", apnName, apnNameNumElements, "");

    if (result != LE_OK)
    {
        LE_ERROR("Failed to get APN Name.");
        goto done;
    }

    result = le_cfg_GetString(iterRef, "userName", userName, userNameNumElements, "");

    if (result != LE_OK)
    {
        LE_ERROR("Failed to get APN User Name.");
        goto done;
    }

    result = le_cfg_GetString(iterRef, "password", userPassword, userPasswordNumElements, "");

    if (result != LE_OK)
    {
        LE_ERROR("Failed to get APN Password.");
        goto done;
    }

done:
    le_cfg_CancelTxn(iterRef);
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
    if ((strlen(apnName) > LE_AVC_APN_NAME_MAX_LEN) ||
        (strlen(userName) > LE_AVC_USERNAME_MAX_LEN) ||
        (strlen(userPassword) > LE_AVC_PASSWORD_MAX_LEN))
    {
        return LE_OVERFLOW;
    }

    le_cfg_IteratorRef_t iterRef = le_cfg_CreateWriteTxn(CFG_AVC_CONFIG_PATH);

    le_cfg_GoToNode(iterRef, "apn");
    le_cfg_SetString(iterRef, "name", apnName);
    le_cfg_SetString(iterRef, "userName", userName);
    le_cfg_SetString(iterRef, "password", userPassword);

    le_cfg_CommitTxn(iterRef);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to read the retry timers.
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if not able to read the timers.
 *
 * @deprecated This API should not be used for new applications and will be removed in the future
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_GetRetryTimers
(
    uint16_t* timerValuePtr,  ///< [OUT] Retry timer array
    size_t* numTimers         ///< [IN/OUT] Max num of timers to get/num of timers retrieved
)
{
    if (numTimers < LE_AVC_NUM_RETRY_TIMERS)
    {
        LE_ERROR("Supplied retry timer array too small (%d). Expected %d.",
                 numTimers, LE_AVC_NUM_RETRY_TIMERS);
        return LE_FAULT;
    }

    le_cfg_IteratorRef_t iterRef = le_cfg_CreateReadTxn(CFG_AVC_CONFIG_PATH);

    if (le_cfg_IsEmpty(iterRef, "retryTimers"))
    {
        le_cfg_CancelTxn(iterRef);
        return LE_FAULT;
    }

    le_cfg_GoToNode(iterRef, "retryTimers");

    char timerName[TIMER_NAME_BYTES] = {0};
    int i;
    for (i = 0; i < LE_AVC_NUM_RETRY_TIMERS; i++)
    {
        snprintf(timerName, TIMER_NAME_BYTES, "%d", i);
        timerValuePtr[i] = le_cfg_GetInt(iterRef, timerName, 0);
    }

    le_cfg_CancelTxn(iterRef);

    *numTimers = LE_AVC_NUM_RETRY_TIMERS;

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to set the retry timers.
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if not able to read the timers.
 *
 * @deprecated This API should not be used for new applications and will be removed in the future
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_SetRetryTimers
(
    const uint16_t* timerValuePtr, ///< [IN] Retry timer array
    size_t numTimers               ///< [IN] Number of retry timers
)
{
    if (numTimers < LE_AVC_NUM_RETRY_TIMERS)
    {
        LE_ERROR("Supplied retry timer array too small (%d). Expected %d.",
                 numTimers, LE_AVC_NUM_RETRY_TIMERS);
        return LE_FAULT;
    }

    le_cfg_IteratorRef_t iterRef = le_cfg_CreateWriteTxn(CFG_AVC_CONFIG_PATH);

    le_cfg_GoToNode(iterRef, "retryTimers");

    char timerName[TIMER_NAME_BYTES] = {0};
    int i;
    for (i = 0; i < LE_AVC_NUM_RETRY_TIMERS; i++)
    {
        snprintf(timerName, TIMER_NAME_BYTES, "%d", i);
        le_cfg_SetInt(iterRef, timerName, timerValuePtr[i]);
    }

    le_cfg_CommitTxn(iterRef);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to read the polling timer.
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT if not available
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_GetPollingTimer
(
    uint32_t* pollingTimerPtr  ///< [OUT] Polling timer
)
{
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateReadTxn(CFG_AVC_CONFIG_PATH);

    if (le_cfg_IsEmpty(iterRef, "pollingTimer"))
    {
        le_cfg_CancelTxn(iterRef);
        return LE_FAULT;
    }

    *pollingTimerPtr = le_cfg_GetInt(iterRef, "pollingTimer", 0);

    le_cfg_CancelTxn(iterRef);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to set the polling timer.
 *
 * @return
 *      - LE_OK on success.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avc_SetPollingTimer
(
    uint32_t pollingTimer ///< [IN] Polling timer
)
{
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateWriteTxn(CFG_AVC_CONFIG_PATH);

    le_cfg_SetInt(iterRef, "pollingTimer", pollingTimer);

    le_cfg_CommitTxn(iterRef);

    return LE_OK;
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
void avcServer_NotifyUserApp
(
    le_avc_Status_t updateStatus,
    uint numBytes,
    uint progress,
    le_avc_ErrorCode_t errorCode
)
{
    CurrentState = updateStatus;

    if (StatusHandlerRef != NULL)
    {
        LE_DEBUG("Report progress to registered handler.");

        // Notify registered control app
        StatusHandlerRef(updateStatus,
                         numBytes,
                         progress,
                         StatusHandlerContextPtr);
    }
    else
    {
        LE_DEBUG("No handler registered to receive progress.");
    }

    if (updateStatus == LE_AVC_INSTALL_FAILED)
    {
        LE_ERROR("Error in update Status %d.", errorCode);
        AvcErrorCode = errorCode;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the default AVMS config, only if no config exists.
 */
//--------------------------------------------------------------------------------------------------
static void SetDefaultAVMSConfig
(
    void
)
{
    /* Default values */
    uint32_t pollingTimer = 0;
    uint16_t timerValue[LE_AVC_NUM_RETRY_TIMERS] = {0};
    size_t numTimers = LE_AVC_NUM_RETRY_TIMERS;

    // dummy variables used to see if there are any current configs present
    uint32_t pollingTimerCurr = 0;
    uint16_t timerValueCurr[LE_AVC_NUM_RETRY_TIMERS] = {0};
    size_t numTimersCurr = 0;

    if (LE_FAULT == le_avc_GetPollingTimer(&pollingTimerCurr))
    {
        le_avc_SetPollingTimer(pollingTimer);
    }

    if (LE_FAULT == le_avc_GetRetryTimers(timerValueCurr, &numTimersCurr))
    {
        le_avc_SetRetryTimers(timerValue, numTimers);
    }
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
    le_avc_Status_t updateStatus = LE_AVC_NO_UPDATE;
    bool isInstallRequest = false;
    uint8_t downloadUri[LWM2MCORE_PACKAGE_URI_MAX_LEN+1];
    size_t uriLen = LWM2MCORE_PACKAGE_URI_MAX_LEN+1;
    lwm2mcore_UpdateType_t updateType = LWM2MCORE_MAX_UPDATE_TYPE;
    avcApp_InternalState_t internalState;

    lwm2mcore_FwUpdateState_t fwUpdateState = LWM2MCORE_FW_UPDATE_STATE_IDLE;
    lwm2mcore_FwUpdateResult_t fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;

    lwm2mcore_SwUpdateState_t swUpdateState = LWM2MCORE_SW_UPDATE_STATE_INITIAL;
    lwm2mcore_SwUpdateResult_t swUpdateResult = LWM2MCORE_SW_UPDATE_RESULT_INITIAL;

    if (   (LE_OK == packageDownloader_GetFwUpdateState(&fwUpdateState))
        && (LE_OK == packageDownloader_GetFwUpdateResult(&fwUpdateResult))
        && (LE_OK == packageDownloader_GetSwUpdateState(&swUpdateState))
        && (LE_OK == packageDownloader_GetSwUpdateResult(&swUpdateResult)))
    {
        // Check if an update package URI is stored
        if (LE_OK == packageDownloader_GetResumeInfo(downloadUri, &uriLen, &updateType))
        {
            if (LWM2MCORE_FW_UPDATE_TYPE == updateType)
            {
                // Check if a FW update was ongoing
                if ( (LWM2MCORE_FW_UPDATE_STATE_DOWNLOADING == fwUpdateState)
                  && (LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL == fwUpdateResult))
                {
                    // Package is under download
                    updateStatus = LE_AVC_DOWNLOAD_IN_PROGRESS;
                }

                if ( (LWM2MCORE_FW_UPDATE_STATE_IDLE == fwUpdateState)
                       && (LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL == fwUpdateResult))
                {
                    // A Package URI is stored but the download is not launched
                    // Send a notification: LE_AVC_DOWNLOAD_PENDING
                    updateStatus = LE_AVC_DOWNLOAD_PENDING;
                }
            }
            else if (LWM2MCORE_SW_UPDATE_TYPE == updateType)
            {
                LE_DEBUG("SW update type");

                // Check if a SW update was ongoing
                if ((LWM2MCORE_SW_UPDATE_STATE_DOWNLOAD_STARTED == swUpdateState)
                    && (LWM2MCORE_SW_UPDATE_RESULT_INITIAL == swUpdateResult))
                {
                    updateStatus = LE_AVC_DOWNLOAD_IN_PROGRESS;
                }

                if ((LWM2MCORE_SW_UPDATE_STATE_INITIAL == swUpdateState)
                    && (LE_OK == avcApp_GetSwUpdateInternalState(&internalState))
                    && (INTERNAL_STATE_DOWNLOAD_REQUESTED == internalState))
                {
                    // Download requested from the server
                    // Send a notification: LE_AVC_DOWNLOAD_PENDING
                    updateStatus = LE_AVC_DOWNLOAD_PENDING;
                }
            }
            else
            {
                LE_ERROR("Incorrect update type");
            }
        }
    }

    // Check LE_AVC_INSTALL_PENDING notification for FOTA
    if ( (LE_OK == packageDownloader_GetFwUpdateInstallPending(&isInstallRequest))
      && isInstallRequest)
    {
        updateStatus = LE_AVC_INSTALL_PENDING;
        updateType = LWM2MCORE_FW_UPDATE_TYPE;
    }

    LE_INFO("Init: updateStatus %d, updateType %d", updateStatus, updateType);

    if (LE_AVC_NO_UPDATE != updateStatus)
    {
        // Send a notification to the application
        avcServer_UpdateHandler(updateStatus,
                                updateType,
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

    // Read the user defined timeout from config tree @ /apps/avcService/modemActivityTimeout
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateReadTxn(AVC_SERVICE_CFG);
    int timeout = le_cfg_GetInt(iterRef, "modemActivityTimeout", 20);
    le_cfg_CancelTxn(iterRef);

    // Check to see if le_avc is bound, which means there is an installed control app.
    IsControlAppInstalled = IsAvcBound();
    LE_INFO("Is control app installed? %i", IsControlAppInstalled);

    // Set default AVMS config values
    SetDefaultAVMSConfig();

    // Initialize user agreement.
    avcServer_InitUserAgreement();

    // Check if any notification needs to be sent to the application concerning
    // firmware update and application update
    CheckNotificationAtStartup();

    avcApp_Init();
}

