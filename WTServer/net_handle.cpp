#include "SysMessage.h"
#include "includes.h"
#include "packet_format.h"
#include "starup_handle.h"
#include "port.h"
#include "RFID_label.h"
#include "jzLocker.h"
#include <string>
#include <map>
#include <algorithm>
#include <iterator>

NVP_REGISTER reg_unit;

static net_packet_t net_packet;
static net_packet_head_t packet_head;
static u8 data_buf[0x800];
sem	g_StartEventSem;
sem	g_StoppedEventSem;


int server_read_timeout(int server_fd, u8 *buf, int len, int timeout);
int get_one_packet(int net_fd, u8* buf);
int net_system_CTRL_handle(int server_fd, u8 *buf, int len);

NET_SYS_CTRL sys_ctrl;
extern volatile u32 DEV_request_count;
extern void GetCurTime(char *pTime);
extern struct struDeviceAndServiceStatus g_ObjectStatus;
extern pthread_mutex_t g_StatusMutex;
extern label_list_t label_list;
extern std::map<std::string, strutLabelModifiers> g_LabelMap;
extern locker       		  g_LabelMapLock;

int connect_server(const char *host_name, int port_id)
{
	struct sockaddr_in server_addr;
	struct hostent *host;
	int sock;
	int flag;

	host = gethostbyname(host_name);
	if (NULL == host)
	{
		//LOG("get hostname error!\n");
		syslog(LOG_DEBUG, "get hostname error!\n");
		return -1;
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sock)
	{
		//LOG("socket error:%s\a\n!\n", strerror(errno));
		syslog(LOG_DEBUG, "socket error:%s\a\n!\n", strerror(errno));
		return -2;
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_id);
	server_addr.sin_addr = *((struct in_addr *) host->h_addr);

	if (connect(sock, (struct sockaddr *) (&server_addr),
			sizeof(struct sockaddr_in)) == -1)
	{
		//LOG_M("connect error:%s\n", strerror(errno));
		syslog(LOG_DEBUG, "connect error-3:%s\n", strerror(errno));
		close(sock);
		return -3;
	}

	flag = fcntl(sock, F_GETFL, 0);
	fcntl(sock, F_SETFL, flag | O_NONBLOCK);

	return sock;
}

void *net_handle_thread(void *arg)
{
	volatile int net_disconnected = 0;
	int ret;
	int server_fd2 = 0;

	while (1)
	{
		//ret = get_one_packet(server_fd2, data_buf);

		//if (ret > 0) //get data
		//{
		//	memcpy((void *) &net_packet, data_buf, ret);

		//	switch (net_packet.msg_head.msg_type)
		//	{
		//	//case MSG_TYPE_CTRL_EVENT:
		//	//	net_system_CTRL_handle(server_fd2, net_packet.msg_body,
		//	//			net_packet.msg_head.packet_len);
		//	//	break;
		//	case MSG_TYPE_KEEP_LIVE_ACK:
		//		//if (packet_head.packet_len != 0)
		//		{
		//			//LOG_M("keeplive packet_len =%d\n", packet_head.packet_len);
		//			//syslog(LOG_DEBUG, "keeplive packet_len =%d\n", packet_head.packet_len);
		//			//LOG("keep live trigger ACK!\n");
		//			syslog(LOG_DEBUG, "keep live trigger ACK!\n");
		//			dev_unit.dev_keeplive_ACK_cnt++;

		//			pthread_mutex_lock(&g_StatusMutex);
		//			strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
		//			g_ObjectStatus.nLocalPort		= 0;
		//			strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
		//			g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
		//			strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
		//			g_ObjectStatus.nServiceType	= 0;
		//			strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
		//			strcpy(g_ObjectStatus.szObjectStatus, "000");
		//			strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");

		//			{
		//				char szNowTime[32] = {0};
		//				GetCurTime(szNowTime);
		//				szNowTime[31] = '\0';
		//				strcpy(g_ObjectStatus.szReportTime, szNowTime);
		//			}
		//			pthread_mutex_unlock(&g_StatusMutex);

		//		}
		//		break;
		//	default:
		//		//printf("invalid request! len=%d\n", ret);
		//		syslog(LOG_DEBUG, "invalid request! len=%d\n", ret);
		//		break;
		//	}
		//}
		//else if (-99 == ret) //server close
		//{
		//	//LOG("the server is closed!");
		//	syslog(LOG_DEBUG, "the server is closed!");
		//	net_disconnected = 1;
		//}
		//else
		//{

		//}

		upload_data_handle(server_fd2);

		//ret = net_dev_keeplive_handle(server_fd2);
		//if (ret < 0)
		//{
		//	net_disconnected = 1;
		//}

		//if (0 != net_disconnected)
		//{
		//	close(server_fd2);
		//	net_disconnected = 0;
		//	server_retry_connect_handle(&server_fd2);

		//}
	}

	close(server_fd2);
}

