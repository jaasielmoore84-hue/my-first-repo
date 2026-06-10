#include <stdio.h>
#include "led_drv.h"
#include "delay.h"
#include "usb2com_drv.h"
#include "systick.h"
#include "key_drv.h"
#include "wifi_drv.h"
#include "wifi_app.h"
#include "ntc_drv.h"
#include "rh_drv.h"
#include "rtc_drv.h"
#include "iap_drv.h"
#include "store_app.h"
#include "app_config.h"
#include "ota_rtos.h"

static void DrvInit(void)
{
	InitIrqAfterBoot();
	DelayInit();
	LedDrvInit();
	KeyDrvInit();
	SystickInit();
	Usb2ComDrvInit();
	WifiDrvInit();
	TempDrvInit();
	HumiDrvInit();
	RtcDrvInit();
}

static void AppInit(void)
{
	AppConfigInit();
	InitSysParam();
	OtaRtosInit();
	OtaRtosPostCommand(OTA_CMD_START);
}

int main(void)
{	
	DrvInit();
	AppInit();

	uint8_t keyVal = 0;
	printf("\n******************111*******************\n");
	printf("************This is App Demo************\n");
	printf("************This is App Demo************\n");
	printf("************This is App Demo************\n");
	printf("****************************************\n");
	while (1)
	{
		OtaRtosNetworkTask();
		keyVal = GetKeyVal();
		if (keyVal == KEY1_LONG_PRESS)
		{
			if (SetUpdateFlag())   // 按键确认：标记区有有效固件信息时才置 flag 并重�? CRC，再复位�? Boot
			{
				printf("************reset to bootloader************\n");
				ResetToBoot();
			}
			else
			{
				printf("************no valid firmware, ignore************\n");
			}
		}
	}
}
