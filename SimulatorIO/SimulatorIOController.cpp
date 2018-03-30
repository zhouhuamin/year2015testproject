// ex_7_1.cpp : Defines the entry point for the console application.
//	map local port to remote host port

#if defined(_WIN32)||defined(_WIN64)
#pragma warning(disable: 4996)
#include	<winsock2.h>
#include	<windows.h>
#include	<process.h>
#pragma comment(lib,"ws2_32.lib")	//	64λ��������Ҫ����Ӧ�ĵ���

WORD wVersionRequested;
WSADATA wsaData;

void InitWinSock()
{
	wVersionRequested = MAKEWORD( 2, 0 );
	WSAStartup(wVersionRequested,&wsaData);
}
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
#include <unistd.h>
#include <strings.h>
#include	"sys/types.h"
#include	"string.h"
#include 	<sys/types.h>
#include 	<fcntl.h>
#include	"stdio.h"
#include	"stdlib.h"
#include <syslog.h>
#include <string>
#include <sys/time.h>
#include "tinyxml/tinyxml.h"

using namespace std;

extern "C"
{
typedef void*(*THREADFUNC)(void*);
}

typedef unsigned char BYTE;

#if	defined(_WIN32)||defined(_WIN64)	
	CRITICAL_SECTION MyMutex;
#else
	pthread_mutex_t MyMutex;
#endif
int GlobalStopFlag=0;
#define	MAX_SOCKET	0x2000
//	max client per server
#define MAX_HOST_DATA 4096
#define random(x) (rand()%x)
int GlobalRemotePort=-1;
char GlobalRemoteHost[256];
	char g_szWeight[50] = {0};
	volatile int  g_Flag = 0;

pthread_mutex_t g_StatusMutex;

typedef struct _SERVER_PARAM
{
	int nSocket;
	sockaddr_in ServerSockaddr;
	int nEndFlag;	//	�ж�accept �߳��Ƿ����
	int pClientSocket[MAX_SOCKET];	//	��ſͻ������ӵ��׽���
}	SERVER_PARAM;

typedef	struct _CLIENT_PARAM
{
	int nSocket;
	unsigned int nIndexOff;
	int nEndFlag;	//	�жϴ���ͻ������ӵ��߳��Ƿ����
	SERVER_PARAM * pServerParam;
	char szClientName[256];
}	CLIENT_PARAM;

struct structConfigInfo
{
	std::string strKaKouIPHandle;
	std::string strKaKouPortHandle;
	std::string strWcfURLHandle;
	std::string strLocalPortHandle;

	std::string strImageIP;
	std::string strImagePort;
};
structConfigInfo g_configInfo;

int GetProcessPath(char* cpPath)
{
	int iCount;
	iCount = readlink("/proc/self/exe", cpPath, 256);

	if (iCount < 0 || iCount >= 256)
	{
		syslog(LOG_DEBUG, "********get process absolute path failed,errno:%d !\n", errno);
		return -1;
	}

	cpPath[iCount] = '\0';
	return 0;
}