//dev login server

int net_dev_register_handle(int server_fd)
{
	NVP_REGISTER *p_reg;
	NVP_REGISTER_ACK *p_reg_ack;
	char str_tmp[16];
	int w_len, r_len;
	int ret;

	packet_head.msg_type = MSG_TYPE_DEV_REGISTER;
	packet_head.packet_len = sizeof(NVP_REGISTER);
	strcpy(packet_head.szVersion, "1.0");

	strcpy((char *) data_buf, "0XJZTECH");
	memcpy(data_buf + 8, &packet_head, sizeof(net_packet_head_t));
	p_reg = (NVP_REGISTER *) (data_buf + 8 + sizeof(net_packet_head_t));

	strcpy(p_reg->dev_tag, gSetting_system.register_DEV_TAG);
	strcpy(p_reg->dev_id, gSetting_system.register_DEV_ID);

	w_len = sizeof(net_packet_head_t) + sizeof(NVP_REGISTER) + 8;
	strcpy((char *) (data_buf + w_len), "NJTECHJZ");
	w_len += 8;

	//tcflush(server_fd, TCIFLUSH);
	ret = write(server_fd, data_buf, w_len);

	if (ret != w_len)
	{
		//LOG_M("write data error! %d\n", ret);
		syslog(LOG_DEBUG, "write data error! %d\n", ret);
		return -1;
	}

	r_len = 8 + sizeof(net_packet_head_t) + sizeof(NVP_REGISTER_ACK) + 8;
	ret = server_read_timeout(server_fd, data_buf, r_len, 500);
	if (ret != r_len)
	{
		//LOG("get register ack r_len is error! %d\n", ret);
		syslog(LOG_DEBUG, "get register ack r_len is error! %d\n", ret);
		return -2;
	}

	memcpy(str_tmp, data_buf, 8);
	str_tmp[8] = 0;
	if (strcmp(str_tmp, NET_PACKET_HEAD_STRING))
	{
		//LOG("head string check error!\n");
		syslog(LOG_DEBUG, "head string check error!\n");
		return -3;
	}

	memcpy(str_tmp, data_buf + r_len - 8, 8);
	str_tmp[8] = 0;
	if (strcmp(str_tmp, NET_PACKET_TAIL_STRING))
	{
		//LOG("tail string check error!\n");
		syslog(LOG_DEBUG, "tail string check error!\n");
		return -4;
	}

	memcpy(&packet_head, data_buf + 8, sizeof(net_packet_head_t));
	if ((packet_head.msg_type != MSG_TYPE_DEV_REGISTER_ACK)
			|| (packet_head.packet_len != sizeof(NVP_REGISTER_ACK)))
	{
		//LOG_M("packer_len =%d\n", packet_head.packet_len);
		syslog(LOG_DEBUG, "packer_len =%d\n", packet_head.packet_len);
		return -5;

	}

	p_reg_ack = (NVP_REGISTER_ACK*) (data_buf + sizeof(net_packet_head_t) + 8);
	if (0 == p_reg_ack->reg_result)
	{
		gReal_system.dev_register_flag = 1;
		gReal_system.work_flag = p_reg_ack->next_action;
	}
	else
	{
		//LOG("dev register error!\n");
		syslog(LOG_DEBUG, "dev register error!\n");
	}

	//LOG_M("result: %d, action: %d", p_reg_ack->reg_result, p_reg_ack->next_action);
	syslog(LOG_DEBUG, "result: %d, action: %d", p_reg_ack->reg_result,
			p_reg_ack->next_action);
	return 0;
}

