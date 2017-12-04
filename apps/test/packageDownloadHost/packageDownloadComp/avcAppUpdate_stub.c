//--------------------------------------------------------------------------------------------------
/**
 * This file is a stubbed version of  the original avcAppUpdate.c that handles managing application
 * update (legato side) over LWM2M.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include <lwm2mcore/update.h>
#include "legato.h"
#include "interfaces.h"
#include "avcServer.h"
#include "lwm2mcorePackageDownloader.h"
#include "packageDownloader.h"
#include "avcAppUpdate.h"
#include "avcFsConfig.h"

//--------------------------------------------------------------------------------------------------
// Public functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Function called to kick off the install of a Legato application.
 *
 * @return
 *      - LE_OK if installation started.
 *      - LE_BUSY if install is not finished yet.
 *      - LE_FAULT if there is an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StartInstall
(
    uint16_t instanceId                 ///< [IN] Instance id of the app to be installed.
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function called to unpack the downloaded package
 *
 * @return
 *      - LE_OK if installation started.
 *      - LE_UNSUPPORTED if not supported
 *      - LE_FAULT if there is an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StartUpdate
(
    void
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Function called to prepare for an application uninstall. This function doesn't remove the app
 *  but deletes only the app objects, so that an existing app can stay running during an upgrade
 *  operation. During an uninstall operation the app will be removed after the client receives the
 *  object9 delete command.
 *
 */
//--------------------------------------------------------------------------------------------------
void avcApp_PrepareUninstall
(
    uint16_t instanceId     ///< [IN] Instance id of the app to be removed.
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 *  Start up the requested legato application.
 *
 *  @return
 *       - LE_OK if start request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_UNAVAILABLE if specified app isn't installed
 *       - LE_DUPLICATE if specified app is already running
 *       - LE_FAULT if there is any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StartApp
(
    uint16_t instanceId   ///< [IN] Instance id of object 9 for this app.
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Stop a Legato application.
 *
 *  @return
 *       - LE_OK if stop request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_UNAVAILABLE if specified app isn't installed
 *       - LE_FAULT if there is any other error.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StopApp
(
    uint16_t instanceId   ///< [IN] Instance id of object 9 for this app.
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Function to get application name.
 *
 *  @return
 *       - LE_OK if stop request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_FAULT if there is any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetPackageName
(
    uint16_t instanceId,    ///< [IN] Instance id of object 9 for this app.
    char* appNamePtr,       ///< [OUT] Buffer to store appName
    size_t len              ///< [IN] Size of the buffer to store appName
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Function to get package name (application name).
 *
 *  @return
 *       - LE_OK if stop request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_FAULT if there is any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetPackageVersion
(
    uint16_t instanceId,    ///< [IN] Instance id of object 9 for this app.
    char* versionPtr,       ///< [OUT] Buffer to store version
    size_t len              ///< [IN] Size of the buffer to store version
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Function to get application activation status.
 *
 *  @return
 *       - LE_OK if stop request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_FAULT if there is any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetActivationState
(
    uint16_t instanceId,    ///< [IN] Instance id of object 9 for this app.
    bool *valuePtr          ///< [OUT] Activation status
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create an object 9 instance
 *
 * @return:
 *      - LE_OK on success
 *      - LE_DUPLICATE if already exists an instance
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_CreateObj9Instance
(
    uint16_t instanceId            ///< [IN] object 9 instance id
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete an object 9 instance
 *
 * @return
 *     - LE_OK if successful
 *     - LE_BUSY if system busy.
 *     - LE_NOT_FOUND if given instance not found or given app is not installed.
 *     - LE_FAULT for any other failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_DeleteObj9Instance
(
    uint16_t instanceId            ///< [IN] object 9 instance id
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Store SW package function
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StoreSwPackage
(
    void *ctxPtr
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Return the offset of the downloaded package.
 *
 * @return
 *      - LE_OK on success
 *      - LE_BAD_PARAMETER Invalid parameter
 *      - LE_FAULT on failure
 */;
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetResumePosition
(
    size_t* positionPtr
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software update state in asset data and SW update workspace for ongoing update.
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if no ongoing update.
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_SetSwUpdateState
(
    lwm2mcore_SwUpdateState_t updateState
)
{
    LE_ASSERT_OK(WriteFs(SW_UPDATE_STATE_PATH, (uint8_t *)&updateState,
                                         sizeof(lwm2mcore_SwUpdateState_t)));
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software update result in asset data and SW update workspace for ongoing update.
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if no ongoing update.
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t  avcApp_SetSwUpdateResult
(
    lwm2mcore_SwUpdateResult_t updateResult
)
{
    LE_ASSERT_OK(WriteFs(SW_UPDATE_RESULT_PATH, (uint8_t *)&updateResult,
                                          sizeof(lwm2mcore_SwUpdateResult_t)));
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update result
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if instance not found
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetSwUpdateResult
(
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateResultPtr        ///< [OUT] Software update result
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update state
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if instance not found
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetSwUpdateState
(
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateStatePtr         ///< [OUT] Software update state
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software update bytes downloaded to workspace
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_SetSwUpdateBytesDownloaded
(
    void
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software update instance id to workspace
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_SetSwUpdateInstanceId
(
    int instanceId     ///< [IN] SW update instance id
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * End download
 */
//--------------------------------------------------------------------------------------------------
void avcApp_EndDownload
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 * Save software update internal state to workspace for resume operation
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_SetSwUpdateInternalState
(
    avcApp_InternalState_t internalState            ///< [IN] internal state
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete SOTA workspace
 */
//--------------------------------------------------------------------------------------------------
void avcApp_DeletePackage
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 * Get saved software update state from workspace for resume operation
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetSwUpdateRestoreState
(
    lwm2mcore_SwUpdateState_t* swUpdateStatePtr     ///< [OUT] SW update state
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get saved software update result from workspace for resume operation
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetSwUpdateRestoreResult
(
    lwm2mcore_SwUpdateResult_t* swUpdateResultPtr     ///< [OUT] SW update result
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update internal state
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetSwUpdateInternalState
(
    avcApp_InternalState_t* internalStatePtr     ///< [OUT] internal state
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Save software update state and result in SW update workspace
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_SaveSwUpdateStateResult
(
    lwm2mcore_SwUpdateState_t swUpdateState,     ///< [IN] New SW update state
    lwm2mcore_SwUpdateResult_t swUpdateResult    ///< [IN] New SW update result
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Send a list of object 9 instances currently managed by legato to lwm2mcore
  */
//--------------------------------------------------------------------------------------------------
void avcApp_NotifyObj9List
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialization function avcApp. Should be called only once.
 */
//--------------------------------------------------------------------------------------------------
void avcApp_Init
(
   void
)
{
    LE_DEBUG("Stub");
}