void ReadConfigInfo(struct structConfigInfo &configInfo)
{
	//char szAppName[MAX_PATH] = {0};
	//size_t  len = 0;
	//::GetModuleFileName(AfxGetInstanceHandle(), szAppName, sizeof(szAppName));
	//len = strlen(szAppName);
	//for(size_t i=len; i>0; i--)
	//{
	//	if(szAppName[i] == '\\')
	//	{
	//		szAppName[i+1]='\0';
	//		break;
	//	}
	//}


	//CString strFileName=szAppName;
	//strFileName += "SimulationPlatformConfig.xml";
	char exeFullPath[256]	= {0};
	char EXE_FULL_PATH[256]	= {0};

	GetProcessPath(exeFullPath); //�õ�����ģ�����ƣ�ȫ·��

	string strFullExeName(exeFullPath);

	int nLast = strFullExeName.find_last_of("/");

	strFullExeName = strFullExeName.substr(0, nLast + 1);

	strcpy(EXE_FULL_PATH, strFullExeName.c_str());

	char cpPath[256] = {0};

	//char temp[256] = {0};

	sprintf(cpPath, "%sSimulationPlatformConfig.xml", strFullExeName.c_str());



	TiXmlDocument doc(cpPath);
	doc.LoadFile();

	//TiXmlNode* node = 0;
	//TiXmlElement* areaInfoElement = 0;
	//TiXmlElement* itemElement = 0;

	TiXmlHandle docHandle( &doc );
	TiXmlHandle ConfigInfoHandle 	= docHandle.FirstChildElement("ConfigInfo");
	TiXmlHandle KaKouIPHandle 	= docHandle.FirstChildElement("ConfigInfo").ChildElement("KaKouIP", 0).FirstChild();
	TiXmlHandle KaKouPortHandle 	= docHandle.FirstChildElement("ConfigInfo").ChildElement("KaKouPort", 0).FirstChild();
	TiXmlHandle WcfURLHandle 	= docHandle.FirstChildElement("ConfigInfo").ChildElement("WcfURL", 0).FirstChild();
	TiXmlHandle LocalPortHandle 	= docHandle.FirstChildElement("ConfigInfo").ChildElement("LocalPort", 0).FirstChild();

//=====================================
	TiXmlHandle ImageIPHandle 	= docHandle.FirstChildElement("ConfigInfo").ChildElement("ImageIP", 0).FirstChild();
	TiXmlHandle ImagePortHandle 	= docHandle.FirstChildElement("ConfigInfo").ChildElement("ImagePort", 0).FirstChild();
//=====================================


	string strKaKouIPHandle		= "";
	string strKaKouPortHandle	= "";
	string strWcfURLHandle		= "";
	string strLocalPortHandle	= "";

	string strImageIP			= "";
	string strImagePort		= "";


	if (KaKouIPHandle.Node() != NULL)
		configInfo.strKaKouIPHandle = strKaKouIPHandle 			= KaKouIPHandle.Text()->Value();

	if (KaKouPortHandle.Node() != NULL)
		configInfo.strKaKouPortHandle = strKaKouPortHandle 		= KaKouPortHandle.Text()->Value();

	if (WcfURLHandle.Node() != NULL)
		configInfo.strWcfURLHandle = strWcfURLHandle 			= WcfURLHandle.Text()->Value();

	if (LocalPortHandle.Node() != NULL)
		configInfo.strLocalPortHandle = strLocalPortHandle 		= LocalPortHandle.Text()->Value();

	if (ImageIPHandle.Node() != NULL)
		configInfo.strImageIP = strImageIP 					= ImageIPHandle.Text()->Value();

	if (ImagePortHandle.Node() != NULL)
		configInfo.strImagePort = strImagePort 				= ImagePortHandle.Text()->Value();


	syslog(LOG_DEBUG, "%s, %s, %s, %s, %s, %s", strKaKouIPHandle.c_str(), strKaKouPortHandle.c_str(), strWcfURLHandle.c_str(), strLocalPortHandle.c_str(), strImageIP.c_str(), strImagePort.c_str());
}

void UninitWinSock()
{
#if	defined(_WIN32)||defined(_WIN64)	
	WSACleanup();
#endif
}

void	MyInitLock()
{
#if	defined(_WIN32)||defined(_WIN64)
	InitializeCriticalSection(&MyMutex);
#else
	pthread_mutex_init(&MyMutex,NULL);
#endif
}

void	MyUninitLock()
{
#if	defined(_WIN32)||defined(_WIN64)
	DeleteCriticalSection(&MyMutex);
#else
	pthread_mutex_destroy(&MyMutex);
#endif
}

void MyLock()
{
#if	defined(_WIN32)||defined(_WIN64)
    EnterCriticalSection(&MyMutex);
#else
	pthread_mutex_lock(&MyMutex);
#endif
}

void MyUnlock()
{
#if	defined(_WIN32)||defined(_WIN64)
    LeaveCriticalSection(&MyMutex);
#else
	pthread_mutex_unlock(&MyMutex);
#endif
}


void PrintUsage(const char * szAppName)
{
	syslog(LOG_DEBUG, "Usage : %s <LocalPort> <RemoteHostName> <RemotePort> \n",szAppName);
}

int CreateSocket()
{
	int nSocket;
	nSocket=(int)socket(PF_INET,SOCK_STREAM,0);
	return nSocket;
}


int CheckSocketResult(int nResult)
{
//	check result;
#if !defined(_WIN32)&&!defined(_WIN64)
	if(nResult==-1)
		return 0;
	else
		return 1;
#else
	if(nResult==SOCKET_ERROR)
		return 0;
	else
		return 1;
#endif
}

int CheckSocketValid(int nSocket)
{
//	check socket valid

#if !defined(_WIN32)&&!defined(_WIN64)
	if(nSocket==-1)
		return 0;
	else
		return 1;
#else
	if(((SOCKET)nSocket)==INVALID_SOCKET)
		return 0;
	else
		return 1;
#endif
}

int BindPort(int nSocket,int nPort)
{
	int rc;
	int optval=1;
#if	defined(_WIN32)||defined(_WIN64)
	rc=setsockopt((SOCKET)nSocket,SOL_SOCKET,SO_REUSEADDR,
		(const char *)&optval, sizeof(int));
#else
	rc=setsockopt(nSocket,SOL_SOCKET,SO_REUSEADDR,
		(const char *)&optval,sizeof(int));
#endif
	if(!CheckSocketResult(rc))
		return 0;
   	sockaddr_in name;
   	memset(&name,0,sizeof(sockaddr_in));
   	name.sin_family=AF_INET;
   	name.sin_port=htons((unsigned short)nPort);
	name.sin_addr.s_addr=INADDR_ANY;
#if	defined(_WIN32)||defined(_WIN64)
    rc=bind((SOCKET)nSocket,(sockaddr *)&name,sizeof(sockaddr_in));
#else
    rc=bind(nSocket,(sockaddr *)&name,sizeof(sockaddr_in));
#endif
	if(!CheckSocketResult(rc))
		return 0;
	return 1;
}

