/**
 * @file osPortDevice.c
 *
 * Porting layer for device parameters
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "osPortDevice.h"
#include "assetData.h"
#include "avcUpdateShared.h" // For MAX_VERSION_STR_BYTES
#include <sys/utsname.h>

//--------------------------------------------------------------------------------------------------
/**
 * Define for LK version length
 */
//--------------------------------------------------------------------------------------------------
#define LK_VERSION_LENGTH 10

//--------------------------------------------------------------------------------------------------
/**
 *                  OBJECT 3: DEVICE
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device manufacturer
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portDeviceManufacturer
(
    char *bufferPtr,                        ///< [INOUT] data buffer
    size_t *lenPtr                          ///< [INOUT] length of input buffer and length of the
                                            ///< returned data
)
{
    lwm2mcore_sid_t result;

    if ((bufferPtr == NULL) || (lenPtr == NULL))
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        le_result_t leresult = le_info_GetManufacturerName ((char*)bufferPtr, (uint32_t) *lenPtr);

        switch (leresult)
        {
            case LE_OK:
                result = LWM2MCORE_ERR_COMPLETED_OK;
                break;

            case LE_OVERFLOW:
                result = LWM2MCORE_ERR_OVERFLOW;
                break;

            case LE_FAULT:
            default:
                result = LWM2MCORE_ERR_GENERAL_ERROR;
                break;
        }
    }
    LE_INFO("os_portDeviceManufacturer result %d", result);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device model number
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portDeviceModelNumber
(
    char *bufferPtr,                        ///< [INOUT] data buffer
    size_t *lenPtr                          ///< [INOUT] length of input buffer and length of the
                                            ///< returned data
)
{
    lwm2mcore_sid_t result;

    if ((bufferPtr == NULL) || (lenPtr == NULL))
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        le_result_t leresult = le_info_GetDeviceModel ((char*)bufferPtr, (uint32_t) *lenPtr);

        switch (leresult)
        {
            case LE_OVERFLOW:
                result = LWM2MCORE_ERR_OVERFLOW;
                break;

            case LE_OK:
                result = LWM2MCORE_ERR_COMPLETED_OK;
                break;

            case LE_FAULT:
            default:
                result = LWM2MCORE_ERR_GENERAL_ERROR;
                break;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device serial number
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portDeviceSerialNumber
(
    char *bufferPtr,                        ///< [INOUT] data buffer
    size_t *lenPtr                          ///< [INOUT] length of input buffer and length of the
                                            ///< returned data
)
{
    lwm2mcore_sid_t result;

    if ((bufferPtr == NULL) || (lenPtr == NULL))
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        le_result_t leresult = le_info_GetPlatformSerialNumber ((char*)bufferPtr,
                                                                (uint32_t) *lenPtr);

        switch (leresult)
        {
            case LE_OVERFLOW:
                result = LWM2MCORE_ERR_OVERFLOW;
                break;

            case LE_OK:
                result = LWM2MCORE_ERR_COMPLETED_OK;
                break;

            case LE_FAULT:
            default:
                result = LWM2MCORE_ERR_GENERAL_ERROR;
                break;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
 /**
 *  Path to the file that stores the Legato version number string.
 */
//--------------------------------------------------------------------------------------------------
#define LEGATO_VERSION_FILE "/legato/systems/current/version"



//--------------------------------------------------------------------------------------------------
/**
 *  Attempt to read the Legato version string from the file system.
 */
