#include "includes.h"
#include "packet_format.h"
#include "pthread.h"
#include "starup_handle.h"
#include "port.h"
#include <cstdlib>
#include "ace/Signal.h"
#include "ace/streams.h"
#include "ace/Thread_Manager.h"
#include "ace/Select_Reactor.h"
#include "MSGHandleCenter.h"

#include "SysUtil.h"
#include <syslog.h>
#include <dlfcn.h>
#include <stack>
#include <vector>
#include <new>

#define SOFTWARE_VERSION  "version 1.0.0.0"         //软件版本号

pthread_t tid_DEV;
pthread_t tid_worker_task;
pthread_t tid_net;
pthread_t tid_arrived;
pthread_t tid_started;
pthread_t tid_stopped;
pthread_t tid_timer;

volatile u32 DEV_request_count = 0;
int gFd_dev = -1;
struct struDeviceAndServiceStatus g_ObjectStatus;
pthread_mutex_t g_StatusMutex;

using namespace std;

static ACE_THR_FUNC_RETURN event_loop(void *arg)
{
    ACE_Reactor *reactor = (ACE_Reactor*)arg;
    reactor->owner(ACE_OS::thr_self());
    reactor->run_reactor_event_loop();
    return 0;
}

int Daemon()
{
    pid_t pid;
    pid = fork();
    if (pid < 0)
    {
            return -1;
    }
    else if (pid != 0)
    {
            exit(0);
    }
    setsid();
    return 0;
}

//int rsmain()
int main(int argc, char *argv[])
{
    ACE_Sig_Action sig((ACE_SignalHandler) SIG_IGN, SIGPIPE);
    ACE_UNUSED_ARG(sig);

    openlog("BarcodeServer", LOG_PID, LOG_LOCAL4);
    memset(&g_ObjectStatus, 0x00, sizeof(g_ObjectStatus));
    char szNowTime[32] = {0};
    GetCurTime(szNowTime);
    szNowTime[31] = '\0';

    param_init_handle();
    pthread_create(&tid_worker_task, NULL, pthread_worker_task, NULL);
    syslog(LOG_DEBUG, "create the worker task pthread OK!\n");


    ACE_Select_Reactor *select_reactor;
    ACE_NEW_RETURN(select_reactor, ACE_Select_Reactor, 1);
    ACE_Reactor *reactor;
    ACE_NEW_RETURN(reactor, ACE_Reactor(select_reactor, 1), 1);
    ACE_Reactor::close_singleton();
    ACE_Reactor::instance(reactor, 1);

    ACE_Thread_Manager::instance()->spawn(event_loop, reactor);
    syslog(LOG_DEBUG, "starting up ledserver daemon\n");
    syslog(LOG_DEBUG, "starting up reactor event loop ...\n");
    MSG_HANDLE_CENTER::instance()->open();

    ACE_Reactor* reactorptr = ACE_Reactor::instance();
    pthread_mutex_lock(&g_StatusMutex);
    strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
    g_ObjectStatus.nLocalPort		= 0;
    strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
    g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
    strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
    g_ObjectStatus.nServiceType	= 0;
    strcpy(g_ObjectStatus.szObjectCode, gSetting_system.register_DEV_ID);
    strcpy(g_ObjectStatus.szObjectStatus, "030");
    strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");
    {
        char szNowTime[32] = {0};
        GetCurTime(szNowTime);
        szNowTime[31] = '\0';
        strcpy(g_ObjectStatus.szReportTime, szNowTime);
    }
    pthread_mutex_unlock(&g_StatusMutex);

    dev_open_handle(&gFd_dev);
    pthread_create(&tid_DEV, NULL, pthread_dev_handle, NULL);
    syslog(LOG_DEBUG, "create the DEV pthread OK!\n");

    pthread_create(&tid_arrived, NULL, ArrivedEventThread, NULL);
    syslog(LOG_DEBUG, "create the arrived pthread OK!\n");

    pthread_create(&tid_started, NULL, StartEventThread, NULL);
    syslog(LOG_DEBUG, "create the started pthread OK!\n");	

    pthread_create(&tid_stopped, NULL, StopEventThread, NULL);
    syslog(LOG_DEBUG, "create the stopped pthread OK!\n");	

    pthread_create(&tid_timer, NULL, TimerEventThread, NULL);
    syslog(LOG_DEBUG, "create the timer pthread OK!\n");	
    
    MSG_HANDLE_CENTER::instance()->wait();
    ACE_Thread_Manager::instance()->wait();
    ACE_DEBUG((LM_DEBUG, "(%P|%t) shutting down ledserver daemon\n"));
    close(gFd_dev);

    strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
    g_ObjectStatus.nLocalPort		= 0;
    strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
    g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
    strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
    g_ObjectStatus.nServiceType	= 0;
    strcpy(g_ObjectStatus.szObjectCode, gSetting_system.register_DEV_ID);
    strcpy(g_ObjectStatus.szObjectStatus, "031");
    strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");
    //char szNowTime[32] = {0};
    GetCurTime(szNowTime);
    szNowTime[31] = '\0';
    strcpy(g_ObjectStatus.szReportTime, szNowTime);

    closelog();
    usleep(50 * 1000);
    return 0;
}

//int main(int argc, char *argv[])
//{
//	int iRet;
//	int iStatus;
//	pid_t pid;
//	//显示版本号
//	if (argc == 2)
//	{
//		//如果是查看版本号
//		if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "-V") || !strcmp(argv[1], "-Version") || !strcmp(argv[1], "-version"))
//		{
//			printf("%s  %s\n", argv[0], SOFTWARE_VERSION);
//			return 0;
//		}
//	}
//    
//	Daemon();
//
//createchildprocess:
//	//开始创建子进程
//	printf("begin to create	the child process of %s\n", argv[0]);
//
//	int itest = 0;
//	switch (fork())//switch(fork())
//	{
//	case -1 : //创建子进程失败
//		printf("cs创建子进程失败\n");
//		return -1;
//	case 0://子进程
//		printf("cs创建子进程成功\n");
//		rsmain();
//		return -1;
//	default://父进程
//		pid = wait(&iStatus);
//		printf("子进程退出，5秒后重新启动......\n");
//		sleep(3);
//		goto createchildprocess; //重新启动进程
//		break;
//	}
//	return 0;
//}
