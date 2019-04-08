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
 * Convert a DER key to PEM key
 *
 */
//--------------------------------------------------------------------------------------------------
static int ConvertDERToPEM
(
    const unsigned char*  derKeyPtr, ///< [IN] DER key
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
