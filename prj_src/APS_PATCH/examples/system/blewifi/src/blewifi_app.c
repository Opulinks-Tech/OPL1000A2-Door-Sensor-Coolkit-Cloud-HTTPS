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
 * @file blewifi_app.c
 * @author Vincent Chen
 * @date 12 Feb 2018
 * @brief File creates the wifible app task architecture.
 *
 */
#include "blewifi_configuration.h"
#include "blewifi_common.h"
#include "blewifi_app.h"
#include "blewifi_wifi_api.h"
#include "blewifi_ble_api.h"
#include "blewifi_ctrl.h"
#include "blewifi_ctrl_http_ota.h"
#include "iot_data.h"
#include "sys_common_api.h"
#include "ps_public.h"
#include "mw_fim_default_group03.h"
#include "mw_fim_default_group03_patch.h"
#include "app_at_cmd.h"
#include "at_cmd_task_patch.h"
#include "at_cmd_common.h"
#include "wifi_api.h"
#include "hal_auxadc_patch.h"
#include "ps_patch.h"

blewifi_ota_t *gTheOta = 0;

void BleWifiAppInit(void)
{
    T_MwFim_SysMode tSysMode;
	gTheOta = 0;

    // get the settings of system mode
	if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP03_PATCH_SYS_MODE, 0, MW_FIM_SYS_MODE_SIZE, (uint8_t*)&tSysMode))
    {
        // if fail, get the default value
        memcpy(&tSysMode, &g_tMwFimDefaultSysMode, MW_FIM_SYS_MODE_SIZE);
    }

    if ((tSysMode.ubSysMode == MW_FIM_SYS_MODE_MP) || (tSysMode.ubSysMode == MW_FIM_SYS_MODE_INIT))
    {
        set_echo_on(false);
    }

    if(tSysMode.ubSysMode == MW_FIM_SYS_MODE_USER)
    {
        ps_32k_xtal_measure(1000);
    }
    // only for the user mode
    if ((tSysMode.ubSysMode == MW_FIM_SYS_MODE_INIT) || (tSysMode.ubSysMode == MW_FIM_SYS_MODE_USER))
    {
        /* Aux ADC calibration Initialization */
        Hal_Aux_AdcCal_Init();

        /* SNTP Initialization */
        BleWifi_SntpInit();

        /* Wi-Fi Initialization */
        BleWifi_Wifi_Init();

        /* BLE Stack Initialization */
        BleWifi_Ble_Init();

        /* blewifi "control" task Initialization */
        BleWifi_Ctrl_Init();

        /* blewifi HTTP OTA */
        #if (WIFI_OTA_FUNCTION_EN == 1)
        blewifi_ctrl_http_ota_task_create();
        #endif

        /* IoT device Initialization */
        #if (IOT_DEVICE_DATA_TX_EN == 1) || (IOT_DEVICE_DATA_RX_EN == 1)
        Iot_Data_Init();
        #endif

        /* RF Power settings */
        BleWifi_RFPowerSetting(BLEWIFI_COM_RF_POWER_SETTINGS);

    }

    // update the system mode
    BleWifi_Ctrl_SysModeSet(tSysMode.ubSysMode);

    // add app cmd
    app_at_cmd_add();

    // Enable CR+LF as default
    at_cmd_crlf_term_set(CRLF_ENABLE);
}
