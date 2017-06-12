/**
 * @file sslUtilities.h
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef _SSLUTILITIES_H
#define _SSLUTILITIES_H

#include <stddef.h>

//--------------------------------------------------------------------------------------------------
/**
 * Certificate max length
 */
//--------------------------------------------------------------------------------------------------
#define MAX_CERT_LEN    8192

//--------------------------------------------------------------------------------------------------
/**
 * Lay out a base64 string into PEM
 *
 * @note:
 *     - Make sure that strPtr is big enough to hold the new string.
 *     - The buffer size must be >=
 *          ( (strLen/64)+strlen(PEM_CERT_HEADER)+strlen(PEM_CERT_FOOTER)+4 )
 */
//--------------------------------------------------------------------------------------------------
int ssl_LayOutPEM
(
    char*   strPtr,
    int     strLen
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if SSL certificate exists and load it
 */
//--------------------------------------------------------------------------------------------------
le_result_t ssl_CheckCertificate
(
    void
);

#endif /* _SSLUTILITIES_H */
