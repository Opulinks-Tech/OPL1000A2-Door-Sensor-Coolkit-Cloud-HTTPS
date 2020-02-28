
/******************************************************************************
*  Copyright 2019 - 2019, Opulinks Technology Ltd.
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/
#include "cmsis_os.h"
#include "sensor_button.h"
#include "hal_vic.h"
#include "blewifi_ctrl.h"
#include "blewifi_ble_api.h"

osTimerId g_tAppButtonTimerId;
osTimerId g_tAppButtonLongPressTimerId;
osTimerId g_tAppButtonBleAdvTimerId;

uint32_t g_ulSoftTimerTickPre = 0;  // store previous tick value ,
uint32_t g_ulSoftTimerTickNow = 0;  //system tick value by osKernaltick fun.

/*************************************************************************
* FUNCTION:
*   Sensor_ButtonPress_GPIOCallBack
*
* DESCRIPTION:
*   GPIO call back function
*
* PARAMETERS
*   tGpioIdx  : [In] The GPIO pin
*
* RETURNS
*   none
*
*************************************************************************/
static void Sensor_ButtonPress_GPIOCallBack(E_GpioIdx_t tGpioIdx)
{
    // ISR level dont do printf
    Hal_Vic_GpioIntEn(BUTTON_IO_PORT, 0);
    // send the result to the task of blewifi control.
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BUTTON_STATECHANGE, NULL, 0);
}

/*************************************************************************
* FUNCTION:
*   Sensor_ButtonPress_TimerCallBack
*
* DESCRIPTION:
*   Timer call back function
*
* PARAMETERS
*   argu  : [In] argument to the timer call back function.
*
* RETURNS
*   none
*
*************************************************************************/
static void Sensor_ButtonPress_TimerCallBack(void const *argu)
{
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BUTTON_DEBOUNCETIMEOUT, NULL, 0);
}

static void BleWifi_ButtonPress_LongPressTimerCallBack(void const *argu)
{
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BUTTON_LONG_PRESS_TIMEOUT, NULL, 0);
}

static void Sensor_ButtonPress_BleAdvTimerCallBack(void const *argu)
{
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BUTTON_BLEADVTIMEOUT, NULL, 0);
}

void Sensor_ButtonPress_BleAdvInit(void)
{
    osTimerDef_t tTimerButtonDef;

    // create the timer
    tTimerButtonDef.ptimer = Sensor_ButtonPress_BleAdvTimerCallBack;
    g_tAppButtonBleAdvTimerId = osTimerCreate(&tTimerButtonDef, osTimerOnce, NULL);
    if (g_tAppButtonBleAdvTimerId == NULL)
    {
        printf("To create the timer for AppTimer is fail.\n");
    }

    tTimerButtonDef.ptimer = BleWifi_ButtonPress_LongPressTimerCallBack;
    g_tAppButtonLongPressTimerId = osTimerCreate(&tTimerButtonDef, osTimerOnce, NULL);
    if (g_tAppButtonLongPressTimerId == NULL)
    {
        printf("To create the long-press timer for AppTimer is fail.\n");
    }
}

/*************************************************************************
* FUNCTION:
*   Sensor_ButtonPress_TimerInit
*
* DESCRIPTION:
*   Timer initializaion setting
*
* PARAMETERS
*   none
*
* RETURNS
*   none
*
*************************************************************************/
void Sensor_ButtonPress_TimerInit(void)
{
    osTimerDef_t tTimerButtonDef;

    // create the timer
    tTimerButtonDef.ptimer = Sensor_ButtonPress_TimerCallBack;
    g_tAppButtonTimerId = osTimerCreate(&tTimerButtonDef, osTimerOnce, NULL);
    if (g_tAppButtonTimerId == NULL)
    {
        printf("To create the timer for AppTimer is fail.\n");
    }
}

/*************************************************************************
* FUNCTION:
*   Sensor_ButtonPress_GPIOInit
*
* DESCRIPTION:
*   Initialization the function of GPIO
*
* PARAMETERS
*   epinIdx  : [In] The GPIO pin
*
* RETURNS
*   none
*
*************************************************************************/
void Sensor_ButtonPress_GPIOInit(E_GpioIdx_t epinIdx)
{
    unsigned int u32PinLevel = 0;

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(BUTTON_IO_PORT);

    if(GPIO_LEVEL_LOW == u32PinLevel)
    {
        Hal_Vic_GpioCallBackFuncSet(epinIdx, Sensor_ButtonPress_GPIOCallBack);
        Hal_Vic_GpioDirection(epinIdx, GPIO_INPUT);
        Hal_Vic_GpioIntTypeSel(epinIdx, INT_TYPE_LEVEL);
        Hal_Vic_GpioIntInv(epinIdx, 0);
        Hal_Vic_GpioIntMask(epinIdx, 0);
        Hal_Vic_GpioIntEn(epinIdx, 1);
    }
    else
    {
        Hal_Vic_GpioCallBackFuncSet(epinIdx, Sensor_ButtonPress_GPIOCallBack);
        Hal_Vic_GpioDirection(epinIdx, GPIO_INPUT);
        Hal_Vic_GpioIntTypeSel(epinIdx, INT_TYPE_LEVEL);
        Hal_Vic_GpioIntInv(epinIdx, 1);
        Hal_Vic_GpioIntMask(epinIdx, 0);
        Hal_Vic_GpioIntEn(epinIdx, 1);
    }
}

void Sensor_ButtonPress_Init(void)
{
    /* Init Buttton Action */
    Sensor_ButtonPress_GPIOInit(BUTTON_IO_PORT);
    Sensor_ButtonPress_TimerInit();
    Sensor_ButtonPress_BleAdvInit();
}
