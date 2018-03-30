#pragma once


int param_init_handle(void);
int dev_open_handle(int  *p_fd_com);
int dev_connect_handle(int fd_dev);
int server_connect_handle(int *p_fd_ser);
int server_register_handle(int server_fd);
int dev_retry_connect_handle(int *pFd_dev);
int server_retry_connect_handle(int *pFd_ser);
