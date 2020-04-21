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
 * @file blewifi_ctrl.c
 * @author Vincent Chen, Michael Liao
 * @date 20 Feb 2018
 * @brief File creates the blewifi ctrl task architecture.
 *
 */

#include <stdlib.h>
#include <string.h>
#include "sys_os_config.h"
#include "sys_os_config_patch.h"
#include "at_cmd_common.h"

#include "blewifi_common.h"
#include "blewifi_configuration.h"
#include "blewifi_ctrl.h"
#include "blewifi_wifi_api.h"
#include "blewifi_ble_api.h"
#include "blewifi_data.h"
#include "blewifi_app.h"
#include "mw_ota_def.h"
#include "mw_ota.h"
#include "hal_system.h"
#include "mw_fim_default_group03.h"
#include "mw_fim_default_group03_patch.h"
#include "mw_fim_default_group11_project.h"
#include "ps_public.h"
#include "mw_fim.h"
#include "controller_wifi_com.h"
#include "wifi_api.h"

#include "iot_data.h"
#include "sensor_button.h"
#include "sensor_door.h"
#include "sensor_https.h"
#include "sensor_data.h"
#include "sensor_common.h"
#include "sensor_battery.h"
#include "app_at_cmd.h"
#include "etharp.h"


#define BLEWIFI_CTRL_RESET_DELAY    (3000)  // ms


int8_t g_WifiRSSI;

osThreadId   g_tAppCtrlTaskId;
osMessageQId g_tAppCtrlQueueId;
osTimerId    g_tAppCtrlAutoConnectTriggerTimer;
osTimerId    g_tAppCtrlLedTimer;
osTimerId    g_tAppCtrlHttpPostTimer;
osTimerId    g_tAppCtrlSysTimer;
osTimerId    g_tAppCtrlTestModeId;
osTimerId    g_tAppCtrlHourlyHttpPostRetryTimer;
osTimerId    g_tAppCtrlType1_2_3_HttpPostRetryTimer;

// Check test mode
#define BLEWIFI_CTRL_IS_TEST_MODE_UNCHECK   (-1)    // Init, it can change to normal mode or test mode
#define BLEWIFI_CTRL_IS_TEST_MODE_NO        (0)     // Result is Normal mode, it can not changed mode in this mode
#define BLEWIFI_CTRL_IS_TEST_MODE_YES       (1)     // Result is Test mode, it can not changed mode in this mode
int8_t      g_s8IsTestMode = BLEWIFI_CTRL_IS_TEST_MODE_UNCHECK;    // 0 : No, 1 : Yes

EventGroupHandle_t g_tAppCtrlEventGroup;

extern T_HalAuxCalData g_tHalAux_CalData;
extern uint32_t g_ulHalAux_AverageCount;
extern int g_nDoType1_2_3_Retry_Flag;

uint8_t g_ulAppCtrlSysMode;
uint8_t g_ubAppCtrlSysStatus;
uint8_t g_ubAppCtrlLedStatus;
uint8_t g_u8ButtonProcessed;
uint8_t g_u8ShortPressButtonProcessed; //It means the time of button pressed is less than 5s 20191018EL

uint8_t g_ubAppCtrlRequestRetryTimes;
uint32_t g_ulAppCtrlAutoConnectInterval;
uint32_t g_ulAppCtrlDoAutoConnectCumulativeTime = 0;
uint32_t g_ulAppCtrlWifiDtimTime;

uint8_t g_nLastPostDatatType = TIMER_POST;


T_MwFim_GP11_WifiConnectSettings g_tAppCtrlWifiConnectSettings;

T_MwFim_GP12_HttpPostContent g_tHttpPostContent;
T_MwFim_GP12_HttpHostInfo g_tHostInfo;

T_MwFim_GP12_DCSlope g_tDCSlope;

uint32_t g_ulaAppCtrlLedInterval[BLEWIFI_CTRL_LED_NUM] =
{
    LED_TIME_BLE_ON_1,
    LED_TIME_BLE_OFF_1,
    
    LED_TIME_AUTOCONN_ON_1,
    LED_TIME_AUTOCONN_OFF_1,
    
    LED_TIME_OTA_ON,
    LED_TIME_OTA_OFF,

    LED_TIME_ALWAYS_OFF,

    LED_TIME_TEST_MODE_ON_1,
    LED_TIME_TEST_MODE_OFF_1,
    LED_TIME_TEST_MODE_ON_2,
    LED_TIME_TEST_MODE_OFF_2,
    LED_TIME_TEST_MODE_ON_3,
    LED_TIME_TEST_MODE_OFF_3,
    LED_TIME_TEST_MODE_ON_4,
    LED_TIME_TEST_MODE_OFF_4,

    LED_TIME_NOT_CNT_SRV_ON_1,
    LED_TIME_NOT_CNT_SRV_OFF_1,
    LED_TIME_NOT_CNT_SRV_ON_2,
    LED_TIME_NOT_CNT_SRV_OFF_2,

    LED_TIME_OFFLINE_ON,
    LED_TIME_OFFLINE_OFF,

    LED_TIME_SHORT_PRESS_ON,

    LED_TIME_BOOT_ON_1,
    LED_TIME_BOOT_OFF_1,
    LED_TIME_BOOT_ON_1
};

