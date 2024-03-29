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
#ifdef LE_CONFIG_LINUX
static le_cellnet_StateEventHandlerRef_t CellNetStateEventRef;
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Reference to the AVC status event handler
 */
//--------------------------------------------------------------------------------------------------
static le_avc_StatusEventHandlerRef_t AvcStatusEventRef;

//--------------------------------------------------------------------------------------------------
/**
 * Reference to the SIM state changer event handler
 */
//--------------------------------------------------------------------------------------------------
static le_sim_NewStateHandlerRef_t SimStateEventRef;

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
#ifdef LE_CONFIG_LINUX
static void CellNetStateHandler
(
    le_cellnet_State_t state,       ///< [IN] Cellular network state
    void*              contextPtr   ///< [IN] Associated context pointer
)
{
    le_mdc_ProfileRef_t profileRef;
    le_result_t result = LE_FAULT;

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
                result = le_mdc_SetDefaultAPN(profileRef);
                if (result != LE_OK)
                {
                    if (result == LE_UNSUPPORTED)
                    {
                        LE_WARN("Default APN switching is unsupported");
                    }
                    else
                    {
                        LE_ERROR("Could not set default APN for the select SIM");
                    }
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
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for SIM states Notifications.
 */
//--------------------------------------------------------------------------------------------------
static void SimStateHandler
(
    le_sim_Id_t     simId,
    le_sim_States_t simState,
    void*           contextPtr
)
{
    if (!SimHandlerPtr->avcConnectionRequest)
    {
        return;
    }

    if (LE_SIM_ABSENT == simState)
    {
        LE_WARN("SIM card is absent. Perform a rollback");
        SimHandlerPtr->status = SIM_SWITCH_ERROR;
        SimModeRollback();
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
    le_sim_Id_t prevCard = le_sim_GetSelectedCard();

    le_avc_StopSession();

    // Disable automatic SIM selection if already enabled
    if (SimHandlerPtr->previousMode == MODE_PREF_EXTERNAL_SIM)
    {
        le_sim_SetAutomaticSelection(false);
    }

    // Select SIM card based on the requested mode
    switch (SimHandlerPtr->modeRequest)
    {
        case MODE_EXTERNAL_SIM:
            status = le_sim_SelectCard(LE_SIM_EXTERNAL_SLOT_1);
            break;

        case MODE_INTERNAL_SIM:
            status = le_sim_SelectCard(LE_SIM_EMBEDDED);
            break;

        case MODE_PREF_EXTERNAL_SIM:
            status = le_sim_SetAutomaticSelection(true);
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
        // Switching between automatic SIM selection and static SIM may keep the same SIM card
        // selected. In this case, request a connection to AVC server and exit.
        if (le_sim_GetSelectedCard() == prevCard)
        {
            if (LE_OK != le_avc_StartSession())
            {
                LE_ERROR("Unable to start AVC session");
                SimHandlerPtr->status = SIM_SWITCH_ERROR;
            }
            return;
        }

        // A new SIM card has been selected, wait for network attach and request AVC session
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

    le_sim_SimMode_t simMode = le_sim_GetSimMode();
    SimMode_t ret;
    switch (simMode)
    {
        case(LE_SIM_FORCE_EXTERNAL):
            ret = MODE_EXTERNAL_SIM;
            break;
        case(LE_SIM_FORCE_INTERNAL):
            ret = MODE_INTERNAL_SIM;
            break;
        case(LE_SIM_FORCE_REMOTE):
            ret = MODE_INTERNAL_SIM;
            break;
        case(LE_SIM_PREF_EXTERNAL):
            ret = MODE_PREF_EXTERNAL_SIM;
            break;
        default:
            LE_ERROR("Invalid Sim Mode returned when getting current sim mode");
            ret = MODE_MAX;
    }
    return ret;
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

    SimMode_t currentSimMode = GetCurrentSimMode();

    if (currentSimMode == MODE_IN_PROGRESS)
    {
        LE_WARN("Mode switch in progress");
        return LE_FAULT;
    }

    if (currentSimMode == simMode)
    {
        LE_INFO("Mode already enabled");
        return LE_OK;
    }

    le_timer_Stop(ModeRollbackTimer);
    le_timer_Restart(ModeExecTimer);

    SimHandlerPtr->modeRequest = simMode;
    SimHandlerPtr->rollbackRequest = false;
    SimHandlerPtr->previousMode = currentSimMode;
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

#ifdef LE_CONFIG_LINUX
    // Register a handler for Cellular Network Service state changes.
    CellNetStateEventRef = le_cellnet_AddStateEventHandler(CellNetStateHandler, NULL);
#endif
    // Register a handler for AVC events
    AvcStatusEventRef = le_avc_AddStatusEventHandler(AvcStatusHandler, NULL);

    // Register for SIM state changes
    SimStateEventRef = le_sim_AddNewStateHandler(SimStateHandler, NULL);

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
#ifdef LE_CONFIG_LINUX
    le_cellnet_RemoveStateEventHandler(CellNetStateEventRef);
#endif
    le_avc_RemoveStatusEventHandler(AvcStatusEventRef);
    le_sim_RemoveNewStateHandler(SimStateEventRef);

    SimHandlerPtr->isInit = false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set SIM APDU config.
 *
 * @return
 *      - LE_OK if the treatment succeeds
 *      - LE_FAULT if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcSim_SetSimApduConfig
(
    const uint8_t* bufferPtr,   ///< [IN] SIM APDU
    size_t length               ///< [IN] SIM APDU length
)
{
    LE_DEBUG("data length %" PRIuS, length);
    LE_DUMP(bufferPtr, length);

#if LE_CONFIG_AVC_FEATURE_EDM
    // Save to the config tree
    le_cfg_IteratorRef_t iteratorRef = le_cfg_CreateWriteTxn(LE_AVC_CONFIG_TREE_ROOT);

    le_cfg_SetBinary(iteratorRef, LE_AVC_CONFIG_SIM_APDU_PATH, (uint8_t *) bufferPtr, length);

    le_cfg_CommitTxn(iteratorRef);

    return LE_OK;
#else
    LE_ERROR("ConfigTree is not supported: SIM APDU config can't be stored");
    return LE_FAULT;
#endif
}

//--------------------------------------------------------------------------------------------------
/**
 * Execute the (previously set) SIM APDU config.
 *
 * @return
 *      - LE_OK if the treatment succeeds
 *      - LE_FAULT if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcSim_ExecuteSimApduConfig
(
    void
)
{
#if LE_CONFIG_AVC_FEATURE_EDM
    uint8_t readBuf[256] = {0};
    uint8_t byte = 0;
    le_result_t result = LE_FAULT;

    // Clear the APDU response
    avcSim_SetSimApduResponse(NULL, 0);

    // Read stored APDU from the config tree
    le_cfg_IteratorRef_t iteratorRef = le_cfg_CreateReadTxn(LE_AVC_CONFIG_TREE_ROOT);

    size_t readLen = sizeof(readBuf);
    le_cfg_GetBinary(iteratorRef, LE_AVC_CONFIG_SIM_APDU_PATH, readBuf, &readLen,
                     &byte, sizeof(byte));

    le_cfg_CancelTxn(iteratorRef);

    LE_DEBUG("Retrieved from ConfigTree: data len %" PRIuS, readLen);
    LE_DUMP(readBuf, readLen);

    // Send to FW
    le_sim_Id_t simId = le_sim_GetSelectedCard();

    uint8_t rspImsi[128];
    size_t rspImsiLen = sizeof(rspImsi);

    result = le_sim_SendApdu(simId,
                             readBuf,
                             readLen,
                             rspImsi,
                             &rspImsiLen);

    LE_DEBUG("SendApdu returned %s: len %" PRIuS, LE_RESULT_TXT(result), rspImsiLen);
    LE_DUMP(rspImsi, rspImsiLen);
    // Basic check of the response buffer (first 2 bytes to match the expected values)
    if (rspImsiLen >=2 && rspImsi[0] == 0x09 && rspImsi[1] == 0x00)
    {
        return LE_OK;
    }

    return result;
#else
    LE_ERROR("ConfigTree is not supported: SIM APDU config can't be executed");
    return LE_FAULT;
#endif
}

//--------------------------------------------------------------------------------------------------
/**
 * Set (store) the SIM APDU response.
 *
 * @return
 *      - LE_OK if the treatment succeeds
 *      - LE_FAULT if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcSim_SetSimApduResponse
(
    const uint8_t* bufferPtr,   ///< [IN] SIM APDU response. If NULL, delete the stored response.
    size_t length               ///< [IN] SIM APDU response length
)
{
    // Save to the config tree
    LE_DEBUG("data length %" PRIuS, length);
    if (bufferPtr)
    {
        LE_DUMP(bufferPtr, length);
    }

#if LE_CONFIG_AVC_FEATURE_EDM
    le_cfg_IteratorRef_t iteratorRef = le_cfg_CreateWriteTxn(LE_AVC_CONFIG_TREE_ROOT);

    if (bufferPtr)
    {
        le_cfg_SetBinary(iteratorRef, LE_AVC_CONFIG_SIM_APDU_RESP_PATH, bufferPtr, length);
    }
    else
    {
        le_cfg_DeleteNode(iteratorRef, LE_AVC_CONFIG_SIM_APDU_RESP_PATH);
    }
    le_cfg_CommitTxn(iteratorRef);

    return LE_OK;
#else
    LE_ERROR("ConfigTree is not supported: SIM APDU response can't be stored");
    return LE_FAULT;
#endif
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM APDU response.
 *
 * @return
 *      - LE_OK if the treatment succeeds
 *      - LE_OVERFLOW Supplied buffer was not large enough to hold the value.
 *      - LE_FAULT if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcSim_GetSimApduResponse
(
    uint8_t* bufferPtr, ///< [OUT] SIM APDU response
    size_t* lenPtr      ///< [INOUT] SIM APDU buffer size / response length
)
{
#if LE_CONFIG_AVC_FEATURE_EDM
    // Read the data from the config tree.
    // Normally, the APDU response would be written to the Config Tree by atAirVantage app,
    // after it's received from the Modem FW (via AT command).
    uint8_t byte = 0;

    le_cfg_IteratorRef_t iteratorRef = le_cfg_CreateReadTxn(LE_AVC_CONFIG_TREE_ROOT);

    le_result_t result = le_cfg_GetBinary(iteratorRef, LE_AVC_CONFIG_SIM_APDU_RESP_PATH,
                                          bufferPtr, lenPtr,
                                          &byte, 0);
    le_cfg_CancelTxn(iteratorRef);
    if (result != LE_OK)
    {
        LE_ERROR("Error reading APDU response %s", LE_RESULT_TXT(result));
        return result;
    }

    return LE_OK;
#else
    LE_UNUSED(bufferPtr);
    LE_UNUSED(lenPtr);
    LE_ERROR("ConfigTree is not supported: SIM APDU response can't be read");
    return LE_FAULT;
#endif
}
