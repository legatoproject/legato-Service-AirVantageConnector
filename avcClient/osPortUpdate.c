/**
 * @file osPortUpdate.c
 *
 * Porting layer for Over The Air updates
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "osPortUpdate.h"
#include <packageDownloader.h>

//--------------------------------------------------------------------------------------------------
/**
 * Timer used to launch the update
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t LaunchUpdateTimer;

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
    lwm2mcore_updateType_t updateType;

    updateType = (lwm2mcore_updateType_t)le_timer_GetContextPtr(timerRef);

    switch (updateType)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            LE_DEBUG("Launch FW update");
            packageDownloader_SetFwUpdateStateModified(LWM2MCORE_FW_UPDATE_STATE_UPDATING);
            le_fwupdate_DualSysSwap();
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            LE_DEBUG("Launch SW update");
            break;

        case LWM2MCORE_MAX_UPDATE_TYPE:
            LE_DEBUG("Launch internal update");
            break;

        default:
            LE_ERROR("Unknown update type %u", updateType);
            break;
    }
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
lwm2mcore_sid_t os_portUpdatePushPackage
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
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
lwm2mcore_sid_t os_portUpdateSetPackageUri
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] Data buffer
    size_t len                      ///< [IN] Length of input buffer
)
{
    lwm2mcore_sid_t sid;
    LE_DEBUG("URI : len %d", len);

    if (0 == len)
    {
        /* If len is 0, then :
         * the Update State shall be set to default value
         * the package URI is deleted from storage file
         * any active download is suspended
         */
        packageDownloader_AbortDownload(type);
        sid = LWM2MCORE_ERR_COMPLETED_OK;
    }
    else
    {
        /* Parameter check */
        if ((!bufferPtr)
         || (LWM2MCORE_PACKAGE_URI_MAX_LEN < len)
         || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
        {
            LE_INFO("os_portUpdateWritePackageUri : bad param");
            sid = LWM2MCORE_ERR_INVALID_ARG;
        }
        else
        {
            /* Package URI: LWM2MCORE_PACKAGE_URI_MAX_LEN+1 for null byte: string format */
            uint8_t downloadUri[LWM2MCORE_PACKAGE_URI_MAX_LEN+1];
            memset(downloadUri, 0, LWM2MCORE_PACKAGE_URI_MAX_LEN+1);
            memcpy(downloadUri, bufferPtr, len);

            LE_DEBUG("Request to download firmware update from URL : %s, len %d", downloadUri, len);
            /* Call API to launch the package download */
            if (LE_FAULT == packageDownloader_StartDownload(downloadUri, type))
            {
                sid = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            else
            {
                sid = LWM2MCORE_ERR_COMPLETED_OK;
            }
        }
    }
    LE_DEBUG("SetPackageUri type %d: %d", type, sid);
    return sid;
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
lwm2mcore_sid_t os_portUpdateGetPackageUri
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
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
lwm2mcore_sid_t os_portUpdateLaunchUpdate
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] Data buffer
    size_t len                      ///< [IN] Length of input buffer
)
{
    lwm2mcore_sid_t sid;
    if (LWM2MCORE_MAX_UPDATE_TYPE <= type)
    {
        sid = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        // Acknowledge the launch update notification and launch the update later
        le_clk_Time_t interval = {2, 0};
        LaunchUpdateTimer = le_timer_Create("launch update timer");
        if (   (LE_OK != le_timer_SetHandler(LaunchUpdateTimer, LaunchUpdateTimerExpiryHandler))
            || (LE_OK != le_timer_SetContextPtr(LaunchUpdateTimer, (void*)type))
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
lwm2mcore_sid_t os_portUpdateGetUpdateState
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateStatePtr         ///< [OUT] Firmware update state
)
{
    lwm2mcore_sid_t sid;
    if ((NULL == updateStatePtr) || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    /* Call API to get the update state */
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            if (LE_OK == packageDownloader_GetFwUpdateState(
                                                        (lwm2mcore_fwUpdateState_t*)updateStatePtr))
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
            if (LE_OK == packageDownloader_GetSwUpdateState(
                                                        (lwm2mcore_swUpdateState_t*)updateStatePtr))
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
lwm2mcore_sid_t os_portUpdateGetUpdateResult
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateResultPtr        ///< [OUT] Firmware update result
)
{
    lwm2mcore_sid_t sid;
    if ((NULL == updateResultPtr) || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    /* Call API to get the update result */
    switch (type)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            if (LE_OK == packageDownloader_GetFwUpdateResult(
                                                    (lwm2mcore_fwUpdateResult_t*)updateResultPtr))
            {
                sid = LWM2MCORE_ERR_COMPLETED_OK;
                LE_DEBUG("updateState : %d", *updateResultPtr);
            }
            else
            {
                sid = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            if (LE_OK == packageDownloader_GetSwUpdateResult(
                                                    (lwm2mcore_swUpdateResult_t*)updateResultPtr))
            {
                sid = LWM2MCORE_ERR_COMPLETED_OK;
                LE_DEBUG("updateState : %d", *updateResultPtr);
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

/* For test only: will be removed with Legato development*/
#define APPLICATION "application"
#define VERSION "version"

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
lwm2mcore_sid_t os_portUpdateGetPackageName
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    uint32_t len                    ///< [IN] length of input buffer
)
{
    LE_INFO("os_portUpdateGetPackageName len %d", len);
    memset(bufferPtr, 0, len);
    snprintf(bufferPtr, len, "%s%d", APPLICATION, instanceId);
    os_debug_data_dump ("application name", bufferPtr, strlen(bufferPtr));
    return LWM2MCORE_ERR_COMPLETED_OK;
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
lwm2mcore_sid_t os_portUpdateGetPackageVersion
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    uint32_t len                    ///< [IN] length of input buffer
)
{
    LE_INFO("os_portUpdateGetPackageVersion len %d", len);
    memset(bufferPtr, 0, len);
    snprintf(bufferPtr, len, "%s%d", VERSION, instanceId);
    os_debug_data_dump ("application version",bufferPtr, strlen(bufferPtr));
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server sets the "update supported objects" field for software update
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portUpdateSetSwSupportedObjects
(
    uint16_t instanceId,            ///< [IN] Intance Id (any value for SW)
    bool value                      ///< [IN] Update supported objects field value
)
{
    LE_INFO("os_portUpdateSetSwSupportedObjects oiid %d, value %d", instanceId, value);
    /* TODO: add Legato treatment */
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the "update supported objects" field for software update
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portUpdateGetSwSupportedObjects
(
    uint16_t instanceId,            ///< [IN] Intance Id (any value for SW)
    bool* valuePtr                  ///< [INOUT] Update supported objects field value
)
{
    if (NULL == valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        /* TODO: add Legato treatment */
        *valuePtr = true;
        LE_INFO("os_portUpdateGetSwSupportedObjects, oiid %d, value %d", instanceId, *valuePtr);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * The server requires the activation state for one embedded application
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portUpdateGetSwActivationState
(
    uint16_t instanceId,            ///< [IN] Intance Id (any value for SW)
    bool* valuePtr                  ///< [INOUT] Activation state
)
{
    if (NULL == valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        /* TODO: add Legato treatment */
        *valuePtr = true;
        LE_INFO("os_portUpdateGetSwActivationState oiid %d, value %d", instanceId, *valuePtr);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires an embedded application to be uninstalled (only for software update)
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portUpdateLaunchSwUninstall
(
    uint16_t instanceId,            ///< [IN] Intance Id (any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t len                      ///< [IN] length of input buffer
)
{
    if ((NULL == bufferPtr) && len)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        /* TODO: add Legato treatment */
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires an embedded application to be activated or deactivated (only for software
 * update)
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portUpdateActivateSoftware
(
    bool activation,        ///< [IN] Requested activation (true: activate, false: deactivate)
    uint16_t instanceId,    ///< [IN] Intance Id (any value for SW)
    char* bufferPtr,        ///< [INOUT] data buffer
    size_t len              ///< [IN] length of input buffer
)
{
    if ((NULL == bufferPtr) && len)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        /* TODO: add Legato treatment */
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * The server request to create or delete an object instance of object 9
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portUpdateSoftwareInstance
(
    bool create,                ///<[IN] Create (true) or delete (false)
    uint16_t instanceId         ///<[IN] Object instance Id to create or delete
)
{
    lwm2mcore_sid_t sID = LWM2MCORE_ERR_COMPLETED_OK;
    /* TODO: add Legato treatment */
    LE_INFO("os_portUpdateSoftwareInstance create %d, Oiid %d", create, instanceId);
    return sID;
}

