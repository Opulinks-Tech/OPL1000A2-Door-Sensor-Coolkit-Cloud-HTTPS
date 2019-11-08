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
#ifndef __SENSOR_BUTTON_PRESS_H__
#define __SENSOR_BUTTON_PRESS_H__

#include "cmsis_os.h"
#include "hal_vic.h"

extern osTimerId g_tAppButtonTimerId;
extern osTimerId g_tAppButtonLongPressTimerId;
extern osTimerId g_tAppButtonBleAdvTimerId;

extern uint32_t g_ulSoftTimerTickPre;  // store previous tick value ,
extern uint32_t g_ulSoftTimerTickNow;  //system tick value by osKernaltick fun.

void Sensor_ButtonPress_Init(void);

#endif // end of __SENSOR_BUTTON_PRESS_H__
