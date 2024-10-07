/**
 * @file avcDataChannel.h
 *
 * AVC data channel management (for internal use only)
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef LEGATO_AVC_DATA_CHANNEL_INCLUDE_GUARD
#define LEGATO_AVC_DATA_CHANNEL_INCLUDE_GUARD

#define LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TREE_ROOT \
    "apps/avcService/avcClient/dataChannel/dedicated"
#define LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH_NODE "tech"
#define LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_NAME_NODE "name"
#define LE_AVC_CONFIG_DEDICATED_DATA_CHANNEL_TECH      "ethernet"


LE_SHARED le_result_t avcDataChannelGetDedicatedConfig
(
    le_dcs_Technology_t *tech,   ///< [OUT] technology type of the dedicated data channel
    char* channelName            ///< [OUT] name of the channel
);

#endif // LEGATO_AVC_DATA_CHANNEL_INCLUDE_GUARD
