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

#include <stdio.h>
#include <string.h>
#include "blewifi_common.h"
#include "sys_common_api.h"
#include "mw_fim_default_group01.h"
#include "lwip/errno.h"
#include "sys_common_api_patch.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/md5.h"
#include "cmsis_os.h"

#if (SNTP_FUNCTION_EN == 1)
#include "lwip/netdb.h"
#endif

uint32_t g_ulSystemSecondInit;    // System Clock Time Initialize
uint32_t g_ulSntpSecondInit;      // GMT Time Initialize

void BleWifi_HexDump(const char *title, const uint8_t *buf, size_t len)
{
	size_t i;

	printf("\n%s - hexdump(len=%lu):", title, (unsigned long)len);
	if (buf == NULL)
	{
		printf(" [NULL]");
	}
	else
	{
		for (i = 0; i < len; i++)
		{
            if (i%16 == 0)
                printf("\n");
			printf(" %02x", buf[i]);
		}
	}
	printf("\n");

}

uint32_t BleWifi_CurrentSystemTimeGet(void)
{
    uint32_t ulTick;
    int32_t  dwOverflow;
    uint32_t ulsecond;

    uint32_t ulSecPerTickOverflow;
    uint32_t ulSecModTickOverflow;
    uint32_t ulMsModTickOverflow;

    uint32_t ulSecPerTick;
    uint32_t ulSecModTick;

    osKernelSysTickEx(&ulTick, &dwOverflow);

    // the total time of TickOverflow is 4294967295 + 1 ms
    ulSecPerTickOverflow = (0xFFFFFFFF / osKernelSysTickFrequency) * dwOverflow;
    ulSecModTickOverflow = (((0xFFFFFFFF % osKernelSysTickFrequency) + 1) * dwOverflow) / osKernelSysTickFrequency;
    ulMsModTickOverflow = (((0xFFFFFFFF % osKernelSysTickFrequency) + 1) * dwOverflow) % osKernelSysTickFrequency;

    ulSecPerTick = (ulTick / osKernelSysTickFrequency);
    ulSecModTick = (ulTick % osKernelSysTickFrequency + ulMsModTickOverflow) / osKernelSysTickFrequency;

    ulsecond = (ulSecPerTickOverflow + ulSecModTickOverflow) + (ulSecPerTick + ulSecModTick);

    return ulsecond;
}

void BleWifi_SntpInit(void)
{
    g_ulSntpSecondInit = SNTP_SEC_2019;     // Initialize the Sntp Value
    g_ulSystemSecondInit = 0;               // Initialize System Clock Time
}

#if (SNTP_FUNCTION_EN == 1)
int BleWifi_SntpUpdate(void)
{
    int lRet = false;
    sntp_header_t sntp_h;

    struct sockaddr_in server;
    struct addrinfo *res = NULL;
    struct in_addr *addr = NULL;
    struct timeval receiving_timeout;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };

    int err;
    int sockfd = 0;
    int retry = 0;
    int sntp_port = 0;

    time_t rawtime;
    struct tm *pSntpTime = NULL;

    /* get sntp server ip */
    err = getaddrinfo(SNTP_SERVER, SNTP_PORT_NUM, &hints, &res);
    if (err != 0 || res == NULL)
    {
        goto fail;
    }

    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    printf("[SNTP] DNS lookup succeeded.\r\n[SNTP] Server IP=%s \r\n", inet_ntoa(*addr));

    memset((char*) &server, 0, sizeof(server));

    sscanf(SNTP_PORT_NUM, "%d", &sntp_port);
    server.sin_family = AF_INET;
    server.sin_port = htons(sntp_port);
    server.sin_addr.s_addr = addr->s_addr;

    // When sntp get data fail, then reconnect the connection.
    while (retry < 5)
    {
        sockfd = socket(AF_INET,SOCK_DGRAM,0);
        if (sockfd < 0) {
            printf("[SNTP] create socket failed.\n");
            goto CloseSocket;
        }

        if (connect(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0)
        {
            printf("[SNTP] connecting failed.\n");
            goto CloseSocket;
        }

        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0)
        {
            printf("[SNTP] failed to set socket receiving timeout\r\n");
            goto CloseSocket;
        }

        /* init sntp header */
        sntp_h.li_vn_mode = 0x1b; /* li = 0, no warning, vn = 3, ntp version, mode = 3, client */

        if (write(sockfd, (char*) &sntp_h, sizeof(sntp_h)) < 0)
        {
           printf("[SNTP] sendto failed.\n");
            goto CloseSocket;
        }
        else
        {
            //int nRet = read(sockfd, (char*) &sntp_h, sizeof(sntp_h));
            int nRet;
            int error;
            socklen_t errlen = sizeof(error);

            if (-1 == getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &errlen))
            {
                printf("getsockopt error return -1.");
            }

            if ((nRet = read(sockfd, (char*) &sntp_h, sizeof(sntp_h)) ) < 0)
            {
                printf("[SNTP] recvfrom failed ret is %d .\n",nRet);
                printf(" [SNTP] getsockopt return errinfo = %d", error);

                if (errno == EWOULDBLOCK)
                {
                    printf("sntp read timeout \n");
                }
                else
                {
                    printf("sntp recv error\n");
                }
               goto CloseSocket;
           }
           else
           {
               g_ulSystemSecondInit = BleWifi_CurrentSystemTimeGet();
               g_ulSntpSecondInit = ntohl(sntp_h.trantimeint);
               rawtime = SNTP_CONVERT_TIME(g_ulSntpSecondInit);
               pSntpTime = localtime(&rawtime);
               printf("Current time: %d-%d-%d %d:%d:%d\n", pSntpTime->tm_year + 1900, pSntpTime->tm_mon + 1, pSntpTime->tm_mday, pSntpTime->tm_hour, pSntpTime->tm_min, pSntpTime->tm_sec);
               lRet = true;
               if (sockfd >= 0)
                   close(sockfd);
               break;
           }
        }

