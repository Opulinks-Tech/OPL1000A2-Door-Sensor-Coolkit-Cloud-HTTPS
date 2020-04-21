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

#include <stdlib.h>
#include <string.h>
#include "blewifi_ctrl.h"
#include "at_cmd.h"
#include "at_cmd_common.h"
#include "at_cmd_data_process.h"
#include "hal_flash.h"
#include "at_cmd_task.h"
#include "at_cmd_func_patch.h"
#include "blewifi_configuration.h"
#include "blewifi_ctrl.h"
#include "blewifi_data.h"
#include "blewifi_wifi_api.h"
#include "blewifi_ble_api.h"
#include "app_at_cmd.h"

#include "agent_patch.h"
#include "mw_fim.h"
#include "at_cmd_data_process_patch.h"
#include "ftoa_util.h"
#include "mw_fim_default_group12_project.h"
#include "mw_fim_default_group03.h"
#include "sys_common_api.h"
#include "wifi_api.h"
#include "scrt_patch.h"
#include "sensor_common.h"
#include "sensor_battery.h"
#include "mw_ota_def.h"
#include "mw_ota.h"
#include "hal_pin.h"
#include "hal_auxadc_internal.h"
#include "hal_vic.h"

uint8_t g_WifiSSID[WIFI_MAX_LENGTH_OF_SSID];
uint8_t g_WifiPassword[MAX_WIFI_PASSWORD_LEN];
uint8_t g_WifiSSIDLen;
uint8_t g_WifiPasswordLen;
osTimerId g_tAppFactoryWifiConnTimerId;

extern uint32_t g_ulHalAux_AverageCount;
extern T_HalAuxCalData g_tHalAux_CalData;

extern T_MwFim_GP12_HttpPostContent g_tHttpPostContent;

uint8_t g_ATWIFICNTRetry =0;

//#define CAL_DEBUG

#define DBG_MSG_LL_ARR(name, arr, len)                            \
{                                                                 \
    int var_idx;                                                  \
    msg_print_uart1(name"(%d):[", len);                           \
    for (var_idx = 0; var_idx < len; var_idx++)                   \
        msg_print_uart1("%02X ", *((uint8_t *) arr + var_idx));   \
    msg_print_uart1("]\r\n");                                     \
}

#define BUFFER_SIZE_128 128
#define SHA256_AUTH_KEY_FORMAT "%s%s%s%s%s"

#define AT_SUCCESS 0
#define AT_FAIL    1

//#define AT_LOG                      msg_print_uart1
#define AT_LOG(...)

/* For at_cmd_sys_write_fim */
#define AT_FIM_DATA_LENGTH 2 /* EX: 2 = FF */
#define AT_FIM_DATA_LENGTH_WITH_COMMA (AT_FIM_DATA_LENGTH + 1) /* EX: 3 = FF, */

#define AT_AUXADC_IO_VOLTAGE_GET_AVERAGE_COUNT    (3000)
#define AT_AUXADC_VALUE_GET_AVERAGE_COUNT         (1000)


typedef struct
{
    uint32_t u32Id;
    uint16_t u16Index;
    uint16_t u16DataTotalLen;

    uint32_t u32DataRecv;       // Calcuate the receive data
    uint32_t TotalSize;         // user need to input total bytes

    char     u8aReadBuf[8];
    uint8_t  *ResultBuf;
    uint32_t u32StringIndex;       // Indicate the location of reading string
    uint16_t u16Resultindex;       // Indicate the location of result string
    uint8_t  fIgnoreRestString;    // Set a flag for last one comma
    uint8_t  u8aTemp[1];
} T_AtFimParam;

typedef struct
{
    uint16_t u16DataTotalLen;

    uint32_t u32DataRecv;       // Calcuate the receive data
    uint32_t TotalSize;         // user need to input total bytes

    char     u8aReadBuf[8];
    uint8_t  *ResultBuf;
    uint32_t u32StringIndex;       // Indicate the location of reading string
    uint16_t u16Resultindex;       // Indicate the location of result string
    uint8_t  fIgnoreRestString;    // Set a flag for last one comma
    uint8_t  u8aTemp[1];
} T_AtSendParam;

typedef struct
{
    char ubaDeviceId[DEVICE_ID_LEN];
    char ubaApiKey[API_KEY_LEN];
    char ubaWifiMac[CHIP_ID_LEN];
    char ubaBleMac[CHIP_ID_LEN];
    char ubaDeviceModel[MODEL_ID_LEN];
} T_SendJSONParam;

