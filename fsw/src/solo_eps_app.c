/*******************************************************************************
** File: 
**  solo_eps_app.c
**
** Purpose:
**   This file contains the source code for the Solo_eps App.
**
*******************************************************************************/

/*
** Include Files:
*/

//#include "string.h"
//#include "solo_eps_app_events.h"

#include "solo_eps_app.h"
#include "solo_eps_app_version.h"
#include "solo_eps_app_msgids.h"
#include "solo_eps_app_perfids.h"
#include "solo_eps_device.h"
#include "cfe_error.h"

/*
** global data
*/
SOLO_EPS_AppData_t SOLO_EPS_AppData;

// Forward declarations
static int32 SOLO_EPS_AppInit(void);
static void  SOLO_EPS_ProcessCommandPacket(CFE_SB_MsgPtr_t Msg);
static void  SOLO_EPS_ProcessGroundCommand(CFE_SB_MsgPtr_t Msg);
static int32 SOLO_EPS_ReportHousekeeping(const CFE_SB_CmdHdr_t *Msg);
static int32 SOLO_EPS_ResetCounters(const SOLO_EPS_ResetCounters_t *Msg);
static int32 SOLO_EPS_Noop(const SOLO_EPS_Noop_t *Msg);
static bool SOLO_EPS_VerifyCmdLength(CFE_SB_MsgPtr_t Msg, uint16 ExpectedLength);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_AppMain()                                                    */
/* Purpose:                                                                   */
/*        Application entry point and main process loop                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void SOLO_EPS_AppMain(void)
{
    int32 status;

    /*
    ** Register the app with Executive services
    */
    CFE_ES_RegisterApp();

    /*
    ** Create the first Performance Log entry
    */
    CFE_ES_PerfLogEntry(SOLO_EPS_APP_PERF_ID);

    /*
    ** Perform application specific initialization
    ** If the Initialization fails, set the RunStatus to
    ** CFE_ES_RunStatus_APP_ERROR and the App will not enter the RunLoop
    */
    status = SOLO_EPS_AppInit();
    if (status != CFE_SUCCESS)
    {
        RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** SOLO_EPS Runloop
    */
    while (CFE_ES_RunLoop(&RunStatus) == true)
    {
        /*
        ** Performance Log Exit Stamp
        */
        CFE_ES_PerfLogExit(SOLO_EPS_APP_PERF_ID);

        /* Pend on receipt of command packet */
        status = CFE_SB_RcvMsg(&SOLO_EPS_AppData.MsgPtr, SOLO_EPS_AppData.CommandPipe, CFE_SB_PEND_FOREVER);

        /*
        ** Performance Log Entry Stamp
        */
        CFE_ES_PerfLogEntry(SOLO_EPS_APP_PERF_ID);

        if (status == CFE_SUCCESS)
        {
            SOLO_EPS_ProcessCommandPacket(SOLO_EPS_AppData.MsgPtr);
        }
        else
        {
            CFE_EVS_SendEvent(SOLO_EPS_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "SOLO_EPS APP: SB Pipe Read Error, App Will Exit");

            RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    RunStatus = CFE_ES_RunStatus_APP_EXIT; // we are wanting to exit... make sure everyone knows it

    status = SOLO_EPS_DeviceShutdown();
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Solo_eps App: Error Shutting Down Device, RC = 0x%08lX\n", (unsigned long)status);
    }

    /*
    ** Performance Log Exit Stamp
    */
    CFE_ES_PerfLogExit(SOLO_EPS_APP_PERF_ID);

    CFE_ES_ExitApp(RunStatus);

} /* End of SOLO_EPS_AppMain() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/*                                                                            */
/* Name:  SOLO_EPS_AppInit()                                                    */
/*                                                                            */
/* Purpose:                                                                   */
/*        Initialization                                                      */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
static int32 SOLO_EPS_AppInit(void)
{
    int32 status;

    RunStatus = CFE_ES_RunStatus_APP_RUN;

    /*
    ** Initialize app command execution counters
    */
    SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandCounter = 0;
    SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter = 0;

    /*
    ** Initialize app configuration data
    */
    SOLO_EPS_AppData.PipeDepth = SOLO_EPS_PIPE_DEPTH;

    /*
    ** Initialize event filter table...
    */
    SOLO_EPS_AppData.EventFilters[0].EventID = SOLO_EPS_STARTUP_INF_EID;
    SOLO_EPS_AppData.EventFilters[0].Mask    = 0x0000;
    SOLO_EPS_AppData.EventFilters[1].EventID = SOLO_EPS_COMMAND_ERR_EID;
    SOLO_EPS_AppData.EventFilters[1].Mask    = 0x0000;
    SOLO_EPS_AppData.EventFilters[2].EventID = SOLO_EPS_COMMANDNOP_INF_EID;
    SOLO_EPS_AppData.EventFilters[2].Mask    = 0x0000;
    SOLO_EPS_AppData.EventFilters[3].EventID = SOLO_EPS_COMMANDRST_INF_EID;
    SOLO_EPS_AppData.EventFilters[3].Mask    = 0x0000;
    SOLO_EPS_AppData.EventFilters[4].EventID = SOLO_EPS_INVALID_MSGID_ERR_EID;
    SOLO_EPS_AppData.EventFilters[4].Mask    = 0x0000;
    SOLO_EPS_AppData.EventFilters[5].EventID = SOLO_EPS_LEN_ERR_EID;
    SOLO_EPS_AppData.EventFilters[5].Mask    = 0x0000;
    SOLO_EPS_AppData.EventFilters[6].EventID = SOLO_EPS_PIPE_ERR_EID;
    SOLO_EPS_AppData.EventFilters[6].Mask    = 0x0000;

    /*
    ** Register the events
    */
    status = CFE_EVS_Register(SOLO_EPS_AppData.EventFilters, SOLO_EPS_EVENT_COUNTS, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Solo_eps App: Error Registering Events, RC = 0x%08lX\n", (unsigned long)status);
        return (status);
    }

    /*
    ** Initialize housekeeping packet (clear user data area).
    */
    CFE_SB_InitMsg(&SOLO_EPS_AppData.HkBuf.MsgHdr, SOLO_EPS_APP_HK_TLM_MID, sizeof(SOLO_EPS_AppData.HkBuf), true);

    /*
    ** Create Software Bus message pipe.
    */
    status = CFE_SB_CreatePipe(&SOLO_EPS_AppData.CommandPipe, SOLO_EPS_AppData.PipeDepth, "SOLO_EPS_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Solo_eps App: Error creating pipe, RC = 0x%08lX\n", (unsigned long)status);
        return (status);
    }

    /*
    ** Subscribe to Housekeeping request commands
    */
    status = CFE_SB_Subscribe(SOLO_EPS_APP_SEND_HK_MID, SOLO_EPS_AppData.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Solo_eps App: Error Subscribing to HK request, RC = 0x%08lX\n", (unsigned long)status);
        return (status);
    }

    /*
    ** Subscribe to ground command packets
    */
    status = CFE_SB_Subscribe(SOLO_EPS_APP_CMD_MID, SOLO_EPS_AppData.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Solo_eps App: Error Subscribing to Command, RC = 0x%08lX\n", (unsigned long)status);

        return (status);
    }

    status = SOLO_EPS_DeviceInit();
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Solo_eps App: Error Initializing Device, RC = 0x%08lX\n", (unsigned long)status);

        return (status);
    }

    CFE_EVS_SendEvent(SOLO_EPS_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "SOLO_EPS App Initialized. Version %d.%d.%d.%d",
                      SOLO_EPS_APP_MAJOR_VERSION,
                      SOLO_EPS_APP_MINOR_VERSION,
                      SOLO_EPS_APP_REVISION,
                      SOLO_EPS_APP_MISSION_REV);

    return (CFE_SUCCESS);

} /* End of SOLO_EPS_AppInit() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  SOLO_EPS_ProcessCommandPacket                                        */
/*                                                                            */
/*  Purpose:                                                                  */
/*     This routine will process any packet that is received on the SOLO_EPS    */
/*     command pipe.                                                          */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
static void SOLO_EPS_ProcessCommandPacket(CFE_SB_MsgPtr_t Msg)
{
    CFE_SB_MsgId_t MsgId;

    MsgId = CFE_SB_GetMsgId(Msg);

    switch (MsgId)
    {
        case SOLO_EPS_APP_CMD_MID:
            SOLO_EPS_ProcessGroundCommand(Msg);
            break;

        case SOLO_EPS_APP_SEND_HK_MID:
            SOLO_EPS_ReportHousekeeping((CFE_SB_CmdHdr_t *)Msg);
            break;

        default:
            CFE_EVS_SendEvent(SOLO_EPS_INVALID_MSGID_ERR_EID, CFE_EVS_EventType_ERROR,
                              "SOLO_EPS: invalid command packet,MID = 0x%x", (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            break;
    }

    return;

} /* End SOLO_EPS_ProcessCommandPacket */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_ProcessGroundCommand()                                       */
/*                                                                            */
/* Purpose:                                                                   */
/*        SOLO_EPS ground commands                                              */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
static void SOLO_EPS_ProcessGroundCommand(CFE_SB_MsgPtr_t Msg)
{
    uint16 CommandCode = CFE_SB_GetCmdCode(Msg);
    SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandCounter++;

    /*
    ** Process "known" SOLO_EPS app ground commands
    */
    switch (CommandCode)
    {
        case SOLO_EPS_APP_NOOP_CC:
            if (SOLO_EPS_VerifyCmdLength(Msg, sizeof(SOLO_EPS_Noop_t)))
            {
                SOLO_EPS_Noop((SOLO_EPS_Noop_t *)Msg);
            } else {
                SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
            }

            break;

        case SOLO_EPS_APP_RESET_COUNTERS_CC:
            if (SOLO_EPS_VerifyCmdLength(Msg, sizeof(SOLO_EPS_ResetCounters_t)))
            {
                SOLO_EPS_ResetCounters((SOLO_EPS_ResetCounters_t *)Msg);
            } else {
                SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
            }

            break;

        /*
        ** TODO: Edit and add more command codes as appropriate for the application
        */
        case SOLO_EPS_APP_RESET_DEV_CNTRS_CC:
            SOLO_EPS_DeviceResetCounters();
            break;

        case SOLO_EPS_GET_DEV_DATA_CC:
            if (SOLO_EPS_VerifyCmdLength(Msg, sizeof(SOLO_EPS_GetDevData_cmd_t))) {
                SOLO_EPS_DeviceGetSolo_epsDataCommand();
            } else {
                SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
            }
            break;

        case SOLO_EPS_CONFIG_CC:
            if (SOLO_EPS_VerifyCmdLength(Msg, sizeof(SOLO_EPS_Config_cmd_t)))
            {
                SOLO_EPS_DeviceConfigurationCommand(((SOLO_EPS_Config_cmd_t *)Msg)->MillisecondStreamDelay);
            } else {
                SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
            }
            break;

        case SOLO_EPS_OTHER_CMD_CC:
            if (SOLO_EPS_VerifyCmdLength(Msg, sizeof(SOLO_EPS_Other_cmd_t)))
            {
                SOLO_EPS_DeviceOtherCommand();
            } else {
                SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
            }
            break;

        case SOLO_EPS_RAW_CMD_CC:
            if (SOLO_EPS_VerifyCmdLength(Msg, sizeof(SOLO_EPS_Raw_cmd_t)))
            {
                SOLO_EPS_DeviceRawCommand(((SOLO_EPS_Raw_cmd_t *)Msg)->RawCmd, sizeof(((SOLO_EPS_Raw_cmd_t *)Msg)->RawCmd));
            } else {
                SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
            }
            break;

        case SOLO_EPS_SEND_DEV_HK_CC:
            if (SOLO_EPS_VerifyCmdLength(Msg, sizeof(SOLO_EPS_SendDevHk_cmd_t))) {
                SOLO_EPS_ReportDeviceHousekeeping();
            } else {
                SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
            }
            break;

        case SOLO_EPS_SEND_DEV_DATA_CC:
            if (SOLO_EPS_VerifyCmdLength(Msg, sizeof(SOLO_EPS_SendDevData_cmd_t))) {
                SOLO_EPS_ReportDeviceSolo_epsData();
            } else {
                SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
            }
            break;

        /* default case already found during FC vs length test */
        default:
            CFE_EVS_SendEvent(SOLO_EPS_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR, "Invalid ground command code: CC = %d",
                              CommandCode);
            SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
            break;
    }

    return;

} /* End of SOLO_EPS_ProcessGroundCommand() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  SOLO_EPS_ReportHousekeeping                                          */
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function is triggered in response to a task telemetry request */
/*         from the housekeeping task. This function will gather the Apps     */
/*         telemetry, packetize it and send it to the housekeeping task via   */
/*         the software bus                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
static int32 SOLO_EPS_ReportHousekeeping(const CFE_SB_CmdHdr_t *Msg)
{
    SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandCounter++;

    /*
    ** Send housekeeping telemetry packet...
    */
    CFE_SB_TimeStampMsg(&SOLO_EPS_AppData.HkBuf.MsgHdr);
    CFE_SB_SendMsg(&SOLO_EPS_AppData.HkBuf.MsgHdr);

    return CFE_SUCCESS;

} /* End of SOLO_EPS_ReportHousekeeping() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_Noop                                                         */
/*                                                                            */
/* Purpose:                                                                   */
/*        SOLO_EPS NOOP command                                                 */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
static int32 SOLO_EPS_Noop(const SOLO_EPS_Noop_t *Msg)
{

    CFE_EVS_SendEvent(SOLO_EPS_COMMANDNOP_INF_EID, CFE_EVS_EventType_INFORMATION, "SOLO_EPS: NOOP command");

    return CFE_SUCCESS;

} /* End of SOLO_EPS_Noop */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  SOLO_EPS_ResetCounters                                               */
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function resets all the global counter variables that are     */
/*         part of the task telemetry.                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
static int32 SOLO_EPS_ResetCounters(const SOLO_EPS_ResetCounters_t *Msg)
{

    SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandCounter = 0;
    SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter = 0;

    CFE_EVS_SendEvent(SOLO_EPS_COMMANDRST_INF_EID, CFE_EVS_EventType_INFORMATION, "SOLO_EPS: RESET command");

    return CFE_SUCCESS;

} /* End of SOLO_EPS_ResetCounters() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_VerifyCmdLength()                                            */
/*                                                                            */
/* Purpose:                                                                   */
/*        Verify command packet length                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
static bool SOLO_EPS_VerifyCmdLength(CFE_SB_MsgPtr_t Msg, uint16 ExpectedLength)
{
    bool result = true;

    uint16 ActualLength = CFE_SB_GetTotalMsgLength(Msg);

    /*
    ** Verify the command packet length.
    */
    if (ExpectedLength != ActualLength)
    {
        CFE_SB_MsgId_t MessageID   = CFE_SB_GetMsgId(Msg);
        uint16         CommandCode = CFE_SB_GetCmdCode(Msg);

        CFE_EVS_SendEvent(SOLO_EPS_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid Msg length: ID = 0x%X,  CC = %d, Len = %d, Expected = %d",
                          (unsigned int)CFE_SB_MsgIdToValue(MessageID), CommandCode, ActualLength, ExpectedLength);

        result = false;

        SOLO_EPS_AppData.HkBuf.HkTlm.Payload.CommandErrorCounter++;
    }

    return (result);

} /* End of SOLO_EPS_VerifyCmdLength() */
