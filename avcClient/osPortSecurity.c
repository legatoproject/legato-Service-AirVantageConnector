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
#include <openssl/hmac.h>
#include <lwm2mcore/security.h>
#include <legato.h>
#include <interfaces.h>
#include <avcClient.h>
#include <avcFsConfig.h>
#include <avcFs.h>
#include <sslUtilities.h>

//--------------------------------------------------------------------------------------------------
/**
 * Prefix to retrieve files from secStore service.
 */
//--------------------------------------------------------------------------------------------------
#define SECURE_STORAGE_PREFIX   "/avms"

//--------------------------------------------------------------------------------------------------
/**
 * Object 10243, certificate max size
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_CERT_MAX_SIZE     4000

//--------------------------------------------------------------------------------------------------
/**
 * Backup copy of a specific credential.
 */
//--------------------------------------------------------------------------------------------------
#define CREDENTIAL_BACKUP      "_BACKUP"

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
 * Retrieve a credential.
 * This API treatment needs to have a procedural treatment.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetCredential
(
    lwm2mcore_Credentials_t credId,     ///< [IN] credential Id of credential to be retrieved
    uint16_t                serverId,   ///< [IN] server Id
    char*                   bufferPtr,  ///< [INOUT] data buffer
    size_t*                 lenPtr      ///< [INOUT] length of input buffer and length of the
                                        ///< returned data
)
{
    (void)serverId;

    if ((bufferPtr == NULL) || (lenPtr == NULL) || (credId >= LWM2MCORE_CREDENTIAL_MAX))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    char credsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credsPathStr,
                                        sizeof(credsPathStr),
                                        CredentialLocations[credId],
                                        NULL), "Buffer is not long enough");
    le_result_t result = le_secStore_Read(credsPathStr, (uint8_t*)bufferPtr, lenPtr);
    if (LE_OK != result)
    {
        LE_ERROR("Unable to retrieve credentials for %d: %s: %d %s",
                 credId, credsPathStr, result, LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("credId %d, len %zu", credId, *lenPtr);

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
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetCredential
(
    lwm2mcore_Credentials_t credId,     ///< [IN] credential Id of credential to be set
    uint16_t                serverId,   ///< [IN] server Id
    char*                   bufferPtr,  ///< [INOUT] data buffer
    size_t                  len         ///< [IN] length of input buffer
)
{
    (void)serverId;

    if ((bufferPtr == NULL) || (credId >= LWM2MCORE_CREDENTIAL_MAX))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    char credsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credsPathStr,
                                        sizeof(credsPathStr),
                                        CredentialLocations[credId],
                                        NULL), "Buffer is not long enough");

    le_result_t result = le_secStore_Write(credsPathStr, (uint8_t*)bufferPtr, len);
    if (LE_OK != result)
    {
        LE_ERROR("Unable to write credentials for %d", credId);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("credId %d, len %zu", credId, len);

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to check if one credential is present in platform storage.
 *
 * Since there is no GetSize in the le_secStore.api, tries to retrieve the credentials with a buffer
 * too small.
 *
 * @return
 *      - true if the credential is present
 *      - false else
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_CheckCredential
(
    lwm2mcore_Credentials_t credId,     ///< [IN] Credential identifier
    uint16_t                serverId    ///< [IN] server Id
)
{
    char buffer[LWM2MCORE_PUBLICKEY_LEN] = {0};
    size_t bufferSz = sizeof(buffer);
    bool ret = false;
    const char* retTxt = "Not Present";
    lwm2mcore_Sid_t result;

    (void)serverId;

    result = lwm2mcore_GetCredential(credId, serverId, buffer, &bufferSz);
    if ((LWM2MCORE_ERR_COMPLETED_OK == result) && bufferSz)
    {
        ret = true;
        retTxt = "Present";
    }

    LE_DEBUG("credId %d result %s [%d]", credId, retTxt, ret);
    return ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to check if one credential is present in platform storage and matches with our
 * credentials.
 *
 * @return
 *      - true if the credential and matches with our credentials
 *      - false else
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_CredentialMatch
(
    lwm2mcore_Credentials_t credId,     ///< [IN] Credential identifier
    uint16_t                serverId,   ///< [IN] server Id
    const char*             credential  ///< [IN] Credential
)
{
    char buffer[LWM2MCORE_PUBLICKEY_LEN] = {0};
    size_t bufferSz = sizeof(buffer);
    bool ret = false;
    const char* retTxt = "Not Present";
    lwm2mcore_Sid_t result;

    (void)serverId;

    result = lwm2mcore_GetCredential(credId, serverId, buffer, &bufferSz);
    if ((LWM2MCORE_ERR_COMPLETED_OK == result) && bufferSz)
    {
        ret = true;
        retTxt = "Present";
    }

    if ((credential != NULL) && ret)
    {
        LE_DEBUG("Checking credentials against input credential.");
        if (strncmp(buffer, credential, bufferSz) != 0)
        {
            ret = false;
        }
    }

    LE_DEBUG("credId %d result %s [%d]", credId, retTxt, ret);
    return ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function erases one credential from platform storage
 *
 * @return
 *      - true if the credential is deleted
 *      - false else
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_DeleteCredential
(
    lwm2mcore_Credentials_t credId,     ///< [IN] Credential identifier
    uint16_t                serverId    ///< [IN] server Id
)
{
    (void)serverId;

    if (credId >= LWM2MCORE_CREDENTIAL_MAX)
    {
        LE_ERROR("Bad parameter credId[%u]", credId);
        return LE_BAD_PARAMETER;
    }

    char credsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credsPathStr,
                                        sizeof(credsPathStr),
                                        CredentialLocations[credId],
                                        NULL), "Buffer is not long enough");

    le_result_t result = le_secStore_Delete(credsPathStr);
    if ((LE_OK != result) && (LE_NOT_FOUND != result))
    {
        LE_ERROR("Unable to delete credentials for %d: %d %s",
                 credId, result, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    LE_DEBUG("credId %d deleted", credId);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Backup a credential.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid argument
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_BackupCredential
(
    lwm2mcore_Credentials_t credId,     ///< [IN] credential Id of credential to be retrieved
    uint16_t                serverId    ///< [IN] server Id
)
{
    (void)serverId;

    if (credId >= LWM2MCORE_CREDENTIAL_MAX)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    char credsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credsPathStr,
                                        sizeof(credsPathStr),
                                        CredentialLocations[credId],
                                        NULL), "Buffer is not long enough");

    char buffer[LE_SECSTORE_MAX_NAME_BYTES];
    size_t bufferSize = sizeof(buffer);
    le_result_t result = le_secStore_Read(credsPathStr, (uint8_t*)buffer, &bufferSize);
    if (LE_OK != result)
    {
        LE_ERROR("Unable to retrieve credentials for %d: %s: %d %s",
                 credId, credsPathStr, result, LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("credId %d, bufferSize %zu", credId, bufferSize);

    char backupCredsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = {0};
    snprintf(backupCredsPathStr, sizeof(backupCredsPathStr), "%s%s", credsPathStr, CREDENTIAL_BACKUP);

    result = le_secStore_Write(backupCredsPathStr, (uint8_t*)buffer, bufferSize);

    if (LE_OK != result)
    {
        LE_ERROR("Unable to backup credentials for %d: %s: %d %s",
                 credId, backupCredsPathStr, result, LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Restore credential to the previous credential.
 * Used for bs credential where we restore the credential back to the previous bs credential if
 * they exist. If not, we also remove the current bs credential and allow the device to retrieve
 * the bs credential from the modem.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_RestoreCredential
(
    lwm2mcore_Credentials_t credId,     ///< [IN] credential Id of credential to be retrieved
    uint16_t                serverId    ///< [IN] server Id
)
{
    (void)serverId;

    if (credId >= LWM2MCORE_CREDENTIAL_MAX)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    char backupCredsId[LE_SECSTORE_MAX_NAME_BYTES] = {0};
    snprintf(backupCredsId, sizeof(backupCredsId), "%s%s", CredentialLocations[credId], CREDENTIAL_BACKUP);

    char backupCredsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        backupCredsPathStr,
                                        sizeof(backupCredsPathStr),
                                        backupCredsId,
                                        NULL), "Buffer is not long enough");

    char buffer[LE_SECSTORE_MAX_NAME_BYTES];
    size_t bufferSize = sizeof(buffer);

    le_result_t result = le_secStore_Read(backupCredsPathStr, (uint8_t*)buffer, &bufferSize);

    char credsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credsPathStr,
                                        sizeof(credsPathStr),
                                        CredentialLocations[credId],
                                        NULL), "Buffer is not long enough");

    // If doesn't exist, then it's OK. No backup is not an indication of an error, just implies that
    // key rotation never occured or we have just restore the backup.
    if (LE_OK != result)
    {
        // Remove the current bs credential.
        le_secStore_Delete(credsPathStr);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    // Restore current bootstrap with backup copy
    result = le_secStore_Write(credsPathStr, (uint8_t*)buffer, bufferSize);

    if (LE_OK != result)
    {
        LE_ERROR("Unable to restore credentials for: %s: %d %s",
                 credsPathStr, result, LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Delete backup
    result = le_secStore_Delete(backupCredsPathStr);

    if (LE_OK != result)
    {
        LE_ERROR("Unable to delete credentials for: %s: %d %s",
                 credsPathStr, result, LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
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
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    BIO_set_close(bio, BIO_NOCLOSE);

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
    lwm2mcore_PkgDwlType_t packageType, ///< [IN] Package type (FW or SW)
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

//--------------------------------------------------------------------------------------------------
/**
 * Update SSL Certificate
 *
 * @note To delete the saved certificate, set the length to 0
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the update succeeds
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the size of the certificate is > 4000 bytes
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the update fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_UpdateSslCertificate
(
    char*           certPtr,    ///< [IN] Certificate
    size_t          len         ///< [IN] Certificate len
)
{
    char cert[MAX_CERT_LEN] = {0};
    le_result_t result;

    if (!certPtr)
    {
        LE_ERROR("NULL certificate");
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LWM2M_CERT_MAX_SIZE < len)
    {
        LE_ERROR("Size %zu is > than %d authorized", len, LWM2M_CERT_MAX_SIZE);
        return LWM2MCORE_ERR_INCORRECT_RANGE;
    }

    if (!len)
    {
        result = DeleteFs(SSLCERT_PATH);
        if (LE_OK != result)
        {
            LE_ERROR("Failed to delete certificate file");
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }
        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    memcpy(cert, certPtr, len);

    int pemLen = ssl_LayOutPEM(cert, len);
    if (-1 == pemLen)
    {
        LE_ERROR("ssl_LayOutPEM failed");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    result = WriteFs(SSLCERT_PATH, (uint8_t *)cert, pemLen);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to update certificate file");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}
