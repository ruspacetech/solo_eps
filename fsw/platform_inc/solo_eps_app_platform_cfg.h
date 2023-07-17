/************************************************************************
** File:
**  solo_eps_app_platform_cfg.h
**
** Purpose: 
**  Define Solo_eps App platform configuration parameters
**
** Notes:
**
**
*************************************************************************/
#ifndef _SOLO_EPS_APP_PLATFORM_CFG_H_
#define _SOLO_EPS_APP_PLATFORM_CFG_H_

#define SOLO_EPS_UART_BUFFER_SIZE 1024

#define SOLO_EPS_DEVICE_NAME       "SOLO_EPS_DEVICE"
/* Note: Solo_eps child task stack and priority get defined here. Main app is specified in cfe_es_startup.scr */
#define SOLO_EPS_DEVICE_CHILD_STACK_SIZE  2048
#define SOLO_EPS_DEVICE_CHILD_PRIORITY    100
#define SOLO_EPS_DEVICE_MUTEX_NAME "SOLO_EPS_DEVICE_MUTEX"

/*
** Default Solo_eps Configuration
** Note: In practice this configuration should be specified in the mission_defs/cpuX_device_cfg.h
**       These parameters are for the I/O to the device and can change per platform (NOS3 vs. flight vs. flatsat)
**       The definitions here will only be used if SOLO_EPS_CFG is not in the cpuX_device_cfg.h 
*/
#ifndef SOLO_EPS_CFG
    /* Note: NOS3 uart requires matching handle and bus number */
    #define SOLO_EPS_CFG_STRING           "usart_29"
    #define SOLO_EPS_CFG_HANDLE           29 
    #define SOLO_EPS_CFG_BAUDRATE_HZ      115200
    #define SOLO_EPS_CFG_MS_TIMEOUT       250
#endif

#endif /* _SOLO_EPS_PLATFORM_CFG_H_ */

/************************/
/*  End of File Comment */
/************************/
