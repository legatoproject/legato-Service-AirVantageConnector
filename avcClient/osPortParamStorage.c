/**
 * @file osPortParamStorage.c
 *
 * Porting layer for parameter storage in platform memory
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/paramStorage.h>
#include "avcFsConfig.h"
#include "avcFs.h"


//--------------------------------------------------------------------------------------------------
/**
 * Write parameter in platform memory
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetParam
(
    lwm2mcore_Param_t paramId,      ///< [IN] Parameter Id
    uint8_t* bufferPtr,             ///< [IN] data buffer
    size_t len                      ///< [IN] length of input buffer
)
{
    if ((LWM2MCORE_MAX_PARAM <= paramId) || (NULL == bufferPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    le_result_t result;
    int pathLen;
    char path[LE_FS_PATH_MAX_LEN];
    memset(path, 0, LE_FS_PATH_MAX_LEN);
    pathLen = snprintf(path, LE_FS_PATH_MAX_LEN, "%s/param%d", PKGDWL_LEFS_DIR, paramId);
    if (pathLen > LE_FS_PATH_MAX_LEN)
    {
        return LWM2MCORE_ERR_INCORRECT_RANGE;
    }

    result = WriteFs(path, bufferPtr, len);

    if (LE_OK == result)
    {
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
    else
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Read parameter from platform memory
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetParam
(
    lwm2mcore_Param_t paramId,      ///< [IN] Parameter Id
    uint8_t* bufferPtr,             ///< [INOUT] data buffer
    size_t* lenPtr                  ///< [INOUT] length of input buffer
)
{
    if ((LWM2MCORE_MAX_PARAM <= paramId) || (NULL == bufferPtr) || (NULL == lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    char path[LE_FS_PATH_MAX_LEN];
    le_result_t result;
    int pathLen;
    memset(path, 0, LE_FS_PATH_MAX_LEN);
    pathLen = snprintf(path, LE_FS_PATH_MAX_LEN, "%s/param%d", PKGDWL_LEFS_DIR, paramId);
    if (pathLen > LE_FS_PATH_MAX_LEN)
    {
        return LWM2MCORE_ERR_INCORRECT_RANGE;
    }

    result = ReadFs(path, bufferPtr, lenPtr);

    if (LE_OK == result)
    {
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
    else
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete parameter from platform memory
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_DeleteParam
(
    lwm2mcore_Param_t paramId       ///< [IN] Parameter Id
)
{
    if (LWM2MCORE_MAX_PARAM <= paramId)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    char path[LE_FS_PATH_MAX_LEN];
    le_result_t result;
    int pathLen;
    memset(path, 0, LE_FS_PATH_MAX_LEN);
    pathLen = snprintf(path, LE_FS_PATH_MAX_LEN, "%s/param%d", PKGDWL_LEFS_DIR, paramId);
    if (pathLen > LE_FS_PATH_MAX_LEN)
    {
        return LWM2MCORE_ERR_INCORRECT_RANGE;
    }

    result = DeleteFs(path);
    if (LE_OK == result)
    {
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
    else
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }
}

