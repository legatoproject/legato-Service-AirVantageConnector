//--------------------------------------------------------------------------------------------------
/**
 * Simple test app that verifies CoAP handler api
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "coapHandlerTest.h"

// Reference to push timer
le_timer_Ref_t ServerUpdateTimerRef = NULL;

// AV session state
le_avdata_SessionState_t AvSessionState = LE_AVDATA_SESSION_STOPPED;

// Response payload shared by all CoAP responses as well as push
char ResponsePayload[LE_COAP_MAX_PAYLOAD_NUM_BYTES];

// Context of the transmit message
typedef struct
{
    char filename[MAX_FILE_PATH_BYTES];        // Filename
    int fp;                                    // File pointer for open file (tx in progress)
    size_t offset;                             // not used
} coapTransmitContext_t;

// CoAP transmit context
coapTransmitContext_t TransmitContext[2];


// CoAP transmit status
static bool PushBusy = false;

//-------------------------------------------------------------------------------------------------
/**
 * Write received data to a file.
 */
//-------------------------------------------------------------------------------------------------
int CopyToFile(char* filePath, const char* bufferPtr, size_t length, bool isNewFile)
{
    int fp;
    size_t bytesWritten;

    if (isNewFile)
    {
        fp = open(filePath, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
    }
    else
    {
        fp = open(filePath, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    }

    if (fp)
    {
        bytesWritten = write(fp, bufferPtr, length);
        LE_ASSERT(bytesWritten == length);
        close (fp);
        return 0;
    }

    LE_ERROR("Failed to write to file %s", filePath);
    return 1;
}


//-------------------------------------------------------------------------------------------------
/**
 * Read data from file to buffer.
 */
//-------------------------------------------------------------------------------------------------
int CopyToBuffer(responseType_t respType, char* bufferPtr, size_t maxNumBytes, bool isNewFile)
{
    size_t maxLength = maxNumBytes - 1;
    int readLength = 0;
    coapTransmitContext_t* contextPtr;

    contextPtr = &TransmitContext[respType];

    if (isNewFile)
    {
        contextPtr->fp = open(contextPtr->filename, O_RDONLY,  S_IRUSR | S_IWUSR);
    }

    if (contextPtr->fp)
    {
        // Read the bytes, retrying if interrupted by a signal.
        do
        {
            readLength = read(contextPtr->fp, bufferPtr, maxLength);
        }
        while ((-1 == readLength) && (EINTR == errno));

        if (readLength != maxLength)
        {
            close(contextPtr->fp);
        }

        return readLength;
    }

    LE_ERROR("Failed to open file %s", contextPtr->filename);
    return -1;
}

//--------------------------------------------------------------------------------------------------
/**
 * Receives notification from avdata about session state.
 */
//--------------------------------------------------------------------------------------------------
static void SessionHandler
(
    le_avdata_SessionState_t sessionState,
    void* contextPtr
)
{
    if (sessionState == LE_AVDATA_SESSION_STARTED)
    {
        LE_INFO("Airvantage session started.");
        AvSessionState = LE_AVDATA_SESSION_STARTED;
    }
    else
    {
        LE_INFO("Airvantage session stopped.");
        AvSessionState = LE_AVDATA_SESSION_STOPPED;
    }
}


//-------------------------------------------------------------------------------------------------
/**
 * Handler for receiving CoAP Stream
 */
//-------------------------------------------------------------------------------------------------
static uint32_t CoapRxStreamHandler
(
    le_coap_StreamStatus_t streamStatus,
    const char* bufferPtr,
    size_t length
)
{
    // Copy stream data to buffer or file
    // send ack for incoming stream or start processing payload if stream succeeded.
    switch (streamStatus)
    {
        case LE_COAP_RX_STREAM_START:
            LE_INFO("Stream start: Create file and write received data");
            CopyToFile(RECEIVED_STREAM_FILE, bufferPtr, length, true);
            return COAP_NO_ERROR;

        case LE_COAP_RX_STREAM_IN_PROGRESS:
            LE_INFO("Stream in progress: Copy received data to file");
            CopyToFile(RECEIVED_STREAM_FILE, bufferPtr, length, false);
            return COAP_NO_ERROR;

        case LE_COAP_RX_STREAM_END:
            LE_INFO("Stream completed: Start processing received data");
            CopyToFile(RECEIVED_STREAM_FILE, bufferPtr, length, false);
            return COAP_204_CHANGED;

        case LE_COAP_RX_STREAM_ERROR:
            LE_INFO("Stream cancelled");
            return COAP_500_INTERNAL_SERVER_ERROR;

        default:
            LE_INFO("Unexpected stream status during PUT");
            return COAP_500_INTERNAL_SERVER_ERROR;
    }
}


//-------------------------------------------------------------------------------------------------
/**
 * Handler for transmitting CoAP Stream
 */
//-------------------------------------------------------------------------------------------------
static uint32_t CoapTxStreamHandler
(
    responseType_t respType,
    char* bufferPtr,
    size_t* lengthPtr,
    le_coap_StreamStatus_t* txStreamStatusPtr
)
{
    *lengthPtr = 0;
    le_coap_StreamStatus_t streamStatus = *txStreamStatusPtr;
    *txStreamStatusPtr = LE_COAP_TX_STREAM_ERROR;

    // Copy data from file to buffer
    // respond using le_coap_SendResponse()
    switch (streamStatus)
    {
        case LE_COAP_STREAM_NONE:
            LE_INFO("No stream");

            *lengthPtr = CopyToBuffer(respType, bufferPtr, LE_COAP_MAX_PAYLOAD_NUM_BYTES, true);

            if (*lengthPtr == -1)
            {
                *lengthPtr = 0;    // reset size of payload to 0
                *txStreamStatusPtr = LE_COAP_TX_STREAM_ERROR;
                return COAP_500_INTERNAL_SERVER_ERROR;
            }

            *txStreamStatusPtr = LE_COAP_STREAM_NONE;
            return COAP_205_CONTENT;

        case LE_COAP_TX_STREAM_START:
            LE_INFO("Stream started: Start sending data from file");
            *lengthPtr = CopyToBuffer(respType, bufferPtr, LE_COAP_MAX_PAYLOAD_NUM_BYTES, true);

            if (*lengthPtr == -1)
            {
                *lengthPtr = 0;    // reset size of payload to 0
                *txStreamStatusPtr = LE_COAP_TX_STREAM_ERROR;
                return COAP_500_INTERNAL_SERVER_ERROR;
            }

            *txStreamStatusPtr = LE_COAP_TX_STREAM_START;
            return COAP_205_CONTENT;

        case LE_COAP_TX_STREAM_IN_PROGRESS:
            LE_INFO("Stream in progress: Continue sending data from file");
            *lengthPtr = CopyToBuffer(respType, bufferPtr, LE_COAP_MAX_PAYLOAD_NUM_BYTES, false);

            if (*lengthPtr == -1)
            {
                *lengthPtr = 0;    // reset size of payload to 0
                *txStreamStatusPtr = LE_COAP_TX_STREAM_ERROR;
                return COAP_500_INTERNAL_SERVER_ERROR;
            }

            if (*lengthPtr == LE_COAP_MAX_PAYLOAD)
            {
                *txStreamStatusPtr = LE_COAP_TX_STREAM_IN_PROGRESS;
                return COAP_205_CONTENT;
            }

            *txStreamStatusPtr = LE_COAP_TX_STREAM_END;
            return COAP_205_CONTENT;

        case LE_COAP_TX_STREAM_END:
            LE_INFO("Stream completed");
            *txStreamStatusPtr = LE_COAP_TX_STREAM_END;
            return COAP_NO_ERROR;

        case LE_COAP_TX_STREAM_ERROR:
            LE_INFO("Stream cancelled");
            *txStreamStatusPtr = LE_COAP_TX_STREAM_END;
            return COAP_500_INTERNAL_SERVER_ERROR;

        default:
            LE_INFO("Unexpected stream status during GET");
            *txStreamStatusPtr = LE_COAP_TX_STREAM_END;
            return COAP_500_INTERNAL_SERVER_ERROR;
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
    PushBusy = false;

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
    le_coap_Code_t code,
        ///< CoAP method or response code
    le_coap_StreamStatus_t streamStatus,
        ///< CoAP stream status
    uint16_t messageId,
        ///< message id
    uint16_t contentType,
        ///< content type
    const char* uri,
        ///< URI of resource
    size_t uriLength,
        ///< URI length
    const char* token,
        ///< token
    uint8_t tokenLength,
        ///< token length
    const char* payload,
        ///< CoAP payload
    size_t payloadLength,
        ///< CoAP payload length
    void* contextPtr
        ///<
)
{
    uint32_t coapResponseCode;
    struct stat txFileStat;
    size_t responsePayloadLength = 0;
    le_coap_StreamStatus_t txStreamStatus;
    le_result_t result;
    char* getReponseFile;

    switch (code)
    {
        case LE_COAP_CODE_PUT:
        case LE_COAP_CODE_POST:    // POST will also copy the contents to a file
            // Check if we are receiving a stream
            // Copy stream data to buffer or file
            // send ack for incoming stream or start processing payload if stream succeeded.
            // respond using le_coap_SendResponse()

            if (streamStatus == LE_COAP_STREAM_NONE)
            {
                LE_DEBUG("No Stream: Process received message");

                // send actual CoAP response.
                le_coap_SendResponse(messageId,
                                     token,
                                     tokenLength,
                                     LWM2M_CONTENT_CBOR,
                                     COAP_204_CHANGED,
                                     LE_COAP_STREAM_NONE,
                                     ResponsePayload,
                                     0);
            }
            else
            {
                coapResponseCode = CoapRxStreamHandler(streamStatus, payload, payloadLength);

                if (coapResponseCode != COAP_NO_ERROR)
                {
                    // send actual CoAP response.
                    le_coap_SendResponse(messageId,
                                         token,
                                         tokenLength,
                                         LWM2M_CONTENT_CBOR,
                                         coapResponseCode,
                                         LE_COAP_STREAM_NONE,
                                         ResponsePayload,
                                         0);
                }
            }
            break;

        case LE_COAP_CODE_GET:
            // Check if we are receiving a stream
            // Copy stream data from file to buffer
            // If response if more than 1KB start streaming data out
            // If all bytes are streamed out successfully, send actual response
            getReponseFile = TransmitContext[REQUEST_RESPONSE].filename;

            if (strncmp(uri, URL_GET_SMALL_STRING, sizeof(URL_GET_SMALL_STRING)) == 0)
            {
                strncpy(getReponseFile, GET_RESPONSE_SMALL_STRING, MAX_FILE_PATH_BYTES);
            }
            else if (strncmp(uri, URL_GET_2KB_STRING, sizeof(URL_GET_2KB_STRING)) == 0)
            {
                strncpy(getReponseFile, GET_RESPONSE_2KB_STRING, MAX_FILE_PATH_BYTES);
            }
            else
            {
                LE_ERROR("URI %s not found", uri);
                le_coap_SendResponse(messageId,
                                     "",
                                     0,
                                     LWM2M_CONTENT_CBOR,
                                     COAP_404_NOT_FOUND,
                                     LE_COAP_STREAM_NONE,
                                     "",
                                     0);
                return;
            }

            LE_ASSERT(stat(getReponseFile, &txFileStat) == 0);
            LE_INFO("Size of transmit file = %ld", txFileStat.st_size);

            if (streamStatus == LE_COAP_STREAM_NONE)
            {
                LE_DEBUG("No Stream: Process received message");

                // send actual CoAP response.
                if (txFileStat.st_size <= LE_COAP_MAX_PAYLOAD)
                {
                    txStreamStatus = LE_COAP_STREAM_NONE;
                }
                else
                {
                    txStreamStatus = LE_COAP_TX_STREAM_START;
                }
            }
            else
            {
                txStreamStatus = streamStatus;
            }

            coapResponseCode = CoapTxStreamHandler(REQUEST_RESPONSE,
                                                   ResponsePayload,
                                                   &responsePayloadLength,
                                                   &txStreamStatus);

             if (coapResponseCode != COAP_NO_ERROR)
             {
                 // send actual CoAP response.
                 le_coap_SendResponse(messageId,
                                      token,
                                      tokenLength,
                                      LWM2M_CONTENT_CBOR,
                                      coapResponseCode,
                                      txStreamStatus,
                                      ResponsePayload,
                                      responsePayloadLength);
             }
             break;

        case LE_COAP_CODE_DELETE:
            LE_ERROR("CoAP DELETE is not handled");
            break;

        case LE_COAP_CODE_231_CONTINUE:
            LE_DEBUG("continue streaming");

            if (PushBusy)
            {
                LE_DEBUG("PUSH stream in progress: Continue streaming data from file");

                txStreamStatus = LE_COAP_TX_STREAM_IN_PROGRESS;
                coapResponseCode = CoapTxStreamHandler(UNSOLICITED_RESPONSE,
                                                       ResponsePayload,
                                                       &responsePayloadLength,
                                                       &txStreamStatus);

                if (coapResponseCode != COAP_NO_ERROR)
                {
                    // send actual CoAP response.
                    result = le_coap_Push(PUSH_URI,
                                          token,
                                          0,
                                          LWM2M_CONTENT_CBOR,
                                          txStreamStatus,
                                          ResponsePayload,
                                          responsePayloadLength,
                                          PushAckCallBack,
                                          NULL);

                    if (result != LE_OK)
                    {
                        LE_ERROR("Push failed");
                    }
                }
            }
            else
            {
                LE_ERROR("Unexpected CoAP response received. Push not in progress");
            }
            break;

        default:
            LE_ERROR("Unhandled CoAP code %d", code);
            break;
    }
}


//-------------------------------------------------------------------------------------------------
/**
 * This function is called every 20 seconds to push data from device to cloud
 */
//-------------------------------------------------------------------------------------------------
static void PushResources(le_timer_Ref_t  timerRef)
{
    uint32_t coapResponseCode;
    struct stat txFileStat;
    size_t responsePayloadLength = 0;
    le_coap_StreamStatus_t txStreamStatus;
    char token[LE_COAP_MAX_TOKEN_NUM_BYTES];
    le_result_t result;
    static bool isLargeFile = false;

    char* pushFilePtr = TransmitContext[UNSOLICITED_RESPONSE].filename;

    // Alternate between a small and large string
    if (isLargeFile)
    {
        strncpy(pushFilePtr, TRANSMIT_SMALL_STRING, MAX_FILE_PATH_BYTES);
        isLargeFile = false;
    }
    else
    {
        strncpy(pushFilePtr, TRANSMIT_LARGE_STRING, MAX_FILE_PATH_BYTES);
        isLargeFile = true;
    }

    LE_INFO("Start pushing data");

    // if session is still open, push the values
    if (AvSessionState == LE_AVDATA_SESSION_STARTED)
    {
        stat(pushFilePtr, &txFileStat);
        LE_INFO("Size of transmit file = %ld", txFileStat.st_size);

        // Check if a push is already in progress
        if (PushBusy)
        {
            LE_ERROR("Busy: Push in progress");
            return;
        }

        // send actual CoAP response.
        if (txFileStat.st_size <= LE_COAP_MAX_PAYLOAD)
        {
            txStreamStatus = LE_COAP_STREAM_NONE;
        }
        else
        {
            txStreamStatus = LE_COAP_TX_STREAM_START;
            PushBusy = true;
        }

        coapResponseCode = CoapTxStreamHandler(UNSOLICITED_RESPONSE,
                                               ResponsePayload,
                                               &responsePayloadLength,
                                               &txStreamStatus);

        if (coapResponseCode != COAP_NO_ERROR)
        {
            result = le_coap_Push(PUSH_URI,
                                  token,
                                  0,
                                  LWM2M_CONTENT_CBOR,
                                  txStreamStatus,
                                  ResponsePayload,
                                  responsePayloadLength,
                                  PushAckCallBack,
                                  NULL);

            if (result != LE_OK)
            {
                LE_ERROR("Push failed");
            }
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Component initializer.  Must return when done initializing.
 * Note: Assumes session is opened.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    le_avdata_AddSessionStateHandler(SessionHandler, NULL);
    le_coap_AddMessageEventHandler(ExternalCoapHandler, NULL);

    le_avdata_RequestSession();

    // Initialize the TransmitContext
    strncpy(TransmitContext[REQUEST_RESPONSE].filename, GET_RESPONSE_SMALL_STRING, MAX_FILE_PATH_BYTES);
    strncpy(TransmitContext[UNSOLICITED_RESPONSE].filename, TRANSMIT_SMALL_STRING, MAX_FILE_PATH_BYTES);

    // [PushTimer]
    //Set timer to update on server on a regular basis
    ServerUpdateTimerRef = le_timer_Create("serverUpdateTimer");         //create timer
    le_clk_Time_t serverUpdateInterval = { 60, 0 };                      //update server every 60 seconds
    le_timer_SetInterval(ServerUpdateTimerRef, serverUpdateInterval);
    le_timer_SetRepeat(ServerUpdateTimerRef, 0);                         //set repeat to always
    //set callback function to handle timer expiration event
    le_timer_SetHandler(ServerUpdateTimerRef, PushResources);
    //start timer
    le_timer_Start(ServerUpdateTimerRef);
    // [PushTimer]
}
