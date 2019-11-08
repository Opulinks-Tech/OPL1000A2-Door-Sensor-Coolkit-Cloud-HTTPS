/******************************************************************************
*  Copyright 2017 - 2019, Opulinks Technology Ltd.
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2019
******************************************************************************/
#include "sensor_common.h"

/*************************************************************************
* FUNCTION:
*   Sensor_SHA256_Calc
*
* DESCRIPTION:
*   Calculate the value of SHA256 
*
* PARAMETERS
*   SHA256_Output  : [In] An empty array to store the value of SHA256
*   len : Input the size value of array 
* RETURNS
*   true: setting complete
*   false: error
*
*************************************************************************/
int Sensor_SHA256_Calc(uint8_t SHA256_Output[], int len, unsigned char uwSHA256_Buff[])
{       
    /*
        int nl_scrt_sha
        (
            uint8_t u8Type,         => SCRT_TYPE_SHA_256
            uint8_t u8Step,         => 0 (Data send once, so this value set 0.)
            uint32_t u32TotalLen,   => input data length
            uint8_t *u8aData,       => start address of input data
            uint32_t u32DataLen,    => same as u32TotalLen
            uint8_t u8HasInterMac,  => 0
            uint8_t *u8aMac         => output buffer: caller needs to prepare a 32-bytes buffer to save SHA-256 output
        )
    */
    if(!nl_scrt_sha(SCRT_TYPE_SHA_256, 0, len, uwSHA256_Buff, len, 0, SHA256_Output))
    {
        tracer_cli(LOG_HIGH_LEVEL, "nl_scrt_sha_1_step fail\n");
        return false;
    }
    
    return true;
}

