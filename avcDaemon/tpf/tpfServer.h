/**
 * @file tpfServer.h
 *
 * Interface for TPF Server (for internal use only)
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef LEGATO_RPD_SERVER_INCLUDE_GUARD
#define LEGATO_RPD_SERVER_INCLUDE_GUARD

#include "legato.h"
#include "assetData/assetData.h"
#include "lwm2mcore/update.h"
#include "lwm2mcore/lwm2mcorePackageDownloader.h"
#include "avcFs/avcFs.h"

//--------------------------------------------------------------------------------------------------
/**
 * TPF mode state filesystem path
 */
//--------------------------------------------------------------------------------------------------
#define TPF_SERVER_LEFS_DIR                     "/avc/fw/isTpfServerEnable"

//--------------------------------------------------------------------------------------------------
/**
 * TPF server package URL filesystem path
 */
//--------------------------------------------------------------------------------------------------
#define TPF_SERVER_URL_DIR                      "/avc/param5"

//--------------------------------------------------------------------------------------------------
/**
 * TPF cipher suite index filesystem path
 */
//--------------------------------------------------------------------------------------------------
#define TPF_CIPHER_SUITE_INDEX_PATH             "/avc/param6"

//--------------------------------------------------------------------------------------------------
/**
 * Default cipher suite index
 */
//--------------------------------------------------------------------------------------------------
#define TPF_DEFAULT_CIPHER_SUITE_INDEX      -1


//--------------------------------------------------------------------------------------------------
/**
 * Set TPF mode state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t tpfServer_SetTpfState
(
    bool isTpfEnabled                     ///< [IN] is TPF mode enable
);

//--------------------------------------------------------------------------------------------------
/**
 * Get TPF mode state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t tpfServer_GetTpfState
(
    bool* isTpfEnabledPtr                  ///< [OUT] true if third party FOTA service is activated
);

//--------------------------------------------------------------------------------------------------
/**
 * Function to get the cipher suite profile index for download from 3rd party server.
 *
 * @return
 *      - LE_OK                 on success
 *      - LE_NOT_FOUND          if configuration not found in file system
 *      - LE_FAULT              failure in all other cases
 */
//--------------------------------------------------------------------------------------------------
le_result_t tpfServer_GetCipherSuiteProfileIndex
(
    int32_t* cipherSuiteProfileIndex        ///< [OUT] Cipher suite profile index
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the TPF subsystem
 *
 * Restarts TPF download if it was interrupted by power loss.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void tpfServer_Init
(
    void
);

#endif // LEGATO_AVC_SERVER_INCLUDE_GUARD
