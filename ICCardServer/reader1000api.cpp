#include "includes.h"
#include "reader1000api.h"
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include "config.h"

#define MAX_PACKET_LEN	32*8+6
#define	PLOYM			4129

//CRC16编码1
unsigned short update_good_crc(unsigned short ch, unsigned short g_crc)
{
	unsigned short i, v, xor_flag, good_crc, poly;
	good_crc = g_crc;
	poly = PLOYM;

	/*
	 Align test bit with leftmost bit of the message u8.
	 */
	v = 0x80;

	for (i = 0; i < 8; i++)
	{
		if (good_crc & 0x8000)
		{
			xor_flag = 1;
		}
		else
		{
			xor_flag = 0;
		}
		good_crc = good_crc << 1;

		if (ch & v)
		{
			/*
			 Append next bit of message to end of CRC if it is not zero.
			 The zero bit placed there by the shift above need not be
			 changed if the next bit of the message is zero.
			 */
			good_crc = good_crc + 1;
		}

		if (xor_flag)
		{
			good_crc = good_crc ^ poly;
		}

		/*
		 Align test bit with next bit of the message u8.
		 */
		v = v >> 1;
	}
	return good_crc;
}

//CRC16编码2
unsigned short augment_message_for_good_crc(unsigned short g_crc)
{
	unsigned short i, xor_flag, good_crc, poly;
	good_crc = g_crc;
	poly = PLOYM;

	for (i = 0; i < 16; i++)
	{
		if (good_crc & 0x8000)
		{
			xor_flag = 1;
		}
		else
		{
			xor_flag = 0;
		}
		good_crc = good_crc << 1;

		if (xor_flag)
		{
			good_crc = good_crc ^ poly;
		}
	}
	return good_crc;
}

//计算CRC16
apiReturn CalculateCRC16(u8 *ComData, int Length, unsigned short *crc)
{
	unsigned short good_crc;
//	void update_good_crc(unsigned short ch, unsigned short good_crc);
//	void augment_message_for_good_crc(unsigned short good_crc);
	int j;
	unsigned short ad;
	good_crc = 0xFFFF;

	for (j = 0; j < Length; j++)
	{
		ad = update_good_crc(ComData[j], good_crc);
		good_crc = ad;
	}
	*crc = augment_message_for_good_crc(good_crc);
	return _OK;

}

//向串口写数据包
BOOL WriteABuffer(int hComm, u8 *lpBuf, u32 dwToWrite)
{
	BOOL fRes;
	int ret;

// Issue write
	ret = write(hComm, lpBuf, dwToWrite);
	if (-1 == ret)
	{
		fRes = FALSE;
	}
	else if (ret == dwToWrite)
	{
		fRes = TRUE;
	}
	else
	{
		fRes = FALSE;
	}

//   CloseHandle(osWrite.hEvent);
	return fRes;
}

//从串口读数据包
int ReadABuffer(int hComm, u8 *lpBuf, u32 dwToRead, int time_out)
{
	int fRes = 0;   // 0:Ok; -1:error;  -2:timeout
	int ret;
	int r_len = 0;

	while (1)
	{
		ret = read(hComm, lpBuf + r_len, dwToRead - r_len);

		if (ret > 0)
		{
			r_len += ret;
			if (r_len >= dwToRead)
			{
				fRes = 0;
				break;
			}
		}
		else if (0 == ret)
		{
			fRes = -9;  //server is closed
//			LOG("server is closed!\n");
//			break;
		}
		else if (ret == -1)
		{
//			fRes = -99;
//			LOG_M("read comm error:%s",strerror(errno));
//			break;
		}
		else
		{
			//LOG_M("reader return %d",ret);
			syslog(LOG_DEBUG, "reader return %d",ret);
		}

		usleep(1000);
		time_out--;
		if (time_out <= 0)
		{
			fRes = -2;
			//LOG("read time out!\n");
			syslog(LOG_DEBUG, "read time out!\n");
			break;
		}
	}

	return fRes;
}

//从串口读数据包
BOOL TK900_ReadABuffer(int hComm, u8 lpBuf[MAX_PACKET_LEN])
{
	int i;
	u8 readpk[MAX_PACKET_LEN];
	u32 start, stop;
	start = get_system_ms();
	for (;;)
	{
		stop = get_system_ms();
		if ((stop - start) > 1000)
		{
			return FALSE;
		}

		if (ReadABuffer(hComm, readpk, 1, 0))
		{
			continue;
		}
		if ((readpk[0] == 0xF0) || (readpk[0] == 0xF4))
		{
			lpBuf[0] = readpk[0];

			if (ReadABuffer(hComm, readpk, 1, 0))
			{
				continue;
			}
			lpBuf[1] = readpk[0];
			if (readpk[0] > MAX_PACKET_LEN)
			{
				continue;
			}
			if (ReadABuffer(hComm, readpk, lpBuf[1], 0))
			{
				continue;
			}
			for (i = 0; i < lpBuf[1]; i++)
			{
				lpBuf[i + 2] = readpk[i];
			}

			return TRUE;
		}
		else
		{
			continue;
		}
	}
}

//reader 数据报文交互
int Packet(int hComm, u8 *cmd, u8 *return_data, int delay)
{
	u8 writepk[MAX_PACKET_LEN];
	u8 readpk[MAX_PACKET_LEN];

	int i;
	int pklen = cmd[0] + 2;
	if (pklen > MAX_PACKET_LEN)
	{
		//LOG("发送包太大");
		syslog(LOG_ERR, "package is too big!\n");
		return -1;
	}

// 命令打包
	writepk[0] = 0x40;
	u8 checksum = 0x40;
	for (i = 0; i < cmd[0]; i++)
	{
		writepk[i + 1] = cmd[i];

		checksum += writepk[i + 1];
	}
	checksum = ~checksum;
	checksum++;
	writepk[i + 1] = checksum;

	tcflush(hComm, TCIOFLUSH);  //flush com queue
	if (!write(hComm, writepk, pklen))
	{
		//LOG("write packet error!\n");
		syslog(LOG_ERR, "write packet error!\n");
		return -2;
	}

	switch (writepk[2])
	{
	case 0x14:
		sleep(2);
		break;
	case 0x17:
		sleep(2);
		break;
	case 0xD8:
		sleep(1);
	case 0x09:
		usleep(200 * 1000);
		break;
	default:
		break;
	}

	int res;
	if (writepk[2] == 0xCE)
	{
		res = TK900_ReadABuffer(hComm, readpk);
		if (!res)
			return 0;

		pklen = readpk[1] + 2;
	}
	else
	{
		//-----------
		do
		{
			//BOOL
			res = ReadABuffer(hComm, readpk, 2, 500);
			if (res == 0)
				break;
			else if (res == -2)   //Time Out
			{
				delay--;
				if (delay <= 0)
				{
					//LOG("read timeout!\n");
					syslog(LOG_DEBUG, "read timeout!\n");
					return -99;
				}
				usleep(1000);
			}
			else if (-99 == res)  //reader is closed
			{
				return -99;
			}
			else
			{
				//LOG_M("\r\nread packet head error(%d)!", res);
				syslog(LOG_DEBUG, "\r\nread packet head error(%d)!", res);
				return -4;
			}
		} while (delay > 0);

		pklen = readpk[1] + 2;
		if (pklen > MAX_PACKET_LEN)
		{
			//LOG("recv packet length is too big!");
			syslog(LOG_ERR, "recv packet length is too big!");
			return -5;
		}

		if (ReadABuffer(hComm, &readpk[2], pklen - 2, 0) < 0)
		{
//			LOG("read packet error!");
			return -6;
		}
		//-----------
	}

	checksum = 0;
	for (i = 0; i < pklen; i++)
		checksum += readpk[i];
	if (checksum != 0)
	{
//		gpMainWnd->ShowInfo("the checksum of recv packet error!", 0);
		return -7;
	}

	int fRes = 0;
	if (readpk[2] == writepk[2])
	{
		switch (readpk[0])
		{
		case 0xF0:
			return_data[0] = 0xF0;
			if (pklen - 4 > 0)
			{
				if (writepk[2] == 0xCF)
				{
					return_data[1] = readpk[1];
					memcpy(&return_data[2], &readpk[3], pklen - 4);
				}
				else
				{
					memcpy(&return_data[1], &readpk[3], pklen - 4);
				}
			}
			fRes = 1;
			break;

		case 0xF4:
			return_data[0] = 0xF4;
			return_data[1] = readpk[3];
			if (writepk[2] == 0xEF)
			{
				memcpy(&return_data[1], &readpk[3], 2);
			}
			fRes = 1;
			break;
		}
	}

	return fRes;
}

//连接读写器
apiReturn ConnectScanner(int *hScanner, char *szPort, int nBaudRate)
{
	int fd_com;
	struct sockaddr_in server_addr;
	struct hostent *host;
	int flag;

	host = gethostbyname(gSetting_system.com_ip);
	if ( NULL == host)
	{
		//LOG("get hostname error!\n");
		syslog(LOG_ERR, "get hostname error!\n");
		return -1;
	}

	fd_com = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == fd_com)
	{
		//LOG("socket error:%s\a\n!\n", strerror(errno));
		syslog(LOG_ERR, "socket error:%s\a\n!\n", strerror(errno));
		return -2;
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(gSetting_system.com_port);
	server_addr.sin_addr = *((struct in_addr *) host->h_addr);

	if (connect(fd_com, (struct sockaddr *) (&server_addr),
			sizeof(struct sockaddr_in)) == -1)
	{
		//LOG_M("connect error:%s", strerror(errno));
		syslog(LOG_ERR, "connect error:%s", strerror(errno));
		close(fd_com);
		return -3;
	}

	flag = fcntl(fd_com, F_GETFL, 0);
	fcntl(fd_com, F_SETFL, flag | O_NONBLOCK);

//	setsockopt(fd_com, SOL_SOCKET, SO_KEEPALIVE, (void*) (&keep_alive),
//			(socklen_t) sizeof(int));
//	setsockopt(fd_com, SOL_TCP, TCP_KEEPIDLE, (void*) (&keep_idle),
//			(socklen_t) sizeof(int));
//	setsockopt(fd_com, SOL_TCP, TCP_KEEPINTVL, (void*) (&keep_interval),
//			(socklen_t) sizeof(int));
//	setsockopt(fd_com, SOL_TCP, TCP_KEEPCNT, (void*) (&keep_count),
//			(socklen_t) sizeof(int));

	*hScanner = fd_com;
	return 0;
}

//断开连接
apiReturn DisconnectScanner(int hScanner)
{
	close(hScanner);

	return _OK;
}

