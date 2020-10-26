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

/**
 * @file blewifi_data.c
 * @author Michael Liao
 * @date 20 Feb 2018
 * @brief File creates the wifible_data task architecture.
 *
 */

#include "sys_os_config.h"
#include "blewifi_app.h"
#include "blewifi_data.h"
#include "blewifi_ctrl.h"
#include "blewifi_server_app.h"
#include "blewifi_wifi_api.h"
#include "blewifi_ble_api.h"
#include "wifi_api.h"
#include "lwip/netif.h"
#include "mw_ota.h"
#include "blewifi_ctrl_http_ota.h"
#include "hal_auxadc_patch.h"
#include "hal_system.h"
#include "mw_fim_default_group03.h"
#include "mw_fim_default_group03_patch.h"
#include "at_cmd_common.h"

#include "mw_fim_default_group12_project.h"
#include "sensor_data.h"
#include "iot_data.h"
#include "sys_common_api.h"
#include "app_at_cmd.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/md5.h"

#define HI_UINT16(a) (((a) >> 8) & 0xFF)
#define LO_UINT16(a) ((a) & 0xFF)

uint8_t g_wifi_disconnectedDoneForAppDoWIFIScan = 1;   //Goter

extern T_MwFim_GP12_HttpPostContent g_tHttpPostContent;
extern T_MwFim_GP12_HttpHostInfo g_tHostInfo;
extern uint8_t g_ubAppCtrlRequestRetryTimes;
extern wifi_config_t wifi_config_req_connect;
extern uint8_t g_u8ConnectTimeOutFlag;
extern osTimerId g_tAppWifiConnectTimeOutTimerId;
extern uint8_t g_bManuallyConnScanFlag;
extern uint8_t g_u8ManuallyConnScanCnt;

typedef struct {
    uint16_t total_len;
    uint16_t remain;
    uint16_t offset;
    uint8_t *aggr_buf;
} blewifi_rx_packet_t;

blewifi_rx_packet_t g_rx_packet = {0};
unsigned char g_ucSecretKey[SECRETKEY_LEN + 1] = {0};
unsigned char g_ucAppCode[UUID_SIZE + 1] = {0};
unsigned char g_ucDeviceCode[UUID_SIZE + 1] = {0};

