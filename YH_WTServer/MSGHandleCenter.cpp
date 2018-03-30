// MSGHandleCenter.cpp: implementation of the CMSG_Handle_Center class.

//

//////////////////////////////////////////////////////////////////////

#include "MSGHandleCenter.h"



//#include "SimpleConfig.h"
#include "includes.h"
#include "packet_format.h"
#include "includes.h"
#include "config.h"
//#include "MyLog.h"

#include "ace/SOCK_Connector.h"

#include "ace/Signal.h"

#include "SysMessage.h"

#include <vector>

#include "CascadeCmdHandler.h"

#include "SysUtil.h"
#include "RFID_label.h"
#include "dev.h"


#include <dlfcn.h>
#include <syslog.h>





#define MAX_RECORD_NUMBER    8



ACE_Thread_Mutex CMSG_Handle_Center::msg_lock_;



std::vector<NET_PACKET_MSG*> CMSG_Handle_Center::msgVec_;

int CMSG_Handle_Center::m_nMsgBufferCount = 0;

char CMSG_Handle_Center::msg_buffer_[sizeof (NET_PACKET_MSG) * MAX_BUFFERD_MESSAGE_NUMBER] = {0};



extern "C" {

    typedef void*(*THREADFUNC)(void*);

}

extern int clean_label_list_cnt;
extern label_list_t label_list;
extern int net_system_CTRL_handle(int server_fd, u8 *buf, int len);
extern struct struDeviceAndServiceStatus g_ObjectStatus;
extern pthread_mutex_t g_StatusMutex;

void ReceiveReaderErrorState(int error_state, void* user_data) {

}

CMSG_Handle_Center::~CMSG_Handle_Center() {

    ACE_Reactor::instance()->cancel_timer(this);

}

void CMSG_Handle_Center::ThreadHandleMessage(void* lParam) {

    CMSG_Handle_Center* pTask = (CMSG_Handle_Center*) lParam;

    while (1) {

        pTask->msg_semaphore_.acquire();



        NET_PACKET_MSG* pMsg_ = NULL;

        {

            ACE_GUARD(ACE_Thread_Mutex, guard, pTask->msg_handle_lock_);



            if (pTask->msgToHandleList_.size() == 0) {



                struct timeval tv;

                tv.tv_sec = 0;

                tv.tv_usec = 100 * 1000;

                select(0, 0, NULL, NULL, &tv);





                continue;

            }



            pMsg_ = pTask->msgToHandleList_.front();

            pTask->msgToHandleList_.pop_front();



        }



        if (!pMsg_) {

            continue;

        }



        try 
		{

            pTask->HandleNetMessage(pMsg_);

            syslog(LOG_DEBUG, "end handle msg ......\n");

        }        
		catch (...) 
		{

            syslog(LOG_DEBUG, "********ThreadHandleMessage exception!msg type %d\n", pMsg_->msg_head.msg_type);

            exit(0);

        }

    }

}



int CMSG_Handle_Center::handle_timeout(const ACE_Time_Value &tv, const void *arg) {



    ACE_UNUSED_ARG(tv);

    ACE_UNUSED_ARG(arg);



 



    NET_PACKET_MSG* pMsg_ = NULL;

    get_message(pMsg_);

    if (!pMsg_) {

        return -1;

    }



    memset(pMsg_, 0, sizeof (NET_PACKET_MSG));



    //  put(pMsg_);



    return 0;

}



static ACE_THR_FUNC_RETURN ThreadConnectToCascadeServerFunc(void *lParam) {

    CMSG_Handle_Center* pTask = (CMSG_Handle_Center*) lParam;

    pTask->ConnectToCascadeServer();

    return 0;

}



