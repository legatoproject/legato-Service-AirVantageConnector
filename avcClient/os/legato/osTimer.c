/**
 * @file osTimer.c
 *
 * Adaptation layer for timer management
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */


#include <stdbool.h>
#include <lwm2mcore/timer.h>
#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * LwM2M step timer
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t Lwm2mStepTimerRef = NULL;
const static char Lwm2mStepTimerName[] = "lwm2mStepTimer";

//--------------------------------------------------------------------------------------------------
/**
 * Timer to monitor LwM2M inactivity
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t Lwm2mInactivityTimerRef = NULL;
const static char Lwm2mInactivityTimerName[] = "lwm2mInactivityTimer";

//--------------------------------------------------------------------------------------------------
/**
 * Reboot on expiry timer
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t Lwm2mLaunchRebootRef = NULL;
const static char Lwm2mLaunchRebootName[] = "lwm2mLaunchReboot";

//--------------------------------------------------------------------------------------------------
/**
 * Called when the lwm2m step timer expires.
 */
//--------------------------------------------------------------------------------------------------
static void TimerHandler
(
    le_timer_Ref_t timerRef
)
{
    lwm2mcore_TimerCallback_t timerCallbackPtr = le_timer_GetContextPtr(timerRef);
    if (timerCallbackPtr)
    {
        timerCallbackPtr();
    }
    else
    {
        LE_ERROR("timerCallbackPtr unable to get Timer context pointer");
    }
}
//--------------------------------------------------------------------------------------------------
/**
 * Adaptation function for timer launch
 *
 * @return
 *      - true  on success
 *      - false on failure
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_TimerSet
(
    lwm2mcore_TimerType_t timer,    ///< [IN] Timer Id
    uint32_t time,                  ///< [IN] Timer value in seconds
    lwm2mcore_TimerCallback_t cb    ///< [IN] Timer callback
)
{
    le_result_t start = LE_FAULT;
    bool result = false;
    le_clk_Time_t timerInterval;
    le_timer_Ref_t timerRef;
    const char* timerName;

    LE_DEBUG ("lwm2mcore_TimerSet %d time %d sec", timer, time);

    if (timer >= LWM2MCORE_TIMER_MAX)
    {
        return false;
    }

    switch (timer)
    {
        case LWM2MCORE_TIMER_STEP:
            timerRef = Lwm2mStepTimerRef;
            timerName = Lwm2mStepTimerName;
            break;

        case LWM2MCORE_TIMER_INACTIVITY:
            timerRef = Lwm2mInactivityTimerRef;
            timerName = Lwm2mInactivityTimerName;
            break;

        case LWM2MCORE_TIMER_REBOOT:
            timerRef = Lwm2mLaunchRebootRef;
            timerName = Lwm2mLaunchRebootName;
            break;

        default:
            LE_ERROR("Unhandled timer reference");
            return false;
    }

    if (timerRef == NULL)
    {
        timerInterval.sec = time;
        timerInterval.usec = 0;
        timerRef = le_timer_Create(timerName);
        if (timerRef)
        {
            if ((LE_OK != le_timer_SetInterval(timerRef, timerInterval))
                || (LE_OK != le_timer_SetHandler(timerRef, TimerHandler))
                || (LE_OK != le_timer_SetContextPtr(timerRef, (void*) cb))
                || (LE_OK != le_timer_Start(timerRef)))
            {
                start = LE_FAULT;
            }
            else
            {
                start = LE_OK;
            }
        }
    }
    else
    {
        le_result_t stop = LE_FAULT;
        /* check if the timer is running */
        if (le_timer_IsRunning(timerRef))
        {
            /* Stop the timer */
            stop = le_timer_Stop(timerRef);
            if (stop != LE_OK)
            {
                LE_ERROR ("Error when stopping step timer");
            }
        }

        timerInterval.sec = time;
        timerInterval.usec = 0;

        le_timer_SetInterval(timerRef, timerInterval);
        start = le_timer_Start(timerRef);
    }

    if (start == LE_OK)
    {
        result = true;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Adaptation function for timer stop
 *
 * @return
 *      - true  on success
 *      - false on failure
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_TimerStop
(
    lwm2mcore_TimerType_t timer    ///< [IN] Timer Id
)
{
    le_result_t stop = false;
    bool result = false;

    if (timer < LWM2MCORE_TIMER_MAX)
    {
        switch (timer)
        {
            case LWM2MCORE_TIMER_STEP:

                if (Lwm2mStepTimerRef != NULL)
                {
                    stop = le_timer_Stop(Lwm2mStepTimerRef);
                }
                break;

            case LWM2MCORE_TIMER_INACTIVITY:
                if (Lwm2mInactivityTimerRef != NULL)
                {
                    stop = le_timer_Stop(Lwm2mInactivityTimerRef);
                }
                break;

            default:
                break;
        }
    }

    if (stop == LE_OK)
    {
        result = true;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Adaptation function for timer state
 *
 * @return
 *      - true  if the timer is running
 *      - false if the timer is stopped
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_TimerIsRunning
(
    lwm2mcore_TimerType_t timer    ///< [IN] Timer Id
)
{
    bool isRunning = false;

    if (timer < LWM2MCORE_TIMER_MAX)
    {
        switch (timer)
        {
            case LWM2MCORE_TIMER_STEP:
                if (Lwm2mStepTimerRef != NULL)
                {
                    isRunning = le_timer_IsRunning (Lwm2mStepTimerRef);
                }
                LE_DEBUG("LWM2MCORE_TIMER_STEP timer is running %d", isRunning);
                break;

            case LWM2MCORE_TIMER_INACTIVITY:
                if (Lwm2mInactivityTimerRef != NULL)
                {
                    isRunning = le_timer_IsRunning(Lwm2mInactivityTimerRef);
                }
                LE_DEBUG("LWM2MCORE_TIMER_INACTIVITY timer is running %d", isRunning);
                break;

            default:
                break;
        }
    }
    return isRunning;
}

