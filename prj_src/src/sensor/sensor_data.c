/******************************************************************************
*  Copyright 2019, Netlink Communication Corp.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Netlnik Communication Corp. (C) 2018
******************************************************************************/
#include <string.h>
#include "cmsis_os.h"
#include "sensor_data.h"

#include "blewifi_common.h"

Sensor_Data_t g_tSensorData;
extern osTimerId    g_tAppCtrlType1_2_3_HttpPostRetryTimer;
extern int g_nDoType1_2_3_Retry_Flag;
extern int g_nType1_2_3_Retry_counter;

uint8_t Sensor_Data_Push(uint8_t ubDoorStatus, uint8_t ubType, time_t TimeStamp)
{

    uint32_t ulWriteNext;

    // full, ulWriteIdx + 1 == ulReadIdx
    ulWriteNext = (g_tSensorData.ulWriteIdx + 1) % SENSOR_COUNT;
    
    // Read index alway prior to write index
    if (ulWriteNext == g_tSensorData.ulReadIdx)
    {
        // discard the oldest data, and read index move forware one step.
        g_tSensorData.ulReadIdx = (g_tSensorData.ulReadIdx + 1) % SENSOR_COUNT;
    }

    // update the temperature data to write index
    g_tSensorData.ubaDoorStatus[g_tSensorData.ulWriteIdx] = ubDoorStatus;
    g_tSensorData.ubaType[g_tSensorData.ulWriteIdx] = ubType;
    g_tSensorData.aTimeStamp[g_tSensorData.ulWriteIdx] = TimeStamp;
    g_tSensorData.ulWriteIdx = ulWriteNext;

    //if new data comes during the type1/2/3 data re-send duration, will skip this re-transmission
    osTimerStop(g_tAppCtrlType1_2_3_HttpPostRetryTimer); 
    if(g_nDoType1_2_3_Retry_Flag == 1)
    {
        g_nDoType1_2_3_Retry_Flag = 0;
    }
    else
    {
        g_nType1_2_3_Retry_counter = 0;
    }

    return SENSOR_DATA_OK;
}

uint8_t Sensor_Data_Pop(uint8_t *ubDoorStatus, uint8_t *ubType, time_t *TimeStamp)
{
    uint8_t bRet = SENSOR_DATA_FAIL;
    extern uint8_t g_nLastPostDatatType;

    // empty, ulWriteIdx == ulReadIdx
    if (g_tSensorData.ulWriteIdx == g_tSensorData.ulReadIdx)
        goto done;

    *ubDoorStatus = g_tSensorData.ubaDoorStatus[g_tSensorData.ulReadIdx];
    *ubType = g_tSensorData.ubaType[g_tSensorData.ulReadIdx];
    *TimeStamp = g_tSensorData.aTimeStamp[g_tSensorData.ulReadIdx];
    g_nLastPostDatatType = *ubType;
    
    bRet = SENSOR_DATA_OK;

done:
    printf("POST Data W#%d R#%d --> D:%d T:%d T:%d\n",g_tSensorData.ulWriteIdx ,g_tSensorData.ulReadIdx, *ubDoorStatus, *ubType, *TimeStamp);
    return bRet;
}

uint8_t Sensor_Data_CheckEmpty(void)
{
    // empty, ulWriteIdx == ulReadIdx
    if (g_tSensorData.ulWriteIdx == g_tSensorData.ulReadIdx)
        return SENSOR_DATA_OK;
    
    return SENSOR_DATA_FAIL;
}

void Sensor_Data_ResetBuffer(void)
{
    g_tSensorData.ulReadIdx = 0;
    g_tSensorData.ulWriteIdx = 0;    
}

uint8_t Sensor_Data_ReadIdxUpdate(void)
{
    g_tSensorData.ulReadIdx = (g_tSensorData.ulReadIdx + 1) % SENSOR_COUNT;

    return SENSOR_DATA_OK;
}

