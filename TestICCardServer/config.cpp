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
	gSetting_system.keeplive_gap = -1;
	gSetting_system.DEV_scan_gap = -1;
	gSetting_system.DEV_scan_duration_time = -1;
	gSetting_system.RFID_label_type = -1;
	gSetting_system.IC_read_block_NO = -1;

	gSetting_system.WT_protocol_head_len = -1;
	gSetting_system.WT_threshold_value = -1;
	gSetting_system.WT_protocol_tail_len = -1;
	gSetting_system.WT_threshold_value = -1;
	gSetting_system.WT_average_diff_value = -1;

	ret = load_setting_from_xml(&gSetting_system);
	if (ret < 0)
	{
		//LOG("load setting error!\n");
		syslog(LOG_DEBUG, "load setting error!\n");

		set_parame_default();

		save_setting_to_xml(&gSetting_system);
	}

	system_setting_verify();

	gReal_system.dev_register_flag = 0;
	gReal_system.work_flag = 0;

	msg_queue_init();
	log_init();
	return 0;
}

//将参数设置到默认状�?
int set_parame_default(void)
{
	strcpy(gSetting_system.server_ip, SERVER_IP_ADDRESS_STR);
	gSetting_system.server_port = SERVER_NET_PORT;

	strcpy(gSetting_system.publisher_server_ip, "127.0.0.1");
	gSetting_system.publisher_server_port = 7766;

	strcpy(gSetting_system.com_ip, NET_COMM_IP_ADDRESS);
	gSetting_system.com_port = NET_COMM_PORT;

	gSetting_system.DEV_scan_gap = 200;
	gSetting_system.keeplive_gap = 5000;

	gSetting_system.DEV_scan_duration_time = 0;

	strcpy(gSetting_system.register_DEV_TAG, REGISTER_DEV_TAG);
	strcpy(gSetting_system.register_DEV_ID, REGISTER_DEV_ID);
	strcpy(gSetting_system.SYS_CTRL_R_start_event_ID,
	SYS_CTRL_R_START_EVENT_ID);
	strcpy(gSetting_system.SYS_CTRL_R_stop_event_ID, SYS_CTRL_R_STOP_EVENT_ID);

	strcpy(gSetting_system.gather_event_id, GATHER_DATA_EVENT_ID);
	strcpy(gSetting_system.gather_DEV_TAG, GATHER_DATA_DEV_TAG);
	strcpy(gSetting_system.arrived_DEV_event_id, ARRIVED_DEV_EVENT_ID);
	strcpy(gSetting_system.onload_event_id, ONLOAD_DEV_DEV_TAG);

	gSetting_system.RFID_label_type = 0;
	gSetting_system.IC_read_block_NO = 1;
	gSetting_system.WT_protocol_head_len = 1;
	gSetting_system.WT_protocol_head[0] = 0x20;
	gSetting_system.WT_protcol_value_bytes = 6;
	gSetting_system.WT_protocol_tail_len = 2;
	gSetting_system.WT_protocol_tail[0] = 0x3D;
	gSetting_system.WT_protocol_tail[1] = 0xFE;

	gSetting_system.WT_threshold_value = 2000;
	gSetting_system.WT_average_diff_value = 20;
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
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "KEEPLIVE_GAP"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == interger_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%d", &gSetting_system.keeplive_gap);
			}
			else
			{
				//LOG("load keeplive gap error!\n");
				syslog(LOG_DEBUG, "load keeplive gap error!\n");
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
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "DEV_SCAN_DURATION_TIME"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == interger_check((char *) str_tmp))
			{
				sscanf((char *) str_tmp, "%d",
						&gSetting_system.DEV_scan_duration_time);
			}
			else
			{
				//LOG("load scan duration time error!\n");
				syslog(LOG_DEBUG, "load scan duration time error!\n");
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
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "ONLOAD_DEV_EVENT_ID"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			sscanf((char *) str_tmp, "%s", gSetting_system.onload_event_id);

			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "RFID_LABEL_TYPE"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == strcmp((char*) str_tmp, "ISO1800-6B"))
			{
				gSetting_system.RFID_label_type = 1;
			}
			else
			{
				gSetting_system.RFID_label_type = 0;
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "IC_READ_BLOCK_NO"))
		{
			str_tmp = xmlNodeGetContent(cur_node);
			if (0 == interger_check((char*) str_tmp))
			{
				sscanf((char *) str_tmp, "%d",
						&gSetting_system.IC_read_block_NO);
			}
			else
			{
				gSetting_system.IC_read_block_NO = 1;
			}
			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "WT_DEV_PROTOCOL_HEAD"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			str_to_data(gSetting_system.WT_protocol_head,
					&(gSetting_system.WT_protocol_head_len), (char*) str_tmp);

			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "WT_DEV_VALUE_BYTES"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			sscanf((char *) str_tmp, "%d",
					&gSetting_system.WT_protcol_value_bytes);

			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "WT_DEV_PROTOCOL_TAIL"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			str_to_data(gSetting_system.WT_protocol_tail,
					&gSetting_system.WT_protocol_tail_len, (char*) str_tmp);

			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "WT_THRESHOLD_VALUE"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			sscanf((char *) str_tmp, "%d", &gSetting_system.WT_threshold_value);

			xmlFree(str_tmp);
		}
		else if (!xmlStrcmp(cur_node->name, BAD_CAST "WT_AVERAGE_DIFF_VALUE"))
		{
			str_tmp = xmlNodeGetContent(cur_node);

			sscanf((char *) str_tmp, "%d",
					&gSetting_system.WT_average_diff_value);

			xmlFree(str_tmp);
		}
		cur_node = cur_node->next;
	}
	xmlFreeDoc(doc);
	return 0;
}

