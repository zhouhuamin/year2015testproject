#include "includes.h"
#include "Barcode.h"
#include "dev.h"
#include "packet_format.h"
#include "starup_handle.h"
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
#include "SysUtil.h"

bool g_timer_flag = false;
int g_nArrivedFlag = 0;
string g_strArriveData;
string g_strBarcode;
locker g_nArrivedFlagLock;
locker g_barcodeLock;

bool bStartFlag = false;
extern std::list<structLcdMsg>  g_lcdMsgList;
extern struct struDeviceAndServiceStatus g_ObjectStatus;
extern pthread_mutex_t g_StatusMutex;
extern sem	g_StartEventSem;
extern sem	g_StoppedEventSem;
extern int gFd_dev;
static net_packet_head_t packet_head;
static u8 data_buf[0x800];
static int msg_LED;
static char gb_buf[200];

int set_keep_live(int nSocket, int keep_alive_times, int keep_alive_interval)
{

#ifdef WIN32                                //WIndows下
	TCP_KEEPALIVE inKeepAlive = {0}; //输入参数
	unsigned long ulInLen = sizeof (TCP_KEEPALIVE);

	TCP_KEEPALIVE outKeepAlive = {0}; //输出参数
	unsigned long ulOutLen = sizeof (TCP_KEEPALIVE);

	unsigned long ulBytesReturn = 0;

	//设置socket的keep alive为5秒，并且发送次数为3次
	inKeepAlive.on_off = keep_alive_times;
	inKeepAlive.keep_alive_interval = keep_alive_interval * 1000; //两次KeepAlive探测间的时间间隔
	inKeepAlive.keep_alive_time = keep_alive_interval * 1000; //开始首次KeepAlive探测前的TCP空闭时间


	outKeepAlive.on_off = keep_alive_times;
	outKeepAlive.keep_alive_interval = keep_alive_interval * 1000; //两次KeepAlive探测间的时间间隔
	outKeepAlive.keep_alive_time = keep_alive_interval * 1000; //开始首次KeepAlive探测前的TCP空闭时间


	if (WSAIoctl((unsigned int) nSocket, SIO_KEEPALIVE_VALS,
		(LPVOID) & inKeepAlive, ulInLen,
		(LPVOID) & outKeepAlive, ulOutLen,
		&ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
	{
		//ACE_DEBUG((LM_INFO,
		//	ACE_TEXT("(%P|%t) WSAIoctl failed. error code(%d)!\n"), WSAGetLastError()));
	}

#else                                        //linux下
	int keepAlive = 1; //设定KeepAlive
	int keepIdle = keep_alive_interval; //开始首次KeepAlive探测前的TCP空闭时间
	int keepInterval = keep_alive_interval; //两次KeepAlive探测间的时间间隔
	int keepCount = keep_alive_times; //判定断开前的KeepAlive探测次数
	if (setsockopt(nSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*) & keepAlive, sizeof (keepAlive)) == -1)
	{
		syslog(LOG_DEBUG, "setsockopt SO_KEEPALIVE error!\n");
	}
	if (setsockopt(nSocket, SOL_TCP, TCP_KEEPIDLE, (const char *) & keepIdle, sizeof (keepIdle)) == -1)
	{
		syslog(LOG_DEBUG, "setsockopt TCP_KEEPIDLE error!\n");
	}
	if (setsockopt(nSocket, SOL_TCP, TCP_KEEPINTVL, (const char *) & keepInterval, sizeof (keepInterval)) == -1)
	{
		syslog(LOG_DEBUG, "setsockopt TCP_KEEPINTVL error!\n");
	}
	if (setsockopt(nSocket, SOL_TCP, TCP_KEEPCNT, (const char *) & keepCount, sizeof (keepCount)) == -1)
	{
		syslog(LOG_DEBUG, "setsockopt TCP_KEEPCNT error!\n");
	}

#endif

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
    strcpy(pReadData->device_tag, gSetting_system.gather_data_tag);

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
    //浣跨??tcp??璁?杩?琛???淇★???瑕?杩??ョ???????哄??IP?板??锟??92.168.1.2
    snprintf(szPublisherIp, sizeof(szPublisherIp), "tcp://%s:%d", gSetting_system.publisher_server_ip, gSetting_system.publisher_server_port);
    szPublisherIp[63] = '\0';
    const char * pAddr = szPublisherIp; // "tcp://192.168.1.101:7766";

    //??寤?context
   if((pCtx = zmq_ctx_new()) == NULL)
    {
        return 0;
    }
    //??寤?socket
    if((pSock = zmq_socket(pCtx, ZMQ_DEALER)) == NULL)
    {
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    int iSndTimeout = 5000;// millsecond
    //璁剧疆?ユ?惰???
    if(zmq_setsockopt(pSock, ZMQ_RCVTIMEO, &iSndTimeout, sizeof(iSndTimeout)) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    //杩??ョ????IP192.168.1.2锛?绔?锟??766
    if(zmq_connect(pSock, pAddr) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    //寰???????娑?锟??   
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
            syslog(LOG_ERR, "send message faild\n");
            continue;
        }
        syslog(LOG_DEBUG, "send message : [%s] succeed\n", szMsg);
        // getchar();
        usleep (30000 * 1000);
    }
	return 0;
}

void *pthread_dev_handle(void *arg)
{
    bool bStop = false;
    int ret = 0;
    bool flag = 0;
    int nSocket = -1;
    int total_len = 0;
    char szbuffer[MAX_BUF_SIZE] = {0};
    char szBarcode[MAX_BUF_SIZE] = {0};
    signal(SIGPIPE,SIG_IGN);
    while (1)
    {               
        try
        {
            bStop = false;
            memset(szbuffer,0,sizeof(szbuffer)); 
            if(total_len >= MAX_BUF_SIZE-1)
            {
                total_len = 0;
            }

            if(nSocket < 0)
            {
                nSocket = SysUtil::CreateSocket();
            }     
            
//            struct timeval timeout = {3, 0};
//            socklen_t len = sizeof(timeout);
//            setsockopt(nSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);
            set_keep_live(nSocket,3, 10);  
            syslog(LOG_DEBUG, "*********************************connect start.\n");
            if (SysUtil::ConnectSocket(nSocket, gSetting_system.com_ip, gSetting_system.com_port) != 0)
            {
                SysUtil::CloseSocket(nSocket);
                nSocket = -1;
                syslog(LOG_DEBUG, "connect to com server %s:%d fail...\n", gSetting_system.com_ip, gSetting_system.com_port);
                sleep(3);
                continue;
            }

            while(!bStop)
            {
                ret=SysUtil::SocketRead(nSocket, szbuffer, MAX_BUF_SIZE);	
                //ret = read(gFd_dev, szbuffer, MAX_BUF_SIZE);     
                syslog(LOG_DEBUG, "*********************************read end ret=%d\n", ret);
                if( ret > 0)
                {
                    for(int i=0; i<ret; i++)
                    {
                        if (szbuffer[i] == 0x0D)
                        {           
                            flag = true;
                            szBarcode[total_len] = '\0';
                            break;                 
                        }
                        else
                        {
                            szBarcode[total_len] = szbuffer[i];
                            total_len++;                    
                        }
                    }

                    if(flag)
                    {
                        flag = false;
                        total_len = 0; 
                        g_strArriveData = szBarcode;
                        g_barcodeLock.lock();
                        g_strBarcode = szBarcode;
                        g_barcodeLock.unlock();                    

                        syslog(LOG_DEBUG, "=======================%s\n", szBarcode); 
                    }
                }
                else
                {
                    total_len = 0;
                    //close(gFd_dev);
                    SysUtil::CloseSocket(nSocket);
                    nSocket = -1;
                    bStop = true; 
                }  
                
                usleep(10*1000);
            }
        }
        catch(...)
        {
            //dev_retry_connect_handle(&gFd_dev);
            SysUtil::CloseSocket(nSocket);
            nSocket = -1;
        }  
    }

    return 0;
}

void *ArrivedEventThread(void *arg)
{
    char szArriveData[MAX_BUF_SIZE] = {0};
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
            barcode_upload_arrived_data(szArriveData);         
        }

        usleep(1000);
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
        bStartFlag = true;
        g_barcodeLock.lock();
        g_strBarcode = "";
        g_barcodeLock.unlock();  

        int nFlag = 0;
        while(1)
        {
            if(!bStartFlag)
            {
                break;
            }
            
            g_barcodeLock.lock();
            if (g_strBarcode.length() > 0)
            {
                nFlag = 1;
                g_barcodeLock.unlock();
                break;
            }
            g_barcodeLock.unlock();

            usleep(100*1000);
        }

        if (nFlag == 1)
        {         
            char szBuffer[256] = {0};
            strcpy(szBuffer,g_strBarcode.c_str());
            barcode_upload_gather_data(szBuffer, gSetting_system.gather_data_event_id);
            g_barcodeLock.lock();
            g_strBarcode = "";
            g_barcodeLock.unlock();

            syslog(LOG_DEBUG, "******************upload finshed data.\n");
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
        g_nArrivedFlagLock.lock();
        g_nArrivedFlag = 0;
        g_strArriveData = "";
        g_nArrivedFlagLock.unlock();
        bStartFlag = false;
        g_barcodeLock.lock();
        g_strBarcode = "";
        g_barcodeLock.unlock();

        usleep(1000);
    }
    return 0;
}

void *TimerEventThread(void *arg)
{
    return 0;
}

int barcode_upload_gather_data(const char *barcode, char *pEventID)
{
//    NET_gather_data *p_gather_data;
//    xmlDocPtr doc = NULL;
//    xmlNodePtr root_node = NULL;
//    int w_len;
//    xmlChar * xml_buf;
//    char *p_send;
//    int xml_len;
//    int ret;
//    char szTmp[MAX_BUF_SIZE] = {0};
//
//    packet_head.msg_type = MSG_TYPE_GATHER_DATA;
//    strcpy(packet_head.szVersion, "1.0");
//
//    p_gather_data = (NET_gather_data *) (data_buf + sizeof(net_packet_head_t)
//                    + 8);
//
//    strcpy(p_gather_data->event_id, pEventID);
//    ret = 0;
//    memcpy(&(p_gather_data->is_data_full), &ret, sizeof(int));
//    strcpy(p_gather_data->dev_tag, gSetting_system.gather_data_tag);
//
//    /**********************************XML*********************************/
//    doc = xmlNewDoc(NULL);
//    root_node = xmlNewNode(NULL, BAD_CAST gSetting_system.gather_data_tag);
//    xmlDocSetRootElement(doc, root_node);
//    xmlNewChild(root_node, NULL, BAD_CAST "CODE", BAD_CAST (barcode));
//    xmlDocDumpFormatMemoryEnc(doc, &xml_buf, &xml_len, "GB2312", 1);
//    sprintf(szTmp,"<%s>",gSetting_system.gather_data_tag);
//    p_send = strstr((char*) xml_buf, szTmp);
//
//    if (p_send == NULL)
//    {
//            return -1;
//    }
//
//    xml_len -= (p_send - (char*) xml_buf);
//
//    p_gather_data->xml_data_len = xml_len + 1;
//    memcpy(p_gather_data->xml_data, p_send, xml_len);
//    p_gather_data->xml_data[xml_len] = 0;
//
//    xmlFree(xml_buf);
//    xmlFreeDoc(doc);
//    xmlCleanupParser();
//    xmlMemoryDump();
//    /**********************************XML END*********************************/
//
//    w_len = ((u8*) p_gather_data->xml_data - data_buf) + xml_len + 1;
//    packet_head.packet_len = w_len - sizeof(net_packet_head_t) - 8;
//    strcpy((char *) data_buf, "0XJZTECH");
//
//    memcpy(data_buf + 8, &packet_head, sizeof(net_packet_head_t));
//    strcpy((char*) (data_buf + w_len), "NJTECHJZ");
//    w_len += 8;

    char szBuffer[MAX_BUF_SIZE]= {0};
    sprintf(szBuffer, "<BARCODE>\
  <CODE>%s</CODE>\
</BARCODE>",barcode);
    
    syslog(LOG_DEBUG, "upload gather data \n%s\n",szBuffer);
    ReceiveReaderData(szBuffer, strlen(szBuffer), pEventID);
    return 0;
}

int barcode_upload_arrived_data(const char *barcode)
{  
    ReceiveReaderData(0, 0, gSetting_system.gather_arrived_event_id);      
    return 0;
}