static void BleWifi_Ctrl_TaskEvtHandler_BleInitComplete(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_BleAdvertisingCfm(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_BleAdvertisingExitCfm(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_BleAdvertisingTimeChangeCfm(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_BleConnectionComplete(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_BleConnectionFail(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_BleDisconnect(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_BleDataInd(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_WifiInitComplete(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_WifiScanDoneInd(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_WifiConnectionInd(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_WifiDisconnectionInd(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_WifiGotIpInd(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_WifiAutoConnectInd(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_OtherOtaOn(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_OtherOtaOff(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_OtherOtaOffFail(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_ButtonStateChange(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_ButtonDebounceTimeOut(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_ButtonBleAdvTimeOut(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_ButtonLongPressTimeOut(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_DoorStateChange(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_DoorDebounceTimeOut(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_HttpPostDataInd(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_HttpPostDataTYPE1_2_3_Retry(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_OtherLedTimer(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_OtherSysTimer(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_NetworkingStart(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_NetworkingStop(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_IsTestModeAtFactory(uint32_t evt_type, void *data, int len);
static void BleWifi_Ctrl_TaskEvtHandler_IsTestModeTimeout(uint32_t evt_type, void *data, int len);
static T_BleWifi_Ctrl_EvtHandlerTbl g_tCtrlEvtHandlerTbl[] =
{
    {BLEWIFI_CTRL_MSG_BLE_INIT_COMPLETE,                BleWifi_Ctrl_TaskEvtHandler_BleInitComplete},
    {BLEWIFI_CTRL_MSG_BLE_ADVERTISING_CFM,              BleWifi_Ctrl_TaskEvtHandler_BleAdvertisingCfm},
    {BLEWIFI_CTRL_MSG_BLE_ADVERTISING_EXIT_CFM,         BleWifi_Ctrl_TaskEvtHandler_BleAdvertisingExitCfm},
    {BLEWIFI_CTRL_MSG_BLE_ADVERTISING_TIME_CHANGE_CFM,  BleWifi_Ctrl_TaskEvtHandler_BleAdvertisingTimeChangeCfm},
    {BLEWIFI_CTRL_MSG_BLE_CONNECTION_COMPLETE,          BleWifi_Ctrl_TaskEvtHandler_BleConnectionComplete},
    {BLEWIFI_CTRL_MSG_BLE_CONNECTION_FAIL,              BleWifi_Ctrl_TaskEvtHandler_BleConnectionFail},
    {BLEWIFI_CTRL_MSG_BLE_DISCONNECT,                   BleWifi_Ctrl_TaskEvtHandler_BleDisconnect},
    {BLEWIFI_CTRL_MSG_BLE_DATA_IND,                     BleWifi_Ctrl_TaskEvtHandler_BleDataInd},

    {BLEWIFI_CTRL_MSG_WIFI_INIT_COMPLETE,               BleWifi_Ctrl_TaskEvtHandler_WifiInitComplete},
    {BLEWIFI_CTRL_MSG_WIFI_SCAN_DONE_IND,               BleWifi_Ctrl_TaskEvtHandler_WifiScanDoneInd},
    {BLEWIFI_CTRL_MSG_WIFI_CONNECTION_IND,              BleWifi_Ctrl_TaskEvtHandler_WifiConnectionInd},
    {BLEWIFI_CTRL_MSG_WIFI_DISCONNECTION_IND,           BleWifi_Ctrl_TaskEvtHandler_WifiDisconnectionInd},
    {BLEWIFI_CTRL_MSG_WIFI_GOT_IP_IND,                  BleWifi_Ctrl_TaskEvtHandler_WifiGotIpInd},
    {BLEWIFI_CTRL_MSG_WIFI_AUTO_CONNECT_IND,            BleWifi_Ctrl_TaskEvtHandler_WifiAutoConnectInd},

    {BLEWIFI_CTRL_MSG_OTHER_OTA_ON,                     BleWifi_Ctrl_TaskEvtHandler_OtherOtaOn},
    {BLEWIFI_CTRL_MSG_OTHER_OTA_OFF,                    BleWifi_Ctrl_TaskEvtHandler_OtherOtaOff},
    {BLEWIFI_CTRL_MSG_OTHER_OTA_OFF_FAIL,               BleWifi_Ctrl_TaskEvtHandler_OtherOtaOffFail},

    {BLEWIFI_CTRL_MSG_BUTTON_STATECHANGE,               BleWifi_Ctrl_TaskEvtHandler_ButtonStateChange},
    {BLEWIFI_CTRL_MSG_BUTTON_DEBOUNCETIMEOUT,           BleWifi_Ctrl_TaskEvtHandler_ButtonDebounceTimeOut},
    {BLEWIFI_CTRL_MSG_BUTTON_BLEADVTIMEOUT,             BleWifi_Ctrl_TaskEvtHandler_ButtonBleAdvTimeOut},
    {BLEWIFI_CTRL_MSG_DOOR_STATECHANGE,                 BleWifi_Ctrl_TaskEvtHandler_DoorStateChange},
    {BLEWIFI_CTRL_MSG_DOOR_DEBOUNCETIMEOUT,             BleWifi_Ctrl_TaskEvtHandler_DoorDebounceTimeOut},
    {BLEWIFI_CTRL_MSG_BUTTON_LONG_PRESS_TIMEOUT,        BleWifi_Ctrl_TaskEvtHandler_ButtonLongPressTimeOut},

    {BLEWIFI_CTRL_MSG_HTTP_POST_DATA_IND,               BleWifi_Ctrl_TaskEvtHandler_HttpPostDataInd},
    {BLEWIFI_CTRL_MSG_HTTP_POST_DATA_TYPE1_2_3_RETRY,   BleWifi_Ctrl_TaskEvtHandler_HttpPostDataTYPE1_2_3_Retry},
    
    {BLEWIFI_CTRL_MSG_OTHER_LED_TIMER,                  BleWifi_Ctrl_TaskEvtHandler_OtherLedTimer},
    {BLEWIFI_CTRL_MSG_OTHER_SYS_TIMER,                  BleWifi_Ctrl_TaskEvtHandler_OtherSysTimer},
    {BLEWIFI_CTRL_MSG_NETWORKING_START,                 BleWifi_Ctrl_TaskEvtHandler_NetworkingStart},
    {BLEWIFI_CTRL_MSG_NETWORKING_STOP,                  BleWifi_Ctrl_TaskEvtHandler_NetworkingStop},

    {BLEWIFI_CTRL_MSG_IS_TEST_MODE_AT_FACTORY,          BleWifi_Ctrl_TaskEvtHandler_IsTestModeAtFactory},
    {BLEWIFI_CTRL_MSG_IS_TEST_MODE_TIMEOUT,             BleWifi_Ctrl_TaskEvtHandler_IsTestModeTimeout},

    {0xFFFFFFFF,                                        NULL}
};

void FinalizedATWIFIcmd()
{
    if ( true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE))
    {
         wifi_auto_connect_reset();
         wifi_connection_disconnect_ap();

         osTimerStop(g_tAppFactoryWifiConnTimerId);

         #ifdef TEST_MODE_DEBUG_ENABLE
         msg_print_uart1("cleanAutoConnectInfo\r\n");
         #endif
     }
}

void BleWifi_Ctrl_DoLedDisplay(void)
{
    if(!g_u8ShortPressButtonProcessed) //if short press button is processed, showing this light status is the 1st priority 20191018EL
    {
        switch (g_ubAppCtrlLedStatus)
        {
            case BLEWIFI_CTRL_LED_BLE_ON_1:          // pair #1
            case BLEWIFI_CTRL_LED_AUTOCONN_ON_1:     // pair #2
            case BLEWIFI_CTRL_LED_OTA_ON:            // pair #3
            case BLEWIFI_CTRL_LED_TEST_MODE_ON_1:      // pair #4
            case BLEWIFI_CTRL_LED_TEST_MODE_ON_2:      // pair #4
            case BLEWIFI_CTRL_LED_TEST_MODE_ON_3:      // pair #4
            case BLEWIFI_CTRL_LED_TEST_MODE_ON_4:      // pair #4
            case BLEWIFI_CTRL_LED_NOT_CNT_SRV_ON_1:  // pair #5
            case BLEWIFI_CTRL_LED_NOT_CNT_SRV_ON_2:  // pair #5
            case BLEWIFI_CTRL_LED_OFFLINE_ON_1:      // pair #6
            case BLEWIFI_CTRL_LED_BOOT_ON_1:         // pair #7//Goter
            case BLEWIFI_CTRL_LED_BOOT_ON_2:         // pair #7//Goter
            case BLEWIFI_CTRL_LED_SHORT_PRESS_ON:    // LEO one blink for short press//Goter
            Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_HIGH);
            break;

            case BLEWIFI_CTRL_LED_BLE_OFF_1:          // pair #1
            case BLEWIFI_CTRL_LED_AUTOCONN_OFF_1:     // pair #2
            case BLEWIFI_CTRL_LED_OTA_OFF:            // pair #3
            case BLEWIFI_CTRL_LED_TEST_MODE_OFF_1:      // pair #4
            case BLEWIFI_CTRL_LED_TEST_MODE_OFF_2:      // pair #4
            case BLEWIFI_CTRL_LED_TEST_MODE_OFF_3:      // pair #4
            case BLEWIFI_CTRL_LED_TEST_MODE_OFF_4:      // pair #4
            case BLEWIFI_CTRL_LED_NOT_CNT_SRV_OFF_1:  // pair #5
            case BLEWIFI_CTRL_LED_NOT_CNT_SRV_OFF_2:  // pair #5
            case BLEWIFI_CTRL_LED_OFFLINE_OFF_1:      // pair #6
            case BLEWIFI_CTRL_LED_BOOT_OFF_1:         // pair #7//Goter
            case BLEWIFI_CTRL_LED_ALWAYS_OFF:         // LED always off
            Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_LOW);
            break;

        // error handle
            default:
            Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_LOW);
            return;
        }
    }

    // start the led timer
    osTimerStop(g_tAppCtrlLedTimer);
    osTimerStart(g_tAppCtrlLedTimer, g_ulaAppCtrlLedInterval[g_ubAppCtrlLedStatus]);
}

void BleWifi_Ctrl_LedStatusChange(void)
{
    // LED status: Test Mode > OTA > BLE || BLE Adv > Wifi > WiFi Autoconnect > None

    // Test Mode
    if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE))
    {
        // status change
        g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_TEST_MODE_OFF_4;
        BleWifi_Ctrl_DoLedDisplay();
    }
    // OTA
    else if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_OTA))
    {
        // status change
        g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_OTA_OFF;
        BleWifi_Ctrl_DoLedDisplay();
    }
    // Network now
    else if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_NETWORK))
    {
        // status change
        g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_BLE_OFF_1;
        BleWifi_Ctrl_DoLedDisplay();
    }

    // Wifi Auto
    else if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_WIFI_AUTOCONN))
    {
        // status change
        g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_AUTOCONN_OFF_1;
        BleWifi_Ctrl_DoLedDisplay();
    }
    // Device offline
    else if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_OFFLINE))
    {
        g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_OFFLINE_OFF_1;
        BleWifi_Ctrl_DoLedDisplay();
    }
    // Got IP but cannot connect to server
    else if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_NOT_CNT_SRV))
    {
        // status change
        g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_NOT_CNT_SRV_OFF_1;
        BleWifi_Ctrl_DoLedDisplay();
    }
    #if 0  //Goter
    // Short Press
    else if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_SHORT_PRESS))
    {
        // status change
        g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_SHORT_PRESS_ON;
        BleWifi_Ctrl_DoLedDisplay();
    }
    #endif
    // Wi-Fi is connected
    else if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_GOT_IP))
    {
        // status change
        g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_ALWAYS_OFF;
        BleWifi_Ctrl_DoLedDisplay();
    }

    // None
    else
    {
        // status change
        g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_ALWAYS_OFF;
        BleWifi_Ctrl_DoLedDisplay();
    }
}

void BleWifi_Ctrl_LedStatusAutoUpdate(void)
{
    switch (g_ubAppCtrlLedStatus)
    {
        case BLEWIFI_CTRL_LED_BLE_ON_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_BLE_OFF_1;
            break;

        case BLEWIFI_CTRL_LED_BLE_OFF_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_BLE_ON_1;
            break;

        case BLEWIFI_CTRL_LED_AUTOCONN_ON_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_AUTOCONN_OFF_1;
            break;

        case BLEWIFI_CTRL_LED_AUTOCONN_OFF_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_AUTOCONN_ON_1;
            break;

        case BLEWIFI_CTRL_LED_OTA_ON:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_OTA_OFF;
            break;

        case BLEWIFI_CTRL_LED_OTA_OFF:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_OTA_ON;
            break;

        case BLEWIFI_CTRL_LED_ALWAYS_OFF:
            break;

        case BLEWIFI_CTRL_LED_TEST_MODE_ON_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_TEST_MODE_OFF_1;
            break;

        case BLEWIFI_CTRL_LED_TEST_MODE_OFF_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_TEST_MODE_ON_2;
            break;

        case BLEWIFI_CTRL_LED_TEST_MODE_ON_2:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_TEST_MODE_OFF_2;
            break;

        case BLEWIFI_CTRL_LED_TEST_MODE_OFF_2:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_TEST_MODE_ON_3;
            break;

        case BLEWIFI_CTRL_LED_TEST_MODE_ON_3:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_TEST_MODE_OFF_3;
            break;

        case BLEWIFI_CTRL_LED_TEST_MODE_OFF_3:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_TEST_MODE_ON_4;
            break;

        case BLEWIFI_CTRL_LED_TEST_MODE_ON_4:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_TEST_MODE_OFF_4;
            break;

        case BLEWIFI_CTRL_LED_TEST_MODE_OFF_4:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_TEST_MODE_ON_1;
            break;

        case BLEWIFI_CTRL_LED_NOT_CNT_SRV_ON_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_NOT_CNT_SRV_OFF_1;
            break;

        case BLEWIFI_CTRL_LED_NOT_CNT_SRV_OFF_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_NOT_CNT_SRV_ON_2;
            break;

        case BLEWIFI_CTRL_LED_NOT_CNT_SRV_ON_2:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_NOT_CNT_SRV_OFF_2;
            break;

        case BLEWIFI_CTRL_LED_NOT_CNT_SRV_OFF_2:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_NOT_CNT_SRV_ON_1;
            break;

        case BLEWIFI_CTRL_LED_OFFLINE_ON_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_OFFLINE_OFF_1;
            break;

        case BLEWIFI_CTRL_LED_OFFLINE_OFF_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_OFFLINE_ON_1;
            break;
#if 0  //Goter
        case BLEWIFI_CTRL_LED_SHORT_PRESS_ON:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_ALWAYS_OFF;
            break;

        case BLEWIFI_CTRL_LED_BOOT_ON_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_BOOT_OFF_1;
            break;

        case BLEWIFI_CTRL_LED_BOOT_OFF_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_BOOT_ON_2;
            break;

        case BLEWIFI_CTRL_LED_BOOT_ON_2:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_ALWAYS_OFF;
            break;

#endif

        default:
            break;
    }
}

void BleWifi_Ctrl_LedTimeout(void const *argu)
{
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_OTHER_LED_TIMER, NULL, 0);
}

void BleWifi_Ctrl_HttpPostData(void const *argu)
{
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_HTTP_POST_DATA_IND, NULL, 0);
}

void BleWifi_Ctrl_Type1_2_3_HttpPostData_Retry(void const *argu)
{
    // Trigger to re-send type 1,2,3 http data
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_HTTP_POST_DATA_TYPE1_2_3_RETRY, NULL, 0);
}

void BleWifi_Ctrl_SysModeSet(uint8_t mode)
{
    g_ulAppCtrlSysMode = mode;
}

uint8_t BleWifi_Ctrl_SysModeGet(void)
{
    return g_ulAppCtrlSysMode;
}

void BleWifi_Ctrl_EventStatusSet(uint32_t dwEventBit, uint8_t status)
{
// ISR mode is not supported.
#if 0
    BaseType_t xHigherPriorityTaskWoken, xResult;

    // check if it is ISR mode or not
    if (0 != __get_IPSR())
    {
        if (true == status)
        {
            // xHigherPriorityTaskWoken must be initialised to pdFALSE.
    		xHigherPriorityTaskWoken = pdFALSE;

            // Set bit in xEventGroup.
            xResult = xEventGroupSetBitsFromISR(g_tAppCtrlEventGroup, dwEventBit, &xHigherPriorityTaskWoken);
            if( xResult == pdPASS )
    		{
    			// If xHigherPriorityTaskWoken is now set to pdTRUE then a context
    			// switch should be requested.  The macro used is port specific and
    			// will be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() -
    			// refer to the documentation page for the port being used.
    			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    		}
        }
        else
            xEventGroupClearBitsFromISR(g_tAppCtrlEventGroup, dwEventBit);
    }
    // Taske mode
    else
#endif
    {
        if (true == status)
            xEventGroupSetBits(g_tAppCtrlEventGroup, dwEventBit);
        else
            xEventGroupClearBits(g_tAppCtrlEventGroup, dwEventBit);
    }
}

uint8_t BleWifi_Ctrl_EventStatusGet(uint32_t dwEventBit)
{
    EventBits_t tRetBit;

    tRetBit = xEventGroupGetBits(g_tAppCtrlEventGroup);
    if (dwEventBit == (dwEventBit & tRetBit))
        return true;

    return false;
}

uint8_t BleWifi_Ctrl_EventStatusWait(uint32_t dwEventBit, uint32_t millisec)
{
    EventBits_t tRetBit;

    tRetBit = xEventGroupWaitBits(g_tAppCtrlEventGroup,
                                  dwEventBit,
                                  pdFALSE,
                                  pdFALSE,
                                  millisec);
    if (dwEventBit == (dwEventBit & tRetBit))
        return true;

    return false;
}

void BleWifi_Ctrl_DtimTimeSet(uint32_t value)
{
    g_ulAppCtrlWifiDtimTime = value;
    BleWifi_Wifi_SetDTIM(g_ulAppCtrlWifiDtimTime);
}

uint32_t BleWifi_Ctrl_DtimTimeGet(void)
{
    return g_ulAppCtrlWifiDtimTime;
}

void BleWifi_Ctrl_DoAutoConnect(void)
{
    uint8_t data[2];

    // if the count of auto-connection list is empty, don't do the auto-connect
    if (0 == BleWifi_Wifi_AutoConnectListNum())
        return;

    // if request connect by Peer side, don't do the auto-connection
    if (g_ubAppCtrlRequestRetryTimes <= g_tAppCtrlWifiConnectSettings.ubConnectRetry)
        return;

    // BLE is disconnect and Wifi is disconnect, too.
    if ((false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_BLE)) && (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_WIFI)))
    {
        // start to scan
        // after scan, do the auto-connect
        if (g_ubAppCtrlRequestRetryTimes == BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE)
        {
            data[0] = 1;    // Enable to scan AP whose SSID is hidden
            data[1] = 2;    // mixed mode
            BleWifi_Wifi_DoScan(data, 2);

            g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_SCAN;
        }
    }
}

void BleWifi_Ctrl_AutoConnectTrigger(void const *argu)
{
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_WIFI_AUTO_CONNECT_IND, NULL, 0);
}

void BleWifi_Ctrl_SysStatusChange(void)
{
    T_MwFim_SysMode tSysMode;
    T_MwFim_GP11_PowerSaving tPowerSaving;

    // get the settings of system mode
    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP03_PATCH_SYS_MODE, 0, MW_FIM_SYS_MODE_SIZE, (uint8_t*)&tSysMode))
    {
        // if fail, get the default value
        memcpy(&tSysMode, &g_tMwFimDefaultSysMode, MW_FIM_SYS_MODE_SIZE);
    }

    // get the settings of power saving
    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP11_PROJECT_POWER_SAVING, 0, MW_FIM_GP11_POWER_SAVING_SIZE, (uint8_t*)&tPowerSaving))
    {
        // if fail, get the default value
        memcpy(&tPowerSaving, &g_tMwFimDefaultGp11PowerSaving, MW_FIM_GP11_POWER_SAVING_SIZE);
    }

    // change from init to normal
    if (g_ubAppCtrlSysStatus == BLEWIFI_CTRL_SYS_INIT)
    {
        g_ubAppCtrlSysStatus = BLEWIFI_CTRL_SYS_NORMAL;

        /* Power saving settings */
        if ((tSysMode.ubSysMode == MW_FIM_SYS_MODE_USER) && (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE)))
        { 
            ps_smart_sleep(tPowerSaving.ubPowerSaving);
        }

