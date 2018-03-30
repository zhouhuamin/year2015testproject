/* 
* File:   ProcessBroadcast.cpp
* Author: root
* 
* Created on 2015��1��28��, ����10:45
*/
#include <stdio.h>
#include <algorithm>
#include <iterator>
#include <set>
#include <vector>
#include <iostream>
#include <syslog.h>
#include <sys/time.h>
#include "ProcessBroadcast.h"
#include "config.h"

using namespace std;
int GlobalStopFlag = 0;
#define MAX_HOST_DATA   4096

using namespace std;

ProcessBroadcast::ProcessBroadcast()
{
}

ProcessBroadcast::ProcessBroadcast(const ProcessBroadcast& orig)
{
}

ProcessBroadcast::~ProcessBroadcast()
{
}

int ProcessBroadcast::CreateSocket()
{
	int nSocket;
	nSocket=(int)socket(PF_INET,SOCK_STREAM,0);
	return nSocket;
}

int ProcessBroadcast::ConnectSocket(int nSocket,const char * szHost,int nPort)
{
	hostent *pHost=NULL;
#if defined(_WIN32)||defined(_WIN64)
	pHost=gethostbyname(szHost);
	if(pHost==0)
	{
		return 0;
	}
#else
	hostent localHost;
	char pHostData[MAX_HOST_DATA];
	int h_errorno=0;
	//#ifdef	Linux
	int h_rc=gethostbyname_r(szHost,&localHost,pHostData,MAX_HOST_DATA,&pHost,&h_errorno);
	if((pHost==0)||(h_rc!=0))
	{
		return 0;
	}
	//#else
	//	//	we assume defined SunOS
	//	pHost=gethostbyname_r(szHost,&localHost,pHostData,MAX_HOST_DATA,&h_errorno);
	//	if((pHost==0))
	//	{
	//		return 0;
	//	}
	//#endif
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

int ProcessBroadcast::CheckSocketValid(int nSocket)
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

int ProcessBroadcast::CloseSocket(int nSocket)
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

void ProcessBroadcast::SetSocketNotBlock(int nSocket)
{
	//	�ı��ļ����Ϊ������ģ�?
#if	defined(_WIN32)||defined(_WIN64)
	ULONG optval2=1;
	ioctlsocket((SOCKET)nSocket,FIONBIO,&optval2);
#else
	long fileattr;
	fileattr=fcntl(nSocket,F_GETFL);
	fcntl(nSocket,F_SETFL,fileattr|O_NDELAY);
#endif
}

void ProcessBroadcast::SysSleep(long nTime) //	��ʱnTime���룬������ǧ��֮һ��
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

int ProcessBroadcast::SocketWrite(int nSocket,char * pBuffer,int nLen,int nTimeout)
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

int  ProcessBroadcast::SocketRead(int nSocket,void * pBuffer,int nLen)
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

size_t ProcessBroadcast::ReadAll(FILE *fd, void *buff, size_t len)
{
	size_t n	= 0;
	size_t sum	= 0;
	do 
	{
		n = fread((char*)buff + sum, 1, len - sum, fd);
		sum += n;
	} while (sum < len && n != 0);
	if (n == 0 && ferror(fd))
		return 0;
	if (n == 0)
		return 1;
	return 1;
}

size_t ProcessBroadcast::WriteAll(FILE *fd, void *buff, size_t len)
{
	size_t n	= 0;
	size_t sum	= 0;
	do 
	{
		n = fwrite((char*)buff + sum, 1, len - sum, fd);
		sum += n;
	} while (sum < len && n != 0);
	if (n == 0 && ferror(fd))
		return 0;
	if (n == 0)
		return 1;
	return 1;
}

std::string ProcessBroadcast::Byte2String(BYTE ch)
{
	BYTE a = ch;
	char buf[128] = {0};
	char tmp[10] = {0};
	char sav[10] = {0};
	//for (int i = 0; i < sizeof(a) / sizeof(BYTE); i++)
	for (int i = 0; i < 1; i++)
	{
		int j = 0;
		// itoa((int)a[i], sav, 2);
		my_itoa((int)a, sav, 2);
		for (int w = 0; w < 8 - strlen(sav); w++)
			tmp[w] = '0';
		for (int w = 8 - strlen(sav); w < 8; w++)
			tmp[w] = sav[j++];
		sprintf(buf, "%8s", tmp);
		//sprintf(buf, "%s%8s", buf, tmp);
		string strRet = buf;
		return strRet;
	}
	return "";
}
int ProcessBroadcast::my_itoa(int val, char* buf, int radix)
{
	//const int radix = 2;
	char* p;
	int a;        //every digit
	int len;
	char* b;    //start of the digit char
	char temp;
	p = buf;
	if (val < 0)
	{
		*p++ = '-';
		val = 0 - val;
	}
	b = p;
	do
	{
		a = val % radix;
		val /= radix;
		*p++ = a + '0';
	} while (val > 0);
	len = (int)(p - buf);
	*p-- = 0;
	//swap
	do
	{
		temp = *p;
		*p = *b;
		*b = temp;
		--p;
		++b;
	} while (b < p);
	return len;
}
string ProcessBroadcast::GetCurTime(void)
{
	time_t t		= time(0);
	tm *ld			= NULL;
	char tmp[64]	= "";
	ld				= localtime(&t);
	strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", ld);
	return string(tmp);
}
long ProcessBroadcast::getCurrentTime()    
{    
	struct timeval tv;    
	gettimeofday(&tv,NULL);    
	//return tv.tv_sec * 1000 + tv.tv_usec / 1000; 
	return tv.tv_sec;    
}  

int ProcessBroadcast::set_keep_live(int nSocket, int keep_alive_times, int keep_alive_interval)
{

#ifdef WIN32                                //WIndows��
	TCP_KEEPALIVE inKeepAlive = {0}; //�������?
	unsigned long ulInLen = sizeof (TCP_KEEPALIVE);

	TCP_KEEPALIVE outKeepAlive = {0}; //�������?
	unsigned long ulOutLen = sizeof (TCP_KEEPALIVE);

	unsigned long ulBytesReturn = 0;

	//����socket��keep aliveΪ5�룬���ҷ��ʹ���Ϊ3��
	inKeepAlive.on_off = keep_alive_times;
	inKeepAlive.keep_alive_interval = keep_alive_interval * 1000; //����KeepAlive̽����ʱ����
	inKeepAlive.keep_alive_time = keep_alive_interval * 1000; //��ʼ�״�KeepAlive̽��ǰ��TCP�ձ�ʱ��


	outKeepAlive.on_off = keep_alive_times;
	outKeepAlive.keep_alive_interval = keep_alive_interval * 1000; //����KeepAlive̽����ʱ����
	outKeepAlive.keep_alive_time = keep_alive_interval * 1000; //��ʼ�״�KeepAlive̽��ǰ��TCP�ձ�ʱ��


	if (WSAIoctl((unsigned int) nSocket, SIO_KEEPALIVE_VALS,
		(LPVOID) & inKeepAlive, ulInLen,
		(LPVOID) & outKeepAlive, ulOutLen,
		&ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
	{
		//ACE_DEBUG((LM_INFO,
		//	ACE_TEXT("(%P|%t) WSAIoctl failed. error code(%d)!\n"), WSAGetLastError()));
	}

#else                                        //linux��
	int keepAlive = 1; //�趨KeepAlive
	int keepIdle = keep_alive_interval; //��ʼ�״�KeepAlive̽��ǰ��TCP�ձ�ʱ��
	int keepInterval = keep_alive_interval; //����KeepAlive̽����ʱ����
	int keepCount = keep_alive_times; //�ж��Ͽ�ǰ��KeepAlive̽�����?
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
