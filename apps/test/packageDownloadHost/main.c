/**
 * @file main.c
 *
 * This application is used to perform unitary tests on the package downloader
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "main.h"
#include "packageDownloader.h"
#include "limit.h"

//--------------------------------------------------------------------------------------------------
/**
 * Maximum path length
 */
//--------------------------------------------------------------------------------------------------
#define PATH_MAX_LENGTH    LWM2MCORE_PACKAGE_URI_MAX_BYTES

//--------------------------------------------------------------------------------------------------
/**
 * Package size
 */
//--------------------------------------------------------------------------------------------------
#define PACKAGE_SIZE    1024

//--------------------------------------------------------------------------------------------------
/**
 * Relative location to the test download image
 */
//--------------------------------------------------------------------------------------------------
#define DOWNLOAD_URI    "../data/test.dwl"

//--------------------------------------------------------------------------------------------------
/**
 * Absolute location of the firmware image to be sent to the modem
 */
//--------------------------------------------------------------------------------------------------
#define FWUPDATE_ABS_PATH_FILE     "/tmp/data/le_fs/fwupdate"

//--------------------------------------------------------------------------------------------------
/**
 * Image start offset in bytes
 */
//--------------------------------------------------------------------------------------------------
#define CWE_IMAGE_START_OFFSET     0x140

//--------------------------------------------------------------------------------------------------
/**
 * Image signature size in bytes
 */
//--------------------------------------------------------------------------------------------------
#define CWE_IMAGE_SIGNATURE_SIZE    0x120

//--------------------------------------------------------------------------------------------------
/**
 * Static Thread Reference
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t TestRef;

//--------------------------------------------------------------------------------------------------
/**
 * Test synchronization semaphore
 */
//--------------------------------------------------------------------------------------------------
static le_sem_Ref_t SyncSemRef;

//--------------------------------------------------------------------------------------------------
/**
 * Global structure that holds the download results
 */
//--------------------------------------------------------------------------------------------------
DownloadResult_t DownloadResult;

//--------------------------------------------------------------------------------------------------
/**
 *  Notify the end of download
 */