//==============================设备控制命令==============================
//设置波特率
apiReturn SetBaudRate(int hScanner, int nBaudRate, int Address)
{
	int BaudRate[] =
	{ 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
	int i;

	for (i = 0; i < 9; i++)
		if (nBaudRate == BaudRate[i])
			break;
	if (i > 8)
		return _baudrate_error;

	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0x01;	// 命令码
		put[2] = i;		// 波特率码
		put[3] = 0;
	}
	else
	{
		put[0] = 0x04;	// 包数据长度
		put[1] = 0x01;	// 命令码
		put[2] = (u8) Address;
		put[3] = i;		// 波特率码
		put[4] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读取版本号
apiReturn GetReaderVersion(int hScanner, u16 *wHardVer, u16 *wSoftVer,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int ret;

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0x02;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0x02;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}
//	if (!Packet(hScanner, put, get, 1))
//		return _comm_error;

	ret = Packet(hScanner, put, get, 1);
	if (0 >= ret)
	{
		return ret;
	}

	if (get[0] == 0xF4)
		return get[1];

	/*memcpy(wHardVer, &get[1], 2);
	 memcpy(wSoftVer, &get[3], 2);*/
	if (Address != 0)
	{
		*wHardVer = get[2] * 0x100 + get[3];
		*wSoftVer = get[4] * 0x100 + get[5];
	}
	else
	{
		*wHardVer = get[1] * 0x100 + get[2];
		*wSoftVer = get[3] * 0x100 + get[4];
	}

	return _OK;
}

//设置读写器继电器状态
apiReturn SetRelay(int hScanner, int Relay, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0x03;	// 命令码
		put[2] = (u8) Relay;
		put[3] = 0;
	}
	else
	{
		put[0] = 0x04;	// 包数据长度
		put[1] = 0x03;	// 命令码
		put[2] = (u8) Address;
		put[3] = (u8) Relay;
		put[4] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设定输出功率
apiReturn SetOutputPower(int hScanner, int nPower1, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0x04;	// 命令码
		put[2] = (u8) nPower1;
		put[3] = 0;
	}
	else
	{
		put[0] = 0x04;	// 包数据长度
		put[1] = 0x04;	// 命令码
		put[2] = (u8) Address;
		put[3] = (u8) nPower1;
		put[4] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设定工作频率
apiReturn SetFrequency(int hScanner, int Min_Frequency, int Max_Frequency,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x04;	// 包数据长度
		put[1] = 0x05;	// 命令码
		put[2] = (u8) Min_Frequency;
		put[3] = (u8) Max_Frequency;
		put[4] = 0;
	}
	else
	{
		put[0] = 0x05;	// 包数据长度
		put[1] = 0x05;	// 命令码
		put[2] = (u8) Address;
		put[3] = (u8) Min_Frequency;
		put[4] = (u8) Max_Frequency;
		put[5] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读取写器工作参数
apiReturn ReadParam(int hScanner, Reader1000Param * pParam, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0x06;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0x06;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];
	if (Address != 0)
	{
		memcpy(pParam, &get[2], sizeof(Reader1000Param));
	}
	else
	{
		memcpy(pParam, &get[1], sizeof(Reader1000Param));
	}

	return _OK;
}
//设置读写器工作参数
apiReturn WriteParam(int hScanner, Reader1000Param * pParam, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	int n = sizeof(Reader1000Param);
	if (Address == 0)
	{
		put[0] = 2 + n;		// 包数据长度
		put[1] = 0x09;	// 命令码
		memcpy(&put[2], pParam, n);
		put[2 + n] = 0;
	}
	else
	{
		put[0] = 3 + n;		// 包数据长度
		put[1] = 0x09;	// 命令码
		put[2] = (u8) Address;
		memcpy(&put[3], pParam, n);
		put[3 + n] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置读写器出厂参数
apiReturn WriteFactoryParameter(int hScanner, Reader1000Param * pParam)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	int n = sizeof(Reader1000Param);
	put[0] = 2 + n;		// 包数据长度
	put[1] = 0x0C;	// 命令码
	memcpy(&put[2], pParam, n);
	put[2 + n] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读取读写器出厂参数
apiReturn ReadFactoryParameter(int hScanner)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x0D;	// 命令码
	put[2] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//选择天线
apiReturn SetAntenna(int hScanner, int Antenna, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0x0A;	// 命令码
		put[2] = (u8) Antenna;
		put[3] = 0;
	}
	else
	{
		put[0] = 0x04;	// 包数据长度
		put[1] = 0x0A;	// 命令码
		put[2] = (u8) Address;
		put[3] = (u8) Antenna;
		put[4] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//复位读写器
apiReturn Reboot(int hScanner, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0x0E;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0x0E;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//增加名单
apiReturn AddLableID(int hScanner, int listlen, int datalen, u8 * data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 4 + listlen * datalen;		// 包数据长度
	put[1] = 0x13;					// 命令码
	put[2] = (u8) listlen;
	put[3] = (u8) datalen;
	memcpy(&put[4], data, listlen * datalen);
	put[4 + listlen * datalen] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//删除名单
apiReturn DelLableID(int hScanner, int listlen, int datalen, u8 * data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 4 + listlen * datalen;		// 包数据长度
	put[1] = 0x14;					// 命令码
	put[2] = (u8) listlen;
	put[3] = (u8) datalen;
	memcpy(&put[4], data, listlen * datalen);
	put[4 + listlen * datalen] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得名单
apiReturn GetLableID(int hScanner, int startaddr, int listlen, int *relistlen,
		int *taglen, u8 * data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 5;		// 包数据长度
	put[1] = 0x15;					// 命令码
	put[2] = (u8) (startaddr >> 8);
	put[3] = (u8) (startaddr & 0xFF);
	put[4] = (u8) listlen;
	put[5] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	*relistlen = get[1];
	*taglen = get[2];
	memcpy(data, &get[3], get[1] * get[2]);

	return _OK;
}

//获得记录
apiReturn GetRecord(int hScanner, ReaderDate *stime, ReaderDate *etime,
		int startaddr, int listlen, int *relistlen, int *taglen, u8 * data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 17;		// 包数据长度
	put[1] = 0x16;					// 命令码
	memcpy(&put[2], stime, sizeof(ReaderDate));
	memcpy(&put[8], etime, sizeof(ReaderDate));
	put[14] = (u8) (startaddr >> 8);
	put[15] = (u8) (startaddr & 0xFF);
	put[16] = (u8) listlen;
	put[17] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	*relistlen = get[1];
	*taglen = get[2];
	memcpy(data, &get[3], get[1] * get[2]);

	return _OK;
}

//删除全部记录
apiReturn DeleteAllRecord(int hScanner)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x17;	// 命令码
	put[2] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置读写器工作模式
apiReturn SetWorkMode(int hScanner, int mode, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0x0F;	// 命令码
		put[2] = (u8) mode;
		put[3] = 0;
	}
	else
	{
		put[0] = 0x04;	// 包数据长度
		put[1] = 0x0F;	// 命令码
		put[2] = (u8) Address;
		put[3] = (u8) mode;
		put[4] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置标签过滤器
apiReturn SetReportFilter(int hScanner, int ptr, int len, u8 *mask)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (len == 0)
	{
		put[0] = 0x06;	// 包数据长度
		put[1] = 0x18;	// 命令码
		put[2] = (u8) (ptr >> 8);
		put[3] = (u8) ptr;
		put[4] = 0x00;
		put[5] = 0x00;
		put[6] = 0;
	}
	else
	{
		int i, m;
		if (len % 8 == 0)
			m = len / 8;
		else
			m = len / 8 + 1;

		put[0] = 0x06 + m;	// 包数据长度
		put[1] = 0x18;	// 命令码
		put[2] = (u8) (ptr >> 8);
		put[3] = (u8) ptr;
		put[4] = (u8) (len >> 8);
		put[5] = (u8) len;

		for (i = 0; i < m; i++)
		{
			put[6 + i] = mask[i];
		}
		put[6 + i] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得标签过滤器
apiReturn GetReportFilter(int hScanner, int *ptr, int *len, u8 *mask)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int l;

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x19;	// 命令码
	put[2] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (get[1] == 0x04)
	{
		*ptr = 0;
		*len = 0;
		*mask = NULL;
	}
	else
	{
		*ptr = get[1] * 0x100 + get[2];
		*len = get[3] * 0x100 + get[4];
		if (*len % 8 == 0)
		{
			l = *len / 8;
		}
		else
		{
			l = *len / 8 + 1;
		}
		memcpy(mask, &get[5], l);
	}

	return _OK;
}

//==============================ISO-6B数据读写命令==============================
//检测标签存在
apiReturn ISO6B_LabelPresent(int hScanner, int *nCounter, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0xFF;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0xFF;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}
	*nCounter = 0;
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
	{
		*nCounter = (int) get[1];
	}
	else
	{
		*nCounter = (int) get[2];
	}

	return _OK;
}

//开始列出标签ID
apiReturn ISO6B_ListID(int hScanner, u8 *btID, int *nCounter, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int ret;

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0xFE;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0xFE;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}

	*nCounter = 0;
//	if (!Packet(hScanner, put, get, 1))
//		return _comm_error;
	ret = Packet(hScanner, put, get, 1);
	if (0 >= ret)
	{
		return ret;
	}

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
	{
		*nCounter = (int) get[1];
		if (*nCounter <= 8)
			memcpy(btID, &get[2], *nCounter * ID_MAX_SIZE_64BIT);
		else
			memcpy(btID, &get[2], 8 * ID_MAX_SIZE_64BIT);
	}
	else
	{
		*nCounter = (int) get[2];
		if (*nCounter <= 8)
			memcpy(btID, &get[3], *nCounter * ID_MAX_SIZE_64BIT);
		else
			memcpy(btID, &get[3], 8 * ID_MAX_SIZE_64BIT);
	}

	return _OK;
}

//取得列出的标签ID
apiReturn ISO6B_ListIDReport(int hScanner, u8 *btID, int stNum, int nCounter,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x04;	// 包数据长度
		put[1] = 0xFD;	// 命令码
		put[2] = (u8) stNum;
		put[3] = (u8) nCounter;
		put[4] = 0;
	}
	else
	{
		put[0] = 0x05;	// 包数据长度
		put[1] = 0xFD;	// 命令码
		put[2] = (u8) Address;
		put[3] = (u8) stNum;
		put[4] = (u8) nCounter;
		put[5] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
	{
		memcpy(btID, &get[1], nCounter * ID_MAX_SIZE_64BIT);
	}
	else
	{
		memcpy(btID, &get[2], nCounter * ID_MAX_SIZE_64BIT);
	}
	return _OK;
}

//读取ISO6B标签ID号
apiReturn ISO6B_ReadLabelID(int hScanner, u8 *IDBuffer, int *nCounter,
		int Address)
{
	apiReturn res;
	int i, j, k;

	*nCounter = 0;
	res = ISO6B_ListID(hScanner, &IDBuffer[0], nCounter, Address);
	if (res != _OK)
		return res;

	if (*nCounter > 8)
	{
		j = *nCounter / 8;
		k = *nCounter - j * 8;
		for (i = 1; i < j; i++)
		{
			res = ISO6B_ListIDReport(hScanner,
					&IDBuffer[i * ID_MAX_SIZE_64BIT * 8], i * 8, 8, Address);
			if (res != _OK)
				return res;
		}
		if (k != 0)
		{
			res = ISO6B_ListIDReport(hScanner,
					&IDBuffer[j * ID_MAX_SIZE_64BIT * 8], j * 8, k, Address);
			if (res != _OK)
				return res;
		}
	}

//	ISO6B_StopListID(hScanner);

	return _OK;
}

//列出选定标签
apiReturn ISO6B_ListSelectedID(int hScanner, int Cmd, int ptr, u8 Mask,
		u8 *Data, u8 *IDBuffer, int *nCounter, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	apiReturn res;
	int i, j, k;

	if (Address == 0)
	{
		put[0] = 0x0D;	// 包数据长度
		put[1] = 0xFB;	// 命令码
		put[2] = (u8) Cmd;
		put[3] = (u8) ptr;
		put[4] = (u8) Mask;
		for (i = 0; i < 8; i++)
		{
			put[5 + i] = Data[i];
		}
		put[13] = 0;
	}
	else
	{
		put[0] = 0x0E;	// 包数据长度
		put[1] = 0xFB;	// 命令码
		put[2] = (u8) Address;
		put[3] = (u8) Cmd;
		put[4] = (u8) ptr;
		put[5] = (u8) Mask;
		for (i = 0; i < 8; i++)
		{
			put[6 + i] = Data[i];
		}
		put[14] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
	{
		*nCounter = (int) get[1];
	}
	else
		*nCounter = (int) get[2];

	if (*nCounter > 8)
	{
		if (Address == 0)
			memcpy(&IDBuffer[0], &get[2], 8 * ID_MAX_SIZE_64BIT);
		else
			memcpy(&IDBuffer[0], &get[3], 8 * ID_MAX_SIZE_64BIT);
		j = *nCounter / 8;
		k = *nCounter - j * 8;
		for (i = 1; i < j; i++)
		{
			res = ISO6B_ListIDReport(hScanner,
					&IDBuffer[i * ID_MAX_SIZE_64BIT * 8], i * 8, 8, Address);
			if (res != _OK)
				return res;
		}
		if (k != 0)
		{
			res = ISO6B_ListIDReport(hScanner,
					&IDBuffer[j * ID_MAX_SIZE_64BIT * 8], j * 8, k, Address);
			if (res != _OK)
				return res;
		}
	}
	else
	{
		if (Address == 0)
			memcpy(IDBuffer, &get[2], *nCounter * ID_MAX_SIZE_64BIT);
		else
			memcpy(IDBuffer, &get[3], *nCounter * ID_MAX_SIZE_64BIT);
	}

//	ISO6B_StopListID(hScanner);
	return _OK;
}

//读一块数据
apiReturn ISO6B_ReadByteBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 len,
		u8 *Data, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x0C;	// 包数据长度
		put[1] = 0xF6;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = ptr;
		put[11] = len;
		put[12] = 0;
	}
	else
	{
		put[0] = 0x0D;	// 包数据长度
		put[1] = 0xF6;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = ptr;
		put[12] = len;
		put[13] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
		memcpy(Data, &get[1], len);
	else
		memcpy(Data, &get[2], len);

	return _OK;
}
/*
 //写一块数据
 apiReturn  ISO6B_WriteByteBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 len, u8 *Data,int Address)
 {
 u8 put[MAX_PACKET_LEN];
 u8 get[MAX_PACKET_LEN];
 int i;
 if(Address==0)
 {
 put[0]=12+len;	// 包数据长度
 put[1]=0xF5;	// 命令码
 for(i=0;i<ID_MAX_SIZE_64BIT;i++)
 {
 put[2+i]=IDBuffer[i];
 }
 put[10]=ptr;
 put[11]=len;
 for(i=0;i<len;i++)
 {
 put[12+i]=Data[i];
 }
 put[12+i]=0;
 }
 else
 {
 put[0]=13+len;	// 包数据长度
 put[1]=0xF5;	// 命令码
 put[2]=(u8)Address;
 for(i=0;i<ID_MAX_SIZE_64BIT;i++)
 {
 put[3+i]=IDBuffer[i];
 }
 put[11]=ptr;
 put[12]=len;
 for(i=0;i<len;i++)
 {
 put[13+i]=Data[i];
 }
 put[13+i]=0;
 }
 if (!Packet(hScanner, put, get, 1))
 return _comm_error;

 if (get[0]==0xF4)
 return get[1];

 return _OK;
 }
 */
//一次写4个字节数据，配合下条指令工作
apiReturn ISO6B_WriteData(int hScanner, u8 *IDBuffer, u8 ptr, u8 len, u8 *Data,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;
	if (Address == 0)
	{
		put[0] = 12 + len;	// 包数据长度
		put[1] = 0xF5;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = ptr;
		put[11] = len;
		for (i = 0; i < len; i++)
		{
			put[12 + i] = Data[i];
		}
		put[12 + i] = 0;
	}
	else
	{
		put[0] = 13 + len;	// 包数据长度
		put[1] = 0xF5;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = ptr;
		put[12] = len;
		for (i = 0; i < len; i++)
		{
			put[13 + i] = Data[i];
		}
		put[13 + i] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//一次写4个字节数据
apiReturn ISO6B_WriteByteBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 len,
		u8 *Data, int Address)
{
	apiReturn result;
	int i_test, i_send, totl;
	int meg = 4;
	if (len <= meg)
	{
		i_test = 0;
		while (i_test < 3)
		{
			result = ISO6B_WriteData(hScanner, IDBuffer, ptr, len, Data,
					Address);
			if (result == _OK)
				break;
			else
				i_test++;
		}
		return result;
	}
	else
	{
		totl = len / meg;
		for (i_send = 0; i_send < totl; i_send++)
		{
			i_test = 0;
			while (i_test < 3)
			{
				result = ISO6B_WriteData(hScanner, IDBuffer, ptr + i_send * meg,
						meg, Data + i_send * meg, Address);
				if (result == _OK)
					break;
				else
					i_test++;
			}
			if (result != _OK)
				return result;
		}
		if ((totl * meg) < len)
		{
			totl = len - totl * meg;
			i_test = 0;
			while (i_test < 3)
			{
				result = ISO6B_WriteData(hScanner, IDBuffer, ptr + i_send * meg,
						totl, Data + i_send * meg, Address);
				if (result == _OK)
					break;
				else
					i_test++;
			}
		}
		return result;
	}
}

//一次写一个字节数据，配合下条指令工作
apiReturn ISO6B_WriteAData(int hScanner, u8 *IDBuffer, u8 ptr, u8 len, u8 *Data,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;
	if (Address == 0)
	{
		put[0] = 12 + len;	// 包数据长度
		put[1] = 0xF2;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = ptr;
		put[11] = len;
		for (i = 0; i < len; i++)
		{
			put[12 + i] = Data[i];
		}
		put[12 + i] = 0;
	}
	else
	{
		put[0] = 13 + len;	// 包数据长度
		put[1] = 0xF2;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = ptr;
		put[12] = len;
		for (i = 0; i < len; i++)
		{
			put[13 + i] = Data[i];
		}
		put[13 + i] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//一次写一个字节数据
apiReturn ISO6B_WriteAByte(int hScanner, u8 *IDBuffer, u8 ptr, u8 len, u8 *Data,
		int Address)
{
	apiReturn result;
	int i_test, i_send, totl;
	int meg = 4;
	if (len <= meg)
	{
		i_test = 0;
		while (i_test < 3)
		{
			result = ISO6B_WriteAData(hScanner, IDBuffer, ptr, len, Data,
					Address);
			if (result == _OK)
				break;
			else
				i_test++;
		}
		return result;
	}
	else
	{
		totl = len / meg;
		for (i_send = 0; i_send < totl; i_send++)
		{
			i_test = 0;
			while (i_test < 3)
			{
				result = ISO6B_WriteAData(hScanner, IDBuffer,
						ptr + i_send * meg, meg, Data + i_send * meg, Address);
				if (result == _OK)
					break;
				else
					i_test++;
			}
			if (result != _OK)
				return result;
		}
		if ((totl * meg) < len)
		{
			totl = len - totl * meg;
			i_test = 0;
			while (i_test < 3)
			{
				result = ISO6B_WriteAData(hScanner, IDBuffer,
						ptr + i_send * meg, totl, Data + i_send * meg, Address);
				if (result == _OK)
					break;
				else
					i_test++;
			}
		}
		return result;
	}
}

//置写保护状态
apiReturn ISO6B_WriteProtect(int hScanner, u8 *IDBuffer, u8 ptr, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x0B;	// 包数据长度
		put[1] = 0xF4;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = ptr;
		put[11] = 0;
	}
	else
	{
		put[0] = 0x0C;	// 包数据长度
		put[1] = 0xF4;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = ptr;
		put[12] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读写保护状态
apiReturn ISO6B_ReadWriteProtect(int hScanner, u8 *IDBuffer, u8 ptr,
		u8 *Protected, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x0B;	// 包数据长度
		put[1] = 0xF3;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = ptr;
		put[11] = 0;
	}
	else
	{
		put[0] = 0x0C;	// 包数据长度
		put[1] = 0xF3;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = ptr;
		put[12] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
		*Protected = get[1];
	else
		*Protected = get[2];

	return _OK;
}

//全部清除
apiReturn ISO6B_ClearMemory(int hScanner, u8 CardType, u8 *IDBuffer,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x0B;	// 包数据长度
		put[1] = 0xF2;	// 命令码
		put[2] = CardType;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = 0;
	}
	else
	{
		put[0] = 0x0C;	// 包数据长度
		put[1] = 0xF2;	// 命令码
		put[2] = (u8) Address;
		put[3] = CardType;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[4 + i] = IDBuffer[i];
		}
		put[12] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//-----------TX Mode-----------
//读取ISO6B标签ID号
apiReturn ISO6B_ReadLabelID_TXMode(int hScanner, u8 *IDBuffer, int *nCounter,
		int Address)
{
	apiReturn res;
	int i, j, k;

	*nCounter = 0;
	res = ISO6B_ListID(hScanner, &IDBuffer[0], nCounter, Address);
	if (res != _OK)
		return res;

	if (*nCounter > 8)
	{
		j = *nCounter / 8;
		k = *nCounter - j * 8;
		for (i = 1; i < j; i++)
		{
			res = ISO6B_ListIDReport(hScanner,
					&IDBuffer[i * ID_MAX_SIZE_64BIT * 8], i * 8, 8, Address);
			if (res != _OK)
				return res;
		}
		if (k != 0)
		{
			res = ISO6B_ListIDReport(hScanner,
					&IDBuffer[j * ID_MAX_SIZE_64BIT * 8], j * 8, k, Address);
			if (res != _OK)
				return res;
		}
	}

	return _OK;
}
//-----------TX Mode-----------

//==============================EPC C1G2数据读写命令==============================
apiReturn EPC1G2_ListTagID(int hScanner, u8 mem, int ptr, u8 len, u8 *mask,
		u8 *btID, int *nCounter, int *IDlen, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i, m, next, last;
	int ret;

	if (len == 0)
		m = 0;
	else
	{
		m = len / 8;
		if (len % 8 != 0)
			m++;
	}
	if (Address == 0)
	{
		put[0] = m + 6;		// 包数据长度
		put[1] = 0xEE;	// 命令码
		put[2] = mem;
		put[3] = (u8) (ptr >> 8);
		put[4] = (u8) (ptr);
		put[5] = len;
		for (i = 0; i < m; i++)
		{
			put[6 + i] = mask[i];
		}
		put[6 + i] = 0;

	}
	else
	{
		put[0] = m + 7;		// 包数据长度
		put[1] = 0xEE;	// 命令码
		put[2] = (u8) Address;
		put[3] = mem;
		put[4] = (u8) (ptr >> 8);
		put[5] = (u8) (ptr);
		put[6] = len;
		for (i = 0; i < m; i++)
		{
			put[7 + i] = mask[i];
		}
		put[7 + i] = 0;
	}

	ret = Packet(hScanner, put, get, 1);
	if (0 >= ret)
	{
		return ret;
	}

//	if (!Packet(hScanner, put, get, 1))
//		return _comm_error;

	if (get[0] == 0xF4) //error
	{
		*nCounter = 0;
		return get[1];
	}

	if (Address == 0)
	{
		*nCounter = (int) get[1];
		last = 2;
	}
	else
	{
		*nCounter = (int) get[2];
		last = 3;
	}
//**++**++**++**++**++**++**++**++**++**++**++**++**++**++**++**++**++**++**++**++**++**++**//
	if (*nCounter <= 8)
	{
		for (i = 0; i < *nCounter; i++)
		{
			next = get[last] * 2 + 1;	//1word=16bit
			if (Address == 0)
				memcpy(&btID[last - 2], &get[last], next);
			else
				memcpy(&btID[last - 3], &get[last], next);
			last += next;
		}
	}
	else
	{
		for (i = 0; i < 8; i++)
		{
			next = get[last] * 2 + 1;	//1word=16bit
			if (Address == 0)
				memcpy(&btID[last - 2], &get[last], next);
			else
				memcpy(&btID[last - 3], &get[last], next);
			last += next;
		}
	}

	*IDlen = last - 2;

	return _OK;
}

apiReturn EPC1G2_GetIDList(int hScanner, u8 *btID, int stNum, int nCounter,
		int *IDlen, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x04;	// 包数据长度
		put[1] = 0xED;	// 命令码
		put[2] = (u8) stNum;
		put[3] = (u8) nCounter;
		put[4] = 0;
	}
	else
	{
		put[0] = 0x05;	// 包数据长度
		put[1] = 0xED;	// 命令码
		put[2] = (u8) Address;
		put[3] = (u8) stNum;
		put[4] = (u8) nCounter;
		put[5] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	int i, next, last;
//--++==**--++==**--++==**--++==**--++==**--++==**--++==**--++==**--++==**--++==**--++==**//
	if (Address == 0)
		last = 2;
	else
		last = 3;

	for (i = 0; i < nCounter; i++)
	{
		next = get[last] * 2 + 1;	//1word=16bit
		memcpy(&btID[last - 2], &get[last], next);
		last += next;
	}

	*IDlen = last - 2;

	return _OK;
}

//读取EPC1G2标签ID号
apiReturn EPC1G2_ReadLabelID(int hScanner, u8 mem, int ptr, u8 len, u8 *mask,
		u8 *IDBuffer, int *nCounter, int Address)
{
	apiReturn res;
	int i, j, k, l, IDlen;

	*nCounter = 0;
	res = EPC1G2_ListTagID(hScanner, mem, ptr, len, mask, &IDBuffer[0],
			nCounter, &IDlen, Address);
	if (res != _OK)
		return res;

	if (*nCounter > 8)
	{
		j = *nCounter / 8;
		k = *nCounter - j * 8;
		for (i = 1; i < j; i++)
		{
			res = EPC1G2_GetIDList(hScanner, &IDBuffer[IDlen], i * 8, 8, &l,
					Address);
			if (res != _OK)
				return res;
			IDlen += l;
		}
		if (k != 0)
		{
			res = EPC1G2_GetIDList(hScanner, &IDBuffer[IDlen], j * 8, k, &l,
					Address);
			if (res != _OK)
				return res;
		}
	}

	return _OK;
}

//读一块数据
apiReturn EPC1G2_ReadWordBlock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 mem,
		int ptr, u8 len, u8 *Data, u8 *AccessPassword, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = EPC_WORD * 2 + 10;	// 包数据长度
		put[1] = 0xEC;			// 命令码
		put[2] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 3] = mem;
		if (ptr <= 127)
		{
			put[EPC_WORD * 2 + 4] = ptr;
			put[EPC_WORD * 2 + 5] = len;
			for (i = 0; i < 4; i++)
			{
				put[EPC_WORD * 2 + 6 + i] = AccessPassword[i];
			}
			put[EPC_WORD * 2 + 6 + i] = 0;
		}
		else
		{
			put[0] = EPC_WORD * 2 + 12;	// 包数据长度
			put[EPC_WORD * 2 + 4] = 0xC0;
			put[EPC_WORD * 2 + 5] = (u8) (ptr >> 8);
			put[EPC_WORD * 2 + 6] = (u8) (ptr);
			put[EPC_WORD * 2 + 7] = len;
			for (i = 0; i < 4; i++)
			{
				put[EPC_WORD * 2 + 8 + i] = AccessPassword[i];
			}
			put[EPC_WORD * 2 + 8 + i] = 0;
		}
	}
	else
	{
		put[0] = EPC_WORD * 2 + 11;	// 包数据长度
		put[1] = 0xEC;			// 命令码
		put[2] = (u8) Address;
		put[3] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[4 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 4] = mem;
		if (ptr <= 127)
		{
			put[EPC_WORD * 2 + 5] = ptr;
			put[EPC_WORD * 2 + 6] = len;
			for (i = 0; i < 4; i++)
			{
				put[EPC_WORD * 2 + 7 + i] = AccessPassword[i];
			}
			put[EPC_WORD * 2 + 7 + i] = 0;
		}
		else
		{
			put[0] = EPC_WORD * 2 + 13;	// 包数据长度
			put[EPC_WORD * 2 + 5] = 0xC0;
			put[EPC_WORD * 2 + 6] = (u8) (ptr >> 8);
			put[EPC_WORD * 2 + 7] = (u8) (ptr);
			put[EPC_WORD * 2 + 8] = len;
			for (i = 0; i < 4; i++)
			{
				put[EPC_WORD * 2 + 9 + i] = AccessPassword[i];
			}
			put[EPC_WORD * 2 + 9 + i] = 0;
		}
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
		memcpy(Data, &get[1], len * 2);
	else
		memcpy(Data, &get[2], len * 2);

	return _OK;
}

//识别EPC同时读数据
apiReturn EPC1G2_ReadEPCandData(int hScanner, u8 *EPC_WORD, u8 *IDBuffer,
		u8 mem, u8 ptr, u8 len, u8 *Data, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 5;	// 包数据长度
		put[1] = 0xE0;			// 命令码
		put[2] = mem;
		put[3] = ptr;
		put[4] = len;
		put[5] = 0;
	}
	else
	{
		put[0] = 6;	// 包数据长度
		put[1] = 0xE0;			// 命令码
		put[2] = (u8) Address;
		put[3] = mem;
		put[4] = ptr;
		put[5] = len;
		put[6] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
	{
		*EPC_WORD = get[1];
		memcpy(IDBuffer, &get[2], get[1] * 2);
		memcpy(Data, &get[2 + get[1] * 2], len * 2);
	}
	else
	{
		*EPC_WORD = get[2];
		memcpy(IDBuffer, &get[3], get[2] * 2);
		memcpy(Data, &get[3 + get[2] * 2], len * 2);
	}

	return _OK;

}

//写一块数据
apiReturn EPC1G2_WriteWordBlock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 mem,
		int ptr, u8 len, u8 *Data, u8 *AccessPassword, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = EPC_WORD * 2 + len * 2 + 10;	// 包数据长度
		put[1] = 0xEB;				// 命令码
		put[2] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 3] = mem;
		if (ptr <= 127)
		{
			put[EPC_WORD * 2 + 4] = ptr;
			put[EPC_WORD * 2 + 5] = len;
			for (i = 0; i < len * 2; i++)
			{
				put[EPC_WORD * 2 + 6 + i] = Data[i];
			}
			for (i = 0; i < 4; i++)
			{
				put[EPC_WORD * 2 + len * 2 + 6 + i] = AccessPassword[i];
			}
			put[EPC_WORD * 2 + len * 2 + 6 + i] = 0;
		}
		else
		{
			put[0] = EPC_WORD * 2 + len * 2 + 12;	// 包数据长度
			put[EPC_WORD * 2 + 4] = 0xC0;
			put[EPC_WORD * 2 + 5] = (u8) (ptr >> 8);
			put[EPC_WORD * 2 + 6] = (u8) (ptr);
			put[EPC_WORD * 2 + 7] = len;
			for (i = 0; i < len * 2; i++)
			{
				put[EPC_WORD * 2 + 8 + i] = Data[i];
			}
			for (i = 0; i < 4; i++)
			{
				put[EPC_WORD * 2 + len * 2 + 8 + i] = AccessPassword[i];
			}
			put[EPC_WORD * 2 + len * 2 + 8 + i] = 0;
		}
	}
	else
	{
		put[0] = EPC_WORD * 2 + len * 2 + 11;	// 包数据长度
		put[1] = 0xEB;				// 命令码
		put[2] = (u8) Address;
		put[3] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[4 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 4] = mem;
		if (ptr <= 127)
		{
			put[EPC_WORD * 2 + 5] = ptr;
			put[EPC_WORD * 2 + 6] = len;
			for (i = 0; i < len * 2; i++)
			{
				put[EPC_WORD * 2 + 7 + i] = Data[i];
			}
			for (i = 0; i < 4; i++)
			{
				put[EPC_WORD * 2 + len * 2 + 7 + i] = AccessPassword[i];
			}
			put[EPC_WORD * 2 + len * 2 + 7 + i] = 0;
		}
		else
		{
			put[0] = EPC_WORD * 2 + len * 2 + 13;	// 包数据长度
			put[EPC_WORD * 2 + 5] = 0xC0;
			put[EPC_WORD * 2 + 6] = (u8) (ptr >> 8);
			put[EPC_WORD * 2 + 7] = (u8) (ptr);
			put[EPC_WORD * 2 + 8] = len;
			for (i = 0; i < len * 2; i++)
			{
				put[EPC_WORD * 2 + 9 + i] = Data[i];
			}
			for (i = 0; i < 4; i++)
			{
				put[EPC_WORD * 2 + len * 2 + 9 + i] = AccessPassword[i];
			}
			put[EPC_WORD * 2 + len * 2 + 9 + i] = 0;
		}
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置读写保护状态
apiReturn EPC1G2_SetLock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 mem,
		u8 Lock, u8 *AccessPassword, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = EPC_WORD * 2 + 9;	// 包数据长度
		put[1] = 0xEA;			// 命令码
		put[2] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 3] = mem;
		put[EPC_WORD * 2 + 4] = Lock;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 5 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 9] = 0;
	}
	else
	{
		put[0] = EPC_WORD * 2 + 10;	// 包数据长度
		put[1] = 0xEA;			// 命令码
		put[2] = (u8) Address;
		put[3] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[4 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 4] = mem;
		put[EPC_WORD * 2 + 5] = Lock;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 6 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 10] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//擦除标签数据
apiReturn EPC1G2_EraseBlock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 mem,
		u8 ptr, u8 len, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = EPC_WORD * 2 + 6;	// 包数据长度
		put[1] = 0xE9;			// 命令码
		put[2] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 3] = mem;
		put[EPC_WORD * 2 + 4] = ptr;
		put[EPC_WORD * 2 + 5] = len;
		put[EPC_WORD * 2 + 6] = 0;
	}
	else
	{
		put[0] = EPC_WORD * 2 + 7;	// 包数据长度
		put[1] = 0xE9;			// 命令码
		put[2] = (u8) Address;
		put[3] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[4 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 4] = mem;
		put[EPC_WORD * 2 + 5] = ptr;
		put[EPC_WORD * 2 + 6] = len;
		put[EPC_WORD * 2 + 7] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//永久休眠标签
apiReturn EPC1G2_KillTag(int hScanner, u8 EPC_WORD, u8 *IDBuffer,
		u8 *KillPassword, int recom, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = EPC_WORD * 2 + 8;	// 包数据长度
		put[1] = 0xE8;			// 命令码
		put[2] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 3 + i] = KillPassword[i];
		}
		put[EPC_WORD * 2 + 3 + i] = (u8) recom;
		put[EPC_WORD * 2 + 4 + i] = 0;
	}
	else
	{
		put[0] = EPC_WORD * 2 + 9;	// 包数据长度
		put[1] = 0xE8;			// 命令码
		put[2] = (u8) Address;
		put[3] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[4 + i] = IDBuffer[i];
		}
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 4 + i] = KillPassword[i];
		}
		put[EPC_WORD * 2 + 4 + i] = (u8) recom;
		put[EPC_WORD * 2 + 5 + i] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//写EPC
apiReturn EPC1G2_WriteEPC(int hScanner, u8 len, u8 *Data, u8 *AccessPassword,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 7 + len * 2;			// 包数据长度
		put[1] = 0xE7;	// 命令码
		put[2] = len;
		for (i = 0; i < len * 2; i++)
		{
			put[3 + i] = Data[i];
		}
		for (i = 0; i < 4; i++)
		{
			put[len * 2 + 3 + i] = AccessPassword[i];
		}
		put[len * 2 + 3 + i] = 0;
	}
	else
	{
		put[0] = 8 + len * 2;	// 包数据长度
		put[1] = 0xE7;	// 命令码
		put[2] = (u8) Address;
		put[3] = len;
		for (i = 0; i < len * 2; i++)
		{
			put[4 + i] = Data[i];
		}
		for (i = 0; i < 4; i++)
		{
			put[len * 2 + 4 + i] = AccessPassword[i];
		}
		put[len * 2 + 4 + i] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//块锁命令
apiReturn EPC1G2_BlockLock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 ptr,
		u8 *AccessPassword, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = EPC_WORD * 2 + 8;	// 包数据长度
		put[1] = 0xE6;			// 命令码
		put[2] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[3 + i] = ptr;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 4 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 4 + i] = 0;
	}
	else
	{
		put[0] = EPC_WORD * 2 + 9;	// 包数据长度
		put[1] = 0xE6;			// 命令码
		put[2] = (u8) Address;
		put[3] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[4 + i] = IDBuffer[i];
		}
		put[4 + i] = ptr;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 5 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 5 + i] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//EAS状态操作命令
