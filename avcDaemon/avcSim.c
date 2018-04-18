/**
 * @file avcSim.c
 *
 * Implementation of the SIM mode management
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "avcSim.h"

//--------------------------------------------------------------------------------------------------
/**
 * Expiration delay of the timer used for SIM mode switch procedure in ms
 */
//--------------------------------------------------------------------------------------------------
#define MODE_EXEC_TIMER_DELAY          5000

//--------------------------------------------------------------------------------------------------
/**
 * Expiration delay of the timer used for SIM mode rollback procedure in ms
 */
//--------------------------------------------------------------------------------------------------
#define MODE_ROLLBACK_TIMER_DELAY      300000

//--------------------------------------------------------------------------------------------------
/**
 * SIM Mode handler structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    SimMode_t         modeRequest;            ///< SIM mode change request
    bool              rollbackRequest;        ///< SIM mode rollback request
    bool              avcConnectionRequest;   ///< AVC connection request
    bool              isInit;                 ///< True if SIM mode resources are initialized
    SimMode_t         mode;                   ///< Current SIM mode
    SimMode_t         previousMode;           ///< Previous SIM mode
    SimSwitchStatus_t status;                 ///< Last SIM switch status
}
SimHandler_t;

//--------------------------------------------------------------------------------------------------
/**
 * Timer used to execute the SIM mode switch procedure
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t ModeExecTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Timer used to execute the SIM mode rollback procedure
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t ModeRollbackTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Reference to the Cellular Network state event handler
 */
//--------------------------------------------------------------------------------------------------
static le_cellnet_StateEventHandlerRef_t CellNetStateEventRef;

//--------------------------------------------------------------------------------------------------
/**
 * Reference to the AVC status event handler
 */
//--------------------------------------------------------------------------------------------------
static le_avc_StatusEventHandlerRef_t AvcStatusEventRef;

//--------------------------------------------------------------------------------------------------
/**
 * SIM mode handler
 */
//--------------------------------------------------------------------------------------------------
SimHandler_t SimHandler =
{
    .modeRequest          = MODE_MAX,
    .rollbackRequest      = false,
    .avcConnectionRequest = false,
    .isInit               = false,
    .mode                 = MODE_MAX,
    .previousMode         = MODE_MAX,
    .status               = SIM_NO_ERROR

};
SimHandler_t* SimHandlerPtr = &SimHandler;


//--------------------------------------------------------------------------------------------------
/**
 * Rollback to the previous SIM mode
 */
//--------------------------------------------------------------------------------------------------
static void SimModeRollback
(
    void
)
{
    if (SimHandlerPtr->rollbackRequest)
    {
        LE_ERROR("A SIM mode rollback is already ongoing");
        return;
    }

    SimHandlerPtr->rollbackRequest = true;
    SimHandlerPtr->modeRequest = SimHandlerPtr->previousMode;
    SimHandlerPtr->mode = MODE_IN_PROGRESS;

    le_avc_StopSession();
    le_timer_Restart(ModeExecTimer);
}

//--------------------------------------------------------------------------------------------------
/**
 * Event callback for AVC status changes
 */
