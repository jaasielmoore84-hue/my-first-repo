#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "store_app.h"

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
	/* 调试：打印读到的关键字段，定位 App 为何回退默认版本 */
	uint8_t crcCalc = CalcCrc8((uint8_t *)sysParam, sizeof(SysParam_t) - 1);
	printf("ReadSysParam: sizeof=%d, magic=0x%08X(expect 0x%08X), ver=%s, crcCalc=0x%02X, crcStored=0x%02X\n",
			(int)sizeof(SysParam_t), sysParam->magicCode, (uint32_t)STORE_MAGIC_CODE,
			sysParam->softwareVersion, crcCalc, sysParam->crcVal);

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
* @brief  按键确认升级：先判断标记区是否已有有效固件信息，
*         有则把 flag 置位并重算整个区域的 CRC 后写回
* @return true:已成功置位(可复位进 Boot)；false:无有效固件信息
***********************************************************
*/
bool SetUpdateFlag(void)
{
	UpdateInfo_t updateInfo;
	if (!ReadUpdateInfo(&updateInfo))   // 先确认区域有有效数据(magic + CRC 通过)
	{
		return false;
	}
	updateInfo.needUpdateFlag = UPDATE_FLAG_SET;  // 用户确认升级
	updateInfo.crcVal = CalcCrc8((uint8_t *)&updateInfo, sizeof(UpdateInfo_t) - 1);  // 重算整个区域 CRC

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
* @brief  Bootloader 侧调用：升级成功后清除标记，避免重复升级
***********************************************************
*/
void ClearUpdateInfo(void)
{
	FlashErase(UPDATE_INFO_AREA_ADDR_IN_FLASH, sizeof(UpdateInfo_t));
}
