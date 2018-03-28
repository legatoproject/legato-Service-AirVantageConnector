/**
 * This module implements some stubs for avData unit tests.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "liblwm2m.h"
#include "coapHandlers.h"
#include "cbor.h"

//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionRef_t le_avdata_GetClientSessionRef
(
    void
)
{
    return (le_msg_SessionRef_t)0x1001;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the server service reference
 */
//--------------------------------------------------------------------------------------------------
le_msg_ServiceRef_t le_avdata_GetServiceRef
(
    void
)
{
    return (le_msg_ServiceRef_t)0x1002;
}

//--------------------------------------------------------------------------------------------------
/**
 * Registers a function to be called whenever one of this service's sessions is closed by
 * the client. (STUBBED FUNCTION)
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionEventHandlerRef_t MyAddServiceCloseHandler
(
    le_msg_ServiceRef_t             serviceRef, ///< [in] Reference to the service.
    le_msg_SessionEventHandler_t    handlerFunc,///< [in] Handler function.
    void*                           contextPtr  ///< [in] Opaque pointer value to pass to handler.
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add service open handler stub
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionEventHandlerRef_t MyAddServiceOpenHandler
(
    le_msg_ServiceRef_t               serviceRef,
    le_msg_SessionEventHandler_t      handlerFunc,
    void                              *contextPtr
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Fetches the user credentials of the client at the far end of a given IPC session.
 *
 * @warning This function can only be called for the server-side of a session.
 *
 * @return LE_OK if successful.
 *         LE_CLOSED if the session has closed.
 **/
