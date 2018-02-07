//-------------------------------------------------------------------------------------------------
/**
 * @file avcTest.c
 *
 * Integration test application for the airVantageConnector service
 * @note The platform has to be configured to make data connection possible with DCS
 *
 * Default listening port: 7000. To change it :
 *          app runProc avcTest avcTest -- port 7001
 *
 * 1. In order to connect to the socket from localhost, add the iptable rule in the target:
 *          iptables -I INPUT -p tcp -m tcp --dport 7000 -j ACCEPT
 * 2. Run the avcTest server as a regular app:
 *          app runProc avcTest avcTest
 * 3. Run netcat from target or localhost console:
 *          nc localhost 7000
 *          nc ip_target 7000
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//-------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>

//--------------------------------------------------------------------------------------------------
/**
 * Define value for help command
 */
//--------------------------------------------------------------------------------------------------
#define HELP_COMMAND "help"

//--------------------------------------------------------------------------------------------------
/**
 * Description for help command
 */
//--------------------------------------------------------------------------------------------------
#define HELP_DESC    "Type '"HELP_COMMAND" [COMMAND]' for more details on a command."

//--------------------------------------------------------------------------------------------------
/**
 * Description for unknown command
 */
//--------------------------------------------------------------------------------------------------
#define UNKNOWN_CMD_MSG "Unknown command. Type '"HELP_COMMAND"' for help."

//--------------------------------------------------------------------------------------------------
/**
 * Maximum packet size
 */
//--------------------------------------------------------------------------------------------------
#define MAX_PACKET_SIZE 256

//--------------------------------------------------------------------------------------------------
/**
 * Socket buffer size
 */
//--------------------------------------------------------------------------------------------------
#define SOCKET_BUFFER_SIZE 1024

//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of parameters in each command
 */
//--------------------------------------------------------------------------------------------------
#define MAX_NB_PARAMETERS 10

//--------------------------------------------------------------------------------------------------
/**
 * Prefix to retrieve files from secStoreGlobal service.
 */
//--------------------------------------------------------------------------------------------------
#define SECURE_STORAGE_PREFIX "/avms"

//--------------------------------------------------------------------------------------------------
/**
 * Socket port number
 */
//--------------------------------------------------------------------------------------------------
static int SocketPort = 7000;

//--------------------------------------------------------------------------------------------------
/**
 * Data socket file descriptor
 */
//--------------------------------------------------------------------------------------------------
static int SocketFd = -1;

//--------------------------------------------------------------------------------------------------
/**
 * Static variable to know if the application needs to quit
 */
//--------------------------------------------------------------------------------------------------
static bool Quit = false;

//--------------------------------------------------------------------------------------------------
/**
 * Static AVC status event handler
 */
//--------------------------------------------------------------------------------------------------
static le_avc_StatusEventHandlerRef_t AvcEventHandlerRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static event Id for thread
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t AvcRegistrationEventId;

//--------------------------------------------------------------------------------------------------
/**
 * Static event Id for incoming user command
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t UserCommandEventId;

//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference
 */
//--------------------------------------------------------------------------------------------------
static le_event_HandlerRef_t EventHandlerRef;

//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference
 */
//--------------------------------------------------------------------------------------------------
static le_event_HandlerRef_t AvcCmdHandlerRef;

//--------------------------------------------------------------------------------------------------
/**
 * Static boolean variable for "auto" argument:
 * automatic mode: automatically accept user agreements)
 */
//--------------------------------------------------------------------------------------------------
static bool AutoMode = false;

//--------------------------------------------------------------------------------------------------
/**
 * Static variable for command thread
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t UserCommandThreadRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static variable for AVC thread
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t AvcThreadRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Static reference for block update request
 */
//--------------------------------------------------------------------------------------------------
static le_avc_BlockRequestRef_t BlockRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Startup synchronization semaphore
 */
//--------------------------------------------------------------------------------------------------
static le_sem_Ref_t SyncSemRef;

//--------------------------------------------------------------------------------------------------
/**
 * Static variable for status
 */
