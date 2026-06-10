#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "systick.h"
#include "wifi_drv.h"
#include "delay.h"
#include "ntc_drv.h"
#include "rh_drv.h"
#include "store_app.h"
#include "inflash_drv.h"
#include "md5.h"
#include "app_config.h"
#include "app_log.h"
#include "http_parser.h"
#include "ota_resume.h"

static MD5_CTX g_md5c;

static char g_recvStrBuf[WIFI_MAX_BUF_SIZE];
static uint32_t g_pktRcvdLen;

static char g_httpPostBuf[500];  // 存放HTTP POST请求的命令字符串,在多个case中使用相同的post命令，在多个函数中使用相同的buf，避免多次分配内�?

static void ClearRecvWifiStr(void)
{
	memset(g_recvStrBuf, 0, WIFI_MAX_BUF_SIZE);
}

typedef enum
{
	WIFI_COMM_WAIT,    // 正在通信�?
	WIFI_COMM_OK,      // 初始化或者通信成功
	WIFI_COMM_FAIL,    // 通信失败
} WifiCommState_t;

/**
***********************************************************
* @brief 发送AT命令并解析判断响应数�?
* @param cmd,输入，发送的AT命令字符�?
* @param rsp,输入，发送的AT命令对应的响应字符串
* @param timeoutMs,输入，超时时�?
* @param maxRetryNum,输入，最大重试次�?
* @return WifiCommState_t,返回通信状�?
***********************************************************
*/
WifiCommState_t AtCmdHandle(char *cmd, char *rsp, uint32_t timeoutMs, uint8_t maxRetryNum)
{
    static WifiCommState_t s_commState = WIFI_COMM_OK;
    static uint64_t s_sendCmdTime;
    static uint8_t  s_retryCount;

    switch (s_commState)
    {
        case WIFI_COMM_OK:    // 上次成功，或初始状�?
        case WIFI_COMM_FAIL:  // 上次失败，fall-through，同样重新发�?
            if (cmd != NULL)
			{
				SendWifiModuleStr(cmd);
			}
            s_commState = WIFI_COMM_WAIT;
            s_sendCmdTime = GetSysRunTime();
            s_retryCount = 0;
            break;

        case WIFI_COMM_WAIT:
            if ((GetSysRunTime() - s_sendCmdTime) < timeoutMs)
            {
                g_pktRcvdLen = RecvWifiModuleStr(g_recvStrBuf);
                if (g_pktRcvdLen != 0)
                {
                    if (strstr(g_recvStrBuf, rsp) != NULL)
                    {
                        s_commState  = WIFI_COMM_OK;
                        s_retryCount = 0;
                    }
                }
            }
            else  // 超时
            {
                if (s_retryCount < maxRetryNum)
                {
                    s_retryCount++;
                    if (cmd != NULL)
					{
						SendWifiModuleStr(cmd);
					}
                    s_sendCmdTime = GetSysRunTime();
                }
                else
                {
                    s_commState = WIFI_COMM_FAIL;
                    s_retryCount = 0;
                }
            }
            break;

        default:
            break;
    }
    return s_commState;
}

typedef struct {
    /* 要发送的AT命令 */
    char *cmd;
    /* 期望的应答数据，默认处理匹配到该字符串认为命令执行成�? */
    char *rsp;
    /* 得到应答的超时时间，达到超时时间为执行失败，单位ms*/
    uint32_t timeoutMs;
	uint8_t maxRetryNum;
} AtCmdInfo_t;

typedef enum
{
	AT_RST,
	AT_RST_DELAY,
	AT_E0,
	AT_CWMODE_1,
} AtInitModuleCmd_t;

static AtCmdInfo_t g_initModuleCmd[] = 
{
	[AT_RST] = {
		.cmd = "AT+RST\r\n",
		.rsp = "OK",
		.timeoutMs = 500,
		.maxRetryNum = 3,
	},
	[AT_RST_DELAY] = {  // 只为非阻塞延�?2S，延�?1S有些�?
		.cmd = NULL,
		.rsp = "deadbeaf",
		.timeoutMs = 2000,
		.maxRetryNum = 0,
	},
	[AT_E0] = {
		.cmd = "ATE0\r\n",
		.rsp = "OK",
		.timeoutMs = 500,
		.maxRetryNum = 3,
	},
	[AT_CWMODE_1] = {
		.cmd = "AT+CWMODE=1\r\n",
		.rsp = "OK",
		.timeoutMs = 500,
		.maxRetryNum = 0,
	},
};

