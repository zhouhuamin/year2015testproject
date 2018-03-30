#include "includes.h"
#include "config.h"
#include "ic_card.h"
//#include "mwrf.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include "common.h"
#include <syslog.h>
#include <string>
#include <vector>

static ic_send_frame_t w_frame;
static ic_recv_frame_t r_frame;

static int ic_dev_send_package(int fd_dev, ic_send_frame_t *p_frame);
static int ic_dev_get_package(int fd_dev, ic_recv_frame_t *p_frame,
		int time_out);
static int ic_dev_package_check(ic_recv_frame_t *p_frame);

int ic_app_get_version(int fd_dev, u8 * ver, int *p_len)
{
	int ret;

	w_frame.frame.cmd = 0x00;
	w_frame.frame.data_len = 0;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}
	*p_len = r_frame.data_len;
	memcpy(ver, r_frame.frame.data, *p_len);

	return 0;
}
int ic_app_module_close(int fd_dev)
{
	int ret;

	w_frame.frame.cmd = cmd_module_close;
	w_frame.frame.data_len = 0;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (0 == r_frame.data_len)
	{
		return r_frame.frame.state;
	}
	else
	{
		return -1;

	}
}

int ic_app_module_warm_reset(int fd_dev)
{
	int ret;

	w_frame.frame.cmd = cmd_module_warm_reset;
	w_frame.frame.data_len = 0;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (0 == r_frame.data_len)
	{
		return r_frame.frame.state;
	}
	else
	{
		return -1;
	}
}
int ic_app_rf_reset(int fd_dev, u8 time_ms)
{
	int ret;

	w_frame.frame.cmd = cmd_rf_reset;
	w_frame.frame.data[0] = time_ms;
	w_frame.frame.data_len = 1;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (0 == r_frame.data_len)
	{
		return r_frame.frame.state;
	}
	else
	{
		return -1;
	}
}
int ic_app_request(int fd_dev, u8 mode, int *card_type)
{
	int ret;

	w_frame.frame.cmd = cmd_request;
	w_frame.frame.data[0] = mode;
	w_frame.frame.data_len = 1;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (2 == r_frame.data_len)
	{
		*card_type = *(u16*) (r_frame.frame.data);
		return r_frame.frame.state;
	}
	else
	{
		return -1;
	}
}

int ic_app_anticoll(int fd_dev, u8 anti_layer, u8 card_bits, u32 *p_serial_num)
{
	int ret;

	w_frame.frame.cmd = cmd_anticoll;
	w_frame.frame.data[0] = anti_layer;
	w_frame.frame.data[1] = card_bits;
	w_frame.frame.data_len = 2;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (4 == r_frame.data_len)
	{
		*p_serial_num = *(u32*) (r_frame.frame.data);
		return r_frame.frame.state;
	}
	else
	{
		syslog(LOG_DEBUG, "datalen error:%d!\n",r_frame.data_len);
		return -1;
	}
}

int ic_app_select(int fd_dev, u8 anti_layer, u32 serial_num, int *p_result)
{
	int ret;

	w_frame.frame.cmd = cmd_select;
	w_frame.frame.data[0] = anti_layer;
	w_frame.frame.data[1] = serial_num & 0xFF;
	serial_num >>= 8;
	w_frame.frame.data[2] = serial_num & 0xFF;
	serial_num >>= 8;
	w_frame.frame.data[3] = serial_num & 0xFF;
	serial_num >>= 8;
	w_frame.frame.data[4] = serial_num & 0xFF;

	w_frame.frame.data_len = 5;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (1 == r_frame.data_len)
	{
		*p_result = r_frame.frame.data[0];
		return r_frame.frame.state;
	}
	else
	{
		syslog(LOG_DEBUG, "data len error:%d!\n",r_frame.data_len);
		return -1;
	}
}

