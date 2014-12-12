#include "openpilot.h"
#include "pios_can.h"
#include "obd.h"

#define DEBUG_MSG(format, ...) PIOS_COM_SendFormattedString(PIOS_COM_DEBUG, format, ## __VA_ARGS__)

#define SHELL_CMD_BUFFER_LEN	128
#define SHELL_WELCOME_STRING	"Welcome\r\n"
#define SHELL_PROMPT_STRING	"# "

#define ARRAY_SIZE(a)	(sizeof(a)/sizeof(a[0]))
#define STACK_SIZE 1024 

struct shell_command {
	char *name;
	char *usage;
	void (*function )(int argc, char **argv);
};

static struct {
	char buf[SHELL_CMD_BUFFER_LEN];
	unsigned int index;
	unsigned int match;
} shell;
int g_debug_level = 1;
int show_can_id = 0;
extern int stop_obdTask;
extern int manual_upload;

/* built-in shell commands */
static void shell_help(int argc, char **argv);
static void at(int argc, char **argv);
static void obd(unsigned char *cmd, int len);

static struct shell_command commands[] = {
	{ "help", "",	shell_help },
	{ "at",	"sp <6-8>, dp, cf <hhhhhhhh>, cm <hhhhhhhh> show <id>", at},
};

int uart_getchar(void)
{
	uint8_t c;
	int res;

//	DEBUG_MSG("PIOS_COM_ReceiveBuffer\r\n");
	res = PIOS_COM_ReceiveBuffer(PIOS_COM_DEBUG, &c, 1, -1);
//	DEBUG_MSG("PIOS_COM_ReceiveBuffer return %d",res);
	if (res == 1)
		return c;
	else
	{
		return -1;
	}
}

int uart_putchar(int c)
{
	
	return PIOS_COM_SendChar(PIOS_COM_DEBUG, c);
}

int uart_puts(const char *s)
{
	return PIOS_COM_SendString(PIOS_COM_DEBUG, s);
}


static void prompt(void)
{
	uart_puts(SHELL_PROMPT_STRING);
}

static char is_whitespace(char c)
{
	return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static unsigned int hex2bin( const char *s, int len )
{
    unsigned int ret=0;
    int n = len;
    int i;
    for( i=0; (i<n) && (i<8); i++ )
    {
        char c = *s++;
        int n=0;
        if( '0'<=c && c<='9' )
            n = c-'0';
        else if( 'a'<=c && c<='f' )
            n = 10 + c-'a';
        else if( 'A'<=c && c<='F' )
            n = 10 + c-'A';
	else
	   return -1;
        ret = n + ret*16;
    }
    return ret;
}

static void do_command(int argc, char **argv)
{
	unsigned int i;
	unsigned int nl = strlen(argv[0]);
	unsigned int cl;
	unsigned char cmd[16];
	unsigned int cmd_len = 0;
	int d;
	if(argc > 16)
	{
		uart_puts("?\r\n");
		return;
	}
	memset(cmd,0,sizeof(cmd)); 
	for (i = 0; i < argc; i++)
	{
		d = hex2bin(argv[i],strlen(argv[i]));
		if(d != -1)
		{
			cmd[i+1] = d;	
			cmd_len++;
		}
	}
	if(cmd_len == argc)
	{
		obd(cmd,cmd_len);
		return;
	}
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		cl = strlen(commands[i].name);

		if (cl == nl && commands[i].function != NULL &&
				!strncmp(argv[0], commands[i].name, nl)) {
			commands[i].function(argc, argv);
			uart_putchar('\r');
			uart_putchar('\n');
		}
	}
}

static void parse_command(void)
{
	unsigned char i;
	char *argv[16];
	int argc = 0;
	char *in_arg = NULL;

	for (i = 0; i < shell.index; i++) {
		if (is_whitespace(shell.buf[i]) && argc == 0)
			continue;

		if (is_whitespace(shell.buf[i])) {
			if (in_arg) {
				shell.buf[i] = '\0';
				in_arg = NULL;
			}
		} else if (!in_arg) {
			in_arg = &shell.buf[i];
			argv[argc] = in_arg;
			argc++;
		}
	}
	shell.buf[i] = '\0';

	if (argc > 0)
		do_command(argc, argv);
}

