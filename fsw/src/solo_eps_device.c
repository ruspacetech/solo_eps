/*******************************************************************************
** File:
**  solo_eps_device.c
**
** Purpose:
**   This file includes all of the code for interacting with the solo_eps
**   hardware device.  This includes maintaining status information about the
**   device and sending data to and receiving data from the device.
**   It provides the functions to support commands that were sent to the 
**   main solo_eps app but are destined for commanding the device.  It also
**   provides functions to support reporting data and telemetry from and about
**   the device.
**
** Note:  There are basically two models for interacting with a device:
** 1.  The macro ASYNCHRONOUS can be defined or not defined to switch between
**     the two models below.  If you select one of these models for your
**     device, you probably want to remove the macro, the macro protections,
**     and delete the code for the other model.
** 2.  Synchronously, i.e. send data/command to device, receive data back from
**     device.  In this scenario, the main app code and the device code are in
**     a single task. (e.g. a USART, I2C, SPI, or other synchronous 
**     communicating device)
** 3.  Asynchronously, i.e. the device can send data at any time (e.g. if
**     it can stream data), so the device code must be ready for it at  
**     any time.  For this reason, in this scenario, much of the 
**     device code (the code that responds to data) executes 
**     in a child task of the main app.  However, the functions that
**     support commands to the device still execute in the main app context, as
**     do the functions that return status type data about the device.  Any
**     shared state about the device must be properly mutexed for sharing
**     between the two tasks.  (e.g. a UART device or other asynchronous
**     communicating device)
**
**
*******************************************************************************/

#include "solo_eps_device.h"
#include "solo_eps_app_msgids.h"
#include "solo_eps_app_events.h"
#include "solo_eps_app_platform_cfg.h"
#include "hwlib.h"

#define ASYNCHRONOUS // asynchronous = streaming

/*
** global data - handle with care if the device is asychronous, and so there are multiple tasks in here
*/

/*
** Run Status variable used in the main processing loop.  If the device is asynchronous, this Status
** variable is also used in the device child processing loop.
*/
uint32 RunStatus;

static SOLO_EPS_DeviceHkBuffer_t SOLO_EPS_DeviceHkBuffer;
static SOLO_EPS_DeviceSolo_epsBuffer_t SOLO_EPS_DeviceSolo_epsBuffer;

static int32_t handle;

#ifdef ASYNCHRONOUS
    static uint32 DeviceMutex; /* Locks device data and protocol */
    static uint32 DeviceID;    /* Device ID provided by CFS on initialization */
    #define DEV_MUTEX_TAKE OS_MutSemTake(DeviceMutex);
    #define DEV_MUTEX_GIVE OS_MutSemGive(DeviceMutex);
    static void SOLO_EPS_DeviceBlockingReadAndProcessData(void); // Forward declaration
#else // synchronous, no mutex needed
    #define DEV_MUTEX_TAKE
    #define DEV_MUTEX_GIVE
    static void SOLO_EPS_DeviceNonblockingReadAndProcessData(void); // Forward declaration
#endif