//--------------------------------------------------------------------------------------------------
le_result_t MsgGetClientUserCreds
(
    le_msg_SessionRef_t sessionRef,   ///< [IN] Reference to the session.
    uid_t*              userIdPtr,    ///< [OUT] Ptr to where the uid is to be stored on success.
    pid_t*              processIdPtr  ///< [OUT] Ptr to where the pid is to be stored on success.
)
{
    *userIdPtr = 0;
    *processIdPtr = 0;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
// Config Tree service stubbing
//--------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------
/**
 * le_cfg_CreateReadTxn() stub.
 */
// -------------------------------------------------------------------------------------------------
le_cfg_IteratorRef_t le_cfg_CreateReadTxn
(
    const char* basePath
        ///< [IN]
        ///< Path to the location to create the new iterator.
)
{
    return NULL;
}

// -------------------------------------------------------------------------------------------------
/**
 * le_cfg_CreateWriteTxn() stub.
 */
// -------------------------------------------------------------------------------------------------
le_cfg_IteratorRef_t le_cfg_CreateWriteTxn
(
    const char*    basePath
        ///< [IN]
        ///< Path to the location to create the new iterator.
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_CommitTxn() stub.
 */
//--------------------------------------------------------------------------------------------------
void le_cfg_CommitTxn
(
    le_cfg_IteratorRef_t iteratorRef
        ///< [IN]
        ///< Iterator object to commit.
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_CancelTxn() stub.
 */
//--------------------------------------------------------------------------------------------------
void le_cfg_CancelTxn
(
    le_cfg_IteratorRef_t iteratorRef
        ///< [IN]
        ///< Iterator object to close.
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_NodeExists() stub.
 */
//--------------------------------------------------------------------------------------------------
bool le_cfg_NodeExists
(
    le_cfg_IteratorRef_t iteratorRef,
        ///< [IN]
        ///< Iterator to use as a basis for the transaction.

    const char* path
        ///< [IN]
        ///< Path to the target node. Can be an absolute path, or
        ///< a path relative from the iterator's current position.
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_GetString() stub.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_cfg_GetString
(
    le_cfg_IteratorRef_t iteratorRef,
        ///< [IN]
        ///< Iterator to use as a basis for the transaction.

    const char* path,
        ///< [IN]
        ///< Path to the target node. Can be an absolute path,
        ///< or a path relative from the iterator's current
        ///< position.

    char* value,
        ///< [OUT]
        ///< Buffer to write the value into.

    size_t valueNumElements,
        ///< [IN]

    const char* defaultValue
        ///< [IN]
        ///< Default value to use if the original can't be
        ///<   read.
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_GetInt() stub.
 */
//--------------------------------------------------------------------------------------------------
int32_t le_cfg_GetInt
(
    le_cfg_IteratorRef_t iteratorRef,
        ///< [IN]
        ///< Iterator to use as a basis for the transaction.

    const char* path,
        ///< [IN]
        ///< Path to the target node. Can be an absolute path, or
        ///< a path relative from the iterator's current position.

    int32_t defaultValue
        ///< [IN]
        ///< Default value to use if the original can't be
        ///<   read.
)
{
    return 1;
}
//--------------------------------------------------------------------------------------------------
/**
 * Reads a 64-bit floating point value from the config tree.
 *
 * If the value is an integer then the value will be promoted to a float. Otherwise, if the
 * underlying value is not a float or integer, the default value will be returned.
 *
 * If the path is empty, the iterator's current node will be read.
 *
 * @note Floating point values will only be stored up to 6 digits of precision.
 */
//--------------------------------------------------------------------------------------------------
  double le_cfg_GetFloat
(
    le_cfg_IteratorRef_t iteratorRef, ///< [IN] Iterator to use as a basis for the transaction.
    const char* LE_NONNULL path,      ///< [IN] Path to the target node. Can be an absolute path, or
                                       ///< a path relative from the iterator's current position.
    double defaultValue                ///< [IN] Default value to use if the original can't be read.
)
{
    return 1.1;
}
//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_SetInt() stub.
 */
//--------------------------------------------------------------------------------------------------
void le_cfg_SetInt
(
    le_cfg_IteratorRef_t iteratorRef,
        ///< [IN]
        ///< Iterator to use as a basis for the transaction.

    const char* path,
        ///< [IN]
        ///< Full or relative path to the value to write.

    int32_t value
        ///< [IN]
        ///< Value to write.
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_GetBool() stub.
 */
//--------------------------------------------------------------------------------------------------
bool le_cfg_GetBool
(
    le_cfg_IteratorRef_t iteratorRef,   ///< [IN] Iterator to use as a basis for the transaction.
    const char* path,                   ///< [IN] Path to the target node. Can be an absolute path,
                                        ///<      or a path relative from the iterator's current
                                        ///<      position
    bool defaultValue                   ///< [IN] Default value to use if the original can't be
                                        ///<      read.
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the data type of node where the iterator is currently pointing.
 *
 * @return le_cfg_nodeType_t value indicating the stored value.
 */
//--------------------------------------------------------------------------------------------------
le_cfg_nodeType_t le_cfg_GetNodeType
(
    le_cfg_IteratorRef_t externalRef,  ///< [IN] Iterator object to use to read from the tree.
    const char* pathPtr                ///< [IN] Absolute or relative path to read from.
)
{
    return LE_CFG_TYPE_DOESNT_EXIST;
}

//--------------------------------------------------------------------------------------------------
/**
 * Move the iterator to the parent of the current node (moves up the tree).
 *
 * @return Return code will be one of the following values:
 *
 *         - LE_OK        - Commit was completed successfully.
 *         - LE_NOT_FOUND - Current node is the root node: has no parent.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_cfg_GoToParent
(
    le_cfg_IteratorRef_t iteratorRef   ///< [IN] Iterator to move.
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Jumps the iterator to the next child node of the current node. Assuming the following tree:
 *
 * @code
 * baseNode
 *      |
 *      +childA
 *          |
 *          +valueA
 *          |
 *          +valueB
 * @endcode
 *
 * If the iterator is moved to the path, "/baseNode/childA/valueA". After the first
 * GoToNextSibling the iterator will be pointing at valueB. A second call to GoToNextSibling
 * will cause the function to return LE_NOT_FOUND.
 *
 * @return Returns one of the following values:
 *
 *         - LE_OK            - Commit was completed successfully.
 *         - LE_NOT_FOUND     - Iterator has reached the end of the current list of siblings.
 *                              Also returned if the the current node has no siblings.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_cfg_GoToNextSibling
(
    le_cfg_IteratorRef_t iteratorRef     ///< [IN] Iterator to iterate.
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Writes a boolean value to the config tree. Only valid during a write
 * transaction.
 *
 * If the path is empty, the iterator's current node will be set.
 */
//--------------------------------------------------------------------------------------------------
void le_cfg_SetBool
(
    le_cfg_IteratorRef_t iteratorRef, ///< [IN] Iterator to use as a basis for the transaction.
    const char* LE_NONNULL path,      ///< [IN] Path to the target node. Can be an absolute path,
                                      ///< or a path relative from the iterator's current position.
    bool value                        ///< [IN] Value to write.
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * Writes a string value to the config tree. Only valid during a write
 * transaction.
 *
 * If the path is empty, the iterator's current node will be set.
 */
//--------------------------------------------------------------------------------------------------
void le_cfg_SetString
(
    le_cfg_IteratorRef_t iteratorRef,  ///< [IN] Iterator to use as a basis for the transaction.
    const char* LE_NONNULL path,       ///< [IN] Path to the target node. Can be an absolute path,
                                       ///< or a path relative from the iterator's current position.
    const char* LE_NONNULL value       ///< [IN] Value to write.
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * Writes a 64-bit floating point value to the config tree. Only valid during a write
 * transaction.
 *
 * If the path is empty, the iterator's current node will be set.
 */
//--------------------------------------------------------------------------------------------------
void le_cfg_SetFloat
(
    le_cfg_IteratorRef_t iteratorRef, ///< [IN] Iterator to use as a basis for the transaction.
    const char* LE_NONNULL path,      ///< [IN] Path to the target node. Can be an absolute path,
                                      ///< or a path relative from the iterator's current position.
    double value                      ///< [IN] Value to write.
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * Moves the iterator to the the first child of the node from the current location.
 *
 * For read iterators without children, this function will fail. If the iterator is a write
 * iterator, then a new node is automatically created. If this node or newly created
 * children of this node are not written to, then this node will not persist even if the iterator
 * is committed.
 *
 * @return Return code will be one of the following values:
 *
 *         - LE_OK        - Move was completed successfully.
 *         - LE_NOT_FOUND - The given node has no children.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_cfg_GoToFirstChild
(
    le_cfg_IteratorRef_t iteratorRef     ///< [IN] Iterator object to move.
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the name of the node where the iterator is currently pointing.
 *
 * @return - LE_OK       Read was completed successfully.
 *         - LE_OVERFLOW Supplied string buffer was not large enough to hold the value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_cfg_GetNodeName
(
    le_cfg_IteratorRef_t iteratorRef,  ///< [IN] Iterator object to use to read from the tree.
    const char* LE_NONNULL path,       ///< [IN] Path to the target node. Can be an absolute path, or
                                       ///< a path relative from the iterator's current position.
    char* name,                        ///< [OUT] Read the name of the node object.
    size_t nameSize                    ///< [IN]
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Gets the application name of the process with the specified PID.
 *
 * @return
 *      LE_OK if the application name was successfully found.
 *      LE_OVERFLOW if the application name could not fit in the provided buffer.
 *      LE_NOT_FOUND if the process is not part of an application.
 *      LE_FAULT if there was an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_appInfo_GetName
(
    int32_t pid,
        ///< [IN]
        ///< PID of the process.

    char* appName,
        ///< [OUT]
        ///< Application name

    size_t appNameNumElements
        ///< [IN]
)
{
    memcpy(appName, "test", sizeof("test"));
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to send an asynchronous response to server.
 *
 * @return
 *      - true if an asynchronous response is initiated
 *      - else false
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_SendAsyncResponse
(
    lwm2mcore_Ref_t instanceRef,                ///< [IN] instance reference
    lwm2mcore_CoapRequest_t* requestPtr,        ///< [IN] CoAP request refernce
    lwm2mcore_CoapResponse_t* responsePtr       ///< [IN] CoAP response
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get URI from request
 */
//--------------------------------------------------------------------------------------------------
const char* lwm2mcore_GetRequestUri
(
    lwm2mcore_CoapRequest_t* requestRef    ///< [IN] Coap request reference
)
{
    return "coap://leshan.eclipse.org:5784";
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get method from request
 */
//--------------------------------------------------------------------------------------------------
coap_method_t lwm2mcore_GetRequestMethod
(
    lwm2mcore_CoapRequest_t* requestRef        ///< [IN] Coap request reference
)
{
    return COAP_GET;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get payload from request
 */
//--------------------------------------------------------------------------------------------------
const uint8_t* lwm2mcore_GetRequestPayload
(
    lwm2mcore_CoapRequest_t* requestRef    ///< [IN] Coap request reference
)
{
    return "1234";
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get payload length from request
 */
//--------------------------------------------------------------------------------------------------
size_t lwm2mcore_GetRequestPayloadLength
(
    lwm2mcore_CoapRequest_t* requestRef    ///< [IN] Coap request reference
)
{
    return 4;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get token from request
 */
//--------------------------------------------------------------------------------------------------
const uint8_t* lwm2mcore_GetToken
(
    lwm2mcore_CoapRequest_t* requestRef    ///< [IN] Coap request reference
)
{
    return "1";
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get token length from request
 */
//--------------------------------------------------------------------------------------------------
size_t lwm2mcore_GetTokenLength
(
    lwm2mcore_CoapRequest_t* requestRef    ///< [IN] Coap request reference
)
{
    return 1;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set coap event handler
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_SetCoapEventHandler
(
    coap_request_handler_t handlerRef    ///< [IN] Coap action handler
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the push callback handler
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_SetPushCallback
(
    lwm2mcore_PushAckCallback_t callbackP  ///< [IN] push callback pointer
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * Request the avcServer to open a AV session.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_DUPLICATE if already connected.
 *      - LE_BUSY if currently retrying.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_RequestSession
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Request the avcServer to close a AV session.
 *
 * @return
 *      - LE_OK if able to initiate a session close
 *      - LE_FAULT on error
 *      - LE_BUSY if session is owned by control app
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcServer_ReleaseSession
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initializes a CborEncoder structure encoder by pointing it to buffer of size. The flags field is
 * currently unused and must be zero.
 */
//--------------------------------------------------------------------------------------------------
void cbor_encoder_init
(
    CborEncoder* encoder,
    uint8_t *buffer,
    size_t size,
    int flags
)
{
    if ( NULL != encoder )
    {
        encoder->data.ptr = buffer + size;
        encoder->end = buffer + size;
        encoder->added = 0;
        encoder->flags = flags;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Creates a CBOR map in the CBOR stream provided by encoder and initializes mapEncoder.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_encoder_create_map
(
    CborEncoder* encoder,
    CborEncoder* mapEncoder,
    size_t length
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Appends the byte string of length to the CBOR stream provided by encoder. CBOR byte strings are
 * arbitrary raw data.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_encode_text_string
(
    CborEncoder* encoder,
    const char *string,
    size_t length
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Appends the signed 64-bit integer value to the CBOR stream provided by encoder.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_encode_int
(
    CborEncoder* encoder,
    int64_t value
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Creates a CBOR array in the CBOR stream provided by encoder and initializes arrayEncoder.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_encoder_create_array
(
    CborEncoder* encoder,
    CborEncoder* arrayEncoder,
    size_t length
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Appends the floating-point value of type fpType and pointed to by value to the CBOR stream
 * provided by encoder.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_encode_floating_point
(
    CborEncoder* encoder,
    CborType fpType,
    const void *value)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Appends the unsigned 64-bit integer value to the CBOR stream provided by encoder.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_encode_uint
(
    CborEncoder* encoder,
    uint64_t value
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Closes the CBOR container (array or map) provided by containerEncoder and updates the CBOR stream
 * provided by encoder.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_encoder_close_container
(
    CborEncoder* encoder,
    const CborEncoder* containerEncoder
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieves the CBOR integer value that points to and stores it in result.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_value_get_int_checked
(
    const CborValue *value,
    int *result
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Decodes the CBOR integer value when it is larger than the 16 bits available in value->extra.
 */
//--------------------------------------------------------------------------------------------------
uint64_t _cbor_value_decode_int64_internal
(
    const CborValue *value
)
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
/**
 * Creates a CborValue iterator pointing to the first element of the container
 * represented by \a it and saves it in \a recursed. The \a it container object
 * needs to be kept and passed again to cbor_value_leave_container() in order
 * to continue iterating past this container.
 *
 * The \a it CborValue iterator must point to a container.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_value_enter_container
(
    const CborValue *it,
    CborValue *recursed
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Copies the string pointed by value into the buffer provided at buffer of buflen bytes
 */
//--------------------------------------------------------------------------------------------------
CborError _cbor_value_copy_string
(
    const CborValue *value,
    void *buffer,
    size_t *buflen,
    CborValue *next
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Appends the CBOR Simple Type of value \a value to the CBOR stream provided by
 * \a encoder.
 *
 * This function is a stub.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_encode_simple_value(CborEncoder* encoder, uint8_t value)
{

    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Appends the CBOR Simple Type of value \a value to the CBOR stream provided by
 * \a encoder.
 * This function is a stub.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_value_leave_container
(
    CborValue *it,
    const CborValue *recursed
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Advances the CBOR value \a it by one element, skipping over containers.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_value_advance
(
    CborValue *it
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initializes the CBOR parser for parsing \a size bytes beginning at \a
 * buffer.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_parser_init
(
    const uint8_t *buffer,
    size_t size,
    int flags,
    CborParser *parser,
    CborValue *it
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * Returns the error string corresponding to the CBOR error condition \a error.
 */
//--------------------------------------------------------------------------------------------------
const char *cbor_error_string
(
    CborError error
)
{
    return "stub function";
}

//--------------------------------------------------------------------------------------------------
/**
 * Calculates the length of the byte or text string that \a value points to and
 * stores it in \a len.
 */
//--------------------------------------------------------------------------------------------------
CborError cbor_value_calculate_string_length
(
    const CborValue *value,
    size_t *len
)
{
    return CborNoError;
}

//--------------------------------------------------------------------------------------------------
/**
 * LwM2M client entry point to push data.
 *
 * @return
 *      - LE_OK in case of success.
 *      - LE_BUSY if busy pushing data.
 *      - LE_FAULT in case of failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Push
(
    uint8_t* payload,                       ///< [IN] Payload to push.
    size_t payloadLength,                   ///< [IN] Payload length.
    lwm2mcore_PushContent_t contentType,    ///< [IN] Content type.
    uint16_t* midPtr                        ///< [OUT] Message identifier.
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Returns the instance reference of this client.
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Ref_t avcClient_GetInstance
(
    void
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Main function
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    avData_Init();
    push_Init();
    timeSeries_Init();
}