static void BleWifi_Ble_ProtocolHandler_Auth(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_AuthToken(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Scan(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Connect(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Disconnect(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Reconnect(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_ReadDeviceInfo(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_WriteDeviceInfo(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_WifiStatus(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Reset(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Manually_Connect_AP(uint16_t type, uint8_t *data, int len);

#if (BLE_OTA_FUNCTION_EN == 1)
static void BleWifi_Ble_ProtocolHandler_OtaVersion(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_OtaUpgrade(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_OtaRaw(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_OtaEnd(uint16_t type, uint8_t *data, int len);
#endif

#if (WIFI_OTA_FUNCTION_EN == 1)
static void BleWifi_Ble_ProtocolHandler_HttpOtaTrig(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_HttpOtaDeviceVersion(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_HttpOtaServerVersion(uint16_t type, uint8_t *data, int len);
#endif

static void BleWifi_Ble_ProtocolHandler_MpCalVbat(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpCalIoVoltage(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpCalTmpr(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpSysModeWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpSysModeRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngSysReset(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngWifiMacWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngWifiMacRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleMacWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleMacRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleCmd(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_AppDeviceInfo(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_AppWifiConnection(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleCloudInfoWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleCloudInfoRead(uint16_t type, uint8_t *data, int len);

static T_BleWifi_Ble_ProtocolHandlerTbl g_tBleProtocolHandlerTbl[] =
{

    {BLEWIFI_REQ_AUTH,                      BleWifi_Ble_ProtocolHandler_Auth},
    {BLEWIFI_REQ_AUTH_TOKEN,                BleWifi_Ble_ProtocolHandler_AuthToken},
    {BLEWIFI_REQ_SCAN,                      BleWifi_Ble_ProtocolHandler_Scan},
    {BLEWIFI_REQ_CONNECT,                   BleWifi_Ble_ProtocolHandler_Connect},
    {BLEWIFI_REQ_APP_DEVICE_INFO,           BleWifi_Ble_ProtocolHandler_AppDeviceInfo},
    {BLEWIFI_REQ_APP_HOST_INFO,             BleWifi_Ble_ProtocolHandler_AppWifiConnection},
    {BLEWIFI_REQ_MANUAL_CONNECT_AP,         BleWifi_Ble_ProtocolHandler_Manually_Connect_AP},

    //{BLEWIFI_REQ_SCAN,                    BleWifi_Ble_ProtocolHandler_Scan},
    //{BLEWIFI_REQ_CONNECT,                 BleWifi_Ble_ProtocolHandler_Connect},
    {BLEWIFI_REQ_DISCONNECT,                BleWifi_Ble_ProtocolHandler_Disconnect},
    {BLEWIFI_REQ_RECONNECT,                 BleWifi_Ble_ProtocolHandler_Reconnect},
    {BLEWIFI_REQ_READ_DEVICE_INFO,          BleWifi_Ble_ProtocolHandler_ReadDeviceInfo},
    {BLEWIFI_REQ_WRITE_DEVICE_INFO,         BleWifi_Ble_ProtocolHandler_WriteDeviceInfo},
    {BLEWIFI_REQ_WIFI_STATUS,               BleWifi_Ble_ProtocolHandler_WifiStatus},
    {BLEWIFI_REQ_RESET,                     BleWifi_Ble_ProtocolHandler_Reset},
    //{BLEWIFI_REQ_MANUAL_CONNECT_AP,       BleWifi_Ble_ProtocolHandler_Manually_Connect_AP},

#if (BLE_OTA_FUNCTION_EN == 1)
    {BLEWIFI_REQ_OTA_VERSION,               BleWifi_Ble_ProtocolHandler_OtaVersion},
    {BLEWIFI_REQ_OTA_UPGRADE,               BleWifi_Ble_ProtocolHandler_OtaUpgrade},
    {BLEWIFI_REQ_OTA_RAW,                   BleWifi_Ble_ProtocolHandler_OtaRaw},
    {BLEWIFI_REQ_OTA_END,                   BleWifi_Ble_ProtocolHandler_OtaEnd},
#endif

#if (WIFI_OTA_FUNCTION_EN == 1)
    {BLEWIFI_REQ_HTTP_OTA_TRIG,             BleWifi_Ble_ProtocolHandler_HttpOtaTrig},
    {BLEWIFI_REQ_HTTP_OTA_DEVICE_VERSION,   BleWifi_Ble_ProtocolHandler_HttpOtaDeviceVersion},
    {BLEWIFI_REQ_HTTP_OTA_SERVER_VERSION,   BleWifi_Ble_ProtocolHandler_HttpOtaServerVersion},
#endif

    {BLEWIFI_REQ_MP_CAL_VBAT,               BleWifi_Ble_ProtocolHandler_MpCalVbat},
    {BLEWIFI_REQ_MP_CAL_IO_VOLTAGE,         BleWifi_Ble_ProtocolHandler_MpCalIoVoltage},
    {BLEWIFI_REQ_MP_CAL_TMPR,               BleWifi_Ble_ProtocolHandler_MpCalTmpr},
    {BLEWIFI_REQ_MP_SYS_MODE_WRITE,         BleWifi_Ble_ProtocolHandler_MpSysModeWrite},
    {BLEWIFI_REQ_MP_SYS_MODE_READ,          BleWifi_Ble_ProtocolHandler_MpSysModeRead},

    {BLEWIFI_REQ_ENG_SYS_RESET,             BleWifi_Ble_ProtocolHandler_EngSysReset},
    {BLEWIFI_REQ_ENG_WIFI_MAC_WRITE,        BleWifi_Ble_ProtocolHandler_EngWifiMacWrite},
    {BLEWIFI_REQ_ENG_WIFI_MAC_READ,         BleWifi_Ble_ProtocolHandler_EngWifiMacRead},
    {BLEWIFI_REQ_ENG_BLE_MAC_WRITE,         BleWifi_Ble_ProtocolHandler_EngBleMacWrite},
    {BLEWIFI_REQ_ENG_BLE_MAC_READ,          BleWifi_Ble_ProtocolHandler_EngBleMacRead},
    {BLEWIFI_REQ_ENG_BLE_CMD,               BleWifi_Ble_ProtocolHandler_EngBleCmd},
    {BLEWIFI_REQ_ENG_BLE_CLOUD_INFO_WRITE,  BleWifi_Ble_ProtocolHandler_EngBleCloudInfoWrite},
    {BLEWIFI_REQ_ENG_BLE_CLOUD_INFO_READ,   BleWifi_Ble_ProtocolHandler_EngBleCloudInfoRead},

    //{BLEWIFI_REQ_APP_DEVICE_INFO,         BleWifi_Ble_ProtocolHandler_AppDeviceInfo},
    //{BLEWIFI_REQ_APP_HOST_INFO,           BleWifi_Ble_ProtocolHandler_AppWifiConnection},
    {0xFFFFFFFF,                            NULL}
};

static void BleWifi_Wifi_Scan_Check(wifi_scan_config_t *pstScan_config)
{
    g_wifi_disconnectedDoneForAppDoWIFIScan = 1;

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_SCAN \r\n");
    /*++++++++++++++++++++++ Goter ++++++++++++++++++++++++++++*/
    if(true == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_WIFI_CONNECTING))
    {
        printf(" Postpone Do WIFI Scan Due to Auto Connect running IND \n");
        g_wifi_disconnectedDoneForAppDoWIFIScan = 0;
        int count = 0;
        while(count < 100)
        {
            if(g_wifi_disconnectedDoneForAppDoWIFIScan == 1)
            {
                break;
            }
            else
            {
                count++;
                osDelay(10);
            }
        }// wait event back
        printf(" Postpone Do WIFI Scan Due to Auto Connect running END \n");
    }
    /*---------------------- Goter -----------------------------*/

    if(g_wifi_disconnectedDoneForAppDoWIFIScan == 0)
    {
        return ;
    }

    if(false == BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_WIFI_SCANNING))
    {
        BleWifi_Wifi_DoScan(pstScan_config);
    }

    g_ubAppCtrlRequestRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
}

#if (BLE_OTA_FUNCTION_EN == 1)
static void BleWifi_OtaSendVersionRsp(uint8_t status, uint16_t pid, uint16_t cid, uint16_t fid)
{
	uint8_t data[7];
	uint8_t *p = (uint8_t *)data;

	*p++ = status;
	*p++ = LO_UINT16(pid);
	*p++ = HI_UINT16(pid);
	*p++ = LO_UINT16(cid);
	*p++ = HI_UINT16(cid);
	*p++ = LO_UINT16(fid);
	*p++ = HI_UINT16(fid);

	BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_VERSION, data, 7);
}

static void BleWifi_OtaSendUpgradeRsp(uint8_t status)
{
	BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_UPGRADE, &status, 1);
}

static void BleWifi_OtaSendEndRsp(uint8_t status, uint8_t stop)
{
	BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_END, &status, 1);

    if (stop)
    {
        if (gTheOta)
        {
            if (status != BLEWIFI_OTA_SUCCESS)
                MwOta_DataGiveUp();
            free(gTheOta);
            gTheOta = 0;

            if (status != BLEWIFI_OTA_SUCCESS)
                BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_OTHER_OTA_OFF_FAIL, NULL, 0);
            else
                BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_OTHER_OTA_OFF, NULL, 0);
        }
    }
}

static void BleWifi_HandleOtaVersionReq(uint8_t *data, int len)
{
	uint16_t pid;
	uint16_t cid;
	uint16_t fid;
	uint8_t state = MwOta_VersionGet(&pid, &cid, &fid);

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_VERSION\r\n");

	if (state != MW_OTA_OK)
		BleWifi_OtaSendVersionRsp(BLEWIFI_OTA_ERR_HW_FAILURE, 0, 0, 0);
	else
		BleWifi_OtaSendVersionRsp(BLEWIFI_OTA_SUCCESS, pid, cid, fid);
}

static uint8_t BleWifi_MwOtaPrepare(uint16_t uwProjectId, uint16_t uwChipId, uint16_t uwFirmwareId, uint32_t ulImageSize, uint32_t ulImageSum)
{
	uint8_t state = MW_OTA_OK;

	state = MwOta_Prepare(uwProjectId, uwChipId, uwFirmwareId, ulImageSize, ulImageSum);
	return state;
}

static uint8_t BleWifi_MwOtaDatain(uint8_t *pubAddr, uint32_t ulSize)
{
	uint8_t state = MW_OTA_OK;

	state = MwOta_DataIn(pubAddr, ulSize);
	return state;
}

static uint8_t BleWifi_MwOtaDataFinish(void)
{
	uint8_t state = MW_OTA_OK;

	state = MwOta_DataFinish();
	return state;
}

static void BleWifi_HandleOtaUpgradeReq(uint8_t *data, int len)
{
	blewifi_ota_t *ota = gTheOta;
	uint8_t state = MW_OTA_OK;

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_UPGRADE\r\n");

	if (len != 26)
	{
		BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_ERR_INVALID_LEN);
		return;
	}

	if (ota)
	{
		BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_ERR_IN_PROGRESS);
		return;
	}

	ota = malloc(sizeof(blewifi_ota_t));

	if (ota)
	{
		T_MwOtaFlashHeader *ota_hdr= (T_MwOtaFlashHeader*) &data[2];

		ota->pkt_idx = 0;
		ota->idx     = 0;
        ota->rx_pkt  = *(uint16_t *)&data[0];
        ota->proj_id = ota_hdr->uwProjectId;
        ota->chip_id = ota_hdr->uwChipId;
        ota->fw_id   = ota_hdr->uwFirmwareId;
        ota->total   = ota_hdr->ulImageSize;
        ota->chksum  = ota_hdr->ulImageSum;
		ota->curr 	 = 0;

		state = BleWifi_MwOtaPrepare(ota->proj_id, ota->chip_id, ota->fw_id, ota->total, ota->chksum);

        if (state == MW_OTA_OK)
        {
	        BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_SUCCESS);
	        gTheOta = ota;

	        BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_OTHER_OTA_ON, NULL, 0);
        }
        else
            BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_HW_FAILURE, TRUE);
    }
	else
	{
		BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_ERR_MEM_CAPACITY_EXCEED);
	}
}

static uint32_t BleWifi_OtaAdd(uint8_t *data, int len)
{
	uint16_t i;
	uint32_t sum = 0;

	for (i = 0; i < len; i++)
    {
		sum += data[i];
    }

    return sum;
}

static void BleWifi_HandleOtaRawReq(uint8_t *data, int len)
{
	blewifi_ota_t *ota = gTheOta;
	uint8_t state = MW_OTA_OK;

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_RAW\r\n");

	if (!ota)
	{
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_NOT_ACTIVE, FALSE);
        goto err;
	}

	if ((ota->curr + len) > ota->total)
	{
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_INVALID_LEN, TRUE);
		goto err;
    }

	ota->pkt_idx++;
	ota->curr += len;
	ota->curr_chksum += BleWifi_OtaAdd(data, len);

	if ((ota->idx + len) >= 256)
	{
		UINT16 total = ota->idx + len;
		UINT8 *s = data;
		UINT8 *e = data + len;
		UINT16 cpyLen = 256 - ota->idx;

		if (ota->idx)
		{
			MemCopy(&ota->buf[ota->idx], s, cpyLen);
			s += cpyLen;
			total -= 256;
			ota->idx = 0;

			state = BleWifi_MwOtaDatain(ota->buf, 256);
		}

		if (state == MW_OTA_OK)
		{
			while (total >= 256)
			{
				state = BleWifi_MwOtaDatain(s, 256);
				s += 256;
				total -= 256;

				if (state != MW_OTA_OK) break;
			}

			if (state == MW_OTA_OK)
			{
				MemCopy(ota->buf, s, e - s);
				ota->idx = e - s;

				if ((ota->curr == ota->total) && ota->idx)
				{
					state = BleWifi_MwOtaDatain(ota->buf, ota->idx);
				}
			}
		}
	}
	else
	{
		MemCopy(&ota->buf[ota->idx], data, len);
		ota->idx += len;

		if ((ota->curr == ota->total) && ota->idx)
		{
			state = BleWifi_MwOtaDatain(ota->buf, ota->idx);
		}
	}

	if (state == MW_OTA_OK)
	{
		if (ota->rx_pkt && (ota->pkt_idx >= ota->rx_pkt))
		{
	        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_RAW, 0, 0);
	    		ota->pkt_idx = 0;
    }
  }
    else
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_HW_FAILURE, TRUE);

err:
	return;
}

static void BleWifi_HandleOtaEndReq(uint8_t *data, int len)
{
	blewifi_ota_t *ota = gTheOta;
	uint8_t status = data[0];

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_END\r\n");

	if (!ota)
	{
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_NOT_ACTIVE, FALSE);
        goto err;
    }

		if (status == BLEWIFI_OTA_SUCCESS)
		{
		if (ota->curr == ota->total)
				{
					if (BleWifi_MwOtaDataFinish() == MW_OTA_OK)
						BleWifi_OtaSendEndRsp(BLEWIFI_OTA_SUCCESS, TRUE);
                    else
						BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_CHECKSUM, TRUE);
	            }
	            else
				{
					BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_INVALID_LEN, TRUE);
	            }
	        }
			else
			{
		if (ota) MwOta_DataGiveUp();

			// APP stop OTA
			BleWifi_OtaSendEndRsp(BLEWIFI_OTA_SUCCESS, TRUE);
		}

err:
	return;
}
#endif /* #if (BLE_OTA_FUNCTION_EN == 1) */

#if (WIFI_OTA_FUNCTION_EN == 1)
void BleWifi_Wifi_OtaTrigReq(uint8_t *data)
{
    // data length = string length + 1 (\n)
    blewifi_ctrl_http_ota_msg_send(BLEWIFI_CTRL_HTTP_OTA_MSG_TRIG, data, strlen((char*)data) + 1);
}

void BleWifi_Wifi_OtaTrigRsp(uint8_t status)
{
    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_HTTP_OTA_TRIG, &status, 1);
}

void BleWifi_Wifi_OtaDeviceVersionReq(void)
{
    blewifi_ctrl_http_ota_msg_send(BLEWIFI_CTRL_HTTP_OTA_MSG_DEVICE_VERSION, NULL, 0);
}

void BleWifi_Wifi_OtaDeviceVersionRsp(uint16_t fid)
{
    uint8_t data[2];
    uint8_t *p = (uint8_t *)data;

    *p++ = LO_UINT16(fid);
    *p++ = HI_UINT16(fid);

    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_HTTP_OTA_DEVICE_VERSION, data, 2);
}

