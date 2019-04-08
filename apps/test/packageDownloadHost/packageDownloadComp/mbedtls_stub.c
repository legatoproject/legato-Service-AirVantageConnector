/**
 * @file mbedtls_stub.c
 *
 * This file is stubbed version of mbedtls file
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <stdio.h>
#include <string.h>
#include "mbedtls/ssl.h"
#include "mbedtls/pem.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/pkcs12.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/padlock.h"
#include "mbedtls/base64.h"
#include "mbedtls/ccm.h"
#include "mbedtls/gcm.h"
#include "mbedtls/oid.h"
#include "mbedtls/error.h"

#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * \brief Translate a mbed TLS error code into a string representation,
 *        Result is truncated if necessary and always includes a terminating
 *        null byte.
 *
 * \param errnum    error code
 * \param bufPtr    buffer to place representation in
 * \param buflen    length of the buffer
 */
//--------------------------------------------------------------------------------------------------
void mbedtls_strerror(
    int     ret,        ///< [IN] error code
    char*   bufPtr,     ///< [IN] buffer to place representation in
    size_t  buflen      ///< [IN] length of the buffer
)
{
    size_t len;
    int use_ret;

    if( buflen == 0 )
    {
        return;
    }

    memset( bufPtr, 0x00, buflen );

    if( ret < 0 )
    {
        ret = -ret;
    }

    if( ret & 0xFF80 )
    {
        use_ret = ret & 0xFF80;

        // High level error codes
        //
        // BEGIN generated code
#if defined(MBEDTLS_CIPHER_C)
        if( use_ret == -(MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE) )
        {
            snprintf(bufPtr, buflen, "CIPHER - The selected feature is not available");
        }
        if( use_ret == -(MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "CIPHER - Bad input parameters to function");
        }
        if( use_ret == -(MBEDTLS_ERR_CIPHER_ALLOC_FAILED) )
        {
            snprintf(bufPtr, buflen, "CIPHER - Failed to allocate memory");
        }
        if( use_ret == -(MBEDTLS_ERR_CIPHER_INVALID_PADDING) )
        {
            snprintf(bufPtr,
                      buflen,
                      "CIPHER - Input data contains invalid padding and is rejected");
        }
        if( use_ret == -(MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED) )
        {
            snprintf(bufPtr, buflen, "CIPHER - Decryption of block requires a full block");
        }
        if( use_ret == -(MBEDTLS_ERR_CIPHER_AUTH_FAILED) )
        {
            snprintf(bufPtr, buflen, "CIPHER - Authentication failed (for AEAD modes)");
        }
        if( use_ret == -(MBEDTLS_ERR_CIPHER_INVALID_CONTEXT) )
        {
            snprintf(bufPtr, buflen, "CIPHER - The context is invalid, eg because it was free()ed");
        }
#endif /* MBEDTLS_CIPHER_C */

#if defined(MBEDTLS_DHM_C)
        if( use_ret == -(MBEDTLS_ERR_DHM_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "DHM - Bad input parameters to function");
        }
        if( use_ret == -(MBEDTLS_ERR_DHM_READ_PARAMS_FAILED) )
        {
            snprintf(bufPtr, buflen, "DHM - Reading of the DHM parameters failed");
        }
        if( use_ret == -(MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED) )
        {
            snprintf(bufPtr, buflen, "DHM - Making of the DHM parameters failed");
        }
        if( use_ret == -(MBEDTLS_ERR_DHM_READ_PUBLIC_FAILED) )
        {
            snprintf(bufPtr, buflen, "DHM - Reading of the public values failed");
        }
        if( use_ret == -(MBEDTLS_ERR_DHM_MAKE_PUBLIC_FAILED) )
        {
            snprintf(bufPtr, buflen, "DHM - Making of the public value failed");
        }
        if( use_ret == -(MBEDTLS_ERR_DHM_CALC_SECRET_FAILED) )
        {
            snprintf(bufPtr, buflen, "DHM - Calculation of the DHM secret failed");
        }
        if( use_ret == -(MBEDTLS_ERR_DHM_INVALID_FORMAT) )
        {
            snprintf(bufPtr, buflen, "DHM - The ASN.1 data is not formatted correctly");
        }
        if( use_ret == -(MBEDTLS_ERR_DHM_ALLOC_FAILED) )
        {
            snprintf(bufPtr, buflen, "DHM - Allocation of memory failed");
        }
        if( use_ret == -(MBEDTLS_ERR_DHM_FILE_IO_ERROR) )
        {
            snprintf(bufPtr, buflen, "DHM - Read/write of file failed");
        }
#endif /* MBEDTLS_DHM_C */

#if defined(MBEDTLS_ECP_C)
        if( use_ret == -(MBEDTLS_ERR_ECP_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "ECP - Bad input parameters to function");
        }
        if( use_ret == -(MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL) )
        {
            snprintf(bufPtr, buflen, "ECP - The buffer is too small to write to");
        }
        if( use_ret == -(MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE) )
        {
            snprintf(bufPtr, buflen, "ECP - Requested curve not available");
        }
        if( use_ret == -(MBEDTLS_ERR_ECP_VERIFY_FAILED) )
        {
            snprintf(bufPtr, buflen, "ECP - The signature is not valid");
        }
        if( use_ret == -(MBEDTLS_ERR_ECP_ALLOC_FAILED) )
        {
            snprintf(bufPtr, buflen, "ECP - Memory allocation failed");
        }
        if( use_ret == -(MBEDTLS_ERR_ECP_RANDOM_FAILED) )
        {
            snprintf(bufPtr, buflen,
                      "ECP - Generation of random value, such as (ephemeral) key, failed");
        }
        if( use_ret == -(MBEDTLS_ERR_ECP_INVALID_KEY) )
        {
            snprintf(bufPtr, buflen, "ECP - Invalid private or public key");
        }
        if( use_ret == -(MBEDTLS_ERR_ECP_SIG_LEN_MISMATCH) )
        {
            snprintf(bufPtr, buflen,
                      "ECP - Signature is valid but shorter than the user-supplied length");
        }
#endif /* MBEDTLS_ECP_C */

#if defined(MBEDTLS_MD_C)
        if( use_ret == -(MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE) )
        {
            snprintf(bufPtr, buflen, "MD - The selected feature is not available");
        }
        if( use_ret == -(MBEDTLS_ERR_MD_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "MD - Bad input parameters to function");
        }
        if( use_ret == -(MBEDTLS_ERR_MD_ALLOC_FAILED) )
        {
            snprintf(bufPtr, buflen, "MD - Failed to allocate memory");
        }
        if( use_ret == -(MBEDTLS_ERR_MD_FILE_IO_ERROR) )
        {
            snprintf(bufPtr, buflen, "MD - Opening or reading of file failed");
        }
#endif /* MBEDTLS_MD_C */

#if defined(MBEDTLS_PEM_PARSE_C) || defined(MBEDTLS_PEM_WRITE_C)
        if( use_ret == -(MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) )
        {
            snprintf(bufPtr, buflen, "PEM - No PEM header or footer found");
        }
        if( use_ret == -(MBEDTLS_ERR_PEM_INVALID_DATA) )
        {
            snprintf(bufPtr, buflen, "PEM - PEM string is not as expected");
        }
        if( use_ret == -(MBEDTLS_ERR_PEM_ALLOC_FAILED) )
        {
            snprintf(bufPtr, buflen, "PEM - Failed to allocate memory");
        }
        if( use_ret == -(MBEDTLS_ERR_PEM_INVALID_ENC_IV) )
        {
            snprintf(bufPtr, buflen, "PEM - RSA IV is not in hex-format");
        }
        if( use_ret == -(MBEDTLS_ERR_PEM_UNKNOWN_ENC_ALG) )
        {
            snprintf(bufPtr, buflen, "PEM - Unsupported key encryption algorithm");
        }
        if( use_ret == -(MBEDTLS_ERR_PEM_PASSWORD_REQUIRED) )
        {
            snprintf(bufPtr, buflen, "PEM - Private key password can't be empty");
        }
        if( use_ret == -(MBEDTLS_ERR_PEM_PASSWORD_MISMATCH) )
        {
            snprintf(bufPtr, buflen,
                      "PEM - Given private key password does not allow for correct decryption");
        }
        if( use_ret == -(MBEDTLS_ERR_PEM_FEATURE_UNAVAILABLE) )
        {
            snprintf(bufPtr, buflen,
                      "PEM - Unavailable feature, e.g. hashing/encryption combination");
        }
        if( use_ret == -(MBEDTLS_ERR_PEM_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "PEM - Bad input parameters to function");
        }
#endif /* MBEDTLS_PEM_PARSE_C || MBEDTLS_PEM_WRITE_C */

#if defined(MBEDTLS_PK_C)
        if( use_ret == -(MBEDTLS_ERR_PK_ALLOC_FAILED) )
        {
            snprintf(bufPtr, buflen, "PK - Memory allocation failed");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_TYPE_MISMATCH) )
        {
            snprintf(bufPtr, buflen, "PK - Type mismatch, eg attempt to encrypt with an ECDSA key");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "PK - Bad input parameters to function");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_FILE_IO_ERROR) )
        {
            snprintf(bufPtr, buflen, "PK - Read/write of file failed");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_KEY_INVALID_VERSION) )
        {
            snprintf(bufPtr, buflen, "PK - Unsupported key version");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT) )
        {
            snprintf(bufPtr, buflen, "PK - Invalid key tag or value");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_UNKNOWN_PK_ALG) )
        {
            snprintf(bufPtr, buflen,
                      "PK - Key algorithm is unsupported (only RSA and EC are supported)");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_PASSWORD_REQUIRED) )
        {
            snprintf(bufPtr, buflen, "PK - Private key password can't be empty");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_PASSWORD_MISMATCH) )
        {
            snprintf(bufPtr, buflen,
                      "PK - Given private key password does not allow for correct decryption");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_INVALID_PUBKEY) )
        {
            snprintf(bufPtr, buflen,
                      "PK - The pubkey tag or value is invalid (only RSA and EC are supported)");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_INVALID_ALG) )
        {
            snprintf(bufPtr, buflen, "PK - The algorithm tag or value is invalid");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE) )
        {
            snprintf(bufPtr, buflen,
                      "PK - Elliptic curve is unsupported (only NIST curves are supported)");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE) )
        {
            snprintf(bufPtr, buflen, "PK - Unavailable feature, e.g. RSA disabled for RSA key");
        }
        if( use_ret == -(MBEDTLS_ERR_PK_SIG_LEN_MISMATCH) )
        {
            snprintf(bufPtr, buflen,
                      "PK - The signature is valid but its length is less than expected");
        }
