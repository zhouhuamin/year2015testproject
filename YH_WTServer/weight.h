#pragma once



#define REAL_VALUE_BUF_SIZE    		6
#define AVERAGE_VALUE_BUF_SIZE    	5

typedef struct
{
	int index;
	int data[REAL_VALUE_BUF_SIZE];
}real_value_t;


typedef struct
{
	int index;
	int cnt;
	int data[AVERAGE_VALUE_BUF_SIZE];
}average_value_t;


int weight_init(int *p_fd_com);
int weight_get(int fd_wt, int time_out);
int insert_real_value(int wt_value);
int get_average_value();
int real_value_parse(int real_value);
int weight_clean(void);