void BleWifi_Wifi_OtaServerVersionReq(void)
{
    blewifi_ctrl_http_ota_msg_send(BLEWIFI_CTRL_HTTP_OTA_MSG_SERVER_VERSION, NULL, 0);
}

void BleWifi_Wifi_OtaServerVersionRsp(uint16_t fid)
{
    uint8_t data[2];
    uint8_t *p = (uint8_t *)data;

    *p++ = LO_UINT16(fid);
    *p++ = HI_UINT16(fid);

    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_HTTP_OTA_SERVER_VERSION, data, 2);
}
#endif /* #if (WIFI_OTA_FUNCTION_EN == 1) */

static void BleWifi_AppDeviceInfoRsp()
{
    int len = 0;
    uint8_t *data;
    uint8_t DeviceIDLength = 0;
    uint8_t ApiKeyLength = 0;
    uint8_t ChipIDLength = 0;
    uint8_t TotalSize = 0;
    unsigned char ucCbcEncData[128];
    size_t uCbcEncLen = 0;
    unsigned char ucBaseData[128];
    size_t uBaseLen = 0;
    unsigned char iv[17]={0};


    DeviceIDLength =  strlen(g_tHttpPostContent.ubaDeviceId);
    ApiKeyLength =  MAX_RSP_BASE64_API_KEY_LEN;
    ChipIDLength =  MAX_RSP_BASE64_CHIP_ID_LEN;

    TotalSize = DeviceIDLength + ApiKeyLength + ChipIDLength + 3;

    data = (uint8_t*) malloc((TotalSize + 1) * sizeof(uint8_t));

    memcpy(&data[len], &DeviceIDLength, 1);
    len = len + 1;
    memcpy(&data[len],  g_tHttpPostContent.ubaDeviceId, DeviceIDLength);
    len = len + DeviceIDLength;

    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    uCbcEncLen = strlen(g_tHttpPostContent.ubaApiKey);
    uCbcEncLen = (((uCbcEncLen  >> 4) + 1) << 4);
    BleWifi_CBC_encrypt((void *)g_tHttpPostContent.ubaApiKey , strlen(g_tHttpPostContent.ubaApiKey) , iv , g_ucSecretKey , (void *)ucCbcEncData);
    mbedtls_base64_encode((unsigned char *)ucBaseData , 128  ,&uBaseLen ,(unsigned char *)ucCbcEncData , uCbcEncLen);
    memcpy(&data[len], &uBaseLen, 1);
    len = len + 1;
    memcpy(&data[len],  ucBaseData, uBaseLen);
    len = len + uBaseLen;

    memset(ucCbcEncData , 0 , 128);
    memset(ucBaseData , 0 , 128);
    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    uCbcEncLen = strlen(g_tHttpPostContent.ubaChipId);
    uCbcEncLen = (((uCbcEncLen  >> 4) + 1) << 4);
    BleWifi_CBC_encrypt((void *)g_tHttpPostContent.ubaChipId , strlen(g_tHttpPostContent.ubaChipId) , iv , g_ucSecretKey , (void *)ucCbcEncData);
    mbedtls_base64_encode((unsigned char *)ucBaseData , 128  ,&uBaseLen ,(unsigned char *)ucCbcEncData , uCbcEncLen);
    memcpy(&data[len], &uBaseLen, 1);
    len = len + 1;
    memcpy(&data[len],  ucBaseData, uBaseLen);
    len = len + uBaseLen;


    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_APP_DEVICE_INFO, data, len);

    free(data);

}

