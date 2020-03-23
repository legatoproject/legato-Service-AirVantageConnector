/**
 * @file osPortSim.c
 *
 * Porting layer for device sim interface
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/device.h>

#include "legato.h"
#include "interfaces.h"
#include "assetData/assetData.h"
#ifdef LE_CONFIG_LINUX
#include <sys/reboot.h>
#include <sys/utsname.h>
#endif
#ifndef LE_CONFIG_SOTA
#include "avcAppUpdate/avcAppUpdate.h"
#endif
#include "avcServer/avcServer.h"
#include "avcSim/avcSim.h"



//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the currently used SIM card
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetCurrentSimCard
(
    uint8_t*   currentSimPtr  ///< [OUT]    Currently used SIM card
)
{
    if (!currentSimPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *currentSimPtr = GetCurrentSimCard();
    LE_DEBUG("lwm2mcore_GetCurrentSimCard: %d", *currentSimPtr);

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set SIM mode
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetSimMode
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    char* savePtr = NULL;
    char* token = NULL;
    SimMode_t mode = MODE_MAX;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Received parameter format is: ['1'='x']. So, extract the value of x from the string.
    if (NULL != strtok_r(bufferPtr, "=", &savePtr))
    {
        token = strtok_r(NULL, "'", &savePtr);
        if (token)
        {
            mode = (SimMode_t)atoi(token);
        }
    }

    if ((mode >= MODE_MAX) || (mode <= MODE_IN_PROGRESS))
    {
        LE_ERROR("Invalid mode: %d", mode);
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    SimModeInit();
    SetSimMode(mode);

    LE_DEBUG("lwm2mcore_SetSimMode: %x", mode);

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the current SIM mode
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetCurrentSimMode
(
    uint8_t*   simModePtr  ///< [OUT]    SIM mode pointer
)
{
    if (!simModePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *simModePtr = GetCurrentSimMode();
    LE_DEBUG("lwm2mcore_GetCurrentSimMode: %d", *simModePtr);

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the last SIM switch procedure status
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLastSimSwitchStatus
(
    uint8_t*   switchStatusPtr  ///< [OUT]    SIM switch status
)
{
    if (!switchStatusPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *switchStatusPtr = GetLastSimSwitchStatus();
    LE_DEBUG("lwm2mcore_GetLastSimSwitchStatus: %d", *switchStatusPtr);

    return LWM2MCORE_ERR_COMPLETED_OK;
}
