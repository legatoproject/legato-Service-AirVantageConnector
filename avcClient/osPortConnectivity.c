/**
 * @file osPortConnectivity.c
 *
 * Porting layer for connectivity parameters
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/connectivity.h>
#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Define value for the base used in strtoul
 */
//--------------------------------------------------------------------------------------------------
#define BASE10  10

//--------------------------------------------------------------------------------------------------
// Static functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Convert a Radio Access Technology to a LWM2M network bearer
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t ConvertRatToNetworkBearer
(
    le_mrc_Rat_t rat,                                   ///< [IN] Radio Access Technology
    lwm2mcore_networkBearer_enum_t* networkBearerPtr    ///< [INOUT] LWM2M network bearer
)
{
    lwm2mcore_Sid_t sID;

    switch (rat)
    {
        case LE_MRC_RAT_GSM:
            *networkBearerPtr = LWM2MCORE_NETWORK_BEARER_GSM;
            sID = LWM2MCORE_ERR_COMPLETED_OK;
            break;

        case LE_MRC_RAT_UMTS:
            *networkBearerPtr = LWM2MCORE_NETWORK_BEARER_WCDMA;
            sID = LWM2MCORE_ERR_COMPLETED_OK;
            break;

        case LE_MRC_RAT_LTE:
            *networkBearerPtr = LWM2MCORE_NETWORK_BEARER_LTE_FDD;
            sID = LWM2MCORE_ERR_COMPLETED_OK;
            break;

        case LE_MRC_RAT_CDMA:
            *networkBearerPtr = LWM2MCORE_NETWORK_BEARER_CDMA2000;
            sID = LWM2MCORE_ERR_COMPLETED_OK;
            break;

        case LE_MRC_RAT_UNKNOWN:
        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the IP addresses of the connected profiles
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t GetIpAddresses
(
    char ipAddrList[CONN_MONITOR_IP_ADDRESSES_MAX_NB][CONN_MONITOR_IP_ADDR_MAX_BYTES],
                            ///< [INOUT] IP addresses list
    uint16_t* ipAddrNbPtr   ///< [INOUT] IP addresses number
)
{
    le_mdc_ProfileRef_t profileRef;
    le_mdc_ConState_t state = LE_MDC_DISCONNECTED;
    uint32_t i = le_mdc_GetProfileIndex(le_mdc_GetProfile(LE_MDC_DEFAULT_PROFILE));
    lwm2mcore_Sid_t sID = LWM2MCORE_ERR_COMPLETED_OK;
    le_result_t result;

    do
    {
        LE_DEBUG("Profile index: %d", i);
        profileRef = le_mdc_GetProfile(i);

        if (   (profileRef)
            && (LE_OK == le_mdc_GetSessionState(profileRef, &state))
            && (LE_MDC_CONNECTED == state)
           )
        {
            if (le_mdc_IsIPv4(profileRef))
            {
                result = le_mdc_GetIPv4Address(profileRef,
                                               ipAddrList[*ipAddrNbPtr],
                                               sizeof(ipAddrList[*ipAddrNbPtr]));
                switch (result)
                {
                    case LE_OK:
                        (*ipAddrNbPtr)++;
                        sID = LWM2MCORE_ERR_COMPLETED_OK;
                        break;

                    case LE_OVERFLOW:
                        sID = LWM2MCORE_ERR_OVERFLOW;
                        break;

                    default:
                        sID = LWM2MCORE_ERR_GENERAL_ERROR;
                        break;
                }
            }
            if (le_mdc_IsIPv6(profileRef))
            {
                result = le_mdc_GetIPv6Address(profileRef,
                                               ipAddrList[*ipAddrNbPtr],
                                               sizeof(ipAddrList[*ipAddrNbPtr]));
                switch (result)
                {
                    case LE_OK:
                        (*ipAddrNbPtr)++;
                        sID = LWM2MCORE_ERR_COMPLETED_OK;
                        break;

                    case LE_OVERFLOW:
                        sID = LWM2MCORE_ERR_OVERFLOW;
                        break;

                    default:
                        sID = LWM2MCORE_ERR_GENERAL_ERROR;
                        break;
                }
            }
        }
        i++;
    }
    while (   (*ipAddrNbPtr <= CONN_MONITOR_IP_ADDRESSES_MAX_NB)
           && (profileRef)
           && (LWM2MCORE_ERR_COMPLETED_OK == sID)
          );

    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the router IP addresses of the connected profiles
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t GetRouterIpAddresses
(
    char ipAddrList[CONN_MONITOR_ROUTER_IP_ADDRESSES_MAX_NB][CONN_MONITOR_IP_ADDR_MAX_BYTES],
                            ///< [INOUT] IP addresses list
    uint16_t* ipAddrNbPtr   ///< [INOUT] IP addresses number
)
{
    le_mdc_ProfileRef_t profileRef;
    le_mdc_ConState_t state = LE_MDC_DISCONNECTED;
    uint32_t i = le_mdc_GetProfileIndex(le_mdc_GetProfile(LE_MDC_DEFAULT_PROFILE));
    lwm2mcore_Sid_t sID = LWM2MCORE_ERR_COMPLETED_OK;
    le_result_t result;

    do
    {
        LE_DEBUG("Profile index: %d", i);
        profileRef = le_mdc_GetProfile(i);

        if (   (profileRef)
            && (LE_OK == le_mdc_GetSessionState(profileRef, &state))
            && (LE_MDC_CONNECTED == state)
           )
        {
            if (le_mdc_IsIPv4(profileRef))
            {
                result = le_mdc_GetIPv4GatewayAddress(profileRef,
                                                      ipAddrList[*ipAddrNbPtr],
                                                      sizeof(ipAddrList[*ipAddrNbPtr]));
                switch (result)
                {
                    case LE_OK:
                        (*ipAddrNbPtr)++;
                        sID = LWM2MCORE_ERR_COMPLETED_OK;
                        break;

                    case LE_OVERFLOW:
                        sID = LWM2MCORE_ERR_OVERFLOW;
                        break;

                    default:
                        sID = LWM2MCORE_ERR_GENERAL_ERROR;
                        break;
                }
            }
            if (le_mdc_IsIPv6(profileRef))
            {
                result = le_mdc_GetIPv6GatewayAddress(profileRef,
                                                      ipAddrList[*ipAddrNbPtr],
                                                      sizeof(ipAddrList[*ipAddrNbPtr]));
                switch (result)
                {
                    case LE_OK:
                        (*ipAddrNbPtr)++;
                        sID = LWM2MCORE_ERR_COMPLETED_OK;
                        break;

                    case LE_OVERFLOW:
                        sID = LWM2MCORE_ERR_OVERFLOW;
                        break;

                    default:
                        sID = LWM2MCORE_ERR_GENERAL_ERROR;
                        break;
                }
            }
        }
        i++;
    }
    while (   (*ipAddrNbPtr <= CONN_MONITOR_ROUTER_IP_ADDRESSES_MAX_NB)
           && (profileRef)
           && (LWM2MCORE_ERR_COMPLETED_OK == sID)
          );

    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the APN of the connected profiles
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t GetApn
(
    char apnList[CONN_MONITOR_APN_MAX_NB][CONN_MONITOR_APN_MAX_BYTES],  ///< [INOUT] APN list
    uint16_t* apnNbPtr                                                  ///< [INOUT] APN number
)
{
    le_mdc_ProfileRef_t profileRef;
    le_mdc_ConState_t state = LE_MDC_DISCONNECTED;
    uint32_t i = le_mdc_GetProfileIndex(le_mdc_GetProfile(LE_MDC_DEFAULT_PROFILE));
    lwm2mcore_Sid_t sID = LWM2MCORE_ERR_COMPLETED_OK;
    le_result_t result;

    do
    {
        LE_DEBUG("Profile index: %d", i);
        profileRef = le_mdc_GetProfile(i);

        if (   (profileRef)
            && (LE_OK == le_mdc_GetSessionState(profileRef, &state))
            && (LE_MDC_CONNECTED == state)
           )
        {
            result = le_mdc_GetAPN(profileRef, apnList[*apnNbPtr], sizeof(apnList[*apnNbPtr]));
            switch (result)
            {
                case LE_OK:
                    (*apnNbPtr)++;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                case LE_OVERFLOW:
                    sID = LWM2MCORE_ERR_OVERFLOW;
                    break;

                case LE_BAD_PARAMETER:
                    sID = LWM2MCORE_ERR_INVALID_ARG;
                    break;

                default:
                    sID = LWM2MCORE_ERR_GENERAL_ERROR;
                    break;
            }
        }
        i++;
    }
    while (   (*apnNbPtr <= CONN_MONITOR_APN_MAX_NB)
           && (profileRef)
           && (LWM2MCORE_ERR_COMPLETED_OK == sID)
          );

    return sID;
}

//--------------------------------------------------------------------------------------------------
// Public functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the network bearer used for the current LWM2M communication session
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetNetworkBearer
(
    lwm2mcore_networkBearer_enum_t* valuePtr    ///< [INOUT] data buffer
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t currentTech;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
        {
            le_mrc_Rat_t currentRat;
            le_result_t result = le_mrc_GetRadioAccessTechInUse(&currentRat);

            switch (result)
            {
                case LE_OK:
                    sID = ConvertRatToNetworkBearer(currentRat, valuePtr);
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
        break;

        case LE_DATA_WIFI:
            *valuePtr = LWM2MCORE_NETWORK_BEARER_WLAN;
            sID = LWM2MCORE_ERR_COMPLETED_OK;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("os_portConnectivityNetworkBearer result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the list of current available network bearers
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetAvailableNetworkBearers
(
    lwm2mcore_networkBearer_enum_t* bearersListPtr,     ///< [IN]    bearers list pointer
    uint16_t* bearersNbPtr                              ///< [INOUT] bearers number
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t technology;

    if ((!bearersListPtr) || (!bearersNbPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    technology = le_data_GetFirstUsedTechnology();
    *bearersNbPtr = 0;

    do
    {
        switch (technology)
        {
            case LE_DATA_CELLULAR:
            {
                // Use the supported network bearers for now, to remove when asynchronous
                // response is supported
                le_mrc_RatBitMask_t ratBitMask = 0;

                if (LE_OK != le_mrc_GetRatPreferences(&ratBitMask))
                {
                    return LWM2MCORE_ERR_GENERAL_ERROR;
                }

                if (LE_MRC_BITMASK_RAT_ALL == ratBitMask)
                {
                    *(bearersListPtr + (*bearersNbPtr)) = LWM2MCORE_NETWORK_BEARER_GSM;
                    (*bearersNbPtr)++;
                    *(bearersListPtr + (*bearersNbPtr)) = LWM2MCORE_NETWORK_BEARER_WCDMA;
                    (*bearersNbPtr)++;
                    *(bearersListPtr + (*bearersNbPtr)) = LWM2MCORE_NETWORK_BEARER_LTE_FDD;
                    (*bearersNbPtr)++;
                    *(bearersListPtr + (*bearersNbPtr)) = LWM2MCORE_NETWORK_BEARER_CDMA2000;
                    (*bearersNbPtr)++;
                }
                else
                {
                    if (ratBitMask & LE_MRC_BITMASK_RAT_GSM)
                    {
                        *(bearersListPtr + (*bearersNbPtr)) = LWM2MCORE_NETWORK_BEARER_GSM;
                        (*bearersNbPtr)++;
                    }
                    if (ratBitMask & LE_MRC_BITMASK_RAT_UMTS)
                    {
                        *(bearersListPtr + (*bearersNbPtr)) = LWM2MCORE_NETWORK_BEARER_WCDMA;
                        (*bearersNbPtr)++;
                    }
                    if (ratBitMask & LE_MRC_BITMASK_RAT_LTE)
                    {
                        *(bearersListPtr + (*bearersNbPtr)) = LWM2MCORE_NETWORK_BEARER_LTE_FDD;
                        (*bearersNbPtr)++;
                    }
                    if (ratBitMask & LE_MRC_BITMASK_RAT_CDMA)
                    {
                        *(bearersListPtr + (*bearersNbPtr)) = LWM2MCORE_NETWORK_BEARER_CDMA2000;
                        (*bearersNbPtr)++;
                    }
                }
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            break;

            case LE_DATA_WIFI:
                *(bearersListPtr + (*bearersNbPtr)) = LWM2MCORE_NETWORK_BEARER_WLAN;
                (*bearersNbPtr)++;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
                break;

            default:
                sID = LWM2MCORE_ERR_GENERAL_ERROR;
                break;
        }

        technology = le_data_GetNextUsedTechnology();
    }
    while (   (LE_DATA_MAX != technology)
           && (*bearersNbPtr <= CONN_MONITOR_AVAIL_NETWORK_BEARER_MAX_NB)
           && (LWM2MCORE_ERR_COMPLETED_OK == sID));

    LE_DEBUG("os_portConnectivityAvailableNetworkBearers result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the average value of the received signal strength indication used in the current
 * network bearer (in dBm)
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetSignalStrength
(
    int32_t* valuePtr   ///< [INOUT] data buffer
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t currentTech;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
        {
            le_mrc_Rat_t rat;
            int32_t  rxLevel = 0;
            uint32_t er      = 0;
            int32_t  ecio    = 0;
            int32_t  rscp    = 0;
            int32_t  sinr    = 0;
            int32_t  rsrq    = 0;
            int32_t  rsrp    = 0;
            int32_t  snr     = 0;
            int32_t  io      = 0;
            le_mrc_MetricsRef_t metricsRef = le_mrc_MeasureSignalMetrics();

            if (!metricsRef)
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }

            rat = le_mrc_GetRatOfSignalMetrics(metricsRef);

            switch (rat)
            {
                case LE_MRC_RAT_GSM:
                    if (LE_OK != le_mrc_GetGsmSignalMetrics(metricsRef, &rxLevel, &er))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    *valuePtr = rxLevel;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                case LE_MRC_RAT_UMTS:
                    if (LE_OK != le_mrc_GetUmtsSignalMetrics(metricsRef, &rxLevel, &er,
                                                             &ecio, &rscp, &sinr))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    *valuePtr = rxLevel;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                case LE_MRC_RAT_LTE:
                    if (LE_OK != le_mrc_GetLteSignalMetrics(metricsRef, &rxLevel, &er,
                                                            &rsrq, &rsrp, &snr))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    *valuePtr = rxLevel;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                case LE_MRC_RAT_CDMA:
                    if (LE_OK != le_mrc_GetCdmaSignalMetrics(metricsRef, &rxLevel, &er,
                                                             &ecio, &sinr, &io))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    *valuePtr = rxLevel;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                default:
                    sID = LWM2MCORE_ERR_GENERAL_ERROR;
                    break;
            }

            le_mrc_DeleteSignalMetrics(metricsRef);
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("os_portConnectivitySignalStrength result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the received link quality
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLinkQuality
(
    uint16_t* valuePtr  ///< [INOUT] data buffer
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t currentTech;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
        {
            le_result_t result = le_mrc_GetSignalQual(valuePtr);

            switch (result)
            {
                case LE_OK:
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
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
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("os_portConnectivityLinkQuality result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the list of IP addresses assigned to the connectivity interface
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetIpAddresses
(
    char ipAddrList[CONN_MONITOR_IP_ADDRESSES_MAX_NB][CONN_MONITOR_IP_ADDR_MAX_BYTES],
                            ///< [INOUT] IP addresses list
    uint16_t* ipAddrNbPtr   ///< [INOUT] IP addresses number
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t currentTech;

    if (!ipAddrNbPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *ipAddrNbPtr = 0;
    memset(ipAddrList, 0, sizeof(ipAddrList));
    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
            sID = GetIpAddresses(ipAddrList, ipAddrNbPtr);
            break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("os_portConnectivityIpAddresses result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the list of the next-hop router IP addresses
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetRouterIpAddresses
(
    char ipAddrList[CONN_MONITOR_ROUTER_IP_ADDRESSES_MAX_NB][CONN_MONITOR_IP_ADDR_MAX_BYTES],
                            ///< [INOUT] IP addresses list
    uint16_t* ipAddrNbPtr   ///< [INOUT] IP addresses number
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t currentTech;

    if (!ipAddrNbPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *ipAddrNbPtr = 0;
    memset(ipAddrList, 0, sizeof(ipAddrList));
    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
            sID = GetRouterIpAddresses(ipAddrList, ipAddrNbPtr);
            break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("os_portConnectivityRouterIpAddresses result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the average utilization of the link to the next-hop IP router in %
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLinkUtilization
(
    uint8_t* valuePtr   ///< [INOUT] data buffer
)
{
    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    return LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the list of the Access Point Names
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetAccessPointNames
(
    char apnList[CONN_MONITOR_APN_MAX_NB][CONN_MONITOR_APN_MAX_BYTES],  ///< [INOUT] APN list
    uint16_t* apnNbPtr                                                  ///< [INOUT] APN number
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t currentTech;

    if (!apnNbPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    *apnNbPtr = 0;
    memset(apnList, 0, sizeof(apnList));
    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
            sID = GetApn(apnList, apnNbPtr);
            break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("os_portConnectivityApn result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the serving cell ID
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetCellId
(
    uint32_t* valuePtr  ///< [INOUT] data buffer
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t currentTech;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
        {
            uint32_t cellId = le_mrc_GetServingCellId();
            if (UINT32_MAX != cellId)
            {
                *valuePtr = cellId;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            else
            {
                sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            }
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("os_portConnectivityCellId result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the serving Mobile Network Code and/or the serving Mobile Country Code
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INCORRECT_RANGE if the provided parameters (WRITE operation) is incorrect
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_OP_NOT_SUPPORTED  if the resource is not supported
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetMncMcc
(
    uint16_t* mncPtr,   ///< [INOUT] MNC buffer, NULL if not needed
    uint16_t* mccPtr    ///< [INOUT] MCC buffer, NULL if not needed
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t currentTech;

    if ((!mncPtr) && (!mccPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
        {
            char mcc[LE_MRC_MCC_BYTES] = {0};
            char mnc[LE_MRC_MNC_BYTES] = {0};
            le_result_t result = le_mrc_GetCurrentNetworkMccMnc(mcc, LE_MRC_MCC_BYTES,
                                                                mnc, LE_MRC_MNC_BYTES);
            if (LE_OK == result)
            {
                if (mncPtr)
                {
                    *mncPtr = (uint16_t)strtoul(mnc, NULL, BASE10);
                }
                if (mccPtr)
                {
                    *mccPtr = (uint16_t)strtoul(mcc, NULL, BASE10);
                }
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            else
            {
                sID = LWM2MCORE_ERR_GENERAL_ERROR;
            }
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("os_portConnectivityMncMcc result: %d", sID);
    return sID;
}
