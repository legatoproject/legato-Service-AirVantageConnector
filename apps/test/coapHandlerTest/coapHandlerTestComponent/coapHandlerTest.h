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

#define COAP(x)     (uint8_t)x

/*
 * Error code as defined in RFC 7252 section 12.1.2
 */
#define COAP_NO_ERROR                   (uint8_t)0x00
#define COAP_IGNORE                     (uint8_t)0x01

#define COAP_201_CREATED                COAP(0x41)
#define COAP_202_DELETED                COAP(0x42)
#define COAP_204_CHANGED                COAP(0x44)
#define COAP_205_CONTENT                COAP(0x45)
#define COAP_231_CONTINUE               COAP(0x5F)
#define COAP_400_BAD_REQUEST            COAP(0x80)
#define COAP_401_UNAUTHORIZED           COAP(0x81)
#define COAP_402_BAD_OPTION             COAP(0x82)
#define COAP_404_NOT_FOUND              COAP(0x84)
#define COAP_405_METHOD_NOT_ALLOWED     COAP(0x85)
#define COAP_406_NOT_ACCEPTABLE         COAP(0x86)
#define COAP_408_REQ_ENTITY_INCOMPLETE  COAP(0x88)
#define COAP_412_PRECONDITION_FAILED    COAP(0x8C)
#define COAP_413_ENTITY_TOO_LARGE       COAP(0x8D)
#define COAP_500_INTERNAL_SERVER_ERROR  COAP(0xA0)
#define COAP_501_NOT_IMPLEMENTED        COAP(0xA1)
#define COAP_503_SERVICE_UNAVAILABLE    COAP(0xA3)

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

// Maximum size for file name
#define MAX_FILE_PATH_BYTES 256

// CoAP URLs
#define URL_GET_SMALL_STRING    "/smallString"
#define URL_GET_2KB_STRING      "/2kbString"

typedef enum
{
    REQUEST_RESPONSE = 0,
    UNSOLICITED_RESPONSE
}responseType_t;

#endif
