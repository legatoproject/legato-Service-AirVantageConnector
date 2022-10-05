/**
 * @file der.c
 *
 * Encodes and decodes ANS.1 structures with DER (Distinguished Encoding Rules) encodings.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "iks_basic.h"
#include "err.h"
#include "bigEndianInt.h"
#include "der.h"


//--------------------------------------------------------------------------------------------------
/**
 * Standard ASN.1 Tags
 */
//--------------------------------------------------------------------------------------------------
#define ASN1_SEQUENCE               0x30
#define ASN1_INTEGER                0x02
#define ASN1_OCTET_STRING           0x04
#define ASN1_IA5_STRING             0x16


//--------------------------------------------------------------------------------------------------
/**
 * Currently only supports low-tag-number types.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_LOW_TAG_NUMBER          0x3E


//--------------------------------------------------------------------------------------------------
/**
 * Get the number of bytes of the length field not including the first byte that just indicates the
 * length of the length field.
 *
 * @return
 *      Size of the length.
 */
//--------------------------------------------------------------------------------------------------
static uint8_t GetNumLenBytes
(
    uint8_t         firstByte       ///< [IN] First byte of the length field.
)
{
    if ((firstByte & 0x80) == 0)
    {
        return 0;
    }

    return firstByte & ~0x80;
}


//--------------------------------------------------------------------------------------------------
/**
 * Calculates the number of bytes needed for the length field for a given length value.
 *
 * @return
 *      Size of the length.
 */
