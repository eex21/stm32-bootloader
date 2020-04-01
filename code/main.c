/*--------------------------------------------------------------------------------------
@file:    IAP_Serial
@version: V1.03
@author:  dong
@data:    2019_05_29
@Introduction : Unlock flash   load_app from serial 
--------------------------------------------------------------------------------------*/
#include "usart.h"   
#include "led.h"  
#include "stmflash.h" 
#include "iap.h"  
#include <stdio.h> 

//#define REMOTE  /* default DOGG*/


// 最少要多留2页空间用于存放SN等数据,程序空间必须位于页面开始位置。
#define BOOT_SIZE			0x10000	
#define FLASH_APP1_ADDR	(0x08000000+BOOT_SIZE)	//第一个应用程序起始地址(存放在FLASH)

#ifdef REMOTE
#define TIME_25ms	 36000
#else
#define TIME_25ms	 34000
#endif
#define TIME_100ms		(uint32_t) 720000
#define TIME_1s			 40 	
#define TIME_IAP			 (TIME_1s*10)
#define TIME_BT			 (TIME_1s*30)

u32 main_delay,delay0,key_detecTime;
u8 mode;

#define GPIO_Boot  		GPIOB
#define GPIO_BootPin  	GPIO_Pin_4

#ifdef REMOTE
#define PowerSupply_On()		do{GPIO_SetBits(GPIOE,GPIO_Pin_1);}while(0)
#define PowerSupply_Off()	do{GPIO_ResetBits(GPIOE,GPIO_Pin_1);}while(0)
#define LED_red()			do{}while(0)
#define LED_blue()			do{}while(0)
#define LED_pink()			do{}while(0)
#define LED_OFF()			do{}while(0)
#else 
#define PowerSupply_On()		do{GPIO_SetBits(GPIOD,GPIO_Pin_5);}while(0)		//MCU_SYSTEM
#define PowerSupply_Off() 	do{GPIO_ResetBits(GPIOD,GPIO_Pin_5);}while(0)
#define LED_red()			do{GPIO_SetBits(GPIOA,GPIO_Pin_1);GPIO_ResetBits(GPIOA,GPIO_Pin_0);}while(0)
#define LED_blue()			do{GPIO_SetBits(GPIOA,GPIO_Pin_0);GPIO_ResetBits(GPIOA,GPIO_Pin_1);}while(0)
#define LED_pink()			do{GPIO_SetBits(GPIOA,GPIO_Pin_1|GPIO_Pin_1);}while(0)
#define LED_OFF()			do{GPIO_ResetBits(GPIOA,GPIO_Pin_0|GPIO_Pin_1);}while(0)
#endif

#if 0//def BOARD_V25
#define PowerSwitch     ((GPIOC->IDR & GPIO_Pin_3)? 0 : 1)
#else
#define PowerSwitch     ((GPIOB->IDR & GPIO_Pin_1)? 0 : 1)
#endif

u8 USART_RX_BUF[USART_REC_LEN] __attribute__ ((at(0X20001000)));//接收缓冲,最大USART_REC_LEN个字节,起始地址为0X20001000.    //接收状态

u16 USART_RX_STA=0;       	//接收状态标记	  
u16 USART_RX_CNT=0;		//接收的字节数
u8 i =0;

u32 addrCur = FLASH_APP1_ADDR;   //写flash  偏移地址   
//u16 buff_size_flag = 0;
u8 data_flag = 0 ;              //flag 
u8 success_getLength_flag = 0;  //success get the length of one package 
u8 crc_check = 0;               //crc_check
u16 length = 0 ;
u16 remainSend =0;

u16 datalength = 0;            //length of  one package
u16 j=0;
u16 receiveTime = 0;
u8  BIN_BUFF[2048] = {0};     
GPIO_InitTypeDef GPIO_InitStructure;

