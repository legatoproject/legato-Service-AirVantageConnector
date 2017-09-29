/**
 * @file osDebug.c
 *
 * Adaptation layer for debug
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * Define buffer length for data dump
 */
//--------------------------------------------------------------------------------------------------
#define DUMP_BUFFER_LEN 80

//--------------------------------------------------------------------------------------------------
/**
 * Define buffer length for log
 */
//--------------------------------------------------------------------------------------------------
#define LOG_BUFFER_LEN 255

//--------------------------------------------------------------------------------------------------
/**
 * Function for assert
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_Assert
(
    bool condition,             /// [IN] Condition to be checked
    const char* functionPtr,    /// [IN] Function name which calls the assert function
    uint32_t line               /// [IN] Function line which calls the assert function
)
{
    if (!(condition))
    {
        LE_FATAL("Assertion at function %s: line %d !!!!!!", functionPtr, line);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Macro for assertion
 */
//--------------------------------------------------------------------------------------------------
//#define OS_ASSERT(X) LE_ASSERT(X)

//--------------------------------------------------------------------------------------------------
/**
 * Adaptation function for log
 */
//--------------------------------------------------------------------------------------------------
void lwm2m_printf
(
    const char * format,
    ...
)
{
    va_list ap;
    int ret;
    static char strBuffer[LOG_BUFFER_LEN];
    memset(strBuffer, 0, LOG_BUFFER_LEN);

    va_start(ap, format);
    ret = vsnprintf(strBuffer, LOG_BUFFER_LEN, format, ap);
    va_end(ap);
    /* LOG and LOG_ARG macros sets <CR><LF> at the end: remove it */
    if (strBuffer[ret-1] == '\n')
    {
        if (strBuffer[ret-2] == '\r')
        {
             strBuffer[ret-2] = '\0';
        }
        else
        {
              strBuffer[ret-1] = '\0';
        }
    }
    LE_DEBUG("%s", strBuffer);
}

//--------------------------------------------------------------------------------------------------
/**
 * Adaptation function for log: dump data
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_DataDump
(
    char *descPtr,                  ///< [IN] data description
    void *addrPtr,                  ///< [IN] Data address to be dumped
    int len                         ///< [IN] Data length
)
{
    int i;
    unsigned char buffPtr[17];
    unsigned char *pcPtr = (unsigned char*)addrPtr;
    static char strBuffer[DUMP_BUFFER_LEN];
    memset(strBuffer, 0, DUMP_BUFFER_LEN);


    // Output description if given.
    if (NULL != descPtr)
    {
        LE_DEBUG("%s:", descPtr);
    }

    if (NULL == addrPtr)
    {
        LE_DEBUG("NULL");
        return;
    }

    if (0 == len)
    {
        LE_DEBUG("  ZERO LENGTH");
        return;
    }

    if (0 > len)
    {
        LE_DEBUG("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++)
    {
        // Multiple of 16 means new line (with line offset).
        if ((i % 16) == 0)
        {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
            {
                snprintf(strBuffer + strlen(strBuffer),
                         DUMP_BUFFER_LEN - strlen(strBuffer),
                         "  %s", buffPtr);
                LE_DEBUG("%s", strBuffer);
                memset(strBuffer, 0, DUMP_BUFFER_LEN);
            }

            // Output the offset.
            snprintf(strBuffer + strlen(strBuffer),
                     DUMP_BUFFER_LEN - strlen(strBuffer),
                     "  %04x ", i);
        }
        // Now the hex code for the specific character.
        snprintf(strBuffer + strlen(strBuffer),
                 DUMP_BUFFER_LEN - strlen(strBuffer),
                 " %02x", pcPtr[i]);

        // And store a printable ASCII character for later.
        if ((pcPtr[i] < 0x20) || (pcPtr[i] > 0x7e))
        {
            buffPtr[i % 16] = '.';
        }
        else
        {
            buffPtr[i % 16] = pcPtr[i];
        }
        buffPtr[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0)
    {
        snprintf(strBuffer + strlen (strBuffer), DUMP_BUFFER_LEN - strlen(strBuffer), "   ");
        i++;
    }

    // And print the final ASCII bit.
    snprintf(strBuffer + strlen (strBuffer),
             DUMP_BUFFER_LEN - strlen(strBuffer),
             "  %s", buffPtr);
    LE_DEBUG("%s", strBuffer);
}

