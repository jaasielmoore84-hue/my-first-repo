#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "gd32f30x.h"
#include "inflash_drv.h"

/**
*******************************************************************
* @function 指定地址开始读出指定个数的数据
* @param    readAddr,读取地址
* @param    pBuffer,数组首地址
* @param    numToRead,要读出的数据个数
* @return   
*******************************************************************
*/
bool FlashRead(uint32_t readAddr, uint8_t *pBuffer, uint32_t numToRead) //0x8000000
{
	if ((readAddr + numToRead) > FLASH_DEADLINE_ADDRESS)
	{
		return false;
	}
	uint32_t addr = readAddr;
	for (uint32_t i = 0; i < numToRead; i++)
	{
		*pBuffer = *(uint8_t *)addr; // 按照一个字节步长读取数据
		addr++;  // 0x8000001,这里不是地址+1
		pBuffer++;
	}
	return true;
}

/**
*******************************************************************
* @function 指定地址开始写入指定个数的数据,半字写入
* @param    writeAddr,写入地址
* @param    pBuffer,数组首地址
* @param    numToWrite,要写入的数据个数，字节为单位
* @return                                                         
*******************************************************************
*/
bool FlashWrite(uint32_t writeAddr, uint8_t *pBuffer, uint32_t numToWrite)
{
	if ((writeAddr + numToWrite) > FLASH_DEADLINE_ADDRESS)
	{
		return false;
	}
	
	if (writeAddr % 2 == 1)   // 半字(2字节)写入，地址要对齐
	{
		return false;
	}
	
	fmc_state_enum fmcState = FMC_READY;
	__disable_irq();   // 单bank Flash：编程期间禁止中断，防止CPU取指与写Flash冲突
	fmc_unlock();
	for (uint32_t i = 0; i < numToWrite / 2; i++) 
	{
		fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
		fmcState = fmc_halfword_program(writeAddr, *(uint16_t *)pBuffer); // 转换成步长为2
		if (fmcState != FMC_READY)
		{
			fmc_lock();
			__enable_irq();
			return false;
		}

		pBuffer += 2;
		writeAddr += 2;
	}
	uint16_t temp;
	if (numToWrite % 2) // 如果写入数据个数为奇数，则需要补一个字节
	{
		fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
		temp = *pBuffer | 0xff00; // 补一个高字节，补0xff
		fmcState = fmc_halfword_program(writeAddr, temp);
	}

	fmc_lock();
	__enable_irq();
	return true;
}

/**
*******************************************************************
* @function 擦除从eraseAddr开始到eraseAddr + numToErase的页
* @param    eraseAddr,地址
* @param    numToErase,对应写入数据时的个数
* @return                                                         
*******************************************************************
*/
bool FlashErase(uint32_t eraseAddr, uint32_t numToErase)
{
	if ((eraseAddr + numToErase) > FLASH_DEADLINE_ADDRESS)
	{
		return false;
	}

	fmc_state_enum fmcState = FMC_READY;
	uint32_t addrOffset = eraseAddr % FLASH_PAGE_SIZE;
	uint32_t pageNum;

	__disable_irq();   // 单bank Flash：擦除期间禁止中断，防止CPU取指与擦Flash冲突
	fmc_unlock();
	
	if (numToErase > (FLASH_PAGE_SIZE - addrOffset)) // 判断是否跨页，跨页则先擦本页，再擦后续页
	{
		fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
		fmcState = fmc_page_erase(eraseAddr);           // 擦本页
		if (fmcState != FMC_READY)
		{
			goto erase_err;
		}
		
		eraseAddr += FLASH_PAGE_SIZE - addrOffset;   // 对齐到下一页起始地址
		numToErase -= FLASH_PAGE_SIZE - addrOffset;
		pageNum = numToErase / FLASH_PAGE_SIZE;
		while (pageNum--) 
		{
			fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
			fmcState = fmc_page_erase(eraseAddr);
			if (fmcState != FMC_READY)
			{
				goto erase_err;
			}
			eraseAddr += FLASH_PAGE_SIZE;
		}
		if (numToErase % FLASH_PAGE_SIZE != 0) // 如果有不足一页，则擦除剩余部分
		{
			fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
			fmcState = fmc_page_erase(eraseAddr);          
			if (fmcState != FMC_READY)
			{
				goto erase_err;
			}
		}	
	}
	else // 不跨页，则直接擦除本页
	{
		fmc_flag_clear(FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR);
		fmcState = fmc_page_erase(eraseAddr); // eraseAddr不必是页起始地址，也可擦除本页
		if (fmcState != FMC_READY)
		{
			goto erase_err;
		}
	}
	fmc_lock();
	__enable_irq();
	return true;
	
erase_err:
	fmc_lock();
	__enable_irq();
	return false;
}

#if 0
#define BUFFER_SIZE                   3000
#define FLASH_TEST_ADDRESS            0x0807F004  
uint8_t bufferWrite[BUFFER_SIZE];
uint8_t bufferRead[BUFFER_SIZE];
void FlashDrvTest(void)
{

	
	printf("flash writing data:\n");
	for (uint16_t i = 0; i < BUFFER_SIZE; i++)
	{ 
		bufferWrite[i] = i + 1;
		printf("0x%02X ", bufferWrite[i]);
    }
	printf("\n start writing...\n");
	
	if (!FlashErase(FLASH_TEST_ADDRESS, BUFFER_SIZE))
	{
		printf("Flash erase error.\n");
		return;
	}

	if (!FlashWrite(FLASH_TEST_ADDRESS, bufferWrite, BUFFER_SIZE))
	{
		printf("Flash write error.\n");
		return;
	}
	
	printf("start reading...\n");
	if (!FlashRead(FLASH_TEST_ADDRESS, bufferRead, BUFFER_SIZE))
	{
		printf("Flash read error\n");
		return;
	}
	for (uint16_t i = 0; i < BUFFER_SIZE; i++)
	{
        if (bufferRead[i] != bufferWrite[i]){
            printf("0x%02X ", bufferRead[i]);
            printf("Flash test error\n");
            return;
        }
        printf("0x%02X ", bufferRead[i]);

    }
	printf("\nFlash test pass.\n");
}
#endif
