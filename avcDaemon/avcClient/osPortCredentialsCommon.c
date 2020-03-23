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
#include "avcClient.h"
#include "avcFs/avcFsConfig.h"
#include "avcFs/avcFs.h"
#include "packageDownloader/sslUtilities.h"

//--------------------------------------------------------------------------------------------------
/**
 * Object 10243, certificate max size
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_CERT_MAX_SIZE     4000

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

    int pemLen = ssl_LayOutPEM(certPtr, len);
    if (-1 == pemLen)
    {
        LE_ERROR("ssl_LayOutPEM failed");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    result = WriteFs(SSLCERT_PATH, (uint8_t*)certPtr, pemLen);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to update certificate file");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
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
