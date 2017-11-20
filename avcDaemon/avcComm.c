/**
 * @file avcComm.c
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <legato.h>
#include <interfaces.h>
#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/udp.h>

//--------------------------------------------------------------------------------------------------
/**
 * Get CoAP response class
 */
//--------------------------------------------------------------------------------------------------
#define CLASS(code)    (code >> 5)

//--------------------------------------------------------------------------------------------------
/**
 * Get CoAP response details
 */
//--------------------------------------------------------------------------------------------------
#define DETAILS(code)  (code & 0x1f)

//--------------------------------------------------------------------------------------------------
/**
 * Communication information struct definition
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint8_t     code;                                 ///< Error code identifier
    char        str[LE_AVC_COMM_INFO_STR_MAX_LEN+1];  ///< Error code message
}
CommInfo_t;

//--------------------------------------------------------------------------------------------------
/**
 * Communication information event id
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t CommEventId = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Data connection state handler reference
 */
//--------------------------------------------------------------------------------------------------
static le_data_ConnectionStateHandlerRef_t  DataHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Data interface name
 */
//--------------------------------------------------------------------------------------------------
static char IfName[LE_DATA_INTERFACE_NAME_MAX_LEN] = "";

//--------------------------------------------------------------------------------------------------
/**
 * Convert a UDP error to string
 */
//--------------------------------------------------------------------------------------------------
static const char* UdpErrorToStr
(
    int code
)
{
    const char* str;

    switch (code)
    {
        case LWM2MCORE_UDP_NO_ERR:
            str = STRINGIZE_EXPAND(LWM2MCORE_UDP_NO_ERR);
            break;
        case LWM2MCORE_UDP_OPEN_ERR:
            str = STRINGIZE_EXPAND(LWM2MCORE_UDP_OPEN_ERR);
            break;
        case LWM2MCORE_UDP_CLOSE_ERR:
            str = STRINGIZE_EXPAND(LWM2MCORE_UDP_CLOSE_ERR);
            break;
        case LWM2MCORE_UDP_SEND_ERR:
            str = STRINGIZE_EXPAND(LWM2MCORE_UDP_SEND_ERR);
            break;
        case LWM2MCORE_UDP_RECV_ERR:
            str = STRINGIZE_EXPAND(LWM2MCORE_UDP_RECV_ERR);
            break;
        case LWM2MCORE_UDP_CONNECT_ERR:
            str = STRINGIZE_EXPAND(LWM2MCORE_UDP_CONNECT_ERR);
            break;
        default:
            str = "";
    }

    return str;
};

//--------------------------------------------------------------------------------------------------
/**
 * Report communication information
 */
