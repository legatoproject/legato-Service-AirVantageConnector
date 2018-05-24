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
#define BASE10              10

//--------------------------------------------------------------------------------------------------
/**
 * Value of 1 kilobytes in bytes
 */
//--------------------------------------------------------------------------------------------------
#define KILOBYTE            1000

//--------------------------------------------------------------------------------------------------
/**
 * Define for maximum string length of the currently used cellular technology
 */
//--------------------------------------------------------------------------------------------------
#define MAX_TECH_LEN        20

//--------------------------------------------------------------------------------------------------
/**
 * Define value for the signal bars range (0 to 5)
 */
//--------------------------------------------------------------------------------------------------
#define SIGNAL_BARS_RANGE   6

//--------------------------------------------------------------------------------------------------
/**
 * Measures used for signal bars computation depending on the cellular technology
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    SIGNAL_BARS_WITH_RSSI,          ///< Used for GSM
    SIGNAL_BARS_WITH_RSCP,          ///< Used for WCDMA
    SIGNAL_BARS_WITH_ECIO,          ///< Used for WCDMA
    SIGNAL_BARS_WITH_RSRP,          ///< Used for LTE
    SIGNAL_BARS_WITH_RSRQ,          ///< Used for LTE
    SIGNAL_BARS_WITH_SINR,          ///< Used for LTE
    SIGNAL_BARS_WITH_3GPP2_RSSI,    ///< Used for CDMA 1x and HRPD
    SIGNAL_BARS_WITH_3GPP2_ECIO,    ///< Used for CDMA 1x and HRPD
    SIGNAL_BARS_WITH_MAX            ///< Should be the last of this enum
}
SignalBarsTech_t;

//--------------------------------------------------------------------------------------------------
/**
 * Table defining the signal bars for different cellular technologies
 * Based on:
 *  - AT&T 13340 Device Requirement CDR-RBP-1030 for GSM, UMTS and LTE
 *  - Android source code (SignalStrength API) for CDMA
 */
