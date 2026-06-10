#include "gd32f30x.h"
#include "inflash_drv.h"

typedef void (*pFunction)(void);

#define RAM_START_ADDRESS   0x20000000
#define RAM_SIZE            0x10000
void BootToApp(void)
{
	uint32_t resetHandlerAddr = *(uint32_t*) (APP_AREA_ADDR_IN_FLASH + 4);
	uint32_t stackTopAddr = *(uint32_t*)APP_AREA_ADDR_IN_FLASH; 

	if (stackTopAddr > RAM_START_ADDRESS && stackTopAddr < (RAM_START_ADDRESS + RAM_SIZE)) //判断栈顶地址是否在合法范围内
	{
		__disable_irq();              // 关闭内核NVIC中总中断
			
		__set_MSP(stackTopAddr);
		nvic_vector_table_set(NVIC_VECTTAB_FLASH, APP_AREA_ADDR_IN_FLASH - NVIC_VECTTAB_FLASH);  // 重新配置APP的中断向量表
		((pFunction) resetHandlerAddr)();
	}
}