int save_setting_to_xml(sSystem_setting *p_setting)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;
	int ret;
	char str_tmp[50];

	/**********************************XML*********************************/
	doc = xmlNewDoc(BAD_CAST "1.0");

	root_node = xmlNewNode(NULL, BAD_CAST "CONFIG_DEV");

	xmlDocSetRootElement(doc, root_node);

	sprintf(str_tmp, "%s", gSetting_system.server_ip);
	xmlNewChild(root_node, NULL, BAD_CAST "SERVER_IP", BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.server_port);
	xmlNewChild(root_node, NULL, BAD_CAST "SERVER_PORT", BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.publisher_server_ip);
	xmlNewChild(root_node, NULL, BAD_CAST "PUBLISHER_SERVER_IP", BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.publisher_server_port);
	xmlNewChild(root_node, NULL, BAD_CAST "PUBLISHER_SERVER_PORT", BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.com_ip);
	xmlNewChild(root_node, NULL, BAD_CAST "DEV_COM_IP", BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.com_port);
	xmlNewChild(root_node, NULL, BAD_CAST "DEV_COM_PORT", BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.keeplive_gap);
	xmlNewChild(root_node, NULL, BAD_CAST "KEEPLIVE_GAP", BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.DEV_scan_gap);
	xmlNewChild(root_node, NULL, BAD_CAST "DEV_SCAN_GAP", BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.DEV_scan_duration_time);
	xmlNewChild(root_node, NULL, BAD_CAST "DEV_SCAN_DURATION_TIME",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.register_DEV_TAG);
	xmlNewChild(root_node, NULL, BAD_CAST "REGISTER_DEV_TAG",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.register_DEV_ID);
	xmlNewChild(root_node, NULL, BAD_CAST "REGISTER_DEV_ID",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.SYS_CTRL_R_start_event_ID);
	xmlNewChild(root_node, NULL, BAD_CAST "SYS_CTRL_R_START_EVENT_ID",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.SYS_CTRL_R_stop_event_ID);
	xmlNewChild(root_node, NULL, BAD_CAST "SYS_CTRL_R_STOP_EVENT_ID",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.gather_event_id);
	xmlNewChild(root_node, NULL, BAD_CAST "GATHER_DATA_EVENT_ID",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.gather_DEV_TAG);
	xmlNewChild(root_node, NULL, BAD_CAST "GATHER_DATA_DEV_TAG",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.arrived_DEV_event_id);
	xmlNewChild(root_node, NULL, BAD_CAST "ARRIVED_DEV_EVENT_ID",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s", gSetting_system.onload_event_id);
	xmlNewChild(root_node, NULL, BAD_CAST "ONLOAD_DEV_EVENT_ID",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%s",
			(gSetting_system.RFID_label_type != 0) ?
					("ISO1800-6B") : ("ISO1800-6C"));
	xmlNewChild(root_node, NULL, BAD_CAST "RFID_LABEL_TYPE",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.IC_read_block_NO);
	xmlNewChild(root_node, NULL, BAD_CAST "IC_READ_BLOCK_NO",
	BAD_CAST (str_tmp));

	data_to_str(str_tmp, gSetting_system.WT_protocol_head,
			gSetting_system.WT_protocol_head_len);
	xmlNewChild(root_node, NULL, BAD_CAST "WT_DEV_PROTOCOL_HEAD",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.WT_protcol_value_bytes);
	xmlNewChild(root_node, NULL, BAD_CAST "WT_DEV_VALUE_BYTES",
	BAD_CAST (str_tmp));

	data_to_str(str_tmp, gSetting_system.WT_protocol_tail,
			gSetting_system.WT_protocol_tail_len);
	xmlNewChild(root_node, NULL, BAD_CAST "WT_DEV_PROTOCOL_TAIL",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.WT_threshold_value);
	xmlNewChild(root_node, NULL, BAD_CAST "WT_THRESHOLD_VALUE",
	BAD_CAST (str_tmp));

	sprintf(str_tmp, "%d", gSetting_system.WT_average_diff_value);
	xmlNewChild(root_node, NULL, BAD_CAST "WT_AVERAGE_DIFF_VALUE",
	BAD_CAST (str_tmp));

	ret = xmlSaveFormatFileEnc(SYSTEM_SETTING_CML_FILE_NAME, doc, "utf-8",
			XML_PARSE_RECOVER);

	xmlFreeDoc(doc);

	xmlCleanupParser();

	xmlMemoryDump();
	/**********************************XML END*********************************/

	return ret;
}

//static int com_parity_check(char *p_str)
//{
//	int len;
//
//	if ( NULL == p_str)
//	{
//		return -1;
//	}
//
//	len = strlen(p_str);
//	if (1 != len)
//	{
//		LOG_M("len = %d\n", len);
//		return -2;
//	}
//
//	if ((p_str[0] == 'N') || (p_str[0] == 'n') || (p_str[0] == 'O')
//			|| (p_str[0] == 'o') || (p_str[0] == 'E') || (p_str[0] == 'e'))
//	{
//
//	}
//	else
//	{
//		return -3;
//	}
//
//	return 0;
//}