static	WifiCommState_t InitWifiModule(void)
{
	WifiCommState_t commState;
	static AtInitModuleCmd_t s_cmdType = AT_RST;
	
	switch (s_cmdType)
	{
		case AT_RST:
			commState = AtCmdHandle(g_initModuleCmd[AT_RST].cmd, g_initModuleCmd[AT_RST].rsp, 
						g_initModuleCmd[AT_RST].timeoutMs, g_initModuleCmd[AT_RST].maxRetryNum);
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_RST_DELAY;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				return WIFI_COMM_FAIL;
			}
			break;
		case AT_RST_DELAY:
			commState = AtCmdHandle(g_initModuleCmd[AT_RST_DELAY].cmd, g_initModuleCmd[AT_RST_DELAY].rsp, 
						g_initModuleCmd[AT_RST_DELAY].timeoutMs, g_initModuleCmd[AT_RST_DELAY].maxRetryNum);	
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_E0;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_E0;
			}
			break;
		case AT_E0:
			commState = AtCmdHandle(g_initModuleCmd[AT_E0].cmd, g_initModuleCmd[AT_E0].rsp, 
						g_initModuleCmd[AT_E0].timeoutMs, g_initModuleCmd[AT_E0].maxRetryNum);
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CWMODE_1;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_RST;
				return WIFI_COMM_FAIL;
			}
			break;
		case AT_CWMODE_1:
			commState = AtCmdHandle(g_initModuleCmd[AT_CWMODE_1].cmd, g_initModuleCmd[AT_CWMODE_1].rsp, 
						g_initModuleCmd[AT_CWMODE_1].timeoutMs, g_initModuleCmd[AT_CWMODE_1].maxRetryNum);
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_RST;
				return WIFI_COMM_OK;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_RST;
				return WIFI_COMM_FAIL;
			}
			break;
		default:
			break;
	}
	return WIFI_COMM_WAIT;
}

typedef enum 
{
	AT_CWJAP_SSID_PWD,
	AT_CWJAP_DELAY,
} AtConnectHotspotCmd_t;

static AtCmdInfo_t g_ConnectHotspotCmd[] = {
	[AT_CWJAP_SSID_PWD] = {
		.cmd = "AT+CWJAP=\"pratol\",\"123456789\"\r\n",   // 这里的\是给编译器用�?
		.rsp = "GOT IP",
		.timeoutMs = 15000,
		.maxRetryNum = 3
	},
	[AT_CWJAP_DELAY] = {
		.cmd = NULL,
		.rsp = "deadbeaf",
		.timeoutMs = 5000,
		.maxRetryNum = 0
	},
};

static WifiCommState_t ConnectWifiHotspot(void)
{
	WifiCommState_t commState;
	static AtConnectHotspotCmd_t s_cmdType = AT_CWJAP_SSID_PWD;
	switch (s_cmdType)
	{		
		case AT_CWJAP_SSID_PWD:
			commState = AtCmdHandle(g_ConnectHotspotCmd[AT_CWJAP_SSID_PWD].cmd, g_ConnectHotspotCmd[AT_CWJAP_SSID_PWD].rsp, 
									g_ConnectHotspotCmd[AT_CWJAP_SSID_PWD].timeoutMs, g_ConnectHotspotCmd[AT_CWJAP_SSID_PWD].maxRetryNum);
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CWJAP_DELAY;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				return WIFI_COMM_FAIL;
			}
			break;
		case AT_CWJAP_DELAY:
			commState = AtCmdHandle(g_ConnectHotspotCmd[AT_CWJAP_DELAY].cmd, g_ConnectHotspotCmd[AT_CWJAP_DELAY].rsp, 
									g_ConnectHotspotCmd[AT_CWJAP_DELAY].timeoutMs, g_ConnectHotspotCmd[AT_CWJAP_DELAY].maxRetryNum);
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CWJAP_SSID_PWD;
				return WIFI_COMM_OK;
			}
			else if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CWJAP_SSID_PWD;
				return WIFI_COMM_OK;
			}
			break;		
		default:
			break;
	}
	return WIFI_COMM_WAIT;	
}

typedef enum 
{
	AT_CONNECT_OTA_SERVER = 0,
	AT_CONNECT_SERVER_STATE,
	AT_REPORT_VER_PREPARE,
	AT_REPORT_VER_PROCESS,
} AtConnectServerCmd_t;

