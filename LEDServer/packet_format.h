#pragma once
#include <string>

#define MAX_MSG_BODY_LEN  (0x200)

#define MSG_TYPE_DEV_REGISTER		0x01
#define MSG_TYPE_DEV_REGISTER_ACK	0x02
#define MSG_TYPE_KEEP_LIVE			0x03
#define MSG_TYPE_KEEP_LIVE_ACK		0x04
#define MSG_TYPE_CTRL_EVENT			0x09
#define MSG_TYPE_GATHER_DATA		0x05

#define NET_PACKET_HEAD_STRING      "0XJZTECH"
#define NET_PACKET_HEAD_STRING_LEN  8

#define NET_PACKET_TAIL_STRING      "NJTECHJZ"
#define NET_PACKET_TAIL_STRING_LEN  8

typedef struct
{
	int msg_type;
	int packet_len;
	char szVersion[16];  //packet version
	int proxy_count;
	int net_proxy[10];
} net_packet_head_t;

typedef struct
{
	char dev_tag[32];
	char dev_id[32];
} NVP_REGISTER;

typedef struct
{
	int reg_result;
	int next_action;
} NVP_REGISTER_ACK;

typedef struct
{
	char event_id[32];
	int xml_data_len;
	char xml_data[MAX_MSG_BODY_LEN - 36];
} NET_SYS_CTRL;

typedef struct
{
	char event_id[32];
	int is_data_full;
	char dev_tag[32];
	int xml_data_len;
	char xml_data[MAX_MSG_BODY_LEN - 72];
} NET_gather_data;

typedef struct
{
	char str_head[8];
	net_packet_head_t msg_head;
	u8 msg_body[MAX_MSG_BODY_LEN];
	char str_tail[8];
} net_packet_t;

struct structLedMsg
{
    u32         id;
    std::string strMsg;
    structLedMsg()
    {
        id      = 0;
        strMsg  = "";
    }
};


int net_dev_register_handle(int server_fd);
int net_dev_keeplive_handle(int server_fd);
int server_read_timeout(int server_fd, u8 *buf, int len, int time_out);
void *net_handle_thread(void *arg);
int connect_server(const char *host_name, int port_id);
int upload_data_handle(int server_fd);
