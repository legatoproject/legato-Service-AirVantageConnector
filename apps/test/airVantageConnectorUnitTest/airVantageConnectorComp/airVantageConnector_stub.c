/**
 * This module implements some stubs for airVantage Connector unit tests.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "lwm2mcorePackageDownloader.h"


//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Iterator reference for simulated config tree
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// airVantage connector stubbing
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionRef_t le_avc_GetClientSessionRef
(
    void
)
{
    return (le_msg_SessionRef_t) 0x1001;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the server service reference
 */
//--------------------------------------------------------------------------------------------------
le_msg_ServiceRef_t le_avc_GetServiceRef
(
    void
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Registers a function to be called whenever one of this service's sessions is closed by
 * the client.  (STUBBED FUNCTION)
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionEventHandlerRef_t MyAddServiceCloseHandler
(
    le_msg_ServiceRef_t             serviceRef, ///< [in] Reference to the service.
    le_msg_SessionEventHandler_t    handlerFunc,///< [in] Handler function.
    void*                           contextPtr  ///< [in] Opaque pointer value to pass to handler.
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Setup temporary files
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_Init
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Init this sub-component
 */
//--------------------------------------------------------------------------------------------------
le_result_t assetData_Init
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the avData module
 */
//--------------------------------------------------------------------------------------------------
void avData_Init
(
    void
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize time series
 */
//--------------------------------------------------------------------------------------------------
le_result_t timeSeries_Init
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Init push subcomponent
 */
//--------------------------------------------------------------------------------------------------
le_result_t push_Init
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retry pushing items queued in the list after AV connection reset
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_NOT_FOUND      If nothing to be retried
 *  - LE_FAULT          On any other errors
 */
//--------------------------------------------------------------------------------------------------
le_result_t push_Retry
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialization function avcApp. Should be called only once.
 */
//--------------------------------------------------------------------------------------------------
void avcApp_Init
(
   void
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start watchdogs 0..N-1.  Typically this is used in COMPONENT_INIT to start all watchdogs needed
 * by the process.
 */
//--------------------------------------------------------------------------------------------------
void le_wdogChain_Init
(
    uint32_t wdogCount
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Begin monitoring the event loop on the current thread.
 */
//--------------------------------------------------------------------------------------------------
void le_wdogChain_MonitorEventLoop
(
    uint32_t watchdog,          ///< Watchdog to use for monitoring
    le_clk_Time_t watchdogInterval ///< Interval at which to check event loop is functioning
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update install pending status
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateInstallPending
(
    bool* isFwInstallPendingPtr                  ///< [OUT] Is FW install pending?
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Received user agreement; proceed to download package.
 *
 * @note This function is accessed from the avc thread and shares 'PkgDwlObj' with the package
 * downloader thread. While the package downloader is waiting for user agreement, the avc thread
 * merely changes the state of the package downloader and allows the package downloader to proceed.
 * Care must be taken to protect 'PkgDwlObj' while modifying this function.
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_PackageDownloaderAcceptDownload
(
    void
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update state
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateState
(
    lwm2mcore_FwUpdateState_t* fwUpdateStatePtr     ///< [INOUT] FW update state
)
{
     return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update result
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateResult
(
    lwm2mcore_FwUpdateResult_t* fwUpdateResultPtr   ///< [INOUT] FW update result
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update state
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetSwUpdateState
(
    lwm2mcore_SwUpdateState_t* swUpdateStatePtr     ///< [INOUT] SW update state
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update result
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetSwUpdateResult
(
    lwm2mcore_SwUpdateResult_t* swUpdateResultPtr   ///< [INOUT] SW update result
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve package information necessary to resume a download (URI and package type)
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetResumeInfo
(
    char* uriPtr,                       ///< [INOUT] package URI
    size_t* uriLenPtr,                  ///< [INOUT] package URI length
    lwm2mcore_UpdateType_t* typePtr     ///< [INOUT] Update type
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the number of bytes to download on resume. Function will give valid data if suspend
 * state was LE_AVC_DOWNLOAD_PENDING, LE_DOWNLOAD_IN_PROGRESS or LE_DOWNLOAD_COMPLETE.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_BytesLeftToDownload
(
    uint64_t *numBytes          ///< [OUT] Number of bytes to download on resume. Will give valid
                                ///<       data if suspend state was LE_AVC_DOWNLOAD_PENDING,
                                ///<       LE_DOWNLOAD_IN_PROGRESS or LE_DOWNLOAD_COMPLETE.
                                ///<       Otherwise undefined.
)
{
    *numBytes = 0;
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Download and store a package
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_StartDownload
(
    const char*            uriPtr,  ///< Package URI
    lwm2mcore_UpdateType_t type,    ///< Update type (FW/SW)
    bool                   resume   ///< Indicates if it is a download resume
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software update bytes downloaded to workspace
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_SetSwUpdateBytesDownloaded
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * End download
 */
//--------------------------------------------------------------------------------------------------
void avcApp_EndDownload
(
    void
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if there is a software update related notification to send after a reboot
 * or a service restart.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_CheckNotificationToSend
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Called by avcServer when the session started or stopped.
 */
//--------------------------------------------------------------------------------------------------
void avData_ReportSessionState
(
    le_avdata_SessionState_t sessionState
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete FW update related info.
 */
//--------------------------------------------------------------------------------------------------
void packageDownloader_DeleteFwUpdateInfo
(
    void
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update internal state
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetSwUpdateInternalState
(
    avcApp_InternalState_t* internalStatePtr     ///< [OUT] internal state
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete package information necessary to resume a download (URI and package type)
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_DeleteResumeInfo
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get package download HTTP response code
 *
 */
//--------------------------------------------------------------------------------------------------
uint16_t pkgDwlCb_GetHttpStatus
(
    void
)
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update notification
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateNotification
(
    bool* isNotificationRequestPtr              ///< [IN] is a FOTA result needed to be sent to the
                                                ///< server ?
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set firmware update result
 *
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetFwUpdateResult
(
    lwm2mcore_FwUpdateResult_t fwUpdateResult   ///< [IN] New FW update result
)
{
    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set firmware update state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetFwUpdateState
(
    lwm2mcore_FwUpdateState_t fwUpdateState     ///< [IN] New FW update state
)
{
    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software update result
 *
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetSwUpdateResult
(
    lwm2mcore_SwUpdateResult_t swUpdateResult   ///< [IN] New SW update result
)
{
    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software update state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t packageDownloader_SetSwUpdateState
(
    lwm2mcore_SwUpdateState_t swUpdateState     ///< [IN] New SW update state
)
{
    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set firmware update notification.
 * This is used to indicate if the FOTA result needs to be notified to the application and sent to
 * the server after an install.
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_SetFwUpdateNotification
(
    bool                notifRequested,     ///< [IN] Indicates if a notification is requested
    le_avc_Status_t     updateStatus,       ///< [IN] Update status
    le_avc_ErrorCode_t  errorCode           ///< [IN] Error code
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Send a list of object 9 instances currently managed by legato to lwm2mcore
  */
//--------------------------------------------------------------------------------------------------
void avcApp_NotifyObj9List
(
    void
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete SOTA workspace
 */
//--------------------------------------------------------------------------------------------------
void avcApp_DeletePackage
(
    void
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
// Config Tree service stubbing
//--------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------
/**
 *  le_cfg_CreateReadTxn() stub.
 *
 */
// -------------------------------------------------------------------------------------------------
le_cfg_IteratorRef_t le_cfg_CreateReadTxn
(
    const char* basePath
        ///< [IN]
        ///< Path to the location to create the new iterator.
)
{
    return NULL;
}

// -------------------------------------------------------------------------------------------------
/**
 *  le_cfg_CreateWriteTxn() stub.
 *
 */
// -------------------------------------------------------------------------------------------------
le_cfg_IteratorRef_t le_cfg_CreateWriteTxn
(
    const char *    basePath
        ///< [IN]
        ///< Path to the location to create the new iterator.
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_CommitTxn() stub.
 *
 */
//--------------------------------------------------------------------------------------------------
void le_cfg_CommitTxn
(
    le_cfg_IteratorRef_t iteratorRef
        ///< [IN]
        ///< Iterator object to commit.
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_CancelTxn() stub.
 *
 */
//--------------------------------------------------------------------------------------------------
void le_cfg_CancelTxn
(
    le_cfg_IteratorRef_t iteratorRef
        ///< [IN]
        ///< Iterator object to close.
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_NodeExists() stub.
 *
 */
//--------------------------------------------------------------------------------------------------
bool le_cfg_NodeExists
(
    le_cfg_IteratorRef_t iteratorRef,
        ///< [IN]
        ///< Iterator to use as a basis for the transaction.

    const char* path
        ///< [IN]
        ///< Path to the target node. Can be an absolute path, or
        ///< a path relative from the iterator's current position.
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_GetString() stub.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_cfg_GetString
(
    le_cfg_IteratorRef_t iteratorRef,
        ///< [IN]
        ///< Iterator to use as a basis for the transaction.

    const char* path,
        ///< [IN]
        ///< Path to the target node. Can be an absolute path,
        ///< or a path relative from the iterator's current
        ///< position.

    char* value,
        ///< [OUT]
        ///< Buffer to write the value into.

    size_t valueNumElements,
        ///< [IN]

    const char* defaultValue
        ///< [IN]
        ///< Default value to use if the original can't be
        ///<   read.
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_GetInt() stub.
 *
 */
//--------------------------------------------------------------------------------------------------
int32_t le_cfg_GetInt
(
    le_cfg_IteratorRef_t iteratorRef,
        ///< [IN]
        ///< Iterator to use as a basis for the transaction.

    const char* path,
        ///< [IN]
        ///< Path to the target node. Can be an absolute path, or
        ///< a path relative from the iterator's current position.

    int32_t defaultValue
        ///< [IN]
        ///< Default value to use if the original can't be
        ///<   read.
)
{
    return 1;
}

// -------------------------------------------------------------------------------------------------
/**
 *  le_cfg_SetInt() stub.
 */
// -------------------------------------------------------------------------------------------------
void le_cfg_SetInt
(
    le_cfg_IteratorRef_t iteratorRef,
        ///< [IN]
        ///< Iterator to use as a basis for the transaction.

    const char* path,
        ///< [IN]
        ///< Full or relative path to the value to write.

    int32_t value
        ///< [IN]
        ///< Value to write.
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_cfg_GetBool() stub.
 *
 */
//--------------------------------------------------------------------------------------------------
bool le_cfg_GetBool
(
    le_cfg_IteratorRef_t iteratorRef,   ///< [IN] Iterator to use as a basis for the transaction.
    const char* path,                   ///< [IN] Path to the target node. Can be an absolute path,
                                        ///<      or a path relative from the iterator's current
                                        ///<      position
    bool defaultValue                   ///< [IN] Default value to use if the original can't be
                                        ///<      read.
)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to retrieve the International Mobile Equipment Identity (IMEI).
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_info_GetImei
(
    char*            imeiPtr,  ///< [OUT] The IMEI string.
    size_t           len       ///< [IN] The length of IMEI string.
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the Radio Module power state.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_mrc_GetRadioPower
(
    le_onoff_t*    powerPtr   ///< [OUT] The power state.
)
{
    *powerPtr = LE_ON;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *
 * le_data_ConnectService() stubbed.
 *
 */
//--------------------------------------------------------------------------------------------------
void le_data_ConnectService
(
    void
)
{
   return;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function adds a handler...
 */
//--------------------------------------------------------------------------------------------------
le_data_ConnectionStateHandlerRef_t le_data_AddConnectionStateHandler
(
    le_data_ConnectionStateHandlerFunc_t handlerPtr,    ///< [IN] Handler pointer
    void* contextPtr                                    ///< [IN] Associated context pointer
)
{
    return (le_data_ConnectionStateHandlerRef_t)0x100A;
}

//--------------------------------------------------------------------------------------------------
/**
 * Request the default data connection
 *
 */
//--------------------------------------------------------------------------------------------------
le_data_RequestObjRef_t le_data_Request
(
    void
)
{
    return (le_data_RequestObjRef_t)0x1000A;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the date from the configured server using the configured time protocol.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_data_GetDate
(
    uint16_t* yearPtr,      ///< [OUT] UTC Year A.D. [e.g. 2017].
    uint16_t* monthPtr,     ///< [OUT] UTC Month into the year [range 1...12].
    uint16_t* dayPtr        ///< [OUT] UTC Days into the month [range 1...31].
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the time from the configured server using the configured time protocol.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_data_GetTime
(
    uint16_t* hoursPtr,         ///< [OUT] UTC Hours into the day [range 0..23].
    uint16_t* minutesPtr,       ///< [OUT] UTC Minutes into the hour [range 0..59].
    uint16_t* secondsPtr,       ///< [OUT] UTC Seconds into the minute [range 0..59].
    uint16_t* millisecondsPtr   ///< [OUT] UTC Milliseconds into the second [range 0..999].
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Release a previously requested data connection
 */
//--------------------------------------------------------------------------------------------------
void le_data_Release
(
    le_data_RequestObjRef_t requestRef  ///< [IN] Reference to a previously requested connection
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function removes a handler...
 */
//--------------------------------------------------------------------------------------------------
void le_data_RemoveConnectionStateHandler
(
    le_data_ConnectionStateHandlerRef_t addHandlerRef   ///< [IN] Connection state handler reference
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Return the update status, which is either the last status of the systems swap if it failed, or
 * the status of the secondary bootloader (SBL).
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_fwupdate_GetUpdateStatus
(
    le_fwupdate_UpdateStatus_t *statusPtr, ///< [OUT] Returned update status
    char *statusLabelPtr,                  ///< [OUT] Points to the string matching the status
    size_t statusLabelLength               ///< [IN]  Maximum label length
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Register an handler function for SMS message reception.
 *
 */
//--------------------------------------------------------------------------------------------------
le_sms_RxMessageHandlerRef_t le_sms_AddRxMessageHandler
(
    le_sms_RxMessageHandlerFunc_t handlerFuncPtr, ///< [IN] The handler function for message
    ///  reception.
    void*                         contextPtr      ///< [IN] The handler's context.
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the message format.
 *
 */
//--------------------------------------------------------------------------------------------------
le_sms_Format_t le_sms_GetFormat
(
    le_sms_MsgRef_t msgRef
        ///< [IN] Reference to the message object.
)
{
   return LE_SMS_FORMAT_TEXT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the text Message.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sms_GetText
(
    le_sms_MsgRef_t msgRef,
        ///< [IN] Reference to the message object.
    char* text,
        ///< [OUT] SMS text.
    size_t textSize
        ///< [IN]
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete an SMS message from the storage area.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sms_DeleteFromStorage
(
    le_sms_MsgRef_t msgRef
        ///< [IN] Reference to the message object.
)
{
    return LE_OK;
}
