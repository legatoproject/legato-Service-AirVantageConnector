/**
 * @file osTime.c
 *
 * Adaptation layer for time
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */


#include "legato.h"
#include "interfaces.h"
#include "osTime.h"

//--------------------------------------------------------------------------------------------------
/**
 * Function to retreive the device time
 *
 * @return
 *      - device time (UNIX time: seconds since January 01, 1970)
 */
//--------------------------------------------------------------------------------------------------
time_t lwm2m_gettime
(
    void
)
{
    le_result_t res = LE_FAULT;
    uint64_t   millisecondsPastGpsEpoch = 0;
    struct timeval tv;

    res = le_rtc_GetUserTime (&millisecondsPastGpsEpoch);
    LE_INFO ("lwm2m_gettime le_rtc_GetUserTime res %d, millisecondsPastGpsEpoch %d",
            res, millisecondsPastGpsEpoch);

    if (0 != gettimeofday(&tv, NULL))
    {
        return -1;
    }
    LE_INFO ("tv.tv_sec %d", tv.tv_sec);

    return tv.tv_sec;
}

