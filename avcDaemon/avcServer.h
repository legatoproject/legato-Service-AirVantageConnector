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
 * Set the update type of the currently pending update, used only during restore
 *
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void avcServer_SetUpdateType
(
    le_avc_UpdateType_t updateType  ///< [IN]
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
    bool resume                                     ///< [IN] Is it a download resume?
);


//--------------------------------------------------------------------------------------------------
/**
 * Initializes user agreement queries of download, install and uninstall. Used after a session
 * start for SOTA resume.
 */
//--------------------------------------------------------------------------------------------------
void avcServer_InitUserAgreement
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
LE_SHARED void avcServer_UpdateHandler
(
    le_avc_Status_t updateStatus,
    le_avc_UpdateType_t updateType,
    int32_t totalNumBytes,
    int32_t dloadProgress,
    le_avc_ErrorCode_t errorCode
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
 * Function to check if the agent needs to connect to the server. For FOTA it should be called
 * only after reboot, and for SOTA it should be called after update finishes. However, this function
 * will request connection to server only if there is no session going on.
 */
//--------------------------------------------------------------------------------------------------
void avcServer_RequestConnection
(
    le_avc_UpdateType_t updateType
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

#endif // LEGATO_AVC_SERVER_INCLUDE_GUARD
