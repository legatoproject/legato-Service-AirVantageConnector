/**
 * @file tpfServer.c
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "tpfServer.h"
#include "updateInfo.h"
#include "le_print.h"
#include "avcFsConfig.h"
#include "avcClient.h"
#include "workspace.h"


//--------------------------------------------------------------------------------------------------
/**
 * Set TPF mode state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t tpfServer_SetTpfState
(
    bool isTpfEnabled                     ///< [IN] is TPF mode enable
)
{
    le_result_t result;
    LE_DEBUG("tpfServer_SetTpfstate to %d", isTpfEnabled);

    result = WriteFs(TPF_SERVER_LEFS_DIR, (uint8_t*)&isTpfEnabled, sizeof(bool));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", TPF_SERVER_LEFS_DIR, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get TPF mode state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t tpfServer_GetTpfState
(
    bool* isTpfEnabledPtr                   ///< [OUT] true if third party FOTA service is activated
)
{
    size_t size;
    bool isTpfEnabled = false;
    le_result_t result;

    if (!isTpfEnabledPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_BAD_PARAMETER;
    }

    size = sizeof(bool);
    result = ReadFs(TPF_SERVER_LEFS_DIR, (uint8_t*)&isTpfEnabled, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_WARN("FW update install pending not found");
            *isTpfEnabledPtr = false;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", TPF_SERVER_LEFS_DIR, LE_RESULT_TXT(result));
        return result;
    }
    LE_DEBUG("the tpf server state is %d", isTpfEnabled);
    *isTpfEnabledPtr = isTpfEnabled;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set package URL
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SetPkgUri
(
    const char* url                     ///< [IN] is TPF mode enable
)
{
    le_result_t result;
    LE_DEBUG("rpdServer_SetTpfUrl to %s", url);
    size_t len = strlen(url);
    result = WriteFs(TPF_SERVER_URL_DIR, (uint8_t*)url, len);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", TPF_SERVER_URL_DIR, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get package URL
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_tpf_GetPackageUri
(
    char* uriPtr,                     ///< [OUT] Package address
    size_t packageUriSize             ///< [IN] URI max size
)
{
    le_result_t result;

    if (!uriPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_BAD_PARAMETER;
    }

    // Initialize URI buffer
    memset(uriPtr, 0, packageUriSize);

    result = ReadFs(TPF_SERVER_URL_DIR, (uint8_t*)uriPtr, &packageUriSize);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_WARN("Package URI not found");
            return LE_FAULT;
        }
        LE_ERROR("Failed to read %s: %s", TPF_SERVER_URL_DIR, LE_RESULT_TXT(result));
        return result;
    }

    LE_DEBUG("Package URI: %s", uriPtr);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start a package download from a 3rd party server
 *
 * This will sent a request to the server to start a download.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_tpf_Start
(
    void
)
{
    le_result_t res ;
    res = tpfServer_SetTpfState(true);
    if (res != LE_OK)
    {
        return LE_FAULT;
    }
    char bufferPtr[LE_TPF_URI_PACKAGE_MAX_SIZE];
    size_t len = LE_TPF_URI_PACKAGE_MAX_SIZE;
    if (LE_OK != le_tpf_GetPackageUri(bufferPtr, len))
    {
        LE_ERROR("ERROR on reading the FS to get the URL of package");
        tpfServer_SetTpfState(false);
        return LE_FAULT;
    }

    res = avcClient_Connect();
    if (res != LE_OK)
    {
        tpfServer_SetTpfState(false);
    }
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set package URI for download from 3rd party server.
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if failed to configure the package URI.
 *      - LE_BAD_PARAMETER if the given URI or port number are not in the right format
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_tpf_SetPackageUri
(
const char* packageUri             ///< [IN] The adresse of the package
)
{
    if( LE_OK != SetPkgUri(packageUri))
    {
        LE_ERROR("Error on saving workspace");
        return LE_FAULT;
    }
    LE_INFO("Stored uri %s", packageUri);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *
 * Get the current state of TPF service.
 *
 * @return
 *      - true if TPF service is enable .
 *      - false if TPF service is disable .
 */
//--------------------------------------------------------------------------------------------------
bool le_tpf_IsTpfStarted
(
    void
)
{
    bool state = false;
    return ((LE_OK == tpfServer_GetTpfState(&state)) && (state));
}
