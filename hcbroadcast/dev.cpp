#include "includes.h"
#include "reader1000api.h"
#include "dev.h"
#include "broadcast.h"
#include "packet_format.h"
#include "starup_handle.h"
#include "broadcast.h"
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
#include "ProcessBroadcast.h"
#include <map>
#include <algorithm>
#include <iterator>
#include <boost/timer.hpp>

extern int gFd_dev;
static net_packet_head_t packet_head;
static u8 data_buf[0x800];
static char tmp_str[150];
int g_nArrivedFlag = 0;
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
extern sem	g_BroadcastEventSem;
static int msg_LED;

static char gb_buf[200];
int code_convert(const char *from_charset, const char *to_charset, char *inbuf, size_t inlen, char *outbuf, size_t outlen);

int broadcast_msg_play(char *xml_buf, int xml_buf_len)
{
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

    char *pItemStart    = strstr(pXmlData, "<BROADCAST>");
    char *pItemEnd      = strstr(pXmlData, "</BROADCAST>");
    if (pItemStart != NULL && pItemEnd != NULL && pItemStart < pItemEnd)
    {
        int nFront = pItemEnd - pItemStart + 12;
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

	if (xmlStrcmp(root_node->name, BAD_CAST "BROADCAST"))
	{
		syslog(LOG_DEBUG, "the root node name  error!\n");
		return -2;
	}

	cur_node = root_node->children;
	while ( NULL != cur_node)
	{
		if (!xmlStrcmp(cur_node->name, BAD_CAST "BROADCAST_CODE"))
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
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "BROADCAST_MSG"))
		{          
            std::string strFromCharset  = "utf-8";
            std::string strToCharset    = "gb2312";
			led_msg = xmlNodeGetContent(cur_node);
			code_convert(strFromCharset.c_str(), strToCharset.c_str(), (char *)led_msg, strlen((char*)led_msg), gb_buf, sizeof(gb_buf));

		}
		cur_node = cur_node->next;
	}
        
    structLedMsg tmpMsg;
    tmpMsg.id       = led_code;
    tmpMsg.strMsg   = gb_buf;
    g_MsgVectLock.lock();
    g_MsgVect.push_back(tmpMsg);
    g_MsgVectLock.unlock();

    g_BroadcastEventSem.post();

	xmlFree(led_msg);
	xmlFreeDoc(doc);
	return 0;
}

int broadcast_setvolume_handle()
{
    syslog(LOG_DEBUG, "==================================Set volume Start===============================\n");   

    char pNewBuffer[1024] = {0};
    int nLen = 0;
    ProcessBroadcast broadcast;
    int nNewSocket = broadcast.CreateSocket();
    if (nNewSocket == -1)
    {
        //	���ܽ����׽��֣�ֱ�ӷ���
        syslog(LOG_DEBUG, "Can't create socket\n");
        sleep(3);
        nNewSocket = broadcast.CreateSocket();
        if(nNewSocket == -1)
        {
                return -1;
        }		
    }

    struct timeval timeout = {3, 0};
    socklen_t len = sizeof(timeout);
    setsockopt(nNewSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    setsockopt(nNewSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);

    if (broadcast.ConnectSocket(nNewSocket, gSetting_system.com_ip, gSetting_system.com_port) <= 0)
    {
        broadcast.CloseSocket(nNewSocket);
        syslog(LOG_DEBUG, "Time Out!\n");
        
        pthread_mutex_lock(&g_StatusMutex);
        strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
        g_ObjectStatus.nLocalPort		= 0;
        strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
        g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
        strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
        g_ObjectStatus.nServiceType	= 0;
        strcpy(g_ObjectStatus.szObjectCode, gSetting_system.register_DEV_ID);
        strcpy(g_ObjectStatus.szObjectStatus, "021");
        strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");
        {
            char szNowTime[32] = {0};
            GetCurTime(szNowTime);
            szNowTime[31] = '\0';
            strcpy(g_ObjectStatus.szReportTime, szNowTime);
        }
        pthread_mutex_unlock(&g_StatusMutex);
            
        return -1;
    }

    broadcast.set_keep_live(nNewSocket, 3, 5);
    broadcast_setvolume(nNewSocket, gSetting_system.Broadcast_volume);
    nLen = broadcast.SocketRead(nNewSocket, pNewBuffer, 1);	
    syslog(LOG_DEBUG, "recv  nLen=%d\n", nLen);
    broadcast.CloseSocket(nNewSocket);
    syslog(LOG_DEBUG, "==================================Set volume Stop===============================\n");         

    pthread_mutex_lock(&g_StatusMutex);
    strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
    g_ObjectStatus.nLocalPort		= 0;
    strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
    g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
    strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
    g_ObjectStatus.nServiceType	= 0;
    strcpy(g_ObjectStatus.szObjectCode, gSetting_system.register_DEV_ID);
    strcpy(g_ObjectStatus.szObjectStatus, "020");
    strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");
    {
        char szNowTime[32] = {0};
        GetCurTime(szNowTime);
        szNowTime[31] = '\0';
        strcpy(g_ObjectStatus.szReportTime, szNowTime);
    }
    pthread_mutex_unlock(&g_StatusMutex);
        
    return 0;
}

int broadcast_trans_handle()
{
    int ret = 0;
    u32 item_id;
    char led_msg[140] = { 0 };
    std::string strMsg = "";   
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
    }
    g_MsgVectLock.unlock();   

    if (ret > 0)
    {
        memcpy(led_msg, strMsg.c_str(), strMsg.size());
        syslog(LOG_DEBUG, "==================================Play Start===============================\n");   
        boost::timer tt;

        char pNewBuffer[1024] = {0};
        int nLen = 0;
        ProcessBroadcast broadcast;
        int nNewSocket = broadcast.CreateSocket();
        if (nNewSocket == -1)
        {
            //	���ܽ����׽��֣�ֱ�ӷ���
            syslog(LOG_DEBUG, "Can't create socket\n");
            sleep(3);
            nNewSocket = broadcast.CreateSocket();
            if(nNewSocket == -1)
            {
                    return -1;
            }		
        }

        struct timeval timeout = {3, 0};
        socklen_t len = sizeof(timeout);
        setsockopt(nNewSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
        setsockopt(nNewSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);

        if (broadcast.ConnectSocket(nNewSocket, gSetting_system.com_ip, gSetting_system.com_port) <= 0)
        {
            broadcast.CloseSocket(nNewSocket);
            syslog(LOG_DEBUG, "Time Out!\n");
            return -1;
        }

        broadcast.set_keep_live(nNewSocket, 3, 5);
        broadcast_item_ID(nNewSocket, item_id);
        nLen = broadcast.SocketRead(nNewSocket, pNewBuffer, 1);	
        syslog(LOG_DEBUG, "recv  nLen=%d\n", nLen);
        broadcast.CloseSocket(nNewSocket);
        syslog(LOG_DEBUG, "==================================Play Stop:%.3f===============================\n", tt.elapsed());         
    }

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
    signal(SIGPIPE,SIG_IGN);

    int nResult = 0;
    broadcast_setvolume_handle();
    while (1)
    {
        nResult = g_BroadcastEventSem.wait();
        broadcast_trans_handle();
        usleep(500 * 1000);
    }

    return 0;
}

void GetCurTime(char *pTime)
{
	time_t t 		= time(0);
	struct tm *ld	= NULL;
	char tmp[32] 	= {0};
	ld	= localtime(&t);
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
"<LED_CODE>1</LED_CODE>"
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
           broadcast_msg_play(xml_buf, xml_buf_len);     
        }

        sleep(3);
    }

    return 0;
}
