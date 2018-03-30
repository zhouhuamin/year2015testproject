#pragma once
#include "config.h"

#define ETAG_NEW_DATA_QUEUE_TMP_FILE	"/tmp/848"
#define ICCARD_NEW_DATA_QUEUE_TMP_FILE	"/tmp/849"
#define LED_NEW_DATA_QUEUE_TMP_FILE	"/tmp/850"
#define WEIGHT_NEW_DATA_QUEUE_TMP_FILE	"/tmp/851"

#define MSG_LOG_FROM_ETAG 			0x50
#define MSG_LOG_FROM_ICCARD 			0x51
#define MSG_LOG_FROM_WEIGHT 			0x52
#define MSG_LOG_FROM_LED 			0x53
#define MSG_LOG_FROM_BROADCAST 			0x54
#define MSG_FROM_DEV_PTHREAD			0x48

#define	DEV_NEW_DATA_QUEUE_TMP_FILE		ETAG_NEW_DATA_QUEUE_TMP_FILE
#define	MSG_LOG_FROM_DEV		        MSG_LOG_FROM_BROADCAST
#define BROADCAST_NEW_INFO_QUEUE_TMP_FILE	"/tmp/882"
#define SYSTEM_SETTING_CML_FILE_NAME   	"EBroadcastServerConfig.xml"


#define SERVER_IP_ADDRESS_STR  			("127.0.0.1")
#define SERVER_NET_PORT 			(8888)
#define REGISTER_DEV_TAG			("BROADCAST")
#define REGISTER_DEV_ID				("DEV_BRT_001")
#define SYS_CTRL_R_START_EVENT_ID		("EC_TRANS_BRT")
#define SYS_CTRL_R_STOP_EVENT_ID		("EC_STOP_LED")
#define SYS_CTRL_IC_RING_EVENT_ID               ("EC_SUCCESS_IC")


int broadcast_msg_play(char *xml_buf, int xml_buf_len);
void ReceiveReaderData(char* pXmlData, int nXmlLen, char *pEventID);
void *pthread_dev_handle(void *arg);
void GetCurTime(char *pTime);
void BuildStatusString(struct struDeviceAndServiceStatus *pStatus, char *pMsg, int nCount);
void *pthread_worker_task(void *arg);
void *ArrivedEventThread(void *arg);
void *StartEventThread(void *arg);
void *StopEventThread(void *arg);
void *TimerEventThread(void *arg);
void *pthread_dev_handle_test(void *arg);
