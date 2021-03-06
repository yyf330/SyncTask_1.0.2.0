#include <string>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <list>   
#include <dirent.h>
#include "task.h"
#include "taskqueue.h"
#include "threadstate.h"
#include "threadstatecollection.h"
#include "commandparser.h"
#include "socketex.h"
#include "smsmanager.h"
#include "alarmmanager.h"

#include "hiredis.h"

#include "createtable.h"
/************************************* 常量定义 *********************************/
#define DEFAULT_SCAN_AGENT_STATUS_INTERVAL          30          // 秒
#define DEFAULT_CHECK_DAEMON_STATUS_INTERVAL		60			// 秒
#define APP_VERSION_INFORMATION                     "1.1.1"
#define APP_RELEASE_TIME                            "2017-9-10"
#define APP_OS_SUPPORT                              "Linux/AIX"

/************************************* 函数声明 *********************************/
void * RefreshTaskThread(void *arg);		//刷新任务线程
void * CleanThread(void *arg);
void * ListenMonitorThread(void *arg);		//侦听监控信息线程
void * ListenOperationThread(void *arg);	//侦听操作线程
void * ListenInternalUdpThread(void *arg);	//侦听内部线程

void * QueryAgentStatusThread(void *arg);
void * QueryDBMirrorStatusThread(void *arg);

void * SendAlarmThread(void *arg);
void * MonitorDaemonThread(void *arg);	//监视Daemon是否运行
void * HBHandleThread(void *arg);		//根据心跳状态处理报警
void * SelfCheckThread(void *arg);
//void *DataCheckOutThread(void *arg);

bool CreateThreads();
void StopThreads();

void InitSourceDBMap();
void InitStrategyMap();

void CatchInterrupt(int sig);
bool SelfCheck();
bool CheckLicense();
void BaseInit();
void ShowLicense();					//显示授权信息
void PrintVersion();				//打印版本信息
void ShowBasicInfo();				//显示基本信息
bool ParseOption(int argc, char** argv);

/*
 *  功能：
 *      处理客户端操作
 *  参数
 *      sock                :   套接字对象
 */
void HandleClientOperation(SocketEx *sock);

bool HandleMonitorData(string monitordata);

/************************************* 私有变量 *********************************/
ThreadStateCollection MyThreadStateCol; // 线程状态集合
bool MyNeedToExitFlag = false; // 需要退出标志
BeCommand *MyCommandManager = NULL;

int ZwgDebugLevel = 2;
int MyCurrentListenPort = 0;
string ExecutePath = "/home/dss/";
string LicenseType = "LICENSE";
int ExtractorAutostartInterval = 60;
int LoaderAutostartInterval = 60;
int AutoClean = 0;

map<string, time_t> SourceDBMap;		// 源库集合(源库ID,挖取进程最近一次发送状态的时间)
map<string, time_t> StrategyMap;		// 策略集合(策略ID,装载进程最近一次发送状态的时间)
map<string, time_t> TranspDBMap;		// 源库集合(源库ID,挖取进程最近一次发送状态的时间)


#define  DB_NAME 2  //   1��pg  2;mysql
#if (DB_NAME==1) //postgresql
DbDataManager_PQ *MyDbDataManager = NULL;
#elif (DB_NAME==2)  //mysql
DbDataManager *MyDbDataManager = NULL;
#else//mysql
DbDataManager *MyDbDataManager = NULL;
#endif

#if 1
#include <netdb.h>

#define SIZE 32
#define TOCAPTURE 9300   // capture   process   connect  port
#define TOTRANSPORT 9400 //transport process  connect  port
#define TOLOAD   9500    //load process connect   port

typedef enum MsgType
{
    MSG_SEND_FOR_CLIENT_STATUS=0x5c,
    MSG_UNKNOW, // 
}Msg_t;


typedef enum CommandType
{
    COMMAND_UNKNOWN=0x10,
    COMMAND_START, // 
    COMMAND_SUSPEND, //
    COMMAND_QUIT, // 
}command_t;

typedef struct data_package
{
    int msg_type;
    int cmd_type;
	int id;
	char buf[24];
}data_t;

int SendMsgToCap_Tran_Load(int ToWhich,Msg_t msg_type,command_t command_type,int id) 
{
	BesLog::DebugPrint("SendMsgToCap_Tran_Load-----", BesLog::LOG_MESSAGE_TYPE_ERROR);
	cout<<"id ="<<id <<endl;
	cout<<"ToWhich ="<<ToWhich <<endl;

    int fd;
	int port=ToWhich+id;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in saddr;
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	int on = 1;
	//reuse_addr
	setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(int *) &on,sizeof(on));

    int conn;
    conn = connect(fd, (struct sockaddr*)&saddr, sizeof(saddr));
    if( conn<0 ) {
        printf("connect error\n");
        close(fd); //  
        return -1;  
    }
    char bufRecv[16]={0};
    data_t senddata;
    senddata.msg_type=msg_type;
    senddata.cmd_type=command_type;
    senddata.id=id;
    memcpy(senddata.buf,"hello world!",13);
    //printf("%s", buf);
    //write(fd, (char*)(&senddata), sizeof(senddata));
        
    int ret = send(fd, (char*)(&senddata), sizeof(senddata), 0);  
    if (-1 == ret)  
    {  
		printf("send--------error!\n"); 
        close(fd); //  
        return -1;  
    }  
    ret=recv(fd, bufRecv, 5 , 0);
	if(-1 == ret)
	{
		printf("bufRecv--------error!\n"); 
		close(fd);
		return -1;
	}
    printf("bufRecv=[%s]\n",bufRecv); 
    close(fd);
    return 0;
}

#endif

#define SCN_REFRESH 1
#if SCN_REFRESH//modify for scn refresh
#define LINUXOCCI
#include <occi.h>
using namespace oracle::occi;
Environment *scn_env;

void OracleConnect_Init()
{

	scn_env=Environment::createEnvironment(Environment::OBJECT);
	BesLog::DebugPrint("init oracle success!", BesLog::LOG_MESSAGE_TYPE_INFO);

}

int ScnRefresh_Func(string name,string pass,string srvName,string sourceid)
{
	try
	{
		Connection *conn;
		Statement *stmt;
		conn = scn_env->createConnection(name,pass,srvName);
		BesLog::DebugPrint("connect oracle success!", BesLog::LOG_MESSAGE_TYPE_INFO);

		string strsql("select dbms_flashback.get_system_change_number as scn,to_char(sysdate,'yyyy-MM-dd HH24:mi:ss') from dual");
		stmt = conn->createStatement(strsql);
		ResultSet *rset = stmt->executeQuery();
		string scn;		
		string now_time;
		while(rset->next())
		{
					scn = rset->getString(1);
					now_time= rset->getString(2);
					//string dname = rset->getString("");
					//cout<<"scn ="<<scn <<endl;
		}
		stmt->closeResultSet(rset);
		conn->terminateStatement(stmt);
		scn_env->terminateConnection(conn);
		MyDbDataManager->UpdateNowScn(scn,sourceid);
		MyDbDataManager->UpdateNowTime(now_time,sourceid);
	}
	catch(SQLException e)
	{
		BesLog::DebugPrint(e.what(), BesLog::LOG_MESSAGE_TYPE_INFO);
		return -1;
	}
	
	return 0;
}
#endif
#if 0

redisContext *redis_connect=NULL;

bool redis_connect_init()
{
	redis_connect	= redisConnect("127.0.0.1",6379);
	if(redis_connect != NULL && redis_connect->err)
	{
		BesLog::DebugPrint("redis connection error: "+string(redis_connect->errstr), BesLog::LOG_MESSAGE_TYPE_ERROR);
 		return false;
	}
	
	redisReply *reply = (redisReply*)redisCommand(redis_connect,"auth admin");
	freeReplyObject(reply);
	
 	return true;
}


redisReply * redis_commond(redisContext *conn,char *commond)
{
	redisReply *reply = (redisReply*)redisCommand(conn,commond);//"hmset hfoo name daolin	age 1234");
	//freeReplyObject(reply);
	return reply;
}

void redis_disconnect(redisContext *c, int select_db)
{
	BesLog::DebugPrint("redis disconnect !", BesLog::LOG_MESSAGE_TYPE_INFO);

    redisReply *reply;   
	/* Make sure we're on DB 9. */    
	reply = redisCommand(c,"SELECT %d",select_db);    
	//assert(reply != NULL);    
	freeReplyObject(reply);    
	reply = redisCommand(c,"FLUSHDB");    
	//assert(reply != NULL);    
	freeReplyObject(reply);

	redisFree(c);
}
#endif


/*------------------------------------ 入口函数 --------------------------------*/
int main(int argc, char** argv)
{
    BaseInit();

    // 解析选项
    if (ParseOption(argc, argv) == true)
    {
        if (MyDbDataManager != NULL) delete MyDbDataManager;
        return (EXIT_SUCCESS);
    }

    // 自检
    if (SelfCheck() == false)
    {
        return (EXIT_FAILURE);
    }

	if (CheckLicense() == false) return (EXIT_FAILURE);

    signal(SIGINT, CatchInterrupt);

	//对所有需要自启的挖取，创建001任务
#if (DB_NAME==1) //postgresql
	pqxx::result SourceDBResultSet = MyDbDataManager->GetAutostartSourceDBResultSet();
	//int num_rows = DBResultSet.size();
	if (SourceDBResultSet.size() > 0)
	{	
		for (pqxx::result::const_iterator row = SourceDBResultSet.begin(); row != SourceDBResultSet.end(); ++row)
		{
			string SourceID = row[0].as<string>();
			MyDbDataManager->AddTask(DEFAULT_COMMAND_START_EXTRACTOR, SourceID+string(";"));
			BesLog::DebugPrint("add autostart capture task", BesLog::LOG_MESSAGE_TYPE_INFO);
		}

	}

	pqxx::result StrategyResultSet = MyDbDataManager->GetAutostartStrategyResultSet();
	if (StrategyResultSet.size() > 0)
	{	
		for (pqxx::result::const_iterator row = StrategyResultSet.begin(); row != StrategyResultSet.end(); ++row)
		{
			string StrategyID = row[0].as<string>();
			MyDbDataManager->AddTask(DEFAULT_COMMAND_START_LOADER, StrategyID+string(";"));
			BesLog::DebugPrint("add autostart loader task", BesLog::LOG_MESSAGE_TYPE_INFO);
		}

	}
	
#elif (DB_NAME==2) //mysql
	sql::ResultSet * SourceDBResultSet = MyDbDataManager->GetAutostartSourceDBResultSet();
	if (SourceDBResultSet != NULL && SourceDBResultSet->rowsCount() > 0)
	{
		while (SourceDBResultSet->next())
		{
			string SourceID = SourceDBResultSet->getString("SID");
			MyDbDataManager->AddTask(DEFAULT_COMMAND_START_EXTRACTOR, SourceID+string(";"));
			BesLog::DebugPrint("add autostart capture task", BesLog::LOG_MESSAGE_TYPE_INFO);
		}
	}
	MyDbDataManager->DestroyResultSet(SourceDBResultSet);

	//对所有需要自启的装载，创建003任务
	sql::ResultSet * StrategyResultSet = MyDbDataManager->GetAutostartStrategyResultSet();
	if (StrategyResultSet != NULL && StrategyResultSet->rowsCount() > 0)
	{
		while (StrategyResultSet->next())
		{
			string StrategyID = StrategyResultSet->getString("STRATEGY_ID");
			MyDbDataManager->AddTask(DEFAULT_COMMAND_START_LOADER, StrategyID+string(";"));
			BesLog::DebugPrint("add autostart loader task", BesLog::LOG_MESSAGE_TYPE_INFO);
		}
	}
	MyDbDataManager->DestroyResultSet(StrategyResultSet);
#endif
	InitSourceDBMap();
	InitStrategyMap();

    // 添加到线程集合
    MyThreadStateCol.Add(ThreadState("RefreshTaskThread", RefreshTaskThread));		//刷新任务    启动抓取、停止抓取、启动装载、停止装载
	//if(AutoClean == 1) MyThreadStateCol.Add(ThreadState("CleanThread", CleanThread));	//清理
	MyThreadStateCol.Add(ThreadState("ListenMonitorThread", ListenMonitorThread));	//接收状态    在8585端口接收进程发来的状态信息(抓取:源库id,状态  装载:策略id,状态)。根据状态信息修改数据库的状态字段。
	MyThreadStateCol.Add(ThreadState("HBHandleThread", HBHandleThread));
	MyThreadStateCol.Add(ThreadState("SelfCheckThread", SelfCheckThread));			//????????web??????????
//	MyThreadStateCol.Add(ThreadState("DataCheckOutThread", DataCheckOutThread));			//????????web??????????
    // ??????
    if (CreateThreads() == false) return (EXIT_FAILURE);

    StopThreads();
    delete MyDbDataManager;
    return (EXIT_SUCCESS);
}

bool CheckLicense()
{
	if(LicenseType == "DOG"){
		if(BesEncryptD::CheckDogExist() == false){
			BesLog::DebugPrint("[License]Dog Not Exist!", BesLog::LOG_MESSAGE_TYPE_ERROR);
			return false;
		}
	}
	else{
	    // License文件是否存在
	    if (BesEncryptF::IsLicenseExist() == false){
	        BesLog::DebugPrint("[License]License File Not Exist!", BesLog::LOG_MESSAGE_TYPE_ERROR);
	        return false;
	    }
	}

    // 检查策略数授权
	BesLog::DebugPrint("Check strategy number...");
    int TotalStrategy = MyDbDataManager->GetStrategyCount();
    int StrategyLicense;
	//加密狗方式
	if(LicenseType == "DOG"){
		StrategyLicense = BeGeneralLib::StringToInt(BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_HOST));
		if (TotalStrategy <= StrategyLicense && StrategyLicense > 0 && BesEncryptD::ReadDogHostCount() <= StrategyLicense){
//			BesLog::DebugPrint("Strategy number:     [OK]");
		}
		else{
			if (StrategyLicense == 0)
			   BesLog::DebugPrint("Strategy number:     [0]", BesLog::LOG_MESSAGE_TYPE_INFO);
	        else
	            BesLog::DebugPrint("Strategy number:     [FAILED]", BesLog::LOG_MESSAGE_TYPE_ERROR);
			return false;
		}
#if (DB_NAME==1) //postgresql
		
		pqxx::result HostRs = MyDbDataManager->GetStrategyResultSet();
		if (HostRs.size() > 0)
		{	
			for (pqxx::result::const_iterator row = HostRs.begin(); row != HostRs.end(); ++row)
			{
				string createtime_t = row[1].as<string>();
				if(BesEncryptD::GetDogIndex(createtime_t)==-1)
				{
					BesLog::DebugPrint("invalid strategy detected! will quit!", BesLog::LOG_MESSAGE_TYPE_ERROR);
					MyDbDataManager->DestroyResultSet(HostRs);
					return false;
				}
			}
		}
		
#elif (DB_NAME==2)

		//检查CREATETIME
		sql::ResultSet *HostRs = MyDbDataManager->GetStrategyResultSet();
	    if (HostRs != NULL && HostRs->rowsCount() > 0){
	        while (HostRs->next()){
				if(BesEncryptD::GetDogIndex(HostRs->getString("CREATETIME"))==-1){
					BesLog::DebugPrint("invalid strategy detected! will quit!", BesLog::LOG_MESSAGE_TYPE_ERROR);
					MyDbDataManager->DestroyResultSet(HostRs);
					return false;
				}
	        }
	    }
	    MyDbDataManager->DestroyResultSet(HostRs);
#endif
	}
	//文件方式
	else{
		StrategyLicense = BeGeneralLib::StringToInt(BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_HOST));
		if (TotalStrategy <= StrategyLicense && StrategyLicense > 0){
//			BesLog::DebugPrint("Strategy number:     [OK]");
	    }
	    else{
	        if (StrategyLicense == 0)
	            BesLog::DebugPrint("Strategy number:     [0]", BesLog::LOG_MESSAGE_TYPE_INFO);
	        else
	            BesLog::DebugPrint("Strategy number: 1    [FAILED]", BesLog::LOG_MESSAGE_TYPE_ERROR);
			return false;
	    }
	}

	// 检查源库数授权