//        // start the sys timer
//        osTimerStop(g_tAppCtrlSysTimer);
//        osTimerStart(g_tAppCtrlSysTimer, BLEWIFI_COM_SYS_TIME_NORMAL);
    }
    // change from normal to ble off
    else if (g_ubAppCtrlSysStatus == BLEWIFI_CTRL_SYS_NORMAL)
    {
        g_ubAppCtrlSysStatus = BLEWIFI_CTRL_SYS_BLE_OFF;

//        // change the advertising time
//        BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MAX);
    }
}

uint8_t BleWifi_Ctrl_SysStatusGet(void)
{
    return g_ubAppCtrlSysStatus;
}

void BleWifi_Ctrl_SysTimeout(void const *argu)
{
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_OTHER_SYS_TIMER, NULL, 0);
}

static void BleWifi_Ctrl_TaskEvtHandler_BleInitComplete(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BLE_INIT_COMPLETE \r\n");

    /* BLE Init Step 2: BLE Advertising - The BLE time is 10s/once at the begining */
    BleWifi_Ble_StartAdvertising();
}

static void BleWifi_Ctrl_TaskEvtHandler_BleAdvertisingCfm(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BLE_ADVERTISING_CFM \r\n");

    /* BLE Init Step 3: BLE is ready for peer BLE device's connection trigger */
}

