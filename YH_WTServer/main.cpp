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
#include "ProcessWeight.h"

#include <syslog.h>
#include <dlfcn.h>
#include <stack>
#include <vector>
#include <new>

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

int main(int argc, char** argv)
{
    	ACE_Sig_Action sig((ACE_SignalHandler) SIG_IGN, SIGPIPE);
    	ACE_UNUSED_ARG(sig);

	int server_fd;
	openlog("YHWTServer", LOG_PID, LOG_LOCAL4);
	memset(&g_ObjectStatus, 0x00, sizeof(g_ObjectStatus));

	//strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
	//g_ObjectStatus.nLocalPort		= 0;
	//strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
	//g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
	//strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
	//g_ObjectStatus.nServiceType	= 0;
	//strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
	//strcpy(g_ObjectStatus.szObjectStatus, "030");
	//strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");
	char szNowTime[32] = {0};
	GetCurTime(szNowTime);
	szNowTime[31] = '\0';
	//strcpy(g_ObjectStatus.szReportTime, szNowTime);


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
	syslog(LOG_DEBUG, "starting up etagserver daemon\n");
	syslog(LOG_DEBUG, "starting up reactor event loop ...\n");
    MSG_HANDLE_CENTER::instance()->open();

    ACE_Reactor* reactorptr = ACE_Reactor::instance();



	//dev_open_handle(&gFd_dev);
	//dev_connect_handle(gFd_dev);
	//server_connect_handle(&server_fd);

	pthread_mutex_lock(&g_StatusMutex);
	strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
	g_ObjectStatus.nLocalPort		= 0;
	strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
	g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
	strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
	g_ObjectStatus.nServiceType	= 0;
	strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
	strcpy(g_ObjectStatus.szObjectStatus, "030");
	strcpy(g_ObjectStatus.szConnectedObjectCode,	"COMM_CONTROL_001");

	{
		char szNowTime[32] = {0};
		GetCurTime(szNowTime);
		szNowTime[31] = '\0';
		strcpy(g_ObjectStatus.szReportTime, szNowTime);
	}
	pthread_mutex_unlock(&g_StatusMutex);


	//server_register_handle(server_fd);

	//pthread_create(&tid_DEV, NULL, pthread_dev_handle2, NULL);
	//syslog(LOG_DEBUG, "create the DEV pthread OK!\n");
        
        ProcessWeight weight;
        weight.Init();
        weight.Run();

	////net_handle(server_fd);
	//pthread_create(&tid_net, NULL, net_handle_thread, NULL);
	//syslog(LOG_DEBUG, "create the net handle pthread OK!\n");

	pthread_create(&tid_arrived, NULL, ArrivedEventThread, NULL);
	syslog(LOG_DEBUG, "create the arrived pthread OK!\n");

	pthread_create(&tid_started, NULL, StartEventThread, NULL);
	syslog(LOG_DEBUG, "create the started pthread OK!\n");	

	pthread_create(&tid_stopped, NULL, StopEventThread, NULL);
	syslog(LOG_DEBUG, "create the stopped pthread OK!\n");	

	//pthread_create(&tid_timer, NULL, TimerEventThread, NULL);
	//syslog(LOG_DEBUG, "create the timer pthread OK!\n");	


    MSG_HANDLE_CENTER::instance()->wait();
    ACE_Thread_Manager::instance()->wait();
    ACE_DEBUG((LM_DEBUG, "(%P|%t) shutting down etagserver daemon\n"));

	close(gFd_dev);



	strcpy(g_ObjectStatus.szLocalIP, gSetting_system.server_ip);
	g_ObjectStatus.nLocalPort		= 0;
	strcpy(g_ObjectStatus.szUpstreamIP, gSetting_system.server_ip);
	g_ObjectStatus.nUpstreamPort	= gSetting_system.server_port;
	strcpy(g_ObjectStatus.szUpstreamObjectCode, "APP_CHANNEL_001");
	g_ObjectStatus.nServiceType	= 0;
	strcpy(g_ObjectStatus.szObjectCode, "DEV_WEIGHT_001");
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

