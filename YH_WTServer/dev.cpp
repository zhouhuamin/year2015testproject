#include "includes.h"
#include "reader1000api.h"
#include "dev.h"
#include "weight.h"
#include "packet_format.h"
#include "starup_handle.h"
#include "RFID_label.h"
#include "signal.h"
#include <time.h>
#include <zmq.h>
#include "ace/Log_Priority.h"
#include "ace/Log_Msg.h"
#include "ace/SOCK_Connector.h"
#include "ace/Signal.h"
#include "SysMessage.h"
#include "MSGHandleCenter.h"
#include "jzLocker.h"
#include <map>
#include <set>
#include <functional>
#include <algorithm>
#include <iterator>
#include <boost/circular_buffer.hpp>
#include <stdlib.h>
#include <boost/lexical_cast.hpp>

extern int gFd_dev;
static net_packet_head_t packet_head;
static u8 data_buf[0x800];
static char tmp_str[150];
int g_nArrivedFlag = 0;	// 0Ϊ������״̬, 1Ϊ����״̬
locker g_nArrivedFlagLock;
extern struct struDeviceAndServiceStatus g_ObjectStatus;
extern pthread_mutex_t g_StatusMutex;
extern int dev_retry_connect_handle(int *pFd_dev);
extern std::map<std::string, strutLabelModifiers> g_LabelMap;
extern locker       		  g_LabelMapLock;
extern boost::circular_buffer<int>         g_WeightQueue;
extern locker                              g_WeightLock;
extern boost::circular_buffer<int>          g_WeightSet;
extern locker                              g_WeightSetLock;
extern sem	g_StartEventSem;
extern sem	g_StoppedEventSem;

int add_weight_to_queue(int nWeight)
{
    g_WeightLock.lock();
    g_WeightQueue.push_back(nWeight);
    g_WeightLock.unlock();
    return 0;
}


int WT_upload_gather_data(int server_fd, char *weight_value)
{
	NET_gather_data *p_gather_data;
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	int w_len;
	xmlChar * xml_buf;
	char *p_send;
	int xml_len;
	int ret;

	packet_head.msg_type = MSG_TYPE_GATHER_DATA;
	strcpy(packet_head.szVersion, "1.0");

	p_gather_data = (NET_gather_data *) (data_buf + sizeof(net_packet_head_t)
			+ 8);

	strcpy(p_gather_data->event_id, gSetting_system.gather_event_id);
	sprintf(tmp_str, "RE\t%s\n", weight_value);

	ret = 0;
	memcpy(&(p_gather_data->is_data_full), &ret, sizeof(int));
	strcpy(p_gather_data->dev_tag, gSetting_system.gather_DEV_TAG);

	/**********************************XML*********************************/
	doc = xmlNewDoc(NULL);

	root_node = xmlNewNode(NULL, BAD_CAST "WEIGHT");

	xmlDocSetRootElement(doc, root_node);

	xmlNewChild(root_node, NULL, BAD_CAST "GROSS_WT",
	BAD_CAST (weight_value));

	xmlDocDumpFormatMemoryEnc(doc, &xml_buf, &xml_len, "GB2312", 1);

	p_send = strstr((char*) xml_buf, "<WEIGHT>");

	if (p_send == NULL)
	{
		return -1;
	}

	xml_len -= (p_send - (char*) xml_buf);

	p_gather_data->xml_data_len = xml_len + 1;
	memcpy(p_gather_data->xml_data, p_send, xml_len);
	p_gather_data->xml_data[xml_len] = 0;

	xmlFree(xml_buf);
	xmlFreeDoc(doc);

	xmlCleanupParser();

	xmlMemoryDump();
	/**********************************XML END*********************************/

	w_len = ((u8*) p_gather_data->xml_data - data_buf) + xml_len + 1;
	packet_head.packet_len = w_len - sizeof(net_packet_head_t) - 8;
	strcpy((char *) data_buf, NET_PACKET_HEAD_STRING);

	memcpy(data_buf + 8, &packet_head, sizeof(net_packet_head_t));
	strcpy((char*) (data_buf + w_len), NET_PACKET_TAIL_STRING);
	w_len += 8;

//	//	LOG("upload gather data w_len = %d\n", w_len);
//	ret = write(server_fd, data_buf, w_len);
//	if (ret != w_len)
//	{
//		LOG_M("upload gather data error! ret =%d\n", ret);
//	}
//
//	log_send(tmp_str);
        syslog(LOG_DEBUG, "upload gather data w_len = %d\n", w_len);
        ReceiveReaderData(p_gather_data->xml_data, xml_len, p_gather_data->event_id);


	return 0;
}

