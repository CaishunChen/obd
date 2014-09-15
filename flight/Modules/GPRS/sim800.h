/*
 * sim800.h
 */

#ifndef __SIM800_H__
#define __SIM800_H__

#define TRUE                    1
#define FALSE                   0

#define SIM800_TX_PIN           8
#define SIM800_RX_PIN           7
#define SIM800_POWER_PIN        9
#define SIM800_POWER_STATUS     12
#define SIM800_BAUDRATE         9600

#define UART_DEBUG

#ifdef UART_DEBUG
#define ERROR(x)            Serial.println(x)
#define DEBUG(x)            Serial.println(x);
#else
#define ERROR(x)
#define DEBUG(x)
#endif

#define DEFAULT_TIMEOUT     5

void SIM800_preInit(void);

/** read from SIM800 module and save to buffer array
 *  @param  buffer  buffer array to save what read from SIM800 module
 *  @param  count   the maximal bytes number read from SIM800 module
 *  @param  timeOut time to wait for reading from SIM800 module 
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_readBuffer(char* buffer,int count, unsigned int timeOut);


/** clean Buffer
 *  @param buffer   buffer to clean
 *  @param count    number of bytes to clean
 */
void SIM800_cleanBuffer(char* buffer, int count);

/** send AT command to SIM800 module
 *  @param cmd  command array which will be send to GPRS module
 */
void SIM800_sendCmd(const char* cmd);

/**send "AT" to SIM800 module
 */
void SIM800_sendATTest(void);

/**send '0x1A' to SIM800 Module
 */
void SIM800_sendEndMark(void);

/** check SIM800 module response before time out
 *  @param  *resp   correct response which SIM800 module will return
 *  @param  *timeout    waiting seconds till timeout
 *  @returns
 *      0 on success
 *      -1 on error
 */ 
int SIM800_waitForResp(const char* resp, unsigned timeout);

/** send AT command to GPRS module and wait for correct response
 *  @param  *cmd    AT command which will be send to GPRS module
 *  @param  *resp   correct response which GPRS module will return
 *  @param  *timeout    waiting seconds till timeout
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_sendCmdAndWaitForResp(const char* cmd, const char *resp, unsigned timeout);

int SIM800_checkSIMStatus(void);

/** check network is OK or not
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_networkCheck(void);

/** send text SMS
 *  @param  *number phone number which SMS will be send to
 *  @param  *data   message that will be send to
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_sendSMS(char* number, char* data);

/** read SMS if getting a SMS message
 *  @param  buffer  buffer that get from GPRS module(when getting a SMS, GPRS module will return a buffer array)
 *  @param  message buffer used to get SMS message
 *  @param  check   whether to check phone number(we may only want to read SMS from specified phone number)
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_readSMS(int messageIndex, char *message, int length);

/** delete SMS message on SIM card
 *  @param  index   the index number which SMS message will be delete
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_deleteSMS(int index);

/** call someone
 *  @param  number  the phone number which you want to call
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_callUp(char* number);

/** auto answer if coming a call
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_answer(void);

/** build TCP connect
 *  @param  ip  ip address which will connect to
 *  @param  port    TCP server' port number
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_connectTCP(const char* ip, int port);

/** send data to TCP server
 *  @param  data    data that will be send to TCP server
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_sendTCPData(char* data);

/** close TCP connection
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_closeTCP(void);

/** close TCP service
 *  @returns
 *      0 on success
 *      -1 on error
 */
int SIM800_shutTCP(void);

#endif
