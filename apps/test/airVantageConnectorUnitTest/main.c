/**
 * This module implements the unit tests for the airVantage Connector.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "packageDownloader/packageDownloader.h"
#include "interfaces.h"
#include "avcServer/avcServer.h"

//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 *  Short semaphore timeout in seconds
 */
//--------------------------------------------------------------------------------------------------
#define SHORT_TIMEOUT   1

//--------------------------------------------------------------------------------------------------
/**
 *  Long semaphore timeout in seconds
 */
//--------------------------------------------------------------------------------------------------
#define LONG_TIMEOUT    10

// -------------------------------------------------------------------------------------------------
/**
 *  Application context structure
 */
// -------------------------------------------------------------------------------------------------
typedef struct
{
    le_sem_Ref_t                        appSemaphore;       ///< Sem reference
    le_thread_Ref_t                     appThreadRef;       ///< Thread reference
    le_avc_StatusEventHandlerRef_t      appStateHandlerRef; ///< AVC event handler reference
} AppContext_t;

// -------------------------------------------------------------------------------------------------
/**
 *  Application contexts
 */
// -------------------------------------------------------------------------------------------------
static AppContext_t AppCtx;

//--------------------------------------------------------------------------------------------------
/**
 * Defer and Download.
 *
 */
//--------------------------------------------------------------------------------------------------
static void DeferAndDownload
(
    uint32_t deferTime /// Defer time in minutes
)
{
    // defer download
    LE_ASSERT_OK(le_avc_DeferDownload(deferTime));
    // Accept Download
    LE_ASSERT_OK(le_avc_AcceptDownload());
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer and Install.
 *
 */
//--------------------------------------------------------------------------------------------------
static void DeferAndInstall
(
    uint32_t deferTime /// Defer time in minutes
)
{
    // Defer download
    LE_ASSERT_OK(le_avc_DeferInstall(deferTime));
    // Accept Download
    LE_ASSERT_OK(le_avc_AcceptInstall());
}

//--------------------------------------------------------------------------------------------------
/**
 * Defer and Install.
 *
 */
//--------------------------------------------------------------------------------------------------
static void DeferAndUninstall
(
    uint32_t deferTime /// Defer time in minutes
)
{
    // Defer download
    LE_ASSERT_OK(le_avc_DeferUninstall(deferTime));
    // Accept Download
    LE_ASSERT_OK(le_avc_AcceptUninstall());
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for AVC Event Notifications.
 *
 */
//--------------------------------------------------------------------------------------------------
static void AvcStateHandler
(
    le_avc_Status_t updateStatus,
        ///< Status of pending update, if available
    int32_t totalNumBytes,
        ///< Total number of bytes to be downloaded
    int32_t dloadProgress,
        ///< Download completion in percentage
    void* contextPtr
        ///< AVC session context
)
{
    LE_INFO("Update status %i", updateStatus);
    LE_INFO("totalNumBytes %d, dloadProgress %d", totalNumBytes, dloadProgress);

    le_avc_UpdateType_t updateType;

    switch ( updateStatus )
    {
        case LE_AVC_CONNECTION_PENDING:
            LE_INFO("AVC status LE_AVC_CONNECTION_PENDING");
            break;

        case LE_AVC_DOWNLOAD_PENDING:
        {
            LE_INFO("AVC status LE_AVC_DOWNLOAD_PENDING");

            if (LE_OK == le_avc_GetUpdateType(&updateType))
            {
                LE_INFO("Update type is %i", updateType);
            }
            else
            {
                LE_INFO("Update type is not available");
            }

            // Accept install should fail
            LE_ASSERT(LE_FAULT == le_avc_AcceptInstall());

            // Defer and accept download
            DeferAndDownload(1);
        }
        break;

        case LE_AVC_INSTALL_PENDING:
        {
            LE_INFO("AVC status LE_AVC_INSTALL_PENDING");
            // Defer and accept install
            DeferAndInstall(1);
        }
        break;

        case LE_AVC_UNINSTALL_PENDING:
            LE_INFO("AVC status LE_AVC_UNINSTALL_PENDING");
            DeferAndUninstall(1);
            break;

        case LE_AVC_REBOOT_PENDING:
            LE_INFO("AVC status LE_AVC_REBOOT_PENDING");
            break;

        case LE_AVC_DOWNLOAD_IN_PROGRESS:
            LE_INFO("AVC status LE_AVC_DOWNLOAD_IN_PROGRESS");
            break;

        case LE_AVC_DOWNLOAD_COMPLETE:
            LE_INFO("AVC status LE_AVC_DOWNLOAD_COMPLETE");
            break;

        case LE_AVC_UNINSTALL_IN_PROGRESS:
        case LE_AVC_UNINSTALL_FAILED:
        case LE_AVC_UNINSTALL_COMPLETE:
            LE_ERROR("Received unexpected update status.");
            break;

        case LE_AVC_NO_UPDATE:
        case LE_AVC_INSTALL_COMPLETE:
            LE_INFO("AVC status LE_AVC_NO_UPDATE");
            break;

        case LE_AVC_DOWNLOAD_FAILED:
        case LE_AVC_INSTALL_FAILED:
            LE_INFO("AVC status LE_AVC_DOWNLOAD_FAILED");
            break;

        case LE_AVC_SESSION_STARTED:
            LE_INFO("AVC status LE_AVC_SESSION_STARTED");
            break;

        case LE_AVC_INSTALL_IN_PROGRESS:
        case LE_AVC_SESSION_STOPPED:
            LE_INFO("AVC status LE_AVC_SESSION_STOPPED");
            break;

        case LE_AVC_AUTH_STARTED:
            LE_DEBUG("Authenticated started");
            break;

        case LE_AVC_AUTH_FAILED:
            LE_DEBUG("Authenticated failed");
            break;

        default:
            LE_DEBUG("Unhandled updateStatus");
            break;
    }

    le_sem_Post(AppCtx.appSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Synchronize test thread (i.e. main) and application threads
 */
//--------------------------------------------------------------------------------------------------
static void SynchronizeTest
(
    void
)
{
    le_clk_Time_t timeToWait = {LONG_TIMEOUT, 0};

    LE_ASSERT_OK(le_sem_WaitWithTimeOut(AppCtx.appSemaphore, timeToWait));
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: le_avc_StartSession().
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_avc_StartSession
(
    void* param1Ptr, /// Value to be passed as param1Ptr to the function
    void* param2Ptr  /// Value to be passed as param2Ptr to the function
)
{
    LE_INFO("======== Test le_avc_StartSession ========");
    AppContext_t* appCtxPtr = (AppContext_t*) param1Ptr;

    LE_ASSERT_OK(le_avc_StartSession());
    le_sem_Post(appCtxPtr->appSemaphore);
}
//--------------------------------------------------------------------------------------------------
/**
 * Test: le_avc_StartDownload().
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_avc_StartDownload
(
    void* param11Ptr, /// Value to be passed as param1Ptr to the function
    void* param12Ptr /// Value to be passed as param2Ptr to the function
)
{
    LE_INFO("======== Test le_avc_StartDownload ========");
    AppContext_t* appCtxPtr = (AppContext_t*) param11Ptr;
    uint64_t bytesToDownload = 10;
    lwm2mcore_UpdateType_t type = LWM2MCORE_SW_UPDATE_TYPE;
    avcServer_QueryDownload(packageDownloader_StartDownload,
                            bytesToDownload,
                            type,
                            true,
                            LE_AVC_ERR_NONE
                           );
    le_sem_Post(appCtxPtr->appSemaphore);
}
//--------------------------------------------------------------------------------------------------
/**
 * Test: le_avc_StopSession().
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_avc_StopSession
(
    void* param1Ptr, /// Value to be passed as param1Ptr to the function
    void* param2Ptr  /// Value to be passed as param2Ptr to the function
)
{
    LE_INFO("======== Test le_avc_StopSession ========");
    AppContext_t* appCtxPtr = (AppContext_t*) param1Ptr;


    LE_ASSERT_OK(le_avc_StopSession());
    le_sem_Post(appCtxPtr->appSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: le_avc_RemoveStatusEventHandler().
 *
 */
//--------------------------------------------------------------------------------------------------
static void RemoveStatusEventHandler
(
    void* param1Ptr, /// Value to be passed as param1Ptr to the function
    void* param2Ptr  /// Value to be passed as param2Ptr to the function
)
{
    LE_INFO("======== Test le_avc_RemoveStatusEventHandler ========");
    AppContext_t* appCtxPtr = (AppContext_t*) param1Ptr;

    le_avc_RemoveStatusEventHandler(appCtxPtr->appStateHandlerRef);

    le_sem_Post(appCtxPtr->appSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Polling
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_avc_Polling
(
    void* param1Ptr, /// Value to be passed as param1Ptr to the function
    void* param2Ptr  /// Value to be passed as param2Ptr to the function
)
{
    uint32_t pollingValue = LE_AVC_POLLING_TIMER_MIN_VAL;
    AppContext_t* appCtxPtr = (AppContext_t*) param1Ptr;

    LE_INFO("======== Test polling ========");
    LE_ASSERT_OK(le_avc_GetPollingTimer(&pollingValue));
    LE_ASSERT(0 == pollingValue);

    pollingValue = LE_AVC_POLLING_TIMER_MIN_VAL;
    LE_ASSERT_OK(le_avc_SetPollingTimer(pollingValue));
    LE_ASSERT_OK(le_avc_GetPollingTimer(&pollingValue));
    LE_ASSERT(LE_AVC_POLLING_TIMER_MIN_VAL == pollingValue);

    pollingValue = LE_AVC_POLLING_TIMER_MAX_VAL;
    LE_ASSERT_OK(le_avc_SetPollingTimer(pollingValue));
    LE_ASSERT_OK(le_avc_GetPollingTimer(&pollingValue));
    LE_ASSERT(LE_AVC_POLLING_TIMER_MAX_VAL == pollingValue);

    pollingValue = LE_AVC_POLLING_TIMER_MAX_VAL + 1;
    LE_ASSERT(LE_OUT_OF_RANGE == le_avc_SetPollingTimer(pollingValue));
    LE_ASSERT_OK(le_avc_GetPollingTimer(&pollingValue));
    LE_ASSERT(LE_AVC_POLLING_TIMER_MAX_VAL == pollingValue);

    le_sem_Post(appCtxPtr->appSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Function to simulate a CoAP push
 *
 */
//--------------------------------------------------------------------------------------------------
static le_result_t OnePush
(
    void
)
{
    #define CONTENT_TYPE_OCTAVE 12120

    #define LE_COAP_MAX_TOKEN_NUM_BYTES 9
    static uint8_t token[] = "mytoken";
    static char uri[] = "/push";
    static uint8_t payload[3];
    int step = 0;
    payload[step] = 0x01; step++;
    payload[step] = 0xf6; step++;
    payload[step] = 0x18;

    return le_coap_Push(uri,
                        (uint8_t*)token,
                        7,
                        CONTENT_TYPE_OCTAVE,
                        LE_COAP_TX_STREAM_START,
                        payload,
                        3);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: CoAP push handler
 *
 */
//--------------------------------------------------------------------------------------------------
void PushAckCallBack
(
    le_coap_PushStatus_t status,
    const uint8_t* token,
    size_t tokenLength,
    void* contextPtr
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: CoAP
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_coap
(
    void* param1Ptr, /// Value to be passed as param1Ptr to the function
    void* param2Ptr  /// Value to be passed as param2Ptr to the function
)
{
    AppContext_t* appCtxPtr = (AppContext_t*) param1Ptr;
    static int counter = 0;

    LE_INFO("======== Test CoAP ========");

    counter++;

    if (counter == 1)
    {
        le_coap_AddPushEventHandler(PushAckCallBack, NULL);
        // 1st push: OK
        LE_ASSERT_OK(OnePush());

        // No ack: 2nd push busy
        LE_ASSERT(LE_BUSY == OnePush());
    }
    else if (counter == 2)
    {
        avcServer_UpdateStatus(LE_AVC_SESSION_STOPPED, LE_AVC_UNKNOWN_UPDATE,
                                       -1, -1, LE_AVC_ERR_NONE);

    }
    else
    {
        sleep(1);
        // 3rd push: OK
        LE_ASSERT_OK(OnePush());
    }

    le_sem_Post(appCtxPtr->appSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: le_avc_GetUpdateType().
 *
 */
//--------------------------------------------------------------------------------------------------
static void GetUpdateType
(
    void* param1Ptr, /// Value to be passed as param1Ptr to the function
    void* param2Ptr  /// Value to be passed as param2Ptr to the function
)
{
    LE_INFO("======== Get session type ========");
    AppContext_t* appCtxPtr = (AppContext_t*) param1Ptr;

    le_avc_UpdateType_t updateType;

    // Get update type
    LE_ASSERT_OK(le_avc_GetUpdateType(&updateType));
    LE_INFO("Update type : %d", updateType);

    le_sem_Post(appCtxPtr->appSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Restart session.
 *
 */
//--------------------------------------------------------------------------------------------------
static void RestartSession
(
    void* param1Ptr, /// Value to be passed as param1Ptr to the function
    void* param2Ptr  /// Value to be passed as param2Ptr to the function
)
{
    LE_INFO("======== Test Restart Session ========");
    AppContext_t* appCtxPtr = (AppContext_t*) param1Ptr;

    // Stop the session
    LE_ASSERT_OK(le_avc_StopSession());
    // Start Session
    LE_ASSERT_OK(le_avc_StartSession());

    le_sem_Post(appCtxPtr->appSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Thread used to simulate an application
 */
//--------------------------------------------------------------------------------------------------
static void* AppHandler
(
    void* ctxPtr /// Application context
)
{
    AppContext_t* appCtxPtr = (AppContext_t*) ctxPtr;

    // Register handler for cellular network state change
    appCtxPtr->appStateHandlerRef = le_avc_AddStatusEventHandler(AvcStateHandler, ctxPtr);
    LE_ASSERT(NULL != appCtxPtr->appStateHandlerRef);
    LE_INFO("AvcStateHandler %p added", appCtxPtr->appStateHandlerRef);

    // Semaphore is used to synchronize the task execution with the core test
    le_sem_Post(appCtxPtr->appSemaphore);

    // Run the event loop
    le_event_RunLoop();

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * This thread is used to start airVantage connector unit tests
 */
//--------------------------------------------------------------------------------------------------
static void* AirVantageUnitTestThread
(
    void* contextPtr /// Context to be passed
)
{
    LE_INFO("AirVantage UT Thread Started");

    // Initialize application contexts
    memset(&AppCtx, 0, sizeof(AppContext_t));

    // Create a semaphore to coordinate the test
    AppCtx.appSemaphore = le_sem_Create("avcSem", 0);

    AppCtx.appThreadRef = le_thread_Create("avcThread", AppHandler, &AppCtx);
    le_thread_Start(AppCtx.appThreadRef);
    // Wait for thread start
    SynchronizeTest();

    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   Testle_avc_StartSession, &AppCtx, NULL);
    SynchronizeTest();
    le_thread_Sleep(1);

    le_avcTest_SimulateLwm2mEvent(LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_DETAILS,
                                  LWM2MCORE_SW_UPDATE_TYPE,
                                  1024,
                                  0);

    le_avcTest_SimulateLwm2mEvent(LWM2MCORE_EVENT_DOWNLOAD_PROGRESS,
                                  LWM2MCORE_SW_UPDATE_TYPE, 1024, 10);
    SynchronizeTest();

    // Start dowload
    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   Testle_avc_StartDownload, &AppCtx, NULL);
    SynchronizeTest();
    int i=0;
    for(i = 0; i <= 10; i++)
    {
        le_avcTest_SimulateLwm2mEvent(LWM2MCORE_EVENT_DOWNLOAD_PROGRESS,
                                      LWM2MCORE_SW_UPDATE_TYPE, 1024, 10*i);
        SynchronizeTest();
    }
    // Test get update type
    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   GetUpdateType, &AppCtx, NULL);
    SynchronizeTest();

    le_avcTest_SimulateLwm2mEvent(LWM2MCORE_EVENT_UPDATE_STARTED,
                                  LWM2MCORE_SW_UPDATE_TYPE, -1, -1);
    SynchronizeTest();

    // Test restart session
    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   RestartSession, &AppCtx, NULL);
    SynchronizeTest();

    // Test remove status handler
    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   RemoveStatusEventHandler, &AppCtx, NULL);
    SynchronizeTest();

    // Test avc stop session
    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   Testle_avc_StopSession, &AppCtx, NULL);
    SynchronizeTest();

    // Test polling
    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   Testle_avc_Polling, &AppCtx, NULL);
    SynchronizeTest();


    // Test CoAP push
    // Make 2 push: 1st OK, 2nd BUSY
    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   Testle_coap, &AppCtx, NULL);
    SynchronizeTest();

    // Call again: simulate a LE_AVC_SESSION_STOPPED event
    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   Testle_coap, &AppCtx, NULL);
    SynchronizeTest();

    // Wait for AVC handler call in coap.c
    sleep(1);

    // Make a Push, should be OK
    le_event_QueueFunctionToThread(AppCtx.appThreadRef,
                                   Testle_coap, &AppCtx, NULL);
    SynchronizeTest();


    LE_INFO("======== UnitTest of airVantage Connector Passed ========");

    exit(EXIT_SUCCESS);

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Main of the test
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // To reactivate for all DEBUG logs
    le_log_SetFilterLevel(LE_LOG_DEBUG);

    LE_INFO("======== Start UnitTest of airVantage Connector ========");

    // Start the unit test thread
    le_thread_Start(le_thread_Create("AirVantage UT Thread",
                                     AirVantageUnitTestThread, NULL));
}