int ConnectSocket(int nSocket,const char * szHost,int nPort)
{
    hostent *pHost=NULL;
#if		defined(_WIN32)||defined(_WIN64)
    pHost=gethostbyname(szHost);
    if(pHost==0)
    {
        return 0;
    }
#else
	hostent localHost;
	char pHostData[MAX_HOST_DATA];
	int h_errorno=0;
#ifdef	Linux
	int h_rc=gethostbyname_r(szHost,&localHost,pHostData,MAX_HOST_DATA,&pHost,&h_errorno);
    if((pHost==0)||(h_rc!=0))
    {
        return 0;
    }
#else
//	we assume defined SunOS
    pHost=gethostbyname_r(szHost,&localHost,pHostData,MAX_HOST_DATA,&h_errorno);
    if((pHost==0))
    {
        return 0;
    }
#endif
#endif
	struct in_addr in;
    memcpy(&in.s_addr, pHost->h_addr_list[0],sizeof (in.s_addr));
    sockaddr_in name;
    memset(&name,0,sizeof(sockaddr_in));
    name.sin_family=AF_INET;
    name.sin_port=htons((unsigned short)nPort);
	name.sin_addr.s_addr=in.s_addr;
#if defined(_WIN32)||defined(_WIN64)
	int rc=connect((SOCKET)nSocket,(sockaddr *)&name,sizeof(sockaddr_in));
#else
	int rc=connect(nSocket,(sockaddr *)&name,sizeof(sockaddr_in));
#endif
	if(rc>=0)
		return 1;
	return 0;
}

int CloseSocket(int nSocket)
{
	int rc=0;
	if(!CheckSocketValid(nSocket))
	{
		return rc;
	}
#if	defined(_WIN32)||defined(_WIN64)
	shutdown((SOCKET)nSocket,SD_BOTH);
	closesocket((SOCKET)nSocket);
#else
	shutdown(nSocket,SHUT_RDWR);
	close(nSocket);
#endif
	rc=1;
	return rc;
}	

int ListenSocket(int nSocket,int nMaxQueue)
{
	int rc=0;
#if	defined(_WIN32)||defined(_WIN64)
	rc=listen((SOCKET)nSocket,nMaxQueue);
#else
	rc=listen(nSocket,nMaxQueue);
#endif
	return CheckSocketResult(rc);
}

void SetSocketNotBlock(int nSocket)
{
//	�ı��ļ����Ϊ������ģʽ
#if	defined(_WIN32)||defined(_WIN64)
	ULONG optval2=1;
	ioctlsocket((SOCKET)nSocket,FIONBIO,&optval2);
#else
    long fileattr;
    fileattr=fcntl(nSocket,F_GETFL);
    fcntl(nSocket,F_SETFL,fileattr|O_NDELAY);
#endif
}

void SysSleep(long nTime)
//	��ʱnTime���룬������ǧ��֮һ��
{
#if defined(_WIN32 )||defined(_WIN64)
//	windows ����
	MSG msg;
	while(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE))
	{
		if(GetMessage(&msg,NULL,0,0)!=-1)
		{
			TranslateMessage(&msg); 
			DispatchMessage(&msg);
		}
	}
	Sleep(nTime);
#else
//	unix/linux����
	timespec localTimeSpec;
	timespec localLeftSpec;
	localTimeSpec.tv_sec=nTime/1000;
	localTimeSpec.tv_nsec=(nTime%1000)*1000000;
	nanosleep(&localTimeSpec,&localLeftSpec);
#endif
}

int CheckSocketError(int nResult)
{
//	���������׽��ִ���
	if(nResult>0)
		return 0;
	if(nResult==0)
		return 1;

#if !defined(_WIN32)&&!defined(_WIN64)
	if(errno==EAGAIN)
		return 0;
	return 1;
#else
	if(WSAGetLastError()==WSAEWOULDBLOCK)
		return 0;
	else
		return 1;
#endif
}