#endif /* MBEDTLS_PK_C */

#if defined(MBEDTLS_PKCS12_C)
        if( use_ret == -(MBEDTLS_ERR_PKCS12_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "PKCS12 - Bad input parameters to function");
        }
        if( use_ret == -(MBEDTLS_ERR_PKCS12_FEATURE_UNAVAILABLE) )
        {
            snprintf(bufPtr, buflen,
                      "PKCS12 - Feature not available, e.g. unsupported encryption scheme");
        }
        if( use_ret == -(MBEDTLS_ERR_PKCS12_PBE_INVALID_FORMAT) )
        {
            snprintf(bufPtr, buflen, "PKCS12 - PBE ASN.1 data not as expected");
        }
        if( use_ret == -(MBEDTLS_ERR_PKCS12_PASSWORD_MISMATCH) )
        {
            snprintf(bufPtr, buflen,
                      "PKCS12 - Given private key password does not allow for correct decryption");
        }
#endif /* MBEDTLS_PKCS12_C */

#if defined(MBEDTLS_PKCS5_C)
        if( use_ret == -(MBEDTLS_ERR_PKCS5_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "PKCS5 - Bad input parameters to function");
        }
        if( use_ret == -(MBEDTLS_ERR_PKCS5_INVALID_FORMAT) )
        {
            snprintf(bufPtr, buflen, "PKCS5 - Unexpected ASN.1 data");
        }
        if( use_ret == -(MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE) )
        {
            snprintf(bufPtr, buflen, "PKCS5 - Requested encryption or digest alg not available");
        }
        if( use_ret == -(MBEDTLS_ERR_PKCS5_PASSWORD_MISMATCH) )
        {
            snprintf(bufPtr, buflen,
                      "PKCS5 - Given private key password does not allow for correct decryption");
        }
