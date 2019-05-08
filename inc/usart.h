#ifndef __USART_H
#define __USART_H
#include "stdio.h"	
#include "stm32f10x_conf.h"
#include "sys.h" 

#define USART_REC_LEN  			10*1024 //serial data buffer
#define EN_USART3_RX 			1		
	  	
extern u8  USART_RX_BUF[USART_REC_LEN]; //接收缓冲
extern u16 USART_RX_STA;         		//接收状态标记	
extern u16 USART_RX_CNT;				//接收的字节数	

#endif


