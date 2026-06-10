#include <stdint.h>
#include <stdio.h>
#include "gd32f30x.h"
#include "delay.h"
#include "systick.h"

typedef struct
{
	rcu_periph_enum rcu;
	uint32_t gpio;
	uint32_t pin;
} Key_GPIO_t;

static Key_GPIO_t g_gpioList[] = 
{
	{RCU_GPIOA, GPIOA, GPIO_PIN_0},   // key1
	{RCU_GPIOG, GPIOG, GPIO_PIN_13},  // key2
	{RCU_GPIOG, GPIOG, GPIO_PIN_14},  // key3
	{RCU_GPIOG, GPIOG, GPIO_PIN_15}   // key4
};

#define KEY_NUM_MAX (sizeof(g_gpioList) / sizeof(g_gpioList[0]))

/**
***********************************************************
* @brief 按键硬件初始化
* @param
* @return 
***********************************************************
*/
void KeyDrvInit(void)
{
	for (uint8_t i = 0; i < KEY_NUM_MAX; i++)
	{
		rcu_periph_clock_enable(g_gpioList[i].rcu);
		gpio_init(g_gpioList[i].gpio, GPIO_MODE_IN_FLOATING,
				  GPIO_OSPEED_2MHZ, g_gpioList[i].pin);
	}
}

#if 1
typedef enum
{
	KEY_STATE_RELEASE,            // 释放松开
	KEY_STATE_DEBOUNCING,         // 消抖
	KEY_STATE_SHORT_PRESSED,      // 短按
	KEY_STATE_LONG_PRESSED,       // 长按
} KEY_STATE;

typedef struct
{
	KEY_STATE keyState;
//	uint64_t prvSysTime;
	uint64_t IoChangeSysTime;      // 用于按键消抖和长按判断
	uint8_t keyReleaseNum;
	uint64_t firstReleaseSysTime;  // 用于按键双击判断
} KeyInfo_t;

static KeyInfo_t g_keyInfo[KEY_NUM_MAX];

#define DEBOUNCING_TIME   (10)              // 按键消抖时间窗ms
#define LONGPRESS_TIME    (1000)            // 长按时间窗ms
#define DOUBLE_CLICK_INTERVAL_TIME  (300)   // 两次抬起时间窗100ms，用来判断是否双击

