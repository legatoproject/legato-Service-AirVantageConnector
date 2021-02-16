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
#include <lwm2mcore/lwm2mcorePackageDownloader.h>
#include "downloader.h"
#include <lwm2mcore/update.h>
#include <lwm2mcore/security.h>
#include "packageDownloader.h"
#include "avcAppUpdate/avcAppUpdate.h"
#include "avcFs/avcFs.h"
#include "avcFs/avcFsConfig.h"
#include "avcClient/avcClient.h"
#include "avcServer/avcServer.h"

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
 * Download thread stack size in words
 */
//--------------------------------------------------------------------------------------------------
#define STR_THR_STACK_SIZE (5*1024)

//--------------------------------------------------------------------------------------------------
/**
 * Allocate stacks for both threads
 */
//--------------------------------------------------------------------------------------------------
LE_THREAD_DEFINE_STATIC_STACK(ThreadStrStack, STR_THR_STACK_SIZE);

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
 * Static value for error code in case of package download suspend due to netowrk, memory issue.
 */
//--------------------------------------------------------------------------------------------------
static le_avc_ErrorCode_t ErrorCode = LE_AVC_ERR_NONE;

//--------------------------------------------------------------------------------------------------
/**
 * Static value for FW update result in case of error (the store thread is then closed before
 * main thread calls the join)
 */
//--------------------------------------------------------------------------------------------------
static le_result_t FwUpdateResult = LE_FAULT;

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
    uint32_t            fwUpdateErrorCode;  ///< FW update error code
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
    // Check if the device is connected
    if (LE_UNAVAILABLE == avcClient_Update())
    {
        lwm2mcore_PackageDownloader_t* pkgDwlPtr = &PkgDwl;
        LE_WARN("Not possible to check the route -> make a connection, updateType %d",
                pkgDwlPtr->data.updateType);
        if (LWM2MCORE_FW_UPDATE_TYPE == pkgDwlPtr->data.updateType)
        {
            avcServer_QueryConnection(LE_AVC_FIRMWARE_UPDATE, NULL, NULL);
        }
        else if ((LWM2MCORE_SW_UPDATE_TYPE == pkgDwlPtr->data.updateType))
        {
            avcServer_QueryConnection(LE_AVC_APPLICATION_UPDATE, NULL, NULL);
        }
        else
        {
            LE_ERROR("Incorrect update type %d", pkgDwlPtr->data.updateType);
        }

    }
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
    DownloadStatus = newDownloadStatus;
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
    uint8_t currentDownloadStatus = DownloadStatus;
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
 * Function to send a download pending request
 */
//--------------------------------------------------------------------------------------------------
static void ResumeDownloadRequest
(
    void* param1,   /// [IN] 1st parameter set in le_event_QueueFunction
    void* param2    /// [IN] 2nd parameter set in le_event_QueueFunction
)
{
    uint64_t numBytesToDownload = 0;
    (void)param1;
    (void)param2;

    // Indicate that a download is pending
    if (LE_OK == packageDownloader_BytesLeftToDownload(&numBytesToDownload))
    {
        avcServer_QueryDownload(packageDownloader_StartDownload,
                                numBytesToDownload,
                                PkgDwl.data.updateType,
                                true,
                                ErrorCode);
    }
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

#ifndef LE_CONFIG_CUSTOM_OS
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

#endif /* !LE_CONFIG_CUSTOM_OS */

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
#ifndef LE_CONFIG_CUSTOM_OS
    DeleteFs(FW_UPDATE_STATE_PATH);
    DeleteFs(FW_UPDATE_RESULT_PATH);
#endif /* !LE_CONFIG_CUSTOM_OS */
    DeleteFs(FW_UPDATE_NOTIFICATION_PATH);
    DeleteFs(FW_UPDATE_INSTALL_PENDING_PATH);
}

#ifndef LE_CONFIG_CUSTOM_OS
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
#endif /* !LE_CONFIG_CUSTOM_OS */

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
        if (LE_FAULT == le_dir_MakePath(PKGDWL_TMP_PATH, S_IRWXU))
        {
            LE_ERROR("failed to create pkgdwl directory %d", errno);
            return LE_FAULT;
        }
    }

    if ( (-1 == le_fd_MkFifo(FIFO_PATH, S_IRUSR | S_IWUSR)) && (EEXIST != errno) )
    {
        LE_ERROR("failed to create fifo: %d", errno);
        return LE_FAULT;
    }

    return LE_OK;
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

