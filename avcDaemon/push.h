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
 * Maximum number of items queued for push.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_PUSH_QUEUE 10

//--------------------------------------------------------------------------------------------------
/**
 * Maximum buffer allocated for all push operations.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_PUSH_BUFFER_BYTES (MAX_PUSH_QUEUE*AVDATA_PUSH_BUFFER_BYTES)


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
 *  - LE_BUSY           Push service is busy. Data added to queue list for later push
 *  - LE_OVERFLOW       Data size exceeds the maximum allowed size
 *  - LE_NO_MEMORY      Data queue is full, try pushing data again later
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
