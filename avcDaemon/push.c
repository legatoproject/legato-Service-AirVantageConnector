/**
 * @file push.c
 *
 * Implementation of push mechanism
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "push.h"

#include <lwm2mcore/lwm2mcore.h>

//--------------------------------------------------------------------------------------------------
/**
 * Push data memory pool.  Initialized in push_Init().
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t PushDataPoolRef = NULL;


//--------------------------------------------------------------------------------------------------
/**
 * List of data push.  Initialized in push_Init().
 */
//--------------------------------------------------------------------------------------------------
static le_dls_List_t PushDataList;


//--------------------------------------------------------------------------------------------------
/**
 * Returns if data is currently being pushed to the server.
 */
//--------------------------------------------------------------------------------------------------
static bool IsPushing = false;


//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of items queued for push.
 * The number 10 is used because it takes max memory limit / MAX_CBOR_BUFFER_NUMBYTES
 */
//--------------------------------------------------------------------------------------------------
#define MAX_PUSH_QUEUE 10


//--------------------------------------------------------------------------------------------------
/**
 * Content contained in data being pushed
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint16_t mid;
    uint8_t buffer[4096];
    size_t bufferLength;
    lwm2mcore_PushContent_t contentType;
    bool isSent;
    le_avdata_CallbackResultFunc_t handlerPtr;
    void* callbackContextPtr;
    le_dls_Link_t link;
}
PushData_t;


//--------------------------------------------------------------------------------------------------
/**
 * Returns if the service is busy pushing data or will be pushing another set of data
 */
//--------------------------------------------------------------------------------------------------
bool IsPushBusy
(
    void
)
{
    size_t pushQueueLength = le_dls_NumLinks(&PushDataList);
    if (IsPushing && (pushQueueLength > 0))
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Handles ACK returned for every data pushed
 */
//--------------------------------------------------------------------------------------------------
static void PushCallBackHandler
(
    lwm2mcore_AckResult_t result,
    uint16_t mid
)
{
    LE_INFO("Push callback mid: %d", mid);
    le_avdata_PushStatus_t status;

    if (result == LWM2MCORE_ACK_RECEIVED)
    {
        status = LE_AVDATA_PUSH_SUCCESS;
    }
    else
    {
        status = LE_AVDATA_PUSH_FAILED;
    }

    le_dls_Link_t* linkPtr = le_dls_Peek(&PushDataList);

    // Return callback with associated message id
    while (linkPtr != NULL)
    {
        PushData_t* pDataPtr = CONTAINER_OF(linkPtr, PushData_t, link);
        if (pDataPtr->mid == mid)
        {
            le_avdata_CallbackResultFunc_t handlerPtr = pDataPtr->handlerPtr;
            if (handlerPtr != NULL)
            {
                handlerPtr(status, pDataPtr->callbackContextPtr);
            }
            le_dls_Remove(&PushDataList, linkPtr);
            le_mem_Release(pDataPtr);
            linkPtr = NULL;
            IsPushing = false;
            break;
        }

        linkPtr = le_dls_PeekNext(&PushDataList, linkPtr);
    }

    // Try sending the next queued item
    linkPtr = NULL;
    linkPtr = le_dls_Peek(&PushDataList);

    while (linkPtr != NULL)
    {
        PushData_t* pDataPtr = CONTAINER_OF(linkPtr, PushData_t, link);

        if (!pDataPtr->isSent)
        {
            uint16_t mid;
            le_result_t result;
            result = avcClient_Push(pDataPtr->buffer,
                                    pDataPtr->bufferLength,
                                    pDataPtr->contentType,
                                    &mid);

            // Send was successful, otherwise we need to keep it in the queue until next try
            if (result == LE_OK)
            {
                pDataPtr->mid = mid;
                IsPushing = true;
            }

            break;
        }

        linkPtr = le_dls_PeekNext(&PushDataList, linkPtr);
    }
}


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
le_result_t PushBuffer
(
    uint8_t* bufferPtr,
    size_t bufferLength,
    lwm2mcore_PushContent_t contentType,
    le_avdata_CallbackResultFunc_t handlerPtr,
    void* contextPtr
)
{
    uint16_t mid;
    le_result_t result;

    if (le_dls_NumLinks(&PushDataList) >= MAX_PUSH_QUEUE)
    {
        return LE_NOT_POSSIBLE;
    }

    result = avcClient_Push(bufferPtr, bufferLength, contentType, &mid);

    if (result != LE_FAULT)
    {
        PushData_t* pDataPtr = le_mem_ForceAlloc(PushDataPoolRef);

        if (result == LE_OK)
        {
            LE_DEBUG("Data has been pushed.");
            pDataPtr->mid = mid;
            pDataPtr->isSent = true;
            IsPushing = true;
        }
        else
        {
            LE_DEBUG("Data has been queued.");
            pDataPtr->isSent = false;
            memcpy(pDataPtr->buffer, bufferPtr, bufferLength);
            pDataPtr->bufferLength = bufferLength;
        }

        pDataPtr->handlerPtr = handlerPtr;
        pDataPtr->callbackContextPtr = contextPtr;
        pDataPtr->contentType = contentType;
        pDataPtr->link = LE_DLS_LINK_INIT;
        le_dls_Queue(&PushDataList, &pDataPtr->link);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Init push subcomponent
 */
//--------------------------------------------------------------------------------------------------
le_result_t push_Init
(
    void
)
{
    PushDataPoolRef = le_mem_CreatePool("Push record pool", sizeof(PushData_t));
    PushDataList = LE_DLS_LIST_INIT;

    // Set the push callback handler
    lwm2mcore_SetPushCallback(PushCallBackHandler);

    return LE_OK;
}
