/**
 * @file curl.c
 *
 * This component is used to stub the curl library in order to test the package Downloader
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
#include <curl/curl.h>
#include <lwm2mcore/update.h>
#include <stdarg.h>
#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * This is a return code for the read callback that, when returned, will
 * signal libcurl to pause sending data on the current transfer.
 */
//--------------------------------------------------------------------------------------------------
#define CURL_READFUNC_PAUSE 0x10000001

//--------------------------------------------------------------------------------------------------
/**
 * Memory pool size
 */
//--------------------------------------------------------------------------------------------------
#define CURL_POOL_SIZE   2

//--------------------------------------------------------------------------------------------------
/**
 * Curl callback prototype definition
 */
//--------------------------------------------------------------------------------------------------
typedef size_t (*callback)(void*, size_t, size_t, void*);

//--------------------------------------------------------------------------------------------------
/**
 * Curl handler structure definition
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    CURL* handle;                               ///< Curl session handler
    long flags;                                 ///< Option flags
    int cnxTimeout;                             ///< Timeout for the connect phase
    int lowSpeedTime;                           ///< Time to be below the speed to trigger abort.
    int lowSpeedLimit;                          ///< Low speed limit to abort transfer
    callback writeFnc;                          ///< User callback
    bool noBody;                                ///< Sends the header without the body
    int dataOffset;                             ///< Data offset to resume a transfert
    void* contextPtr;                           ///< User context pointer
    char url[LWM2MCORE_PACKAGE_URI_MAX_BYTES];  ///< Download path
}
CurlTestHandler_t;

//--------------------------------------------------------------------------------------------------
/**
 * Curl handler
 */
//--------------------------------------------------------------------------------------------------
static CurlTestHandler_t* CurlTestHandler = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Curl pool reference
 */
//--------------------------------------------------------------------------------------------------
le_mem_PoolRef_t CurlPool = NULL;

//==================================================================================================
//                                       Public API Functions
//==================================================================================================

//--------------------------------------------------------------------------------------------------
/**
 * Set up the program environment that libcurl needs.
 */
