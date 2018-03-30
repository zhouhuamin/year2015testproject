/* 
* File:   ProcessImage.h
* Author: root
*
* Created on 2015��1��28��, ����10:45
*/
#ifndef PROCESSIMAGE_H
#define	PROCESSIMAGE_H
#include <pthread.h>
#include <set>
#include <stack>
#include <vector>
#include <string>
#include <cstdlib>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

typedef unsigned char BYTE;

class ProcessWeight {
public:
	ProcessWeight();
	ProcessWeight(const ProcessWeight& orig);
	virtual ~ProcessWeight();


	void Init();
	void Run();
private:
	pthread_t controllerThreadID;

private:
	static void *ProcessControllerThread(void *pParam);
	void ProcessControllerProc();

private:
	int		CreateSocket();
	int		ConnectSocket(int nSocket,const char * szHost,int nPort);
	int		CheckSocketValid(int nSocket);
	int		CloseSocket(int nSocket);
	void	SetSocketNotBlock(int nSocket);
	void	SysSleep(long nTime);
	int		SocketWrite(int nSocket,char * pBuffer,int nLen,int nTimeout);
	int		SocketRead(int nSocket,void * pBuffer,int nLen);   
	size_t	ReadAll(FILE *fd, void *buff, size_t len);
	size_t	WriteAll(FILE *fd, void *buff, size_t len);
	int set_keep_live(int nSocket, int keep_alive_times, int keep_alive_interval);
public:
	int     Wait();

private:
	std::string		Byte2String(BYTE ch);
	int				my_itoa(int val, char* buf, int radix);
	std::string		GetCurTime(void);
	long			getCurrentTime();

private:
	//_READ_DATA_CALLBACK_ m_pReadDataCallback;

private:
	
	
};
#endif	/* PROCESSIMAGE_H */

