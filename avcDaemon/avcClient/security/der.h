/**
 * @file der.h
 *
 * Encodes and decodes ANS.1 structures with DER (Distinguished Encoding Rules) encodings.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef IOT_KEY_STORE_DER_INCLUDE_GUARD
#define IOT_KEY_STORE_DER_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Supported ASN.1 types.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    DER_PRE_FORMED = 0,             ///< Special type that specifies a preformatted ANS.1/DER
                                    ///  encoding.  May be used to create nested sequences or
                                    ///  preprocessed values.
    DER_NATIVE_UINT,                ///< This maps to an unsigned integer but values can be given in
                                    ///  in the native machine format, ie. uint16_t, size_t, etc.
    DER_UNSIGNED_INT,               ///< Unsigned integer type.  Must be in big endian format.
                                    ///  A leading 0x00 will be added as necessary.
    DER_INTEGER,                    ///< Integer type.  Must be in big endian format.
    DER_OCTET_STRING,               ///< Octet array type.
    DER_IA5_STRING,                 ///< IA5 string type.
    DER_CONTEXT_SPECIFIC = 0x80     ///< Used to specify context-specific types.  To create a
                                    ///  context-specific type OR this value with your chosen type
                                    ///  value.  Currently only supports low-tag-number forms so
                                    ///  type values can have any value between 0x00 and 0x3E
                                    ///  inclusive.
}
der_Type_t;


//--------------------------------------------------------------------------------------------------
/**
 * Item to encode.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    der_Type_t          type;
    const uint8_t*      valPtr;
    size_t              valSize;
}
der_EncodeItem_t;


//--------------------------------------------------------------------------------------------------
/**
 * Item to decode.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    der_Type_t          type;           ///< Type.
    uint8_t*            valPtr;         ///< Buffer to store the decoded value.
    size_t*             valSizePtr;     ///< Buffer size on input.
}
der_DecodeItem_t;


//--------------------------------------------------------------------------------------------------
/**
 * Encode an item in ASN.1/DER encoding.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if either valPtr, bufPtr or bufSizePtr is NULL or the type is unsupported.
 *      IKS_OVERFLOW if the buffer is too small to hold the encoding.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t der_EncodeVal
(
    der_Type_t          type,           ///< [IN] Type.
    const uint8_t*      valPtr,         ///< [IN] Value to encode.
    size_t              valSize,        ///< [IN] Number of bytes in value.
    uint8_t*            bufPtr,         ///< [OUT] Buffer to hold the encoding.
    size_t*             bufSizePtr      ///< [IN/OUT] On entry: size of the buffer.
                                        ///           On exit: size of the encoding.
);


//--------------------------------------------------------------------------------------------------
/**
 * Decode a value from an ASN.1/DER encoding.
 *
 * @note
 *      If there are unread bytes in the encodingPtr after decoding the value IKS_OUT_OF_RANGE is
 *      returned.  It is up to the caller to decide if this an error.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if either encodingPtr, bufPtr or bufSizePtr is NULL.
 *      IKS_FORMAT_ERROR if there is a format error or a type mismatch.
 *      IKS_OVERFLOW if the buffer is too small to hold the decoded value.
 *      IKS_OUT_OF_RANGE if there are extra bytes in the encoding.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t der_DecodeVal
(
    der_Type_t          type,           ///< [IN] Expected type.
    const uint8_t*      encodingPtr,    ///< [IN] Encoding
    size_t              encodingSize,   ///< [IN] Encoding size.
    size_t*             bytesReadPtr,   ///< [OUT] Bytes read from encoding.  NULL if not used.
    uint8_t*            bufPtr,         ///< [OUT] Buffer to hold the decoded value.
    size_t*             bufSizePtr      ///< [IN/OUT] On entry: size of the buffer.
                                        ///           On exit: size of the value.
);


//--------------------------------------------------------------------------------------------------
/**
 * Encode a list of items into an ASN.1/DER format.
 *
 * When a DER_PRE_FORMED type is encountered the entire content of the item is added verbatim to the
 * output without any additional formatting.
 *
 * When an items valPtr is NULL the item is ignored which may be useful for OPTIONAL fields.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if either valPtr, bufPtr or bufSizePtr is NULL.
 *      IKS_OVERFLOW if the buffer is too small to hold the encoded list.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t der_EncodeList
(
    der_EncodeItem_t    items[],        ///< [IN] List of items.
    size_t              numItems,       ///< [IN] Number of items.
    uint8_t*            bufPtr,         ///< [OUT] Buffer to hold the encoded list.
    size_t*             bufSizePtr      ///< [IN/OUT] On entry: size of the buffer.
                                        ///           On exit: size of the encoded list.
);


//--------------------------------------------------------------------------------------------------
/**
 * Decode a list of ASN.1/DER items.
 *
 * When an items valPtr is NULL the item is ignored otherwise on entry the valSizePtr for the item
 * must specify the length of the item buffer.  On exit the valSizePtr will be updated to the value
 * size.
 *
 * When a DER_PRE_FORMED type is requested the entire item is copied to the valuePtr buffer
 * including the tag, length and value.  The valSizePtr is updated to include the size of the entire
 * item including the tag, length and value.
 *
 * @note
 *      If there are no more specified items but there are still bytes to read in the bufPtr
 *      IKS_OUT_OF_RANGE is returned but all the specified items are still decoded.  It is up to the
 *      caller to decide if this an error.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if either encodingPtr is NULL.
 *      IKS_FORMAT_ERROR if there is an invalid type, a type mismatch or if there are no more bytes
 *                       in bufPtr.
 *      IKS_OVERFLOW if an item's valuePtr buffer is too small to hold the value.
 *      IKS_OUT_OF_RANGE if there are no more specified items but there are still bytes to read in
 *                       the bufPtr.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t der_DecodeList
(
    const uint8_t*      encodingPtr,    ///< [IN] Encoding
    size_t              encodingSize,   ///< [IN] Encoding size.
    size_t*             bytesReadPtr,   ///< [OUT] Bytes read from encoding.  NULL if not used.
    der_DecodeItem_t    items[],        ///< [IN/OUT] List of items.
    size_t              numItems        ///< [IN] Number of items.
);


//--------------------------------------------------------------------------------------------------
/**
 * Encode a list of items into an ASN.1 sequence in DER format.
 *
 * When a DER_PRE_FORMED type is encountered the entire content of the item is added verbatim to the
 * output without any additional formatting.
 *
 * When an items valPtr is NULL the item is ignored which may be useful for OPTIONAL fields.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if either valPtr, bufPtr or bufSizePtr is NULL.
 *      IKS_OVERFLOW if the buffer is too small to hold the encoded sequence.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t der_EncodeSeq
(
    der_EncodeItem_t    items[],        ///< [IN] List of items.
    size_t              numItems,       ///< [IN] Number of items.
    uint8_t*            bufPtr,         ///< [OUT] Buffer to hold the encoded list.
    size_t*             bufSizePtr      ///< [IN/OUT] On entry: size of the buffer.
                                        ///           On exit: size of the encoded list.
);


//--------------------------------------------------------------------------------------------------
/**
 * Decode an ASN.1 sequence in DER format into the list of specified items.
 *
 * When an items valPtr is NULL the item is ignored otherwise on entry the valSizePtr for the item
 * must specify the length of the item buffer.  On exit the valSizePtr will be updated to the value
 * size.
 *
 * When a DER_PRE_FORMED type is requested the entire item is copied to the valuePtr buffer
 * including the tag, length and value.  The valSizePtr is updated to include the size of the entire
 * item including the tag, length and value.
 *
 * @note
 *      If there are no more specified items but there are still bytes to read in the bufPtr
 *      IKS_OUT_OF_RANGE is returned but all the specified items are still decoded.  It is up to the
 *      caller to decide if this an error.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if either encodingPtr is NULL.
 *      IKS_FORMAT_ERROR if there is an invalid type, a type mismatch or if there are no more bytes
 *                       in bufPtr.
 *      IKS_OVERFLOW if an item's valuePtr buffer is too small to hold the value.
 *      IKS_OUT_OF_RANGE if there are no more specified items but there are still bytes to read in
 *                       the bufPtr.
 */
//--------------------------------------------------------------------------------------------------
iks_result_t der_DecodeSeq
(
    const uint8_t*      encodingPtr,    ///< [IN] Encoding
    size_t              encodingSize,   ///< [IN] Encoding size.
    size_t*             bytesReadPtr,   ///< [OUT] Bytes read from encoding.  NULL if not used.
    der_DecodeItem_t    items[],        ///< [IN/OUT] List of items.
    size_t              numItems        ///< [IN] Number of items.
);


#endif // IOT_KEY_STORE_DER_INCLUDE_GUARD