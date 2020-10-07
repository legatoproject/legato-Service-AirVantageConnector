/**
 * @file osPortCredentials.c
 *
 * Porting layer for credential management
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/security.h>
#include <lwm2mcore/device.h>
#include <legato.h>
#include <interfaces.h>
#include "avcClient.h"
#include "avcFs/avcFsConfig.h"
#include "avcFs/avcFs.h"
#include "packageDownloader/sslUtilities.h"

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
 * Number of BS credentials which can be restored
 */
//--------------------------------------------------------------------------------------------------
#define BS_CREDENTIAL_NB_TO_RESTORE     3

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
 * Structure for BS credentials restore process
 */
//--------------------------------------------------------------------------------------------------
typedef struct {
    lwm2mcore_Credentials_t credId;             ///< LwM2MCore credential Id
    bool                    isCurrentPresent;   ///< Is the current BS credential present?
    bool                    isBackupPresent;    ///< Is the backup BS credential present?
    size_t                  currentSize;        ///< Current BS credential size
    size_t                  backupSize;         ///< Backup BS credential size
}
BsCredentialList_t;


//--------------------------------------------------------------------------------------------------
/**
 * Details for BS credentials restore process (Order of backup)
 */
//--------------------------------------------------------------------------------------------------
static BsCredentialList_t bsCredentialsList[BS_CREDENTIAL_NB_TO_RESTORE] =
{
    {LWM2MCORE_CREDENTIAL_BS_PUBLIC_KEY,    false, false, 0, 0},
    {LWM2MCORE_CREDENTIAL_BS_SECRET_KEY,    false, false, 0, 0},
    {LWM2MCORE_CREDENTIAL_BS_ADDRESS,       false, false, 0, 0}
};

//--------------------------------------------------------------------------------------------------
/**
 * Compose a credentials path.
 */
//--------------------------------------------------------------------------------------------------
static void GetCredPath
(
    uint16_t                serverId,       ///< [IN] server Id
    lwm2mcore_Credentials_t credId,         ///< [IN] credential Id of credential to be set
    char*                   credPathPtr,    ///< [INOUT] data buffer
    size_t                  credPathSize    ///< [IN] data buffer length
)
{
    LE_ASSERT(snprintf(credPathPtr,
                       credPathSize,
                       "%s",
                       SECURE_STORAGE_PREFIX) < credPathSize);
#if LE_CONFIG_AVC_FEATURE_EDM
    if (serverId <= LE_AVC_SERVER_ID_AIRVANTAGE)
    {
        // Store to the backward-compatible location (/avms/)
        LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                            credPathPtr,
                                            credPathSize,
                                            CredentialLocations[credId],
                                            (char*)NULL), "Buffer is not long enough");
    }
    else
    {
        // Store to /avms/ServerId/
        char serverIdStr[8] = {0};
        LE_ASSERT(snprintf(serverIdStr,
                           sizeof(serverIdStr),
                           "%"PRIu16,
                           serverId) < sizeof(serverIdStr));
        LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                            credPathPtr,
                                            credPathSize,
                                            serverIdStr,
                                            CredentialLocations[credId],
                                            (char*)NULL), "Buffer is not long enough");

        LE_INFO("Cred path: %s", credPathPtr);
    }
#else   // LE_CONFIG_AVC_FEATURE_EDM
    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credPathPtr,
                                        credPathSize,
                                        CredentialLocations[credId],
                                        (char*)NULL), "Buffer is not long enough");
