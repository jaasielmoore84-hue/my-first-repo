#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "gd32f30x.h"
#include "store_app.h"
#include "md5.h"

static SysParam_t g_sysParamCurrent;  // 当前系统参数，始终保持最新

static SysParam_t g_sysParamDefault = 
{
	.magicCode = STORE_MAGIC_CODE,
	.softwareVersion = "1.0",
};

static uint8_t CalcCrc8(uint8_t *buf, uint32_t len)
{
    uint8_t crc = 0xFF;

    for (uint8_t byte = 0; byte < len; byte++)
    {
        crc ^= (buf[byte]);
        for (uint8_t i = 8; i > 0; --i)
        {
			if (crc & 0x80)
			{
				crc = (crc << 1) ^ 0x31;
			}
			else 
			{	
				crc = (crc<<1);
			}
		}
    }
    return crc;
}

static bool ReadSysParam(SysParam_t *sysParam)
{
	if (!FlashRead(PARAMETER_AREA_ADDR_IN_FLASH, (uint8_t *)sysParam, sizeof(SysParam_t)))
	{
		return false;
	}

	if (sysParam->magicCode != STORE_MAGIC_CODE) // 判断有没有参数，没有则返回false
	{
		return false;
	}
	uint8_t crcVal = CalcCrc8((uint8_t *)sysParam, sizeof(SysParam_t) - 1); // 将crcVal成员前面的数据做CRC运算
	if (crcVal != sysParam->crcVal)
	{
		return false;
	}
	return true;
}

static bool WriteSysParam(SysParam_t *sysParam)
{
	uint16_t sysParamLen = sizeof(SysParam_t);
	if (sysParamLen > SYSPARAM_MAX_SIZE)  // 判断有没有超过2048个字节
	{
		return false;
	}
	
	sysParam->crcVal = CalcCrc8((uint8_t *)sysParam, sizeof(SysParam_t) - 1);
	if (!FlashErase(PARAMETER_AREA_ADDR_IN_FLASH, sizeof(SysParam_t)))
	{
		return false;
	}
	if (!FlashWrite(PARAMETER_AREA_ADDR_IN_FLASH, (uint8_t *)sysParam, sizeof(SysParam_t)))
	{
		return false;
	}	
	return true;
}

bool SetSoftwareVersionParam(char *version)
{
	if (version == NULL)
	{
		return false;
	}
	if (strcmp(g_sysParamCurrent.softwareVersion, version) == 0)
	{
		return true;  // 版本号未变，无需写入
	}
//	memset(g_sysParamCurrent.softwareVersion, 0, sizeof(g_sysParamCurrent.softwareVersion));
//	strcpy(g_sysParamCurrent.softwareVersion, version);
//	WriteSysParam(&g_sysParamCurrent);
	
	SysParam_t sysParm = g_sysParamCurrent;    // 防止写入失败，不影响原有的参数数据
	memset(sysParm.softwareVersion, 0, sizeof(sysParm.softwareVersion));
	strcpy(sysParm.softwareVersion, version);
	if (!WriteSysParam(&sysParm))
	{
		return false;
	}
	g_sysParamCurrent = sysParm;  // 写入成功了再重新赋值给静态全局变量，保持最新
	return true;
}

void GetSoftwareVersionParam(char *version)
{
	if (version == NULL)
	{
		return;
	}
	strcpy(version, g_sysParamCurrent.softwareVersion);
}

void InitSysParam(void)
{	
	SysParam_t sysParam; // 不直接读取到g_sysParamCurrent
	
	if (ReadSysParam(&sysParam))
	{
		g_sysParamCurrent = sysParam; // 结构体可以整体赋值
	}
	else
	{
		g_sysParamCurrent = g_sysParamDefault;
	}
}

/**
***********************************************************
* @brief  App 侧调用：把待升级信息写入 UPDATE_INFO 区
* @param  fwSize,  固件字节数(g_otaBinSize)
* @param  md5,     云端下发的 MD5 字符串(g_otaVerMd5)
* @param  version, 目标版本号(g_otaVerStr)
* @return true:写入成功
* @note   与 PARAMETER 区分页隔离，写标记不影响产品参数
***********************************************************
*/
bool WriteUpdateInfo(uint32_t fwSize, const char *md5, const char *version)
{
	if (md5 == NULL || version == NULL)
	{
		return false;
	}

	UpdateInfo_t updateInfo;
	memset(&updateInfo, 0, sizeof(UpdateInfo_t));
	updateInfo.magicCode = STORE_MAGIC_CODE;
	updateInfo.needUpdateFlag = 0;     // 仅下载完成,尚未经用户确认,不升级
	updateInfo.fwSize    = fwSize;
	strncpy(updateInfo.fwMd5, md5, sizeof(updateInfo.fwMd5) - 1);
	strncpy(updateInfo.fwVersion, version, sizeof(updateInfo.fwVersion) - 1);
	updateInfo.crcVal = CalcCrc8((uint8_t *)&updateInfo, sizeof(UpdateInfo_t) - 1);

	if (!FlashErase(UPDATE_INFO_AREA_ADDR_IN_FLASH, sizeof(UpdateInfo_t)))
	{
		return false;
	}
	if (!FlashWrite(UPDATE_INFO_AREA_ADDR_IN_FLASH, (uint8_t *)&updateInfo, sizeof(UpdateInfo_t)))
	{
		return false;
	}
	return true;
}

