/**
 * This module implements some stubs for lwm2mcore.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/update.h>
#include <lwm2mcore/lwm2mcorePackageDownloader.h>
#include "downloader.h"

//--------------------------------------------------------------------------------------------------
/**
 * Function to send status event to the application, using the callback stored in the LwM2MCore
 * session manager
 */
//--------------------------------------------------------------------------------------------------
void smanager_SendStatusEvent
(
    lwm2mcore_Status_t status
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the package downloader.
 *
 * This function is called to initialize the package downloader: the associated workspace is
 * deleted if necessary to be able to start a new download.
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_PackageDownloaderInit
(
    void
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * Start package download
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_ARG when the package URL is not valid
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_StartPackageDownloader
(
    void*   ctxPtr      ///< [IN] Context pointer
)
{
    char     packageUriPtr[LWM2MCORE_PACKAGE_URI_MAX_BYTES];
    if (!ctxPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    memset(packageUriPtr, 0, LWM2MCORE_PACKAGE_URI_MAX_BYTES);
    snprintf(packageUriPtr, LWM2MCORE_PACKAGE_URI_MAX_LEN, "%s", "http://www.somewhere.com/1234");
    downloader_StartDownload(packageUriPtr, 0, ctxPtr);
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to abort a download
 *
 * @note
 * This function could be called by the client in order to abort a download if any issue happens
 * on client side.
 *
 * @warning
 * This function is called in a dedicated thread/task.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_STATE if no package download is on-going
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_AbortDownload
(
    void
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to suspend a download
 *
 * @note
 * This function could be called by the client in order to abort a download if any issue happens
 * on client side.
 *
 * @warning
 * This function is called in a dedicated thread/task.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_STATE if no package download is on-going
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SuspendDownload
(
    void
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get download information
 *
 * @remark Public function which can be called by the client.
 *
 * @warning
 * This function is called in a dedicated thread/task.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_ARG when at least one parameter is invalid
 *  - LWM2MCORE_ERR_INVALID_STATE if no package download is on-going
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetDownloadInfo
(
    lwm2mcore_UpdateType_t* typePtr,        ///< [OUT] Update type
    uint64_t*               packageSizePtr  ///< [OUT] Package size
)
{
    if ((!typePtr) || (!packageSizePtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Indicates that the Firmware update is accepted
 *
 * @note
 * This function is not available if @c LWM2M_EXTERNAL_DOWNLOADER compilation flag is embedded
 *
 * @return
 *      - @ref LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - @ref LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - @ref LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the request
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetUpdateAccepted
(
    void
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Indicates that the Firmware update succeeds
 *
 * @note
 * This function is not available if @c LWM2M_EXTERNAL_DOWNLOADER compilation flag is embedded
 *
 * @return
 *      - @ref LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - @ref LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - @ref LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the request
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetUpdateResult
(
    bool    isSuccess   ///< [IN] true to indicate the update success, else failure
)
{
    (void)isSuccess;
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief The server sends a package URI to the LWM2M client
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @return
 *  - @ref LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *  - @ref LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *  - @ref LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters is incorrect
 *  - @ref LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *  - @ref LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *  - @ref LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *  - @ref LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetUpdatePackageUri //TODO: intenral now
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t len                      ///< [IN] length of input buffer
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * @brief Function to check if a package download for FW update is over and if the install request
 * was not received.
 * This function can be called by the client when a connection is closed to the server, or at client
 * initialization to know if the client needs to initiate a connection to the server in order to
 * receive the FW update install request from the server (a package was fully downloaded but the
 * install request was not received).
 *
 * @remark Public function which can be called by the client.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_ARG when at least one parameter is invalid
 *  - LWM2MCORE_ERR_INVALID_STATE if no package download was ended
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_IsFwUpdateInstallWaited
(
    bool*   IsFwUpdateInstallWaitedPtr    ///< [INOUT] True if a FW update install request is waited
)
{
    (void)IsFwUpdateInstallWaitedPtr;
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Function to check if a FW update is on-going
 * This function returns true if the FW upate install was accepted (lwm2mcore_SetUpdateAccepted)
 * and before final FW update ()lwm2mcore_SetUpdateResult)
 *
 * @remark Public function which can be called by the client.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_ARG when at least one parameter is invalid
 *  - LWM2MCORE_ERR_INVALID_STATE if no package download is on-going
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_IsFwUpdateOnGoing
(
    bool*   IsFwUpdateOnGoingPtr    ///< [INOUT] True if a FW update is ongoing, false otherwise
)
{
    if (!IsFwUpdateOnGoingPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    *IsFwUpdateOnGoingPtr = false;
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to indicate that a package download/install failed on client side
 *
 * @remark Public function which can be called by the client.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetDownloadError
(
    lwm2mcore_UpdateError_t error   ///< [IN] Update error
)
{
    (void)error;
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Process the downloaded data.
 *
 * Downloaded data should be sequentially transmitted to the package downloader with this function.
 *
 * @return
 *  - DWL_OK    The function succeeded
 *  - DWL_FAULT The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t lwm2mcore_PackageDownloaderReceiveData
(
    uint8_t*    bufPtr,     ///< [IN] Received data
    size_t      bufSize,    ///< [IN] Size of received data
    void*       opaquePtr   ///< [IN] Opaque pointer
)
{
    (void)opaquePtr;

    // Check downloaded buffer
    if (!bufPtr)
    {
        return DWL_FAULT;
    }

    if (!bufSize)
    {
        return DWL_OK;
    }

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Request a download retry.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_GENERAL_ERROR if unable to request a retry
 *  - LWM2MCORE_ERR_RETRY_FAILED if retry attempt failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_RequestDownloadRetry
(
    void
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Handle package download state machine
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_ARG when the package URL is not valid
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_HandlePackageDownloader
(
    void
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}