#ifndef LE_CONFIG_CUSTOM_OS
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
#endif /* !LE_CONFIG_CUSTOM_OS */

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
    bool                        notifRequested, ///< [IN] Indicates if a notification is requested
    le_avc_Status_t             updateStatus,   ///< [IN] Update status
    le_avc_ErrorCode_t          errorCode,      ///< [IN] Error code
    le_fwupdate_UpdateStatus_t  fwErrorCode     ///< [IN] FW update error code
)
{
    FwUpdateNotif_t notification;
    notification.notifRequested = notifRequested;
    notification.updateStatus = updateStatus;
    notification.errorCode = errorCode;
    notification.fwUpdateErrorCode = (uint32_t)fwErrorCode;

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
    bool*                       notifRequestedPtr,  ///< [OUT] Is a notification requested?
    le_avc_Status_t*            updateStatusPtr,    ///< [OUT] Update status
    le_avc_ErrorCode_t*         errorCodePtr,       ///< [OUT] Error code
    le_fwupdate_UpdateStatus_t* fwErrorCodePtr      ///< [IN] FW update error code
)
{
    le_result_t result;
    FwUpdateNotif_t notification;
    size_t size = sizeof(FwUpdateNotif_t);

    if ((!notifRequestedPtr) || (!updateStatusPtr) || (!errorCodePtr) || (!fwErrorCodePtr))
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
    // The aim of this check is to avoid a reboot in loop if a local update is done from an old
    // soft to new one which include this modification.
    if (sizeof(FwUpdateNotif_t) != size)
    {
        // Delete the old file
        result = DeleteFs(FW_UPDATE_NOTIFICATION_PATH);
        if (LE_OK != result)
        {
            LE_ERROR("Failed to delete %s: %s", FW_UPDATE_NOTIFICATION_PATH, LE_RESULT_TXT(result));
            return LE_FAULT;
        }
        *notifRequestedPtr = false;
        *updateStatusPtr = LE_AVC_NO_UPDATE;
        *errorCodePtr = LE_AVC_ERR_NONE;
        *fwErrorCodePtr = LE_FWUPDATE_UPDATE_STATUS_OK;
    }
    else
    {
        *notifRequestedPtr = notification.notifRequested;
        *updateStatusPtr = notification.updateStatus;
        *errorCodePtr = notification.errorCode;
        *fwErrorCodePtr = (le_fwupdate_UpdateStatus_t)notification.fwUpdateErrorCode;
    }

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
 * Set connection notification status
 * The aim of this function is to query a connection in the boot if the server was not notified by
 * the download complete.
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetConnectionNotificationState
(
    bool isConnectionNeeded                     ///< [IN] Is connection enabled ?

)
{
    le_result_t result;
    LE_DEBUG("Connection notification state set %d", isConnectionNeeded);

    result = WriteFs(CONNECTION_PENDING_PATH, (uint8_t*)&isConnectionNeeded, sizeof(bool));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", CONNECTION_PENDING_PATH, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get connection notification status
 * The aim of this function is to check at the boot if a connection is need to notify the server
 * that a download is complete and ready to be installed.
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetConnectionNotificationState
(
    bool* isConnectionNeededPtr                  ///< [OUT] Is connection enabled ?
)
{
    size_t size;
    bool isConnectionPending;
    le_result_t result;

    if (!isConnectionNeededPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_BAD_PARAMETER;
    }

    size = sizeof(bool);
    result = ReadFs(CONNECTION_PENDING_PATH, (uint8_t*)&isConnectionPending, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_WARN("Connection pending not found");
            *isConnectionNeededPtr = false;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", CONNECTION_PENDING_PATH, LE_RESULT_TXT(result));
        return result;
    }
    LE_DEBUG("Connection pending %d", isConnectionPending);

    *isConnectionNeededPtr = isConnectionPending;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Request package downloading
 */
//--------------------------------------------------------------------------------------------------
static le_result_t RequestDownload
(
    packageDownloader_DownloadCtx_t* dwlCtxPtr    ///< [IN] Download context pointer
)
{
    lwm2mcore_Sid_t downloaderResult;

    // Initialize file descriptors
    dwlCtxPtr->downloadFd = -1;
    dwlCtxPtr->recvFd = -1;

    if (dwlCtxPtr->fifoPtr)
    {
        LE_INFO("Create a FIFO");
        dwlCtxPtr->downloadFd = le_fd_Open(dwlCtxPtr->fifoPtr, O_WRONLY);
    }
#if LE_CONFIG_SOTA
    else
    {
        int fd[2] = {0};

        LE_INFO("Create a PIPE");
        // For SOTA jobs, download and storage is done in the same thread. FIFO requires two threads
        // to operate. This is why we create a PIPE here instead of FIFO.
        if (!pipe(fd))
        {
            dwlCtxPtr->recvFd = fd[0];
            dwlCtxPtr->downloadFd = fd[1];
        }
    }
#endif

    if (-1 == dwlCtxPtr->downloadFd)
    {
        lwm2mcore_PackageDownloader_t* pkgDwlPtr = &PkgDwl;

        lwm2mcore_SetDownloadError(LWM2MCORE_UPDATE_ERROR_DEVICE_SPECIFIC);

        LE_ERROR("Open FIFO failed: %d", errno);

        // Trigger a connection to the server: the update state and result will be read
        // to determine if the download was successful
        le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                       UpdateStatus,
                                       &(pkgDwlPtr->data.updateType),
                                       NULL);
        return LE_IO_ERROR;
    }

    downloaderResult = lwm2mcore_StartPackageDownloader(dwlCtxPtr);
    if (LWM2MCORE_ERR_COMPLETED_OK != downloaderResult)
    {
        LE_ERROR("Package download failed downloaderResult %d", downloaderResult);

        if (LWM2MCORE_ERR_RETRY_FAILED == downloaderResult)
        {
            downloader_RequestDownloadRetry(NULL, NULL);
            return LE_OK;
        }


        if ((LWM2MCORE_ERR_NET_ERROR != downloaderResult)
         && (LWM2MCORE_ERR_MEMORY != downloaderResult))
        {
            avcClient_Update();
        }

        // Consider download errors in which the download is suspended
        // (and not considered as failed)
        if (downloader_CheckDownloadToSuspend())
        {
            le_result_t result = LE_OK;
            switch(downloaderResult)
            {
                case LWM2MCORE_ERR_NET_ERROR:
                    result = LE_COMM_ERROR;
                    break;

                case LWM2MCORE_ERR_MEMORY:
                    result = LE_NO_MEMORY;
                    break;

                default:
                    break;
            }

            if (LE_OK != result)
            {
                // Finalize the download:
                // - Close the file descriptor as the downloaded data has been written to FIFO
                // - Send notifications
                packageDownloader_FinalizeDownload(result);
            }
        }
        else
        {
            le_fd_Close(dwlCtxPtr->downloadFd);
        }

        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Finalize package downloading
 */
//--------------------------------------------------------------------------------------------------
void packageDownloader_FinalizeDownload
(
    le_result_t   downloadStatus   ///< [IN] Package download final status
)
{
    lwm2mcore_PackageDownloader_t* pkgDwlPtr = &PkgDwl;
    packageDownloader_DownloadCtx_t* dwlCtxPtr = pkgDwlPtr->ctxPtr;
    le_result_t ret = downloadStatus;

    LE_INFO("End downloader (status: %d): Stop FD", ret);

    // Close the file descriptior as the downloaded data has been written to FIFO
    // Firstly check if the fd is valid
    if (-1 == dwlCtxPtr->downloadFd)
    {
        LE_DEBUG("Download fd is already closed");
        // Check if the download was already finalized
        // This could happen if the device deregisters from the network while last package bytes are
        // received.
        // AVC thread suspends the download and finalizes it, then download thread finalizes the
        // download
        if(GetDownloadStatus() == DOWNLOAD_STATUS_IDLE)
        {
            LE_DEBUG("Download is already finalized");
            return;
        }
    }
    else if(le_fd_Close(dwlCtxPtr->downloadFd) == -1)
    {
        LE_WARN("Failed to close download fd");
    }
    dwlCtxPtr->downloadFd = -1;

    // At this point, download has ended. Wait for the end of store thread used for FOTA
    if (LWM2MCORE_FW_UPDATE_TYPE == pkgDwlPtr->data.updateType)
    {
        le_result_t* storeThreadReturnPtr = 0;

        if (StoreFwRef)
        {
            le_thread_Join(StoreFwRef, (void**) &storeThreadReturnPtr);
            StoreFwRef = NULL;
            LE_DEBUG("Store thread joined with return value = %d", *storeThreadReturnPtr);
        }
        else
        {
            storeThreadReturnPtr = &FwUpdateResult;
            LE_DEBUG("Store thread with return value = %d", *storeThreadReturnPtr);
        }


        // Check is an issue happened on download start
        // In this case, LwM2MCore already sends a notification to AVC
        if (LE_UNAVAILABLE == ret)
        {
            le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                           UpdateStatus,
                                           &(pkgDwlPtr->data.updateType),
                                           NULL);
           goto end;
        }

        // Check the download result
        if (LE_OK != ret)
        {
            bool isRegUpdateToBeSent = false;
            // Download failure
            if(LE_OK != *storeThreadReturnPtr)
            {
                if (LE_NO_MEMORY == *storeThreadReturnPtr)
                {
                    ErrorCode = LE_AVC_ERR_RAM;
                    le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                                   ResumeDownloadRequest,
                                                   NULL,
                                                   NULL);
                }
                else if (LE_CLOSED == *storeThreadReturnPtr)
                {
                    avcServer_UpdateStatus(LE_AVC_DOWNLOAD_TIMEOUT,
                                           LE_AVC_FIRMWARE_UPDATE,
                                           -1,
                                           -1,
                                           LE_AVC_ERR_INTERNAL);
                }
                else
                {
                    avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED,
                                           LE_AVC_FIRMWARE_UPDATE,
                                           -1,
                                           -1,
                                           LE_AVC_ERR_INTERNAL);
                    lwm2mcore_SetDownloadError(LWM2MCORE_UPDATE_ERROR_UNSUPPORTED_PACKAGE);
                    isRegUpdateToBeSent = true;
                }
            }
            else
            {
                le_avc_ErrorCode_t errorCode = LE_AVC_ERR_INTERNAL;
                switch (ret)
                {
                    case LE_COMM_ERROR:
                    case LE_TERMINATED:
                    case LE_FAULT:
                        errorCode = LE_AVC_ERR_NETWORK;
                        break;

                    case LE_TIMEOUT:
                        errorCode = LE_AVC_ERR_NETWORK;
                        break;

                    case LE_NO_MEMORY:
                        errorCode = LE_AVC_ERR_RAM;
                        break;

                    case LE_FORMAT_ERROR:
                        errorCode = LE_AVC_ERR_NONE;
                        break;

                    default:
                        errorCode = LE_AVC_ERR_INTERNAL;
                        break;
                }
                LE_ERROR("errorCode %d", errorCode);

                // In case of LE_AVC_ERR_NONE, the notification is sent by LwM2MCore:
                // LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FAILED
                if (LE_AVC_ERR_NONE != errorCode)
                {
                    avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED,
                                           LE_AVC_FIRMWARE_UPDATE,
                                           -1,
                                           -1,
                                           errorCode);
                }
            }
            // Trigger a connection to the server: the update state and result will be read
            // to determine if the download was successful
            if (isRegUpdateToBeSent)
            {
                le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                               UpdateStatus,
                                               &(pkgDwlPtr->data.updateType),
                                               NULL);
            }
        }
        else
        {
            uint16_t errorCode = 0;
            lwm2mcore_GetLastHttpErrorCode(&errorCode);

            if (HTTP_404 == errorCode)
            {
                // In this case, no data were sent to FW update
                // Trigger a connection to the server: the update state and result will be read
                // to determine if the download was successful
                le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                               UpdateStatus,
                                               &(pkgDwlPtr->data.updateType),
                                               NULL);
                goto end;
            }

            switch (*storeThreadReturnPtr)
            {
                case LE_OUT_OF_RANGE:
                    avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED,
                                           LE_AVC_FIRMWARE_UPDATE,
                                           -1,
                                           -1,
                                           LE_AVC_ERR_PKG_TOO_BIG);
                    lwm2mcore_SetDownloadError(LWM2MCORE_UPDATE_ERROR_NO_STORAGE_SPACE);
                    le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                                   UpdateStatus,
                                                   &(pkgDwlPtr->data.updateType),
                                                   NULL);
                    break;

                case LE_NO_MEMORY:
                case LE_CLOSED:
                {
                    uint64_t numBytesToDownload;
                    LE_DEBUG("Download suspend, store return %d", *storeThreadReturnPtr);

                    // Retrieve number of bytes left to download
                    if (LE_OK != packageDownloader_BytesLeftToDownload(&numBytesToDownload))
                    {
                        LE_ERROR("Unable to retrieve bytes left to download");
                        numBytesToDownload = 0;
                        goto end;
                    }

                    if (!numBytesToDownload)
                    {
                        // The whole package was downloaded but FW update suspend
                        // Indicate that the download fails
                        // This is needed because for the downloader, the download succeeds
                        // but the download on FW update side failed
                        avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED,
                                               LE_AVC_FIRMWARE_UPDATE,
                                               -1,
                                               -1,
                                               LE_AVC_ERR_BAD_PACKAGE);
                        lwm2mcore_SetDownloadError(LWM2MCORE_UPDATE_ERROR_UNSUPPORTED_PACKAGE);

                        // Trigger a connection to the server: the update state and result will be
                        // read to determine if the download was successful
                        le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                                       UpdateStatus,
                                                       &(pkgDwlPtr->data.updateType),
                                                       NULL);
                    }
                    else
                    {
                        le_avc_ErrorCode_t errorCode = LE_AVC_ERR_NONE;
                        if (LE_NO_MEMORY == *storeThreadReturnPtr)
                        {
                            errorCode = LE_AVC_ERR_RAM;
                        }

                        if (LE_OK != downloadStatus)
                        {
                            switch (downloadStatus)
                            {
                                case LE_COMM_ERROR:
                                case LE_TERMINATED:
                                case LE_FAULT:
                                case LE_TIMEOUT:
                                case LE_UNAVAILABLE:
                                    errorCode = LE_AVC_ERR_NETWORK;
                                    break;

                                case LE_NO_MEMORY:
                                    errorCode = LE_AVC_ERR_RAM;
                                    break;

                                case LE_BAD_PARAMETER:
                                    errorCode = LE_AVC_ERR_BAD_PACKAGE;
                                    break;

                                default:
                                    break;
                            }
                        }
                        ErrorCode = errorCode;
                        le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                                       ResumeDownloadRequest,
                                                       NULL,
                                                       NULL);
                    }
                }
                break;

                case LE_OK:
                    LE_DEBUG("Download OK");
                    // Check if the downloader returns network or memory issue
                    if (LE_OK != downloadStatus)
                    {
                        //uint64_t numBytesToDownload = 0;
                        le_avc_ErrorCode_t errorCode = LE_AVC_ERR_NONE;

                        switch (downloadStatus)
                        {
                            case LE_COMM_ERROR:
                            case LE_TERMINATED:
                            case LE_FAULT:
                            case LE_TIMEOUT:
                                errorCode = LE_AVC_ERR_NETWORK;
                                break;

                            case LE_NO_MEMORY:
                                errorCode = LE_AVC_ERR_RAM;
                                break;

                            default:
                                break;
                        }
                        ErrorCode = errorCode;
                        le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                                       ResumeDownloadRequest,
                                                       NULL,
                                                       NULL);
                    }
                    else
                    {
                        // Send download complete event.
                        // Not setting the downloaded number of bytes and percentage
                        // allows using the last stored values.
                        avcServer_UpdateStatus(LE_AVC_DOWNLOAD_COMPLETE,
                                               LE_AVC_FIRMWARE_UPDATE,
                                               -1,
                                               -1,
                                               LE_AVC_ERR_NONE);

                        // Trigger a connection to the server: the update state and result will be
                        // read to determine if the download was successful
                        le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                                       UpdateStatus,
                                                       &(pkgDwlPtr->data.updateType),
                                                       NULL);
                    }
                    break;

                default:
                    LE_ERROR("Package download failure");
                    lwm2mcore_SetDownloadError(LWM2MCORE_UPDATE_ERROR_UNSUPPORTED_PACKAGE);
                    // Send download failed event and set the error to "bad package",
                    // as it was rejected by the FW update process.
                    avcServer_UpdateStatus(LE_AVC_DOWNLOAD_FAILED,
                                           LE_AVC_FIRMWARE_UPDATE,
                                           -1,
                                           -1,
                                           LE_AVC_ERR_BAD_PACKAGE);

                    // Trigger a connection to the server: the update state and result will be read
                    // to determine if the download was successful
                    le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                                   UpdateStatus,
                                                   &(pkgDwlPtr->data.updateType),
                                                   NULL);
                    break;
            }
        }
    }