static AtCmdInfo_t g_connectServerCmd[] = {
	[AT_CONNECT_OTA_SERVER] = {
        .cmd = "AT+CIPSTART=\"TCP\",\"iot-api.heclouds.com\",80\r\n",
		.rsp = "CONNECT",
		.timeoutMs = 5000,	
		.maxRetryNum = 3,
	},
	[AT_CONNECT_SERVER_STATE] = {
        .cmd = "AT+CIPSTATE?\r\n",
		.rsp = "OK",
		.timeoutMs = 5000,
		.maxRetryNum = 3,		
	},
	
	[AT_REPORT_VER_PREPARE] = {
		.cmd = "AT+CIPSEND=%d\r\n",  // 第三步，计算下面字符串长度，AT+CIPSEND=333\r\n
		.rsp = ">",
		.timeoutMs = 5000,	
	},
	[AT_REPORT_VER_PROCESS] = {
        .cmd =  "POST http://iot-api.heclouds.com/fuse-ota/KPw28SAyeA/board1/version HTTP/1.1\r\n" 
			    "Authorization:version=2018-10-31&res=products%%2FKPw28SAyeA%%2Fdevices%%2Fboard1&et=1800523492&method=md5&sign=NUF4HQG2vJ6iZK281Ugp5g%%3D%%3D\r\n"  // 注意%如果要作为字符，而不是格式化符号，需要再加上一�?%
				// version=2018-10-31&res=products%2F7V45i9XgPY%2Fdevices%2Fboard1&et=1800501704&method=md5&sign=LeUmhaQFAf8EB0d784Npfw%3D%3D  ,因为下面代码要使用sprintf会将这里�?%按照格式化符合处理，所以需要再加上%就是实际的字�?%�?
				"Content-Type:application/json\r\n" 
				"Host:iot-api.heclouds.com\r\n" 
			    "Content-Length:%d\r\n\r\n"    // 第二步，计算下面字符串长度，每行的\r\n是用来实现字符串换行的，这一行必须要多一个\r\n
				"%s",   // 第一�?"{\"s_version\":\"1.0\", \"f_version\":\"1.0\"}"
		.rsp = "succ",
		.timeoutMs = 10000,	
	},
};

#define VERSION_STR        "{\"s_version\":\"%s\", \"f_version\":\"1.0\"}"

static WifiCommState_t ConnectIotServer(void)
{
	WifiCommState_t commState;
	static AtConnectServerCmd_t s_cmdType = AT_CONNECT_OTA_SERVER;
	char strBuf[50] = {0};
	uint16_t strLen = 0;

	switch (s_cmdType)
	{
		case AT_CONNECT_OTA_SERVER:
			commState = AtCmdHandle(g_connectServerCmd[AT_CONNECT_OTA_SERVER].cmd, g_connectServerCmd[AT_CONNECT_OTA_SERVER].rsp,
									g_connectServerCmd[AT_CONNECT_OTA_SERVER].timeoutMs, g_connectServerCmd[AT_CONNECT_OTA_SERVER].maxRetryNum);
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CONNECT_SERVER_STATE;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				return WIFI_COMM_FAIL;
			}
			break;
		case AT_CONNECT_SERVER_STATE:
			commState = AtCmdHandle(g_connectServerCmd[AT_CONNECT_SERVER_STATE].cmd, g_connectServerCmd[AT_CONNECT_SERVER_STATE].rsp,
									g_connectServerCmd[AT_CONNECT_SERVER_STATE].timeoutMs, g_connectServerCmd[AT_CONNECT_SERVER_STATE].maxRetryNum);
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_REPORT_VER_PREPARE;

				char verBuf[50] = {0};
				GetSoftwareVersionParam(strBuf);       // "1.0"
				sprintf(verBuf, VERSION_STR, strBuf);  // "{\"s_version\":\"%s\", \"f_version\":\"1.0\"}"
				//sprintf(verBuf, VERSION_STR, "1.0");   // "{\"s_version\":\"1.0\", \"f_version\":\"1.0\"}"
				strLen = strlen(verBuf);               // 38
				memset(g_httpPostBuf, 0, sizeof(g_httpPostBuf));
				sprintf(g_httpPostBuf, g_connectServerCmd[AT_REPORT_VER_PROCESS].cmd, strLen, verBuf); // 格式化POST�?
			}
			if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CONNECT_OTA_SERVER;
				return WIFI_COMM_FAIL;
			}
			break;
		case AT_REPORT_VER_PREPARE:
			sprintf(strBuf, g_connectServerCmd[AT_REPORT_VER_PREPARE].cmd, strlen(g_httpPostBuf)); // "AT+CIPSEND=333\r\n",
			commState = AtCmdHandle(strBuf, g_connectServerCmd[AT_REPORT_VER_PREPARE].rsp,
									g_connectServerCmd[AT_REPORT_VER_PREPARE].timeoutMs, g_connectServerCmd[AT_REPORT_VER_PREPARE].maxRetryNum);
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_REPORT_VER_PROCESS;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CONNECT_OTA_SERVER;
				return WIFI_COMM_FAIL;
			}        
			break;
		case AT_REPORT_VER_PROCESS:
			commState = AtCmdHandle(g_httpPostBuf, g_connectServerCmd[AT_REPORT_VER_PROCESS].rsp, 
									g_connectServerCmd[AT_REPORT_VER_PROCESS].timeoutMs, g_connectServerCmd[AT_REPORT_VER_PROCESS].maxRetryNum);
			if (commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CONNECT_OTA_SERVER;
				return WIFI_COMM_OK;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CONNECT_OTA_SERVER;
				return WIFI_COMM_FAIL;
			}			
			break;
		default:
			break;
	}
	return WIFI_COMM_WAIT;
}

