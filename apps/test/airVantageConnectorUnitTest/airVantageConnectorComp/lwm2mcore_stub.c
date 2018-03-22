/**
 * This module implements some stubs for lwm2mcore.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 *  lwm2mcore events (session and package download)
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Status_t Status = {0};
static lwm2mcore_StatusCb_t EventCb;

//--------------------------------------------------------------------------------------------------
/**
 * Simulate a new Lwm2m event
 */
//--------------------------------------------------------------------------------------------------
void le_avcTest_SimulateLwm2mEvent
(
    lwm2mcore_StatusType_t status,   ///< Event
    lwm2mcore_PkgDwlType_t pkgType,  ///< Package type.
    uint32_t numBytes,               ///< For package download, num of bytes to be downloaded
    uint32_t progress                ///< For package download, package download progress in %
)
{
    LE_INFO("SimulateLwm2mEvent");
    // Update lwm2mcore status
    Status.event =status;

    // Update package type and package info
    Status.u.pkgStatus.pkgType = pkgType;
    Status.u.pkgStatus.numBytes = numBytes;
    Status.u.pkgStatus.progress = progress;

    lwm2mcore_Status_t *statusPtr = &Status;
    EventCb(Status);

}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the LWM2M core
 *
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Ref_t lwm2mcore_Init
(
    lwm2mcore_StatusCb_t eventCb    ///< [IN] event callback
)
{
    if (NULL == eventCb)
    {
        LE_ERROR("Handler function is NULL !");
        return NULL;
    }

    EventCb = eventCb;

    return (lwm2mcore_Ref_t)(0x1009);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the lifetime in the server object and save to disk.
 *
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetLifetime
(
    uint32_t lifetime               ///< [IN] Lifetime in seconds
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to read the lifetime from the server object.
 *
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLifetime
(
    uint32_t* lifetimePtr           ///< [OUT] Lifetime in seconds
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to know what is the current connection
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_ConnectionGetType
(
    lwm2mcore_Ref_t instanceRef,    ///< [IN] instance reference
    bool* isDeviceManagement        ///< [INOUT] Session type (false: bootstrap,
                                    ///< true: device management)
)
{
    *isDeviceManagement = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Adaptation function for timer state
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_TimerIsRunning
(
    lwm2mcore_TimerType_t timer    ///< [IN] Timer Id
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to close a connection
 *
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_Disconnect
(
    lwm2mcore_Ref_t instanceRef     ///< [IN] instance reference
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Free the LWM2M core
 *
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_Free
(
    lwm2mcore_Ref_t instanceRef     ///< [IN] instance reference
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to send an update message to the Device Management server.
 *
 * This API can be used when the application wants to send a notification or during a firmware/app
 * update in order to be able to fully treat the scheduled update job
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_Update
(
    lwm2mcore_Ref_t instanceRef     ///< [IN] instance reference
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to initiate a connection
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_Connect
(
    lwm2mcore_Ref_t instanceRef     ///< [IN] instance reference
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if the update state/result should be changed after a FW install
 * and update them if necessary
 *
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetFirmwareUpdateInstallResult
(
    void
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the status of credentials provisioned on the device
 *
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_CredentialStatus_t lwm2mcore_GetCredentialStatus
(
    void
)
{
    return LWM2MCORE_DM_CREDENTIAL_PROVISIONED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to push data to lwm2mCore
 *
 * @return
 *      - LWM2MCORE_PUSH_INITIATED if data push transaction is initiated
 *      - LWM2MCORE_PUSH_FAILED if data push transaction failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_PushResult_t lwm2mcore_Push
(
    lwm2mcore_Ref_t instanceRef,            ///< [IN] instance reference
    uint8_t* payloadPtr,                    ///< [IN] payload
    size_t payloadLength,                   ///< [IN] payload length
    lwm2mcore_PushContent_t content,        ///< [IN] content type
    uint16_t* midPtr                        ///< [OUT] message id
)
{
    return LWM2MCORE_PUSH_INITIATED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to notify LwM2MCore of supported object instance list for software and asset data
 *
 * @return
 *      - true if the list was successfully treated
 *      - else false
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_UpdateSwList
(
    lwm2mcore_Ref_t instanceRef,    ///< [IN] Instance reference (Set to NULL if this API is used if
                                    ///< lwm2mcore_init API was no called)
    const char* listPtr,            ///< [IN] Formatted list
    size_t listLen                  ///< [IN] Size of the update list
)
{
    return true;
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
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Register the object table and service API
 *
 * @note If handlerPtr parameter is NULL, LwM2MCore registers it's own "standard" object list
 *
 * @return
 *      - number of registered objects
 */
//--------------------------------------------------------------------------------------------------
uint16_t lwm2mcore_ObjectRegister
(
    lwm2mcore_Ref_t instanceRef,             ///< [IN] instance reference
    char* endpointPtr,                       ///< [IN] Device endpoint
    lwm2mcore_Handler_t* const handlerPtr,   ///< [IN] List of supported object/resource by client
    void * const servicePtr                  ///< [IN] Client service API table
)
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read a resource from the object table
 *
 * @return
 *      - true if resource is found and read succeeded
 *      - else false
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_ResourceRead
(
    uint16_t objectId,                 ///< [IN] object identifier
    uint16_t objectInstanceId,         ///< [IN] object instance identifier
    uint16_t resourceId,               ///< [IN] resource identifier
    uint16_t resourceInstanceId,       ///< [IN] resource instance identifier
    char*    dataPtr,                  ///< [OUT] Array of requested resources to be read
    size_t*  dataSizePtr               ///< [IN/OUT] Size of the array
)
{
    return true;
}

