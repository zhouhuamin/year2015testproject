#ifdef WIN32
#include "stdafx.h"
#else
#endif

#include "SysUtil.h"
#include "SysMessage.h"

#define  MAX_HOST_DATA       (4096)

SysUtil::SysUtil()
{
}

SysUtil::~SysUtil()
{
}

int SysUtil::CreateSocket()
{
    int nSocket;
    nSocket = (int) socket(PF_INET, SOCK_STREAM, 0);
    return nSocket;
}

int SysUtil::CheckSocketResult(int nResult)
{
    //	check result;
    if (nResult == -1)
        return 0;
    else
        return 1;
}

int SysUtil::CheckSocketValid(int nSocket)
{
    //	check socket valid
    if (nSocket == -1)
        return 0;
    else
        return 1;
}

int SysUtil::BindPort(int nSocket, int nPort)
{
    int rc;
    int optval = 1;
    rc = setsockopt(nSocket, SOL_SOCKET, SO_REUSEADDR,
            (const char *) & optval, sizeof (int));

    if (!CheckSocketResult(rc))
    {
        return 0;
    }

    sockaddr_in name;
    memset(&name, 0, sizeof (sockaddr_in));
    name.sin_family = AF_INET;
    name.sin_port = htons((unsigned short) nPort);
    name.sin_addr.s_addr = INADDR_ANY;

    rc = bind(nSocket, (sockaddr *) & name, sizeof (sockaddr_in));
    if (!CheckSocketResult(rc))
    {
        return 0;
    }

    return 1;
}

int SysUtil::ConnectSocket(int nSocket, const char * szHost, int nPort)
{
    hostent *pHost = NULL;
#ifdef WIN32
	pHost = gethostbyname(szHost);
    if (pHost == 0)
    {
        return 0;
    }
#else
    hostent localHost;
    char pHostData[MAX_HOST_DATA];
    int h_errorno = 0;
#endif

#ifdef	WIN32
    pHost = gethostbyname(szHost);
    if (pHost == 0)
    {
        return 0;
    }
#else
    int h_rc = gethostbyname_r(szHost, &localHost, pHostData, MAX_HOST_DATA, &pHost, &h_errorno);
    if ((pHost == 0) || (h_rc != 0))
    {
        return 0;
    }
#endif

    struct in_addr in;
    memcpy(&in.s_addr, pHost->h_addr_list[0], sizeof (in.s_addr));
    sockaddr_in name;
    memset(&name, 0, sizeof (sockaddr_in));
    name.sin_family = AF_INET;
    name.sin_port = htons((unsigned short) nPort);
    name.sin_addr.s_addr = in.s_addr;
#ifdef WN32
    int rc = connect((SOCKET) nSocket, (sockaddr *) & name, sizeof (sockaddr_in));
#else
    int rc = connect(nSocket, (sockaddr *) & name, sizeof (sockaddr_in));
#endif
    if (rc >= 0)
        return 0;
    return -1;

}

int SysUtil::CloseSocket(int nSocket)
{
    int rc = 1;
    if (!CheckSocketValid(nSocket))
    {
        return rc;
    }
#ifdef WIN32
    shutdown((SOCKET) nSocket, SD_BOTH);
    closesocket((SOCKET) nSocket);
#else
    shutdown(nSocket, SHUT_RDWR);
    close(nSocket);
#endif
    rc = 0;
    return rc;
}

int SysUtil::ListenSocket(int nSocket, int nMaxQueue)
{
    int rc = 0;
    rc = listen(nSocket, nMaxQueue);
    return CheckSocketResult(rc);
}

void SysUtil::SetSocketNotBlock(int nSocket)
{
    //	改变文件句柄为非阻塞模式
#ifdef WIN32
    unsigned long block = 1;
    ioctlsocket(nSocket, FIONBIO, &block);

#else
    int iFlags = fcntl(nSocket, F_GETFL);
    fcntl(nSocket, F_SETFL, iFlags | ~O_NONBLOCK);


#endif


}

int SysUtil::CheckSocketError(int nResult)
{
    //	检查非阻塞套接字错误
    if (nResult > 0)
        return 0;
    if (nResult == 0)
        return 1;

#ifdef WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK)
        return 0;
    else
        return 1;
#else
    if (errno == EAGAIN)
        return 0;
    return 1;
#endif


}

