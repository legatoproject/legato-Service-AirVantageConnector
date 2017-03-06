/**
 * @file packageDownloaderUpdateInfo.h
 *
 * Header for the package downloader update information
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef __PACKAGEDOWNLOADERUPDATEINFO_H__
#define __PACKAGEDOWNLOADERUPDATEINFO_H__

#include <lwm2mcore.h>
#include "osPortUpdate.h"

//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Define filenames in which the package downloader update information is stored
 */
//--------------------------------------------------------------------------------------------------
#define UPDATE_INFO_DIR             "/avms/packageDownloader"
#define FW_UPDATE_INFO_DIR          UPDATE_INFO_DIR "/" "fw"
#define SW_UPDATE_INFO_DIR          UPDATE_INFO_DIR "/" "sw"
#define UPDATE_RESULT_FILENAME      "updateResult"
#define UPDATE_STATE_FILENAME       "updateState"
#define FW_UPDATE_RESULT_FILENAME   FW_UPDATE_INFO_DIR "/" UPDATE_RESULT_FILENAME
#define FW_UPDATE_STATE_FILENAME    FW_UPDATE_INFO_DIR "/" UPDATE_STATE_FILENAME
#define SW_UPDATE_RESULT_FILENAME   SW_UPDATE_INFO_DIR "/" UPDATE_RESULT_FILENAME
#define SW_UPDATE_STATE_FILENAME    SW_UPDATE_INFO_DIR "/" UPDATE_STATE_FILENAME

//--------------------------------------------------------------------------------------------------
// Public functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Set FW update result
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateResult
(
    lwm2mcore_fwUpdateResult_t fwUpdateResult   ///< [IN] New FW update result
);

//--------------------------------------------------------------------------------------------------
/**
 * Set FW update state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateState
(
    lwm2mcore_fwUpdateState_t fwUpdateState     ///< [IN] New FW update state
);

//--------------------------------------------------------------------------------------------------
/**
 * Set SW update result
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetSwUpdateResult
(
    lwm2mcore_swUpdateResult_t swUpdateResult   ///< [IN] New SW update result
);

//--------------------------------------------------------------------------------------------------
/**
 * Set SW update state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetSwUpdateState
(
    lwm2mcore_swUpdateState_t swUpdateState     ///< [IN] New SW update state
);

//--------------------------------------------------------------------------------------------------
/**
 * Get FW update result
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateResult
(
    lwm2mcore_fwUpdateResult_t* fwUpdateResultPtr   ///< [INOUT] FW update result
);

//--------------------------------------------------------------------------------------------------
/**
 * Get FW update state
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateState
(
    lwm2mcore_fwUpdateState_t* fwUpdateStatePtr     ///< [INOUT] FW update state
);

//--------------------------------------------------------------------------------------------------
/**
 * Get SW update result
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetSwUpdateResult
(
    lwm2mcore_swUpdateResult_t* swUpdateResultPtr   ///< [INOUT] SW update result
);

//--------------------------------------------------------------------------------------------------
/**
 * Get SW update state
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetSwUpdateState
(
    lwm2mcore_swUpdateState_t* swUpdateStatePtr     ///< [INOUT] SW update state
);
#endif /* __PACKAGEDOWNLOADERUPDATEINFO_H__ */
