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
#include <defaultDerKey.h>
#include "packageDownloaderCallbacks.h"
#include "packageDownloader.h"
#include "avcAppUpdate.h"
#include "avcFs.h"
#include "avcFsConfig.h"

//--------------------------------------------------------------------------------------------------
/**
 * Download statuses
 */
//--------------------------------------------------------------------------------------------------
#define DOWNLOAD_STATUS_IDLE        0x00
#define DOWNLOAD_STATUS_ACTIVE      0x01
#define DOWNLOAD_STATUS_ABORT       0x02

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
 * Semaphore to synchronize download abort.
 */
//--------------------------------------------------------------------------------------------------
static le_sem_Ref_t DownloadAbortSemaphore = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Update download status
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
        le_clk_Time_t timeout = {2, 0};
        if (LE_OK != le_sem_WaitWithTimeOut(DownloadAbortSemaphore, timeout))
        {
            LE_ERROR("Error while aborting download");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Write PEM key to default certificate file path
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WritePEMCertificate
(
    const char*     certPtr,
    unsigned char*  pemKeyPtr,
    int             pemKeyLen
)
{
    int fd;
    ssize_t count;
    mode_t mode = 0;

    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    fd = open(certPtr, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (!fd)
    {
        LE_ERROR("failed to open %s: %m", certPtr);
        return LE_FAULT;
    }

    count = write(fd, pemKeyPtr, pemKeyLen);
    if (count == -1)
    {
        LE_ERROR("failed to write PEM cert: %m");
        close(fd);
        return LE_FAULT;
    }
    if (count < pemKeyLen)
    {
        LE_ERROR("failed to write PEM cert: wrote %zd", count);
        close(fd);
        return LE_FAULT;
    }

    close(fd);

    return LE_OK;
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
static le_result_t DeleteResumeInfo
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
    unsigned char pemKeyPtr[MAX_CERT_LEN] = "\0";
    struct stat st;
    uint8_t derKey[MAX_CERT_LEN] = "\0";
    size_t derKeyLen = MAX_CERT_LEN;
    int pemKeyLen = MAX_CERT_LEN;
    le_result_t result;

    if (-1 == stat(PKGDWL_TMP_PATH, &st))
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

    result = ReadFs(DERCERT_PATH, derKey, &derKeyLen);
    if (LE_OK != result)
    {
        LE_ERROR("using default DER key");
        if (MAX_CERT_LEN < DEFAULT_DER_KEY_LEN)
        {
            LE_ERROR("Not enough space to hold the default key");
            return LE_FAULT;
        }
        memcpy(derKey, DefaultDerKey, DEFAULT_DER_KEY_LEN);
        derKeyLen = DEFAULT_DER_KEY_LEN;
    }

    result = lwm2mcore_ConvertDERToPEM((unsigned char *)derKey, derKeyLen,
                                       pemKeyPtr, &pemKeyLen);
    if (LE_OK != result)
    {
        return LE_FAULT;
    }

    result = WritePEMCertificate(PEMCERT_PATH, pemKeyPtr, pemKeyLen);
    if (LE_OK != result)
    {
        return LE_FAULT;
    }

    // Create a semaphore to coordinate download abort
    DownloadAbortSemaphore = le_sem_Create("DownloadAbortSem", 0);

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
                     (uint8_t *)&fwUpdateState,
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
                     (uint8_t *)&fwUpdateResult,
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
    result = ReadFs(FW_UPDATE_STATE_PATH, (uint8_t *)&updateState, &size);
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
    result = ReadFs(FW_UPDATE_RESULT_PATH, (uint8_t *)&updateResult, &size);
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

    *fwUpdateResultPtr = updateResult;

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
    le_fs_ConnectService();

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t*)ctxPtr;
    dwlCtxPtr = (packageDownloader_DownloadCtx_t*)pkgDwlPtr->ctxPtr;

    // Initialize the package downloader, except for a download resume
    if (!dwlCtxPtr->resume)
    {
        lwm2mcore_PackageDownloaderInit();
    }

    SetDownloadStatus(DOWNLOAD_STATUS_ACTIVE);

    if (lwm2mcore_PackageDownloaderRun(pkgDwlPtr) != DWL_OK)
    {
        LE_ERROR("packageDownloadRun failed");
        ret = -1;
    }

    le_sem_Post(DownloadAbortSemaphore);

    // Download finished or aborted: delete stored URI and update type
    if (LE_OK != DeleteResumeInfo())
    {
        ret = -1;
    }

    // If the download was not aborted, trigger a connection to the server:
    // the update state and result will be read to determine if the download was successful
    if (DOWNLOAD_STATUS_ABORT != GetDownloadStatus())
    {
        le_event_QueueFunctionToThread(dwlCtxPtr->mainRef, UpdateStatus, NULL, NULL);
    }

    SetDownloadStatus(DOWNLOAD_STATUS_IDLE);

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
    int fd;
    le_result_t result;

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t*)ctxPtr;
    dwlCtxPtr = pkgDwlPtr->ctxPtr;

    // Connect to services used by this thread
    le_fwupdate_ConnectService();
    le_fs_ConnectService();

    // Initialize the fwupdate process, except for a download resume
    if (!dwlCtxPtr->resume)
    {
        if (LE_OK != le_fwupdate_InitDownload())
        {
            LE_ERROR("Failed to initialize fwupdate");
            return (void *)-1;
        }
    }

    fd = open(dwlCtxPtr->fifoPtr, O_RDONLY | O_NONBLOCK, 0);
    if (-1 == fd)
    {
        LE_ERROR("failed to open fifo %m");
        return (void*)-1;
    }

    result = le_fwupdate_Download(fd);
    if (LE_OK != result)
    {
        LE_ERROR("failed to update firmware %s", LE_RESULT_TXT(result));
        close(fd);

        if (DOWNLOAD_STATUS_ABORT != GetDownloadStatus())
        {
            lwm2mcore_FwUpdateResult_t fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;

            // Abort active download
            AbortDownload();

            // Set the update result if not already done by LWM2M package downloader
            if (   (LE_OK == packageDownloader_GetFwUpdateResult(&fwUpdateResult))
                && (LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL == fwUpdateResult)
               )
            {
                if (LE_OK != packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE))
                {
                    LE_ERROR("Error while setting FW Update State");
                }
                if (LE_OK != packageDownloader_SetFwUpdateResult(
                                                   LWM2MCORE_FW_UPDATE_RESULT_UNSUPPORTED_PKG_TYPE))
                {
                    LE_ERROR("Error while setting FW Update Result");
                }
            }
        }

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
    static lwm2mcore_PackageDownloader_t pkgDwl;
    static packageDownloader_DownloadCtx_t dwlCtx;
    lwm2mcore_PackageDownloaderData_t data;
    le_thread_Ref_t dwlRef, storeRef;
    char* dwlType[2] = {
        [0] = "FW_UPDATE",
        [1] = "SW_UPDATE",
    };

    LE_DEBUG("downloading a `%s'", dwlType[type]);

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
    pkgDwl.data = data;

    // Set the package downloader callbacks
    pkgDwl.initDownload = pkgDwlCb_InitDownload;
    pkgDwl.getInfo = pkgDwlCb_GetInfo;
    pkgDwl.setFwUpdateState = packageDownloader_SetFwUpdateState;
    pkgDwl.setFwUpdateResult = packageDownloader_SetFwUpdateResult;
    pkgDwl.setSwUpdateState = avcApp_SetDownloadState;
    pkgDwl.setSwUpdateResult = avcApp_SetDownloadResult;
    pkgDwl.download = pkgDwlCb_Download;
    pkgDwl.storeRange = pkgDwlCb_StoreRange;
    pkgDwl.endDownload = pkgDwlCb_EndDownload;

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
                le_fwupdate_GetResumePosition(&pkgDwl.data.updateOffset);
                LE_DEBUG("updateOffset: %llu", pkgDwl.data.updateOffset);
            }
            dwlCtx.storePackage = (void*)packageDownloader_StoreFwPackage;
            break;
        case LWM2MCORE_SW_UPDATE_TYPE:
            dwlCtx.storePackage = (void*)avcApp_StoreSwPackage;
            break;
        default:
            LE_ERROR("unknown download type");
            return LE_FAULT;
    }
    dwlCtx.resume = resume;
    pkgDwl.ctxPtr = (void*)&dwlCtx;

    dwlRef = le_thread_Create("Downloader", (void*)dwlCtx.downloadPackage,
                (void*)&pkgDwl);
    le_thread_Start(dwlRef);


    if (type == LWM2MCORE_SW_UPDATE_TYPE)
    {
        // Spawning a new thread won't be a good idea for updateDaemon. For single installation
        // UpdateDaemon requires all its API to be called from same thread. If we spawn thread,
        // both download and installation has to be done from the same thread which will bring
        // unwanted complexity.
        return avcApp_StoreSwPackage((void*)&pkgDwl);
    }
    else
    {
        storeRef = le_thread_Create("Store", (void*)dwlCtx.storePackage,
                        (void*)&pkgDwl);
        le_thread_Start(storeRef);
    }

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
    DeleteResumeInfo();

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
            result = avcApp_SetDownloadState(LWM2MCORE_SW_UPDATE_STATE_INITIAL);
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
