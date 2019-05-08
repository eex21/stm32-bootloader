#ifndef __STMFLASH_H__
#define __STMFLASH_H__
#include "sys.h"   
//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEK STM32F407������
//STM32�ڲ�FLASH��д ��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2014/5/9
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 



//FLASH��ʼ��ַ
#define STM32_FLASH_BASE 0x08000000 	//STM32 FLASH����ʼ��ַ
 
#define SIZE_FLASH_SECTOR    	((u32) 0x800) 	//size 2 Kbytes  

//FLASH ��������ʼ��ַ
#define ADDR_FLASH_SECTOR(n)    (STM32_FLASH_BASE+(n * SIZE_FLASH_SECTOR)) 	//����0��ʼ��ַ0xx800000, size 2 Kbytes  


u32 STMFLASH_ReadWord(u32 faddr);		  	//������  
void STMFLASH_Write(u32 WriteAddr,u32 *pBuffer,u32 NumToWrite);		//��ָ����ַ��ʼд��ָ�����ȵ�����
void STMFLASH_Read(u32 ReadAddr,u32 *pBuffer,u32 NumToRead);   		//��ָ����ַ��ʼ����ָ�����ȵ�����
						   
#endif

















