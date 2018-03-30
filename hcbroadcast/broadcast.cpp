#include "broadcast.h"
#include "CRC.h"
#include "common.h"
#include "config.h"
#include "includes.h"
#include "broadcast.h"
#include "packet_format.h"

#include "jzLocker.h"
#include "ProcessBroadcast.h"

#include <stdlib.h>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <iterator>
#include <errno.h>


label_list_t label_list;
std::map<std::string, strutLabelModifiers> g_LabelMap;
locker       			   g_LabelMapLock;
std::vector<structLedMsg>            g_MsgVect;
locker                              g_MsgVectLock;

int DigitToInteger(char c)
{
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}
	if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F')
	{
		return c - 'A' + 10;
	}
	return 0;
}

void ToBytes(std::string strHex, void *pBuff, int nBuffLen)
{
	for (int i = 0; i < (signed)strHex.length() / 2 && i < nBuffLen; i++)
	{
		int a = DigitToInteger(strHex[i * 2]);
		int b = DigitToInteger(strHex[i * 2 + 1]);
		unsigned char c = (unsigned char)(a * 16 + b);
		memcpy((char*)pBuff + i, &c, 1);
	}
}

int broadcast_setvolume(int fd_broadcast, int volume)
{
    syslog(LOG_DEBUG, "REQ:\t<set volume> %d\n", volume);
    
    char data_buf[64] = {0};   
    char index[4] = {0};
    unsigned char vol[4] = {0};
    snprintf(index,4,"%02x",volume);
    ToBytes(index,vol,4);
      
    //7E 04 31 19 2C EF
    data_buf[0] = 0x7E;
    data_buf[1] = 0x04;
    data_buf[2] = 0x31;
    data_buf[3] = vol[0];
    data_buf[4] = data_buf[1] xor data_buf[2] xor data_buf[3];
    data_buf[5] = 0xEF;

    ProcessBroadcast broadcast;
    int nRet = broadcast.SocketWrite(fd_broadcast, data_buf, 6, 100);

    return 0;
}

int broadcast_item_ID(int fd_broadcast, int item_id)
{
    syslog(LOG_DEBUG, "Send:\t<Play> %d.mp3\n", item_id);
    signal(SIGPIPE,SIG_IGN);  
    
    char data_buf[64] = {0}; 
    char index[4] = {0};
    unsigned char vol[4] = {0};
    snprintf(index,4,"%02x",item_id);
    ToBytes(index,vol,4);
    
    //7E 05 41 00 01 45 EF 
    data_buf[0] = 0x7E;
    data_buf[1] = 0x05;
    data_buf[2] = 0x41;
    data_buf[3] = 0x00;
    data_buf[4] = vol[0];
    data_buf[5] = data_buf[1] xor data_buf[2] xor data_buf[3] xor data_buf[4];
    data_buf[6] = 0xEF;
        
    ProcessBroadcast broadcast;    
    int nRet = broadcast.SocketWrite(fd_broadcast, data_buf, 7, 100);
    syslog(LOG_DEBUG, "Send:\t Ret=%d\n", nRet);

    return 0;
}