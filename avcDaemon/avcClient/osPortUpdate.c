/**
 * @file osPortUpdate.c
 *
 * Porting layer for Over The Air updates
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/update.h>
#include "packageDownloader/packageDownloader.h"
#include "legato.h"
#include "interfaces.h"
#include "avcAppUpdate/avcAppUpdate.h"
#include "avcServer/avcServer.h"
#include "tpf/tpfServer.h"
#include "avcClient.h"
#include "avcFs/avcFsConfig.h"

//--------------------------------------------------------------------------------------------------
/**
 * Size of install timer memory pool.
 */
//--------------------------------------------------------------------------------------------------
#define INSTALL_TIMER_POOL_SIZE  5

//--------------------------------------------------------------------------------------------------
/**
 * Default timer value for install request
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_INSTALL_TIMER  2

//--------------------------------------------------------------------------------------------------
/**
 * Timer to treat install request
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t TreatInstallTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Structure to treat install request timer
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    lwm2mcore_UpdateType_t type;    ///< Update type
    uint16_t instanceId;            ///< Instance Id (0 for FW, any value for SW)
}
InstallTimerData_t;

//--------------------------------------------------------------------------------------------------
/**
 * Define static pool used to pass activity timer events to the main thread.
 */
//--------------------------------------------------------------------------------------------------
LE_MEM_DEFINE_STATIC_POOL(InstallTimerPool,
                          INSTALL_TIMER_POOL_SIZE,
                          sizeof(InstallTimerData_t));

//--------------------------------------------------------------------------------------------------
/**
 * Pool used to pass install timer data
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t InstallTimerPool;

//--------------------------------------------------------------------------------------------------
/**
 * Launch update
 */
