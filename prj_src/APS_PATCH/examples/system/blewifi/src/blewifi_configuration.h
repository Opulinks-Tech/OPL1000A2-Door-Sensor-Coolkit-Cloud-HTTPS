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

#ifndef __BLEWIFI_CONFIGURATION_H__
#define __BLEWIFI_CONFIGURATION_H__

// Common part

/*
Smart sleep
*/
#define BLEWIFI_COM_POWER_SAVE_EN       (1)     // 1: enable    0: disable

/*
RF Power

.-----------------.----------------.
|                 |  BLE Low Power |
:-----------------+----------------+
| WIFI Low power  |  0x00          |
:-----------------+----------------+
| WIFI Low power  |  0x20          |
|   + 2 DB        |                |
:-----------------+----------------+
| WIFI Low power  |  0x40          |
|   + 4 DB        |                |
:-----------------+----------------+
| WIFI High power |  0xB0          |
:-----------------+----------------+
| WIFI High power |  0xD0          |
|   + 3 DB        |                |
| (For SDK >= 20) |                |
'-----------------+----------------+

*/
#define BLEWIFI_COM_RF_POWER_SETTINGS   (0xA0)
#define BLEWIFI_COM_RF_SMPS_SETTING     (2)     //0 : 1.2,  2 : 1.4

/*
SNTP
*/
#define SNTP_FUNCTION_EN      (0)                   // SNTP 1: enable / 0: disable
#define SNTP_SERVER           "1.cn.pool.ntp.org"   // SNTP Server
#define SNTP_PORT_NUM         "123"                 // SNTP port Number
#define SNTP_TIME_ZONE        (0)                   // Time zone: GMT+0

/*
BLE OTA FLAG
*/
#define BLE_OTA_FUNCTION_EN      (1)  // BLE OTA Function Enable (1: enable / 0: disable)

/*
WIFI OTA FLAG
*/
#define WIFI_OTA_FUNCTION_EN     (1)  // WIFI OTA Function Enable (1: enable / 0: disable)
#define WIFI_OTA_HTTP_URL        "http://192.168.0.100/ota.bin"

/*
IoT device
    1. if want to send data to server, set the Tx path to enable
    2. if want to receive data from server, set the Rx path to enable
*/
#define IOT_DEVICE_DATA_TX_EN               (1)     // 1: enable / 0: disable
#define IOT_DEVICE_DATA_RX_EN               (0)     // 1: enable / 0: disable
#define IOT_DEVICE_DATA_TASK_STACK_SIZE_TX  (512*3)
#define IOT_DEVICE_DATA_TASK_STACK_SIZE_RX  (1024)
#define IOT_DEVICE_DATA_QUEUE_SIZE_TX       (20)

/*
after the time, change the system state
*/
#define BLEWIFI_COM_SYS_TIME_INIT       (15000)      // ms from init to normal
#define BLEWIFI_COM_SYS_TIME_NORMAL     (120000)    // ms from normal to ble off


// BLE part
/*
BLE Service UUID
*/
#define BLEWIFI_BLE_UUID_SERVICE        0xAAAA
#define BLEWIFI_BLE_UUID_DATA_IN        0xBBB0
#define BLEWIFI_BLE_UUID_DATA_OUT       0xBBB1

/* Device Name
method 1: use prefix + mac address
    The max length of prefix is 17 bytes.
    The length of mac address is 12 bytes.

    Ex: OPL_33:44:55:66

method 2: full name
    The max length of device name is 29 bytes.
*/
#define BLEWIFI_BLE_DEVICE_NAME_METHOD      1           // 1 or 2
#define BLEWIFI_BLE_DEVICE_NAME_POST_COUNT  4           // for method 1 "OPL_33:44:55:66"
#define BLEWIFI_BLE_DEVICE_NAME_PREFIX      "ck_"      // for method 1 "OPL_33:44:55:66"
#define BLEWIFI_BLE_DEVICE_NAME_FULL        "OPL1000"   // for method 2

/* Advertisement Interval When BLE disconnectted:
 0xFFFF is deifined 30 min in dirver part
*/
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MIN  0xFFFF  // For 30 min   //Goter
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MAX  0xFFFF  // For 30 min