int app_at_cmd_sys_read_fim(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};
    uint32_t i = 0;
    uint8_t *readBuf = NULL;
    uint32_t u32Id  = 0;
    uint16_t u16Index  = 0;
    uint16_t u16Size  = 0;

    if(!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    if(argc != 4)
    {
        AT_LOG("invalid param number\r\n");
        goto done;
    }

    u32Id  = (uint32_t)strtoul(argv[1], NULL, 16);
    u16Index  = (uint16_t)strtoul(argv[2], NULL, 0);
    u16Size  = (uint16_t)strtoul(argv[3], NULL, 0);

    if((u16Size == 0) )
    {
        AT_LOG("invalid size[%d]\r\n", u16Size);
        goto done;
    }

    switch(mode)
    {
        case AT_CMD_MODE_SET:
        {
            readBuf = (uint8_t *)malloc(u16Size);

            if(MW_FIM_OK == MwFim_FileRead(u32Id, u16Index, u16Size, readBuf))
            {
                msg_print_uart1("%02X",readBuf[0]);
                for(i = 1 ; i < u16Size ; i++)
                {
                    msg_print_uart1(",%02X",readBuf[i]);
                }
            }
            else
            {
                goto done;
            }

            msg_print_uart1("\r\n");
            break;
        }

        default:
            goto done;
    }

    iRet = 1;
done:
    if(iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    if(readBuf != NULL)
        free(readBuf);
    return iRet;
}

int write_fim_handle(uint32_t u32Type, uint8_t *u8aData, uint32_t u32DataLen, void *pParam)
{
    T_AtFimParam *ptParam = (T_AtFimParam *)pParam;

    uint8_t  iRet = 0;
    uint8_t  u8acmp[] = ",\0";
    uint32_t i = 0;

    ptParam->u32DataRecv += u32DataLen;

    /* If previous segment is error then ignore the rest of segment */
    if(ptParam->fIgnoreRestString)
    {
        goto done;
    }

    for(i = 0 ; i < u32DataLen ; i++)
    {
        if(u8aData[i] != u8acmp[0])
        {
            if(ptParam->u32StringIndex >= AT_FIM_DATA_LENGTH)
            {
                ptParam->fIgnoreRestString = 1;
                goto done;
            }

            /* compare string. If not comma then store into array. */
            ptParam->u8aReadBuf[ptParam->u32StringIndex] = u8aData[i];
            ptParam->u32StringIndex++;
        }
        else
        {
            /* Convert string into Hex and store into array */
            ptParam->ResultBuf[ptParam->u16Resultindex] = (uint8_t)strtoul(ptParam->u8aReadBuf, NULL, 16);

            /* Result index add one */
            ptParam->u16Resultindex++;

            /* re-count when encounter comma */
            ptParam->u32StringIndex=0;
        }
    }

    /* If encounter the last one comma
       1. AT_FIM_DATA_LENGTH:
       Max character will pick up to compare.

       2. (ptParam->u16DataTotalLen - 1):
       If total length minus 1 is equal (ptParam->u16Resultindex) mean there is no comma at the rest of string.
    */
    if((ptParam->u16Resultindex == (ptParam->u16DataTotalLen - 1)) && (ptParam->u32StringIndex >= AT_FIM_DATA_LENGTH))
    {
        ptParam->ResultBuf[ptParam->u16Resultindex] = (uint8_t)strtoul(ptParam->u8aReadBuf, NULL, 16);

        /* Result index add one */
        ptParam->u16Resultindex++;
    }

    /* Collect array data is equal to total lengh then write data to fim. */
    if(ptParam->u16Resultindex == ptParam->u16DataTotalLen)
    {
       	if(MW_FIM_OK == MwFim_FileWrite(ptParam->u32Id, ptParam->u16Index, ptParam->u16DataTotalLen, ptParam->ResultBuf))
        {
            msg_print_uart1("OK\r\n");
        }
        else
        {
            ptParam->fIgnoreRestString = 1;
        }
    }
    else
    {
        goto done;
    }

done:
    if(ptParam->TotalSize >= ptParam->u32DataRecv)
    {
        if(ptParam->fIgnoreRestString)
        {
            msg_print_uart1("ERROR\r\n");
        }

        if(ptParam != NULL)
        {
            if (ptParam->ResultBuf != NULL)
            {
                free(ptParam->ResultBuf);
            }
            free(ptParam);
            ptParam = NULL;
        }
    }

    return iRet;
}

int app_at_cmd_sys_write_fim(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    /* Initialization the value */
    T_AtFimParam *tAtFimParam = (T_AtFimParam*)malloc(sizeof(T_AtFimParam));
    if(tAtFimParam == NULL)
    {
        goto done;
    }
    memset(tAtFimParam, 0, sizeof(T_AtFimParam));

    if(!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    if(argc != 4)
    {
        msg_print_uart1("invalid param number\r\n");
        goto done;
    }

    /* save parameters to process uart1 input */
    tAtFimParam->u32Id = (uint32_t)strtoul(argv[1], NULL, 16);
    tAtFimParam->u16Index = (uint16_t)strtoul(argv[2], NULL, 0);
    tAtFimParam->u16DataTotalLen = (uint16_t)strtoul(argv[3], NULL, 0);

    /* If user input data length is 0 then go to error.*/
    if(tAtFimParam->u16DataTotalLen == 0)
    {
        goto done;
    }

    switch(mode)
    {
        case AT_CMD_MODE_SET:
        {
            tAtFimParam->TotalSize = ((tAtFimParam->u16DataTotalLen * AT_FIM_DATA_LENGTH_WITH_COMMA) - 1);

            /* Memory allocate a memory block for pointer */
            tAtFimParam->ResultBuf = (uint8_t *)malloc(tAtFimParam->u16DataTotalLen);
            if(tAtFimParam->ResultBuf == NULL)
                goto done;

            // register callback to process uart1 input
            agent_data_handle_reg(write_fim_handle, tAtFimParam);

            // redirect uart1 input to callback
            data_process_lock_patch(LOCK_OTHERS, (tAtFimParam->TotalSize));

            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if(iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
        if (tAtFimParam != NULL)
        {
            if (tAtFimParam->ResultBuf != NULL)
            {
		        free(tAtFimParam->ResultBuf);
            }
            free(tAtFimParam);
            tAtFimParam = NULL;
        }
    }

    return iRet;
}

int app_at_cmd_sys_dtim_time(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    uint32_t ulDtimTime;

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            msg_print_uart1("DTIM Time: %d\r\n", BleWifi_Ctrl_DtimTimeGet());
            break;
        }

        case AT_CMD_MODE_SET:
        {
            // at+dtim=<value>
            if(argc != 2)
            {
                AT_LOG("invalid param number\r\n");
                goto done;
            }

            ulDtimTime = strtoul(argv[1], NULL, 0);
            BleWifi_Ctrl_DtimTimeSet(ulDtimTime);
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if(iRet)
    {
        msg_print_uart1("OK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_get_voltage(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    float fVBat;
    float fVBatAverage = 0;

#ifdef CAL_DEBUG
    // For ADC value use
    uint32_t ulAdcConditionValue;
#endif

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_CAL_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_CAL_MAX);
            g_ulHalAux_AverageCount = AT_AUXADC_IO_VOLTAGE_GET_AVERAGE_COUNT;
            Hal_Aux_IoVoltageGet(BATTERY_IO_PORT, &fVBat);
            fVBatAverage = fVBat;

#ifdef CAL_DEBUG
            // Print IO7 ADC value
            Hal_Pin_ConfigSet(7, PIN_TYPE_AUX_7, PIN_DRIVING_FLOAT);
            if (1 != Hal_Aux_SourceSelect(HAL_AUX_SRC_GPIO, 7))
                return 0;

            if (1 != Hal_Aux_AdcValueGet(&ulAdcConditionValue))
                 return 0;

            msg_print_uart1("ADC [%d]\n",ulAdcConditionValue);
#endif
            BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MAX);
            // fVBatPercentage need multiple 2 then add voltage offset (fVoltageOffset)
            fVBatAverage = (fVBatAverage * 2);

            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if(iRet)
    {
        msg_print_uart1("AT+GETVOLTAGE=%f\r\n",fVBatAverage);
    }
    else
    {
        msg_print_uart1("AT+GETVOLTAGE=ERROR\r\n");
    }

    return iRet;

}

int app_at_cmd_sys_voltage_offset(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    uint8_t Index;
    uint32_t ulAdcValue;
    float fAdcSumValue;
    float fVoltage;
    T_MwFim_GP12_DCSlope DCSlope;

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_DC_SLOPE, 0, MW_FIM_GP12_DC_SLOPE_SIZE, (uint8_t*)&DCSlope))
            {
                // if fail, return fail
                 goto done;
            }

            // Calibration data
            msg_print_uart1("Slope = %d, DC = %d\n",(int)(DCSlope.fSlope * 1000000), DCSlope.DC);

            break;
        }

        case AT_CMD_MODE_SET:
        {
            if(argc != 3)
            {
                AT_LOG("invalid param number\r\n");
                goto done;
            }

            Index = atoi(argv[1]);
            fVoltage = atoi(argv[2]);

            if ((Index != 1) && (Index != 2))
                goto done;

            // Base on voltage dividing circuit, so original voltage (MAXIMUM_VOLTAGE_DEF & MINIMUM_VOLTAGE_DEF) must multiply 2000.
            if (fVoltage > ((MAXIMUM_VOLTAGE_DEF * 2) * 1000))
                goto done;

            if (fVoltage < ((MINIMUM_VOLTAGE_DEF * 2) * 1000))
                goto done;

            // Get IO7 ADC value
            Hal_Pin_ConfigSet(7, PIN_TYPE_AUX_7, PIN_DRIVING_FLOAT);
            if (1 != Hal_Aux_SourceSelect(HAL_AUX_SRC_GPIO, 7))
                return 0;

            BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_CAL_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_CAL_MAX);

            g_ulHalAux_AverageCount = AT_AUXADC_VALUE_GET_AVERAGE_COUNT;
            Hal_Aux_AdcValueGet(&ulAdcValue);
            fAdcSumValue = ulAdcValue;

            BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MAX);
#if 0
            for (i = 0 ;i < AverageTimes ;i++)
            {
                if (1 != Hal_Aux_AdcValueGet(&ulAdcValue))
                     return 0;

                fAdcSumValue += (float)ulAdcValue;
            }

            fAdcSumValue = (fAdcSumValue / AverageTimes);
#endif

#ifdef CAL_DEBUG
            msg_print_uart1("ADC [%f]\n",fAdcSumValue);
#endif

            if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_DC_SLOPE, 0, MW_FIM_GP12_DC_SLOPE_SIZE, (uint8_t*)&DCSlope))
            {
                // if fail, return fail
                goto done;
            }

            // Calculate DC and Slope
            DCSlope.fVoltage[Index-1] = fVoltage;
            DCSlope.fADC[Index-1] = fAdcSumValue;