//--------------------------------------------------------------------------------------------------
static void LaunchUpdate
(
    lwm2mcore_UpdateType_t updateType,  ///< Update type (FW or SW)
    uint16_t instanceId                 ///< Instance Id (0 for FW, any value for SW)
)
{
    switch (updateType)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            LE_DEBUG("Launch FW update");
            if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_SetUpdateAccepted())
            {
                LE_ERROR("Unable to set FW update state to UPDATING");
                return;
            }
            // This function returns only if there was an error
            if (LE_OK != le_fwupdate_Install())
            {
                avcServer_UpdateStatus(LE_AVC_INSTALL_FAILED, LE_AVC_FIRMWARE_UPDATE,
                                       -1, -1, LE_AVC_ERR_INTERNAL);
                lwm2mcore_SetUpdateResult(false);
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
 * The server requires the software update state
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetSwUpdateState
(
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateStatePtr         ///< [OUT] Firmware update state
)
{
    lwm2mcore_Sid_t sid;
    if (NULL == updateStatePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK == avcApp_GetSwUpdateState(instanceId, updateStatePtr))
    {
        sid = LWM2MCORE_ERR_COMPLETED_OK;
        LE_DEBUG("updateState : %d", *updateStatePtr);
    }
    else
    {
        sid = LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return sid;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function for setting software update state
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetSwUpdateState
(
    lwm2mcore_SwUpdateState_t swUpdateState     ///< [IN] New SW update state
)
{
    le_result_t result;
    result = avcApp_SetSwUpdateState(swUpdateState);

    if (LE_OK != result)
    {
        LE_ERROR("Failed to set SW update state: %d. %s",
                 (int)swUpdateState,
                 LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function for setting software update result
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetSwUpdateResult
(
    lwm2mcore_SwUpdateResult_t swUpdateResult   ///< [IN] New SW update result
)
{
    le_result_t result = avcApp_SetSwUpdateResult(swUpdateResult);

    if (LE_OK != result)
    {
        LE_ERROR("Failed to set SW update result: %d. %s",
                 (int)swUpdateResult,
                 LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the software update result
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetSwUpdateResult
(
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateResultPtr        ///< [OUT] Firmware update result
)
{
    lwm2mcore_Sid_t sid;
    if (NULL == updateResultPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK == avcApp_GetSwUpdateResult(instanceId, updateResultPtr))
    {
        sid = LWM2MCORE_ERR_COMPLETED_OK;
        LE_DEBUG("updateResult : %d", *updateResultPtr);
    }
    else
    {
        sid = LWM2MCORE_ERR_GENERAL_ERROR;
    }
    return sid;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set legacy firmware update state
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetLegacyFwUpdateState
(
    lwm2mcore_FwUpdateState_t fwUpdateState     ///< [IN] New FW update state
)
{
    le_result_t result;

    result = WriteFs(FW_UPDATE_STATE_PATH,
                     (uint8_t*)&fwUpdateState,
                     sizeof(lwm2mcore_FwUpdateState_t));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", FW_UPDATE_STATE_PATH, LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set legacy firmware update result
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetLegacyFwUpdateResult
(
    lwm2mcore_FwUpdateResult_t fwUpdateResult   ///< [IN] New FW update result
)
{
    le_result_t result;

    result = WriteFs(FW_UPDATE_RESULT_PATH,
                     (uint8_t*)&fwUpdateResult,
                     sizeof(lwm2mcore_FwUpdateResult_t));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", FW_UPDATE_RESULT_PATH, LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get legacy firmware update state
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLegacyFwUpdateState
(
    lwm2mcore_FwUpdateState_t* fwUpdateStatePtr     ///< [INOUT] FW update state
)
{
    lwm2mcore_FwUpdateState_t updateState;
    size_t size = sizeof(lwm2mcore_FwUpdateState_t);

    if (!fwUpdateStatePtr)
    {
        LE_ERROR("Invalid input parameter");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK != ReadFs(FW_UPDATE_STATE_PATH, (uint8_t*)&updateState, &size))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("FW Update state %d", updateState);
    *fwUpdateStatePtr = updateState;

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get legacy firmware update result
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLegacyFwUpdateResult
(
    lwm2mcore_FwUpdateResult_t* fwUpdateResultPtr   ///< [INOUT] FW update result
)
{
    lwm2mcore_FwUpdateResult_t updateResult;
    size_t size = sizeof(lwm2mcore_FwUpdateResult_t);;

    if (!fwUpdateResultPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK != ReadFs(FW_UPDATE_RESULT_PATH, (uint8_t*)&updateResult, &size))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("FW Update result %d", updateResult);
    *fwUpdateResultPtr = updateResult;

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Launch the timer to treat the install request
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t LaunchInstallRequestTimer
(
    lwm2mcore_UpdateType_t type,    ///< [IN] Update type
    uint16_t instanceId             ///< [IN] Instance Id (0 for FW, any value for SW)
)
{
    le_clk_Time_t interval = { .sec = DEFAULT_INSTALL_TIMER };
    InstallTimerData_t* timerDataPtr = le_mem_ForceAlloc(InstallTimerPool);
    timerDataPtr->type = type;
    timerDataPtr->instanceId = instanceId;
    if ((LE_OK == le_timer_SetInterval(TreatInstallTimer, interval))
     && (LE_OK == le_timer_SetContextPtr(TreatInstallTimer, timerDataPtr))
     && (LE_OK == le_timer_Start(TreatInstallTimer)))
    {
        return LE_OK;
    }
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the timer for install treatment expires
 */
//--------------------------------------------------------------------------------------------------
static void TreatInstallExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    InstallTimerData_t* timerDataPtr = NULL;
    timerDataPtr = le_timer_GetContextPtr(TreatInstallTimer);
    if (!timerDataPtr)
    {
        LE_ERROR("timerDataPtr NULL");
        return;
    }
    LE_DEBUG("Timer for install: type %d, instanceId %d",
            timerDataPtr->type, timerDataPtr->instanceId);
    avcServer_QueryInstall(LaunchUpdate, timerDataPtr->type, timerDataPtr->instanceId);
    le_mem_Release(timerDataPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * The server pushes a package to the LWM2M client
 *
 * @return
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
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
 * The server requires the current package URI stored in the LWM2M client
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
 * @warning The client MUST store a parameter in non-volatile memory in order to keep in memory that
 * an install request was received and launch a timer (value could be decided by the client
 * implementation) in order to treat the install request.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
    lwm2mcore_Sid_t sid = LWM2MCORE_ERR_GENERAL_ERROR;
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
        case LWM2MCORE_SW_UPDATE_TYPE:
        {
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

            // Process the install request:
            // - return the user agreement if needed
            // - when the install is accepted or automatically launched, a 2-second
            //   timer is launched and the install process is launched when this timer expires
            avcServer_QueryInstall(LaunchUpdate, type, instanceId);
            sid = LWM2MCORE_ERR_COMPLETED_OK;
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
 * Clean the stale workspace of aborted SOTA/FOTA job
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_CleanStaleData
(
    lwm2mcore_UpdateType_t type     ///< [IN] Update type
)
{
    // Delete all unfinished/aborted SOTA/FOTA job info
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            // Delete old FOTA job info.
            packageDownloader_DeleteFwUpdateInfo();
#if LE_CONFIG_SOTA
            // Delete aborted/stale stored SOTA job info. Otherwise, they may create problem during
            // FOTA suspend resume activity.
            avcApp_DeletePackage();
#endif /* LE_CONFIG_SOTA */
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            // Delete stale FOTA job info only. No need to delete stale SOTA job info. Because for
            // SOTA, delete command is explicitly sent from server.
            packageDownloader_DeleteFwUpdateInfo();
            break;

        default:
            LE_ERROR("Unknown download type");
            break;
    }

}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the package name
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_LaunchSwUpdateUninstall
(
    uint16_t instanceId,            ///< [IN] Instance Id (any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t len                      ///< [IN] length of input buffer
)
{
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
    avcApp_SaveSwUpdateStateResult((lwm2mcore_SwUpdateState_t)updateState,
                                   (lwm2mcore_SwUpdateResult_t)updateResult);

    avcApp_SetSwUpdateInternalState(INTERNAL_STATE_UNINSTALL_REQUESTED);

    // Received new uninstallation request. Clear all query handler references. This is specially
    // needed to clear any stale query handler references of aborted stale FOTA/SOTA job.
    avcServer_ResetQueryHandlers();

    // Here we are only delisting the app. The deletion of app will be called when deletion
    // of object 9 instance is requested. But get user agreement before delisting.
    avcServer_QueryUninstall(avcApp_PrepareUninstall,
                             instanceId);

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
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
        if (LE_DUPLICATE == result)
        {
            LE_WARN("Object creation overrides instanceId %d", instanceId);
            return LWM2MCORE_ERR_COMPLETED_OK;
        }
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
 * Resume firmware install
 *
 * @return None
 */
//--------------------------------------------------------------------------------------------------
void ResumeFwInstall
(
    void
)
{
    LaunchInstallRequestTimer(LWM2MCORE_FW_UPDATE_TYPE, 0);
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Function to get the package offset on client side
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @note
 * This function is not available if @c LWM2M_EXTERNAL_DOWNLOADER compilation flag is embedded
 *
 * @note
 * When a package started to be downloaded, the client stores the downloaded data in memory.
 * When the download is suspended, LwM2MCore needs to know the package offset which is stored in
 * client side in order to resume the download to the correct offset.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_STATE if no package download is on-going
 *  - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetPackageOffsetStorage
(
    lwm2mcore_UpdateType_t  updateType,     ///< [IN] Update type
    uint64_t*               offsetPtr       ///< [IN] Package offset
)
{
    if (!offsetPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    switch (updateType)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
        {
            *offsetPtr = packageDownloader_GetResumePosition();
        }
        break;

        case LWM2MCORE_SW_UPDATE_TYPE:
        {
            size_t offset;
            // Get swupdate offset before launching the download
            avcApp_GetResumePosition((size_t *)&offset);
            LE_DEBUG("updateOffset: %zu", offset);
            *offsetPtr = (uint64_t)offset;
        }
        break;

        default:
            LE_ERROR("unknown download type");
            return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}
//--------------------------------------------------------------------------------------------------
/**
 * Get TPF mode state
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetTpfState
(
    bool*  statePtr        ///< [OUT] true if third party FOTA service is activated
)
{
    if (NULL == statePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    if (LE_OK == tpfServer_GetTpfState(statePtr))
    {
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
    return LWM2MCORE_ERR_GENERAL_ERROR;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the AVC update client sub-component.
 *
 * @note This function should be called during the initializaion phase of the AVC daemon.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_UpdateInit
(
   void
)
{
    // Create pool to report install timer events.
    InstallTimerPool = le_mem_InitStaticPool(InstallTimerPool,
                                             INSTALL_TIMER_POOL_SIZE,
                                             sizeof(InstallTimerData_t));

    TreatInstallTimer = le_timer_Create("launch timer for install treatment");
    le_timer_SetHandler(TreatInstallTimer, TreatInstallExpiryHandler);
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Function to indicate that the server reads the update result resource.
 *
 * @remark Platform adaptor function which needs to be defined on client side.
 *
 * @return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 *  - LWM2MCORE_ERR_GENERAL_ERROR on failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_UpdateResultWasNotified
(
    lwm2mcore_UpdateType_t type     ///< [IN] Update type
)
{
    bool notifRequested = false;
    le_avc_Status_t updateStatus = LE_AVC_NO_UPDATE;
    le_avc_ErrorCode_t errorCode = LE_AVC_ERR_NONE;
    le_fwupdate_UpdateStatus_t fwUpdateErrorCode = LE_FWUPDATE_UPDATE_STATUS_OK;
    le_result_t result;

    if (LWM2MCORE_FW_UPDATE_TYPE != type)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = packageDownloader_GetFwUpdateNotification(&notifRequested,
                                                       &updateStatus,
                                                       &errorCode,
                                                       &fwUpdateErrorCode);
    LE_DEBUG("notifRequested %d", notifRequested);
    if ((LE_OK == result) && (notifRequested))
    {
        result = packageDownloader_SetFwUpdateNotification(false,
                                                           LE_AVC_NO_UPDATE,
                                                           LE_AVC_ERR_NONE,
                                                           LE_FWUPDATE_UPDATE_STATUS_OK);
    }
    return (result != LE_OK) ? LWM2MCORE_ERR_GENERAL_ERROR : LWM2MCORE_ERR_COMPLETED_OK;
}
