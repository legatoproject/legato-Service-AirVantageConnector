/**
 * @file osPortUpdate.c
 *
 * Porting layer for Over The Air updates
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/update.h>
#include <packageDownloader.h>
#include <packageDownloaderCallbacks.h>
#include "legato.h"
#include "interfaces.h"
#include "avcAppUpdate.h"


//--------------------------------------------------------------------------------------------------
/**
 * Launch update structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    lwm2mcore_UpdateType_t  type;       ///< update type (firmware or software)
    uint16_t                instanceId; ///< instance id (0 for firmware update)
}
LaunchUpdateCtx_t;


//--------------------------------------------------------------------------------------------------
/**
 * Timer used to launch the update
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t LaunchUpdateTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Current update context.
 */
//--------------------------------------------------------------------------------------------------
static LaunchUpdateCtx_t UpdateCtx;

//--------------------------------------------------------------------------------------------------
/**
 * Launch update
 */
//--------------------------------------------------------------------------------------------------
static void LaunchUpdate(lwm2mcore_UpdateType_t updateType, uint16_t instanceId)
{
    switch (updateType)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            LE_DEBUG("Launch FW update");
            avcServer_UpdateHandler(LE_AVC_INSTALL_IN_PROGRESS, LE_AVC_FIRMWARE_UPDATE,
                                    -1, -1, LE_AVC_ERR_NONE);
            if (LE_OK != packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_UPDATING))
            {
                LE_ERROR("Unable to set FW update state to UPDATING");
                return;
            }

            // Install pending is accepted by user. So, clear the install pending flag
            if (LE_OK != packageDownloader_SetFwUpdateInstallPending(false))
            {
                LE_ERROR("Unable to clear fw update install pending flag");
                return;
            }

            // This function returns only if there was an error
            if (LE_OK != le_fwupdate_Install())
            {
                 avcServer_UpdateHandler(LE_AVC_INSTALL_FAILED, LE_AVC_FIRMWARE_UPDATE,
                                         -1, -1, LE_AVC_ERR_INTERNAL);
                 packageDownloader_SetFwUpdateResult(LWM2MCORE_FW_UPDATE_RESULT_INSTALL_FAILURE);
            }
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            LE_DEBUG("Launch SW update");
            avcApp_StartInstall(instanceId);
            break;

        default:
            LE_ERROR("Unknown update type %u", updateType);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the install defer timer expires.
 */
//--------------------------------------------------------------------------------------------------
static void LaunchUpdateTimerExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    LaunchUpdateCtx_t* updateCtxPtr = (LaunchUpdateCtx_t*)le_timer_GetContextPtr(timerRef);

    // Notify for user agreement and launch update after user acceptance.
    if (NULL == updateCtxPtr)
    {
        LE_ERROR("Update context not available");
    }
    else
    {
        avcServer_QueryInstall(LaunchUpdate, updateCtxPtr->type, updateCtxPtr->instanceId);
    }
}

//--------------------------------------------------------------------------------------------------
/* Check if FOTA download is in progress
 *
 * @return
 *  true if FOTA download is in progress, false otherwise
 */