//--------------------------------------------------------------------------------------------------
static uint16_t SignalBarsTable[SIGNAL_BARS_WITH_MAX][SIGNAL_BARS_RANGE] =
{
    {  125, 104,  98,  89,  80, 0 },    ///< RSSI (GSM)
    {  125, 106, 100,  90,  80, 0 },    ///< RSCP (UMTS)
    {   63,  32,  28,  24,  20, 0 },    ///< ECIO (UMTS)
    {  125, 115, 105,  95,  85, 0 },    ///< RSRP (LTE)
    {  125,  16,  13,  10,   7, 0 },    ///< RSRQ (LTE)
    { -200, -30,  10,  45, 130, 0 },    ///< 10xSINR (LTE)
    {  125, 100,  95,  85,  75, 0 },    ///< RSSI (CDMA)
    {   63,  15,  13,  11,   9, 0 }     ///< ECIO (CDMA)
};

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
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t ConvertRatToNetworkBearer
(
    le_mrc_Rat_t rat,                                   ///< [IN] Radio Access Technology
    lwm2mcore_networkBearer_enum_t* networkBearerPtr    ///< [INOUT] LWM2M network bearer
)
{
    lwm2mcore_Sid_t sID;

    if (!networkBearerPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

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
 * Convert a data bearer technology to a human-readable string
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t ConvertBearerTechnologyToString
(
    le_mdc_DataBearerTechnology_t technology,   ///< [IN] Data bearer technology
    char* bufferPtr,                            ///< [IN] String buffer pointer
    size_t* lenPtr                              ///< [INOUT] String buffer length
)
{
    char cellularTech[MAX_TECH_LEN];
    int cellularTechLen;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    memset(cellularTech, 0, sizeof(cellularTech));

    switch (technology)
    {
        case LE_MDC_DATA_BEARER_TECHNOLOGY_WCDMA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "WCDMA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HSDPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "HSDPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HSUPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "HSUPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HSPA_PLUS:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "HSPA+");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_DC_HSPA_PLUS:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "DC-HSPA+");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_64_QAM:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "64 QAM");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HSPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "HSPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_GPRS:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "GPRS");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_EGPRS:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "EDGE");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_GSM:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "GSM");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_S2B:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "S2B");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_LTE:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "LTE");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_LTE_FDD:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "LTE FDD");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_LTE_TDD:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "LTE TDD");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_TD_SCDMA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "TD-SCDMA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_DC_HSUPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "DC HSUPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_DC_HSPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "DC HSPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_LTE_CA_DL:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "LTE CA DL");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_LTE_CA_UL:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "LTE CA UL");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_IS95_1X:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "IS95 1X");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_CDMA2000_1X:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "CDMA 1X");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_CDMA2000_EVDO:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "CDMA Ev-DO");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_CDMA2000_EVDO_REVA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "CDMA Ev-DO Rev.A");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_CDMA2000_EHRPD:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "CDMA eHRPD");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HDR_REV0_DPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "REV0 DPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HDR_REVA_DPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "REVA DPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HDR_REVB_DPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "RREVB DPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HDR_REVA_MPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "REVA MPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HDR_REVB_MPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "REVB MPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HDR_REVA_EMPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "REVA EMPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HDR_REVB_EMPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "REVB EMPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HDR_REVB_MMPA:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "REVB MMPA");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_HDR_EVDO_FMC:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "EVDO FMC");
            break;

        case LE_MDC_DATA_BEARER_TECHNOLOGY_UNKNOWN:
        default:
            cellularTechLen = snprintf(cellularTech, MAX_TECH_LEN, "Unknown");
            break;
    }

    if ((cellularTechLen < 0) || (MAX_TECH_LEN < cellularTechLen))
    {
        LE_ERROR("Failed to print the data bearer technology");
        return LWM2MCORE_ERR_GENERAL_ERROR;
    }

    if (*lenPtr < cellularTechLen)
    {
        LE_WARN("Buffer too small to hold the data bearer technology");
        return LWM2MCORE_ERR_OVERFLOW;
    }

    memcpy(bufferPtr, cellularTech, cellularTechLen);
    *lenPtr = cellularTechLen;
    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the IP addresses of the connected profiles when using a cellular technology
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t GetCellularIpAddresses
(
    char ipAddrList[CONN_MONITOR_IP_ADDRESSES_MAX_NB][CONN_MONITOR_IP_ADDR_MAX_BYTES],
                            ///< [INOUT] IP addresses list
    uint16_t* ipAddrNbPtr   ///< [INOUT] IP addresses number
)
{
    le_mdc_ProfileRef_t profileRef;
    le_mdc_ConState_t state = LE_MDC_DISCONNECTED;
    int i = 1;
    lwm2mcore_Sid_t sID = LWM2MCORE_ERR_COMPLETED_OK;
    le_result_t result;

    if (!ipAddrNbPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

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
    while (   (i <= le_mdc_NumProfiles())
           && (*ipAddrNbPtr < CONN_MONITOR_IP_ADDRESSES_MAX_NB)
           && (profileRef)
           && (LWM2MCORE_ERR_COMPLETED_OK == sID)
          );

    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the router IP addresses of the connected profiles when using a cellular technology
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t GetCellularRouterIpAddresses
(
    char ipAddrList[CONN_MONITOR_ROUTER_IP_ADDRESSES_MAX_NB][CONN_MONITOR_IP_ADDR_MAX_BYTES],
                            ///< [INOUT] IP addresses list
    uint16_t* ipAddrNbPtr   ///< [INOUT] IP addresses number
)
{
    le_mdc_ProfileRef_t profileRef;
    le_mdc_ConState_t state = LE_MDC_DISCONNECTED;
    uint32_t i = 1;
    lwm2mcore_Sid_t sID = LWM2MCORE_ERR_COMPLETED_OK;
    le_result_t result;

    if (!ipAddrNbPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

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
    while (   (i <= le_mdc_NumProfiles())
           && (*ipAddrNbPtr < CONN_MONITOR_ROUTER_IP_ADDRESSES_MAX_NB)
           && (profileRef)
           && (LWM2MCORE_ERR_COMPLETED_OK == sID)
          );

    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the APN of the connected profiles when using a cellular technology
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t GetCellularApn
(
    char apnList[CONN_MONITOR_APN_MAX_NB][CONN_MONITOR_APN_MAX_BYTES],  ///< [INOUT] APN list
    uint16_t* apnNbPtr                                                  ///< [INOUT] APN number
)
{
    le_mdc_ProfileRef_t profileRef;
    int i = 1;
    lwm2mcore_Sid_t sID = LWM2MCORE_ERR_COMPLETED_OK;
    le_result_t result;

    if (!apnNbPtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    do
    {
        LE_DEBUG("Profile index: %d", i);
        profileRef = le_mdc_GetProfile(i);

        if (profileRef)
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
    while (   (i <= le_mdc_NumProfiles())
           && (*apnNbPtr < CONN_MONITOR_APN_MAX_NB)
           && (profileRef)
           && (LWM2MCORE_ERR_COMPLETED_OK == sID)
          );

    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the number of signal bars when using a cellular technology
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_Sid_t GetCellularSignalBars
(
    uint8_t* valuePtr   ///< [INOUT] Signal bars
)
{
    lwm2mcore_Sid_t sID = LWM2MCORE_ERR_GENERAL_ERROR;
    uint8_t signalBars = 0;
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
    le_mrc_MetricsRef_t metricsRef;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    metricsRef = le_mrc_MeasureSignalMetrics();
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

            while ((signalBars < SIGNAL_BARS_RANGE) && (sID != LWM2MCORE_ERR_COMPLETED_OK))
            {
                if ((-rxLevel) >= SignalBarsTable[SIGNAL_BARS_WITH_RSSI][signalBars])
                {
                    *valuePtr = signalBars;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                }
                else
                {
                    signalBars++;
                }
            }
            break;

        case LE_MRC_RAT_UMTS:
        case LE_MRC_RAT_TDSCDMA:
            if (LE_OK != le_mrc_GetUmtsSignalMetrics(metricsRef, &rxLevel, &er,
                                                     &ecio, &rscp, &sinr))
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }
            // Ec/Io value is given with a decimal by the le_mrc API
            ecio = ecio/10;

            while ((signalBars < SIGNAL_BARS_RANGE) && (sID != LWM2MCORE_ERR_COMPLETED_OK))
            {
                if (   (   (INT32_MAX != rscp)  // INT32_MAX returned if RSCP not available
                        && ((-rscp) >= SignalBarsTable[SIGNAL_BARS_WITH_RSCP][signalBars])
                       )
                    || ((-ecio) >= SignalBarsTable[SIGNAL_BARS_WITH_ECIO][signalBars])
                   )
                {
                    *valuePtr = signalBars;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                }
                else
                {
                    signalBars++;
                }
            }
            break;

        case LE_MRC_RAT_LTE:
            if (LE_OK != le_mrc_GetLteSignalMetrics(metricsRef, &rxLevel, &er,
                                                    &rsrq, &rsrp, &snr))
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }

            // RSRP value is given with a decimal by the le_mrc API
            rsrp = rsrp / 10;

            while ((signalBars < SIGNAL_BARS_RANGE) && (sID != LWM2MCORE_ERR_COMPLETED_OK))
            {
                if ((-rsrp) >= SignalBarsTable[SIGNAL_BARS_WITH_RSRP][signalBars])
                {
                    *valuePtr = signalBars;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                }
                else
                {
                    signalBars++;
                }
            }
            break;

        case LE_MRC_RAT_CDMA:
            if (LE_OK != le_mrc_GetCdmaSignalMetrics(metricsRef, &rxLevel, &er,
                                                     &ecio, &sinr, &io))
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }

            // Ec/Io value is given with a decimal by the le_mrc API
            ecio = ecio / 10;

            while ((signalBars < SIGNAL_BARS_RANGE) && (sID != LWM2MCORE_ERR_COMPLETED_OK))
            {
                if (   ((-rxLevel) >= SignalBarsTable[SIGNAL_BARS_WITH_3GPP2_RSSI][signalBars])
                    || ((-ecio) >= SignalBarsTable[SIGNAL_BARS_WITH_3GPP2_ECIO][signalBars])
                   )
                {
                    *valuePtr = signalBars;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                }
                else
                {
                    signalBars++;
                }
            }
            break;

        default:
            LE_ERROR("Unknown RAT %d", rat);
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }
    le_mrc_DeleteSignalMetrics(metricsRef);

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
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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

    LE_DEBUG("Result: %d", sID);
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
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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

    LE_DEBUG("Result: %d", sID);
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
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
                case LE_MRC_RAT_TDSCDMA:
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

    LE_DEBUG("Result: %d", sID);
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
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLinkQuality
(
    int* valuePtr  ///< [INOUT] data buffer
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
                    if (UINT32_MAX == er)
                    {
                        sID = LWM2MCORE_ERR_INVALID_STATE;
                    }
                    else
                    {
                        *valuePtr = (int)er;
                        sID = LWM2MCORE_ERR_COMPLETED_OK;
                    }
                    break;

                case LE_MRC_RAT_UMTS:
                case LE_MRC_RAT_TDSCDMA:
                    if (LE_OK != le_mrc_GetUmtsSignalMetrics(metricsRef, &rxLevel, &er,
                                                             &ecio, &rscp, &sinr))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    *valuePtr = (int)ecio/10;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                case LE_MRC_RAT_LTE:
                    if (LE_OK != le_mrc_GetLteSignalMetrics(metricsRef, &rxLevel, &er,
                                                            &rsrq, &rsrp, &snr))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    *valuePtr = (int)rsrq/10;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                case LE_MRC_RAT_CDMA:
                    if (LE_OK != le_mrc_GetCdmaSignalMetrics(metricsRef, &rxLevel, &er,
                                                             &ecio, &sinr, &io))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    *valuePtr = (int)ecio/10;
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

    LE_DEBUG("Result: %d", sID);
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
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
    memset(ipAddrList, 0,
           ((CONN_MONITOR_IP_ADDRESSES_MAX_NB)*(CONN_MONITOR_IP_ADDR_MAX_BYTES)*sizeof(char)));
    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
            sID = GetCellularIpAddresses(ipAddrList, ipAddrNbPtr);
            break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
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
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
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
    memset(ipAddrList, 0,
           ((CONN_MONITOR_IP_ADDRESSES_MAX_NB)*(CONN_MONITOR_IP_ADDR_MAX_BYTES)*sizeof(char)));
    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
            sID = GetCellularRouterIpAddresses(ipAddrList, ipAddrNbPtr);
            break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
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
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
    memset(apnList, 0, ((CONN_MONITOR_APN_MAX_NB)*(CONN_MONITOR_APN_MAX_BYTES)*sizeof(char)));
    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
            sID = GetCellularApn(apnList, apnNbPtr);
            break;

        case LE_DATA_WIFI:
            // The SSID could be returned in this case
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
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
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
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
                sID = LWM2MCORE_ERR_INVALID_STATE;
            }
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
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
 *      - LWM2MCORE_ERR_NOT_YET_IMPLEMENTED if the resource is not yet implemented
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
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
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the signal bars (range 0-5)
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetSignalBars
(
    uint8_t* valuePtr   ///< [INOUT] data buffer
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
            sID = GetCellularSignalBars(valuePtr);
            break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_NOT_YET_IMPLEMENTED;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the currently used cellular technology
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_OVERFLOW in case of buffer overflow
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetCellularTechUsed
(
    char*   bufferPtr,  ///< [IN]    data buffer pointer
    size_t* lenPtr      ///< [INOUT] length of input buffer and length of the returned data
)
{
    lwm2mcore_Sid_t sID;
    le_data_Technology_t currentTech;

    if ((!bufferPtr) || (!lenPtr))
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    currentTech = le_data_GetTechnology();

    switch (currentTech)
    {
        case LE_DATA_CELLULAR:
        {
            le_result_t result;
            le_mdc_DataBearerTechnology_t uplinkTech;
            le_mdc_DataBearerTechnology_t downlinkTech;
            uint32_t profileIndex;

            profileIndex = le_data_GetCellularProfileIndex();
            result = le_mdc_GetDataBearerTechnology(le_mdc_GetProfile((uint32_t) profileIndex),
                                                    &downlinkTech,
                                                    &uplinkTech);
            if (LE_OK != result)
            {
                LE_ERROR("Failed to retrieve the data bearer technology");
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }

            // Consider only the downlink technology, as it is the relevant one for
            // most of the AVC use cases (FOTA, SOTA)
            sID = ConvertBearerTechnologyToString(downlinkTech, bufferPtr, lenPtr);
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the roaming indicator (0: home, 1: roaming)
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetRoamingIndicator
(
    uint8_t* valuePtr   ///< [INOUT] data buffer
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
            le_result_t result;
            le_mrc_NetRegState_t state = LE_MRC_REG_UNKNOWN;

            result = le_mrc_GetNetRegState(&state);
            switch (result)
            {
                case LE_OK:
                    if (LE_MRC_REG_ROAMING == state)
                    {
                        *valuePtr = 1;
                    }
                    else
                    {
                        *valuePtr = 0;
                    }
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
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the signal to noise Ec/Io ratio (in dBm)
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetEcIo
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
            int32_t  io      = 0;
            le_mrc_MetricsRef_t metricsRef;

            metricsRef = le_mrc_MeasureSignalMetrics();
            if (!metricsRef)
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }

            rat = le_mrc_GetRatOfSignalMetrics(metricsRef);
            switch (rat)
            {
                case LE_MRC_RAT_GSM:
                case LE_MRC_RAT_LTE:
                    // No Ec/Io available for GSM and LTE
                    sID = LWM2MCORE_ERR_INVALID_STATE;
                    break;

                case LE_MRC_RAT_UMTS:
                case LE_MRC_RAT_TDSCDMA:
                    if (LE_OK != le_mrc_GetUmtsSignalMetrics(metricsRef, &rxLevel, &er,
                                                             &ecio, &rscp, &sinr))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    // Ec/Io value is given with a decimal by the le_mrc API
                    *valuePtr = ecio / 10;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                case LE_MRC_RAT_CDMA:
                    if (LE_OK != le_mrc_GetCdmaSignalMetrics(metricsRef, &rxLevel, &er,
                                                             &ecio, &sinr, &io))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    // Ec/Io value is given with a decimal by the le_mrc API
                    *valuePtr = ecio / 10;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                default:
                    LE_ERROR("Unknown RAT %d", rat);
                    sID = LWM2MCORE_ERR_GENERAL_ERROR;
                    break;
            }
            le_mrc_DeleteSignalMetrics(metricsRef);
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the Reference Signal Received Power (in dBm) if LTE is used
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetRsrp
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
            int32_t  rsrq    = 0;
            int32_t  rsrp    = 0;
            int32_t  snr     = 0;
            le_mrc_MetricsRef_t metricsRef;

            metricsRef = le_mrc_MeasureSignalMetrics();
            if (!metricsRef)
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }

            rat = le_mrc_GetRatOfSignalMetrics(metricsRef);
            switch (rat)
            {
                case LE_MRC_RAT_GSM:
                case LE_MRC_RAT_UMTS:
                case LE_MRC_RAT_TDSCDMA:
                case LE_MRC_RAT_CDMA:
                    // RSRP available only for LTE
                    sID = LWM2MCORE_ERR_INVALID_STATE;
                    break;

                case LE_MRC_RAT_LTE:
                    if (LE_OK != le_mrc_GetLteSignalMetrics(metricsRef, &rxLevel, &er,
                                                            &rsrq, &rsrp, &snr))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    // RSRP value is given with a decimal by the le_mrc API
                    *valuePtr = rsrp / 10;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                default:
                    LE_ERROR("Unknown RAT %d", rat);
                    sID = LWM2MCORE_ERR_GENERAL_ERROR;
                    break;
            }
            le_mrc_DeleteSignalMetrics(metricsRef);
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the Reference Signal Received Quality (in dB) if LTE is used
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetRsrq
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
            int32_t  rsrq    = 0;
            int32_t  rsrp    = 0;
            int32_t  snr     = 0;
            le_mrc_MetricsRef_t metricsRef;

            metricsRef = le_mrc_MeasureSignalMetrics();
            if (!metricsRef)
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }

            rat = le_mrc_GetRatOfSignalMetrics(metricsRef);
            switch (rat)
            {
                case LE_MRC_RAT_GSM:
                case LE_MRC_RAT_UMTS:
                case LE_MRC_RAT_TDSCDMA:
                case LE_MRC_RAT_CDMA:
                    // RSRQ available only for LTE
                    sID = LWM2MCORE_ERR_INVALID_STATE;
                    break;

                case LE_MRC_RAT_LTE:
                    if (LE_OK != le_mrc_GetLteSignalMetrics(metricsRef, &rxLevel, &er,
                                                            &rsrq, &rsrp, &snr))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    // RSRQ value is given with a decimal by the le_mrc API
                    *valuePtr = rsrq / 10;
                    sID = LWM2MCORE_ERR_COMPLETED_OK;
                    break;

                default:
                    LE_ERROR("Unknown RAT %d", rat);
                    sID = LWM2MCORE_ERR_GENERAL_ERROR;
                    break;
            }
            le_mrc_DeleteSignalMetrics(metricsRef);
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the Received Signal Code Power (in dBm) if UMTS is used
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetRscp
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
            le_mrc_MetricsRef_t metricsRef;

            metricsRef = le_mrc_MeasureSignalMetrics();
            if (!metricsRef)
            {
                return LWM2MCORE_ERR_GENERAL_ERROR;
            }

            rat = le_mrc_GetRatOfSignalMetrics(metricsRef);
            switch (rat)
            {
                case LE_MRC_RAT_GSM:
                case LE_MRC_RAT_LTE:
                case LE_MRC_RAT_CDMA:
                    // RSCP available only for UMTS and TD-SCDMA
                    sID = LWM2MCORE_ERR_INVALID_STATE;
                    break;

                case LE_MRC_RAT_UMTS:
                case LE_MRC_RAT_TDSCDMA:
                    if (LE_OK != le_mrc_GetUmtsSignalMetrics(metricsRef, &rxLevel, &er,
                                                             &ecio, &rscp, &sinr))
                    {
                        return LWM2MCORE_ERR_GENERAL_ERROR;
                    }
                    if (INT32_MAX == rscp)
                    {
                        // This value means that the value is not available
                        sID = LWM2MCORE_ERR_INVALID_STATE;
                    }
                    else
                    {
                        *valuePtr = rscp;
                        sID = LWM2MCORE_ERR_COMPLETED_OK;
                    }
                    break;

                default:
                    LE_ERROR("Unknown RAT %d", rat);
                    sID = LWM2MCORE_ERR_GENERAL_ERROR;
                    break;
            }
            le_mrc_DeleteSignalMetrics(metricsRef);
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the Location Area Code
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLac
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
            uint32_t lac;

            lac = le_mrc_GetServingCellLocAreaCode();
            if (UINT32_MAX != lac)
            {
                *valuePtr = lac;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            else
            {
                sID = LWM2MCORE_ERR_INVALID_STATE;
            }
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the Tracking Area Code (LTE)
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 *      - LWM2MCORE_ERR_INVALID_STATE in case of invalid state to treat the resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetServingCellLteTracAreaCode
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
            uint16_t tac;

            tac = le_mrc_GetServingCellLteTracAreaCode();
            if (UINT16_MAX != tac)
            {
                *valuePtr = tac;
                sID = LWM2MCORE_ERR_COMPLETED_OK;
            }
            else
            {
                sID = LWM2MCORE_ERR_INVALID_STATE;
            }
        }
        break;

        case LE_DATA_WIFI:
            sID = LWM2MCORE_ERR_INVALID_STATE;
            break;

        default:
            sID = LWM2MCORE_ERR_GENERAL_ERROR;
            break;
    }

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the total number of SMS successfully transmitted during the collection period
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetSmsTxCount
(
    uint64_t* valuePtr  ///< [INOUT] data buffer
)
{
    int32_t smsTxCount;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK == le_sms_GetCount(LE_SMS_TYPE_TX, &smsTxCount))
    {
        *valuePtr = smsTxCount;
        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    *valuePtr = 0;
    return LWM2MCORE_ERR_GENERAL_ERROR;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the total number of SMS successfully received during the collection period
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetSmsRxCount
(
    uint64_t* valuePtr  ///< [INOUT] data buffer
)
{
    int32_t smsRxCount;

    if (!valuePtr)
    {
        return LWM2MCORE_ERR_INVALID_ARG;
    }

    if (LE_OK == le_sms_GetCount(LE_SMS_TYPE_RX, &smsRxCount))
    {
        *valuePtr = smsRxCount;
        return LWM2MCORE_ERR_COMPLETED_OK;
    }

    *valuePtr = 0;
    return LWM2MCORE_ERR_GENERAL_ERROR;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the total amount of data transmitted during the collection period (in kilobytes)
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetTxData
(
    uint64_t* valuePtr  ///< [INOUT] data buffer
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
            uint64_t rxBytes, txBytes;

            if (LE_OK == le_mdc_GetBytesCounters(&rxBytes, &txBytes))
            {
                // Amount of data is converted from bytes to kilobytes
                *valuePtr = txBytes / KILOBYTE;
                LE_DEBUG("txBytes: %"PRIu64" -> Tx Data = %"PRIu64" kB", txBytes, *valuePtr);
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

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the total amount of data received during the collection period (in kilobytes)
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 *      - LWM2MCORE_ERR_INVALID_ARG if a parameter is invalid in resource handler
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetRxData
(
    uint64_t* valuePtr  ///< [INOUT] data buffer
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
            uint64_t rxBytes, txBytes;

            if (LE_OK == le_mdc_GetBytesCounters(&rxBytes, &txBytes))
            {
                // Amount of data is converted from bytes to kilobytes
                *valuePtr = rxBytes / KILOBYTE;
                LE_DEBUG("rxBytes: %"PRIu64" -> Rx Data = %"PRIu64" kB", rxBytes, *valuePtr);
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

    LE_DEBUG("Result: %d", sID);
    return sID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Reset SMS and data counters and start to collect information
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_StartConnectivityCounters
(
    void
)
{
    // Reset and start SMS counters
    le_sms_ResetCount();
    le_sms_StartCount();

    // Reset and start cellular data counters
    if (LE_DATA_CELLULAR == le_data_GetTechnology())
    {
        if (   (LE_OK != le_mdc_ResetBytesCounter())
            || (LE_OK != le_mdc_StartBytesCounter()))
        {
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop SMS and data counters without resetting the counters
 * This API treatment needs to have a procedural treatment
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_StopConnectivityCounters
(
    void
)
{
    // Stop SMS counters without resetting the counters
    le_sms_StopCount();

    // Stop cellular data counters without resetting the counters
    if (LE_DATA_CELLULAR == le_data_GetTechnology())
    {
        if (LE_OK != le_mdc_StopBytesCounter())
        {
            return LWM2MCORE_ERR_GENERAL_ERROR;
        }
    }

    return LWM2MCORE_ERR_COMPLETED_OK;
}