#if LE_CONFIG_SOTA
    else if (LWM2MCORE_SW_UPDATE_TYPE == pkgDwlPtr->data.updateType)
    {
        if (downloader_CheckDownloadToSuspend())
        {
            le_avc_ErrorCode_t errorCode = LE_AVC_ERR_NONE;

            if (LE_OK != downloadStatus)
            {
                switch (downloadStatus)
                {
                    case LE_COMM_ERROR:
                    case LE_TERMINATED:
                    case LE_FAULT:
                    case LE_TIMEOUT:
                        errorCode = LE_AVC_ERR_NETWORK;
                        break;

                    case LE_NO_MEMORY:
                        errorCode = LE_AVC_ERR_RAM;
                        break;

                    default:
                        break;
                }
            }

            ErrorCode = errorCode;
            le_event_QueueFunctionToThread(dwlCtxPtr->mainRef,
                                           ResumeDownloadRequest,
                                           NULL,
                                           NULL);
        }
    }
#endif /* LE_CONFIG_SOTA */

end:
    SetDownloadStatus(DOWNLOAD_STATUS_IDLE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Store FW package thread function
 */
//--------------------------------------------------------------------------------------------------
static void* StoreFwThread
(
    void* ctxPtr    ///< [IN] Context pointer
)
{
    lwm2mcore_PackageDownloader_t* pkgDwlPtr;
    packageDownloader_DownloadCtx_t* dwlCtxPtr;
    le_result_t result;
    int fd = -1;
    bool fwupdateInitError = false;
    static le_result_t ret;

    // Initialize the return value at every start
    ret = LE_OK;
    FwUpdateResult = LE_OK;

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t*)ctxPtr;
    dwlCtxPtr = pkgDwlPtr->ctxPtr;

    LE_DEBUG("Started storing FW package, resume %d", dwlCtxPtr->resume);

    // Connect to services used by this thread
    le_fwupdate_ConnectService();

    // Open the FIFO file descriptor to read downloaded data, non blocking
    fd = le_fd_Open(dwlCtxPtr->fifoPtr, O_RDONLY | O_NONBLOCK);
    if (-1 == fd)
    {
        LE_ERROR("Failed to open FIFO %d", errno);
        ret = LE_IO_ERROR;
        goto thread_end;
    }

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

            case LE_NO_MEMORY:
                LE_ERROR("FW update download initialization: memory allocation issue");
                lwm2mcore_SuspendDownload();
                // Do not return, the FIFO should be opened in order to unblock the Download thread
                fwupdateInitError = true;
                break;

            default:
                LE_ERROR("Failed to initialize FW update download: %s", LE_RESULT_TXT(result));
                // Indicate that the download should be aborted
                lwm2mcore_AbortDownload();

                // Do not return, the FIFO should be opened in order to unblock the Download thread
                fwupdateInitError = true;
                break;
        }
    }

    // There was an error during the FW update initialization, stop here
    if (fwupdateInitError)
    {
        if (LE_NO_MEMORY == result)
        {
            ret = result;
        }
        else
        {
            ret = LE_FAULT;
        }
        goto thread_end_close_fd;
    }

    result = le_fwupdate_Download(fd);
    LE_DEBUG("le_fwupdate_Download returns %d", result);
    ret = result;

    // fd has been passed to le_fwupdate_Download(), so do not close in this thread.
    fd = -1;

    if (LE_OK != result)
    {
        lwm2mcore_SuspendDownload();
    }

