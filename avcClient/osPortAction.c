/**
 * @file osPortAction.c
 *
 * Porting layer for actions performed on the device
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/device.h>

#include "legato.h"
#include "interfaces.h"
#include "assetData.h"
#include "avcAppUpdate.h"
#include "avcServer.h"
#ifdef LE_CONFIG_LINUX
#include <sys/reboot.h>
#include <sys/utsname.h>
#endif
//--------------------------------------------------------------------------------------------------
/**
 * Default timer value for reboot request
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_REBOOT_TIMER  2

//--------------------------------------------------------------------------------------------------
/**
 * Timer to treat platform reboot
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t TreatRebootTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Launch the timer to treat the platform reboot request
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t LaunchRebootRequestTimer
(
    void
)
{
    le_clk_Time_t interval = { .sec = DEFAULT_REBOOT_TIMER };
    if ((LE_OK == le_timer_SetInterval(TreatRebootTimer, interval))
     && (LE_OK == le_timer_Start(TreatRebootTimer)))
    {
        return LE_OK;
    }
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Request to reboot the device
 * This API needs to have a procedural treatment
 *
 * @warning The client MUST acknowledge this function before treating the reboot request, in order
 * to allow LwM2MCore to acknowledge the LwM2M server that the reboot request is correctly taken
 * into account.
 * Advice: launch a timer (value could be decided by the client implementation) in order to treat
 * the reboot request.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_RebootDevice
(
    void
)
{
    // Launch timer as requested by LwM2MCore
    if (LE_OK == LaunchRebootRequestTimer())
    {
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
    return LWM2MCORE_ERR_GENERAL_ERROR;
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the timer for platform reboot expires
 */
//--------------------------------------------------------------------------------------------------
static void TreatRebootExpiryHandler
(
    le_timer_Ref_t timerRef    ///< Timer that expired
)
{
    lwm2mcore_RebootDevice();
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the AVC device client sub-component.
 *
 * @note This function should be called during the initializaion phase of the AVC daemon.
 */
//--------------------------------------------------------------------------------------------------
void avcClient_DeviceInit
(
   void
)
{
    TreatRebootTimer = le_timer_Create("launch timer for reboot");
    le_timer_SetHandler(TreatRebootTimer, TreatRebootExpiryHandler);
}
