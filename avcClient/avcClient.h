/**
 * @file avcClient.h
 *
 * Interface for AVC Client (for internal use only)
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef LEGATO_AVC_CLIENT_INCLUDE_GUARD
#define LEGATO_AVC_CLIENT_INCLUDE_GUARD

#include "legato.h"
#include "interfaces.h"
#include <lwm2mcore/lwm2mcore.h>

//--------------------------------------------------------------------------------------------------
/**
 * Connect to the server. Connection is retried according to the configured retry timers, if session
 * isn't started. If this function is called while one of the retry timers is running, retry isn't
 * performed and LE_BUSY is returned.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_DUPLICATE if already connected.
 *      - LE_BUSY if currently retrying.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Connect
(
    void
);

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
    bool resetRetry  ///< [IN] if true, reset the retry timers
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * LWM2M client entry point to push data
 *
 * @return
 *      - LE_OK in case of success
 *      - LE_BUSY if busy pushing data
 *      - LE_FAULT in case of failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Push
(
    uint8_t* payload,
    size_t payloadLength,
    lwm2mcore_PushContent_t contentType,
    uint16_t* midPtr
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Returns the instance reference of this client
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Ref_t avcClient_GetInstance
(
    void
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * This function sets up the activity timer. The timeout will default to 20 seconds if
 * user defined value doesn't exist or if the defined value is less than 0.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_SetActivityTimeout
(
    int timeout               ///< [IN] Timeout for activity timer
);

//--------------------------------------------------------------------------------------------------
/**
 * Start a timer to monitor the activity between device and server.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_StartActivityTimer
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Stop a timer to monitor the activity between device and server.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_StopActivityTimer
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Reset the retry timers by resetting the retrieved reset timer config, and stopping the current
 * retry timer.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_ResetRetryTimer
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialization function avcClient. Should be called only once.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_Init
(
   void
);

//--------------------------------------------------------------------------------------------------
/**
 * Stop a timer to monitor the activity between device and server.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_StopActivityTimer
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Start a timer to monitor the activity between device and server.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_StartActivityTimer
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function sets up the activity timer. The timeout will default to 20 seconds if
 * user defined value doesn't exist or if the defined value is less than 0.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_SetActivityTimeout
(
    int timeout               ///< [IN] Timeout for activity timer
);

#endif // LEGATO_AVC_CLIENT_INCLUDE_GUARD