/*
{"target":"2.0","tid":1290643,"size":13496,"md5":"0fa04fe1e9f99e1b9c0bc0e46d24ca12","status":1,"type":1},"request_id":"5f1efc170fb44d0582632698c74db0b9"}
*/

static uint32_t g_otaBinSize;
static char g_otaTid[10];
static char g_otaVerStr[10];
static char g_otaVerMd5[40];

#define DOWNLOAD_SPLIT_SIZE   OTA_DOWNLOAD_SPLIT_SIZE   // 分片下载大小512字节，必须小于WIFI_MAX_BUF_SIZE
static uint32_t g_SplitTotalNum;
static uint32_t g_otaDownloadedSize;

static void ParaseFwInfo(char *recvStrBuf)
{		
	char *pos;
	// strstr 定位 key，sscanf �? %[^"] 提取到下一个引号，宽度限制防止越界
	if ((pos = strstr(recvStrBuf, "\"target\":")) != NULL)
	{
		sscanf(pos, "\"target\":\"%9[^\"]", g_otaVerStr);   // g_otaVerStr[10]
	}
	if ((pos = strstr(recvStrBuf, "\"tid\":")) != NULL)
	{
		sscanf(pos, "\"tid\":%9[^,]", g_otaTid);            // tid 为数字，取到逗号�?
	}
	if ((pos = strstr(recvStrBuf, "\"size\":")) != NULL)
	{
		sscanf(pos, "\"size\":%u", &g_otaBinSize);
	}
	if ((pos = strstr(recvStrBuf, "\"md5\":")) != NULL)
	{
		sscanf(pos, "\"md5\":\"%39[^\"]", g_otaVerMd5);     // g_otaVerMd5[40]
	}

	printf("ver: %s, tid: %s, size: %d, md5:%s\r\n",
			g_otaVerStr, g_otaTid, g_otaBinSize, g_otaVerMd5);
	
	/* 计算下载次数 */
	if (g_otaBinSize % DOWNLOAD_SPLIT_SIZE == 0)	// 正好整除
	{
		g_SplitTotalNum = g_otaBinSize / DOWNLOAD_SPLIT_SIZE;
	}
	else
	{
		g_SplitTotalNum = g_otaBinSize / DOWNLOAD_SPLIT_SIZE + 1;
	}	
}

typedef enum 
{
	AT_CHECK_FW_PREPARE = 0,
	AT_CHECK_FW_PROCESS,
	AT_CHECK_FW_FINISH,
	AT_DOWNLOAD_FW_PREPARE,
	AT_DOWNLOAD_FW_PROCESS,
	AT_OTA_FW_FINISH_PRPARE,
	AT_OTA_FW_FINISH_PROCESS,
} AtOtaDownloadCmd_t;

static AtCmdInfo_t g_otaDownloadCmd[] = {
	[AT_CHECK_FW_PREPARE] = {
        .cmd = "AT+CIPSEND=%d\r\n", // 检查是否有新版�? 准备
		.rsp = ">",
		.timeoutMs = 15000,	//5000
		.maxRetryNum = 3,
	},
	[AT_CHECK_FW_PROCESS] = {
		/* 
		检查是否有新版�? 执行
		*/
		.cmd = "GET http://iot-api.heclouds.com/fuse-ota/KPw28SAyeA/board1/check?type=2&version=1.0 HTTP/1.1\r\n"
				"Authorization:version=2018-10-31&res=products%2FKPw28SAyeA%2Fdevices%2Fboard1&et=1800523492&method=md5&sign=NUF4HQG2vJ6iZK281Ugp5g%3D%3D\r\n"
				"Content-Type:application/json\r\n"
				"Host:iot-api.heclouds.com\r\n\r\n",
		.rsp = "+IPD",  // 先判断有没有收到响应
		.timeoutMs = 10000,	
		.maxRetryNum = 0,
	},	
	[AT_CHECK_FW_FINISH] = {
		.cmd = NULL,
		.rsp = "succ",  // 收到响应，再去判断是否有新版本succ，如果没有新版本会对应not exist
		.timeoutMs = 0,	
	},
	[AT_DOWNLOAD_FW_PREPARE] = {
		.cmd = "AT+CIPSEND=%d\r\n",
		.rsp = ">",
		.timeoutMs = 5000,	
	},
	[AT_DOWNLOAD_FW_PROCESS] = {
        .cmd = "GET http://iot-api.heclouds.com/fuse-ota/KPw28SAyeA/board1/%s/download HTTP/1.1\r\n"
				"Authorization:version=2018-10-31&res=products%%2FKPw28SAyeA%%2Fdevices%%2Fboard1&et=1800523492&method=md5&sign=NUF4HQG2vJ6iZK281Ugp5g%%3D%%3D\r\n"
				// version=2018-10-31&res=products%2F7V45i9XgPY%2Fdevices%2Fboard1&et=1800501704&method=md5&sign=LeUmhaQFAf8EB0d784Npfw%3D%3D  ,因为下面代码要使用sprintf会将这里�?%按照格式化符合处理，所以需要再加上%就是实际的字�?%�?
				"Range:bytes=%d-%d\r\n"
				"Host:iot-api.heclouds.com\r\n\r\n",
		.rsp = "Ota-Errno: 0",
		.timeoutMs = 10000,	
	},
	[AT_OTA_FW_FINISH_PRPARE] = {
		.cmd = "AT+CIPSEND=%d\r\n",
		.rsp = ">",
		.timeoutMs = 5000,	
	},
	[AT_OTA_FW_FINISH_PROCESS] = {
        .cmd = "POST http://iot-api.heclouds.com/fuse-ota/KPw28SAyeA/board1/%s/status HTTP/1.1\r\n"
				"Authorization:version=2018-10-31&res=products%%2FKPw28SAyeA%%2Fdevices%%2Fboard1&et=1800523492&method=md5&sign=NUF4HQG2vJ6iZK281Ugp5g%%3D%%3D\r\n"
				"Content-Type:application/json\r\n"
				"Host:iot-api.heclouds.com\r\n"
				"Content-Length:12\r\n\r\n"
				"{\"step\":201}\r\n",
		.rsp = "succ",
		.timeoutMs = 10000,	
	},
};

