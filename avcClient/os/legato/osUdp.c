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
const char * localPortPtr = "56830";

lwm2mcore_SocketConfig_t SocketConfig;

static lwm2mcore_UdpCb_t udpCb = NULL;

#define OS_SOCK_AF  AF_INET
#define OS_SOCK_PROTO SOCK_DGRAM

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

    LE_INFO("Lwm2mClientReceive events %d", events);

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
            in_port_t port;

            LE_INFO("Lwm2mClientReceive numBytes %d", numBytes);

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

            LE_WARN("%d bytes received from [%s]:%hu.", numBytes, s, ntohs(port));
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
static int createSocket
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

    /* IP v4 */
    SocketConfig.instanceRef = instanceRef;
    SocketConfig.af = OS_SOCK_AF;
    SocketConfig.type = LWM2MCORE_SOCK_UDP;
    SocketConfig.proto = OS_SOCK_PROTO;
    SocketConfig.sock = createSocket (localPortPtr, SocketConfig);
    LE_INFO ("sock %d", SocketConfig.sock);
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

    LE_INFO ("lwm2mcore_UdpOpen %d", result);
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
    LE_INFO ("close sock %d -> %d", config.sock, rc);
    if (rc == 0)
{
        result = true;
    }

    LE_INFO ("lwm2mcore_UdpClose %d", result);
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