#ifdef ASYNCHRONOUS
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceMain()                                                 */
/* Purpose:                                                                   */
/*        Device child task entry point and main process loop                 */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
static void SOLO_EPS_DeviceMain(void)
{
    /*
    ** Register the device task with Executive Services
    */
    int32 status = CFE_ES_RegisterChildTask();
    if(status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(SOLO_EPS_DEVICE_REG_ERR_EID, CFE_EVS_ERROR, "SOLO_EPS: Register device task error %d", status);
        CFE_ES_ExitChildTask();
        return;
    }
    else
    {
        CFE_EVS_SendEvent(SOLO_EPS_DEVICE_REG_INF_EID, CFE_EVS_INFORMATION, "SOLO_EPS: Device task registration complete");
    }

    /*
    ** Device Run Loop
    */
    // N.B.!!  Not locking RunStatus... never setting it in the child... if we get an inconsistent read, later it will be consistent
    while (RunStatus == CFE_ES_RunStatus_APP_RUN)
    {
        SOLO_EPS_DeviceBlockingReadAndProcessData();
    }

    /*
    ** Clean up mutex
    */
   if (OS_MutSemGetIdByName(&DeviceMutex, SOLO_EPS_DEVICE_MUTEX_NAME) == OS_SUCCESS)
   {
       OS_MutSemDelete(DeviceMutex);
   }

} /* End of SOLO_EPS_DeviceMain() */
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceInit()                                                 */
/* Purpose:                                                                   */
/*        Initialize the device                                               */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
int32 SOLO_EPS_DeviceInit(void)
{
    int32 status;

    // Initialize message ID and clear user data area of device housekeeping and device solo_eps packets
    CFE_SB_InitMsg(&SOLO_EPS_DeviceHkBuffer.MsgHdr, SOLO_EPS_APP_SEND_DEV_HK_MID, sizeof(SOLO_EPS_DeviceHkBuffer), true); // no lock needed, only one task atm
    CFE_SB_InitMsg(&SOLO_EPS_DeviceSolo_epsBuffer.MsgHdr, SOLO_EPS_APP_SEND_DEV_DATA_MID, sizeof(SOLO_EPS_DeviceSolo_epsBuffer), true); // no lock needed, only one task atm

    uart_info_t Solo_epsUart;
    Solo_epsUart.deviceString = SOLO_EPS_CFG_STRING;
    Solo_epsUart.handle = handle = SOLO_EPS_CFG_HANDLE;
    Solo_epsUart.isOpen = PORT_CLOSED;
    Solo_epsUart.baud = SOLO_EPS_CFG_BAUDRATE_HZ;

    status = uart_init_port(&Solo_epsUart);
    if (status != UART_SUCCESS)
    {
        CFE_EVS_SendEvent(SOLO_EPS_UART_ERR_EID, CFE_EVS_ERROR, "SOLO_EPS: UART port initialization error %d", status);
        return status;
    }

#ifdef ASYNCHRONOUS
    /*
    ** Create device mutex for shared variables and RunStatus
    */
    status = OS_MutSemCreate(&DeviceMutex, SOLO_EPS_DEVICE_MUTEX_NAME, 0);
    if (status != OS_SUCCESS)
    {
        CFE_EVS_SendEvent(SOLO_EPS_MUTEX_ERR_EID, CFE_EVS_ERROR, "SOLO_EPS: Create device mutex error %d", status);
        return status;
    }

    /* 
    ** Create device task
    */
    status = CFE_ES_CreateChildTask(&DeviceID,
                                    SOLO_EPS_DEVICE_NAME,
                                    (void *) SOLO_EPS_DeviceMain, 0,
                                    SOLO_EPS_DEVICE_CHILD_STACK_SIZE,
                                    SOLO_EPS_DEVICE_CHILD_PRIORITY, 0);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(SOLO_EPS_CREATE_DEVICE_ERR_EID, CFE_EVS_ERROR, "SOLO_EPS: Create device task error %d", status);
        return status;
    }

