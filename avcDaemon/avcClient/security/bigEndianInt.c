/**
 * @file bigEndianInt.c
 *
 * Converts integers to and from big endian format.
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

#include "iks_basic.h"
#include "err.h"
#include "bigEndianInt.h"


//--------------------------------------------------------------------------------------------------
/**
 * Endianess indicator of the platform.
 */
//--------------------------------------------------------------------------------------------------
static bool IsNativeBigEndian = false;


//--------------------------------------------------------------------------------------------------
/**
 * Should be called before other functions in this API.
 */
//--------------------------------------------------------------------------------------------------
void bei_Init
(
    void
)
{
    int x = 1;
    uint8_t* ptr = (uint8_t*)(&x);

    IsNativeBigEndian = (ptr[0] != 1);
}


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
)
{
    return IsNativeBigEndian;
}


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
)
{
    if ( (inPtr == NULL) || (outPtr == NULL) )
    {
        return IKS_INVALID_PARAM;
    }

    if (outSize < inSize)
    {
        return IKS_OVERFLOW;
    }

    memset(outPtr, 0, outSize);

    if (!IsNativeBigEndian)
    {
        size_t i = 0;
        for (; i < inSize; i++)
        {
            outPtr[i] = inPtr[inSize - 1 - i];
        }
    }
    else
    {
        memcpy(outPtr + outSize - inSize, inPtr, inSize);
    }

    return IKS_OK;
}