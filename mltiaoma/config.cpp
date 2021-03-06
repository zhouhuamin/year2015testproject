#include "includes.h"
#include "config.h"
#include "packet_format.h"
#include "port.h"

sSystem_setting gSetting_system;
sRealdata_system gReal_system;
sRealdata_system gReal_system_keyboard;
sRealdata_system gReal_system_barcode;
DEV_UNIT_t dev_unit =
{ 0, 0 };

//参数初始化
int parame_init(void)
{
    int ret;

    memset(&gSetting_system, 0, sizeof(sSystem_setting));
    gSetting_system.server_port = 0;
    gSetting_system.com_port = 0;
    gSetting_system.keeplive_gap = 0;

    ret = load_setting_from_xml(&gSetting_system);
    if (ret < 0)
    {
        syslog(LOG_DEBUG, "load setting error!\n");
        //set_parame_default();
        //save_setting_to_xml(&gSetting_system);
        return -1;
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
//    strcpy(gSetting_system.server_ip, SERVER_IP_ADDRESS_STR);
//    gSetting_system.server_port = SERVER_NET_PORT;
//
//    strcpy(gSetting_system.publisher_server_ip, "127.0.0.1");
//    gSetting_system.publisher_server_port = 7766;
//
//    strcpy(gSetting_system.com_ip, NET_COMM_IP_ADDRESS);
//    gSetting_system.com_port = NET_COMM_PORT;
//    gSetting_system.keeplive_gap = 5000;
//
//    strcpy(gSetting_system.register_DEV_TAG, REGISTER_DEV_TAG);
//    strcpy(gSetting_system.register_DEV_ID, REGISTER_DEV_ID);
//    strcpy(gSetting_system.SYS_CTRL_start_event_ID, SYS_CTRL_START_EVENT_ID);
//    strcpy(gSetting_system.SYS_CTRL_stop_event_ID, SYS_CTRL_STOP_EVENT_ID);
//
//    strcpy(gSetting_system.gather_data_tag, GATHER_DATA_DEV_TAG);
//    strcpy(gSetting_system.gather_data_event_id, GATHER_DATA_EVENT_ID);
//    strcpy(gSetting_system.gather_arrived_event_id, GATHER_ARRIVED_EVENT_ID);   
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
        syslog(LOG_DEBUG, "open xml file error!\n");
        return -1;
    }

    root_node = xmlDocGetRootElement(doc);
    if ( NULL == root_node)
    {
        syslog(LOG_DEBUG, "get the root node error!\n");
        xmlFreeDoc(doc);
        return -1;
    }

    if (xmlStrcmp(root_node->name, BAD_CAST "CONFIG_DEV"))
    {
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
                syslog(LOG_DEBUG, "load keeplive gap error!\n");
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
                syslog(LOG_DEBUG, "load lcd com ip port error!\n");
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
            syslog(LOG_DEBUG, "load lcd com port port error!\n");
        }
        xmlFree(str_tmp);
    }
    else if (!xmlStrcmp(cur_node->name, BAD_CAST "REGISTER_DEV_TAG"))
    {
        str_tmp = xmlNodeGetContent(cur_node);
        if (0 != str_tmp)
        {
            sscanf((char*) str_tmp, "%s",gSetting_system.register_DEV_TAG);
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
    else if (!xmlStrcmp(cur_node->name, BAD_CAST "SYS_CTRL_START_EVENT_ID"))
    {
        str_tmp = xmlNodeGetContent(cur_node);
        sscanf((char *) str_tmp, "%s", gSetting_system.SYS_CTRL_start_event_ID);
        xmlFree(str_tmp);
    }
    else if (!xmlStrcmp(cur_node->name,	BAD_CAST "SYS_CTRL_STOP_EVENT_ID"))
    {
        str_tmp = xmlNodeGetContent(cur_node);
        sscanf((char *) str_tmp, "%s", gSetting_system.SYS_CTRL_stop_event_ID);
        xmlFree(str_tmp);
    }
    else if (!xmlStrcmp(cur_node->name, BAD_CAST "GATHER_DATA_EVENT_ID"))
    {
        str_tmp = xmlNodeGetContent(cur_node);
        sscanf((char *) str_tmp, "%s", gSetting_system.gather_data_event_id);
        xmlFree(str_tmp);
    }
    else if (!xmlStrcmp(cur_node->name, BAD_CAST "GATHER_DATA_DEV_TAG"))
    {
        str_tmp = xmlNodeGetContent(cur_node);
        sscanf((char *) str_tmp, "%s", gSetting_system.gather_data_tag);
        xmlFree(str_tmp);
    }
    else if (!xmlStrcmp(cur_node->name, BAD_CAST "GATHER_ARRIVED_EVENT_ID"))
    {
        str_tmp = xmlNodeGetContent(cur_node);
        sscanf((char *) str_tmp, "%s", gSetting_system.gather_arrived_event_id);
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

    sprintf(str_tmp, "%s", gSetting_system.register_DEV_TAG);
    xmlNewChild(root_node, NULL, BAD_CAST "REGISTER_DEV_TAG",
    BAD_CAST (str_tmp));

    sprintf(str_tmp, "%s", gSetting_system.register_DEV_ID);
    xmlNewChild(root_node, NULL, BAD_CAST "REGISTER_DEV_ID",
    BAD_CAST (str_tmp));

    sprintf(str_tmp, "%s", gSetting_system.SYS_CTRL_start_event_ID);
    xmlNewChild(root_node, NULL, BAD_CAST "SYS_CTRL_START_EVENT_ID",
    BAD_CAST (str_tmp));

    sprintf(str_tmp, "%s", gSetting_system.SYS_CTRL_stop_event_ID);
	xmlNewChild(root_node, NULL, BAD_CAST "SYS_CTRL_STOP_EVENT_ID",
	BAD_CAST (str_tmp));

    sprintf(str_tmp, "%s", gSetting_system.gather_data_tag);
    xmlNewChild(root_node, NULL, BAD_CAST "GATHER_DATA_DEV_TAG",
    BAD_CAST (str_tmp));

    sprintf(str_tmp, "%s", gSetting_system.gather_data_event_id);
    xmlNewChild(root_node, NULL, BAD_CAST "GATHER_DATA_EVENT_ID",
    BAD_CAST (str_tmp));

    sprintf(str_tmp, "%s", gSetting_system.gather_arrived_event_id);
    xmlNewChild(root_node, NULL, BAD_CAST "GATHER_ARRIVED_EVENT_ID",
    BAD_CAST (str_tmp));

    ret = xmlSaveFormatFileEnc(SYSTEM_SETTING_CML_FILE_NAME, doc, "utf-8",
                    XML_PARSE_RECOVER);

    xmlFreeDoc(doc);
    xmlCleanupParser();
    xmlMemoryDump();
    /**********************************XML END*********************************/

    return ret;
}
