/**
 * @file packageDownloaderCallbacks.h
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef _PACKAGEDOWNLOADERCALLBACKS_H
#define _PACKAGEDOWNLOADERCALLBACKS_H

#include <legato.h>
#include <osPortUpdate.h>
#include <lwm2mcorePackageDownloader.h>

//--------------------------------------------------------------------------------------------------
/**
 * Initialize download callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_InitDownload
(
    char* uriPtr,
    void* ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Get package information callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_GetInfo
(
    lwm2mcore_PackageDownloaderData_t* dataPtr,
    void*                              ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * SetFwUpdateState callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_SetFwUpdateState
(
    lwm2mcore_fwUpdateState_t fwUpdateState
);

//--------------------------------------------------------------------------------------------------
/**
 * SetFwUpdateResult callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_SetFwUpdateResult
(
    lwm2mcore_fwUpdateResult_t fwUpdateResult
);

//--------------------------------------------------------------------------------------------------
/**
 * SetSwUpdateState callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_SetSwUpdateState
(
    lwm2mcore_swUpdateState_t swUpdateState
);

//--------------------------------------------------------------------------------------------------
/**
 * SetSwUpdateResult callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_SetSwUpdateResult
(
    lwm2mcore_swUpdateResult_t swUpdateResult
);

//--------------------------------------------------------------------------------------------------
/**
 * Download callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_Download
(
    uint64_t startOffset,
    void*    ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Store range callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_StoreRange
(
    uint8_t* bufPtr,
    size_t   bufSize,
    uint64_t offset,
    void*    ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * End download callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_EndDownload
(
    void* ctxPtr
);

#endif /* _PACKAGEDOWNLOADERCALLBACKS_H */
