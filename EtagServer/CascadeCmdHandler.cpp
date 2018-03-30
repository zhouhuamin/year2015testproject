// CascadeCmdHandler.cpp: implementation of the CCascadeCmdHandler class.

//

//////////////////////////////////////////////////////////////////////

#ifdef WIN32

#include "stdafx.h"

#include "Winsock2.h"

#else

#include <netinet/tcp.h>

#endif


#include "includes.h"
#include "CascadeCmdHandler.h"

#include "SysUtil.h"

#include "SysMessage.h"
#include "packet_format.h"
#include "MSGHandleCenter.h"

//#include "SimpleConfig.h"
#include "includes.h"
#include "config.h"
#include "RFID_label.h"
#include "jzLocker.h"



#include <string>

using namespace std;



bool CCascadeCmdHandler::m_bConnected = false;

int CCascadeCmdHandler::socket_ = -1;
bool CCascadeCmdHandler::bGoWork_ = false;
bool CCascadeCmdHandler::bThread_flag_ = false;



extern int clean_label_list_cnt;
extern label_list_t label_list;
extern int g_nArrivedFlag;	// 0Ϊ������״̬, 1Ϊ����״̬
extern locker g_nArrivedFlagLock;



extern "C" {

    typedef void*(*THREADFUNC)(void*);

}



void* HandlerProc(void * pParam)

{

    if (!pParam)

    {

        return 0;

    }

    CCascadeCmdHandler* pHandler = (CCascadeCmdHandler*) pParam;

    if (!pHandler)

    {

        return 0;

    }   



    pHandler->svc();



    SysUtil::SysSleep(500);



    pthread_exit(NULL);



    return 0;

}





CCascadeCmdHandler::CCascadeCmdHandler(void)

{

    dwRecvBuffLen = 0;

    bGoWork_ = true;

    socket_ = -1;

    bThread_flag_ = false;

}



CCascadeCmdHandler::~CCascadeCmdHandler(void)

{

	if (CCascadeCmdHandler::bGoWork_)

    {

		CCascadeCmdHandler::bGoWork_ = false;

        SysUtil::SysSleep(1000);

    }



    m_bConnected = false;

    if (socket_ > 0)

    {

        SysUtil::CloseSocket(socket_);

    }

}



int CCascadeCmdHandler::open(void*)

{

    Register();

    

	if (!CCascadeCmdHandler::bThread_flag_)

    {

        pthread_t localThreadId;

        int nThreadErr = pthread_create(&localThreadId, NULL,

                (THREADFUNC) HandlerProc, this);

        if (nThreadErr == 0)

        {

            pthread_detach(localThreadId); //	�ͷ��߳�˽������,���صȴ�pthread_join();



        }



    }



    MSG_HANDLE_CENTER::instance()->RecordCascadeConnection(socket_);

    return 0;

}



int CCascadeCmdHandler::svc()

