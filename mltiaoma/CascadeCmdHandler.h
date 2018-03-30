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

    int ReceiveTimer(int fd); //检测是否有网络数据

    CCascadeCmdHandler(); //构造函数

    virtual ~CCascadeCmdHandler(); //析构函数





    int open(void*); //初始化连接

    int ConnectToCascadeServer(short port, char* chIP); //连接到ICS

    int svc(); //线程体

    

    void Register();



    static bool m_bConnected; //是否处于连接状态

    static int socket_; //连接句柄

private:

    //recv buffer to hole recv data

    char chRecvBuffer[MAX_MSG_BODYLEN];

    //recv data pointer,maybe one recv can't recv total packet or over a packet

    unsigned long dwRecvBuffLen;



    static bool bGoWork_; //继续运行标志

    static bool bThread_flag_; //线程启动标志



};



#endif 

