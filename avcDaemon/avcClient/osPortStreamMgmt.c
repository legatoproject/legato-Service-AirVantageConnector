/**
 * @file osPortStreamMgmt.c
 *
 * Porting layer for Stream Management Object
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/fileTransfer.h>
#include "avcFileTransfer/avFileTransfer.h"


//--------------------------------------------------------------------------------------------------
/**
 * @brief File transfer request
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @note
 * This function is available only if @c LWM2M_OBJECT_33406 flag is embedded
 *
 * @note
 * For CoAP retry reason, this function treatment needs to be synchronous
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if parameter is invalid
 *  - @ref LWM2MCORE_ERR_OVERFLOW if the buffer is too long
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR other failure
 *  - @ref LWM2MCORE_ERR_ALREADY_PROCESSED if the file is already present
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_FileTransferRequest
(
    lwm2mcore_FileTransferRequest_t fileTransferInfo,       ///< [IN] File transfer info
    bool*                           couldDwnldBeLaunchedPtr ///< [OUT] Could download be launched?
)
{
    le_fileStreamClient_StreamMgmt_t streamMgmtObj;
    le_result_t result;
    uint16_t instanceId = 0;

    LE_DEBUG("File info for transfer");
    LE_DEBUG("Name: %s - Class %s - Hash %s - Direction %d",
             fileTransferInfo.fileName,
             fileTransferInfo.fileClass,
             fileTransferInfo.fileHash,
             fileTransferInfo.direction);

    if (!couldDwnldBeLaunchedPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Data length was already made
    if ((strlen(fileTransferInfo.fileName) > LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR)
     || (strlen(fileTransferInfo.fileClass) > LWM2MCORE_FILE_TRANSFER_CLASS_MAX_CHAR)
     || (strlen(fileTransferInfo.fileHash) > LWM2MCORE_FILE_TRANSFER_HASH_MAX_CHAR)
     || (fileTransferInfo.direction >= LWM2MCORE_FILE_TRANSFER_DIRECTION_MAX))
    {
        LE_ERROR("File transfer overflow");
        return LWM2MCORE_ERR_OVERFLOW;
    }

    if (fileTransferInfo.direction >= LWM2MCORE_FILE_TRANSFER_DIRECTION_MAX)
    {
        LE_ERROR("File transfer invalid arg");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Search if the file is already present (same name, same hash)
    result = le_fileStreamServer_IsFilePresent(fileTransferInfo.fileName,
                                               fileTransferInfo.fileHash,
                                               &instanceId);
    LE_DEBUG("Is file already present ?: %s", LE_RESULT_TXT(result));
    if (LE_OK == result)
    {
        if (LE_FILESTREAMSERVER_INSTANCE_ID_DOWNLOAD == instanceId)
        {
            // If the same file is in downloading phase, consider it like OK
            LE_DEBUG("The file is already in downloading phasis");
            *couldDwnldBeLaunchedPtr = false;
        }
        else
        {
            *couldDwnldBeLaunchedPtr = true;
        }
        return LWM2MCORE_ERR_ALREADY_PROCESSED;
    }

    // Search if a file with the same name is already present
    result = le_fileStreamServer_IsFilePresent(fileTransferInfo.fileName, "", &instanceId);
    LE_DEBUG("Is file with same name present ?: %s", LE_RESULT_TXT(result));
    if (LE_OK == result)
    {
        LE_DEBUG("Need to delete the previous version");

        // Delete the file
        if (LE_OK != le_fileStreamServer_Delete(fileTransferInfo.fileName))
        {
            LE_ERROR("Not possible to delete the current file version %s",
                     fileTransferInfo.fileName);
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }
        LE_DEBUG("Old version of %s file was successfully deleted", fileTransferInfo.fileName);
    }

    if (le_fileStreamClient_GetStreamMgmtObject(LE_FILESTREAMSERVER_INSTANCE_ID_DOWNLOAD,
                                                &streamMgmtObj) != LE_OK)
    {
        LE_DEBUG("No file for download");
    }

    memset(&streamMgmtObj, 0, sizeof(le_fileStreamClient_StreamMgmt_t));

    streamMgmtObj.instanceId = LE_FILESTREAMSERVER_INSTANCE_ID_DOWNLOAD;
    snprintf(streamMgmtObj.pkgName,
             LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR+1,
             "%s",
             fileTransferInfo.fileName);
    snprintf(streamMgmtObj.pkgTopic,
             LWM2MCORE_FILE_TRANSFER_CLASS_MAX_CHAR+1,
             "%s",
             fileTransferInfo.fileClass);
    snprintf(streamMgmtObj.hash,
             LWM2MCORE_FILE_TRANSFER_HASH_MAX_CHAR+1,
             "%s",
             fileTransferInfo.fileHash);
    streamMgmtObj.direction = (uint8_t)fileTransferInfo.direction;

    if (le_fileStreamClient_SetStreamMgmtObject(&streamMgmtObj) != LE_OK)
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    le_fileStreamServer_DownloadStatus(LE_FILESTREAMCLIENT_DOWNLOAD_IDLE, 0, 0);
    *couldDwnldBeLaunchedPtr = true;

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Get the file checksum for the file transfer
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *  - @ref LWM2MCORE_ERR_OVERFLOW on buffer overflow
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if parameter is invalid
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetFileTransferChecksum
(
    char*   bufferPtr,                ///< [OUT] Buffer
    size_t* bufferSizePtr             ///< [OUT] Buffer size
)
{
    le_fileStreamClient_StreamMgmt_t streamMgmtObj;
    uint16_t instanceId = LE_FILESTREAMSERVER_INSTANCE_ID_DOWNLOAD;

    if ((!bufferPtr) || (!bufferSizePtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (le_fileStreamClient_GetStreamMgmtObject(instanceId, &streamMgmtObj) != LE_OK)
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
    if (strlen(streamMgmtObj.hash) > (*bufferSizePtr))
    {
        return LWM2MCORE_ERR_OVERFLOW;
    }

    strncpy(bufferPtr, streamMgmtObj.hash, *bufferSizePtr);
    *bufferSizePtr = strlen(bufferPtr);

    return LWM2MCORE_ERR_COMPLETED_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * @brief Get the file name from its intance Id
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if parameter is invalid
 *  - @ref LWM2MCORE_ERR_OVERFLOW on buffer overflow
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetFileNameByInstance
(
    uint16_t    instanceId,         ///< [IN] Instance Id of object 33406
    char*       bufferPtr,          ///< [OUT] Buffer
    size_t*     bufferSizePtr       ///< [INOUT] Buffer size
)
{
    if ((!bufferPtr) || (!bufferSizePtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK != le_fileStreamServer_GetFileInfoByInstance(instanceId,
                                                           bufferPtr,
                                                           *bufferSizePtr,
                                                           NULL,
                                                           0,
                                                           NULL,
                                                           0,
                                                           NULL,
                                                           NULL
                                                           ))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    *bufferSizePtr = strlen(bufferPtr);
    LE_DEBUG("File name %s", bufferPtr);
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Get the file class from its intance Id
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @note
 * This function is available only if @c LWM2M_OBJECT_33406 flag is embedded
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *  - @ref LWM2MCORE_ERR_OVERFLOW on buffer overflow
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if parameter is invalid
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetFileClassByInstance
(
    uint16_t    instanceId,         ///< [IN] Instance Id of object 33406
    char*       bufferPtr,          ///< [OUT] Buffer
    size_t*     bufferSizePtr       ///< [INOUT] Buffer size
)
{
    char        fileNamePtr[LE_FILESTREAMSERVER_FILE_NAME_MAX_BYTES];
    uint8_t     origin = 0;
    if ((!bufferPtr) || (!bufferSizePtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK != le_fileStreamServer_GetFileInfoByInstance(instanceId,
                                                           fileNamePtr,
                                                           LE_FILESTREAMSERVER_FILE_NAME_MAX_BYTES,
                                                           bufferPtr,
                                                           *bufferSizePtr,
                                                           NULL,
                                                           0,
                                                           NULL,
                                                           &origin
                                                           ))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    *bufferSizePtr = strlen(bufferPtr);
    LE_DEBUG("File class %s", bufferPtr);
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Get the file hashcode from its intance Id
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if parameter is invalid
 *  - @ref LWM2MCORE_ERR_OVERFLOW on buffer overflow
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetFileChecksumByInstance
(
    uint16_t    instanceId,         ///< [IN] Instance Id of object 33406
    char*       bufferPtr,          ///< [OUT] Buffer
    size_t*     bufferSizePtr       ///< [INOUT] Buffer size
)
{
    char        fileNamePtr[LE_FILESTREAMSERVER_FILE_NAME_MAX_BYTES];
    char        fileTopicPtr[LE_FILESTREAMSERVER_FILE_TOPIC_MAX_BYTES];

    if ((!bufferPtr) || (!bufferSizePtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK != le_fileStreamServer_GetFileInfoByInstance(instanceId,
                                                           fileNamePtr,
                                                           LE_FILESTREAMSERVER_FILE_NAME_MAX_BYTES,
                                                           fileTopicPtr,
                                                           LE_FILESTREAMSERVER_FILE_TOPIC_MAX_BYTES,
                                                           bufferPtr,
                                                           *bufferSizePtr,
                                                           NULL,
                                                           NULL
                                                           ))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    *bufferSizePtr = strlen(bufferPtr);
    LE_DEBUG("File hash %s", bufferPtr);
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Get the file origin
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @note
 * This function is available only if @c LWM2M_OBJECT_33406 flag is embedded
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if parameter is invalid
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetFileOriginByInstance
(
    uint16_t                        instanceId,          ///< [IN] Instance Id of object 33406
    lwm2mcore_FileListOrigin_t*     originPtr            ///< [OUT] File origin
)
{
    if (!originPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK != le_fileStreamServer_GetFileInfoByInstance(instanceId,
                                                           NULL,
                                                           0,
                                                           NULL,
                                                           0,
                                                           NULL,
                                                           0,
                                                           NULL,
                                                           (uint8_t*)originPtr
                                                            ))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("File origin %d", *originPtr);
    return LWM2MCORE_ERR_COMPLETED_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * @brief Delete a file by its instance Id
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @note
 * This function is available only if @c LWM2M_OBJECT_33406 flag is embedded
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if parameter is invalid
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_DeleteFileByInstance
(
    uint16_t instanceId                                 ///< [IN] Instance Id of object 33406
)
{
    le_result_t result;
    lwm2mcore_Sid_t sID;
    char fileNamePtr[LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR + 1];
    size_t fileNameSize = LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR;

    sID = lwm2mcore_GetFileNameByInstance(instanceId, fileNamePtr, &fileNameSize);
    LE_DEBUG("lwm2mcore_GetFileNameByInstance return %d, fileNamePtr %s", sID, fileNamePtr);

    result = le_fileStreamServer_DeleteFileByInstance(instanceId);
    LE_DEBUG("le_fileStreamServer_DeleteFileByInstance return %d", result);

    switch (result)
    {
        case LE_OK:
            if(LWM2MCORE_ERR_COMPLETED_OK == sID)
            {
                avFileTransfer_SendStatusEvent(LE_AVTRANSFER_DELETED, fileNamePtr, 0, 0, NULL);
            }
            else
            {
                LE_ERROR("Can not send DELETE notification (get name error %d)", sID);
            }

            // Update the supported object instances list
            avFileTransfer_InitFileInstanceList();
            return LWM2MCORE_ERR_COMPLETED_OK;

        case LE_BAD_PARAMETER:
        return LWM2MCORE_ERR_INVALID_ARG;

        default:
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Get available space for file storage
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @note
 * This function is available only if @c LWM2M_OBJECT_33406 flag is embedded
 *
 * @note
 * For CoAP retry reason, this function treatment needs to be synchronous
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if parameter is invalid
 *  - @ref LWM2MCORE_ERR_OVERFLOW if the buffer is too long
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_FileTransferAvailableSpace
(
    uint64_t*   availableSpacePtr           ///< [OUT] Available space
)
{
    if (!availableSpacePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK != le_fileStreamServer_GetAvailableSpace(availableSpacePtr))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief The file transfer is aborted
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @note
 * This function is available only if @c LWM2M_OBJECT_33406 flag is embedded
 *
 * @note
 * For CoAP retry reason, this function treatment needs to be synchronous
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_FileTransferAbort
(
    void
)
{
    char fileName[LE_FILESTREAMSERVER_FILE_NAME_MAX_BYTES] = {0};
    size_t fileNameLen = LE_FILESTREAMSERVER_FILE_NAME_MAX_LEN;

    if (LE_OK == avFileTransfer_GetTransferName(fileName, &fileNameLen))
    {
        le_fileStreamServer_DeleteFileByInstance(LE_FILESTREAMSERVER_INSTANCE_ID_DOWNLOAD);
        avFileTransfer_SendStatusEvent(LE_AVTRANSFER_ABORTED, fileName, 0, 0, NULL);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
    return LWM2MCORE_ERR_GENERAL_ERROR;
}
