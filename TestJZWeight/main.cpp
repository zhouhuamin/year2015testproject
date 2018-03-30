/* 
 * File:   main.cpp
 * Author: root
 *
 * Created on 2015年7月5日, 上午9:31
 */

#include <cstdlib>
#include "ProcessWeight.h"
#include <syslog.h>

using namespace std;

//#define random(x)   (rand()%x)
/*
 * 
 */
int main(int argc, char** argv)
{
//    srand((int)time(0));
//    for (int x = 0; x < 10; ++x)
//        printf("%d\n", random(100000));
    openlog("BoxRecogServer", LOG_PID, LOG_LOCAL1);
    syslog(LOG_DEBUG, "start work!\n");
    ProcessWeight weight;
    weight.Init();
    weight.Run();
    weight.Wait();
    return 0;
}

