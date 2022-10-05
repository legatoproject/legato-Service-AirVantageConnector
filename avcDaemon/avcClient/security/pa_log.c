/**
 * @file pa_log.c
 *
 * This logger for the Linux platform simply prints messages out to standard error.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

//#include "keyStore.h"


//--------------------------------------------------------------------------------------------------
/**
 * Log an error message.
 */
//--------------------------------------------------------------------------------------------------
void pa_Error
(
    const char*     msg             ///< [IN] Error message to log.
)
{
    //fprintf(stderr, "%s\n", msg);
}

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
)
{
    return -1;
}

//--------------------------------------------------------------------------------------------------
/**
 * restore errno
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_RestoreErrno
(
    int saved_errno                 ///< [IN] errno to restore
)
{
    //errno = saved_errno;
}