static void BleWifi_AppWifiConnection(uint8_t *data, int len)
{
    int totallen = data[0] + (data[1] << 8);
    char *AllURL = NULL;
    char HeaderStr[12] = {0};
    const char *PosStart = (char *) &data[2];
    const char *PosResult;
    const char *NeedleStart = "/";
    uint8_t TerminalNull = 1;

    T_MwFim_GP12_HttpHostInfo HostInfo;
    memset(&HostInfo ,0, sizeof(T_MwFim_GP12_HttpHostInfo));

    if(NULL != strstr(PosStart, "http://"))
    {
        strcpy(HeaderStr, "http://");
    }
    else if(NULL != strstr(PosStart, "https://"))
    {
        strcpy(HeaderStr, "https://");
    }
    else
    {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_APP_HOST_INFO, 1);

        return;
    }

    AllURL = (char *)malloc(totallen + TerminalNull);
    if (NULL == AllURL)
    {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_APP_HOST_INFO, 1);
        return;
    }

    memset(AllURL ,0, totallen);

    if ( (PosStart=strstr(PosStart,HeaderStr)) != NULL ) {
        PosResult = PosStart;
    }

    // Copy http://testapi.coolkit.cn:8080/api/user/device/update to AllURL
    memcpy(AllURL, PosResult, totallen);
    AllURL[totallen] = '\0';

    PosResult = strstr ((AllURL + strlen(HeaderStr)), NeedleStart);

    // Copy testapi.coolkit.cn:8080 / testapi.coolkit.cn to URL.
    // Calculate URL length = PosResult - AllURL - strlen(HeaderStr)
    strncpy (HostInfo.ubaHostInfoURL, (AllURL + strlen(HeaderStr)), PosResult - AllURL - strlen(HeaderStr));

    memcpy (HostInfo.ubaHostInfoDIR, (AllURL + strlen(HeaderStr) + strlen(HostInfo.ubaHostInfoURL)), (totallen - strlen(HeaderStr) - strlen(HostInfo.ubaHostInfoURL)));

    memcpy(&g_tHostInfo,&HostInfo,sizeof(T_MwFim_GP12_HttpHostInfo));

    if (MW_FIM_OK != MwFim_FileWrite(MW_FIM_IDX_GP12_PROJECT_HOST_INFO, 0, MW_FIM_GP12_HTTP_HOST_INFO_SIZE, (uint8_t *)&HostInfo)) {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_APP_HOST_INFO, 1);
    }

    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_CHANGE_HTTPURL, true);

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_APP_HOST_INFO, 0);
    osDelay(100);
    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_NETWORKING_STOP, NULL, 0);

    Sensor_Data_Push(BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR), SHORT_TRIG,  BleWifi_SntpGetRawData());
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_DATA_POST, NULL, 0);

    if(AllURL != NULL)
    {
        free(AllURL);
    }
}