int CMSG_Handle_Center::open(void*) {

    m_nCascadeSocket = -1;



    m_nTimeCount = 0;



    m_pSendBuffer = new char[20 * 1024];



    init_message_buffer();

    msg_semaphore_.acquire();

    msg_send_semaphore_.acquire();



    m_pDevice = NULL;



    InitDeviceSo();



    //���������̣߳��������׶δ�����յ�ͼƬ���?
    ACE_Thread_Manager::instance()->spawn(ThreadConnectToCascadeServerFunc, this);



    SysUtil::SysSleep(1000);





    int nThreadErr = 0;

    pthread_t rcsThreadId;

    nThreadErr = pthread_create(&rcsThreadId, NULL,

            (THREADFUNC) ThreadHandleMessage, this);





    



    if (ACE_Reactor::instance()->schedule_timer(this,

            0,

            ACE_Time_Value(2, 0),

            ACE_Time_Value(2, 0)) == -1) {

        return -1;

    }



    return 0;

}



int CMSG_Handle_Center::get_message(NET_PACKET_MSG *&pMsg) {

    ACE_GUARD_RETURN(ACE_Thread_Mutex, guard, msg_lock_, -1);



    pMsg = msgVec_[m_nMsgBufferCount];



    m_nMsgBufferCount++;

    if (m_nMsgBufferCount > MAX_BUFFERD_MESSAGE_NUMBER - 1) {

        m_nMsgBufferCount = 0;

    }



    return 0;



}



int CMSG_Handle_Center::init_message_buffer() {

    for (int i = 0; i < MAX_BUFFERD_MESSAGE_NUMBER; i++) {

        char* chMsgBuffer = msg_buffer_;

        NET_PACKET_MSG* pMsg = (NET_PACKET_MSG*) (chMsgBuffer + i * sizeof (NET_PACKET_MSG));

        msgVec_.push_back(pMsg);

    }



    return 0;

}



int CMSG_Handle_Center::HandleNetMessage(NET_PACKET_MSG *pMsg) {

    char chMsg[512] = {0};



    if (!pMsg) {

        return -1;

    }



    syslog(LOG_DEBUG, "begin handle msg type %d \n", pMsg->msg_head.msg_type);



    switch (pMsg->msg_head.msg_type) {

        case SYS_MSG_SYSTEM_REGISTER_ACK:

            handleRegisterAck(pMsg);

            break;

        case SYS_MSG_PUBLISH_EVENT:

            handleICReaderData(pMsg);

            break;

		case MSG_TYPE_CTRL_EVENT:
			net_system_CTRL_handle(0, (u8 *)pMsg, sizeof(NET_PACKET_MSG));
			break;

        default:

            break;

    }



    return 0;



}



