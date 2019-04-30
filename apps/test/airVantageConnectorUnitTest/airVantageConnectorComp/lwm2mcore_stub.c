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
static lwm2mcore_StatusCb_t EventCb = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Simulated last HTTP(S) error code
 */
//--------------------------------------------------------------------------------------------------
static uint16_t HttpErrorCode = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Simulated lifetime
 */
//--------------------------------------------------------------------------------------------------
uint32_t Lifetime = LWM2MCORE_LIFETIME_VALUE_DISABLED;

//--------------------------------------------------------------------------------------------------
/**
 * Simulate a new Lwm2m event
 */
//--------------------------------------------------------------------------------------------------
void le_avcTest_SimulateLwm2mEvent
(
    lwm2mcore_StatusType_t status,   ///< Event
    lwm2mcore_UpdateType_t pkgType,  ///< Package type
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
    if(!EventCb)
    {
        LE_INFO("EventCb NULL");
        return;
    }
    EventCb(Status);

}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Set an event handler for LWM2M core events
 *
 * @note The handler can also be set using @ref lwm2mcore_Init function.
 * @ref lwm2mcore_Init function is called before initiating a connection to any LwM2M server.
 * @ref lwm2mcore_SetEventHandler function is called at device boot in order to receive events.
 *
 * @return
 *  - true on success
 *  - false on failure
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_SetEventHandler
(
    lwm2mcore_StatusCb_t eventCb    ///< [IN] event callback
)
{
    if (NULL == eventCb)
    {
        return false;
    }
    EventCb = eventCb;
    return true;
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
    Lifetime = lifetime;
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
    if(!lifetimePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    *lifetimePtr = Lifetime;
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the module identity (IMEI)
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetDeviceImei
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
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
 * @brief Function to close a connection. A deregister message is first sent to the server. After
 *        the end of its treatement, the connection with the server is closed.
 *
 * @note The deregister procedure may take several seconds.
 *
 * @return
 *      - @c true if the treatment is launched
 *      - else @c false
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_DisconnectWithDeregister
(
    lwm2mcore_Ref_t instanceRef     ///< [IN] instance reference
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Function to close a connection with the server without initiating a deregister procedure.
 *        It is the case when the data connection is lost.
 *
 * @return
 *      - @c true if the treatment is launched
 *      - else @c false
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
 *      - LWM2MCORE_PUSH_BUSY if state machine is busy doing a block transfer
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

//--------------------------------------------------------------------------------------------------
/**
 * Read a resource from the object table
 *
 * @return
 *      - true if resource is found and read succeeded
 *      - else false
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_ResourceWrite
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

//--------------------------------------------------------------------------------------------------
/**
 * Esecute a resource from the object table
 *
 * @return
 *      - true if resource is found and read succeeded
 *      - else false
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_ResourceExec
(
    uint16_t objectId,                 ///< [IN] object identifier
    uint16_t objectInstanceId,         ///< [IN] object instance identifier
    uint16_t resourceId,               ///< [IN] resource identifier
    uint16_t resourceInstanceId,       ///< [IN] resource instance identifier
    char*    dataPtr,                  ///< [IN] Array of requested resources to be write
    size_t*  dataSizePtr               ///< [IN/OUT] Size of the array
)
{
    return true;
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
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Function to get download information
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

    *typePtr = LWM2MCORE_FW_UPDATE_TYPE;
    *packageSizePtr = 0;

    return LWM2MCORE_ERR_COMPLETED_OK;
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
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete the package downloader resume info.
 *
 * This function is called to delete resume related information from the package downloader
 * workspace.
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_DeletePackageDownloaderResumeInfo
(
    void
)
{
    return;
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
 * Perform base64 data encoding.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 *      - LWM2MCORE_ERR_OVERFLOW if buffer overflow occurs
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_Base64Encode
(
    const uint8_t*  src,    ///< [IN] Data to be encoded
    size_t          srcLen, ///< [IN] Data length
    char*           dst,    ///< [OUT] Base64-encoded string buffer
    size_t*         dstLen  ///< [INOUT] Length of the base64-encoded string buffer
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Decode base64-encoded data.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 *      - LWM2MCORE_ERR_OVERFLOW if buffer overflow occurs
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if incorrect data range
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_Base64Decode
(
    char*       src,    ///< [IN] Base64-encoded data string
    uint8_t*    dst,    ///< [OUT] Decoded data buffer
    size_t*     dstLen  ///< [INOUT] Decoded data buffer length
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Compute HMAC SHA256 digest using the given data and credential.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_ComputeHmacSHA256
(
    uint8_t*                data,           ///< [IN] Data buffer
    size_t                  dataLen,        ///< [IN] Data length
    lwm2mcore_Credentials_t credId,         ///< [IN] Key type
    uint8_t*                result,         ///< [OUT] Digest buffer
    size_t*                 resultLenPtr    ///< [INOUT] Digest buffer length
)
{
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
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get the last HTTP(S) error code on a package download.
 *
 * Public function which can be called by the client.
 *
 * @note
 * This function is not available if @c LWM2M_EXTERNAL_DOWNLOADER compilation flag is embedded
 *
 * @note
 * If a package download error happens, this funciton could be called in order to get the last
 * HTTP(S) error code related to the package download from device startup.
 * This function only concerns the package download.
 * The value is not persitent to reset.
 * If no package download was made, the error code is set to 0.
 *
 * return
 *  - LWM2MCORE_ERR_COMPLETED_OK on success
 *  - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLastHttpErrorCode
(
    uint16_t*   errorCode       ///< [IN] HTTP(S) error code
)
{
    if (!errorCode)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *errorCode = HttpErrorCode;
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize memory areas for Lwm2m
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_InitMem
(
    void
)
{
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
    bool*  stateptr        ///< [OUT] the third party FOTA service state
)
{
    return LWM2MCORE_ERR_COMPLETED_OK;
}
