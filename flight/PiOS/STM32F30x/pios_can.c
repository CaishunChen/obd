
#include "pios_can.h"

#define CAN_RX_PIN GPIO_Pin_11
#define CAN_TX_PIN GPIO_Pin_12
//#define CAN_RX_PIN GPIO_Pin_8
//#define CAN_TX_PIN GPIO_Pin_9

static xQueueHandle xCAN_receive_queue0;
static xQueueHandle xCAN_receive_queue1;

void can_init(void)
{

  GPIO_InitTypeDef GPIO_InitStructure =
    {
      .GPIO_Pin   = CAN_RX_PIN | CAN_TX_PIN,
      .GPIO_Speed = GPIO_Speed_50MHz,
      .GPIO_Mode  = GPIO_Mode_AF,
    };
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_9);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_9);
  //GPIO_Init(GPIOB, &GPIO_InitStructure);
  //GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_9);
  //GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_9);

//  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;	// default is un-pulled input
//  GPIO_InitStructure.GPIO_Pin = (GPIO_Pin_11 | GPIO_Pin_12);				// leave USB D+/D- alone
//  GPIO_Init(GPIOA, &GPIO_InitStructure);


  CAN_InitTypeDef CAN_InitStructure;
  CAN_StructInit(&CAN_InitStructure);

  /* CAN register init */
  CAN_DeInit(CAN1);

  /* CAN cell init */
  CAN_InitStructure.CAN_TTCM = DISABLE;
  CAN_InitStructure.CAN_ABOM = DISABLE;
  CAN_InitStructure.CAN_AWUM = DISABLE;
  CAN_InitStructure.CAN_NART = ENABLE;
  CAN_InitStructure.CAN_RFLM = DISABLE;
  CAN_InitStructure.CAN_TXFP = DISABLE;
  
  CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
  //CAN_InitStructure.CAN_Mode = CAN_Mode_LoopBack;
  CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
  CAN_InitStructure.CAN_BS1 = CAN_BS1_8tq;
  //CAN_InitStructure.CAN_BS1 = CAN_BS1_5tq;
  CAN_InitStructure.CAN_BS2 = CAN_BS2_7tq;
  //CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq;
  CAN_InitStructure.CAN_Prescaler = 9;
  CAN_Init(CAN1, &CAN_InitStructure); //36Mhz 250Kbps

  CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
  CAN_ITConfig(CAN1, CAN_IT_FMP1, ENABLE);

  NVIC_InitTypeDef NVIC_InitStructure =
    {
      .NVIC_IRQChannel = CAN1_RX1_IRQn,
      .NVIC_IRQChannelPreemptionPriority = 7,
      .NVIC_IRQChannelSubPriority = 0,
      .NVIC_IRQChannelCmd = ENABLE,
    };
  NVIC_Init(&NVIC_InitStructure);

 /* CAN filter init */
  CAN_FilterInitTypeDef  CAN_FilterInitStructure;
  CAN_FilterInitStructure.CAN_FilterNumber=0;
  CAN_FilterInitStructure.CAN_FilterMode=CAN_FilterMode_IdMask;
  CAN_FilterInitStructure.CAN_FilterScale=CAN_FilterScale_32bit;
  CAN_FilterInitStructure.CAN_FilterIdHigh=0x0000;
  CAN_FilterInitStructure.CAN_FilterIdLow=0x0000;
  CAN_FilterInitStructure.CAN_FilterMaskIdHigh=0x0000;
  CAN_FilterInitStructure.CAN_FilterMaskIdLow=0x0000;  
  CAN_FilterInitStructure.CAN_FilterFIFOAssignment=CAN_FilterFIFO1;
  CAN_FilterInitStructure.CAN_FilterActivation=ENABLE;
  CAN_FilterInit(&CAN_FilterInitStructure);

  xCAN_receive_queue0 = xQueueCreate(10, sizeof(CanRxMsg));
  xCAN_receive_queue1 = xQueueCreate(10, sizeof(CanRxMsg));

}

/*       BS1 BS2 SJW Pre
+1M:      5   3   1   4
+500k:    7   4   1   6
+250k:    9   8   1   8
+125k:    9   8   1   16
+100k:    9   8   1   20 */
void can_set_baud(int kbps)
{
  CAN_InitTypeDef CAN_InitStructure;
  CAN_StructInit(&CAN_InitStructure);

  /* CAN cell init */
  CAN_InitStructure.CAN_TTCM = DISABLE;
  CAN_InitStructure.CAN_ABOM = DISABLE;
  CAN_InitStructure.CAN_AWUM = DISABLE;
  CAN_InitStructure.CAN_NART = ENABLE;
  CAN_InitStructure.CAN_RFLM = DISABLE;
  CAN_InitStructure.CAN_TXFP = DISABLE;
  
  CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
  if(kbps == 500) //500kbps
  {
  	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
  	CAN_InitStructure.CAN_BS1 = CAN_BS1_5tq;
  	CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq;
  	CAN_InitStructure.CAN_Prescaler = 9;
  }else if(kbps == 250)//250kbps
  {
  //CAN_InitStructure.CAN_Mode = CAN_Mode_LoopBack;
  	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
  	CAN_InitStructure.CAN_BS1 = CAN_BS1_8tq;
  	CAN_InitStructure.CAN_BS2 = CAN_BS2_7tq;
  	CAN_InitStructure.CAN_Prescaler = 9;
  }else if(kbps == 125)
  {
  	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
  	CAN_InitStructure.CAN_BS1 = CAN_BS1_9tq;
  	CAN_InitStructure.CAN_BS2 = CAN_BS2_8tq;
  	CAN_InitStructure.CAN_Prescaler = 16;
  }
  CAN_Init(CAN1, &CAN_InitStructure); //36Mhz 250Kbps
}

void can_set_filters(can_t* can_filters)
{
  for (int i = 0; i < can_filters->n_filters; i++)
    CAN_FilterInit(can_filters->CAN_filterInit + i);
}

void CAN1_RX1_IRQHandler()
{
  CanRxMsg RxMessage;
  portBASE_TYPE resch = pdFALSE;
  CAN_Receive(CAN1, CAN_FIFO1, &RxMessage);
  xQueueSendFromISR(xCAN_receive_queue1, &RxMessage, &resch);
  portEND_SWITCHING_ISR(resch);
}

void can_send_msg(uint8_t data[8], uint32_t TxID,int len)
{
  static CanTxMsg TxMessage;

  // Values between 0 to 0x7FF
  if(IS_CAN_STDID(TxID))
  {
  	TxMessage.StdId = TxID;
  	// Specifies the type of identifier for the message that will be transmitted
  	TxMessage.IDE = CAN_ID_STD;
  }else if(IS_CAN_EXTID(TxID))
  {
	TxMessage.ExtId = TxID;
	TxMessage.IDE = CAN_ID_EXT;
  }
  // Specifies the type of frame for the message that will be transmitted
  TxMessage.RTR = CAN_RTR_DATA;
  // Specifies the length of the frame that will be transmitted
  TxMessage.DLC = len;

  for (int i = 0; i < 8; i++)
    TxMessage.Data[i] = data[i];

  CAN_Transmit(CAN1,&TxMessage);
}

portBASE_TYPE can_receive_msgFIFO0(CanRxMsg* RxMessage)
{
  return xQueueReceive(xCAN_receive_queue0, RxMessage, portMAX_DELAY);
}

portBASE_TYPE can_receive_msgFIFO1(CanRxMsg* RxMessage)
{
  return xQueueReceive(xCAN_receive_queue1, RxMessage, portMAX_DELAY);
}


