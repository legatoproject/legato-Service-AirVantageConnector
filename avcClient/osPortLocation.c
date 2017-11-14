/**
 * @file osPortLocation.c
 *
 * Porting layer for location parameters
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/location.h>
#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the WSG84 latitude
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLatitude
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    int32_t latitude;
    int32_t longitude;
    int32_t hAccuracy;
    size_t latitudeLen;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_pos_Get2DLocation(&latitude, &longitude, &hAccuracy);
    switch (result)
    {
        case LE_OK:
            latitudeLen = snprintf(bufferPtr, *lenPtr, "%.6f", (float)latitude/1e6);
            if (*lenPtr < latitudeLen)
            {
                sID = LWM2MCORE_ERR_OVERFLOW;
            }
            else
            {
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

        case LE_OUT_OF_RANGE:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("lwm2mcore_LocationLatitude result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the WSG84 longitude
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLongitude
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    int32_t latitude;
    int32_t longitude;
    int32_t hAccuracy;
    size_t longitudeLen;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_pos_Get2DLocation(&latitude, &longitude, &hAccuracy);
    switch (result)
    {
        case LE_OK:
            longitudeLen = snprintf(bufferPtr, *lenPtr, "%.6f", (float)longitude/1e6);
            if (*lenPtr < longitudeLen)
            {
                sID = LWM2MCORE_ERR_OVERFLOW;
            }
            else
            {
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

        case LE_OUT_OF_RANGE:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("lwm2mcore_LocationLongitude result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the altitude (meters above sea level)
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetAltitude
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    int32_t latitude;
    int32_t longitude;
    int32_t altitude;
    int32_t hAccuracy;
    int32_t vAccuracy;
    size_t altitudeLen;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_pos_Get3DLocation(&latitude, &longitude, &hAccuracy, &altitude, &vAccuracy);
    switch (result)
    {
        case LE_OK:
            altitudeLen = snprintf(bufferPtr, *lenPtr, "%d", altitude);
            if (*lenPtr < altitudeLen)
            {
                sID = LWM2MCORE_ERR_OVERFLOW;
            }
            else
            {
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

        case LE_OUT_OF_RANGE:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("lwm2mcore_LocationAltitude result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the direction of movement (range 0-359 degrees, where 0 is True North)
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetDirection
(
    uint32_t* valuePtr  ///< [INOUT] data buffer
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    uint32_t direction;
    uint32_t directionAccuracy;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_pos_GetDirection(&direction, &directionAccuracy);
    switch (result)
    {
        case LE_OK:
        case LE_OUT_OF_RANGE:
            if (UINT32_MAX != direction)
            {
                *valuePtr = direction;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            else
            {
                sID = LWM2MCORE_ERR_INVALID_STATE;
            }
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("lwm2mcore_LocationDirection result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the horizontal speed in m/s
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetHorizontalSpeed
(
    uint32_t* valuePtr  ///< [INOUT] data buffer
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    uint32_t hSpeed;
    uint32_t hSpeedAccuracy;
    int32_t  vSpeed;
    int32_t  vSpeedAccuracy;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_pos_GetMotion(&hSpeed, &hSpeedAccuracy, &vSpeed, &vSpeedAccuracy);
    switch (result)
    {
        case LE_OK:
        case LE_OUT_OF_RANGE:
            if (UINT32_MAX != hSpeed)
            {
                *valuePtr = hSpeed;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            else
            {
                sID = LWM2MCORE_ERR_INVALID_STATE;
            }
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("lwm2mcore_LocationHorizontalSpeed result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the vertical speed in m/s, positive up
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetVerticalSpeed
(
    int32_t* valuePtr   ///< [INOUT] data buffer
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    uint32_t hSpeed;
    uint32_t hSpeedAccuracy;
    int32_t  vSpeed;
    int32_t  vSpeedAccuracy;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_pos_GetMotion(&hSpeed, &hSpeedAccuracy, &vSpeed, &vSpeedAccuracy);
    switch (result)
    {
        case LE_OK:
        case LE_OUT_OF_RANGE:
            if (INT32_MAX != vSpeed)
            {
                *valuePtr = vSpeed;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            else
            {
                sID = LWM2MCORE_ERR_INVALID_STATE;
            }
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("lwm2mcore_LocationVerticalSpeed result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the timestamp of when the location measurement was performed
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLocationTimestamp
(
    uint64_t* valuePtr  ///< [INOUT] data buffer
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    uint64_t milliseconds;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Get Epoch time of last position sample
    le_gnss_SampleRef_t positionSampleRef = le_gnss_GetLastSampleRef();
    result = le_gnss_GetEpochTime(positionSampleRef, &milliseconds);
    if (LE_OK == result)
    {
        // Convert value to seconds
        *valuePtr = milliseconds / 1000;
        sID = LWM2MCORE_ERR_COMPLETED_OK;
    }
    else
    {
        sID = LWM2MCORE_ERR_INVALID_STATE;
    }

    // Release provided position sample reference
    le_gnss_ReleaseSampleRef(positionSampleRef);

    LE_DEBUG("lwm2mcore_LocationTimestamp result: %d", sID);
    return sID;
}