{

    unsigned long dwRecvBuffLen = 0;

    char* chRecvBuffer = new char[MAX_MSG_BODYLEN];

    int nRecvLen = 0;



    int nTimeCount = 0;

    char* chDest = new char[MAX_MSG_BODYLEN];

    memset(chDest, 0, MAX_MSG_BODYLEN);



	while (CCascadeCmdHandler::bGoWork_)

    {

        if (socket_ > 0)

        {



            if(nRecvLen> 5*1024)

            {

                nRecvLen=0;

            }



            /*������������*/

            if (SysUtil::ReceiveTimer(socket_) > 0)

            {

                int nRet = SysUtil::SocketRead(socket_, chRecvBuffer + nRecvLen, MAX_MSG_BODYLEN * 50 - nRecvLen);

                nRecvLen += nRet;



                if (nRet > 0)

                {

                    nTimeCount = 0; //�Ѿ����ܵ����ݣ�����Ҫ�ٷ���·���ֱ���



                    int nOffset = 0;

                    int nLen = 0;

                    while (VerifyRecvPacket(chRecvBuffer, chDest, nRecvLen, nOffset, nLen) == 0)

                    {

                        NET_PACKET_HEAD* pMsgHead = (NET_PACKET_HEAD*) (chDest + nOffset);

                        int nPacketLen = pMsgHead->packet_len;



                        if (nLen == nPacketLen + sizeof (NET_PACKET_HEAD))

                        {

                            if (pMsgHead->msg_type == SYS_MSG_SYSTEM_KEEPLIVE_ACK)

                            {

                                SysUtil::SysSleep(50);

                                continue;



                            }



                            

                            

                            NET_PACKET_MSG* pMsg_ = NULL;

                            CMSG_Handle_Center::get_message(pMsg_);

                            if (!pMsg_)

                            {

                                return -1;

                            }



                            memset(pMsg_, 0, sizeof (NET_PACKET_MSG));

                            //������ʼ����

                            memcpy(pMsg_, chDest + nOffset, nPacketLen + sizeof (NET_PACKET_HEAD));

                            MSG_HANDLE_CENTER::instance()->put(pMsg_);

                        }

                    }

                }

                else

                {

                    SysUtil::CloseSocket(socket_);

                    socket_ = -1;

                    m_bConnected = false;
					clean_label_list_cnt = 0;



                    nRecvLen = 0;

                    SysUtil::SysSleep(50);

                }

            }

            else

            {

                nTimeCount++;

                if (nTimeCount > 300)

                {

                    nTimeCount = 0;

                    SendHeartbeatMsg();

                }

            }



        }

        else

        {

            SysUtil::SysSleep(500);

        }

    }



    SysUtil::SysSleep(50);



    delete [] chDest;



    return 0;

}



int CCascadeCmdHandler::ConnectToCascadeServer(short port, char* chIP)

{

    if (socket_ > 0)

    {

        SysUtil::CloseSocket(socket_);

        socket_ = -1;

    }


	g_nArrivedFlagLock.lock();
	g_nArrivedFlag	= 0;
	g_nArrivedFlagLock.unlock();
	label_list.msg_send_flag = 0;
	clean_label_list_cnt = 0;
    socket_ = SysUtil::CreateSocket();

    if (SysUtil::ConnectSocket(socket_, chIP, port) == 0)

    {

        int sockbufsize = 10 * 1024;

        setsockopt(socket_,

                SOL_SOCKET,

                SO_SNDBUF,

                (char *) & sockbufsize,

                sizeof ( sockbufsize));







        set_keep_live(3, 5);

        m_bConnected = true;

        open(NULL);
		
        return 0;

    }

    else

    {

        SysUtil::CloseSocket(socket_);

        socket_ = -1;

        m_bConnected = false;

        return -1;

    }

}



int CCascadeCmdHandler::ReceiveTimer(int fd)

{

    int iret = 0;

    fd_set rset;

    struct timeval tv;



    FD_ZERO(&rset);

    FD_SET(fd, &rset);

    tv.tv_sec = 0;

    tv.tv_usec = 100 * 1000;



    iret = select(fd + 1, &rset, NULL, NULL, &tv);



    return iret;

}



int CCascadeCmdHandler::SendHeartbeatMsg()

{

    char chReq[1024] = {0};

    memset(chReq, 1024, 0);



    int sendLen = 0;

    memcpy(chReq, SYS_NET_MSGHEAD, 8); //��ͷ����

    sendLen += 8;



    NET_PACKET_HEAD* pHead = (NET_PACKET_HEAD*) (chReq + sendLen);

    pHead->msg_type = SYS_MSG_SYSTEM_KEEPLIVE_REQ;

    pHead->packet_len = 0;



    sendLen += sizeof (NET_PACKET_HEAD);



    memcpy(chReq + sendLen, SYS_NET_MSGTAIL, 8); //��ͷ����

    sendLen += 8;

    int nRet = SysUtil::SocketWrite(socket_, chReq, sendLen, 100);



    return 0;

}



int CCascadeCmdHandler::set_keep_live(int keep_alive_times, int keep_alive_interval)

