/**
 * @file err.h
 *
 * IOT Key Store error handling routines.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef IOT_KEY_STORE_ERROR_INCLUDE_GUARD
#define IOT_KEY_STORE_ERROR_INCLUDE_GUARD

/* Log level set to compile time and lower level logs will be
 * turned off to save memory. Examples:
 * level set to IKS_LOG_DEBUG, all logs turned on,
 *  - keystore image size 100KB
 * level set to IKS_LOG_ERR, debug_print turned off,
 *  - keystore image size 99KB
 * level set to IKS_LOG_CRIT, turning off logs
 *  - keystore image size 83KB
 */
#ifndef IKS_LOG_LEVEL
#define IKS_LOG_LEVEL     IKS_LOG_DEBUG
#endif

/* log levels simlar to kernel log level */
#define IKS_LOG_EMERG     0
#define IKS_LOG_ALERT     1
#define IKS_LOG_CRIT      2
#define IKS_LOG_ERR       3
#define IKS_LOG_WARNING   4
#define IKS_LOG_NOTICE    5
#define IKS_LOG_INFO      6
#define IKS_LOG_DEBUG     7


//--------------------------------------------------------------------------------------------------
/**
 * Internal functions.
 */
//--------------------------------------------------------------------------------------------------
void _err_Print
(
    const char* filenamePtr,
    const unsigned int lineNumber,
    const char* formatPtr,
    ...
) __attribute__ ((format (printf, 3, 4)));


//--------------------------------------------------------------------------------------------------
/**
 * use __FILENAME__ some __FILE__ contains full source file path and can be very long
 */
//--------------------------------------------------------------------------------------------------
#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Prints a debug message and appends some debug information.
 */
//--------------------------------------------------------------------------------------------------
#if (IKS_LOG_LEVEL >= IKS_LOG_DEBUG)
#define DEBUG_PRINT(formatString, ...) \
    do \
    { \
        _err_Print(__FILENAME__, __LINE__, formatString, ##__VA_ARGS__); \
    } while(0)
#else
#define DEBUG_PRINT(formatString, ...)
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Prints a warning message and appends some debug information.
 */
//--------------------------------------------------------------------------------------------------
#if (IKS_LOG_LEVEL >= IKS_LOG_WARNING)
#define WARNING_PRINT(formatString, ...) \
    do \
    { \
        _err_Print(__FILENAME__, __LINE__, formatString, ##__VA_ARGS__); \
    } while(0)
#else
#define WARNING_PRINT(formatString, ...)
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Prints an error message and appends some debug information.
 */
//--------------------------------------------------------------------------------------------------
#if (IKS_LOG_LEVEL >= IKS_LOG_ERR)
#define ERR_PRINT(formatString, ...) \
    do \
    { \
        _err_Print(__FILENAME__, __LINE__, formatString, ##__VA_ARGS__); \
    } while(0)
#else
#define ERR_PRINT(formatString, ...)
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Prints an emergency message and appends some debug information.
 */
//--------------------------------------------------------------------------------------------------
#if (IKS_LOG_LEVEL >= IKS_LOG_EMERG)
#define EMERG_PRINT(formatString, ...) \
    do \
    { \
        _err_Print(__FILENAME__, __LINE__, formatString, ##__VA_ARGS__); \
    } while(0)
#else
#define EMERG_PRINT(formatString, ...)
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Prints an error or debug message based on error code
 */
//--------------------------------------------------------------------------------------------------
#if (IKS_LOG_LEVEL >= IKS_LOG_DEBUG)
#define ERR_PRINT_IF(ret, formatString, ...) \
        DEBUG_PRINT(formatString, ##__VA_ARGS__)
#else
#define ERR_PRINT_IF(ret, formatString, ...) \
    do \
    { \
        if (ret) \
        { \
            ERR_PRINT(formatString, ##__VA_ARGS__); \
        } \
    } while(0)
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Prints an error message and exits.
 */
//--------------------------------------------------------------------------------------------------
#define FATAL_HALT(formatString, ...) \
    do \
    { \
        _err_Print(__FILENAME__, __LINE__, formatString, ##__VA_ARGS__); \
        exit(EXIT_FAILURE); \
    } while(0)


#endif // IOT_KEY_STORE_ERROR_INCLUDE_GUARD
