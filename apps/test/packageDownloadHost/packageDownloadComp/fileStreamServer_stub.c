/**
 * @file fileStreamServer_stub.c
 *
 * This file is a stubbed version of the fileStreamServer
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
#include <netinet/in.h>

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Location of the firmware image to be sent to the modem
 */
//--------------------------------------------------------------------------------------------------
#define FWUPDATE_STORE_FILE         "/firmware.bin"

//--------------------------------------------------------------------------------------------------
/**
 * Image size offset
 */
//--------------------------------------------------------------------------------------------------
#define CWE_IMAGE_SIZE_OFST     0x114

//--------------------------------------------------------------------------------------------------
/**
 * CWE image header size
 */
//--------------------------------------------------------------------------------------------------
#define CWE_HEADER_SIZE         400


//==================================================================================================
//                                       Public API Functions
//==================================================================================================


//--------------------------------------------------------------------------------------------------
/**
 * Read an integer of the given size and in network byte order from the buffer
 */
//--------------------------------------------------------------------------------------------------
static void ReadUint
(
    uint8_t*    dataPtr,        ///< [IN] Data to be formatted
    uint32_t*   valuePtr        ///< [INOUT] Formated data
)
{
    uint32_t networkValue=0;
    uint8_t* networkValuePtr = ((uint8_t*)&networkValue);

    memcpy(networkValuePtr, dataPtr, sizeof(uint32_t));

    *valuePtr = ntohl(networkValue);
}

//--------------------------------------------------------------------------------------------------
/**
 * Download the firmware image file into /tmp/fwupdate.txt
 *
 * @return
 *      - LE_OK              On success
 *      - LE_BAD_PARAMETER   If an input parameter is not valid
 *      - LE_CLOSED          File descriptor has been closed before all data have been received
 *      - LE_FAULT           On failure
 *
 * @note
 *      The process exits, if an invalid file descriptor (e.g. negative) is given.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_fileStreamServer_Download
(
    int fd
        ///< [IN]
        ///< File descriptor of the image, opened to the start of the image.
)
{

    le_fs_FileRef_t fileRef;
    le_result_t result;
    ssize_t readCount = 0;
    size_t totalCount = 0;
    size_t fullImageLength = 0;
    uint8_t bufPtr[512] = {0};
    uint32_t imageSize;
    int cweImageSizeOffset = CWE_IMAGE_SIZE_OFST;

    result = le_fs_Open(FWUPDATE_STORE_FILE, LE_FS_WRONLY | LE_FS_CREAT, &fileRef);
    if (LE_OK != result)
    {
        LE_ERROR("failed to open %s: %s", FWUPDATE_STORE_FILE, LE_RESULT_TXT(result));
        return result;
    }

    // Make the file descriptor blocking
    int flags = fcntl(fd, F_GETFL);
    if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) != 0)
    {
         LE_ERROR("fcntl failed: %m");

         result = le_fs_Close(fileRef);
         if (LE_OK != result)
         {
             LE_ERROR("failed to close %s: %s", FWUPDATE_STORE_FILE, LE_RESULT_TXT(result));
             return result;
         }

         return LE_FAULT;
    }

    while (true)
    {
        do
        {
            readCount = read(fd, bufPtr, sizeof(bufPtr));
        }
        while ((-1 == readCount) && (EINTR == errno));

        if (readCount > 0)
        {
            totalCount += readCount;
            result = le_fs_Write(fileRef, (uint8_t* )bufPtr, readCount);
            if (LE_OK != result)
            {
                LE_ERROR("failed to write %s: %s", FWUPDATE_STORE_FILE, LE_RESULT_TXT(result));
            }
        }

        if (0 == fullImageLength)
        {
            // Get application image size
            if (totalCount >= CWE_IMAGE_SIZE_OFST + 4)
            {
                ReadUint((uint8_t* )(bufPtr + cweImageSizeOffset), &imageSize);

                // Full length of the CWE image is provided inside the
                // first CWE header
                fullImageLength = imageSize + CWE_HEADER_SIZE;
                LE_DEBUG("fullImageLength: %zu", fullImageLength);
            }
            else
            {
                cweImageSizeOffset = CWE_IMAGE_SIZE_OFST - totalCount;
            }
        }
        else
        {
            if (totalCount >= fullImageLength)
            {
                break;
            }
        }
    }

    LE_INFO("Expected size: %zu, received size: %zu", fullImageLength, totalCount);
    LE_ASSERT(fullImageLength == totalCount);

    result = le_fs_Close(fileRef);
    if (LE_OK != result)
    {
        LE_ERROR("failed to close %s: %s", FWUPDATE_STORE_FILE, LE_RESULT_TXT(result));
        return result;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
  * Init function
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_fileStreamServer_InitStream
(
    void
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *
 * Connect the current client thread to the service providing this API. Block until the service is
 * available.
 *
 * For each thread that wants to use this API, either ConnectService or TryConnectService must be
 * called before any other functions in this API.  Normally, ConnectService is automatically called
 * for the main thread, but not for any other thread. For details, see @ref apiFilesC_client.
 *
 * This function is created automatically.
 */
//--------------------------------------------------------------------------------------------------
void le_fileStreamServer_ConnectService
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 *
 * Disconnect the current client thread from the service providing this API.
 *
 * Normally, this function doesn't need to be called. After this function is called, there's no
 * longer a connection to the service, and the functions in this API can't be used. For details, see
 * @ref apiFilesC_client.
 *
 * This function is created automatically.
 */
//--------------------------------------------------------------------------------------------------
void le_fileStreamServer_DisconnectService
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 * Find resume position of the stream currently in progress
 *
 * @return
 *      - LE_OK if able to retrieve resume position
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_fileStreamServer_GetResumePosition
(
    size_t* resumePosPtr
)
{
    *resumePosPtr = 0;
    LE_DEBUG("stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Find if a stream is currently in progress
 *
 * @return
 *      - 0 if not busy
 *      - 1 if busy
 */
//--------------------------------------------------------------------------------------------------
bool le_fileStreamServer_IsBusy
(
    void
)
{
    LE_DEBUG("stub");
    return false;
}
