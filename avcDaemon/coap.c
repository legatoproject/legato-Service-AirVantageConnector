/**
 * @file coap.c
 *
 * Implementation of CoAP external handler mechanism. This file provides apis to receive
 * and respond to CoAP messages not handled by AirVantage Connector.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/lwm2mcore.h>
#include "avcClient.h"
#include <lwm2mcore/coapHandlers.h>
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * CoAP client session instance reference.
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Ref_t CoapClientRef;

//--------------------------------------------------------------------------------------------------
/**
 * Is a push stream in progress?
 */
//--------------------------------------------------------------------------------------------------
static bool PushBusy = false;

//--------------------------------------------------------------------------------------------------
/**
 * Static AVC event handler
 */
//--------------------------------------------------------------------------------------------------
static le_avc_StatusEventHandlerRef_t AvcStatusHandler;

//--------------------------------------------------------------------------------------------------
/**
 * Data associated with the CoAP message event
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_coap_Code_t code;                              ///< CoAP method code / response code
    le_coap_StreamStatus_t streamStatus;              ///< Stream status
    uint16_t messageId;                               ///< CoAP message id
    uint16_t contentType;                             ///< Payload content type
    uint8_t uri[LE_COAP_MAX_URI_NUM_BYTES];           ///< URI
    uint8_t token[LE_COAP_MAX_TOKEN_NUM_BYTES];       ///< Token
    uint8_t tokenLength;                              ///< Token length
    uint8_t payload[LE_COAP_MAX_PAYLOAD_NUM_BYTES];   ///< Payload of CoAP request
    size_t payloadLength;                             ///< length of input buffer
    uint16_t blockSize;                               ///< Block size
}
CoapMessageData_t;

//--------------------------------------------------------------------------------------------------
/**
 * Event for reporting receieved CoAP messages to user application.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t CoapMessageEvent;

//--------------------------------------------------------------------------------------------------
/**
 * CoAP response
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_CoapResponse_t CoapResponse;

//--------------------------------------------------------------------------------------------------
/**
 * CoAP notification
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_CoapNotification_t CoapNotification = {0};

//--------------------------------------------------------------------------------------------------
/**
 * Convert Lwm2mCore stream status to legato stream status
 *
 * @return
 *     - Lwm2mCore stream status
 */
