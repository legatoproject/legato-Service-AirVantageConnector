//--------------------------------------------------------------------------------------------------
/**
 * Echo test for CoAP API.
 *
 * This is a loop-back test to verify the communication through CoAP APIs. Server sends formatted
 * data using POST or PUT method and read it back using GET method.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "coapHandlerTest.h"

//--------------------------------------------------------------------------------------------------
// Definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Server update period
 */
//--------------------------------------------------------------------------------------------------
#define UPDATE_PERIOD_SEC        30

//--------------------------------------------------------------------------------------------------
/**
 * Maximum stream size
 */
//--------------------------------------------------------------------------------------------------
#define STREAM_MAX_SIZE        (12 * LE_COAP_MAX_PAYLOAD_NUM_BYTES)

//--------------------------------------------------------------------------------------------------
// Data structures
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Stream context structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint8_t  stream[STREAM_MAX_SIZE];      ///< Stream content
    size_t   streamLength;                 ///< Stream total length
    off_t    offset;                       ///< Read offset
    bool     streamEnd;                    ///< True when the stream is completely received
}
StreamCtx_t;

//--------------------------------------------------------------------------------------------------
// Local variables
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Server periodical updates timer
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t ServerUpdateTimerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * AVC session current state
 */
//--------------------------------------------------------------------------------------------------
static le_avc_Status_t AvcSessionState = LE_AVC_SESSION_STOPPED;

//--------------------------------------------------------------------------------------------------
/**
 * Stream context. This structure holds data received from remote DM server.
 */
//--------------------------------------------------------------------------------------------------
static StreamCtx_t StreamCtx;

//--------------------------------------------------------------------------------------------------
// Local functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Status handler for avcService updates
 */
