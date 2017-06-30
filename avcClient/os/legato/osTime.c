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


#include <liblwm2m.h>
#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Function to retrieve the device time
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
    LE_DEBUG ("lwm2m_gettime le_rtc_GetUserTime res %d, millisecondsPastGpsEpoch %d",
            res, millisecondsPastGpsEpoch);

    if (0 != gettimeofday(&tv, NULL))
    {
        return -1;
    }
    LE_DEBUG ("tv.tv_sec %d", tv.tv_sec);

    return tv.tv_sec;
}
