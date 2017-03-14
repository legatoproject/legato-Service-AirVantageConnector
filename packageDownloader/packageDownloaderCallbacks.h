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

#include <lwm2mcore/update.h>
#include <lwm2mcorePackageDownloader.h>
#include <legato.h>

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
 * Set update status callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_SetFwUpdateState
(
    lwm2mcore_FwUpdateState_t updateState
);

//--------------------------------------------------------------------------------------------------
/**
 * Set update result callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_SetFwUpdateResult
(
    lwm2mcore_FwUpdateResult_t updateResult
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