void RCC_Configuration(void)
	{   
	/* RCC system reset(for debug purpose) */
	RCC_DeInit();
	FLASH_SetLatency(FLASH_Latency_2);
	FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
	RCC_HSEConfig(RCC_HSE_ON);
	if (RCC_WaitForHSEStartUp() == SUCCESS)
		{
		RCC_HCLKConfig(RCC_SYSCLK_Div1); 
		RCC_PCLK1Config(RCC_HCLK_Div2);
		RCC_PCLK2Config(RCC_HCLK_Div1); 
		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
		SystemCoreClock = 72000000;
		}
	else
		{
		RCC_DeInit();
		RCC_HSICmd(ENABLE);//使能HSI  
		while(RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET);//等待HSI使能成功
		RCC_HCLKConfig(RCC_SYSCLK_Div1);   
		RCC_PCLK1Config(RCC_HCLK_Div2);
		RCC_PCLK2Config(RCC_HCLK_Div1);
		RCC_PLLConfig(RCC_PLLSource_HSI_Div2, RCC_PLLMul_16); // =(8/2) *16
		SystemCoreClock = 64000000;
		}
	RCC_PLLCmd(ENABLE);
	while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET){};
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	while(RCC_GetSYSCLKSource() != 0x08);
	}

void GPIO_BT_mode(u8 mode){
	// P2.0
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	if (mode == 1)
		GPIO_ResetBits(GPIOB,GPIO_Pin_4);//ean
	else
		GPIO_SetBits(GPIOB,GPIO_Pin_4);//ean
	
	GPIO_ResetBits(GPIOB,GPIO_Pin_3);//p2.1
	GPIO_SetBits(GPIOB,GPIO_Pin_5);//Vbus
	}

void GPIO_upgrade_mode(void)
	{
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2|GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	  

	GPIO_SetBits(GPIOB,GPIO_Pin_3);//BT p2.1
	GPIO_ResetBits(GPIOB,GPIO_Pin_4);//BT ean
	GPIO_ResetBits(GPIOB,GPIO_Pin_5);//BT Vbus
	}

void uart2_init(u32 bound)
	{
	USART_InitTypeDef USART_InitStructure;
	RCC_AHBPeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE); 
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);	  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	USART_InitStructure.USART_BaudRate = bound;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure); 
	USART_Cmd(USART2, ENABLE); 
}


void uart3_init(u32 bound)
	{
	USART_InitTypeDef USART_InitStructure;
	RCC_AHBPeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE); 
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);	  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	USART_InitStructure.USART_BaudRate = bound;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure); 
	USART_Cmd(USART3, ENABLE); 
}


int fputc(int ch, FILE *f)
	{ 	
	while((USART3->SR & USART_FLAG_TC)==0);  
	USART3->DR = (u8) ch;      
	return ch;
	}

void sendError(void)
	{
	printf("Error \r\n");
	USART_RX_CNT = 0;
	}

void sendOK(void)
	{
	printf("OK \r\n");
	USART_RX_CNT = 0;
	}

u8 xCal_crc(u8 *ptr,u32 len)
	{
	u8 crc=0;
	u8 i;
	crc = 0;
	while(len--)
    		{
       	crc ^= *ptr++;
      		for(i = 0;i < 8;i++)
			{
			if(crc & 0x01)
				{
				crc = (crc >> 1) ^ 0x8C;
				}
			else 
				crc >>= 1;
			}
   	 	}
    	return crc;
	}

void getLength(u8 *data, int currentPos)     //
{
	if(currentPos==2 && data[0]=='#' && data[2]=='#')       //find the header
	{
		remainSend = data[1];
		data_flag = 1;
	}
	else if(currentPos==4 && data_flag==1)
	{
		datalength = data[4]*256 + data[3];
		if(datalength>2048 || datalength <=0)
		{
			sendError();
			return;
		}
		success_getLength_flag=1;
	}
	else{}
}
ErrorStatus verify(u32  addr, u8 * BinBuf, u16 datalength)
	{
	u8 * p=(u8 *)addr;
	while(datalength--)
		{
		if((* p++) != (*BinBuf++))return ERROR;
		}
	return SUCCESS;
	}

