 #include "stm32f30x.h"  
   
/**************************************************************************************/  
    
void RCC_Configuration(void)  
{  
  /* Enable GPIO clock */  
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);  
   
  /* Enable USART clock */  
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);  
}  
   
/**************************************************************************************/  
    
void GPIO_Configuration(void)  
{  
  GPIO_InitTypeDef GPIO_InitStructure;  
   
  /* Connect PA9 to USART1_Tx */  
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_7);  
   
  /* Connect PA10 to USART1_Rx */  
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_7);  
   
  /* Configure USART Tx as alternate function push-pull */  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;  
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;  
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;  
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  
  GPIO_Init(GPIOA, &GPIO_InitStructure);  
   
  /* Configure USART Rx as alternate function push-pull */  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;  
  GPIO_Init(GPIOA, &GPIO_InitStructure);  
}  
   
/**************************************************************************************/  
   
void USART1_Configuration(void)  
{  
  USART_InitTypeDef USART_InitStructure;  
   
  /* USART resources configuration (Clock, GPIO pins and USART registers) ----*/  
  /* USART configured as follow: 
        - BaudRate = 115200 baud 
        - Word Length = 8 Bits 
        - One Stop Bit 
        - No parity 
        - Hardware flow control disabled (RTS and CTS signals) 
        - Receive and transmit enabled 
  */  
  USART_InitStructure.USART_BaudRate = 115200;  
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;  
  USART_InitStructure.USART_StopBits = USART_StopBits_1;  
  USART_InitStructure.USART_Parity = USART_Parity_No;  
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;  
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;  
   
  /* USART configuration */  
  USART_Init(USART1, &USART_InitStructure);  
   
  /* Enable USART */  
  USART_Cmd(USART1, ENABLE);  
}   

void printchar(int ch)
{

    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // Wait for Empty  
    USART_SendData(USART1, ch); // Send 'I'   
}
