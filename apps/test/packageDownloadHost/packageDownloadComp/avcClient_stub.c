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