//  BesLog::DebugPrint("Check source db number...");
    int TotalSourceDB = MyDbDataManager->GetSourceDBCount();
    int SourceDBLicense;
	if(LicenseType == "DOG"){
		SourceDBLicense = BeGeneralLib::StringToInt(BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_SNAPSHOT));
	}
	else{
		SourceDBLicense = BeGeneralLib::StringToInt(BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_SNAPSHOT));
	}
	
	cout << "TotalSourceDB="<<TotalSourceDB << endl;
    if (TotalSourceDB <= SourceDBLicense && SourceDBLicense > 0){
//      BesLog::DebugPrint("Source db number:     [OK]");
    }
    else{
        if (SourceDBLicense == 0)
            BesLog::DebugPrint("Source db number:     [0]", BesLog::LOG_MESSAGE_TYPE_INFO);
        else
            BesLog::DebugPrint("Source db number: 2    [FAILED]", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return false;
    }

    // 检查时间授权 start
	string ExpiredDate;
    string LicenseCreateTime;
	if(LicenseType == "DOG"){
		ExpiredDate = BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_EXPIRED_DATE);
	    LicenseCreateTime = BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_CREATE_TIME);
	}
	else{
		ExpiredDate = BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_EXPIRED_DATE);
	    LicenseCreateTime = BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_CREATE_TIME);
	}
    if (BeGeneralLib::IsStringEmptyOrInvalidValue(ExpiredDate) == true)
    {
        // 无效的授权到期日
		BesLog::DebugPrint("[License]Expired Date Invalid!", BesLog::LOG_MESSAGE_TYPE_ERROR);
        return false;
    }

    // 如果为临时时间授权
    if (ExpiredDate != "00000000")
    {
        // 检查当前系统时间
        time_t LicenseCreateDate = BeGeneralLib::ConvertStringToTime(BeGeneralLib::GetTimeString(BeGeneralLib::ConvertStringToTime(LicenseCreateTime, BeGeneralLib::TIME_FORMAT_FULL_DATETIME), BeGeneralLib::TIME_FORMAT_FULL_DATE), BeGeneralLib::TIME_FORMAT_FULL_DATE);
        if (BeGeneralLib::ConvertStringToTime(BeGeneralLib::GetTimeString(BeGeneralLib::TIME_FORMAT_FULL_DATE), BeGeneralLib::TIME_FORMAT_FULL_DATE) < LicenseCreateDate)
        {
            // 当前系统时间早于授权文件创建时间
			BesLog::DebugPrint("[License]System Time Earlier Than License Date!", BesLog::LOG_MESSAGE_TYPE_ERROR);
            return false;
        }

        // 检查时间戳文件
        string TimestampFile = BeGeneralLib::GetExecutePath() + DEFAULT_LOG_DIRECTORY + LINUX_PATH_SEPCHAR + string("zts");
        if (BeGeneralLib::IsFileExist(TimestampFile) == false)
        {
        	FILE *fp = fopen(TimestampFile.c_str(), "wb");
        	if (fp == NULL)
        	{
        		BesLog::DebugPrint("Handle timestamp error");
        		return false;
        	}
        	long ts = atol(LicenseCreateTime.c_str());
        	if (fwrite(&ts, sizeof(ts), 1, fp) != 1)
        	{
        		BesLog::DebugPrint("Handle timestamp error");
        		return false;
        	}
        	fclose(fp);
        }

        // 获取文件上次状态修改时间
        if (BeGeneralLib::GetFileStatusChangeTime(TimestampFile) > BeGeneralLib::GetSystemTime())
        {
            // 系统时间已修改
			BesLog::DebugPrint("[License]System Time Changed!", BesLog::LOG_MESSAGE_TYPE_ERROR);
            return false;
        }

        // 读取文件内容
        long ts = 0;
        FILE *fp = fopen(TimestampFile.c_str(), "rb");
        if (fp == NULL)
        {
        	BesLog::DebugPrint("Handle timestamp error");
        	return false;
        }
        if(fread(&ts, sizeof(ts), 1, fp)!=1)
        {
        	BesLog::DebugPrint("Handle timestamp error");
        	return false;
        }
        fclose(fp);
        if (BeGeneralLib::ConvertStringToTime(BeGeneralLib::Number2String(ts), BeGeneralLib::TIME_FORMAT_FULL_DATETIME) > BeGeneralLib::ConvertStringToTime(LicenseCreateTime, BeGeneralLib::TIME_FORMAT_FULL_DATETIME))
        {
            // 该授权文件早于上个授权文件的授权时间
			BesLog::DebugPrint("[License]Current Earlier Than Last License Date!", BesLog::LOG_MESSAGE_TYPE_ERROR);
            return false;
        }

        // 检查授权是否到期
        if (BeGeneralLib::ConvertStringToTime(BeGeneralLib::GetTimeString(BeGeneralLib::GetSystemTime(), BeGeneralLib::TIME_FORMAT_FULL_DATE), BeGeneralLib::TIME_FORMAT_FULL_DATE) > BeGeneralLib::ConvertStringToTime(ExpiredDate, BeGeneralLib::TIME_FORMAT_FULL_DATE))
        {
            // 授权到期
			BesLog::DebugPrint("[License]License Expired!", BesLog::LOG_MESSAGE_TYPE_ERROR);
            return false;
        }

        BeGeneralLib::SetFileLocked(TimestampFile, false);	// 解锁文件
        BeGeneralLib::ChangeFileTimestamp(TimestampFile);	// 修改文件时间戳
        BeGeneralLib::SetFileLocked(TimestampFile, true);	// 锁定文件
    }
	// 检查时间授权 end

    return true;
}

void * CleanThread(void *arg)
{
    ThreadState Thread = MyThreadStateCol.GetThreadState("CleanThread");

    while (Thread.Running())
    {
//		BesLog::DebugPrint("CleanThread is running...");
        time_t NowTime;
        time(&NowTime);

        if (MyNeedToExitFlag == true) break;

		// 删除最近一个月内的任务
		MyDbDataManager->DeleteExpiredTask();
#if (DB_NAME==1) //postgresql
		
		pqxx::result SourceDBResultSet = MyDbDataManager->GetSourceDBResultSet();
		if (SourceDBResultSet.size() > 0)
		{	
			for (pqxx::result::const_iterator row = SourceDBResultSet.begin(); row != SourceDBResultSet.end(); ++row)
			{
				string SourceID = row[0].as<string>();
				string TrunkTable = row[2].as<string>();
				if(BeGeneralLib::IsStringEmpty(TrunkTable) == false){
					MyDbDataManager->DeleteExpiredTrunkData(SourceID);
				}

			}
		}

#elif (DB_NAME==2)

		//清理TBL_TRUNKDATA_xx表
		sql::ResultSet * SourceDBResultSet = MyDbDataManager->GetSourceDBResultSet();
        if (SourceDBResultSet != NULL && SourceDBResultSet->rowsCount() > 0)
        {
			while (SourceDBResultSet->next())
			{
				string SourceID = SourceDBResultSet->getString("SID");
				string TrunkTable = SourceDBResultSet->getString("TRUNK_TABLE");
				if(BeGeneralLib::IsStringEmpty(TrunkTable) == false){
					MyDbDataManager->DeleteExpiredTrunkData(SourceID);
				}
			}
		}
		MyDbDataManager->DestroyResultSet(SourceDBResultSet);
#endif
		if (MyNeedToExitFlag == true)
			break;
		else
			sleep(30);
    }
    BesLog::DebugPrint("CleanThread exit...");
    pthread_exit(NULL);
}

//刷新任务线程
#if (DB_NAME==1) //postgresql
void * RefreshTaskThread(void *arg)
{

    ThreadState Thread = MyThreadStateCol.GetThreadState("RefreshTaskThread");

    // 将原有的准备执行、正在执行任务设置为完成，并置结果为失败
    MyDbDataManager->UpdateUncompletedTask();

    while (Thread.Running() == true)
    {
//		BesLog::DebugPrint("RefreshTaskThread is running...");
		if (MyNeedToExitFlag == true) break;

		// 获取空闲任务
		pqxx::result TaskResultSet = MyDbDataManager->GetTaskResultSet();

        //if (TaskResultSet == NULL) MyDbDataManager->Repair();

        if (TaskResultSet.size() > 0)
        {
			for (pqxx::result::const_iterator row = TaskResultSet.begin(); row != TaskResultSet.end(); ++row)
			{
				string TaskId = row[0].as<string>();
				string CommandCode = row[1].as<string>();
				string Parameter = row[2].as<string>();

				BesLog::DebugPrint("new task...");
				//string TaskId = TaskResultSet->getString("TASKID");
				//string CommandCode = TaskResultSet->getString("COMMANDCODE");
				//string Parameter = TaskResultSet->getString("PARAMETER");
				
//				int Status = TaskResultSet->getInt("STATUS");
//				int MaxCounter = TaskResultSet->getInt("MAXCOUNTER");
				vector<string> ParaArray = BeGeneralLib::StringSplit(Parameter, COMMAND_PARAMETER_SEPCHAR, BeGeneralLib::WITH_EMPTY_ITEM);

				if (ParaArray.size() != MyCommandManager->GetParameterCount(CommandCode)){
					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "命令参数个数不符");
					BesLog::DebugPrint("命令参数个数不符", BesLog::LOG_MESSAGE_TYPE_ERROR);
					continue;
				}
				BesLog::DebugPrint("------------------------------...");

				bool flag;
				string PID;

				//启动抓取
				if(CommandCode == DEFAULT_COMMAND_START_EXTRACTOR){
					//检查授权 start
					if (CheckLicense() == false){
//						MyNeedToExitFlag = true;
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "超出授权");
						BesLog::DebugPrint("超出授权", BesLog::LOG_MESSAGE_TYPE_ERROR);
						break;
					}
					//检查授权 end
					BesLog::DebugPrint("------------7------------------...");

					string SourceID = ParaArray[0];

					if(! MyDbDataManager->IsSourceDBExist(SourceID)){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "源库不存在");
						BesLog::DebugPrint("源库不存在", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					//创建TBL_TRUNKDATA_xx表
					flag = MyDbDataManager->CreateSourceTable(SourceID);
					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "创建相关表失败");
						BesLog::DebugPrint("创建相关表失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
//					BesLog::DebugPrint("table created");

					string ProcessName = string("cd /home/dss/capture/;source /home/dss/capture/env.sh;java cn.com.zhiwang.Capture ") + SourceID;

					string ProcessName1 = string("cn.com.zhiwang.Capture ") + SourceID;

					//如果进程已经启动则报错
					string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
//					BesLog::DebugPrint(ShellCommand);
					vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
					if(PidList.size() > 0){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "进程已启动");
						BesLog::DebugPrint("进程已启动", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					//启动抓取进程
					ShellCommand = ProcessName + string(" &");
					flag = BeGeneralLib::ExecuteSystem(ShellCommand, true, false);
					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "启动进程失败");
						MyDbDataManager->UpdateStrategyStatus(SourceID,"extractor");
						BesLog::DebugPrint("启动进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					BesLog::DebugPrint("process created");

					//获取进程号
					sleep(1);
					ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
					BesLog::DebugPrint(ShellCommand);
					PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
					if(PidList.size() != 1){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "获取进程号失败");
						BesLog::DebugPrint("获取进程号失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					PID = PidList[0];
					BesLog::DebugPrint(string("extractor pid : ") + PID);

					//保存进程号
					MyDbDataManager->UpdateExtractorProcess(SourceID, PID);

					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_SUCCESS, "");

					InitSourceDBMap();
					time(&SourceDBMap[SourceID]);
				}

				//停止抓取
				else if(CommandCode == DEFAULT_COMMAND_STOP_EXTRACTOR){
					string SourceID = ParaArray[0];
					BesLog::DebugPrint("------------6------------------...");

					//获取保存的进程号
					string PID = MyDbDataManager->GetExtractorProcess(SourceID);

					//获取实际的进程号
					string ProcessName1 = string("cn.com.zhiwang.Capture ") + SourceID;
					string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
					BesLog::DebugPrint(ShellCommand);
					vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
					if(PidList.size() == 0){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "进程已停止");
						BesLog::DebugPrint("进程已停止", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					//实际的和保存的不匹配
					if(PidList[0] != PID){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "需要人工停止进程");
						BesLog::DebugPrint("需要人工停止进程", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					//杀死进程
				    ShellCommand = string("kill -s KILL ") + PID;
					BesLog::DebugPrint(ShellCommand);
				    flag = BeGeneralLib::ExecuteSystem(ShellCommand, false, false);
					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "停止进程失败");
						BesLog::DebugPrint("停止进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					BesLog::DebugPrint("process stopped");
					MyDbDataManager->UpdateExtractorProcess(SourceID, "0");

					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_SUCCESS, "");

					SourceDBMap[SourceID] = -1;
				}

				//启动装载
				else if(CommandCode == DEFAULT_COMMAND_START_LOADER){
					//检查授权 start
					if (CheckLicense() == false){
//						MyNeedToExitFlag = true;
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "超出授权");
						BesLog::DebugPrint("超出授权", BesLog::LOG_MESSAGE_TYPE_ERROR);
						break;
					}
					//检查授权 end
					BesLog::DebugPrint("------------5------------------...");

					string StrategyID = ParaArray[0];

					if(! MyDbDataManager->IsStrategyExist(StrategyID)){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "策略不存在");
						BesLog::DebugPrint("策略不存在", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					//创建TBL_DONETRANS_xx表
					flag = MyDbDataManager->CreateStrategyTable(StrategyID);
					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "创建相关表失败");
						BesLog::DebugPrint("创建相关表失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
//					BesLog::DebugPrint("table created");

					string ProcessName = string("cd /home/dss/load;source /home/dss/load/env.sh;java cn.com.zhiwang.Load ") + StrategyID;

					string ProcessName1 = string("cn.com.zhiwang.Load ") + StrategyID;

					//如果进程已经启动则报错
					string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
					vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
					if(PidList.size() > 0){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "进程已启动");
						BesLog::DebugPrint("进程已启动", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					//启动装载进程
					ShellCommand = ProcessName + string(" &");
					flag = BeGeneralLib::ExecuteSystem(ShellCommand, false, false);
					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "启动进程失败");
						MyDbDataManager->UpdateStrategyStatus(StrategyID,"loader");

						BesLog::DebugPrint("启动进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					BesLog::DebugPrint("process created");

					//获取进程号
					sleep(1);
					ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
					PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
					if(PidList.size() != 1){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "获取进程号失败");
						BesLog::DebugPrint("获取进程号失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					PID = PidList[0];
					BesLog::DebugPrint(string("loader pid : ") + PID);

					//保存进程号
					MyDbDataManager->UpdateLoaderProcess(StrategyID, PID);

					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_SUCCESS, "");

					InitStrategyMap();
					time(&StrategyMap[StrategyID]);
				}

				//停止装载
				else if(CommandCode == DEFAULT_COMMAND_STOP_LOADER){
					string StrategyID = ParaArray[0];
					BesLog::DebugPrint("------------4-------StrategyID-----------..."+StrategyID);

					//获取保存的进程号
					string PID = MyDbDataManager->GetLoaderProcess(StrategyID);

					//获取实际的进程号
					string ProcessName1 = string("cn.com.zhiwang.Load ") + StrategyID;
					string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
					BesLog::DebugPrint(ShellCommand);
					vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
					if(PidList.size() == 0){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "进程已停止");
						BesLog::DebugPrint("进程已停止", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					BesLog::DebugPrint("------------4----4--------------...");

					//实际的和保存的不匹配
					if(PidList[0] != PID){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "需要人工停止进程");
						BesLog::DebugPrint("需要人工停止进程", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					//杀死进程
				    ShellCommand = string("kill -s KILL ") + PID;
					BesLog::DebugPrint(ShellCommand);
				    flag = BeGeneralLib::ExecuteSystem(ShellCommand, false, false);
					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "停止进程失败");
						BesLog::DebugPrint("停止进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					BesLog::DebugPrint("process stopped");
					MyDbDataManager->UpdateLoaderProcess(StrategyID, "0");

					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_SUCCESS, "");

					StrategyMap[StrategyID] = -1;
				}

				//删除策略
				else if(CommandCode == DEFAULT_COMMAND_DEL_STRATEGY){
					string StrategyID = ParaArray[0];
					BesLog::DebugPrint("------------3------------------...");

					if(! MyDbDataManager->IsStrategyExist(StrategyID)){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "策略不存在");
						BesLog::DebugPrint("策略不存在", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					flag = false;

					//加密狗授权
					if(LicenseType == "DOG"){
						string CreateTime = MyDbDataManager->GetStrategyCreateTime(StrategyID);
		                if(MyDbDataManager->DeleteStrategy(StrategyID)){
							if(BesEncryptD::DelDogTime(BesEncryptD::GetDogIndex(CreateTime))){
								BesEncryptD::WriteDogHostCount(BesEncryptD::ReadDogHostCount() - 1);
								flag = true;
							}
						}
					}
					//文件授权
					else{
						flag = MyDbDataManager->DeleteStrategy(StrategyID);
					}

					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "删除策略失败");
						BesLog::DebugPrint("删除策略失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					BesLog::DebugPrint("strategy deleted");
					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_SUCCESS, "");

					InitStrategyMap();
				}

				//删除源库
				else if(CommandCode == DEFAULT_COMMAND_DEL_SOURCEDB){
					string SourceID = ParaArray[0];

					if(! MyDbDataManager->IsSourceDBExist(SourceID)){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "源库不存在");
						BesLog::DebugPrint("源库不存在", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}

					flag = MyDbDataManager->DeleteSourceDB(SourceID);
					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "删除源库失败");
						BesLog::DebugPrint("删除源库失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					BesLog::DebugPrint("sourcedb deleted");
					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_SUCCESS, "");

					InitSourceDBMap();
				}

				//增加策略
				else if(CommandCode == DEFAULT_COMMAND_ADD_STRATEGY){
					BesLog::DebugPrint("------------2------------------...");

					//检查授权 start
					int TotalStrategy = MyDbDataManager->GetStrategyCount();
					//加密狗授权
					if(LicenseType == "DOG"){
						int StrategyLicense = BeGeneralLib::StringToInt(BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_HOST));
						if (TotalStrategy >= StrategyLicense || StrategyLicense <= 0 || BesEncryptD::ReadDogHostCount() >= StrategyLicense){
							MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "超出授权个数");
							BesLog::DebugPrint("超出授权个数", BesLog::LOG_MESSAGE_TYPE_ERROR);
							continue;
						}
					}
					//文件授权
					else{
						int StrategyLicense = BeGeneralLib::StringToInt(BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_HOST));
						if (TotalStrategy >= StrategyLicense || StrategyLicense <= 0){
				            MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "超出授权个数");
							BesLog::DebugPrint("超出授权个数", BesLog::LOG_MESSAGE_TYPE_ERROR);
							continue;
				        }
					}
					//检查授权 end

					string StrategyName = ParaArray[0];
					string SID = ParaArray[1];
					string DID = ParaArray[2];
					string SCN = ParaArray[3];
					string SyncUsers = ParaArray[4];
					string Interval = ParaArray[5];
					string ErrorSkip = ParaArray[6];
					string AutoStart = ParaArray[7];

					flag = false;

					//modify for s->d again
					if(MyDbDataManager->IsStrategyS2DExist(SID,DID)){
						BesLog::DebugPrint("增加策略失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					
					//加密狗授权
					if(LicenseType == "DOG"){
						if(MyDbDataManager->AddStrategy(StrategyName,SID,DID,SCN,SyncUsers,Interval,ErrorSkip,AutoStart)){
							if(BesEncryptD::AddDogTime(MyDbDataManager->GetStrategyCreateTime(""))){	//取最新的时间
								BesEncryptD::WriteDogHostCount(BesEncryptD::ReadDogHostCount() + 1);
								flag = true;
							}
						}
					}
					//文件授权
					else{
						flag = MyDbDataManager->AddStrategy(StrategyName,SID,DID,SCN,SyncUsers,Interval,ErrorSkip,AutoStart);
					}

					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "增加策略失败");
						BesLog::DebugPrint("增加策略失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					BesLog::DebugPrint("strategy added");
					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_SUCCESS, "");

					InitStrategyMap();
				}

				//增加源库
				else if(CommandCode == DEFAULT_COMMAND_ADD_SOURCEDB){
					//检查授权start
					int TotalSourceDB = MyDbDataManager->GetSourceDBCount();
					if(LicenseType == "DOG"){
						int SourceDBLicense = BeGeneralLib::StringToInt(BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_SNAPSHOT));
						if (TotalSourceDB >= SourceDBLicense || SourceDBLicense <= 0){
							MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "超出授权个数");
							BesLog::DebugPrint("超出授权个数", BesLog::LOG_MESSAGE_TYPE_ERROR);
							continue;
					    }
					}
					else{
					    int SourceDBLicense = BeGeneralLib::StringToInt(BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_SNAPSHOT));
					    if (TotalSourceDB >= SourceDBLicense || SourceDBLicense <= 0){
							MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "超出授权个数");
							BesLog::DebugPrint("超出授权个数", BesLog::LOG_MESSAGE_TYPE_ERROR);
							continue;
					    }
					}
					//检查授权end