#endif  // LE_CONFIG_AVC_FEATURE_EDM
}

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
    le_result_t result;
    char credsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    if ((bufferPtr == NULL) || (lenPtr == NULL) || (credId >= LWM2MCORE_CREDENTIAL_MAX))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    GetCredPath(serverId, credId, credsPathStr, sizeof(credsPathStr));

    LE_INFO("getting credential %d for server %u: path '%s'", credId, serverId, credsPathStr);

    result = le_secStore_Read(credsPathStr, (uint8_t*)bufferPtr, lenPtr);
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

    GetCredPath(serverId, credId, credsPathStr, sizeof(credsPathStr));

    LE_INFO("setting credential %d for server %u: path '%s'", credId, serverId, credsPathStr);

    le_result_t result = le_secStore_Write(credsPathStr, (uint8_t*)bufferPtr, len);
    if (LE_OK != result)
    {
        LE_ERROR("Unable to write credentials for %d, path '%s'", credId, credsPathStr);
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
    __attribute__((unused)) const char* retTxt = "Not Present";
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
    if (credId >= LWM2MCORE_CREDENTIAL_MAX)
    {
        LE_ERROR("Bad parameter credId[%u]", credId);
        return false;
    }

    char credsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    GetCredPath(serverId, credId, credsPathStr, sizeof(credsPathStr));

    le_result_t result = le_secStore_Delete(credsPathStr);
    if ((LE_OK != result) && (LE_NOT_FOUND != result))
    {
        LE_ERROR("Unable to delete credentials for %d: %d %s",
                 credId, result, LE_RESULT_TXT(result));
        return false;
    }

    LE_DEBUG("credId %d deleted", credId);

    return true;
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
                                        (char*)NULL), "Buffer is not long enough");

    char buffer[LE_SECSTORE_MAX_ITEM_SIZE];
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
    LE_ASSERT(snprintf(backupCredsPathStr,
                       sizeof(backupCredsPathStr),
                       "%s%s",
                       credsPathStr,
                       CREDENTIAL_BACKUP) < sizeof(backupCredsPathStr));

    result = le_secStore_Write(backupCredsPathStr, (uint8_t*)buffer, bufferSize);

    if (LE_OK != result)
    {
        LE_ERROR("Unable to backup credentials for %d: %s: %d %s",
                 credId, backupCredsPathStr, result, LE_RESULT_TXT(result));
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//-------------------------------------------------------------------------------------------------
/**
 * Restore credentials. Used to trigger rollback mechanism in case of failure.
 * @return
 *      - LE_OK if restore operation completes successfully
 *      - LE_NOT_FOUND if entry does not exist
 *      - LE_FAULT if error occurs
 */
//-------------------------------------------------------------------------------------------------
static le_result_t RestoreCredentials
(
    lwm2mcore_Credentials_t credId     ///< [IN] credential Id of credential to be retrieved
)
{
    if (credId >= LWM2MCORE_CREDENTIAL_MAX)
    {
        return LE_FAULT;
    }

    char backupCredsId[LE_SECSTORE_MAX_NAME_BYTES] = {0};
    LE_ASSERT(snprintf(backupCredsId,
                       sizeof(backupCredsId),
                       "%s%s",
                       CredentialLocations[credId],
                       CREDENTIAL_BACKUP) < sizeof(backupCredsId));

    char backupCredsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        backupCredsPathStr,
                                        sizeof(backupCredsPathStr),
                                        backupCredsId,
                                        (void*)0), "Buffer is not long enough");

    char buffer[LE_SECSTORE_MAX_ITEM_SIZE];
    size_t bufferSize = sizeof(buffer);

    le_result_t result = le_secStore_Read(backupCredsPathStr, (uint8_t*)buffer, &bufferSize);

    char credsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

    LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                        credsPathStr,
                                        sizeof(credsPathStr),
                                        CredentialLocations[credId],
                                        (void*)0), "Buffer is not long enough");

    // If doesn't exist, then it's OK. No backup is not an indication of an error, just implies that
    // key rotation never occured or we have just restore the backup.
    if (LE_OK != result)
    {
        // Remove the current bs credential.
        le_secStore_Delete(credsPathStr);
        return LE_NOT_FOUND;
    }

    // Restore current bootstrap with backup copy
    result = le_secStore_Write(credsPathStr, (uint8_t*)buffer, bufferSize);

    if (LE_OK != result)
    {
        LE_ERROR("Unable to restore credentials for: %s: %d %s",
                 credsPathStr, result, LE_RESULT_TXT(result));
    }

    // Delete backup
    result = le_secStore_Delete(backupCredsPathStr);

    if (LE_OK != result)
    {
        LE_ERROR("Unable to delete credentials for: %s: %d %s",
                 credsPathStr, result, LE_RESULT_TXT(result));
    }

    return result;
}

//-------------------------------------------------------------------------------------------------
/**
 * Restore bootstrap credentials. Used to trigger rollback mechanism in case of failure.
 */
