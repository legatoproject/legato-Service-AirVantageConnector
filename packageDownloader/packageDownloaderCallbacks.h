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
 *      - HTTP response code            The function succeeded
 *      - LE_AVC_HTTP_STATUS_INVALID    The function failed
 */
//--------------------------------------------------------------------------------------------------
uint16_t pkgDwlCb_GetHttpStatus
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Get update package size
 *
 * @return
 *      - LE_OK             The function succeeded
 *      - LE_BAD_PARAMETER  A parameter is not correct
 *      - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t pkgDwlCb_GetPackageSize
(
    char* packageUri,           ///< [IN]  Update package URI
    uint64_t* packageSizePtr    ///< [OUT] Update package size
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize download callback function
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_InitDownload
(
    char* uriPtr,   ///< [IN] Package URI
    void* ctxPtr    ///< [IN] Context pointer
);

//--------------------------------------------------------------------------------------------------
/**
 * Get package information callback function
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_GetInfo
(
    lwm2mcore_PackageDownloaderData_t* dataPtr, ///< [IN] Package downloader data pointer
    void*                              ctxPtr   ///< [IN] Context pointer
);

//--------------------------------------------------------------------------------------------------
/**
 * Download callback function
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_SUSPENDED The download is suspended
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_Download
(
    uint64_t    startOffset,    ///< [IN] Start offset for the download
    void*       ctxPtr          ///< [IN] Context pointer
);

//--------------------------------------------------------------------------------------------------
/**
 * Store range callback function
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_StoreRange
(
    uint8_t* bufPtr,    ///< [IN] Pointer on data to store
    size_t   bufSize,   ///< [IN] Size of the data to store
    void*    ctxPtr     ///< [IN] Context pointer
);

//--------------------------------------------------------------------------------------------------
/**
 * End download callback function
 *
 * @return
 *      - DWL_OK        The function succeeded
 *      - DWL_FAULT     The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t pkgDwlCb_EndDownload
(
    void* ctxPtr    ///< [IN] Context pointer
);

#endif /* _PACKAGEDOWNLOADERCALLBACKS_H */