//--------------------------------------------------------------------------------------------------
static void StatusHandler
(
    le_avc_Status_t updateStatus,
    int32_t totalNumBytes,
    int32_t downloadProgress,
    void* contextPtr
)
{
    switch (updateStatus)
    {
        case LE_AVC_SESSION_STARTED:
            LE_INFO("AVC session started.");
            AvcSessionState = LE_AVC_SESSION_STARTED;
            break;

        case LE_AVC_SESSION_STOPPED:
            LE_INFO("AVC session stopped.");
            AvcSessionState = LE_AVC_SESSION_STOPPED;
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback handler when a CoAP push message is acked, timed out or failed. When a push message is
 * streamed this event is called only when the last block is transmitted out.
 */
//--------------------------------------------------------------------------------------------------
static void PushAckCallBack
(
    le_coap_PushStatus_t status,
    void* contextPtr
)
{
    LE_INFO("Push finished");
    switch (status)
    {
        case LE_COAP_PUSH_SUCCESS:
            LE_INFO("Push Successful");
            break;

        case LE_COAP_PUSH_FAILED:
            LE_ERROR("Push Failed");
            break;

        default:
            LE_ERROR("Push status = %d", status);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler for receiving incoming CoAP messages
 */
//--------------------------------------------------------------------------------------------------
static void ExternalCoapHandler
(
    le_coap_Code_t code,                            ///< [IN] CoAP method or response code
    le_coap_StreamStatus_t streamStatus,            ///< [IN] CoAP stream status
    uint16_t messageId,                             ///< [IN] message id
    uint16_t contentType,                           ///< [IN] content type
    const char* uriPtr,                             ///< [IN] URI pointer of resource
    const uint8_t* tokenPtr,                        ///< [IN] token pointer
    size_t tokenLength,                             ///< [IN] token length
    const uint8_t* payloadPtr,                      ///< [IN] CoAP payload pointer
    size_t payloadLength,                           ///< [IN] CoAP payload length
    void* contextPtr                                ///< [IN] User context pointer
)
{
    StreamCtx_t* streamCtxPtr = &StreamCtx;
    le_coap_Code_t responseCode = LE_COAP_CODE_205_CONTENT;
    le_result_t status;

    // Check input parameters
    if ((!uriPtr) || (!tokenPtr) || (!payloadPtr))
    {
        LE_ERROR("NULL pointer provided");
        return;
    }

    LE_INFO("====Incoming CoAP message====");
    LE_INFO("URI[%zu]: %s", strlen(uriPtr), uriPtr);
    LE_INFO("Data[%zu]: %.*s", payloadLength, payloadLength, payloadPtr);
    LE_INFO("Code: %d, status:%d, msgId: %d, type:%d", code, streamStatus, messageId, contentType);
    LE_INFO("=============================");

    switch (code)
    {
        case LE_COAP_CODE_PUT:
        case LE_COAP_CODE_POST:

            if (streamCtxPtr->streamEnd)
            {
                LE_INFO("New stream. Clean the previous storage");
                streamCtxPtr->streamLength = 0;
                streamCtxPtr->streamEnd = false;
            }

            // Copy content to the stream context
            if ((streamCtxPtr->streamLength + payloadLength) < STREAM_MAX_SIZE)
            {
                memcpy(streamCtxPtr->stream + streamCtxPtr->streamLength,
                       payloadPtr,
                       payloadLength);

                streamCtxPtr->streamLength += payloadLength;
            }
            else
            {
                LE_ERROR("Data size exceeds maximum size, dismiss received data");
            }

            switch (streamStatus)
            {
                case LE_COAP_RX_STREAM_START:
                case LE_COAP_RX_STREAM_IN_PROGRESS:
                    responseCode = LE_COAP_CODE_NO_RESPONSE;
                    break;

                case LE_COAP_STREAM_NONE:
                case LE_COAP_RX_STREAM_END:
                    LE_INFO("Total bytes received: %zd", streamCtxPtr->streamLength);
                    streamCtxPtr->streamEnd = true;
                    responseCode = LE_COAP_CODE_204_CHANGED;
                    break;

                default:
                    responseCode = LE_COAP_CODE_500_INTERNAL_SERVER_ERROR;
                    break;
            }

            // Send response
            if (responseCode != LE_COAP_CODE_NO_RESPONSE)
            {
                status = le_coap_SendResponse(messageId,
                                              tokenPtr,
                                              tokenLength,
                                              LWM2M_CONTENT_CBOR,
                                              responseCode,
                                              LE_COAP_STREAM_NONE,
                                              (const uint8_t*)"",
                                              0);
                if(status != LE_OK)
                {
                    LE_ERROR("Unable to send response. Status: %d", status);
                }
            }
            break;

        case LE_COAP_CODE_GET:
        case LE_COAP_CODE_231_CONTINUE:
        {
            uint8_t* dataPtr = (streamCtxPtr->stream + streamCtxPtr->offset);
            size_t dataLength = (streamCtxPtr->streamLength - streamCtxPtr->offset);

            // First message received from server, check if data needs to be splitted in few chunks
            if (streamStatus == LE_COAP_STREAM_NONE)
            {
                // Data needs to be streamed
                if (streamCtxPtr->streamLength > LE_COAP_MAX_PAYLOAD)
                {
                   dataLength = LE_COAP_MAX_PAYLOAD;
                   streamStatus = LE_COAP_TX_STREAM_START;
                }
                else
                {
                    dataLength = streamCtxPtr->streamLength;
                    streamStatus = LE_COAP_STREAM_NONE;
                }
            }
            else
            {
                if (dataLength > LE_COAP_MAX_PAYLOAD)
                {
                    dataLength = LE_COAP_MAX_PAYLOAD;
                    streamStatus = LE_COAP_TX_STREAM_IN_PROGRESS;
                }
                else
                {
                    streamStatus = LE_COAP_TX_STREAM_END;
                }
            }

            LE_INFO("Data[%zu]: %.*s", dataLength, dataLength, dataPtr);

            if (code == LE_COAP_CODE_GET)
            {
                status = le_coap_SendResponse(messageId,
                                              tokenPtr,
                                              tokenLength,
                                              LWM2M_CONTENT_CBOR,
                                              responseCode,
                                              streamStatus,
                                              dataPtr,
                                              dataLength);
            }
            else
            {
                status = le_coap_Push("/push",
                                      tokenPtr,
                                      tokenLength,
                                      LWM2M_CONTENT_CBOR,
                                      streamStatus,
                                      dataPtr,
                                      dataLength,
                                      PushAckCallBack,
                                      NULL);
            }

            if (status != LE_OK)
            {
                LE_ERROR("Unable to send response. Error: %d", status);
                return;
            }

            streamCtxPtr->offset += dataLength;
            if (streamCtxPtr->offset == streamCtxPtr->streamLength)
            {
                LE_INFO("Total bytes received: %lu", streamCtxPtr->offset);
                streamCtxPtr->offset = 0;
            }
            break;
        }

        case LE_COAP_CODE_DELETE:
            LE_ERROR("Delete not currently supported");
            break;

        default:
            LE_ERROR("Unhandled CoAP code: %d", code);
            break;
    }
}


//-------------------------------------------------------------------------------------------------
/**
 * This function is called periodically from timer handler to push data from device to DM server.
 * If data is bigger than max CoAP payload, it is splitted and transferred in a stream.
 */
//-------------------------------------------------------------------------------------------------
static void PushResources
(
    le_timer_Ref_t  timerRef    ///< [IN] Timer reference
)
{
    StreamCtx_t* streamCtxPtr = &StreamCtx;
    uint8_t token[LE_COAP_MAX_TOKEN_NUM_BYTES] = {0};
    uint8_t* payloadPtr = streamCtxPtr->stream;
    size_t payloadLength = streamCtxPtr->streamLength;
    uint32_t streamStatus;
    le_result_t status;

    // Check if AVC session is started
    if (AvcSessionState != LE_AVC_SESSION_STARTED)
    {
        LE_INFO("AVC session not yet started");
        return;
    }

    // Check if there is data stored in device to be pushed
    if (!streamCtxPtr->streamLength)
    {
        LE_INFO("Storage empty, nothing to push");
        return;
    }

    // Check if data needs to be splitted into stream chunks
    if (streamCtxPtr->streamLength > LE_COAP_MAX_PAYLOAD)
    {
       payloadLength = LE_COAP_MAX_PAYLOAD;
       streamStatus = LE_COAP_TX_STREAM_START;
    }
    else
    {
        payloadLength = streamCtxPtr->streamLength;
        streamStatus = LE_COAP_STREAM_NONE;
    }

    // Send a PUSH CoAP message
    status = le_coap_Push("/push",
                          token,
                          0,
                          LWM2M_CONTENT_CBOR,
                          streamStatus,
                          payloadPtr,
                          payloadLength,
                          PushAckCallBack,
                          NULL);

    if (status != LE_OK)
    {
        LE_ERROR("Push failed. Return status: %d", status);
        return;
    }

    if (streamStatus == LE_COAP_TX_STREAM_START)
    {
        // Pushing chunks will continue when receiving LE_COAP_CODE_231_CONTINUE event
        streamCtxPtr->offset = payloadLength;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Component initializer
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    le_clk_Time_t serverUpdateInterval = { UPDATE_PERIOD_SEC, 0 };

    // Subscribe handlers
    le_avc_AddStatusEventHandler(StatusHandler, NULL);
    le_coap_AddMessageEventHandler(ExternalCoapHandler, NULL);

    // Start an AVC session
    switch (le_avc_StartSession())
    {
        case LE_OK:
            LE_INFO("AVC starting");
            break;

        case LE_DUPLICATE:
            LE_INFO("AVC session already started");
            AvcSessionState = LE_AVC_SESSION_STARTED;
            break;

        default:
            LE_ERROR("Wrong state");
            exit(EXIT_FAILURE);
            break;
    }

    // Push data periodically to DM server
    ServerUpdateTimerRef = le_timer_Create("serverUpdateTimer");
    le_timer_SetInterval(ServerUpdateTimerRef, serverUpdateInterval);
    le_timer_SetRepeat(ServerUpdateTimerRef, 0);
    le_timer_SetHandler(ServerUpdateTimerRef, PushResources);
    le_timer_Start(ServerUpdateTimerRef);
}