static void BleWifi_Ctrl_TaskEvtHandler_BleAdvertisingExitCfm(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BLE_ADVERTISING_EXIT_CFM \r\n");
}

static void BleWifi_Ctrl_TaskEvtHandler_BleAdvertisingTimeChangeCfm(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BLE_ADVERTISING_TIME_CHANGE_CFM \r\n");
    if (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_NETWORK))
    {
        BleWifi_Ble_StartAdvertising();
    }
}

static void BleWifi_Ctrl_TaskEvtHandler_BleConnectionComplete(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BLE_CONNECTION_COMPLETE \r\n");
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_BLE, true);

    /* BLE Init Step 4: BLE said it's connected with a peer BLE device */
}

static void BleWifi_Ctrl_TaskEvtHandler_BleConnectionFail(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BLE_CONNECTION_FAIL \r\n");
    BleWifi_Ble_StartAdvertising();
}

static void BleWifi_Ctrl_TaskEvtHandler_BleDisconnect(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BLE_DISCONNECT \r\n");
    if (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_NETWORK))
    {
        // When ble statu is not at networking, then change ble time
        /* When button press timer is finish, then change ble time from 0.5 second to 10 second */
        BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MAX);
    }
    else
    {
        // When ble statu is at networking, then ble disconnect. Ble adv again.
        BleWifi_Ble_StartAdvertising();
    }

    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_BLE, false);
    BleWifi_Ctrl_LedStatusChange();

    /* start to do auto-connection. */
    g_ulAppCtrlDoAutoConnectCumulativeTime = 0;
    g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalInit;
    BleWifi_Ctrl_DoAutoConnect();

    /* stop the OTA behavior */
    if (gTheOta)
    {
        MwOta_DataGiveUp();
        free(gTheOta);
        gTheOta = 0;

        BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_OTHER_OTA_OFF_FAIL, NULL, 0);
    }
}

static void BleWifi_Ctrl_TaskEvtHandler_BleDataInd(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BLE_DATA_IND \r\n");
    BleWifi_Ble_DataRecvHandler(data, len);
}

static void BleWifi_Ctrl_TaskEvtHandler_WifiInitComplete(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_INIT_COMPLETE \r\n");

    /* When device power on, start to do auto-connection. */
    g_ulAppCtrlDoAutoConnectCumulativeTime = 0;
    g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalInit;
    BleWifi_Ctrl_DoAutoConnect();

    /* Check wifi list
       If wifi list have record in flash of device then set WIFI_AUTOCONN as true.
    */
    if (0 == BleWifi_Wifi_AutoConnectListNum())
    {
        /* When do wifi scan, set wifi auto connect is false */
        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI_AUTOCONN, false);
    }
    else
    {
        /* When do wifi scan, set wifi auto connect is true */
        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI_AUTOCONN, true);
    }
    BleWifi_Ctrl_LedStatusChange();
}

void BleWifi_Ctrl_WiFiConnect(void)
{
    uint8_t i;
    uint8_t WifiConnectDataLen;
    uint8_t WifiConnectData[128] = {0};
    int8_t ResultIndex = -1;

    // Wifi scan result
    scan_report_t *result = NULL;

    result = wifi_get_scan_result();

    if (result == NULL) {
        msg_print_uart1("AT+SIGNAL=0\r\n");
        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE, false);
        #ifdef TEST_MODE_DEBUG_ENABLE
        msg_print_uart1("Fail to scan the AP for AT_WIFI\r\n");
        #endif
        FinalizedATWIFIcmd();
        return;
    }

    for (i = 0; i < result->uScanApNum; i++)
    {
        if ((strcmp ((char *)g_WifiSSID, result->pScanInfo[i].ssid)) == 0)
        {
            ResultIndex = i;
            break;
        }

       // msg_print_uart1("%s \n", result->pScanInfo[i].ssid);
    }

    if( ResultIndex < 0)
    {
        msg_print_uart1("AT+SIGNAL=0\r\n");
        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE, false);
        #ifdef TEST_MODE_DEBUG_ENABLE
        msg_print_uart1("Fail to find the AP for AT_WIFI\r\n");
        #endif
        FinalizedATWIFIcmd();
        return;
    }

    g_WifiRSSI = result->pScanInfo[ResultIndex].rssi;
#ifdef TEST_MODE_DEBUG_ENABLE
    msg_print_uart1("RSSI: %d\n",g_WifiRSSI);

    msg_print_uart1("BSSID: %02X %02X %02X %02X %02X %02X\n",
    result->pScanInfo[ResultIndex].bssid[0],
    result->pScanInfo[ResultIndex].bssid[1],
    result->pScanInfo[ResultIndex].bssid[2],
    result->pScanInfo[ResultIndex].bssid[3],
    result->pScanInfo[ResultIndex].bssid[4],
    result->pScanInfo[ResultIndex].bssid[5]
    );
#endif

    WifiConnectDataLen = WIFI_MAC_ADDRESS_LENGTH + 1 + 1 + g_WifiPasswordLen;

    for (i = 0; i < WifiConnectDataLen; i++)
    {
        if (i < 6)
            WifiConnectData[i] = result->pScanInfo[ResultIndex].bssid[i];
        else if (i == 6)
            WifiConnectData[i] = 0;
        else if (i == 7)
            WifiConnectData[i] = g_WifiPasswordLen;
        else
            WifiConnectData[i] = g_WifiPassword[i - (WIFI_MAC_ADDRESS_LENGTH + 1 + 1)];
    }

