#include "includes.h"
#include "config.h"
#include "ic_card.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include "common.h"
#include <syslog.h>
#include "SysUtil.h"

const int max_len = 6;
const char IC_RECV[max_len+3] = {0x40, 0x07, 0x00, 0x00, 0x08, 0x04, 0x00, 0x08, 0x04};

int ic_card_physics_read(card_block_t *p_block)
{
	int send_len = 0;
    char ic_number[32] = {0};
    char pNewBuffer[64] = {0};  
    
    int nNewSocket = SysUtil::CreateSocket();
    if (nNewSocket == -1)
    {
        syslog(LOG_DEBUG, "Can't create socket\n");
        sleep(3);
        nNewSocket = SysUtil::CreateSocket();
        if(nNewSocket == -1)
        {
            return -1;
        }		
    }
        
    struct timeval timeout = {3, 0};
    socklen_t len = sizeof(timeout);
    setsockopt(nNewSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    setsockopt(nNewSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);
    
    if (SysUtil::ConnectSocket(nNewSocket, gSetting_system.com_ip, gSetting_system.com_port) != 0)
    {
        SysUtil::CloseSocket(nNewSocket);
        syslog(LOG_DEBUG, "connect to com server %s:%d fail...\n", gSetting_system.com_ip, gSetting_system.com_port);
        return -1;
    }

    memcpy(pNewBuffer,"\x40\x07\x00\x01\x00\x00\x00\x0D",8);
    send_len = SysUtil::SocketWrite(nNewSocket, pNewBuffer, 8, 500);
    if (send_len != 8)
    {
        syslog(LOG_DEBUG, "send data to com server %s:%d fail...\n", gSetting_system.com_ip, gSetting_system.com_port);
    }
    
        char szTmpData[5 + 1] = {0};
       std::string strLogData = "Send Data:";
       for (int i = 0; i < 8; ++i)
       {
           memset(szTmpData, 0x00, 5);
           sprintf(szTmpData, "%02X ", pNewBuffer[i]);
           strLogData += szTmpData;
       }
       syslog(LOG_DEBUG, "%s\n", strLogData.c_str());     
    

    int nReadLen = 0;  
    u8 szReadBuffer[64] = {0};
    nReadLen = SysUtil::SocketRead(nNewSocket, szReadBuffer, 512);	
    if(!strncmp((char*)szReadBuffer,IC_RECV,send_len+1) && nReadLen > send_len+1+4 )
    {
        //从第4个字节开始截取
        for (int i = send_len+1; i < send_len+1+4; i++)
        {
            sprintf(ic_number + strlen(ic_number), "%02X", szReadBuffer[i]);
        }
        //syslog(LOG_DEBUG, "****************************read ic card no:%s\n", ic_number);
    }
    else
    {
        //syslog(LOG_DEBUG, "ic read physics error!\n");
        SysUtil::CloseSocket(nNewSocket); 
        return -1;        
    }
    SysUtil::CloseSocket(nNewSocket);
    
        char szTmpData2[5 + 1] = {0};
        std::string strLogData2 = "Recv Data:";
        for (int i = 0; i < nReadLen; ++i)
        {
            memset(szTmpData2, 0x00, 5);
            sprintf(szTmpData2, "%02X ", szReadBuffer[i]);
            strLogData2 += szTmpData2;
        }
        syslog(LOG_DEBUG, "%s\n", strLogData2.c_str());       
    
    
    
    
    p_block->len = strlen(ic_number);
    p_block->system_tick = get_system_ms();
    p_block->data = ic_number;

	return 0;    
}