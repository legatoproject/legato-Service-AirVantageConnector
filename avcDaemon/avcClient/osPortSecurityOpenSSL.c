/**
 * @file osPortSecurityOpenSSL.c
 *
 * Porting layer for package security (CRC, signature)
 *
 * @note The CRC is computed using the crc32 function from zlib.
 * @note The signature verification uses the OpenSSL library.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <zlib.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/hmac.h>
#include <lwm2mcore/security.h>
#include <legato.h>
#include <interfaces.h>
#include "avcClient.h"
#include "avcFs/avcFsConfig.h"
#include "avcFs/avcFs.h"
#include "packageDownloader/sslUtilities.h"

//--------------------------------------------------------------------------------------------------
/**
 * Package verification
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Compute and update CRC32 with data buffer passed as an argument
 *
 * @return Updated CRC32
 */
//--------------------------------------------------------------------------------------------------
uint32_t lwm2mcore_Crc32
(
    uint32_t crc,       ///< [IN] Current CRC32 value
    uint8_t* bufPtr,    ///< [IN] Data buffer to hash
    size_t   len        ///< [IN] Data buffer length
)
{
    return crc32(crc, bufPtr, len);
}

//--------------------------------------------------------------------------------------------------
/**
 * Calculate the length of the data decoded from the Base64 format.
 *
 * @return decoded data length. 0 represents error.
 */
//--------------------------------------------------------------------------------------------------
static size_t CalculateBase64DecodeLength
(
    const char* b64input    ///< [IN] Base64-encoded data
)
{
    size_t len = strlen(b64input);
    size_t padding = 0;

    if (len < 4)
    {
        // minimum length of the encoded string is 4 bytes
        return 0;
    }
    if (b64input[len - 1] == '=' && b64input[len - 2] == '=') // Last two chars are =
    {
        padding = 2;
    }
    else if (b64input[len - 1] == '=') // Last char is =
    {
        padding = 1;
    }

    return (len * 3) / 4 - padding;
}

