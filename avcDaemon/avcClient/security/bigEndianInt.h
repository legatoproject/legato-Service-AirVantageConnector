/**
 * @file bigEndianInt.h
 *
 * Converts integers to and from big endian format.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef IOT_KEY_STORE_BIG_ENDIAN_INT_INCLUDE_GUARD
#define IOT_KEY_STORE_BIG_ENDIAN_INT_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Should be called before other functions in this API.
 */
//--------------------------------------------------------------------------------------------------
void bei_Init
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Check if the native format is big-endian.
 *
 * @return
 *      true if the native format is big-endian.
 *      false otherwise.
 */
//--------------------------------------------------------------------------------------------------
bool bei_IsNativeBigEndian
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Converts an unsigned integer to/from big-endian format from/to native format.
 *
 * @return
 *      IKS_OK if successful.
 *      IKS_INVALID_PARAM if either inPtr or outPtr is NULL.
 *      IKS_OVERFLOW if the out buffer is too small.
 */
//--------------------------------------------------------------------------------------------------
size_t bei_ConvertUnsigned
(
    const uint8_t*  inPtr,          ///< [IN] In array.
    size_t          inSize,         ///< [IN] In array size.
    uint8_t*        outPtr,         ///< [OUT] Out array.
    size_t          outSize         ///< [IN] Out array size.
);


#endif // IOT_KEY_STORE_BIG_ENDIAN_INT_INCLUDE_GUARD