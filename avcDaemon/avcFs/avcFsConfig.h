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
#define PKGDWL_LEFS_DIR                     "/avc"

//--------------------------------------------------------------------------------------------------
/**
 * AVC configuration path
 */
//--------------------------------------------------------------------------------------------------
#define AVC_CONFIG_PATH                     PKGDWL_LEFS_DIR "/" "config"

//--------------------------------------------------------------------------------------------------
/**
 * SSL certificate path
 */
//--------------------------------------------------------------------------------------------------
#define SSLCERT_PATH                        PKGDWL_LEFS_DIR "/" "cert"

//--------------------------------------------------------------------------------------------------
/**
 * Firmware update information directory
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_INFO_DIR                  PKGDWL_LEFS_DIR "/" "fw"

//--------------------------------------------------------------------------------------------------
/**
 * Software update information directory
 */
//--------------------------------------------------------------------------------------------------
#define SW_UPDATE_INFO_DIR                  PKGDWL_LEFS_DIR "/" "sw"

//--------------------------------------------------------------------------------------------------
/**
 * Firmware update state path
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_STATE_PATH                FW_UPDATE_INFO_DIR "/" "updateState"

//--------------------------------------------------------------------------------------------------
/**
 * Firmware update result path
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_RESULT_PATH               FW_UPDATE_INFO_DIR "/" "updateResult"

//--------------------------------------------------------------------------------------------------
/**
 * Firmware update notification path: connection is requested to the server
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_NOTIFICATION_PATH         FW_UPDATE_INFO_DIR "/" "updateNotification"

//--------------------------------------------------------------------------------------------------
/**
 * Firmware update intall pending path
 */
//--------------------------------------------------------------------------------------------------
#define FW_UPDATE_INSTALL_PENDING_PATH      FW_UPDATE_INFO_DIR "/" "isInstallPending"

//--------------------------------------------------------------------------------------------------
/**
 * Connection pending path
 */
//--------------------------------------------------------------------------------------------------
#define CONNECTION_PENDING_PATH      FW_UPDATE_INFO_DIR "/" "isConnectionPending"

//--------------------------------------------------------------------------------------------------
/**
 * Software update state path
 */
//--------------------------------------------------------------------------------------------------
#define SW_UPDATE_STATE_PATH                SW_UPDATE_INFO_DIR "/" "updateState"

//--------------------------------------------------------------------------------------------------
/**
 * Software update instance path
 */
//--------------------------------------------------------------------------------------------------
#define SW_UPDATE_INSTANCE_PATH             SW_UPDATE_INFO_DIR "/" "instanceId"

//--------------------------------------------------------------------------------------------------
/**
 * Software update bytes downloaded path
 */
//--------------------------------------------------------------------------------------------------
#define SW_UPDATE_BYTES_DOWNLOADED_PATH     SW_UPDATE_INFO_DIR "/" "bytesDownloaded"

//--------------------------------------------------------------------------------------------------
/**
 * Software update internal state path
 */
//--------------------------------------------------------------------------------------------------
#define SW_UPDATE_INTERNAL_STATE_PATH       SW_UPDATE_INFO_DIR "/" "internalState"

//--------------------------------------------------------------------------------------------------
/**
 * Software update result path
 */
//--------------------------------------------------------------------------------------------------
#define SW_UPDATE_RESULT_PATH               SW_UPDATE_INFO_DIR "/" "updateResult"

//--------------------------------------------------------------------------------------------------
/**
 * Package downloader update information directory
 */
//--------------------------------------------------------------------------------------------------
#define UPDATE_INFO_DIR                     PKGDWL_LEFS_DIR "/" "packageDownloader"

#ifndef LE_CONFIG_CUSTOM_OS
//--------------------------------------------------------------------------------------------------
/**
 * Package URI path
 */
//--------------------------------------------------------------------------------------------------
#define PACKAGE_URI_FILENAME                UPDATE_INFO_DIR "/" "packageUri"

//--------------------------------------------------------------------------------------------------
/**
 * Package size filename
 */
//--------------------------------------------------------------------------------------------------
#define PACKAGE_SIZE_FILENAME               UPDATE_INFO_DIR "/" "pkgSize"

//--------------------------------------------------------------------------------------------------
/**
 * Package update type path
 */
//--------------------------------------------------------------------------------------------------
#define UPDATE_TYPE_FILENAME                UPDATE_INFO_DIR "/" "updateType"
#endif /* !LE_CONFIG_CUSTOM_OS */

//--------------------------------------------------------------------------------------------------
/**
 *  Name of the avc configuration file
 */
//--------------------------------------------------------------------------------------------------
#define AVC_CONFIG_PARAM        "avcConfigParam"

//--------------------------------------------------------------------------------------------------
/**
 *  Name of the file transfer configuration file
 */
//--------------------------------------------------------------------------------------------------
#define FILE_TRANSFER_CONFIG_PARAM        "FileTransferConfigParam"

//--------------------------------------------------------------------------------------------------
/**
 * Package download temporary directory
 */
//--------------------------------------------------------------------------------------------------
#define PKGDWL_TMP_PATH                     "/tmp/pkgdwl"

//--------------------------------------------------------------------------------------------------
/**
 * Fifo file path
 */
//--------------------------------------------------------------------------------------------------
#define FIFO_PATH                           PKGDWL_TMP_PATH "/" "fifo"

//--------------------------------------------------------------------------------------------------
/**
 * PEM certificate file path
 */
//--------------------------------------------------------------------------------------------------
#define PEMCERT_PATH                        PKGDWL_TMP_PATH "/" "cert.pem"

//--------------------------------------------------------------------------------------------------
/**
 * Path to root certificates
 */
//--------------------------------------------------------------------------------------------------
#define ROOTCERT_PATH                       "/etc/ssl/certs"

#endif /* _AVCFSCONFIG_H */