#endif /* MBEDTLS_PKCS5_C */

#if defined(MBEDTLS_RSA_C)
        if( use_ret == -(MBEDTLS_ERR_RSA_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "RSA - Bad input parameters to function");
        }
        if( use_ret == -(MBEDTLS_ERR_RSA_INVALID_PADDING) )
        {
            snprintf(bufPtr, buflen, "RSA - Input data contains invalid padding and is rejected");
        }
        if( use_ret == -(MBEDTLS_ERR_RSA_KEY_GEN_FAILED) )
        {
            snprintf(bufPtr, buflen, "RSA - Something failed during generation of a key");
        }
        if( use_ret == -(MBEDTLS_ERR_RSA_KEY_CHECK_FAILED) )
        {
            snprintf(bufPtr, buflen, "RSA - Key failed to pass the library's validity check");
        }
        if( use_ret == -(MBEDTLS_ERR_RSA_PUBLIC_FAILED) )
        {
            snprintf(bufPtr, buflen, "RSA - The public key operation failed");
        }
        if( use_ret == -(MBEDTLS_ERR_RSA_PRIVATE_FAILED) )
        {
            snprintf(bufPtr, buflen, "RSA - The private key operation failed");
        }
        if( use_ret == -(MBEDTLS_ERR_RSA_VERIFY_FAILED) )
        {
            snprintf(bufPtr, buflen, "RSA - The PKCS#1 verification failed");
        }
        if( use_ret == -(MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE) )
        {
            snprintf(bufPtr, buflen, "RSA - The output buffer for decryption is not large enough");
        }
        if( use_ret == -(MBEDTLS_ERR_RSA_RNG_FAILED) )
        {
            snprintf(bufPtr, buflen, "RSA - The random generator failed to generate non-zeros");
        }
#endif /* MBEDTLS_RSA_C */

