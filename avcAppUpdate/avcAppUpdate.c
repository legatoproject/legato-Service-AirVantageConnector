
//--------------------------------------------------------------------------------------------------
/**
 * This file handles managing application update (legato side) over LWM2M.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "le_print.h"
#include "appCfg.h"
#include "assetData.h"
#include "avcServer.h"
#include "osPortUpdate.h"
#include "lwm2mcorePackageDownloader.h"
#include "packageDownloader.h"
#include "avcAppUpdate.h"

//--------------------------------------------------------------------------------------------------
/**
 *  Name of the standard objects in LW M2M.
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_NAME "lwm2m"

//--------------------------------------------------------------------------------------------------
/**
 *  LWM2M software object (i.e. object 9).
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_OBJ9  9

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
 *  Maximum allowed size for lwm2m object list strings.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_OBJ9_STR             20
#define MAX_OBJ9_NUM             256
#define MAX_OBJ9_STR_LIST_BYTES  (MAX_OBJ9_STR*MAX_OBJ9_NUM + 1)

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
    lwm2mcore_swUpdateState_t state  ///< The enumeration value to convert.
)
{
    char* resultPtr;

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
        lwm2mcore_swUpdateResult_t swUpdateResult  ///< The enumeration value to convert.
)
{
    char* resultPtr;

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
                "wifiWebAp",
                "fsService",
                "avcUserApp",
                "lwm2mControl"
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
 *  Handler to terminate an ongoing update.
 */
//--------------------------------------------------------------------------------------------------
void UpdateEndHandler
(
    void *reportPtr
)
{
    le_update_End();
}

//--------------------------------------------------------------------------------------------------
/**
 *  Handler to start download.
 */
//--------------------------------------------------------------------------------------------------
static void DownloadHandler
(
     void *contextPtr
)
{
    lwm2mcore_PackageDownloader_t *pkgDwlPtr;
    packageDownloader_DownloadCtx_t *dwlCtxPtr;
    le_result_t result;
    int fd;

    pkgDwlPtr = (lwm2mcore_PackageDownloader_t *)contextPtr;
    dwlCtxPtr = pkgDwlPtr->ctxPtr;

    LE_INFO("contxtptr: %p", pkgDwlPtr);

    fd = open(dwlCtxPtr->fifoPtr, O_RDONLY, 0);
    LE_INFO("Opened fifo");
    if (-1 == fd)
    {
        LE_ERROR("failed to open fifo %m");
        return;
    }

    LE_INFO("Calling update");
    result = le_update_Start(fd);
    if (LE_OK != result)
    {
        LE_ERROR("failed to update software %s", LE_RESULT_TXT(result));
        close(fd);
        return;
    }

    UpdateStarted = true;
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
    lwm2mcore_swUpdateState_t state,          ///< The new state.
    lwm2mcore_swUpdateResult_t result,        ///< The new result.
    const char* functionNamePtr,              ///< Name of the function that called this one.
    size_t line                               ///< The line of this file this function was called
                                              ///< from.
)
{
    int instanceId;
    assetData_GetInstanceId(instanceRef, &instanceId);
    LE_INFO("<%s: %zu>: Set object 9 state/result on instance %d: (%d) %s / (%d) %s",
             functionNamePtr,
             line,
             instanceId,
             state,
             UpdateStateToStr(state),
             result,
             UpdateResultToStr(result));

    if (instanceRef == NULL)
    {
        LE_WARN("Setting state on NULL object.");
        return;
    }

    LE_ASSERT_OK(assetData_client_SetInt(instanceRef, O9F_UPDATE_STATE, state));
    LE_ASSERT_OK(assetData_client_SetInt(instanceRef, O9F_UPDATE_RESULT, result));
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

        LE_INFO("Application '%s' mapped to instance %d.", appNamePtr, instanceId);
    }
    else
    {
        le_cfg_DeleteNode(iterRef, appNamePtr);
        LE_INFO("Deletion of '%s' from cfgTree %s successful", appNamePtr, CFG_OBJECT_INFO_PATH);
    }

    le_cfg_CommitTxn(iterRef);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Read the current state of the given object 9 instance.
 */