#ifdef TEST_MODE_DEBUG_ENABLE
    msg_print_uart1("WIFI Connect Data: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n"
        , WifiConnectData[0]
        , WifiConnectData[1]
        , WifiConnectData[2]
        , WifiConnectData[3]
        , WifiConnectData[4]
        , WifiConnectData[5]
        , WifiConnectData[6]
        , WifiConnectData[7]
        , WifiConnectData[8]
        , WifiConnectData[9]
        , WifiConnectData[10]
        , WifiConnectData[11]
        , WifiConnectData[12]
        , WifiConnectData[13]
        , WifiConnectData[14]
        , WifiConnectData[15]
    );
#endif

    BleWifi_Wifi_DoConnect(WifiConnectData, WifiConnectDataLen);
}

static void BleWifi_Ctrl_TaskEvtHandler_WifiScanDoneInd(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_SCAN_DONE_IND \r\n");
    if (( true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE))
        && (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE)))
    {
            printf("BleWifi_Ctrl_TaskEvtHandler_WifiScanDoneInd\r\n");
            BleWifi_Ctrl_WiFiConnect();
            return;
    }

    // scan by auto-connect
    if (g_ubAppCtrlRequestRetryTimes == BLEWIFI_CTRL_AUTO_CONN_STATE_SCAN)
    {
        BleWifi_Wifi_UpdateScanInfoToAutoConnList();
        BleWifi_Wifi_DoAutoConnect();
        g_ulAppCtrlAutoConnectInterval = g_ulAppCtrlAutoConnectInterval + g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalDiff;
        if (g_ulAppCtrlAutoConnectInterval > g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalMax)
            g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalMax;

        g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
    }
    // scan by user
    else
    {
        BleWifi_Wifi_SendScanReport();
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_SCAN_END, 0);
    }
}

static void BleWifi_Ctrl_TaskEvtHandler_WifiConnectionInd(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_CONNECTION_IND \r\n");
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI, true);

    // return to the idle of the connection retry
    g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
    g_ulAppCtrlDoAutoConnectCumulativeTime = 0;
    g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalInit;
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_CONNECTED_DONE);
}

static void BleWifi_Ctrl_TaskEvtHandler_WifiDisconnectionInd(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_DISCONNECTION_IND \r\n");
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI, false);
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_GOT_IP, false);
    BleWifi_Wifi_SetDTIM(0);

    lwip_one_shot_arp_enable();//Goter

    if (( true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE))
        && (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE)))
    {
            msg_print_uart1("AT+SIGNAL=0\r\n");
            BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE, false);
            #ifdef TEST_MODE_DEBUG_ENABLE
            msg_print_uart1("WifiDisconnection\r\n");
            #endif
            FinalizedATWIFIcmd();   // Terence
            return ;
    }

    // Stop Http Post Timer
    // osTimerStop(g_tAppCtrlHttpPostTimer);

    if (0 == BleWifi_Wifi_AutoConnectListNum())
    {
        /* When do wifi scan, set wifi auto connect is false */
        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI_AUTOCONN, false);
        //BleWifi_Ctrl_LedStatusChange();
    }

    // continue the connection retry
    if (g_ubAppCtrlRequestRetryTimes < g_tAppCtrlWifiConnectSettings.ubConnectRetry)
    {
        BleWifi_Wifi_ReqConnectRetry();
        g_ubAppCtrlRequestRetryTimes++;
    }
    // stop the connection retry
    else if (g_ubAppCtrlRequestRetryTimes == g_tAppCtrlWifiConnectSettings.ubConnectRetry)
    {
        // return to the idle of the connection retry
        g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
        g_ulAppCtrlDoAutoConnectCumulativeTime = 0;
        g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalInit;
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_CONNECTED_FAIL);

        /* do auto-connection. */
        if (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_BLE))
        {
            if (0 != BleWifi_Wifi_AutoConnectListNum())
            {
                osTimerStop(g_tAppCtrlAutoConnectTriggerTimer);
                osTimerStart(g_tAppCtrlAutoConnectTriggerTimer, g_ulAppCtrlAutoConnectInterval);

                /* When do wifi scan, set wifi auto connect is true */
                BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI_AUTOCONN, true);
                BleWifi_Ctrl_LedStatusChange();
            }
        }
    }
    else
    {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_DISCONNECT, BLEWIFI_WIFI_DISCONNECTED_DONE);

        if(g_ulAppCtrlDoAutoConnectCumulativeTime >= BLEWIFI_WIFI_AUTO_CONNECT_LIFETIME_MAX)
        {
            BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI_AUTOCONN, false);
            BleWifi_Ctrl_LedStatusChange();
            g_ulAppCtrlDoAutoConnectCumulativeTime = 0;
            return;
        }


        /* do auto-connection. */
        if (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_BLE))
        {
            if (0 != BleWifi_Wifi_AutoConnectListNum())
            {
                osTimerStop(g_tAppCtrlAutoConnectTriggerTimer);
                osTimerStart(g_tAppCtrlAutoConnectTriggerTimer, g_ulAppCtrlAutoConnectInterval);

                g_ulAppCtrlDoAutoConnectCumulativeTime = g_ulAppCtrlDoAutoConnectCumulativeTime + g_ulAppCtrlAutoConnectInterval;

                /* When do wifi scan, set wifi auto connect is true */
                BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI_AUTOCONN, true);
                BleWifi_Ctrl_LedStatusChange();
            }
        }
    }
}

static void BleWifi_Ctrl_TaskEvtHandler_WifiGotIpInd(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_GOT_IP_IND \r\n");
    if (( true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE))
        && (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE)))
    {
            msg_print_uart1("AT+SIGNAL=%d\r\n", g_WifiRSSI);
            BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE, false);
            FinalizedATWIFIcmd();
            return ;
    }
    BLEWIFI_INFO("SendStatusInfo BLEWIFI_IND_IP_STATUS_NOTIFY \r\n");
    BleWifi_Wifi_SendStatusInfo(BLEWIFI_IND_IP_STATUS_NOTIFY);  //Goter

#if (SNTP_FUNCTION_EN == 1)
    BleWifi_SntpInit();
#endif
    // Update Battery voltage for post data
    UpdateBatteryContent();
    BleWifi_Wifi_UpdateBeaconInfo();
    BleWifi_Wifi_SetDTIM(BleWifi_Ctrl_DtimTimeGet());

    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_GOT_IP, true);

    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI_AUTOCONN, false);
    BleWifi_Ctrl_LedStatusChange();

    g_ulAppCtrlDoAutoConnectCumulativeTime = 0;

    Sensor_Data_ResetBuffer();

    // Trigger to send http data
    Sensor_Data_Push(BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR), TIMER_POST,  BleWifi_SntpGetRawData());
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_DATA_POST, NULL, 0);

    // When got ip then start timer to post data
    //osTimerStop(g_tAppCtrlHttpPostTimer);
    //osTimerStart(g_tAppCtrlHttpPostTimer, POST_DATA_TIME);
}

static void BleWifi_Ctrl_TaskEvtHandler_WifiAutoConnectInd(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_AUTO_CONNECT_IND \r\n");
    printf("BLEWIFI_CTRL_MSG_WIFI_AUTO_CONNECT_IND Total %d  \r\n",g_ulAppCtrlDoAutoConnectCumulativeTime);
    BleWifi_Ctrl_DoAutoConnect();
}

static void BleWifi_Ctrl_TaskEvtHandler_OtherOtaOn(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_OTHER_OTA_ON \r\n");
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_OTA, true);
}

static void BleWifi_Ctrl_TaskEvtHandler_OtherOtaOff(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_OTHER_OTA_OFF \r\n");
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_OTA, false);
    msg_print_uart1("OK\r\n");

    // restart the system
    osDelay(BLEWIFI_CTRL_RESET_DELAY);
    Hal_Sys_SwResetAll();
}

static void BleWifi_Ctrl_TaskEvtHandler_OtherOtaOffFail(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_OTHER_OTA_OFF_FAIL \r\n");
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_OTA, false);
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_OTA_MODE, false);
}

static void BleWifi_Ctrl_TaskEvtHandler_ButtonStateChange(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BUTTON_STATECHANGE \r\n");

    /* Start timer to debounce */
    osTimerStop(g_tAppButtonTimerId);
    osTimerStart(g_tAppButtonTimerId, TIMEOUT_DEBOUNCE_TIME);
}