#if defined(MBEDTLS_SSL_TLS_C)
        if( use_ret == -(MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE) )
        {
            snprintf(bufPtr, buflen, "SSL - The requested feature is not available");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "SSL - Bad input parameters to function");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_INVALID_MAC) )
        {
            snprintf(bufPtr, buflen, "SSL - Verification of the message MAC failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_INVALID_RECORD) )
        {
            snprintf(bufPtr, buflen, "SSL - An invalid SSL record was received");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_CONN_EOF) )
        {
            snprintf(bufPtr, buflen, "SSL - The connection indicated an EOF");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_UNKNOWN_CIPHER) )
        {
            snprintf(bufPtr, buflen, "SSL - An unknown cipher was received");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_NO_CIPHER_CHOSEN) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - The server has no ciphersuites in common with the client");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_NO_RNG) )
        {
            snprintf(bufPtr, buflen, "SSL - No RNG was provided to the SSL module");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - No client certification received from the client");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_CERTIFICATE_TOO_LARGE) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Our own certificate(s) is/are too large to send in an SSL message");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_CERTIFICATE_REQUIRED) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - The own certificate is not set, but needed by the server");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - The own private key or pre-shared key is not set, but needed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED) )
        {
            snprintf(bufPtr, buflen, "SSL - No CA Chain is set, but required to operate");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE) )
        {
            snprintf(bufPtr, buflen, "SSL - An unexpected message was received from our peer");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE) )
        {
            snprintf(bufPtr, buflen, "SSL - A fatal alert message was received from our peer");
            return;
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_PEER_VERIFY_FAILED) )
        {
            snprintf(bufPtr, buflen, "SSL - Verification of our peer failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - The peer notified us that the connection is going to be closed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_CLIENT_HELLO) )
        {
            snprintf(bufPtr, buflen, "SSL - Processing of the ClientHello handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_SERVER_HELLO) )
        {
            snprintf(bufPtr, buflen, "SSL - Processing of the ServerHello handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE) )
        {
            snprintf(bufPtr, buflen, "SSL - Processing of the Certificate handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE_REQUEST) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Processing of the CertificateRequest handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Processing of the ServerKeyExchange handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_SERVER_HELLO_DONE) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Processing of the ServerHelloDone handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Processing of the ClientKeyExchange handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_RP) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Processing of the ClientKeyExchange handshake message failed in DHM / ECDH Read Public");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_CS) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Processing of the ClientKeyExchange handshake message failed in DHM / ECDH Calculate Secret");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Processing of the CertificateVerify handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_CHANGE_CIPHER_SPEC) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Processing of the ChangeCipherSpec handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_FINISHED) )
        {
            snprintf(bufPtr, buflen, "SSL - Processing of the Finished handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_ALLOC_FAILED) )
        {
            snprintf(bufPtr, buflen, "SSL - Memory allocation failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_HW_ACCEL_FAILED) )
        {
            snprintf(bufPtr, buflen, "SSL - Hardware acceleration function returned with error");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_HW_ACCEL_FALLTHROUGH) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Hardware acceleration function skipped / left alone data");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_COMPRESSION_FAILED) )
        {
            snprintf(bufPtr, buflen, "SSL - Processing of the compression / decompression failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_PROTOCOL_VERSION) )
        {
            snprintf(bufPtr, buflen, "SSL - Handshake protocol not within min/max boundaries");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BAD_HS_NEW_SESSION_TICKET) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Processing of the NewSessionTicket handshake message failed");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED) )
        {
            snprintf(bufPtr, buflen, "SSL - Session ticket has expired");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Public key type mismatch (eg, asked for RSA key exchange and presented EC key)");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY) )
        {
            snprintf(bufPtr, buflen, "SSL - Unknown identity received (eg, PSK identity)");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_INTERNAL_ERROR) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - Internal error (eg, unexpected failure in lower-level module)");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_COUNTER_WRAPPING) )
        {
            snprintf(bufPtr, buflen, "SSL - A counter would wrap (eg, too many messages exchanged)");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO) )
        {
            snprintf(bufPtr, buflen, "SSL - Unexpected message at ServerHello in renegotiation");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) )
        {
            snprintf(bufPtr, buflen, "SSL - DTLS client must retry for hello verification");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL) )
        {
            snprintf(bufPtr, buflen, "SSL - A buffer is too small to receive or write a message");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_NO_USABLE_CIPHERSUITE) )
        {
            snprintf(bufPtr, buflen,
                      "SSL - None of the common ciphersuites is usable (eg, no suitable certificate, see debug messages)");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_WANT_READ) )
        {
            snprintf(bufPtr, buflen, "SSL - Connection requires a read call");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_WANT_WRITE) )
        {
            snprintf(bufPtr, buflen, "SSL - Connection requires a write call");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_TIMEOUT) )
        {
            snprintf(bufPtr, buflen, "SSL - The operation timed out");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_CLIENT_RECONNECT) )
        {
            snprintf(bufPtr, buflen, "SSL - The client initiated a reconnect from the same port");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_UNEXPECTED_RECORD) )
        {
            snprintf(bufPtr, buflen, "SSL - Record header looks valid but is not expected");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_NON_FATAL) )
        {
            snprintf(bufPtr, buflen, "SSL - The alert message received indicates a non-fatal error");
        }
        if( use_ret == -(MBEDTLS_ERR_SSL_INVALID_VERIFY_HASH) )
        {
            snprintf(bufPtr, buflen, "SSL - Couldn't set the hash for verifying CertificateVerify");
        }
#endif /* MBEDTLS_SSL_TLS_C */

#if defined(MBEDTLS_X509_USE_C) || defined(MBEDTLS_X509_CREATE_C)
        if( use_ret == -(MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE) )
        {
            snprintf(bufPtr, buflen,
                      "X509 - Unavailable feature, e.g. RSA hashing/encryption combination");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_UNKNOWN_OID) )
        {
            snprintf(bufPtr, buflen, "X509 - Requested OID is unknown");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_INVALID_FORMAT) )
        {
            snprintf(bufPtr, buflen,
                      "X509 - The CRT/CRL/CSR format is invalid, e.g. different type expected");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_INVALID_VERSION) )
        {
            snprintf(bufPtr, buflen, "X509 - The CRT/CRL/CSR version element is invalid");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_INVALID_SERIAL) )
        {
            snprintf(bufPtr, buflen, "X509 - The serial tag or value is invalid");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_INVALID_ALG) )
        {
            snprintf(bufPtr, buflen, "X509 - The algorithm tag or value is invalid");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_INVALID_NAME) )
        {
            snprintf(bufPtr, buflen, "X509 - The name tag or value is invalid");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_INVALID_DATE) )
        {
            snprintf(bufPtr, buflen, "X509 - The date tag or value is invalid");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_INVALID_SIGNATURE) )
        {
            snprintf(bufPtr, buflen, "X509 - The signature tag or value invalid");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_INVALID_EXTENSIONS) )
        {
            snprintf(bufPtr, buflen, "X509 - The extension tag or value is invalid");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_UNKNOWN_VERSION) )
        {
            snprintf(bufPtr, buflen, "X509 - CRT/CRL/CSR has an unsupported version number");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG) )
        {
            snprintf(bufPtr, buflen, "X509 - Signature algorithm (oid) is unsupported");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_SIG_MISMATCH) )
        {
            snprintf(bufPtr, buflen, "X509 - Signature algorithms do not match.");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) )
        {
            snprintf(bufPtr, buflen,
                      "X509 - Certificate verification failed, e.g. CRL, CA or signature check failed");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT) )
        {
            snprintf(bufPtr, buflen, "X509 - Format not recognized as DER or PEM");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_BAD_INPUT_DATA) )
        {
            snprintf(bufPtr, buflen, "X509 - Input invalid");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_ALLOC_FAILED) )
        {
            snprintf(bufPtr, buflen, "X509 - Allocation of memory failed");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_FILE_IO_ERROR) )
        {
            snprintf(bufPtr, buflen, "X509 - Read/write of file failed");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_BUFFER_TOO_SMALL) )
        {
            snprintf(bufPtr, buflen, "X509 - Destination buffer is too small");
        }
        if( use_ret == -(MBEDTLS_ERR_X509_FATAL_ERROR) )
        {
            snprintf(bufPtr, buflen,
                      "X509 - A fatal error occured, eg the chain is too long or the vrfy callback failed");
        }