static void handle_tab(void)
{
	int i, j;
	unsigned int match = 0;
	struct shell_command *cmd;
	unsigned int sl = 0;

	shell.match = 0;

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		for (j = strlen(commands[i].name); j > 0; j--) {
			if (!strncmp(commands[i].name, shell.buf, shell.index)) {
				match++;
				shell.match |= (1<<i);
				break;
			}
		}
	}

	if (match == 1) {
		for (i = 0; i < ARRAY_SIZE(commands); i++) {
			if (shell.match & (1<<i)) {
				cmd = &commands[i];
				memcpy(shell.buf + shell.index, cmd->name + shell.index,
						strlen(cmd->name) - shell.index);
				shell.index += strlen(cmd->name) - shell.index;
			}
		}
	} else if (match > 1) {
		for (i = 0; i < ARRAY_SIZE(commands); i++) {
			if (shell.match & (1<<i)) {
				uart_putchar('\r');
				uart_putchar('\n');
				uart_puts(commands[i].name);

				if (sl == 0 || strlen(commands[i].name) < sl) {
					sl = strlen(commands[i].name);
				}
			}
		}

		for (i = 0; i < ARRAY_SIZE(commands); i++) {
			if (shell.match & (1<<i) && strlen(commands[i].name) == sl) {
				memcpy(shell.buf + shell.index, commands[i].name + shell.index,
						sl - shell.index);
				shell.index += sl - shell.index;
				break;
			}
		}
	}

	uart_putchar('\r');
	uart_putchar('\n');
	prompt();
	uart_puts(shell.buf);
}

static void handle_input(char c)
{
	switch (c) {
		case '\r':
		case '\n':
			uart_putchar('\r');
			uart_putchar('\n');
			if (shell.index > 0) {
				parse_command();
				shell.index = 0;
				memset(shell.buf, 0, sizeof(shell.buf));
			}
			prompt();
			break;

		case '\b':
			if (shell.index) {
				shell.buf[shell.index-1] = '\0';
				shell.index--;
				uart_putchar(c);
				uart_putchar(' ');
				uart_putchar(c);
			}
			break;

		case '\t':
			handle_tab();
			break;

		default:
			if (shell.index < SHELL_CMD_BUFFER_LEN - 1) {
				uart_putchar(c);
				shell.buf[shell.index] = c;
				shell.index++;
			}
	}
}

static void shell_task(void *params)
{
	uart_puts(SHELL_WELCOME_STRING);
	prompt();

	while (1) {
		int c=-1;
		
//		vTaskDelay(1000);
//		DEBUG_MSG("handle_input %x\r\n",handle_input);		
		c = uart_getchar();
		if (c != -1) {
			handle_input((char)c);
			PIOS_COM_SendChar(PIOS_COM_GPRS, c);
			//printf("%d ",c);
			//PIOS_COM_SendChar(PIOS_COM_GPS, c);
		}
//		vTaskDelay(1000);
		
	}
}

void shell_init(void)
{
	memset(&shell, 0, sizeof(shell));

	xTaskCreate(shell_task, (signed char *)"SHELL", STACK_SIZE, (void *)NULL, tskIDLE_PRIORITY + 1, NULL);
}

static void shell_help(int argc, char **argv)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		uart_puts(commands[i].name);
		uart_putchar('\t');
		uart_puts(commands[i].usage);
		uart_putchar('\r');
		uart_putchar('\n');
	}
}
static char *can_protos[] = {
	"(11 bit ID, 500 Kbaud)\r\n", //6	
	"(29 bit ID, 500 Kbaud)\r\n", //7	
	"(11 bit ID, 250 Kbaud)\r\n", //8	
	"(11 bit ID, 125 Kbaud)\r\n", //9	
	"(29 bit ID, 250 Kbaud)\r\n", //10	
	"(SAE J1939, 250 Kbaud)\r\n",	//11	
};
int can_cur_proto = 8;

