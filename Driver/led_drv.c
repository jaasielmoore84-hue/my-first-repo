#include <stdint.h>
#include "gd32f30x.h"
#include "led_drv.h"

typedef struct 
{
    rcu_periph_enum rcu;
    uint32_t port;
    uint32_t pin;
} LedHwCfg_t;

const static LedHwCfg_t g_ledCfgList[] = 
{
    {RCU_GPIOA, GPIOA, GPIO_PIN_8},
    {RCU_GPIOE, GPIOE, GPIO_PIN_6},
    {RCU_GPIOF, GPIOF, GPIO_PIN_6}
};

#define LED_NUM_MAX  (sizeof(g_ledCfgList) / sizeof(LedHwCfg_t))
	
/**
***********************************************************
* @brief LED硬件初始化
* @param
* @return 
***********************************************************
*/
void LedDrvInit(void)
{
	/*************GPIO初始化************/
	for (uint8_t i = 0; i < LED_NUM_MAX; i++)
	{
        rcu_periph_clock_enable(g_ledCfgList[i].rcu);
		gpio_init(g_ledCfgList[i].port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, g_ledCfgList[i].pin);
	}
}

/**
***********************************************************
* @brief 点亮LED
* @param ledNo，LED标号，0~2
* @return 
***********************************************************
*/
void TurnOnLed(uint8_t ledNo)
{
	if (ledNo >= LED_NUM_MAX)
	{
		return;
	}
	gpio_bit_write(g_ledCfgList[ledNo].port, g_ledCfgList[ledNo].pin, SET);
}

/**
***********************************************************
* @brief 熄灭LED
* @param ledNo，LED标号，0~2
* @return 
***********************************************************
*/
void TurnOffLed(uint8_t ledNo)
{
	if (ledNo >= LED_NUM_MAX)
	{
		return;
	}
	/***将LED对应的引脚设置为低电平***/
	gpio_bit_write(g_ledCfgList[ledNo].port, g_ledCfgList[ledNo].pin, RESET);  // 将LED对应的引脚设置为低电平
}

/**
***********************************************************
* @brief LED状态取反
* @param ledNo，LED标号，0~2
* @return 
***********************************************************
*/
void ToggleLed(uint8_t ledNo)
{
	if (ledNo >= LED_NUM_MAX)
	{
		return;
	}
	FlagStatus status = gpio_input_bit_get(g_ledCfgList[ledNo].port, g_ledCfgList[ledNo].pin);
	gpio_bit_write(g_ledCfgList[ledNo].port, g_ledCfgList[ledNo].pin, (bit_status)(!status));
}
