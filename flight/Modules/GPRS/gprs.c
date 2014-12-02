#include "openpilot.h"
#include "sim800.h"

#define STACK_SIZE_BYTES 1024
#define TASK_PRIORITY (tskIDLE_PRIORITY+1)

static xTaskHandle GPRSTaskHandle;
static char line[1024];
static int line_index = 0;
static void GPRSTask(void *parameters)
{
	PIOS_LED_On(7); //power on gprs
	/* Handle usart -> vcp direction */
	while (1) {
		uint32_t rx_bytes;
		char *s = NULL;
		uint8_t c;

		//printf("GPRS recv");
		//memset(gprsBuffer,0,sizeof(gprsBuffer));
		rx_bytes = PIOS_COM_ReceiveBuffer(PIOS_COM_GPRS, &c, 1, 500);
		if (rx_bytes > 0) {
			line[line_index++] = c;
			if(c == '\n')
			{
				line[line_index++] = 0;
				line_index = 0;	
				printf("%s",line);
				if(NULL != strstr(line,"RING"))
				{
					SIM800_answer();
				}else if(NULL != (s = strstr(line,"+CMTI: \"SM\""))){
					int messageIndex = atoi(s+12);
					char message[256];
					SIM800_readSMS(messageIndex,message,sizeof(message));	
					printf("SMS: %s",message);
				}else if(NULL != (s = strstr(line,"+BTSPPDATA: 1,"))){//+BTSPPDATA: 1,10,123456789
					//printf("%s",str);
					char *l = strsep(&s,",");
					int len = strtol(l, NULL, 10);
					printf("%d:%s",len,s);
				}
			}	
		}
	}
}


static int32_t GPRSInitialize(void)
{
	printf("GPRSInitialize");
	// Start tasks
	xTaskCreate(GPRSTask, (signed char *)"GPRSTask", STACK_SIZE_BYTES/4, NULL, TASK_PRIORITY, &GPRSTaskHandle);
	return 0;
}


MODULE_INITCALL(GPRSInitialize, 0)

