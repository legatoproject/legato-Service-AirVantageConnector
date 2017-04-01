/**
 * @file osPortSecurity.c
 *
 * Porting layer for credential management and package security (CRC, signature)
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
#include "legato.h"
#include "interfaces.h"
#include "osPortSecurity.h"

//--------------------------------------------------------------------------------------------------
/**
 *                  OBJECT 0: SECURITY
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Prefix to retrieve files from secStoreGlobal service.
 */
//--------------------------------------------------------------------------------------------------
#define SECURE_STORAGE_PREFIX "/avms"

//--------------------------------------------------------------------------------------------------
/**
 * Array to describe the location of a specific credential type in the secure storage.
 */
//--------------------------------------------------------------------------------------------------
static const char* CredentialLocations[LWM2MCORE_CREDENTIAL_MAX] = {
    "LWM2M_FW_KEY",                     ///< LWM2MCORE_CREDENTIAL_FW_KEY
    "LWM2M_SW_KEY",                     ///< LWM2MCORE_CREDENTIAL_SW_KEY
    "certificate",                      ///< LWM2MCORE_CREDENTIAL_CERTIFICATE
    "LWM2M_BOOTSTRAP_SERVER_IDENTITY",  ///< LWM2MCORE_CREDENTIAL_BS_PUBLIC_KEY
    "bs_server_public_key",             ///< LWM2MCORE_CREDENTIAL_BS_SERVER_PUBLIC_KEY
    "LWM2M_BOOTSTRAP_SERVER_PSK",       ///< LWM2MCORE_CREDENTIAL_BS_SECRET_KEY
    "LWM2M_BOOTSTRAP_SERVER_ADDR",      ///< LWM2MCORE_CREDENTIAL_BS_ADDRESS
    "LWM2M_DM_PSK_IDENTITY",            ///< LWM2MCORE_CREDENTIAL_DM_PUBLIC_KEY
    "dm_server_public_key",             ///< LWM2MCORE_CREDENTIAL_DM_SERVER_PUBLIC_KEY
    "LWM2M_DM_PSK_SECRET",              ///< LWM2MCORE_CREDENTIAL_DM_SECRET_KEY
    "LWM2M_DM_SERVER_ADDR",             ///< LWM2MCORE_CREDENTIAL_DM_ADDRESS
};

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve a credential
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portSecurityGetCredential
(
    lwm2mcore_credentials_t credId,         ///< [IN] credential Id of credential to be retrieved
    char* bufferPtr,                        ///< [INOUT] data buffer
    size_t* lenPtr                          ///< [INOUT] length of input buffer and length of the
                                            ///< returned data
)
{
    if ((bufferPtr == NULL) || (lenPtr == NULL) || (credId >= LWM2MCORE_CREDENTIAL_MAX))
    {
        LE_ERROR("Bad parameter bufferPtr[%p] lenPtr[%p] credId[%u]", bufferPtr, lenPtr, credId);
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    char credsPathStr[SECSTOREGLOBAL_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credsPathStr,
                                        sizeof(credsPathStr),
                                        CredentialLocations[credId],
                                        NULL), "Buffer is not long enough");
    le_result_t result = secStoreGlobal_Read(credsPathStr, (uint8_t*)bufferPtr, lenPtr);
    if (LE_OK != result)
    {
        LE_ERROR("Unable to retrieve credentials for %d [%s]: %d %s",
                 credId, CredentialLocations[credId], result, LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("credId %d [%s], len %zu", credId, CredentialLocations[credId], *lenPtr);

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set a credential
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portSecuritySetCredential
(
    lwm2mcore_credentials_t credId,         ///< [IN] credential Id of credential to be set
    char* bufferPtr,                        ///< [INOUT] data buffer
    size_t len                              ///< [IN] length of input buffer and length of the
                                            ///< returned data
)
{
    if ((bufferPtr == NULL) || (credId >= LWM2MCORE_CREDENTIAL_MAX))
    {
        LE_ERROR("Bad parameter bufferPtr[%p] len[%d] credId[%u]", bufferPtr, len, credId);
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    char credsPathStr[SECSTOREGLOBAL_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credsPathStr,
                                        sizeof(credsPathStr),
                                        CredentialLocations[credId],
                                        NULL), "Buffer is not long enough");

    le_result_t result = secStoreGlobal_Write(credsPathStr, (uint8_t*)bufferPtr, len);
    if (LE_OK != result)
    {
        LE_ERROR("Unable to write credentials for %d [%s]", credId, CredentialLocations[credId]);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("credId %d [%s], len %zu", credId, CredentialLocations[credId], len);

    return LWM2MCORE_ERR_COMPLETED_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to check if one credential is present in platform storage.
 *
 * Since there is no GetSize in the le_secStore.api (that provides secStoreGlobal), tries to
 * retrieve the credentials with a buffer too small.
 *
 * @return
 *      - true if the credential is present
 *      - false else
 *
 */
//--------------------------------------------------------------------------------------------------
bool os_portSecurityCheckCredential
(
    lwm2mcore_credentials_t credId      ///< [IN] Credential identifier
)
{
    char buffer[SECSTOREGLOBAL_MAX_NAME_BYTES] = {0};
    size_t bufferSz = sizeof(buffer);
    bool ret = false;
    const char* retTxt = "Not Present";
    lwm2mcore_sid_t result;

    result = os_portSecurityGetCredential(credId, buffer, &bufferSz);
    if ( (LWM2MCORE_ERR_COMPLETED_OK == result) && (buffer[0] != 0))
    {
        ret = true;
        retTxt = "Present";
    }

    LE_DEBUG("credId %d result %s [%d]", credId, retTxt, ret);
    return ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function erases one credential from NV storage
 *
 * @return
 *      - true if the credential is deleted
 *      - false else
 */
//--------------------------------------------------------------------------------------------------
bool os_portSecurityDeleteCredential
(
    lwm2mcore_credentials_t credId      ///< [IN] Credential identifier
)
{
    if (credId >= LWM2MCORE_CREDENTIAL_MAX)
    {
        LE_ERROR("Bad parameter credId[%u]", credId);
        return LE_BAD_PARAMETER;
    }

    char credsPathStr[SECSTOREGLOBAL_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credsPathStr,
                                        sizeof(credsPathStr),
                                        CredentialLocations[credId],
                                        NULL), "Buffer is not long enough");

    le_result_t result = secStoreGlobal_Delete(credsPathStr);
    if ((LE_OK != result) && (LE_NOT_FOUND != result))
    {
        LE_ERROR("Unable to delete credentials for %d [%s]: %d %s",
                 credId, CredentialLocations[credId],
                 result, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    LE_DEBUG("credId %d [%s] deleted", credId, CredentialLocations[credId]);

    return LE_OK;
}

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
uint32_t os_portSecurityCrc32
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
 * Print OpenSSL errors
 */
//--------------------------------------------------------------------------------------------------
static void PrintOpenSSLErrors
(
    void
)
{
    char errorString[OS_PORT_ERROR_STR_MAX_LEN];
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
lwm2mcore_sid_t os_portSecuritySha1Start
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
lwm2mcore_sid_t os_portSecuritySha1Process
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
lwm2mcore_sid_t os_portSecuritySha1End
(
    void* sha1CtxPtr,                   ///< [IN] SHA1 context pointer
    lwm2mcore_PkgDwlType_t packageType, ///< [IN] Package type (FW or SW)
    uint8_t* signaturePtr,              ///< [IN] Package signature used for verification
    size_t signatureLen                 ///< [IN] Package signature length
)
{
    unsigned char sha1Digest[SHA_DIGEST_LENGTH];
    lwm2mcore_credentials_t credId;
    char publicKey[OS_PORT_PUBLICKEY_LEN];
    size_t publicKeyLen = OS_PORT_PUBLICKEY_LEN;
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
        case LWM2MCORE_PKG_FW:
            credId = LWM2MCORE_CREDENTIAL_FW_KEY;
            break;

        case LWM2MCORE_PKG_SW:
            credId = LWM2MCORE_CREDENTIAL_SW_KEY;
            break;

        default:
            LE_ERROR("Unknown or unsupported package type %d", packageType);
            return LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    // Retrieve the public key corresponding to the package type
    if (LWM2MCORE_ERR_COMPLETED_OK != os_portSecurityGetCredential(credId,
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
 * Cancel and reset the SHA1 computation
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portSecuritySha1Cancel
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
 * Convert a DER key to PEM key
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the conversion succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the conversion fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portSecurityConvertDERToPEM
(
    unsigned char *derKeyPtr,   ///< [IN]       DER key
    int derKeyLen,              ///< [IN]       DER key length
    unsigned char *pemKeyPtr,   ///< [OUT]      PEM key
    int *pemKeyLenPtr           ///< [IN/OUT]   PEM key length
)
{
    X509 *certPtr;
    BIO *memPtr;
    int count;

    if ( (!derKeyPtr) || (!pemKeyPtr) || (!pemKeyLenPtr) )
    {
        LE_ERROR("invalid input arguments: derKeyPtr (%p), pemKeyPtr(%p), "
            "pemKeyLenPtr(%p)", derKeyPtr, pemKeyPtr);
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (!derKeyLen)
    {
        LE_ERROR("derKeyLen cannot be 0");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    certPtr = d2i_X509(NULL, &derKeyPtr, derKeyLen);
    if (!certPtr)
    {
        LE_ERROR("unable to parse certificate");
        PrintOpenSSLErrors();
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    memPtr = BIO_new(BIO_s_mem());
    if (!memPtr)
    {
        LE_ERROR("failed to set BIO type");
        PrintOpenSSLErrors();
        goto x509_err;
    }

    if (!PEM_write_bio_X509(memPtr, certPtr))
    {
        LE_ERROR("failed to write certificate");
        PrintOpenSSLErrors();
        goto bio_err;
    }

    if (memPtr->num_write > *pemKeyLenPtr)
    {
        LE_ERROR("not enough space to hold the key");
        goto bio_err;
    }

    memset(pemKeyPtr, 0, memPtr->num_write + 1);

    *pemKeyLenPtr = BIO_read(memPtr, pemKeyPtr, memPtr->num_write);
    if (*pemKeyLenPtr < memPtr->num_write)
    {
        LE_ERROR("failed to read certificate: count (%d)", *pemKeyLenPtr);
        PrintOpenSSLErrors();
        goto pem_err;
    }

    BIO_free(memPtr);
    X509_free(certPtr);

    return LWM2MCORE_ERR_COMPLETED_OK;

pem_err:
    memset(pemKeyPtr, 0, memPtr->num_write + 1);
bio_err:
    BIO_free(memPtr);
x509_err:
    X509_free(certPtr);
    return LWM2MCORE_ERR_GENERAL_ERROR;
}
