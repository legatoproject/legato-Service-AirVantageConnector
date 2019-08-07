/**
 * @file avFileTransfer.c
 *
 * AirVantage file transfer service
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include <lwm2mcore/lwm2mcore.h>
#include <lwm2mcore/fileTransfer.h>
#include "avcClient/avcClient.h"
#include <lwm2mcore/lwm2mcorePackageDownloader.h>
#include "downloader.h"
#include "legato.h"
#include "interfaces.h"
#include "avcServer/avcServer.h"
#include "updateInfo.h"
#include "le_print.h"
#include "avcFs/avcFsConfig.h"

//--------------------------------------------------------------------------------------------------
// Definitions
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * AVC file transfer configuration file
 */
//--------------------------------------------------------------------------------------------------
#define FILE_TRANSFER_CONFIG_FILE      AVC_CONFIG_PATH "/" FILE_TRANSFER_CONFIG_PARAM

//--------------------------------------------------------------------------------------------------
/**
 * Default setting for user agreement
 *
 * @note User agreement is disabled by default which means that avcDaemon automatically accepts
 *       requests from the server without requesting user approval. Default value is used when
 *       there is no configuration file stored in the target.
 */
//--------------------------------------------------------------------------------------------------
#define USER_AGREEMENT_DEFAULT  0

//--------------------------------------------------------------------------------------------------
/**
 * File instance list: This list includes all files instances of available files
 * File instance = LwM2M object 33407 instance
 */
//--------------------------------------------------------------------------------------------------
uint16_t FileInstanceList[LWM2MCORE_FILE_TRANSFER_NUMBER_MAX+1];

//--------------------------------------------------------------------------------------------------
/**
 * Available object instance list for object 33407 to be send to LwM2MCore
 */
//--------------------------------------------------------------------------------------------------
static char FileObjectInstanceListPtr[LWM2MCORE_FILE_TRANSFER_OBJECT_INSTANCE_LIST_MAX_LEN + 1];

//--------------------------------------------------------------------------------------------------
/**
 * Limits for FileObjectInstanceListPtr
 */
//--------------------------------------------------------------------------------------------------
#define OBJECT_SEPARATOR    ","
#define OBJECT_START        "</lwm2m/33407/"
#define OBJECT_END          ">"


//--------------------------------------------------------------------------------------------------
// Data structures
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Data associated with user agreement configuration
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    bool download;                  ///< is auto download?
    bool upload;                    ///< is auto upload?
}
UserAgreementConfig_t;


//--------------------------------------------------------------------------------------------------
/**
 * Data associated with file download configuration
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    UserAgreementConfig_t ua;                           ///< User agreement configuration
                                                        ///< made by the polling timer
}
FileTransferConfigData_t;


//--------------------------------------------------------------------------------------------------
/**
 * Data associated with the FileTransferStatusEvent
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_avtransfer_Status_t  status;             ///< File transfer status
    char                    fileName[LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR+1]; ///< File name
    int32_t                 totalNumBytes;      ///< Total number of bytes to download
    int32_t                 progress;           ///< Progress in percent
    void*                   contextPtr;         ///< Context
}
UpdateStatusData_t;


//--------------------------------------------------------------------------------------------------
/**
 * Event for sending file transfer status notification to applications
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t FileTransferStatusEvent;


//--------------------------------------------------------------------------------------------------
/**
 * Number of registered status handlers
 */
//--------------------------------------------------------------------------------------------------
static uint32_t NumStatusHandlers = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Timer for download progress
 */
//--------------------------------------------------------------------------------------------------
static le_timer_Ref_t DownloadProgressTimer;

//--------------------------------------------------------------------------------------------------
/**
 * Download progress timer duration (seconds)
 */
//--------------------------------------------------------------------------------------------------
#define AV_FILE_TRANSFER_DOWNLOAD_PROGRESS_TIMER        120

//--------------------------------------------------------------------------------------------------
/**
 * Download progress reduction
 */