int SysUtil::SocketWrite(int nSocket, char * pBuffer, int nLen, int nTimeout)
{
    int nOffset = 0;
    int nWrite;
    int nLeft = nLen;
    int nLoop = 0;
    int nTotal = 0;
    int nNewTimeout = nTimeout * 10;

    /*
     *	这里的nTimeout其实是重发次数，而不是实际的时间
     */

    while ((nLoop <= nNewTimeout) && (nLeft > 0))
    {
        nWrite = send(nSocket, pBuffer + nOffset, nLeft, 0);
        if (nWrite == 0)
        {
            return -1;
        }
#ifdef WIN32
        if (nWrite == SOCKET_ERROR)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
            {
                return -1;
            }
        }
#else
        if (nWrite == -1)
        {
            if (errno != EAGAIN)
            {
                return -1;
            }
        }
#endif
        if (nWrite < 0)
        {
            return nWrite;
        }
        nOffset += nWrite;
        nLeft -= nWrite;
        nTotal += nWrite;
        if (nLeft > 0)
        {
            //	延时100ms
            SysSleep(100);
        }
        nLoop++;
    }


    return nTotal;



}

int SysUtil::SocketRead(int nSocket, void * pBuffer, int nLen)
{
    if (nSocket == -1)
        return -1;
    int len = 0;
#ifdef WIN32
    len = recv((SOCKET) nSocket, (char *) pBuffer, nLen, 0);
#else
    len = recv(nSocket, (char *) pBuffer, nLen, 0);
#endif

    if (len == 0)
    {
        return -1;
    }
    if (len == -1)
    {
#ifdef WIN32
        int localError = WSAGetLastError();
        if (localError == WSAEWOULDBLOCK)
            return 0;
        return -1;
#else
        if (errno == 0)
            return -1;
        if (errno == EAGAIN)
            return 0;
#endif
        return len;
    }
    if (len > 0)
        return len;
    else
        return -1;
}

void SysUtil::SysSleep(long ms)
{

#ifdef WIN32
    Sleep(ms);
#else
    //	unix/linux代码
    timespec localTimeSpec;
    timespec localLeftSpec;
    localTimeSpec.tv_sec = ms / 1000;
    localTimeSpec.tv_nsec = (ms % 1000)*1000000;
    nanosleep(&localTimeSpec, &localLeftSpec);
#endif

}

int SysUtil::ConnectSocket(int nSocket, unsigned long serverip, int port)
{
    struct sockaddr_in ServerAddress;
    ServerAddress.sin_family = AF_INET;
    ServerAddress.sin_addr.s_addr = serverip;
    ServerAddress.sin_port = ntohs(port);
#ifdef WIN32
    int rc = connect((SOCKET) nSocket, (sockaddr *) & ServerAddress, sizeof (sockaddr_in));
#else
    int rc = connect(nSocket, (sockaddr *) & ServerAddress, sizeof (sockaddr_in));
#endif

    if (rc >= 0)
        return 0;
    return -1;
}

int SysUtil::InitNetLib()
{
#ifdef WIN32
    WSADATA wsd;
    int nRet = ::WSAStartup(MAKEWORD(2, 2), &wsd);
    if (nRet != 0)
    {
        return -1;
    }
#else
#endif
    return 0;
}

int SysUtil::ReleaseNetLib()
{
#ifdef WIN32
    ::WSACleanup();
#else
#endif

    return 0;
}

int SysUtil::ReceiveTimer(int fd)
{
    int iRet = 0;
    fd_set rset;
    struct timeval tv;

    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    tv.tv_sec = 0;
    tv.tv_usec = 500 * 1000;

    iRet = select(fd + 1, &rset, NULL, NULL, &tv);
    return iRet;
}

int SysUtil::SearchHeadPos(char *chBuffer, int nDataLen)
{

    int end, i, j;
    end = nDataLen - 8; /* 计算结束位置*/

    if (end > 0)
    {
        for (i = 0; i <= end; i++)
        {
            //循环比较
            for (j = i; chBuffer[j] == SYS_NET_MSGHEAD[j - i]; j++)
            {
                if (SYS_NET_MSGHEAD[j - i + 1] == '\0')
                {
                    return i; /* 找到了子字符串   */
                }
            }
        }
    }

    return -1;
}

int SysUtil::SearchTailPos(char *chBuffer, int nDataLen)
{
    int end, i, j;
    end = nDataLen - 8; /* 计算结束位置*/

    if (end > 0)
    {
        for (i = 0; i <= end; i++)
        {
            //循环比较
            for (j = i; chBuffer[j] == SYS_NET_MSGTAIL[j - i]; j++)
            {
                if (SYS_NET_MSGTAIL[j - i + 1] == '\0')
                {
                    return i; /* 找到了子字符串   */
                }
            }
        }
    }

    return -1;

}