//--------------------------------------------------------------------------------------------------
static le_avc_Status_t NotificationId;

//--------------------------------------------------------------------------------------------------
/**
 * Static variable for package download progress
 */
//--------------------------------------------------------------------------------------------------
static int32_t DownloadProgress = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Struture for thread event registration
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_avc_StatusEventHandlerRef_t* eventHandlerRefPtr;
    le_avc_StatusHandlerFunc_t      eventHandler;
    void*                           contextPtr;
}
RegisterAvcEvent_t;

//--------------------------------------------------------------------------------------------------
/**
 * Supported commands enumeration
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    CNX_STATE,                   ///< Get connection state
    START_CNX,                   ///< Start a connection
    STOP_CNX,                    ///< Stop a connection
    USER_AGREEMENTS,             ///< Enable/Disable user agreements
    UPDATE_REQUEST,              ///< Send a registration update
    ACCEPT_DOWNLOAD,             ///< Accept package download
    DEFER_DOWNLOAD,              ///< Defer package download
    ACCEPT_INSTALL,              ///< Accept package install
    DEFER_INSTALL,               ///< Defer package install
    ACCEPT_UNINSTALL,            ///< Accept package uninstall
    DEFER_UNINSTALL,             ///< Defer package uninstall
    ACCEPT_REBOOT,               ///< Accept device reboot
    DEFER_REBOOT,                ///< Defer device reboot
    DEFER_CNX,                   ///< Defer a connecton
    BLOCK_UNBLOCK,               ///< Block /Unblock application install
    QUIT,                        ///< Quit
    MAX_CMD                      ///< Internal usage
}
AvcCommand_t;

//--------------------------------------------------------------------------------------------------
/**
 * Structure for supported commands table
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char*              namePtr;        ///< Command string
    char*              shortDescPtr;   ///< Command description
    AvcCommand_t       cmdId;          ///< Related command enumeration
}
CommandDesc_t;


//--------------------------------------------------------------------------------------------------
/**
 * Structure for thread avc command
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    AvcCommand_t        commandId;          ///< Related command enumeration
    char                rawCmd[MAX_PACKET_SIZE];
    void*               contextPtr;
}
AvcCmdEvent_t;


//--------------------------------------------------------------------------------------------------
/**
 * Supported AVC command table
 */
//--------------------------------------------------------------------------------------------------
CommandDesc_t Commands[] =
{
    {"state",            "Get connection state",                         CNX_STATE              },
    {"start",            "Launch a connection",                          START_CNX              },
    {"stop",             "Stop a connection",                            STOP_CNX               },
    {"userAgr",          "Enable/Disable user agreements."\
                         "(takes 1 arg: [1,0])",                         USER_AGREEMENTS        },
    {"update",           "Trigger a registration update",                UPDATE_REQUEST         },
    {"acceptDownload",   "Accept package download",                      ACCEPT_DOWNLOAD        },
    {"deferDownload",    "Defer package download.(1 arg: [x] min)",      DEFER_DOWNLOAD         },
    {"acceptInstall",    "Accept package install",                       ACCEPT_INSTALL         },
    {"deferInstall",     "Defer package install.(1 arg: [x] min)",       DEFER_INSTALL          },
    {"acceptUninstall",  "Accept package uninstall",                     ACCEPT_UNINSTALL       },
    {"deferUninstall",   "Defer package uninstall.(1 arg: [x] min)",     DEFER_UNINSTALL        },
    {"acceptReboot",     "Accept device reboot",                         ACCEPT_REBOOT          },
    {"deferReboot",      "Defer device reboot.(1 arg: [x] min)",         DEFER_REBOOT           },
    {"deferConnection",  "Defer a connection",                           DEFER_CNX              },
    {"blockInstall",     "Block/Unblock app install",                    BLOCK_UNBLOCK          },
    {"quit",             "Quit the server gracefully",                   QUIT                   },
    {"^C",               "Quit the client",                              MAX_CMD                },
    {NULL,               NULL,                                           MAX_CMD                }
};

//--------------------------------------------------------------------------------------------------
/**
 * Close a socket
 */
