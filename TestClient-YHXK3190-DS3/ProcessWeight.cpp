/* 
* File:   ProcessWeight.cpp
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
#include "ProcessWeight.h"

using namespace std;
int GlobalStopFlag = 0;
#define MAX_HOST_DATA   4096

using namespace std;

ProcessWeight::ProcessWeight()
{
	syslog(LOG_DEBUG, "Enter ProcessWeight Constructor!\n");
}

ProcessWeight::ProcessWeight(const ProcessWeight& orig)
{
}

ProcessWeight::~ProcessWeight()
{
}

void ProcessWeight::Init()
{
	//SetReadDataCallback((_READ_DATA_CALLBACK_) ReceiveReaderData, this);
	return;
}

void ProcessWeight::Run()
{
	pthread_create (&controllerThreadID, NULL, ProcessControllerThread, NULL);
	return;
}

void *ProcessWeight::ProcessControllerThread(void* pParam)
{
	ProcessWeight *pThis = (ProcessWeight*)pParam;
	pThis->ProcessControllerProc();
	return 0; 
}

void ProcessWeight::ProcessControllerProc()
{
	//printf("Enter ProcessControllerProc\n");
	//	�ͻ��˴����߳�,�ѽ��յ����ݷ��͸�Ŀ�����,����Ŀ��������ص����ݷ��ص��ͻ���
	int nLen=0;
	int nLoopTotal=0;
	int nLoopMax=20*300;	//	300 ���ж�ѡѭ��
#define	nMaxLen 256		// 0x1000
	char pBuffer[nMaxLen+1];
	char pNewBuffer[nMaxLen+1];
	memset(pBuffer,0,nMaxLen);
	memset(pNewBuffer,0,nMaxLen);
	int nSocketErrorFlag=0;
	int nNewLen=0;

	while (!GlobalStopFlag)
	{
		int nNewSocket = CreateSocket();
		if (nNewSocket == -1)
		{
			//	���ܽ����׽��֣�ֱ�ӷ���
			syslog(LOG_DEBUG, "Can't create socket\n");
			sleep(10);
			nNewSocket = CreateSocket();
			if(nNewSocket == -1)
			{
				continue;
			}		
		}
		//SetSocketNotBlock(nNewSocket);
		//if (ConnectSocket(nNewSocket, GlobalRemoteHost, GlobalRemotePort) <= 0)
		if (ConnectSocket(nNewSocket, "127.0.0.1", 5000) <= 0)
		{
			//	���ܽ������ӣ�ֱ�ӷ���
			//CloseSocket(nSocket);
			syslog(LOG_DEBUG, "Can't connect host\n");
			CloseSocket(nNewSocket);
			sleep(3);
			continue;
			//EndClient(pParam);
		}

		set_keep_live(nNewSocket, 3, 30);
		//ConnectSocket(nNewSocket, "192.168.1.188", 502);
		//struct timeval timeout ;
		//fd_set sets;
		//FD_ZERO(&sets);
		//FD_SET(nNewSocket, &sets);
		//timeout.tv_sec = 15; //���ӳ�ʱ15��
		//timeout.tv_usec =0;
		//int result = select(0, 0, &sets, 0, &timeout);
		//if (result <=0)
		//{
		//	CloseSocket(nNewSocket);
		//	printf("Enter select\n");
		//	sleep(10);
		//	continue;
		//}

		//SetSocketNotBlock(nNewSocket);
		while(!GlobalStopFlag && !nSocketErrorFlag)
		{
			nLoopTotal++;
			nLen=SocketRead(nNewSocket, pNewBuffer, nMaxLen);	
			//	��ȡ�ͻ�������
			syslog(LOG_DEBUG, "recv  nLen=%d\n", nLen);
			if(nLen>0)
			{
				//syslog(LOG_DEBUG, "%s Recv Data:", GetCurTime().c_str());
                                char szTmpData[5 + 1] = {0};
				std::string strLogData = "Recv Data:";
				for (size_t i = 0; i < nLen; ++i)
				{
					memset(szTmpData, 0x00, 5);
					sprintf(szTmpData, "%02X ", (BYTE)pNewBuffer[i]);
					strLogData += szTmpData;
				}
				syslog(LOG_DEBUG, "%s\n", strLogData.c_str());
				pNewBuffer[nLen]=0;
				nLoopTotal=0;
				int nProcessed = 0;
				int nLenLeft = nLen;
				while (nLenLeft > 0)
				{
					// �����ж�,������������
					if (pNewBuffer[0] == 0x02 && pNewBuffer[11] == 0x03)
					{
						char szWeight[50] = {0};
						int nStep = 12;
						vector<BYTE> upDataVect((BYTE*)pNewBuffer, (BYTE*)pNewBuffer + nStep);
						nLenLeft -= nStep;
						if (nLenLeft > 0)
							memmove(pNewBuffer, pNewBuffer + nStep, nLenLeft);
						
						int nDecimal	= 0;
						if (upDataVect[8] >= 0x30)
							nDecimal	= upDataVect[8] - 0x30;
						else
							nDecimal	= upDataVect[8];
						if (nDecimal > 4)
							nDecimal	= 4;
						for (int i = 2, j = 0; i < 8; ++i, ++j)
						{
							if (j == 6 - nDecimal)
							{
								szWeight[j] = '.';
								szWeight[j+1]	= upDataVect[i];
								++j;
								continue;
							}
							szWeight[j]	= upDataVect[i];
						}

						syslog(LOG_DEBUG, "szWeight:%s, %lf\n", szWeight, atof(szWeight));
					}
					else
					{
						syslog(LOG_DEBUG, "%s head[0] head[11]: 0x%02X, 0x%02X\n", GetCurTime().c_str(), (BYTE)pNewBuffer[0], (BYTE)pNewBuffer[11]);
						nLenLeft	= 0;
					}
				}
			}
			if(nLen<0)
			{
				//	���Ͽ�
				CloseSocket(nNewSocket);
				break;
			}

			if((nSocketErrorFlag==0)&&(nLoopTotal>0))
			{
				SysSleep(50);
				if(nLoopTotal>=nLoopMax)
				{
					nLoopTotal=0;
				}
			}
		}		
	}
	SysSleep(50);
	pthread_exit(NULL);
	return;    
}

int ProcessWeight::CreateSocket()
{
	int nSocket;
	nSocket=(int)socket(PF_INET,SOCK_STREAM,0);
	return nSocket;
}

int ProcessWeight::ConnectSocket(int nSocket,const char * szHost,int nPort)
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

int ProcessWeight::CheckSocketValid(int nSocket)
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

int ProcessWeight::CloseSocket(int nSocket)
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

void ProcessWeight::SetSocketNotBlock(int nSocket)
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

void ProcessWeight::SysSleep(long nTime) //	��ʱnTime���룬������ǧ��֮һ��
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

int ProcessWeight::SocketWrite(int nSocket,char * pBuffer,int nLen,int nTimeout)
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

int  ProcessWeight::SocketRead(int nSocket,void * pBuffer,int nLen)
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

size_t ProcessWeight::ReadAll(FILE *fd, void *buff, size_t len)
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

size_t ProcessWeight::WriteAll(FILE *fd, void *buff, size_t len)
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

std::string ProcessWeight::Byte2String(BYTE ch)
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
int ProcessWeight::my_itoa(int val, char* buf, int radix)
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
string ProcessWeight::GetCurTime(void)
{
	time_t t		= time(0);
	tm *ld			= NULL;
	char tmp[64]	= "";
	ld				= localtime(&t);
	strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", ld);
	return string(tmp);
}
long ProcessWeight::getCurrentTime()    
{    
	struct timeval tv;    
	gettimeofday(&tv,NULL);    
	//return tv.tv_sec * 1000 + tv.tv_usec / 1000; 
	return tv.tv_sec;    
}  

int ProcessWeight::set_keep_live(int nSocket, int keep_alive_times, int keep_alive_interval)
{

#ifdef WIN32                                //WIndows��
	TCP_KEEPALIVE inKeepAlive = {0}; //�������
	unsigned long ulInLen = sizeof (TCP_KEEPALIVE);

	TCP_KEEPALIVE outKeepAlive = {0}; //�������
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
	int keepCount = keep_alive_times; //�ж��Ͽ�ǰ��KeepAlive̽�����
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

int ProcessWeight::Wait()
{
	if (controllerThreadID != 0)
		 pthread_join(controllerThreadID, NULL);
}
