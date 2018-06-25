/**
 * @file avcServer.c
 *
 * This is a stubbed version of the file avcServer.c which handles AirVantage Controller Daemon
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "avcServer.h"
#include "main.h"

//--------------------------------------------------------------------------------------------------
/**
 * Send update status notifications to AVC server
 */
//--------------------------------------------------------------------------------------------------
void avcServer_UpdateStatus
(
    le_avc_Status_t updateStatus,                ///< Update status
    le_avc_UpdateType_t updateType,              ///< Update type
    int32_t totalNumBytes,                       ///< Total number of bytes to download (-1 if not set)
    int32_t dloadProgress,                       ///< Download Progress in percent (-1 if not set)
    le_avc_ErrorCode_t errorCode,                ///< Error code
    le_avc_StatusHandlerFunc_t statusHandlerPtr, ///< Pointer on handler function
    void* contextPtr                             ///< Context
)
{
    DownloadResult_t result;

    LE_DEBUG("Stub");

    if (updateStatus == LE_AVC_DOWNLOAD_COMPLETE)
    {
        result.updateStatus = updateStatus;
        result.updateType = updateType;
        result.totalNumBytes = totalNumBytes;
        result.dloadProgress = dloadProgress;
        result.errorCode = errorCode;
        NotifyCompletion(&result);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Resets user agreement query handlers of download, install and uninstall. This also stops
 * corresponding defer timers.
 */
//--------------------------------------------------------------------------------------------------
void avcServer_ResetQueryHandlers
(
    void
)
{
    LE_DEBUG("Stub");
}


//--------------------------------------------------------------------------------------------------
/**
 * Reset the stored download agreement
 */
//--------------------------------------------------------------------------------------------------
void avcServer_ResetDownloadAgreement
(
    void
)
{
    LE_DEBUG("Stub");
}


//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with an application uninstall
 *
 * If an uninstall can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with an uninstall. Note that handlerRef will be called at most once.
 *
 * @return
 *      - LE_OK if uninstall can proceed right away (handlerRef will not be called)
 *      - LE_BUSY if handlerRef will be called later to notify when uninstall can proceed
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
void avcServer_QueryInstall
(
    avcServer_InstallHandlerFunc_t handlerRef,  ///< [IN] Handler to receive install response.
    lwm2mcore_UpdateType_t type,                ///< [IN] Update type.
    uint16_t instanceId                         ///< [IN] Instance id.
)
{
    LE_DEBUG("Stub");
}

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
void avcServer_QueryUninstall
(
    avcServer_UninstallHandlerFunc_t handlerRef,  ///< [IN] Handler to receive install response.
    uint16_t instanceId                           ///< Instance Id (0 for FW, any value for SW)
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a package download
 *
 * If a download can't proceed right away, then the handlerRef function will be called when it is
 * okay to proceed with a download. Note that handlerRef will be called at most once.
 *
 * @return
 *      - LE_OK if download can proceed right away (handlerRef will not be called)
 *      - LE_BUSY if handlerRef will be called later to notify when download can proceed
 *      - LE_FAULT on error
 */
//--------------------------------------------------------------------------------------------------
void avcServer_QueryDownload
(
    avcServer_DownloadHandlerFunc_t handlerFunc,    ///< [IN] Download handler function
    uint64_t bytesToDownload,                       ///< [IN] Number of bytes to download
    lwm2mcore_UpdateType_t type,                    ///< [IN] Update type
    char* uriPtr,                                   ///< [IN] Update package URI
    bool resume,                                    ///< [IN] Is it a download resume?
    le_avc_StatusHandlerFunc_t statusHandlerPtr,    ///< Pointer on handler function
    void*                      contextPtr           ///< Context
)
{
    LE_DEBUG("Stub");
    handlerFunc(uriPtr, type, resume);
}

//--------------------------------------------------------------------------------------------------
/**
 * Component entry function
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
}
