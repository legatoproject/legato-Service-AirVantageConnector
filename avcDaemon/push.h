/**
 * @file push.h
 *
 * Push mechanism interface
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/lwm2mcore.h>

//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of bytes for CBOR encoded data
 */
//--------------------------------------------------------------------------------------------------
#define MAX_CBOR_BUFFER_NUMBYTES 4096 // TODO: verify value


//--------------------------------------------------------------------------------------------------
/**
 * Returns if the service is busy pushing data or will be pushing another set of data
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool IsPushBusy
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Push buffer to the server
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BUSY           Data queued for push
 *  - LE_NOT_POSSIBLE   Data queue is full, try pushing data again later
 *  - LE_FAULT          On any other errors
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t PushBuffer
(
    uint8_t* bufferPtr,
    size_t bufferLength,
    lwm2mcore_PushContent_t contentType,
    le_avdata_CallbackResultFunc_t handlerPtr,
    void* contextPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Retry pushing items queued in the list after AV connection reset
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_NOT_FOUND      If nothing to be retried
 *  - LE_FAULT          On any other errors
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t push_Retry
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Init push subcomponent
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t push_Init
(
    void
);
