/**
 * This module implements some stubs for avData unit tests.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef _INTERFACES_H
#define _INTERFACES_H

#include "le_avdata_interface.h"
#include "le_avc_interface.h"
#include "le_cfg_interface.h"
#include "lwm2mcore.h"
#include "liblwm2m.h"

//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionRef_t le_avcData_GetClientSessionRef
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the server service reference
 */
//--------------------------------------------------------------------------------------------------
le_msg_ServiceRef_t le_avcData_GetServiceRef
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Registers a function to be called whenever one of this service's sessions is closed by
 * the client.  (STUBBED FUNCTION)
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionEventHandlerRef_t MyAddServiceCloseHandler
(
    le_msg_ServiceRef_t             serviceRef, ///< [IN] Reference to the service.
    le_msg_SessionEventHandler_t    handlerFunc,///< [IN] Handler function.
    void*                           contextPtr  ///< [IN] Opaque pointer value to pass to handler.
);

//--------------------------------------------------------------------------------------------------
/**
 * Add service open handler stub
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionEventHandlerRef_t MyAddServiceOpenHandler
(
    le_msg_ServiceRef_t               serviceRef,
    le_msg_SessionEventHandler_t      handlerFunc,
    void                              *contextPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Init this sub-component
 */
//--------------------------------------------------------------------------------------------------
le_result_t assetData_Init
(
    void
);

#endif /* interfaces.h */
