/**
 * @file avcDataChannel.c
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
#include "avcDataChannel.h"

//--------------------------------------------------------------------------------------------------
/**
 * Archive of the technology type, channel name and reference of AVC's dedicated data channel
 */
//--------------------------------------------------------------------------------------------------
static le_dcs_Technology_t DedicatedChannelTech = LE_DCS_TECH_UNKNOWN;
static le_dcs_ChannelRef_t DedicatedChannelRef = NULL;
static char DedicatedChannelName[LE_CFG_STR_LEN_BYTES] = {0};

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
le_result_t avcDataChannelGetDedicatedConfig
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

    // Retrieve LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH_NODE, check if ethernet
    cfg = le_cfg_CreateReadTxn(LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TREE_ROOT);
    if (!le_cfg_NodeExists(cfg, LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH_NODE)
        || (le_cfg_GetString(cfg, LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH_NODE, cfgStr,
                             LE_CFG_STR_LEN_BYTES-1, "") != LE_OK)
        || (strncmp(cfgStr, LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH,
                    strlen(LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH)) != 0))
    {
        LE_DEBUG("Found no configured dedicated data channel tech");
        le_cfg_CancelTxn(cfg);
        DedicatedChannelTech = LE_DCS_TECH_UNKNOWN;
        DedicatedChannelName[0] = '\0';
        DedicatedChannelRef = NULL;
        return LE_NOT_FOUND;
    }

    // Handle LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_NAME_NODE
    if ((le_cfg_GetString(cfg, LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_NAME_NODE, channelName,
                          LE_CFG_STR_LEN_BYTES-1, "") != LE_OK) || (strlen(channelName) == 0))
    {
        LE_DEBUG("Found no configured dedicated data channel name");
        le_cfg_CancelTxn(cfg);
        DedicatedChannelTech = LE_DCS_TECH_UNKNOWN;
        DedicatedChannelName[0] = '\0';
        DedicatedChannelRef = NULL;
        return LE_NOT_FOUND;
    }

    le_cfg_CancelTxn(cfg);
    DedicatedChannelTech = LE_DCS_TECH_ETHERNET;
    strcpy(DedicatedChannelName, channelName);
    DedicatedChannelRef = le_dcs_GetReference(DedicatedChannelName, DedicatedChannelTech);

    *tech = LE_DCS_TECH_ETHERNET;
    LE_INFO("Found dedicated data channel %s of tech type %d", DedicatedChannelName, DedicatedChannelTech);

    return LE_OK;
}