apiReturn EPC1G2_ChangeEas(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 State,
		u8 *AccessPassword, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = EPC_WORD * 2 + 8;	// 包数据长度
		put[1] = 0xE5;			// 命令码
		put[2] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[3 + i] = State;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 4 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 4 + i] = 0;
	}
	else
	{
		put[0] = EPC_WORD * 2 + 9;	// 包数据长度
		put[1] = 0xE5;			// 命令码
		put[2] = (u8) Address;
		put[3] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[4 + i] = IDBuffer[i];
		}
		put[4 + i] = State;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 5 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 5 + i] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//EAS报警命令
apiReturn EPC1G2_EasAlarm(int hScanner, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0xE4;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0xE4;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读保护设置
apiReturn EPC1G2_ReadProtect(int hScanner, u8 *AccessPassword, u8 EPC_WORD,
		u8 *IDBuffer, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = EPC_WORD * 2 + 7;	// 包数据长度
		put[1] = 0xE3;			// 命令码
		for (i = 0; i < 4; i++)
		{
			put[2 + i] = AccessPassword[i];
		}
		put[2 + i] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[7 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 7] = 0;
	}
	else
	{
		put[0] = EPC_WORD * 2 + 8;	// 包数据长度
		put[1] = 0xE3;			// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < 4; i++)
		{
			put[3 + i] = AccessPassword[i];
		}
		put[3 + i] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[8 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 8] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//复位读保护设置
apiReturn EPC1G2_RStreadProtect(int hScanner, u8 *AccessPassword, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x06; //数据长度
		put[1] = 0xE2; //命令码
		for (i = 0; i < 4; i++)
		{
			put[2 + i] = AccessPassword[i];
		}
		put[6] = 0;
	}
	else
	{
		put[0] = 0x07; //数据长度
		put[1] = 0xE2; //命令码
		put[2] = (u8) Address;
		for (i = 0; i < 4; i++)
		{
			put[3 + i] = AccessPassword[i];
		}
		put[7] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置用户区数据块读保护
apiReturn EPC1G2_BlockReadLock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 Lock,
		u8 *AccessPassword, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = EPC_WORD * 2 + 8;	// 包数据长度
		put[1] = 0xE1;			// 命令码
		put[2] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 3] = Lock;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 4 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 8] = 0;
	}
	else
	{
		put[0] = EPC_WORD * 2 + 10;	// 包数据长度
		put[1] = 0xE1;			// 命令码
		put[2] = (u8) Address;
		put[3] = EPC_WORD;
		for (i = 0; i < EPC_WORD * 2; i++)
		{
			put[4 + i] = IDBuffer[i];
		}
		put[EPC_WORD * 2 + 4] = Lock;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 5 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 9] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//频谱校验