//-------------------------------------------------------------------------------------------------
void avcClient_FixBootstrapCredentials
(
    bool isBsAuthFailure        ///< [IN] Was authentication failed with the bootstrap server ?
)
{
    le_result_t result = LE_OK;
    uint8_t bsServerAddrIndex = 0;

    char currentCredential[LE_SECSTORE_MAX_ITEM_SIZE];
    size_t currentCredentialSize = sizeof(currentCredential);
    char backupCredential[LE_SECSTORE_MAX_ITEM_SIZE];
    size_t backupCredentialSize = sizeof(backupCredential);

    int i = 0;
    for (i = 0; i < sizeof(bsCredentialsList) / sizeof(BsCredentialList_t); i++)
    {
        if (LWM2MCORE_CREDENTIAL_BS_ADDRESS == bsCredentialsList[i].credId)
        {
            bsServerAddrIndex = (uint8_t)i;
        }

        memset(currentCredential, 0, LE_SECSTORE_MAX_ITEM_SIZE);
        currentCredentialSize = sizeof(currentCredential);

        memset(backupCredential, 0, LE_SECSTORE_MAX_ITEM_SIZE);
        backupCredentialSize = sizeof(backupCredential);

        // Get current BS credential
        char credsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

        LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                            credsPathStr,
                                            sizeof(credsPathStr),
                                            CredentialLocations[bsCredentialsList[i].credId],
                                            (void*)0), "Buffer is not long enough");

        result = le_secStore_Read(credsPathStr,
                                  (uint8_t*)currentCredential,
                                  &currentCredentialSize);

        if (result != LE_OK)
        {
            LE_WARN("Unable to read: %s", credsPathStr);
        }
        else
        {
            bsCredentialsList[i].isCurrentPresent = true;
            bsCredentialsList[i].currentSize = currentCredentialSize;
        }

        // Get backup BS credential
        char backupCredsId[LE_SECSTORE_MAX_NAME_BYTES] = {0};
        LE_ASSERT(snprintf(backupCredsId,
                           sizeof(backupCredsId),
                           "%s%s",
                           CredentialLocations[bsCredentialsList[i].credId],
                           CREDENTIAL_BACKUP) < sizeof(backupCredsId));

        char backupCredsPathStr[LE_SECSTORE_MAX_NAME_BYTES] = SECURE_STORAGE_PREFIX;

        LE_FATAL_IF(LE_OK != le_path_Concat("/",
                                            backupCredsPathStr,
                                            sizeof(backupCredsPathStr),
                                            backupCredsId,
                                            (char*)NULL), "Buffer is not long enough");

        result = le_secStore_Read(backupCredsPathStr,
                                 (uint8_t*)backupCredential,
                                 &backupCredentialSize);

        if (result != LE_OK)
        {
            LE_WARN("Unable to read: %s", backupCredsPathStr);
        }
        else
        {
            bsCredentialsList[i].isBackupPresent = true;
            bsCredentialsList[i].backupSize = backupCredentialSize;
        }
    }

    LE_DEBUG("bsServerAddrIndex %d", bsServerAddrIndex);
    // If current BS server addr len is 0, restore all BS credentials.
    // If bs authentication failure, restore all BS credentials.
    if (
          (isBsAuthFailure) ||
           ((bsCredentialsList[bsServerAddrIndex].isCurrentPresent)
             && (0 == bsCredentialsList[bsServerAddrIndex].currentSize))
       )
    {
        LE_DEBUG("Restoring bootstrap credentials.");
        result = RestoreCredentials(LWM2MCORE_CREDENTIAL_BS_PUBLIC_KEY);

        if (result != LE_OK)
        {
            LE_WARN("Restore BS PSK Id failure: %s", LE_RESULT_TXT(result));
        }

        result = RestoreCredentials(LWM2MCORE_CREDENTIAL_BS_SECRET_KEY);

        if (result != LE_OK)
        {
            LE_WARN("Restore BS PSK secret failure: %s", LE_RESULT_TXT(result));
        }

        result = RestoreCredentials(LWM2MCORE_CREDENTIAL_BS_ADDRESS);

        if (result != LE_OK)
        {
            LE_WARN("Restore BS server addr failure: %s", LE_RESULT_TXT(result));
        }
    }
}
