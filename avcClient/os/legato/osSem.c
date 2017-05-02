/**
 * @file osSem.c
 *
 * Adaptation layer for semaphores
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * Function to initialize semaphores
 *
 * @return:
 *    - Reference to the created semaphore
 */
//--------------------------------------------------------------------------------------------------
void* lwm2mcore_SemCreate
(
    const char* name,               ///< [IN] Name of the semaphore
    int32_t initialCount            ///< [IN] initial number of semaphore
)
{
    return (void*)(le_sem_Create(name, initialCount));
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to post a semaphore
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_SemPost
(
    void* semaphorePtr              ///< [IN] Pointer to the semaphore.
)
{
    le_sem_Post((le_sem_Ref_t)semaphorePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to wait for a semaphore
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_SemWait
(
    void* semaphorePtr              ///< [IN] Pointer to the semaphore.
)
{
    le_sem_Wait((le_sem_Ref_t)semaphorePtr);

}

//--------------------------------------------------------------------------------------------------
/**
 * Function to delete a semaphore
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_SemDelete
(
    void* semaphorePtr              ///< [IN] Pointer to the semaphore.
)
{
    le_sem_Delete((le_sem_Ref_t)semaphorePtr);

}


