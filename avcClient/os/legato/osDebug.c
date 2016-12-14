/**
 * @file osDebug.c
 *
 * Adaptation layer for debug
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */

#include "legato.h"
#include "dtls_debug.h"

//--------------------------------------------------------------------------------------------------
/**
 * Function for assert
 */
//--------------------------------------------------------------------------------------------------
void os_assert
(
    bool condition,         /// [IN] Condition to be checked
    char* functionPtr,      /// [IN] Function name which calls the assert function
    uint32_t line           /// [IN] Function line which calls the assert function
)
{
    if (!(condition))
    {
        LE_FATAL ("Assertion at function %s: line %d !!!!!!", functionPtr, line);
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
void lwm2m_printf(
    const char * format,
    ...
)
{
    va_list ap;
    int ret;
    static char strBuffer[128];
    memset (strBuffer, 0, 128);

    va_start (ap, format);
    ret = vsprintf (strBuffer,format, ap);
    va_end (ap);
    /* LOG and LOG_ARG macros sets <CR><LF> at the end: remove it */
    strBuffer[ret-2] = '\0';
    LE_INFO ((char*)strBuffer);
}


//--------------------------------------------------------------------------------------------------
/**
 * Adaptation function for log: dump data
 */
//--------------------------------------------------------------------------------------------------
void os_debug_data_dump(
    char *descPtr,                  ///< [IN] data description
    void *addrPtr,                  ///< [IN] Data address to be dumped
    int len                         ///< [IN] Data length
)
{
    int i;
    unsigned char buffPtr[17];
    unsigned char *pcPtr = (unsigned char*)addrPtr;
    static char strBuffer[80];
    memset (strBuffer, 0, 80);


    // Output description if given.
    if (descPtr != NULL)
    {
        LE_INFO ("%s:", descPtr);
    }

    if (len == 0)
    {
        LE_INFO ("  ZERO LENGTH");
        return;
    }

    if (len < 0)
    {
        LE_INFO ("  NEGATIVE LENGTH: %i\n",len);
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
                sprintf (strBuffer + strlen (strBuffer), "  %s", buffPtr);
                LE_INFO ((char*)strBuffer);
                memset (strBuffer, 0, 80);
            }

            // Output the offset.
            sprintf (strBuffer + strlen (strBuffer), "  %04x ", i);
        }
        // Now the hex code for the specific character.
        sprintf (strBuffer + strlen (strBuffer), " %02x", pcPtr[i]);

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
        //printf ("   ");
        sprintf (strBuffer + strlen (strBuffer), "   ");
        i++;
    }

    // And print the final ASCII bit.
    sprintf (strBuffer + strlen (strBuffer), "  %s", buffPtr);
    LE_INFO ((char*)strBuffer);
}