int CMSG_Handle_Center::ConnectToCascadeServer() {

    CCascadeCmdHandler * handler = new CCascadeCmdHandler;



    while (1) {

        if (!CCascadeCmdHandler::m_bConnected) 
		{

			if (handler->ConnectToCascadeServer((short)gSetting_system.server_port, gSetting_system.server_ip) == -1) 
			{
				pthread_mutex_lock(&g_StatusMutex);
				strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
				g_ObjectStatus.nLocalPort		= 0;
				strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
				g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
				strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
				g_ObjectStatus.nServiceType	= 0;
				strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
				strcpy(g_ObjectStatus.szObjectStatus, "011");
				strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");

				{
					char szNowTime[32] = {0};
					GetCurTime(szNowTime);
					szNowTime[31] = '\0';
					strcpy(g_ObjectStatus.szReportTime, szNowTime);
				}
				pthread_mutex_unlock(&g_StatusMutex);
            } 
			else 
			{

                CCascadeCmdHandler::m_bConnected = true;
				syslog(LOG_DEBUG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

				pthread_mutex_lock(&g_StatusMutex);
				strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
				g_ObjectStatus.nLocalPort		= 0;
				strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
				g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
				strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
				g_ObjectStatus.nServiceType	= 0;
				strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
				strcpy(g_ObjectStatus.szObjectStatus, "010");
				strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");

				{
					char szNowTime[32] = {0};
					GetCurTime(szNowTime);
					szNowTime[31] = '\0';
					strcpy(g_ObjectStatus.szReportTime, szNowTime);
				}
				pthread_mutex_unlock(&g_StatusMutex);
            }

        }



        //select����sleep,�ȴ�һ��ʱ����5s

        SysUtil::SysSleep(5 * 1000);

    }



    delete handler;



}



int CMSG_Handle_Center::RecordCascadeConnection(int nSocket) {

    m_nCascadeSocket = nSocket;

    RegisterToDeviceServer();



    return 0;

}

int CMSG_Handle_Center::InitDeviceSo() 
{
//    if (strlen(CSimpleConfig::DEVICE_SO) > 0) {
//
//        DeviceInfo device_info;
//
//        memset(&device_info, 0, sizeof (DeviceInfo));
//
//
//
//        device_info.com_port = CSimpleConfig::COM_PORT;
//
//        device_info.baud_date = CSimpleConfig::BAUD_RATE;
//
//
//
//        char device_so_path[256] = {0};
//
//        sprintf(device_so_path, "/usr/lib/%s", CSimpleConfig::DEVICE_SO);
//
//
//
//        void* device_so = dlopen(device_so_path, RTLD_LAZY);
//
//        if (!device_so) {
//
//
//
//            CMyLog::m_pLog->_XGSysLog("********load library fail! dll name is  %s ,error %s %d \n", device_so_path, dlerror(), errno);
//
//            return 1;
//
//        } else {
//
//            CMyLog::m_pLog->_XGSysLog("load library succ! dll name is  %s \n", device_so_path);
//
//        }
//
//
//
//        // reset errors
//
//        dlerror();
//
//
//
//        // load the symbols
//
//        create_t* create_device = (create_t*) dlsym(device_so, "create");
//
//        const char* dlsym_error = dlerror();
//
//        if (dlsym_error) {
//
//            CMyLog::m_pLog->_XGSysLog("Cannot load symbol create:  %s \n", dlsym_error);
//
//            return 1;
//
//        }
//
//
//
//        if (create_device) {
//
//            printf("get create function pointer %p succ !\n", create_device);
//
//        }
//
//
//
////        // create an instance of the class
//
////        m_pDevice = create_device();
//
////
//
////        if (m_pDevice != NULL) //�����豸�ɹ�
//
////        {
//
////
//
////            CMyLog::m_pLog->_XGSysLog("create device object succ\n");
//
////
//
////            int nRet = m_pDevice->init_device(device_info);
//
////
//
////            //m_pDevice->SetReadDataCallback((_READ_DATA_CALLBACK_) ReceiveReaderData, this);
//
////            m_pDevice->SetErrorStateCallback((_ERROR_STATE_CALLBACK_) ReceiveReaderErrorState, this);
//
////
//
////        } else {
//
////            CMyLog::m_pLog->_XGSysLog("********create device object fail\n");
//
////
//
////        }
//
//    }

    return 0;
}



int CMSG_Handle_Center::handleRegisterAck(NET_PACKET_MSG *pMsg) {

    if (!pMsg) {

        return -1;

    }



    T_DeviceServerRegister_Ack* pRegisterAck = (T_DeviceServerRegister_Ack*) pMsg->msg_body;

    if (pRegisterAck->reg_result == 0) {

        if (pRegisterAck->next_action == 1) {

           // m_pDevice->start_work();

        }

    }



    return 0;

}



int CMSG_Handle_Center::handleICReaderData(NET_PACKET_MSG *pMsg) {

    if (!pMsg) {

        return -1;

    }



    if (m_nCascadeSocket > 0) {

        T_SysEventData* pReadData = (T_SysEventData*) pMsg->msg_body;



        char chReq[1024 * 10] = {0};

        memset(chReq, 1024 * 10, 0);



        int sendLen = 0;

        memcpy(chReq, SYS_NET_MSGHEAD, 8); //��ͷ����

        sendLen += 8;



        NET_PACKET_HEAD* pHead = (NET_PACKET_HEAD*) (chReq + sendLen);

        pHead->msg_type = pMsg->msg_head.msg_type;

        pHead->packet_len = pMsg->msg_head.packet_len;

        sendLen += sizeof (NET_PACKET_HEAD);



        T_SysEventData* pReadDataToSend = (T_SysEventData*) (chReq + sendLen);

        memcpy(pReadDataToSend, pReadData, sizeof (T_SysEventData) + pReadData->xml_data_len);





        sendLen += sizeof (T_SysEventData) + pReadData->xml_data_len;



        memcpy(chReq + sendLen, SYS_NET_MSGTAIL, 8); //��ͷ����

        sendLen += 8;



        int nRet = SysUtil::SocketWrite(m_nCascadeSocket, chReq, sendLen, 100);

        if (nRet == sendLen) 
		{
            syslog(LOG_DEBUG, "send etagserver data to device server succ...\n");

			pthread_mutex_lock(&g_StatusMutex);
			strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
			g_ObjectStatus.nLocalPort		= 0;
			strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
			g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
			strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
			g_ObjectStatus.nServiceType	= 0;
			strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
			strcpy(g_ObjectStatus.szObjectStatus, "000");
			strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");

			{
				char szNowTime[32] = {0};
				GetCurTime(szNowTime);
				szNowTime[31] = '\0';
				strcpy(g_ObjectStatus.szReportTime, szNowTime);
			}
			pthread_mutex_unlock(&g_StatusMutex);

        } 
		else 
		{

            syslog(LOG_DEBUG, "********send etagserver data to device server fail,to send is %d ,send is %d...\n", sendLen, nRet);
			pthread_mutex_lock(&g_StatusMutex);
			strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
			g_ObjectStatus.nLocalPort		= 0;
			strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
			g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
			strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
			g_ObjectStatus.nServiceType	= 0;
			strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
			strcpy(g_ObjectStatus.szObjectStatus, "011");
			strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");

			{
				char szNowTime[32] = {0};
				GetCurTime(szNowTime);
				szNowTime[31] = '\0';
				strcpy(g_ObjectStatus.szReportTime, szNowTime);
			}
			pthread_mutex_unlock(&g_StatusMutex);

        }



    } else {

        syslog(LOG_DEBUG, "********device server connection is broken...\n");

    }



}



int CMSG_Handle_Center::RegisterToDeviceServer() {

    char chReq[1024] = {0};

    memset(chReq, 1024, 0);



    int sendLen = 0;

    memcpy(chReq, SYS_NET_MSGHEAD, 8); //��ͷ����

    sendLen += 8;



    NET_PACKET_HEAD* pHead = (NET_PACKET_HEAD*) (chReq + sendLen);

    pHead->msg_type = SYS_MSG_SYSTEM_REGISTER_REQ;

    pHead->packet_len = sizeof (T_DeviceServerRegister_Req);



    sendLen += sizeof (NET_PACKET_HEAD);





    T_DeviceServerRegister_Req* pRegisterReq = (T_DeviceServerRegister_Req*) (chReq + sendLen);

    strcpy(pRegisterReq->device_tag, gSetting_system.register_DEV_TAG);

    strcpy(pRegisterReq->device_id, gSetting_system.register_DEV_ID);



    sendLen += sizeof (T_DeviceServerRegister_Req);



    memcpy(chReq + sendLen, SYS_NET_MSGTAIL, 8); //��ͷ����

    sendLen += 8;

    int nRet = SysUtil::SocketWrite(m_nCascadeSocket, chReq, sendLen, 100);



    if (nRet == sendLen) {

		clean_label_list_cnt = 0;
		label_list.msg_send_flag = 0;
        syslog(LOG_DEBUG, "register to device server succ...\n");

    } else {

        syslog(LOG_DEBUG, "********egister to device server fail,to send is %d ,send is %d...\n", sendLen, nRet);

    }

}



