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
 * Get package download HTTP response code
 *
 * @return
 *      - HTTP response code            The function succeded
 *      - LE_AVC_HTTP_STATUS_INVALID    The function failed
 */
//--------------------------------------------------------------------------------------------------
uint16_t pkgDwlCb_GetHttpStatus
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize download callback function
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_InitDownload
(
    char* uriPtr,
    void* ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Get package information callback function
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_GetInfo
(
    lwm2mcore_PackageDownloaderData_t* dataPtr,
    void*                              ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Set update status callback function for firmware
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_SetFwUpdateState
(
    lwm2mcore_FwUpdateState_t updateState
);

//--------------------------------------------------------------------------------------------------
/**
 * Set update result callback function for firmware
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_SetFwUpdateResult
(
    lwm2mcore_FwUpdateResult_t updateResult
);

//--------------------------------------------------------------------------------------------------
/**
 * Download callback function
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_Download
(
    uint64_t startOffset,
    void*    ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Store range callback function
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
 * End download callback function
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_EndDownload
(
    void* ctxPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Download user agreement callback function definition
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_UserAgreement
(
    uint32_t pkgSize        ///< Package size
);

#endif /* _PACKAGEDOWNLOADERCALLBACKS_H */
