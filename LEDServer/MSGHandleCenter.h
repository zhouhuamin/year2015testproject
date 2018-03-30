// MSGHandleCenter.h: interface for the CMSG_Handle_Center class.

#if !defined _MSG_HANDLE_CENTER_

#define _MSG_HANDLE_CENTER_







#include "ace/Reactor.h"

#include <ace/Semaphore.h>

#include "ace/Guard_T.h"

#include "ace/Task.h"

#include "BaseDevice.h"

#include "SysMessage.h"




#include <syslog.h>
#include <vector>

#include <list>

#include <map>

using namespace std;



#define MAX_BUFFERD_MESSAGE_NUMBER                (50)



class CMSG_Handle_Center : public ACE_Task<ACE_SYNCH>

{

    // Instantiated with an MT synchronization trait.

public:



    enum

    {

        MAX_THREADS = 1

    };



    ~CMSG_Handle_Center(void);



    virtual int open(void * = 0);



    virtual int put(NET_PACKET_MSG* pMsg)

    {

        ACE_GUARD_RETURN(ACE_Thread_Mutex, guard, msg_handle_lock_, -1);

        msgToHandleList_.push_back(pMsg);

        if (msgToHandleList_.size() > 5)

        {

            syslog(LOG_DEBUG, "size of msgToHandleList_ is %d\n", msgToHandleList_.size());

        }



        msg_semaphore_.release();

        return 0;

    }



  

    virtual int handle_timeout(const ACE_Time_Value &tv, const void *arg);



    static void ThreadHandleMessage(void* lParam);





public:

    int InitDeviceSo();

 

    int RecordCascadeConnection(int nSocket);

    int ConnectToCascadeServer();

    int HandleNetMessage(NET_PACKET_MSG *pMsg);



    int RegisterToDeviceServer();

    int handleRegisterAck(NET_PACKET_MSG *pMsg);

    int handleICReaderData(NET_PACKET_MSG *pMsg);

   



    static int init_message_buffer();

    static int get_message(NET_PACKET_MSG*& pMsg);



    

    std::list<NET_PACKET_MSG*> msgToHandleList_;

    ACE_Semaphore msg_semaphore_;

    ACE_Thread_Mutex msg_handle_lock_;



    std::list<NET_PACKET_MSG*> msgToSendList_;

    ACE_Semaphore msg_send_semaphore_;

    ACE_Thread_Mutex msg_send_handle_lock_;



private:

    int m_nCascadeSocket;



    int m_nTimeCount;

    static bool m_bWork;

    static ACE_Thread_Mutex msg_lock_;

    static std::vector<NET_PACKET_MSG*> msgVec_;

    static int m_nMsgBufferCount;

    static char msg_buffer_[sizeof (NET_PACKET_MSG) * MAX_BUFFERD_MESSAGE_NUMBER];



    char* m_pSendBuffer;



    CBaseDevice*   m_pDevice;



};





//将此类定义为Singleton，保证只有一个类的实例在运行，同时也方便其他对象的访问

typedef ACE_Singleton<CMSG_Handle_Center, ACE_Null_Mutex> MSG_HANDLE_CENTER;





#endif // !defined(AFX_MSG_TASK_H__69346340_854F_4162_801C_94B418746B85__INCLUDED_)

