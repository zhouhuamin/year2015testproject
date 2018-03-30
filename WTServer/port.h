#pragma once

#include "dev.h"

int dev_open(int *pFd_dev);

int dev_connect(int fd_dev);
void system_setting_verify(void);
int xml_upload_threshold(int server_fd);
int xml_upload_gather_data(int server_fd);
int xml_upload_arrived_data(int server_fd);
int xml_system_ctrl_hook(char *xml_buf, int xml_buf_len);
