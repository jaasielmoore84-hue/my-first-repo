#ifndef _USB2COM_DRV_H_
#define _USB2COM_DRV_H_

void Usb2ComDrvInit(void);

/**
***********************************************************
* @brief 在main while(1)被调用，判断异或校验，控制LED
* @param 
* @return 
***********************************************************
*/
void Usb2ComTask(void);

#endif
