#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "gd32f30x.h"
#include "usb2com_drv.h"
#include "led_drv.h"

static void GpioInit(void)
{
	rcu_periph_clock_enable(RCU_GPIOA);
	gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_9);
	gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, GPIO_PIN_10); // 需要配置成上拉输入，增加抗干扰能力
}

static void UartInit(uint32_t baudRate)
{
	/* 使能UART时钟；*/
	rcu_periph_clock_enable(RCU_USART0);
	/* 复位UART；*/
	usart_deinit(USART0);
	
	/* 通过USART_CTL0寄存器的WL设置字长；*/ 
	/* 通过USART_CTL0寄存器的PCEN设置校验位；*/ 
	/* 在USART_CTL1寄存器中写STB[1:0]位来设置停止位的长度；*/ 
	/* 以上保持默认，8位数据位，1位停止位，没有奇偶校验位 */
	
	/* 在USART_BAUD寄存器中设置波特率；*/ 
	usart_baudrate_set(USART0, baudRate);
	/* 在USART_CTL0寄存器中设置TEN位，使能发送功能；*/
	usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
	/* 在USART_CTL0寄存器中置位UEN位，使能UART；*/
	usart_enable(USART0);
}

void Usb2ComDrvInit(void)
{
	GpioInit();
	UartInit(115200);
}

/**
***********************************************************
* @brief printf函数默认打印输出到显示器，如果要输出到串口，
		 必须重新实现fputc函数，将输出指向串口，称为重定向
* @param
* @return 
***********************************************************
*/
int fputc(int ch, FILE *f)
{
	usart_data_transmit(USART0, (uint8_t)ch);
	while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET);
	return ch;
}
