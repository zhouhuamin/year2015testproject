#include "includes.h"
#include "RFID_label.h"
#include "packet_format.h"
#include "config.h"
#include "jzLocker.h"

#include <string>
#include <map>
#include <set>
#include <functional>
#include <algorithm>
#include <iterator>
#include <boost/circular_buffer.hpp>

label_list_t label_list;
std::map<std::string, strutLabelModifiers> g_LabelMap;
locker                              g_LabelMapLock;
boost::circular_buffer<int>         g_WeightQueue(10);
locker                              g_WeightLock;
boost::circular_buffer<int>         g_WeightSet(10);
locker                              g_WeightSetLock;

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