//--------------------------------------------------------------------------------------------------
void NotifyCompletion
(
    DownloadResult_t* result
)
{
    // Store the download result in the global structure
    memcpy(&DownloadResult, result, sizeof(DownloadResult_t));

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Find the path containing the currently-running program executable
 */
//--------------------------------------------------------------------------------------------------
le_result_t GetExecPath(char* buffer)
{
    int length;
    char* pathEndPtr = NULL;

    length = readlink("/proc/self/exe", buffer, PATH_MAX_LENGTH - 1);
    if (length <= 0)
    {
        return LE_FAULT;
    }
    buffer[length] = '\0';

    // Delete the binary name from the path
    pathEndPtr = strrchr(buffer, '/');
    if (NULL == pathEndPtr)
    {
        return LE_FAULT;
    }
    *(pathEndPtr+1) = '\0';

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Compare the downloaded file regarding the source file
 */
//--------------------------------------------------------------------------------------------------
static le_result_t CheckDownloadedFile
(
    char* sourceFilePath
)
{
    le_result_t result = LE_OK;
    FileComp_t sourceFile, dwnldedFile;

    sourceFile.fd = open(sourceFilePath, O_RDONLY);
    if (sourceFile.fd == -1)
    {
        LE_ERROR("Unable to open file '%s' for reading (%m).", sourceFilePath);
        return LE_FAULT;
    }

    dwnldedFile.fd = open(FWUPDATE_ABS_PATH_FILE, O_RDONLY);
    if (dwnldedFile.fd == -1)
    {
        LE_ERROR("Unable to open file '%s' for reading (%m).", FWUPDATE_ABS_PATH_FILE);
        close(sourceFile.fd);
        return LE_FAULT;
    }

    // Check downloaded file size
    sourceFile.fileSize = lseek(sourceFile.fd, 0, SEEK_END);
    dwnldedFile.fileSize = lseek(dwnldedFile.fd, 0, SEEK_END);

    if (dwnldedFile.fileSize != (sourceFile.fileSize - CWE_IMAGE_START_OFFSET -
        CWE_IMAGE_SIGNATURE_SIZE))
    {
        close(sourceFile.fd);
        close(dwnldedFile.fd);
        return LE_FAULT;
    }

    // The downloaded file is a shrink from the original file. So, we apply an offset
    // before starting comparison
    if (lseek(sourceFile.fd, CWE_IMAGE_START_OFFSET, SEEK_SET) == -1)
    {
        LE_ERROR("Seek file to offset %d failed.", CWE_IMAGE_START_OFFSET);
        close(sourceFile.fd);
        close(dwnldedFile.fd);
        return LE_FAULT;
    }

    lseek(dwnldedFile.fd, 0, SEEK_SET);

    do
    {
        dwnldedFile.readBytes = read(dwnldedFile.fd, dwnldedFile.buffer,
                                     sizeof(dwnldedFile.buffer));
        if (dwnldedFile.readBytes <= 0)
        {
            break;
        }

        sourceFile.readBytes = read(sourceFile.fd, sourceFile.buffer, sizeof(sourceFile.buffer));
        if (sourceFile.readBytes <= 0)
        {
            // Original file should not be smaller than the downloaded file
            result = LE_FAULT;
            break;
        }

        if (0 != memcmp(sourceFile.buffer, dwnldedFile.buffer, dwnldedFile.readBytes))
        {
            result = LE_FAULT;
            break;
        }

    } while (true);

    close(sourceFile.fd);
    close(dwnldedFile.fd);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 0: Initialize the Package Downloader
 */
//--------------------------------------------------------------------------------------------------
static void Test_InitPackageDownloader
(
    void* param1Ptr,
    void* param2Ptr
)
{
    LE_INFO("Running test : %s\n", __func__);

    LE_ASSERT_OK(packageDownloader_Init());

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 1: Download a regular Firmware.
 */
//--------------------------------------------------------------------------------------------------
static void Test_DownloadRegularFw
(
    void* param1Ptr,
    void* param2Ptr
)
{
    uint16_t instanceId = 0;
    char     path[PATH_MAX_LENGTH] = {0};

    LE_INFO("Running test: %s\n", __func__);

    GetExecPath(path);

    LE_ASSERT((strlen(path) + strlen(DOWNLOAD_URI)) < LWM2MCORE_PACKAGE_URI_MAX_BYTES);
    strncat(path, DOWNLOAD_URI, strlen(DOWNLOAD_URI));

    LE_ASSERT_OK(packageDownloader_Init());

    LE_ASSERT(LWM2MCORE_ERR_COMPLETED_OK == lwm2mcore_SetUpdatePackageUri(LWM2MCORE_FW_UPDATE_TYPE,
              instanceId, path, strlen(path)));
}

//--------------------------------------------------------------------------------------------------
/**
 *  This function checks the test 1 results.
 */
//--------------------------------------------------------------------------------------------------
static void Check_DownloadRegularFw
(
    void* param1Ptr,
    void* param2Ptr
)
{
    char path[PATH_MAX_LENGTH] = {0};

    // Check download result
    LE_ASSERT(LE_AVC_DOWNLOAD_COMPLETE == DownloadResult.updateStatus);
    LE_ASSERT(LE_AVC_FIRMWARE_UPDATE == DownloadResult.updateType);
    LE_ASSERT(-1 == DownloadResult.totalNumBytes);
    LE_ASSERT(-1 == DownloadResult.dloadProgress);
    LE_ASSERT(LE_AVC_ERR_NONE == DownloadResult.errorCode);

    GetExecPath(path);

    LE_ASSERT((strlen(path) + strlen(DOWNLOAD_URI)) < LWM2MCORE_PACKAGE_URI_MAX_BYTES);
    strncat(path, DOWNLOAD_URI, strlen(DOWNLOAD_URI));

    // Compare the downloaded file and the source file
    LE_ASSERT_OK(CheckDownloadedFile(path));

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 2: Test packageDownloader_SetFwUpdateState() and packageDownloader_GetFwUpdateState().
 */
//--------------------------------------------------------------------------------------------------
static void Test_SetAndGetFwUpdateState
(
    void* param1Ptr,
    void* param2Ptr
)
{
    lwm2mcore_FwUpdateState_t fwUpdateState;

    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT(DWL_OK == packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_DOWNLOADED));

    LE_ASSERT(LE_FAULT == packageDownloader_GetFwUpdateState(NULL));
    LE_ASSERT_OK(packageDownloader_GetFwUpdateState(&fwUpdateState));
    LE_ASSERT(LWM2MCORE_FW_UPDATE_STATE_DOWNLOADED == fwUpdateState);

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 3: Test packageDownloader_SetFwUpdateResult() and packageDownloader_GetFwUpdateResult().
 */
//--------------------------------------------------------------------------------------------------
static void Test_SetAndGetFwUpdateResult
(
    void* param1Ptr,
    void* param2Ptr
)
{
    lwm2mcore_FwUpdateResult_t fwUpdateResult;

    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT(DWL_OK == packageDownloader_SetFwUpdateResult(
                            LWM2MCORE_FW_UPDATE_RESULT_INSTALLED_SUCCESSFUL));

    LE_ASSERT(LE_BAD_PARAMETER == packageDownloader_GetFwUpdateResult(NULL));
    LE_ASSERT_OK(packageDownloader_GetFwUpdateResult(&fwUpdateResult));
    LE_ASSERT(LWM2MCORE_FW_UPDATE_RESULT_INSTALLED_SUCCESSFUL == fwUpdateResult);

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 4: Test packageDownloader_SetSwUpdateState() and packageDownloader_GetSwUpdateState().
 */
//--------------------------------------------------------------------------------------------------
static void Test_SetAndGetSwUpdateState
(
    void* param1Ptr,
    void* param2Ptr
)
{
    lwm2mcore_SwUpdateState_t swUpdateState;

    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT(DWL_OK == packageDownloader_SetSwUpdateState(LWM2MCORE_SW_UPDATE_STATE_DOWNLOADED));

    LE_ASSERT(LE_FAULT == packageDownloader_GetSwUpdateState(NULL));
    LE_ASSERT_OK(packageDownloader_GetSwUpdateState(&swUpdateState));
    LE_ASSERT(LWM2MCORE_SW_UPDATE_STATE_DOWNLOADED == swUpdateState);

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 4: Test packageDownloader_SetSwUpdateResult() and packageDownloader_GetSwUpdateResult().
 */
//--------------------------------------------------------------------------------------------------
static void Test_SetAndGetSwUpdateResult
(
    void* param1Ptr,
    void* param2Ptr
)
{
    lwm2mcore_SwUpdateResult_t swUpdateResult;

    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT(DWL_OK == packageDownloader_SetSwUpdateResult(
                            LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED));

    LE_ASSERT(LE_BAD_PARAMETER == packageDownloader_GetSwUpdateResult(NULL));
    LE_ASSERT_OK(packageDownloader_GetSwUpdateResult(&swUpdateResult));
    LE_ASSERT(LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED == swUpdateResult);

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 5: Test packageDownloader_SetUpdatePackageSize() and
                 packageDownloader_GetUpdatePackageSize().
 */
//--------------------------------------------------------------------------------------------------
static void Test_SetAndGetUpdatePackageSize
(
    void* param1Ptr,
    void* param2Ptr
)
{
    uint64_t packageSize;

    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT_OK(packageDownloader_SetUpdatePackageSize(PACKAGE_SIZE));

    LE_ASSERT(LE_FAULT == packageDownloader_GetUpdatePackageSize(NULL));
    LE_ASSERT_OK(packageDownloader_GetUpdatePackageSize(&packageSize));
    LE_ASSERT(PACKAGE_SIZE == packageSize);

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 6: Test packageDownloader_SetFwUpdateNotification() and
 *               packageDownloader_GetFwUpdateNotification().
 */
//--------------------------------------------------------------------------------------------------
static void Test_SetAndGetFwUpdateNotification
(
    void* param1Ptr,
    void* param2Ptr
)
{
    bool               notifRequested;
    le_avc_Status_t    updateStatus;
    le_avc_ErrorCode_t errorCode;

    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT_OK(packageDownloader_SetFwUpdateNotification(true, LE_AVC_DOWNLOAD_COMPLETE,
                                                                           LE_AVC_ERR_NONE));

    LE_ASSERT(LE_FAULT == packageDownloader_GetFwUpdateNotification(NULL, NULL, NULL));
    LE_ASSERT_OK(packageDownloader_GetFwUpdateNotification(&notifRequested,
                                                           &updateStatus, &errorCode));
    LE_ASSERT((true == notifRequested) && (LE_AVC_DOWNLOAD_COMPLETE == updateStatus)
                                       && (LE_AVC_ERR_NONE == errorCode));

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 7: Test packageDownloader_AbortDownload() and
 *               packageDownloader_CheckDownloadToAbort().
 */
//--------------------------------------------------------------------------------------------------
static void Test_AbortDownload
(
    void* param1Ptr,
    void* param2Ptr
)
{
    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT(false == packageDownloader_CheckDownloadToAbort());

    LE_ASSERT_OK(packageDownloader_AbortDownload(LWM2MCORE_FW_UPDATE_TYPE));
    LE_ASSERT(true == packageDownloader_CheckDownloadToAbort());

    LE_ASSERT_OK(packageDownloader_AbortDownload(LWM2MCORE_SW_UPDATE_TYPE));
    LE_ASSERT(true == packageDownloader_CheckDownloadToAbort());

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 8: Test packageDownloader_SuspendDownload() and
 *               packageDownloader_CheckDownloadToSuspend().
 */
//--------------------------------------------------------------------------------------------------
static void Test_SuspendDownload
(
    void* param1Ptr,
    void* param2Ptr
)
{
    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT(false == packageDownloader_CheckDownloadToSuspend());
    LE_ASSERT_OK(packageDownloader_SuspendDownload());
    LE_ASSERT(true == packageDownloader_CheckDownloadToSuspend());

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 9: Test packageDownloader_SetFwUpdateInstallPending() and
 *               packageDownloader_GetFwUpdateInstallPending().
 */
//--------------------------------------------------------------------------------------------------
static void Test_SetAndGetFwUpdateInstallPending
(
    void* param1Ptr,
    void* param2Ptr
)
{
    bool isFwInstallPending;

    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT(LE_BAD_PARAMETER ==  packageDownloader_GetFwUpdateInstallPending(NULL));

    LE_ASSERT_OK(packageDownloader_SetFwUpdateInstallPending(false));
    LE_ASSERT_OK(packageDownloader_GetFwUpdateInstallPending(&isFwInstallPending));
    LE_ASSERT(false == isFwInstallPending);

    LE_ASSERT_OK(packageDownloader_SetFwUpdateInstallPending(true));
    LE_ASSERT_OK(packageDownloader_GetFwUpdateInstallPending(&isFwInstallPending));
    LE_ASSERT(true == isFwInstallPending);

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 10: Test packageDownloader_DeleteFwUpdateInfo().
 */
//--------------------------------------------------------------------------------------------------
static void Test_DeleteFwUpdateInfo
(
    void* param1Ptr,
    void* param2Ptr
)
{
    LE_INFO("Running test: %s\n", __func__);

    packageDownloader_DeleteFwUpdateInfo();

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 11: Test packageDownloader_SetResumeInfo()
 *                packageDownloader_GetResumeInfo() and
 *                packageDownloader_BytesLeftToDownload().
 */
//--------------------------------------------------------------------------------------------------
static void Test_SetAndGetResumeInfo
(
    void* param1Ptr,
    void* param2Ptr
)
{
    char     path[PATH_MAX_LENGTH] = {0};
    char     uri[LWM2MCORE_PACKAGE_URI_MAX_BYTES]= {0};
    size_t   uriLen = sizeof(uri);
    size_t   uriSwLen = sizeof(uri);
    uint64_t numBytes;

    lwm2mcore_UpdateType_t type;

    LE_INFO("Running test: %s\n", __func__);

    GetExecPath(path);

    LE_ASSERT((strlen(path) + strlen(DOWNLOAD_URI)) < LWM2MCORE_PACKAGE_URI_MAX_BYTES);
    strncat(path, DOWNLOAD_URI, strlen(DOWNLOAD_URI));

    LE_ASSERT_OK(packageDownloader_Init());

    // Test for bad parameter
    LE_ASSERT(LE_BAD_PARAMETER == packageDownloader_SetResumeInfo(NULL, LWM2MCORE_FW_UPDATE_TYPE));
    LE_ASSERT(LE_BAD_PARAMETER == packageDownloader_GetResumeInfo(NULL, NULL, NULL));

    // Test for fw update type.
    LE_ASSERT_OK(packageDownloader_SetResumeInfo(path, LWM2MCORE_FW_UPDATE_TYPE));
    LE_ASSERT_OK(packageDownloader_GetResumeInfo(uri, &uriLen, &type));

    LE_ASSERT(LE_FAULT == packageDownloader_BytesLeftToDownload(&numBytes));

    LE_ASSERT(DWL_OK == packageDownloader_SetFwUpdateResult(
                            LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL));
    // Check number of bytes left when fw update state is idle.
    LE_ASSERT(DWL_OK == packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE));
    LE_ASSERT_OK(packageDownloader_BytesLeftToDownload(&numBytes));

    // Check number of bytes left when fw update state is downloading.
    LE_ASSERT(DWL_OK == packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_DOWNLOADING));
    LE_ASSERT_OK(packageDownloader_BytesLeftToDownload(&numBytes));

    // Test for sw update type.
    LE_ASSERT_OK(packageDownloader_SetResumeInfo(path, LWM2MCORE_SW_UPDATE_TYPE));
    LE_ASSERT_OK(packageDownloader_GetResumeInfo(uri, &uriSwLen, &type));

    LE_ASSERT(LE_FAULT == packageDownloader_BytesLeftToDownload(&numBytes));

    LE_ASSERT(DWL_OK == packageDownloader_SetSwUpdateResult(
                            LWM2MCORE_SW_UPDATE_RESULT_INITIAL));
    LE_ASSERT(DWL_OK == packageDownloader_SetSwUpdateState(
                            LWM2MCORE_SW_UPDATE_STATE_DOWNLOAD_STARTED));
    LE_ASSERT_OK(packageDownloader_BytesLeftToDownload(&numBytes));

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test packageDownloader_BytesLeftToDownload.
 */
//--------------------------------------------------------------------------------------------------
static void Test_BytesLeftToDownload
(
    void* param1Ptr,
    void* param2Ptr
)
{
    uint64_t numBytes;

    LE_INFO("Running test: %s\n", __func__);

    LE_ASSERT(LE_BAD_PARAMETER == packageDownloader_BytesLeftToDownload(NULL));
    LE_ASSERT_OK(packageDownloader_BytesLeftToDownload(&numBytes));

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Package Downloader Test Thread.
 *  The reason of creating a thread is mainely because the package downloader must be called from a
 *  thread and it needs a timer to run
 */
//--------------------------------------------------------------------------------------------------
static void* TestThread
(
    void
)
{
    le_sem_Post(SyncSemRef);

    le_event_RunLoop();

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Main entry component
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("======== START UnitTest of PACKAGE DOWNLOADER ========");

    // Create a semaphore to coordinate the test
    SyncSemRef = le_sem_Create("sync-test", 0);

    // Create test thread
    TestRef = le_thread_Create("PackageDownloadTester", (void*)TestThread, NULL);
    le_thread_SetJoinable(TestRef);

    // Wait for the thread to be started
    le_thread_Start(TestRef);
    le_sem_Wait(SyncSemRef);

    // Test 0: Initialize packet forwarder
    le_event_QueueFunctionToThread(TestRef, Test_InitPackageDownloader, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    // Test 1: Download a regular firmware and check results
    le_event_QueueFunctionToThread(TestRef, Test_DownloadRegularFw, NULL, NULL);
    le_sem_Wait(SyncSemRef);
    le_event_QueueFunctionToThread(TestRef, Check_DownloadRegularFw, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_BytesLeftToDownload, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_SetAndGetFwUpdateState, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_SetAndGetFwUpdateResult, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_SetAndGetSwUpdateState, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_SetAndGetSwUpdateResult, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_SetAndGetFwUpdateNotification, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_SetAndGetFwUpdateInstallPending, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_SetAndGetUpdatePackageSize, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_SetAndGetResumeInfo, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_SuspendDownload, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_BytesLeftToDownload, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_AbortDownload, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    le_event_QueueFunctionToThread(TestRef, Test_DeleteFwUpdateInfo, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    // Kill the test thread
    le_thread_Cancel(TestRef);
    le_thread_Join(TestRef, NULL);

    le_sem_Delete(SyncSemRef);

    LE_INFO("======== UnitTest of PACKAGE DOWNLOADER FINISHED ========");

    exit(EXIT_SUCCESS);
}