//--------------------------------------------------------------------------------------------------
/**
 * Perform base64 data encoding.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 *      - LWM2MCORE_ERR_OVERFLOW if buffer overflow occurs
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_Base64Encode
(
    const uint8_t*  src,    ///< [IN] Data to be encoded
    size_t          srcLen, ///< [IN] Data length
    char*           dst,    ///< [OUT] Base64-encoded string buffer
    size_t*         dstLen  ///< [INOUT] Length of the base64-encoded string buffer
)
{
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    int res;
    lwm2mcore_Sid_t rc = LWM2MCORE_ERR_COMPLETED_OK;

    if (!dstLen || !src || !dst || (0 == srcLen))
    {
        LE_ERROR("Null pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines - write everything in one line
    BIO_write(bio, src, srcLen);
    res = BIO_flush(bio);
    LE_WARN_IF(res != 1, "BIO_flush failed");

    BIO_get_mem_ptr(bio, &bufferPtr);
    res = BIO_set_close(bio, BIO_NOCLOSE);
    LE_WARN_IF(res != 1, "BIO_set_close failed");

    // Check if the output buffer can hold encoded string + '\0'
    if (*dstLen < strlen((*bufferPtr).data) + 1)
    {
        LE_ERROR("Insufficient buffer size: %zu", *dstLen);
        rc = LWM2MCORE_ERR_OVERFLOW;
    }
    else
    {
        memcpy(dst, (*bufferPtr).data, *dstLen);
    }

    BIO_free_all(bio);

    return rc;
}

//--------------------------------------------------------------------------------------------------
/**
 * Decode base64-encoded data.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 *      - LWM2MCORE_ERR_OVERFLOW if buffer overflow occurs
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if incorrect data range
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_Base64Decode
(
    char*       src,    ///< [IN] Base64-encoded data string
    uint8_t*    dst,    ///< [OUT] Decoded data buffer
    size_t*     dstLen  ///< [INOUT] Decoded data buffer length
)
{
    BIO *bio, *b64;
    size_t decodeLen = 0;

    if (!dstLen || !src || !dst)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
        LE_ERROR("Null pointer provided");
    }

    decodeLen = CalculateBase64DecodeLength(src);
    if (0 == decodeLen)
    {
        LE_ERROR("Cannot calculate decoded data length");
        return LWM2MCORE_ERR_INCORRECT_RANGE;
    }
    if (*dstLen < decodeLen)
    {
        LE_ERROR("Insufficient buffer size: %zu < %zu", *dstLen, decodeLen);
        return LWM2MCORE_ERR_OVERFLOW;
    }

    bio = BIO_new_mem_buf(src, -1);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer
    *dstLen = BIO_read(bio, dst, strlen(src));

    BIO_free_all(bio);

    // Check whether pre-calculated decoded length match the actual
    if (*dstLen != decodeLen)
    {
        LE_ERROR("Decoded length is different from expected: %zu %zu", *dstLen, decodeLen);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Compute HMAC SHA256 digest using the given data and credential.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_ComputeHmacSHA256
(
    uint8_t*                data,           ///< [IN] Data buffer
    size_t                  dataLen,        ///< [IN] Data length
    lwm2mcore_Credentials_t credId,         ///< [IN] Key type
    uint8_t*                result,         ///< [OUT] Digest buffer
    size_t*                 resultLenPtr    ///< [INOUT] Digest buffer length
)
{
    char key[LWM2MCORE_PSK_LEN];
    size_t keyLen = sizeof(key);
    unsigned int resultLen;
    lwm2mcore_Sid_t rc = LWM2MCORE_ERR_COMPLETED_OK;

    // Check the inputs
    if (!data || !result || !resultLenPtr)
    {
        LE_ERROR("Null pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }
    resultLen = *resultLenPtr;
    if ((0 == dataLen) || (0 == resultLen))
    {
        LE_ERROR("Buffer length is zero");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Retrieve the encryption key
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_GetCredential(credId,
                                                              LWM2MCORE_NO_SERVER_ID,
                                                              key,
                                                              &keyLen))
    {
        LE_ERROR("Error while retrieving credentials %d", credId);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Calculate the digest
    if (NULL == HMAC(EVP_sha256(), key, keyLen, data, dataLen, result, &resultLen))
    {
        LE_ERROR("HMAC() returned NULL");
        rc = LWM2MCORE_ERR_GENERAL_ERROR;
    }
    else
    {
        *resultLenPtr = resultLen;
        rc = LWM2MCORE_ERR_COMPLETED_OK;
    }
    // erase the key.
    memset(key, 0, sizeof(key));

    return rc;
}

//--------------------------------------------------------------------------------------------------
/**
 * Print OpenSSL errors
 */
