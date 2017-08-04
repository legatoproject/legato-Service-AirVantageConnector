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
static le_thread_Ref_t DownloaderRef = NULL;

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
 * Maximal time to wait for the correct download abort.
 * Set to 15 seconds in order to allow a complete abort even with slow data connection, as at least
 * one data chunk should be downloaded before being able to abort a download.
 */
//--------------------------------------------------------------------------------------------------
#define DOWNLOAD_ABORT_TIMEOUT      15

//--------------------------------------------------------------------------------------------------
/**
 * Semaphore to synchronize download abort.
 */
//--------------------------------------------------------------------------------------------------
static le_sem_Ref_t DownloadAbortSemaphore = NULL;

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
 * Check if the current download should be aborted
 */
//--------------------------------------------------------------------------------------------------
bool packageDownloader_CurrentDownloadToAbort
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
 */
//--------------------------------------------------------------------------------------------------
bool packageDownloader_CheckDownloadToSuspend
(
    void
){
    if (DOWNLOAD_STATUS_SUSPEND == GetDownloadStatus())
    {
        return true;
    }

    return false;
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
    switch (GetDownloadStatus())
    {
        case DOWNLOAD_STATUS_IDLE:
            // Nothing to abort
            break;

        case DOWNLOAD_STATUS_ACTIVE:
            // Abort ongoing download
            SetDownloadStatus(DOWNLOAD_STATUS_ABORT);
            break;

        default:
            LE_ERROR("Unexpected DownloadStatus %d", DownloadStatus);
            SetDownloadStatus(DOWNLOAD_STATUS_IDLE);
            break;
    }

    if (DOWNLOAD_STATUS_IDLE != GetDownloadStatus())
    {
        // Wait for download end
        le_clk_Time_t timeout = {DOWNLOAD_ABORT_TIMEOUT, 0};
        if (LE_OK != le_sem_WaitWithTimeOut(DownloadAbortSemaphore, timeout))
        {
            LE_ERROR("Error while aborting download");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Suspend current download
 */
//--------------------------------------------------------------------------------------------------
static void SuspendDownload
(
    void
)
{
    if (DOWNLOAD_STATUS_IDLE != GetDownloadStatus())
    {
        LE_DEBUG("Wait till download thread exits");

        // Wait till download thread exits
        le_clk_Time_t timeout = {DOWNLOAD_ABORT_TIMEOUT, 0};
        if (LE_OK != le_sem_WaitWithTimeOut(DownloadAbortSemaphore, timeout))
        {
            LE_ERROR("Error while suspending download");
        }
    }
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
static le_result_t SetResumeInfo
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
    size_t* uriLenPtr,                  ///< [INOUT] package URI length
    lwm2mcore_UpdateType_t* typePtr     ///< [INOUT] Update type
)
{
    if (   (!uriPtr) || (!uriLenPtr) || (!typePtr)
        || (*uriLenPtr < (LWM2MCORE_PACKAGE_URI_MAX_LEN + 1))
       )
    {
        return LE_BAD_PARAMETER;
    }

    le_result_t result;
    result = ReadFs(PACKAGE_URI_FILENAME, (uint8_t*)uriPtr, uriLenPtr);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to read %s: %s", PACKAGE_URI_FILENAME, LE_RESULT_TXT(result));
        return result;
    }

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

    // Create a semaphore to coordinate download abort
    DownloadAbortSemaphore = le_sem_Create("DownloadAbortSem", 0);

    // Initialize package downloader
    lwm2mcore_PackageDownloaderGlobalInit();

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set firmware update state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateState
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
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set firmware update result
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateResult
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
        return LE_FAULT;
    }

    return LE_OK;
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
            LE_ERROR("FW update state not found");
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
            LE_ERROR("FW update result not found");
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
            LE_ERROR("FW update install pending not found");
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

    result = WriteFs(PACKAGE_SIZE_FILENAME, (uint64_t*)&size, sizeof(uint64_t));
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

    result = ReadFs(PACKAGE_SIZE_FILENAME, (uint64_t*)&packageSize, &size);
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
 * Set firmware update notification
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateNotification
(
    bool notificationRequest                    ///< [IN] Notification requested
)
{
    le_result_t result = WriteFs(FW_UPDATE_NOTIFICATION_PATH,
                                 (bool*)&notificationRequest,
                                 sizeof(bool));
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
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateNotification
(
    bool* isNotificationRequestPtr              ///< [IN] is a FOTA result needed to be sent to the
                                                ///< server ?
)
{
    le_result_t result;
    bool isNotificationRequest;
    size_t size = sizeof(bool);

    if (!isNotificationRequestPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_FAULT;
    }

    result = ReadFs(FW_UPDATE_NOTIFICATION_PATH, (bool*)&isNotificationRequest, &size);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to read %s: %s", FW_UPDATE_NOTIFICATION_PATH, LE_RESULT_TXT(result));
        return LE_FAULT;
    }
    *isNotificationRequestPtr = isNotificationRequest;

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
            LE_ERROR("SW update state not found");
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
            LE_ERROR("SW update result not found");
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
void* packageDownloader_DownloadPackage
(
    void* ctxPtr    ///< Context pointer
)
{
    lwm2mcore_PackageDownloader_t* pkgDwlPtr;
    packageDownloader_DownloadCtx_t* dwlCtxPtr;
    static int ret = 0;

    // Connect to services used by this thread
    secStoreGlobal_ConnectService();

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t*)ctxPtr;
    dwlCtxPtr = (packageDownloader_DownloadCtx_t*)pkgDwlPtr->ctxPtr;

    // Open the FIFO file descriptor to write downloaded data, blocking
    dwlCtxPtr->downloadFd = open(dwlCtxPtr->fifoPtr, O_WRONLY);

    if (-1 != dwlCtxPtr->downloadFd)
    {
        // Initialize the package downloader, except for a download resume
        if (!dwlCtxPtr->resume)
        {
            lwm2mcore_PackageDownloaderInit();
        }

        // The download can already be aborted if the store thread
        // encountered an error during its initialization.
        if (DOWNLOAD_STATUS_ABORT != GetDownloadStatus())
        {
            // Download will start soon
            SetDownloadStatus(DOWNLOAD_STATUS_ACTIVE);

            if (DWL_OK != lwm2mcore_PackageDownloaderRun(pkgDwlPtr))
            {
                LE_ERROR("packageDownloadRun failed");
                ret = -1;
                // An error occurred, close the file descriptor in order to stop the Store thread
                close(dwlCtxPtr->downloadFd);
                dwlCtxPtr->downloadFd = -1;
            }
        }

        if ((DOWNLOAD_STATUS_ABORT == GetDownloadStatus())
           ||(DOWNLOAD_STATUS_SUSPEND == GetDownloadStatus()))
        {
            le_sem_Post(DownloadAbortSemaphore);

            LE_DEBUG("Close download file descriptor");

            // Signal fwupdate download interruption by closing the file descriptor.
            // This operation will stop the Store thread. Once the Store thread ends we will
            // join the store thread with the main thread.
            if (-1 != dwlCtxPtr->downloadFd)
            {
                close(dwlCtxPtr->downloadFd);
                dwlCtxPtr->downloadFd = -1;
            }
        }

        // If package download is finished or aborted: delete stored URI and update type
        if ((DOWNLOAD_STATUS_SUSPEND != GetDownloadStatus())
         && (LE_OK != packageDownloader_DeleteResumeInfo()))
        {
            ret = -1;
        }

        // Wait for the end of the store thread used for FOTA
        if (LWM2MCORE_FW_UPDATE_TYPE == pkgDwlPtr->data.updateType)
        {
            void* storeRet;
            le_thread_Join(dwlCtxPtr->storeRef, &storeRet);
            ret = storeRet;
            LE_DEBUG("Store thread joined with ret=%d", ret);

            // Delete the semaphore used to synchronize threads
            le_sem_Delete(dwlCtxPtr->semRef);

            // Status notification is not relevant for suspend.
            if (DOWNLOAD_STATUS_SUSPEND != GetDownloadStatus())
            {
                // Check if store thread finished with an error
                if (ret < 0)
                {
                    // Send download failed event and set the error to "bad package",
                    // as it was rejected by the FW update process.
                    avcServer_UpdateHandler(LE_AVC_DOWNLOAD_FAILED,
                                            LE_AVC_FIRMWARE_UPDATE,
                                            -1,
                                            -1,
                                            LE_AVC_ERR_BAD_PACKAGE);
                }
                else
                {
                    // Send download complete event.
                    // Not setting the downloaded number of bytes and percentage
                    // allows using the last stored values.
                    avcServer_UpdateHandler(LE_AVC_DOWNLOAD_COMPLETE,
                                            LE_AVC_FIRMWARE_UPDATE,
                                            -1,
                                            -1,
                                            LE_AVC_ERR_NONE);
                }
            }
        }

        if (DOWNLOAD_STATUS_SUSPEND != GetDownloadStatus())
        {
            // Reset download status if the package download is not suspended
            SetDownloadStatus(DOWNLOAD_STATUS_IDLE);
        }

        LE_DEBUG("Close download file descriptor");

        // Close the file descriptor if necessary.
        if (-1 != dwlCtxPtr->downloadFd)
        {
            close(dwlCtxPtr->downloadFd);
            dwlCtxPtr->downloadFd = -1;
        }
    }
    else
    {
        LE_ERROR("Open FIFO failed: %m");
        ret = -1;

        switch (pkgDwlPtr->data.updateType)
        {
            case LWM2MCORE_FW_UPDATE_TYPE:
                packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
                packageDownloader_SetFwUpdateResult(LWM2MCORE_FW_UPDATE_RESULT_COMMUNICATION_ERROR);
                break;

            case LWM2MCORE_SW_UPDATE_TYPE:
                avcApp_SetSwUpdateState(LWM2MCORE_SW_UPDATE_STATE_INITIAL);
                avcApp_SetSwUpdateResult(LWM2MCORE_SW_UPDATE_RESULT_CONNECTION_LOST);
                break;

            default:
                LE_ERROR("Unknown download type");
                break;
        }
    }

    if (DOWNLOAD_STATUS_SUSPEND != GetDownloadStatus())
    {
        // Trigger a connection to the server: the update state and result will be read to determine
        // if the download was successful
        le_event_QueueFunctionToThread(dwlCtxPtr->mainRef, UpdateStatus, NULL, NULL);
    }

    return (void*)&ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * Store FW package thread function
 */
//--------------------------------------------------------------------------------------------------
void* packageDownloader_StoreFwPackage
(
    void* ctxPtr    ///< Context pointer
)
{
    lwm2mcore_PackageDownloader_t* pkgDwlPtr;
    packageDownloader_DownloadCtx_t* dwlCtxPtr;
    le_result_t result;
    int fd;
    int ret = 0;

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
                SetDownloadStatus(DOWNLOAD_STATUS_ABORT);
                // Set the update state and update result
                packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
                packageDownloader_SetFwUpdateResult(LWM2MCORE_FW_UPDATE_RESULT_COMMUNICATION_ERROR);
                // Do not return, the FIFO should be opened in order to unblock the Download thread
                ret = -1;
                break;
        }
    }

    // Open the FIFO file descriptor to read downloaded data, non blocking
    fd = open(dwlCtxPtr->fifoPtr, O_RDONLY | O_NONBLOCK, 0);
    if (-1 == fd)
    {
        LE_ERROR("Failed to open FIFO %m");
        return (void *)-1;
    }

    // There was an error during the FW update initialization, stop here
    if (-1 == ret)
    {
        close(fd);
        return (void*)-1;
    }

    // Wait for the download beginning before launching the update process:
    // the timeout associated to the FW update should be launched only when the download is really
    // started and not before the user agreed to the download.
    le_sem_Wait(dwlCtxPtr->semRef);

    result = le_fwupdate_Download(fd);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to update firmware: %s", LE_RESULT_TXT(result));

        // No further action required if the download is aborted
        // by writing an empty update package URI
        if ((DOWNLOAD_STATUS_ABORT != GetDownloadStatus())
         && (DOWNLOAD_STATUS_SUSPEND != GetDownloadStatus()))
        {
            lwm2mcore_FwUpdateResult_t fwUpdateResult;

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

        close(fd);
        return (void*)-1;
    }

    close(fd);
    return (void*)0;
}

//--------------------------------------------------------------------------------------------------
/**
 * Download and store a package
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_StartDownload
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

    LE_DEBUG("downloading a `%s'", dwlType[type]);

    avcServer_InitUserAgreement();

    // Stop activity timer to prevent NO_UPDATE notification
    avcClient_StopActivityTimer();

    // Store URI and update type to be able to resume the download if necessary
    if (LE_OK != SetResumeInfo(uriPtr, type))
    {
        return LE_FAULT;
    }

    // Set the package downloader data structure
    memset(data.packageUri, 0, LWM2MCORE_PACKAGE_URI_MAX_LEN + 1);
    memcpy(data.packageUri, uriPtr, strlen(uriPtr));
    data.packageSize = 0;
    data.updateType = type;
    data.updateOffset = 0;
    data.isResume = resume;
    PkgDwl.data = data;

    // Set the package downloader callbacks
    PkgDwl.initDownload = pkgDwlCb_InitDownload;
    PkgDwl.getInfo = pkgDwlCb_GetInfo;
    PkgDwl.userAgreement = pkgDwlCb_UserAgreement;
    PkgDwl.setFwUpdateState = packageDownloader_SetFwUpdateState;
    PkgDwl.setFwUpdateResult = packageDownloader_SetFwUpdateResult;
    PkgDwl.setSwUpdateState = avcApp_SetSwUpdateState;
    PkgDwl.setSwUpdateResult = avcApp_SetSwUpdateResult;
    PkgDwl.download = pkgDwlCb_Download;
    PkgDwl.storeRange = pkgDwlCb_StoreRange;
    PkgDwl.endDownload = pkgDwlCb_EndDownload;

    dwlCtx.fifoPtr = FIFO_PATH;
    dwlCtx.mainRef = le_thread_GetCurrent();
    dwlCtx.certPtr = PEMCERT_PATH;
    dwlCtx.downloadPackage = (void*)packageDownloader_DownloadPackage;
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            if (resume)
            {
                // Get fwupdate offset before launching the download
                // and the blocking call to le_fwupdate_Download()
                le_fwupdate_GetResumePosition(&PkgDwl.data.updateOffset);
                LE_DEBUG("updateOffset: %llu", PkgDwl.data.updateOffset);
            }
            dwlCtx.storePackage = (void*)packageDownloader_StoreFwPackage;
            // Create semaphore to synchronize threads
            dwlCtx.semRef = le_sem_Create("DownloadSemaphore", 0);
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            if (resume)
            {
                // Get swupdate offset before launching the download
                avcApp_GetResumePosition(&PkgDwl.data.updateOffset);
                LE_DEBUG("updateOffset: %llu", PkgDwl.data.updateOffset);
            }
            dwlCtx.storePackage = NULL;
            dwlCtx.semRef = NULL;
            break;

        default:
            LE_ERROR("unknown download type");
            return LE_FAULT;
    }
    dwlCtx.resume = resume;
    PkgDwl.ctxPtr = (void*)&dwlCtx;

    DownloaderRef = le_thread_Create("Downloader", (void*)dwlCtx.downloadPackage, (void*)&PkgDwl);
    le_thread_Start(DownloaderRef);

    if (LWM2MCORE_SW_UPDATE_TYPE == type)
    {
        // Spawning a new thread won't be a good idea for updateDaemon. For single installation
        // UpdateDaemon requires all its API to be called from same thread. If we spawn thread,
        // both download and installation has to be done from the same thread which will bring
        // unwanted complexity.
        return avcApp_StoreSwPackage((void*)&PkgDwl);
    }

    // Start the Store thread for a FOTA update
    dwlCtx.storeRef = le_thread_Create("Store", (void*)dwlCtx.storePackage, (void*)&PkgDwl);
    le_thread_SetJoinable(dwlCtx.storeRef);
    le_thread_Start(dwlCtx.storeRef);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Abort a package download
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_AbortDownload
(
    lwm2mcore_UpdateType_t type     ///< Update type (FW/SW)
)
{
    le_result_t result;

    LE_DEBUG("Download abort requested");

    // Abort active download
    AbortDownload();

    // Delete resume information if the files are still present
    packageDownloader_DeleteResumeInfo();

    // Set update state and result to the default values
    LE_DEBUG("Download aborted");
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            result = packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
            if (LE_OK != result)
            {
                return result;
            }
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            result = avcApp_SetSwUpdateState(LWM2MCORE_SW_UPDATE_STATE_INITIAL);
            if (LE_OK != result)
            {
                return result;
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
    LE_DEBUG("Download status = %d", GetDownloadStatus());

    if (DOWNLOAD_STATUS_ACTIVE == GetDownloadStatus())
    {
        LE_INFO("Suspend download thread");

        // Suspend ongoing download
        SetDownloadStatus(DOWNLOAD_STATUS_SUSPEND);

        // end the state machine
        lwm2mcore_PackageDownloaderSuspend();

        // Wait till we get out of the thread
        SuspendDownload();
    }
    return LE_OK;
}

