#include "includes.h"
#include "reader1000api.h"
#include "dev.h"
#include "LED.h"
#include "packet_format.h"
#include "starup_handle.h"
#include "LED.h"
#include "signal.h"

#include <iconv.h>
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
#include <algorithm>
#include <iterator>

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
extern std::vector<structLedMsg>            g_MsgVect;
extern locker                              g_MsgVectLock;
extern sem	g_StartEventSem;
extern sem	g_StoppedEventSem;
static int msg_LED;

static char gb_buf[200];

static void LED_msg_queue_init(void);
static int LED_msg_send(u8 cmd, u32 param, char *led_msg);
static int msg_recv_LED(u32 *p_item_id, char *led_msg);
int led_show_handle(int fd_dev);
int code_convert(const char *from_charset, const char *to_charset, char *inbuf, size_t inlen, char *outbuf, size_t outlen);


int led_msg_show(char *xml_buf, int xml_buf_len)
{
    syslog(LOG_DEBUG, "Enter led_msg_show\n");
	xmlDocPtr doc;
	xmlNodePtr root_node, cur_node;
	xmlChar *led_msg, *str_tmp;
	int led_code = 0;
	char log_str[160];
	int len;
	int i;
        
        char *pXmlData = new char[xml_buf_len + 1];
        memcpy(pXmlData, xml_buf, xml_buf_len);
        pXmlData[xml_buf_len] = '\0';
        std::string strXmlDataFront = "<?xml version=\"1.0\" encoding=\"GB2312\"?>";
        std::string strXmlDataBack  = pXmlData;
        std::string strXmlData      = strXmlDataFront;
        //strXmlData      += strXmlDataBack;
        
        char *pItemStart    = strstr(pXmlData, "<LED>");
        char *pItemEnd      = strstr(pXmlData, "</LED>");
        if (pItemStart != NULL && pItemEnd != NULL && pItemStart < pItemEnd)
        {
            int nFront = pItemEnd - pItemStart + 6;
            char *pTmpData = new char[nFront + 1];
            memcpy(pTmpData, pItemStart, nFront);
            pTmpData[nFront]    = '\0';
            strXmlDataBack = pTmpData;
            delete []pTmpData;
            strXmlData      += strXmlDataBack;
        }
        else
        {
            delete []pXmlData;    
            return 0;
        }
        delete []pXmlData;
        syslog(LOG_DEBUG, "led_msg_show:%s\n", strXmlData.c_str());
        

//	doc = xmlParseMemory(xml_buf, xml_buf_len);
//	doc = xmlReadMemory(xml_buf, xml_buf_len, NULL, "GB2312", 1);
        doc = xmlReadMemory(strXmlData.c_str(), strXmlData.size(), NULL, "GB2312", 1);
	if ( NULL == doc)
	{
		syslog(LOG_DEBUG, "open xml file error!\n");
		return -1;
	}

	root_node = xmlDocGetRootElement(doc);
	if ( NULL == root_node)
	{
		syslog(LOG_DEBUG, "get the root node error!\n");
		xmlFreeDoc(doc);
		return -1;
	}

	if (xmlStrcmp(root_node->name, BAD_CAST "LED"))
	{
		syslog(LOG_DEBUG, "the root node name  error!\n");
		xmlFreeDoc(doc);
		return -2;
	}

	cur_node = root_node->children;
	while ( NULL != cur_node)
	{
		if (!xmlStrcmp(cur_node->name, BAD_CAST "LED_CODE"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == interger_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%d", &led_code);
			}
			else
			{
				syslog(LOG_DEBUG, "load led code error!\n");
			}
			xmlFree(str_tmp);
                        
                        syslog(LOG_DEBUG, "led_code:%d!\n", led_code);
                        if (led_code == 1)
                        {
                            char tmp_buf[200] = {0};
                            std::string strFromCharset  = "utf-8";
                            std::string strToCharset    = "gb2312";
                            std::string led_msg = "不查验，请直行";
                            code_convert(strFromCharset.c_str(), strToCharset.c_str(), (char *)led_msg.c_str(), strlen(led_msg.c_str()), tmp_buf, sizeof(tmp_buf));
                            structLedMsg tmpMsg;
                            tmpMsg.id       = 1;
                            tmpMsg.strMsg   = tmp_buf;
                            g_MsgVectLock.lock();
                            g_MsgVect.push_back(tmpMsg);
                            g_MsgVectLock.unlock();
                            xmlFreeDoc(doc);
                            return 0;

                        }
                        else if (led_code == 2)
                        {
                            char tmp_buf[200] = {0};
                            std::string strFromCharset  = "utf-8";
                            std::string strToCharset    = "gb2312";
                            std::string led_msg = "请到查验区进行查验";
                            code_convert(strFromCharset.c_str(), strToCharset.c_str(), (char *)led_msg.c_str(), strlen(led_msg.c_str()), tmp_buf, sizeof(tmp_buf));
                            structLedMsg tmpMsg;
                            tmpMsg.id       = 2;
                            tmpMsg.strMsg   = tmp_buf;
                            g_MsgVectLock.lock();
                            g_MsgVect.push_back(tmpMsg);
                            g_MsgVectLock.unlock();
                            xmlFreeDoc(doc);
                            return 0;
                        }
                        else if (led_code == 3)
                        {
                            char tmp_buf[200] = {0};
                            std::string strFromCharset  = "utf-8";
                            std::string strToCharset    = "gb2312";
                            std::string led_msg = "放行抬杆";
                            code_convert(strFromCharset.c_str(), strToCharset.c_str(), (char *)led_msg.c_str(), strlen(led_msg.c_str()), tmp_buf, sizeof(tmp_buf));
                            structLedMsg tmpMsg;
                            tmpMsg.id       = 3;
                            tmpMsg.strMsg   = tmp_buf;
                            g_MsgVectLock.lock();
                            g_MsgVect.push_back(tmpMsg);
                            g_MsgVectLock.unlock();
                            xmlFreeDoc(doc);
                            return 0;
                        }
                        else if (led_code == 4)
                        {
                            char tmp_buf[200] = {0};
                            std::string strFromCharset  = "utf-8";
                            std::string strToCharset    = "gb2312";
                            std::string led_msg = "不放行";
                            code_convert(strFromCharset.c_str(), strToCharset.c_str(), (char *)led_msg.c_str(), strlen(led_msg.c_str()), tmp_buf, sizeof(tmp_buf));
                            structLedMsg tmpMsg;
                            tmpMsg.id       = 4;
                            tmpMsg.strMsg   = tmp_buf;
                            g_MsgVectLock.lock();
                            g_MsgVect.push_back(tmpMsg);
                            g_MsgVectLock.unlock();
                            xmlFreeDoc(doc);
                            return 0;
                        }
                        else if (led_code == 5)
                        {
                            char tmp_buf[200] = {0};
                            std::string strFromCharset  = "utf-8";
                            std::string strToCharset    = "gb2312";
                            std::string led_msg = "重量异常不抬杆";
                            code_convert(strFromCharset.c_str(), strToCharset.c_str(), (char *)led_msg.c_str(), strlen(led_msg.c_str()), tmp_buf, sizeof(tmp_buf));
                            structLedMsg tmpMsg;
                            tmpMsg.id       = 5;
                            tmpMsg.strMsg   = tmp_buf;
                            g_MsgVectLock.lock();
                            g_MsgVect.push_back(tmpMsg);
                            g_MsgVectLock.unlock();
                            xmlFreeDoc(doc);
                            return 0;
                        }
                        else if (led_code == 6)
                        {
                            char tmp_buf[200] = {0};
                            std::string strFromCharset  = "utf-8";
                            std::string strToCharset    = "gb2312";
                            std::string led_msg = "刷卡成功";
                            code_convert(strFromCharset.c_str(), strToCharset.c_str(), (char *)led_msg.c_str(), strlen(led_msg.c_str()), tmp_buf, sizeof(tmp_buf));
                            structLedMsg tmpMsg;
                            tmpMsg.id       = 6;
                            tmpMsg.strMsg   = tmp_buf;
                            g_MsgVectLock.lock();
                            g_MsgVect.push_back(tmpMsg);
                            g_MsgVectLock.unlock();
                            xmlFreeDoc(doc);
                            return 0;                            
                        }
                        else if (led_code == 7)
                        {
                            char tmp_buf[200] = {0};
                            std::string strFromCharset  = "utf-8";
                            std::string strToCharset    = "gb2312";
                            std::string led_msg = "人工放行";
                            code_convert(strFromCharset.c_str(), strToCharset.c_str(), (char *)led_msg.c_str(), strlen(led_msg.c_str()), tmp_buf, sizeof(tmp_buf));
                            structLedMsg tmpMsg;
                            tmpMsg.id       = 7;
                            tmpMsg.strMsg   = tmp_buf;
                            g_MsgVectLock.lock();
                            g_MsgVect.push_back(tmpMsg);
                            g_MsgVectLock.unlock();
                            xmlFreeDoc(doc);
                            return 0;                            
                        }
                        
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "LED_MSG"))
		{
                        std::string strFromCharset  = "utf-8";
                        std::string strToCharset    = "gb2312";
			led_msg = xmlNodeGetContent(cur_node);
//                        memcpy(gb_buf, (char*)led_msg, strlen((char*)led_msg));
			code_convert(strFromCharset.c_str(), strToCharset.c_str(), (char *)led_msg, strlen((char*)led_msg), gb_buf, sizeof(gb_buf));

//			len = strlen(led_msg);
//
//			LOG("len = %d\n", len);
//			for (i = 0; i < len; i++)
//			{
//				sprintf(log_str + strlen(log_str), "%0.2X", led_msg[i]);
//			}
//			LOG(log_str);
		}
		cur_node = cur_node->next;
	}

	//LED_msg_send(0, led_code, (char*) gb_buf);
        structLedMsg tmpMsg;
        tmpMsg.id       = led_code;
        tmpMsg.strMsg   = gb_buf;
        g_MsgVectLock.lock();
        g_MsgVect.push_back(tmpMsg);
        g_MsgVectLock.unlock();

