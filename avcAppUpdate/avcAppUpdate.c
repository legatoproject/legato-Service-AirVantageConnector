
//--------------------------------------------------------------------------------------------------
/**
 * This file handles managing application update (legato side) over LWM2M.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include <lwm2mcore/update.h>
#include "legato.h"
#include "interfaces.h"
#include "le_print.h"
#include "appCfg.h"
#include "assetData.h"
#include "avcServer.h"
#include "lwm2mcorePackageDownloader.h"
#include "packageDownloader.h"
#include "avcAppUpdate.h"
#include "avcFsConfig.h"

//--------------------------------------------------------------------------------------------------
/**
 *  String to return when an application does not include it's own version string.
 */
//--------------------------------------------------------------------------------------------------
#define VERSION_UNKNOWN "unknown"

//--------------------------------------------------------------------------------------------------
/**
 *  Maximum allowed size for application name strings.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_APP_NAME        LE_LIMIT_APP_NAME_LEN
#define MAX_APP_NAME_BYTES  (MAX_APP_NAME + 1)

//--------------------------------------------------------------------------------------------------
/**
 *  Maximum size of the download file path
 */
//--------------------------------------------------------------------------------------------------
#define MAX_FILE_PATH_BYTES 256

//--------------------------------------------------------------------------------------------------
/**
 * Name of the temporary download file.
 */
//--------------------------------------------------------------------------------------------------
#define NAME_DOWNLOAD_FILE          "/download.update"

//--------------------------------------------------------------------------------------------------
/**
 *  Maximum allowed size for lwm2m object list strings.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_OBJ9_STR             20
#define MAX_OBJ9_NUM             256
#define MAX_OBJ9_STR_LIST_BYTES  ((MAX_OBJ9_STR*MAX_OBJ9_NUM) + 1)

//--------------------------------------------------------------------------------------------------
/**
 *  Base path for an Object 9 application binding inside of the configTree.
 */
//--------------------------------------------------------------------------------------------------
#define CFG_OBJECT_INFO_PATH "system:/lwm2m/objectMap"

//--------------------------------------------------------------------------------------------------
/**
 *  Base path of lwm2m config tree.
 */
//--------------------------------------------------------------------------------------------------
#define CFG_OBJECT_PATH    "system:/lwm2m"

//--------------------------------------------------------------------------------------------------
/**
 *  objectMap node name in lwm2m config tree.
 */
//--------------------------------------------------------------------------------------------------
#define CFG_OBJECT_MAP    "objectMap"

//--------------------------------------------------------------------------------------------------
/**
 * Buffer size for package store.
 */
//--------------------------------------------------------------------------------------------------
#define DWL_STORE_BUF_SIZE (16*1024)

//--------------------------------------------------------------------------------------------------
/**
 * File descriptor to read the package from.
 */
//--------------------------------------------------------------------------------------------------
static int UpdateReadFd = -1;

//--------------------------------------------------------------------------------------------------
/**
 * File descriptor to store the package to.
 */
//--------------------------------------------------------------------------------------------------
static int UpdateStoreFd = -1;

//--------------------------------------------------------------------------------------------------
/**
 * Reference to the FD Monitor for the input stream
 */
//--------------------------------------------------------------------------------------------------
static le_fdMonitor_Ref_t StoreFdMonitor = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Total number of bytes of payload written to disk.
 */
//--------------------------------------------------------------------------------------------------
static size_t TotalCount = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Downloaded package will be stored in this directory.
 */
//--------------------------------------------------------------------------------------------------
static const char* AppDownloadPath = "/legato/download";

//--------------------------------------------------------------------------------------------------
/**
 *  Indices for all of the fields of object 9.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    O9F_PKG_NAME                 = 0,   ///< Application name.
    O9F_PKG_VERSION              = 1,   ///< Application version.
    O9F_PACKAGE                  = 2,   ///< <Not supported>
    O9F_PACKAGE_URI              = 3,   ///< Uri for downloading a new application.
    O9F_INSTALL                  = 4,   ///< Command to start an install operation.
    O9F_CHECKPOINT               = 5,   ///< <Not supported>
    O9F_UNINSTALL                = 6,   ///< Command to remove an application.
    O9F_UPDATE_STATE             = 7,   ///< The install state of the application.
    O9F_UPDATE_SUPPORTED_OBJECTS = 8,   ///< Inform the registered LWM2M Servers of Objects and
                                        ///< Object Instances parameter after the SW update
                                        ///< operation
    O9F_UPDATE_RESULT            = 9,   ///< The result of the last install request.
    O9F_ACTIVATE                 = 10,  ///< Command to start the application.
    O9F_DEACTIVATE               = 11,  ///< Command to stop the application.
    O9F_ACTIVATION_STATE         = 12,  ///< Report if the application is running.
    O9F_PACKAGE_SETTINGS         = 13   ///< <Not supported>
}
LwObj9Fids;

//--------------------------------------------------------------------------------------------------
/**
 *  The current instance of object 9 that is being downloaded to. NULL if no downloads or
 *  installations are taking place.
 */
//--------------------------------------------------------------------------------------------------
static assetData_InstanceDataRef_t CurrentObj9 = NULL;

//--------------------------------------------------------------------------------------------------
/**
 *  Whether the install is initated from AVMS server or locally using 'app remove'
 */
//--------------------------------------------------------------------------------------------------
static bool AvmsInstall = false;

//--------------------------------------------------------------------------------------------------
/**
 *  Started update process?
 */
//--------------------------------------------------------------------------------------------------
static bool UpdateStarted = false;

//--------------------------------------------------------------------------------------------------
/**
 * Event ID to end update.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t UpdateEndEventId;

//--------------------------------------------------------------------------------------------------
/**
 * Event ID to start download.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t DownloadEventId;

//--------------------------------------------------------------------------------------------------
/**
 *  Convert an UpdateState value to a string for debugging.
 *
 *  @return string version of the supplied enumeration value.
 */
