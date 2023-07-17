/*******************************************************************************
** File: 
**  solo_eps_app.h
**
** Purpose:
**   This file is main header file for the Solo_eps application.
**
*******************************************************************************/
#ifndef _SOLO_EPS_APP_H_
#define _SOLO_EPS_APP_H_

/*
** Required header files.
*/
#include "solo_eps_app_msg.h"
#include "solo_eps_app_events.h"
#include "cfe_sb.h"
#include "cfe_evs.h"

/***********************************************************************/
#define SOLO_EPS_PIPE_DEPTH 32 /* Depth of the Command Pipe for Application */

/************************************************************************
** Type Definitions
*************************************************************************/

/*
 * Buffer to hold telemetry data prior to sending
 * Defined as a union to ensure proper alignment for a CFE_SB_Msg_t type
 */
typedef union
{
    CFE_SB_Msg_t   MsgHdr;
    SOLO_EPS_HkTlm_t HkTlm;
} SOLO_EPS_HkBuffer_t;

/*
** Global Data
*/
typedef struct
{
    /*
    ** Housekeeping telemetry packet...
    */
    SOLO_EPS_HkBuffer_t HkBuf;

    /*
    ** Operational data (not reported in housekeeping)...
    */
    CFE_SB_PipeId_t CommandPipe;
    CFE_SB_MsgPtr_t MsgPtr;

    /*
    ** Initialization data (not reported in housekeeping)...
    */
    char   PipeName[16];
    uint16 PipeDepth;

    CFE_EVS_BinFilter_t EventFilters[SOLO_EPS_EVENT_COUNTS];

} SOLO_EPS_AppData_t;

/****************************************************************************/
/*
** Function prototypes.
**
** Note: Except for the entry point (SOLO_EPS_AppMain), these
**       functions are not called from any other source module.
*/
void  SOLO_EPS_AppMain(void);

#endif /* _solo_eps_app_h_ */

/************************/
/*  End of File Comment */
/************************/
