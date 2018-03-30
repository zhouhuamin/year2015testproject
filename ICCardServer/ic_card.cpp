#include "includes.h"
#include "config.h"
#include "ic_card.h"
//#include "mwrf.h"
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

u8 IC_keys[7] =
//{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
		{ 0xA8, 0x3F, 0x63, 0x17, 0x69, 0xE2, 0x00 };

int ic_dev_open(int *p_fd_ic)
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
		syslog(LOG_DEBUG, "connect error:%s", strerror(errno));
		close(fd_com);
		return -3;
	}

	flag = fcntl(fd_com, F_GETFL, 0);
	fcntl(fd_com, F_SETFL, flag | O_NONBLOCK);

	*p_fd_ic = fd_com;
	return 0;
}

int ic_card_section_read(int fd_ic, int block_NO, card_block_t *p_block)
{
	int ret = 0;
	int ic_type = 0;
	u32 ic_serial_num = 0;
	int result = 0;

	ret = ic_app_request(fd_ic, 0, &ic_type);
	if (0 != ret)
	{
//		LOG("ic request error!\n");
		return ret;
	}

	ret = ic_app_anticoll(fd_ic, 0x93, 0, &ic_serial_num);
	if (0 != ret)
	{
//		LOG("ic anticoll error!\n");
		return ret;
	}

	ret = ic_app_select(fd_ic, 0x93, ic_serial_num, &result);
	if ((0 != ret) && (0x08 != result))
	{
//		LOG("ic select error!\n");
		return ret;
	}

	if (ic_type == 0x04)
	{
		ret = ic_app_authentication(fd_ic, 0x00, (block_NO / 4), IC_keys);
		if (0 != ret)
		{
//			LOG("ic authentication error!\n");
			return ret;
		}

		ret = ic_app_read_block(fd_ic, block_NO, p_block);
		if (0 != ret)
		{
//			LOG("ic read block error!\n");
			return ret;
		}
		p_block->system_tick = get_system_ms();
	}
	else
	{
//		LOG("ic type error!\n");
		return -1;
	}

	return 0;
}

int compare_ic_block(card_block_t *p_old_block, card_block_t *p_new_block)
{
	int i;

	if (p_old_block->len != p_new_block->len)
	{
		return 1;
	}

	for (i = 0; i < p_new_block->len; i++)
	{
		if (p_old_block->data[i] != p_new_block->data[i])
		{
			return 2;
		}
	}

	if ((p_new_block->system_tick - p_old_block->system_tick)
			> gSetting_system.DEV_scan_duration_time)
	{
		return 3;
	}

	return 0;
}