int ic_app_get_card_num(int fd_dev, u8 mode, int *p_state, card_t *p_card)
{
	int ret;

	w_frame.frame.cmd = cmd_get_card_num;
	w_frame.frame.data[0] = mode;
	w_frame.frame.data_len = 1;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (4 < r_frame.data_len)
	{
		p_card->type = *(u16*) r_frame.frame.data;
		*p_state = r_frame.frame.data[2];
		p_card->len = r_frame.frame.data[3];
		memcpy(p_card->data, &(r_frame.frame.data[4]), p_card->len);
	}
	else
	{
		*p_state = 0;
		p_card->len = 0;
	}

	return 0;
}

int ic_app_authentication(int fd_dev, u8 key_type, u8 section_id, u8 key[6])
{
	int ret;

	w_frame.frame.cmd = cmd_authentication;
	w_frame.frame.data[0] = key_type;
	w_frame.frame.data[1] = section_id;

	memcpy(&w_frame.frame.data[2], key, 6);

	w_frame.frame.data_len = 8;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (0 == r_frame.data_len)
	{
		return r_frame.frame.state;
	}
	else
	{
		return -1;
	}
}

int ic_app_read_block(int fd_dev, int block_NO, card_block_t *p_block)
{
	int ret;

	w_frame.frame.cmd = cmd_read_block;
	w_frame.frame.data[0] = block_NO;

	w_frame.frame.data_len = 1;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (0 <= r_frame.data_len)
	{
		p_block->len = r_frame.data_len;
		memcpy(p_block->data, r_frame.frame.data, p_block->len);
		return r_frame.frame.state;
	}
	else
	{
		p_block->len = 0;
		return -1;
	}
}

int ic_app_write_block(int fd_dev, int block_NO, u8 *buf, int len)
{
	int ret;

	w_frame.frame.cmd = cmd_write_block;
	w_frame.frame.data[0] = block_NO;
	memcpy(&(w_frame.frame.data[1]), buf, len);
	w_frame.frame.data_len = 1 + len;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (0 == r_frame.data_len)
	{
		return r_frame.frame.state;
	}
	else
	{
		return -1;
	}
}

int ic_app_value(int fd_dev, u8 value_type, int block_NO, u32 value,
		int op_block_NO)
{
	int ret;

	w_frame.frame.cmd = cmd_value;
	w_frame.frame.data[0] = value_type;
	w_frame.frame.data[1] = block_NO;
	memcpy(&(w_frame.frame.data[3]), &value, 4);

	w_frame.frame.data[6] = op_block_NO;

	w_frame.frame.data_len = 7;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (0 == r_frame.data_len)
	{
		return r_frame.frame.state;
	}
	else
	{
		return -1;
	}
}
int ic_app_halt(int fd_dev)
{
	int ret;

	w_frame.frame.cmd = cmd_halt;
	w_frame.frame.data_len = 0;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (0 == r_frame.data_len)
	{
		return r_frame.frame.state;
	}
	else
	{
		return -1;
	}
	return 0;
}

int ic_app_beep(int fd_dev,int time_ms)
{
	int ret;

	w_frame.frame.cmd = cmd_beep;
	w_frame.frame.data[0] = time_ms;
	w_frame.frame.data_len = 1;

	ret = ic_dev_send_package(fd_dev, &w_frame);
	if (0 != ret)
	{
		return ret;
	}

	ret = ic_dev_get_package(fd_dev, &r_frame, 200);
	if (0 != ret)
	{
		return ret;
	}

	if (0 == r_frame.data_len)
	{
		return r_frame.frame.state;
	}
	else
	{
		return -1;
	}
	return 0;
}

