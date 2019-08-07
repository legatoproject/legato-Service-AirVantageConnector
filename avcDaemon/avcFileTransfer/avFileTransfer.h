/**
 * @file avFileTransfer.h
 *
 * Interface for AVC file transfer (for internal use only)
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef LEGATO_AVC_FILE_TRANSFER_INCLUDE_GUARD
#define LEGATO_AVC_FILE_TRANSFER_INCLUDE_GUARD

#ifdef LE_CONFIG_AVC_FEATURE_FILETRANSFER
#include "legato.h"


//--------------------------------------------------------------------------------------------------
// Definitions.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Interface functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the AVC file transfer sub-component.
 *
 * @note This function should be called during the initializaion phase of the AVC daemon.
 */
//--------------------------------------------------------------------------------------------------
void avFileTransfer_Init
(
   void
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the LwM2M object instance list to be provided to LwM2MCore
 */
//--------------------------------------------------------------------------------------------------
void avFileTransfer_InitFileInstanceList
(
   void
);


//--------------------------------------------------------------------------------------------------
/**
 * Send file transfer status event to registered applications
 */
//--------------------------------------------------------------------------------------------------
void avFileTransfer_SendStatusEvent
(
    le_avtransfer_Status_t  status,             ///< [IN] File transfer status
    char*                   fileNamePtr,        ///< [IN] File name
    int32_t                 totalNumBytes,      ///< [IN] Total number of bytes to download
    int32_t                 progress,           ///< [IN] Progress in percent
    void*                   contextPtr          ///< [IN] Context
);


//--------------------------------------------------------------------------------------------------
/**
 * Convert an AVC update status to corresponding file transfer status
 *
 * @return
 *      - le_avtransfer_Status_t for correct convertion
 *      - LE_AVTRANSFER_STATUS_MAX otherwise
 */
//--------------------------------------------------------------------------------------------------
le_avtransfer_Status_t avFileTransfer_ConvertAvcState
(
    le_avc_Status_t     avcUpdateStatus         ///< [IN] AVC update status to convert
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the file name for the file transfer
 *
 * return
 *  - LE_OK if succeeds
 *  - LE_OVERFLOW on buffer overflow
 *  - LE_BAD_PARAMETER if parameter is invalid
 *  - LE_FAULT other failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avFileTransfer_GetTransferName
(
    char*   bufferPtr,              ///< [OUT] Buffer
    size_t* bufferSizePtr           ///< [OUT] Buffer size
);

//--------------------------------------------------------------------------------------------------
/**
 * Treat file transfer progress
 */
//--------------------------------------------------------------------------------------------------
void avFileTransfer_TreatProgress
(
    bool        isLaunched,         ///< [IN] Is transfer launched?
    uint8_t     downloadProgress    ///< [IN] Download progress
);

#endif /* LE_CONFIG_AVC_FEATURE_FILETRANSFER */

#endif // LEGATO_AVC_FILE_TRANSFER_INCLUDE_GUARD
