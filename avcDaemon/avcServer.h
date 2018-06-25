/**
 * @file avcServer.h
 *
 * Interface for AVC Server (for internal use only)
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef LEGATO_AVC_SERVER_INCLUDE_GUARD
#define LEGATO_AVC_SERVER_INCLUDE_GUARD

#include "legato.h"
#include "assetData.h"
#include "lwm2mcore/update.h"
#include "lwm2mcorePackageDownloader.h"
#include "avcFs.h"

//--------------------------------------------------------------------------------------------------
/**
 *  Path to the persistent asset setting path
 */
//--------------------------------------------------------------------------------------------------
#define CFG_ASSET_SETTING_PATH "/apps/avcService/settings"

//--------------------------------------------------------------------------------------------------
// Definitions.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for handler used with avcServer_QueryInstall() to return install response.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*avcServer_InstallHandlerFunc_t)
(
    lwm2mcore_UpdateType_t type,    ///< Update type
    uint16_t instanceId             ///< Instance id (0 for fw update)
);

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for handler used with avcServer_QueryUninstall() to return uninstall response.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*avcServer_UninstallHandlerFunc_t)
(
    uint16_t instanceId             ///< [IN] Instance id.
);

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for handler used with avcServer_QueryDownload() to return download response.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*avcServer_DownloadHandlerFunc_t)
(
    const char*            uriPtr,  ///< Package URI
    lwm2mcore_UpdateType_t type,    ///< Update type (FW/SW)
    bool                   resume   ///< Indicates if it is a download resume
);

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for handler used with avcServer_QueryReboot() to return reboot response.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*avcServer_RebootHandlerFunc_t)
(
    void
);


//--------------------------------------------------------------------------------------------------
// Interface functions
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with an application install.
 *
 * If an install can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with an install. Note that handlerRef will be called at most once.
 * If an install can proceed right away, it will be launched.
 *
 * @return None
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void avcServer_QueryInstall
(
    avcServer_InstallHandlerFunc_t handlerRef,  ///< [IN] Handler to receive install response.
    lwm2mcore_UpdateType_t type,                ///< [IN] Update type.
    uint16_t instanceId                         ///< [IN] Instance id.
);

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with an application uninstall.
 *
 * If an uninstall can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with an uninstall. Note that handlerRef will be called at most once.
 * If an uninstall can proceed right away, it will be launched.
 *
 * @return None
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void avcServer_QueryUninstall
(
    avcServer_UninstallHandlerFunc_t handlerRef,  ///< [IN] Handler to receive uninstall response.
    uint16_t instanceId                           ///< Instance Id (0 for FW, any value for SW)
);

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a package download.
 *
 * If a download can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with a download. Note that handlerRef will be called at most once.
 * If a download can proceed right away, it will be launched.
 *
 * @return None
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void avcServer_QueryDownload
(
    avcServer_DownloadHandlerFunc_t handlerFunc,    ///< [IN] Download handler function
    uint64_t bytesToDownload,                       ///< [IN] Number of bytes to download
    lwm2mcore_UpdateType_t type,                    ///< [IN] Update type
    char* uriPtr,                                   ///< [IN] Update package URI
    bool resume,                                    ///< [IN] Is it a download resume?
    le_avc_StatusHandlerFunc_t statusHandlerPtr,    ///< [IN] Pointer on handler function
    void*                      contextPtr           ///< [IN] Context

);


//--------------------------------------------------------------------------------------------------
/**
 * Resets user agreement query handlers of download, install and uninstall. This also stops
 * corresponding defer timers.
 */