#endif

    return CFE_SUCCESS;
} /* End of SOLO_EPS_DeviceInit() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceShutdown()                                             */
/* Purpose:                                                                   */
/*        Shut down the device                                                */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
int32 SOLO_EPS_DeviceShutdown(void)
{
    return uart_close_port(handle);
} /* End of SOLO_EPS_DeviceShutdown() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceCommand()                                              */
/* Purpose:                                                                   */
/*        Write a generic command (specified by cmd bytes) to the device      */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
static int32 SOLO_EPS_DeviceCommand(const uint8 cmd[], const uint32_t cmd_length)
{
    int32 status;
    int32_t bytes = uart_write_port(handle, cmd, cmd_length);
    if (bytes != (int32_t)cmd_length)
    {
        SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.CommandErrorCounter++;
        CFE_EVS_SendEvent(SOLO_EPS_UART_WRITE_ERR_EID, CFE_EVS_ERROR, "SOLO_EPS: Command uart write error, expected %u and returned %d", cmd_length, bytes);
        status = OS_ERROR;
        return status;
    }
    return CFE_SUCCESS;
} /* End of SOLO_EPS_DeviceCommand() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceGetSolo_epsDataCommand()                                 */
/* Purpose:                                                                   */
/*        Formulate the "get solo_eps data" command bytes and send the command  */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void SOLO_EPS_DeviceGetSolo_epsDataCommand(void)
{
    uint8 cmd_bytes[13];
    cmd_bytes[ 0] = 0xDE;
    cmd_bytes[ 1] = 0xAD;
    cmd_bytes[ 2] = 's';
    cmd_bytes[ 3] = 'a';
    cmd_bytes[ 4] = 'm';
    cmd_bytes[ 5] = 'p';
    cmd_bytes[ 6] = 0x01; // send data command
    // 7, 8, 9, 10 ignored for this command
    cmd_bytes[11] = 0xBE;
    cmd_bytes[12] = 0xEF;

    DEV_MUTEX_TAKE
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.GetDataCmdCounter++;
    int32 status = SOLO_EPS_DeviceCommand(cmd_bytes, sizeof(cmd_bytes)/sizeof(uint8));
    if (status != CFE_SUCCESS) {
        SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.CommandErrorCounter++;
    }
    DEV_MUTEX_GIVE
    
#ifndef ASYNCHRONOUS
    SOLO_EPS_DeviceNonblockingReadAndProcessData();
#endif

} /* End of SOLO_EPS_DeviceGetSolo_epsDataCommand() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceConfigurationCommand()                                 */
/* Purpose:                                                                   */
/*        Formulate the "configure device" command bytes and send the command */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void SOLO_EPS_DeviceConfigurationCommand(uint32_t millisecond_stream_delay)
{
    uint8 cmd_bytes[13];
    cmd_bytes[ 0] = 0xDE;
    cmd_bytes[ 1] = 0xAD;
    cmd_bytes[ 2] = 's';
    cmd_bytes[ 3] = 'a';
    cmd_bytes[ 4] = 'm';
    cmd_bytes[ 5] = 'p';
    cmd_bytes[ 6] = 0x02; // configure stream delay command
    cmd_bytes[ 7] = (uint8)((millisecond_stream_delay >> 24) & 0x000000FF);
    cmd_bytes[ 8] = (uint8)((millisecond_stream_delay >> 16) & 0x000000FF);
    cmd_bytes[ 9] = (uint8)((millisecond_stream_delay >>  8) & 0x000000FF);
    cmd_bytes[10] = (uint8)((millisecond_stream_delay      ) & 0x000000FF);
    cmd_bytes[11] = 0xBE;
    cmd_bytes[12] = 0xEF;

    DEV_MUTEX_TAKE
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.CfgCmdCounter++;
    int32 status = SOLO_EPS_DeviceCommand(cmd_bytes, sizeof(cmd_bytes)/sizeof(uint8));
    if (status != CFE_SUCCESS) {
        SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.CommandErrorCounter++;
    }
    DEV_MUTEX_GIVE

#ifndef ASYNCHRONOUS
    SOLO_EPS_DeviceNonblockingReadAndProcessData();
#endif

} /* End of SOLO_EPS_DeviceConfigurationCommand() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceOtherCommand()                                         */
/* Purpose:                                                                   */
/*        Formulate the "other command" command bytes and send the command    */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void SOLO_EPS_DeviceOtherCommand(void)
{
    uint8 cmd_bytes[13];
    cmd_bytes[ 0] = 0xDE;
    cmd_bytes[ 1] = 0xAD;
    cmd_bytes[ 2] = 's';
    cmd_bytes[ 3] = 'a';
    cmd_bytes[ 4] = 'm';
    cmd_bytes[ 5] = 'p';
    cmd_bytes[ 6] = 0x03; // other command
    // 7, 8, 9, 10 ignored for this command
    cmd_bytes[11] = 0xBE;
    cmd_bytes[12] = 0xEF;

    DEV_MUTEX_TAKE
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.OtherCmdCounter++;
    int32 status = SOLO_EPS_DeviceCommand(cmd_bytes, sizeof(cmd_bytes)/sizeof(uint8));
    if (status != CFE_SUCCESS) {
        SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.CommandErrorCounter++;
    }
    DEV_MUTEX_GIVE

#ifndef ASYNCHRONOUS
    SOLO_EPS_DeviceNonblockingReadAndProcessData();
#endif

} /* End of SOLO_EPS_DeviceOtherCommand() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceRawCommand()                                           */
/* Purpose:                                                                   */
/*        Formulate the "raw" command bytes and send the command              */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void SOLO_EPS_DeviceRawCommand(const uint8 cmd[], const uint32_t cmd_length)
{
    int32 status = CFE_SUCCESS;

    DEV_MUTEX_TAKE
    if (cmd_length != 5)
    {
        CFE_EVS_SendEvent(SOLO_EPS_COMMANDRAW_INF_EID, CFE_EVS_ERROR, "SOLO_EPS: Raw command error.  Expected length 5, command was length %u", cmd_length);
    } else {
        uint8 cmd_bytes[13];
        cmd_bytes[ 0] = 0xDE;
        cmd_bytes[ 1] = 0xAD;
        cmd_bytes[ 2] = 's';
        cmd_bytes[ 3] = 'a';
        cmd_bytes[ 4] = 'm';
        cmd_bytes[ 5] = 'p';
        cmd_bytes[ 6] = cmd[0]; // Embed the raw command
        cmd_bytes[ 7] = cmd[1]; // Embed the raw command
        cmd_bytes[ 8] = cmd[2]; // Embed the raw command
        cmd_bytes[ 9] = cmd[3]; // Embed the raw command
        cmd_bytes[10] = cmd[4]; // Embed the raw command
        cmd_bytes[11] = 0xBE;
        cmd_bytes[12] = 0xEF;

        SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.RawCmdCounter++;
        int32 status = SOLO_EPS_DeviceCommand(cmd_bytes, sizeof(cmd_bytes)/sizeof(uint8));
    }
    
    if (status != CFE_SUCCESS) {
        SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.CommandErrorCounter++;
    }
    DEV_MUTEX_GIVE

#ifndef ASYNCHRONOUS
    SOLO_EPS_DeviceNonblockingReadAndProcessData();
#endif

} /* End of SOLO_EPS_DeviceRawCommand() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceResetCounters()                                        */
/* Purpose:                                                                   */
/*        Reset counters about interaction with the device                    */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void SOLO_EPS_DeviceResetCounters(void)
{
    DEV_MUTEX_TAKE
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.CommandErrorCounter = 0;
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.GetDataCmdCounter = 0;
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.CfgCmdCounter = 0;
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.OtherCmdCounter = 0;
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceCmdData.RawCmdCounter = 0;
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.CfgRespCounter = 0;
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.OtherRespCounter = 0;
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.RawRespCounter = 0;
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.UnknownResponseCounter = 0;
    SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.DeviceSolo_epsDataCounter = 0;
    DEV_MUTEX_GIVE

    CFE_EVS_SendEvent(SOLO_EPS_CMD_DEVRST_INF_EID, CFE_EVS_EventType_INFORMATION, "SOLO_EPS: RESET Device counters command");

} /* End of SOLO_EPS_DeviceResetCounters() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_ReportDeviceHousekeeping()                                   */
/* Purpose:                                                                   */
/*        Report housekeeping data about the device                           */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void SOLO_EPS_ReportDeviceHousekeeping(void)
{
    DEV_MUTEX_TAKE
    CFE_SB_TimeStampMsg(&SOLO_EPS_DeviceHkBuffer.MsgHdr);
    CFE_SB_SendMsg(&SOLO_EPS_DeviceHkBuffer.MsgHdr);
    DEV_MUTEX_GIVE
} /* End of SOLO_EPS_ReportDeviceHousekeeping() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_ReportDeviceSolo_epsData()                                     */
/* Purpose:                                                                   */
/*        Report solo_eps data (previously received) from the device            */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void SOLO_EPS_ReportDeviceSolo_epsData(void)
{
    DEV_MUTEX_TAKE
    CFE_SB_TimeStampMsg(&SOLO_EPS_DeviceSolo_epsBuffer.MsgHdr);
    CFE_SB_SendMsg(&SOLO_EPS_DeviceSolo_epsBuffer.MsgHdr);
    DEV_MUTEX_GIVE
} /* End of SOLO_EPS_ReportDeviceSolo_epsData() */

