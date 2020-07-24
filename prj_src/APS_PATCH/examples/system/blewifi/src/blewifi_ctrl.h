/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

/**
 * @file blewifi_ctrl.h
 * @author Vincent Chen, Michael Liao
 * @date 20 Feb 2018
 * @brief File includes the function declaration of blewifi ctrl task.
 *
 */

#ifndef __BLEWIFI_CTRL_H__
#define __BLEWIFI_CTRL_H__

#include <stdint.h>
#include <stdbool.h>
#include "blewifi_configuration.h"
#include "mw_fim_default_group11_project.h"
#include "mw_fim_default_group12_project.h"
#include "cmsis_os.h"
#include "event_groups.h"

#define BLEWIFI_CTRL_QUEUE_SIZE         (20)
#define SLEEP_TIMER_ISSUE

#define SHORT_TRIG          1
#define DOOR_CLOSE          2
#define DOOR_OPEN           3
#define TIMER_POST          4
#define DOUBLE_SHORT_TRIG   5

#define AverageTimes              (100)

extern osTimerId    g_tAppCtrlSysTimer;
extern osTimerId    g_tAppCtrlTestModeId;

typedef enum blewifi_ctrl_msg_type
{
    /* BLE Trigger */
    BLEWIFI_CTRL_MSG_BLE_INIT_COMPLETE = 0,             //BLE report status
    BLEWIFI_CTRL_MSG_BLE_ADVERTISING_CFM,               //BLE report status
    BLEWIFI_CTRL_MSG_BLE_ADVERTISING_EXIT_CFM,          //BLE report status
    BLEWIFI_CTRL_MSG_BLE_ADVERTISING_TIME_CHANGE_CFM,   //BLE report status
    BLEWIFI_CTRL_MSG_BLE_CONNECTION_COMPLETE,           //BLE report status
    BLEWIFI_CTRL_MSG_BLE_CONNECTION_FAIL,               //BLE report status
    BLEWIFI_CTRL_MSG_BLE_DISCONNECT,                    //BLE report status
    BLEWIFI_CTRL_MSG_BLE_DATA_IND,                      //BLE receive the data from peer to device

    BLEWIFI_CTRL_MSG_BLE_NUM,

    /* Wi-Fi Trigger */
    BLEWIFI_CTRL_MSG_WIFI_INIT_COMPLETE = 0x80, //Wi-Fi report status
    BLEWIFI_CTRL_MSG_WIFI_RESET_DEFAULT_IND,    //Wi-Fi report status
    BLEWIFI_CTRL_MSG_WIFI_SCAN_DONE_IND,        //Wi-Fi report status
    BLEWIFI_CTRL_MSG_WIFI_CONNECTION_IND,       //Wi-Fi report status
    BLEWIFI_CTRL_MSG_WIFI_DISCONNECTION_IND,    //Wi-Fi report status
    BLEWIFI_CTRL_MSG_WIFI_GOT_IP_IND,           //Wi-Fi report status
    BLEWIFI_CTRL_MSG_WIFI_AUTO_CONNECT_IND,     //Wi-Fi the auto connection is triggered by timer

    BLEWIFI_CTRL_MSG_WIFI_NUM,

    /* Others Event */
    BLEWIFI_CTRL_MSG_OTHER_OTA_ON = 0x100,      //OTA
    BLEWIFI_CTRL_MSG_OTHER_OTA_OFF,             //OTA success
    BLEWIFI_CTRL_MSG_OTHER_OTA_OFF_FAIL,        //OTA fail

#if 0
    BLEWIFI_CTRL_MSG_BUTTON_STATECHANGE,       //Button Stage Change
    BLEWIFI_CTRL_MSG_BUTTON_DEBOUNCETIMEOUT,   //Button Debounce Time Out
    BLEWIFI_CTRL_MSG_BUTTON_BLEADVTIMEOUT,     //Button press more then 5 second, then timer start to count down. If time out then ble adv stop.
    BLEWIFI_CTRL_MSG_BUTTON_LONG_PRESS_TIMEOUT,
#endif
#if (BLEWIFI_CTRL_BUTTON_SENSOR_EN == 1)
    BLEWIFI_CTRL_MSG_BUTTON_STATE_CHANGE,           //Button Stage Change
    BLEWIFI_CTRL_MSG_BUTTON_DEBOUNCE_TIMEOUT,       //Button Debounce Time Out
    BLEWIFI_CTRL_MSG_BUTTON_RELEASE_TIMEOUT,        //Button Release Time Out
#endif
    BLEWIFI_CTRL_MSG_DOOR_STATECHANGE,
    BLEWIFI_CTRL_MSG_DOOR_DEBOUNCETIMEOUT,
    BLEWIFI_CTRL_MSG_BLE_ADV_TIMEOUT,               //Button press more then 5 second, then timer start to count down. If time out then ble adv stop.

    BLEWIFI_CTRL_MSG_HTTP_POST_DATA_IND,
    BLEWIFI_CTRL_MSG_HTTP_POST_DATA_TYPE1_2_3_RETRY,

    BLEWIFI_CTRL_MSG_OTHER_LED_TIMER,
    BLEWIFI_CTRL_MSG_OTHER_SYS_TIMER,          //SYS timer

    BLEWIFI_CTRL_MSG_NETWORKING_START,
    BLEWIFI_CTRL_MSG_NETWORKING_STOP,

    BLEWIFI_CTRL_MSG_IS_TEST_MODE_AT_FACTORY,
    BLEWIFI_CTRL_MSG_IS_TEST_MODE_TIMEOUT,

    BLEWIFI_CTRL_MSG_OTHER__NUM
} blewifi_ctrl_msg_type_e;