apiReturn EPC1G2_Calibrate(int hScanner, u8 *AccessPassword, u8 Kword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x07; //数据长度
	put[1] = 0x8A; //命令码
	for (i = 0; i < 4; i++)
	{
		put[2 + i] = AccessPassword[i];
	}
	put[2 + i] = Kword;
	put[7] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//侦测标签
apiReturn EPC1G2_DetectTag(int hScanner, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x02;
		put[1] = 0xEF;
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;
		put[1] = 0xEF;
		put[2] = (u8) Address;
		put[3] = 0;
	}

	if (!Packet(hScanner, put, get, 3))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//==============================TK900数据读写命令==============================
//开始列出标签ID
apiReturn TK900_ListID(int hScanner, u8 *btID, int *nCounter, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0xCE;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0xCE;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
	{
		*nCounter = (int) get[1];
		if (*nCounter <= 8)
			memcpy(btID, &get[2], *nCounter * ID_MAX_SIZE_64BIT);
		else
			memcpy(btID, &get[2], 8 * ID_MAX_SIZE_64BIT);
	}
	else
	{
		*nCounter = (int) get[2];
		if (*nCounter <= 8)
			memcpy(btID, &get[3], *nCounter * ID_MAX_SIZE_64BIT);
		else
			memcpy(btID, &get[3], 8 * ID_MAX_SIZE_64BIT);
	}

	return _OK;
}

//取得列出的标签ID
apiReturn TK900_ListIDReport(int hScanner, u8 *btID, int stNum, int nCounter,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x04;	// 包数据长度
		put[1] = 0xCD;	// 命令码
		put[2] = (u8) stNum;
		put[3] = (u8) nCounter;
		put[4] = 0;
	}
	else
	{
		put[0] = 0x05;	// 包数据长度
		put[1] = 0xCD;	// 命令码
		put[2] = (u8) Address;
		put[3] = (u8) stNum;
		put[4] = (u8) nCounter;
		put[5] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
		memcpy(btID, &get[1], nCounter * ID_MAX_SIZE_64BIT);
	else
		memcpy(btID, &get[2], nCounter * ID_MAX_SIZE_64BIT);

	return _OK;
}

//TX读卡
apiReturn TK900_TXListID(int hScanner, int Address)
{
	u8 put[MAX_PACKET_LEN];

	put[0] = 0x40;
	put[1] = 0x02;	// 包数据长度
	put[2] = 0xCE;	// 命令码
	put[3] = 0xF0;

	if (!WriteABuffer(hScanner, put, 4))
		return _comm_error;

	return _OK;
}

//停止TX读卡
apiReturn TK900_StopTXListID(int hScanner, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0xCF;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0xCF;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读取TK900标签ID号
apiReturn TK900_ReadLabelID(int hScanner, u8 *IDBuffer, int *nCounter,
		int Address)
{
	apiReturn res;
	int i, j, k;

	*nCounter = 0;
	res = TK900_ListID(hScanner, &IDBuffer[0], nCounter, Address);
	if (res != _OK)
		return res;

	if (*nCounter > 8)
	{
		j = *nCounter / 8;
		k = *nCounter - j * 8;
		for (i = 1; i < j; i++)
		{
			res = TK900_ListIDReport(hScanner,
					&IDBuffer[i * ID_MAX_SIZE_64BIT * 8], i * 8, 8, Address);
			if (res != _OK)
				return res;
		}
		if (k != 0)
		{
			res = TK900_ListIDReport(hScanner,
					&IDBuffer[j * ID_MAX_SIZE_64BIT * 8], j * 8, k, Address);
			if (res != _OK)
				return res;
		}
	}

	return _OK;
}

//读一块数据
apiReturn TK900_ReadPageBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 len,
		u8 *Data, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x0C;	// 包数据长度
		put[1] = 0xCC;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = ptr;
		put[11] = len;
		put[12] = 0;
	}
	else
	{
		put[0] = 0x0D;	// 包数据长度
		put[1] = 0xCC;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = ptr;
		put[12] = len;
		put[13] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
		memcpy(Data, &get[1], len * 8);
	else
		memcpy(Data, &get[2], len * 8);

	return _OK;
}

//写一块数据
apiReturn TK900_WritePageBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 *Data,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x13;	// 包数据长度
		put[1] = 0xCB;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = ptr;
		for (i = 0; i < 8; i++)
		{
			put[11 + i] = Data[i];
		}
		put[11 + i] = 0;
	}
	else
	{
		put[0] = 0x14;	// 包数据长度
		put[1] = 0xCB;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = ptr;
		for (i = 0; i < 8; i++)
		{
			put[12 + i] = Data[i];
		}
		put[12 + i] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//置写保护状态
apiReturn TK900_SetProtect(int hScanner, u8 *IDBuffer, u8 ptr, u8 len,
		int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x0C;	// 包数据长度
		put[1] = 0xCA;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = ptr;
		put[11] = len;
		put[12] = 0;
	}
	else
	{
		put[0] = 0x0D;	// 包数据长度
		put[1] = 0xCA;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = ptr;
		put[12] = len;
		put[13] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读写保护状态
apiReturn TK900_GetProtect(int hScanner, u8 *IDBuffer, u8 *state, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x0C;	// 包数据长度
		put[1] = 0xC9;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = 15;
		put[11] = 1;
		put[12] = 0;
	}
	else
	{
		put[0] = 0x0D;	// 包数据长度
		put[1] = 0xC9;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = 15;
		put[12] = 1;
		put[13] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
		memcpy(state, &get[1], 2);
	else
		memcpy(state, &get[2], 2);
	return _OK;
}

//设置标签进入TTO状态
apiReturn TK900_SetTTO(int hScanner, u8 *IDBuffer, u8 ptr, u8 len, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x0C;	// 包数据长度
		put[1] = 0xC8;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = ptr;
		put[11] = len;
		put[12] = 0;
	}
	else
	{
		put[0] = 0x0D;	// 包数据长度
		put[1] = 0xC8;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = ptr;
		put[12] = len;
		put[13] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读取TTO标签
apiReturn TK900_ListTagPage(int hScanner, u8 *IDBuffer, u8 *Data, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0xCF;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0xCF;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}
	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];
	if (Address == 0)
	{
		memcpy(IDBuffer, &get[1], ID_MAX_SIZE_64BIT + 1);
		memcpy(Data, &get[10], get[1] - 10);
	}
	else
	{
		memcpy(IDBuffer, &get[2], ID_MAX_SIZE_64BIT + 1);
		memcpy(Data, &get[11], get[1] - 10);
	}

	return _OK;

}

//得到TTO标签的开始地址和长度
apiReturn TK900_GetTTOStartAdd(int hScanner, u8 *IDBuffer, u8 *len, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	if (Address == 0)
	{
		put[0] = 0x0A;	// 包数据长度
		put[1] = 0xC2;	// 命令码
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[2 + i] = IDBuffer[i];
		}
		put[10] = 0;
	}
	else
	{
		put[0] = 0x0B;	// 包数据长度
		put[1] = 0xC2;	// 命令码
		put[2] = (u8) Address;
		for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
		{
			put[3 + i] = IDBuffer[i];
		}
		put[11] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
		memcpy(len, &get[1], 2);
	else
		memcpy(len, &get[2], 2);

	return _OK;
}

////TK900串口写入
//BOOL TK900_WriteABuffer(int hComm, u8 *lpBuf, u32 dwToWrite)
//{
//	u32 dwNumBytesWritten;
//	u32 dwHaveNumWritten =0 ; //已经写入多少
//
//	SetCommMask (hComm, EV_TXEMPTY);
//
//	do
//	{
//		if (WriteFile (hComm,					//串口句柄
//			lpBuf,				                //被写数据缓冲区
//			dwToWrite,                          //被写数据缓冲区大小
//			&dwNumBytesWritten,					//函数执行成功后，返回实际向串口写的个数
//			NULL))								//此处必须设置NULL
//		{
//			dwHaveNumWritten = dwNumBytesWritten;
//
//			//写入完成
//			if (dwHaveNumWritten == dwToWrite)
//			{
//				break;
//			}
//			Sleep(10);
//		}
//		else
//		{
//			return FALSE;
//		}
//	}while (TRUE);
//
//	return TRUE;
//}

//获得时间
apiReturn GetReaderTime(int hScanner, ReaderDate *time, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x02;	// 包数据长度
		put[1] = 0x12;	// 命令码
		put[2] = 0;
	}
	else
	{
		put[0] = 0x03;	// 包数据长度
		put[1] = 0x12;	// 命令码
		put[2] = (u8) Address;
		put[3] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	if (Address == 0)
		memcpy(time, &get[1], sizeof(ReaderDate));
	else
		memcpy(time, &get[2], sizeof(ReaderDate));

	return _OK;
}

//设置时间
apiReturn SetReaderTime(int hScanner, ReaderDate time, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (Address == 0)
	{
		put[0] = 0x08;	// 包数据长度
		put[1] = 0x11;	// 命令码
		put[2] = time.Year;
		put[3] = time.Month;
		put[4] = time.Day;
		put[5] = time.Hour;
		put[6] = time.Minute;
		put[7] = time.Second;
		put[8] = 0;
	}
	else
	{
		put[0] = 0x09;	// 包数据长度
		put[1] = 0x11;	// 命令码
		put[2] = (u8) Address;
		put[3] = time.Year;
		put[4] = time.Month;
		put[5] = time.Day;
		put[6] = time.Hour;
		put[7] = time.Minute;
		put[8] = time.Second;
		put[9] = 0;
	}

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//==============================网络命令==============================
//设置读写器网络地址
apiReturn SetReaderNetwork(int hScanner, u8 IP_Address[4], int Port, u8 Mask[4],
		u8 Gateway[4])
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x10;	// 包数据长度
	put[1] = 0x30;	// 命令码
	for (i = 0; i < 4; i++)
	{
		put[2 + i] = IP_Address[i];
		put[8 + i] = Mask[i];
		put[12 + i] = Gateway[i];
	}
	put[6] = (u8) (Port >> 8);
	put[7] = (u8) Port;
	put[16] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得读写器网络地址
apiReturn GetReaderNetwork(int hScanner, u8 *IP_Address, int *Port, u8 *Mask,
		u8 *Gateway)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x31;	// 命令码
	put[2] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(IP_Address, &get[1], 4);
	*Port = (((int) get[5]) << 8) + (int) get[6];
	memcpy(Mask, &get[7], 4);
	memcpy(Gateway, &get[11], 4);

	return _OK;
}

//设置读写器网络MAC
apiReturn SetReaderMAC(int hScanner, u8 MAC[6])
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x08;	// 包数据长度
	put[1] = 0x32;	// 命令码
	for (i = 0; i < 6; i++)
	{
		put[2 + i] = MAC[i];
	}
	put[2 + i] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得读写器网络MAC
apiReturn GetReaderMAC(int hScanner, u8 *MAC)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x33;	// 命令码
	put[2] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(MAC, &get[1], 6);

	return _OK;
}