int SocketWrite(int nSocket,char * pBuffer,int nLen,int nTimeout)
{
	int nOffset=0;
	int nWrite;
	int nLeft=nLen;
	int nLoop=0;
	int nTotal=0;
	int nNewTimeout=nTimeout*10;
	while((nLoop<=nNewTimeout)&&(nLeft>0))
	{
		nWrite=send(nSocket,pBuffer+nOffset,nLeft,0);
		if(nWrite==0)
		{
			return -1;
         }
#if defined(_WIN32)||defined(_WIN64)
		if(nWrite==SOCKET_ERROR)
		{
			if(WSAGetLastError()!=WSAEWOULDBLOCK)
			{	
				return -1;
			}
		}
#else
		if(nWrite==-1)
		{
	          if(errno!=EAGAIN)
			{
				return -1;
			}
			  else
			  {
				  usleep(1000);
				  continue;
			  }
		}
#endif
		if(nWrite<0)
		{
			return nWrite;
		}	
		nOffset+=nWrite;
		nLeft-=nWrite;
		nTotal+=nWrite;
		if(nLeft>0)
		{
//	��ʱ100ms
			SysSleep(100);
		}
		nLoop++;
	}
	return nTotal;
}

int  SocketRead(int nSocket,void * pBuffer,int nLen)
{
	if(nSocket==-1)
		return -1;
	int len=0;
#if	defined(_WIN32)||defined(_WIN64)
	len=recv((SOCKET) nSocket,(char *)pBuffer,nLen,0);
#else
	len=recv(nSocket,(char *)pBuffer,nLen,0);
#endif

	if(len==0)
	{
		return -1;
	}
	if(len==-1)
	{
#if	defined(_WIN32)||defined(_WIn64)
		int localError=WSAGetLastError();
		if(localError==WSAEWOULDBLOCK)
			return 0;
		return -1;
#else
		if(errno==0)
			return -1;
		if(errno==EAGAIN)
			return 0;
#endif		
		return len;
	}
	if(len>0)
		return len;
	else
		return -1;
}

void EndClient(void * pParam)
{
    CLIENT_PARAM * localParam=(CLIENT_PARAM *) pParam;
	MyLock();
	localParam->nEndFlag=1;
	localParam->pServerParam->pClientSocket[localParam->nIndexOff]=-1;
	delete (CLIENT_PARAM *)localParam;
	MyUnlock();
#ifdef	_WIN32
	Sleep(50);
	_endthread();
    return ;
#else
	SysSleep(50);
	pthread_exit(NULL);
	return ;
#endif

}



int ReceiveTimer(int fd)
{
    int iRet = 0;
    fd_set rset;
    struct timeval tv;

    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    tv.tv_sec = 1;
    tv.tv_usec = 500 * 1000;

    iRet = select(fd + 1, &rset, NULL, NULL, &tv);
    return iRet;
}


#ifdef	_WIN32
void ClientProc(void * pParam)
#else
void * ClientProc(void * pParam)
#endif
{
//	�ͻ��˴����߳�,�ѽ��յ����ݷ��͸�Ŀ�����,����Ŀ��������ص����ݷ��ص��ͻ���
    CLIENT_PARAM * localParam=(CLIENT_PARAM *) pParam;
	localParam->nEndFlag=0;
	int nSocket=localParam->nSocket;

	int nLen=0;
#if	defined(_WIN32)||defined(_WIN64)
	MSG msg;
#endif
	int nLoopTotal=0;
	//int nLoopMax=20*300;	//	300 ���ж�ѡѭ��
#define	nMaxLen 0x1000
	char pBuffer[nMaxLen+1]		= {0};
	char pNewBuffer[nMaxLen+1]	= {0};
	memset(pBuffer,	0,	nMaxLen);
	memset(pNewBuffer,	0,	nMaxLen);
	int nSocketErrorFlag=0;

	int nRecvLen = 11;
	int nSendLen = 0;
	int nFistFlag = 0;


	while(!GlobalStopFlag&&!nSocketErrorFlag)
	{
		nLoopTotal++;
#if	defined(_WIN32)||defined(_WIN64)		
		while(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE))
		{
			if(GetMessage(&msg,NULL,0,0)!=-1)
			{
				TranslateMessage(&msg); 
				DispatchMessage(&msg);
			}
		}
#endif

/*
		if (ReceiveTimer(nSocket) > 0)
		{
		}
		else
		{
			SysSleep(3000);
			continue;
		}*/


		// 0.
		nLen = SocketRead(nSocket, pBuffer, nRecvLen);	
		//	��ȡ�ͻ�������
		syslog(LOG_DEBUG, "recv  nLen=%d\n", nLen);
		char szTmpData[5 + 1] = {0};
		string strLogData = "Recv Data:";
		if(nLen>0)
		{
			strLogData = "Recv Data:";
			for (int i = 0; i < nLen; ++i)
			{
				memset(szTmpData, 0x00, 5);
				sprintf(szTmpData, "%02X ", (BYTE)pBuffer[i]);
				strLogData += szTmpData;
			}
			syslog(LOG_DEBUG, "%s\n", strLogData.c_str());
			pBuffer[nLen]=0;
		}
		if(nLen<0)
		{
			//	���Ͽ�
			break;
		}


		if (pBuffer[0] == 0x01 && pBuffer[1] == 0x10 && pBuffer[2] == 0x03)
		{
			pBuffer[0] =  0x01;
			pBuffer[1] =  0x10;

			pBuffer[2] =  0x03;
			pBuffer[3] =  0xFF;
			pBuffer[4] =  0x00;
			pBuffer[5] =  0x01;
			pBuffer[6] =  0x31;
			pBuffer[7] =  0xBD;

			int nSendLen = 8;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
	//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}
memset(pBuffer, 0x00, nMaxLen+1);

			SysSleep(1000);
		}
		else if (pBuffer[0] == 0x01 && pBuffer[1] == 0x65 && pBuffer[2] == 0x00)
		{
			pBuffer[0] =  0x01;
			pBuffer[1] =  0x65;

			pBuffer[2] =  0x00;
			pBuffer[3] =  0x0B;
			pBuffer[4] =  0x50;

			int nSendLen = 5;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
	//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}
memset(pBuffer, 0x00, nMaxLen+1);
			SysSleep(1000);
		}
		else
		{
// 01 53 23 2F 0F 00 0C 00 36 43------------1
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x23;
			pBuffer[3] =  0x2F;
			pBuffer[4] =  0x0F;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x0C;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0x36;
			pBuffer[9] =  0x43;

			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);


// 01 53 24 0F 0E 00 0C 00 B7 CF------------2
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x24;
			pBuffer[3] =  0x0F;
			pBuffer[4] =  0x0E;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x0C;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xB7;
			pBuffer[9] =  0xCF;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);


