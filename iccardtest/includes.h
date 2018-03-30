#pragma once

#include    <stdio.h>
#include    <string.h>
#include    <ctype.h>
#include    <stdlib.h>
#include    <stdarg.h>
#include 	<errno.h>

#include 	<unistd.h>
#include 	<fcntl.h>
#include 	<linux/fb.h>
#include 	<sys/mman.h>
#include 	<termio.h>
#include 	<sys/socket.h>
#include 	<sys/types.h>
#include 	<netdb.h>
#include 	<netinet/in.h>
#include 	<sys/ipc.h>
#include 	<sys/msg.h>
#include <sys/time.h>
#include <pthread.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <syslog.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef char s8;
typedef short s16;
typedef int s32;

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;

typedef unsigned short int color_type;
typedef enum
{
	FALSE = 0, TRUE,
} BOOL;

#include "config.h"
#include "common.h"






#define _PIG_DEBUG_		1

#if _PIG_DEBUG_

#define LOG( ... ) printf(  __VA_ARGS__ )
#define LOG_M(format, ... ) printf( format"\n", __VA_ARGS__ )
#define DBG( ... ) printf(  __VA_ARGS__ )

#else

#define LOG( ... )
#define LOG_M(format, ... )
#define DBG( ... )
#endif


#define NET_COMM_IP_ADDRESS		("192.168.1.254")
#define NET_COMM_PORT    		(4001)

