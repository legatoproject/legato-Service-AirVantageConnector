/**
 * @file timeseriesData.h
 *
 * Time Series Interface
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef LEGATO_TIMESERIES_DATA_INCLUDE_GUARD
#define LEGATO_TIMESERIES_DATA_INCLUDE_GUARD

#include "legato.h"

#define NUM_TIME_SERIES_MAPS 3


//--------------------------------------------------------------------------------------------------
/**
 * Reference to a record.
 */
//--------------------------------------------------------------------------------------------------
typedef struct le_avdata_Record* timeSeries_RecordRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Checks the return value from the tinyCBOR encoder and returns from function if an error is found.
 */
//--------------------------------------------------------------------------------------------------
#define \
    RETURN_IF_CBOR_ERROR( err ) \
    ({ \
        if (err != CborNoError) \
        { \
            LE_ERROR("CBOR encoding error %s", cbor_error_string(err)); \
            if (err == CborErrorOutOfMemory) \
            { \
                return LE_NO_MEMORY; \
            } \
            return LE_FAULT; \
        } \
    })


//--------------------------------------------------------------------------------------------------
/**
 * Create a timeseries record
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t timeSeries_Create
(
    timeSeries_RecordRef_t* recRefPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Delete a timeseries record
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void timeSeries_Delete
(
    timeSeries_RecordRef_t recRef
);


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
LE_SHARED le_result_t timeSeries_AddInt
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    int32_t value,
    uint64_t timestamp
);


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
LE_SHARED le_result_t timeSeries_AddFloat
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    double value,
    uint64_t timestamp
);


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
LE_SHARED le_result_t timeSeries_AddBool
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    bool value,
    uint64_t timestamp
);


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
LE_SHARED le_result_t timeSeries_AddString
(
    timeSeries_RecordRef_t recRef,
    const char* path,
    const char* value,
    uint64_t timestamp
);


//--------------------------------------------------------------------------------------------------
/**
 * Compress the accumulated time series data and send it to server.
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t timeSeries_PushRecord
(
    timeSeries_RecordRef_t recRef,
    le_avdata_CallbackResultFunc_t handlerPtr,
    void* contextPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Init this sub-component
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t timeSeries_Init
(
    void
);

#endif // LEGATO_ASSET_DATA_INCLUDE_GUARD