// 01 53 25 0E 0E 00 00 00 8E DE------------3
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x25;
			pBuffer[3] =  0x0E;
			pBuffer[4] =  0x0E;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x00;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0x8E;
			pBuffer[9] =  0xDE;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);


// 01 53 26 0E 0C 00 02 00 8E 35------------4
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x26;
			pBuffer[3] =  0x0E;
			pBuffer[4] =  0x0C;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x02;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0x8E;
			pBuffer[9] =  0x35;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

		

// 01 53 27 0C 08 00 02 00 F7 14------------5
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x27;
			pBuffer[3] =  0x0C;
			pBuffer[4] =  0x08;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x02;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xF7;
			pBuffer[9] =  0x14;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);


// 01 53 28 08 00 07 02 00 B5 8A------------6
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x28;
			pBuffer[3] =  0x08;
			pBuffer[4] =  0x00;
			pBuffer[5] =  0x07;
			pBuffer[6] =  0x02;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xB5;
			pBuffer[9] =  0x8A;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);


// 01 53 29 00 00 00 02 00 E4 5B------------7
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x29;
			pBuffer[3] =  0x00;
			pBuffer[4] =  0x00;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x02;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xE4;
			pBuffer[9] =  0x5B;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

// 01 53 2A 00 01 00 02 00 E5 94------------8
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x2A;
			pBuffer[3] =  0x00;
			pBuffer[4] =  0x01;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x02;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xE5;
			pBuffer[9] =  0x94;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

// 01 53 2B 01 03 0E 02 00 B9 FE------------9
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x2B;
			pBuffer[3] =  0x01;
			pBuffer[4] =  0x03;
			pBuffer[5] =  0x0E;
			pBuffer[6] =  0x02;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xB9;
			pBuffer[9] =  0xFE;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

// 01 53 2C 03 03 00 02 00 A0 4A------------10
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x2C;
			pBuffer[3] =  0x03;
			pBuffer[4] =  0x03;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x02;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xA0;
			pBuffer[9] =  0x4A;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

// 01 53 2D 03 07 00 00 00 A1 CB------------11
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x2D;
			pBuffer[3] =  0x03;
			pBuffer[4] =  0x07;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x00;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xA1;
			pBuffer[9] =  0xCB;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);


// 01 53 2E 07 0F 00 00 00 52 58------------12
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x2E;
			pBuffer[3] =  0x07;
			pBuffer[4] =  0x0F;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x00;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0x52;
			pBuffer[9] =  0x58;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

// 01 53 2F 0F 2F 00 00 00 B9 88------------13
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x2F;
			pBuffer[3] =  0x0F;
			pBuffer[4] =  0x2F;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x00;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xB9;
			pBuffer[9] =  0x88;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);


// 01 53 30 2F 0F 00 0C 00 34 E0------------14
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x30;
			pBuffer[3] =  0x2F;
			pBuffer[4] =  0x0F;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x0C;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0x34;
			pBuffer[9] =  0xE0;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

