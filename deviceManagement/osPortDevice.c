/**
 * @file osPortDevice.c
 *
 * Porting layer for device parameters
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/device.h>
#include "legato.h"
#ifdef LE_CONFIG_LINUX
#include <sys/reboot.h>
#include <sys/utsname.h>
#endif
#include "interfaces.h"
#include "assetData.h"
#include "avcAppUpdate.h"
#include "avcServer.h"
#include "avcSim.h"

#include "avcClient.h"
#include "clientConfig.h"
#include "osPortCache.h"

//--------------------------------------------------------------------------------------------------
/**
 * Set tag delimiters for version string based on platform requirements.
 */
//--------------------------------------------------------------------------------------------------
#if MK_CONFIG_AVC_VERSION_TAG_UNDERSCORE
#   define FIRST_DELIM(t)       t "_"
#   define DELIM(t)         "_" t "_"
#else
#   define FIRST_DELIM(t)       t "="
#   define DELIM(t)         "," t "="
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Define for modem tag in FW version string
 */
//--------------------------------------------------------------------------------------------------
#define MODEM_TAG           FIRST_DELIM("MDM")

//--------------------------------------------------------------------------------------------------
/**
 * Define for LK tag in FW version string
 */
//--------------------------------------------------------------------------------------------------
#define LK_TAG              DELIM("LK")

//--------------------------------------------------------------------------------------------------
/**
 * Define for modem tag in Linux version string
 */
//--------------------------------------------------------------------------------------------------
#define LINUX_TAG           DELIM("OS")

//--------------------------------------------------------------------------------------------------
/**
 * Define for root FS tag in FW version string
 */
//--------------------------------------------------------------------------------------------------
#define ROOT_FS_TAG         DELIM("RFS")

//--------------------------------------------------------------------------------------------------
/**
 * Define for user FS tag in FW version string
 */
//--------------------------------------------------------------------------------------------------
#define USER_FS_TAG         DELIM("UFS")

//--------------------------------------------------------------------------------------------------
/**
 * Define for Legato baseline tag in FW version string
 */
//--------------------------------------------------------------------------------------------------
#define LEGATO_TAG          DELIM("LE")

//--------------------------------------------------------------------------------------------------
/**
 * Define for Legato override tag in FW version string
 */
//--------------------------------------------------------------------------------------------------
#define LEGATO_OVERRIDE_TAG DELIM("LEO")

//--------------------------------------------------------------------------------------------------
/**
 * Define for Customer PRI tag in FW version string (per AirVantage bundle packages specification)
 */
//--------------------------------------------------------------------------------------------------
#define CUSTOMER_PRI_TAG    DELIM("CUPRI")

//--------------------------------------------------------------------------------------------------
/**
 * Define for Carrier PRI tag in FW version string
 */
//--------------------------------------------------------------------------------------------------
#define CARRIER_PRI_TAG     DELIM("CAPRI")

//--------------------------------------------------------------------------------------------------
/**
 * Define for MCU tag in FW version string
 */
//--------------------------------------------------------------------------------------------------
#define MCU_TAG             DELIM("MCU")

//--------------------------------------------------------------------------------------------------
/**
 * Define for TEE (Trusted Execution Environment) tag in FW version string
 */
//--------------------------------------------------------------------------------------------------
#define TEE_TAG             DELIM("TEE")

//--------------------------------------------------------------------------------------------------
 /**
 *  Path to the file that stores the Legato baseline version number string.
 */
//--------------------------------------------------------------------------------------------------
#define LEGATO_BASELINE_VERSION_FILE "/mnt/legato/system/version"

//--------------------------------------------------------------------------------------------------
 /**
 *  Path to the file that stores the Legato override version number string.
 */
//--------------------------------------------------------------------------------------------------
#define LEGATO_OVERRIDE_VERSION_FILE "/legato/systems/current/version"

//--------------------------------------------------------------------------------------------------
 /**
 *  Path to the file that stores the root FS version number string.
 */
//--------------------------------------------------------------------------------------------------
#define RFS_VERSION_FILE "/etc/rootfsver.txt"

//--------------------------------------------------------------------------------------------------
 /**
 *  Path to the file that stores the user FS version number string.
 */
//--------------------------------------------------------------------------------------------------
#define UFS_VERSION_FILE "/opt/userfsver.txt"

