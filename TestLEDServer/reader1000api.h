#include "includes.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>



//#pragma comment(lib, "wsock32.lib")
//#pragma comment(lib, "ws2_32.lib")

//==============================数据类型定义==============================
typedef int apiReturn;   // 函数返回值类型
//==============================常量定义==============================
#define	ID_MAX_SIZE_64BIT	8		//电子标签的ID号为64bit
#define ID_MAX_SIZE_96BIT	65		//电子标签的ID号
#define MAX_LABELS			100		// 一次读写操作最多不超过100个标签

//==============================API函数返回码==============================
#define	_OK					0x00	// 操作成功

//通信方面出错信息
#define _init_rs232_err		0x81	//  通信口初始化失败
#define _no_scanner			0x82	//  找不到读写器
#define _comm_error         0x83	//  收发通信数据出错
#define _baudrate_error     0x84	//  设置波特率出错
#define _init_net_error		0x85	//  网口初始化失败
#define _net_error          0x86	//  网口收发通信数据出错

// 读写器返回出错信息
#define _no_antenna			0x01   //天线连接失败
#define _no_label			0x02   //未检测到标签
#define _invalid_label		0x03   //非法标签
#define _less_power			0x04   //读写功率不够
#define _write_prot_error	0x05   //该区写保护
#define _check_sum_error	0x06   //校验和错误
#define _parameter_error	0x07   //参数错误
#define _memory_error		0x08   //数据区不存在
#define _password_error		0x09   //密码不正确
#define _killpassword_error	0x0a   //G2标签毁灭密码为全零
#define _nonlicet_command	0x0b   //非法命令
#define _nonlicet_user		0x0c   //密码不匹配的非法用户
#define _invalid_command	0x1e   //表示无效指令，如参数不正确的指令
#define _other_error		0x1f   //未知命令

//函数输入错误
#define _no_cardID_input   0x20   //其它错误

typedef struct tagReaderDate
{
	u8	Year;			//年
	u8	Month;			//月
	u8	Day;			//日
	u8	Hour;			//时
	u8	Minute;			//分
	u8	Second;			//秒
}ReaderDate;


typedef struct tagReader2200Param
{
	u8	BaudRate;			//串口的通信速率，取值：00H~08H，代表速率同"设定波特率"命令。
	u8	Power;				//发射功率值，取值：30~160。
	u8	Min_Frequence;		//发射微波信号频率的起始点，取值： 1~63。
	u8	Antenna;			//天线选择。1-ant1,2-ant2,4-ant4,8-ant8
	u8	WorkMode;			//读写器工作方式：1-定时方式，2-触发方式，3-命令方式，4-定时方式2，5-触发方式2 。
	u8	ReadInterval;		//读卡间隔。 0-10ms, 1-30ms, 2-50ms
	u8	OutMode;			//卡号输出方式(直接输出,标准输出)。
	u8	TriggerMode;	    //触发方式。
	u8	IDPosition;			//卡号在电子标签上的存放地址。
	u8	IfTestValidity;		//标签是否合法。
	u8	OutInterface;		//卡号输出接口和格式(Wiegan26,Wiegan34,RS485,RS232,RJ45)。
	u8	NumofCard;			//最多读卡数目。
	u8	Power2;				//发射功率系数2， 取值：30~160。
	u8	TagType;			//标签种类：01H－ISO18000-6B，02H－EPCC1，04H－EPCC1G2，08H－TK900。
	u8	WiegandWidth;		//Weigand脉冲宽度值。
	u8	WiegandInterval;	//Weigand脉冲间隔值。
	u8	ID_Start;			//输出卡号的起始位，取值0～4。
	u8	Max_Frequence;		//发射微波信号频率的终止点，取值： 1~63。
	u8	ReadDuration;		//读卡持续时间：射频发射持续时间，只针对TK900标签有效；0－10ms，1－20ms，2－30ms，3－40ms。
	u8	StandardTime;		//标准输出时间间隔，缺省值为120秒，1～255。
	u8	EnableBuzzer;		//1:使能蜂鸣器0:不使能蜂鸣器
	u8	ReaderAddress;		//0-255
	u8	HostIP1;			//上位机IP地址1
	u8	HostIP2;			//上位机IP地址2
	u8	HostIP3;			//上位机IP地址3
	u8	HostIP4;			//上位机IP地址4
	u8	HostPort1;			//上位机端口1
	u8	HostPort2;			//上位机端口2
	u8	Reserve29;			//保留
	u8	Reserve30;			//保留
	u8	TX_Mode;			//发射模式：0-表示收发模式；1-表示发射模式。
	u8	Modulation;			//调制设置：0-表示无调制信号；1-表示有调制信号。
} Reader1000Param;

