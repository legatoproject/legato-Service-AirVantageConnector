/**
 * @file pa_avc.h
 *
 * Legato @ref c_pa_avc include file.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef LEGATO_PA_AVC_INCLUDE_GUARD
#define LEGATO_PA_AVC_INCLUDE_GUARD

#include "legato.h"
#include "interfaces.h"

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
LE_SHARED le_result_t pa_avc_SetEdmPollingTimerInSeconds
(
    uint32_t pollingTimeSecs ///< [IN] Polling timer interval, seconds
);

#endif // LEGATO_PA_AVC_INCLUDE_GUARD
