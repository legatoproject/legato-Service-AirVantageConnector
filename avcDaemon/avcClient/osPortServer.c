/**
 * @file osPortServer.c
 *
 * Porting layer for server object parameters
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/server.h>
#include "avcServer/avcServer.h"

//--------------------------------------------------------------------------------------------------
/**
 * Lifetime maximum value
 * 31536000 = 1 year in seconds
 */
//--------------------------------------------------------------------------------------------------
#define LIFETIME_VALUE_MAX            31536000

//--------------------------------------------------------------------------------------------------
/**
 * Lifetime minimum value
 */
//--------------------------------------------------------------------------------------------------
#define LIFETIME_VALUE_MIN            1

//--------------------------------------------------------------------------------------------------
/**
 * Number of seconds in a minute
 */
//--------------------------------------------------------------------------------------------------
#define SECONDS_IN_A_MIN 60

//--------------------------------------------------------------------------------------------------
/**
 * Function to check if the lifetime is within acceptable limits
 *
 * @return
 *      - true if lifetime is within limits
 *      - false else
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_CheckLifetimeLimit
(
    uint32_t lifetime                  ///< [IN] Lifetime in seconds
)
{
    // Check only when enabling lifetime
    if (lifetime != LWM2MCORE_LIFETIME_VALUE_DISABLED)
    {
        if ((lifetime < LIFETIME_VALUE_MIN)
            || (lifetime > LIFETIME_VALUE_MAX))
        {
            LE_ERROR("Lifetime not within limit");
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Polling Timer
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *      - LWM2MCORE_ERR_INCORRECT_RANGE parameter out of range
 *      - LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetPollingTimer
(
    uint32_t interval   ///< [IN] Polling Timer interval in seconds
)
{
    uint32_t value = interval;
    LE_INFO("Setting polling timer to %"PRIu32" seconds", interval);
    if (false == lwm2mcore_CheckLifetimeLimit(interval))
    {
        return LWM2MCORE_ERR_INCORRECT_RANGE;
    }

    if (LWM2MCORE_LIFETIME_VALUE_DISABLED == interval)
    {
        value = 0;
    }

    if (LE_OK != avcServer_SetPollingTimerInSeconds(value))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the EDM Polling Timer
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if succeeds
 *      - LWM2MCORE_ERR_INCORRECT_RANGE parameter out of range
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED if not supported
 *      - LWM2MCORE_ERR_GENERAL_ERROR other failure
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetEdmPollingTimer
(
    uint32_t interval   ///< [IN] Polling Timer interval in seconds
)
{
#if LE_CONFIG_AVC_FEATURE_EDM
    uint32_t value = interval;
    LE_INFO("Setting EDM polling timer to %"PRIu32" seconds", interval);
    // TBD: 1s < value < 1year - is it good enough check for EDM?
    if (false == lwm2mcore_CheckLifetimeLimit(interval))
    {
        return LWM2MCORE_ERR_INCORRECT_RANGE;
    }

    // TBD: 20years magic number - is it good for EDM?
    if (LWM2MCORE_LIFETIME_VALUE_DISABLED == interval)
    {
        value = 0;
    }

    if (LE_OK != avcServer_SetEdmPollingTimerInSeconds(value))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
#else
    return LWM2MCORE_ERR_OP_NOT_SUPPORTED;
#endif
}
