/**
 * @file avc_portDevice.c
 *
 * Porting layer for device parameters
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "lwm2mcorePortDevice.h"

#include "../avcDaemon/avData.h"
#include "../avcDaemon/assetData.h"
#include "../avcAppUpdate/avcUpdateShared.h"
#include <sys/utsname.h>

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
    lwm2mcore_sid_t result = LWM2MCORE_ERR_OP_NOT_SUPPORTED;

    if ((bufferPtr == NULL) || (lenPtr == NULL))
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        le_result_t leresult = le_info_GetManufacturerName ((char*)bufferPtr, (uint32_t) *lenPtr);

        switch (leresult)
        {
            case LE_OVERFLOW:
            {
                result = LWM2MCORE_ERR_OVERFLOW;
            }
            break;

            case LE_FAULT:
            {
                result = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

            case LE_OK:
            {
                result = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

            default:
            {
                result = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;
        }
    }
    LE_INFO ("os_portDeviceManufacturer result %d", result);
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
    lwm2mcore_sid_t result = LWM2MCORE_ERR_OP_NOT_SUPPORTED;

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
            {
                result = LWM2MCORE_ERR_OVERFLOW;
            }
            break;

            case LE_FAULT:
            {
                result = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

            case LE_OK:
            {
                result = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

            default:
            {
                result = LWM2MCORE_ERR_GENERAL_ERROR;
            }
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
    lwm2mcore_sid_t result = LWM2MCORE_ERR_OP_NOT_SUPPORTED;

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
            {
                result = LWM2MCORE_ERR_OVERFLOW;
            }
            break;

            case LE_FAULT:
            {
                result = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

            case LE_OK:
            {
                result = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

            default:
            {
                result = LWM2MCORE_ERR_GENERAL_ERROR;
            }
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
    char *versionBuffer         ///< [INOUT] Buffer to hold the string.
)
{
    FILE* versionFile = NULL;
    LE_INFO ("Read the Legato version string.");

    do
    {
        versionFile = fopen(LEGATO_VERSION_FILE, "r");
    }
    while (   (versionFile != NULL)
           && (errno == EINTR));

    if (versionFile == NULL)
    {
        LE_INFO ("Could not open Legato version file.");
        return;
    }

    if (fgets(versionBuffer, MAX_VERSION_STR_BYTES, versionFile) != NULL)
    {
        char* newLine = strchr(versionBuffer, '\n');

        if (newLine != NULL)
        {
            *newLine = 0;
        }

        LE_INFO ("The current Legato framework version is, '%s'.", versionBuffer);
    }
    else
    {
        LE_INFO ("Could not read Legato version.");
    }

    int retVal = -1;

    do
    {
        retVal = fclose(versionFile);
    }
    while (   (retVal == -1)
           && (errno == EINTR));
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
    char *bufferPtr,                        ///< [INOUT] data buffer
    size_t *lenPtr                          ///< [INOUT] length of input buffer and length of the
                                            ///< returned data
)
{
    lwm2mcore_sid_t result = LWM2MCORE_ERR_OP_NOT_SUPPORTED;

    if ((bufferPtr == NULL) || (lenPtr == NULL))
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
#if 0
        le_result_t leresult = le_info_GetFirmwareVersion ((char*)bufferPtr, (uint32_t) *lenPtr);

        switch (leresult)
        {
            case LE_OVERFLOW:
            {
                result = LWM2MCORE_ERR_OVERFLOW;
            }
            break;

            case LE_FAULT:
            {
                result = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;

            case LE_OK:
            {
                result = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

            default:
            {
                result = LWM2MCORE_ERR_GENERAL_ERROR;
            }
            break;
        }
#endif

        char* token;
        char* tmp_buffer_ptr;
        uint32_t length;
        struct utsname linuxInfo;
        size_t read;
        FILE* fp;
        le_result_t leresult;

        char tmp_buffer[512];

        strcpy (bufferPtr, "MDM_");

        leresult = le_info_GetFirmwareVersion (tmp_buffer, sizeof(tmp_buffer));
        if (leresult == LE_OK)
        {
            tmp_buffer_ptr = strtok(tmp_buffer, " ");
            strcat(bufferPtr, tmp_buffer_ptr);

            LE_INFO ("bufferPtr: %s", bufferPtr);

            fp = fopen("/proc/cmdline", "r");
            if (fp == NULL)
                LE_ERROR("Can't read LK version");
            read = getline(&tmp_buffer_ptr, &length, fp);
            if (read == -1)
                LE_ERROR("Can't read LK version");

            LE_INFO ("/proc/cmdline: %s", tmp_buffer_ptr);

            token = strtok(tmp_buffer_ptr, " ");

            /* walk through other tokens */
            while( token != NULL )
            {
               LE_INFO ("token=  %s", token );
               token = strtok(NULL, " ");

               if (strncmp(token, "lkversion=", 10) == 0)
               {
                   token += 10;
                   break;
               }
            }

            fclose(fp);

            if (token == NULL)
                LE_ERROR("Can't read LK version");

            strcat(bufferPtr, "_LK_");
            strcat(bufferPtr, token);
            LE_INFO ("bufferPtr = %s", bufferPtr);

            if ( uname(&linuxInfo) == 0 )
            {
                LE_INFO ("Linux Version: %s", linuxInfo.release);
            }

            strcat(bufferPtr, "_OS_");
            strcat(bufferPtr, linuxInfo.release);

            LE_INFO ("bufferPtr = %s", bufferPtr);

            strcat(bufferPtr, "_RFS_");
            strcat(bufferPtr, "unknown");

            strcat(bufferPtr, "_UFS_");
            strcat(bufferPtr, "unknown");

            LE_INFO ("bufferPtr = %s", bufferPtr);

            strcat(bufferPtr, "_LE_");

            GetLegatoVersion (tmp_buffer);
            LE_INFO ("Legato version = %s", tmp_buffer);
            LE_INFO ("fw version = %s", bufferPtr);
            strcat(bufferPtr, tmp_buffer);

            LE_INFO ("bufferPtr = %s", bufferPtr);


            strcat(bufferPtr, "_PRI_");

            char priIdPn[LE_INFO_MAX_PRIID_PN_BYTES];
            char priIdRev[LE_INFO_MAX_PRIID_REV_BYTES];

            leresult = le_info_GetPriId (priIdPn,
                                         LE_INFO_MAX_PRIID_PN_BYTES,
                                         priIdRev,
                                         LE_INFO_MAX_PRIID_REV_BYTES);
            if (leresult == LE_OK)
            {
                LE_INFO ("le_info_GetPriId get priIdPn => %s", priIdPn);
                LE_INFO ("le_info_GetPriId get priIdRev => %s", priIdRev);
            }

            strcat(bufferPtr, priIdPn);
            strcat(bufferPtr, "-");
            strcat(bufferPtr, priIdRev);

            *lenPtr = strlen (bufferPtr);
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
    lwm2mcore_sid_t result = LWM2MCORE_ERR_OP_NOT_SUPPORTED;

    if (valuePtr == NULL)
    {
        result = LWM2MCORE_ERR_INVALID_ARG;
    }
    else
    {
        le_clk_Time_t time = le_clk_GetAbsoluteTime();
        *valuePtr = 0;
        LE_INFO ("time %d", time.sec);
        if (time.sec == 0)
        {
            result = LWM2MCORE_ERR_GENERAL_ERROR;
        }
        else
        {
            *valuePtr = time.sec;
            result = LWM2MCORE_ERR_COMPLETED_OK;
        }
    }
    return result;
}

