/**
 * @file packageDownloader.h
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef _PACKAGEDOWNLOADER_H
#define _PACKAGEDOWNLOADER_H

#include <legato.h>
#include <osPortUpdate.h>
#include <lwm2mcorePackageDownloader.h>

//--------------------------------------------------------------------------------------------------
/**
 * Download context data structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    const char*      fifoPtr;               ///< store FIFO pointer
    void*            ctxPtr;                ///< context pointer
    le_thread_Ref_t  mainRef;               ///< main thread reference
    void (*downloadPackage)(void *ctxPtr);  ///< download package callback
    void (*storePackage)(void *ctxPtr);     ///< store package callback
}
packageDownloader_DownloadCtx_t;

//--------------------------------------------------------------------------------------------------
/**
 * Download package thread function
 */
//--------------------------------------------------------------------------------------------------
void* packageDownloader_DownloadPackage
(
    void* ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Store FW package thread function
 */
//--------------------------------------------------------------------------------------------------
void* packageDownloader_StoreFwPackage
(
    void* ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Store SW package thread function
 */
//--------------------------------------------------------------------------------------------------
void* packageDownloader_StoreSwPackage
(
    void* ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Download and store a package
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_StartDownload
(
    const char*            uriPtr,
    lwm2mcore_updateType_t type
);

//--------------------------------------------------------------------------------------------------
/**
 * Abort a package download
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_AbortDownload
(
    void
);

#endif /*_PACKAGEDOWNLOADER_H */