int net_dev_keeplive_handle(int server_fd)
{
	static u32 keeplive_trig_systick = 0;
	int w_len;
	int ret;

	if ((get_system_ms() - keeplive_trig_systick)
			< gSetting_system.keeplive_gap)
	{
		return 0;
	}

	if (dev_unit.dev_keeplive_cnt >= 11)
	{
		if (dev_unit.dev_keeplive_ACK_cnt <= 0)
		{
			return -1;
		}
		dev_unit.dev_keeplive_cnt = 0;
		dev_unit.dev_keeplive_ACK_cnt = 0;
	}

	keeplive_trig_systick = get_system_ms();

	packet_head.msg_type = MSG_TYPE_KEEP_LIVE;
	packet_head.packet_len = 0;
	strcpy(packet_head.szVersion, "1.0");

	strcpy((char *) data_buf, NET_PACKET_HEAD_STRING);
	memcpy(data_buf + NET_PACKET_HEAD_STRING_LEN, &packet_head,
			sizeof(net_packet_head_t));

	w_len = sizeof(net_packet_head_t) + NET_PACKET_HEAD_STRING_LEN;

	strcpy((char*) (data_buf + w_len), NET_PACKET_TAIL_STRING);
	w_len += NET_PACKET_TAIL_STRING_LEN;

	tcflush(server_fd, TCIFLUSH);
	ret = write(server_fd, data_buf, w_len);
	if (ret != w_len)
	{
		//LOG_M("write data error! %d\n", ret);
		syslog(LOG_DEBUG, "write data error! %d\n", ret);
	}

	dev_unit.dev_keeplive_cnt++;

	//LOG("keep live trigger!\n");
	syslog(LOG_DEBUG, "keep live trigger!\n");

	return 0;
}

int server_read_timeout(int server_fd, u8 *buf, int len, int time_out)
{
	int ret;
	int r_len = 0;
	int nOldTimeOut = time_out;

	while (1)
	{
		ret = read(server_fd, buf + r_len, len - r_len);

		if (ret > 0)
		{
			r_len += ret;
			break;
		}
		else if (ret == 0) //server close
		{
			return -99;
		}

		if (errno == 0)
			return -99;
		if (errno == EAGAIN)
		{
			usleep(50 * 1000);
			//usleep(1 * 1000);
			time_out--;
			if (time_out <= 0)
			{
				//time_out = nOldTimeOut;
				r_len = 0;
				break;
			}
			continue;
		}
	}

	return r_len;
}

int upload_data_handle(int server_fd)
{
	eMsg_type CMD;

	int ret = 0;

	do
	{
		ret = recv_msg_from_dev(&CMD);
		syslog(LOG_DEBUG, "recv_msg_from_dev:ret:%d, CMD:%d\n", ret, CMD);
		if (ret > 0)
		{
			//			if (gReal_system.work_flag)
			{
				if (msg_DEV_NEW_DATA == CMD) //new data
				{
					syslog(LOG_DEBUG, "DEV_request_count:%d,%d,%d\n", DEV_request_count, dev_unit.dev_scan_count,dev_unit.read_request_respond_flag);
					if (DEV_request_count != dev_unit.dev_scan_count)
					{
						//if (0 == dev_unit.read_request_respond_flag)
						{

							xml_upload_gather_data(server_fd);
							dev_unit.read_request_respond_flag = 1;
						}
						//else  //drop
						//{

						//}
					}
					else
					{
						xml_upload_arrived_data(server_fd);
					}
				}
				else if (msg_WT_THRESHOLD == CMD)
				{
					if (DEV_request_count == dev_unit.dev_scan_count)
					{
						//label_list.label_cnt = 0;
						//label_list.msg_send_flag = 0;
						xml_upload_threshold(server_fd);
					}
				}
			}
		}
		usleep(50 * 1000);
	} while (ret > 0);

	return 0;
}