/**
***********************************************************
* @brief 计算并校�? MD5，与云端下发�? MD5 字符串比�?
* @param md5Str,输入，云端下发的 MD5 字符�?
* @return true: 校验通过；false: 校验失败
***********************************************************
*/
static bool VerifyFwMd5(char *md5Str)
{
	if (md5Str == NULL)
	{
		return false;
	}

	uint8_t bin[16];
	char    ascii[33] = {0};  // 局部变量，每次调用自动清零，修复重试时追加拼接�? bug
	char    tmp[3];

	MD5Final(&g_md5c, bin);
	for (uint8_t i = 0; i < 16; i++)
	{
		sprintf(tmp, "%02x", bin[i]);
		strcat(ascii, tmp);
	}
	return strcmp(ascii, md5Str) == 0;
}

static uint32_t CaculateDownloadSplitSize(uint32_t splitCurrNum, uint32_t splitTotalNum)
{
	if (splitCurrNum < splitTotalNum)  // 即将下载的不是最后一�?
	{
		return DOWNLOAD_SPLIT_SIZE;  // 第一种情况，512字节
	}
	else if (splitCurrNum == splitTotalNum) // 即将下载是最后一�?
	{
		/* 计算下载次数 */
		if (g_otaBinSize % DOWNLOAD_SPLIT_SIZE == 0)	// 正好整除
		{	
			return DOWNLOAD_SPLIT_SIZE;
		}
		else
		{
			return g_otaBinSize % DOWNLOAD_SPLIT_SIZE; // 第二种，最后一包，不足512字节
		}				
	}
	return 0;   // 第三种，已经下载完成，返�?0
}
#define   CHECK_VERSION_PERIOD     OTA_CHECK_VERSION_PERIOD_MS    // 查询版本周期，单位ms

static bool RebuildMd5FromFlash(uint32_t size)
{
	uint8_t flashBuf[128];
	uint32_t offset = 0;
	uint32_t readLen;

	MD5Init(&g_md5c);
	while (offset < size)
	{
		readLen = size - offset;
		if (readLen > sizeof(flashBuf))
		{
			readLen = sizeof(flashBuf);
		}
		if (!FlashRead(DOWNLOAD_AREA_ADDR_IN_FLASH + offset, flashBuf, readLen))
		{
			return false;
		}
		MD5Update(&g_md5c, flashBuf, readLen);
		offset += readLen;
	}
	return true;
}

static void FillOtaProgress(OtaProgress_t *progress, uint32_t downloadedSize, OtaProgressState_t state)
{
	memset(progress, 0, sizeof(OtaProgress_t));
	progress->state = state;
	progress->fwSize = g_otaBinSize;
	progress->downloadedSize = downloadedSize;
	strncpy(progress->tid, g_otaTid, sizeof(progress->tid) - 1);
	strncpy(progress->version, g_otaVerStr, sizeof(progress->version) - 1);
	strncpy(progress->md5, g_otaVerMd5, sizeof(progress->md5) - 1);
}

static bool SaveOtaProgress(uint32_t downloadedSize, OtaProgressState_t state)
{
	OtaProgress_t progress;

	FillOtaProgress(&progress, downloadedSize, state);
	return OtaProgressSave(&progress);
}

