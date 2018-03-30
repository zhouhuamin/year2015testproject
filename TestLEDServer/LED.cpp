#include "LED.h"
#include "CRC.h"
#include "common.h"
#include "config.h"
#include "includes.h"
#include "LED.h"
#include "packet_format.h"

#include "jzLocker.h"

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

static LED_frame_s led_frame_src;
static LED_frame_s led_frame_send;
static LED_frame_s led_frame_recv;

static int led_frame_attach(LED_frame_s *p_src, LED_frame_s *p_dst);
static int led_frame_check(LED_frame_s *p_src, LED_frame_s *p_dst);

int clean_label_list_cnt = 0;

int add_label_to_list(u8 *data, u8 len)
{
	if (data == NULL || len <= 0)
		return -1;
	char szLabel[200] = {0};
	for (int i = 0; i < len; i++)
	{
		sprintf(szLabel + strlen(szLabel), "%02X", data[i]);
	}
	szLabel[199] = '\0';
	std::map<std::string, strutLabelModifiers>::iterator labelMapIter;

	std::string strTmpLabel = szLabel;
	strutLabelModifiers	modifiers = {0};
	modifiers.nCount		= 1;
	modifiers.nTime			= time(0);
	modifiers.nSendFlag		= 0;
	//syslog(LOG_DEBUG, "strTmpLabel:%s\n", strTmpLabel.c_str());

	g_LabelMapLock.lock();
	labelMapIter = g_LabelMap.find(strTmpLabel);
	if (labelMapIter != g_LabelMap.end())
	{
		++(labelMapIter->second.nCount);
		//time_t t 		= time(0);
		//labelMapIter->second.nTime = t;
		//syslog(LOG_DEBUG, "11111strTmpLabel:%s\n", strTmpLabel.c_str());
	}
	else
	{
		//time_t t 		= time(0);
		//labelMapIter->second = (int)t;
		//int nCount = (int)t;
		g_LabelMap.insert(make_pair(strTmpLabel, modifiers));
		//syslog(LOG_DEBUG, "22222strTmpLabel:%s\n", strTmpLabel.c_str());
	}
	g_LabelMapLock.unlock();



	//syslog(LOG_DEBUG, "enter chk_label_in_list clean_label_list_cnt:%d, dev_unit.request_clean_history_cnt:%d\n", clean_label_list_cnt, dev_unit.request_clean_history_cnt);

	//if (clean_label_list_cnt != dev_unit.request_clean_history_cnt)
	//{
	//	clean_label_list_cnt = dev_unit.request_clean_history_cnt;
	//	clean_list();
	//}

	//if (chk_label_in_list(data, len))
	//{
	//	syslog(LOG_DEBUG, "chk_label_in_list return 1\n");
	//	return 1;
	//}

	//memcpy(label_list.label[label_list.cur_index].data, data, len);
	//label_list.label[label_list.cur_index].len = len;
	//label_list.cur_index++;
	//label_list.cur_index %= LABEL_LIST_SIZE;
	//if (label_list.label_cnt < LABEL_LIST_SIZE)
	//{
	//	if (0 == label_list.label_cnt)
	//	{
	//		dev_unit.dev_scan_ok_start_systick = get_system_ms();
	//	}
	//	label_list.label_cnt++;
	//}

	//syslog(LOG_DEBUG, "chk_label_in_list return 0\n");
	return 0;
}

int send_msg_to_main_pthread(void)
{
	syslog(LOG_DEBUG, "Enter send_msg_to_main_pthread, label_list.label_cnt=%d\n", label_list.label_cnt);
	if (((get_system_ms() - dev_unit.dev_scan_ok_start_systick)
			< gSetting_system.DEV_scan_duration_time)
			&& (label_list.label_cnt < 2))
	{
		return -1;
	}

	syslog(LOG_DEBUG, "label_cnt:%d, msg_send_flag:%d\n", label_list.label_cnt, label_list.msg_send_flag);
	if (label_list.label_cnt > 0)
	{
		if (0 == label_list.msg_send_flag)
		{
			dev_send_msg(msg_DEV_NEW_DATA, 0);
			//label_list.msg_send_flag = 1;
		}
	}

	return 0;
}

