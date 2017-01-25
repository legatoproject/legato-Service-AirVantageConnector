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
lwm2mcore_DwlResult_t InitDownload
(
    char *uriPtr,
    void *ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Get package information callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t GetInfo
(
    lwm2mcore_PackageDownloaderData_t   *dataPtr,
    void                                *ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Set update status callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t SetUpdateState
(
    lwm2mcore_fwUpdateState_t updateState
);

//--------------------------------------------------------------------------------------------------
/**
 * Set update result callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t SetUpdateResult
(
    lwm2mcore_fwUpdateResult_t updateResult
);

//--------------------------------------------------------------------------------------------------
/**
 * Download range callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t DownloadRange
(
    uint8_t     *bufPtr,
    size_t      bufSize,
    uint64_t    startOffset,
    size_t      *dwlLenPtr,
    void        *ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Store range callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t StoreRange
(
    uint8_t     *bufPtr,
    size_t      bufSize,
    uint64_t    offset,
    void        *ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * End download callback definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t EndDownload
(
    void *ctxPtr
);

#endif /* _PACKAGEDOWNLOADERCALLBACKS_H */
