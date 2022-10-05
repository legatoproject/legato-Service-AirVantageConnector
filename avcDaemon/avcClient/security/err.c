/**
 * @file err.c
 *
 * IOT Key Store error handling routines.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include <stdio.h>
#include <stdarg.h>

#include "pa_log.h"
#include "err.h"


//--------------------------------------------------------------------------------------------------
/**
 * Max message size.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_MSG_SIZE            300


//--------------------------------------------------------------------------------------------------
/**
 * Prints an error message and appends some debug information.
 */
//--------------------------------------------------------------------------------------------------
void _err_Print
(
    const char* filenamePtr,
    const unsigned int lineNumber,
    const char* formatPtr,
    ...
)
{
    // Save the current errno because some of the system calls below may change errno.
    int savedErrno = pa_GetErrno();

    // Build the user message.
    char userMsg[MAX_MSG_SIZE] = "";

    va_list varParams;
    va_start(varParams, formatPtr);

    // Reset the errno to ensure that we report the proper errno value.
    pa_RestoreErrno(savedErrno);

    // Don't need to check the return value because if there is an error we can't do anything about
    // it.  If there was a truncation then that'll just show up in the message.
    vsnprintf(userMsg, sizeof(userMsg), formatPtr, varParams);

    va_end(varParams);

    // Build the final log message.
    char msg[500] = "";

    // Don't need to check the return value because if there is an error we can't do anything about
    // it.  If there was a truncation then that'll just show up in the log message.
    snprintf(msg, sizeof(msg), "%s %u | %s", filenamePtr, lineNumber, userMsg);

    // Call the PA's error logging system.
    pa_Error(msg);
}
