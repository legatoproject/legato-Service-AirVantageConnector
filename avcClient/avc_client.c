/**
 * @file avc_client.c
 *
 * client of the LWM2M stack
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */

/* include files */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "legato.h"
#include "interfaces.h"
#include "lwm2mcore.h"
#include "osTimer.h"
#include "osPortSecurity.h"
#include "osPortTypes.h"
#include "pa_avc.h"


//--------------------------------------------------------------------------------------------------
/**
 * Static context for LWM2MCore
 */
//--------------------------------------------------------------------------------------------------
static int context = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Static data reference
 */
//--------------------------------------------------------------------------------------------------
le_data_RequestObjRef_t DataRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static data connection handler
 */
//--------------------------------------------------------------------------------------------------
le_data_ConnectionStateHandlerRef_t DataHandler;

//--------------------------------------------------------------------------------------------------
/**
 *  Call back registered in LWM2M client for bearer related events
 */
//--------------------------------------------------------------------------------------------------
void avc_bearerCb
(
    bool connected,     ///< [IN] Indicates if the bearer is connected or disconnected
    void* contextPtr    ///< [IN] User data
)
{
    LE_INFO( "bearerOpenCb connected %d", connected);
    if (connected)
    {
        char endpointPtr[LWM2MCORE_ENDPOINT_LEN];
        bool result = false;
        le_result_t lresult = LE_FAULT;

        /* Register objects to LWM2M and set the device endpoint:
         * - endpoint shall be unique for each client: IMEI/ESN/MEID
         * - the number of objects we will be passing through and the objects array
         */

        /* Get the device endpoint: IMEI */
        memset (endpointPtr, 0, LWM2MCORE_ENDPOINT_LEN);
        lresult = le_info_GetImei ((char*)endpointPtr, (uint32_t) LWM2MCORE_ENDPOINT_LEN);
        if (lresult != LE_OK)
        {
            LE_ERROR ("Error to retreive the device IMEI");
        }
        else
        {
            /* Register to the LWM2M agent */
            uint16_t objNumber = lwm2mcore_objectRegister (context,
                                                            endpointPtr,
                                                            NULL,
                                                            NULL);
            LE_INFO ("lwm2mcore_objectRegister %d", objNumber);

            if (!objNumber)
            {
                LE_ERROR ("ERROR in LWM2M obj reg");
            }
            else
            {
                result = lwm2mcore_connect (context);
                if (result != true)
                {
                    LE_ERROR ("connect error");
                }
            }
        }
    }
    else if (!connected)
    {
        /* The data connection is closed */
        lwm2mcore_free (context);
        context = 0;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback for the connection state
 */
//--------------------------------------------------------------------------------------------------
static void ConnectionStateHandler
(
    const char* intfNamePtr,    ///< [IN] Interface name
    bool connected,             ///< [IN] connection state (true = connected, else false)
    void* contextPtr            ///< [IN] User data
)
{
    if (connected)
    {
        LE_DEBUG ("Connected through interface '%s'", intfNamePtr);
    }
    else
    {
        LE_WARN ("Disconnected from data connection service");
    }
    /* Call the callback */
    avc_bearerCb (connected, contextPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback for the LWM2M events
 *
 * @return
 *      - 0 on success
 *      - negative value on failure.

 */
//--------------------------------------------------------------------------------------------------
static int EventHandler
(
    lwm2mcore_status_t eventStatus              ///< [IN] event status
)
{
    int result = -1;

    switch (eventStatus.event)
    {
        case LWM2MCORE_EVENT_INITIALIZED:
        {
            LE_INFO ("LWM2MCORE_EVENT_INITIALIZED");
        }
        break;

        case LWM2MCORE_EVENT_AUTHENTICATION_STARTED:
        {
        }
        break;

        case LWM2MCORE_EVENT_AUTHENTICATION_FAILED:
        {
        }
        break;

        case LWM2MCORE_EVENT_SESSION_STARTED:
        {
        }
        break;

        case LWM2MCORE_EVENT_SESSION_FAILED:
        {
            /* TODO: check if current session is DM one */
            bool dmSession = false;
            if (lwm2mcore_connectionGetType (context, &dmSession) && dmSession)
            {
                /* Erase DM credentials to force a bootstrap session */
                pa_avc_CredentialDmErase();
            }
        }
        break;

        case LWM2MCORE_EVENT_SESSION_FINISHED:
        {
        }
        break;

        case LWM2MCORE_EVENT_LWM2M_SESSION_TYPE_START:
        {
            LE_INFO ("LWM2MCORE_EVENT_LWM2M_SESSION_TYPE_START");
            if (eventStatus.u.sessionType == LWM2MCORE_SESSION_BOOTSTRAP)
            {
                LE_INFO ("BOOTSTRAP");
            }
            else
            {
                LE_INFO ("DEVICE MANAGEMENT");
            }
        }
        break;

        default:
        {
            LE_ERROR ("bad event %d", eventStatus.event);
        }
        break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Connect to the server
 *
 * @return
 *      - true on success
 *      - false else
 */
//--------------------------------------------------------------------------------------------------
bool lwm2m_connect
(
    void
)
{
    bool result = false;
    LE_INFO ("lwm2m_connect context %d", context);

    if (!context)
    {
        context = lwm2mcore_init (EventHandler);

        //memset (&data, 0, sizeof(client_data_t));

        /* Initiatlize the bearer */
        /* Open a data connection */
        le_data_ConnectService();

        DataHandler = le_data_AddConnectionStateHandler (ConnectionStateHandler, NULL);

        /* Request data connection */
        DataRef = le_data_Request();
        if (DataRef != NULL)
        {
            result = true;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to close a connection
 *
 * @return
 *      - true on success
 *      - false else
 */
//--------------------------------------------------------------------------------------------------
bool lwm2m_disconnect
(
    void
)
{
    /* Always return true to keep historical behavior */
    bool result = true;
    LE_INFO ("lwm2m_disconnect");

    /* If the OS_TIMER_STEP timer is running, this means that a connection is active */
    if (os_timerIsRunning(OS_TIMER_STEP) == true)
    {
        result = lwm2mcore_disconnect(context);

        if (result == true)
        {
            /* stop the bearer */
            /* Check that a data connection was opened */
            if (DataRef != NULL)
            {
                /* Close the data connection */
                le_data_Release (DataRef);
            }
        }
    }

    return result;
}