// 01 53 31 0F 0D 00 0C 00 B5 4E------------15
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x31;
			pBuffer[3] =  0x0F;
			pBuffer[4] =  0x0D;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x0C;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xB5;
			pBuffer[9] =  0x4E;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

// 01 53 32 0D 0F 00 0C 00 CD 05------------16
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x32;
			pBuffer[3] =  0x0D;
			pBuffer[4] =  0x0F;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x0C;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xCD;
			pBuffer[9] =  0x05;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

// 01 53 33 0F 0F 00 00 00 B0 14------------17
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x33;
			pBuffer[3] =  0x0F;
			pBuffer[4] =  0x0F;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x00;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xB0;
			pBuffer[9] =  0x14;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(100);

// 01 53 34 0F 2F 00 00 00 BA 63------------18
			pBuffer[0] = 0x01;
			pBuffer[1] = 0x53;

			pBuffer[2] =  0x34;
			pBuffer[3] =  0x0F;
			pBuffer[4] =  0x2F;
			pBuffer[5] =  0x00;
			pBuffer[6] =  0x00;
			pBuffer[7] =  0x00;

			pBuffer[8] =  0xBA;
			pBuffer[9] =  0x63;
			nSendLen = 10;

			nLen = SocketWrite(nSocket, pBuffer, nSendLen, 30);	
			//	�ͻ�������
			if(nLen>0)
			{
				nLoopTotal=0;
			}
			if(nLen<0)
			{
				//	���Ͽ�
				break;
			}

			SysSleep(1000);


// send data .................
			char szSendData[2] 	= {2, '\0'};
			int nSendDataLen 		= 1;
			int nImagePort 		= atoi(g_configInfo.strImagePort.c_str());
                  int nSendSocket = CreateSocket();
                  if (nSendSocket == -1)
                        {
                            //	���ܽ����׽��֣�ֱ�ӷ���
                    syslog(LOG_DEBUG, "Can't create socket\n");
                    sleep(60 * 3);
                    nSendSocket = CreateSocket();
                    if (nSendSocket == -1)
                           {
                    	continue;
                           }
                        }

                  //SetSocketNotBlock(nNewSocket);
                  //if (ConnectSocket(nNewSocket, GlobalRemoteHost, GlobalRemotePort) <= 0)
                  if (ConnectSocket(nSendSocket, g_configInfo.strImageIP.c_str(), nImagePort) <= 0)
                        {
				//	���ܽ������ӣ�ֱ�ӷ���
				// CloseSocket(nSocket);
				syslog(LOG_DEBUG, "Can't connect host\n");
				CloseSocket(nSendSocket);
				sleep(60 * 3);
				continue;
				//EndClient(pParam);
                        }


                  int nSendLen = SocketWrite(nSendSocket, szSendData, nSendDataLen, 30);
                        
                  if (nSendLen!=nSendDataLen)
                        {
                     syslog(LOG_DEBUG, "send to image server fail......\n");
                        }
                        
                  CloseSocket(nSendSocket);
// send data end






			sleep(60);

		}









		//if((nSocketErrorFlag==0)&&(nLoopTotal>0))
		//{
		//	SysSleep(100);
		//	if(nLoopTotal>=nLoopMax)
		//	{
		//		nLoopTotal=0;
		//	}
		//}
	}
	CloseSocket(nSocket);
	EndClient(pParam);
#ifdef	_WIN32
	Sleep(50);
	_endthread();
	return ;
#else
	SysSleep(50);
	pthread_exit(NULL);
	return NULL;
#endif
}


