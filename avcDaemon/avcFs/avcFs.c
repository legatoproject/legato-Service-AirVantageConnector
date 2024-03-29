/**
 * @file avcFs.c
 *
 * Implementation of filesystem management
 * New file system management implementation should go here
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <legato.h>
#include <interfaces.h>
#include "avcFs.h"

//--------------------------------------------------------------------------------------------------
/**
 * Read from file using Legato le_fs API
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Incorrect parameter provided
 *  - LE_OVERFLOW       The file path is too long
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t ReadFs
(
    const char* pathPtr,    ///< File path
    uint8_t*    bufPtr,     ///< Data buffer
    size_t*     sizePtr     ///< Buffer size
)
{
    le_fs_FileRef_t fileRef;
    le_result_t result;

    LE_FATAL_IF(!pathPtr, "Invalid parameter");
    LE_FATAL_IF(!bufPtr, "Invalid parameter");

    result = le_fs_Open(pathPtr, LE_FS_RDONLY, &fileRef);
    if (LE_OK != result)
    {
        if (result == LE_NOT_FOUND)
        {
            LE_DEBUG("failed to open %s: %s", pathPtr, LE_RESULT_TXT(result));
        }
        else
        {
            LE_ERROR("failed to open %s: %s", pathPtr, LE_RESULT_TXT(result));
        }
        return result;
    }

    result = le_fs_Read(fileRef, bufPtr, sizePtr);
    if (LE_OK != result)
    {
        LE_ERROR("failed to read %s: %s", pathPtr, LE_RESULT_TXT(result));
        if (LE_OK != le_fs_Close(fileRef))
        {
            LE_ERROR("failed to close %s", pathPtr);
        }
        return result;
    }

    result = le_fs_Close(fileRef);
    if (LE_OK != result)
    {
        LE_ERROR("failed to close %s: %s", pathPtr, LE_RESULT_TXT(result));
        return result;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Write to file using Legato le_fs API
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Incorrect parameter provided
 *  - LE_OVERFLOW       The file path is too long
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t WriteFs
(
    const char  *pathPtr,   ///< File path
    uint8_t     *bufPtr,    ///< Data buffer
    size_t      size        ///< Buffer size
)
{
    le_fs_FileRef_t fileRef;
    le_result_t result;

    LE_FATAL_IF(!pathPtr, "Invalid parameter");
    LE_FATAL_IF(!bufPtr, "Invalid parameter");

    // Don't use LE_FS_TRUNC as it will remove the old data of the file.
    result = le_fs_Open(pathPtr, LE_FS_WRONLY | LE_FS_CREAT | LE_FS_SYNC, &fileRef);
    if (LE_OK != result)
    {
        LE_ERROR("failed to open %s: %s", pathPtr, LE_RESULT_TXT(result));
        return result;
    }

    result = le_fs_Write(fileRef, bufPtr, size);
    if (LE_OK != result)
    {
        LE_ERROR("failed to write %s: %s", pathPtr, LE_RESULT_TXT(result));
        if (LE_OK != le_fs_Close(fileRef))
        {
            LE_ERROR("failed to close %s", pathPtr);
        }
        return result;
    }

    result = le_fs_Close(fileRef);
    if (LE_OK != result)
    {
        LE_ERROR("failed to close %s: %s", pathPtr, LE_RESULT_TXT(result));
        return result;
    }

    // Truncate down to the new size in case the new size is different from the old size
    // On some platforms SetSize is not implemented, but is unnecessary in that case
    // because file is truncated down to size anyway.
    result = le_fs_SetSize(pathPtr, size);
    if (LE_OK != result && LE_NOT_IMPLEMENTED != result)
    {
        LE_ERROR("Failed to set file size %s: %s", pathPtr, LE_RESULT_TXT(result));
        return result;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete file using Legato le_fs API
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  A parameter is invalid
 *  - LE_OVERFLOW       The file path is too long
 *  - LE_NOT_FOUND      The file does not exist or a directory in the path does not exist
 *  - LE_NOT_PERMITTED  The access right fails to delete the file or access is not granted to a
 *                      a directory in the path
 *  - LE_UNSUPPORTED    The function is unusable
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t DeleteFs
(
    const char* pathPtr    ///< File path
)
{
    le_result_t result;

    LE_FATAL_IF(!pathPtr, "Invalid parameter");

    result = le_fs_Delete(pathPtr);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_DEBUG("failed to delete %s: %s", pathPtr, LE_RESULT_TXT(result));
        }
        else
        {
            LE_ERROR("failed to delete %s: %s", pathPtr, LE_RESULT_TXT(result));
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Verify if a file exists using Legato le_fs API
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Incorrect parameter provided
 *  - LE_NOT_FOUND      The file does not exist or a directory in the path does not exist
 */
//--------------------------------------------------------------------------------------------------
le_result_t ExistsFs
(
    const char* pathPtr ///< File path
)
{
    LE_FATAL_IF(!pathPtr, "Invalid parameter");

    if (le_fs_Exists(pathPtr))
    {
        return LE_OK;
    }
    else
    {
        return LE_NOT_FOUND;
    }
}