static void BleWifi_MP_CalVbat(uint8_t *data, int len)
{
    float fTargetVbat;

    memcpy(&fTargetVbat, &data[0], 4);
    Hal_Aux_VbatCalibration(fTargetVbat);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_CAL_VBAT, 0);
}

static void BleWifi_MP_CalIoVoltage(uint8_t *data, int len)
{
    float fTargetIoVoltage;
    uint8_t ubGpioIdx;

    memcpy(&ubGpioIdx, &data[0], 1);
    memcpy(&fTargetIoVoltage, &data[1], 4);
    Hal_Aux_IoVoltageCalibration(ubGpioIdx, fTargetIoVoltage);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_CAL_IO_VOLTAGE, 0);
}

static void BleWifi_MP_CalTmpr(uint8_t *data, int len)
{
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_CAL_TMPR, 0);
}

static void BleWifi_MP_SysModeWrite(uint8_t *data, int len)
{
    T_MwFim_SysMode tSysMode;

    // set the settings of system mode
    tSysMode.ubSysMode = data[0];
    if (tSysMode.ubSysMode < MW_FIM_SYS_MODE_MAX)
    {
        if (MW_FIM_OK == MwFim_FileWrite(MW_FIM_IDX_GP03_PATCH_SYS_MODE, 0, MW_FIM_SYS_MODE_SIZE, (uint8_t*)&tSysMode))
        {
            BleWifi_Ctrl_SysModeSet(tSysMode.ubSysMode);
            BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_SYS_MODE_WRITE, 0);
            return;
        }
    }

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_SYS_MODE_WRITE, 1);
}

static void BleWifi_MP_SysModeRead(uint8_t *data, int len)
{
    uint8_t ubSysMode;

    ubSysMode = BleWifi_Ctrl_SysModeGet();
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_SYS_MODE_READ, ubSysMode);
}

static void BleWifi_Eng_SysReset(uint8_t *data, int len)
{
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_SYS_RESET, 0);

    // wait the BLE response, then reset the system
    osDelay(3000);
    Hal_Sys_SwResetAll();
}

static void BleWifi_Eng_BleCmd(uint8_t *data, int len)
{
    msg_print_uart1("+BLE:%s\r\n", data);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CMD, 0);
}

static void BleWifi_Ble_ProtocolHandler_Auth(uint16_t type, uint8_t *data, int len)
{
    unsigned char iv[IV_SIZE] = {0};
    unsigned char ucBase64Dec[MAX_AUTH_DATA_SIZE + 1] = {0};
    size_t uBase64DecLen = 0;
    unsigned char ucUuidEncData[UUID_ENC_SIZE + 1] = {0};
    unsigned char ucUuidtoBaseData[ENC_UUID_TO_BASE64_SIZE + 1] = {0};  // to base64 max size ((4 * n / 3) + 3) & ~3
    size_t uBase64EncLen = 0;

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_AUTH \r\n");

    if(len > MAX_AUTH_DATA_SIZE)
    {
        uBase64EncLen = 0;
        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_AUTH, ucUuidtoBaseData , uBase64EncLen);
        return ;
    }

    mbedtls_md5((unsigned char *)&g_tHttpPostContent.ubaApiKey , strlen(g_tHttpPostContent.ubaApiKey) , g_ucSecretKey);

    mbedtls_base64_decode(ucBase64Dec , MAX_AUTH_DATA_SIZE + 1 , &uBase64DecLen , (unsigned char *)data , len);
    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    BleWifi_CBC_decrypt((void *)ucBase64Dec , uBase64DecLen , iv , g_ucSecretKey , g_ucAppCode);

    printf("g_ucAppCode = %s\r\n",g_ucAppCode);

    //UUID generate
    memset(g_ucDeviceCode , 0 , UUID_SIZE);
    if(BleWifi_UUID_Generate(g_ucDeviceCode , (UUID_SIZE + 1)) == false)
    {
        uBase64EncLen = 0;
        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_AUTH, ucUuidtoBaseData , uBase64EncLen);
        return ;
    }

    printf("g_ucDeviceCode = %s\r\n",g_ucDeviceCode);

    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    BleWifi_CBC_encrypt((void *)g_ucDeviceCode , UUID_SIZE , iv , g_ucSecretKey , (void *)ucUuidEncData);
    mbedtls_base64_encode((unsigned char *)ucUuidtoBaseData , ENC_UUID_TO_BASE64_SIZE + 1  ,&uBase64EncLen ,(unsigned char *)ucUuidEncData , UUID_ENC_SIZE);

    //BLEWIFI_INFO("ucUuidtoBaseData = %s\r\n",ucUuidtoBaseData);

    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_AUTH, ucUuidtoBaseData , uBase64EncLen);
}

