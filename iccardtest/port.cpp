#include "includes.h"
#include "port.h"
#include "ic_card.h"

extern card_block_t old_card_block;
static char ic_number[200];

int dev_open(int *pFd_dev)
{
	return true;
}

int dev_connect(int fd_dev)
{
	u8 ver[100];
	int len;

	return true;
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

	if (0 > gSetting_system.DEV_scan_gap)
	{
		syslog(LOG_DEBUG, "DEV SCAN GAP TIME error!\nPlease check <DEV_SCAN_GAP>\n");
		exit(-1);
	}

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
		syslog(LOG_DEBUG, "SYS CTRL START error!\nPlease check <SYS_CTRL_R_START_EVENT_ID>\n");
		exit(-1);
	}

	if (1 > strlen(gSetting_system.SYS_CTRL_R_stop_event_ID))
	{
		syslog(LOG_DEBUG, "SYS CTRL STOP error!\nPlease check <SYS_CTRL_R_STOP_EVENT_ID>\n");
		exit(-1);
	}

	if (1 > strlen(gSetting_system.gather_event_id))
	{
		syslog(LOG_DEBUG, "GATHER DEV ID error!\nPlease check <GATHER_DATA_EVENT_ID>\n");
		exit(-1);
	}

	if (1 > strlen(gSetting_system.gather_DEV_TAG))
	{
		syslog(LOG_DEBUG, "GATHER DEV TAG error!\nPlease check <GATHER_DATA_DEV_TAG>\n");
		exit(-1);
	}

	if (1 > strlen(gSetting_system.arrived_DEV_event_id))
	{
		syslog(LOG_DEBUG, "ARRIVED DEV EVENT ID error!\nPlease check <ARRIVED_DEV_EVENT_ID>\n");
		exit(-1);
	}
}

int xml_system_ctrl_hook(char *xml_buf, int xml_buf_len)
{
	return 0;
}

int xml_upload_threshold(int server_fd)
{

	return 0;
}

int xml_upload_gather_data(int server_fd)
{
    return 0;
//	int ret;
//	u8 *p_data;
//	int i;
//
//	p_data = old_card_block.data;
//
//	ic_number[0] = 0;
//	if (gSetting_system.IC_read_block_NO == 1)  //ic card
//	{
//		for (i = 0; i < old_card_block.len; i++)
//		{
//			if (isprint(p_data[i]))
//			{
//				ic_number[i] = p_data[i];
//			}
//			else
//			{
//				ic_number[i] = '\0';
//				break;
//			}
//		}
//		ic_number[i] = '\0';
//	}
//	else
//	{
//		for (i = 0; i < old_card_block.len; i++)
//		{
//			sprintf(ic_number + strlen(ic_number), "%02X", p_data[i]);
//		}
//	}
//
//	return IC_upload_gather_data(server_fd, ic_number);
}

int xml_upload_arrived_data(int server_fd)
{
    return 0;
//	int ret;
//	u8 *p_data;
//	int i;
//
//	p_data = old_card_block.data;
//
//	ic_number[0] = 0;
//	if (gSetting_system.IC_read_block_NO == 1)  //ic card
//	{
//		for (i = 0; i < old_card_block.len; i++)
//		{
//			if (isprint(p_data[i]))
//			{
//				ic_number[i] = p_data[i];
//			}
//			else
//			{
//				ic_number[i] = '\0';
//				break;
//			}
//		}
//		ic_number[i] = '\0';
//	}
//	else
//	{
//		for (i = 0; i < old_card_block.len; i++)
//		{
//			sprintf(ic_number + strlen(ic_number), "%02X", p_data[i]);
//		}
//	}
//
//	return IC_upload_arrived_data(server_fd, ic_number);
}
