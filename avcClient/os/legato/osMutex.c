/**
 * @file osMutex.c
 *
 * Adaptation layer for mutex
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * Function to create a mutex
 *
 * @return:
 *   - 0 on success; error code otherwise
 */
//--------------------------------------------------------------------------------------------------
void* lwm2mcore_MutexCreate
(
    const char* mutexNamePtr              ///< mutex name
)
{
    return (void*) le_mutex_CreateNonRecursive(mutexNamePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to lock a mutex
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_MutexLock
(
    void* mutexPtr                  ///< [IN] mutex
)
{
    le_mutex_Lock((le_mutex_Ref_t)mutexPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to unlock a mutex
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_MutexUnlock
(
    void* mutexPtr                  ///< [IN] mutex
)
{
    le_mutex_Unlock((le_mutex_Ref_t)mutexPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to delete a mutex
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_MutexDelete
(
    void* mutexPtr                  ///< [IN] mutex
)
{
    le_mutex_Delete((le_mutex_Ref_t)mutexPtr);
}
