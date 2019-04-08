/**
 * This module implements some stubs for httpClient.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef LE_HTTPCLIENT_STUB
#define LE_HTTPCLIENT_STUB

#include "legato.h"
#include "interfaces.h"
#include "le_httpClientLib.h"

//--------------------------------------------------------------------------------------------------
/**
 * Define for maximum length of a key in HTTP header
 */
//--------------------------------------------------------------------------------------------------
#define KEY_MAX_LEN 100


//--------------------------------------------------------------------------------------------------
/**
 * Enum for HTTP client state machine
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    STATE_IDLE,            ///< State machine is in idle state
    STATE_REQ_LINE,        ///< Build and send HTTP request line
    STATE_REQ_CREDENTIAL,  ///< Append optional HTTP connection credential
    STATE_REQ_RESOURCE,    ///< Append optional user-defined resources (key/value pairs)
    STATE_REQ_BODY,        ///< Append optional user-defined body to HTTP request
    STATE_RESP_PARSE,      ///< Parse remote server response
    STATE_END              ///< Notify end of HTTP request transaction
}
HttpSessionState_t;

//--------------------------------------------------------------------------------------------------
/**
 * Structure that defines HTTP session context
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_httpClient_Ref_t reference;                 ///< Safe reference to this object
    le_httpCommand_t    command;                   ///< Command of current HTTP request
    le_result_t         result;                    ///< Result of current HTTP request
    HttpSessionState_t  state;                     ///< HTTP client current state

    le_httpClient_SendRequestRspCb_t responseCb;        ///< Asynchronous request result callback
    le_httpClient_BodyResponseCb_t   bodyResponseCb;    ///< User-defined callback: Body response
    le_httpClient_HeaderResponseCb_t headerResponseCb;  ///< User-defined callback: Header response
    le_httpClient_StatusCodeCb_t     statusCodeCb;      ///< User-defined callback: Status code
    le_httpClient_ResourceUpdateCb_t resourceUpdateCb;  ///< User-defined callback: Resources update
    le_httpClient_BodyConstructCb_t  bodyConstructCb;   ///< User-defined callback: Body construct
}
HttpSessionCtx_t;

//--------------------------------------------------------------------------------------------------
/**
 * Structure to define a key field in the HTTP header
 */
//--------------------------------------------------------------------------------------------------
typedef struct _keyHeader_t_
{
    struct _keyHeader_t_ * nextPtr; ///< Next field structure
    char    key[KEY_MAX_LEN];       ///< Key field
    int     keyLen;                 ///< Key field size
    char    keyValue[KEY_MAX_LEN];  ///< Key value field
    int     keyValueLen;            ///< Key value field size
}
keyHeader_t;

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Function to wait the download semaphore
 */
//--------------------------------------------------------------------------------------------------
void test_le_httpClient_WaitDownloadSemaphore
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Function to initialize the httpClient stub
 */
//--------------------------------------------------------------------------------------------------
void test_le_httpClient_Init
(
    void
);

#endif /* LE_HTTPCLIENT_STUB */