thread_end_close_fd:
    FwUpdateResult = result;
    // Only close fd if it is open in this thread.
    if (fd != -1)
    {
        if (le_fd_Close(fd) == -1)
        {
            LE_WARN("Failed to close fifo FD");
        }
    }
thread_end:
    StoreFwRef = NULL;
    le_fwupdate_DisconnectService();
    return (void*)&ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start package downloading and storing process.
 *
 */
//--------------------------------------------------------------------------------------------------
void packageDownloader_StartDownload
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type (FW/SW)
    bool                   resume   ///< [IN] Indicates if it is a download resume
)
{
    static packageDownloader_DownloadCtx_t dwlCtx;
    size_t offset = 0;

    // Do not start a new download if a previous one is still in progress.
    // A download pending notification will be sent when it is over in order to resume the download.
    if ((GetDownloadStatus() != DOWNLOAD_STATUS_IDLE) || (StoreFwRef))
    {
        LE_ERROR("A download is still in progress, wait for its end");
        return;
    }

    // Stop activity timer to prevent NO_UPDATE notification
    avcClient_StopActivityTimer();

    // Fill package downloader structure
    dwlCtx.mainRef = le_thread_GetCurrent();
    dwlCtx.certPtr = PEMCERT_PATH;
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            if (resume)
            {
                // Get fwupdate offset before launching the download and the blocking call
                // to le_fwupdate_Download()
                if (LE_OK != le_fwupdate_GetResumePosition(&offset))
                {
                    offset = 0;
                    resume = false;
                }
            }
            dwlCtx.storePackage = (storePackageCb)StoreFwThread;
            dwlCtx.fifoPtr = FIFO_PATH;
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            if (resume)
            {
                // Get swupdate offset before launching the download
                if (LE_OK != avcApp_GetResumePosition(&offset))
                {
                    offset = 0;
                    resume = false;
                }
            }
            dwlCtx.storePackage = NULL;
            dwlCtx.fifoPtr = NULL;
            break;

        default:
            LE_ERROR("Unknown download type");
            return;
    }

    PkgDwl.data.updateOffset = (uint64_t)offset;
    PkgDwl.data.isResume = resume;
    PkgDwl.data.updateType = type;
    PkgDwl.ctxPtr = (void*)&dwlCtx;
    dwlCtx.resume = resume;

    // Download starts
    SetDownloadStatus(DOWNLOAD_STATUS_ACTIVE);
    LE_INFO("Download type: %d, resume:%d, offset:%zd", type, resume, offset);

    if (LWM2MCORE_FW_UPDATE_TYPE == type)
    {
        // Start the Store thread for a FOTA update
        StoreFwRef = le_thread_Create("Store", (le_thread_MainFunc_t)dwlCtx.storePackage, (void*)&PkgDwl);
        le_thread_SetJoinable(StoreFwRef);
        LE_THREAD_SET_STATIC_STACK(StoreFwRef, ThreadStrStack);
        le_thread_Start(StoreFwRef);
    }

    // Request download
    if (LE_OK != RequestDownload(&dwlCtx))
    {
        LE_ERROR("Unable to start package downloader");
        SetDownloadStatus(DOWNLOAD_STATUS_IDLE);
        return;
    }

    if (LWM2MCORE_SW_UPDATE_TYPE == type)
    {
        // Spawning a new thread won't be a good idea for updateDaemon. For single installation
        // UpdateDaemon requires all its API to be called from same thread. If we spawn thread,
        // both download and installation has to be done from the same thread which will bring
        // unwanted complexity.
        avcApp_StoreSwPackage((void*)&PkgDwl);
    }
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