void MCU_upgrade(void)              
{
	u8 Res;	
	if(USART_GetFlagStatus(USART3, USART_FLAG_RXNE) != RESET)
	{
		USART_ClearFlag(USART3, USART_FLAG_RXNE | USART_FLAG_ORE);
		Res = USART_ReceiveData(USART3);
		USART_RX_BUF[USART_RX_CNT] = Res;
		USART_RX_CNT++;
		
		main_delay = TIME_IAP;
		mode =3;
/*---------------------------------------------------------------------
		protocol-----|#|remainSend|#|length|data[]|CRC_check
---------------------------------------------------------------------*/		
		if(success_getLength_flag == 1)
		{
			if(j<datalength)
				BIN_BUFF[j++] = Res;   //缓存
			else if(j==datalength)   
			{
				crc_check = xCal_crc(BIN_BUFF, datalength);
				if(crc_check == USART_RX_BUF[5+datalength]) //check OK 
				{
					j = 0;
					success_getLength_flag = 0;
					data_flag = 0;
					//=======================================
					iap_write_appbin(addrCur, BIN_BUFF, datalength);//将读取的数据写入Flash中
					if(verify(addrCur, BIN_BUFF, datalength) != SUCCESS)
						{
						printf("\r\n Firmware Update fail .\r\n");		
						LED_OFF();
						delay0 = TIME_100ms;
						while(delay0--);
						PowerSupply_Off();
						return;
						}
					addrCur += 2048;                                //flash address offset 2K   
					//=======================================
					receiveTime++;
					printf("Part %d :",receiveTime);	
					sendOK();					
					if(remainSend==0)
					{
						printf("\r\n Firmware Update finished .\r\n");		
						LED_blue();
						delay0 = TIME_100ms;
						while(delay0--);
						PowerSupply_Off();
					} 
				}
				else
				{
					sendError();
					printf("Failed the %d time \r\n",receiveTime);
				}
			}
		}
		else
		{
			getLength(USART_RX_BUF, USART_RX_CNT - 1);   //
		}		
	}		
  } 

void Uart_BT_TestMode(void)
	{
	u8 Res;
	if(USART_GetFlagStatus(USART2, USART_FLAG_RXNE) != RESET)
		{
		Res = USART_ReceiveData(USART2);
		USART_ClearFlag(USART2, USART_FLAG_RXNE | USART_FLAG_ORE);
		while((USART3->SR & USART_FLAG_TC)==0);  
		USART3->DR = (u8) Res;	 
		}
	if(USART_GetFlagStatus(USART3, USART_FLAG_RXNE) != RESET)
		{
		Res = USART_ReceiveData(USART3);
		USART_ClearFlag(USART3, USART_FLAG_RXNE | USART_FLAG_ORE);
		while((USART2->SR & USART_FLAG_TC)==0);  
		USART2->DR = (u8) Res;	   
		main_delay = TIME_BT;
		}
	}

void MCU_upgradeMode(void)
	{
	printf("\r\n Bootloader V2.02 \r\n");
	printf("MCU upgrade mode \r\n");
	mode =0;
	main_delay = TIME_IAP;
	GPIO_upgrade_mode();
	PowerSupply_On();
	LED_blue();		
	USART_RX_STA=0; 		//接收状态标记	  
	USART_RX_CNT=0; 		//接收的字节数
	}

void BT_testMode(void)
	{
	printf("\r\n Bootloader V2.02 \r\n");
	printf("BlueTouth test mode \r\n");
	mode =1;
	main_delay = TIME_BT;
	GPIO_BT_mode(1);
	uart2_init(115200);
	PowerSupply_On();
	LED_blue();				
	}

void BT_upgradeMode(void)
	{
	printf("\r\n Bootloader V2.02 \r\n");
	printf("BlueTouth upgrade mode \r\n");
	mode =2;
	main_delay = TIME_BT;
	GPIO_BT_mode(2);
	uart2_init(115200);
	PowerSupply_On();
	LED_pink();				
	}



