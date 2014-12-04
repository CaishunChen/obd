#include "openpilot.h"
#include "sim800.h"


#define delay PIOS_DELAY_WaitmS
//#define DEBUG_ERROR printf
#define DEBUG_ERROR(x); 

void SIM800_powerOn(void)
{
	PIOS_LED_On(7);
}

void SIM800_powerOff(void)
{
	PIOS_LED_Off(7);
}

static int uart_getchar(uint32_t timeOut)
{
	uint8_t c=0;
	uint32_t rx_bytes;

//	printf("PIOS_COM_ReceiveBuffer");
	rx_bytes = PIOS_COM_ReceiveBuffer(PIOS_COM_GPRS, &c, 1, timeOut);
	printf("%c ",c);
	if (rx_bytes == 1)
		return c;
	else
	{
		return -1;
	}
}

static int uart_putchar(int c)
{

	return PIOS_COM_SendChar(PIOS_COM_GPRS, c);
}

static int uart_puts(const char *s)
{
	return PIOS_COM_SendString(PIOS_COM_GPRS, s);
}

void SIM800_clearOutput()
{
	
	while(uart_getchar(3000) > 0);
}
int SIM800_readBuffer(char *buffer,int count, uint32_t timeOut)
{
	int i = 0;
	int t = 0;
	while (1) {
		char c = uart_getchar(timeOut);
		if( c == 255)
		{
			printf("t ");
			t++;
			if(t+i >= count)
				return i;
			else
				continue;
		}
		if (c == '\r' || c == '\n') c = '$';
		buffer[i++] = c;
		if(i > count-1)break;
	}
	return i;
}

void SIM800_cleanBuffer(char *buffer, int count)
{
	for(int i=0; i < count; i++) {
		buffer[i] = '\0';
	}
}

void SIM800_sendCmd(const char* cmd)
{
#if 0
	int i;
	int len = strlen(cmd);
	for(i=0; i<len; i++)
		PIOS_COM_SendChar(PIOS_COM_GPRS, cmd[i]);
#else
	PIOS_COM_SendString(PIOS_COM_GPRS,cmd);
#endif
}

int SIM800_sendATTest(void)
{
	return SIM800_sendCmdAndWaitForResp("AT\r\n","OK",DEFAULT_TIMEOUT);
}

int SIM800_waitForResp(const char *resp, uint32_t timeout)
{
	int len = strlen(resp);
	int sum=0;
	int t = 0;
	while(1) {
		char c = uart_getchar(timeout);
		if(c == 255)
		{
			printf("t ");
			t++;
			if(t > len)
				return -1;
			else
				continue;
		}
		PIOS_COM_SendChar(PIOS_COM_DEBUG, c);
		sum = (c==resp[sum]) ? sum+1 : 0;
		if(sum == len)break;
	}
	return 0;
}

void SIM800_sendEndMark(void)
{
	uart_putchar((char)26);
}


int SIM800_sendCmdAndWaitForResp(const char* cmd, const char *resp, uint32_t timeout)
{
	SIM800_sendCmd(cmd);
	return SIM800_waitForResp(resp,timeout);
}

int SIM800_GPRSInit(void)
{   
	for(int i = 0; i < 2; i++){
		SIM800_sendCmd("AT\r\n");
		delay(100);
	}
	SIM800_sendCmd("AT+CFUN=1\r\n"); 
	if(0 != SIM800_checkSIMStatus()) {
		DEBUG_ERROR("ERROR:checkSIMStatus");
		return -1;
	}
	return 0;
}

int SIM800_checkSIMStatus(void)
{
	char gprsBuffer[32];
	int count = 0;
	SIM800_cleanBuffer(gprsBuffer,32);
	while(count < 3) {
		SIM800_sendCmd("AT+CPIN?\r\n");
		SIM800_readBuffer(gprsBuffer,32,DEFAULT_TIMEOUT);
		if((NULL != strstr(gprsBuffer,"+CPIN: READY"))) {
			break;
		}
		count++;
		delay(1000);
	}
	if(count == 3) {
		return -1;
	}
	return 0;
}

int SIM800_networkCheck(void)
{
//	delay(1000);
	if(0 != SIM800_sendCmdAndWaitForResp("AT+CGREG?\r\n","+CGREG: 0,1",DEFAULT_TIMEOUT*3)) { 
		DEBUG_ERROR("ERROR:CGREG");
		return -1;
	}
	SIM800_clearOutput();
//	delay(1000);
	if(0 != SIM800_sendCmdAndWaitForResp("AT+CGATT?\r\n","+CGATT: 1",DEFAULT_TIMEOUT)) {
		DEBUG_ERROR("ERROR:CGATT");
		return -1;
	}
	SIM800_clearOutput();
	return 0;
}

