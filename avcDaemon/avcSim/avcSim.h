/**
 * @file avcSim.h
 *
 * Header for SIM mode management.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef _AVCSIM_H
#define _AVCSIM_H

//--------------------------------------------------------------------------------------------------
/**
 * Enum of SIM modes
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    MODE_IN_PROGRESS = 0,       ///< SIM switch in progress
    MODE_EXTERNAL_SIM,          ///< Mode 1: The module uses only the external SIM
    MODE_INTERNAL_SIM,          ///< Mode 2: The module uses only the internal SIM
    MODE_PREF_EXTERNAL_SIM,     ///< Mode 3: The module uses the external SIM if inserted, otherwise
                                ///< internal SIM is used
    MODE_MAX                    ///< Modes count
}
SimMode_t;

//--------------------------------------------------------------------------------------------------
/**
 * Enum of selected SIM slot
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    SLOT_EXTERNAL = 1,          ///< External SIM
    SLOT_INTERNAL               ///< Internal SIM
}
SimSlot_t;

//--------------------------------------------------------------------------------------------------
/**
 * Enum of SIM switch status
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    SIM_NO_ERROR = 0,            ///< SIM switch succeeded
    SIM_SWITCH_ERROR,            ///< SIM card error: no SIM card detected or communication failure
    SIM_SWITCH_TIMEOUT           ///< SIM switch timeout
}
SimSwitchStatus_t;

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
);

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
);

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
);

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
    SimMode_t simMode    ///< [IN]    New SIM mode to be applied
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Free the resources used for the SIM mode switch component.
 */
//--------------------------------------------------------------------------------------------------
void SimModeDeinit
(
    void
);

#endif /* _AVCSIM_H */
