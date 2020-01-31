//--------------------------------------------------------------------------------------------------
/**
 * This file is a stubbed version of  the original avcClient.c which is the client of LWM2M stack
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "avcClient.h"

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to send a registration update
 *
 * @return
 *      - LE_OK in case of success
 *      - LE_UNAVAILABLE when session closed.
 *      - LE_FAULT in case of failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Update
(
    void
)
{
    LE_DEBUG("Stub");
    return LE_OK;
}
//--------------------------------------------------------------------------------------------------
/**
 * Start a timer to monitor the activity between device and server.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_StartActivityTimer
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop a timer to monitor the activity between device and server.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_StopActivityTimer
(
    void
)
{
    LE_DEBUG("Stub");
}

//--------------------------------------------------------------------------------------------------
/**
 * Query the AVC Server if it's okay to proceed with a server connection.
 *
 * For FOTA it should be called only after reboot, and for SOTA it should be called after the
 * update finishes. However, this function will request a connection to the server only if there
 * is no session going on.
 * If the connection can proceed right away, it will be launched.
 */
//--------------------------------------------------------------------------------------------------
void avcServer_QueryConnection
(
    le_avc_UpdateType_t        updateType,        ///< Update type
    le_avc_StatusHandlerFunc_t statusHandlerPtr,  ///< Pointer on handler function
    void*                      contextPtr         ///< Context
)
{
    LE_DEBUG("Stub");
}
