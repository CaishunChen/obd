#include "openpilot.h"
#include "sim800.h"
#include "gpsposition.h"
#include "accels.h"
#include "gyros.h"
#include "magnetometer.h"
#include "attitudeactual.h"
#include "obd.h"

#define STACK_SIZE_BYTES 2048 
#define TASK_PRIORITY (tskIDLE_PRIORITY+1)
typedef enum {
	NETWORK_ZERO,
	NETWORK_REG,
	NETWORK_ATTACHED,
	NETWORK_CONNECTING,
	NETWORK_CONNECTED,
	NETWORK_ID_SENDING,
	NETWORK_ID_SENT,
	NETWORK_SENDING,
	NETWORK_SENT_OK,
	NETWORK_CLOSED,
	NETWORK_WAIT_SHUT,
	NETWORK_WAITING,
	NETWORK_ERROR = -1,
}NETWORK_STATE;

typedef enum{
	BLUETOOTH_ZERO = 0,
	BLUETOOTH_PAIRING = 1,
	BLUETOOTH_PAIRED = 2,
	BLUETOOTH_CONNECTING = 4,
	BLUETOOTH_CONNECTED = 8,
	BLUETOOTH_SPP_RECEIVED = 0x10,
	BLUETOOTH_SPP_SENDING = 0x20,
	BLUETOOTH_SPP_SENT_OK = 0x40,
}BLUETOOTH_STATE;

char *gprmcs[] = {
"$GPRMC,040141.000,A,3957.5162,N,11617.7944,E,0.31,35.39,071114,,,A*4A\r\n",
"$GPRMC,040203.000,A,3957.5404,N,11617.8729,E,23.82,62.90,071114,,,A*78\r\n",
"$GPRMC,040211.000,A,3957.5714,N,11617.9388,E,26.93,57.99,071114,,,A*7D\r\n",
"$GPRMC,040258.000,A,3957.6435,N,11618.0901,E,0.00,53.08,071114,,,A*4C\r\n",
"$GPRMC,040423.000,A,3957.6425,N,11618.1031,E,0.14,187.26,071114,,,A*7D\r\n",
"$GPRMC,040451.000,A,3957.7127,N,11618.1300,E,17.76,350.12,071114,,,A*42\r\n",
"$GPRMC,040516.000,A,3957.8806,N,11618.0987,E,15.76,345.72,071114,,,A*41\r\n",
"$GPRMC,040539.000,A,3958.0314,N,11618.0687,E,26.41,353.79,071114,,,A*44\r\n",
"$GPRMC,040638.000,A,3958.4834,N,11617.6965,E,41.07,355.80,071114,,,A*42\r\n",
"$GPRMC,040708.000,A,3958.8170,N,11617.6693,E,34.42,356.84,071114,,,A*47\r\n"
};
extern int g_debug_level;
extern char *get_cur_proto();
extern int stop_obdTask;
extern int obd2(char *str, int len, char *res);
extern void PIOS_USART_printStatics(int n);

int manual_upload = 0; //0 auto, 1 stop 2 start
int upload_intervala = 1000;

int upload_bytes = 0;

static GPSPositionData gpsPosition;
static GyrosData gyrosData;
static AccelsData accelsData;
static MagnetometerData magData;
static AttitudeActualData attitudeData;
static OBDData obdData;
static char upload[256];
static char spp_upload[64];
static char spp_cmd[64];
static xTaskHandle GPRSTaskHandle;
static char line[512];
static int line_index = 0;
static NETWORK_STATE network_state = NETWORK_ZERO;
static BLUETOOTH_STATE bluetooth_state = BLUETOOTH_ZERO;
static int at_ready = 0;
static int gps_status = 0;
static int sending_timeout = 0;
static int bt_sending_timeout = 0;
static char imei[16];
static char csq[32];
static char btrssi[32];

typedef enum {
	AT_0,
	AT_GSN,
}AT_CMD;
static AT_CMD cmd_is = AT_0;
static int received_n = 1;
static int received_bytes = 0;