//--------------------------------------------------------------------------------------------------
 /**
 *  Define of space
 */
//--------------------------------------------------------------------------------------------------
#define SPACE " "


//--------------------------------------------------------------------------------------------------
/**
 * Function pointer to get a component version
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
typedef size_t (*getVersion_t)
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t lenPtr                   ///< [IN] Buffer length
);

//--------------------------------------------------------------------------------------------------
/**
 * Structure to get a component version and its corresponding tag for the FW version string
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char* tagPtr;               ///< Component tag
    getVersion_t funcPtr;       ///< Function to read the component version
}
ComponentVersion_t;

#ifndef LE_CONFIG_CUSTOM_OS
//--------------------------------------------------------------------------------------------------
/**
 * Declare LkVersionCache used to store the current LK Version.
 */
//--------------------------------------------------------------------------------------------------
static char LkVersionCache[FW_BUFFER_LENGTH] = UNKNOWN_VERSION;
#endif //LE_CONFIG_CUSTOM_OS

//--------------------------------------------------------------------------------------------------
/**
 * Convert le_power power source enum type to lwm2m enum type
 *
 * @return
 *      - LWM2M power source enum type is returned
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_powerSource_enum_t ConvertPowerSource
(
    le_power_PowerSource_t powerSource
)
{
    switch (powerSource)
    {
        case LE_POWER_DC_POWER:
            return LWM2MCORE_DEVICE_PWR_SRC_TYPE_DC_POWER;

        case LE_POWER_INTERNAL_BATTERY:
            return LWM2MCORE_DEVICE_PWR_SRC_TYPE_BAT_INT;

        case LE_POWER_EXTERNAL_BATTERY:
            return LWM2MCORE_DEVICE_PWR_SRC_TYPE_BAT_EXT;

        case LE_POWER_UNDEFINED:
            return LWM2MCORE_DEVICE_PWR_SRC_TYPE_UNUSED;

        case LE_POWER_POE:
            return LWM2MCORE_DEVICE_PWR_SRC_TYPE_PWR_OVER_ETH;

        case LE_POWER_USB:
            return LWM2MCORE_DEVICE_PWR_SRC_TYPE_USB;

        case LE_POWER_AC_POWER:
            return LWM2MCORE_DEVICE_PWR_SRC_TYPE_AC_POWER;

        case LE_POWER_SOLAR:
            return LWM2MCORE_DEVICE_PWR_SRC_TYPE_SOLAR;

        default:
            return LWM2MCORE_DEVICE_PWR_SRC_TYPE_DC_POWER;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert le_power battery status enum type to lwm2m enum type
 *
 * @return
 *      - LWM2M power source enum type is returned
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_batteryStatus_enum_t ConvertBatteryStatus
(
    le_power_BatteryStatus_t batteryStatus
)
{
    switch (batteryStatus)
    {
        case LE_POWER_NORMAL:
            return LWM2MCORE_DEVICE_BATTERY_NORMAL;

        case LE_POWER_CHARGING:
            return LWM2MCORE_DEVICE_BATTERY_CHARGING;

        case LE_POWER_CHARGE_COMPLETE:
            return LWM2MCORE_DEVICE_BATTERY_CHARGE_COMPLETE;

        case LE_POWER_DAMAGED:
            return LWM2MCORE_DEVICE_BATTERY_DAMAGED;

        case LE_POWER_LOW:
            return LWM2MCORE_DEVICE_BATTERY_LOW;

        case LE_POWER_NOT_INSTALL:
            return LWM2MCORE_DEVICE_BATTERY_NOT_INSTALL;

        case LE_POWER_UNKNOWN:
        default:
            return LWM2MCORE_DEVICE_BATTERY_UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Attempt to read the Modem version string
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetModemVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;
    if (NULL != versionBufferPtr)
    {
        char tmpModemBufferPtr[FW_BUFFER_LENGTH];
        if (LE_OK == le_info_GetFirmwareVersion(tmpModemBufferPtr, FW_BUFFER_LENGTH))
        {
            char* savePtr;
            char* tmpBufferPtr = strtok_r(tmpModemBufferPtr, SPACE, &savePtr);
            if (NULL != tmpBufferPtr)
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", tmpBufferPtr);
            }
            else
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
            }
        }
        else
        {
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }
        LE_INFO("Modem version = %s, returnedLen %zd", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}

#ifndef LE_CONFIG_CUSTOM_OS

//--------------------------------------------------------------------------------------------------
/**
 * Attempt to write the new LK version string into cache.
 *
 * @return
 *      - LE_OK when successfully written to LkVersionCache.
 *      - LE_FAULT otherwise.
 */
