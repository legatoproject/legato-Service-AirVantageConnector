/**
 * @file main.c
 *
 * This application is used to perform unitary tests on the package downloader
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "main.h"
#include "limit.h"
#include "packageDownloader.h"
#include "downloader.h"
#include "le_httpClient_stub.h"

//--------------------------------------------------------------------------------------------------
/**
 * Maximum path length
 */
//--------------------------------------------------------------------------------------------------
#define PATH_MAX_LENGTH    LWM2MCORE_PACKAGE_URI_MAX_BYTES

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
 * Find the path containing the currently-running program executable
 */
//--------------------------------------------------------------------------------------------------
le_result_t GetExecPath
(
    char* bufferPtr
)
{
    int length;
    char* pathEndPtr = NULL;

    length = readlink("/proc/self/exe", bufferPtr, PATH_MAX_LENGTH - 1);
    if (length <= 0)
    {
        return LE_FAULT;
    }
    bufferPtr[length] = '\0';

    // Delete the binary name from the path
    pathEndPtr = strrchr(bufferPtr, '/');
    if (NULL == pathEndPtr)
    {
        return LE_FAULT;
    }
    *(pathEndPtr+1) = '\0';

    return LE_OK;
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
    LE_INFO("======== Running test : %s ========", __func__);

    LE_ASSERT_OK(packageDownloader_Init());

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test 1: Test downloader_GetPackageSize
 */
//--------------------------------------------------------------------------------------------------
static void Test_downloader_GetPackageSize
(
    void* param1Ptr,
    void* param2Ptr
)
{
    char     packageUriPtr[LWM2MCORE_PACKAGE_URI_MAX_BYTES];
    uint64_t packageSize = 0;
    keyHeader_t* keyTab;

    LE_INFO("======== Running test : %s ========", __func__);

    memset(packageUriPtr, 0, LWM2MCORE_PACKAGE_URI_MAX_BYTES);
    snprintf(packageUriPtr, LWM2MCORE_PACKAGE_URI_MAX_LEN, "%s", "http://www.somewhere.com/1234");
    LE_ASSERT(DOWNLOADER_INVALID_ARG == downloader_GetPackageSize(NULL, NULL));
    LE_ASSERT(DOWNLOADER_INVALID_ARG == downloader_GetPackageSize(packageUriPtr, NULL));
    LE_ASSERT(DOWNLOADER_INVALID_ARG == downloader_GetPackageSize(NULL, &packageSize));

    // Simulate the response to a HEAD command (no body): Success (HTTP 200)
    keyTab = malloc(sizeof(keyHeader_t));
    LE_ASSERT(keyTab);
    snprintf(keyTab->key, KEY_MAX_LEN - 1, "%s", "Content-Length");
    keyTab->keyLen = strlen(keyTab->key);
    snprintf(keyTab->keyValue, KEY_MAX_LEN - 1, "%s", "1000");
    keyTab->keyValueLen = strlen(keyTab->keyValue);
    keyTab->nextPtr = NULL;
    test_le_httpClient_SimulateHttpResponse(keyTab, HTTP_200, NULL, 0);
    free(keyTab);
    keyTab = NULL;
    LE_ASSERT(DOWNLOADER_OK == downloader_GetPackageSize(packageUriPtr, &packageSize));
    LE_ASSERT(packageSize == 1000);

    // Simulate the response to a HEAD command (no body): File not found (HTTP 404)
    packageSize = 0;
    memset(packageUriPtr, 0, LWM2MCORE_PACKAGE_URI_MAX_BYTES);
    snprintf(packageUriPtr, LWM2MCORE_PACKAGE_URI_MAX_LEN, "%s", "http://www.somewhere.com/1234");
    test_le_httpClient_SimulateHttpResponse(NULL, HTTP_404, NULL, 0);
    LE_ASSERT(DOWNLOADER_INVALID_ARG == downloader_GetPackageSize(packageUriPtr, &packageSize));
    LE_ASSERT(packageSize == 0);

    le_sem_Post(SyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Test packageDownloader_StartDownload
 */
//--------------------------------------------------------------------------------------------------
static void Test_packageDownloader_StartDownload
(
    void* param1Ptr,
    void* param2Ptr
)
{
    LE_INFO("======== Running test : %s ========", __func__);

    packageDownloader_StartDownload(LWM2MCORE_FW_UPDATE_TYPE, 0);
    test_le_httpClient_WaitDownloadSemaphore();
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
    // To reactivate for all DEBUG logs
    le_log_SetFilterLevel(LE_LOG_DEBUG);

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
    // To reactivate for all DEBUG logs
    le_log_SetFilterLevel(LE_LOG_DEBUG);

    LE_DEBUG("======== START UnitTest of PACKAGE DOWNLOADER ========");

    test_le_httpClient_Init();

    // Create a semaphore to coordinate the test
    SyncSemRef = le_sem_Create("sync-test", 0);

    // Create test thread
    TestRef = le_thread_Create("PackageDownloadTester", (void*)TestThread, NULL);
    le_thread_SetJoinable(TestRef);

    // Wait for the thread to be started
    le_thread_Start(TestRef);
    le_sem_Wait(SyncSemRef);

    // Test 0: Initialize package downloader
    le_event_QueueFunctionToThread(TestRef, Test_InitPackageDownloader, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    // Test 1: test downloader_GetPackageSize
    le_event_QueueFunctionToThread(TestRef, Test_downloader_GetPackageSize, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    // Test 2: test downloader_StartDownload
    le_event_QueueFunctionToThread(TestRef, Test_packageDownloader_StartDownload, NULL, NULL);
    le_sem_Wait(SyncSemRef);

    // Kill the test thread
    le_thread_Cancel(TestRef);
    le_thread_Join(TestRef, NULL);

    le_sem_Delete(SyncSemRef);

    LE_INFO("======== UnitTest of PACKAGE DOWNLOADER FINISHED ========");

    exit(EXIT_SUCCESS);
}