#if 0
					string DbType = ParaArray[0];
					string SIP = ParaArray[1];
					string InstanceName = ParaArray[2];
					string Port = ParaArray[3];
					string Username = ParaArray[4];
					string Password = ParaArray[5];
					string TSName = ParaArray[6];
					string TrunkScn = ParaArray[7];
					string BranchScn = ParaArray[8];
					string Interval = ParaArray[9];
#endif
					flag = MyDbDataManager->AddSourceDB(ParaArray);
					if(flag == false){
						MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_FAILED, "增加源库失败");
						BesLog::DebugPrint("增加源库失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					BesLog::DebugPrint("sourcedb added");
					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager_PQ::TASK_STATE_FINISH, DbDataManager_PQ::TASK_RESULT_SUCCESS, "");

					InitSourceDBMap();
				}
            }
		}
		#if SCN_REFRESH
		
		BesLog::DebugPrint("------------1------------------...");
		pqxx::result SourceDBResultSet = MyDbDataManager->GetSourceDBForScnRefresh();

        if (SourceDBResultSet.size() > 0)
        {
			for (pqxx::result::const_iterator row = SourceDBResultSet.begin(); row != SourceDBResultSet.end(); ++row)
			{
				string SourceID = row[0].as<string>();
				string SourceIp = row[1].as<string>();
				string username = row[2].as<string>();
				string instanceN = row[3].as<string>();
				string userpasswd = row[4].as<string>();
				string srvName=SourceIp+":1521/"+instanceN;
				ScnRefresh_Func(username,userpasswd,srvName,SourceID);
			}
    	}
        
		#endif
		if (MyNeedToExitFlag == true) break;
		sleep(3);
    }
    BesLog::DebugPrint("RefreshTaskThread exit...");
    pthread_exit(NULL);
}


#elif (DB_NAME==2)//mysql

