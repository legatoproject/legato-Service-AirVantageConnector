/**
 * @file avcFs.c
 *
 * Impelementation of filesystem management
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
le_result_t avc_FsRead
(
    const char* pathPtr,    ///< File path
    uint8_t*    bufPtr,     ///< Data buffer
    size_t*     sizePtr     ///< Buffer size
)
{
    le_fs_FileRef_t fileRef;
    le_result_t result;

    result = le_fs_Open(pathPtr, LE_FS_RDONLY, &fileRef);
    if (LE_OK != result)
    {
        LE_ERROR("failed to open %s: %s", pathPtr, LE_RESULT_TXT(result));
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
le_result_t avc_FsWrite
(
    const char  *pathPtr,   ///< File path
    uint8_t     *bufPtr,    ///< Data buffer
    size_t      size        ///< Buffer size
)
{
    le_fs_FileRef_t fileRef;
    le_result_t result;

    result = le_fs_Open(pathPtr, LE_FS_WRONLY | LE_FS_CREAT, &fileRef);
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

    return LE_OK;
}
