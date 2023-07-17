/*******************************************************************************
** File:
**  solo_eps_device_msg.h
**
** Purpose:
**  Define Solo_eps Device Messages and info
**
** Notes:
**
**
*******************************************************************************/
#ifndef _SOLO_EPS_DEVICE_MSG_H_
#define _SOLO_EPS_DEVICE_MSG_H_

#include "osapi.h" // for types used below
#include "cfe_sb.h" // for CFE_SB_TLM_HDR_SIZE

/*************************************************************************/
/*
** Type definition (SOLO_EPS Device housekeeping)
*/

typedef struct
{
    uint8 CommandErrorCounter;
    uint8 GetDataCmdCounter;
    uint8 CfgCmdCounter;
    uint8 OtherCmdCounter;
    uint8 RawCmdCounter;
} OS_PACK SOLO_EPS_DeviceCmdData_t;

typedef struct
{
    uint8 CfgRespCounter;
    uint8 OtherRespCounter;
    uint8 RawRespCounter;
    uint32 UnknownResponseCounter;
    uint32 DeviceSolo_epsDataCounter;
    uint32 MillisecondStreamDelay;
} OS_PACK SOLO_EPS_DeviceRespHkData_t;

typedef struct
{
    SOLO_EPS_DeviceCmdData_t    SOLO_EPS_DeviceCmdData;
    SOLO_EPS_DeviceRespHkData_t SOLO_EPS_DeviceRespHkData;
} OS_PACK SOLO_EPS_DeviceHkTlm_Payload_t;

typedef struct
{
    uint8                        TlmHeader[CFE_SB_TLM_HDR_SIZE];
    SOLO_EPS_DeviceHkTlm_Payload_t Payload;

} OS_PACK SOLO_EPS_DeviceHkTlm_t;

/*************************************************************************/
/*
** Type definition (SOLO_EPS Device Data)
*/

typedef struct
{
    uint32 Solo_epsProcessedTimeSeconds;
    uint32 Solo_epsProcessedTimeSubseconds;
    uint32 Solo_epssSent;
    uint16 Solo_epsDataX;
    uint16 Solo_epsDataY;
    uint16 Solo_epsDataZ;
} OS_PACK SOLO_EPS_DeviceRespSolo_epsData_t;

typedef struct
{
    SOLO_EPS_DeviceRespSolo_epsData_t SOLO_EPS_DeviceRespSolo_epsData;
} OS_PACK SOLO_EPS_Solo_epsTlm_Payload_t;

typedef struct
{
    uint8                      TlmHeader[CFE_SB_TLM_HDR_SIZE];
    SOLO_EPS_Solo_epsTlm_Payload_t Payload;

} OS_PACK SOLO_EPS_DeviceSolo_epsTlm_t;

#endif /* _SOLO_EPS_DEVICE_MSG_H_ */

/************************/
/*  End of File Comment */
/************************/
