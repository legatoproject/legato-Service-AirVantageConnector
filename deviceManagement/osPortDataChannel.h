/**
 * @file osPortDataChannel.h
 *
 * AVC data channel management (for internal use only)
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
#ifndef LEGATO_DM_OS_PORT_DATA_CHANNEL_INCLUDE_GUARD
#define LEGATO_DM_OS_PORT_DATA_CHANNEL_INCLUDE_GUARD

LE_SHARED le_data_Technology_t osPortGetConnectedTech
(
    bool leDataConnected
);

#endif // LEGATO_OS_PORT_DATA_CHANNEL_INCLUDE_GUARD
