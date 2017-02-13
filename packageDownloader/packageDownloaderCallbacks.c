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
#include <osPortUpdate.h>
#include <lwm2mcorePackageDownloader.h>
#include <interfaces.h>
#include "packageDownloaderCallbacks.h"
#include "packageDownloader.h"

//--------------------------------------------------------------------------------------------------
/**
 * Value of 1 mebibyte in bytes
 */
//--------------------------------------------------------------------------------------------------
#define MEBIBYTE            (1 << 20)

//--------------------------------------------------------------------------------------------------
/**
 * Maximum in memory chunk size
 */
//--------------------------------------------------------------------------------------------------
#define MAX_DWL_SIZE        (4 * MEBIBYTE)

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
 * Download status
 */
//--------------------------------------------------------------------------------------------------
#define DWL_RESUME  0x00
#define DWL_PAUSE   0x01
#define DWL_ERROR   0x02

//--------------------------------------------------------------------------------------------------
/**
 * PackageInfo data structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    double  totalSize;              ///< total file size
    long    httpRespCode;           ///< http response code
    char    curlVersion[BUF_SIZE];  ///< libcurl version
}
PackageInfo_t;

//--------------------------------------------------------------------------------------------------
/**
 * Memory chunk data structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint8_t         mem[MAX_DWL_SIZE];  ///< downloaded data chunk
    size_t          size;               ///< downloaded data size
    size_t          offset;             ///< downloaded data chunks offset
    uint8_t         state;              ///< download state
    le_sem_Ref_t    resumeSem;          ///< resume semaphore
    le_sem_Ref_t    pauseSem;           ///< pause semaphore
}
Chunk_t;

//--------------------------------------------------------------------------------------------------
/**
 * Package data structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    CURL            *curlPtr;   ///< curl pointer
    int             fd;         ///< fifo file descriptor
    const char      *uriPtr;    ///< package URI pointer
    Chunk_t         chunk;      ///< memory chunk
    PackageInfo_t   pkgInfo;    ///< package information
}
Package_t;

//--------------------------------------------------------------------------------------------------
/**
 * Write downloaded data to memory chunk
 */
//--------------------------------------------------------------------------------------------------
static size_t Write
(
    void    *contentsPtr,
    size_t  size,
    size_t  nmemb,
    void    *streamPtr
)
{
    size_t count;
    Chunk_t *chunkPtr;
    chunkPtr = (Chunk_t *)streamPtr;

    count = size * nmemb;
    memcpy(chunkPtr->mem + chunkPtr->size, contentsPtr, count);
    chunkPtr->size += count;
    chunkPtr->mem[chunkPtr->size] = 0;

    return count;
}

//--------------------------------------------------------------------------------------------------
/**
 * Construct range string
 */
//--------------------------------------------------------------------------------------------------
static int CalculateRange
(
    char        *bufPtr,
    uint64_t    *offsetPtr,
    uint64_t    *sizePtr
)
{
    int ret;
    uint64_t lowLimit;
    uint64_t highLimit;

    lowLimit = *offsetPtr;

    if (MAX_DWL_SIZE > *sizePtr)
    {
        highLimit = (lowLimit + *sizePtr) - 1;
        *sizePtr = 0;
    }
    else
    {
        highLimit = (lowLimit + MAX_DWL_SIZE) - 1;
        *sizePtr -= MAX_DWL_SIZE;
        *offsetPtr += MAX_DWL_SIZE;
    }

    ret = snprintf(bufPtr, BUF_SIZE, "%llu-%llu",
                (unsigned long long) lowLimit,
                (unsigned long long) highLimit);

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
 * Get download information
 */
//--------------------------------------------------------------------------------------------------
static int GetDownloadInfo
(
    Package_t *pkgPtr
)
{
    CURLcode rc;
    PackageInfo_t *pkgInfoPtr;

    pkgInfoPtr = &pkgPtr->pkgInfo;

    // CURLOPT_WRITEFUNCTION will always succeed
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_WRITEFUNCTION, Write);

    // just get the header, will always succeed
    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_NOBODY, 1L);

    rc = curl_easy_perform(pkgPtr->curlPtr);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to perform curl request: %s", curl_easy_strerror(rc));
        return -1;
    }

    // check for a valid response
    rc = curl_easy_getinfo(pkgPtr->curlPtr, CURLINFO_RESPONSE_CODE,
            &pkgInfoPtr->httpRespCode);
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
 * Download thread function
 */