#ifdef	_WIN32
void AcceptProc(void * pParam)
#else
void * AcceptProc(void * pParam)
#endif
{
    SERVER_PARAM * localParam=(SERVER_PARAM *) pParam;
    int s;
    unsigned int Len;
    sockaddr_in inAddr;
	unsigned long nIndex;
	char pClientName[256] = {0};
	unsigned int nCurIndex=(unsigned int)-1;
	unsigned int nIndexOff=0;
	CLIENT_PARAM * localClientParam=NULL;

	localParam->nEndFlag=0;
	for(nIndex=0;nIndex<MAX_SOCKET;nIndex++)
	{
		localParam->pClientSocket[nIndex]=-1;
	}
	while(!GlobalStopFlag)
	{
       	Len=sizeof(sockaddr_in);
       	inAddr=localParam->ServerSockaddr;
#ifdef	Linux
        s=accept(localParam->nSocket,(sockaddr *)(&inAddr),
				(socklen_t *)&Len);
#else
#if	defined(_WIN32)||defined(_WIN64)
        s=(int)accept((SOCKET)localParam->nSocket,(sockaddr *)(&inAddr),
                                (int *)&Len);
#else
        s=accept(localParam->nSocket,(sockaddr *)(&inAddr),
                               (int *)&Len);
#endif
#endif
		if(s==0)
			break;	//	socket error
		if(s==-1)
		{
			if(CheckSocketError(s))
				break;	//	socket error
			SysSleep(50);
			continue;
		}
		MyLock();
		strcpy(pClientName,inet_ntoa(inAddr.sin_addr));
//	��Linux��,inet_ntoa()�������̲߳���ȫ��
		MyUnlock();
		SetSocketNotBlock(s);
		nIndexOff=nCurIndex;
		MyLock();
//	���ҿ��е��׽���λ��,
//	��Ϊ�ͻ��˵��׽����̴߳������ҲҪ����������,�����Ҫ����
		for(nIndex=0;nIndex<MAX_SOCKET;nIndex++)
		{
			nIndexOff++;
			nIndexOff%=MAX_SOCKET;
			if(localParam->pClientSocket[nIndexOff]==-1)
			{
//	��ƫ����������½������׽���
				localParam->pClientSocket[nIndexOff]=s;
				nCurIndex=nIndexOff;
				break;
			}
		}
		MyUnlock();
		if(nIndex>=MAX_SOCKET)
		{
//	û���ҵ�����׽��ֵ�ƫ����,̫�������
			CloseSocket(s);
			SysSleep(50);
			continue;
		}
		localClientParam=new CLIENT_PARAM;
		localClientParam->nEndFlag=1;
		localClientParam->nIndexOff=nCurIndex;
		localClientParam->nSocket=s;
		localClientParam->pServerParam=localParam;
		memset(localClientParam->szClientName,0,256);
		strcpy(localClientParam->szClientName,pClientName);
		int nThreadErr=0;

#if	defined(_WIN32)||defined(_WIN64)
		if(_beginthread(ClientProc,NULL,localClientParam)<=1)
			nThreadErr=1;
#else
		pthread_t localThreadId;
        nThreadErr=pthread_create(&localThreadId,NULL,
            (THREADFUNC)ClientProc,localClientParam);
		if(nThreadErr==0)
			pthread_detach(localThreadId);
//	�ͷ��߳�˽������,���صȴ�pthread_join();
#endif
		if(nThreadErr)
		{
//	�����߳�ʧ��
			MyLock();
			localParam->pClientSocket[nIndexOff]=-1;
			MyUnlock();
			continue;
		}
	}
	int nUsedSocket=MAX_SOCKET;
	while(nUsedSocket>0)
	{
		nUsedSocket=0;
		SysSleep(50);
		MyLock();
		for(nIndex=0;nIndex<MAX_SOCKET;nIndex++)
		{
			if(localParam->pClientSocket[nIndex]!=-1)
			{
				nUsedSocket=1;
				break;
			}
		}
		MyUnlock();
	}
#ifdef	_WIN32
	Sleep(50);
	localParam->nEndFlag=1;
	_endthread();
    return ;
#else
	SysSleep(50);
	localParam->nEndFlag=1;
	pthread_exit(NULL);
	return NULL;
#endif
}

#if !defined(_WIN32)&&!defined(_WIN64)
extern "C" void	sig_term(int signo)
{
	if(signo==SIGTERM)
	{
		GlobalStopFlag=1;
		signal(SIGTERM,sig_term);
		signal(SIGHUP,SIG_IGN);
		signal(SIGKILL,sig_term);
		signal(SIGINT,sig_term);
    	signal(SIGPIPE,SIG_IGN);
    	signal(SIGALRM,SIG_IGN);
	}
}

long daemon_init()
{
	pid_t pid;
    signal(SIGALRM,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);

	if((pid=fork())<0)
		return 0;
	else if(pid!=0)
		exit(0);
	setsid();

	signal(SIGTERM,sig_term);
	signal(SIGINT,sig_term);
	signal(SIGHUP,SIG_IGN);
	signal(SIGKILL,sig_term);
    signal(SIGPIPE,SIG_IGN);
    signal(SIGALRM,SIG_IGN);

	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	return 1;
}
#else
BOOL WINAPI ControlHandler ( DWORD dwCtrlType )
{
    switch( dwCtrlType )
    {
        case CTRL_BREAK_EVENT:  // use Ctrl+C or Ctrl+Break to simulate
        case CTRL_C_EVENT:      // SERVICE_CONTROL_STOP in debug mode
		case CTRL_CLOSE_EVENT:	//	close console	
            GlobalStopFlag=1;
            return TRUE;
            break;
    }
    return FALSE;
}

#endif

void random_chars(char *buf, int len) 
{
    if (!buf || len == 0) {
        return;
    }   
    struct timeval tv; 
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec + tv.tv_usec);
    char src[] = "0123456789";
    //int cnt = 10; 
    for (int i = 0; i < len; ++i) {
        buf[i] = src[rand() % 36];
    }   
}

