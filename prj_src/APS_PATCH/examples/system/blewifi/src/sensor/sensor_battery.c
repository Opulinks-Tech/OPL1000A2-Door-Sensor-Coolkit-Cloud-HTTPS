/******************************************************************************
*  Copyright 2019, Netlink Communication Corp.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Netlnik Communication Corp. (C) 2019
******************************************************************************/

#include "blewifi_wifi_api.h"
#include "blewifi_configuration.h"
#include "boot_sequence.h"
#include "hal_auxadc.h"
#include "hal_vic.h"
#include "sensor_battery.h"
#include "mw_fim_default_group12_project.h"
#include "mw_fim_default_group16_project.h"
#include "hal_pin.h"
#include "hal_pin_def.h"
#include "cmsis_os.h"

extern uint32_t g_ulHalAux_AverageCount;

Sensor_Battery_t BatteryVoltage;

#define SENSOR_AUXADC_IO_VOLTAGE_GET_AVERAGE_COUNT    (30)

float g_fVoltageOffset = 0;

float Sensor_Auxadc_VBat_Convert_to_Percentage(void)
{
    float fVBat;
    float fAverageVBat;

    Hal_Aux_IoVoltageGet(BATTERY_IO_PORT, &fVBat);

    if (fVBat > MAXIMUM_VOLTAGE_DEF)
        fVBat = MAXIMUM_VOLTAGE_DEF;
    if (fVBat < MINIMUM_VOLTAGE_DEF)
        fVBat = MINIMUM_VOLTAGE_DEF;

    // error handle: if the average count = 0
    if (SENSOR_MOVING_AVERAGE_COUNT == 0)
    {
        fAverageVBat = fVBat;
    }
    // compute the moving average value
    else
    {
        // update the new average count
        if (BatteryVoltage.ulSensorVbatMovingAverageCount < SENSOR_MOVING_AVERAGE_COUNT)
            BatteryVoltage.ulSensorVbatMovingAverageCount++;

        // compute the new moving average value
        fAverageVBat = BatteryVoltage.fSensorVbatCurrentValue * (BatteryVoltage.ulSensorVbatMovingAverageCount - 1);
        fAverageVBat = (fAverageVBat + fVBat) / BatteryVoltage.ulSensorVbatMovingAverageCount;
    }

    // the value is updated when the new value is the lower one.
    if (fAverageVBat < BatteryVoltage.fSensorVbatCurrentValue)
        BatteryVoltage.fSensorVbatCurrentValue = fAverageVBat;

    // Return
    return ((BatteryVoltage.fSensorVbatCurrentValue - MINIMUM_VOLTAGE_DEF)/(MAXIMUM_VOLTAGE_DEF - MINIMUM_VOLTAGE_DEF))*100;
}

float Sensor_Auxadc_VBat_Get(void)
{
    float fVBat;
    float fAverageVBat;

    g_ulHalAux_AverageCount = SENSOR_AUXADC_IO_VOLTAGE_GET_AVERAGE_COUNT;
    Hal_Pin_ConfigSet(BATTERY_IO_PORT, PIN_TYPE_GPIO_OUTPUT_HIGH, PIN_DRIVING_FLOAT);
    osDelay(3);
    Hal_Aux_IoVoltageGet(BATTERY_IO_PORT, &fVBat);
    Hal_Pin_ConfigSet(BATTERY_IO_PORT, PIN_TYPE_NONE, PIN_DRIVING_FLOAT);

    fVBat -= g_fVoltageOffset;
/*
    if (fVBat > MAXIMUM_VOLTAGE_DEF)
        fVBat = MAXIMUM_VOLTAGE_DEF;
    if (fVBat < MINIMUM_VOLTAGE_DEF)
        fVBat = MINIMUM_VOLTAGE_DEF;
*/
#if 0
    // error handle: if the average count = 0
    if (SENSOR_MOVING_AVERAGE_COUNT == 0)
    {
        fAverageVBat = fVBat;
    }
    // compute the moving average value
    else
    {
        // update the new average count
        if (BatteryVoltage.ulSensorVbatMovingAverageCount < SENSOR_MOVING_AVERAGE_COUNT)
            BatteryVoltage.ulSensorVbatMovingAverageCount++;

        // compute the new moving average value
        fAverageVBat = BatteryVoltage.fSensorVbatCurrentValue * (BatteryVoltage.ulSensorVbatMovingAverageCount - 1);
        fAverageVBat = (fAverageVBat + fVBat) / BatteryVoltage.ulSensorVbatMovingAverageCount;
    }
#else
    fAverageVBat = fVBat;
#endif

    // the value is updated when the new value is the lower one.
    //if (fAverageVBat < BatteryVoltage.fSensorVbatCurrentValue)
        BatteryVoltage.fSensorVbatCurrentValue = fAverageVBat;

    // Return
    return (BatteryVoltage.fSensorVbatCurrentValue);
}

void Sensor_Auxadc_Init(void)
{
    // cold boot
    if (0 == Boot_CheckWarmBoot())
    {
        BatteryVoltage.ulSensorVbatMovingAverageCount = 0;
        BatteryVoltage.fSensorVbatCurrentValue = MAXIMUM_VOLTAGE_DEF;
    }

    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP16_PROJECT_VOLTAGE_OFFSET, 0, MW_FIM_GP16_VOLTAGE_OFFSET_SIZE, (uint8_t *)&g_fVoltageOffset))
    {
        // if fail, return fail
        printf("Read FIM fail \r\n");
        g_fVoltageOffset = VOLTAGE_OFFSET;
    }

    // Calibration data
    g_ulHalAux_AverageCount = SENSOR_AUXADC_IO_VOLTAGE_GET_AVERAGE_COUNT;
}

