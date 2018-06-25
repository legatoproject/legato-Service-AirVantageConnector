/**
 * @file packageDownloader.c
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <legato.h>
#include <interfaces.h>
#include <lwm2mcorePackageDownloader.h>
#include <lwm2mcore/update.h>
#include <lwm2mcore/security.h>
#include "packageDownloaderCallbacks.h"
#include "packageDownloader.h"
#include "avcAppUpdate.h"
#include "avcFs.h"
#include "avcFsConfig.h"
#include "sslUtilities.h"
#include "avcClient.h"
#include "avcServer.h"

//--------------------------------------------------------------------------------------------------
/**
 * Download statuses
 */
//--------------------------------------------------------------------------------------------------
#define DOWNLOAD_STATUS_IDLE        0x00
#define DOWNLOAD_STATUS_ACTIVE      0x01
#define DOWNLOAD_STATUS_ABORT       0x02
#define DOWNLOAD_STATUS_SUSPEND     0x03

//--------------------------------------------------------------------------------------------------
/**
 * Static downloader thread reference.
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t DownloadRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static store FW thread reference.
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t StoreFwRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static package downloader structure
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_PackageDownloader_t PkgDwl;

//--------------------------------------------------------------------------------------------------
/**
 * Current download status.
 */
//--------------------------------------------------------------------------------------------------
static uint8_t DownloadStatus = DOWNLOAD_STATUS_IDLE;

//--------------------------------------------------------------------------------------------------
/**
 * Mutex to prevent race condition between threads.
 */
//--------------------------------------------------------------------------------------------------
static pthread_mutex_t DownloadStatusMutex = PTHREAD_MUTEX_INITIALIZER;

//--------------------------------------------------------------------------------------------------
/**
 * Macro used to prevent race condition between threads.
 */
//--------------------------------------------------------------------------------------------------
#define LOCK()    LE_FATAL_IF((pthread_mutex_lock(&DownloadStatusMutex)!=0), \
                               "Could not lock the mutex")
#define UNLOCK()  LE_FATAL_IF((pthread_mutex_unlock(&DownloadStatusMutex)!=0), \
                               "Could not unlock the mutex")

//--------------------------------------------------------------------------------------------------
/**
 * Firmware update notification structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    bool                notifRequested;     ///< Indicates if a notification is requested
    le_avc_Status_t     updateStatus;       ///< Update status
    le_avc_ErrorCode_t  errorCode;          ///< Error code
}
FwUpdateNotif_t;

//--------------------------------------------------------------------------------------------------
/**
 * Send a registration update to the server in order to follow the update treatment
 */
//--------------------------------------------------------------------------------------------------
static void UpdateStatus
(
    void* param1,
    void* param2
)
{
    avcClient_Update();
}

//--------------------------------------------------------------------------------------------------
/**
 * Set download status
 */
//--------------------------------------------------------------------------------------------------
static void SetDownloadStatus
(
    uint8_t newDownloadStatus   ///< New download status to set
)
{
    LOCK();
    DownloadStatus = newDownloadStatus;
    UNLOCK();
}

//--------------------------------------------------------------------------------------------------
/**
 * Get download status
 */
//--------------------------------------------------------------------------------------------------
static uint8_t GetDownloadStatus
(
    void
)
{
    uint8_t currentDownloadStatus;

    LOCK();
    currentDownloadStatus = DownloadStatus;
    UNLOCK();

    return currentDownloadStatus;
}

//--------------------------------------------------------------------------------------------------
/**
 * Abort current download
 */