//--------------------------------------------------------------------------------------------------
static void AvcStatusHandler
(
    le_avc_Status_t updateStatus,
    int32_t totalNumBytes,
    int32_t progress,
    void* contextPtr
)
{
    if (SimHandlerPtr->mode != MODE_IN_PROGRESS)
    {
        return;
    }

    if (updateStatus ==  LE_AVC_AUTH_STARTED)
    {
        le_timer_Stop(ModeRollbackTimer);

        if (!SimHandlerPtr->rollbackRequest)
        {
            SimHandlerPtr->status = SIM_NO_ERROR;
        }

        SimHandlerPtr->mode = SimHandlerPtr->modeRequest;
        SimHandlerPtr->modeRequest = MODE_MAX;
        SimHandlerPtr->rollbackRequest = false;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Event callback for Cellular Network Service state changes
 */
//--------------------------------------------------------------------------------------------------
static void CellNetStateHandler
(
    le_cellnet_State_t state,       ///< [IN] Cellular network state
    void*              contextPtr   ///< [IN] Associated context pointer
)
{
    le_mdc_ProfileRef_t profileRef;

    if (SimHandlerPtr->mode != MODE_IN_PROGRESS)
    {
        return;
    }

    switch (state)
    {
        case LE_CELLNET_REG_HOME:
        case LE_CELLNET_REG_ROAMING:
            if (!SimHandlerPtr->avcConnectionRequest)
            {
                break;
            }

            // Use the default APN for the current SIM card
            profileRef = le_mdc_GetProfile((uint32_t)le_data_GetCellularProfileIndex());
            if (!profileRef)
            {
                LE_ERROR("Unable to get the current data profile");
            }
            else
            {
                if (le_mdc_SetDefaultAPN(profileRef) != LE_OK)
                {
                    LE_ERROR("Could not set default APN for the select SIM");
                }
                else
                {
                    LE_INFO("Default APN is set");
                }
            }

            // Request a connection to AVC server
            if (le_avc_StartSession() == LE_FAULT)
            {
                LE_ERROR("Unable to start AVC sessions");
                SimHandlerPtr->status = SIM_SWITCH_ERROR;
                SimModeRollback();
            }

            SimHandlerPtr->avcConnectionRequest = false;
            break;

        default:
            break;

    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Timer handler: On expiry, this function rollbacks to the previous SIM mode configuration.
 */
//--------------------------------------------------------------------------------------------------
static void SimModeRollbackHandler
(
    le_timer_Ref_t timerRef    ///< [IN] This timer has expired
)
{
    SimHandlerPtr->status = SIM_SWITCH_TIMEOUT;
    SimModeRollback();
}

//--------------------------------------------------------------------------------------------------
/**
 * Timer handler: On expiry, this function attempts a switch to the new SIM according to the last
 * command received.
 */
//--------------------------------------------------------------------------------------------------
static void SimModeExecHandler
(
    le_timer_Ref_t timerRef    ///< [IN] This timer has expired
)
{
    le_result_t status = LE_FAULT;

    le_avc_StopSession();

    switch (SimHandlerPtr->modeRequest)
    {
        case MODE_EXTERNAL_SIM:
            status = le_sim_SelectCard(LE_SIM_EXTERNAL_SLOT_1);
            break;

        case MODE_INTERNAL_SIM:
            status = le_sim_SelectCard(LE_SIM_EMBEDDED);
            break;

        case MODE_PREF_EXTERNAL_SIM:
            LE_ERROR("Mode 3 is not supported");
            status = LE_FAULT;
            break;

        default:
            LE_ERROR("Unhandled mode");
            status = LE_FAULT;
            break;
    }

    if (status != LE_OK)
    {
        SimHandlerPtr->status = SIM_SWITCH_ERROR;
        SimModeRollback();
    }
    else
    {
        SimHandlerPtr->avcConnectionRequest = true;
        le_timer_Start(ModeRollbackTimer);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the current SIM mode
 *
 * @return
 *      - An enum as defined in SimMode_t
 */
//--------------------------------------------------------------------------------------------------
SimMode_t GetCurrentSimMode
(
    void
)
{
    if (SimHandlerPtr->mode == MODE_IN_PROGRESS)
    {
        return MODE_IN_PROGRESS;
    }

    if (le_sim_GetSelectedCard() == LE_SIM_EXTERNAL_SLOT_1)
    {
        return MODE_EXTERNAL_SIM;
    }
    else
    {
        return MODE_INTERNAL_SIM;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the current SIM card
 *
 * @return
 *      - An enum as defined in SimSlot_t
 */
//--------------------------------------------------------------------------------------------------
SimSlot_t GetCurrentSimCard
(
    void
)
{
    if (le_sim_GetSelectedCard() == LE_SIM_EXTERNAL_SLOT_1)
    {
        return SLOT_EXTERNAL;
    }
    else
    {
        return SLOT_INTERNAL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the last SIM switch procedure status
 *
 * @return
 *      - An enum as defined in SimSwitchStatus_t
 */
//--------------------------------------------------------------------------------------------------
SimSwitchStatus_t GetLastSimSwitchStatus
(
    void
)
{
    return SimHandlerPtr->status;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set SIM mode
 *
 * @return
 *      - LE_OK if the treatment succeeds
 *      - LE_BAD_PARAMETER if an invalid SIM mode is provided
 *      - LE_FAULT if the treatment fails
 *      - LE_BUSY if the is an ongoing SIM mode switch
 */
//--------------------------------------------------------------------------------------------------
le_result_t SetSimMode
(
    SimMode_t simMode    ///< [IN] New SIM mode to be applied
)
{
    if ((simMode >= MODE_MAX) || (simMode <= MODE_IN_PROGRESS))
    {
        LE_ERROR("Invalid SIM mode provided: %d", simMode);
        return LE_BAD_PARAMETER;
    }

    // Mode 3 will be implemented later
    if (simMode == MODE_PREF_EXTERNAL_SIM)
    {
        LE_ERROR("Mode 3 is not supported");
        return LE_FAULT;
    }

    SimHandlerPtr->mode = GetCurrentSimMode();

    if (SimHandlerPtr->mode == MODE_IN_PROGRESS)
    {
        LE_WARN("Mode switch in progress");
        return LE_FAULT;
    }

    if (SimHandlerPtr->mode == simMode)
    {
        LE_INFO("Mode already enabled");
        return LE_OK;
    }

    le_timer_Stop(ModeRollbackTimer);
    le_timer_Restart(ModeExecTimer);

    SimHandlerPtr->modeRequest = simMode;
    SimHandlerPtr->rollbackRequest = false;
    SimHandlerPtr->previousMode = SimHandlerPtr->mode;
    SimHandlerPtr->mode = MODE_IN_PROGRESS;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the resources needed for the SIM mode switch component.
 *
 * @return
 *      - LE_OK if the treatment succeeds
 *      - LE_DUPLICATE if the resources are already initialized
 */
//--------------------------------------------------------------------------------------------------
le_result_t SimModeInit
(
    void
)
{
    if (SimHandlerPtr->isInit)
    {
        return LE_DUPLICATE;
    }

    // Initialize SIM mode execution timer. Upon timer expiration, the device attempts a switch to
    // the new SIM according to the last command received.
    ModeExecTimer = le_timer_Create("ModeExecTimer");
    le_timer_SetMsInterval(ModeExecTimer, MODE_EXEC_TIMER_DELAY);
    le_timer_SetRepeat(ModeExecTimer, 1);
    le_timer_SetHandler(ModeExecTimer, SimModeExecHandler);

    // Initialize SIM rollback timer. Upon timer expiration, the device rollbacks to the previous
    // SIM mode configuration.
    ModeRollbackTimer = le_timer_Create("ModeRollbackTimer");
    le_timer_SetMsInterval(ModeRollbackTimer, MODE_ROLLBACK_TIMER_DELAY);
    le_timer_SetRepeat(ModeRollbackTimer, 1);
    le_timer_SetHandler(ModeRollbackTimer, SimModeRollbackHandler);

    // Register a handler for Cellular Network Service state changes.
    CellNetStateEventRef = le_cellnet_AddStateEventHandler(CellNetStateHandler, NULL);

    // Register a handler for AVC events
    AvcStatusEventRef  = le_avc_AddStatusEventHandler(AvcStatusHandler, NULL);

    // Get the current SIM mode
    SimHandlerPtr->mode = GetCurrentSimMode();

    SimHandlerPtr->isInit = true;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Free the resources used for the SIM mode switch component.
 */
//--------------------------------------------------------------------------------------------------
void SimModeDeinit
(
    void
)
{
    if (!SimHandlerPtr->isInit)
    {
        // SIM mode already deinitialized. Nothing to do
        return;
    }

    // Remove timers
    le_timer_Delete(ModeExecTimer);
    le_timer_Delete(ModeRollbackTimer);

    // Remove event handlers
    le_cellnet_RemoveStateEventHandler(CellNetStateEventRef);
    le_avc_RemoveStatusEventHandler(AvcStatusEventRef);

    SimHandlerPtr->isInit = false;
}
