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

#ifndef __SENSOR_HTTPS_H__
#define __SENSOR_HTTPS_H__
#include <stdlib.h>
#include <string.h>
#include <time.h> // This is for time_t

#define BUFFER_SIZE 2048

#define BUFFER_SIZE_128 128

typedef struct {
  time_t TimeStamp;
  uint8_t DoorStatus;
  uint8_t ContentType;
  uint8_t ubaMacAddr[6];
  char Battery[8];
  int rssi;
  char FwVersion[24];
} HttpPostData_t;

extern unsigned char g_uwSHA256_Buff[BUFFER_SIZE];

int Sensor_Https_Post(unsigned char *post_data, int len);
int Sensor_Https_Post_On_Line(void);
void UpdateBatteryContent(void);

#endif  // end of __SENSOR_HTTPS_H__
