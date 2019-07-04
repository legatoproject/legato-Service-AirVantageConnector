/**
 * @file downloader.c
 *
 * Package downloader network layer
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <legato.h>
#include <interfaces.h>

#include <platform/types.h>
#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/lwm2mcorePackageDownloader.h>
#include "downloader.h"
#include "defaultDerKey.h"
#include "packageDownloader.h"
#include "le_httpClientLib.h"

//--------------------------------------------------------------------------------------------------
/**
 * Define value for HTTP protocol in HTTP header
 */
//--------------------------------------------------------------------------------------------------
#define HTTP_PROTOCOL       "http"

//--------------------------------------------------------------------------------------------------
/**
 * Define value for HTTPS protocol in HTTP header
 */
//--------------------------------------------------------------------------------------------------
#define HTTPS_PROTOCOL      "https"

//--------------------------------------------------------------------------------------------------
/**
 * Define value for HTTP port
 */
//--------------------------------------------------------------------------------------------------
#define HTTP_PORT           80

//--------------------------------------------------------------------------------------------------
/**
 * Define value for HTTPS port
 */
//--------------------------------------------------------------------------------------------------
#define HTTPS_PORT          443

//--------------------------------------------------------------------------------------------------
/**
 * Define value for base 10
 */
//--------------------------------------------------------------------------------------------------
#define BASE10              10

//--------------------------------------------------------------------------------------------------
/**
 * HTTP client timeout for data reception in milliseconds
 */
//--------------------------------------------------------------------------------------------------
#define HTTP_TIMEOUT_MS     30000

//--------------------------------------------------------------------------------------------------
/**
 * Structure used to parse an URI and package information
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    bool                isSecure;           ///< Protocol to be used: HTTP or HTTPS
    char*               hostPtr;            ///< Host
    char*               pathPtr;            ///< Package path
    uint32_t            packageSize;        ///< Package size
    uint32_t            downloadedBytes;    ///< Downloaded bytes
    uint32_t            range;              ///< Range for HTTP GET
    int                 httpCode;           ///< Last HTTP error code
    void*               opaquePtr;          ///< Opaque pointer
    uint16_t            port;               ///< Port number
}
PackageUriDetails_t;

//--------------------------------------------------------------------------------------------------
/**
 * Static structure for package details
 */
//--------------------------------------------------------------------------------------------------
static PackageUriDetails_t PackageUriDetails;

//--------------------------------------------------------------------------------------------------
/**
 * HTTP client session reference
 */
//--------------------------------------------------------------------------------------------------
static le_httpClient_Ref_t HttpClientRef;

//--------------------------------------------------------------------------------------------------
/**
 * Current download status.
 */
//--------------------------------------------------------------------------------------------------
static uint8_t DownloadStatus = DWL_OK;

//--------------------------------------------------------------------------------------------------
/**
 * Global value for last HTTP(S) error code.
 */
//--------------------------------------------------------------------------------------------------
static uint16_t HttpErrorCode = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Global value for HTTP client result
 */
//--------------------------------------------------------------------------------------------------
static le_result_t HttpClientResult = LE_OK;

//--------------------------------------------------------------------------------------------------
/**
 * Finalize current download
 */
//--------------------------------------------------------------------------------------------------
static void FinalizeDownload
(
    le_result_t status      ///< [IN] Package download final status
);

//--------------------------------------------------------------------------------------------------
/**
 *  Request a download retry
 */
//--------------------------------------------------------------------------------------------------
static void RequestDownloadRetry
(
    void* param1Ptr,     ///< [IN] Not used, should be NULL
    void* param2Ptr      ///< [IN] Not used, should be NULL
);

//--------------------------------------------------------------------------------------------------
/**
 * Convert an le_result_t status to downloaderResult_t
 *
 */
