#ifndef __IAP_H__
#define __IAP_H__
#include "sys.h"  
#include "iap.h"  
typedef  void (*iapfun)(void);				//����һ���������͵Ĳ���.   
											//����0X08000000~0X0800FFFF�Ŀռ�ΪBootloaderʹ��(��64KB)	   
void iap_write_appbin(u32 appxaddr,u8 *appbuf,u32 applen);	//��ָ����ַ��ʼ,д��bin
#endif