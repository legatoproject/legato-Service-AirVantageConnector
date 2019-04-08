/**
 * @file osPortCredentials.c
 *
 * Porting layer for credential management
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

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