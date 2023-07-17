/*******************************************************************************
** File:
**  solo_eps_device.h
**
** Purpose:
**   This file is the header file for the Solo_eps device
**
**
*******************************************************************************/

#ifndef _SOLO_EPS_DEVICE_H_
#define _SOLO_EPS_DEVICE_H_

#include "solo_eps_device_msg.h"

/*
 * Buffers to hold telemetry data prior to sending
 * Defined as a union to ensure proper alignment for a CFE_SB_Msg_t type
 */

typedef union
{
    CFE_SB_Msg_t         MsgHdr;
    SOLO_EPS_DeviceHkTlm_t HkTlm;
} SOLO_EPS_DeviceHkBuffer_t;

typedef union
{
    CFE_SB_Msg_t             MsgHdr;
    SOLO_EPS_DeviceSolo_epsTlm_t Solo_epsTlm;
} SOLO_EPS_DeviceSolo_epsBuffer_t;

/*
** Run Status variable used in the main processing loop.  If the device is asynchronous, this Status
** variable is also used in the device child processing loop.
*/
extern uint32 RunStatus;

/****************************************************************************/
/*
** Function prototypes.
**
*/
int32 SOLO_EPS_DeviceInit(void);
int32 SOLO_EPS_DeviceShutdown(void);
void  SOLO_EPS_DeviceResetCounters(void);

void  SOLO_EPS_DeviceGetSolo_epsDataCommand(void);
void  SOLO_EPS_DeviceConfigurationCommand(uint32_t millisecond_stream_delay);
void  SOLO_EPS_DeviceOtherCommand(void);
void  SOLO_EPS_DeviceRawCommand(const uint8 cmd[], const uint32_t cmd_length);

void SOLO_EPS_ReportDeviceHousekeeping(void);
void SOLO_EPS_ReportDeviceSolo_epsData(void);

#endif

/************************/
/*  End of File Comment */
/************************/
