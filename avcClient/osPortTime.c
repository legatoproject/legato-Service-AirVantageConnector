/**
 * @file osPortTime.c
 *
 * Porting layer for device time parameters
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/lwm2mcore.h>
#include "legato.h"
#include "interfaces.h"


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
 * Initiate the setting of the device time (UNIX time in seconds) to the given clock time by
 * archiving this input time and registering a post LWM2M request processing handler so that
 * after a response is sent out over for this device clock setting request, then the actual clock
 * change execution thru this handler can be carried out. This is because if this execution isn't
 * carried out in this chronological arrangement, the sudden clock change will fail the sending
 * out of the response, e.g. over DTLS, and the subsequent outstanding LWM2M jobs on its queue.
 * In another word, the response has to go out 1st before the clock change happens.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_SetDeviceCurrentTime
(
    uint64_t inputTime  ///< [IN] Current clock time given
)
{
    LE_DEBUG("input time %ld", (long)inputTime);

    if (!lwm2mcore_AddPostRequestHandler(lwm2mcore_UpdateSystemClock))
    {
        LE_ERROR("Failed to initiate clock time update");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Save inputTime onto config tree to make this given value persistent across system restart
    // in case it is needed as a last resort system clock time. This can happen to a device
    // after a restart & its total failure to get any more up-to-date clock so that its system
    // clock has to restart from 1970/1/1 again. Then this last archived clock time provided by
    // an AV server is still relatively more up-to-date for use.
    le_cfg_IteratorRef_t cfg;
    cfg = le_cfg_CreateWriteTxn(LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE);
    le_cfg_SetInt(cfg, LE_CLKSYNC_CONFIG_NODE_SOURCE_AVC_TIMESTAMP, inputTime);
    le_cfg_CommitTxn(cfg);

    return LWM2MCORE_ERR_COMPLETED_OK;
}
