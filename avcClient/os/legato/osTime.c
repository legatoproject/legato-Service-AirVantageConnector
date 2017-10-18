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
    le_clk_Time_t deviceTime = le_clk_GetAbsoluteTime();

    LE_DEBUG("Device time: %ld", deviceTime.sec);

    return deviceTime.sec;
}
