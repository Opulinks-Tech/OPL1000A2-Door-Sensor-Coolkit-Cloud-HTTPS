/******************************************************************************
*  Copyright 2018, Netlink Communication Corp.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Netlnik Communication Corp. (C) 2018
******************************************************************************/
#ifndef __SENSOR_COMMON_H__
#define __SENSOR_COMMON_H__

#include "scrt_patch.h"
#include "sensor_https.h"

//uint32_t Sensor_CurrentSystemTimeGet(void);
int Sensor_SHA256_Calc(uint8_t SHA256_Output[], int len, unsigned char uwSHA256_Buff[]);

#endif // end of __SENSOR_COMMON_H__

