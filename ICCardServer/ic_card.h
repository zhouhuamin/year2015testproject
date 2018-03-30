#pragma once



#define IC_CARD_LIST_SIZE    (0x05)

typedef enum
{
	cmd_get_version = 0x00,
	cmd_module_close,
	cmd_module_warm_reset,
	cmd_rf_reset,
	cmd_request,
	cmd_anticoll,
	cmd_select,
	cmd_get_card_num,
	cmd_authentication,
	cmd_read_block,
	cmd_write_block = 0x10,
	cmd_value,
	cmd_halt,
	cmd_beep = 0xA6,
} cmd_ic_t;

typedef struct
{
	int len;
	int data_len;
	union
	{
		struct
		{
			u8 head_flag;
			u8 cmd;
			u8 reser;
			u8 data_len;
			u8 data[256];
		} frame;
		u8 buf[0x400];
	};
} ic_send_frame_t;

typedef struct
{
	int len;
	int data_len;
	union
	{
		struct
		{
			u8 head_flag;
			u8 cmd;
			u8 state;
			u8 data_len_h;
			u8 data_len_l;
			u8 data[256];
		} frame;
		u8 buf[0x400];
	};
} ic_recv_frame_t;

typedef struct
{
	int len;
	int type;
	u8 data[0x100];
} card_t;

typedef struct
{
	int len;
	u32 system_tick;
	u8 data[0x100];
} card_block_t;



//typedef struct
//{
//	int label_cnt;
//	int cur_index;
//	card_block_t block[IC_CARD_LIST_SIZE];
//}label_list_t;



int ic_app_get_version(int fd_dev, u8 * ver, int *p_len);
int ic_app_module_close(int fd_dev);
int ic_app_module_warm_reset(int fd_dev);
int ic_app_rf_reset(int fd_dev, u8 time_ms);
int ic_app_request(int fd_dev, u8 mode, int *card_type);
int ic_app_anticoll(int fd_dev, u8 anti_layer, u8 card_bits, u32 *p_serial_num);
int ic_app_select(int fd_dev, u8 anti_layer, u32 serial_num, int *p_result);
int ic_app_get_card_num(int fd_dev, u8 mode, int *p_state, card_t *p_card);
int ic_app_authentication(int fd_dev, u8 key_type, u8 section_id, u8 key[6]);
int ic_app_read_block(int fd_dev, int block_NO, card_block_t *p_block);
int ic_app_write_block(int fd_dev, int block_NO, u8 *buf, int len);
int ic_app_value(int fd_dev, u8 value_type, int block_NO, u32 value,
		int op_block_NO);
int ic_app_halt(int fd_dev);
int ic_app_beep(int fd_dev,int time_ms);

int ic_dev_open(int *p_fd_ic);
int ic_card_section_read(int fd_ic, int block_NO, card_block_t *p_block);
int compare_ic_block(card_block_t *p_old_block, card_block_t *p_new_block);

