/**
 * @file avc_portSecurity.c
 *
 * Porting layer for credential management
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "lwm2mcorePortSecurity.h"
#include "pa_avc.h"

//--------------------------------------------------------------------------------------------------
/**
 *                  OBJECT 0: SECURITY
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Retrieve a credential
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
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
    lwm2mcore_credentials_t credId,         ///< [IN] credential Id of credential to be retreived
    char *bufferPtr,                        ///< [INOUT] data buffer
    size_t *lenPtr                          ///< [INOUT] length of input buffer and length of the
                                            ///< returned data
)
{
    lwm2mcore_sid_t result = LWM2MCORE_ERR_OP_NOT_SUPPORTED;

    if ((bufferPtr == NULL) || (lenPtr == NULL) || (credId >= LWM2MCORE_CREDENTIAL_MAX))
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        le_result_t paResult = pa_avc_GetCredential(credId, bufferPtr, lenPtr);
        if (LE_OK == paResult)
        {
            result = LWM2MCORE_ERR_COMPLETED_OK;
            LE_INFO("os_portSecurityGetCredential bufferPtr %d lenPtr %d", bufferPtr, *lenPtr);
        }
        else
        {
            result = LWM2MCORE_ERR_GENERAL_ERROR;
        }
    }
    LE_INFO("os_portSecurityGetCredential credId %d result %d", credId, result);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set a credential
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
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
    char *bufferPtr,                        ///< [INOUT] data buffer
    size_t len                              ///< [IN] length of input buffer and length of the
                                            ///< returned data
)
{
    lwm2mcore_sid_t result = LWM2MCORE_ERR_OP_NOT_SUPPORTED;

    if ((bufferPtr == NULL) || (!len) || (credId >= LWM2MCORE_CREDENTIAL_MAX))
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        le_result_t paResult = pa_avc_SetCredential(credId, bufferPtr, len);
        if (LE_OK == paResult)
        {
            result = LWM2MCORE_ERR_COMPLETED_OK;
        }
        else
        {
            result = LWM2MCORE_ERR_GENERAL_ERROR;
        }
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to check if one credential is present in platform storage
 *
 * @return
 *      - true if the credential is present
 *      - false else
 *
 */
//--------------------------------------------------------------------------------------------------
static bool CredentialCheckPresence
(
    lwm2mcore_credentials_t credId      ///< [IN] Credential identifier
)
{
    bool result = false;
    le_result_t paResult;
    size_t size = 0;

    paResult = pa_avc_GetCredentialLength((uint8_t)credId, &size);

    if ((LE_OK == paResult) && size)
    {
        result = true;
    }

    LE_INFO("Credential presence: credId %d result %d", credId, result);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to check if a Device Management server credential was provided
 *
 * @note This API is called by LWM2MCore
 *
 * @return
 *      - true if  a Device Management server was provided
 *      - false else
 */
//--------------------------------------------------------------------------------------------------
bool os_portSecurityCheckDmCredentialsPresence
(
    void
)
{
    bool result = false;

    /* Check if credentials linked to DM server are present:
     * PSK Id, PSK secret, server URL
     */
    if (CredentialCheckPresence(LWM2MCORE_CREDENTIAL_DM_PUBLIC_KEY)
     && CredentialCheckPresence(LWM2MCORE_CREDENTIAL_DM_SECRET_KEY)
     && CredentialCheckPresence(LWM2MCORE_CREDENTIAL_DM_ADDRESS))
    {
        result = true;
    }
    LE_INFO("os_portSecurityDmServerPresence result %d", result);
    return result;
}

