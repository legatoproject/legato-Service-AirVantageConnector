/**
 * @file packageDownloader.h
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef _PACKAGEDOWNLOADER_H
#define _PACKAGEDOWNLOADER_H

#include <lwm2mcore/update.h>
#include <lwm2mcore/lwm2mcorePackageDownloader.h>
#include <legato.h>
#include <interfaces.h>

typedef void (*storePackageCb)(void *ctxPtr);

//--------------------------------------------------------------------------------------------------
/**
 * Download context data structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    const char*      fifoPtr;               ///< Store FIFO pointer
    int              downloadFd;            ///< Download file descriptor
    int              recvFd;                ///< Reception file descriptor in case of a PIPE
    void*            ctxPtr;                ///< Context pointer
    le_thread_Ref_t  mainRef;               ///< Main thread reference
    const char*      certPtr;               ///< PEM certificate path
    void (*downloadPackage)(void *ctxPtr);  ///< Download package callback
    storePackageCb storePackage;            ///< Store package callback
    bool             resume;                ///< Indicates if it is a download resume
}
packageDownloader_DownloadCtx_t;

//--------------------------------------------------------------------------------------------------
/**
 * Setup temporary files
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_Init
(
    void
);

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
);

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
);

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
);

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
);


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
);

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
);

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
);

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
);

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
    bool* isConnectionNeededPtr                  ///< [OUT] Is connection enabled?
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Download and store a package
 *
 */
//--------------------------------------------------------------------------------------------------
void packageDownloader_StartDownload
(
    lwm2mcore_UpdateType_t type,    ///< Update type (FW/SW)
    bool                   resume   ///< Indicates if it is a download resume
);

//--------------------------------------------------------------------------------------------------
/**
 * Finalize package downloading
 */
//--------------------------------------------------------------------------------------------------
void packageDownloader_FinalizeDownload
(
    le_result_t   downloadStatus    ///< [IN] Package download final status
);

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
    lwm2mcore_UpdateType_t  type    ///< Update type (FW/SW)
);

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
);

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
);

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
);

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
);
#endif /* !LE_CONFIG_CUSTOM_OS */

//--------------------------------------------------------------------------------------------------
/**
 * Delete FW update related info.
 */
//--------------------------------------------------------------------------------------------------
void packageDownloader_DeleteFwUpdateInfo
(
    void
);

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
    size_t* uriSizePtr,                 ///< [INOUT] package URI size
    lwm2mcore_UpdateType_t* typePtr     ///< [INOUT] Update type
);

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
);

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
);
#endif /* !LE_CONFIG_CUSTOM_OS */

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the number of bytes to download on resume. Function will give valid data if suspend
 * state was LE_AVC_DOWNLOAD_PENDING or LE_DOWNLOAD_COMPLETE.
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
                                ///<       data if suspend state was LE_AVC_DOWNLOAD_PENDING or
                                ///<       LE_DOWNLOAD_COMPLETE. Otherwise undefined.
);

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
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 *  Request a download retry
 */
//--------------------------------------------------------------------------------------------------
void downloader_RequestDownloadRetry
(
    void* param1Ptr,     ///< [IN] Not used, should be NULL
    void* param2Ptr      ///< [IN] Not used, should be NULL
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the downloader module
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
void downloader_Init
(
    void
);

#endif /*_PACKAGEDOWNLOADER_H */
