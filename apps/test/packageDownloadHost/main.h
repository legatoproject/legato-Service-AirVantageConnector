/**
 * @file main.h
 *
 * Application used to test the package downloader
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef LEGATO_PACKAGEDOWNLOADER_TEST
#define LEGATO_PACKAGEDOWNLOADER_TEST

#include "interfaces.h"
#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 *  Test result structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct {
    le_avc_Status_t updateStatus;   ///< Update status
    le_avc_UpdateType_t updateType; ///< Update type
    int32_t totalNumBytes;          ///< Total number of bytes to download (-1 if not set)
    int32_t dloadProgress;          ///< Download Progress in percent (-1 if not set)
    le_avc_ErrorCode_t errorCode;   ///< Error code
}
DownloadResult_t;

//--------------------------------------------------------------------------------------------------
/**
 *  File comparison structure used in CheckDownloadedFile()
 */
//--------------------------------------------------------------------------------------------------
typedef struct {
    int fd;              ///< File descriptor
    int readBytes;       ///< Number of read bytes by read()
    int fileSize;        ///< File size in bytes
    char buffer[512];    ///< Read buffer
}
FileComp_t;

//--------------------------------------------------------------------------------------------------
/**
 *  Notify the end of the download
 */
//--------------------------------------------------------------------------------------------------
void NotifyCompletion
(
    DownloadResult_t* result
);

#endif
