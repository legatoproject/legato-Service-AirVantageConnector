/**
 * @file osConfig.c
 *
 * Adaptation layer for Configuration
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/lwm2mcore.h>
#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * Function to read the polling timer.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetPollingTimer
(
    uint32_t* pollingTimerPtr  ///< [OUT] Polling timer
)
{
    return (LE_OK == le_avc_GetPollingTimer(pollingTimerPtr)) ? LWM2MCORE_ERR_COMPLETED_OK :
                                                                LWM2MCORE_ERR_GENERAL_ERROR;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the polling timer.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetPollingTimer
(
    uint32_t pollingTimer ///< [IN] Polling timer
)
{
    return (LE_OK == le_avc_SetPollingTimer(pollingTimer)) ? LWM2MCORE_ERR_COMPLETED_OK :
                                                             LWM2MCORE_ERR_GENERAL_ERROR;
}