//获得读写器ID
apiReturn GetReaderID(int hScanner, u8 *ReaderID)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x8C;	// 命令码
	put[2] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(ReaderID, &get[1], 6);

	return _OK;
}

#define MAX_READER_IP  500
u32 TimeOut;
int m_InitSocket[8] =
{ -1, -1, -1, -1, -1, -1, -1, -1 };
int m_SocketNum = -1;

typedef struct ReaderIPInfo
{
	char ip[16];			//读写器IP地址
	int port;			//读写器IP端口
} ReaderIPdata;

ReaderIPdata m_InforeaderIP[MAX_READER_IP];

BOOL SocketBuffer(int hSocket, u8 *lpPutBuf, u8 *lpGetBuf, int nBufLen)
{
	apiReturn res = 0;
	u32 nBefore, nAfter;
	u32 dwTime = 0;

//recv(m_InitSocket[m_SocketNum],(char*)lpGetBuf,MAX_PACKET_LEN,0);

	struct sockaddr_in SockAddrIn;

	SockAddrIn.sin_family = AF_INET;
	SockAddrIn.sin_addr.s_addr = inet_addr(m_InforeaderIP[m_SocketNum].ip);
	SockAddrIn.sin_port = htons(m_InforeaderIP[m_SocketNum].port);

//	QueryPerformanceFrequency(&nFreq);
	res = sendto(m_InitSocket[m_SocketNum], (char *) lpPutBuf, nBufLen, 0,
			(struct sockaddr*) &SockAddrIn, sizeof(struct sockaddr_in));
//	QueryPerformanceCounter(&nBefore);
	nBefore = get_system_ms();
	do
	{
//		QueryPerformanceCounter(&nAfter);
		nAfter = get_system_ms();
		dwTime = (u32) (nAfter - nBefore);
		if (dwTime > TimeOut)
		{
			return FALSE;
		}
		res = recv(m_InitSocket[m_SocketNum], (char*) lpGetBuf, MAX_PACKET_LEN,
				0);
		//res=recvfrom(hSocket,(char*)lpGetBuf,MAX_PACKET_LEN,0,(struct sockaddr*)&from,&fromlen);
	} while ((res < 1) || (res > MAX_PACKET_LEN));

	return TRUE;
}

BOOL Net_Packet(int hSocket, u8 *lpSendBuf, u8 *lpReceiveBuf)
{
	u8 SendTemp[MAX_PACKET_LEN];
	u8 ReceiveTemp[MAX_PACKET_LEN];

	int BufLen = lpSendBuf[0] + 2;
	if (BufLen > MAX_PACKET_LEN)
	{
		return FALSE;
	}

// 命令打包
	SendTemp[0] = 0x40;
	u8 checksum = 0x40;
	int i;
	for (i = 0; i < lpSendBuf[0]; i++)
	{
		SendTemp[i + 1] = lpSendBuf[i];

		checksum += SendTemp[i + 1];
	}
	checksum = ~checksum;
	checksum++;
	SendTemp[i + 1] = checksum;

	if (!SocketBuffer(hSocket, SendTemp, ReceiveTemp, BufLen))
	{
		return FALSE;
	}

	BufLen = ReceiveTemp[1] + 2;
	if (BufLen > MAX_PACKET_LEN)
	{
		return FALSE;
	}

	checksum = 0;
	for (i = 0; i < BufLen; i++)
		checksum += ReceiveTemp[i];
	if (checksum != 0)
	{
		return FALSE;
	}

	BOOL fRes = FALSE;
	if (ReceiveTemp[2] == SendTemp[2])
	{
		switch (ReceiveTemp[0])
		{
		case 0xF0:
			lpReceiveBuf[0] = 0xF0;
			if (BufLen - 4 > 0)
			{
				if ((ReceiveTemp[2] == 0x15) || (ReceiveTemp[2] == 0x16))
				{
					memcpy(&lpReceiveBuf[1], &ReceiveTemp[1], BufLen - 2);
				}
				else
				{
					memcpy(&lpReceiveBuf[1], &ReceiveTemp[3], BufLen - 4);
				}
			}
			fRes = TRUE;
			break;

		case 0xF4:
			lpReceiveBuf[0] = 0xF4;
			lpReceiveBuf[1] = ReceiveTemp[3];
			fRes = TRUE;
			break;
		}
	}

	return fRes;
}

//读取版本号
apiReturn Net_GetReaderVersion(int hSocket, u16 *wHardVer, u16 *wSoftVer)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x02;	// 命令码
	put[2] = 0;

	TimeOut = 50;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*wHardVer = get[1] * 0x100 + get[2];
	*wSoftVer = get[3] * 0x100 + get[4];

	return _OK;
}

//连接读写器_UDP连接方式
apiReturn Net_ConnectScanner(int *hSocket, char *nTargetAddress,
		u32 nTargetPort, char *nHostAddress, u32 nHostPort)
{
	apiReturn res;
	u16 gwHardVer, gwSoftVer;
	int m_Sock;

//	WSADATA WSAData;
//	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
//	{
//		WSACleanup();
//		return _init_net_error;
//	}
	m_SocketNum++;

	m_Sock = -1;
	m_Sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_Sock == -1)
	{
		m_SocketNum--;
		return _init_net_error;
	}

	if (m_InitSocket[m_SocketNum] == -1)
	{
		struct sockaddr_in m_SockHostAddr;
		m_SockHostAddr.sin_family = AF_INET;
		m_SockHostAddr.sin_addr.s_addr = inet_addr(nHostAddress);
		m_SockHostAddr.sin_port = htons(nHostPort);
		if (bind(m_Sock, (struct sockaddr *) &m_SockHostAddr,
				sizeof(struct sockaddr_in)) == -1)
		{
			m_SocketNum--;
			close(m_Sock);
			return _init_net_error;
		}

//		WSAEVENT hEvent;
//		hEvent = WSACreateEvent();
//		if (hEvent == WSA_INVALID_EVENT)
//		{
//			m_InitSocket[m_SocketNum] = -1;
//			m_SocketNum--;
//			closesocket(m_Sock);
//			return _init_net_error;
//		}
		fd_set fdp;
		struct timeval tm_out =
		{ 0, 0 };

		FD_ZERO(&fdp);
		FD_SET(m_Sock, &fdp);

//		if (WSAEventSelect(m_Sock, hEvent, FD_READ) == -1)
		if (select(m_Sock + 1, &fdp, NULL, NULL, &tm_out))
		{
			m_InitSocket[m_SocketNum] = -1;
			m_SocketNum--;
			close(m_Sock);
			return _init_net_error;
		}
	}

	if (m_InitSocket[m_SocketNum] == -1)
	{
		m_InitSocket[m_SocketNum] = m_Sock;
		sprintf(m_InforeaderIP[m_SocketNum].ip, "%s",
				(const char*) nTargetAddress);
		m_InforeaderIP[m_SocketNum].port = nTargetPort;
	}
