#pragma once

#include "includes.h"



#define MSG_LOG_KEY_TMP_FILE_NAME    	"/tmp/870"
#define LOG_MAX_SIZE            		(150)

typedef struct
{
	u32 cmd;
	u32 src_id;
	u32 parame;
	char log[LOG_MAX_SIZE];
} msg_log_i;

typedef struct
{
	int msg_type;
	union
	{
		msg_log_i msg;
		char data[LOG_MAX_SIZE];
	};
} msg_log_t;

typedef enum
{
	msg_DEV_NEW_DATA = 0x75,
	msg_WT_THRESHOLD = 0x87,
} eMsg_type;

typedef struct
{
	u32 cmd;
	u32 src_id;
	u32 parame;
	char str[128];
} msg_event_i;

typedef struct
{
	int msg_type;
	union
	{
		msg_event_i msg;
		char data[150];
	};
} msg_event_t;

u32 get_system_ms(void);

int log_init(void);
int log_write(char *info);
int log_add(char *info, int tim_flag);
int log_send(char *info);
void msg_queue_init(void);
int dev_send_msg(eMsg_type cmd, u32 src_id);
int recv_msg_from_dev(eMsg_type *pCMD);
int str_to_data(u8 * const d_buf, int *d_len, char *str_in);
int data_to_str(char *const str_out, u8 *d_buf, int d_len );
int ip_address_check(char *p_ip_str);
int interger_check(char *p_str);
