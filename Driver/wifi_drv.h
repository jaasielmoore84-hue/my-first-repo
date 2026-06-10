#ifndef _WIFI_DRV_H_
#define _WIFI_DRV_H_

#include <stdint.h>

#define WIFI_MAX_BUF_SIZE  (1024)

void EnableWifiModule(void);

void DisableWifiModule(void);

void WifiDrvInit(void);
/**
****************************************************************
* @brief 通过串口发送字符串（\0结束，不发送），模组需要\r\n结尾
* @param sendStr,输入，字符串
* @return 
****************************************************************
*/
void SendWifiModuleStr(char *sendStr);

/**
**************************************************************
* @brief 通过串口接收字符(模组返回的没有\0)
* @param buffer,输出，在上层对应保存字符的数组，使用str库解析
* @return 返回字符个数
**************************************************************
*/
uint16_t RecvWifiModuleStr(char *buffer);
#endif
