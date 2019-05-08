/*--------------------------------------------------------------------------------------
@file:    IAP_Serial
@version: V1.02
@author:  dong
@data:    2019_04_24
@Introduction : Unlock flash   load_app from serial 
--------------------------------------------------------------------------------------*/
#include "usart.h"   
#include "led.h"  
#include "stmflash.h" 
#include "iap.h"  
#include <stdio.h> 

#define REMOTE  /* default DOGG*/

// 最少要多留2页空间用于存放SN等数据,程序空间必须位于页面开始位置。
#define BOOT_SIZE		0x8000	

//#define UART1
//#define UART2
#define UART3

#define FLASH_APP1_ADDR		(0x08000000+BOOT_SIZE)	//第一个应用程序起始地址(存放在FLASH)

#define TIME_WAIT	(uint32_t) 1060000 // 1s
#define TIME_IAP		(uint32_t) 5000000
#define TIME_100ms	(uint32_t) 720000	//0.1s

#ifdef UART1
#define USARTx USART1
#define RCC_APBPeriph_GPIOx 	RCC_APB2Periph_GPIOA
#define RCC_APBPeriph_USARTx 	RCC_APB2Periph_USART1
#define GPIO_UART_TX 			GPIOA
#define GPIO_UART_PinTX 		GPIO_Pin_9
#define GPIO_UART_RX 			GPIOA
#define GPIO_UART_PinRX 		GPIO_Pin_10
#endif
#ifdef UART2
#define USARTx USART2
#define RCC_APBPeriph_GPIOx 	RCC_APB2Periph_GPIOA
#define RCC_APBPeriph_USARTx 	RCC_APB1Periph_USART2
#define GPIO_UART_TX			GPIOA
#define GPIO_UART_PinTX 		GPIO_Pin_2
#define GPIO_UART_RX 			GPIOA
#define GPIO_UART_PinRX 		GPIO_Pin_3
#endif
#ifdef UART3
#define USARTx USART3
#define RCC_APBPeriph_GPIOx 	RCC_APB2Periph_GPIOB
#define RCC_APBPeriph_USARTx 	RCC_APB1Periph_USART3
#define GPIO_UART_TX 			GPIOB
#define GPIO_UART_PinTX 		GPIO_Pin_10
#define GPIO_UART_RX 			GPIOB
#define GPIO_UART_PinRX 		GPIO_Pin_11
#endif


u32 delay;

#ifdef REMOTE
#define PowerSupply_On()	{GPIO_SetBits(GPIOE,GPIO_Pin_1);}
#define PowerSupply_Off(){GPIO_ResetBits(GPIOE,GPIO_Pin_1);}
#define LED_red()	
#define LED_blue()	
#define LED_pink()	
#define LED_OFF()	
#else 
#define PowerSupply_On() {GPIO_SetBits(GPIOD,GPIO_Pin_5);}		//MCU_SYSTEM
#define PowerSupply_Off() {GPIO_ResetBits(GPIOD,GPIO_Pin_5);}
#define LED_red()	{GPIO_SetBits(GPIOA,GPIO_Pin_1);GPIO_ResetBits(GPIOA,GPIO_Pin_0);}
#define LED_blue()	{GPIO_SetBits(GPIOA,GPIO_Pin_0);GPIO_ResetBits(GPIOA,GPIO_Pin_1);}
#define LED_pink()	{GPIO_SetBits(GPIOA,GPIO_Pin_1|GPIO_Pin_1);}
#define LED_OFF()	{GPIO_ResetBits(GPIOA,GPIO_Pin_0|GPIO_Pin_1);}
#endif

int fputc(int ch, FILE *f)
	{ 	
	while((USARTx->SR & USART_FLAG_TC)==0);  
	USARTx->DR = (u8) ch;      
	return ch;
	}

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

void initBord_GPIO(void)
	{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA| RCC_APB2Periph_GPIOB|  RCC_APB2Periph_GPIOD |   RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO, ENABLE);
	//GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);
#ifdef REMOTE
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOE,GPIO_Pin_1);
#else
	GPIO_InitStructure.GPIO_Pin = (GPIO_Pin_0 | GPIO_Pin_1);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOA, (GPIO_Pin_0 | GPIO_Pin_1));
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOD,GPIO_Pin_5);
#endif
	}


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

void uart_init(u32 bound)
	{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	RCC_AHBPeriphClockCmd(RCC_APBPeriph_GPIOx,ENABLE); 
	RCC_APB1PeriphClockCmd(RCC_APBPeriph_USARTx,ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_UART_PinTX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_UART_TX, &GPIO_InitStructure);	  
	GPIO_InitStructure.GPIO_Pin = GPIO_UART_PinRX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIO_UART_RX, &GPIO_InitStructure);
	USART_InitStructure.USART_BaudRate = bound;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USARTx, &USART_InitStructure); 
	USART_Cmd(USARTx, ENABLE); 
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

void USART_rx(void)              
{
	u8 Res;	
	if(USART_GetFlagStatus(USARTx, USART_FLAG_RXNE) != RESET)
	{
		USART_ClearFlag(USARTx, USART_FLAG_RXNE | USART_FLAG_ORE);
		Res = USART_ReceiveData(USARTx);
		USART_RX_BUF[USART_RX_CNT] = Res;
		USART_RX_CNT++;
		
		delay = TIME_IAP;
		LED_pink();
		PowerSupply_On();
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
						delay = TIME_100ms;
						while(delay--);
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
						delay = TIME_100ms;
						while(delay--);
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



typedef  void (*pFunction)(void);
void Jumpto_APP(void)
	{
	pFunction JumpToApplication;
	uint32_t JumpAddress;
	/* Test if user code is programmed starting from address "APPLICATION_ADDRESS" */
	if (((*(__IO uint32_t*)FLASH_APP1_ADDR) & 0x2FFE0000 ) == 0x20000000)
  		{
		printf("\r\n Run app \r\n");
		USART_Cmd(USARTx, DISABLE);   //失能串口 ，防止跳转到App程序时死机		
		USART_ClearFlag(USARTx, USART_FLAG_RXNE | USART_FLAG_ORE);
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
	initBord_GPIO();
	uart_init(115200);
	USART_ClearFlag(USARTx, USART_FLAG_RXNE | USART_FLAG_ORE);
	retry:
	printf("\r\n Bootloader V1.02 \r\n");
	LED_blue();
	USART_RX_STA=0; 		//接收状态标记	  
	USART_RX_CNT=0; 		//接收的字节数
	delay = TIME_WAIT;
	while(delay)
		{
		USART_rx();
		delay--;
		}
	Jumpto_APP();
	delay = TIME_100ms;
	while(delay--);
	goto retry;
	}

