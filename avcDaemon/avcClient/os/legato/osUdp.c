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

#include "legato.h"
#include "interfaces.h"

#ifdef LE_CONFIG_LINUX
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#   include <sys/stat.h>
#   include <netdb.h>
#   include <resolv.h>
#endif

#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/udp.h>

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
        if ((numBytes < 0) && (errno == EBADF))
        {
            LE_DEBUG("Received on closed socket, ignoring");
            return;
        }

        if (0 > numBytes)
        {
            LE_ERROR("Error in receiving lwm2m data: %d %s.", errno, LE_ERRNO_TXT(errno));
        }
        else if (0 < numBytes)
        {
            char s[INET6_ADDRSTRLEN];
            __attribute__((unused)) in_port_t port = 0;

            LE_DEBUG("Lwm2mClientReceive numBytes %d", numBytes);

            if (AF_INET == addr.ss_family)
            {
                struct sockaddr_in *saddr = (struct sockaddr_in *)&addr;
                inet_ntop(saddr->sin_family, &saddr->sin_addr, s, INET_ADDRSTRLEN);
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

#ifdef LE_CONFIG_TARGET_GILL
//--------------------------------------------------------------------------------------------------
/**
 * Get the details information of the interface
 *
 * @return
 *      -  LE_OK on success
 *      -  LE_FAULT on error
 *
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetInterfaceDetails(char* iface_name, char* mdc_ipAddressStr)
{
    int32_t profileIndex_Cellular = le_data_GetCellularProfileIndex();
    le_mdc_ProfileRef_t mdc_ProfileRef = le_mdc_GetProfile(profileIndex_Cellular);
    le_result_t ret;
    if (mdc_ipAddressStr == NULL)
    {
        LE_ERROR("Invalid IP address buffer for iface %s", iface_name);
        return LE_FAULT;
    }
    if (mdc_ProfileRef == NULL)
    {
        LE_DEBUG("Cannot get profile index");
        return LE_FAULT;
    }

    if (le_mdc_GetInterfaceName(mdc_ProfileRef, iface_name, LE_MDC_INTERFACE_NAME_MAX_BYTES)
        != LE_OK)
    {
        LE_DEBUG("Cannot get interface name");
        return LE_FAULT;
    }

    if (le_mdc_IsIPv4(mdc_ProfileRef))
    {
        ret = le_mdc_GetIPv4Address(mdc_ProfileRef, mdc_ipAddressStr, LE_MDC_IPV6_ADDR_MAX_BYTES);
    }
    else if (le_mdc_IsIPv6(mdc_ProfileRef))
    {
        ret = le_mdc_GetIPv6Address(mdc_ProfileRef, mdc_ipAddressStr, LE_MDC_IPV6_ADDR_MAX_BYTES);
    }
    else
    {
        LE_ERROR("Cannot get IP address of the iface %s", iface_name);
        return LE_FAULT;
    }

    if (ret != LE_OK)
    {
        LE_ERROR("Failed to get IP address of the iface %s", iface_name);
        return LE_FAULT;
    }

    LE_INFO("IP address of the iface %s is %s", iface_name, mdc_ipAddressStr);
    return LE_OK;
}
#endif // LE_CONFIG_TARGET_GILL

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
    int enable = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = config.af;
    hints.ai_socktype = config.proto;
    hints.ai_flags = AI_PASSIVE;


    LE_DEBUG("Attempt to DNS-resolve service on port %s", portStr);

#ifdef LE_CONFIG_TARGET_GILL
    char iface_name[LE_MDC_INTERFACE_NAME_MAX_BYTES] = {0};
    char mdc_ipAddressStr[LE_MDC_IPV6_ADDR_MAX_BYTES] = {0};

    if (LE_OK != GetInterfaceDetails(iface_name, mdc_ipAddressStr))
    {
        LE_DEBUG("Cannot get the details information of iface %s ", iface_name);
        return -1;
    }

    if (LE_OK != getaddrinfo_on_iface(NULL, portStr, &hints, &res, iface_name))
        {
            LE_DEBUG("Cannot resolve DNS on iface %s ", iface_name);
            return -1;
        }
#else // LE_CONFIG_TARGET_GILL
    if (0 != getaddrinfo(NULL, portStr, &hints, &res))
    {
        LE_DEBUG("Cannot resolve DNS");
        return -1;
    }

#endif // LE_CONFIG_TARGET_GILL
    for(p = res ; p != NULL && s == -1 ; p = p->ai_next)
    {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s >= 0)
        {
            if(-1 == setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
            {
                close(s);
                s = - 1;
                continue;
            }

            if (-1 == bind(s, p->ai_addr, p->ai_addrlen))
            {
                close(s);
                s = -1;
            }
#ifdef LE_CONFIG_TARGET_GILL
 //Bind the socket to the specific cellular profile that config in WDSS command
            struct sockaddr_in clientAddr;
            memset(&clientAddr, 0, sizeof(clientAddr));
            clientAddr.sin_family = AF_INET;
            clientAddr.sin_addr.s_addr = inet_addr(mdc_ipAddressStr);
            if (bind(s, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0)
            {
                close(s);
                s = -1;
            }
#endif // LE_CONFIG_TARGET_GILL
        }
    }

    freeaddrinfo(res);
    return s;
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
        SocketConfig.af = (lwm2mcore_SocketAf_t)AF_INET6;
    }
    else if (le_mdc_IsIPv4(profileRef) && !le_mdc_IsIPv6(profileRef))
    {
        SocketConfig.af = (lwm2mcore_SocketAf_t)AF_INET;
    }
    else
    {
        SocketConfig.af = (lwm2mcore_SocketAf_t)AF_UNSPEC;
    }

    SocketConfig.instanceRef = instanceRef;
    SocketConfig.type = LWM2MCORE_SOCK_TYPE_MAX;
    SocketConfig.proto = LWM2MCORE_SOCK_UDP;
    SocketConfig.sock = CreateSocket(LOCAL_PORT, SocketConfig);
    LE_DEBUG("sock %d", SocketConfig.sock);
    memcpy (configPtr, &SocketConfig, sizeof (lwm2mcore_SocketConfig_t));

    if (SocketConfig.sock < 0)
    {
        LE_ERROR("Failed to open socket: %d %s.", errno, LE_ERRNO_TXT(errno));
        return false;
    }

    Lwm2mMonitorRef = le_fdMonitor_Create ("LWM2M Client",
                                            SocketConfig.sock,
                                            Lwm2mClientReceive,
                                            POLLIN);
    LE_DEBUG("Opened lwm2m UDP socket %d with FD monitor %p", SocketConfig.sock, Lwm2mMonitorRef);

    if (Lwm2mMonitorRef != NULL)
    {
        result = true;
        // Register the callback
        udpCb = callback;
    }

    LE_DEBUG("lwm2mcore_UdpOpen %d", result);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Close the udp connection
 * This function is called by the LWM2MCore and must be adapted to the platform
 * The aim of this function is to close a udp connection
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

    if (config.sock == SocketConfig.sock)
    {
        int rc = 0;
        // Delete FD monitor if the socket was opened in lwm2mcore_UdpOpen().
        LE_DEBUG("Closed lwm2m UDP socket %d with FD monitor %p", config.sock, Lwm2mMonitorRef);
        le_fdMonitor_Delete(Lwm2mMonitorRef);

        rc = close (config.sock);
        if (0 == rc)
        {
            result = true;
        }
    }

    LE_DEBUG ("lwm2mcore_UdpClose %d", result);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Close the provided socket
 * This function is called by the LWM2MCore and must be adapted to the platform.
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_UdpSocketClose
(
    int sockFd             ///< [IN] socket file descriptor
)
{
    close(sockFd);
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
    return sendto(sockfd, (void*)bufferPtr, length, flags, (struct sockaddr *)dest_addrPtr, addrlen);
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
    struct sockaddr* saPtr,             ///< [OUT] Socket address pointer
    socklen_t* slPtr,                   ///< [OUT] Socket address length
    int* sockPtr                        ///< [OUT] socket file descriptor
)
{
    char ipAddressStr[LE_MDC_IPV6_ADDR_MAX_BYTES] = {0};
    le_result_t res;
    int rc;
    struct addrinfo* resultPtr;
    struct addrinfo* nextPtr;
    struct addrinfo hints;
    bool successfullyConnected = false;
    int sockfd = -1;
    int enable = 1;

    // Make sure all these pointers are not NULL
    if ((!serverAddressPtr) || (!hostPtr) || (!portPtr) || (!saPtr) || (!slPtr) || (!sockPtr))
    {
        LE_ERROR("Missing parameters or NULL parameters passed into function");
        return false;
    }

    if (!strlen(serverAddressPtr))
    {
        LE_ERROR("No server address was passed into function");
        return false;
    }
    char* urlStrPtr = ExtractServerName(serverAddressPtr);
    LE_DEBUG("lwm2mcore_UdpConnect: urlStrPtr %s", urlStrPtr);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = addressFamily;
    hints.ai_socktype = SOCK_DGRAM;

    LE_DEBUG("Attempt to DNS-resolve url: '%s', with host name: '%s', and on port: '%s'",
            urlStrPtr, hostPtr, portPtr);

#ifdef LE_CONFIG_LINUX
// Code block is written as per manpage: https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
// However, EAI_AGAIN/EAI_SYSTEM only available on linux and missing in rtos platform. Hence, it is
// put under separate code block.
    do
    {
        rc = getaddrinfo(urlStrPtr, portPtr, &hints, &resultPtr);
    }
    while (EAI_AGAIN == rc);
    if (rc)
    {
        if (EAI_SYSTEM == rc)
        {
            LE_ERROR("IP %s not resolved: %s, %m", urlStrPtr, gai_strerror(rc));
        }
        else
        {
            LE_ERROR("IP %s not resolved: %s", urlStrPtr, gai_strerror(rc));
        }
        return false;
    }
#else
#ifdef LE_CONFIG_TARGET_GILL
    char iface[LE_MDC_INTERFACE_NAME_MAX_BYTES] = {0};
    res = GetInterfaceDetails(iface, NULL);

    if (LE_OK == res)
    {
        LE_INFO("Resolve DNS on iface %s", iface);
        rc = getaddrinfo_on_iface(urlStrPtr, portPtr, &hints, &resultPtr, iface);
    }
    else
    {
        LE_INFO("Trying to resolve DNS with default interface");
        rc = getaddrinfo_on_iface(urlStrPtr, portPtr, &hints, &resultPtr, NULL);
    }
#else // LE_CONFIG_TARGET_GILL
    rc = getaddrinfo(urlStrPtr, portPtr, &hints, &resultPtr);
#endif // LE_CONFIG_TARGET_GILL

    if (rc)
    {
        LE_ERROR("IP %s not resolved: %s", urlStrPtr, gai_strerror(rc));
        return false;
    }
#endif

    // We test the various addresses and break once we've successfully connected to one
    for (nextPtr = resultPtr; nextPtr != NULL && sockfd == -1; nextPtr = nextPtr->ai_next)
    {
        sockfd = socket(nextPtr->ai_addr->sa_family, nextPtr->ai_socktype, nextPtr->ai_protocol);
        if (sockfd >= 0)
        {
            *slPtr = nextPtr->ai_addrlen;
            memcpy(saPtr, nextPtr->ai_addr, nextPtr->ai_addrlen);

            if (nextPtr->ai_addr->sa_family == AF_INET)
            {
                struct sockaddr_in *serverPtr = (struct sockaddr_in*) nextPtr->ai_addr;
                int bytesTriedToCopy = snprintf(ipAddressStr, sizeof(ipAddressStr), "%s",
                                                inet_ntoa(serverPtr->sin_addr));
                if(bytesTriedToCopy < 0)
                {
                    LE_ERROR("Error occurred while copying IP address %m");
                }
                else if(bytesTriedToCopy >= sizeof(ipAddressStr))
                {
                    LE_ERROR("Buffer isn't large enough to store entire IP address");
                }
                else
                {
                    LE_DEBUG("Found possible Hostname IP Address %s", ipAddressStr);
                }
            }
            else if (nextPtr->ai_addr->sa_family == AF_INET6)
            {
                struct sockaddr_in6 *serverPtr = (struct sockaddr_in6*) nextPtr->ai_addr;
                inet_ntop(nextPtr->ai_addr->sa_family,
                        &serverPtr->sin6_addr,
                        ipAddressStr,
                        LE_MDC_IPV6_ADDR_MAX_BYTES);
                LE_DEBUG("Found possible Hostname IP Address %s", ipAddressStr);
            }
            else
            {
                LE_DEBUG("Unknown Address Family");
            }

            if(-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
            {
                close(sockfd);
                sockfd = - 1;
                continue;
            }

            // Add the route if the default route is not set by the data connection service
            if (!le_data_GetDefaultRouteStatus())
            {
                LE_DEBUG("Add route %s", ipAddressStr);
                res = le_data_AddRoute(ipAddressStr);
                LE_ERROR_IF((LE_OK != res), "Not able to add the route (%s)", LE_RESULT_TXT(res));
            }

            if (-1 == connect(sockfd, nextPtr->ai_addr, nextPtr->ai_addrlen))
            {
                close(sockfd);
                sockfd = -1;
            }
            else
            {
                LE_DEBUG("Connection accepted at Hostname IP: %s", ipAddressStr);
                successfullyConnected = true;
                break;
            }
        }
    }

    // If connection is denied for all the possible hostnames IP
    if (!successfullyConnected)
    {
        freeaddrinfo(resultPtr);
        LE_ERROR("Unable to establish any connection to %s", urlStrPtr);
        return false;
    }

    *sockPtr = sockfd;
    freeaddrinfo(resultPtr);

    return true;
}