static uint8_t KeyScan(uint8_t keyIndex)
{
	FlagStatus keyIoVal = gpio_input_bit_get(g_gpioList[keyIndex].gpio, 
											g_gpioList[keyIndex].pin);
	switch (g_keyInfo[keyIndex].keyState)
	{
		case KEY_STATE_RELEASE:
			if (keyIoVal == RESET)
			{
				//printf("keyIndex: %d, 11111111111111111\n", keyIndex);
				g_keyInfo[keyIndex].IoChangeSysTime = GetSysRunTime();
				g_keyInfo[keyIndex].keyState = KEY_STATE_DEBOUNCING;
				break;
			}
			
			if (g_keyInfo[keyIndex].keyReleaseNum == 1)
			{
				if (GetSysRunTime() - g_keyInfo[keyIndex].firstReleaseSysTime > DOUBLE_CLICK_INTERVAL_TIME) //  超过双击时间间隔，返回单击码值
				{
					g_keyInfo[keyIndex].keyReleaseNum = 0;
					return (keyIndex + 0x01); //  返回按键码值，三个按键单击对应0x01, 0x02, 0x03
				}
			}
			if (g_keyInfo[keyIndex].keyReleaseNum == 2)
			{
				g_keyInfo[keyIndex].keyReleaseNum = 0;
				if (GetSysRunTime() - g_keyInfo[keyIndex].firstReleaseSysTime <= DOUBLE_CLICK_INTERVAL_TIME) //  在双击时间间隔内，返回双击码值
				{
					
					return (keyIndex + 0x51); //  返回按键码值，三个按键双击对应0x51, 0x52, 0x53
				}
			}
			
			break;
		case KEY_STATE_DEBOUNCING:
			if (GetSysRunTime() - g_keyInfo[keyIndex].IoChangeSysTime < DEBOUNCING_TIME)
			{
				break;
			}
			if (keyIoVal == RESET)
			{
				//printf("keyIndex: %d, 2222222222222222\n", keyIndex);
				g_keyInfo[keyIndex].keyState = KEY_STATE_SHORT_PRESSED;
			}
			else
			{
				//printf("keyIndex: %d, 3333333333333333\n", keyIndex);
				g_keyInfo[keyIndex].keyState = KEY_STATE_RELEASE;
			}
			break;
		case KEY_STATE_SHORT_PRESSED:
			//printf("keyIndex: %d, 444444444444444444\n", keyIndex);
			if (keyIoVal != RESET)
			{
				//printf("keyIndex: %d, 5555555555555555\n", keyIndex);
				g_keyInfo[keyIndex].keyState = KEY_STATE_RELEASE;
//				return (keyIndex + 1);
				g_keyInfo[keyIndex].keyReleaseNum++;
				if (g_keyInfo[keyIndex].keyReleaseNum == 1)   
				{
					g_keyInfo[keyIndex].firstReleaseSysTime = GetSysRunTime();   // 记录抬起那一时刻的运行时间
					break;
				}
			}
			else
			{
				if (GetSysRunTime() - g_keyInfo[keyIndex].IoChangeSysTime > LONGPRESS_TIME)
				{
					//printf("keyIndex: %d, 666666666666666666\n", keyIndex);
					g_keyInfo[keyIndex].keyState = KEY_STATE_LONG_PRESSED;
				}	
			}
			break;
		case KEY_STATE_LONG_PRESSED:
			//printf("keyIndex: %d, 777777777777777777\n", keyIndex);
			if (keyIoVal != RESET)
			{
				//printf("keyIndex: %d, 888888888888888888\n", keyIndex);
				g_keyInfo[keyIndex].keyState = KEY_STATE_RELEASE;
				return (keyIndex + 0x81);
			}
			break;
		default:
			break;
	}
	return 0;
}

#else
static uint8_t KeyScan(uint8_t keyIndex)
{
	FlagStatus keyIoVal = gpio_input_bit_get(g_gpioList[keyIndex].gpio, 
											g_gpioList[keyIndex].pin);
	
	if (keyIoVal == RESET)
	{
		printf("keyIndex: %d, 00000000000000000\n", keyIndex);
		DelayNms(10);
		keyIoVal = gpio_input_bit_get(g_gpioList[keyIndex].gpio, 
											g_gpioList[keyIndex].pin);
		if (keyIoVal == RESET)
		{
			printf("keyIndex: %d, 11111111111111111\n", keyIndex);
			while (RESET == gpio_input_bit_get(g_gpioList[keyIndex].gpio, g_gpioList[keyIndex].pin));
			printf("keyIndex: %d, 22222222222222222\n", keyIndex);
			return keyIndex + 1;
		}
	}
	return 0;
}
#endif
/**
***********************************************************
* @brief 获取按键码值,屏蔽底层实现,应用只关心按键值,不需要关心是短按/长按还是多连击
* @param
* @return 四个按键码值，短按0x01 0x02 0x03，0x04
***********************************************************
*/
uint8_t GetKeyVal(void)
{
	uint8_t res = 0;
/*
	for (uint8_t i = 0; i < KEY_NUM_MAX; i++)
	{
		res = KeyScan(i);
		if (res != 0)
		{
			return res;
		}
	}
*/
	res = KeyScan(0);
	if (res != 0)
	{
		return res;
	}
	
	res = KeyScan(1);
	if (res != 0)
	{
		return res;
	}
	
	res = KeyScan(2);
	if (res != 0)
	{
		return res;
	}
	
	res = KeyScan(3);
	if (res != 0)
	{
		return res;
	}
	return 0;
}