static bool PrepareOtaDownload(void)
{
	OtaProgress_t progress;

	if (g_otaBinSize == 0 || g_otaBinSize > OTA_FIRMWARE_MAX_SIZE)
	{
		APP_LOGE("ota size invalid:%u", g_otaBinSize);
		return false;
	}

	if (OtaProgressLoad(&progress) &&
		OtaProgressMatch(&progress, g_otaBinSize, g_otaTid, g_otaVerStr, g_otaVerMd5) &&
		progress.state == OTA_PROGRESS_DOWNLOADING)
	{
		g_otaDownloadedSize = progress.downloadedSize;
		if (!RebuildMd5FromFlash(g_otaDownloadedSize))
		{
			return false;
		}
		APP_LOGI("ota resume from:%u", g_otaDownloadedSize);
		return true;
	}

	OtaProgressInvalidate();
	g_otaDownloadedSize = 0;
	MD5Init(&g_md5c);
	if (!FlashErase(DOWNLOAD_AREA_ADDR_IN_FLASH, OTA_FIRMWARE_MAX_SIZE))
	{
		return false;
	}
	return SaveOtaProgress(0, OTA_PROGRESS_DOWNLOADING);
}

static bool ParseDownloadBody(uint8_t **body, uint32_t *bodyLen, uint32_t expectedStart, uint32_t expectedLen)
{
	HttpResponse_t rsp;

	if (!HttpParseResponse((uint8_t *)g_recvStrBuf, g_pktRcvdLen, &rsp))
	{
		APP_LOGE("http parse failed");
		return false;
	}
	if (!HttpValidateRangeResponse(&rsp, expectedStart, expectedLen))
	{
		APP_LOGE("http range invalid,status:%u,start:%u", rsp.statusCode, rsp.rangeStart);
		return false;
	}
	if (rsp.bodyLengthInPacket < expectedLen)
	{
		APP_LOGE("http body short:%u/%u", rsp.bodyLengthInPacket, expectedLen);
		return false;
	}

	*body = (uint8_t *)&g_recvStrBuf[rsp.bodyOffset];
	*bodyLen = expectedLen;
	return true;
}

#define INIT_DOWNLOAD_PROGRESS() \
	do { \
		s_splitCurrNum = 0; \
		s_splitDownloadSize = DOWNLOAD_SPLIT_SIZE; \
	} while (0)