//--------------------------------------------------------------------------------------------------
void avcServer_ResetQueryHandlers
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Report the status of install back to the control app
 *
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void avcServer_NotifyUserApp
(
    le_avc_Status_t updateStatus,
    uint numBytes,                 ///< [IN]  Number of bytes to download.
    uint installProgress,          ///< [IN]  Percentage of install completed.
                                   ///        Applicable only when le_avc_Status_t is one of
                                   ///        LE_AVC_INSTALL_IN_PROGRESS, LE_AVC_INSTALL_COMPLETE
                                   ///        or LE_AVC_INSTALL_FAILED.
    le_avc_ErrorCode_t errorCode   ///< [IN]  Error code if installation failed.
                                   ///        Applicable only when le_avc_Status_t is
                                   ///        LE_AVC_INSTALL_FAILED.
);



//--------------------------------------------------------------------------------------------------
/**
 * Request the avcServer to open a AV session.
 *
 * @return
 *      - LE_OK if able to initiate a session open
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t avcServer_RequestSession
(
);

//--------------------------------------------------------------------------------------------------
/**
 * Request the avcServer to close a AV session.
 *
 * @return
 *      - LE_OK if able to initiate a session close
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t avcServer_ReleaseSession
(
);

//--------------------------------------------------------------------------------------------------
/**
 * Handler to receive update status notifications from PA
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void avcServer_UpdateStatus
(
    le_avc_Status_t updateStatus,
    le_avc_UpdateType_t updateType,
    int32_t totalNumBytes,
    int32_t dloadProgress,
    le_avc_ErrorCode_t errorCode,
    le_avc_StatusHandlerFunc_t statusHandlerPtr,
    void* contextPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Start a session with the AirVantage server
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_FAULT on failure
 *      - LE_DUPLICATE if already connected.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_StartSession
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Request the avcServer to open a AV session.
 *
 * @return
 *      - LE_OK if able to initiate a session open
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t avcServer_RequestSession
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Request the avcServer to close a AV session.
 *
 * @return
 *      - LE_OK if able to initiate a session close
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t avcServer_ReleaseSession
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a server connection.
 *
 * For FOTA it should be called only after reboot, and for SOTA it should be called after the
 * update finishes. However, this function will request a connection to the server only if there
 * is no session going on.
 * If the connection can proceed right away, it will be launched.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void avcServer_QueryConnection
(
    le_avc_UpdateType_t        updateType,        ///< [IN] Update type
    le_avc_StatusHandlerFunc_t statusHandlerPtr,  ///< [IN] Pointer on handler function
    void*                      contextPtr         ///< [IN] Context
);

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a device reboot.
 *
 * If a reboot can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with a reboot. Note that handlerRef will be called at most once.
 * If a reboot can proceed right away, it will be launched.
 *
 * @return None
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void avcServer_QueryReboot
(
    avcServer_RebootHandlerFunc_t handlerFunc   ///< [IN] Reboot handler function.
);

//--------------------------------------------------------------------------------------------------
/**
 * Reset the stored download agreement
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void avcServer_ResetDownloadAgreement
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Is the current state AVC_IDLE?
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool avcServer_IsIdle
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Is the current session initiated by user app?
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool avcServer_IsUserSession
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Resume firmware install if necessary
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
void ResumeFwInstall
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if the update state/result should be changed after a FW install
 * and update them if necessary
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetFirmwareUpdateInstallResult
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Handler to receive update status notifications
 */
//--------------------------------------------------------------------------------------------------
void avcServer_UpdateHandler
(
    le_avc_Status_t updateStatus,    ///< [IN] Update status
    le_avc_UpdateType_t updateType,  ///< [IN] AirVantageConnector update type
    int32_t totalNumBytes,           ///< [IN] Total number of bytes to download
    int32_t dloadProgress,           ///< [IN] Download Progress in %
    le_avc_ErrorCode_t errorCode     ///< [IN]  Error code if installation failed.
                                     ///        Applicable only when le_avc_Status_t is
                                     ///        LE_AVC_INSTALL_FAILED.
);

//--------------------------------------------------------------------------------------------------
/**
 * Function to check the user agreement for download
 */
//--------------------------------------------------------------------------------------------------
bool IsDownloadAccepted
(
    void
);

#endif // LEGATO_AVC_SERVER_INCLUDE_GUARD
