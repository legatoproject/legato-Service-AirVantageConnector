/**
 * @file avcFsConfig.h
 *
 * Header for file system configuration.
 * This file contains directory and file pathes and any other filesystem config
 * related definitions
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#ifndef _AVCFSCONFIG_H
#define _AVCFSCONFIG_H

//--------------------------------------------------------------------------------------------------
/**
 * Package downloader Legato filesystem directory path
 */
//--------------------------------------------------------------------------------------------------
#define PKGDWL_LEFS_DIR     "/avc"


//--------------------------------------------------------------------------------------------------
/**
 * DER certificate path
 */
//--------------------------------------------------------------------------------------------------
#define DERCERT_PATH        PKGDWL_LEFS_DIR "/" "cert.der"


#endif /* _AVCFSCONFIG_H */