static void BleWifi_Ble_ProtocolHandler_AuthToken(uint16_t type, uint8_t *data, int len)
{
    unsigned char iv[IV_SIZE + 1] = {0};
    unsigned char ucBase64Dec[MAX_AUTH_TOKEN_DATA_SIZE + 1] = {0};
    size_t uBase64DecLen = 0;
    unsigned char ucCbcDecData[MAX_AUTH_TOKEN_DATA_SIZE + 1] = {0};
    uint8_t u8Ret = 0; // 0 success , 1 fail
    char * pcToken = NULL;

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_AUTH_TOKEN \r\n");

    if(len > MAX_AUTH_TOKEN_DATA_SIZE)
    {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_AUTH_TOKEN , u8Ret);
        return;
    }

    mbedtls_base64_decode(ucBase64Dec , MAX_AUTH_TOKEN_DATA_SIZE + 1 , &uBase64DecLen , (unsigned char *)data , len);
    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    BleWifi_CBC_decrypt((void *)ucBase64Dec , uBase64DecLen , iv , g_ucSecretKey , ucCbcDecData);

    printf("ucCbcDecData = %s\r\n",ucCbcDecData);

    pcToken = strtok((char *)ucCbcDecData , "_");
    if(strcmp(pcToken ,(char *)g_ucAppCode) != 0)
    {
        u8Ret = 1;
    }
    pcToken = strtok(NULL , "_");
    if(strcmp(pcToken ,(char *)g_ucDeviceCode) != 0)
    {
        u8Ret = 1;
    }

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_AUTH_TOKEN , u8Ret);
}

static void BleWifi_Ble_ProtocolHandler_Scan(uint16_t type, uint8_t *data, int len)
{
    wifi_scan_config_t scan_config = {0};

    scan_config.show_hidden = data[0];
    scan_config.scan_type = WIFI_SCAN_TYPE_MIX;

    BleWifi_Wifi_Scan_Check(&scan_config);

}

static void BleWifi_Ble_ProtocolHandler_Connect(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_CONNECT \r\n");
    BleWifi_Wifi_DoConnect(data, len);
}

static void BleWifi_Ble_ProtocolHandler_Manually_Connect_AP(uint16_t type, uint8_t *data, int len)
{
    unsigned char ucDecPassword[BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE + 1] = {0};
    unsigned char iv[IV_SIZE + 1] = {0};
    size_t u16DecPasswordLen = 0;
    uint8_t u8PasswordLen = 0;
    uint8_t u8TimeOutSettings;
    wifi_scan_config_t scan_config = {0};

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MANUAL_CONNECT_AP \r\n");

    memset(&wifi_config_req_connect , 0 ,sizeof(wifi_config_t));

    BLEWIFI_INFO("BLEWIFI: Recv Connect Request\r\n");


    // data format for connecting a hidden AP
    //--------------------------------------------------------------------------------------
    //|        1     |    1~32    |    1      |     1    |         1          |   8~63     |
    //--------------------------------------------------------------------------------------
    //| ssid length  |    ssid    | Connected |  timeout |   password_length  |   password |
    //--------------------------------------------------------------------------------------

    wifi_config_req_connect.sta_config.ssid_length = data[0];

    memcpy(wifi_config_req_connect.sta_config.ssid, &data[1], data[0]);
    BLEWIFI_INFO("\r\n %d  Recv Connect Request  SSID is %s\r\n",__LINE__, wifi_config_req_connect.sta_config.ssid);

    if (len >= (1 +data[0] +1 +1 +1) ) // ssid length(1) + ssid (data(0)) + Connected(1) + timeout + password_length (1)
    {
        BLEWIFI_INFO(" \r\n Do Manually connect %d \r\n ",__LINE__);

        u8TimeOutSettings = data[data[0] + 2];
        if(u8TimeOutSettings <= BLEWIFI_WIFI_DEFAULT_TIMEOUT) //set timeout
        {
            g_ubAppCtrlRequestRetryTimes = BLEWIFI_WIFI_REQ_CONNECT_RETRY_TIMES;
            g_u8ConnectTimeOutFlag = 1;
        }
        else
        {
            osTimerStop(g_tAppWifiConnectTimeOutTimerId);
            osTimerStart(g_tAppWifiConnectTimeOutTimerId, u8TimeOutSettings - BLEWIFI_WIFI_DEFAULT_TIMEOUT);
        }

        u8PasswordLen = data[data[0] + 3];
        if(u8PasswordLen == 0) //password len = 0
        {
            printf("password_length = 0\r\n");
            wifi_config_req_connect.sta_config.password_length = 0;
            memset((char *)wifi_config_req_connect.sta_config.password, 0 , WIFI_LENGTH_PASSPHRASE);
        }
        else
        {
            if(u8PasswordLen > BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE)
            {
                BLEWIFI_INFO(" \r\n Not do Manually connect %d \r\n ",__LINE__);
                BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_PASSWORD_FAIL);
                return;
            }
            mbedtls_base64_decode(ucDecPassword , BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE + 1 , &u16DecPasswordLen , (unsigned char *)&data[data[0] + 4] , u8PasswordLen);
            memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
            BleWifi_CBC_decrypt((void *)ucDecPassword , u16DecPasswordLen , iv , g_ucSecretKey , (void *)wifi_config_req_connect.sta_config.password);
            wifi_config_req_connect.sta_config.password_length = strlen((char *)wifi_config_req_connect.sta_config.password);
            //printf("password = %s\r\n" , wifi_config_req_connect.sta_config.password);
            //printf("password_length = %u\r\n" , wifi_config_req_connect.sta_config.password_length);
        }

        g_bManuallyConnScanFlag = true;
        g_u8ManuallyConnScanCnt = 1;

        scan_config.show_hidden = data[0];
        scan_config.scan_type = WIFI_SCAN_TYPE_MIX;
        //printf("wifi_config_req_connect.sta_config.ssid = %s\r\n",wifi_config_req_connect.sta_config.ssid);
        //printf("wifi_config_req_connect.sta_config.ssid_length = %u\r\n",wifi_config_req_connect.sta_config.ssid_length);

        memcpy(scan_config.ssid , wifi_config_req_connect.sta_config.ssid , wifi_config_req_connect.sta_config.ssid_length);

        BleWifi_Wifi_Scan_Check(&scan_config);

    }
    else
    {
        BLEWIFI_INFO(" \r\n Not do Manually connect %d \r\n ",__LINE__);
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_CONNECTED_FAIL);
    }
}


