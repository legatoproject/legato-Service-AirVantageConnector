/**
 * This module implements some stubs for httpClient.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "le_httpClientLib.h"
#include "le_httpClient_stub.h"

//--------------------------------------------------------------------------------------------------
/**
 *                              TEST FUNCTIONS
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Static Thread Reference
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t DownloadTestRef;

//--------------------------------------------------------------------------------------------------
/**
 * Test download synchronization semaphore
 */
//--------------------------------------------------------------------------------------------------
static le_sem_Ref_t DownloadSyncSemRef;

//--------------------------------------------------------------------------------------------------
/**
 * Static for simulated HTTP status code response
 */
//--------------------------------------------------------------------------------------------------
static int HttpResponseCode = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Static structure list for key fiels in HTTP header response simulation
 */
//--------------------------------------------------------------------------------------------------
static keyHeader_t* KeyFieldPtr = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static char for HTTP response body simulation
 */
//--------------------------------------------------------------------------------------------------
static char* Body = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static char for HTTP response body length simulation
 */
//--------------------------------------------------------------------------------------------------
static int BodyLen = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Asynchronous request callback
 */
//--------------------------------------------------------------------------------------------------
static le_httpClient_SendRequestRspCb_t AsyncRequestCallback = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Function to prepare the le_httpClient with a specific HEAD response
 */
