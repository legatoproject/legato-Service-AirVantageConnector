
//--------------------------------------------------------------------------------------------------
/**
 *  This component handles managing application update over LWM2M as well as the Legato application
 *  objects.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "le_print.h"
#include "appCfg.h"
#include "../avcDaemon/assetData.h"
#include "../avcDaemon/avcServer.h"
#include "pa_avc.h"
#include "avcUpdateShared.h"



//--------------------------------------------------------------------------------------------------
/**
 *  Name of the standard objects in LW M2M.
 */
//--------------------------------------------------------------------------------------------------
#define LWM2M_NAME "lwm2m"



//--------------------------------------------------------------------------------------------------
/**
 *  Config tree path where the state of the update process is backed up.
 */
//--------------------------------------------------------------------------------------------------
#define UPDATE_STATE_BACKUP "/apps/avcService/backup"



//--------------------------------------------------------------------------------------------------
/**
 *  Backup of the object 9 state.
 */
//--------------------------------------------------------------------------------------------------
#define OBJ_INST_ID                 "ObjectInstanceId"
#define STATE_RESTORE               "RestoreState"
#define STATE_RESULT                "RestoreResult"
#define STATE_INSTALL_STARTED       "InstallStarted"
#define STATE_UNINSTALL_STARTED     "UninstallStarted"
#define STATE_DOWNLOAD_REQUESTED    "DownloadRequested"



//--------------------------------------------------------------------------------------------------
/**
 *  Name of this service.
 */
//--------------------------------------------------------------------------------------------------
#define AVC_SERVICE_NAME "avcService"




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
 *  Maximum allowed size for application process name strings.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_PROC_NAME        LE_LIMIT_PROC_NAME_LEN
#define MAX_PROC_NAME_BYTES  (MAX_PROC_NAME + 1)




//--------------------------------------------------------------------------------------------------
/**
 *  Maximum allowed size for URI strings.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_URI_STR        255
#define MAX_URI_STR_BYTES  (MAX_URI_STR + 1)




//--------------------------------------------------------------------------------------------------
/**
 *  Base path for an Object 9 application binding inside of the configTree.
 */
//--------------------------------------------------------------------------------------------------
#define CFG_OBJECT_INFO_PATH "system:/lwm2m/objectMap"


COMPONENT_INIT
{
}