//	sprintf(log_str, "ASK:\tID:%d\tMSG:%s\n", led_code, (char*) led_msg);
//	log_send(log_str);

	xmlFree(led_msg);
	xmlFreeDoc(doc);
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
	strncpy(szXMLGatherInfo, pXmlData, nXmlLen);


	syslog(LOG_DEBUG, "%s\n", szXMLGatherInfo);
	strcpy(pReadData->xml_data, szXMLGatherInfo);
	pReadData->xml_data_len = strlen(szXMLGatherInfo) + 1;
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
	int ret;
	int retry_cnt = 0;

	//LED_msg_queue_init();
	signal(SIGPIPE,SIG_IGN);

	while (1)
	{
		ret = led_show_handle(gFd_dev);
		if (-99 == ret)  //time out
		{
			retry_cnt++;
			if (retry_cnt >= 3)
			{
				retry_cnt = 0;
				dev_retry_connect_handle(&gFd_dev);
			}
		}
		else
		{
			retry_cnt = 0;
		}

                //sleep(1);
		usleep(500 * 1000);
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
	ReceiveReaderData(p_gather_data->xml_data, xml_len, p_gather_data->event_id);


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
//		strcpy(g_ObjectStatus.szObjectCode,			"DEV_LED_001");
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

		int nFlag = 0;
		g_LabelMapLock.lock();
		if (g_LabelMap.size() > 0)
		{
			nFlag = 1;
		}
		g_LabelMapLock.unlock();
		syslog(LOG_DEBUG, "nFlag:%d\n", nFlag);

		// ���-����	
		if (nFlag == 1)
		{
			int nIterator = 0;
			std::string str1 = "";
			std::string str2 = "";
			int nCountValue1 = 0;
			int nCountValue2 = 0;
			int nTimeValue1  = 0;
			int nTimeValue2  = 0;
			g_LabelMapLock.lock();
			for (std::map<std::string, strutLabelModifiers>::iterator it = g_LabelMap.begin(); it != g_LabelMap.end(); ++it)
			{
				if ((*it).second.nSendFlag == 0  &&  nIterator == 0)
				{
					str1					= (*it).first;
					nCountValue1			= (*it).second.nCount;
					nTimeValue1             = (*it).second.nTime;
					//(*it).second.nSendFlag	= 1;
					++nIterator;
				}
				else if ((*it).second.nSendFlag == 0  &&  nIterator == 1)
				{
					str2	= (*it).first;
					nCountValue2 = (*it).second.nCount;
					nTimeValue2             = (*it).second.nTime;
					//(*it).second.nSendFlag	= 1;
					++nIterator;
				}
			}
			g_LabelMapLock.unlock();

			//if (nCountValue2 != 0 && nCountValue1 > nCountValue2 && abs(nCountValue1 - nCountValue2) >= 1)
			//{
			//	str2 = "";
			//}

			//if (nCountValue2 != 0 && nCountValue2 > nCountValue1 && abs(nCountValue2 - nCountValue1) >= 1)
			//{
			//	str1 = str2;
			//	str2 = "";
			//}

			if (nCountValue2 != 0 && nTimeValue1 > nTimeValue2 && abs(nTimeValue1 - nTimeValue2) >= 1)
			{
				str2 = "";
			}

			if (nCountValue2 != 0 && nTimeValue2 > nTimeValue1 && abs(nTimeValue2 - nTimeValue1) >= 1)
			{
				str1 = str2;
				str2 = "";
			}

			if (str1.empty() && str2.empty())
			{

			}
			else
			{
				syslog(LOG_DEBUG, "Arrived Read Count1=%d, Count2=%d\n", nCountValue1, nCountValue2);
				RFID_upload_arrived_data(0, str1.c_str(), str2.c_str());
			}
		}

		//usleep(gSetting_system.DEV_scan_gap * 1000);
		sleep(3);
	}
	return 0;
}