int WT_upload_arrived_data(int server_fd, char *weight_value)
{
	NET_gather_data *p_gather_data;
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	int w_len;
	xmlChar * xml_buf;
	char *p_send;
	int xml_len;
	int ret;

	packet_head.msg_type = MSG_TYPE_GATHER_DATA;
	strcpy(packet_head.szVersion, "1.0");

	p_gather_data = (NET_gather_data *) (data_buf + sizeof(net_packet_head_t)
			+ 8);

	strcpy(p_gather_data->event_id, gSetting_system.arrived_DEV_event_id); // onload_event_id);
	sprintf(tmp_str, "ARRIVED\t%s\n", weight_value);

	ret = 0;
	memcpy(&(p_gather_data->is_data_full), &ret, sizeof(int));
	strcpy(p_gather_data->dev_tag, gSetting_system.gather_DEV_TAG);

	/**********************************XML*********************************/
	doc = xmlNewDoc(NULL);

	root_node = xmlNewNode(NULL, BAD_CAST "WEIGHT");

	xmlDocSetRootElement(doc, root_node);

	xmlNewChild(root_node, NULL, BAD_CAST "GROSS_WT",
	BAD_CAST (weight_value));

	xmlDocDumpFormatMemoryEnc(doc, &xml_buf, &xml_len, "GB2312", 1);

	p_send = strstr((char*) xml_buf, "<WEIGHT>");

	if (p_send == NULL)
	{
		return -1;
	}

	xml_len -= (p_send - (char*) xml_buf);

	p_gather_data->xml_data_len = xml_len + 1;
	memcpy(p_gather_data->xml_data, p_send, xml_len);
	p_gather_data->xml_data[xml_len] = 0;

	xmlFree(xml_buf);
	xmlFreeDoc(doc);

	xmlCleanupParser();

	xmlMemoryDump();
	/**********************************XML END*********************************/

	w_len = ((u8*) p_gather_data->xml_data - data_buf) + xml_len + 1;
	packet_head.packet_len = w_len - sizeof(net_packet_head_t) - 8;
	strcpy((char *) data_buf, NET_PACKET_HEAD_STRING);

	memcpy(data_buf + 8, &packet_head, sizeof(net_packet_head_t));
	strcpy((char*) (data_buf + w_len), NET_PACKET_TAIL_STRING);
	w_len += 8;

//	//	LOG("upload gather data w_len = %d\n", w_len);
//	ret = write(server_fd, data_buf, w_len);
//	if (ret != w_len)
//	{
//		LOG_M("upload gather data error! ret =%d\n", ret);
//	}
//
//	log_send(tmp_str);
        syslog(LOG_DEBUG, "upload arrived data w_len = %d\n", w_len);
        syslog(LOG_DEBUG, "%s\n", p_gather_data->xml_data);
        ReceiveReaderData(0, 0, p_gather_data->event_id);


	return 0;
}

int WT_upload_onload_data(int server_fd, char *weight_value)
{
	NET_gather_data *p_gather_data;
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	int w_len;
	xmlChar * xml_buf;
	char *p_send;
	int xml_len;
	int ret;

	packet_head.msg_type = MSG_TYPE_GATHER_DATA;
	strcpy(packet_head.szVersion, "1.0");

	p_gather_data = (NET_gather_data *) (data_buf + sizeof(net_packet_head_t)
			+ 8);

	strcpy(p_gather_data->event_id, gSetting_system.arrived_DEV_event_id); // onload_event_id);
	sprintf(tmp_str, "ONLOAD\t%s\n", weight_value);

	ret = 0;
	memcpy(&(p_gather_data->is_data_full), &ret, sizeof(int));
	strcpy(p_gather_data->dev_tag, gSetting_system.gather_DEV_TAG);

	/**********************************XML*********************************/
	doc = xmlNewDoc(NULL);

	root_node = xmlNewNode(NULL, BAD_CAST "WEIGHT");

	xmlDocSetRootElement(doc, root_node);

	xmlNewChild(root_node, NULL, BAD_CAST "GROSS_WT",
	BAD_CAST (weight_value));

	xmlDocDumpFormatMemoryEnc(doc, &xml_buf, &xml_len, "GB2312", 1);

	p_send = strstr((char*) xml_buf, "<WEIGHT>");

	if (p_send == NULL)
	{
		return -1;
	}

	xml_len -= (p_send - (char*) xml_buf);

	p_gather_data->xml_data_len = xml_len + 1;
	memcpy(p_gather_data->xml_data, p_send, xml_len);
	p_gather_data->xml_data[xml_len] = 0;

	xmlFree(xml_buf);
	xmlFreeDoc(doc);

	xmlCleanupParser();

	xmlMemoryDump();
	/**********************************XML END*********************************/

	w_len = ((u8*) p_gather_data->xml_data - data_buf) + xml_len + 1;
	packet_head.packet_len = w_len - sizeof(net_packet_head_t) - 8;
	strcpy((char *) data_buf, NET_PACKET_HEAD_STRING);

	memcpy(data_buf + 8, &packet_head, sizeof(net_packet_head_t));
	strcpy((char*) (data_buf + w_len), NET_PACKET_TAIL_STRING);
	w_len += 8;

//	//	LOG("upload gather data w_len = %d\n", w_len);
//	ret = write(server_fd, data_buf, w_len);
//	if (ret != w_len)
//	{
//		LOG_M("upload gather data error! ret =%d\n", ret);
//	}
//
//	log_send(tmp_str);
        syslog(LOG_DEBUG, "upload arrived data w_len = %d\n", w_len);
        syslog(LOG_DEBUG, "%s\n", p_gather_data->xml_data);
        ReceiveReaderData(0, 0, p_gather_data->event_id);

	return 0;
}