typedef struct tagReaderDebugState
{
	u8	Test;				//0－工作状态；1－调试状态。
	u8	Reserve2;			//保留
	u8	Reserve3;			//保留
	u8	Reserve4;			//保留
} ReaderDebugState;

//==============================函数定义==============================
BOOL SocketBuffer(int hSocket,u8 *lpPutBuf,u8 *lpGetBuf,int nBufLen);

//连接读写器
apiReturn  ConnectScanner(int *hScanner, char *szPort, int nBaudRate);

//断开连接
apiReturn  DisconnectScanner(int hScanner);

//==============================设备控制命令==============================
//设置波特率
apiReturn  SetBaudRate(int hScanner, int nBaudRate,int Address);

//读取版本号
apiReturn  GetReaderVersion(int hScanner, u16 *wHardVer, u16  *wSoftVer,int Address);

//设置读写器继电器状态
apiReturn  SetRelay(int hScanner, int Relay,int Address);

//设定输出功率
apiReturn  SetOutputPower(int hScanner, int nPower1,int Address);

//设定工作频率
apiReturn  SetFrequency(int hScanner, int Min_Frequency, int Max_Frequency,int Address);

//读取写器工作参数
apiReturn  ReadParam(int hScanner, Reader1000Param * pParam,int Address);

//设置读写器工作参数
apiReturn  WriteParam(int hScanner, Reader1000Param * pParam,int Address);

//设置读写器出厂参数
apiReturn  WriteFactoryParameter(int hScanner, Reader1000Param * pParam);

//读取读写器出厂参数
apiReturn  ReadFactoryParameter(int hScanner);

//选择天线
apiReturn  SetAntenna(int hScanner, int Antenna,int Address);

//复位读写器
apiReturn  Reboot(int hScanner,int Address);

//设置时间
apiReturn  SetReaderTime(int hScanner, ReaderDate time,int Address);

//获得时间
apiReturn  GetReaderTime(int hScanner, ReaderDate *time,int Address);

//获得读写器ID
apiReturn  GetReaderID(int hScanner, u8 *ReaderID);

//增加名单
apiReturn  AddLableID(int hScanner, int listlen, int datalen, u8 * data);

//删除名单
apiReturn  DelLableID(int hScanner, int listlen, int datalen, u8 * data);

//获得名单
apiReturn  GetLableID(int hScanner, int startaddr, int listlen, int *relistlen, int *taglen, u8 * data);

//获得记录
apiReturn  GetRecord(int hScanner, ReaderDate *stime, ReaderDate *etime, int startaddr, int listlen, int *relistlen, int *taglen, u8 * data);

//删除全部记录
apiReturn  DeleteAllRecord(int hScanner);

//设置读写器工作模式
apiReturn  SetWorkMode(int hScanner, int mode,int Address);

//设置标签过滤器
apiReturn  SetReportFilter(int hScanner, int ptr, int len, u8 *mask);

//获得标签过滤器
apiReturn  GetReportFilter(int hScanner, int *ptr, int *len, u8 *mask);