//--------------------------------------------------------------------------------------------------
static void PrintOpenSSLErrors
(
    void
)
{
    char errorString[LWM2MCORE_ERROR_STR_MAX_LEN];
    unsigned long error;

    // Retrieve the first error and remove it from the queue
    error = ERR_get_error();
    while (0 != error)
    {
        // Convert the error code to a human-readable string and print it
        ERR_error_string_n(error, errorString, sizeof(errorString));
        LE_ERROR("%s", errorString);

        // Retrieve the next error and remove it from the queue
        error = ERR_get_error();
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the SHA1 computation
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_StartSha1
(
    void** sha1CtxPtr   ///< [INOUT] SHA1 context pointer
)
{
    static SHA_CTX shaCtx;

    // Check if SHA1 context pointer is set
    if (!sha1CtxPtr)
    {
        LE_ERROR("No SHA1 context pointer");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Load the error strings
    ERR_load_crypto_strings();

    // Initialize the SHA1 context
    // SHA1_Init function returns 1 for success, 0 otherwise
    if (1 != SHA1_Init(&shaCtx))
    {
        LE_ERROR("SHA1_Init failed");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
    else
    {
        *sha1CtxPtr = (void*)&shaCtx;
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Compute and update SHA1 digest with the data buffer passed as an argument
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_ProcessSha1
(
    void*    sha1CtxPtr,    ///< [IN] SHA1 context pointer
    uint8_t* bufPtr,        ///< [IN] Data buffer to hash
    size_t   len            ///< [IN] Data buffer length
)
{
    // Check if pointers are set
    if ((!sha1CtxPtr) || (!bufPtr))
    {
        LE_ERROR("NULL pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Update SHA1 digest
    // SHA1_Update function returns 1 for success, 0 otherwise
    if (1 != SHA1_Update((SHA_CTX*)sha1CtxPtr, bufPtr, len))
    {
        LE_ERROR("SHA1_Update failed");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
    else
    {
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Finalize SHA1 digest and verify the package signature
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_EndSha1
(
    void* sha1CtxPtr,                   ///< [IN] SHA1 context pointer
    lwm2mcore_UpdateType_t packageType, ///< [IN] Package type (FW or SW)
    uint8_t* signaturePtr,              ///< [IN] Package signature used for verification
    size_t signatureLen                 ///< [IN] Package signature length
)
{
    unsigned char sha1Digest[SHA_DIGEST_LENGTH];
    lwm2mcore_Credentials_t credId;
    char publicKey[LWM2MCORE_PUBLICKEY_LEN];
    size_t publicKeyLen = LWM2MCORE_PUBLICKEY_LEN;
    BIO* bufioPtr = NULL;
    RSA* rsaKeyPtr = NULL;
    EVP_PKEY* evpPkeyPtr = NULL;
    EVP_PKEY_CTX* evpPkeyCtxPtr = NULL;

    // Check if pointers are set
    if ((!sha1CtxPtr) || (!signaturePtr))
    {
        LE_ERROR("NULL pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Finalize SHA1 digest
    // SHA1_Final function returns 1 for success, 0 otherwise
    if (1 != SHA1_Final(sha1Digest, (SHA_CTX*)sha1CtxPtr))
    {
        LE_ERROR("SHA1_Final failed");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // The package type indicates the public key to use
    switch (packageType)
    {
        case LWM2MCORE_FW_UPDATE_TYPE:
            credId = LWM2MCORE_CREDENTIAL_FW_KEY;
            break;

        case LWM2MCORE_SW_UPDATE_TYPE:
            credId = LWM2MCORE_CREDENTIAL_SW_KEY;
            break;

        default:
            LE_ERROR("Unknown or unsupported package type %d", packageType);
            return LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    // Retrieve the public key corresponding to the package type
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_GetCredential(credId,
                                                              LWM2MCORE_NO_SERVER_ID,
                                                              publicKey,
                                                              &publicKeyLen))
    {
        LE_ERROR("Error while retrieving credentials %d", credId);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // The public key is stored in PKCS #1 DER format, convert it to a RSA key.
    // Note that two formats are possible, try both of them if necessary:
    // - PEM DER ASN.1 PKCS#1 RSA Public key: ASN.1 type RSAPublicKey
    // - X.509 SubjectPublicKeyInfo: Object Identifier rsaEncryption added for AlgorithmIdentifier

    // First create the memory BIO containing the DER key
    bufioPtr = BIO_new_mem_buf((void*)publicKey, publicKeyLen);
    if (!bufioPtr)
    {
        LE_ERROR("Unable to create a memory BIO");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
    // Then convert it to a RSA key using PEM DER ASN.1 PKCS#1 RSA Public key format
    rsaKeyPtr = d2i_RSAPublicKey_bio(bufioPtr, NULL);
    if (!rsaKeyPtr)
    {
        // Memory BIO is modified by last function call, retrieve the DER key again
        BIO_free(bufioPtr);
        bufioPtr = BIO_new_mem_buf((void*)publicKey, publicKeyLen);
        if (!bufioPtr)
        {
            LE_ERROR("Unable to create a memory BIO");
            PrintOpenSSLErrors();
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }

        // Then convert it to a RSA key using X.509 SubjectPublicKeyInfo format
        rsaKeyPtr = d2i_RSA_PUBKEY_bio(bufioPtr, NULL);
    }
    BIO_free(bufioPtr);
    if (!rsaKeyPtr)
    {
        LE_ERROR("Unable to retrieve public key");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
    evpPkeyPtr = EVP_PKEY_new();
    if (!evpPkeyPtr)
    {
        LE_ERROR("Unable to create EVP_PKEY structure");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
    // EVP_PKEY_assign_RSA returns 1 for success and 0 for failure
    if (1 != EVP_PKEY_assign_RSA(evpPkeyPtr, rsaKeyPtr))
    {
        LE_ERROR("Unable to assign public key");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Create EVP public key context, necessary to verify the signature
    evpPkeyCtxPtr = EVP_PKEY_CTX_new(evpPkeyPtr, NULL);
    if (   (!evpPkeyCtxPtr)
        || (1 != EVP_PKEY_verify_init(evpPkeyCtxPtr))
       )
    {
        LE_ERROR("Unable to create and initialize EVP PKEY context");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Set the signature verification options:
    // - RSA padding mode is PSS
    // - message digest type is SHA1
    // EVP_PKEY_CTX_ctrl functions return a positive value for success
    // and 0 or a negative value for failure
    if (   (EVP_PKEY_CTX_set_rsa_padding(evpPkeyCtxPtr, RSA_PKCS1_PSS_PADDING) <= 0)
        || (EVP_PKEY_CTX_set_signature_md(evpPkeyCtxPtr, EVP_sha1()) <= 0)
       )
    {
        LE_ERROR("Error during EVP PKEY context initialization");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Verify signature
    // VP_PKEY_verify returns 1 if the verification was successful and 0 if it failed
    if (1 != EVP_PKEY_verify(evpPkeyCtxPtr,
                             signaturePtr,
                             signatureLen,
                             sha1Digest,
                             sizeof(sha1Digest)))
    {
        LE_ERROR("Signature verification failed");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Copy the SHA1 context in a buffer
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_CopySha1
(
    void*  sha1CtxPtr,  ///< [IN] SHA1 context pointer
    void*  bufPtr,      ///< [INOUT] Buffer
    size_t bufSize      ///< [INOUT] Buffer length
)
{
    // Check if pointers are set
    if ((!sha1CtxPtr) || (!bufPtr))
    {
        LE_ERROR("Null pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Check buffer length
    if (bufSize < sizeof(SHA_CTX))
    {
        LE_ERROR("Buffer is too short (%zu < %zd)", bufSize, sizeof(SHA_CTX));
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Copy the SHA1 context
    memset(bufPtr, 0, bufSize);
    memcpy(bufPtr, sha1CtxPtr, sizeof(SHA_CTX));
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Restore the SHA1 context from a buffer
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_RestoreSha1
(
    void*  bufPtr,      ///< [IN] Buffer
    size_t bufSize,     ///< [IN] Buffer length
    void** sha1CtxPtr   ///< [INOUT] SHA1 context pointer
)
{
    // Check if pointers are set
    if ((!sha1CtxPtr) || (!bufPtr))
    {
        LE_ERROR("Null pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Check buffer length
    if (bufSize < sizeof(SHA_CTX))
    {
        LE_ERROR("Buffer is too short (%zu < %zd)", bufSize, sizeof(SHA_CTX));
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Initialize SHA1 context
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_StartSha1(sha1CtxPtr))
    {
        LE_ERROR("Unable to initialize SHA1 context");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Restore the SHA1 context
    memcpy(*sha1CtxPtr, bufPtr, sizeof(SHA_CTX));
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Cancel and reset the SHA1 computation
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_CancelSha1
(
    void** sha1CtxPtr   ///< [INOUT] SHA1 context pointer
)
{
    // Check if SHA1 context pointer is set
    if (!sha1CtxPtr)
    {
        LE_ERROR("No SHA1 context pointer");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Reset SHA1 context
    *sha1CtxPtr = NULL;

    return LWM2MCORE_ERR_COMPLETED_OK;
}
