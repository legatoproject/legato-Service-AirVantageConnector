/**
 * @file avcClient.c
 *
 * client of the LWM2M stack
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

/* include files */
#include <stdbool.h>
#include <stdint.h>
#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/timer.h>
#include <lwm2mcore/security.h>

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Static context for LWM2MCore
 */
//--------------------------------------------------------------------------------------------------
static int Context = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Static data connection state for agent
 */
//--------------------------------------------------------------------------------------------------
static bool DataConnected = false;

//--------------------------------------------------------------------------------------------------
/**
 * Static data reference
 */
//--------------------------------------------------------------------------------------------------
static le_data_RequestObjRef_t DataRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static data connection handler
 */
//--------------------------------------------------------------------------------------------------
static le_data_ConnectionStateHandlerRef_t DataHandler;

//--------------------------------------------------------------------------------------------------
/**
 * Event ID on bootstrap connection failure.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t BsFailureEventId;

//--------------------------------------------------------------------------------------------------
/**
 *  Call back registered in LWM2M client for bearer related events
 */
//--------------------------------------------------------------------------------------------------
static void BearerEventCb
(
    bool connected,     ///< [IN] Indicates if the bearer is connected or disconnected
    void* contextPtr    ///< [IN] User data
)
{
    LE_INFO( "connected %d", connected);
    if (connected)
    {
        char endpointPtr[LWM2MCORE_ENDPOINT_LEN];
        bool result = false;

        /* Register objects to LWM2M and set the device endpoint:
         * - endpoint shall be unique for each client: IMEI/ESN/MEID
         * - the number of objects we will be passing through and the objects array
         */

        /* Get the device endpoint: IMEI */
        memset(endpointPtr, 0, LWM2MCORE_ENDPOINT_LEN);
        if (LE_OK != le_info_GetImei((char*)endpointPtr, (uint32_t) LWM2MCORE_ENDPOINT_LEN))
        {
            LE_ERROR("Error to retrieve the device IMEI");
        }
        else
        {
            /* Register to the LWM2M agent */
            if (!lwm2mcore_objectRegister(Context, endpointPtr, NULL, NULL))
            {
                LE_ERROR("ERROR in LWM2M obj reg");
            }
            else
            {
                result = lwm2mcore_Connect(Context);
                if (result != true)
                {
                    LE_ERROR("connect error");
                }
            }
        }
    }
    else
    {
        if (Context)
        {
            /* The data connection is closed */
            lwm2mcore_Free(Context);
            Context = 0;

            /* Remove the data handler */
            le_data_RemoveConnectionStateHandler(DataHandler);
        }
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
        LE_DEBUG("Connected through interface '%s'", intfNamePtr);
        DataConnected = true;
        /* Call the callback */
        BearerEventCb(connected, contextPtr);
    }
    else
    {
        LE_WARN("Disconnected from data connection service, current state %d", DataConnected);
        if (DataConnected)
        {
            /* Call the callback */
            BearerEventCb(connected, contextPtr);
            DataConnected = false;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback for the LWM2M events linked to package download and update
 *
 * @return
 *      - 0 on success
 *      - negative value on failure.

 */
//--------------------------------------------------------------------------------------------------
static int PackageEventHandler
(
    lwm2mcore_Status_t status              ///< [IN] event status
)
{
    int result = 0;

    switch (status.event)
    {
        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_DETAILS:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_PENDING, LE_AVC_FIRMWARE_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_PENDING, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_DOWNLOAD_PROGRESS:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_IN_PROGRESS, LE_AVC_FIRMWARE_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_IN_PROGRESS, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FINISHED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_COMPLETE, LE_AVC_FIRMWARE_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_COMPLETE, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else
            {
                LE_ERROR("Not yet supported package download type %d",
                         status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FAILED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_FAILED, LE_AVC_FIRMWARE_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_DOWNLOAD_FAILED, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_UPDATE_STARTED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_COMPLETE, LE_AVC_FIRMWARE_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_COMPLETE, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_UPDATE_FINISHED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_COMPLETE, LE_AVC_FIRMWARE_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_COMPLETE, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        case LWM2MCORE_EVENT_UPDATE_FAILED:
            if (LWM2MCORE_PKG_FW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_FAILED, LE_AVC_FIRMWARE_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else if (LWM2MCORE_PKG_SW == status.u.pkgStatus.pkgType)
            {
                avcServer_UpdateHandler(LE_AVC_INSTALL_FAILED, LE_AVC_APPLICATION_UPDATE,
                                        status.u.pkgStatus.numBytes, status.u.pkgStatus.progress,
                                        status.u.pkgStatus.errorCode);
            }
            else
            {
                LE_ERROR("Not yet supported package type %d", status.u.pkgStatus.pkgType);
            }
            break;

        default:
            if (LWM2MCORE_EVENT_LAST <= status.event)
            {
                LE_ERROR("unsupported event %d", status.event);
                result = -1;
            }
            break;
    }
    return result;
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
    lwm2mcore_Status_t status              ///< [IN] event status
)
{
    int result = 0;

    switch (status.event)
    {
        case LWM2MCORE_EVENT_SESSION_STARTED:
            LE_DEBUG("Session start");
            break;

        case LWM2MCORE_EVENT_SESSION_FAILED:
            LE_ERROR("Session failure");
            // If the device is connected to the bootstrap server, disconnect from server
            // If the device is connected to the DM server, a bootstrap connection will be
            // automatically initiated (session is not stopped)
            if (LE_AVC_BOOTSTRAP_SESSION == le_avc_GetSessionType())
            {
                LE_ERROR("Session failure on bootstrap server");
                le_event_Report(BsFailureEventId, NULL, 0);
            }
            break;

        case LWM2MCORE_EVENT_SESSION_FINISHED:
            LE_DEBUG("Session finished");
            avcServer_UpdateHandler(LE_AVC_SESSION_STOPPED, LE_AVC_UNKNOWN_UPDATE,
                                    0, 0, LE_AVC_ERR_NONE);
            break;

        case LWM2MCORE_EVENT_LWM2M_SESSION_TYPE_START:
            if (status.u.session.type == LWM2MCORE_SESSION_BOOTSTRAP)
            {
                LE_DEBUG("Connected to bootstrap");
            }
            else
            {
                LE_DEBUG("Connected to DM");
                avcServer_UpdateHandler(LE_AVC_SESSION_STARTED, LE_AVC_UNKNOWN_UPDATE,
                                        0, 0, LE_AVC_ERR_NONE);
            }
            break;

        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_DETAILS:
        case LWM2MCORE_EVENT_DOWNLOAD_PROGRESS:
        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FINISHED:
        case LWM2MCORE_EVENT_PACKAGE_DOWNLOAD_FAILED:
        case LWM2MCORE_EVENT_UPDATE_STARTED:
        case LWM2MCORE_EVENT_UPDATE_FINISHED:
        case LWM2MCORE_EVENT_UPDATE_FAILED:
            result = PackageEventHandler(status);
            break;

        default:
            if (LWM2MCORE_EVENT_LAST <= status.event)
            {
                LE_ERROR("unsupported event %d", status.event);
                result = -1;
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
 *      - LE_OK in case of success
 *      - LE_FAULT in case of failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Connect
(
    void
)
{
    le_result_t result = LE_FAULT;
    LE_DEBUG("Connect Context %d", Context);

    if (!Context)
    {
        Context = lwm2mcore_Init(EventHandler);

        /* Initialize the bearer */
        /* Open a data connection */
        le_data_ConnectService();

        DataHandler = le_data_AddConnectionStateHandler(ConnectionStateHandler, NULL);

        /* Request data connection */
        DataRef = le_data_Request();
        if (NULL != DataRef)
        {
            result = LE_OK;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to close a connection
 *
 * @return
 *      - LE_OK in case of success
 *      - LE_FAULT in case of failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Disconnect
(
    void
)
{
    LE_DEBUG("Disconnect");

    /* If the LWM2MCORE_TIMER_STEP timer is running, this means that a connection is active */
    if (true == lwm2mcore_TimerIsRunning(LWM2MCORE_TIMER_STEP))
    {
        if (true == lwm2mcore_Disconnect(Context))
        {
            /* stop the bearer */
            /* Check that a data connection was opened */
            if (NULL != DataRef)
            {
                /* Close the data connection */
                le_data_Release(DataRef);
            }
            /* The data connection is closed */
            lwm2mcore_Free(Context);
            Context = 0;

            /* Remove the data handler */
            le_data_RemoveConnectionStateHandler(DataHandler);

            return LE_OK;
        }
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to send a registration update
 *
 * @return
 *      - LE_OK in case of success
 *      - LE_FAULT in case of failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Update
(
    void
)
{
    LE_DEBUG("Registration update");

    if (true == lwm2mcore_Update(Context))
    {
        return LE_OK;
    }
    else
    {
        return LE_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to push data
 *
 * @return
 *      - LE_OK in case of success
 *      - LE_FAULT in case of failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Push
(
    uint8_t* payload,
    size_t payloadLength,
    void* callback

)
{
    LE_DEBUG("Push data");

    if (true == lwm2mcore_Push(Context, payload, payloadLength, callback))
    {
        LE_DEBUG("Push success");
        return LE_OK;
    }
    else
    {
        LE_INFO("Push failed");
        return LE_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Send instances of object 9 and the Legato objects for all currently installed applications.
 *
 */
//--------------------------------------------------------------------------------------------------
void avcClient_SendList
(
    char* lwm2mObjListPtr,      ///< [IN] Object instances list
    size_t objListLen           ///< [IN] List length
)
{
    lwm2mcore_UpdateSwList(Context, lwm2mObjListPtr, objListLen);
}

//--------------------------------------------------------------------------------------------------
/**
 * Returns the context of this client
 */
//--------------------------------------------------------------------------------------------------
int avcClient_GetContext
(
    void
)
{
    return Context;
}

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to get session status
 *
 * @return
 *      - LE_AVC_DM_SESSION when the device is connected to the DM server
 *      - LE_AVC_BOOTSTRAP_SESSION when the device is connected to the BS server
 *      - LE_AVC_SESSION_INVALID in other cases
 */
//--------------------------------------------------------------------------------------------------
le_avc_SessionType_t avcClient_GetSessionType
(
    void
)
{
    bool isDeviceManagement = false;

    if (lwm2mcore_ConnectionGetType(Context, &isDeviceManagement))
    {
        return (isDeviceManagement ? LE_AVC_DM_SESSION : LE_AVC_BOOTSTRAP_SESSION);
    }
    return LE_AVC_SESSION_INVALID;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Handler to terminate a connection to bootstrap on failure
 */
//--------------------------------------------------------------------------------------------------
void BsFailureHandler
(
    void* reportPtr
)
{
    lwm2mcore_Disconnect(Context);
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialization function avcClient. Should be called only once.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_Init
(
   void
)
{
    BsFailureEventId = le_event_CreateId("BsFailure", 0);
    le_event_AddHandler("BsFailureHandler", BsFailureEventId, BsFailureHandler);
}