/* For network period
100 (ms) / 0.625 (ms) = 160 = 0xA0
*/
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MIN   0xA0  // For 100 millisecond
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MAX   0xA0  // For 100 millisecond

/* For Calibration
100 (ms) / 0.625 (ms) = 160 = 0xA0
*/
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_CAL_MIN   0xA0  // For 100 millisecond
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_CAL_MAX   0xA0  // For 100 millisecond

/* For the initial settings of Advertisement Interval */
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_INIT_MIN     BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MIN
#define BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_INIT_MAX     BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MAX

// Wifi part
/* Connection Retry times:
When BLE send connect reqest for connecting AP.
If failure will retry times which define below.
*/
#define BLEWIFI_WIFI_REQ_CONNECT_RETRY_TIMES    5


/* Auto Connection Interval:
if the auto connection is fail, the interval will be increased
    Ex: Init 5000, Diff 1000, Max 30000
        1st:  5000 ms
        2nd:  6000 ms
            ...
        26th: 30000 ms
        27th: 30000 ms
            ...
*/
#define BLEWIFI_WIFI_AUTO_CONNECT_INTERVAL_INIT     (5000)      // ms
#define BLEWIFI_WIFI_AUTO_CONNECT_INTERVAL_DIFF     (1000)      // ms
#define BLEWIFI_WIFI_AUTO_CONNECT_INTERVAL_MAX      (30000)     // ms
#define BLEWIFI_WIFI_AUTO_CONNECT_LIFETIME_MAX      (180000)    //ms

#define BLEWIFI_POST_FAIL_LED_MAX                   (60000)     //ms

/* DTIM the times of Interval: ms
*/
#define BLEWIFI_WIFI_DTIM_INTERVAL                  (3000)      // ms

/* Http Post */
#define SERVER_PORT    "8080"
#define SERVER_NAME    "testapi.coolkit.cn"
#define APIKEY         "00000000-0000-0000-0000-000000000000"
#define DEVICE_ID      "0000000000"
#define CHIP_ID        "000000000000"
#define MODEL_ID       "000-000-00"

#define HOSTINFO_URL   "testapi.coolkit.cn:8080"
#define HOSTINFO_DIR   "/api/user/device/update"

/* GPIO IO Port */
#define BUTTON_IO_PORT      (GPIO_IDX_04)
#define MAGNETIC_IO_PORT    (GPIO_IDX_05)
#define BATTERY_IO_PORT     (GPIO_IDX_02)
#define LED_IO_PORT         (GPIO_IDX_21)

/* LED time : unit: ms */
#define LED_TIME_BLE_ON_1           (100)
#define LED_TIME_BLE_OFF_1          (400)

#define LED_TIME_AUTOCONN_ON_1      (100)
#define LED_TIME_AUTOCONN_OFF_1     (1900)

#define LED_TIME_OTA_ON             (50)
#define LED_TIME_OTA_OFF            (1950)

#define LED_TIME_ALWAYS_OFF         (0x7FFFFFFF)

#define LED_TIME_TEST_MODE_ON_1       (200)
#define LED_TIME_TEST_MODE_OFF_1      (200)
#define LED_TIME_TEST_MODE_ON_2       (200)
#define LED_TIME_TEST_MODE_OFF_2      (200)
#define LED_TIME_TEST_MODE_ON_3       (200)
#define LED_TIME_TEST_MODE_OFF_3      (200)
#define LED_TIME_TEST_MODE_ON_4       (200)
#define LED_TIME_TEST_MODE_OFF_4      (600)

#define LED_TIME_NOT_CNT_SRV_ON_1   (100)
#define LED_TIME_NOT_CNT_SRV_OFF_1  (100)
#define LED_TIME_NOT_CNT_SRV_ON_2   (100)
#define LED_TIME_NOT_CNT_SRV_OFF_2  (700)

