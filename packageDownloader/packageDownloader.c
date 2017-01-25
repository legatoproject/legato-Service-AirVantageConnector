/**
 * @file packageDownloader.c
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <legato.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <interfaces.h>
#include <lwm2mcorePackageDownloader.h>
#include <osPortUpdate.h>
#include "packageDownloaderCallbacks.h"
#include "packageDownloader.h"

#define PKGDWL_PATH "/tmp/pkgdwl"
#define FIFO_NAME   "fifo"
#define FIFO_PATH   PKGDWL_PATH "/" FIFO_NAME
#define STATE_PATH  PKGDWL_PATH "/" "state"
#define RESULT_PATH PKGDWL_PATH "/" "result"

//--------------------------------------------------------------------------------------------------
/**
 * SetUpdateState temporary callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetUpdateStateModified
(
    lwm2mcore_fwUpdateState_t updateState
)
{
    FILE *file;
    char buf[2];

    file = fopen(STATE_PATH, "w");
    if (!file)
    {
        LE_ERROR("failed to open %s: %m", STATE_PATH);
        return DWL_FAULT;
    }

    LE_DEBUG("update state %d", updateState);

    sprintf(buf, "%d", (int)updateState);

    fwrite(buf, strlen(buf), 1, file);

    fclose(file);

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * SetUpdateResult temporary callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetUpdateResultModified
(
    lwm2mcore_fwUpdateResult_t updateResult
)
{
    FILE *file;
    char buf[2];

    file = fopen(RESULT_PATH, "w");
    if (!file)
    {
        LE_ERROR("failed to open %s: %m", RESULT_PATH);
        return DWL_FAULT;
    }

    LE_DEBUG("update result %d", updateResult);

    sprintf(buf, "%d", (int)updateResult);

    fwrite(buf, strlen(buf), 1, file);

    fclose(file);

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get update state
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetUpdateState
(
    lwm2mcore_fwUpdateState_t *updateState
)
{
    FILE *file;
    char buf[2];

    file = fopen(STATE_PATH, "r");
    if (!file)
    {
        if (ENOENT == errno)
        {
            LE_ERROR("download not started");
            *updateState = LWM2MCORE_FW_UPDATE_STATE_IDLE;
            return LE_OK;
        }
        LE_ERROR("failed to open %s: %m", STATE_PATH);
        return LE_FAULT;
    }

    memset(buf, 0, 2);
    fread(buf, 2, 1, file);

    fclose(file);

    LE_DEBUG("update state %s", buf);

    *updateState = (int)strtol(buf, NULL, 10);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get update result
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetUpdateResult
(
    lwm2mcore_fwUpdateResult_t *updateResult
)
{
    FILE *file;
    char buf[2];

    file = fopen(RESULT_PATH, "r");
    if (!file)
    {
        if (ENOENT == errno)
        {
            LE_ERROR("download not started");
            *updateResult = LWM2MCORE_FW_UPDATE_RESULT_INSTALLED_SUCCESSFUL;
            return LE_OK;
        }
        LE_ERROR("failed to open %s: %m", RESULT_PATH);
        return LE_FAULT;
    }

    memset(buf, 0, 2);
    fread(buf, 2, 1, file);

    fclose(file);

    LE_DEBUG("update result %s", buf);

    *updateResult = (int)strtol(buf, NULL, 10);

    return LE_OK;
}

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

    if (isSync == false)
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
 * Downlod and store a package
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
    char *dwlType[3] = {
        [0] = "FW_UPDATE",
        [1] = "SW_UPDATE",
    };

    LE_DEBUG("downloading a `%s' from `%s'", dwlType[type], uriPtr);

    memset(data.packageUri, 0, LWM2MCORE_PACKAGE_URI_MAX_LEN + 1);
    memcpy(data.packageUri, uriPtr, strlen(uriPtr));
    data.packageSize = 0;

    if (stat(PKGDWL_PATH, &st) == -1)
    {
        if (mkdir("/tmp/pkgdwl", 0700) == -1)
        {
            LE_ERROR("failed to create pkgdwl directory %m");
            return LE_FAULT;
        }
    }

    if ( (-1 == mkfifo(FIFO_PATH, 0600)) && (EEXIST != errno) )
    {
        LE_ERROR("failed to create fifo: %m");
        return LE_FAULT;
    }

    if (unlink(STATE_PATH) == -1)
    {
        if (errno != ENOENT)
        {
            LE_ERROR("failed to unlink %s: %m", STATE_PATH);
            return LE_FAULT;
        }
    }

    if (unlink(RESULT_PATH) == -1)
    {
        if (errno != ENOENT)
        {
            LE_ERROR("failed to unlink %s: %m", RESULT_PATH);
            return LE_FAULT;
        }
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
    pkgDwl.initDownload = InitDownload;
    pkgDwl.getInfo = GetInfo;
    pkgDwl.setUpdateState = packageDownloader_SetUpdateStateModified;
    pkgDwl.setUpdateResult = packageDownloader_SetUpdateResultModified;
    pkgDwl.downloadRange = DownloadRange;
    pkgDwl.storeRange = StoreRange;
    pkgDwl.endDownload = EndDownload;
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
    /* The Update State needs to be set to default value
     * The package URI needs to be deleted from storage file
     * Any active download is suspended
     */
    packageDownloader_SetUpdateStateModified(LWM2MCORE_FW_UPDATE_STATE_IDLE);

    return LE_OK;
}

