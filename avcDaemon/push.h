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
 * Init push subcomponent
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t push_Init
(
    void
);
