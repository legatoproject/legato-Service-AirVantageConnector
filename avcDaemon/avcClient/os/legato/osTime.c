/**
 * @file osTime.c
 *
 * Adaptation layer for time
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <liblwm2m.h>
#include "legato.h"
#include "interfaces.h"
#include <lwm2mcore/lwm2mcore.h>
#include "dtlsConnection.h"
#include "clockTimeConfiguration.h"


//--------------------------------------------------------------------------------------------------
/**
 * Names of all clock sources in string format
 */
//--------------------------------------------------------------------------------------------------
static const char* ClockSourceTypeString[LE_CLKSYNC_CLOCK_SOURCE_MAX] = {"tp", "ntp", "gps"};


//--------------------------------------------------------------------------------------------------
/**
 * The boolean to reflect whether the Clock Service is currently running a system clock update
 * that might cause a clock change and jump in time.
 */
//--------------------------------------------------------------------------------------------------
static bool UpdateSystemClockInProgress = false;


//--------------------------------------------------------------------------------------------------
/**
 * Function to retrieve the device time
 *
 * @return
 *      - device time (UNIX time: seconds since January 01, 1970)
 */
//--------------------------------------------------------------------------------------------------
time_t lwm2m_gettime
(
    void
)
{
    le_clk_Time_t deviceTime = le_clk_GetAbsoluteTime();

    LE_DEBUG("Device time: %ld", deviceTime.sec);

    return deviceTime.sec;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to perform an immediate system clock update using the clock time value set on the config
 * tree.  Leave this value there not reset after use so that it can serve as a last resort clock
 * time more up-to-date than 1970/1/1 in case the device after a restart cannot succeed in any way
 * e.g. via QMI, TP, NTP, etc., to get the current clock time.
 * The use of this last-resort clock time hasn't been implemented yet, but will be soon under the
 * new Clock Service in Legato.
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_UpdateSystemClock
(
    void* connP
)
{
    dtls_Connection_t* connPtr = (dtls_Connection_t*)connP;
    le_cfg_IteratorRef_t cfg;
    le_result_t result;
    le_clk_Time_t t;
    time_t clockStamp;

    cfg = le_cfg_CreateReadTxn(LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE);
    if (!cfg)
    {
        LE_WARN("No clock stamp given to update the system clock");
        return;
    }
    if (!le_cfg_NodeExists(cfg, LE_CLKSYNC_CONFIG_NODE_SOURCE_AVC_TIMESTAMP))
    {
        LE_WARN("No clock stamp given to update the system clock");
        le_cfg_CancelTxn(cfg);
        return;
    }

    clockStamp = le_cfg_GetInt(cfg, LE_CLKSYNC_CONFIG_NODE_SOURCE_AVC_TIMESTAMP, 0);
    le_cfg_CancelTxn(cfg);

    if (clockStamp <= 0)
    {
        LE_WARN("No valid clock stamp retrieved to update the system clock");
        return;
    }

    t = le_clk_GetAbsoluteTime();
    LE_INFO("device's time %ld sec %ld usec before", (long)t.sec, (long)t.usec);

    t.sec = clockStamp;
    t.usec = 0;

    result = le_clk_SetAbsoluteTime(t);
    LE_INFO("Result in setting system clock time: %d", result);

    t = le_clk_GetAbsoluteTime();
    LE_INFO("device's time %ld  %ld usec after", (long)t.sec, (long)t.usec);

    LE_INFO("Triggering DTLS rehandshake after system clock update");

    if (!connPtr)
    {
        LE_DEBUG("No need to initiate a DTLS handshake");
        return;
    }

    // Initiate a DTLS handshake after the system clock has changed so that DTLS can continue to
    // work for AVC
    if (0 != dtls_Rehandshake(connPtr, false))
    {
        LE_ERROR("Unable to perform a DTLS rehandshake for connection %p", connPtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to retrieve the configured priority of the given clock time source from the config tree
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the info retrieval has succeeded
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the info retrieval has failed
 *      - LWM2MCORE_ERR_INVALID_ARG if a config parameter is invalid
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the retrieved value is out of the proper range
 */
//--------------------------------------------------------------------------------------------------
int lwm2mcore_GetClockTimeSourcePriority
(
    uint16_t source,
    int16_t* priority
)
{
    le_cfg_IteratorRef_t cfg;
    char configPath[LE_CFG_STR_LEN_BYTES];
    int32_t output;

    // Validate that the source type is valid and also within ClockSourceTypeString's max array
    // index
    if (!CLOCK_SOURCE_IS_VALID(source))
    {
        LE_ERROR("Invalid clock source %d", source);
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *priority = LE_CLKSYNC_SOURCE_PRIORITY_MIN;

    cfg = le_cfg_CreateReadTxn(LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE);
    if (!cfg)
    {
        LE_DEBUG("No clock source %d configured", source);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
    if (!le_cfg_NodeExists(cfg, ClockSourceTypeString[source]))
    {
        LE_INFO("Clock source %d not configured", source);
        le_cfg_CancelTxn(cfg);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    le_cfg_CancelTxn(cfg);
    snprintf(configPath, LE_CFG_STR_LEN_BYTES, "%s/%s", LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE,
             ClockSourceTypeString[source]);
    cfg = le_cfg_CreateReadTxn(configPath);
    output = le_cfg_GetInt(cfg, LE_CLKSYNC_CONFIG_NODE_SOURCE_PRIORITY,
                           LE_CLKSYNC_SOURCE_PRIORITY_MIN);
    le_cfg_CancelTxn(cfg);

    if ((output < LE_CLKSYNC_SOURCE_PRIORITY_MIN) || (output > LE_CLKSYNC_SOURCE_PRIORITY_MAX))
    {
        LE_ERROR("Invalid priority %d retrieved for clock source %d", output, source);
        return LWM2MCORE_ERR_INCORRECT_RANGE;
    }

    *priority = (int16_t)output;
    LE_INFO("Priority %d retrieved for clock source %d", *priority, source);
    return LWM2MCORE_ERR_COMPLETED_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to set the priority of the given clock time source provided in the input onto the
 * config tree
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the input has been successfully set
 *      - LWM2MCORE_ERR_INVALID_ARG if the input is invalid
 */
//--------------------------------------------------------------------------------------------------
int lwm2mcore_SetClockTimeSourcePriority
(
    uint16_t source,
    int16_t priority
)
{
    le_cfg_IteratorRef_t cfg;
    char configPath[LE_CFG_STR_LEN_BYTES];

    // Validate that the source type is valid and also within ClockSourceTypeString's max array
    // index
    if (!CLOCK_SOURCE_IS_VALID(source))
    {
        LE_ERROR("Invalid clock source %d", source);
        return LWM2MCORE_ERR_INVALID_ARG;

    }

    if ((priority < LE_CLKSYNC_SOURCE_PRIORITY_MIN) || (priority > LE_CLKSYNC_SOURCE_PRIORITY_MAX))
    {
        LE_ERROR("Invalid priority %d given to clock source %d", priority, source);
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    snprintf(configPath, LE_CFG_STR_LEN_BYTES, "%s/%s", LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE,
             ClockSourceTypeString[source]);
    cfg = le_cfg_CreateWriteTxn(configPath);
    le_cfg_SetInt(cfg, LE_CLKSYNC_CONFIG_NODE_SOURCE_PRIORITY, priority);
    le_cfg_CommitTxn(cfg);

    LE_INFO("Priority %d set for clock source %d", priority, source);
    return LWM2MCORE_ERR_COMPLETED_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to retrieve the clock time source config as server name, IPv4/v6 address, etc., from
 * the config tree
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the info retrieval has succeeded
 *      - LWM2MCORE_ERR_INVALID_ARG if there is no source config configured to be retrieved
 */
//--------------------------------------------------------------------------------------------------
int lwm2mcore_GetClockTimeSourceConfig
(
    uint16_t source,
    char* bufferPtr,
    size_t* lenPtr
)
{
    le_cfg_IteratorRef_t cfg;
    char configPath[LE_CFG_STR_LEN_BYTES];
    size_t bufferLength = *lenPtr;
    bufferPtr[0] = '\0';
    *lenPtr = 0;

    // Validate that the source type is valid and also within ClockSourceTypeString's max array
    // index
    if (!CLOCK_SOURCE_IS_VALID(source))
    {
        LE_ERROR("Invalid clock source %d", source);
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    cfg = le_cfg_CreateReadTxn(LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE);
    if (!cfg)
    {
        LE_DEBUG("No clock source %d configured", source);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }
    if (!le_cfg_NodeExists(cfg, ClockSourceTypeString[source]))
    {
        LE_INFO("Clock source %d not configured", source);
        le_cfg_CancelTxn(cfg);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    le_cfg_CancelTxn(cfg);
    snprintf(configPath, LE_CFG_STR_LEN_BYTES, "%s/%s/%s", LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE,
             ClockSourceTypeString[source],LE_CLKSYNC_CONFIG_NODE_SOURCE_CONFIG);
    cfg = le_cfg_CreateReadTxn(configPath);
    if (!cfg)
    {
        LE_DEBUG("Clock source %d configured with no config", source);
        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    if (!le_cfg_NodeExists(cfg, "1"))
    {
        LE_DEBUG("Clock source %d has no config 1 retrieved", source);
    }
    else
    {
        if ((LE_OK != le_cfg_GetString(cfg, "1", bufferPtr, bufferLength, "")) ||
            (strlen(bufferPtr) == 0))
        {
            LE_DEBUG("Clock source %d has no config 1", source);
            le_cfg_CancelTxn(cfg);
            return LWM2MCORE_ERR_INVALID_ARG;
        }
        *lenPtr = strlen(bufferPtr);
        LE_DEBUG("Clock source %d with config retrieved: %s, length %d", source, bufferPtr,
                 (int)*lenPtr);
    }

    le_cfg_CancelTxn(cfg);
    return LWM2MCORE_ERR_COMPLETED_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to set the clock time source config as server name, IPv4/v6 address, etc., onto the
 * config tree
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the input has been successfully set
 *      - LWM2MCORE_ERR_INVALID_ARG if the input is invalid
 */
//--------------------------------------------------------------------------------------------------
int lwm2mcore_SetClockTimeSourceConfig
(
    uint16_t source,
    char* bufferPtr,
    size_t length
)
{
    le_cfg_IteratorRef_t cfg;
    char configPath[LE_CFG_STR_LEN_BYTES];

    // Validate that the source type is valid and also within ClockSourceTypeString's max array
    // index
    if (!CLOCK_SOURCE_IS_VALID(source))
    {
        LE_ERROR("Invalid clock source %d", source);
        return LWM2MCORE_ERR_INVALID_ARG;

    }
    if (!bufferPtr || (length == 0))
    {
        LE_ERROR("Invalid config provided for clock source %d", source);
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    snprintf(configPath, LE_CFG_STR_LEN_BYTES, "%s/%s/%s", LE_CLKSYNC_CONFIG_TREE_ROOT_SOURCE,
             ClockSourceTypeString[source], LE_CLKSYNC_CONFIG_NODE_SOURCE_CONFIG);
    cfg = le_cfg_CreateWriteTxn(configPath);
    le_cfg_SetString(cfg, "1", bufferPtr);
    le_cfg_CommitTxn(cfg);

    LE_INFO("Clock source %d config set: %s", source, bufferPtr);
    return LWM2MCORE_ERR_COMPLETED_OK;
}

#ifdef LE_CONFIG_LINUX
//--------------------------------------------------------------------------------------------------
/**
 * Callback function for executing a clock time update
 */
//--------------------------------------------------------------------------------------------------
static void ClockTimeUpdateCallbackFunction
(
    le_result_t status,        ///< Status of the system time update
    void* contextPtr
)
{
    LE_INFO("Clock update result: %d", status);
    // Reset the in progress status marker after the execution of a clock update is complete
    UpdateSystemClockInProgress = false;
}
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Function to execute the device's system clock update by acquiring it from the clock source(s)
 * configured and, if successful, setting it in
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if successful
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED if this functionality is not supported
 */
//--------------------------------------------------------------------------------------------------
int lwm2mcore_ExecuteClockTimeUpdate
(
    char* bufferPtr,
    size_t length
)
{
    LE_INFO("bufferPtr: %p, length: %d", bufferPtr, (int)length);
    // Set the in progress status marker at the start of a system clock update attempt
#ifdef LE_CONFIG_LINUX
    UpdateSystemClockInProgress = true;
    le_clkSync_UpdateSystemTime(ClockTimeUpdateCallbackFunction, NULL);
    return LWM2MCORE_ERR_COMPLETED_OK;
#else
    return LWM2MCORE_ERR_OP_NOT_SUPPORTED;
#endif
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to retrieve the status of the last execution of clock time update
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the status retrieval has succeeded
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the status retrieval has failed
 *      - LWM2MCORE_ERR_INVALID_ARG if there is no status to be retrieved
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the retrieved status is out of the proper range
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED if this functionality is not supported
 */
//--------------------------------------------------------------------------------------------------
int lwm2mcore_GetClockTimeStatus
(
    uint16_t source,
    int16_t* status
)
{
#ifdef LE_CONFIG_LINUX
    *status = (int16_t)le_clkSync_GetUpdateSystemStatus((le_clkSync_ClockSource_t)source);
    LE_INFO("Clock source %d got last update status %d", source, *status);
    return LWM2MCORE_ERR_COMPLETED_OK;
#else
    *status = 0;
    return LWM2MCORE_ERR_OP_NOT_SUPPORTED;
#endif
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to return a boolean to reveal whether the Clock Service is the process of doing a
 * system clock update.
 *
 * @return
 *      - true if a system clock update is in progress
 *      - false otherwise
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_UpdateSystemClockInProgress
(
    void
)
{
    return UpdateSystemClockInProgress;
}
