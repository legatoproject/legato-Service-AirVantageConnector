/**
 * @file osPortCredentialsSecure.c
 *
 * Porting layer for credential management.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
#include <string.h>
#include <lwm2mcore/security.h>
#include "legato.h"
#include "interfaces.h"
#include "avcClient.h"
#include "iks_keyStore.h"
#ifdef LE_CONFIG_LINUX
#include "iks_keyManagement.h"
#include "cmdFormat.h"
#else
#include "../src/cmdFormat.h"
#endif

//--------------------------------------------------------------------------------------------------
/**
 * IKS wrapping key identifier
 */
//--------------------------------------------------------------------------------------------------
#define IKS_WRAP_KEY_NAME                "iksWrapKey"

//--------------------------------------------------------------------------------------------------
/**
 * Bootstrap PSK maximum size in bytes
 */
//--------------------------------------------------------------------------------------------------
#define BOOTSTRAP_PSK_MAX_SIZE           (64)

#ifndef LE_CONFIG_TARGET_HL78
//--------------------------------------------------------------------------------------------------
/**
 * Prefix of path where AVMS credentials are stored.
 * Note that ThreadX and Linux platform uses same path names for storing AVMS credentials.
 */
//--------------------------------------------------------------------------------------------------
#define AVMS_PATH_PREFIX "avms"

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of credential path name
 */
//--------------------------------------------------------------------------------------------------
#define LE_CREDPATH_MAX_NAME_BYTES 256

//--------------------------------------------------------------------------------------------------
/**
 * Server ID for the current session.
 */
//--------------------------------------------------------------------------------------------------
static uint16_t ServerId = LE_AVC_SERVER_ID_AIRVANTAGE;

//--------------------------------------------------------------------------------------------------
/**
 * AVMS credentials key entries
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
#else
// Due to legacy design, FreeRTOS platform used different location than Linux and ThreadX platform.
// Keep the original/intial path naming to store AVMS credential on FreeRTOS platform.
#define AVMS_PATH_PREFIX "AVMS"

static const char* CredentialLocations[LWM2MCORE_CREDENTIAL_MAX] = {
   "Firmware_PubKey",          ///< LWM2MCORE_CREDENTIAL_FW_KEY
   "Software_PubKey",          ///< LWM2MCORE_CREDENTIAL_SW_KEY
   "Certif",                   ///< LWM2MCORE_CREDENTIAL_CERTIFICATE
   "BsPskId",                  ///< LWM2MCORE_CREDENTIAL_BS_PUBLIC_KEY
   "BsServerPskId",            ///< LWM2MCORE_CREDENTIAL_BS_SERVER_PUBLIC_KEY
   "BsPSK",                    ///< LWM2MCORE_CREDENTIAL_BS_SECRET_KEY
   "BsAddr",                   ///< LWM2MCORE_CREDENTIAL_BS_ADDRESS
   "DmPskId",                  ///< LWM2MCORE_CREDENTIAL_DM_PUBLIC_KEY
   "DmServerPskId ",           ///< LWM2MCORE_CREDENTIAL_DM_SERVER_PUBLIC_KEY
   "DmPSK",                    ///< LWM2MCORE_CREDENTIAL_DM_SECRET_KEY
   "DmAddr"                    ///< LWM2MCORE_CREDENTIAL_DM_ADDRESS
};
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Compose a credential name string
 *
 * @return
 *     LE_OK if successful
 *     LE_NO_MEMORY is the name buffer is insufficiently sized
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetCredentialName
(
    char*                   credName,     ///< [OUT] Credential name buffer
    size_t                  credNameSize, ///< [IN] Size of credential name buffer
    lwm2mcore_Credentials_t credId,       ///< [IN] Credential ID
    uint16_t                serverId      ///< [IN] Server ID
)
{
    le_result_t status = LE_OK;

    LE_ASSERT(NULL != credName);
    LE_ASSERT(0    <  credNameSize);

    (void)serverId; // To avoid warnings if !LE_CONFIG_AVC_FEATURE_EDM

#if LE_CONFIG_AVC_FEATURE_EDM
    if (serverId <= LE_AVC_SERVER_ID_AIRVANTAGE)
    {
#endif // LE_CONFIG_AVC_FEATURE_EDM
        if (snprintf(credName, credNameSize, "%s/%s", AVMS_PATH_PREFIX,
                     CredentialLocations[credId]) < 0)
        {
            status = LE_NO_MEMORY;
        }
#if LE_CONFIG_AVC_FEATURE_EDM
    }
    else
    {
        if (snprintf(credName, credNameSize, "%s/%u/%s", AVMS_PATH_PREFIX, serverId,
                     CredentialLocations[credId]) < 0)
        {
            status = LE_NO_MEMORY;
        }
    }
#endif // LE_CONFIG_AVC_FEATURE_EDM

    if (LE_NO_MEMORY == status)
    {
        LE_ERROR("Credential name buffer is too small");
    }
    return status;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the IKS wrapping key container and provision the wrapping key into it.
 *
 * The IKS generates a wrapping key once per boot. This function fetches that key and provisions it
 * into an ephemeral IKS key container.
 */
