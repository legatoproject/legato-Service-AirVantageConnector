/**
 * @file avcFs.h
 *
 * Header for file system management.
 * New file system management variables or functions should be defined here
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef _AVCFS_H
#define _AVCFS_H

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
    const char* pathPtr,   ///< File path
    uint8_t*    bufPtr,    ///< Data buffer
    size_t*     sizePtr    ///< Buffer size
);

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
    const char* pathPtr,   ///< File path
    uint8_t*    bufPtr,    ///< Data buffer
    size_t      size       ///< Buffer size
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Verify if a file exists using Legato le_fs API
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Incorrect parameter provided
 *  - LE_OVERFLOW       The file path is too long
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t ExistsFs
(
    const char* pathPtr ///< File path
);

#endif /* _AVCFS_H */