//--------------------------------------------------------------------------------------------------
static lwm2mcore_swUpdateState_t GetObj9State
(
    assetData_InstanceDataRef_t instanceRef  ///< The object instance to read.
)
{
    int state;
    LE_INFO("InstanceRef: %p", instanceRef);

    LE_ASSERT_OK(assetData_client_GetInt(instanceRef, O9F_UPDATE_STATE, &state));
    return (lwm2mcore_swUpdateState_t)state;
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
    LE_INFO("Getting object 9 instance for application '%s'.", appNamePtr);

    // Attempt to read the mapping from the configuration.
    assetData_InstanceDataRef_t instanceRef = NULL;
    le_cfg_IteratorRef_t iterRef = le_cfg_CreateReadTxn(CFG_OBJECT_INFO_PATH);

    le_cfg_GoToNode(iterRef, appNamePtr);
    int instanceId = le_cfg_GetInt(iterRef, "oiid", -1);
    le_cfg_CancelTxn(iterRef);

    if (instanceId != -1)
    {
        LE_INFO("Was mapped to instance, %d.", instanceId);

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
                LE_INFO("Instance is existing and has been reused.");
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
    // TODO: Implement this using memory based approach(avoid configTree traversing)

    appCfg_Iter_t appIterRef = appCfg_CreateAppsIter();
    char appName[MAX_APP_NAME_BYTES] = "";
    le_result_t result;

    int foundAppCount = 0;

    result = appCfg_GetNextItem(appIterRef);

    while (result == LE_OK)
    {
        result = appCfg_GetAppName(appIterRef, appName, sizeof(appName));

        if (   (result == LE_OK)
            && (false == IsHiddenApp(appName)))
        {
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
    char lwm2mObjList[MAX_OBJ9_STR_LIST_BYTES] = "";
    size_t objListLen = 0;

    LE_INFO("Found app count %d.", foundAppCount);

    while (foundAppCount > 0)
    {
        assetData_InstanceDataRef_t instanceRef = NULL;
        le_result_t result = assetData_GetInstanceRefById(LWM2M_NAME,
                                                          LWM2M_OBJ9,
                                                          index,
                                                          &instanceRef);

        LE_INFO("Index %d.", index);

        if (result == LE_OK)
        {
            objListLen = snprintf(lwm2mObjList, sizeof(lwm2mObjList), "%s</%s/9/%d>", lwm2mObjList,
                                  LWM2M_NAME,
                                  index);

            LE_ASSERT(objListLen < sizeof(lwm2mObjList));
            foundAppCount--;

            if (foundAppCount)
            {
                // Add comma (delimiter) if it is not the last object
                objListLen = snprintf(lwm2mObjList, sizeof(lwm2mObjList), "%s,", lwm2mObjList);

                LE_ASSERT(objListLen < sizeof(lwm2mObjList));
            }

        }

        index++;
    }

    LE_INFO("ObjListLen; %zd lwm2mObjList: %s", objListLen, lwm2mObjList);
    avcClient_SendList(lwm2mObjList, objListLen);
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
            LE_INFO("Loading object instance for app, '%s'.", appName);

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

    LE_INFO("Found app count %d.", foundAppCount);

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
        LE_INFO("Index %d.", index);

        if (result == LE_OK)
        {
            assetData_client_GetString(instanceRef, O9F_PKG_NAME, appName, sizeof(appName));

            LE_INFO("Mapping app '%s'.", appName);

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

    LE_INFO("AvmsInstall: %d, CurrentObj9: %p", AvmsInstall, CurrentObj9);

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
        LE_INFO("LWM2M Uninstall of application: %p.", CurrentObj9);

        assetData_DeleteInstance(CurrentObj9);
        // State already set to initial in PrepareUninstall
        CurrentObj9 = NULL;

        // If it is not hidden/system app, remove it from lwm2m config tree
        if (false == IsHiddenApp(appNamePtr))
        {
            LE_INFO("Deleting '%s' instance from cfgTree: %s", appNamePtr, CFG_OBJECT_INFO_PATH);
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
            LE_INFO("Deleting '%s' instance from cfgTree: %s", appNamePtr, CFG_OBJECT_INFO_PATH);
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
    LE_FATAL_IF(result == LE_FAULT, "Internal error, error in getting instanceRef for instance: %d",
                                    instanceId);
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

    LE_FATAL_IF(result == LE_FAULT, "Internal error, error in getting appName for instance: %d",
                                     instanceId);
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
 * Function called to kick off the install of a Legato application.
 *
 * @return
 *      - LE_OK if installation started.
 *      - LE_BUSY if package download is not finished yet.
 *      - LE_FAULT if there is an error.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StartInstall
(
    uint16_t instanceId    ///< [IN] Instance id of the app to be installed.
)
{
    LE_INFO("Install application using AirVantage, instanceID: %d.", instanceId);

    assetData_InstanceDataRef_t instanceRef = NULL;
    // Now create an entry into assetData by specifying instanceId
    LE_ASSERT_OK(assetData_GetInstanceRefById(LWM2M_NAME, LWM2M_OBJ9, instanceId, &instanceRef));

    LE_FATAL_IF(CurrentObj9 != instanceRef, "Internal error. CurrentObj9 = %p, instanceRef = %p",
                                             CurrentObj9, instanceRef);

    le_result_t result = le_update_Install();

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

    LE_INFO("Application '%s' uninstall requested, instanceID: %d", appName, instanceId);

    // Just set the state of this object 9 to initial.
    // The server queries for this state and sends us object9 delete, which will kick an uninstall.
    SetObj9State(instanceRef,
                 LWM2MCORE_SW_UPDATE_STATE_INITIAL,
                 LWM2MCORE_SW_UPDATE_RESULT_INITIAL);

    CurrentObj9 = NULL;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function called to kick off an application uninstall.
 *
 * @return
 *     - LE_OK if successful
 *     - LE_NOT_FOUND if given instance not found or given app is not installed.
 *     - LE_FAULT for any other failure.
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_StartUninstall
(
    uint16_t instanceId    ///< [IN] Instance id of the app to be uninstalled.
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

    LE_INFO("Application '%s' uninstall requested, instanceID: %d", appName, instanceId);
    LE_INFO("Send uninstall request.");


    result = le_appRemove_Remove(appName);

    if (result == LE_OK)
    {
        LE_INFO("Uninstall of application completed.");
        CurrentObj9 = instanceRef;
    }
    else
    {
        LE_INFO("Uninstall of application failed.");
    }
    return result;
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

    LE_INFO("Application '%s' start requested, instanceID: %d, instanceRef: %p",
            appName,
            instanceId,
            instanceRef);

    if (GetObj9State(instanceRef) != LWM2MCORE_SW_UPDATE_STATE_INSTALLED)
    {
        LE_ERROR("Application '%s' not installed.", appName);
        return LE_UNAVAILABLE;
    }

    LE_INFO("Send start request.");
    return le_appCtrl_Start(appName);
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

    LE_INFO("Application '%s' stop requested.", appName);

    if (GetObj9State(instanceRef) != LWM2MCORE_SW_UPDATE_STATE_INSTALLED)
    {
        LE_INFO("Application '%s' not installed.", appName);
        return LE_UNAVAILABLE;
    }

    LE_INFO("Send stop request.");
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

    LE_INFO("Application Name: '%s', instanceId: %d.", appNamePtr, instanceId);
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

    LE_FATAL_IF(result == LE_FAULT, "Internal error, error in getting instanceRef for instance: %d",
                                   instanceId);
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

    LE_INFO("App version: '%s', instanceId: %d.", versionPtr, instanceId);
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

    LE_INFO("Application '%s' activation status requested.", appName);

    le_appInfo_State_t state = le_appInfo_GetState(appName);
    LE_INFO("Read of application state, '%s' was found to be: %d", appName, state);
    *valuePtr = (state == LE_APPINFO_RUNNING);

    LE_INFO("App: %s activationState: %d", appName, *valuePtr);

    return LE_OK;
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
    LE_INFO("UpdateProgressHandler");

    switch (updateState)
    {
        case LE_UPDATE_STATE_UNPACKING:
            LE_INFO("Unpacking package, percentDone: %d.", percentDone);
            break;

        case LE_UPDATE_STATE_DOWNLOAD_SUCCESS:
             SetObj9State(CurrentObj9,
                          LWM2MCORE_SW_UPDATE_STATE_DELIVERED,
                          LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED);
            LE_INFO("Download successful");
            break;

        case LE_UPDATE_STATE_APPLYING:
            LE_INFO("Doing update.");
            break;

        case LE_UPDATE_STATE_SUCCESS:
            LE_INFO("Install completed.");
            le_update_End();
            break;

        case LE_UPDATE_STATE_FAILED:
            LE_ERROR("Install/uninstall failed.");
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
 * Create an object 9 instance
 *
 * @return:
 *      - LE_OK on success
 *      - LE_FAULT on any other error
 */
//--------------------------------------------------------------------------------------------------
le_result_t avcApp_CreateObj9Instance
(
    uint16_t instanceId            ///< [IN] object 9 instance id
)
{
    LE_INFO("Requested to create instance: %d", instanceId);
    assetData_InstanceDataRef_t instanceRef = NULL;
    // Now create an entry into assetData by specifying instanceId
    le_result_t result = assetData_CreateInstanceById(LWM2M_NAME,
                                                      LWM2M_OBJ9,
                                                      instanceId,
                                                      &instanceRef);

    LE_ASSERT(result != LE_FAULT);

    // TODO: Should I need to support LE_DUPLICATE or anything other than LE_OK should be an error?
    if (result == LE_DUPLICATE)
    {
        LE_ASSERT_OK(assetData_GetInstanceRefById(LWM2M_NAME,
                                                  LWM2M_OBJ9,
                                                  instanceId,
                                                  &instanceRef));
    }

    CurrentObj9 = instanceRef;
    LE_INFO("Instance creation result: %s", LE_RESULT_TXT(result));
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
    LE_INFO("Requested to Delete instance: %d", instanceId);
    assetData_InstanceDataRef_t instanceRef = NULL;
    char appName[MAX_APP_NAME_BYTES] = "";

    LE_ASSERT_OK(assetData_GetInstanceRefById(LWM2M_NAME,
                                              LWM2M_OBJ9,
                                              instanceId,
                                              &instanceRef));


    le_result_t result = assetData_client_GetString(instanceRef,
                                                    O9F_PKG_NAME,
                                                    appName,
                                                    sizeof(appName));
    LE_ASSERT(result != LE_FAULT);
    LE_ASSERT(result != LE_OVERFLOW);

    if (result == LE_OK)
    {
        // No app installed
        result = avcApp_StartUninstall(instanceId);
    }
    else if (result == LE_NOT_FOUND)
    {
        assetData_DeleteInstance(instanceRef);
        result = LE_OK;
    }
    else
    {
        LE_FATAL("Internal error");
    }

    LE_INFO("Instance deletion result: %s", LE_RESULT_TXT(result));
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

    LE_INFO("contxt ptr: %p", ctxPtr);

    // TODO: This is a workaround to avoid delay in opening named fifo. This function need to return
    // quickly(<2s) to prevent coap retry. Remove it later.
    le_event_Report(DownloadEventId, ctxPtr, sizeof(lwm2mcore_PackageDownloader_t));

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
    lwm2mcore_swUpdateResult_t updateResult
)
{
    LE_INFO("Requested to set result: %d, instance: %p", updateResult, CurrentObj9);
    LE_ASSERT(CurrentObj9 != NULL);

    switch (updateResult)
    {
        case LWM2MCORE_SW_UPDATE_RESULT_INITIAL:
            LE_INFO("Initial state");
            break;

        case LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADING:
            LE_INFO("Package Downloading");
            break;

        case LWM2MCORE_SW_UPDATE_RESULT_INSTALLED:
            LE_INFO("Package Installed");
            break;

        case LWM2MCORE_SW_UPDATE_RESULT_DOWNLOADED:
            LE_INFO("Package downloaded");
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
    lwm2mcore_swUpdateState_t updateState
)
{
    LE_INFO("Requested to set state: %d, instance: %p", updateState, CurrentObj9);
    LE_ASSERT(CurrentObj9 != NULL);

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

    LE_INFO("Requested to get update result for instance id: %d", instanceId);
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
    LE_ASSERT_OK(assetData_client_GetInt(instanceRef, O9F_UPDATE_RESULT, &updateResult));

    *updateResultPtr = (uint8_t)updateResult;

    LE_INFO("UpdateResult: %d, instance id: %d", updateResult, instanceId);
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

    LE_INFO("Requested to get update state for instance id: %d", instanceId);

    le_result_t result = assetData_GetInstanceRefById(LWM2M_NAME,
                                                      LWM2M_OBJ9,
                                                      instanceId,
                                                      &instanceRef);

    if (result != LE_OK)
    {
        return result;
    }

    int updateState = 0;
    LE_ASSERT_OK(assetData_client_GetInt(instanceRef, O9F_UPDATE_STATE, &updateState));

    *updateStatePtr = (uint8_t)updateState;
    LE_INFO("UpdateState: %d, instance id: %d", *updateStatePtr, instanceId);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialisation function avcApp. Should be called only once.
 */
//--------------------------------------------------------------------------------------------------
void avcApp_Init
(
   void
)
{
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
}
