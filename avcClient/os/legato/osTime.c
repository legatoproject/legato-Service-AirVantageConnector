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
#include "le_cfg_interface.h"
#include <lwm2mcore/lwm2mcore.h>
#include "dtlsConnection.h"

//--------------------------------------------------------------------------------------------------
/**
 * Config tree root directory and node paths for clock time configurations
 * Note: These 2 defines are temporary. After the new Legato Clock Service interface le_clkSync is
 * added, these defines will be there in le_clkSync.api and these ones can be removed.
 */
//--------------------------------------------------------------------------------------------------
#define LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE          "clockTime:/source"
#define LE_CLKSYNC_CONFIG_NODE_SOURCE_AVC_TIMESTAMP "timeStamp"

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

//--------------------------------------------------------------------------------------------------
/**
 * Function to perform an immediate system clock update using the clock time value set on the config
 * tree.  Leave this value there not reset after use so that it can serve as a last resort clock
 * time more up-to-date than 1970/1/1 in case the device after a restart cannot succeed in any way
 * e.g. via QMI, TP, NTP, etc., to get the current clock time.
 * The use of this last-resort clock time hasn't been implemented yet, but will be soon under the
 * new Clock Service in Legato.
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_UpdateSystemClock
(
    void* connP
)
{
    dtls_Connection_t* connPtr = (dtls_Connection_t*)connP;
    le_cfg_IteratorRef_t cfg;
    le_result_t result;
    le_clk_Time_t t;
    time_t clockStamp;

    cfg = le_cfg_CreateReadTxn(LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE);
    if (!cfg)
    {
        LE_WARN("No clock stamp given to update the system clock");
        return;
    }
    if (!le_cfg_NodeExists(cfg, LE_CLKSYNC_CONFIG_NODE_SOURCE_AVC_TIMESTAMP))
    {
        LE_WARN("No clock stamp given to update the system clock");
        le_cfg_CancelTxn(cfg);
        return;
    }

    clockStamp = le_cfg_GetInt(cfg, LE_CLKSYNC_CONFIG_NODE_SOURCE_AVC_TIMESTAMP, 0);
    le_cfg_CancelTxn(cfg);

    if (clockStamp <= 0)
    {
        LE_WARN("No valid clock stamp retrieved to update the system clock");
        return;
    }

    t = le_clk_GetAbsoluteTime();
    LE_INFO("device's time %ld sec %ld usec before", (long)t.sec, (long)t.usec);

    t.sec = clockStamp;
    t.usec = 0;

    result = le_clk_SetAbsoluteTime(t);
    LE_INFO("Result in setting system clock time: %d", result);

    t = le_clk_GetAbsoluteTime();
    LE_INFO("device's time %ld  %ld usec after", (long)t.sec, (long)t.usec);

    LE_INFO("Triggering DTLS rehandshake after system clock update");

    if (!connPtr)
    {
        LE_DEBUG("No need to initiate a DTLS handshake");
        return;
    }

    // Initiate a DTLS handshake after the system clock has changed so that DTLS can continue to
    // work for AVC
    if (0 != dtls_Rehandshake(connPtr, false))
    {
        LE_ERROR("Unable to perform a DTLS rehandshake for connection %p", connPtr);
    }
}