//--------------------------------------------------------------------------------------------------
static void CloseSocket
(
    int socketfd
)
{
    if (socketfd >= 0)
    {
        if (!close(socketfd))
        {
            LE_ERROR("Error in close(): %d %m", errno);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Create a socket in blocking mode and start listening on the specified port
 */
//--------------------------------------------------------------------------------------------------
static le_result_t RunSocket
(
    int port,
    int* socketfd
)
{
    int result;
    struct sockaddr_in name;
    int enable = 1;
    int sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        LE_ERROR("Unable to create a socket");
        return LE_FAULT;
    }

    result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (result < 0)
    {
        LE_ERROR("setsockopt(SO_REUSEADDR) failed");
        CloseSocket(sock);
        return LE_FAULT;
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    result = bind(sock, (struct sockaddr *)&name, sizeof(name));
    if (result < 0)
    {
        LE_ERROR("Unable to bind the socket");
        CloseSocket(sock);
        return LE_FAULT;
    }

    result = listen(sock, 1);
    if (result < 0)
    {
        LE_ERROR("Unable to listen to the socket");
        CloseSocket(sock);
        return LE_FAULT;
    }

    *socketfd = sock;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send data to the connected socket file descriptor (Similar to printf)
 */
//--------------------------------------------------------------------------------------------------
static void SendSocket
(
    const char *format, ///< Variable arguments similar to printf arguments
    ...
)
{
    va_list arg;
    char buffer[SOCKET_BUFFER_SIZE] = {0};
    int length;

    va_start(arg, format);
    length = vsprintf(buffer, format, arg);
    va_end(arg);

    if (SocketFd >= 0)
    {
        if (send(SocketFd, buffer, length, 0) < 0)
        {
            LE_ERROR("Error in send(): %d %m", errno);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Tokenise an input buffer to argc, argv format
 *
 */
//--------------------------------------------------------------------------------------------------
static void TokeniseCommand
(
    char* bufferPtr,  ///< [INOUT] Modifiable String Buffer To Tokenise
    int* argc,        ///< [OUT] Argument Count
    char* argv[],     ///< [OUT] Argument String Vector Array
    int  argv_length  ///< [IN] Maximum Count For *argv[]
)
{
    int i = 0;

    // Tokenise string buffer into argc and argv format (req: string.h)
    for (i = 0; i < argv_length; i++)
    {
        // Fill argv via strtok_r()
        argv[i] = strtok_r(NULL , " ", &bufferPtr);
        if (NULL == argv[i])
        {
            break;
        }
    }
    *argc = i;
}

//--------------------------------------------------------------------------------------------------
/**
 * Status handler
 *
 * Everything is driven from this handler
 */
//--------------------------------------------------------------------------------------------------
static void StatusHandler
(
    le_avc_Status_t updateStatus,
    int32_t         totalNumBytes,
    int32_t         downloadProgress,
    void*           contextPtr
)
{
    NotificationId = updateStatus;
    DownloadProgress = downloadProgress;

    switch (updateStatus)
    {
        case LE_AVC_NO_UPDATE:
            LE_INFO("NO UPDATE");
            SendSocket("Receive LE_AVC_NO_UPDATE\n> ");
            break;

        case LE_AVC_CONNECTION_PENDING:
            LE_INFO("CONNECTION PENDING");
            SendSocket("Receive LE_AVC_CONNECTION_PENDING\n> ");
            break;

        case LE_AVC_DOWNLOAD_PENDING:
            LE_INFO("DOWNLOAD PENDING");
            SendSocket("Receive LE_AVC_DOWNLOAD_PENDING: size %d\n> ", totalNumBytes);

            if (AutoMode && (totalNumBytes > 0))
            {
                le_result_t result = le_avc_AcceptDownload();
                LE_INFO("Automatically accept package download %d", result);
                SendSocket("Automatically accept package download %d\n> ", result);
            }

            if (totalNumBytes == -1)
            {
                SendSocket("ERROR on DOWNLOAD_PENDING, size -1\n> ");
            }
            break;

        case LE_AVC_DOWNLOAD_IN_PROGRESS:
            LE_INFO("DOWNLOAD IN PROGRESS %d", downloadProgress);
            SendSocket("Receive LE_AVC_DOWNLOAD_IN_PROGRESS %d\n> ", downloadProgress);
            break;

        case LE_AVC_DOWNLOAD_COMPLETE:
            LE_INFO("DOWNLOAD COMPLETE");
            SendSocket("Receive LE_AVC_DOWNLOAD_COMPLETE\n> ");
            break;

        case LE_AVC_DOWNLOAD_FAILED:
            LE_INFO("DOWNLOAD FAILED");
            SendSocket("Receive LE_AVC_DOWNLOAD_FAILED\n> ");
            break;

        case LE_AVC_INSTALL_PENDING:
            LE_INFO("INSTALL PENDING");
            SendSocket("Receive LE_AVC_INSTALL_PENDING\n> ");
            if (AutoMode)
            {
                le_result_t result = le_avc_AcceptInstall();
                LE_INFO("Automatically accept package install %d", result);
            }
            break;

        case LE_AVC_INSTALL_IN_PROGRESS:
            LE_INFO("INSTALL IN PROGRESS");
            SendSocket("Receive LE_AVC_INSTALL_IN_PROGRESS\n> ");
            break;

        case LE_AVC_INSTALL_COMPLETE:
            LE_INFO("INSTALL COMPLETE");
            SendSocket("Receive LE_AVC_INSTALL_COMPLETE\n> ");
            break;

        case LE_AVC_INSTALL_FAILED:
            LE_INFO("INSTALL FAILED");
            SendSocket("Receive LE_AVC_INSTALL_FAILED\n> ");
            break;

        case LE_AVC_UNINSTALL_PENDING:
            LE_INFO("UNINSTALL PENDING");
            SendSocket("Receive LE_AVC_UNINSTALL_PENDING\n> ");
            if (AutoMode)
            {
                le_result_t result = le_avc_AcceptUninstall();
                LE_INFO("Automatically accept package uninstall %d", result);
            }
            break;

        case LE_AVC_UNINSTALL_IN_PROGRESS:
            LE_INFO("UNINSTALL IN PROGRESS");
            SendSocket("Receive LE_AVC_UNINSTALL_IN_PROGRESS\n> ");
            break;

        case LE_AVC_UNINSTALL_COMPLETE:
            LE_INFO("UNINSTALL COMPLETE");
            SendSocket("Receive LE_AVC_UNINSTALL_COMPLETE\n> ");
            break;

        case LE_AVC_UNINSTALL_FAILED:
            LE_INFO("UNINSTALL FAILED");
            SendSocket("Receive LE_AVC_UNINSTALL_FAILED\n> ");
            break;

        case LE_AVC_REBOOT_PENDING:
            LE_INFO("REBOOT PENDING");
            SendSocket("Receive LE_AVC_REBOOT_PENDING\n> ");
            if (AutoMode)
            {
                le_result_t result = le_avc_AcceptReboot();
                LE_INFO("Automatically accept device reboot %d", result);
            }
            break;

        case LE_AVC_SESSION_STARTED:
            SendSocket("Receive LE_AVC_SESSION_STARTED: device is connected to DM server\n> ");
            LE_INFO("SESSION START to DM");
            break;

        case LE_AVC_SESSION_STOPPED:
            LE_INFO("SESSION STOP");
            SendSocket("Receive LE_AVC_SESSION_STOPPED\n> ");
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Meta events for thread
*/
//--------------------------------------------------------------------------------------------------
static void MetaEventHandler
(
    void* reportPtr
)
{
    RegisterAvcEvent_t* eventContextPtr = (RegisterAvcEvent_t*)reportPtr;
    if (NULL != eventContextPtr)
    {
        le_avc_StatusEventHandlerRef_t avcStatusHandler;
        avcStatusHandler = le_avc_AddStatusEventHandler(eventContextPtr->eventHandler,
                                                        eventContextPtr->contextPtr);
        LE_ASSERT(avcStatusHandler != NULL);

        if (eventContextPtr->eventHandlerRefPtr != NULL)
        {
            (*eventContextPtr->eventHandlerRefPtr) = avcStatusHandler;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * AVC command treatment
*/
//--------------------------------------------------------------------------------------------------
static void AvcCmdHandler
(
    void* reportPtr
)
{
    le_result_t result = LE_BAD_PARAMETER;
    int argc = 0;
    char* argvPtr[MAX_NB_PARAMETERS] = {0};
    int deferMinutes = 1;

    AvcCmdEvent_t* CmdPtr = (AvcCmdEvent_t*)reportPtr;
    if (NULL == CmdPtr)
    {
        LE_ERROR("AvcCmdHandler param error");
        return;
    }
    LE_INFO("AvcCmdHandler cmdId %d", CmdPtr->commandId);

    // Extract parameters from raw buffer
    TokeniseCommand(CmdPtr->rawCmd, &argc, argvPtr, sizeof(argvPtr));

    switch (CmdPtr->commandId)
    {
        case CNX_STATE:
        {
            le_avc_SessionType_t cnx = le_avc_GetSessionType();
            if (LE_AVC_BOOTSTRAP_SESSION ==  cnx)
            {
                SendSocket("Connected to bootstrap server\n> ");
            }
            else if (LE_AVC_DM_SESSION ==  cnx)
            {
                SendSocket("Connected to DM server\n> ");
            }
            else
            {
                SendSocket("Invalid connection or not connected\n> ");
            }
            result = LE_OK;
        }
        break;

        case START_CNX:
            result = le_avc_StartSession();
            break;

        case STOP_CNX:
            result = le_avc_StopSession();
            break;

        case DEFER_CNX:
            result = le_avc_DeferConnect(1);
            break;

        case USER_AGREEMENTS:
        {
            bool enable = false;
            if (argc >= 2)
            {
                enable = (strtol(argvPtr[1], NULL, 0) >= 1) ? true : false;
            }
            result = le_avc_SetUserAgreement(LE_AVC_USER_AGREEMENT_CONNECTION, enable);
            result |= le_avc_SetUserAgreement(LE_AVC_USER_AGREEMENT_DOWNLOAD, enable);
            result |= le_avc_SetUserAgreement(LE_AVC_USER_AGREEMENT_INSTALL, enable);
            result |= le_avc_SetUserAgreement(LE_AVC_USER_AGREEMENT_UNINSTALL, enable);
            result |= le_avc_SetUserAgreement(LE_AVC_USER_AGREEMENT_REBOOT, enable);
        }
        break;

        case UPDATE_REQUEST:
            result = le_avc_CheckRoute();
            break;

        case ACCEPT_DOWNLOAD:
            result = le_avc_AcceptDownload();
            break;

        case DEFER_DOWNLOAD:
            if (argc >= 2)
            {
                deferMinutes = (uint32_t)strtol(argvPtr[1], NULL, 0);
            }
            result = le_avc_DeferDownload(deferMinutes);
            break;

        case ACCEPT_INSTALL:
            result = le_avc_AcceptInstall();
            break;

        case DEFER_INSTALL:
            if (argc >= 2)
            {
                deferMinutes = (uint32_t)strtol(argvPtr[1], NULL, 0);
            }
            result = le_avc_DeferInstall(deferMinutes);
            break;

        case ACCEPT_UNINSTALL:
            result = le_avc_AcceptUninstall();
            break;

        case DEFER_UNINSTALL:
            if (argc >= 2)
            {
                deferMinutes = (uint32_t)strtol(argvPtr[1], NULL, 0);
            }
            result = le_avc_DeferUninstall(deferMinutes);
            break;

        case ACCEPT_REBOOT:
            result = le_avc_AcceptReboot();
            break;

        case DEFER_REBOOT:
            if (argc >= 2)
            {
                deferMinutes = (uint32_t)strtol(argvPtr[1], NULL, 0);
            }
            result = le_avc_DeferReboot(deferMinutes);
            break;

        case BLOCK_UNBLOCK:
            if (NULL == BlockRef)
            {
                BlockRef = le_avc_BlockInstall();
                LE_INFO("Block mode activated: Prevent any pending updates from being installed");
                SendSocket("Block mode activated: Prevent any pending updates from being"\
                           "installed\n> ");
            }
            else
            {
                le_avc_UnblockInstall(BlockRef);
                BlockRef = NULL;
                LE_INFO("Block mode deactivated: Allow any pending updates to be installed");
                SendSocket("Block mode deactivated: Allow any pending updates to be installed\n> ");
            }
            result = LE_OK;
            break;

        case QUIT:
            Quit = true;
            break;

        default:
            LE_ERROR("Bad cmd Id");
            break;
    }
    LE_INFO("Cmd treatment result %d", result);
}

//--------------------------------------------------------------------------------------------------
/**
 * Registration of thread
*/
//--------------------------------------------------------------------------------------------------
static void RegisterToAvcEvent
(
    void
)
{
    LE_INFO("RegisterToAvcEvent");
    RegisterAvcEvent_t avcEvent;
    avcEvent.eventHandlerRefPtr = &AvcEventHandlerRef;
    avcEvent.eventHandler = StatusHandler;
    avcEvent.contextPtr = NULL;
    le_event_Report(AvcRegistrationEventId, &avcEvent, sizeof(RegisterAvcEvent_t));
}

//--------------------------------------------------------------------------------------------------
/**
 * Notification thread start
 */
//--------------------------------------------------------------------------------------------------
static void* AvcThread
(
    void* contextPtr
)
{
    LE_INFO("Connect to AVC service");
    le_avc_ConnectService();

    EventHandlerRef = le_event_AddHandler("MetaEventHandlerRef",
                                          AvcRegistrationEventId,
                                          MetaEventHandler);
    AvcCmdHandlerRef =  le_event_AddHandler("MetaEventHandlerRef",
                                            UserCommandEventId,
                                            AvcCmdHandler);
    le_sem_Post(SyncSemRef);
    le_event_RunLoop();
    LE_INFO("Now Running");
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Service thread start
 */
//--------------------------------------------------------------------------------------------------
static void* UserCommandHandlerThread
(
    void* contextPtr
)
{
    le_event_RunLoop();
    LE_INFO("Now Running");
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Thread init
*/
//--------------------------------------------------------------------------------------------------
static void ThreadInit
(
    void
)
{
    LE_INFO("Start the handler thread to monitor the events.");

    AvcRegistrationEventId = le_event_CreateId("EventThreadEvent", sizeof(RegisterAvcEvent_t));
    UserCommandEventId = le_event_CreateId("AvcCommandEvent", sizeof(AvcCmdEvent_t));

    UserCommandThreadRef = le_thread_Create("UserCommandThread", UserCommandHandlerThread, NULL);
    LE_ASSERT(UserCommandThreadRef != NULL);
    le_thread_SetJoinable(UserCommandThreadRef);
    le_thread_Start(UserCommandThreadRef);

    AvcThreadRef = le_thread_Create("AvcThread", AvcThread, NULL);
    LE_ASSERT(AvcThreadRef != NULL);
    le_thread_Start(AvcThreadRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to find a command in supported commands
*/
//--------------------------------------------------------------------------------------------------
static CommandDesc_t* FindCommand
(
    CommandDesc_t*  commandArrayPtr,    ///< [IN] Supported commands table
    char*           bufferPtr,          ///< [IN] Incoming command
    size_t          length              ///< [IN] Incoming command length
)
{
    int i;

    if (0 == length)
    {
        return NULL;
    }

    i = 0;
    while ((commandArrayPtr[i].namePtr != NULL)
        && ((strlen(commandArrayPtr[i].namePtr) != length)
        || (strncmp(bufferPtr, commandArrayPtr[i].namePtr, length))))
    {
        i++;
    }

    if (commandArrayPtr[i].namePtr == NULL)
    {
        return NULL;
    }

    return &commandArrayPtr[i];
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to display the executable help
*/
//--------------------------------------------------------------------------------------------------
static void DisplayExecHelp
(
    void
)
{
    LE_INFO("avcIntegrationTest usage: ");

    LE_INFO("To launch avcIntegrationTest: "
            "app runProc avcTest avcTest"
            );
    LE_INFO("To launch avcIntegrationTest with automatic acceptance to user agreements: "
            "app runProc avcTest avcTest -- auto"
            );
    LE_INFO("To change the listening port: "
            "app runProc avcTest avcTest -- port 7001"
            );
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to display the command help
*/
//--------------------------------------------------------------------------------------------------
static void DisplayCommandHelp
(
    CommandDesc_t*  commandArrayPtr,    ///< [IN] Supported commands table
    char*           bufferPtr           ///< [IN] Incoming command
)
{
    if ((NULL != commandArrayPtr) && (NULL != bufferPtr))
    {
        CommandDesc_t* cmdPtr;
        int length;

        // Find end of first argument
        length = 0;
        while ((bufferPtr[length] != 0) && (!isspace(bufferPtr[length]&0xff)))
        {
            length++;
        }

        cmdPtr = FindCommand(commandArrayPtr, bufferPtr, length);

        if (cmdPtr == NULL)
        {
            int i;

            SendSocket("%-20s "HELP_DESC"\n",HELP_COMMAND);

            for (i = 0 ; commandArrayPtr[i].namePtr != NULL ; i++)
            {
                SendSocket("%-20s %s\n",
                                commandArrayPtr[i].namePtr,
                                commandArrayPtr[i].shortDescPtr);
            }
        }
        else
        {
            SendSocket("%s\n", cmdPtr->shortDescPtr);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to treat a supported command
*/
//--------------------------------------------------------------------------------------------------
static void HandleCommand
(
    CommandDesc_t* commandArrayPtr, ///< [IN] Supported commands table
    char*          bufferPtr        ///< [IN] Incoming command
)
{
    if ((NULL != commandArrayPtr) && (NULL != bufferPtr))
    {
        CommandDesc_t* cmdPtr;
        int length;
        int cmdLen;

        // Find and check full command length
        cmdLen = strlen(bufferPtr);
        if (cmdLen > MAX_PACKET_SIZE)
        {
            LE_ERROR("Command size exceeds the maximum allowed size");
            return;
        }

        // Check if carriage return is received
        if ((1 == cmdLen) && (0x0A == bufferPtr[0]))
        {
            SendSocket("\n> ");
            return;
        }

        // Find end of command name
        length = 0;
        while ((bufferPtr[length] != 0) && (!isspace(bufferPtr[length]&0xFF)))
        {
            length++;
        }
        LE_INFO("Incoming command: %s", bufferPtr);

        cmdPtr = FindCommand(commandArrayPtr, bufferPtr, length);
        if (cmdPtr != NULL)
        {
            while ((bufferPtr[length] != 0) && (isspace(bufferPtr[length]&0xFF)))
            {
                length++;
            }

            if (QUIT == cmdPtr->cmdId)
            {
                Quit = true;
            }
            else if (cmdPtr->cmdId < MAX_CMD)
            {
                AvcCmdEvent_t AvcCmdevent;
                AvcCmdevent.commandId = cmdPtr->cmdId;
                memset((char*)AvcCmdevent.rawCmd, 0x00, MAX_PACKET_SIZE);
                memcpy((char*)AvcCmdevent.rawCmd, bufferPtr, cmdLen);

                le_event_Report(UserCommandEventId, &AvcCmdevent, sizeof(AvcCmdEvent_t));
            }
            else
            {
                LE_ERROR("bad cmd Id");
            }
        }
        else
        {
            if (!strncmp(bufferPtr, HELP_COMMAND, length))
            {
                while ((bufferPtr[length] != 0) && (isspace(bufferPtr[length]&0xFF)))
                {
                    length++;
                }
                DisplayCommandHelp(commandArrayPtr, bufferPtr + length);
            }
            else
            {
                SendSocket(UNKNOWN_CMD_MSG"\r\n");
            }
        }
        SendSocket("\n> ");
    }
}



//--------------------------------------------------------------------------------------------------
/**
 * Entry point for client
*/
//--------------------------------------------------------------------------------------------------
static void Client
(
    void* param1Ptr,
    void* param2Ptr
)
{
    int numBytes;
    uint8_t buffer[MAX_PACKET_SIZE];
    struct sockaddr_in clientname;
    socklen_t size = sizeof(clientname);
    int sock;

    // Create a socket in blocking mode and start listening
    if (LE_OK != RunSocket(SocketPort, &sock))
    {
        LE_ERROR("Unable to start socket on specified port");
        le_thread_Exit(NULL);
    }

    while (false == Quit)
    {
        // Accept connection from an incoming client. Note that only a client is served at a time.
        SocketFd = accept(sock, (struct sockaddr*)&clientname, (socklen_t*)&size);
        if (SocketFd < 0)
        {
            LE_ERROR("Accept failed: %d %m", errno);

            if ((EBADF == errno) || (ENOTSOCK == errno) || (EFAULT == errno) || (EINVAL == errno))
            {
                // These errors are not recoverable
                Quit = true;
            }

            // Restart the main loop
            continue;
        }

        // At this point, a client is connected
        DisplayCommandHelp(Commands, (char*)buffer);
        SendSocket("> ");

        // Receive a message from client
        do
        {
            numBytes = recv(SocketFd, buffer, MAX_PACKET_SIZE - 1, 0);
            if (-1 == numBytes)
            {
                LE_ERROR("Recv failed: %d %m", errno);

                if ((EBADF == errno) || (ENOTSOCK == errno) || (EFAULT == errno) ||
                    (EINVAL == errno) || (ECONNREFUSED == errno) || (ENOTCONN == errno))
                {
                    // Connection lost, go back to the main loop
                    CloseSocket(SocketFd);
                    break;
                }
                else
                {
                    continue;
                }
            }
            else if (0 == numBytes)
            {
                LE_DEBUG("Client disconnected");
                CloseSocket(SocketFd);
                break;
            }
            else if (numBytes > 0)
            {
                buffer[numBytes] = 0;
                // Call the corresponding callback of the typed command passing it the
                // buffer for further arguments
                HandleCommand(Commands, (char*)buffer);

                // Check if client asked to shutdown the server
                if (true == Quit)
                {
                    break;
                }
            }
        }
        while(1);
    }

    // Close client socket if already connected and the original socket
    CloseSocket(sock);
    CloseSocket(SocketFd);

    le_thread_Exit(NULL);
}

//--------------------------------------------------------------------------------------------------
/**
 * Init the component
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    int i;
    int nbArguments;
    const char* argPtr;

    // Parse input arguments
    nbArguments = le_arg_NumArgs();
    i = 0;
    while (i < nbArguments)
    {
        argPtr  = le_arg_GetArg(i);
        if (!strcmp(argPtr, "auto"))
        {
            AutoMode = true;
        }
        else if ((!strcmp(argPtr, "help")) ||
                 (!strcmp(argPtr, "--help")) ||
                 (!strcmp(argPtr, "-h")))
        {
            DisplayExecHelp();
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp(argPtr, "port"))
        {
            if (i < (nbArguments-1))
            {
                SocketPort = strtol(le_arg_GetArg(i+1), NULL, 0);
                i = i+1;
            }
        }
        else
        {
            LE_INFO("Unhandled input argument");
        }
        i = i+1;
    }

    // Create a semaphore to coordinate the starup sequence
    SyncSemRef = le_sem_Create("startup", 0);

    // Check if a thread needs to be used
    ThreadInit();
    le_sem_Wait(SyncSemRef);
    le_sem_Delete(SyncSemRef);

    // Register AirVantage status report handler.
    RegisterToAvcEvent();

    le_event_QueueFunctionToThread(UserCommandThreadRef, Client, NULL, NULL);
    le_thread_Join(UserCommandThreadRef, NULL);

    le_avc_RemoveStatusEventHandler(AvcEventHandlerRef);

    LE_INFO("exit avcIntegrationTest");
    exit(EXIT_SUCCESS);
}
