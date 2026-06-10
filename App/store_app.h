#ifndef _STORE_APP_H_
#define _STORE_APP_H_

#include <stdint.h>
#include <stdbool.h>
#include "inflash_drv.h"

#define SYSPARAM_MAX_SIZE                    FLASH_PAGE_SIZE
#define STORE_MAGIC_CODE                     (0xdeadbeef)

#pragma pack(1)     // 按照1字节对齐
typedef struct
{
	uint32_t magicCode;        // 用来证明FLASH读取的数据有没有参数
	/* 添加配置参数开始 */
	char softwareVersion[10];
	/* 添加配置参数结束 */
	uint8_t crcVal;            // 用来证明FLASH的参数数据对不对
} SysParam_t;	
#pragma pack()

#define UPDATE_FLAG_SET                      1            // flag=1: 用户已确认升级

#pragma pack(1)     // 按照1字节对齐
typedef struct
{
	uint32_t magicCode;        // MAGIC_CODE(0xdeadbeef)=该区域有有效数据(仅标识有无数据)
	uint8_t  needUpdateFlag;   // 升级标志: 0=已下载未确认; UPDATE_FLAG_SET(1)=用户已确认,执行升级
	uint32_t fwSize;           // 固件字节数，Bootloader 据此从下载区搬到 APP 区
	char     fwMd5[33];        // 云端下发的 MD5 字符串(32字符+'\0')，搬运后整体校验
	char     fwVersion[10];    // 目标版本号，升级成功后写回 PARAMETER 区
	uint8_t  crcVal;           // 结构体自身 CRC8，证明标记区数据没写坏
} UpdateInfo_t;
#pragma pack()

void InitSysParam(void);
void GetSoftwareVersionParam(char *version);
bool SetSoftwareVersionParam(char *version);

/* 升级标记区接口 */
bool WriteUpdateInfo(uint32_t fwSize, const char *md5, const char *version); // 下载完成写入信息(flag=0)
bool SetUpdateFlag(void);                                                    // 按键确认,置flag并重算CRC
bool ReadUpdateInfo(UpdateInfo_t *updateInfo);
void ClearUpdateInfo(void);

#endif