static WifiCommState_t OtaCheckAndDownloadFw(void)
{
	static WifiCommState_t s_commState = WIFI_COMM_OK;
	static AtOtaDownloadCmd_t s_cmdType = AT_CHECK_FW_PREPARE;
	char cmdStrBuf[256] = {0};
	static uint64_t s_lastVersionSysTime = 0;
	static uint32_t s_splitCurrNum = 0;  // 保存即将第几次下�?
	static uint32_t s_splitDownloadSize = DOWNLOAD_SPLIT_SIZE;  // 本次分片下载的字节数

	switch (s_cmdType)
	{
		case AT_CHECK_FW_PREPARE:
			if (s_commState != WIFI_COMM_WAIT)   // 必须是静态的
			{
				if ((GetSysRunTime() - s_lastVersionSysTime) < CHECK_VERSION_PERIOD)
				{
					s_cmdType = AT_CHECK_FW_PREPARE;
					return WIFI_COMM_OK;			
				}
				else 
				{
					s_lastVersionSysTime = GetSysRunTime();
					sprintf(cmdStrBuf, g_otaDownloadCmd[AT_CHECK_FW_PREPARE].cmd, strlen(g_otaDownloadCmd[AT_CHECK_FW_PROCESS].cmd));   // "AT+CIPSEND=111\r\n"
				}
			}
			s_commState = AtCmdHandle(cmdStrBuf, g_otaDownloadCmd[AT_CHECK_FW_PREPARE].rsp, 
									g_otaDownloadCmd[AT_CHECK_FW_PREPARE].timeoutMs, g_otaDownloadCmd[AT_CHECK_FW_PREPARE].maxRetryNum);
			if (s_commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CHECK_FW_PROCESS;
			}
			if (s_commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CHECK_FW_PREPARE;
			}	
			break;	
		case AT_CHECK_FW_PROCESS:
			s_commState = AtCmdHandle(g_otaDownloadCmd[AT_CHECK_FW_PROCESS].cmd, g_otaDownloadCmd[AT_CHECK_FW_PROCESS].rsp, 
									g_otaDownloadCmd[AT_CHECK_FW_PROCESS].timeoutMs, g_otaDownloadCmd[AT_CHECK_FW_PROCESS].maxRetryNum);
			if (s_commState == WIFI_COMM_OK) // 收到响应数据
			{
				s_cmdType = AT_CHECK_FW_FINISH; // 收到响应数据，确认是否有新版�?
			}
			if (s_commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CHECK_FW_PREPARE;
				return WIFI_COMM_FAIL;
			}
			break;
		case AT_CHECK_FW_FINISH:
			if (strstr(g_recvStrBuf, g_otaDownloadCmd[AT_CHECK_FW_FINISH].rsp) != NULL)
			{
				ParaseFwInfo(g_recvStrBuf);  // 检测到新版本，解析版本信息
				if (!PrepareOtaDownload())
				{
					ClearRecvWifiStr();
					s_cmdType = AT_CHECK_FW_PREPARE;
					return WIFI_COMM_FAIL;
				}
				ClearRecvWifiStr();
				s_cmdType = AT_DOWNLOAD_FW_PREPARE;
			}
			else // 没有新版本，not exist，回到检查版本的case，继续检查版�?
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CHECK_FW_PREPARE;
				return WIFI_COMM_OK;  // 没有新版本，返回OK，可以执行其他上报传感器数据任务
			}
			break;
		case AT_DOWNLOAD_FW_PREPARE:
			if (s_commState != WIFI_COMM_WAIT)
			{
				uint32_t rangeStart = 0;
				uint32_t rangeEnd = 0;
				s_splitCurrNum++;
				s_splitDownloadSize = CaculateDownloadSplitSize(s_splitCurrNum, g_SplitTotalNum);
				if (s_splitDownloadSize == 0)  // 下载完成
				{
					printf("\r\n#########OTA_DownLoad finish!#########\r\n");
					
					if (VerifyFwMd5(g_otaVerMd5))
					{
						printf("\r\nmd5 check success!!!\r\n");
						INIT_DOWNLOAD_PROGRESS();
						s_cmdType = AT_OTA_FW_FINISH_PRPARE;  // 准备上报完成
						break;
					}
					else
					{
						s_cmdType = AT_CHECK_FW_PREPARE;
						INIT_DOWNLOAD_PROGRESS();
						return WIFI_COMM_FAIL;               // 校验失败，返回FAIL，可以从AT+RST重新执行
					}
				}
				rangeStart = (s_splitCurrNum - 1) * DOWNLOAD_SPLIT_SIZE; // 0 512 1024
				rangeEnd = rangeStart + s_splitDownloadSize - 1; // 511 1023 1039,最后一包可能不�?512
				memset(g_httpPostBuf, 0, sizeof(g_httpPostBuf));
				sprintf(g_httpPostBuf, g_otaDownloadCmd[AT_DOWNLOAD_FW_PROCESS].cmd, g_otaTid, rangeStart, rangeEnd);  // GET�? 
			}
		
			sprintf(cmdStrBuf, g_otaDownloadCmd[AT_DOWNLOAD_FW_PREPARE].cmd, strlen(g_httpPostBuf));  // "AT+CIPSEND=256\r\n"
			s_commState = AtCmdHandle(cmdStrBuf, g_otaDownloadCmd[AT_DOWNLOAD_FW_PREPARE].rsp, 
									g_otaDownloadCmd[AT_DOWNLOAD_FW_PREPARE].timeoutMs, g_otaDownloadCmd[AT_DOWNLOAD_FW_PREPARE].maxRetryNum);
			if (s_commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_DOWNLOAD_FW_PROCESS;
			}
			if (s_commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				INIT_DOWNLOAD_PROGRESS();
				s_cmdType = AT_CHECK_FW_PREPARE;
				return WIFI_COMM_FAIL;
			}
			break;	
		case AT_DOWNLOAD_FW_PROCESS:
			s_commState = AtCmdHandle(g_httpPostBuf, g_otaDownloadCmd[AT_DOWNLOAD_FW_PROCESS].rsp, 
									g_otaDownloadCmd[AT_DOWNLOAD_FW_PROCESS].timeoutMs, g_otaDownloadCmd[AT_DOWNLOAD_FW_PROCESS].maxRetryNum);		
			if (s_commState == WIFI_COMM_OK)
			{
				printf("g_pktRcvdLen = %d\n", g_pktRcvdLen);
				
				FlashWrite(DOWNLOAD_AREA_ADDR_IN_FLASH + (s_splitCurrNum - 1) * DOWNLOAD_SPLIT_SIZE, (uint8_t *)&g_recvStrBuf[g_pktRcvdLen - s_splitDownloadSize - 2], s_splitDownloadSize); // 最后有2个字�?0D0A
				MD5Update(&g_md5c, (unsigned char *)&g_recvStrBuf[g_pktRcvdLen - s_splitDownloadSize - 2], s_splitDownloadSize); // 每次分包下载BIN文件，都要更新MD5数据
				
				ClearRecvWifiStr();
				s_cmdType = AT_DOWNLOAD_FW_PREPARE;
			}
			if (s_commState == WIFI_COMM_FAIL)
			{
				s_cmdType = AT_CHECK_FW_PREPARE;
				INIT_DOWNLOAD_PROGRESS();
				return WIFI_COMM_FAIL;
			}
			break;
		case AT_OTA_FW_FINISH_PRPARE:
			memset(g_httpPostBuf, 0, sizeof(g_httpPostBuf));
			sprintf(g_httpPostBuf, g_otaDownloadCmd[AT_OTA_FW_FINISH_PROCESS].cmd, g_otaTid); // POST�?
		
			sprintf(cmdStrBuf, g_otaDownloadCmd[AT_OTA_FW_FINISH_PRPARE].cmd, strlen(g_httpPostBuf)); // "AT+CIPSEND=298\r\n"
			s_commState = AtCmdHandle(cmdStrBuf, g_otaDownloadCmd[AT_OTA_FW_FINISH_PRPARE].rsp, 
									g_otaDownloadCmd[AT_OTA_FW_FINISH_PRPARE].timeoutMs, g_otaDownloadCmd[AT_OTA_FW_FINISH_PRPARE].maxRetryNum);
			if (s_commState == WIFI_COMM_OK)
			{	
				ClearRecvWifiStr();
				s_cmdType = AT_OTA_FW_FINISH_PROCESS;
			}
			if (s_commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CHECK_FW_PREPARE;
				return WIFI_COMM_FAIL;
			}
			break;	
		case AT_OTA_FW_FINISH_PROCESS:
			s_commState = AtCmdHandle(g_httpPostBuf, g_otaDownloadCmd[AT_OTA_FW_FINISH_PROCESS].rsp, 
									g_otaDownloadCmd[AT_OTA_FW_FINISH_PROCESS].timeoutMs, g_otaDownloadCmd[AT_OTA_FW_FINISH_PROCESS].maxRetryNum);	
			if (s_commState == WIFI_COMM_OK)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CHECK_FW_PREPARE;
				/* 新固件已完整存入 BACKUP 区并校验通过、状态已上报云端�?
				   写入升级信息(此时 flag=0，仅记录固件信息，尚未确认升�?)�?
				   真正的升�? flag 推迟到用户按键时�? SetUpdateFlag 置位 */
				WriteUpdateInfo(g_otaBinSize, g_otaVerMd5, g_otaVerStr);
				return WIFI_COMM_OK;
			}
			if (s_commState == WIFI_COMM_FAIL)
			{
				ClearRecvWifiStr();
				s_cmdType = AT_CHECK_FW_PREPARE;
				return WIFI_COMM_FAIL;
			}
			break;	
		default:
			break;
	}
	return WIFI_COMM_WAIT;
}