//else
//{
//	for(i=1;i<MAX_READER_IP;i++)
//	{
//		if(m_InforeaderIP[i].port == 0)
//		{
//			wsprintf(m_InforeaderIP[i].ip, "%s",LPCTSTR(nTargetAddress));
//			m_InforeaderIP[i].port=nTargetPort;
//			m_Sock = m_InitSocket[m_SocketNum] + i;
//			break;
//		}
//	}

//}

	res = Net_GetReaderVersion(m_Sock, &gwHardVer, &gwSoftVer);
//	res=Net_SetAntenna(m_Sock,1);
	if (res != _OK)
	{

		Net_DisconnectScanner();
		m_SocketNum--;
		return _no_scanner;
	}

	*hSocket = m_Sock;

	return _OK;
}

//连接读写器_TCP连接方式
apiReturn TCP_ConnectScanner(int *hSocket, char *nTargetAddress,
		u32 nTargetPort, char *nHostAddress, u32 nHostPort)
{
	apiReturn res;
	u16 gwHardVer, gwSoftVer;
//	WSADATA WSAData;
	int m_Sock;

//	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
//	{
//		WSACleanup();
//		return _init_net_error;
//	}

	m_SocketNum++;

	m_Sock = -1;
	m_Sock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_Sock == -1)
	{
		m_SocketNum--;
		return _init_net_error;
	}

	if (m_InitSocket[m_SocketNum] == -1)
	{
		m_InitSocket[m_SocketNum] = m_Sock;
		sprintf(m_InforeaderIP[m_SocketNum].ip, "%s", nTargetAddress);
		m_InforeaderIP[m_SocketNum].port = nTargetPort;
	}
	/*else
	 {
	 for(i=1;i<MAX_READER_IP;i++)
	 {
	 if(m_InforeaderIP[i].port == 0)
	 {
	 wsprintf(m_InforeaderIP[i].ip, "%s",LPCTSTR(nTargetAddress));
	 m_InforeaderIP[i].port=nTargetPort;
	 m_Sock = m_InitSocket[m_SocketNum] + i;
	 break;
	 }
	 }

	 }*/
	struct sockaddr_in SockAddrIn;

	SockAddrIn.sin_family = AF_INET;
	SockAddrIn.sin_addr.s_addr = inet_addr(m_InforeaderIP[m_SocketNum].ip);
	SockAddrIn.sin_port = htons(m_InforeaderIP[m_SocketNum].port);

	res = connect(m_Sock, (struct sockaddr*) &SockAddrIn,
			sizeof(struct sockaddr_in));
	if (res == -1)
	{
		close(m_Sock);
		m_InitSocket[m_SocketNum] = -1;
		m_SocketNum--;
		return _init_net_error;
	}

	res = Net_GetReaderVersion(m_Sock, &gwHardVer, &gwSoftVer);
//	res=Net_SetAntenna(m_Sock,1);
	if (res != _OK)
	{
		Net_DisconnectScanner();
		m_SocketNum--;
		return _no_scanner;
	}

	*hSocket = m_Sock;

	return _OK;
}
apiReturn Net_SetCurrentSocketNum(int socketnum)
{
	m_SocketNum = socketnum;
	return _OK;
}
apiReturn Net_GetCurrentSocket(int *hsocket)
{
	*hsocket = m_InitSocket[m_SocketNum];
	if (*hsocket == -1)
		return _net_error;
	else
		return _OK;
}
//断开连接
apiReturn Net_DisconnectScanner()
{
	close(m_InitSocket[m_SocketNum]);
	m_InitSocket[m_SocketNum] = -1;

	return _OK;
}

//==============================设备控制命令==============================
//设置波特率
apiReturn Net_SetBaudRate(int hSocket, int nBaudRate)
{
	int BaudRate[] =
	{ 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
	int i;

	for (i = 0; i < 9; i++)
		if (nBaudRate == BaudRate[i])
			break;
	if (i > 8)
		return _baudrate_error;

	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x01;	// 命令码
	put[2] = i;		// 波特率码
	put[3] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设定输出功率
apiReturn Net_SetOutputPower(int hSocket, int nPower)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x04;	// 命令码
	put[2] = (u8) nPower;
	put[3] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设定工作频率
apiReturn Net_SetFrequency(int hSocket, int Min_Frequency, int Max_Frequency)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x04;	// 包数据长度
	put[1] = 0x05;	// 命令码
	put[2] = (u8) Min_Frequency;
	put[3] = (u8) Max_Frequency;
	put[4] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读取写器工作参数
apiReturn Net_ReadParam(int hSocket, Reader1000Param *pParam)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x06;	// 命令码
	put[2] = 0;

	TimeOut = 200;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(pParam, &get[1], sizeof(Reader1000Param));

	return _OK;
}

//设置调制度
apiReturn Net_SetModDepth(int hSocket, int ModDepth)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x07;	// 命令码
	put[2] = (u8) ModDepth;
	put[3] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得调制度
apiReturn Net_GetModDepth(int hSocket, int *ModDepth)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x08;	// 命令码
	put[2] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*ModDepth = get[1];

	return _OK;
}

//设置读写器工作参数
apiReturn Net_WriteParam(int hSocket, Reader1000Param *pParam)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	int n = sizeof(Reader1000Param);
	put[0] = 2 + n;		// 包数据长度
	put[1] = 0x09;	// 命令码
	memcpy(&put[2], pParam, n);
	put[2 + n] = 0;

	TimeOut = 200;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//选择天线
apiReturn Net_SetAntenna(int hSocket, int Antenna)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x0A;	// 命令码
	put[2] = (u8) Antenna;
	put[3] = 0;

	TimeOut = 200;

	if (!Net_Packet(hSocket, put, get))
	{
		return _net_error;
	}

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置读写器出厂参数
apiReturn Net_WriteFactoryParameter(int hSocket, Reader1000Param *pParam)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	int n = sizeof(Reader1000Param);
	put[0] = 2 + n;		// 包数据长度
	put[1] = 0x0C;	// 命令码
	memcpy(&put[2], pParam, n);
	put[2 + n] = 0;

	TimeOut = 200;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读取读写器出厂参数
apiReturn Net_ReadFactoryParameter(int hSocket)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x0D;	// 命令码
	put[2] = 0;

	TimeOut = 200;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//复位读写器
apiReturn Net_Reboot(int hSocket)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x0E;	// 命令码
	put[2] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置时间
apiReturn Net_SetReaderTime(int hSocket, ReaderDate time)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x08;	// 包数据长度
	put[1] = 0x11;	// 命令码
	put[2] = time.Year;
	put[3] = time.Month;
	put[4] = time.Day;
	put[5] = time.Hour;
	put[6] = time.Minute;
	put[7] = time.Second;
	put[8] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得时间
apiReturn Net_GetReaderTime(int hSocket, ReaderDate *time)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x12;	// 命令码
	put[2] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(time, &get[1], sizeof(ReaderDate));

	return _OK;
}

//增加名单
apiReturn Net_AddLableID(int hSocket, int listlen, int datalen, u8 * data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 4 + listlen * datalen;		// 包数据长度
	put[1] = 0x13;					// 命令码
	put[2] = (u8) listlen;
	put[3] = (u8) datalen;
	memcpy(&put[4], data, listlen * datalen);
	put[4 + listlen * datalen] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//删除名单
apiReturn Net_DelLableID(int hSocket, int listlen, int datalen, u8 * data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 4 + listlen * datalen;		// 包数据长度
	put[1] = 0x14;					// 命令码
	put[2] = (u8) listlen;
	put[3] = (u8) datalen;
	memcpy(&put[4], data, listlen * datalen);
	put[4 + listlen * datalen] = 0;

	TimeOut = 3000;
	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得名单
apiReturn Net_GetLableID(int hSocket, int startaddr, int listlen,
		int *relistlen, int *taglen, u8 * data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 5;		// 包数据长度
	put[1] = 0x15;					// 命令码
	put[2] = (u8) (startaddr >> 8);
	put[3] = (u8) (startaddr & 0xFF);
	put[4] = (u8) listlen;
	put[5] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*relistlen = get[3];
	*taglen = get[4];
	memcpy(data, &get[5], get[3] * get[4]);

	return _OK;
}

//获得记录
apiReturn Net_GetRecord(int hSocket, ReaderDate *stime, ReaderDate *etime,
		int startaddr, int listlen, int *relistlen, int *taglen, u8 * data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 17;		// 包数据长度
	put[1] = 0x16;					// 命令码
	memcpy(&put[2], stime, sizeof(ReaderDate));
	memcpy(&put[8], etime, sizeof(ReaderDate));
	put[14] = (u8) (startaddr >> 8);
	put[15] = (u8) (startaddr & 0xFF);
	put[16] = (u8) listlen;
	put[17] = 0;

	TimeOut = 200;
	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*relistlen = get[3];
	*taglen = get[4];
	memcpy(data, &get[5], get[3] * get[4]);

	return _OK;
}

//删除全部记录
apiReturn Net_DeleteAllRecord(int hSocket)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x17;	// 命令码
	put[2] = 0;

	TimeOut = 3000;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置读写器工作模式
apiReturn Net_SetWorkMode(int hSocket, int mode)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x0F;	// 命令码
	put[2] = (u8) mode;
	put[3] = 0;

	TimeOut = 3000;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置标签过滤器
apiReturn Net_SetReportFilter(int hSocket, int ptr, int len, u8 *mask)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	if (len == 0)
	{
		put[0] = 0x06;	// 包数据长度
		put[1] = 0x18;	// 命令码
		put[2] = (u8) (ptr >> 8);
		put[3] = (u8) ptr;
		put[4] = 0x00;
		put[5] = 0x00;
		put[6] = 0;
	}
	else
	{
		int i, m;
		if (len % 8 == 0)
			m = len / 8;
		else
			m = len / 8 + 1;

		put[0] = 0x06 + m;	// 包数据长度
		put[1] = 0x15;	// 命令码
		put[2] = (u8) (ptr >> 8);
		put[3] = (u8) ptr;
		put[4] = (u8) (len >> 8);
		put[5] = (u8) len;

		for (i = 0; i < m; i++)
		{
			put[6 + i] = mask[i];
		}
		put[6 + i] = 0;
	}

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得标签过滤器
apiReturn Net_GetReportFilter(int hSocket, int *ptr, int *len, u8 *mask)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int l;

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x19;	// 命令码
	put[2] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	if (get[1] == 0x04)
	{
		*ptr = 0;
		*len = 0;
		*mask = NULL;
	}
	else
	{
		*ptr = get[1] * 0x100 + get[2];
		*len = get[3] * 0x100 + get[4];
		if (*len % 8 == 0)
		{
			l = *len / 8;
		}
		else
		{
			l = *len / 8 + 1;
		}
		memcpy(mask, &get[5], l);
	}

	return _OK;
}

//==============================网络命令==============================
//设置读写器网络地址
apiReturn Net_SetReaderNetwork(int hSocket, u8 IP_Address[4], int Port,
		u8 Mask[4], u8 Gateway[4])
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x10;	// 包数据长度
	put[1] = 0x30;	// 命令码
	for (i = 0; i < 4; i++)
	{
		put[2 + i] = IP_Address[i];
		put[8 + i] = Mask[i];
		put[12 + i] = Gateway[i];
	}
	put[6] = (u8) (Port >> 8);
	put[7] = (u8) Port;
	put[16] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得读写器网络地址
apiReturn Net_GetReaderNetwork(int hSocket, u8 *IP_Address, int *Port, u8 *Mask,
		u8 *Gateway)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x31;	// 命令码
	put[2] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(IP_Address, &get[1], 4);
	*Port = (((int) get[5]) << 8) + (int) get[6];
	memcpy(Mask, &get[7], 4);
	memcpy(Gateway, &get[11], 4);

	return _OK;
}

//设置读写器网络MAC
apiReturn Net_SetReaderMAC(int hSocket, u8 MAC[6])
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x08;	// 包数据长度
	put[1] = 0x32;	// 命令码
	for (i = 0; i < 6; i++)
	{
		put[2 + i] = MAC[i];
	}
	put[2 + i] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得读写器网络MAC
apiReturn Net_GetReaderMAC(int hSocket, u8 *MAC)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x33;	// 命令码
	put[2] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(MAC, &get[1], 6);

	return _OK;
}
//设置读写器网络地址
/*apiReturn  Net_SetNetwork(int hSocket, ReaderIPParam *ipram)
 {
 u8 put[MAX_PACKET_LEN];
 u8 get[MAX_PACKET_LEN];

 int n=sizeof(ReaderIPParam);
 put[0]=0x10;	// 包数据长度
 put[1]=0x34;	// 命令码
 memcpy(&put[2], ipram, n);
 put[2+n]=0;

 TimeOut=100;

 if (!Net_Packet(hSocket,put,get))
 return _net_error;

 if (get[0]==0xF4)
 return get[1];

 return _OK;
 }

 //获得读写器网络地址
 apiReturn  Net_GetNetwork(int hSocket, ReaderIPParam *ipram)
 {
 u8 put[MAX_PACKET_LEN];
 u8 get[MAX_PACKET_LEN];

 put[0]=0x02;	// 包数据长度
 put[1]=0x35;	// 命令码
 put[2]=0;

 TimeOut=100;

 if (!Net_Packet(hSocket,put,get))
 return _net_error;

 if (get[0]==0xF4)
 return get[1];

 memcpy(ipram, &get[1], sizeof(ReaderIPParam));

 return _OK;
 }
 */
//==============================IO命令==============================
//设置读写器继电器状态
apiReturn Net_SetRelay(int hSocket, int relay)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x03;	// 命令码
	put[2] = (u8) relay;
	put[3] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//获得读写器继电器状态
