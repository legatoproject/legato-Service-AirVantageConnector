//--------------------------------------------------------------------------------------------------
/**
 * @file pa_avc_default.c
 *
 * Default implementation of @ref pa_avc interface
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "pa_avc.h"


//--------------------------------------------------------------------------------------------------
/**
 * Function to set the EDM polling timer to a value in seconds
 *
 * @return
 *      - LE_OK on success.
 *      - LE_OUT_OF_RANGE if the polling timer value is out of range (0 to 525600).
 *      - LE_FAULT upon failure to set it.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_avc_SetEdmPollingTimerInSeconds
(
    uint32_t pollingTimeSecs ///< [IN] Polling timer interval, seconds
)
{
    LE_ERROR("Unsupported function called");
    return LE_FAULT;
}


//--------------------------------------------------------------------------------------------------
/**
 * Init this component
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
}