int SIM800_sendSMS(char *number, char *data)
{
	char cmd[32];
	if(0 != SIM800_sendCmdAndWaitForResp("AT+CMGF=1\r\n", "OK", DEFAULT_TIMEOUT)) { // Set message mode to ASCII
		DEBUG_ERROR("ERROR:CMGF");
		return -1;
	}
	delay(500);
	snprintf(cmd, sizeof(cmd),"AT+CMGS=\"%s\"\r\n", number);
	if(0 != SIM800_sendCmdAndWaitForResp(cmd,">",DEFAULT_TIMEOUT)) {
		DEBUG_ERROR("ERROR:CMGS");
		return -1;
	}
	delay(1000);
	uart_puts(data);
	delay(500);
	SIM800_sendEndMark();
	return 0;
}

int SIM800_readSMS(int messageIndex, char *message,int length)
{
	int i = 0;
	char gprsBuffer[100];
	char cmd[16];
	char *p,*s;

	SIM800_sendCmdAndWaitForResp("AT+CMGF=1\r\n","OK",DEFAULT_TIMEOUT);
	delay(1000);
	snprintf(cmd,sizeof(cmd),"AT+CMGR=%d\r\n",messageIndex);
	uart_puts(cmd);
	SIM800_cleanBuffer(gprsBuffer,100);
	SIM800_readBuffer(gprsBuffer,100,DEFAULT_TIMEOUT);

	if(NULL != ( s = strstr(gprsBuffer,"+CMGR"))){
		if(NULL != ( s = strstr(gprsBuffer,"+32"))){
			p = s + 6;
			while((*p != '$')&&(i < length-1)) {
				message[i++] = *(p++);
			}
			message[i] = '\0';
		}
	}
	return 0;
}

int SIM800_readIMEI(char *imei, int len)
{
	char gprsBuffer[32];
	char *p;
	int i = 0;
	SIM800_sendCmd("at+gsn\r\n");
	SIM800_cleanBuffer(gprsBuffer,32);
	SIM800_readBuffer(gprsBuffer,32,DEFAULT_TIMEOUT);
	printf("xxx imei:%s",gprsBuffer);
	if(strstr(gprsBuffer,"OK"))
	{
		printf("123 ");
		p = gprsBuffer;
		while(i < (len-1))
		{
			if(*p != '$')
				imei[i++] = *p;
			p++;
		}
		imei[i] = '\0';
		printf(" %d",i);
		return i;
	}else
		printf("xxx");
	return 0;
}

int SIM800_deleteSMS(int index)
{
	char cmd[16];
	snprintf(cmd,sizeof(cmd),"AT+CMGD=%d\r\n",index);
	SIM800_sendCmd(cmd);
	return 0;
}

int SIM800_callUp(char *number)
{
	char cmd[24];
	if(0 != SIM800_sendCmdAndWaitForResp("AT+COLP=1\r\n","OK",5)) {
		DEBUG_ERROR("COLP");
		return -1;
	}
	delay(1000);
	sprintf(cmd,"\r\nATD%s;\r\n", number);
	SIM800_sendCmd(cmd);
	return 0;
}

int SIM800_answer(void)
{
	uart_puts("ATA\r\n");
	return 0;
}

int SIM800_connectTCP(const char *ip, int port)
{
	char cipstart[50];
	sprintf(cipstart, "AT+CIPSTART=\"TCP\",\"%s\",\"%d\"\r\n", ip, port);
	if(0 != SIM800_sendCmdAndWaitForResp(cipstart, "CONNECT OK", DEFAULT_TIMEOUT)) {// connect tcp
		DEBUG_ERROR("ERROR:CIPSTART");
		return -1;
	}

	return 0;
}
int SIM800_sendTCPData(char *data)
{
	char cmd[32];
	int len = strlen(data);
	snprintf(cmd,sizeof(cmd),"AT+CIPSEND=%d\r\n",len);
	if(0 != SIM800_sendCmdAndWaitForResp(cmd,">",5*DEFAULT_TIMEOUT)) {
		DEBUG_ERROR("ERROR:CIPSEND");
		return -1;
	}
	if(0 != SIM800_sendCmdAndWaitForResp(data,"SEND OK",DEFAULT_TIMEOUT)) {
		DEBUG_ERROR("ERROR:SendTCPData");
		return -1;
	}
	return 0;
}

int SIM800_closeTCP(void)
{
	SIM800_sendCmd("AT+CIPCLOSE\r\n");
	return 0;
}

int SIM800_shutTCP(void)
{
	SIM800_sendCmd("AT+CIPSHUT\r\n");
	return 0;
}