apiReturn Net_GetRelay(int hSocket, int *relay)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x0B;	// 命令码
	put[2] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*relay = get[1];

	return _OK;
}

//获得读写器ID
apiReturn Net_GetReaderID(int hSocket, u8 *ReaderID)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0x8C;	// 命令码
	put[2] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(ReaderID, &get[1], 6);

	return _OK;
}

//==============================ISO-6B数据读写命令==============================
//检测标签存在
apiReturn Net_ISO6B_LabelPresent(int hSocket, int *nCounter)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0xFF;	// 命令码
	put[2] = 0;

	TimeOut = 500;
	*nCounter = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*nCounter = (int) get[1];

	return _OK;
}

//开始列出标签ID
apiReturn Net_ISO6B_ListID(int hSocket, u8 *btID, int *nCounter)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0xFE;	// 命令码
	put[2] = 0;

	TimeOut = 500;
	*nCounter = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*nCounter = (int) get[1];
	if (*nCounter <= 8)
		memcpy(btID, &get[2], *nCounter * ID_MAX_SIZE_64BIT);
	else
		memcpy(btID, &get[2], 8 * ID_MAX_SIZE_64BIT);

	return _OK;
}

//取得列出的标签ID
apiReturn Net_ISO6B_ListIDReport(int hSocket, u8 *btID, int stNum, int nCounter)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x04;	// 包数据长度
	put[1] = 0xFD;	// 命令码
	put[2] = (u8) stNum;
	put[3] = (u8) nCounter;
	put[4] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(btID, &get[1], nCounter * ID_MAX_SIZE_64BIT);

	return _OK;
}

//停止列出标签ID
apiReturn Net_ISO6B_StopListID(int hSocket)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0xFC;	// 命令码
	put[2] = 0;

	TimeOut = 50;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读取ISO6B标签ID号
apiReturn Net_ISO6B_ReadLabelID(int hSocket, u8 *IDBuffer, int *nCounter)
{
	apiReturn res;
	int i, j, k;

	*nCounter = 0;
	res = Net_ISO6B_ListID(hSocket, &IDBuffer[0], nCounter);
	if (res != _OK)
		return res;

	if (*nCounter > 8)
	{
		j = *nCounter / 8;
		k = *nCounter - j * 8;
		for (i = 1; i < j; i++)
		{
			res = Net_ISO6B_ListIDReport(hSocket,
					&IDBuffer[i * ID_MAX_SIZE_64BIT * 8], i * 8, 8);
			if (res != _OK)
				return res;
		}
		if (k != 0)
		{
			res = Net_ISO6B_ListIDReport(hSocket,
					&IDBuffer[j * ID_MAX_SIZE_64BIT * 8], j * 8, k);
			if (res != _OK)
				return res;
		}
	}

	Net_ISO6B_StopListID(hSocket);

	return _OK;
}

//列出选定标签
apiReturn Net_ISO6B_ListSelectedID(int hSocket, int Cmd, int ptr, u8 Mask,
		u8 *Data, u8 *IDBuffer, int *nCounter)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	apiReturn res;
	int i, j, k;

	put[0] = 0x0D;	// 包数据长度
	put[1] = 0xFB;	// 命令码
	put[2] = (u8) Cmd;
	put[3] = (u8) ptr;
	put[4] = (u8) Mask;
	for (i = 0; i < 8; i++)
	{
		put[5 + i] = Data[i];
	}
	put[13] = 0;

	TimeOut = 500;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*nCounter = (int) get[1];
	if (*nCounter > 8)
	{
		memcpy(&IDBuffer[0], &get[2], 8 * ID_MAX_SIZE_64BIT);
		j = *nCounter / 8;
		k = *nCounter - j * 8;
		for (i = 1; i < j; i++)
		{
			res = Net_ISO6B_ListIDReport(hSocket,
					&IDBuffer[i * ID_MAX_SIZE_64BIT * 8], i * 8, 8);
			if (res != _OK)
				return res;
		}
		if (k != 0)
		{
			res = Net_ISO6B_ListIDReport(hSocket,
					&IDBuffer[j * ID_MAX_SIZE_64BIT * 8], j * 8, k);
			if (res != _OK)
				return res;
		}
	}
	else
	{
		memcpy(IDBuffer, &get[2], *nCounter * ID_MAX_SIZE_64BIT);
	}

	Net_ISO6B_StopListID(hSocket);

	return _OK;
}

//读一块数据
apiReturn Net_ISO6B_ReadByteBlock(int hSocket, u8 *IDBuffer, u8 ptr, u8 len,
		u8 *Data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x0C;	// 包数据长度
	put[1] = 0xF6;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = ptr;
	put[11] = len;
	put[12] = 0;

	TimeOut = len * 50;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(Data, &get[1], len);

	return _OK;
}

//一次写4个字节数据，配合下条指令工作
apiReturn Net_ISO6B_WriteData(int hSocket, u8 *IDBuffer, u8 ptr, u8 len,
		u8 *Data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 12 + len;	// 包数据长度
	put[1] = 0xF5;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = ptr;
	put[11] = len;
	for (i = 0; i < len; i++)
	{
		put[12 + i] = Data[i];
	}
	put[12 + i] = 0;

	TimeOut = len * 50;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}
//一次写4个字节数据
apiReturn Net_ISO6B_WriteByteBlock(int hSocket, u8 *IDBuffer, u8 ptr, u8 len,
		u8 *Data)
{
	apiReturn result;
	int i_test, i_send, totl;
	int meg = 4;
	if (len <= meg)
	{
		i_test = 0;
		while (i_test < 3)
		{
			result = Net_ISO6B_WriteData(hSocket, IDBuffer, ptr, len, Data);
			if (result == _OK)
				break;
			else
				i_test++;
		}
		return result;
	}
	else
	{
		totl = len / meg;
		for (i_send = 0; i_send < totl; i_send++)
		{
			i_test = 0;
			while (i_test < 3)
			{
				result = Net_ISO6B_WriteData(hSocket, IDBuffer,
						ptr + i_send * meg, meg, Data + i_send * meg);
				if (result == _OK)
					break;
				else
					i_test++;
			}
			if (result != _OK)
				return result;
		}
		if ((totl * meg) < len)
		{
			totl = len - totl * meg;
			i_test = 0;
			while (i_test < 3)
			{
				result = Net_ISO6B_WriteData(hSocket, IDBuffer,
						ptr + i_send * meg, totl, Data + i_send * meg);
				if (result == _OK)
					break;
				else
					i_test++;
			}
		}
		return result;
	}
}

//一次写一个字节数据，配合下条指令工作
apiReturn Net_ISO6B_WriteAData(int hSocket, u8 *IDBuffer, u8 ptr, u8 len,
		u8 *Data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 12 + len;	// 包数据长度
	put[1] = 0xF2;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = ptr;
	put[11] = len;
	for (i = 0; i < len; i++)
	{
		put[12 + i] = Data[i];
	}
	put[12 + i] = 0;

//	TimeOut=1000;
	usleep(1000);
	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}
//一次写一个字节数据
apiReturn Net_ISO6B_WriteAByte(int hSocket, u8 *IDBuffer, u8 ptr, u8 len,
		u8 *Data)
{
	apiReturn result;
	int i_test, i_send, totl;
	int meg = 4;
	if (len <= meg)
	{
		i_test = 0;
		while (i_test < 3)
		{
			result = Net_ISO6B_WriteAData(hSocket, IDBuffer, ptr, len, Data);
			if (result == _OK)
				break;
			else
				i_test++;
		}
		return result;
	}
	else
	{
		totl = len / meg;
		for (i_send = 0; i_send < totl; i_send++)
		{
			i_test = 0;
			while (i_test < 3)
			{
				result = Net_ISO6B_WriteAData(hSocket, IDBuffer,
						ptr + i_send * meg, meg, Data + i_send * meg);
				if (result == _OK)
					break;
				else
					i_test++;
			}
			if (result != _OK)
				return result;
		}
		if ((totl * meg) < len)
		{
			totl = len - totl * meg;
			i_test = 0;
			while (i_test < 3)
			{
				result = Net_ISO6B_WriteAData(hSocket, IDBuffer,
						ptr + i_send * meg, totl, Data + i_send * meg);
				if (result == _OK)
					break;
				else
					i_test++;
			}
		}
		return result;
	}
}

//置写保护状态
apiReturn Net_ISO6B_WriteProtect(int hSocket, u8 *IDBuffer, u8 ptr)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x0B;	// 包数据长度
	put[1] = 0xF4;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = ptr;
	put[11] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读写保护状态
apiReturn Net_ISO6B_ReadWriteProtect(int hSocket, u8 *IDBuffer, u8 ptr,
		u8 *Protected)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x0B;	// 包数据长度
	put[1] = 0xF3;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = ptr;
	put[11] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*Protected = get[1];

	return _OK;
}

//==============================EPC C1G2数据读写命令==============================
apiReturn Net_EPC1G2_ListTagID(int hSocket, u8 mem, int ptr, u8 len, u8 *mask,
		u8 *btID, int *nCounter, int *IDlen)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i, m, next, last;

	if (len == 0)
		m = 0;
	else
	{
		m = len / 8;
		if (len % 8 != 0)
			m++;
	}

	put[0] = m + 6;		// 包数据长度
	put[1] = 0xEE;	// 命令码
	put[2] = mem;
	put[3] = (u8) (ptr >> 8);
	put[4] = (u8) (ptr);
	put[5] = len;
	for (i = 0; i < m; i++)
	{
		put[6 + i] = mask[i];
	}
	put[6 + i] = 0;

	TimeOut = 500;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*nCounter = (int) get[1];
	last = 2;
	if (*nCounter <= 8)
	{
		for (i = 0; i < *nCounter; i++)
		{
			next = get[last] * 2 + 1;	//1word=16bit
			memcpy(&btID[last - 2], &get[last], next);
			last += next;
		}
	}
	else
	{
		for (i = 0; i < 8; i++)
		{
			next = get[last] * 2 + 1;	//1word=16bit
			memcpy(&btID[last - 2], &get[last], next);
			last += next;
		}
	}

	*IDlen = last - 2;

	return _OK;
}

apiReturn Net_EPC1G2_GetIDList(int hSocket, u8 *btID, int stNum, int nCounter,
		int *IDlen)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x04;	// 包数据长度
	put[1] = 0xED;	// 命令码
	put[2] = (u8) stNum;
	put[3] = (u8) nCounter;
	put[4] = 0;

	TimeOut = 40;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	int i, next, last;
	last = 2;
	for (i = 0; i < nCounter; i++)
	{
		next = get[last] * 2 + 1;	//1word=16bit
		memcpy(&btID[last - 2], &get[last], next);
		last += next;
	}

	*IDlen = last - 2;

	return _OK;
}

//读取EPC1G2标签ID号
apiReturn Net_EPC1G2_ReadLabelID(int hSocket, u8 mem, int ptr, u8 len, u8 *mask,
		u8 *IDBuffer, int *nCounter)
{
	apiReturn res;
	int i, j, k, l, IDlen;

	*nCounter = 0;
	res = Net_EPC1G2_ListTagID(hSocket, mem, ptr, len, mask, &IDBuffer[0],
			nCounter, &IDlen);
	if (res != _OK)
		return res;

	if (*nCounter > 8)
	{
		j = *nCounter / 8;
		k = *nCounter - j * 8;
		for (i = 1; i < j; i++)
		{
			res = Net_EPC1G2_GetIDList(hSocket, &IDBuffer[IDlen], i * 8, 8, &l);
			if (res != _OK)
				return res;
			IDlen += l;
		}
		if (k != 0)
		{
			res = Net_EPC1G2_GetIDList(hSocket, &IDBuffer[IDlen], j * 8, k, &l);
			if (res != _OK)
				return res;
		}
	}

	return _OK;
}

//读一块数据
apiReturn Net_EPC1G2_ReadWordBlock(int hSocket, u8 EPC_WORD, u8 *IDBuffer,
		u8 mem, int ptr, u8 len, u8 *Data, u8 *AccessPassword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = EPC_WORD * 2 + 10;	// 包数据长度
	put[1] = 0xEC;			// 命令码
	put[2] = EPC_WORD;
	for (i = 0; i < EPC_WORD * 2; i++)
	{
		put[3 + i] = IDBuffer[i];
	}
	put[EPC_WORD * 2 + 3] = mem;
	if (ptr <= 127)
	{
		put[EPC_WORD * 2 + 4] = ptr;
		put[EPC_WORD * 2 + 5] = len;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 6 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 6 + i] = 0;
	}
	else
	{
		put[0] = EPC_WORD * 2 + 12;	// 包数据长度
		put[EPC_WORD * 2 + 4] = 0xC0;
		put[EPC_WORD * 2 + 5] = (u8) (ptr >> 8);
		put[EPC_WORD * 2 + 6] = (u8) (ptr);
		put[EPC_WORD * 2 + 7] = len;
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + 8 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + 8 + i] = 0;
	}

	TimeOut = EPC_WORD * 50;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(Data, &get[1], len * 2);

	return _OK;
}

//识别EPC同时读数据
apiReturn Net_EPC1G2_ReadEPCandData(int hSocket, u8 *EPC_WORD, u8 *IDBuffer,
		u8 mem, u8 ptr, u8 len, u8 *Data, int Address)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 5;	// 包数据长度
	put[1] = 0xE0;			// 命令码
	put[2] = mem;
	put[3] = ptr;
	put[4] = len;
	put[5] = 0;

	TimeOut = 40;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*EPC_WORD = get[1];
	memcpy(IDBuffer, &get[2], get[1] * 2);
	memcpy(Data, &get[2 + get[1] * 2], len * 2);

	return _OK;

}