//--------------------------------------------------------------------------------------------------
le_result_t osPortDevice_SetLkVersion
(
    const char *newLkVersion
)
{
    int bytesWritten = snprintf(LkVersionCache, sizeof(LkVersionCache), "%s", newLkVersion);
    if (bytesWritten <= 0)
    {
        LE_ERROR("Error while copying the newLkVersion (%m)");
        return LE_FAULT;
    }
    else if (bytesWritten >= sizeof(LkVersionCache))
    {
        LE_ERROR("Size of LkVersionCache buffer isn't large enough to hold the newLkVersion");
        return LE_FAULT;
    }
    else
    {
        LE_INFO("Successfully copied new LK version: %s", LkVersionCache);
    }
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Attempt to read the LK version string from the file system.
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetLkVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;
    // Check input parameters
    if (versionBufferPtr == NULL)
    {
        LE_ERROR("The versionBufferPtr is NULL");
    }
    else if (len == 0)
    {
        LE_ERROR("Buffer size is zero");
    }
    else
    {
        // We are reading directly from the cached value present when this component is
        // initialized, and the reason is because the new LK version will not be present
        // within /proc/cmdline in the event of firware update until the device is rebooted.
        returnedLen = snprintf(versionBufferPtr, len, "%s", LkVersionCache);
        LE_INFO("App Bootloader version %s, returnedLen %zd", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}
#endif

#ifndef LE_CONFIG_CUSTOM_OS
//--------------------------------------------------------------------------------------------------
/**
 * Attempt to read the Linux version string from the file system.
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetOsVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;
    if (NULL != versionBufferPtr)
    {
        struct utsname linuxInfo;
        if (0 == uname(&linuxInfo))
        {
            LE_INFO("Linux Version: %s", linuxInfo.release);
            returnedLen = snprintf(versionBufferPtr,
                                   len,
                                   linuxInfo.release,
                                   strlen(linuxInfo.release));
        }
        else
        {
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }
        LE_INFO("OsVersion %s, len %zd", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}
#endif

#ifndef LE_CONFIG_CUSTOM_OS
//--------------------------------------------------------------------------------------------------
/**
 * Attempt to read the root FS version string from the file system.
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetRfsVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;
    if (NULL != versionBufferPtr)
    {
        char tmpRfsBufferPtr[FW_BUFFER_LENGTH];
        FILE* fpPtr;
        fpPtr = fopen(RFS_VERSION_FILE, "r");
        if ((NULL != fpPtr)
         && (NULL != fgets(tmpRfsBufferPtr, FW_BUFFER_LENGTH, fpPtr)))
        {
            char* savePtr;
            char* tmpBufferPtr = strtok_r(tmpRfsBufferPtr, SPACE, &savePtr);
            if (NULL != tmpBufferPtr)
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", tmpBufferPtr);
            }
            else
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
            }
        }
        else
        {
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }

        if (NULL != fpPtr)
        {
            fclose(fpPtr);
        }
        LE_INFO("RfsVersion %s, len %zu", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}
#endif

#ifndef LE_CONFIG_CUSTOM_OS
//--------------------------------------------------------------------------------------------------
/**
 * Attempt to read the user FS version string from the file system.
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetUfsVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;
    if (NULL != versionBufferPtr)
    {
        char tmpUfsBufferPtr[FW_BUFFER_LENGTH];
        FILE* fpPtr;
        fpPtr = fopen(UFS_VERSION_FILE, "r");
        if ((NULL != fpPtr)
         && (NULL != fgets(tmpUfsBufferPtr, FW_BUFFER_LENGTH, fpPtr)))
        {
            char* savePtr;
            char* tmpBufferPtr = strtok_r(tmpUfsBufferPtr, SPACE, &savePtr);
            if (NULL != tmpBufferPtr)
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", tmpBufferPtr);
            }
            else
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
            }
        }
        else
        {
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }

        if (NULL != fpPtr)
        {
            fclose(fpPtr);
        }
        LE_INFO("UfsVersion %s, len %zd", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}
#endif

#ifndef MK_CONFIG_AVC_DISABLE_LEGATO_VERSION
//--------------------------------------------------------------------------------------------------
/**
 * Attempt to read the Legato version string from the file system.
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t ReadLegatoVersion
(
    const char* fileName,           ///< [IN] File name
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;
    if ((NULL != versionBufferPtr) && (NULL != fileName))
    {
#if LE_CONFIG_LINUX
        FILE* versionFilePtr = fopen(fileName, "r");
        if (NULL == versionFilePtr)
        {
            LE_INFO("Could not open Legato version file %s", fileName);
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
            return returnedLen;
        }

        char tmpLegatoVersionBuffer[MAX_VERSION_STR_BYTES];
        if (fgets(tmpLegatoVersionBuffer, MAX_VERSION_STR_BYTES, versionFilePtr) != NULL)
        {
            char* savePtr;
            char* tmpBufferPtr = strtok_r(tmpLegatoVersionBuffer, "-_", &savePtr);
            if (NULL != tmpBufferPtr)
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", tmpBufferPtr);
            }
            else
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
            }
        }
        else
        {
            LE_INFO("Could not read Legato version.");
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }
        fclose(versionFilePtr);
#else
        char tmpLegatoVersionBuffer[MAX_VERSION_STR_BYTES];
        snprintf(tmpLegatoVersionBuffer, MAX_VERSION_STR_BYTES, "%s", LE_VERSION);

        char* savePtr;
        char* tmpBufferPtr = strtok_r(tmpLegatoVersionBuffer, "-_", &savePtr);
        if (NULL != tmpBufferPtr)
        {
            returnedLen = snprintf(versionBufferPtr, len, "%s", tmpBufferPtr);
        }
        else
        {
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }

#endif //LE_CONFIG_LINUX
        LE_INFO("Legato version = %s, len %zd", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}
#endif //MK_CONFIG_AVC_DISABLE_LEGATO_VERSION

#ifndef MK_CONFIG_AVC_DISABLE_LEGATO_VERSION
//--------------------------------------------------------------------------------------------------
/**
 * Get the Legato baseline version string from the file system.
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetLegatoBaselineVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    return ReadLegatoVersion(LEGATO_BASELINE_VERSION_FILE, versionBufferPtr, len);
}
#endif

#ifndef LE_CONFIG_CUSTOM_OS
//--------------------------------------------------------------------------------------------------
/**
 * Get the Legato override version string from the file system.
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetLegatoOverrideVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    return ReadLegatoVersion(LEGATO_OVERRIDE_VERSION_FILE, versionBufferPtr, len);
}

//--------------------------------------------------------------------------------------------------
/**
 * Attempt to read the Customer PRI version string from the file system.
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetCustomerPriVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;
    if (NULL != versionBufferPtr)
    {
        char priIdPn[LE_INFO_MAX_PRIID_PN_BYTES];
        char priIdRev[LE_INFO_MAX_PRIID_REV_BYTES];

        if (LE_OK == le_info_GetPriId(priIdPn, LE_INFO_MAX_PRIID_PN_BYTES,
                                      priIdRev, LE_INFO_MAX_PRIID_REV_BYTES))
        {
            if (strlen(priIdPn) && strlen(priIdRev))
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s-%s", priIdPn, priIdRev);
            }
            else
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
            }
        }
        else
        {
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }
        LE_INFO("PriVersion %s, len %zu", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}
#endif

#ifndef MK_CONFIG_NO_CARRIER_PRI
//--------------------------------------------------------------------------------------------------
/**
 * Attempt to read the Carrier PRI version string from the file system.
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetCarrierPriVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;
    if (NULL != versionBufferPtr)
    {
        char priName[LE_INFO_MAX_CAPRI_NAME_BYTES];
        char priRev[LE_INFO_MAX_CAPRI_REV_BYTES];

        if (LE_OK == le_info_GetCarrierPri(priName, LE_INFO_MAX_CAPRI_NAME_BYTES,
                                           priRev, LE_INFO_MAX_CAPRI_REV_BYTES))
        {
            if (strlen(priName) && strlen(priRev))
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s-%s", priName, priRev);
            }
            else
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
            }
        }
        else
        {
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }
        LE_INFO("Carrier PRI Version %s, len %zd", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}
#endif

#ifndef MK_CONFIG_AVC_DISABLE_MCU_VERSION
//--------------------------------------------------------------------------------------------------
/**
 * Retrieve MCU version
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetMcuVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;

    if (NULL != versionBufferPtr)
    {
        char mcuVersion[LE_ULPM_MAX_VERS_LEN+1];
        if (LE_OK == le_ulpm_GetFirmwareVersion(mcuVersion, sizeof(mcuVersion)))
        {
            if (strlen(mcuVersion))
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", mcuVersion);
            }
            else
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
            }
        }
        else
        {
            LE_ERROR("Failed to retrieve MCU version");
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }
        LE_INFO("MCU version %s, len %zd", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}
#endif

#ifdef MK_CONFIG_AVC_ENABLE_TEE_VERSION
//--------------------------------------------------------------------------------------------------
/**
 * Retrieve TEE (Trusted Execution Environment) version
 *
 * @return
 *      - written buffer length
 */
//--------------------------------------------------------------------------------------------------
static size_t GetTeeVersion
(
    char* versionBufferPtr,         ///< [INOUT] Buffer to hold the string.
    size_t len                      ///< [IN] Buffer length
)
{
    size_t returnedLen = 0;

    if (NULL != versionBufferPtr)
    {
        char teeVersion[FW_BUFFER_LENGTH];
        if (LE_OK == le_info_GetTeeVersion(teeVersion, sizeof(teeVersion)))
        {
            if (strnlen(teeVersion, sizeof(teeVersion)))
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", teeVersion);
            }
            else
            {
                returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
            }
        }
        else
        {
            LE_ERROR("Failed to retrieve TEE version");
            returnedLen = snprintf(versionBufferPtr, len, "%s", UNKNOWN_VERSION);
        }
        LE_INFO("TEE version %s, len %zd", versionBufferPtr, returnedLen);
    }
    return returnedLen;
}
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device manufacturer
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetDeviceManufacturer
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_info_GetManufacturerName((char*)bufferPtr, (uint32_t)*lenPtr);

    switch (result)
    {
        case LE_OK:
            sID = LWM2MCORE_ERR_COMPLETED_OK;
            break;

        case LE_OVERFLOW:
            sID = LWM2MCORE_ERR_OVERFLOW;
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device model number
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetDeviceModelNumber
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_info_GetDeviceModel((char*)bufferPtr, (uint32_t)*lenPtr);

    switch (result)
    {
        case LE_OVERFLOW:
            sID = LWM2MCORE_ERR_OVERFLOW;
            break;

        case LE_OK:
            sID = LWM2MCORE_ERR_COMPLETED_OK;
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device serial number
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetDeviceSerialNumber
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_info_GetPlatformSerialNumber((char*)bufferPtr, (uint32_t)*lenPtr);

    switch (result)
    {
        case LE_OVERFLOW:
            sID = LWM2MCORE_ERR_OVERFLOW;
            break;

        case LE_OK:
            sID = LWM2MCORE_ERR_COMPLETED_OK;
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device firmware version
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetDeviceFirmwareVersion
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    char tmpBufferPtr[FW_BUFFER_LENGTH];
    uint32_t remainingLen = 0;
    size_t len;
    uint32_t i = 0;
    ComponentVersion_t versionInfo[] =
    {
      { MODEM_TAG,              GetModemVersion             },
#ifndef MK_CONFIG_AVC_DISABLE_LEGATO_VERSION
      { LEGATO_TAG,             GetLegatoBaselineVersion    },
#endif
#ifndef LE_CONFIG_CUSTOM_OS
      { LK_TAG,                 GetLkVersion                },
      { LINUX_TAG,              GetOsVersion                },
      { ROOT_FS_TAG,            GetRfsVersion               },
      { USER_FS_TAG,            GetUfsVersion               },
      { LEGATO_OVERRIDE_TAG,    GetLegatoOverrideVersion    },
      { CUSTOMER_PRI_TAG,       GetCustomerPriVersion       },
#endif
#ifndef MK_CONFIG_NO_CARRIER_PRI
      { CARRIER_PRI_TAG,        GetCarrierPriVersion        },
#endif
#ifndef MK_CONFIG_AVC_DISABLE_MCU_VERSION
      { MCU_TAG,                GetMcuVersion               },
#endif
#ifdef MK_CONFIG_AVC_ENABLE_TEE_VERSION
      { TEE_TAG,                GetTeeVersion             }
#endif
    };

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    remainingLen = *lenPtr;
    LE_DEBUG("remainingLen %"PRIu32, remainingLen);

    for (i = 0; i < NUM_ARRAY_MEMBERS(versionInfo); i++)
    {
        if (NULL != versionInfo[i].funcPtr)
        {
            len = versionInfo[i].funcPtr(tmpBufferPtr, FW_BUFFER_LENGTH);
            LE_DEBUG("len %zu - remainingLen %"PRIu32, len, remainingLen);
            /* len doesn't contain the final \0
             * remainingLen contains the final \0
             * So we have to keep one byte for \0
             */
            if (len > (remainingLen - 1))
            {
                *lenPtr = 0;
                bufferPtr[*lenPtr] = '\0';
                return LWM2MCORE_ERR_OVERFLOW;
            }
            else
            {
                snprintf(bufferPtr + strlen(bufferPtr),
                         remainingLen,
#ifdef AV_SYSTEM_CONFIGURATION
                         "%s",
#else
                         "%s%s",
                         versionInfo[i].tagPtr,
#endif
                         tmpBufferPtr);
                remainingLen -= len;
                LE_DEBUG("remainingLen %"PRIu32, remainingLen);
            }
        }
    }

    *lenPtr = strlen(bufferPtr);
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the available power source
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetAvailablePowerInfo
(
    lwm2mcore_powerInfo_t* powerInfoPtr,  ///< [INOUT] power source list
    size_t* powerNbPtr                    ///< [INOUT] power source number
)
{
    uint8_t index = 0;
    le_power_PowerInfo_t powerInfo[CONN_MONITOR_AVAIL_POWER_SOURCE_MAX_NB] = { { .voltage = 0 } };

    if (!powerInfoPtr || !powerNbPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK != le_power_GetPowerInfo(powerInfo, powerNbPtr))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    for (index = 0; index < *powerNbPtr; ++index)
    {
        powerInfoPtr[index].source  = ConvertPowerSource(powerInfo[index].source);
        powerInfoPtr[index].voltage = powerInfo[index].voltage;
        powerInfoPtr[index].current = powerInfo[index].current;
        powerInfoPtr[index].level   = powerInfo[index].level;
        powerInfoPtr[index].status  = ConvertBatteryStatus(powerInfo[index].status);
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the battery level (percentage)
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetBatteryLevel
(
    uint8_t* valuePtr  ///< [INOUT] data buffer
)
{
    le_ips_PowerSource_t powerSource;
    uint8_t batteryLevel;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK != le_ips_GetPowerSource(&powerSource))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    // Get the battery level only if the device is powered by a battery
    if (LE_IPS_POWER_SOURCE_BATTERY != powerSource)
    {
        LE_DEBUG("Device is not powered by a battery");
        return LWM2MCORE_ERR_INVALID_STATE;
    }

    if (LE_OK != le_ips_GetBatteryLevel(&batteryLevel))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("Battery level: %d%%", batteryLevel);
    *valuePtr = batteryLevel;

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device time (UNIX time in seconds)
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetDeviceCurrentTime
(
    uint64_t* valuePtr  ///< [INOUT] data buffer
)
{
    le_clk_Time_t t;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    t = le_clk_GetAbsoluteTime();
    *valuePtr = t.sec;
    LE_DEBUG("time %ld", (long)t.sec);

    if (0 == t.sec)
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the module identity (IMEI)
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetDeviceImei
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    char imei[LE_INFO_IMEI_MAX_BYTES];
    size_t imeiLen;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    memset(imei, 0, sizeof(imei));

    result = le_info_GetImei(imei, sizeof(imei));
    switch (result)
    {
        case LE_OK:
            imeiLen = strlen(imei);

            if (*lenPtr < imeiLen)
            {
                sID = LWM2MCORE_ERR_OVERFLOW;
            }
            else
            {
                memcpy(bufferPtr, imei, imeiLen);
                *lenPtr = imeiLen;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

        case LE_OVERFLOW:
            sID = LWM2MCORE_ERR_OVERFLOW;
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the SIM card identifier (ICCID)
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetIccid
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    le_sim_Id_t simId = le_sim_GetSelectedCard();
    char iccid[LE_SIM_ICCID_BYTES];
    size_t iccidLen;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Check if SIM card is present
    if (!le_sim_IsPresent(simId))
    {
        return LWM2MCORE_ERR_INVALID_STATE;
    }

    memset(iccid, 0, sizeof(iccid));

    result = le_sim_GetICCID(simId, iccid, sizeof(iccid));
    switch (result)
    {
        case LE_OK:
            iccidLen = strlen(iccid);

            if (*lenPtr < iccidLen)
            {
                sID = LWM2MCORE_ERR_OVERFLOW;
            }
            else
            {
                memcpy(bufferPtr, iccid, iccidLen);
                *lenPtr = iccidLen;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

        case LE_OVERFLOW:
            sID = LWM2MCORE_ERR_OVERFLOW;
            break;

        case LE_BAD_PARAMETER:
            sID = LWM2MCORE_ERR_INVALID_ARG;
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}


//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the subscription identity (MEID/ESN/IMSI)
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetSubscriptionIdentity
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    le_mrc_Rat_t currentRat;
    char imsi[LE_SIM_IMSI_BYTES];
    char esn[LE_INFO_MAX_ESN_BYTES];
    char meid[LE_INFO_MAX_MEID_BYTES];
    size_t imsiLen, esnLen, meidLen;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_DATA_CELLULAR != le_data_GetTechnology())
    {
        return LWM2MCORE_ERR_INVALID_STATE;
    }

    if (LE_OK != le_mrc_GetRadioAccessTechInUse(&currentRat))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    memset(imsi, 0, sizeof(imsi));
    memset(esn, 0, sizeof(esn));
    memset(meid, 0, sizeof(meid));

    // MEID and ESN are used in CDMA systems while IMSI is used in GSM/UMTS/LTE systems.
    if (LE_MRC_RAT_CDMA == currentRat)
    {
        // Try to retrieve the ESN first, then the MEID if the ESN is not available
        result = le_info_GetEsn(esn, sizeof(esn));
        switch (result)
        {
            case LE_OK:
                esnLen = strlen(esn);

                if (*lenPtr < esnLen)
                {
                    sID = LWM2MCORE_ERR_OVERFLOW;
                }
                else
                {
                    memcpy(bufferPtr, esn, esnLen);
                    *lenPtr = esnLen;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                }
                break;

            case LE_OVERFLOW:
                sID = LWM2MCORE_ERR_OVERFLOW;
                break;

            case LE_FAULT:
            default:
                sID = LWM2MCORE_ERR_GENERAL_ERROR;
                break;
        }

        // ESN not available, try to retrieve the MEID
        if (LWM2MCORE_ERR_COMPLETED_OK != sID)
        {
            result = le_info_GetMeid(meid, sizeof(meid));
            switch (result)
            {
                case LE_OK:
                    meidLen = strlen(meid);

                    if (*lenPtr < meidLen)
                    {
                        sID = LWM2MCORE_ERR_OVERFLOW;
                    }
                    else
                    {
                        memcpy(bufferPtr, meid, meidLen);
                        *lenPtr = meidLen;
                        sID = LWM2MCORE_ERR_COMPLETED_OK;
                    }
                    break;

                case LE_OVERFLOW:
                    sID = LWM2MCORE_ERR_OVERFLOW;
                    break;

                case LE_FAULT:
                default:
                    sID = LWM2MCORE_ERR_GENERAL_ERROR;
                    break;
            }
        }
    }
    else
    {
        le_sim_Id_t simId = le_sim_GetSelectedCard();

        // Check if SIM card is present
        if (!le_sim_IsPresent(simId))
        {
            return LWM2MCORE_ERR_INVALID_STATE;
        }

        // Retrieve the IMSI for GSM/UMTS/LTE
        result = le_sim_GetIMSI(simId, imsi, sizeof(imsi));
        switch (result)
        {
            case LE_OK:
                imsiLen = strlen(imsi);

                if (*lenPtr < imsiLen)
                {
                    sID = LWM2MCORE_ERR_OVERFLOW;
                }
                else
                {
                    memcpy(bufferPtr, imsi, imsiLen);
                    *lenPtr = imsiLen;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                }
                break;

            case LE_OVERFLOW:
                sID = LWM2MCORE_ERR_OVERFLOW;
                break;

            case LE_BAD_PARAMETER:
                sID = LWM2MCORE_ERR_INVALID_ARG;
                break;

            case LE_FAULT:
            default:
                sID = LWM2MCORE_ERR_GENERAL_ERROR;
                break;
        }
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the phone number (MSISDN)
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetMsisdn
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    le_sim_Id_t simId = le_sim_GetSelectedCard();
    char msisdn[LE_MDMDEFS_PHONE_NUM_MAX_BYTES];
    size_t msisdnLen;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Check if SIM card is present
    if (!le_sim_IsPresent(simId))
    {
        return LWM2MCORE_ERR_INVALID_STATE;
    }

    memset(msisdn, 0, sizeof(msisdn));

    result = le_sim_GetSubscriberPhoneNumber(simId, msisdn, sizeof(msisdn));
    switch (result)
    {
        case LE_OK:
            msisdnLen = strlen(msisdn);

            if (*lenPtr < msisdnLen)
            {
                sID = LWM2MCORE_ERR_OVERFLOW;
            }
            else
            {
                memcpy(bufferPtr, msisdn, msisdnLen);
                *lenPtr = msisdnLen;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

        case LE_OVERFLOW:
            sID = LWM2MCORE_ERR_OVERFLOW;
            break;

        case LE_BAD_PARAMETER:
            sID = LWM2MCORE_ERR_INVALID_ARG;
            break;

        case LE_FAULT:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the device temperature (in °C)
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetDeviceTemperature
(
    int32_t* valuePtr   ///< [INOUT] data buffer
)
{
    lwm2mcore_Sid_t sID;
    le_result_t result;
    le_temp_SensorRef_t sensorRef;
    int32_t temp;
    int i;
    // List of sensors classified by order of priority
    const char* sensorName[] = {"POWER_CONTROLLER", "POWER_AMPLIFIER"};

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    // Get the temperature sensor reference
    for (i = 0; i < NUM_ARRAY_MEMBERS(sensorName); i++)
    {
        sensorRef = le_temp_Request(sensorName[i]);
        if (sensorRef)
        {
            LE_INFO("Found sensor: %s", sensorName[i]);
            break;
        }
    }

    if (!sensorRef)
    {
        LE_WARN("No temperature sensor present in the current target");
        return LWM2MCORE_ERR_INVALID_STATE;
    }

    // Retrieve the temperature
    result = le_temp_GetTemperature(sensorRef, &temp);
    if (LE_OK == result)
    {
        *valuePtr = temp;
        sID = LWM2MCORE_ERR_COMPLETED_OK;
    }
    else
    {
        sID = LWM2MCORE_ERR_GENERAL_ERROR;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the number of unexpected resets
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetDeviceUnexpectedResets
(
    uint32_t* valuePtr  ///< [INOUT] data buffer
)
{
    uint64_t count;
    le_result_t result;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    result = le_info_GetUnexpectedResetsCount(&count);
    LE_DEBUG("le_info_GetUnexpectedResetsCount %d", result);
    if (LE_UNSUPPORTED == result)
    {
        return LWM2MCORE_ERR_INVALID_STATE;
    }
    else if (LE_OK != result)
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    *valuePtr = (uint32_t)count;

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the total number of resets
 * This API needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
LWM2MCORE_SHARED lwm2mcore_Sid_t lwm2mcore_GetDeviceTotalResets
(
    uint32_t* valuePtr  ///< [INOUT] data buffer
)
{
    uint64_t expected, unexpected;
    le_result_t resultExpected;
    le_result_t resultUnexpected;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    resultExpected = le_info_GetExpectedResetsCount(&expected);
    resultUnexpected = le_info_GetUnexpectedResetsCount(&unexpected);
    LE_DEBUG("le_info_GetExpectedResetsCount %d", resultExpected);
    LE_DEBUG("le_info_GetUnexpectedResetsCount %d", resultUnexpected);

    if ((LE_UNSUPPORTED == resultExpected) || (LE_UNSUPPORTED == resultUnexpected))
    {
        return LWM2MCORE_ERR_INVALID_STATE;
    }
    else if ((LE_OK != resultExpected) || (LE_OK != resultUnexpected))
    {
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    *valuePtr = (uint32_t)(expected + unexpected);

    return LWM2MCORE_ERR_COMPLETED_OK;
}