void ReceiveReaderData(char* pXmlData, int nXmlLen, char *pEventID) 
{
	NET_PACKET_MSG* pMsg_ = NULL;
	CMSG_Handle_Center::get_message(pMsg_);
	if (!pMsg_) 
	{
		return;
	}
	memset(pMsg_, 0, sizeof (NET_PACKET_MSG));
	pMsg_->msg_head.msg_type = SYS_MSG_PUBLISH_EVENT;
	pMsg_->msg_head.packet_len = sizeof (T_SysEventData);
	T_SysEventData* pReadData = (T_SysEventData*) pMsg_->msg_body;

	strcpy(pReadData->event_id, pEventID);
	pReadData->event_id[31]	= '\0';
	strcpy(pReadData->device_tag, gSetting_system.gather_DEV_TAG);

	pReadData->is_data_full	= 0; // 1;
	char szXMLGatherInfo[10 * 1024] = {0};
        if (nXmlLen > 0 && pXmlData != NULL)
        {
            strncpy(szXMLGatherInfo, pXmlData, nXmlLen);            
            strcpy(pReadData->xml_data, szXMLGatherInfo);
            pReadData->xml_data_len = strlen(szXMLGatherInfo) + 1;
            syslog(LOG_DEBUG, "%s\n", szXMLGatherInfo);
        }
        else
        {
            pReadData->xml_data_len = 0;
        }
	pMsg_->msg_head.packet_len = sizeof (T_SysEventData) + pReadData->xml_data_len;
	syslog(LOG_DEBUG, "pack len:%d\n", pMsg_->msg_head.packet_len);
	MSG_HANDLE_CENTER::instance()->put(pMsg_);
	return;
}

void *pthread_dev_handle(void *arg)
{
	int ret;
	int n_count;
	int r_len;
	u8 EPCID[200];
	int i;
	u8 *p_start;
	int retry_cnt = 0;

	signal(SIGPIPE,SIG_IGN);

	while (1)
	{
		n_count = 0;

		if (gSetting_system.RFID_label_type)
		{
			ret = ISO6B_ListID(gFd_dev, EPCID, &n_count, 0);
			ret = ISO6B_ListID(gFd_dev, EPCID, &n_count, 0);

			n_count = 0;
			ret = ISO6B_ListID(gFd_dev, EPCID, &n_count, 0);
		}
		else
		{
			ret = EPC1G2_ListTagID(gFd_dev, 1, 0, 0, NULL, EPCID, &n_count,
					&r_len, 0);
			ret = EPC1G2_ListTagID(gFd_dev, 1, 0, 0, NULL, EPCID, &n_count,
					&r_len, 0);

			n_count = 0;
			ret = EPC1G2_ListTagID(gFd_dev, 1, 0, 0, NULL, EPCID, &n_count,
					&r_len, 0);
		}
		
		//syslog(LOG_DEBUG, "pthread_dev_handle:ret-%d, n_count-%d,gSetting_system.RFID_label_type-%d\n", ret, n_count, gSetting_system.RFID_label_type);

		if (0 == ret)
		{
			if (n_count > 0)
			{
				p_start = EPCID;
				for (i = 0; i < n_count; i++)
				{
					if (gSetting_system.RFID_label_type)
					{
						add_label_to_list(&p_start[0 + 8 * i], 8);
					}
					else
					{
						add_label_to_list(&p_start[1], p_start[0] * 2);
						p_start += (p_start[0] * 2 + 1);
					}
				}
			}
			//send_msg_to_main_pthread();
			retry_cnt = 0;
		}
		else if (-99 == ret)
		{
			retry_cnt++;
			if (retry_cnt >= 3)
			{
				retry_cnt = 0;
				dev_retry_connect_handle(&gFd_dev);
			}
		}
		else  //协议报文交互出了问题
		{
//			LOG("the reader no respond! ret = %d\n", ret);
		}

		usleep(gSetting_system.DEV_scan_gap * 1000);
		usleep(1000);
	}

	return 0;
}