//==============================特殊命令==============================

//修改为EP500
apiReturn  SetEP500(int hScanner, u8 ifEP500);

//开关射频功率
apiReturn  Switch_RF_Power(int hScanner, int test, int RF_onoff);

//设置读写器ID
apiReturn  SetReaderID(int hScanner, u8 *ReaderID);

//修改读写器频率范围
apiReturn  changeFrequency(int hScanner, u8 fre);

//修改读写器硬件版本号
apiReturn  SetHardVersion(int hScanner, int wHardVer);

//==============================网络命令==============================
//设置读写器IP地址
apiReturn  SetReaderNetwork(int hScanner, u8 IP_Address[4], int Port, u8 Mask[4], u8 Gateway[4]);

//获得读写器IP地址
apiReturn  GetReaderNetwork(int hScanner, u8 *IP_Address, int *Port, u8 *Mask, u8 *Gateway);

//设置读写器MAC地址
apiReturn  SetReaderMAC(int hScanner, u8 MAC[6]);

//获得读写器MAC地址
apiReturn  GetReaderMAC(int hScanner, u8 *MAC);

//==============================ISO-6B数据读写命令==============================
//检测标签存在
apiReturn  ISO6B_LabelPresent(int hScanner, int *nCounter,int Address);

//读取ISO6B标签ID号
apiReturn  ISO6B_ReadLabelID(int hScanner, u8 *IDBuffer, int *nCounter,int Address);

//列出选定标签
apiReturn  ISO6B_ListSelectedID(int hScanner, int Cmd, int ptr, u8 Mask, u8 *Data, u8 *IDBuffer, int *nCounter,int Address);

//读一块数据
apiReturn  ISO6B_ReadByteBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 len,u8 *Data,int Address);

//一次写4字节数据
apiReturn  ISO6B_WriteByteBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 len, u8 *Data,int Address);

//一次写一字节数据
apiReturn  ISO6B_WriteAByte(int hScanner, u8 *IDBuffer, u8 ptr, u8 len, u8 *Data,int Address);

//写大块数据，字节数超过16
apiReturn  ISO6B_WriteLongBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 len, u8 *Data,int Address);

//置写保护状态
apiReturn  ISO6B_WriteProtect(int hScanner, u8 *IDBuffer, u8 ptr,int Address);

//读写保护状态
apiReturn  ISO6B_ReadWriteProtect(int hScanner, u8 *IDBuffer, u8 ptr, u8 *Protected,int Address);

//全部清除
apiReturn  ISO6B_ClearMemory(int hScanner, u8 CardType, u8 *IDBuffer,int Address);

//==============================EPC C1G2数据读写命令==============================

apiReturn EPC1G2_ListTagID(int hScanner, u8 mem, int ptr, u8 len, u8 *mask,
		u8 *btID, int *nCounter, int *IDlen, int Address);

//读取EPC1G2标签ID号
apiReturn  EPC1G2_ReadLabelID(int hScanner, u8 mem, int ptr, u8 len, u8 *mask, u8 *IDBuffer, int *nCounter,int Address);

//读一块数据
apiReturn  EPC1G2_ReadWordBlock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 mem, int ptr, u8 len, u8 *Data, u8 *AccessPassword,int Address);

//写一块数据
apiReturn  EPC1G2_WriteWordBlock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 mem, int ptr, u8 len, u8 *Data, u8 *AccessPassword,int Address);

//设置读写保护状态
apiReturn  EPC1G2_SetLock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 mem, u8 Lock, u8 *AccessPassword,int Address);

//擦除标签数据
apiReturn  EPC1G2_EraseBlock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 mem, u8 ptr, u8 len,int Address);

//永久休眠标签
apiReturn  EPC1G2_KillTag(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 *KillPassword, int recom, int Address);

//写EPC
apiReturn  EPC1G2_WriteEPC(int hScanner, u8 len, u8 *Data, u8 *AccessPassword,int Address);