//--------------------------------------------------------------------------------------------------
static void ReportInfo
(
    CommInfo_t *infoPtr
)
{
    if (CommEventId)
    {
        le_event_Report(CommEventId, infoPtr, sizeof(CommInfo_t));
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get cellular network information
 */
//--------------------------------------------------------------------------------------------------
static void CellularInfo
(
    int32_t index
)
{
    le_mdc_ProfileRef_t profile;
    le_mdc_Pdp_t pdp;
    CommInfo_t info;

    profile = le_mdc_GetProfile(index);

    info.code = LE_AVC_COMM_INFO_PDP_CONTEXT;

    pdp = le_mdc_GetPDP(profile);
    switch (pdp)
    {
        case LE_MDC_PDP_IPV4:
            snprintf(info.str, LE_AVC_COMM_INFO_STR_MAX_LEN, "IPV4");
            break;
        case LE_MDC_PDP_IPV6:
            snprintf(info.str, LE_AVC_COMM_INFO_STR_MAX_LEN, "IPV6");
            break;
        case LE_MDC_PDP_IPV4V6:
            snprintf(info.str, LE_AVC_COMM_INFO_STR_MAX_LEN, "IPV4V6");
            break;
        default:
            snprintf(info.str, LE_AVC_COMM_INFO_STR_MAX_LEN, "UNKNOWN");
            break;
    }

    LE_DEBUG("PDP type %s", info.str);

    ReportInfo(&info);
}

//--------------------------------------------------------------------------------------------------
/**
 * Data handler
 */
//--------------------------------------------------------------------------------------------------
static void DataHandler
(
    const char  *ifNamePtr,
    bool        isConnected,
    void        *ctxPtr
)
{
    CommInfo_t info;

    if (strlen(ifNamePtr))
    {
        memset(IfName, 0, LE_DATA_INTERFACE_NAME_MAX_LEN);
        memcpy(IfName, ifNamePtr, LE_DATA_INTERFACE_NAME_MAX_LEN);
    }

    if (!isConnected)
    {
        LE_DEBUG("%s disconnected", IfName);
        info.code = LE_AVC_COMM_INFO_BEARER_DOWN;
        snprintf(info.str, LE_AVC_COMM_INFO_STR_MAX_LEN, "%s disconnected", IfName);
        ReportInfo(&info);
        return;
    }

    LE_DEBUG("Connected through %s", IfName);
    info.code = LE_AVC_COMM_INFO_BEARER_UP;
    snprintf(info.str, LE_AVC_COMM_INFO_STR_MAX_LEN, "Connected through %s", IfName);
    ReportInfo(&info);

    if (LE_DATA_CELLULAR == le_data_GetTechnology())
    {
        CellularInfo(le_data_GetCellularProfileIndex());
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer communication errors handler
 */
//--------------------------------------------------------------------------------------------------
static void CommInfoHandler
(
    void* reportPtr,
    void* func
)
{
    CommInfo_t* infoPtr = reportPtr;
    le_avc_CommInfoHandlerFunc_t clientFunc = func;

    clientFunc(infoPtr->code, infoPtr->str, le_event_GetContextPtr());
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to report UDP error code.
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_ReportUdpErrorCode
(
    int code
)
{
    CommInfo_t info;

    info.code = (uint8_t)code;
    snprintf(info.str, LE_AVC_COMM_INFO_STR_MAX_LEN, "%s", UdpErrorToStr(code));

    ReportInfo(&info);

    LE_DEBUG("UDP error is %d: %s", code, UdpErrorToStr(code));
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to report CoAP response code.
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_ReportCoapResponseCode
(
    int code
)
{
    CommInfo_t info;

    info.code = (uint8_t)code;
    snprintf(info.str, LE_AVC_COMM_INFO_STR_MAX_LEN, "COAP %d.%.2d", CLASS(code), DETAILS(code));

    ReportInfo(&info);

    LE_DEBUG("Received response code %d: %s", info.code, info.str);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register a handler function in order to receive communication information notifications
 * @note This function creates the event handler ID if not yet created
 */
//--------------------------------------------------------------------------------------------------
le_avc_CommInfoHandlerRef_t le_avc_AddCommInfoHandler
(
    le_avc_CommInfoHandlerFunc_t func,
    void*                       ctxPtr
)
{
    le_event_HandlerRef_t ref;

    if (!func)
    {
        LE_ERROR("Invalid parameter");
        return NULL;
    }

    if (!CommEventId)
    {
        CommEventId = le_event_CreateId("AvcCommInfo", sizeof(CommInfo_t));
    }

    ref = le_event_AddLayeredHandler("AvcCommInfo", CommEventId, CommInfoHandler, func);

    le_event_SetContextPtr(ref, ctxPtr);

    DataHandlerRef = le_data_AddConnectionStateHandler(DataHandler, NULL);

    return (le_avc_CommInfoHandlerRef_t)ref;
}

//--------------------------------------------------------------------------------------------------
/**
 * Unregister a handler
 */
//--------------------------------------------------------------------------------------------------
void le_avc_RemoveCommInfoHandler
(
    le_avc_CommInfoHandlerRef_t ref
)
{
    if (NULL != DataHandlerRef)
    {
        le_data_RemoveConnectionStateHandler(DataHandlerRef);
    }

    if (NULL != ref)
    {
        le_event_RemoveHandler((le_event_HandlerRef_t)ref);
    }
}
