 /**
 * @file dtlsConnection.h
 *
 * Header file for DTLS connection stub
 *
 */

#ifndef DTLS_CONNECTION_H_
#define DTLS_CONNECTION_H_

//--------------------------------------------------------------------------------------------------
/**
 * @brief Define value for the DTLS rehandshake: after 40 seconds of inactivity, a rehandshake is
 * needed in order to send any data to the server
 */
//--------------------------------------------------------------------------------------------------
#define DTLS_NAT_TIMEOUT 40

//--------------------------------------------------------------------------------------------------
/**
 * @brief Define short value for the DTLS rehandshake: after 5 seconds of inactivity
 */
//--------------------------------------------------------------------------------------------------
#define DTLS_SHORT_NAT_TIMEOUT 5

//--------------------------------------------------------------------------------------------------
/**
 * Set the DTLS NAT timeout (stub)
 */
//--------------------------------------------------------------------------------------------------
void dtls_SetNatTimeout
(
    uint32_t        timeout        ///< [IN] Timeout (unit: seconds)
);

#endif