//块锁命令
apiReturn  EPC1G2_BlockLock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 ptr, u8 *AccessPassword,int Address);

//EAS状态操作命令
apiReturn  EPC1G2_ChangeEas(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 State, u8 *AccessPassword,int Address);

//EAS报警命令
apiReturn  EPC1G2_EasAlarm(int hScanner,int Address);

//读保护设置
apiReturn  EPC1G2_ReadProtect(int hScanner,u8 *AccessPassword, u8 EPC_WORD, u8 *IDBuffer,int Address);

//复位读保护设置
apiReturn  EPC1G2_RStreadProtect(int hScanner, u8 *AccessPassword,int Address);

//设置用户区数据块读保护
apiReturn  EPC1G2_BlockReadLock(int hScanner, u8 EPC_WORD, u8 *IDBuffer, u8 Lock, u8 *AccessPassword,int Address);

//识别EPC同时读数据
apiReturn  EPC1G2_ReadEPCandData(int hScanner, u8 *EPC_WORD, u8 *IDBuffer, u8 mem, u8 ptr, u8 len, u8 *Data, int Address);

//频谱校验
apiReturn  EPC1G2_Calibrate(int hScanner, u8 *AccessPassword, u8 Kword);

//侦测标签
apiReturn  EPC1G2_DetectTag(int hScanner,int Address);
//==============================TK900数据读写命令==============================
//读取TK900标签ID号
apiReturn  TK900_ReadLabelID(int hScanner, u8 *IDBuffer, int *nCounter,int Address);

//读一块数据
apiReturn  TK900_ReadPageBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 len,u8 *Data,int Address);

//写一块数据
apiReturn  TK900_WritePageBlock(int hScanner, u8 *IDBuffer, u8 ptr, u8 *Data,int Address);

//置写保护状态
apiReturn  TK900_SetProtect(int hScanner, u8 *IDBuffer, u8 ptr, u8 len,int Address);

//读写保护状态
apiReturn  TK900_GetProtect(int hScanner, u8 *IDBuffer ,u8 *state,int Address);

//设置标签进入TTO状态
apiReturn  TK900_SetTTO(int hScanner, u8 *IDBuffer ,u8 ptr, u8 len,int Address);

//读取TTO标签
apiReturn  TK900_ListTagPage(int hScanner, u8 *IDBuffer ,u8 *Data,int Address);

//得到TTO标签的开始地址和长度
apiReturn  TK900_GetTTOStartAdd(int hScanner, u8 *IDBuffer ,u8 *len,int Address);

//==============================网口函数定义==============================
//连接读写器_UDP连接方式
apiReturn  Net_ConnectScanner(int *hSocket,char *nTargetAddress,u32 nTargetPort,char *nHostAddress,u32 nHostPort);
//连接读写器_TCP连接方式
apiReturn  TCP_ConnectScanner(int *hSocket,char *nTargetAddress,u32 nTargetPort,char *nHostAddress,u32 nHostPort);
//设置当前连接的套接字序号
apiReturn  Net_SetCurrentSocketNum(int socketnum);
//获取当前连接的套接字
apiReturn  Net_GetCurrentSocket(int *hsocket);

//断开连接
apiReturn  Net_DisconnectScanner();

//==============================设备控制命令==============================
//设置波特率
apiReturn  Net_SetBaudRate(int hSocket, int nBaudRate);

//读取版本号
apiReturn  Net_GetReaderVersion(int hSocket, u16 *wHardVer, u16  *wSoftVer);

//设定输出功率
apiReturn  Net_SetOutputPower(int hSocket, int nPower);

//设定工作频率
apiReturn  Net_SetFrequency(int hSocket, int Min_Frequency, int Max_Frequency);

//读取写器工作参数
apiReturn  Net_ReadParam(int hSocket, Reader1000Param *pParam);

