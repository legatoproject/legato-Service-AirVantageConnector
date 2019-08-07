/**
 * @file fileStreamClient_stub.c
 *
 * This file is a stubbed version of the fileStreamClient
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 *
 * Connect the current client thread to the service providing this API. Block until the service is
 * available.
 *
 * For each thread that wants to use this API, either ConnectService or TryConnectService must be
 * called before any other functions in this API.  Normally, ConnectService is automatically called
 * for the main thread, but not for any other thread. For details, see @ref apiFilesC_client.
 *
 * This function is created automatically.
 */
//--------------------------------------------------------------------------------------------------
void le_fileStreamClient_ConnectService
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 *
 * Disconnect the current client thread from the service providing this API.
 *
 * Normally, this function doesn't need to be called. After this function is called, there's no
 * longer a connection to the service, and the functions in this API can't be used. For details, see
 * @ref apiFilesC_client.
 *
 * This function is created automatically.
 */
//--------------------------------------------------------------------------------------------------
void le_fileStreamClient_DisconnectService
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 * Read the stream management object
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_fileStreamClient_GetStreamMgmtObject
(
    uint16_t                            instanceId,         ///< [IN] Intance Id of the file
    le_fileStreamClient_StreamMgmt_t*   streamMgmtObjPtr    ///< [OUT] Stream management object
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Write the stream management object
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT if failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_fileStreamClient_SetStreamMgmtObject
(
    const le_fileStreamClient_StreamMgmt_t* streamMgmtObjPtr    ///< [IN] Stream management object
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}