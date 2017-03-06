/**
 * @file packageDownloaderUpdateInfo.c
 *
 * Package downloader update information
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "osPortUpdate.h"
#include "packageDownloaderUpdateInfo.h"

//--------------------------------------------------------------------------------------------------
// Static functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Function to read a package downloader update information file from platform memory
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Incorrect parameter provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ReadPkgDwlUpdateInfoFile
(
    char* namePtr,          ///< [IN] File name
    uint8_t* bufferPtr,     ///< [INOUT] data buffer
    size_t* lenPtr          ///< [INOUT] length of input buffer
)
{
    if (   (!namePtr) || (!bufferPtr) || (!lenPtr)
        || (strlen(namePtr) > LE_FS_PATH_MAX_LEN)
       )
    {
        return LE_BAD_PARAMETER;
    }

    le_result_t result;
    le_fs_FileRef_t fileRef;

    result = le_fs_Open(namePtr, LE_FS_RDONLY, &fileRef);
    if (LE_OK != result)
    {
        LE_ERROR("Error while opening file %s", namePtr);
        return LE_FAULT;
    }

    result = le_fs_Read(fileRef, bufferPtr, lenPtr);
    if (LE_OK != result)
    {
        le_fs_Close(fileRef);

        LE_ERROR("Error while reading file %s", namePtr);
        return LE_FAULT;
    }

    le_fs_Close(fileRef);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to write a the package downloader update information file in platform memory
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Incorrect parameter provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WritePkgDwlUpdateInfoFile
(
    char* namePtr,          ///< [IN] File name
    uint8_t* bufferPtr,     ///< [IN] data buffer
    size_t len              ///< [IN] length of input buffer
)
{
    if (   (!namePtr) || (!bufferPtr)
        || (strlen(namePtr) > LE_FS_PATH_MAX_LEN)
       )
    {
        return LE_BAD_PARAMETER;
    }

    le_result_t result;
    le_fs_FileRef_t fileRef;

    result = le_fs_Open(namePtr, LE_FS_CREAT | LE_FS_WRONLY, &fileRef);
    if (LE_OK != result)
    {
        LE_ERROR("Error while opening file %s", namePtr);
        return LE_FAULT;
    }

    result = le_fs_Write(fileRef, bufferPtr, len);
    if (LE_OK != result)
    {
        le_fs_Close(fileRef);

        LE_ERROR("Error while writing file %s", namePtr);
        return LE_FAULT;
    }

    le_fs_Close(fileRef);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
// Public functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Set FW update result
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateResult
(
    lwm2mcore_fwUpdateResult_t fwUpdateResult   ///< [IN] New FW update result
)
{
    return WritePkgDwlUpdateInfoFile(FW_UPDATE_RESULT_FILENAME,
                                     (uint8_t*)&fwUpdateResult,
                                     sizeof(lwm2mcore_fwUpdateResult_t));
}

//--------------------------------------------------------------------------------------------------
/**
 * Set FW update state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateState
(
    lwm2mcore_fwUpdateState_t fwUpdateState     ///< [IN] New FW update state
)
{
    return WritePkgDwlUpdateInfoFile(FW_UPDATE_STATE_FILENAME,
                                     (uint8_t*)&fwUpdateState,
                                     sizeof(lwm2mcore_fwUpdateState_t));
}

//--------------------------------------------------------------------------------------------------
/**
 * Set SW update result
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetSwUpdateResult
(
    lwm2mcore_swUpdateResult_t swUpdateResult   ///< [IN] New SW update result
)
{
    return WritePkgDwlUpdateInfoFile(SW_UPDATE_RESULT_FILENAME,
                                     (uint8_t*)&swUpdateResult,
                                     sizeof(lwm2mcore_swUpdateResult_t));
}

//--------------------------------------------------------------------------------------------------
/**
 * Set SW update state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetSwUpdateState
(
    lwm2mcore_swUpdateState_t swUpdateState     ///< [IN] New SW update state
)
{
    return WritePkgDwlUpdateInfoFile(SW_UPDATE_STATE_FILENAME,
                                     (uint8_t*)&swUpdateState,
                                     sizeof(lwm2mcore_swUpdateState_t));
}

//--------------------------------------------------------------------------------------------------
/**
 * Get FW update result
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateResult
(
    lwm2mcore_fwUpdateResult_t* fwUpdateResultPtr   ///< [INOUT] FW update result
)
{
    if (!fwUpdateResultPtr)
    {
        return LE_BAD_PARAMETER;
    }

    le_result_t result;
    size_t fileLen = sizeof(lwm2mcore_fwUpdateResult_t);
    result = ReadPkgDwlUpdateInfoFile(FW_UPDATE_RESULT_FILENAME,
                                      (uint8_t*)fwUpdateResultPtr,
                                      &fileLen);

    // An error occurred, set the default value
    if ((LE_OK != result) || (sizeof(lwm2mcore_fwUpdateResult_t) != fileLen))
    {
        *fwUpdateResultPtr = LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get FW update state
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateState
(
    lwm2mcore_fwUpdateState_t* fwUpdateStatePtr     ///< [INOUT] FW update state
)
{
    if (!fwUpdateStatePtr)
    {
        return LE_BAD_PARAMETER;
    }

    le_result_t result;
    size_t fileLen = sizeof(lwm2mcore_fwUpdateState_t);
    result = ReadPkgDwlUpdateInfoFile(FW_UPDATE_STATE_FILENAME,
                                      (uint8_t*)fwUpdateStatePtr,
                                      &fileLen);

    // An error occurred, set the default value
    if ((LE_OK != result) || (sizeof(lwm2mcore_fwUpdateState_t) != fileLen))
    {
        *fwUpdateStatePtr = LWM2MCORE_FW_UPDATE_STATE_IDLE;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get SW update result
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetSwUpdateResult
(
    lwm2mcore_swUpdateResult_t* swUpdateResultPtr   ///< [INOUT] SW update result
)
{
    if (!swUpdateResultPtr)
    {
        return LE_BAD_PARAMETER;
    }

    le_result_t result;
    size_t fileLen = sizeof(lwm2mcore_swUpdateResult_t);
    result = ReadPkgDwlUpdateInfoFile(SW_UPDATE_RESULT_FILENAME,
                                      (uint8_t*)swUpdateResultPtr,
                                      &fileLen);

    // An error occurred, set the default value
    if ((LE_OK != result) || (sizeof(lwm2mcore_swUpdateResult_t) != fileLen))
    {
        *swUpdateResultPtr = LWM2MCORE_SW_UPDATE_RESULT_INITIAL;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get SW update state
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetSwUpdateState
(
    lwm2mcore_swUpdateState_t* swUpdateStatePtr     ///< [INOUT] SW update state
)
{
    if (!swUpdateStatePtr)
    {
        return LE_BAD_PARAMETER;
    }

    le_result_t result;
    size_t fileLen = sizeof(lwm2mcore_swUpdateState_t);
    result = ReadPkgDwlUpdateInfoFile(SW_UPDATE_STATE_FILENAME,
                                      (uint8_t*)swUpdateStatePtr,
                                      &fileLen);

    // An error occurred, set the default value
    if ((LE_OK != result) || (sizeof(lwm2mcore_swUpdateState_t) != fileLen))
    {
        *swUpdateStatePtr = LWM2MCORE_SW_UPDATE_STATE_INITIAL;
    }

    return LE_OK;
}