static int ic_dev_send_package(int fd_dev, ic_send_frame_t *p_frame)
{
	int data_len = 0;
	int ret = 0;

	p_frame->frame.head_flag = 0x40;
	data_len = p_frame->frame.data_len;

	p_frame->buf[4 + data_len] = 0;
	p_frame->buf[5 + data_len] = 0;
	p_frame->buf[6 + data_len] = 0x0D;
	p_frame->len = data_len + 7;

	tcflush(fd_dev, TCIOFLUSH);
        
        char szTmpData[5 + 1] = {0};
        std::string strLogData = "Send Data:";
        for (int i = 0; i < p_frame->len; ++i)
        {
            memset(szTmpData, 0x00, 5);
            sprintf(szTmpData, "%02X ", p_frame->buf[i]);
            strLogData += szTmpData;
        }
        syslog(LOG_DEBUG, "%s\n", strLogData.c_str());

	ret = write(fd_dev, p_frame->buf, p_frame->len);
	if (ret != p_frame->len)
	{
		syslog(LOG_DEBUG, "send package error!\n");
		return -99;
	}
	return 0;
}

static int ic_dev_package_check(ic_recv_frame_t *p_frame)
{
	u8 sum_chk = 0;
	u8 xor_chk = 0;
	int i;

	for (i = 0; i < p_frame->len - 3; i++)
	{
		sum_chk += p_frame->buf[i];
		xor_chk ^= p_frame->buf[i];
	}

	if (sum_chk != p_frame->buf[p_frame->len - 3])
	{
		syslog(LOG_DEBUG, "sum chk error! %x,%x\n", p_frame->buf[p_frame->len - 3], sum_chk);
		return -1;
	}

	if (xor_chk != p_frame->buf[p_frame->len - 2])
	{
		syslog(LOG_DEBUG, "xor chk error! %x,%x\n", p_frame->buf[p_frame->len - 2], sum_chk);
		return -2;
	}

	return 0;
}
static int ic_dev_get_package(int fd_dev, ic_recv_frame_t *p_frame,
		int time_out)
{
	int ret;
	int r_len = 0;
	int len = 0;
	u32 start_time_ms;
	u8 *p_data;

	p_data = p_frame->buf;

	start_time_ms = get_system_ms();
	while (1)
	{
		ret = read(fd_dev, p_data, 1);
		if (ret > 0)
		{
			if (0x40 == p_data[0])
			{
				r_len = 1;
				break;
			}
		}
		else
		{
			usleep(1000);
		}

		if ((get_system_ms() - start_time_ms) >= time_out)
		{
			syslog(LOG_DEBUG, "get head timeout!\n");
			return -99;
		}
	}

	len = 4 + r_len; //cmd+status+data_len
	while (1)
	{
		ret = read(fd_dev, p_data + r_len, len - r_len);
		if (ret > 0)
		{
			r_len += ret;
			if (r_len >= len)
			{
				break;
			}
		}
		else
		{
			usleep(1000);
		}

		if ((get_system_ms() - start_time_ms) >= time_out)
		{
			syslog(LOG_DEBUG, "get data len timeout!\n");
			return -99;
		}
	}

	p_frame->data_len = p_frame->frame.data_len_h * 256
			+ p_frame->frame.data_len_l;

	if (p_frame->data_len >= 0x400 - 20)
	{
		syslog(LOG_DEBUG, "get the data len error:%d!", p_frame->data_len);
		return -1;
	}
	len = r_len + p_frame->data_len + 2 + 1;
	while (1)
	{
		ret = read(fd_dev, p_data + r_len, len - r_len);
		if (ret > 0)
		{
			r_len += ret;
			if (r_len >= len)
			{
				break;
			}
		}
		else
		{
			usleep(1000);
		}

		if ((get_system_ms() - start_time_ms) >= time_out)
		{
			syslog(LOG_DEBUG, "get data timeout!\n");
			return -99;
		}
	}

	p_frame->len = p_frame->data_len + 8;
	if (0x0D != p_frame->buf[p_frame->len - 1])
	{
		syslog(LOG_DEBUG, "tail chk timeout!\n");
		return -2;
	}

	if (0 != ic_dev_package_check(p_frame))
	{
		syslog(LOG_DEBUG, "package chk timeout!\n");
		return -1;
	}
        
        char szTmpData[5 + 1] = {0};
        std::string strLogData = "Recv Data:";
        for (int i = 0; i < p_frame->len; ++i)
        {
            memset(szTmpData, 0x00, 5);
            sprintf(szTmpData, "%02X ", p_data[i]);
            strLogData += szTmpData;
        }
        syslog(LOG_DEBUG, "%s\n", strLogData.c_str());

	return 0;
}

