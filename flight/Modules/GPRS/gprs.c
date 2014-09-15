#include "openpilot.h"
#include "sim800.h"

#define STACK_SIZE_BYTES 1024
#define TASK_PRIORITY (tskIDLE_PRIORITY+1)

static xTaskHandle GPRSTaskHandle;
static char gprsBuffer[64];
static void GPRSTask(void *parameters)
{
	/* Handle usart -> vcp direction */
	while (1) {
		uint32_t rx_bytes;
		char *s = NULL;

		//printf("GPRS recv");
		memset(gprsBuffer,0,sizeof(gprsBuffer));
		rx_bytes = PIOS_COM_ReceiveBuffer(PIOS_COM_GPRS, (uint8_t *)gprsBuffer, sizeof(gprsBuffer), 500);
		if (rx_bytes > 0) {
			printf("%s",gprsBuffer);
			if(NULL != strstr(gprsBuffer,"RING"))
			{
				SIM800_answer();
			}else if(NULL != (s = strstr(gprsBuffer,"+CMTI: \"SM\""))){
				int messageIndex = atoi(s+12);
				char message[256];
				SIM800_readSMS(messageIndex,message,sizeof(message));	
				printf("SMS: %s",message);
			}	
		}
	}
}


static int32_t GPRSInitialize(void)
{
	printf("GPRSInitialize");
	PIOS_LED_On(7); //power on gprs
	// Start tasks
	xTaskCreate(GPRSTask, (signed char *)"GPRSTask", STACK_SIZE_BYTES/4, NULL, TASK_PRIORITY, &GPRSTaskHandle);
	return 0;
}


MODULE_INITCALL(GPRSInitialize, 0)