//设置调制度
apiReturn  Net_SetModDepth(int hSocket, int ModDepth);

//获得调制度
apiReturn  Net_GetModDepth(int hSocket, int *ModDepth);

//设置读写器工作参数
apiReturn  Net_WriteParam(int hSocket, Reader1000Param *pParam);

//选择天线
apiReturn  Net_SetAntenna(int hSocket, int Antenna);

//设置读写器出厂参数
apiReturn  Net_WriteFactoryParameter(int hSocket, Reader1000Param *pParam);

//读取读写器出厂参数
apiReturn  Net_ReadFactoryParameter(int hSocket);

//复位读写器
apiReturn  Net_Reboot(int hSocket);

//设置时间
apiReturn  Net_SetReaderTime(int hSocket, ReaderDate time);

//获得时间
apiReturn  Net_GetReaderTime(int hSocket, ReaderDate *time);

//增加名单
apiReturn  Net_AddLableID(int hSocket, int listlen, int datalen, u8 * data);

//删除名单
apiReturn  Net_DelLableID(int hSocket, int listlen, int datalen, u8 * data);

//获得名单
apiReturn  Net_GetLableID(int hSocket, int startaddr, int listlen, int *relistlen, int *taglen, u8 * data);

//获得记录
apiReturn  Net_GetRecord(int hSocket, ReaderDate *stime, ReaderDate *etime, int startaddr, int listlen, int *relistlen, int *taglen, u8 * data);

//删除全部记录
apiReturn  Net_DeleteAllRecord(int hSocket);

//设置读写器工作模式
apiReturn  Net_SetWorkMode(int hSocket, int mode);

//设置标签过滤器
apiReturn  Net_SetReportFilter(int hSocket, int ptr, int len, u8 *mask);

//获得标签过滤器
apiReturn  Net_GetReportFilter(int hSocket, int *ptr, int *len, u8 *mask);

//==============================网络命令==============================
//设置读写器IP地址
apiReturn  Net_SetReaderNetwork(int hSocket, u8 IP_Address[4], int Port, u8 Mask[4], u8 Gateway[4]);

//获得读写器IP地址
apiReturn  Net_GetReaderNetwork(int hSocket, u8 *IP_Address, int *Port, u8 *Mask, u8 *Gateway);

//设置读写器MAC地址
apiReturn  Net_SetReaderMAC(int hSocket, u8 MAC[6]);

//获得读写器MAC地址
apiReturn  Net_GetReaderMAC(int hSocket, u8 *MAC);

//获得读写器ID
apiReturn  Net_GetReaderID(int hSocket, u8 *ReaderID);

//==============================IO命令==============================
//设置读写器继电器状态
apiReturn  Net_SetRelay(int hSocket, int relay);

//获得读写器继电器状态
apiReturn  Net_GetRelay(int hSocket, int *relay);

//==============================ISO-6B数据读写命令==============================
//检测标签存在
apiReturn  Net_ISO6B_LabelPresent(int hSocket, int *nCounter);

//读取ISO6B标签ID号
apiReturn  Net_ISO6B_ReadLabelID(int hSocket, u8 *IDBuffer, int *nCounter);

//列出选定标签
apiReturn  Net_ISO6B_ListSelectedID(int hSocket, int Cmd, int ptr, u8 Mask, u8 *Data, u8 *IDBuffer, int *nCounter);

//读一块数据
apiReturn  Net_ISO6B_ReadByteBlock(int hSocket, u8 *IDBuffer, u8 ptr, u8 len,u8 *Data);

//一次写4字节数据
apiReturn  Net_ISO6B_WriteByteBlock(int hSocket, u8 *IDBuffer, u8 ptr, u8 len, u8 *Data);

//一次写一字节数据
apiReturn  Net_ISO6B_WriteAByte(int hSocket, u8 *IDBuffer, u8 ptr, u8 len, u8 *Data);