//--------------------------------------------------------------------------------------------------
void test_le_httpClient_SimulateHttpResponse
(
    keyHeader_t*    keyPtr,     ///[IN] Key field list
    int             status,     ///[IN] Status code
    char*           bodyPtr,    ///[IN] Body response
    int             bodyLen     ///[IN] Body response length
)
{
    if (keyPtr)
    {
        keyHeader_t* keyLocalPtr = keyPtr;
        while (keyLocalPtr)
        {
            keyHeader_t* newKeyPtr = malloc(sizeof(keyHeader_t));
            LE_ASSERT(NULL != newKeyPtr);
            newKeyPtr->nextPtr = NULL;

            snprintf(newKeyPtr->key, KEY_MAX_LEN - 1, "%s", keyLocalPtr->key);
            newKeyPtr->keyLen = strlen(keyLocalPtr->key);
            snprintf(newKeyPtr->keyValue, KEY_MAX_LEN - 1, "%s", keyLocalPtr->keyValue);
            newKeyPtr->keyValueLen = strlen(keyLocalPtr->keyValue);

            if (!KeyFieldPtr)
            {
                KeyFieldPtr = newKeyPtr;
            }
            else
            {
                keyHeader_t* listKeyPtr = KeyFieldPtr;
                while(listKeyPtr->nextPtr)
                {
                    listKeyPtr = listKeyPtr->nextPtr;
                }
                listKeyPtr->nextPtr = newKeyPtr;
            }

            keyLocalPtr = keyLocalPtr->nextPtr;
        }
    }

    if (bodyPtr)
    {
        Body = malloc(bodyLen * sizeof(char));
        memcpy(Body, bodyPtr, bodyLen);
    }
    else
    {
        if (Body)
        {
            free(Body);
        }
        Body = NULL;
    }
    BodyLen = bodyLen;

    HttpResponseCode = status;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Downloader Test Thread.
 */
//--------------------------------------------------------------------------------------------------
static void* DownloadTestThread
(
    void
)
{
    le_sem_Post(DownloadSyncSemRef);
    // To reactivate for all DEBUG logs
    le_log_SetFilterLevel(LE_LOG_DEBUG);

    le_event_RunLoop();

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Function to simulate asynchronous response
 */
//--------------------------------------------------------------------------------------------------
static void AsyncRequestRsp
(
    void* param1Ptr,
    void* param2Ptr
)
{
    HttpSessionCtx_t* contextPtr = (HttpSessionCtx_t*)param1Ptr;
    keyHeader_t* keyLocalPtr = KeyFieldPtr;

    if (!contextPtr)
    {
        LE_ERROR("Incorrect context");
        return;
    }

    // At this point, the data are sent
    // Simulate response
    while (keyLocalPtr)
    {
        contextPtr->headerResponseCb((le_httpClient_Ref_t)contextPtr,
                                     keyLocalPtr->key,
                                     keyLocalPtr->keyLen,
                                     keyLocalPtr->keyValue,
                                     keyLocalPtr->keyValueLen);
        keyLocalPtr = keyLocalPtr->nextPtr;
    }

    keyLocalPtr = KeyFieldPtr;

    while(keyLocalPtr)
    {
        keyHeader_t* listKeyPtr = keyLocalPtr->nextPtr;
        free(keyLocalPtr);
        keyLocalPtr = listKeyPtr;
    }
    KeyFieldPtr = NULL;

    contextPtr->statusCodeCb((le_httpClient_Ref_t)contextPtr, HttpResponseCode);
    if (Body)
    {
        contextPtr->bodyResponseCb((le_httpClient_Ref_t)contextPtr, Body, BodyLen);
        free(Body);
    }
    contextPtr->result = LE_OK;
    contextPtr->responseCb((le_httpClient_Ref_t)contextPtr, contextPtr->result);
    le_sem_Post(DownloadSyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to wait the download semaphore
 */
//--------------------------------------------------------------------------------------------------
void test_le_httpClient_WaitDownloadSemaphore
(
    void
)
{
    le_sem_Wait(DownloadSyncSemRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to initialize the httpClient stub
 */
//--------------------------------------------------------------------------------------------------
void test_le_httpClient_Init
(
    void
)
{
    // Create a semaphore to coordinate the download test
    DownloadSyncSemRef = le_sem_Create("download-sync-test", 0);
}

//--------------------------------------------------------------------------------------------------
/**
 *                              STUB FUNCTIONS
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Free a HTTP session context and make it available for future use.
 *
 * @return Socket descriptor
 */
//--------------------------------------------------------------------------------------------------
static void FreeHttpSessionContext
(
    HttpSessionCtx_t*    contextPtr    ///< [IN] Socket context pointer
)
{
    free(contextPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create a HTTP session reference and store the host address in a dedicated context.
 *
 * @return
 *  - Reference to the created context
 */
//--------------------------------------------------------------------------------------------------
le_httpClient_Ref_t le_httpClient_Create
(
    char*            hostPtr,     ///< [IN] HTTP server address
    uint16_t         port         ///< [IN] HTTP server port numeric number (0-65535)
)
{
    HttpSessionCtx_t* contextPtr = NULL;

    // Check input parameters
    if (NULL == hostPtr)
    {
        LE_ERROR("Unspecified host address");
        return NULL;
    }

    // Allocate a HTTP session context and save server parameters
    contextPtr = malloc(sizeof(HttpSessionCtx_t));
    if (NULL == contextPtr)
    {
        LE_ERROR("Unable to allocate a HTTP session context");
        return NULL;
    }

    contextPtr->state = STATE_IDLE;

    return (le_httpClient_Ref_t)contextPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a previously created HTTP socket and free allocated resources.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_Delete
(
    le_httpClient_Ref_t    ref  ///< [IN] HTTP session context reference
)
{
    HttpSessionCtx_t *contextPtr = (HttpSessionCtx_t *)ref;
    if (contextPtr == NULL)
    {
        LE_ERROR("Reference not found: %p", ref);
        return LE_BAD_PARAMETER;
    }

    FreeHttpSessionContext(contextPtr);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add a certificate to the HTTP session in order to make the connection secure
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_AddCertificate
(
    le_httpClient_Ref_t  ref,             ///< [IN] HTTP session context reference
    const uint8_t*       certificatePtr,  ///< [IN] Certificate Pointer
    size_t               certificateLen   ///< [IN] Certificate Length
)
{
    (void)ref;
    (void)certificatePtr;
    (void)certificateLen;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set a callback to handle HTTP response body data.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_SetBodyResponseCallback
(
    le_httpClient_Ref_t              ref,      ///< [IN] HTTP session context reference
    le_httpClient_BodyResponseCb_t   callback  ///< [IN] Callback
)
{
    HttpSessionCtx_t *contextPtr = (HttpSessionCtx_t *)ref;
    if (contextPtr == NULL)
    {
        LE_ERROR("Reference not found: %p", ref);
        return LE_BAD_PARAMETER;
    }

    contextPtr->bodyResponseCb = callback;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set callback to insert/update resources (key/value pairs) during a HTTP request.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_SetResourceUpdateCallback
(
    le_httpClient_Ref_t              ref,      ///< [IN] HTTP session context reference
    le_httpClient_ResourceUpdateCb_t callback  ///< [IN] Callback
)
{
    HttpSessionCtx_t *contextPtr = (HttpSessionCtx_t *)ref;
    if (contextPtr == NULL)
    {
        LE_ERROR("Reference not found: %p", ref);
        return LE_BAD_PARAMETER;
    }

    contextPtr->resourceUpdateCb = callback;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set a callback to handle HTTP header key/value pair.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_SetHeaderResponseCallback
(
    le_httpClient_Ref_t               ref,      ///< [IN] HTTP session context reference
    le_httpClient_HeaderResponseCb_t  callback  ///< [IN] Callback
)
{
    HttpSessionCtx_t *contextPtr = (HttpSessionCtx_t *)ref;
    if (contextPtr == NULL)
    {
        LE_ERROR("Reference not found: %p", ref);
        return LE_BAD_PARAMETER;
    }

    contextPtr->headerResponseCb = callback;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set callback to handle HTTP status code.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_SetStatusCodeCallback
(
    le_httpClient_Ref_t             ref,      ///< [IN] HTTP session context reference
    le_httpClient_StatusCodeCb_t    callback  ///< [IN] Callback
)
{
    HttpSessionCtx_t *contextPtr = (HttpSessionCtx_t *)ref;
    if (contextPtr == NULL)
    {
        LE_ERROR("Reference not found: %p", ref);
        return LE_BAD_PARAMETER;
    }

    contextPtr->statusCodeCb = callback;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initiate a connection with the server using the defined configuration.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_TIMEOUT       Timeout occurred during communication
 *  - LE_UNAVAILABLE   Unable to reach the server or DNS issue
 *  - LE_FAULT         Internal error
 *  - LE_NO_MEMORY     Memory allocation issue
 *  - LE_CLOSED        In case of end of file error
 *  - LE_COMM_ERROR    Connection failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_Start
(
    le_httpClient_Ref_t  ref    ///< [IN] HTTP session context reference
)
{
    HttpSessionCtx_t *contextPtr = (HttpSessionCtx_t *)ref;
    if (contextPtr == NULL)
    {
        LE_ERROR("Reference not found: %p", ref);
        return LE_BAD_PARAMETER;
    }

    // Simulate socket connection result
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop the HTTP connection with the server.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_Stop
(
    le_httpClient_Ref_t  ref     ///< [IN] HTTP session context reference
)
{
    HttpSessionCtx_t *contextPtr = (HttpSessionCtx_t *)ref;
    if (contextPtr == NULL)
    {
        LE_ERROR("Reference not found: %p", ref);
        return LE_BAD_PARAMETER;
    }

    // Simulate socket disconnection result
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send a HTTP command request and block until response is received from server or timeout reached.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_TIMEOUT       Timeout occurred during communication
 *  - LE_BUSY          Busy state machine
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_SendRequest
(
    le_httpClient_Ref_t  ref,              ///< [IN] HTTP session context reference
    le_httpCommand_t     command,          ///< [IN] HTTP command request
    char*                requestUriPtr     ///< [IN] URI buffer pointer
)
{
    HttpSessionCtx_t *contextPtr = (HttpSessionCtx_t *)ref;
    keyHeader_t* keyLocalPtr = KeyFieldPtr;

    if ((contextPtr == NULL) || (requestUriPtr == NULL))
    {
        LE_ERROR("Reference not found: %p", ref);
        return LE_BAD_PARAMETER;
    }

    if (command >= HTTP_MAX)
    {
        LE_ERROR("Unrecognized HTTP command: %d", command);
        return LE_BAD_PARAMETER;
    }

    if (contextPtr->state != STATE_IDLE)
    {
        LE_ERROR("Busy handling previous request. Current state: %d", contextPtr->state);
        return LE_BUSY;
    }

    // At this point, the data are sent
    // Simulate response
    while (keyLocalPtr)
    {
        contextPtr->headerResponseCb(ref,
                                     keyLocalPtr->key,
                                     keyLocalPtr->keyLen,
                                     keyLocalPtr->keyValue,
                                     keyLocalPtr->keyValueLen);
        keyLocalPtr = keyLocalPtr->nextPtr;
    }

    keyLocalPtr = KeyFieldPtr;

    while(keyLocalPtr)
    {
        keyHeader_t* listKeyPtr = keyLocalPtr->nextPtr;
        free(keyLocalPtr);
        keyLocalPtr = listKeyPtr;
    }
    KeyFieldPtr = NULL;

    contextPtr->statusCodeCb(ref, HttpResponseCode);
    if (Body)
    {
        contextPtr->bodyResponseCb(ref, Body, BodyLen);
        free(Body);
    }

    // Loop HTTP client state machine until the request is executed and response parsed.
    contextPtr->state = STATE_REQ_CREDENTIAL;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check whether the HTTP client mode is set to asynchronous.
 *
 * @return
 *  - True if the current mode is asynchronous, false otherwise.
 */
//--------------------------------------------------------------------------------------------------
bool le_httpClient_IsAsyncMode
(
    le_httpClient_Ref_t     ref       ///< [IN] HTTP session context reference
)
{
    (void)ref;
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send a HTTP command request to remote server. Response reception is handled in an asynchronous
 * way in the calling thread event loop.  This API is non-blocking.
 *
 * @note Function execution result can be retrieved through the provided callback
 */
//--------------------------------------------------------------------------------------------------
void le_httpClient_SendRequestAsync
(
    le_httpClient_Ref_t              ref,           ///< [IN] HTTP session context reference
    le_httpCommand_t                 command,       ///< [IN] HTTP command request
    char*                            requestUriPtr, ///< [IN] URI buffer pointer
    le_httpClient_SendRequestRspCb_t callback       ///< [IN] Function execution result callback
)
{
    le_result_t status = LE_OK;
    HttpSessionCtx_t *contextPtr = (HttpSessionCtx_t *)ref;

    if ((contextPtr == NULL) || (requestUriPtr == NULL))
    {
        LE_ERROR("Reference not found: %p", ref);
        return;
    }

    if (command >= HTTP_MAX)
    {
        LE_ERROR("Unrecognized HTTP command: %d", command);
        status = LE_BAD_PARAMETER;
        goto end;
    }

    if (contextPtr->state != STATE_IDLE)
    {
        LE_ERROR("Busy handling previous request. Current state: %d", contextPtr->state);
        status = LE_BUSY;
        goto end;
    }

    // At this point, asynchronous state machine will continue the request handling
    contextPtr->responseCb = callback;
    contextPtr->state = STATE_REQ_CREDENTIAL;

    AsyncRequestCallback = callback;

    // Create download test thread
    DownloadTestRef = le_thread_Create("DownloadTester", (void*)DownloadTestThread, NULL);
    le_thread_SetJoinable(DownloadTestRef);

    // Wait for the thread to be started
    le_thread_Start(DownloadTestRef);
    le_sem_Wait(DownloadSyncSemRef);

    le_event_QueueFunctionToThread(DownloadTestRef, AsyncRequestRsp, contextPtr, NULL);

    return;

end:
    if (callback)
    {
        callback(contextPtr->reference, status);
    }
    contextPtr->state = STATE_IDLE;

}

//--------------------------------------------------------------------------------------------------
/**
 * Enable or disable HTTP client asynchronous mode. By default, HTTP client is synchronous.
 *
 * @note If asynchronous mode is enabled, calling thread should provide an event loop to catch
 *       remote server events after using @c le_httpClient_SendRequestAsync() API.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_DUPLICATE     Request already executed
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_SetAsyncMode
(
    le_httpClient_Ref_t     ref,      ///< [IN] HTTP session context reference
    bool                    enable    ///< [IN] True to activate asynchronous mode, false otherwise
)
{
    (void)ref;
    (void)enable;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the HTTP session communication timeout. This timeout is used when server takes too much time
 * before responding.
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_httpClient_SetTimeout
(
    le_httpClient_Ref_t    ref,       ///< [IN] HTTP session context reference
    uint32_t               timeout    ///< [IN] Timeout in milliseconds
)
{
    (void)ref;
    (void)timeout;
    return LE_OK;
}