static void BleWifi_Ctrl_TaskEvtHandler_ButtonDebounceTimeOut(uint32_t evt_type, void *data, int len)
{
    uint32_t u32sec = 0;
    unsigned int u32PinLevel = 0;

    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BUTTON_DEBOUNCETIMEOUT \r\n");

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(BUTTON_IO_PORT);
    printf("BUTTON_IO_PORT pin level = %s\r\n", u32PinLevel ? "GPIO_LEVEL_LOW" : "GPIO_LEVEL_HIGH");

    // User Press button
    // When detect falling edge, then modify to raising edge.
    if(GPIO_LEVEL_LOW == u32PinLevel)
    {
        Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_HIGH);//Goter
        g_u8ShortPressButtonProcessed = 1;  //20191018EL
    
        /* Enable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(BUTTON_IO_PORT, 0);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(BUTTON_IO_PORT, 1);

        if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE))
        {
            // stop the led timer
            osTimerStop(g_tAppCtrlLedTimer);
            Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_HIGH);
        }
        else
        {
#ifdef SLEEP_TIMER_ISSUE
            BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_BUTTON_PRESS, true);
#endif
            g_ulSoftTimerTickPre = osKernelSysTick();

            // When user press button last for 5 second, then ble adv start.
            g_u8ButtonProcessed = 0;
            osTimerStop(g_tAppButtonLongPressTimerId);
            osTimerStart(g_tAppButtonLongPressTimerId, BLEWIFI_CTRL_KEY_PRESS_TIME);
        }
    }
    // User release button
    else
    {
        Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_LOW);//Goter
        g_u8ShortPressButtonProcessed =0;  //20191018EL

		
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(BUTTON_IO_PORT, 1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(BUTTON_IO_PORT, 1);

        if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE))
        {
            // start the led timer
            Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_LOW);
            osTimerStop(g_tAppCtrlLedTimer);
            osTimerStart(g_tAppCtrlLedTimer, g_ulaAppCtrlLedInterval[BLEWIFI_CTRL_LED_TEST_MODE_OFF_4]);
        }
        else
        {
            if(!g_u8ButtonProcessed)
            {
                // Stop g_tAppButtonLongPressTimerId
                osTimerStop(g_tAppButtonLongPressTimerId);
            }

            g_ulSoftTimerTickNow = osKernelSysTick();

#ifdef SLEEP_TIMER_ISSUE
            if (true != BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_BUTTON_PRESS))
                return;

            BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_BUTTON_PRESS, false);
#endif
            if(g_ulSoftTimerTickNow >= g_ulSoftTimerTickPre)
                u32sec = (g_ulSoftTimerTickNow - g_ulSoftTimerTickPre);
            else
            {
                // When g_ulSoftTimerTickNow is overflow then execute this
                u32sec = (0XFFFFFFFF - g_ulSoftTimerTickPre + g_ulSoftTimerTickNow + 1);
            }

            // If press button time long than 5 second.
            if (u32sec >= BLEWIFI_CTRL_KEY_PRESS_TIME)
            {
            }
            // If press button time less than 5 second.
            else
            {
                // if the state is not at ble connection and network (ble adv), then do post data
                if(false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_BLE) && false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_NETWORK))
                {
                    Sensor_Data_Push(BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR), SHORT_TRIG,  BleWifi_SntpGetRawData());
                    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_DATA_POST, NULL, 0);

                    if((g_ulAppCtrlDoAutoConnectCumulativeTime == 0) && (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_GOT_IP)))
                    {
                        printf("---->AutoConn...Bottom\n");
                        // the idle of the connection retry
                        g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
                        g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalInit;

                        osTimerStop(g_tAppCtrlAutoConnectTriggerTimer);
                        osTimerStart(g_tAppCtrlAutoConnectTriggerTimer, g_ulAppCtrlAutoConnectInterval);
                    }
                    
                    #if 0//Goter
                    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_SHORT_PRESS, true);
                    BleWifi_Ctrl_LedStatusChange();
                    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_SHORT_PRESS, false);
                    #endif
                }
                else
                {
                    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_NETWORK, false);
                    if(false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_BLE))
                    {
                        /* When button press timer is finish, then change ble time from 0.5 second to 10 second */
                        BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MAX);
                    }
                    else
                    {
                        BleWifi_Ble_Disconnect();
                    }
                    BleWifi_Ctrl_LedStatusChange();
                }
            }

            printf("keypress times is = %d miliseconds \r\n", u32sec);
        }
    }
}

static void BleWifi_Ctrl_TaskEvtHandler_ButtonLongPressTimeOut(uint32_t evt_type, void *data, int len)
{
    //APP_DBG("[%s %d] long press timeout\n", __func__, __LINE__);

    // Long Press button process now
    g_u8ButtonProcessed = 1;
    g_u8ShortPressButtonProcessed = 0; //20191018EL

    if (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_NETWORK))
    {
        /* Start timer to ble adv stop */
        osTimerStop(g_tAppButtonBleAdvTimerId);
        osTimerStart(g_tAppButtonBleAdvTimerId, BLEWIFI_CTRL_BLEADVSTOP_TIME);

        /* BLE Adv */
        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_NETWORK, true);
        /* When button press long than 5 second, then change ble time from 10 second to 0.5 second */
        BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MAX);
        BleWifi_Ctrl_LedStatusChange();
    }
}

static void BleWifi_Ctrl_TaskEvtHandler_ButtonBleAdvTimeOut(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BUTTON_BLEADVTIMEOUT \r\n");
    /* BLE Adv short change to long, LED turn off */
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_NETWORK, false);
    if(false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_BLE))
    {
        /* When button press timer is finish, then change ble time from 0.5 second to 10 second */
        BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MAX);
    }
    else
    {
        BleWifi_Ble_Disconnect();
    }
    BleWifi_Ctrl_LedStatusChange();
}

static void BleWifi_Ctrl_TaskEvtHandler_DoorStateChange(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_DOOR_STATECHANGE \r\n");
    /* Start timer to debounce */
    osTimerStop(g_tAppDoorTimerId);
    osTimerStart(g_tAppDoorTimerId, DOOR_DEBOUNCE_TIMEOUT);
}

static void BleWifi_Ctrl_TaskEvtHandler_DoorDebounceTimeOut(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_DOOR_DEBOUNCETIMEOUT \r\n");
    unsigned int u32PinLevel = 0;

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(MAGNETIC_IO_PORT);
    printf("MAG_IO_PORT pin level = %s\r\n", u32PinLevel ? "GPIO_LEVEL_HIGH:OPEN" : "GPIO_LEVEL_LOW:CLOSE");

    // When detect falling edge, then modify to raising edge.

    // Voltage Low   / DoorStatusSet - True  / Door Status - CLose - switch on  - type = 2
    // Voltage Hight / DoorStatusSet - False / Door Status - Open  - switch off - type = 3
    if(GPIO_LEVEL_LOW == u32PinLevel)
    {
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(MAGNETIC_IO_PORT, 0);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(MAGNETIC_IO_PORT, 1);

        if (BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR) == false)
        {
            /* Send to IOT task to post data */
            BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_DOOR, true); // true is Door Close
            Sensor_Data_Push(BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR), BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR) ? DOOR_OPEN:DOOR_CLOSE,  BleWifi_SntpGetRawData());
            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_DATA_POST, NULL, 0);

            if((g_ulAppCtrlDoAutoConnectCumulativeTime == 0) && (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_GOT_IP)))
            {
                printf("---->AutoConn...DoorClose\n");
                // the idle of the connection retry
                g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
                g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalInit;

                osTimerStop(g_tAppCtrlAutoConnectTriggerTimer);
                osTimerStart(g_tAppCtrlAutoConnectTriggerTimer, g_ulAppCtrlAutoConnectInterval);
            }
        }
    }
    else
    {
        /* Enable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(MAGNETIC_IO_PORT, 1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(MAGNETIC_IO_PORT, 1);

        if (BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR) == true)
        {
            /* Send to IOT task to post data */
            BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_DOOR, false); // false is Door Open
            Sensor_Data_Push(BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR), BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR) ? DOOR_OPEN:DOOR_CLOSE,  BleWifi_SntpGetRawData());
            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_DATA_POST, NULL, 0);

            
            if((g_ulAppCtrlDoAutoConnectCumulativeTime == 0) && (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_GOT_IP)))
            {
                printf("---->AutoConn...DoorOpen\n");
                // the idle of the connection retry
                g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
                g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalInit;

                osTimerStop(g_tAppCtrlAutoConnectTriggerTimer);
                osTimerStart(g_tAppCtrlAutoConnectTriggerTimer, g_ulAppCtrlAutoConnectInterval);
            }
        }
    }
}