//置写保护状态
apiReturn  Net_ISO6B_WriteProtect(int hSocket, u8 *IDBuffer, u8 ptr);

//读写保护状态
apiReturn  Net_ISO6B_ReadWriteProtect(int hSocket, u8 *IDBuffer, u8 ptr, u8 *Protected);

//==============================EPC C1G2数据读写命令==============================
//读取EPC1G2标签ID号
apiReturn  Net_EPC1G2_ReadLabelID(int hSocket, u8 mem, int ptr, u8 len, u8 *mask, u8 *IDBuffer, int *nCounter);

//读一块数据
apiReturn  Net_EPC1G2_ReadWordBlock(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 mem, int ptr, u8 len, u8 *Data, u8 *AccessPassword);

//写一块数据
apiReturn  Net_EPC1G2_WriteWordBlock(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 mem, int ptr, u8 len, u8 *Data, u8 *AccessPassword);

//设置读写保护状态
apiReturn  Net_EPC1G2_SetLock(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 mem, u8 Lock, u8 *AccessPassword);

//擦除标签数据
apiReturn  Net_EPC1G2_EraseBlock(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 mem, u8 ptr, u8 len);

//写EPC
apiReturn  Net_EPC1G2_WriteEPC(int hSocket,u8 len, u8 *Data, u8 *AccessPassword);

//块锁命令
apiReturn  Net_EPC1G2_BlockLock(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 ptr, u8 *AccessPassword);

//EAS状态操作命令
apiReturn  Net_EPC1G2_ChangeEas(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 State, u8 *AccessPassword);

//EAS报警命令
apiReturn  Net_EPC1G2_EasAlarm(int hSocket);

//读保护设置
apiReturn  Net_EPC1G2_ReadProtect(int hSocket,u8 *AccessPassword, u8 EPC_WORD, u8 *IDBuffer);

//识别EPC同时读数据
apiReturn  Net_EPC1G2_ReadEPCandData(int hSocket, u8 *EPC_WORD, u8 *IDBuffer, u8 mem, u8 ptr, u8 len, u8 *Data, int Address);

//设置用户区数据块读保护
apiReturn  Net_EPC1G2_BlockReadLock(int hSocket, u8 EPC_WORD, u8 *IDBuffer, u8 Lock, u8 *AccessPassword);

//复位读保护设置
apiReturn  Net_EPC1G2_RStreadProtect(int hSocket, u8 *AccessPassword);

//侦测标签
apiReturn  Net_EPC1G2_DetectTag(int hSocket);

//==============================TK900数据读写命令==============================
//读取TK900标签ID号
apiReturn  Net_TK900_ReadLabelID(int hSocket, u8 *IDBuffer, int *nCounter);

//读一块数据
apiReturn  Net_TK900_ReadPageBlock(int hSocket, u8 *IDBuffer, u8 ptr, u8 len,u8 *Data);

//写一块数据
apiReturn  Net_TK900_WritePageBlock(int hSocket, u8 *IDBuffer, u8 ptr, u8 *Data);

//置写保护状态
apiReturn  Net_TK900_SetProtect(int hSocket, u8 *IDBuffer, u8 ptr, u8 len);

//读写保护状态
apiReturn  Net_TK900_GetProtect(int hSocket, u8 *IDBuffer ,u8 *state);

//设置标签进入TTO状态
apiReturn  Net_TK900_SetTTO(int hSocket, u8 *IDBuffer ,u8 ptr, u8 len);

//读取TTO标签
apiReturn  Net_TK900_ListTagPage(int hSocket, u8 *IDBuffer ,u8 *Data);

//得到TTO标签的开始地址和长度
apiReturn  Net_TK900_GetTTOStartAdd(int hSocket, u8 *IDBuffer ,u8 *len);


apiReturn ISO6B_ListID(int hScanner, u8 *btID, int *nCounter, int Address);
