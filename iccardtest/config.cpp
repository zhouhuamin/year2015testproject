#include "includes.h"
#include "config.h"
#include "packet_format.h"
#include "port.h"

sSystem_setting gSetting_system;
sRealdata_system gReal_system;
DEV_UNIT_t dev_unit =
{ 0, 0 };

//static int com_parity_check(char *p_str);

//参数初始�?
int parame_init(void)
{
	int ret;

	memset(&gSetting_system, 0, sizeof(sSystem_setting));
	gSetting_system.server_port = -1;
	gSetting_system.com_port = -1;
	gSetting_system.DEV_scan_gap = -1;

	ret = load_setting_from_xml(&gSetting_system);
	if (ret < 0)
	{
		//LOG("load setting error!\n");
		syslog(LOG_DEBUG, "load setting error!\n");
	}

	system_setting_verify();

	gReal_system.dev_register_flag = 0;
	gReal_system.work_flag = 0;

	msg_queue_init();
	log_init();
	return 0;
}

int load_setting_from_xml(sSystem_setting *p_setting)
{
	xmlDocPtr doc;
	xmlNodePtr root_node, cur_node;
	xmlChar *str_tmp;

	doc = xmlReadFile(SYSTEM_SETTING_CML_FILE_NAME, "utf-8", XML_PARSE_RECOVER);
	if ( NULL == doc)
	{
		//LOG("open xml file error!\n");
		syslog(LOG_DEBUG, "open xml file error!\n");
		return -1;
	}

	root_node = xmlDocGetRootElement(doc);
	if ( NULL == root_node)
	{
		//LOG("get the root node error!\n");
		syslog(LOG_DEBUG, "get the root node error!\n");
		xmlFreeDoc(doc);
		return -1;
	}

	if (xmlStrcmp(root_node->name, BAD_CAST "CONFIG_DEV"))
	{
		//LOG("the root node name  error!\n");
		syslog(LOG_DEBUG, "the root node name  error!\n");
		xmlFreeDoc(doc);
		return -2;
	}

	cur_node = root_node->children;
	while ( NULL != cur_node)
	{
		if (!xmlStrcmp(cur_node->name, BAD_CAST "SERVER_IP"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == ip_address_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%s", gSetting_system.server_ip);
			}
			else
			{
				//LOG("load server ip error!\n");
				syslog(LOG_DEBUG, "load server ip error!\n");
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "SERVER_PORT"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == interger_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%d", &gSetting_system.server_port);
			}
			else
			{
				//LOG("load server port error!\n");
				syslog(LOG_DEBUG, "load server port error!\n");
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "PUBLISHER_SERVER_IP"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == ip_address_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%s", gSetting_system.publisher_server_ip);
			}
			else
			{
				//LOG("load server ip error!\n");
				syslog(LOG_DEBUG, "load publisher server ip error!\n");
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "PUBLISHER_SERVER_PORT"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == interger_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%d", &gSetting_system.publisher_server_port);
			}
			else
			{
				//LOG("load server port error!\n");
				syslog(LOG_DEBUG, "load publisher server port error!\n");
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "DEV_SCAN_GAP"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == interger_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%d", &gSetting_system.DEV_scan_gap);
			}
			else
			{
				//LOG("load RFID scan gap error!\n");
				syslog(LOG_DEBUG, "load RFID scan gap error!\n");
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "DEV_COM_IP"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == ip_address_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%s", gSetting_system.com_ip);
			}
			else
			{
				//LOG("load rfid com ip port error!\n");
				syslog(LOG_DEBUG, "load rfid com ip port error!\n");
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "DEV_COM_PORT"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == interger_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%d", &gSetting_system.com_port);
			}
			else
			{
				//LOG("load rfid com port port error!\n");
				syslog(LOG_DEBUG, "load rfid com port port error!\n");
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "REGISTER_DEV_TAG"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 != str_tmp)
			{
				sscanf((char *) str_tmp, "%s",
						gSetting_system.register_DEV_TAG);
			}
			else
			{
				//LOG("load reg dev tag port error!\n");
				syslog(LOG_DEBUG, "load reg dev tag port error!\n");
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "REGISTER_DEV_ID"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			sscanf((char *) str_tmp, "%s", gSetting_system.register_DEV_ID);
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name,
		BAD_CAST "SYS_CTRL_R_START_EVENT_ID"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			sscanf((char *) str_tmp, "%s",
					gSetting_system.SYS_CTRL_R_start_event_ID);

			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name,
		BAD_CAST "SYS_CTRL_R_STOP_EVENT_ID"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			sscanf((char *) str_tmp, "%s",
					gSetting_system.SYS_CTRL_R_stop_event_ID);

			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "GATHER_DATA_EVENT_ID"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			sscanf((char *) str_tmp, "%s", gSetting_system.gather_event_id);

			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "GATHER_DATA_DEV_TAG"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			sscanf((char *) str_tmp, "%s", gSetting_system.gather_DEV_TAG);

			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "ARRIVED_DEV_EVENT_ID"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			sscanf((char *) str_tmp, "%s",
					gSetting_system.arrived_DEV_event_id);

			xmlFree(str_tmp);
		}
        
		cur_node = cur_node->next;
	}
	xmlFreeDoc(doc);
	return 0;
}