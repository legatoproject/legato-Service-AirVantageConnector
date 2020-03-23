/**
 * @file tpfServer_stub.c
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "tpf/tpfServer.h"

//--------------------------------------------------------------------------------------------------
/**
 * Get TPF mode state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t tpfServer_GetTpfState
(
    bool* isTpfEnabledPtr                   ///< [OUT] true if third party FOTA service is activated
)
{
    if (!isTpfEnabledPtr)
    {
        return LE_BAD_PARAMETER;
    }
    *isTpfEnabledPtr = false;
    return LE_OK;
}