//--------------------------------------------------------------------------------------------------
static void *Download
(
    void *ctxPtr
)
{

    CURLcode rc;
    Package_t *pkgPtr;
    Chunk_t *chunkPtr;
    uint64_t totalSize;
    uint64_t offset;
    double dwlSize;
    char buf[BUF_SIZE];

    pkgPtr = (Package_t *)ctxPtr;
    chunkPtr = &pkgPtr->chunk;

    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_NOBODY, 0L);

    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_WRITEFUNCTION, Write);

    curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_WRITEDATA, (void *)chunkPtr);

    totalSize = (uint64_t)pkgPtr->pkgInfo.totalSize;
    offset = 0;
    dwlSize = 0;

    while (totalSize)
    {
        CalculateRange(buf, &offset, &totalSize);

        memset(chunkPtr->mem, 0, MAX_DWL_SIZE);
        chunkPtr->size = 0;

        curl_easy_setopt(pkgPtr->curlPtr, CURLOPT_RANGE, buf);

        rc = curl_easy_perform(pkgPtr->curlPtr);

        if(CURLE_OK != rc)
        {
            LE_ERROR("curl_easy_perform failed: %s", curl_easy_strerror(rc));
            chunkPtr->size = 0;
            chunkPtr->state = DWL_ERROR;
        }
        else
        {
            dwlSize += (double)chunkPtr->size;
            LE_DEBUG("last download: %g MiB", (double)chunkPtr->size / MEBIBYTE);
            LE_DEBUG("total download: %g MiB - %g%%", dwlSize / MEBIBYTE,
                (dwlSize / pkgPtr->pkgInfo.totalSize) * 100);
            chunkPtr->state = DWL_PAUSE;
        }

        le_sem_Post(chunkPtr->pauseSem);

        if (DWL_PAUSE == chunkPtr->state)
        {
            le_sem_Wait(chunkPtr->resumeSem);
        }
    }

    return (void *)0;
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
    static Package_t pkg;
    CURLcode rc;
    le_thread_Ref_t dwlRef;
    packageDownloader_DownloadCtx_t *dwlCtxPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)ctxPtr;

    pkg.curlPtr = NULL;
    pkg.fd = -1;

    dwlCtxPtr->ctxPtr = (void *)&pkg;

    LE_DEBUG("Initialize package downloader on `%s'", uriPtr);

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

    // set URL to get here
    rc= curl_easy_setopt(pkg.curlPtr, CURLOPT_URL, uriPtr);
    if (CURLE_OK != rc)
    {
        LE_ERROR("failed to set URI %s: %s", uriPtr, curl_easy_strerror(rc));
        return DWL_FAULT;
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

    if (-1 == GetDownloadInfo(&pkg))
    {
        return DWL_FAULT;
    }

    if (-1 == CheckHttpStatusCode(pkg.pkgInfo.httpRespCode))
    {
        return DWL_FAULT;
    }

    pkg.fd = open(dwlCtxPtr->fifoPtr, O_WRONLY);
    if (-1 == pkg.fd)
    {
        LE_ERROR("open fifo failed: %m");
        return DWL_FAULT;
    }

    pkg.uriPtr = uriPtr;

    pkg.chunk.size = 0;
    pkg.chunk.offset = 0;
    pkg.chunk.state = DWL_RESUME;

    pkg.chunk.resumeSem = le_sem_Create("Resume-Semaphore", 0);
    pkg.chunk.pauseSem = le_sem_Create("Pause-Semaphore", 0);

    dwlRef = le_thread_Create("Downloader", (void *)Download, (void *)&pkg);
    le_thread_Start(dwlRef);

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
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    Package_t *pkgPtr;
    PackageInfo_t *pkgInfoPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)ctxPtr;
    pkgPtr = (Package_t *)dwlCtxPtr->ctxPtr;
    pkgInfoPtr = &pkgPtr->pkgInfo;

    LE_DEBUG("using: %s", pkgInfoPtr->curlVersion);
    LE_DEBUG("connection status: %ld", pkgInfoPtr->httpRespCode);
    LE_DEBUG("package full size: %g MiB", pkgInfoPtr->totalSize / MEBIBYTE);

    dataPtr->packageSize = (uint64_t)pkgInfoPtr->totalSize;

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
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    Package_t *pkgPtr;
    Chunk_t *chunkPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)ctxPtr;
    pkgPtr = (Package_t *)dwlCtxPtr->ctxPtr;
    chunkPtr = &pkgPtr->chunk;

    if (DWL_RESUME == chunkPtr->state)
    {
        le_sem_Wait(chunkPtr->pauseSem);
    }

    if ( (!chunkPtr->size) || (DWL_ERROR == chunkPtr->state) )
    {
        return DWL_FAULT;
    }

    if (bufSize > chunkPtr->size)
    {
        bufSize = chunkPtr->size;
        chunkPtr->size = 0;
    }
    else
    {
        chunkPtr->size -= bufSize;
    }

    startOffset -= chunkPtr->offset * MAX_DWL_SIZE;

    *dwlLenPtr = bufSize;
     memcpy(bufPtr, chunkPtr->mem + startOffset, bufSize);

    if (!chunkPtr->size)
    {
        chunkPtr->offset++;
        chunkPtr->state = DWL_RESUME;
        le_sem_Post(chunkPtr->resumeSem);
    }

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
    Chunk_t *chunkPtr;

    dwlCtxPtr = (packageDownloader_DownloadCtx_t *)ctxPtr;
    pkgPtr = (Package_t *)dwlCtxPtr->ctxPtr;
    chunkPtr = &pkgPtr->chunk;

    curl_easy_cleanup(pkgPtr->curlPtr);

    curl_global_cleanup();

    if (close(pkgPtr->fd) == -1)
    {
        LE_ERROR("failed to close fd: %m");
        return DWL_FAULT;
    }

    le_sem_Delete(chunkPtr->pauseSem);
    le_sem_Delete(chunkPtr->resumeSem);

    return DWL_OK;
}
