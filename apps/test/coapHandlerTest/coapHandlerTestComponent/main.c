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
#include "cbor.h"

//--------------------------------------------------------------------------------------------------
/**
 * Test payload
 */
//--------------------------------------------------------------------------------------------------
#define TEST_STRING                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

//--------------------------------------------------------------------------------------------------
/**
 * Number of data points that will be pushed to the server when push timer expires.
 */
//--------------------------------------------------------------------------------------------------
#define PUSH_NUM_DATA_POINTS    3000

//--------------------------------------------------------------------------------------------------
/**
 * Number of data points that will be sent on receiving a GET request from server.
 */
//--------------------------------------------------------------------------------------------------
#define GET_NUM_DATA_POINTS        5000

//--------------------------------------------------------------------------------------------------
/**
 * Maximum size of CBOR file that will be generated for test
 */
//--------------------------------------------------------------------------------------------------
#define MAX_SIZE_CBOR_FILE        (256*1024)

//--------------------------------------------------------------------------------------------------
/**
 * Structure defining the CoAP transmit context
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char filename[MAX_FILE_PATH_BYTES];        ///< Filename
    int fp;                                    ///< File pointer for open file (tx in progress)
}
coapTransmitContext_t;

//--------------------------------------------------------------------------------------------------
/**
 * Reference to the timer which triggers a push operation to server.
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t ServerUpdateTimerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Session state
 */
//--------------------------------------------------------------------------------------------------
static le_avdata_SessionState_t AvSessionState = LE_AVDATA_SESSION_STOPPED;

//--------------------------------------------------------------------------------------------------
/**
 * CoAP response payload
 */
//--------------------------------------------------------------------------------------------------
static uint8_t ResponsePayload[LE_COAP_MAX_PAYLOAD_NUM_BYTES];

//--------------------------------------------------------------------------------------------------
/**
 * Transmit context
 */
//--------------------------------------------------------------------------------------------------
static coapTransmitContext_t TransmitContext[2];

//--------------------------------------------------------------------------------------------------
/**
 * Status of the current push operation.
 */
//--------------------------------------------------------------------------------------------------
static bool PushBusy = false;

//--------------------------------------------------------------------------------------------------
/**
 * Set of different test modes.
 */
//--------------------------------------------------------------------------------------------------
#define COAP_TEST_SMALL_STRING          0
#define COAP_TEST_LARGE_STRING          1
#define COAP_TEST_LARGE_STRING_CANCEL   2
#define COAP_TEST_MAX                   3

//--------------------------------------------------------------------------------------------------
/**
 * Test mode that is currently running.
 */
//--------------------------------------------------------------------------------------------------
static int TestMode = -1;

//-------------------------------------------------------------------------------------------------
/**
 * Write received data to a file.
 */
//-------------------------------------------------------------------------------------------------
static void CopyToFile
(
    char* filePath,               ///< [IN] Read file from this location
    const uint8_t* bufferPtr,     ///< [IN] Copy read data to this location
    size_t length,                ///< [IN] Length to read from file
    bool isNewFile                ///< [IN] Start fresh or continue from current offset?
)
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
    }

    LE_ERROR("Failed to write to file %s", filePath);
}


//-------------------------------------------------------------------------------------------------
/**
 * Read data from file to buffer.
 */