void *pthread_dev_handle2(void *arg)
{
	int WT_value = 0;
	int valid_value = 0;
	int weight_get_value_failed_cnt = 0;

	signal(SIGPIPE,SIG_IGN);
	tcflush(gFd_dev, TCIFLUSH);  //flush com queue
	while (1)
	{
		WT_value = weight_get(gFd_dev, 200);

		if (WT_value >= 0)
		{
                    add_weight_to_queue(WT_value);
//			valid_value = real_value_parse(WT_value);
//			if (valid_value > 0)
//			{
////				LOG("ON LOAD!\n");
//				on_load_weight_value = valid_value;
//				dev_send_msg(msg_DEV_NEW_DATA, 0);
//			}
//			else if (-99 == valid_value)  //arrived
//			{
////				LOG("THRESHOLD!\n");
//				dev_send_msg(msg_WT_THRESHOLD, 0);
//			}
			weight_get_value_failed_cnt = 0;
		}
		else //鍗忚鎶ユ枃浜や簰鍑轰簡闂
		{
			weight_get_value_failed_cnt++;
			if (weight_get_value_failed_cnt >= 10)
			{
				weight_get_value_failed_cnt = 0;
				dev_retry_connect_handle(&gFd_dev);
			}
		}

		usleep(gSetting_system.DEV_scan_gap * 1000);
		usleep(1000);
	}

	return 0;
}

int RFID_upload_gather_data(int server_fd, const char *ic_number1, const char *ic_number2)
{
	syslog(LOG_DEBUG, "Enter RFID_upload_gather_data\n");
	NET_gather_data *p_gather_data;
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	int w_len;
	xmlChar * xml_buf;
	char *p_send;
	int xml_len;
	int ret;

	packet_head.msg_type = MSG_TYPE_GATHER_DATA;
	strcpy(packet_head.szVersion, "1.0");

	p_gather_data = (NET_gather_data *) (data_buf + sizeof(net_packet_head_t)
			+ 8);

	strcpy(p_gather_data->event_id, gSetting_system.gather_event_id);
	sprintf(tmp_str, "RE:\t%s\t%s\n", ic_number1, ic_number2);

	ret = 0;
	memcpy(&(p_gather_data->is_data_full), &ret, sizeof(int));
	strcpy(p_gather_data->dev_tag, gSetting_system.gather_DEV_TAG);

	/**********************************XML*********************************/
	doc = xmlNewDoc(NULL);

	root_node = xmlNewNode(NULL, BAD_CAST "CAR");

	xmlDocSetRootElement(doc, root_node);

	xmlNewChild(root_node, NULL, BAD_CAST "VE_NAME", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "CAR_EC_NO",
	BAD_CAST (ic_number1));

	xmlNewChild(root_node, NULL, BAD_CAST "CAR_EC_NO2", BAD_CAST (ic_number2));
	xmlNewChild(root_node, NULL, BAD_CAST "VE_CUSTOMS_NO", BAD_CAST "");

	xmlNewChild(root_node, NULL, BAD_CAST "VE_WT", BAD_CAST "");

	xmlDocDumpFormatMemoryEnc(doc, &xml_buf, &xml_len, "GB2312", 1);

	p_send = strstr((char*) xml_buf, "<CAR>");

	if (p_send == NULL)
	{
		return -1;
	}

	xml_len -= (p_send - (char*) xml_buf);

	p_gather_data->xml_data_len = xml_len + 1;
	memcpy(p_gather_data->xml_data, p_send, xml_len);
	p_gather_data->xml_data[xml_len] = 0;

	xmlFree(xml_buf);
	xmlFreeDoc(doc);

	xmlCleanupParser();

	xmlMemoryDump();
	/**********************************XML END*********************************/

	w_len = ((u8*) p_gather_data->xml_data - data_buf) + xml_len + 1;
	packet_head.packet_len = w_len - sizeof(net_packet_head_t) - 8;
	strcpy((char *) data_buf, "0XJZTECH");

	memcpy(data_buf + 8, &packet_head, sizeof(net_packet_head_t));
	strcpy((char*) (data_buf + w_len), "NJTECHJZ");
	w_len += 8;

	//	LOG("upload gather data w_len = %d\n", w_len);
	//ret = write(server_fd, data_buf, w_len);
	//if (ret != w_len)
	//{
	//	//LOG_M("upload gather data error! ret =%d\n", ret);
	//	syslog(LOG_DEBUG, "upload gather data error! ret =%d\n", ret);
	//}

	//log_send(tmp_str);
	syslog(LOG_DEBUG, "upload gather data w_len = %d\n", w_len);
	ReceiveReaderData(p_gather_data->xml_data, xml_len, p_gather_data->event_id);

	return 0;
}