static void process_bytes_received(const uint8 response[], const uint32_t response_length); // Forward declaration

#ifdef ASYNCHRONOUS
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceBlockingReadAndProcessData()                           */
/* Purpose:                                                                   */
/*        Block data coming from the device.  When data arrives, process it.  */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
static void SOLO_EPS_DeviceBlockingReadAndProcessData(void)
{
    uint8 response[SOLO_EPS_UART_BUFFER_SIZE];
    uint32 max_response_length = sizeof(response)/sizeof(uint8);

    int bytes = 1;
    bytes = uart_read_port(handle, response, bytes); // block on reading one byte... depends on uart_read_port blocking when nothing to read and no error
    if (bytes > 0) {
        bytes = uart_bytes_available(handle); // how many more are there?
        if (bytes > (int32)(max_response_length-1)) {
            CFE_EVS_SendEvent(SOLO_EPS_UART_READ_ERR_EID, CFE_EVS_ERROR, 
                "SOLO_EPS: Device read error.  Somehow the UART had more bytes %d than the buffer holds %u.  Only reading %u bytes.", 
                bytes+1, max_response_length, max_response_length);
            bytes = max_response_length;
        }
        // else... we are good... the buffer is big enough
        if (bytes > 0) {
            bytes = uart_read_port(handle, &(response[1]), bytes); // read the more into the buffer after the first byte
        }
        // else:  there was only the first byte to read
        if (bytes >= 0) { // if this is >=0, we at least read the one byte we blocked on... process the bytes
            process_bytes_received(response, bytes);
        } else { // hmmm... we got an error on the second read
            CFE_EVS_SendEvent(SOLO_EPS_UART_READ_ERR_EID, CFE_EVS_ERROR, 
                "SOLO_EPS: Device read error.  uart_read_port returned %d.", bytes);       
        }
    } else if (bytes < 0) {
        CFE_EVS_SendEvent(SOLO_EPS_UART_READ_ERR_EID, CFE_EVS_ERROR, 
            "SOLO_EPS: Device read error.  uart_read_port returned %d.", bytes);
    } 
    // else:  kind of confusing... do nothing... we should be blocking, so there should be no need for an else (no 0 byte read possible)

} /* End of SOLO_EPS_DeviceBlockingReadAndProcessData() */
#else
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  SOLO_EPS_DeviceNonblockingReadAndProcessData()                        */
/* Purpose:                                                                   */
/*        See if any data is available from the device.  If it is, process it.*/
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
static void SOLO_EPS_DeviceNonblockingReadAndProcessData(void)
{
    uint8 response[SOLO_EPS_UART_BUFFER_SIZE];
    uint32 max_response_length = sizeof(response)/sizeof(uint8);

    int bytes = uart_bytes_available(handle);
    if (bytes > (int32)max_response_length) {
        CFE_EVS_SendEvent(SOLO_EPS_UART_READ_ERR_EID, CFE_EVS_ERROR, 
            "SOLO_EPS: Device read error.  Somehow the UART had more bytes %d than the buffer holds %u.  Only reading %u bytes.", 
            bytes, max_response_length, max_response_length);
        bytes = max_response_length;
    }
    if (bytes > 0) { // there is stuff to read and process... do it
        bytes = uart_read_port(handle, response, bytes);
        if (bytes > 0) {
            process_bytes_received(response, bytes);
        } else if (bytes < 0) {
            CFE_EVS_SendEvent(SOLO_EPS_UART_READ_ERR_EID, CFE_EVS_ERROR, 
                "SOLO_EPS: Device read error.  uart_read_port returned %d.", bytes);
        } 
        // else:  kind of confusing... do nothing... we thought we had bytes to read... but we did not
    } // otherwise... nothing to read... do nothing

} /* End of SOLO_EPS_DeviceNonblockingReadAndProcessData() */
#endif