static void BleWifi_Ctrl_TaskEvtHandler_HttpPostDataInd(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_HTTP_POST_DATA_IND \r\n");

    // Trigger to send http data
    Sensor_Data_Push(BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR), TIMER_POST,  BleWifi_SntpGetRawData());
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_DATA_POST, NULL, 0);
    if((g_ulAppCtrlDoAutoConnectCumulativeTime == 0) && (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_GOT_IP)))
    {
        printf("---->AutoConn...HourlyPost\n");
        // the idle of the connection retry
        g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
        g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalInit;

        osTimerStop(g_tAppCtrlAutoConnectTriggerTimer);
        osTimerStart(g_tAppCtrlAutoConnectTriggerTimer, g_ulAppCtrlAutoConnectInterval);
    }
}

static void BleWifi_Ctrl_TaskEvtHandler_HttpPostDataTYPE1_2_3_Retry(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_HTTP_POST_DATA_TYPE1_2_3_RETRY \r\n");

    // Trigger to re-send http data type 1,2,3
    g_nDoType1_2_3_Retry_Flag = 1;
    Sensor_Data_Push(BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR), g_nLastPostDatatType,  BleWifi_SntpGetRawData());
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_DATA_POST, NULL, 0);

}

static void BleWifi_Ctrl_TaskEvtHandler_OtherLedTimer(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_OTHER_LED_TIMER \r\n");
    BleWifi_Ctrl_LedStatusAutoUpdate();
    BleWifi_Ctrl_DoLedDisplay();
}

static void BleWifi_Ctrl_TaskEvtHandler_OtherSysTimer(uint32_t evt_type, void *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_OTHER_SYS_TIMER \r\n");
    BleWifi_Ctrl_SysStatusChange();
}

void BleWifi_Ctrl_NetworkingStart(void)
{
    if (false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_NETWORK))
    {
        BLEWIFI_INFO("[%s %d] start\n", __func__, __LINE__);

        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_NETWORK, true);
    }
    else
    {
        BLEWIFI_WARN("[%s %d] BLEWIFI_CTRL_EVENT_BIT_NETWORK already true\n", __func__, __LINE__);
    }
}

void BleWifi_Ctrl_NetworkingStop(void)
{
    if (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_NETWORK))
    {
        BLEWIFI_INFO("[%s %d] start\n", __func__, __LINE__);

        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_NETWORK, false);
        BleWifi_Ctrl_LedStatusChange();
    }
    else
    {
        BLEWIFI_WARN("[%s %d] BLEWIFI_CTRL_EVENT_BIT_NETWORK already false\n", __func__, __LINE__);
    }
}

static void BleWifi_Ctrl_TaskEvtHandler_NetworkingStart(uint32_t evt_type, void *data, int len)
{
    BleWifi_Ctrl_NetworkingStart();
}

static void BleWifi_Ctrl_TaskEvtHandler_NetworkingStop(uint32_t evt_type, void *data, int len)
{
    BleWifi_Ctrl_NetworkingStop();
}

static void BleWifi_Ctrl_TaskEvtHandler_IsTestModeAtFactory(uint32_t evt_type, void *data, int len)
{
    if ( g_s8IsTestMode != BLEWIFI_CTRL_IS_TEST_MODE_UNCHECK)
    {   // Never write data or timeout
        msg_print_uart1("\r\n>");
        return;
    }
    g_s8IsTestMode = BLEWIFI_CTRL_IS_TEST_MODE_YES;
    
    // stop the sys timer
    osTimerStop(g_tAppCtrlSysTimer);
    // stop the test mode timer
    osTimerStop(g_tAppCtrlTestModeId);
    
    // start test mode of LED blink
    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE, true);
    BleWifi_Ctrl_LedStatusChange();
    
    msg_print_uart1("factory mode\r\n");
}

static void BleWifi_Ctrl_TaskEvtHandler_IsTestModeTimeout(uint32_t evt_type, void *data, int len)
{
    if ( g_s8IsTestMode != BLEWIFI_CTRL_IS_TEST_MODE_UNCHECK)
    { // at+factory
        msg_print_uart1("\r\n>");
        return;
    }
    g_s8IsTestMode = BLEWIFI_CTRL_IS_TEST_MODE_NO;

    msg_print_uart1("normal mode\r\n");
}

void BleWifi_Ctrl_TaskEvtHandler(uint32_t evt_type, void *data, int len)
{
    uint32_t i = 0;

    while (g_tCtrlEvtHandlerTbl[i].ulEventId != 0xFFFFFFFF)
    {
        // match
        if (g_tCtrlEvtHandlerTbl[i].ulEventId == evt_type)
        {
            g_tCtrlEvtHandlerTbl[i].fpFunc(evt_type, data, len);
            break;
        }

        i++;
    }

    // not match
    if (g_tCtrlEvtHandlerTbl[i].ulEventId == 0xFFFFFFFF)
    {
    }
}

static void BleWifi_Ctrl_TestMode_Timeout(void const *argu)
{
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_IS_TEST_MODE_TIMEOUT, NULL, 0);
}

void BleWifi_Ctrl_Task(void *args)
{
    uint8_t i;
    osEvent rxEvent;
    xBleWifiCtrlMessage_t *rxMsg;
    osTimerDef_t tTimerTestModeDef;

    // create the timer
    tTimerTestModeDef.ptimer = BleWifi_Ctrl_TestMode_Timeout;
    g_tAppCtrlTestModeId = osTimerCreate(&tTimerTestModeDef, osTimerOnce, NULL);
    if (g_tAppCtrlTestModeId == NULL)
    {
        printf("To create the timer for AppTimer is fail.\n");
    }

    /* print "factory?\r\n" */
    for (i=0; i<3; i++)
    {
        msg_print_uart1("factory?\r\n");
        osDelay(20);
    }

    if ( strcmp(g_tHttpPostContent.ubaDeviceId, DEVICE_ID) == 0 || strcmp(g_tHttpPostContent.ubaChipId, CHIP_ID) == 0 )    
    {// if no write data, enter to factory mode.
        g_s8IsTestMode = BLEWIFI_CTRL_IS_TEST_MODE_YES;

        // stop the sys timer
        osTimerStop(g_tAppCtrlSysTimer);

        // start test mode of LED blink
        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE, true);
        BleWifi_Ctrl_LedStatusChange();

        msg_print_uart1("factory mode\r\n");
    }
    else
    {
        // start the test mode timer
        osTimerStop(g_tAppCtrlTestModeId);
        osTimerStart(g_tAppCtrlTestModeId, TEST_MODE_TIMER_DEF);
    }

    for(;;)
    {
        /* Wait event */
        rxEvent = osMessageGet(g_tAppCtrlQueueId, osWaitForever);
        if(rxEvent.status != osEventMessage)
            continue;

        rxMsg = (xBleWifiCtrlMessage_t *)rxEvent.value.p;
        BleWifi_Ctrl_TaskEvtHandler(rxMsg->event, rxMsg->ucaMessage, rxMsg->length);

        /* Release buffer */
        if (rxMsg != NULL)
            free(rxMsg);
    }
}

int BleWifi_Ctrl_MsgSend(int msg_type, uint8_t *data, int data_len)
{
    xBleWifiCtrlMessage_t *pMsg = 0;

	if (NULL == g_tAppCtrlQueueId)
	{
        BLEWIFI_ERROR("BLEWIFI: No queue \r\n");
        return -1;
	}

    /* Mem allocate */
    pMsg = malloc(sizeof(xBleWifiCtrlMessage_t) + data_len);
    if (pMsg == NULL)
	{
        BLEWIFI_ERROR("BLEWIFI: ctrl task pmsg allocate fail \r\n");
	    goto error;
    }

    pMsg->event = msg_type;
    pMsg->length = data_len;
    if (data_len > 0)
    {
        memcpy(pMsg->ucaMessage, data, data_len);
    }

    if (osMessagePut(g_tAppCtrlQueueId, (uint32_t)pMsg, osWaitForever) != osOK)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task message send fail \r\n");
        goto error;
    }

    return 0;

