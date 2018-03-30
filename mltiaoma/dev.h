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
#define MSG_FROM_DEV_PTHREAD			0x48


#define	DEV_NEW_DATA_QUEUE_TMP_FILE		ETAG_NEW_DATA_QUEUE_TMP_FILE
#define	MSG_LOG_FROM_DEV                        MSG_LOG_FROM_ETAG
#define LED_NEW_INFO_QUEUE_TMP_FILE             "/tmp/880"
#define SYSTEM_SETTING_CML_FILE_NAME            "BarcodeConfig.xml"
#define SERVER_IP_ADDRESS_STR  			("127.0.0.1")
#define SERVER_NET_PORT 			(8888)
#define REGISTER_DEV_TAG			("LCD")
#define REGISTER_DEV_ID				("DEV_LCD_001")
#define SYS_CTRL_SHOW_EVENT_ID                  ("EC_SHOW_LCD")
#define SYS_CTRL_DATA_EVENT_ID                  ("EC_DATA_LCD")
#define SYS_CTRL_RESET_EVENT_ID                 ("EC_RESET_LCD")
#define SYS_CTRL_START_EVENT_ID                 ("EC_START_GATHER")
#define SYS_CTRL_STOP_EVENT_ID                  ("EC_STOP_GATHER")
#define GATHER_DATA_DEV_TAG			("GATHER")
#define GATHER_DATA_EVENT_ID			("EG_FINISHED_GATHER")
#define GATHER_ARRIVED_EVENT_ID			("EG_ARRIVED_GATHER")


int barcode_upload_gather_data(const char *barcode, char *pEventID);
int barcode_upload_arrived_data(const char *barcode);
void ReceiveReaderData(char* pXmlData, int nXmlLen, char *pEventID);
void *pthread_dev_handle(void *arg);
void GetCurTime(char *pTime);
void BuildStatusString(struct struDeviceAndServiceStatus *pStatus, char *pMsg, int nCount);
void *pthread_worker_task(void *arg);
void *ArrivedEventThread(void *arg);
void *StartEventThread(void *arg);
void *StopEventThread(void *arg);
void *TimerEventThread(void *arg);
