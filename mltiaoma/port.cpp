#include "includes.h"
#include "port.h"
#include "Barcode.h"

char label1[200];
char label2[200];

char led_cur_str[0x400] = "Welcome";

int dev_open(int *pFd_dev)
{
    return barcode_dev_open(pFd_dev);
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

    if (1 > strlen(gSetting_system.SYS_CTRL_start_event_ID))
    {
        syslog(LOG_DEBUG, 
                        "SYS CTRL START error!\nPlease check <SYS_CTRL_START_EVENT_ID>\n");
        exit(-1);
    }
}

int xml_system_ctrl_hook(char *xml_buf, int xml_buf_len)
{
    return 0;
}

int xml_system_ctrl_data()
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
}

int xml_upload_arrived_data(int server_fd)
{
    return 0;
}
