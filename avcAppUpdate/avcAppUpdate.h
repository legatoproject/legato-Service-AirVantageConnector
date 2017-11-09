//--------------------------------------------------------------------------------------------------
/**
 * Header file containing API to manage application update (legato side) over LWM2M.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef LEGATO_AVC_APP_UPDATE_DEFS_INCLUDE_GUARD
#define LEGATO_AVC_APP_UPDATE_DEFS_INCLUDE_GUARD

#include <lwm2mcore/update.h>
#include <lwm2mcorePackageDownloader.h>


//--------------------------------------------------------------------------------------------------
/**
 * Internal state of avc app update used to track of the commands received from AV server or device.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    INTERNAL_STATE_INVALID = 0,             ///< Invalid internal state
    INTERNAL_STATE_DOWNLOAD_REQUESTED,      ///< Download request from server received by device
    INTERNAL_STATE_INSTALL_REQUESTED,       ///< Install request from server received by device
    INTERNAL_STATE_UNINSTALL_REQUESTED,     ///< Uninstall request from server received by device
    INTERNAL_STATE_CONNECTION_REQUESTED     ///< Device requests connection after any app update

}
avcApp_InternalState_t;

//--------------------------------------------------------------------------------------------------
/**
 *  Maximum allowed size for for a Legato framework version string.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_VERSION_STR 100
#define MAX_VERSION_STR_BYTES (MAX_VERSION_STR + 1)

//--------------------------------------------------------------------------------------------------
/**
 * Store SW package function
 *
 * @return
 *     - LE_OK if storing starts successfully.
 *     - LE_FAULT if there is any error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StoreSwPackage
(
    void *ctxPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Start installation of a Legato application.
 *
 * @return
 *      - LE_OK if installation started.
 *      - LE_BUSY if package download is not finished yet.
 *      - LE_FAULT if there is an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StartInstall
(
    uint16_t instanceId    ///< [IN] Instance id of the app to be installed.
);


//--------------------------------------------------------------------------------------------------
/**
 *  Prepare for an application removal. This function doesn't remove the app but deletes only the
 *  app objects, so that an existing app can stay running during an upgrade operation. During an
 *  uninstall operation the app will be removed after the client receives the object9 delete
 *  command.
 *
 */
//--------------------------------------------------------------------------------------------------
void avcApp_PrepareUninstall
(
    uint16_t instanceId     ///< [IN] Instance id of the app to be removed.
);


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
);


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
);


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
);


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
);


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
);

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
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * End download
 */
//--------------------------------------------------------------------------------------------------
void avcApp_EndDownload
(
    void
);

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
    avcApp_InternalState_t internalState            ///< [IN] Internal state
);

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
);

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
);

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
);

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
);

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
le_result_t avcApp_GetPackageName
(
    uint16_t instanceId,    ///< [IN] Instance id of object 9 for this app.
    char* appNamePtr,       ///< [OUT] Buffer to store appName
    size_t len              ///< [IN] Size of the buffer to store appName
);


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
le_result_t avcApp_GetPackageVersion
(
    uint16_t instanceId,    ///< [IN] Instance id of object 9 for this app.
    char* versionPtr,       ///< [OUT] Buffer to store version
    size_t len              ///< [IN] Size of the buffer to store version
);


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
);


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
);


//--------------------------------------------------------------------------------------------------
/**
 * Create an object 9 instance and initializes SOTA workspace. If no instance exists, then instance
 * will be created. If already an instance exists, its fields will be overridden. In both case, SOTA
 * workspace will be cleared.
 *
 * @return:
 *      - LE_OK on success.
 *      - LE_DUPLICATE if already exists an instance
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_CreateObj9Instance
(
    uint16_t instanceId            ///< [IN] object 9 instance id
);


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
);

//--------------------------------------------------------------------------------------------------
/**
 * Send a list of object 9 instances currently managed by legato to lwm2mcore
 */
//--------------------------------------------------------------------------------------------------
void avcApp_NotifyObj9List
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if there is a software update related notification to send after a reboot
 * or a service restart.
 *
 * @return
 *      - LE_OK     No notification to send
 *      - LE_BUSY   Notification sent
 *      - LE_FAULT  An error occurred
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_CheckNotificationToSend
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialisation function avcApp. Should be called only once.
 */
//--------------------------------------------------------------------------------------------------
void avcApp_Init
(
   void
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete SOTA workspace
 */
//--------------------------------------------------------------------------------------------------
void avcApp_DeletePackage
(
    void
);

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
);

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
);

#endif