void *StartEventThread(void *arg)
{
	signal(SIGPIPE,SIG_IGN);

	int nResult = 0;
	while (1)
	{
		nResult = g_StartEventSem.wait();

		int nFlag = 0;
		g_MsgVectLock.lock();
		if (g_MsgVect.size() > 0)
		{
			nFlag = 1;
		}
		g_MsgVectLock.unlock();

		if (nFlag == 1)
		{
                    //syslog(LOG_DEBUG, "Started Read Count1=%d, Count2=%d\n", nCountValue1, nCountValue2);
                    //RFID_upload_gather_data(0, str1.c_str(), str2.c_str());
		}		
		// finished

		////
		//g_LabelMapLock.lock();
		//if (g_LabelMap.size() > 0)
		//	g_LabelMap.clear();
		//g_LabelMapLock.unlock();



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

		////
		//g_LabelMapLock.lock();
		//if (g_LabelMap.size() > 0)
		//	g_LabelMap.clear();
		//g_LabelMapLock.unlock();
		//usleep(1000);

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

int led_show_handle(int fd_dev)
{
	u32 item_id;
	char led_msg[140] = { 0 };

	int ret = 0;
        std::string strMsg = "";

	//ret = msg_recv_LED(&item_id, led_msg);
        
        g_MsgVectLock.lock();
        if (g_MsgVect.empty())
        {
            ret = 0;
        }
        else
        {
            ret     = 1;
            item_id = g_MsgVect[0].id;
            strMsg  = g_MsgVect[0].strMsg;
            g_MsgVect.erase(g_MsgVect.begin());
            //g_MsgVect.clear();
        }
        g_MsgVectLock.unlock();      
        
	if (ret > 0)
	{
//		if (strlen(led_msg) > 0)
//		{
                memcpy(led_msg, strMsg.c_str(), strMsg.size());
		led_print_string(fd_dev, 0xF, led_msg);
                if (strMsg.size() > 8)
                {
                    sleep(2);
                    usleep(500 * 1000);                    
                }                

                return 0;
//		}
//		else
//		{
//			return led_print_item_ID(fd_dev, item_id);
//		}
	}
	else
	{
            //sleep(1);
            usleep(500 * 1000);  
		return led_print_item_ID(fd_dev, 0);
	}

	return 0;
}

static void LED_msg_queue_init(void)
{
	key_t key;

	system("touch "LED_NEW_INFO_QUEUE_TMP_FILE);
	key = ftok(LED_NEW_INFO_QUEUE_TMP_FILE, 'a');

	msg_LED = msgget(key, IPC_CREAT | 0666);
	if (msg_LED < 0)
	{
		LOG("mag play record id < 0!\n");
		exit(0);
	}
}

int LED_msg_send(u8 cmd, u32 param, char *led_msg)
{
	msg_event_t msg;
	int ret;

	msg.msg_type = MSG_FROM_DEV_PTHREAD;

	msg.msg.cmd = cmd;
	msg.msg.src_id = 0;
	msg.msg.parame = param;

	if (led_msg == NULL)
	{
		return -1;
	}

	strcpy(msg.msg.str, led_msg);

	ret = msgsnd(msg_LED, &msg, sizeof(msg.data), IPC_NOWAIT);
	if (ret < 0)
	{
		printf("send msg error\n");
	}
	return 0;
}

static int msg_recv_LED(u32 *p_item_id, char *led_msg)
{
	msg_event_t msg;
	int ret;

	ret = msgrcv(msg_LED, &msg, sizeof(msg.data), MSG_FROM_DEV_PTHREAD,
	IPC_NOWAIT);

	if (ret > 0)
	{
		*p_item_id = msg.msg.parame;
		strcpy(led_msg, msg.msg.str);
		return 1;
	}
	return 0;
}

int code_convert(const char *from_charset, const char *to_charset, char *inbuf, size_t inlen,
		char *outbuf, size_t outlen)
{
	iconv_t cd;
	int rc;
	char **pin = &inbuf;
	char **pout = &outbuf;

	cd = iconv_open(to_charset, from_charset);
	if (cd == 0)
		return -1;
	memset(outbuf, 0, outlen);
	if (iconv(cd, pin, &inlen, pout, &outlen) == -1)
		return -1;
	iconv_close(cd);
	return 0;
}


void *pthread_dev_handle_test(void *arg)
{
    int nResult = 0;
    signal(SIGPIPE, SIG_IGN);

    while (1)
    {
        int ch = getchar();

        if (ch == 'p')
        {
            std::string strMsg = "<CONTROL_INFO> <EVENT type=\"CAR\" hint=\"放行抬杆\" eventid=\"EC_FREE_PLATFORM\"><![CDATA[<LED>"
"<LED_CODE>2</LED_CODE>"
"<LED_MSG>禁止抬杆</LED_MSG>"
"</LED>"
"<BROADCAST>"
"<BROADCAST_CODE>3</BROADCAST_CODE>"
"<BROADCAST_MSG>禁止抬杆</BROADCAST_MSG>"
"</BROADCAST>"
"<PRINT>"
"<VENAME></VENAME>"
"<CONTA_ID_F></CONTA_ID_F>"
"<CONTA_ID_B></CONTA_ID_B>"
"<CONTA_MODEL_F></CONTA_MODEL_F>"
"<CONTA_MODEL_B></CONTA_MODEL_B>"
"<Oper_Name></Oper_Name>"
"</PRINT>"
"]]></EVENT> </CONTROL_INFO>";
            
            
            
            char xml_buf[1024] = {0};
            memcpy(xml_buf, strMsg.c_str(), strMsg.size());
            int xml_buf_len = strMsg.size();
           led_msg_show(xml_buf, xml_buf_len);     
        }

        sleep(3);
    }

    return 0;
}