int RFID_upload_arrived_data(int server_fd, const char *ic_number1, const char *ic_number2)
{
	syslog(LOG_DEBUG, "Enter RFID_upload_arrived_data\n");
	NET_gather_data *p_gather_data;
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	int w_len;
	xmlChar * xml_buf;
	char *p_send;
	int xml_len;
	int ret;

	packet_head.msg_type = MSG_TYPE_GATHER_DATA;
	strcpy(packet_head.szVersion, "1.0");

	p_gather_data = (NET_gather_data *) (data_buf + sizeof(net_packet_head_t)
			+ 8);

	strcpy(p_gather_data->event_id, gSetting_system.arrived_DEV_event_id);
	sprintf(tmp_str, "ARRIVED:\t%s\t%s\n", ic_number1, ic_number2);

	ret = 0;
	memcpy(&(p_gather_data->is_data_full), &ret, sizeof(int));
	strcpy(p_gather_data->dev_tag, gSetting_system.gather_DEV_TAG);

	/**********************************XML*********************************/
	doc = xmlNewDoc(NULL);

	root_node = xmlNewNode(NULL, BAD_CAST "CAR");

	xmlDocSetRootElement(doc, root_node);

	xmlNewChild(root_node, NULL, BAD_CAST "VE_NAME", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "CAR_EC_NO",
	BAD_CAST (ic_number1));

	xmlNewChild(root_node, NULL, BAD_CAST "CAR_EC_NO2", BAD_CAST (ic_number2));
	xmlNewChild(root_node, NULL, BAD_CAST "VE_CUSTOMS_NO", BAD_CAST "");

	xmlNewChild(root_node, NULL, BAD_CAST "VE_WT", BAD_CAST "");

	xmlDocDumpFormatMemoryEnc(doc, &xml_buf, &xml_len, "GB2312", 1);

	p_send = strstr((char*) xml_buf, "<CAR>");

	if (p_send == NULL)
	{
		return -1;
	}

	xml_len -= (p_send - (char*) xml_buf);

	p_gather_data->xml_data_len = xml_len + 1;
	memcpy(p_gather_data->xml_data, p_send, xml_len);
	p_gather_data->xml_data[xml_len] = 0;

	xmlFree(xml_buf);
	xmlFreeDoc(doc);

	xmlCleanupParser();

	xmlMemoryDump();
	/**********************************XML END*********************************/

	w_len = ((u8*) p_gather_data->xml_data - data_buf) + xml_len + 1;
	packet_head.packet_len = w_len - sizeof(net_packet_head_t) - 8;
	strcpy((char *) data_buf, "0XJZTECH");

	memcpy(data_buf + 8, &packet_head, sizeof(net_packet_head_t));
	strcpy((char*) (data_buf + w_len), "NJTECHJZ");
	w_len += 8;

	syslog(LOG_DEBUG, "upload arrived data w_len = %d\n", w_len);
	//ReceiveReaderData(p_gather_data->xml_data, xml_len, p_gather_data->event_id);
        ReceiveReaderData(0, 0, p_gather_data->event_id);


	//ret = write(server_fd, data_buf, w_len);
	//if (ret != w_len)
	//{
	//	//LOG_M("upload gather data error! ret =%d\n", ret);
	//	syslog(LOG_DEBUG, "upload gather data error! ret =%d\n", ret);
	//}

	//log_send(tmp_str);

	return 0;
}

void GetCurTime(char *pTime)
{
	time_t t 		= time(0);
	struct tm *ld	= NULL;
	char tmp[32] 	= {0};
	ld					= localtime(&t);
	strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", ld);
	memcpy(pTime, tmp, 64);
}