#if 1   // 20200528, Terence change offline led as same as NOT_CNT_SRV
#define LED_TIME_OFFLINE_ON_1   (100)
#define LED_TIME_OFFLINE_OFF_1  (100)
#define LED_TIME_OFFLINE_ON_2   (100)
#define LED_TIME_OFFLINE_OFF_2  (700)
#else
#define LED_TIME_OFFLINE_ON         (1000)
#define LED_TIME_OFFLINE_OFF        (1000)
#endif

#define LED_TIME_SHORT_PRESS_ON     (100)//Goter

#define LED_TIME_BOOT_ON_1          (100)//Goter
#define LED_TIME_BOOT_OFF_1         (100)//Goter
#define LED_TIME_BOOT_ON_2          (100)//Goter

#define LED_MP_NO_ROUTER_ON_1       (100)
#define LED_MP_NO_ROUTER_OFF_1      (1900)
#define LED_MP_NO_SERVER_ON_1       (100)
#define LED_MP_NO_SERVER_OFF_1      (100)
#define LED_MP_NO_SERVER_ON_2       (100)
#define LED_MP_NO_SERVER_OFF_2      (1700)

#define LED_ALWAYS_ON               (0x7FFFFFFF)

#if 0
/* Button Debounce time : unit: ms */
#define TIMEOUT_DEBOUNCE_TIME        (30)      // 30 ms
#define BLEWIFI_CTRL_KEY_PRESS_TIME  (5000)    // ms
#define BLEWIFI_CTRL_BLEADVSTOP_TIME (180000)  // ms
#endif

/* BUTTON SENSOR Config */
#define BLEWIFI_CTRL_BUTTON_SENSOR_EN               (1)
#define BLEWIFI_CTRL_BUTTON_IO_PORT                 GPIO_IDX_04
#define BLEWIFI_CTRL_BUTTON_PRESS_LEVEL             GPIO_LEVEL_LOW          // GPIO_LEVEL_HIGH | GPIO_LEVEL_LOW
#define BLEWIFI_CTRL_BUTTON_TIMEOUT_DEBOUNCE_TIME   (30)      // 30 ms
#define BLEWIFI_CTRL_BUTTON_PRESS_TIME              (5000)    // ms
#define BLEWIFI_CTRL_BUTTON_RELEASE_COUNT_TIME      (800)     // ms

#define BLEWIFI_CTRL_BLEADVSTOP_TIME                (180000)  // ms

/* Door Debounce time : unit: ms */
#define DOOR_DEBOUNCE_TIMEOUT        (200)     // 200ms

/* SSL RX Socket Timeout : unit: ms */
#define SOCKET_CONNECT_TIMEOUT       (3000)
#define SSL_HANDSHAKE_TIMEOUT        (4600)
#define SSL_SOCKET_TIMEOUT           (3500)    // 2 sec
#define OTA_SOCKET_TIMEOUT           (30000)

#define OTA_TOTAL_TIMEOUT           (180000)

/*
Define the moving average count for temperature and VBAT
*/
#define SENSOR_MOVING_AVERAGE_COUNT         (1)

/*
Define Maximum Voltage & Minimum Voltage
*/
#define MAXIMUM_VOLTAGE_DEF     (3.0f)    //(3.0f)
#define MINIMUM_VOLTAGE_DEF     (1.5f)    //(1.5f)
#define VOLTAGE_OFFSET          (0.0f)

/*
Define CR+LF Enable / Disable (Windows:CR+LF, Linux:CR and Mac:LF)
*/
#define CRLF_ENABLE             (1)

/*
Define Timer:
*/
#define TEST_MODE_TIMER_DEF       (200)     // ms
#define TEST_MODE_WIFI_DEFAULT_SSID     "8DB0839D"
#define TEST_MODE_WIFI_DEFAULT_PASSWORD "094FAFE8"

// Application Tasks Stack Size
#define OS_TASK_STACK_SIZE_BLEWIFI_CTRL_APP         (1024)

// Set Uart baudrate
#define BLEWIFI_SYS_UART_BAUDRATE (9600)

#define MP_MODE_SCAN_AP_TIMEOUT   (3000)    // ms

#endif /* __BLEWIFI_CONFIGURATION_H__ */