void TASK_START_EXTRACTOR(string TaskId,string SourceID)
{
	bool flag;

	//检查授权 start
	if (CheckLicense() == false){
//						MyNeedToExitFlag = true;
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "超出授权");
		BesLog::DebugPrint("超出授权", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	//检查授权 end

	//string SourceID = ParaArray[0];

	if(! MyDbDataManager->IsSourceDBExist(SourceID)){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "源库不存在");
		BesLog::DebugPrint("源库不存在", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	//创建TBL_TRUNKDATA_xx表
	flag = MyDbDataManager->CreateSourceTable(SourceID);
	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "创建相关表失败");
		BesLog::DebugPrint("创建相关表失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
//					BesLog::DebugPrint("table created");

	string ProcessName = string("cd /home/dss/capture/;source /home/dss/capture/env.sh;java cn.com.zhiwang.Capture ") + SourceID;

	string ProcessName1 = string("cn.com.zhiwang.Capture ") + SourceID;

	//如果进程已经启动则报错
	string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
//					BesLog::DebugPrint(ShellCommand);
	vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
	if(PidList.size() > 0){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "进程已启动");
		BesLog::DebugPrint("进程已启动", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	//启动抓取进程
	ShellCommand = ProcessName + string(" &");
	flag = BeGeneralLib::ExecuteSystem(ShellCommand, true, false);
	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "启动进程失败");
		//MyDbDataManager->UpdateStrategyStatus(SourceID,"extractor");
		BesLog::DebugPrint("启动进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	BesLog::DebugPrint("process created");

	//获取进程号
	sleep(1);
	ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
	BesLog::DebugPrint(ShellCommand);
	PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
	if(PidList.size() != 1){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "获取进程号失败");
		BesLog::DebugPrint("获取进程号失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	string PID = PidList[0];
	BesLog::DebugPrint(string("extractor pid : ") + PID);

	MyDbDataManager->UpdateExtractorStatus(SourceID, "1");

	//保存进程号
	MyDbDataManager->UpdateExtractorProcess(SourceID, PID);

	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	InitSourceDBMap();
	time(&SourceDBMap[SourceID]);
}

void TASK_STOP_EXTRACTOR(string TaskId,string SourceID)
{
	bool flag;

	//获取保存的进程号
	string PID = MyDbDataManager->GetExtractorProcess(SourceID);

	//获取实际的进程号
	string ProcessName1 = string("cn.com.zhiwang.Capture ") + SourceID;
	string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
	BesLog::DebugPrint(ShellCommand);
	vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
	if(PidList.size() == 0){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "进程已停止");
		BesLog::DebugPrint("进程已停止", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	#if 0
	//实际的和保存的不匹配
	if(PidList[0] != PID){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "需要人工停止进程");
		BesLog::DebugPrint("需要人工停止进程", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	//杀死进程
	
	ShellCommand = string("kill -s KILL ") + PID;
	BesLog::DebugPrint(ShellCommand);
	flag = BeGeneralLib::ExecuteSystem(ShellCommand, false, false);
	if(flag == false)
	#else
	if(SendMsgToCap_Tran_Load(TOCAPTURE,MSG_SEND_FOR_CLIENT_STATUS,COMMAND_QUIT,BeGeneralLib::StringToInt(SourceID))==-1)
	#endif
	{
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "停止进程失败");
		BesLog::DebugPrint("停止进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	
	BesLog::DebugPrint("process stopped");
	MyDbDataManager->UpdateExtractorProcess(SourceID, "0");
	MyDbDataManager->UpdateExtractorStatus(SourceID, "0");

	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	SourceDBMap[SourceID] = -1;
}

void TASK_START_LOADER(string TaskId,string StrategyID)
{
	bool flag;

	//检查授权 start
	if (CheckLicense() == false){
//						MyNeedToExitFlag = true;
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "超出授权");
		BesLog::DebugPrint("超出授权", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	//检查授权 end

	

	if(! MyDbDataManager->IsStrategyExist(StrategyID)){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "策略不存在");
		BesLog::DebugPrint("策略不存在", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	//创建TBL_DONETRANS_xx表
	flag = MyDbDataManager->CreateStrategyTable(StrategyID);
	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "创建相关表失败");
		BesLog::DebugPrint("创建相关表失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
//					BesLog::DebugPrint("table created");

	string ProcessName = string("cd /home/dss/load;source /home/dss/load/env.sh;java cn.com.zhiwang.Load ") + StrategyID;

	string ProcessName1 = string("cn.com.zhiwang.Load ") + StrategyID;

	//如果进程已经启动则报错
	string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
	vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
	if(PidList.size() > 0){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "进程已启动");
		BesLog::DebugPrint("进程已启动", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	//启动装载进程
	ShellCommand = ProcessName + string(" &");
	flag = BeGeneralLib::ExecuteSystem(ShellCommand, false, false);
	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "启动进程失败");
		BesLog::DebugPrint("启动进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	BesLog::DebugPrint("process created");
#if 1//cn.com.zhiwang.database.cassandra.Cassandra 
	string ProcessNameCassandra = string("cd /home/dss/load;source /home/dss/load/env.sh;java cn.com.zhiwang.database.cassandra.Cassandra  ") + StrategyID;

	string ProcessName1Cassandra = string("cn.com.zhiwang.database.cassandra.Cassandra  ") + StrategyID;

	//如果进程已经启动则报错
	string ShellCommandCassandra = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1Cassandra + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
	vector<string> PidListCassandra = BeGeneralLib::ReadShellReturnValueAll(ShellCommandCassandra);
	if(PidListCassandra.size() > 0){
		//MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "进程已启动");
		BesLog::DebugPrint("Cassandra进程已启动", BesLog::LOG_MESSAGE_TYPE_ERROR);
		//return;
	}

	//启动Cassandra进程
	ShellCommandCassandra = ProcessNameCassandra + string(" &");
	flag = BeGeneralLib::ExecuteSystem(ShellCommandCassandra, false, false);
	if(flag == false){
		//MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "启动进程失败");
		BesLog::DebugPrint("Cassandra启动进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		//return;
	}
	BesLog::DebugPrint("Cassandra process created");
#endif
	//获取进程号
	sleep(1);
	ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
	PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
	if(PidList.size() != 1){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "获取进程号失败");
		BesLog::DebugPrint("获取进程号失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	string PID = PidList[0];
	BesLog::DebugPrint(string("loader pid : ") + PID);
	
	MyDbDataManager->UpdateLoaderStatus(StrategyID, "1");

	//保存进程号
	MyDbDataManager->UpdateLoaderProcess(StrategyID, PID);

	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	InitStrategyMap();
	time(&StrategyMap[StrategyID]);
	
}

void TASK_STOP_LOADER(string TaskId,string StrategyID)
{
	bool flag;

	//获取保存的进程号
	string PID = MyDbDataManager->GetLoaderProcess(StrategyID);

	//获取实际的进程号
	string ProcessName1 = string("cn.com.zhiwang.Load ") + StrategyID;
	string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
	BesLog::DebugPrint(ShellCommand);
	vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
	if(PidList.size() == 0){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "进程已停止");
		BesLog::DebugPrint("进程已停止", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
#if 0
	//实际的和保存的不匹配
	if(PidList[0] != PID){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "需要人工停止进程");
		BesLog::DebugPrint("需要人工停止进程", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	//杀死进程

	ShellCommand = string("kill -s KILL ") + PID;
	BesLog::DebugPrint(ShellCommand);
	flag = BeGeneralLib::ExecuteSystem(ShellCommand, false, false);
	if(flag == false)
#else
	if(SendMsgToCap_Tran_Load(TOLOAD,MSG_SEND_FOR_CLIENT_STATUS,COMMAND_QUIT,BeGeneralLib::StringToInt(StrategyID))==-1)
#endif
	{
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "停止进程失败");
		BesLog::DebugPrint("停止进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	BesLog::DebugPrint("process stopped");
	MyDbDataManager->UpdateLoaderProcess(StrategyID, "0");
	MyDbDataManager->UpdateLoaderStatus(StrategyID, "0");

	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	StrategyMap[StrategyID] = -1;	
}

void TASK_DEL_STRATEGY(string TaskId,string StrategyID)
{
	bool flag;

	if(! MyDbDataManager->IsStrategyExist(StrategyID)){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "策略不存在");
		BesLog::DebugPrint("策略不存在", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	flag = false;

	//加密狗授权
	if(LicenseType == "DOG"){
		string CreateTime = MyDbDataManager->GetStrategyCreateTime(StrategyID);
		if(MyDbDataManager->DeleteStrategy(StrategyID)){
			if(BesEncryptD::DelDogTime(BesEncryptD::GetDogIndex(CreateTime))){
				BesEncryptD::WriteDogHostCount(BesEncryptD::ReadDogHostCount() - 1);
				flag = true;
			}
		}
	}
	//文件授权
	else{
		flag = MyDbDataManager->DeleteStrategy(StrategyID);
	}

	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "删除策略失败");
		BesLog::DebugPrint("删除策略失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	BesLog::DebugPrint("strategy deleted");
	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	InitStrategyMap();
	
}

void TASK_DEL_SOURCEDB(string TaskId,string SourceID)
{
	bool flag;

	if(! MyDbDataManager->IsSourceDBExist(SourceID)){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "源库不存在");
		BesLog::DebugPrint("源库不存在", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	flag = MyDbDataManager->DeleteSourceDB(SourceID);
	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "删除源库失败");
		BesLog::DebugPrint("删除源库失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	BesLog::DebugPrint("sourcedb deleted");
	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	InitSourceDBMap();
	
}

void TASK_ADD_STRATEGY(string TaskId,vector<string> ParaArray)
{
	bool flag;

	string StrategyName = ParaArray[0];
	string SID = ParaArray[1];
	string DID = ParaArray[2];
	string SCN = ParaArray[3];
	string SyncUsers = ParaArray[4];
	string Interval = ParaArray[5];
	string ErrorSkip = ParaArray[6];
	string AutoStart = ParaArray[7];
	//检查授权 start
	int TotalStrategy = MyDbDataManager->GetStrategyCount();
	//加密狗授权
	if(LicenseType == "DOG"){
		int StrategyLicense = BeGeneralLib::StringToInt(BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_HOST));
		if (TotalStrategy >= StrategyLicense || StrategyLicense <= 0 || BesEncryptD::ReadDogHostCount() >= StrategyLicense){
			MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "超出授权个数");
			BesLog::DebugPrint("超出授权个数", BesLog::LOG_MESSAGE_TYPE_ERROR);
			return;
		}
	}
	//文件授权
	else{
		int StrategyLicense = BeGeneralLib::StringToInt(BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_HOST));
		if (TotalStrategy >= StrategyLicense || StrategyLicense <= 0){
			MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "超出授权个数");
			BesLog::DebugPrint("超出授权个数", BesLog::LOG_MESSAGE_TYPE_ERROR);
			return;
		}
	}
	//检查授权 end



	flag = false;

	//modify for s->d again
	if(MyDbDataManager->IsStrategyS2DExist(SID,DID)){
		BesLog::DebugPrint("增加策略失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	
	//加密狗授权
	if(LicenseType == "DOG"){
		if(MyDbDataManager->AddStrategy(StrategyName,SID,DID,SCN,SyncUsers,Interval,ErrorSkip,AutoStart)){
			if(BesEncryptD::AddDogTime(MyDbDataManager->GetStrategyCreateTime(""))){	//取最新的时间
				BesEncryptD::WriteDogHostCount(BesEncryptD::ReadDogHostCount() + 1);
				flag = true;
			}
		}
	}
	//文件授权
	else{
		flag = MyDbDataManager->AddStrategy(StrategyName,SID,DID,SCN,SyncUsers,Interval,ErrorSkip,AutoStart);
	}

	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "增加策略失败");
		BesLog::DebugPrint("增加策略失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	BesLog::DebugPrint("strategy added");
	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	InitStrategyMap();
	
}

void TASK_ADD_SOURCEDB(string TaskId,vector<string> ParaArray)
{
	bool flag;

	//检查授权start
	int TotalSourceDB = MyDbDataManager->GetSourceDBCount();
	if(LicenseType == "DOG"){
		int SourceDBLicense = BeGeneralLib::StringToInt(BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_SNAPSHOT));
		if (TotalSourceDB >= SourceDBLicense || SourceDBLicense <= 0){
			MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "超出授权个数");
			BesLog::DebugPrint("超出授权个数", BesLog::LOG_MESSAGE_TYPE_ERROR);
			return;
		}
	}
	else{
		int SourceDBLicense = BeGeneralLib::StringToInt(BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_SNAPSHOT));
		if (TotalSourceDB >= SourceDBLicense || SourceDBLicense <= 0){
			MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "超出授权个数");
			BesLog::DebugPrint("超出授权个数", BesLog::LOG_MESSAGE_TYPE_ERROR);
			return;
		}
	}
	//检查授权end
	#if 0
//DBTYPE;SIP;INSTANCENAME;PORT;USERNAME;PASSWORD;TSNAME;time_interval;scn;per_time;start_time;sync_users;capture_mode;
	string DbType = ParaArray[0];
	string SIP = ParaArray[1];
	string InstanceName = ParaArray[2];
	string Port = ParaArray[3];
	string Username = ParaArray[4];
	string Password = ParaArray[5];
	string TSName = ParaArray[6];
	string Time_interval = ParaArray[7];
	string Scn = ParaArray[8];
	string Pre_time = ParaArray[9];
	string Start_time = ParaArray[10];
	string Sync_users = ParaArray[11];
	string Capture_mode = ParaArray[12];
	#endif
	flag = MyDbDataManager->AddSourceDB(ParaArray);
	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "增加源库失败");
		BesLog::DebugPrint("增加源库失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	BesLog::DebugPrint("sourcedb added");
	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	InitSourceDBMap();
	
}

void TASK_START_TRANSPORT(string TaskId,string SourceID)
{
	bool flag;

	//检查授权 start
	if (CheckLicense() == false){
//						MyNeedToExitFlag = true;
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "超出授权");
		BesLog::DebugPrint("超出授权", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	//检查授权 end

	//string SourceID = ParaArray[0];

	if(! MyDbDataManager->IsSourceDBExist(SourceID)){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "源库不存在");
		BesLog::DebugPrint("源库不存在", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	//创建TBL_TRUNKDATA_xx表
	flag = MyDbDataManager->CreateSourceTable(SourceID);
	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "创建相关表失败");
		BesLog::DebugPrint("创建相关表失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
//					BesLog::DebugPrint("table created");

	string ProcessName = string("cd /home/dss/transport/;python3.4 zhiwang_transport.py ") + SourceID;

	string ProcessName1 = string("zhiwang_transport.py ") + SourceID;

	//如果进程已经启动则报错
	string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
//					BesLog::DebugPrint(ShellCommand);
	vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
	if(PidList.size() > 0){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "进程已启动");
		BesLog::DebugPrint("进程已启动", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	//启动转换进程
	ShellCommand = ProcessName + string(" &");
	flag = BeGeneralLib::ExecuteSystem(ShellCommand, true, false);
	if(flag == false){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "启动进程失败");
		//MyDbDataManager->UpdateStrategyStatus(SourceID,"extractor");
		BesLog::DebugPrint("启动进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	BesLog::DebugPrint("process created");

	//获取进程号
	sleep(1);
	ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
	BesLog::DebugPrint(ShellCommand);
	PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
	if(PidList.size() != 1){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "获取进程号失败");
		BesLog::DebugPrint("获取进程号失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	string PID = PidList[0];
	BesLog::DebugPrint(string("transport pid : ") + PID);
	
	MyDbDataManager->UpdateTRansportStatus(SourceID, "1");

	//保存进程号
	MyDbDataManager->UpdateTRANSPORTProcess(SourceID, PID);

	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	InitSourceDBMap();
	time(&TranspDBMap[SourceID]);
}

void TASK_STOP_TRANSPORT(string TaskId,string SourceID)
{
	bool flag;

	//获取保存的进程号
	string PID = MyDbDataManager->GetTRANSPORTProcess(SourceID);

	//获取实际的进程号
	string ProcessName1 = string("zhiwang_transport.py ") + SourceID;
	string ShellCommand = string("ps -eo pid,cmd | gawk '{if($0~\"") + ProcessName1 + string("\") print $0}' | grep -v gawk | gawk '{print $1}'");
	BesLog::DebugPrint(ShellCommand);
	vector<string> PidList = BeGeneralLib::ReadShellReturnValueAll(ShellCommand);
	if(PidList.size() == 0){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "进程已停止");
		BesLog::DebugPrint("进程已停止", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
#if 0
	//实际的和保存的不匹配
	if(PidList[0] != PID){
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "需要人工停止进程");
		BesLog::DebugPrint("需要人工停止进程", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}

	//杀死进程

	ShellCommand = string("kill -s KILL ") + PID;
	BesLog::DebugPrint(ShellCommand);
	flag = BeGeneralLib::ExecuteSystem(ShellCommand, false, false);
	if(flag == false)
#else
	if(SendMsgToCap_Tran_Load(TOTRANSPORT,MSG_SEND_FOR_CLIENT_STATUS,COMMAND_QUIT,BeGeneralLib::StringToInt(SourceID))==-1)
#endif
	{
		MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "停止进程失败");
		BesLog::DebugPrint("停止进程失败", BesLog::LOG_MESSAGE_TYPE_ERROR);
		return;
	}
	BesLog::DebugPrint("process stopped");
	MyDbDataManager->UpdateTRANSPORTProcess(SourceID, "0");
	MyDbDataManager->UpdateTRansportStatus(SourceID, "0");

	MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_SUCCESS, "");

	TranspDBMap[SourceID] = -1;
}

void TASK_START_STRATEGY(string TaskId,string SourceID,string StrategyID)
{
	TASK_START_EXTRACTOR(TaskId,SourceID);
	
	TASK_START_TRANSPORT(TaskId,SourceID);

	TASK_START_LOADER(TaskId,StrategyID);
}

void TASK_STOP_STRATEGY(string TaskId,string SourceID,string StrategyID)
{
	TASK_STOP_EXTRACTOR(TaskId,SourceID);
	
	TASK_STOP_TRANSPORT(TaskId,SourceID);

	TASK_STOP_LOADER(TaskId,StrategyID);
}

void * RefreshTaskThread(void *arg)
{
    ThreadState Thread = MyThreadStateCol.GetThreadState("RefreshTaskThread");

    // 将原有的准备执行、正在执行任务设置为完成，并置结果为失败
    MyDbDataManager->UpdateUncompletedTask();

    while (Thread.Running() == true)
    {
//		BesLog::DebugPrint("RefreshTaskThread is running...");
		if (MyNeedToExitFlag == true) break;

		// 获取空闲任务
		sql::ResultSet * TaskResultSet = MyDbDataManager->GetTaskResultSet();

        if (TaskResultSet == NULL) MyDbDataManager->Repair();

        if (TaskResultSet != NULL && TaskResultSet->rowsCount() > 0)
        {
			while (TaskResultSet->next())
			{
				BesLog::DebugPrint("new task...");
				string TaskId = TaskResultSet->getString("TASKID");
//				string CommandHeader = TaskResultSet->getString("COMMANDHEADER");
				string CommandCode = TaskResultSet->getString("COMMANDCODE");
				string Parameter = TaskResultSet->getString("PARAMETER");
				
//				int Status = TaskResultSet->getInt("STATUS");
//				int MaxCounter = TaskResultSet->getInt("MAXCOUNTER");
				vector<string> ParaArray = BeGeneralLib::StringSplit(Parameter, COMMAND_PARAMETER_SEPCHAR, BeGeneralLib::WITH_EMPTY_ITEM);

				if (ParaArray.size() != MyCommandManager->GetParameterCount(CommandCode)){
					MyDbDataManager->UpdateTaskStatus(TaskId, DbDataManager::TASK_STATE_FINISH, DbDataManager::TASK_RESULT_FAILED, "命令参数个数不符");
					BesLog::DebugPrint("命令参数个数不符", BesLog::LOG_MESSAGE_TYPE_ERROR);
					printf("%s---%d\n",__FILE__, __LINE__);
					continue;
				}

				string PID;
				//启动策略 
				if(CommandCode == DEFAULT_COMMAND_START_STRATEGY){
					string SourceID = ParaArray[0];
					string StrategyID = ParaArray[1];
					TASK_START_STRATEGY(TaskId,SourceID,StrategyID);
				}

				//停止策略 
				else if(CommandCode == DEFAULT_COMMAND_STOP_STRATEGY){
					string SourceID = ParaArray[0];
					string StrategyID = ParaArray[1];
					TASK_STOP_STRATEGY(TaskId,SourceID,StrategyID);
				}
				
				//启动抓取
				if(CommandCode == DEFAULT_COMMAND_START_EXTRACTOR){
					string SourceID = ParaArray[0];
					TASK_START_EXTRACTOR(TaskId,SourceID);
				}

				//停止抓取
				else if(CommandCode == DEFAULT_COMMAND_STOP_EXTRACTOR){
					string SourceID = ParaArray[0];
					TASK_STOP_EXTRACTOR(TaskId,SourceID);
				}
				
				//启动转换
				if(CommandCode == DEFAULT_COMMAND_START_TRANSPORT){
					string SourceID = ParaArray[0];
					TASK_START_TRANSPORT(TaskId,SourceID);
				}
				
				//停止转换
				if(CommandCode == DEFAULT_COMMAND_STOP_TRANSPORT){
					string SourceID = ParaArray[0];
					TASK_STOP_TRANSPORT(TaskId,SourceID);
				}
				
				//启动装载
				else if(CommandCode == DEFAULT_COMMAND_START_LOADER){
					string StrategyID = ParaArray[0];
					TASK_START_LOADER(TaskId,StrategyID);
				}

				//停止装载
				else if(CommandCode == DEFAULT_COMMAND_STOP_LOADER){
					string StrategyID = ParaArray[0];
					TASK_STOP_LOADER(TaskId,StrategyID);
				}

				//删除策略
				else if(CommandCode == DEFAULT_COMMAND_DEL_STRATEGY){
					string StrategyID = ParaArray[0];
					TASK_DEL_STRATEGY(TaskId,StrategyID);
				}

				//删除源库
				else if(CommandCode == DEFAULT_COMMAND_DEL_SOURCEDB){
					string SourceID = ParaArray[0];
					TASK_DEL_SOURCEDB(TaskId,SourceID);
				}

				//增加策略
				else if(CommandCode == DEFAULT_COMMAND_ADD_STRATEGY){
					TASK_ADD_STRATEGY(TaskId,ParaArray);
				}

				//增加源库
				else if(CommandCode == DEFAULT_COMMAND_ADD_SOURCEDB){
					TASK_ADD_SOURCEDB(TaskId,ParaArray);
				}
            }
		}
		#if SCN_REFRESH
		sql::ResultSet * SourceDBResultSet = MyDbDataManager->GetSourceDBForScnRefresh();
        if (SourceDBResultSet != NULL && SourceDBResultSet->rowsCount() > 0)
        {
			while (SourceDBResultSet->next())
			{
				string SourceID = SourceDBResultSet->getString("SID");
				string SourceIp = SourceDBResultSet->getString("SIP");
				string username = SourceDBResultSet->getString("USERNAME");
				string userpasswd = SourceDBResultSet->getString("PASSWORD");
				string instanceN = SourceDBResultSet->getString("INSTANCENAME");
				string srvName=SourceIp+":1521/"+instanceN;
				ScnRefresh_Func(username,userpasswd,srvName,SourceID);
			}
		}
		#endif
        MyDbDataManager->DestroyResultSet(TaskResultSet);
		if (MyNeedToExitFlag == true) break;
		sleep(3);
    }
    BesLog::DebugPrint("RefreshTaskThread exit...");
    pthread_exit(NULL);
}
#endif
//侦听监控信息线程
void * ListenMonitorThread(void *arg)
{
    ThreadState Thread = MyThreadStateCol.GetThreadState("ListenMonitorThread");

    struct sockaddr_in ClientSock;													// 客户端套接字
    SocketEx * ServerSock = new SocketEx(SocketEx::SOCKETEX_TYPE_UDP_SERVER, 8686);	// 服务端套接字
    ServerSock->Open();

    while (Thread.Running())
    {
		if (MyNeedToExitFlag == true) break;
//		BesLog::DebugPrint("ListenMonitorThread is running...");
        bzero(&ClientSock, sizeof (ClientSock));
        string RecvString = ServerSock->ReceiveFrom(&ClientSock);
        string ClientIp = inet_ntoa(ClientSock.sin_addr);
        BesLog::DebugPrint(string("[") + ClientIp + string("]:MonitorMsg=") + RecvString, BesLog::LOG_MESSAGE_TYPE_ERROR, false);
		printf("%s---%d\n",__FILE__, __LINE__);

        if (HandleMonitorData(RecvString) == false) sleep(1);

        if (MyNeedToExitFlag == true) break;
    }
    ServerSock->Close();
    delete ServerSock;
    BesLog::DebugPrint("ListenMonitorThread exit...");
    pthread_exit(NULL);
}

void InitSourceDBMap()
{
    if (MyDbDataManager == NULL) return;
	
#if (DB_NAME==1) //postgresql

	pqxx::result SourceRs = MyDbDataManager->GetSourceDBResultSet();
	//int num_rows = DBResultSet.size();
	if (SourceRs.size() > 0)
	{	
		for (pqxx::result::const_iterator row = SourceRs.begin(); row != SourceRs.end(); ++row)
		{
			string SourceID = row[0].as<string>();
			if (SourceDBMap.find(SourceID) == SourceDBMap.end()){
				SourceDBMap.insert(pair<string,time_t> (SourceID, -1));
			}
			if (TranspDBMap.find(SourceID) == TranspDBMap.end()){
				TranspDBMap.insert(pair<string,time_t> (SourceID, -1));
			}
		}

	}

#elif (DB_NAME==2)

	sql::ResultSet *SourceRs = MyDbDataManager->GetSourceDBResultSet();
	if (SourceRs != NULL && SourceRs->rowsCount() > 0)
	{
		while (SourceRs->next())
		{
			string SourceID = SourceRs->getString("SID");
			if (SourceDBMap.find(SourceID) == SourceDBMap.end()){
				SourceDBMap.insert(pair<string,time_t> (SourceID, -1));
			}
			if (TranspDBMap.find(SourceID) == TranspDBMap.end()){
				TranspDBMap.insert(pair<string,time_t> (SourceID, -1));
			}
		}
	}
	MyDbDataManager->DestroyResultSet(SourceRs);

#endif
    // 检查是否有不存在的SourceDB
	map<string, time_t> ::iterator iter;
	for(iter = SourceDBMap.begin(); iter != SourceDBMap.end(); iter++){
		string TempID = iter -> first;
		if (MyDbDataManager->IsSourceDBExist(TempID) == false) SourceDBMap.erase(iter);
	}
}

void InitStrategyMap()
{
    if (MyDbDataManager == NULL) return;
	
#if (DB_NAME==1) //postgresql
	
		pqxx::result StrategyRs = MyDbDataManager->GetStrategyResultSet();
		//int num_rows = DBResultSet.size();
		if (StrategyRs.size() > 0)
		{	
			for (pqxx::result::const_iterator row = StrategyRs.begin(); row != StrategyRs.end(); ++row)
			{
				string StrategyID = row[0].as<string>();
				if (StrategyMap.find(StrategyID) == StrategyMap.end()){
					StrategyMap.insert(pair<string,time_t> (StrategyID, -1));
				}
			}
	
		}
	
#elif (DB_NAME==2)

    sql::ResultSet *StrategyRs = MyDbDataManager->GetStrategyResultSet();
    if (StrategyRs != NULL && StrategyRs->rowsCount() > 0)
    {
        while (StrategyRs->next()) {
            string StrategyID = StrategyRs->getString("STRATEGY_ID");
			if (StrategyMap.find(StrategyID) == StrategyMap.end()){
				StrategyMap.insert(pair<string,time_t> (StrategyID, -1));
			}
        }
    }
    MyDbDataManager->DestroyResultSet(StrategyRs);
	
#endif
    // 检查是否有不存在的Strategy
	map<string, time_t> ::iterator iter;
    for(iter = StrategyMap.begin(); iter != StrategyMap.end(); iter++){
		string TempID = iter -> first;
		if (MyDbDataManager->IsStrategyExist(TempID) == false) StrategyMap.erase(iter);
	}
}

bool CreateThreads()
{
    for (int i = 0; i < MyThreadStateCol.Size(); i++)
    {
        if (MyThreadStateCol[i].Create() == false)
        {
            return false;
        }
    }

    for (int i = 0; i < MyThreadStateCol.Size(); i++)
    {
        void * r;
        int res = pthread_join(MyThreadStateCol[i].ThreadId(), &r);
        if (res != 0)
        {
            cout << "Join Failed" << endl;
        }
    }
    return true;
}

void StopThreads()
{
    for (int i = 0; i < MyThreadStateCol.Size(); i++)
    {
        MyThreadStateCol[i].Stop();
    }
}

//获取中断信号
void CatchInterrupt(int sig)
{
    if (sig == SIGINT)
    {
        if (MyNeedToExitFlag == false)
        {
            MyNeedToExitFlag = true;
            StopThreads();
			
		#if SCN_REFRESH//modify for scn refresh
			Environment::terminateEnvironment(scn_env);
		#endif
		
			//freeReplyObject(redis_connect);
        }
    }
}

//自检
bool SelfCheck()
{
    BesLog::DebugPrint("[SyncTask Self Checking]...");

    // 检查程序是否已经运行
    BesLog::DebugPrint("Check instance ...");
    if (BeGeneralLib::IsInstanceExist()){
        BesLog::DebugPrint("Instance:   [already running]!");
        return false;
    }
    BesLog::DebugPrint("Instance:   [OK]");

    // 检测端口号是否在使用中
    BesLog::DebugPrint("Check port ...");
    BeGeneralLib::CheckPort(8585);

	//centos7
	BeGeneralLib::ExecuteSystem("systemctl disable udisks2");
	BeGeneralLib::ExecuteSystem("systemctl stop udisks2");
	BeGeneralLib::ExecuteSystem("systemctl disable NetworkManager");
	BeGeneralLib::ExecuteSystem("systemctl stop NetworkManager");

    return true;
}

void BaseInit()
{
    // 设置终端颜色
    BeGeneralLib::ResetConsoleColor();

    // 配置动态链接
    BeGeneralLib::ConfigureDynamicLinker();

    // 检查数据库是否OK
    string Host = DEFAULT_EMPTY_STRING;
    string UserName = DEFAULT_EMPTY_STRING;
    string Password = DEFAULT_EMPTY_STRING;
    string DbName = DEFAULT_EMPTY_STRING;
    string ConfigFileName = BeGeneralLib::GetExecutePath() + DEFAULT_CONFIG_FILE_NAME;
    if (BeGeneralLib::IsFileExist(ConfigFileName))
    {
        Host = BeGeneralLib::GetConfigItemValue(DEFAULT_CONFIG_ITEM_HOST);
        UserName = BeGeneralLib::GetConfigItemValue(DEFAULT_CONFIG_ITEM_USER);
        Password = BeGeneralLib::GetConfigItemValue(DEFAULT_CONFIG_ITEM_PASSWORD);
        DbName = BeGeneralLib::GetConfigItemValue(DEFAULT_CONFIG_ITEM_DBNAME);
		ExtractorAutostartInterval = BeGeneralLib::StringToInt(BeGeneralLib::GetConfigItemValue("EXTRACTORAUTOSTARINTERVAL"));
		LoaderAutostartInterval = BeGeneralLib::StringToInt(BeGeneralLib::GetConfigItemValue("LOADERAUTOSTARINTERVAL"));
		AutoClean = BeGeneralLib::StringToInt(BeGeneralLib::GetConfigItemValue("AUTOCLEAN"));

		LicenseType = BeGeneralLib::GetConfigItemValue("LICENSETYPE");
		if(LicenseType == "DOG") {
		}
		else{
			LicenseType = "FILE";
		}
		BesLog::DebugPrint(string("License Type : ") + LicenseType);
    }
    else
    {
        Host = BES_DATABASE_HOST;
        UserName = BES_USER_NAME;
        Password = BES_PASSWORD;
        DbName = DEFAULT_BES_DB_NAME;
    }
#if (DB_NAME==1) //postgresql
	MyDbDataManager = new DbDataManager_PQ(Host, UserName, Password, DbName);
#else//mysql
    MyDbDataManager = new DbDataManager(Host, UserName, Password, DbName);
#endif
    MyCommandManager = new BeCommand();

	ExecutePath = BeGeneralLib::AddPathSeparator(BeGeneralLib::GetExecutePath());	//    /home/dss/
	OracleConnect_Init();
	//if(redis_connect_init()) 
	//	BesLog::DebugPrint(string("redis_connect_init ok !"),BesLog::LOG_MESSAGE_TYPE_INFO);
	#if 0
	string source = string("121");
	char *Table_id ; 
	int len = source.length();
	Table_id=(char*)malloc((len+1)*sizeof(char));
	source.copy(Table_id,len,0);
	char Buf[1024]={0};
    bzero(Buf, sizeof (Buf));
    sprintf(Buf, "%s\0", source.c_str());
	//string TableName=string("TBL_TRUNKDATA");
	char  TableName[]= "TBL_TRUNKDATA";
	CreateTableByBaseTable(TableName,Buf);
	#endif
}

//显示授权信息
void ShowLicense()
{
	int StrategyLicense;
	int SourceDBLicense;
	string ExpiredDate;
	string LicenseCreateTime;

	if(LicenseType == "DOG"){
		StrategyLicense = BeGeneralLib::StringToInt(BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_HOST));
	    SourceDBLicense = BeGeneralLib::StringToInt(BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_SNAPSHOT));
	    ExpiredDate = BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_EXPIRED_DATE);
	    LicenseCreateTime = BesEncryptD::GetLicense(BesEncryptD::LICENSE_TYPE_CREATE_TIME);
	}
	else{
	    StrategyLicense = BeGeneralLib::StringToInt(BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_HOST));
	    SourceDBLicense = BeGeneralLib::StringToInt(BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_SNAPSHOT));
	    ExpiredDate = BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_EXPIRED_DATE);
	    LicenseCreateTime = BesEncryptF::GetLicense(BesEncryptF::LICENSE_TYPE_CREATE_TIME);
	}

    // 写入license信息文件
    string LicenseStr = "";
    string FileName = BeGeneralLib::GetExecutePath() + "dsslicinfo.txt";
    FILE *fp = fopen(FileName.c_str(), "w+");
    if (fp != NULL)
    {
		string DataStr;

        // 策略数
        DataStr = string("Strategy:\t") + BeGeneralLib::Number2String(StrategyLicense) + string("\n");
        BeGeneralLib::WriteDataToFile(fp, DataStr);

        // 源库数
        DataStr = string("SourceDB:\t") + BeGeneralLib::Number2String(SourceDBLicense) + string("\n");
        BeGeneralLib::WriteDataToFile(fp, DataStr);

        // 许可到期日
        if (BeGeneralLib::IsStringEmptyOrInvalidValue(ExpiredDate) == false)
        {
            if (ExpiredDate == "00000000")
                DataStr = string("Expired date:\t Never\n");
            else
                DataStr = string("Expired date:\t") + BeGeneralLib::GetTimeString(BeGeneralLib::ConvertStringToTime(ExpiredDate, BeGeneralLib::TIME_FORMAT_FULL_DATE), BeGeneralLib::TIME_FORMAT_DATE) + string("\n");

            BeGeneralLib::WriteDataToFile(fp, DataStr);
        }

        // 授权创建时间
        DataStr = string("Create time:\t") + BeGeneralLib::GetTimeString(BeGeneralLib::ConvertStringToTime(LicenseCreateTime, BeGeneralLib::TIME_FORMAT_FULL_DATETIME), BeGeneralLib::TIME_FORMAT_LONG) + string("\n");
        BeGeneralLib::WriteDataToFile(fp, DataStr);

        fclose(fp);
    }

    // 显示license信息
    string ShellCommand = string("cat ") + FileName;
    BeGeneralLib::ExecuteSystem(ShellCommand, false, false);
}

//处理监控数据
bool HandleMonitorData(string monitordata)
{
	vector<string> ParaArray1 = BeGeneralLib::StringSplit(monitordata, '#', BeGeneralLib::REMOVE_EMPTY_ITEM);
	if (ParaArray1.size() < 3){
        BesLog::DebugPrint("Monitor parameter is invalid!");
		BesLog::DebugPrint(string("size:")+BeGeneralLib::Number2String(ParaArray1.size()));
        return false;
    }
	BesLog::DebugPrint(string("time:") + ParaArray1[0] + string(" code:") + ParaArray1[1] + string(" status_str:") + ParaArray1[2],BesLog::LOG_MESSAGE_TYPE_NORMAL,false);

	string CmdCode = ParaArray1[1];
	string Parameter = ParaArray1[2];
//  Parameter = BeGeneralLib::StringRTrim(Parameter, COMMAND_PARAMETER_SEPCHAR); // 清除参数右端分割符

    vector<string> ParaArray2 = BeGeneralLib::StringSplit(Parameter, ';', BeGeneralLib::REMOVE_EMPTY_ITEM);
    if (ParaArray2.size() < 2){
        BesLog::DebugPrint("Monitor parameter is invalid!");
		BesLog::DebugPrint(string("size:")+BeGeneralLib::Number2String(ParaArray2.size()));
        return false;
    }

    string ID = ParaArray2[0];
	string status = ParaArray2[1];

	BesLog::DebugPrint(string("ID:") + ID + string(" status:") + status,BesLog::LOG_MESSAGE_TYPE_NORMAL,false);

	if(CmdCode == DEFAULT_COMMAND_QUERY_EXTRACTOR_STATE){
		if (SourceDBMap[ID] == 0){
			BesLog::DebugPrint(string("source db ") + ID +string(" not exist"));
			return false;
		}

		if(status == "0"){
			MyDbDataManager->UpdateExtractorStatus(ID, "0");
		}
		else if(status == "1"){
			MyDbDataManager->UpdateExtractorStatus(ID, "1");
			time(&SourceDBMap[ID]);
		}
	}
	else if(CmdCode == DEFAULT_COMMAND_QUERY_LOADER_STATE){
		if (StrategyMap[ID] == 0){
			BesLog::DebugPrint(string("strategy ") + ID +string(" not exist"));
			return false;
		}

		if(status == "0"){
			MyDbDataManager->UpdateLoaderStatus(ID, "0");
		}
		else if(status == "1"){
			MyDbDataManager->UpdateLoaderStatus(ID, "1");
			time(&StrategyMap[ID]);
		}
	}
	else if(CmdCode == DEFAULT_COMMAND_QUERY_TRANSPORT_STATE){
		if (TranspDBMap[ID] == 0){
			BesLog::DebugPrint(string("transp db  ") + ID +string(" not exist"));
			return false;
		}

		if(status == "0"){
			MyDbDataManager->UpdateTRansportStatus(ID, "0");
		}
		else if(status == "1"){
			MyDbDataManager->UpdateTRansportStatus(ID, "1");
			time(&TranspDBMap[ID]);
		}
	}
	return true;
}

//更新抓取进程、装载进程的状态
void * HBHandleThread(void *arg)
{
    ThreadState Thread = MyThreadStateCol.GetThreadState("HBHandleThread");
    while (Thread.Running()){
//    	BesLog::DebugPrint("HBHandleThread is Running...");
    	if (MyNeedToExitFlag == true) break;

		map<string, time_t> ::iterator iter;

		//更新抓取进程的状态
		for(iter = SourceDBMap.begin(); iter != SourceDBMap.end(); iter++){
			string TempID = iter -> first;
//			time_t TempTime = SourceDBMap[TempID];
			time_t NowTime; time(&NowTime);

			if (SourceDBMap[TempID] > 0){
				if(difftime(NowTime, SourceDBMap[TempID]) >= 30){
					MyDbDataManager->UpdateExtractorStatus(TempID, "0");
				}
				#if 0
				//自动重启抓取进程
				if (ExtractorAutostartInterval > 0 && difftime(NowTime, SourceDBMap[TempID]) >= ExtractorAutostartInterval){
					MyDbDataManager->AddTask(DEFAULT_COMMAND_START_EXTRACTOR, TempID+string(";"));
					BesLog::DebugPrint("capture failed!", BesLog::LOG_MESSAGE_TYPE_INFO);
					time(&SourceDBMap[TempID]);
				}
				#endif
			}
		}

		for(iter = TranspDBMap.begin(); iter != TranspDBMap.end(); iter++){
			string TempID = iter -> first;
//			time_t TempTime = SourceDBMap[TempID];
			time_t NowTime; time(&NowTime);

			if (TranspDBMap[TempID] > 0){
				if(difftime(NowTime, TranspDBMap[TempID]) >= 30){
					MyDbDataManager->UpdateTRansportStatus(TempID, "0");
				}
			}
		}


		//更新装载进程的状态
		for(iter = StrategyMap.begin(); iter != StrategyMap.end(); iter++){
			string TempID = iter -> first;
//			time_t TempTime = StrategyMap[TempID];
			time_t NowTime; time(&NowTime);

			if(StrategyMap[TempID] > 0){
				if (difftime(NowTime, StrategyMap[TempID]) >= 30){
					MyDbDataManager->UpdateLoaderStatus(TempID, "0");
				}
				
				#if 0
				//自动重启装载进程
				if (LoaderAutostartInterval >0 && difftime(NowTime, StrategyMap[TempID]) >= LoaderAutostartInterval){
					MyDbDataManager->AddTask(DEFAULT_COMMAND_START_LOADER, TempID+string(";"));
					BesLog::DebugPrint("loader failed, add autostart task", BesLog::LOG_MESSAGE_TYPE_INFO);
					time(&StrategyMap[TempID]);
				}
				#endif
			}
		}

		sleep(1);
    }
    BesLog::DebugPrint("HBHandleThread exit...");
    pthread_exit(NULL);
}
typedef enum XidType
{
	XID_TYPE_BEGIN=0,
	XID_TYPE_ROLLBACK_START = 1,
	XID_TYPE_COMMIT_START = 2,
	XID_TYPE_COMMIT = 3,
	XID_TYPE_WARN = 4,
	XID_TYPE_NULL = 5,
}check_xid_t;
typedef list<string> LISTCHAR;  
#if 0
void * DataCheckOutThread(void *arg)
{
	ThreadState Thread = MyThreadStateCol.GetThreadState("DataCheckOutThread");
  
  cout << "DataCheckOutThread=" << endl;
	while (Thread.Running())
	{
        if (MyNeedToExitFlag == true) break;

		redisReply * reply_begin;
		redisReply * reply;
		//reply = redisCommand(redis_connect,"");
		pqxx::result SourceDBResultSet = MyDbDataManager->GetSourceDB_SID();
		if (SourceDBResultSet.size() > 0)
		{	
			for (pqxx::result::const_iterator row = SourceDBResultSet.begin(); row != SourceDBResultSet.end(); ++row)
			{
				char hash_key1[64]={0};
				string begin_xid = "";
				check_xid_t checktype=XID_TYPE_NULL;

				list<string> list_capture;
				list<string>::iterator list_capturetiterator;
				
				
				string SourceId = row[0].as<string>();
				MyDbDataManager->CreateSourceTable(SourceId);
				reply_begin = (redisReply*)redisCommand(redis_connect,"select %s",SourceId.c_str());
				freeReplyObject(reply_begin);
				reply_begin = (redisReply*)redisCommand(redis_connect,"LRANGE capture_xid 0 -1");
				int i;
				if(reply_begin->elements==0)
				{
					freeReplyObject(reply_begin);
					continue;
				}
				int xid_num=reply_begin->elements ;//BeGeneralLib::StringToInt();
				freeReplyObject(reply_begin);
				cout << "===========1=========" << endl;

				for (i = 0; i <xid_num ; i++ )
				{
					reply_begin= (redisReply*)redisCommand(redis_connect,"EXISTS capture_xid");
					if(reply_begin->integer==0)
					{
						cout << "NO   capture_xid" << endl;
						freeReplyObject(reply_begin);
						continue;
					}
				
					freeReplyObject(reply_begin);
					
					reply_begin = (redisReply*)redisCommand(redis_connect,"rpop capture_xid");
					string xid_control=string(reply_begin->str);
					cout << "Come   xid_control="+xid_control << endl;
					freeReplyObject(reply_begin);
					reply_begin = (redisReply*)redisCommand(redis_connect,"LRANGE capture 0 -1");
					if(reply_begin->elements==0)
								continue;
					for (i = reply_begin->elements-1; i >=0 ; i-- )
					{
						list_capture.push_back(string(reply_begin->element[i]->str));   
					}
					
					cout << "===========2=========" << endl;
					freeReplyObject(reply_begin);
					bool commit_flag=false;
					int j;
					string allsql;
					string midallsql;
					string mid_key;
					for (list_capturetiterator = list_capture.begin(); list_capturetiterator != list_capture.end(); ++list_capturetiterator)	
					{
						mid_key=string(*list_capturetiterator);
						memset(hash_key1,0,sizeof(hash_key1));
						strcpy(hash_key1,mid_key.c_str());
						
						reply = (redisReply*)redisCommand(redis_connect,"EXISTS %s ",hash_key1);
						if(reply->integer==0)
						{
							cout << "NO   hash_keyall66="+string(hash_key1) << endl;
							freeReplyObject(reply);
							continue;
						}
						freeReplyObject(reply);
						//modify for capture same xid  into postgresql or delete
						{
						
							cout << "===========3=========" << endl;
							reply =(redisReply*) redisCommand(redis_connect,"hmget %s  operation_code xid ",hash_key1);
							string operation_code=string(reply->element[0]->str);
							string xid=string(reply->element[1]->str);
							freeReplyObject(reply);
							
							if(xid_control!=xid)
							{
								cout << "Not   samexid="+xid << endl;
								continue;
							}
							
							cout << "===========operation_code========="+operation_code << endl;
							if(operation_code=="6")
							{
								//delete
								reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key1);
								freeReplyObject(reply);
								reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key1);
								freeReplyObject(reply);
								continue;

							}
							else if(operation_code=="36")
							{
								//delete
								reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key1);
								freeReplyObject(reply);
								reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key1);
								freeReplyObject(reply);
								break;
							}
							else if(operation_code=="7")
							{
								commit_flag=true;
								//delete
								reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key1);
								freeReplyObject(reply);
								reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key1);
								freeReplyObject(reply);
								break;
							}
							else
							{
								string sql;
								string mid_str;
								reply =(redisReply*) redisCommand(redis_connect,"hmget %s scn TIMESTAMP operation_code seg_owner SSN CSF TABLE_SPACE TABLE_NAME sql_redo xid ",hash_key1);

								for(i=0;i<reply->elements;i++)
								{
									if(strlen(reply->element[i]->str)==0)
									{
										mid_str="null ";
									}
									else
									{
										mid_str=string(reply->element[i]->str);
							
									}
								
								//cout << "elements="<<reply->element[i]->str << endl;
								
								//cout << "mid_str="<<mid_str << endl;
									if(i==0)
									{
										sql=sql+"'"+mid_str+"'";//string(reply->element[i]->str);
									}
									else if(i==(reply->elements-2))
									{
										sql=sql+","+"'"+string(mid_str)+"'"+",'now()'";//string(reply->element[i]->str);
							
									}
									else
									{
										sql=sql+","+"'"+mid_str+"'";//string(reply->element[i]->str);
							
									}
									//cout << "elements="<<reply->element[i]->str << endl;
							
								}
								 
								//cout << "sql="+sql << endl;
								freeReplyObject(reply);
								//cout << "sql="+string(sql) << endl;
								#if 0
								if(MyDbDataManager->GETCOMMITINSERTINTOTRUNKTABLE(sql,SourceId)==false)
								{
									BesLog::DebugPrint("GETCOMMITINSERTINTOTRUNKTABLE failed!", BesLog::LOG_MESSAGE_TYPE_ERROR);
								}
								#else
								//SourceId="1";
								midallsql=MyDbDataManager->GETCOMMITINSERTINTOsql(sql,SourceId);

								#endif
								if(j==0)
									allsql=allsql+midallsql;

								else
									allsql=allsql+";"+midallsql;
								j++;
								cout << "allsql="<<allsql << endl;
								#if 1
								reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key1);
								freeReplyObject(reply);
								reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key1);
								freeReplyObject(reply);

								#endif
							}

						}
					}
					if(commit_flag)
					{
						allsql="begin;"+allsql+";end;";
						cout << "allsql="+allsql << endl;
						if(MyDbDataManager->GETCOMMITINSERTINTOTRUNKTABLE(allsql,SourceId)==false)
						{
							BesLog::DebugPrint("GETCOMMITINSERTINTOTRUNKTABLE failed!", BesLog::LOG_MESSAGE_TYPE_ERROR);
						}
						
						cout << "===========commit=========" << endl;
					}
					list_capture.clear();
				}
				cout << "===========5=========" << endl;

			}
		}			
        if (MyNeedToExitFlag == true)
        {
            break;
        }
		else{
	        sleep(10);
		}
    }
    BesLog::DebugPrint("DataCheckOutThread exit...");
    pthread_exit(NULL);
}

//#else
void * DataCheckOutThread(void *arg)
{
    ThreadState Thread = MyThreadStateCol.GetThreadState("DataCheckOutThread");
  
  cout << "DataCheckOutThread=" << endl;
    while (Thread.Running())
    {
        //time_t NowTime; time(&NowTime);

        if (MyNeedToExitFlag == true) break;

		#if 1
		
		redisReply * reply_begin;
		redisReply * reply;
		//reply = redisCommand(redis_connect,"");

		pqxx::result SourceDBResultSet = MyDbDataManager->GetSourceDB_SID();
		if (SourceDBResultSet.size() > 0)
		{	
			for (pqxx::result::const_iterator row = SourceDBResultSet.begin(); row != SourceDBResultSet.end(); ++row)
			{
				string SourceId = row[0].as<string>();
				//MyDbDataManager->CreateSourceTable(SourceId);
				reply_begin = (redisReply*)redisCommand(redis_connect,"select %s",SourceId.c_str());
				freeReplyObject(reply_begin);
				reply_begin = (redisReply*)redisCommand(redis_connect,"LRANGE capture 0 -1");
				#if 1
                int i;
				char hash_key1[64]={0};
				char hash_key2[64]={0};
				char hash_key3[64]={0};
				string begin_xid = "";
				check_xid_t checktype=XID_TYPE_NULL;
				//LISTCHAR list_capture;   
  				//List<string> list_capture=new ArrayList();
				list<string> list_capture;
				list<string>::iterator list_capturetiterator;
				
				//cout << "select="<<SourceId << endl;
				//cout << "capture_num="<<reply_begin->elements << endl;
				if(reply_begin->elements==0)
							continue;
                for (i = reply_begin->elements-1; i >=0 ; i-- )
                {
                	list_capture.push_back(string(reply_begin->element[i]->str));   
                }
				
				freeReplyObject(reply_begin);


				
				list<string>::iterator titerator_mid_start=list_capture.begin();
				list<string>::iterator titerator_mid_end;	
				list<string>::iterator titerator_mid_key;
				string xid_same;
				string mid_key;
				string operation_code_mid;
				for (list_capturetiterator = list_capture.begin(); list_capturetiterator != list_capture.end(); ++list_capturetiterator)	
				{
					string ddd=string(*list_capturetiterator);
                	redisReply * reply1;
                	memset(hash_key1,0,sizeof(hash_key1));
					strcpy( hash_key1,ddd.c_str());
					
					//cout << "hash_key1="+string(hash_key1) << endl;
					
					reply1 = (redisReply*)redisCommand(redis_connect,"EXISTS %s ",hash_key1);
					if(reply1->integer==0)
					{
						cout << "NO   hash_key1="+string(hash_key1) << endl;
						freeReplyObject(reply1);
						continue;
					}
					freeReplyObject(reply1);

					
					reply1 = (redisReply*)redisCommand(redis_connect,"hmget %s operation_code xid rollback",hash_key1);
					string operation_code1=string(reply1->element[0]->str);
					string xid1=string(reply1->element[1]->str);
					string rollback1=string(reply1->element[2]->str);
					
					//cout << "hash_key1="+string(hash_key1) << endl;
					//cout << "operation_code1="+string(operation_code1) << endl;
					//cout << "xid1="+string(xid1) << endl;
					freeReplyObject(reply1);
					//reply2 = redisCommand(redis_connect,"hmget %s operation_code xid rollback",hash_key2);
					//string operation_code2=string(reply2->element[0]->str);
					//string xid2=string(reply2->element[1]->str);
					//string rollback2=string(reply2->element[2]->str);
					if(operation_code1=="36")
					{
						checktype=XID_TYPE_ROLLBACK_START;
						begin_xid=string(xid1);// rollback start
						#if 1
						{
							reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key1);
							freeReplyObject(reply);
							reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key1);
							freeReplyObject(reply);
						}
						#endif
					#if 1
						reply1=(redisReply*)redisCommand(redis_connect,"LRANGE ALLROLLBACK1 0 -1");
						//LISTCHAR list_allcommit;	 
						list<string> list_rollback1;
						list<string>::iterator list_allrollbacktiterator1;
						//List<string> list_allcommit1=new ArrayList();
						//cout << "allcommit="<<reply1->elements << endl;
						if(reply1->elements>0)
						{
								//continue;
							for (i = 0; i <reply1->elements ; i++ )// the  last   operation_code is 7
							{
								list_rollback1.push_front(string(reply1->element[i]->str));	 
							}
							//string operation_code_key = string(reply1->element[reply1->elements-1]->str);// the  last   operation_code is 7
							
							freeReplyObject(reply1);

							
							for (list_allrollbacktiterator1 = list_rollback1.begin(); list_allrollbacktiterator1!= list_rollback1.end(); ++list_allrollbacktiterator1)	
							{
								string ddd1=string(*list_allrollbacktiterator1);
								memset(hash_key3,0,sizeof(hash_key3));
								//memset(hash_key2,0,sizeof(hash_key2));
								strcpy( hash_key3,ddd1.c_str());
								//cout << "===========11=========" << endl;
							
								//cout << "come  hash_key3="+string(hash_key3) << endl;
								reply1 = (redisReply*)redisCommand(redis_connect,"EXISTS %s ",hash_key3);
								if(reply1->integer==0)
								{
									cout << "NO   hash_keyall66="+string(hash_key3) << endl;
									freeReplyObject(reply1);
									continue;
								}
								freeReplyObject(reply1);


								reply =(redisReply*)redisCommand(redis_connect,"hmget %s xid",hash_key3);
								if(string(reply->element[0]->str)==begin_xid)
								{
									reply=(redisReply*)redisCommand(redis_connect,"LPUSH ALLROLLBACK %s",hash_key3);
									freeReplyObject(reply);

									reply=(redisReply*)redisCommand(redis_connect,"lrem ALLROLLBACK1 1 %s",hash_key3);
									freeReplyObject(reply);
								}
								//cout << "===========21=========" << endl;
								//cout << "operation_code_key="<< reply->element[0]->str<< endl;


							}
							//cout << "===========01=========" << endl;
							//for( i=0;i<list_allcommit.size();i++)

						}
						else
						{
							freeReplyObject(reply1);

						}

							
					#endif	
						
						titerator_mid_end=list_capturetiterator;
						for (titerator_mid_key = titerator_mid_start; titerator_mid_key != titerator_mid_end; ++titerator_mid_key)
						{
							mid_key=string(*titerator_mid_key);
		                	memset(hash_key2,0,sizeof(hash_key2));
							strcpy(hash_key2,mid_key.c_str());
							
							//cout << "hash_key2="+string(hash_key2) << endl;
							reply1 = (redisReply*)redisCommand(redis_connect,"EXISTS %s ",hash_key2);
							if(reply1->integer==0)
							{
								cout << "NO   hash_key2="+string(hash_key2) << endl;
								freeReplyObject(reply1);
								continue;
							}
							freeReplyObject(reply1);
							

							reply1 = (redisReply*)redisCommand(redis_connect,"hmget %s xid",hash_key2);

							xid_same=string(reply1->element[0]->str);
							
							if(xid_same==begin_xid)
							{
								reply=(redisReply*)redisCommand(redis_connect,"LPUSH ALLROLLBACK %s",hash_key2);
								freeReplyObject(reply);
								#if 0
								
								list<string>::iterator pos;        //用pos2来保存pos指针
								pos=titerator_mid_key;
								titerator_mid_key++;
								list_capture.erase(pos);
								#endif
							}
							else
							{
								reply=(redisReply*)redisCommand(redis_connect,"LPUSH ALLROLLBACK1 %s",hash_key2);
								freeReplyObject(reply);
							}

						}
						titerator_mid_start=titerator_mid_end;//list_capture.begin();


						//modify for  ALLROLLBACK
						{
							reply1=(redisReply*)redisCommand(redis_connect,"LRANGE ALLROLLBACK 0 -1");
						
							list<string> list_allrollback;
							list<string>::iterator list_allrollbacktiterator;
							//List<string> list_allcommit=new ArrayList();
							//cout << "ALLROLLBACK="<<reply1->elements << endl;
							if(reply1->elements==0)
								continue;
							for (i = 0; i <reply1->elements; i++ )// the  last	operation_code is 7
							{
								list_allrollback.push_back(string(reply1->element[i]->str));   
							}
							freeReplyObject(reply1);
							for (list_allrollbacktiterator = list_allrollback.begin(); list_allrollbacktiterator!= list_allrollback.end(); ++list_allrollbacktiterator) 
							{
							
								string ddd1=string(*list_allrollbacktiterator);
								reply =(redisReply*)redisCommand(redis_connect,"lrem ALLROLLBACK 1 %s",ddd1.c_str());
								freeReplyObject(reply);
								
								reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",ddd1.c_str());
								freeReplyObject(reply);
								
								reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",ddd1.c_str());
								freeReplyObject(reply);
							}
							list_allrollback.clear();

						}
					}
					else if (operation_code1=="7")
					{
						#define test_1 1
						checktype=XID_TYPE_COMMIT_START;
						begin_xid=string(xid1);// commit start	
						titerator_mid_end=list_capturetiterator;//for  for
						#if 1
						{
							//cout << "777or666	hash_keyall77="+string(hash_key1) << endl;									
							reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key1);
							freeReplyObject(reply);
							reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key1);
							freeReplyObject(reply);
						}
						#endif
						//from start to commit  
						#if 1
						reply1=(redisReply*)redisCommand(redis_connect,"LRANGE ALLCOMMIT1 0 -1");
						//LISTCHAR list_allcommit;	 
						list<string> list_allcommit1;
						list<string>::iterator list_allcommittiterator1;
						//List<string> list_allcommit1=new ArrayList();
						//cout << "allcommit="<<reply1->elements << endl;
						if(reply1->elements>0)
						{
								//continue;
							for (i = 0; i <reply1->elements ; i++ )// the  last   operation_code is 7
							{
								list_allcommit1.push_front(string(reply1->element[i]->str));	 
							}
							//string operation_code_key = string(reply1->element[reply1->elements-1]->str);// the  last   operation_code is 7
							
							freeReplyObject(reply1);

							
							for (list_allcommittiterator1 = list_allcommit1.begin(); list_allcommittiterator1!= list_allcommit1.end(); ++list_allcommittiterator1)	
							{
								string ddd1=string(*list_allcommittiterator1);
								memset(hash_key3,0,sizeof(hash_key3));
								//memset(hash_key2,0,sizeof(hash_key2));
								strcpy( hash_key3,ddd1.c_str());
								//cout << "===========11=========" << endl;
							
								//cout << "come  hash_key3="+string(hash_key3) << endl;
								reply1 = (redisReply*)redisCommand(redis_connect,"EXISTS %s ",hash_key3);
								if(reply1->integer==0)
								{
									cout << "NO   hash_keyall66="+string(hash_key3) << endl;
									freeReplyObject(reply1);
									continue;
								}
								freeReplyObject(reply1);


								reply =(redisReply*)redisCommand(redis_connect,"hmget %s xid",hash_key3);
								if(string(reply->element[0]->str)==begin_xid)
								{
									reply=(redisReply*)redisCommand(redis_connect,"LPUSH ALLCOMMIT %s",hash_key3);
									freeReplyObject(reply);

									reply=(redisReply*)redisCommand(redis_connect,"lrem ALLCOMMIT1 1 %s",hash_key3);
									freeReplyObject(reply);
								}
								//cout << "===========21=========" << endl;
								//cout << "operation_code_key="<< reply->element[0]->str<< endl;


							}
							//cout << "===========01=========" << endl;
							//for( i=0;i<list_allcommit.size();i++)

						}
						else
						{
							freeReplyObject(reply1);

						}

							
						#endif	
						
						for (titerator_mid_key = titerator_mid_start; titerator_mid_key != titerator_mid_end; ++titerator_mid_key)
						{
							mid_key=string(*titerator_mid_key);
		                	memset(hash_key2,0,sizeof(hash_key2));
							strcpy(hash_key2,mid_key.c_str());
							
							//cout << "hash_key2="+string(hash_key2) << endl;
							reply1 = (redisReply*)redisCommand(redis_connect,"EXISTS %s ",hash_key2);
							if(reply1->integer==0)
							{
								cout << "NO   hash_key23="+string(hash_key2) << endl;
								freeReplyObject(reply1);
								continue;
							}
							freeReplyObject(reply1);

							reply1 = (redisReply*)redisCommand(redis_connect,"hmget %s xid operation_code",hash_key2);
							xid_same=string(reply1->element[0]->str);
							operation_code_mid=string(reply1->element[1]->str);					
							
							if(xid_same==begin_xid)
							{

								reply=(redisReply*)redisCommand(redis_connect,"LPUSH ALLCOMMIT %s",hash_key2);
								freeReplyObject(reply);

#if 0
								list<string>::iterator pos;        //用pos2来保存pos指针
								pos=titerator_mid_key;
								titerator_mid_key++;
								list_capture.erase(pos);
#endif
								//list_capture.erase(titerator_mid_key++); 

							}
							else
							{
								reply=(redisReply*)redisCommand(redis_connect,"LPUSH ALLCOMMIT1 %s",hash_key2);
								freeReplyObject(reply);
							}
						}
						titerator_mid_start=titerator_mid_end;//list_capture.begin();//for  for
						//modify for all commit
						{
							reply1=(redisReply*)redisCommand(redis_connect,"LRANGE ALLCOMMIT 0 -1");
							//LISTCHAR list_allcommit;   
							list<string> list_allcommit;
							list<string>::iterator list_allcommittiterator;
							//List<string> list_allcommit1=new ArrayList();
							//cout << "allcommit="<<reply1->elements << endl;
							//if(reply1->elements<2)
								//continue;
							for (i = 0; i <reply1->elements ; i++ )// the  last   operation_code is 7
							{
								list_allcommit.push_back(string(reply1->element[i]->str));   
							}
							//string operation_code_key = string(reply1->element[reply1->elements-1]->str);// the  last   operation_code is 7
							
							freeReplyObject(reply1);
							
							//cout << "===========01=========" << endl;
							//for( i=0;i<list_allcommit.size();i++)
							string allsql;
							
							string midallsql;
							int j=0;
							for (list_allcommittiterator = list_allcommit.begin(); list_allcommittiterator!= list_allcommit.end(); ++list_allcommittiterator)	
							{
								string ddd1=string(*list_allcommittiterator);
								memset(hash_key3,0,sizeof(hash_key3));
								//memset(hash_key2,0,sizeof(hash_key2));
								strcpy( hash_key3,ddd1.c_str());
								//cout << "===========11=========" << endl;
							
								//cout << "come  hash_key3="+string(hash_key3) << endl;
								reply1 = (redisReply*)redisCommand(redis_connect,"EXISTS %s ",hash_key3);
								if(reply1->integer==0)
								{
									cout << "NO   hash_keyall66="+string(hash_key3) << endl;
									freeReplyObject(reply1);
									continue;
								}
								freeReplyObject(reply1);


								reply =(redisReply*)redisCommand(redis_connect,"hmget %s operation_code",hash_key3);
								
								//cout << "===========21=========" << endl;
								//cout << "operation_code_key="<< reply->element[0]->str<< endl;
								if (string(reply->element[0]->str)=="7"||string(reply->element[0]->str)=="6")
								{
								
									//cout << "777or666   hash_keyall77="+string(hash_key3) << endl;
									freeReplyObject(reply);
									#if test_1
									reply =(redisReply*)redisCommand(redis_connect,"lrem ALLCOMMIT 1 %s",hash_key3);
									freeReplyObject(reply);
									reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key3);
									freeReplyObject(reply);
									reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key3);
									#endif
									//cout << "========DEL===hash_keyall77=========" << endl;
									continue;
								}
								freeReplyObject(reply);
							
								//cout << "========hmget===21=========" << endl;

								reply1 = (redisReply*)redisCommand(redis_connect,"EXISTS %s ",hash_key3);
								if(reply1->integer==0)
								{
									cout << "NO   hash_keyall99="+string(hash_key3) << endl;
									freeReplyObject(reply1);
									continue;
								}
								freeReplyObject(reply1);
								
								reply =(redisReply*) redisCommand(redis_connect,"hmget %s scn TIMESTAMP SEQUENCE operation_code seg_owner username RS_ID SSN rollback CSF TABLE_SPACE TABLE_NAME sql_redo xid ",hash_key3);
								//string scn;	string TIMESTAMP;string SEQUENCE;string operation_code; string seg_owner;
								//string username;string RS_ID;string SSN;string rollback;string CSF;	string TABLE_SPACE;
								//string TABLE_NAME;string sql_redo;string xid;
								string sql;
								string mid_str;
								for(i=0;i<reply->elements;i++)
								{
									if(strlen(reply->element[i]->str)==0)
									{
										mid_str="null ";
									}
									else
									{
										mid_str=string(reply->element[i]->str);
							
									}
								
								//cout << "elements="<<reply->element[i]->str << endl;
								
								//cout << "mid_str="<<mid_str << endl;
									if(i==0)
									{
										sql=sql+"'"+mid_str+"'";//string(reply->element[i]->str);
									}
									else if(i==(reply->elements-2))
									{
										sql=sql+","+"'"+string(mid_str)+"'"+",'now()'";//string(reply->element[i]->str);
							
									}
									else
									{
										sql=sql+","+"'"+mid_str+"'";//string(reply->element[i]->str);
							
									}
									//cout << "elements="<<reply->element[i]->str << endl;
							
								}
								 
								//cout << "sql="+sql << endl;
								freeReplyObject(reply);
								//cout << "sql="+string(sql) << endl;
								#if 0
								if(MyDbDataManager->GETCOMMITINSERTINTOTRUNKTABLE(sql,SourceId)==false)
								{
									BesLog::DebugPrint("GETCOMMITINSERTINTOTRUNKTABLE failed!", BesLog::LOG_MESSAGE_TYPE_ERROR);
								}
								#else
								//SourceId="1";
								midallsql=MyDbDataManager->GETCOMMITINSERTINTOsql(sql,SourceId);

								#endif
								if(j==0)
									allsql=midallsql+allsql;

								else
									allsql=midallsql+";"+allsql;
								j++;
								#if test_1
								reply =(redisReply*)redisCommand(redis_connect,"lrem ALLCOMMIT 1 %s",hash_key3);
								freeReplyObject(reply);
								reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key3);
								freeReplyObject(reply);
								reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key3);
								#endif
							}
							allsql="begin;"+allsql+";end;";
							cout << "allsql="+allsql << endl;
							if(MyDbDataManager->GETCOMMITINSERTINTOTRUNKTABLE(allsql,SourceId)==false)
							{
								BesLog::DebugPrint("GETCOMMITINSERTINTOTRUNKTABLE failed!", BesLog::LOG_MESSAGE_TYPE_ERROR);
							}
							//MyDbDataManager->ExecuteCommit_For_Redis();
							
							#if test_1
							reply =(redisReply*)redisCommand(redis_connect,"DEL ALLCOMMIT");
							freeReplyObject(reply);
							#endif
							list_allcommit.clear();

						}
						//modify for all commit
					}

				}
				
				list_capture.clear();
				#else
                int i;
				char hash_key1[64]={0};
				char hash_key2[64]={0};
				string begin_xid = "";
				check_xid_t checktype=XID_TYPE_NULL;
				//LISTCHAR list_capture;   
  				//List<string> list_capture=new ArrayList();
				list<string> list_capture;
				list<string>::iterator list_capturetiterator;
				cout << "select="<<SourceId << endl;
				cout << "capture_num="<<reply_begin->elements << endl;
				if(reply_begin->elements==0)
							continue;
                for (i = 0; i <reply_begin->elements ; i++ )
                {
                	list_capture.push_back(string(reply_begin->element[i]->str));   
                }
				
				freeReplyObject(reply_begin);
				//for( i=0;i<list_capture.size();i++)
				for (list_capturetiterator = list_capture.begin(); list_capturetiterator != list_capture.end(); ++list_capturetiterator)	
				{
					string ddd=string(*list_capturetiterator);
                	redisReply * reply1;
					//redisReply * reply2;
                	memset(hash_key1,0,sizeof(hash_key1));
                	//memset(hash_key2,0,sizeof(hash_key1));
					strcpy( hash_key1,ddd.c_str());
					//strcpy( hash_key2,reply->element[i+1]->str);
					
					//cout << "i="<<i << endl;
					//cout << "reply_begin->element[i]->str="+string(reply_begin->element[i]->str) << endl;
					cout << "hash_key1="+string(hash_key1) << endl;
					//cout << "hash_key2="+string(hash_key2) << endl;
					


					reply1 = (redisReply*)redisCommand(redis_connect,"hmget %s operation_code xid rollback",hash_key1);
					if ( reply1->type == REDIS_REPLY_NIL)  
					{
						
						cout << "NO   hash_key="+string(hash_key1) << endl;
						continue;  
					}  
					string operation_code1=string(reply1->element[0]->str);
					string xid1=string(reply1->element[1]->str);
					string rollback1=string(reply1->element[2]->str);
					
					cout << "operation_code1="+string(operation_code1) << endl;
					cout << "xid1="+string(xid1) << endl;
					freeReplyObject(reply1);
					//reply2 = redisCommand(redis_connect,"hmget %s operation_code xid rollback",hash_key2);
					//string operation_code2=string(reply2->element[0]->str);
					//string xid2=string(reply2->element[1]->str);
					//string rollback2=string(reply2->element[2]->str);

					if(checktype==XID_TYPE_COMMIT_START&&(BeGeneralLib::StringToInt(begin_xid)!=atoi(xid1.c_str())))
					{
						reply1=(redisReply*)redisCommand(redis_connect,"LRANGE ALLCOMMIT 0 -1");
						//LISTCHAR list_allcommit;   
						list<string> list_allcommit1;
						list<string>::iterator list_allcommittiterator1;
						//List<string> list_allcommit1=new ArrayList();
						cout << "allcommit="<<reply1->elements << endl;
						if(reply1->elements<2)
							continue;
						for (i = 0; i <reply1->elements ; i++ )// the  last   operation_code is 7
						{
							list_allcommit1.push_back(string(reply1->element[i]->str));   
						}
						//string operation_code_key = string(reply1->element[reply1->elements-1]->str);// the  last   operation_code is 7
						
						freeReplyObject(reply1);
						
						cout << "===========01=========" << endl;
						//for( i=0;i<list_allcommit.size();i++)
						for (list_allcommittiterator1 = list_allcommit1.begin(); list_allcommittiterator1!= list_allcommit1.end(); ++list_allcommittiterator1)	
						{
							string ddd1=string(*list_allcommittiterator1);
							memset(hash_key2,0,sizeof(hash_key2));
							//memset(hash_key2,0,sizeof(hash_key2));
							strcpy( hash_key2,ddd1.c_str());
							cout << "===========11=========" << endl;

							reply =(redisReply*)redisCommand(redis_connect,"hmget %s operation_code",hash_key2);
							if ( reply->type == REDIS_REPLY_NIL)  
							{
								cout << "NO   hash_keyall="+string(hash_key2) << endl;
								
								freeReplyObject(reply);
								continue;  
							}  
							
							cout << "===========21=========" << endl;
							cout << "operation_code_key="<< reply->element[0]->str<< endl;
							if (string(reply->element[0]->str)=="7")
							{
							
								cout << "NO   hash_keyall="+string(hash_key2) << endl;									
								freeReplyObject(reply);
								reply =(redisReply*)redisCommand(redis_connect,"lrem ALLCOMMIT 1 %s",hash_key2);
								freeReplyObject(reply);
								continue;
							}
							freeReplyObject(reply);

							
							reply =(redisReply*) redisCommand(redis_connect,"hmget %s scn TIMESTAMP SEQUENCE operation_code seg_owner username RS_ID SSN rollback CSF TABLE_SPACE TABLE_NAME sql_redo xid ",hash_key2);
							//string scn;	string TIMESTAMP;string SEQUENCE;string operation_code;	string seg_owner;
							//string username;string RS_ID;string SSN;string rollback;string CSF;	string TABLE_SPACE;
							//string TABLE_NAME;string sql_redo;string xid;
							if ( reply1->type == REDIS_REPLY_NIL)  
							{
								cout << "NO   hash_keyall="+string(hash_key1) << endl;
								continue;  
							}  

							string sql;
							string mid_str;
							for(i=0;i<reply->elements;i++)
							{
								if(strlen(reply->element[i]->str)==0)
								{
									mid_str="null ";
								}
								else
								{
									mid_str=string(reply->element[i]->str);

								}
							
							//cout << "elements="<<reply->element[i]->str << endl;
							
							//cout << "mid_str="<<mid_str << endl;
								if(i==0)
								{
									sql=sql+"'"+mid_str+"'";//string(reply->element[i]->str);
								}
								else if(i==(reply->elements-2))
								{
									sql=sql+","+"'"+string(mid_str)+"'"+",'now()'";//string(reply->element[i]->str);

								}
								else
								{
									sql=sql+","+"'"+mid_str+"'";//string(reply->element[i]->str);

								}
								//cout << "elements="<<reply->element[i]->str << endl;

							}
							 
							cout << "sql="+sql << endl;
							freeReplyObject(reply);
							//cout << "sql="+string(sql) << endl;
							if(MyDbDataManager->GETCOMMITINSERTINTOTRUNKTABLE(sql,SourceId)==false)
							{
								BesLog::DebugPrint("GETCOMMITINSERTINTOTRUNKTABLE failed!", BesLog::LOG_MESSAGE_TYPE_ERROR);
							}
							reply =(redisReply*)redisCommand(redis_connect,"lrem ALLCOMMIT 1 %s",hash_key2);
							freeReplyObject(reply);
							reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key2);
							freeReplyObject(reply);
							reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key2);

						}
						reply =(redisReply*)redisCommand(redis_connect,"DEL ALLCOMMIT");
						freeReplyObject(reply);
						list_allcommit1.clear();
					}
					
					
					if(operation_code1=="36")
					{
						checktype=XID_TYPE_ROLLBACK_START;
						begin_xid=string(xid1);// rollback start
						
						reply =(redisReply*)redisCommand(redis_connect,"LPUSH ALLROLLBACK %s",hash_key1);
						freeReplyObject(reply);
					}
					else if (operation_code1=="7")
					{
						checktype=XID_TYPE_COMMIT_START;
						begin_xid=string(xid1);// commit start	
						
						reply=(redisReply*)redisCommand(redis_connect,"LPUSH ALLCOMMIT %s",hash_key1);
						freeReplyObject(reply);

					}
					else if(operation_code1!="36"&&operation_code1!="7"&&operation_code1!="6"&&
						(BeGeneralLib::StringToInt(begin_xid)==atoi(xid1.c_str())))
					{
						cout << "------------1-------------------------" << endl;
						if(checktype==XID_TYPE_COMMIT_START)
						{
							cout << "------------22-------------------------" << endl;
							reply=(redisReply*)redisCommand(redis_connect,"LPUSH ALLCOMMIT %s",hash_key1);
							freeReplyObject(reply);

						}
						else if(checktype==XID_TYPE_ROLLBACK_START)
						{
							reply =(redisReply*)redisCommand(redis_connect,"LPUSH ALLROLLBACK %s",hash_key1);
							freeReplyObject(reply);

						}
					}
					else if(operation_code1=="6"&&(BeGeneralLib::StringToInt(begin_xid)==atoi(xid1.c_str())))
					{
						if(checktype==XID_TYPE_COMMIT_START)
						{
							reply1=(redisReply*)redisCommand(redis_connect,"LRANGE ALLCOMMIT 0 -1");
							//LISTCHAR list_allcommit;   
							list<string> list_allcommit;
							list<string>::iterator list_allcommittiterator;
							//List<string> list_allcommit=new ArrayList();
							cout << "allcommit="<<reply1->elements << endl;
							if(reply1->elements<2)
								continue;
			                for (i = 0; i <reply1->elements ; i++ )// the  last   operation_code is 7
							{
			                	list_allcommit.push_back(string(reply1->element[i]->str));   
			                }
							//string operation_code_key = string(reply1->element[reply1->elements-1]->str);// the  last   operation_code is 7
							
							freeReplyObject(reply1);
							
							cout << "====================" << endl;
							//for( i=0;i<list_allcommit.size();i++)
							for (list_allcommittiterator = list_allcommit.begin(); list_allcommittiterator!= list_allcommit.end(); ++list_allcommittiterator)	
                			{
								string ddd1=string(*list_allcommittiterator);
								memset(hash_key2,0,sizeof(hash_key2));
								//memset(hash_key2,0,sizeof(hash_key2));
								strcpy( hash_key2,ddd1.c_str());
								cout << "===========1=========" << endl;

                				reply =(redisReply*)redisCommand(redis_connect,"hmget %s operation_code",hash_key2);
								if ( reply->type == REDIS_REPLY_NIL)  
								{
									cout << "NO   hash_keyall="+string(hash_key2) << endl;
									
									freeReplyObject(reply);
									continue;  
								}  
								
								cout << "===========2=========" << endl;
								cout << "operation_code_key="<< reply->element[0]->str<< endl;
								if (string(reply->element[0]->str)=="7")
								{
								
									cout << "NO   hash_keyall="+string(hash_key2) << endl;									
									freeReplyObject(reply);
									reply =(redisReply*)redisCommand(redis_connect,"lrem ALLCOMMIT 1 %s",hash_key2);
									freeReplyObject(reply);
									continue;
								}
								freeReplyObject(reply);

								
								reply =(redisReply*) redisCommand(redis_connect,"hmget %s scn TIMESTAMP SEQUENCE operation_code seg_owner username RS_ID SSN rollback CSF TABLE_SPACE TABLE_NAME sql_redo xid ",hash_key2);
								//string scn;	string TIMESTAMP;string SEQUENCE;string operation_code;	string seg_owner;
								//string username;string RS_ID;string SSN;string rollback;string CSF;	string TABLE_SPACE;
								//string TABLE_NAME;string sql_redo;string xid;
								if ( reply1->type == REDIS_REPLY_NIL)  
								{
									cout << "NO   hash_keyall="+string(hash_key1) << endl;
									continue;  
								}  

								string sql;
								string mid_str;
								for(i=0;i<reply->elements;i++)
								{
									if(strlen(reply->element[i]->str)==0)
									{
										mid_str="null ";
									}
									else
									{
										mid_str=string(reply->element[i]->str);

									}
								
								//cout << "elements="<<reply->element[i]->str << endl;
								
								//cout << "mid_str="<<mid_str << endl;
									if(i==0)
									{
										sql=sql+"'"+mid_str+"'";//string(reply->element[i]->str);
									}
									else if(i==(reply->elements-2))
									{
										sql=sql+","+"'"+string(mid_str)+"'"+",'now()'";//string(reply->element[i]->str);

									}
									else
									{
										sql=sql+","+"'"+mid_str+"'";//string(reply->element[i]->str);

									}
									//cout << "elements="<<reply->element[i]->str << endl;

								}
								 
								cout << "sql="+sql << endl;
								freeReplyObject(reply);
								//cout << "sql="+string(sql) << endl;
								if(MyDbDataManager->GETCOMMITINSERTINTOTRUNKTABLE(sql,SourceId)==false)
								{
									BesLog::DebugPrint("GETCOMMITINSERTINTOTRUNKTABLE failed!", BesLog::LOG_MESSAGE_TYPE_ERROR);
								}
								reply =(redisReply*)redisCommand(redis_connect,"lrem ALLCOMMIT 1 %s",hash_key2);
								freeReplyObject(reply);
								reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key2);
								freeReplyObject(reply);
								reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key2);
								freeReplyObject(reply);

							}
							
							list_allcommit.clear();
						}
						else if(checktype==XID_TYPE_ROLLBACK_START)
						{
							reply1=(redisReply*)redisCommand(redis_connect,"LRANGE ALLROLLBACK 0 -1");

							list<string> list_allrollback;
							list<string>::iterator list_allrollbacktiterator;
							//List<string> list_allcommit=new ArrayList();
							cout << "ALLROLLBACK="<<reply1->elements << endl;
							if(reply1->elements==0)
								continue;
			                for (i = 0; i <reply1->elements-1 ; i++ )// the  last   operation_code is 7
							{
			                	list_allrollback.push_back(string(reply1->element[i]->str));   
			                }
							freeReplyObject(reply1);
							for (list_allrollbacktiterator = list_allrollback.begin(); list_allrollbacktiterator!= list_allrollback.end(); ++list_allrollbacktiterator)	
                			{
                			
								string ddd1=string(*list_allrollbacktiterator);
								reply =(redisReply*)redisCommand(redis_connect,"lrem ALLROLLBACK 1 %s",ddd1.c_str());
								freeReplyObject(reply);
								
								reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",ddd1.c_str());
								freeReplyObject(reply);
								
								reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",ddd1.c_str());
								freeReplyObject(reply);
							}
							
							//freeReplyObject(reply1);
						}
						checktype=XID_TYPE_BEGIN;
						begin_xid=="";

						reply =(redisReply*)redisCommand(redis_connect,"DEL ALLCOMMIT");
						freeReplyObject(reply);

						reply =(redisReply*)redisCommand(redis_connect,"DEL ALLROLLBACK");
						freeReplyObject(reply);

						
						//reply =(redisReply*)redisCommand(redis_connect,"DEL %s ",hash_key1);
						//freeReplyObject(reply);
						
						//reply =(redisReply*)redisCommand(redis_connect,"lrem capture 1 %s",hash_key1);
						//freeReplyObject(reply);
					}

					if((checktype==XID_TYPE_COMMIT_START||checktype==XID_TYPE_ROLLBACK_START)
						&&(BeGeneralLib::StringToInt(begin_xid)!=atoi(xid1.c_str())))
					{
						checktype=XID_TYPE_WARN;
						//exit(1);
					}
					
					
				}
				list_capture.clear();
				#endif
			}
		
		}

		#else//  mysql -----oracle
		sql::ResultSet * SourceDBResultSet = MyDbDataManager->GetSourceDB_SID();       
		if (SourceDBResultSet != NULL && SourceDBResultSet->rowsCount() > 0)        
		{
			while (SourceDBResultSet->next())			
			{
				bool flag;
				string SourceId = SourceDBResultSet->getString("SID");
				int RECOUNT = MyDbDataManager->GetCountResultSetFromTEMP(SourceId);	 
				if (RECOUNT > 0)	  
				{
					
					flag=MyDbDataManager->CheckCommitAndInsertDelete(SourceId);
					if(flag)
					{
						BesLog::DebugPrint("Commit", BesLog::LOG_MESSAGE_TYPE_ERROR);
						continue;
					}
					sql::ResultSet * ResultSet = MyDbDataManager->GetXIDBYOPERATION_CODE(SourceId);       
					if (ResultSet != NULL && ResultSet->rowsCount() > 0)        
					{
						while (ResultSet->next())			
						{
							string XID = SourceDBResultSet->getString("XID");
							flag=MyDbDataManager->CheckByXID_InsertDelete(SourceId,XID);
							if(flag)
							{
								BesLog::DebugPrint("", BesLog::LOG_MESSAGE_TYPE_ERROR);
								continue;
							}
						}
					}
				}
				flag=MyDbDataManager->DeleteNotCommitDB(SourceId);
				if(flag)
				{
					BesLog::DebugPrint("delete", BesLog::LOG_MESSAGE_TYPE_ERROR);
					continue;
				}
			}

		}
		#endif
        if (MyNeedToExitFlag == true)
        {
            break;
        }
		else{
	        sleep(10);
		}
    }
    BesLog::DebugPrint("DataCheckOutThread exit...");
    pthread_exit(NULL);
}
#endif
void * SelfCheckThread(void *arg)
{
    ThreadState Thread = MyThreadStateCol.GetThreadState("SelfCheckThread");

    // 获取执行路径
    string ExecPath = BeGeneralLib::GetExecutePath();

    // 检查是否需要检查License
    time_t LastCheckTime; time(&LastCheckTime);
//  LastCheckTime -= 2* DEFAULT_CHECK_LICENSE_TIME;		//确保立即执行

    while (Thread.Running())
    {
//		BesLog::DebugPrint("SelfCheckThread is running...");
        // 检查License
        time_t NowTime; time(&NowTime);

        // 是否需要退出
        if (MyNeedToExitFlag == true) break;

        if (difftime(NowTime, LastCheckTime) > 300)
        {
            // 记录上次检查时间
            LastCheckTime = NowTime;
            // 检查授权
			if (CheckLicense() == false) MyNeedToExitFlag = true;
        }

        // 是否需要退出
        if (MyNeedToExitFlag == true)
        {
            break;
        }
		else{
	        sleep(30);
		}
    }
    BesLog::DebugPrint("SelfCheckThread exit...");
    pthread_exit(NULL);
}

