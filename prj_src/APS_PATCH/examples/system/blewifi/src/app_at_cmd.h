/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#ifndef _APP_AT_CMD_H_
#define _APP_AT_CMD_H_


void app_at_cmd_add(void);


/* For Check strength of wifi */
#define TIMEOUT_WIFI_CONN_TIME                          (20000) //unit : ms 
#define MAX_WIFI_PASSWORD_LEN                           (64)

extern uint8_t g_WifiSSID[WIFI_MAX_LENGTH_OF_SSID];
extern uint8_t g_WifiPassword[MAX_WIFI_PASSWORD_LEN];
extern uint8_t g_WifiSSIDLen;
extern uint8_t g_WifiPasswordLen;
extern osTimerId g_tAppFactoryWifiConnTimerId;


#endif /* _APP_AT_CMD_H_ */