void *Thread1(void *arg)
{
	signal(SIGPIPE,SIG_IGN);
	while (1)
	{
		if (g_Flag == 1)
		{
			sleep(1);
			continue;
		}
		struct timeval tv; 
    		gettimeofday(&tv, NULL);
    		srand(tv.tv_sec + tv.tv_usec);
		int nRand = rand();
		pthread_mutex_lock(&g_StatusMutex);
		sprintf(g_szWeight, "%06d", nRand);
		pthread_mutex_unlock(&g_StatusMutex);
		
		SysSleep(50);
	}
	return 0;
}

void *Thread2(void *arg)
{
	signal(SIGPIPE,SIG_IGN);
	while (1)
	{
		
		g_Flag	= 1;
		/*struct timeval tv; 
    		gettimeofday(&tv, NULL);
    		srand(tv.tv_sec + tv.tv_usec);
		srand((int)time(0));
		int nRand = random(100000);
		pthread_mutex_lock(&g_StatusMutex);
		sprintf(g_szWeight, "%06d", nRand);
		pthread_mutex_unlock(&g_StatusMutex);
		*/
		sleep(3);
		
		g_Flag	= 0;
	}
	return 0;
}

int main(int argc, char * argv[])
{
	signal(SIGALRM,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	openlog("SimulatorIO", LOG_PID, LOG_LOCAL7);
	ReadConfigInfo(g_configInfo);
	//srand((int)time(0));
	    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec + tv.tv_usec);
	//if(argc<4)
	//{
	//	PrintUsage(argv[0]);
	//	return -1;
	//}
	int nLocalPort	=-1;
	int nRemotePort	=-1;
	nLocalPort		= atoi(g_configInfo.strLocalPortHandle.c_str());
	nRemotePort		=  atoi(g_configInfo.strKaKouPortHandle.c_str());
	if ((nLocalPort<=0) || (nRemotePort<=0))
	{
		syslog(LOG_DEBUG, "invalid local port or remote port \n");
		return -1;
	}

	// char * szRemoteHost = "192.168.1.130"; // argv[2];
	GlobalRemotePort = nRemotePort;
	strcpy(GlobalRemoteHost, g_configInfo.strKaKouIPHandle.c_str());
//#if !defined(_WIN32)&&!defined(_WIN64)
////	become a daemon program
//	daemon_init();
//#else
//	InitWinSock();
//    SetConsoleCtrlHandler( ControlHandler, TRUE );
//#endif
	int nSocket = CreateSocket();
//	create socket
	if(!CheckSocketValid(nSocket))
	{
		UninitWinSock();
		return -1;
	}
	int rc=0;
	rc=BindPort(nSocket,nLocalPort);
	if(!rc)
	{
		CloseSocket(nSocket);
		UninitWinSock();
		return -1;
	}
	rc=ListenSocket(nSocket,30);
	if(!rc)
	{
		CloseSocket(nSocket);
		UninitWinSock();
		return -1;
	}
	MyInitLock();				
	SetSocketNotBlock(nSocket);
	SERVER_PARAM * localServerParam=new SERVER_PARAM;
   	sockaddr_in name;
   	memset(&name,0,sizeof(sockaddr_in));
   	name.sin_family=AF_INET;
   	name.sin_port=htons((unsigned short)nLocalPort);
	name.sin_addr.s_addr=INADDR_ANY;
	localServerParam->ServerSockaddr=name;
	localServerParam->nSocket=nSocket;
	localServerParam->nEndFlag=1;
#if defined(_WIN32)||defined(_WIN64)
	uintptr_t	localThreadId;
#else
	pthread_t localThreadId;
#endif

	int nThreadErr=0;

#if	defined(_WIN32)||defined(_WIN64)
	if((localThreadId=_beginthread(AcceptProc,NULL,localServerParam))<=1)
		nThreadErr=1;
#else
    nThreadErr=pthread_create(&localThreadId,NULL,
            (THREADFUNC)AcceptProc,localServerParam);
	if(nThreadErr==0)
		pthread_detach(localThreadId);
//	�ͷ��߳�˽������,���صȴ�pthread_join();
#endif
	if(nThreadErr)
	{
//	�����߳�ʧ��
		MyUninitLock();
		CloseSocket(nSocket);
		UninitWinSock();
		delete localServerParam;
		return -1;
	}

	pthread_t tid_thread1;
	pthread_t tid_thread2;
	pthread_create(&tid_thread1, NULL, Thread1, NULL);
	pthread_create(&tid_thread2, NULL, Thread2, NULL);

	while((GlobalStopFlag==0)||(localServerParam->nEndFlag==0))
	{
		SysSleep(500);
	}
	printf("%d:%d\n", GlobalStopFlag, localServerParam->nEndFlag);
	MyUninitLock();
	CloseSocket(nSocket);
	UninitWinSock();
	delete localServerParam;
	return 0;
}