int net_system_CTRL_handle(int server_fd, u8 *buf, int len)
{
	NET_PACKET_MSG *p_ctrl = NULL;

	p_ctrl = (NET_PACKET_MSG*) buf;

	if (!strcmp(p_ctrl->msg_body, gSetting_system.SYS_CTRL_R_start_event_ID))
	{
		//log_send("ASK:\tStart read!\n");
		syslog(LOG_DEBUG, "ASK:\tStart read!\n");

		//DEV_request_count++;
		//dev_unit.request_clean_history_cnt++;
		//dev_unit.read_request_respond_flag = 0;
		////xml_system_ctrl_hook(p_ctrl->xml_data, p_ctrl->xml_data_len);
		g_StartEventSem.post();
	}
	else if (!strcmp(p_ctrl->msg_body,
			gSetting_system.SYS_CTRL_R_stop_event_ID))
	{
		//log_send("ASK:\tStop read!\n");
		syslog(LOG_DEBUG, "ASK:\tStop read!\n");
		//DEV_request_count = dev_unit.dev_scan_count;
		//dev_unit.request_clean_history_cnt++;
		////xml_system_ctrl_hook(p_ctrl->xml_data, p_ctrl->xml_data_len);
		g_StoppedEventSem.post();
	}
	else
	{
		//LOG("system ctrl id= %s\n", p_ctrl->event_id);
		syslog(LOG_DEBUG, "system ctrl id= %s\n", p_ctrl->msg_body);
	}

	return 0;
}

//get a packet
//return value: 0:no data; >0: get packet -1:server close -2:error

int get_one_packet(int net_fd, u8* buf)
{
	int ret, r_len = 0;
	net_packet_head_t msg_head;
	char str_tmp[16];

	ret = server_read_timeout(net_fd, buf, sizeof(net_packet_head_t) + 8, 10);
	if (0 >= ret)
	{
		return ret;
	}
	else if (ret != (sizeof(net_packet_head_t) + 8))
	{
		//printf("get str head len error!	%s\n", str_tmp);
		syslog(LOG_DEBUG, "get str head len error!	%s\n", str_tmp);
		return -2;
	}

	memcpy(str_tmp, buf, 8);
	str_tmp[8] = 0;
	if (strcmp((char*) str_tmp, "0XJZTECH"))
	{
		//printf("str head check error!\n");
		syslog(LOG_DEBUG, "str head check error!\n");
		return -2;
	}
	memcpy(&msg_head, buf + 8, sizeof(net_packet_head_t));
	r_len = sizeof(net_packet_head_t) + 8;

	if (msg_head.packet_len > 0)
	{
		ret = server_read_timeout(net_fd, buf + r_len, msg_head.packet_len, 10);
		if (0 >= ret)
		{
			return ret;
		}
		else if (ret != msg_head.packet_len)
		{
			//printf("get msg body len error!\n");
			syslog(LOG_DEBUG, "get msg body len error!\n");
			return -2;
		}
		r_len += msg_head.packet_len;
	}

	ret = server_read_timeout(net_fd, buf + r_len, 8, 10);
	if (0 >= ret)
	{
		return ret;
	}
	else if (ret != 8)
	{
		//printf("get str tail len error!\n");
		syslog(LOG_DEBUG, "get str tail len error!\n");
		return -2;
	}

	memcpy(str_tmp, buf + r_len, 8);
	str_tmp[8] = 0;
	if (strcmp(str_tmp, "NJTECHJZ"))
	{
		//printf("str tail check error!\n");
		syslog(LOG_DEBUG, "str tail check error!\n");
		return -2;
	}

	r_len += 8;

	return r_len;
}

//			close(server_fd);
//			log_send("ERR:\tthe server's connection is lost!\n");
//			log_send("MSG:\tRetry to connect to the server...\n");
//
//			//连接服务�?//			while (1)
//			{
//				server_fd = connect_server(gSetting_system.server_ip,
//						gSetting_system.server_port);
//				if (server_fd <= 0)
//				{
//					printf("connect to server error!\n");
//				}
//				else
//				{
//					log_send("MSG:\tconnected to the server!\n");
//					ret = net_dev_register_handle(server_fd);
//					if (0 == ret)
//					{
//						log_send("MSG:\tregister to the server!\n");
//						break;
//					}
//					else
//					{
//						close(server_fd);
//					}
//				}
//				sleep(2);
//			}


