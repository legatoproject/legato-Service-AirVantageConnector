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
 * Starts a periodic connection attempt to the AirVantage server.
 *
 * @note - After a user-initiated call, this function registers itself inside a timer expiry handler
 *         to perform retries. On connection success, this function deinitializes the timer.
 *       - If this function is called when another connection is in the middle of being initiated
 *         or when the device is authenticating then LE_BUSY will be returned.
 *
 * @return
 *      - LE_OK if connection request has been sent.
 *      - LE_BUSY if currently retrying or authenticating.
 *      - LE_DUPLICATE if already connected to AirVantage server.
*/
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Connect
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * LwM2M client entry point to close a connection.
 *
 * @return
 *      - LE_OK in case of success.
 *      - LE_FAULT in case of failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Disconnect
(
    bool resetRetry  ///< [IN] if true, reset the retry timers.
);

//--------------------------------------------------------------------------------------------------
/**
 * LwM2M client entry point to send a registration update.
 *
 * @return
 *      - LE_OK in case of success.
 *      - LE_UNAVAILABLE when session closed.
 *      - LE_FAULT in case of failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Update
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * LwM2M client entry point to push data.
 *
 * @return
 *      - LE_OK in case of success.
 *      - LE_BUSY if busy pushing data.
 *      - LE_FAULT in case of failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcClient_Push
(
    uint8_t* payload,                       ///< [IN] Payload to push.
    size_t payloadLength,                   ///< [IN] Payload length.
    lwm2mcore_PushContent_t contentType,    ///< [IN] Content type.
    uint16_t* midPtr                        ///< [OUT] Message identifier.
);

//--------------------------------------------------------------------------------------------------
/**
 * Notify LwM2M of supported object instance list for software and asset data.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_SendList
(
    char* lwm2mObjListPtr,      ///< [IN] Object instances list.
    size_t objListLen           ///< [IN] List length.
);

//--------------------------------------------------------------------------------------------------
/**
 * Returns the instance reference of this client.
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Ref_t avcClient_GetInstance
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * LwM2M client entry point to get session status.
 *
 * @return
 *      - LE_AVC_DM_SESSION when the device is connected to the DM server.
 *      - LE_AVC_BOOTSTRAP_SESSION when the device is connected to the BS server.
 *      - LE_AVC_SESSION_INVALID in other cases.
 */
//--------------------------------------------------------------------------------------------------
le_avc_SessionType_t avcClient_GetSessionType
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function sets up the activity timer.
 *
 * @note The timeout will default to DEFAULT_ACTIVITY_TIMER if user defined value is less or equal
 * to 0.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_SetActivityTimeout
(
    int timeout               ///< [IN] Timeout for activity timer.
);

//--------------------------------------------------------------------------------------------------
/**
 * Start a timer to monitor the activity between device and server
 */
//--------------------------------------------------------------------------------------------------
void avcClient_StartActivityTimer
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Stop a timer to monitor the activity between device and server
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
 * Checks whether retry timer is active
 */
//--------------------------------------------------------------------------------------------------
bool avcClient_IsRetryTimerActive
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the AVC client sub-component.
 *
 * @note This function should be called during the initializaion phase of the AVC daemon.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_Init
(
   void
);

#endif // LEGATO_AVC_CLIENT_INCLUDE_GUARD
