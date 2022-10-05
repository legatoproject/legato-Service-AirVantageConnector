/**
 * @file pa_log.h
 *
 * Platform adapter's logging interface.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef IOT_KEY_STORE_PA_LOG_INCLUDE_GUARD
#define IOT_KEY_STORE_PA_LOG_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Log an error message.
 */
//--------------------------------------------------------------------------------------------------
void pa_Error
(
    const char*     msg             ///< [IN] Error message to log.
);

//--------------------------------------------------------------------------------------------------
/**
 * get errno
 *
 * @return
 *      errno
 */
//--------------------------------------------------------------------------------------------------
int pa_GetErrno
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * restore errno
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_RestoreErrno
(
    int saved_errno                 ///< [IN] errno to restore
);

#endif // IOT_KEY_STORE_PA_LOG_INCLUDE_GUARD