//--------------------------------------------------------------------------------------------------
#define AV_FILE_TRANSFER_DOWNLOAD_PROGRESS_REDUCTION    4

//--------------------------------------------------------------------------------------------------
/**
 * Download progress step
 */
//--------------------------------------------------------------------------------------------------
#define AV_FILE_TRANSFER_DOWNLOAD_STEP                  5

//--------------------------------------------------------------------------------------------------
/**
 * Static time of the last download progress notification
 */
//--------------------------------------------------------------------------------------------------
static time_t LastDownloadProgressReportTime = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Static transfer progress of the last download progress notification
 */
//--------------------------------------------------------------------------------------------------
static uint8_t LastDownloadProgressReportProgress = 0;

//--------------------------------------------------------------------------------------------------
// Local functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Initialize file instance list
 *
 * @note This function should be called during the initializaion phase of the AVC daemon.
 */
//--------------------------------------------------------------------------------------------------
void avFileTransfer_InitFileInstanceList
(
    void
)
{
    int loop = 0;
    uint32_t writtenLen = 0;
    size_t listSize = LWM2MCORE_FILE_TRANSFER_NUMBER_MAX;
    memset(FileInstanceList, UINT16_MAX, sizeof(uint16_t)*(LWM2MCORE_FILE_TRANSFER_NUMBER_MAX+1));
    if (LE_OK == le_fileStreamServer_GetFileInstanceList(FileInstanceList, &listSize))
    {
        memset(FileObjectInstanceListPtr,
               0,
               LWM2MCORE_FILE_TRANSFER_OBJECT_INSTANCE_LIST_MAX_LEN + 1);
        for (loop = 0; loop < listSize; loop++)
        {
            if(loop)
            {
                strncat(FileObjectInstanceListPtr+writtenLen,
                        OBJECT_SEPARATOR,
                        LWM2MCORE_FILE_TRANSFER_OBJECT_INSTANCE_LIST_MAX_LEN - writtenLen);
                writtenLen += strlen(OBJECT_SEPARATOR);
            }
            snprintf(FileObjectInstanceListPtr+writtenLen,
                     LWM2MCORE_FILE_TRANSFER_OBJECT_INSTANCE_LIST_MAX_LEN - writtenLen,
                     "%s%d%s",
                     OBJECT_START,
                     FileInstanceList[loop],
                     OBJECT_END);
            writtenLen = strlen(FileObjectInstanceListPtr);
        }
        LE_DEBUG("FileObjectInstanceListPtr %s", FileObjectInstanceListPtr);
        lwm2mcore_UpdateFileTransferList(avcClient_GetInstance(),
                                         FileObjectInstanceListPtr,
                                         writtenLen);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 *  Convert file transfer state to string.
 */
//--------------------------------------------------------------------------------------------------
static char* FileTransferStateToStr
(
    le_avtransfer_Status_t state  ///< [IN] The file transfer state to convert.
)
{
    char* result;

    switch (state)
    {
        case LE_AVTRANSFER_NONE:        result = "No file to be transferred";   break;
        case LE_AVTRANSFER_PENDING:     result = "File transfer pending";       break;
        case LE_AVTRANSFER_IN_PROGRESS: result = "File transfer in progress";   break;
        case LE_AVTRANSFER_COMPLETE:    result = "File transfer complete";      break;
        case LE_AVTRANSFER_FAILED:      result = "File transfer Failed";        break;
        case LE_AVTRANSFER_DELETED:     result = "A file was deleted";          break;
        case LE_AVTRANSFER_ABORTED:     result = "A file transfer was aborted"; break;
        default:                        result = "File transfer: Unknown";      break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer Update Status Handler
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerUpdateStatusHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    UpdateStatusData_t* eventDataPtr = reportPtr;
    le_avtransfer_StatusHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    clientHandlerFunc(eventDataPtr->status,
                      eventDataPtr->fileName,
                      eventDataPtr->totalNumBytes,
                      eventDataPtr->progress,
                      le_event_GetContextPtr());
}

//--------------------------------------------------------------------------------------------------
/**
 * Send file transfer status event to registered applications
 */
//--------------------------------------------------------------------------------------------------
void avFileTransfer_SendStatusEvent
(
    le_avtransfer_Status_t  status,             ///< [IN] File transfer status
    char*                   fileNamePtr,        ///< [IN] File name
    int32_t                 totalNumBytes,      ///< [IN] Total number of bytes to download
    int32_t                 progress,           ///< [IN] Progress in percent
    void*                   contextPtr          ///< [IN] Context
)
{
    UpdateStatusData_t eventData;

    // Initialize the event data
    memset(&eventData, 0, sizeof(UpdateStatusData_t));
    eventData.status = status;
    snprintf(eventData.fileName,
             LWM2MCORE_FILE_TRANSFER_NAME_MAX_CHAR+1,
             "%s",
             fileNamePtr);
    eventData.totalNumBytes = totalNumBytes;
    eventData.progress = progress;
    eventData.contextPtr = contextPtr;

    if(LE_AVTRANSFER_COMPLETE == status)
    {
        eventData.progress = 100;
        eventData.totalNumBytes = 0;
    }

    LE_DEBUG("Reporting %s", FileTransferStateToStr(status));
    LE_DEBUG("File %s", fileNamePtr);
    LE_DEBUG("Number of bytes to download %"PRId32, eventData.totalNumBytes);
    LE_DEBUG("Progress %"PRId32, eventData.progress);
    LE_DEBUG("ContextPtr %p", eventData.contextPtr);

    // Send the event to interested applications
    le_event_Report(FileTransferStatusEvent, &eventData, sizeof(eventData));
}

//--------------------------------------------------------------------------------------------------
/**
 * Write file transfer configuration parameter to platform memory
 *
 * @return
 *      - LE_OK if successful
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SetFileTransferConfig
(
    FileTransferConfigData_t* configPtr   ///< [IN] configuration data buffer
)
{
    le_result_t result;
    size_t size = sizeof(FileTransferConfigData_t);

    if (NULL == configPtr)
    {
        LE_ERROR("AVC configuration pointer is null");
        return LE_FAULT;
    }

    result = WriteFs(FILE_TRANSFER_CONFIG_FILE, (uint8_t*)configPtr, size);

    if (LE_OK == result)
    {
        return LE_OK;
    }
    else
    {
        LE_ERROR("Error writing to %s", FILE_TRANSFER_CONFIG_FILE);
        return LE_FAULT;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Read AVC configuration parameter to platform memory
 *
 * @return
 *      - LE_OK if successful
 *      - LE_UNAVAILABLE if the configuration file is not present or can not be read
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetFileTransferConfig
(
    FileTransferConfigData_t* configPtr   ///< [INOUT] configuration data buffer
)
{
    le_result_t result;

    if (NULL == configPtr)
    {
        LE_ERROR("AVC configuration pointer is null");
        return LE_FAULT;
    }

    size_t size = sizeof(FileTransferConfigData_t);
    result = ReadFs(FILE_TRANSFER_CONFIG_FILE, (uint8_t*)configPtr, &size);

    if (LE_OK == result)
    {
        return LE_OK;
    }
    else
    {
        LE_ERROR("Error reading from %s", FILE_TRANSFER_CONFIG_FILE);
        return LE_UNAVAILABLE;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the default file transfer config
 */
//--------------------------------------------------------------------------------------------------
static void SetFileTransferDefaultConfig
(
    void
)
{
    FileTransferConfigData_t config;

    // set user agreement to default
    config.ua.download = USER_AGREEMENT_DEFAULT;
    config.ua.upload = USER_AGREEMENT_DEFAULT;

    // write the config file
    SetFileTransferConfig(&config);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get time
 */
//--------------------------------------------------------------------------------------------------
static time_t GetTime
(
    void
)
{
    le_clk_Time_t deviceTime = le_clk_GetAbsoluteTime();
    LE_DEBUG("Device time: %ld", deviceTime.sec);
    return deviceTime.sec;
}

//--------------------------------------------------------------------------------------------------
/**
 * Treatment for transfer progress notification
 */
//--------------------------------------------------------------------------------------------------
static void SendCheckRoute
(
    void
)
{
    LE_DEBUG("SendCheckRoute for file transfer");
    if (LE_OK != le_avc_CheckRoute())
    {
        LE_WARN("Not possible to check the route during file transfer");
        return;
    }
    LastDownloadProgressReportTime = GetTime();
    if (!le_timer_IsRunning(DownloadProgressTimer))
    {
        le_clk_Time_t interval = {.sec = AV_FILE_TRANSFER_DOWNLOAD_PROGRESS_TIMER};
        if (LE_OK != le_timer_SetInterval(DownloadProgressTimer, interval))
        {
            LE_WARN("Issue to start file transfer progress timer");
            return;
        }
        le_timer_Start(DownloadProgressTimer);
    }
    else
    {
        le_timer_Restart(DownloadProgressTimer);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Called when the download progress timer expires.
 */
//--------------------------------------------------------------------------------------------------
static void DownloadProgressTimerExpiryHandler
(
    le_timer_Ref_t timerRef    ///< [IN] Timer that expired
)
{
    SendCheckRoute();
}

//--------------------------------------------------------------------------------------------------
// Internal interface functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the AVC file transfer sub-component.
 *
 * @note This function should be called during the initializaion phase of the AVC daemon.
 */
//--------------------------------------------------------------------------------------------------
void avFileTransfer_Init
(
   void
)
{
    FileTransferStatusEvent = le_event_CreateId("File transfer Status", sizeof(UpdateStatusData_t));
    DownloadProgressTimer = le_timer_Create("File download progress timer");
    le_timer_SetHandler(DownloadProgressTimer, DownloadProgressTimerExpiryHandler);

    // Write default if configuration file doesn't exist
    if(LE_OK != ExistsFs(FILE_TRANSFER_CONFIG_FILE))
    {
        LE_INFO("Set default configuration");
        SetFileTransferDefaultConfig();
    }

    // Update the supported object instances list
    avFileTransfer_InitFileInstanceList();
}


//--------------------------------------------------------------------------------------------------
/**
 * Convert an AVC update status to corresponding file transfer status
 *
 * @return
 *      - le_avtransfer_Status_t for correct convertion
 *      - LE_AVTRANSFER_MAX otherwise
 */
//--------------------------------------------------------------------------------------------------
le_avtransfer_Status_t avFileTransfer_ConvertAvcState
(
    le_avc_Status_t     avcUpdateStatus         ///< [IN] AVC update status to convert
)
{
    le_avtransfer_Status_t avtransferStatus = LE_AVTRANSFER_MAX;

    switch(avcUpdateStatus)
    {
        case LE_AVC_DOWNLOAD_PENDING:
            avtransferStatus = LE_AVTRANSFER_PENDING;
            break;

        case LE_AVC_DOWNLOAD_IN_PROGRESS:
            avtransferStatus = LE_AVTRANSFER_IN_PROGRESS;
            break;

        case LE_AVC_DOWNLOAD_COMPLETE:
            avtransferStatus = LE_AVTRANSFER_COMPLETE;
            break;

        case LE_AVC_DOWNLOAD_FAILED:
            avtransferStatus = LE_AVTRANSFER_FAILED;
            break;

        default:
            break;
    }

    LE_DEBUG("Convert AV update status %d to av file transfer status %d",
             avcUpdateStatus, avtransferStatus);

    return avtransferStatus;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the file name for the file transfer
 *
 * return
 *  - LE_OK if succeeds
 *  - LE_OVERFLOW on buffer overflow
 *  - LE_BAD_PARAMETER if parameter is invalid
 *  - LE_FAULT other failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t avFileTransfer_GetTransferName
(
    char*   bufferPtr,              ///< [OUT] Buffer
    size_t* bufferSizePtr           ///< [OUT] Buffer size
)
{
    le_fileStreamClient_StreamMgmt_t streamMgmtObj;
    uint16_t instanceId = UINT16_MAX;

    if ((!bufferPtr) || (!bufferSizePtr))
    {
        return LE_BAD_PARAMETER;
    }

    if (le_fileStreamClient_GetStreamMgmtObject(instanceId, &streamMgmtObj) != LE_OK)
    {
        return LE_FAULT;
    }

    if (strlen (streamMgmtObj.pkgName) > (*bufferSizePtr))
    {
        return LE_OVERFLOW;
    }

    strncpy(bufferPtr, streamMgmtObj.pkgName, *bufferSizePtr);
    LE_INFO("file name: %s", bufferPtr);
    *bufferSizePtr = strlen(bufferPtr);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Treat file transfer progress
 */
//--------------------------------------------------------------------------------------------------
void avFileTransfer_TreatProgress
(
    bool        isLaunched,         ///< [IN] Is transfer launched?
    uint8_t     downloadProgress    ///< [IN] Download progress
)
{
    LE_DEBUG("File transfer: isLaunched %d, progress %d", isLaunched, downloadProgress);

    if (isLaunched)
    {
        time_t now = GetTime();
        if (!downloadProgress)
        {
            LE_DEBUG("Reset last transfer progress");
            LastDownloadProgressReportProgress = 0;
            LastDownloadProgressReportTime = now;
        }

        if ( (downloadProgress > LastDownloadProgressReportProgress)
          && ((downloadProgress - LastDownloadProgressReportProgress) >=
                                                                    AV_FILE_TRANSFER_DOWNLOAD_STEP)
          && (downloadProgress != 100)
        )
        {
            time_t timeDiff;
            if (AV_FILE_TRANSFER_DOWNLOAD_PROGRESS_REDUCTION)
            {
                timeDiff = AV_FILE_TRANSFER_DOWNLOAD_PROGRESS_TIMER /
                           AV_FILE_TRANSFER_DOWNLOAD_PROGRESS_REDUCTION;
            }
            else
            {
                timeDiff = AV_FILE_TRANSFER_DOWNLOAD_PROGRESS_TIMER;
            }
            LE_DEBUG("timeDiff %ld", timeDiff);
            if  ((now > LastDownloadProgressReportTime)
             && ((now - LastDownloadProgressReportTime) > timeDiff)
            )
            {
                SendCheckRoute();
                LastDownloadProgressReportProgress = downloadProgress;
            }
        }
    }
    else
    {
        le_timer_Stop(DownloadProgressTimer);
    }
}


//--------------------------------------------------------------------------------------------------
// API functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * le_avtransfer_StatusHandler handler ADD function
 */
//--------------------------------------------------------------------------------------------------
le_avtransfer_StatusEventHandlerRef_t le_avtransfer_AddStatusEventHandler
(
    le_avtransfer_StatusHandlerFunc_t handlerPtr,   ///< [IN] Pointer on handler function
    void* contextPtr                                ///< [IN] Context pointer
)
{
    le_event_HandlerRef_t handlerRef;

    // handlerPtr must be valid
    if (NULL == handlerPtr)
    {
        LE_KILL_CLIENT("Null handlerPtr");
        return NULL;
    }

    LE_PRINT_VALUE("%p", handlerPtr);
    LE_PRINT_VALUE("%p", contextPtr);

    // Register the user app handler
    handlerRef = le_event_AddLayeredHandler("FileTransferUpdateStaus",
                                            FileTransferStatusEvent,
                                            FirstLayerUpdateStatusHandler,
                                            (le_event_HandlerFunc_t)handlerPtr);
    le_event_SetContextPtr(handlerRef, contextPtr);

    // Number of user apps registered
    NumStatusHandlers++;

    // Check if any notification needs to be sent to the application concerning file transfer
    //CheckNotificationToSend(handlerPtr, contextPtr); // TODO
    /*if (NotifyApplication)
    {
        handlerPtr(UpdateStatusNotification, -1, -1, contextPtr);
    }*/
    return (le_avtransfer_StatusEventHandlerRef_t)handlerRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * le_avtransfer_StatusHandler handler REMOVE function
 */
//--------------------------------------------------------------------------------------------------
void le_avtransfer_RemoveStatusEventHandler
(
    le_avtransfer_StatusEventHandlerRef_t addHandlerRef     ///< [IN] Handler to remove
)
{
    LE_PRINT_VALUE("%p", addHandlerRef);

    le_event_RemoveHandler((le_event_HandlerRef_t)addHandlerRef);

    // Decrement number of registered handlers
    NumStatusHandlers--;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to get the user agreement state
 *
 * @return
 *      - LE_OK on success
 *      - LE_UNAVAILABLE if reading the config file failed
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avtransfer_GetUserAgreement
(
    le_avtransfer_UserAgreement_t   userAgreement,      ///< [IN] User agreement operation
    bool*                           isEnabledPtr        ///< [OUT] true if enabled
)
{
    le_result_t result = LE_OK;
    FileTransferConfigData_t config;

    if (isEnabledPtr == NULL)
    {
        LE_KILL_CLIENT("isEnabledPtr is NULL.");
        return LE_FAULT;
    }

    // Retrieve configuration from le_fs
    result = GetFileTransferConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to retrieve avc config from le_fs");
       return result;
    }

    switch (userAgreement)
    {
        case LE_AVTRANSFER_USER_AGREEMENT_DOWNLOAD:
            *isEnabledPtr = config.ua.download;
            break;

        case LE_AVTRANSFER_USER_AGREEMENT_UPLOAD:
            *isEnabledPtr = config.ua.upload;
            break;

        default:
            *isEnabledPtr = false;
            result = LE_FAULT;
            break;
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to set the user agreement state
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avtransfer_SetUserAgreement
(
    le_avtransfer_UserAgreement_t   userAgreement,  ///< [IN] Operation for which user agreement
                                                    ///  is configured
    bool                            isEnabled       ///< [IN] true if enabled
)
{
    le_result_t result = LE_OK;
    FileTransferConfigData_t config;

    // Retrieve configuration from le_fs
    result = GetFileTransferConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to retrieve avc config from le_fs");
       return result;
    }

    switch (userAgreement)
    {
        case LE_AVTRANSFER_USER_AGREEMENT_DOWNLOAD:
            LE_DEBUG("Set user agreement for file transfer download %d", isEnabled);
            config.ua.download = isEnabled;
            break;

        case LE_AVTRANSFER_USER_AGREEMENT_UPLOAD:
            config.ua.upload = isEnabled;
            break;

        default:
            LE_ERROR("User agreement configuration invalid");
            break;
    }

    // Write configuration to le_fs
    result = SetFileTransferConfig(&config);
    if (result != LE_OK)
    {
       LE_ERROR("Failed to write avc config from le_fs");
       return result;
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to accept a file transfer
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avtransfer_Accept
(
    void
)
{
    return le_avc_AcceptDownload();
}


//--------------------------------------------------------------------------------------------------
/**
 * Function to suspend a file transfer
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avtransfer_Suspend
(
    void
)
{
    downloader_SuspendDownload();
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to synchronize the LwM2M objects regarding stored files with the server
 */
//--------------------------------------------------------------------------------------------------
void le_avtransfer_Synchronize
(
)
{
    avFileTransfer_InitFileInstanceList();
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to abort a file transfer
 *
 * @return
 *      - LE_OK on success.
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_avtransfer_Abort
(
    void
)
{
    lwm2mcore_Sid_t sID = lwm2mcore_AbortDownload();
    LE_DEBUG("Abort request returns sID %d", sID);

    if (LWM2MCORE_ERR_COMPLETED_OK  != sID)
    {
        return LE_FAULT;
    }
    le_avc_CheckRoute();

    return LE_OK;
}

