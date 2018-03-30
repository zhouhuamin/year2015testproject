#include "includes.h"
#include "reader1000api.h"
#include "dev.h"
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
extern sem	g_StartEventSem;
extern sem	g_StoppedEventSem;


int add_card_to_list(card_block_t *pBlock)
{
    if (pBlock == NULL)
        return 0;
    
    int ret;
    u8 *p_data;
    int i;

    p_data = pBlock->data;

    char ic_number[200] = {0};
    if (gSetting_system.IC_read_block_NO == 1)  //ic card
    {
            for (i = 0; i < pBlock->len; i++)
            {
                    if (isprint(p_data[i]))
                    {
                            ic_number[i] = p_data[i];
                    }
                    else
                    {
                            ic_number[i] = '\0';
                            break;
                    }
            }
            ic_number[i] = '\0';
    }
    else
    {
            for (i = 0; i < pBlock->len; i++)
            {
                    sprintf(ic_number + strlen(ic_number), "%02X", p_data[i]);
            }
    }

    ic_number[199] = '\0';
    std::map<std::string, strutLabelModifiers>::iterator labelMapIter;

    std::string strTmpLabel = ic_number;
    strutLabelModifiers	modifiers = {0};
    modifiers.nCount		= 1;
    modifiers.nTime		= time(0);
    modifiers.nSendFlag		= 0;
    //syslog(LOG_DEBUG, "strTmpLabel:%s\n", strTmpLabel.c_str());

    g_LabelMapLock.lock();
    labelMapIter = g_LabelMap.find(strTmpLabel);
    if (labelMapIter != g_LabelMap.end())
    {
            ++(labelMapIter->second.nCount);
            //syslog(LOG_DEBUG, "11111strTmpLabel:%s\n", strTmpLabel.c_str());
    }
    else
    {
            g_LabelMap.insert(make_pair(strTmpLabel, modifiers));
            //syslog(LOG_DEBUG, "22222strTmpLabel:%s\n", strTmpLabel.c_str());
    }
    g_LabelMapLock.unlock();
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
            //strcpy(pReadData->xml_data, szXMLGatherInfo);
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
	int ret;
	int connect_failed_cnt = 0;

	signal(SIGPIPE, SIG_IGN);
	while (1)
	{
                //card_block_t old_card_block = {0};
                card_block_t card_block     = {0};
		//ret = ic_card_section_read(gFd_dev, 1, &card_block);
                ret = ic_card_section_read(gFd_dev, 0, &card_block);
		if (ret == 0)
		{
			if (card_block.len > 0)
			{
//				if (0 != compare_ic_block(&old_card_block, &card_block))
//				{
//					memcpy(&old_card_block, &card_block, sizeof(card_block_t));
//					dev_send_msg(msg_DEV_NEW_DATA, 0);
//					ic_app_beep(gFd_dev, 30);
//				}
                                add_card_to_list(&card_block);
                                ic_app_beep(gFd_dev, 30);
			}
			connect_failed_cnt = 0;
		}
		else if( ret == -99)
		{
			syslog(LOG_DEBUG, "read ic error!\n");
			connect_failed_cnt++;
			if (connect_failed_cnt >= 15)
			{
				connect_failed_cnt = 0;
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
//		strcpy(g_ObjectStatus.szObjectCode,			"DEV_IC_001");
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
				//RFID_upload_arrived_data(0, str1.c_str(), str2.c_str());
                                IC_upload_arrived_data(0, (char*)str1.c_str());
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
		g_nArrivedFlagLock.lock();
		g_nArrivedFlag = 1;
		g_nArrivedFlagLock.unlock();


		int nFlag = 0;
		g_LabelMapLock.lock();
		if (g_LabelMap.size() > 0)
		{
			nFlag = 1;
		}
		g_LabelMapLock.unlock();

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
					g_LabelMapLock.lock();
					if (g_LabelMap.size() > 0)
					{
						g_LabelMapLock.unlock();
						nFlag = 1;
						break;
					}
					g_LabelMapLock.unlock();
				}

				if (nFlag == 0)
					continue;
			}			
		}

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
					(*it).second.nSendFlag	= 1;
					++nIterator;
				}
				else if ((*it).second.nSendFlag == 0  &&  nIterator == 1)
				{
					str2	= (*it).first;
					nCountValue2 = (*it).second.nCount;
					nTimeValue2             = (*it).second.nTime;
					(*it).second.nSendFlag	= 1;
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
				syslog(LOG_DEBUG, "Started Read Count1=%d, Count2=%d\n", nCountValue1, nCountValue2);
				//RFID_upload_gather_data(0, str1.c_str(), str2.c_str());
                                IC_upload_gather_data(0, (char*)str1.c_str());
			}
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
			if ((*it).second.nSendFlag == 1  &&  t - (*it).second.nTime > 5)
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

