/**
 * @file timeseriesData.c
 *
 * Implementation of Time Series Interface
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"

#include "limit.h"
#include "timeseriesData.h"
#include "push.h"
#include "le_print.h"

#include "tinycbor/cbor.h"
#include "zlib.h"

//--------------------------------------------------------------------------------------------------
/**
* Record data pool.  Initialized in timeSeries_Init().
*/
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t RecordDataPoolRef = NULL;


//--------------------------------------------------------------------------------------------------
/**
* Timestamp data pool.  Initialized in timeSeries_Init().
*/
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t TimestampDataPoolRef = NULL;


//--------------------------------------------------------------------------------------------------
/**
* Resource data pool.  Initialized in timeSeries_Init().
*/
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t ResourceDataPoolRef = NULL;


//--------------------------------------------------------------------------------------------------
/**
* Data value pool.  Initialized in timeSeries_Init().
*/
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t DataValuePoolRef = NULL;


//--------------------------------------------------------------------------------------------------
/**
* String value pool.  Initialized in timeSeries_Init().
*/
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t StringValuePoolRef = NULL;


//--------------------------------------------------------------------------------------------------
/**
 * CBOR buffer memory pool.  Initialized in timeSeries_Init().
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t CborBufferPoolRef = NULL;


//--------------------------------------------------------------------------------------------------
/**
* Supported data types.  TODO: Share with asset data
*/
//--------------------------------------------------------------------------------------------------
typedef enum
{
    DATA_TYPE_NONE,    ///< Some fields do not have a data type, i.e. EXEC only fields
    DATA_TYPE_INT,
    DATA_TYPE_BOOL,
    DATA_TYPE_STRING,
    DATA_TYPE_FLOAT     ///< 64 bit floating point value
}
DataType_t;


//--------------------------------------------------------------------------------------------------
/**
* Data contained in time series
*/
//--------------------------------------------------------------------------------------------------
typedef struct le_avdata_Record
{
    le_dls_List_t timestampList;    ///< List of timestamps for this record
    le_dls_List_t resourceList;     ///< List of resources for this record

    uint8_t* bufferPtr;             ///< Buffer for accumulating history data.
    size_t bufferSize;              ///< Buffer size of history data.
    double timestampFactor;         ///< Factor of timestamp

    CborEncoder streamRef;          ///< CBOR encoded stream reference.
    CborEncoder mapRef;             ///< CBOR encoded map reference.
    CborEncoder headerArray;        ///< CBOR encoded header reference
    CborEncoder factorArray;        ///< CBOR encoded factor reference
    CborEncoder sampleArray;        ///< CBOR encoded sample data reference.

    bool isEncoded;
}
RecordData_t;


//--------------------------------------------------------------------------------------------------
/**
* Unique timestamps values accumulated
*/
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint64_t timestamp;                     ///< The timestamp
    le_dls_Link_t link;                     ///< For adding to the timestamp list
}
TimestampData_t;


//--------------------------------------------------------------------------------------------------
/**
* Data contained in a single resource of a timeseries record
*/
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char name[LE_AVDATA_PATH_NAME_BYTES];   ///< The name of the resource
    DataType_t type;                       ///< The type of the resource
    le_dls_List_t dataList;                 ///< List of data accumulated over time
    double factor;                          ///< Factor of data
    int32_t lastIntValue;                   ///< Last recorded int value
    double lastFloatValue;                  ///< Last recorded float value
    le_dls_Link_t link;                     ///< For adding to the resource list
}
ResourceData_t;


//--------------------------------------------------------------------------------------------------
/**
 * Supported data types
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint64_t timestamp;
    union
    {
        int intValue;
        double floatValue;
        bool boolValue;
        char* strValuePtr;
    };
    le_dls_Link_t link;
}
Data_t;


//--------------------------------------------------------------------------------------------------
/**
 * Get the number of unique timestamps in a timeseries record
 */