CloseSocket:
        retry++;
        if (sockfd >= 0)
            close(sockfd);
    }

fail:
    if (res != NULL)
        freeaddrinfo(res);

    return lRet;
}
#endif

void BleWifi_SntpGet(struct tm *pSystemTime)
{
    time_t rawtime;
    struct tm * timeinfo;
    uint32_t ulTmpSystemSecond = 0;
    uint32_t ulDeltaSystemSecond = 0;

    ulTmpSystemSecond = BleWifi_CurrentSystemTimeGet();
    ulDeltaSystemSecond =  (ulTmpSystemSecond - g_ulSystemSecondInit);
    rawtime = SNTP_CONVERT_TIME(g_ulSntpSecondInit + ulDeltaSystemSecond);
    timeinfo = localtime(&rawtime);
    memcpy(pSystemTime, timeinfo, sizeof(struct tm));
    pSystemTime->tm_year = pSystemTime->tm_year + 1900;
    pSystemTime->tm_mon = pSystemTime->tm_mon + 1;
    // printf("Current time: %d-%d-%d %d:%d:%d\n", pSystemTime->tm_year, pSystemTime->tm_mon, pSystemTime->tm_mday, pSystemTime->tm_hour, pSystemTime->tm_min, pSystemTime->tm_sec);
}

time_t BleWifi_SntpGetRawData(void)
{
    time_t rawtime;
    uint32_t ulTmpSystemSecond = 0;
    uint32_t ulDeltaSystemSecond = 0;

    ulTmpSystemSecond = BleWifi_CurrentSystemTimeGet();
    ulDeltaSystemSecond =  (ulTmpSystemSecond - g_ulSystemSecondInit);
    rawtime = SNTP_CONVERT_TIME(g_ulSntpSecondInit + ulDeltaSystemSecond);

    return rawtime;
}

/*
Set RF power (0x00 - 0xFF)
*/
void BleWifi_RFPowerSetting(uint8_t level)
{
    T_RfCfg tCfg;

    // Get the settings of RF power
    if (MW_FIM_OK !=  MwFim_FileRead(MW_FIM_IDX_GP01_RF_CFG, 0, MW_FIM_RF_CFG_SIZE, (uint8_t *)&tCfg) )
    {
        // if fail, get the default settings of RF power
        memcpy(&tCfg, &g_tMwFimDefaultRfConfig, MW_FIM_RF_CFG_SIZE);
    }

    if (tCfg.u8HighPwrStatus != level)
    {
        tCfg.u8HighPwrStatus = level;
        if (MW_FIM_OK != MwFim_FileWrite(MW_FIM_IDX_GP01_RF_CFG, 0, MW_FIM_RF_CFG_SIZE, (uint8_t *)&tCfg))
        {
            printf("\r\nGroup 01 Write RF Power Value - ERROR\r\n");
        }
    }

    sys_set_wifi_lowpower_tx_vdd_rf(BLEWIFI_COM_RF_SMPS_SETTING);

#if 0
    int ret = 0;

    ret = sys_set_config_rf_power_level(level);
    printf("RF Power Settings is = %s \n", (ret == 1 ? "successful":"false"));
#endif
}