#ifndef LE_CONFIG_CUSTOM_OS
    // Delete resume information if the files are still present
    packageDownloader_DeleteResumeInfo();
#endif /* !LE_CONFIG_CUSTOM_OS */

    // Reset stored download agreement as it was only valid for the download which is being aborted
    avcServer_ResetDownloadAgreement();

    // Set update state to the default value
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            return LE_FAULT;

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
    lwm2mcore_UpdateType_t updateType = LWM2MCORE_MAX_UPDATE_TYPE;
    bool isFwUpdateInstallWaited = false;
    uint64_t packageSize = 0;

    if (!numBytes)
    {
        LE_ERROR("Invalid input parameter");
        return LE_BAD_PARAMETER;
    }

    // Check if a package was fully downloaded for FW update and if the install request was not
    // received from the server
    if (LWM2MCORE_ERR_COMPLETED_OK == lwm2mcore_IsFwUpdateInstallWaited(&isFwUpdateInstallWaited)
     && (isFwUpdateInstallWaited))
    {
        *numBytes = 0;
        return LE_OK;
    }

    // Check if a download can be resumed
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_GetDownloadInfo (&updateType, &packageSize))
    {
        LE_DEBUG("No download to resume");
        return LE_FAULT;
    }

    switch (updateType)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
        {
            size_t resumePos = 0;
            if (LE_OK != le_fwupdate_GetResumePosition(&resumePos))
            {
                LE_CRIT("Failed to get fwupdate resume position");
                resumePos = 0;
            }
            LE_DEBUG("FW resume position: %zd", resumePos);
            *numBytes = packageSize - resumePos;
        }
        break;

        case LWM2MCORE_SW_UPDATE_TYPE:
        {
            size_t resumePos = 0;

            if (LE_OK != avcApp_GetResumePosition(&resumePos))
            {
                LE_CRIT("Failed to get swupdate resume position");
                resumePos = 0;
            }

            LE_DEBUG("SW resume position: %zd", resumePos);
            *numBytes = packageSize - resumePos;

        }
        break;

        default:
            LE_CRIT("Incorrect update type");
            return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Write data
 *
 * This function is called in a dedicated thread/task.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_STATE if no package download is suspended
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_WritePackageData
(
    uint8_t* bufferPtr,     ///< [IN] Data to be written
    uint32_t length,        ///< [IN] Data length
    void*    opaquePtr      ///< [IN] Opaque pointer
)
{
    ssize_t count = -1;
    packageDownloader_DownloadCtx_t* dwlCtxPtr = (packageDownloader_DownloadCtx_t*)opaquePtr;

    if ((!dwlCtxPtr) || (!bufferPtr))
    {
        return LWM2MCORE_ERR_INVALID_STATE;
    }

    count = le_fd_Write(dwlCtxPtr->downloadFd, bufferPtr, length);

    if (-1 == count)
    {
        // Check if the error is not caused by an error in the FW update process,
        // which would have closed the pipe.
        if ((errno == EPIPE) && (true == packageDownloader_CheckDownloadToAbort()))
        {
            LE_WARN("Download aborted by FW update process");
            // No error returned, the package downloader will be stopped
            // through the progress callback.
            return LWM2MCORE_ERR_COMPLETED_OK;
        }
        else
        {
            LE_ERROR("Failed to write to fifo: %d", errno);
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }
    }

    if (length > count)
    {
        LE_ERROR("Failed to write data: size %"PRIu32", count %zd", length, count);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Resume a package download
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @note
 * This function is not available if @c LWM2M_EXTERNAL_DOWNLOADER compilation flag is embedded
 *
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_ResumePackageDownloader
(
    lwm2mcore_UpdateType_t updateType   ///< [IN] Update type (FW/SW)
)
{
    uint64_t numBytesToDownload = 0;

    LE_DEBUG("lwm2mcore_ResumePackageDownloader type %d", updateType);
    if (LE_OK != packageDownloader_BytesLeftToDownload(&numBytesToDownload))
    {
        LE_ERROR("Unable to retrieve bytes left to download");
        return;
    }

    // Resuming a download: clear all query handler references which might be left by
    // previous SOTA/FOTA jobs interrupted by a session stop.
    avcServer_ResetQueryHandlers();

    // Check if the download active is stopped
    // If the downloader thread is active, the notification will be returned when it's stopped
    if(GetDownloadStatus() == DOWNLOAD_STATUS_IDLE)
    {
        // Request user agreement before proceeding with download
        avcServer_QueryDownload(packageDownloader_StartDownload,
                                numBytesToDownload,
                                updateType,
                                true,
                                LE_AVC_ERR_NONE);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get resume position from FW update
 *
 * @return
 *   - LE_OK                If function succeeded
 *   - LE_FAULT             If function failed
 */
//--------------------------------------------------------------------------------------------------
uint64_t packageDownloader_GetResumePosition
(
    void
)
{
    LE_ERROR("packageDownloader_GetResumePosition PkgDwl.data.updateOffset %" PRIu64,
             PkgDwl.data.updateOffset);
    return PkgDwl.data.updateOffset;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if the downloader thread is running
 *
 * @return
 *   - LE_OK                If function succeeded
 *   - LE_BAD_PARAMETER     Null pointer provided
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_IsDownloadInProgress
(
    bool*    isDownloadPtr   ///< [INOUT] Download thread state (true = running, false = stopped)
)
{
    if( NULL == isDownloadPtr)
    {
        return LE_BAD_PARAMETER;
    }

    if ((GetDownloadStatus() != DOWNLOAD_STATUS_IDLE))
    {
        *isDownloadPtr = true;
    }
    else
    {
        *isDownloadPtr = false;
    }
    return LE_OK;
}