#endif /* MBEDTLS_X509_USE_C || MBEDTLS_X509_CREATE_C */
        // END generated code

        if( strlen( bufPtr ) == 0 )
        {
            snprintf(bufPtr, buflen, "UNKNOWN ERROR CODE (%04X)", use_ret );
        }
    }

    use_ret = ret & ~0xFF80;

    if( use_ret == 0 )
    {
        return;
    }

    // If high level code is present, make a concatenation between both
    // error strings.
    //
    len = strlen( bufPtr );

    if( len > 0 )
    {
        if( buflen - len < 5 )
        {
            return;
        }

        snprintf(bufPtr + len, buflen - len, " : ");

        bufPtr += len + 3;
        buflen -= len + 3;
    }

    // Low level error codes
    //
    // BEGIN generated code
#if defined(MBEDTLS_AES_C)
    if( use_ret == -(MBEDTLS_ERR_AES_INVALID_KEY_LENGTH) )
    {
        snprintf(bufPtr, buflen, "AES - Invalid key length");
    }
    if( use_ret == -(MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH) )
    {
        snprintf(bufPtr, buflen, "AES - Invalid data input length");
    }
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_ASN1_PARSE_C)
    if( use_ret == -(MBEDTLS_ERR_ASN1_OUT_OF_DATA) )
    {
        snprintf(bufPtr, buflen, "ASN1 - Out of data when parsing an ASN1 data structure");
    }
    if( use_ret == -(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG) )
    {
        snprintf(bufPtr, buflen, "ASN1 - ASN1 tag was of an unexpected value");
    }
    if( use_ret == -(MBEDTLS_ERR_ASN1_INVALID_LENGTH) )
    {
        snprintf(bufPtr, buflen,
                  "ASN1 - Error when trying to determine the length or invalid length");
    }
    if( use_ret == -(MBEDTLS_ERR_ASN1_LENGTH_MISMATCH) )
    {
        snprintf(bufPtr, buflen, "ASN1 - Actual length differs from expected length");
    }
    if( use_ret == -(MBEDTLS_ERR_ASN1_INVALID_DATA) )
    {
        snprintf(bufPtr, buflen, "ASN1 - Data is invalid. (not used)");
    }
    if( use_ret == -(MBEDTLS_ERR_ASN1_ALLOC_FAILED) )
    {
        snprintf(bufPtr, buflen, "ASN1 - Memory allocation failed");
    }
    if( use_ret == -(MBEDTLS_ERR_ASN1_BUF_TOO_SMALL) )
    {
        snprintf(bufPtr, buflen, "ASN1 - Buffer too small when writing ASN.1 data structure");
    }
#endif /* MBEDTLS_ASN1_PARSE_C */

#if defined(MBEDTLS_BASE64_C)
    if( use_ret == -(MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) )
    {
        snprintf(bufPtr, buflen, "BASE64 - Output buffer too small");
    }
    if( use_ret == -(MBEDTLS_ERR_BASE64_INVALID_CHARACTER) )
    {
        snprintf(bufPtr, buflen, "BASE64 - Invalid character in input");
    }
#endif /* MBEDTLS_BASE64_C */

