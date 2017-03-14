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
le_result_t avc_FsRead
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
le_result_t avc_FsWrite
(
    const char* pathPtr,   ///< File path
    uint8_t*    bufPtr,    ///< Data buffer
    size_t      size       ///< Buffer size
);

#endif /* _AVCFS_H */
