/* 
 * File:   main.cpp
 * Author: root
 *
 * Created on 2015å¹?9??20??, ä¸???11:02
 */

#include <cstdlib>
#include <czmq.h>
#include "stdio.h"
#include "zhelpers.h"
#include "jzLocker.h"
#include "SysUtil.h"

#include <list>
#include <string>
#include <boost/shared_array.hpp>
#include <syslog.h>

using namespace std;
using namespace boost;



list<string> messageVect;
locker m_queuelocker;
sem m_queuestat;

typedef unsigned char BYTE;

static void *worker_task (void *args)
{
    void * pCtx = NULL;
    void * pSock = NULL;
    const char * pAddr = "tcp://*:7766";

    //??å»?contextï¼?zmq??socket ??è¦???contextä¸?è¿?è¡???å»? 
    if((pCtx = zmq_ctx_new()) == NULL)
    {
        return 0;
    }
    //??å»?zmq socket ï¼?socket??????6ä¸?å±??? ï¼?è¿???ä½¿ç??dealer?¹å?
    //?·ä?ä½¿ç?¨æ?¹å?è¯·å????zmqå®??¹æ??æ¡£ï?zmq????ï¼? 
    if((pSock = zmq_socket(pCtx, ZMQ_DEALER)) == NULL)
    {
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    int iRcvTimeout = 5000;// millsecond
    //è®¾ç½®zmq???¥æ?¶è??¶æ?¶é?´ä¸º5ç§? 
    if(zmq_setsockopt(pSock, ZMQ_RCVTIMEO, &iRcvTimeout, sizeof(iRcvTimeout)) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    //ç»?å®??°å?? tcp://*:7766 
    //ä¹?å°±æ??ä½¿ç??tcp??è®?è¿?è¡???ä¿¡ï?ä½¿ç?¨ç?ç»?ç«??? 7766
    if(zmq_bind(pSock, pAddr) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    syslog(LOG_DEBUG, "bind at : %s\n", pAddr);
    while(1)
    {
        char szMsg[2048] = {0};
        syslog(LOG_DEBUG, "waitting...\n");
        errno = 0;
        //å¾???ç­?å¾??¥æ?¶å?°æ?¥ç??æ¶???ï¼?å½?è¶?è¿?5ç§?æ²¡æ???¥å?°æ????¶ï?
        //zmq_recv?½æ?°è?????è¯?ä¿¡æ?? ï¼?å¹¶ä½¿??zmq_strerror?½æ?°è?è¡???è¯?å®?ä½? 
        if(zmq_recv(pSock, szMsg, sizeof(szMsg), 0) < 0)
        {
            syslog(LOG_DEBUG, "error = %s\n", zmq_strerror(errno));
            continue;
        }
        syslog(LOG_DEBUG, "received message : %s\n", szMsg);
	m_queuelocker.lock();	
	messageVect.push_back(szMsg);
	m_queuelocker.unlock();
	m_queuestat.post();
    }
    return NULL;
}

static void *publisher_task (void *args)
{
	    //  Prepare our context and publisher
    void *context = zmq_ctx_new ();
    void *publisher = zmq_socket (context, ZMQ_PUB);
    int rc = zmq_bind (publisher, "tcp://*:7777");
    assert (rc == 0);
    
    const char packetHead[4]={(char)0xB2, (char)0xC3, (char)0xD4, (char)0xE5};
    const char packetEnd[2]={(char)0xFF, (char)0xFF};

    while (1) 
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if ( messageVect.empty() )
        {
            m_queuelocker.unlock();
            continue;
        }
	string strMessage = messageVect.front();
	messageVect.pop_front();
	m_queuelocker.unlock();

	if (strMessage != "")
	{
//            printf ("%s", strMessage.c_str());
//        	//  Send message to all subscribers
//        	char update [1024] = {0};
//        	// sprintf (update, "NJJZTECH %s", strMessage.c_str());
//		sprintf (update, "%s", strMessage.c_str());
//        	s_send (publisher, update);
                
                // 2015-9-19
                string strGatherXml = strMessage.c_str();
                int nXmlLen = strGatherXml.size(); // strSendXml.size();
                int PackDataLen = 4 + 4 + 4 + nXmlLen + 2;

                boost::shared_array<BYTE> spPacketData;
                spPacketData.reset(new BYTE[PackDataLen + 1]);
                bzero(spPacketData.get(), PackDataLen + 1);

                BYTE *p = spPacketData.get();
                memcpy(p, packetHead ,4);
                p += 4;

                memcpy(p, (BYTE*)&PackDataLen, sizeof(int));
                p += 4;

                memcpy(p, &nXmlLen, 4);
                p+=4;

                memcpy(p, strGatherXml.c_str(), nXmlLen);
                p += nXmlLen;
                memcpy(p, packetEnd ,2);
                // end                
                
                //???§ä?????å¹³å?°ï?å¯¹æ?°æ?????°æ????
                int send_socket = SysUtil::CreateSocket();
                if (SysUtil::ConnectSocket(send_socket, "10.61.4.190", 9005) != 0)
                {
                    SysUtil::CloseSocket(send_socket);
                    syslog(LOG_DEBUG, "connect platform failed\n");
                    continue;
                }

                int nRet = SysUtil::SocketWrite(send_socket, (char*)spPacketData.get(), PackDataLen, 30);

                if (nRet == PackDataLen)
                {
                    syslog(LOG_DEBUG, "send message success\n");
                }
                else
                {
                    syslog(LOG_DEBUG, "send message failed\n");
                }

                SysUtil::CloseSocket(send_socket);                
                
                
	}
	zclock_sleep (1000);
    }
    zmq_close (publisher);
    zmq_ctx_destroy (context);
	return NULL;
}

/*
 * 
 */
int main(int argc, char** argv)
{
    openlog("DeviceStatusService", LOG_PID, LOG_LOCAL7);
    zthread_new (worker_task, NULL);
    zthread_new (publisher_task, NULL);

    while (1)
            zclock_sleep (1000);
    return 0;
}

