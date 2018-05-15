#ifndef _INTERFACES_H
#define _INTERFACES_H

#include "le_avc_interface.h"
#include "le_avdata_interface.h"
#include "le_cfg_interface.h"
#include "le_data_interface.h"
#include "le_sms_interface.h"
#include "lwm2mcore.h"
#include "update.h"
#include "liblwm2m.h"
#include "lwm2mcore/timer.h"
#include "avcAppUpdate.h"


//--------------------------------------------------------------------------------------------------
// airVantage connector stubbing
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of the update status label
 */
//--------------------------------------------------------------------------------------------------
#define LE_FWUPDATE_STATUS_LABEL_LENGTH_MAX 32

//--------------------------------------------------------------------------------------------------
/**
 * Update status
 * Indicates either a success or the defective image partition.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    LE_FWUPDATE_UPDATE_STATUS_OK = 0,
        ///< Last update succeeded
    LE_FWUPDATE_UPDATE_STATUS_PARTITION_ERROR = 1,
        ///< At least, a partition is corrupted
    LE_FWUPDATE_UPDATE_STATUS_DWL_ONGOING = 2,
        ///< Downloading in progress
    LE_FWUPDATE_UPDATE_STATUS_DWL_FAILED = 3,
        ///< Last downloading failed
    LE_FWUPDATE_UPDATE_STATUS_DWL_TIMEOUT = 4,
        ///< Last downloading ended with timeout
    LE_FWUPDATE_UPDATE_STATUS_UNKNOWN = 5
        ///< Unknown status. It has to be the last one.
}
le_fwupdate_UpdateStatus_t;

//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionRef_t le_avc_GetClientSessionRef
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the server service reference
 */
//--------------------------------------------------------------------------------------------------
le_msg_ServiceRef_t le_avc_GetServiceRef
(
    void
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to retrieve the International Mobile Equipment Identity (IMEI).
 *
 * @return LE_FAULT       The function failed to retrieve the IMEI.
 * @return LE_OVERFLOW    The IMEI length exceed the maximum length.
 * @return LE_OK          The function succeeded.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_info_GetImei
(
    char*            imeiPtr,  ///< [OUT] The IMEI string.
    size_t           len       ///< [IN] The length of IMEI string.
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the Radio Module power state.
 *
 * @return LE_FAULT         The function failed to get the Radio Module power state.
 * @return LE_BAD_PARAMETER if powerPtr is NULL.
 * @return LE_OK            The function succeed.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_mrc_GetRadioPower
(
    le_onoff_t*    powerPtr   ///< [OUT] The power state.
);

//--------------------------------------------------------------------------------------------------
/**
 * Setup temporary files
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Init this sub-component
 */
//--------------------------------------------------------------------------------------------------
le_result_t assetData_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the avData module
 */
//--------------------------------------------------------------------------------------------------
void avData_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize time series
 */
//--------------------------------------------------------------------------------------------------
le_result_t timeSeries_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialization function avcApp. Should be called only once.
 */
//--------------------------------------------------------------------------------------------------
void avcApp_Init
(
   void
);

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update install pending status
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateInstallPending
(
    bool* isFwInstallPendingPtr                  ///< [OUT] Is FW install pending?
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Function to set the lifetime in the server object and save to disk.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_SetLifetime
(
    uint32_t lifetime               ///< [IN] Lifetime in seconds
);

//--------------------------------------------------------------------------------------------------
/**
 * Function to read the lifetime from the server object.
 *
 * @return
 *      - LWM2MCORE_ERR_COMPLETED_OK if the treatment succeeds
 *      - LWM2MCORE_ERR_GENERAL_ERROR if the treatment fails
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Sid_t lwm2mcore_GetLifetime
(
    uint32_t* lifetimePtr           ///< [OUT] Lifetime in seconds
);

//--------------------------------------------------------------------------------------------------
/**
 * Get firmware update state
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t packageDownloader_GetFwUpdateState
(
    lwm2mcore_FwUpdateState_t* fwUpdateStatePtr     ///< [INOUT] FW update state
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the LWM2M core
 *
 * @return
 *  - instance reference
 *  - NULL in case of error
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_Ref_t lwm2mcore_Init
(
    lwm2mcore_StatusCb_t eventCb    ///< [IN] event callback
);

//--------------------------------------------------------------------------------------------------
/**
 * Simulate a new Lwm2m event
 */
//--------------------------------------------------------------------------------------------------
void le_avcTest_SimulateLwm2mEvent
(
    lwm2mcore_StatusType_t status,   ///< Event
    lwm2mcore_PkgDwlType_t pkgType,  ///< Package type.
    uint32_t numBytes,               ///< For package download, num of bytes to be downloaded
    uint32_t progress                ///< For package download, package download progress in %
);

//--------------------------------------------------------------------------------------------------
/**
 * Adaptation function for timer state
 *
 * @return
 *      - true  if the timer is running
 *      - false if the timer is stopped
 */
//--------------------------------------------------------------------------------------------------
bool lwm2mcore_TimerIsRunning
(
    lwm2mcore_TimerType_t timer    ///< [IN] Timer Id
);

#endif /* interfaces.h */
