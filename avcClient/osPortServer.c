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
#define LIFETIME_VALUE_MIN            0

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