//--------------------------------------------------------------------------------------------------
static downloaderResult_t ConvertResult
(
    le_result_t status
)
{
    switch (status)
    {
        case LE_OK:
        case LE_DUPLICATE:
            return DOWNLOADER_OK;

        case LE_BAD_PARAMETER:
            return DOWNLOADER_INVALID_ARG;

        case LE_UNAVAILABLE:
            return DOWNLOADER_CONNECTION_ERROR;

        case LE_TIMEOUT:
            return DOWNLOADER_TIMEOUT;

        case LE_FAULT:
            return DOWNLOADER_RECV_ERROR;

        case LE_NO_MEMORY:
            return DOWNLOADER_MEMORY_ERROR;

        case LE_CLOSED:
            return DOWNLOADER_RECV_ERROR;

        case LE_COMM_ERROR:
            return DOWNLOADER_RECV_ERROR;

        case LE_FORMAT_ERROR:
            return DOWNLOADER_CERTIF_ERROR;

        default:
            return DOWNLOADER_ERROR;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert string to a long integer
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetLong
(
    char*        strPtr,         ///< [IN] String to be converted
    long*        valPtr          ///< [OUT] Conversion result
)
{
    long rc;

    if (!strPtr || !valPtr)
    {
        return LE_BAD_PARAMETER;
    }

    if ('\0' == *strPtr)
    {
        return LE_BAD_PARAMETER;
    }
    errno = 0;
    rc = strtoul(strPtr, NULL, BASE10);

    if (errno)
    {
        return LE_FAULT;
    }

    *valPtr = rc;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert string to port number
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetPortNumber
(
    char*        strPtr,          ////< [IN] String to be converted
    uint16_t*    valPtr           ///< [OUT] Conversion result
)
{
    long value;
    le_result_t status = GetLong(strPtr, &value);
    if (LE_OK != status)
    {
        return status;
    }

    if ((1 > value) || (USHRT_MAX < value))
    {
        return LE_FAULT;
    }

    *valPtr = (uint16_t)value;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert string to package size
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetPackageSize
(
    char*        strPtr,          ///< [IN] String to be converted
    uint32_t*    valPtr           ///< [OUT] Conversion result
)
{
    long value;
    le_result_t status = GetLong(strPtr, &value);
    if (LE_OK != status)
    {
        return status;
    }

    if ((0 > value) || (UINT_MAX < value))
    {
        return LE_FAULT;
    }

    *valPtr = (uint32_t)value;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Parse an URI and fill information in packageUriDetails parameter
 *
 * @return
 *  - true on success
 *  - false on failure
 */
//--------------------------------------------------------------------------------------------------
static bool ParsePackageURI
(
    char*                   packageUriPtr,          ///< [IN] Package URI
    PackageUriDetails_t*    packageUriDetailsPtr    ///< [INOUT] Package URI details
)
{
    char* tokenPtr;
    char* savedTokenPtr;
    char* subTokenPtr;
    char* subSavedTokenPtr;

    if ((!packageUriPtr) || (!packageUriDetailsPtr))
    {
        return false;
    }

    if (!strlen(packageUriPtr))
    {
        LE_ERROR("Empty URL");
        return false;
    }

    LE_DEBUG("Parse URL: packageUriPtr %s", packageUriPtr);

    /* Get the protocol */
    tokenPtr = strtok_r(packageUriPtr, ":", &savedTokenPtr);

    if (!tokenPtr)
    {
        return false;
    }

    // Check if the protocol is HTTP or HTTPS
    if (0 == strncasecmp(tokenPtr, HTTPS_PROTOCOL, strlen(HTTPS_PROTOCOL)))
    {
        packageUriDetailsPtr->isSecure = true;
    }
    else if (0 == strncasecmp(tokenPtr, HTTP_PROTOCOL, strlen(HTTP_PROTOCOL)))
    {
        packageUriDetailsPtr->isSecure = false;
    }
    else
    {
        LE_ERROR("ERROR in URI");
        return false;
    }

    // Get hostname
    tokenPtr = strtok_r(NULL, "/", &savedTokenPtr);
    if (!tokenPtr)
    {
        return false;
    }

    // Check if a specific port is selected
    subTokenPtr = strtok_r(tokenPtr, ":", &subSavedTokenPtr);
    if (!subTokenPtr)
    {
        return false;
    }
    packageUriDetailsPtr->hostPtr = subTokenPtr;

    // Get the port number if it exists, otherwise use default ones
    subTokenPtr = strtok_r(NULL, "/", &subSavedTokenPtr);
    if (!subTokenPtr)
    {
        LE_DEBUG("Port number is not provided so use http(s) default port");
        if(packageUriDetailsPtr->isSecure)
        {
            packageUriDetailsPtr->port = HTTPS_PORT;
        }
        else
        {
            packageUriDetailsPtr->port = HTTP_PORT;
        }
    }
    else
    {
        if (LE_OK != GetPortNumber(subTokenPtr, &(packageUriDetailsPtr->port)))
        {
            return false;
        }
        LE_DEBUG("Port number : %"PRIu16 "", packageUriDetailsPtr->port);
    }
    // Get path
    tokenPtr = strtok_r(NULL, "?", &savedTokenPtr);
    if (!tokenPtr)
    {
        return false;
    }

    packageUriDetailsPtr->pathPtr = tokenPtr;
    LE_DEBUG("pathPtr %s", packageUriDetailsPtr->pathPtr);

    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Callback to handle HTTP header response.
 */
//--------------------------------------------------------------------------------------------------
static void HeaderResponseCb
(
    le_httpClient_Ref_t ref,        ///< [IN] HTTP session context reference
    const char*         keyPtr,     ///< [IN] Key field in the HTTP header response
    int                 keyLen,     ///< [IN] Key field length
    const char*         valuePtr,   ///< [IN] Key value in the HTTP header response
    int                 valueLen    ///< [IN] Key value length
)
{
    LE_DEBUG("Key: %.*s, Value: %.*s", keyLen, keyPtr, valueLen, valuePtr);

    // Check if package size has already been retrieved from HTTP headers
    if (PackageUriDetails.packageSize)
    {
        return;
    }
    char tempBuffer[valueLen+1];
    memset(tempBuffer, 0, valueLen+1);
    strncpy(tempBuffer,valuePtr,(size_t)valueLen);
    if (!strncasecmp("content-length", keyPtr, keyLen))
    {
        if (LE_OK != GetPackageSize((char*)tempBuffer, &(PackageUriDetails.packageSize)))
        {
            LE_ERROR("Unable to retrieve package size");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Callback to handle HTTP body response.
 */
//--------------------------------------------------------------------------------------------------
static void BodyResponseCb
(
    le_httpClient_Ref_t ref,        ///< [IN] HTTP session context reference
    const char*         dataPtr,    ///< [IN] Received data pointer
    int                 size        ///< [IN] Received data size
)
{
    PackageUriDetails.downloadedBytes += (uint32_t)size;

    if (DWL_OK != lwm2mcore_PackageDownloaderReceiveData((uint8_t*)dataPtr,
                                                         (size_t)size,
                                                         PackageUriDetails.opaquePtr))
    {
        LE_ERROR("Error on treated received data");
        FinalizeDownload(LE_FORMAT_ERROR);
        return;
    }

    // Suspend or abort requested
    if (DWL_OK != downloader_GetDownloadStatus())
    {
        LE_INFO("Finalize download");
        FinalizeDownload(LE_OK);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Callback to handle resources (key/value pairs) insertion.
 *
 * @return
 *  - LE_OK            Callback should be called again to gather another key/value pair
 *  - LE_TERMINATED    All keys have been transmitted, do not recall callback
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ResourceUpdateCb
(
    le_httpClient_Ref_t ref,           ///< [IN] HTTP session context reference
    char*               keyPtr,        ///< [OUT] Key field pointer
    int*                keyLenPtr,     ///< [INOUT] Key field size
    char*               valuePtr,      ///< [OUT] Key value pointer
    int*                valueLenPtr    ///< [INOUT] Key value size
)
{
    if (!(PackageUriDetails.range))
    {
        *keyLenPtr = 0;
        *valueLenPtr = 0;
        return LE_TERMINATED;
    }

    LE_DEBUG("Resume download from range: %"PRIu32, PackageUriDetails.range);

    *keyLenPtr = snprintf(keyPtr, *keyLenPtr, "Range");
    *valueLenPtr = snprintf(valuePtr, *valueLenPtr, "bytes=%"PRIu32"-", PackageUriDetails.range);

    return LE_TERMINATED;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Callback to handle HTTP status code
 */
//--------------------------------------------------------------------------------------------------
static void StatusCodeCb
(
    le_httpClient_Ref_t ref,        ///< [IN] HTTP session context reference
    int                 code        ///< [IN] HTTP status code
)
{
    LE_DEBUG("HTTP status code: %d", code);

    PackageUriDetails.httpCode = code;
    HttpErrorCode = code;

    if ((HTTP_200 !=code) && (HTTP_206 !=code) && HttpClientRef)
    {
        // Remove the body callback: the body can be filled by a HTML page which explain the HTTP
        // error code
        le_httpClient_SetBodyResponseCallback(HttpClientRef, NULL);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Callback definition for le_httpClient_SendRequestAsync result value.
 */
//--------------------------------------------------------------------------------------------------
static void SendRequestRspCb
(
    le_httpClient_Ref_t ref,          ///< [IN] HTTP session context reference
    le_result_t         result        ///< [IN] Result value
)
{
    // Save HTTP client result
    HttpClientResult = result;

    if (result != LE_OK)
    {
        LE_ERROR("Failure during HTTP reception. Result: %d", result);
        // Failure during HTTP reception occurred. In this case, notify package downloader that no
        // data has been received and check its returned status
        return RequestDownloadRetry(NULL, NULL);
    }

    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_HandlePackageDownloader())
    {
        LE_ERROR("Package download failed");
        result = LE_FAULT;
    }

    FinalizeDownload(result);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Finalize download handler. Since this function deletes HTTP client context, it should not be
 * called from a HTTP client callback.
 */
//--------------------------------------------------------------------------------------------------
static void FinalizeDownloadHandler
(
    void* param1Ptr,     ///< [IN] Download final status
    void* param2Ptr      ///< [IN] Not used, should be NULL
)
{
    if (!param1Ptr)
    {
        LE_ERROR("NULL pointer provided");
        return;
    }

    le_result_t result = *(le_result_t*)param1Ptr;

    if (HttpClientRef)
    {
        le_httpClient_Delete(HttpClientRef);
        HttpClientRef = NULL;
    }

    packageDownloader_FinalizeDownload(result);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Request a download retry
 */
//--------------------------------------------------------------------------------------------------
static void RequestDownloadRetry
(
    void* param1Ptr,     ///< [IN] Download final status
    void* param2Ptr      ///< [IN] Not used, should be NULL
)
{
    lwm2mcore_Sid_t status;

    if (DWL_OK != downloader_GetDownloadStatus())
    {
        LE_INFO("Abort or Suspend requested");
        FinalizeDownload(HttpClientResult);
        return;
    }

    status = lwm2mcore_RequestDownloadRetry();
    switch (status)
    {
        case LWM2MCORE_ERR_COMPLETED_OK:
            LE_INFO("Package downloader is willing to retry download");
            break;

        case LWM2MCORE_ERR_RETRY_FAILED:
            LE_INFO("Last retry failed, request a new retry");
            le_event_QueueFunction(RequestDownloadRetry, NULL, NULL);
            break;

        default:
            LE_ERROR("Unable to request a download retry");
            FinalizeDownload(HttpClientResult);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize and start HTTP client
 *
 * @return
 *  - LE_OK            Function success
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_TIMEOUT       Timeout occurred during communication
 *  - LE_BUSY          Busy state machine
 *  - LE_UNAVAILABLE   Unable to reach the server or DNS issue
 *  - LE_FAULT         Internal error
 *  - LE_NO_MEMORY     Memory allocation issue
 *  - LE_CLOSED        In case of end of file error
 *  - LE_COMM_ERROR    Connection failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t StartHttpClient
(
    char*       packageUriPtr      ///< [IN] Package URI
)
{
    le_result_t status;

    // Check if HTTP client reference already exists.
    if (HttpClientRef)
    {
        le_httpClient_Delete(HttpClientRef);
        HttpClientRef = NULL;
    }

    memset(&PackageUriDetails, 0, sizeof(PackageUriDetails_t));

    // Parse the package URL
    if (ParsePackageURI(packageUriPtr, &PackageUriDetails))
    {
        LE_INFO("Package URL details:\n"
                "protocol \t%s\nhost \t\t%s\npath \t\t%s\nport \t\t%"PRIu16 "",
                 (true == PackageUriDetails.isSecure)?"HTTPS": "HTTP",
                 PackageUriDetails.hostPtr,
                 PackageUriDetails.pathPtr,
                 PackageUriDetails.port);
    }
    else
    {
        LE_ERROR("Error on package URL parsing");
        return LE_BAD_PARAMETER;
    }

    if ((!PackageUriDetails.pathPtr) || (!PackageUriDetails.hostPtr))
    {
        LE_ERROR("Error on URL parsing");
        return LE_BAD_PARAMETER;
    }

    HttpClientRef = le_httpClient_Create(PackageUriDetails.hostPtr, PackageUriDetails.port);
    if (!HttpClientRef)
    {
        LE_ERROR("Unable to create HTTP client");
        return LE_FAULT;
    }

    if (PackageUriDetails.isSecure)
    {
        status = le_httpClient_AddCertificate(HttpClientRef, DefaultDerKey, DEFAULT_DER_KEY_LEN);
        if (LE_OK != status)
        {
            LE_ERROR("Failed to add certificate");
            return status;
        }
    }

    le_httpClient_SetTimeout(HttpClientRef, HTTP_TIMEOUT_MS);

    // Setup callbacks
    le_httpClient_SetBodyResponseCallback(HttpClientRef, BodyResponseCb);
    le_httpClient_SetResourceUpdateCallback(HttpClientRef, ResourceUpdateCb);
    le_httpClient_SetHeaderResponseCallback(HttpClientRef, HeaderResponseCb);
    le_httpClient_SetStatusCodeCallback(HttpClientRef, StatusCodeCb);

    status = le_httpClient_Start(HttpClientRef);
    if (LE_UNAVAILABLE == status)
    {
        LE_ERROR("Unable to connect HTTP client, bad package URI");
        StatusCodeCb(HttpClientRef, HTTP_404);
        return status;
    }

    if (LE_OK != status)
    {
        LE_ERROR("Unable to connect HTTP client");
        return status;
    }

    HttpClientResult = status;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set download status
 */
//--------------------------------------------------------------------------------------------------
static void SetDownloadStatus
(
    lwm2mcore_DwlResult_t newDownloadStatus   ///< New download status to set
)
{
    DownloadStatus = newDownloadStatus;
}

//--------------------------------------------------------------------------------------------------
/**
 * Finalize current download
 */
//--------------------------------------------------------------------------------------------------
static void FinalizeDownload
(
    le_result_t status      ///< [IN] Package download final status
)
{
    static le_result_t finalResult;

    finalResult = status;
    le_event_QueueFunction(FinalizeDownloadHandler, (void*)(&finalResult), NULL);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get package size to be downloaded from the server
 *
 * @note
 * This function is not available if @c LWM2M_EXTERNAL_DOWNLOADER compilation flag is embedded
 *
 * @note
 * The client can call this function if it requested to know the package size before downloading it.
 *
 * @return
 *  - @ref DOWNLOADER_OK on success
 *  - @ref DOWNLOADER_INVALID_ARG when the package URL is not valid
 *  - @ref DOWNLOADER_CONNECTION_ERROR when the host can not be reached
 *  - @ref DOWNLOADER_RECV_ERROR when error occurs on data receipt
 *  - @ref DOWNLOADER_ERROR on failure
 *  - @ref DOWNLOADER_TIMEOUT if any timer expires for a LwM2MCore called function
 */
//--------------------------------------------------------------------------------------------------
downloaderResult_t downloader_GetPackageSize
(
    char*       packageUriPtr,      ///< [IN] Package URI
    uint64_t*   packageSizePtr      ///< [OUT] Package size
)
{
    le_result_t status;

    if ((!packageUriPtr) || (!packageSizePtr))
    {
        return ConvertResult(LE_BAD_PARAMETER);
    }

    SetDownloadStatus(DWL_OK);

    // Reset the last HTTP error code
    HttpErrorCode = 0;

    status = StartHttpClient(packageUriPtr);
    if (LE_OK != status)
    {
        LE_ERROR("Unable to start HTTP client, status %d", status);
        goto end;
    }

    LE_INFO("Sending a HTTP HEAD command on URI...");
    status = le_httpClient_SendRequest(HttpClientRef, HTTP_HEAD, PackageUriDetails.pathPtr);
    if (LE_OK != status)
    {
        LE_ERROR("Unable to send request");
        goto end;
    }

    // Even if the SendRequest API returns LE_OK, the HTTP code could be 404
    if (HTTP_200 == PackageUriDetails.httpCode)
    {
        *packageSizePtr = PackageUriDetails.packageSize;
    }
    else if ((HTTP_404 == PackageUriDetails.httpCode) || (HTTP_414 == PackageUriDetails.httpCode))
    {
        status = LE_BAD_PARAMETER;
    }
    else
    {
        status = LE_UNAVAILABLE;
    }

end:
    if (HttpClientRef)
    {
        le_httpClient_Delete(HttpClientRef);
        HttpClientRef = NULL;
    }

    return ConvertResult(status);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get download status
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t downloader_GetDownloadStatus
(
    void
)
{
    lwm2mcore_DwlResult_t currentDownloadStatus;

    currentDownloadStatus = DownloadStatus;

    return currentDownloadStatus;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start a package download in downloader
 *
 * This function is called in a dedicated thread/task.
 *
 * @return
 *  - @ref DOWNLOADER_OK on success
 *  - @ref DOWNLOADER_INVALID_ARG when the package URL is not valid
 *  - @ref DOWNLOADER_CONNECTION_ERROR when the host can not be reached
 *  - @ref DOWNLOADER_PARTIAL_FILE when partial file is received even if HTTP request succeeds
 *  - @ref DOWNLOADER_RECV_ERROR when error occurs on data receipt
 *  - @ref DOWNLOADER_ERROR on failure
 *  - @ref DOWNLOADER_TIMEOUT if any timer expires for a LwM2MCore called function
 */
//--------------------------------------------------------------------------------------------------
downloaderResult_t downloader_StartDownload
(
    char*                   packageUriPtr,  ///< [IN] Package URI
    uint64_t                offset,         ///< [IN] Offset for the download
    void*                   opaquePtr       ///< [IN] Opaque pointer
)
{
    le_result_t status;

    if (!packageUriPtr)
    {
        status = LE_BAD_PARAMETER;
        goto end;
    }

    SetDownloadStatus(DWL_OK);

    // Reset the last HTTP error code
    HttpErrorCode = 0;

    status = StartHttpClient(packageUriPtr);
    if (LE_OK != status)
    {
        LE_ERROR("Unable to start HTTP client");
        goto end;
    }

    status = le_httpClient_SetAsyncMode(HttpClientRef, true);
    if ((LE_OK != status) && (LE_DUPLICATE != status))
    {
        LE_ERROR("Unable to set asynchronous mode");
        goto end;
    }

    PackageUriDetails.opaquePtr = opaquePtr;
    PackageUriDetails.range = offset;

    LE_INFO("Sending a HTTP GET command on URI...");
    le_httpClient_SendRequestAsync(HttpClientRef,
                                   HTTP_GET,
                                   PackageUriDetails.pathPtr,
                                   SendRequestRspCb);
end:
    return ConvertResult(status);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get last downloader error
 *
 * This function is called in a dedicated thread/task.
 *
 * @note
 * This function is not available if @c LWM2M_EXTERNAL_DOWNLOADER compilation flag is embedded
 *
 * @note
 * This function is called when the downloader tries to download @c DWL_RETRIES time a package.
 *
 * @return
 *  - @ref DOWNLOADER_OK on success
 *  - @ref DOWNLOADER_INVALID_ARG when the package URL is not valid
 *  - @ref DOWNLOADER_CONNECTION_ERROR when the host can not be reached
 *  - @ref DOWNLOADER_PARTIAL_FILE when partial file is received even if HTTP request succeeds
 *  - @ref DOWNLOADER_RECV_ERROR when error occurs on data receipt
 *  - @ref DOWNLOADER_ERROR on failure
 *  - @ref DOWNLOADER_MEMORY_ERROR in case of memory allocation
 */
//--------------------------------------------------------------------------------------------------
downloaderResult_t downloader_GetLastDownloadError
(
    void
)
{
    return ConvertResult(HttpClientResult);
}

//--------------------------------------------------------------------------------------------------
/**
 * Abort current download
 */
//--------------------------------------------------------------------------------------------------
void downloader_AbortDownload
(
    void
)
{
    LE_INFO("Abort download, download status was %d", downloader_GetDownloadStatus());

    // Suspend ongoing download
    SetDownloadStatus(DWL_ABORTED);
}

//--------------------------------------------------------------------------------------------------
/**
 * Suspend current download
 */
//--------------------------------------------------------------------------------------------------
void downloader_SuspendDownload
(
    void
)
{
    LE_INFO("Suspend download, download status was %d", downloader_GetDownloadStatus());

    // Suspend ongoing download
    SetDownloadStatus(DWL_SUSPEND);
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if the current download should be aborted
 *
 * @return
 *      True    Download abort is requested
 *      False   Download can continue
 */
//--------------------------------------------------------------------------------------------------
bool downloader_CheckDownloadToAbort
(
    void
)
{
    if (DWL_ABORTED == downloader_GetDownloadStatus())
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if the current download should be suspended
 *
 * @return
 *      True    Download suspend is requested
 *      False   Download can continue
 */
//--------------------------------------------------------------------------------------------------
bool downloader_CheckDownloadToSuspend
(
    void
)
{
    if (DWL_SUSPEND == downloader_GetDownloadStatus())
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Function to get the last HTTP(S) error code on a package download.
 *
 * @remark Public function which can be called by the client.
 *
 * @note
 * This function is not available if @c LWM2M_EXTERNAL_DOWNLOADER compilation flag is embedded
 *
 * @note
 * If a package download error happens, this function could be called in order to get the last
 * HTTP(S) error code related to the package download after package URI retrieval from the server.
 * This function only concerns the package download.
 * The value is not persistent to reset.
 * If no package download was made, the error code is set to 0.
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK on success
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLastHttpErrorCode
(
    uint16_t*   errorCode       ///< [IN] HTTP(S) error code
)
{
    if (!errorCode)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *errorCode = HttpErrorCode;
    return LWM2MCORE_ERR_COMPLETED_OK;;
}