typedef enum
{
	INIT_WIFI_MODULE,
	CONNECT_WIFI_HOTSPOT,
	CONNECT_IOT_SERVER,
	OTA_CHECK_AND_DOWNLOAD_VERSION,
	HWRESET_WIFI_MODULE,
	WIFI_MODULE_ERROR,
} WifiWorkState_t;

void WifiNetworkTask(void)
{
	WifiCommState_t commState;
	static WifiWorkState_t s_workState = INIT_WIFI_MODULE;
	static uint8_t s_hwresetCnt = 0;
	
	switch (s_workState)
	{
		case INIT_WIFI_MODULE:
			commState = InitWifiModule();
			if (commState == WIFI_COMM_OK)
			{
				s_hwresetCnt = 0;
				s_workState = CONNECT_WIFI_HOTSPOT;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				s_workState = HWRESET_WIFI_MODULE;
			}
			break;
		case CONNECT_WIFI_HOTSPOT:
			commState = ConnectWifiHotspot();
			if (commState == WIFI_COMM_OK)
			{
				s_workState = CONNECT_IOT_SERVER;
			}
			else if (commState == WIFI_COMM_FAIL)
			{
				s_workState = INIT_WIFI_MODULE;
			}
			break;
		case CONNECT_IOT_SERVER:
			commState = ConnectIotServer();
			if (commState == WIFI_COMM_OK)
			{
				s_workState = OTA_CHECK_AND_DOWNLOAD_VERSION;
			}
			else if (commState == WIFI_COMM_FAIL)
			{
				s_workState = INIT_WIFI_MODULE;
			}
			break;
		case OTA_CHECK_AND_DOWNLOAD_VERSION:
			commState = OtaCheckAndDownloadFw();
			if (commState == WIFI_COMM_OK)
			{
				s_workState = OTA_CHECK_AND_DOWNLOAD_VERSION;
			}
			if (commState == WIFI_COMM_FAIL)
			{
				s_workState = INIT_WIFI_MODULE;
			}		
			break;
		case HWRESET_WIFI_MODULE:
			if (s_hwresetCnt < 1)                 // 如果AT命令不通，硬件复位1�?
			{
				DisableWifiModule();
				commState = AtCmdHandle(NULL, "deadbeef", 500, 0);  // 只是为了非阻塞等�?
				if (commState == WIFI_COMM_FAIL)  // 非阻塞延时到了，肯定返回FAIL
				{
					s_workState = INIT_WIFI_MODULE;
					s_hwresetCnt++;
					EnableWifiModule();
				}
			}
			else
			{
				printf("wifi module error!\n");
				s_workState = WIFI_MODULE_ERROR;  // 如果硬件复位1次，AT命令还是不通，就不再执行WIFI任务的业务逻辑，直接退出，避免影响其他任务
			}
			break;
		case WIFI_MODULE_ERROR:
			break;
		default:
			break;	
	}
}

