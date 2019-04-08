/**
 * @file sslLayOutPEM.c
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <legato.h>
#include "sslUtilities.h"

//--------------------------------------------------------------------------------------------------
/**
 * Base64 line break position
 */
//--------------------------------------------------------------------------------------------------
#define BASE64_NL           64

//--------------------------------------------------------------------------------------------------
/**
 * PEM certificate header
 */
//--------------------------------------------------------------------------------------------------
#define PEM_CERT_HEADER     "-----BEGIN CERTIFICATE-----"

//--------------------------------------------------------------------------------------------------
/**
 * PEM certificate footer
 */
//--------------------------------------------------------------------------------------------------
#define PEM_CERT_FOOTER     "-----END CERTIFICATE-----"

//--------------------------------------------------------------------------------------------------
/**
 * Insert a character val at position pos
 */
//--------------------------------------------------------------------------------------------------
static void InsertCharAtPosition
(
    char*   strPtr,      ///< [IN/OUT] string buffer
    size_t  strLen,      ///< [IN] string buffer size
    int     pos,         ///< [IN] position to insert character at
    char    val          ///< [IN] character to insert
)
{
    while (strLen>=pos)
    {
        *(strPtr+strLen+1) = *(strPtr+strLen);
        strLen--;
    }

    *(strPtr+pos) = val;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add a new line to string
 *
 * note:
 *    Due to the 64 bit encoding openssl expects a new line each 64 character
 */
//--------------------------------------------------------------------------------------------------
static void AddNewLine
(
    char*   strPtr,    ///< [IN/OUT] string buffer to break up into lines
    size_t  strLen     ///< [IN] string buffer size
)
{
    int j=1,n=0;

    while (j<=strLen)
    {
        if (!(j%BASE64_NL))
        {
            InsertCharAtPosition(strPtr, strLen+n, j+n, '\n');
            n++;
        }
        j++;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Lay out a base64 string into PEM
 *
 * @note:
 *     - Make sure that strPtr is big enough to hold the new string.
 *     - The buffer size must be >=
 *          ( strLen+(strLen/64)+strlen(PEM_CERT_HEADER)+strlen(PEM_CERT_FOOTER)+2 )
 */
//--------------------------------------------------------------------------------------------------
int ssl_LayOutPEM
(
    char*   strPtr,     ///< [IN/OUT] string buffer to format
    int     strLen      ///< [IN] string buffer size
)
{
    int size;
    char tmpStr[MAX_CERT_LEN] = {0};
    size_t finalSize;

    finalSize = strlen(strPtr)+(strlen(strPtr)/BASE64_NL)+strlen(PEM_CERT_HEADER)
        +strlen(PEM_CERT_FOOTER)+2;

    if (strLen < finalSize)
    {
        LE_ERROR("The buffer isn't big enough to hold the new string");
        return -1;
    }

    AddNewLine(strPtr, strlen(strPtr));

    size = snprintf(tmpStr, MAX_CERT_LEN, "%s\n", PEM_CERT_HEADER);
    size += snprintf(tmpStr+size, MAX_CERT_LEN-size, "%s", strPtr);
    size += snprintf(tmpStr+size, MAX_CERT_LEN-size, "\n%s\n", PEM_CERT_FOOTER);

    memcpy(strPtr, tmpStr, size+1);

    return size;
}
