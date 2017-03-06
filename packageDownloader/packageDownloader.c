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
#include <osPortUpdate.h>
#include "packageDownloaderCallbacks.h"
#include "packageDownloaderUpdateInfo.h"
#include "packageDownloader.h"

//--------------------------------------------------------------------------------------------------
/**
 * Package download temporary directory
 */
//--------------------------------------------------------------------------------------------------
#define PKGDWL_PATH         "/tmp/pkgdwl"

//--------------------------------------------------------------------------------------------------
/**
 * Fifo file name
 */
//--------------------------------------------------------------------------------------------------
#define FIFO_NAME           "fifo"

//--------------------------------------------------------------------------------------------------
/**
 * Fifo file path
 */
//--------------------------------------------------------------------------------------------------
#define FIFO_PATH           PKGDWL_PATH "/" FIFO_NAME

//--------------------------------------------------------------------------------------------------
/**
 * Package download directory mode
 */
//--------------------------------------------------------------------------------------------------
#define PKGDWL_DIR_MODE     0700

//--------------------------------------------------------------------------------------------------
/**
 * Fifo mode
 */
//--------------------------------------------------------------------------------------------------
#define FIFO_MODE           0600

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
 * Store sw package thread function
 */
//--------------------------------------------------------------------------------------------------
void *packageDownloader_StoreSwPackage
(
    void *ctxPtr
)
{
    lwm2mcore_PackageDownloader_t *pkgDwlPtr;
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    le_result_t result;
    int fd;

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t *)ctxPtr;
    dwlCtxPtr = pkgDwlPtr->ctxPtr;

    fd = open(dwlCtxPtr->fifoPtr, O_RDONLY | O_NONBLOCK, 0);
    if (-1 == fd)
    {
        LE_ERROR("failed to open fifo %m");
        return (void *)-1;
    }

    le_update_ConnectService();

    result = le_update_Start(fd);
    if (LE_OK != result)
    {
        LE_ERROR("failed to update software %s", LE_RESULT_TXT(result));
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
    lwm2mcore_updateType_t  type
)
{
    static lwm2mcore_PackageDownloader_t pkgDwl;
    static packageDownloader_DownloadCtx_t dwlCtx;
    lwm2mcore_PackageDownloaderData_t data;
    le_thread_Ref_t dwlRef;
    le_thread_Ref_t storeRef;
    struct stat st;
    char *dwlType[2] = {
        [0] = "FW_UPDATE",
        [1] = "SW_UPDATE",
    };

    LE_DEBUG("downloading a `%s' from `%s'", dwlType[type], uriPtr);

    memset(data.packageUri, 0, LWM2MCORE_PACKAGE_URI_MAX_LEN + 1);
    memcpy(data.packageUri, uriPtr, strlen(uriPtr));
    data.packageSize = 0;
    data.updateType = type;

    if (-1 == stat(PKGDWL_PATH, &st))
    {
        if (-1 == mkdir(PKGDWL_PATH, PKGDWL_DIR_MODE))
        {
            LE_ERROR("failed to create pkgdwl directory %m");
            return LE_FAULT;
        }
    }

    if ( (-1 == mkfifo(FIFO_PATH, FIFO_MODE)) && (EEXIST != errno) )
    {
        LE_ERROR("failed to create fifo: %m");
        return LE_FAULT;
    }

    dwlCtx.fifoPtr = FIFO_PATH;
    dwlCtx.mainRef = le_thread_GetCurrent();
    dwlCtx.downloadPackage = (void *)packageDownloader_DownloadPackage;

    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            dwlCtx.storePackage = (void *)packageDownloader_StoreFwPackage;
            break;
        case LWM2MCORE_SW_UPDATE_TYPE:
            dwlCtx.storePackage = (void *)packageDownloader_StoreSwPackage;
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
    pkgDwl.setSwUpdateState = pkgDwlCb_SetSwUpdateState;
    pkgDwl.setSwUpdateResult = pkgDwlCb_SetSwUpdateResult;
    pkgDwl.download = pkgDwlCb_Download;
    pkgDwl.storeRange = pkgDwlCb_StoreRange;
    pkgDwl.endDownload = pkgDwlCb_EndDownload;
    pkgDwl.ctxPtr = (void *)&dwlCtx;

    dwlRef = le_thread_Create("Downloader", (void *)dwlCtx.downloadPackage,
                (void *)&pkgDwl);
    le_thread_Start(dwlRef);

    storeRef = le_thread_Create("Store", (void *)dwlCtx.storePackage,
                    (void *)&pkgDwl);
    le_thread_Start(storeRef);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Abort a package download
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_AbortDownload
(
    void
)
{
    le_result_t result;

    // ToDo Abort active download
    // ToDo Delete stored URI and update type
    // Set update state to the default value
    result = packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE);
    if (LE_OK != result)
    {
        return result;
    }

    return LE_OK;
}
