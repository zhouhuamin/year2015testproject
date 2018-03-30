#include "includes.h"
#include <stdlib.h>
#include "weight.h"
#include "config.h"
#include "errno.h"
#include <syslog.h>

static u8 buf[200];
static average_value_t average_value_unit;
static real_value_t real_value_unit;

static int value_state = 0; //0:value <2000 1:value >2000 2:keep stable
static int value_state_clean_request = 0;
static int prev_average_value = 0;

int weight_init(int *p_fd_com)
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
		syslog(LOG_DEBUG, "connect error:%s\n", strerror(errno));
		close(fd_com);
		return -3;
	}

	flag = fcntl(fd_com, F_GETFL, 0);
	fcntl(fd_com, F_SETFL, flag | O_NONBLOCK);

	*p_fd_com = fd_com;
	return 0;
}

int weight_get(int fd_wt, int time_out)
{
	int ret;
	int r_len = 0;
	int len = 0;
	int value = 0;
	int i;
	int state = 0;
	int start_time_ms;

	len = gSetting_system.WT_protocol_head_len
			+ gSetting_system.WT_protcol_value_bytes
			+ gSetting_system.WT_protocol_tail_len;

	start_time_ms = get_system_ms();
	while (1)
	{
		ret = read(fd_wt, buf + state, 1);
		if (ret > 0)
		{
			if (buf[state] == gSetting_system.WT_protocol_head[state])
			{
				state++;
				if (state >= gSetting_system.WT_protocol_head_len)
				{
					r_len = gSetting_system.WT_protocol_head_len;
					break;
				}
			}
			else
			{
				state = 0;
			}
		}
		else
		{
			usleep(1000);
		}

		if ((get_system_ms() - start_time_ms) >= time_out)
		{
			return -2;
		}
	}

	while (1)
	{
		ret = read(fd_wt, buf + r_len, len - r_len);

		if (ret > 0)
		{
			r_len += ret;
			if (r_len >= len)
			{
				break;
			}
		}

		usleep(1000);
		time_out--;

		if (time_out <= 0)
		{
			break;
		}
	}

	if (time_out == 0)
	{
		return -1;
	}

	for (i = 0; i < gSetting_system.WT_protocol_tail_len; i++)
	{
		if (buf[gSetting_system.WT_protocol_head_len
				+ gSetting_system.WT_protcol_value_bytes + i]
				!= gSetting_system.WT_protocol_tail[i])
		{
			break;
		}
	}

	if (i < gSetting_system.WT_protocol_tail_len)
	{
		return -2;
	}
	else
	{
		buf[gSetting_system.WT_protocol_head_len
				+ gSetting_system.WT_protcol_value_bytes] = '\0';
		value = atoi((const char *)&buf[gSetting_system.WT_protocol_head_len]);
	}

	return value;
}

int real_value_parse(int real_value)
{
	int sum = 0;
	int i = 0;
	int diff_value = 0;
	average_value_t *pAverage;
	int average_value;

	pAverage = &average_value_unit;

	insert_real_value(real_value);
	average_value = get_average_value();

	pAverage->data[pAverage->index] = average_value;
	pAverage->index++;
	pAverage->index %= AVERAGE_VALUE_BUF_SIZE;

	if( value_state_clean_request >0 )
	{
		value_state_clean_request = 0;
		value_state = 0;

		memset(&real_value_unit,0,sizeof(real_value_t));
		memset(&average_value_unit,0,sizeof(average_value_t));
	}

	switch (value_state)
	{
	case 0:
		if (average_value > gSetting_system.WT_threshold_value)
		{
			value_state++;
			prev_average_value = 0;
			return -99;
//			LOG(
//					"-----------------------threshold value-------------------------------\n");
		}
		break;
	case 1:
		for (i = 0; i < AVERAGE_VALUE_BUF_SIZE - 1; i++)
		{
			if (pAverage->data[i] >= pAverage->data[i + 1])
			{
				sum += (pAverage->data[i] - pAverage->data[i + 1]);
			}
			else
			{
				sum += (pAverage->data[i + 1] - pAverage->data[i]);
			}
		}

		diff_value = sum / AVERAGE_VALUE_BUF_SIZE;

		if ((diff_value <= gSetting_system.WT_average_diff_value)
				&& (average_value >= gSetting_system.WT_threshold_value))
		{
			value_state++;
//			LOG(
//					"-----------------------stable value-------------------------------\n");
			return average_value;
		}

		if (average_value < prev_average_value)
		{
			value_state++;
//			LOG(
//					"-----------------------stable value-------------------------------\n");
			return prev_average_value;
		}

		if (average_value <= gSetting_system.WT_threshold_value)
		{
			value_state = 0;
//			LOG(
//					"-----------------------state 0-------------------------------\n");
		}

		prev_average_value = average_value;
		break;
	case 2:
		if (average_value <= gSetting_system.WT_threshold_value)
		{
			value_state = 0;
//			LOG(
//					"-----------------------state 0-------------------------------\n");
		}
		break;
	default:
		value_state = 0;
		break;
	}

	return -1;
}

int get_average_value(void)
{
	int value = 0;
	int i;
	int sum = 0;
	real_value_t *pReal;

	pReal = &real_value_unit;

	for (i = 0; i < REAL_VALUE_BUF_SIZE; i++)
	{
		sum += pReal->data[i];
	}

	value = sum / REAL_VALUE_BUF_SIZE;

	return value;
}

int insert_real_value(int wt_value)
{
	real_value_t *pReal;
	pReal = &real_value_unit;
	pReal->data[pReal->index] = wt_value;
	pReal->index++;
	pReal->index %= REAL_VALUE_BUF_SIZE;

	return 0;
}

int weight_clean(void)
{
	value_state_clean_request = 1;

	return 0;
}