//--------------------------------------------------------------------------------------------------
static uint8_t CalcLenSize
(
    size_t          length          ///< [IN] Value length.
)
{
    uint8_t sizeOfLen = 1;

    if (length >= 0x80)
    {
        // The length field is at least 2 bytes long.
        sizeOfLen = 2;

        // See how many more bytes are needed to represent the length field.
        size_t mask = 0xFF;
        size_t i = 0;
        for (; i < sizeof(size_t); i++)
        {
            if (length > mask)
            {
                sizeOfLen++;
            }
            else
            {
                break;
            }

            mask = mask << 8;
            mask += 0xFF;
        }
    }

    return sizeOfLen;
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the number of leading zero bytes (not including the last byte even if it is a zero).
 *
 * @return
 *      Number of leading zeros.
 */
//--------------------------------------------------------------------------------------------------
static size_t GetNumLeadZeros
(
    const uint8_t*  bufPtr,         ///< [IN] Buffer.
    size_t          bufSize         ///< [IN] Buffer size.
)
{
    if (bufSize == 0)
    {
        return 0;
    }

    // Get the first non-zero byte but don't go past the end.
    size_t i = 0;
    for (; i < bufSize-1; i++)
    {
        if (bufPtr[i] != 0)
        {
            break;
        }
    }

    return i;
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the unsigned integer that is contained in the specified buffer with unnecessary leading
 * zeros stripped off.  The outSizePtr will contain the size of the returned unsigned integer.
 *
 * If the buffer does not contain any leading zeros but instead requires that a leading zero be
 * added then the returned value will point to the first byte in the buffer but the outSizePtr will
 * be equal to the original buffer size + one.
 *
 * This function is helpful when formatting unsigned big endian integers for DER encoding/decoding.
 *
 * @return
 *      Pointer to the unsigned integer in the buffer.
 */
//--------------------------------------------------------------------------------------------------
static const uint8_t* GetUnsignedInt
(
    const uint8_t*  bufPtr,         ///< [IN] Buffer.
    size_t          bufSize,        ///< [IN] Buffer size.
    size_t*         outSizePtr      ///< [OUT] Size of the unsigned integer.
)
{
    // Get the first non-zero byte but don't go past the end.
    size_t i = GetNumLeadZeros(bufPtr, bufSize);

    // A leading zero is needed if the high order bit is a 1.
    if ( (bufSize > 0) && ((bufPtr[i] & 0x80) != 0) )
    {
        if (i > 0)
        {
            // Move the index back one.
            i--;
        }
        else
        {
            // Can't move the index back any further so tell the caller to manually add a lead zero.
            bufSize++;
        }
    }

    *outSizePtr = bufSize - i;

    return bufPtr + i;
}


//--------------------------------------------------------------------------------------------------
/**
 * Calculates the total size of an item in DER encoding, including the tag, length and value fields.
 *
 * @return
 *      IKS_OK on success.
 *      IKS_OVERFLOW if the item size is too big to fit in a size_t type.
 */
//--------------------------------------------------------------------------------------------------
static iks_result_t CalcItemSize
(
    der_Type_t      type,           ///< [IN] Type.
    size_t          length,         ///< [IN] Length.
    const uint8_t*  valuePtr,       ///< [IN] Value.
    size_t*         itemSizePtr     ///< [OUT] Calculated item size.
)
{
    if (type == DER_PRE_FORMED)
    {
        *itemSizePtr = length;
        return IKS_OK;
    }

    size_t size = length;

    if (type == DER_UNSIGNED_INT)
    {
        GetUnsignedInt(valuePtr, length, &size);
    }

    // Check for integer overflow.
    size_t tagAndLenSize = CalcLenSize(size) + 1;

    if (size > (SIZE_MAX - tagAndLenSize))
    {
        return IKS_OVERFLOW;
    }

    // Add one byte for the tag.
    *itemSizePtr = size + tagAndLenSize;
    return IKS_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the tag value.
 *
 * @return
 *      IKS_OK on success.
 *      IKS_INVALID_PARAM if the type is unrecognized.
 */
//--------------------------------------------------------------------------------------------------
static iks_result_t GetTag
(
    der_Type_t      type,           ///< [IN] Type.
    uint8_t*        tagPtr          ///< [OUT] Tag value.
)
{
    switch(type)
    {
        case DER_NATIVE_UINT:
        case DER_UNSIGNED_INT:
        case DER_INTEGER:
            *tagPtr = ASN1_INTEGER;
            break;

        case DER_OCTET_STRING:
            *tagPtr = ASN1_OCTET_STRING;
            break;

        case DER_IA5_STRING:
            *tagPtr = ASN1_IA5_STRING;
            break;

        default:
            if ((type & (BIT7 | BIT6)) == DER_CONTEXT_SPECIFIC)
            {
                if ((type & ~(BIT7 | BIT6)) > MAX_LOW_TAG_NUMBER)
                {
                    ERR_PRINT("Unsupported context-specific type: %d.", type);
                    return IKS_INVALID_PARAM;
                }

                *tagPtr = (uint8_t)type;
            }
            else
            {
                ERR_PRINT("Unsupported ASN.1/DER type: %d.", type);
                return IKS_INVALID_PARAM;
            }
            break;
    }

    return IKS_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Write the length field into the buffer.
 *
 * @return
 *      Number of bytes written.
 */
//--------------------------------------------------------------------------------------------------
static size_t WriteLength
(
    size_t          length,         ///< [IN] Length of the item.
    uint8_t*        bufPtr          ///< [OUT] The buffer to write to.
)
{
    // Add one byte for the tag.
    uint8_t sizeOfLen = CalcLenSize(length);

    if (sizeOfLen == 1)
    {
        bufPtr[0] = (uint8_t)length;
        return 1;
    }

    // Add the size of length to the length field.
    uint8_t numBytes = sizeOfLen-1;
    bufPtr[0] = 0x80 + numBytes;

    if (bei_ConvertUnsigned((const uint8_t*)(&length), numBytes, bufPtr + 1, numBytes) != IKS_OK)
    {
        FATAL_HALT("Could not convert length value to an array.");
    }

    return sizeOfLen;
}


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
)
{
    // Check params.
    if ( (valPtr == NULL) || (bufPtr == NULL) || (bufSizePtr == NULL) )
    {
        return IKS_INVALID_PARAM;
    }

    if (*bufSizePtr == 0)
    {
        return IKS_OVERFLOW;
    }

    // Convert small integers to an integer array.
    uint8_t uintArray[sizeof(size_t)];

    if (type == DER_NATIVE_UINT)
    {
        if (!bei_IsNativeBigEndian())
        {
            if (bei_ConvertUnsigned(valPtr, valSize, uintArray, sizeof(uintArray)) != IKS_OK)
            {
                return IKS_INVALID_PARAM;
            }
            valPtr = uintArray;
        }

        type = DER_UNSIGNED_INT;
    }

    // Check the output buffer size is large enough.
    size_t itemSize;

    if ( (CalcItemSize(type, valSize, valPtr, &itemSize) != IKS_OK) ||
         (itemSize > *bufSizePtr) )
    {
        return IKS_OVERFLOW;
    }

    // Encode.
    if (type == DER_PRE_FORMED)
    {
        // Just write the entire pre-formatted sequence directly.
        memcpy(bufPtr, valPtr, valSize);
        *bufSizePtr = valSize;
        return IKS_OK;
    }

    uint8_t* currPtr = bufPtr;

    // Write the item tag.
    uint8_t tag;

    if (GetTag(type, &tag) != IKS_OK)
    {
        return IKS_INVALID_PARAM;
    }

    *currPtr = tag;
    currPtr++;

    // Adjust the item value and length if neccesary.
    const uint8_t* srcPtr = valPtr;
    size_t srcSize = valSize;

    if (type == DER_UNSIGNED_INT)
    {
        srcPtr = GetUnsignedInt(valPtr, valSize, &srcSize);
    }

    // Write the item length.
    size_t l = WriteLength(srcSize, currPtr);
    currPtr += l;

    // Write the item value.
    if (srcSize > valSize)
    {
        // Add a leading zero.
        *currPtr = 0;
        currPtr++;

        srcSize--;
    }

    memcpy(currPtr, srcPtr, srcSize);

    *bufSizePtr = itemSize;

    return IKS_OK;
}


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
 *      IKS_INVALID_PARAM if either encodingPtr, bufPtr or bufSizePtr is NULL or type is
 *                        unrecognized.
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
)
{
    // Check params.
    if ( (encodingPtr == NULL) || (bufPtr == NULL) || (bufSizePtr == NULL) )
    {
        return IKS_INVALID_PARAM;
    }

    if (*bufSizePtr == 0)
    {
        return IKS_OVERFLOW;
    }

    size_t index = 0;

    // Get the tag from the encoding.
    if (encodingSize < 2)
    {
        ERR_PRINT("Tag and/or length is missing.");
        return IKS_FORMAT_ERROR;
    }

    uint8_t tag = encodingPtr[index++];

    // Check that the tag matches the specified type except for preformatted types.
    if (type != DER_PRE_FORMED)
    {
        uint8_t expectedTag;

        if (GetTag(type, &expectedTag) != IKS_OK)
        {
            return IKS_INVALID_PARAM;
        }

        if (expectedTag != tag)
        {
            ERR_PRINT("Mismatched type. Tag is: %u but expected %u.", tag, expectedTag);
            return IKS_FORMAT_ERROR;
        }
    }

    // Get the length from the buffer.
    size_t itemLen = 0;
    uint8_t lenIndicator = encodingPtr[index++];
    uint8_t numLenBytes = GetNumLenBytes(lenIndicator);

    if (numLenBytes == 0)
    {
        itemLen = lenIndicator;
    }
    else
    {
        if (encodingSize < index + numLenBytes)
        {
            ERR_PRINT("End of encoding.");
            return IKS_FORMAT_ERROR;
        }

        if (bei_ConvertUnsigned(&(encodingPtr[index]), numLenBytes,
                                (uint8_t*)(&itemLen), sizeof(itemLen)) != IKS_OK)
        {
            return IKS_OVERFLOW;
        }

        index += numLenBytes;
    }

    // Calculate the number of bytes read from the encoding.
    size_t numBytesRead = index + itemLen;

    // Calculate the bytes to copy.
    if (type == DER_PRE_FORMED)
    {
        // Copy the entire item including the tag, length indicator, length and data.
        itemLen = itemLen + numLenBytes + 2;
        index = 0;
    }
    else if (tag == DER_UNSIGNED_INT)
    {
        if (encodingSize < index + itemLen)
        {
            ERR_PRINT("End of encoding. %zu, %zu, %zu", encodingSize, index, itemLen);
            return IKS_FORMAT_ERROR;
        }

        size_t numLeadZeros = GetNumLeadZeros(&(encodingPtr[index]), itemLen);

        if (itemLen < numLeadZeros)
        {
            return IKS_INVALID_PARAM;
        }

        itemLen -= numLeadZeros;
        index += numLeadZeros;
    }

    // Check if the item will fit into the user buffer.
    if (itemLen > *bufSizePtr)
    {
        ERR_PRINT("Item buffer size %zu is too small.  Need %zu bytes.", *bufSizePtr, itemLen);
        return IKS_OVERFLOW;
    }

    // Check if the length is correct.
    if (encodingSize < index + itemLen)
    {
        ERR_PRINT("End of encoding.");
        return IKS_FORMAT_ERROR;
    }

    // Copy the value to the user buffer.
    if (type == DER_NATIVE_UINT)
    {
        if (bei_ConvertUnsigned(&(encodingPtr[index]), itemLen,
                                bufPtr, *bufSizePtr) != IKS_OK)
        {
            return IKS_OVERFLOW;
        }
    }
    else
    {
        memcpy(bufPtr, &(encodingPtr[index]), itemLen);
        *bufSizePtr = itemLen;
    }

    if (bytesReadPtr != NULL)
    {
        *bytesReadPtr = numBytesRead;
    }

    if (numBytesRead != encodingSize)
    {
        return IKS_OUT_OF_RANGE;
    }

    return IKS_OK;
}


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
)
{
    // Check params.
    if ( (bufPtr == NULL) || (bufSizePtr == NULL) )
    {
        return IKS_INVALID_PARAM;
    }

    // Traverse the list to build the buffer.
    size_t bufIndex = 0;

    size_t i = 0;
    for (; i < numItems; i++)
    {
        if (items[i].valPtr == NULL)
        {
            continue;
        }

        // Encode the item.
        size_t l = *bufSizePtr - bufIndex;

        iks_result_t result = der_EncodeVal(items[i].type, items[i].valPtr, items[i].valSize,
                                            bufPtr + bufIndex, &l);

        if (result != IKS_OK)
        {
            return result;
        }

        bufIndex += l;
    }

    *bufSizePtr = bufIndex;
    return IKS_OK;
}


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
)
{
    // Check params.
    if (encodingPtr == NULL)
    {
        return IKS_INVALID_PARAM;
    }

    // Process each specified item at a time.
    iks_result_t result = IKS_OUT_OF_RANGE;
    size_t bufIndex = 0;

    size_t i = 0;
    for (; i < numItems; i++)
    {
        if (items[i].valPtr == NULL)
        {
            continue;
        }

        // Check for the end of the input buffer.
        if (encodingSize <= bufIndex)
        {
            ERR_PRINT("End of buffer.");
            return IKS_FORMAT_ERROR;
        }

        // Decode the item.
        size_t encodingBytesRead = 0;

        result = der_DecodeVal(items[i].type, encodingPtr + bufIndex, encodingSize - bufIndex,
                               &encodingBytesRead,  items[i].valPtr, items[i].valSizePtr);

        if ( (result != IKS_OK) && (result != IKS_OUT_OF_RANGE) )
        {
            return result;
        }

        bufIndex += encodingBytesRead;
    }

    if (bytesReadPtr != NULL)
    {
        *bytesReadPtr = bufIndex;
    }

    return result;
}


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
)
{
    // Check params.
    if ( (bufPtr == NULL) || (bufSizePtr == NULL) )
    {
        return IKS_INVALID_PARAM;
    }

    // Allocate space for the sequence header.
    size_t headerSize = 2;

    if (*bufSizePtr < headerSize)
    {
        return IKS_OVERFLOW;
    }

    // Traverse the list to build the buffer.
    size_t encodingSize = *bufSizePtr - headerSize;

    iks_result_t result = der_EncodeList(items, numItems, bufPtr + headerSize, &encodingSize);

    if (result != IKS_OK)
    {
        return result;
    }

    // See how many bytes we need for the length field.
    uint8_t lenSize = CalcLenSize(encodingSize);

    if (lenSize > 1)
    {
        // We need to shift the contents of the buffer to fit the length field.
        size_t origHeaderSize = headerSize;
        headerSize = lenSize + 1;

        if (*bufSizePtr < encodingSize + headerSize)
        {
            return IKS_OVERFLOW;
        }

        memmove(bufPtr + headerSize, bufPtr + origHeaderSize, encodingSize);
    }

    // Write the sequence header.
    bufPtr[0] = ASN1_SEQUENCE;
    WriteLength(encodingSize, bufPtr + 1);

    *bufSizePtr = encodingSize + headerSize;
    return IKS_OK;
}


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
)
{
    // Check params.
    if (encodingPtr == NULL)
    {
        return IKS_INVALID_PARAM;
    }

    // Check that the initial header is a sequence.
    size_t bufIndex = 0;

    if (encodingSize < 2)
    {
        ERR_PRINT("Header sequence is missing.");
        return IKS_FORMAT_ERROR;
    }

    if (encodingPtr[bufIndex++] != ASN1_SEQUENCE)
    {
        ERR_PRINT("Buffer is not an ASN.1 sequence.");
        return IKS_FORMAT_ERROR;
    }

    uint8_t numLenBytes = GetNumLenBytes(encodingPtr[bufIndex++]);
    bufIndex += numLenBytes;

    if (encodingSize <= bufIndex)
    {
        ERR_PRINT("End of buffer.");
        return IKS_FORMAT_ERROR;
    }

    // Process each specified item at a time.
    return der_DecodeList(encodingPtr + bufIndex, encodingSize - bufIndex,
                          bytesReadPtr,
                          items, numItems);
}