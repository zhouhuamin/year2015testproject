#include "includes.h"
#include "port.h"
#include "packet_format.h"

extern void GetCurTime(char *pTime);
extern struct struDeviceAndServiceStatus g_ObjectStatus;
extern pthread_mutex_t g_StatusMutex;

int param_init_handle(void)
{
	int ret;

	//log_send("parame init...");
	syslog(LOG_DEBUG, "parame init...\n");

	do
	{
		ret = parame_init();
		if (ret < 0)
		{
			//printf("parame init error!\n");
			syslog(LOG_DEBUG, "parame init error!\n");
		}
		else
		{
			break;
		}
		sleep(1);
	} while (ret < 0);
	//log_send("LOG:\tparam init OK!\n");
	syslog(LOG_DEBUG, "LOG:\tparam init OK!\n");

	return 0;
}

int dev_open_handle(int *p_fd_com)
{
	int ret;
	//log_send("LOG:\topen the reader...\n");
	syslog(LOG_DEBUG, "LOG:\topen the reader...\n");

	//连接读卡�?	
	while (1)
	{
		ret = dev_open(p_fd_com);
		if (ret < 0)
		{
			//printf("open reader port error! %d\n", ret);
			syslog(LOG_DEBUG, "open reader port error! %d\n", ret);
		}
		else
		{
			gReal_system.dev_open_flag = 1;
			break;
		}
		sleep(2);
	}
	//log_send("LOG:\topen the reader OK!\n");
	syslog(LOG_DEBUG, "LOG:\topen the reader OK!\n");
	return 0;
}
int dev_connect_handle(int fd_dev)
{
	int ret;

	//log_send("LOG:\tconnect the dev...\n");
	syslog(LOG_DEBUG, "LOG:\tconnect the dev...\n");

	while (1) //
	{
		if (gReal_system.dev_open_flag)
		{
			ret = dev_connect(fd_dev);
			if (ret != 0)
			{
				//printf("connect read error! %d\n", ret);
				syslog(LOG_DEBUG, "connect read error! %d\n", ret);

				pthread_mutex_lock(&g_StatusMutex);
				strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
				g_ObjectStatus.nLocalPort		= 0;
				strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
				g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
				strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
				g_ObjectStatus.nServiceType	= 0;
				strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
				strcpy(g_ObjectStatus.szObjectStatus, "021");
				strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");

				{
					char szNowTime[32] = {0};
					GetCurTime(szNowTime);
					szNowTime[31] = '\0';
					strcpy(g_ObjectStatus.szReportTime, szNowTime);
				}
				pthread_mutex_unlock(&g_StatusMutex);

			}
			else
			{
				gReal_system.dev_connect_flag = 1;
				break;
			}
		}

		sleep(2);
	}
	//log_send("LOG:\tconnect the dev OK!\n");
	syslog(LOG_DEBUG, "LOG:\tconnect the dev OK!\n");
	return 0;
}

int server_connect_handle(int *pFd_ser)
{
	//log_send("LOG:\tconnect the server...\n");
	syslog(LOG_DEBUG, "LOG:\tconnect the server...\n");
	//连接服务�?	
	while (1)
	{
		*pFd_ser = connect_server(gSetting_system.server_ip,
				gSetting_system.server_port);
		if (*pFd_ser <= 0)
		{
			//printf("connect to server error!\n");
			syslog(LOG_DEBUG, "connect to server error-1!\n");

			pthread_mutex_lock(&g_StatusMutex);
			strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
			g_ObjectStatus.nLocalPort		= 0;
			strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
			g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
			strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
			g_ObjectStatus.nServiceType	= 0;
			strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
			strcpy(g_ObjectStatus.szObjectStatus, "011");
			strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");
			char szNowTime[32] = {0};
			GetCurTime(szNowTime);
			szNowTime[31] = '\0';
			strcpy(g_ObjectStatus.szReportTime, szNowTime);
			pthread_mutex_unlock(&g_StatusMutex);
		}
		else
		{
			gReal_system.server_connect_flag = 1;
			break;
		}
		sleep(2);
	}
	//log_send("LOG:\tconnect the server OK!\n");
	syslog(LOG_DEBUG, "LOG:\tconnect the server OK!\n");

	return 0;
}
int server_register_handle(int server_fd)
{
	int ret;

	//log_send("LOG:\tregister the device...\n");
	syslog(LOG_DEBUG, "LOG:\tregister the device...\n");

	while (1)
	{
		if (gReal_system.server_connect_flag)
		{
			ret = net_dev_register_handle(server_fd);
			if (ret < 0)
			{
				// LOG("register dev error!\n");
				syslog(LOG_DEBUG, "register dev error!\n");
			}
			else
			{
				gReal_system.server_register_flag = 1;
				break;
			}
		}

		sleep(2);
	}
	//log_send("LOG:\tregister the device OK\n");
	syslog(LOG_DEBUG, "LOG:\tregister the device OK\n");
	return 0;
}

