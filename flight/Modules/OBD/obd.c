#include "openpilot.h"
#include "pios_can.h"

//#if defined(PIOS_INCLUDE_DEBUG_CONSOLE)
#define DEBUG_MSG(format, ...) PIOS_COM_SendFormattedString(PIOS_COM_DEBUG, format, ## __VA_ARGS__)
//#else
//#define DEBUG_MSG(format, ...)
//#endif

#define STACK_SIZE 512
#define TASK_PRIORITY (tskIDLE_PRIORITY+1)
// Private types

// Private variables
static xTaskHandle taskHandle;
//static xTaskHandle taskHandle1;
extern int show_can_id;
// Private functions
static void obdTask(void *parameters);
extern void shell_init(void);
//static void cansendTask(void *parameters);

static unsigned char auto_ids[0x6ff];
int32_t OBDInitialize()
{
	//unsigned char canData[8];
#if 1 
	DEBUG_MSG("OBDInitialize\n");
	CAN_FilterInitTypeDef CAN_filterInit[] = {
		{
			// Non-vital message filter
			.CAN_FilterActivation     = ENABLE,
			.CAN_FilterFIFOAssignment = CAN_FilterFIFO1,
			.CAN_FilterIdHigh         = 0x0000,
			.CAN_FilterIdLow          = 0x0000,
			.CAN_FilterMaskIdHigh     = 0x0000,
			.CAN_FilterMaskIdLow      = 0x0000,
			.CAN_FilterMode           = CAN_FilterMode_IdMask,
			.CAN_FilterNumber         = 0,
			.CAN_FilterScale          = CAN_FilterScale_32bit,
		}
	};

	can_t can_filters = {
		.CAN_filterInit = CAN_filterInit,
		.n_filters = 1,
	};

	memset(auto_ids,0,sizeof(auto_ids));
	can_init();
	can_set_filters(&can_filters);

//	canData[0] = 0x55;
//	canData[1] = 0xAA;
//	can_send_msg(canData,0x11);
#endif
	// Start main task
	xTaskCreate(obdTask, (signed char *)"OBDThread", STACK_SIZE, NULL, TASK_PRIORITY, &taskHandle);
//	xTaskCreate(cansendTask, (signed char *)"cansend", STACK_SIZE, NULL, TASK_PRIORITY, &taskHandle1);
	shell_init();

	DEBUG_MSG("OBDInitialize end\n");
	return 0;
}

/**
 * Module thread, should not return.
 */
static void obdTask(void *parameters)
{

	// Main task loop
	while (1) {

		CanRxMsg RxMessage;
  		can_receive_msgFIFO1(&RxMessage);
		if(CAN_Id_Standard == RxMessage.IDE)
		{
			if(show_can_id == 0)
			{
				auto_ids[RxMessage.StdId] = 1;
  				DEBUG_MSG("%x: ",RxMessage.StdId);
			}
			if(show_can_id == 0x800)
			{
				if(auto_ids[RxMessage.StdId] == 0)
				{
  					DEBUG_MSG("%x: ",RxMessage.StdId);
  					for (int i = 0; i < 8; i++)
    						DEBUG_MSG("%02x ", RxMessage.Data[i]);
					DEBUG_MSG("\r\n");
				}
			}	
			if(RxMessage.StdId == show_can_id)
			{
  				DEBUG_MSG("%x: ",RxMessage.StdId);
  				for (int i = 0; i < 8; i++)
    					DEBUG_MSG("%02x ", RxMessage.Data[i]);
				DEBUG_MSG("\r\n");
			}
		}
		else
		{
  			DEBUG_MSG("%x: ",RxMessage.ExtId);
  			for (int i = 0; i < 8; i++)
    				DEBUG_MSG("%02x ", RxMessage.Data[i]);
			DEBUG_MSG("\r\n");
		}
	}
}

#if 0
static void cansendTask(void *parameters)
{
	portTickType lastSysTime;
	unsigned char canData[8];
	unsigned char id = 0;

	DEBUG_MSG("cansendTask\r\n");
	// Main task loop
	lastSysTime = xTaskGetTickCount();
	while (1) {
		canData[0] = 0x55;
		canData[1] = 0xAA;
		can_send_msg(canData,id);
		DEBUG_MSG("can_send_msg %d\r\n",id);
		id++;
		vTaskDelayUntil(&lastSysTime, 1000 / portTICK_RATE_MS);
	}
}
#endif
MODULE_INITCALL(OBDInitialize, 0)

