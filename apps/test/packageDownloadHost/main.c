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
    int paddingLen;

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
        LE_ERROR("Seek file to offset %zd failed.", CWE_IMAGE_START_OFFSET);
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
    char path[PATH_MAX_LENGTH] = {0};

    LE_INFO("Running test: %s\n", __func__);

    GetExecPath(path);
    strncat(path, DOWNLOAD_URI, strlen(path));

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
    strncat(path, DOWNLOAD_URI, strlen(path));

    // Compare the downloaded file and the source file
    LE_ASSERT_OK(CheckDownloadedFile(path));

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

    // Kill the test thread
    le_thread_Cancel(TestRef);
    le_thread_Join(TestRef, NULL);

    le_sem_Delete(SyncSemRef);

    LE_INFO("======== UnitTest of PACKAGE DOWNLOADER FINISHED ========");

    exit(EXIT_SUCCESS);
}
