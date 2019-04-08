/**
 * @file osPlatform.c
 *
 * Adaptation layer for platform
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include <liblwm2m.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>


//--------------------------------------------------------------------------------------------------
/**
 * Largest block of memory that can be allocated by LWM2M
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_MEM_MAX   1100

//--------------------------------------------------------------------------------------------------
/**
 * Number of large blocks to allocate
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_MEM_MAX_COUNT 23

//--------------------------------------------------------------------------------------------------
/**
 * Big block of memory that can be allocated by LWM2M (le_coap API)
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_MEM_BIG   512

//--------------------------------------------------------------------------------------------------
/**
 * Number of large blocks to allocate
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_MEM_BIG_COUNT 45

//--------------------------------------------------------------------------------------------------
/**
 * Medium block of memory that can be allocated by LWM2M
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_MEM_MED 100

//--------------------------------------------------------------------------------------------------
/**
 * Number of medium blocks to allocate
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_MEM_MED_COUNT 140

//--------------------------------------------------------------------------------------------------
/**
 * Small block of memory that can be allocated by LWM2M
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_MEM_SMALL 30

//--------------------------------------------------------------------------------------------------
/**
 * Number of small blocks to allocate
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_MEM_SMALL_COUNT 50


//--------------------------------------------------------------------------------------------------
/**
 * Memory pool for LWM2M memory allocation.
 */
//--------------------------------------------------------------------------------------------------
le_mem_PoolRef_t Lwm2mPool = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static memory pool for LWM2M memory allocation -- reduces fragmentation.
 */
//--------------------------------------------------------------------------------------------------
LE_MEM_DEFINE_STATIC_POOL(Lwm2mPool, LWM2M_MEM_MAX_COUNT, LWM2M_MEM_MAX);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize memory areas for Lwm2m
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_InitMem
(
    void
)
{
    if (!Lwm2mPool)
    {
        le_mem_PoolRef_t outerPool =
            le_mem_InitStaticPool(Lwm2mPool, LWM2M_MEM_MAX_COUNT, LWM2M_MEM_MAX);

        Lwm2mPool = le_mem_CreateReducedPool(
            le_mem_CreateReducedPool(
                le_mem_CreateReducedPool(
                    outerPool,
                    "Lwm2mBigPool",
                    LWM2M_MEM_BIG_COUNT,
                    LWM2M_MEM_BIG),
                "Lwm2mMedPool",
                LWM2M_MEM_MED_COUNT,
                LWM2M_MEM_MED),
            "Lwm2mSmallPool",
            LWM2M_MEM_SMALL_COUNT,
            LWM2M_MEM_SMALL);
    }
}

#ifndef LWM2M_MEMORY_TRACE

//--------------------------------------------------------------------------------------------------
/**
 * Memory allocation with trace
 *
 * @return
 *  - memory address
 */
//--------------------------------------------------------------------------------------------------
void* lwm2m_malloc
(
    size_t size     ///< [IN] Memory size to be allocated
)
{
    void* mem = le_mem_ForceVarAlloc(Lwm2mPool, size);

    return mem;
}

//--------------------------------------------------------------------------------------------------
/**
 * Memory free
 */
//--------------------------------------------------------------------------------------------------
void lwm2m_free
(
    void* ptr   ///< [IN] Memory address to release
)
{
    if (ptr)
    {
        le_mem_Release(ptr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Duplicate a string
 *
 * @return
 *  - Duplicated string address
 */
//--------------------------------------------------------------------------------------------------
char* lwm2m_strdup
(
    const char* strPtr  ///< [IN] String to be duplicated
)
{
    size_t strSize = strlen(strPtr) + 1;

    char* dupPtr = le_mem_ForceVarAlloc(Lwm2mPool, strSize);
    memcpy(dupPtr, strPtr, strSize);

    return dupPtr;
}

#endif

//--------------------------------------------------------------------------------------------------
/**
 * Compare strings
 *
 * @return
 *  - integer less than, equal to, or greater than zero if s1Ptr (or the first n bytes thereof) is
 *    found, respectively, to be less than, to  match,  or  begreater than s2Ptr.
 */
//--------------------------------------------------------------------------------------------------
int lwm2m_strncmp
(
    const char* s1Ptr,  ///< [IN] First string to be compared
    const char* s2Ptr,  ///< [IN] Second string to be compared
    size_t n            ///< [IN] Comparison length
)
{
    return strncmp(s1Ptr, s2Ptr, n);
}

//--------------------------------------------------------------------------------------------------
/**
 * Memory reallocation
 *
 * @return
 *  - memory address
 */
//--------------------------------------------------------------------------------------------------
void* lwm2mcore_realloc
(
    void*  ptr,         ///< [IN] Data address
    size_t newSize      ///< [IN] New size allocation
)
{
    if (!ptr)
    {
        return lwm2m_malloc(newSize);
    }
    else if (newSize == 0)
    {
        lwm2m_free(ptr);
        return NULL;
    }

    size_t blkSize = le_mem_GetBlockSize(ptr);

    if (newSize <= blkSize)
    {
        return ptr;
    }
    else
    {
        void* newPtr = lwm2m_malloc(newSize);

        memcpy(newPtr, ptr, blkSize);

        lwm2m_free(ptr);

        return newPtr;
    }
}
