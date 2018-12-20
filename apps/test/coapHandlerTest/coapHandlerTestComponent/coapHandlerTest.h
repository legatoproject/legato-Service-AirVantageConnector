/**
 * Coap handler test defines
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef _COAP_HANDLER_TEST_H
#define _COAP_HANDLER_TEST_H

// Content types
#define LWM2M_CONTENT_TEXT         0
#define LWM2M_CONTENT_LINK         40
#define LWM2M_CONTENT_OPAQUE       42
#define LWM2M_CONTENT_TLV_OLD      1542
#define LWM2M_CONTENT_TLV          11542
#define LWM2M_CONTENT_JSON_OLD     1543
#define LWM2M_CONTENT_JSON         11543
#define LWM2M_CONTENT_CBOR         60
#define LWM2M_CONTENT_ZCBOR        12118

// File where CoAP PUT will be stored
#define RECEIVED_STREAM_FILE   "receivedStream.cbor"

// File that will be pushed out from device without a server initiated request
// i.e., CoAP POST from device on a specified URI
#define TRANSMIT_SMALL_STRING   "transmitSmallString.cbor"
#define TRANSMIT_LARGE_STRING   "transmitLargeString.cbor"

// URI where PUSH messages (CoAP post from device) will end.
#define PUSH_URI "/push"

// File that will be sent in response to CoAP GET
#define GET_RESPONSE_SMALL_STRING   "getSmallString.cbor"
#define GET_RESPONSE_2KB_STRING     "get2kbString.cbor"
#define GET_RESPONSE_LARGE_STRING   "getLargeString.cbor"

// Maximum size for file name
#define MAX_FILE_PATH_BYTES 256

// CoAP URLs
#define URL_GET_SMALL_STRING    "/smallString"
#define URL_GET_2KB_STRING      "/2kbString"
#define URL_GET_LARGE_STRING    "/largeString"

typedef enum
{
    REQUEST_RESPONSE = 0,
    UNSOLICITED_RESPONSE
}responseType_t;

#endif