int dev_retry_connect_handle(int *pFd_dev)
{
	int fd_tmp;
	int ret;
	//log_send("ERR:\tthe reader's connection lost \n");
	syslog(LOG_DEBUG, "ERR:\tthe reader's connection lost \n");

	//连接读卡�?	
	while (1)
	{
		ret = dev_open(&fd_tmp);
		if (ret >= 0)
		{
			gReal_system.dev_open_flag = 1;
			close(*pFd_dev);
			*pFd_dev = fd_tmp;
			//log_send("MSG:\tconnect the reader with new fd!\n");
			syslog(LOG_DEBUG, "MSG:\tconnect the reader with new fd!\n");
			break;
		}
		else
		{
			ret = dev_connect(*pFd_dev);
			if (ret == 0)
			{
				gReal_system.dev_connect_flag = 1;
				//log_send("MSG:\tconnect the reader with old fd!\n");
				syslog(LOG_DEBUG, "MSG:\tconnect the reader with old fd!\n");
				break;
			}
		}
		sleep(1);
	}
	return 0;
}

int server_retry_connect_handle(int *pFd_ser)
{
	int ret;

	close(*pFd_ser);
	//log_send("ERR:\tthe server's connection is lost!\n");
	//log_send("MSG:\tRetry to connect to the server...\n");
	syslog(LOG_DEBUG, "ERR:\tthe server's connection is lost!\n");
	syslog(LOG_DEBUG, "MSG:\tRetry to connect to the server...\n");

	//连接服务�?	
	while (1)
	{
		int nTmpSocket = -1;
		nTmpSocket = connect_server(gSetting_system.server_ip,
				gSetting_system.server_port);
		syslog(LOG_DEBUG, "BBBBBBBBBBBBBBBBBB\n");
		if (nTmpSocket <= 0)
		{
			//printf("connect to server error!\n");
			syslog(LOG_DEBUG, "connect to server error-2!\n");

			pthread_mutex_lock(&g_StatusMutex);
			strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
			g_ObjectStatus.nLocalPort		= 0;
			strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
			g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
			strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
			g_ObjectStatus.nServiceType	= 0;
			strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
			strcpy(g_ObjectStatus.szObjectStatus, "011");
			strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");
			char szNowTime[32] = {0};
			GetCurTime(szNowTime);
			szNowTime[31] = '\0';
			strcpy(g_ObjectStatus.szReportTime, szNowTime);
			pthread_mutex_unlock(&g_StatusMutex);
			syslog(LOG_DEBUG, "XXXXXXXXXXXXXXX\n");
		}
		else
		{
			*pFd_ser = nTmpSocket;
			//log_send("MSG:\tconnected to the server!\n");
			syslog(LOG_DEBUG, "MSG:\tconnected to the server!\n");
			ret = net_dev_register_handle(*pFd_ser);
			if (0 == ret)
			{
				//log_send("MSG:\tregister to the server!\n");
				syslog(LOG_DEBUG, "MSG:\tregister to the server!\n");
				break;
			}
			else
			{
				close(*pFd_ser);
			}
		}
		sleep(2);
	}
	return 0;
}

