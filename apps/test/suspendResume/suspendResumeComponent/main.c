/**
 * @file main.c
 *
 * AirVantage control application used to test the suspend resume functionality.
 * This app interrupts an AV job at every possible opportunity. A SOTA / FOTA job should be
 * resilient to these interruptions.
 *
 * There are two methods to interrupt a job in this test. One is to reboot the device and other
 * is to restart the AV session. Set the flag "/avtest/IsResetTest" to false to  interrupt the
 * job using a session restart.
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Use reset or session stop to interrupt AV job. By default session stop will be used for a job
 * interruption. Set the flag "avtest/isResetTest" in config tree to true, to use interrupt a job by
 * resetting the device.
 */
//--------------------------------------------------------------------------------------------------
static bool IsResetTest = false;

//--------------------------------------------------------------------------------------------------
/**
 * Update status string
 */
//--------------------------------------------------------------------------------------------------
char UpdateStatusString[100];

//--------------------------------------------------------------------------------------------------
/**
 * AV test fields
 */
//--------------------------------------------------------------------------------------------------

#define IS_RESET_TEST                   "avtest/isResetTest"

#define INTERRUPT_DOWNLOAD_PENDING      "avtest/isDownloadPendingInterrupted"
#define INTERRUPT_DOWNLOAD_PROGRESS     "avtest/isDownloadProgressInterrupted"
#define INTERRUPT_DOWNLOAD_MIDWAY       "avtest/isDownloadMidwayInterrupted"
#define INTERRUPT_DOWNLOAD_COMPLETE     "avtest/isDownloadCompleteInterrupted"
#define INTERRUPT_DOWNLOAD_FAILED       "avtest/isDownloadFailedInterrupted"

#define INTERRUPT_INSTALL_PENDING       "avtest/isInstallPendingInterrupted"
#define INTERRUPT_INSTALL_PROGRESS      "avtest/isInstallProgressInterrupted"
#define INTERRUPT_INSTALL_COMPLETE      "avtest/isInstallCompleteInterrupted"
#define INTERRUPT_INSTALL_FAILED        "avtest/isInstallFailedInterrupted"

#define INTERRUPT_UNINSTALL_PENDING     "avtest/isUninstallPendingInterrupted"
#define INTERRUPT_UNINSTALL_PROGRESS    "avtest/isUninstallProgressInterrupted"
#define INTERRUPT_UNINSTALL_COMPLETE    "avtest/isUninstallCompleteInterrupted"
#define INTERRUPT_UNINSTALL_FAILED      "avtest/isUninstallFailedInterrupted"

//-------------------------------------------------------------------------------------------------
/**
 * Fetch a string describing the type of update underway over Air Vantage.
 *
 * @return Pointer to a null-terminated string constant.
 */
//-------------------------------------------------------------------------------------------------
static const char* GetUpdateType
(
    void
)
{
    const char* updateTypePtr = NULL;
    le_avc_UpdateType_t type;
    le_result_t res = le_avc_GetUpdateType(&type);
    if (res != LE_OK)
    {
        LE_CRIT("Unable to get update type (%s)", LE_RESULT_TXT(res));
        updateTypePtr = "UNKNOWN";
    }
    else
    {
        switch (type)
        {
            case LE_AVC_FIRMWARE_UPDATE:
                updateTypePtr = "FIRMWARE";
                break;

            case LE_AVC_APPLICATION_UPDATE:
                updateTypePtr = "APPLICATION";
                break;

            case LE_AVC_FRAMEWORK_UPDATE:
                updateTypePtr = "FRAMEWORK";
                break;

            case LE_AVC_UNKNOWN_UPDATE:
                updateTypePtr = "UNKNOWN";
                break;

            default:
                LE_CRIT("Unexpected update type %d", type);
                updateTypePtr = "UNKNOWN";
                break;
        }
    }

    return updateTypePtr;
}

//--------------------------------------------------------------------------------------------------
/**
 * Clear all interrupted status
 */