//--------------------------------------------------------------------------------------------------
static void GetLegatoVersion
(
    char* versionBufferPtr          ///< [INOUT] Buffer to hold the string.
)
{
    FILE* versionFilePtr = NULL;
    LE_INFO("Read the Legato version string.");

    do
    {
        versionFilePtr = fopen(LEGATO_VERSION_FILE, "r");
    }
    while (   (NULL != versionFilePtr)
           && (EINTR == errno));

    if (versionFilePtr == NULL)
    {
        LE_INFO("Could not open Legato version file.");
        return;
    }

    if (fgets(versionBufferPtr, MAX_VERSION_STR_BYTES, versionFilePtr) != NULL)
    {
        char* newLine = strchr(versionBufferPtr, '\n');

        if (newLine != NULL)
        {
            *newLine = '\0';
        }

        LE_INFO("The current Legato framework version is, '%s'.", versionBufferPtr);
    }
    else
    {
        LE_INFO("Could not read Legato version.");
    }

    int retVal;

    do
    {
        retVal = fclose(versionFilePtr);
    }
    while (   (-1 == retVal)
           && (EINTR == errno));
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the firmware version
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portDeviceFirmwareVersion
(
    char* bufferPtr,                        ///< [INOUT] data buffer
    size_t* lenPtr                          ///< [INOUT] length of input buffer and length of the
                                            ///< returned data
)
{
    lwm2mcore_sid_t result;

    if ((bufferPtr == NULL) || (lenPtr == NULL))
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        char* tokenPtr;
        char* tmpBufferPtr;
        uint32_t length;
        struct utsname linuxInfo;
        size_t read;
        char* save1Ptr;
        char* save2Ptr;
        FILE* fpPtr;
        le_result_t leresult;

        char tmpFwBufferPtr[512];

        strncpy(bufferPtr, "MDM_", strlen ("MDM_"));

        leresult = le_info_GetFirmwareVersion(tmpFwBufferPtr, sizeof(tmpFwBufferPtr));
        if (leresult == LE_OK)
        {
            tmpBufferPtr = strtok_r(tmpFwBufferPtr, " ", &save1Ptr);
            strncat(bufferPtr, tmpBufferPtr, strlen(tmpBufferPtr));

            LE_INFO("bufferPtr: %s", bufferPtr);

            fpPtr = fopen("/proc/cmdline", "r");
            if (fpPtr == NULL)
            {
                LE_ERROR("Can't read LK version");
            }
            read = getline(&tmpBufferPtr, &length, fpPtr);
            if (read == -1)
            {
                LE_ERROR("Can't read LK version");
            }

            LE_INFO("/proc/cmdline: %s", tmpBufferPtr);

            tokenPtr = strtok_r(tmpBufferPtr, " ", &save2Ptr);

            /* walk through other tokens */
            while( tokenPtr != NULL )
            {
               LE_INFO("tokenPtr=  %s", tokenPtr );
               tokenPtr = strtok_r(NULL, " ", &save2Ptr);

               if (0 == strncmp(tokenPtr, "lkversion=", LK_VERSION_LENGTH))
               {
                   tokenPtr += LK_VERSION_LENGTH;
                   break;
               }
            }

            fclose(fpPtr);

            if (tokenPtr == NULL)
            {
                LE_ERROR("Can't read LK version");
            }

            strncat(bufferPtr, "_LK_", strlen("_LK_"));
            strncat(bufferPtr, tokenPtr, strlen(tokenPtr));
            LE_INFO("bufferPtr = %s", bufferPtr);

            if (0 ==  uname(&linuxInfo))
            {
                LE_INFO("Linux Version: %s", linuxInfo.release);
            }

            strncat(bufferPtr, "_OS_", strlen("_OS_"));
            strncat(bufferPtr, linuxInfo.release, strlen(linuxInfo.release));

            LE_INFO("bufferPtr = %s", bufferPtr);

            strncat(bufferPtr, "_RFS_", strlen("_RFS_"));
            strncat(bufferPtr, "unknown", strlen("unknown"));

            strncat(bufferPtr, "_UFS_", strlen("_UFS_"));
            strncat(bufferPtr, "unknown", strlen("unknown"));

            LE_INFO("bufferPtr = %s", bufferPtr);

            strncat(bufferPtr, "_LE_", strlen("_LE_"));

            GetLegatoVersion(tmpFwBufferPtr);
            LE_INFO("Legato version = %s", tmpFwBufferPtr);
            LE_INFO("fw version = %s", bufferPtr);
            strncat(bufferPtr, tmpFwBufferPtr, strlen(tmpFwBufferPtr));

            LE_INFO("bufferPtr = %s", bufferPtr);


            strncat(bufferPtr, "_PRI_", strlen("_PRI_"));

            char priIdPn[LE_INFO_MAX_PRIID_PN_BYTES];
            char priIdRev[LE_INFO_MAX_PRIID_REV_BYTES];

            leresult = le_info_GetPriId (priIdPn,
                                         LE_INFO_MAX_PRIID_PN_BYTES,
                                         priIdRev,
                                         LE_INFO_MAX_PRIID_REV_BYTES);
            if (leresult == LE_OK)
            {
                LE_INFO("le_info_GetPriId get priIdPn => %s", priIdPn);
                LE_INFO("le_info_GetPriId get priIdRev => %s", priIdRev);
            }

            strncat(bufferPtr, priIdPn, strlen(priIdPn));
            strncat(bufferPtr, "-", strlen("-"));
            strncat(bufferPtr, priIdRev, strlen(priIdRev));

            *lenPtr = strlen(bufferPtr);
            result = LWM2MCORE_ERR_COMPLETED_OK;
        }
        else
        {
            result = LWM2MCORE_ERR_GENERAL_ERROR;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device time
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treament succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_sid_t os_portDeviceCurrentTime
(
    uint64_t* valuePtr                      ///< [INOUT] data buffer
)
{
    lwm2mcore_sid_t result;

    if (valuePtr == NULL)
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        le_clk_Time_t t = le_clk_GetAbsoluteTime();
        *valuePtr = 0;
        LE_INFO("time %d", t.sec);
        if (0 == t.sec)
        {
            result = LWM2MCORE_ERR_GENERAL_ERROR;
        }
        else
        {
            *valuePtr = t.sec;
            result = LWM2MCORE_ERR_COMPLETED_OK;
        }
    }
    return result;
}

