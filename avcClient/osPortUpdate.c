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

//--------------------------------------------------------------------------------------------------
/**
 * The server pushes a package to the LWM2M client
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
lwm2mcore_sid_t os_portUpdatePushPackage
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Intance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t len                      ///< [IN] length of input buffer
)
{
    return LWM2MCORE_ERR_OP_NOT_SUPPORTED;
}


//--------------------------------------------------------------------------------------------------
/**
 * The server sends a package URI to the LWM2M client
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
lwm2mcore_sid_t os_portUpdateSetPackageUri
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Intance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t len                      ///< [IN] length of input buffer
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
            sid = LWM2MCORE_ERR_COMPLETED_OK;
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
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
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
    uint16_t instanceId,            ///< [IN] Intance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t* lenPtr                  ///< [INOUT] length of input buffer and length of the returned
                                    ///< data
)
{
    lwm2mcore_sid_t sid;

    if ((NULL == bufferPtr) || (NULL == lenPtr) || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
    {
        sid = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        sid = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
    }
    LE_DEBUG("GetPackageUri : %d", sid);
    return sid;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requests to launch an update
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
lwm2mcore_sid_t os_portUpdateLaunchUpdate
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Intance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t len                      ///< [IN] length of input buffer
)
{
    lwm2mcore_sid_t sid;
    if (LWM2MCORE_MAX_UPDATE_TYPE <= type)
    {
        sid = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        /* Call API to launch the package update */
        sid = LWM2MCORE_ERR_COMPLETED_OK;
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
    uint16_t instanceId,            ///< [IN] Intance Id (0 for FW, any value for SW)
    uint8_t* updateStatePtr         ///< [OUT] Firmware update state
)
{
    lwm2mcore_sid_t sid;
    if ((NULL == updateStatePtr) || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
    {
        sid = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        /* Call API to get the update state
         * For the moment, hard-coded to LWM2MCORE_FW_UPDATE_STATE_IDLE
         */
        if (LWM2MCORE_FW_UPDATE_TYPE == type)
        {
            /* Firmware update */
            *updateStatePtr = LWM2MCORE_FW_UPDATE_STATE_IDLE;
            sid = LWM2MCORE_ERR_COMPLETED_OK;
            LE_DEBUG("updateState : %d", *updateStatePtr);
        }
        else
        {
            /* Software update */
            sid = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
        }
    }
    LE_DEBUG("GetUpdateState type %d: %d", type, sid);
    return sid;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the update result
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
lwm2mcore_sid_t os_portUpdateGetUpdateResult
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Intance Id (0 for FW, any value for SW)
    uint8_t* updateResultPtr        ///< [OUT] Firmware update result
)
{
    lwm2mcore_sid_t sid;
    if ((NULL == updateResultPtr) || (LWM2MCORE_MAX_UPDATE_TYPE <= type))
    {
        sid = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        /* Call API to get the update result
         * For the moment, hard-coded to LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL
         */
        if (LWM2MCORE_FW_UPDATE_TYPE == type)
        {
            /* Firmware update */
            *updateResultPtr = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;
            LE_DEBUG("updateState : %d", *updateResultPtr);
            sid = LWM2MCORE_ERR_COMPLETED_OK;
        }
        else
        {
            /* Software update */
            sid = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
        }
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
lwm2mcore_sid_t os_portUpdateGetPackageName
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Intance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t* lenPtr                  ///< [INOUT] length of input buffer and length of the returned
                                    ///< data
)
{
    return LWM2MCORE_ERR_OP_NOT_SUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * The server requires the package version
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
lwm2mcore_sid_t os_portUpdateGetPackageVersion
(
    lwm2mcore_updateType_t type,    ///< [IN] Update type
    uint16_t instanceId,            ///< [IN] Intance Id (0 for FW, any value for SW)
    char* bufferPtr,                ///< [INOUT] data buffer
    size_t* lenPtr                  ///< [INOUT] length of input buffer and length of the returned
                                    ///< data
)
{
    return LWM2MCORE_ERR_OP_NOT_SUPPORTED;
}

