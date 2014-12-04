#ifndef PIOS_CAN_H
#define PIOS_CAN_H

#include <pios.h>
#include <pios_stm32.h>

typedef struct
{
  CAN_FilterInitTypeDef* CAN_filterInit;
  int n_filters;
} can_t;

void can_init(void);
void can_set_baud(int kbps);
void can_set_filters(can_t* can_filters);
void can_send_msg(uint8_t data[8], uint32_t TxID, int len);
portBASE_TYPE can_receive_msgFIFO0(CanRxMsg* RxMessage);
portBASE_TYPE can_receive_msgFIFO1(CanRxMsg* RxMessage, int timeout);

#endif