//--------------------------------------------------------------------------------------------------
static void AbortDownload
(
    void
)
{
    LE_DEBUG("Abort download, download status was %d", GetDownloadStatus());

    // Suspend ongoing download
    SetDownloadStatus(DOWNLOAD_STATUS_ABORT);
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if the current download should be aborted
 *
 * @return
 *      True    Download abort is requested
 *      False   Download can continue
 */
//--------------------------------------------------------------------------------------------------
bool packageDownloader_CheckDownloadToAbort
(
    void
)
{
    if (DOWNLOAD_STATUS_ABORT == GetDownloadStatus())
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if the current download should be suspended
 *
 * @return
 *      True    Download suspend is requested
 *      False   Download can continue
 */
//--------------------------------------------------------------------------------------------------
bool packageDownloader_CheckDownloadToSuspend
(
    void
)
{
    if (DOWNLOAD_STATUS_SUSPEND == GetDownloadStatus())
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Store package information necessary to resume a download if necessary (URI and package type)
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Incorrect parameter provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetResumeInfo
(
    char* uriPtr,                   ///< [IN] package URI
    lwm2mcore_UpdateType_t type     ///< [IN] Update type
)
{
    if (!uriPtr)
    {
        return LE_BAD_PARAMETER;
    }

    le_result_t result;
    result = WriteFs(PACKAGE_URI_FILENAME, (uint8_t*)uriPtr, strlen(uriPtr));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", PACKAGE_URI_FILENAME, LE_RESULT_TXT(result));
        return result;
    }

    result = WriteFs(UPDATE_TYPE_FILENAME, (uint8_t*)&type, sizeof(lwm2mcore_UpdateType_t));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", UPDATE_TYPE_FILENAME, LE_RESULT_TXT(result));
        return result;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete package information necessary to resume a download (URI and package type)
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_DeleteResumeInfo
(
    void
)
{
    le_result_t result;
    result = DeleteFs(PACKAGE_URI_FILENAME);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to delete %s: %s", PACKAGE_URI_FILENAME, LE_RESULT_TXT(result));
        return result;
    }

    result = DeleteFs(UPDATE_TYPE_FILENAME);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to delete %s: %s", UPDATE_TYPE_FILENAME, LE_RESULT_TXT(result));
        return result;
    }

    result = DeleteFs(PACKAGE_SIZE_FILENAME);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to delete %s: %s", PACKAGE_SIZE_FILENAME, LE_RESULT_TXT(result));
        return result;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete FW update related info.
 */
//--------------------------------------------------------------------------------------------------
void packageDownloader_DeleteFwUpdateInfo
(
    void
)
{   // Deleting FW_UPDATE_STATE_PATH and FW_UPDATE_RESULT_PATH will be ok, as function for getting
    // FW update state/result should handle the situation when these files don't exist in flash.
    DeleteFs(FW_UPDATE_STATE_PATH);
    DeleteFs(FW_UPDATE_RESULT_PATH);
    DeleteFs(FW_UPDATE_NOTIFICATION_PATH);
    DeleteFs(FW_UPDATE_INSTALL_PENDING_PATH);
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve package information necessary to resume a download (URI and package type)
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Incorrect parameter provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetResumeInfo
(
    char* uriPtr,                       ///< [INOUT] package URI
    size_t* uriSizePtr,                  ///< [INOUT] package URI length
    lwm2mcore_UpdateType_t* typePtr     ///< [INOUT] Update type
)
{
    if (   (!uriPtr) || (!uriSizePtr) || (!typePtr)
        || (*uriSizePtr < LWM2MCORE_PACKAGE_URI_MAX_BYTES)
       )
    {
        return LE_BAD_PARAMETER;
    }

    le_result_t result;
    result = ReadFs(PACKAGE_URI_FILENAME, (uint8_t*)uriPtr, uriSizePtr);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to read %s: %s", PACKAGE_URI_FILENAME, LE_RESULT_TXT(result));
        return result;
    }

    if (*uriSizePtr > LWM2MCORE_PACKAGE_URI_MAX_LEN)
    {
        LE_ERROR("Uri length too big. Max allowed: %d, Found: %zd",
                  LWM2MCORE_PACKAGE_URI_MAX_LEN,
                  *uriSizePtr);
        return LE_FAULT;
    }

    uriPtr[*uriSizePtr] = '\0';

    size_t fileLen = sizeof(lwm2mcore_UpdateType_t);
    result = ReadFs(UPDATE_TYPE_FILENAME, (uint8_t*)typePtr, &fileLen);
    if ((LE_OK != result) || (sizeof(lwm2mcore_UpdateType_t) != fileLen))
    {
        LE_ERROR("Failed to read %s: %s", UPDATE_TYPE_FILENAME, LE_RESULT_TXT(result));
        *typePtr = LWM2MCORE_MAX_UPDATE_TYPE;
        return result;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Setup temporary files
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_Init
(
    void
)
{
    struct stat sb;

    if (-1 == stat(PKGDWL_TMP_PATH, &sb))
    {
        if (-1 == mkdir(PKGDWL_TMP_PATH, S_IRWXU))
        {
            LE_ERROR("failed to create pkgdwl directory %m");
            return LE_FAULT;
        }
    }

    if ( (-1 == mkfifo(FIFO_PATH, S_IRUSR | S_IWUSR)) && (EEXIST != errno) )
    {
        LE_ERROR("failed to create fifo: %m");
        return LE_FAULT;
    }

    if (LE_OK != ssl_CheckCertificate())
    {
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set firmware update state
 *
 * @return
 *  - DWL_OK     The function succeeded
 *  - DWL_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetFwUpdateState
(
    lwm2mcore_FwUpdateState_t fwUpdateState     ///< [IN] New FW update state
)
{
    le_result_t result;

    result = WriteFs(FW_UPDATE_STATE_PATH,
                     (uint8_t*)&fwUpdateState,
                     sizeof(lwm2mcore_FwUpdateState_t));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", FW_UPDATE_STATE_PATH, LE_RESULT_TXT(result));
        return DWL_FAULT;
    }

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set firmware update result
 *
 * @return
 *  - DWL_OK     The function succeeded
 *  - DWL_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetFwUpdateResult
(
    lwm2mcore_FwUpdateResult_t fwUpdateResult   ///< [IN] New FW update result
)
{
    le_result_t result;

    result = WriteFs(FW_UPDATE_RESULT_PATH,
                     (uint8_t*)&fwUpdateResult,
                     sizeof(lwm2mcore_FwUpdateResult_t));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", FW_UPDATE_RESULT_PATH, LE_RESULT_TXT(result));
        return DWL_FAULT;
    }

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function for setting software update state
 *
 * @return
 *  - DWL_OK     The function succeeded
 *  - DWL_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetSwUpdateState
(
    lwm2mcore_SwUpdateState_t swUpdateState     ///< [IN] New SW update state
)
{
    le_result_t result;
    result = avcApp_SetSwUpdateState(swUpdateState);

    if (LE_OK != result)
    {
        LE_ERROR("Failed to set SW update state: %d. %s",
                 (int)swUpdateState,
                 LE_RESULT_TXT(result));
        return DWL_FAULT;
    }

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function for setting software update result
 *
 * @return
 *  - DWL_OK     The function succeeded
 *  - DWL_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetSwUpdateResult
(
    lwm2mcore_SwUpdateResult_t swUpdateResult   ///< [IN] New SW update result
)
{
    le_result_t result;
    result = avcApp_SetSwUpdateResult(swUpdateResult);

    if (LE_OK != result)
    {
        LE_ERROR("Failed to set SW update result: %d. %s",
                 (int)swUpdateResult,
                 LE_RESULT_TXT(result));
        return DWL_FAULT;
    }

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update state
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateState
(
    lwm2mcore_FwUpdateState_t* fwUpdateStatePtr     ///< [INOUT] FW update state
)
{
    lwm2mcore_FwUpdateState_t updateState;
    size_t size;
    le_result_t result;

    if (!fwUpdateStatePtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_FAULT;
    }

    size = sizeof(lwm2mcore_FwUpdateState_t);
    result = ReadFs(FW_UPDATE_STATE_PATH, (uint8_t*)&updateState, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_WARN("FW update state not found");
            *fwUpdateStatePtr = LWM2MCORE_FW_UPDATE_STATE_IDLE;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", FW_UPDATE_STATE_PATH, LE_RESULT_TXT(result));
        return result;
    }
    LE_DEBUG("FW Update state %d", updateState);

    *fwUpdateStatePtr = updateState;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update result
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateResult
(
    lwm2mcore_FwUpdateResult_t* fwUpdateResultPtr   ///< [INOUT] FW update result
)
{
    lwm2mcore_FwUpdateResult_t updateResult;
    size_t size;
    le_result_t result;

    if (!fwUpdateResultPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_BAD_PARAMETER;
    }

    size = sizeof(lwm2mcore_FwUpdateResult_t);
    result = ReadFs(FW_UPDATE_RESULT_PATH, (uint8_t*)&updateResult, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_WARN("FW update result not found");
            *fwUpdateResultPtr = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", FW_UPDATE_RESULT_PATH, LE_RESULT_TXT(result));
        return result;
    }
    LE_DEBUG("FW Update result %d", updateResult);

    *fwUpdateResultPtr = updateResult;

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update install pending status
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateInstallPending
(
    bool* isFwInstallPendingPtr                  ///< [OUT] Is FW install pending?
)
{
    size_t size;
    bool isInstallPending;
    le_result_t result;

    if (!isFwInstallPendingPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_BAD_PARAMETER;
    }

    size = sizeof(bool);
    result = ReadFs(FW_UPDATE_INSTALL_PENDING_PATH, (uint8_t*)&isInstallPending, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_WARN("FW update install pending not found");
            *isFwInstallPendingPtr = false;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", FW_UPDATE_INSTALL_PENDING_PATH, LE_RESULT_TXT(result));
        return result;
    }
    LE_DEBUG("FW Install pending %d", isInstallPending);

    *isFwInstallPendingPtr = isInstallPending;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set firmware update install pending status
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateInstallPending
(
    bool isFwInstallPending                     ///< [IN] Is FW install pending?
)
{
    le_result_t result;
    LE_DEBUG("packageDownloader_SetFwUpdateInstallPending set %d", isFwInstallPending);

    result = WriteFs(FW_UPDATE_INSTALL_PENDING_PATH, (uint8_t*)&isFwInstallPending, sizeof(bool));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", FW_UPDATE_INSTALL_PENDING_PATH, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Save package size
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetUpdatePackageSize
(
    uint64_t size           ///< [IN] Package size
)
{
    le_result_t result;

    result = WriteFs(PACKAGE_SIZE_FILENAME, (uint8_t*)&size, sizeof(uint64_t));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", PACKAGE_SIZE_FILENAME, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get package size
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetUpdatePackageSize
(
    uint64_t* packageSizePtr        ///< [OUT] Package size
)
{
    le_result_t result;
    uint64_t packageSize;
    size_t size = sizeof(uint64_t);

    if (!packageSizePtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_FAULT;
    }

    result = ReadFs(PACKAGE_SIZE_FILENAME, (uint8_t*)&packageSize, &size);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to read %s: %s", PACKAGE_SIZE_FILENAME, LE_RESULT_TXT(result));
        return LE_FAULT;
    }
    *packageSizePtr = packageSize;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set firmware update notification.
 * This is used to indicate if the FOTA result needs to be notified to the application and sent to
 * the server after an install.
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateNotification
(
    bool                notifRequested,     ///< [IN] Indicates if a notification is requested
    le_avc_Status_t     updateStatus,       ///< [IN] Update status
    le_avc_ErrorCode_t  errorCode           ///< [IN] Error code
)
{
    FwUpdateNotif_t notification;
    notification.notifRequested = notifRequested;
    notification.updateStatus = updateStatus;
    notification.errorCode = errorCode;

    le_result_t result = WriteFs(FW_UPDATE_NOTIFICATION_PATH,
                                 (uint8_t*)&notification,
                                 sizeof(FwUpdateNotif_t));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", FW_UPDATE_NOTIFICATION_PATH, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update notification
 * This is used to check if the FOTA result needs to be notified to the application and sent to
 * the server after an install.
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateNotification
(
    bool*               notifRequestedPtr,  ///< [OUT] Indicates if a notification is requested
    le_avc_Status_t*    updateStatusPtr,    ///< [OUT] Update status
    le_avc_ErrorCode_t* errorCodePtr        ///< [OUT] Error code
)
{
    le_result_t result;
    FwUpdateNotif_t notification;
    size_t size = sizeof(FwUpdateNotif_t);

    if ((!notifRequestedPtr) || (!updateStatusPtr) || (!errorCodePtr))
    {
        LE_ERROR("Invalid input parameter");
        return LE_FAULT;
    }

    result = ReadFs(FW_UPDATE_NOTIFICATION_PATH, (uint8_t*)&notification, &size);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to read %s: %s", FW_UPDATE_NOTIFICATION_PATH, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    *notifRequestedPtr = notification.notifRequested;
    *updateStatusPtr = notification.updateStatus;
    *errorCodePtr = notification.errorCode;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update state
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetSwUpdateState
(
    lwm2mcore_SwUpdateState_t* swUpdateStatePtr     ///< [INOUT] SW update state
)
{
    lwm2mcore_SwUpdateState_t updateState;
    size_t size;
    le_result_t result;

    if (!swUpdateStatePtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_FAULT;
    }

    size = sizeof(lwm2mcore_SwUpdateState_t);
    result = ReadFs(SW_UPDATE_STATE_PATH, (uint8_t *)&updateState, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_WARN("SW update state not found");
            *swUpdateStatePtr = LWM2MCORE_SW_UPDATE_STATE_INITIAL;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", SW_UPDATE_STATE_PATH, LE_RESULT_TXT(result));
        return result;
    }

    *swUpdateStatePtr = updateState;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update result
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetSwUpdateResult
(
    lwm2mcore_SwUpdateResult_t* swUpdateResultPtr   ///< [INOUT] SW update result
)
{
    lwm2mcore_SwUpdateResult_t updateResult;
    size_t size;
    le_result_t result;

    if (!swUpdateResultPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_BAD_PARAMETER;
    }

    size = sizeof(lwm2mcore_SwUpdateResult_t);
    result = ReadFs(SW_UPDATE_RESULT_PATH, (uint8_t *)&updateResult, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_WARN("SW update result not found");
            *swUpdateResultPtr = LWM2MCORE_SW_UPDATE_RESULT_INITIAL;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", SW_UPDATE_RESULT_PATH, LE_RESULT_TXT(result));
        return result;
    }

    *swUpdateResultPtr = updateResult;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Download package thread function
 */
//--------------------------------------------------------------------------------------------------
static void* DownloadThread
(
    void* ctxPtr    ///< Context pointer
)
{
    lwm2mcore_PackageDownloader_t* pkgDwlPtr;
    packageDownloader_DownloadCtx_t* dwlCtxPtr;
    static le_result_t ret;

    // Initialize the return value at every start
    ret = LE_OK;

    // Connect to services used by this thread
    le_secStore_ConnectService();

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t*)ctxPtr;
    dwlCtxPtr = (packageDownloader_DownloadCtx_t*)pkgDwlPtr->ctxPtr;

    // Open the FIFO file descriptor to write downloaded data, blocking
    dwlCtxPtr->downloadFd = open(dwlCtxPtr->fifoPtr, O_WRONLY);

    if (-1 == dwlCtxPtr->downloadFd)
    {
        LE_ERROR("Open FIFO failed: %m");
        ret = LE_IO_ERROR;

        switch (pkgDwlPtr->data.updateType)
        {
            case LWM2MCORE_FW_UPDATE_TYPE:
                packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
                packageDownloader_SetFwUpdateResult(LWM2MCORE_FW_UPDATE_RESULT_COMMUNICATION_ERROR);
                break;

            case LWM2MCORE_SW_UPDATE_TYPE:
                packageDownloader_SetSwUpdateState(LWM2MCORE_SW_UPDATE_STATE_INITIAL);
                packageDownloader_SetSwUpdateResult(LWM2MCORE_SW_UPDATE_RESULT_CONNECTION_LOST);
                break;

            default:
                LE_ERROR("Unknown download type");
                break;
        }

        goto thread_end;
    }

    // Initialize the package downloader, except for a download resume
    if (!dwlCtxPtr->resume)
    {
        lwm2mcore_PackageDownloaderInit();
    }

    // The download can already be aborted if the store thread
    // encountered an error during its initialization.
    // It can also be suspended before being started.
    if (   (DOWNLOAD_STATUS_ABORT != GetDownloadStatus())
        && (DOWNLOAD_STATUS_SUSPEND != GetDownloadStatus()))
    {
        if (DWL_OK != lwm2mcore_PackageDownloaderRun(pkgDwlPtr))
        {
            LE_ERROR("packageDownloadRun failed");
            ret = LE_FAULT;
        }
    }

    // Close the file descriptior as the downloaded data has been written to FIFO
    LE_DEBUG("Close download file descriptor");
    close(dwlCtxPtr->downloadFd);
    dwlCtxPtr->downloadFd = -1;


    // At this point, download has ended. Wait for the end of store thread used for FOTA
    if (LWM2MCORE_FW_UPDATE_TYPE == pkgDwlPtr->data.updateType)
    {
        le_result_t* storeThreadReturnPtr = 0;
        le_thread_Join(StoreFwRef, (void**) &storeThreadReturnPtr);
        StoreFwRef = NULL;
        LE_DEBUG("Store thread joined with return value = %d", *storeThreadReturnPtr);

        if (LE_OK != ret)
        {
            // An error occured during download. A package download failure notification
            // has already been sent by packagedownloader callbacks. No need to check store
            // status as it will also fail and send the notification a second time

            LE_ERROR("Package download failed");
        }
        else
        {
            // No error during download. Check store thread return status.
            if (LE_OK != *storeThreadReturnPtr)
            {
                // An error occured during store, send a notification.
                LE_ERROR("Package store failed");
                ret = LE_FAULT;

                // Status notification is not relevant for suspend.
                if (DOWNLOAD_STATUS_SUSPEND != GetDownloadStatus())
                {
                    // Send download failed event and set the error to "bad package",
                    // as it was rejected by the FW update process.
                    avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED,
                                           LE_AVC_FIRMWARE_UPDATE,
                                           -1,
                                           -1,
                                           LE_AVC_ERR_BAD_PACKAGE,
                                           NULL,
                                           NULL
                                          );
                }
            }
            else
            {
                // Package download is aborted or suspended and no error: the download end
                // is expected in this case, no need to send a notification.
                if ((DOWNLOAD_STATUS_ABORT != GetDownloadStatus())
                    && (DOWNLOAD_STATUS_SUSPEND != GetDownloadStatus()))
                {
                    // Send download complete event.
                    // Not setting the downloaded number of bytes and percentage
                    // allows using the last stored values.
                    avcServer_UpdateStatus(LE_AVC_DOWNLOAD_COMPLETE,
                                           LE_AVC_FIRMWARE_UPDATE,
                                           -1,
                                           -1,
                                           LE_AVC_ERR_NONE,
                                           NULL,
                                           NULL
                                          );
                }
            }
        }
    }

    switch (GetDownloadStatus())
    {
        case DOWNLOAD_STATUS_ACTIVE:
            // Package download is finished: delete stored information
            packageDownloader_DeleteResumeInfo();

            // Trigger a connection to the server: the update state and result will be read
            // to determine if the download was successful
            le_event_QueueFunctionToThread(dwlCtxPtr->mainRef, UpdateStatus, NULL, NULL);
            break;

        case DOWNLOAD_STATUS_SUSPEND:
        {
            uint64_t numBytesToDownload;

            le_fwupdate_ConnectService();

            // Retrieve number of bytes left to download
            if (LE_OK != packageDownloader_BytesLeftToDownload(&numBytesToDownload))
            {
                LE_ERROR("Unable to retrieve bytes left to download");
                numBytesToDownload = 0;
            }

            // Indicate that a download is pending
            avcServer_QueryDownload(packageDownloader_StartDownload,
                                    numBytesToDownload,
                                    pkgDwlPtr->data.updateType,
                                    pkgDwlPtr->data.packageUri,
                                    true,
                                    NULL,
                                    NULL
                                   );

            le_fwupdate_DisconnectService();
        }
        break;

        case DOWNLOAD_STATUS_ABORT:
            if (ret != LE_OK)
            {
                // The abort was caused by an error during the firmware update process:
                // - Delete stored information
                packageDownloader_DeleteResumeInfo();
                // - Trigger a connection to the server to indicate the download failure
                le_event_QueueFunctionToThread(dwlCtxPtr->mainRef, UpdateStatus, NULL, NULL);
            }
            else
            {
                // The abort was requested by the server:
                // - Do not delete the stored information: it was already done when receiving the
                //   abort command from the server and a new download URI might have been sent
                //   by the server in the meantime.
                // - Set update state to the default value
                switch (pkgDwlPtr->data.updateType)
                {
                    case LWM2MCORE_FW_UPDATE_TYPE:
                        packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
                        break;

                    case LWM2MCORE_SW_UPDATE_TYPE:
                        avcApp_SetSwUpdateState(LWM2MCORE_SW_UPDATE_STATE_INITIAL);
                        break;

                    default:
                        LE_ERROR("Unknown download type");
                        break;
                }
            }
            break;

        default:
            LE_ERROR("Unexpected download status %d", GetDownloadStatus());
            break;
    }

thread_end:
    // Reset download status and thread reference
    SetDownloadStatus(DOWNLOAD_STATUS_IDLE);
    DownloadRef = NULL;
    return (void*)&ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * Store FW package thread function
 */
//--------------------------------------------------------------------------------------------------
static void* StoreFwThread
(
    void* ctxPtr    ///< Context pointer
)
{
    lwm2mcore_PackageDownloader_t* pkgDwlPtr;
    packageDownloader_DownloadCtx_t* dwlCtxPtr;
    le_result_t result;
    int fd;
    bool fwupdateInitError = false;
    static le_result_t ret;

    // Initialize the return value at every start
    ret = LE_OK;

    LE_DEBUG("Started storing FW package");

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t*)ctxPtr;
    dwlCtxPtr = pkgDwlPtr->ctxPtr;

    // Connect to services used by this thread
    le_fwupdate_ConnectService();

    // Initialize the FW update process, except for a download resume
    if (!dwlCtxPtr->resume)
    {
        result = le_fwupdate_InitDownload();

        switch (result)
        {
            case LE_OK:
                LE_DEBUG("FW update download initialization successful");
                break;

            case LE_UNSUPPORTED:
                LE_DEBUG("FW update download initialization not supported");
                break;

            default:
                LE_ERROR("Failed to initialize FW update download: %s", LE_RESULT_TXT(result));
                // Indicate that the download should be aborted
                AbortDownload();
                // Set the update state and update result
                packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
                packageDownloader_SetFwUpdateResult(LWM2MCORE_FW_UPDATE_RESULT_COMMUNICATION_ERROR);
                // Do not return, the FIFO should be opened in order to unblock the Download thread
                fwupdateInitError = true;
                break;
        }
    }

    // Open the FIFO file descriptor to read downloaded data, non blocking
    fd = open(dwlCtxPtr->fifoPtr, O_RDONLY | O_NONBLOCK, 0);
    if (-1 == fd)
    {
        LE_ERROR("Failed to open FIFO %m");
        ret = LE_IO_ERROR;
        goto thread_end;
    }

    // There was an error during the FW update initialization, stop here
    if (fwupdateInitError)
    {
        ret = LE_FAULT;
        goto thread_end_close_fd;
    }

    result = le_fwupdate_Download(fd);
    if (LE_OK != result)
    {
        // No further action required if the download is aborted or suspended:
        // the fwupdate error is expected in this case.
        if (   (DOWNLOAD_STATUS_ABORT != GetDownloadStatus())
            && (DOWNLOAD_STATUS_SUSPEND != GetDownloadStatus()))
        {
            lwm2mcore_FwUpdateResult_t fwUpdateResult;

            LE_ERROR("Failed to update firmware: %s", LE_RESULT_TXT(result));
            ret = LE_FAULT;

            // Abort active download
            AbortDownload();

            // Set the update state and update
            packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
            if (LE_CLOSED == result)
            {
                // File descriptor has been closed before all data have been received,
                // this is a communication error
                fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_COMMUNICATION_ERROR;
            }
            else
            {
                // All other error codes are triggered by an incorrect package
                fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_UNSUPPORTED_PKG_TYPE;
            }
            packageDownloader_SetFwUpdateResult(fwUpdateResult);
        }
        else
        {
            LE_DEBUG("Firmware update stopped for download abort/suspend");
        }
    }

thread_end_close_fd:
    close(fd);
thread_end:
    return (void*)&ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * Download and store a package
 *
 */
//--------------------------------------------------------------------------------------------------
void packageDownloader_StartDownload
(
    const char*            uriPtr,  ///< Package URI
    lwm2mcore_UpdateType_t type,    ///< Update type (FW/SW)
    bool                   resume   ///< Indicates if it is a download resume
)
{
    static packageDownloader_DownloadCtx_t dwlCtx;
    lwm2mcore_PackageDownloaderData_t data;
    char* dwlType[2] = {
        [0] = "FW_UPDATE",
        [1] = "SW_UPDATE",
    };

    // Do not start a new download if a previous one is still in progress.
    // A download pending notification will be sent when it is over in order to resume the download.
    if ((DownloadRef) || (StoreFwRef))
    {
        LE_WARN("A download is still in progress, wait for its end");
        return;
    }

    LE_DEBUG("downloading a `%s'", dwlType[type]);

    // Stop activity timer to prevent NO_UPDATE notification
    avcClient_StopActivityTimer();

    // Set the package downloader data structure
    memset(data.packageUri, 0, LWM2MCORE_PACKAGE_URI_MAX_BYTES);
    memcpy(data.packageUri, uriPtr, strlen(uriPtr));
    data.packageSize = 0;
    data.updateType = type;
    data.updateOffset = 0;
    data.isResume = resume;
    PkgDwl.data = data;

    // Set the package downloader callbacks
    PkgDwl.initDownload = pkgDwlCb_InitDownload;
    PkgDwl.getInfo = pkgDwlCb_GetInfo;
    PkgDwl.setFwUpdateState = packageDownloader_SetFwUpdateState;
    PkgDwl.setFwUpdateResult = packageDownloader_SetFwUpdateResult;
    PkgDwl.setSwUpdateState = packageDownloader_SetSwUpdateState;
    PkgDwl.setSwUpdateResult = packageDownloader_SetSwUpdateResult;
    PkgDwl.download = pkgDwlCb_Download;
    PkgDwl.storeRange = pkgDwlCb_StoreRange;
    PkgDwl.endDownload = pkgDwlCb_EndDownload;

    dwlCtx.fifoPtr = FIFO_PATH;
    dwlCtx.mainRef = le_thread_GetCurrent();
    dwlCtx.certPtr = PEMCERT_PATH;
    dwlCtx.downloadPackage = (void*)DownloadThread;
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            if (resume)
            {
                // Get fwupdate offset before launching the download
                // and the blocking call to le_fwupdate_Download()
                le_fwupdate_GetResumePosition((size_t *)&PkgDwl.data.updateOffset);
                LE_DEBUG("updateOffset: %"PRIu64, PkgDwl.data.updateOffset);
            }
            dwlCtx.storePackage = (void*)StoreFwThread;
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            if (resume)
            {
                // Get swupdate offset before launching the download
                avcApp_GetResumePosition((size_t *)&PkgDwl.data.updateOffset);
                LE_DEBUG("updateOffset: %"PRIu64, PkgDwl.data.updateOffset);
            }
            dwlCtx.storePackage = NULL;
            break;

        default:
            LE_ERROR("unknown download type");
            return;
    }
    dwlCtx.resume = resume;
    PkgDwl.ctxPtr = (void*)&dwlCtx;

    // Download starts
    SetDownloadStatus(DOWNLOAD_STATUS_ACTIVE);

    DownloadRef = le_thread_Create("Downloader", (void*)dwlCtx.downloadPackage, (void*)&PkgDwl);
    le_thread_Start(DownloadRef);

    if (LWM2MCORE_SW_UPDATE_TYPE == type)
    {
        // Spawning a new thread won't be a good idea for updateDaemon. For single installation
        // UpdateDaemon requires all its API to be called from same thread. If we spawn thread,
        // both download and installation has to be done from the same thread which will bring
        // unwanted complexity.
        avcApp_StoreSwPackage((void*)&PkgDwl);
        return;
    }

    // Start the Store thread for a FOTA update
    StoreFwRef = le_thread_Create("Store", (void*)dwlCtx.storePackage, (void*)&PkgDwl);
    le_thread_SetJoinable(StoreFwRef);
    le_thread_Start(StoreFwRef);

    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Abort a package download
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_AbortDownload
(
    lwm2mcore_UpdateType_t type     ///< Update type (FW/SW)
)
{
    lwm2mcore_DwlResult_t dwlResult;

    LE_DEBUG("Download abort requested");

    // Abort active download
    AbortDownload();

    // Delete resume information if the files are still present
    packageDownloader_DeleteResumeInfo();

    // Reset stored download agreement as it was only valid for the download which is being aborted
    avcServer_ResetDownloadAgreement();

    // Set update state to the default value
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            dwlResult = packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
            if (DWL_OK != dwlResult)
            {
                return LE_FAULT;
            }
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            dwlResult = packageDownloader_SetSwUpdateState(LWM2MCORE_SW_UPDATE_STATE_INITIAL);
            if (DWL_OK != dwlResult)
            {
                return LE_FAULT;
            }
            break;

        default:
            LE_ERROR("Unknown download type %d", type);
            return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Suspend a package download
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SuspendDownload
(
    void
)
{
    LE_DEBUG("Suspend download, download status was %d", GetDownloadStatus());

    // Suspend ongoing download
    SetDownloadStatus(DOWNLOAD_STATUS_SUSPEND);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the number of bytes to download on resume. Function will give valid data if suspend
 * state was LE_AVC_DOWNLOAD_PENDING, LE_DOWNLOAD_IN_PROGRESS or LE_DOWNLOAD_COMPLETE.
 *
 * @return
 *   - LE_OK                If function succeeded
 *   - LE_BAD_PARAMETER     If parameter null
 *   - LE_FAULT             If function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_BytesLeftToDownload
(
    uint64_t *numBytes          ///< [OUT] Number of bytes to download on resume. Will give valid
                                ///<       data if suspend state was LE_AVC_DOWNLOAD_PENDING,
                                ///<       LE_DOWNLOAD_IN_PROGRESS or LE_DOWNLOAD_COMPLETE.
                                ///<       Otherwise undefined.
)
{
    if (!numBytes)
    {
        LE_ERROR("Invalid input parameter");
        return LE_BAD_PARAMETER;
    }

    lwm2mcore_FwUpdateState_t fwUpdateState = LWM2MCORE_FW_UPDATE_STATE_IDLE;
    lwm2mcore_FwUpdateResult_t fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;

    lwm2mcore_SwUpdateState_t swUpdateState = LWM2MCORE_SW_UPDATE_STATE_INITIAL;
    lwm2mcore_SwUpdateResult_t swUpdateResult = LWM2MCORE_SW_UPDATE_RESULT_INITIAL;

    if (   (LE_OK != packageDownloader_GetFwUpdateState(&fwUpdateState))
        || (LE_OK != packageDownloader_GetFwUpdateResult(&fwUpdateResult))
        || (LE_OK != packageDownloader_GetSwUpdateState(&swUpdateState))
        || (LE_OK != packageDownloader_GetSwUpdateResult(&swUpdateResult)))
    {
        LE_ERROR("Failed to retrieve suspend information");
        return LE_FAULT;
    }

    char downloadUri[LWM2MCORE_PACKAGE_URI_MAX_BYTES];
    size_t uriLen = LWM2MCORE_PACKAGE_URI_MAX_BYTES;
    lwm2mcore_UpdateType_t updateType = LWM2MCORE_MAX_UPDATE_TYPE;
    avcApp_InternalState_t internalState;
    memset(downloadUri, 0, sizeof(downloadUri));

    // Check if an update package URI is stored
    if (LE_OK == packageDownloader_GetResumeInfo(downloadUri, &uriLen, &updateType))
    {
        // Resume info can successfully be retrieved, i.e. there should be some data to download
        uint64_t packageSize = 0;

        if (LE_OK != packageDownloader_GetUpdatePackageSize(&packageSize))
        {
            LE_CRIT("Failed to get package size");
            return LE_FAULT;
        }

        LE_INFO("Package size: %"PRIu64, packageSize);

        // Check whether it is SW or FW update type and update status based on stored status.
        switch (updateType)
        {
            case LWM2MCORE_FW_UPDATE_TYPE:
                LE_DEBUG("FW update type");

                if (LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL == fwUpdateResult)
                {
                    switch(fwUpdateState)
                    {
                        case LWM2MCORE_FW_UPDATE_STATE_IDLE:
                            // No download started yet. Need to download whole package
                            *numBytes = packageSize;
                            return LE_OK;

                        case LWM2MCORE_FW_UPDATE_STATE_DOWNLOADING:
                        {
                            size_t resumePos = 0;

                            if (LE_OK != le_fwupdate_GetResumePosition(&resumePos))
                            {
                                LE_CRIT("Failed to get fwupdate resume position");
                                return LE_FAULT;
                            }

                            LE_DEBUG("FW resume position: %zd", resumePos);

                            *numBytes = packageSize - resumePos;
                            return LE_OK;
                        }
                            break;

                        default:
                            LE_ERROR("Bad FOTA state: %d", fwUpdateState);
                            return LE_FAULT;
                    }
                }
                else
                {
                    LE_ERROR("Bad FW update result: %d", fwUpdateResult);
                    return LE_FAULT;
                }
                break;

            case LWM2MCORE_SW_UPDATE_TYPE:
                LE_DEBUG("SW update type");

                if ((LWM2MCORE_SW_UPDATE_STATE_DOWNLOAD_STARTED == swUpdateState)
                    && (LWM2MCORE_SW_UPDATE_RESULT_INITIAL == swUpdateResult))
                {
                    size_t resumePos = 0;

                    if (LE_OK != avcApp_GetResumePosition(&resumePos))
                    {
                        LE_CRIT("Failed to get swupdate resume position");
                        return LE_FAULT;
                    }

                    LE_DEBUG("SW resume position: %zd", resumePos);


                    *numBytes = packageSize - resumePos;

                    return LE_OK;
                }
                else if ((LWM2MCORE_SW_UPDATE_STATE_INITIAL == swUpdateState)
                    && (LE_OK == avcApp_GetSwUpdateInternalState(&internalState))
                    && (INTERNAL_STATE_DOWNLOAD_REQUESTED == internalState))
                {
                    // No download started yet. Need to download whole package
                    *numBytes = packageSize;
                    return LE_OK;
                }
                else
                {
                    LE_ERROR("Bad SOTA state: %d, result: %d", swUpdateState, swUpdateResult);
                    return LE_FAULT;
                }
                break;

            default:
                LE_CRIT("Incorrect update type");
                return LE_FAULT;
        }
    }
    else if (   (   (swUpdateState == LWM2MCORE_SW_UPDATE_STATE_DOWNLOADED)
                 || (swUpdateState == LWM2MCORE_SW_UPDATE_STATE_DELIVERED))
             && (swUpdateResult == LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED))
    {
        // Check whether any install request was sent from server, if no request sent
        // then reboot happened on LE_AVC_DOWNLOAD_COMPLETE but before LE_AVC_INSTALL_PENDING.
        if ((LE_OK == avcApp_GetSwUpdateInternalState(&internalState)) &&
            (INTERNAL_STATE_INSTALL_REQUESTED != internalState))
        {
            *numBytes = 0;
            return LE_OK;
        }
    }
    else if ((fwUpdateState == LWM2MCORE_FW_UPDATE_STATE_DOWNLOADED) &&
             (fwUpdateResult == LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL))
    {
        *numBytes = 0;
        return LE_OK;
    }
    else
    {
        LE_ERROR("Bad state, swUpdateState: %d, swUpdateResult: %d, fwUpdateState: %d,"
                 "fwUpdateResult: %d",
                  swUpdateState,
                  swUpdateResult,
                  fwUpdateState,
                  fwUpdateResult);
        return LE_FAULT;
    }

    return LE_FAULT;
}