error:
	if (pMsg != NULL)
	{
	    free(pMsg);
    }

	return -1;
}

void BleWifi_Ctrl_Init(void)
{
    osThreadDef_t task_def;
    osMessageQDef_t blewifi_queue_def;
    osTimerDef_t timer_auto_connect_def;
    osTimerDef_t timer_led_def;
    osTimerDef_t timer_http_post_def;
    osTimerDef_t timer_hrly_http_post_retry_def;
    osTimerDef_t timer_type1_2_3_http_post_retry_def;
    osTimerDef_t timer_sys_def;

    //T_MwFim_GP12_HttpPostContent HttpPostContent;

     memset(&g_tHostInfo ,0, sizeof(T_MwFim_GP12_HttpHostInfo));
     memset(&g_tHttpPostContent ,0, sizeof(T_MwFim_GP12_HttpPostContent));

     // Goter, got MW_FIM_IDX_GP12_PROJECT_DEVICE_AUTH_CONTENT / MW_FIM_IDX_GP12_PROJECT_HOST_INFO
     if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_DEVICE_AUTH_CONTENT, 0, MW_FIM_GP12_HTTP_POST_CONTENT_SIZE, (uint8_t *)&g_tHttpPostContent)) 
     {
         memcpy(&g_tHttpPostContent, &g_tMwFimDefaultGp12HttpPostContent, MW_FIM_GP12_HTTP_POST_CONTENT_SIZE);
     }
    
     if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_HOST_INFO, 0, MW_FIM_GP12_HTTP_HOST_INFO_SIZE, (uint8_t *)&g_tHostInfo) ) {
         memcpy(&g_tHostInfo, &g_tMwFimDefaultGp12HttpHostInfo, MW_FIM_GP12_HTTP_HOST_INFO_SIZE);
     }

    /* Create ble-wifi task */
    task_def.name = "blewifi ctrl";
    task_def.stacksize = OS_TASK_STACK_SIZE_BLEWIFI_CTRL_APP;
    task_def.tpriority = OS_TASK_PRIORITY_APP;
    task_def.pthread = BleWifi_Ctrl_Task;
    g_tAppCtrlTaskId = osThreadCreate(&task_def, (void*)NULL);
    if(g_tAppCtrlTaskId == NULL)
    {
        BLEWIFI_INFO("BLEWIFI: ctrl task create fail \r\n");
    }
    else
    {
        BLEWIFI_INFO("BLEWIFI: ctrl task create successful \r\n");
    }

    /* Create message queue*/
    blewifi_queue_def.item_sz = sizeof(xBleWifiCtrlMessage_t);
    blewifi_queue_def.queue_sz = BLEWIFI_CTRL_QUEUE_SIZE;
    g_tAppCtrlQueueId = osMessageCreate(&blewifi_queue_def, NULL);
    if(g_tAppCtrlQueueId == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create queue fail \r\n");
    }

    /* create timer to trig auto connect */
    timer_auto_connect_def.ptimer = BleWifi_Ctrl_AutoConnectTrigger;
    g_tAppCtrlAutoConnectTriggerTimer = osTimerCreate(&timer_auto_connect_def, osTimerOnce, NULL);
    if(g_tAppCtrlAutoConnectTriggerTimer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create auto-connection timer fail \r\n");
    }

    /* create timer to trig the sys state */
    timer_sys_def.ptimer = BleWifi_Ctrl_SysTimeout;
    g_tAppCtrlSysTimer = osTimerCreate(&timer_sys_def, osTimerOnce, NULL);
    if(g_tAppCtrlSysTimer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create SYS timer fail \r\n");
    }

    /* Create the event group */
    g_tAppCtrlEventGroup = xEventGroupCreate();
    if(g_tAppCtrlEventGroup == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create event group fail \r\n");
    }

    /* the init state of system mode is init */
    g_ulAppCtrlSysMode = MW_FIM_SYS_MODE_INIT;

    // get the settings of Wifi connect settings
	if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP11_PROJECT_WIFI_CONNECT_SETTINGS, 0, MW_FIM_GP11_WIFI_CONNECT_SETTINGS_SIZE, (uint8_t*)&g_tAppCtrlWifiConnectSettings))
    {
        // if fail, get the default value
        memcpy(&g_tAppCtrlWifiConnectSettings, &g_tMwFimDefaultGp11WifiConnectSettings, MW_FIM_GP11_WIFI_CONNECT_SETTINGS_SIZE);
    }

    /* Init the DTIM time (ms) */
    g_ulAppCtrlWifiDtimTime = g_tAppCtrlWifiConnectSettings.ulDtimInterval;

    // the idle of the connection retry
    g_ulAppCtrlDoAutoConnectCumulativeTime = 0;
    g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
    g_ulAppCtrlAutoConnectInterval = g_tAppCtrlWifiConnectSettings.ulAutoConnectIntervalInit;

    /* create timer to trig led */
    timer_led_def.ptimer = BleWifi_Ctrl_LedTimeout;
    g_tAppCtrlLedTimer = osTimerCreate(&timer_led_def, osTimerOnce, NULL);
    if(g_tAppCtrlLedTimer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create LED timer fail \r\n");
    }

    /* create http post timer */
    timer_http_post_def.ptimer = BleWifi_Ctrl_HttpPostData;
    g_tAppCtrlHttpPostTimer = osTimerCreate(&timer_http_post_def, osTimerPeriodic, NULL);
    if(g_tAppCtrlHttpPostTimer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create Http Post timer fail \r\n");
    }
        // When got ip then start timer to post data
    osTimerStop(g_tAppCtrlHttpPostTimer);
    osTimerStart(g_tAppCtrlHttpPostTimer, POST_DATA_TIME);

    /* create hourly http post retry timer */
    timer_hrly_http_post_retry_def.ptimer = BleWifi_Ctrl_HttpPostData;
    g_tAppCtrlHourlyHttpPostRetryTimer= osTimerCreate(&timer_hrly_http_post_retry_def, osTimerOnce, NULL);
    if(g_tAppCtrlHourlyHttpPostRetryTimer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create Hourly Http Post Retry timer fail \r\n");
    }

    /* create type 1,2,3 http post retry timer */
    timer_type1_2_3_http_post_retry_def.ptimer = BleWifi_Ctrl_Type1_2_3_HttpPostData_Retry;
    g_tAppCtrlType1_2_3_HttpPostRetryTimer = osTimerCreate(&timer_type1_2_3_http_post_retry_def, osTimerOnce, NULL);
    if(g_tAppCtrlType1_2_3_HttpPostRetryTimer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create Type 1.2.3 Http Post Retry timer fail \r\n");
    }

    /* the init stat of LED is none off */
    g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_ALWAYS_OFF;
    BleWifi_Ctrl_DoLedDisplay();

    /* the init state of SYS is init */
    g_ubAppCtrlSysStatus = BLEWIFI_CTRL_SYS_INIT;
    // start the sys timer
    osTimerStop(g_tAppCtrlSysTimer);
    osTimerStart(g_tAppCtrlSysTimer, BLEWIFI_COM_SYS_TIME_INIT);

    /* Init Buttton Action */
    Sensor_ButtonPress_Init();

    /* Init Door Action */
    Sensor_DoorPress_Init();

    /* Ring Buffer reset */
    Sensor_Data_ResetBuffer();

    /* Init Auxadc for battery */
    Sensor_Auxadc_Init();

    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_DC_SLOPE, 0, MW_FIM_GP12_DC_SLOPE_SIZE, (uint8_t*)&g_tDCSlope))
    {
        // if fail, return fail
        BLEWIFI_ERROR("BLEWIFI: Read FIM fail \r\n");
    }

    // Calibration data
    g_ulHalAux_AverageCount = AverageTimes;
    g_tHalAux_CalData.fSlopeIo = g_tDCSlope.fSlope;
    g_tHalAux_CalData.wDcOffsetIo = g_tDCSlope.DC;
    
    g_u8ShortPressButtonProcessed = 0; //It means the time of button pressed is less than 5s 20191018EL

    //BleWifi_Ctrl_LedStatusChange();//Goter
}

