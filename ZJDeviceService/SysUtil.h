#ifndef _SYS_UTIL_H_
#define	_SYS_UTIL_H_

#pragma warning(disable: 4996)


#if defined(WIN32)||defined(_WIN64)
#pragma warning(disable: 4996)
#include	<winsock2.h>
#include	<windows.h>
#include	<process.h>
#pragma comment(lib,"ws2_32.lib")	//	64位环境下需要做相应的调整

#else
#include	<pthread.h>
#include	<sys/socket.h>
#include	<sys/types.h>
#include	<netinet/in.h>
#include	<netdb.h>
#include 	<arpa/inet.h>
#include	<unistd.h>
#include	<signal.h>
#include	<errno.h>
#endif


#include	"sys/types.h"
#include	"string.h"
#include 	<sys/types.h>
#include 	<fcntl.h>
#include	"stdio.h"
#include	"stdlib.h"



class SysUtil
{
public:
    SysUtil();
    virtual ~SysUtil();
public:
    static int SearchTailPos(char *chBuffer, int nDataLen);
    static int SearchHeadPos(char *chBuffer, int nDataLen);
    static int ReceiveTimer(int fd);

    static int InitNetLib();
    static int ReleaseNetLib();

    static int ConnectSocket(int nSocket, unsigned long serverip, int port);
    static int CreateSocket();
    static int CheckSocketResult(int nResult);
    static int CheckSocketValid(int nSocket);
    static int BindPort(int nSocket, int nPort);
    static int ConnectSocket(int nSocket, const char * szHost, int nPort);
    static int CloseSocket(int nSocket);
    static int ListenSocket(int nSocket, int nMaxQueue);
    static void SetSocketNotBlock(int nSocket);
    static int CheckSocketError(int nResult);
    static int SocketWrite(int nSocket, char * pBuffer, int nLen, int nTimeout);
    static int SocketRead(int nSocket, void * pBuffer, int nLen);
    static void SysSleep(long ms);

};

#endif	

