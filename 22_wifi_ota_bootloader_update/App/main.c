#include <stdio.h>
#include "led_drv.h"
#include "delay.h"
#include "usb2com_drv.h"
#include "iap_drv.h"
#include "store_app.h"

static void DrvInit(void)
{
	LedDrvInit();
	DelayInit();
	Usb2ComDrvInit();
}

static void AppInit(void)
{
	InitSysParam();
}

int main(void)
{	
	DrvInit();
	AppInit();
	printf("*****This is bootloader*****\n");

	UpdateInfo_t updateInfo;

	if (ReadUpdateInfo(&updateInfo) && CheckNeedUpdate(&updateInfo)) // 确认下载区已有校验通过的新固件，才允许进 Boot 升级
	{
		//BackupAppToRollBack();
		if (UpdateAppFromDownloadArea(&updateInfo)) // 将下载区固件搬运到 APP 区并校验，执行完 APP 区已经没有老固件了
		{
			printf("*********update success***********\n");
			/* 更新产品参数版本号 */
			SetSoftwareVersionParam(updateInfo.fwVersion);
		}
		else
		{
			printf("*********update failed***********\n");
		}
		/* 清除升级标记 */
		ClearUpdateInfo();
	}

	printf("*********boot to app***********\n"); 
	BootToApp();

	while(1);
}
