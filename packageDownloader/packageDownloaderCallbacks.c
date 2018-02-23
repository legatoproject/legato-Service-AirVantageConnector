/**
 * @file packageDownloaderCallbacks.c
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <legato.h>
#include <curl/curl.h>
#include <lwm2mcore/update.h>
#include <lwm2mcorePackageDownloader.h>
#include <interfaces.h>
#include <avcFs.h>
#include <avcFsConfig.h>
#include "packageDownloader.h"
#include "packageDownloaderCallbacks.h"
#include "avcServer.h"
#include "file.h"

//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of bits an unsigned integer can hold
 */
//--------------------------------------------------------------------------------------------------
#define UINT_MAX_BITS                   32

//--------------------------------------------------------------------------------------------------
/**
 * Number of milliseconds in a second
 */
//--------------------------------------------------------------------------------------------------
#define SECS_TO_MSECS                   1000

//--------------------------------------------------------------------------------------------------
/**
 * Value of 1 mebibyte in bytes
 */
//--------------------------------------------------------------------------------------------------
#define MEBIBYTE                        (1 << 20)

//--------------------------------------------------------------------------------------------------
/**
 * Curl minimum download speed (expect to download atleast 100 bytes per second)
 */
//--------------------------------------------------------------------------------------------------
#define CURL_MINIMUM_SPEED              100L

//--------------------------------------------------------------------------------------------------
/**
 * Curl timeout in seconds. Timeout, if the download speed is less than CURL_MINIMUM_SPEED for
 * more than CURL_TIMEOUT_SECONDS.
 */
//--------------------------------------------------------------------------------------------------
#define CURL_TIMEOUT_SECONDS            350L

//--------------------------------------------------------------------------------------------------
/**
 * Curl connection timeout (Maximum time curl can spend during connection phase)
 */
//--------------------------------------------------------------------------------------------------
#define CURL_CONNECT_TIMEOUT_SECONDS    300L

//--------------------------------------------------------------------------------------------------
/**
 * Number of download retries in case an error occurs
 */
//--------------------------------------------------------------------------------------------------
#define DWL_RETRIES                     5

//--------------------------------------------------------------------------------------------------
/**
 * HTTP status codes
 */
//--------------------------------------------------------------------------------------------------
#define NOT_FOUND                       404
#define INTERNAL_SERVER_ERROR           500
#define BAD_GATEWAY                     502
#define SERVICE_UNAVAILABLE             503

//--------------------------------------------------------------------------------------------------
/**
 * Max string buffer size
 */
//--------------------------------------------------------------------------------------------------
#define BUF_SIZE                        512

//--------------------------------------------------------------------------------------------------
/**
 * PackageInfo data structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    double  totalSize;              ///< total file size
    long    httpRespCode;           ///< HTTP response code
    char    curlVersion[BUF_SIZE];  ///< libcurl version
}
PackageInfo_t;

//--------------------------------------------------------------------------------------------------
/**
 * Package data structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    CURL*                   curlPtr;    ///< curl pointer
    const char*             uriPtr;     ///< package URI pointer
    PackageInfo_t           pkgInfo;    ///< package information
    size_t                  size;       ///< package current size
    lwm2mcore_DwlResult_t   result;     ///< download result
}
Package_t;

//--------------------------------------------------------------------------------------------------
/**
 * HTTP response code
 */
//--------------------------------------------------------------------------------------------------
static long HttpRespCode = LE_AVC_HTTP_STATUS_INVALID;

//--------------------------------------------------------------------------------------------------
/**
 * Send downloaded data to the package downloader
 *
  * @return
 *      Number of bytes processed by this callback
 */
