#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "gd32f30x.h"
#include "wifi_drv.h"
#include "delay.h"


static uint8_t g_rcvDataBuf[WIFI_MAX_BUF_SIZE];
static uint32_t g_pktLen;
static bool g_pktRcvd;

static void GpioInit(void)
{
	rcu_periph_clock_enable(RCU_GPIOB);
	gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_10);
	gpio_init(GPIOB, GPIO_MODE_IPU, GPIO_OSPEED_10MHZ, GPIO_PIN_11); // 需要配置成上拉输入，增加抗干扰能力

	rcu_periph_clock_enable(RCU_GPIOG);
	gpio_init(GPIOG, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_7);
}

void EnableWifiModule(void)
{
	gpio_bit_set(GPIOG, GPIO_PIN_7);   // 使能WIFI模组
}

void DisableWifiModule(void)
{
	gpio_bit_reset(GPIOG, GPIO_PIN_7); // 复位WIFI模组
}

static void UartInit(uint32_t baudRate)
{
	/* 使能UART时钟；*/
	rcu_periph_clock_enable(RCU_USART2);
	/* 复位UART；*/
	usart_deinit(USART2);
	
	/* 通过USART_CTL0寄存器的WL设置字长；*/ 
	/* 通过USART_CTL0寄存器的PCEN设置校验位；*/ 
	/* 在USART_CTL1寄存器中写STB[1:0]位来设置停止位的长度；*/ 
	/* 以上保持默认，8位数据位，1位停止位，没有奇偶校验位 */
	
	/* 在USART_BAUD寄存器中设置波特率；*/ 
	usart_baudrate_set(USART2, baudRate);
	/* 在USART_CTL0寄存器中设置TEN位，使能发送功能；*/
	usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);
	/* 在USART_CTL0寄存器中设置REN位，使能接收功能；*/
	usart_receive_config(USART2, USART_RECEIVE_ENABLE);
	/* 打开串口模块中的中断开关 */
	//usart_interrupt_enable(USART2, USART_INT_RBNE);  // 接收非空中断
	usart_interrupt_enable(USART2, USART_INT_IDLE);    // 接收空闲中断
	/* 打开串口dma接收数据功能 */
	usart_dma_receive_config(USART2, USART_RECEIVE_DMA_ENABLE);
	/* 打开NVIC中的中断开关 */
	nvic_irq_enable(USART2_IRQn, 0, 0);
	/* 在USART_CTL0寄存器中置位UEN位，使能UART；*/
	usart_enable(USART2);
}

#define USART2_DATA_ADDR  (USART2 + 0x04)   // 串口2的接收数据寄存器地址的数值

static void DmaInit(void)
{
	/* 使能DMA时钟；*/
	rcu_periph_clock_enable(RCU_DMA0);
	/* 复位DMA通道；*/
	dma_deinit(DMA0, DMA_CH2);
	
	dma_parameter_struct dmaStruct;
	dma_struct_para_init(&dmaStruct);
	/* 配置传输方向；*/ 
	dmaStruct.direction = DMA_PERIPHERAL_TO_MEMORY;
	/* 配置数据源地址；*/ 
	dmaStruct.periph_addr = USART2_DATA_ADDR;
	/* 配置源地址是固定的还是增长的；*/ 
	dmaStruct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
	/* 配置源数据传输位宽；*/ 
	dmaStruct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
	
	/* 配置数据目的地址；*/
	dmaStruct.memory_addr = (uint32_t)g_rcvDataBuf;
	/* 配置目的地址是固定的还是增长的；*/ 
	dmaStruct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
	/* 配置目的数据传输位宽；*/ 
	dmaStruct.memory_width = DMA_MEMORY_WIDTH_8BIT;
	/* 配置数据传输最大次数；*/ 
	dmaStruct.number = WIFI_MAX_BUF_SIZE;
	/* 配置DMA通道优先级；*/ 
	dmaStruct.priority = DMA_PRIORITY_HIGH;
	dma_init(DMA0, DMA_CH2, &dmaStruct);
	/* 使能DMA通道；*/ 
	dma_channel_enable(DMA0, DMA_CH2);
}

void WifiDrvInit(void)
{
	GpioInit();
	UartInit(115200);
	DmaInit();
	DisableWifiModule();  // 先硬件复位等一会再使能，没有复位直接使能会返回error
	printf("wifi module,now hwreset it!\n");
	DelayNms(100);
	EnableWifiModule();
}

void USART2_IRQHandler(void)
{
	if (usart_interrupt_flag_get(USART2, USART_INT_FLAG_IDLE) != RESET) //清除IDLE标志，第一步，读取stat0寄存器
	{
		usart_data_receive(USART2); //清除IDLE标志，第二步，读取接收数据寄存器，只是为了清除标志
		g_pktLen = WIFI_MAX_BUF_SIZE - dma_transfer_number_get(DMA0, DMA_CH2); //dma_transfer_number_get返回的是剩余没有搬运的次数
		g_pktRcvd = true;
		//printf("USART2_IRQHandler:recv wifi rsp:%s\n", g_rcvDataBuf);

		/* 在DMA搬运数据的工程中，方向/增长/地址都不会变化，但是搬运次数会递减，所以需要重新配置 */
		dma_channel_disable(DMA0, DMA_CH2);
		dma_transfer_number_config(DMA0, DMA_CH2, WIFI_MAX_BUF_SIZE);
		dma_channel_enable(DMA0, DMA_CH2);
	}
}

/**
**************************************************************
* @brief 通过串口接收字符(模组返回的没有\0)
* @param buffer,输出，在上层对应保存字符的数组，使用str库解析
* @return 返回字符个数
**************************************************************
*/
uint16_t RecvWifiModuleStr(char *buffer)
{
	if (g_pktRcvd)
	{
		g_pktRcvd = false;
		memcpy(buffer, g_rcvDataBuf, g_pktLen);
		printf("RecvWifiModuleStr:recv wifi rsp:%s\n", buffer);
		memset(g_rcvDataBuf, 0, g_pktLen);
		return g_pktLen;
	}
	return 0;
}

/**
****************************************************************
* @brief 通过串口发送字符串（\0结束，不发送），模组需要\r\n结尾
* @param sendStr,输入，字符串
* @return 
****************************************************************
*/
void SendWifiModuleStr(char *sendStr)
{
	printf("wifi send cmd:%s\n", sendStr);
	while (*sendStr != '\0')
	{
		usart_data_transmit(USART2, *sendStr);
		while (usart_flag_get(USART2, USART_FLAG_TBE) == RESET);
		sendStr++;
	}
}