#if defined(MBEDTLS_BIGNUM_C)
    if( use_ret == -(MBEDTLS_ERR_MPI_FILE_IO_ERROR) )
    {
        snprintf(bufPtr, buflen,
                  "BIGNUM - An error occurred while reading from or writing to a file");
    }
    if( use_ret == -(MBEDTLS_ERR_MPI_BAD_INPUT_DATA) )
    {
        snprintf(bufPtr, buflen, "BIGNUM - Bad input parameters to function");
    }
    if( use_ret == -(MBEDTLS_ERR_MPI_INVALID_CHARACTER) )
    {
        snprintf(bufPtr, buflen, "BIGNUM - There is an invalid character in the digit string");
    }
    if( use_ret == -(MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL) )
    {
        snprintf(bufPtr, buflen, "BIGNUM - The buffer is too small to write to");
    }
    if( use_ret == -(MBEDTLS_ERR_MPI_NEGATIVE_VALUE) )
    {
        snprintf(bufPtr, buflen,
                  "BIGNUM - The input arguments are negative or result in illegal output");
    }
    if( use_ret == -(MBEDTLS_ERR_MPI_DIVISION_BY_ZERO) )
    {
        snprintf(bufPtr, buflen,
                  "BIGNUM - The input argument for division is zero, which is not allowed");
    }
    if( use_ret == -(MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) )
    {
        snprintf(bufPtr, buflen, "BIGNUM - The input arguments are not acceptable");
    }
    if( use_ret == -(MBEDTLS_ERR_MPI_ALLOC_FAILED) )
    {
        snprintf(bufPtr, buflen, "BIGNUM - Memory allocation failed");
    }
#endif /* MBEDTLS_BIGNUM_C */

#if defined(MBEDTLS_BLOWFISH_C)
    if( use_ret == -(MBEDTLS_ERR_BLOWFISH_INVALID_KEY_LENGTH) )
    {
        snprintf(bufPtr, buflen, "BLOWFISH - Invalid key length");
    }
    if( use_ret == -(MBEDTLS_ERR_BLOWFISH_INVALID_INPUT_LENGTH) )
    {
        snprintf(bufPtr, buflen, "BLOWFISH - Invalid data input length");
    }
#endif /* MBEDTLS_BLOWFISH_C */

#if defined(MBEDTLS_CAMELLIA_C)
    if( use_ret == -(MBEDTLS_ERR_CAMELLIA_INVALID_KEY_LENGTH) )
    {
        snprintf(bufPtr, buflen, "CAMELLIA - Invalid key length");
    }
    if( use_ret == -(MBEDTLS_ERR_CAMELLIA_INVALID_INPUT_LENGTH) )
    {
        snprintf(bufPtr, buflen, "CAMELLIA - Invalid data input length");
    }
#endif /* MBEDTLS_CAMELLIA_C */

#if defined(MBEDTLS_CCM_C)
    if( use_ret == -(MBEDTLS_ERR_CCM_BAD_INPUT) )
    {
        snprintf(bufPtr, buflen, "CCM - Bad input parameters to function");
    }
    if( use_ret == -(MBEDTLS_ERR_CCM_AUTH_FAILED) )
    {
        snprintf(bufPtr, buflen, "CCM - Authenticated decryption failed");
    }
#endif /* MBEDTLS_CCM_C */

#if defined(MBEDTLS_CTR_DRBG_C)
    if( use_ret == -(MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED) )
    {
        snprintf(bufPtr, buflen, "CTR_DRBG - The entropy source failed");
    }
    if( use_ret == -(MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG) )
    {
        snprintf(bufPtr, buflen, "CTR_DRBG - Too many random requested in single call");
    }
    if( use_ret == -(MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG) )
    {
        snprintf(bufPtr, buflen, "CTR_DRBG - Input too large (Entropy + additional)");
    }
    if( use_ret == -(MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR) )
    {
        snprintf(bufPtr, buflen, "CTR_DRBG - Read/write error in file");
    }
#endif /* MBEDTLS_CTR_DRBG_C */

#if defined(MBEDTLS_DES_C)
    if( use_ret == -(MBEDTLS_ERR_DES_INVALID_INPUT_LENGTH) )
    {
        snprintf(bufPtr, buflen, "DES - The data input has an invalid length");
    }
#endif /* MBEDTLS_DES_C */

#if defined(MBEDTLS_ENTROPY_C)
    if( use_ret == -(MBEDTLS_ERR_ENTROPY_SOURCE_FAILED) )
    {
        snprintf(bufPtr, buflen, "ENTROPY - Critical entropy source failure");
    }
    if( use_ret == -(MBEDTLS_ERR_ENTROPY_MAX_SOURCES) )
    {
        snprintf(bufPtr, buflen, "ENTROPY - No more sources can be added");
    }
    if( use_ret == -(MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED) )
    {
        snprintf(bufPtr, buflen, "ENTROPY - No sources have been added to poll");
    }
    if( use_ret == -(MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE) )
    {
        snprintf(bufPtr, buflen, "ENTROPY - No strong sources have been added to poll");
    }
    if( use_ret == -(MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR) )
    {
        snprintf(bufPtr, buflen, "ENTROPY - Read/write error in file");
    }
#endif /* MBEDTLS_ENTROPY_C */

#if defined(MBEDTLS_GCM_C)
    if( use_ret == -(MBEDTLS_ERR_GCM_AUTH_FAILED) )
    {
        snprintf(bufPtr, buflen, "GCM - Authenticated decryption failed");
    }
    if( use_ret == -(MBEDTLS_ERR_GCM_BAD_INPUT) )
    {
        snprintf(bufPtr, buflen, "GCM - Bad input parameters to function");
    }