//--------------------------------------------------------------------------------------------------
void InitIksWrappingKey
(
    void
)
{
    iks_result_t iksStatus;
    iks_KeyRef_t wrapKeyRef    = NULL;
    le_result_t  overallStatus = LE_FAULT;

    // If the wrapping key already exists, do nothing
    iksStatus = iks_GetKey(IKS_WRAP_KEY_NAME, &wrapKeyRef);
    if (IKS_OK == iksStatus)
    {
        LE_INFO("IKS wrapping key already initialized");
    }
    else
    {
        uint8_t       wrapKeyPkg[IKS_MAX_SERIALIZED_SIZE] = {0};
        size_t        wrapKeyPkgSize                      = sizeof(wrapKeyPkg);
        uint8_t       wrapKeyVal[IKS_MAX_SERIALIZED_SIZE] = {0};
        size_t        wrapKeyValSize                      = sizeof(wrapKeyVal);
        iks_KeyType_t wrapKeyType                         = IKS_KEY_TYPE_MAX;
        size_t        wrapKeySize                         = 0;

        LE_INFO("Initializing IKS wrapping key");

        // Get IKS wrapping key package
        iksStatus = iks_GetWrappingKey(wrapKeyPkg,
                                       &wrapKeyPkgSize);
        if (IKS_OK != iksStatus)
        {
            LE_ERROR("Failed to get IKS wrapping key package: %u", iksStatus);
            goto cleanup;
        }

        // Parse IKS wrapping key package and extract the embedded wrapping key. The package
        // is a DER encoding of the ASN.1 structure PublicKey defined in iks_KeyManagement.h.
        iksStatus = cmd_ReadWrappingKeyPackage(wrapKeyPkg,
                                               wrapKeyPkgSize,
                                               &wrapKeyType,
                                               &wrapKeySize,
                                               wrapKeyVal,
                                               &wrapKeyValSize);
        if (IKS_OK != iksStatus)
        {
            LE_ERROR("Failed to parse IKS wrapping key package: %u", iksStatus);
            goto cleanup;
        }

        // Create an ephemeral container to store the wrapping key
        iksStatus = iks_CreateKeyByType(IKS_WRAP_KEY_NAME,
                                        wrapKeyType,
                                        wrapKeySize,
                                        &wrapKeyRef);
        if (IKS_OK != iksStatus)
        {
            LE_ERROR("Failed to create IKS wrapping key container: %u", iksStatus);
            goto cleanup;
        }

        // Load the wrapping key into the container
        iksStatus = iks_ProvisionKeyValue(wrapKeyRef,
                                          wrapKeyVal,
                                          wrapKeyValSize);
        if (IKS_OK != iksStatus)
        {
            LE_ERROR("Failed to provision IKS wrapping key: %u", iksStatus);
            goto cleanup;
        }
    }

    overallStatus = LE_OK;

cleanup:
    if (LE_OK != overallStatus)
    {
        if (NULL != wrapKeyRef)
        {
            iks_DeleteKey(wrapKeyRef, NULL, 0);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a reference to the IKS wrapping key.
 *
 * @return
 *      LE_OK if successful
 *      LE_NOT_FOUND if wrapping key cannot be located
 *      LE_FAULT if unable to get the key type of the wrapping key
 */
//--------------------------------------------------------------------------------------------------
static iks_result_t GetIksWrappingKey
(
    iks_KeyType_t* const retKeyType, /// [OUT] Wrapping key type
    iks_KeyRef_t* const  retKeyRef   /// [OUT] Wrapping key reference
)
{
    iks_result_t  iksStatus;
    iks_KeyRef_t  wrapKeyRef = NULL;
    iks_KeyType_t wrapKeyType;

    LE_ASSERT(NULL != retKeyType);
    LE_ASSERT(NULL != retKeyRef);

    iksStatus = iks_GetKey(IKS_WRAP_KEY_NAME, &wrapKeyRef);
    if (IKS_OK != iksStatus)
    {
        LE_ERROR("Unable to get reference to IKS wrapping key: %u", iksStatus);
        return IKS_NOT_FOUND;
    }

    iksStatus = iks_GetKeyType(wrapKeyRef, &wrapKeyType);
    if (IKS_OK != iksStatus)
    {
        LE_ERROR("Failed to get IKS wrapping key types: %u", iksStatus);
        return IKS_OPERATION_FAILED;
    }

    *retKeyRef  = wrapKeyRef;
    *retKeyType = wrapKeyType;
    return IKS_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Wrap a credential using an IKS-generated wrapping key.
 *
 * @return
 *      LE_OK if successful. Otherwise an appropriate error code is returned.
 */
//--------------------------------------------------------------------------------------------------
static iks_result_t WrapIksCredential
(
    const uint8_t*       cred,           ///< [IN] Credential
    const size_t         credLen,        ///< [IN] Credential length
    const iks_KeyType_t  wrapKeyType,    ///< [IN] Wrapping key type
    const iks_KeyRef_t   wrapKeyRef,     ///< [IN] Wrapping key reference
    uint8_t*             wrappedCred,    ///< [OUT] Wrapped credential
    size_t*              wrappedCredLen  ///< [OUT] Wrapped credential length
)
{
    iks_result_t iksStatus                          = IKS_INTERNAL_ERROR;
    uint8_t      ciphertext[BOOTSTRAP_PSK_MAX_SIZE] = {0};
    uint8_t      ephemKey[IKS_LARGEST_KEY_SIZE]     = {0};
    size_t       ephemKeyLen                        = sizeof(ephemKey);
    uint8_t      authTag[IKS_AES_GCM_TAG_SIZE]      = {0};

    LE_ASSERT(NULL != cred);
    LE_ASSERT(NULL != wrapKeyRef);
    LE_ASSERT(NULL != wrappedCred);
    LE_ASSERT(NULL != wrappedCredLen);
    LE_ASSERT(credLen <= sizeof(ciphertext)); // Ciphertext is always same length as credential

    switch (wrapKeyType)
    {
#if defined(IKS_RSA_ENABLED) && defined(IKS_AES_GSM_ENABLED)
        case IKS_KEY_TYPE_PUB_RSAES_OAEP_SHA256_AES128_GCM:
        {
            iksStatus = iks_rsaHybrid_EncryptPacket(wrapKeyRef,
                                                    NULL,
                                                    0,
                                                    cred,
                                                    ciphertext,
                                                    credLen,
                                                    ephemKey,
                                                    &ephemKeyLen,
                                                    authTag,
                                                    sizeof(authTag));
            if (IKS_OK != iksStatus)
            {
                LE_ERROR("Failed to encrypt credential: %u", iksStatus);
                goto exit;
            }
            break;
        }
#endif // defined(IKS_RSA_ENABLED) && defined(IKS_AES_GSM_ENABLED)

#if defined(IKS_ECC_ENABLED) && defined(IKS_AES_GCM_ENABLED)
        case IKS_KEY_TYPE_PUB_ECIES_HKDF_SHA512_AES256_GCM:
        case IKS_KEY_TYPE_PUB_ECIES_HKDF_SHA256_AES128_GCM:
        {
            iksStatus = iks_ecies_EncryptPacket(wrapKeyRef,
                                                NULL,
                                                0,
                                                cred,
                                                ciphertext,
                                                credLen,
                                                ephemKey,
                                                &ephemKeyLen,
                                                authTag,
                                                sizeof(authTag));
            if (IKS_OK != iksStatus)
            {
                LE_ERROR("Failed to encrypt credential: %u", iksStatus);
                goto exit;
            }
            break;
        }
#endif // defined(IKS_ECC_ENABLED) && defined(IKS_AES_GCM_ENABLED)

        default:
        {
            LE_ERROR("Key type not supported: %u", wrapKeyType);
            goto exit;
        }
    }

    // Create WrappedData package
    iksStatus = cmd_CreateWrappedData(ephemKey,
                                      ephemKeyLen,
                                      authTag,
                                      sizeof(authTag),
                                      ciphertext,
                                      credLen,
                                      wrappedCred,
                                      wrappedCredLen);
    if (IKS_OK != iksStatus)
    {
        LE_ERROR("Failed to create WrappedData package: %u", iksStatus);
    }

exit:
    memset(ciphertext, 0, sizeof(ciphertext));
    memset(ephemKey, 0, sizeof(ephemKey));
    memset(authTag, 0, sizeof(authTag));

    return iksStatus;
}

//--------------------------------------------------------------------------------------------------
/**
 * Store a credential in the IKS.
 *
 * @return
 *      LE_OK if successful.
 *      LE_FAULT if operation fails.
 */
//--------------------------------------------------------------------------------------------------
static iks_result_t WriteIksCredential
(
    const char*         credName,    ///< [IN] Credential name
    const iks_KeyType_t credType,    ///< [IN] Credential type
    const uint8_t*      credValue,   ///< [IN] Credential value
    const size_t        credValueLen ///< [IN] Credential length
)
{
    iks_result_t  iksStatus;
    iks_KeyType_t wrapKeyType;
    iks_KeyRef_t  wrapKeyRef = NULL;
    iks_KeyRef_t  credRef    = NULL;

    LE_ASSERT(NULL != credName);
    LE_ASSERT(NULL != credValue);
    LE_ASSERT(0    <  credValueLen);

    InitIksWrappingKey();

    // If a credential with the same name already exists, delete it as the
    // IoT Key Store rejects duplicates.
    iksStatus = iks_GetKey(credName, &credRef);
    if (IKS_OK == iksStatus)
    {
        LE_INFO("Credential %s already exists... deleting duplicate", credName);
        iks_DeleteKey(credRef, NULL, 0);
    }

    iksStatus = iks_CreateKeyByType(credName, credType, credValueLen, &credRef);
    if (IKS_OK != iksStatus)
    {
        LE_ERROR("Failed to create credential container: %u", iksStatus);
        goto err_exit;
    }

    iksStatus = GetIksWrappingKey(&wrapKeyType, &wrapKeyRef);
    if (IKS_OK != iksStatus)
    {
        LE_ERROR("Failed to retrieve IKS wrapping key: %d", iksStatus);
        goto err_exit;
    }

    // NOTE: New scope to reduce number of large buffers co-existing on the stack
    {
        uint8_t wrappedCredPkg[IKS_MAX_SERIALIZED_SIZE] = {0};
        size_t  wrappedCredPkgSize                      = sizeof(wrappedCredPkg);

        iksStatus = WrapIksCredential(credValue,
                                   credValueLen,
                                   wrapKeyType,
                                   wrapKeyRef,
                                   wrappedCredPkg,
                                   &wrappedCredPkgSize);
        if (IKS_OK != iksStatus)
        {
            LE_ERROR("Failed to wrap IKS credential: %d", iksStatus);
            goto err_exit;
        }

        iksStatus = iks_ProvisionKeyValue(credRef, wrappedCredPkg, wrappedCredPkgSize);
        if (IKS_OK != iksStatus)
        {
            LE_ERROR("Failed to provision credential %s: %u", credName, iksStatus);
            goto err_exit;
        }
    }

    iksStatus = iks_SaveKey(credRef);
    if (IKS_OK != iksStatus)
    {
        LE_ERROR("Failed to save credential %s: %u", credName, iksStatus);
        goto err_exit;
    }

    return IKS_OK;

err_exit:
    if (NULL != credRef)
    {
        iks_DeleteKey(credRef, NULL, 0);
    }

    return IKS_OPERATION_FAILED;
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
    le_result_t  status;
    char         credName[LE_SECSTORE_MAX_NAME_BYTES] = {0};
    iks_result_t iksStatus;
    iks_KeyRef_t pskRef                               = NULL;

    if (   (NULL == bufferPtr)
        || (NULL == lenPtr)
        || (credId >= LWM2MCORE_CREDENTIAL_MAX))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    status = GetCredentialName(credName, sizeof(credName), credId, serverId);
    if (LE_OK != status)
    {
        LE_ERROR("Failed to compose credential name: %d", status);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    switch (credId)
    {
        case LWM2MCORE_CREDENTIAL_BS_SECRET_KEY:
        case LWM2MCORE_CREDENTIAL_DM_SECRET_KEY:
        {
            LE_INFO("Retrieving %s from IoTKeystore", credName);
            // NOTE: It is not possible to retrieve the raw value of a credential stored in the
            // IKS so, instead, we get a reference and cast it as a char[]. To use the key, the
            // caller must convert it back to an IKS key reference object.
            iksStatus = iks_GetKey(credName, &pskRef);
            if (IKS_OK != iksStatus)
            {
                LE_ERROR("Failed to retrieve PSK credential %s reference: %s",
                         credName,
                         iks_ResultStr(iksStatus));
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }
            if (sizeof(pskRef) > *lenPtr)
            {
                LE_ERROR("The result buffer is too small");
                return LWM2MCORE_ERR_MEMORY;
            }
            memcpy((void*)bufferPtr, (void*)&pskRef, sizeof(pskRef));
            *lenPtr = sizeof(pskRef);
            break;
        }
        case LWM2MCORE_CREDENTIAL_FW_KEY:
        case LWM2MCORE_CREDENTIAL_SW_KEY:
        case LWM2MCORE_CREDENTIAL_CERTIFICATE:
        case LWM2MCORE_CREDENTIAL_BS_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_BS_SERVER_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_BS_ADDRESS:
        case LWM2MCORE_CREDENTIAL_DM_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_DM_SERVER_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_DM_ADDRESS:
        {
            status = le_secStore_Read(credName, (uint8_t*)bufferPtr, lenPtr);
            if (LE_OK != status)
            {
                LE_ERROR("Failed to retrieve credential %s: %s", credName, LE_RESULT_TXT(status));
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;
        }
        default:
        {
            // ID should have been sanitized before reaching this code
            LE_FATAL("== SHOULDN'T GET HERE! ==");
            break;
        }
    }

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
    le_result_t status;
    iks_result_t iksStatus;
    char        credName[LE_SECSTORE_MAX_NAME_BYTES] = {0};

    if (   (NULL == bufferPtr)
        || (credId >= LWM2MCORE_CREDENTIAL_MAX))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    status = GetCredentialName(credName, sizeof(credName), credId, serverId);
    if (LE_OK != status)
    {
        LE_ERROR("Failed to compose credential name: %d", status);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    switch (credId)
    {
        case LWM2MCORE_CREDENTIAL_BS_SECRET_KEY:
        case LWM2MCORE_CREDENTIAL_DM_SECRET_KEY:
        {
            LE_INFO("Setting %s in IoTKeystore", credName);
            iksStatus = WriteIksCredential(credName,
                                        IKS_KEY_TYPE_TLS_1_2_PSK_SHA256,
                                        (uint8_t*)bufferPtr,
                                        len);
            if (IKS_OK != iksStatus)
            {
                LE_ERROR("Failed to store IKS credential %s: %d", credName, iksStatus);
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;
        }
        case LWM2MCORE_CREDENTIAL_FW_KEY:
        case LWM2MCORE_CREDENTIAL_SW_KEY:
        case LWM2MCORE_CREDENTIAL_CERTIFICATE:
        case LWM2MCORE_CREDENTIAL_BS_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_BS_SERVER_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_BS_ADDRESS:
        case LWM2MCORE_CREDENTIAL_DM_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_DM_SERVER_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_DM_ADDRESS:
        {
            status = le_secStore_Write(credName,
                                       (uint8_t*)bufferPtr,
                                       len);
            if (LE_OK != status)
            {
                LE_ERROR("Failed to store SecStore credential %s: %d", credName, status);
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;
        }
        default:
        {
            // ID should have been sanitized before reaching this code
            LE_FATAL("== SHOULDN'T GET HERE! ==");
            break;
        }
    }

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
    le_result_t  status;
    iks_result_t iksStatus;
    iks_KeyRef_t iksKeyRef                               = NULL;
    uint8_t      secStoreKeyBuf[LWM2MCORE_PUBLICKEY_LEN] = {0};
    size_t       secStoreKeySize                         = sizeof(secStoreKeyBuf);
    char         credName[LE_SECSTORE_MAX_NAME_BYTES]    = {0};
    bool         ret                                     = false;
    __attribute__((unused)) const char*  retTxt          = "Not Present";

    if (credId >= LWM2MCORE_CREDENTIAL_MAX)
    {
        LE_ERROR("Invalid credential ID: %u", credId);
        goto exit;
    }

    status = GetCredentialName(credName, sizeof(credName), credId, serverId);
    if (LE_OK != status)
    {
        LE_ERROR("Failed to compose credential name: %d", status);
        goto exit;
    }

    switch (credId)
    {
        case LWM2MCORE_CREDENTIAL_BS_SECRET_KEY:
        case LWM2MCORE_CREDENTIAL_DM_SECRET_KEY:
        {
            iksStatus = iks_GetKey(credName, &iksKeyRef);
            if (IKS_OK == iksStatus)
            {
                ret = true;
            }
            break;
        }
        case LWM2MCORE_CREDENTIAL_FW_KEY:
        case LWM2MCORE_CREDENTIAL_SW_KEY:
        case LWM2MCORE_CREDENTIAL_CERTIFICATE:
        case LWM2MCORE_CREDENTIAL_BS_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_BS_SERVER_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_BS_ADDRESS:
        case LWM2MCORE_CREDENTIAL_DM_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_DM_SERVER_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_DM_ADDRESS:
        {
            status = le_secStore_Read(credName, secStoreKeyBuf, &secStoreKeySize);
            if (LE_OK == status)
            {
                ret = true;
            }
            break;
        }
        default:
        {
            // ID should have been sanitized before reaching this code
            LE_FATAL("== SHOULDN'T GET HERE! ==");
            break;
        }
    }

exit:
    if (ret)
    {
        retTxt = "Present";
        LE_INFO("Credential %s check result: %s", credName, retTxt);
    }

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
    le_result_t  status;
    bool         ret                                  = false;
    char         credName[LE_SECSTORE_MAX_NAME_BYTES] = {0};

    if (credId >= LWM2MCORE_CREDENTIAL_MAX)
    {
        LE_ERROR("Invalid credential ID: %u", credId);
        goto exit;
    }

    status = GetCredentialName(credName, sizeof(credName), credId, serverId);
    if (LE_OK != status)
    {
        LE_ERROR("Failed to compose credential name: %d", status);
        goto exit;
    }

    switch (credId)
    {
        case LWM2MCORE_CREDENTIAL_BS_SECRET_KEY:
        case LWM2MCORE_CREDENTIAL_DM_SECRET_KEY:
        {
            iks_result_t iksStatus;
            iks_KeyRef_t iksKeyRef = NULL;

            iksStatus = iks_GetKey(credName, &iksKeyRef);
            if (IKS_OK != iksStatus)
            {
                LE_ERROR("Credential %s not found", credName);
                goto exit;
            }

            iksStatus = iks_DeleteKey(iksKeyRef, NULL, 0);
            if (IKS_OK != iksStatus)
            {
                LE_ERROR("Failed to delete IKS credential %s: %u", credName, iksStatus);
                goto exit;
            }

            ret = true;
            break;
        }
        case LWM2MCORE_CREDENTIAL_FW_KEY:
        case LWM2MCORE_CREDENTIAL_SW_KEY:
        case LWM2MCORE_CREDENTIAL_CERTIFICATE:
        case LWM2MCORE_CREDENTIAL_BS_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_BS_SERVER_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_BS_ADDRESS:
        case LWM2MCORE_CREDENTIAL_DM_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_DM_SERVER_PUBLIC_KEY:
        case LWM2MCORE_CREDENTIAL_DM_ADDRESS:
        {
            status = le_secStore_Delete(credName);
            if (LE_OK != status)
            {
                LE_ERROR("Failed to delete SecStore credential %s: %d", credName, status);
                goto exit;
            }

            ret = true;
            break;
        }
        default:
        {
            // ID should have been sanitized before reaching this code
            LE_FATAL("== SHOULDN'T GET HERE! ==");
            break;
        }
    }

exit:
    if (ret)
    {
        LE_INFO("Deleted LwM2M credential %s", credName);
    }
    return ret;
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
    LE_UNUSED(credId);
    LE_UNUSED(serverId);
    // TODO: implement this function for RTOS
    return LWM2MCORE_ERR_COMPLETED_OK;
}

#ifndef LE_CONFIG_TARGET_HL78
//--------------------------------------------------------------------------------------------------
/**
 * Migrate given credential value from secure storage to IoTKeystore.
 * First check whether the credential already exists in IoTKeystore. If the credential already
 * exist then there is no need to migrate credential. However, if the credential does not exist in
 * IoTKeystore, migrate the credential from config tree based secure storage or modem SFS depending
 * on where the credential is stored in the platform.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK on success.
 *      - LWM2MCORE_ERR_GENERAL_ERROR if failed.
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t MigrateCredentialID
(
    lwm2mcore_Credentials_t credId /// [IN] lwm2mcore credential ID
)
{
    char                   credName[LE_CREDPATH_MAX_NAME_BYTES] = {0};
    char                   buffer[1024] = {0};
    size_t                 len = sizeof(buffer);
    le_result_t            status = LE_FAULT;

    status = GetCredentialName(credName, sizeof(credName), credId, ServerId);
    if (LE_OK != status)
    {
        LE_ERROR("Failed to compose credential name: %d", status);
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    if (lwm2mcore_CheckCredential(credId, ServerId) == true)
    {
        LE_INFO("Credential '%s' already exists in IoTKeystore. Skip migration.", credName);
        return LWM2MCORE_ERR_ALREADY_PROCESSED;
    }

    LE_INFO("Migrate %s to IoTKeyStore", credName);

    // Read from either config tree based secure storage or modem SFS.
    status = le_secStore_Read(credName, (uint8_t*)buffer, &len);
    if (LE_OK != status)
    {
        LE_ERROR("Failed to retrieve credential %s: %s from secure storage",
                    credName, LE_RESULT_TXT(status));

        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    lwm2mcore_Sid_t lwm2mStatus = LWM2MCORE_ERR_GENERAL_ERROR;

    lwm2mStatus = lwm2mcore_SetCredential(credId,
                                          ServerId,
                                          buffer,
                                          len);
    if (LWM2MCORE_ERR_COMPLETED_OK != lwm2mStatus)
    {
        LE_ERROR("Failed to write LwM2M credential to IoTKeystore: %u", lwm2mStatus);

        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_INFO("LwM2M cred %u (%s) successfully written to IoTKeystore", credId, credName);

    LE_INFO("Deleting LwM2M cred %u (%s) from secure storage", credId, credName);
    status = le_secStore_Delete(credName);
    if (LE_OK != status)
    {
        LE_ERROR("Failed to retrieve credential %s: %s from secure storage",
                    credName, LE_RESULT_TXT(status));

        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Migrate secret AVMS credential value from secure storage to IoTKeystore
 */
//--------------------------------------------------------------------------------------------------
void MigrateAVMSCredentialIKS
(
    void
)
{
    lwm2mcore_Sid_t status = LWM2MCORE_ERR_GENERAL_ERROR;
    char* resultstr = "Fail";

    status = MigrateCredentialID(LWM2MCORE_CREDENTIAL_BS_SECRET_KEY);
    if (LWM2MCORE_ERR_COMPLETED_OK == status)
    {
        resultstr = "Pass";
    }
    else if (LWM2MCORE_ERR_ALREADY_PROCESSED == status)
    {
        resultstr = "Not Applicable";
    }

    LE_INFO("%s: Migration of BS secret key from secure storage to IoTKeystore", resultstr);

    resultstr = "Fail";

    status = MigrateCredentialID(LWM2MCORE_CREDENTIAL_DM_SECRET_KEY);
    if (LWM2MCORE_ERR_COMPLETED_OK == status)
    {
        resultstr = "Pass";
    }
    else if (LWM2MCORE_ERR_ALREADY_PROCESSED == status)
    {
        resultstr = "Not Applicable";
    }

    LE_INFO("%s: Migration of DM secret key from secure storage to IoTKeystore", resultstr);
}
#endif

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
}