#ifdef CAL_DEBUG
            msg_print_uart1("DC %f  voltage %f\n", DCSlope.fADC[Index-1], DCSlope.fVoltage[Index-1]);
#endif

            if (Index == 2)
            {
                DCSlope.fSlope = (((DCSlope.fVoltage[0]/2000.0) - (DCSlope.fVoltage[1]/2000.0)) / ((DCSlope.fADC[0]) - (DCSlope.fADC[1])));
                DCSlope.DC = (int)(DCSlope.fADC[0] - ((DCSlope.fVoltage[0]/2000.0) / DCSlope.fSlope));

#ifdef CAL_DEBUG
                msg_print_uart1("Slope [%f], DC = [%d]\n",DCSlope.fSlope, DCSlope.DC);
#endif

                // Calibration data
                g_tHalAux_CalData.fSlopeIo = DCSlope.fSlope;
                g_tHalAux_CalData.wDcOffsetIo = DCSlope.DC;
            }

            if (MW_FIM_OK != MwFim_FileWrite(MW_FIM_IDX_GP12_PROJECT_DC_SLOPE, 0, MW_FIM_GP12_DC_SLOPE_SIZE, (uint8_t*)&DCSlope))
            {
                // if fail, return fail
                goto done;
            }

            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if(iRet)
    {
        msg_print_uart1("AT+CALIBRATION=OK\r\n");
    }
    else
    {
        msg_print_uart1("AT+CALIBRATION=ERROR\r\n");
    }

    return iRet;

}