static void GPRSTask(void *parameters)
{
	PIOS_LED_On(7); //power on gprs
	imei[0] = 0;
	imei[1] = 0;
	imei[2] = 0;
	csq[0]  = 0;
	btrssi[0] = 0;

	/* Handle usart -> vcp direction */
	while (1) {
		uint32_t rx_bytes;
		char *s = NULL;
		uint8_t c;
		
		//memset(gprsBuffer,0,sizeof(gprsBuffer));
		c = 0;
		rx_bytes = PIOS_COM_ReceiveBuffer(PIOS_COM_GPRS, &c, 1, 1000);
		if(g_debug_level >= 2)
			printf("r:%d %x\r\n",(int)rx_bytes,c);
		if (rx_bytes > 0) {
			received_bytes++;
			received_n = 0;
			line[line_index++] = c;
			line[line_index] = 0;
			if(c == '\n')
			{
				received_n = 1;

				line[line_index++] = 0; line_index = 0;	
				printf("%s",line);
				if(strstr(line,"BTPAIRING"))
				{
					bluetooth_state = BLUETOOTH_PAIRING;
				}else if(strstr(line,"+BTPAIR:"))
				{
					bluetooth_state = BLUETOOTH_PAIRED;
				}
				else if(strstr(line,"BTCONNECTING"))
				{
					if(strstr(line,"SPP"))
					{
						bluetooth_state = BLUETOOTH_CONNECTING;
					}
				}else if(strstr(line,"BTCONNECT: 1,"))
				{	
					
					if(strstr(line,"SPP"))
					{
						bluetooth_state = BLUETOOTH_CONNECTED;
						stop_obdTask = 1;
					}
				}
				else if(strstr(line,"+BTDISCONN"))
				{
					if(strstr(line,"SPP"))
					{
						bluetooth_state = BLUETOOTH_ZERO;
						stop_obdTask = 0;
					}
				}
				else if(NULL != strstr(line,"RING"))
				{
					manual_upload++;
					if(manual_upload > 2)
						manual_upload = 0;
					
					network_state = NETWORK_ERROR;
					SIM800_answer();
				}else if(NULL != (s = strstr(line,"+CMTI: \"SM\""))){
					int messageIndex = atoi(s+12); char message[256];
					SIM800_readSMS(messageIndex,message,sizeof(message));	
					printf("SMS: %s",message);
				}else if(NULL != (s = strstr(line,"+BTSPPDATA: 1,"))){//+BTSPPDATA: 1,10,123456789
					//printf("%s",str);
					s += 14;
					char *l = strsep(&s,",");
					int len = strtol(l, NULL, 10);
					if(len < sizeof(spp_cmd))
					{
						strcpy(spp_cmd,s);
						spp_cmd[len-1] = 0;
						bluetooth_state |= BLUETOOTH_SPP_RECEIVED;
					}
					printf("%d:%s",len,s);
				}else if(strstr(line,"+CREG:"))
				{
					if(strstr(line,"+CREG: 0,1"))
					{
						printf("creg ok");
						network_state = NETWORK_REG;
					}else
					{
						PIOS_DELAY_WaitmS(1000);
						SIM800_sendCmd("AT+CREG?\r\n");
					}
				}else if(strstr(line,"+CGATT:"))
				{
					if(strstr(line,"+CGATT: 1"))
					{
						printf("cgatt ok");
						network_state = NETWORK_ATTACHED;
					}else
					{
						PIOS_DELAY_WaitmS(1000);
						SIM800_sendCmd("AT+CGATT?\r\n");
					}
				}else if(strstr(line,"Call Ready"))
				{
					printf("call ready");
					at_ready = 3;
				}else if(strstr(line,"CONNECT OK"))
				{
					network_state = NETWORK_CONNECTED;
					printf("connect server ok");
				}else if(strstr(line,"CLOSED"))
				{
					printf("connection closed");
					network_state = NETWORK_ATTACHED; 
				}else if(strstr(line,"SEND OK"))
				{
					if(network_state == NETWORK_ID_SENDING)
					{
						network_state = NETWORK_ID_SENT;
					}else if(network_state == NETWORK_SENDING)
						network_state = NETWORK_SENT_OK;
					else if(bluetooth_state & BLUETOOTH_SPP_SENDING)
					{
						bluetooth_state  &= ~BLUETOOTH_SPP_SENDING;
						bluetooth_state |= BLUETOOTH_SPP_SENT_OK;	
					}
				}else if(strstr(line,"CLOSE OK"))
				{
					//network_state = NETWORK_CLOSED;
				}else if(strstr(line,"SHUT OK"))
				{
					network_state = NETWORK_ATTACHED;
				}else if(strstr(line,"PDP: DEACT"))
				{
				}
				else if(strstr(line,"ERROR"))
				{
					//network_state = NETWORK_ATTACHED;
				}
				else if(strstr(line,"CSQ"))
				{
					strcpy(csq,line);
				}
				else if(strstr(line,"BTRSSI"))
				{
					strcpy(btrssi,line);
				}
				else if(strstr(line,"ATE0"))
				{
					printf("ate0 ok");
					if(at_ready == 1)
					{
						at_ready = 2;
					}
				}
				else if(strstr(line,"AT+"))
				{
					if(at_ready == 3)
					{
						SIM800_sendCmd("ATE0\r\n");
					}	
				}
				else if(strstr(line,"OK"))
				{
					if(at_ready == 0)
					{
						at_ready = 1;
						printf("at ready,send ate0");
						SIM800_sendCmd("ATE0\r\n");
						//PIOS_DELAY_WaitmS(1000);
					}
				}
				else if(line[0] != '\r' && line[0] >= 0x30 && line[0] <= 0x39 && cmd_is == AT_GSN)
				{
					cmd_is = AT_0;
					line[15] = 0;
					strcpy(imei,line);	
				}
			//}else if(c == '>')
			}else if(strstr(line,"> "))
			{
				line[line_index-1] = '_';
				if(network_state == NETWORK_SENDING || network_state == NETWORK_ID_SENDING)
				{
					SIM800_sendCmd(upload);
				}
				else if(bluetooth_state & BLUETOOTH_SPP_SENDING)
				{
					printf("send: %s\r\n",spp_upload);
					SIM800_sendCmd(spp_upload);
				}
			}	
		}
		if(received_n || received_bytes < 10/*work around for the unknown bytes from bootup*/)
		{
			if(g_debug_level > 0)
			{
				printf("bluetooth_state: %x\r\n",bluetooth_state);
				printf("network_state: %x\r\n",network_state);
			}
			if(bluetooth_state & BLUETOOTH_SPP_SENDING)
			{
				if(xTaskGetTickCount() - bt_sending_timeout >= 10000)
				{
					bluetooth_state = 0;
					stop_obdTask = 0;
				}
				continue;
			}
			if(bluetooth_state == BLUETOOTH_PAIRING)
			{
				if(network_state != NETWORK_SENDING && network_state != NETWORK_ID_SENDING)
				{
					SIM800_sendCmd("AT+BTPAIR=1,1\r\n");
					PIOS_DELAY_WaitmS(1000);
					SIM800_sendCmd("AT+BTSTATUS?\r\n");
					bluetooth_state = BLUETOOTH_SPP_SENDING;	
					bt_sending_timeout = xTaskGetTickCount();
					continue;
				}
			}
			if(bluetooth_state == BLUETOOTH_CONNECTING)
			{
				if(network_state != NETWORK_SENDING && network_state != NETWORK_ID_SENDING)
				{
					SIM800_sendCmd("AT+BTACPT=1\r\n");
					PIOS_DELAY_WaitmS(1000);
					SIM800_sendCmd("AT+BTSTATUS?\r\n");
					bluetooth_state = BLUETOOTH_SPP_SENDING;	
					bt_sending_timeout = xTaskGetTickCount();
					continue;
				}
			}
			if((bluetooth_state  & BLUETOOTH_SPP_RECEIVED) && (!(bluetooth_state & BLUETOOTH_SPP_SENDING)) )
			{
				if(network_state != NETWORK_SENDING && network_state != NETWORK_ID_SENDING)
				{
					if(strstr(spp_cmd,"SIMCOMSPPFORAPP"))
					{
						char cmd[32];
						sprintf(spp_upload,">ELM327 - meeXing%s\r\n>",get_cur_proto());
						sprintf(cmd,"AT+BTSPPSEND=%d\r\n",strlen(spp_upload));
						SIM800_sendCmd(cmd);
						bluetooth_state = BLUETOOTH_SPP_SENDING;
					}
					else if(strstr(spp_cmd,"ATRV"))
					{
						char cmd[32];
						//SIM800_sendCmd("AT+BTRSSI=1\r\n");
						//sprintf(spp_upload,"12V%s%s\r\n>",csq,btrssi);
						sprintf(spp_upload,"12V%s\r\n>",csq);
						sprintf(cmd,"AT+BTSPPSEND=%d\r\n",strlen(spp_upload));
						SIM800_sendCmd(cmd);
						bluetooth_state = BLUETOOTH_SPP_SENDING;

					}
					else if(strstr(spp_cmd,"ATDP"))
					{
						char cmd[32];
						sprintf(spp_upload,"%s\r\n>",get_cur_proto());
						sprintf(cmd,"AT+BTSPPSEND=%d\r\n",strlen(spp_upload));
						SIM800_sendCmd(cmd);
						bluetooth_state = BLUETOOTH_SPP_SENDING;
					}
					else if(strstr(spp_cmd,"ATRSSI"))
					{
						char cmd[32];
						SIM800_sendCmd("AT+BTRSSI=1\r\n");
						sprintf(spp_upload,"%s%s\r\n>",csq,btrssi);
						sprintf(cmd,"AT+BTSPPSEND=%d\r\n",strlen(spp_upload));
						SIM800_sendCmd(cmd);
						bluetooth_state = BLUETOOTH_SPP_SENDING;

					}
					else if(strstr(spp_cmd,"AT"))
					{
						char cmd[32];
						sprintf(spp_upload,"OK\r\n>");
						sprintf(cmd,"AT+BTSPPSEND=%d\r\n",strlen(spp_upload));
						SIM800_sendCmd(cmd);
						bluetooth_state = BLUETOOTH_SPP_SENDING;
					}else{
						//OBD cmd
						char res[32];
						char cmd[32];
						res[0] = 0;
						obd2(spp_cmd,strlen(spp_cmd),res);
						if(strstr(spp_cmd,"0100"))
						{
							SIM800_sendCmd("AT+BTRSSI=1\r\n");
							sprintf(spp_upload,"%s%s\r\n>%s\r\n>",csq,btrssi,res);
						}else
							sprintf(spp_upload,"%s\r\n>",res);
						sprintf(cmd,"AT+BTSPPSEND=%d\r\n",strlen(spp_upload));
						SIM800_sendCmd(cmd);
						bluetooth_state = BLUETOOTH_SPP_SENDING;
					}
					bt_sending_timeout = xTaskGetTickCount();
					continue;
				}
			}
			GPSPositionGet(&gpsPosition);
			if(at_ready == 0)
			{
				printf("send at"); 
				SIM800_sendCmd("AT\r"); 
			} 
			if(network_state == NETWORK_ZERO && at_ready == 3)
			{
				printf("at+creg");
				SIM800_sendCmd("AT+CREG?\r\n");
				//PIOS_DELAY_WaitmS(1000);
				network_state = NETWORK_WAITING;
			}
			if(network_state == NETWORK_REG)
			{
				printf("send at+cgatt");
				SIM800_sendCmd("AT+CGATT?\r\n");	
				//PIOS_DELAY_WaitmS(1000);
				network_state = NETWORK_WAITING;
			}
			if(network_state == NETWORK_ATTACHED)
			{
				char cipstart[50];
				sprintf(cipstart, "AT+CIPSTART=\"TCP\",\"%s\",\"%d\"\r\n", "42.121.193.134", 5005);
				printf("connect to server");
				SIM800_sendCmd(cipstart);
				network_state = NETWORK_CONNECTING;
				sending_timeout = xTaskGetTickCount();
			}
			if(network_state == NETWORK_CONNECTED)
			{
				if(imei[0] == 0 && imei[1] == 0 && imei[2] == 0 && cmd_is == AT_0)
				{
					SIM800_sendCmd("AT+GSN\r\n");
					cmd_is = AT_GSN;
				}else if(strlen(imei) == 15)
				{
					char cmd[32];	
					sprintf(upload,"$PGID,%s\r\n",imei);
					printf("send imei: %s\n",upload);
					int len = strlen(upload);
					snprintf(cmd,sizeof(cmd),"AT+CIPSEND=%d\r\n",len);
					SIM800_sendCmd(cmd);
					network_state = NETWORK_ID_SENDING;
					sending_timeout = xTaskGetTickCount();
				}
			}
			if(network_state == NETWORK_ID_SENT)
			{
				SIM800_sendCmd("AT+CTTS=2,network connected success\r\n");
				PIOS_DELAY_WaitmS(1000);
				network_state = NETWORK_SENT_OK;
			}
			if(network_state == NETWORK_SENT_OK)
			{
				if(xTaskGetTickCount() - sending_timeout >= upload_intervala)
				{
					GyrosGet(&gyrosData);
					AccelsGet(&accelsData);
					MagnetometerGet(&magData);
					AttitudeActualGet(&attitudeData);
					OBDGet(&obdData);
					sprintf(upload,"$%s,g,%f,%f,%f,a,%f,%f,%f,m,%f,%f,%f,at,%f,%f,%f,o,%d,%d\r\n",
							gpsPosition.gprmc,
							gyrosData.x,gyrosData.y,gyrosData.z,
							accelsData.x,accelsData.y,accelsData.z,
							magData.x,magData.y,magData.z,
							attitudeData.Roll,attitudeData.Pitch,attitudeData.Yaw,
							(int)obdData.vehicleSpeed,
							(int)obdData.engineRPM
					       );
					if(g_debug_level > 0)
					{
						printf("%d:%s",strlen(upload),upload);
						if(g_debug_level > 1)
						{
							PIOS_USART_printStatics(1);
							PIOS_USART_printStatics(2);
							PIOS_USART_printStatics(3);
						}
						SIM800_sendCmd("AT+CSQ\r\n");	
					}
					int gpsSpeed = (int)gpsPosition.Groundspeed;
					int vehicleSpeed = (int)(int)obdData.vehicleSpeed;
					int update = (vehicleSpeed+gpsSpeed+obdData.engineRPM) > 0;
					if((update && (manual_upload == 0)) || (manual_upload == 2))//vehicleSpeed > (int)0)
					{
						char cmd[32];	
						int len = strlen(upload);
						snprintf(cmd,sizeof(cmd),"AT+CIPSEND=%d\r\n",len);
						SIM800_sendCmd(cmd);
						network_state = NETWORK_SENDING;
						upload_bytes+=len;
					}
					sending_timeout = xTaskGetTickCount();
				}
			}
			if(network_state == NETWORK_ERROR)
			{
				SIM800_sendCmd("AT+CIPACK\r\n");
				PIOS_DELAY_WaitmS(1000);
				SIM800_sendCmd("AT+CSQ\r\n");
				PIOS_DELAY_WaitmS(1000);
				SIM800_sendCmd("AT+CIPCLOSE\r\n");
				PIOS_DELAY_WaitmS(1000);
				SIM800_sendCmd("AT+CIPSHUT\r\n");
				network_state = NETWORK_WAIT_SHUT;
				sending_timeout = xTaskGetTickCount();
			}
			if(network_state == NETWORK_SENDING || network_state == NETWORK_ID_SENDING || network_state == NETWORK_WAIT_SHUT || network_state == NETWORK_CONNECTING)
			{
				int tick = xTaskGetTickCount();
				if(tick - sending_timeout > 10000)
				{
					sending_timeout = xTaskGetTickCount();
					if(network_state == NETWORK_WAIT_SHUT)
						network_state = NETWORK_ATTACHED;
					else
						network_state = NETWORK_ERROR;	
				}
				continue;
			}
			if(g_debug_level > 0)
				printf("gpsStatus: %d\r\n",gpsPosition.Status);
			if(gpsPosition.Status > 1)
			{
				if(gps_status <= 1)
				{
					char cmd[64];
					sprintf(cmd,"AT+CTTS=2,gps is ok, Satellites is %d\r\n",gpsPosition.Satellites);
					SIM800_sendCmd(cmd);
					gps_status = gpsPosition.Status;		
				}
			}else if(gpsPosition.Status == 1){
				if(gps_status > 1)
					SIM800_sendCmd("AT+CTTS=2,gps is not ok\r\n");
				gps_status = gpsPosition.Status;	
			}
		}else{
			if(bluetooth_state & BLUETOOTH_SPP_SENDING)
			{
				if(xTaskGetTickCount() - bt_sending_timeout >= 10000)
				{
					bluetooth_state = 0;
					received_n = 1;
					stop_obdTask = 0;
				}
			}
	
			if(network_state == NETWORK_SENDING || network_state == NETWORK_ID_SENDING || network_state == NETWORK_WAIT_SHUT || network_state == NETWORK_CONNECTING)
			{
				int tick = xTaskGetTickCount();
				if(tick - sending_timeout > 10000)
				{
					sending_timeout = xTaskGetTickCount();
					if(network_state == NETWORK_WAIT_SHUT)
						network_state = NETWORK_ATTACHED;
					else
						network_state = NETWORK_ERROR;	
					received_n = 1;
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

