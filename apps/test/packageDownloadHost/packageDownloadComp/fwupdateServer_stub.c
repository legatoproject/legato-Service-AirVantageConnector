/**
 * @file fwupdateServer.c
 *
 * This file is a stubbed version of the fwupdateServer that implements the FW Update API
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
#define FWUPDATE_STORE_FILE         "/fwupdate"

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
static void ReadUint(uint8_t* dataPtr, uint32_t* valuePtr)
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
le_result_t le_fwupdate_Download
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
                LE_DEBUG("fullImageLength: %u", fullImageLength);
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

    LE_INFO("Expected size: %d, received size: %d", fullImageLength, totalCount);
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
 * Download initialization:
 *  - for single and dual systems, it resets resume position,
 *  - for dual systems, it synchronizes the systems if needed.
 *
 * @note
 *      When invoked, resuming a previous download is not possible, a full update package has to be
 *      downloaded.
 *
 * @return
 *      - LE_OK         On success
 *      - LE_FAULT      On failure
 *      - LE_IO_ERROR   Dual systems platforms only -- The synchronization fails due to
 *                      unrecoverable ECC errors. In this case, the update without synchronization
 *                      is forced, but the whole system must be updated to ensure that the new
 *                      update system will be workable
 *                      ECC stands for Error-Correction-Code: some errors may be corrected. If this
 *                      correction fails, a unrecoverable error is registered and the data become
 *                      corrupted.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_fwupdate_InitDownload
(
    void
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Return the downloaded update package write position.
 *
 * @return
 *      - LE_OK on success
 *      - LE_BAD_PARAMETER Invalid parameter
 *      - LE_FAULT on failure
 */;
//--------------------------------------------------------------------------------------------------
le_result_t le_fwupdate_GetResumePosition
(
    size_t *positionPtr     ///< [OUT] Update package read position
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Return the update status, which is either the last status of the systems swap if it failed, or
 * the status of the secondary bootloader (SBL).
 *
 * @return
 *      - LE_OK on success
 *      - LE_BAD_PARAMETER Invalid parameter
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_fwupdate_GetUpdateStatus
(
    le_fwupdate_UpdateStatus_t *statusPtr, ///< [OUT] Returned update status
    char *statusLabelPtr,                  ///< [OUT] Points to the string matching the status
    size_t statusLabelLength               ///< [IN]  Maximum label length
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Request to install the package. Calling this API will result in a system reset.
 *
 * Dual System: After reset, the UPDATE and ACTIVE systems will be swapped.
 * Single System: After reset, the image in the FOTA partition will be installed on the device.
 *
 * @note On success, a device reboot will be initiated.
 *
 *
 * @return
 *      - LE_BUSY          Download is ongoing, install is not allowed
 *      - LE_UNSUPPORTED   The feature is not supported
 *      - LE_FAULT         On failure
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_fwupdate_Install
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
void le_fwupdate_ConnectService
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
void le_fwupdate_DisconnectService
(
    void
)
{
    LE_DEBUG("Stub");
}
