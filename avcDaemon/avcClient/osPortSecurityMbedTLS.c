/**
 * @file osPortSecurity.c
 *
 * Porting layer for package security (CRC, signature)
 *
 * @note The CRC is computed using a crc32 function copied from zlib.
 * @note The signature verification uses the mbedTLS library.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <mbedtls/sha1.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>
#include <mbedtls/sha256.h>
#include <lwm2mcore/security.h>
#include <legato.h>
#include <interfaces.h>
#include "avcClient.h"
#include "avcFs/avcFsConfig.h"
#include "avcFs/avcFs.h"
#include "packageDownloader/sslUtilities.h"


//--------------------------------------------------------------------------------------------------
/**
 * SHA1 digest length
 */
//--------------------------------------------------------------------------------------------------
#define SHA_DIGEST_LENGTH          64

//--------------------------------------------------------------------------------------------------
/**
 * SHA256 digest length in bytes
 */
//--------------------------------------------------------------------------------------------------
#define SHA256_DIGEST_LENGTH        32

//--------------------------------------------------------------------------------------------------
/**
 * Public key prolog for RSA key (needed for mbedTLS to parse PKCS#1 keys)
 */
//--------------------------------------------------------------------------------------------------
static const uint8_t RSAKeyPrefix[] =
{
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
    0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0d, 0x00
};


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
    crc = ~crc;

    return ~le_crc_Crc32(bufPtr, len, crc);
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
    static mbedtls_sha1_context shaCtx;

    // Check if SHA1 context pointer is set
    if (!sha1CtxPtr)
    {
        LE_ERROR("No SHA1 context pointer");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Initialize the SHA1 context
    mbedtls_sha1_init(&shaCtx);

    // And start the SHA1 computation
    if (mbedtls_sha1_starts_ret(&shaCtx) != 0)
    {
        LE_ERROR("Start SHA1 computation");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    *sha1CtxPtr = (void*)&shaCtx;
    return LWM2MCORE_ERR_COMPLETED_OK;
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
    if (mbedtls_sha1_update_ret(sha1CtxPtr, bufPtr, len) != 0)
    {
        LE_ERROR("Update SHA1 digest.");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
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
    unsigned char publicKey[LWM2MCORE_PUBLICKEY_LEN];
    unsigned char *publicKeyPtr = publicKey;
    size_t publicKeyLen = LWM2MCORE_PUBLICKEY_LEN;
    const size_t publicKeyPrefixLen = sizeof(RSAKeyPrefix) + 4; // save 4 bytes for tag
    uint16_t publicKeyTaggedLen;
    mbedtls_pk_context publicKeyCtx;
    int errorCode;
    lwm2mcore_Sid_t result = LWM2MCORE_ERR_GENERAL_ERROR;
    static const mbedtls_pk_rsassa_pss_options sigOptions =
        { .mgf1_hash_id = MBEDTLS_MD_SHA1,
          .expected_salt_len = MBEDTLS_RSA_SALT_LEN_ANY };

    // Check if pointers are set
    if ((!sha1CtxPtr) || (!signaturePtr))
    {
        LE_ERROR("NULL pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (mbedtls_sha1_finish_ret(sha1CtxPtr, sha1Digest) != 0)
    {
        LE_ERROR("Finish SHA1 operation");
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
    }

    publicKeyPtr = publicKey + publicKeyPrefixLen;
    publicKeyLen = sizeof(publicKey) - publicKeyPrefixLen;

    // Retrieve the public key corresponding to the package type
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_GetCredential(credId,
                                                              LWM2MCORE_NO_SERVER_ID,
                                                              (char*)publicKeyPtr,
                                                              &publicKeyLen))
    {
        LE_ERROR("Error while retrieving credentials %d", credId);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Now add RSA prefix before key to treat it as PKCS#1 key.
    publicKey[0] = 0x30;
    publicKey[1] = 0x82;
    LE_ASSERT(LWM2MCORE_PUBLICKEY_LEN <= UINT16_MAX);
    publicKeyTaggedLen = htons((uint16_t)publicKeyLen + sizeof(RSAKeyPrefix));
    memcpy(&publicKey[2], &publicKeyTaggedLen, sizeof(uint16_t));
    memcpy(&publicKey[4], RSAKeyPrefix, sizeof(RSAKeyPrefix));

    publicKeyPtr = publicKey;
    publicKeyLen += publicKeyPrefixLen;

    // The public key is stored in PKCS #1 DER format, convert it to a RSA key.  The following
    // two formats are supported (tried in this order);
    // - PEM DER ASN.1 PKCS#1 RSA Public key: ASN.1 type RSAPublicKey
    // - X.509 SubjectPublicKeyInfo: Object Identifier rsaEncryption added for AlgorithmIdentifier
    mbedtls_pk_init(&publicKeyCtx);

    errorCode = mbedtls_pk_parse_subpubkey(&publicKeyPtr,
                                           (const unsigned char *)publicKeyPtr + publicKeyLen,
                                           &publicKeyCtx);

    if (errorCode != 0)
    {
        // Maybe was already in subject public key info format -- retry
        publicKeyPtr = publicKey + publicKeyPrefixLen;
        publicKeyLen -= publicKeyPrefixLen;

        errorCode = mbedtls_pk_parse_subpubkey(&publicKeyPtr,
                                               (const unsigned char *)publicKeyPtr + publicKeyLen,
                                               &publicKeyCtx);

        if (errorCode != 0)
        {
            char errorStr[256];
            mbedtls_strerror(errorCode, errorStr, sizeof(errorStr));
            LE_ERROR("Unable to retrieve public key (-%x): %256s", errorCode, errorStr);

            result = LWM2MCORE_ERR_GENERAL_ERROR;
            goto done;
        }
    }

    if (!mbedtls_pk_can_do(&publicKeyCtx, MBEDTLS_PK_RSA))
    {
        LE_ERROR("Key is not an RSA key");

        result = LWM2MCORE_ERR_GENERAL_ERROR;

        goto done;
    }

    // Verify signature
    // - RSA padding mode is PSS
    // - message digest type is SHA1
    errorCode = mbedtls_pk_verify_ext(MBEDTLS_PK_RSASSA_PSS, &sigOptions,
                                      &publicKeyCtx, MBEDTLS_MD_SHA1,
                                      sha1Digest, sizeof(sha1Digest),
                                      signaturePtr, signatureLen);

    if (errorCode != 0)
    {
        char errorStr[256];
        mbedtls_strerror(errorCode, errorStr, sizeof(errorStr));
        LE_ERROR("Signature verification failed (-%x): %256s", -errorCode, errorStr);
        result = LWM2MCORE_ERR_GENERAL_ERROR;

        goto done;
    }

    result = LWM2MCORE_ERR_COMPLETED_OK;

done:
    // Free key
    mbedtls_pk_free(&publicKeyCtx);

    return result;
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
    if (bufSize < sizeof(mbedtls_sha1_context))
    {
        LE_ERROR("Buffer is too short (%zu < %lu)", bufSize, sizeof(mbedtls_sha1_context));
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Copy the SHA1 context
    memset(bufPtr, 0, bufSize);
    memcpy(bufPtr, sha1CtxPtr, sizeof(mbedtls_sha1_context));
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
    if (bufSize < sizeof(mbedtls_sha1_context))
    {
        LE_ERROR("Buffer is too short (%zu < %lu)", bufSize, sizeof(mbedtls_sha1_context));
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Initialize SHA1 context
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_StartSha1(sha1CtxPtr))
    {
        LE_ERROR("Unable to initialize SHA1 context");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Restore the SHA1 context
    memcpy(*sha1CtxPtr, bufPtr, sizeof(mbedtls_sha1_context));
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

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the SHA256 computation
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_StartSha256
(
    void** sha256CtxPtr   ///< [INOUT] SHA256 context pointer
)
{
    static mbedtls_sha256_context shaCtx;
    int res = 0;

    mbedtls_sha256_init(&shaCtx);
    res = mbedtls_sha256_starts_ret(&shaCtx, 0);
    if (0 != res)
    {
        LE_ERROR("mbedtls_sha256_starts_ret failed with code: %d", res);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    *sha256CtxPtr = (void*)&shaCtx;
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Compute and update SHA256 digest with the data buffer passed as an argument
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_ProcessSha256
(
    void*    sha256CtxPtr,  ///< [IN] SHA256 context pointer
    uint8_t* bufPtr,        ///< [IN] Data buffer to hash
    size_t   len            ///< [IN] Data buffer length
)
{
    int res = 0;

    // Check if pointers are set
    if ((!sha256CtxPtr) || (!bufPtr))
    {
        LE_ERROR("NULL pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Update SHA256 digest
    res = mbedtls_sha256_update_ret((mbedtls_sha256_context*)sha256CtxPtr, bufPtr, len);
    if (0 != res)
    {
        LE_ERROR("mbedtls_sha256_update_ret failed with code: %d", res);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * @brief Finalize SHA256 digest and verify the checksum.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_EndAndCheckSha256
(
    void* sha256CtxPtr,                 ///< [IN] SHA256 context pointer
    char* sha256DigestToCompare         ///< [IN] SHA256 digest to compare
)
{
    int res = 0;
    int i = 0;
    unsigned char sha256Digest[SHA256_DIGEST_LENGTH];
    char outputBuffer[2 * SHA256_DIGEST_LENGTH + 1];

    // Check if pointers are set
    if ((!sha256CtxPtr) || (!sha256DigestToCompare))
    {
        LE_ERROR("NULL pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    res = mbedtls_sha256_finish_ret((mbedtls_sha256_context*)sha256CtxPtr, sha256Digest);
    if (0 != res)
    {
        LE_ERROR("mbedtls_sha256_finish_ret failed with code: %d", res);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    mbedtls_sha256_free((mbedtls_sha256_context*)sha256CtxPtr);

    for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", sha256Digest[i]);
    }
    outputBuffer[2 * SHA256_DIGEST_LENGTH] = 0;

    if (strncmp(outputBuffer, sha256DigestToCompare, 2 * SHA256_DIGEST_LENGTH))
    {
        LE_ERROR("SHA256 check error, \n device side:\t%s\nserver side:\t%s",
                 outputBuffer, sha256DigestToCompare);
        return LWM2MCORE_ERR_SHA_DIGEST_MISMATCH;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Copy the SHA256 context in a buffer
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_CopySha256
(
    void*  sha256CtxPtr,    ///< [IN] SHA256 context pointer
    void*  bufPtr,          ///< [INOUT] Buffer
    size_t bufSize          ///< [IN] Buffer length
)
{
    // Check if pointers are set
    if ((!sha256CtxPtr) || (!bufPtr))
    {
        LE_ERROR("Null pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Check buffer length
    if (bufSize < sizeof(mbedtls_sha256_context))
    {
        LE_ERROR("Buffer is too short (%zu < %zd)", bufSize, sizeof(mbedtls_sha256_context));
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Copy the SHA256 context
    memset(bufPtr, 0, bufSize);
    memcpy(bufPtr, sha256CtxPtr, sizeof(mbedtls_sha256_context));
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Restore the SHA256 context from a buffer
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_RestoreSha256
(
    void*  bufPtr,      ///< [IN] Buffer
    size_t bufSize,     ///< [IN] Buffer length
    void** sha256CtxPtr ///< [INOUT] SHA256 context pointer
)
{
    // Check if pointers are set
    if ((!sha256CtxPtr) || (!bufPtr))
    {
        LE_ERROR("Null pointer provided");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Check buffer length
    if (bufSize < sizeof(mbedtls_sha256_context))
    {
        LE_ERROR("Buffer is too short (%zu < %zd)", bufSize, sizeof(mbedtls_sha256_context));
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Initialize SHA256 context
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mcore_StartSha256(sha256CtxPtr))
    {
        LE_ERROR("Unable to initialize SHA256 context");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Restore the SHA256 context
    memcpy(*sha256CtxPtr, bufPtr, sizeof(mbedtls_sha256_context));
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Cancel and reset the SHA256 computation
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_CancelSha256
(
    void** sha256CtxPtr   ///< [INOUT] SHA256 context pointer
)
{
    // Check if SHA256 context pointer is set
    if (!sha256CtxPtr)
    {
        LE_ERROR("No SHA256 context pointer");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Reset SHA256 context
    *sha256CtxPtr = NULL;

    return LWM2MCORE_ERR_COMPLETED_OK;
}
