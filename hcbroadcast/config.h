#pragma once
#include "includes.h"


typedef struct
{
    char server_ip[20];
    int server_port;

    char com_ip[20];
    int com_port;

    int keeplive_gap;
    char register_DEV_TAG[32];
    char register_DEV_ID[32];
    char SYS_CTRL_start_event_ID[32];
    char SYS_CTRL_stop_event_ID[32];
    char SYS_CTRL_brt_event_ID[32];
    char SYS_CTRL_ring_event_ID[32];
    char gather_event_id[32];
    char gather_DEV_TAG[32];
    char arrived_DEV_event_id[32];
    int Broadcast_volume;
    char publisher_server_ip[20];
    int  publisher_server_port;
} sSystem_setting;

typedef struct
{
    int dev_open_flag;
    int dev_connect_flag;
    int server_connect_flag;
    int server_register_flag;
    int dev_register_flag;   //device register flag
    int work_flag;           //0:wait 1:working
}sRealdata_system;

typedef struct
{
    u32 dev_scan_count;
    int dev_scan_ok_start_systick; //成功扫描到标签时的时间
    u32 dev_keeplive_cnt;
    u32 dev_keeplive_ACK_cnt;
    u32 request_clean_history_cnt;
    u32 read_request_respond_flag;
} DEV_UNIT_t;

// 2015-5-19
struct struDeviceAndServiceStatus
{
    char szLocalIP[32];					// 服务IP地址
    int  nLocalPort;					// 服务端口

    char szUpstreamIP[32];				// 上位服务IP
    int	 nUpstreamPort;				// 上位服务端口号
    char szUpstreamObjectCode[32];	// 上位服务代号
    int  nServiceType;					// 服务类别代码

    char szObjectCode[32];				// 对象代号
    char szObjectStatus[3 + 1];		// 状态值
    char szConnectedObjectCode[32];	// 对接对象代号
    char szReportTime[32];				// 时间戳
};

int parame_init(void);
int load_setting_from_xml(sSystem_setting *p_setting);


extern sSystem_setting gSetting_system;
extern sRealdata_system gReal_system;
extern DEV_UNIT_t dev_unit;

