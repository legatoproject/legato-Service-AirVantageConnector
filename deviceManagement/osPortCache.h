/**
 * @file osPortCache.h
 *
 * Header file for variables that are cached.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
#ifndef OS_PORT_CACHE_INCLUDE_GUARD
#define OS_PORT_CACHE_INCLUDE_GUARD

//--------------------------------------------------------------------------------------------------
/**
 * Define for unknown version
 */
//--------------------------------------------------------------------------------------------------
#define UNKNOWN_VERSION     "unknown"

//--------------------------------------------------------------------------------------------------
/**
 * Define for FW version buffer length
 */
//--------------------------------------------------------------------------------------------------
#define FW_BUFFER_LENGTH    512

//--------------------------------------------------------------------------------------------------
/**
 * Attempt to write the new LK version string into cache.
 *
 * @return
 *      - LE_OK when successfully written to LkVersionCache.
 *      - LE_FAULT otherwise.
 */
//--------------------------------------------------------------------------------------------------
le_result_t osPortDevice_SetLkVersion
(
    const char *newLkVersion
);

#endif //OS_PORT_CACHE_INCLUDE_GUARD
