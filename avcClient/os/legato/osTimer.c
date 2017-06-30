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

static le_timer_Ref_t Lwm2mStepTimerRef = NULL;

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

    LE_DEBUG ("lwm2mcore_TimerSet %d time %d sec", timer, time);
    if (timer < LWM2MCORE_TIMER_MAX)
    {
        switch (timer)
        {
            case LWM2MCORE_TIMER_STEP:
            {
                if (Lwm2mStepTimerRef == NULL)
                {
                    timerInterval.sec = time;
                    timerInterval.usec = 0;
                    Lwm2mStepTimerRef = le_timer_Create ("lwm2mStepTimer");
                    start = le_timer_SetInterval (Lwm2mStepTimerRef, timerInterval);
                    start = le_timer_SetHandler (Lwm2mStepTimerRef, cb);
                    start = le_timer_Start (Lwm2mStepTimerRef);
                }
                else
                {
                    le_result_t stop = LE_FAULT;
                    /* check if the timer is running */
                    if (le_timer_IsRunning (Lwm2mStepTimerRef))
                    {
                        /* Stop the timer */
                        stop = le_timer_Stop (Lwm2mStepTimerRef);
                        if (stop != LE_OK)
                        {
                            LE_ERROR ("Error when stopping step timer");
                        }
                    }

                    timerInterval.sec = time;
                    timerInterval.usec = 0;

                    start = le_timer_SetInterval (Lwm2mStepTimerRef, timerInterval);
                    start = le_timer_Start (Lwm2mStepTimerRef);
                }
            }
            break;

            default:
            break;
        }
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
            {
                if (Lwm2mStepTimerRef != NULL)
                {
                    stop = le_timer_Stop (Lwm2mStepTimerRef);
                }
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
            {
                if (Lwm2mStepTimerRef != NULL)
                {
                    isRunning = le_timer_IsRunning  (Lwm2mStepTimerRef);
                }
            }
            break;

            default:
            break;
        }
    }
    LE_DEBUG ("LWM2MCORE_TIMER_STEP timer is running %d", isRunning);
    return isRunning;
}

