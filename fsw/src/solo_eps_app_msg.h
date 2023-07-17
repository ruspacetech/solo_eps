/*******************************************************************************
** File:
**  solo_eps_app_msg.h
**
** Purpose:
**  Define Solo_eps App Messages and info
**
** Notes:
**
**
*******************************************************************************/
#ifndef _SOLO_EPS_APP_MSG_H_
#define _SOLO_EPS_APP_MSG_H_

#include "osapi.h" // for types used below
#include "cfe_sb.h" // for CFE_SB_CMD_HDR_SIZE, CFE_SB_TLM_HDR_SIZE


/*
** SOLO_EPS App command codes
*/
#define SOLO_EPS_APP_NOOP_CC            0
#define SOLO_EPS_APP_RESET_COUNTERS_CC  1
#define SOLO_EPS_GET_DEV_DATA_CC        2
#define SOLO_EPS_CONFIG_CC              3
#define SOLO_EPS_OTHER_CMD_CC           4
#define SOLO_EPS_RAW_CMD_CC             5
#define SOLO_EPS_APP_RESET_DEV_CNTRS_CC 6
#define SOLO_EPS_SEND_DEV_HK_CC         7
#define SOLO_EPS_SEND_DEV_DATA_CC       8

/*************************************************************************/

/*
** Type definition (generic "no arguments" command)
*/
typedef struct
{
    uint8 CmdHeader[CFE_SB_CMD_HDR_SIZE];

} SOLO_EPS_NoArgsCmd_t;

/*
** The following commands all share the "NoArgs" format
**
** They are each given their own type name matching the command name, which_open_mode
** allows them to change independently in the future without changing the prototype
** of the handler function
*/
typedef SOLO_EPS_NoArgsCmd_t SOLO_EPS_Noop_t;
typedef SOLO_EPS_NoArgsCmd_t SOLO_EPS_ResetCounters_t;
typedef SOLO_EPS_NoArgsCmd_t SOLO_EPS_Process_t;

typedef SOLO_EPS_NoArgsCmd_t SOLO_EPS_GetDevData_cmd_t;
typedef SOLO_EPS_NoArgsCmd_t SOLO_EPS_Other_cmd_t;
typedef SOLO_EPS_NoArgsCmd_t SOLO_EPS_SendDevHk_cmd_t;
typedef SOLO_EPS_NoArgsCmd_t SOLO_EPS_SendDevData_cmd_t;

/*
** SOLO_EPS write configuration command
*/
typedef struct
{
    uint8    CmdHeader[CFE_SB_CMD_HDR_SIZE];
    uint32   MillisecondStreamDelay;

} SOLO_EPS_Config_cmd_t;

/*
** SOLO_EPS raw command
*/
typedef struct
{
    uint8    CmdHeader[CFE_SB_CMD_HDR_SIZE];
    uint8    RawCmd[5];
} SOLO_EPS_Raw_cmd_t;

/*************************************************************************/
/*
** Type definition (SOLO_EPS App housekeeping)
*/

typedef struct
{
    uint8 CommandErrorCounter;
    uint8 CommandCounter;
} OS_PACK SOLO_EPS_HkTlm_Payload_t;

typedef struct
{
    uint8                  TlmHeader[CFE_SB_TLM_HDR_SIZE];
    SOLO_EPS_HkTlm_Payload_t Payload;

} OS_PACK SOLO_EPS_HkTlm_t;

#endif /* _SOLO_EPS_APP_MSG_H_ */

/************************/
/*  End of File Comment */
/************************/
