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
#include "packageDownloader.h"
#include "avcAppUpdate.h"

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
 * Firmware download state path
 */
//--------------------------------------------------------------------------------------------------
#define FW_STATE_PATH       PKGDWL_PATH "/" "fwstate"

//--------------------------------------------------------------------------------------------------
/**
 * Firmware result state path
 */
//--------------------------------------------------------------------------------------------------
#define FW_RESULT_PATH      PKGDWL_PATH "/" "fwresult"

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
 * Update state and update result buffer size
 */
//--------------------------------------------------------------------------------------------------
#define BUFSIZE             3

//--------------------------------------------------------------------------------------------------
/**
 * SetFwUpdateState temporary callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetFwUpdateStateModified
(
    lwm2mcore_fwUpdateState_t updateState
)
{
    FILE *file;
    char buf[BUFSIZE];

    file = fopen(FW_STATE_PATH, "w");
    if (!file)
    {
        LE_ERROR("failed to open %s: %m", FW_STATE_PATH);
        return DWL_FAULT;
    }

    snprintf(buf, BUFSIZE, "%d", (int)updateState);

    fwrite(buf, strlen(buf), 1, file);

    fclose(file);

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * SetFwUpdateResult temporary callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetFwUpdateResultModified
(
    lwm2mcore_fwUpdateResult_t updateResult
)
{
    FILE *file;
    char buf[BUFSIZE];

    file = fopen(FW_RESULT_PATH, "w");

    if (!file)
    {
        LE_ERROR("failed to open %s: %m", FW_RESULT_PATH);
        return DWL_FAULT;
    }

    snprintf(buf, BUFSIZE, "%d", (int)updateResult);

    fwrite(buf, strlen(buf), 1, file);

    fclose(file);

    return DWL_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update state
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateState
(
    lwm2mcore_fwUpdateState_t* updateStatePtr
)
{
    FILE *file;
    char buf[BUFSIZE];

    if (!updateStatePtr)
    {
        LE_ERROR("invalid input parameter");
        return LE_FAULT;
    }

    file = fopen(FW_STATE_PATH, "r");
    if (!file)
    {
        if (ENOENT == errno)
        {
            LE_ERROR("download not started");
            *updateStatePtr = LWM2MCORE_FW_UPDATE_STATE_IDLE;
            return LE_OK;
        }
        LE_ERROR("failed to open %s: %m", FW_STATE_PATH);
        return LE_FAULT;
    }

    memset(buf, 0, BUFSIZE);
    fread(buf, BUFSIZE, 1, file);

    fclose(file);

    LE_DEBUG("update state %s", buf);

    *updateStatePtr = (int)strtol(buf, NULL, 10);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update result
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateResult
(
    lwm2mcore_fwUpdateResult_t* updateResultPtr
)
{
    FILE *file;
    char buf[BUFSIZE];

    if (!updateResultPtr)
    {
        LE_ERROR("invalid input parameter");
        return LE_FAULT;
    }

    file = fopen(FW_RESULT_PATH, "r");
    if (!file)
    {
        if (ENOENT == errno)
        {
            LE_ERROR("download not started");
            *updateResultPtr = LWM2MCORE_FW_UPDATE_RESULT_INSTALLED_SUCCESSFUL;
            return LE_OK;
        }
        LE_ERROR("failed to open %s: %m", FW_RESULT_PATH);
        return LE_FAULT;
    }

    memset(buf, 0, BUFSIZE);
    fread(buf, BUFSIZE, 1, file);

    fclose(file);

    LE_DEBUG("update result %s", buf);

    *updateResultPtr = (int)strtol(buf, NULL, 10);

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

    if (-1 == unlink(FW_STATE_PATH))
    {
        if (errno != ENOENT)
        {
            LE_ERROR("failed to unlink %s: %m", FW_STATE_PATH);
            return LE_FAULT;
        }
    }

    if (-1 == unlink(FW_RESULT_PATH))
    {
        if (errno != ENOENT)
        {
            LE_ERROR("failed to unlink %s: %m", FW_RESULT_PATH);
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
            dwlCtx.storePackage = (void *)avcApp_StoreSwPackage;
            break;
        default:
            LE_ERROR("unknown download type");
            return LE_FAULT;
    }

    pkgDwl.data = data;
    pkgDwl.initDownload = pkgDwlCb_InitDownload;
    pkgDwl.getInfo = pkgDwlCb_GetInfo;
    pkgDwl.setFwUpdateState = packageDownloader_SetFwUpdateStateModified;
    pkgDwl.setFwUpdateResult = packageDownloader_SetFwUpdateResultModified;
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
    lwm2mcore_updateType_t  type
)
{
    /* The Update State needs to be set to default value
     * The package URI needs to be deleted from storage file
     * Any active download is suspended
     */
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            packageDownloader_SetFwUpdateStateModified(LWM2MCORE_FW_UPDATE_STATE_IDLE);
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

