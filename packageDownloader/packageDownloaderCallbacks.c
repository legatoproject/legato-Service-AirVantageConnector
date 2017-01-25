/**
 * @file packageDownloaderCallbacks.c
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <legato.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <curl/curl.h>
#include <osPortUpdate.h>
#include <lwm2mcorePackageDownloader.h>
#include <interfaces.h>
#include "packageDownloaderCallbacks.h"
#include "packageDownloader.h"

//--------------------------------------------------------------------------------------------------
/**
 * Curl connection to server timeout
 */
//--------------------------------------------------------------------------------------------------
#define CONNECTION_TIMEOUT  25L

//--------------------------------------------------------------------------------------------------
/**
 * HTTP status codes
 */
//--------------------------------------------------------------------------------------------------
#define NOT_FOUND               404
#define INTERNAL_SERVER_ERROR   500
#define BAD_GATEWAY             502
#define SERVICE_UNAVAILABLE     503

//--------------------------------------------------------------------------------------------------
/**
 * Max string buffer size
 */
//--------------------------------------------------------------------------------------------------
#define BUF_SIZE  512

//--------------------------------------------------------------------------------------------------
/**
 * Package data structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    CURL    *curlPtr;                       ///< curl pointer
    int     fd;                             ///< fifo file descriptor
    uint8_t chunk[MAX_DATA_BUFFER_CHUNK];   ///< downloaded data chunk
    char    range[BUF_SIZE];                ///< curl range string
    size_t  size;                           ///< downloaded data size
    size_t  offset;                         ///< downloaded data offset
}
Package_t;

//--------------------------------------------------------------------------------------------------
/**
 * Write downloaded data to chunk
 */
//--------------------------------------------------------------------------------------------------
static size_t WriteData
(
    void    *ptr,
    size_t  size,
    size_t  nmemb,
    void    *streamPtr
)
{
    Package_t *pkgPtr = (Package_t *)streamPtr;
    size_t count = size * nmemb;

    if (count > MAX_DATA_BUFFER_CHUNK)
    {
        LE_ERROR("read data size is higher than chunk max size");
        pkgPtr->size = 0;
        return 0;
    }

    memcpy(pkgPtr->chunk + pkgPtr->size, ptr, count);
    pkgPtr->size += count;

    return count;
}

//--------------------------------------------------------------------------------------------------
/**
 * Return the size of downloaded data
 */
//--------------------------------------------------------------------------------------------------
static size_t WriteDummy
(
    void    *ptr,
    size_t  size,
    size_t  nmemb,
    void    *streamPtr
)
{
    return (size * nmemb);
}

//--------------------------------------------------------------------------------------------------
/**
 * Construct range string
 */