static void BleWifi_Ble_ProtocolHandler_Disconnect(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_DISCONNECT \r\n");
    BleWifi_Wifi_DoDisconnect();
}

static void BleWifi_Ble_ProtocolHandler_Reconnect(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_RECONNECT \r\n");
}

static void BleWifi_Ble_ProtocolHandler_ReadDeviceInfo(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_READ_DEVICE_INFO \r\n");
    BleWifi_Wifi_ReadDeviceInfo();
}

static void BleWifi_Ble_ProtocolHandler_WriteDeviceInfo(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_WRITE_DEVICE_INFO \r\n");
    BleWifi_Wifi_WriteDeviceInfo(data, len);
}

static void BleWifi_Ble_ProtocolHandler_WifiStatus(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_WIFI_STATUS \r\n");
    BleWifi_Wifi_SendStatusInfo(BLEWIFI_RSP_WIFI_STATUS);
}

static void BleWifi_Ble_ProtocolHandler_Reset(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_RESET \r\n");
    BleWifi_Wifi_ResetRecord();
}

#if (BLE_OTA_FUNCTION_EN == 1)
static void BleWifi_Ble_ProtocolHandler_OtaVersion(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_VERSION \r\n");
    BleWifi_HandleOtaVersionReq(data, len);
}

static void BleWifi_Ble_ProtocolHandler_OtaUpgrade(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_UPGRADE \r\n");
    BleWifi_HandleOtaUpgradeReq(data, len);
}

static void BleWifi_Ble_ProtocolHandler_OtaRaw(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_RAW \r\n");
    BleWifi_HandleOtaRawReq(data, len);
}

static void BleWifi_Ble_ProtocolHandler_OtaEnd(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_END \r\n");
    BleWifi_HandleOtaEndReq(data, len);
}
#endif

#if (WIFI_OTA_FUNCTION_EN == 1)
static void BleWifi_Ble_ProtocolHandler_HttpOtaTrig(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_HTTP_OTA_TRIG \r\n");
    BleWifi_Wifi_OtaTrigReq(WIFI_OTA_HTTP_URL);
}

static void BleWifi_Ble_ProtocolHandler_HttpOtaDeviceVersion(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_HTTP_OTA_DEVICE_VERSION \r\n");
    BleWifi_Wifi_OtaDeviceVersionReq();
}

static void BleWifi_Ble_ProtocolHandler_HttpOtaServerVersion(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_HTTP_OTA_SERVER_VERSION \r\n");
    BleWifi_Wifi_OtaServerVersionReq();
}
#endif