//--------------------------------------------------------------------------------------------------
CURLcode curl_global_init
(
    long flags
)
{
    LE_DEBUG("Stub");

    if (NULL == CurlPool)
    {
        // Create and expand Curl memory pool
        CurlPool = le_mem_CreatePool("Curl Pool", sizeof(CurlTestHandler_t));
        le_mem_ExpandPool(CurlPool, CURL_POOL_SIZE);
    }

    // Malloc the handler structure
    CurlTestHandler = le_mem_TryAlloc(CurlPool);
    if (NULL == CurlTestHandler)
    {
        return CURLE_FAILED_INIT;
    }

    // Assign the global init flags
    CurlTestHandler->flags = flags;

    return CURLE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize and returns a CURL easy handle that should be used as input to other functions in
 * the easy interface.
 */
//--------------------------------------------------------------------------------------------------
CURL *curl_easy_init
(
    void
)
{
    LE_DEBUG("Stub");

    // Zero-initialize the strucure and set a dummy handler
    memset(CurlTestHandler, 0, sizeof(CurlTestHandler_t));
    CurlTestHandler->handle = (CURL* )le_rand_GetNumBetween(0, 10000008);

    return CurlTestHandler->handle;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set options for libCurl
 */
//--------------------------------------------------------------------------------------------------
CURLcode curl_easy_setopt
(
    CURL *handle,
    CURLoption option,
    ...
)
{
    va_list arg;
    void *paramPtr;

    if (NULL == CurlTestHandler)
    {
      return CURLE_FAILED_INIT;
    }

    va_start(arg, option);
    paramPtr = va_arg(arg, void*);

    switch (option)
    {
        case CURLOPT_URL:
        {
            // Get the size of the URL
            size_t urlSize = strlen((const char*)paramPtr);
            if ((0 == urlSize) || (LWM2MCORE_PACKAGE_URI_MAX_BYTES <= urlSize))
            {
                va_end(arg);
                return CURLE_URL_MALFORMAT;
            }

            // Copy the new URL
            memcpy(CurlTestHandler->url,(const char*)paramPtr, urlSize);
            CurlTestHandler->url[urlSize] = '\0';

            // Check if we have acess to this URL (It needs to be local)
            struct stat buffer;
            if (stat(CurlTestHandler->url, &buffer) != 0)
            {
                va_end(arg);
                return CURLE_READ_ERROR;
            }
        }
        break;

        case CURLOPT_WRITEFUNCTION:
            CurlTestHandler->writeFnc = paramPtr;
            break;

        case CURLOPT_WRITEDATA:
            CurlTestHandler->contextPtr = (int*)paramPtr;
            break;

        case CURLOPT_NOBODY:
            CurlTestHandler->noBody = ((int*)paramPtr > 0) ? true : false;
            break;

        default:
            break;
    }
    va_end(arg);

    return CURLE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * curl_easy_perform
 */
//--------------------------------------------------------------------------------------------------
CURLcode curl_easy_perform
(
    CURL * easy_handle
)
{
    int fd;
    ssize_t readBytes = 0, writeBytes = 0, totalBytes = 0;
    char buffer[1024] = {0};

    if (NULL == CurlTestHandler)
    {
      return CURLE_FAILED_INIT;
    }

    if (false == CurlTestHandler->noBody)
    {
        fd = open(CurlTestHandler->url, O_RDONLY);
        if (-1 == fd)
        {
            LE_ERROR("Unable to open file '%s' for reading", CurlTestHandler->url);
            return CURLE_READ_ERROR;
        }

        // Since this function handles pause and resume, we adjust the read pointer in order
        // to not re-send previous data
        if (lseek(fd, CurlTestHandler->dataOffset, SEEK_SET) == -1)
        {
            LE_ERROR("Seek file to offset %d failed.", CurlTestHandler->dataOffset);
            close(fd);
            return LE_FAULT;
        }
        CurlTestHandler->dataOffset = 0;

        // Read the file by shrunks and send it to the callback
        do
        {
            readBytes = read(fd, buffer, sizeof(buffer));
            if (readBytes > 0)
            {
                writeBytes = CurlTestHandler->writeFnc(buffer,readBytes,sizeof(char),
                                                       CurlTestHandler->contextPtr);
                if (writeBytes != readBytes)
                {
                    LE_ERROR("Cb didn't read all data %lu, read %lu\n", readBytes, writeBytes);
                    close(fd);
                    return CURLE_WRITE_ERROR;
                }

                // Check if the callback needs to pause the current transfert
                if (CURL_READFUNC_PAUSE == writeBytes)
                {
                    CurlTestHandler->dataOffset = totalBytes;
                    close(fd);
                    return CURLE_OK;
                }
                totalBytes += readBytes;
            }
        } while (readBytes > 0);

        close(fd);
    }
    else
    {
        // Since user asked only for the header, we do nothing special here.
    }

    return CURLE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * curl_easy_getopt
 */
//--------------------------------------------------------------------------------------------------
CURLcode curl_easy_getinfo
(
    CURL *curl,
    CURLINFO info,
    ...
)
{
    va_list arg;
    void *paramPtr;
    CURLcode result = CURLE_OK;

    LE_DEBUG("Stub");

    if (NULL == CurlTestHandler)
    {
      return CURLE_FAILED_INIT;
    }

    va_start(arg, info);
    paramPtr = va_arg(arg, void *);

    switch (info)
    {
        case CURLINFO_RESPONSE_CODE:
            *(int*)paramPtr = 200;
            break;

        case CURLINFO_CONTENT_LENGTH_DOWNLOAD:
        {
            struct stat st;
            if (stat(CurlTestHandler->url, &st) != 0)
            {
                result = CURLE_READ_ERROR;
            }
            else
            {
                *(double*)paramPtr = st.st_size;
            }
        }
        break;

        default:
            break;
    }

    va_end(arg);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * curl_easy_cleanup
 */
//--------------------------------------------------------------------------------------------------
void curl_easy_cleanup
(
    CURL *curl
)
{
    LE_DEBUG("Stub");

    // De-allocate the current handler structure
    if (NULL != CurlTestHandler)
    {
        le_mem_Release(CurlTestHandler);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * curl_global_cleanup
 */
//--------------------------------------------------------------------------------------------------
void curl_global_cleanup
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 * curl_easy_strerror
 */
//--------------------------------------------------------------------------------------------------
const char *curl_easy_strerror
(
    CURLcode errornum
)
{
    return "ERROR";
}

//--------------------------------------------------------------------------------------------------
/**
 * curl_version
 */
//--------------------------------------------------------------------------------------------------
char *curl_version
(
    void
)
{
    // Return a dummy version of curl (This is actually the current release of Curl)
    return "7.55.1";
}
