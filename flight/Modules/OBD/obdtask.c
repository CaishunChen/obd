#include "openpilot.h"
#include "pios_can.h"
#include "obd.h"

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
int32_t obdInit()
{
	//unsigned char canData[8];
#if 1 
	DEBUG_MSG("OBDInitialize\n");

	OBDInitialize();
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
int stop_obdTask = 0;
extern int g_debug_level;
static void obdTask(void *parameters)
{

	// Main task loop
	while (1) {
		int ret;
	
		if(g_debug_level > 2)
			printf("stop_obdTask=%d\r\n",stop_obdTask);
		if(stop_obdTask == 1)
		{
			PIOS_DELAY_WaitmS(1000);
//			stop_obdTask = 2;
		}else
		{
			CanRxMsg RxMessage;
			ret = can_receive_msgFIFO1(&RxMessage, 1000);
			if(g_debug_level > 2)
				printf("can_recv: %d\r\n",ret);
			if(ret > 0)
			{
				if(CAN_Id_Standard == RxMessage.IDE)
				{
					if(show_can_id == 0)
					{
						auto_ids[RxMessage.StdId] = 1;
						if(g_debug_level > 0)
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
					if((RxMessage.StdId >= 0x7e8) && (RxMessage.StdId <= 0x7ef))
					{
						if(RxMessage.Data[0] == 4 && RxMessage.Data[1] == 0x41 && RxMessage.Data[2] == 0x0c)//Engine RPM
						{
							uint32_t engineRPM = (RxMessage.Data[3]*256+RxMessage.Data[4])/4;
							OBDengineRPMSet( &engineRPM );
							//printf("engineRPM: %d\n", engineRPM);
						}else if(RxMessage.Data[0] == 3 && RxMessage.Data[1] == 0x41 && RxMessage.Data[2] == 0x0d)//Vehicle speed
						{
							uint32_t vechicleSpeed = RxMessage.Data[3];
							OBDvehicleSpeedSet( &vechicleSpeed);
							//printf("vechicleSpeed: %d\n",vechicleSpeed);
						}
					}
				}
				else
				{
					DEBUG_MSG("%x: ",RxMessage.ExtId);
					for (int i = 0; i < 8; i++)
						DEBUG_MSG("%02x ", RxMessage.Data[i]);
					DEBUG_MSG("\r\n");
				}
			}else{
				unsigned char cmd[8];
				cmd[0] = 2;
				cmd[1] = 0x01;
				cmd[2] = 0x0c;//Engine RPM
				can_send_msg(cmd,0x7df,8);
				cmd[2] = 0x0d;//Vehicle speed
				can_send_msg(cmd,0x7df,8);
			}
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
MODULE_INITCALL(obdInit, 0)

