/**
 * @file osPortDataChannel.c
 *
 * AVC data channel management
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

/* include files */
#include <stdbool.h>
#include <stdint.h>
#include "legato.h"
#include "interfaces.h"
#include "osPortDataChannel.h"
#include "avcDataChannel.h"

//--------------------------------------------------------------------------------------------------
/**
 * Function to retrieve from the config tree AVC's dedicated data channel and only support
 * technology ethernet. There can only be one configured. An example is:
 *
 * root@swi-mdm9x28-wp:~# config get apps/avcService/avcClient/dataChannel/dedicated/
 * dedicated/
 *   tech<string> == ethernet
 *   name<string> == eth0
 *
 * @return
 *     - LE_BAD_PARAMETER: invalid input argument
 *     - LE_OK: a configured dedicated data channel successfully retrieved
 *     - LE_NOT_FOUND: no dedicated data channel found configured
 */
//--------------------------------------------------------------------------------------------------
static le_result_t osPortGetDedicatedDataChannel
(
    le_dcs_Technology_t *tech,   ///< [OUT] technology type of the dedicated data channel
    char* channelName            ///< [OUT] name of the channel
)
{
    char cfgStr[LE_CFG_STR_LEN_BYTES] = {0};

    le_cfg_IteratorRef_t cfg;

    if (!tech || !channelName)
    {
        LE_DEBUG("Invalid input for retrieving the dedicated data channel's info");
        return LE_BAD_PARAMETER;
    }

    *tech = LE_DCS_TECH_UNKNOWN;
    channelName[0] = '\0';

    // Retrieve the tech type of the dedicated data channel

    cfg = le_cfg_CreateReadTxn(LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TREE_ROOT);
    if (!le_cfg_NodeExists(cfg, LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH_NODE)
        || (le_cfg_GetString(cfg, LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH_NODE, cfgStr,
                             LE_CFG_STR_LEN_BYTES-1, "") != LE_OK)
        || (strncmp(cfgStr, LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH,
                    strlen(LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH)) != 0))
    {
        LE_DEBUG("Found dedicated data channel tech not ethernet");
        le_cfg_CancelTxn(cfg);
        return LE_NOT_FOUND;
    }

    // Retrieve the name of the dedicated data channel
    if ((le_cfg_GetString(cfg, LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_NAME_NODE, channelName,
                          LE_CFG_STR_LEN_BYTES-1, "") != LE_OK) || (strlen(channelName) == 0))
    {
        LE_DEBUG("Found no configured dedicated data channel name");
        le_cfg_CancelTxn(cfg);
        return LE_NOT_FOUND;
    }

    le_cfg_CancelTxn(cfg);
    *tech = LE_DCS_TECH_ETHERNET;
    LE_INFO("Found dedicated data channel %s of tech type %d", channelName, *tech);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the le_data technology type of the currently connected data connection
 *
 * @return
 *     - the currently connection connection's le_data technology type
 *     - LE_DATA_MAX if not connected.
 */
//--------------------------------------------------------------------------------------------------
le_data_Technology_t osPortGetConnectedTech
(
    bool leDataConnected
)
{
    le_dcs_Technology_t channelTech;
    char channelName[LE_CFG_STR_LEN_BYTES];
    if (osPortGetDedicatedDataChannel(&channelTech, channelName) == LE_OK)
    {
        le_dcs_State_t state;
        char ifName[LE_DCS_INTERFACE_NAME_MAX_LEN+1];
        le_dcs_ChannelRef_t channelRef = le_dcs_GetReference(channelName, channelTech);
        if (LE_OK != le_dcs_GetState(channelRef, &state, ifName, LE_DCS_INTERFACE_NAME_MAX_LEN)
            || (state != LE_DCS_STATE_UP))
        {
            return LE_DATA_MAX;
        }

        if (channelTech == LE_DCS_TECH_ETHERNET)
        {
            return LE_DATA_ETHERNET_EXT;
        }
        return LE_DATA_MAX;
    }
    return leDataConnected ? le_data_GetTechnology() : LE_DATA_MAX;
}