void Write_Json_BLE_MAC_Data(T_SendJSONParam *DeviceData)
{
    uint8_t ubaBLEMacAddr[6];

    // Write Json BLE MAC Data
    sscanf(DeviceData->ubaBleMac, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", &ubaBLEMacAddr[5], &ubaBLEMacAddr[4], &ubaBLEMacAddr[3], &ubaBLEMacAddr[2], &ubaBLEMacAddr[1], &ubaBLEMacAddr[0]);
    ble_set_config_bd_addr(ubaBLEMacAddr);
}

void Write_Json_Wifi_MAC_Data(T_SendJSONParam *DeviceData)
{
    uint8_t ubaWiFiMacAddr[6];

    // write Wi-Fi MAC
    sscanf(DeviceData->ubaWifiMac, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", &ubaWiFiMacAddr[0], &ubaWiFiMacAddr[1], &ubaWiFiMacAddr[2], &ubaWiFiMacAddr[3], &ubaWiFiMacAddr[4], &ubaWiFiMacAddr[5]);

    wifi_config_set_mac_address(WIFI_MODE_STA, ubaWiFiMacAddr);
    // apply the mac address from flash
    mac_addr_set_config_source(MAC_IFACE_WIFI_STA, MAC_SOURCE_FROM_FLASH);
}

#define SHOW_SHA256_RESULT_MAX_COUNT (3)
uint8_t Calc_Sha256_key_Data(T_SendJSONParam *DeviceData)
{
    int i;
    int len = 0;
    unsigned char *uwSHA256_Buff = NULL;
    //unsigned char uwSHA256_Buff[BUFFER_SIZE] = {0};
    uint8_t ubaSHA256CalcStrBuf[SCRT_SHA_256_OUTPUT_LEN];
    int TerminalNull = 1;
    T_MwFim_SysMode tSysMode;
    int count;

    int TotalLen = strlen(DeviceData->ubaDeviceId)
                 + strlen(DeviceData->ubaApiKey)
                 + strlen(DeviceData->ubaWifiMac)
                 + strlen(DeviceData->ubaBleMac)
                 + strlen(DeviceData->ubaDeviceModel)
                 + TerminalNull;

    uwSHA256_Buff = (unsigned char *) malloc(TotalLen);
    if (uwSHA256_Buff == NULL)
        return AT_FAIL;

    // Combine https Post key
    memset(uwSHA256_Buff, 0, TotalLen);
    len = sprintf((char *)uwSHA256_Buff, SHA256_AUTH_KEY_FORMAT
                                       , DeviceData->ubaDeviceId
                                       , DeviceData->ubaApiKey
                                       , DeviceData->ubaWifiMac
                                       , DeviceData->ubaBleMac
                                       , DeviceData->ubaDeviceModel);
    //msg_print_uart1("uwSHA256_Buff = %s\n", uwSHA256_Buff);

    if(0 ==  Sensor_SHA256_Calc(ubaSHA256CalcStrBuf, len, uwSHA256_Buff))
    {
        if(NULL != uwSHA256_Buff)
            free(uwSHA256_Buff);

        return AT_FAIL;
     }

    for(count = 0; count < SHOW_SHA256_RESULT_MAX_COUNT; count++) {
        for(i = 0; i < sizeof(ubaSHA256CalcStrBuf); i++) {
            msg_print_uart1("%02x", ubaSHA256CalcStrBuf[i]);
        }
        msg_print_uart1("\r\n");
    }

    msg_print_uart1("OK\r\n");

    if(NULL != uwSHA256_Buff)
        free(uwSHA256_Buff);

    // After write data, then switch to user mode.
    tSysMode.ubSysMode = MW_FIM_SYS_MODE_USER;
    if (MW_FIM_OK != MwFim_FileWrite(MW_FIM_IDX_GP03_SYS_MODE, 0, MW_FIM_SYS_MODE_SIZE, (uint8_t*)&tSysMode))
    {
        msg_print_uart1("\r\nUSER Mode Switch ERROR\r\n");
    }

    return AT_SUCCESS;
}

int Write_data_into_fim(T_SendJSONParam *DeviceData)
{
    T_MwFim_GP12_HttpPostContent HttpPostContent;
    memset(&HttpPostContent, 0, sizeof(T_MwFim_GP12_HttpPostContent));

    strcpy(HttpPostContent.ubaDeviceId ,DeviceData->ubaDeviceId);
    strcpy(HttpPostContent.ubaApiKey ,DeviceData->ubaApiKey);
    strcpy(HttpPostContent.ubaChipId ,DeviceData->ubaWifiMac);
    strcpy(HttpPostContent.ubaModelId ,DeviceData->ubaDeviceModel);

    memcpy(&g_tHttpPostContent,&HttpPostContent,sizeof(T_MwFim_GP12_HttpPostContent));

    if(MW_FIM_OK != MwFim_FileWrite(MW_FIM_IDX_GP12_PROJECT_DEVICE_AUTH_CONTENT, 0, MW_FIM_GP12_HTTP_POST_CONTENT_SIZE, (uint8_t*)&HttpPostContent))
    {
        msg_print_uart1("ERROR\r\n");
        return AT_FAIL;
    }

    Write_Json_BLE_MAC_Data(DeviceData);
    //DBG_MSG_LL_ARR("ubaBLEMacAddr raw", ubaBLEMacAddr, 6);

    Write_Json_Wifi_MAC_Data(DeviceData);
    //DBG_MSG_LL_ARR("ubaWiFiMacAddr raw", ubaWiFiMacAddr, 6);

    if (AT_FAIL == (Calc_Sha256_key_Data(DeviceData)))
        return AT_FAIL;

    return AT_SUCCESS;
}

char *ParseStringFromJson(char *ptr, char *string)
{
    char *ptrResult = NULL;
    if (NULL != (ptr = strstr(ptr, string)))
    {
        if (NULL != (ptr = strstr(ptr, "\"")))
        {
            if (NULL != (ptr = strstr(ptr, ":")))
            {
                if (NULL != (ptr = strstr(ptr, "\"")))
                {
                    ptrResult = ptr;

                    printf("ptrResult = %s\n", ptrResult);
                }
            }
        }
    }

    return (strtok(ptrResult,"\""));
}

int Send_burn_string_handle(uint32_t u32Type, uint8_t *u8aData, uint32_t u32DataLen, void *pParam)
{
    uint8_t  iRet = 0;
    char *ptr = (char *)u8aData;
    char deviceid[] = "deviceid";
    char apikey[] = "factory_apikey";
    char stamac[] = "sta_mac";
    char sapmac[] = "sap_mac";
    char model[] = "device_model";

    T_AtSendParam *ptParam = (T_AtSendParam *)pParam;

    /* Initialization the value */
    T_SendJSONParam *DeviceData = (T_SendJSONParam*)malloc(sizeof(T_SendJSONParam));
    if(DeviceData == NULL)
    {
        goto done;
    }
    memset(DeviceData, 0, sizeof(T_SendJSONParam));

    ptParam->u32DataRecv += u32DataLen;

    /* If previous segment is error then ignore the rest of segment */
    if(ptParam->fIgnoreRestString)
    {
        goto done;
    }

    ptr = (char *) malloc(u32DataLen);
    if (ptr == NULL)
        goto done;

    memset(ptr, 0, u32DataLen);
    memcpy(ptr, (char *) u8aData, u32DataLen);
    memcpy(DeviceData->ubaDeviceId , ParseStringFromJson(ptr, deviceid), DEVICE_ID_LEN);

    memset(ptr, 0, u32DataLen);
    memcpy(ptr, (char *) u8aData, u32DataLen);
    memcpy(DeviceData->ubaApiKey , ParseStringFromJson(ptr, apikey), API_KEY_LEN);

    memset(ptr, 0, u32DataLen);
    memcpy(ptr, (char *) u8aData, u32DataLen);
    memcpy(DeviceData->ubaWifiMac , ParseStringFromJson(ptr, stamac), CHIP_ID_LEN);

    memset(ptr, 0, u32DataLen);
    memcpy(ptr, (char *) u8aData, u32DataLen);
    memcpy(DeviceData->ubaBleMac , ParseStringFromJson(ptr, sapmac), CHIP_ID_LEN);

    memset(ptr, 0, u32DataLen);
    memcpy(ptr, (char *) u8aData, u32DataLen);
    memcpy(DeviceData->ubaDeviceModel , ParseStringFromJson(ptr, model), MODEL_ID_LEN);

    if (AT_SUCCESS !=  Write_data_into_fim(DeviceData))
        msg_print_uart1("ERROR\r\n");

done:
    if(ptParam->TotalSize >= ptParam->u32DataRecv)
    {
        if(ptParam->fIgnoreRestString)
        {
            msg_print_uart1("ERROR\r\n");
        }

        if(ptParam != NULL)
        {
            if (ptParam->ResultBuf != NULL)
            {
                free(ptParam->ResultBuf);
            }
            free(ptParam);
            ptParam = NULL;
        }
    }

        if (DeviceData != NULL)
    {
            free(DeviceData);
    }

    if(ptr != NULL)
    {
        free(ptr);
    }
    return iRet;
}

int app_at_cmd_sys_send(char *buf, int len, int mode)
{
        int iRet = 0;
        int argc = 0;
        char *argv[AT_MAX_CMD_ARGS] = {0};

        /* Initialization the value */
        T_AtSendParam *tAtSendParam = (T_AtSendParam*)malloc(sizeof(T_AtSendParam));
        if(tAtSendParam == NULL)
        {
            goto done;
        }
        memset(tAtSendParam, 0, sizeof(T_AtSendParam));

        if(!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
        {
            goto done;
        }

        if(argc != 2)
        {
            msg_print_uart1("invalid param number\r\n");
            goto done;
        }

        /* save parameters to process uart1 input */
        tAtSendParam->u16DataTotalLen = (uint16_t)strtoul(argv[1], NULL, 0);

        /* If user input data length is 0 then go to error.*/
        if(tAtSendParam->u16DataTotalLen == 0)
        {
            goto done;
        }

        switch(mode)
        {
            case AT_CMD_MODE_SET:
            {
                tAtSendParam->TotalSize = (tAtSendParam->u16DataTotalLen);

                /* Memory allocate a memory block for pointer */
                tAtSendParam->ResultBuf = (uint8_t *)malloc(tAtSendParam->u16DataTotalLen);
                if(tAtSendParam->ResultBuf == NULL)
                    goto done;

                // register callback to process uart1 input
                agent_data_handle_reg(Send_burn_string_handle, tAtSendParam);

                // redirect uart1 input to callback
                data_process_lock_patch(LOCK_OTHERS, (tAtSendParam->TotalSize));

                break;
            }

            default:
                goto done;
        }

        iRet = 1;

    done:
        if(iRet)
        {
            msg_print_uart1(">\n");
        }
        else
        {
            msg_print_uart1("ERROR\r\n");
            if (tAtSendParam != NULL)
            {
                if (tAtSendParam->ResultBuf != NULL)
                {
                    free(tAtSendParam->ResultBuf);
                }
                free(tAtSendParam);
                tAtSendParam = NULL;
            }
        }

        return iRet;
}

int app_at_cmd_sys_check_id(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};
    T_MwFim_GP12_HttpPostContent HttpPostContent;
    memset(&HttpPostContent, 0, sizeof(T_MwFim_GP12_HttpPostContent));

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            msg_print_uart1("%s\r\n", g_tHttpPostContent.ubaDeviceId);

            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if(iRet)
    {
        msg_print_uart1("\r\nOK\r\n");
    }
    else
    {
        msg_print_uart1("ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_device_id(char *buf, int len, int mode)
{
    int iRet = 0;
    
    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            if ( strcmp(g_tHttpPostContent.ubaDeviceId, DEVICE_ID) == 0) {
                goto done;
            }
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if(iRet)
    {
        msg_print_uart1("AT+DEVID=%s\r\n", g_tHttpPostContent.ubaDeviceId);
    }
    else
    {
        msg_print_uart1("AT+DEVID=ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_chip_id(char *buf, int len, int mode)
{
    int iRet = 0;

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            if ( strcmp(g_tHttpPostContent.ubaChipId, CHIP_ID) == 0) {
                goto done;
            }
            break;
        }

        default:
            goto done;
    }

    iRet = 1;

done:
    if(iRet)
    {
        msg_print_uart1("AT+CHIPID=%s\r\n", g_tHttpPostContent.ubaChipId);
    }
    else
    {
        msg_print_uart1("AT+CHIPID=ERROR\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_read_switch(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};
    T_MwFim_GP12_HttpPostContent HttpPostContent;
    memset(&HttpPostContent, 0, sizeof(T_MwFim_GP12_HttpPostContent));

    if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_READ:
        {
            if ( true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR))
                iRet = 1;
            else
                goto done;

            break;
        }

        default:
            goto done;
    }

done:
    if(iRet)
    {
        msg_print_uart1("AT+READ_SWITCH=ON\r\n");
    }
    else
    {
        msg_print_uart1("AT+READ_SWITCH=OFF\r\n");
    }

    return iRet;
}

int app_at_cmd_sys_factory(char *buf, int len, int mode)
{
    if (AT_CMD_MODE_EXECUTION == mode)
    {
        BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_IS_TEST_MODE_AT_FACTORY, NULL, 0);
    }

    return true;
}

int app_at_cmd_sys_version(char *buf, int len, int mode)
{
    uint16_t uwProjectId;
    uint16_t uwChipId;
    uint16_t uwFirmwareId;

    switch (mode)
    {
        case AT_CMD_MODE_READ:
            if (MW_OTA_OK == MwOta_VersionGet(&uwProjectId, &uwChipId, &uwFirmwareId))
            {
                msg_print_uart1("AT+VER=%u-%u-%u\r\n", uwProjectId, uwChipId, uwFirmwareId);
            }
            else
            {
                msg_print_uart1("AT+VER=ERROR\r\n");
            }
            break;

        default:
            msg_print_uart1("AT+VER=ERROR\r\n");
            break;
    }

    return true;
}

static void Wifi_Conn_TimerCallBack(void const *argu)
{
    if (( true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_TEST_MODE))
        && (true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE)))
    {
        msg_print_uart1("AT+SIGNAL=0\r\n");
        BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE, false);
        #ifdef TEST_MODE_DEBUG_ENABLE
        msg_print_uart1("Wifi_Conn_Time out \r\n");
        #endif

    }
}

static void Wifi_Conn_Start(char *ssid, char *password)
{
    uint8_t data[2];

    g_ATWIFICNTRetry =0;

    osTimerStop(g_tAppFactoryWifiConnTimerId);
    osTimerStart(g_tAppFactoryWifiConnTimerId, TIMEOUT_WIFI_CONN_TIME);

    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_AT_WIFI_MODE, true);

    memset(g_WifiSSID,0x00,sizeof(g_WifiSSID));
    memset(g_WifiPassword,0x00,sizeof(g_WifiPassword));

    g_WifiSSIDLen = strlen(ssid);
    g_WifiPasswordLen = strlen(password);
    memcpy((char *)g_WifiSSID, ssid, g_WifiSSIDLen);
    memcpy((char *)g_WifiPassword, password, g_WifiPasswordLen);

    data[0] = 1;    // Enable to scan AP whose SSID is hidden
    data[1] = 2;    // mixed mode

    BleWifi_Wifi_DoScan(data, 0);
}

int app_at_cmd_sys_wifi(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    if(!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
    {
        msg_print_uart1("AT+SIGNAL=0\r\n");
        goto done;
    }

    if ( mode == AT_CMD_MODE_READ && argc == 1 )
    {
        Wifi_Conn_Start(TEST_MODE_WIFI_DEFAULT_SSID, TEST_MODE_WIFI_DEFAULT_PASSWORD);
        msg_print_uart1("OK\r\n");
    }
    else if ( mode == AT_CMD_MODE_SET && argc == 3 )
    {
        Wifi_Conn_Start(argv[1], argv[2]);
        msg_print_uart1("OK\r\n");
    }
    else 
    {
        msg_print_uart1("AT+SIGNAL=0\r\n");
    }

done:
    return iRet;
}

int app_at_cmd_sys_gpio_read(char *buf, int len, int mode)
{
    int iRet = 0;
    int argc = 0;
    char *argv[GPIO_IDX_MAX + 1] = {0};

    int i;
    uint8_t u8GpioNum;
    uint32_t u32PinLevel[GPIO_IDX_MAX] = {0};

    if(!at_cmd_buf_to_argc_argv(buf, &argc, argv, GPIO_IDX_MAX + 1))
    {
        msg_print_uart1("AT+GPIO_READ=ERROR\r\n");
        goto done;
    }

    if(argc < 2 || (GPIO_IDX_MAX + 1) < argc )
    {
        AT_LOG("invalid param number\n");
        msg_print_uart1("AT+GPIO_READ=ERROR\r\n");
        goto done;
    }

    switch (mode)
    {
        case AT_CMD_MODE_SET:
        {
            for ( i=1;i<argc;i++ )
            {
                u8GpioNum = (uint8_t)strtoul(argv[i], NULL, 0);
                if ( u8GpioNum != BUTTON_IO_PORT && u8GpioNum != MAGNETIC_IO_PORT ) {
                    AT_LOG("invalid GPIO number\r\n");
                    msg_print_uart1("AT+GPIO_READ=ERROR\r\n");
                    goto done;
                }
                u32PinLevel[i-1] = Hal_Vic_GpioInput((E_GpioIdx_t)u8GpioNum);
            }

            msg_print_uart1("AT+GPIO_READ=");
            for ( i=1;i<argc;i++)
            {
                if ( i == 1 ) {
                    msg_print_uart1("%d", u32PinLevel[i-1] ? 1 : 0);
                }
                else {
                    msg_print_uart1(",%d", u32PinLevel[i-1] ? 1 : 0);
                }
            }
            msg_print_uart1("\r\n");
        }
            break;
        default:{
            msg_print_uart1("AT+GPIO_READ=ERROR\r\n");
            goto done;
        }
    }
    iRet = 1;
done:
    return iRet;
}

int app_at_cmd_sys_delete_data(char *buf, int len, int mode)
{
    int iRet = 0;

    switch (mode)
    {
        case AT_CMD_MODE_EXECUTION:
        {
            if(MW_FIM_FAIL == MwFim_FileWriteDefault(MW_FIM_IDX_GP03_LE_CFG, 0))
            {
                goto done;
            }
            if(MW_FIM_FAIL == MwFim_FileWriteDefault(MW_FIM_IDX_GP03_STA_MAC_ADDR, 0))
            {
                goto done;
            }
            if(MW_FIM_FAIL == MwFim_FileWriteDefault(MW_FIM_IDX_GP12_PROJECT_DEVICE_AUTH_CONTENT, 0))
            {
                goto done;
            }
        }
            break;
        default:
            goto done;
    }
    iRet = 1;
done:
    if (iRet) {
        msg_print_uart1("AT+DELETE_DATA=OK\r\n");
    }
    else {
        msg_print_uart1("AT+DELETE_DATA=ERROR\r\n");
    }
    return iRet;
}

#if (WIFI_OTA_FUNCTION_EN == 1)
int app_at_cmd_sys_do_wifi_ota(char *buf, int len, int mode)
{
    int argc = 0;
    char *argv[AT_MAX_CMD_ARGS] = {0};

    if (AT_CMD_MODE_EXECUTION == mode)
    {
        BleWifi_Wifi_OtaTrigReq(WIFI_OTA_HTTP_URL);
        //msg_print_uart1("OK\r\n");
    }
    else if (AT_CMD_MODE_SET == mode)
    {
        if (!at_cmd_buf_to_argc_argv(buf, &argc, argv, AT_MAX_CMD_ARGS))
        {
            return false;
        }

        BleWifi_Wifi_OtaTrigReq((uint8_t*)(argv[1]));
        //msg_print_uart1("OK\r\n");
    }

    return true;
}
#endif

at_command_t g_taAppAtCmd[] =
{
    { "at+readfim",     app_at_cmd_sys_read_fim,    "Read FIM data" },
    { "at+writefim",    app_at_cmd_sys_write_fim,   "Write FIM data" },
    { "at+dtim",        app_at_cmd_sys_dtim_time,   "Wifi DTIM" },
    { "at+calibration", app_at_cmd_sys_voltage_offset,     "Voltage Offset" },
    { "at+getvoltage",  app_at_cmd_sys_get_voltage,        "Get Voltage" },
    { "at+send",        app_at_cmd_sys_send,               "Send JSON Format" },
    { "at+checkid",     app_at_cmd_sys_check_id,           "Check ID Format" },
    { "at+devid",       app_at_cmd_sys_device_id,          "Query Device ID" },
    { "at+chipid",      app_at_cmd_sys_chip_id,            "Query Chip ID" },
    { "at+read_switch", app_at_cmd_sys_read_switch,        "Check Read Switch" },
    { "at+factory",     app_at_cmd_sys_factory,            "Enter Test Mode" },
    { "at+ver",         app_at_cmd_sys_version,            "Firmware Version" },
    { "at+wifi",        app_at_cmd_sys_wifi,               "Check strength of wifi" },
    { "at+gpio_read",   app_at_cmd_sys_gpio_read,          "Check Level of GPIO num" },
    { "at+delete_data", app_at_cmd_sys_delete_data,        "Delete product data" },
#if (WIFI_OTA_FUNCTION_EN == 1)
    { "at+ota",         app_at_cmd_sys_do_wifi_ota, "Do Wifi OTA" },
#endif
    { NULL,             NULL,                       NULL},
};

void app_at_cmd_add(void)
{
    at_cmd_ext_tbl_reg(g_taAppAtCmd);

    osTimerDef_t tTimerFactoryWifiConnDef;

    // create the timer
    tTimerFactoryWifiConnDef.ptimer = Wifi_Conn_TimerCallBack;
    g_tAppFactoryWifiConnTimerId = osTimerCreate(&tTimerFactoryWifiConnDef, osTimerOnce, NULL);
    if (g_tAppFactoryWifiConnTimerId == NULL)
    {
        printf("To create the timer is fail.\n");
    }
    return;
}