/**
***********************************************************
* @brief  Bootloader 侧调用：读取并校验升级信息
* @param  updateInfo,输出，读到的升级信息
* @return true:有效且 magic/CRC 均正确(确实需要升级)
***********************************************************
*/
bool ReadUpdateInfo(UpdateInfo_t *updateInfo)
{
	if (updateInfo == NULL)
	{
		return false;
	}
	if (!FlashRead(UPDATE_INFO_AREA_ADDR_IN_FLASH, (uint8_t *)updateInfo, sizeof(UpdateInfo_t)))
	{
		return false;
	}
	if (updateInfo->magicCode != STORE_MAGIC_CODE)
	{
		return false;
	}
	uint8_t crcVal = CalcCrc8((uint8_t *)updateInfo, sizeof(UpdateInfo_t) - 1);
	if (crcVal != updateInfo->crcVal)
	{
		return false;
	}
	return true;
}

/**
***********************************************************
* @brief  Bootloader 侧调用：升级成功后清除标记，避免重复升级
***********************************************************
*/
void ClearUpdateInfo(void)
{
	FlashErase(UPDATE_INFO_AREA_ADDR_IN_FLASH, sizeof(UpdateInfo_t));
}

/**
***********************************************************
* @brief  Bootloader 侧：判断是否需要升级
* @param  updateInfo,输入，读到的升级信息
* @return true:区域数据有效(magic+CRC) 且 flag 已被用户置位
* @note   magic+CRC 仅说明"有合法固件信息"，flag 才代表"用户确认要升级"
***********************************************************
*/
bool CheckNeedUpdate(UpdateInfo_t *updateInfo)
{
	if (updateInfo == NULL)
	{
		return false;
	}
	if (updateInfo->needUpdateFlag != UPDATE_FLAG_SET)   // 用户未确认升级
	{
		return false;
	}
	if (updateInfo->fwSize == 0 || updateInfo->fwSize > APP_AREA_SIZE)
	{
		return false;
	}
	return true;
}

/**
***********************************************************
* @brief  把 16 字节 MD5 摘要转成 32 字符小写十六进制字符串
*         并与云端下发的 MD5 字符串比较
* @param  digest,输入，MD5Final 得到的 16 字节摘要
* @param  md5Str,输入，云端下发的 MD5 字符串(32字符)
* @return true:一致
***********************************************************
*/
static bool Md5DigestMatch(uint8_t *digest, char *md5Str)
{
	char ascii[33] = {0};
	for (uint8_t i = 0; i < 16; i++)
	{
		sprintf(&ascii[i * 2], "%02x", digest[i]);
	}
	return strcmp(ascii, md5Str) == 0;
}

#define UPDATE_COPY_BUF_SIZE   512   // 搬运用的 RAM 缓冲区大小

/**
***********************************************************
* @brief  Bootloader 侧核心：把下载区固件搬运到 APP 区并校验
* @param  updateInfo,输入，读到的升级信息
* @return true:搬运+MD5校验均成功(已写回版本号并清除标记)
* @note   1) 先擦 APP 区，再分块 下载区→RAM→APP 搬运，边搬边算 MD5
*         2) 校验失败不清标记，下次开机重试(固件仍在下载区，安全)
***********************************************************
*/
bool UpdateAppFromDownloadArea(UpdateInfo_t *updateInfo)
{
	if (updateInfo == NULL)
	{
		return false;
	}
	printf("updating: ver=%s, size=%u, md5=%s\n",
			updateInfo->fwVersion, updateInfo->fwSize, updateInfo->fwMd5);

	/* 1. 擦除 APP 区(按固件大小覆盖到页) ，此时APP区已经没有老固件了*/
	FlashErase(APP_AREA_ADDR_IN_FLASH, updateInfo->fwSize);

	/* 2. 分块搬运 下载区→APP，边搬边算 MD5
	   ，避免 512B 大数组压栈导致栈溢出踩坏 g_sysParamCurrent,把栈空间调大 */
	MD5_CTX md5Ctx;
	uint8_t copyBuf[UPDATE_COPY_BUF_SIZE];
	MD5Init(&md5Ctx);
	for (uint32_t offset = 0; offset < updateInfo->fwSize; offset += UPDATE_COPY_BUF_SIZE)
	{
		uint32_t remain = updateInfo->fwSize - offset;
		uint32_t copyLen = (remain > UPDATE_COPY_BUF_SIZE) ? UPDATE_COPY_BUF_SIZE : remain;

		if (!FlashRead(DOWNLOAD_AREA_ADDR_IN_FLASH + offset, copyBuf, copyLen))
		{
			return false;
		}

		if (!FlashWrite(APP_AREA_ADDR_IN_FLASH + offset, copyBuf, copyLen))
		{
			return false;
		}
		MD5Update(&md5Ctx, copyBuf, copyLen);
	}

	/* 3. 校验搬运后 APP 区固件的 MD5 */
	uint8_t digest[16];
	MD5Final(&md5Ctx, digest);
	if (!Md5DigestMatch(digest, updateInfo->fwMd5))
	{
		printf("update md5 check fail!\n");
		return false;   // 不清标记，下次开机重试
	}

	/* 调试：回读 PARAMETER 区，打印与 App 端 ReadSysParam 相同的字段以便对比 */
	// SysParam_t chk;
	// FlashRead(PARAMETER_AREA_ADDR_IN_FLASH, (uint8_t *)&chk, sizeof(SysParam_t));
	// uint8_t crcCalc = CalcCrc8((uint8_t *)&chk, sizeof(SysParam_t) - 1);
	// printf("PARAM readback: sizeof=%d, magic=%08X, ver=%s, crcCalc=%02X, crcStored=%02X\n",
	// 		(int)sizeof(SysParam_t), chk.magicCode, chk.softwareVersion, crcCalc, chk.crcVal);

	printf("update md5 check success!\n");
	return true;
}