int get_label_frome_list(char label1[200], char label2[200])
{
	int index_start;
	u8 *p_data;
	int len;
	int i;

	label1[0] = 0;
	label2[0] = 0;

	if (label_list.label_cnt >= LABEL_LIST_SIZE)
	{
		index_start = (label_list.cur_index + LABEL_LIST_SIZE - 2)
				% LABEL_LIST_SIZE;

		p_data = label_list.label[index_start].data;
		len = label_list.label[index_start].len;
		if (len > LABEL_SIZE_MAX)
		{
			len = LABEL_SIZE_MAX;
		}

		for (i = 0; i < len; i++)
		{
			sprintf(label1 + strlen(label1), "%02X", p_data[i]);
		}

		p_data = label_list.label[index_start + 1].data;
		len = label_list.label[index_start + 1].len;
		if (len > LABEL_SIZE_MAX)
		{
			len = LABEL_SIZE_MAX;
		}
		for (i = 0; i < len; i++)
		{
			sprintf(label2 + strlen(label2), "%02X", p_data[i]);
		}
	}
	else if (label_list.label_cnt >= 2)
	{
		index_start = label_list.cur_index - 2;
		p_data = label_list.label[index_start].data;
		len = label_list.label[index_start].len;
		if (len > LABEL_SIZE_MAX)
		{
			len = LABEL_SIZE_MAX;
		}
		for (i = 0; i < len; i++)
		{
			sprintf(label1 + strlen(label1), "%02X", p_data[i]);
		}

		p_data = label_list.label[index_start + 1].data;
		len = label_list.label[index_start + 1].len;
		if (len > LABEL_SIZE_MAX)
		{
			len = LABEL_SIZE_MAX;
		}
		for (i = 0; i < len; i++)
		{
			sprintf(label2 + strlen(label2), "%02X", p_data[i]);
		}
	}
	else if (label_list.label_cnt > 0)
	{
		index_start = 0;
		p_data = label_list.label[index_start].data;
		len = label_list.label[index_start].len;
		if (len > LABEL_SIZE_MAX)
		{
			len = LABEL_SIZE_MAX;
		}
		for (i = 0; i < len; i++)
		{
			sprintf(label1 + strlen(label1), "%02X", p_data[i]);
		}
	}
	else
	{
		return -1;
	}

	label_list.label_cnt = 0;
	label_list.msg_send_flag = 0;

	return 0;
}

//the same:1   NO:0
//
int chk_label_in_list(u8 *data, u8 len)
{
	int i, j;
	label_t *p_label;
	for (i = 0; i < LABEL_LIST_SIZE; i++)
	{
		p_label = &(label_list.label[i]);
		if (p_label->len == len)
		{
			for (j = 0; j < len; j++)
			{
				if (p_label->data[j] != data[j])
				{
					break;
				}
			}

			if (j == len)  //
			{
				return 1;
			}
		}
	}
	return 0;
}

int clean_list(void)
{
	memset(&label_list, 0, sizeof(label_list_t));
	return 0;
}

int led_dev_open(int *p_fd_led)
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

	*p_fd_led = fd_com;
	return 0;
}

int led_print_item_ID(int fd_led, int item_id)
{
	u8 *p_send;
	int ret;

	p_send = led_frame_src.buf;
	memset(&led_frame_src, 0, sizeof(LED_frame_s));

	*p_send++ = 0x55;
	//cmd
	*p_send++ = 0x44;
	*p_send++ = 0x00;
	//srcADDR
	*p_send++ = 0x0;
	//dstADDR
	*p_send++ = 0x0;
	//item_id
	*(u32*) p_send = item_id;
	p_send += 4;

	led_frame_src.len = p_send - led_frame_src.buf;
	Modbus_CRC_attach(&led_frame_src);
	led_frame_src.buf[led_frame_src.len++] = 0xAA;
	led_frame_attach(&led_frame_src, &led_frame_send);
	tcflush(fd_led, TCIOFLUSH);

	write(fd_led, led_frame_send.buf, led_frame_send.len);

	ret = led_get_respond_package(fd_led, 200);

	return ret;
}

