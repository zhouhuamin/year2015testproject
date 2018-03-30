#include "includes.h"
#include "common.h"
#include "port.h"

char str_tmp[0x800];
int msg_log;
static int msg_dev;

u32 get_system_ms(void)
{
	static u32 first_in = 1;
	static struct timeval tm_s;
	struct timeval tm_cur;

	if (first_in)
	{
		first_in = 0;
		gettimeofday(&tm_s, 0);
	}

	gettimeofday(&tm_cur, 0);

	return ((tm_cur.tv_sec * 1000 + tm_cur.tv_usec / 1000)
			- (tm_s.tv_sec * 1000 + tm_s.tv_usec / 1000));
}

int log_init(void)
{
	key_t key;

	system("touch "MSG_LOG_KEY_TMP_FILE_NAME);
	key = ftok(MSG_LOG_KEY_TMP_FILE_NAME, 'a');

	msg_log = msgget(key, IPC_CREAT | 0666);
	if (msg_log < 0)
	{
		//LOG("msg log id < 0!\n");
		syslog(LOG_DEBUG, "msg log id < 0!\n");
		exit(0);
	}

	return 0;
}

int log_send(char *info)
{
	msg_log_t msg;
	int ret;

	if ( NULL == info)
	{
		return -1;
	}

	msg.msg_type = MSG_LOG_FROM_DEV;

	msg.msg.cmd = 0;
	msg.msg.src_id = 0;
	msg.msg.parame = 0;

	if (strlen(info) >= LOG_MAX_SIZE)
	{
		info[LOG_MAX_SIZE] = '\0';
	}
	strcpy(msg.msg.log, info);

	ret = msgsnd(msg_log, &msg, sizeof(msg.data), IPC_NOWAIT);
	if (ret < 0)
	{
		//printf("send msg error\n");
		syslog(LOG_DEBUG, "send msg error\n");
	}

	//printf(info);
	syslog(LOG_DEBUG, "%s\n", info);

	return 0;
}

void msg_queue_init(void)
{
	key_t key;

	system("touch "DEV_NEW_DATA_QUEUE_TMP_FILE);
	key = ftok(DEV_NEW_DATA_QUEUE_TMP_FILE, 'a');

	msg_dev = msgget(key, IPC_CREAT | 0666);
	if (msg_dev < 0)
	{
		//LOG("mag play record id < 0!\n");
		syslog(LOG_DEBUG, "mag play record id < 0!\n");
		exit(0);
	}
}
int dev_send_msg(eMsg_type cmd, u32 src_id)
{
	msg_event_t msg;
	int ret;

	msg.msg_type = MSG_FROM_DEV_PTHREAD;

	msg.msg.cmd = cmd;
	msg.msg.src_id = src_id;
	msg.msg.parame = 0;

	ret = msgsnd(msg_dev, &msg, sizeof(msg.data), IPC_NOWAIT);
	if (ret < 0)
	{
		//printf("send dev msg error\n");
		syslog(LOG_DEBUG, "send dev msg error\n");
	}
	return 0;
}

int recv_msg_from_dev(eMsg_type *pCMD)
{
	msg_event_t msg;
	int ret;

	ret = msgrcv(msg_dev, &msg, sizeof(msg.data), MSG_FROM_DEV_PTHREAD,
	IPC_NOWAIT);

	if (ret > 0)
	{
		*pCMD = (eMsg_type)msg.msg.cmd;
		return 1;
	}
	return 0;
}

int data_to_str(char *const str_out, u8 *d_buf, int d_len )
{
    int i = 0;
    char *p_str;

    str_out[0] ='\0';
    for( i = 0; i < d_len; i++)
    {
        sprintf(str_out+strlen(str_out),"%02X ",d_buf[i]);
    }

    return 0;
}

int str_to_data(u8 * const d_buf, int *d_len, char *str_in)
{
	u8 *p_data, value = 0;
	int flag = 0;

	if (NULL == str_in)
	{
		return -1;
	}
	else if (0 == strlen(str_in))
	{
		*d_len = 0;
		return 0;
	}

	while (*str_in != '\0')
	{
		if ((*str_in >= '0' && *str_in <= '9')
				|| (*str_in >= 'a' && *str_in <= 'f')
				|| (*str_in >= 'A' && *str_in <= 'F'))
		{
			break;
		}
		str_in++;
	}

	p_data = d_buf;
	while (*str_in != '\0')
	{
		if (*str_in >= '0' && *str_in <= '9')
		{
			if (flag)
			{
				value <<= 4;
				value += *str_in - '0';
				*p_data++ = value;
				value = 0;
				flag = 0;
			}
			else
			{
				value = *str_in - '0';
				flag = 1;
			}
		}
		else if (*str_in >= 'a' && *str_in <= 'f')
		{
			if (flag)
			{
				value <<= 4;
				value += *str_in - 'a' + 10;
				*p_data++ = value;
				value = 0;
				flag = 0;
			}
			else
			{
				value = *str_in - 'a' + 10;
				flag = 1;
			}
		}
		else if (*str_in >= 'A' && *str_in <= 'F')
		{
			if (flag)
			{
				value <<= 4;
				value += *str_in - 'A' + 10;
				*p_data++ = value;
				value = 0;
				flag = 0;
			}
			else
			{
				value = *str_in - 'A' + 10;
				flag = 1;
			}
		}
		else if (' ' == *str_in || '\n' == *str_in)
		{
			if (flag)
			{
				*p_data++ = value;
				value = 0;
				flag = 0;
			}
		}
		else
		{
			*d_len = p_data - d_buf;
			return -1;
		}

		str_in++;
	}

	*d_len = p_data - d_buf;
	return 0;
}

int ip_address_check(char *p_ip_str)
{
	int len;
	int i;
	int dot_number = 0;

	if ( NULL == p_ip_str)
	{
		return -1;
	}

	len = strlen(p_ip_str);

	if (len < 8 || len > 16) //1.1.1.1  and 192.168.128.254
	{
		return -2;
	}

	for (i = 0; i < len; i++)
	{
		if (p_ip_str[i] == '.')
		{
			dot_number++;
			continue;
		}
		if ((p_ip_str[i] < '0') || (p_ip_str[i] > '9')) //invalid char
		{
			return -3;
		}
	}

	if (3 != dot_number) //format error
	{
		return -4;
	}

	return 0;
}

int interger_check(char *p_str)
{
	int len;
	int i;

	if ( NULL == p_str)
	{
		return -1;
	}

	len = strlen(p_str);

	for (i = 0; i < len; i++)
	{
		if ((p_str[i] < '0') || (p_str[i] > '9')) //invalid char
		{
			return -2;
		}
	}

	return 0;
}