typedef struct
{
    uint32_t event;
    uint32_t length;
    uint8_t ucaMessage[];
} xBleWifiCtrlMessage_t;

typedef enum blewifi_ctrl_led_state
{
    BLEWIFI_CTRL_LED_BLE_ON_1 = 0x00,
    BLEWIFI_CTRL_LED_BLE_OFF_1,

    BLEWIFI_CTRL_LED_AUTOCONN_ON_1,
    BLEWIFI_CTRL_LED_AUTOCONN_OFF_1,

    BLEWIFI_CTRL_LED_OTA_ON,
    BLEWIFI_CTRL_LED_OTA_OFF,

    BLEWIFI_CTRL_LED_ALWAYS_OFF,

    BLEWIFI_CTRL_LED_TEST_MODE_ON_1,
    BLEWIFI_CTRL_LED_TEST_MODE_OFF_1,
    BLEWIFI_CTRL_LED_TEST_MODE_ON_2,
    BLEWIFI_CTRL_LED_TEST_MODE_OFF_2,
    BLEWIFI_CTRL_LED_TEST_MODE_ON_3,
    BLEWIFI_CTRL_LED_TEST_MODE_OFF_3,
    BLEWIFI_CTRL_LED_TEST_MODE_ON_4,
    BLEWIFI_CTRL_LED_TEST_MODE_OFF_4,

    BLEWIFI_CTRL_LED_NOT_CNT_SRV_ON_1,
    BLEWIFI_CTRL_LED_NOT_CNT_SRV_OFF_1,
    BLEWIFI_CTRL_LED_NOT_CNT_SRV_ON_2,
    BLEWIFI_CTRL_LED_NOT_CNT_SRV_OFF_2,

    BLEWIFI_CTRL_LED_OFFLINE_ON_1,
    BLEWIFI_CTRL_LED_OFFLINE_OFF_1,
#if 1   // 20200528, Terence change offline led as same as NOT_CNT_SRV
    BLEWIFI_CTRL_LED_OFFLINE_ON_2,
    BLEWIFI_CTRL_LED_OFFLINE_OFF_2,
#endif

    BLEWIFI_CTRL_LED_SHORT_PRESS_ON,//Goter

    BLEWIFI_CTRL_LED_BOOT_ON_1,//Goter
    BLEWIFI_CTRL_LED_BOOT_OFF_1,//Goter
    BLEWIFI_CTRL_LED_BOOT_ON_2,//Goter

    BLEWIFI_CTRL_LED_NUM
} blewifi_ctrl_led_state_e;

typedef enum blewifi_ctrl_sys_state
{
    BLEWIFI_CTRL_SYS_INIT = 0x00,       // PS(0), Wifi(1), Ble(1)
    BLEWIFI_CTRL_SYS_NORMAL,            // PS(1), Wifi(1), Ble(1)
    BLEWIFI_CTRL_SYS_BLE_OFF,           // PS(1), Wifi(1), Ble(0)

    BLEWIFI_CTRL_SYS_NUM
} blewifi_ctrl_sys_state_e;