void LED_display(void)
	{
	static u8 time_led,flag_led;;
	if(++time_led >=10)
		{
		time_led = 0;
		switch(mode)
			{
			case 1:
				if(flag_led)
					LED_OFF();
				else
					LED_blue();
			break;
			case 2:
				if(flag_led)
					LED_blue();
				else
					LED_pink();
			break;
			case 3:
				if(flag_led)
					LED_OFF();
				else
					LED_pink();
			break;
			default:
				LED_blue();
			break;
			}
		flag_led = (flag_led )? 0 :1;
		}
	}

void key_detect(void)
	{
	u8  key0;
	static u8  key1;
	static u8  key2=1;
	key0 = PowerSwitch;
	if(key0 != key1)
		{
		key1=key0;
		}
	else if(key0 != key2)
		{
		if(key2 ==0)
			{
			mode++;
			if(mode == 2)
				{
				BT_upgradeMode();
				}
			else if(mode ==1)
				{
				BT_testMode();
				}
			else
				{
				mode = 0;
				MCU_upgradeMode();
				}
			}
		key2 = key0;
		}
	}

typedef  void (*pFunction)(void);
void Jumpto_APP(void)
	{
	pFunction JumpToApplication;
	uint32_t JumpAddress;
	/* Test if user code is programmed starting from address "APPLICATION_ADDRESS" */
	if (((*(__IO uint32_t*)FLASH_APP1_ADDR) & 0x2FFE0000 ) == 0x20000000)
  		{
		printf("\r\n Run app \r\n");
		USART_Cmd(USART2, DISABLE);   //失能串口 ，防止跳转到App程序时死机		
		USART_ClearFlag(USART2, USART_FLAG_RXNE | USART_FLAG_ORE);
		USART_Cmd(USART3, DISABLE);   //失能串口 ，防止跳转到App程序时死机		
		USART_ClearFlag(USART3, USART_FLAG_RXNE | USART_FLAG_ORE);
		RCC_DeInit();
		LED_OFF();
		__disable_irq(); 
		/* Jump to user application */
		JumpAddress = *(__IO uint32_t*) (FLASH_APP1_ADDR + 4);
		JumpToApplication = (pFunction) JumpAddress;
		/* Initialize user application's Stack Pointer */
		__set_MSP(*(__IO uint32_t*) FLASH_APP1_ADDR);
		JumpToApplication();
		}
	else
		{
		LED_red();
		printf("\r\n Run app error \r\n");
		}
	}


int main(void)
	{
	RCC_Configuration();
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA| RCC_APB2Periph_GPIOB|  RCC_APB2Periph_GPIOD |   RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO, ENABLE);
	uart3_init(115200);	
	USART_ClearFlag(USART3, USART_FLAG_RXNE | USART_FLAG_ORE);
	
#ifdef GPIO_BootPin
	GPIO_InitStructure.GPIO_Pin = GPIO_BootPin; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIO_Boot, &GPIO_InitStructure);
	delay0 = TIME_100ms;
	while(delay0--);
	if((GPIO_Boot ->IDR & GPIO_BootPin)==0)Jumpto_APP();
#endif

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
#ifdef REMOTE
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;  // power
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOE,GPIO_Pin_1);
#else
	GPIO_InitStructure.GPIO_Pin = (GPIO_Pin_0 | GPIO_Pin_1);//LED
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOA, (GPIO_Pin_0 | GPIO_Pin_1));
		
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5; // power
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOD,GPIO_Pin_5);
#endif
	retry:
#ifdef GPIO_BootPin
	main_delay = TIME_1s*20;
#else
	main_delay = TIME_1s;
#endif
#ifndef REMOTE
	key_detect();
	key_detect();
	key_detect();
#endif
	/////////
	MCU_upgradeMode();
	while(main_delay)
		{
		key_detecTime = TIME_25ms;
		while(key_detecTime)
			{
			key_detecTime--;
			switch(mode)
				{
				default:
					MCU_upgrade();
				break;
#ifndef REMOTE
				case 1:
				case 2:
					Uart_BT_TestMode();
				break;
#endif
				}
			}
		main_delay--;
#ifndef REMOTE
		key_detect();
#endif
		LED_display();
		}
	Jumpto_APP();
	delay0 = TIME_100ms;
	while(delay0--);
	goto retry;
	}