//--------------------------------------------------------------------------------------------------
static size_t Write
(
    void*   contentsPtr,    ///< [IN] Pointer to delivered data
    size_t  size,           ///< [IN] Nominal size of the delivered data
    size_t  nmemb,          ///< [IN] Number of nominal elements in the delivered data
    void*   contextPtr      ///< [IN] Context pointer
)
{
    size_t count = size * nmemb;
    Package_t *pkgPtr = (Package_t *)contextPtr;

    pkgPtr->result = DWL_FAULT;

    // Process the downloaded data
    if (DWL_OK != lwm2mcore_PackageDownloaderReceiveData(contentsPtr, count))
    {
        LE_ERROR("Data processing stopped by DWL parser");
        return 0;
    }

    if (count)
    {
        pkgPtr->result = DWL_OK;
    }

    pkgPtr->size += count;

    return count;
}

//--------------------------------------------------------------------------------------------------
/**
 * Progress callback
 *
 * @return
 *      - 0 if nothing to report
 *      - non-zero value to abort the download
 */
//--------------------------------------------------------------------------------------------------
static int Progress
(
    void* contextPtr,       ///< [IN] Context pointer
    curl_off_t dlTotal,     ///< [IN] Total number of bytes to download
    curl_off_t dlNow,       ///< [IN] Number of bytes downloaded so far
    curl_off_t ulTotal,     ///< [IN] Total number of bytes to upload
    curl_off_t ulNow        ///< [IN] Number of bytes uploaded so far
)
{
    Package_t *pkgPtr = (Package_t *)contextPtr;

    // Check if the download should be aborted
    if (true == packageDownloader_CheckDownloadToAbort())
    {
        LE_INFO("Download aborted");
        pkgPtr->result = DWL_ABORTED;
        return 1;
    }

    // Check if the download should be suspended
    if (true == packageDownloader_CheckDownloadToSuspend())
    {
        LE_INFO("Download suspended");
        pkgPtr->result = DWL_SUSPEND;
        return 1;
    }

    return 0;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check HTTP status codes
 *
 * @return
 *      - 0 if the HTTP code doesn't indicates an error
 *      - -1 otherwise
 */
//--------------------------------------------------------------------------------------------------
static int CheckHttpStatusCode
(
    long code   ///< [IN] HTTP response code
)
{
    int ret = 0;

    switch (code)
    {
        case NOT_FOUND:
            LE_DEBUG("404 - NOT FOUND");
            ret = -1;
            break;
        case INTERNAL_SERVER_ERROR:
            LE_DEBUG("500 - INTERNAL SERVER ERROR");
            ret = -1;
            break;
        case BAD_GATEWAY:
            LE_DEBUG("502 - BAD GATEWAY");
            ret = -1;
            break;
        case SERVICE_UNAVAILABLE:
            LE_DEBUG("503 - SERVICE UNAVAILABLE");
            ret = -1;
            break;
        default: break;
    }

    return ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get download information
 *
 * @return
 *      - 0 if the function succeeded
 *      - -1 if the function failed
 */
//--------------------------------------------------------------------------------------------------
static int GetDownloadInfo
(
    Package_t* pkgPtr   ///< [IN] Package data pointer
)
{
    CURLcode rc;
    PackageInfo_t* pkgInfoPtr;

    pkgInfoPtr = &pkgPtr->pkgInfo;

    // just get the header, will always succeed
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_XFERINFOFUNCTION, Progress);
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_XFERINFODATA, (void *)pkgPtr);
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_NOPROGRESS, 0L);

    // perform the download operation
    rc = curl_easy_perform(pkgPtr->curlPtr);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to perform curl request: %s", curl_easy_strerror(rc));
        return -1;
    }

    // check for a valid response
    rc = curl_easy_getinfo(pkgPtr->curlPtr, CURLINFO_RESPONSE_CODE, &pkgInfoPtr->httpRespCode);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to get response code: %s", curl_easy_strerror(rc));
        return -1;
    }

    rc = curl_easy_getinfo(pkgPtr->curlPtr, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                           &pkgInfoPtr->totalSize);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to get file size: %s", curl_easy_strerror(rc));
        return -1;
    }

    memset(pkgInfoPtr->curlVersion, 0, BUF_SIZE);
    memcpy(pkgInfoPtr->curlVersion, curl_version(), BUF_SIZE);

    return 0;
}