//--------------------------------------------------------------------------------------------------
static int CurlRange
(
    size_t  size,
    size_t  offset,
    char    *rangePtr
)
{
    int ret;

    memset(rangePtr, 0, BUF_SIZE);
    ret = snprintf(rangePtr, BUF_SIZE, "%zu-%zu", offset, (offset + size) - 1);

    return ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check http status codes
 */
//--------------------------------------------------------------------------------------------------
static int CheckHttpStatusCode
(
    long code
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
 * InitDownload callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t InitDownload
(
    char *uriPtr,
    void *ctxPtr
)
{
    CURLcode rc;
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    static Package_t pkg;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)ctxPtr;

    pkg.curlPtr = NULL;
    pkg.fd = -1;

    dwlCtxPtr->ctxPtr = (void *)&pkg;

    pkg.fd = open(dwlCtxPtr->fifoPtr, O_WRONLY);
    if (-1 == pkg.fd)
    {
        LE_ERROR("open fifo failed: %m");
        return DWL_FAULT;
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

    LE_DEBUG("libcurl version %s", curl_version());

    // set URL to get here
    rc= curl_easy_setopt(pkg.curlPtr, CURLOPT_URL, uriPtr);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to set URI %s: %s", uriPtr, curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    // set the connection to server timeout
    curl_easy_setopt(pkg.curlPtr, CURLOPT_CONNECTTIMEOUT, CONNECTION_TIMEOUT);

    // set the tcp keep alive option
    rc = curl_easy_setopt(pkg.curlPtr, CURLOPT_TCP_KEEPALIVE, 1L);
    if (CURLE_OK != rc)
    {
        LE_ERROR("tcp keepalive option is not supported, libcurl version %s",
            curl_version());
    }

    // disable tls verification
    // TODO this is really bad, needs to be reactivated later
    rc= curl_easy_setopt(pkg.curlPtr, CURLOPT_SSL_VERIFYPEER, 0);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to disable peer's ssl certificate verification %s: %s",
            uriPtr, curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    rc= curl_easy_setopt(pkg.curlPtr, CURLOPT_SSL_VERIFYHOST, 0);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to disable peer's ssl certificate verification %s: %s",
            uriPtr, curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * GetInfo callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t GetInfo
(
    lwm2mcore_PackageDownloaderData_t   *dataPtr,
    void                                *ctxPtr
)
{
    CURLcode rc;
    double size;
    long code;
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    Package_t *pkgPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)ctxPtr;
    pkgPtr = (Package_t *)dwlCtxPtr->ctxPtr;

    // CURLOPT_WRITEFUNCTION will always succeed
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_WRITEFUNCTION, WriteDummy);

    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_NOBODY, 1L);

    rc = curl_easy_perform(pkgPtr->curlPtr);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to perform curl request: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    // check for a valid response
    rc = curl_easy_getinfo(pkgPtr->curlPtr, CURLINFO_RESPONSE_CODE, &code);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to get file info: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    if (-1 == CheckHttpStatusCode(code))
    {
        LE_ERROR("invalid response code %ld", code);
        return DWL_FAULT;
    }

    rc = curl_easy_getinfo(pkgPtr->curlPtr, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to get file info: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    dataPtr->packageSize = (uint64_t)size;

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * SetUpdateState callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t SetUpdateState
(
    lwm2mcore_fwUpdateState_t updateState
)
{
    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * SetUpdateResult callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t SetUpdateResult
(
    lwm2mcore_fwUpdateResult_t updateResult
)
{
    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * DownloadRange callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t DownloadRange
(
    uint8_t     *bufPtr,
    size_t      bufSize,
    uint64_t    startOffset,
    size_t      *dwlLenPtr,
    void        *ctxPtr
)
{
    int ret;
    CURLcode rc;
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    Package_t *pkgPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)ctxPtr;
    pkgPtr = (Package_t *)dwlCtxPtr->ctxPtr;

    memset(pkgPtr->range, 0, BUF_SIZE);

    ret = CurlRange(bufSize, startOffset, pkgPtr->range);
    if (BUF_SIZE <= ret)
    {
        return DWL_FAULT;
    }

    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_NOBODY, 0L);

    memset(pkgPtr->chunk, 0, MAX_DATA_BUFFER_CHUNK);
    pkgPtr->size = 0;
    pkgPtr->offset = 0;

    rc = curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_RANGE, pkgPtr->range);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to set curl range: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    // will always succeed
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_WRITEFUNCTION, WriteData);

    // will always succeed
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_WRITEDATA, (void *)pkgPtr);

    rc = curl_easy_perform(pkgPtr->curlPtr);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to perform curl request: %s", curl_easy_strerror(rc));
        return DWL_FAULT;
    }

    *dwlLenPtr = pkgPtr->size;
    memcpy(bufPtr, pkgPtr->chunk, pkgPtr->size);

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * StoreRange callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t StoreRange
(
    uint8_t *bufPtr,
    size_t bufSize,
    uint64_t offset,
    void *ctxPtr
)
{
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    Package_t *pkgPtr;
    ssize_t count;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)ctxPtr;
    pkgPtr = (Package_t *)dwlCtxPtr->ctxPtr;

    count = write(pkgPtr->fd, bufPtr, bufSize);

    if (-1 == count)
    {
        LE_ERROR("failed to write to fifo: %m");
        return DWL_FAULT;
    }

    if (bufSize > count)
    {
        LE_ERROR("failed to write data: size %zu, count %zd", bufSize, count);
        return DWL_FAULT;
    }

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * EndDownload callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t EndDownload
(
    void *ctxPtr
)
{
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    Package_t *pkgPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)ctxPtr;
    pkgPtr = (Package_t *)dwlCtxPtr->ctxPtr;

    curl_easy_cleanup(pkgPtr->curlPtr);

    curl_global_cleanup();

    if (close(pkgPtr->fd) == -1)
    {
        LE_ERROR("failed to close fd: %m");
        return DWL_FAULT;
    }

    return DWL_OK;
}
