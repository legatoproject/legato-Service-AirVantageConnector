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
 * LwM2M timers list
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t Lwm2mTimerRef[LWM2MCORE_TIMER_MAX] = {NULL};

//--------------------------------------------------------------------------------------------------
/**
 *  Convert timer name ID to string
 */
//--------------------------------------------------------------------------------------------------
static char* ConvertTimerIdToString
(
    lwm2mcore_TimerType_t timer     ///< [IN] Timer Identifier
)
{
    char* result;

    switch (timer)
    {
        case LWM2MCORE_TIMER_STEP:
            result = STRINGIZE_EXPAND(LWM2MCORE_TIMER_STEP);
            break;

        case LWM2MCORE_TIMER_INACTIVITY:
            result = STRINGIZE_EXPAND(LWM2MCORE_TIMER_INACTIVITY);
            break;

        case LWM2MCORE_TIMER_REBOOT:
            result = STRINGIZE_EXPAND(LWM2MCORE_TIMER_REBOOT);
            break;

        default:
            LE_WARN("Timer ID not present in the list");
            result = "Undefined timer";
            break;
    }

    return result;
}

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
    le_clk_Time_t timerInterval = {time, 0};

    if (timer >= LWM2MCORE_TIMER_MAX)
    {
        return false;
    }

    LE_DEBUG("lwm2mcore_TimerSet %s (id:%d, time %d sec)",
             ConvertTimerIdToString(timer), timer, time);

    if (Lwm2mTimerRef[timer] == NULL)
    {
        LE_DEBUG("Create a new timer %d", timer);

        Lwm2mTimerRef[timer] = le_timer_Create(ConvertTimerIdToString(timer));
        if (Lwm2mTimerRef[timer] == NULL)
        {
            LE_ERROR("Unable to create timer %d", timer);
            return false;
        }

        if ((LE_OK != le_timer_SetInterval(Lwm2mTimerRef[timer], timerInterval))
            || (LE_OK != le_timer_SetHandler(Lwm2mTimerRef[timer], TimerHandler))
            || (LE_OK != le_timer_SetContextPtr(Lwm2mTimerRef[timer], (void*) cb))
            || (LE_OK != le_timer_Start(Lwm2mTimerRef[timer])))
        {
            LE_ERROR("Unable to configure timer %d", timer);
            return false;
        }
    }
    else
    {
        LE_DEBUG("Update previously created timer %s", ConvertTimerIdToString(timer));

        // Check if the timer is running and stop it
        if (le_timer_IsRunning(Lwm2mTimerRef[timer]))
        {
            if (le_timer_Stop(Lwm2mTimerRef[timer]) != LE_OK)
            {
                LE_ERROR("Unable to stop timer %d", timer);
                return false;
            }
        }

        if ((LE_OK != le_timer_SetInterval(Lwm2mTimerRef[timer], timerInterval))
            || (LE_OK != le_timer_Start(Lwm2mTimerRef[timer])))
        {
            LE_ERROR("Unable to start timer %d", timer);
            return false;
        }
    }

    return true;
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
    if (timer >= LWM2MCORE_TIMER_MAX)
    {
        return false;
    }

    if (Lwm2mTimerRef[timer])
    {
        if (le_timer_Stop(Lwm2mTimerRef[timer]) != LE_OK)
        {
            LE_ERROR("Unable to stop the timer");
            return false;
        }
    }
    else
    {
        LE_ERROR("Timer reference is NULL");
        return false;
    }

    return true;
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

    if (timer >= LWM2MCORE_TIMER_MAX)
    {
        return false;
    }

    if (Lwm2mTimerRef[timer] != NULL)
    {
        isRunning = le_timer_IsRunning(Lwm2mTimerRef[timer]);
    }
    else
    {
        LE_ERROR("Timer reference is NULL");
    }

    LE_DEBUG("%s timer is running %d", ConvertTimerIdToString(timer), isRunning);
    return isRunning;
}