{                                        //linux��

    int keepAlive = 1; //�趨KeepAlive

    int keepIdle = keep_alive_interval; //��ʼ�״�KeepAlive̽��ǰ��TCP�ձ�ʱ��

    int keepInterval = keep_alive_interval; //����KeepAlive̽����ʱ����

    int keepCount = keep_alive_times; //�ж��Ͽ�ǰ��KeepAlive̽�����?
    if (setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE, (const char*) & keepAlive, sizeof (keepAlive)) == -1)

    {

        ACE_DEBUG((LM_INFO,

                ACE_TEXT("(%P|%t) setsockopt SO_KEEPALIVE error!\n")));

    }

    if (setsockopt(socket_, SOL_TCP, TCP_KEEPIDLE, (const char *) & keepIdle, sizeof (keepIdle)) == -1)

    {

        ACE_DEBUG((LM_INFO,

                ACE_TEXT("(%P|%t) setsockopt TCP_KEEPIDLE error!\n")));

    }

    if (setsockopt(socket_, SOL_TCP, TCP_KEEPINTVL, (const char *) & keepInterval, sizeof (keepInterval)) == -1)

    {

        ACE_DEBUG((LM_INFO,

                ACE_TEXT("(%P|%t) setsockopt TCP_KEEPINTVL error!\n")));

    }

    if (setsockopt(socket_, SOL_TCP, TCP_KEEPCNT, (const char *) & keepCount, sizeof (keepCount)) == -1)

    {

        ACE_DEBUG((LM_INFO,

                ACE_TEXT("(%P|%t)setsockopt TCP_KEEPCNT error!\n")));

    }

    return 0;

}



int CCascadeCmdHandler::VerifyRecvPacket(char *chRecvBuffer, char* chDest, int &nRecLen, int &nOffset, int &nLen)

{

    /*����������С����С֡����*/

    if (nRecLen < sizeof (NET_PACKET_HEAD))

    {

        return -1;

    }



    int nHeadPos = SysUtil::SearchHeadPos(chRecvBuffer, nRecLen);

    if (nHeadPos < 0)

    {

        return -1;

    }



    int nTailPos = SysUtil::SearchTailPos(chRecvBuffer, nRecLen);

    if (nTailPos < 0)

    {

        return -1;

    }



    memcpy(chDest, chRecvBuffer + nHeadPos, (nTailPos - nHeadPos) + 8);



    if (nRecLen - nTailPos - 8 > 0)

    {

        //���յ����ݿ����������İ�

        memcpy(chRecvBuffer, chRecvBuffer + nTailPos + 8, nRecLen - nTailPos - 8);

    }



    nOffset = nHeadPos = 8;

    nLen = nTailPos - nHeadPos;

    nRecLen = nRecLen - nTailPos - 8;



    return 0;

}



void CCascadeCmdHandler::Register()

{



	

	char chReq[1024]={0};

	memset(chReq,1024,0);

	

	int sendLen=0;

	memcpy(chReq,SYS_NET_MSGHEAD,8); //��ͷ����

	sendLen+=8;

	

	NET_PACKET_MSG* pMsg=(NET_PACKET_MSG*)(chReq+sendLen);

	

	NET_PACKET_HEAD* pHead=(NET_PACKET_HEAD*)(chReq+sendLen);

	pHead->msg_type=1;

	pHead->packet_len=sizeof(NVP_REGISTER);

	

	sendLen+=sizeof(NET_PACKET_HEAD);

	

	

	

	NVP_REGISTER* pTrigger=(NVP_REGISTER*)pMsg->msg_body;

	

	

	sprintf(pTrigger->dev_tag, gSetting_system.register_DEV_TAG);

	sprintf(pTrigger->dev_id, gSetting_system.register_DEV_ID);

	

	

	sendLen+=sizeof(NVP_REGISTER);

	

	memcpy(chReq+sendLen,SYS_NET_MSGTAIL,8); //��ͷ����

	sendLen+=8;	





	int nRet=SysUtil::SocketWrite(socket_,chReq,sendLen,100);

	

}







