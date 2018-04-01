// CascadeCmdHandler.h: interface for the CCascadeCmdHandler class.

//

//////////////////////////////////////////////////////////////////////



#if !defined _CASCADE_CMD_HANDLER_H_

#define _CASCADE_CMD_HANDLER_H_



#include "SysMessage.h"

#include <vector>

#include <string>



using namespace std;



#define MAX_RECV_LENGTH (MAX_MSG_BODYLEN*2)



class CCascadeCmdHandler

{

public:

    int VerifyRecvPacket(char *chRecvBuffer, char* chDest, int &nRecLen, int &nOffset, int &nLen);

    int set_keep_live(int keep_alive_times, int keep_alive_interval);

    int SendHeartbeatMsg();

    int ReceiveTimer(int fd); //����Ƿ�����������

    CCascadeCmdHandler(); //���캯��

    virtual ~CCascadeCmdHandler(); //��������





    int open(void*); //��ʼ������

    int ConnectToCascadeServer(short port, char* chIP); //���ӵ�ICS

    int svc(); //�߳���

    

    void Register();



    static bool m_bConnected; //�Ƿ�������״̬

    static int socket_; //���Ӿ��

private:

    //recv buffer to hole recv data

    char chRecvBuffer[MAX_MSG_BODYLEN];

    //recv data pointer,maybe one recv can't recv total packet or over a packet

    unsigned long dwRecvBuffLen;



    static bool bGoWork_; //�������б�־

    static bool bThread_flag_; //�߳�������־



};



#endif 