//--------------------------------------------------------------------------------------------------
/**
 * A simple function to calculate the power of an unsigned integer to an unsigned integer
 */
//--------------------------------------------------------------------------------------------------
static unsigned Power
(
    unsigned base,
    unsigned exponent
)
{
    unsigned result = 1;

    if (UINT_MAX_BITS <= exponent)
    {
        return UINT_MAX;
    }

    while (exponent--)
    {
        result *= base;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function waits for s seconds before returning
 */
//--------------------------------------------------------------------------------------------------
static void Wait
(
    unsigned s
)
{
    int rc;
    struct timespec req, rem;

    req.tv_sec = s;
    req.tv_nsec = 0;

    do
    {
        LE_DEBUG("waiting for %lds, %ldns", req.tv_sec, req.tv_nsec);
        rc = nanosleep(&req, &rem);
        if (-1 == rc)
        {
            if (EINTR != errno)
            {
                LE_ERROR("nanosleep(): %m");
                return;
            }
            LE_DEBUG("remaining time %lds, %ldns\n", rem.tv_sec, rem.tv_nsec);
            req = rem;
        }
    } while (rc);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get package download HTTP response code
 *
 * @return
 *      - HTTP response code            The function succeeded
 *      - LE_AVC_HTTP_STATUS_INVALID    The function failed
 */
//--------------------------------------------------------------------------------------------------
uint16_t pkgDwlCb_GetHttpStatus
(
    void
)
{
    return (uint16_t)HttpRespCode;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get update package size
 *
 * @return
 *      - LE_OK             The function succeeded
 *      - LE_BAD_PARAMETER  A parameter is not correct
 *      - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t pkgDwlCb_GetPackageSize
(
    char* packageUri,           ///< [IN]  Update package URI
    uint64_t* packageSizePtr    ///< [OUT] Update package size
)
{
    CURLcode rc;
    CURL* curlPtr;
    double packageSize;

    if ((!packageUri) || ('\0' == packageUri[0]))
    {
        LE_ERROR("Incorrect package URI");
        return LE_BAD_PARAMETER;
    }

    if (!packageSizePtr)
    {
        LE_ERROR("packageSizePtr is NULL");
        return LE_BAD_PARAMETER;
    }

    *packageSizePtr = 0;

    // Initialize everything possible
    rc = curl_global_init(CURL_GLOBAL_ALL);
    if (CURLE_OK != rc)
    {
        LE_ERROR("Failed to initialize libcurl: %s", curl_easy_strerror(rc));
        return LE_FAULT;
    }

    // Initialize the curl session
    curlPtr = curl_easy_init();
    if (!curlPtr)
    {
        LE_ERROR("Failed to initialize the curl session");
        goto global_cleanup;
    }

    // Set the timeout for connection phase
    rc = curl_easy_setopt(curlPtr, CURLOPT_CONNECTTIMEOUT, CURL_CONNECT_TIMEOUT_SECONDS);
    if (CURLE_OK != rc)
    {
        LE_ERROR("Failed to set curl connection timeout: %s", curl_easy_strerror(rc));
        goto easy_cleanup;
    }

    // Set URL to get here
    rc= curl_easy_setopt(curlPtr, CURLOPT_URL, packageUri);
    if (CURLE_OK != rc)
    {
        LE_ERROR("Failed to set URI: %s", curl_easy_strerror(rc));
        goto easy_cleanup;
    }

    // Set the path to CA bundle
    if (file_Exists(PEMCERT_PATH))
    {
        rc= curl_easy_setopt(curlPtr, CURLOPT_CAINFO, PEMCERT_PATH);
        if (CURLE_OK != rc)
        {
            LE_ERROR("Failed to set CA path: %s", curl_easy_strerror(rc));
            goto easy_cleanup;
        }
    }

    rc= curl_easy_setopt(curlPtr, CURLOPT_CAPATH, ROOTCERT_PATH);
    if (CURLE_OK != rc)
    {
        LE_ERROR("Failed to set CA path: %s", curl_easy_strerror(rc));
        goto easy_cleanup;
    }

    // Just get the header, will always succeed
    curl_easy_setopt(curlPtr, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curlPtr, CURLOPT_WRITEFUNCTION, NULL);

    // Retrieve the header
    rc = curl_easy_perform(curlPtr);
    if (CURLE_OK != rc)
    {
        LE_ERROR("Failed to perform curl request: %s", curl_easy_strerror(rc));
        goto easy_cleanup;
    }

    rc = curl_easy_getinfo(curlPtr, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &packageSize);
    if (CURLE_OK != rc)
    {
        LE_ERROR("Failed to get file size: %s", curl_easy_strerror(rc));
        goto easy_cleanup;
    }

    curl_easy_cleanup(curlPtr);
    curl_global_cleanup();

    *packageSizePtr = (uint64_t)packageSize;
    packageDownloader_SetUpdatePackageSize(packageSize);
    return LE_OK;

easy_cleanup:
    curl_easy_cleanup(curlPtr);
global_cleanup:
    curl_global_cleanup();
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize download callback function
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_InitDownload
(
    char* uriPtr,   ///< [IN] Package URI
    void* ctxPtr    ///< [IN] Context pointer
)
{
    static Package_t pkg;
    CURLcode rc;
    packageDownloader_DownloadCtx_t* dwlCtxPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t*)ctxPtr;

    pkg.curlPtr = NULL;

    dwlCtxPtr->ctxPtr = (void *)&pkg;

    LE_DEBUG("Initialize package downloader");

    // Check as soon as possible if download should be aborted
    if (true == packageDownloader_CheckDownloadToAbort())
    {
        LE_INFO("Download aborted");
        return DWL_ABORTED;
    }

    // initialize everything possible
    rc = curl_global_init(CURL_GLOBAL_ALL);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to initialize libcurl: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    // init the curl session
    pkg.curlPtr = curl_easy_init();
    if (!pkg.curlPtr)
    {
        LE_ERROR("failed to initialize the curl session");
        return DWL_FAULT;
    }

    // set the timeout for connection phase
    rc = curl_easy_setopt(pkg.curlPtr, CURLOPT_CONNECTTIMEOUT, CURL_CONNECT_TIMEOUT_SECONDS);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to set curl connection timeout: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    // set timeout for the download speed to be very low
    rc = curl_easy_setopt(pkg.curlPtr, CURLOPT_LOW_SPEED_TIME, CURL_TIMEOUT_SECONDS);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to set curl timeout: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    // set the minimum download speed expected. If the download speed continuous to
    // be less than CURL_MINIMUM_SPEED for more than CURL_TIMEOUT_SECONDS, curl
    // will timeout
    rc = curl_easy_setopt(pkg.curlPtr, CURLOPT_LOW_SPEED_LIMIT, CURL_MINIMUM_SPEED);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to set curl download speed limit: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    // set URL to get here
    rc= curl_easy_setopt(pkg.curlPtr, CURLOPT_URL, uriPtr);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to set URI: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    // set the path to CA bundle
    if (file_Exists(dwlCtxPtr->certPtr))
    {
        rc= curl_easy_setopt(pkg.curlPtr, CURLOPT_CAINFO, dwlCtxPtr->certPtr);
        if (CURLE_OK != rc)
        {
            LE_ERROR("failed to set CA path: %s", curl_easy_strerror(rc));
            return DWL_FAULT;
        }
    }

    rc= curl_easy_setopt(pkg.curlPtr, CURLOPT_CAPATH, ROOTCERT_PATH);
    if (CURLE_OK != rc)
    {
        LE_ERROR("Failed to set CA path: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    if (-1 == GetDownloadInfo(&pkg))
    {
        if ((DWL_ABORTED == pkg.result) || (DWL_SUSPEND == pkg.result))
        {
            return pkg.result;
        }
        else
        {
            return DWL_FAULT;
        }
    }

    if (-1 == CheckHttpStatusCode(pkg.pkgInfo.httpRespCode))
    {
        LE_ERROR("HTTP error %ld", pkg.pkgInfo.httpRespCode);
        return DWL_FAULT;
    }

    pkg.uriPtr = uriPtr;

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get package information callback function
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_GetInfo
(
    lwm2mcore_PackageDownloaderData_t* dataPtr, ///< [IN] Package downloader data pointer
    void*                              ctxPtr   ///< [IN] Context pointer
)
{
    packageDownloader_DownloadCtx_t* dwlCtxPtr;
    Package_t* pkgPtr;
    PackageInfo_t* pkgInfoPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t*)ctxPtr;
    pkgPtr = (Package_t*)dwlCtxPtr->ctxPtr;
    pkgInfoPtr = &pkgPtr->pkgInfo;

    LE_DEBUG("using: %s", pkgInfoPtr->curlVersion);
    LE_DEBUG("connection status: %ld", pkgInfoPtr->httpRespCode);
    LE_DEBUG("package full size: %g MiB", pkgInfoPtr->totalSize / MEBIBYTE);
    LE_DEBUG("updateType: %d", dataPtr->updateType);

    // Check as soon as possible if download should be aborted
    if (true == packageDownloader_CheckDownloadToAbort())
    {
        LE_INFO("Download aborted");
        return DWL_ABORTED;
    }

    dataPtr->packageSize = (uint64_t)pkgInfoPtr->totalSize;
    if(LWM2MCORE_FW_UPDATE_TYPE == dataPtr->updateType)
    {
        LE_INFO("FW update type");
        packageDownloader_SetUpdatePackageSize(dataPtr->packageSize);
    }
    else if (LWM2MCORE_SW_UPDATE_TYPE == dataPtr->updateType)
    {
        LE_INFO("SW update type");
        packageDownloader_SetUpdatePackageSize(dataPtr->packageSize);
    }
    else
    {
        LE_ERROR("incorrect update type");
    }

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Download callback function
 *
 * This implements a HTTP/S download starting at startOffset.
 *
 * In case of a proxy resolving issue, a host resolving issue, a failure to connect, or an operation
 * timeout it will retry for DWL_RETRIES and exit with DWL_FAIL in case of an unsuccessful retry
 * otherwise it reinitializes the retry count, continues to download and returns DWL_OK when done.
 *
 * Each time an issue happens it will wait for 2^(retry-1) seconds before retrying to download
 * e.g:
 * first attempt: it'll wait for 2^0 = 1 second
 * second attempt: it'll wait for 2^1 = 2 seconds
 * third attempt: it'll wait for 2^2 = 4 seconds
 * ...
 * In the case of a successful retry, the count is reinitialized.
 * e.g.:
 * first attempt: wait for 1 second, retry failed
 * second attempt: wait for 2 seconds, retry failed
 * third attempt: wait for 4 seconds, retry succeeded
 * sometime later the download fails again
 * first attempt: wait for 1 second ...
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_SUSPENDED The download is suspended
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_Download
(
    uint64_t    startOffset,    ///< [IN] Start offset for the download
    void*       ctxPtr          ///< [IN] Context pointer
)
{
    packageDownloader_DownloadCtx_t* dwlCtxPtr;
    Package_t* pkgPtr;
    CURLcode rc;
    int retry = 0;
    long osErrno;
    char buf[BUF_SIZE];
    size_t size = 0;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t*)ctxPtr;
    pkgPtr = (Package_t*)dwlCtxPtr->ctxPtr;

    pkgPtr->size = 0;

    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_WRITEFUNCTION, Write);
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_WRITEDATA, (void *)pkgPtr);
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_XFERINFOFUNCTION, Progress);
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_XFERINFODATA, (void *)pkgPtr);
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_NOPROGRESS, 0L);

    // Start download at offset given by startOffset
    if (startOffset)
    {
        memset(buf, 0, BUF_SIZE);
        snprintf(buf, BUF_SIZE, "%llu-", (unsigned long long)startOffset);
        curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_RANGE, buf);
        pkgPtr->size = (size_t)startOffset;
    }

    while (retry < DWL_RETRIES)
    {
        LE_INFO("attempt %d", retry);
        // perform download operation
        rc = curl_easy_perform(pkgPtr->curlPtr);
        switch (rc)
        {
            case CURLE_OK:
            case CURLE_ABORTED_BY_CALLBACK:
                retry = DWL_RETRIES;
                break;
            case CURLE_COULDNT_RESOLVE_PROXY:
            case CURLE_COULDNT_RESOLVE_HOST:
            case CURLE_OPERATION_TIMEDOUT:
            case CURLE_RECV_ERROR:
            case CURLE_PARTIAL_FILE:
                LE_DEBUG("Received error: %s", curl_easy_strerror(rc));
                retry++;
                break;
            case CURLE_COULDNT_CONNECT:
                curl_easy_getinfo(pkgPtr->curlPtr, CURLINFO_OS_ERRNO, &osErrno);
                (ECONNREFUSED == osErrno)?retry++:(retry = DWL_RETRIES);
                break;
            default:
                LE_ERROR("failed to perform curl request: %s", curl_easy_strerror(rc));
                retry = DWL_RETRIES;
                break;
        }

        if (DWL_RETRIES > retry)
        {
            LE_ERROR("failed to perform curl request: %s", curl_easy_strerror(rc));
            if (size != pkgPtr->size)
            {
                retry = 1;
            }
            size = pkgPtr->size;
            memset(buf, 0, BUF_SIZE);
            snprintf(buf, BUF_SIZE, "%zu-", pkgPtr->size);
            curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_RANGE, buf);
            Wait(Power(2, retry - 1));
        }

        rc = curl_easy_getinfo(pkgPtr->curlPtr, CURLINFO_RESPONSE_CODE, &HttpRespCode);
        if (CURLE_OK != rc)
        {
            LE_WARN("failed to get response code: %s", curl_easy_strerror(rc));
        }
    }

    return pkgPtr->result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Store range callback function
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_StoreRange
(
    uint8_t* bufPtr,    ///< [IN] Pointer on data to store
    size_t   bufSize,   ///< [IN] Size of the data to store
    void*    ctxPtr     ///< [IN] Context pointer
)
{
    packageDownloader_DownloadCtx_t* dwlCtxPtr;
    ssize_t count;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t*)ctxPtr;

    count = write(dwlCtxPtr->downloadFd, bufPtr, bufSize);

    if (-1 == count)
    {
        // Check if the error is not caused by an error in the FW update process,
        // which would have closed the pipe.
        if ((errno == EPIPE) && (true == packageDownloader_CheckDownloadToAbort()))
        {
            LE_WARN("Download aborted by FW update process");
            // No error returned, the package downloader will be stopped
            // through the progress callback.
            return DWL_OK;
        }
        else
        {
            LE_ERROR("Failed to write to fifo: %m");
            return DWL_FAULT;
        }
    }

    if (bufSize > count)
    {
        LE_ERROR("Failed to write data: size %zu, count %zd", bufSize, count);
        return DWL_FAULT;
    }

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * End download callback function
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_EndDownload
(
    void* ctxPtr    ///< [IN] Context pointer
)
{
    packageDownloader_DownloadCtx_t* dwlCtxPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t*)ctxPtr;

    // Clean up the curl context only if it was previously set
    if (NULL != dwlCtxPtr->ctxPtr)
    {
        Package_t* pkgPtr;
        pkgPtr = (Package_t*)dwlCtxPtr->ctxPtr;
        curl_easy_cleanup(pkgPtr->curlPtr);
    }

    curl_global_cleanup();

    return DWL_OK;
}