//--------------------------------------------------------------------------------------------------
static void ClearInterruptedStatus
(
    void
)
{
    LE_INFO("Reset interrupt status");

    // Set the download pending interrupted
    le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_PENDING, true);

    // clear everything else
    le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_PROGRESS, false);
    le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_MIDWAY, false);
    le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_COMPLETE, false);
    le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_FAILED, false);

    le_cfg_QuickSetBool(INTERRUPT_INSTALL_PENDING, false);
    le_cfg_QuickSetBool(INTERRUPT_INSTALL_PROGRESS, false);
    le_cfg_QuickSetBool(INTERRUPT_INSTALL_COMPLETE, false);
    le_cfg_QuickSetBool(INTERRUPT_INSTALL_FAILED, false);

    le_cfg_QuickSetBool(INTERRUPT_UNINSTALL_PENDING, false);
    le_cfg_QuickSetBool(INTERRUPT_UNINSTALL_PROGRESS, false);
    le_cfg_QuickSetBool(INTERRUPT_UNINSTALL_COMPLETE, false);
    le_cfg_QuickSetBool(INTERRUPT_UNINSTALL_FAILED, false);
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert update status to string
 */
//--------------------------------------------------------------------------------------------------
const char* GetUpdateStatusString
(
    le_avc_Status_t updateStatus
)
{
    switch (updateStatus)
    {
        case LE_AVC_NO_UPDATE:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_NO_UPDATE", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_DOWNLOAD_PENDING:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_DOWNLOAD_PENDING", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_DOWNLOAD_IN_PROGRESS:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_DOWNLOAD_IN_PROGRESS", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_DOWNLOAD_COMPLETE:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_DOWNLOAD_COMPLETE", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_DOWNLOAD_FAILED:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_DOWNLOAD_FAILED", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_INSTALL_PENDING:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_INSTALL_PENDING", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_INSTALL_IN_PROGRESS:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_INSTALL_IN_PROGRESS", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_INSTALL_COMPLETE:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_INSTALL_COMPLETE", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_INSTALL_FAILED:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_INSTALL_FAILED", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_SESSION_STARTED:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_SESSION_STARTED", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_SESSION_STOPPED:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_SESSION_STOPPED", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_UNINSTALL_PENDING:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_UNINSTALL_PENDING", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_UNINSTALL_IN_PROGRESS:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_UNINSTALL_IN_PROGRESS", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_UNINSTALL_COMPLETE:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_UNINSTALL_COMPLETE", sizeof(UpdateStatusString), NULL);
            break;
        case LE_AVC_UNINSTALL_FAILED:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_UNINSTALL_FAILED", sizeof(UpdateStatusString), NULL);
            break;
        default:
            le_utf8_Copy(UpdateStatusString, "LE_AVC_UNKNOWN", sizeof(UpdateStatusString), NULL);
            break;
    }

    return UpdateStatusString;
}

//--------------------------------------------------------------------------------------------------
/**
 * Interrupt av job
 */
//--------------------------------------------------------------------------------------------------
static void InterruptAvJob
(
    le_avc_Status_t status
)
{
    LE_WARN("Interrupt AV job at %s", GetUpdateStatusString(status));

    if (IsResetTest)
    {
        LE_WARN("Reset device");

        reboot(RB_AUTOBOOT);
    }
    else
    {
        LE_WARN("Restart Session");

        le_avc_StopSession();
        sleep(10);
        le_avc_StartSession();
    }
}

//-------------------------------------------------------------------------------------------------
/**
 * Status handler for avcService updates
 */