int BleWifi_CBC_encrypt(void *src , int len , unsigned char *iv , const unsigned char *key , void *out)
{
    int len1 = len & 0xfffffff0;
    int len2 = len1 + 16;
    int pad = len2 - len;
    uint32_t u32Keybits = 128;
    uint16_t i = 0;
    uint16_t u16BlockNum = 0;
    int ret = 0;
    void * pTempSrcPos = src;
    void * pTempOutPos = out;

    if((pTempSrcPos == NULL) || (pTempOutPos == NULL))
    {
        return -1;
    }
    mbedtls_aes_context aes_ctx = {0};

    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx , key , u32Keybits);

    if (len1) //execute encrypt for n-1 block
    {
        u16BlockNum = len >> 4 ;
        for (i = 0; i < u16BlockNum ; ++i)
        {
            ret = mbedtls_aes_crypt_cbc(&aes_ctx , MBEDTLS_AES_ENCRYPT , AES_BLOCK_SIZE, iv , (unsigned char *)pTempSrcPos , (unsigned char *)pTempOutPos);
            pTempSrcPos = ((char*)pTempSrcPos)+16;
            pTempOutPos = ((char*)pTempOutPos)+16;
        }
    }
    if (pad) //padding & execute encrypt for last block
    {
        char buf[16];
        memcpy((char *)buf, (char *)src + len1, len - len1);
        memset((char *)buf + len - len1, pad, pad);
        ret = mbedtls_aes_crypt_cbc(&aes_ctx , MBEDTLS_AES_ENCRYPT , AES_BLOCK_SIZE, iv , (unsigned char *)buf , (unsigned char *)out + len1);
    }
    mbedtls_aes_free(&aes_ctx);

    if(ret != 0)
        return -1;
    else
        return 0;
}

int BleWifi_CBC_decrypt(void *src, int len , unsigned char *iv , const unsigned char *key, void *out)
{
    mbedtls_aes_context aes_ctx = {0};
    int n = len >> 4;
    char *out_c = NULL;
    int offset = 0;
    int ret = 0;
    uint32_t u32Keybits = 128;
    uint16_t u16BlockNum = 0;
    char pad = 0;
    void * pTempSrcPos = src;
    void * pTempOutPos = out;
    uint16_t i = 0;

    if((pTempSrcPos == NULL) || (pTempOutPos == NULL))
    {
        return -1;
    }

    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx , key , u32Keybits);

    //decrypt n-1 block
    u16BlockNum = n - 1;
    if (n > 1)
    {
        for (i = 0; i < u16BlockNum ; ++i)
        {
            ret = mbedtls_aes_crypt_cbc(&aes_ctx , MBEDTLS_AES_DECRYPT , AES_BLOCK_SIZE, iv , (unsigned char *)pTempSrcPos , (unsigned char *)pTempOutPos);
            pTempSrcPos = ((char*)pTempSrcPos)+16;
            pTempOutPos = ((char*)pTempOutPos)+16;
        }

    }

    out_c = (char *)out;
    offset = n > 0 ? ((n - 1) << 4) : 0;
    out_c[offset] = 0;

    //decrypt last block
    ret = mbedtls_aes_crypt_cbc(&aes_ctx , MBEDTLS_AES_DECRYPT , AES_BLOCK_SIZE, iv , (unsigned char *)src + offset , (unsigned char *)out_c + offset);

    //paddind data set 0
    pad = out_c[len - 1];
    out_c[len - pad] = 0;

    mbedtls_aes_free(&aes_ctx);

    if(ret != 0)
        return -1;
    else
        return 0;
}

int BleWifi_UUID_Generate(unsigned char *ucUuid , uint16_t u16BufSize)
{
    uint8_t i = 0;
    uint8_t u8Random = 0;
    if(u16BufSize < 36)
    {
        return false;
    }
    srand(osKernelSysTick());
    for(i = 0; i<36 ; i++)
    {
        if((i == 8) || (i == 13) || (i == 18) || (i == 23))
        {
            ucUuid[i] = '-';
        }
        else
        {
            u8Random = rand()%16;
            if(u8Random < 10)
            {
                ucUuid[i] = u8Random + '0';
            }
            else
            {
                ucUuid[i] = (u8Random - 10) + 'a';
            }
        }
    }
    return true;
}

