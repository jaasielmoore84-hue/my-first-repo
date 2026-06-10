#include "gd32f30x.h"
#include "store_app.h"
#include "inflash_drv.h"

void InitIrqAfterBoot(void)
{
	__enable_irq();
}

void ResetToBoot(void)
{
	__disable_irq();    // 关闭所有中断
	NVIC_SystemReset(); // 复位函数，需要一些执行的时间,所以先关闭中断，不再执行中断函数
}
