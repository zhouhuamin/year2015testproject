#pragma once
#include "config.h"
#include "ic_card.h"

#define ETAG_NEW_DATA_QUEUE_TMP_FILE	"/tmp/848"
#define ICCARD_NEW_DATA_QUEUE_TMP_FILE	"/tmp/849"
#define LED_NEW_DATA_QUEUE_TMP_FILE		"/tmp/850"
#define WEIGHT_NEW_DATA_QUEUE_TMP_FILE	"/tmp/851"

#define MSG_LOG_FROM_ETAG 				0x50
#define MSG_LOG_FROM_ICCARD 			0x51
#define MSG_LOG_FROM_WEIGHT 			0x52
#define MSG_LOG_FROM_LED 				0x53

#define MSG_FROM_DEV_PTHREAD			0x48



#define	DEV_NEW_DATA_QUEUE_TMP_FILE		ETAG_NEW_DATA_QUEUE_TMP_FILE
#define	MSG_LOG_FROM_DEV				MSG_LOG_FROM_ETAG

#define SYSTEM_SETTING_CML_FILE_NAME   	"ICServerConfig.xml"

#define SERVER_IP_ADDRESS_STR  			("127.0.0.1")
#define SERVER_NET_PORT 				(8888)

#define REGISTER_DEV_TAG				("IC")
#define REGISTER_DEV_ID					("DEV_IC_001")
#define SYS_CTRL_R_START_EVENT_ID		("EC_START_IC")
#define SYS_CTRL_R_STOP_EVENT_ID		("EC_STOP_IC")

#define GATHER_DATA_EVENT_ID			("EG_FINISHED_IC")
#define ARRIVED_DEV_EVENT_ID			("EG_ARRIVED_IC")
#define ONLOAD_DEV_DEV_TAG				("EG_ONLOAD_WEIGHT")

#define GATHER_DATA_DEV_TAG				("IC")


int add_card_to_list(card_block_t *pBlock);
void ReceiveReaderData(char* pXmlData, int nXmlLen, char *pEventID);
void *pthread_dev_handle(void *arg);
void *pthread_dev_handle2(void *arg);
int RFID_upload_gather_data(int server_fd, const char *ic_number1,  const char *ic_number2);
int RFID_upload_arrived_data(int server_fd, const char *ic_number1, const char *ic_number2);
void GetCurTime(char *pTime);
void BuildStatusString(struct struDeviceAndServiceStatus *pStatus, char *pMsg, int nCount);
void *pthread_worker_task(void *arg);
void *ArrivedEventThread(void *arg);
void *StartEventThread(void *arg);
void *StopEventThread(void *arg);
void *TimerEventThread(void *arg);
int IC_upload_gather_data(int server_fd, char *ic_number);
int IC_upload_arrived_data(int server_fd, char *ic_number);