//int IC_reader_connect(int *pic_dev, int baudrate)
//{
////	int ic_dev = 0;
//	int status = 0;
//	u8 card_status[32] =
//	{ 0 };
//
//	if (*pic_dev > 0 && *pic_dev < 65536)
//	{
//		status = rf_reset(*pic_dev, 5);
//		rf_exit(*pic_dev);
//		*pic_dev = 0;
//	}
//
//	LOG("rf init!\n");
//	*pic_dev = rf_init("ic_conf", baudrate);
//
//	LOG("ic_dev=%d\n", *pic_dev);
//	LOG("rf get status!\n");
//	status = rf_get_status(*pic_dev, card_status);
//
//	if (*pic_dev < 0 || status)
//	{
//		printf("connect fail!...\n");
//		return -1;
//	}
//
//	LOG("rf reset!\n");
//	status = rf_reset(*pic_dev, 5);
//	if (status)
//	{
//		printf("rf reset error!\n");
//		return -1;
//	}
////	*pic_dev = ic_dev;
//	return 0;
//}
//
//int IC_card_ID_read(int ic_dev, int addr, u8 * buf)
//{
//	u32 tag_type = 0;
//	int status = 0;
//	static u8 key[7] =
//	{ 0xA8, 0x3F, 0x63, 0x17, 0x69, 0xE2, 0x00 };
//	unsigned long snr = 0;
//	u8 size = 0;
//	int r_len = 0;
//	int return_value = 0;
//	u8 tmp[64] =
//	{ 0 };
//	int i;
//
//	LOG("rf_request!\n");
//	LOG("return value =%d!\n", return_value);
//	if (1 != return_value)
//	{
//
//		return -1;
//	}
//	status = rf_request(ic_dev, 1, &tag_type);
//	if (status)
//	{
//		printf("find card fail!.....\n");
//		return -1;
//	}
//
//	LOG("rf_anticoll!\n");
//	status = rf_anticoll(ic_dev, 0, &snr);
//	if (status)
//	{
//		printf("anti card fail!...\n");
//		return -1;
//	}
//	LOG("rf_select!\n");
//	status = rf_select(ic_dev, snr, &size);
//	if (status)
//	{
//		printf("select card fail!...\n");
//		return -1;
//	}
//	LOG("rf_load_key!\n");
//	status = rf_load_key(ic_dev, 0, 0, key);
//	if (status)
//	{
//		printf("load the key fail!...\n");
//		return -1;
//	}
//	LOG("rf_authentication!\n");
//	status = rf_authentication(ic_dev, 0, 0);
//	if (status)
//	{
//		printf("authentication error!...\n");
//		return -1;
//	}
////	u8 tmp[64] =
////	{ 0 };
//	LOG("rf_read!\n");
//	status = rf_read(ic_dev, addr, tmp);
//	if (status)
//	{
//		printf("read data error!...\n");
//		return -1;
//	}
//	else
//	{
//		rf_beep(ic_dev, 10);
//	}
//
//	for (i = 0; i < 64; i++)
//	{
////		if (gSetting_system.IC_read_block_NO == 1)  //ic card
////		{
////			if (isprint(tmp[i]))
////			{
////				buf[i] = tmp[i];
////			}
////			else
////			{
////				r_len = i;
////				buf[i] = '\0';
////				break;
////			}
////		}
////		else
////		{
//		if (tmp[i])
//		{
//			buf[i] = tmp[i];
//		}
//		else
//		{
//			r_len = i;
//			break;
//		}
////		}
//	}
//
//	return r_len;
//}