#endif /* MBEDTLS_GCM_C */

#if defined(MBEDTLS_HMAC_DRBG_C)
    if( use_ret == -(MBEDTLS_ERR_HMAC_DRBG_REQUEST_TOO_BIG) )
    {
        snprintf(bufPtr, buflen, "HMAC_DRBG - Too many random requested in single call");
    }
    if( use_ret == -(MBEDTLS_ERR_HMAC_DRBG_INPUT_TOO_BIG) )
    {
        snprintf(bufPtr, buflen, "HMAC_DRBG - Input too large (Entropy + additional)");
    }
    if( use_ret == -(MBEDTLS_ERR_HMAC_DRBG_FILE_IO_ERROR) )
    {
        snprintf(bufPtr, buflen, "HMAC_DRBG - Read/write error in file");
    }
    if( use_ret == -(MBEDTLS_ERR_HMAC_DRBG_ENTROPY_SOURCE_FAILED) )
    {
        snprintf(bufPtr, buflen, "HMAC_DRBG - The entropy source failed");
    }
#endif /* MBEDTLS_HMAC_DRBG_C */

#if defined(MBEDTLS_NET_C)
    if( use_ret == -(MBEDTLS_ERR_NET_SOCKET_FAILED) )
    {
        snprintf(bufPtr, buflen, "NET - Failed to open a socket");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_CONNECT_FAILED) )
    {
        snprintf(bufPtr, buflen, "NET - The connection to the given server / port failed");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_BIND_FAILED) )
    {
        snprintf(bufPtr, buflen, "NET - Binding of the socket failed");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_LISTEN_FAILED) )
    {
        snprintf(bufPtr, buflen, "NET - Could not listen on the socket");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_ACCEPT_FAILED) )
    {
        snprintf(bufPtr, buflen, "NET - Could not accept the incoming connection");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_RECV_FAILED) )
    {
        snprintf(bufPtr, buflen, "NET - Reading information from the socket failed");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_SEND_FAILED) )
    {
        snprintf(bufPtr, buflen, "NET - Sending information through the socket failed");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_CONN_RESET) )
    {
        snprintf(bufPtr, buflen, "NET - Connection was reset by peer");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_UNKNOWN_HOST) )
    {
        snprintf(bufPtr, buflen, "NET - Failed to get an IP address for the given hostname");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_BUFFER_TOO_SMALL) )
    {
        snprintf(bufPtr, buflen, "NET - Buffer is too small to hold the data");
    }
    if( use_ret == -(MBEDTLS_ERR_NET_INVALID_CONTEXT) )
    {
        snprintf(bufPtr, buflen, "NET - The context is invalid, eg because it was free()ed");
    }
#endif /* MBEDTLS_NET_C */

#if defined(MBEDTLS_OID_C)
    if( use_ret == -(MBEDTLS_ERR_OID_NOT_FOUND) )
    {
        snprintf(bufPtr, buflen, "OID - OID is not found");
    }
    if( use_ret == -(MBEDTLS_ERR_OID_BUF_TOO_SMALL) )
    {
        snprintf(bufPtr, buflen, "OID - output buffer is too small");
    }
#endif /* MBEDTLS_OID_C */

#if defined(MBEDTLS_PADLOCK_C)
    if( use_ret == -(MBEDTLS_ERR_PADLOCK_DATA_MISALIGNED) )
    {
        snprintf(bufPtr, buflen, "PADLOCK - Input data should be aligned");
    }
#endif /* MBEDTLS_PADLOCK_C */

#if defined(MBEDTLS_THREADING_C)
    if( use_ret == -(MBEDTLS_ERR_THREADING_FEATURE_UNAVAILABLE) )
    {
        snprintf(bufPtr, buflen, "THREADING - The selected feature is not available");
    }
    if( use_ret == -(MBEDTLS_ERR_THREADING_BAD_INPUT_DATA) )
    {
        snprintf(bufPtr, buflen, "THREADING - Bad input parameters to function");
    }
    if( use_ret == -(MBEDTLS_ERR_THREADING_MUTEX_ERROR) )
    {
        snprintf(bufPtr, buflen, "THREADING - Locking / unlocking / free failed with error code");
    }
#endif /* MBEDTLS_THREADING_C */

#if defined(MBEDTLS_XTEA_C)
    if( use_ret == -(MBEDTLS_ERR_XTEA_INVALID_INPUT_LENGTH) )
    {
        snprintf(bufPtr, buflen, "XTEA - The data input has an invalid length");
    }
#endif /* MBEDTLS_XTEA_C */
    // END generated code

    if( strlen( bufPtr ) != 0 )
    {
        return;
    }

    snprintf(bufPtr, buflen, "UNKNOWN ERROR CODE (%04X)", use_ret );
}

int mbedtls_x509_crt_parse
(
    mbedtls_x509_crt *chain,
    const unsigned char *buf,
    size_t buflen
)
{
    (void)chain;
    (void)buf;
    (void)buflen;
    return 0;
}


void mbedtls_ssl_conf_ca_chain
(
    mbedtls_ssl_config *conf,
    mbedtls_x509_crt *ca_chain,
    mbedtls_x509_crl *ca_crl
)
{
    (void)conf;
    (void)ca_chain;
    (void)ca_crl;
}