//-------------------------------------------------------------------------------------------------
static void StatusHandler
(
    le_avc_Status_t updateStatus,
    int32_t totalNumBytes,
    int32_t downloadProgress,
    void* contextPtr
)
{
    le_result_t res;

    LE_WARN("Update status: %s", GetUpdateStatusString(updateStatus));
    LE_WARN("totalNumBytes: %d", totalNumBytes);
    LE_WARN("downloadProgress: %d", downloadProgress);

    switch (updateStatus)
    {
        case LE_AVC_NO_UPDATE:
            break;

        case LE_AVC_DOWNLOAD_PENDING:
            if (le_cfg_QuickGetBool(INTERRUPT_DOWNLOAD_PENDING, false) == false)
            {
                ClearInterruptedStatus();

                le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_PENDING, true);
                InterruptAvJob(updateStatus);
            }
            else
            {
                LE_INFO("Accepting %s update.", GetUpdateType());
                res = le_avc_AcceptDownload();

                if (res != LE_OK)
                {
                    LE_ERROR("Failed to accept download from Air Vantage (%s)", LE_RESULT_TXT(res));
                }
            }
            break;

        case LE_AVC_DOWNLOAD_IN_PROGRESS:
            if (le_cfg_QuickGetBool(INTERRUPT_DOWNLOAD_PROGRESS, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_PROGRESS, true);
                InterruptAvJob(updateStatus);
            }
            else if (le_cfg_QuickGetBool(INTERRUPT_DOWNLOAD_MIDWAY, false) == false)
            {
                if (downloadProgress > 50)
                {
                    le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_MIDWAY, true);
                    InterruptAvJob(updateStatus);
                }
            }
            break;

        case LE_AVC_DOWNLOAD_COMPLETE:
            if (le_cfg_QuickGetBool(INTERRUPT_DOWNLOAD_COMPLETE, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_COMPLETE, true);
                InterruptAvJob(updateStatus);
            }
            break;

        case LE_AVC_DOWNLOAD_FAILED:
            if (le_cfg_QuickGetBool(INTERRUPT_DOWNLOAD_FAILED, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_DOWNLOAD_FAILED, true);
                InterruptAvJob(updateStatus);
            }
            break;

        case LE_AVC_INSTALL_PENDING:
            if (le_cfg_QuickGetBool(INTERRUPT_INSTALL_PENDING, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_INSTALL_PENDING, true);
                InterruptAvJob(updateStatus);
            }
            else
            {
                LE_INFO("Accepting %s install.", GetUpdateType());
                res = le_avc_AcceptInstall();

                if (res != LE_OK)
                {
                    LE_ERROR("Failed to accept install from Air Vantage (%s)", LE_RESULT_TXT(res));
                }
            }
            break;

        case LE_AVC_INSTALL_IN_PROGRESS:
            if (le_cfg_QuickGetBool(INTERRUPT_INSTALL_PROGRESS, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_INSTALL_PROGRESS, true);
                InterruptAvJob(updateStatus);
            }
            break;

        case LE_AVC_INSTALL_COMPLETE:
            if (le_cfg_QuickGetBool(INTERRUPT_INSTALL_COMPLETE, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_INSTALL_COMPLETE, true);
                InterruptAvJob(updateStatus);
            }
            break;

        case LE_AVC_INSTALL_FAILED:
            if (le_cfg_QuickGetBool(INTERRUPT_INSTALL_FAILED, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_INSTALL_FAILED, true);
                InterruptAvJob(updateStatus);
            }
            break;

        case LE_AVC_UNINSTALL_PENDING:
            if (le_cfg_QuickGetBool(INTERRUPT_UNINSTALL_PENDING, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_UNINSTALL_PENDING, true);
                InterruptAvJob(updateStatus);
            }
            else
            {
                LE_INFO("Accepting %s uninstall.", GetUpdateType());
                res = le_avc_AcceptUninstall();

                if (res != LE_OK)
                {
                    LE_ERROR("Failed to accept uninstall from Air Vantage (%s)", LE_RESULT_TXT(res));
                }
            }
            break;

        case LE_AVC_UNINSTALL_IN_PROGRESS:
            if (le_cfg_QuickGetBool(INTERRUPT_UNINSTALL_PROGRESS, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_UNINSTALL_PROGRESS, true);
                InterruptAvJob(updateStatus);
            }
            break;

        case LE_AVC_UNINSTALL_COMPLETE:
            if (le_cfg_QuickGetBool(INTERRUPT_UNINSTALL_COMPLETE, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_UNINSTALL_COMPLETE, true);
                InterruptAvJob(updateStatus);
            }
            break;

        case LE_AVC_UNINSTALL_FAILED:
            if (le_cfg_QuickGetBool(INTERRUPT_UNINSTALL_FAILED, false) == false)
            {
                le_cfg_QuickSetBool(INTERRUPT_UNINSTALL_FAILED, true);
                InterruptAvJob(updateStatus);
            }
            break;

        case LE_AVC_SESSION_STARTED:
            break;

        case LE_AVC_SESSION_STOPPED:
            break;

        default:
            break;
    }
}

COMPONENT_INIT
{
    // default to use the session stop for job interruption.
    IsResetTest = le_cfg_QuickGetBool(IS_RESET_TEST, false);

    LE_INFO("IsResetTest = %d", IsResetTest);

    // Register AirVantage status report handler.
    le_avc_AddStatusEventHandler(StatusHandler, NULL);

    // Start an AV session.
    le_avc_StartSession();
}