//--------------------------------------------------------------------------------------------------
static le_coap_StreamStatus_t ConvertLwm2mStreamStatus
(
    lwm2mcore_StreamStatus_t lwm2mStreamStatus   ///< [IN] Stream status in Lwm2mCore
)
{
    switch (lwm2mStreamStatus)
    {
        case LWM2MCORE_STREAM_NONE:
            return LE_COAP_STREAM_NONE;

        case LWM2MCORE_RX_STREAM_START:
            return LE_COAP_RX_STREAM_START;

        case LWM2MCORE_RX_STREAM_IN_PROGRESS:
            return LE_COAP_RX_STREAM_IN_PROGRESS;

        case LWM2MCORE_RX_STREAM_END:
            return LE_COAP_RX_STREAM_END;

        case LWM2MCORE_RX_STREAM_ERROR:
            return LE_COAP_RX_STREAM_ERROR;

        case LWM2MCORE_TX_STREAM_START:
            return LE_COAP_TX_STREAM_START;

        case LWM2MCORE_TX_STREAM_IN_PROGRESS:
            return LE_COAP_TX_STREAM_IN_PROGRESS;

        case LWM2MCORE_TX_STREAM_END:
            return LE_COAP_TX_STREAM_END;

        case LWM2MCORE_TX_STREAM_ERROR:
            return LE_COAP_TX_STREAM_ERROR;

        default:
            return LE_COAP_STREAM_INVALID;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * AVC event handler
 */
//--------------------------------------------------------------------------------------------------
static void StatusHandler
(
    le_avc_Status_t status,                 ///< [IN] AVC status
    int32_t         totalNumBytes,          ///< [IN] not used
    int32_t         downloadProgress,       ///< [IN] not used
    void*           contextPtr              ///< [IN] not used
)
{
    (void)totalNumBytes;
    (void)downloadProgress;
    (void)contextPtr;

    if (LE_AVC_SESSION_STOPPED == status)
    {
        if(PushBusy)
        {
            LE_DEBUG("Session is stopped and CoAP is on-going: pass it to false");
            PushBusy = false;
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Convert legato stream status to lwm2mcore stream status
 *
 * @return
 *     - Legato stream status
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_StreamStatus_t ConvertLeStreamStatus
(
    le_coap_StreamStatus_t leStreamStatus    ///< [IN] Stream status in Legato
)
{
    switch (leStreamStatus)
    {
        case LE_COAP_STREAM_NONE:
            return LWM2MCORE_STREAM_NONE;

        case LE_COAP_RX_STREAM_START:
            return LWM2MCORE_RX_STREAM_START;

        case LE_COAP_RX_STREAM_IN_PROGRESS:
            return LWM2MCORE_RX_STREAM_IN_PROGRESS;

        case LE_COAP_RX_STREAM_END:
            return LWM2MCORE_RX_STREAM_END;

        case LE_COAP_RX_STREAM_ERROR:
            return LWM2MCORE_RX_STREAM_ERROR;

        case LE_COAP_TX_STREAM_START:
            return LWM2MCORE_TX_STREAM_START;

        case LE_COAP_TX_STREAM_IN_PROGRESS:
            return LWM2MCORE_TX_STREAM_IN_PROGRESS;

        case LE_COAP_TX_STREAM_END:
            return LWM2MCORE_TX_STREAM_END;

        case LE_COAP_TX_STREAM_ERROR:
            return LWM2MCORE_TX_STREAM_ERROR;

        default:
            return LWM2MCORE_STREAM_INVALID;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert lwm2mcore ack status to legato push status
 *
 * @return
 *     - Push status
 */
//--------------------------------------------------------------------------------------------------
static le_coap_PushStatus_t ConvertAckToPushStatus
(
    lwm2mcore_AckResult_t result     ///< [IN] Acknowledge status in Lwm2mCore
)
{
    if (result == LWM2MCORE_ACK_RECEIVED)
    {
        return LE_COAP_PUSH_SUCCESS;
    }

    return LE_COAP_PUSH_FAILED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Handles CoAP messages from server such as read, write, execute and streams (block transfers)
 */
//--------------------------------------------------------------------------------------------------
static void CoapMessageHandler
(
    lwm2mcore_CoapRequest_t* requestRef    ///< [IN] CoAP request reference
)
{
    const char* uriPtr;
    CoapMessageData_t coapMsgData;

    // Save the session context and server request ref, so when reply function such as
    // le_avdata_ReplyExecResult is called at the end of the command execution,
    // it can async reply AV server with them.
    CoapClientRef = avcClient_GetInstance();
    if (NULL == CoapClientRef)
    {
        LE_ERROR("Cannot get CoAP client session context. Stop processing CoAP request.");
        return;
    }

    memset(&coapMsgData, 0, sizeof(CoapMessageData_t));

    // Extract info from the server request.
    coapMsgData.code = (uint8_t)lwm2mcore_GetRequestMethod(requestRef);
    coapMsgData.streamStatus = ConvertLwm2mStreamStatus(lwm2mcore_GetStreamStatus(requestRef));
    coapMsgData.messageId = lwm2mcore_GetMessageId(requestRef);
    coapMsgData.contentType = lwm2mcore_GetContentType(requestRef);
    uriPtr = lwm2mcore_GetRequestUri(requestRef); // cannot have trailing slash.

    le_utf8_Copy((char*)coapMsgData.uri, uriPtr, sizeof(coapMsgData.uri), NULL);

    // Get payload
    uint8_t* payloadPtr = (uint8_t *)lwm2mcore_GetRequestPayload(requestRef);
    coapMsgData.payloadLength = lwm2mcore_GetRequestPayloadLength(requestRef);

    LE_DEBUG("Rx Msg from server: code %u streamStatus %u contentType %u length %"PRIuS,
             coapMsgData.code, coapMsgData.streamStatus,
             coapMsgData.contentType, coapMsgData.payloadLength);

    if (coapMsgData.streamStatus == LE_COAP_TX_STREAM_ERROR)
    {
        // Reset the busy flag, to prevent the stream from permanently remaining in busy state.
        PushBusy = false;
    }

    size_t payloadLength = coapMsgData.payloadLength;
    if (payloadLength >= LE_COAP_MAX_PAYLOAD_NUM_BYTES)
    {
        LE_ERROR("Payload exceeded maximum length");
        payloadLength = LE_COAP_MAX_PAYLOAD_NUM_BYTES - 1;
    }

    memcpy(coapMsgData.payload, payloadPtr, payloadLength);

    // Get token
    uint8_t* tokenPtr = (uint8_t *)lwm2mcore_GetToken(requestRef);
    coapMsgData.tokenLength = lwm2mcore_GetTokenLength(requestRef);

    size_t tokenLength = coapMsgData.tokenLength;
    if (tokenLength >= LE_COAP_MAX_TOKEN_NUM_BYTES)
    {
       LE_ERROR("Token exceeded maximum length");
       tokenLength = LE_COAP_MAX_TOKEN_NUM_BYTES - 1;
    }

    memcpy(coapMsgData.token, tokenPtr, tokenLength);

    coapMsgData.blockSize = lwm2mcore_GetRequestBlock1Size(requestRef);

    // Send the event to external CoAP handler
    le_event_Report(CoapMessageEvent, &coapMsgData, sizeof(coapMsgData));
}

//--------------------------------------------------------------------------------------------------
/**
 * This function sends CoAP Ack messages to external app.
 */
//--------------------------------------------------------------------------------------------------
static void CoapAckHandler
(
    lwm2mcore_AckResult_t ackResult     ///< [IN] Acknowledge status
)
{
    lwm2mcore_CoapNotification_t* coapNotificationPtr = &CoapNotification;
    le_coap_PushHandlerFunc_t handlerPtr =
                              (le_coap_PushHandlerFunc_t)coapNotificationPtr->callbackRef;

    le_coap_PushStatus_t pushStatus = ConvertAckToPushStatus(ackResult);

    if (pushStatus == LE_COAP_PUSH_FAILED)
    {
        // Reset the busy flag, to prevent the stream from permanently remaining in busy state.
        PushBusy = false;
    }

    if (handlerPtr != NULL)
    {
        handlerPtr(pushStatus,
                   (const uint8_t*)&coapNotificationPtr->tokenPtr,
                   coapNotificationPtr->tokenLength,
                   coapNotificationPtr->callbackContextPtr);
    }
    else
    {
        LE_WARN("Callback handler doesn't exist");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer CoAP message handler
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerCoapMessageHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    CoapMessageData_t* coapMsgDataPtr = reportPtr;
    le_coap_MessageHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    clientHandlerFunc(coapMsgDataPtr->code,
                      coapMsgDataPtr->streamStatus,
                      coapMsgDataPtr->messageId,
                      coapMsgDataPtr->contentType,
                      (const char*)&coapMsgDataPtr->uri,
                      (const uint8_t*)&coapMsgDataPtr->token,
                      coapMsgDataPtr->tokenLength,
                      (const uint8_t*)&coapMsgDataPtr->payload,
                      coapMsgDataPtr->payloadLength,
                      coapMsgDataPtr->blockSize,
                      le_event_GetContextPtr());
}

//--------------------------------------------------------------------------------------------------
/**
 * CoAP add message event handler
 *
 *  @return:
 *    - Reference to message event handler if successful
 *    - NULL if the message event handler cannot be added
 */
//--------------------------------------------------------------------------------------------------
le_coap_MessageEventHandlerRef_t le_coap_AddMessageEventHandler
(
    le_coap_MessageHandlerFunc_t handlerPtr,      ///< [IN] Pointer on handler function
    void* contextPtr                              ///< [IN] Context pointer
)
{
    le_event_HandlerRef_t handlerRef;

    // handlerPtr must be valid
    if (NULL == handlerPtr)
    {
        LE_ERROR("Handler cannot be NULL");
        return NULL;
    }

    // Set the CoAP message handler.
    // This is the default message handler for CoAP content types not handled by LwM2M.
    lwm2mcore_SetCoapExternalHandler(CoapMessageHandler);

    // Register the user app handler
    handlerRef = le_event_AddLayeredHandler("CoapExternalHandler",
                                            CoapMessageEvent,
                                            FirstLayerCoapMessageHandler,
                                            (le_event_HandlerFunc_t)handlerPtr);
    le_event_SetContextPtr(handlerRef, contextPtr);

    return (le_coap_MessageEventHandlerRef_t)handlerRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * CoAP add message event handler
 */
//--------------------------------------------------------------------------------------------------
void le_coap_RemoveMessageEventHandler
(
    le_coap_MessageEventHandlerRef_t handlerRef      ///< [IN] Handler reference
)
{
    le_event_RemoveHandler((le_event_HandlerRef_t)handlerRef);

    lwm2mcore_SetCoapExternalHandler(NULL);

    lwm2mcore_SetCoapAckHandler(NULL);
}

//--------------------------------------------------------------------------------------------------
/**
 * Sends asynchronous CoAP response to server.
 *
 * @return
 *     - LE_OK on success.
 *     - LE_FAULT if failed.
 *
 * Note: This API will return success if it successful in sending the message down the stack.
 * Retransmission will be handled at CoAP layer and errors reports from server will be sent as
 * new incoming messages.
 *
 * Note: blockSize parameter needs to be a value equal to 16, 32, 64, 128, 256, 512 or 1024.
 * It allows the application to indicate to the server that the device wants to negotiate another
 * block size value. The blockSize value should be equal or smaller than blockSize provided
 * in le_coap_MessageHandlerFunc_t handler.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_coap_SendResponse
(
    uint16_t messageId,                  ///< [IN] Message identifier
    const uint8_t* tokenPtr,             ///< [IN] Token pointer
    size_t tokenLength,                  ///< [IN] Token length
    uint16_t contentType,                ///< [IN] Content type
    le_coap_Code_t responseCode,         ///< [IN] Result of CoAP operation
    le_coap_StreamStatus_t streamStatus, ///< [IN] Status of the transmit stream
    const uint8_t* payloadPtr,           ///< [IN] Payload pointer
    size_t payloadLength,                ///< [IN] Payload Length
    uint16_t blockSize                   ///< [IN] Block size
)
{
    bool result;
    lwm2mcore_CoapResponse_t* coapResponsePtr = &CoapResponse;

    LE_INFO("Response: CoAP response from app");

    if (avcClient_GetInstance() == NULL)
    {
        LE_ERROR("Session disconnected");
        return LE_FAULT;
    }

    if (payloadLength > LE_COAP_MAX_PAYLOAD)
    {
        LE_ERROR("Invalid payload length");
        return LE_FAULT;
    }

    if (tokenLength > LE_COAP_MAX_TOKEN_LENGTH)
    {
        LE_ERROR("Invalid token length");
        return LE_FAULT;
    }

    // Pass response code directly as it is not converted back inside lwm2mcore
    coapResponsePtr->code = (uint32_t)responseCode;
    coapResponsePtr->contentType = contentType;
    coapResponsePtr->streamStatus = ConvertLeStreamStatus(streamStatus);
    coapResponsePtr->payloadLength = payloadLength;
    coapResponsePtr->messageId = messageId;
    coapResponsePtr->payloadPtr = (uint8_t*)payloadPtr;
    coapResponsePtr->blockSize = blockSize;

    // Allow app to send token as well.
    // Might be useful to respond with just tokens for unsolicited responses.
    memcpy((char*)coapResponsePtr->token, (const char*)tokenPtr, tokenLength);
    coapResponsePtr->tokenLength = tokenLength;

    result = lwm2mcore_SendResponse(CoapClientRef, &CoapResponse);

    if(result)
    {
        return LE_OK;
    }

    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add push event handler
 */
//--------------------------------------------------------------------------------------------------
le_coap_PushEventHandlerRef_t le_coap_AddPushEventHandler
(
    le_coap_PushHandlerFunc_t handlerPtr, ///< [IN] Push result callback
    void* contextPtr                      ///< [IN] Context pointer
)
{
    if (CoapNotification.callbackRef != NULL)
    {
        LE_ERROR("Can't add new handler: old one has to be removed first");
        return NULL;
    }
    // Not all ack responses received on CoAP is sent to the external CoAP Handler.
    // This is the default message handler for push ack received / timeout.
    lwm2mcore_SetCoapAckHandler(CoapAckHandler);

    CoapNotification.callbackRef = handlerPtr;
    CoapNotification.callbackContextPtr = contextPtr;

    AvcStatusHandler = le_avc_AddStatusEventHandler(StatusHandler, NULL);

    return (le_coap_PushEventHandlerRef_t) handlerPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove push event handler
 */
//--------------------------------------------------------------------------------------------------
void le_coap_RemovePushEventHandler
(
    le_coap_PushEventHandlerRef_t handlerRef ///< [IN] Push result callback reference
)
{
    if (CoapNotification.callbackRef == handlerRef)
    {
        CoapNotification.callbackRef = NULL;
        CoapNotification.callbackContextPtr = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function sends unsolicited CoAP push messages to the server. Responses to push will be
 * received by push handler function.
 *
 * @return:
 *   - LE_OK - If payload is sent to CoAP layer for transmission
 *   - LE_BUSY - If another push stream is in progress
 *   - LE_FAULT - On any other failure
 *
 * Note: This api cannot be used concurrently by two apps or process. It is the responsibility of
 * the app to track stream status before a push operation.
 *
 * Note: The token can be generated by AirVantage Connector. In this case, tokenPtr parameter should
 * be set to NULL and tokenLength parameter should be set to 0.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_coap_Push
(
    const char* uriPtr,                   ///< [IN] URI where push should end
    const uint8_t* tokenPtr,              ///< [IN] Token pointer
    size_t tokenLength,                   ///< [IN] Token length
    uint16_t contentType,                 ///< [IN] Content type
    le_coap_StreamStatus_t streamStatus,  ///< [IN] Status of transmit stream
    const uint8_t* payloadPtr,            ///< [IN] Payload pointer
    size_t payloadLength                  ///< [IN] Payload Length
)
{
    bool result;
    lwm2mcore_CoapNotification_t* coapNotificationPtr = &CoapNotification;

    LE_INFO("Push: CoAP POST from device");

    if (avcClient_GetInstance() == NULL)
    {
        LE_ERROR("Session disconnected");
        return LE_FAULT;
    }

    if (payloadLength > LE_COAP_MAX_PAYLOAD)
    {
        LE_ERROR("Invalid payload length");
        return LE_FAULT;
    }

    if (tokenLength > LE_COAP_MAX_TOKEN_LENGTH)
    {
        LE_ERROR("Invalid token length");
        return LE_FAULT;
    }

    switch (streamStatus)
    {
        case LE_COAP_STREAM_NONE:
            if (PushBusy == true)
            {
                LE_ERROR("Busy: Push stream in progress");
                return LE_BUSY;
            }
            break;

        case LE_COAP_TX_STREAM_START:
            if (PushBusy == true)
            {
                LE_ERROR("Busy: Push stream in progress");
                return LE_BUSY;
            }
            else
            {
                LE_INFO("Starting a new push stream");
                PushBusy = true;
            }
            break;

        case LE_COAP_TX_STREAM_IN_PROGRESS:
            if (PushBusy == false)
            {
                LE_ERROR("Stream not started yet");
                return LE_FAULT;
            }
            break;

        case LE_COAP_TX_STREAM_CANCEL:
            PushBusy = false;
            return LE_OK;

        case LE_COAP_TX_STREAM_END:
        case LE_COAP_TX_STREAM_ERROR:
            PushBusy = false;
            break;

        default:
            LE_ERROR("Invalid stream status");
            return LE_FAULT;
    }

    coapNotificationPtr->uriPtr = (uint8_t*)uriPtr;
    coapNotificationPtr->contentType = contentType;
    coapNotificationPtr->streamStatus = ConvertLeStreamStatus(streamStatus);
    coapNotificationPtr->payloadLength = payloadLength;
    coapNotificationPtr->payloadPtr = (uint8_t*)payloadPtr;
    coapNotificationPtr->tokenPtr = (uint8_t*)tokenPtr;
    coapNotificationPtr->tokenLength = tokenLength;

    result = lwm2mcore_SendNotification(&CoapNotification);

    if(result)
    {
        return LE_OK;
    }

    // Reset the busy flag, to prevent the stream from permanently remaining in busy state.
    PushBusy = false;
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Init CoAP subcomponent
 */
//--------------------------------------------------------------------------------------------------
void coap_Init
(
    void
)
{
    // Create CoAP message event.
    CoapMessageEvent = le_event_CreateId("CoAP Message Event", sizeof(CoapMessageData_t));
}