void BuildStatusString(struct struDeviceAndServiceStatus *pStatus, char *pMsg, int nCount)
{
        char szMsg[512] = {0};
		snprintf(szMsg, sizeof(szMsg), "NJJZTECH : |%s|%d|%s|%d|%s|%d|%s|%s|%s|%s|%d", \
pStatus->szLocalIP, \
pStatus->nLocalPort, \
pStatus->szUpstreamIP, \
pStatus->nUpstreamPort, \
pStatus->szUpstreamObjectCode, \
pStatus->nServiceType, \
pStatus->szObjectCode, \
pStatus->szObjectStatus, \
pStatus->szConnectedObjectCode, \
pStatus->szReportTime, \
nCount);
		memcpy(pMsg, szMsg, sizeof(szMsg));
}

void *pthread_worker_task(void *arg)
{
	char szPublisherIp[64] = {0};
	signal(SIGPIPE,SIG_IGN);

    void * pCtx = NULL;
    void * pSock = NULL;
    //使用tcp协议进行通信，需要连接的目标机器IP地址�?92.168.1.2
    //通信使用的网络端�?�?766
    snprintf(szPublisherIp, sizeof(szPublisherIp), "tcp://%s:%d", gSetting_system.publisher_server_ip, gSetting_system.publisher_server_port);
    szPublisherIp[63] = '\0';
    const char * pAddr = szPublisherIp; // "tcp://192.168.1.101:7766";

    //创建context
   if((pCtx = zmq_ctx_new()) == NULL)
    {
        return 0;
    }
    //创建socket
    if((pSock = zmq_socket(pCtx, ZMQ_DEALER)) == NULL)
    {
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    int iSndTimeout = 5000;// millsecond
    //设置接收超时
    if(zmq_setsockopt(pSock, ZMQ_RCVTIMEO, &iSndTimeout, sizeof(iSndTimeout)) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    //连接目标IP192.168.1.2，端�?766
    if(zmq_connect(pSock, pAddr) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    //循环发送消�?   
	while(1)
    {
      static int i = 0;
      //struct struDeviceAndServiceStatus tmpStatus;
      //memset(&tmpStatus, 0x00, sizeof(tmpStatus));
//		strcpy(g_ObjectStatus.szLocalIP,				"127.0.0.1");
//		g_ObjectStatus.nLocalPort	= 9002;
//
//		strcpy(g_ObjectStatus.szUpstreamIP,		"127.0.0.1");
//		g_ObjectStatus.nUpstreamPort	= 9003;
//		strcpy(g_ObjectStatus.szUpstreamObjectCode,		"APP_CENTER");
//
//		g_ObjectStatus.nServiceType	= 0;
//
//		strcpy(g_ObjectStatus.szObjectCode,			"DEV_WEIGHT_001");
//		strcpy(g_ObjectStatus.szObjectStatus,	"000");
//		strcpy(g_ObjectStatus.szConnectedObjectCode,	"APP_CHANNEL");
//		strcpy(g_ObjectStatus.szReportTime,			"2015-03-29 16:38:25");

		char szMsg[1024] = {0};
		pthread_mutex_lock(&g_StatusMutex);
		BuildStatusString(&g_ObjectStatus, szMsg, i++);
		pthread_mutex_unlock(&g_StatusMutex);




      //_snprintf(szMsg, sizeof(szMsg), "NJJZTECH : %3d", i++);
      //printf("Enter to send...\n");
      syslog(LOG_DEBUG, "Enter to send...\n");
      if(zmq_send(pSock, szMsg, sizeof(szMsg), 0) < 0)
        {
            //fprintf(stderr, "send message faild\n");
            syslog(LOG_ERR, "send message faild\n");
            continue;
        }
      syslog(LOG_DEBUG, "send message : [%s] succeed\n", szMsg);
      // getchar();
      usleep (5000 * 1000);
    }



//	while (1)
//	{
//		usleep(gSetting_system.DEV_scan_gap * 1000);
//		usleep(1000);
//	}

	return 0;
}

void *ArrivedEventThread(void *arg)
{
	signal(SIGPIPE,SIG_IGN);
	while (1)
	{
		int nArrivedFlag = 0;
		g_nArrivedFlagLock.lock();
		nArrivedFlag = g_nArrivedFlag;
		g_nArrivedFlagLock.unlock();
		if (nArrivedFlag == 1)
		{
			syslog(LOG_DEBUG, "nArrivedFlag:%d\n", nArrivedFlag);
			sleep(3);
			continue;
		}

		int nFlag   = 0;
                int nSum    = 0;
                int nAvg    = 0;
                boost::circular_buffer<int> tmpQueue(10);
                
                g_WeightLock.lock();
                tmpQueue = g_WeightQueue;
                g_WeightLock.unlock();
                
                //char szDataBuffer[100] = {0};
                sort(tmpQueue.begin(), tmpQueue.end());
                std::string strTmpData = "";
                //for (int i = 0; i < tmpQueue.size(); ++i)
                int nQueueSize = tmpQueue.size();
                for (int i = 1; i < nQueueSize - 1; ++i)
                {
                    nSum += tmpQueue[i];
                    std::string strTmp = boost::lexical_cast<std::string>(tmpQueue[i]);
                    strTmpData += strTmp;
                    strTmpData += string("    ");
                    //snprintf(szDataBuffer, sizeof(szDataBuffer), "%d", tmpQueue[i]);
                }
                syslog(LOG_DEBUG, "arrived data:%s\n", strTmpData.c_str());
                
                
                nAvg = nSum / 8;
                if (nAvg > gSetting_system.WT_threshold_value)
                    nFlag = 1;
                
		syslog(LOG_DEBUG, "nFlag:%d\n", nFlag);

		// ���-����	
		if (nFlag == 1)
		{
                    g_WeightSetLock.lock();
                    g_WeightSet.push_back(nAvg);
                    g_WeightSetLock.unlock();
                    //RFID_upload_arrived_data(0, str1.c_str(), str2.c_str());
                    char szValue[10 + 1] = {0};
                    sprintf(szValue, "%d", nAvg);
                    WT_upload_arrived_data(0, szValue);
                    syslog(LOG_DEBUG, "upload arrived data nAvg=%d\n", nAvg);
		}

		//usleep(gSetting_system.DEV_scan_gap * 1000);
		sleep(3);
	}
	return 0;
}

void *StartEventThread(void *arg)
{
	signal(SIGPIPE,SIG_IGN);

        
	int nResult     = 0;
	while (1)
	{
            nResult = g_StartEventSem.wait();
            g_nArrivedFlagLock.lock();
            g_nArrivedFlag = 1;
            g_nArrivedFlagLock.unlock();

            g_WeightLock.lock();
            if (g_WeightQueue.size() > 0)
                g_WeightQueue.clear();
            g_WeightLock.unlock();
            
            g_WeightSetLock.lock();
            if (g_WeightSet.size() > 0)
                g_WeightSet.clear();
            g_WeightSetLock.unlock();            
            
            int nIterator   = 10;
            sleep(2);

jzlabel:     

            if (nIterator <= 0)
            {
                nIterator = 10;
                continue;
            }
    
            int nFlag = 0;                
            boost::circular_buffer<int> tmpQueue(10);

            g_WeightLock.lock();
            tmpQueue = g_WeightQueue;
            g_WeightLock.unlock();

            if (tmpQueue.size() >= 10)
            {
                nFlag = 1;
            }


            if (nFlag == 0)
            {
                int nCount = 5;
                int nSpan  = 5000 / 5;
                if (nSpan > 0)
                {
                    while (nCount)
                    {
                        sleep(1);
                        nCount--;
                        g_WeightLock.lock();
                        tmpQueue = g_WeightQueue;
                        g_WeightLock.unlock();

                        if (tmpQueue.size() >= 10)
                        {
                            nFlag = 1;
                            break;
                        }
                    }
                    
                    if (nFlag == 0)
                    {
                        sleep(1);
                        --nIterator;
                        goto jzlabel;
                    }
                }			
            }

            int nSum        = 0;
            int nDiffSum    = 0;
            int nAvg        = 0;
            int nDiffAvg    = 0;

            g_WeightLock.lock();
            tmpQueue = g_WeightQueue;
            g_WeightLock.unlock();

            int n = tmpQueue.size();
            std::string strTmpData = "";
            //for (int i = 0; i < n; ++i)
            for (int i = 1; i < n - 1; ++i)
            {
                nSum += tmpQueue[i];
                std::string strTmp = boost::lexical_cast<std::string>(tmpQueue[i]);
                strTmpData += strTmp;
                strTmpData += string("    ");
            }
            nAvg = nSum / 8;
            g_WeightSetLock.lock();
            if (nAvg > gSetting_system.WT_threshold_value)
                g_WeightSet.push_back(nAvg);
            g_WeightSetLock.unlock();
            
            n = tmpQueue.size();
            for (int i = 0; i < n - 1; ++i)
            {
                nDiffSum += abs(tmpQueue[i] - tmpQueue[i + 1]);
            }          
            

            if (n != 1)
                nDiffAvg = nDiffSum; // / (n - 1);
            else
            {
                sleep(1);
                --nIterator;
                goto jzlabel;
            }
            
            syslog(LOG_DEBUG, "gather data:%s\n", strTmpData.c_str());
            syslog(LOG_DEBUG, "nDiffAvg:%d, nAvg:%d\n", nDiffAvg, nAvg);

            int nMax = 0;
            std::map<int, int> tmpMap;
            n = tmpQueue.size();
            for (int i = 0; i < n; ++i)
            {
                std::map<int, int>::iterator it = tmpMap.find(tmpQueue[i]);
                if (it != tmpMap.end())
                {
                    ++(it->second);
                }
                else
                {
                    tmpMap.insert(make_pair(tmpQueue[i], 1));
                }
            }
            
            int nLastCount = 0;
            for (std::map<int, int>::iterator it = tmpMap.begin(); it != tmpMap.end(); ++it)
            {
                if (it->second > nLastCount)
                {
                    nMax = it->first;
                    nLastCount = it->second;
                }
            }
                                  
                        
//            int kk  = 0;

//            g_WeightSetLock.lock();
//            for (std::set<int, std::greater<int> >::iterator it = g_WeightSet.begin(); it != g_WeightSet.end(); ++it, ++kk)
//            {
//                if (kk == 1)
//                {
//                    nMax = *it;
//                    break;
//                }
//            }
//            if (!g_WeightSet.empty())
//                nMax = *g_WeightSet.begin();
            
//            g_WeightSetLock.unlock();
            // if (nDiffAvg <= gSetting_system.WT_average_diff_value && nAvg >= gSetting_system.WT_threshold_value)
            if (nDiffAvg <= gSetting_system.WT_average_diff_value && nMax >= gSetting_system.WT_threshold_value)
                nFlag = 1;
            else
            {
                sleep(1);
                --nIterator;
                goto jzlabel;
            }

            syslog(LOG_DEBUG, "nFlag:%d\n", nFlag);

            // ���-����	
            if (nFlag == 1)
            {
                syslog(LOG_DEBUG, "gather data:%s\n", strTmpData.c_str());
                //RFID_upload_arrived_data(0, str1.c_str(), str2.c_str());
                char szValue[10 + 1] = {0};
                sprintf(szValue, "%d", nMax);
                WT_upload_gather_data(0, szValue);
                syslog(LOG_DEBUG, "upload gather data nAvg=%d\n", nMax);
                
                // finished
                g_WeightLock.lock();
                if (g_WeightQueue.size() > 0)
                    g_WeightQueue.clear();
                g_WeightLock.unlock();
                
                g_WeightSetLock.lock();
                if (g_WeightSet.size() > 0)
                    g_WeightSet.clear();
                g_WeightSetLock.unlock();  
            }



            usleep(1000);
	}
	return 0;
}

void *StopEventThread(void *arg)
{
	signal(SIGPIPE,SIG_IGN);

	int nResult = 0;
	while (1)
	{
		nResult = g_StoppedEventSem.wait();
                
                g_WeightLock.lock();
                if (g_WeightQueue.size() > 0)
                    g_WeightQueue.clear();
                g_WeightLock.unlock();
                
                g_WeightSetLock.lock();
                if (g_WeightSet.size() > 0)
                    g_WeightSet.clear();
                g_WeightSetLock.unlock();  
		usleep(1000);

		g_nArrivedFlagLock.lock();
		g_nArrivedFlag = 0;
		g_nArrivedFlagLock.unlock();


		usleep(1000);
	}
	return 0;
}


void *TimerEventThread(void *arg)
{
	signal(SIGPIPE,SIG_IGN);

	while (1)
	{
		//syslog(LOG_DEBUG, "9999999999999999999999999999999999999999999999\n");
		time_t t 		= time(0);
		
		////
		//g_LabelMapLock.lock();
		//if (g_LabelMap.size() > 0)
		//	g_LabelMap.clear();
		//g_LabelMapLock.unlock();
		//usleep(1000);
		g_LabelMapLock.lock();
		for (std::map<std::string, strutLabelModifiers>::iterator it = g_LabelMap.begin(); it != g_LabelMap.end();)
		{
			if ((*it).second.nSendFlag == 1  &&  t - (*it).second.nTime > 60)
			{
				g_LabelMap.erase(it++);
			}
			else
			{
				++it;
			}
		}
		g_LabelMapLock.unlock();


		sleep(4);
	}
	return 0;
}

