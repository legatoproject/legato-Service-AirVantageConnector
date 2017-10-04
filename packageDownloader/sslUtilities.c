/**
 * @file sslUtilities.c
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <legato.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <avcFsConfig.h>
#include "sslUtilities.h"
#include "defaultDerKey.h"
#include "avcFs.h"

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
    char*   strPtr,
    size_t  strLen,
    int     pos,
    char    val
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
    char*   strPtr,
    size_t  strLen
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
 * Convert a DER key to PEM key
 *
 */
//--------------------------------------------------------------------------------------------------
static int ConvertDERToPEM
(
    unsigned char*  derKeyPtr,      ///< [IN] DER key
    int             derKeyLen,      ///< [IN] DER key length
    unsigned char*  pemKeyPtr,      ///< [OUT] PEM key
    int             pemKeyLen       ///< [IN] PEM key length
)
{
    X509 *certPtr;
    BIO *memPtr;
    int count;

    if ( (!derKeyPtr) || (!pemKeyPtr) )
    {
        LE_ERROR("invalid input arguments: derKeyPtr (%p), pemKeyPtr(%p)",
            derKeyPtr, pemKeyPtr);
        return -1;
    }

    if (!derKeyLen)
    {
        LE_ERROR("derKeyLen cannot be 0");
        return -1;
    }

    certPtr = d2i_X509(NULL, (const unsigned char **)&derKeyPtr, derKeyLen);
    if (!certPtr)
    {
        LE_ERROR("unable to parse certificate: %lu", ERR_get_error());
        return -1;
    }

    memPtr = BIO_new(BIO_s_mem());
    if (!memPtr)
    {
        LE_ERROR("failed to set BIO type: %lu", ERR_get_error());
        goto x509_err;
    }

    if (!PEM_write_bio_X509(memPtr, certPtr))
    {
        LE_ERROR("failed to write certificate: %lu", ERR_get_error());
        goto bio_err;
    }

    if (BIO_number_written(memPtr) > pemKeyLen)
    {
        LE_ERROR("not enough space to hold the key");
        goto bio_err;
    }

    memset(pemKeyPtr, 0, BIO_number_written(memPtr) + 1);

    count = BIO_read(memPtr, pemKeyPtr, BIO_number_written(memPtr));
    if (count < BIO_number_written(memPtr))
    {
        LE_ERROR("failed to read certificate: count (%d): %d", count, (int)ERR_get_error());
        goto pem_err;
    }

    BIO_free(memPtr);
    X509_free(certPtr);

    return count;

pem_err:
    memset(pemKeyPtr, 0, BIO_number_written(memPtr) + 1);
bio_err:
    BIO_free(memPtr);
x509_err:
    X509_free(certPtr);
    return -1;
}

//--------------------------------------------------------------------------------------------------
/**
 * Load default certificate
 */
//--------------------------------------------------------------------------------------------------
static le_result_t LoadDefaultCertificate
(
    void
)
{
    unsigned char cert[MAX_CERT_LEN] = {0};
    int len;

    len = ConvertDERToPEM(DefaultDerKey, DEFAULT_DER_KEY_LEN, cert, MAX_CERT_LEN);
    if (-1 == len)
    {
        return LE_FAULT;
    }

    return WriteFs(SSLCERT_PATH, cert, len);
}

//--------------------------------------------------------------------------------------------------
/**
 * Write PEM key to default certificate file path
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WritePEMCertificate
(
    const char*     certPtr,
    unsigned char*  pemKeyPtr,
    int             pemKeyLen
)
{
    int fd;
    ssize_t count;
    mode_t mode = 0;

    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    fd = open(certPtr, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0)
    {
        LE_ERROR("failed to open %s: %m", certPtr);
        return LE_FAULT;
    }

    while (pemKeyLen)
    {
        count = write(fd, pemKeyPtr, pemKeyLen);
        if (count == -1)
        {
            LE_ERROR("failed to write PEM cert: %m");
            close(fd);
            return LE_FAULT;
        }
        if (0 <= count)
        {
            pemKeyLen -= count;
            pemKeyPtr += count;
        }

    }

    close(fd);

    return LE_OK;
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
    char*   strPtr,
    int     strLen
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

//--------------------------------------------------------------------------------------------------
/**
 * Check if SSL certificate exists and load it
 */
//--------------------------------------------------------------------------------------------------
le_result_t ssl_CheckCertificate
(
    void
)
{
    uint8_t buf[MAX_CERT_LEN] = {0};
    size_t size = MAX_CERT_LEN;
    le_result_t result;

    if (LE_OK != ExistsFs(SSLCERT_PATH))
    {
        LE_INFO("SSL certificate not found, loading default certificate");
        result = LoadDefaultCertificate();
        if (LE_OK != result)
        {
            return result;
        }
    }
    else
    {
        LE_INFO("Using saved SSL certificate");
    }

    result = ReadFs(SSLCERT_PATH, buf, &size);
    if (LE_OK != result)
    {
        return result;
    }

    return WritePEMCertificate(PEMCERT_PATH, buf, size);
}