void PrintVersion()
{
    cout << "\33[2J";
    cout << "\33[3;5H \33[36m" << endl;
    cout << "\33[4;5H************************************************************" << endl;
    cout << "\33[5;5H*                                                          *" << endl;
    cout << "\33[6;5H*                    APPNAME :  SyncTask                   *" << endl;
    cout << "\33[8;5H*                    VERSION :  " << APP_VERSION_INFORMATION << "                     *" << endl;
    cout << "\33[9;5H*                    RETIME  :  " << APP_RELEASE_TIME << "                 *" << endl;
    cout << "\33[10;5H*                    AGENT   :  " << APP_OS_SUPPORT << "   *" << endl;
    cout << "\33[11;5H*                                                          *" << endl;
    cout << "\33[12;5H************************************************************" << endl;
    cout << "\33[0m";
}

bool ParseOption(int argc, char** argv)
{
    bool HasOption = false;
    opterr = 0;
    int oc = -1;
    while ((oc = getopt(argc, argv, "bdefghiklprsvwx")) != -1)
    {
        HasOption = true;
        switch (oc)
        {
            case 0:
            {
                // 处理长选项
                break;
            }
            case 'b':
            {
                MyDbDataManager->BackupDatabase();
                break;
            }
            case 'k': // 杀死程序
            {
                for (int i = 0; i < 3; i++)
                {
                    string ShellCommand = string("killall -s 2 SyncTask");
                    BeGeneralLib::ExecuteSystem(ShellCommand, true, false);
                    sleep(1);
                }
                break;
            }
            case 'l':
            {
				ShowLicense();
                break;
            }
            case 'r':
            {
                MyDbDataManager->Repair();
                break;
            }
            case 'v':
            {
                PrintVersion();
                break;
            }
            default:
            {
                break;
            }
        }
    }
    return HasOption;
}

void ShowBasicInfo(){
    cout << "-------------------------INFORMATION-------------------------" << endl;
    cout << "|" << endl;
    cout << "|" << endl;
    cout << string("|\tHostName:\t") << BeGeneralLib::GetSystemHostName() << endl;
    cout << "|" << endl;
    cout << "|" << endl;
    cout << "-------------------------------------------------------------" << endl;
}