int mbedtls_ssl_set_hostname
(
    mbedtls_ssl_context *ssl,
    const char *hostname
)
{
    (void)ssl;
    (void)hostname;
    return 0;
}

void mbedtls_x509_crt_free
(
    mbedtls_x509_crt *crt
)
{
    (void)crt;
}

int mbedtls_ctr_drbg_random
(
    void *p_rng,
    unsigned char *output,
    size_t output_len
)
{
    (void)p_rng;
    (void)output;
    (void)output_len;
    return 0;
}

int mbedtls_net_send
(
    void *ctx,
    const unsigned char *buf,
    size_t len
)
{
    (void)ctx;
    (void)buf;
    (void)len;
    return 10;
}

int mbedtls_net_recv_timeout(
    void *ctx,
    unsigned char *buf,
    size_t len,
    uint32_t timeout
)
{
    (void)ctx;
    (void)buf;
    (void)len;
    (void)timeout;
    return 10;
}

int mbedtls_entropy_func
(
    void *data,
    unsigned char *output,
    size_t len
)
{
    (void)data;
    (void)output;
    (void)len;
    return 0;
}

void mbedtls_net_init
(
    mbedtls_net_context *ctx
)
{
    (void)ctx;
}

void mbedtls_ssl_init
(
    mbedtls_ssl_context *ssl
)
{
    (void)ssl;
}

void mbedtls_ssl_config_init
(
    mbedtls_ssl_config *conf
)
{
    (void)conf;
}

void mbedtls_ctr_drbg_init
(
    mbedtls_ctr_drbg_context *ctx
)
{
    (void)ctx;
}

void mbedtls_entropy_init
(
    mbedtls_entropy_context *ctx
)
{
    (void)ctx;
}

int mbedtls_ctr_drbg_seed
(
    mbedtls_ctr_drbg_context *ctx,
    int (*f_entropy)(void *, unsigned char *, size_t),
    void *p_entropy,
    const unsigned char *custom,
    size_t len
)
{
    (void)ctx;
    (void)p_entropy;
    (void)custom;
    (void)len;
    return 0;
}

int mbedtls_ssl_config_defaults
(
    mbedtls_ssl_config *conf,
    int endpoint,
    int transport,
    int preset
)
{
    (void)conf;
    (void)endpoint;
    (void)transport;
    (void)preset;
    return 0;
}

void mbedtls_ssl_conf_rng
(
    mbedtls_ssl_config *conf,
    int (*f_rng)(void *,unsigned char *, size_t),
    void *p_rng
)
{
    (void)conf;
    (void)p_rng;
}

void mbedtls_net_free
(
    mbedtls_net_context *ctx
)
{
    (void)ctx;
}

void mbedtls_ssl_free
(
    mbedtls_ssl_context *ssl
)
{
    (void)ssl;
}

void mbedtls_ssl_config_free
(
    mbedtls_ssl_config *conf
)
{
    (void)conf;
}

void mbedtls_ctr_drbg_free
(
    mbedtls_ctr_drbg_context *ctx
)
{
    (void)ctx;
}

void mbedtls_entropy_free
(
    mbedtls_entropy_context *ctx
)
{
    (void)ctx;
}

/**
 * Errors to be tested:
 * MBEDTLS_ERR_NET_CONNECT_FAILED
 * MBEDTLS_ERR_NET_UNKNOWN_HOST
 **/
int mbedtls_net_connect
(
    mbedtls_net_context *ctx,
    const char *host,
    const char *port,
    int proto
)
{
    (void)ctx;
    (void)host;
    (void)port;
    (void)proto;
    return 0;
}


void mbedtls_ssl_conf_authmode
(
    mbedtls_ssl_config *conf,
    int authmode
)
{
    (void)conf;
    (void)authmode;
}

int mbedtls_ssl_setup
(
    mbedtls_ssl_context *ssl,
    const mbedtls_ssl_config *conf
)
{
    (void)ssl;
    (void)conf;
    return 0;
}

void mbedtls_ssl_set_bio
(
    mbedtls_ssl_context *ssl,
    void *p_bio,
    mbedtls_ssl_send_t *f_send,
    mbedtls_ssl_recv_t *f_recv,
    mbedtls_ssl_recv_timeout_t *f_recv_timeout
)
{
    (void)ssl;
    (void)p_bio;
}

void mbedtls_ssl_conf_read_timeout
(
    mbedtls_ssl_config *conf,
    uint32_t timeout
)
{
    (void)conf;
    (void)timeout;
}

/**
 * Errors to be tested:
 * MBEDTLS_ERR_SSL_CONN_EOF
 * MBEDTLS_ERR_SSL_TIMEOUT
 * MBEDTLS_ERR_NET_RECV_FAILED
 **/
int mbedtls_ssl_handshake
(
    mbedtls_ssl_context *ssl
)
{
    (void)ssl;
    return 0;
}

int mbedtls_ssl_write( mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len )
{
    (void)ssl;
    (void)buf;
    (void)len;
    return (int)len;
}

int mbedtls_ssl_read( mbedtls_ssl_context *ssl, unsigned char *buf, size_t len )
{
    (void)ssl;
    (void)buf;
    (void)len;
    return (int)len;
}

int mbedtls_platform_set_calloc_free( void * (*calloc_func)( size_t, size_t ), void (*free_func)( void * ) )
{
    return 0;
}