//--------------------------------------------------------------------------------------------------
static const char* UpdateStateToStr
(
    lwm2mcore_SwUpdateState_t state  ///< The enumeration value to convert.
)
{
    const char* resultPtr;

    switch (state)
    {
        case LWM2MCORE_SW_UPDATE_STATE_INITIAL:
            resultPtr = "LWM2MCORE_SW_UPDATE_STATE_INITIAL";
            break;
        case LWM2MCORE_SW_UPDATE_STATE_DOWNLOAD_STARTED:
            resultPtr = "LWM2MCORE_SW_UPDATE_STATE_DOWNLOAD_STARTED";
            break;
        case LWM2MCORE_SW_UPDATE_STATE_DOWNLOADED:
            resultPtr = "LWM2MCORE_SW_UPDATE_STATE_DOWNLOADED";
            break;
        case LWM2MCORE_SW_UPDATE_STATE_DELIVERED:
            resultPtr = "LWM2MCORE_SW_UPDATE_STATE_DELIVERED";
            break;
        case LWM2MCORE_SW_UPDATE_STATE_INSTALLED:
            resultPtr = "LWM2MCORE_SW_UPDATE_STATE_INSTALLED";
            break;
        case LWM2MCORE_SW_UPDATE_STATE_WAITINSTALLRESULT:
            resultPtr = "LWM2MCORE_SW_UPDATE_STATE_WAITINSTALLRESULT";
            break;
        default:
            resultPtr = "Unknown";
            break;
    }
    return resultPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Convert an UpdateResult value to a string for debugging.
 *
 *  @return string version of the supplied enumeration value.
 */
//--------------------------------------------------------------------------------------------------
static const char* UpdateResultToStr
(
    lwm2mcore_SwUpdateResult_t swUpdateResult  ///< The enumeration value to convert.
)
{
    const char* resultPtr;

    switch (swUpdateResult)
    {
        case LWM2MCORE_SW_UPDATE_RESULT_INITIAL:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_INITIAL";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADING:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADING";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_INSTALLED:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_INSTALLED";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_NOT_ENOUGH_MEMORY:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_NOT_ENOUGH_MEMORY";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_OUT_OF_MEMORY:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_OUT_OF_MEMORY";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_CONNECTION_LOST:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_CONNECTION_LOST";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_CHECK_FAILURE:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_CHECK_FAILURE";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_UNSUPPORTED_TYPE:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_UNSUPPORTED_TYPE";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_INVALID_URI:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_INVALID_URI";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_DEVICE_ERROR:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_DEVICE_ERROR";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_INSTALL_FAILURE:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_INSTALL_FAILURE";
            break;
        case LWM2MCORE_SW_UPDATE_RESULT_UNINSTALL_FAILURE:
            resultPtr = "LWM2MCORE_SW_UPDATE_RESULT_UNINSTALL_FAILURE";
            break;
        default:
            resultPtr = "Unknown";
            break;
    }
    return resultPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 *  If a given app is in the "disapproved" list, is is not exposed through LWM2M.
 *
 *  @return true if the app is hidden from lwm2m, false if not.
 */
//--------------------------------------------------------------------------------------------------
static bool IsHiddenApp
(
    const char* appNamePtr  ///< Name of the application to check.
)
{
    if (true == le_cfg_QuickGetBool("/lwm2m/hideDefaultApps", true))
    {
        static char* appList[] =
            {
                "airvantage",
                "audioService",
                "avcService",
                "cellNetService",
                "dataConnectionService",
                "modemService",
                "positioningService",
                "powerMgr",
                "secStore",
                "voiceCallService",
                "fwupdateService",
                "smsInboxService",
                "gpioService",
                "tools",
                "atService",
                "devMode",
                "spiService",
                "wifi",
                "wifiApTest",
                "wifiClientTest",
                "wifiService",
                "wifiWebAp"
            };

        for (size_t i = 0; i < NUM_ARRAY_MEMBERS(appList); i++)
        {
            if (0 == strcmp(appList[i], appNamePtr))
            {
                return true;
            }
        }
    }
    return false;
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete the downloaded package.
 */
//--------------------------------------------------------------------------------------------------
static void DeletePackage
(
    void
)
{
    // Remove the download directory
    LE_FATAL_IF(le_dir_RemoveRecursive(AppDownloadPath) != LE_OK,
                "Failed to recursively delete '%s'.",
                AppDownloadPath);

    // Remove the sw update workspace directory
    LE_FATAL_IF(le_dir_RemoveRecursive(SW_UPDATE_INFO_DIR) != LE_OK,
                "Failed to recursively delete '%s'.",
                SW_UPDATE_INFO_DIR);
}


//--------------------------------------------------------------------------------------------------
/**
 *  Handler to terminate an ongoing update.
 */
//--------------------------------------------------------------------------------------------------
void UpdateEndHandler
(
    void *reportPtr
)
{
    le_update_End();

    LE_DEBUG("Delete package downloaded.");
    DeletePackage();
}

//--------------------------------------------------------------------------------------------------
/**
 *  Update the state of the object 9 instance. Also, because they are so closely related, update
 *  the update result field while we're at it.
 */
//--------------------------------------------------------------------------------------------------
static void SetObj9State_
(
    assetData_InstanceDataRef_t instanceRef,  ///< The instance to update.
    lwm2mcore_SwUpdateState_t state,          ///< The new state.
    lwm2mcore_SwUpdateResult_t result,        ///< The new result.
    const char* functionNamePtr,              ///< Name of the function that called this one.
    size_t line                               ///< The line of this file this function was called
                                              ///< from.
)
{
    int instanceId;

    if (instanceRef == NULL)
    {
        LE_WARN("Setting state on NULL object.");
        return;
    }

    assetData_GetInstanceId(instanceRef, &instanceId);
    LE_DEBUG("<%s: %zu>: Set object 9 state/result on instance %d: (%d) %s / (%d) %s",
             functionNamePtr,
             line,
             instanceId,
             state,
             UpdateStateToStr(state),
             result,
             UpdateResultToStr(result));

    LE_ASSERT_OK(assetData_client_SetInt(instanceRef, O9F_UPDATE_STATE, state));
    LE_ASSERT_OK(assetData_client_SetInt(instanceRef, O9F_UPDATE_RESULT, result));

    LE_DEBUG("Save the state and result in a file for suspend / resume");
    avcApp_SetDownloadState(state);
    avcApp_SetDownloadResult(result);

    // Send a registration update after changing the obj state/result of the device.
    // This will trigger the server to query for the state/result.
    avcClient_Update();
}

#define SetObj9State(insref, state, result) SetObj9State_(insref,       \
                                                          state,        \
                                                          result,       \
                                                          __FUNCTION__, \
                                                          __LINE__)

//--------------------------------------------------------------------------------------------------
/**
 *  Set the LWM2M object 9 instance mapping for the application. If NULL is passed for the instance
 *  reference, then any association is cleared.
 */
//--------------------------------------------------------------------------------------------------
static void SetObject9InstanceForApp
(
    const char* appNamePtr,                   ///< The name of the application in question.
    assetData_InstanceDataRef_t instanceRef   ///< The instance of object 9 to link to.  Pass NULL
                                              ///< if the link is to be cleared.
)
{
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateWriteTxn(CFG_OBJECT_INFO_PATH);

    if (instanceRef != NULL)
    {
        int instanceId;
        LE_ASSERT_OK(assetData_GetInstanceId(instanceRef, &instanceId));

        le_cfg_GoToNode(iterRef, appNamePtr);
        le_cfg_SetInt(iterRef, "oiid", instanceId);

        LE_DEBUG("Application '%s' mapped to instance %d.", appNamePtr, instanceId);
    }
    else
    {
        le_cfg_DeleteNode(iterRef, appNamePtr);
        LE_DEBUG("Deletion of '%s' from cfgTree %s successful", appNamePtr, CFG_OBJECT_INFO_PATH);
    }

    le_cfg_CommitTxn(iterRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Read the current state of the given object 9 instance.
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_SwUpdateState_t GetObj9State
(
    assetData_InstanceDataRef_t instanceRef  ///< The object instance to read.
)
{
    int state;
    LE_DEBUG("InstanceRef: %p", instanceRef);

    LE_ASSERT_OK(assetData_client_GetInt(instanceRef, O9F_UPDATE_STATE, &state));
    return (lwm2mcore_SwUpdateState_t)state;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Try to get the current object 9 instance for the given application.  If one can not be found
 *  then create one.
 */
//--------------------------------------------------------------------------------------------------
static assetData_InstanceDataRef_t GetObject9InstanceForApp
(
    const char* appNamePtr,  ///< Name of the application in question.
    bool mapIfNotFound       ///< If an instance was created, should a mapping be created for it?
)
{
    LE_DEBUG("Getting object 9 instance for application '%s'.", appNamePtr);

    // Attempt to read the mapping from the configuration.
    assetData_InstanceDataRef_t instanceRef = NULL;
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateReadTxn(CFG_OBJECT_INFO_PATH);

    le_cfg_GoToNode(iterRef, appNamePtr);
    int instanceId = le_cfg_GetInt(iterRef, "oiid", -1);
    le_cfg_CancelTxn(iterRef);

    if (instanceId != -1)
    {
        LE_DEBUG("Was mapped to instance, %d.", instanceId);

        // Looks like there was a mapping. Try to get that instance and make sure it's not taken
        // by another application. If the instance was taken by another application, remap this
        // application to a new instance and update the mapping.
        if (assetData_GetInstanceRefById(LWM2M_NAME, LWM2M_OBJ9, instanceId, &instanceRef) == LE_OK)
        {
            char newName[MAX_APP_NAME_BYTES] = "";
            LE_ASSERT_OK(assetData_client_GetString(instanceRef,
                                                 O9F_PKG_NAME,
                                                 newName,
                                                 sizeof(newName)));

            if (strcmp(newName, appNamePtr) != 0)
            {
                LE_INFO("Instance has been taken by '%s', creating new.", newName);

                LE_ASSERT_OK(assetData_CreateInstanceById(LWM2M_NAME,
                                                          LWM2M_OBJ9,
                                                          -1,
                                                          &instanceRef));
                LE_ASSERT_OK(assetData_client_SetString(instanceRef, O9F_PKG_NAME, appNamePtr));

                if (mapIfNotFound)
                {
                    LE_INFO("Recording new instance id.");
                    SetObject9InstanceForApp(appNamePtr, instanceRef);
                }
            }
            else
            {
                LE_INFO("Instance exists and has been reused.");
            }
        }
        else
        {
            LE_INFO("No instance found, creating new as mapped.");

            LE_ASSERT_OK(assetData_CreateInstanceById(LWM2M_NAME,
                                                   LWM2M_OBJ9,
                                                   instanceId,
                                                   &instanceRef));

            LE_ASSERT_OK(assetData_client_SetString(instanceRef, O9F_PKG_NAME, appNamePtr));
        }
    }
    else
    {
        LE_INFO("No instance mapping found, creating new.");

        // A mapping was not found. So create a new object, and let the data store assign an
        // instance Id. If desired, at this point record the instance mapping for later use.
        LE_ASSERT_OK(assetData_CreateInstanceById(LWM2M_NAME, LWM2M_OBJ9, -1, &instanceRef));
        LE_ASSERT_OK(assetData_client_SetString(instanceRef, O9F_PKG_NAME, appNamePtr));

        if (mapIfNotFound)
        {
            LE_INFO("Recording new instance id.");
            SetObject9InstanceForApp(appNamePtr, instanceRef);
        }
    }
    return instanceRef;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Create instances of object 9 and the Legato objects for all currently installed applications.
 *
 */
//--------------------------------------------------------------------------------------------------
static void NotifyAppObjLists
(
    void
)
{
    char obj9List[MAX_OBJ9_STR_LIST_BYTES] = "";
    size_t obj9ListLen = 0;
    int numObjInstances = 0;
    le_result_t result;

    result = assetData_GetObj9InstanceList(&obj9List,
                                           sizeof(obj9List),
                                           &obj9ListLen,
                                           &numObjInstances);

    if (result != LE_OK)
    {
        LE_ERROR("Error retrieving object 9 list");
        return;
    }

    LE_INFO("Found %d object 9 instances", numObjInstances);
    LE_INFO("obj9ListLen; %zd obj9List: %s", obj9ListLen, obj9List);

    avcClient_SendList(obj9List, obj9ListLen);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Create instances of object 9 and the Legato objects for all currently installed applications.
 */
//--------------------------------------------------------------------------------------------------
static void PopulateAppInfoObjects
(
    void
)
{
    appCfg_Iter_t appIterRef = appCfg_CreateAppsIter();
    char appName[MAX_APP_NAME_BYTES] = "";
    char versionBuffer[MAX_VERSION_STR_BYTES] = "";
    le_result_t result;

    int foundAppCount = 0;

    result = appCfg_GetNextItem(appIterRef);

    while (result == LE_OK)
    {
        result = appCfg_GetAppName(appIterRef, appName, sizeof(appName));

        if (   (result == LE_OK)
            && (false == IsHiddenApp(appName)))
        {
            LE_DEBUG("Loading object instance for app, '%s'.", appName);

            assetData_InstanceDataRef_t instanceRef = GetObject9InstanceForApp(appName, false);

            if (appCfg_GetVersion(appIterRef, versionBuffer, sizeof(versionBuffer)) == LE_OVERFLOW)
            {
                LE_WARN("Warning, app, '%s' version string truncated to '%s'.",
                        appName,
                        versionBuffer);
            }

            if (0 == strlen(versionBuffer))
            {
                le_utf8_Copy(versionBuffer, VERSION_UNKNOWN, sizeof(versionBuffer), NULL);
            }

            assetData_client_SetString(instanceRef, O9F_PKG_VERSION, versionBuffer);

            assetData_client_SetBool(instanceRef, O9F_UPDATE_SUPPORTED_OBJECTS, false);

            // No need to save the status in config tree, while populating object9
            SetObj9State(instanceRef,
                         LWM2MCORE_SW_UPDATE_STATE_INSTALLED,
                         LWM2MCORE_SW_UPDATE_RESULT_INSTALLED);

            foundAppCount++;
        }
        else
        {
            LE_WARN("Application name too large or is hidden, '%s.'", appName);
        }

        result = appCfg_GetNextItem(appIterRef);
    }

    appCfg_DeleteIter(appIterRef);
    LE_FATAL_IF(result != LE_NOT_FOUND,
                "Application cache initialization, unexpected error returned, (%d): \"%s\"",
                result,
                LE_RESULT_TXT(result));

    int index = 0;

    LE_INFO("Found %d app.", foundAppCount);

    // Now cleanup the lwm2m/objectMap config tree
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateWriteTxn(CFG_OBJECT_PATH);
    le_cfg_DeleteNode(iterRef, CFG_OBJECT_MAP);
    le_cfg_CommitTxn(iterRef);

    while (foundAppCount > 0)
    {
        assetData_InstanceDataRef_t instanceRef = NULL;
        le_result_t result = assetData_GetInstanceRefById(LWM2M_NAME,
                                                          LWM2M_OBJ9,
                                                          index,
                                                          &instanceRef);
        LE_DEBUG("Index %d.", index);

        if (result == LE_OK)
        {
            assetData_client_GetString(instanceRef, O9F_PKG_NAME, appName, sizeof(appName));

            LE_DEBUG("Mapping app '%s'.", appName);

            SetObject9InstanceForApp(appName, instanceRef);
            foundAppCount--;
        }

        index++;
    }

    // Notify lwm2mcore the list of app objects
    NotifyAppObjLists();
}

//--------------------------------------------------------------------------------------------------
/**
 *  Notification handler that's called when an application is installed.
 */
//--------------------------------------------------------------------------------------------------
static void AppInstallHandler
(
    const char* appNamePtr,  ///< Name of the new application.
    void* contextPtr         ///< Registered context for this callback.
)

{
    if (NULL == appNamePtr)
    {
        return;
    }

    LE_INFO("Application, '%s,' has been installed.", appNamePtr);

    if (true == IsHiddenApp(appNamePtr))
    {
        LE_INFO("Application is hidden.");
        return;
    }

    assetData_InstanceDataRef_t instanceRef = NULL;

    LE_DEBUG("AvmsInstall: %d, CurrentObj9: %p", AvmsInstall, CurrentObj9);

    // If the install was initiated from AVMS use the existing object9 instance.
    if (true == AvmsInstall)
    {
        AvmsInstall = false;

        if (CurrentObj9 != NULL)
        {
            instanceRef = CurrentObj9;
            CurrentObj9 = NULL;

            // Use the current instance and check if the object instance exists
            LE_INFO("AVMS install, use existing object9 instance.");
            LE_ASSERT_OK(assetData_client_SetString(instanceRef, O9F_PKG_NAME, appNamePtr));
            SetObject9InstanceForApp(appNamePtr, instanceRef);
        }
        else
        {
            LE_ASSERT("Valid Object9 instance expected for AVMS install.");
        }
    }
    else
    {
        // Otherwise, create one for this application that was installed outside of LWM2M.
        LE_INFO("Local install, create new object9 instance.");
        instanceRef = GetObject9InstanceForApp(appNamePtr, true);
    }

    // Mark the application as being installed.
    SetObj9State(instanceRef,
                 LWM2MCORE_SW_UPDATE_STATE_INSTALLED,
                 LWM2MCORE_SW_UPDATE_RESULT_INSTALLED);

    // Update the application's version string.
    appCfg_Iter_t appIterRef = appCfg_FindApp(appNamePtr);
    char versionBuffer[MAX_VERSION_STR_BYTES] = "";

    if (appCfg_GetVersion(appIterRef, versionBuffer, sizeof(versionBuffer)) == LE_OVERFLOW)
    {
        LE_WARN("Warning, app, '%s' version string truncated to '%s'.",
                appNamePtr,
                versionBuffer);
    }

    if (0 == strlen(versionBuffer))
    {
        le_utf8_Copy(versionBuffer, VERSION_UNKNOWN, sizeof(versionBuffer), NULL);
    }

    assetData_client_SetString(instanceRef, O9F_PKG_VERSION, versionBuffer);

    appCfg_DeleteIter(appIterRef);

    // Notify lwm2mcore that an app is installed
    NotifyAppObjLists();
}

//--------------------------------------------------------------------------------------------------
/**
 *  Handler that's called when an application is uninstalled.
 */
//--------------------------------------------------------------------------------------------------
static void AppUninstallHandler
(
    const char* appNamePtr,  ///< App being removed.
    void* contextPtr         ///< Context for this function.
)
{
    if (NULL == appNamePtr)
    {
        return;
    }

    LE_INFO("Application, '%s,' has been uninstalled.", appNamePtr);

    if (true == IsHiddenApp(appNamePtr))
    {
        LE_INFO("Application is hidden.");
        return;
    }

    // For local uninstall, check for an instance of object 9 for this
    // application and delete that instance if found.
    if (true == AvmsInstall)
    {
        LE_INFO("Reuse object9 instance for upgrades.");
    }
    else if (CurrentObj9 != NULL)
    {
        LE_DEBUG("LWM2M Uninstall of instanceRef: %p.", CurrentObj9);

        assetData_DeleteInstance(CurrentObj9);
        // State already set to initial in PrepareUninstall
        CurrentObj9 = NULL;

        // If it is not hidden/system app, remove it from lwm2m config tree
        if (false == IsHiddenApp(appNamePtr))
        {
            LE_DEBUG("Deleting '%s' instance from cfgTree: %s", appNamePtr, CFG_OBJECT_INFO_PATH);
            SetObject9InstanceForApp(appNamePtr, NULL);
        }
    }
    else
    {
        LE_INFO("Local Uninstall of application.");

        assetData_InstanceDataRef_t objectRef = GetObject9InstanceForApp(appNamePtr, false);

        if (objectRef != NULL)
        {
            assetData_DeleteInstance(objectRef);
            // If it is in assetData, then no need to check config tree.
            LE_DEBUG("Deleting '%s' instance from cfgTree: %s", appNamePtr, CFG_OBJECT_INFO_PATH);
            SetObject9InstanceForApp(appNamePtr, NULL);
        }
    }

    // Notify lwm2mcore that an app is uninstalled
    NotifyAppObjLists();
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to get appName and instance reference.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetAppNameAndInstanceRef
(
    uint16_t instanceId,                            ///< [IN] Instance id of the app
    assetData_InstanceDataRef_t* instanceRefPtr,    ///< [OUT] Corresponding instance reference of
                                                    ///<       provided instance id.
    char* appNamePtr,                               ///< [OUT] Buffer to store appName
    size_t len                                      ///< [IN] Size of the buffer to store appName.
)
{

    le_result_t result = assetData_GetInstanceRefById(LWM2M_NAME,
                                                      LWM2M_OBJ9,
                                                      instanceId,
                                                      instanceRefPtr);
    if (result != LE_OK)
    {
        LE_ERROR("Error: '%s' while getting instanceRef for instance: %d",
                 LE_RESULT_TXT(result),
                 instanceId);

        return result;
    }

    LE_DEBUG("instanceRef: %p, *instanceRef: %p", instanceRefPtr, *instanceRefPtr);

    result = assetData_client_GetString(*instanceRefPtr,
                                        O9F_PKG_NAME,
                                        appNamePtr,
                                        len);

    if (result != LE_OK)
    {
        LE_ERROR("Error: '%s' while getting appName for instance: %d",
                 LE_RESULT_TXT(result),
                 instanceId);

        return result;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function called to kick off an application uninstall.
 *
 * @return
 *     - LE_OK if successful
 *     - LE_BUSY if system busy.
 *     - LE_NOT_FOUND if given app is not installed.
 *     - LE_FAULT for any other failure.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t StartUninstall
(
    const char* appNamePtr    ///< [IN] App to be uninstalled.
)
{
    LE_DEBUG("Application '%s' uninstall requested", appNamePtr);

    le_result_t result = le_appRemove_Remove(appNamePtr);

    if (result == LE_OK)
    {
        LE_DEBUG("Uninstall of application completed.");
    }
    else
    {
        LE_ERROR("Uninstall of application failed (%s).", LE_RESULT_TXT(result));
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 *  Called during an application install.
 */
//--------------------------------------------------------------------------------------------------
static void UpdateProgressHandler
(
  le_update_State_t updateState,  ///< State of the update in question.
  uint percentDone,               ///< How much work has been done.
  void* contextPtr                ///< Context for the callback.
)
{
    le_avc_ErrorCode_t avcErrorCode = LE_AVC_ERR_NONE;
    le_result_t result;

    switch (updateState)
    {
        case LE_UPDATE_STATE_UNPACKING:
            LE_INFO("Unpacking package, percentDone: %d.", percentDone);
            break;

        case LE_UPDATE_STATE_DOWNLOAD_SUCCESS:
             SetObj9State(CurrentObj9,
                          LWM2MCORE_SW_UPDATE_STATE_DELIVERED,
                          LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED);
            LE_INFO("Package delivered");
            break;

        case LE_UPDATE_STATE_APPLYING:
            avcServer_UpdateHandler(LE_AVC_INSTALL_IN_PROGRESS,
                                    LE_AVC_APPLICATION_UPDATE,
                                    -1,
                                    percentDone,
                                    LE_AVC_ERR_NONE);

            LE_INFO("Doing update.");
            break;

        case LE_UPDATE_STATE_SUCCESS:
            avcServer_UpdateHandler(LE_AVC_INSTALL_COMPLETE,
                                    LE_AVC_APPLICATION_UPDATE,
                                    -1,
                                    percentDone,
                                    LE_AVC_ERR_NONE);

            LE_INFO("Install completed.");
            le_update_End();
            break;

        case LE_UPDATE_STATE_FAILED:
            LE_DEBUG("Install/uninstall failed.");

            // Get the error code.
            switch (le_update_GetErrorCode())
            {
                case LE_UPDATE_ERR_SECURITY_FAILURE:
                    avcErrorCode = LE_AVC_ERR_SECURITY_FAILURE;
                    break;

                case LE_UPDATE_ERR_BAD_PACKAGE:
                    avcErrorCode = LE_AVC_ERR_BAD_PACKAGE;
                    break;

                case LE_UPDATE_ERR_INTERNAL_ERROR:
                    avcErrorCode = LE_AVC_ERR_INTERNAL;
                    break;

                default:
                    LE_ERROR("Should have an error code in failed state.");
                    break;
            }

            // Notify registered control app
            avcServer_UpdateHandler(LE_AVC_INSTALL_FAILED,
                                    LE_AVC_APPLICATION_UPDATE,
                                    -1,
                                    percentDone,
                                    avcErrorCode);

            SetObj9State(CurrentObj9,
                         LWM2MCORE_SW_UPDATE_STATE_INITIAL,
                         LWM2MCORE_SW_UPDATE_RESULT_INSTALL_FAILURE);
            le_update_End();
            CurrentObj9 = NULL;
            UpdateStarted = false;
        break;

        default:
            LE_ERROR("Bad state: %d\n", updateState);
            break;
     }
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop storing the download package.
 */
//--------------------------------------------------------------------------------------------------
static void StopStoringPackage
(
    le_result_t result      ///< [IN] Result of store operation.
)
{
    if (StoreFdMonitor != NULL)
    {
        LE_DEBUG("Delete Store Fd Monitor");
        le_fdMonitor_Delete(StoreFdMonitor);
        StoreFdMonitor = NULL;
    }

    if (UpdateReadFd != -1)
    {
        LE_DEBUG("Close downloader read pipe.");
        fd_Close(UpdateReadFd);
        UpdateReadFd = -1;
    }

    if (UpdateStoreFd != -1)
    {
        LE_DEBUG("Close store pipe.");
        fd_Close(UpdateStoreFd);
        UpdateStoreFd = -1;
    }

    if (result == LE_OK)
    {
        SetObj9State(CurrentObj9,
                     LWM2MCORE_SW_UPDATE_STATE_DOWNLOADED,
                     LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED);
        LE_INFO("Download successful");
    }
    else
    {
        SetObj9State(CurrentObj9,
                     LWM2MCORE_SW_UPDATE_STATE_INITIAL,
                     LWM2MCORE_SW_UPDATE_RESULT_INSTALL_FAILURE);
        LE_INFO("Download Failed");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Copy the downloaded bytes to Fd
 */
//--------------------------------------------------------------------------------------------------
static void WriteBytesToFd
(
    int fd,                 ///< [IN] FD to write to
    uint8_t *bufferPtr,     ///< [IN] Buffer pointer to Downloaded bytes
    size_t readCount        ///< [IN] Number of bytes downloaded
)
{
    ssize_t bytesWritten;
    ssize_t writeResult;

    bytesWritten = 0;

    // Write the bytes that we read.
    do
    {
        writeResult = write(UpdateStoreFd,
                            bufferPtr + bytesWritten,
                            readCount - bytesWritten);

        // If some bytes were written, remember how many bytes, so we don't
        // try to write the same bytes again if we have more to write.
        if (writeResult > 0)
        {
            bytesWritten += writeResult;
        }
    }
    // Retry if interrupted by a signal; continue if not done
    while (   ((-1 == writeResult) && (EINTR == errno))
           || ((writeResult != -1) && (bytesWritten < readCount))  );

    // Check for errors.
    if (writeResult == -1)
    {
        LE_ERROR("Failed to write bytes to fd: bytesWritten %d", bytesWritten);
        StopStoringPackage(LE_FAULT);
    }
    else
    {
        TotalCount += bytesWritten;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Event handler for the input fd when storing the bytes to disk.
 */
//--------------------------------------------------------------------------------------------------
static void StoreFdEventHandler
(
    int fd,                 ///< [IN] Read file descriptor
    short events            ///< [IN] FD events
)
{
    uint8_t buffer[DWL_STORE_BUF_SIZE];
    ssize_t readCount;

    if (events & POLLHUP)
    {
        // file descriptor has been closed
        LE_DEBUG("file descriptor %d has been closed", fd);

        // Store the remaining bytes to disk.
        while(1)
        {
            readCount = read (fd, buffer,sizeof(buffer));
            LE_DEBUG("read %d bytes", readCount);

            // Write the bytes read to disk.
            if (readCount > 0)
            {
                WriteBytesToFd(UpdateStoreFd, buffer, readCount);
            }
            else if (readCount < 0)
            {
                LE_ERROR("error reading fd");
                StopStoringPackage(LE_OK);
            }
            else
            {
                LE_DEBUG("Finished writing; close store fd");
                LE_INFO("%d bytes stored", TotalCount);
                StopStoringPackage(LE_OK);

                LE_DEBUG("Start unpacking.");
                avcApp_StartUpdate();
                break;
            }
        }
    }
    else if (events & EPOLLIN)
    {
        readCount = read (fd, buffer,sizeof(buffer));
        LE_DEBUG("read %d bytes", readCount);

        // Write the bytes read to disk.
        if (readCount > 0)
        {
            WriteBytesToFd(UpdateStoreFd, buffer, readCount);
        }
    }
    else
    {
        LE_WARN("unexpected event received 0x%x",
                events & ~(POLLHUP|POLLIN));
        StopStoringPackage(LE_FAULT);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Prepare the app download directory (delete any old one and create a fresh empty one).
 */
//--------------------------------------------------------------------------------------------------
void PrepareDownloadDirectory
(
    char *downloadPathPtr
)
{
    // Clear out the current unpack dir, if it exists, then make sure it exists.
    LE_FATAL_IF(le_dir_RemoveRecursive(downloadPathPtr) != LE_OK,
                "Failed to recursively delete '%s'.",
                downloadPathPtr);
    LE_FATAL_IF(LE_OK != le_dir_MakePath(downloadPathPtr, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH),
                "Failed to create directory '%s'.",
                downloadPathPtr);
}

//-------------------------------------------------------------------------------------------------
/**
 * Stores update file to temporary location.
 *
 * @return
 *      - LE_OK if accepted
 *      - LE_UNSUPPORTED if read only file system
 *      - LE_FAULT otherwise
 */
//-------------------------------------------------------------------------------------------------
static le_result_t StartStoringPackage
(
    int clientFd,   ///<[IN] Open file descriptor from which the update can be read.
    bool isResume   ///<[IN] Resume the previously interrupted operation.
)
{
    char downloadFile[MAX_FILE_PATH_BYTES];
    le_result_t result;
    size_t offset = 0;

    // Make sure legato is NOT a read only system
    if ((0 == access("/mnt/legato/systems/current/read-only", R_OK)))
    {
        LE_ERROR("Legato is R/O");
        return LE_UNSUPPORTED;
    }

    // The name of temporary file where the package downloaded will be stored.
    le_utf8_Copy(downloadFile, AppDownloadPath, sizeof(downloadFile), NULL);
    le_utf8_Append(downloadFile, NAME_DOWNLOAD_FILE, sizeof(downloadFile), NULL);

    LE_INFO("Store update file at %s", downloadFile);

    if (isResume)
    {
        if (false == file_Exists(downloadFile))
        {
            LE_ERROR("update file doesn't exist");
            return LE_FAULT;
        }

        // Open existing download file
        UpdateStoreFd = open(downloadFile, O_WRONLY, 0);
        if (UpdateStoreFd == -1)
        {
            LE_ERROR("Unable to open file '%s' for writing (%m).", downloadFile);
            return LE_FAULT;
        }

        // Read the resume offset from the workspace
        result = GetSwUpdateBytesDownloaded(&offset);

        if(result != LE_OK)
        {
            LE_ERROR("Can't read download offset");
            return LE_FAULT;
        }

        // Seek to the resume offset
        LE_DEBUG("Seek to offset %d", offset);
        size_t fileOffset = lseek(UpdateStoreFd, offset, SEEK_SET);

        if (fileOffset == -1)
        {
            LE_ERROR("Seek file to offset %d failed.", offset);
            fd_Close(UpdateStoreFd);
            return LE_FAULT;
        }
    }
    else
    {
        // Make a directory
        PrepareDownloadDirectory(AppDownloadPath);

        // Create new download file
        UpdateStoreFd = open(downloadFile, O_WRONLY | O_TRUNC | O_CREAT, 0);
        if (UpdateStoreFd == -1)
        {
            LE_ERROR("Unable to open file '%s' for writing (%m).", downloadFile);
            return LE_FAULT;
        }
    }

    // Init total count
    TotalCount = 0;

    // Set fd as non blocking
    fd_SetNonBlocking(clientFd);

    // Create FD monitor for the input FD
    UpdateReadFd = clientFd;
    StoreFdMonitor = le_fdMonitor_Create("store", UpdateReadFd,
                                         StoreFdEventHandler, (POLLIN | POLLHUP));

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 *  Handler to start download.
 */
//--------------------------------------------------------------------------------------------------
static void DownloadHandler
(
     void* contextPtr
)
{
    lwm2mcore_PackageDownloader_t* pkgDwlPtr;
    packageDownloader_DownloadCtx_t* dwlCtxPtr;
    le_result_t result;
    int fd;

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t*)contextPtr;
    dwlCtxPtr = pkgDwlPtr->ctxPtr;

    LE_DEBUG("contextPtr: %p", pkgDwlPtr);

    fd = open(dwlCtxPtr->fifoPtr, O_RDONLY, 0);
    LE_DEBUG("Opened fifo");

    if (-1 == fd)
    {
        LE_ERROR("failed to open fifo %m");
        return;
    }

    LE_DEBUG("Start storing the downloaded package.");
    result = StartStoringPackage(fd, dwlCtxPtr->resume);

    if (LE_OK != result)
    {
        LE_ERROR("Failed to store download package %s", LE_RESULT_TXT(result));

        // Set the CurrentObj9 status to failure
        SetObj9State(CurrentObj9,
                     LWM2MCORE_SW_UPDATE_STATE_INITIAL,
                     LWM2MCORE_SW_UPDATE_RESULT_INSTALL_FAILURE);
        CurrentObj9 = NULL;

        StopStoringPackage(LE_FAULT);
        return;
    }

    UpdateStarted = true;
}

//--------------------------------------------------------------------------------------------------
// Public functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Function called to kick off the install of a Legato application.
 *
 * @return
 *      - LE_OK if installation started.
 *      - LE_BUSY if install is not finished yet.
 *      - LE_FAULT if there is an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StartInstall
(
    uint16_t instanceId                 ///< [IN] Instance id of the app to be installed.
)
{
    LE_DEBUG("Install application using AirVantage, instanceID: %d.", instanceId);

    assetData_InstanceDataRef_t instanceRef = NULL;

    // Now get entry from assetData by specifying instanceId
    le_result_t result = assetData_GetInstanceRefById(LWM2M_NAME,
                                                      LWM2M_OBJ9,
                                                      instanceId,
                                                      &instanceRef);
    if (result != LE_OK)
    {
        LE_ERROR("Error in retrieving assetData for instance: %d (%s)",
                  instanceId,
                  LE_RESULT_TXT(result));

        return LE_FAULT;
    }

    if(CurrentObj9 != instanceRef)
    {
        LE_ERROR("Internal error. Object reference mismatch. CurrentObj9 = %p, instanceRef = %p",
                 CurrentObj9,
                 instanceRef);
        return LE_FAULT;
    }

    result = le_update_Install();

    if (result == LE_OK)
    {
        AvmsInstall = true;
    }
    else
    {
        LE_ERROR("Could not start update.");
        SetObj9State(CurrentObj9,
                     LWM2MCORE_SW_UPDATE_STATE_INITIAL,
                     LWM2MCORE_SW_UPDATE_RESULT_INSTALL_FAILURE);
        CurrentObj9 = NULL;
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function called to unpack the downloaded package
 *
 * @return
 *      - LE_OK if installation started.
 *      - LE_UNSUPPORTED if not supported
 *      - LE_FAULT if there is an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StartUpdate
(
    void
)
{
    le_result_t result;
    uint16_t instanceId = -1;
    assetData_GetInstanceId(CurrentObj9, &instanceId);
    LE_DEBUG("unpack object instance %d", instanceId);

    char downloadFile[MAX_FILE_PATH_BYTES];

    if ((0 == access("/mnt/legato/systems/current/read-only", R_OK)))
    {
        LE_ERROR("Legato is R/O");
        return LE_UNSUPPORTED;
    }

    // Check if the downloaded package exists.
    le_utf8_Copy(downloadFile, AppDownloadPath, sizeof(downloadFile), NULL);
    le_utf8_Append(downloadFile, NAME_DOWNLOAD_FILE, sizeof(downloadFile), NULL);

    LE_INFO("Read update file from %s", downloadFile);

    if (false == file_Exists(downloadFile))
    {
        LE_ERROR("update file doesn't exist");
        return LE_FAULT;
    }

    // Open the downloaded package file.
    int readFd = open(downloadFile, O_RDONLY, 0);

    if (readFd == -1)
    {
       LE_ERROR("Unable to open file '%s' for reading (%m).", downloadFile);
       return LE_FAULT;
    }

    // Start unpacking the downloaded file.
    result = le_update_Start(readFd);

    if (result != LE_OK)
    {
        LE_ERROR("Unable to start update");
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Function called to prepare for an application uninstall. This function doesn't remove the app
 *  but deletes only the app objects, so that an existing app can stay running during an upgrade
 *  operation. During an uninstall operation the app will be removed after the client receives the
 *  object9 delete command.
 *
 *  @return
 *      - LE_OK if successful
 *      - LE_NOT_FOUND if instanceId/appName not found
 *      - LE_FAULT if there is any other error.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_PrepareUninstall
(
    uint16_t instanceId     ///< [IN] Instance id of the app to be removed.
)
{
    assetData_InstanceDataRef_t instanceRef;
    char appName[MAX_APP_NAME_BYTES] = "";

    le_result_t result = GetAppNameAndInstanceRef(instanceId,
                                                  &instanceRef,
                                                  appName,
                                                  sizeof(appName));

    if (result != LE_OK)
    {
        return result;
    }

    LE_DEBUG("Application '%s' uninstall requested, instanceID: %d", appName, instanceId);

    // Just set the state of this object 9 to initial.
    // The server queries for this state and sends us object9 delete, which will kick an uninstall.
    SetObj9State(instanceRef,
                 LWM2MCORE_SW_UPDATE_STATE_INITIAL,
                 LWM2MCORE_SW_UPDATE_RESULT_INITIAL);

    CurrentObj9 = instanceRef;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Start up the requested legato application.
 *
 *  @return
 *       - LE_OK if start request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_UNAVAILABLE if specified app isn't installed
 *       - LE_DUPLICATE if specified app is already running
 *       - LE_FAULT if there is any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StartApp
(
    uint16_t instanceId   ///< [IN] Instance id of object 9 for this app.
)
{
    assetData_InstanceDataRef_t instanceRef;
    char appName[MAX_APP_NAME_BYTES] = "";

    le_result_t result = GetAppNameAndInstanceRef(instanceId,
                                                  &instanceRef,
                                                  appName,
                                                  sizeof(appName));

    if (result != LE_OK)
    {
        return result;
    }

    LE_DEBUG("Application '%s' start requested, instanceID: %d, instanceRef: %p",
              appName,
              instanceId,
              instanceRef);

    if (GetObj9State(instanceRef) != LWM2MCORE_SW_UPDATE_STATE_INSTALLED)
    {
        LE_ERROR("Application '%s' not installed.", appName);
        return LE_UNAVAILABLE;
    }

    result = le_appCtrl_Start(appName);

    if (result == LE_DUPLICATE)
    {
        LE_DEBUG("Application %s is already running, ignoring LE_DUPLICATE", appName);
        // App is already running, so return LE_OK.
        result = LE_OK;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Stop a Legato application.
 *
 *  @return
 *       - LE_OK if stop request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_UNAVAILABLE if specified app isn't installed
 *       - LE_FAULT if there is any other error.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StopApp
(
    uint16_t instanceId   ///< [IN] Instance id of object 9 for this app.
)
{
    assetData_InstanceDataRef_t instanceRef;
    char appName[MAX_APP_NAME_BYTES] = "";

    le_result_t result = GetAppNameAndInstanceRef(instanceId,
                                                  &instanceRef,
                                                  appName,
                                                  sizeof(appName));

    if (result != LE_OK)
    {
        return result;
    }

    LE_DEBUG("Application '%s' stop requested.", appName);

    if (GetObj9State(instanceRef) != LWM2MCORE_SW_UPDATE_STATE_INSTALLED)
    {
        LE_ERROR("Application '%s' not installed.", appName);
        return LE_UNAVAILABLE;
    }

    return le_appCtrl_Stop(appName);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Function to get application name.
 *
 *  @return
 *       - LE_OK if stop request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_FAULT if there is any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetPackageName
(
    uint16_t instanceId,    ///< [IN] Instance id of object 9 for this app.
    char* appNamePtr,       ///< [OUT] Buffer to store appName
    size_t len              ///< [IN] Size of the buffer to store appName
)
{
    if (NULL == appNamePtr)
    {
        return LE_FAULT;
    }
    assetData_InstanceDataRef_t instanceRef;

    le_result_t result = GetAppNameAndInstanceRef(instanceId, &instanceRef, appNamePtr, len);

    if (result != LE_OK)
    {
        return result;
    }

    LE_DEBUG("Application Name: '%s', instanceId: %d.", appNamePtr, instanceId);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Function to get package name (application name).
 *
 *  @return
 *       - LE_OK if stop request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_FAULT if there is any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetPackageVersion
(
    uint16_t instanceId,    ///< [IN] Instance id of object 9 for this app.
    char* versionPtr,       ///< [OUT] Buffer to store version
    size_t len              ///< [IN] Size of the buffer to store version
)
{
    if (NULL == versionPtr)
    {
        return LE_FAULT;
    }
    assetData_InstanceDataRef_t instanceRef;

    le_result_t result = assetData_GetInstanceRefById(LWM2M_NAME,
                                                      LWM2M_OBJ9,
                                                      instanceId,
                                                      &instanceRef);

    if (result != LE_OK)
    {
       LE_ERROR("Error: '%s' while getting instanceRef for instance: %d",
                 LE_RESULT_TXT(result),
                 instanceId);

       return result;
    }

    result = assetData_client_GetString(instanceRef,
                                        O9F_PKG_VERSION,
                                        versionPtr,
                                        len);

    if (result != LE_OK)
    {
       LE_ERROR("Error: '%s' while getting package version for instance: %d",
                LE_RESULT_TXT(result),
                instanceId);

       return result;
    }

    LE_DEBUG("App version: '%s', instanceId: %d.", versionPtr, instanceId);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Function to get application activation status.
 *
 *  @return
 *       - LE_OK if stop request is sent successfully
 *       - LE_NOT_FOUND if specified object 9 instance isn't found
 *       - LE_FAULT if there is any other error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetActivationState
(
    uint16_t instanceId,    ///< [IN] Instance id of object 9 for this app.
    bool *valuePtr          ///< [OUT] Activation status
)
{
    if (NULL == valuePtr)
    {
        return LE_FAULT;
    }
    assetData_InstanceDataRef_t instanceRef;
    char appName[MAX_APP_NAME_BYTES] = "";

    le_result_t result = GetAppNameAndInstanceRef(instanceId,
                                                  &instanceRef,
                                                  appName,
                                                  sizeof(appName));

    if (result != LE_OK)
    {
        return result;
    }

    LE_DEBUG("Application '%s' activation status requested.", appName);

    if (0 == strcmp(appName, ""))
    {
        LE_INFO("Appname is empty, sending default value 'false'");
        *valuePtr = false;
    }
    else
    {
        le_appInfo_State_t state = le_appInfo_GetState(appName);

        *valuePtr = (state == LE_APPINFO_RUNNING);

        LE_DEBUG("App: %s activationState: %d", appName, *valuePtr);
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create an object 9 instance
 *
 * @return:
 *      - LE_OK on success
 *      - LE_DUPLICATE if already exists an instance
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_CreateObj9Instance
(
    uint16_t instanceId            ///< [IN] object 9 instance id
)
{
    LE_DEBUG("Requested to create instance: %d", instanceId);
    assetData_InstanceDataRef_t instanceRef = NULL;
    // Now create an entry into assetData by specifying instanceId
    le_result_t result = assetData_CreateInstanceById(LWM2M_NAME,
                                                      LWM2M_OBJ9,
                                                      instanceId,
                                                      &instanceRef);

    if (result != LE_OK)
    {
        LE_ERROR("Failed to create instance: %d (%s)", instanceId, LE_RESULT_TXT(result));

        return result;
    }

    CurrentObj9 = instanceRef;

    LE_DEBUG("Initialize sw update workspace.");

    DeletePackage();

    SetSwUpdateInstanceId(instanceId);
    SetSwUpdateInternalState(INTERNAL_STATE_DOWNLOAD_REQUESTED);
    packageDownloader_SetSwUpdateState(LWM2MCORE_SW_UPDATE_STATE_INITIAL);
    packageDownloader_SetSwUpdateResult(LWM2MCORE_FW_UPDATE_RESULT_DEFAULT_NORMAL);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete an object 9 instance
 *
 * @return
 *     - LE_OK if successful
 *     - LE_BUSY if system busy.
 *     - LE_NOT_FOUND if given instance not found or given app is not installed.
 *     - LE_FAULT for any other failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_DeleteObj9Instance
(
    uint16_t instanceId            ///< [IN] object 9 instance id
)
{
    LE_DEBUG("Requested to Delete instance: %d", instanceId);
    assetData_InstanceDataRef_t instanceRef = NULL;
    char appName[MAX_APP_NAME_BYTES] = "";

    le_result_t result = assetData_GetInstanceRefById(LWM2M_NAME,
                                                      LWM2M_OBJ9,
                                                      instanceId,
                                                      &instanceRef);

    if (result != LE_OK)
    {
        LE_ERROR("Error in getting assetData for instance: %d (%s)",
                  instanceId,
                  LE_RESULT_TXT(result));

        return result;
    }

    result = assetData_client_GetString(instanceRef, O9F_PKG_NAME, appName, sizeof(appName));

    switch(result)
    {
        case LE_OK:
            if (0 == strcmp(appName, ""))
            {
                assetData_DeleteInstance(instanceRef);
                CurrentObj9 = NULL;
            }
            else
            {
                result = StartUninstall(appName);

                if (result == LE_OK)
                {
                    // Keep the instance reference so that it can be used when Uninstall callback
                    // called
                    CurrentObj9 = instanceRef;
                }
                else if (result == LE_NOT_FOUND)
                {
                    // App not installed. Just delete the instance from assetData
                    assetData_DeleteInstance(instanceRef);
                    CurrentObj9 = NULL;
                    result = LE_OK;
                }
                else
                {
                    // Something wrong
                    CurrentObj9 = NULL;
                }
            }
            break;

        default:
            LE_CRIT("Can't get mandatory field 'packageName' for obj9 instance: %d (%s)",
                      instanceId,
                      LE_RESULT_TXT(result));
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Store SW package function
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StoreSwPackage
(
    void *ctxPtr
)
{
    LE_INFO("Initiating Downloading update package");

    LE_DEBUG("contextPtr: %p", ctxPtr);

    le_event_Report(DownloadEventId, ctxPtr, sizeof(lwm2mcore_PackageDownloader_t));

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Return the offset of the downloaded package.
 *
 * @return
 *      - LE_OK on success
 *      - LE_BAD_PARAMETER Invalid parameter
 *      - LE_FAULT on failure
 */;
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetResumePosition
(
    size_t* positionPtr
)
{
    char downloadFile[MAX_FILE_PATH_BYTES];
    int storeFd;
    le_result_t result;

    *positionPtr = 0;

    // The name of temporary file where the package downloaded will be stored.
    le_utf8_Copy(downloadFile, AppDownloadPath, sizeof(downloadFile), NULL);
    le_utf8_Append(downloadFile, NAME_DOWNLOAD_FILE, sizeof(downloadFile), NULL);

    LE_DEBUG("Get the size of %s", downloadFile);

    if (false == file_Exists(downloadFile))
    {
        LE_WARN("update file doesn't exist, create one");

        PrepareDownloadDirectory(AppDownloadPath);

        storeFd = open(downloadFile, O_WRONLY | O_TRUNC | O_CREAT, 0);

        if (storeFd == -1)
        {
            LE_ERROR("Unable to open file '%s' for writing (%m).", downloadFile);
            return LE_FAULT;
        }

        close(storeFd);
    }
    else
    {
        int32_t offset;
        result = GetSwUpdateBytesDownloaded(&offset);

        if(result != LE_OK)
        {
            LE_ERROR("Can't read download offset");
            return LE_FAULT;
        }

        LE_INFO("Resuming from offset %zd", offset);
        *positionPtr = offset;
    }

    // Create a new object 9 instance for application resume.
    uint16_t instanceId;
    if (LE_OK == GetSwUpdateInstanceId(&instanceId))
    {
        LE_DEBUG("Restoring application update process.");
        if (assetData_GetInstanceRefById(LWM2M_NAME, LWM2M_OBJ9,
                                         instanceId, &CurrentObj9) == LE_NOT_FOUND)
        {
            LE_DEBUG("Create a new object 9 instance.");
            LE_ASSERT(assetData_CreateInstanceById(LWM2M_NAME,
                                    LWM2M_OBJ9, instanceId, &CurrentObj9) == LE_OK);

            // Notify lwm2mcore that a new instance is created.
            NotifyAppObjLists();
        }
        else
        {
            LE_DEBUG("Instance ID invalid.");
            return LE_FAULT;
        }
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software download result
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if instance not found
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t  avcApp_SetDownloadResult
(
    lwm2mcore_SwUpdateResult_t updateResult
)
{
    LE_DEBUG("Requested to set result: %d, instance: %p", updateResult, CurrentObj9);

    if (CurrentObj9 == NULL)
    {
        return DWL_FAULT;
    }

    switch (updateResult)
    {
        case LWM2MCORE_SW_UPDATE_RESULT_INITIAL:
            LE_DEBUG("Initial state");
            break;

        case LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADING:
            LE_DEBUG("Package Downloading");
            break;

        case LWM2MCORE_SW_UPDATE_RESULT_INSTALLED:
            LE_DEBUG("Package Installed");
            break;

        case LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED:
            LE_DEBUG("Package downloaded");
            break;

        default:
            LE_ERROR("Error status: %d", updateResult);
            if (UpdateStarted)
            {
                UpdateStarted = false;
                le_event_Report(UpdateEndEventId, NULL, 0);
            }
            break;

    }

    LE_ASSERT_OK(assetData_client_SetInt(CurrentObj9, O9F_UPDATE_RESULT, updateResult));
    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software download state
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if instance not found
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
lwm2mcore_DwlResult_t  avcApp_SetDownloadState
(
    lwm2mcore_SwUpdateState_t updateState
)
{
    LE_DEBUG("Requested to set state: %d, instance: %p", updateState, CurrentObj9);

    if (CurrentObj9 == NULL)
    {
        return DWL_FAULT;
    }

    LE_ASSERT_OK(assetData_client_SetInt(CurrentObj9, O9F_UPDATE_STATE, updateState));

    return DWL_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update result
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if instance not found
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetUpdateResult
(
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateResultPtr        ///< [OUT] Software update result
)
{
    if (NULL == updateResultPtr)
    {
        return LE_FAULT;
    }

    LE_DEBUG("Requested to get update result for instance id: %d", instanceId);
    // Use the assetData api to get the update result
    assetData_InstanceDataRef_t instanceRef;

    le_result_t result = assetData_GetInstanceRefById(LWM2M_NAME,
                                                      LWM2M_OBJ9,
                                                      instanceId,
                                                      &instanceRef);

    if (result != LE_OK)
    {
        return result;
    }

    int updateResult = 0;

    result = assetData_client_GetInt(instanceRef, O9F_UPDATE_RESULT, &updateResult);

    if (result != LE_OK)
    {
        LE_ERROR("Error in getting UpdateResult of instance: %d (%s)",
                  instanceId,
                  LE_RESULT_TXT(result));

        return result;
    }

    *updateResultPtr = (uint8_t)updateResult;

    LE_DEBUG("UpdateResult: %d, instance id: %d", updateResult, instanceId);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update state
 *
 * @return:
 *      - LE_OK on success
 *      - LE_NOT_FOUND if instance not found
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_GetUpdateState
(
    uint16_t instanceId,            ///< [IN] Instance Id (0 for FW, any value for SW)
    uint8_t* updateStatePtr         ///< [OUT] Software update state
)
{
    if (NULL == updateStatePtr)
    {
        return LE_FAULT;
    }

    // Use the assetData api to get the update result
    assetData_InstanceDataRef_t instanceRef;

    LE_DEBUG("Requested to get update state for instance id: %d", instanceId);

    le_result_t result = assetData_GetInstanceRefById(LWM2M_NAME,
                                                      LWM2M_OBJ9,
                                                      instanceId,
                                                      &instanceRef);

    if (result != LE_OK)
    {
        return result;
    }

    int updateState = 0;

    result = assetData_client_GetInt(instanceRef, O9F_UPDATE_STATE, &updateState);

    if (result != LE_OK)
    {
        LE_ERROR("Error in getting UpdateState of instance: %d (%s)",
                  instanceId,
                  LE_RESULT_TXT(result));

        return result;
    }

    *updateStatePtr = (uint8_t)updateState;
    LE_DEBUG("UpdateState: %d, instance id: %d", *updateStatePtr, instanceId);
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set software update instance id
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t SetSwUpdateInstanceId
(
    int instanceId     ///< [IN] SW update instance id
)
{
    le_result_t result;

    result = WriteFs(SW_UPDATE_INSTANCE_PATH,
                     (uint8_t *)&instanceId,
                     sizeof(int));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", SW_UPDATE_INSTANCE_PATH, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set software update bytes downloaded
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t SetSwUpdateBytesDownloaded
(
    void
)
{
    le_result_t result;

    LE_INFO("TotalCount = %zd", TotalCount);

    result = WriteFs(SW_UPDATE_BYTES_DOWNLOADED_PATH,
                     (uint8_t *)&TotalCount,
                     sizeof(size_t));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", SW_UPDATE_BYTES_DOWNLOADED_PATH, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get software update bytes downloaded
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t GetSwUpdateBytesDownloaded
(
    size_t* bytesDownloadedPtr     ///< [OUT] bytes downloaded
)
{
    size_t size;
    le_result_t result;
    size_t bytesDownloaded;

    if (!bytesDownloadedPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_FAULT;
    }

    size = sizeof(size_t);
    result = ReadFs(SW_UPDATE_BYTES_DOWNLOADED_PATH, (size_t*)&bytesDownloaded, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_ERROR("SW update instance id not found");
            *bytesDownloadedPtr = -1;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", SW_UPDATE_BYTES_DOWNLOADED_PATH, LE_RESULT_TXT(result));
        return result;
    }

    *bytesDownloadedPtr = bytesDownloaded;

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set software internal state
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t SetSwUpdateInternalState
(
    avcApp_InternalState_t internalState   ///< [IN] internal state
)
{
    le_result_t result;

    result = WriteFs(SW_UPDATE_INTERNAL_STATE_PATH,
                     (uint8_t *)&internalState,
                     sizeof(int));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", SW_UPDATE_INTERNAL_STATE_PATH, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set software update package size
 *
 * @return
 *  - LE_OK     The function succeeded
 *  - LE_FAULT  The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t SetSwUpdatePackageSize
(
    size_t pkgSize   ///< [IN] package size
)
{
    le_result_t result;

    result = WriteFs(SW_UPDATE_PKGSIZE_PATH,
                     (uint8_t *)&pkgSize,
                     sizeof(int));
    if (LE_OK != result)
    {
        LE_ERROR("Failed to write %s: %s", SW_UPDATE_PKGSIZE_PATH, LE_RESULT_TXT(result));
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update instance ID
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t GetSwUpdateInstanceId
(
    int* instanceIdPtr     ///< [OUT] instance id
)
{
    size_t size;
    le_result_t result;
    int instanceId;

    if (!instanceIdPtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_FAULT;
    }

    size = sizeof(lwm2mcore_SwUpdateState_t);
    result = ReadFs(SW_UPDATE_INSTANCE_PATH, (int*)&instanceId, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_ERROR("SW update instance id not found");
            *instanceIdPtr = -1;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", SW_UPDATE_INSTANCE_PATH, LE_RESULT_TXT(result));
        return result;
    }

    *instanceIdPtr = instanceId;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update internal state
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t GetSwUpdateInternalState
(
    avcApp_InternalState_t* internalStatePtr     ///< [OUT] internal state
)
{
    size_t size;
    le_result_t result;
    int internalState;

    if (!internalStatePtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_FAULT;
    }

    size = sizeof(int);
    result = ReadFs(SW_UPDATE_INTERNAL_STATE_PATH, (int*)&internalState, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_ERROR("SW update internal state not found");
            *internalStatePtr = LWM2MCORE_SW_UPDATE_STATE_INITIAL;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", SW_UPDATE_INTERNAL_STATE_PATH, LE_RESULT_TXT(result));
        return result;
    }

    switch (internalState)
    {
        case INTERNAL_STATE_DOWNLOAD_REQUESTED:
            *internalStatePtr = INTERNAL_STATE_DOWNLOAD_REQUESTED;
            break;

        case INTERNAL_STATE_INSTALL_REQUESTED:
            *internalStatePtr = INTERNAL_STATE_INSTALL_REQUESTED;
            break;

        case INTERNAL_STATE_UNINSTALL_REQUESTED:
            *internalStatePtr = INTERNAL_STATE_UNINSTALL_REQUESTED;
            break;

        default:
            *internalStatePtr = INTERNAL_STATE_INVALID;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get software update packageSize
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  Null pointer provided
 *  - LE_FAULT          The function failed
 */
//--------------------------------------------------------------------------------------------------
le_result_t GetSwUpdatePackageSize
(
    size_t* pkgSizePtr     ///< [OUT] package size
)
{
    size_t size;
    size_t pkgSize;
    le_result_t result;

    if (!pkgSizePtr)
    {
        LE_ERROR("Invalid input parameter");
        return LE_FAULT;
    }

    size = sizeof(size_t);
    result = ReadFs(SW_UPDATE_PKGSIZE_PATH, (int*)&pkgSize, &size);
    if (LE_OK != result)
    {
        if (LE_NOT_FOUND == result)
        {
            LE_ERROR("SW update package size not found");
            *pkgSizePtr = -1;
            return LE_OK;
        }
        LE_ERROR("Failed to read %s: %s", SW_UPDATE_PKGSIZE_PATH, LE_RESULT_TXT(result));
        return result;
    }

    *pkgSizePtr = pkgSize;

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 *  Launch install process
 */
//--------------------------------------------------------------------------------------------------
static void LaunchSwUpdate
(
    lwm2mcore_UpdateType_t updateType,
    uint16_t instanceId
)
{
    avcApp_StartInstall(instanceId);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Restore the state of the avc update process after a reboot or power cycle
 */
//--------------------------------------------------------------------------------------------------
static void RestoreAvcAppUpdateState
(
    void
)
{
    int instanceId = -1;
    char uri[MAX_FILE_PATH_BYTES];
    lwm2mcore_SwUpdateState_t restoreState;
    lwm2mcore_SwUpdateResult_t restoreResult;
    avcApp_InternalState_t internalState;
    size_t pkgSize = 0;
    assetData_InstanceDataRef_t instanceRef;
    le_result_t result;
    lwm2mcore_UpdateType_t updateType;

    uint8_t downloadUri[LWM2MCORE_PACKAGE_URI_MAX_LEN+1];
    size_t uriLen = LWM2MCORE_PACKAGE_URI_MAX_LEN+1;

    if ((LE_OK == packageDownloader_GetSwUpdateState(&restoreState))
        && (LE_OK == packageDownloader_GetSwUpdateResult(&restoreResult))
        && (LE_OK == GetSwUpdateInstanceId(&instanceId))
        && (LE_OK == GetSwUpdateInternalState(&internalState)))
    {
        LE_PRINT_VALUE("%i", instanceId);
        LE_PRINT_VALUE("%i", restoreState);
        LE_PRINT_VALUE("%i", restoreResult);
        LE_PRINT_VALUE("%p", uri);

        if (instanceId == -1)
        {
            LE_DEBUG("Instance ID invalid");
            return;
        }

        // Do not restore if already installed
        if ((restoreResult != LWM2MCORE_SW_UPDATE_STATE_INITIAL)
            && (restoreResult != LWM2MCORE_SW_UPDATE_RESULT_INSTALLED)
            && (restoreResult != LWM2MCORE_SW_UPDATE_RESULT_INSTALL_FAILURE))
        {
            LE_DEBUG("SW update not resumed: instanceId %d, result %d", instanceId, restoreResult);
            return;
        }

        if (assetData_GetInstanceRefById(LWM2M_NAME,
                                         LWM2M_OBJ9,
                                         instanceId,
                                         &instanceRef) == LE_OK)
        {
            LE_DEBUG("Object 9 instance exists.");
        }
        else
        {
            LE_DEBUG("Create a new object 9 instance.");
            LE_ASSERT_OK(assetData_CreateInstanceById(LWM2M_NAME,
                                         LWM2M_OBJ9, instanceId, &instanceRef));
        }

        // Restore the state of Object9
        SetObj9State(instanceRef, restoreState, restoreResult);

        // Notify lwm2mcore that a new instance is created.
        NotifyAppObjLists();

        // Force the type of the install to application install.
        avcServer_SetUpdateType(LE_AVC_APPLICATION_UPDATE);

        switch (restoreState)
        {
            case LWM2MCORE_SW_UPDATE_STATE_INITIAL:
            case LWM2MCORE_SW_UPDATE_STATE_DOWNLOAD_STARTED:
            case LWM2MCORE_SW_UPDATE_STATE_DOWNLOADED:
                break;

            case LWM2MCORE_SW_UPDATE_STATE_DELIVERED:
                // If we got interrupted after receiving the install command from the server,
                // we will restart the install process, else we will wait for the server to
                // send O9F_INSTALL
                if (internalState == INTERNAL_STATE_INSTALL_REQUESTED)
                {
                    LE_INFO("Resuming Install.");
                    // Query control app for permission to install.
                    CurrentObj9 = instanceRef;
                    AvmsInstall = true;
                    result = avcServer_QueryInstall(LaunchSwUpdate,
                                                    LWM2MCORE_SW_UPDATE_TYPE,
                                                    instanceId);

                    LE_FATAL_IF(result == LE_FAULT,
                              "Unexpected error in query install: %s",
                              LE_RESULT_TXT(result));

                    if (result != LE_BUSY)
                    {
                        avcApp_StartInstall(instanceId);
                    }
                }
                break;

            case LWM2MCORE_SW_UPDATE_STATE_INSTALLED:
                if (internalState == INTERNAL_STATE_UNINSTALL_REQUESTED)
                {
                    CurrentObj9 = instanceRef;
                    LE_INFO("Resuming Uninstall.");

                    result = avcServer_QueryUninstall(avcApp_PrepareUninstall, instanceId);

                    if (result != LE_BUSY)
                    {
                        avcApp_PrepareUninstall(instanceId);
                    }
                }
                break;

            default:
                LE_ERROR("Invalid Object 9 state");
                break;
        }
    }
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
    le_sig_Block(SIGPIPE);

    // Register our handler for update progress reports from the Update Daemon.
    le_update_AddProgressHandler(UpdateProgressHandler, NULL);

    // Make sure that we're notified when applications are installed and removed from the system.
    le_instStat_AddAppInstallEventHandler(AppInstallHandler, NULL);
    le_instStat_AddAppUninstallEventHandler(AppUninstallHandler, NULL);

    UpdateEndEventId = le_event_CreateId("UpdateEnd", 0);
    le_event_AddHandler("UpdateEndHandler", UpdateEndEventId, UpdateEndHandler);

    DownloadEventId = le_event_CreateId("DownloadEvent", sizeof(lwm2mcore_PackageDownloader_t));
    le_event_AddHandler("DownloadHandler", DownloadEventId, DownloadHandler);

    PopulateAppInfoObjects();

    // Restore the state of the update process, if avcService was rebooted or interrupted
    // by a power failure while in the middle of an application update.
    RestoreAvcAppUpdateState();
}