// event group bit (0 ~ 23 bits)
#define BLEWIFI_CTRL_EVENT_BIT_BLE      0x00000001U
#define BLEWIFI_CTRL_EVENT_BIT_WIFI     0x00000002U
#define BLEWIFI_CTRL_EVENT_BIT_OTA      0x00000004U
#define BLEWIFI_CTRL_EVENT_BIT_GOT_IP   0x00000008U
#define BLEWIFI_CTRL_EVENT_BIT_IOT_INIT 0x00000010U
#define BLEWIFI_CTRL_EVENT_BIT_NETWORK  0x00000020U

#ifdef SLEEP_TIMER_ISSUE
// When button or key press timeout, system will trigger button event occasionally.
#define BLEWIFI_CTRL_EVENT_BIT_BUTTON_PRESS    0x00010000U  // Workaround: button timeout retrigger
#endif
#define BLEWIFI_CTRL_EVENT_BIT_TEST_MODE       0x00020000U  // Test Mode
#define BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE    0x00040000U  // AT+WIFI Mode
#define BLEWIFI_CTRL_EVENT_BIT_OTA_MODE        0x00080000U  // OTA Mode
#define BLEWIFI_CTRL_EVENT_BIT_DOOR            0x00100000U  // Door (Key) Status
#define BLEWIFI_CTRL_EVENT_BIT_BOOT            0x00200000U  // First boot
#define BLEWIFI_CTRL_EVENT_BIT_WIFI_AUTOCONN   0x00400000U
#define BLEWIFI_CTRL_EVENT_BIT_OFFLINE         0x00800000U
#define BLEWIFI_CTRL_EVENT_BIT_NOT_CNT_SRV     0x00001000U
#define BLEWIFI_CTRL_EVENT_BIT_SHORT_PRESS     0x00002000U //Goter
#define BLEWIFI_CTRL_EVENT_BIT_CHANGE_HTTPURL  0x00004000U //Goter




#define POST_DATA_TIME              (3600000)  // 1 hour - smart sleep for one hour then post data
#define POST_DATA_TIME_RETRY        (20000)   // 20 sec - hourly post fail will per 20 sec so retry yes


//#define TEST_MODE_DEBUG_ENABLE


typedef void (*T_BleWifi_Ctrl_EvtHandler_Fp)(uint32_t evt_type, void *data, int len);
typedef struct
{
    uint32_t ulEventId;
    T_BleWifi_Ctrl_EvtHandler_Fp fpFunc;
} T_BleWifi_Ctrl_EvtHandlerTbl;

#define BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE   (g_tAppCtrlWifiConnectSettings.ubConnectRetry + 1)
#define BLEWIFI_CTRL_AUTO_CONN_STATE_SCAN   (BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE + 1)

extern T_MwFim_GP11_WifiConnectSettings g_tAppCtrlWifiConnectSettings;
extern T_MwFim_GP12_DCSlope g_tDCSlope;

void BleWifi_Ctrl_SysModeSet(uint8_t mode);
uint8_t BleWifi_Ctrl_SysModeGet(void);
void BleWifi_Ctrl_EventStatusSet(uint32_t dwEventBit, uint8_t status);
uint8_t BleWifi_Ctrl_EventStatusGet(uint32_t dwEventBit);
uint8_t BleWifi_Ctrl_EventStatusWait(uint32_t dwEventBit, uint32_t millisec);
void BleWifi_Ctrl_DtimTimeSet(uint32_t value);
uint32_t BleWifi_Ctrl_DtimTimeGet(void);
void BleWifi_Ctrl_LedStatusChange(void);
int BleWifi_Ctrl_MsgSend(int msg_type, uint8_t *data, int data_len);
void BleWifi_Ctrl_Init(void);

void BleWifi_Ctrl_NetworkingStart(void);
void BleWifi_Ctrl_NetworkingStop(void);
#if (BLEWIFI_CTRL_BUTTON_SENSOR_EN == 1)
void BleWifi_Ctrl_ButtonReleaseHandle(uint8_t u8ReleaseCount);
#endif

void BleWifi_Ctrl_WiFiConnect(void);//Goter

#endif /* __BLEWIFI_CTRL_H__ */

