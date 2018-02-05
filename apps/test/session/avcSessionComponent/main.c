//--------------------------------------------------------------------------------------------------
/**
 * @file main.c
 *
 * This component is used for testing the AirVantage session feature
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

//-------------------------------------------------------------------------------------------------
/**
 * Status handler for avcService updates
 */
//-------------------------------------------------------------------------------------------------
static void StatusHandler
(
    le_avc_Status_t updateStatus,
    int32_t totalNumBytes,
    int32_t downloadProgress,
    void* contextPtr
)
{
    switch (updateStatus)
    {
        case LE_AVC_SESSION_STARTED:
            LE_INFO("Session has been started, attempting to close connection.");
            LE_ASSERT(le_avc_StopSession() == LE_OK);
            break;

        case LE_AVC_SESSION_STOPPED:
            LE_INFO("Session has been stopped, attempting to close again.");
            LE_ASSERT(le_avc_StopSession() == LE_DUPLICATE);
            break;
        default:    // don't care about the other notifications
            break;
    }

}


//--------------------------------------------------------------------------------------------------
/**
 * Init the component
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("hi");
    le_avc_AddStatusEventHandler(StatusHandler, NULL);

    // Start avc session
    LE_ASSERT(le_avc_StartSession() == LE_OK);
}