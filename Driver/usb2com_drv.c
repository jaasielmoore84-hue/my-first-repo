#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "gd32f30x.h"
#include "usb2com_drv.h"
#include "led_drv.h"

#define PACKET_DATA_LEN     (6)
static uint8_t g_rcvDataBuf[PACKET_DATA_LEN];

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
	/* 在USART_CTL0寄存器中设置REN位，使能接收功能；*/
	usart_receive_config(USART0, USART_RECEIVE_ENABLE);
	/* 打开串口模块中的中断开关 */
	//usart_interrupt_enable(USART0, USART_INT_RBNE);  // 接收非空中断
	usart_interrupt_enable(USART0, USART_INT_IDLE);    // 接收空闲中断
	/* 打开串口dma接收数据功能 */
	usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);
	/* 打开NVIC中的中断开关 */
	nvic_irq_enable(USART0_IRQn, 0, 0);
	/* 在USART_CTL0寄存器中置位UEN位，使能UART；*/
	usart_enable(USART0);
}

#define USART0_DATA_ADDR  (USART0 + 0x04)   // 串口0的接收数据寄存器地址的数值

static void DmaInit(void)
{
	/* 使能DMA时钟；*/
	rcu_periph_clock_enable(RCU_DMA0);
	/* 复位DMA通道；*/
	dma_deinit(DMA0, DMA_CH4);
	
	dma_parameter_struct dmaStruct;
	dma_struct_para_init(&dmaStruct);
	/* 配置传输方向；*/ 
	dmaStruct.direction = DMA_PERIPHERAL_TO_MEMORY;
	/* 配置数据源地址；*/ 
	dmaStruct.periph_addr = USART0_DATA_ADDR;   // 0x4001 3804
	/* 配置源地址是固定的还是增长的；*/ 
	dmaStruct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
	/* 配置源数据传输位宽；*/ 
	dmaStruct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
	
	/* 配置数据目的地址；*/
	dmaStruct.memory_addr = (uint32_t)g_rcvDataBuf; // 0x2000 0000
	/* 配置目的地址是固定的还是增长的；*/ 
	dmaStruct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
	/* 配置目的数据传输位宽；*/ 
	dmaStruct.memory_width = DMA_MEMORY_WIDTH_8BIT;
	/* 配置数据传输最大次数；*/ 
	dmaStruct.number = PACKET_DATA_LEN;
	/* 配置DMA通道优先级；*/ 
	dmaStruct.priority = DMA_PRIORITY_HIGH;
	dma_init(DMA0, DMA_CH4, &dmaStruct);
	/* 使能DMA通道；*/ 
	dma_channel_enable(DMA0, DMA_CH4);
}

void Usb2ComDrvInit(void)
{
	GpioInit();
	UartInit(115200);
	DmaInit();
}

static bool g_pktRcvd;
/**
***********************************************************************
包格式：帧头0    帧头1    功能字   LED编号    亮/灭    异或校验数据
        0x55    0xAA     0x06      0x02      0x01      0x  
***********************************************************************
*/
#define FRAM_HEAD_0         (0x55)
#define FRAM_HEAD_1         (0xAA)
#define LED_WRITE_CODE      (0x06)
#define CTRL_DATA_IDX       (2)       // 功能字数组下标
#define LED_NO_IDX          (3)       // LED编号数组下标
#define LED_CMD_IDX         (4)       // 命令字数组下标

/**
 * @brief 计算8位异或校验值
 * @param data 数据缓冲区指针
 * @param length 数据长度（字节数）
 * @return 计算得到的8位异或校验值
 */
static uint8_t CalXorSum(const uint8_t *data, uint32_t length) 
{
    uint8_t checksum = 0;
    
    // 遍历所有数据字节进行异或运算
    for (uint32_t i = 0; i < length; i++) 
	{
        checksum ^= data[i];
    }
    
    return checksum;
}

/**
***********************************************************
* @brief 在main while(1)被调用，判断异或校验，控制LED
* @param 
* @return 
***********************************************************
*/
void Usb2ComTask(void)
{
	if (!g_pktRcvd)
	{
		return;
	}
	g_pktRcvd = false;
	
	if (g_rcvDataBuf[0] != FRAM_HEAD_0 || g_rcvDataBuf[1] != FRAM_HEAD_1)
	{
		return;
	}

	if (CalXorSum(g_rcvDataBuf, PACKET_DATA_LEN - 1) != g_rcvDataBuf[PACKET_DATA_LEN - 1])
	{
		return;
	}
	(g_rcvDataBuf[LED_CMD_IDX] != 0) ? TurnOnLed(g_rcvDataBuf[LED_NO_IDX]) : TurnOffLed(g_rcvDataBuf[LED_NO_IDX]);
}

void USART0_IRQHandler(void)
{
	if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE) != RESET) //清除IDLE标志，第一步，读取stat0寄存器
	{
		usart_data_receive(USART0); //清除IDLE标志，第二步，读取接收数据寄存器，只是为了清除标志
		if (PACKET_DATA_LEN == (PACKET_DATA_LEN - dma_transfer_number_get(DMA0, DMA_CH4))) //dma_transfer_number_get返回的是剩余没有搬运的次数
		{
			g_pktRcvd = true;
		}
		/* 在DMA搬运数据的工程中，方向/增长/地址都不会变化，但是搬运次数会递减，所以需要重新配置 */
		dma_channel_disable(DMA0, DMA_CH4);
		dma_transfer_number_config(DMA0, DMA_CH4, PACKET_DATA_LEN);
		dma_channel_enable(DMA0, DMA_CH4);
	}
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
