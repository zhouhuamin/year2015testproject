#include "Barcode.h"
#include "CRC.h"
#include "common.h"
#include "config.h"
#include "includes.h"
#include "packet_format.h"
#include "jzLocker.h"
#include "SysUtil.h"
#include <map>
#include <list>
#include <vector>
#include <algorithm>
#include <iterator>
#include <errno.h>

std::list<structLcdMsg>  g_lcdMsgList;
locker g_lcdShowLock;

int barcode_dev_open(int *p_fd_led)
{
    int fd_com;
    struct sockaddr_in server_addr;
    struct hostent *host;
    int flag;

    host = gethostbyname(gSetting_system.com_ip);
    if ( NULL == host)
    {
        syslog(LOG_DEBUG, "get hostname error!\n");
        return -1;
    }

    fd_com = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == fd_com)
    {
        syslog(LOG_DEBUG, "socket error:%s\a\n!\n", strerror(errno));
        return -2;
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(gSetting_system.com_port);
    server_addr.sin_addr = *((struct in_addr *) host->h_addr);

    if (connect(fd_com, (struct sockaddr *) (&server_addr),
                    sizeof(struct sockaddr_in)) == -1)
    {
        syslog(LOG_DEBUG, "connect eror:%s", strerror(errno));
        close(fd_com);
        return -3;
    }

    flag = fcntl(fd_com, F_GETFL, 0);
    //fcntl(fd_com, F_SETFL, flag | O_NONBLOCK);  
    fcntl(fd_com, F_SETFL, 0);

    *p_fd_led = fd_com;
    return 0;
}

//字节流转换为十六进制字符串  
void ByteToHexStr(unsigned char* source, char* dest, int sourceLen)  
{  
    short i;  
    unsigned char highByte, lowByte;  
  
    for (i = 0; i < sourceLen; i++)  
    {  
        highByte = source[i] >> 4;  
        lowByte = source[i] & 0x0f ;  
  
        highByte += 0x30;  
  
        if (highByte > 0x39)  
                dest[i * 2] = highByte + 0x07;  
        else  
                dest[i * 2] = highByte;  
  
        lowByte += 0x30;  
        if (lowByte > 0x39)  
            dest[i * 2 + 1] = lowByte + 0x07;  
        else  
            dest[i * 2 + 1] = lowByte;  
    }  
    return ;  
}  
  
//字节流转换为十六进制字符串的另一种实现方式  
void Hex2Str(char *sSrc,  char *sDest, int nSrcLen )  
{  
    int  i;  
    char szTmp[3];  
  
    for( i = 0; i < nSrcLen; i++ )  
    {  
        sprintf( szTmp, "%02X", (unsigned char) sSrc[i] );  
        memcpy( &sDest[i * 2], szTmp, 2 );  
    }  
    return ;  
}  
  
//十六进制字符串转换为字节流  
void HexStrToByte(const char* source, unsigned char* dest, int sourceLen)  
{  
    short i;  
    unsigned char highByte, lowByte;  
      
    for (i = 0; i < sourceLen; i += 2)  
    {  
        highByte = toupper(source[i]);  
        lowByte  = toupper(source[i + 1]);  
  
        if (highByte > 0x39)  
            highByte -= 0x37;  
        else  
            highByte -= 0x30;  
  
        if (lowByte > 0x39)  
            lowByte -= 0x37;  
        else  
            lowByte -= 0x30;  
  
        dest[i / 2] = (highByte << 4) | lowByte;  
    }  
    return ;  
}

std::string& trim(std::string &s)   
{  
    if (s.empty())   
    {  
        return s;  
    }  
     s.erase(0,s.find_first_not_of(" "));  
     s.erase(s.find_last_not_of(" ") + 1);  
    return s;  
}