int IC_upload_gather_data(int server_fd, char *ic_number)
{
	NET_gather_data *p_gather_data;
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	int w_len;
	xmlChar * xml_buf;
	char *p_send;
	int xml_len;
	int ret;
	char str_tmp[100];

	packet_head.msg_type = MSG_TYPE_GATHER_DATA;
	strcpy(packet_head.szVersion, "1.0");

	p_gather_data = (NET_gather_data *) (data_buf + sizeof(net_packet_head_t)
			+ 8);

	strcpy(p_gather_data->event_id, gSetting_system.gather_event_id);
	ret = 0;
	memcpy(&(p_gather_data->is_data_full), &ret, sizeof(int));
	strcpy(p_gather_data->dev_tag, gSetting_system.gather_DEV_TAG);

	/**********************************XML*********************************/
	doc = xmlNewDoc(NULL);

	root_node = xmlNewNode(NULL, BAD_CAST "IC");

	xmlDocSetRootElement(doc, root_node);

	xmlNewChild(root_node, NULL, BAD_CAST "DR_IC_NO", BAD_CAST (ic_number));

	xmlNewChild(root_node, NULL, BAD_CAST "IC_DR_CUSTOMS_NO", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_CO_CUSTOMS_NO", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_BILL_NO", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_GROSS_WT", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_VE_CUSTOMS_NO", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_VE_NAME", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_CONTA_ID", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_ESEAL_ID", BAD_CAST "");

	xmlNewChild(root_node, NULL, BAD_CAST "IC_BUSS_TYPE", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_EX_DATA", BAD_CAST "");

	xmlDocDumpFormatMemoryEnc(doc, &xml_buf, &xml_len, "GB2312", 1);

	p_send = strstr((char*) xml_buf, "<IC>");

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

////	LOG("upload gather data w_len = %d\n", w_len);
//	ret = write(server_fd, data_buf, w_len);
//	if (ret != w_len)
//	{
//		syslog(LOG_DEBUG, "upload gather data error! ret =%d\n", ret);
//	}
//
//	sprintf(str_tmp, "RE:\t%s\n", ic_number);
//	log_send(str_tmp);
	syslog(LOG_DEBUG, "upload gather data w_len = %d\n", w_len);
	ReceiveReaderData(p_gather_data->xml_data, xml_len, p_gather_data->event_id);
	return 0;
}

int IC_upload_arrived_data(int server_fd, char *ic_number)
{
	NET_gather_data *p_gather_data;
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	int w_len;
	xmlChar * xml_buf;
	char *p_send;
	int xml_len;
	int ret;
	char str_tmp[100];

	packet_head.msg_type = MSG_TYPE_GATHER_DATA;
	strcpy(packet_head.szVersion, "1.0");

	p_gather_data = (NET_gather_data *) (data_buf + sizeof(net_packet_head_t)
			+ 8);

	strcpy(p_gather_data->event_id, gSetting_system.arrived_DEV_event_id);
	ret = 0;
	memcpy(&(p_gather_data->is_data_full), &ret, sizeof(int));
	strcpy(p_gather_data->dev_tag, gSetting_system.gather_DEV_TAG);

	/**********************************XML*********************************/
	doc = xmlNewDoc(NULL);

	root_node = xmlNewNode(NULL, BAD_CAST "IC");

	xmlDocSetRootElement(doc, root_node);

	xmlNewChild(root_node, NULL, BAD_CAST "DR_IC_NO", BAD_CAST (ic_number));

	xmlNewChild(root_node, NULL, BAD_CAST "IC_DR_CUSTOMS_NO", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_CO_CUSTOMS_NO", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_BILL_NO", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_GROSS_WT", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_VE_CUSTOMS_NO", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_VE_NAME", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_CONTA_ID", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_ESEAL_ID", BAD_CAST "");

	xmlNewChild(root_node, NULL, BAD_CAST "IC_BUSS_TYPE", BAD_CAST "");
	xmlNewChild(root_node, NULL, BAD_CAST "IC_EX_DATA", BAD_CAST "");

	xmlDocDumpFormatMemoryEnc(doc, &xml_buf, &xml_len, "GB2312", 1);

	p_send = strstr((char*) xml_buf, "<IC>");

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

//	ret = write(server_fd, data_buf, w_len);
//	if (ret != w_len)
//	{
//		LOG_M("upload gather data error! ret =%d\n", ret);
//	}
//
//	sprintf(str_tmp, "ARRIVED:\t%s\n", ic_number);
//	log_send(str_tmp);
	syslog(LOG_DEBUG, "upload arrived data w_len = %d\n", w_len);
        syslog(LOG_DEBUG, "%s\n", p_gather_data->xml_data);   
        ReceiveReaderData(0, 0, p_gather_data->event_id);
//	ReceiveReaderData(p_gather_data->xml_data, xml_len, p_gather_data->event_id);
	return 0;
}

