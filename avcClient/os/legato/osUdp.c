/**
 * @file osUdp.c
 *
 * Adaptation layer for UDP socket management
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netdb.h>
#include <resolv.h>
#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/udp.h>
#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
/**
 *  Define the File Descriptor Monitor reference for socket
 */
//--------------------------------------------------------------------------------------------------
le_fdMonitor_Ref_t Lwm2mMonitorRef;

//--------------------------------------------------------------------------------------------------
/**
 * Local port for socket
 */
//--------------------------------------------------------------------------------------------------
#define LOCAL_PORT  "56830"

lwm2mcore_SocketConfig_t SocketConfig;

static lwm2mcore_UdpCb_t udpCb = NULL;

//--------------------------------------------------------------------------------------------------
/**
 *  lwm2m client receive monitor.
 */
//--------------------------------------------------------------------------------------------------
static void Lwm2mClientReceive
(
    int readfs,     ///< [IN] File descriptor
    short events    ///< [IN] Bit map of events that occurred
)
{
    // POLLOUT event must be fired to invoke this routine. POLLHUP is mutually exclusive with
    // POLLOUT i.e. this routine should be called when POLLOUT or POLLOUT|POLLERR event fire.
    // LE_ASSERT((events == POLLOUT) || (events == (POLLOUT | POLLERR)));

    uint8_t buffer[LWM2MCORE_UDP_MAX_PACKET_SIZE];
    int numBytes;

    LE_DEBUG("Lwm2mClientReceive events %d", events);

    // If an event happens on the socket
    if (events == POLLIN)
    {
        struct sockaddr_storage addr;
        socklen_t addrLen;

        addrLen = sizeof(addr);

        // We retrieve the data received
        numBytes = recvfrom (readfs, buffer, LWM2MCORE_UDP_MAX_PACKET_SIZE,
                            0, (struct sockaddr *)&addr, &addrLen);

        if (0 > numBytes)
        {
            LE_ERROR("Error in receiving lwm2m data: %d %s.", errno, strerror(errno));
        }
        else if (0 < numBytes)
        {
            char s[INET6_ADDRSTRLEN];
            in_port_t port = 0;

            LE_DEBUG("Lwm2mClientReceive numBytes %d", numBytes);

            if (AF_INET == addr.ss_family)
            {
                struct sockaddr_in *saddr = (struct sockaddr_in *)&addr;
                inet_ntop(saddr->sin_family, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
                port = saddr->sin_port;
            }
            else if (AF_INET6 == addr.ss_family)
            {
                struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)&addr;
                inet_ntop(saddr->sin6_family, &saddr->sin6_addr, s, INET6_ADDRSTRLEN);
                port = saddr->sin6_port;
            }

            LE_DEBUG("%d bytes received from [%s]:%hu.", numBytes, s, ntohs(port));
            //lwm2mcore_DataDump ("received bytes", buffer, numBytes);

            if (udpCb != NULL)
            {
                /* Call the registered UDP callback */
                udpCb (buffer, (uint32_t)numBytes, &addr, addrLen, SocketConfig);
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Create a socket
 *
 * @return
 *      - socket id on success
 *      - -1 on error
 *
 */
//--------------------------------------------------------------------------------------------------
static int CreateSocket
(
    const char * portStr,
    lwm2mcore_SocketConfig_t config
)
{
    int s = -1;
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = config.af;
    hints.ai_socktype = config.proto;
    hints.ai_flags = AI_PASSIVE;

    if (0 != getaddrinfo(NULL, portStr, &hints, &res))
    {
        return -1;
    }

    for(p = res ; p != NULL && s == -1 ; p = p->ai_next)
    {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s >= 0)
        {
            if (-1 == bind(s, p->ai_addr, p->ai_addrlen))
            {
                close(s);
                s = -1;
            }
        }
    }

    freeaddrinfo(res);

    return s;
}

//--------------------------------------------------------------------------------------------------
/**
 * Resolve the server address name
 *
 * @return
 *      - LE_OK     on success
 *      - LE_FAULT  on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ResolveIpAddress
(
    char* urlStrPtr,    ///< [IN] Server name to resolve
    char* ipStrPtr      ///< [IN] Resolved server IP address
)
{
    int rc;
    struct addrinfo* resultPtr;
    struct addrinfo* nextPtr;
    struct addrinfo hints;
    struct sockaddr_in *serverPtr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(urlStrPtr, NULL, &hints, &resultPtr);
    if (rc)
    {
        LE_ERROR("IP %s not resolved: %s", urlStrPtr,  gai_strerror(rc));
        return LE_FAULT;
    }

    for (nextPtr = resultPtr; nextPtr != NULL; nextPtr = nextPtr->ai_next)
    {
        serverPtr = (struct sockaddr_in*) nextPtr->ai_addr;
        strncpy(ipStrPtr, inet_ntoa(serverPtr->sin_addr), strlen(inet_ntoa(serverPtr->sin_addr)));
        LE_DEBUG("Hostname IP Address %s", ipStrPtr);
        freeaddrinfo(resultPtr);
        return LE_OK;
    }
    LE_ERROR("IP %s not resolved", urlStrPtr);
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Extract the server name to be resolved
 *
 * @return
 *      - char*     server name to be resolved
 */
//--------------------------------------------------------------------------------------------------
static char* ExtractServerName
(
    char* urlStrPtr     ///< [IN] Server URL to extract
)
{
    // Check if protocol is present in the URL
    char* urlTempPtr = strrchr(urlStrPtr, '/');
    if(urlTempPtr)
    {
        urlStrPtr = urlTempPtr + 1;
    }

    // Check if port is present in the url
    urlTempPtr = strchr(urlStrPtr, ':');
    if (urlTempPtr)
    {
        // Stop URL string on ':' char
        *urlTempPtr = '\0';
    }
    return urlStrPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 * Resolve server IP address name
 *
 * @return
 *      - LE_OK             on success
 *      - LE_BAD_PARAMETER  NULL pointer provided
 *      - LE_FAULT          on failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ResolveServerIp
(
    char * urlStrPtr,   ///< [IN] Server URL to resolve
    char * ipStrPtr     ///< [OUT] Resolved server IP address
)
{
    if ((NULL == urlStrPtr)
     || (NULL == ipStrPtr) )
    {
        return LE_BAD_PARAMETER;
    }

    if (!strlen(urlStrPtr))
    {
        return LE_FAULT;
    }

    LE_DEBUG("Try to resolve %s", urlStrPtr);

    // Resolve server address
    return ResolveIpAddress(ExtractServerName(urlStrPtr), ipStrPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Open a socket to the server
 * This function is called by the LWM2MCore and must be adapted to the platform
 * The aim of this function is to create a socket and fill the configPtr structure
 *
 * @return
 *      - true on success
 *      - false on error
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_UdpOpen
(
    lwm2mcore_Ref_t instanceRef,            ///< [IN] LWM2M instance reference
    lwm2mcore_UdpCb_t callback,             ///< [IN] callback for data receipt
    lwm2mcore_SocketConfig_t* configPtr     ///< [INOUT] socket configuration
)
{
    bool result = false;

    le_mdc_ProfileRef_t profileRef = le_mdc_GetProfile(le_data_GetCellularProfileIndex());

    if (le_mdc_IsIPv6(profileRef) && !le_mdc_IsIPv4(profileRef))
    {
        SocketConfig.af = AF_INET6;
    }
    else
    {
        SocketConfig.af = AF_UNSPEC;
    }

    SocketConfig.instanceRef = instanceRef;
    SocketConfig.type = LWM2MCORE_SOCK_TYPE_MAX;
    SocketConfig.proto = LWM2MCORE_SOCK_UDP;
    SocketConfig.sock = CreateSocket(LOCAL_PORT, SocketConfig);
    LE_DEBUG ("sock %d", SocketConfig.sock);
    memcpy (configPtr, &SocketConfig, sizeof (lwm2mcore_SocketConfig_t));

    if (SocketConfig.sock < 0)
    {
        LE_FATAL ("Failed to open socket: %d %s.", errno, strerror(errno));
    }
    else
    {
        Lwm2mMonitorRef = le_fdMonitor_Create ("LWM2M Client",
                                                SocketConfig.sock,
                                                Lwm2mClientReceive,
                                                POLLIN);
        if (Lwm2mMonitorRef != NULL)
        {
            result = true;
            // Register the callback
            udpCb = callback;
        }
    }

    LE_DEBUG ("lwm2mcore_UdpOpen %d", result);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Close the socket
 * This function is called by the LWM2MCore and must be adapted to the platform
 * The aim of this function is to close a socket
 *
 * @return
 *      - true on success
 *      - false on error
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_UdpClose
(
    lwm2mcore_SocketConfig_t config        ///< [INOUT] socket configuration
)
{
    bool result = false;
    int rc = 0;

    rc = close (config.sock);
    LE_DEBUG ("close sock %d -> %d", config.sock, rc);
    if (0 == rc)
    {
        result = true;
    }

    LE_DEBUG ("lwm2mcore_UdpClose %d", result);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send data on a socket
 * This function is called by the LWM2MCore and must be adapted to the platform
 * The aim of this function is to send data on a socket
 *
 * @return
 *      -
 *      - false on error
 *
 */
//--------------------------------------------------------------------------------------------------
ssize_t lwm2mcore_UdpSend
(
    int sockfd,
    const void *bufferPtr,
    size_t length,
    int flags,
    const struct sockaddr *dest_addrPtr,
    socklen_t addrlen
)
{
    return sendto(sockfd, bufferPtr, length, flags, dest_addrPtr, addrlen);
}

//--------------------------------------------------------------------------------------------------
/**
 * Connect a socket
 * This function is called by the LWM2MCore and must be adapted to the platform
 * The aim of this function is to send data on a socket
 *
 * @return
 *      - true  on success
 *      - false on error
 *
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_UdpConnect
(
    char* serverAddressPtr,             ///< [IN] Server address URL
    char* hostPtr,                      ///< [IN] Host
    char* portPtr,                      ///< [IN] Port
    int addressFamily,                  ///< [IN] Address familly
    struct sockaddr* saPtr,             ///< [IN] Socket address pointer
    socklen_t* slPtr,                   ///< [IN] Socket address length
    int* sockPtr                        ///< [IN] socket file descriptor
)
{
    char ipAddressStr[LE_MDC_IPV6_ADDR_MAX_BYTES] = {0};
    le_result_t res;

    // Resolve the server address
    res = ResolveServerIp(serverAddressPtr, ipAddressStr);
    if (LE_OK == res)
    {
        struct addrinfo hints;
        struct addrinfo* servinfoPtr = NULL;
        struct addrinfo* p;
        int sockfd;

        // Add the route if the default route is not set by the data connection service
        if (!le_data_GetDefaultRouteStatus())
        {
            LE_INFO("Add route %s", ipAddressStr);
            res = le_data_AddRoute(ipAddressStr);
            LE_ERROR_IF((LE_OK != res), "Not able to add the route (%s)", LE_RESULT_TXT(res));
        }

        // connect
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = addressFamily;
        hints.ai_socktype = SOCK_DGRAM;

        if ((0 != getaddrinfo(hostPtr, portPtr, &hints, &servinfoPtr)) || (servinfoPtr == NULL))
        {
            return false;
        }

        // we test the various addresses
        sockfd = -1;
        for(p = servinfoPtr; (p != NULL) && (sockfd == -1); p = p->ai_next)
        {
            sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

            if (sockfd >= 0)
            {
                *slPtr = p->ai_addrlen;
                memcpy(saPtr, p->ai_addr, p->ai_addrlen);

                if (-1 == connect(sockfd, p->ai_addr, p->ai_addrlen))
                {
                    close(sockfd);
                    sockfd = -1;
                }
            }
        }
        *sockPtr = sockfd;
        if (NULL != servinfoPtr)
        {
            freeaddrinfo(servinfoPtr);
        }
        return true;
    }
    return false;
}