char *get_cur_proto()
{
	return can_protos[can_cur_proto-6];
}

static void at_sp_help()
{
	DEBUG_MSG("set the can protocol to one of:\r\n");
	for(int i = 0; i<ARRAY_SIZE(can_protos); i++)
		DEBUG_MSG(can_protos[i]);
	
}

static	CAN_FilterInitTypeDef CAN_filterInit[] = {
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

static can_t can_filters = {
			.CAN_filterInit = CAN_filterInit,
			.n_filters = 1,
		};

static void at(int argc, char **argv)
{
	if(argc <= 1)
	{
		DEBUG_MSG("at sp <6-8>, at dp, at cf <hhhhhhhh>, at cm <hhhhhhhh>\r\n");
		return;	
	}
	if(!strcmp(argv[1],"sp"))
	{
		if(argc <= 2)
		{
			at_sp_help();
			return;
		}
		int proto = atoi(argv[2]);
		if(proto < 6 || proto > 10)
		{
			at_sp_help();
			return;
		}	
		if(proto == 6 || proto == 7)
			can_set_baud(500);
		else if(proto == 9)
			can_set_baud(125);
		else if(proto == 8 || proto == 10 || proto == 11)
			can_set_baud(250);
		can_cur_proto = proto;			
	}else if(!strcmp(argv[1],"dp"))
	{
		DEBUG_MSG(can_protos[can_cur_proto-6]);	
	}else if(!strcmp(argv[1],"cf"))
	{
		if(argc <= 2)
		{
			if(CAN_filterInit[0].CAN_FilterIdLow & CAN_Id_Extended)
			{
				unsigned int id =  CAN_filterInit[0].CAN_FilterIdHigh << 16|CAN_filterInit[0].CAN_FilterIdLow;
				id >>= 3;
				DEBUG_MSG("at cf %08x\r\n", id);
			}else
				DEBUG_MSG("at cf %03x\r\n", CAN_filterInit[0].CAN_FilterIdHigh>>5);
			return;
		}
		unsigned int id = hex2bin(argv[2],strlen(argv[2]));
		if(IS_CAN_STDID(id))
		{
			CAN_filterInit[0].CAN_FilterIdHigh = id<<5;
			CAN_filterInit[0].CAN_FilterIdLow = 0;
		}else{
			id <<= 3;
			CAN_filterInit[0].CAN_FilterIdHigh = (id>>16)&0xffff;
			CAN_filterInit[0].CAN_FilterIdLow = (id&0xffff)|CAN_Id_Extended;
		}
		can_set_filters(&can_filters);
	}else if(!strcmp(argv[1],"cm"))
	{
		if(argc <= 2)
		{
			if(CAN_filterInit[0].CAN_FilterIdLow & CAN_Id_Extended)
			{
				unsigned int mask = CAN_filterInit[0].CAN_FilterMaskIdHigh << 16 | CAN_filterInit[0].CAN_FilterMaskIdLow;
				mask >>= 3;
				DEBUG_MSG("at cm %08x\r\n", mask);
			}else{
				DEBUG_MSG("at cf %03x\r\n", CAN_filterInit[0].CAN_FilterMaskIdHigh>>5);
			}
			return;
		}
		unsigned int id = hex2bin(argv[2],strlen(argv[2]));
		if(CAN_filterInit[0].CAN_FilterIdLow & CAN_Id_Extended)
		{
			id <<= 3;
			CAN_filterInit[0].CAN_FilterMaskIdHigh = (id&0xffff0000)>>16;
			CAN_filterInit[0].CAN_FilterMaskIdLow = id&0xffff;
		}else{
			CAN_filterInit[0].CAN_FilterMaskIdHigh = id<<5;
			CAN_filterInit[0].CAN_FilterMaskIdLow = 0;
		}
		can_set_filters(&can_filters);
	}else if(!strcmp(argv[1],"show"))
	{
		show_can_id = hex2bin(argv[2],strlen(argv[2]));	
		DEBUG_MSG("show %x\r\n", show_can_id);
	}else if(!strcmp(argv[1], "debug"))
	{
		g_debug_level = hex2bin(argv[2],strlen(argv[2]));
	}else if(!strcmp(argv[1], "obd"))
	{
		stop_obdTask = hex2bin(argv[2],strlen(argv[2]));
	}else if(!strcmp(argv[1], "upload"))
	{
		manual_upload = hex2bin(argv[2],strlen(argv[2]));
	}else if(!strcmp(argv[1], "gprs"))
	{
		int on = hex2bin(argv[2],strlen(argv[2]));
		if(on)
			PIOS_LED_On(7); //power on gprs
		else
			PIOS_LED_Off(7); //power on gprs
			
	}else if(!strcmp(argv[1],"gps"))
	{
		int on = hex2bin(argv[2],strlen(argv[2]));
		if(on)
			PIOS_LED_On(6); //power on gprs
		else
			PIOS_LED_Off(6); //power on gprs
	}
	else if(!strcmp(argv[1],"send"))
	{
		unsigned char cmd[16];
		unsigned int cmd_len = 0;
		unsigned int can_id = 0;
		int d,i;
			
		can_id = hex2bin(argv[2],strlen(argv[2]));
		if(can_id == -1)
		{
			DEBUG_MSG("invalid can id\r\n");
			return;	
		}	
		memset(cmd,0,sizeof(cmd)); 
		for (i = 3; i < argc; i++)
		{
			d = hex2bin(argv[i],strlen(argv[i]));
			if(d != -1)
			{
				cmd[i-3] = d;	
				cmd_len++;
			}else
			{
				DEBUG_MSG("invalid can data\r\n");
				return;	
			}
		}
		can_send_msg(cmd,can_id,cmd_len);
	}
}
static void obd(unsigned char *cmd, int len)
{
	DEBUG_MSG("obd %d\r\n",len);
	cmd[0] = len;
	if(can_cur_proto == 6 || can_cur_proto == 8)
		can_send_msg(cmd,0x7df,8);
	else if(can_cur_proto == 7 || can_cur_proto == 9)
		can_send_msg(cmd,0x18db33f1,8);
	else if(can_cur_proto == 10)
		can_send_msg(cmd,0x18eafff9,8);
	else
		uart_puts("?\r\n");
}

int obd2(char *str, int len, char *res)
{
	unsigned char cmd[8];
	int i;
	int ret;
	char *res_org = res;
	CanRxMsg RxMessage;


	memset(cmd,0,sizeof(cmd));

	len = len >> 1;
	cmd[0] = len;

	for(i=0; i<len; i++)
	{
		cmd[i+1] = hex2bin(str,2);
		str += 2;
	}
	if(g_debug_level > 1)
	{
		printf("obd2: ");
		for(i=0; i<8; i++)
			printf("%02x ",cmd[i]);
		printf("\r\n ");
	}
	if(can_cur_proto == 6 || can_cur_proto == 8)
		can_send_msg(cmd,0x7df,8);
	else if(can_cur_proto == 7 || can_cur_proto == 9)
		can_send_msg(cmd,0x18db33f1,8);
	else if(can_cur_proto == 10)
		can_send_msg(cmd,0x18eafff9,8);

reget:
	ret = can_receive_msgFIFO1(&RxMessage, 1000);
	if(ret > 0)
	{
		if(CAN_Id_Standard == RxMessage.IDE)
		{
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
				if(RxMessage.Data[2] != cmd[2])
					goto reget;
				for(i=0; i<RxMessage.Data[0]; i++)
				{
					sprintf(res,"%02X", RxMessage.Data[i+1]);
					res += 2;
				}
				if(g_debug_level > 0)
					printf("%s\r\n",res_org);
				return i*2;
			}
		}

	}

	return 0;
}