//--------------------------------------------------------------------------------------------------
size_t GetTimestampCount
(
    timeSeries_RecordRef_t recRef
)
{
    return le_dls_NumLinks(&recRef->timestampList);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the number of resources in a timeseries record
 */
//--------------------------------------------------------------------------------------------------
size_t GetResourceCount
(
    timeSeries_RecordRef_t recRef
)
{
    return le_dls_NumLinks(&recRef->resourceList);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the specified timestamp
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if resource does not exist
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetTimestamp
(
    timeSeries_RecordRef_t recRef,
    uint64_t timestamp,
    TimestampData_t** timestampPtrPtr
)
{
    if (!le_dls_IsEmpty(&recRef->timestampList))
    {
        le_dls_Link_t* linkPtr = le_dls_Peek(&recRef->timestampList);
        TimestampData_t* timestampPtr;

        // Loop through the resources
        while ( linkPtr != NULL )
        {
            timestampPtr = CONTAINER_OF(linkPtr, TimestampData_t, link);

            // if resource already exists but we are trying to accumulate value of a different type
            if (timestampPtr->timestamp == timestamp)
            {
                *timestampPtrPtr = timestampPtr;
                return LE_OK;
            }

            linkPtr = le_dls_PeekNext(&recRef->timestampList, linkPtr);
        }
    }

    return LE_NOT_FOUND;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get data with the corresponding timestamp
 *
 * @return:
 *      - NULL if data does not exist with the corresponding timestamp
 */
//--------------------------------------------------------------------------------------------------
static Data_t* GetTimestampData
(
    ResourceData_t* resourceDataPtr,
    uint64_t timestamp
)
{
    le_dls_Link_t* linkPtr = le_dls_Peek(&resourceDataPtr->dataList);
    Data_t* dataPtr;

    // Loop through all the data
    while ( linkPtr != NULL )
    {
        dataPtr = CONTAINER_OF(linkPtr, Data_t, link);

        if (dataPtr->timestamp == timestamp)
        {
            return dataPtr;
        }

        linkPtr = le_dls_PeekNext(&resourceDataPtr->dataList, linkPtr);
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the number of resources collected with a specific timestamp
 */
//--------------------------------------------------------------------------------------------------
size_t GetResourceDataTimestampCount
(
    timeSeries_RecordRef_t recRef,
    uint64_t timestamp
)
{
    int rdCount = 0;

    le_dls_Link_t* linkPtr = le_dls_Peek(&recRef->resourceList);
    ResourceData_t* resourceDataPtr;

    while ( linkPtr != NULL )
    {
        resourceDataPtr = CONTAINER_OF(linkPtr, ResourceData_t, link);
        if (GetTimestampData(resourceDataPtr, timestamp) != NULL)
        {
            rdCount++;
        }

        linkPtr = le_dls_PeekNext(&recRef->resourceList, linkPtr);
    }

    return rdCount;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add timestamp into sorted timestamp list
 */
//--------------------------------------------------------------------------------------------------
static void AddTimestamp
(
    timeSeries_RecordRef_t recRef,
    uint64_t timestamp
)
{
    TimestampData_t* timestampPtr;
    le_result_t result;

    result = GetTimestamp(recRef, timestamp, &timestampPtr);

    // allocate memory for this timestamp and add to timestamplist
    if (result == LE_NOT_FOUND)
    {
        timestampPtr = le_mem_ForceAlloc(TimestampDataPoolRef);
        timestampPtr->timestamp = timestamp;
        timestampPtr->link = LE_DLS_LINK_INIT;

        if (le_dls_IsEmpty(&recRef->timestampList))
        {
            le_dls_Queue(&recRef->timestampList, &timestampPtr->link);
        }
        else
        {
            le_dls_Link_t* linkPtr = le_dls_Peek(&recRef->timestampList);
            le_dls_Link_t* nextLinkPtr = NULL;

            while (linkPtr != NULL)
            {
                nextLinkPtr = le_dls_PeekNext(&recRef->timestampList, linkPtr);

                if (nextLinkPtr != NULL)
                {
                    TimestampData_t* nextTimestampPtr;
                    nextTimestampPtr = CONTAINER_OF(nextLinkPtr, TimestampData_t, link);

                    // add value before next timestamp
                    if (timestamp < nextTimestampPtr->timestamp)
                    {
                        le_dls_AddBefore(&recRef->timestampList, nextLinkPtr, &timestampPtr->link);
                        break;
                    }

                    linkPtr = le_dls_PeekNext(&recRef->timestampList, linkPtr);
                }
                else
                {
                    // queue timestamp if there's not next value to compare to
                    le_dls_Queue(&recRef->timestampList, &timestampPtr->link);
                    break;
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Clear all the timestamps of a record
 */
//--------------------------------------------------------------------------------------------------
static void ClearTimestamp
(
    timeSeries_RecordRef_t recRef
)
{
    le_dls_Link_t* linkPtr = le_dls_Pop(&recRef->timestampList);
    TimestampData_t* timestampPtr;

    // Go through each resource, delete the data and remove
    while ( linkPtr != NULL )
    {
        timestampPtr = CONTAINER_OF(linkPtr, TimestampData_t, link);
        le_mem_Release(timestampPtr);
        linkPtr = le_dls_Pop(&recRef->timestampList);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Clear all the resources of a record
 */
//--------------------------------------------------------------------------------------------------
static void ClearResources
(
    timeSeries_RecordRef_t recRef
)
{
    le_dls_Link_t* resourcelinkPtr = le_dls_Pop(&recRef->resourceList);
    ResourceData_t* resourceDataPtr;

    // Go through each resource, delete the data and remove
    while ( resourcelinkPtr != NULL )
    {
        resourceDataPtr = CONTAINER_OF(resourcelinkPtr, ResourceData_t, link);

        le_dls_Link_t* datalinkPtr = le_dls_Pop(&resourceDataPtr->dataList);
        Data_t* dataPtr;

        // Loop through all the data
        while ( datalinkPtr != NULL )
        {
            dataPtr = CONTAINER_OF(datalinkPtr, Data_t, link);

            if (resourceDataPtr->type == DATA_TYPE_STRING)
            {
                le_mem_Release(dataPtr->strValuePtr);
            }

            le_mem_Release(dataPtr);
            datalinkPtr = le_dls_Pop(&resourceDataPtr->dataList);
        }

        le_mem_Release(resourceDataPtr);
        resourcelinkPtr = le_dls_Pop(&recRef->resourceList);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Reset the last valid value stored
 */
//--------------------------------------------------------------------------------------------------
static void ResetResourceLastValue
(
    timeSeries_RecordRef_t recRef
)
{
    le_dls_Link_t* linkPtr = le_dls_Peek(&recRef->resourceList);
    ResourceData_t* resourceDataPtr;

    while ( linkPtr != NULL )
    {
        resourceDataPtr = CONTAINER_OF(linkPtr, ResourceData_t, link);
        resourceDataPtr->lastIntValue = 0;
        resourceDataPtr->lastFloatValue = 0;
        linkPtr = le_dls_PeekNext(&recRef->resourceList, linkPtr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete a specified timestamp
 */
//--------------------------------------------------------------------------------------------------
static void DeleteTimestamp
(
    timeSeries_RecordRef_t recRef,
    uint64_t timestamp
)
{
    LE_DEBUG("Deleting timestamp: %" PRIu64, timestamp);
    TimestampData_t* timestampPtr;
    le_dls_Link_t* linkPtr = le_dls_Peek(&recRef->timestampList);

    while ( linkPtr != NULL )
    {
        timestampPtr = CONTAINER_OF(linkPtr, TimestampData_t, link);

        if (timestampPtr->timestamp == timestamp)
        {
            le_mem_Release(timestampPtr);
            le_dls_Remove(&recRef->timestampList, linkPtr);
            linkPtr = NULL;
        }
        else
        {
            linkPtr = le_dls_PeekNext(&recRef->resourceList, linkPtr);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete a resource data
  * If no other data exists for this specific resource, resource will also be deleted.
 */
//--------------------------------------------------------------------------------------------------
static void DeleteResourceData
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    uint64_t timestamp
)
{
    le_dls_Link_t* linkPtr = le_dls_Peek(&recRef->resourceList);
    ResourceData_t* resourceDataPtr;

    // Go through each resource until you find this one and remove
    while ( linkPtr != NULL )
    {
        resourceDataPtr = CONTAINER_OF(linkPtr, ResourceData_t, link);

        if (0 == strcmp(path, resourceDataPtr->name))
        {
            Data_t* dataPtr = (Data_t*)GetTimestampData(resourceDataPtr, timestamp);

            // Delete this specific resource
            if (dataPtr != NULL)
            {
                LE_DEBUG("Deleting this resource data");
                // if string we need to deallocate memory for string as well
                if (resourceDataPtr->type == DATA_TYPE_STRING)
                {
                    le_mem_Release(dataPtr->strValuePtr);
                }

                le_mem_Release(dataPtr);
                le_dls_Remove(&resourceDataPtr->dataList, &dataPtr->link);

                // Delete this resource if this is the only data entry
                if (0 == le_dls_NumLinks(&resourceDataPtr->dataList))
                {
                    LE_DEBUG("Deleting the resource since no data");
                    le_mem_Release(resourceDataPtr);
                    le_dls_Remove(&recRef->resourceList, linkPtr);
                }
            }

            linkPtr = NULL;
        }
        else
        {
            linkPtr = le_dls_PeekNext(&recRef->resourceList, linkPtr);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete a data of a with a specific resource name and timestamp.
 * If no other data exists with this timestamp, timestamp will be deleted as well.
 */
//--------------------------------------------------------------------------------------------------
static void DeleteData
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    uint64_t timestamp
)
{
    DeleteResourceData(recRef, path, timestamp);

    // delete timestamp ref if there are no data associated with this timestamp
    if (0 == GetResourceDataTimestampCount(recRef, timestamp))
    {
        LE_DEBUG("Deleting timestamp ref since no data exists for this timestamp.");
        DeleteTimestamp(recRef, timestamp);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete a data of a with a specific resource name and timestamp.
 */
//--------------------------------------------------------------------------------------------------
static void ResetRecord
(
    timeSeries_RecordRef_t recRef
)
{
    ClearResources(recRef);
    ClearTimestamp(recRef);
    recRef->timestampFactor = 1;
    recRef->isEncoded = false;
}


//--------------------------------------------------------------------------------------------------
/**
 * Return the size of the encoded data
 */
//--------------------------------------------------------------------------------------------------
static size_t GetEncodedDataSize
(
    timeSeries_RecordRef_t recRef
)
{
    uint32_t cborStreamSize;

    if (!recRef->isEncoded)
    {
        cborStreamSize = 0;
    }
    else
    {
        cborStreamSize = cbor_encoder_get_buffer_size(&recRef->streamRef, recRef->bufferPtr);
    }

    return cborStreamSize;
}


//--------------------------------------------------------------------------------------------------
/**
 * Encode resource name to cbor sample array
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t EncodeResourceNameToCborArray
(
    timeSeries_RecordRef_t recRef
)
{
    CborError err;

    le_dls_Link_t* linkPtr = le_dls_Peek(&recRef->resourceList);
    ResourceData_t* resourceDataPtr;

    // Loop through the resources
    while ( linkPtr != NULL )
    {
        resourceDataPtr = CONTAINER_OF(linkPtr, ResourceData_t, link);
        err = cbor_encode_text_string(&recRef->headerArray,
                                      resourceDataPtr->name,
                                      strlen(resourceDataPtr->name));
        RETURN_IF_CBOR_ERROR(err);

        linkPtr = le_dls_PeekNext(&recRef->resourceList, linkPtr);
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Encode factors to cbor factor array
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t EncodeFactorToCborArray
(
    timeSeries_RecordRef_t recRef
)
{
    CborError err;

    err = cbor_encode_double(&recRef->factorArray, recRef->timestampFactor);
    RETURN_IF_CBOR_ERROR(err);

    le_dls_Link_t* linkPtr = le_dls_Peek(&recRef->resourceList);
    ResourceData_t* resourceDataPtr;

    // Loop through the timestamps
    while ( linkPtr != NULL )
    {
        resourceDataPtr = CONTAINER_OF(linkPtr, ResourceData_t, link);

        err = cbor_encode_double(&recRef->factorArray, resourceDataPtr->factor);
        RETURN_IF_CBOR_ERROR(err);

        linkPtr = le_dls_PeekNext(&recRef->resourceList, linkPtr);
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Encode a null value if none exist
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t EncodeResourceDefaultValue
(
    timeSeries_RecordRef_t recRef
)
{
    CborError err;

    err = cbor_encode_null(&recRef->sampleArray);
    RETURN_IF_CBOR_ERROR(err);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Encode delta value
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t EncodeResourceDeltaValue
(
    timeSeries_RecordRef_t recRef,
    ResourceData_t* resourceDataPtr,
    TimestampData_t* currentTimestampPtr,
    TimestampData_t* prevTimestampPtr
)
{
    CborError err;

    // get data with this timestamp from this resource
    Data_t* dataPtr = (Data_t*)GetTimestampData(resourceDataPtr,
                                                 currentTimestampPtr->timestamp);
    Data_t* prevDataPtr = NULL;

    int intDelta;
    double floatDelta;

    int prevIntValue;
    double prevFloatValue;

    if (!dataPtr)
    {
       LE_ERROR("dataPtr is NULL");
       return LE_FAULT;
    }

    if (prevTimestampPtr != NULL)
    {
        prevDataPtr = (Data_t*)GetTimestampData(resourceDataPtr, prevTimestampPtr->timestamp);
    }

    // delta value is only applicable to int and floats
    switch (resourceDataPtr->type)
    {
        case DATA_TYPE_INT:
            if (prevTimestampPtr == NULL)
            {
                intDelta = dataPtr->intValue * resourceDataPtr->factor;
            }
            else
            {
                if (prevDataPtr == NULL)
                {
                    prevIntValue = resourceDataPtr->lastIntValue;
                }
                else
                {
                    prevIntValue = prevDataPtr->intValue;
                }

                intDelta = (dataPtr->intValue - prevIntValue) * resourceDataPtr->factor;
            }

            resourceDataPtr->lastIntValue = dataPtr->intValue;
            err = cbor_encode_int(&recRef->sampleArray, intDelta);
            RETURN_IF_CBOR_ERROR(err);
            break;

        case DATA_TYPE_FLOAT:
            if (prevTimestampPtr == NULL)
            {
                floatDelta = dataPtr->floatValue;
            }
            else
            {
                if (prevDataPtr == NULL)
                {
                    prevFloatValue = resourceDataPtr->lastFloatValue;
                }
                else
                {
                    prevFloatValue = prevDataPtr->floatValue;
                }

                floatDelta = (dataPtr->floatValue - prevFloatValue) * resourceDataPtr->factor;
            }

            resourceDataPtr->lastFloatValue = dataPtr->floatValue;
            err = cbor_encode_double(&recRef->sampleArray, floatDelta);
            RETURN_IF_CBOR_ERROR(err);
            break;

        case DATA_TYPE_BOOL:
            err = cbor_encode_boolean(&recRef->sampleArray, dataPtr->boolValue);
            RETURN_IF_CBOR_ERROR(err);
            break;

        case DATA_TYPE_STRING:
            err = cbor_encode_text_string(&recRef->sampleArray,
                                          dataPtr->strValuePtr,
                                          strlen(dataPtr->strValuePtr));
            RETURN_IF_CBOR_ERROR(err);
            break;

        default:
            LE_INFO("Invalid type");
            break;
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add resource data to cbor stream array
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t EncodeResourceDataToCborArray
(
    timeSeries_RecordRef_t recRef
)
{
    CborError err;
    le_result_t result = LE_OK;

    le_dls_Link_t* tsLinkPtr = le_dls_Peek(&recRef->timestampList);
    TimestampData_t* timestampPtr;
    TimestampData_t* prevTimestampPtr = NULL;

    // Loop through the timestamps
    while ( tsLinkPtr != NULL )
    {
        timestampPtr = CONTAINER_OF(tsLinkPtr, TimestampData_t, link);

        // sample array starts with timestamp followed by resource data with this timestamp
        uint64_t timestamp;
        if (prevTimestampPtr == NULL)
        {
            ResetResourceLastValue(recRef);
            timestamp = timestampPtr->timestamp * recRef->timestampFactor;
        }
        else
        {
            uint64_t deltaTimestamp = ((timestampPtr->timestamp) - (prevTimestampPtr->timestamp));
            timestamp = deltaTimestamp * recRef->timestampFactor;
        }

        err = cbor_encode_uint(&recRef->sampleArray, timestamp);
        RETURN_IF_CBOR_ERROR(err);

        le_dls_Link_t* rdLinkPtr = le_dls_Peek(&recRef->resourceList);
        ResourceData_t* resourceDataPtr;

        // Loop through the resource data with this timestamp
        while ( rdLinkPtr != NULL )
        {
            resourceDataPtr = CONTAINER_OF(rdLinkPtr, ResourceData_t, link);

            if (GetTimestampData(resourceDataPtr, timestampPtr->timestamp) == NULL)
            {
                result = EncodeResourceDefaultValue(recRef);
            }
            else
            {
                result = EncodeResourceDeltaValue(recRef,
                                                  resourceDataPtr,
                                                  timestampPtr,
                                                  prevTimestampPtr);
            }

            rdLinkPtr = le_dls_PeekNext(&recRef->resourceList, rdLinkPtr);
        }

        prevTimestampPtr = timestampPtr;
        tsLinkPtr = le_dls_PeekNext(&recRef->timestampList, tsLinkPtr);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Encode the data accumulated
 *
 * @return:
 *      - LE_OK on success
 *      - LE_OVERFLOW if buffer is full
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t Encode
(
    timeSeries_RecordRef_t recRef
)
{
    CborError err;
    le_result_t result = LE_OK;

    // only encode if it hasn't been encoded
    if (false == recRef->isEncoded)
    {
        // clear buffer
        memset(recRef->bufferPtr, 0, recRef->bufferSize);

        // Initialize CBOR stream.
        cbor_encoder_init(&recRef->streamRef,
                          recRef->bufferPtr,
                          MAX_CBOR_BUFFER_NUMBYTES,
                          0);

        err = cbor_encoder_create_map(&recRef->streamRef,
                                      &recRef->mapRef,
                                      NUM_TIME_SERIES_MAPS);
        RETURN_IF_CBOR_ERROR(err);

        // Create a map and add the header in to the map.
        err = cbor_encode_text_stringz(&recRef->mapRef, "h");
        RETURN_IF_CBOR_ERROR(err);

        // Create an array for the header.
        err = cbor_encoder_create_array(&recRef->mapRef,
                                        &recRef->headerArray,
                                        GetResourceCount(recRef));
        RETURN_IF_CBOR_ERROR(err);

        // Encode resource names to header array
        result = EncodeResourceNameToCborArray(recRef);
        if (result != LE_OK)
        {
            return result;
        }

        // Close the header array i.e done with entering resource names
        cbor_encoder_close_container(&recRef->mapRef,
                                     &recRef->headerArray);


        // Create a map for factor.
        err = cbor_encode_text_stringz(&recRef->mapRef, "f");
        RETURN_IF_CBOR_ERROR(err);

        // size of factor array is number of resources + 1 to account for the timestamp factor
        size_t factorArraySize = GetResourceCount(recRef) + 1;

        // Create an array of factors (time stamp factor [1], data factor [n])
        err = cbor_encoder_create_array(&recRef->mapRef,
                                        &recRef->factorArray,
                                        factorArraySize);
        RETURN_IF_CBOR_ERROR(err);

        // Encode factor to factor array
        result = EncodeFactorToCborArray(recRef);
        if (result != LE_OK)
        {
            return result;
        }

        // Close the factor array i.e done with entering resource names
        cbor_encoder_close_container(&recRef->mapRef,
                                     &recRef->factorArray);


        // Create a map for samples.
        err = cbor_encode_text_stringz(&recRef->mapRef, "s");
        RETURN_IF_CBOR_ERROR(err);

        // size of sample array
        size_t sampleArraySize = (GetResourceCount(recRef) + 1) * GetTimestampCount(recRef);

        // Create an array of samples
        err = cbor_encoder_create_array(&recRef->mapRef,
                                        &recRef->sampleArray,
                                        sampleArraySize);

        // Encode resource data to sample array
        result = EncodeResourceDataToCborArray(recRef);
        if (result != LE_OK)
        {
            return result;
        }

        // Close the sample array
        cbor_encoder_close_container(&recRef->mapRef,
                                     &recRef->sampleArray);

        cbor_encoder_close_container(&recRef->streamRef,
                                     &recRef->mapRef);

        RETURN_IF_CBOR_ERROR(err);

        recRef->isEncoded = true;
    }

    LE_DEBUG("Encoded size: %zd", GetEncodedDataSize(recRef));
    LE_DUMP(recRef->bufferPtr, GetEncodedDataSize(recRef));

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Create a timeseries record
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t timeSeries_Create
(
    timeSeries_RecordRef_t* recRefPtr
)
{
    RecordData_t* recordDataPtr;

    recordDataPtr = le_mem_ForceAlloc(RecordDataPoolRef);
    recordDataPtr->timestampList = LE_DLS_LIST_INIT;
    recordDataPtr->resourceList = LE_DLS_LIST_INIT;
    recordDataPtr->bufferPtr = le_mem_ForceAlloc(CborBufferPoolRef);
    recordDataPtr->bufferSize = MAX_CBOR_BUFFER_NUMBYTES;
    recordDataPtr->timestampFactor = 1;
    recordDataPtr->isEncoded = false;
    *recRefPtr = recordDataPtr;

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete a timeseries record
 */
//--------------------------------------------------------------------------------------------------
void timeSeries_Delete
(
    timeSeries_RecordRef_t recRef
)
{
    ResetRecord(recRef);
    le_mem_Release(recRef->bufferPtr);
    le_mem_Release(recRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the specified resource from the given record
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT if resource type is incorrect
 *      - LE_NOT_FOUND if resource does not exist
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetResourceData
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    DataType_t type,
    ResourceData_t** rdataPtrPtr
)
{
    if (!le_dls_IsEmpty(&recRef->resourceList))
    {
        le_dls_Link_t* linkPtr = le_dls_Peek(&recRef->resourceList);
        ResourceData_t* resourcePtr;

        // Loop through the resources
        while ( linkPtr != NULL )
        {
            resourcePtr = CONTAINER_OF(linkPtr, ResourceData_t, link);

            // if resource already exists but we are trying to accumulate value of a different type
            if (0 == strcmp(resourcePtr->name, path))
            {
                if (resourcePtr->type != type)
                {
                    return LE_FAULT;
                }
                else
                {
                    *rdataPtrPtr = resourcePtr;
                    return LE_OK;
                }
            }
            linkPtr = le_dls_PeekNext(&recRef->resourceList, linkPtr);
        }
    }

    return LE_NOT_FOUND;
}


//--------------------------------------------------------------------------------------------------
/**
 * Create a specified resource under a record
 *
 * @return:
 *      - LE_OK on success
 *      - LE_OVERFLOW if resource specified is too long
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
static le_result_t CreateResourceData
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    DataType_t type
)
{
    LE_DEBUG("Creating resource: %s of type %d", path, type);

    ResourceData_t* resourceDataPtr = le_mem_ForceAlloc(ResourceDataPoolRef);

    if (le_utf8_Copy(resourceDataPtr->name, path, LE_AVDATA_PATH_NAME_BYTES, NULL) == LE_OVERFLOW)
    {
        return LE_OVERFLOW;
        le_mem_Release(resourceDataPtr);
    }

    resourceDataPtr->type = type;
    resourceDataPtr->dataList = LE_DLS_LIST_INIT;
    resourceDataPtr->link = LE_DLS_LINK_INIT;

    if ((type == DATA_TYPE_STRING) || (type == DATA_TYPE_BOOL))
    {
        resourceDataPtr->factor = 0;
    }
    else
    {
        resourceDataPtr->factor = 1;
        resourceDataPtr->lastIntValue = 0;
        resourceDataPtr->lastFloatValue = 0;
    }

    le_dls_Queue(&recRef->resourceList, &resourceDataPtr->link);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add the integer value for the specified resource
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AddIntResourceData
(
    timeSeries_RecordRef_t recRef,
    ResourceData_t* rdataPtr,
    int32_t value,
    uint64_t timestamp
)
{
    le_result_t result;

    Data_t* dataPtr = (Data_t*)GetTimestampData(rdataPtr, timestamp);

    // if existing timestamp is used, update value
    if (dataPtr != NULL)
    {
        dataPtr->intValue = value;
    }
    else
    {
        dataPtr = le_mem_ForceAlloc(DataValuePoolRef);
        dataPtr->timestamp = timestamp;
        dataPtr->intValue = value;
        dataPtr->link = LE_DLS_LINK_INIT;
        le_dls_Queue(&rdataPtr->dataList, &dataPtr->link);
    }

    // new entry, we need re-encode
    recRef->isEncoded = false;
    result = Encode(recRef);

    // if our buffer cannot fit this new added data, remove it
    if (result == LE_NO_MEMORY)
    {
        DeleteData(recRef, rdataPtr->name, timestamp);
        recRef->isEncoded = false;
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add the float value for the specified resource
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AddFloatResourceData
(
    timeSeries_RecordRef_t recRef,
    ResourceData_t* rdataPtr,
    double value,
    uint64_t timestamp
)
{
    le_result_t result;

    Data_t* dataPtr = (Data_t*)GetTimestampData(rdataPtr, timestamp);

    // if existing timestamp is used, update value
    if (dataPtr != NULL)
    {
        dataPtr->floatValue = value;
    }
    else
    {
        dataPtr = le_mem_ForceAlloc(DataValuePoolRef);
        dataPtr->timestamp = timestamp;
        dataPtr->floatValue = value;
        dataPtr->link = LE_DLS_LINK_INIT;
        le_dls_Queue(&rdataPtr->dataList, &dataPtr->link);
    }

    // new entry, we need re-encode
    recRef->isEncoded = false;
    result = Encode(recRef);

    // if our buffer cannot fit this new added data, remove it
    if (result == LE_NO_MEMORY)
    {
        DeleteData(recRef, rdataPtr->name, timestamp);
        recRef->isEncoded = false;
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add the boolean value for the specified resource
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AddBoolResourceData
(
    timeSeries_RecordRef_t recRef,
    ResourceData_t* rdataPtr,
    bool value,
    uint64_t timestamp
)
{
    le_result_t result;

    Data_t* dataPtr = (Data_t*)GetTimestampData(rdataPtr, timestamp);

    // if existing timestamp is used, update value
    if (dataPtr != NULL)
    {
        dataPtr->boolValue = value;
    }
    else
    {
        dataPtr = le_mem_ForceAlloc(DataValuePoolRef);
        dataPtr->timestamp = timestamp;
        dataPtr->boolValue = value;
        dataPtr->link = LE_DLS_LINK_INIT;
        le_dls_Queue(&rdataPtr->dataList, &dataPtr->link);
    }

    // new entry, we need re-encode
    recRef->isEncoded = false;
    result = Encode(recRef);

    // if our buffer cannot fit this new added data, remove it
    if (result == LE_NO_MEMORY)
    {
        DeleteData(recRef, rdataPtr->name, timestamp);
        recRef->isEncoded = false;
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add the string value for the specified resource
 */
//--------------------------------------------------------------------------------------------------
static le_result_t AddStringResourceData
(
    timeSeries_RecordRef_t recRef,
    ResourceData_t* rdataPtr,
    const char* value,
    uint64_t timestamp
)
{
    le_result_t result;

    Data_t* dataPtr = (Data_t*)GetTimestampData(rdataPtr, timestamp);

    // if existing timestamp is used, update value
    if (dataPtr != NULL)
    {
        le_utf8_Copy(dataPtr->strValuePtr, value, LE_AVDATA_STRING_VALUE_BYTES, NULL);
    }
    else
    {
        dataPtr = le_mem_ForceAlloc(DataValuePoolRef);
        dataPtr->timestamp = timestamp;
        dataPtr->strValuePtr = le_mem_ForceAlloc(StringValuePoolRef);
        // TODO: handle case when string value is too long
        le_utf8_Copy(dataPtr->strValuePtr, value, LE_AVDATA_STRING_VALUE_BYTES, NULL);
        dataPtr->link = LE_DLS_LINK_INIT;
        le_dls_Queue(&rdataPtr->dataList, &dataPtr->link);
    }

    // new entry, we need re-encode
    recRef->isEncoded = false;
    result = Encode(recRef);

    // if our buffer cannot fit this new added data, remove it
    if (result == LE_NO_MEMORY)
    {
        DeleteData(recRef, rdataPtr->name, timestamp);
        recRef->isEncoded = false;
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add the integer value for the specified resource
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NO_MEMORY if the current entry was NOT added because the time series buffer is full.
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t timeSeries_AddInt
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    int32_t value,
    uint64_t timestamp
)
{
    le_result_t result;
    ResourceData_t* resourceDataPtr;

    result = GetResourceData(recRef, path, DATA_TYPE_INT, &resourceDataPtr);

    // create or add resource data
    if (result != LE_FAULT)
    {
        AddTimestamp(recRef, timestamp);

        // resource data does not exists
        if (result == LE_NOT_FOUND)
        {
            result = CreateResourceData(recRef, path, DATA_TYPE_INT);

            if (result != LE_OK)
            {
                return result;
            }

            result = GetResourceData(recRef, path, DATA_TYPE_INT, &resourceDataPtr);

            // if creating it and we still cannot the resource, there is an issue
            if (result != LE_OK)
            {
                return result;
            }
        }

        result = AddIntResourceData(recRef, resourceDataPtr, value, timestamp);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add the float value for the specified resource
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NO_MEMORY if the current entry was NOT added because the time series buffer is full.
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t timeSeries_AddFloat
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    double value,
    uint64_t timestamp
)
{
    le_result_t result;
    ResourceData_t* resourceDataPtr;

    result = GetResourceData(recRef, path, DATA_TYPE_FLOAT, &resourceDataPtr);

    // cmust be ok or not found
    if (result != LE_FAULT)
    {
        AddTimestamp(recRef, timestamp);

        // resource data does not exists
        if (result == LE_NOT_FOUND)
        {
            result = CreateResourceData(recRef, path, DATA_TYPE_FLOAT);

            if (result != LE_OK)
            {
                return result;
            }

            result = GetResourceData(recRef, path, DATA_TYPE_FLOAT, &resourceDataPtr);

            // if creating it and we still cannot the resource, there is an issue
            if (result != LE_OK)
            {
                return result;
            }
        }

        result = AddFloatResourceData(recRef, resourceDataPtr, value, timestamp);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add the boolean value for the specified resource
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NO_MEMORY if the current entry was NOT added because the time series buffer is full.
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t timeSeries_AddBool
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    bool value,
    uint64_t timestamp
)
{
    le_result_t result;
    ResourceData_t* resourceDataPtr;

    result = GetResourceData(recRef, path, DATA_TYPE_BOOL, &resourceDataPtr);

    // cmust be ok or not found
    if (result != LE_FAULT)
    {
        AddTimestamp(recRef, timestamp);

        // resource data does not exists
        if (result == LE_NOT_FOUND)
        {
            result = CreateResourceData(recRef, path, DATA_TYPE_BOOL);

            if (result != LE_OK)
            {
                return result;
            }

            result = GetResourceData(recRef, path, DATA_TYPE_BOOL, &resourceDataPtr);

            // if creating it and we still cannot the resource, there is an issue
            if (result != LE_OK)
            {
                return result;
            }
        }

        result = AddBoolResourceData(recRef, resourceDataPtr, value, timestamp);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add the string value for the specified resource
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NO_MEMORY if the current entry was NOT added because the time series buffer is full.
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t timeSeries_AddString
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    const char* value,
    uint64_t timestamp
)
{
    le_result_t result;
    ResourceData_t* resourceDataPtr;

    result = GetResourceData(recRef, path, DATA_TYPE_STRING, &resourceDataPtr);

    // cmust be ok or not found
    if (result != LE_FAULT)
    {
        AddTimestamp(recRef, timestamp);

        // resource data does not exists
        if (result == LE_NOT_FOUND)
        {
            result = CreateResourceData(recRef, path, DATA_TYPE_STRING);

            if (result != LE_OK)
            {
                return result;
            }

            result = GetResourceData(recRef, path, DATA_TYPE_STRING, &resourceDataPtr);

            // if creating it and we still cannot the resource, there is an issue
            if (result != LE_OK)
            {
                return result;
            }
        }

        result = AddStringResourceData(recRef, resourceDataPtr, value, timestamp);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Compress the accumulated time series data and send it to server.
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t timeSeries_PushRecord
(
    timeSeries_RecordRef_t recRef,
    le_avdata_CallbackResultFunc_t handlerPtr,
    void* contextPtr
)
{

    le_result_t result;
    uint8_t buffer[MAX_CBOR_BUFFER_NUMBYTES];
    size_t bufferLength;
    z_stream defstream;

    result = Encode(recRef);

    // Compress the cbor encoded data
    if (result == LE_OK)
    {
        defstream.zalloc = Z_NULL;
        defstream.zfree = Z_NULL;
        defstream.opaque = Z_NULL;

        defstream.avail_in = GetEncodedDataSize(recRef);
        defstream.next_in = (Bytef *)recRef->bufferPtr;
        defstream.avail_out = (uInt)sizeof(buffer);
        defstream.next_out = (Bytef *)buffer;

        deflateInit(&defstream, Z_BEST_COMPRESSION);
        deflate(&defstream, Z_FINISH);
        deflateEnd(&defstream);

        bufferLength = defstream.total_out;

        result = PushBuffer(buffer,
                            bufferLength,
                            LWM2MCORE_PUSH_CONTENT_ZCBOR,
                            handlerPtr,
                            contextPtr);

        // if data was successfully pushed, reset our record
        if ((result == LE_OK) || (result == LE_BUSY))
        {
            LE_DEBUG("Data push success");
            ResetRecord(recRef); // clear all data accumulated for this record
        }
    }

    return result;
}


le_result_t timeSeries_Init
(
    void
)
{
    // Create the various memory pools
    RecordDataPoolRef = le_mem_CreatePool("Record pool", sizeof(RecordData_t));
    TimestampDataPoolRef = le_mem_CreatePool("Timestamp pool", sizeof(TimestampData_t));
    ResourceDataPoolRef = le_mem_CreatePool("Resource pool", sizeof(ResourceData_t));
    DataValuePoolRef = le_mem_CreatePool("Data value pool", sizeof(Data_t));
    StringValuePoolRef = le_mem_CreatePool("String pool", LE_AVDATA_STRING_VALUE_BYTES);

    CborBufferPoolRef = le_mem_CreatePool("CBOR buffer pool", MAX_CBOR_BUFFER_NUMBYTES);

    return LE_OK;
}
