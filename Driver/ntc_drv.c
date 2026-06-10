#include <stdint.h>
#include <stdio.h>
#include "gd32f30x.h"
#include "delay.h"

static void GpioInit(void)
{
	rcu_periph_clock_enable(RCU_GPIOC);
	gpio_init(GPIOC, GPIO_MODE_AIN, GPIO_OSPEED_10MHZ, GPIO_PIN_3);
}

static void AdcInit(void)
{
	/* 使能时钟；*/
	rcu_periph_clock_enable(RCU_ADC0);
	/* 设置分频系数；*/
	rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV6);  // 6分频，120MHz / 6 = 20MHz
	/* 设置独立模式；*/
	adc_mode_config(ADC_MODE_FREE); 
	/* 设置连续模式；*/ 
	adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
	/* 设置数据对齐；*/
	adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
	/* 设置转换通道个数；*/ 
	adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 1);
	/* 设置转换哪一个通道以及所处序列位置；*/ 
	adc_regular_channel_config(ADC0, 0, ADC_CHANNEL_13, ADC_SAMPLETIME_55POINT5);
	/* 设置选择哪一个外部触发源；*/ 
	adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_REGULAR_NONE); 
	/* 使能外部触发；*/ 
	adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);
	adc_dma_mode_enable(ADC0);
	/* 使能ADC；*/ 
	adc_enable(ADC0);
	/* 内部校准；*/ 
    DelayNms(1);
    adc_calibration_enable(ADC0);
	adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
}

void TempDrvInit(void)
{
	GpioInit();
	AdcInit();
}

static uint16_t GetAdcVal(void)
{
	uint16_t adcVal;
	while (SET != adc_flag_get(ADC0, ADC_FLAG_EOC));
	adcVal = adc_regular_data_read(ADC0);
//	printf("adc val = %d\n", adcVal);
	return adcVal;
}

static const uint16_t g_ntcAdcTable[] = {
	3847, 3833, 3819, 3804, 3789, 3773, 3757, 3740, 3722, 3703,     //-30 ~ -21℃
	3684, 3664, 3643, 3622, 3599, 3576, 3552, 3527, 3502, 3475,     //-20 ~ -11℃
	3448, 3420, 3390, 3360, 3329, 3298, 3265, 3231, 3197, 3162,		//-10 ~  -1℃
	3123, 3089, 3051, 3013, 2973, 2933, 2893, 2852, 2810, 2767,     //0   ~   9℃
	2720, 2681, 2637, 2593, 2548, 2503, 2458, 2412, 2367, 2321,     //10  ~  19℃
	2275, 2230, 2184, 2138, 2093, 2048, 2002, 1958, 1913, 1869,     //20  ~  29℃
	1825, 1782, 1739, 1697, 1655, 1614, 1573, 1533, 1494, 1455,     //30  ~  39℃
	1417, 1380, 1343, 1307, 1272, 1237, 1203, 1170, 1138, 1106,     //40  ~  49℃
	1081, 1045, 1016, 987,  959,  932,  905,  879,  854,  829,      //50  ~  59℃
	806,  782,  760,  738,  716,  696,  675,  656,  637,  618,      //60  ~  69℃
	600,  583,  566,  550,  534,  518,  503,  489,  475,  461,      //70  ~  79℃
	448,  435,  422,  410,  398,  387,  376,  365,  355,  345,      //80  ~  89℃
	335,  326,  316,  308,  299,  290,  283,  274,  267,  259,      //90  ~  99℃
};
#define NTC_TABLE_SIZE         (sizeof(g_ntcAdcTable) / sizeof(g_ntcAdcTable[0]))
#define INDEX_TO_TEMP(index)   ((int32_t)(index - 30))

static float g_tempData;
#define MAX_BUF_SIZE      10
static int16_t g_temp10MplBuf[MAX_BUF_SIZE];

static int32_t DescBinarySearch(const uint16_t *arr, int32_t size, uint16_t key) 
{
	int32_t left = 0;              			
	int32_t right = size - 1;       		
	int32_t mid;
	int32_t index = size - 1;
	while (left <= right)             		
	{
		mid = left + (right - left) / 2; 
		if (key >= arr[mid])
		{
			right = mid - 1;
			index = mid;
		}
		else
		{
			left = mid + 1;  
		}
	}
    return index;               				
}

static int16_t AdcToTemp10Mpl(uint16_t adcVal)
{
	int32_t index = DescBinarySearch(g_ntcAdcTable, NTC_TABLE_SIZE, adcVal);
	
	if (index == 0)
	{
		return INDEX_TO_TEMP(index) * 10;
	}
	int16_t temp10Mpl = INDEX_TO_TEMP(index - 1) * 10 + (g_ntcAdcTable[index - 1] - adcVal) * 10 / (g_ntcAdcTable[index - 1] - g_ntcAdcTable[index]);
	return temp10Mpl;
}

static void PushDataToBuf(int16_t temp10Mpl)
{
	static uint16_t s_index = 0;
	g_temp10MplBuf[s_index] = temp10Mpl;
	s_index++;
	s_index %= MAX_BUF_SIZE;
}

/**
***********************************************************
* @brief 冒泡排序,把数据按大小排列 
* @param 
* @return 
***********************************************************
*/
static void BubbleSort(int16_t *arr, uint16_t n)
{
	int16_t i, j, temp;
	
	for (i = 0; i < (n - 1); i++)		// 每次外层循环都会确定当前未排序部分中的最大/最小值所在位置
	{			
		for (j = 0;j < (n - i - 1); j++) 
		{
			if (arr[j] > arr[j + 1])    // 如果发现后面的元素更大/更小，则进行交换
			{
				temp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = temp;
			}
		}
	}
}

/**
***********************************************************
* @brief 算术平均滤波
* @param arr，数组首地址
* @param len，元素个数
* @return 平均运算结果
***********************************************************
*/
static int16_t ArithAvgFltr(int16_t *arr, uint32_t len)
{
	int32_t sum = 0;
	for (uint32_t i = 0; i < len; i++)
	{
		sum += arr[i]; 
	}
	return (int16_t)(sum / len);
}

/**
***********************************************************
* @brief 中位值平均滤波
* @param arr，数组首地址
* @param len，元素个数，需要大于等于3个
* @return 平均运算结果
***********************************************************
*/
static int16_t MedianAvgFltr(int16_t *arr, uint32_t len)
{
	BubbleSort(arr, len);
	return ArithAvgFltr(&arr[1], len - 2);
}

/**
***********************************************************
* @brief 生产温度数据
* @param
* @return 
***********************************************************
*/
void TempDataProc(void)
{
	static uint16_t s_convertNum = 0;
	
	uint16_t adcVal = GetAdcVal();
	int16_t temp10Mpl = AdcToTemp10Mpl(adcVal);
	PushDataToBuf(temp10Mpl);
	
	s_convertNum++;
	
	if (s_convertNum < 3)
	{
		g_tempData = g_temp10MplBuf[0] / 10.0f;
		return;
	}
	
	if (s_convertNum > MAX_BUF_SIZE)
	{
		s_convertNum = MAX_BUF_SIZE;
	}
	g_tempData = MedianAvgFltr(g_temp10MplBuf, s_convertNum) / 10.0f;
}

/**
***********************************************************
* @brief 获取温度传感器数据
* @param
* @return 温度数据，小数
***********************************************************
*/
float GetTempData(void)
{
	return g_tempData;
}

void TempDrvTest(void)
{
	float tempData = GetTempData();
	printf("tempData = %.1f .\n", tempData);
}
