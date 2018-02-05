//--------------------------------------------------------------------------------------------------
/**
 * @file main.c
 *
 * This component is used for testing the AirVantage request session feature.
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

#include "le_print.h"


static le_avdata_RequestSessionObjRef_t SessionReqRef;


//--------------------------------------------------------------------------------------------------
/**
 * Receives notification from avdata about session state.
 */
//--------------------------------------------------------------------------------------------------
static void SessionHandler
(
    le_avdata_SessionState_t sessionState,
    void* contextPtr
)
{
    if (sessionState == LE_AVDATA_SESSION_STARTED)
    {
        LE_INFO("Airvantage session started.");
    }
    else
    {
        LE_INFO("Airvantage session stopped.");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Init the component
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    le_avdata_AddSessionStateHandler(SessionHandler, NULL);

    SessionReqRef = le_avdata_RequestSession();
}