static void BleWifi_Ble_ProtocolHandler_MpCalVbat(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_CAL_VBAT \r\n");
    BleWifi_MP_CalVbat(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpCalIoVoltage(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_CAL_IO_VOLTAGE \r\n");
    BleWifi_MP_CalIoVoltage(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpCalTmpr(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_CAL_TMPR \r\n");
    BleWifi_MP_CalTmpr(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpSysModeWrite(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_SYS_MODE_WRITE \r\n");
    BleWifi_MP_SysModeWrite(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpSysModeRead(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_SYS_MODE_READ \r\n");
    BleWifi_MP_SysModeRead(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngSysReset(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_SYS_RESET \r\n");
    BleWifi_Eng_SysReset(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngWifiMacWrite(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_WIFI_MAC_WRITE \r\n");
    BleWifi_Wifi_MacAddrWrite(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngWifiMacRead(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_WIFI_MAC_READ \r\n");
    BleWifi_Wifi_MacAddrRead(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngBleMacWrite(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_BLE_MAC_WRITE \r\n");
    BleWifi_Ble_MacAddrWrite(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngBleMacRead(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_BLE_MAC_READ \r\n");
    BleWifi_Ble_MacAddrRead(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngBleCmd(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_BLE_CMD \r\n");
    BleWifi_Eng_BleCmd(data, len);
}

void BleWifi_Ble_ProtocolHandler_EngBleCloudInfoWrite(uint16_t type, uint8_t *data, int len)
{
    T_SendJSONParam DeviceData = {0};
    uint16_t i = 0;
    uint8_t u8DelimitersCnt = 0; // The count of ','
    uint16_t u16StrStartPos = 0;
    uint16_t u16StrLen = 0;

    if(data == NULL)
    {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_WRITE , 1);
        return ;
    }

    for(i = 0 ; i < len ; i++)
    {
        if(data[i] == ',')
        {
            data[i] = 0;
            u16StrLen = (i - u16StrStartPos);
            if(u8DelimitersCnt == 0)
            {
                if(u16StrLen > DEVICE_ID_LEN)
                {
                    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_WRITE , 1);
                    return ;
                }
                else
                {
                    memcpy(DeviceData.ubaDeviceId , data + u16StrStartPos , u16StrLen);
                }

            }
            else if(u8DelimitersCnt == 1)
            {
                if(u16StrLen > API_KEY_LEN)
                {
                    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_WRITE , 1);
                    return ;
                }
                else
                {
                    memcpy(DeviceData.ubaApiKey , data + u16StrStartPos , u16StrLen);
                }
            }
            else if(u8DelimitersCnt == 2)
            {
                if(u16StrLen > CHIP_ID_LEN)
                {
                    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_WRITE , 1);
                    return ;
                }
                else
                {
                    memcpy(DeviceData.ubaWifiMac , data + u16StrStartPos , u16StrLen);
                }
            }
            else if(u8DelimitersCnt == 3)
            {
                if(u16StrLen > CHIP_ID_LEN)
                {
                    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_WRITE , 1);
                    return ;
                }
                else
                {
                    memcpy(DeviceData.ubaBleMac , data + u16StrStartPos , u16StrLen);
                }
            }
            u16StrStartPos = (i + 1);
            u8DelimitersCnt++;
        }
        else if(u8DelimitersCnt == DELIMITER_NUM) // last param
        {
            break;
        }
    }

    if(u8DelimitersCnt != DELIMITER_NUM)
    {
        printf("CloudInfoWrite format err\r\n");
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_WRITE , 1);
    }

    //last param handle
    //if last param need to convert integer , it must special handling. 1. malloc + 1 size , 2. strcpy 3. strtoul
    u16StrLen = len - u16StrStartPos;
    if((u16StrLen) > MODEL_ID_LEN)
    {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_WRITE , 1);
    }
    else
    {
        memcpy(DeviceData.ubaDeviceModel , data + u16StrStartPos , u16StrLen);
    }

    Write_data_into_fim(&DeviceData);

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_WRITE , 0);
}

void BleWifi_Ble_ProtocolHandler_EngBleCloudInfoRead(uint16_t type, uint8_t *data, int len)
{
    uint8_t u8RspData[RSP_CLOUD_INFO_READ_PAYLOAD_SIZE] = {0};
    int16_t s16RspLen = 0;
    T_MwFim_GP12_HttpPostContent HttpPostContent = {0};
    uint8_t ubaMacAddr[6];

    if(MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_DEVICE_AUTH_CONTENT, 0 , MW_FIM_GP12_HTTP_POST_CONTENT_SIZE, (uint8_t*)&HttpPostContent) != MW_FIM_OK)
    {
        s16RspLen = 0;
        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_READ, u8RspData , s16RspLen);
        return ;
    }

    ble_get_config_bd_addr(ubaMacAddr);

    s16RspLen = snprintf((char *)u8RspData , RSP_CLOUD_INFO_READ_PAYLOAD_SIZE , "%s,%s,%s,%02x:%02x:%02x:%02x:%02x:%02x,%s" ,
                        HttpPostContent.ubaDeviceId , HttpPostContent.ubaApiKey , HttpPostContent.ubaChipId ,
                        ubaMacAddr[5] ,ubaMacAddr[4] ,ubaMacAddr[3] , ubaMacAddr[2] , ubaMacAddr[1] , ubaMacAddr[0] ,
                        HttpPostContent.ubaModelId);

    if(s16RspLen < 0) //error handle
    {
        s16RspLen = 0;
        memset(&u8RspData , 0 , RSP_CLOUD_INFO_READ_PAYLOAD_SIZE);
    }

    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_READ, u8RspData , s16RspLen);
}

static void BleWifi_Ble_ProtocolHandler_AppDeviceInfo(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_APP_DEVICE_INFO \r\n");
    BleWifi_AppDeviceInfoRsp();
}

static void BleWifi_Ble_ProtocolHandler_AppWifiConnection(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_APP_HOST_INFO \r\n");
    BleWifi_AppWifiConnection(data, len);
}

// it is used in the ctrl task
void BleWifi_Ble_ProtocolHandler(uint16_t type, uint8_t *data, int len)
{
    uint32_t i = 0;

    while (g_tBleProtocolHandlerTbl[i].ulEventId != 0xFFFFFFFF)
    {
        // match
        if (g_tBleProtocolHandlerTbl[i].ulEventId == type)
        {
            g_tBleProtocolHandlerTbl[i].fpFunc(type, data, len);
            break;
        }

        i++;
    }

    // not match
    if (g_tBleProtocolHandlerTbl[i].ulEventId == 0xFFFFFFFF)
    {
    }
}

// it is used in the ctrl task
void BleWifi_Ble_DataRecvHandler(uint8_t *data, int data_len)
{
    blewifi_hdr_t *hdr = NULL;
    int hdr_len = sizeof(blewifi_hdr_t);

    /* 1.aggregate fragment data packet, only first frag packet has header */
    /* 2.handle blewifi data packet, if data frame is aggregated completely */
    if (g_rx_packet.offset == 0)
    {
        hdr = (blewifi_hdr_t*)data;
        g_rx_packet.total_len = hdr->data_len + hdr_len;
        g_rx_packet.remain = g_rx_packet.total_len;
        g_rx_packet.aggr_buf = malloc(g_rx_packet.total_len);

        if (g_rx_packet.aggr_buf == NULL) {
           BLEWIFI_ERROR("%s no mem, len %d\n", __func__, g_rx_packet.total_len);
           return;
        }
    }

    // error handle
    // if the size is overflow, don't copy the whole data
    if (data_len > g_rx_packet.remain)
        data_len = g_rx_packet.remain;

    memcpy(g_rx_packet.aggr_buf + g_rx_packet.offset, data, data_len);
    g_rx_packet.offset += data_len;
    g_rx_packet.remain -= data_len;

    /* no frag or last frag packet */
    if (g_rx_packet.remain == 0)
    {
        hdr = (blewifi_hdr_t*)g_rx_packet.aggr_buf;
        BleWifi_Ble_ProtocolHandler(hdr->type, g_rx_packet.aggr_buf + hdr_len,  (g_rx_packet.total_len - hdr_len));
        g_rx_packet.offset = 0;
        g_rx_packet.remain = 0;
        free(g_rx_packet.aggr_buf);
        g_rx_packet.aggr_buf = NULL;
    }
}

void BleWifi_Ble_DataSendEncap(uint16_t type_id, uint8_t *data, int total_data_len)
{
    blewifi_hdr_t *hdr = NULL;
    int remain_len = total_data_len;

    /* 1.fragment data packet to fit MTU size */

    /* 2.Pack blewifi header */
    hdr = malloc(sizeof(blewifi_hdr_t) + remain_len);
    if (hdr == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: memory alloc fail\r\n");
        return;
    }

    hdr->type = type_id;
    hdr->data_len = remain_len;
    if (hdr->data_len)
        memcpy(hdr->data, data, hdr->data_len);

    BLEWIFI_DUMP("[BLEWIFI]:out packet", (uint8_t*)hdr, (hdr->data_len + sizeof(blewifi_hdr_t)));

    /* 3.send app data to BLE stack */
    BleWifi_Ble_SendAppMsgToBle(BLEWIFI_APP_MSG_SEND_DATA, (hdr->data_len + sizeof(blewifi_hdr_t)), (uint8_t *)hdr);

    free(hdr);
}

void BleWifi_Ble_SendResponse(uint16_t type_id, uint8_t status)
{
    BleWifi_Ble_DataSendEncap(type_id, &status, 1);
}
