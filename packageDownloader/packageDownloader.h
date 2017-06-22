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
#include <lwm2mcorePackageDownloader.h>
#include <legato.h>

//--------------------------------------------------------------------------------------------------
/**
 * Download context data structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    const char*      fifoPtr;               ///< Store FIFO pointer
    int              downloadFd;            ///< Download FIFO file descriptor
    void*            ctxPtr;                ///< Context pointer
    le_thread_Ref_t  mainRef;               ///< Main thread reference
    le_thread_Ref_t  storeRef;              ///< Store thread reference
    const char*      certPtr;               ///< PEM certificate path
    void (*downloadPackage)(void *ctxPtr);  ///< Download package callback
    void (*storePackage)(void *ctxPtr);     ///< Store package callback
    bool             resume;                ///< Indicates if it is a download resume
    le_sem_Ref_t     semRef;                ///< Semaphore synchronizing download and store for FOTA
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
);

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
);

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
);

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
);

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
 * Download package thread function
 */
//--------------------------------------------------------------------------------------------------
void* packageDownloader_DownloadPackage
(
    void* ctxPtr    ///< Context pointer
);

//--------------------------------------------------------------------------------------------------
/**
 * Store FW package thread function
 */
//--------------------------------------------------------------------------------------------------
void* packageDownloader_StoreFwPackage
(
    void* ctxPtr    ///< Context pointer
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Abort a package download
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_AbortDownload
(
    lwm2mcore_UpdateType_t  type    ///< Update type (FW/SW)
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if the current download should be aborted
 */
//--------------------------------------------------------------------------------------------------
bool packageDownloader_CurrentDownloadToAbort
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if the current download should be suspended
 */
//--------------------------------------------------------------------------------------------------
bool packageDownloader_CheckDownloadToSuspend
(
    void
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

#endif /*_PACKAGEDOWNLOADER_H */