//-------------------------------------------------------------------------------------------------
static int CopyToBuffer
(
    responseType_t respType,            ///< [IN] Response to incoming message or unsolicited?
    uint8_t* bufferPtr,                 ///< [IN] Buffer to write data to
    size_t maxNumBytes,                 ///< [IN] Length to read from file
    bool isNewFile                      ///< [IN] Start fresh or continue from current offset?
)
{
    int readLength = 0;
    coapTransmitContext_t* contextPtr;
    size_t result;

    contextPtr = &TransmitContext[respType];

    if (isNewFile)
    {
        contextPtr->fp = open(contextPtr->filename, O_RDONLY,  S_IRUSR | S_IWUSR);
        LE_ASSERT(contextPtr->fp != -1);
    }

    if (contextPtr->fp)
    {
        int bytesRead = 0;
        uint8_t buffer[maxNumBytes+1];
        memset(buffer, 0, sizeof(buffer));

        do
        {
            result = (read(contextPtr->fp, bufferPtr + readLength, (maxNumBytes - readLength)));
            if (result > 0)
            {
                readLength += result;
                LE_ASSERT(bytesRead <= maxNumBytes);
            }

            if ((result < 0) && (errno != EINTR))
            {
                LE_ERROR("Error writing the test pattern to file");
                exit(1);
            }
        }
        while ((readLength < maxNumBytes) && (result != 0));

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
    le_avdata_SessionState_t sessionState,          ///< [IN] AVC Session state
    void* contextPtr                                ///< [IN] Context
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
 *
 * @return:
 *    - CoAP response code
 */
//-------------------------------------------------------------------------------------------------
static le_coap_Code_t CoapRxStreamHandler
(
    le_coap_StreamStatus_t streamStatus,        ///< [IN] CoAP stream status received
    const uint8_t* bufferPtr,                   ///< [IN] Buffer to copy data to
    size_t length                               ///< [IN] size of data received
)
{
    // Copy stream data to buffer or file
    // send ack for incoming stream or start processing payload if stream succeeded.
    switch (streamStatus)
    {
        case LE_COAP_RX_STREAM_START:
            LE_INFO("Stream start: Create file and write received data");
            CopyToFile(RECEIVED_STREAM_FILE, bufferPtr, length, true);
            return LE_COAP_CODE_NO_RESPONSE;

        case LE_COAP_RX_STREAM_IN_PROGRESS:
            LE_INFO("Stream in progress: Copy received data to file");
            CopyToFile(RECEIVED_STREAM_FILE, bufferPtr, length, false);
            return LE_COAP_CODE_NO_RESPONSE;

        case LE_COAP_RX_STREAM_END:
            LE_INFO("Stream completed: Start processing received data");
            CopyToFile(RECEIVED_STREAM_FILE, bufferPtr, length, false);
            return LE_COAP_CODE_204_CHANGED;

        case LE_COAP_RX_STREAM_ERROR:
            LE_INFO("Stream cancelled");
            return LE_COAP_CODE_500_INTERNAL_SERVER_ERROR;

        default:
            LE_INFO("Unexpected stream status during PUT");
            return LE_COAP_CODE_500_INTERNAL_SERVER_ERROR;
    }
}


//-------------------------------------------------------------------------------------------------
/**
 * Handler for transmitting CoAP Stream
 *
 * @return:
 *    - CoAP response code
 */
//-------------------------------------------------------------------------------------------------
static le_coap_Code_t CoapTxStreamHandler
(
    responseType_t respType,                            ///< [IN] Unsolicited / request response
    uint8_t* bufferPtr,                                 ///< [IN] Buffer to write data
    size_t* lengthPtr,                                  ///< [IN] length of data
    le_coap_StreamStatus_t* txStreamStatusPtr           ///< [IN] Status of transmit stream
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

            *lengthPtr = CopyToBuffer(respType, bufferPtr, LE_COAP_MAX_PAYLOAD, true);

            if (*lengthPtr == -1)
            {
                *lengthPtr = 0;    // reset size of payload to 0
                *txStreamStatusPtr = LE_COAP_TX_STREAM_ERROR;
                return LE_COAP_CODE_500_INTERNAL_SERVER_ERROR;
            }

            *txStreamStatusPtr = LE_COAP_STREAM_NONE;
            return LE_COAP_CODE_205_CONTENT;

        case LE_COAP_TX_STREAM_START:
            LE_INFO("Stream started: Start sending data from file");
            *lengthPtr = CopyToBuffer(respType, bufferPtr, LE_COAP_MAX_PAYLOAD, true);

            if (*lengthPtr == -1)
            {
                *lengthPtr = 0;    // reset size of payload to 0
                *txStreamStatusPtr = LE_COAP_TX_STREAM_ERROR;
                return LE_COAP_CODE_500_INTERNAL_SERVER_ERROR;
            }

            *txStreamStatusPtr = LE_COAP_TX_STREAM_START;
            return LE_COAP_CODE_205_CONTENT;

        case LE_COAP_TX_STREAM_IN_PROGRESS:
            if (TestMode == COAP_TEST_LARGE_STRING_CANCEL)
            {
                LE_INFO("Testing Stream Cancellation");
                *txStreamStatusPtr = LE_COAP_TX_STREAM_CANCEL;
                PushBusy = false;
                return LE_COAP_CODE_205_CONTENT;
            }
            LE_INFO("Stream in progress: Continue sending data from file");
            *lengthPtr = CopyToBuffer(respType, bufferPtr, LE_COAP_MAX_PAYLOAD, false);

            if (*lengthPtr == -1)
            {
                *lengthPtr = 0;    // reset size of payload to 0
                *txStreamStatusPtr = LE_COAP_TX_STREAM_ERROR;
                return LE_COAP_CODE_500_INTERNAL_SERVER_ERROR;
            }

            if (*lengthPtr == LE_COAP_MAX_PAYLOAD)
            {
                *txStreamStatusPtr = LE_COAP_TX_STREAM_IN_PROGRESS;
                return LE_COAP_CODE_205_CONTENT;
            }

            *txStreamStatusPtr = LE_COAP_TX_STREAM_END;
            return LE_COAP_CODE_205_CONTENT;

        case LE_COAP_TX_STREAM_END:
            LE_INFO("Stream completed");
            *txStreamStatusPtr = LE_COAP_TX_STREAM_END;
            return LE_COAP_CODE_NO_RESPONSE;

        case LE_COAP_TX_STREAM_ERROR:
            LE_INFO("Stream cancelled");
            *txStreamStatusPtr = LE_COAP_TX_STREAM_END;
            return LE_COAP_CODE_500_INTERNAL_SERVER_ERROR;

        default:
            LE_INFO("Unexpected stream status during GET");
            *txStreamStatusPtr = LE_COAP_TX_STREAM_END;
            return LE_COAP_CODE_500_INTERNAL_SERVER_ERROR;
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
    le_coap_PushStatus_t status,                ///< [IN] Push status
    const uint8_t* token,                       ///< [IN] token
    size_t tokenLength,                         ///< [IN] token length
    void* contextPtr                            ///< [IN] Push context
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
    le_coap_Code_t code,                            ///< [IN] CoAP method or response code
    le_coap_StreamStatus_t streamStatus,            ///< [IN] CoAP stream status
    uint16_t messageId,                             ///< [IN] message id
    uint16_t contentType,                           ///< [IN] content type
    const char* uri,                                ///< [IN] URI of resource
    const uint8_t* token,                           ///< [IN] token
    size_t tokenLength,                             ///< [IN] token length
    const uint8_t* payload,                         ///< [IN] CoAP payload
    size_t payloadLength,                           ///< [IN] CoAP payload length
    void* contextPtr                                ///< [IN] Context
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
                                     LE_COAP_CODE_204_CHANGED,
                                     LE_COAP_STREAM_NONE,
                                     ResponsePayload,
                                     0);
            }
            else
            {
                coapResponseCode = CoapRxStreamHandler(streamStatus, payload, payloadLength);

                if (coapResponseCode != LE_COAP_CODE_NO_RESPONSE)
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
            else if (strncmp(uri, URL_GET_LARGE_STRING, sizeof(URL_GET_LARGE_STRING)) == 0)
            {
                strncpy(getReponseFile, GET_RESPONSE_LARGE_STRING, MAX_FILE_PATH_BYTES);
            }
            else
            {
                LE_ERROR("URI %s not found", uri);
                le_coap_SendResponse(messageId,
                                     token,
                                     tokenLength,
                                     LWM2M_CONTENT_CBOR,
                                     LE_COAP_CODE_404_NOT_FOUND,
                                     LE_COAP_STREAM_NONE,
                                     (const uint8_t*)"",
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

             if (coapResponseCode != LE_COAP_CODE_NO_RESPONSE)
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
                LE_INFO("PUSH stream in progress: Continue streaming data from file");
                txStreamStatus = LE_COAP_TX_STREAM_IN_PROGRESS;

                coapResponseCode = CoapTxStreamHandler(UNSOLICITED_RESPONSE,
                                                       ResponsePayload,
                                                       &responsePayloadLength,
                                                       &txStreamStatus);

                if (coapResponseCode != LE_COAP_CODE_NO_RESPONSE)
                {
                    // send actual CoAP response.
                    LE_INFO("pushing: length %"PRIuS" streamStatus %d",
                            responsePayloadLength, txStreamStatus);
                    result = le_coap_Push(PUSH_URI,
                                          token,
                                          0,
                                          LWM2M_CONTENT_CBOR,
                                          txStreamStatus,
                                          ResponsePayload,
                                          responsePayloadLength);
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
static void PushResources
(
    le_timer_Ref_t  timerRef                    ///< [IN] Timer reference
)
{
    uint32_t coapResponseCode;
    struct stat txFileStat;
    size_t responsePayloadLength = 0;
    le_coap_StreamStatus_t txStreamStatus;
    uint8_t token[LE_COAP_MAX_TOKEN_NUM_BYTES];
    le_result_t result;

    TestMode = (TestMode + 1) % COAP_TEST_MAX;
    LE_INFO("Start pushing data: mode %d", TestMode);

    char* pushFilePtr = TransmitContext[UNSOLICITED_RESPONSE].filename;

    // Alternate between a small and large string
    if (TestMode == COAP_TEST_SMALL_STRING)
    {
        strncpy(pushFilePtr, TRANSMIT_SMALL_STRING, MAX_FILE_PATH_BYTES);
    }
    else
    {
        strncpy(pushFilePtr, TRANSMIT_LARGE_STRING, MAX_FILE_PATH_BYTES);
    }

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
        LE_INFO("COAP response code %d", coapResponseCode);

        if (coapResponseCode != LE_COAP_CODE_NO_RESPONSE)
        {
            LE_INFO("pushing: length %"PRIuS" streamStatus %d",
                    responsePayloadLength, txStreamStatus);
            result = le_coap_Push(PUSH_URI,
                                  token,
                                  0,
                                  LWM2M_CONTENT_CBOR,
                                  txStreamStatus,
                                  ResponsePayload,
                                  responsePayloadLength);

            if (result != LE_OK)
            {
                LE_ERROR("Push failed");
            }
        }
    }
}

//-------------------------------------------------------------------------------------------------
/**
 * Generate a CBOR payload
 */
//-------------------------------------------------------------------------------------------------
static void CreateCborData
(
    char* outputFile,                       ///< [IN] Output file name
    int numData                             ///< [IN] Number of data points
)
{
    CborEncoder mapNode;
    char path[256];
    uint8_t buf[MAX_SIZE_CBOR_FILE] = {0};
    CborEncoder encoder;

    FILE* f = fopen(outputFile, "wb");

    LE_ASSERT(f != NULL);
    cbor_encoder_init(&encoder, (uint8_t*)&buf, sizeof(buf), 0);

    LE_ASSERT(CborNoError == cbor_encoder_create_map(&encoder, &mapNode, CborIndefiniteLength));

    for (int i = 0; i < numData; i++)
    {
        snprintf(path, sizeof(path), "%s%d", "test-", i);
        LE_ASSERT(CborNoError == cbor_encode_text_stringz(&mapNode, path));
        LE_ASSERT(CborNoError == cbor_encode_text_string(&mapNode, TEST_STRING, strlen(TEST_STRING)));
    }

    LE_ASSERT(CborNoError == cbor_encoder_close_container(&encoder, &mapNode));

    size_t cborSize = cbor_encoder_get_buffer_size(&encoder, buf);

    LE_ASSERT(cborSize < sizeof(buf))

    fwrite(buf, 1, cborSize, f);
    fclose(f);
}


//--------------------------------------------------------------------------------------------------
/**
 * Component initializer.  Must return when done initializing.
 * Note: Assumes session is opened.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("Start CoapHandler Test");
    le_avdata_AddSessionStateHandler(SessionHandler, NULL);
    le_coap_AddMessageEventHandler(ExternalCoapHandler, NULL);
    le_coap_AddPushEventHandler(PushAckCallBack, NULL);

    le_avdata_RequestSession();

    // Initialize the TransmitContext
    strncpy(TransmitContext[REQUEST_RESPONSE].filename, GET_RESPONSE_SMALL_STRING, MAX_FILE_PATH_BYTES);
    strncpy(TransmitContext[UNSOLICITED_RESPONSE].filename, TRANSMIT_SMALL_STRING, MAX_FILE_PATH_BYTES);

    // Generate CBOR test vectors with many data points
    CreateCborData(TRANSMIT_LARGE_STRING, PUSH_NUM_DATA_POINTS);
    CreateCborData(GET_RESPONSE_LARGE_STRING, GET_NUM_DATA_POINTS);

    // Set timer to update on server on a regular basis
    ServerUpdateTimerRef = le_timer_Create("serverUpdateTimer");       //create timer
    le_clk_Time_t serverUpdateInterval = { 60, 0 };                    //update server every 60 seconds
    le_timer_SetInterval(ServerUpdateTimerRef, serverUpdateInterval);
    le_timer_SetRepeat(ServerUpdateTimerRef, 0);                       //set repeat to always

    // set callback function to handle timer expiration event
    le_timer_SetHandler(ServerUpdateTimerRef, PushResources);

    // start timer
    le_timer_Start(ServerUpdateTimerRef);
}