static uint32_t find_next_dead_index(const uint32_t index, const uint8 response[], const uint32_t response_length); // Forward declaration
static uint32_t process_message_at_index(uint32_t index, const uint8 response[], const uint32_t response_length); // Forward declaration
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  process_bytes_received()                                            */
/* Purpose:                                                                   */
/*        Process a bunch of bytes from the device.  Try to find zero to many */
/*        messages in the data.  Throw out bytes that are not part of a       */
/*        message so we can resync on the expected message header.            */
/*        Note that this processing is very specific to the protocol provided */
/*        by the device.                                                      */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
static void process_bytes_received(const uint8 response[], const uint32_t response_length)
{
    uint32_t index = 0;
    uint32_t msgs_processed = 0;

    while(index < response_length) { // keep going in case multiple messages were sent to the UART before we read it
        index = find_next_dead_index(index, response, response_length);
        if (index < response_length) {
            index = process_message_at_index(index, response, response_length); // found the start of a message... figure out what it is
            msgs_processed++;
        }
    }
    CFE_EVS_SendEvent(SOLO_EPS_UART_MSG_CNT_DBG_EID, CFE_EVS_DEBUG, 
        "SOLO_EPS:  Processed %u messages from the %u bytes received.", msgs_processed, response_length);
} /* End of process_bytes_received() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  find_next_dead_index()                                              */
/* Purpose:                                                                   */
/*        Find the byte index in the buffer where the message header (the     */
/*        bytes 0xDE 0xAD) begin.  Start at the input index and search from   */
/*        there forward.  Again, very specific to the device protocol.        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
static uint32_t find_next_dead_index(const uint32_t index, const uint8 response[], const uint32_t response_length)
{
    for (uint32_t i = index; i < response_length; i++) {
        if ((response[i] == 0xDE) && (response[i+1] == 0xAD)) {
            return i;
        }
    }
    return response_length;
} /* End of find_next_dead_index() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Name:  process_message_at_index()                                          */
/* Purpose:                                                                   */
/*        Process the message whose header was found to begin at index.       */
/*        For this device, messages have a trailer (the bytes 0xBE 0xEF).     */
/*        Again, very specific to the device protocol.                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
static uint32_t process_message_at_index(uint32_t index, const uint8 response[], const uint32_t response_length)
{
    DEV_MUTEX_TAKE
    if ((response[index+11] == 0xBE) && (response[index+12] == 0xEF)) { // this is a command response message from the device
        if (response[index+6] == 0x02) {
            SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.CfgRespCounter++;
            SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.MillisecondStreamDelay = 
                (response[index+ 7] << 24) + // positive set of this data based on real device cfg
                (response[index+ 8] << 16) +
                (response[index+ 9] <<  8) +
                (response[index+10]      );
        } else if (response[index+6] == 0x03) {
            SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.OtherRespCounter++;
        } else { // assume it was a raw command
            SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.RawRespCounter++;
        }
        index += 13; // increment index to be after the end of the message
    } else if ((response[index+12] == 0xBE) && (response[index+13] == 0xEF)) { // this is a data message from the device
        SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.DeviceSolo_epsDataCounter++;
        CFE_TIME_SysTime_t time = CFE_TIME_GetTime();
        SOLO_EPS_DeviceSolo_epsBuffer.Solo_epsTlm.Payload.SOLO_EPS_DeviceRespSolo_epsData.Solo_epsProcessedTimeSeconds = time.Seconds;
        SOLO_EPS_DeviceSolo_epsBuffer.Solo_epsTlm.Payload.SOLO_EPS_DeviceRespSolo_epsData.Solo_epsProcessedTimeSubseconds = time.Subseconds;
        SOLO_EPS_DeviceSolo_epsBuffer.Solo_epsTlm.Payload.SOLO_EPS_DeviceRespSolo_epsData.Solo_epssSent = 
            (response[index+ 2] << 24) + 
            (response[index+ 3] << 16) + 
            (response[index+ 4] <<  8) + 
            (response[index+ 5]);
        SOLO_EPS_DeviceSolo_epsBuffer.Solo_epsTlm.Payload.SOLO_EPS_DeviceRespSolo_epsData.Solo_epsDataX = (response[index+ 6] << 8) + (response[index+ 7]);
        SOLO_EPS_DeviceSolo_epsBuffer.Solo_epsTlm.Payload.SOLO_EPS_DeviceRespSolo_epsData.Solo_epsDataY = (response[index+ 8] << 8) + (response[index+ 9]);
        SOLO_EPS_DeviceSolo_epsBuffer.Solo_epsTlm.Payload.SOLO_EPS_DeviceRespSolo_epsData.Solo_epsDataZ = (response[index+10] << 8) + (response[index+11]);

        // If we want to send the solo_eps data every time we receive it:
        SOLO_EPS_ReportDeviceSolo_epsData();

        index += 14; // increment index to be after the end of the message
    } else {
        SOLO_EPS_DeviceHkBuffer.HkTlm.Payload.SOLO_EPS_DeviceRespHkData.UnknownResponseCounter++;
        index += 1; // Found nothing... increment index to try again next time at the next position!
    }
    DEV_MUTEX_GIVE

    return index;
} /* End of process_message_at_index() */

/************************/
/*  End of File Comment */
/************************/