int led_print_string(int fd_led, int item_id, char *str)
{
	u8 *p_send;
	int str_len;
	int ret;

	p_send = led_frame_src.buf;
	if( NULL == str)
	{
		syslog(LOG_DEBUG, "%s",__LINE__);
		return -1;
	}

	memset(&led_frame_src, 0, sizeof(LED_frame_s));

	*p_send++ = 0x55;
	//cmd
	*p_send++ = 0x1D;
	*p_send++ = 0x00;
	//srcADDR
	*p_send++ = 0x0;
	//dstADDR
	*p_send++ = 0x0;
	//item_id
	*(u32*) p_send = item_id;
	p_send += 4;

	str_len = strlen(str);
	if ((p_send - led_frame_src.buf) + str_len >= 990)
	{
		syslog(LOG_DEBUG, "%s",__LINE__);
		return -1;
	}
	str_len += 1;
	memcpy(p_send, str, str_len);

	if (0 != (str_len % 4))
	{
		p_send += (str_len + 4 - (str_len % 4));
	}
	else
	{
		p_send += str_len;
	}

	led_frame_src.len = p_send - led_frame_src.buf;
	Modbus_CRC_attach(&led_frame_src);
	led_frame_src.buf[led_frame_src.len++] = 0xAA;
	led_frame_attach(&led_frame_src, &led_frame_send);

	tcflush(fd_led, TCIOFLUSH);

	write(fd_led, led_frame_send.buf, led_frame_send.len);
	syslog(LOG_DEBUG, "package len = %d\n", led_frame_send.len);
        
        char szTmpData[5 + 1] = {0};
        std::string strLogData = "Send Data:";
        for (int i = 0; i < led_frame_send.len; ++i)
        {
            memset(szTmpData, 0x00, 5);
            sprintf(szTmpData, "%02X ", led_frame_send.buf[i]);
            strLogData += szTmpData;
        }
        syslog(LOG_DEBUG, "%s\n", strLogData.c_str());
        
	ret = led_get_respond_package(fd_led, 200);

	return ret;
}


static int led_frame_attach(LED_frame_s *p_src, LED_frame_s *p_dst)
{
	int len = 0;
	int i;

	p_dst->buf[len++] = p_src->buf[0];
	for (i = 1; i < p_src->len - 1; i++)
	{
		if (0x55 == p_src->buf[i])
		{
			p_dst->buf[len] = 0xBB;
			len++;
			p_dst->buf[len] = 0x56;
		}
		else if (0xAA == p_src->buf[i])
		{
			p_dst->buf[len] = 0xBB;
			len++;
			p_dst->buf[len] = 0xAB;
		}
		else if (0xBB == p_src->buf[i])
		{
			p_dst->buf[len] = 0xBB;
			len++;
			p_dst->buf[len] = 0xBC;
		}
		else
		{
			p_dst->buf[len] = p_src->buf[i];
		}
		len++;
	}
	p_dst->buf[len++] = p_src->buf[p_src->len - 1];
	p_dst->len = len;

//	memcpy(p_dst, p_src, sizeof(LED_frame_s));
	return 0;
}

static int led_frame_check(LED_frame_s *p_src, LED_frame_s *p_dst)
{
	int len = 0;
	int i;
	int flag = 0;

	p_dst->buf[len++] = p_src->buf[0];
	for (i = 1; i < p_src->len - 1; i++)
	{
		if (flag)
		{
			flag = 0;
			switch (p_src->buf[i])
			{
			case 0x56:
				p_dst->buf[len] = 0x55;
				break;
			case 0xAB:
				p_dst->buf[len] = 0xAA;
				break;
			case 0xBC:
				p_dst->buf[len] = 0xBB;
				break;
			default:
				break;
			}
			len++;
		}
		else
		{
			if (0xBB == p_src->buf[i])
			{
				flag = 1;
			}
			else
			{
				p_dst->buf[len] = p_src->buf[i];
				len++;
			}
		}

	}
	p_dst->buf[len++] = p_src->buf[p_src->len - 1];
	p_dst->len = len;

	return 0;
}

int led_get_respond_package(int fd_led, int time_out)
{
	int ret;
	int r_len = 0;
	int state = 0;
	int start_time_ms;

	u8 *p_recv = led_frame_recv.buf;

	start_time_ms = get_system_ms();
	while (1)
	{
		ret = read(fd_led, p_recv, 1);
		if (ret > 0)
		{
			if (0x55 == p_recv[0])
			{
				r_len++;
				break;
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
			return -99;
		}
	}

	while (1)
	{
		ret = read(fd_led, p_recv + r_len, 1);

		if (ret > 0)
		{
			if (0xAA == p_recv[r_len])
			{
				r_len++;
				break;
			}
			else
			{
				r_len += ret;
			}
		}
		else
		{
			usleep(1000);
		}

		time_out--;

		if (time_out <= 0)
		{
			return -99;
		}
	}

	led_frame_recv.len = r_len;
	led_frame_check(&led_frame_recv, &led_frame_src);

	if (Modbus_CRC_check(&led_frame_src))
	{
		syslog(LOG_DEBUG, "crc error! len = %d\n", led_frame_src.len);
		return -3;
	}

	return 0;
}
