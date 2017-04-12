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
 * Check system state
 */
//--------------------------------------------------------------------------------------------------
static le_result_t CheckSystemState
(
    void
)
{
    le_result_t result;
    bool isSync;

    result = le_fwupdate_DualSysSyncState(&isSync);
    if (result != LE_OK)
    {
        LE_ERROR("Sync State check failed. Error %s", LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    if (false == isSync)
    {
        result = le_fwupdate_DualSysSync();
        if (result != LE_OK)
        {
            LE_ERROR("SYNC operation failed. Error %s", LE_RESULT_TXT(result));
            return LE_FAULT;
        }
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Update download status
 */
//--------------------------------------------------------------------------------------------------
static void UpdateStatus
(
    void *param1,
    void *param2
)
{
    avcClient_Update();
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
void *packageDownloader_DownloadPackage
(
    void *ctxPtr
)
{
    lwm2mcore_PackageDownloader_t *pkgDwlPtr;
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    static int ret = 0;

    // Connect to services used by this thread
    secStoreGlobal_ConnectService();
    le_fs_ConnectService();

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t *)ctxPtr;
    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)pkgDwlPtr->ctxPtr;

    if (lwm2mcore_PackageDownloaderRun(pkgDwlPtr) != DWL_OK)
    {
        LE_ERROR("packageDownloadRun failed");
        ret = -1;
    }

    le_event_QueueFunctionToThread(dwlCtxPtr->mainRef, UpdateStatus, NULL, NULL);

    return (void *)&ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * Store fw package thread function
 */
//--------------------------------------------------------------------------------------------------
void *packageDownloader_StoreFwPackage
(
    void *ctxPtr
)
{
    lwm2mcore_PackageDownloader_t *pkgDwlPtr;
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    int fd;
    le_result_t result;

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t *)ctxPtr;
    dwlCtxPtr = pkgDwlPtr->ctxPtr;

    le_fwupdate_ConnectService();

    if (LE_OK != CheckSystemState())
    {
        LE_ERROR("failed to sync");
        return (void *)-1;
    }

    fd = open(dwlCtxPtr->fifoPtr, O_RDONLY | O_NONBLOCK, 0);
    if (-1 == fd)
    {
        LE_ERROR("failed to open fifo %m");
        return (void *)-1;
    }

    result = le_fwupdate_Download(fd);
    if (LE_OK != result)
    {
        LE_ERROR("failed to update firmware %s", LE_RESULT_TXT(result));
        close(fd);
        return (void *)-1;
    }

    close(fd);

    return (void *)0;
}

//--------------------------------------------------------------------------------------------------
/**
 * Download and store a package
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_StartDownload
(
    const char              *uriPtr,
    lwm2mcore_UpdateType_t  type
)
{
    static lwm2mcore_PackageDownloader_t pkgDwl;
    static packageDownloader_DownloadCtx_t dwlCtx;
    lwm2mcore_PackageDownloaderData_t data;
    le_thread_Ref_t dwlRef, storeRef;
    char *dwlType[2] = {
        [0] = "FW_UPDATE",
        [1] = "SW_UPDATE",
    };

    LE_DEBUG("downloading a `%s'", dwlType[type]);

    memset(data.packageUri, 0, LWM2MCORE_PACKAGE_URI_MAX_LEN + 1);
    memcpy(data.packageUri, uriPtr, strlen(uriPtr));
    data.packageSize = 0;
    data.updateType = type;

    dwlCtx.fifoPtr = FIFO_PATH;
    dwlCtx.mainRef = le_thread_GetCurrent();
    dwlCtx.certPtr = PEMCERT_PATH;
    dwlCtx.downloadPackage = (void *)packageDownloader_DownloadPackage;

    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            dwlCtx.storePackage = (void *)packageDownloader_StoreFwPackage;
            break;
        case LWM2MCORE_SW_UPDATE_TYPE:
            dwlCtx.storePackage = (void *)avcApp_StoreSwPackage;
            break;
        default:
            LE_ERROR("unknown download type");
            return LE_FAULT;
    }

    pkgDwl.data = data;
    pkgDwl.initDownload = pkgDwlCb_InitDownload;
    pkgDwl.getInfo = pkgDwlCb_GetInfo;
    pkgDwl.setFwUpdateState = pkgDwlCb_SetFwUpdateState;
    pkgDwl.setFwUpdateResult = pkgDwlCb_SetFwUpdateResult;
    pkgDwl.setSwUpdateState = avcApp_SetDownloadState;
    pkgDwl.setSwUpdateResult = avcApp_SetDownloadResult;

    pkgDwl.download = pkgDwlCb_Download;
    pkgDwl.storeRange = pkgDwlCb_StoreRange;
    pkgDwl.endDownload = pkgDwlCb_EndDownload;
    pkgDwl.ctxPtr = (void *)&dwlCtx;

    dwlRef = le_thread_Create("Downloader", (void *)dwlCtx.downloadPackage,
                (void *)&pkgDwl);
    le_thread_Start(dwlRef);


    if (type == LWM2MCORE_SW_UPDATE_TYPE)
    {
        // Spawning a new thread won't be a good idea for updateDaemon. For single installation
        // UpdateDaemon requires all its API to be called from same thread. If we spawn thread,
        // both download and installation has to be done from the same thread which will bring
        // unwanted complexity.
        return avcApp_StoreSwPackage((void *)&pkgDwl);
    }
    else
    {
        storeRef = le_thread_Create("Store", (void *)dwlCtx.storePackage,
                        (void *)&pkgDwl);
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
    lwm2mcore_UpdateType_t  type
)
{
    /* The Update State needs to be set to default value
     * The package URI needs to be deleted from storage file
     * Any active download is suspended
     */
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
            break;
        case LWM2MCORE_SW_UPDATE_TYPE:
            avcApp_SetDownloadState(LWM2MCORE_SW_UPDATE_STATE_INITIAL);
            break;
        default:
            LE_ERROR("unknown download type");
            return LE_FAULT;
    }

    return LE_OK;
}