//--------------------------------------------------------------------------------------------------
static bool IsFotaDownloading
(
    void
)
{
    lwm2mcore_FwUpdateState_t fwUpdateState = LWM2MCORE_FW_UPDATE_STATE_IDLE;
    lwm2mcore_FwUpdateResult_t fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;

    return ((LE_OK == packageDownloader_GetFwUpdateState(&fwUpdateState))
            && (LE_OK == packageDownloader_GetFwUpdateResult(&fwUpdateResult))
            && (LWM2MCORE_FW_UPDATE_STATE_DOWNLOADING == fwUpdateState)
            && (LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL == fwUpdateResult));
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if SOTA download is in progress
 *
 * @return
 *  true if SOTA download is in progress, false otherwise
 */
//--------------------------------------------------------------------------------------------------
static bool IsSotaDownloading
(
    void
)
{
    lwm2mcore_SwUpdateState_t swUpdateState = LWM2MCORE_SW_UPDATE_STATE_INITIAL;
    lwm2mcore_SwUpdateResult_t swUpdateResult = LWM2MCORE_SW_UPDATE_RESULT_INITIAL;

    return ((LE_OK == avcApp_GetSwUpdateRestoreState(&swUpdateState))
            && (LE_OK == avcApp_GetSwUpdateRestoreResult(&swUpdateResult))
            && (LWM2MCORE_SW_UPDATE_STATE_DOWNLOAD_STARTED == swUpdateState)
            && (LWM2MCORE_SW_UPDATE_RESULT_INITIAL == swUpdateResult));
}

//--------------------------------------------------------------------------------------------------
/**
 * The server pushes a package to the LWM2M client
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
lwm2mcore_Sid_t lwm2mcore_PushUpdatePackage
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] Data buffer
    size_t len                      ///< [IN] Length of input buffer
)
{
    return LWM2MCORE_ERR_OP_NOT_SUPPORTED;
}


//--------------------------------------------------------------------------------------------------
/**
 * The server sends a package URI to the LWM2M client
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
lwm2mcore_Sid_t lwm2mcore_SetUpdatePackageUri
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] Data buffer
    size_t len                      ///< [IN] Length of input buffer
)
{
    LE_DEBUG("URI: len %d", len);

    if (0 == len)
    {
        // If length is 0, then the Update State shall be set to default value,
        // any active download is suspended and the package URI is deleted from storage file.
        if (LE_OK != packageDownloader_AbortDownload(type))
        {
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }

        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    // Parameter check
    if (   (!bufferPtr)
        || (LWM2MCORE_PACKAGE_URI_MAX_LEN < len)
        || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
    {
        LE_ERROR("lwm2mcore_UpdateSetPackageUri: bad parameter");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Package URI: LWM2MCORE_PACKAGE_URI_MAX_LEN+1 for null byte: string format
    uint8_t downloadUri[LWM2MCORE_PACKAGE_URI_MAX_LEN+1];
    memset(downloadUri, 0, LWM2MCORE_PACKAGE_URI_MAX_LEN+1);
    memcpy(downloadUri, bufferPtr, len);
    LE_DEBUG("Request to download update package from URL : %s, len %d", downloadUri, len);

    // Reset the update state
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            if (LE_OK != packageDownloader_SetFwUpdateResult(
                                                    LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL))
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            if (LE_OK != avcApp_SetSwUpdateResult(LWM2MCORE_SW_UPDATE_RESULT_INITIAL))
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

        default:
            LE_ERROR("Unknown download type");
            return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Call API to launch the package download
    if (LE_OK != packageDownloader_StartDownload(downloadUri, type, false))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the current package URI stored in the LWM2M client
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
lwm2mcore_Sid_t lwm2mcore_GetUpdatePackageUri
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] Data buffer
    size_t* lenPtr                  ///< [INOUT] Length of input buffer and length of the returned
                                    ///< data
)
{
    if ((NULL == bufferPtr) || (NULL == lenPtr) || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *lenPtr = 0;
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requests to launch an update
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
lwm2mcore_Sid_t lwm2mcore_LaunchUpdate
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] Data buffer
    size_t len                      ///< [IN] Length of input buffer
)
{
    lwm2mcore_Sid_t sid;
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
        case LWM2MCORE_SW_UPDATE_TYPE:
            {
                // Acknowledge the launch update notification and launch the update later
                UpdateCtx.type = type;
                UpdateCtx.instanceId = instanceId;

                le_clk_Time_t interval = {2, 0};
                LaunchUpdateTimer = le_timer_Create("launch update timer");
                if (   (LE_OK != le_timer_SetHandler(LaunchUpdateTimer,
                                                     LaunchUpdateTimerExpiryHandler))
                    || (LE_OK != le_timer_SetContextPtr(LaunchUpdateTimer,
                                                        (LaunchUpdateCtx_t*)&UpdateCtx))
                    || (LE_OK != le_timer_SetInterval(LaunchUpdateTimer, interval))
                    || (LE_OK != le_timer_Start(LaunchUpdateTimer))
                   )
                {
                    sid = LWM2MCORE_ERR_GENERAL_ERROR;
                }
                else
                {
                    sid = LWM2MCORE_ERR_COMPLETED_OK;
                }

                if (type == LWM2MCORE_SW_UPDATE_TYPE)
                {
                    avcApp_SetSwUpdateInternalState(INTERNAL_STATE_INSTALL_REQUESTED);
                }
                else
                {
                    if (LE_OK != packageDownloader_SetFwUpdateInstallPending(true))
                    {
                        LE_ERROR("Unable to set fw update install pending flag");
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                }
            }
            break;

        default:
            sid = LWM2MCORE_ERR_INVALID_ARG;
            break;
    }
    LE_DEBUG("LaunchUpdate type %d: %d", type, sid);
    return sid;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the update state
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
lwm2mcore_Sid_t lwm2mcore_GetUpdateState
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateStatePtr         ///< [OUT] Firmware update state
)
{
    lwm2mcore_Sid_t sid;
    if ((NULL == updateStatePtr) || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Call API to get the update state
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            if (LE_OK == packageDownloader_GetFwUpdateState(
                                                (lwm2mcore_FwUpdateState_t*)updateStatePtr))
            {
                sid = LWM2MCORE_ERR_COMPLETED_OK;
                LE_DEBUG("updateState : %d", *updateStatePtr);
            }
            else
            {
                sid = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            if (LE_OK == avcApp_GetSwUpdateState(instanceId,
                                                (lwm2mcore_SwUpdateState_t*)updateStatePtr))
            {
                sid = LWM2MCORE_ERR_COMPLETED_OK;
                LE_DEBUG("updateState : %d", *updateStatePtr);
            }
            else
            {
                sid = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

        default:
            LE_ERROR("Bad update type");
            return LWM2MCORE_ERR_INVALID_ARG;
    }
    LE_DEBUG("GetUpdateState type %d: %d", type, sid);
    return sid;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the update result
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
lwm2mcore_Sid_t lwm2mcore_GetUpdateResult
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateResultPtr        ///< [OUT] Firmware update result
)
{
    lwm2mcore_Sid_t sid;
    if ((NULL == updateResultPtr) || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Call API to get the update result
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            if (LE_OK == packageDownloader_GetFwUpdateResult(
                                                    (lwm2mcore_FwUpdateResult_t*)updateResultPtr))
            {
                sid = LWM2MCORE_ERR_COMPLETED_OK;
                packageDownloader_SetFwUpdateNotification(false);
                LE_DEBUG("updateResult : %d", *updateResultPtr);
            }
            else
            {
                sid = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            if (LE_OK == avcApp_GetSwUpdateResult(instanceId,
                                                 (lwm2mcore_SwUpdateResult_t*)updateResultPtr))
            {
                sid = LWM2MCORE_ERR_COMPLETED_OK;
                LE_DEBUG("updateResult : %d", *updateResultPtr);
            }
            else
            {
                sid = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

        default:
            LE_ERROR("Bad update type");
            return LWM2MCORE_ERR_INVALID_ARG;
    }
    LE_DEBUG("GetUpdateResult type %d: %d", type, sid);
    return sid;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the package name
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
lwm2mcore_Sid_t lwm2mcore_GetUpdatePackageName
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] Data buffer
    uint32_t len                    ///< [IN] Length of input buffer
)
{
    if (bufferPtr == NULL)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    lwm2mcore_Sid_t result;

    switch (type)
    {
        case LWM2MCORE_SW_UPDATE_TYPE:
            if (avcApp_GetPackageName(instanceId, bufferPtr, len) == LE_OK)
            {
                result = LWM2MCORE_ERR_COMPLETED_OK;
            }
            else
            {
                result = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

        default:
            LE_ERROR("Not supported for package type: %d", type);
            result = LWM2MCORE_ERR_OP_NOT_SUPPORTED;
            break;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the package version
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
lwm2mcore_Sid_t lwm2mcore_GetUpdatePackageVersion
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    uint32_t len                    ///< [IN] length of input buffer
)
{
    if (bufferPtr == NULL)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    lwm2mcore_Sid_t result;

    switch (type)
    {
        case LWM2MCORE_SW_UPDATE_TYPE:
            if (avcApp_GetPackageVersion(instanceId, bufferPtr, len) == LE_OK)
            {
                result = LWM2MCORE_ERR_COMPLETED_OK;
            }
            else
            {
                result= LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

        default:
            LE_ERROR("Not supported for package type: %d", type);
            result = LWM2MCORE_ERR_OP_NOT_SUPPORTED;
            break;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server sets the "update supported objects" field for software update
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
lwm2mcore_Sid_t lwm2mcore_SetSwUpdateSupportedObjects
(
    uint16_t instanceId,            ///< [IN] Instance Id (any value for SW)
    bool value                      ///< [IN] Update supported objects field value
)
{
    LE_DEBUG("lwm2mcore_UpdateSetSwSupportedObjects oiid %d, value %d", instanceId, value);
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the "update supported objects" field for software update
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
lwm2mcore_Sid_t lwm2mcore_GetSwUpdateSupportedObjects
(
    uint16_t instanceId,            ///< [IN] Instance Id (any value for SW)
    bool* valuePtr                  ///< [INOUT] Update supported objects field value
)
{
    if (NULL == valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        *valuePtr = true;
        LE_DEBUG("lwm2mcore_UpdateGetSwSupportedObjects, oiid %d, value %d", instanceId, *valuePtr);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * The server requires the activation state for one embedded application
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
lwm2mcore_Sid_t lwm2mcore_GetSwUpdateActivationState
(
    uint16_t instanceId,            ///< [IN] Instance Id (any value for SW)
    bool* valuePtr                  ///< [INOUT] Activation state
)
{
    if (NULL == valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    le_result_t result = avcApp_GetActivationState(instanceId, valuePtr);

    if (result == LE_OK)
    {
        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    if (result == LE_NOT_FOUND)
    {
        LE_ERROR("InstanceId: %u not found", instanceId);
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    return LWM2MCORE_ERR_GENERAL_ERROR;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires an embedded application to be uninstalled (only for software update)
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
lwm2mcore_Sid_t lwm2mcore_LaunchSwUpdateUninstall
(
    uint16_t instanceId,            ///< [IN] Instance Id (any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t len                      ///< [IN] length of input buffer
)
{
    le_result_t result;
    uint8_t updateState;
    uint8_t updateResult;

    if ((NULL == bufferPtr) && len)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Save the uninstall request in SW update workspace
    avcApp_SetSwUpdateInstanceId(instanceId);

    // Read the state of this object9 instance and save it in SW update workspace
    if (avcApp_GetSwUpdateState(instanceId, &updateState) != LE_OK)
    {
        LE_ERROR("Failed to read object9 state for instanceid %d", instanceId);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Read the result of this object 9 instance and save it in SW update workspace
    if (avcApp_GetSwUpdateResult(instanceId, &updateResult) != LE_OK)
    {
        LE_ERROR("Failed to read object9 result for instanceid %d", instanceId);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("Set the update state %d and result %d to workspace", updateState, updateResult);
    avcApp_SaveSwUpdateStateResult(updateState, updateResult);

    avcApp_SetSwUpdateInternalState(INTERNAL_STATE_UNINSTALL_REQUESTED);

    // Here we are only delisting the app. The deletion of app will be called when deletion
    // of object 9 instance is requested. But get user agreement before delisting.
    result = avcServer_QueryUninstall(avcApp_PrepareUninstall, instanceId);

    if (LE_OK == result)
    {
        LE_DEBUG("uninstall accepted");

        if (avcApp_PrepareUninstall(instanceId) != LE_OK)
        {
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }
    }
    else if (LE_BUSY == result)
    {
        LE_DEBUG("Wait for uninstall acceptance");
    }
    else
    {
        LE_ERROR("Unexpected error in Query uninstall.");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires an embedded application to be activated or deactivated (only for software
 * update)
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
lwm2mcore_Sid_t lwm2mcore_ActivateSoftware
(
    bool activation,        ///< [IN] Requested activation (true: activate, false: deactivate)
    uint16_t instanceId,    ///< [IN] Instance Id (any value for SW)
    char* bufferPtr,        ///< [INOUT] data buffer
    size_t len              ///< [IN] length of input buffer
)
{
    if ((NULL == bufferPtr) && len)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    le_result_t result;

    if (activation)
    {
        result = avcApp_StartApp(instanceId);
    }
    else
    {
        result = avcApp_StopApp(instanceId);
    }

    return (result == LE_OK) ? LWM2MCORE_ERR_COMPLETED_OK : LWM2MCORE_ERR_GENERAL_ERROR;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server request to create or delete an object instance of object 9
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
lwm2mcore_Sid_t lwm2mcore_SoftwareUpdateInstance
(
    bool create,                ///<[IN] Create (true) or delete (false)
    uint16_t instanceId         ///<[IN] Object instance Id to create or delete
)
{
    le_result_t result;
    if (create)
    {
        result = avcApp_CreateObj9Instance(instanceId);
        LE_DEBUG("Instance creation result: %s ", LE_RESULT_TXT(result));
    }
    else
    {
        result = avcApp_DeleteObj9Instance(instanceId);
        LE_DEBUG("Instance Deletion result: %s ", LE_RESULT_TXT(result));
    }

    return (result == LE_OK) ? LWM2MCORE_ERR_COMPLETED_OK : LWM2MCORE_ERR_GENERAL_ERROR;
}

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
)
{
    lwm2mcore_FwUpdateState_t fwUpdateState = LWM2MCORE_FW_UPDATE_STATE_IDLE;
    lwm2mcore_FwUpdateResult_t fwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;

    // Check if a FW update was ongoing
    if (   (LE_OK == packageDownloader_GetFwUpdateState(&fwUpdateState))
        && (LE_OK == packageDownloader_GetFwUpdateResult(&fwUpdateResult))
        && (LWM2MCORE_FW_UPDATE_STATE_UPDATING == fwUpdateState)
        && (LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL == fwUpdateResult)
       )
    {
        // Retrieve FW update result
        le_fwupdate_UpdateStatus_t fwUpdateStatus;
        lwm2mcore_FwUpdateResult_t newFwUpdateResult;
        char statusStr[LE_FWUPDATE_STATUS_LABEL_LENGTH_MAX];

        if (LE_OK != le_fwupdate_GetUpdateStatus(&fwUpdateStatus, statusStr, sizeof(statusStr)))
        {
            LE_ERROR("Error while reading the FW update status");
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }

        LE_DEBUG("Update status: %s (%d)", statusStr, fwUpdateStatus);

        // Set the update state to IDLE in all cases
        if (LE_OK != packageDownloader_SetFwUpdateState(LWM2MCORE_FW_UPDATE_STATE_IDLE))
        {
            LE_ERROR("Error while setting FW update state");
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }

        // Set the update result according to the FW update status
        if (LE_FWUPDATE_UPDATE_STATUS_OK == fwUpdateStatus)
        {
            newFwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_INSTALLED_SUCCESSFUL;
            avcServer_UpdateHandler(LE_AVC_INSTALL_COMPLETE, LE_AVC_FIRMWARE_UPDATE,
                                    -1, -1, LE_AVC_ERR_NONE);
        }
        else
        {
            le_avc_ErrorCode_t errorCode;

            newFwUpdateResult = LWM2MCORE_FW_UPDATE_RESULT_INSTALL_FAILURE;
            if (LE_FWUPDATE_UPDATE_STATUS_PARTITION_ERROR == fwUpdateStatus)
            {
                errorCode = LE_AVC_ERR_BAD_PACKAGE;
            }
            else
            {
                errorCode = LE_AVC_ERR_INTERNAL;
            }
            avcServer_UpdateHandler(LE_AVC_INSTALL_FAILED, LE_AVC_FIRMWARE_UPDATE,
                                    -1, -1, errorCode);
        }
        packageDownloader_SetFwUpdateNotification(true);
        LE_DEBUG("Set FW update result to %d", newFwUpdateResult);
        if (LE_OK != packageDownloader_SetFwUpdateResult(newFwUpdateResult))
        {
            LE_ERROR("Error while setting FW update result");
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Resume a package download if necessary
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
lwm2mcore_Sid_t lwm2mcore_ResumePackageDownload
(
    void
)
{
    uint8_t downloadUri[LWM2MCORE_PACKAGE_URI_MAX_LEN+1];
    size_t uriLen = LWM2MCORE_PACKAGE_URI_MAX_LEN+1;
    lwm2mcore_UpdateType_t updateType = LWM2MCORE_MAX_UPDATE_TYPE;
    bool downloadResume = false;
    memset(downloadUri, 0, uriLen);

    // Check if an update package URI is stored
    if (LE_OK != packageDownloader_GetResumeInfo(downloadUri, &uriLen, &updateType))
    {
        LE_DEBUG("No download to resume");
        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    LE_DEBUG("Download to resume");

    if (   (0 == strncmp(downloadUri, "", LWM2MCORE_PACKAGE_URI_MAX_LEN+1))
        || (LWM2MCORE_MAX_UPDATE_TYPE == updateType)
       )
    {
        LE_ERROR("Download to resume but no URI/updateType stored");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Check if a download was started
    if (IsFotaDownloading() || IsSotaDownloading() || IsDownloadAccepted())
    {
        downloadResume = true;
    }

    LE_INFO("downloadResume %d", downloadResume);

    // Call API to launch the package download
    if (LE_OK != packageDownloader_StartDownload(downloadUri, updateType, downloadResume))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Resume firmware install if necessary
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t ResumeFwInstall
(
    void
)
{
    le_result_t result;
    bool isFwInstallPending;

    result = packageDownloader_GetFwUpdateInstallPending(&isFwInstallPending);

    if (LE_OK != result)
    {
        LE_ERROR("Error reading FW update install pending status");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    if (isFwInstallPending)
    {
        result = avcServer_QueryInstall(LaunchUpdate, LWM2MCORE_FW_UPDATE_TYPE, 0);

        if (LE_OK == result)
        {
            LE_DEBUG("install accepted");
            LaunchUpdate(LWM2MCORE_FW_UPDATE_TYPE, 0);
        }
        else if (LE_BUSY == result)
        {
            LE_DEBUG("Wait for install acceptance");
        }
        else
        {
            LE_ERROR("Unexpected error in Query install %d", result);
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }
    }
    else
    {
        LE_DEBUG("No FW install to resume");
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Suspend a package download if necessary
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SuspendPackageDownload
(
    void
)
{
    // Suspend the download thread if there is any.
    if (LE_OK == packageDownloader_SuspendDownload())
    {
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
    return LWM2MCORE_ERR_GENERAL_ERROR;
}
