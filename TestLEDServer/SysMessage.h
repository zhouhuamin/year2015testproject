#if !defined _SYS_MESSAGE_H_


#define _SYS_MESSAGE_H_








//define sys net protocal





#define MAX_MSG_BODYLEN                        (10*1024)





#define SYS_NET_MSGHEAD                          "0XJZTECH"


#define SYS_NET_MSGTAIL                           "NJTECHJZ"





#define SYS_SYSTEM                             0X00


#define SYS_SUB_SYSTEM_ICCARDREADER         1000000


#define SYS_SUB_SYSTEM_LEVERCONTROLER      2000000


#define SYS_SUB_SYSTEM_LCDCONTROLER        3000000








#define SYS_MSG_SYSTEM_REGISTER_REQ          SYS_SYSTEM +1


#define SYS_MSG_SYSTEM_REGISTER_ACK          SYS_SYSTEM +2





#define SYS_MSG_SYSTEM_KEEPLIVE_REQ          SYS_SYSTEM +3


#define SYS_MSG_SYSTEM_KEEPLIVE_ACK          SYS_SYSTEM +4








#define SYS_MSG_PUBLISH_EVENT                SYS_SYSTEM+5


#define SYS_MSG_PUBLISH_EVENT_ACK           SYS_SYSTEM+6








#define SYS_MSG_ICCARDREADER_READ            SYS_SUB_SYSTEM_ICCARDREADER+1


#define SYS_MSG_ICCARDREADER_READ_ACK       SYS_SUB_SYSTEM_ICCARDREADER+2








#define SYS_MSG_LEVERCONTROLER_ON            SYS_SUB_SYSTEM_ICCARDREADER+3


#define SYS_MSG_LEVERCONTROLER_ON_ACK       SYS_SUB_SYSTEM_ICCARDREADER+4





#define SYS_MSG_LEVERCONTROLER_OFF         SYS_SUB_SYSTEM_ICCARDREADER+5


#define SYS_MSG_LEVERCONTROLER_OFF_ACK    SYS_SUB_SYSTEM_ICCARDREADER+6








#define SYS_MSG_LCD_DISPLAY                     SYS_SUB_SYSTEM_ICCARDREADER+7


#define SYS_MSG_LCD_DISPLAY_ACK              SYS_SUB_SYSTEM_ICCARDREADER+8








#define SYS_EVENT_CONTA_RECOG_COMPLETE   "1600000002"      // 箱号识别完成





struct NET_PACKET_HEAD


{


    int msg_type;


    int packet_len;


	char version_no[16];


    int proxy_count;


    int net_proxy[10];


};





struct NET_PACKET_MSG


{


    NET_PACKET_HEAD msg_head;


    char msg_body[MAX_MSG_BODYLEN];


};





//struct NVP_REGISTER
//{
//	char                     device_tag[32];
//	char                     device_id[32];
//};





//struct NVP_PACKET_HEAD
//

//{
//

//    int req_id;
//

//    int msg_type;
//

//    int packet_len;
//

//    int proxy_count;
//

//    int net_proxy[10];
//

//};





//struct NVP_PACKET_MSG
//

//{
//

//	NET_PACKET_HEAD msg_head;
//

//	char msg_body[MAX_MSG_BODYLEN];
//

//};





struct T_DeviceServerRegister_Req


{


    char device_tag[32];


    char device_id[32]; //服务模块ID，唯一值


};





struct T_DeviceServerRegister_Ack


{


    int reg_result; //注册结果，成功还是失败


    int next_action; //后续动作，是立即开始工作还是等待启动指令


};





struct T_KeepliveAck


{


    int nResult;


};





struct T_SysEventData


{


    char  event_id[32];
	int		is_data_full;


    char  device_tag[32];


    int   xml_data_len;


    char  xml_data[];


};





#endif





