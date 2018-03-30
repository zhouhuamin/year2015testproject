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
locker g_iccardLock;
string g_strArriveData;
string g_strIccard;

extern struct struDeviceAndServiceStatus g_ObjectStatus;
extern pthread_mutex_t g_StatusMutex;
extern std::map<std::string, strutLabelModifiers> g_LabelMap;
extern locker       		  g_LabelMapLock;
extern sem	g_StartEventSem;
extern sem	g_StoppedEventSem;

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
    signal(SIGPIPE, SIG_IGN);
    while (1)
    {
        card_block_t card_block     = {0};
        ret = ic_card_physics_read(&card_block);
        if (ret == 0)
        {
            if (card_block.len > 0)
            {
                g_nArrivedFlagLock.lock();
                g_strArriveData = card_block.data;
                g_nArrivedFlagLock.unlock();
                
                g_iccardLock.lock();
                g_strIccard = card_block.data;
                g_iccardLock.unlock();   
            }
        }
        else if( ret == -99)
        {                    
            syslog(LOG_DEBUG, "read ic error! \n");                      
        }
        
        usleep(gSetting_system.DEV_scan_gap * 1000);
    }

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
		char szMsg[1024] = {0};
		pthread_mutex_lock(&g_StatusMutex);
		BuildStatusString(&g_ObjectStatus, szMsg, i++);
		pthread_mutex_unlock(&g_StatusMutex);

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

	return 0;
}


void *ArrivedEventThread(void *arg)
{
    char szArriveData[256] = {0};
    signal(SIGPIPE,SIG_IGN);
    while (1)
    {
        memset(szArriveData,0,sizeof(szArriveData));
        int nArrivedFlag = 0;
        g_nArrivedFlagLock.lock();
        nArrivedFlag = g_nArrivedFlag;
        g_nArrivedFlagLock.unlock();
        if (nArrivedFlag == 1)
        {
            sleep(3);
            continue;
        }

        int nFlag = 0;
        g_nArrivedFlagLock.lock();
        if (g_strArriveData.length() > 0)
        {
            nFlag = 1;
            syslog(LOG_DEBUG, "nArrivedFlag:%d\n", nFlag);
            strcpy(szArriveData,g_strArriveData.c_str());
            g_strArriveData = "";
        }
        g_nArrivedFlagLock.unlock();

        if (nFlag == 1)
        {
            IC_upload_arrived_data(szArriveData);         
        }

        usleep(1000);
    }
    return 0;
}

void *StartEventThread(void *arg)
{
    signal(SIGPIPE,SIG_IGN);
    int nResult = 0;
    bool bStatus = false;//true:busying
    while (1)
    {    
        nResult = g_StartEventSem.wait();
        g_nArrivedFlagLock.lock();
        g_nArrivedFlag = 1;
        g_nArrivedFlagLock.unlock();
        
        g_iccardLock.lock();
        g_strIccard = "";
        g_iccardLock.unlock();  

        int nFlag = 0;
        while(1)
        {
            bStatus = true;
            g_iccardLock.lock();
            if (g_strIccard.length() > 0)
            {
                nFlag = 1;
                g_iccardLock.unlock();
                break;
            }
            g_iccardLock.unlock();

            usleep(100*1000);
        }

        if (nFlag == 1)
        {         
            char szBuffer[256] = {0};
            strcpy(szBuffer,g_strIccard.c_str());
            IC_upload_gather_data(szBuffer, gSetting_system.gather_event_id);
            g_iccardLock.lock();
            g_strIccard = "";
            g_iccardLock.unlock();
            syslog(LOG_DEBUG, "******************upload finshed data.\n");
        }
        
        bStatus = false;
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
        g_nArrivedFlagLock.lock();
        g_nArrivedFlag = 0;
        g_strArriveData = "";
        g_nArrivedFlagLock.unlock();

        g_iccardLock.lock();
        g_strIccard = "";
        g_iccardLock.unlock();

        usleep(1000);
    }
    return 0;
}

void *TimerEventThread(void *arg)
{
    return 0;
}

int IC_upload_gather_data(char *ic_number, char *pEventID)
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

//	xmlNewChild(root_node, NULL, BAD_CAST "IC_BUSS_TYPE", BAD_CAST "");
//	xmlNewChild(root_node, NULL, BAD_CAST "IC_EX_DATA", BAD_CAST "");

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

	syslog(LOG_DEBUG, "upload gather data w_len = %d\n", w_len);
	ReceiveReaderData(p_gather_data->xml_data, xml_len, p_gather_data->event_id);
	return 0;
}

int IC_upload_arrived_data(char *ic_number)
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

//	xmlNewChild(root_node, NULL, BAD_CAST "IC_BUSS_TYPE", BAD_CAST "");
//	xmlNewChild(root_node, NULL, BAD_CAST "IC_EX_DATA", BAD_CAST "");

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

	syslog(LOG_DEBUG, "upload arrived data w_len = %d\n", w_len);  
    ReceiveReaderData(0, 0, p_gather_data->event_id);
	return 0;
}

