#include "includes.h"
#include "reader1000api.h"
#include "port.h"
#include "broadcast.h"

char label1[200];
char label2[200];

char led_cur_str[0x400] = "Welcome";

int dev_open(int *pFd_dev)
{
	return broadcast_dev_open(pFd_dev);
}

int dev_connect(int fd_dev)
{
	return broadcast_item_ID(fd_dev, 10000);
}


void system_setting_verify(void)
{
	if (0 != ip_address_check(gSetting_system.server_ip))
	{
		syslog(LOG_DEBUG, "server IP address error!\nPlease check <SERVER_IP>\n");
		exit(-1);
	}

	if (0 != ip_address_check(gSetting_system.com_ip))
	{
		syslog(LOG_DEBUG, "DEV IP address error!\nPlease check <DEV_COM_IP>\n");
		exit(-1);
	}

	if (0 >= gSetting_system.server_port)
	{
		syslog(LOG_DEBUG, "server PORT address error!\nPlease check <SERVER_PORT>\n");
		exit(-1);
	}

	if (0 >= gSetting_system.com_port)
	{
		syslog(LOG_DEBUG, "DEV PORT address error!\nPlease check <DEV_COM_PORT>\n");
		exit(-1);
	}

	if (0 > gSetting_system.keeplive_gap)
	{
		syslog(LOG_DEBUG, "DEV KEEPLIVE GAP TIME error!\nPlease check <KEEPLIVE_GAP>\n");
		exit(-1);
	}

//	if (0 > gSetting_system.DEV_scan_gap)
//	{
//		perror("DEV SCAN GAP TIME error!\nPlease check <DEV_SCAN_GAP>\n");
//		exit(-1);
//	}

//	if (0 > gSetting_system.DEV_scan_duration_time)
//	{
//		perror(
//				"DEV DURATION TIME error!\nPlease check <DEV_SCAN_DURATION_TIME>\n");
//		exit(-1);
//	}

	if (1 > strlen(gSetting_system.register_DEV_TAG))
	{
		syslog(LOG_DEBUG, "REGISTER DEV TAG error!\nPlease check <REGISTER_DEV_TAG>\n");
		exit(-1);
	}

	if (1 > strlen(gSetting_system.register_DEV_ID))
	{
		syslog(LOG_DEBUG, "REGISTER DEV ID error!\nPlease check <REGISTER_DEV_ID>\n");
		exit(-1);
	}

	if (1 > strlen(gSetting_system.SYS_CTRL_R_start_event_ID))
	{
		syslog(LOG_DEBUG, 
				"SYS CTRL START error!\nPlease check <SYS_CTRL_R_START_EVENT_ID>\n");
		exit(-1);
	}

	if (0 > gSetting_system.Broadcast_volume)
	{
		syslog(LOG_DEBUG, 
				"BROADCAST VOLUME error!\nPlease check <BROADCAST_VOLUME>\n");
		exit(-1);
	}

//	if (1 > strlen(gSetting_system.SYS_CTRL_R_stop_event_ID))
//	{
//		perror(
//				"SYS CTRL STOP error!\nPlease check <SYS_CTRL_R_STOP_EVENT_ID>\n");
//		exit(-1);
//	}
//
//	if (1 > strlen(gSetting_system.gather_event_id))
//	{
//		perror("GATHER DEV ID error!\nPlease check <GATHER_DATA_EVENT_ID>\n");
//		exit(-1);
//	}
//
//	if (1 > strlen(gSetting_system.gather_DEV_TAG))
//	{
//		perror("GATHER DEV TAG error!\nPlease check <GATHER_DATA_DEV_TAG>\n");
//		exit(-1);
//	}
//
//	if (1 > strlen(gSetting_system.arrived_DEV_event_id))
//	{
//		perror(
//				"ARRIVED DEV EVENT ID error!\nPlease check <ARRIVED_DEV_EVENT_ID>\n");
//		exit(-1);
//	}
//
//	if (1 > strlen(gSetting_system.onload_event_id))
//	{
//		perror("ONLOAD EVENT ID error!\nPlease check <ONLOAD_DEV_EVENT_ID>\n");
//		exit(-1);
//	}
//
//	if (0 > gSetting_system.RFID_label_type)
//	{
//		perror("RFID LABEL TYPE error!\nPlease check <RFID_LABEL_TYPE>\n");
//		exit(-1);
//	}
//
//	if (0 > gSetting_system.IC_read_block_NO)
//	{
//		perror("IC CARD BLOCK NO error!\nPlease check <IC_READ_BLOCK_NO>\n");
//		exit(-1);
//	}
//
//	if (1 > gSetting_system.WT_protocol_head_len)
//	{
//		perror(
//				"WT PROTOCOL HEAD NO error!\nPlease check <WT_DEV_PROTOCOL_HEAD>\n");
//		exit(-1);
//	}
//
//	if (1 > gSetting_system.WT_protcol_value_bytes)
//	{
//		perror("WT VALUE BYTES error!\nPlease check <WT_DEV_VA"
//				"LUE_BYTES>\n");
//		exit(-1);
//	}
//
//	if (1 > gSetting_system.WT_protocol_tail_len)
//	{
//		perror(
//				"WT PROTOCOL TAIL NO error!\nPlease check <WT_DEV_PROTOCOL_TAIL>\n");
//		exit(-1);
//	}
//
//	if (0 > gSetting_system.WT_threshold_value)
//	{
//		perror(
//				"WT THRESHOLD VALUE error!\nPlease check <WT_THRESHOLD_VALUE>\n");
//		exit(-1);
//	}
//
//	if (0 > gSetting_system.WT_average_diff_value)
//	{
//		perror(
//				"WT AVERAGE DIFF VALUE error!\nPlease check <WT_AVERAGE_DIFF_VALUE>\n");
//		exit(-1);
//	}
}

int xml_system_ctrl_hook(char *xml_buf, int xml_buf_len)
{
	return broadcast_msg_play(xml_buf, xml_buf_len);
}

int xml_upload_threshold(int server_fd)
{

	return 0;
}

int xml_upload_gather_data(int server_fd)
{
	return 0;
}

int xml_upload_arrived_data(int server_fd)
{
	return 0;
}
