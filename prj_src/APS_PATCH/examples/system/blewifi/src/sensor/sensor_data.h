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
/**
 * @file sensor_data.h
 * @author Michael Liao
 * @date 15 Mar 2018
 * @brief File containing declaration of sensor application & definition of structure for reference.
 *
 */
#ifndef __SENSOR_DATA_H__
#define __SENSOR_DATA_H__
#include <stdint.h>
#include <time.h> // This is for time_t

#define SENSOR_DATA_OK      0
#define SENSOR_DATA_FAIL    1

#define SENSOR_COUNT   (32)

typedef struct
{
    uint8_t ubaDoorStatus[SENSOR_COUNT];     /* Door Status */
    uint8_t ubaType[SENSOR_COUNT];           /* Type Status */
    time_t aTimeStamp[SENSOR_COUNT];         /* Time */
    uint32_t ulReadIdx;
    uint32_t ulWriteIdx;
} Sensor_Data_t;

uint8_t Sensor_Data_Push(uint8_t ubDoorStatus, uint8_t ubType, time_t TimeStamp);
uint8_t Sensor_Data_Pop(uint8_t *ubDoorStatus, uint8_t *ubType, time_t *TimeStamp);
uint8_t Sensor_Data_CheckEmpty(void);
void Sensor_Data_ResetBuffer(void);
uint8_t Sensor_Data_ReadIdxUpdate(void);

#endif // __SENSOR_DATA_H__