//写一块数据
apiReturn Net_EPC1G2_WriteWordBlock(int hSocket, u8 EPC_WORD, u8 *IDBuffer,
		u8 mem, int ptr, u8 len, u8 *Data, u8 *AccessPassword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = EPC_WORD * 2 + len * 2 + 10;	// 包数据长度
	put[1] = 0xEB;				// 命令码
	put[2] = EPC_WORD;
	for (i = 0; i < EPC_WORD * 2; i++)
	{
		put[3 + i] = IDBuffer[i];
	}
	put[EPC_WORD * 2 + 3] = mem;
	if (ptr <= 127)
	{
		put[EPC_WORD * 2 + 4] = ptr;
		put[EPC_WORD * 2 + 5] = len;
		for (i = 0; i < len * 2; i++)
		{
			put[EPC_WORD * 2 + 6 + i] = Data[i];
		}
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + len * 2 + 6 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + len * 2 + 6 + i] = 0;
	}
	else
	{
		put[0] = EPC_WORD * 2 + len * 2 + 12;	// 包数据长度
		put[EPC_WORD * 2 + 4] = 0xC0;
		put[EPC_WORD * 2 + 5] = (u8) (ptr >> 8);
		put[EPC_WORD * 2 + 6] = (u8) (ptr);
		put[EPC_WORD * 2 + 7] = len;
		for (i = 0; i < len * 2; i++)
		{
			put[EPC_WORD * 2 + 8 + i] = Data[i];
		}
		for (i = 0; i < 4; i++)
		{
			put[EPC_WORD * 2 + len * 2 + 8 + i] = AccessPassword[i];
		}
		put[EPC_WORD * 2 + len * 2 + 8 + i] = 0;
	}

	TimeOut = EPC_WORD * 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置读写保护状态
apiReturn Net_EPC1G2_SetLock(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 mem,
		u8 Lock, u8 *AccessPassword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = EPC_WORD * 2 + 9;	// 包数据长度
	put[1] = 0xEA;			// 命令码
	put[2] = EPC_WORD;
	for (i = 0; i < EPC_WORD * 2; i++)
	{
		put[3 + i] = IDBuffer[i];
	}
	put[EPC_WORD * 2 + 3] = mem;
	put[EPC_WORD * 2 + 4] = Lock;
	for (i = 0; i < 4; i++)
	{
		put[EPC_WORD * 2 + 5 + i] = AccessPassword[i];
	}
	put[EPC_WORD * 2 + 9] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//擦除标签数据
apiReturn Net_EPC1G2_EraseBlock(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 mem,
		u8 ptr, u8 len)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = EPC_WORD * 2 + 6;	// 包数据长度
	put[1] = 0xE9;			// 命令码
	put[2] = EPC_WORD;
	for (i = 0; i < EPC_WORD * 2; i++)
	{
		put[3 + i] = IDBuffer[i];
	}
	put[EPC_WORD * 2 + 3] = mem;
	put[EPC_WORD * 2 + 4] = ptr;
	put[EPC_WORD * 2 + 5] = len;
	put[EPC_WORD * 2 + 6] = 0;

	TimeOut = EPC_WORD * 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//永久休眠标签
apiReturn Net_EPC1G2_KillTag(int hSocket, u8 EPC_WORD, u8 *IDBuffer,
		u8 *KillPassword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = EPC_WORD * 2 + 7;	// 包数据长度
	put[1] = 0xE8;			// 命令码
	put[2] = EPC_WORD;
	for (i = 0; i < EPC_WORD * 2; i++)
	{
		put[3 + i] = IDBuffer[i];
	}
	for (i = 0; i < 4; i++)
	{
		put[EPC_WORD * 2 + 3 + i] = KillPassword[i];
	}
	put[EPC_WORD * 2 + 3 + i] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//写EPC
apiReturn Net_EPC1G2_WriteEPC(int hSocket, u8 len, u8 *Data, u8 *AccessPassword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 7 + len * 2;			// 包数据长度
	put[1] = 0xE7;	// 命令码
	put[2] = len;
	for (i = 0; i < len * 2; i++)
	{
		put[3 + i] = Data[i];
	}
	for (i = 0; i < 4; i++)
	{
		put[len * 2 + 3 + i] = AccessPassword[i];
	}
	put[len * 2 + 3 + i] = 0;

	TimeOut = 500;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//块锁命令
apiReturn Net_EPC1G2_BlockLock(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 ptr,
		u8 *AccessPassword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = EPC_WORD * 2 + 8;	// 包数据长度
	put[1] = 0xE6;			// 命令码
	put[2] = EPC_WORD;
	for (i = 0; i < EPC_WORD * 2; i++)
	{
		put[3 + i] = IDBuffer[i];
	}
	put[3 + i] = ptr;
	for (i = 0; i < 4; i++)
	{
		put[EPC_WORD * 2 + 4 + i] = AccessPassword[i];
	}
	put[EPC_WORD * 2 + 4 + i] = 0;

	TimeOut = EPC_WORD * 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//EAS状态操作命令
apiReturn Net_EPC1G2_ChangeEas(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 State,
		u8 *AccessPassword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = EPC_WORD * 2 + 8;	// 包数据长度
	put[1] = 0xE5;			// 命令码
	put[2] = EPC_WORD;
	for (i = 0; i < EPC_WORD * 2; i++)
	{
		put[3 + i] = IDBuffer[i];
	}
	put[3 + i] = State;
	for (i = 0; i < 4; i++)
	{
		put[EPC_WORD * 2 + 4 + i] = AccessPassword[i];
	}
	put[EPC_WORD * 2 + 4 + i] = 0;

	TimeOut = EPC_WORD * 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//EAS报警命令
apiReturn Net_EPC1G2_EasAlarm(int hSocket)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x07;	// 包数据长度
	put[1] = 0xE4;	// 命令码
	put[2] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读保护设置
apiReturn Net_EPC1G2_ReadProtect(int hSocket, u8 *AccessPassword, u8 EPC_WORD,
		u8 *IDBuffer)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = EPC_WORD * 2 + 7;	// 包数据长度
	put[1] = 0xE3;			// 命令码
	for (i = 0; i < 4; i++)
	{
		put[2 + i] = AccessPassword[i];
	}
	put[2 + i] = EPC_WORD;
	for (i = 0; i < EPC_WORD * 2; i++)
	{
		put[7 + i] = IDBuffer[i];
	}
	put[EPC_WORD * 2 + 7] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//复位读保护设置
apiReturn Net_EPC1G2_RStreadProtect(int hSocket, u8 *AccessPassword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x06; //数据长度
	put[1] = 0xE2; //命令码
	for (i = 0; i < 4; i++)
	{
		put[2 + i] = AccessPassword[i];
	}
	put[6] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置用户区数据块读保护
apiReturn Net_EPC1G2_BlockReadLock(int hSocket, u8 EPC_WORD, u8 *IDBuffer,
		u8 Lock, u8 *AccessPassword)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = EPC_WORD * 2 + 8;	// 包数据长度
	put[1] = 0xE1;			// 命令码
	put[2] = EPC_WORD;
	for (i = 0; i < EPC_WORD * 2; i++)
	{
		put[3 + i] = IDBuffer[i];
	}
	put[EPC_WORD * 2 + 3] = Lock;
	for (i = 0; i < 4; i++)
	{
		put[EPC_WORD * 2 + 4 + i] = AccessPassword[i];
	}
	put[EPC_WORD * 2 + 8] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//侦测标签
apiReturn Net_EPC1G2_DetectTag(int hSocket)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;
	put[1] = 0xEF;
	put[3] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//==============================EM4442数据读写命令==============================
//开始列出标签ID
apiReturn Net_TK900_ListID(int hSocket, u8 *btID, int *nCounter)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0xCE;	// 命令码
	put[2] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	*nCounter = (int) get[1];
	if (*nCounter <= 8)
		memcpy(btID, &get[2], *nCounter * ID_MAX_SIZE_64BIT);
	else
		memcpy(btID, &get[2], 8 * ID_MAX_SIZE_64BIT);

	return _OK;
}

//取得列出的标签ID
apiReturn Net_TK900_ListIDReport(int hSocket, u8 *btID, int stNum, int nCounter)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x04;	// 包数据长度
	put[1] = 0xCD;	// 命令码
	put[2] = (u8) stNum;
	put[3] = (u8) nCounter;
	put[4] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(btID, &get[1], nCounter * ID_MAX_SIZE_64BIT);

	return _OK;
}

//读取EM4442标签ID号
apiReturn Net_TK900_ReadLabelID(int hSocket, u8 *IDBuffer, int *nCounter)
{
	apiReturn res;
	int i, j, k;

	*nCounter = 0;
	res = Net_TK900_ListID(hSocket, &IDBuffer[0], nCounter);
	if (res != _OK)
		return res;

	if (*nCounter > 8)
	{
		j = *nCounter / 8;
		k = *nCounter - j * 8;
		for (i = 1; i < j; i++)
		{
			res = Net_TK900_ListIDReport(hSocket,
					&IDBuffer[i * ID_MAX_SIZE_64BIT * 8], i * 8, 8);
			if (res != _OK)
				return res;
		}
		if (k != 0)
		{
			res = Net_TK900_ListIDReport(hSocket,
					&IDBuffer[j * ID_MAX_SIZE_64BIT * 8], j * 8, k);
			if (res != _OK)
				return res;
		}
	}

	return _OK;
}

//读一块数据
apiReturn Net_TK900_ReadPageBlock(int hSocket, u8 *IDBuffer, u8 ptr, u8 len,
		u8 *Data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x0C;	// 包数据长度
	put[1] = 0xCC;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = ptr;
	put[11] = len;
	put[12] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(Data, &get[1], len * 8);

	return _OK;
}

//写一块数据
apiReturn Net_TK900_WritePageBlock(int hSocket, u8 *IDBuffer, u8 ptr, u8 *Data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x13;	// 包数据长度
	put[1] = 0xCB;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = ptr;
	for (i = 0; i < 8; i++)
	{
		put[11 + i] = Data[i];
	}
	put[11 + i] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//置写保护状态
apiReturn Net_TK900_SetProtect(int hSocket, u8 *IDBuffer, u8 ptr, u8 len)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x0C;	// 包数据长度
	put[1] = 0xCA;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = ptr;
	put[11] = len;
	put[12] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读写保护状态
apiReturn Net_TK900_GetProtect(int hSocket, u8 *IDBuffer, u8 *state)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x0C;	// 包数据长度
	put[1] = 0xC9;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = 15;
	put[11] = 1;
	put[12] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(state, &get[1], 2);

	return _OK;
}

//设置标签进入TTO状态
apiReturn Net_TK900_SetTTO(int hSocket, u8 *IDBuffer, u8 ptr, u8 len)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x0C;	// 包数据长度
	put[1] = 0xC8;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = ptr;
	put[11] = len;
	put[12] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//读取TTO标签
apiReturn Net_TK900_ListTagPage(int hSocket, u8 *IDBuffer, u8 *Data)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x02;	// 包数据长度
	put[1] = 0xCF;	// 命令码
	put[2] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(IDBuffer, &get[1], ID_MAX_SIZE_64BIT + 1);
	memcpy(Data, &get[10], get[1] - 10);

	return _OK;

}

//得到TTO标签的开始地址和长度
apiReturn Net_TK900_GetTTOStartAdd(int hSocket, u8 *IDBuffer, u8 *len)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x0A;	// 包数据长度
	put[1] = 0xC2;	// 命令码
	for (i = 0; i < ID_MAX_SIZE_64BIT; i++)
	{
		put[2 + i] = IDBuffer[i];
	}
	put[10] = 0;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	memcpy(len, &get[1], 2);

	return _OK;
}

//==============================特殊命令==============================
//切换标签类型
apiReturn Net_SetTagType(int hSocket, int tagType)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x92;	// 命令码
	put[2] = (u8) tagType;
	put[3] = 0;

	TimeOut = 100;

	if (!Net_Packet(hSocket, put, get))
		return _net_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//修改为EP500
apiReturn SetEP500(int hScanner, u8 ifEP500)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x88;	// 命令码
	put[2] = ifEP500;
	put[3] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//开关射频功率
apiReturn Switch_RF_Power(int hScanner, int test, int RF_onoff)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x06;	// 包数据长度
	put[1] = 0x87;	// 命令码
	put[2] = (u8) test;
	put[3] = (u8) RF_onoff;
	put[4] = 0x00;
	put[5] = 0x00;
	put[6] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//设置读写器ID
apiReturn SetReaderID(int hScanner, u8 *ReaderID)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];
	int i;

	put[0] = 0x08;	// 包数据长度
	put[1] = 0x8B;	// 命令码
	for (i = 0; i < 6; i++)
	{
		put[2 + i] = ReaderID[i];
	}
	put[2 + i] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//修改读写器频率范围
apiReturn changeFrequency(int hScanner, u8 fre)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x86;	// 命令码
	put[2] = (u8) fre;
	put[3] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

//修改读写器硬件版本号
apiReturn SetHardVersion(int hScanner, int wHardVer)
{
	u8 put[MAX_PACKET_LEN];
	u8 get[MAX_PACKET_LEN];

	put[0] = 0x03;	// 包数据长度
	put[1] = 0x85;	// 命令码
	put[2] = (u8) wHardVer;
	put[3] = 0;

	if (!Packet(hScanner, put, get, 1))
		return _comm_error;

	if (get[0] == 0xF4)
		return get[1];

	return _OK;
}

